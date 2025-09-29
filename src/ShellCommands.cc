#include "ShellCommands.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ChatCommands.hh"
#include "GameServer.hh"
#include "ReceiveCommands.hh"
#include "ReplaySession.hh"
#include "SendCommands.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

using namespace std;

vector<const ShellCommand*> ShellCommand::commands_by_order;
unordered_map<string, const ShellCommand*> ShellCommand::commands_by_name;

exit_shell::exit_shell() : runtime_error("shell exited") {}

shared_ptr<Client> ShellCommand::Args::get_client() const {
  if (!s->game_server) {
    throw logic_error("game server is missing");
  }

  shared_ptr<Client> c;
  if (this->session_name.empty()) {
    return this->s->game_server->get_client();
  } else {
    auto clients = this->s->game_server->get_clients_by_identifier(this->session_name);
    if (clients.empty()) {
      throw runtime_error("no such client");
    }
    if (clients.size() > 1) {
      throw runtime_error("multiple clients found");
    }
    return clients[0];
  }
}

shared_ptr<Client> ShellCommand::Args::get_proxy_client() const {
  auto c = this->get_client();
  if (!c->proxy_session) {
    throw runtime_error("client is not in a proxy session");
  }
  return c;
}

ShellCommand::ShellCommand(
    const char* name,
    const char* help_text,
    asio::awaitable<deque<string>> (*run)(Args&))
    : name(name),
      help_text(help_text),
      run(run) {
  ShellCommand::commands_by_order.emplace_back(this);
  ShellCommand::commands_by_name.emplace(this->name, this);
}

asio::awaitable<deque<string>> ShellCommand::dispatch_str(shared_ptr<ServerState> s, const string& command) {
  size_t command_end = phosg::skip_non_whitespace(command, 0);
  size_t args_begin = phosg::skip_whitespace(command, command_end);
  Args args;
  args.s = s;
  args.command = command.substr(0, command_end);
  args.args = command.substr(args_begin);
  co_return co_await ShellCommand::dispatch(args);
}

asio::awaitable<deque<string>> ShellCommand::dispatch(Args& args) {
  const ShellCommand* def = nullptr;
  try {
    def = commands_by_name.at(args.command);
  } catch (const out_of_range&) {
  }
  if (!def) {
    throw runtime_error("no such command; try 'help'");
  } else {
    return def->run(args);
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
    s = s.substr(phosg::skip_whitespace(s, z + 1));
  } else {
    s = s.substr(phosg::skip_whitespace(s, z));
  }
  return ret;
}

static asio::awaitable<deque<string>> empty_handler(ShellCommand::Args&) {
  co_return deque<string>();
}

ShellCommand c_nop1("", nullptr, empty_handler);
ShellCommand c_nop2("//", nullptr, empty_handler);
ShellCommand c_nop3("#", nullptr, empty_handler);

ShellCommand c_help(
    "help", "help\n\
    You\'re reading it now.",
    +[](ShellCommand::Args&) -> asio::awaitable<deque<string>> {
      deque<string> ret({"Commands:"});
      for (const auto& def : ShellCommand::commands_by_order) {
        if (def->help_text) {
          // TODO: It's not great that we copy the text here.
          auto& s = ret.emplace_back("  ");
          s += def->help_text;
        }
      }
      co_return ret;
    });
ShellCommand c_exit(
    "exit", "exit (or ctrl+d)\n\
    Shut down the server.",
    +[](ShellCommand::Args&) -> asio::awaitable<deque<string>> {
      throw exit_shell();
    });
ShellCommand c_on(
    "on", "on SESSION COMMAND [ARGS...]\n\
    Run a command on a specific game server client or proxy server session.\n\
    Without this prefix, commands that affect a single client or session will\n\
    work only if there's exactly one connected client or open session. SESSION\n\
    may be a client ID (e.g. C-3), a player name, an account ID, an Xbox\n\
    gamertag, or a BB account username. For proxy commands, SESSION should be\n\
    the session ID, which generally is the same as the player\'s account ID\n\
    and appears after \"LinkedSession:\" in the log output.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      size_t session_name_end = phosg::skip_non_whitespace(args.args, 0);
      size_t command_begin = phosg::skip_whitespace(args.args, session_name_end);
      size_t command_end = phosg::skip_non_whitespace(args.args, command_begin);
      size_t args_begin = phosg::skip_whitespace(args.args, command_end);
      args.session_name = args.args.substr(0, session_name_end);
      args.command = args.args.substr(command_begin, command_end - command_begin);
      args.args = args.args.substr(args_begin);
      return ShellCommand::dispatch(args);
    });

ShellCommand c_reload(
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
    definitions until the battle ends. Similarly, BB clients are not forced to\n\
    disconnect or reload the battle parameters, so if these are changed without\n\
    restarting, clients may see (for example) EXP messages inconsistent with\n\
    the amounts of EXP actually received.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto types = phosg::split(args.args, ' ');
      for (const auto& type : types) {
        if (type == "all") {
          args.s->load_all(true);
        } else if (type == "bb-keys") {
          args.s->load_bb_private_keys();
        } else if (type == "accounts") {
          args.s->load_accounts();
        } else if (type == "maps") {
          args.s->load_maps();
        } else if (type == "caches") {
          args.s->clear_file_caches();
        } else if (type == "patch-files") {
          args.s->load_patch_indexes();
        } else if (type == "ep3-cards") {
          args.s->load_ep3_cards();
        } else if (type == "ep3-maps") {
          args.s->load_ep3_maps();
        } else if (type == "ep3-tournaments") {
          args.s->load_ep3_tournament_state();
        } else if (type == "functions") {
          args.s->compile_functions();
        } else if (type == "dol-files") {
          args.s->load_dol_files();
        } else if (type == "set-tables") {
          args.s->load_set_data_tables();
        } else if (type == "battle-params") {
          args.s->load_battle_params();
        } else if (type == "level-tables") {
          args.s->load_level_tables();
        } else if (type == "text-index") {
          args.s->load_text_index();
        } else if (type == "word-select") {
          args.s->load_word_select_table();
        } else if (type == "item-definitions") {
          args.s->load_item_definitions();
        } else if (type == "item-name-index") {
          args.s->load_item_name_indexes();
        } else if (type == "drop-tables") {
          args.s->load_drop_tables();
        } else if (type == "config") {
          args.s->load_config_early();
          args.s->load_config_late();
        } else if (type == "teams") {
          args.s->load_teams();
        } else if (type == "quests") {
          args.s->load_quest_index();
        } else {
          throw runtime_error("invalid data type: " + type);
        }
      }

      co_return deque<string>{};
    });

ShellCommand c_list_accounts(
    "list-accounts", "list-accounts\n\
    List all accounts registered on the server.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      deque<string> ret;
      auto accounts = args.s->account_index->all();
      if (accounts.empty()) {
        ret.emplace_back("No accounts registered");
      } else {
        for (const auto& a : accounts) {
          ret.emplace_back(a->str());
        }
      }
      co_return ret;
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

ShellCommand c_add_account(
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
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto account = make_shared<Account>();
      for (const string& token : phosg::split(args.args, ' ')) {
        if (token.starts_with("id=")) {
          account->account_id = stoul(token.substr(3), nullptr, 16);
        } else if (token.starts_with("ep3-current-meseta=")) {
          account->ep3_current_meseta = stoul(token.substr(19), nullptr, 0);
        } else if (token.starts_with("ep3-total-meseta=")) {
          account->ep3_total_meseta_earned = stoul(token.substr(17), nullptr, 0);
        } else if (token == "temporary") {
          account->is_temporary = true;
        } else if (token.starts_with("flags=")) {
          account->flags = parse_account_flags(token.substr(6));
        } else if (token.starts_with("user-flags=")) {
          account->user_flags = parse_account_user_flags(token.substr(11));
        } else {
          throw invalid_argument("invalid account field: " + token);
        }
      }
      args.s->account_index->add(account);
      account->save();
      co_return deque<string>{format("Account {:08X} added", account->account_id)};
    });
ShellCommand c_update_account(
    "update-account", "update-account ACCOUNT-ID PARAMETERS...\n\
    Update an existing license. ACCOUNT-ID (8 hex digits) specifies which\n\
    account to update. The options are similar to the add-account command:\n\
      flags=FLAGS: Sets behaviors and permissions for the account (same as\n\
          add-account).\n\
      user-flags=FLAGS: Sets behaviors for the account (same as add-account).\n\
      ban-duration=DURATION: bans this account for the specified duration; the\n\
          duration should be of the form 3d, 2w, 1mo, or 1y. If any clients\n\
          are connected with this account when this command is run, they will\n\
          be disconnected.\n\
      unban: Clears any existing ban from this account.\n\
      ep3-current-meseta=MESETA: Sets Episode 3 Meseta value.\n\
      ep3-total-meseta=MESETA: Sets Episode 3 total Meseta ever earned.\n\
      temporary: Marks the account as temporary; it is not saved to disk and\n\
          therefore will be deleted when the server shuts down.\n\
      permanent: If the account was temporary, makes it non-temporary.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
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
        if (token.starts_with("ep3-current-meseta=")) {
          new_ep3_current_meseta = stoul(token.substr(19), nullptr, 0);
        } else if (token.starts_with("ep3-total-meseta=")) {
          new_ep3_total_meseta = stoul(token.substr(17), nullptr, 0);
        } else if (token == "temporary") {
          new_is_temporary = 1;
        } else if (token == "permanent") {
          new_is_temporary = 0;
        } else if (token.starts_with("flags=")) {
          new_flags = parse_account_flags(token.substr(6));
        } else if (token.starts_with("user-flags=")) {
          new_user_flags = parse_account_user_flags(token.substr(11));
        } else if (token == "unban") {
          new_ban_duration = 0;
        } else if (token.starts_with("ban-duration=")) {
          auto duration_str = token.substr(13);
          if (duration_str.ends_with("s")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 1000000LL;
          } else if (duration_str.ends_with("m")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 60000000LL;
          } else if (duration_str.ends_with("h")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 3600000000LL;
          } else if (duration_str.ends_with("d")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 86400000000LL;
          } else if (duration_str.ends_with("w")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 1)) * 604800000000LL;
          } else if (duration_str.ends_with("mo")) {
            new_ban_duration = stoull(duration_str.substr(0, duration_str.size() - 2)) * 2952000000000LL;
          } else if (duration_str.ends_with("y")) {
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
      if (new_ban_duration > 0) {
        args.s->disconnect_all_banned_clients();
      }

      co_return deque<string>{format("Account {:08X} updated", account->account_id)};
    });
ShellCommand c_delete_account(
    "delete-account", "delete-account ACCOUNT-ID\n\
    Delete an account from the server. If a player is online with the deleted\n\
    account, they will not be automatically disconnected.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto account = args.s->account_index->from_account_id(stoul(args.args, nullptr, 16));
      args.s->account_index->remove(account->account_id);
      account->is_temporary = true;
      account->delete_file();
      co_return deque<string>{"Account deleted"};
    });

ShellCommand c_add_license(
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
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
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
      co_return deque<string>{format("Account {:08X} updated", account->account_id)};
    });
ShellCommand c_delete_license(
    "delete-license", "delete-license ACCOUNT-ID TYPE PRIMARY-CREDENTIAL\n\
    Delete a license from an account. ACCOUNT-ID and TYPE have the same\n\
    meanings as for add-license. PRIMARY-CREDENTIAL is the first credential\n\
    for the license type; specifically:\n\
      DC-NTE: PRIMARY-CREDENTIAL is the serial number\n\
      DC: PRIMARY-CREDENTIAL is the serial number (8 hex digits)\n\
      PC: PRIMARY-CREDENTIAL is the serial number (8 hex digits)\n\
      GC: PRIMARY-CREDENTIAL is the serial number (decimal)\n\
      XB: PRIMARY-CREDENTIAL is the user ID (16 hex digits)\n\
      BB: PRIMARY-CREDENTIAL is the username\n\
    Examples (deleting licenses from account 385A92C4):\n\
      delete-license 385A92C4 DC 107862F9\n\
      delete-license 385A92C4 PC 2F94C303\n\
      delete-license 385A92C4 GC 0418572923\n\
      delete-license 385A92C4 XB 7E29A2950019EB20\n\
      delete-license 385A92C4 BB user1",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
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
        args.s->account_index->remove_xb_license(account, stoull(tokens[2], nullptr, 16));
      } else if (type_str == "BB") {
        args.s->account_index->remove_bb_license(account, tokens[2]);
      } else {
        throw runtime_error("invalid license type");
      }

      account->save();
      co_return deque<string>{format("Account {:08X} updated", account->account_id)};
    });

ShellCommand c_lookup(
    "lookup", "lookup USER\n\
    Find the account for a logged-in user.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto target = args.s->find_client(&args.args);
      if (target->login) {
        co_return deque<string>{format("Found client {} with account ID {:08X}",
            target->channel->name, target->login->account->account_id)};
      } else {
        // This should be impossible
        throw logic_error("find_client found user who is not logged in");
      }
    });
ShellCommand c_kick(
    "kick", "kick USER\n\
    Disconnect a user from the server. USER may be an account ID, player name,\n\
    or client ID (beginning with \"C-\"). This does not ban the user; they are\n\
    free to reconnect after doing this.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto target = args.s->find_client(&args.args);
      send_message_box(target, "$C6You have been kicked off the server.");
      target->channel->disconnect();
      co_return deque<string>{format("Client C-{:X} disconnected from server", target->id)};
    });

ShellCommand c_announce(
    "announce", "announce MESSAGE\n\
    Send an announcement message to all players.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      send_text_or_scrolling_message(args.s, args.args, args.args);
      co_return deque<string>{};
    });
ShellCommand c_announce_mail(
    "announce-mail", "announce-mail MESSAGE\n\
    Send an announcement message via Simple Mail to all players.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      send_simple_mail(args.s, 0, args.s->name, args.args);
      co_return deque<string>{};
    });

ShellCommand c_create_tournament(
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
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      string name = get_quoted_string(args.args);
      string map_name = get_quoted_string(args.args);
      auto map = args.s->ep3_map_index->get(map_name);
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
          } else if (token.starts_with("dice=")) {
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
            auto atk_range = parse_range_p(subtokens[0]);
            rules.min_dice_value = atk_range.first;
            rules.max_dice_value = atk_range.second;
            if (subtokens.size() >= 2) {
              rules.def_dice_value_range = parse_range_c(subtokens[1]);
              if (subtokens.size() >= 3) {
                rules.atk_dice_value_range_2v1 = parse_range_c(subtokens[2]);
                if (subtokens.size() == 3) {
                  rules.def_dice_value_range_2v1 = rules.atk_dice_value_range_2v1;
                } else if (subtokens.size() == 4) {
                  rules.def_dice_value_range_2v1 = parse_range_c(subtokens[3]);
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
          } else if (token.starts_with("overall-time-limit=")) {
            uint32_t limit = stoul(token.substr(19));
            if (limit > 600) {
              throw runtime_error("overall-time-limit must be 600 or fewer minutes");
            }
            if (limit % 5) {
              throw runtime_error("overall-time-limit must be a multiple of 5 minutes");
            }
            rules.overall_time_limit = limit;
          } else if (token.starts_with("phase-time-limit=")) {
            rules.phase_time_limit = stoul(token.substr(17));
          } else if (token.starts_with("hp=")) {
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
      deque<string> ret;
      if (rules.check_and_reset_invalid_fields()) {
        ret.emplace_back("Warning: Some rules were invalid and reset to defaults");
      }
      auto tourn = args.s->ep3_tournament_index->create_tournament(name, map, rules, num_teams, flags);
      ret.emplace_back(format("Created tournament \"{}\"", tourn->get_name()));
      co_return ret;
    });

ShellCommand c_delete_tournament(
    "delete-tournament", "delete-tournament TOURNAMENT-NAME\n\
    Delete a tournament. Quotes are required around the tournament name unless\n\
    the name contains no spaces.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      string name = get_quoted_string(args.args);
      if (args.s->ep3_tournament_index->delete_tournament(name)) {
        co_return deque<string>{"Deleted tournament"};
      } else {
        throw runtime_error("tournament does not exist");
      }
    });
ShellCommand c_list_tournaments(
    "list-tournaments", "list-tournaments\n\
    List the names and numbers of all existing tournaments.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      deque<string> ret;
      for (const auto& it : args.s->ep3_tournament_index->all_tournaments()) {
        ret.emplace_back("  " + it.second->get_name());
      }
      co_return ret;
    });
ShellCommand c_start_tournament(
    "start-tournament", "start-tournament TOURNAMENT-NAME\n\
    End registration for a tournament and allow matches to begin. Quotes are\n\
    required around the tournament name unless the name contains no spaces.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      string name = get_quoted_string(args.args);
      auto tourn = args.s->ep3_tournament_index->get_tournament(name);
      if (tourn) {
        tourn->start();
        args.s->ep3_tournament_index->save();
        tourn->send_all_state_updates();
        send_ep3_text_message_fmt(args.s, "$C7The tournament\n$C6{}$C7\nhas begun", tourn->get_name());
        co_return deque<string>{"Tournament started"};
      } else {
        throw runtime_error("tournament does not exist");
      }
    });
ShellCommand c_describe_tournament(
    "describe-tournament", "describe-tournament TOURNAMENT-NAME\n\
    Show the current state of a tournament. Quotes are required around the\n\
    tournament name unless the name contains no spaces.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      string name = get_quoted_string(args.args);
      auto tourn = args.s->ep3_tournament_index->get_tournament(name);
      if (tourn) {
        co_return deque<string>{tourn->bracket_str()};
      } else {
        throw runtime_error("tournament does not exist");
      }
    });

ShellCommand c_cc(
    "cc", "cc COMMAND\n\
    Execute a chat command as if a client had sent it in-game. The command\n\
    should be specified exactly as it would be typed in-game; for example:\n\
      cc $itemnotifs rare\n\
    This command cannot send chat messages to other players or to the server\n\
    (in proxy sessions); it can only execute chat commands. Chat commands run\n\
    via this command are exempt from permission checks, so commands that\n\
    require cheat mode or debug mode are always available via cc even if the\n\
    player cannot normamlly use them.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto c = args.get_client();
      co_await on_chat_command(c, args.args, false);
      co_return deque<string>{};
    });

asio::awaitable<deque<string>> f_sc_ss(ShellCommand::Args& args) {
  string data = phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
  if (data.size() == 0) {
    throw invalid_argument("no data given");
  }
  data.resize((data.size() + 3) & (~3));

  auto c = args.get_client();
  if (args.command[1] == 's') {
    if (c->proxy_session) {
      send_command_with_header(c->proxy_session->server_channel, data.data(), data.size());
    } else {
      co_await on_command_with_header(c, data);
    }
  } else {
    send_command_with_header(c->channel, data.data(), data.size());
  }

  co_return deque<string>{};
}

ShellCommand c_sc("sc", "sc DATA\n\
    Send a network command to the client.",
    f_sc_ss);
ShellCommand c_ss("ss", "ss DATA\n\
    Send a network command to the server.",
    f_sc_ss);

ShellCommand c_show_slots(
    "show-slots", "show-slots\n\
    Show the player names, Guild Card numbers, and client IDs of all players in\n\
    the current lobby or game.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto c = args.get_proxy_client();

      deque<string> ret;
      for (size_t z = 0; z < c->proxy_session->lobby_players.size(); z++) {
        const auto& player = c->proxy_session->lobby_players[z];
        if (player.guild_card_number) {
          ret.emplace_back(format("  {}: {} => {} ({}, {}, {})",
              z, player.guild_card_number, player.name,
              char_for_language_code(player.language),
              name_for_char_class(player.char_class),
              name_for_section_id(player.section_id)));
        } else {
          ret.emplace_back(format("  {}: (no player)", z));
        }
      }
      co_return ret;
    });

asio::awaitable<deque<string>> fn_chat(ShellCommand::Args& args) {
  auto c = args.get_client();
  bool is_dchat = (args.command == "dchat");

  if (c->proxy_session) {
    if (!is_dchat && uses_utf16(c->version())) {
      send_chat_message_from_client(c->proxy_session->server_channel, args.args, 0);
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
      c->proxy_session->server_channel->send(0x06, 0x00, data);
    }
  } else if (c->login) {
    string text = is_dchat ? phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES) : args.args;
    auto l = c->require_lobby();
    for (auto& lc : l->clients) {
      if (lc) {
        send_chat_message(lc, c->login->account->account_id, c->character_file()->disp.name.decode(c->language()), text, 0);
      }
    }
  }

  co_return deque<string>{};
}
ShellCommand c_c("c", "c TEXT", fn_chat);
ShellCommand c_chat("chat", "chat TEXT\n\
    Send a chat message to the server.",
    fn_chat);
ShellCommand c_dchat("dchat", "dchat DATA\n\
    Send a chat message to the server with arbitrary data in it.",
    fn_chat);

asio::awaitable<deque<string>> fn_wchat(ShellCommand::Args& args) {
  auto c = args.get_client();
  if (!is_ep3(c->version())) {
    throw runtime_error("wchat can only be used on Episode 3");
  }
  if (c->proxy_session) {
    string data(8, '\0');
    data.push_back('\x40'); // private_flags: visible to all
    data.push_back('\x09');
    data.push_back('E');
    data += args.args;
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));
    c->proxy_session->server_channel->send(0x06, 0x00, data);
  } else if (c->login) {
    auto l = c->require_lobby();
    for (auto& lc : l->clients) {
      if (lc) {
        send_chat_message(
            lc, c->login->account->account_id, c->character_file()->disp.name.decode(c->language()), args.args, 0x40);
      }
    }
  }
  co_return deque<string>{};
}
ShellCommand c_wc("wc", "wc TEXT", fn_wchat);
ShellCommand c_wchat("wchat", "wchat TEXT\n\
    Send a chat message with private_flags on Episode 3.",
    fn_wchat);

ShellCommand c_marker(
    "marker", "marker COLOR-ID\n\
    Change your lobby marker color.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto c = args.get_proxy_client();
      c->proxy_session->server_channel->send(0x89, stoul(args.args));
      co_return deque<string>{};
    });

asio::awaitable<deque<string>> fn_warp(ShellCommand::Args& args) {
  auto c = args.get_proxy_client();
  uint8_t floor = stoul(args.args);
  send_warp(c->channel, c->lobby_client_id, floor, true);
  if (args.command == "warpall") {
    send_warp(c->proxy_session->server_channel, c->lobby_client_id, floor, false);
  }
  co_return deque<string>{};
}
ShellCommand c_warp("warp", "warp FLOOR-ID", fn_warp);
ShellCommand c_warpme("warpme", "warpme FLOOR-ID\n\
    Send yourself to a specific floor.",
    fn_warp);
ShellCommand c_warpall("warpall", "warpall FLOOR-ID\n\
    Send everyone to a specific floor.",
    fn_warp);

asio::awaitable<deque<string>> fn_info_board(ShellCommand::Args& args) {
  auto c = args.get_proxy_client();

  string data;
  if (args.command == "info-board-data") {
    data += phosg::parse_data_string(args.args, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
  } else {
    data += args.args;
  }
  data.push_back('\0');
  data.resize((data.size() + 3) & (~3));

  c->proxy_session->server_channel->send(0xD9, 0x00, data);
  co_return deque<string>{};
}
ShellCommand c_info_board("info-board", "info-board TEXT\n\
    Set your info board contents. This will affect the current session only,\n\
    and will not be saved for future sessions.",
    fn_info_board);
ShellCommand c_info_board_data("info-board-data", "info-board-data DATA\n\
    Set your info board contents with arbitrary data. Like the above, affects\n\
    the current session only.",
    fn_info_board);

ShellCommand c_create_item(
    "create-item", "create-item DATA\n\
    Create an item as if the client had run the $item command.",
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      auto c = args.get_proxy_client();

      if (c->version() == Version::BB_V4) {
        throw runtime_error("proxy session is BB");
      }
      if (!c->proxy_session->is_in_game) {
        throw runtime_error("proxy session is not in a game");
      }
      if (c->lobby_client_id != c->proxy_session->leader_client_id) {
        throw runtime_error("proxy session is not game leader");
      }

      ItemData item = args.s->parse_item_description(c->version(), args.args);
      item.id = phosg::random_object<uint32_t>() | 0x80000000;

      send_drop_stacked_item_to_channel(args.s, c->channel, item, c->floor, c->pos);
      send_drop_stacked_item_to_channel(args.s, c->proxy_session->server_channel, item, c->floor, c->pos);

      string name = args.s->describe_item(c->version(), item, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);
      send_text_message(c->channel, "$C7Item created:\n" + name);
      co_return deque<string>{};
    });

ShellCommand c_replay_log(
    "replay-log", nullptr,
    +[](ShellCommand::Args& args) -> asio::awaitable<deque<string>> {
      if (args.s->allow_saving_accounts) {
        throw runtime_error("Replays cannot be run when account saving is enabled");
      }
      auto log_f = phosg::fopen_shared(args.args, "rt");
      auto replay_session = make_shared<ReplaySession>(args.s, log_f.get(), true);
      co_await replay_session->run();
      co_return deque<string>{};
    });
