#include "HTTPServer.hh"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <inttypes.h>
#include <stdlib.h>

#include <phosg/Network.hh>
#include <string>
#include <vector>

#include "Loggers.hh"
#include "ProxyServer.hh"
#include "Server.hh"

using namespace std;

const unordered_map<int, const char*> HTTPServer::explanation_for_response_code({
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unathorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I\'m a Teapot"},
    {420, "Enhance Your Calm"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {444, "No Response"},
    {449, "Retry With"},
    {451, "Unavailable For Legal Reasons"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {509, "Bandwidth Limit Exceeded"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"},
    {598, "Network Read Timeout Error"},
    {599, "Network Connect Timeout Error"},
});

HTTPServer::http_error::http_error(int code, const string& what)
    : runtime_error(what),
      code(code) {}

void HTTPServer::send_response(struct evhttp_request* req, int code, const char* content_type, struct evbuffer* b) {
  struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
  evhttp_add_header(headers, "Content-Type", content_type);
  evhttp_add_header(headers, "Server", "newserv");
  evhttp_send_reply(req, code, explanation_for_response_code.at(code), b);
}

void HTTPServer::send_response(struct evhttp_request* req, int code, const char* content_type, const char* fmt, ...) {
  unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
  va_list va;
  va_start(va, fmt);
  evbuffer_add_vprintf(out_buffer.get(), fmt, va);
  va_end(va);
  HTTPServer::send_response(req, code, content_type, out_buffer.get());
}

unordered_multimap<string, string> HTTPServer::parse_url_params(const string& query) {
  unordered_multimap<string, string> params;
  if (query.empty()) {
    return params;
  }
  for (auto it : split(query, '&')) {
    size_t first_equals = it.find('=');
    if (first_equals != string::npos) {
      string value(it, first_equals + 1);

      size_t write_offset = 0, read_offset = 0;
      for (; read_offset < value.size(); write_offset++) {
        if ((value[read_offset] == '%') && (read_offset < value.size() - 2)) {
          value[write_offset] =
              static_cast<char>(value_for_hex_char(value[read_offset + 1]) << 4) |
              static_cast<char>(value_for_hex_char(value[read_offset + 2]));
          read_offset += 3;
        } else if (value[write_offset] == '+') {
          value[write_offset] = ' ';
          read_offset++;
        } else {
          value[write_offset] = value[read_offset];
          read_offset++;
        }
      }
      value.resize(write_offset);

      params.emplace(piecewise_construct, forward_as_tuple(it, 0, first_equals),
          forward_as_tuple(value));
    } else {
      params.emplace(it, "");
    }
  }
  return params;
}

unordered_map<string, string> HTTPServer::parse_url_params_unique(const string& query) {
  unordered_map<string, string> ret;
  for (const auto& it : HTTPServer::parse_url_params(query)) {
    ret.emplace(it.first, std::move(it.second));
  }
  return ret;
}

const string& HTTPServer::get_url_param(
    const unordered_multimap<string, string>& params, const string& key, const string* _default) {

  auto range = params.equal_range(key);
  if (range.first == range.second) {
    if (!_default) {
      throw out_of_range("URL parameter " + key + " not present");
    }
    return *_default;
  }

  return range.first->second;
}

HTTPServer::HTTPServer(shared_ptr<ServerState> state)
    : state(state),
      http(evhttp_new(this->state->base.get()), evhttp_free) {
  evhttp_set_gencb(this->http.get(), this->dispatch_handle_request, this);
}

void HTTPServer::listen(const string& socket_path) {
  int fd = ::listen(socket_path, 0, SOMAXCONN);
  server_log.info("Listening on Unix socket %s on fd %d (HTTP)", socket_path.c_str(), fd);
  this->add_socket(fd);
}

void HTTPServer::listen(const string& addr, int port) {
  if (port == 0) {
    this->listen(addr);
  } else {
    int fd = ::listen(addr, port, SOMAXCONN);
    string netloc_str = render_netloc(addr, port);
    server_log.info("Listening on TCP interface %s on fd %d (HTTP)", netloc_str.c_str(), fd);
    this->add_socket(fd);
  }
}

void HTTPServer::listen(int port) {
  this->listen("", port);
}

void HTTPServer::add_socket(int fd) {
  evhttp_accept_socket(this->http.get(), fd);
}

void HTTPServer::dispatch_handle_request(struct evhttp_request* req, void* ctx) {
  reinterpret_cast<HTTPServer*>(ctx)->handle_request(req);
}

JSON HTTPServer::generate_quest_json(shared_ptr<const Quest> q) const {
  if (!q) {
    return nullptr;
  }
  auto battle_rules_json = q->battle_rules ? q->battle_rules->json() : nullptr;
  auto challenge_template_index_json = (q->challenge_template_index >= 0)
      ? q->challenge_template_index
      : JSON(nullptr);
  return JSON::dict({
      {"Number", q->quest_number},
      {"Episode", name_for_episode(q->episode)},
      {"Joinable", q->joinable},
      {"Name", q->name},
      {"BattleRules", std::move(battle_rules_json)},
      {"ChallengeTemplateIndex", std::move(challenge_template_index_json)},
  });
}

JSON HTTPServer::generate_client_config_json(const Client::Config& config) const {
  const char* drop_notifications_mode = "unknown";
  switch (config.get_drop_notification_mode()) {
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

  auto ret = JSON::dict({
      {"SpecificVersion", config.specific_version},
      {"SwitchAssistEnabled", (config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? true : false)},
      {"InfiniteHPEnabled", (config.check_flag(Client::Flag::INFINITE_HP_ENABLED) ? true : false)},
      {"InfiniteTPEnabled", (config.check_flag(Client::Flag::INFINITE_TP_ENABLED) ? true : false)},
      {"DropNotificationMode", drop_notifications_mode},
      {"DebugEnabled", (config.check_flag(Client::Flag::DEBUG_ENABLED) ? true : false)},
      {"ProxySaveFilesEnabled", (config.check_flag(Client::Flag::PROXY_SAVE_FILES) ? true : false)},
      {"ProxyChatCommandsEnabled", (config.check_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED) ? true : false)},
      {"ProxyPlayerNotificationsEnabled", (config.check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) ? true : false)},
      {"ProxySuppressClientPings", (config.check_flag(Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS) ? true : false)},
      {"ProxyEp3InfiniteMesetaEnabled", (config.check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED) ? true : false)},
      {"ProxyEp3InfiniteTimeEnabled", (config.check_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED) ? true : false)},
      {"ProxyBlockFunctionCalls", (config.check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS) ? true : false)},
      {"ProxyEp3UnmaskWhispers", (config.check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) ? true : false)},
  });
  ret.emplace("OverrideRandomSeed", config.check_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED) ? config.override_random_seed : JSON(nullptr));
  ret.emplace("OverrideSectionID", (config.override_section_id != 0xFF) ? config.override_section_id : JSON(nullptr));
  ret.emplace("OverrideLobbyEvent", (config.override_lobby_event != 0xFF) ? config.override_lobby_event : JSON(nullptr));
  ret.emplace("OverrideLobbyNumber", (config.override_lobby_number != 0x80) ? config.override_lobby_number : JSON(nullptr));
  return ret;
}

JSON HTTPServer::generate_license_json(shared_ptr<const License> l) const {
  auto ret = JSON::dict({
      {"SerialNumber", l->serial_number},
      {"Flags", l->flags},
      {"Ep3CurrentMeseta", l->ep3_current_meseta},
      {"Ep3TotalMesetaEarned", l->ep3_total_meseta_earned},
      {"BBTeamID", l->bb_team_id},
  });
  ret.emplace("BanEndTime", l->ban_end_time ? l->ban_end_time : JSON(nullptr));
  ret.emplace("XBGamertag", !l->xb_gamertag.empty() ? l->xb_gamertag : JSON(nullptr));
  ret.emplace("XBUserID", l->xb_user_id ? l->xb_user_id : JSON(nullptr));
  ret.emplace("BBUsername", !l->bb_username.empty() ? l->bb_username : JSON(nullptr));
  return ret;
};

JSON HTTPServer::generate_game_client_json(shared_ptr<const Client> c) const {
  auto ret = JSON::dict({
      {"ID", c->id},
      {"RemoteAddress", render_sockaddr_storage(c->channel.remote_addr)},
      {"Version", name_for_enum(c->version())},
      {"SubVersion", c->sub_version},
      {"Config", this->generate_client_config_json(c->config)},
      {"Language", name_for_language_code(c->language())},
      {"LocationX", c->x},
      {"LocationZ", c->z},
      {"LocationFloor", c->floor},
      {"CanChat", c->can_chat},
  });
  ret.emplace("license", c->license ? this->generate_license_json(c->license) : JSON(nullptr));
  auto l = c->lobby.lock();
  if (l) {
    ret.emplace("LobbyID", l->lobby_id);
    ret.emplace("LobbyClientID", c->lobby_client_id);
  }
  if (c->version() == Version::BB_V4) {
    ret.emplace("BBCharacterIndex", c->bb_character_index);
  }
  auto p = c->character(false, false);
  if (p) {
    if (!is_ep3(c->version())) {
      ret.emplace("InventoryItems", p->inventory.num_items);
      if (c->version() != Version::DC_NTE) {
        ret.emplace("InventoryLanguage", p->inventory.language);
        ret.emplace("NumHPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::HP));
        ret.emplace("NumTPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
        if (!is_v1_or_v2(c->version())) {
          ret.emplace("NumPowerMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER));
          ret.emplace("NumDefMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF));
          ret.emplace("NumMindMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND));
          ret.emplace("NumEvadeMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE));
          ret.emplace("NumLuckMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
        }
      }
      JSON items_json = JSON::list();
      for (size_t z = 0; z < p->inventory.num_items; z++) {
        const auto& item = p->inventory.items[z];
        string description = this->state->describe_item(c->version(), item.data, false);
        string data_str = item.data.hex();
        auto item_dict = JSON::dict({
            {"Flags", item.flags.load()},
            {"Data", std::move(data_str)},
            {"Description", std::move(description)},
            {"ItemID", item.data.id.load()},
        });
        items_json.emplace_back(std::move(item_dict));
      }
      ret.emplace("ATP", p->disp.stats.char_stats.atp.load());
      ret.emplace("MST", p->disp.stats.char_stats.mst.load());
      ret.emplace("EVP", p->disp.stats.char_stats.evp.load());
      ret.emplace("HP", p->disp.stats.char_stats.hp.load());
      ret.emplace("DFP", p->disp.stats.char_stats.dfp.load());
      ret.emplace("ATA", p->disp.stats.char_stats.ata.load());
      ret.emplace("LCK", p->disp.stats.char_stats.lck.load());
      ret.emplace("EXP", p->disp.stats.experience.load());
      ret.emplace("Meseta", p->disp.stats.meseta.load());
      auto tech_levels_json = JSON::dict();
      for (size_t z = 0; z < 0x13; z++) {
        auto level = p->get_technique_level(z);
        tech_levels_json.emplace(name_for_technique(z), (level != 0xFF) ? level : JSON(nullptr));
      }
      ret.emplace("TechniqueLevels", std::move(tech_levels_json));
    }
    ret.emplace("Height", p->disp.stats.height.load());
    ret.emplace("Level", p->disp.stats.level.load());
    ret.emplace("NameColor", p->disp.visual.name_color.load());
    ret.emplace("ExtraModel", (p->disp.visual.validation_flags & 2) ? p->disp.visual.extra_model : JSON(nullptr));
    ret.emplace("SectionID", name_for_section_id(p->disp.visual.section_id));
    ret.emplace("CharClass", name_for_char_class(p->disp.visual.section_id));
    ret.emplace("Costume", p->disp.visual.costume.load());
    ret.emplace("Skin", p->disp.visual.skin.load());
    ret.emplace("Face", p->disp.visual.face.load());
    ret.emplace("Head", p->disp.visual.head.load());
    ret.emplace("Hair", p->disp.visual.hair.load());
    ret.emplace("HairR", p->disp.visual.hair_r.load());
    ret.emplace("HairG", p->disp.visual.hair_g.load());
    ret.emplace("HairB", p->disp.visual.hair_b.load());
    ret.emplace("ProportionX", p->disp.visual.proportion_x.load());
    ret.emplace("ProportionY", p->disp.visual.proportion_y.load());

    ret.emplace("Name", p->disp.name.decode(c->language()));
    ret.emplace("PlayTimeSeconds", p->play_time_seconds.load());

    ret.emplace("AutoReply", p->auto_reply.decode(c->language()));
    ret.emplace("InfoBoard", p->info_board.decode(c->language()));
    auto battle_place_counts = JSON::list({
        p->battle_records.place_counts[0].load(),
        p->battle_records.place_counts[1].load(),
        p->battle_records.place_counts[2].load(),
        p->battle_records.place_counts[3].load(),
    });
    ret.emplace("BattlePlaceCounts", std::move(battle_place_counts));
    ret.emplace("BattleDisconnectCount", p->battle_records.disconnect_count.load());

    if (!is_ep3(c->version())) {
      auto json_for_challenge_times = []<size_t Count>(const parray<ChallengeTime<false>, Count>& times) -> JSON {
        auto times_json = JSON::list();
        for (size_t z = 0; z < times.size(); z++) {
          times_json.emplace_back(times[z].load());
        }
        return times_json;
      };
      ret.emplace("ChallengeTitleColorXRGB1555", p->challenge_records.title_color.load());
      ret.emplace("ChallengeTimesEp1Online", json_for_challenge_times(p->challenge_records.times_ep1_online));
      ret.emplace("ChallengeTimesEp2Online", json_for_challenge_times(p->challenge_records.times_ep2_online));
      ret.emplace("ChallengeTimesEp1Offline", json_for_challenge_times(p->challenge_records.times_ep1_offline));
      ret.emplace("ChallengeGraveIsEp2", p->challenge_records.grave_is_ep2 ? true : false);
      ret.emplace("ChallengeGraveStageNum", p->challenge_records.grave_stage_num);
      ret.emplace("ChallengeGraveFloor", p->challenge_records.grave_floor);
      ret.emplace("ChallengeGraveDeaths", p->challenge_records.grave_deaths.load());
      {
        uint16_t year = 2000 + ((p->challenge_records.grave_time >> 28) & 0x0F);
        uint8_t month = (p->challenge_records.grave_time >> 24) & 0x0F;
        uint8_t day = (p->challenge_records.grave_time >> 16) & 0xFF;
        uint8_t hour = (p->challenge_records.grave_time >> 8) & 0xFF;
        uint8_t minute = p->challenge_records.grave_time & 0xFF;
        ret.emplace("ChallengeGraveTime", string_printf("%04hu-%02hhu-%02hhu %02hhu:%02hhu:00", year, month, day, hour, minute));
      }
      string grave_enemy_types;
      if (p->challenge_records.grave_defeated_by_enemy_rt_index) {
        for (EnemyType type : enemy_types_for_rare_table_index(p->challenge_records.grave_is_ep2 ? Episode::EP2 : Episode::EP1, p->challenge_records.grave_defeated_by_enemy_rt_index)) {
          if (!grave_enemy_types.empty()) {
            grave_enemy_types += "/";
          }
          grave_enemy_types += name_for_enum(type);
        }
      }
      ret.emplace("ChallengeGraveDefeatedByEnemy", std::move(grave_enemy_types));
      ret.emplace("ChallengeGraveX", p->challenge_records.grave_x.load());
      ret.emplace("ChallengeGraveY", p->challenge_records.grave_y.load());
      ret.emplace("ChallengeGraveZ", p->challenge_records.grave_z.load());
      ret.emplace("ChallengeGraveTeam", p->challenge_records.grave_team.decode());
      ret.emplace("ChallengeGraveMessage", p->challenge_records.grave_message.decode());
      ret.emplace("ChallengeAwardStateEp1OnlineFlags", p->challenge_records.ep1_online_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp1OnlineMaxRank", p->challenge_records.ep1_online_award_state.maximum_rank.load());
      ret.emplace("ChallengeAwardStateEp2OnlineFlags", p->challenge_records.ep2_online_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp2OnlineMaxRank", p->challenge_records.ep2_online_award_state.maximum_rank.load());
      ret.emplace("ChallengeAwardStateEp1OfflineFlags", p->challenge_records.ep1_offline_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp1OfflineMaxRank", p->challenge_records.ep1_offline_award_state.maximum_rank.load());
      ret.emplace("ChallengeRankTitle", p->challenge_records.rank_title.decode());
    }
  }
  return ret;
}

JSON HTTPServer::generate_proxy_client_json(shared_ptr<const ProxyServer::LinkedSession> ses) const {
  struct LobbyPlayer {
    uint32_t guild_card_number = 0;
    uint64_t xb_user_id = 0;
    std::string name;
    uint8_t language = 0;
    uint8_t section_id = 0;
    uint8_t char_class = 0;
  };
  std::vector<LobbyPlayer> lobby_players;

  auto lobby_players_json = JSON::list();
  for (size_t z = 0; z < ses->lobby_players.size(); z++) {
    const auto& p = ses->lobby_players[z];
    if (p.guild_card_number) {
      lobby_players_json.emplace_back(JSON::dict({
          {"GuildCardNumber", p.guild_card_number},
          {"Name", p.name},
          {"Language", name_for_language_code(p.language)},
          {"SectionID", name_for_section_id(p.section_id)},
          {"CharClass", name_for_char_class(p.char_class)},
      }));
      lobby_players_json.back().emplace("XBUserID", p.xb_user_id ? p.xb_user_id : JSON(nullptr));
    } else {
      lobby_players_json.emplace_back(nullptr);
    }
  }

  auto ret = JSON::dict({
      {"ID", ses->id},
      {"RemoteClientAddress", render_sockaddr_storage(ses->client_channel.remote_addr)},
      {"RemoteServerAddress", render_sockaddr_storage(ses->server_channel.remote_addr)},
      {"LocalPort", ses->local_port},
      {"NextDestination", render_sockaddr_storage(ses->next_destination)},
      {"Version", name_for_enum(ses->version())},
      {"SubVersion", ses->sub_version},
      {"Name", ses->character_name},
      {"DCHardwareID", ses->hardware_id},
      {"RemoteGuildCardNumber", ses->remote_guild_card_number},
      {"RemoteClientConfigData", format_data_string(&ses->remote_client_config_data[0], ses->remote_client_config_data.size())},
      {"Config", this->generate_client_config_json(ses->config)},
      {"Language", name_for_language_code(ses->language())},
      {"LobbyClientID", ses->lobby_client_id},
      {"LeaderClientID", ses->leader_client_id},
      {"LocationX", ses->x},
      {"LocationZ", ses->z},
      {"LocationFloor", ses->floor},
      {"IsInGame", ses->is_in_game},
      {"IsInQuest", ses->is_in_quest},
      {"LobbyEvent", ses->lobby_event},
      {"LobbyDifficulty", name_for_difficulty(ses->lobby_difficulty)},
      {"LobbySectionID", name_for_section_id(ses->lobby_section_id)},
      {"LobbyMode", name_for_mode(ses->lobby_mode)},
      {"LobbyEpisode", name_for_episode(ses->lobby_episode)},
      {"LobbyRandomSeed", ses->lobby_random_seed},
      {"LobbyPlayers", std::move(lobby_players_json)},
  });
  switch (ses->drop_mode) {
    case ProxyServer::LinkedSession::DropMode::DISABLED:
      ret.emplace("DropMode", "none");
      break;
    case ProxyServer::LinkedSession::DropMode::PASSTHROUGH:
      ret.emplace("DropMode", "default");
      break;
    case ProxyServer::LinkedSession::DropMode::INTERCEPT:
      ret.emplace("DropMode", "proxy");
      break;
  }
  ret.emplace("License", ses->license ? this->generate_license_json(ses->license) : JSON(nullptr));
  return ret;
}

JSON HTTPServer::generate_lobby_json(shared_ptr<const Lobby> l) const {
  std::array<std::shared_ptr<Client>, 12> clients;

  auto client_ids_json = JSON::list();
  for (size_t z = 0; z < l->max_clients; z++) {
    client_ids_json.emplace_back(l->clients[z] ? l->clients[z]->id : JSON(nullptr));
  }

  auto ret = JSON::dict({
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
    ret.emplace("CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED));
    ret.emplace("MinLevel", l->min_level + 1);
    ret.emplace("MaxLevel", l->max_level + 1);
    ret.emplace("BaseVersion", l->base_version);
    ret.emplace("Episode", name_for_episode(l->episode));
    ret.emplace("HasPassword", !l->password.empty());
    ret.emplace("Name", l->name);
    ret.emplace("RandomSeed", l->random_seed);
    if (l->episode != Episode::EP3) {
      ret.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
      ret.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
      auto variations_json = JSON::list();
      for (size_t z = 0; z < l->variations.size(); z++) {
        variations_json.emplace_back(l->variations[z].load());
      }
      ret.emplace("Variations", std::move(variations_json));
      ret.emplace("SectionID", name_for_section_id(l->section_id));
      ret.emplace("Mode", name_for_mode(l->mode));
      ret.emplace("Difficulty", name_for_difficulty(l->difficulty));
      ret.emplace("BaseEXPMultiplier", l->base_exp_multiplier);
      ret.emplace("AllowedDropModes", l->allowed_drop_modes);
      switch (l->drop_mode) {
        case Lobby::DropMode::DISABLED:
          ret.emplace("DropMode", "none");
          break;
        case Lobby::DropMode::CLIENT:
          ret.emplace("DropMode", "client");
          break;
        case Lobby::DropMode::SERVER_SHARED:
          ret.emplace("DropMode", "shared");
          break;
        case Lobby::DropMode::SERVER_PRIVATE:
          ret.emplace("DropMode", "private");
          break;
        case Lobby::DropMode::SERVER_DUPLICATE:
          ret.emplace("DropMode", "duplicate");
          break;
      }
      if (l->mode == GameMode::CHALLENGE) {
        ret.emplace("ChallengeEXPMultiplier", l->challenge_exp_multiplier);
        if (l->challenge_params) {
          ret.emplace("ChallengeStageNumber", l->challenge_params->stage_number);
          ret.emplace("ChallengeRankColor", l->challenge_params->rank_color);
          ret.emplace("ChallengeRankText", l->challenge_params->rank_text);
          ret.emplace("ChallengeRank0ThresholdBitmask", l->challenge_params->rank_thresholds[0].bitmask);
          ret.emplace("ChallengeRank0ThresholdSeconds", l->challenge_params->rank_thresholds[0].seconds);
          ret.emplace("ChallengeRank1ThresholdBitmask", l->challenge_params->rank_thresholds[1].bitmask);
          ret.emplace("ChallengeRank1ThresholdSeconds", l->challenge_params->rank_thresholds[1].seconds);
          ret.emplace("ChallengeRank2ThresholdBitmask", l->challenge_params->rank_thresholds[2].bitmask);
          ret.emplace("ChallengeRank2ThresholdSeconds", l->challenge_params->rank_thresholds[2].seconds);
        }
      }

      auto floor_items_json = JSON::list();
      for (size_t floor = 0; floor < l->floor_item_managers.size(); floor++) {
        for (const auto& it : l->floor_item_managers[floor].items) {
          const auto& item = it.second;
          string description = this->state->describe_item(l->base_version, item->data, false);
          string data_str = item->data.hex();
          auto item_dict = JSON::dict({
              {"LocationFloor", floor},
              {"LocationX", item->x},
              {"LocationZ", item->z},
              {"DropNumber", item->drop_number},
              {"VisibilityFlags", item->visibility_flags},
              {"Data", std::move(data_str)},
              {"Description", std::move(description)},
              {"ItemID", item->data.id.load()},
          });
          floor_items_json.emplace_back(std::move(item_dict));
        }
      }
      ret.emplace("FloorItems", std::move(floor_items_json));
      ret.emplace("Quest", this->generate_quest_json(l->quest));

    } else {
      ret.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
      ret.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
      ret.emplace("SpectatorsForbidden", l->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN));

      auto ep3s = l->ep3_server;
      if (ep3s) {
        auto players_json = JSON::list();
        for (size_t z = 0; z < 4; z++) {
          if (!ep3s->name_entries[z].present) {
            players_json.emplace_back(nullptr);
          } else {
            auto lc = l->clients[z];

            auto deck_entry = ep3s->deck_entries[z];
            JSON deck_json = nullptr;
            if (deck_entry) {
              auto cards_json = JSON::list();
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
              deck_json = JSON::dict({
                  {"Name", deck_entry->name.decode(lc ? lc->language() : 1)},
                  {"TeamID", deck_entry->team_id.load()},
                  {"Cards", std::move(cards_json)},
                  {"GodWhimFlag", deck_entry->god_whim_flag},
                  {"PlayerLevel", deck_entry->player_level.load()},
              });
            }

            auto player_json = JSON::dict({
                {"PlayerName", ep3s->name_entries[z].name.decode(lc ? lc->language() : 1)},
                {"ClientID", ep3s->name_entries[z].client_id},
                {"IsCOM", !!ep3s->name_entries[z].is_cpu_player},
                {"Deck", std::move(deck_json)},
            });
            players_json.emplace_back(std::move(player_json));
          }
        }
        auto battle_state_json = JSON::dict({
            {"BehaviorFlags", ep3s->options.behavior_flags},
            {"RandomSeed", ep3s->options.random_crypt ? ep3s->options.random_crypt->seed() : JSON(nullptr)},
            {"RandomOffset", ep3s->options.random_crypt ? ep3s->options.random_crypt->absolute_offset() : JSON(nullptr)},
            {"Tournament", ep3s->options.tournament ? ep3s->options.tournament->json() : nullptr},
            {"MapNumber", ep3s->last_chosen_map ? ep3s->last_chosen_map->map_number : JSON(nullptr)},
            {"EnvironmentNumber", ep3s->map_and_rules ? ep3s->map_and_rules->environment_number : JSON(nullptr)},
            {"Rules", ep3s->map_and_rules ? ep3s->map_and_rules->rules.json() : nullptr},
            {"Players", std::move(players_json)},
            {"IsBattleFinished", ep3s->battle_finished},
            {"IsBattleInprogress", ep3s->battle_in_progress},
            {"RoundNumber", ep3s->round_num},
            {"FirstTeamTurn", ep3s->first_team_turn},
            {"CurrentTeamTurn", ep3s->current_team_turn1},
            {"BattlePhase", name_for_enum(ep3s->battle_phase)},
            {"SetupPhase", ep3s->setup_phase},
            {"RegistrationPhase", ep3s->registration_phase},
            {"ActionSubphase", ep3s->action_subphase},
            {"BattleStartTimeUsecs", ep3s->battle_start_usecs},
            {"TeamEXP", JSON::list({ep3s->team_exp[0], ep3s->team_exp[1]})},
            {"TeamDiceBonus", JSON::list({ep3s->team_dice_bonus[0], ep3s->team_dice_bonus[1]})},
        });
        // std::shared_ptr<StateFlags> state_flags;
        // std::array<std::shared_ptr<PlayerState>, 4> player_states;
        ret.emplace("Episode3BattleState", std::move(battle_state_json));
      } else {
        ret.emplace("Episode3BattleState", nullptr);
      }
      auto watched_lobby = l->watched_lobby.lock();
      if (watched_lobby) {
        ret.emplace("WatchedLobbyID", watched_lobby->lobby_id);
      }
      auto watcher_lobby_ids_json = JSON::list();
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        watcher_lobby_ids_json.emplace_back(watcher_lobby->lobby_id);
      }
      ret.emplace("WatcherLobbyIDs", std::move(watcher_lobby_ids_json));
      ret.emplace("IsReplayLobby", !!l->battle_player);
    }

  } else { // Not game
    ret.emplace("IsPublic", l->check_flag(Lobby::Flag::PUBLIC));
    ret.emplace("IsDefault", l->check_flag(Lobby::Flag::DEFAULT));
    ret.emplace("IsOverflow", l->check_flag(Lobby::Flag::IS_OVERFLOW));
    ret.emplace("Block", l->block);
  }
  return ret;
}

JSON HTTPServer::generate_game_server_clients_json() const {
  JSON res = JSON::list();
  for (const auto& it : this->state->channel_to_client) {
    res.emplace_back(this->generate_game_client_json(it.second));
  }
  return res;
}

JSON HTTPServer::generate_proxy_server_clients_json() const {
  JSON res = JSON::list();
  for (const auto& it : this->state->proxy_server->all_sessions()) {
    res.emplace_back(this->generate_proxy_client_json(it.second));
  }
  return res;
}

JSON HTTPServer::generate_server_info_json() const {
  size_t game_count = 0;
  size_t lobby_count = 0;
  for (const auto& it : this->state->id_to_lobby) {
    if (it.second->is_game()) {
      game_count++;
    } else {
      lobby_count++;
    }
  }
  uint64_t uptime_usecs = now() - this->state->creation_time;
  return JSON::dict({
      {"StartTimeUsecs", this->state->creation_time},
      {"StartTime", format_time(this->state->creation_time)},
      {"UptimeUsecs", uptime_usecs},
      {"Uptime", format_duration(uptime_usecs)},
      {"LobbyCount", lobby_count},
      {"GameCount", game_count},
      {"ClientCount", this->state->channel_to_client.size()},
      {"ProxySessionCount", this->state->proxy_server->num_sessions()},
      {"ServerName", this->state->name},
  });
}

JSON HTTPServer::generate_lobbies_json() const {
  JSON res = JSON::list();
  for (const auto& it : this->state->id_to_lobby) {
    res.emplace_back(this->generate_lobby_json(it.second));
  }
  return res;
}

JSON HTTPServer::generate_summary_json() const {
  auto clients_json = JSON::list();
  for (const auto& it : this->state->channel_to_client) {
    auto c = it.second;
    auto p = c->character(false, false);
    auto l = c->lobby.lock();
    clients_json.emplace_back(JSON::dict({
        {"ID", c->id},
        {"SerialNumber", c->license ? c->license->serial_number : JSON(nullptr)},
        {"Name", p ? p->disp.name.decode(it.second->language()) : JSON(nullptr)},
        {"Version", name_for_enum(it.second->version())},
        {"Language", name_for_language_code(it.second->language())},
        {"Level", p ? p->disp.stats.level + 1 : JSON(nullptr)},
        {"Class", p ? name_for_char_class(p->disp.visual.char_class) : JSON(nullptr)},
        {"SectionID", p ? name_for_section_id(p->disp.visual.section_id) : JSON(nullptr)},
        {"LobbyID", l ? l->lobby_id : JSON(nullptr)},
    }));
  }

  auto proxy_clients_json = JSON::list();
  for (const auto& it : this->state->proxy_server->all_sessions()) {
    proxy_clients_json.emplace_back(JSON::dict({
        {"SerialNumber", it.second->license ? it.second->license->serial_number : JSON(nullptr)},
        {"Name", it.second->character_name},
        {"Version", name_for_enum(it.second->version())},
        {"Language", name_for_language_code(it.second->language())},
    }));
  }

  auto games_json = JSON::list();
  for (const auto& it : this->state->id_to_lobby) {
    auto l = it.second;
    if (l->is_game()) {
      auto game_json = JSON::dict({
          {"ID", l->lobby_id},
          {"Name", l->name},
          {"BaseVersion", name_for_enum(l->base_version)},
          {"Players", l->count_clients()},
          {"CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED)},
          {"Episode", name_for_episode(l->episode)},
          {"HasPassword", !l->password.empty()},
      });
      if (l->episode == Episode::EP3) {
        auto ep3s = l->ep3_server;
        game_json.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
        game_json.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
        game_json.emplace("MapNumber", (ep3s && ep3s->last_chosen_map) ? ep3s->last_chosen_map->map_number : JSON(nullptr));
        game_json.emplace("Rules", (ep3s && ep3s->map_and_rules) ? ep3s->map_and_rules->rules.json() : nullptr);
      } else {
        game_json.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
        game_json.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
        game_json.emplace("SectionID", name_for_section_id(l->section_id));
        game_json.emplace("Mode", name_for_mode(l->mode));
        game_json.emplace("Difficulty", name_for_difficulty(l->difficulty));
        game_json.emplace("Quest", this->generate_quest_json(l->quest));
      }
      games_json.emplace_back(std::move(game_json));
    }
  }

  return JSON::dict({
      {"Clients", std::move(clients_json)},
      {"ProxyClients", std::move(proxy_clients_json)},
      {"Games", std::move(games_json)},
      {"Server", this->generate_server_info_json()},
  });
}

JSON HTTPServer::generate_all_json() const {
  return JSON::dict({
      {"Clients", this->generate_game_server_clients_json()},
      {"ProxyClients", this->generate_proxy_server_clients_json()},
      {"Lobbies", this->generate_lobbies_json()},
      {"Server", this->generate_server_info_json()},
  });
}

void HTTPServer::handle_request(struct evhttp_request* req) {
  JSON ret;
  uint32_t serialize_options = 0;
  try {
    string uri = evhttp_request_get_uri(req);

    std::unordered_multimap<std::string, std::string> query;
    size_t query_pos = uri.find('?');
    if (query_pos != string::npos) {
      query = this->parse_url_params(uri.substr(query_pos + 1));
      uri.resize(query_pos);
    }

    static const string default_format_option = "false";
    if (this->get_url_param(query, "format", &default_format_option) == "true") {
      serialize_options = JSON::SerializeOption::FORMAT | JSON::SerializeOption::SORT_DICT_KEYS;
    }

    if (uri == "/") {
      auto endpoints_json = JSON::list({
          "/y/clients",
          "/y/proxy-clients",
          "/y/lobbies",
          "/y/summary",
          "/y/all",
      });
      ret = JSON::dict({{"endpoints", std::move(endpoints_json)}});

    } else if (uri == "/y/clients") {
      ret = this->generate_game_server_clients_json();
    } else if (uri == "/y/proxy-clients") {
      ret = this->generate_proxy_server_clients_json();
    } else if (uri == "/y/lobbies") {
      ret = this->generate_lobbies_json();
    } else if (uri == "/y/server") {
      ret = this->generate_server_info_json();
    } else if (uri == "/y/summary") {
      ret = this->generate_summary_json();
    } else if (uri == "/y/all") {
      ret = this->generate_all_json();

    } else {
      throw http_error(404, "unknown action");
    }

  } catch (const http_error& e) {
    unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
    evbuffer_add_printf(out_buffer.get(), "%s", e.what());
    this->send_response(req, e.code, "text/plain", out_buffer.get());
    return;

  } catch (const exception& e) {
    unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
    evbuffer_add_printf(out_buffer.get(), "Error during request: %s", e.what());
    this->send_response(req, 500, "text/plain", out_buffer.get());
    server_log.warning("internal server error during http request: %s", e.what());
    return;
  }

  unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
  string* serialized = new string(ret.serialize(JSON::SerializeOption::ESCAPE_CONTROLS_ONLY | serialize_options));
  auto cleanup = +[](const void*, size_t, void* s) -> void {
    delete reinterpret_cast<string*>(s);
  };
  evbuffer_add_reference(out_buffer.get(), serialized->data(), serialized->size(), cleanup, serialized);
  this->send_response(req, 200, "application/json", out_buffer.get());
}
