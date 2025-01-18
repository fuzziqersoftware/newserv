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

precondition_failed precondition_failed_printf(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  string ret = phosg::string_vprintf(fmt, va);
  va_end(va);
  return precondition_failed(ret);
}

struct Args {
  std::string text;
  bool check_permissions;
};

struct ServerArgs : public Args {
  std::shared_ptr<Client> c;

  void check_version(Version version) const {
    if (this->c->version() != version) {
      throw precondition_failed("$C6This command cannot\nbe used for your\nversion of PSO.");
    }
  }

  void check_is_ep3(bool is_ep3) const {
    if (::is_ep3(this->c->version()) != is_ep3) {
      throw precondition_failed(
          is_ep3 ? "$C6This command can only\nbe used in Episode 3." : "$C6This command cannot\nbe used in Episode 3.");
    }
  }

  void check_is_game(bool is_game) const {
    if (this->c->require_lobby()->is_game() != is_game) {
      throw precondition_failed(
          is_game ? "$C6This command cannot\nbe used in lobbies." : "$C6This command cannot\nbe used in games.");
    }
  }

  void check_account_flag(Account::Flag flag) const {
    if (!this->c->login) {
      throw precondition_failed("$C6You are not\nlogged in.");
    }
    if (!this->c->login->account->check_flag(flag)) {
      throw precondition_failed("$C6You do not have\npermission to\nrun this command.");
    }
  }

  void check_debug_enabled() const {
    if (this->check_permissions && !this->c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      throw precondition_failed("$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    }
  }

  void check_cheats_enabled(bool behavior_is_cheating) const {
    if (behavior_is_cheating &&
        this->check_permissions &&
        !this->c->require_lobby()->check_flag(Lobby::Flag::CHEATS_ENABLED) &&
        !this->c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)) {
      throw precondition_failed("$C6This command can\nonly be used in\ncheat mode.");
    }
  }

  void check_cheats_allowed(bool behavior_is_cheating) const {
    if (behavior_is_cheating &&
        this->check_permissions &&
        (this->c->require_server_state()->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) &&
        (!this->c->login || !this->c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE))) {
      throw precondition_failed("$C6Cheats are disabled\non this server.");
    }
  }

  void check_is_leader() const {
    if (this->check_permissions && (this->c->require_lobby()->leader_id != this->c->lobby_client_id)) {
      throw precondition_failed("$C6This command can\nonly be used by\nthe game leader.");
    }
  }
};

struct ProxyArgs : public Args {
  shared_ptr<ProxyServer::LinkedSession> ses;

  void check_cheats_allowed(bool behavior_is_cheating) const {
    if (behavior_is_cheating &&
        this->check_permissions &&
        (this->ses->require_server_state()->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) &&
        (!this->ses->login || !this->ses->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE))) {
      throw precondition_failed("$C6Cheats are disabled\non this proxy.");
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// Command definitions

struct ChatCommandDefinition {
  using ServerHandler = void (*)(const ServerArgs& args);
  using ProxyHandler = void (*)(const ProxyArgs& args);

  std::vector<const char*> names;
  ServerHandler server_handler;
  ProxyHandler proxy_handler;

  static unordered_map<string, const ChatCommandDefinition*> all_defs;

  ChatCommandDefinition(std::initializer_list<const char*> names, ServerHandler server_handler, ProxyHandler proxy_handler)
      : names(names), server_handler(server_handler), proxy_handler(proxy_handler) {
    for (const char* name : this->names) {
      if (!this->all_defs.emplace(name, this).second) {
        throw logic_error("duplicate command definition: " + string(name));
      }
    }
  }
};

unordered_map<string, const ChatCommandDefinition*> ChatCommandDefinition::all_defs;

void unavailable_on_game_server(const ServerArgs&) {
  throw precondition_failed("$C6This command is\nnot available on\nthe game server");
}

void unavailable_on_proxy_server(const ProxyArgs&) {
  throw precondition_failed("$C6This command is\nnot available on\nthe proxy server");
}

////////////////////////////////////////////////////////////////////////////////
// All commands (in alphabetical order)

ChatCommandDefinition cc_allevent(
    {"$allevent"},
    +[](const ServerArgs& a) -> void {
      a.check_account_flag(Account::Flag::CHANGE_EVENT);

      uint8_t new_event = event_for_name(a.text);
      if (new_event == 0xFF) {
        throw precondition_failed("$C6No such lobby event");
      }

      auto s = a.c->require_server_state();
      for (auto l : s->all_lobbies()) {
        if (l->is_game() || !l->check_flag(Lobby::Flag::DEFAULT)) {
          continue;
        }

        l->event = new_event;
        send_change_event(l, l->event);
      }
    },
    unavailable_on_proxy_server);

static void server_command_announce_inner(const ServerArgs& a, bool mail, bool anonymous) {
  auto s = a.c->require_server_state();
  a.check_account_flag(Account::Flag::ANNOUNCE);
  if (anonymous) {
    if (mail) {
      send_simple_mail(s, 0, s->name, a.text);
    } else {
      send_text_or_scrolling_message(s, a.text, a.text);
    }
  } else {
    auto from_name = a.c->character()->disp.name.decode(a.c->language());
    if (mail) {
      send_simple_mail(s, 0, from_name, a.text);
    } else {
      auto message = from_name + ": " + a.text;
      send_text_or_scrolling_message(s, message, message);
    }
  }
}

ChatCommandDefinition cc_ann_named(
    {"$ann"},
    +[](const ServerArgs& a) {
      server_command_announce_inner(a, false, false);
    },
    unavailable_on_proxy_server);
ChatCommandDefinition cc_ann_anonymous(
    {"$ann?"},
    +[](const ServerArgs& a) {
      server_command_announce_inner(a, false, true);
    },
    unavailable_on_proxy_server);
ChatCommandDefinition cc_ann_mail_named(
    {"$ann!"},
    +[](const ServerArgs& a) {
      server_command_announce_inner(a, true, false);
    },
    unavailable_on_proxy_server);
ChatCommandDefinition cc_ann_mail_anonymous(
    {"$ann?!", "$ann!?"},
    +[](const ServerArgs& a) {
      server_command_announce_inner(a, true, true);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_ann_mail_anonymous2(
    {"$announcerares"},
    +[](const ServerArgs& a) {
      a.c->login->account->toggle_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST);
      a.c->login->account->save();
      send_text_message_printf(a.c, "$C6Rare announcements\n%s for your\nitems",
          a.c->login->account->check_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST) ? "disabled" : "enabled");
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_arrow(
    {"$arrow"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      a.c->lobby_arrow_color = stoull(a.text, nullptr, 0);
      if (!l->is_game()) {
        send_arrow_update(l);
      }
    },
    +[](const ProxyArgs& a) {
      a.ses->server_channel.send(0x89, stoull(a.text, nullptr, 0));
    });

ChatCommandDefinition cc_auction(
    {"$auction"},
    +[](const ServerArgs& a) {
      a.check_account_flag(Account::Flag::DEBUG);

      auto l = a.c->require_lobby();
      if (l->is_game() && l->is_ep3()) {
        G_InitiateCardAuction_Ep3_6xB5x42 cmd;
        cmd.header.sender_client_id = a.c->lobby_client_id;
        send_command_t(l, 0xC9, 0x00, cmd);
      }
    },
    +[](const ProxyArgs& a) {
      G_InitiateCardAuction_Ep3_6xB5x42 cmd;
      cmd.header.sender_client_id = a.ses->lobby_client_id;
      a.ses->client_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
      a.ses->server_channel.send(0xC9, 0x00, &cmd, sizeof(cmd));
    });

static string name_for_client(shared_ptr<Client> c) {
  auto player = c->character(false);
  if (player.get()) {
    return escape_player_name(player->disp.name.decode(player->inventory.language));
  }

  if (c->login) {
    return phosg::string_printf("SN:%" PRIu32, c->login->account->account_id);
  }

  return "Player";
}

ChatCommandDefinition cc_ban(
    {"$ban"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_account_flag(Account::Flag::BAN_USER);

      size_t space_pos = a.text.find(' ');
      if (space_pos == string::npos) {
        throw precondition_failed("$C6Incorrect arguments");
      }

      string identifier = a.text.substr(space_pos + 1);
      auto target = s->find_client(&identifier);
      if (!target->login) {
        // This should be impossible, but I'll bet it's not actually
        throw precondition_failed("$C6Client not logged in");
      }

      if (target->login->account->check_flag(Account::Flag::BAN_USER)) {
        throw precondition_failed("$C6You do not have\nsufficient privileges.");
      }
      if (a.c == target) {
        // This shouldn't be possible because you need BAN_USER to get here,
        // but the target can't have BAN_USER if we get here, so if a.c and
        // target are the same, one of the preceding conditions must be false.
        throw logic_error("client attempts to ban themself");
      }

      uint64_t usecs = stoull(a.text, nullptr, 0) * 1000000;

      size_t unit_offset = 0;
      for (; isdigit(a.text[unit_offset]); unit_offset++)
        ;
      if (a.text[unit_offset] == 'm') {
        usecs *= 60;
      } else if (a.text[unit_offset] == 'h') {
        usecs *= 60 * 60;
      } else if (a.text[unit_offset] == 'd') {
        usecs *= 60 * 60 * 24;
      } else if (a.text[unit_offset] == 'w') {
        usecs *= 60 * 60 * 24 * 7;
      } else if (a.text[unit_offset] == 'M') {
        usecs *= 60 * 60 * 24 * 30;
      } else if (a.text[unit_offset] == 'y') {
        usecs *= 60 * 60 * 24 * 365;
      }

      target->login->account->ban_end_time = phosg::now() + usecs;
      target->login->account->save();
      send_message_box(target, "$C6You have been banned.");
      string target_name = name_for_client(target);
      s->game_server->disconnect_client(target);
      send_text_message_printf(a.c, "$C6%s banned", target_name.c_str());
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_bank(
    {"$bank"},
    +[](const ServerArgs& a) {
      a.check_version(Version::BB_V4);

      if (a.c->config.check_flag(Client::Flag::AT_BANK_COUNTER)) {
        throw runtime_error("cannot change banks while at the bank counter");
      }
      if (a.c->has_overlay()) {
        throw runtime_error("cannot change banks while Battle or Challenge is in progress");
      }

      ssize_t new_char_index = a.text.empty() ? (a.c->bb_character_index + 1) : stol(a.text, nullptr, 0);

      if (new_char_index == 0) {
        if (a.c->use_shared_bank()) {
          send_text_message(a.c, "$C6Using shared bank (0)");
        } else {
          send_text_message(a.c, "$C6Created shared bank (0)");
        }
      } else if (new_char_index <= 4) {
        a.c->use_character_bank(new_char_index - 1);
        auto bp = a.c->current_bank_character();

        auto name = escape_player_name(bp->disp.name.decode(a.c->language()));
        send_text_message_printf(a.c, "$C6Using %s\'s bank (%zu)", name.c_str(), new_char_index);
      } else {
        throw precondition_failed("$C6Invalid bank number");
      }

      auto& bank = a.c->current_bank();
      bank.assign_ids(0x99000000 + (a.c->lobby_client_id << 20));
      a.c->log.info("Assigned bank item IDs");
      a.c->print_bank(stderr);

      send_text_message_printf(a.c, "%" PRIu32 " items\n%" PRIu32 " Meseta", bank.num_items.load(), bank.meseta.load());
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_battle(
    {"$battle"},
    +[](const ServerArgs& a) {
      a.check_is_game(false);
      if (!is_v1(a.c->version())) {
        throw precondition_failed("$C6This command can\nonly be used on\nDC v1 and earlier");
      }

      a.c->config.toggle_flag(Client::Flag::FORCE_BATTLE_MODE_GAME);
      if (a.c->config.check_flag(Client::Flag::FORCE_BATTLE_MODE_GAME)) {
        send_text_message(a.c, "$C6Battle mode enabled\nfor next game");
      } else {
        send_text_message(a.c, "$C6Battle mode disabled\nfor next game");
      }
    },
    unavailable_on_proxy_server);

static void server_command_bbchar_savechar(const ServerArgs& a, bool is_bb_conversion) {
  auto s = a.c->require_server_state();
  auto l = a.c->require_lobby();
  a.check_is_game(false);

  if (is_bb_conversion && is_ep3(a.c->version())) {
    throw precondition_failed("$C6Episode 3 players\ncannot be converted\nto BB format");
  }

  auto pending_export = make_unique<Client::PendingCharacterExport>();

  if (is_bb_conversion) {
    vector<string> tokens = phosg::split(a.text, ' ');
    if (tokens.size() != 3) {
      throw precondition_failed("$C6Incorrect argument\ncount");
    }

    // username/password are tokens[0] and [1]
    pending_export->character_index = stoll(tokens[2]) - 1;
    if ((pending_export->character_index > 3) || (pending_export->character_index < 0)) {
      throw precondition_failed("$C6Player index must\nbe in range 1-4");
    }

    try {
      auto dest_login = s->account_index->from_bb_credentials(tokens[0], &tokens[1], false);
      pending_export->dest_account = dest_login->account;
      pending_export->dest_bb_license = dest_login->bb_license;
    } catch (const exception& e) {
      throw precondition_failed_printf("$C6Login failed: %s", e.what());
    }

  } else {
    pending_export->character_index = stoll(a.text) - 1;
    if ((pending_export->character_index > 15) || (pending_export->character_index < 0)) {
      throw precondition_failed("$C6Player index must\nbe in range 1-16");
    }
    pending_export->dest_account = a.c->login->account;
  }

  a.c->pending_character_export = std::move(pending_export);

  // Request the player data. The client will respond with a 61 or 30, and the
  // handler for either of those commands will execute the conversion
  send_get_player_info(a.c, true);
}

ChatCommandDefinition cc_bbchar(
    {"$bbchar"},
    +[](const ServerArgs& a) {
      server_command_bbchar_savechar(a, true);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_cheat(
    {"$cheat"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_leader();
      if (a.check_permissions && l->check_flag(Lobby::Flag::CANNOT_CHANGE_CHEAT_MODE)) {
        throw precondition_failed("$C6Cheat mode cannot\nbe changed on this\nserver");
      } else {
        l->toggle_flag(Lobby::Flag::CHEATS_ENABLED);
        send_text_message_printf(l, "Cheat mode %s", l->check_flag(Lobby::Flag::CHEATS_ENABLED) ? "enabled" : "disabled");

        if (!l->check_flag(Lobby::Flag::CHEATS_ENABLED) &&
            !a.c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE) &&
            s->cheat_flags.insufficient_minimum_level) {
          size_t default_min_level = s->default_min_level_for_game(a.c->version(), l->episode, l->difficulty);
          if (l->min_level < default_min_level) {
            l->min_level = default_min_level;
            send_text_message_printf(l, "$C6Minimum level set\nto %" PRIu32, l->min_level + 1);
          }
        }
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_debug(
    {"$debug"},
    +[](const ServerArgs& a) {
      a.check_account_flag(Account::Flag::DEBUG);
      a.c->config.toggle_flag(Client::Flag::DEBUG_ENABLED);
      send_text_message_printf(a.c, "Debug %s",
          (a.c->config.check_flag(Client::Flag::DEBUG_ENABLED) ? "enabled" : "disabled"));
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_dicerange(
    {"$dicerange"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_is_leader();

      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
        throw precondition_failed("$C6Battle is already\nin progress");
      }
      if (l->tournament_match) {
        throw precondition_failed("$C6Cannot override\ndice ranges in a\ntournament");
      }

      auto parse_dice_range = +[](const string& spec) -> uint8_t {
        auto tokens = phosg::split(spec, '-');
        if (tokens.size() == 1) {
          uint8_t v = stoull(spec);
          return (v << 4) | (v & 0x0F);
        } else if (tokens.size() == 2) {
          return (stoull(tokens[0]) << 4) | (stoull(tokens[1]) & 0x0F);
        } else {
          throw runtime_error("invalid dice spec format");
        }
      };

      uint8_t def_dice_range = 0;
      uint8_t atk_dice_range_2v1 = 0;
      uint8_t def_dice_range_2v1 = 0;
      for (const auto& spec : phosg::split(a.text, ' ')) {
        auto tokens = phosg::split(spec, ':');
        if (tokens.size() != 2) {
          throw precondition_failed("$C6Invalid dice spec\nformat");
        }
        if (tokens[0] == "d") {
          def_dice_range = parse_dice_range(tokens[1]);
        } else if (tokens[0] == "1") {
          atk_dice_range_2v1 = parse_dice_range(tokens[1]);
          def_dice_range_2v1 = atk_dice_range_2v1;
        } else if (tokens[0] == "a1") {
          atk_dice_range_2v1 = parse_dice_range(tokens[1]);
        } else if (tokens[0] == "d1") {
          def_dice_range_2v1 = parse_dice_range(tokens[1]);
        }
      }

      l->ep3_server->def_dice_value_range_override = def_dice_range;
      l->ep3_server->atk_dice_value_range_2v1_override = atk_dice_range_2v1;
      l->ep3_server->def_dice_value_range_2v1_override = def_dice_range_2v1;

      if (!def_dice_range && !atk_dice_range_2v1 && !def_dice_range_2v1) {
        send_text_message_printf(l, "$C7Dice ranges reset\nto defaults");
      } else {
        send_text_message_printf(l, "$C7Dice ranges changed:");
        if (def_dice_range) {
          send_text_message_printf(l, "$C7DEF: $C6%hhu-%hhu",
              static_cast<uint8_t>(def_dice_range >> 4), static_cast<uint8_t>(def_dice_range & 0x0F));
        }
        if (atk_dice_range_2v1) {
          send_text_message_printf(l, "$C7ATK (1p in 2v1): $C6%hhu-%hhu",
              static_cast<uint8_t>(atk_dice_range_2v1 >> 4), static_cast<uint8_t>(atk_dice_range_2v1 & 0x0F));
        }
        if (def_dice_range_2v1) {
          send_text_message_printf(l, "$C7DEF (1p in 2v1): $C6%hhu-%hhu",
              static_cast<uint8_t>(def_dice_range_2v1 >> 4), static_cast<uint8_t>(def_dice_range_2v1 & 0x0F));
        }
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_dropmode(
    {"$dropmode"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      if (a.text.empty()) {
        switch (l->drop_mode) {
          case Lobby::DropMode::DISABLED:
            send_text_message(a.c, "Drop mode: disabled");
            break;
          case Lobby::DropMode::CLIENT:
            send_text_message(a.c, "Drop mode: client");
            break;
          case Lobby::DropMode::SERVER_SHARED:
            send_text_message(a.c, "Drop mode: server\nshared");
            break;
          case Lobby::DropMode::SERVER_PRIVATE:
            send_text_message(a.c, "Drop mode: server\nprivate");
            break;
          case Lobby::DropMode::SERVER_DUPLICATE:
            send_text_message(a.c, "Drop mode: server\nduplicate");
            break;
        }

      } else {
        a.check_is_leader();
        Lobby::DropMode new_mode;
        if ((a.text == "none") || (a.text == "disabled")) {
          new_mode = Lobby::DropMode::DISABLED;
        } else if (a.text == "client") {
          new_mode = Lobby::DropMode::CLIENT;
        } else if ((a.text == "shared") || (a.text == "server")) {
          new_mode = Lobby::DropMode::SERVER_SHARED;
        } else if ((a.text == "private") || (a.text == "priv")) {
          new_mode = Lobby::DropMode::SERVER_PRIVATE;
        } else if ((a.text == "duplicate") || (a.text == "dup")) {
          new_mode = Lobby::DropMode::SERVER_DUPLICATE;
        } else {
          throw precondition_failed("Invalid drop mode");
        }

        if (!(l->allowed_drop_modes & (1 << static_cast<size_t>(new_mode)))) {
          throw precondition_failed("Drop mode not\nallowed");
        }

        l->drop_mode = new_mode;
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
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.proxy_override_drops);

      using DropMode = ProxyServer::LinkedSession::DropMode;
      if (a.text.empty()) {
        switch (a.ses->drop_mode) {
          case DropMode::DISABLED:
            send_text_message(a.ses->client_channel, "Drop mode: disabled");
            break;
          case DropMode::PASSTHROUGH:
            send_text_message(a.ses->client_channel, "Drop mode: default");
            break;
          case DropMode::INTERCEPT:
            send_text_message(a.ses->client_channel, "Drop mode: proxy");
            break;
        }

      } else {
        DropMode new_mode;
        if ((a.text == "none") || (a.text == "disabled")) {
          new_mode = DropMode::DISABLED;
        } else if ((a.text == "default") || (a.text == "passthrough")) {
          new_mode = DropMode::PASSTHROUGH;
        } else if ((a.text == "proxy") || (a.text == "intercept")) {
          new_mode = DropMode::INTERCEPT;
        } else {
          throw precondition_failed("Invalid drop mode");
        }

        a.ses->set_drop_mode(new_mode);
        switch (a.ses->drop_mode) {
          case DropMode::DISABLED:
            send_text_message(a.ses->client_channel, "Item drops disabled");
            break;
          case DropMode::PASSTHROUGH:
            send_text_message(a.ses->client_channel, "Item drops changed\nto default mode");
            break;
          case DropMode::INTERCEPT:
            send_text_message(a.ses->client_channel, "Item drops changed\nto proxy mode");
            break;
        }
      }
    });

ChatCommandDefinition cc_edit(
    {"$edit"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(false);
      if (!is_v1_or_v2(a.c->version()) && (a.c->version() != Version::BB_V4)) {
        throw precondition_failed("$C6This command cannot\nbe used for your\nversion of PSO.");
      }

      bool cheats_allowed = (!a.check_permissions ||
          (s->cheat_mode_behavior != ServerState::BehaviorSwitch::OFF) ||
          a.c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));

      string encoded_args = phosg::tolower(a.text);
      vector<string> tokens = phosg::split(encoded_args, ' ');

      using MatType = PSOBBCharacterFile::MaterialType;

      try {
        auto p = a.c->character();
        if (tokens.at(0) == "atp" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.atp = stoul(tokens.at(1));
        } else if (tokens.at(0) == "mst" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.mst = stoul(tokens.at(1));
        } else if (tokens.at(0) == "evp" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.evp = stoul(tokens.at(1));
        } else if (tokens.at(0) == "hp" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.hp = stoul(tokens.at(1));
        } else if (tokens.at(0) == "dfp" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.dfp = stoul(tokens.at(1));
        } else if (tokens.at(0) == "ata" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.ata = stoul(tokens.at(1));
        } else if (tokens.at(0) == "lck" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.char_stats.lck = stoul(tokens.at(1));
        } else if (tokens.at(0) == "meseta" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.meseta = stoul(tokens.at(1));
        } else if (tokens.at(0) == "exp" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.experience = stoul(tokens.at(1));
        } else if (tokens.at(0) == "level" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          p->disp.stats.level = stoul(tokens.at(1)) - 1;
          p->recompute_stats(s->level_table(a.c->version()));
        } else if (((tokens.at(0) == "material") || (tokens.at(0) == "mat")) && !is_v1_or_v2(a.c->version()) && (cheats_allowed || !s->cheat_flags.reset_materials)) {
          if (tokens.at(1) == "reset") {
            const auto& which = tokens.at(2);
            if (which == "power") {
              p->set_material_usage(MatType::POWER, 0);
            } else if (which == "mind") {
              p->set_material_usage(MatType::MIND, 0);
            } else if (which == "evade") {
              p->set_material_usage(MatType::EVADE, 0);
            } else if (which == "def") {
              p->set_material_usage(MatType::DEF, 0);
            } else if (which == "luck") {
              p->set_material_usage(MatType::LUCK, 0);
            } else if (which == "hp") {
              p->set_material_usage(MatType::HP, 0);
            } else if (which == "tp") {
              p->set_material_usage(MatType::TP, 0);
            } else if (which == "all") {
              p->set_material_usage(MatType::POWER, 0);
              p->set_material_usage(MatType::MIND, 0);
              p->set_material_usage(MatType::EVADE, 0);
              p->set_material_usage(MatType::DEF, 0);
              p->set_material_usage(MatType::LUCK, 0);
            } else if (which == "every") {
              p->set_material_usage(MatType::POWER, 0);
              p->set_material_usage(MatType::MIND, 0);
              p->set_material_usage(MatType::EVADE, 0);
              p->set_material_usage(MatType::DEF, 0);
              p->set_material_usage(MatType::LUCK, 0);
              p->set_material_usage(MatType::HP, 0);
              p->set_material_usage(MatType::TP, 0);
            } else {
              throw precondition_failed("$C6Invalid subcommand");
            }
          } else {
            throw precondition_failed("$C6Invalid subcommand");
          }
          p->recompute_stats(s->level_table(a.c->version()));
        } else if (tokens.at(0) == "namecolor") {
          uint32_t new_color;
          sscanf(tokens.at(1).c_str(), "%8X", &new_color);
          p->disp.visual.name_color = new_color;
        } else if (tokens.at(0) == "language" || tokens.at(0) == "lang") {
          if (tokens.at(1).size() != 1) {
            throw runtime_error("invalid language");
          }
          uint8_t new_language = language_code_for_char(tokens.at(1).at(0));
          a.c->channel.language = new_language;
          p->inventory.language = new_language;
          p->guild_card.language = new_language;
          auto sys = a.c->system_file(false);
          if (sys) {
            sys->language = new_language;
          }
        } else if (tokens.at(0) == "secid") {
          if (!cheats_allowed && (p->disp.stats.level > 0) && s->cheat_flags.edit_section_id) {
            throw precondition_failed("$C6You cannot change\nyour Section ID\nafter level 1");
          }
          uint8_t secid = section_id_for_name(tokens.at(1));
          if (secid == 0xFF) {
            throw precondition_failed("$C6No such section ID");
          } else {
            p->disp.visual.section_id = secid;
          }
        } else if (tokens.at(0) == "name") {
          vector<string> orig_tokens = phosg::split(a.text, ' ');
          p->disp.name.encode(orig_tokens.at(1), p->inventory.language);
        } else if (tokens.at(0) == "npc") {
          if (tokens.at(1) == "none") {
            p->disp.visual.extra_model = 0;
            p->disp.visual.validation_flags &= 0xFD;
            // Restore saved fields, if any
            if (p->disp.visual.unused[0] == 0x8D) {
              p->disp.visual.char_class = p->disp.visual.unused[1];
              p->disp.visual.head = p->disp.visual.unused[2];
              p->disp.visual.hair = p->disp.visual.unused[3];
              p->disp.visual.unused.clear(0);
            }
          } else {
            uint8_t npc = npc_for_name(tokens.at(1), a.c->version());
            if (npc == 0xFF) {
              throw precondition_failed("$C6No such NPC");
            }

            // Some NPCs can crash the client if the character's class is
            // incorrect. To handle this, we save the affected fields in the unused
            // bytes after extra_model.
            int8_t replacement_class = -1;
            switch (npc) {
              case 1: // Rico (replace with HUnewearl)
              case 6: // Elly (replace with HUnewearl)
                replacement_class = 0x01;
                break;
              case 0: // Ninja (replace with HUmar)
              case 2: // Sonic (replace with HUmar)
              case 5: // Flowen (replace with HUmar)
                replacement_class = 0x00;
                break;
            }
            if (replacement_class >= 0) {
              if (p->disp.visual.unused[0] != 0x8D) {
                p->disp.visual.unused[0] = 0x8D;
                p->disp.visual.unused[1] = p->disp.visual.char_class;
                p->disp.visual.unused[2] = p->disp.visual.head;
                p->disp.visual.unused[3] = p->disp.visual.hair;
              }
              p->disp.visual.char_class = replacement_class;
              p->disp.visual.head = 0x00;
              p->disp.visual.hair = 0x00;
            }

            p->disp.visual.extra_model = npc;
            p->disp.visual.validation_flags |= 0x02;
          }
        } else if (tokens.at(0) == "tech" && (cheats_allowed || !s->cheat_flags.edit_stats)) {
          uint8_t level = stoul(tokens.at(2)) - 1;
          if (tokens.at(1) == "all") {
            for (size_t x = 0; x < 0x14; x++) {
              p->set_technique_level(x, level);
            }
          } else {
            uint8_t tech_id = technique_for_name(tokens.at(1));
            if (tech_id == 0xFF) {
              throw precondition_failed("$C6No such technique");
            }
            try {
              p->set_technique_level(tech_id, level);
            } catch (const out_of_range&) {
              throw precondition_failed("$C6Invalid technique");
            }
          }
        } else {
          throw precondition_failed("$C6Unknown field");
        }
      } catch (const out_of_range&) {
        throw precondition_failed("$C6Not enough arguments");
      }

      // Reload the client in the lobby
      send_player_leave_notification(l, a.c->lobby_client_id);
      if (a.c->version() == Version::BB_V4) {
        send_complete_player_bb(a.c);
      }
      a.c->v1_v2_last_reported_disp.reset();
      s->send_lobby_join_notifications(l, a.c);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_event(
    {"$event"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      a.check_is_game(false);
      a.check_account_flag(Account::Flag::CHANGE_EVENT);

      uint8_t new_event = event_for_name(a.text);
      if (new_event == 0xFF) {
        throw precondition_failed("$C6No such lobby event");
      }

      l->event = new_event;
      send_change_event(l, l->event);
    },
    +[](const ProxyArgs& a) {
      if (a.text.empty()) {
        a.ses->config.override_lobby_event = 0xFF;
      } else {
        uint8_t new_event = event_for_name(a.text);
        if (new_event == 0xFF) {
          throw precondition_failed("$C6No such lobby event");
        } else {
          a.ses->config.override_lobby_event = new_event;
          if (!is_v1_or_v2(a.ses->version())) {
            a.ses->client_channel.send(0xDA, a.ses->config.override_lobby_event);
          }
        }
      }
    });

ChatCommandDefinition cc_exit(
    {"$exit"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      if (l->is_game()) {
        if (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) || l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
          G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
          a.c->channel.send(0x60, 0x00, cmd);
          a.c->floor = 0;
        } else if (is_ep3(a.c->version())) {
          a.c->channel.send(0xED, 0x00);
        } else {
          throw precondition_failed("$C6You must return to\nthe lobby first");
        }
      } else {
        send_self_leave_notification(a.c);
        if (!a.c->config.check_flag(Client::Flag::NO_D6)) {
          send_message_box(a.c, "");
        }

        send_client_to_login_server(a.c);
      }
    },
    +[](const ProxyArgs& a) {
      if (a.ses->is_in_game) {
        if (is_ep3(a.ses->version())) {
          a.ses->client_channel.send(0xED, 0x00);
        } else if (a.ses->is_in_quest) {
          G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
          a.ses->client_channel.send(0x60, 0x00, cmd);
        } else {
          throw precondition_failed("$C6You must return to\nthe lobby first");
        }
      } else {
        a.ses->disconnect_action = ProxyServer::LinkedSession::DisconnectAction::CLOSE_IMMEDIATELY;
        a.ses->send_to_game_server();
      }
    });

ChatCommandDefinition cc_gc(
    {"$gc"},
    +[](const ServerArgs& a) {
      send_guild_card(a.c, a.c);
    },
    +[](const ProxyArgs& a) {
      bool any_card_sent = false;
      for (const auto& p : a.ses->lobby_players) {
        if (!p.name.empty() && a.text == p.name) {
          send_guild_card(a.ses->client_channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
          any_card_sent = true;
        }
      }

      if (!any_card_sent) {
        size_t index = stoull(a.text, nullptr, 0);
        const auto& p = a.ses->lobby_players.at(index);
        if (!p.name.empty()) {
          send_guild_card(a.ses->client_channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
        }
      }
    });

ChatCommandDefinition cc_infhp(
    {"$infhp"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      if (a.c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        a.c->config.clear_flag(Client::Flag::INFINITE_HP_ENABLED);
        send_text_message(a.c, "$C6Infinite HP disabled");
      } else {
        a.check_cheats_enabled(s->cheat_flags.infinite_hp_tp);
        a.c->config.set_flag(Client::Flag::INFINITE_HP_ENABLED);
        send_text_message(a.c, "$C6Infinite HP enabled");
        send_remove_negative_conditions(a.c);
      }
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();

      if (a.ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        a.ses->config.clear_flag(Client::Flag::INFINITE_HP_ENABLED);
        send_text_message(a.ses->client_channel, "$C6Infinite HP disabled");
      } else {
        a.check_cheats_allowed(s->cheat_flags.infinite_hp_tp);
        a.ses->config.set_flag(Client::Flag::INFINITE_HP_ENABLED);
        send_text_message(a.ses->client_channel, "$C6Infinite HP enabled");
        send_remove_negative_conditions(a.ses->client_channel, a.ses->lobby_client_id);
        send_remove_negative_conditions(a.ses->server_channel, a.ses->lobby_client_id);
      }
    });

ChatCommandDefinition cc_inftime(
    {"$inftime"},
    +[](const ServerArgs& a) {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_is_leader();

      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION) {
        throw precondition_failed("$C6Battle is already\nin progress");
      }

      l->ep3_server->options.behavior_flags ^= Episode3::BehaviorFlag::DISABLE_TIME_LIMITS;
      bool infinite_time_enabled = (l->ep3_server->options.behavior_flags & Episode3::BehaviorFlag::DISABLE_TIME_LIMITS);
      send_text_message(l, infinite_time_enabled ? "$C6Infinite time enabled" : "$C6Infinite time disabled");
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_inftp(
    {"$inftp"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      if (a.c->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        a.c->config.clear_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.c, "$C6Infinite TP disabled");
      } else {
        a.check_cheats_enabled(s->cheat_flags.infinite_hp_tp);
        a.c->config.set_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.c, "$C6Infinite TP enabled");
      }
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();

      if (a.ses->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        a.ses->config.clear_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.ses->client_channel, "$C6Infinite TP disabled");
      } else {
        a.check_cheats_allowed(s->cheat_flags.infinite_hp_tp);
        a.ses->config.set_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.ses->client_channel, "$C6Infinite TP enabled");
      }
    });

ChatCommandDefinition cc_item(
    {"$item", "$i"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_cheats_enabled(s->cheat_flags.create_items);

      ItemData item = s->parse_item_description(a.c->version(), a.text);
      item.id = l->generate_item_id(a.c->lobby_client_id);

      if ((l->drop_mode == Lobby::DropMode::SERVER_PRIVATE) || (l->drop_mode == Lobby::DropMode::SERVER_DUPLICATE)) {
        l->add_item(a.c->floor, item, a.c->pos, nullptr, nullptr, (1 << a.c->lobby_client_id));
        send_drop_stacked_item_to_channel(s, a.c->channel, item, a.c->floor, a.c->pos);
      } else {
        l->add_item(a.c->floor, item, a.c->pos, nullptr, nullptr, 0x00F);
        send_drop_stacked_item_to_lobby(l, item, a.c->floor, a.c->pos);
      }

      string name = s->describe_item(a.c->version(), item, true);
      send_text_message(a.c, "$C7Item created:\n" + name);
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.create_items);
      if (a.ses->version() == Version::BB_V4) {
        throw precondition_failed("$C6This command cannot\nbe used on the proxy\nserver in BB games");
      }
      if (!a.ses->is_in_game) {
        throw precondition_failed("$C6You must be in\na game to use this\ncommand");
      }
      if (a.ses->lobby_client_id != a.ses->leader_client_id) {
        throw precondition_failed("$C6You must be the\nleader to use this\ncommand");
      }

      bool set_drop = (!a.text.empty() && (a.text[0] == '!'));

      ItemData item = s->parse_item_description(a.ses->version(), (set_drop ? a.text.substr(1) : a.text));
      item.id = phosg::random_object<uint32_t>() | 0x80000000;

      if (set_drop) {
        a.ses->next_drop_item = item;

        string name = s->describe_item(a.ses->version(), item, true);
        send_text_message(a.ses->client_channel, "$C7Next drop:\n" + name);

      } else {
        send_drop_stacked_item_to_channel(s, a.ses->client_channel, item, a.ses->floor, a.ses->pos);
        send_drop_stacked_item_to_channel(s, a.ses->server_channel, item, a.ses->floor, a.ses->pos);

        string name = s->describe_item(a.ses->version(), item, true);
        send_text_message(a.ses->client_channel, "$C7Item created:\n" + name);
      }
    });

static void command_item_notifs(Channel& ch, Client::Config& config, const std::string& text) {
  if (text == "every" || text == "everything") {
    config.set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA);
    send_text_message_printf(ch, "$C6Notifications enabled\nfor all items and\nMeseta");
  } else if (text == "all" || text == "on") {
    config.set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS);
    send_text_message_printf(ch, "$C6Notifications enabled\nfor all items");
  } else if (text == "rare" || text == "rares") {
    config.set_drop_notification_mode(Client::ItemDropNotificationMode::RARES_ONLY);
    send_text_message_printf(ch, "$C6Notifications enabled\nfor rare items only");
  } else if (text == "none" || text == "off") {
    config.set_drop_notification_mode(Client::ItemDropNotificationMode::NOTHING);
    send_text_message_printf(ch, "$C6Notifications disabled\nfor all items");
  } else {
    throw precondition_failed("$C6You must specify\n$C6off$C7, $C6rare$C7, $C6on$C7, or\n$C6everything$C7");
  }
}

ChatCommandDefinition cc_itemnotifs(
    {"$itemnotifs"},
    +[](const ServerArgs& a) {
      command_item_notifs(a.c->channel, a.c->config, a.text);
    },
    +[](const ProxyArgs& a) {
      command_item_notifs(a.ses->client_channel, a.ses->config, a.text);
    });

ChatCommandDefinition cc_kick(
    {"$kick"},
    +[](const ServerArgs& a) {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_account_flag(Account::Flag::KICK_USER);

      auto target = s->find_client(&a.text);
      if (!target->login) {
        // This should be impossible, but I'll bet it's not actually
        throw precondition_failed("$C6Client not logged in");
      }

      if (target->login->account->check_flag(Account::Flag::KICK_USER)) {
        throw precondition_failed("$C6You do not have\nsufficient privileges.");
      }
      if (a.c == target) {
        // This shouldn't be possible because you need KICK_USER to get here,
        // but the target can't have KICK_USER if we get here, so if a.c and
        // target are the same, one of the preceding conditions must be false.
        throw logic_error("client attempts to kick themself off");
      }

      send_message_box(target, "$C6You have been kicked off the server.");
      string target_name = name_for_client(target);
      s->game_server->disconnect_client(target);
      send_text_message_printf(a.c, "$C6%s kicked off", target_name.c_str());
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_killcount(
    {"$killcount"},
    +[](const ServerArgs& a) {
      auto p = a.c->character();
      size_t item_index;
      try {
        item_index = p->inventory.find_equipped_item(EquipSlot::WEAPON);
      } catch (const out_of_range&) {
        throw precondition_failed("No weapon equipped");
      }

      const auto& item = p->inventory.items.at(item_index);
      if (!item.data.has_kill_count()) {
        throw precondition_failed("Weapon does not\nhave a kill count");
      }

      // Kill counts are only accurate on the server side at all times on BB. On
      // other versions, we update the server's view of the client's inventory
      // during games, but we can't track kills because the client doesn't inform
      // the server whether it counted a kill for any individual enemy. So, on
      // non-BB versions, the kill count is accurate at all times in the lobby
      // (since kills can't occur there), or at the beginning of a game.
      if ((a.c->version() == Version::BB_V4) || !a.c->require_lobby()->is_game()) {
        send_text_message_printf(a.c, "%hu kills", item.data.get_kill_count());
      } else {
        send_text_message_printf(a.c, "%hu kills as of\ngame join", item.data.get_kill_count());
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_lobby_info(
    {"$li"},
    +[](const ServerArgs& a) -> void {
      vector<string> lines;

      auto l = a.c->lobby.lock();
      if (!l) {
        lines.emplace_back("$C4No lobby info");

      } else {
        if (l->is_game()) {
          if (!l->is_ep3()) {
            if (l->max_level == 0xFFFFFFFF) {
              lines.emplace_back(phosg::string_printf("$C6%08X$C7 L$C6%d+$C7", l->lobby_id, l->min_level + 1));
            } else {
              lines.emplace_back(phosg::string_printf("$C6%08X$C7 L$C6%d-%d$C7", l->lobby_id, l->min_level + 1, l->max_level + 1));
            }
            lines.emplace_back(phosg::string_printf("$C7Section ID: $C6%s$C7", name_for_section_id(l->effective_section_id())));

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
            lines.emplace_back(phosg::string_printf("$C7State seed: $C6%08X$C7", l->random_seed));
          }

        } else {
          lines.emplace_back(phosg::string_printf("$C7Lobby ID: $C6%08X$C7", l->lobby_id));
        }

        string slots_str = "Slots: ";
        for (size_t z = 0; z < l->clients.size(); z++) {
          if (!l->clients[z]) {
            slots_str += phosg::string_printf("$C0%zX$C7", z);
          } else {
            bool is_self = (l->clients[z] == a.c);
            bool is_leader = (z == l->leader_id);
            if (is_self && is_leader) {
              slots_str += phosg::string_printf("$C6%zX$C7", z);
            } else if (is_self) {
              slots_str += phosg::string_printf("$C2%zX$C7", z);
            } else if (is_leader) {
              slots_str += phosg::string_printf("$C4%zX$C7", z);
            } else {
              slots_str += phosg::string_printf("%zX", z);
            }
          }
        }
        lines.emplace_back(std::move(slots_str));
      }

      send_text_message(a.c, phosg::join(lines, "\n"));
    },
    +[](const ProxyArgs& a) -> void {
      string msg;
      // On non-masked-GC sessions (BB), there is no remote Guild Card number, so we
      // don't show it. (The user can see it in the pause menu, unlike in masked-GC
      // sessions like GC.)
      if (a.ses->remote_guild_card_number >= 0) {
        msg = phosg::string_printf("$C7GC: $C6%" PRId64 "$C7\n", a.ses->remote_guild_card_number);
      }
      msg += "Slots: ";

      for (size_t z = 0; z < a.ses->lobby_players.size(); z++) {
        bool is_self = (z == a.ses->lobby_client_id);
        bool is_leader = (z == a.ses->leader_client_id);
        if (a.ses->lobby_players[z].guild_card_number == 0) {
          msg += phosg::string_printf("$C0%zX$C7", z);
        } else if (is_self && is_leader) {
          msg += phosg::string_printf("$C6%zX$C7", z);
        } else if (is_self) {
          msg += phosg::string_printf("$C2%zX$C7", z);
        } else if (is_leader) {
          msg += phosg::string_printf("$C4%zX$C7", z);
        } else {
          msg += phosg::string_printf("%zX", z);
        }
      }

      vector<const char*> cheats_tokens;
      if (a.ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        cheats_tokens.emplace_back("HP");
      }
      if (a.ses->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        cheats_tokens.emplace_back("TP");
      }
      if (!cheats_tokens.empty()) {
        msg += "\n$C7Cheats: $C6";
        msg += phosg::join(cheats_tokens, ",");
      }

      vector<const char*> behaviors_tokens;
      if (a.ses->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
        behaviors_tokens.emplace_back("SWA");
      }
      if (a.ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
        behaviors_tokens.emplace_back("SF");
      }
      if (a.ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN)) {
        behaviors_tokens.emplace_back("SL");
      }
      if (a.ses->config.check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS)) {
        behaviors_tokens.emplace_back("BF");
      }
      if (!behaviors_tokens.empty()) {
        msg += "\n$C7Flags: $C6";
        msg += phosg::join(behaviors_tokens, ",");
      }

      if (a.ses->config.override_section_id != 0xFF) {
        msg += "\n$C7SecID*: $C6";
        msg += name_for_section_id(a.ses->config.override_section_id);
      }

      send_text_message(a.ses->client_channel, msg);
    });

ChatCommandDefinition cc_ln(
    {"$ln"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(false);

      uint8_t new_type;
      if (a.text.empty()) {
        new_type = 0x80;
      } else {
        new_type = lobby_type_for_name(a.text);
        if (new_type == 0x80) {
          throw precondition_failed("$C6No such lobby type");
        }
      }

      a.c->config.override_lobby_number = new_type;
      send_join_lobby(a.c, l);
    },
    +[](const ProxyArgs& a) {
      uint8_t new_type;
      if (a.text.empty()) {
        new_type = 0x80;
      } else {
        new_type = lobby_type_for_name(a.text);
        if (new_type == 0x80) {
          throw precondition_failed("$C6No such lobby type");
        }
      }
      a.ses->config.override_lobby_number = new_type;
    });

ChatCommandDefinition cc_loadchar(
    {"$loadchar"},
    +[](const ServerArgs& a) -> void {
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }
      auto l = a.c->require_lobby();
      a.check_is_game(false);

      size_t index = stoull(a.text, nullptr, 0) - 1;
      if (index > 15) {
        throw precondition_failed("$C6Player index must\nbe in range 1-16");
      }

      shared_ptr<PSOGCEp3CharacterFile::Character> ep3_char;
      if (is_ep3(a.c->version())) {
        ep3_char = a.c->load_ep3_backup_character(a.c->login->account->account_id, index);
      } else {
        a.c->load_backup_character(a.c->login->account->account_id, index);
      }

      if (a.c->version() == Version::BB_V4) {
        // On BB, it suffices to simply send the character file again
        auto s = a.c->require_server_state();
        send_complete_player_bb(a.c);
        send_player_leave_notification(l, a.c->lobby_client_id);
        s->send_lobby_join_notifications(l, a.c);

      } else if ((a.c->version() == Version::DC_V2) ||
          (a.c->version() == Version::GC_NTE) ||
          (a.c->version() == Version::GC_V3) ||
          (a.c->version() == Version::GC_EP3_NTE) ||
          (a.c->version() == Version::GC_EP3) ||
          (a.c->version() == Version::XB_V3)) {
        // TODO: Support extended player info on other versions
        auto s = a.c->require_server_state();
        if (!a.c->config.check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) ||
            a.c->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY)) {
          throw precondition_failed_printf("Can\'t load character\ndata on this game\nversion");
        }

        auto send_set_extended_player_info = []<typename CharT>(shared_ptr<Client> c, const CharT& char_file) -> void {
          prepare_client_for_patches(c, [wc = weak_ptr<Client>(c), char_file]() {
            auto c = wc.lock();
            if (!c) {
              return;
            }
            try {
              auto s = c->require_server_state();
              auto fn = s->function_code_index->get_patch("SetExtendedPlayerInfo", c->config.specific_version);
              send_function_call(c, fn, {}, &char_file, sizeof(CharT));
              c->function_call_response_queue.emplace_back([wc = weak_ptr<Client>(c)](uint32_t, uint32_t) -> void {
                auto c = wc.lock();
                if (!c) {
                  return;
                }
                auto l = c->lobby.lock();
                if (l) {
                  auto s = c->require_server_state();
                  send_player_leave_notification(l, c->lobby_client_id);
                  s->send_lobby_join_notifications(l, c);
                }
              });
            } catch (const exception& e) {
              c->log.warning("Failed to set extended player info: %s", e.what());
              throw precondition_failed_printf("Failed to set\nplayer info:\n%s", e.what());
            }
          });
        };

        if (a.c->version() == Version::DC_V2) {
          PSODCV2CharacterFile::Character dc_char = *a.c->character();
          send_set_extended_player_info(a.c, dc_char);
        } else if (a.c->version() == Version::GC_NTE) {
          PSOGCNTECharacterFileCharacter gc_char = *a.c->character();
          send_set_extended_player_info(a.c, gc_char);
        } else if (a.c->version() == Version::GC_V3) {
          PSOGCCharacterFile::Character gc_char = *a.c->character();
          send_set_extended_player_info(a.c, gc_char);
        } else if (a.c->version() == Version::GC_EP3_NTE) {
          PSOGCEp3NTECharacter nte_char = *ep3_char;
          send_set_extended_player_info(a.c, nte_char);
        } else if (a.c->version() == Version::GC_EP3) {
          send_set_extended_player_info(a.c, *ep3_char);
        } else if (a.c->version() == Version::XB_V3) {
          if (!a.c->login || !a.c->login->xb_license) {
            throw runtime_error("XB client is not logged in");
          }
          PSOXBCharacterFileCharacter xb_char = *a.c->character();
          xb_char.guild_card.xb_user_id_high = (a.c->login->xb_license->user_id >> 32) & 0xFFFFFFFF;
          xb_char.guild_card.xb_user_id_low = a.c->login->xb_license->user_id & 0xFFFFFFFF;
          send_set_extended_player_info(a.c, xb_char);
        } else {
          throw logic_error("unimplemented extended player info version");
        }

      } else {
        // On v1 and v2, the client will assign its character data from the lobby
        // join command, so it suffices to just resend the join notification.
        auto s = a.c->require_server_state();
        send_player_leave_notification(l, a.c->lobby_client_id);
        s->send_lobby_join_notifications(l, a.c);
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_matcount(
    {"$matcount"},
    +[](const ServerArgs& a) -> void {
      auto p = a.c->character();
      if (is_v1_or_v2(a.c->version())) {
        send_text_message_printf(a.c, "%hhu HP, %hhu TP",
            p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
      } else {
        send_text_message_printf(a.c, "%hhu HP, %hhu TP, %hhu POW\n%hhu MIND, %hhu EVADE\n%hhu DEF, %hhu LUCK",
            p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::TP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_maxlevel(
    {"$maxlevel"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_leader();

      l->max_level = stoull(a.text) - 1;
      if (l->max_level >= 200) {
        l->max_level = 0xFFFFFFFF;
      }

      if (l->max_level == 0xFFFFFFFF) {
        send_text_message(l, "$C6Maximum level set\nto unlimited");
      } else {
        send_text_message_printf(l, "$C6Maximum level set\nto %" PRIu32, l->max_level + 1);
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_minlevel(
    {"$minlevel"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_leader();

      size_t new_min_level = stoull(a.text) - 1;

      auto s = a.c->require_server_state();
      bool cheats_allowed = (l->check_flag(Lobby::Flag::CHEATS_ENABLED) ||
          a.c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));
      if (!cheats_allowed && s->cheat_flags.insufficient_minimum_level) {
        size_t default_min_level = s->default_min_level_for_game(a.c->version(), l->episode, l->difficulty);
        if (new_min_level < default_min_level) {
          throw precondition_failed_printf("$C6Cannot set minimum\nlevel below %zu", default_min_level + 1);
        }
      }

      l->min_level = new_min_level;
      send_text_message_printf(l, "$C6Minimum level set\nto %" PRIu32, l->min_level + 1);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_next(
    {"$next"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_cheats_enabled(s->cheat_flags.warp);

      size_t limit = floor_limit_for_episode(l->episode);
      if (limit == 0) {
        return;
      }
      send_warp(a.c, (a.c->floor + 1) % limit, true);
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.warp);
      if (!a.ses->is_in_game) {
        throw precondition_failed("$C6You must be in a\ngame to use this\ncommand");
      }
      send_warp(a.ses->client_channel, a.ses->lobby_client_id, a.ses->floor, true);
    });

ChatCommandDefinition cc_password(
    {"$password"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_leader();

      if (a.text.empty()) {
        l->password.clear();
        send_text_message(l, "$C6Game unlocked");

      } else {
        l->password = a.text;
        string escaped = remove_color(l->password);
        send_text_message_printf(l, "$C6Game password:\n%s", escaped.c_str());
      }
    },
    unavailable_on_proxy_server);

struct PatchCommandArgs {
  string patch_name;
  unordered_map<string, uint32_t> label_writes;

  PatchCommandArgs(const string& args) {
    auto tokens = phosg::split(args, ' ');
    if (tokens.empty()) {
      throw runtime_error("not enough arguments");
    }
    this->patch_name = std::move(tokens[0]);
    for (size_t z = 0; z < tokens.size() - 1; z++) {
      this->label_writes.emplace(phosg::string_printf("arg%zu", z), stoul(tokens[z + 1], nullptr, 0));
    }
  }
};

ChatCommandDefinition cc_patch(
    {"$patch"},
    +[](const ServerArgs& a) -> void {
      PatchCommandArgs pca(a.text);

      prepare_client_for_patches(a.c, [wc = weak_ptr<Client>(a.c), pca]() {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        try {
          auto s = c->require_server_state();
          // Note: We can't look this up outside of the closure because
          // c->specific_version can change during prepare_client_for_patches
          auto fn = s->function_code_index->get_patch(pca.patch_name, c->config.specific_version);
          send_function_call(c, fn, pca.label_writes);
          c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
        } catch (const out_of_range&) {
          throw precondition_failed("Invalid patch name");
        }
      });
    },
    +[](const ProxyArgs& a) {
      PatchCommandArgs pca(a.text);

      auto send_call = [ses = a.ses, pca](uint32_t specific_version, uint32_t) {
        try {
          if (ses->config.specific_version != specific_version) {
            ses->config.specific_version = specific_version;
            ses->log.info("Version detected as %08" PRIX32, ses->config.specific_version);
          }
          auto s = ses->require_server_state();
          auto fn = s->function_code_index->get_patch(pca.patch_name, ses->config.specific_version);
          send_function_call(ses->client_channel, ses->config, fn, pca.label_writes);
          // Don't forward the patch response to the server
          ses->function_call_return_handler_queue.emplace_back(empty_function_call_response_handler);
        } catch (const out_of_range&) {
          throw precondition_failed("Invalid patch name");
        }
      };

      auto send_version_detect_or_send_call = [ses = a.ses, send_call]() {
        bool is_gc = ::is_gc(ses->version());
        bool is_xb = (ses->version() == Version::XB_V3);
        if ((is_gc || is_xb) && specific_version_is_indeterminate(ses->config.specific_version)) {
          auto s = ses->require_server_state();
          send_function_call(
              ses->client_channel,
              ses->config,
              s->function_code_index->name_to_function.at(is_xb ? "VersionDetectXB" : "VersionDetectGC"));
          ses->function_call_return_handler_queue.emplace_back(send_call);
        } else {
          send_call(ses->config.specific_version, 0);
        }
      };

      // This mirrors the implementation in prepare_client_for_patches
      if (!a.ses->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH)) {
        auto s = a.ses->require_server_state();
        send_function_call(
            a.ses->client_channel, a.ses->config, s->function_code_index->name_to_function.at("CacheClearFix-Phase1"), {}, "", 0, 0, 0x7F2734EC);
        a.ses->function_call_return_handler_queue.emplace_back([s, ses = a.ses, send_version_detect_or_send_call](uint32_t, uint32_t) -> void {
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
    });

ChatCommandDefinition cc_persist(
    {"$persist"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      if (l->check_flag(Lobby::Flag::DEFAULT)) {
        throw precondition_failed("$C6Default lobbies\ncannot be marked\ntemporary");
      } else if (!l->is_game()) {
        throw precondition_failed("$C6Private lobbies\ncannot be marked\npersistent");
      } else if (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) || l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
        throw precondition_failed("$C6Games cannot be\npersistent if a\nquest has already\nbegun");
      } else if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
        throw precondition_failed("$C6Spectator teams\ncannot be marked\npersistent");
      } else {
        l->toggle_flag(Lobby::Flag::PERSISTENT);
        send_text_message_printf(l, "Lobby persistence\n%s", l->check_flag(Lobby::Flag::PERSISTENT) ? "enabled" : "disabled");
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_ping(
    {"$ping"},
    +[](const ServerArgs& a) -> void {
      a.c->ping_start_time = phosg::now();
      send_command(a.c, 0x1D, 0x00);
    },
    +[](const ProxyArgs& a) {
      a.ses->client_ping_start_time = phosg::now();
      a.ses->server_ping_start_time = a.ses->client_ping_start_time;

      C_GuildCardSearch_40 cmd = {0x00010000, a.ses->remote_guild_card_number, a.ses->remote_guild_card_number};
      a.ses->client_channel.send(0x1D, 0x00);
      a.ses->server_channel.send(0x40, 0x00, &cmd, sizeof(cmd));
    });

static string file_path_for_recording(const std::string& args, uint32_t account_id) {
  for (char ch : args) {
    if (ch <= 0x20 || ch > 0x7E || ch == '/') {
      throw runtime_error("invalid recording name");
    }
  }
  return phosg::string_printf("system/ep3/battle-records/%010" PRIu32 "_%s.mzrd", account_id, args.c_str());
}

ChatCommandDefinition cc_playrec(
    {"$playrec"},
    +[](const ServerArgs& a) -> void {
      if (!is_ep3(a.c->version())) {
        throw precondition_failed("$C4This command can\nonly be used on\nEpisode 3");
      }

      auto l = a.c->require_lobby();
      if (l->is_game() && l->battle_player) {
        l->battle_player->start();
      } else if (!l->is_game()) {
        string file_path = file_path_for_recording(a.text, a.c->login->account->account_id);

        auto s = a.c->require_server_state();
        string filename = a.text;
        bool start_battle_player_immediately = (filename[0] == '!');
        if (start_battle_player_immediately) {
          filename = filename.substr(1);
        }

        string data;
        try {
          data = phosg::load_file(file_path);
        } catch (const phosg::cannot_open_file&) {
          throw precondition_failed("$C4The recording does\nnot exist");
        }
        auto record = make_shared<Episode3::BattleRecord>(data);
        auto battle_player = make_shared<Episode3::BattleRecordPlayer>(record, s->game_server->get_base());
        auto game = create_game_generic(
            s, a.c, a.text, "", Episode::EP3, GameMode::NORMAL, 0, false, nullptr, battle_player);
        if (game) {
          if (start_battle_player_immediately) {
            game->set_flag(Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY);
          }
          s->change_client_lobby(a.c, game);
          a.c->config.set_flag(Client::Flag::LOADING);
          a.c->log.info("LOADING flag set");
        }
      } else {
        throw precondition_failed("$C4This command cannot\nbe used in a game");
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_qcall(
    {"$qcall"},
    +[](const ServerArgs& a) -> void {
      a.check_debug_enabled();

      auto l = a.c->require_lobby();
      if (l->is_game() && l->quest) {
        send_quest_function_call(a.c, stoul(a.text, nullptr, 0));
      } else {
        throw precondition_failed("$C6You must be in a\nquest to use this\ncommand");
      }
    },
    +[](const ProxyArgs& a) {
      if (a.ses->is_in_game && a.ses->is_in_quest) {
        send_quest_function_call(a.ses->client_channel, stoul(a.text, nullptr, 0));
      } else {
        throw precondition_failed("$C6You must be in a\nquest to use this\ncommand");
      }
    });

ChatCommandDefinition cc_qcheck(
    {"$qcheck"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      uint16_t flag_num = stoul(a.text, nullptr, 0);

      if (l->is_game()) {
        if (!l->quest_flags_known || l->quest_flags_known->get(l->difficulty, flag_num)) {
          send_text_message_printf(a.c, "$C7Game: flag 0x%hX (%hu)\nis %s on %s",
              flag_num, flag_num,
              a.c->character()->quest_flags.get(l->difficulty, flag_num) ? "set" : "not set",
              name_for_difficulty(l->difficulty));
        } else {
          send_text_message_printf(a.c, "$C7Game: flag 0x%hX (%hu)\nis unknown on %s",
              flag_num, flag_num, name_for_difficulty(l->difficulty));
        }
      } else if (a.c->version() == Version::BB_V4) {
        send_text_message_printf(a.c, "$C7Player: flag 0x%hX (%hu)\nis %s on %s",
            flag_num, flag_num,
            a.c->character()->quest_flags.get(l->difficulty, flag_num) ? "set" : "not set",
            name_for_difficulty(l->difficulty));
      }
    },
    unavailable_on_proxy_server);

static void server_command_qset_qclear(const ServerArgs& a, bool should_set) {
  a.check_debug_enabled();

  auto l = a.c->require_lobby();
  if (!l->is_game()) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  uint16_t flag_num = stoul(a.text, nullptr, 0);

  if (l->is_game()) {
    if (l->quest_flags_known) {
      l->quest_flags_known->set(l->difficulty, flag_num);
    }
    if (should_set) {
      l->quest_flag_values->set(l->difficulty, flag_num);
    } else {
      l->quest_flag_values->clear(l->difficulty, flag_num);
    }
  }

  auto p = a.c->character(false);
  if (p) {
    if (should_set) {
      p->quest_flags.set(l->difficulty, flag_num);
    } else {
      p->quest_flags.clear(l->difficulty, flag_num);
    }
  }

  if (is_v1_or_v2(a.c->version())) {
    G_UpdateQuestFlag_DC_PC_6x75 cmd = {{0x75, 0x02, 0x0000}, flag_num, should_set ? 0 : 1};
    send_command_t(l, 0x60, 0x00, cmd);
  } else {
    G_UpdateQuestFlag_V3_BB_6x75 cmd = {{{0x75, 0x03, 0x0000}, flag_num, should_set ? 0 : 1}, l->difficulty, 0x0000};
    send_command_t(l, 0x60, 0x00, cmd);
  }
}

static void proxy_command_qset_qclear(const ProxyArgs& a, bool should_set) {
  if (!a.ses->is_in_game) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  uint16_t flag_num = stoul(a.text, nullptr, 0);
  if (is_v1_or_v2(a.ses->version())) {
    G_UpdateQuestFlag_DC_PC_6x75 cmd = {{0x75, 0x02, 0x0000}, flag_num, should_set ? 0 : 1};
    a.ses->client_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
    a.ses->server_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
  } else {
    G_UpdateQuestFlag_V3_BB_6x75 cmd = {{{0x75, 0x03, 0x0000}, flag_num, should_set ? 0 : 1}, a.ses->lobby_difficulty, 0x0000};
    a.ses->client_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
    a.ses->server_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
  }
}

ChatCommandDefinition cc_qclear(
    {"$qclear"},
    +[](const ServerArgs& a) -> void {
      server_command_qset_qclear(a, false);
    },
    +[](const ProxyArgs& a) -> void {
      proxy_command_qset_qclear(a, false);
    });

ChatCommandDefinition cc_qfread(
    {"$qfread"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();

      uint8_t counter_index;
      uint32_t mask;
      try {
        const auto& def = s->quest_counter_fields.at(a.text);
        counter_index = def.first;
        mask = def.second;
      } catch (const out_of_range&) {
        throw precondition_failed("$C4Invalid field name");
      }
      if (mask == 0) {
        throw runtime_error("invalid quest counter definition");
      }

      uint32_t counter_value = a.c->character()->quest_counters.at(counter_index) & mask;

      while (!(mask & 1)) {
        mask >>= 1;
        counter_value >>= 1;
      }

      if (mask == 1) {
        send_text_message_printf(a.c, "$C7Field %s\nhas value %s", a.text.c_str(), counter_value ? "TRUE" : "FALSE");
      } else {
        send_text_message_printf(a.c, "$C7Field %s\nhas value %" PRIu32, a.text.c_str(), counter_value);
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_qgread(
    {"$qgread"},
    +[](const ServerArgs& a) -> void {
      uint8_t counter_num = stoul(a.text, nullptr, 0);
      const auto& counters = a.c->character()->quest_counters;
      if (counter_num >= counters.size()) {
        throw precondition_failed_printf("$C7Counter ID must be\nless than %zu", counters.size());
      } else {
        send_text_message_printf(a.c, "$C7Quest counter %hhu\nhas value %" PRIu32, counter_num, counters[counter_num].load());
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_qgwrite(
    {"$qgwrite"},
    +[](const ServerArgs& a) -> void {
      if (a.c->version() != Version::BB_V4) {
        throw precondition_failed("$C6This command can\nonly be used on BB");
      }
      a.check_debug_enabled();
      auto l = a.c->require_lobby();
      if (!l->is_game()) {
        throw precondition_failed("$C6This command cannot\nbe used in the lobby");
      }

      auto tokens = phosg::split(a.text, ' ');
      if (tokens.size() != 2) {
        throw precondition_failed("$C6Incorrect number\nof arguments");
      }

      uint8_t counter_num = stoul(tokens[0], nullptr, 0);
      uint32_t value = stoul(tokens[1], nullptr, 0);
      auto& counters = a.c->character()->quest_counters;
      if (counter_num >= counters.size()) {
        throw precondition_failed_printf("$C7Counter ID must be\nless than %zu", counters.size());
      } else {
        a.c->character()->quest_counters[counter_num] = value;
        G_SetQuestCounter_BB_6xD2 cmd = {{0xD2, sizeof(G_SetQuestCounter_BB_6xD2) / 4, a.c->lobby_client_id}, counter_num, value};
        send_command_t(a.c, 0x60, 0x00, cmd);
        send_text_message_printf(a.c, "$C7Quest counter %hhu\nset to %" PRIu32, counter_num, value);
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_qset(
    {"$qset"},
    +[](const ServerArgs& a) -> void {
      return server_command_qset_qclear(a, true);
    },
    +[](const ProxyArgs& a) {
      return proxy_command_qset_qclear(a, true);
    });

static void server_command_qsync_qsyncall(const ServerArgs& a, bool send_to_lobby) {
  a.check_debug_enabled();

  auto l = a.c->require_lobby();
  if (!l->is_game()) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  auto tokens = phosg::split(a.text, ' ');
  if (tokens.size() != 2) {
    throw precondition_failed("$C6Incorrect number of\narguments");
  }

  G_SyncQuestRegister_6x77 cmd;
  cmd.header = {0x77, 0x03, 0x0000};
  cmd.register_number = stoul(tokens[0].substr(1), nullptr, 0);
  cmd.unused = 0;
  if (tokens[0][0] == 'r') {
    cmd.value.as_int = stoul(tokens[1], nullptr, 0);
  } else if (tokens[0][0] == 'f') {
    cmd.value.as_float = stof(tokens[1]);
  } else {
    throw precondition_failed("$C6First argument must\nbe a register");
  }
  if (send_to_lobby) {
    send_command_t(l, 0x60, 0x00, cmd);
  } else {
    send_command_t(a.c, 0x60, 0x00, cmd);
  }
}

static void proxy_command_qsync_qsyncall(const ProxyArgs& a, bool send_to_lobby) {
  if (!a.ses->is_in_game) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  auto tokens = phosg::split(a.text, ' ');
  if (tokens.size() != 2) {
    throw precondition_failed("$C6Incorrect number of\narguments");
  }

  G_SyncQuestRegister_6x77 cmd;
  cmd.header = {0x77, 0x03, 0x0000};
  cmd.register_number = stoul(tokens[0].substr(1), nullptr, 0);
  cmd.unused = 0;
  if (tokens[0][0] == 'r') {
    cmd.value.as_int = stoul(tokens[1], nullptr, 0);
  } else if (tokens[0][0] == 'f') {
    cmd.value.as_float = stof(tokens[1]);
  } else {
    throw precondition_failed("$C6First argument must\nbe a register");
  }
  a.ses->client_channel.send(0x60, 0x00, cmd);
  if (send_to_lobby) {
    a.ses->server_channel.send(0x60, 0x00, cmd);
  }
}

ChatCommandDefinition cc_qsync(
    {"$qsync"},
    +[](const ServerArgs& a) -> void {
      server_command_qsync_qsyncall(a, false);
    },
    +[](const ProxyArgs& a) {
      proxy_command_qsync_qsyncall(a, false);
    });

ChatCommandDefinition cc_qsyncall(
    {"$qsyncall"},
    +[](const ServerArgs& a) -> void {
      server_command_qsync_qsyncall(a, true);
    },
    +[](const ProxyArgs& a) {
      proxy_command_qsync_qsyncall(a, true);
    });

ChatCommandDefinition cc_quest(
    {"$quest"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      Version effective_version = is_ep3(a.c->version()) ? Version::GC_V3 : a.c->version();
      auto q = s->quest_index(effective_version)->get(stoul(a.text));
      if (!q) {
        throw precondition_failed("$C6Quest not found");
      }

      if (a.check_permissions && !a.c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        if (l->count_clients() > 1) {
          throw precondition_failed("$C6This command can only\nbe used with no\nother players present");
        }
        if (!q->allow_start_from_chat_command) {
          throw precondition_failed("$C6This quest cannot\nbe started with the\n%squest command");
        }
      }

      set_lobby_quest(a.c->require_lobby(), q, true);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_rand(
    {"$rand"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(false);
      a.check_cheats_allowed(s->cheat_flags.override_random_seed);

      if (a.text.empty()) {
        a.c->config.clear_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
        a.c->config.override_random_seed = 0;
        send_text_message(a.c, "$C6Override seed\nremoved");
      } else {
        a.c->config.set_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
        a.c->config.override_random_seed = stoul(a.text, 0, 16);
        send_text_message(a.c, "$C6Override seed\nset");
      }
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.override_random_seed);
      if (a.text.empty()) {
        a.ses->config.clear_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
        a.ses->config.override_random_seed = 0;
        send_text_message(a.ses->client_channel, "$C6Override seed\nremoved");
      } else {
        a.ses->config.set_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED);
        a.ses->config.override_random_seed = stoul(a.text, 0, 16);
        send_text_message(a.ses->client_channel, "$C6Override seed\nset");
      }
    });

static bool console_address_in_range(Version version, uint32_t addr) {
  if (is_dc(version)) {
    return ((addr > 0x8C000000) && (addr <= 0x8CFFFFFC));
  } else if (is_gc(version)) {
    return ((addr > 0x80000000) && (addr <= 0x817FFFFC));
  } else {
    return true;
  }
}

ChatCommandDefinition cc_readmem(
    {"$readmem"},
    +[](const ServerArgs& a) -> void {
      a.check_debug_enabled();

      uint32_t addr = stoul(a.text, nullptr, 16);
      if (!console_address_in_range(a.c->version(), addr)) {
        throw precondition_failed("$C4Address out of\nrange");
      }

      prepare_client_for_patches(a.c, [wc = weak_ptr<Client>(a.c), addr]() {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        try {
          auto s = c->require_server_state();
          const char* function_name = is_dc(c->version())
              ? "ReadMemoryWordDC"
              : is_gc(c->version())
              ? "ReadMemoryWordGC"
              : "ReadMemoryWordX86";
          auto fn = s->function_code_index->name_to_function.at(function_name);
          send_function_call(c, fn, {{"address", addr}});
          c->function_call_response_queue.emplace_back([wc = weak_ptr<Client>(c), addr](uint32_t ret, uint32_t) {
            auto c = wc.lock();
            if (c) {
              string data_str;
              if (is_big_endian(c->version())) {
                be_uint32_t v = ret;
                data_str = phosg::format_data_string(&v, sizeof(v));
              } else {
                le_uint32_t v = ret;
                data_str = phosg::format_data_string(&v, sizeof(v));
              }
              send_text_message_printf(c, "Bytes at %08" PRIX32 ":\n$C6%s", addr, data_str.c_str());
            }
          });
        } catch (const out_of_range&) {
          throw precondition_failed("Invalid patch name");
        }
      });
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_save(
    {"$save"},
    +[](const ServerArgs& a) -> void {
      a.check_version(Version::BB_V4);
      a.c->save_all();
      send_text_message(a.c, "All data saved");
      a.c->reschedule_save_game_data_event();
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_savechar(
    {"$savechar"},
    +[](const ServerArgs& a) -> void {
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }
      server_command_bbchar_savechar(a, false);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_saverec(
    {"$saverec"},
    +[](const ServerArgs& a) -> void {
      if (!a.c->ep3_prev_battle_record) {
        throw precondition_failed("$C4No finished\nrecording is\npresent");
      }
      string file_path = file_path_for_recording(a.text, a.c->login->account->account_id);
      string data = a.c->ep3_prev_battle_record->serialize();
      phosg::save_file(file_path, data);
      send_text_message(a.c, "$C7Recording saved");
      a.c->ep3_prev_battle_record.reset();
    },
    unavailable_on_proxy_server);

static void server_command_send_command(const ServerArgs& a, bool to_client, bool to_server) {
  a.check_debug_enabled();
  string data = phosg::parse_data_string(a.text);
  data.resize((data.size() + 3) & (~3));
  if (to_client) {
    a.c->channel.send(data);
  }
  if (to_server) {
    on_command_with_header(a.c, data);
  }
}

static void proxy_command_send_command(const ProxyArgs& a, bool to_client, bool to_server) {
  string data = phosg::parse_data_string(a.text);
  data.resize((data.size() + 3) & (~3));
  if (to_client) {
    a.ses->client_channel.send(data);
  }
  if (to_server) {
    a.ses->server_channel.send(data);
  }
}

ChatCommandDefinition cc_sb(
    {"$sb"},
    +[](const ServerArgs& a) -> void {
      server_command_send_command(a, true, true);
    },
    +[](const ProxyArgs& a) -> void {
      proxy_command_send_command(a, true, true);
    });

ChatCommandDefinition cc_sc(
    {"$sc"},
    +[](const ServerArgs& a) -> void {
      server_command_send_command(a, true, false);
    },
    +[](const ProxyArgs& a) -> void {
      proxy_command_send_command(a, true, false);
    });

ChatCommandDefinition cc_secid(
    {"$secid"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      auto s = a.c->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.override_section_id);

      uint8_t new_override_section_id;

      if (a.text.empty()) {
        new_override_section_id = 0xFF;
        send_text_message(a.c, "$C6Override section ID\nremoved");
      } else {
        new_override_section_id = section_id_for_name(a.text);
        if (new_override_section_id == 0xFF) {
          throw precondition_failed("$C6Invalid section ID");
        } else {
          send_text_message_printf(a.c, "$C6Override section ID\nset to %s", name_for_section_id(new_override_section_id));
        }
      }

      a.c->config.override_section_id = new_override_section_id;
      if (l->is_game() && (l->leader_id == a.c->lobby_client_id)) {
        l->override_section_id = new_override_section_id;
        l->create_item_creator();
      }
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.check_cheats_allowed(s->cheat_flags.override_section_id);

      if (a.text.empty()) {
        a.ses->config.override_section_id = 0xFF;
        send_text_message(a.ses->client_channel, "$C6Override section ID\nremoved");
      } else {
        uint8_t new_secid = section_id_for_name(a.text);
        if (new_secid == 0xFF) {
          throw precondition_failed("$C6Invalid section ID");
        } else {
          a.ses->config.override_section_id = new_secid;
          send_text_message_printf(a.ses->client_channel, "$C6Override section ID\nset to %s", name_for_section_id(new_secid));
        }
      }
    });

ChatCommandDefinition cc_setassist(
    {"$setassist"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_cheats_enabled(s->cheat_flags.ep3_replace_assist);

      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
        throw precondition_failed("$C6Battle has not\nyet begun");
      }
      if (a.text.empty()) {
        throw precondition_failed("$C6Missing arguments");
      }

      size_t client_id;
      string card_name;
      if (isdigit(a.text[0])) {
        auto tokens = phosg::split(a.text, ' ', 1);
        client_id = stoul(tokens.at(0), nullptr, 0) - 1;
        card_name = tokens.at(1);
      } else {
        client_id = a.c->lobby_client_id;
        card_name = a.text;
      }
      if (client_id >= 4) {
        throw precondition_failed("$C6Invalid client ID");
      }

      shared_ptr<const Episode3::CardIndex::CardEntry> ce;
      try {
        ce = l->ep3_server->options.card_index->definition_for_name_normalized(card_name);
      } catch (const out_of_range&) {
        throw precondition_failed("$C6Card not found");
      }
      if (ce->def.type != Episode3::CardType::ASSIST) {
        throw precondition_failed("$C6Card is not an\nAssist card");
      }
      l->ep3_server->force_replace_assist_card(client_id, ce->def.card_id);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_server_info(
    {"$si"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      string uptime_str = phosg::format_duration(phosg::now() - s->creation_time);
      if (s->proxy_server) {
        send_text_message_printf(a.c,
            "Uptime: $C6%s$C7\nLobbies: $C6%zu$C7\nClients: $C6%zu$C7(g) $C6%zu$C7(p)",
            uptime_str.c_str(),
            s->id_to_lobby.size(),
            s->channel_to_client.size(),
            s->proxy_server->num_sessions());
      } else {
        send_text_message_printf(a.c,
            "Uptime: $C6%s$C7\nLobbies: $C6%zu$C7\nClients: $C6%zu",
            uptime_str.c_str(),
            s->id_to_lobby.size(),
            s->channel_to_client.size());
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_silence(
    {"$silence"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_account_flag(Account::Flag::SILENCE_USER);

      auto target = s->find_client(&a.text);
      if (!target->login) {
        // this should be impossible, but I'll bet it's not actually
        throw precondition_failed("$C6Client not logged in");
      }

      if (target->login->account->check_flag(Account::Flag::SILENCE_USER)) {
        throw precondition_failed("$C6You do not have\nsufficient privileges.");
      }

      target->can_chat = !target->can_chat;
      string target_name = name_for_client(target);
      send_text_message_printf(a.c, "$C6%s %ssilenced", target_name.c_str(),
          target->can_chat ? "un" : "");
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_song(
    {"$song"},
    +[](const ServerArgs& a) -> void {
      a.check_is_ep3(true);
      send_ep3_change_music(a.c->channel, stoul(a.text, nullptr, 0));
    },
    +[](const ProxyArgs& a) {
      int32_t song = stol(a.text, nullptr, 0);
      if (song < 0) {
        song = -song;
        send_ep3_change_music(a.ses->server_channel, song);
      }
      send_ep3_change_music(a.ses->client_channel, song);
    });

ChatCommandDefinition cc_spec(
    {"$spec"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      if (!l->is_ep3()) {
        throw logic_error("Episode 3 client in non-Episode 3 game");
      }

      // In non-tournament games, only the leader can do this; in a tournament
      // match, the players don't have control over who the leader is, so we allow
      // all players to use this command
      if (!l->tournament_match) {
        a.check_is_leader();
      }

      if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
        throw precondition_failed("$C6This command cannot\nbe used in a spectator\nteam");
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
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_ss(
    {"$ss"},
    +[](const ServerArgs& a) -> void {
      server_command_send_command(a, false, true);
    },
    +[](const ProxyArgs& a) -> void {
      proxy_command_send_command(a, false, true);
    });

ChatCommandDefinition cc_stat(
    {"$stat"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
        throw precondition_failed("$C6Battle has not\nyet started");
      }
      auto ps = l->ep3_server->player_states.at(a.c->lobby_client_id);
      if (!ps) {
        throw precondition_failed("$C6Player is missing");
      }
      uint8_t team_id = ps->get_team_id();
      if (team_id > 1) {
        throw logic_error("team ID is incorrect");
      }

      if (a.text == "rank") {
        float score = ps->stats.score(l->ep3_server->get_round_num());
        uint8_t rank = ps->stats.rank_for_score(score);
        const char* rank_name = ps->stats.name_for_rank(rank);
        send_text_message_printf(a.c, "$C7Score: %g\nRank: %hhu (%s)", score, rank, rank_name);
      } else if (a.text == "duration") {
        string s = phosg::format_duration(phosg::now() - l->ep3_server->battle_start_usecs);
        send_text_message_printf(a.c, "$C7Duration: %s", s.c_str());
      } else if (a.text == "fcs-destroyed") {
        send_text_message_printf(a.c, "$C7Team FCs destroyed:\n%" PRIu32, l->ep3_server->team_num_ally_fcs_destroyed[team_id]);
      } else if (a.text == "cards-destroyed") {
        send_text_message_printf(a.c, "$C7Team cards destroyed:\n%" PRIu32, l->ep3_server->team_num_cards_destroyed[team_id]);
      } else if (a.text == "damage-given") {
        send_text_message_printf(a.c, "$C7Damage given: %hu", ps->stats.damage_given.load());
      } else if (a.text == "damage-taken") {
        send_text_message_printf(a.c, "$C7Damage taken: %hu", ps->stats.damage_taken.load());
      } else if (a.text == "opp-cards-destroyed") {
        send_text_message_printf(a.c, "$C7Opp. cards destroyed:\n%hu", ps->stats.num_opponent_cards_destroyed.load());
      } else if (a.text == "own-cards-destroyed") {
        send_text_message_printf(a.c, "$C7Own cards destroyed:\n%hu", ps->stats.num_owned_cards_destroyed.load());
      } else if (a.text == "move-distance") {
        send_text_message_printf(a.c, "$C7Move distance: %hu", ps->stats.total_move_distance.load());
      } else if (a.text == "cards-set") {
        send_text_message_printf(a.c, "$C7Cards set: %hu", ps->stats.num_cards_set.load());
      } else if (a.text == "fcs-set") {
        send_text_message_printf(a.c, "$C7FC cards set: %hu", ps->stats.num_item_or_creature_cards_set.load());
      } else if (a.text == "attack-actions-set") {
        send_text_message_printf(a.c, "$C7Attack actions set:\n%hu", ps->stats.num_attack_actions_set.load());
      } else if (a.text == "techs-set") {
        send_text_message_printf(a.c, "$C7Techs set: %hu", ps->stats.num_tech_cards_set.load());
      } else if (a.text == "assists-set") {
        send_text_message_printf(a.c, "$C7Assists set: %hu", ps->stats.num_assist_cards_set.load());
      } else if (a.text == "defenses-self") {
        send_text_message_printf(a.c, "$C7Defenses on self:\n%hu", ps->stats.defense_actions_set_on_self.load());
      } else if (a.text == "defenses-ally") {
        send_text_message_printf(a.c, "$C7Defenses on ally:\n%hu", ps->stats.defense_actions_set_on_ally.load());
      } else if (a.text == "cards-drawn") {
        send_text_message_printf(a.c, "$C7Cards drawn: %hu", ps->stats.num_cards_drawn.load());
      } else if (a.text == "max-attack-damage") {
        send_text_message_printf(a.c, "$C7Maximum attack damage:\n%hu", ps->stats.max_attack_damage.load());
      } else if (a.text == "max-combo") {
        send_text_message_printf(a.c, "$C7Longest combo: %hu", ps->stats.max_attack_combo_size.load());
      } else if (a.text == "attacks-given") {
        send_text_message_printf(a.c, "$C7Attacks given: %hu", ps->stats.num_attacks_given.load());
      } else if (a.text == "attacks-taken") {
        send_text_message_printf(a.c, "$C7Attacks taken: %hu", ps->stats.num_attacks_taken.load());
      } else if (a.text == "sc-damage") {
        send_text_message_printf(a.c, "$C7SC damage taken: %hu", ps->stats.sc_damage_taken.load());
      } else if (a.text == "damage-defended") {
        send_text_message_printf(a.c, "$C7Damage defended: %hu", ps->stats.action_card_negated_damage.load());
      } else {
        throw precondition_failed("$C6Unknown statistic");
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_surrender(
    {"$surrender"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
        throw precondition_failed("$C6Battle has not\nyet started");
      }
      auto ps = l->ep3_server->get_player_state(a.c->lobby_client_id);
      if (!ps || !ps->is_alive()) {
        throw precondition_failed("$C6Defeated players\ncannot surrender");
      }
      string name = remove_color(a.c->character()->disp.name.decode(a.c->language()));
      send_text_message_printf(l, "$C6%s has\nsurrendered", name.c_str());
      for (const auto& watcher_l : l->watcher_lobbies) {
        send_text_message_printf(watcher_l, "$C6%s has\nsurrendered", name.c_str());
      }
      l->ep3_server->force_battle_result(a.c->lobby_client_id, false);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_swa(
    {"$swa"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      a.c->config.toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
      send_text_message_printf(a.c, "$C6Switch assist %s",
          a.c->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? "enabled" : "disabled");
    },
    +[](const ProxyArgs& a) {
      auto s = a.ses->require_server_state();
      a.ses->config.toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
      send_text_message_printf(a.ses->client_channel, "$C6Switch assist %s",
          a.ses->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? "enabled" : "disabled");
    });

static void server_command_swset_swclear(const ServerArgs& a, bool should_set) {
  a.check_debug_enabled();
  auto l = a.c->require_lobby();
  if (!l->is_game()) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  auto tokens = phosg::split(a.text, ' ');
  uint8_t floor, flag_num;
  if (tokens.size() == 1) {
    floor = a.c->floor;
    flag_num = stoul(tokens[0], nullptr, 0);
  } else if (tokens.size() == 2) {
    floor = stoul(tokens[0], nullptr, 0);
    flag_num = stoul(tokens[1], nullptr, 0);
  } else {
    throw precondition_failed("$C4Incorrect parameters");
  }

  if (should_set) {
    l->switch_flags->set(floor, flag_num);
  } else {
    l->switch_flags->clear(floor, flag_num);
  }

  uint8_t cmd_flags = should_set ? 0x01 : 0x00;
  G_SwitchStateChanged_6x05 cmd = {{0x05, 0x03, 0xFFFF}, 0, 0, flag_num, floor, cmd_flags};
  send_command_t(l, 0x60, 0x00, cmd);
}

static void proxy_command_swset_swclear(const ProxyArgs& a, bool should_set) {
  if (!a.ses->is_in_game) {
    throw precondition_failed("$C6This command cannot\nbe used in the lobby");
  }

  auto tokens = phosg::split(a.text, ' ');
  uint8_t floor, flag_num;
  if (tokens.size() == 1) {
    floor = a.ses->floor;
    flag_num = stoul(tokens[0], nullptr, 0);
  } else if (tokens.size() == 2) {
    floor = stoul(tokens[0], nullptr, 0);
    flag_num = stoul(tokens[1], nullptr, 0);
  } else {
    throw precondition_failed("$C4Incorrect parameters");
  }

  uint8_t cmd_flags = should_set ? 0x01 : 0x00;
  G_SwitchStateChanged_6x05 cmd = {{0x05, 0x03, 0xFFFF}, 0, 0, flag_num, floor, cmd_flags};
  a.ses->client_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
  a.ses->server_channel.send(0x60, 0x00, &cmd, sizeof(cmd));
}

ChatCommandDefinition cc_swclear(
    {"$swclear"},
    +[](const ServerArgs& a) -> void {
      server_command_swset_swclear(a, false);
    },
    +[](const ProxyArgs& a) {
      proxy_command_swset_swclear(a, false);
    });

ChatCommandDefinition cc_swset(
    {"$swset"},
    +[](const ServerArgs& a) -> void {
      return server_command_swset_swclear(a, true);
    },
    +[](const ProxyArgs& a) {
      return proxy_command_swset_swclear(a, true);
    });

ChatCommandDefinition cc_swsetall(
    {"$swsetall"},
    +[](const ServerArgs& a) -> void {
      a.check_debug_enabled();

      auto l = a.c->require_lobby();
      if (!l->is_game()) {
        throw precondition_failed("$C6This command cannot\nbe used in the lobby");
      }

      l->switch_flags->data[a.c->floor].clear(0xFF);

      parray<G_SwitchStateChanged_6x05, 0x100> cmds;
      for (size_t z = 0; z < cmds.size(); z++) {
        auto& cmd = cmds[z];
        cmd.header.subcommand = 0x05;
        cmd.header.size = 0x03;
        cmd.header.entity_id = 0xFFFF;
        cmd.switch_flag_floor = a.c->floor;
        cmd.switch_flag_num = z;
        cmd.flags = 0x01;
      }
      cmds[0].flags = 0x03; // Play room unlock sound
      send_command_t(l, 0x6C, 0x00, cmds);
    },
    +[](const ProxyArgs& a) {
      if (!a.ses->is_in_game) {
        throw precondition_failed("$C6This command cannot\nbe used in the lobby");
      }

      parray<G_SwitchStateChanged_6x05, 0x100> cmds;
      for (size_t z = 0; z < cmds.size(); z++) {
        auto& cmd = cmds[z];
        cmd.header.subcommand = 0x05;
        cmd.header.size = 0x03;
        cmd.header.entity_id = 0xFFFF;
        cmd.switch_flag_floor = a.ses->floor;
        cmd.switch_flag_num = z;
        cmd.flags = 0x01;
      }
      cmds[0].flags = 0x03; // Play room unlock sound
      a.ses->client_channel.send(0x6C, 0x00, &cmds, sizeof(cmds));
      a.ses->server_channel.send(0x6C, 0x00, &cmds, sizeof(cmds));
    });

ChatCommandDefinition cc_unset(
    {"$unset"},
    +[](const ServerArgs& a) -> void {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_cheats_enabled(s->cheat_flags.ep3_unset_field_character);

      if (l->episode != Episode::EP3) {
        throw logic_error("non-Ep3 client in Ep3 game");
      }
      if (!l->ep3_server) {
        throw precondition_failed("$C6Episode 3 server\nis not initialized");
      }
      if (l->ep3_server->setup_phase != Episode3::SetupPhase::MAIN_BATTLE) {
        throw precondition_failed("$C6Battle has not\nyet begun");
      }

      size_t index = stoull(a.text) - 1;
      l->ep3_server->force_destroy_field_character(a.c->lobby_client_id, index);
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_variations(
    {"$variations"},
    +[](const ServerArgs& a) -> void {
      // Note: This command is intentionally undocumented, since it's primarily used
      // for testing. If we ever make it public, we should add some kind of user
      // feedback (currently it sends no message when it runs).
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(false);
      a.check_cheats_allowed(s->cheat_flags.override_variations);

      a.c->override_variations = make_unique<Variations>();
      for (size_t z = 0; z < min<size_t>(a.c->override_variations->entries.size() * 2, a.text.size()); z++) {
        auto& entry = a.c->override_variations->entries.at(z / 2);
        if (z & 1) {
          entry.entities = a.text[z] - '0';
        } else {
          entry.layout = a.text[z] - '0';
        }
      }
      auto vars_str = a.c->override_variations->str();
      a.c->log.info("Override variations set to %s", vars_str.c_str());
    },
    unavailable_on_proxy_server);

static void server_command_warp(const ServerArgs& a, bool is_warpall) {
  auto s = a.c->require_server_state();
  auto l = a.c->require_lobby();
  a.check_is_game(true);
  a.check_cheats_enabled(s->cheat_flags.warp);

  uint32_t floor = stoul(a.text, nullptr, 0);
  if (!is_warpall && (a.c->floor == floor)) {
    return;
  }

  size_t limit = floor_limit_for_episode(l->episode);
  if (limit == 0) {
    return;
  } else if (floor > limit) {
    throw precondition_failed_printf("$C6Area numbers must\nbe %zu or less", limit);
  }

  if (is_warpall) {
    send_warp(l, floor, false);
  } else {
    send_warp(a.c, floor, true);
  }
}

static void proxy_command_warp(const ProxyArgs& a, bool is_warpall) {
  auto s = a.ses->require_server_state();
  a.check_cheats_allowed(s->cheat_flags.warp);
  if (!a.ses->is_in_game) {
    throw precondition_failed("$C6You must be in a\ngame to use this\ncommand");
  }
  uint32_t floor = stoul(a.text, nullptr, 0);
  send_warp(a.ses->client_channel, a.ses->lobby_client_id, floor, !is_warpall);
  if (is_warpall) {
    send_warp(a.ses->server_channel, a.ses->lobby_client_id, floor, false);
  }
}

ChatCommandDefinition cc_warp(
    {"$warp", "$warpme"},
    +[](const ServerArgs& a) -> void {
      server_command_warp(a, false);
    },
    +[](const ProxyArgs& a) {
      proxy_command_warp(a, false);
    });

ChatCommandDefinition cc_warpall(
    {"$warpall"},
    +[](const ServerArgs& a) -> void {
      server_command_warp(a, true);
    },
    +[](const ProxyArgs& a) {
      proxy_command_warp(a, true);
    });

ChatCommandDefinition cc_what(
    {"$what"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      if (!episode_has_arpg_semantics(l->episode)) {
        return;
      }

      float min_dist2 = 0.0f;
      shared_ptr<const Lobby::FloorItem> nearest_fi;
      for (const auto& it : l->floor_item_managers.at(a.c->floor).items) {
        if (!it.second->visible_to_client(a.c->lobby_client_id)) {
          continue;
        }
        float dist2 = (it.second->pos - a.c->pos).norm2();
        if (!nearest_fi || (dist2 < min_dist2)) {
          nearest_fi = it.second;
          min_dist2 = dist2;
        }
      }

      if (!nearest_fi) {
        throw precondition_failed("$C4No items are near you");
      } else {
        auto s = a.c->require_server_state();
        string name = s->describe_item(a.c->version(), nearest_fi->data, true);
        send_text_message(a.c, name);
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_where(
    {"$where"},
    +[](const ServerArgs& a) -> void {
      auto l = a.c->require_lobby();
      send_text_message_printf(a.c, "$C7%01" PRIX32 ":%s X:%" PRId32 " Z:%" PRId32,
          a.c->floor,
          short_name_for_floor(l->episode, a.c->floor),
          static_cast<int32_t>(a.c->pos.x.load()),
          static_cast<int32_t>(a.c->pos.z.load()));
      for (auto lc : l->clients) {
        if (lc && (lc != a.c)) {
          string name = lc->character()->disp.name.decode(lc->language());
          send_text_message_printf(a.c, "$C6%s$C7 %01" PRIX32 ":%s",
              name.c_str(), lc->floor, short_name_for_floor(l->episode, lc->floor));
        }
      }
    },
    unavailable_on_proxy_server);

ChatCommandDefinition cc_writemem(
    {"$writemem"},
    +[](const ServerArgs& a) -> void {
      a.check_debug_enabled();

      auto tokens = phosg::split(a.text, ' ');
      if (tokens.size() < 2) {
        throw precondition_failed("Incorrect arguments");
      }

      uint32_t addr = stoul(tokens[0], nullptr, 16);
      if (!console_address_in_range(a.c->version(), addr)) {
        throw precondition_failed("$C4Address out of\nrange");
      }

      tokens.erase(tokens.begin());
      std::string data = phosg::parse_data_string(phosg::join(tokens, " "));

      prepare_client_for_patches(a.c, [wc = weak_ptr<Client>(a.c), addr, data]() {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        try {
          auto s = c->require_server_state();
          const char* function_name = is_dc(c->version())
              ? "WriteMemoryDC"
              : is_gc(c->version())
              ? "WriteMemoryGC"
              : "WriteMemoryX86";
          auto fn = s->function_code_index->name_to_function.at(function_name);
          send_function_call(c, fn, {{"dest_addr", addr}, {"size", data.size()}}, data.data(), data.size());
          c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
        } catch (const out_of_range&) {
          throw precondition_failed("Invalid patch name");
        }
      });
    },
    unavailable_on_proxy_server);

////////////////////////////////////////////////////////////////////////////////
// Dispatch methods

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
void on_chat_command(std::shared_ptr<Client> c, const std::string& text, bool check_permissions) {
  SplitCommand cmd(text);
  if (!cmd.name.empty() && cmd.name[0] == '@') {
    cmd.name[0] = '$';
  }

  const ChatCommandDefinition* def = nullptr;
  try {
    def = ChatCommandDefinition::all_defs.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(c, "$C6Unknown command");
    return;
  }

  if (!def->server_handler) {
    send_text_message(c, "$C6Command not available\non game server");
  } else {
    try {
      def->server_handler(ServerArgs{{cmd.args, check_permissions}, c});
    } catch (const precondition_failed& e) {
      send_text_message(c, e.what());
    } catch (const exception& e) {
      send_text_message(c, "$C6Failed:\n" + remove_color(e.what()));
    }
  }
}

void on_chat_command(shared_ptr<ProxyServer::LinkedSession> ses, const std::string& text, bool check_permissions) {
  SplitCommand cmd(text);

  const ChatCommandDefinition* def = nullptr;
  try {
    def = ChatCommandDefinition::all_defs.at(cmd.name);
  } catch (const out_of_range&) {
    send_text_message(ses->client_channel, "$C6Unknown command");
    return;
  }

  if (!def->proxy_handler) {
    send_text_message(ses->client_channel, "$C6Command not available\non proxy server");
  } else {
    try {
      def->proxy_handler(ProxyArgs{{cmd.args, check_permissions}, ses});
    } catch (const precondition_failed& e) {
      send_text_message(ses->client_channel, e.what());
    } catch (const exception& e) {
      send_text_message(ses->client_channel, "$C6Failed:\n" + remove_color(e.what()));
    }
  }
}
