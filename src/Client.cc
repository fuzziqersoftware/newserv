#include "Client.hh"

#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "Server.hh"
#include "Version.hh"

using namespace std;

const uint64_t CLIENT_CONFIG_MAGIC = 0x8399AC32;

static atomic<uint64_t> next_id(1);

void Client::Config::set_flags_for_version(GameVersion version, int64_t sub_version) {
  this->set_flag(Flag::PROXY_CHAT_COMMANDS_ENABLED);
  this->set_flag(Flag::PROXY_CHAT_FILTER_ENABLED);

  switch (sub_version) {
    case -1: // Initial check (before sub_version recognition)
      switch (version) {
        case GameVersion::DC:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        case GameVersion::GC:
          break;
        case GameVersion::XB:
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        case GameVersion::PC:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY);
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        case GameVersion::PATCH:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::NO_SEND_FUNCTION_CALL);
          break;
        case GameVersion::BB:
          this->set_flag(Flag::NO_D6);
          this->set_flag(Flag::SAVE_ENABLED);
          this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
          break;
        default:
          throw logic_error("invalid game version");
      }
      break;

    case 0x20: // DCNTE, possibly also DCv1 JP
    case 0x21: // DCv1 US
      this->set_flag(Flag::IS_DC_V1);
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::NO_SEND_FUNCTION_CALL);
      // In the case of DCNTE, the IS_DC_TRIAL_EDITION flag is already set when
      // we get here
      break;
    case 0x23: // DCv1 EU
      this->set_flag(Flag::IS_DC_V1);
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::NO_SEND_FUNCTION_CALL);
      break;
    case 0x25: // DCv2 JP
    case 0x26: // DCv2 US
    case 0x28: // DCv2 EU
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x29: // PC
      this->set_flag(Flag::NO_D6);
      this->set_flag(Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x30: // GC Ep1&2 GameJam demo, GC Ep1&2 JP v1.02, at least one version of PSO XB
    case 0x31: // GC Ep1&2 US v1.00, GC US v1.01, GC EU v1.00, GC JP v1.00
    case 0x34: // GC Ep1&2 JP v1.03
      // In the case of GC Trial Edition, the IS_GC_TRIAL_EDITION flag is
      // already set when we get here (because the client has used V2 encryption
      // instead of V3)
      break;
    case 0x32: // GC Ep1&2 EU 50Hz
    case 0x33: // GC Ep1&2 EU 60Hz
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      break;
    case 0x35: // GC Ep1&2 JP v1.04 (Plus)
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x36: // GC Ep1&2 US v1.02 (Plus)
    case 0x39: // GC Ep1&2 JP v1.05 (Plus)
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::NO_SEND_FUNCTION_CALL);
      break;
    case 0x40: // GC Ep3 JP and Trial Edition
               // sub_version can't be used to tell JP final and Trial Edition apart; we
               // instead look at header.flag in the 61 command.
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::IS_EPISODE_3);
      this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x42: // Also GC Ep3 JP?
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::IS_EPISODE_3);
      this->set_flag(Flag::ENCRYPTED_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x41: // GC Ep3 US
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::IS_EPISODE_3);
      this->set_flag(Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL);
      this->set_flag(Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
      break;
    case 0x43: // GC Ep3 EU
      this->set_flag(Flag::NO_D6_AFTER_LOBBY);
      this->set_flag(Flag::IS_EPISODE_3);
      this->set_flag(Flag::NO_SEND_FUNCTION_CALL);
      break;
    default:
      throw runtime_error(string_printf("unknown sub_version %" PRIX64, sub_version));
  }
}

Client::Client(
    shared_ptr<Server> server,
    struct bufferevent* bev,
    GameVersion version,
    ServerBehavior server_behavior)
    : server(server),
      id(next_id++),
      log(string_printf("[C-%" PRIX64 "] ", this->id), client_log.min_level),
      channel(bev, version, 1, nullptr, nullptr, this, string_printf("C-%" PRIX64, this->id), TerminalFormat::FG_YELLOW, TerminalFormat::FG_GREEN),
      server_behavior(server_behavior),
      should_disconnect(false),
      should_send_to_lobby_server(false),
      should_send_to_proxy_server(false),
      bb_connection_phase(0xFF),
      x(0.0f),
      z(0.0f),
      area(0),
      lobby_client_id(0),
      lobby_arrow_color(0),
      preferred_lobby_id(-1),
      save_game_data_event(
          event_new(
              bufferevent_get_base(bev), -1, EV_TIMEOUT | EV_PERSIST,
              &Client::dispatch_save_game_data, this),
          event_free),
      send_ping_event(
          event_new(
              bufferevent_get_base(bev), -1, EV_TIMEOUT,
              &Client::dispatch_send_ping, this),
          event_free),
      idle_timeout_event(
          event_new(
              bufferevent_get_base(bev), -1, EV_TIMEOUT,
              &Client::dispatch_idle_timeout, this),
          event_free),
      card_battle_table_number(-1),
      card_battle_table_seat_number(0),
      card_battle_table_seat_state(0),
      next_exp_value(0),
      can_chat(true),
      pending_bb_save_player_index(0),
      dol_base_addr(0) {

  this->config.set_flags_for_version(version, -1);
  this->config.specific_version = default_specific_version_for_version(version, -1);

  this->last_switch_enabled_command.header.subcommand = 0;
  memset(&this->next_connection_addr, 0, sizeof(this->next_connection_addr));

  if (this->version() == GameVersion::BB) {
    struct timeval tv = usecs_to_timeval(60000000); // 1 minute
    event_add(this->save_game_data_event.get(), &tv);
  }
  this->reschedule_ping_and_timeout_events();

  this->log.info("Created");
}

Client::~Client() {
  if (!this->disconnect_hooks.empty()) {
    this->log.warning("Disconnect hooks pending at client destruction time:");
    for (const auto& it : this->disconnect_hooks) {
      this->log.warning("  %s", it.first.c_str());
    }
  }

  this->log.info("Deleted");
}

void Client::reschedule_ping_and_timeout_events() {
  struct timeval ping_tv = usecs_to_timeval(30000000); // 30 seconds
  event_add(this->send_ping_event.get(), &ping_tv);
  struct timeval idle_tv = usecs_to_timeval(60000000); // 1 minute
  event_add(this->idle_timeout_event.get(), &idle_tv);
}

QuestScriptVersion Client::quest_version() const {
  switch (this->version()) {
    case GameVersion::DC:
      if (this->config.check_flag(Flag::IS_DC_TRIAL_EDITION)) {
        return QuestScriptVersion::DC_NTE;
      } else if (this->config.check_flag(Flag::IS_DC_V1)) {
        return QuestScriptVersion::DC_V1;
      } else {
        return QuestScriptVersion::DC_V2;
      }
    case GameVersion::PC:
      return QuestScriptVersion::PC_V2;
    case GameVersion::GC:
      if (this->config.check_flag(Flag::IS_GC_TRIAL_EDITION)) {
        return QuestScriptVersion::GC_NTE;
      } else if (this->config.check_flag(Flag::IS_EPISODE_3)) {
        return QuestScriptVersion::GC_EP3;
      } else {
        return QuestScriptVersion::GC_V3;
      }
    case GameVersion::XB:
      return QuestScriptVersion::XB_V3;
    case GameVersion::BB:
      return QuestScriptVersion::BB_V4;
    default:
      throw logic_error("client\'s game version does not have a quest version");
  }
}

void Client::set_license(shared_ptr<License> l) {
  this->license = l;
  this->game_data.guild_card_number = this->license->serial_number;
  if (this->version() == GameVersion::BB) {
    this->game_data.bb_username = this->license->bb_username;
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

void Client::dispatch_save_game_data(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Client*>(ctx)->save_game_data();
}

void Client::save_game_data() {
  if (this->version() != GameVersion::BB) {
    throw logic_error("save_game_data called for non-BB client");
  }
  if (this->game_data.account(false)) {
    this->game_data.save_account_data();
  }
  if (this->game_data.player(false)) {
    this->game_data.save_player_data();
  }
}

void Client::dispatch_send_ping(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Client*>(ctx)->send_ping();
}

void Client::send_ping() {
  if (this->version() != GameVersion::PATCH) {
    this->log.info("Sending ping command");
    // The game doesn't use this timestamp; we only use it for debugging purposes
    be_uint64_t timestamp = now();
    try {
      this->channel.send(0x1D, 0x00, &timestamp, sizeof(be_uint64_t));
    } catch (const exception& e) {
      this->log.info("Failed to send ping: %s", e.what());
    }
  }
}

void Client::dispatch_idle_timeout(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Client*>(ctx)->idle_timeout();
}

void Client::idle_timeout() {
  this->log.info("Idle timeout expired");
  auto s = this->server.lock();
  if (s) {
    auto c = this->shared_from_this();
    s->disconnect_client(c);
  } else {
    this->log.info("Server is deleted; cannot disconnect client");
  }
}
