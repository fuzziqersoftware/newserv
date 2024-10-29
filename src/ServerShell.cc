#include "ServerShell.hh"

#include <event2/event.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ChatCommands.hh"
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
    string command;
    uint64_t read_start_usecs = phosg::now();
    try {
      command = phosg::fgets(stdin);
    } catch (const phosg::io_error& e) {
      // Cygwin sometimes causes fgets() to fail with errno -1 when the
      // terminal window is resized. We ignore these events unless the read
      // failed immediately (which probably means it would fail again if we
      // retried immediately).
      if (phosg::now() - read_start_usecs < 1000000 || e.error != -1) {
        throw;
      }
      phosg::log_warning("I/O error reading from terminal: %s (%d)", e.what(), e.error);
      continue;
    }

    // If command is empty (not even a \n), it's probably EOF
    if (command.empty()) {
      fputc('\n', stderr);
      event_base_loopexit(this->state->base.get(), nullptr);
      return;
    }

    phosg::strip_trailing_whitespace(command);

    try {
      // Find the entry in the command table and run the command
      size_t command_end = phosg::skip_non_whitespace(command, 0);
      size_t args_begin = phosg::skip_whitespace(command, command_end);
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
    s = s.substr(phosg::skip_whitespace(s, z + 1));
  } else {
    s = s.substr(phosg::skip_whitespace(s, z));
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
    may be a client ID (e.g. C-3), a player name, an account ID, an Xbox\n\
    gamertag, or a BB account username. For proxy commands, SESSION should be\n\
    the session ID, which generally is the same as the player\'s account ID\n\
    and appears after \"LinkedSession:\" in the log output.",
    false,
    +[](CommandArgs& args) {
      size_t session_name_end = phosg::skip_non_whitespace(args.args, 0);
      size_t command_begin = phosg::skip_whitespace(args.args, session_name_end);
      size_t command_end = phosg::skip_non_whitespace(args.args, command_begin);
      size_t args_begin = phosg::skip_whitespace(args.args, command_end);
      args.session_name = args.args.substr(0, session_name_end);
      args.command = args.args.substr(command_begin, command_end - command_begin);
      args.args = args.args.substr(args_begin);
      CommandDefinition::dispatch(args);
    });

CommandDefinition c_reload(
    "reload", "reload ITEM [ITEM...]\n\
    Reload various parts of the server configuration. The items are:\n\
      accounts - reindex user accounts\n\
      battle-params - reload the BB enemy stats files\n\
      bb-keys - reload BB private keys\n\
      caches - clear all cached files\n\
      config - reload most fields from config.json\n\
      dol-files - reindex all DOL files\n\
      drop-tables - reload drop tables\n\
      ep3-cards - reload Episode 3 card definitions\n\
      ep3-maps - reload Episode 3 maps (not download quests)\n\
      ep3-tournaments - reload Episode 3 tournament state\n\
      functions - recompile all client-side patches and functions\n\
      item-definitions - reload item definitions files\n\
      item-name-index - regenerate item name list\n\
      level-tables - reload the player stats tables\n\
      patch-files - reindex the PC and BB patch directories\n\
      quests - reindex all quests (including Episode3 download quests)\n\
      set-tables - reload set data tables\n\
      teams - reindex all BB teams\n\
      text-index - reload in-game text\n\
      word-select - regenerate the Word Select translation table\n\
      all - do all of the above\n\
    Reloading will not affect items that are in use; for example, if an Episode\n\
    3 battle is in progress, it will continue to use the previous map and card\n\
    definitions. Similarly, BB clients are not forced to disconnect or reload\n\
    the battle parameters, so if these are changed without restarting, clients\n\
    may see (for example) EXP messages inconsistent with the amounts of EXP\n\
    actually received.",
    false,
    +[](CommandArgs& args) {
      auto types = phosg::split(args.args, ' ');
      for (const auto& type : types) {
        if (type == "all") {
          args.s->forward_to_event_thread([s = args.s]() {
            try {
              s->load_all();
            } catch (const exception& e) {
              fprintf(stderr, "FAILED: %s\n", e.what());
              fprintf(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
            }
          });
        } else if (type == "bb-keys") {
          args.s->load_bb_private_keys(true);
        } else if (type == "accounts") {
          args.s->load_accounts(true);
        } else if (type == "caches") {
          args.s->clear_file_caches(true);
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
        } else if (type == "level-tables") {
          args.s->load_level_tables(true);
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
            try {
              s->load_config_early();
              s->load_config_late();
            } catch (const exception& e) {
              fprintf(stderr, "FAILED: %s\n", e.what());
              fprintf(stderr, "Some configuration may have been reloaded. Fix the underlying issue and try again.\n");
            }
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

CommandDefinition c_list_accounts(
    "list-accounts", "list-accounts\n\
    List all accounts registered on the server.",
    true,
    +[](CommandArgs& args) {
      auto accounts = args.s->account_index->all();
      if (accounts.empty()) {
        fprintf(stderr, "No accounts registered\n");
      } else {
        for (const auto& a : accounts) {
          a->print(stderr);
        }
      }
    });

uint32_t parse_account_flags(const string& flags_str) {
  try {
    size_t end_pos = 0;
    uint32_t ret = stoul(flags_str, &end_pos, 16);
    if (end_pos == flags_str.size()) {
      return ret;
    }
  } catch (const exception&) {
  }

  uint32_t ret = 0;
  auto tokens = phosg::split(flags_str, ',');
  for (const auto& token : tokens) {
    string token_upper = phosg::toupper(token);
    if (token_upper == "NONE") {
      // Nothing to do
    } else if (token_upper == "KICK_USER") {
      ret |= static_cast<uint32_t>(Account::Flag::KICK_USER);
    } else if (token_upper == "BAN_USER") {
      ret |= static_cast<uint32_t>(Account::Flag::BAN_USER);
    } else if (token_upper == "SILENCE_USER") {
      ret |= static_cast<uint32_t>(Account::Flag::SILENCE_USER);
    } else if (token_upper == "MODERATOR") {
      ret |= static_cast<uint32_t>(Account::Flag::MODERATOR);
    } else if (token_upper == "CHANGE_EVENT") {
      ret |= static_cast<uint32_t>(Account::Flag::CHANGE_EVENT);
    } else if (token_upper == "ANNOUNCE") {
      ret |= static_cast<uint32_t>(Account::Flag::ANNOUNCE);
    } else if (token_upper == "FREE_JOIN_GAMES") {
      ret |= static_cast<uint32_t>(Account::Flag::FREE_JOIN_GAMES);
    } else if (token_upper == "ADMINISTRATOR") {
      ret |= static_cast<uint32_t>(Account::Flag::ADMINISTRATOR);
    } else if (token_upper == "DEBUG") {
      ret |= static_cast<uint32_t>(Account::Flag::DEBUG);
    } else if (token_upper == "CHEAT_ANYWHERE") {
      ret |= static_cast<uint32_t>(Account::Flag::CHEAT_ANYWHERE);
    } else if (token_upper == "DISABLE_QUEST_REQUIREMENTS") {
      ret |= static_cast<uint32_t>(Account::Flag::DISABLE_QUEST_REQUIREMENTS);
    } else if (token_upper == "ALWAYS_ENABLE_CHAT_COMMANDS") {
      ret |= static_cast<uint32_t>(Account::Flag::ALWAYS_ENABLE_CHAT_COMMANDS);
    } else if (token_upper == "ROOT") {
      ret |= static_cast<uint32_t>(Account::Flag::ROOT);
    } else if (token_upper == "IS_SHARED_ACCOUNT") {
      ret |= static_cast<uint32_t>(Account::Flag::IS_SHARED_ACCOUNT);
    } else {
      throw runtime_error("invalid flag name: " + token_upper);
    }
  }
  return ret;
}

uint32_t parse_account_user_flags(const string& user_flags_str) {
  try {
    size_t end_pos = 0;
    uint32_t ret = stoul(user_flags_str, &end_pos, 16);
    if (end_pos == user_flags_str.size()) {
      return ret;
    }
  } catch (const exception&) {
  }

  uint32_t ret = 0;
  auto tokens = phosg::split(user_flags_str, ',');
  for (const auto& token : tokens) {
    string token_upper = phosg::toupper(token);
    if (token_upper == "NONE") {
      // Nothing to do
    } else if (token_upper == "DISABLE_DROP_NOTIFICATION_BROADCAST") {
      ret |= static_cast<uint32_t>(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST);
    } else {
      throw runtime_error("invalid user flag name: " + token_upper);
    }
  }
  return ret;
}

CommandDefinition c_add_account(
    "add-account", "add-account [PARAMETERS...]\n\
    Add an account to the server. <parameters> is some subset of:\n\
      id=ACCOUNT-ID: preferred account ID in hex (optional)\n\
      flags=FLAGS: behaviors and permissions for the account (see below)\n\
      user-flags=FLAGS: user-set behaviors for the account\n\
      ep3-current-meseta=MESETA: Episode 3 Meseta value\n\
      ep3-total-meseta=MESETA: Episode 3 total Meseta ever earned\n\
      temporary: marks the account as temporary; it is not saved to disk and\n\
          therefore will be deleted when the server shuts down\n\
    If given, FLAGS is a comma-separated list of zero or more the following:\n\
      NONE: Placeholder if no other flags are specified\n\
      KICK_USER: Can kick other users offline\n\
      BAN_USER: Can ban other users\n\
      SILENCE_USER: Can silence other users\n\
      MODERATOR: Alias for all of the above flags\n\
      CHANGE_EVENT: Can change lobby events\n\
      ANNOUNCE: Can make server-wide announcements\n\
      FREE_JOIN_GAMES: Ignores game restrictions (level/quest requirements)\n\
      ADMINISTRATOR: Alias for all of the above flags (including MODERATOR)\n\
      DEBUG: Can use debugging commands\n\
      CHEAT_ANYWHERE: Can use cheat commands even if cheat mode is disabled\n\
      DISABLE_QUEST_REQUIREMENTS: Can play any quest without progression\n\
          restrictions\n\
      ALWAYS_ENABLE_CHAT_COMMANDS: Can use chat commands even if they are\n\
          disabled in config.json\n\
      ROOT: Alias for all of the above flags (including ADMINISTRATOR)\n\
      IS_SHARED_ACCOUNT: Account is a shared serial (disables Access Key and\n\
          password checks; players will get Guild Cards based on their player\n\
          names)",
    true,
    +[](CommandArgs& args) {
      auto account = make_shared<Account>();
      for (const string& token : phosg::split(args.args, ' ')) {
        if (phosg::starts_with(token, "id=")) {
          account->account_id = stoul(token.substr(3), nullptr, 16);
        } else if (phosg::starts_with(token, "ep3-current-meseta=")) {
          account->ep3_current_meseta = stoul(token.substr(19), nullptr, 0);
        } else if (phosg::starts_with(token, "ep3-total-meseta=")) {
          account->ep3_total_meseta_earned = stoul(token.substr(17), nullptr, 0);
        } else if (token == "temporary") {
          account->is_temporary = true;
        } else if (phosg::starts_with(token, "flags=")) {
          account->flags = parse_account_flags(token.substr(6));
        } else if (phosg::starts_with(token, "user-flags=")) {
          account->user_flags = parse_account_user_flags(token.substr(11));
        } else {
          throw invalid_argument("invalid account field: " + token);
        }
      }
      args.s->account_index->add(account);
      account->save();
      fprintf(stderr, "Account %08" PRIX32 " added\n", account->account_id);
    });
CommandDefinition c_update_account(
    "update-account", "update-account ACCOUNT-ID PARAMETERS...\n\
    Update an existing license. ACCOUNT-ID (8 hex digits) specifies which\n\
    account to update. The options are similar to the add-account command:\n\
      flags=FLAGS: sets behaviors and permissions for the account (same as\n\
          add-account)\n\
      user-flags=FLAGS: sets behaviors for the account (same as add-account)\n\
      ban-duration=DURATION: bans this account for the specified duration; the\n\
          duration should be of the form 3d, 2w, 1mo, or 1y\n\
      unban: clears any existing ban from this account\n\
      ep3-current-meseta=MESETA: sets Episode 3 Meseta value\n\
      ep3-total-meseta=MESETA: sets Episode 3 total Meseta ever earned\n\
      temporary: marks the account as temporary; it is not saved to disk and\n\
          therefore will be deleted when the server shuts down\n\
      permanent: if the account was temporary, makes it non-temporary",
    true,
    +[](CommandArgs& args) {
      auto tokens = phosg::split(args.args, ' ');
      if (tokens.size() < 2) {
        throw runtime_error("not enough arguments");
      }
      auto account = args.s->account_index->from_account_id(stoul(tokens[0], nullptr, 16));
      tokens.erase(tokens.begin());

      // Do all the parsing first, then the updates afterward, so we won't
      // partially update the account if parsing a later option fails
      int64_t new_ep3_current_meseta = -1;
      int64_t new_ep3_total_meseta = -1;
      int64_t new_flags = -1;
      int64_t new_user_flags = -1;
      uint8_t new_is_temporary = 0xFF;
      int64_t new_ban_duration = -1;
      for (const string& token : tokens) {
        if (phosg::starts_with(token, "ep3-current-meseta=")) {
          new_ep3_current_meseta = stoul(token.substr(19), nullptr, 0);
        } else if (phosg::starts_with(token, "ep3-total-meseta=")) {
          new_ep3_total_meseta = stoul(token.substr(17), nullptr, 0);
        } else if (token == "temporary") {
          new_is_temporary = 1;
        } else if (token == "permanent") {
          new_is_temporary = 0;
        } else if (phosg::starts_with(token, "flags=")) {
          new_flags = parse_account_flags(token.substr(6));
        } else if (phosg::starts_with(token, "user-flags=")) {
          new_user_flags = parse_account_user_flags(token.substr(11));
        } else if (token == "unban") {
          new_ban_duration = 0;
        } else if (phosg::starts_with(token, "ban-duration=")) {
          auto duration_str = token.substr(13);
          if (phosg::ends_with(duration_str, "s")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 1000000LL;
          } else if (phosg::ends_with(duration_str, "m")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 60000000LL;
          } else if (phosg::ends_with(duration_str, "h")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 3600000000LL;
          } else if (phosg::ends_with(duration_str, "d")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 86400000000LL;
          } else if (phosg::ends_with(duration_str, "w")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 604800000000LL;
          } else if (phosg::ends_with(duration_str, "mo")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 2)) * 2952000000000LL;
          } else if (phosg::ends_with(duration_str, "y")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 31536000000000LL;
          } else {
            throw runtime_error("invalid time unit");
          }
        } else {
          throw invalid_argument("invalid account field: " + token);
        }
      }

      if (new_ban_duration >= 0) {
        account->ban_end_time = phosg::now() + new_ban_duration;
      }
      if (new_ep3_current_meseta >= 0) {
        account->ep3_current_meseta = new_ep3_current_meseta;
      }
      if (new_ep3_total_meseta >= 0) {
        account->ep3_total_meseta_earned = new_ep3_total_meseta;
      }
      if (new_flags >= 0) {
        account->flags = new_flags;
      }
      if (new_user_flags >= 0) {
        account->user_flags = new_user_flags;
      }
      if (new_is_temporary != 0xFF) {
        account->is_temporary = new_is_temporary;
      }

      account->save();
      fprintf(stderr, "Account %08" PRIX32 " updated\n", account->account_id);

      if (new_ban_duration > 0) {
        args.s->disconnect_all_banned_clients();
      }
    });
CommandDefinition c_delete_account(
    "delete-account", "delete-account ACCOUNT-ID\n\
    Delete an account from the server. If a player is online with the deleted\n\
    account, they will not be automatically disconnected.",
    true,
    +[](CommandArgs& args) {
      auto account = args.s->account_index->from_account_id(stoul(args.args, nullptr, 16));
      args.s->account_index->remove(account->account_id);
      account->is_temporary = true;
      account->delete_file();
      fprintf(stderr, "Account deleted\n");
    });

CommandDefinition c_add_license(
    "add-license", "add-license ACCOUNT-ID TYPE CREDENTIALS...\n\
    Add a license to an account. Each account may have multiple licenses of\n\
    each type. The types are:\n\
      DC-NTE: CREDENTIALS is serial number and access key (16 characters each)\n\
      DC: CREDENTIALS is serial number and access key (8 characters each)\n\
      PC: CREDENTIALS is serial number and access key (8 characters each)\n\
      GC: CREDENTIALS is serial number (10 digits), access key (12 digits), and\n\
          password (up to 8 characters)\n\
      XB: CREDENTIALS is gamertag (up to 16 characters), user ID (16 hex\n\
          digits), and account ID (16 hex digits)\n\
      BB: CREDENTIALS is username and password (up to 16 characters each)\n\
    Examples (adding licenses to account 385A92C4):\n\
      add-license 385A92C4 DC 107862F9 d38XTu2p\n\
      add-license 385A92C4 GC 0418572923 282949185033 hunter2\n\
      add-license 385A92C4 BB user1 trustno1",
    true,
    +[](CommandArgs& args) {
      auto tokens = phosg::split(args.args, ' ');
      if (tokens.size() < 3) {
        throw runtime_error("not enough arguments");
      }

      auto account = args.s->account_index->from_account_id(stoul(tokens[0], nullptr, 16));

      string type_str = phosg::toupper(tokens[1]);
      if (type_str == "DC-NTE") {
        if (tokens.size() != 4) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<DCNTELicense>();
        license->serial_number = std::move(tokens[2]);
        license->access_key = std::move(tokens[3]);
        args.s->account_index->add_dc_nte_license(account, license);

      } else if (type_str == "DC") {
        if (tokens.size() != 4) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<V1V2License>();
        license->serial_number = stoul(tokens[2], nullptr, 16);
        license->access_key = std::move(tokens[3]);
        args.s->account_index->add_dc_license(account, license);

      } else if (type_str == "PC") {
        if (tokens.size() != 4) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<V1V2License>();
        license->serial_number = stoul(tokens[2], nullptr, 16);
        license->access_key = std::move(tokens[3]);
        args.s->account_index->add_pc_license(account, license);

      } else if (type_str == "GC") {
        if (tokens.size() != 5) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<GCLicense>();
        license->serial_number = stoul(tokens[2], nullptr, 10);
        license->access_key = std::move(tokens[3]);
        license->password = std::move(tokens[4]);
        args.s->account_index->add_gc_license(account, license);

      } else if (type_str == "XB") {
        if (tokens.size() != 5) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<XBLicense>();
        license->gamertag = std::move(tokens[2]);
        license->user_id = stoull(tokens[3], nullptr, 16);
        license->account_id = stoull(tokens[4], nullptr, 16);
        args.s->account_index->add_xb_license(account, license);

      } else if (type_str == "BB") {
        if (tokens.size() != 4) {
          throw runtime_error("incorrect number of parameters");
        }
        auto license = make_shared<BBLicense>();
        license->username = std::move(tokens[2]);
        license->password = std::move(tokens[3]);
        args.s->account_index->add_bb_license(account, license);

      } else {
        throw runtime_error("invalid license type");
      }

      account->save();
      fprintf(stderr, "Account %08" PRIX32 " updated\n", account->account_id);
    });
CommandDefinition c_delete_license(
    "delete-license", "delete-license ACCOUNT-ID TYPE PRIMARY-CREDENTIAL\n\
    Delete a license from an account. ACCOUNT-ID and TYPE have the same\n\
    meanings as for add-license. PRIMARY-CREDENTIAL is the first credential\n\
    for the license type; specifically:\n\
      DC-NTE: PRIMARY-CREDENTIAL is the serial number\n\
      DC: PRIMARY-CREDENTIAL is the serial number (hex)\n\
      PC: PRIMARY-CREDENTIAL is the serial number (hex)\n\
      GC: PRIMARY-CREDENTIAL is the serial number (decimal)\n\
      XB: PRIMARY-CREDENTIAL is the gamertag\n\
      BB: PRIMARY-CREDENTIAL is the username\n\
    Examples (deleting licenses from account 385A92C4):\n\
      delete-license 385A92C4 DC 107862F9\n\
      delete-license 385A92C4 GC 0418572923\n\
      delete-license 385A92C4 BB user1",
    true,
    +[](CommandArgs& args) {
      auto tokens = phosg::split(args.args, ' ');
      if (tokens.size() != 3) {
        throw runtime_error("incorrect argument count");
      }

      auto account = args.s->account_index->from_account_id(stoul(tokens[0], nullptr, 16));

      string type_str = phosg::toupper(tokens[1]);
      if (type_str == "DC-NTE") {
        args.s->account_index->remove_dc_nte_license(account, tokens[2]);
      } else if (type_str == "DC") {
        args.s->account_index->remove_dc_license(account, stoul(tokens[2], nullptr, 16));
      } else if (type_str == "PC") {
        args.s->account_index->remove_pc_license(account, stoul(tokens[2], nullptr, 16));
      } else if (type_str == "GC") {
        args.s->account_index->remove_gc_license(account, stoul(tokens[2], nullptr, 0));
      } else if (type_str == "XB") {
        args.s->account_index->remove_xb_license(account, tokens[2]);
      } else if (type_str == "BB") {
        args.s->account_index->remove_bb_license(account, tokens[2]);
      } else {
        throw runtime_error("invalid license type");
      }

      account->save();
      fprintf(stderr, "Account %08" PRIX32 " updated\n", account->account_id);
    });

CommandDefinition c_announce(
    "announce", "announce MESSAGE\n\
    Send an announcement message to all players.",
    true,
    +[](CommandArgs& args) {
      send_text_message(args.s, args.args);
    });
CommandDefinition c_announce_mail(
    "announce-mail", "announce-mail MESSAGE\n\
    Send an announcement message via Simple Mail to all players.",
    true,
    +[](CommandArgs& args) {
      send_simple_mail(args.s, 0, args.s->name, args.args);
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
        auto tokens = phosg::split(args.args, ' ');
        for (auto& token : tokens) {
          token = phosg::tolower(token);
          if (token == "2v2") {
            flags |= Episode3::Tournament::Flag::IS_2V2;
          } else if (token == "no-coms") {
            flags &= (~Episode3::Tournament::Flag::HAS_COM_TEAMS);
          } else if (token == "shuffle") {
            flags |= Episode3::Tournament::Flag::SHUFFLE_ENTRIES;
          } else if (token == "resize") {
            flags |= Episode3::Tournament::Flag::RESIZE_ON_START;
          } else if (phosg::starts_with(token, "dice=")) {
            auto parse_range_c = +[](const string& s) -> uint8_t {
              auto tokens = phosg::split(s, '-');
              if (tokens.size() != 2) {
                throw runtime_error("dice spec must be of the form MIN-MAX");
              }
              return (stoul(tokens[0]) << 4) | (stoul(tokens[1]) & 0x0F);
            };
            auto parse_range_p = +[](const string& s) -> pair<uint8_t, uint8_t> {
              auto tokens = phosg::split(s, '-');
              if (tokens.size() != 2) {
                throw runtime_error("dice spec must be of the form MIN-MAX");
              }
              return make_pair(stoul(tokens[0]), stoul(tokens[1]));
            };

            auto subtokens = phosg::split(token.substr(5), ':');
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
          } else if (phosg::starts_with(token, "overall-time-limit=")) {
            uint32_t limit = stoul(token.substr(19));
            if (limit > 600) {
              throw runtime_error("overall-time-limit must be 600 or fewer minutes");
            }
            if (limit % 5) {
              throw runtime_error("overall-time-limit must be a multiple of 5 minutes");
            }
            rules.overall_time_limit = limit;
          } else if (phosg::starts_with(token, "phase-time-limit=")) {
            rules.phase_time_limit = stoul(token.substr(17));
          } else if (phosg::starts_with(token, "hp=")) {
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
        fprintf(stderr, "Warning: Some rules were invalid and reset to defaults\n");
      }
      auto tourn = args.s->ep3_tournament_index->create_tournament(name, map, rules, num_teams, flags);
      fprintf(stderr, "Created tournament \"%s\"\n", tourn->get_name().c_str());
    });

CommandDefinition c_delete_tournament(
    "delete-tournament", "delete-tournament TOURNAMENT-NAME\n\
    Delete a tournament. Quotes are required around the tournament name unless\n\
    the name contains no spaces.",
    true,
    +[](CommandArgs& args) {
      string name = get_quoted_string(args.args);
      if (args.s->ep3_tournament_index->delete_tournament(name)) {
        fprintf(stderr, "Deleted tournament\n");
      } else {
        fprintf(stderr, "No such tournament exists\n");
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
        fprintf(stderr, "Tournament started\n");
      } else {
        fprintf(stderr, "No such tournament exists\n");
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
        fprintf(stderr, "No such tournament exists\n");
      }
    });

CommandDefinition c_cc(
    "cc", "cc COMMAND\n\
    Execute a chat command as if a client had sent it in-game. The command\n\
    should be specified exactly as it would be typed in-game; for example:\n\
      cc $itemnotifs on\n\
    This command cannot send chat messages to other players or to the server\n\
    (in proxy sessions); it can only execute chat commands.",
    true,
    +[](CommandArgs& args) {
      shared_ptr<ProxyServer::LinkedSession> ses;
      try {
        ses = args.shell->get_proxy_session(args.session_name);
      } catch (const exception&) {
      }

      if (ses.get()) {
        on_chat_command(ses, args.args);
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
          on_chat_command(c, args.args);
        } else {
          throw runtime_error("no client available");
        }
      }
      return;
    });

void f_sc_ss(CommandArgs& args) {
  string data = phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
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
      data += phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
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
    data += phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
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
  item.id = phosg::random_object<uint32_t>() | 0x80000000;

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
      if (args.s->proxy_server) {
        size_t count = args.s->proxy_server->delete_disconnected_sessions();
        fprintf(stderr, "%zu sessions closed\n", count);
      } else {
        throw runtime_error("the proxy server is disabled");
      }
    });
