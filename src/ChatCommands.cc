#include "ChatCommands.hh"

#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#include <string>
#include <unordered_map>
#include <vector>

#include "Client.hh"
#include "Lobby.hh"
#include "Loggers.hh"
#include "ProxyServer.hh"
#include "ReceiveCommands.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Checks

class precondition_failed {
public:
  precondition_failed(const std::u16string& user_msg) : user_msg(user_msg) {}
  ~precondition_failed() = default;

  const std::u16string& what() const {
    return this->user_msg;
  }

private:
  std::u16string user_msg;
};

static void check_license_flags(shared_ptr<Client> c, uint32_t mask) {
  if (!c->license) {
    throw precondition_failed(u"$C6You are not\nlogged in.");
  }
  if ((c->license->flags & mask) != mask) {
    throw precondition_failed(u"$C6You do not have\npermission to\nrun this command.");
  }
}

static void check_version(shared_ptr<Client> c, GameVersion version) {
  if (c->version() != version) {
    throw precondition_failed(u"$C6This command cannot\nbe used for your\nversion of PSO.");
  }
}

static void check_not_version(shared_ptr<Client> c, GameVersion version) {
  if (c->version() == version) {
    throw precondition_failed(u"$C6This command cannot\nbe used for your\nversion of PSO.");
  }
}

static void check_is_game(shared_ptr<Lobby> l, bool is_game) {
  if (l->is_game() != is_game) {
    throw precondition_failed(is_game ? u"$C6This command cannot\nbe used in lobbies." : u"$C6This command cannot\nbe used in games.");
  }
}

static void check_is_ep3(shared_ptr<Client> c, bool is_ep3) {
  if (!!(c->flags & Client::Flag::IS_EPISODE_3) != is_ep3) {
    throw precondition_failed(is_ep3 ? u"$C6This command can only\nbe used in Episode 3." : u"$C6This command cannot\nbe used in Episode 3.");
  }
}

static void check_cheats_enabled(shared_ptr<ServerState> s, shared_ptr<Lobby> l = nullptr) {
  if (s->cheat_mode_behavior == ServerState::CheatModeBehavior::OFF) {
    throw precondition_failed(u"$C6Cheats are disabled.");
  }
  if (l && !(l->flags & Lobby::Flag::CHEATS_ENABLED)) {
    throw precondition_failed(u"$C6This command can\nonly be used in\ncheat mode.");
  }
}

static void check_is_leader(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  if (l->leader_id != c->lobby_client_id) {
    throw precondition_failed(u"$C6This command can\nonly be used by\nthe game leader.");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Message commands

static void server_command_lobby_info(shared_ptr<Client> c, const std::u16string&) {
  vector<string> lines;

  auto l = c->lobby.lock();
  if (!l) {
    lines.emplace_back("$C4No lobby info");

  } else {
    if (l->is_game()) {
      lines.emplace_back(string_printf("Game ID: $C6%08X$C7", l->lobby_id));

      if (!l->is_ep3()) {
        if (l->max_level == 0xFFFFFFFF) {
          lines.emplace_back(string_printf("Levels: $C6%d+$C7", l->min_level + 1));
        } else {
          lines.emplace_back(string_printf("Levels: $C6%d-%d$C7", l->min_level + 1, l->max_level + 1));
        }

        lines.emplace_back(string_printf("$C7Section ID: $C6%s$C7", name_for_section_id(l->section_id).c_str()));
        lines.emplace_back(string_printf("$C7Cheat mode: $C6%s$C7", (l->flags & Lobby::Flag::CHEATS_ENABLED) ? "on" : "off"));

      } else {
        lines.emplace_back(string_printf("$C7State seed: $C6%08X$C7", l->random_seed));
      }

    } else {
      lines.emplace_back(string_printf("$C7Lobby ID: $C6%08X$C7", l->lobby_id));
    }

    string slots_str = "Slots: ";
    for (size_t z = 0; z < l->clients.size(); z++) {
      if (!l->clients[z]) {
        slots_str += string_printf("$C0%zX$C7", z);
      } else {
        bool is_self = l->clients[z] == c;
        bool is_leader = z == l->leader_id;
        if (is_self && is_leader) {
          slots_str += string_printf("$C6%zX$C7", z);
        } else if (is_self) {
          slots_str += string_printf("$C2%zX$C7", z);
        } else if (is_leader) {
          slots_str += string_printf("$C4%zX$C7", z);
        } else {
          slots_str += string_printf("%zX", z);
        }
      }
    }
    lines.emplace_back(std::move(slots_str));
  }

  send_text_message(c, decode_sjis(join(lines, "\n")));
}

static void proxy_command_lobby_info(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  string msg;
  // On non-masked-GC sessions (BB), there is no remote Guild Card number, so we
  // don't show it. (The user can see it in the pause menu, unlike in masked-GC
  // sessions like GC.)
  if (ses->remote_guild_card_number >= 0) {
    msg = string_printf("$C7GC: $C6%" PRId64 "$C7\n", ses->remote_guild_card_number);
  }
  msg += "Slots: ";

  for (size_t z = 0; z < ses->lobby_players.size(); z++) {
    bool is_self = z == ses->lobby_client_id;
    bool is_leader = z == ses->leader_client_id;
    if (ses->lobby_players[z].guild_card_number == 0) {
      msg += string_printf("$C0%zX$C7", z);
    } else if (is_self && is_leader) {
      msg += string_printf("$C6%zX$C7", z);
    } else if (is_self) {
      msg += string_printf("$C2%zX$C7", z);
    } else if (is_leader) {
      msg += string_printf("$C4%zX$C7", z);
    } else {
      msg += string_printf("%zX", z);
    }
  }

  vector<const char*> cheats_tokens;
  if (ses->options.switch_assist) {
    cheats_tokens.emplace_back("SWA");
  }
  if (ses->options.infinite_hp) {
    cheats_tokens.emplace_back("HP");
  }
  if (ses->options.infinite_tp) {
    cheats_tokens.emplace_back("TP");
  }
  if (!cheats_tokens.empty()) {
    msg += "\n$C7Cheats: $C6";
    msg += join(cheats_tokens, ",");
  }

  vector<const char*> behaviors_tokens;
  if (ses->options.save_files) {
    behaviors_tokens.emplace_back("SAVE");
  }
  if (ses->options.suppress_remote_login) {
    behaviors_tokens.emplace_back("SL");
  }
  if (ses->options.function_call_return_value >= 0) {
    behaviors_tokens.emplace_back("BFC");
  }
  if (!behaviors_tokens.empty()) {
    msg += "\n$C7Flags: $C6";
    msg += join(behaviors_tokens, ",");
  }

  if (ses->options.override_section_id >= 0) {
    msg += "\n$C7SecID*: $C6";
    msg += name_for_section_id(ses->options.override_section_id);
  }

  send_text_message(ses->client_channel, decode_sjis(msg));
}

static void server_command_ax(shared_ptr<Client> c, const std::u16string& args) {
  check_license_flags(c, License::Flag::ANNOUNCE);
  string message = encode_sjis(args);
  ax_messages_log.info("%s", message.c_str());
}

static void server_command_announce(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  check_license_flags(c, License::Flag::ANNOUNCE);
  send_text_message(s, args);
}

static void server_command_arrow(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  c->lobby_arrow_color = stoull(encode_sjis(args), nullptr, 0);
  if (!l->is_game()) {
    send_arrow_update(l);
  }
}

static void proxy_command_arrow(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  ses->server_channel.send(0x89, stoull(encode_sjis(args), nullptr, 0));
}

static void server_command_debug(shared_ptr<Client> c, const std::u16string&) {
  check_license_flags(c, License::Flag::DEBUG);
  c->options.debug = !c->options.debug;
  send_text_message_printf(c, "Debug %s",
      c->options.debug ? "enabled" : "disabled");
}

static void server_command_show_material_counts(shared_ptr<Client> c, const std::u16string&) {
  auto p = c->game_data.player();
  send_text_message_printf(c, "%hhu HP, %hhu TP, %hhu POW\n%hhu MIND, %hhu EVADE\n%hhu DEF, %hhu LUCK",
      p->get_material_usage(SavedPlayerDataBB::MaterialType::HP),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::TP),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::POWER),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::MIND),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::EVADE),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::DEF),
      p->get_material_usage(SavedPlayerDataBB::MaterialType::LUCK));
}

static void server_command_auction(shared_ptr<Client> c, const std::u16string&) {
  check_license_flags(c, License::Flag::DEBUG);
  auto l = c->require_lobby();
  if (l->is_game() && l->is_ep3()) {
    G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
    cmd.header.sender_client_id = c->lobby_client_id;
    send_command_t(l, 0xC9, 0x00, cmd);
  }
}

static void proxy_command_auction(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
  cmd.header.sender_client_id = ses->lobby_client_id;
  ses->client_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
  ses->server_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
}

static void server_command_patch(shared_ptr<Client> c, const std::u16string& args) {
  string basename = encode_sjis(args);
  try {
    prepare_client_for_patches(c, [wc = weak_ptr<Client>(c), basename]() {
      auto c = wc.lock();
      if (!c) {
        return;
      }
      auto s = c->require_server_state();
      // Note: We can't look this up outside of the closure because
      // c->specific_version can change during prepare_client_for_patches
      auto fn = s->function_code_index->name_and_specific_version_to_patch_function.at(
          string_printf("%s-%08" PRIX32, basename.c_str(), c->specific_version));
      send_function_call(c, fn);
      c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
    });
  } catch (const out_of_range&) {
    send_text_message(c, u"Invalid patch name");
  }
}

static void empty_patch_return_handler(uint32_t, uint32_t) {}

static void proxy_command_patch(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {

  string basename = encode_sjis(args);
  auto send_call = [basename, ses](uint32_t specific_version, uint32_t) {
    try {
      if (ses->newserv_client_config.cfg.specific_version != specific_version) {
        ses->newserv_client_config.cfg.specific_version = specific_version;
        ses->log.info("Version detected as %08" PRIX32, ses->newserv_client_config.cfg.specific_version);
      }
      auto s = ses->require_server_state();
      auto fn = s->function_code_index->name_and_specific_version_to_patch_function.at(
          string_printf("%s-%08" PRIX32, basename.c_str(), ses->newserv_client_config.cfg.specific_version));
      send_function_call(
          ses->client_channel, ses->newserv_client_config.cfg.flags, fn);
      // Don't forward the patch response to the server
      ses->function_call_return_handler_queue.emplace_back(empty_patch_return_handler);
    } catch (const out_of_range&) {
      send_text_message(ses->client_channel, u"Invalid patch name");
    }
  };

  auto send_version_detect_or_send_call = [basename, ses, send_call]() {
    if (ses->version == GameVersion::GC &&
        ses->newserv_client_config.cfg.specific_version == default_specific_version_for_version(GameVersion::GC, -1)) {
      auto s = ses->require_server_state();
      send_function_call(
          ses->client_channel,
          ses->newserv_client_config.cfg.flags,
          s->function_code_index->name_to_function.at("VersionDetect"));
      ses->function_call_return_handler_queue.emplace_back(send_call);
    } else {
      send_call(ses->newserv_client_config.cfg.specific_version, 0);
    }
  };

  // This mirrors the implementation in prepare_client_for_patches
  if (!(ses->newserv_client_config.cfg.flags & Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH)) {
    auto s = ses->require_server_state();
    send_function_call(
        ses->client_channel, ses->newserv_client_config.cfg.flags, s->function_code_index->name_to_function.at("CacheClearFix-Phase1"), {}, "", 0, 0, 0x7F2734EC);
    ses->function_call_return_handler_queue.emplace_back([s, ses, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
      send_function_call(
          ses->client_channel, ses->newserv_client_config.cfg.flags, s->function_code_index->name_to_function.at("CacheClearFix-Phase2"));
      ses->function_call_return_handler_queue.emplace_back([ses, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
        ses->newserv_client_config.cfg.flags |= Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
        send_version_detect_or_send_call();
      });
    });
  } else {
    send_version_detect_or_send_call();
  }
}

static void server_command_persist(shared_ptr<Client> c, const std::u16string&) {
  check_license_flags(c, License::Flag::DEBUG);
  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::DEFAULT) {
    send_text_message(c, u"$C6Default lobbies\ncannot be marked\ntemporary");
  } else {
    l->flags ^= Lobby::Flag::PERSISTENT;
    send_text_message_printf(c, "Lobby persistence\n%s",
        (l->flags & Lobby::Flag::PERSISTENT) ? "enabled" : "disabled");
  }
}

static void server_command_exit(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  if (l->is_game()) {
    if (c->flags & Client::Flag::IS_EPISODE_3) {
      c->channel.send(0xED, 0x00);
    } else if (l->flags & (Lobby::Flag::QUEST_IN_PROGRESS | Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      c->channel.send(0x60, 0x00, cmd);
      c->area = 0;
    } else {
      send_text_message(c, u"$C6You must return to\nthe lobby first");
    }
  } else {
    send_self_leave_notification(c);
    if (!(c->flags & Client::Flag::NO_D6)) {
      send_message_box(c, u"");
    }

    const auto& port_name = version_to_login_port_name.at(static_cast<size_t>(c->version()));
    auto s = c->require_server_state();
    send_reconnect(c, s->connect_address_for_client(c), s->name_to_port_config.at(port_name)->port);
  }
}

static void proxy_command_exit(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  if (ses->is_in_game) {
    if (ses->newserv_client_config.cfg.flags & Client::Flag::IS_EPISODE_3) {
      ses->client_channel.send(0xED, 0x00);
    } else if (ses->is_in_quest) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      ses->client_channel.send(0x60, 0x00, cmd);
    } else {
      send_text_message(ses->client_channel, u"$C6You must return to\nthe lobby first");
    }
  } else {
    ses->disconnect_action = ProxyServer::LinkedSession::DisconnectAction::CLOSE_IMMEDIATELY;
    ses->send_to_game_server();
  }
}

static void server_command_call(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  if (l->is_game() && l->quest) {
    send_quest_function_call(c, stoul(encode_sjis(args), nullptr, 0));
  } else {
    send_text_message(c, u"$C6You must be in\nquest to use this\ncommand");
  }
}

static void proxy_command_call(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  if (ses->is_in_game && ses->is_in_quest) {
    send_quest_function_call(ses->client_channel, stoul(encode_sjis(args), nullptr, 0));
  } else {
    send_text_message(ses->client_channel, u"$C6You must be in\nquest to use this\ncommand");
  }
}

static void server_command_get_self_card(shared_ptr<Client> c, const std::u16string&) {
  send_guild_card(c, c);
}

static void proxy_command_get_player_card(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& u16args) {
  string args = encode_sjis(u16args);

  bool any_card_sent = false;
  for (const auto& p : ses->lobby_players) {
    if (!p.name.empty() && args == p.name) {
      send_guild_card(ses->client_channel, p.guild_card_number, decode_sjis(p.name), u"", u"", p.section_id, p.char_class);
      any_card_sent = true;
    }
  }

  if (!any_card_sent) {
    try {
      size_t index = stoull(args, nullptr, 0);
      const auto& p = ses->lobby_players.at(index);
      if (!p.name.empty()) {
        send_guild_card(ses->client_channel, p.guild_card_number, decode_sjis(p.name), u"", u"", p.section_id, p.char_class);
      }
    } catch (const exception& e) {
      send_text_message_printf(ses->client_channel, "Error: %s", e.what());
    }
  }
}

static void server_command_send_client(shared_ptr<Client> c, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  c->channel.send(data);
}

static void proxy_command_send_client(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  ses->client_channel.send(data);
}

static void proxy_command_send_server(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  ses->server_channel.send(data);
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

static void server_command_cheat(shared_ptr<Client> c, const std::u16string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  if (s->cheat_mode_behavior == ServerState::CheatModeBehavior::OFF) {
    send_text_message(c, u"$C6Cheat mode cannot\nbe enabled on this\nserver");
    return;
  }

  l->flags ^= Lobby::Flag::CHEATS_ENABLED;
  send_text_message_printf(l, "Cheat mode %s",
      (l->flags & Lobby::Flag::CHEATS_ENABLED) ? "enabled" : "disabled");

  // If cheat mode was disabled, turn off all the cheat features that were on
  if (!(l->flags & Lobby::Flag::CHEATS_ENABLED)) {
    for (size_t x = 0; x < l->max_clients; x++) {
      auto c = l->clients[x];
      if (!c) {
        continue;
      }
      c->options.infinite_hp = false;
      c->options.infinite_tp = false;
      c->options.switch_assist = false;
    }
  }
}

static void server_command_lobby_event(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_license_flags(c, License::Flag::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  l->event = new_event;
  send_change_event(l, l->event);
}

static void proxy_command_lobby_event(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  if (args.empty()) {
    ses->options.override_lobby_event = -1;
  } else {
    uint8_t new_event = event_for_name(args);
    if (new_event == 0xFF) {
      send_text_message(ses->client_channel, u"$C6No such lobby event.");
    } else {
      ses->options.override_lobby_event = new_event;
      // This command is supported on all V3 versions except Ep1&2 Trial
      if ((ses->version == GameVersion::GC && !(ses->newserv_client_config.cfg.flags & Client::Flag::IS_GC_TRIAL_EDITION)) ||
          (ses->version == GameVersion::XB) ||
          (ses->version == GameVersion::BB)) {
        ses->client_channel.send(0xDA, ses->options.override_lobby_event);
      }
    }
  }
}

static void server_command_lobby_event_all(shared_ptr<Client> c, const std::u16string& args) {
  check_license_flags(c, License::Flag::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  auto s = c->require_server_state();
  for (auto l : s->all_lobbies()) {
    if (l->is_game() || !(l->flags & Lobby::Flag::DEFAULT)) {
      continue;
    }

    l->event = new_event;
    send_change_event(l, l->event);
  }
}

static void server_command_lobby_type(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);

  uint8_t new_type = args.empty() ? 0 : lobby_type_for_name(args);
  if (new_type == 0x80) {
    send_text_message(c, u"$C6No such lobby type");
    return;
  }

  uint8_t max_standard_type = ((c->flags & Client::Flag::IS_EPISODE_3) ? 20 : 15);
  c->options.override_lobby_number = (new_type < max_standard_type) ? -1 : new_type;
  send_join_lobby(c, l);
}

static void proxy_command_lobby_type(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  uint8_t new_type = args.empty() ? 0 : lobby_type_for_name(args);
  if (new_type == 0x80) {
    send_text_message(ses->client_channel, u"$C6No such lobby type");
    return;
  }

  uint8_t max_standard_type = ((ses->newserv_client_config.cfg.flags & Client::Flag::IS_EPISODE_3) ? 20 : 15);
  ses->options.override_lobby_number = (new_type < max_standard_type) ? -1 : new_type;
}

static string file_path_for_recording(const std::u16string& args, uint32_t serial_number) {
  string filename = encode_sjis(args);
  for (char ch : filename) {
    if (ch <= 0x20 || ch > 0x7E || ch == '/') {
      throw runtime_error("invalid recording name");
    }
  }
  return string_printf("system/ep3/battle-records/%010" PRIu32 "_%s.mzrd", serial_number, filename.c_str());
}

static void server_command_saverec(shared_ptr<Client> c, const std::u16string& args) {
  if (!c->ep3_prev_battle_record) {
    send_text_message(c, u"$C4No finished\nrecording is\npresent");
    return;
  }
  string file_path = file_path_for_recording(args, c->license->serial_number);
  string data = c->ep3_prev_battle_record->serialize();
  save_file(file_path, data);
  send_text_message(c, u"$C7Recording saved");
  c->ep3_prev_battle_record.reset();
}

static void server_command_playrec(shared_ptr<Client> c, const std::u16string& args) {
  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    send_text_message(c, u"$C4This command can\nonly be used on\nEpisode 3");
    return;
  }

  auto l = c->require_lobby();
  if (l->is_game() && l->battle_player) {
    l->battle_player->start();
  } else if (!l->is_game()) {
    string file_path = file_path_for_recording(args, c->license->serial_number);

    auto s = c->require_server_state();
    uint32_t flags = Lobby::Flag::IS_SPECTATOR_TEAM;
    string filename = encode_sjis(args);
    if (filename[0] == '!') {
      flags |= Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY;
      filename = filename.substr(1);
    }

    string data;
    try {
      data = load_file(file_path);
    } catch (const cannot_open_file&) {
      send_text_message(c, u"$C4The recording does\nnot exist");
      return;
    }
    shared_ptr<Episode3::BattleRecord> record(new Episode3::BattleRecord(data));
    shared_ptr<Episode3::BattleRecordPlayer> battle_player(
        new Episode3::BattleRecordPlayer(record, s->game_server->get_base()));
    auto game = create_game_generic(
        s, c, args, u"", Episode::EP3, GameMode::NORMAL, 0, flags, false, nullptr, battle_player);
    if (game) {
      s->change_client_lobby(c, game);
      c->flags |= Client::Flag::LOADING;
    }
  } else {
    send_text_message(c, u"$C4This command cannot\nbe used in a game");
  }
}

static void server_command_meseta(shared_ptr<Client> c, const std::u16string& args) {
  check_is_ep3(c, true);
  if (!c->options.debug) {
    send_text_message(c, u"$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  uint32_t amount = stoul(encode_sjis(args), nullptr, 0);
  c->license->ep3_current_meseta += amount;
  c->license->ep3_total_meseta_earned += amount;
  c->license->save();
  send_ep3_rank_update(c);
  send_text_message_printf(c, "You now have\n$C6%" PRIu32 "$C7 Meseta", c->license->ep3_current_meseta);
}

////////////////////////////////////////////////////////////////////////////////
// Game commands

static void server_command_secid(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);

  if (!args[0]) {
    c->options.override_section_id = -1;
    send_text_message(c, u"$C6Override section ID\nremoved");
  } else {
    uint8_t new_secid = section_id_for_name(args);
    if (new_secid == 0xFF) {
      send_text_message(c, u"$C6Invalid section ID");
    } else {
      c->options.override_section_id = new_secid;
      send_text_message(c, u"$C6Override section ID\nset");
    }
  }
}

static void proxy_command_secid(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  if (!args[0]) {
    ses->options.override_section_id = -1;
    send_text_message(ses->client_channel, u"$C6Override section ID\nremoved");
  } else {
    uint8_t new_secid = section_id_for_name(args);
    if (new_secid == 0xFF) {
      send_text_message(ses->client_channel, u"$C6Invalid section ID");
    } else {
      ses->options.override_section_id = new_secid;
      send_text_message(ses->client_channel, u"$C6Override section ID\nset");
    }
  }
}

static void server_command_rand(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);

  if (!args[0]) {
    c->options.override_random_seed = -1;
    send_text_message(c, u"$C6Override seed\nremoved");
  } else {
    c->options.override_random_seed = stoul(encode_sjis(args), 0, 16);
    send_text_message(c, u"$C6Override seed\nset");
  }
}

static void proxy_command_rand(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  if (!args[0]) {
    ses->options.override_random_seed = -1;
    send_text_message(ses->client_channel, u"$C6Override seed\nremoved");
  } else {
    ses->options.override_random_seed = stoul(encode_sjis(args), 0, 16);
    send_text_message(ses->client_channel, u"$C6Override seed\nset");
  }
}

static void server_command_password(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  if (!args[0]) {
    l->password[0] = 0;
    send_text_message(l, u"$C6Game unlocked");

  } else {
    l->password = args;
    auto encoded = encode_sjis(l->password);
    send_text_message_printf(l, "$C6Game password:\n%s",
        encoded.c_str());
  }
}

static void server_command_toggle_spectator_flag(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (!l->is_ep3()) {
    throw logic_error("Episode 3 client in non-Episode 3 game");
  }

  // In non-tournament games, only the leader can do this; in a tournament
  // match, the players don't have control over who the leader is, so we allow
  // all players to use this command
  if (!l->tournament_match) {
    check_is_leader(l, c);
  }

  if (l->flags & Lobby::Flag::IS_SPECTATOR_TEAM) {
    send_text_message(c, u"$C6This command cannot\nbe used in a spectator\nteam");
  }

  if (l->flags & Lobby::Flag::SPECTATORS_FORBIDDEN) {
    l->flags &= ~Lobby::Flag::SPECTATORS_FORBIDDEN;
    send_text_message(l, u"$C6Spectators allowed");

  } else {
    l->flags |= Lobby::Flag::SPECTATORS_FORBIDDEN;
    for (auto watcher_l : l->watcher_lobbies) {
      send_command(watcher_l, 0xED, 0x00);
    }
    l->watcher_lobbies.clear();
    send_text_message(l, u"$C6Spectators forbidden");
  }
}

static void server_command_min_level(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  u16string buffer;
  l->min_level = stoull(encode_sjis(args)) - 1;
  send_text_message_printf(l, "$C6Minimum level set to %" PRIu32,
      l->min_level + 1);
}

static void server_command_max_level(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  l->max_level = stoull(encode_sjis(args)) - 1;
  if (l->max_level >= 200) {
    l->max_level = 0xFFFFFFFF;
  }

  if (l->max_level == 0xFFFFFFFF) {
    send_text_message(l, u"$C6Maximum level set to unlimited");
  } else {
    send_text_message_printf(l, "$C6Maximum level set to %" PRIu32, l->max_level + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Character commands

static void server_command_edit(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_version(c, GameVersion::BB);

  string encoded_args = tolower(encode_sjis(args));
  vector<string> tokens = split(encoded_args, ' ');

  try {
    auto p = c->game_data.player();
    if (tokens.at(0) == "atp") {
      p->disp.stats.char_stats.atp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "mst") {
      p->disp.stats.char_stats.mst = stoul(tokens.at(1));
    } else if (tokens.at(0) == "evp") {
      p->disp.stats.char_stats.evp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "hp") {
      p->disp.stats.char_stats.hp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "dfp") {
      p->disp.stats.char_stats.dfp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "ata") {
      p->disp.stats.char_stats.ata = stoul(tokens.at(1));
    } else if (tokens.at(0) == "lck") {
      p->disp.stats.char_stats.lck = stoul(tokens.at(1));
    } else if (tokens.at(0) == "meseta") {
      p->disp.stats.meseta = stoul(tokens.at(1));
    } else if (tokens.at(0) == "exp") {
      p->disp.stats.experience = stoul(tokens.at(1));
    } else if (tokens.at(0) == "level") {
      p->disp.stats.level = stoul(tokens.at(1)) - 1;
    } else if (tokens.at(0) == "namecolor") {
      uint32_t new_color;
      sscanf(tokens.at(1).c_str(), "%8X", &new_color);
      p->disp.visual.name_color = new_color;
    } else if (tokens.at(0) == "secid") {
      uint8_t secid = section_id_for_name(decode_sjis(tokens.at(1)));
      if (secid == 0xFF) {
        send_text_message(c, u"$C6No such section ID");
        return;
      } else {
        p->disp.visual.section_id = secid;
      }
    } else if (tokens.at(0) == "name") {
      p->disp.name = add_language_marker(tokens.at(1), 'J');
    } else if (tokens.at(0) == "npc") {
      if (tokens.at(1) == "none") {
        p->disp.visual.extra_model = 0;
        p->disp.visual.validation_flags &= 0xFD;
      } else {
        uint8_t npc = npc_for_name(decode_sjis(tokens.at(1)));
        if (npc == 0xFF) {
          send_text_message(c, u"$C6No such NPC");
          return;
        }
        p->disp.visual.extra_model = npc;
        p->disp.visual.validation_flags |= 0x02;
      }
    } else if (tokens.at(0) == "tech") {
      uint8_t level = stoul(tokens.at(2)) - 1;
      if (tokens.at(1) == "all") {
        for (size_t x = 0; x < 0x14; x++) {
          p->set_technique_level(x, level);
        }
      } else {
        uint8_t tech_id = technique_for_name(decode_sjis(tokens.at(1)));
        if (tech_id == 0xFF) {
          send_text_message(c, u"$C6No such technique");
          return;
        }
        try {
          p->set_technique_level(tech_id, level);
        } catch (const out_of_range&) {
          send_text_message(c, u"$C6Invalid technique");
          return;
        }
      }
    } else {
      send_text_message(c, u"$C6Unknown field");
      return;
    }
  } catch (const out_of_range&) {
    send_text_message(c, u"$C6Not enough arguments");
    return;
  }

  // Reload the client in the lobby
  send_player_leave_notification(l, c->lobby_client_id);
  send_complete_player_bb(c);
  s->send_lobby_join_notifications(l, c);
}

// TODO: implement this (and make sure the bank name is filesystem-safe)
/* static void server_command_change_bank(shared_ptr<Client> c, const std::u16string&) {
  check_version(c, GameVersion::BB);
  ...
} */

// TODO: This can be implemented on the proxy server too.
static void server_command_convert_char_to_bb(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_not_version(c, GameVersion::BB);

  vector<string> tokens = split(encode_sjis(args), L' ');
  if (tokens.size() != 3) {
    send_text_message(c, u"$C6Incorrect argument count");
    return;
  }

  // username/password are tokens[0] and [1]
  c->pending_bb_save_player_index = stoul(tokens[2]) - 1;
  if (c->pending_bb_save_player_index > 3) {
    send_text_message(c, u"$C6Player index must be 1-4");
    return;
  }

  try {
    s->license_index->verify_bb(tokens[0].c_str(), tokens[1].c_str());
  } catch (const exception& e) {
    send_text_message_printf(c, "$C6Login failed: %s", e.what());
    return;
  }

  c->pending_bb_save_username = tokens[0];

  // Request the player data. The client will respond with a 61, and the handler
  // for that command will execute the conversion
  send_get_player_info(c);
}

////////////////////////////////////////////////////////////////////////////////
// Administration commands

static string name_for_client(shared_ptr<Client> c) {
  auto player = c->game_data.player(false);
  if (player.get()) {
    return encode_sjis(player->disp.name);
  }

  if (c->license.get()) {
    return string_printf("SN:%" PRIu32, c->license->serial_number);
  }

  return "Player";
}

static void server_command_silence(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::SILENCE_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  target->can_chat = !target->can_chat;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s %ssilenced", target_name.c_str(),
      target->can_chat ? "un" : "");
}

static void server_command_kick(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::KICK_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // This should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  send_message_box(target, u"$C6You were kicked off by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s kicked off", target_name.c_str());
}

static void server_command_ban(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::BAN_USER);

  u16string args_str(args);
  size_t space_pos = args_str.find(L' ');
  if (space_pos == string::npos) {
    send_text_message(c, u"$C6Incorrect argument count");
    return;
  }

  u16string identifier = args_str.substr(space_pos + 1);
  auto target = s->find_client(&identifier);
  if (!target->license) {
    // This should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::BAN_USER) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  uint64_t usecs = stoull(encode_sjis(args), nullptr, 0) * 1000000;

  size_t unit_offset = 0;
  for (; isdigit(args[unit_offset]); unit_offset++)
    ;
  if (args[unit_offset] == 'm') {
    usecs *= 60;
  } else if (args[unit_offset] == 'h') {
    usecs *= 60 * 60;
  } else if (args[unit_offset] == 'd') {
    usecs *= 60 * 60 * 24;
  } else if (args[unit_offset] == 'w') {
    usecs *= 60 * 60 * 24 * 7;
  } else if (args[unit_offset] == 'M') {
    usecs *= 60 * 60 * 24 * 30;
  } else if (args[unit_offset] == 'y') {
    usecs *= 60 * 60 * 24 * 365;
  }

  target->license->ban_end_time = now() + usecs;
  target->license->save();
  send_message_box(target, u"$C6You were banned by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s banned", target_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Cheat commands

static void server_command_warp(shared_ptr<Client> c, const std::u16string& args, bool is_warpall) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  uint32_t area = stoul(encode_sjis(args), nullptr, 0);
  if (c->area == area) {
    return;
  }

  size_t limit = area_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  } else if (area > limit) {
    send_text_message_printf(c, "$C6Area numbers must\nbe %zu or less.", limit);
    return;
  }

  if (is_warpall) {
    send_warp(l, area, false);
  } else {
    send_warp(c, area, true);
  }
}

static void server_command_warpme(shared_ptr<Client> c, const std::u16string& args) {
  server_command_warp(c, args, false);
}

static void server_command_warpall(shared_ptr<Client> c, const std::u16string& args) {
  server_command_warp(c, args, true);
}

static void proxy_command_warp(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args, bool is_warpall) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, u"$C6You must be in a\ngame to use this\ncommand");
    return;
  }
  uint32_t area = stoul(encode_sjis(args), nullptr, 0);
  send_warp(ses->client_channel, ses->lobby_client_id, area, !is_warpall);
  if (is_warpall) {
    send_warp(ses->server_channel, ses->lobby_client_id, area, false);
  }
  ses->area = area;
}

static void proxy_command_warpme(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  proxy_command_warp(ses, args, false);
}

static void proxy_command_warpall(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  proxy_command_warp(ses, args, true);
}

static void server_command_next(shared_ptr<Client> c, const std::u16string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  size_t limit = area_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  }
  send_warp(c, (c->area + 1) % limit, true);
}

static void proxy_command_next(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, u"$C6You must be in a\ngame to use this\ncommand");
    return;
  }

  ses->area++;
  send_warp(ses->client_channel, ses->lobby_client_id, ses->area, true);
}

static void server_command_what(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);

  if (!episode_has_arpg_semantics(l->episode)) {
    return;
  }
  if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
    send_text_message(c, u"$C4Item tracking is\nnot available");
  } else {
    float min_dist2 = 0.0f;
    uint32_t nearest_item_id = 0xFFFFFFFF;
    for (const auto& it : l->item_id_to_floor_item) {
      if (it.second.area != c->area) {
        continue;
      }
      float dx = it.second.x - c->x;
      float dz = it.second.z - c->z;
      float dist2 = (dx * dx) + (dz * dz);
      if ((nearest_item_id == 0xFFFFFFFF) || (dist2 < min_dist2)) {
        nearest_item_id = it.first;
        min_dist2 = dist2;
      }
    }

    if (nearest_item_id == 0xFFFFFFFF) {
      send_text_message(c, u"$C4No items are near you");
    } else {
      const auto& item = l->item_id_to_floor_item.at(nearest_item_id);
      string name = item.data.name(true);
      send_text_message(c, decode_sjis(name));
    }
  }
}

static void server_command_song(shared_ptr<Client> c, const std::u16string& args) {
  check_is_ep3(c, true);

  uint32_t song = stoul(encode_sjis(args), nullptr, 0);
  send_ep3_change_music(c->channel, song);
}

static void proxy_command_song(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  int32_t song = stol(encode_sjis(args), nullptr, 0);
  if (song < 0) {
    song = -song;
    send_ep3_change_music(ses->server_channel, song);
  }
  send_ep3_change_music(ses->client_channel, song);
}

static void server_command_infinite_hp(shared_ptr<Client> c, const std::u16string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  c->options.infinite_hp = !c->options.infinite_hp;
  send_text_message_printf(c, "$C6Infinite HP %s",
      c->options.infinite_hp ? "enabled" : "disabled");
}

static void proxy_command_infinite_hp(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  ses->options.infinite_hp = !ses->options.infinite_hp;
  send_text_message_printf(ses->client_channel, "$C6Infinite HP %s",
      ses->options.infinite_hp ? "enabled" : "disabled");
}

static void server_command_infinite_tp(shared_ptr<Client> c, const std::u16string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  c->options.infinite_tp = !c->options.infinite_tp;
  send_text_message_printf(c, "$C6Infinite TP %s",
      c->options.infinite_tp ? "enabled" : "disabled");
}

static void proxy_command_infinite_tp(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  ses->options.infinite_tp = !ses->options.infinite_tp;
  send_text_message_printf(ses->client_channel, "$C6Infinite TP %s",
      ses->options.infinite_tp ? "enabled" : "disabled");
}

static void server_command_switch_assist(shared_ptr<Client> c, const std::u16string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  c->options.switch_assist = !c->options.switch_assist;
  send_text_message_printf(c, "$C6Switch assist %s",
      c->options.switch_assist ? "enabled" : "disabled");
}

static void proxy_command_switch_assist(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string&) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  ses->options.switch_assist = !ses->options.switch_assist;
  send_text_message_printf(ses->client_channel, "$C6Switch assist %s",
      ses->options.switch_assist ? "enabled" : "disabled");
}

static void server_command_drop(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);
  l->flags ^= Lobby::Flag::DROPS_ENABLED;
  send_text_message_printf(l, "Drops %s", (l->flags & Lobby::Flag::DROPS_ENABLED) ? "enabled" : "disabled");
}

static void server_command_raretable(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);
  if (l->base_version == GameVersion::BB) {
    send_text_message_printf(c, "Cannot use client\nrare table on BB");
  } else if (l->item_creator) {
    l->item_creator.reset();
    send_text_message_printf(l, "Game switched to\nclient rare tables");
  } else {
    l->create_item_creator();
    send_text_message_printf(l, "Game switched to\nserver rare tables");
  }
}

static void server_command_item(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(s, l);

  ItemData item(encode_sjis(args));
  item.id = l->generate_item_id(c->lobby_client_id);

  l->add_item(item, c->area, c->x, c->z);
  send_drop_stacked_item(l, item, c->area, c->x, c->z);

  string name = item.name(true);
  send_text_message(c, u"$C7Item created:\n" + decode_sjis(name));
}

static void proxy_command_item(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args) {
  auto s = ses->require_server_state();
  check_cheats_enabled(s);
  if (ses->version == GameVersion::BB) {
    send_text_message(ses->client_channel,
        u"$C6This command cannot\nbe used on the proxy\nserver in BB games");
    return;
  }
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel,
        u"$C6You must be in\na game to use this\ncommand");
    return;
  }
  if (ses->lobby_client_id != ses->leader_client_id) {
    send_text_message(ses->client_channel,
        u"$C6You must be the\nleader to use this\ncommand");
    return;
  }

  bool set_drop = (!args.empty() && (args[0] == u'!'));

  ItemData item(encode_sjis(set_drop ? args.substr(1) : args));
  item.id = random_object<uint32_t>();

  if (set_drop) {
    ses->next_drop_item = item;

    string name = ses->next_drop_item.name(true);
    send_text_message(ses->client_channel, u"$C7Next drop:\n" + decode_sjis(name));

  } else {
    send_drop_stacked_item(ses->client_channel, item, ses->area, ses->x, ses->z);
    send_drop_stacked_item(ses->server_channel, item, ses->area, ses->x, ses->z);

    string name = item.name(true);
    send_text_message(ses->client_channel, u"$C7Item created:\n" + decode_sjis(name));
  }
}

static void server_command_enable_ep3_battle_debug_menu(shared_ptr<Client> c, const std::u16string& args) {
  if (!c->options.debug) {
    send_text_message(c, u"$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }

  if (!args.empty()) {
    l->ep3_server->override_environment_number = stoul(encode_sjis(args), nullptr, 16);
    send_text_message_printf(l, "$C6Override environment\nnumber set to %02hhX", l->ep3_server->override_environment_number);
  } else if (l->ep3_server->override_environment_number == 0xFF) {
    l->ep3_server->override_environment_number = 0x1A;
    send_text_message(l, u"$C6Battle setup debug\nmenu enabled");
  } else {
    l->ep3_server->override_environment_number = 0xFF;
    send_text_message(l, u"$C6Battle setup debug\nmenu disabled");
  }
}

static void server_command_ep3_infinite_time(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_is_leader(l, c);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
    send_text_message(c, u"$C6Battle is already\nin progress");
    return;
  }

  l->ep3_server->options.behavior_flags ^= Episode3::BehaviorFlag::DISABLE_TIME_LIMITS;
  bool infinite_time_enabled = (l->ep3_server->options.behavior_flags & Episode3::BehaviorFlag::DISABLE_TIME_LIMITS);
  send_text_message(l, infinite_time_enabled ? u"$C6Infinite time enabled" : u"$C6Infinite time disabled");
}

static void server_command_ep3_set_def_dice_range(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_is_leader(l, c);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
    send_text_message(c, u"$C6Battle is already\nin progress");
    return;
  }

  if (args.empty()) {
    l->ep3_server->map_and_rules->rules.def_dice_range = 0;
    send_text_message_printf(l, "$C6DEF dice range\nset to default");
  } else {
    uint8_t min_dice, max_dice;
    auto tokens = split(encode_sjis(args), '-');
    if (tokens.size() == 1) {
      min_dice = stoul(tokens[0]);
      max_dice = min_dice;
    } else if (tokens.size() == 2) {
      min_dice = stoul(tokens[0]);
      max_dice = stoul(tokens[1]);
    } else {
      send_text_message(c, u"$C6Specify DEF dice\nrange as MIN-MAX");
      return;
    }
    if (min_dice == 0 || min_dice > 9 || max_dice == 0 || max_dice > 9) {
      send_text_message(c, u"$C6DEF dice must be\nin range 1-9");
      return;
    }
    if (min_dice > max_dice) {
      uint8_t t = min_dice;
      min_dice = max_dice;
      max_dice = t;
    }
    l->ep3_server->map_and_rules->rules.def_dice_range = ((min_dice << 4) & 0xF0) | (max_dice & 0x0F);
    send_text_message_printf(l, "$C6DEF dice range\nset to %hhu-%hhu", min_dice, max_dice);
  }
}

static void server_command_ep3_unset_field_character(shared_ptr<Client> c, const std::u16string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_cheats_enabled(s, l);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, u"$C6Battle has not\nyet begun");
    return;
  }

  size_t index = stoull(encode_sjis(args)) - 1;
  l->ep3_server->force_destroy_field_character(c->lobby_client_id, index);
}

static void server_command_surrender(shared_ptr<Client> c, const std::u16string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, u"$C6Battle has not\nyet started");
    return;
  }
  string name = encode_sjis(c->game_data.player()->disp.name);
  send_text_message_printf(l, "$C6%s has\nsurrendered", name.c_str());
  for (const auto& watcher_l : l->watcher_lobbies) {
    send_text_message_printf(watcher_l, "$C6%s has\nsurrendered", name.c_str());
  }
  l->ep3_server->force_battle_result(c->lobby_client_id, false);
}

static void server_command_get_ep3_battle_stat(shared_ptr<Client> c, const std::u16string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, u"$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, u"$C6Battle has not\nyet started");
    return;
  }
  if (c->lobby_client_id >= 4) {
    throw logic_error("client ID is too large");
  }
  auto ps = l->ep3_server->player_states[c->lobby_client_id];
  if (!ps) {
    send_text_message(c, u"$C6Player is missing");
    return;
  }
  uint8_t team_id = ps->get_team_id();
  if (team_id > 1) {
    throw logic_error("team ID is incorrect");
  }

  string what = encode_sjis(args);
  if (what == "rank") {
    float score = ps->stats.score(l->ep3_server->get_round_num());
    uint8_t rank = ps->stats.rank_for_score(score);
    const char* rank_name = ps->stats.name_for_rank(rank);
    send_text_message_printf(c, "$C7Score: %g\nRank: %hhu (%s)", score, rank, rank_name);
  } else if (what == "duration") {
    string s = format_duration(now() - l->ep3_server->battle_start_usecs);
    send_text_message_printf(c, "$C7Duration: %s", s.c_str());
  } else if (what == "fcs-destroyed") {
    send_text_message_printf(c, "$C7Team FCs destroyed:\n%" PRIu32, l->ep3_server->team_num_ally_fcs_destroyed[team_id]);
  } else if (what == "cards-destroyed") {
    send_text_message_printf(c, "$C7Team cards destroyed:\n%" PRIu32, l->ep3_server->team_num_cards_destroyed[team_id]);
  } else if (what == "damage-given") {
    send_text_message_printf(c, "$C7Damage given: %hu", ps->stats.damage_given.load());
  } else if (what == "damage-taken") {
    send_text_message_printf(c, "$C7Damage taken: %hu", ps->stats.damage_taken.load());
  } else if (what == "opp-cards-destroyed") {
    send_text_message_printf(c, "$C7Opp. cards destroyed:\n%hu", ps->stats.num_opponent_cards_destroyed.load());
  } else if (what == "own-cards-destroyed") {
    send_text_message_printf(c, "$C7Own cards destroyed:\n%hu", ps->stats.num_owned_cards_destroyed.load());
  } else if (what == "move-distance") {
    send_text_message_printf(c, "$C7Move distance: %hu", ps->stats.total_move_distance.load());
  } else if (what == "cards-set") {
    send_text_message_printf(c, "$C7Cards set: %hu", ps->stats.num_cards_set.load());
  } else if (what == "fcs-set") {
    send_text_message_printf(c, "$C7FC cards set: %hu", ps->stats.num_item_or_creature_cards_set.load());
  } else if (what == "attack-actions-set") {
    send_text_message_printf(c, "$C7Attack actions set:\n%hu", ps->stats.num_attack_actions_set.load());
  } else if (what == "techs-set") {
    send_text_message_printf(c, "$C7Techs set: %hu", ps->stats.num_tech_cards_set.load());
  } else if (what == "assists-set") {
    send_text_message_printf(c, "$C7Assists set: %hu", ps->stats.num_assist_cards_set.load());
  } else if (what == "defenses-self") {
    send_text_message_printf(c, "$C7Defenses on self:\n%hu", ps->stats.defense_actions_set_on_self.load());
  } else if (what == "defenses-ally") {
    send_text_message_printf(c, "$C7Defenses on ally:\n%hu", ps->stats.defense_actions_set_on_ally.load());
  } else if (what == "cards-drawn") {
    send_text_message_printf(c, "$C7Cards drawn: %hu", ps->stats.num_cards_drawn.load());
  } else if (what == "max-attack-damage") {
    send_text_message_printf(c, "$C7Maximum attack damage:\n%hu", ps->stats.max_attack_damage.load());
  } else if (what == "max-combo") {
    send_text_message_printf(c, "$C7Longest combo: %hu", ps->stats.max_attack_combo_size.load());
  } else if (what == "attacks-given") {
    send_text_message_printf(c, "$C7Attacks given: %hu", ps->stats.num_attacks_given.load());
  } else if (what == "attacks-taken") {
    send_text_message_printf(c, "$C7Attacks taken: %hu", ps->stats.num_attacks_taken.load());
  } else if (what == "sc-damage") {
    send_text_message_printf(c, "$C7SC damage taken: %hu", ps->stats.sc_damage_taken.load());
  } else if (what == "damage-defended") {
    send_text_message_printf(c, "$C7Damage defended: %hu", ps->stats.action_card_negated_damage.load());
  } else {
    send_text_message(c, u"$C6Unknown statistic");
  }
}

////////////////////////////////////////////////////////////////////////////////

typedef void (*server_handler_t)(shared_ptr<Client> c, const std::u16string& args);
typedef void (*proxy_handler_t)(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& args);
struct ChatCommandDefinition {
  server_handler_t server_handler;
  proxy_handler_t proxy_handler;
};

static const unordered_map<u16string, ChatCommandDefinition> chat_commands({
    {u"$allevent", {server_command_lobby_event_all, nullptr}},
    {u"$ann", {server_command_announce, nullptr}},
    {u"$arrow", {server_command_arrow, proxy_command_arrow}},
    {u"$auction", {server_command_auction, proxy_command_auction}},
    {u"$ax", {server_command_ax, nullptr}},
    {u"$ban", {server_command_ban, nullptr}},
    {u"$bbchar", {server_command_convert_char_to_bb, nullptr}},
    {u"$call", {server_command_call, proxy_command_call}},
    {u"$cheat", {server_command_cheat, nullptr}},
    {u"$debug", {server_command_debug, nullptr}},
    {u"$defrange", {server_command_ep3_set_def_dice_range, nullptr}},
    {u"$drop", {server_command_drop, nullptr}},
    {u"$edit", {server_command_edit, nullptr}},
    {u"$event", {server_command_lobby_event, proxy_command_lobby_event}},
    {u"$exit", {server_command_exit, proxy_command_exit}},
    {u"$gc", {server_command_get_self_card, proxy_command_get_player_card}},
    {u"$infhp", {server_command_infinite_hp, proxy_command_infinite_hp}},
    {u"$inftime", {server_command_ep3_infinite_time, nullptr}},
    {u"$inftp", {server_command_infinite_tp, proxy_command_infinite_tp}},
    {u"$item", {server_command_item, proxy_command_item}},
    {u"$i", {server_command_item, proxy_command_item}},
    {u"$kick", {server_command_kick, nullptr}},
    {u"$li", {server_command_lobby_info, proxy_command_lobby_info}},
    {u"$ln", {server_command_lobby_type, proxy_command_lobby_type}},
    {u"$ep3battledebug", {server_command_enable_ep3_battle_debug_menu, nullptr}},
    {u"$matcount", {server_command_show_material_counts, nullptr}},
    {u"$maxlevel", {server_command_max_level, nullptr}},
    {u"$meseta", {server_command_meseta, nullptr}},
    {u"$minlevel", {server_command_min_level, nullptr}},
    {u"$next", {server_command_next, proxy_command_next}},
    {u"$password", {server_command_password, nullptr}},
    {u"$patch", {server_command_patch, proxy_command_patch}},
    {u"$persist", {server_command_persist, nullptr}},
    {u"$playrec", {server_command_playrec, nullptr}},
    {u"$rand", {server_command_rand, proxy_command_rand}},
    {u"$raretable", {server_command_raretable, nullptr}},
    {u"$saverec", {server_command_saverec, nullptr}},
    {u"$sc", {server_command_send_client, proxy_command_send_client}},
    {u"$secid", {server_command_secid, proxy_command_secid}},
    {u"$silence", {server_command_silence, nullptr}},
    {u"$song", {server_command_song, proxy_command_song}},
    {u"$spec", {server_command_toggle_spectator_flag, nullptr}},
    {u"$ss", {nullptr, proxy_command_send_server}},
    {u"$stat", {server_command_get_ep3_battle_stat, nullptr}},
    {u"$surrender", {server_command_surrender, nullptr}},
    {u"$swa", {server_command_switch_assist, proxy_command_switch_assist}},
    {u"$unset", {server_command_ep3_unset_field_character, nullptr}},
    {u"$warp", {server_command_warpme, proxy_command_warpme}},
    {u"$warpme", {server_command_warpme, proxy_command_warpme}},
    {u"$warpall", {server_command_warpall, proxy_command_warpall}},
    {u"$what", {server_command_what, nullptr}},
});

struct SplitCommand {
  u16string name;
  u16string args;

  SplitCommand(const u16string& text) {
    size_t space_pos = text.find(u' ');
    if (space_pos != string::npos) {
      this->name = text.substr(0, space_pos);
      this->args = text.substr(space_pos + 1);
    } else {
      this->name = text;
    }
  }
};

// This function is called every time any player sends a chat beginning with a
// dollar sign. It is this function's responsibility to see if the chat is a
// command, and to execute the command and block the chat if it is.
void on_chat_command(std::shared_ptr<Client> c, const std::u16string& text) {
  SplitCommand cmd(text);

  const ChatCommandDefinition* def = nullptr;
  try {
    def = &chat_commands.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(c, u"$C6Unknown command");
    return;
  }

  if (!def->server_handler) {
    send_text_message(c, u"$C6Command not available\non game server");
  } else {
    try {
      def->server_handler(c, cmd.args);
    } catch (const precondition_failed& e) {
      send_text_message(c, e.what());
    } catch (const exception& e) {
      send_text_message_printf(c, "$C6Failed:\n%s", e.what());
    }
  }
}

void on_chat_command(shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& text) {
  SplitCommand cmd(text);

  const ChatCommandDefinition* def = nullptr;
  try {
    def = &chat_commands.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(ses->client_channel, u"$C6Unknown command");
    return;
  }

  if (!def->proxy_handler) {
    send_text_message(ses->client_channel, u"$C6Command not available\non proxy server");
  } else {
    try {
      def->proxy_handler(ses, cmd.args);
    } catch (const precondition_failed& e) {
      send_text_message(ses->client_channel, e.what());
    } catch (const exception& e) {
      send_text_message_printf(ses->client_channel, "$C6Failed:\n%s", e.what());
    }
  }
}
