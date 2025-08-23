#include "ChatCommands.hh"

#include <string.h>

#include <filesystem>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#include <string>
#include <unordered_map>
#include <vector>

#include "Client.hh"
#include "GameServer.hh"
#include "Lobby.hh"
#include "Loggers.hh"
#include "ProxySession.hh"
#include "ReceiveCommands.hh"
#include "Revision.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Tools

string str_for_flag_ranges(const vector<bool>& flags) {
  string ret;
  auto add_result = [&](size_t start, size_t end) {
    if (!ret.empty()) {
      ret.push_back(',');
    }
    if (start == end) {
      ret += std::format("{}", start);
    } else if (start == end - 1) {
      ret += std::format("{},{}", start, end);
    } else {
      ret += std::format("{}-{}", start, end);
    }
  };

  size_t range_start = 0;
  bool in_range = false;
  for (size_t z = 0; z < flags.size(); z++) {
    if (flags[z] && !in_range) {
      in_range = true;
      range_start = z;
    } else if (!flags[z] && in_range) {
      in_range = false;
      add_result(range_start, z - 1);
    }
  }
  if (in_range) {
    add_result(range_start, flags.size() - 1);
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// Checks

class precondition_failed {
public:
  template <typename... ArgTs>
  precondition_failed(std::format_string<ArgTs...> fmt, ArgTs&&... args)
      : user_msg(std::format(fmt, std::forward<ArgTs>(args)...)) {}

  ~precondition_failed() = default;

  const std::string& what() const {
    return this->user_msg;
  }

private:
  std::string user_msg;
};

struct Args {
  std::string text;
  bool check_permissions;
  std::shared_ptr<Client> c;

  void check_is_proxy(bool is_proxy) const {
    if (is_proxy && (this->c->proxy_session == nullptr)) {
      throw precondition_failed("$C6This command can\nonly be used in\nproxy sessions.");
    } else if (!is_proxy && (this->c->proxy_session != nullptr)) {
      throw precondition_failed("$C6This command cannot\nbe used in proxy\nsessions.");
    }
  }

  void check_version(Version version) const {
    if (this->c->version() != version) {
      throw precondition_failed("$C6This command cannot\nbe used for your\nversion of PSO.");
    }
  }

  void check_is_ep3(bool is_ep3) const {
    if (::is_ep3(this->c->version()) && !is_ep3) {
      throw precondition_failed("$C6This command cannot\nbe used in Episode 3.");
    } else if (!::is_ep3(this->c->version()) && is_ep3) {
      throw precondition_failed("$C6This command can only\nbe used in Episode 3.");
    }
  }

  void check_is_game(bool is_game) const {
    bool client_in_game = this->c->proxy_session
        ? this->c->proxy_session->is_in_game
        : this->c->require_lobby()->is_game();
    if (client_in_game && !is_game) {
      throw precondition_failed("$C6This command cannot\nbe used in games.");
    } else if (!client_in_game && is_game) {
      throw precondition_failed("$C6This command cannot\nbe used in lobbies.");
    }
  }

  void check_account_flag(Account::Flag flag) const {
    if (!this->c->login) {
      throw precondition_failed("$C6You are not\nlogged in.");
    }
    if (this->check_permissions && !this->c->login->account->check_flag(flag)) {
      throw precondition_failed("$C6You do not have\npermission to\nrun this command.");
    }
  }

  void check_debug_enabled() const {
    if (this->check_permissions && !this->c->check_flag(Client::Flag::DEBUG_ENABLED)) {
      throw precondition_failed("$C6This command can only\nbe run in debug mode\n(run %sdebug first)");
    }
  }

  void check_cheats_enabled_in_game(bool behavior_is_cheating) const {
    if (behavior_is_cheating &&
        this->check_permissions &&
        !this->c->require_lobby()->check_flag(Lobby::Flag::CHEATS_ENABLED) &&
        !this->c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)) {
      throw precondition_failed("$C6This command can\nonly be used in\ncheat mode.");
    }
  }

  void check_cheat_mode_available(bool behavior_is_cheating) const {
    if (behavior_is_cheating &&
        this->check_permissions &&
        (this->c->require_server_state()->cheat_mode_behavior == ServerState::BehaviorSwitch::OFF) &&
        (!this->c->login || !this->c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE))) {
      throw precondition_failed("$C6Cheats are disabled");
    }
  }

  void check_cheats_enabled_or_allowed(bool behavior_is_cheating) const {
    if (this->c->proxy_session) {
      this->check_cheat_mode_available(behavior_is_cheating);
    } else {
      this->check_cheats_enabled_in_game(behavior_is_cheating);
    }
  }

  void check_is_leader() const {
    if (this->check_permissions && (this->c->require_lobby()->leader_id != this->c->lobby_client_id)) {
      throw precondition_failed("$C6This command can\nonly be used by\nthe game leader.");
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// Command definitions

struct ChatCommandDefinition {
  using Handler = asio::awaitable<void> (*)(const Args& args);

  std::vector<const char*> names;
  Handler handler;

  static unordered_map<string, const ChatCommandDefinition*> all_defs;

  ChatCommandDefinition(std::initializer_list<const char*> names, Handler handler)
      : names(names), handler(handler) {
    for (const char* name : this->names) {
      if (!this->all_defs.emplace(name, this).second) {
        throw logic_error("duplicate command definition: " + string(name));
      }
    }
  }
};

unordered_map<string, const ChatCommandDefinition*> ChatCommandDefinition::all_defs;

////////////////////////////////////////////////////////////////////////////////
// All commands (in alphabetical order)

ChatCommandDefinition cc_allevent(
    {"$allevent"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
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
      co_return;
    });

static asio::awaitable<void> server_command_announce_inner(const Args& a, bool mail, bool anonymous) {
  a.check_is_proxy(false);
  a.check_account_flag(Account::Flag::ANNOUNCE);

  auto s = a.c->require_server_state();
  if (anonymous) {
    if (mail) {
      send_simple_mail(s, 0, s->name, a.text);
    } else {
      send_text_or_scrolling_message(s, a.text, a.text);
    }
  } else {
    auto from_name = a.c->character_file()->disp.name.decode(a.c->language());
    if (mail) {
      send_simple_mail(s, 0, from_name, a.text);
    } else {
      auto message = from_name + ": " + a.text;
      send_text_or_scrolling_message(s, message, message);
    }
  }
  co_return;
}

ChatCommandDefinition cc_ann_named(
    {"$ann"},
    +[](const Args& a) -> asio::awaitable<void> {
      return server_command_announce_inner(a, false, false);
    });
ChatCommandDefinition cc_ann_anonymous(
    {"$ann?"},
    +[](const Args& a) -> asio::awaitable<void> {
      return server_command_announce_inner(a, false, true);
    });
ChatCommandDefinition cc_ann_mail_named(
    {"$ann!"},
    +[](const Args& a) -> asio::awaitable<void> {
      return server_command_announce_inner(a, true, false);
    });
ChatCommandDefinition cc_ann_mail_anonymous(
    {"$ann?!", "$ann!?"},
    +[](const Args& a) -> asio::awaitable<void> {
      return server_command_announce_inner(a, true, true);
    });

ChatCommandDefinition cc_announce_rares(
    {"$announcerares"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);

      a.c->login->account->toggle_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST);
      a.c->login->account->save();
      send_text_message_fmt(a.c, "$C6Rare announcements\n{} for your\nitems",
          a.c->login->account->check_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST) ? "disabled" : "enabled");
      co_return;
    });

ChatCommandDefinition cc_arrow(
    {"$arrow"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.text == "red") {
        a.c->lobby_arrow_color = 0x01;
      } else if (a.text == "blue") {
        a.c->lobby_arrow_color = 0x02;
      } else if (a.text == "green") {
        a.c->lobby_arrow_color = 0x03;
      } else if (a.text == "yellow") {
        a.c->lobby_arrow_color = 0x04;
      } else if (a.text == "purple") {
        a.c->lobby_arrow_color = 0x05;
      } else if (a.text == "cyan") {
        a.c->lobby_arrow_color = 0x06;
      } else if (a.text == "orange") {
        a.c->lobby_arrow_color = 0x07;
      } else if (a.text == "pink") {
        a.c->lobby_arrow_color = 0x08;
      } else if (a.text == "white") {
        a.c->lobby_arrow_color = 0x09;
      } else if (a.text == "white2") {
        a.c->lobby_arrow_color = 0x0A;
      } else if (a.text == "white3") {
        a.c->lobby_arrow_color = 0x0B;
      } else if (a.text == "black") {
        a.c->lobby_arrow_color = 0x0C;
      } else {
        a.c->lobby_arrow_color = stoull(a.text, nullptr, 0);
      }

      if (a.c->proxy_session) {
        a.c->proxy_session->server_channel->send(0x89, a.c->lobby_arrow_color);
      } else {
        auto l = a.c->require_lobby();
        if (!l->is_game()) {
          send_arrow_update(l);
        }
      }
      co_return;
    });

ChatCommandDefinition cc_auction(
    {"$auction"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_account_flag(Account::Flag::DEBUG);

      G_InitiateCardAuction_Ep3_6xB5x42 cmd;
      cmd.header.sender_client_id = a.c->lobby_client_id;

      if (a.c->proxy_session) {
        a.c->channel->send(0xC9, 0x00, &cmd, sizeof(cmd));
        a.c->proxy_session->server_channel->send(0xC9, 0x00, &cmd, sizeof(cmd));
      } else {
        auto l = a.c->require_lobby();
        if (l->is_game() && l->is_ep3()) {
          send_command_t(l, 0xC9, 0x00, cmd);
        }
      }
      co_return;
    });

static string name_for_client(shared_ptr<Client> c) {
  auto player = c->character_file(false);
  if (player.get()) {
    return escape_player_name(player->disp.name.decode(player->inventory.language));
  }

  if (c->login) {
    return std::format("SN:{}", c->login->account->account_id);
  }

  return "Player";
}

ChatCommandDefinition cc_ban(
    {"$ban"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_account_flag(Account::Flag::BAN_USER);

      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();

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
      target->channel->disconnect();
      send_text_message_fmt(a.c, "$C6{} banned", target_name);
      co_return;
    });

ChatCommandDefinition cc_bank(
    {"$bank"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_version(Version::BB_V4);
      if (a.c->check_flag(Client::Flag::AT_BANK_COUNTER)) {
        throw runtime_error("cannot change banks while at the bank counter");
      }
      if (a.c->has_overlay()) {
        throw runtime_error("cannot change banks while Battle or Challenge is in progress");
      }

      ssize_t new_char_index = a.text.empty() ? (a.c->bb_character_index + 1) : stol(a.text, nullptr, 0);

      if (new_char_index <= 0) {
        a.c->change_bank(-1);
        send_text_message(a.c, "$C6Using shared bank");

      } else if (new_char_index <= 127) {
        a.c->change_bank(new_char_index - 1);
        send_text_message_fmt(a.c, "$C6Using character {}'s bank", new_char_index);

      } else {
        throw precondition_failed("$C6Invalid bank number");
      }

      auto bank = a.c->bank_file();
      bank->assign_ids(0x99000000 + (a.c->lobby_client_id << 20));
      a.c->log.info_f("Assigned bank item IDs");
      a.c->print_bank();

      send_text_message_fmt(a.c, "{} items\n{} Meseta", bank->items.size(), bank->meseta);
      co_return;
    });

static asio::awaitable<void> server_command_bbchar_savechar(const Args& a, bool is_bb_conversion) {
  // TODO: We could support this in proxy sessions; we'd just have to handle the 61/30 correctly
  a.check_is_proxy(false);
  a.check_is_game(false);

  auto s = a.c->require_server_state();
  auto l = a.c->require_lobby();

  if (is_bb_conversion && is_ep3(a.c->version())) {
    throw precondition_failed("$C6Episode 3 players\ncannot be converted\nto BB format");
  }

  shared_ptr<Account> dest_account;
  shared_ptr<BBLicense> dest_bb_license;
  size_t dest_character_index = 0;
  if (is_bb_conversion) {
    vector<string> tokens = phosg::split(a.text, ' ');
    if (tokens.size() != 3) {
      throw precondition_failed("$C6Incorrect argument\ncount");
    }

    // username/password are tokens[0] and [1]
    dest_character_index = stoull(tokens[2]) - 1;
    if (dest_character_index >= 127) {
      throw precondition_failed("$C6Player index must\nbe in range 1-127");
    }

    try {
      auto dest_login = s->account_index->from_bb_credentials(tokens[0], &tokens[1], false);
      dest_account = dest_login->account;
      dest_bb_license = dest_login->bb_license;
    } catch (const exception& e) {
      throw precondition_failed("$C6Login failed: {}", e.what());
    }

  } else {
    dest_character_index = stoull(a.text) - 1;
    if (dest_character_index >= s->num_backup_character_slots) {
      throw precondition_failed("$C6Player index must\nbe in range 1-{}", s->num_backup_character_slots);
    }
    dest_account = a.c->login->account;
  }

  // If the client isn't BB, request the player info. (If they are BB, the
  // server already has it)
  GetPlayerInfoResult ch;
  if (a.c->version() == Version::BB_V4) {
    ch.character = a.c->character_file();
    ch.is_full_info = true;
  } else {
    ch = co_await send_get_player_info(a.c, true);
  }

  string filename = dest_bb_license
      ? Client::character_filename(dest_bb_license->username, dest_character_index)
      : Client::backup_character_filename(dest_account->account_id, dest_character_index, is_ep3(a.c->version()));

  if (ch.is_full_info) {
    // Client sent 30; ch contains the verbatim save file from the client
    if (ch.ep3_character) {
      try {
        Client::save_ep3_character_file(filename, *ch.ep3_character);
        send_text_message(a.c, "$C7Character data saved\n(full save file)");
      } catch (const exception& e) {
        send_text_message_fmt(a.c, "$C6Character data could\nnot be saved:\n{}", e.what());
      }
    }
    if (ch.character) {
      try {
        Client::save_character_file(filename, a.c->system_file(), ch.character);
        send_text_message(a.c, "$C7Character data saved\n(full save file)");
      } catch (const exception& e) {
        send_text_message_fmt(a.c, "$C6Character data could\nnot be saved:\n{}", e.what());
      }
    }

  } else {
    // Client sent 61; generate a BB-format player from the information we have
    // and save that instead
    if (ch.character) {
      auto bb_player = PSOBBCharacterFile::create_from_config(
          a.c->login->account->account_id,
          a.c->language(),
          ch.character->disp.visual,
          ch.character->disp.name.decode(a.c->language()),
          s->level_table(a.c->version()));
      bb_player->disp.visual.version = 4;
      bb_player->disp.visual.name_color_checksum = 0x00000000;
      bb_player->inventory = ch.character->inventory;
      // Before V3, player stats can't be correctly computed from other fields
      // because material usage isn't stored anywhere. For these versions, we
      // have to trust the stats field from the player's data.
      auto level_table = s->level_table(a.c->version());
      if (is_v1_or_v2(a.c->version())) {
        bb_player->disp.stats = ch.character->disp.stats;
        bb_player->import_tethealla_material_usage(level_table);
      } else {
        level_table->advance_to_level(
            bb_player->disp.stats, ch.character->disp.stats.level, bb_player->disp.visual.char_class);
        bb_player->disp.stats.char_stats.atp += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::POWER) * 2;
        bb_player->disp.stats.char_stats.mst += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::MIND) * 2;
        bb_player->disp.stats.char_stats.evp += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE) * 2;
        bb_player->disp.stats.char_stats.dfp += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::DEF) * 2;
        bb_player->disp.stats.char_stats.lck += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK) * 2;
        bb_player->disp.stats.char_stats.hp += bb_player->get_material_usage(PSOBBCharacterFile::MaterialType::HP) * 2;
        bb_player->disp.stats.experience = ch.character->disp.stats.experience;
        bb_player->disp.stats.meseta = ch.character->disp.stats.meseta;
      }
      bb_player->disp.technique_levels_v1 = ch.character->disp.technique_levels_v1;
      bb_player->auto_reply = ch.character->auto_reply;
      bb_player->info_board = ch.character->info_board;
      bb_player->battle_records = ch.character->battle_records;
      bb_player->challenge_records = ch.character->challenge_records;
      bb_player->choice_search_config = ch.character->choice_search_config;

      try {
        Client::save_character_file(filename, a.c->system_file(), bb_player);
        send_text_message(a.c, "$C7Character data saved\n(basic only)");
      } catch (const exception& e) {
        send_text_message_fmt(a.c, "$C6Character data could\nnot be saved:\n{}", e.what());
      }
    }
  }

  co_return;
}

ChatCommandDefinition cc_bbchar(
    {"$bbchar"},
    +[](const Args& a) -> asio::awaitable<void> {
      return server_command_bbchar_savechar(a, true);
    });

ChatCommandDefinition cc_cheat(
    {"$cheat"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_leader();

      auto l = a.c->require_lobby();
      if (a.check_permissions && l->check_flag(Lobby::Flag::CANNOT_CHANGE_CHEAT_MODE)) {
        throw precondition_failed("$C6Cheat mode cannot\nbe changed on this\nserver");
      } else {
        l->toggle_flag(Lobby::Flag::CHEATS_ENABLED);
        send_text_message_fmt(l, "Cheat mode {}", l->check_flag(Lobby::Flag::CHEATS_ENABLED) ? "enabled" : "disabled");

        auto s = a.c->require_server_state();
        if (!l->check_flag(Lobby::Flag::CHEATS_ENABLED) &&
            !a.c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE) &&
            s->cheat_flags.insufficient_minimum_level) {
          size_t default_min_level = s->default_min_level_for_game(a.c->version(), l->episode, l->difficulty);
          if (l->min_level < default_min_level) {
            l->min_level = default_min_level;
            send_text_message_fmt(l, "$C6Minimum level set\nto {}", l->min_level + 1);
          }
        }
      }
      co_return;
    });

ChatCommandDefinition cc_checkchar(
    {"$checkchar"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }

      auto s = a.c->require_server_state();

      if (a.text.empty()) {
        bool is_ep3 = ::is_ep3(a.c->version());

        vector<bool> flags;
        flags.emplace_back(false);
        for (size_t z = 0; z < s->num_backup_character_slots; z++) {
          string filename = a.c->backup_character_filename(a.c->login->account->account_id, z, is_ep3);
          flags.emplace_back(std::filesystem::is_regular_file(filename));
        }
        string used_str = str_for_flag_ranges(flags);
        flags.flip();
        flags[0] = false;
        string free_str = str_for_flag_ranges(flags);
        send_text_message_fmt(a.c, "Used: {}\nFree: {}", used_str, free_str);

      } else {
        size_t index = stoull(a.text, nullptr, 0) - 1;
        if (index >= s->num_backup_character_slots) {
          throw precondition_failed("$C6Player index must\nbe in range 1-{}", s->num_backup_character_slots);
        }

        try {
          if (is_ep3(a.c->version())) {
            string filename = a.c->backup_character_filename(a.c->login->account->account_id, index, true);
            auto ch = phosg::load_object_file<PSOGCEp3CharacterFile::Character>(filename);
            send_text_message_fmt(a.c, "Slot {}: $C6{}$C7\n{} {}\nCLv: on {}.{}, off {}.{}",
                index + 1, ch.disp.visual.name.decode(),
                name_for_section_id(ch.disp.visual.section_id), name_for_char_class(ch.disp.visual.char_class),
                (ch.ep3_config.online_clv_exp / 100) + 1, ch.ep3_config.online_clv_exp % 100,
                (ch.ep3_config.offline_clv_exp / 100) + 1, ch.ep3_config.offline_clv_exp % 100);
          } else {
            string filename = a.c->backup_character_filename(a.c->login->account->account_id, index, false);
            auto ch = PSOCHARFile::load_shared(filename, false).character_file;
            send_text_message_fmt(a.c, "Slot {}: $C6{}$C7\n{} {}\nLevel {}",
                index + 1, ch->disp.name.decode(),
                name_for_section_id(ch->disp.visual.section_id), name_for_char_class(ch->disp.visual.char_class),
                ch->disp.stats.level + 1);
          }
        } catch (const phosg::cannot_open_file&) {
          send_text_message_fmt(a.c, "No character in\nslot {}", index + 1);
        }
      }

      co_return;
    });

ChatCommandDefinition cc_debug(
    {"$debug"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_account_flag(Account::Flag::DEBUG);
      a.c->toggle_flag(Client::Flag::DEBUG_ENABLED);
      send_text_message_fmt(a.c, "Debug {}",
          (a.c->check_flag(Client::Flag::DEBUG_ENABLED) ? "enabled" : "disabled"));
      co_return;
    });

ChatCommandDefinition cc_deletechar(
    {"$deletechar"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }

      size_t index = stoull(a.text, nullptr, 0) - 1;
      if (index > 15) {
        throw precondition_failed("$C6Player index must\nbe in range 1-16");
      }

      string filename = a.c->backup_character_filename(a.c->login->account->account_id, index, is_ep3(a.c->version()));
      if (std::filesystem::is_regular_file(filename)) {
        std::filesystem::remove(filename);
        send_text_message_fmt(a.c, "Character in slot\n{} deleted", index + 1);
      } else {
        send_text_message_fmt(a.c, "No character exists\nin slot {}", index + 1);
      }

      co_return;
    });

ChatCommandDefinition cc_dicerange(
    {"$dicerange"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_is_leader();

      auto l = a.c->require_lobby();
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
        send_text_message_fmt(l, "$C7Dice ranges reset\nto defaults");
      } else {
        send_text_message_fmt(l, "$C7Dice ranges changed:");
        if (def_dice_range) {
          send_text_message_fmt(l, "$C7DEF: $C6{}-{}",
              static_cast<uint8_t>(def_dice_range >> 4), static_cast<uint8_t>(def_dice_range & 0x0F));
        }
        if (atk_dice_range_2v1) {
          send_text_message_fmt(l, "$C7ATK (1p in 2v1): $C6{}-{}",
              static_cast<uint8_t>(atk_dice_range_2v1 >> 4), static_cast<uint8_t>(atk_dice_range_2v1 & 0x0F));
        }
        if (def_dice_range_2v1) {
          send_text_message_fmt(l, "$C7DEF (1p in 2v1): $C6{}-{}",
              static_cast<uint8_t>(def_dice_range_2v1 >> 4), static_cast<uint8_t>(def_dice_range_2v1 & 0x0F));
        }
      }
      co_return;
    });

ChatCommandDefinition cc_dropmode(
    {"$dropmode"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_game(true);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_or_allowed(s->cheat_flags.proxy_override_drops);

      if (a.c->proxy_session) {

        if (a.text.empty()) {
          switch (a.c->proxy_session->drop_mode) {
            case ProxyDropMode::DISABLED:
              send_text_message(a.c, "Drop mode: disabled");
              break;
            case ProxyDropMode::PASSTHROUGH:
              send_text_message(a.c, "Drop mode: default");
              break;
            case ProxyDropMode::INTERCEPT:
              send_text_message(a.c, "Drop mode: proxy");
              break;
          }

        } else {
          ProxyDropMode new_mode;
          if ((a.text == "none") || (a.text == "disabled")) {
            new_mode = ProxyDropMode::DISABLED;
          } else if ((a.text == "default") || (a.text == "passthrough")) {
            new_mode = ProxyDropMode::PASSTHROUGH;
          } else if ((a.text == "proxy") || (a.text == "intercept")) {
            new_mode = ProxyDropMode::INTERCEPT;
          } else {
            throw precondition_failed("Invalid drop mode");
          }

          a.c->proxy_session->set_drop_mode(s, a.c->version(), a.c->override_random_seed, new_mode);
          switch (a.c->proxy_session->drop_mode) {
            case ProxyDropMode::DISABLED:
              send_text_message(a.c->channel, "Item drops disabled");
              break;
            case ProxyDropMode::PASSTHROUGH:
              send_text_message(a.c->channel, "Item drops changed\nto default mode");
              break;
            case ProxyDropMode::INTERCEPT:
              send_text_message(a.c->channel, "Item drops changed\nto proxy mode");
              break;
          }
        }

      } else { // Not proxy session
        auto l = a.c->require_lobby();
        if (a.text.empty()) {
          switch (l->drop_mode) {
            case ServerDropMode::DISABLED:
              send_text_message(a.c, "Drop mode: disabled");
              break;
            case ServerDropMode::CLIENT:
              send_text_message(a.c, "Drop mode: client");
              break;
            case ServerDropMode::SERVER_SHARED:
              send_text_message(a.c, "Drop mode: server\nshared");
              break;
            case ServerDropMode::SERVER_PRIVATE:
              send_text_message(a.c, "Drop mode: server\nprivate");
              break;
            case ServerDropMode::SERVER_DUPLICATE:
              send_text_message(a.c, "Drop mode: server\nduplicate");
              break;
          }

        } else {
          a.check_is_leader();
          ServerDropMode new_mode;
          if ((a.text == "none") || (a.text == "disabled")) {
            new_mode = ServerDropMode::DISABLED;
          } else if (a.text == "client") {
            new_mode = ServerDropMode::CLIENT;
          } else if ((a.text == "shared") || (a.text == "server")) {
            new_mode = ServerDropMode::SERVER_SHARED;
          } else if ((a.text == "private") || (a.text == "priv")) {
            new_mode = ServerDropMode::SERVER_PRIVATE;
          } else if ((a.text == "duplicate") || (a.text == "dup")) {
            new_mode = ServerDropMode::SERVER_DUPLICATE;
          } else {
            throw precondition_failed("Invalid drop mode");
          }

          if (!(l->allowed_drop_modes & (1 << static_cast<size_t>(new_mode)))) {
            throw precondition_failed("Drop mode not\nallowed");
          }

          l->drop_mode = new_mode;
          switch (l->drop_mode) {
            case ServerDropMode::DISABLED:
              send_text_message(l, "Item drops disabled");
              break;
            case ServerDropMode::CLIENT:
              send_text_message(l, "Item drops changed\nto client mode");
              break;
            case ServerDropMode::SERVER_SHARED:
              send_text_message(l, "Item drops changed\nto server shared\nmode");
              break;
            case ServerDropMode::SERVER_PRIVATE:
              send_text_message(l, "Item drops changed\nto server private\nmode");
              break;
            case ServerDropMode::SERVER_DUPLICATE:
              send_text_message(l, "Item drops changed\nto server duplicate\nmode");
              break;
          }
        }
      }
      co_return;
    });

ChatCommandDefinition cc_edit(
    {"$edit"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(false);

      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
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
        auto p = a.c->character_file();
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
          p->disp.visual.name_color = stoul(tokens.at(1), nullptr, 16);
        } else if (tokens.at(0) == "language" || tokens.at(0) == "lang") {
          if (tokens.at(1).size() != 1) {
            throw runtime_error("invalid language");
          }
          uint8_t new_language = language_code_for_char(tokens.at(1).at(0));
          a.c->channel->language = new_language;
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
            p->disp.visual.restore_npc_saved_fields();
          } else {
            uint8_t npc = npc_for_name(tokens.at(1), a.c->version());
            if (npc == 0xFF) {
              throw precondition_failed("$C6No such NPC");
            }
            p->disp.visual.backup_npc_saved_fields();
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
      co_return;
    });

ChatCommandDefinition cc_event(
    {"$event"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.c->proxy_session) {
        if (a.text.empty()) {
          a.c->override_lobby_event = 0xFF;
        } else {
          uint8_t new_event = event_for_name(a.text);
          if (new_event == 0xFF) {
            throw precondition_failed("$C6No such lobby event");
          } else {
            a.c->override_lobby_event = new_event;
            if (!is_v1_or_v2(a.c->version())) {
              a.c->channel->send(0xDA, a.c->override_lobby_event);
            }
          }
        }

      } else { // Not proxy session
        a.check_is_game(false);
        a.check_account_flag(Account::Flag::CHANGE_EVENT);

        auto l = a.c->require_lobby();

        uint8_t new_event = event_for_name(a.text);
        if (new_event == 0xFF) {
          throw precondition_failed("$C6No such lobby event");
        }

        l->event = new_event;
        send_change_event(l, l->event);
      }
      co_return;
    });

ChatCommandDefinition cc_exit(
    {"$exit"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (!(a.c->proxy_session
                  ? a.c->proxy_session->is_in_game
                  : a.c->require_lobby()->is_game())) {
        // Client is in the lobby; send them to the login server (main menu)
        if (a.c->proxy_session) {
          if (is_v4(a.c->version())) {
            throw precondition_failed("$C6You cannot exit proxy\nsessions on BB");
          }
          a.c->proxy_session->server_channel->disconnect();
        } else {
          send_self_leave_notification(a.c);
          if (!a.c->check_flag(Client::Flag::NO_D6)) {
            send_message_box(a.c, "");
          }
          co_await start_login_server_procedure(a.c);
        }
        co_return;
      }

      if (is_ep3(a.c->version())) {
        // Client is on Ep3; command ED triggers game exit
        a.c->channel->send(0xED, 0x00);
        co_return;
      }

      bool is_in_quest;
      if (a.c->proxy_session) {
        is_in_quest = a.c->proxy_session->is_in_quest;
      } else {
        auto l = a.c->require_lobby();
        is_in_quest = (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) ||
            l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
      }
      if (is_in_quest) {
        // Client is in a quest; command 6x73 triggers game exit
        G_UnusedHeader cmd = {0x73, 0x01, 0x0000};
        a.c->channel->send(0x60, 0x00, cmd);
        a.c->floor = 0;
        co_return;
      }

      if (a.c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) &&
          a.c->check_flag(Client::Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE)) {
        co_await prepare_client_for_patches(a.c);
        auto s = a.c->require_server_state();
        shared_ptr<const CompiledFunctionCode> fn;
        try {
          fn = s->function_code_index->get_patch("ExitAnywhere", a.c->specific_version);
        } catch (const out_of_range&) {
        }
        if (fn) {
          co_await send_function_call(a.c, fn);
          co_return;
        }
      }
      throw precondition_failed("$C6You must return to\nthe lobby first");
    });

ChatCommandDefinition cc_gc(
    {"$gc"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.c->proxy_session) {
        bool any_card_sent = false;
        for (const auto& p : a.c->proxy_session->lobby_players) {
          if (!p.name.empty() && a.text == p.name) {
            send_guild_card(a.c->channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
            any_card_sent = true;
          }
        }

        if (!any_card_sent) {
          size_t index = stoull(a.text, nullptr, 0);
          const auto& p = a.c->proxy_session->lobby_players.at(index);
          if (!p.name.empty()) {
            send_guild_card(a.c->channel, p.guild_card_number, p.guild_card_number, p.name, "", "", p.language, p.section_id, p.char_class);
          }
        }
      } else {
        send_guild_card(a.c, a.c);
      }
      co_return;
    });

ChatCommandDefinition cc_infhp(
    {"$infhp"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (!a.c->proxy_session) {
        a.check_is_game(true);
      }

      if (a.c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        a.c->clear_flag(Client::Flag::INFINITE_HP_ENABLED);
        send_text_message(a.c, "$C6Infinite HP disabled");
      } else {
        auto s = a.c->require_server_state();
        a.check_cheats_enabled_or_allowed(s->cheat_flags.infinite_hp_tp);
        a.c->set_flag(Client::Flag::INFINITE_HP_ENABLED);
        co_await send_remove_negative_conditions(a.c);
        if (a.c->proxy_session) {
          send_remove_negative_conditions(a.c->proxy_session->server_channel, a.c->lobby_client_id);
        }
        send_text_message(a.c, "$C6Infinite HP enabled");
      }
      co_return;
    });

ChatCommandDefinition cc_inftime(
    {"$inftime"},
    +[](const Args& a) -> asio::awaitable<void> {
      // TODO: We could implement this in proxy sessions by rewriting the rules
      // struct from the server in various 6xB4 commands.
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      a.check_is_leader();

      auto l = a.c->require_lobby();
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
      co_return;
    });

ChatCommandDefinition cc_inftp(
    {"$inftp"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (!a.c->proxy_session) {
        a.check_is_game(true);
      }

      if (a.c->check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        a.c->clear_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.c, "$C6Infinite TP disabled");
      } else {
        auto s = a.c->require_server_state();
        a.check_cheats_enabled_or_allowed(s->cheat_flags.infinite_hp_tp);
        a.c->set_flag(Client::Flag::INFINITE_TP_ENABLED);
        send_text_message(a.c, "$C6Infinite TP enabled");
      }
      co_return;
    });

ChatCommandDefinition cc_item(
    {"$item", "$i"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_game(true);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_or_allowed(s->cheat_flags.create_items);

      ItemData item;
      if (a.c->proxy_session) {
        if (a.c->version() == Version::BB_V4) {
          throw precondition_failed("$C6This command cannot\nbe used in proxy\nsessions in BB games");
        }
        a.check_is_leader();

        item = s->parse_item_description(a.c->version(), a.text);
        item.id = phosg::random_object<uint32_t>() | 0x80000000;

        send_drop_stacked_item_to_channel(s, a.c->channel, item, a.c->floor, a.c->pos);
        send_drop_stacked_item_to_channel(s, a.c->proxy_session->server_channel, item, a.c->floor, a.c->pos);

      } else {
        auto l = a.c->require_lobby();
        item = s->parse_item_description(a.c->version(), a.text);
        item.id = l->generate_item_id(a.c->lobby_client_id);

        if ((l->drop_mode == ServerDropMode::SERVER_PRIVATE) || (l->drop_mode == ServerDropMode::SERVER_DUPLICATE)) {
          l->add_item(a.c->floor, item, a.c->pos, nullptr, nullptr, (1 << a.c->lobby_client_id));
          send_drop_stacked_item_to_channel(s, a.c->channel, item, a.c->floor, a.c->pos);
        } else {
          l->add_item(a.c->floor, item, a.c->pos, nullptr, nullptr, 0x00F);
          send_drop_stacked_item_to_lobby(l, item, a.c->floor, a.c->pos);
        }
      }

      string name = s->describe_item(a.c->version(), item, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);
      send_text_message(a.c, "$C7Item created:\n" + name);
      co_return;
    });

ChatCommandDefinition cc_itemnotifs(
    {"$itemnotifs"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.text == "every" || a.text == "everything") {
        a.c->set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA);
        send_text_message_fmt(a.c, "$C6Notifications enabled\nfor all items and\nMeseta");
      } else if (a.text == "all" || a.text == "on") {
        a.c->set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS);
        send_text_message_fmt(a.c, "$C6Notifications enabled\nfor all items");
      } else if (a.text == "rare" || a.text == "rares") {
        a.c->set_drop_notification_mode(Client::ItemDropNotificationMode::RARES_ONLY);
        send_text_message_fmt(a.c, "$C6Notifications enabled\nfor rare items only");
      } else if (a.text == "none" || a.text == "off") {
        a.c->set_drop_notification_mode(Client::ItemDropNotificationMode::NOTHING);
        send_text_message_fmt(a.c, "$C6Notifications disabled\nfor all items");
      } else {
        throw precondition_failed("$C6You must specify\n$C6off$C7, $C6rare$C7, $C6on$C7, or\n$C6everything$C7");
      }
      co_return;
    });

ChatCommandDefinition cc_kick(
    {"$kick"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_account_flag(Account::Flag::KICK_USER);

      auto s = a.c->require_server_state();
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
      target->channel->disconnect();
      send_text_message_fmt(a.c, "$C6{} kicked off", target_name);
      co_return;
    });

ChatCommandDefinition cc_killcount(
    {"$killcount"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);

      auto p = a.c->character_file();
      vector<size_t> item_indexes;
      for (size_t z = 0; z < p->inventory.num_items; z++) {
        const auto& item = p->inventory.items[z];
        if (item.is_equipped() && item.data.has_kill_count()) {
          item_indexes.emplace_back(z);
        }
      }

      if (item_indexes.empty()) {
        throw precondition_failed("No equipped items\nhave kill counts");

      } else {
        // Kill counts are only accurate on the server side at all times on BB.
        // On other versions, we update the server's view of the client's
        // inventory during games, but we can't track kills because the client
        // doesn't inform the server whether it counted a kill for any
        // individual enemy. So, on non-BB versions, the kill count is accurate
        // at all times in the lobby (since kills can't occur there), or at the
        // beginning of a game.
        if ((a.c->version() == Version::BB_V4) || !a.c->require_lobby()->is_game()) {
          send_text_message(a.c, "As of now:");
        } else {
          send_text_message(a.c, "As of game join:");
        }

        auto s = a.c->require_server_state();
        for (size_t z : item_indexes) {
          const auto& item = p->inventory.items[z];
          string name = s->describe_item(
              a.c->version(), item.data, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES | ItemNameIndex::Flag::NAME_ONLY);
          send_text_message_fmt(a.c, "{}$C7: {} kills", name, item.data.get_kill_count());
        }
      }
      co_return;
    });

ChatCommandDefinition cc_lobby_info(
    {"$li"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.c->proxy_session) {
        string msg;
        // On non-masked-GC sessions (BB), there is no remote Guild Card number, so we
        // don't show it. (The user can see it in the pause menu, unlike in masked-GC
        // sessions like GC.)
        if (a.c->proxy_session->remote_guild_card_number >= 0) {
          msg = std::format("$C7GC: $C6{}$C7\n", a.c->proxy_session->remote_guild_card_number);
        }
        msg += "Slots: ";

        for (size_t z = 0; z < a.c->proxy_session->lobby_players.size(); z++) {
          bool is_self = (z == a.c->lobby_client_id);
          bool is_leader = (z == a.c->proxy_session->leader_client_id);
          if (a.c->proxy_session->lobby_players[z].guild_card_number == 0) {
            msg += std::format("$C0{:X}$C7", z);
          } else if (is_self && is_leader) {
            msg += std::format("$C6{:X}$C7", z);
          } else if (is_self) {
            msg += std::format("$C2{:X}$C7", z);
          } else if (is_leader) {
            msg += std::format("$C4{:X}$C7", z);
          } else {
            msg += std::format("{:X}", z);
          }
        }

        vector<const char*> cheats_tokens;
        if (a.c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
          cheats_tokens.emplace_back("HP");
        }
        if (a.c->check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
          cheats_tokens.emplace_back("TP");
        }
        if (!cheats_tokens.empty()) {
          msg += "\n$C7Cheats: $C6";
          msg += phosg::join(cheats_tokens, ",");
        }

        vector<const char*> behaviors_tokens;
        if (a.c->check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
          behaviors_tokens.emplace_back("SWA");
        }
        if (a.c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
          behaviors_tokens.emplace_back("SF");
        }
        if (a.c->check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS)) {
          behaviors_tokens.emplace_back("BF");
        }
        if (!behaviors_tokens.empty()) {
          msg += "\n$C7Flags: $C6";
          msg += phosg::join(behaviors_tokens, ",");
        }

        if (a.c->override_section_id != 0xFF) {
          msg += "\n$C7SecID*: $C6";
          msg += name_for_section_id(a.c->override_section_id);
        }

        send_text_message(a.c->channel, msg);

      } else { // Not proxy session
        vector<string> lines;

        auto l = a.c->lobby.lock();
        if (!l) {
          lines.emplace_back("$C4No lobby info");

        } else {
          if (l->is_game()) {
            if (!l->is_ep3()) {
              if (l->max_level == 0xFFFFFFFF) {
                lines.emplace_back(std::format("$C6{:08X}$C7 L$C6{}+$C7", l->lobby_id, l->min_level + 1));
              } else {
                lines.emplace_back(std::format(
                    "$C6{:08X}$C7 L$C6{}-{}$C7", l->lobby_id, l->min_level + 1, l->max_level + 1));
              }
              lines.emplace_back(std::format(
                  "$C7Section ID: $C6{}$C7", name_for_section_id(l->effective_section_id())));

              switch (l->drop_mode) {
                case ServerDropMode::DISABLED:
                  lines.emplace_back("Drops disabled");
                  break;
                case ServerDropMode::CLIENT:
                  lines.emplace_back("Client item table");
                  break;
                case ServerDropMode::SERVER_SHARED:
                  lines.emplace_back("Server item table");
                  break;
                case ServerDropMode::SERVER_PRIVATE:
                  lines.emplace_back("Server indiv items");
                  break;
                case ServerDropMode::SERVER_DUPLICATE:
                  lines.emplace_back("Server dup items");
                  break;
                default:
                  lines.emplace_back("$C4Unknown drop mode$C7");
              }
              if (l->check_flag(Lobby::Flag::CHEATS_ENABLED)) {
                lines.emplace_back("Cheats enabled");
              }

            } else {
              char type_ch = dynamic_pointer_cast<DisabledRandomGenerator>(l->rand_crypt).get()
                  ? 'X'
                  : dynamic_pointer_cast<MT19937Generator>(l->rand_crypt).get()
                  ? 'M'
                  : dynamic_pointer_cast<PSOV2Encryption>(l->rand_crypt).get()
                  ? 'L'
                  : '?';
              lines.emplace_back(std::format("$C7State seed: $C6{:08X}/{:c}$C7", l->random_seed, type_ch));
            }

          } else {
            lines.emplace_back(std::format("$C7Lobby ID: $C6{:08X}$C7", l->lobby_id));
          }

          string slots_str = "Slots: ";
          for (size_t z = 0; z < l->clients.size(); z++) {
            if (!l->clients[z]) {
              slots_str += std::format("$C0{:X}$C7", z);
            } else {
              bool is_self = (l->clients[z] == a.c);
              bool is_leader = (z == l->leader_id);
              if (is_self && is_leader) {
                slots_str += std::format("$C6{:X}$C7", z);
              } else if (is_self) {
                slots_str += std::format("$C2{:X}$C7", z);
              } else if (is_leader) {
                slots_str += std::format("$C4{:X}$C7", z);
              } else {
                slots_str += std::format("{:X}", z);
              }
            }
          }
          lines.emplace_back(std::move(slots_str));
        }

        send_text_message(a.c, phosg::join(lines, "\n"));
      }
      co_return;
    });

ChatCommandDefinition cc_ln(
    {"$ln"},
    +[](const Args& a) -> asio::awaitable<void> {
      uint8_t new_type;
      if (a.text.empty()) {
        new_type = 0x80;
      } else {
        new_type = lobby_type_for_name(a.text);
        if (new_type == 0x80) {
          throw precondition_failed("$C6No such lobby type");
        }
      }

      if (a.c->proxy_session) {
        a.c->override_lobby_number = new_type;
      } else {
        a.check_is_game(false);
        a.c->override_lobby_number = new_type;
        send_join_lobby(a.c, a.c->require_lobby());
      }
      co_return;
    });

ChatCommandDefinition cc_loadchar(
    {"$loadchar"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(false);
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }

      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();

      size_t index = stoull(a.text, nullptr, 0) - 1;
      if (index >= s->num_backup_character_slots) {
        throw precondition_failed("$C6Player index must\nbe in range 1-{}", s->num_backup_character_slots);
      }

      shared_ptr<PSOGCEp3CharacterFile::Character> ep3_char;
      if (is_ep3(a.c->version())) {
        ep3_char = a.c->load_ep3_backup_character(a.c->login->account->account_id, index);
      } else {
        a.c->load_backup_character(a.c->login->account->account_id, index);
      }

      if (a.c->version() == Version::BB_V4) {
        // On BB, it suffices to simply send the character file again
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
        if (!a.c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) ||
            !a.c->check_flag(Client::Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE)) {
          throw precondition_failed("Can\'t load character\ndata on this game\nversion");
        }

        auto send_set_extended_player_info = [&a, &s]<typename CharT>(const CharT& char_file) -> asio::awaitable<void> {
          co_await prepare_client_for_patches(a.c);
          try {
            auto fn = s->function_code_index->get_patch("SetExtendedPlayerInfo", a.c->specific_version);
            co_await send_function_call(a.c, fn, {}, &char_file, sizeof(CharT));
            auto l = a.c->lobby.lock();
            if (l) {
              send_player_leave_notification(l, a.c->lobby_client_id);
              s->send_lobby_join_notifications(l, a.c);
            }
          } catch (const exception& e) {
            a.c->log.warning_f("Failed to set extended player info: {}", e.what());
            throw precondition_failed("Failed to set\nplayer info:\n{}", e.what());
          }
        };

        if (a.c->version() == Version::DC_V2) {
          PSODCV2CharacterFile::Character dc_char = *a.c->character_file();
          co_await send_set_extended_player_info(dc_char);
        } else if (a.c->version() == Version::GC_NTE) {
          PSOGCNTECharacterFileCharacter gc_char = *a.c->character_file();
          co_await send_set_extended_player_info(gc_char);
        } else if (a.c->version() == Version::GC_V3) {
          PSOGCCharacterFile::Character gc_char = *a.c->character_file();
          co_await send_set_extended_player_info(gc_char);
        } else if (a.c->version() == Version::GC_EP3_NTE) {
          PSOGCEp3NTECharacter nte_char = *ep3_char;
          co_await send_set_extended_player_info(nte_char);
        } else if (a.c->version() == Version::GC_EP3) {
          co_await send_set_extended_player_info(*ep3_char);
        } else if (a.c->version() == Version::XB_V3) {
          if (!a.c->login || !a.c->login->xb_license) {
            throw runtime_error("XB client is not logged in");
          }
          PSOXBCharacterFile::Character xb_char = *a.c->character_file();
          xb_char.guild_card.xb_user_id_high = (a.c->login->xb_license->user_id >> 32) & 0xFFFFFFFF;
          xb_char.guild_card.xb_user_id_low = a.c->login->xb_license->user_id & 0xFFFFFFFF;
          co_await send_set_extended_player_info(xb_char);
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
      co_return;
    });

ChatCommandDefinition cc_matcount(
    {"$matcount"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);

      auto p = a.c->character_file();
      if (is_v1_or_v2(a.c->version())) {
        send_text_message_fmt(a.c, "{} HP, {} TP",
            p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
      } else {
        send_text_message_fmt(a.c, "{} HP, {} TP, {} POW\n{} MIND, {} EVADE\n{} DEF, {} LUCK",
            p->get_material_usage(PSOBBCharacterFile::MaterialType::HP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::TP),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF),
            p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
      }
      co_return;
    });

ChatCommandDefinition cc_maxlevel(
    {"$maxlevel"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_leader();

      auto l = a.c->require_lobby();
      l->max_level = stoull(a.text) - 1;
      if (l->max_level >= 200) {
        l->max_level = 0xFFFFFFFF;
      }

      if (l->max_level == 0xFFFFFFFF) {
        send_text_message(l, "$C6Maximum level set\nto unlimited");
      } else {
        send_text_message_fmt(l, "$C6Maximum level set\nto {}", l->max_level + 1);
      }
      co_return;
    });

ChatCommandDefinition cc_minlevel(
    {"$minlevel"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_leader();

      size_t new_min_level = stoull(a.text) - 1;

      auto l = a.c->require_lobby();
      auto s = a.c->require_server_state();
      bool cheats_allowed = (l->check_flag(Lobby::Flag::CHEATS_ENABLED) ||
          a.c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));
      if (!cheats_allowed && s->cheat_flags.insufficient_minimum_level) {
        size_t default_min_level = s->default_min_level_for_game(a.c->version(), l->episode, l->difficulty);
        if (new_min_level < default_min_level) {
          throw precondition_failed("$C6Cannot set minimum\nlevel below {}", default_min_level + 1);
        }
      }

      l->min_level = new_min_level;
      send_text_message_fmt(l, "$C6Minimum level set\nto {}", l->min_level + 1);
      co_return;
    });

ChatCommandDefinition cc_next(
    {"$next"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_game(true);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_or_allowed(s->cheat_flags.warp);

      auto episode = a.c->proxy_session
          ? a.c->proxy_session->lobby_episode
          : a.c->require_lobby()->episode;
      size_t limit = FloorDefinition::limit_for_episode(episode);
      if (limit > 0) {
        send_warp(a.c, (a.c->floor + 1) % limit, true);
      }
      co_return;
    });

ChatCommandDefinition cc_password(
    {"$password"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_leader();

      auto l = a.c->require_lobby();
      if (a.text.empty()) {
        l->password.clear();
        send_text_message(l, "$C6Game unlocked");

      } else {
        l->password = a.text;
        string escaped = remove_color(l->password);
        send_text_message_fmt(l, "$C6Game password:\n{}", escaped);
      }
      co_return;
    });

ChatCommandDefinition cc_patch(
    {"$patch"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto tokens = phosg::split(a.text, ' ');
      if (tokens.empty()) {
        throw runtime_error("not enough arguments");
      }

      string patch_name = std::move(tokens[0]);
      unordered_map<string, uint32_t> label_writes;
      for (size_t z = 0; z < tokens.size() - 1; z++) {
        label_writes.emplace(std::format("arg{}", z), stoul(tokens[z + 1], nullptr, 0));
      }

      co_await prepare_client_for_patches(a.c);
      try {
        auto s = a.c->require_server_state();
        // Note: We can't look this up before prepare_client_for_patches
        // because specific_version may not be set at that point
        auto fn = s->function_code_index->get_patch(patch_name, a.c->specific_version);
        co_await send_function_call(a.c, fn, label_writes);
      } catch (const out_of_range&) {
        send_text_message(a.c, "$C6Invalid patch name");
      }
      co_return;
    });

ChatCommandDefinition cc_persist(
    {"$persist"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);

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
        send_text_message_fmt(l, "Lobby persistence\n{}", l->check_flag(Lobby::Flag::PERSISTENT) ? "enabled" : "disabled");
      }
      co_return;
    });

ChatCommandDefinition cc_ping(
    {"$ping"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.c->ping_start_time = phosg::now();
      send_command(a.c, 0x1D, 0x00);
      if (a.c->proxy_session) {
        a.c->proxy_session->server_ping_start_time = a.c->ping_start_time;
        C_GuildCardSearch_40 cmd = {
            0x00010000,
            a.c->proxy_session->remote_guild_card_number,
            a.c->proxy_session->remote_guild_card_number};
        a.c->proxy_session->server_channel->send(0x40, 0x00, &cmd, sizeof(cmd));
      }
      co_return;
    });

static string file_path_for_recording(const std::string& args, uint32_t account_id) {
  for (char ch : args) {
    if (ch <= 0x20 || ch > 0x7E || ch == '/') {
      throw runtime_error("invalid recording name");
    }
  }
  return std::format("system/ep3/battle-records/{:010}_{}.mzrd", account_id, args);
}

ChatCommandDefinition cc_playrec(
    {"$playrec"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_ep3(true);

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
        auto battle_player = make_shared<Episode3::BattleRecordPlayer>(s->io_context, record);
        auto game = create_game_generic(
            s, a.c, a.text, "", Episode::EP3, GameMode::NORMAL, 0, false, nullptr, battle_player);
        if (game) {
          if (start_battle_player_immediately) {
            game->set_flag(Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY);
          }
          s->change_client_lobby(a.c, game);
          a.c->set_flag(Client::Flag::LOADING);
          a.c->log.info_f("LOADING flag set");
        }
      } else {
        throw precondition_failed("$C4This command cannot\nbe used in a game");
      }
      co_return;
    });

ChatCommandDefinition cc_qcall(
    {"$qcall"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_debug_enabled();
      a.check_is_game(true);

      auto l = a.c->require_lobby();
      if (l->is_game() && l->quest) {
        send_quest_function_call(a.c, stoul(a.text, nullptr, 0));
      }
      co_return;
    });

ChatCommandDefinition cc_qcheck(
    {"$qcheck"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);

      auto l = a.c->require_lobby();
      uint16_t flag_num = stoul(a.text, nullptr, 0);

      if (l->is_game()) {
        if (!l->quest_flags_known || l->quest_flags_known->get(l->difficulty, flag_num)) {
          send_text_message_fmt(a.c, "$C7Game: flag 0x{:X} ({})\nis {} on {}",
              flag_num, flag_num,
              a.c->character_file()->quest_flags.get(l->difficulty, flag_num) ? "set" : "not set",
              name_for_difficulty(l->difficulty));
        } else {
          send_text_message_fmt(a.c, "$C7Game: flag 0x{:X} ({})\nis unknown on {}",
              flag_num, flag_num, name_for_difficulty(l->difficulty));
        }
      } else if (a.c->version() == Version::BB_V4) {
        send_text_message_fmt(a.c, "$C7Player: flag 0x{:X} ({})\nis {} on {}",
            flag_num, flag_num,
            a.c->character_file()->quest_flags.get(l->difficulty, flag_num) ? "set" : "not set",
            name_for_difficulty(l->difficulty));
      }
      co_return;
    });

static void command_qset_qclear(const Args& a, bool should_set) {
  a.check_is_game(true);
  uint16_t flag_num = stoul(a.text, nullptr, 0);

  if (!a.c->proxy_session) {
    a.check_debug_enabled();

    auto l = a.c->require_lobby();
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

    auto p = a.c->character_file(false);
    if (p) {
      if (should_set) {
        p->quest_flags.set(l->difficulty, flag_num);
      } else {
        p->quest_flags.clear(l->difficulty, flag_num);
      }
    }
  }

  if (is_v1_or_v2(a.c->version())) {
    G_UpdateQuestFlag_DC_PC_6x75 cmd = {{0x75, 0x02, 0x0000}, flag_num, should_set ? 0 : 1};
    a.c->channel->send(0x60, 0x00, &cmd, sizeof(cmd));
    if (a.c->proxy_session) {
      a.c->proxy_session->server_channel->send(0x60, 0x00, &cmd, sizeof(cmd));
    }
  } else {
    uint8_t difficulty = a.c->proxy_session ? a.c->proxy_session->lobby_difficulty : a.c->require_lobby()->difficulty;
    G_UpdateQuestFlag_V3_BB_6x75 cmd = {{{0x75, 0x03, 0x0000}, flag_num, should_set ? 0 : 1}, difficulty, 0x0000};
    a.c->channel->send(0x60, 0x00, &cmd, sizeof(cmd));
    if (a.c->proxy_session) {
      a.c->proxy_session->server_channel->send(0x60, 0x00, &cmd, sizeof(cmd));
    }
  }
  return;
}

ChatCommandDefinition cc_qclear(
    {"$qclear"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_qset_qclear(a, false);
      co_return;
    });

ChatCommandDefinition cc_qfread(
    {"$qfread"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
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

      uint32_t counter_value = a.c->character_file()->quest_counters.at(counter_index) & mask;

      while (!(mask & 1)) {
        mask >>= 1;
        counter_value >>= 1;
      }

      if (mask == 1) {
        send_text_message_fmt(a.c, "$C7Field {}\nhas value {}", a.text, counter_value ? "TRUE" : "FALSE");
      } else {
        send_text_message_fmt(a.c, "$C7Field {}\nhas value {}", a.text, counter_value);
      }
      co_return;
    });

ChatCommandDefinition cc_qgread(
    {"$qgread"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      uint8_t counter_num = stoul(a.text, nullptr, 0);
      const auto& counters = a.c->character_file()->quest_counters;
      if (counter_num >= counters.size()) {
        throw precondition_failed("$C7Counter ID must be\nless than {}", counters.size());
      } else {
        send_text_message_fmt(a.c, "$C7Quest counter {}\nhas value {}", counter_num, counters[counter_num]);
      }
      co_return;
    });

ChatCommandDefinition cc_qgwrite(
    {"$qgwrite"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
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
      auto& counters = a.c->character_file()->quest_counters;
      if (counter_num >= counters.size()) {
        throw precondition_failed("$C7Counter ID must be\nless than {}", counters.size());
      } else {
        a.c->character_file()->quest_counters[counter_num] = value;
        G_SetQuestCounter_BB_6xD2 cmd = {{0xD2, sizeof(G_SetQuestCounter_BB_6xD2) / 4, a.c->lobby_client_id}, counter_num, value};
        send_command_t(a.c, 0x60, 0x00, cmd);
        send_text_message_fmt(a.c, "$C7Quest counter {}\nset to {}", counter_num, value);
      }
      co_return;
    });

ChatCommandDefinition cc_qset(
    {"$qset"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_qset_qclear(a, true);
      co_return;
    });

static void command_qsync_qsyncall(const Args& a, bool send_to_lobby) {
  a.check_is_game(true);
  a.check_debug_enabled();

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
  send_command_t(a.c, 0x60, 0x00, cmd);
  if (send_to_lobby) {
    if (a.c->proxy_session) {
      send_command_t(a.c->proxy_session->server_channel, 0x60, 0x00, cmd);
    } else {
      send_command_t(a.c->require_lobby(), 0x60, 0x00, cmd);
    }
  }
}

ChatCommandDefinition cc_qsync(
    {"$qsync"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_qsync_qsyncall(a, false);
      co_return;
    });

ChatCommandDefinition cc_qsyncall(
    {"$qsyncall"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_qsync_qsyncall(a, true);
      co_return;
    });

ChatCommandDefinition cc_quest(
    {"$quest"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);

      auto s = a.c->require_server_state();
      Version effective_version = is_ep3(a.c->version()) ? Version::GC_V3 : a.c->version();
      auto q = s->quest_index(effective_version)->get(stoul(a.text));
      if (!q) {
        throw precondition_failed("$C6Quest not found");
      }

      auto l = a.c->require_lobby();
      if (a.check_permissions && !a.c->check_flag(Client::Flag::DEBUG_ENABLED)) {
        if (l->count_clients() > 1) {
          throw precondition_failed("$C6This command can only\nbe used with no\nother players present");
        }
        if (!q->allow_start_from_chat_command) {
          throw precondition_failed("$C6This quest cannot\nbe started with the\n%squest command");
        }
      }

      set_lobby_quest(a.c->require_lobby(), q, true);
      co_return;
    });

ChatCommandDefinition cc_rand(
    {"$rand"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(false);
      a.check_cheats_enabled_or_allowed(s->cheat_flags.override_random_seed);

      if (a.text.empty()) {
        a.c->override_random_seed = -1;
        send_text_message(a.c, "$C6Override seed\nremoved");
      } else {
        a.c->override_random_seed = stoll(a.text, 0, 16);
        send_text_message(a.c, "$C6Override seed\nset");
      }
      co_return;
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
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_debug_enabled();

      uint32_t addr = stoul(a.text, nullptr, 16);
      if (!console_address_in_range(a.c->version(), addr)) {
        throw precondition_failed("$C4Address out of\nrange");
      }

      co_await prepare_client_for_patches(a.c);

      shared_ptr<const CompiledFunctionCode> fn;
      try {
        auto s = a.c->require_server_state();
        const char* function_name = is_dc(a.c->version())
            ? "ReadMemoryWordDC"
            : is_gc(a.c->version())
            ? "ReadMemoryWordGC"
            : "ReadMemoryWordX86";
        fn = s->function_code_index->name_to_function.at(function_name);
      } catch (const out_of_range&) {
        throw precondition_failed("Invalid patch name");
      }

      unordered_map<string, uint32_t> label_writes{{"address", addr}};
      auto res = co_await send_function_call(a.c, fn, label_writes);
      string data_str;
      if (is_big_endian(a.c->version())) {
        be_uint32_t v = res.return_value.load();
        data_str = phosg::format_data_string(&v, sizeof(v));
      } else {
        data_str = phosg::format_data_string(&res.return_value, sizeof(res.return_value));
      }
      send_text_message_fmt(a.c, "Bytes at {:08X}:\n$C6{}", addr, data_str);
    });

ChatCommandDefinition cc_save(
    {"$save"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_version(Version::BB_V4);
      a.check_is_proxy(false);
      a.c->save_all();
      send_text_message(a.c, "All data saved");
      a.c->reschedule_save_game_data_timer();
      co_return;
    });

ChatCommandDefinition cc_savechar(
    {"$savechar"},
    +[](const Args& a) -> asio::awaitable<void> {
      if (a.check_permissions && a.c->login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT)) {
        throw precondition_failed("$C7This command cannot\nbe used on a shared\naccount");
      }
      co_await server_command_bbchar_savechar(a, false);
      co_return;
    });

ChatCommandDefinition cc_saverec(
    {"$saverec"},
    +[](const Args& a) -> asio::awaitable<void> {
      // TODO: We can probably support this on the proxy server, but it would
      // only include CA commands from the local player
      a.check_is_proxy(false);
      if (!a.c->ep3_prev_battle_record) {
        throw precondition_failed("$C4No finished\nrecording is\npresent");
      }
      string file_path = file_path_for_recording(a.text, a.c->login->account->account_id);
      string data = a.c->ep3_prev_battle_record->serialize();
      phosg::save_file(file_path, data);
      send_text_message(a.c, "$C7Recording saved");
      a.c->ep3_prev_battle_record.reset();
      co_return;
    });

static asio::awaitable<void> command_send_command(const Args& a, bool to_client, bool to_server) {
  if (!a.c->proxy_session) {
    a.check_debug_enabled();
  }
  string data = phosg::parse_data_string(a.text);
  data.resize((data.size() + 3) & (~3));
  if (to_client) {
    a.c->channel->send(data);
  }
  if (to_server) {
    if (a.c->proxy_session) {
      a.c->proxy_session->server_channel->send(data);
    } else {
      co_await on_command_with_header(a.c, data);
    }
  }
  co_return;
}

ChatCommandDefinition cc_sb(
    {"$sb"},
    +[](const Args& a) -> asio::awaitable<void> {
      return command_send_command(a, true, true);
    });

ChatCommandDefinition cc_sc(
    {"$sc"},
    +[](const Args& a) -> asio::awaitable<void> {
      return command_send_command(a, true, false);
    });

ChatCommandDefinition cc_secid(
    {"$secid"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_or_allowed(s->cheat_flags.override_section_id);

      uint8_t new_override_section_id;
      if (a.text.empty()) {
        new_override_section_id = 0xFF;
        send_text_message(a.c, "$C6Override section ID\nremoved");
      } else {
        new_override_section_id = section_id_for_name(a.text);
        if (new_override_section_id == 0xFF) {
          throw precondition_failed("$C6Invalid section ID");
        } else {
          send_text_message_fmt(a.c, "$C6Override section ID\nset to {}", name_for_section_id(new_override_section_id));
        }
      }

      a.c->override_section_id = new_override_section_id;
      if (!a.c->proxy_session) {
        auto l = a.c->require_lobby();
        if (l->is_game() && (l->leader_id == a.c->lobby_client_id)) {
          l->override_section_id = new_override_section_id;
          l->create_item_creator();
        }
      }
      co_return;
    });

ChatCommandDefinition cc_setassist(
    {"$setassist"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_in_game(s->cheat_flags.ep3_replace_assist);

      auto l = a.c->require_lobby();
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
      co_return;
    });

ChatCommandDefinition cc_server_info(
    {"$si"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto s = a.c->require_server_state();
      string uptime_str = phosg::format_duration(phosg::now() - s->creation_time);
      send_text_message_fmt(a.c,
          "Uptime: $C6{}$C7\nLobbies: $C6{}$C7\nClients: $C6{}$C7(g) $C6{}$C7(p)",
          uptime_str,
          s->id_to_lobby.size(),
          s->game_server->all_clients().size() - ProxySession::num_proxy_sessions,
          ProxySession::num_proxy_sessions);
      co_return;
    });

ChatCommandDefinition cc_silence(
    {"$silence"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_account_flag(Account::Flag::SILENCE_USER);

      auto s = a.c->require_server_state();
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
      send_text_message_fmt(a.c, "$C6{} {}silenced", target_name, target->can_chat ? "un" : "");
      co_return;
    });

ChatCommandDefinition cc_song(
    {"$song"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_ep3(true);

      int32_t song = stol(a.text, nullptr, 0);
      if (song < 0 && a.c->proxy_session) {
        song = -song;
        send_ep3_change_music(a.c->proxy_session->server_channel, song);
      }
      send_ep3_change_music(a.c->channel, stoul(a.text, nullptr, 0));
      co_return;
    });

ChatCommandDefinition cc_sound(
    {"$sound"},
    +[](const Args& a) -> asio::awaitable<void> {
      bool echo_to_all = (!a.text.empty() && a.text[0] == '!');
      uint32_t sound_id = stoul(echo_to_all ? a.text.substr(1) : a.text, nullptr, 16);

      // TODO: Using floor is technically incorrect here; it should be area
      G_PlaySoundFromPlayer_6xB2 cmd = {{0xB2, 0x03, 0x0000}, static_cast<uint8_t>(a.c->floor), 0x00, a.c->lobby_client_id, sound_id};
      if (!echo_to_all) {
        send_command_t(a.c, 0x60, 0x00, cmd);
      } else if (a.c->proxy_session) {
        send_command_t(a.c, 0x60, 0x00, cmd);
        send_command_t(a.c->proxy_session->server_channel, 0x60, 0x00, cmd);
      } else {
        a.check_debug_enabled();
        send_command_t(a.c->require_lobby(), 0x60, 0x00, cmd);
      }
      co_return;
    });

ChatCommandDefinition cc_spec(
    {"$spec"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      auto l = a.c->require_lobby();
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
      co_return;
    });

ChatCommandDefinition cc_ss(
    {"$ss"},
    +[](const Args& a) -> asio::awaitable<void> {
      return command_send_command(a, false, true);
    });

ChatCommandDefinition cc_stat(
    {"$stat"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      auto l = a.c->require_lobby();
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
        send_text_message_fmt(a.c, "$C7Score: {:g}\nRank: {} ({})", score, rank, rank_name);
      } else if (a.text == "duration") {
        string s = phosg::format_duration(phosg::now() - l->ep3_server->battle_start_usecs);
        send_text_message_fmt(a.c, "$C7Duration: {}", s);
      } else if (a.text == "fcs-destroyed") {
        send_text_message_fmt(a.c, "$C7Team FCs destroyed:\n{}", l->ep3_server->team_num_ally_fcs_destroyed[team_id]);
      } else if (a.text == "cards-destroyed") {
        send_text_message_fmt(a.c, "$C7Team cards destroyed:\n{}", l->ep3_server->team_num_cards_destroyed[team_id]);
      } else if (a.text == "damage-given") {
        send_text_message_fmt(a.c, "$C7Damage given: {}", ps->stats.damage_given);
      } else if (a.text == "damage-taken") {
        send_text_message_fmt(a.c, "$C7Damage taken: {}", ps->stats.damage_taken);
      } else if (a.text == "opp-cards-destroyed") {
        send_text_message_fmt(a.c, "$C7Opp. cards destroyed:\n{}", ps->stats.num_opponent_cards_destroyed);
      } else if (a.text == "own-cards-destroyed") {
        send_text_message_fmt(a.c, "$C7Own cards destroyed:\n{}", ps->stats.num_owned_cards_destroyed);
      } else if (a.text == "move-distance") {
        send_text_message_fmt(a.c, "$C7Move distance: {}", ps->stats.total_move_distance);
      } else if (a.text == "cards-set") {
        send_text_message_fmt(a.c, "$C7Cards set: {}", ps->stats.num_cards_set);
      } else if (a.text == "fcs-set") {
        send_text_message_fmt(a.c, "$C7FC cards set: {}", ps->stats.num_item_or_creature_cards_set);
      } else if (a.text == "attack-actions-set") {
        send_text_message_fmt(a.c, "$C7Attack actions set:\n{}", ps->stats.num_attack_actions_set);
      } else if (a.text == "techs-set") {
        send_text_message_fmt(a.c, "$C7Techs set: {}", ps->stats.num_tech_cards_set);
      } else if (a.text == "assists-set") {
        send_text_message_fmt(a.c, "$C7Assists set: {}", ps->stats.num_assist_cards_set);
      } else if (a.text == "defenses-self") {
        send_text_message_fmt(a.c, "$C7Defenses on self:\n{}", ps->stats.defense_actions_set_on_self);
      } else if (a.text == "defenses-ally") {
        send_text_message_fmt(a.c, "$C7Defenses on ally:\n{}", ps->stats.defense_actions_set_on_ally);
      } else if (a.text == "cards-drawn") {
        send_text_message_fmt(a.c, "$C7Cards drawn: {}", ps->stats.num_cards_drawn);
      } else if (a.text == "max-attack-damage") {
        send_text_message_fmt(a.c, "$C7Maximum attack damage:\n{}", ps->stats.max_attack_damage);
      } else if (a.text == "max-combo") {
        send_text_message_fmt(a.c, "$C7Longest combo: {}", ps->stats.max_attack_combo_size);
      } else if (a.text == "attacks-given") {
        send_text_message_fmt(a.c, "$C7Attacks given: {}", ps->stats.num_attacks_given);
      } else if (a.text == "attacks-taken") {
        send_text_message_fmt(a.c, "$C7Attacks taken: {}", ps->stats.num_attacks_taken);
      } else if (a.text == "sc-damage") {
        send_text_message_fmt(a.c, "$C7SC damage taken: {}", ps->stats.sc_damage_taken);
      } else if (a.text == "damage-defended") {
        send_text_message_fmt(a.c, "$C7Damage defended: {}", ps->stats.action_card_negated_damage);
      } else {
        throw precondition_failed("$C6Unknown statistic");
      }
      co_return;
    });

ChatCommandDefinition cc_surrender(
    {"$surrender"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      auto l = a.c->require_lobby();
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
      string name = remove_color(a.c->character_file()->disp.name.decode(a.c->language()));
      send_text_message_fmt(l, "$C6{} has\nsurrendered", name);
      for (const auto& watcher_l : l->watcher_lobbies) {
        send_text_message_fmt(watcher_l, "$C6{} has\nsurrendered", name);
      }
      l->ep3_server->force_battle_result(a.c->lobby_client_id, false);
      co_return;
    });

ChatCommandDefinition cc_swa(
    {"$swa"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto s = a.c->require_server_state();
      auto l = a.c->require_lobby();
      a.check_is_game(true);

      a.c->toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
      send_text_message_fmt(a.c, "$C6Switch assist {}",
          a.c->check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? "enabled" : "disabled");
      co_return;
    });

static void command_swset_swclear(const Args& a, bool should_set) {
  a.check_debug_enabled();
  a.check_is_game(true);

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

  uint8_t cmd_flags = should_set ? 0x01 : 0x00;
  G_WriteSwitchFlag_6x05 cmd = {{0x05, 0x03, 0xFFFF}, 0, 0, flag_num, floor, cmd_flags};

  if (!a.c->proxy_session) {
    auto l = a.c->require_lobby();
    if (l->switch_flags) {
      if (should_set) {
        l->switch_flags->set(floor, flag_num);
      } else {
        l->switch_flags->clear(floor, flag_num);
      }
    }
    send_command_t(l, 0x60, 0x00, cmd);
  } else {
    send_command_t(a.c, 0x60, 0x00, cmd);
    send_command_t(a.c->proxy_session->server_channel, 0x60, 0x00, cmd);
  }
}

ChatCommandDefinition cc_swclear(
    {"$swclear"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_swset_swclear(a, false);
      co_return;
    });

ChatCommandDefinition cc_swset(
    {"$swset"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_swset_swclear(a, true);
      co_return;
    });

ChatCommandDefinition cc_swsetall(
    {"$swsetall"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_debug_enabled();
      a.check_is_game(true);

      parray<G_WriteSwitchFlag_6x05, 0x100> cmds;
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

      if (!a.c->proxy_session) {
        auto l = a.c->require_lobby();
        if (!l->is_game()) {
          throw precondition_failed("$C6This command cannot\nbe used in the lobby");
        }
        if (l->switch_flags) {
          l->switch_flags->data[a.c->floor].clear(0xFF);
        }
        send_command_t(l, 0x6C, 0x00, cmds);
      } else {
        send_command_t(a.c, 0x6C, 0x00, cmds);
        send_command_t(a.c->proxy_session->server_channel, 0x6C, 0x00, cmds);
      }
      co_return;
    });

ChatCommandDefinition cc_switchchar(
    {"$switchchar"},
    +[](const Args& a) -> asio::awaitable<void> {
      auto l = a.c->require_lobby();
      auto s = a.c->require_server_state();

      a.check_is_proxy(false);
      a.check_is_game(false);
      if (a.c->version() != Version::BB_V4) {
        throw precondition_failed("This command can only\nbe used on BB");
      }

      int32_t index = stol(a.text, nullptr, 0) - 1;
      if (index < 0) {
        throw precondition_failed("Invalid slot number");
      }
      auto filename = Client::character_filename(a.c->login->bb_license->username, index);
      if (!std::filesystem::is_regular_file(filename)) {
        throw precondition_failed("No character exists\nin that slot");
      }

      a.c->save_and_unload_character();
      a.c->bb_character_index = index;
      a.c->bb_bank_character_index = index;

      // TODO: This can trigger a client bug where the previous character's
      // name label object isn't deleted if the leave and join notifications
      // are received on the same frame. This results in the receiving player
      // seeing both labels over the new character, with the latest one
      // appearing on top. We could fix this by requiring each recipient to
      // reply to a ping between the two commands, similar to how the 64 and
      // 6x6D commands are split during game joining, but implementing that
      // here seems not worth the effort given the low likelihood and impact of
      // this bug.
      send_complete_player_bb(a.c);
      send_player_leave_notification(l, a.c->lobby_client_id);
      s->send_lobby_join_notifications(l, a.c);

      co_return;
    });

ChatCommandDefinition cc_unset(
    {"$unset"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      a.check_is_ep3(true);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_in_game(s->cheat_flags.ep3_unset_field_character);
      auto l = a.c->require_lobby();
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
      co_return;
    });

ChatCommandDefinition cc_variations(
    {"$variations"},
    +[](const Args& a) -> asio::awaitable<void> {
      // Note: This command is intentionally undocumented, since it's primarily used
      // for testing. If we ever make it public, we should add some kind of user
      // feedback (currently it sends no message when it runs).
      a.check_is_proxy(false);
      a.check_is_game(false);
      auto s = a.c->require_server_state();
      a.check_cheats_enabled_in_game(s->cheat_flags.override_variations);

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
      a.c->log.info_f("Override variations set to {}", vars_str);
      co_return;
    });

static void command_warp(const Args& a, bool is_warpall) {
  a.check_is_game(true);
  auto s = a.c->require_server_state();
  a.check_cheats_enabled_or_allowed(s->cheat_flags.warp);

  uint32_t floor = stoul(a.text, nullptr, 0);
  if (!is_warpall && (a.c->floor == floor)) {
    return;
  }

  Episode episode = a.c->proxy_session
      ? a.c->proxy_session->lobby_episode
      : a.c->require_lobby()->episode;
  size_t limit = FloorDefinition::limit_for_episode(episode);
  if (limit == 0) {
    return;
  } else if (floor > limit) {
    throw precondition_failed("$C6Area numbers must\nbe {} or less", limit);
  }

  if (a.c->proxy_session) {
    send_warp(a.c, floor, true);
    if (is_warpall) {
      for (size_t z = 0; z < a.c->proxy_session->lobby_players.size(); z++) {
        const auto& lp = a.c->proxy_session->lobby_players[z];
        if (lp.guild_card_number) {
          send_warp(a.c->proxy_session->server_channel, z, floor, true);
        }
      }
    }
  } else if (is_warpall) {
    send_warp(a.c->require_lobby(), floor, false);
  } else {
    send_warp(a.c, floor, true);
  }
}

ChatCommandDefinition cc_warp(
    {"$warp", "$warpme"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_warp(a, false);
      co_return;
    });

ChatCommandDefinition cc_warpall(
    {"$warpall"},
    +[](const Args& a) -> asio::awaitable<void> {
      command_warp(a, true);
      co_return;
    });

ChatCommandDefinition cc_what(
    {"$what"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_is_proxy(false);
      a.check_is_game(true);
      auto l = a.c->require_lobby();
      if (!episode_has_arpg_semantics(l->episode)) {
        co_return;
      }

      shared_ptr<const Lobby::FloorItem> nearest_fi;
      float min_dist2 = 0.0f;
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
        string name = s->describe_item(a.c->version(), nearest_fi->data, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);
        send_text_message(a.c, name);
      }
      co_return;
    });

static void whatobj_whatene_fn(const Args& a, bool include_objs, bool include_enes) {
  // TODO: This probably wouldn't be too hard to implement for proxy sessions.
  // We already have the map and most of the lobby metadata (episode, etc.)
  a.check_is_proxy(false);
  a.check_is_game(true);
  auto l = a.c->require_lobby();
  if (!l->map_state) {
    throw precondition_failed("$C4No map loaded");
  }

  // TODO: We should use the actual area if a loaded quest has reassigned
  // them; it's likely that the variations will be wrong if we don't
  uint8_t area, layout_var;
  auto s = a.c->require_server_state();
  if (l->episode != Episode::EP3) {
    auto sdt = s->set_data_table(a.c->version(), l->episode, l->mode, l->difficulty);
    area = sdt->default_area_for_floor(l->episode, a.c->floor);
    layout_var = (a.c->floor < 0x10) ? l->variations.entries[a.c->floor].layout.load() : 0x00;
  } else {
    area = a.c->floor;
    layout_var = 0;
  }

  double min_dist2 = -1.0;
  VectorXYZF nearest_worldspace_pos;
  shared_ptr<const MapState::ObjectState> nearest_obj;
  shared_ptr<const MapState::EnemyState> nearest_ene;

  auto check_entity = [&](auto& nearest_entity, auto entity, const auto& def) -> void {
    VectorXYZF worldspace_pos;
    if (l->episode != Episode::EP3) {
      try {
        const auto& room = s->room_layout_index->get_room(area, layout_var, def.set_entry->room);
        // This is the order in which the game does the rotations; not sure why
        worldspace_pos = def.set_entry->pos.rotate_x(room.angle.x).rotate_z(room.angle.z).rotate_y(room.angle.y) + room.position;
      } catch (const out_of_range&) {
        a.c->log.warning_f("Can't find definition for room {:02X}:{:02X}:{:08X}", area, layout_var, def.set_entry->room);
        worldspace_pos = def.set_entry->pos;
      }
    } else {
      worldspace_pos = def.set_entry->pos;
    }

    float dist2 = (VectorXZF(worldspace_pos) - a.c->pos).norm2();
    if ((min_dist2 < 0.0) || (dist2 < min_dist2)) {
      nearest_entity = entity;
      nearest_worldspace_pos = worldspace_pos;
      min_dist2 = dist2;
    }
  };

  if (include_objs) {
    for (const auto& it : l->map_state->iter_object_states(a.c->version())) {
      if (it->super_obj && (it->super_obj->floor == a.c->floor)) {
        const auto& def = it->super_obj->version(a.c->version());
        if (def.set_entry) {
          check_entity(nearest_obj, it, def);
        }
      }
    }
  }
  if (include_enes) {
    for (const auto& it : l->map_state->iter_enemy_states(a.c->version())) {
      if (it->super_ene && (it->super_ene->floor == a.c->floor)) {
        const auto& def = it->super_ene->version(a.c->version());
        if (def.set_entry) {
          check_entity(nearest_ene, it, def);
        }
      }
    }
  }

  // Since we check all objects first, nearest_ene will only be set if
  // there is an enemy closer than all objects. So, we print that if it's
  // set, and print the object if not.
  if (nearest_ene) {
    const auto* set_entry = nearest_ene->super_ene->version(a.c->version()).set_entry;
    string type_name = MapFile::name_for_enemy_type(set_entry->base_type, a.c->version(), area);
    send_text_message_fmt(a.c, "$C5E-{:03X}\n$C6{}\n$C2{}\n$C7X:{:.2f} Z:{:.2f}",
        nearest_ene->e_id, phosg::name_for_enum(nearest_ene->type(a.c->version(), l->episode, l->event)),
        type_name, nearest_worldspace_pos.x, nearest_worldspace_pos.z);
    auto set_str = set_entry->str(a.c->version(), area);
    a.c->log.info_f("Enemy found via $whatobj: E-{:03X} {} at x={:g} y={:g} z={:g}",
        nearest_ene->e_id, set_str,
        nearest_worldspace_pos.x, nearest_worldspace_pos.y, nearest_worldspace_pos.z);

  } else if (nearest_obj) {
    const auto* set_entry = nearest_obj->super_obj->version(a.c->version()).set_entry;
    auto type_name = nearest_obj->type_name(a.c->version());
    send_text_message_fmt(a.c, "$C5K-{:03X}\n$C6{}\n$C7X:{:.2f} Z:{:.2f}",
        nearest_obj->k_id, type_name, nearest_worldspace_pos.x, nearest_worldspace_pos.z);
    auto set_str = set_entry->str(a.c->version(), area);
    a.c->log.info_f("Object found via $whatobj: K-{:03X} {} at x={:g} y={:g} z={:g}",
        nearest_obj->k_id, set_str,
        nearest_worldspace_pos.x, nearest_worldspace_pos.y, nearest_worldspace_pos.z);

  } else {
    throw precondition_failed("$C4No objects or\nenemies are nearby");
  }
}

ChatCommandDefinition cc_whatobj(
    {"$whatobj"},
    +[](const Args& a) -> asio::awaitable<void> {
      whatobj_whatene_fn(a, true, false);
      co_return;
    });
ChatCommandDefinition cc_whatene(
    {"$whatene"},
    +[](const Args& a) -> asio::awaitable<void> {
      whatobj_whatene_fn(a, false, true);
      co_return;
    });

ChatCommandDefinition cc_where(
    {"$where"},
    +[](const Args& a) -> asio::awaitable<void> {
      uint32_t floor;
      Episode episode;
      auto l = a.c->lobby.lock();
      if (a.c->proxy_session) {
        floor = a.c->floor;
        episode = a.c->proxy_session->lobby_episode;
      } else if (l && l->is_game()) {
        floor = a.c->floor;
        episode = l->episode;
      } else {
        floor = 0x0F;
        episode = Episode::EP1;
      }

      send_text_message_fmt(a.c, "$C7{:X}:{} X:{} Z:{}",
          floor,
          FloorDefinition::get(episode, floor).short_name,
          static_cast<int32_t>(a.c->pos.x),
          static_cast<int32_t>(a.c->pos.z));

      if (!a.c->proxy_session && l && l->is_game()) {
        for (auto lc : l->clients) {
          if (lc && (lc != a.c)) {
            string name = lc->character_file()->disp.name.decode(lc->language());
            send_text_message_fmt(a.c, "$C6{}$C7 {:X}:{}",
                name, lc->floor, FloorDefinition::get(l->episode, lc->floor).short_name);
          }
        }
      }
      co_return;
    });

ChatCommandDefinition cc_writemem(
    {"$writemem"},
    +[](const Args& a) -> asio::awaitable<void> {
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

      co_await prepare_client_for_patches(a.c);

      try {
        auto s = a.c->require_server_state();
        const char* function_name = is_dc(a.c->version())
            ? "WriteMemoryDC"
            : is_gc(a.c->version())
            ? "WriteMemoryGC"
            : "WriteMemoryX86";
        auto fn = s->function_code_index->name_to_function.at(function_name);
        unordered_map<string, uint32_t> label_writes{{"dest_addr", addr}, {"size", data.size()}};
        co_await send_function_call(a.c, fn, label_writes, data.data(), data.size());
      } catch (const out_of_range&) {
        throw precondition_failed("Invalid patch name");
      }
      co_return;
    });

ChatCommandDefinition cc_nativecall(
    {"$nativecall"},
    +[](const Args& a) -> asio::awaitable<void> {
      a.check_debug_enabled();

      // TODO: $nativecall is not implemented on x86 (yet) because there are
      // multiple calling conventions used within the executable (at least on
      // Xbox and BB), so we would need a way to specify which calling
      // convention to use, which would be annoying
      if (is_x86(a.c->version())) {
        throw precondition_failed("Command not supported\non x86 clients");
      }

      auto tokens = phosg::split(a.text, ' ');
      if (tokens.size() < 1) {
        throw precondition_failed("Incorrect arguments");
      }

      uint32_t addr = stoul(tokens[0], nullptr, 16);
      if (!console_address_in_range(a.c->version(), addr)) {
        throw precondition_failed("$C4Function address\nout of range");
      }

      unordered_map<string, uint32_t> label_writes{{"call_addr", addr}};
      for (size_t z = 0; z < tokens.size() - 1; z++) {
        label_writes.emplace(std::format("arg{}", z), stoull(tokens[z + 1], nullptr, 16));
      }

      co_await prepare_client_for_patches(a.c);

      try {
        auto s = a.c->require_server_state();
        const char* function_name = is_dc(a.c->version())
            ? "CallNativeFunctionDC"
            : is_gc(a.c->version())
            ? "CallNativeFunctionGC"
            : "CallNativeFunctionX86";
        auto fn = s->function_code_index->name_to_function.at(function_name);
        co_await send_function_call(a.c, fn, label_writes);
      } catch (const out_of_range&) {
        throw precondition_failed("Invalid patch name");
      }
      co_return;
    });

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
asio::awaitable<void> on_chat_command(std::shared_ptr<Client> c, const std::string& text, bool check_permissions) {
  SplitCommand cmd(text);

  // This function is only called by on_06 if it looks like a chat command
  // (starts with $, or @ on 11/2000), so we just normalize all commands to $
  // here
  if (!cmd.name.empty() && cmd.name[0] == '@') {
    cmd.name[0] = '$';
  }

  const ChatCommandDefinition* def = nullptr;
  try {
    def = ChatCommandDefinition::all_defs.at(cmd.name);
  } catch (const out_of_range&) {
  }
  if (!def) {
    send_text_message(c, "$C6Unknown command");
    co_return;
  }

  try {
    co_await def->handler(Args{cmd.args, check_permissions, c});
  } catch (const precondition_failed& e) {
    send_text_message(c, e.what());
  } catch (const exception& e) {
    send_text_message(c, "$C6Failed:\n" + remove_color(e.what()));
  }
}
