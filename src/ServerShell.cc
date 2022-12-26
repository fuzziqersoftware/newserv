#include "ServerShell.hh"

#include <event2/event.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Strings.hh>

#include "ServerState.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"

using namespace std;



ServerShell::ServerShell(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
  : Shell(base), state(state) { }

void ServerShell::print_prompt() {
  fwritex(stdout, Shell::PROMPT);
  fflush(stdout);
}

shared_ptr<ProxyServer::LinkedSession> ServerShell::get_proxy_session() {
  if (!this->state->proxy_server.get()) {
    throw runtime_error("the proxy server is disabled");
  }
  return this->state->proxy_server->get_session();
}

static void set_boolean(bool* target, const string& args) {
  if (args == "on") {
    *target = true;
  } else if (args == "off") {
    *target = false;
  } else {
    throw invalid_argument("argument must be \"on\" or \"off\"");
  }
}

static string get_quoted_string(string& s) {
  string ret;
  char end_char = (s.at(0) == '\"') ? '\"' : ' ';
  size_t z = (s.at(0) == '\"') ? 1 : 0;
  for (; (z < s.size()) && (s[z] != end_char); z++) {
    if (s[z] == '\\') {
      if (z + 1 < s.size()) {
        ret.push_back(s[z + 1]);
      } else {
        throw runtime_error("incomplete escape sequence");
      }
    } else {
      ret.push_back(s[z]);
    }
  }
  if (end_char != ' ') {
    if (z >= s.size()) {
      throw runtime_error("unterminated quoted string");
    }
    s = s.substr(skip_whitespace(s, z + 1));
  } else {
    s = s.substr(skip_whitespace(s, z));
  }
  return ret;
}

void ServerShell::execute_command(const string& command) {
  // find the entry in the command table and run the command
  size_t command_end = skip_non_whitespace(command, 0);
  size_t args_begin = skip_whitespace(command, command_end);
  string command_name = command.substr(0, command_end);
  string command_args = command.substr(args_begin);

  if (command_name == "exit") {
    throw exit_shell();

  } else if (command_name == "help") {
    fprintf(stderr, "\
General commands:\n\
  help\n\
    You\'re reading it now.\n\
\n\
Server commands:\n\
  exit (or ctrl+d)\n\
    Shut down the server.\n\
  reload <item> ...\n\
    Reload data. <item> can be licenses, quests, functions, programs, or ep3.\n\
    Reloading will not affect items that are in use; for example, if a client\'s\n\
    license is deleted by reloading, they will not be disconnected immediately.\n\
  add-license <parameters>\n\
    Add a license to the server. <parameters> is some subset of the following:\n\
      bb-username=<username> (BB username)\n\
      bb-password=<password> (BB password)\n\
      gc-password=<password> (GC password)\n\
      access-key=<access-key> (DC/GC/PC access key)\n\
      serial=<serial-number> (decimal serial number; required for all licenses)\n\
      privileges=<privilege-mask> (can be normal, mod, admin, root, or numeric)\n\
  delete-license <serial-number>\n\
    Delete a license from the server.\n\
  list-licenses\n\
    List all licenses registered on the server.\n\
  set-allow-unregistered-users <true/false>\n\
    Enable or disable allowing unregistered users on the server. Disabling this\n\
    does not disconnect unregistered users who are already connected.\n\
  set-event <event>\n\
    Set the event in all lobbies, and in the main menu before joining a lobby.\n\
    <event> can be none, xmas, val, easter, hallo, sonic, newyear, summer,\n\
    white, wedding, fall, s-spring, s-summer, or spring.\n\
  set-ep3-menu-song <song-num>\n\
    Set the song that plays in the main menu for Episode III clients. If an\n\
    event is also set, the event's visuals appear but this song still plays.\n\
    Song IDs are 0 through 51; the default song is -1.\n\
  announce <message>\n\
    Send an announcement message to all players.\n\
  create-tournament \"Tournament Name\" \"Map Name\" <num-teams> [options...]\n\
    Create an Episode 3 tournament. The quotes are required arount the\n\
    tournament and map names, unless the names contain no spaces.\n\
    Rules options:\n\
      2v2: Set team size to 2 players (default is 1 without this option)\n\
      dice=MIN-MAX: Set minimum and maximum dice rolls\n\
      overall-time-limit=N: Set battle time limit (in multiples of 5 minutes)\n\
      phase-time-limit=N: Set phase time limit (in seconds)\n\
      allowed-cards=ALL/N/NR/NRS: Set rarities of allowed cards\n\
      deck-shuffle=ON/OFF: Enable/disable deck shuffle\n\
      deck-loop=ON/OFF: Enable/disable deck loop\n\
      hp=N: Set Story Character initial HP\n\
      hp-type=TEAM/PLAYER/COMMON: Set team HP type\n\
      allow-assists=ON/OFF: Enable/disable assist cards\n\
      dialogue=ON/OFF: Enable/disable dialogue\n\
      dice-exchange=ATK/DEF/NONE: Set dice exchange mode\n\
      dice-boost=ON/OFF: Enable/disable dice boost\n\
  delete-tournament \"Tournament Name\"\n\
    Delete a tournament. The quotes are required unless the tournament name\n\
    contains no spaces.\n\
  list-tournaments\n\
    List the names and numbers of all existing tournaments.\n\
  start-tournament \"Tournament Name\"\n\
    End registration for a tournament and allow matches to begin. The quotes\n\
    are required unless the tournament name contains no spaces.\n\
  tournament-state \"Tournament Name\"\n\
    Show the current state of a tournament. The quotes are required unless the\n\
    tournament name contains no spaces.\n\
\n\
Proxy commands (these will only work when exactly one client is connected):\n\
  sc <data>\n\
    Send a command to the client.\n\
  ss <data>\n\
    Send a command to the server.\n\
  chat <text>\n\
    Send a chat message to the server.\n\
  dchat <data>\n\
    Send a chat message to the server with arbitrary data in it.\n\
  info-board <text>\n\
    Set your info board contents. This will affect the current session only,\n\
    and will not be saved for future sessions.\n\
  info-board-data <data>\n\
    Set your info board contents with arbitrary data. Like the above, affects\n\
    the current session only.\n\
  marker <color-id>\n\
    Change your lobby marker color.\n\
  warp <area-id>\n\
    Send yourself to a specific area.\n\
  set-override-section-id [section-id]\n\
    Override the section ID for games you create or join. This affects the\n\
    active drop chart if you are the leader of the game and the server doesn't\n\
    override drops entirely. If no argument is given, clears the override.\n\
  set-override-event [event]\n\
    Override the lobby event for all lobbies and games you join. This applies\n\
    only to you; other players do not see this override.  If no argument is\n\
    given, clears the override.\n\
  set-override-lobby-number [number]\n\
    Override the lobby type for all lobbies you join. This applies only to you;\n\
    other players do not see this override. If no argument is given, clears the\n\
    override.\n\
  set-chat-filter <on|off>\n\
    Enable or disable chat filtering (enabled by default). Chat filtering\n\
    applies newserv\'s standard character replacements to chat messages; for\n\
    example, $ becomes a tab character and # becomes a newline.\n\
  set-infinite-hp <on|off>\n\
  set-infinite-tp <on|off>\n\
    Enable or disable infinite HP or TP. When infinite HP is enabled, attacks\n\
    that would kill you in one hit will still do so.\n\
  set-switch-assist <on|off>\n\
    Enable or disable switch assist. When switch assist is on, the proxy will\n\
    remember the last \"enable switch\" command that you send, and will send it\n\
    to you and the server when you step on another switch. Using this, you can\n\
    unlock any doors that require two players to stand on switches by touching\n\
    both switches yourself. With this, all online maps can be completed solo.\n\
  set-save-files <on|off>\n\
    Enable or disable saving of game files (disabled by default). When this is\n\
    on, any file that the remote server sends to the client will be saved to\n\
    the current directory. This includes data like quests, Episode 3 card\n\
    definitions, and GBA games.\n\
  set-block-function-calls [return-value]\n\
    Enable blocking of function calls from the server. When enabled, the proxy\n\
    responds as if the function was called (with the given return value), but\n\
    does not send the code to the client. To stop blocking function calls, omit\n\
    the return value.\n\
  set-next-item <code>\n\
    Set the next item to be dropped as if the client had run the $item command.\n\
  close-idle-sessions\n\
    Closes all sessions that don\'t have a client and server connected.\n\
");



  // SERVER COMMANDS

  } else if (command_name == "reload") {
    auto types = split(command_args, ' ');
    if (types.empty()) {
      throw invalid_argument("no data type given");
    }
    for (const string& type : types) {
      if (type == "licenses") {
        shared_ptr<LicenseManager> lm(new LicenseManager("system/licenses.nsi"));
        this->state->license_manager = lm;
      } else if (type == "quests") {
        shared_ptr<QuestIndex> qi(new QuestIndex("system/quests"));
        this->state->quest_index = qi;
      } else if (type == "functions") {
        shared_ptr<FunctionCodeIndex> fci(new FunctionCodeIndex("system/ppc"));
        this->state->function_code_index = fci;
      } else if (type == "programs") {
        shared_ptr<DOLFileIndex> dfi(new DOLFileIndex("system/dol"));
        this->state->dol_file_index = dfi;
      } else if (type == "programs") {
        shared_ptr<Episode3::DataIndex> data_index(new Episode3::DataIndex(
            "system/ep3", this->state->ep3_behavior_flags));
        this->state->ep3_data_index = data_index;
      } else {
        throw invalid_argument("incorrect data type");
      }
    }

  } else if (command_name == "add-license") {
    shared_ptr<License> l(new License());

    for (const string& token : split(command_args, ' ')) {
      if (starts_with(token, "bb-username=")) {
        if (token.size() >= 32) {
          throw invalid_argument("username too long");
        }
        l->username = token.substr(12);

      } else if (starts_with(token, "bb-password=")) {
        if (token.size() >= 32) {
          throw invalid_argument("bb-password too long");
        }
        l->bb_password = token.substr(12);

      } else if (starts_with(token, "gc-password=")) {
        if (token.size() > 20) {
          throw invalid_argument("gc-password too long");
        }
        l->gc_password = token.substr(12);

      } else if (starts_with(token, "access-key=")) {
        if (token.size() > 23) {
          throw invalid_argument("access-key is too long");
        }
        l->access_key = token.substr(11);

      } else if (starts_with(token, "serial=")) {
        l->serial_number = stoul(token.substr(7));

      } else if (starts_with(token, "privileges=")) {
        string mask = token.substr(11);
        if (mask == "normal") {
          l->privileges = 0;
        } else if (mask == "mod") {
          l->privileges = Privilege::MODERATOR;
        } else if (mask == "admin") {
          l->privileges = Privilege::ADMINISTRATOR;
        } else if (mask == "root") {
          l->privileges = Privilege::ROOT;
        } else {
          l->privileges = stoul(mask);
        }

      } else {
        throw invalid_argument("incorrect field");
      }
    }

    if (!l->serial_number) {
      throw invalid_argument("license does not contain serial number");
    }

    this->state->license_manager->add(l);
    fprintf(stderr, "license added\n");

  } else if (command_name == "delete-license") {
    uint32_t serial_number = stoul(command_args);
    this->state->license_manager->remove(serial_number);
    fprintf(stderr, "license deleted\n");

  } else if (command_name == "list-licenses") {
    for (const auto& l : this->state->license_manager->snapshot()) {
      string s = l.str();
      fprintf(stderr, "%s\n", s.c_str());
    }

  } else if (command_name == "set-allow-unregistered-users") {
    if (command_args == "true") {
      this->state->allow_unregistered_users = true;
    } else if (command_args == "false") {
      this->state->allow_unregistered_users = false;
    } else {
      throw invalid_argument("argument must be true or false");
    }
    fprintf(stderr, "unregistered users are now %s\n",
        this->state->allow_unregistered_users ? "allowed" : "disallowed");

  } else if (command_name == "set-event") {
    uint8_t event_id = event_for_name(command_args);
    if (event_id == 0xFF) {
      throw invalid_argument("invalid event");
    }

    this->state->pre_lobby_event = event_id;
    for (const auto& l : this->state->all_lobbies()) {
      l->event = event_id;
    }
    send_change_event(this->state, event_id);

  } else if (command_name == "set-ep3-menu-song") {
    this->state->ep3_menu_song = stoul(command_args, nullptr, 0);

  } else if (command_name == "announce") {
    u16string message16 = decode_sjis(command_args);
    send_text_message(this->state, message16.c_str());

  } else if (command_name == "create-tournament") {
    string name = get_quoted_string(command_args);
    string map_name = get_quoted_string(command_args);
    auto map = this->state->ep3_data_index->definition_for_map_name(map_name);
    uint32_t num_teams = stoul(get_quoted_string(command_args), nullptr, 0);
    Episode3::Rules rules;
    rules.set_defaults();
    bool is_2v2 = false;
    if (!command_args.empty()) {
      auto tokens = split(command_args, ' ');
      for (auto& token : tokens) {
        token = tolower(token);
        if (token == "2v2") {
          is_2v2 = true;
        } else if (starts_with(token, "dice=")) {
          auto subtokens = split(token.substr(5), '-');
          if (subtokens.size() != 2) {
            throw runtime_error("dice option must be of the form dice=X-Y");
          }
          rules.min_dice = stoul(subtokens[0]);
          rules.max_dice = stoul(subtokens[0]);
        } else if (starts_with(token, "overall-time-limit=")) {
          uint32_t limit = stoul(token.substr(19));
          if (limit > 600) {
            throw runtime_error("overall-time-limit must be 600 or fewer minutes");
          }
          if (limit % 5) {
            throw runtime_error("overall-time-limit must be a multiple of 5 minutes");
          }
          rules.overall_time_limit = limit;
        } else if (starts_with(token, "phase-time-limit=")) {
          rules.phase_time_limit = stoul(token.substr(17));
        } else if (starts_with(token, "hp=")) {
          rules.char_hp = stoul(token.substr(3));
        } else if (token == "allowed-cards=all") {
          rules.allowed_cards = Episode3::AllowedCards::ALL;
        } else if (token == "allowed-cards=n") {
          rules.allowed_cards = Episode3::AllowedCards::N_ONLY;
        } else if (token == "allowed-cards=nr") {
          rules.allowed_cards = Episode3::AllowedCards::N_R_ONLY;
        } else if (token == "allowed-cards=nrs") {
          rules.allowed_cards = Episode3::AllowedCards::N_R_S_ONLY;
        } else if (token == "deck-shuffle=on") {
          rules.disable_deck_shuffle = 0;
        } else if (token == "deck-shuffle=off") {
          rules.disable_deck_shuffle = 1;
        } else if (token == "deck-loop=on") {
          rules.disable_deck_loop = 0;
        } else if (token == "deck-loop=off") {
          rules.disable_deck_loop = 1;
        } else if (token == "allow-assists=on") {
          rules.no_assist_cards = 0;
        } else if (token == "allow-assists=off") {
          rules.no_assist_cards = 1;
        } else if (token == "dialogue=on") {
          rules.disable_dialogue = 0;
        } else if (token == "dialogue=off") {
          rules.disable_dialogue = 1;
        } else if (token == "dice-boost=on") {
          rules.disable_dice_boost = 0;
        } else if (token == "dice-boost=off") {
          rules.disable_dice_boost = 1;
        } else if (token == "hp-type=player") {
          rules.hp_type = Episode3::HPType::DEFEAT_PLAYER;
        } else if (token == "hp-type=team") {
          rules.hp_type = Episode3::HPType::DEFEAT_TEAM;
        } else if (token == "hp-type=common") {
          rules.hp_type = Episode3::HPType::COMMON_HP;
        } else if (token == "dice-exchange=atk") {
          rules.dice_exchange_mode = Episode3::DiceExchangeMode::HIGH_ATK;
        } else if (token == "dice-exchange=def") {
          rules.dice_exchange_mode = Episode3::DiceExchangeMode::HIGH_DEF;
        } else if (token == "dice-exchange=none") {
          rules.dice_exchange_mode = Episode3::DiceExchangeMode::NONE;
        } else {
          throw runtime_error("invalid rules option: " + token);
        }
      }
    }
    if (rules.check_and_reset_invalid_fields()) {
      fprintf(stderr, "warning: some rules were invalid and reset to defaults\n");
    }
    auto tourn = this->state->ep3_tournament_index->create_tournament(
        name, map, rules, num_teams, is_2v2);
    this->state->ep3_tournament_index->save();
    fprintf(stderr, "created tournament %02hhX\n", tourn->get_number());

  } else if (command_name == "delete-tournament") {
    string name = get_quoted_string(command_args);
    auto tourn = this->state->ep3_tournament_index->get_tournament(name);
    if (tourn) {
      this->state->ep3_tournament_index->delete_tournament(tourn->get_number());
      this->state->ep3_tournament_index->save();
      fprintf(stderr, "tournament deleted\n");
    } else {
      fprintf(stderr, "no such tournament exists\n");
    }

  } else if (command_name == "list-tournaments") {
    for (const auto& tourn : this->state->ep3_tournament_index->all_tournaments()) {
      fprintf(stderr, "  %s\n", tourn->get_name().c_str());
    }

  } else if (command_name == "start-tournament") {
    string name = get_quoted_string(command_args);
    auto tourn = this->state->ep3_tournament_index->get_tournament(name);
    if (tourn) {
      tourn->start();
      this->state->ep3_tournament_index->save();
      send_ep3_text_message_printf(this->state, "$C7The tournament\n$C6%s$C7\nhas begun", tourn->get_name().c_str());
      fprintf(stderr, "tournament started\n");
    } else {
      fprintf(stderr, "no such tournament exists\n");
    }

  } else if (command_name == "tournament-status") {
    string name = get_quoted_string(command_args);
    auto tourn = this->state->ep3_tournament_index->get_tournament(name);
    if (tourn) {
      tourn->print_bracket(stderr);
    } else {
      fprintf(stderr, "no such tournament exists\n");
    }



  // PROXY COMMANDS

  } else if ((command_name == "sc") || (command_name == "ss")) {
    string data = parse_data_string(command_args);
    if (data.size() & 3) {
      throw invalid_argument("data size is not a multiple of 4");
    }
    if (data.size() == 0) {
      throw invalid_argument("no data given");
    }

    shared_ptr<ProxyServer::LinkedSession> proxy_session;
    try {
      proxy_session = this->get_proxy_session();
    } catch (const exception&) { }

    if (proxy_session.get()) {
      if (command_name[1] == 's') {
        proxy_session->server_channel.send(data);
      } else {
        proxy_session->client_channel.send(data);
      }

    } else {
      if (command_name [1] == 's') {
        throw runtime_error("cannot send to server in non-proxy session");
      }

      auto c = this->state->game_server->get_client();
      send_command_with_header(c->channel, data.data(), data.size());
    }

  } else if ((command_name == "chat") || (command_name == "dchat")) {
    auto session = this->get_proxy_session();

    string data(8, '\0');
    data.push_back('\x09');
    data.push_back('E');
    if (command_name == "dchat") {
      data += parse_data_string(command_args);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));

    session->server_channel.send(0x06, 0x00, data);

  } else if (command_name == "marker") {
    auto session = this->get_proxy_session();
    session->server_channel.send(0x89, stoul(command_args));

  } else if (command_name == "warp") {
    auto session = this->get_proxy_session();

    uint8_t area = stoul(command_args);
    send_warp(session->client_channel, session->lobby_client_id, area);
    send_warp(session->server_channel, session->lobby_client_id, area);

  } else if ((command_name == "info-board") || (command_name == "info-board-data")) {
    auto session = this->get_proxy_session();

    string data;
    if (command_name == "info-board-data") {
      data += parse_data_string(command_args);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));

    session->server_channel.send(0xD9, 0x00, data);

  } else if (command_name == "set-override-section-id") {
    auto session = this->get_proxy_session();
    if (command_args.empty()) {
      session->options.override_section_id = -1;
    } else {
      session->options.override_section_id = section_id_for_name(command_args);
    }

  } else if (command_name == "set-override-event") {
    auto session = this->get_proxy_session();
    if (command_args.empty()) {
      session->options.override_lobby_event = -1;
    } else {
      session->options.override_lobby_event = event_for_name(command_args);
      if ((session->version != GameVersion::DC) &&
          (session->version != GameVersion::PC) && (
          !((session->version == GameVersion::GC) &&
            (session->newserv_client_config.cfg.flags & Client::Flag::IS_TRIAL_EDITION)))) {
        session->client_channel.send(0xDA, session->options.override_lobby_event);
      }
    }

  } else if (command_name == "set-override-lobby-number") {
    auto session = this->get_proxy_session();
    if (command_args.empty()) {
      session->options.override_lobby_number = -1;
    } else {
      session->options.override_lobby_number = lobby_type_for_name(command_args);
    }

  } else if (command_name == "set-chat-filter") {
    auto session = this->get_proxy_session();
    set_boolean(&session->options.enable_chat_filter, command_args);

  } else if (command_name == "set-infinite-hp") {
    auto session = this->get_proxy_session();
    set_boolean(&session->options.infinite_hp, command_args);

  } else if (command_name == "set-infinite-tp") {
    auto session = this->get_proxy_session();
    set_boolean(&session->options.infinite_tp, command_args);

  } else if (command_name == "set-switch-assist") {
    auto session = this->get_proxy_session();
    set_boolean(&session->options.switch_assist, command_args);

  } else if (command_name == "set-save-files" && this->state->proxy_allow_save_files) {
    auto session = this->get_proxy_session();
    set_boolean(&session->options.save_files, command_args);

  } else if (command_name == "set-block-function-calls") {
    auto session = this->get_proxy_session();
    if (command_args.empty()) {
      session->options.function_call_return_value = -1;
    } else {
      session->options.function_call_return_value = stoul(command_args);
    }

  } else if (command_name == "set-next-item") {
    auto session = this->get_proxy_session();

    if (session->version == GameVersion::BB) {
      throw runtime_error("proxy session is BB");
    }
    if (!session->is_in_game) {
      throw runtime_error("proxy session is not in a game");
    }
    if (session->lobby_client_id != session->leader_client_id) {
      throw runtime_error("proxy session is not game leader");
    }

    string data = parse_data_string(command_args);
    if (data.size() < 2) {
      throw runtime_error("data too short");
    }
    if (data.size() > 16) {
      throw runtime_error("data too long");
    }

    session->next_drop_item.clear();
    if (data.size() <= 12) {
      memcpy(&session->next_drop_item.data.data1, data.data(), data.size());
    } else {
      memcpy(&session->next_drop_item.data.data1, data.data(), 12);
      memcpy(&session->next_drop_item.data.data2, data.data() + 12, data.size() - 12);
    }

    string name = name_for_item(session->next_drop_item.data, true);
    send_text_message(session->client_channel, u"$C7Next drop:\n" + decode_sjis(name));

  } else if (command_name == "close-idle-sessions") {
    size_t count = this->state->proxy_server->delete_disconnected_sessions();
    fprintf(stderr, "%zu sessions closed\n", count);

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
