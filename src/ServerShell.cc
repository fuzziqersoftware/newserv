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

static void set_flag(Client::Config& config, Client::Flag flag, const string& args) {
  if (args == "on") {
    config.set_flag(flag);
  } else if (args == "off") {
    config.clear_flag(flag);
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
    Reload various parts of the server configuration. When you reload any item,\n\
    any other item that depends on it will be reloaded as well. The items are:\n\
      all - reindex/reload everything\n\
      battle-params - reload the BB enemy stats files\n\
      bb-private-keys - reload BB private keys\n\
      config - reload most fields from config.json\n\
      dol-files - reindex all DOL files\n\
      drop-tables - reload drop tables\n\
      ep3-data - reload Episode 3 cards and maps (not download quests)\n\
      functions - recompile all client-side patches and functions\n\
      item-definitions - reload item definitions files\n\
      level-table - reload the level-up tables\n\
      licenses - reindex user licenses\n\
      patch-indexes - reindex the PC and BB patch directories\n\
      quest-index - reindex all quests (including Episode3 download quests)\n\
      teams - reindex all BB teams\n\
      text-index - reload in-game text\n\
      word-select-table - regenerate the Word Select translation table\n\
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
      xb-gamertag=<gamertag> (Xbox gamertag)\n\
      xb-user-id=<user-id> (Xbox user ID)\n\
      xb-account-id=<account-id> (Xbox account ID)\n\
      gc-password=<password> (GC password)\n\
      access-key=<access-key> (DC/GC/PC access key)\n\
      serial=<serial-number> (decimal serial number; required for all licenses)\n\
      flags=<privilege-mask> (can be normal, mod, admin, root, or numeric)\n\
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
      no-coms: Don\'t add any COM teams to the tournament bracket\n\
      shuffle: Shuffle entries when starting the tournament\n\
      resize: If the tournament is less than half full when it starts, reduce\n\
          the number of rounds to fit the existing entries\n\
      dice=MIN-MAX: Set minimum and maximum dice rolls\n\
      dice=MIN-MAX:MIN-MAX: Set minimum and maximum dice rolls for ATK and DEF\n\
          dice separately\n\
      overall-time-limit=N: Set battle time limit (in multiples of 5 minutes)\n\
      phase-time-limit=N: Set phase time limit (in seconds)\n\
      allowed-cards=ALL/N/NR/NRS: Set ranks of allowed cards\n\
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
  describe-tournament TOURNAMENT-NAME\n\
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
  wchat DATA\n\
    Send a chat message with private_flags on Episode 3.\n\
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
    Send yourself to a specific floor.\n\
  warpall AREA-ID\n\
    Send everyone to a specific floor.\n\
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
    for (auto& type : types) {
      for (char& ch : type) {
        if (ch == '-') {
          ch = '_';
        }
      }
    }
    this->state->load_objects(types);

  } else if (command_name == "add-license") {
    auto l = this->state->license_index->create_license();

    for (const string& token : split(command_args, ' ')) {
      if (starts_with(token, "bb-username=")) {
        l->bb_username = token.substr(12);
      } else if (starts_with(token, "bb-password=")) {
        l->bb_password = token.substr(12);
      } else if (starts_with(token, "xb-gamertag=")) {
        l->xb_gamertag = token.substr(12);
      } else if (starts_with(token, "xb-user-id=")) {
        l->xb_user_id = stoull(token.substr(11), nullptr, 16);
      } else if (starts_with(token, "xb-account-id=")) {
        l->xb_account_id = stoull(token.substr(14), nullptr, 16);
      } else if (starts_with(token, "gc-password=")) {
        l->gc_password = token.substr(12);
      } else if (starts_with(token, "access-key=")) {
        l->access_key = token.substr(11);
      } else if (starts_with(token, "serial=")) {
        l->serial_number = stoul(token.substr(7), nullptr, 0);

      } else if (starts_with(token, "flags=")) {
        string mask = token.substr(6);
        if (mask == "normal") {
          l->flags = 0;
        } else if (mask == "mod") {
          l->flags = License::Flag::MODERATOR;
        } else if (mask == "admin") {
          l->flags = License::Flag::ADMINISTRATOR;
        } else if (mask == "root") {
          l->flags = License::Flag::ROOT;
        } else {
          l->flags = stoul(mask, nullptr, 16);
        }

      } else {
        throw invalid_argument("incorrect field: " + token);
      }
    }

    if (!l->serial_number) {
      throw invalid_argument("license does not contain serial number");
    }

    l->save();
    this->state->license_index->add(l);
    fprintf(stderr, "license added\n");

  } else if (command_name == "update-license") {
    auto tokens = split(command_args, ' ');
    if (tokens.size() < 2) {
      throw runtime_error("not enough arguments");
    }
    uint32_t serial_number = stoul(tokens[0]);
    tokens.erase(tokens.begin());
    auto orig_l = this->state->license_index->get(serial_number);
    auto l = this->state->license_index->create_license();
    *l = *orig_l;

    this->state->license_index->remove(orig_l->serial_number);
    try {
      for (const string& token : tokens) {
        if (starts_with(token, "bb-username=")) {
          l->bb_username = token.substr(12);
        } else if (starts_with(token, "bb-password=")) {
          l->bb_password = token.substr(12);
        } else if (starts_with(token, "xb-gamertag=")) {
          l->xb_gamertag = token.substr(12);
        } else if (starts_with(token, "xb-user-id=")) {
          l->xb_user_id = stoull(token.substr(11), nullptr, 16);
        } else if (starts_with(token, "xb-account-id=")) {
          l->xb_account_id = stoull(token.substr(14), nullptr, 16);
        } else if (starts_with(token, "gc-password=")) {
          l->gc_password = token.substr(12);
        } else if (starts_with(token, "access-key=")) {
          l->access_key = token.substr(11);
        } else if (starts_with(token, "serial=")) {
          l->serial_number = stoul(token.substr(7), nullptr, 0);

        } else if (starts_with(token, "flags=")) {
          string mask = token.substr(6);
          if (mask == "normal") {
            l->flags = 0;
          } else if (mask == "mod") {
            l->flags = License::Flag::MODERATOR;
          } else if (mask == "admin") {
            l->flags = License::Flag::ADMINISTRATOR;
          } else if (mask == "root") {
            l->flags = License::Flag::ROOT;
          } else {
            l->flags = stoul(mask, nullptr, 16);
          }

        } else {
          throw invalid_argument("incorrect field: " + token);
        }
      }

      if (!l->serial_number) {
        throw invalid_argument("license does not contain serial number");
      }
    } catch (const exception&) {
      this->state->license_index->add(orig_l);
      throw;
    }

    l->save();
    this->state->license_index->add(l);
    fprintf(stderr, "license updated\n");

  } else if (command_name == "delete-license") {
    uint32_t serial_number = stoul(command_args);
    auto l = this->state->license_index->get(serial_number);
    l->delete_file();
    this->state->license_index->remove(l->serial_number);
    fprintf(stderr, "license deleted\n");

  } else if (command_name == "list-licenses") {
    for (const auto& l : this->state->license_index->all()) {
      string s = l->str();
      fprintf(stderr, "%s\n", s.c_str());
    }

  } else if (command_name == "set-allow-unregistered-users") {
    set_boolean(&this->state->allow_unregistered_users, command_args);
    fprintf(stderr, "unregistered users are now %s\n", this->state->allow_unregistered_users ? "allowed" : "disallowed");

  } else if (command_name == "set-allow-pc-nte") {
    set_boolean(&this->state->allow_pc_nte, command_args);
    fprintf(stderr, "PC NTE is now %s\n", this->state->allow_pc_nte ? "allowed" : "disallowed");

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
    send_text_message(this->state, command_args);

  } else if (command_name == "create-tournament") {
    string name = get_quoted_string(command_args);
    string map_name = get_quoted_string(command_args);
    auto map = this->state->ep3_map_index->for_name(map_name);
    uint32_t num_teams = stoul(get_quoted_string(command_args), nullptr, 0);
    Episode3::Rules rules;
    rules.set_defaults();
    uint8_t flags = Episode3::Tournament::Flag::HAS_COM_TEAMS;
    if (!command_args.empty()) {
      auto tokens = split(command_args, ' ');
      for (auto& token : tokens) {
        token = tolower(token);
        if (token == "2v2") {
          flags |= Episode3::Tournament::Flag::IS_2V2;
        } else if (token == "no-coms") {
          flags &= (~Episode3::Tournament::Flag::HAS_COM_TEAMS);
        } else if (token == "shuffle") {
          flags |= Episode3::Tournament::Flag::SHUFFLE_ENTRIES;
        } else if (token == "resize") {
          flags |= Episode3::Tournament::Flag::RESIZE_ON_START;
        } else if (starts_with(token, "dice=")) {
          auto subtokens = split(token.substr(5), ':');
          if (subtokens.size() == 1) {
            rules.def_dice_range = 0x00;
          } else if (subtokens.size() == 2) {
            auto subsubtokens = split(subtokens[1], '-');
            if (subsubtokens.size() != 2) {
              throw runtime_error("dice option must be of the form dice=A-B or dice=A-B:C-D");
            }
            rules.def_dice_range = ((stoul(subsubtokens[0]) << 4) & 0xF0) | (stoul(subsubtokens[1]) & 0x0F);
          } else {
            throw runtime_error("dice option must be of the form dice=A-B or dice=A-B:C-D");
          }
          auto subsubtokens = split(subtokens[0], '-');
          if (subsubtokens.size() != 2) {
            throw runtime_error("dice option must be of the form dice=A-B or dice=A-B:C-D");
          }
          rules.min_dice = stoul(subsubtokens[0]);
          rules.max_dice = stoul(subsubtokens[1]);
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
        name, map, rules, num_teams, flags);
    fprintf(stderr, "created tournament \"%s\"\n", tourn->get_name().c_str());

  } else if (command_name == "delete-tournament") {
    string name = get_quoted_string(command_args);
    if (this->state->ep3_tournament_index->delete_tournament(name)) {
      fprintf(stderr, "tournament deleted\n");
    } else {
      fprintf(stderr, "no such tournament exists\n");
    }

  } else if (command_name == "list-tournaments") {
    for (const auto& it : this->state->ep3_tournament_index->all_tournaments()) {
      fprintf(stderr, "  %s\n", it.second->get_name().c_str());
    }

  } else if (command_name == "start-tournament") {
    string name = get_quoted_string(command_args);
    auto tourn = this->state->ep3_tournament_index->get_tournament(name);
    if (tourn) {
      tourn->start();
      this->state->ep3_tournament_index->save();
      tourn->send_all_state_updates();
      send_ep3_text_message_printf(this->state, "$C7The tournament\n$C6%s$C7\nhas begun", tourn->get_name().c_str());
      fprintf(stderr, "tournament started\n");
    } else {
      fprintf(stderr, "no such tournament exists\n");
    }

  } else if (command_name == "describe-tournament") {
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

    shared_ptr<ProxyServer::LinkedSession> ses;
    try {
      ses = this->get_proxy_session(session_name);
    } catch (const exception&) {
    }

    if (ses.get()) {
      if (command_name[1] == 's') {
        ses->server_channel.send(data);
      } else {
        ses->client_channel.send(data);
      }

    } else {
      shared_ptr<Client> c;
      if (session_name.empty()) {
        c = this->state->game_server->get_client();
      } else {
        auto clients = this->state->game_server->get_clients_by_identifier(session_name);
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
          on_command_with_header(c, data);
        } else {
          send_command_with_header(c->channel, data.data(), data.size());
        }
      } else {
        throw runtime_error("no client available");
      }
    }

  } else if (command_name == "show-slots") {
    auto ses = this->get_proxy_session(session_name);
    for (size_t z = 0; z < ses->lobby_players.size(); z++) {
      const auto& player = ses->lobby_players[z];
      if (player.guild_card_number) {
        fprintf(stderr, "  %zu: %" PRIu32 " => %s (%c, %s, %s)\n",
            z, player.guild_card_number, player.name.c_str(),
            char_for_language_code(player.language),
            name_for_char_class(player.char_class),
            name_for_section_id(player.section_id));
      } else {
        fprintf(stderr, "  %zu: (no player)\n", z);
      }
    }

  } else if ((command_name == "c") || (command_name == "chat") || (command_name == "dchat")) {
    auto ses = this->get_proxy_session(session_name);
    bool is_dchat = (command_name == "dchat");

    if (!is_dchat && uses_utf16(ses->version())) {
      send_chat_message_from_client(ses->server_channel, command_args, 0);
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
      ses->server_channel.send(0x06, 0x00, data);
    }

  } else if ((command_name == "wc") || (command_name == "wchat")) {
    auto ses = this->get_proxy_session(session_name);
    if (!is_ep3(ses->version())) {
      throw runtime_error("wchat can only be used on Episode 3");
    }
    string data(8, '\0');
    data.push_back('\x40'); // private_flags: visible to all
    data.push_back('\x09');
    data.push_back('E');
    data += command_args;
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));
    ses->server_channel.send(0x06, 0x00, data);

  } else if (command_name == "marker") {
    auto ses = this->get_proxy_session(session_name);
    ses->server_channel.send(0x89, stoul(command_args));

  } else if ((command_name == "warp") || (command_name == "warpme")) {
    auto ses = this->get_proxy_session(session_name);

    uint8_t floor = stoul(command_args);
    send_warp(ses->client_channel, ses->lobby_client_id, floor, true);

  } else if (command_name == "warpall") {
    auto ses = this->get_proxy_session(session_name);

    uint8_t floor = stoul(command_args);
    send_warp(ses->client_channel, ses->lobby_client_id, floor, false);
    send_warp(ses->server_channel, ses->lobby_client_id, floor, false);

  } else if ((command_name == "info-board") || (command_name == "info-board-data")) {
    auto ses = this->get_proxy_session(session_name);

    string data;
    if (command_name == "info-board-data") {
      data += parse_data_string(command_args, nullptr, ParseDataFlags::ALLOW_FILES);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));

    ses->server_channel.send(0xD9, 0x00, data);

  } else if (command_name == "set-override-section-id") {
    auto ses = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      ses->config.override_section_id = 0xFF;
    } else {
      ses->config.override_section_id = section_id_for_name(command_args);
    }

  } else if (command_name == "set-override-event") {
    auto ses = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      ses->config.override_lobby_event = 0xFF;
    } else {
      ses->config.override_lobby_event = event_for_name(command_args);
      if (!is_v1_or_v2(ses->version())) {
        ses->client_channel.send(0xDA, ses->config.override_lobby_event);
      }
    }

  } else if (command_name == "set-override-lobby-number") {
    auto ses = this->get_proxy_session(session_name);
    if (command_args.empty()) {
      ses->config.override_lobby_number = 0x80;
    } else {
      ses->config.override_lobby_number = lobby_type_for_name(command_args);
    }

  } else if (command_name == "set-challenge-rank-title") {
    auto ses = this->get_proxy_session(session_name);
    ses->challenge_rank_title_override = command_args;

  } else if (command_name == "set-challenge-rank-color") {
    auto ses = this->get_proxy_session(session_name);
    ses->challenge_rank_color_override = stoul(command_args, nullptr, 16);

  } else if (command_name == "set-infinite-hp") {
    auto ses = this->get_proxy_session(session_name);
    set_flag(ses->config, Client::Flag::INFINITE_HP_ENABLED, command_args);

  } else if (command_name == "set-infinite-tp") {
    auto ses = this->get_proxy_session(session_name);
    set_flag(ses->config, Client::Flag::INFINITE_TP_ENABLED, command_args);

  } else if (command_name == "set-switch-assist") {
    auto ses = this->get_proxy_session(session_name);
    set_flag(ses->config, Client::Flag::SWITCH_ASSIST_ENABLED, command_args);

  } else if (command_name == "set-save-files" && this->state->proxy_allow_save_files) {
    auto ses = this->get_proxy_session(session_name);
    set_flag(ses->config, Client::Flag::PROXY_SAVE_FILES, command_args);

  } else if (command_name == "set-block-function-calls") {
    auto ses = this->get_proxy_session(session_name);
    set_flag(ses->config, Client::Flag::PROXY_BLOCK_FUNCTION_CALLS, command_args);

  } else if ((command_name == "create-item") || (command_name == "set-next-item")) {
    auto ses = this->get_proxy_session(session_name);

    if (ses->version() == Version::BB_V4) {
      throw runtime_error("proxy session is BB");
    }
    if (!ses->is_in_game) {
      throw runtime_error("proxy session is not in a game");
    }
    if (ses->lobby_client_id != ses->leader_client_id) {
      throw runtime_error("proxy session is not game leader");
    }

    auto s = ses->require_server_state();
    ItemData item = s->parse_item_description(ses->version(), command_args);
    item.id = random_object<uint32_t>() | 0x80000000;

    if (command_name == "set-next-item") {
      ses->next_drop_item = item;

      string name = s->describe_item(ses->version(), ses->next_drop_item, true);
      send_text_message(ses->client_channel, "$C7Next drop:\n" + name);

    } else {
      send_drop_stacked_item_to_channel(s, ses->client_channel, item, ses->floor, ses->x, ses->z);
      send_drop_stacked_item_to_channel(s, ses->server_channel, item, ses->floor, ses->x, ses->z);

      string name = s->describe_item(ses->version(), ses->next_drop_item, true);
      send_text_message(ses->client_channel, "$C7Item created:\n" + name);
    }

  } else if (command_name == "close-idle-sessions") {
    size_t count = this->state->proxy_server->delete_disconnected_sessions();
    fprintf(stderr, "%zu sessions closed\n", count);

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
