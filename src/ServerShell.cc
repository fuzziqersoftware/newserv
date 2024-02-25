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

struct CommandArgs {
  shared_ptr<ServerState> s;
  shared_ptr<ServerShell> shell;
  string command;
  string args;
  string session_name;
};

struct CommandDefinition;

static std::vector<const CommandDefinition*> commands_by_order;
static std::unordered_map<std::string, const CommandDefinition*> commands_by_name;

struct CommandDefinition {

  const char* name;
  const char* help_text;
  bool run_on_event_thread;
  void (*run)(CommandArgs&);

  CommandDefinition(const char* name, const char* help_text, bool run_on_event_thread, void (*run)(CommandArgs&))
      : name(name),
        help_text(help_text),
        run_on_event_thread(run_on_event_thread),
        run(run) {
    commands_by_order.emplace_back(this);
    commands_by_name.emplace(this->name, this);
  }

  static void dispatch(CommandArgs& args) {
    const CommandDefinition* def = nullptr;
    try {
      def = commands_by_name.at(args.command);
    } catch (const out_of_range&) {
    }
    if (!def) {
      fprintf(stderr, "FAILED: no such command; try 'help'\n");
    } else if (def->run_on_event_thread) {
      args.s->call_on_event_thread<void>([def, args]() {
        CommandArgs local_args = args;
        try {
          def->run(local_args);
        } catch (const exception& e) {
          fprintf(stderr, "FAILED: %s\n", e.what());
        }
      });
    } else {
      def->run(args);
    }
  }
};

ServerShell::exit_shell::exit_shell() : runtime_error("shell exited") {}

ServerShell::ServerShell(shared_ptr<ServerState> state)
    : state(state),
      th(&ServerShell::thread_fn, this) {}

ServerShell::~ServerShell() {
  if (this->th.joinable()) {
    this->th.join();
  }
}

void ServerShell::thread_fn() {
  for (;;) {
    fprintf(stdout, "newserv> ");
    fflush(stdout);
    string command = fgets(stdin);

    // If command is empty (not even a \n), it's probably EOF
    if (command.empty()) {
      fputc('\n', stderr);
      event_base_loopexit(this->state->base.get(), nullptr);
      return;
    }

    strip_trailing_whitespace(command);

    try {
      // Find the entry in the command table and run the command
      size_t command_end = skip_non_whitespace(command, 0);
      size_t args_begin = skip_whitespace(command, command_end);
      CommandArgs args;
      args.s = this->state;
      args.shell = this->shared_from_this();
      args.command = command.substr(0, command_end);
      args.args = command.substr(args_begin);
      CommandDefinition::dispatch(args);
    } catch (const exit_shell&) {
      event_base_loopexit(this->state->base.get(), nullptr);
      return;
    } catch (const exception& e) {
      fprintf(stderr, "FAILED: %s\n", e.what());
    }
  }
}

shared_ptr<ProxyServer::LinkedSession> ServerShell::get_proxy_session(const string& name) {
  if (!this->state->proxy_server.get()) {
    throw runtime_error("the proxy server is disabled");
  }
  return name.empty()
      ? this->state->proxy_server->get_session()
      : this->state->proxy_server->get_session_by_name(name);
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

CommandDefinition c_nop1(
    "", nullptr, false, +[](CommandArgs&) {});
CommandDefinition c_nop2(
    "//", nullptr, false, +[](CommandArgs&) {});
CommandDefinition c_nop3(
    "#", nullptr, false, +[](CommandArgs&) {});

CommandDefinition c_help(
    "help", "help\n\
    You\'re reading it now.",
    false,
    +[](CommandArgs&) {
      fputs("Commands:\n", stderr);
      for (const auto& def : commands_by_order) {
        if (def->help_text) {
          fprintf(stderr, "  %s\n", def->help_text);
        }
      }
    });
CommandDefinition c_exit(
    "exit", "exit (or ctrl+d)\n\
    Shut down the server.",
    false,
    +[](CommandArgs&) {
      throw ServerShell::exit_shell();
    });
CommandDefinition c_on(
    "on", "on SESSION COMMAND [ARGS...]\n\
    Run a command on a specific game server client or proxy server session.\n\
    Without this prefix, commands that affect a single client or session will\n\
    work only if there's exactly one connected client or open session. SESSION\n\
    may be a client ID (e.g. C-3), a player name, a license serial number, or\n\
    a BB account username. For proxy commands, SESSION should be the session\n\
    ID, which generally is the same as the player\'s serial number and appears\n\
    after \"LinkedSession:\" in the log output.",
    false,
    +[](CommandArgs& args) {
      size_t session_name_end = skip_non_whitespace(args.args, 0);
      size_t command_begin = skip_whitespace(args.args, session_name_end);
      size_t command_end = skip_non_whitespace(args.args, command_begin);
      size_t args_begin = skip_whitespace(args.args, command_end);
      args.session_name = args.args.substr(0, session_name_end);
      args.command = args.args.substr(command_begin, command_end - command_begin);
      args.args = args.args.substr(args_begin);
      CommandDefinition::dispatch(args);
    });

CommandDefinition c_reload(
    "reload", "reload ITEM [ITEM...]\n\
    Reload various parts of the server configuration. The items are:\n\
      battle-params - reload the BB enemy stats files\n\
      bb-keys - reload BB private keys\n\
      config - reload most fields from config.json\n\
      dol-files - reindex all DOL files\n\
      drop-tables - reload drop tables\n\
      ep3-cards - reload Episode 3 card definitions\n\
      ep3-maps - reload Episode 3 maps (not download quests)\n\
      ep3-tournaments - reload Episode 3 tournament state\n\
      functions - recompile all client-side patches and functions\n\
      item-definitions - reload item definitions files\n\
      item-name-index - regenerate item name list\n\
      level-table - reload the level-up tables\n\
      licenses - reindex user licenses\n\
      patch-files - reindex the PC and BB patch directories\n\
      quests - reindex all quests (including Episode3 download quests)\n\
      set-tables - reload set data tables\n\
      teams - reindex all BB teams\n\
      text-index - reload in-game text\n\
      word-select - regenerate the Word Select translation table\n\
    Reloading will not affect items that are in use; for example, if an Episode\n\
    3 battle is in progress, it will continue to use the previous map and card\n\
    definitions. Similarly, BB clients are not forced to disconnect or reload\n\
    the battle parameters, so if these are changed without restarting, clients\n\
    may see (for example) EXP messages inconsistent with the amounts of EXP\n\
    actually received.",
    false,
    +[](CommandArgs& args) {
      auto types = split(args.args, ' ');
      for (const auto& type : types) {
        if (type == "bb-keys") {
          args.s->load_bb_private_keys(true);
        } else if (type == "licenses") {
          args.s->load_licenses(true);
        } else if (type == "patch-files") {
          args.s->load_patch_indexes(true);
        } else if (type == "ep3-cards") {
          args.s->load_ep3_cards(true);
        } else if (type == "ep3-maps") {
          args.s->load_ep3_maps(true);
        } else if (type == "ep3-tournaments") {
          args.s->load_ep3_tournament_state(true);
        } else if (type == "functions") {
          args.s->compile_functions(true);
        } else if (type == "dol-files") {
          args.s->load_dol_files(true);
        } else if (type == "set-tables") {
          args.s->load_set_data_tables(true);
        } else if (type == "battle-params") {
          args.s->load_battle_params(true);
        } else if (type == "level-table") {
          args.s->load_level_table(true);
        } else if (type == "text-index") {
          args.s->load_text_index(true);
        } else if (type == "word-select") {
          args.s->load_word_select_table(true);
        } else if (type == "item-definitions") {
          args.s->load_item_definitions(true);
        } else if (type == "item-name-index") {
          args.s->load_item_name_indexes(true);
        } else if (type == "drop-tables") {
          args.s->load_drop_tables(true);
        } else if (type == "config") {
          args.s->forward_to_event_thread([s = args.s]() {
            s->load_config_early();
            s->load_config_late();
          });
        } else if (type == "teams") {
          args.s->load_teams(true);
        } else if (type == "quests") {
          args.s->load_quest_index(true);
        } else {
          throw runtime_error("invalid data type: " + type);
        }
      }
    });

CommandDefinition c_add_license(
    "add-license", "add-license PARAMETERS...\n\
    Add a license to the server. <parameters> is some subset of the following:\n\
      bb-username=<username> (BB username)\n\
      bb-password=<password> (BB password)\n\
      xb-gamertag=<gamertag> (Xbox gamertag)\n\
      xb-user-id=<user-id> (Xbox user ID)\n\
      xb-account-id=<account-id> (Xbox account ID)\n\
      gc-password=<password> (GC password)\n\
      dc-nte-serial-number=<serial-number> (DC NTE serial number)\n\
      dc-nte-access-key=<access-key> (DC NTE access key)\n\
      access-key=<access-key> (DC/GC/PC access key)\n\
      serial=<serial-number> (decimal serial number; required for all licenses)\n\
      flags=<privilege-mask> (see below)\n\
    If flags is specified in hex, the meanings of bits are:\n\
      00000001 = Can kick other users offline\n\
      00000002 = Can ban other users\n\
      00000004 = Can silence other users\n\
      00000010 = Can change lobby events\n\
      00000020 = Can make server-wide announcements\n\
      00000040 = Ignores game join restrictions (e.g. level/quest requirements)\n\
      01000000 = Can use debugging commands\n\
      02000000 = Can use cheat commands even if cheat mode is disabled\n\
      04000000 = Can play any quest without progression/flags restrictions\n\
      80000000 = License is a shared serial (disables Access Key and password\n\
        checks; players will get Guild Cards based on their player names)\n\
    There are also shorthands for some general privilege levels:\n\
      flags=moderator = 00000007\n\
      flags=admin = 000000FF\n\
      flags=root = 7FFFFFFF",
    true,
    +[](CommandArgs& args) {
      auto l = args.s->license_index->create_license();

      for (const string& token : split(args.args, ' ')) {
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
        } else if (starts_with(token, "dc-nte-serial-number=")) {
          l->dc_nte_serial_number = token.substr(21);
        } else if (starts_with(token, "dc-nte-access-key=")) {
          l->dc_nte_access_key = token.substr(18);
        } else if (starts_with(token, "access-key=")) {
          l->access_key = token.substr(11);
        } else if (starts_with(token, "serial=")) {
          l->serial_number = stoul(token.substr(7), nullptr, 0);

        } else if (starts_with(token, "flags=")) {
          string mask = token.substr(6);
          if (mask == "normal") {
            l->flags = 0;
          } else if (mask == "mod") {
            l->replace_all_flags(License::Flag::MODERATOR);
          } else if (mask == "admin") {
            l->replace_all_flags(License::Flag::ADMINISTRATOR);
          } else if (mask == "root") {
            l->replace_all_flags(License::Flag::ROOT);
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
      args.s->license_index->add(l);
      fprintf(stderr, "license added\n");
    });

CommandDefinition c_update_license(
    "update-license", "update-license SERIAL-NUMBER PARAMETERS...\n\
    Update an existing license. <serial-number> specifies which license to\n\
    update. The options in <parameters> are the same as for the add-license\n\
    command.",
    true,
    +[](CommandArgs& args) {
      auto tokens = split(args.args, ' ');
      if (tokens.size() < 2) {
        throw runtime_error("not enough arguments");
      }
      uint32_t serial_number = stoul(tokens[0]);
      tokens.erase(tokens.begin());
      auto orig_l = args.s->license_index->get(serial_number);
      auto l = args.s->license_index->create_license();
      *l = *orig_l;

      args.s->license_index->remove(orig_l->serial_number);
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
          } else if (starts_with(token, "dc-nte-serial-number=")) {
            l->dc_nte_serial_number = token.substr(21);
          } else if (starts_with(token, "dc-nte-access-key=")) {
            l->dc_nte_access_key = token.substr(18);
          } else if (starts_with(token, "access-key=")) {
            l->access_key = token.substr(11);
          } else if (starts_with(token, "serial=")) {
            l->serial_number = stoul(token.substr(7), nullptr, 0);

          } else if (starts_with(token, "flags=")) {
            string mask = token.substr(6);
            if (mask == "normal") {
              l->flags = 0;
            } else if (mask == "mod") {
              l->replace_all_flags(License::Flag::MODERATOR);
            } else if (mask == "admin") {
              l->replace_all_flags(License::Flag::ADMINISTRATOR);
            } else if (mask == "root") {
              l->replace_all_flags(License::Flag::ROOT);
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
        args.s->license_index->add(orig_l);
        throw;
      }

      l->save();
      args.s->license_index->add(l);
      fprintf(stderr, "license updated\n");
    });
CommandDefinition c_delete_license(
    "delete-license", "delete-license SERIAL-NUMBER\n\
    Delete a license from the server.",
    true,
    +[](CommandArgs& args) {
      uint32_t serial_number = stoul(args.args);
      auto l = args.s->license_index->get(serial_number);
      l->delete_file();
      args.s->license_index->remove(l->serial_number);
      fprintf(stderr, "license deleted\n");
    });
CommandDefinition c_list_licenses(
    "list-licenses", "list-licenses\n\
    List all licenses registered on the server.",
    true,
    +[](CommandArgs& args) {
      for (const auto& l : args.s->license_index->all()) {
        string s = l->str();
        fprintf(stderr, "%s\n", s.c_str());
      }
    });

CommandDefinition c_announce(
    "announce", "announce MESSAGE\n\
    Send an announcement message to all players.",
    true,
    +[](CommandArgs& args) {
      send_text_message(args.s, args.args);
    });

CommandDefinition c_create_tournament(
    "create-tournament", "create-tournament TOURNAMENT-NAME MAP-NAME NUM-TEAMS [OPTIONS...]\n\
    Create an Episode 3 tournament. Quotes are required around the tournament\n\
    and map names, unless the names contain no spaces.\n\
    OPTIONS may include:\n\
      2v2: Set team size to 2 players (default is 1 without this option)\n\
      no-coms: Don\'t add any COM teams to the tournament bracket\n\
      shuffle: Shuffle entries when starting the tournament\n\
      resize: If the tournament is less than half full when it starts, reduce\n\
          the number of rounds to fit the existing entries\n\
      dice=A-B: Set minimum and maximum dice rolls\n\
      dice=A-B:C-D: Set minimum and maximum dice rolls for ATK dice (A-B) and\n\
          DEF dice (C-D) separately\n\
      dice=A-B:C-D:E-F: Set minimum and maximum dice rolls for ATK dice, DEF\n\
          dice, and solo vs. 2P ATK and DEF dice (E-F) separately\n\
      dice=A-B:C-D:E-F:G-H: Set minimum and maximum dice rolls for ATK dice,\n\
          DEF dice, solo vs. 2P ATK (E-F) and DEF (G-H) dice separately\n\
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
      dice-boost=ON/OFF: Enable/disable dice boost",
    true,
    +[](CommandArgs& args) {
      string name = get_quoted_string(args.args);
      string map_name = get_quoted_string(args.args);
      auto map = args.s->ep3_map_index->for_name(map_name);
      uint32_t num_teams = stoul(get_quoted_string(args.args), nullptr, 0);
      Episode3::Rules rules;
      rules.set_defaults();
      uint8_t flags = Episode3::Tournament::Flag::HAS_COM_TEAMS;
      if (!args.args.empty()) {
        auto tokens = split(args.args, ' ');
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
            auto parse_range_c = +[](const string& s) -> uint8_t {
              auto tokens = split(s, '-');
              if (tokens.size() != 2) {
                throw runtime_error("dice spec must be of the form MIN-MAX");
              }
              return (stoul(tokens[0]) << 4) | (stoul(tokens[1]) & 0x0F);
            };
            auto parse_range_p = +[](const string& s) -> pair<uint8_t, uint8_t> {
              auto tokens = split(s, '-');
              if (tokens.size() != 2) {
                throw runtime_error("dice spec must be of the form MIN-MAX");
              }
              return make_pair(stoul(tokens[0]), stoul(tokens[1]));
            };

            auto subtokens = split(token.substr(5), ':');
            if (subtokens.size() < 1) {
              throw runtime_error("no dice ranges specified in dice= option");
            }
            auto atk_range = parse_range_p(tokens[0]);
            rules.min_dice_value = atk_range.first;
            rules.max_dice_value = atk_range.second;
            if (subtokens.size() >= 2) {
              rules.def_dice_value_range = parse_range_c(tokens[1]);
              if (subtokens.size() >= 3) {
                rules.atk_dice_value_range_2v1 = parse_range_c(tokens[2]);
                if (subtokens.size() == 3) {
                  rules.def_dice_value_range_2v1 = rules.atk_dice_value_range_2v1;
                } else if (subtokens.size() == 4) {
                  rules.def_dice_value_range_2v1 = parse_range_c(tokens[3]);
                } else {
                  throw runtime_error("too many range specs given");
                }
              } else {
                rules.atk_dice_value_range_2v1 = 0;
                rules.def_dice_value_range_2v1 = 0;
              }
            } else {
              rules.def_dice_value_range = 0;
              rules.atk_dice_value_range_2v1 = 0;
              rules.def_dice_value_range_2v1 = 0;
            }
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
      auto tourn = args.s->ep3_tournament_index->create_tournament(name, map, rules, num_teams, flags);
      fprintf(stderr, "created tournament \"%s\"\n", tourn->get_name().c_str());
    });

CommandDefinition c_delete_tournament(
    "delete-tournament", "delete-tournament TOURNAMENT-NAME\n\
    Delete a tournament. Quotes are required around the tournament name unless\n\
    the name contains no spaces.",
    true,
    +[](CommandArgs& args) {
      string name = get_quoted_string(args.args);
      if (args.s->ep3_tournament_index->delete_tournament(name)) {
        fprintf(stderr, "tournament deleted\n");
      } else {
        fprintf(stderr, "no such tournament exists\n");
      }
    });
CommandDefinition c_list_tournaments(
    "list-tournaments", "list-tournaments\n\
    List the names and numbers of all existing tournaments.",
    true,
    +[](CommandArgs& args) {
      for (const auto& it : args.s->ep3_tournament_index->all_tournaments()) {
        fprintf(stderr, "  %s\n", it.second->get_name().c_str());
      }
    });
CommandDefinition c_start_tournament(
    "start-tournament", "start-tournament TOURNAMENT-NAME\n\
    End registration for a tournament and allow matches to begin. Quotes are\n\
    required around the tournament name unless the name contains no spaces.",
    true,
    +[](CommandArgs& args) {
      string name = get_quoted_string(args.args);
      auto tourn = args.s->ep3_tournament_index->get_tournament(name);
      if (tourn) {
        tourn->start();
        args.s->ep3_tournament_index->save();
        tourn->send_all_state_updates();
        send_ep3_text_message_printf(args.s, "$C7The tournament\n$C6%s$C7\nhas begun", tourn->get_name().c_str());
        fprintf(stderr, "tournament started\n");
      } else {
        fprintf(stderr, "no such tournament exists\n");
      }
    });
CommandDefinition c_describe_tournament(
    "describe-tournament", "describe-tournament TOURNAMENT-NAME\n\
    Show the current state of a tournament. Quotes are required around the\n\
    tournament name unless the name contains no spaces.",
    true,
    +[](CommandArgs& args) {
      string name = get_quoted_string(args.args);
      auto tourn = args.s->ep3_tournament_index->get_tournament(name);
      if (tourn) {
        tourn->print_bracket(stderr);
      } else {
        fprintf(stderr, "no such tournament exists\n");
      }
    });

void f_sc_ss(CommandArgs& args) {
  string data = parse_data_string(args.args, nullptr, ParseDataFlags::ALLOW_FILES);
  if (data.size() == 0) {
    throw invalid_argument("no data given");
  }
  data.resize((data.size() + 3) & (~3));

  shared_ptr<ProxyServer::LinkedSession> ses;
  try {
    ses = args.shell->get_proxy_session(args.session_name);
  } catch (const exception&) {
  }

  if (ses.get()) {
    if (args.command[1] == 's') {
      ses->server_channel.send(data);
    } else {
      ses->client_channel.send(data);
    }

  } else {
    shared_ptr<Client> c;
    if (args.session_name.empty()) {
      c = args.s->game_server->get_client();
    } else {
      auto clients = args.s->game_server->get_clients_by_identifier(args.session_name);
      if (clients.empty()) {
        throw runtime_error("no such client");
      }
      if (clients.size() > 1) {
        throw runtime_error("multiple clients found");
      }
      c = std::move(clients[0]);
    }

    if (c) {
      if (args.command[1] == 's') {
        on_command_with_header(c, data);
      } else {
        send_command_with_header(c->channel, data.data(), data.size());
      }
    } else {
      throw runtime_error("no client available");
    }
  }
}

CommandDefinition c_sc("sc", "sc DATA\n\
    Send a command to the client. This command also can be used to send data to\n\
    a client on the game server.",
    true, f_sc_ss);
CommandDefinition c_ss("ss", "ss DATA\n\
    Send a command to the remote server.",
    true, f_sc_ss);

CommandDefinition c_show_slots(
    "show-slots", "show-slots\n\
    Show the player names, Guild Card numbers, and client IDs of all players in\n\
    the current lobby or game.",
    true,
    +[](CommandArgs& args) {
      auto ses = args.shell->get_proxy_session(args.session_name);
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
    });

void fn_chat(CommandArgs& args) {
  auto ses = args.shell->get_proxy_session(args.session_name);
  bool is_dchat = (args.command == "dchat");

  if (!is_dchat && uses_utf16(ses->version())) {
    send_chat_message_from_client(ses->server_channel, args.args, 0);
  } else {
    string data(8, '\0');
    data.push_back('\x09');
    data.push_back('E');
    if (is_dchat) {
      data += parse_data_string(args.args, nullptr, ParseDataFlags::ALLOW_FILES);
    } else {
      data += args.args;
      data.push_back('\0');
    }
    data.resize((data.size() + 3) & (~3));
    ses->server_channel.send(0x06, 0x00, data);
  }
}
CommandDefinition c_c("c", "c TEXT", true, fn_chat);
CommandDefinition c_chat("chat", "chat TEXT\n\
    Send a chat message to the server.",
    true, fn_chat);
CommandDefinition c_dchat("dchat", "dchat DATA\n\
    Send a chat message to the server with arbitrary data in it.",
    true, fn_chat);

void fn_wchat(CommandArgs& args) {
  auto ses = args.shell->get_proxy_session(args.session_name);
  if (!is_ep3(ses->version())) {
    throw runtime_error("wchat can only be used on Episode 3");
  }
  string data(8, '\0');
  data.push_back('\x40'); // private_flags: visible to all
  data.push_back('\x09');
  data.push_back('E');
  data += args.args;
  data.push_back('\0');
  data.resize((data.size() + 3) & (~3));
  ses->server_channel.send(0x06, 0x00, data);
}
CommandDefinition c_wc("wc", "wc TEXT", true, fn_wchat);
CommandDefinition c_wchat("wchat", "wchat TEXT\n\
    Send a chat message with private_flags on Episode 3.",
    true, fn_wchat);

CommandDefinition c_marker(
    "marker", "marker COLOR-ID\n\
    Change your lobby marker color.",
    true, +[](CommandArgs& args) {
      auto ses = args.shell->get_proxy_session(args.session_name);
      ses->server_channel.send(0x89, stoul(args.args));
    });

void fn_warp(CommandArgs& args) {
  auto ses = args.shell->get_proxy_session(args.session_name);

  uint8_t floor = stoul(args.args);
  send_warp(ses->client_channel, ses->lobby_client_id, floor, true);
  if (args.command == "warpall") {
    send_warp(ses->server_channel, ses->lobby_client_id, floor, false);
  }
}
CommandDefinition c_warp("warp", "warp FLOOR-ID", true, fn_warp);
CommandDefinition c_warpme("warpme", "warpme FLOOR-ID\n\
    Send yourself to a specific floor.",
    true, fn_warp);
CommandDefinition c_warpall("warpall", "warpall FLOOR-ID\n\
    Send everyone to a specific floor.",
    true, fn_warp);

void fn_info_board(CommandArgs& args) {
  auto ses = args.shell->get_proxy_session(args.session_name);

  string data;
  if (args.command == "info-board-data") {
    data += parse_data_string(args.args, nullptr, ParseDataFlags::ALLOW_FILES);
  } else {
    data += args.args;
  }
  data.push_back('\0');
  data.resize((data.size() + 3) & (~3));

  ses->server_channel.send(0xD9, 0x00, data);
}
CommandDefinition c_info_board("info-board", "info-board TEXT\n\
    Set your info board contents. This will affect the current session only,\n\
    and will not be saved for future sessions.",
    true, fn_info_board);
CommandDefinition c_info_board_data("info-board-data", "info-board-data DATA\n\
    Set your info board contents with arbitrary data. Like the above, affects\n\
    the current session only.",
    true, fn_info_board);

CommandDefinition c_set_challenge_rank_title(
    "set-challenge-rank-title", "set-challenge-rank-title TEXT\n\
    Set the player\'s override Challenge rank text.",
    true, +[](CommandArgs& args) {
      auto ses = args.shell->get_proxy_session(args.session_name);
      ses->challenge_rank_title_override = args.args;
    });
CommandDefinition c_set_challenge_rank_color(
    "set-challenge-rank-color", "set-challenge-rank-color RRGGBBAA\n\
    Set the player\'s override Challenge rank color.",
    true, +[](CommandArgs& args) {
      auto ses = args.shell->get_proxy_session(args.session_name);
      ses->challenge_rank_color_override = stoul(args.args, nullptr, 16);
    });

void fn_create_item(CommandArgs& args) {
  auto ses = args.shell->get_proxy_session(args.session_name);

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
  ItemData item = s->parse_item_description(ses->version(), args.args);
  item.id = random_object<uint32_t>() | 0x80000000;

  if (args.command == "set-next-item") {
    ses->next_drop_item = item;

    string name = s->describe_item(ses->version(), ses->next_drop_item, true);
    send_text_message(ses->client_channel, "$C7Next drop:\n" + name);

  } else {
    send_drop_stacked_item_to_channel(s, ses->client_channel, item, ses->floor, ses->x, ses->z);
    send_drop_stacked_item_to_channel(s, ses->server_channel, item, ses->floor, ses->x, ses->z);

    string name = s->describe_item(ses->version(), ses->next_drop_item, true);
    send_text_message(ses->client_channel, "$C7Item created:\n" + name);
  }
}
CommandDefinition c_create_item("create-item", "create-item DATA\n\
    Create an item as if the client had run the $item command.",
    true, fn_create_item);
CommandDefinition c_set_next_item("set-next-item", "set-next-item DATA\n\
    Set the next item to be dropped.",
    true, fn_create_item);

CommandDefinition c_close_idle_sessions(
    "close-idle-sessions", "close-idle-sessions\n\
    Close all proxy sessions that don\'t have a client and server connected.",
    true, +[](CommandArgs& args) {
      size_t count = args.s->proxy_server->delete_disconnected_sessions();
      fprintf(stderr, "%zu sessions closed\n", count);
    });
