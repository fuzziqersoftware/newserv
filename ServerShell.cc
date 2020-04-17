#include "ServerShell.hh"

#include <event2/event.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Strings.hh>

#include "ChatCommands.hh"
#include "ServerState.hh"
#include "SendCommands.hh"

using namespace std;



ServerShell::ServerShell(std::shared_ptr<struct event_base> base,
    std::shared_ptr<ServerState> state) : Shell(base, state) { }

void ServerShell::print_prompt() {
  fwrite("newserv> ", 9, 1, stdout);
  fflush(stdout);
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
commands:\n\
  help\n\
    you\'re reading it now\n\
  exit (or ctrl+d)\n\
    shut down the server\n\
  reload <item> ...\n\
    reload data. <item> can be licenses, battle-params, level-table, or quests.\n\
  add-license <parameters>\n\
    add a license to the server. <parameters> is some subset of the following:\n\
      username=<username> (bb username)\n\
      bb-password=<password> (bb password)\n\
      gc-password=<password> (gc password)\n\
      access-key=<access-key> (gc/pc access key)\n\
      serial=<serial-number> (gc/pc serial number; required for all licenses)\n\
      privileges=<privilege-mask> (can be normal, mod, admin, root, or numeric)\n\
  delete-license <serial-number>\n\
    delete a license from the server\n\
  list-licenses\n\
    list all licenses registered on the server\n\
  set-allow-unregistered-users <true/false>\n\
    enable or disable allowing unregistered users on the server. disabling this\n\
    does not disconnect unregistered users who are already connected.\n\
  set-event <event>\n\
    set the event in all lobbies\n\
  announce <message>\n\
    send an announcement message to all players\n\
");

  } else if (command_name == "reload") {
    auto types = split(command_args, ' ');
    if (types.empty()) {
      throw invalid_argument("no data type given");
    }
    for (const string& type : types) {
      if (type == "licenses") {
        shared_ptr<LicenseManager> lm(new LicenseManager("system/licenses.nsi"));
        this->state->license_manager = lm;
      } else if (type == "battle-params") {
        shared_ptr<BattleParamTable> bpt(new BattleParamTable("system/blueburst/BattleParamEntry"));
        this->state->battle_params = bpt;
      } else if (type == "level-table") {
        shared_ptr<LevelTable> lt(new LevelTable("system/blueburst/PlyLevelTbl.prs", true));
        this->state->level_table = lt;
      } else if (type == "quests") {
        shared_ptr<QuestIndex> qi(new QuestIndex("system/quests"));
        this->state->quest_index = qi;
      } else {
        throw invalid_argument("incorrect data type");
      }
    }

  } else if (command_name == "add-license") {
    shared_ptr<License> l(new License());
    memset(l.get(), 0, sizeof(License));

    for (const string& token : split(command_args, ' ')) {
      if (starts_with(token, "username=")) {
        if (token.size() >= 29) {
          throw invalid_argument("username too long");
        }
        strcpy(l->username, token.c_str() + 9);

      } else if (starts_with(token, "bb-password=")) {
        if (token.size() >= 32) {
          throw invalid_argument("bb-password too long");
        }
        strcpy(l->bb_password, token.c_str() + 12);

      } else if (starts_with(token, "gc-password=")) {
        if (token.size() > 20) {
          throw invalid_argument("gc-password too long");
        }
        strcpy(l->gc_password, token.c_str() + 12);

      } else if (starts_with(token, "access-key=")) {
        if (token.size() > 23) {
          throw invalid_argument("access-key is too long");
        }
        strcpy(l->access_key, token.c_str() + 11);

      } else if (starts_with(token, "serial=")) {
        l->serial_number = stoul(token.substr(7));

      } else if (starts_with(token, "privileges=")) {
        string mask = token.substr(11);
        if (mask == "normal") {
          l->privileges = 0;
        } else if (mask == "mod") {
          l->privileges = Privilege::Moderator;
        } else if (mask == "admin") {
          l->privileges = Privilege::Administrator;
        } else if (mask == "root") {
          l->privileges = Privilege::Root;
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

  } else if (command_name == "announce") {
    u16string message16 = decode_sjis(command_args);
    send_text_message(this->state, message16.c_str());

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
