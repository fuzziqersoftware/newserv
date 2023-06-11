#include "ServerShell.hh"

#include <event2/event.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ReceiveCommands.hh"
#include "SendCommands.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

using namespace std;

ServerShell::ServerShell(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
    : Shell(base),
      state(state) {}

void ServerShell::print_prompt() {
  fwritex(stdout, Shell::PROMPT);
  fflush(stdout);
}

shared_ptr<ProxyServer::LinkedSession> ServerShell::get_proxy_session(
    const string& name) {
  if (!this->state->proxy_server.get()) {
    throw runtime_error("the proxy server is disabled");
  }
  return name.empty()
      ? this->state->proxy_server->get_session()
      : this->state->proxy_server->get_session_by_name(name);
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
  // Find the entry in the command table and run the command
  size_t command_end = skip_non_whitespace(command, 0);
  size_t args_begin = skip_whitespace(command, command_end);
  string command_name = command.substr(0, command_end);
  string command_args = command.substr(args_begin);

  string session_name;
  if (command_name == "on") {
    size_t session_name_end = skip_non_whitespace(command_args, 0);
    size_t command_begin = skip_whitespace(command_args, session_name_end);
    command_end = skip_non_whitespace(command_args, command_begin);
    args_begin = skip_whitespace(command_args, command_end);
    session_name = command_args.substr(0, session_name_end);
    command_name = command_args.substr(command_begin, command_end - command_begin);
    command_args = command_args.substr(args_begin);
  }

  if (command_name == "exit") {
    throw exit_shell();

  } else if (command_name == "help") {
    fprintf(stderr, "\
General commands:\n\
  help\n\
    You\'re reading it now.\n\
  exit (or ctrl+d)\n\
    Shut down the server.\n\
  on SESSION COMMAND [ARGS...]\n\
    Run a command on a specific game server client or proxy server session.\n\
    Without this prefix, commands that affect a single client or session will\n\
    work only if there's exactly one connected client or open session. SESSION\n\
    may be a client ID (e.g. C-3), a player name, a license serial number, or\n\
    a BB account username. For proxy commands, SESSION should be the session\n\
    ID, which generally is the same as the player\'s serial number and appears\n\
    after \"LinkedSession:\" in the log output.\n\
\n\
Server commands:\n\
  reload ITEM [ITEM...]\n\
    Reload various parts of the server configuration. ITEMs can be:\n\
      licenses - reload the license index file\n\
      patches - reindex the PC and BB patch directories\n\
      battle-params - reload the enemy stats files\n\
      level-table - reload the level-up tables\n\
      item-tables - reload the item generation tables\n\
      ep3 - reload the Episode 3 card definitions and maps\n\
      quests - reindex all quests\n\
      functions - recompile all client-side functions\n\
      dol-files - reindex all DOL files\n\
      config - reload some fields from config.json\n\
    Reloading will not affect items that are in use; for example, if an Episode\n\
    3 battle is in progress, it will continue to use the previous map and card\n\
    definitions. Similarly, BB clients are not forced to disconnect or reload\n\
    the battle parameters, so if these are changed without restarting, clients\n\
    may see (for example) EXP messages inconsistent with the amounts of EXP\n\
    actually received.\n\
  add-license PARAMETERS...\n\
    Add a license to the server. <parameters> is some subset of the following:\n\
      bb-username=<username> (BB username)\n\
      bb-password=<password> (BB password)\n\
      gc-password=<password> (GC password)\n\
      access-key=<access-key> (DC/GC/PC access key)\n\
      serial=<serial-number> (decimal serial number; required for all licenses)\n\
      privileges=<privilege-mask> (can be normal, mod, admin, root, or numeric)\n\
  update-license SERIAL-NUMBER PARAMETERS...\n\
    Update an existing license. <serial-number> specifies which license to\n\
    update. The options in <parameters> are the same as for the add-license\n\
    command.\n\
  delete-license SERIAL-NUMBER\n\
    Delete a license from the server.\n\
  list-licenses\n\
    List all licenses registered on the server.\n\
  set-allow-unregistered-users on|off\n\
    Enable or disable allowing unregistered users on the server. Disabling this\n\
    does not disconnect unregistered users who are already connected.\n\
  set-event EVENT\n\
    Set the event in all lobbies, and in the main menu before joining a lobby.\n\
    EVENT can be none, xmas, val, easter, hallo, sonic, newyear, summer, white,\n\
    wedding, fall, s-spring, s-summer, or spring.\n\
  set-ep3-menu-song SONG-NUM\n\
    Set the song that plays in the main menu for Episode III clients. If an\n\
    event is also set, the event's visuals appear but this song still plays.\n\
    Song numbers are 0 through 51; the default song is -1.\n\
  announce MESSAGE\n\
    Send an announcement message to all players.\n\
  create-tournament TOURNAMENT-NAME MAP-NAME NUM-TEAMS [OPTIONS...]\n\
    Create an Episode 3 tournament. Quotes are required around the tournament\n\
    and map names, unless the names contain no spaces.\n\
    OPTIONS may include:\n\
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
  delete-tournament TOURNAMENT-NAME\n\
    Delete a tournament. Quotes are required around the tournament name unless\n\
    the name contains no spaces.\n\
  list-tournaments\n\
    List the names and numbers of all existing tournaments.\n\
  start-tournament TOURNAMENT-NAME\n\
    End registration for a tournament and allow matches to begin. Quotes are\n\
    required around the tournament name unless the name contains no spaces.\n\
  tournament-state TOURNAMENT-NAME\n\
    Show the current state of a tournament. Quotes are required around the\n\
    tournament name unless the name contains no spaces.\n\
\n\
Proxy session commands:\n\
  sc DATA\n\
    Send a command to the client. This command also can be used to send data to\n\
    a client on the game server.\n\
  ss DATA\n\
    Send a command to the server.\n\
  show-slots\n\
    Show the player names, Guild Card numbers, and client IDs of all players in\n\
    the current lobby or game.\n\
  c TEXT\n\
  chat TEXT\n\
    Send a chat message to the server.\n\
  dchat DATA\n\
    Send a chat message to the server with arbitrary data in it.\n\
  info-board TEXT\n\
    Set your info board contents. This will affect the current session only,\n\
    and will not be saved for future sessions.\n\
  info-board-data DATA\n\
    Set your info board contents with arbitrary data. Like the above, affects\n\
    the current session only.\n\
  marker COLOR-ID\n\
    Change your lobby marker color.\n\
  warp AREA-ID\n\
  warpme AREA-ID\n\
    Send yourself to a specific area.\n\
  warpall AREA-ID\n\
    Send everyone to a specific area.\n\
  set-override-section-id [SECTION-ID]\n\
    Override the section ID for games you create or join. This affects the\n\
    active drop chart if you are the leader of the game and the server doesn't\n\
    override drops entirely. If no argument is given, clears the override.\n\
  set-override-event [EVENT]\n\
    Override the lobby event for all lobbies and games you join. This applies\n\
    only to you; other players do not see this override.  If no argument is\n\
    given, clears the override.\n\
  set-override-lobby-number [NUMBER]\n\
    Override the lobby type for all lobbies you join. This applies only to you;\n\
    other players do not see this override. If no argument is given, clears the\n\
    override.\n\
  set-chat-filter on|off\n\
    Enable or disable chat filtering (enabled by default). Chat filtering\n\
    applies newserv\'s standard character replacements to chat messages; for\n\
    example, $ becomes a tab character and # becomes a newline.\n\
  set-infinite-hp on|off\n\
  set-infinite-tp on|off\n\
    Enable or disable infinite HP or TP. When infinite HP is enabled, attacks\n\
    that would kill you in one hit will still do so.\n\
  set-switch-assist on|off\n\
    Enable or disable switch assist. When switch assist is on, the server will\n\
    remember the last \"enable switch\" command that you sent, and will send it\n\
    back to you (and to the remote server, if you\'re in a proxy session) when\n\
    you step on another switch. Using this, you can unlock some doors that\n\
    require two players to stand on switches by touching both switches\n\
    yourself.\n\
  set-save-files on|off\n\
    Enable or disable saving of game files (disabled by default). When this is\n\
    on, any file that the remote server sends to the client will be saved to\n\
    the current directory. This includes data like quests, Episode 3 card\n\
    definitions, and GBA games.\n\
  set-block-function-calls [RETURN-VALUE]\n\
    Enable blocking of function calls from the server. When enabled, the proxy\n\
    responds as if the function was called (with the given return value), but\n\
    does not send the code to the client. To stop blocking function calls, omit\n\
    the return value.\n\
  create-item DATA\n\
    Create an item as if the client had run the $item command.\n\
  set-next-item DATA\n\
    Set the next item to be dropped.\n\
  close-idle-sessions\n\
    Close all proxy sessions that don\'t have a client and server connected.\n\
");

    // SERVER COMMANDS

  } else if (command_name == "reload") {
    auto types = split(command_args, ' ');
    if (types.empty()) {
      throw invalid_argument("no data type given");
    }
    for (const string& type : types) {
      if (type == "licenses") {
        this->state->load_licenses();
      } else if (type == "patches") {
        this->state->load_patch_indexes();
      } else if (type == "battle-params") {
        this->state->load_battle_params();
      } else if (type == "level-table") {
        this->state->load_level_table();
      } else if (type == "item-tables") {
        this->state->load_item_tables();
      } else if (type == "ep3") {
        this->state->load_ep3_data();
      } else if (type == "quests") {
        this->state->load_quest_index();
      } else if (type == "functions") {
        auto config_json = this->state->load_config();
        this->state->compile_functions();
        this->state->create_menus(config_json);
      } else if (type == "dol-files") {
        auto config_json = this->state->load_config();
        this->state->load_dol_files();
        this->state->create_menus(config_json);
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
        throw invalid_argument("incorrect field: " + token);
      }
    }

    if (!l->serial_number) {
      throw invalid_argument("license does not contain serial number");
    }

    this->state->license_manager->add(l);
    fprintf(stderr, "license added\n");

  } else if (command_name == "update-license") {
    auto tokens = split(command_args, ' ');
    if (tokens.size() < 2) {
      throw runtime_error("not enough arguments");
    }
    uint32_t serial_number = stoul(tokens[0]);
    tokens.erase(tokens.begin());
    auto orig_l = this->state->license_manager->get(serial_number);
    shared_ptr<License> l(new License(*orig_l));

    for (const string& token : tokens) {
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
        throw invalid_argument("incorrect field: " + token);
      }
    }

    if (!l->serial_number) {
      throw invalid_argument("license does not contain serial number");
    }

    this->state->license_manager->add(l);
    fprintf(stderr, "license updated\n");

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
    set_boolean(&this->state->allow_unregistered_users, command_args);
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
    string data = parse_data_string(command_args, nullptr, ParseDataFlags::ALLOW_FILES);
    if (data.size() == 0) {
      throw invalid_argument("no data given");
    }
    data.resize((data.size() + 3) & (~3));

    shared_ptr<ProxyServer::LinkedSession> proxy_session;
    try {
      proxy_session = this->get_proxy_session(session_name);
    } catch (const exception&) {
    }

    if (proxy_session.get()) {
      if (command_name[1] == 's') {
        proxy_session->server_channel.send(data);
      } else {
        proxy_session->client_channel.send(data);
      }

    } else {
      shared_ptr<Client> c;
      if (session_name.empty()) {
        c = this->state->game_server->get_client();
      } else {
        auto clients = this->state->game_server->get_clients_by_identifier(
            session_name);
        if (clients.empty()) {
          throw runtime_error("no such client");
        }
        if (clients.size() > 1) {
          throw runtime_error("multiple clients found");
        }
        c = std::move(clients[0]);
      }

      if (c) {
        if (command_name[1] == 's') {
          on_command_with_header(this->state, c, data);
        } else {
          send_command_with_header(c->channel, data.data(), data.size());
        }
      } else {
        throw runtime_error("no client available");
      }
    }

  } else if (command_name == "show-slots") {
    auto session = this->get_proxy_session(session_name);
    for (size_t z = 0; z < session->lobby_players.size(); z++) {
      const auto& player = session->lobby_players[z];
      if (player.guild_card_number) {
        auto secid_name = name_for_section_id(player.section_id);
        fprintf(stderr, "  %zu: %" PRIu32 " => %s (%s, %s)\n",
            z, player.guild_card_number, player.name.c_str(),
            name_for_char_class(player.char_class), secid_name.c_str());
      } else {
        fprintf(stderr, "  %zu: (no player)\n", z);
      }
    }

  } else if ((command_name == "c") || (command_name == "chat") || (command_name == "dchat")) {
    auto session = this->get_proxy_session(session_name);
    bool is_dchat = (command_name == "dchat");

    if (!is_dchat && (session->version == GameVersion::PC || session->version == GameVersion::BB)) {
      u16string data(4, u'\0');
      data.push_back(u'\x09');
      data.push_back(u'E');
      data += decode_sjis(command_args);
      data.push_back(u'\0');
      data.resize((data.size() + 1) & (~1));
      session->server_channel.send(0x06, 0x00, data.data(), data.size() * sizeof(char16_t));
    } else {
      string data(8, '\0');
      data.push_back('\x09');
      data.push_back('E');
      if (is_dchat) {
        data += parse_data_string(command_args, nullptr, ParseDataFlags::ALLOW_FILES);
      } else {
        data += command_args;
        data.push_back('\0');
      }
      data.resize((data.size() + 3) & (~3));
      session->server_channel.send(0x06, 0x00, data);
    }

  } else if (command_name == "marker") {
    auto session = this->get_proxy_session(session_name);
    session->server_channel.send(0x89, stoul(command_args));

  } else if ((command_name == "warp") || (command_name == "warpme")) {
    auto session = this->get_proxy_session(session_name);

    uint8_t area = stoul(command_args);
    send_warp(session->client_channel, session->lobby_client_id, area, true);

  } else if (command_name == "warpall") {
    auto session = this->get_proxy_session(session_name);

    uint8_t area = stoul(command_args);
    send_warp(session->client_channel, session->lobby_client_id, area, false);
    send_warp(session->server_channel, session->lobby_client_id, area, false);

  } else if ((command_name == "info-board") || (command_name == "info-board-data")) {
    auto session = this->get_proxy_session(session_name);

    string data;
    if (command_name == "info-board-data") {
      data += parse_data_string(command_args, nullptr, ParseDataFlags::ALLOW_FILES);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));

    session->server_channel.send(0xD9, 0x00, data);

  } else if (command_name == "set-override-section-id") {
    auto session = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      session->options.override_section_id = -1;
    } else {
      session->options.override_section_id = section_id_for_name(command_args);
    }

  } else if (command_name == "set-override-event") {
    auto session = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      session->options.override_lobby_event = -1;
    } else {
      session->options.override_lobby_event = event_for_name(command_args);
      if ((session->version != GameVersion::DC) &&
          (session->version != GameVersion::PC) && (!((session->version == GameVersion::GC) && (session->newserv_client_config.cfg.flags & Client::Flag::IS_TRIAL_EDITION)))) {
        session->client_channel.send(0xDA, session->options.override_lobby_event);
      }
    }

  } else if (command_name == "set-override-lobby-number") {
    auto session = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      session->options.override_lobby_number = -1;
    } else {
      session->options.override_lobby_number = lobby_type_for_name(command_args);
    }

  } else if (command_name == "set-chat-filter") {
    auto session = this->get_proxy_session(session_name);
    set_boolean(&session->options.enable_chat_filter, command_args);

  } else if (command_name == "set-infinite-hp") {
    auto session = this->get_proxy_session(session_name);
    set_boolean(&session->options.infinite_hp, command_args);

  } else if (command_name == "set-infinite-tp") {
    auto session = this->get_proxy_session(session_name);
    set_boolean(&session->options.infinite_tp, command_args);

  } else if (command_name == "set-switch-assist") {
    auto session = this->get_proxy_session(session_name);
    set_boolean(&session->options.switch_assist, command_args);

  } else if (command_name == "set-save-files" && this->state->proxy_allow_save_files) {
    auto session = this->get_proxy_session(session_name);
    set_boolean(&session->options.save_files, command_args);

  } else if (command_name == "set-block-function-calls") {
    auto session = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      session->options.function_call_return_value = -1;
    } else {
      session->options.function_call_return_value = stoul(command_args);
    }

  } else if ((command_name == "create-item") || (command_name == "set-next-item")) {
    auto session = this->get_proxy_session(session_name);

    if (session->version == GameVersion::BB) {
      throw runtime_error("proxy session is BB");
    }
    if (!session->is_in_game) {
      throw runtime_error("proxy session is not in a game");
    }
    if (session->lobby_client_id != session->leader_client_id) {
      throw runtime_error("proxy session is not game leader");
    }

    string data = parse_data_string(command_args, nullptr, ParseDataFlags::ALLOW_FILES);
    if (data.size() < 2) {
      throw runtime_error("data too short");
    }
    if (data.size() > 16) {
      throw runtime_error("data too long");
    }

    PlayerInventoryItem item;
    item.data.id = random_object<uint32_t>();
    if (data.size() <= 12) {
      memcpy(item.data.data1.data(), data.data(), data.size());
    } else {
      memcpy(item.data.data1.data(), data.data(), 12);
      memcpy(item.data.data2.data(), data.data() + 12, data.size() - 12);
    }

    if (command_name == "set-next-item") {
      session->next_drop_item = item;

      string name = session->next_drop_item.data.name(true);
      send_text_message(session->client_channel, u"$C7Next drop:\n" + decode_sjis(name));

    } else {
      send_drop_stacked_item(session->client_channel, item.data, session->area, session->x, session->z);
      send_drop_stacked_item(session->server_channel, item.data, session->area, session->x, session->z);

      string name = item.data.name(true);
      send_text_message(session->client_channel, u"$C7Item created:\n" + decode_sjis(name));
    }

  } else if (command_name == "close-idle-sessions") {
    size_t count = this->state->proxy_server->delete_disconnected_sessions();
    fprintf(stderr, "%zu sessions closed\n", count);

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
