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
#include "Revision.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Checks

class precondition_failed {
public:
  precondition_failed(const std::string& user_msg) : user_msg(user_msg) {}
  ~precondition_failed() = default;

  const std::string& what() const {
    return this->user_msg;
  }

private:
  std::string user_msg;
};

static void check_license_flags(shared_ptr<Client> c, uint32_t mask) {
  if (!c->license) {
    throw precondition_failed("$C6You are not\nlogged in.");
  }
  if ((c->license->flags & mask) != mask) {
    throw precondition_failed("$C6You do not have\npermission to\nrun this command.");
  }
}

static void check_version(shared_ptr<Client> c, Version version) {
  if (c->version() != version) {
    throw precondition_failed("$C6This command cannot\nbe used for your\nversion of PSO.");
  }
}

static void check_is_game(shared_ptr<Lobby> l, bool is_game) {
  if (l->is_game() != is_game) {
    throw precondition_failed(is_game ? "$C6This command cannot\nbe used in lobbies." : "$C6This command cannot\nbe used in games.");
  }
}

static void check_is_ep3(shared_ptr<Client> c, bool is_ep3) {
  if (::is_ep3(c->version()) != is_ep3) {
    throw precondition_failed(is_ep3 ? "$C6This command can only\nbe used in Episode 3." : "$C6This command cannot\nbe used in Episode 3.");
  }
}

static void check_cheats_enabled(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  if (!l->check_flag(Lobby::Flag::CHEATS_ENABLED) && !(c->license->flags & License::Flag::CHEAT_ANYWHERE)) {
    throw precondition_failed("$C6This command can\nonly be used in\ncheat mode.");
  }
}

static void check_cheats_allowed(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  if ((s->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) && !(c->license->flags & License::Flag::CHEAT_ANYWHERE)) {
    throw precondition_failed("$C6Cheats are disabled\non this server.");
  }
}

static void check_cheats_allowed(shared_ptr<ServerState> s, shared_ptr<ProxyServer::LinkedSession> ses) {
  if ((s->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) && (!ses->license || !(ses->license->flags & License::Flag::CHEAT_ANYWHERE))) {
    throw precondition_failed("$C6Cheats are disabled\non this proxy.");
  }
}

static void check_is_leader(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  if (l->leader_id != c->lobby_client_id) {
    throw precondition_failed("$C6This command can\nonly be used by\nthe game leader.");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Message commands

static void server_command_server_info(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  string uptime_str = format_duration(now() - s->creation_time);
  string build_date = format_time(BUILD_TIMESTAMP);
  send_text_message_printf(c,
      "Revision: $C6%s$C7\n$C6%s$C7\nUptime: $C6%s$C7\nLobbies: $C6%zu$C7\nClients: $C6%zu$C7(g) $C6%zu$C7(p)",
      GIT_REVISION_HASH,
      build_date.c_str(),
      uptime_str.c_str(),
      s->id_to_lobby.size(),
      s->channel_to_client.size(),
      s->proxy_server->num_sessions());
}

static void server_command_lobby_info(shared_ptr<Client> c, const std::string&) {
  vector<string> lines;

  auto l = c->lobby.lock();
  if (!l) {
    lines.emplace_back("$C4No lobby info");

  } else {
    if (l->is_game()) {
      if (!l->is_ep3()) {
        if (l->max_level == 0xFFFFFFFF) {
          lines.emplace_back(string_printf("$C6%08X$C7 L$C6%d+$C7", l->lobby_id, l->min_level + 1));
        } else {
          lines.emplace_back(string_printf("$C6%08X$C7 L$C6%d-%d$C7", l->lobby_id, l->min_level + 1, l->max_level + 1));
        }
        lines.emplace_back(string_printf("$C7Section ID: $C6%s$C7", name_for_section_id(l->section_id)));

        switch (l->drop_mode) {
          case Lobby::DropMode::DISABLED:
            lines.emplace_back("Drops disabled");
            break;
          case Lobby::DropMode::CLIENT:
            lines.emplace_back("Client item table");
            break;
          case Lobby::DropMode::SERVER_SHARED:
            lines.emplace_back("Server item table");
            break;
          case Lobby::DropMode::SERVER_PRIVATE:
            lines.emplace_back("Server indiv items");
            break;
          case Lobby::DropMode::SERVER_DUPLICATE:
            lines.emplace_back("Server dup items");
            break;
          default:
            lines.emplace_back("$C4Unknown drop mode$C7");
        }
        if (l->check_flag(Lobby::Flag::CHEATS_ENABLED)) {
          lines.emplace_back("Cheats enabled");
        }

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

  send_text_message(c, join(lines, "\n"));
}

static void server_command_ping(shared_ptr<Client> c, const std::string&) {
  c->ping_start_time = now();
  send_command(c, 0x1D, 0x00);
}

static void proxy_command_ping(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  ses->client_ping_start_time = now();
  ses->server_ping_start_time = now();

  C_GuildCardSearch_40 cmd = {0x00010000, ses->remote_guild_card_number, ses->remote_guild_card_number};
  ses->client_channel.send(0x1D, 0x00);
  ses->server_channel.send(0x40, 0x00, &cmd, sizeof(cmd));
}

static void proxy_command_lobby_info(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
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
  if (ses->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
    cheats_tokens.emplace_back("SWA");
  }
  if (ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
    cheats_tokens.emplace_back("HP");
  }
  if (ses->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
    cheats_tokens.emplace_back("TP");
  }
  if (!cheats_tokens.empty()) {
    msg += "\n$C7Cheats: $C6";
    msg += join(cheats_tokens, ",");
  }

  vector<const char*> behaviors_tokens;
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    behaviors_tokens.emplace_back("SAVE");
  }
  if (ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN)) {
    behaviors_tokens.emplace_back("SL");
  }
  if (ses->config.check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS)) {
    behaviors_tokens.emplace_back("BFC");
  }
  if (!behaviors_tokens.empty()) {
    msg += "\n$C7Flags: $C6";
    msg += join(behaviors_tokens, ",");
  }

  if (ses->config.override_section_id != 0xFF) {
    msg += "\n$C7SecID*: $C6";
    msg += name_for_section_id(ses->config.override_section_id);
  }

  send_text_message(ses->client_channel, msg);
}

static void server_command_ax(shared_ptr<Client> c, const std::string& args) {
  check_license_flags(c, License::Flag::ANNOUNCE);
  ax_messages_log.info("%s", args.c_str());
}

static void server_command_announce(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  check_license_flags(c, License::Flag::ANNOUNCE);
  send_text_message(s, args);
}

static void server_command_arrow(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  c->lobby_arrow_color = stoull(args, nullptr, 0);
  if (!l->is_game()) {
    send_arrow_update(l);
  }
}

static void proxy_command_arrow(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  ses->server_channel.send(0x89, stoull(args, nullptr, 0));
}

static void server_command_debug(shared_ptr<Client> c, const std::string&) {
  check_license_flags(c, License::Flag::DEBUG);
  c->config.toggle_flag(Client::Flag::DEBUG_ENABLED);
  send_text_message_printf(c, "Debug %s", (c->config.check_flag(Client::Flag::DEBUG_ENABLED) ? "enabled" : "disabled"));
}

static void server_command_quest(shared_ptr<Client> c, const std::string& args) {
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  Version effective_version = is_ep3(c->version()) ? Version::GC_V3 : c->version();

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  auto q = s->quest_index_for_version(effective_version)->get(stoul(args));
  if (!q) {
    send_text_message(c, "$C6Quest not found");
  } else {
    set_lobby_quest(c->require_lobby(), q, true);
  }
}

static void server_command_qcheck(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  uint16_t flag_num = stoul(args, nullptr, 0);

  send_text_message_printf(c, "$C7Quest flag 0x%hX (%hu)\nis %s on %s",
      flag_num, flag_num,
      c->character()->quest_flags.get(l->difficulty, flag_num) ? "set" : "not set",
      name_for_difficulty(l->difficulty));
}

static void server_command_qset_qclear(shared_ptr<Client> c, const std::string& args, bool should_set) {
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    send_text_message(c, "$C6This command cannot\nbe used in the lobby");
    return;
  }

  uint16_t flag_num = stoul(args, nullptr, 0);

  if (should_set) {
    c->character()->quest_flags.set(l->difficulty, flag_num);
  } else {
    c->character()->quest_flags.clear(l->difficulty, flag_num);
  }

  if (is_v1_or_v2(c->version())) {
    G_UpdateQuestFlag_DC_PC_6x75 cmd = {{0x75, 0x02, 0x0000}, flag_num, should_set ? 0 : 1};
    send_command_t(l, 0x60, 0x00, cmd);
  } else {
    G_UpdateQuestFlag_V3_BB_6x75 cmd = {{{0x75, 0x03, 0x0000}, flag_num, should_set ? 0 : 1}, l->difficulty, 0x0000};
    send_command_t(l, 0x60, 0x00, cmd);
  }
}

static void server_command_qset(shared_ptr<Client> c, const std::string& args) {
  return server_command_qset_qclear(c, args, true);
}

static void server_command_qclear(shared_ptr<Client> c, const std::string& args) {
  return server_command_qset_qclear(c, args, false);
}

static void proxy_command_qset_qclear(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args, bool should_set) {
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, "$C6This command cannot\nbe used in the lobby");
    return;
  }

  uint16_t flag_num = stoul(args, nullptr, 0);
  if (is_v1_or_v2(ses->version())) {
    G_UpdateQuestFlag_DC_PC_6x75 cmd = {{0x75, 0x02, 0x0000}, flag_num, should_set ? 0 : 1};
    ses->client_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
    ses->server_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
  } else {
    G_UpdateQuestFlag_V3_BB_6x75 cmd = {{{0x75, 0x03, 0x0000}, flag_num, should_set ? 0 : 1}, ses->difficulty, 0x0000};
    ses->client_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
    ses->server_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
  }
}

static void proxy_command_qset(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  return proxy_command_qset_qclear(ses, args, true);
}

static void proxy_command_qclear(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  return proxy_command_qset_qclear(ses, args, false);
}

static void server_command_qsync(shared_ptr<Client> c, const std::string& args) {
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    send_text_message(c, "$C6This command cannot\nbe used in the lobby");
    return;
  }

  auto tokens = split(args, ' ');
  if (tokens.size() != 2) {
    send_text_message(c, "$C6Incorrect number of\narguments");
    return;
  }

  G_SyncQuestData_6x77 cmd;
  cmd.header = {0x77, 0x03, 0x0000};
  cmd.register_number = stoul(tokens[0].substr(1), nullptr, 0);
  cmd.unused = 0;
  if (tokens[0][0] == 'r') {
    cmd.value.as_int = stoul(tokens[1], nullptr, 0);
  } else if (tokens[0][0] == 'f') {
    cmd.value.as_float = stof(tokens[1]);
  } else {
    send_text_message(c, "$C6First argument must\nbe a register");
    return;
  }
  send_command_t(c, 0x60, 0x00, cmd);
}

static void proxy_command_qsync(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, "$C6This command cannot\nbe used in the lobby");
    return;
  }

  auto tokens = split(args, ' ');
  if (tokens.size() != 2) {
    send_text_message(ses->client_channel, "$C6Incorrect number of\narguments");
    return;
  }

  G_SyncQuestData_6x77 cmd;
  cmd.header = {0x77, 0x03, 0x0000};
  cmd.register_number = stoul(tokens[0].substr(1), nullptr, 0);
  cmd.unused = 0;
  if (tokens[0][0] == 'r') {
    cmd.value.as_int = stoul(tokens[1], nullptr, 0);
  } else if (tokens[0][0] == 'f') {
    cmd.value.as_float = stof(tokens[1]);
  } else {
    send_text_message(ses->client_channel, "$C6First argument must\nbe a register");
    return;
  }
  ses->client_channel.send(0x60, 0x00, cmd);
}

static void server_command_qcall(shared_ptr<Client> c, const std::string& args) {
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  auto l = c->require_lobby();
  if (l->is_game() && l->quest) {
    send_quest_function_call(c, stoul(args, nullptr, 0));
  } else {
    send_text_message(c, "$C6You must be in a\nquest to use this\ncommand");
  }
}

static void proxy_command_qcall(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  if (ses->is_in_game && ses->is_in_quest) {
    send_quest_function_call(ses->client_channel, stoul(args, nullptr, 0));
  } else {
    send_text_message(ses->client_channel, "$C6You must be in a\nquest to use this\ncommand");
  }
}

static void server_command_show_material_counts(shared_ptr<Client> c, const std::string&) {
  auto p = c->character();
  if (is_v1_or_v2(c->version())) {
    send_text_message_printf(c, "%hhu HP, %hhu TP",
        p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
  } else {
    send_text_message_printf(c, "%hhu HP, %hhu TP, %hhu POW\n%hhu MIND, %hhu EVADE\n%hhu DEF, %hhu LUCK",
        p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::TP),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF),
        p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
  }
}

static void server_command_auction(shared_ptr<Client> c, const std::string&) {
  check_license_flags(c, License::Flag::DEBUG);
  auto l = c->require_lobby();
  if (l->is_game() && l->is_ep3()) {
    G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
    cmd.header.sender_client_id = c->lobby_client_id;
    send_command_t(l, 0xC9, 0x00, cmd);
  }
}

static void proxy_command_auction(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  G_InitiateCardAuction_GC_Ep3_6xB5x42 cmd;
  cmd.header.sender_client_id = ses->lobby_client_id;
  ses->client_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
  ses->server_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
}

static void server_command_patch(shared_ptr<Client> c, const std::string& args) {
  prepare_client_for_patches(c, [wc = weak_ptr<Client>(c), args]() {
    auto c = wc.lock();
    if (!c) {
      return;
    }
    try {
      auto s = c->require_server_state();
      // Note: We can't look this up outside of the closure because
      // c->specific_version can change during prepare_client_for_patches
      auto fn = s->function_code_index->name_and_specific_version_to_patch_function.at(
          string_printf("%s-%08" PRIX32, args.c_str(), c->config.specific_version));
      send_function_call(c, fn);
      c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
    } catch (const out_of_range&) {
      send_text_message(c, "Invalid patch name");
    }
  });
}

static void empty_patch_return_handler(uint32_t, uint32_t) {}

static void proxy_command_patch(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  auto send_call = [args, ses](uint32_t specific_version, uint32_t) {
    try {
      if (ses->config.specific_version != specific_version) {
        ses->config.specific_version = specific_version;
        ses->log.info("Version detected as %08" PRIX32, ses->config.specific_version);
      }
      auto s = ses->require_server_state();
      auto fn = s->function_code_index->name_and_specific_version_to_patch_function.at(
          string_printf("%s-%08" PRIX32, args.c_str(), ses->config.specific_version));
      send_function_call(ses->client_channel, ses->config, fn);
      // Don't forward the patch response to the server
      ses->function_call_return_handler_queue.emplace_back(empty_patch_return_handler);
    } catch (const out_of_range&) {
      send_text_message(ses->client_channel, "Invalid patch name");
    }
  };

  auto send_version_detect_or_send_call = [args, ses, send_call]() {
    if (is_gc(ses->version()) &&
        ses->config.specific_version == default_specific_version_for_version(ses->version(), -1)) {
      auto s = ses->require_server_state();
      send_function_call(
          ses->client_channel,
          ses->config,
          s->function_code_index->name_to_function.at("VersionDetect"));
      ses->function_call_return_handler_queue.emplace_back(send_call);
    } else {
      send_call(ses->config.specific_version, 0);
    }
  };

  // This mirrors the implementation in prepare_client_for_patches
  if (!ses->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH)) {
    auto s = ses->require_server_state();
    send_function_call(
        ses->client_channel, ses->config, s->function_code_index->name_to_function.at("CacheClearFix-Phase1"), {}, "", 0, 0, 0x7F2734EC);
    ses->function_call_return_handler_queue.emplace_back([s, ses, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
      send_function_call(
          ses->client_channel, ses->config, s->function_code_index->name_to_function.at("CacheClearFix-Phase2"));
      ses->function_call_return_handler_queue.emplace_back([ses, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
        ses->config.set_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
        send_version_detect_or_send_call();
      });
    });
  } else {
    send_version_detect_or_send_call();
  }
}

static void server_command_persist(shared_ptr<Client> c, const std::string&) {
  auto l = c->require_lobby();
  if (l->check_flag(Lobby::Flag::DEFAULT)) {
    send_text_message(c, "$C6Default lobbies\ncannot be marked\ntemporary");
  } else if (!l->check_flag(Lobby::Flag::GAME)) {
    send_text_message(c, "$C6Private lobbies\ncannot be marked\npersistent");
  } else if (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) || l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    send_text_message(c, "$C6Games cannot be\npersistent if a\nquest has already\nbegun");
  } else if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    send_text_message(c, "$C6Spectator teams\ncannot be marked\npersistent");
  } else {
    l->toggle_flag(Lobby::Flag::PERSISTENT);
    send_text_message_printf(l, "Lobby persistence\n%s", l->check_flag(Lobby::Flag::PERSISTENT) ? "enabled" : "disabled");
  }
}

static void server_command_exit(shared_ptr<Client> c, const std::string&) {
  auto l = c->require_lobby();
  if (l->is_game()) {
    if (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) || l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      c->channel.send(0x60, 0x00, cmd);
      c->floor = 0;
    } else if (is_ep3(c->version())) {
      c->channel.send(0xED, 0x00);
    } else {
      send_text_message(c, "$C6You must return to\nthe lobby first");
    }
  } else {
    send_self_leave_notification(c);
    if (!c->config.check_flag(Client::Flag::NO_D6)) {
      send_message_box(c, "");
    }

    send_client_to_login_server(c);
  }
}

static void proxy_command_exit(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  if (ses->is_in_game) {
    if (is_ep3(ses->version())) {
      ses->client_channel.send(0xED, 0x00);
    } else if (ses->is_in_quest) {
      G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
      ses->client_channel.send(0x60, 0x00, cmd);
    } else {
      send_text_message(ses->client_channel, "$C6You must return to\nthe lobby first");
    }
  } else {
    ses->disconnect_action = ProxyServer::LinkedSession::DisconnectAction::CLOSE_IMMEDIATELY;
    ses->send_to_game_server();
  }
}

static void server_command_get_self_card(shared_ptr<Client> c, const std::string&) {
  send_guild_card(c, c);
}

static void proxy_command_get_player_card(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {

  bool any_card_sent = false;
  for (const auto& p : ses->lobby_players) {
    if (!p.name.empty() && args == p.name) {
      send_guild_card(ses->client_channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
      any_card_sent = true;
    }
  }

  if (!any_card_sent) {
    try {
      size_t index = stoull(args, nullptr, 0);
      const auto& p = ses->lobby_players.at(index);
      if (!p.name.empty()) {
        send_guild_card(ses->client_channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
      }
    } catch (const exception& e) {
      send_text_message_printf(ses->client_channel, "Error: %s", e.what());
    }
  }
}

static void server_command_send_client(shared_ptr<Client> c, const std::string& args) {
  string data = parse_data_string(args);
  data.resize((data.size() + 3) & (~3));
  c->channel.send(data);
}

static void proxy_command_send_client(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  string data = parse_data_string(args);
  data.resize((data.size() + 3) & (~3));
  ses->client_channel.send(data);
}

static void proxy_command_send_server(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  string data = parse_data_string(args);
  data.resize((data.size() + 3) & (~3));
  ses->server_channel.send(data);
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

static void server_command_cheat(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);
  if (l->check_flag(Lobby::Flag::CANNOT_CHANGE_CHEAT_MODE)) {
    send_text_message(c, "$C6Cheat mode cannot\nbe changed on this\nserver");
  } else {
    l->toggle_flag(Lobby::Flag::CHEATS_ENABLED);
    send_text_message_printf(l, "Cheat mode %s", l->check_flag(Lobby::Flag::CHEATS_ENABLED) ? "enabled" : "disabled");
  }
}

static void server_command_lobby_event(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_license_flags(c, License::Flag::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, "$C6No such lobby event.");
    return;
  }

  l->event = new_event;
  send_change_event(l, l->event);
}

static void proxy_command_lobby_event(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  if (args.empty()) {
    ses->config.override_lobby_event = 0xFF;
  } else {
    uint8_t new_event = event_for_name(args);
    if (new_event == 0xFF) {
      send_text_message(ses->client_channel, "$C6No such lobby event.");
    } else {
      ses->config.override_lobby_event = new_event;
      if (!is_v1_or_v2(ses->version())) {
        ses->client_channel.send(0xDA, ses->config.override_lobby_event);
      }
    }
  }
}

static void server_command_lobby_event_all(shared_ptr<Client> c, const std::string& args) {
  check_license_flags(c, License::Flag::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, "$C6No such lobby event.");
    return;
  }

  auto s = c->require_server_state();
  for (auto l : s->all_lobbies()) {
    if (l->is_game() || !l->check_flag(Lobby::Flag::DEFAULT)) {
      continue;
    }

    l->event = new_event;
    send_change_event(l, l->event);
  }
}

static void server_command_lobby_type(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);

  uint8_t new_type;
  if (args.empty()) {
    new_type = 0x80;
  } else {
    new_type = lobby_type_for_name(args);
    if (new_type == 0x80) {
      send_text_message(c, "$C6No such lobby type");
      return;
    }
  }

  c->config.override_lobby_number = new_type;
  send_join_lobby(c, l);
}

static void proxy_command_lobby_type(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  uint8_t new_type;
  if (args.empty()) {
    new_type = 0x80;
  } else {
    new_type = lobby_type_for_name(args);
    if (new_type == 0x80) {
      send_text_message(ses->client_channel, "$C6No such lobby type");
      return;
    }
  }
  ses->config.override_lobby_number = new_type;
}

static string file_path_for_recording(const std::string& args, uint32_t serial_number) {
  for (char ch : args) {
    if (ch <= 0x20 || ch > 0x7E || ch == '/') {
      throw runtime_error("invalid recording name");
    }
  }
  return string_printf("system/ep3/battle-records/%010" PRIu32 "_%s.mzrd", serial_number, args.c_str());
}

static void server_command_saverec(shared_ptr<Client> c, const std::string& args) {
  if (!c->ep3_prev_battle_record) {
    send_text_message(c, "$C4No finished\nrecording is\npresent");
    return;
  }
  string file_path = file_path_for_recording(args, c->license->serial_number);
  string data = c->ep3_prev_battle_record->serialize();
  save_file(file_path, data);
  send_text_message(c, "$C7Recording saved");
  c->ep3_prev_battle_record.reset();
}

static void server_command_playrec(shared_ptr<Client> c, const std::string& args) {
  if (!is_ep3(c->version())) {
    send_text_message(c, "$C4This command can\nonly be used on\nEpisode 3");
    return;
  }

  auto l = c->require_lobby();
  if (l->is_game() && l->battle_player) {
    l->battle_player->start();
  } else if (!l->is_game()) {
    string file_path = file_path_for_recording(args, c->license->serial_number);

    auto s = c->require_server_state();
    string filename = args;
    bool start_battle_player_immediately = (filename[0] == '!');
    if (start_battle_player_immediately) {
      filename = filename.substr(1);
    }

    string data;
    try {
      data = load_file(file_path);
    } catch (const cannot_open_file&) {
      send_text_message(c, "$C4The recording does\nnot exist");
      return;
    }
    auto record = make_shared<Episode3::BattleRecord>(data);
    auto battle_player = make_shared<Episode3::BattleRecordPlayer>(record, s->game_server->get_base());
    auto game = create_game_generic(
        s, c, args, "", Episode::EP3, GameMode::NORMAL, 0, false, nullptr, battle_player);
    if (game) {
      if (start_battle_player_immediately) {
        game->set_flag(Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY);
      }
      s->change_client_lobby(c, game);
      c->config.set_flag(Client::Flag::LOADING);
    }
  } else {
    send_text_message(c, "$C4This command cannot\nbe used in a game");
  }
}

static void server_command_meseta(shared_ptr<Client> c, const std::string& args) {
  check_is_ep3(c, true);
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  uint32_t amount = stoul(args, nullptr, 0);
  c->license->ep3_current_meseta += amount;
  c->license->ep3_total_meseta_earned += amount;
  c->license->save();
  send_ep3_rank_update(c);
  send_text_message_printf(c, "You now have\n$C6%" PRIu32 "$C7 Meseta", c->license->ep3_current_meseta);
}

////////////////////////////////////////////////////////////////////////////////
// Game commands

static void server_command_secid(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_cheats_allowed(c->require_server_state(), c);

  if (!args[0]) {
    c->config.override_section_id = 0xFF;
    send_text_message(c, "$C6Override section ID\nremoved");
  } else {
    uint8_t new_secid = section_id_for_name(args);
    if (new_secid == 0xFF) {
      send_text_message(c, "$C6Invalid section ID");
    } else {
      c->config.override_section_id = new_secid;
      send_text_message_printf(c, "$C6Override section ID\nset to %s", name_for_section_id(new_secid));
    }
  }
}

static void proxy_command_secid(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  check_cheats_allowed(ses->require_server_state(), ses);
  if (!args[0]) {
    ses->config.override_section_id = 0xFF;
    send_text_message(ses->client_channel, "$C6Override section ID\nremoved");
  } else {
    uint8_t new_secid = section_id_for_name(args);
    if (new_secid == 0xFF) {
      send_text_message(ses->client_channel, "$C6Invalid section ID");
    } else {
      ses->config.override_section_id = new_secid;
      send_text_message_printf(ses->client_channel, "$C6Override section ID\nset to %s", name_for_section_id(new_secid));
    }
  }
}

static void server_command_rand(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, false);
  check_cheats_allowed(s, c);

  if (!args[0]) {
    c->config.clear_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
    c->config.override_random_seed = 0;
    send_text_message(c, "$C6Override seed\nremoved");
  } else {
    c->config.set_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
    c->config.override_random_seed = stoul(args, 0, 16);
    send_text_message(c, "$C6Override seed\nset");
  }
}

static void proxy_command_rand(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  check_cheats_allowed(ses->require_server_state(), ses);
  if (!args[0]) {
    ses->config.clear_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
    ses->config.override_random_seed = 0;
    send_text_message(ses->client_channel, "$C6Override seed\nremoved");
  } else {
    ses->config.set_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
    ses->config.override_random_seed = stoul(args, 0, 16);
    send_text_message(ses->client_channel, "$C6Override seed\nset");
  }
}

static void server_command_password(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  if (!args[0]) {
    l->password[0] = 0;
    send_text_message(l, "$C6Game unlocked");

  } else {
    l->password = args;
    send_text_message_printf(l, "$C6Game password:\n%s", l->password.c_str());
  }
}

static void server_command_toggle_spectator_flag(shared_ptr<Client> c, const std::string&) {
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

  if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    send_text_message(c, "$C6This command cannot\nbe used in a spectator\nteam");
  }

  if (l->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN)) {
    l->clear_flag(Lobby::Flag::SPECTATORS_FORBIDDEN);
    send_text_message(l, "$C6Spectators allowed");

  } else {
    l->set_flag(Lobby::Flag::SPECTATORS_FORBIDDEN);
    for (auto watcher_l : l->watcher_lobbies) {
      send_command(watcher_l, 0xED, 0x00);
    }
    l->watcher_lobbies.clear();
    send_text_message(l, "$C6Spectators forbidden");
  }
}

static void server_command_min_level(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  l->min_level = stoull(args) - 1;
  send_text_message_printf(l, "$C6Minimum level set\nto %" PRIu32, l->min_level + 1);
}

static void server_command_max_level(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_leader(l, c);

  l->max_level = stoull(args) - 1;
  if (l->max_level >= 200) {
    l->max_level = 0xFFFFFFFF;
  }

  if (l->max_level == 0xFFFFFFFF) {
    send_text_message(l, "$C6Maximum level set\nto unlimited");
  } else {
    send_text_message_printf(l, "$C6Maximum level set\nto %" PRIu32, l->max_level + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Character commands

static void server_command_edit(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, false);
  if (!is_v1_or_v2(c->version()) && (c->version() != Version::BB_V4)) {
    throw precondition_failed("$C6This command cannot\nbe used for your\nversion of PSO.");
  }

  if ((s->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) && !(c->license->flags & License::Flag::CHEAT_ANYWHERE)) {
    send_text_message(l, "$C6Cheats are disabled\non this server");
    return;
  }

  string encoded_args = tolower(args);
  vector<string> tokens = split(encoded_args, ' ');

  try {
    auto p = c->character();
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
      uint32_t level = stoul(tokens.at(1)) - 1;
      p->disp.stats.reset_to_base(p->disp.visual.char_class, s->level_table);
      p->disp.stats.advance_to_level(p->disp.visual.char_class, level, s->level_table);
    } else if (tokens.at(0) == "namecolor") {
      uint32_t new_color;
      sscanf(tokens.at(1).c_str(), "%8X", &new_color);
      p->disp.visual.name_color = new_color;
    } else if (tokens.at(0) == "secid") {
      uint8_t secid = section_id_for_name(tokens.at(1));
      if (secid == 0xFF) {
        send_text_message(c, "$C6No such section ID");
        return;
      } else {
        p->disp.visual.section_id = secid;
      }
    } else if (tokens.at(0) == "name") {
      vector<string> orig_tokens = split(args, ' ');
      string name = ((p->inventory.language == 0) ? "\tE" : "\tJ") + orig_tokens.at(1);
      p->disp.name.clear();
      p->disp.name.encode(name, p->inventory.language);
    } else if (tokens.at(0) == "npc") {
      if (tokens.at(1) == "none") {
        p->disp.visual.extra_model = 0;
        p->disp.visual.validation_flags &= 0xFD;
      } else {
        uint8_t npc = npc_for_name(tokens.at(1));
        if (npc == 0xFF) {
          send_text_message(c, "$C6No such NPC");
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
        uint8_t tech_id = technique_for_name(tokens.at(1));
        if (tech_id == 0xFF) {
          send_text_message(c, "$C6No such technique");
          return;
        }
        try {
          p->set_technique_level(tech_id, level);
        } catch (const out_of_range&) {
          send_text_message(c, "$C6Invalid technique");
          return;
        }
      }
    } else {
      send_text_message(c, "$C6Unknown field");
      return;
    }
  } catch (const out_of_range&) {
    send_text_message(c, "$C6Not enough arguments");
    return;
  }

  // Reload the client in the lobby
  send_player_leave_notification(l, c->lobby_client_id);
  if (c->version() == Version::BB_V4) {
    send_complete_player_bb(c);
  }
  c->v1_v2_last_reported_disp.reset();
  s->send_lobby_join_notifications(l, c);
}

static void server_command_change_bank(shared_ptr<Client> c, const std::string& args) {
  check_version(c, Version::BB_V4);

  if (c->config.check_flag(Client::Flag::AT_BANK_COUNTER)) {
    throw runtime_error("cannot change banks while at the bank counter");
  }
  if (c->has_overlay()) {
    throw runtime_error("cannot change banks while Battle or Challenge is in progress");
  }

  ssize_t new_char_index = args.empty() ? (c->bb_character_index + 1) : stol(args, nullptr, 0);

  if (new_char_index == 0) {
    if (c->use_shared_bank()) {
      send_text_message_printf(c, "$C6Using shared bank (0)");
    } else {
      send_text_message_printf(c, "$C6Created shared bank (0)");
    }
  } else if (new_char_index <= 4) {
    c->use_character_bank(new_char_index - 1);
    auto bp = c->current_bank_character();
    auto name = bp->disp.name.decode(c->language());
    send_text_message_printf(c, "$C6Using %s\'s bank (%zu)", name.c_str(), new_char_index);
  } else {
    throw runtime_error("invalid bank number");
  }

  const auto& bank = c->current_bank();
  send_text_message_printf(c, "%" PRIu32 " items\n%" PRIu32 " Meseta", bank.num_items.load(), bank.meseta.load());
}

static void server_command_bbchar_savechar(shared_ptr<Client> c, const std::string& args, bool is_bb_conversion) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, false);

  auto pending_export = make_unique<Client::PendingCharacterExport>();
  pending_export->is_bb_conversion = is_bb_conversion;

  if (is_bb_conversion) {
    vector<string> tokens = split(args, ' ');
    if (tokens.size() != 3) {
      send_text_message(c, "$C6Incorrect argument count");
      return;
    }

    // username/password are tokens[0] and [1]
    pending_export->character_index = stoll(tokens[2]) - 1;
    if ((pending_export->character_index > 3) || (pending_export->character_index < 0)) {
      send_text_message(c, "$C6Player index must\nbe in range 1-4");
      return;
    }

    try {
      c->pending_character_export->license = s->license_index->verify_bb(tokens[0].c_str(), tokens[1].c_str());
    } catch (const exception& e) {
      send_text_message_printf(c, "$C6Login failed: %s", e.what());
      return;
    }

  } else {
    pending_export->character_index = stoll(args) - 1;
    if ((pending_export->character_index > 3) || (pending_export->character_index < 0)) {
      send_text_message(c, "$C6Player index must\nbe in range 1-4");
      return;
    }
    pending_export->license = c->license;
  }

  c->pending_character_export = std::move(pending_export);
  // Request the player data. The client will respond with a 61, and the handler
  // for that command will execute the conversion
  send_get_player_info(c);
}

static void server_command_bbchar(shared_ptr<Client> c, const std::string& args) {
  server_command_bbchar_savechar(c, args, true);
}

static void server_command_savechar(shared_ptr<Client> c, const std::string& args) {
  server_command_bbchar_savechar(c, args, false);
}

static void server_command_loadchar(shared_ptr<Client> c, const std::string& args) {
  if (!is_v1_or_v2(c->version())) {
    send_text_message(c, "$C7This command can only\nbe used on v1 or v2");
    return;
  }
  auto l = c->require_lobby();
  check_is_game(l, false);

  size_t index = stoull(args, nullptr, 0) - 1;
  if (index > 3) {
    send_text_message(c, "$C6Player index must\nbe in range 1-4");
    return;
  }
  c->load_backup_character(c->license->serial_number, index);

  auto s = c->require_server_state();
  send_player_leave_notification(l, c->lobby_client_id);
  s->send_lobby_join_notifications(l, c);
}

static void server_command_save(shared_ptr<Client> c, const std::string&) {
  check_version(c, Version::BB_V4);
  try {
    c->save_all();
    send_text_message(c, "All data saved");
  } catch (const exception& e) {
    send_text_message_printf(c, "Can\'t save data:\n%s", e.what());
  }
  c->reschedule_save_game_data_event();
}

////////////////////////////////////////////////////////////////////////////////
// Administration commands

static string name_for_client(shared_ptr<Client> c) {
  auto player = c->character(false);
  if (player.get()) {
    return player->disp.name.decode(player->inventory.language);
  }

  if (c->license.get()) {
    return string_printf("SN:%" PRIu32, c->license->serial_number);
  }

  return "Player";
}

static void server_command_silence(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::SILENCE_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, "$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::MODERATOR) {
    send_text_message(c, "$C6You do not have\nsufficient privileges.");
    return;
  }

  target->can_chat = !target->can_chat;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s %ssilenced", target_name.c_str(),
      target->can_chat ? "un" : "");
}

static void server_command_kick(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::KICK_USER);

  auto target = s->find_client(&args);
  if (!target->license) {
    // This should be impossible, but I'll bet it's not actually
    send_text_message(c, "$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::MODERATOR) {
    send_text_message(c, "$C6You do not have\nsufficient privileges.");
    return;
  }

  send_message_box(target, "$C6You were kicked off by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s kicked off", target_name.c_str());
}

static void server_command_ban(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_license_flags(c, License::Flag::BAN_USER);

  size_t space_pos = args.find(' ');
  if (space_pos == string::npos) {
    send_text_message(c, "$C6Incorrect argument count");
    return;
  }

  string identifier = args.substr(space_pos + 1);
  auto target = s->find_client(&identifier);
  if (!target->license) {
    // This should be impossible, but I'll bet it's not actually
    send_text_message(c, "$C6Client not logged in");
    return;
  }

  if (target->license->flags & License::Flag::BAN_USER) {
    send_text_message(c, "$C6You do not have\nsufficient privileges.");
    return;
  }

  uint64_t usecs = stoull(args, nullptr, 0) * 1000000;

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
  send_message_box(target, "$C6You were banned by a moderator.");
  target->should_disconnect = true;
  string target_name = name_for_client(target);
  send_text_message_printf(l, "$C6%s banned", target_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Cheat commands

static void server_command_warp(shared_ptr<Client> c, const std::string& args, bool is_warpall) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  uint32_t floor = stoul(args, nullptr, 0);
  if (c->floor == floor) {
    return;
  }

  size_t limit = floor_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  } else if (floor > limit) {
    send_text_message_printf(c, "$C6Area numbers must\nbe %zu or less.", limit);
    return;
  }

  if (is_warpall) {
    send_warp(l, floor, false);
  } else {
    send_warp(c, floor, true);
  }
}

static void server_command_warpme(shared_ptr<Client> c, const std::string& args) {
  server_command_warp(c, args, false);
}

static void server_command_warpall(shared_ptr<Client> c, const std::string& args) {
  server_command_warp(c, args, true);
}

static void proxy_command_warp(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args, bool is_warpall) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, "$C6You must be in a\ngame to use this\ncommand");
    return;
  }
  uint32_t floor = stoul(args, nullptr, 0);
  send_warp(ses->client_channel, ses->lobby_client_id, floor, !is_warpall);
  if (is_warpall) {
    send_warp(ses->server_channel, ses->lobby_client_id, floor, false);
  }
  ses->floor = floor;
}

static void proxy_command_warpme(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  proxy_command_warp(ses, args, false);
}

static void proxy_command_warpall(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  proxy_command_warp(ses, args, true);
}

static void server_command_next(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  size_t limit = floor_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  }
  send_warp(c, (c->floor + 1) % limit, true);
}

static void proxy_command_next(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, "$C6You must be in a\ngame to use this\ncommand");
    return;
  }

  ses->floor++;
  send_warp(ses->client_channel, ses->lobby_client_id, ses->floor, true);
}

static void server_command_where(shared_ptr<Client> c, const std::string&) {
  send_text_message_printf(c, "$C7Floor: %02" PRIX32 "\nX: %g\nZ: %g", c->floor, c->x, c->z);
}

static void server_command_what(shared_ptr<Client> c, const std::string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);

  if (!episode_has_arpg_semantics(l->episode)) {
    return;
  }

  float min_dist2 = 0.0f;
  shared_ptr<const Lobby::FloorItem> nearest_fi;
  for (const auto& it : l->floor_item_managers.at(c->floor).items) {
    if (!it.second->visible_to_client(c->lobby_client_id)) {
      continue;
    }
    float dx = it.second->x - c->x;
    float dz = it.second->z - c->z;
    float dist2 = (dx * dx) + (dz * dz);
    if (!nearest_fi || (dist2 < min_dist2)) {
      nearest_fi = it.second;
      min_dist2 = dist2;
    }
  }

  if (!nearest_fi) {
    send_text_message(c, "$C4No items are near you");
  } else {
    auto s = c->require_server_state();
    string name = s->describe_item(c->version(), nearest_fi->data, true);
    send_text_message(c, name);
  }
}

static void server_command_song(shared_ptr<Client> c, const std::string& args) {
  check_is_ep3(c, true);

  uint32_t song = stoul(args, nullptr, 0);
  send_ep3_change_music(c->channel, song);
}

static void proxy_command_song(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  int32_t song = stol(args, nullptr, 0);
  if (song < 0) {
    song = -song;
    send_ep3_change_music(ses->server_channel, song);
  }
  send_ep3_change_music(ses->client_channel, song);
}

static void server_command_infinite_hp(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  c->config.toggle_flag(Client::Flag::INFINITE_HP_ENABLED);
  send_text_message_printf(c, "$C6Infinite HP %s", c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED) ? "enabled" : "disabled");
}

static void proxy_command_infinite_hp(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  ses->config.toggle_flag(Client::Flag::INFINITE_HP_ENABLED);
  send_text_message_printf(ses->client_channel, "$C6Infinite HP %s",
      ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED) ? "enabled" : "disabled");
}

static void server_command_infinite_tp(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  c->config.toggle_flag(Client::Flag::INFINITE_TP_ENABLED);
  send_text_message_printf(c, "$C6Infinite TP %s", c->config.check_flag(Client::Flag::INFINITE_TP_ENABLED) ? "enabled" : "disabled");
}

static void proxy_command_infinite_tp(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  ses->config.toggle_flag(Client::Flag::INFINITE_TP_ENABLED);
  send_text_message_printf(ses->client_channel, "$C6Infinite TP %s",
      ses->config.check_flag(Client::Flag::INFINITE_TP_ENABLED) ? "enabled" : "disabled");
}

static void server_command_switch_assist(shared_ptr<Client> c, const std::string&) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  c->config.toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
  send_text_message_printf(c, "$C6Switch assist %s",
      c->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? "enabled" : "disabled");
}

static void proxy_command_switch_assist(shared_ptr<ProxyServer::LinkedSession> ses, const std::string&) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  ses->config.toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
  send_text_message_printf(ses->client_channel, "$C6Switch assist %s",
      ses->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? "enabled" : "disabled");
}

static void server_command_dropmode(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  if (args.empty()) {
    switch (l->drop_mode) {
      case Lobby::DropMode::DISABLED:
        send_text_message(c, "Drop mode: disabled");
        break;
      case Lobby::DropMode::CLIENT:
        send_text_message(c, "Drop mode: client");
        break;
      case Lobby::DropMode::SERVER_SHARED:
        send_text_message(c, "Drop mode: server\nshared");
        break;
      case Lobby::DropMode::SERVER_PRIVATE:
        send_text_message(c, "Drop mode: server\nprivate");
        break;
      case Lobby::DropMode::SERVER_DUPLICATE:
        send_text_message(c, "Drop mode: server\nduplicate");
        break;
    }

  } else {
    check_is_leader(l, c);
    Lobby::DropMode new_mode;
    if ((args == "none") || (args == "disabled")) {
      new_mode = Lobby::DropMode::DISABLED;
    } else if (args == "client") {
      new_mode = Lobby::DropMode::CLIENT;
    } else if ((args == "shared") || (args == "server")) {
      new_mode = Lobby::DropMode::SERVER_SHARED;
    } else if ((args == "private") || (args == "priv")) {
      new_mode = Lobby::DropMode::SERVER_PRIVATE;
    } else if ((args == "duplicate") || (args == "dup")) {
      new_mode = Lobby::DropMode::SERVER_DUPLICATE;
    } else {
      send_text_message(c, "Invalid drop mode");
      return;
    }

    if (!(l->allowed_drop_modes & (1 << static_cast<size_t>(new_mode)))) {
      send_text_message(c, "Drop mode not\nallowed");
      return;
    }

    l->set_drop_mode(new_mode);
    switch (l->drop_mode) {
      case Lobby::DropMode::DISABLED:
        send_text_message(l, "Item drops disabled");
        break;
      case Lobby::DropMode::CLIENT:
        send_text_message(l, "Item drops changed\nto client mode");
        break;
      case Lobby::DropMode::SERVER_SHARED:
        send_text_message(l, "Item drops changed\nto server shared\nmode");
        break;
      case Lobby::DropMode::SERVER_PRIVATE:
        send_text_message(l, "Item drops changed\nto server private\nmode");
        break;
      case Lobby::DropMode::SERVER_DUPLICATE:
        send_text_message(l, "Item drops changed\nto server duplicate\nmode");
        break;
    }
  }
}

static void server_command_item(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_cheats_enabled(l, c);

  ItemData item = s->item_name_index->parse_item_description(c->version(), args);
  item.id = l->generate_item_id(c->lobby_client_id);

  if ((l->drop_mode == Lobby::DropMode::SERVER_PRIVATE) || (l->drop_mode == Lobby::DropMode::SERVER_DUPLICATE)) {
    l->add_item(c->floor, item, c->x, c->z, (1 << c->lobby_client_id));
    send_drop_stacked_item_to_channel(s, c->channel, item, c->floor, c->x, c->z);
  } else {
    l->add_item(c->floor, item, c->x, c->z, 0x00F);
    send_drop_stacked_item_to_lobby(l, item, c->floor, c->x, c->z);
  }

  string name = s->describe_item(c->version(), item, true);
  send_text_message(c, "$C7Item created:\n" + name);
}

static void proxy_command_item(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args) {
  auto s = ses->require_server_state();
  check_cheats_allowed(s, ses);
  if (ses->version() == Version::BB_V4) {
    send_text_message(ses->client_channel, "$C6This command cannot\nbe used on the proxy\nserver in BB games");
    return;
  }
  if (!ses->is_in_game) {
    send_text_message(ses->client_channel, "$C6You must be in\na game to use this\ncommand");
    return;
  }
  if (ses->lobby_client_id != ses->leader_client_id) {
    send_text_message(ses->client_channel, "$C6You must be the\nleader to use this\ncommand");
    return;
  }

  bool set_drop = (!args.empty() && (args[0] == '!'));

  ItemData item = s->item_name_index->parse_item_description(ses->version(), (set_drop ? args.substr(1) : args));
  item.id = random_object<uint32_t>() | 0x80000000;

  if (set_drop) {
    ses->next_drop_item = item;

    string name = s->describe_item(ses->version(), item, true);
    send_text_message(ses->client_channel, "$C7Next drop:\n" + name);

  } else {
    send_drop_stacked_item_to_channel(s, ses->client_channel, item, ses->floor, ses->x, ses->z);
    send_drop_stacked_item_to_channel(s, ses->server_channel, item, ses->floor, ses->x, ses->z);

    string name = s->describe_item(ses->version(), item, true);
    send_text_message(ses->client_channel, "$C7Item created:\n" + name);
  }
}

static void server_command_enable_ep3_battle_debug_menu(shared_ptr<Client> c, const std::string& args) {
  if (!c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message(c, "$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    return;
  }

  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }

  if (!args.empty()) {
    l->ep3_server->override_environment_number = stoul(args, nullptr, 16);
    send_text_message_printf(l, "$C6Override environment\nnumber set to %02hhX", l->ep3_server->override_environment_number);
  } else if (l->ep3_server->override_environment_number == 0xFF) {
    l->ep3_server->override_environment_number = 0x1A;
    send_text_message(l, "$C6Battle setup debug\nmenu enabled");
  } else {
    l->ep3_server->override_environment_number = 0xFF;
    send_text_message(l, "$C6Battle setup debug\nmenu disabled");
  }
}

static void server_command_ep3_infinite_time(shared_ptr<Client> c, const std::string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_is_leader(l, c);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
    send_text_message(c, "$C6Battle is already\nin progress");
    return;
  }

  l->ep3_server->options.behavior_flags ^= Episode3::BehaviorFlag::DISABLE_TIME_LIMITS;
  bool infinite_time_enabled = (l->ep3_server->options.behavior_flags & Episode3::BehaviorFlag::DISABLE_TIME_LIMITS);
  send_text_message(l, infinite_time_enabled ? "$C6Infinite time enabled" : "$C6Infinite time disabled");
}

static void server_command_ep3_set_def_dice_range(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_is_leader(l, c);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
    send_text_message(c, "$C6Battle is already\nin progress");
    return;
  }

  if (args.empty()) {
    l->ep3_server->map_and_rules->rules.def_dice_range = 0;
    send_text_message_printf(l, "$C6DEF dice range\nset to default");
  } else {
    uint8_t min_dice, max_dice;
    auto tokens = split(args, '-');
    if (tokens.size() == 1) {
      min_dice = stoul(tokens[0]);
      max_dice = min_dice;
    } else if (tokens.size() == 2) {
      min_dice = stoul(tokens[0]);
      max_dice = stoul(tokens[1]);
    } else {
      send_text_message(c, "$C6Specify DEF dice\nrange as MIN-MAX");
      return;
    }
    if (min_dice == 0 || min_dice > 9 || max_dice == 0 || max_dice > 9) {
      send_text_message(c, "$C6DEF dice must be\nin range 1-9");
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

static void server_command_ep3_unset_field_character(shared_ptr<Client> c, const std::string& args) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  check_cheats_enabled(l, c);

  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, "$C6Battle has not\nyet begun");
    return;
  }

  size_t index = stoull(args) - 1;
  l->ep3_server->force_destroy_field_character(c->lobby_client_id, index);
}

static void server_command_surrender(shared_ptr<Client> c, const std::string&) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, "$C6Battle has not\nyet started");
    return;
  }
  const string& name = c->character()->disp.name.decode(c->language());
  send_text_message_printf(l, "$C6%s has\nsurrendered", name.c_str());
  for (const auto& watcher_l : l->watcher_lobbies) {
    send_text_message_printf(watcher_l, "$C6%s has\nsurrendered", name.c_str());
  }
  l->ep3_server->force_battle_result(c->lobby_client_id, false);
}

static void server_command_get_ep3_battle_stat(shared_ptr<Client> c, const std::string& args) {
  auto l = c->require_lobby();
  check_is_game(l, true);
  check_is_ep3(c, true);
  if (l->episode != Episode::EP3) {
    throw logic_error("non-Ep3 client in Ep3 game");
  }
  if (!l->ep3_server) {
    send_text_message(c, "$C6Episode 3 server\nis not initialized");
    return;
  }
  if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
    send_text_message(c, "$C6Battle has not\nyet started");
    return;
  }
  if (c->lobby_client_id >= 4) {
    throw logic_error("client ID is too large");
  }
  auto ps = l->ep3_server->player_states[c->lobby_client_id];
  if (!ps) {
    send_text_message(c, "$C6Player is missing");
    return;
  }
  uint8_t team_id = ps->get_team_id();
  if (team_id > 1) {
    throw logic_error("team ID is incorrect");
  }

  if (args == "rank") {
    float score = ps->stats.score(l->ep3_server->get_round_num());
    uint8_t rank = ps->stats.rank_for_score(score);
    const char* rank_name = ps->stats.name_for_rank(rank);
    send_text_message_printf(c, "$C7Score: %g\nRank: %hhu (%s)", score, rank, rank_name);
  } else if (args == "duration") {
    string s = format_duration(now() - l->ep3_server->battle_start_usecs);
    send_text_message_printf(c, "$C7Duration: %s", s.c_str());
  } else if (args == "fcs-destroyed") {
    send_text_message_printf(c, "$C7Team FCs destroyed:\n%" PRIu32, l->ep3_server->team_num_ally_fcs_destroyed[team_id]);
  } else if (args == "cards-destroyed") {
    send_text_message_printf(c, "$C7Team cards destroyed:\n%" PRIu32, l->ep3_server->team_num_cards_destroyed[team_id]);
  } else if (args == "damage-given") {
    send_text_message_printf(c, "$C7Damage given: %hu", ps->stats.damage_given.load());
  } else if (args == "damage-taken") {
    send_text_message_printf(c, "$C7Damage taken: %hu", ps->stats.damage_taken.load());
  } else if (args == "opp-cards-destroyed") {
    send_text_message_printf(c, "$C7Opp. cards destroyed:\n%hu", ps->stats.num_opponent_cards_destroyed.load());
  } else if (args == "own-cards-destroyed") {
    send_text_message_printf(c, "$C7Own cards destroyed:\n%hu", ps->stats.num_owned_cards_destroyed.load());
  } else if (args == "move-distance") {
    send_text_message_printf(c, "$C7Move distance: %hu", ps->stats.total_move_distance.load());
  } else if (args == "cards-set") {
    send_text_message_printf(c, "$C7Cards set: %hu", ps->stats.num_cards_set.load());
  } else if (args == "fcs-set") {
    send_text_message_printf(c, "$C7FC cards set: %hu", ps->stats.num_item_or_creature_cards_set.load());
  } else if (args == "attack-actions-set") {
    send_text_message_printf(c, "$C7Attack actions set:\n%hu", ps->stats.num_attack_actions_set.load());
  } else if (args == "techs-set") {
    send_text_message_printf(c, "$C7Techs set: %hu", ps->stats.num_tech_cards_set.load());
  } else if (args == "assists-set") {
    send_text_message_printf(c, "$C7Assists set: %hu", ps->stats.num_assist_cards_set.load());
  } else if (args == "defenses-self") {
    send_text_message_printf(c, "$C7Defenses on self:\n%hu", ps->stats.defense_actions_set_on_self.load());
  } else if (args == "defenses-ally") {
    send_text_message_printf(c, "$C7Defenses on ally:\n%hu", ps->stats.defense_actions_set_on_ally.load());
  } else if (args == "cards-drawn") {
    send_text_message_printf(c, "$C7Cards drawn: %hu", ps->stats.num_cards_drawn.load());
  } else if (args == "max-attack-damage") {
    send_text_message_printf(c, "$C7Maximum attack damage:\n%hu", ps->stats.max_attack_damage.load());
  } else if (args == "max-combo") {
    send_text_message_printf(c, "$C7Longest combo: %hu", ps->stats.max_attack_combo_size.load());
  } else if (args == "attacks-given") {
    send_text_message_printf(c, "$C7Attacks given: %hu", ps->stats.num_attacks_given.load());
  } else if (args == "attacks-taken") {
    send_text_message_printf(c, "$C7Attacks taken: %hu", ps->stats.num_attacks_taken.load());
  } else if (args == "sc-damage") {
    send_text_message_printf(c, "$C7SC damage taken: %hu", ps->stats.sc_damage_taken.load());
  } else if (args == "damage-defended") {
    send_text_message_printf(c, "$C7Damage defended: %hu", ps->stats.action_card_negated_damage.load());
  } else {
    send_text_message(c, "$C6Unknown statistic");
  }
}

////////////////////////////////////////////////////////////////////////////////

typedef void (*server_handler_t)(shared_ptr<Client> c, const std::string& args);
typedef void (*proxy_handler_t)(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& args);
struct ChatCommandDefinition {
  server_handler_t server_handler;
  proxy_handler_t proxy_handler;
};

static const unordered_map<string, ChatCommandDefinition> chat_commands({
    {"$allevent", {server_command_lobby_event_all, nullptr}},
    {"$ann", {server_command_announce, nullptr}},
    {"$arrow", {server_command_arrow, proxy_command_arrow}},
    {"$auction", {server_command_auction, proxy_command_auction}},
    {"$ax", {server_command_ax, nullptr}},
    {"$ban", {server_command_ban, nullptr}},
    {"$bank", {server_command_change_bank, nullptr}},
    {"$bbchar", {server_command_bbchar, nullptr}},
    {"$cheat", {server_command_cheat, nullptr}},
    {"$debug", {server_command_debug, nullptr}},
    {"$defrange", {server_command_ep3_set_def_dice_range, nullptr}},
    {"$dropmode", {server_command_dropmode, nullptr}},
    {"$edit", {server_command_edit, nullptr}},
    {"$ep3battledebug", {server_command_enable_ep3_battle_debug_menu, nullptr}},
    {"$event", {server_command_lobby_event, proxy_command_lobby_event}},
    {"$exit", {server_command_exit, proxy_command_exit}},
    {"$gc", {server_command_get_self_card, proxy_command_get_player_card}},
    {"$infhp", {server_command_infinite_hp, proxy_command_infinite_hp}},
    {"$inftime", {server_command_ep3_infinite_time, nullptr}},
    {"$inftp", {server_command_infinite_tp, proxy_command_infinite_tp}},
    {"$item", {server_command_item, proxy_command_item}},
    {"$i", {server_command_item, proxy_command_item}},
    {"$kick", {server_command_kick, nullptr}},
    {"$li", {server_command_lobby_info, proxy_command_lobby_info}},
    {"$ln", {server_command_lobby_type, proxy_command_lobby_type}},
    {"$loadchar", {server_command_loadchar, nullptr}},
    {"$matcount", {server_command_show_material_counts, nullptr}},
    {"$maxlevel", {server_command_max_level, nullptr}},
    {"$meseta", {server_command_meseta, nullptr}},
    {"$minlevel", {server_command_min_level, nullptr}},
    {"$next", {server_command_next, proxy_command_next}},
    {"$password", {server_command_password, nullptr}},
    {"$patch", {server_command_patch, proxy_command_patch}},
    {"$persist", {server_command_persist, nullptr}},
    {"$ping", {server_command_ping, proxy_command_ping}},
    {"$playrec", {server_command_playrec, nullptr}},
    {"$qcall", {server_command_qcall, proxy_command_qcall}},
    {"$qcheck", {server_command_qcheck, nullptr}},
    {"$qclear", {server_command_qclear, proxy_command_qclear}},
    {"$qset", {server_command_qset, proxy_command_qset}},
    {"$qsync", {server_command_qsync, proxy_command_qsync}},
    {"$quest", {server_command_quest, nullptr}},
    {"$rand", {server_command_rand, proxy_command_rand}},
    {"$save", {server_command_save, nullptr}},
    {"$savechar", {server_command_savechar, nullptr}},
    {"$saverec", {server_command_saverec, nullptr}},
    {"$sc", {server_command_send_client, proxy_command_send_client}},
    {"$secid", {server_command_secid, proxy_command_secid}},
    {"$si", {server_command_server_info, nullptr}},
    {"$silence", {server_command_silence, nullptr}},
    {"$song", {server_command_song, proxy_command_song}},
    {"$spec", {server_command_toggle_spectator_flag, nullptr}},
    {"$ss", {nullptr, proxy_command_send_server}},
    {"$stat", {server_command_get_ep3_battle_stat, nullptr}},
    {"$surrender", {server_command_surrender, nullptr}},
    {"$swa", {server_command_switch_assist, proxy_command_switch_assist}},
    {"$unset", {server_command_ep3_unset_field_character, nullptr}},
    {"$warp", {server_command_warpme, proxy_command_warpme}},
    {"$warpme", {server_command_warpme, proxy_command_warpme}},
    {"$warpall", {server_command_warpall, proxy_command_warpall}},
    {"$what", {server_command_what, nullptr}},
    {"$where", {server_command_where, nullptr}},
});

struct SplitCommand {
  string name;
  string args;

  SplitCommand(const string& text) {
    size_t space_pos = text.find(' ');
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
void on_chat_command(std::shared_ptr<Client> c, const std::string& text) {
  SplitCommand cmd(text);
  if (!cmd.name.empty() && cmd.name[0] == '@') {
    cmd.name[0] = '$';
  }

  const ChatCommandDefinition* def = nullptr;
  try {
    def = &chat_commands.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(c, "$C6Unknown command");
    return;
  }

  if (!def->server_handler) {
    send_text_message(c, "$C6Command not available\non game server");
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

void on_chat_command(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& text) {
  SplitCommand cmd(text);

  const ChatCommandDefinition* def = nullptr;
  try {
    def = &chat_commands.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(ses->client_channel, "$C6Unknown command");
    return;
  }

  if (!def->proxy_handler) {
    send_text_message(ses->client_channel, "$C6Command not available\non proxy server");
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
