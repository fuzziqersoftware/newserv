#include "HTTPServer.hh"

#include <inttypes.h>
#include <stdlib.h>

#include <phosg/Network.hh>
#include <string>
#include <vector>

#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "Revision.hh"
#include "Server.hh"
#include "ShellCommands.hh"

using namespace std;

HTTPServer::HTTPServer(shared_ptr<ServerState> state)
    : AsyncHTTPServer(state->io_context, "[HTTPServer] "), state(state) {}

asio::awaitable<void> HTTPServer::send_rare_drop_notification(shared_ptr<const phosg::JSON> message) {
  if (!this->rare_drop_subscribers.empty()) {
    string data = message->serialize();

    // Make a copy of the rare drop subscribers set, so we can guarantee that
    // the client objects are all valid until this coroutine returns
    unordered_set<shared_ptr<HTTPClient>> subscribers = this->rare_drop_subscribers;

    size_t expected_results = subscribers.size();
    AsyncPromise<void> complete_promise;
    auto fn = [this, &data, &expected_results, &complete_promise](shared_ptr<HTTPClient> c) -> asio::awaitable<void> {
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

std::shared_ptr<phosg::JSON> HTTPServer::generate_server_version() const {
  return make_shared<phosg::JSON>(phosg::JSON::dict({
      {"ServerType", "newserv"},
      {"BuildTime", BUILD_TIMESTAMP},
      {"BuildTimeStr", phosg::format_time(BUILD_TIMESTAMP)},
      {"Revision", GIT_REVISION_HASH},
  }));
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_account_json(shared_ptr<const Account> a) const {
  auto dc_nte_licenses_json = phosg::JSON::list();
  for (const auto& it : a->dc_nte_licenses) {
    dc_nte_licenses_json.emplace_back(it.first);
  }
  auto dc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->dc_licenses) {
    dc_licenses_json.emplace_back(it.first);
  }
  auto pc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->pc_licenses) {
    pc_licenses_json.emplace_back(it.first);
  }
  auto gc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->gc_licenses) {
    gc_licenses_json.emplace_back(it.first);
  }
  auto xb_licenses_json = phosg::JSON::list();
  for (const auto& it : a->xb_licenses) {
    xb_licenses_json.emplace_back(it.first);
  }
  auto bb_licenses_json = phosg::JSON::list();
  for (const auto& it : a->bb_licenses) {
    bb_licenses_json.emplace_back(it.first);
  }
  auto auto_patches_json = phosg::JSON::list();
  for (const auto& it : a->auto_patches_enabled) {
    auto_patches_json.emplace_back(it);
  }
  return make_shared<phosg::JSON>(phosg::JSON::dict({
      {"AccountID", a->account_id},
      {"Flags", a->flags},
      {"BanEndTime", a->ban_end_time ? a->ban_end_time : phosg::JSON(nullptr)},
      {"Ep3CurrentMeseta", a->ep3_current_meseta},
      {"Ep3TotalMesetaEarned", a->ep3_total_meseta_earned},
      {"BBTeamID", a->bb_team_id},
      {"LastPlayerName", a->last_player_name},
      {"AutoReplyMessage", a->auto_reply_message},
      {"IsTemporary", a->is_temporary},
      {"DCNTELicenses", std::move(dc_nte_licenses_json)},
      {"DCLicenses", std::move(dc_licenses_json)},
      {"PCLicenses", std::move(pc_licenses_json)},
      {"GCLicenses", std::move(gc_licenses_json)},
      {"XBLicenses", std::move(xb_licenses_json)},
      {"BBLicenses", std::move(bb_licenses_json)},
      {"AutoPatchesEnabled", std::move(auto_patches_json)},
  }));
};

std::shared_ptr<phosg::JSON> HTTPServer::generate_client_json(
    shared_ptr<const Client> c, shared_ptr<const ItemNameIndex> item_name_index) const {
  auto s = c->require_server_state();

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

  auto ret = make_shared<phosg::JSON>(phosg::JSON::dict({
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
  }));
  if (c->login) {
    auto acc_json = HTTPServer::generate_account_json(c->login->account);
    ret->emplace("Account", std::move(*acc_json));
  } else {
    ret->emplace("Account", phosg::JSON());
  }
  auto l = c->lobby.lock();
  if (l) {
    ret->emplace("LobbyID", l->lobby_id);
    ret->emplace("LobbyClientID", c->lobby_client_id);
  }
  if (c->version() == Version::BB_V4) {
    ret->emplace("BBCharacterIndex", c->bb_character_index);
  }
  auto p = c->character_file(false, false);
  if (p) {
    if (!is_ep3(c->version())) {
      if (c->version() != Version::DC_NTE) {
        ret->emplace("InventoryLanguage", name_for_language(p->inventory.language));
        ret->emplace("NumHPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::HP));
        ret->emplace("NumTPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
        if (!is_v1_or_v2(c->version())) {
          ret->emplace("NumPowerMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER));
          ret->emplace("NumDefMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF));
          ret->emplace("NumMindMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND));
          ret->emplace("NumEvadeMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE));
          ret->emplace("NumLuckMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
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
      ret->emplace("InventoryItems", std::move(items_json));
      ret->emplace("ATP", p->disp.stats.char_stats.atp.load());
      ret->emplace("MST", p->disp.stats.char_stats.mst.load());
      ret->emplace("EVP", p->disp.stats.char_stats.evp.load());
      ret->emplace("HP", p->disp.stats.char_stats.hp.load());
      ret->emplace("DFP", p->disp.stats.char_stats.dfp.load());
      ret->emplace("ATA", p->disp.stats.char_stats.ata.load());
      ret->emplace("LCK", p->disp.stats.char_stats.lck.load());
      ret->emplace("EXP", p->disp.stats.experience.load());
      ret->emplace("Meseta", p->disp.stats.meseta.load());
      auto tech_levels_json = phosg::JSON::dict();
      for (size_t z = 0; z < 0x13; z++) {
        auto level = p->get_technique_level(z);
        tech_levels_json.emplace(name_for_technique(z), (level != 0xFF) ? level : phosg::JSON(nullptr));
      }
      ret->emplace("TechniqueLevels", std::move(tech_levels_json));
    }
    ret->emplace("Height", p->disp.stats.height.load());
    ret->emplace("Level", p->disp.stats.level.load());
    ret->emplace("NameColor", p->disp.visual.name_color.load());
    ret->emplace("ExtraModel", (p->disp.visual.validation_flags & 2) ? p->disp.visual.extra_model : phosg::JSON(nullptr));
    ret->emplace("SectionID", name_for_section_id(p->disp.visual.section_id));
    ret->emplace("CharClass", name_for_char_class(p->disp.visual.char_class));
    ret->emplace("Costume", p->disp.visual.costume.load());
    ret->emplace("Skin", p->disp.visual.skin.load());
    ret->emplace("Face", p->disp.visual.face.load());
    ret->emplace("Head", p->disp.visual.head.load());
    ret->emplace("Hair", p->disp.visual.hair.load());
    ret->emplace("HairR", p->disp.visual.hair_r.load());
    ret->emplace("HairG", p->disp.visual.hair_g.load());
    ret->emplace("HairB", p->disp.visual.hair_b.load());
    ret->emplace("ProportionX", p->disp.visual.proportion_x.load());
    ret->emplace("ProportionY", p->disp.visual.proportion_y.load());

    ret->emplace("Name", p->disp.name.decode(c->language()));
    ret->emplace("PlayTimeSeconds", p->play_time_seconds.load());

    ret->emplace("AutoReply", p->auto_reply.decode(c->language()));
    ret->emplace("InfoBoard", p->info_board.decode(c->language()));
    auto battle_place_counts = phosg::JSON::list({
        p->battle_records.place_counts[0].load(),
        p->battle_records.place_counts[1].load(),
        p->battle_records.place_counts[2].load(),
        p->battle_records.place_counts[3].load(),
    });
    ret->emplace("BattlePlaceCounts", std::move(battle_place_counts));
    ret->emplace("BattleDisconnectCount", p->battle_records.disconnect_count.load());

    if (!is_ep3(c->version())) {
      auto json_for_challenge_times = []<size_t Count>(const parray<ChallengeTime, Count>& times) -> phosg::JSON {
        auto times_json = phosg::JSON::list();
        for (size_t z = 0; z < times.size(); z++) {
          times_json.emplace_back(times[z].decode());
        }
        return times_json;
      };
      ret->emplace("ChallengeTitleColorXRGB1555", p->challenge_records.title_color.load());
      ret->emplace("ChallengeTimesEp1Online", json_for_challenge_times(p->challenge_records.times_ep1_online));
      ret->emplace("ChallengeTimesEp2Online", json_for_challenge_times(p->challenge_records.times_ep2_online));
      ret->emplace("ChallengeTimesEp1Offline", json_for_challenge_times(p->challenge_records.times_ep1_offline));
      ret->emplace("ChallengeGraveIsEp2", p->challenge_records.grave_is_ep2 ? true : false);
      ret->emplace("ChallengeGraveStageNum", p->challenge_records.grave_stage_num);
      ret->emplace("ChallengeGraveFloor", p->challenge_records.grave_floor);
      ret->emplace("ChallengeGraveDeaths", p->challenge_records.grave_deaths.load());
      {
        uint16_t year = 2000 + ((p->challenge_records.grave_time >> 28) & 0x0F);
        uint8_t month = (p->challenge_records.grave_time >> 24) & 0x0F;
        uint8_t day = (p->challenge_records.grave_time >> 16) & 0xFF;
        uint8_t hour = (p->challenge_records.grave_time >> 8) & 0xFF;
        uint8_t minute = p->challenge_records.grave_time & 0xFF;
        ret->emplace("ChallengeGraveTime", std::format("{:04}-{:02}-{:02} {:02}:{:02}:00", year, month, day, hour, minute));
      }
      string grave_enemy_types;
      if (p->challenge_records.grave_defeated_by_enemy_rt_index) {
        for (EnemyType type : enemy_types_for_rare_table_index(p->challenge_records.grave_is_ep2 ? Episode::EP2 : Episode::EP1, p->challenge_records.grave_defeated_by_enemy_rt_index)) {
          if (!grave_enemy_types.empty()) {
            grave_enemy_types += "/";
          }
          grave_enemy_types += phosg::name_for_enum(type);
        }
      }
      ret->emplace("ChallengeGraveDefeatedByEnemy", std::move(grave_enemy_types));
      ret->emplace("ChallengeGraveX", p->challenge_records.grave_x.load());
      ret->emplace("ChallengeGraveY", p->challenge_records.grave_y.load());
      ret->emplace("ChallengeGraveZ", p->challenge_records.grave_z.load());
      ret->emplace("ChallengeGraveTeam", p->challenge_records.grave_team.decode());
      ret->emplace("ChallengeGraveMessage", p->challenge_records.grave_message.decode());
      ret->emplace("ChallengeAwardStateEp1OnlineFlags", p->challenge_records.ep1_online_award_state.rank_award_flags.load());
      ret->emplace("ChallengeAwardStateEp1OnlineMaxRank", p->challenge_records.ep1_online_award_state.maximum_rank.decode());
      ret->emplace("ChallengeAwardStateEp2OnlineFlags", p->challenge_records.ep2_online_award_state.rank_award_flags.load());
      ret->emplace("ChallengeAwardStateEp2OnlineMaxRank", p->challenge_records.ep2_online_award_state.maximum_rank.decode());
      ret->emplace("ChallengeAwardStateEp1OfflineFlags", p->challenge_records.ep1_offline_award_state.rank_award_flags.load());
      ret->emplace("ChallengeAwardStateEp1OfflineMaxRank", p->challenge_records.ep1_offline_award_state.maximum_rank.decode());
      ret->emplace("ChallengeRankTitle", p->challenge_records.rank_title.decode());
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
    ret->emplace("ProxySession", std::move(ses_json));
  } else {
    ret->emplace("ProxySession", phosg::JSON());
  }
  return ret;
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_lobby_json(
    shared_ptr<const Lobby> l, shared_ptr<const ItemNameIndex> item_name_index) const {
  std::array<std::shared_ptr<Client>, 12> clients;

  auto client_ids_json = phosg::JSON::list();
  for (size_t z = 0; z < l->max_clients; z++) {
    client_ids_json.emplace_back(l->clients[z] ? l->clients[z]->id : phosg::JSON(nullptr));
  }

  auto ret = make_shared<phosg::JSON>(phosg::JSON::dict({
      {"ID", l->lobby_id},
      {"AllowedVersions", l->allowed_versions},
      {"Event", l->event},
      {"LeaderClientID", l->leader_id},
      {"MaxClients", l->max_clients},
      {"IdleTimeoutUsecs", l->idle_timeout_usecs},
      {"ClientIDs", std::move(client_ids_json)},
      {"IsGame", l->is_game()},
      {"IsPersistent", l->check_flag(Lobby::Flag::PERSISTENT)},
  }));

  if (l->is_game()) {
    ret->emplace("CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED));
    ret->emplace("MinLevel", l->min_level + 1);
    ret->emplace("MaxLevel", l->max_level + 1);
    ret->emplace("Episode", name_for_episode(l->episode));
    ret->emplace("HasPassword", !l->password.empty());
    ret->emplace("Name", l->name);
    ret->emplace("RandomSeed", l->random_seed);
    if (l->episode != Episode::EP3) {
      ret->emplace("QuestSelectionInProgress", l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS));
      ret->emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
      ret->emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
      ret->emplace("Variations", l->variations.json());
      uint8_t effective_section_id = l->effective_section_id();
      if (effective_section_id < 10) {
        ret->emplace("SectionID", name_for_section_id(effective_section_id));
      } else {
        ret->emplace("SectionID", nullptr);
      }
      ret->emplace("Mode", name_for_mode(l->mode));
      ret->emplace("Difficulty", name_for_difficulty(l->difficulty));
      ret->emplace("BaseEXPMultiplier", l->base_exp_multiplier);
      ret->emplace("EXPShareMultiplier", l->exp_share_multiplier);
      ret->emplace("AllowedDropModes", l->allowed_drop_modes);
      switch (l->drop_mode) {
        case ServerDropMode::DISABLED:
          ret->emplace("DropMode", "none");
          break;
        case ServerDropMode::CLIENT:
          ret->emplace("DropMode", "client");
          break;
        case ServerDropMode::SERVER_SHARED:
          ret->emplace("DropMode", "shared");
          break;
        case ServerDropMode::SERVER_PRIVATE:
          ret->emplace("DropMode", "private");
          break;
        case ServerDropMode::SERVER_DUPLICATE:
          ret->emplace("DropMode", "duplicate");
          break;
      }
      if (l->mode == GameMode::CHALLENGE) {
        ret->emplace("ChallengeEXPMultiplier", l->challenge_exp_multiplier);
        if (l->challenge_params) {
          ret->emplace("ChallengeStageNumber", l->challenge_params->stage_number);
          ret->emplace("ChallengeRankColor", l->challenge_params->rank_color);
          ret->emplace("ChallengeRankText", l->challenge_params->rank_text);
          ret->emplace("ChallengeRank0ThresholdBitmask", l->challenge_params->rank_thresholds[0].bitmask);
          ret->emplace("ChallengeRank0ThresholdSeconds", l->challenge_params->rank_thresholds[0].seconds);
          ret->emplace("ChallengeRank1ThresholdBitmask", l->challenge_params->rank_thresholds[1].bitmask);
          ret->emplace("ChallengeRank1ThresholdSeconds", l->challenge_params->rank_thresholds[1].seconds);
          ret->emplace("ChallengeRank2ThresholdBitmask", l->challenge_params->rank_thresholds[2].bitmask);
          ret->emplace("ChallengeRank2ThresholdSeconds", l->challenge_params->rank_thresholds[2].seconds);
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
      ret->emplace("FloorItems", std::move(floor_items_json));
      ret->emplace("Quest", l->quest ? l->quest->json() : phosg::JSON(nullptr));

    } else {
      ret->emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
      ret->emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
      ret->emplace("SpectatorsForbidden", l->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN));

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
                } catch (const out_of_range&) {
                  cards_json.emplace_back(deck_entry->card_ids[w].load());
                }
              }
              deck_json = phosg::JSON::dict({
                  {"Name", deck_entry->name.decode(lc ? lc->language() : Language::ENGLISH)},
                  {"TeamID", deck_entry->team_id.load()},
                  {"Cards", std::move(cards_json)},
                  {"GodWhimFlag", deck_entry->god_whim_flag},
                  {"PlayerLevel", deck_entry->player_level.load()},
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
        });
        // std::shared_ptr<StateFlags> state_flags;
        // std::array<std::shared_ptr<PlayerState>, 4> player_states;
        ret->emplace("Episode3BattleState", std::move(battle_state_json));
      } else {
        ret->emplace("Episode3BattleState", nullptr);
      }
      auto watched_lobby = l->watched_lobby.lock();
      if (watched_lobby) {
        ret->emplace("WatchedLobbyID", watched_lobby->lobby_id);
      }
      auto watcher_lobby_ids_json = phosg::JSON::list();
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        watcher_lobby_ids_json.emplace_back(watcher_lobby->lobby_id);
      }
      ret->emplace("WatcherLobbyIDs", std::move(watcher_lobby_ids_json));
      ret->emplace("IsReplayLobby", !!l->battle_player);
    }

  } else { // Not game
    ret->emplace("IsPublic", l->check_flag(Lobby::Flag::PUBLIC));
    ret->emplace("IsDefault", l->check_flag(Lobby::Flag::DEFAULT));
    ret->emplace("IsOverflow", l->check_flag(Lobby::Flag::IS_OVERFLOW));
    ret->emplace("Block", l->block);
  }
  return ret;
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_accounts_json() const {
  auto res = make_shared<phosg::JSON>(phosg::JSON::list());
  for (const auto& it : this->state->account_index->all()) {
    res->emplace_back(it->json());
  }
  return res;
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_clients_json() const {
  auto res = make_shared<phosg::JSON>(phosg::JSON::list());
  for (const auto& it : this->state->game_server->all_clients()) {
    auto client_json = this->generate_client_json(it, this->state->item_name_index_opt(it->version()));
    res->emplace_back(std::move(*client_json));
  }
  return res;
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_server_info_json() const {
  size_t game_count = 0;
  size_t lobby_count = 0;
  for (const auto& it : this->state->id_to_lobby) {
    if (it.second->is_game()) {
      game_count++;
    } else {
      lobby_count++;
    }
  }
  uint64_t uptime_usecs = phosg::now() - this->state->creation_time;
  return make_shared<phosg::JSON>(phosg::JSON::dict({
      {"StartTimeUsecs", this->state->creation_time},
      {"StartTime", phosg::format_time(this->state->creation_time)},
      {"UptimeUsecs", uptime_usecs},
      {"Uptime", phosg::format_duration(uptime_usecs)},
      {"LobbyCount", lobby_count},
      {"GameCount", game_count},
      {"ClientCount", this->state->game_server->all_clients().size() - ProxySession::num_proxy_sessions},
      {"ProxySessionCount", ProxySession::num_proxy_sessions},
      {"ServerName", this->state->name},
  }));
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_lobbies_json() const {
  auto res = make_shared<phosg::JSON>(phosg::JSON::list());
  for (const auto& it : this->state->id_to_lobby) {
    auto leader = it.second->clients[it.second->leader_id];
    Version v = leader ? leader->version() : Version::BB_V4;
    auto lobby_json = this->generate_lobby_json(it.second, this->state->item_name_index_opt(v));
    res->emplace_back(std::move(*lobby_json));
  }
  return res;
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_summary_json() const {
  auto clients_json = phosg::JSON::list();
  for (const auto& c : this->state->game_server->all_clients()) {
    auto p = c->character_file(false, false);
    auto l = c->lobby.lock();
    clients_json.emplace_back(phosg::JSON::dict({
        {"ID", c->id},
        {"AccountID", c->login ? c->login->account->account_id : phosg::JSON(nullptr)},
        {"Name", p ? p->disp.name.decode(c->language()) : phosg::JSON(nullptr)},
        {"Version", phosg::name_for_enum(c->version())},
        {"Language", name_for_language(c->language())},
        {"Level", p ? p->disp.stats.level + 1 : phosg::JSON(nullptr)},
        {"Class", p ? name_for_char_class(p->disp.visual.char_class) : phosg::JSON(nullptr)},
        {"SectionID", p ? name_for_section_id(p->disp.visual.section_id) : phosg::JSON(nullptr)},
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

  auto server_json = this->generate_server_info_json();
  return make_shared<phosg::JSON>(phosg::JSON::dict({
      {"Clients", std::move(clients_json)},
      {"Games", std::move(games_json)},
      {"Server", std::move(*server_json)},
  }));
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_all_json() const {
  auto clients_json = this->generate_clients_json();
  auto lobbies_json = this->generate_lobbies_json();
  auto server_json = this->generate_server_info_json();
  return make_shared<phosg::JSON>(phosg::JSON::dict({
      {"Clients", std::move(*clients_json)},
      {"Lobbies", std::move(*lobbies_json)},
      {"Server", std::move(*server_json)},
  }));
}

asio::awaitable<std::shared_ptr<phosg::JSON>> HTTPServer::generate_ep3_cards_json(bool trial) const {
  auto& index = trial ? this->state->ep3_card_index_trial : this->state->ep3_card_index;
  co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> shared_ptr<phosg::JSON> {
    return make_shared<phosg::JSON>(index->definitions_json());
  });
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_common_table_list_json() const {
  auto ret = make_shared<phosg::JSON>(phosg::JSON::list());
  for (const auto& it : this->state->common_item_sets) {
    ret->emplace_back(it.first);
  }
  return ret;
}

asio::awaitable<std::shared_ptr<phosg::JSON>> HTTPServer::generate_common_table_json(const std::string& table_name) const {
  try {
    const auto& table = this->state->common_item_sets.at(table_name);
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> shared_ptr<phosg::JSON> {
      return make_shared<phosg::JSON>(table->json());
    });
  } catch (const out_of_range&) {
    throw HTTPError(404, "Table does not exist");
  }
}

std::shared_ptr<phosg::JSON> HTTPServer::generate_rare_table_list_json() const {
  auto ret = make_shared<phosg::JSON>(phosg::JSON::list());
  for (const auto& it : this->state->rare_item_sets) {
    ret->emplace_back(it.first);
  }
  return ret;
}

asio::awaitable<std::shared_ptr<phosg::JSON>> HTTPServer::generate_rare_table_json(const std::string& table_name) const {
  try {
    const auto& table = this->state->rare_item_sets.at(table_name);
    shared_ptr<const ItemNameIndex> name_index;
    if (table_name.ends_with("-v1")) {
      name_index = this->state->item_name_index_opt(Version::DC_V1);
    } else if (table_name.ends_with("-v2")) {
      name_index = this->state->item_name_index_opt(Version::PC_V2);
    } else if (table_name.ends_with("-v3")) {
      name_index = this->state->item_name_index_opt(Version::GC_V3);
    } else if (table_name.ends_with("-v4")) {
      name_index = this->state->item_name_index_opt(Version::BB_V4);
    }
    co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> shared_ptr<phosg::JSON> {
      return make_shared<phosg::JSON>(table->json(name_index));
    });
  } catch (const out_of_range&) {
    throw HTTPError(404, "Table does not exist");
  }
}

asio::awaitable<std::shared_ptr<phosg::JSON>> HTTPServer::generate_quest_list_json() {
  co_return co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> shared_ptr<phosg::JSON> {
    return make_shared<phosg::JSON>(this->state->quest_index->json());
  });
}

void HTTPServer::require_GET(const HTTPRequest& req) {
  if (req.method != HTTPRequest::Method::GET) {
    throw HTTPError(405, "GET method required for this endpoint");
  }
}

phosg::JSON HTTPServer::require_POST(const HTTPRequest& req) {
  if (req.method != HTTPRequest::Method::POST) {
    throw HTTPError(405, "POST method required for this endpoint");
  }

  auto* content_type = req.get_header("content-type");
  if (!content_type || (*content_type != "application/json")) {
    throw HTTPError(400, "POST requests must use the application/json content type");
  }

  try {
    return phosg::JSON::parse(req.data);
  } catch (const exception& e) {
    throw HTTPError(400, string("Invalid JSON: ") + e.what());
  }
}

asio::awaitable<std::unique_ptr<HTTPResponse>> HTTPServer::handle_request(shared_ptr<HTTPClient> c, HTTPRequest&& req) {
  shared_ptr<const phosg::JSON> ret;
  uint32_t serialize_options = phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY;
  uint64_t start_time = phosg::now();

  this->log.info_f("{} ...", req.path);

  auto resp = make_unique<HTTPResponse>();
  resp->http_version = req.http_version;
  resp->response_code = 200;
  resp->headers.emplace("Content-Type", "application/json");
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

    if (req.path == "/") {
      this->require_GET(req);
      ret = this->generate_server_version();

    } else if (req.path == "/y/shell-exec") {
      auto json = this->require_POST(req);
      auto command = json.get_string("command");
      try {
        auto dispatch_res = co_await ShellCommand::dispatch_str(this->state, command);
        ret = make_shared<phosg::JSON>(phosg::JSON::dict({{"result", phosg::join(dispatch_res, "\n")}}));
      } catch (const exception& e) {
        throw HTTPError(400, e.what());
      }

    } else if (req.path == "/y/rare-drops/stream") {
      this->require_GET(req);
      if (!(co_await this->enable_websockets(c, req))) {
        throw HTTPError(400, "this path requires a websocket connection");
      } else {
        this->rare_drop_subscribers.emplace(c);
        auto version_message = this->generate_server_version();
        co_await c->send_websocket_message(version_message->serialize());
        co_return nullptr;
      }

    } else if (req.path == "/y/data/ep3-cards") {
      this->require_GET(req);
      ret = co_await this->generate_ep3_cards_json(false);
    } else if (req.path == "/y/data/ep3-cards-trial") {
      this->require_GET(req);
      ret = co_await this->generate_ep3_cards_json(true);
    } else if (req.path == "/y/data/common-tables") {
      this->require_GET(req);
      ret = this->generate_common_table_list_json();
    } else if (req.path.starts_with("/y/data/common-tables/")) {
      this->require_GET(req);
      ret = co_await this->generate_common_table_json(req.path.substr(22));
    } else if (req.path == "/y/data/rare-tables") {
      this->require_GET(req);
      ret = this->generate_rare_table_list_json();
    } else if (req.path.starts_with("/y/data/rare-tables/")) {
      this->require_GET(req);
      ret = co_await this->generate_rare_table_json(req.path.substr(20));
    } else if (req.path == "/y/data/quests") {
      this->require_GET(req);
      ret = co_await this->generate_quest_list_json();
    } else if (req.path == "/y/data/config") {
      this->require_GET(req);
      ret = this->state->config_json;
    } else if (req.path == "/y/accounts") {
      this->require_GET(req);
      ret = this->generate_accounts_json();
    } else if (req.path == "/y/clients") {
      this->require_GET(req);
      ret = this->generate_clients_json();
    } else if (req.path == "/y/lobbies") {
      this->require_GET(req);
      ret = this->generate_lobbies_json();
    } else if (req.path == "/y/server") {
      this->require_GET(req);
      ret = this->generate_server_info_json();
    } else if (req.path == "/y/summary") {
      this->require_GET(req);
      ret = this->generate_summary_json();

    } else {
      throw HTTPError(404, "unknown action");
    }

  } catch (const HTTPError& e) {
    ret = make_shared<phosg::JSON>(phosg::JSON::dict({{"Error", true}, {"Message", e.what()}}));
    resp->response_code = e.code;
  } catch (const exception& e) {
    ret = make_shared<phosg::JSON>(phosg::JSON::dict({{"Error", true}, {"Message", e.what()}}));
    resp->response_code = 500;
  }

  if (!ret) {
    throw logic_error("ret was not set after HTTP handler completed");
  }

  uint64_t handler_end = phosg::now();
  resp->data = co_await call_on_thread_pool(*this->state->thread_pool, [&]() -> string {
    return ret->serialize(serialize_options, 0);
  });
  uint64_t serialize_end = phosg::now();

  string handler_time = phosg::format_duration(handler_end - start_time);
  string serialize_time = phosg::format_duration(serialize_end - handler_end);
  string size_str = phosg::format_size(resp->data.size());
  this->log.info_f("{} in [handler: {}, serialize: {}, size: {}]",
      req.path, handler_time, serialize_time, size_str);

  co_return resp;
}

asio::awaitable<void> HTTPServer::destroy_client(std::shared_ptr<HTTPClient> c) {
  this->rare_drop_subscribers.erase(c);
  co_return;
}
