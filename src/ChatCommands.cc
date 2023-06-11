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

static void check_privileges(shared_ptr<Client> c, uint64_t mask) {
  if (!c->license) {
    throw precondition_failed(u"$C6You are not\nlogged in.");
  }
  if ((c->license->privileges & mask) != mask) {
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

static void check_is_leader(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  if (l->leader_id != c->lobby_client_id) {
    throw precondition_failed(u"$C6This command can\nonly be used by\nthe game leader.");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Message commands

static void server_command_lobby_info(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  vector<string> lines;

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

static void proxy_command_lobby_info(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  string msg;
  // On non-masked-GC sessions (BB), there is no remote Guild Card number, so we
  // don't show it. (The user can see it in the pause menu, unlike in masked-GC
  // sessions like GC.)
  if (session.remote_guild_card_number >= 0) {
    msg = string_printf("$C7GC: $C6%" PRId64 "$C7\n",
        session.remote_guild_card_number);
  }
  msg += "Slots: ";

  for (size_t z = 0; z < session.lobby_players.size(); z++) {
    bool is_self = z == session.lobby_client_id;
    bool is_leader = z == session.leader_client_id;
    if (session.lobby_players[z].guild_card_number == 0) {
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
  if (session.options.switch_assist) {
    cheats_tokens.emplace_back("SWA");
  }
  if (session.options.infinite_hp) {
    cheats_tokens.emplace_back("HP");
  }
  if (session.options.infinite_tp) {
    cheats_tokens.emplace_back("TP");
  }
  if (!cheats_tokens.empty()) {
    msg += "\n$C7Cheats: $C6";
    msg += join(cheats_tokens, ",");
  }

  vector<const char*> behaviors_tokens;
  if (session.options.save_files) {
    behaviors_tokens.emplace_back("SAVE");
  }
  if (session.options.suppress_remote_login) {
    behaviors_tokens.emplace_back("SL");
  }
  if (session.options.function_call_return_value >= 0) {
    behaviors_tokens.emplace_back("BFC");
  }
  if (!behaviors_tokens.empty()) {
    msg += "\n$C7Flags: $C6";
    msg += join(behaviors_tokens, ",");
  }

  if (session.options.override_section_id >= 0) {
    msg += "\n$C7SecID*: $C6";
    msg += name_for_section_id(session.options.override_section_id);
  }

  send_text_message(session.client_channel, decode_sjis(msg));
}

static void server_command_ax(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::ANNOUNCE);
  string message = encode_sjis(args);
  ax_messages_log.info("%s", message.c_str());
}

static void server_command_announce(shared_ptr<ServerState> s, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::ANNOUNCE);
  send_text_message(s, args);
}

static void server_command_arrow(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  // no preconditions
  c->lobby_arrow_color = stoull(encode_sjis(args), nullptr, 0);
  if (!l->is_game()) {
    send_arrow_update(l);
  }
}

static void proxy_command_arrow(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  session.server_channel.send(0x89, stoull(encode_sjis(args), nullptr, 0));
}

static void server_command_debug(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string&) {
  check_privileges(c, Privilege::DEBUG);
  c->options.debug = !c->options.debug;
  send_text_message_printf(c, "Debug %s",
      c->options.debug ? "enabled" : "disabled");
}

static void server_command_auction(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_privileges(c, Privilege::DEBUG);
  if (l->is_game() && l->is_ep3()) {
    G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
    cmd.header.sender_client_id = c->lobby_client_id;
    send_command_t(l, 0xC9, 0x00, cmd);
  }
}

static void proxy_command_auction(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
  cmd.header.sender_client_id = session.lobby_client_id;
  session.client_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
  session.server_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
}

static void server_command_patch(shared_ptr<ServerState> s, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  string basename = encode_sjis(args);
  try {
    prepare_client_for_patches(s, c, [s, wc = weak_ptr<Client>(c), basename]() {
      auto c = wc.lock();
      if (!c) {
        return;
      }
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

static void proxy_command_patch(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, const std::u16string& args) {

  string basename = encode_sjis(args);
  auto send_call = [s, basename, &session](uint32_t specific_version, uint32_t) {
    try {
      if (session.newserv_client_config.cfg.specific_version != specific_version) {
        session.newserv_client_config.cfg.specific_version = specific_version;
        session.log.info("Version detected as %08" PRIX32, session.newserv_client_config.cfg.specific_version);
      }
      auto fn = s->function_code_index->name_and_specific_version_to_patch_function.at(
          string_printf("%s-%08" PRIX32, basename.c_str(), session.newserv_client_config.cfg.specific_version));
      send_function_call(
          session.client_channel, session.newserv_client_config.cfg.flags, fn);
      // Don't forward the patch response to the server
      session.function_call_return_handler_queue.emplace_back(empty_patch_return_handler);
    } catch (const out_of_range&) {
      send_text_message(session.client_channel, u"Invalid patch name");
    }
  };

  auto send_version_detect_or_send_call = [s, basename, &session, send_call]() {
    if (session.version == GameVersion::GC &&
        session.newserv_client_config.cfg.specific_version == default_specific_version_for_version(GameVersion::GC, -1)) {
      send_function_call(
          session.client_channel,
          session.newserv_client_config.cfg.flags,
          s->function_code_index->name_to_function.at("VersionDetect"));
      session.function_call_return_handler_queue.emplace_back(send_call);
    } else {
      send_call(session.newserv_client_config.cfg.specific_version, 0);
    }
  };

  // This mirrors the implementation in prepare_client_for_patches
  if (!(session.newserv_client_config.cfg.flags & Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH)) {
    send_function_call(
        session.client_channel, session.newserv_client_config.cfg.flags, s->function_code_index->name_to_function.at("CacheClearFix-Phase1"), {}, "", 0, 0, 0x7F2734EC);
    session.function_call_return_handler_queue.emplace_back([s, session_p = &session, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
      send_function_call(
          session_p->client_channel, session_p->newserv_client_config.cfg.flags, s->function_code_index->name_to_function.at("CacheClearFix-Phase2"));
      session_p->function_call_return_handler_queue.emplace_back([session_p, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
        session_p->newserv_client_config.cfg.flags |= Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
        send_version_detect_or_send_call();
      });
    });
  } else {
    send_version_detect_or_send_call();
  }
}

static void server_command_persist(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_privileges(c, Privilege::DEBUG);
  if (l->flags & Lobby::Flag::DEFAULT) {
    send_text_message(c, u"$C6Default lobbies\ncannot be marked\ntemporary");
  } else {
    l->flags ^= Lobby::Flag::PERSISTENT;
    send_text_message_printf(c, "Lobby persistence\n%s",
        (l->flags & Lobby::Flag::PERSISTENT) ? "enabled" : "disabled");
  }
}

static void server_command_exit(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  if (l->is_game()) {
    if (c->flags & Client::Flag::IS_EPISODE_3) {
      c->channel.send(0xED, 0x00);
    } else if (l->flags & (Lobby::Flag::QUEST_IN_PROGRESS | Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      c->channel.send(0x60, 0x00, cmd);
    } else {
      send_text_message(c, u"$C6You must return to\nthe lobby first");
    }
  } else {
    send_self_leave_notification(c);
    if (!(c->flags & Client::Flag::NO_D6)) {
      send_message_box(c, u"");
    }

    const auto& port_name = version_to_login_port_name.at(
        static_cast<size_t>(c->version()));
    send_reconnect(c, s->connect_address_for_client(c),
        s->name_to_port_config.at(port_name)->port);
  }
}

static void proxy_command_exit(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  if (session.is_in_game) {
    if (session.newserv_client_config.cfg.flags & Client::Flag::IS_EPISODE_3) {
      session.client_channel.send(0xED, 0x00);
    } else if (session.is_in_quest) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      session.client_channel.send(0x60, 0x00, cmd);
    } else {
      send_text_message(session.client_channel, u"$C6You must return to\nthe lobby first");
    }
  } else {
    session.disconnect_action = ProxyServer::LinkedSession::DisconnectAction::CLOSE_IMMEDIATELY;
    session.send_to_game_server();
  }
}

static void server_command_get_self_card(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string&) {
  send_guild_card(c, c);
}

static void proxy_command_get_player_card(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& u16args) {
  string args = encode_sjis(u16args);

  bool any_card_sent = false;
  for (const auto& p : session.lobby_players) {
    if (!p.name.empty() && args == p.name) {
      send_guild_card(session.client_channel, p.guild_card_number, decode_sjis(p.name), u"", u"", p.section_id, p.char_class);
      any_card_sent = true;
    }
  }

  if (!any_card_sent) {
    try {
      size_t index = stoull(args, nullptr, 0);
      const auto& p = session.lobby_players.at(index);
      if (!p.name.empty()) {
        send_guild_card(session.client_channel, p.guild_card_number, decode_sjis(p.name), u"", u"", p.section_id, p.char_class);
      }
    } catch (const exception& e) {
      send_text_message_printf(session.client_channel, "Error: %s", e.what());
    }
  }
}

static void server_command_send_client(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  c->channel.send(data);
}

static void proxy_command_send_client(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  session.client_channel.send(data);
}

static void proxy_command_send_server(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  string data = parse_data_string(encode_sjis(args));
  data.resize((data.size() + 3) & (~3));
  session.server_channel.send(data);
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

static void server_command_cheat(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);
  check_is_leader(l, c);

  l->flags ^= Lobby::Flag::CHEATS_ENABLED;
  send_text_message_printf(l, "Cheat mode %s",
      (l->flags & Lobby::Flag::CHEATS_ENABLED) ? "enabled" : "disabled");

  // if cheat mode was disabled, turn off all the cheat features that were on
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

static void server_command_lobby_event(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  l->event = new_event;
  send_change_event(l, l->event);
}

static void proxy_command_lobby_event(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  if (args.empty()) {
    session.options.override_lobby_event = -1;
  } else {
    uint8_t new_event = event_for_name(args);
    if (new_event == 0xFF) {
      send_text_message(session.client_channel, u"$C6No such lobby event.");
    } else {
      session.options.override_lobby_event = new_event;
      if ((session.version == GameVersion::GC && !(session.newserv_client_config.cfg.flags & Client::Flag::IS_TRIAL_EDITION)) ||
          (session.version == GameVersion::XB) ||
          (session.version == GameVersion::BB)) {
        session.client_channel.send(0xDA, session.options.override_lobby_event);
      }
    }
  }
}

static void server_command_lobby_event_all(shared_ptr<ServerState> s, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  for (auto l : s->all_lobbies()) {
    if (l->is_game() || !(l->flags & Lobby::Flag::DEFAULT)) {
      continue;
    }

    l->event = new_event;
    send_change_event(l, l->event);
  }
}

static void server_command_lobby_type(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_type = lobby_type_for_name(args);
  if (new_type == 0x80) {
    send_text_message(c, u"$C6No such lobby type.");
    return;
  }

  l->type = new_type;
  if (l->type < (l->is_ep3() ? 20 : 15)) {
    l->type = l->block - 1;
  }

  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      send_join_lobby(l->clients[x], l);
    }
  }
}

static void server_command_saverec(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  if (args.find(u'/') != string::npos) {
    send_text_message(c, u"$C4Recording names\ncannot include\nthe / character");
    return;
  }
  if (!l->prev_battle_record) {
    send_text_message(c, u"$C4No finished\nrecording is\npresent");
    return;
  }
  string filename = "system/ep3/battle-records/" + encode_sjis(args) + ".mzrd";
  string data = l->prev_battle_record->serialize();
  save_file(filename, data);
  send_text_message(l, u"$C7Recording saved");
  l->prev_battle_record.reset();
}

static void server_command_playrec(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    send_text_message(c, u"$C4This command can\nonly be used on\nEpisode 3");
    return;
  }
  if (args.find(u'/') != string::npos) {
    send_text_message(c, u"$C4Recording names\ncannot include\nthe / character");
    return;
  }

  if (l->battle_player) {
    l->battle_player->start();
  } else {
    uint32_t flags = Lobby::Flag::NON_V1_ONLY | Lobby::Flag::IS_SPECTATOR_TEAM;
    string filename = encode_sjis(args);
    if (filename[0] == '!') {
      flags |= Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY;
      filename = filename.substr(1);
    }
    string data = load_file("system/ep3/battle-records/" + filename + ".mzrd");
    shared_ptr<Episode3::BattleRecord> record(new Episode3::BattleRecord(data));
    shared_ptr<Episode3::BattleRecordPlayer> battle_player(
        new Episode3::BattleRecordPlayer(record, s->game_server->get_base()));
    create_game_generic(s, c, args.c_str(), u"", Episode::EP3, GameMode::NORMAL,
        0, flags, nullptr, battle_player);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Game commands

static void server_command_secid(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
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

static void proxy_command_secid(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  if (!args[0]) {
    session.options.override_section_id = -1;
    send_text_message(session.client_channel, u"$C6Override section ID\nremoved");
  } else {
    uint8_t new_secid = section_id_for_name(args);
    if (new_secid == 0xFF) {
      send_text_message(session.client_channel, u"$C6Invalid section ID");
    } else {
      session.options.override_section_id = new_secid;
      send_text_message(session.client_channel, u"$C6Override section ID\nset");
    }
  }
}

static void server_command_rand(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, false);

  if (!args[0]) {
    c->options.override_random_seed = -1;
    send_text_message(c, u"$C6Override seed\nremoved");
  } else {
    c->options.override_random_seed = stoul(encode_sjis(args), 0, 16);
    send_text_message(c, u"$C6Override seed\nset");
  }
}

static void proxy_command_rand(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  if (!args[0]) {
    session.options.override_random_seed = -1;
    send_text_message(session.client_channel, u"$C6Override seed\nremoved");
  } else {
    session.options.override_random_seed = stoul(encode_sjis(args), 0, 16);
    send_text_message(session.client_channel, u"$C6Override seed\nset");
  }
}

static void server_command_password(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
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

static void server_command_spec(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);
  check_is_leader(l, c);
  check_is_ep3(c, true);
  if (!l->is_ep3()) {
    throw logic_error("Episode 3 client in non-Episode 3 game");
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

static void server_command_min_level(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  u16string buffer;
  l->min_level = stoull(encode_sjis(args)) - 1;
  send_text_message_printf(l, "$C6Minimum level set to %" PRIu32,
      l->min_level + 1);
}

static void server_command_max_level(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
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

static void server_command_edit(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, false);
  check_version(c, GameVersion::BB);

  string encoded_args = tolower(encode_sjis(args));
  vector<string> tokens = split(encoded_args, ' ');

  try {
    if (tokens.at(0) == "atp") {
      c->game_data.player()->disp.stats.atp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "mst") {
      c->game_data.player()->disp.stats.mst = stoul(tokens.at(1));
    } else if (tokens.at(0) == "evp") {
      c->game_data.player()->disp.stats.evp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "hp") {
      c->game_data.player()->disp.stats.hp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "dfp") {
      c->game_data.player()->disp.stats.dfp = stoul(tokens.at(1));
    } else if (tokens.at(0) == "ata") {
      c->game_data.player()->disp.stats.ata = stoul(tokens.at(1));
    } else if (tokens.at(0) == "lck") {
      c->game_data.player()->disp.stats.lck = stoul(tokens.at(1));
    } else if (tokens.at(0) == "meseta") {
      c->game_data.player()->disp.meseta = stoul(tokens.at(1));
    } else if (tokens.at(0) == "exp") {
      c->game_data.player()->disp.experience = stoul(tokens.at(1));
    } else if (tokens.at(0) == "level") {
      c->game_data.player()->disp.level = stoul(tokens.at(1)) - 1;
    } else if (tokens.at(0) == "namecolor") {
      uint32_t new_color;
      sscanf(tokens.at(1).c_str(), "%8X", &new_color);
      c->game_data.player()->disp.name_color = new_color;
    } else if (tokens.at(0) == "secid") {
      uint8_t secid = section_id_for_name(decode_sjis(tokens.at(1)));
      if (secid == 0xFF) {
        send_text_message(c, u"$C6No such section ID");
        return;
      } else {
        c->game_data.player()->disp.section_id = secid;
      }
    } else if (tokens.at(0) == "name") {
      c->game_data.player()->disp.name = add_language_marker(tokens.at(1), 'J');
    } else if (tokens.at(0) == "npc") {
      if (tokens.at(1) == "none") {
        c->game_data.player()->disp.extra_model = 0;
        c->game_data.player()->disp.v2_flags &= 0xFD;
      } else {
        uint8_t npc = npc_for_name(decode_sjis(tokens.at(1)));
        if (npc == 0xFF) {
          send_text_message(c, u"$C6No such NPC");
          return;
        }
        c->game_data.player()->disp.extra_model = npc;
        c->game_data.player()->disp.v2_flags |= 0x02;
      }
    } else if (tokens.at(0) == "tech") {
      uint8_t level = stoul(tokens.at(2)) - 1;
      if (tokens.at(1) == "all") {
        for (size_t x = 0; x < 0x14; x++) {
          c->game_data.player()->disp.technique_levels.data()[x] = level;
        }
      } else {
        uint8_t tech_id = technique_for_name(decode_sjis(tokens.at(1)));
        if (tech_id == 0xFF) {
          send_text_message(c, u"$C6No such technique");
          return;
        }
        try {
          c->game_data.player()->disp.technique_levels[tech_id] = level;
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

// TODO: implement this
// TODO: make sure the bank name is filesystem-safe
/* static void server_command_change_bank(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string&) {
  check_version(c, GameVersion::BB);

  TODO
} */

// TODO: This can be implemented on the proxy server too.
static void server_command_convert_char_to_bb(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, const std::u16string& args) {
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
    s->license_manager->verify_bb(tokens[0].c_str(), tokens[1].c_str());
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

static void server_command_silence(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::SILENCE_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  target->can_chat = !target->can_chat;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s %ssilenced", target_name.c_str(),
      target->can_chat ? "un" : "");
}

static void server_command_kick(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::KICK_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // This should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  send_message_box(target, u"$C6You were kicked off by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s kicked off", target_name.c_str());
}

static void server_command_ban(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_privileges(c, Privilege::BAN_USER);

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

  if (target->license->privileges & Privilege::BAN_USER) {
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

  // TODO: put the length of time in this message. or don't; presumably the
  // person deserved it
  s->license_manager->ban_until(target->license->serial_number, now() + usecs);
  send_message_box(target, u"$C6You were banned by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s banned", target_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Cheat commands

static void server_command_warp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, true);

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
  
  G_InterLevelWarp_6x94 cmd = { {0x94, 0x02, 0}, area, {} };
  send_command_t(l, 0x62, c->lobby_client_id, cmd);
}

static void server_command_warpme(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
    check_is_game(l, true);

    uint32_t area = stoul(encode_sjis(args), nullptr, 0);
    if (c->area == area) {
        return;
    }

    size_t limit = area_limit_for_episode(l->episode);
    if (limit == 0) {
        return;
    }
    else if (area > limit) {
        send_text_message_printf(c, "$C6Area numbers must\nbe %zu or less.", limit);
        return;
    }

    send_warp(c, area);
}

static void proxy_command_warp(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  if (!session.is_in_game) {
    send_text_message(session.client_channel, u"$C6You must be in a\ngame to use this\ncommand");
    return;
  }
  uint32_t area = stoul(encode_sjis(args), nullptr, 0);
  // TODO: Add limit check here like in the server command implementation
  send_warp(session.client_channel, session.lobby_client_id, area);
  session.area = area;
}

static void server_command_next(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);

  size_t limit = area_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  }
  send_warp(c, (c->area + 1) % limit);
}

static void proxy_command_next(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  if (!session.is_in_game) {
    send_text_message(session.client_channel, u"$C6You must be in a\ngame to use this\ncommand");
    return;
  }

  session.area++;
  send_warp(session.client_channel, session.lobby_client_id, session.area);
}

static void server_command_what(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
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
      string name = item.inv_item.data.name(true);
      send_text_message(c, decode_sjis(name));
    }
  }
}

static void server_command_song(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_ep3(c, true);

  uint32_t song = stoul(encode_sjis(args), nullptr, 0);
  send_ep3_change_music(c->channel, song);
}

static void proxy_command_song(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  int32_t song = stol(encode_sjis(args), nullptr, 0);
  if (song < 0) {
    song = -song;
    send_ep3_change_music(session.server_channel, song);
  }
  send_ep3_change_music(session.client_channel, song);
}

static void server_command_infinite_hp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);

  c->options.infinite_hp = !c->options.infinite_hp;
  send_text_message_printf(c, "$C6Infinite HP %s",
      c->options.infinite_hp ? "enabled" : "disabled");
}

static void proxy_command_infinite_hp(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  session.options.infinite_hp = !session.options.infinite_hp;
  send_text_message_printf(session.client_channel, "$C6Infinite HP %s",
      session.options.infinite_hp ? "enabled" : "disabled");
}

static void server_command_infinite_tp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);

  c->options.infinite_tp = !c->options.infinite_tp;
  send_text_message_printf(c, "$C6Infinite TP %s",
      c->options.infinite_tp ? "enabled" : "disabled");
}

static void proxy_command_infinite_tp(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  session.options.infinite_tp = !session.options.infinite_tp;
  send_text_message_printf(session.client_channel, "$C6Infinite TP %s",
      session.options.infinite_tp ? "enabled" : "disabled");
}

static void server_command_switch_assist(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);

  c->options.switch_assist = !c->options.switch_assist;
  send_text_message_printf(c, "$C6Switch assist %s",
      c->options.switch_assist ? "enabled" : "disabled");
}

static void proxy_command_switch_assist(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string&) {
  session.options.switch_assist = !session.options.switch_assist;
  send_text_message_printf(session.client_channel, "$C6Switch assist %s",
      session.options.switch_assist ? "enabled" : "disabled");
}

static void server_command_lower_hp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
    check_is_game(l, true);
    if (!l->is_game()) {
        send_text_message(c, u"Cannot use command in\nlobby.");
    }
    send_player_stats_change(l, c, PlayerStatsChange::SUBTRACT_HP, 2550);
    send_player_stats_change(l, c, PlayerStatsChange::ADD_HP, 3);
    send_text_message(c, u"Lowered Client HP.");
}

static void server_command_questburst(shared_ptr<ServerState>, shared_ptr<Lobby>l,
    shared_ptr<Client> c, const std::u16string&) {
    if (l->flags & (Lobby::Flag::QUEST_IN_PROGRESS)) {

        send_text_message(c, u"Now leaving quest."); //Lobby burst
        send_leave_quest(c);
    }
    else {
        send_text_message(c, u"Cannot use outside of\nquest.");
    }
}

static void server_command_drop(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string&) {
  check_is_game(l, true);
  check_is_leader(l, c);
  l->flags ^= Lobby::Flag::DROPS_ENABLED;
  send_text_message_printf(l, "Drops %s", (l->flags & Lobby::Flag::DROPS_ENABLED) ? "enabled" : "disabled");
}

static void server_command_item(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args) {
  check_is_game(l, true);

  string data = parse_data_string(encode_sjis(args));
  if (data.size() < 2) {
    send_text_message(c, u"$C6Item codes must be\n2 bytes or more");
    return;
  }
  if (data.size() > 16) {
    send_text_message(c, u"$C6Item codes must be\n16 bytes or fewer");
    return;
  }

  PlayerInventoryItem item;
  item.data.id = l->generate_item_id(c->lobby_client_id);
  if (data.size() <= 12) {
    memcpy(item.data.data1.data(), data.data(), data.size());
  } else {
    memcpy(item.data.data1.data(), data.data(), 12);
    memcpy(item.data.data2.data(), data.data() + 12, data.size() - 12);
  }

  l->add_item(item, c->area, c->x, c->z);
  send_drop_stacked_item(l, item.data, c->area, c->x, c->z);

  string name = item.data.name(true);
  send_text_message(c, u"$C7Item created:\n" + decode_sjis(name));
}

static void proxy_command_item(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args) {
  if (session.version == GameVersion::BB) {
    send_text_message(session.client_channel,
        u"$C6This command cannot\nbe used on the proxy\nserver in BB games");
    return;
  }
  if (!session.is_in_game) {
    send_text_message(session.client_channel,
        u"$C6You must be in\na game to use this\ncommand");
    return;
  }
  if (session.lobby_client_id != session.leader_client_id) {
    send_text_message(session.client_channel,
        u"$C6You must be the\nleader to use this\ncommand");
    return;
  }

  bool set_drop = (!args.empty() && (args[0] == u'!'));

  string data = parse_data_string(encode_sjis(args));
  if (data.size() < 2) {
    send_text_message(session.client_channel, u"$C6Item codes must be\n2 bytes or more");
    return;
  }
  if (data.size() > 16) {
    send_text_message(session.client_channel, u"$C6Item codes must be\n16 bytes or fewer");
    return;
  }

  PlayerInventoryItem item;
  item.data.id = random_object<uint32_t>();
  if (data.size() <= 12) {
    memcpy(item.data.data1.data(), data.data(), data.size());
  } else {
    memcpy(item.data.data1.data(), data.data(), 12);
    memcpy(item.data.data2.data(), data.data() + 12, data.size() - 12);
  }

  if (set_drop) {
    session.next_drop_item = item;

    string name = session.next_drop_item.data.name(true);
    send_text_message(session.client_channel, u"$C7Next drop:\n" + decode_sjis(name));

  } else {
    send_drop_stacked_item(session.client_channel, item.data, session.area, session.x, session.z);
    send_drop_stacked_item(session.server_channel, item.data, session.area, session.x, session.z);

    string name = item.data.name(true);
    send_text_message(session.client_channel, u"$C7Item created:\n" + decode_sjis(name));
  }
}

////////////////////////////////////////////////////////////////////////////////

typedef void (*server_handler_t)(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const std::u16string& args);
typedef void (*proxy_handler_t)(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, const std::u16string& args);
struct ChatCommandDefinition {
  server_handler_t server_handler;
  proxy_handler_t proxy_handler;
  u16string usage;
};

static const unordered_map<u16string, ChatCommandDefinition> chat_commands({
    // TODO: implement command_help and actually use the usage strings here
    {u"$allevent", {server_command_lobby_event_all, nullptr, u"Usage:\nallevent <name/ID>"}},
    {u"$ann", {server_command_announce, nullptr, u"Usage:\nann <message>"}},
    {u"$arrow", {server_command_arrow, proxy_command_arrow, u"Usage:\narrow <color>"}},
    {u"$auction", {server_command_auction, proxy_command_auction, u"Usage:\nauction"}},
    {u"$ax", {server_command_ax, nullptr, u"Usage:\nax <message>"}},
    {u"$ban", {server_command_ban, nullptr, u"Usage:\nban <name-or-number>"}},
    // TODO: implement this on proxy server
    {u"$bbchar", {server_command_convert_char_to_bb, nullptr, u"Usage:\nbbchar <user> <pass> <1-4>"}},
    {u"$cheat", {server_command_cheat, nullptr, u"Usage:\ncheat"}},
    {u"$debug", {server_command_debug, nullptr, u"Usage:\ndebug"}},
    {u"$edit", {server_command_edit, nullptr, u"Usage:\nedit <stat> <value>"}},
    {u"$event", {server_command_lobby_event, proxy_command_lobby_event, u"Usage:\nevent <name>"}},
    {u"$exit", {server_command_exit, proxy_command_exit, u"Usage:\nexit"}},
    {u"$gc", {server_command_get_self_card, proxy_command_get_player_card, u"Usage:\ngc"}},
    {u"$infhp", {server_command_infinite_hp, proxy_command_infinite_hp, u"Usage:\ninfhp"}},
    {u"$inftp", {server_command_infinite_tp, proxy_command_infinite_tp, u"Usage:\ninftp"}},
    {u"$item", {server_command_item, proxy_command_item, u"Usage:\nitem <item-code>"}},
    {u"$i", {server_command_item, proxy_command_item, u"Usage:\ni <item-code>"}},
    {u"$kick", {server_command_kick, nullptr, u"Usage:\nkick <name-or-number>"}},
    {u"$li", {server_command_lobby_info, proxy_command_lobby_info, u"Usage:\nli"}},
    {u"$maxlevel", {server_command_max_level, nullptr, u"Usage:\nmax_level <level>"}},
    {u"$minlevel", {server_command_min_level, nullptr, u"Usage:\nmin_level <level>"}},
    {u"$next", {server_command_next, proxy_command_next, u"Usage:\nnext"}},
    {u"$password", {server_command_password, nullptr, u"Usage:\nlock [password]\nomit password to\nunlock game"}},
    {u"$patch", {server_command_patch, proxy_command_patch, u"Usage:\npatch <name>"}},
    {u"$persist", {server_command_persist, nullptr, u"Usage:\npersist"}},
    {u"$playrec", {server_command_playrec, nullptr, u"Usage:\nplayrec <filename>"}},
    {u"$rand", {server_command_rand, proxy_command_rand, u"Usage:\nrand [hex seed]\nomit seed to revert\nto default"}},
    {u"$saverec", {server_command_saverec, nullptr, u"Usage:\nsaverec <filename>"}},
    {u"$sc", {server_command_send_client, proxy_command_send_client, u"Usage:\nsc <data>"}},
    {u"$secid", {server_command_secid, proxy_command_secid, u"Usage:\nsecid [section ID]\nomit section ID to\nrevert to normal"}},
    {u"$silence", {server_command_silence, nullptr, u"Usage:\nsilence <name-or-number>"}},
    // TODO: implement this on proxy server
    {u"$song", {server_command_song, proxy_command_song, u"Usage:\nsong <song-number>"}},
    {u"$spec", {server_command_spec, nullptr, u"Usage:\nspec"}},
    {u"$ss", {nullptr, proxy_command_send_server, u"Usage:\nss <data>"}},
    {u"$swa", {server_command_switch_assist, proxy_command_switch_assist, u"Usage:\nswa"}},
    {u"$type", {server_command_lobby_type, nullptr, u"Usage:\ntype <name>"}},
    {u"$warp", {server_command_warp, proxy_command_warp, u"Usage:\nwarp <area-number>"}},
    {u"$warpme", {server_command_warpme, nullptr, u"Usage:\nwarp <area-number>"}},
    {u"$what", {server_command_what, nullptr, u"Usage:\nwhat"}},
    {u"$lower", {server_command_lower_hp, nullptr, u"Usage:\nLowers HP to 3"}},
    {u"$lobby", {server_command_questburst, nullptr, u"Usage:\nExit quest to lobby"}},
    {u"$drop", {server_command_drop, nullptr, u"Usage:\nToggles drops"}},
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
void on_chat_command(std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, const std::u16string& text) {
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
      def->server_handler(s, l, c, cmd.args);
    } catch (const precondition_failed& e) {
      send_text_message(c, e.what());
    } catch (const exception& e) {
      send_text_message_printf(c, "$C6Failed:\n%s", e.what());
    }
  }
}

void on_chat_command(std::shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, const std::u16string& text) {
  SplitCommand cmd(text);

  const ChatCommandDefinition* def = nullptr;
  try {
    def = &chat_commands.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(session.client_channel, u"$C6Unknown command");
    return;
  }

  if (!def->proxy_handler) {
    send_text_message(session.client_channel, u"$C6Command not available\non proxy server");
  } else {
    try {
      def->proxy_handler(s, session, cmd.args);
    } catch (const precondition_failed& e) {
      send_text_message(session.client_channel, e.what());
    } catch (const exception& e) {
      send_text_message_printf(session.client_channel, "$C6Failed:\n%s", e.what());
    }
  }
}
