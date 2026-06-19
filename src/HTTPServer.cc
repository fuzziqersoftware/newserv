#include "HTTPServer.hh"

#include <inttypes.h>
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <phosg/Network.hh>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Client.hh"
#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "Revision.hh"
#include "SaveFileFormats.hh"
#include "Server.hh"
#include "ShellCommands.hh"
#include "StaticGameData.hh"

// Walks system/players/ and invokes the visitor on each successfully-loaded
// character file. The visitor receives the parsed character, the account_id
// parsed from the filename, and the slot index. Files that don't parse are
// silently skipped — a corrupted save shouldn't kill the iteration. Only
// .psochar files are handled here; .pso3char (Ep3) files have a different
// on-disk layout and would need a separate walker.
//
// Two filename prefixes are accepted:
//   backup_player_{account_id}_{slot_index}.psochar  — operator $savechar
//   auto_player_{account_id}_{slot_index}.psochar    — automatic snapshot
//                                                      on disconnect
//
// When the same {account, slot} appears under both prefixes the auto
// snapshot wins (it's almost always fresher than the manual backup).
//
// .psochar embeds a PSOBBCharacterFile preceded by a small header — load
// via PSOCHARFile::load_shared which handles the prefix.
static void walk_backup_characters(
    const std::function<void(
        std::shared_ptr<PSOBBCharacterFile> ch,
        uint32_t account_id,
        size_t slot_index)>& visit) {
  static constexpr std::string_view kBackupPrefix = "backup_player_";
  static constexpr std::string_view kAutoPrefix = "auto_player_";
  static constexpr std::string_view kSuffix = ".psochar";
  const std::filesystem::path players_dir = "system/players";

  std::error_code ec;
  if (!std::filesystem::is_directory(players_dir, ec)) {
    return;
  }

  // First pass: collect candidate entries from both filename conventions,
  // keyed by (account_id, slot_index) so we can dedupe and let
  // auto_player_* win over backup_player_* when both exist.
  std::map<std::pair<uint32_t, size_t>, std::filesystem::path> picks;
  for (const auto& entry : std::filesystem::directory_iterator(players_dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.size() < kSuffix.size() ||
        filename.compare(filename.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) {
      continue;
    }
    std::string_view prefix;
    if (filename.size() >= kAutoPrefix.size() &&
        filename.compare(0, kAutoPrefix.size(), kAutoPrefix) == 0) {
      prefix = kAutoPrefix;
    } else if (filename.size() >= kBackupPrefix.size() &&
               filename.compare(0, kBackupPrefix.size(), kBackupPrefix) == 0) {
      prefix = kBackupPrefix;
    } else {
      continue;
    }

    // Parse "{account_id}_{slot_index}" from the middle of the filename.
    const std::string body = filename.substr(
        prefix.size(), filename.size() - prefix.size() - kSuffix.size());
    const auto sep = body.find('_');
    if (sep == std::string::npos) {
      continue;
    }
    uint32_t account_id = 0;
    size_t slot_index = 0;
    try {
      account_id = static_cast<uint32_t>(std::stoul(body.substr(0, sep)));
      slot_index = std::stoul(body.substr(sep + 1));
    } catch (const std::exception&) {
      continue;
    }

    auto key = std::make_pair(account_id, slot_index);
    auto it = picks.find(key);
    if (it == picks.end()) {
      picks.emplace(key, entry.path());
    } else if (prefix == kAutoPrefix) {
      // Auto-snapshots take precedence over manual $savechar backups
      // because they're populated on every disconnect — almost always
      // fresher than what the operator dumped at some point in the past.
      it->second = entry.path();
    }
  }

  // Second pass: load + emit each picked file.
  for (const auto& [key, path] : picks) {
    std::shared_ptr<PSOBBCharacterFile> ch;
    try {
      ch = PSOCHARFile::load_shared(path.string(), false).character_file;
    } catch (const std::exception&) {
      continue;
    }
    if (!ch) {
      continue;
    }
    visit(std::move(ch), key.first, key.second);
  }
}

// Reads the play-version sidecar written next to each auto snapshot
// (auto_player_{account_id}_{slot}.version, e.g. "BB_V4"). Returns "" when
// absent (older snapshots, or a manual $savechar backup with no sidecar). The
// dashboard uses this to decide whether a character's EXP / play-time is
// server-authoritative (BB) and therefore trustworthy to display.
static std::string read_snapshot_version(uint32_t account_id, size_t slot_index) {
  const std::filesystem::path path = std::filesystem::path("system/players") /
      ("auto_player_" + std::to_string(account_id) + "_" + std::to_string(slot_index) + ".version");
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return "";
  }
  std::string v;
  try {
    v = phosg::load_file(path.string());
  } catch (const std::exception&) {
    return "";
  }
  while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' ' || v.back() == '\t')) {
    v.pop_back();
  }
  return v;
}

// Streams JSON-Lines records (one JSON object per line) from a file under
// system/players/ — used for quest_plays.jsonl (log_quest_play, in
// ReceiveCommands.cc) and quest_completions.jsonl (log_quest_completion, in
// ReceiveSubcommands.cc). The file may not exist yet (nothing recorded since the
// feature shipped), which is not an error. Malformed lines are skipped.
static void walk_jsonl_records(
    const char* filename, const std::function<void(const phosg::JSON&)>& visit) {
  const std::filesystem::path path = std::filesystem::path("system/players") / filename;
  std::ifstream f(path);
  if (!f.is_open()) {
    return;
  }
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      visit(phosg::JSON::parse(line));
    } catch (const std::exception&) {
      // Skip malformed lines rather than failing the whole request.
    }
  }
}

HTTPServer::HTTPServer(std::shared_ptr<ServerState> state)
    : AsyncHTTPServer(state->io_context, "[HTTPServer] "), state(state) {
  using RouterRetT = std::variant<RawResponse, std::shared_ptr<const phosg::JSON>>;
  using RetT = asio::awaitable<RouterRetT>;
  using ArgsT = HTTPRouter<RouterRetT>::Args;

  auto generate_server_version_json = []() -> phosg::JSON {
    return phosg::JSON::dict({
        {"ServerType", "newserv"},
        {"BuildTime", BUILD_TIMESTAMP},
        {"BuildTimeStr", phosg::format_time(BUILD_TIMESTAMP)},
        {"Revision", GIT_REVISION_HASH},
    });
  };

  this->router.add(HTTPRequest::Method::GET, "/", [generate_server_version_json](ArgsT&&) -> RetT {
    co_return std::make_shared<phosg::JSON>(generate_server_version_json());
  });

  this->router.add(HTTPRequest::Method::POST, "/y/shell-exec", [this](ArgsT&& args) -> RetT {
    auto command = args.post_data.get_string("command");
    try {
      auto dispatch_res = co_await ShellCommand::dispatch_str(this->state, command);
      co_return std::make_shared<phosg::JSON>(phosg::JSON::dict({{"result", phosg::join(dispatch_res, "\n")}}));
    } catch (const std::exception& e) {
      throw HTTPError(400, e.what());
    }
  });

  this->router.add(HTTPRequest::Method::GET, "/y/rare-drops/stream", [this, generate_server_version_json](ArgsT&& args) -> RetT {
    if (!(co_await this->enable_websockets(args.client, args.req))) {
      throw HTTPError(400, "this path requires a websocket connection");
    }
    this->rare_drop_subscribers.emplace(args.client);
    co_await args.client->send_websocket_message(generate_server_version_json().serialize());
    co_return nullptr;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/clients", [this](ArgsT&&) -> RetT {
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& c : this->state->game_server->all_clients()) {
      auto item_name_index = this->state->data->item_name_index_opt(c->version());

      const char* drop_notifications_mode = "unknown";
      switch (c->get_drop_notification_mode()) {
        case Client::ItemDropNotificationMode::NOTHING:
          drop_notifications_mode = "off";
          break;
        case Client::ItemDropNotificationMode::RARES_ONLY:
          drop_notifications_mode = "rare";
          break;
        case Client::ItemDropNotificationMode::ALL_ITEMS:
          drop_notifications_mode = "on";
          break;
        case Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA:
          drop_notifications_mode = "every";
          break;
      }
      auto client_json = phosg::JSON::dict({
          {"ID", c->id},
          {"RemoteAddress", c->channel->default_name()},
          {"Version", phosg::name_for_enum(c->version())},
          {"SubVersion", c->sub_version},
          {"Language", name_for_language(c->language())},
          {"LocationX", c->pos.x.load()},
          {"LocationZ", c->pos.z.load()},
          {"LocationFloor", c->floor},
          {"CanChat", c->can_chat},
          {"SpecificVersion", c->specific_version},
          {"SwitchAssistEnabled", (c->check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? true : false)},
          {"InfiniteHPEnabled", (c->check_flag(Client::Flag::INFINITE_HP_ENABLED) ? true : false)},
          {"InfiniteTPEnabled", (c->check_flag(Client::Flag::INFINITE_TP_ENABLED) ? true : false)},
          {"FastKillsEnabled", (c->check_flag(Client::Flag::FAST_KILLS_ENABLED) ? true : false)},
          {"DropNotificationMode", drop_notifications_mode},
          {"DebugEnabled", (c->check_flag(Client::Flag::DEBUG_ENABLED) ? true : false)},
          {"ProxySaveFilesEnabled", (c->check_flag(Client::Flag::PROXY_SAVE_FILES) ? true : false)},
          {"ProxyChatCommandsEnabled", (c->check_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED) ? true : false)},
          {"ProxyPlayerNotificationsEnabled", (c->check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) ? true : false)},
          {"ProxyEp3InfiniteMesetaEnabled", (c->check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED) ? true : false)},
          {"ProxyEp3InfiniteTimeEnabled", (c->check_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED) ? true : false)},
          {"ProxyBlockFunctionCalls", (c->check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS) ? true : false)},
          {"ProxyEp3UnmaskWhispers", (c->check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) ? true : false)},
          {"OverrideRandomSeed", c->override_random_seed},
          {"OverrideSectionID", ((c->override_section_id != 0xFF) ? c->override_section_id : phosg::JSON(nullptr))},
          {"OverrideLobbyEvent", ((c->override_lobby_event != 0xFF) ? c->override_lobby_event : phosg::JSON(nullptr))},
          {"OverrideLobbyNumber", ((c->override_lobby_number != 0x80) ? c->override_lobby_number : phosg::JSON(nullptr))},
      });
      if (c->login) {
        client_json.emplace("Account", c->login->account->json());
      } else {
        client_json.emplace("Account", phosg::JSON());
      }
      auto l = c->lobby.lock();
      if (l) {
        client_json.emplace("LobbyID", l->lobby_id);
        client_json.emplace("LobbyClientID", c->lobby_client_id);
      }
      if (c->version() == Version::BB_V4) {
        client_json.emplace("BBCharacterIndex", c->bb_character_index);
      }
      auto p = c->character_file(false, false);
      if (p) {
        if (!is_ep3(c->version())) {
          if (c->version() != Version::DC_NTE) {
            client_json.emplace("InventoryLanguage", name_for_language(p->inventory.language));
            client_json.emplace("NumHPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::HP));
            client_json.emplace("NumTPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
            if (!is_v1_or_v2(c->version())) {
              client_json.emplace("NumPowerMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER));
              client_json.emplace("NumDefMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF));
              client_json.emplace("NumMindMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND));
              client_json.emplace("NumEvadeMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE));
              client_json.emplace("NumLuckMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
            }
          }
          phosg::JSON items_json = phosg::JSON::list();
          for (size_t z = 0; z < p->inventory.num_items; z++) {
            const auto& item = p->inventory.items[z];
            auto item_dict = phosg::JSON::dict({
                {"Flags", item.flags.load()},
                {"Data", item.data.hex()},
                {"ItemID", item.data.id.load()},
            });
            if (item_name_index) {
              item_dict.emplace("Description", item_name_index->describe_item(item.data));
            }
            items_json.emplace_back(std::move(item_dict));
          }
          client_json.emplace("InventoryItems", std::move(items_json));
          client_json.emplace("ATP", p->disp.stats.char_stats.atp.load());
          client_json.emplace("MST", p->disp.stats.char_stats.mst.load());
          client_json.emplace("EVP", p->disp.stats.char_stats.evp.load());
          client_json.emplace("HP", p->disp.stats.char_stats.hp.load());
          client_json.emplace("DFP", p->disp.stats.char_stats.dfp.load());
          client_json.emplace("ATA", p->disp.stats.char_stats.ata.load());
          client_json.emplace("LCK", p->disp.stats.char_stats.lck.load());
          client_json.emplace("EXP", p->disp.stats.exp.load());
          client_json.emplace("Meseta", p->disp.stats.meseta.load());
          auto tech_levels_json = phosg::JSON::dict();
          for (size_t z = 0; z < 0x13; z++) {
            auto level = p->get_technique_level(z);
            tech_levels_json.emplace(name_for_technique(z), (level != 0xFF) ? (level + 1) : phosg::JSON(nullptr));
          }
          client_json.emplace("TechniqueLevels", std::move(tech_levels_json));
        }
        client_json.emplace("Level", p->disp.stats.level.load() + 1);
        client_json.emplace("NameColor", p->disp.visual.sh.name_color.load());
        client_json.emplace("ExtraModel", (p->disp.visual.sh.validation_flags & 2) ? p->disp.visual.sh.extra_model : phosg::JSON(nullptr));
        client_json.emplace("SectionID", name_for_section_id(p->disp.visual.sh.section_id));
        client_json.emplace("CharClass", name_for_char_class(p->disp.visual.sh.char_class));
        client_json.emplace("Costume", p->disp.visual.sh.costume.load());
        client_json.emplace("Skin", p->disp.visual.sh.skin.load());
        client_json.emplace("Face", p->disp.visual.sh.face.load());
        client_json.emplace("Head", p->disp.visual.sh.head.load());
        client_json.emplace("Hair", p->disp.visual.sh.hair.load());
        client_json.emplace("HairR", p->disp.visual.sh.hair_r.load());
        client_json.emplace("HairG", p->disp.visual.sh.hair_g.load());
        client_json.emplace("HairB", p->disp.visual.sh.hair_b.load());
        client_json.emplace("ProportionX", p->disp.visual.sh.proportion_x.load());
        client_json.emplace("ProportionY", p->disp.visual.sh.proportion_y.load());

        client_json.emplace("Name", p->disp.visual.name.decode(c->language()));
        client_json.emplace("PlayTimeSeconds", p->play_time_seconds.load());

        client_json.emplace("AutoReply", p->auto_reply.decode(c->language()));
        client_json.emplace("InfoBoard", p->info_board.decode(c->language()));
        auto battle_place_counts = phosg::JSON::list({
            p->battle_records.place_counts[0].load(),
            p->battle_records.place_counts[1].load(),
            p->battle_records.place_counts[2].load(),
            p->battle_records.place_counts[3].load(),
        });
        client_json.emplace("BattlePlaceCounts", std::move(battle_place_counts));
        client_json.emplace("BattleDisconnectCount", p->battle_records.disconnect_count.load());

        if (!is_ep3(c->version())) {
          auto json_for_challenge_times = []<size_t Count>(const parray<ChallengeTime, Count>& times) -> phosg::JSON {
            auto times_json = phosg::JSON::list();
            for (size_t z = 0; z < times.size(); z++) {
              times_json.emplace_back(times[z].decode());
            }
            return times_json;
          };
          client_json.emplace("ChallengeTitleColorXRGB1555", p->challenge_records.title_color.load());
          client_json.emplace("ChallengeTimesEp1Online", json_for_challenge_times(p->challenge_records.times_ep1_online));
          client_json.emplace("ChallengeTimesEp2Online", json_for_challenge_times(p->challenge_records.times_ep2_online));
          client_json.emplace("ChallengeTimesEp1Offline", json_for_challenge_times(p->challenge_records.times_ep1_offline));
          client_json.emplace("ChallengeGraveIsEp2", p->challenge_records.grave_is_ep2 ? true : false);
          client_json.emplace("ChallengeGraveStageNum", p->challenge_records.grave_stage_num);
          client_json.emplace("ChallengeGraveFloor", p->challenge_records.grave_floor);
          client_json.emplace("ChallengeGraveDeaths", p->challenge_records.grave_deaths.load());
          {
            uint16_t year = 2000 + ((p->challenge_records.grave_time >> 28) & 0x0F);
            uint8_t month = (p->challenge_records.grave_time >> 24) & 0x0F;
            uint8_t day = (p->challenge_records.grave_time >> 16) & 0xFF;
            uint8_t hour = (p->challenge_records.grave_time >> 8) & 0xFF;
            uint8_t minute = p->challenge_records.grave_time & 0xFF;
            client_json.emplace("ChallengeGraveTime", std::format("{:04}-{:02}-{:02} {:02}:{:02}:00", year, month, day, hour, minute));
          }
          std::string grave_enemy_types;
          if (p->challenge_records.grave_defeated_by_enemy_rt_index) {
            for (EnemyType type : enemy_types_for_rare_table_index(
                     p->challenge_records.grave_is_ep2 ? Episode::EP2 : Episode::EP1,
                     p->challenge_records.grave_defeated_by_enemy_rt_index)) {
              if (!grave_enemy_types.empty()) {
                grave_enemy_types += "/";
              }
              grave_enemy_types += phosg::name_for_enum(type);
            }
          }
          client_json.emplace("ChallengeGraveDefeatedByEnemy", std::move(grave_enemy_types));
          client_json.emplace("ChallengeGraveX", p->challenge_records.grave_x.load());
          client_json.emplace("ChallengeGraveY", p->challenge_records.grave_y.load());
          client_json.emplace("ChallengeGraveZ", p->challenge_records.grave_z.load());
          client_json.emplace("ChallengeGraveTeam", p->challenge_records.grave_team.decode());
          client_json.emplace("ChallengeGraveMessage", p->challenge_records.grave_message.decode());
          client_json.emplace("ChallengeAwardStateEp1OnlineFlags", p->challenge_records.ep1_online_award_state.rank_award_flags.load());
          client_json.emplace("ChallengeAwardStateEp1OnlineMaxRank", p->challenge_records.ep1_online_award_state.maximum_rank.decode());
          client_json.emplace("ChallengeAwardStateEp2OnlineFlags", p->challenge_records.ep2_online_award_state.rank_award_flags.load());
          client_json.emplace("ChallengeAwardStateEp2OnlineMaxRank", p->challenge_records.ep2_online_award_state.maximum_rank.decode());
          client_json.emplace("ChallengeAwardStateEp1OfflineFlags", p->challenge_records.ep1_offline_award_state.rank_award_flags.load());
          client_json.emplace("ChallengeAwardStateEp1OfflineMaxRank", p->challenge_records.ep1_offline_award_state.maximum_rank.decode());
          client_json.emplace("ChallengeRankTitle", p->challenge_records.rank_title.decode());
        }
      }
      auto ses = c->proxy_session;
      if (ses) {
        auto lobby_players_json = phosg::JSON::list();
        for (size_t z = 0; z < ses->lobby_players.size(); z++) {
          const auto& p = ses->lobby_players[z];
          if (p.guild_card_number) {
            lobby_players_json.emplace_back(phosg::JSON::dict({
                {"GuildCardNumber", p.guild_card_number},
                {"Name", p.name},
                {"Language", name_for_language(p.language)},
                {"SectionID", name_for_section_id(p.section_id)},
                {"CharClass", name_for_char_class(p.char_class)},
            }));
            lobby_players_json.back().emplace("XBUserID", p.xb_user_id ? p.xb_user_id : phosg::JSON(nullptr));
          } else {
            lobby_players_json.emplace_back(nullptr);
          }
        }

        auto ses_json = phosg::JSON::dict({
            {"RemoteServerAddress", ses->server_channel->default_name()},
            {"RemoteGuildCardNumber", ses->remote_guild_card_number},
            {"RemoteClientConfigData", phosg::format_data_string(&ses->remote_client_config_data[0], ses->remote_client_config_data.size())},
            {"IsInGame", ses->is_in_game},
            {"IsInQuest", ses->is_in_quest},
            {"LobbyLeaderClientID", ses->leader_client_id},
            {"LobbyEvent", ses->lobby_event},
            {"LobbyDifficulty", name_for_difficulty(ses->lobby_difficulty)},
            {"LobbySectionID", name_for_section_id(ses->lobby_section_id)},
            {"LobbyMode", name_for_mode(ses->lobby_mode)},
            {"LobbyEpisode", name_for_episode(ses->lobby_episode)},
            {"LobbyRandomSeed", ses->lobby_random_seed},
            {"LobbyPlayers", std::move(lobby_players_json)},
        });
        switch (ses->drop_mode) {
          case ProxyDropMode::DISABLED:
            ses_json.emplace("DropMode", "none");
            break;
          case ProxyDropMode::PASSTHROUGH:
            ses_json.emplace("DropMode", "default");
            break;
          case ProxyDropMode::INTERCEPT:
            ses_json.emplace("DropMode", "proxy");
            break;
        }
        client_json.emplace("ProxySession", std::move(ses_json));
      } else {
        client_json.emplace("ProxySession", phosg::JSON());
      }

      res->emplace_back(std::move(client_json));
    }
    co_return res;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/lobbies", [this](ArgsT&&) -> RetT {
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& [_, l] : this->state->id_to_lobby) {
      auto leader = l->clients[l->leader_id];
      Version v = leader ? leader->version() : Version::BB_V4;
      auto item_name_index = this->state->data->item_name_index_opt(v);

      auto client_ids_json = phosg::JSON::list();
      for (size_t z = 0; z < l->max_clients; z++) {
        client_ids_json.emplace_back(l->clients[z] ? l->clients[z]->id : phosg::JSON(nullptr));
      }

      auto lobby_json = phosg::JSON::dict({
          {"ID", l->lobby_id},
          {"AllowedVersions", l->allowed_versions},
          {"Event", l->event},
          {"LeaderClientID", l->leader_id},
          {"MaxClients", l->max_clients},
          {"IdleTimeoutUsecs", l->idle_timeout_usecs},
          {"ClientIDs", std::move(client_ids_json)},
          {"IsGame", l->is_game()},
          {"IsPersistent", l->check_flag(Lobby::Flag::PERSISTENT)},
      });

      if (l->is_game()) {
        lobby_json.emplace("CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED));
        lobby_json.emplace("MinLevel", l->min_level + 1);
        lobby_json.emplace("MaxLevel", l->max_level + 1);
        lobby_json.emplace("Episode", name_for_episode(l->episode));
        lobby_json.emplace("HasPassword", !l->password.empty());
        lobby_json.emplace("Name", l->name);
        lobby_json.emplace("RandomSeed", l->random_seed);
        if (l->episode != Episode::EP3) {
          lobby_json.emplace("QuestSelectionInProgress", l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS));
          lobby_json.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
          lobby_json.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
          lobby_json.emplace("Variations", l->variations.json());
          uint8_t effective_section_id = l->effective_section_id();
          if (effective_section_id < 10) {
            lobby_json.emplace("SectionID", name_for_section_id(effective_section_id));
          } else {
            lobby_json.emplace("SectionID", nullptr);
          }
          lobby_json.emplace("Mode", name_for_mode(l->mode));
          lobby_json.emplace("Difficulty", name_for_difficulty(l->difficulty));
          lobby_json.emplace("BaseEXPMultiplier", l->base_exp_multiplier);
          lobby_json.emplace("EXPShareMultiplier", l->exp_share_multiplier);
          lobby_json.emplace("AllowedDropModes", l->allowed_drop_modes);
          switch (l->drop_mode) {
            case ServerDropMode::DISABLED:
              lobby_json.emplace("DropMode", "none");
              break;
            case ServerDropMode::CLIENT:
              lobby_json.emplace("DropMode", "client");
              break;
            case ServerDropMode::SERVER_SHARED:
              lobby_json.emplace("DropMode", "shared");
              break;
            case ServerDropMode::SERVER_PRIVATE:
              lobby_json.emplace("DropMode", "private");
              break;
            case ServerDropMode::SERVER_DUPLICATE:
              lobby_json.emplace("DropMode", "duplicate");
              break;
          }
          if (l->mode == GameMode::CHALLENGE) {
            lobby_json.emplace("ChallengeEXPMultiplier", l->challenge_exp_multiplier);
            if (l->challenge_params) {
              lobby_json.emplace("ChallengeStageNumber", l->challenge_params->stage_number);
              lobby_json.emplace("ChallengeRankColor", l->challenge_params->rank_color);
              lobby_json.emplace("ChallengeRankText", l->challenge_params->rank_text);
              lobby_json.emplace("ChallengeRank0ThresholdBitmask", l->challenge_params->rank_thresholds[0].bitmask);
              lobby_json.emplace("ChallengeRank0ThresholdSeconds", l->challenge_params->rank_thresholds[0].seconds);
              lobby_json.emplace("ChallengeRank1ThresholdBitmask", l->challenge_params->rank_thresholds[1].bitmask);
              lobby_json.emplace("ChallengeRank1ThresholdSeconds", l->challenge_params->rank_thresholds[1].seconds);
              lobby_json.emplace("ChallengeRank2ThresholdBitmask", l->challenge_params->rank_thresholds[2].bitmask);
              lobby_json.emplace("ChallengeRank2ThresholdSeconds", l->challenge_params->rank_thresholds[2].seconds);
            }
          }

          auto floor_items_json = phosg::JSON::list();
          for (size_t floor = 0; floor < l->floor_item_managers.size(); floor++) {
            for (const auto& it : l->floor_item_managers[floor].items) {
              const auto& item = it.second;
              auto item_dict = phosg::JSON::dict({
                  {"LocationFloor", floor},
                  {"LocationX", item->pos.x.load()},
                  {"LocationZ", item->pos.z.load()},
                  {"DropNumber", item->drop_number},
                  {"Flags", item->flags},
                  {"Data", item->data.hex()},
                  {"ItemID", item->data.id.load()},
              });
              if (item_name_index) {
                item_dict.emplace("Description", item_name_index->describe_item(item->data));
              }
              floor_items_json.emplace_back(std::move(item_dict));
            }
          }
          lobby_json.emplace("FloorItems", std::move(floor_items_json));
          lobby_json.emplace("Quest", l->quest ? l->quest->json() : phosg::JSON(nullptr));

        } else {
          lobby_json.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
          lobby_json.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
          lobby_json.emplace("SpectatorsForbidden", l->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN));

          auto ep3s = l->ep3_server;
          if (ep3s) {
            auto players_json = phosg::JSON::list();
            for (size_t z = 0; z < 4; z++) {
              if (!ep3s->name_entries[z].present) {
                players_json.emplace_back(nullptr);
              } else {
                auto lc = l->clients[z];

                auto deck_entry = ep3s->deck_entries[z];
                phosg::JSON deck_json = nullptr;
                if (deck_entry) {
                  auto cards_json = phosg::JSON::list();
                  for (size_t w = 0; w < deck_entry->card_ids.size(); w++) {
                    try {
                      const auto& ce = ep3s->options.card_index->definition_for_id(deck_entry->card_ids[w]);
                      auto name = ce->def.en_name.decode();
                      if (name.empty()) {
                        name = ce->def.en_short_name.decode();
                      }
                      if (name.empty()) {
                        name = ce->def.jp_name.decode();
                      }
                      if (name.empty()) {
                        name = ce->def.jp_short_name.decode();
                      }
                      cards_json.emplace_back(name);
                    } catch (const std::out_of_range&) {
                      cards_json.emplace_back(deck_entry->card_ids[w].load());
                    }
                  }
                  deck_json = phosg::JSON::dict({
                      {"Name", deck_entry->name.decode(lc ? lc->language() : Language::ENGLISH)},
                      {"TeamID", deck_entry->team_id.load()},
                      {"Cards", std::move(cards_json)},
                      {"GodWhimFlag", deck_entry->god_whim_flag},
                      {"PlayerLevel", deck_entry->player_level.load() + 1},
                  });
                }

                auto player_json = phosg::JSON::dict({
                    {"PlayerName", ep3s->name_entries[z].name.decode(lc ? lc->language() : Language::ENGLISH)},
                    {"ClientID", ep3s->name_entries[z].client_id},
                    {"IsCOM", !!ep3s->name_entries[z].is_cpu_player},
                    {"Deck", std::move(deck_json)},
                });
                players_json.emplace_back(std::move(player_json));
              }
            }
            auto battle_state_json = phosg::JSON::dict({
                {"BehaviorFlags", ep3s->options.behavior_flags},
                {"RandomSeed", ep3s->options.rand_crypt->seed()},
                {"Tournament", ep3s->options.tournament ? ep3s->options.tournament->json() : nullptr},
                {"MapNumber", ep3s->last_chosen_map ? ep3s->last_chosen_map->map_number : phosg::JSON(nullptr)},
                {"EnvironmentNumber", ep3s->map_and_rules ? ep3s->map_and_rules->environment_number : phosg::JSON(nullptr)},
                {"Rules", ep3s->map_and_rules ? ep3s->map_and_rules->rules.json() : nullptr},
                {"Players", std::move(players_json)},
                {"IsBattleFinished", ep3s->battle_finished},
                {"IsBattleInprogress", ep3s->battle_in_progress},
                {"RoundNumber", ep3s->round_num},
                {"FirstTeamTurn", ep3s->first_team_turn},
                {"CurrentTeamTurn", ep3s->current_team_turn1},
                {"BattlePhase", phosg::name_for_enum(ep3s->battle_phase)},
                {"SetupPhase", ep3s->setup_phase},
                {"RegistrationPhase", ep3s->registration_phase},
                {"ActionSubphase", ep3s->action_subphase},
                {"BattleStartTimeUsecs", ep3s->battle_start_usecs},
                {"TeamEXP", phosg::JSON::list({ep3s->team_exp[0], ep3s->team_exp[1]})},
                {"TeamDiceBonus", phosg::JSON::list({ep3s->team_dice_bonus[0], ep3s->team_dice_bonus[1]})},
                // TODO: Include information from these too?
                // std::shared_ptr<StateFlags> state_flags;
                // std::array<std::shared_ptr<PlayerState>, 4> player_states;
            });
            lobby_json.emplace("Episode3BattleState", std::move(battle_state_json));
          } else {
            lobby_json.emplace("Episode3BattleState", nullptr);
          }
          auto watched_lobby = l->watched_lobby.lock();
          if (watched_lobby) {
            lobby_json.emplace("WatchedLobbyID", watched_lobby->lobby_id);
          }
          auto watcher_lobby_ids_json = phosg::JSON::list();
          for (const auto& watcher_lobby : l->watcher_lobbies) {
            watcher_lobby_ids_json.emplace_back(watcher_lobby->lobby_id);
          }
          lobby_json.emplace("WatcherLobbyIDs", std::move(watcher_lobby_ids_json));
          lobby_json.emplace("IsReplayLobby", !!l->battle_player);
        }

      } else { // Not game
        lobby_json.emplace("IsPublic", l->check_flag(Lobby::Flag::PUBLIC));
        lobby_json.emplace("IsDefault", l->check_flag(Lobby::Flag::DEFAULT));
        lobby_json.emplace("IsOverflow", l->check_flag(Lobby::Flag::IS_OVERFLOW));
        lobby_json.emplace("Block", l->block);
      }

      res->emplace_back(std::move(lobby_json));
    }
    co_return res;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/accounts", [this](ArgsT&&) -> RetT {
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& it : this->state->account_index->all()) {
      res->emplace_back(it->json());
    }
    co_return res;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/account/:account_id", [this](ArgsT&& args) -> RetT {
    uint32_t account_id = args.get_param<uint32_t>("account_id");
    try {
      co_return std::make_shared<phosg::JSON>(this->state->account_index->from_account_id(account_id)->json());
    } catch (const AccountIndex::missing_account&) {
      throw HTTPError(404, "Account does not exist");
    }
  });

  this->router.add(HTTPRequest::Method::GET, "/y/teams", [this](ArgsT&&) -> RetT {
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::dict());
    for (const auto& it : this->state->team_index->all()) {
      res->emplace(std::format("{}", it->team_id), it->json());
    }
    co_return res;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/team/:team_id", [this](ArgsT&& args) -> RetT {
    uint32_t team_id = args.get_param<uint32_t>("team_id");
    auto team = this->state->team_index->get_by_id(team_id);
    if (!team) {
      throw HTTPError(404, "Team does not exist");
    }
    co_return std::make_shared<phosg::JSON>(team->json());
  });

  this->router.add(HTTPRequest::Method::GET, "/y/team/:team_id/flag", [this](ArgsT&& args) -> RetT {
    uint32_t team_id = args.get_param<uint32_t>("team_id");
    auto team = this->state->team_index->get_by_id(team_id);
    if (!team) {
      throw HTTPError(404, "Team does not exist");
    }
    if (!team->flag_data) {
      throw HTTPError(404, "Team does not have a flag");
    }
    auto img = team->decode_flag_data();
    co_return RawResponse{.content_type = "image/png", .data = img.serialize(phosg::ImageFormat::PNG)};
  });

  std::function<phosg::JSON()> generate_server_info_json = [this]() -> phosg::JSON {
    size_t game_count = 0;
    size_t lobby_count = 0;
    for (const auto& it : this->state->id_to_lobby) {
      if (it.second->is_game()) {
        game_count++;
      } else {
        lobby_count++;
      }
    }
    uint64_t uptime_usecs = phosg::now() - this->state->data->creation_time;
    return phosg::JSON::dict({
        {"StartTimeUsecs", this->state->data->creation_time},
        {"StartTime", phosg::format_time(this->state->data->creation_time)},
        {"UptimeUsecs", uptime_usecs},
        {"Uptime", phosg::format_duration(uptime_usecs)},
        {"LobbyCount", lobby_count},
        {"GameCount", game_count},
        {"ClientCount", this->state->game_server->all_clients().size() - ProxySession::num_proxy_sessions},
        {"ProxySessionCount", ProxySession::num_proxy_sessions},
        {"ServerName", this->state->data->name},
    });
  };

  this->router.add(HTTPRequest::Method::GET, "/y/server", [generate_server_info_json](ArgsT&&) -> RetT {
    co_return std::make_shared<phosg::JSON>(generate_server_info_json());
  });

  this->router.add(HTTPRequest::Method::GET, "/y/config", [this](ArgsT&&) -> RetT {
    co_return this->state->data->config_json;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/summary", [this, generate_server_info_json](ArgsT&&) -> RetT {
    auto clients_json = phosg::JSON::list();
    for (const auto& c : this->state->game_server->all_clients()) {
      auto p = c->character_file(false, false);
      auto l = c->lobby.lock();
      clients_json.emplace_back(phosg::JSON::dict({
          {"ID", c->id},
          {"AccountID", c->login ? c->login->account->account_id : phosg::JSON(nullptr)},
          {"Name", p ? p->disp.visual.name.decode(c->language()) : phosg::JSON(nullptr)},
          {"Version", phosg::name_for_enum(c->version())},
          {"Language", name_for_language(c->language())},
          {"Level", p ? (p->disp.stats.level + 1) : phosg::JSON(nullptr)},
          {"Class", p ? name_for_char_class(p->disp.visual.sh.char_class) : phosg::JSON(nullptr)},
          {"SectionID", p ? name_for_section_id(p->disp.visual.sh.section_id) : phosg::JSON(nullptr)},
          {"LobbyID", l ? l->lobby_id : phosg::JSON(nullptr)},
          {"IsOnProxy", c->proxy_session ? true : false},
      }));
    }

    auto games_json = phosg::JSON::list();
    for (const auto& it : this->state->id_to_lobby) {
      auto l = it.second;
      if (l->is_game()) {
        auto game_json = phosg::JSON::dict({
            {"ID", l->lobby_id},
            {"Name", l->name},
            {"Players", l->count_clients()},
            {"CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED)},
            {"Episode", name_for_episode(l->episode)},
            {"HasPassword", !l->password.empty()},
        });
        if (l->episode == Episode::EP3) {
          auto ep3s = l->ep3_server;
          game_json.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
          game_json.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
          game_json.emplace("MapNumber", (ep3s && ep3s->last_chosen_map) ? ep3s->last_chosen_map->map_number : phosg::JSON(nullptr));
          game_json.emplace("Rules", (ep3s && ep3s->map_and_rules) ? ep3s->map_and_rules->rules.json() : nullptr);
        } else {
          game_json.emplace("QuestSelectionInProgress", l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS));
          game_json.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
          game_json.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
          uint8_t effective_section_id = l->effective_section_id();
          if (effective_section_id < 10) {
            game_json.emplace("SectionID", name_for_section_id(effective_section_id));
          } else {
            game_json.emplace("SectionID", nullptr);
          }
          game_json.emplace("Mode", name_for_mode(l->mode));
          game_json.emplace("Difficulty", name_for_difficulty(l->difficulty));
          game_json.emplace("Quest", l->quest ? l->quest->json() : phosg::JSON(nullptr));
        }
        games_json.emplace_back(std::move(game_json));
      }
    }

    co_return std::make_shared<phosg::JSON>(phosg::JSON::dict({
        {"Clients", std::move(clients_json)},
        {"Games", std::move(games_json)},
        {"Server", generate_server_info_json()},
    }));
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/ep3/cards", [this](ArgsT&& args) -> RetT {
    auto& index = args.req.query_params.count("trial")
        ? this->state->data->ep3_card_index_trial
        : this->state->data->ep3_card_index;
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
      return std::make_shared<phosg::JSON>(index->definitions_json());
    });
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/ep3/card/:card_id", [this](ArgsT&& args) -> RetT {
    auto& index = args.req.query_params.count("trial")
        ? this->state->data->ep3_card_index_trial
        : this->state->data->ep3_card_index;
    uint32_t card_id = args.get_param<uint32_t>("card_id");
    try {
      co_return std::make_shared<phosg::JSON>(index->definition_for_id(card_id)->def.json());
    } catch (const std::out_of_range&) {
      throw HTTPError(404, "Card definition does not exist");
    }
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/ep3/maps", [this](ArgsT&&) -> RetT {
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
      auto ret = std::make_shared<phosg::JSON>(phosg::JSON::dict());
      for (const auto& [map_number, map] : this->state->data->ep3_map_index->all_maps()) {
        auto languages_json = phosg::JSON::list();
        for (const auto& vm : map->all_versions()) {
          if (vm) {
            languages_json.emplace_back(name_for_language(vm->language));
          }
        }
        auto map_json = phosg::JSON::dict({
            {"Name", map->version(Language::ENGLISH)->map->name.decode(Language::ENGLISH)},
            {"VisibilityFlags", map->visibility_flags},
            {"Languages", std::move(languages_json)},
        });
        ret->emplace(std::format("{:08X}", map_number), std::move(map_json));
      }
      return ret;
    });
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/ep3/map/:map_number/:language", [this](ArgsT&& args) -> RetT {
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
      try {
        auto map = this->state->data->ep3_map_index->map_for_id(args.get_param<uint32_t>("map_number", true));
        auto vm = map->version(language_for_name(args.params.at("language")));
        return std::make_shared<phosg::JSON>(vm->map->json(vm->language));
      } catch (const std::out_of_range&) {
        throw HTTPError(404, "Map version does not exist");
      }
    });
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/ep3/map/:map_number/:language/raw", [this](ArgsT&& args) -> RetT {
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> RawResponse {
      try {
        auto map = this->state->data->ep3_map_index->map_for_id(args.get_param<uint32_t>("map_number"));
        auto vm = map->version(language_for_name(args.params.at("language")));
        std::string data(reinterpret_cast<const char*>(vm->map.get()), sizeof(Episode3::MapDefinition));
        return RawResponse{.content_type = "application/octet-stream", .data = std::move(data)};
      } catch (const std::out_of_range&) {
        throw HTTPError(404, "Map version does not exist");
      }
    });
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/common-tables", [this](ArgsT&&) -> RetT {
    auto ret = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& it : this->state->data->common_item_sets) {
      ret->emplace_back(it.first);
    }
    co_return ret;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/common-table/:table_name", [this](ArgsT&& args) -> RetT {
    try {
      const auto& table = this->state->data->common_item_sets.at(args.params.at("table_name"));
      co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
        return std::make_shared<phosg::JSON>(table->json());
      });
    } catch (const std::out_of_range&) {
      throw HTTPError(404, "Table does not exist");
    }
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/rare-tables", [this](ArgsT&&) -> RetT {
    auto ret = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& it : this->state->data->rare_item_sets) {
      ret->emplace_back(it.first);
    }
    co_return ret;
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/rare-table/:table_name", [this](ArgsT&& args) -> RetT {
    try {
      const auto& table_name = args.params.at("table_name");
      const auto& table = this->state->data->rare_item_sets.at(table_name);
      std::shared_ptr<const ItemNameIndex> name_index;
      if (table_name.ends_with("-v1")) {
        name_index = this->state->data->item_name_index_opt(Version::DC_V1);
      } else if (table_name.ends_with("-v2")) {
        name_index = this->state->data->item_name_index_opt(Version::PC_V2);
      } else if (table_name.ends_with("-v3")) {
        name_index = this->state->data->item_name_index_opt(Version::GC_V3);
      } else if (table_name.ends_with("-v4")) {
        name_index = this->state->data->item_name_index_opt(Version::BB_V4);
      }
      co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
        return std::make_shared<phosg::JSON>(table->json(name_index));
      });
    } catch (const std::out_of_range&) {
      throw HTTPError(404, "Table does not exist");
    }
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/quests", [this](ArgsT&&) -> RetT {
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
      return std::make_shared<phosg::JSON>(this->state->data->quest_index->json());
    });
  });

  this->router.add(HTTPRequest::Method::GET, "/y/data/quest/:quest_num", [this](ArgsT&& args) -> RetT {
    uint32_t quest_num = args.get_param<uint32_t>("quest_num");
    auto q = this->state->data->quest_index->get(quest_num);
    if (!q) {
      throw HTTPError(404, "Quest does not exist");
    }
    co_return std::make_shared<phosg::JSON>(q->json());
  });

  // -----------------------------------------------------------------------
  // /y/data/level-tables — class-by-class EXP thresholds and stat deltas.
  //
  // Returns the contents of system/tables/level-table-{v1-v2,v3,v4}.json in
  // a single response so dashboards can compute "EXP to next level" without
  // shipping a 700 KB copy of the tables themselves. Each top-level key is
  // a version family; each value preserves the on-disk schema:
  //
  //   {
  //     "v1v2": { "BaseStats": [...12 entries], "LevelDeltas": [[...200]×12] },
  //     "v3":   { ...same shape... },
  //     "v4":   { ...same shape... }
  //   }
  //
  // The shape matches what ServerState::load_level_tables() loads at boot;
  // we re-read from disk here so the endpoint reflects on-disk truth even
  // if the in-memory tables get reshaped by a future refactor. The data is
  // static — dashboards should cache aggressively (the JSON parse is the
  // expensive part, not the I/O).
  // -----------------------------------------------------------------------
  this->router.add(HTTPRequest::Method::GET, "/y/data/level-tables", [this](ArgsT&&) -> RetT {
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::shared_ptr<phosg::JSON> {
      auto res = std::make_shared<phosg::JSON>(phosg::JSON::dict());
      static const std::array<std::pair<const char*, const char*>, 3> tables = {{
          {"v1v2", "system/tables/level-table-v1-v2.json"},
          {"v3", "system/tables/level-table-v3.json"},
          {"v4", "system/tables/level-table-v4.json"},
      }};
      for (const auto& [name, path] : tables) {
        try {
          res->emplace(name, phosg::JSON::parse(phosg::load_file(path)));
        } catch (const std::exception&) {
          // Missing or unparseable tables are silently skipped — the
          // dashboard already handles partial coverage (e.g. when only
          // some versions' players have logged in).
        }
      }
      return res;
    });
  });

  // -----------------------------------------------------------------------
  // /y/characters — sanitized list of every saved character on the server.
  //
  // Walks system/players/, dedupes by (account_id, slot_index) across:
  //   - backup_player_*.psochar  (manual `$savechar` snapshots, legacy)
  //   - auto_player_*.psochar    (auto-snapshot: login + every 60s + on
  //                               disconnect; preferred when both exist)
  // and emits a JSON list. Per-entry shape:
  //
  //   {
  //     "AccountID":       42,
  //     "SlotIndex":       0,
  //     "Name":            "Sonic",
  //     "Class":           "HUmar",                  // 12 PSO classes
  //     "SectionID":       "Pinkal",                 // 10 PSO section IDs
  //     "Level":           42,                       // display value (1-based)
  //     "EXP":             123456,
  //     "Meseta":          99999,
  //     "PlayTimeSeconds": 720000,                   // total play time
  //     "Stats":           {"ATP":..., "DFP":..., "MST":..., "ATA":...,
  //                         "EVP":..., "LCK":..., "HP":...},  // base stats
  //   }
  //   {
  //     ...as above...,
  //     "Inventory": [
  //       {
  //         "Name":     "+30 Hit Vjaya +99",          // human-readable
  //         "Kind":     "weapon",                     // weapon/armor/shield/unit/mag/tool/meseta/unknown
  //         "Equipped": true                          // bit 0x08 of slot.flags
  //       },
  //       ...up to 30 entries (PSO inventory cap)...
  //     ]
  //   }
  //
  // Excluded for privacy: serial numbers, access keys, passwords, IP
  // addresses, session tokens, raw inventory data, bank contents, guild
  // card data, auto-reply text, info-board text, choice-search config.
  // Item names are resolved via the BB item parameter table because the
  // .psochar file is BB-format internally regardless of which client
  // populated it; items unique to V3 or earlier may not resolve and fall
  // back to "Unknown" (rare in practice — most items are version-shared).
  this->router.add(HTTPRequest::Method::GET, "/y/characters", [this](ArgsT&&) -> RetT {
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    // The .psochar files are BB-format, so we resolve item names against
    // the BB item parameter table. If that table isn't loaded for some
    // reason, all items fall through to "Unknown" rather than crashing.
    auto bb_name_index = this->state->data->item_name_index_opt(Version::BB_V4);
    // BB item-parameter table, for the per-item Rare flag below.
    auto bb_pmt = this->state->data->item_parameter_table(Version::BB_V4);
    walk_backup_characters([&res, &bb_name_index, &bb_pmt](
                               std::shared_ptr<PSOBBCharacterFile> ch,
                               uint32_t account_id,
                               size_t slot_index) {
      // -- Inventory resolution ------------------------------------------
      // PSO inventory caps at 30 slots. num_items is the count of slots
      // actually populated; trailing entries are zero-filled and not
      // displayed.
      auto inventory_json = phosg::JSON::list();
      const auto& inv = ch->inventory;
      const size_t n = std::min<size_t>(inv.num_items, 30);
      for (size_t i = 0; i < n; i++) {
        const auto& slot = inv.items[i];

        // Item kind comes from data1[0] (top-level type) and data1[1]
        // (subtype for guard items). The same byte pattern appears in
        // every PSO version, so no version dispatch needed.
        const char* kind = "unknown";
        switch (slot.data.data1[0]) {
          case 0x00:
            kind = "weapon";
            break;
          case 0x01:
            switch (slot.data.data1[1]) {
              case 0x01: kind = "armor"; break;
              case 0x02: kind = "shield"; break;
              case 0x03: kind = "unit"; break;
              default:   kind = "guard"; break;
            }
            break;
          case 0x02: kind = "mag"; break;
          case 0x03: kind = "tool"; break;
          case 0x04: kind = "meseta"; break;
        }

        // describe_item produces a rich human-readable string with
        // grinders, attribute %, photon blasts, mag stats, etc. Use the
        // BB item name index — see comment above.
        std::string name;
        if (bb_name_index) {
          try {
            name = bb_name_index->describe_item(slot.data);
          } catch (const std::exception&) {
            name = "Unknown";
          }
        } else {
          name = "Unknown";
        }

        // Rare per the BB item-parameter table — drives the dashboard's gold
        // highlight, matching how the game colours rare items.
        bool item_rare = false;
        if (bb_pmt) {
          try {
            item_rare = bb_pmt->is_item_rare(slot.data);
          } catch (const std::exception&) {}
        }
        inventory_json.emplace_back(phosg::JSON::dict({
            {"Name", std::move(name)},
            {"Kind", kind},
            // Bit 0x08 of the PlayerInventoryItem flags marks "currently
            // equipped." See the comment in PlayerInventoryItemT for the
            // full bit layout.
            {"Equipped", static_cast<bool>(slot.flags & 0x00000008)},
            {"Rare", item_rare},
        }));
      }

      // SnapshotVersion lets the dashboard trust server-authoritative BB
      // EXP / play-time without inferring the version from account licenses
      // (an account can hold both BB and non-BB licenses). Null when no
      // sidecar exists (legacy snapshot or a manual $savechar backup).
      // Saved quest-completion flags, per difficulty. The game persists these
      // permanently, so the dashboard can tell which quests this character has
      // EVER cleared (retroactive — not just clears since logging began) by
      // intersecting the set indices with each quest's CompletionFlag.
      auto quest_flags_json = phosg::JSON::dict();
      for (uint8_t d = 0; d < 4; d++) {
        Difficulty diff = static_cast<Difficulty>(d);
        auto set_flags = phosg::JSON::list();
        for (uint16_t fi = 0; fi < 0x400; fi++) {
          if (ch->quest_flags.get(diff, fi)) {
            set_flags.emplace_back(static_cast<int64_t>(fi));
          }
        }
        quest_flags_json.emplace(name_for_difficulty(diff), std::move(set_flags));
      }

      std::string snapshot_version = read_snapshot_version(account_id, slot_index);
      res->emplace_back(phosg::JSON::dict({
          {"AccountID", account_id},
          {"SlotIndex", slot_index},
          {"Name", ch->disp.visual.name.decode()},
          {"Class", name_for_char_class(ch->disp.visual.sh.char_class)},
          {"SectionID", name_for_section_id(ch->disp.visual.sh.section_id)},
          {"Level", static_cast<uint32_t>(ch->disp.stats.level.load() + 1)},
          {"EXP", ch->disp.stats.exp.load()},
          {"Meseta", ch->disp.stats.meseta.load()},
          {"PlayTimeSeconds", ch->play_time_seconds.load()},
          {"Stats", ch->disp.stats.char_stats.json()},
          {"Inventory", std::move(inventory_json)},
          {"QuestFlags", std::move(quest_flags_json)},
          {"SnapshotVersion", snapshot_version.empty() ? phosg::JSON(nullptr) : phosg::JSON(snapshot_version)},
      }));
    });
    co_return res;
  });

  // -----------------------------------------------------------------------
  // /y/data/quest/:quest_num/plays
  //
  // For a given quest number, the characters who have PLAYED it, with a
  // per-character play count and last-played time. Each entry shape:
  //
  //   { "AccountID": 42, "SlotIndex": 0, "Name": "Sonic",
  //     "PlayCount": 3, "LastPlayedUsecs": 1781799523133000 }
  //
  // Sourced from system/players/quest_plays.jsonl, which records every quest
  // load — the only quest event the server authoritatively observes (online
  // quests run client-side, so there is no server-visible "completed" signal).
  // This replaces the old flag-based "completions" endpoint, which mis-read
  // gameplay quest_flags (set during ordinary play) as quest completions.
  this->router.add(HTTPRequest::Method::GET, "/y/data/quest/:quest_num/plays", [](ArgsT&& args) -> RetT {
    const uint32_t quest_num = args.get_param<uint32_t>("quest_num");
    struct PlayAgg {
      uint32_t account_id = 0;
      uint32_t slot_index = 0;
      std::string name;
      uint32_t play_count = 0;
      uint64_t last_played_usecs = 0;
    };
    std::map<std::pair<uint32_t, uint32_t>, PlayAgg> by_char;
    walk_jsonl_records("quest_plays.jsonl", [&](const phosg::JSON& rec) {
      if (rec.get_int("QuestNumber", -1) != static_cast<int64_t>(quest_num)) {
        return;
      }
      uint32_t account_id = rec.get_int("AccountID", 0);
      uint32_t slot_index = rec.get_int("SlotIndex", 0);
      auto& agg = by_char[std::make_pair(account_id, slot_index)];
      agg.account_id = account_id;
      agg.slot_index = slot_index;
      agg.play_count++;
      uint64_t t = rec.get_int("TimeUsecs", 0);
      if (t > agg.last_played_usecs) {
        agg.last_played_usecs = t;
      }
      agg.name = rec.get_string("CharacterName", agg.name);
    });
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& [key, agg] : by_char) {
      res->emplace_back(phosg::JSON::dict({
          {"AccountID", agg.account_id},
          {"SlotIndex", agg.slot_index},
          {"Name", agg.name},
          {"PlayCount", agg.play_count},
          {"LastPlayedUsecs", agg.last_played_usecs},
      }));
    }
    co_return res;
  });

  // -----------------------------------------------------------------------
  // /y/character/:account_id/:slot_index/quest-plays
  //
  // The inverse of /y/data/quest/:quest_num/plays — for one character, the
  // quests they've PLAYED, aggregated per quest number with a play count,
  // last-played time, and the set of difficulties seen. Per-entry shape:
  //
  //   {
  //     "QuestNumber":      42,
  //     "PlayCount":        3,
  //     "LastPlayedUsecs":  1781799523133000,
  //     "Difficulties":     ["Normal", "Hard"]    // any subset of N/H/VH/U
  //   }
  //
  // Sourced from system/players/quest_plays.jsonl (see log_quest_play in
  // ReceiveCommands.cc). Replaces the old flag-based "completions" endpoint,
  // which reported false positives: PSO quest_flags are general gameplay
  // state set during ordinary play, not a per-quest completion record, and
  // their bit indices are unrelated to quest catalog numbers.
  // -----------------------------------------------------------------------
  this->router.add(HTTPRequest::Method::GET, "/y/character/:account_id/:slot_index/quest-plays",
      [](ArgsT&& args) -> RetT {
        const uint32_t account_id = args.get_param<uint32_t>("account_id");
        const uint32_t slot_index = args.get_param<uint32_t>("slot_index");

        static const std::array<const char*, 4> DIFFICULTY_NAMES =
            {"Normal", "Hard", "VHard", "Ultimate"};

        struct QuestAgg {
          uint32_t play_count = 0;
          uint64_t last_played_usecs = 0;
          std::set<int> difficulties;
        };
        std::map<uint32_t, QuestAgg> by_quest;
        walk_jsonl_records("quest_plays.jsonl", [&](const phosg::JSON& rec) {
          if (rec.get_int("AccountID", -1) != static_cast<int64_t>(account_id)) {
            return;
          }
          if (rec.get_int("SlotIndex", -1) != static_cast<int64_t>(slot_index)) {
            return;
          }
          uint32_t quest_number = rec.get_int("QuestNumber", 0);
          auto& agg = by_quest[quest_number];
          agg.play_count++;
          uint64_t t = rec.get_int("TimeUsecs", 0);
          if (t > agg.last_played_usecs) {
            agg.last_played_usecs = t;
          }
          int diff = rec.get_int("Difficulty", -1);
          if (diff >= 0 && diff < 4) {
            agg.difficulties.insert(diff);
          }
        });

        auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
        for (const auto& [quest_number, agg] : by_quest) {
          auto difficulties = phosg::JSON::list();
          for (int diff : agg.difficulties) {
            difficulties.emplace_back(DIFFICULTY_NAMES[diff]);
          }
          res->emplace_back(phosg::JSON::dict({
              {"QuestNumber", quest_number},
              {"PlayCount", agg.play_count},
              {"LastPlayedUsecs", agg.last_played_usecs},
              {"Difficulties", std::move(difficulties)},
          }));
        }
        co_return res;
      });

  // -----------------------------------------------------------------------
  // /y/data/quest/:quest_num/completions
  //
  // For a given quest number, the characters who have CLEARED it, with a
  // per-character completion count and last-completed time. Sourced from
  // system/players/quest_completions.jsonl, which is appended only when the
  // quest's configured CompletionFlag is set during that quest (see
  // log_quest_completion in ReceiveSubcommands.cc). Quests without a mapped
  // CompletionFlag never produce completions — by design — so this endpoint
  // can never report a false clear (unlike the old flag-guessing endpoint).
  this->router.add(HTTPRequest::Method::GET, "/y/data/quest/:quest_num/completions", [](ArgsT&& args) -> RetT {
    const uint32_t quest_num = args.get_param<uint32_t>("quest_num");
    struct CompAgg {
      uint32_t account_id = 0;
      uint32_t slot_index = 0;
      std::string name;
      uint32_t completion_count = 0;
      uint64_t last_completed_usecs = 0;
    };
    std::map<std::pair<uint32_t, uint32_t>, CompAgg> by_char;
    walk_jsonl_records("quest_completions.jsonl", [&](const phosg::JSON& rec) {
      if (rec.get_int("QuestNumber", -1) != static_cast<int64_t>(quest_num)) {
        return;
      }
      uint32_t account_id = rec.get_int("AccountID", 0);
      uint32_t slot_index = rec.get_int("SlotIndex", 0);
      auto& agg = by_char[std::make_pair(account_id, slot_index)];
      agg.account_id = account_id;
      agg.slot_index = slot_index;
      agg.completion_count++;
      uint64_t t = rec.get_int("TimeUsecs", 0);
      if (t > agg.last_completed_usecs) {
        agg.last_completed_usecs = t;
      }
      agg.name = rec.get_string("CharacterName", agg.name);
    });
    auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
    for (const auto& [key, agg] : by_char) {
      res->emplace_back(phosg::JSON::dict({
          {"AccountID", agg.account_id},
          {"SlotIndex", agg.slot_index},
          {"Name", agg.name},
          {"CompletionCount", agg.completion_count},
          {"LastCompletedUsecs", agg.last_completed_usecs},
      }));
    }
    co_return res;
  });

  // -----------------------------------------------------------------------
  // /y/character/:account_id/:slot_index/quest-completions
  //
  // The inverse of /y/data/quest/:quest_num/completions — for one character,
  // the quests they've CLEARED, aggregated per quest number with a completion
  // count, last-completed time, and the difficulties cleared. Sourced from
  // system/players/quest_completions.jsonl; only quests with a mapped
  // CompletionFlag ever appear here.
  // -----------------------------------------------------------------------
  this->router.add(HTTPRequest::Method::GET, "/y/character/:account_id/:slot_index/quest-completions",
      [](ArgsT&& args) -> RetT {
        const uint32_t account_id = args.get_param<uint32_t>("account_id");
        const uint32_t slot_index = args.get_param<uint32_t>("slot_index");

        static const std::array<const char*, 4> DIFFICULTY_NAMES =
            {"Normal", "Hard", "VHard", "Ultimate"};

        struct QuestAgg {
          uint32_t completion_count = 0;
          uint64_t last_completed_usecs = 0;
          std::set<int> difficulties;
        };
        std::map<uint32_t, QuestAgg> by_quest;
        walk_jsonl_records("quest_completions.jsonl", [&](const phosg::JSON& rec) {
          if (rec.get_int("AccountID", -1) != static_cast<int64_t>(account_id)) {
            return;
          }
          if (rec.get_int("SlotIndex", -1) != static_cast<int64_t>(slot_index)) {
            return;
          }
          uint32_t quest_number = rec.get_int("QuestNumber", 0);
          auto& agg = by_quest[quest_number];
          agg.completion_count++;
          uint64_t t = rec.get_int("TimeUsecs", 0);
          if (t > agg.last_completed_usecs) {
            agg.last_completed_usecs = t;
          }
          int diff = rec.get_int("Difficulty", -1);
          if (diff >= 0 && diff < 4) {
            agg.difficulties.insert(diff);
          }
        });

        auto res = std::make_shared<phosg::JSON>(phosg::JSON::list());
        for (const auto& [quest_number, agg] : by_quest) {
          auto difficulties = phosg::JSON::list();
          for (int diff : agg.difficulties) {
            difficulties.emplace_back(DIFFICULTY_NAMES[diff]);
          }
          res->emplace_back(phosg::JSON::dict({
              {"QuestNumber", quest_number},
              {"CompletionCount", agg.completion_count},
              {"LastCompletedUsecs", agg.last_completed_usecs},
              {"Difficulties", std::move(difficulties)},
          }));
        }
        co_return res;
      });
}

asio::awaitable<void> HTTPServer::send_rare_drop_notification(std::shared_ptr<const phosg::JSON> message) {
  if (!this->rare_drop_subscribers.empty()) {
    std::string data = message->serialize();

    // Make a copy of the rare drop subscribers set, so we can guarantee that the client objects are all valid until
    // this coroutine returns
    std::unordered_set<std::shared_ptr<HTTPClient>> subscribers = this->rare_drop_subscribers;

    size_t expected_results = subscribers.size();
    AsyncPromise<void> complete_promise;
    auto fn = [this, &data, &expected_results, &complete_promise](std::shared_ptr<HTTPClient> c) -> asio::awaitable<void> {
      try {
        co_await c->send_websocket_message(data);
      } catch (const std::exception& e) {
        auto remote_s = str_for_endpoint(c->r.get_socket().remote_endpoint());
        this->log.info_f("Failed to send WebSocket message to {}: {}", remote_s, e.what());
      }
      if (--expected_results == 0) {
        complete_promise.set_value();
      }
      co_return;
    };
    for (const auto& c : subscribers) {
      asio::co_spawn(co_await asio::this_coro::executor, fn(c), asio::detached);
    }
    co_await complete_promise.get();
  }
  co_return;
}

asio::awaitable<std::unique_ptr<HTTPResponse>> HTTPServer::handle_request(std::shared_ptr<HTTPClient> c, HTTPRequest&& req) {
  std::variant<RawResponse, std::shared_ptr<const phosg::JSON>> ret;
  uint32_t serialize_options = phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY;
  uint64_t start_time = phosg::now();

  this->log.info_f("{} ...", req.path);

  auto resp = std::make_unique<HTTPResponse>();
  resp->http_version = req.http_version;
  resp->response_code = 200;
  resp->headers.emplace("Server", "newserv");
  resp->headers.emplace("X-Newserv-Revision", GIT_REVISION_HASH);
  resp->headers.emplace("X-Newserv-Build-Timestamp", phosg::format_time(BUILD_TIMESTAMP));

  try {
    auto* format_param = req.get_query_param("format");
    if (format_param && (*format_param == "true")) {
      serialize_options |= phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::SORT_DICT_KEYS;
    }
    auto* hex_param = req.get_query_param("hex");
    if (hex_param && (*hex_param == "true")) {
      serialize_options |= phosg::JSON::SerializeOption::HEX_INTEGERS;
    }

    ret = co_await this->router.call_handler(c, req);

  } catch (const HTTPError& e) {
    ret = std::make_shared<phosg::JSON>(phosg::JSON::dict({{"Error", true}, {"Message", e.what()}}));
    resp->response_code = e.code;
  } catch (const std::exception& e) {
    ret = std::make_shared<phosg::JSON>(phosg::JSON::dict({{"Error", true}, {"Message", e.what()}}));
    resp->response_code = 500;
  }
  uint64_t handler_end = phosg::now();

  if (holds_alternative<std::shared_ptr<const phosg::JSON>>(ret)) {
    // If the handler returns nullptr (not JSON null), assume it called enable_websockets and send no response
    auto& json = get<std::shared_ptr<const phosg::JSON>>(ret);
    if (!json) {
      co_return nullptr;
    }
    resp->headers.emplace("Content-Type", "application/json");
    resp->data = co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> std::string {
      return json->serialize(serialize_options, 0);
    });
    uint64_t serialize_end = phosg::now();

    this->log.info_f("{} in [handler: {}, serialize: {}, size: {}]",
        req.path,
        phosg::format_duration(handler_end - start_time),
        phosg::format_duration(serialize_end - handler_end),
        phosg::format_size(resp->data.size()));

  } else {
    auto& raw_resp = get<RawResponse>(ret);
    resp->headers.emplace("Content-Type", std::move(raw_resp.content_type));
    resp->data = std::move(raw_resp.data);

    this->log.info_f("{} in [handler: {}, size: {}]",
        req.path,
        phosg::format_duration(handler_end - start_time),
        phosg::format_size(resp->data.size()));
  }

  co_return resp;
}

asio::awaitable<void> HTTPServer::destroy_client(std::shared_ptr<HTTPClient> c) {
  this->rare_drop_subscribers.erase(c);
  co_return;
}
