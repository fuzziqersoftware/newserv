#include "Client.hh"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "Version.hh"

using namespace std;

const uint64_t CLIENT_CONFIG_MAGIC = 0x8399AC32;

static atomic<uint64_t> next_id(1);

void Client::set_flags_for_version(Version version, int64_t sub_version) {
  this->set_flag(Flag::PROXY_CHAT_COMMANDS_ENABLED);

  // BB shares some sub_version values with GC Episode 3, so we handle it
  // separately first.
  if (version == Version::BB_V4) {
    this->set_flag(Flag::NO_D6);
    this->set_flag(Flag::SAVE_ENABLED);
    this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
    this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
    this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
    return;
  }

  switch (sub_version) {
    case -1: // Initial check (before sub_version recognition)
      switch (version) {
        case Version::PC_PATCH:
        case Version::BB_PATCH:
          this->set_flag(Flag::NO_D6);
          break;
        case Version::DC_NTE:
        case Version::DC_11_2000:
        case Version::DC_V1:
          this->set_flag(Flag::NO_D6);
          break;
        case Version::DC_V2:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
          this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        case Version::PC_NTE:
        case Version::PC_V2:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
          // SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE not set here
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        case Version::GC_NTE:
          this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
          this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
          break;
        case Version::GC_EP3_NTE:
          this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
          this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
          this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
          break;
        case Version::GC_V3:
        case Version::GC_EP3:
          // Some of these versions have send_function_call and some don't; we
          // have to set these flags later when we get sub_version
          break;
        case Version::XB_V3:
          // TODO: Do all versions of XB need this flag? US does, at least.
          this->set_flag(Flag::NO_D6_AFTER_LOBBY);
          this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
          this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        default:
          throw logic_error("invalid game version");
      }
      break;

    case 0x20: // DC NTE, 11/2000, DCv1 JP
    case 0x21: // DCv1 US
    case 0x22: // DCv1 EU, 12/2000, and 01/2001, at 50Hz
    case 0x23: // DCv1 EU, 12/2000, and 01/2001, at 60Hz
      this->set_flag(Flag::NO_D6);
      break;
    case 0x25: // DCv2 JP
    case 0x26: // DCv2 US and 08/2001
    case 0x27: // DCv2 EU 50Hz
    case 0x28: // DCv2 EU 60Hz
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x29: // PCv2
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      // SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE not set here
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      this->set_flag(Flag::CAN_RECEIVE_ENABLE_B2_QUEST);
      break;
    case 0x30: // GC Ep1&2 GameJam demo, GC Ep1&2 Trial Edition, GC Ep1&2 JP v1.2, XB JP
    case 0x31: // GC Ep1&2 US v1.0, GC US v1.1, XB US
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
      break;
    case 0x32: // GC Ep1&2 EU 50Hz
    case 0x33: // GC Ep1&2 EU 60Hz
    case 0x34: // GC Ep1&2 JP v1.3
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
      break;
    case 0x35: // GC Ep1&2 JP v1.4 (Plus)
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
      this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x3A: // GC Ep1&2 US v1.2 (Plus) Return to Ragol
      this->set_flag(Flag::IS_CLIENT_CUSTOMIZATION);
      [[fallthrough]];
    case 0x36: // GC Ep1&2 US v1.2 (Plus)
    case 0x39: // GC Ep1&2 JP v1.5 (Plus)
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::CAN_RECEIVE_ENABLE_B2_QUEST);
      break;
    case 0x40: // GC Ep3 JP and Trial Edition (and BB, but BB is handled above)
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::HAS_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
      this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      // sub_version can't be used to tell JP final and Trial Edition apart; we
      // instead look at header.flag in the 61 command and set the version then.
      break;
    case 0x41: // GC Ep3 US (and BB, but BB is handled above)
    case 0x42: // GC Ep3 EU 50Hz
    case 0x43: // GC Ep3 EU 60Hz
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::CAN_RECEIVE_ENABLE_B2_QUEST);
      break;
    default:
      throw runtime_error(std::format("unknown sub_version {:X}", sub_version));
  }
}

Client::ItemDropNotificationMode Client::get_drop_notification_mode() const {
  uint8_t mode_s = (this->check_flag(Flag::ITEM_DROP_NOTIFICATIONS_1) ? 1 : 0) |
      (this->check_flag(Flag::ITEM_DROP_NOTIFICATIONS_2) ? 2 : 0);
  return static_cast<Client::ItemDropNotificationMode>(mode_s);
}

void Client::set_drop_notification_mode(ItemDropNotificationMode new_mode) {
  uint8_t mode_s = static_cast<uint8_t>(new_mode);
  if (mode_s & 1) {
    this->set_flag(Client::Flag::ITEM_DROP_NOTIFICATIONS_1);
  } else {
    this->clear_flag(Client::Flag::ITEM_DROP_NOTIFICATIONS_1);
  }
  if (mode_s & 2) {
    this->set_flag(Client::Flag::ITEM_DROP_NOTIFICATIONS_2);
  } else {
    this->clear_flag(Client::Flag::ITEM_DROP_NOTIFICATIONS_2);
  }
}

Client::Client(
    shared_ptr<GameServer> server,
    shared_ptr<Channel> channel,
    ServerBehavior server_behavior)
    : server(server),
      id(next_id++),
      log(std::format("[C-{:X}] ", this->id), client_log.min_level),
      channel(channel),
      server_behavior(server_behavior),
      save_game_data_timer(*server->get_io_context()),
      send_ping_timer(*server->get_io_context()),
      idle_timeout_timer(*server->get_io_context()),
      should_update_play_time(false) {
  this->update_channel_name();

  // Don't print data sent to patch clients to the logs. The patch server
  // protocol is fully understood and data logs for patch clients are generally
  // more annoying than helpful at this point.
  auto s = server->get_state();
  if (is_patch(this->version()) && s->hide_download_commands) {
    this->channel->terminal_recv_color = phosg::TerminalFormat::END;
    this->channel->terminal_send_color = phosg::TerminalFormat::END;
  } else {
    this->channel->terminal_recv_color = phosg::TerminalFormat::FG_GREEN;
    this->channel->terminal_send_color = phosg::TerminalFormat::FG_YELLOW;
  }

  this->set_flags_for_version(this->version(), -1);
  if (is_v1_or_v2(this->version()) ? s->default_rare_notifs_enabled_v1_v2 : s->default_rare_notifs_enabled_v3_v4) {
    this->set_drop_notification_mode(ItemDropNotificationMode::RARES_ONLY);
  }
  this->specific_version = default_specific_version_for_version(this->version(), -1);

  this->reschedule_save_game_data_timer();
  this->reschedule_ping_and_timeout_timers();

  // Don't print data sent to patch clients to the logs. The patch server
  // protocol is fully understood and data logs for patch clients are generally
  // more annoying than helpful at this point.
  if ((s->hide_download_commands) &&
      ((this->version() == Version::PC_PATCH) || (this->version() == Version::BB_PATCH))) {
    this->channel->terminal_recv_color = phosg::TerminalFormat::END;
    this->channel->terminal_send_color = phosg::TerminalFormat::END;
  } else {
    this->channel->terminal_send_color = phosg::TerminalFormat::FG_YELLOW;
    this->channel->terminal_recv_color = phosg::TerminalFormat::FG_GREEN;
  }

  this->log.info_f("Created");
}

Client::~Client() {
  if (!this->disconnect_hooks.empty()) {
    this->log.warning_f("Disconnect hooks pending at client destruction time:");
    for (const auto& it : this->disconnect_hooks) {
      this->log.warning_f("  {}", it.first);
    }
  }

  if ((this->version() == Version::BB_V4) && (this->character_data.get())) {
    this->save_all();
  }
  this->log.info_f("Deleted");
}

void Client::update_channel_name() {
  string default_name = this->channel->default_name();

  auto player = this->character(false, false);
  if (player) {
    string name_str = player->disp.name.decode(this->language());
    size_t level = player->disp.stats.level + 1;
    this->channel->name = std::format("C-{:X} ({} Lv.{}) @ {}",
        this->id, name_str, level, default_name);
  } else {
    this->channel->name = std::format("C-{:X} @ {}", this->id, default_name);
  }
  this->log.info_f("Channel name updated: {}", this->channel->name);
}

void Client::reschedule_save_game_data_timer() {
  if (this->version() != Version::BB_V4) {
    return;
  }
  this->save_game_data_timer.expires_after(std::chrono::seconds(60));
  this->save_game_data_timer.async_wait([this](std::error_code ec) {
    if (!ec) {
      if (this->character(false)) {
        this->save_all();
      }
      this->reschedule_save_game_data_timer();
    }
  });
}

void Client::reschedule_ping_and_timeout_timers() {
  auto s = this->require_server_state();
  if (!is_patch(this->version())) {
    this->send_ping_timer.expires_after(std::chrono::microseconds(s->client_ping_interval_usecs));
    this->send_ping_timer.async_wait([this](std::error_code ec) {
      if (!ec) {
        this->log.info_f("Sending ping command");
        try {
          // The game doesn't use this timestamp; we only use it for debugging purposes
          be_uint64_t timestamp = phosg::now();
          this->channel->send(0x1D, 0x00, &timestamp, sizeof(be_uint64_t));
        } catch (const exception& e) {
          this->log.warning_f("Failed to send ping: {}", e.what());
        }
      }
    });
  }

  this->idle_timeout_timer.expires_after(std::chrono::microseconds(s->client_idle_timeout_usecs));
  this->idle_timeout_timer.async_wait([this](std::error_code ec) {
    if (!ec) {
      this->log.info_f("Idle timeout expired");
      this->channel->disconnect();
    }
  });
}

void Client::convert_account_to_temporary_if_nte() {
  // If the session is a prototype version and the account was created and we
  // should use a temporary account instead, delete the permanent account and
  // replace it with a temporary account.
  auto s = this->require_server_state();
  if (s->use_temp_accounts_for_prototypes && this->login->account_was_created && is_any_nte(this->version())) {
    this->log.info_f("Client is a prototype version and the account was created during this session; converting permanent account to temporary account");
    this->login->account->is_temporary = true;
    this->login->account->delete_file();
    this->login->account_was_created = false;
  }
}

shared_ptr<ServerState> Client::require_server_state() const {
  auto server = this->server.lock();
  if (!server) {
    throw logic_error("server is deleted");
  }
  return server->get_state();
}

shared_ptr<Lobby> Client::require_lobby() const {
  auto l = this->lobby.lock();
  if (!l) {
    throw runtime_error("client not in any lobby");
  }
  return l;
}

shared_ptr<const TeamIndex::Team> Client::team() const {
  if (!this->login) {
    throw logic_error("Client::team called on client with no account");
  }

  if (this->login->account->bb_team_id == 0) {
    return nullptr;
  }

  auto p = this->character(false);
  auto s = this->require_server_state();
  auto team = s->team_index->get_by_id(this->login->account->bb_team_id);
  if (!team) {
    this->log.info_f("Account contains a team ID, but the team does not exist; clearing team ID from account");
    this->login->account->bb_team_id = 0;
    this->login->account->save();
    return nullptr;
  }

  auto member_it = team->members.find(this->login->account->account_id);
  if (member_it == team->members.end()) {
    this->log.info_f("Account contains a team ID, but the team does not contain this member; clearing team ID from account");
    this->login->account->bb_team_id = 0;
    this->login->account->save();
    return nullptr;
  }

  // The team membership is valid, but the player name may be different; update
  // the team membership if needed
  if (p) {
    auto& m = member_it->second;
    string name = p->disp.name.decode(this->language());
    if (m.name != name) {
      this->log.info_f("Updating player name in team config");
      s->team_index->update_member_name(this->login->account->account_id, name);
    }
  }

  return team;
}

bool Client::evaluate_quest_availability_expression(
    shared_ptr<const IntegralExpression> expr,
    shared_ptr<const Lobby> game,
    uint8_t event,
    uint8_t difficulty,
    size_t num_players,
    bool v1_present) const {
  if (this->login && this->login->account->check_flag(Account::Flag::DISABLE_QUEST_REQUIREMENTS)) {
    return true;
  }
  if (!expr) {
    return true;
  }
  if (game && !game->quest_flag_values) {
    throw logic_error("quest flags are missing from game");
  }
  auto p = this->character();
  IntegralExpression::Env env = {
      .flags = &p->quest_flags.data.at(difficulty),
      .challenge_records = &p->challenge_records,
      .team = this->team(),
      .num_players = num_players,
      .event = event,
      .v1_present = v1_present,
  };
  int64_t ret = expr->evaluate(env);
  if (this->log.should_log(phosg::LogLevel::L_INFO)) {
    string expr_str = expr->str();
    this->log.info_f("Evaluated integral expression {} => {}", expr_str, ret ? "TRUE" : "FALSE");
  }
  return ret;
}

bool Client::can_see_quest(
    shared_ptr<const Quest> q,
    shared_ptr<const Lobby> game,
    uint8_t event,
    uint8_t difficulty,
    size_t num_players,
    bool v1_present) const {
  if (!q->has_version_any_language(this->version())) {
    return false;
  }
  return this->evaluate_quest_availability_expression(q->available_expression, game, event, difficulty, num_players, v1_present);
}

bool Client::can_play_quest(
    shared_ptr<const Quest> q,
    shared_ptr<const Lobby> game,
    uint8_t event,
    uint8_t difficulty,
    size_t num_players,
    bool v1_present) const {
  if (!q->has_version_any_language(this->version())) {
    return false;
  }
  if (num_players > q->max_players) {
    return false;
  }
  return this->evaluate_quest_availability_expression(q->enabled_expression, game, event, difficulty, num_players, v1_present);
}

bool Client::can_use_chat_commands() const {
  if (!this->login) {
    return false;
  }
  if (this->login->account->check_flag(Account::Flag::ALWAYS_ENABLE_CHAT_COMMANDS)) {
    return true;
  }
  return this->require_server_state()->enable_chat_commands;
}

void Client::set_login(shared_ptr<Login> login) {
  this->login = login;
  if (this->log.should_log(phosg::LogLevel::L_INFO)) {
    string login_str = this->login->str();
    this->log.info_f("Login: {}", login_str);
  }
}

void Client::create_battle_overlay(shared_ptr<const BattleRules> rules, shared_ptr<const LevelTable> level_table) {
  this->overlay_character_data = make_shared<PSOBBCharacterFile>(*this->character(true, false));

  if (rules->weapon_and_armor_mode != BattleRules::WeaponAndArmorMode::ALLOW) {
    this->overlay_character_data->inventory.remove_all_items_of_type(0);
    this->overlay_character_data->inventory.remove_all_items_of_type(1);
  }
  if (rules->mag_mode == BattleRules::MagMode::FORBID_ALL) {
    this->overlay_character_data->inventory.remove_all_items_of_type(2);
  }
  if (rules->tool_mode != BattleRules::ToolMode::ALLOW) {
    this->overlay_character_data->inventory.remove_all_items_of_type(3);
  }
  if (rules->replace_char) {
    // TODO: Shouldn't we clear other material usage here? It looks like the
    // original code doesn't, but that seems wrong.
    this->overlay_character_data->inventory.hp_from_materials = 0;
    this->overlay_character_data->inventory.tp_from_materials = 0;

    uint32_t target_level = clamp<uint32_t>(rules->char_level, 0, 199);
    uint8_t char_class = this->overlay_character_data->disp.visual.char_class;
    auto& stats = this->overlay_character_data->disp.stats;

    level_table->reset_to_base(stats, char_class);
    level_table->advance_to_level(stats, target_level, char_class);

    stats.esp = 40;
    stats.meseta = 300;
  }
  if (rules->tech_disk_mode == BattleRules::TechDiskMode::LIMIT_LEVEL) {
    // TODO: Verify this is what the game actually does.
    for (uint8_t tech_num = 0; tech_num < 0x13; tech_num++) {
      uint8_t existing_level = this->overlay_character_data->get_technique_level(tech_num);
      if ((existing_level != 0xFF) && (existing_level > rules->max_tech_level)) {
        this->overlay_character_data->set_technique_level(tech_num, rules->max_tech_level);
      }
    }
  } else if (rules->tech_disk_mode == BattleRules::TechDiskMode::FORBID_ALL) {
    for (uint8_t tech_num = 0; tech_num < 0x13; tech_num++) {
      this->overlay_character_data->set_technique_level(tech_num, 0xFF);
    }
  }
  if (rules->meseta_mode != BattleRules::MesetaMode::ALLOW) {
    this->overlay_character_data->disp.stats.meseta = 0;
  }
  if (rules->forbid_scape_dolls) {
    this->overlay_character_data->inventory.remove_all_items_of_type(3, 9);
  }
}

void Client::create_challenge_overlay(Version version, size_t template_index, shared_ptr<const LevelTable> level_table) {
  auto p = this->character(true, false);
  const auto& tpl = get_challenge_template_definition(version, p->disp.visual.class_flags, template_index);

  this->overlay_character_data = make_shared<PSOBBCharacterFile>(*p);
  auto overlay = this->overlay_character_data;

  for (size_t z = 0; z < overlay->inventory.items.size(); z++) {
    auto& i = overlay->inventory.items[z];
    i.present = 0;
    i.unknown_a1 = 0;
    i.extension_data1 = 0;
    i.extension_data2 = 0;
    i.flags = 0;
    i.data = ItemData();
  }

  overlay->inventory.items[13].extension_data2 = 1;

  level_table->reset_to_base(overlay->disp.stats, overlay->disp.visual.char_class);
  level_table->advance_to_level(overlay->disp.stats, tpl.level, overlay->disp.visual.char_class);

  overlay->disp.stats.esp = 40;
  overlay->disp.stats.unknown_a3 = 10.0;
  overlay->disp.stats.experience = level_table->stats_delta_for_level(overlay->disp.visual.char_class, overlay->disp.stats.level).experience;
  overlay->disp.stats.meseta = 0;
  overlay->clear_all_material_usage();
  for (size_t z = 0; z < 0x13; z++) {
    overlay->set_technique_level(z, 0xFF);
  }

  for (size_t z = 0; z < tpl.items.size(); z++) {
    auto& inv_item = overlay->inventory.items[z];
    inv_item.present = tpl.items[z].present;
    inv_item.unknown_a1 = tpl.items[z].unknown_a1;
    inv_item.flags = tpl.items[z].flags;
    inv_item.data = tpl.items[z].data;
  }
  overlay->inventory.num_items = tpl.items.size();

  for (const auto& tech_level : tpl.tech_levels) {
    overlay->set_technique_level(tech_level.tech_num, tech_level.level);
  }
}

void Client::import_blocked_senders(const parray<le_uint32_t, 30>& blocked_senders) {
  this->blocked_senders.clear();
  for (size_t z = 0; z < blocked_senders.size(); z++) {
    if (blocked_senders[z]) {
      this->blocked_senders.emplace(blocked_senders[z]);
    }
  }
}

shared_ptr<PSOBBBaseSystemFile> Client::system_file(bool allow_load) {
  if (!this->system_data && allow_load) {
    this->load_all_files();
  }
  return this->system_data;
}

shared_ptr<const PSOBBBaseSystemFile> Client::system_file(bool allow_load) const {
  if (!this->system_data.get() && allow_load) {
    throw runtime_error("system data is not loaded");
  }
  return this->system_data;
}

shared_ptr<PSOBBCharacterFile> Client::character(bool allow_load, bool allow_overlay) {
  if (this->overlay_character_data && allow_overlay) {
    return this->overlay_character_data;
  }
  if (!this->character_data && allow_load) {
    if ((this->version() == Version::BB_V4) && (this->bb_character_index < 0)) {
      throw runtime_error("character index not specified");
    }
    this->load_all_files();
  }
  return this->character_data;
}

shared_ptr<const PSOBBCharacterFile> Client::character(bool allow_load, bool allow_overlay) const {
  if (allow_overlay && this->overlay_character_data) {
    return this->overlay_character_data;
  }
  if (!this->character_data && allow_load) {
    throw runtime_error("character data is not loaded");
  }
  return this->character_data;
}

shared_ptr<PSOBBGuildCardFile> Client::guild_card_file(bool allow_load) {
  if (!this->guild_card_data && allow_load) {
    this->load_all_files();
  }
  return this->guild_card_data;
}

shared_ptr<const PSOBBGuildCardFile> Client::guild_card_file(bool allow_load) const {
  if (!this->guild_card_data && allow_load) {
    throw runtime_error("account data is not loaded");
  }
  return this->guild_card_data;
}

string Client::system_filename() const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have system data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  return std::format("system/players/system_{}.psosys", this->login->bb_license->username);
}

string Client::character_filename(const std::string& bb_username, ssize_t index) {
  if (bb_username.empty()) {
    throw logic_error("non-BB players do not have character data");
  }
  if (index < 0) {
    throw logic_error("character index is not set");
  }
  return std::format("system/players/player_{}_{}.psochar", bb_username, index);
}

string Client::backup_character_filename(uint32_t account_id, size_t index, bool is_ep3) {
  return std::format("system/players/backup_player_{}_{}.{}",
      account_id, index, is_ep3 ? "pso3char" : "psochar");
}

string Client::character_filename(ssize_t index) const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have character data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  return this->character_filename(this->login->bb_license->username, (index < 0) ? this->bb_character_index : index);
}

string Client::guild_card_filename() const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have character data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  return std::format("system/players/guild_cards_{}.psocard", this->login->bb_license->username);
}

string Client::shared_bank_filename() const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have character data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  return std::format("system/players/shared_bank_{}.psobank", this->login->bb_license->username);
}

string Client::legacy_account_filename() const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have character data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  return std::format("system/players/account_{}.nsa", this->login->bb_license->username);
}

string Client::legacy_player_filename() const {
  if (this->version() != Version::BB_V4) {
    throw logic_error("non-BB players do not have character data");
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("client is not logged in");
  }
  if (this->bb_character_index < 0) {
    throw logic_error("character index is not set");
  }
  return std::format(
      "system/players/player_{}_{}.nsc",
      this->login->bb_license->username,
      static_cast<ssize_t>(this->bb_character_index + 1));
}

void Client::create_character_file(
    uint32_t guild_card_number,
    uint8_t language,
    const PlayerDispDataBBPreview& preview,
    shared_ptr<const LevelTable> level_table) {
  this->character_data = PSOBBCharacterFile::create_from_preview(guild_card_number, language, preview, level_table);
  this->save_character_file();
}

void Client::load_all_files() {
  if (this->version() != Version::BB_V4) {
    this->system_data = make_shared<PSOBBBaseSystemFile>();
    this->character_data = make_shared<PSOBBCharacterFile>();
    this->guild_card_data = make_shared<PSOBBGuildCardFile>();
    return;
  }
  if (!this->login || !this->login->bb_license) {
    throw logic_error("cannot load BB player data until client is logged in");
  }

  this->system_data.reset();
  this->character_data.reset();
  this->guild_card_data.reset();

  auto files_manager = this->require_server_state()->player_files_manager;

  string sys_filename = this->system_filename();
  this->system_data = files_manager->get_system(sys_filename);
  if (this->system_data) {
    player_data_log.info_f("Using loaded system file {}", sys_filename);
  } else if (std::filesystem::is_regular_file(sys_filename)) {
    this->system_data = make_shared<PSOBBBaseSystemFile>(phosg::load_object_file<PSOBBBaseSystemFile>(sys_filename, true));
    files_manager->set_system(sys_filename, this->system_data);
    player_data_log.info_f("Loaded system data from {}", sys_filename);
  } else {
    player_data_log.info_f("System file is missing: {}", sys_filename);
  }

  if (this->bb_character_index >= 0) {
    string char_filename = this->character_filename();
    this->character_data = files_manager->get_character(char_filename);
    if (this->character_data) {
      player_data_log.info_f("Using loaded character file {}", char_filename);
    } else if (std::filesystem::is_regular_file(char_filename)) {
      auto psochar = PSOCHARFile::load_shared(char_filename, !this->system_data);
      this->character_data = psochar.character_file;
      files_manager->set_character(char_filename, this->character_data);
      player_data_log.info_f("Loaded character data from {}", char_filename);

      // If there was no .psosys file, use the system file from the .psochar
      // file instead
      if (!this->system_data) {
        if (!psochar.system_file) {
          throw logic_error("account system data not present, and also not loaded from psochar file");
        }
        this->system_data = psochar.system_file;
        files_manager->set_system(sys_filename, this->system_data);
        player_data_log.info_f("Loaded system data from {}", char_filename);
      }

      this->update_character_data_after_load(this->character_data);
      this->system_data->language = this->language();

    } else {
      player_data_log.info_f("Character file is missing: {}", char_filename);
    }
  }

  string card_filename = this->guild_card_filename();
  this->guild_card_data = files_manager->get_guild_card(card_filename);
  if (this->guild_card_data) {
    player_data_log.info_f("Using loaded Guild Card file {}", card_filename);
  } else if (std::filesystem::is_regular_file(card_filename)) {
    this->guild_card_data = make_shared<PSOBBGuildCardFile>(phosg::load_object_file<PSOBBGuildCardFile>(card_filename));
    files_manager->set_guild_card(card_filename, this->guild_card_data);
    player_data_log.info_f("Loaded Guild Card data from {}", card_filename);
  } else {
    player_data_log.info_f("Guild Card file is missing: {}", card_filename);
  }

  // If any of the above files were missing, try to load from .nsa/.nsc files instead
  if (!this->system_data || (!this->character_data && (this->bb_character_index >= 0)) || !this->guild_card_data) {
    string nsa_filename = this->legacy_account_filename();
    shared_ptr<LegacySavedAccountDataBB> nsa_data;
    if (std::filesystem::is_regular_file(nsa_filename)) {
      nsa_data = make_shared<LegacySavedAccountDataBB>(phosg::load_object_file<LegacySavedAccountDataBB>(nsa_filename));
      if (!nsa_data->signature.eq(LegacySavedAccountDataBB::SIGNATURE)) {
        throw runtime_error("account data header is incorrect");
      }
      if (!this->system_data) {
        this->system_data = make_shared<PSOBBBaseSystemFile>(nsa_data->system_file);
        files_manager->set_system(sys_filename, this->system_data);
        player_data_log.info_f("Loaded legacy system data from {}", nsa_filename);
      }
      if (!this->guild_card_data) {
        this->guild_card_data = make_shared<PSOBBGuildCardFile>(nsa_data->guild_card_file);
        files_manager->set_guild_card(card_filename, this->guild_card_data);
        player_data_log.info_f("Loaded legacy Guild Card data from {}", nsa_filename);
      }
    }

    if (!this->system_data) {
      this->system_data = make_shared<PSOBBBaseSystemFile>();
      auto s = this->require_server_state();
      if (s->bb_default_keyboard_config) {
        this->system_data->key_config = *s->bb_default_keyboard_config;
      }
      if (s->bb_default_joystick_config) {
        this->system_data->joystick_config = *s->bb_default_joystick_config;
      }
      files_manager->set_system(sys_filename, this->system_data);
      player_data_log.info_f("Created new system data");
    }
    if (!this->guild_card_data) {
      this->guild_card_data = make_shared<PSOBBGuildCardFile>();
      files_manager->set_guild_card(card_filename, this->guild_card_data);
      player_data_log.info_f("Created new Guild Card data");
    }

    if (!this->character_data && (this->bb_character_index >= 0)) {
      string nsc_filename = this->legacy_player_filename();
      auto nsc_data = phosg::load_object_file<LegacySavedPlayerDataBB>(nsc_filename);
      if (nsc_data.signature == LegacySavedPlayerDataBB::SIGNATURE_V0) {
        nsc_data.signature = LegacySavedPlayerDataBB::SIGNATURE_V0;
        nsc_data.unused.clear();
        nsc_data.battle_records.place_counts.clear(0);
        nsc_data.battle_records.disconnect_count = 0;
        nsc_data.battle_records.unknown_a1.clear(0);
      } else if (nsc_data.signature != LegacySavedPlayerDataBB::SIGNATURE_V1) {
        throw runtime_error("legacy player data has incorrect signature");
      }

      this->character_data = make_shared<PSOBBCharacterFile>();
      files_manager->set_character(this->character_filename(), this->character_data);
      this->character_data->inventory = nsc_data.inventory;
      this->character_data->disp = nsc_data.disp;
      this->character_data->play_time_seconds = 0;
      this->character_data->quest_flags = nsc_data.quest_flags;
      this->character_data->death_count = nsc_data.death_count;
      this->character_data->bank = nsc_data.bank;
      this->character_data->guild_card.guild_card_number = this->login->account->account_id;
      this->character_data->guild_card.name = nsc_data.disp.name;
      this->character_data->guild_card.description = nsc_data.guild_card_description;
      this->character_data->guild_card.present = 1;
      this->character_data->guild_card.language = nsc_data.inventory.language;
      this->character_data->guild_card.section_id = nsc_data.disp.visual.section_id;
      this->character_data->guild_card.char_class = nsc_data.disp.visual.char_class;
      this->character_data->auto_reply = nsc_data.auto_reply;
      this->character_data->info_board = nsc_data.info_board;
      this->character_data->battle_records = nsc_data.battle_records;
      this->character_data->challenge_records = nsc_data.challenge_records;
      this->character_data->tech_menu_shortcut_entries = nsc_data.tech_menu_shortcut_entries;
      this->character_data->quest_counters = nsc_data.quest_counters;
      if (nsa_data) {
        this->character_data->option_flags = nsa_data->option_flags;
        this->character_data->symbol_chats = nsa_data->symbol_chats;
        this->character_data->shortcuts = nsa_data->shortcuts;
        player_data_log.info_f("Loaded legacy player data from {} and {}", nsa_filename, nsc_filename);
      } else {
        player_data_log.info_f("Loaded legacy player data from {}", nsc_filename);
      }
      this->update_character_data_after_load(this->character_data);
    }
  }

  this->blocked_senders.clear();
  for (size_t z = 0; z < this->guild_card_data->blocked.size(); z++) {
    if (this->guild_card_data->blocked[z].present) {
      this->blocked_senders.emplace(this->guild_card_data->blocked[z].guild_card_number);
    }
  }

  if (this->character_data) {
    // Clear legacy play_time field
    this->character_data->disp.name.clear_after_bytes(0x18);
    this->login->account->auto_reply_message = this->character_data->auto_reply.decode();
    this->login->account->save();
    this->last_play_time_update = phosg::now();
  }
}

void Client::update_character_data_after_load(shared_ptr<PSOBBCharacterFile> charfile) {
  charfile->import_tethealla_material_usage(this->require_server_state()->level_table(this->version()));

  uint8_t lang = this->language();
  player_data_log.info_f("Overriding language fields in save files with {:02X} ({})", lang, char_for_language_code(lang));
  charfile->inventory.language = lang;
  charfile->guild_card.language = lang;
}

void Client::save_all() {
  if (this->system_data) {
    this->save_system_file();
  }
  if (this->character_data) {
    this->save_character_file();
  }
  if (this->guild_card_data) {
    this->save_guild_card_file();
  }
  if (this->external_bank) {
    string filename = this->shared_bank_filename();
    phosg::save_object_file<PlayerBank200>(filename, *this->external_bank);
    player_data_log.info_f("Saved shared bank file {}", filename);
  }
  if (this->external_bank_character) {
    this->save_character_file(
        this->character_filename(this->external_bank_character_index),
        this->system_data,
        this->external_bank_character);
  }
}

void Client::save_system_file() const {
  if (!this->system_data) {
    throw logic_error("no system file loaded");
  }
  string filename = this->system_filename();
  phosg::save_object_file(filename, *this->system_data);
  player_data_log.info_f("Saved system file {}", filename);
}

void Client::save_character_file(
    const string& filename,
    shared_ptr<const PSOBBBaseSystemFile> system,
    shared_ptr<const PSOBBCharacterFile> character) {
  PSOCHARFile::save(filename, system, character);
  player_data_log.info_f("Saved character file {}", filename);
}

void Client::save_ep3_character_file(
    const string& filename,
    const PSOGCEp3CharacterFile::Character& character) {
  phosg::save_file(filename, &character, sizeof(character));
  player_data_log.info_f("Saved Episode 3 character file {}", filename);
}

void Client::save_character_file() {
  if (!this->system_data.get()) {
    throw logic_error("no system file loaded");
  }
  if (!this->character_data.get()) {
    throw logic_error("no character file loaded");
  }
  if (this->should_update_play_time) {
    // This is slightly inaccurate, since fractions of a second are truncated
    // off each time we save. I'm lazy, so insert shrug emoji here.
    uint64_t t = phosg::now();
    uint64_t seconds = (t - this->last_play_time_update) / 1000000;
    this->character_data->play_time_seconds += seconds;
    player_data_log.info_f("Added {} seconds to play time", seconds);
    this->last_play_time_update = t;
  }

  this->save_character_file(this->character_filename(), this->system_data, this->character_data);
}

void Client::save_guild_card_file() const {
  if (!this->guild_card_data.get()) {
    throw logic_error("no Guild Card file loaded");
  }
  string filename = this->guild_card_filename();
  phosg::save_object_file(filename, *this->guild_card_data);
  player_data_log.info_f("Saved Guild Card file {}", filename);
}

void Client::load_backup_character(uint32_t account_id, size_t index) {
  string filename = this->backup_character_filename(account_id, index, false);
  this->character_data = PSOCHARFile::load_shared(filename, false).character_file;
  this->update_character_data_after_load(this->character_data);
  this->v1_v2_last_reported_disp.reset();
}

shared_ptr<PSOGCEp3CharacterFile::Character> Client::load_ep3_backup_character(uint32_t account_id, size_t index) {
  string filename = this->backup_character_filename(account_id, index, true);
  auto ch = make_shared<PSOGCEp3CharacterFile::Character>(phosg::load_object_file<PSOGCEp3CharacterFile::Character>(filename));
  this->character_data = PSOBBCharacterFile::create_from_file(*ch);
  this->ep3_config = make_shared<Episode3::PlayerConfig>(ch->ep3_config);
  this->update_character_data_after_load(this->character_data);
  this->v1_v2_last_reported_disp.reset();
  return ch;
}

void Client::save_and_unload_character() {
  if (this->character_data) {
    this->save_character_file();
    this->character_data.reset();
    this->log.info_f("Unloaded character");
  }
}

PlayerBank200& Client::current_bank() {
  if (this->external_bank) {
    return *this->external_bank;
  } else if (this->external_bank_character) {
    return this->external_bank_character->bank;
  }
  return this->character()->bank;
}

const PlayerBank200& Client::current_bank() const {
  return const_cast<Client*>(this)->current_bank();
}

std::shared_ptr<PSOBBCharacterFile> Client::current_bank_character() {
  return this->external_bank_character ? this->external_bank_character : this->character();
}

void Client::use_default_bank() {
  if (this->external_bank) {
    string filename = this->shared_bank_filename();
    phosg::save_object_file<PlayerBank200>(filename, *this->external_bank);
    this->external_bank.reset();
    player_data_log.info_f("Detached shared bank {}", filename);
  }
  if (this->external_bank_character) {
    string filename = this->character_filename(this->external_bank_character_index);
    this->save_character_file(filename, this->system_data, this->external_bank_character);
    this->external_bank_character.reset();
    player_data_log.info_f("Detached character {} from bank", filename);
  }
}

bool Client::use_shared_bank() {
  this->use_default_bank();

  string filename = this->shared_bank_filename();
  auto files_manager = this->require_server_state()->player_files_manager;
  this->external_bank = files_manager->get_bank(filename);
  if (this->external_bank) {
    player_data_log.info_f("Using loaded shared bank {}", filename);
    return true;
  } else if (std::filesystem::is_regular_file(filename)) {
    this->external_bank = make_shared<PlayerBank200>(phosg::load_object_file<PlayerBank200>(filename));
    files_manager->set_bank(filename, this->external_bank);
    player_data_log.info_f("Loaded shared bank {}", filename);
    return true;
  } else {
    this->external_bank = make_shared<PlayerBank200>();
    files_manager->set_bank(filename, this->external_bank);
    player_data_log.info_f("Created shared bank for {}", filename);
    return false;
  }
}

void Client::use_character_bank(ssize_t index) {
  this->use_default_bank();
  if (index != this->bb_character_index) {
    auto files_manager = this->require_server_state()->player_files_manager;

    string filename = this->character_filename(index);
    this->external_bank_character = files_manager->get_character(filename);
    if (this->external_bank_character) {
      this->external_bank_character_index = index;
      player_data_log.info_f("Using loaded character file {} for external bank", filename);
    } else if (std::filesystem::is_regular_file(filename)) {
      this->external_bank_character = PSOCHARFile::load_shared(filename, false).character_file;
      this->update_character_data_after_load(this->external_bank_character);
      this->external_bank_character_index = index;
      files_manager->set_character(filename, this->external_bank_character);
      player_data_log.info_f("Loaded character data from {} for external bank", filename);
    } else {
      throw runtime_error("character does not exist");
    }
  }
}

void Client::print_inventory() const {
  auto s = this->require_server_state();
  auto p = this->character();
  this->log.info_f("[PlayerInventory] Meseta: {}", p->disp.stats.meseta);
  this->log.info_f("[PlayerInventory] {} items", p->inventory.num_items);
  for (size_t x = 0; x < p->inventory.num_items; x++) {
    const auto& item = p->inventory.items[x];
    auto hex = item.data.hex();
    auto name = s->describe_item(this->version(), item.data);
    this->log.info_f("[PlayerInventory]   {:2}: [+{:08X}] {} ({})", x, item.flags, hex, name);
  }
}

void Client::print_bank() const {
  auto s = this->require_server_state();
  auto bank = this->current_bank();
  this->log.info_f("[PlayerBank] Meseta: {}", bank.meseta);
  this->log.info_f("[PlayerBank] {} items", bank.num_items);
  for (size_t x = 0; x < bank.num_items; x++) {
    const auto& item = bank.items[x];
    const char* present_token = item.present ? "" : " (missing present flag)";
    auto hex = item.data.hex();
    auto name = s->describe_item(this->version(), item.data);
    this->log.info_f("[PlayerBank]   {:3}: {} ({}) (x{}){}", x, hex, name, item.amount, present_token);
  }
}

void Client::cancel_pending_promises() {
  for (const auto& promise : this->function_call_response_queue) {
    if (!promise->done()) {
      promise->cancel();
    }
  }
  this->function_call_response_queue.clear();

  if (this->character_data_ready_promise && !this->character_data_ready_promise->done()) {
    this->character_data_ready_promise->cancel();
  }
  this->character_data_ready_promise.reset();

  if (this->enable_save_promise && !this->enable_save_promise->done()) {
    this->enable_save_promise->cancel();
  }
  this->enable_save_promise.reset();
}
