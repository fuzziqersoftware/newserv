#pragma once

#include <netinet/in.h>

#include <memory>
#include <stdexcept>

#include "Channel.hh"
#include "CommandFormats.hh"
#include "Episode3/BattleRecord.hh"
#include "Episode3/Tournament.hh"
#include "FileContentsCache.hh"
#include "FunctionCompiler.hh"
#include "License.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "PatchFileIndex.hh"
#include "Player.hh"
#include "Quest.hh"
#include "QuestScript.hh"
#include "TeamIndex.hh"
#include "Text.hh"

extern const uint64_t CLIENT_CONFIG_MAGIC;

class Server;
struct Lobby;

struct Client : public std::enable_shared_from_this<Client> {
  enum class Flag : uint64_t {
    // clang-format off

    // Version-related flags
    CHECKED_FOR_DC_V1_PROTOTYPE         = 0x0000000000000002,
    NO_D6_AFTER_LOBBY                   = 0x0000000000000100,
    NO_D6                               = 0x0000000000000200,
    FORCE_ENGLISH_LANGUAGE_BB           = 0x0000000000000400,

    // Flags describing the behavior for send_function_call
    NO_SEND_FUNCTION_CALL               = 0x0000000000001000,
    ENCRYPTED_SEND_FUNCTION_CALL        = 0x0000000000002000,
    SEND_FUNCTION_CALL_CHECKSUM_ONLY    = 0x0000000000004000,
    SEND_FUNCTION_CALL_NO_CACHE_PATCH   = 0x0000000000008000,
    USE_OVERFLOW_FOR_SEND_FUNCTION_CALL = 0x0000000000010000,

    // State flags
    LOADING                             = 0x0000000000100000,
    LOADING_QUEST                       = 0x0000000000200000,
    LOADING_RUNNING_JOINABLE_QUEST      = 0x0000000000400000,
    LOADING_TOURNAMENT                  = 0x0000000000800000,
    IN_INFORMATION_MENU                 = 0x0000000001000000,
    AT_WELCOME_MESSAGE                  = 0x0000000002000000,
    SAVE_ENABLED                        = 0x0000000004000000,
    HAS_EP3_CARD_DEFS                   = 0x0000000008000000,
    HAS_EP3_MEDIA_UPDATES               = 0x0000000010000000,
    USE_OVERRIDE_RANDOM_SEED            = 0x0000000020000000,
    HAS_GUILD_CARD_NUMBER               = 0x0000000040000000,
    AT_BANK_COUNTER                     = 0x0000000080000000,

    // Cheat mode flags
    SWITCH_ASSIST_ENABLED               = 0x0000000100000000,
    INFINITE_HP_ENABLED                 = 0x0000000200000000,
    INFINITE_TP_ENABLED                 = 0x0000000400000000,
    DEBUG_ENABLED                       = 0x0000000800000000,

    // Proxy option flags
    PROXY_SAVE_FILES                    = 0x0000001000000000,
    PROXY_CHAT_COMMANDS_ENABLED         = 0x0000002000000000,
    PROXY_CHAT_FILTER_ENABLED           = 0x0000004000000000,
    PROXY_PLAYER_NOTIFICATIONS_ENABLED  = 0x0000008000000000,
    PROXY_SUPPRESS_CLIENT_PINGS         = 0x0000010000000000,
    PROXY_SUPPRESS_REMOTE_LOGIN         = 0x0000020000000000,
    PROXY_ZERO_REMOTE_GUILD_CARD        = 0x0000040000000000,
    PROXY_EP3_INFINITE_MESETA_ENABLED   = 0x0000080000000000,
    PROXY_EP3_INFINITE_TIME_ENABLED     = 0x0000100000000000,
    PROXY_RED_NAME_ENABLED              = 0x0000200000000000,
    PROXY_BLANK_NAME_ENABLED            = 0x0000400000000000,
    PROXY_BLOCK_FUNCTION_CALLS          = 0x0000800000000000,
    // clang-format on
  };

  struct Config {
    uint64_t enabled_flags = 0; // Client::Flag enum
    uint32_t specific_version = 0;
    int32_t override_random_seed = 0;
    uint8_t override_section_id = 0xFF; // FF = no override
    uint8_t override_lobby_event = 0xFF; // FF = no override
    uint8_t override_lobby_number = 0x80; // 80 = no override
    uint32_t proxy_destination_address = 0;
    uint16_t proxy_destination_port = 0;

    Config() = default;

    [[nodiscard]] static inline bool check_flag(uint64_t enabled_flags, Flag flag) {
      return !!(enabled_flags & static_cast<uint64_t>(flag));
    }

    [[nodiscard]] inline bool check_flag(Flag flag) const {
      return this->check_flag(this->enabled_flags, flag);
    }
    inline void set_flag(Flag flag) {
      this->enabled_flags |= static_cast<uint64_t>(flag);
    }
    inline void clear_flag(Flag flag) {
      this->enabled_flags &= (~static_cast<uint64_t>(flag));
    }
    inline void toggle_flag(Flag flag) {
      this->enabled_flags ^= static_cast<uint64_t>(flag);
    }

    void set_flags_for_version(Version version, int64_t sub_version);

    template <size_t Bytes>
    void parse_from(const parray<uint8_t, Bytes>& data) {
      StringReader r(data.data(), data.size());
      if (r.get_u32l() != CLIENT_CONFIG_MAGIC) {
        throw std::invalid_argument("config signature is incorrect");
      }
      this->specific_version = r.get_u32l();
      this->enabled_flags = r.get_u64l();
      this->override_random_seed = r.get_u32l();
      this->proxy_destination_address = r.get_u32b();
      this->proxy_destination_port = r.get_u16l();
      this->override_section_id = r.get_u8();
      this->override_lobby_event = r.get_u8();
      this->override_lobby_number = r.get_u8();
    }

    template <size_t Bytes>
    void serialize_into(parray<uint8_t, Bytes>& data) const {
      StringWriter w;
      w.put_u32l(CLIENT_CONFIG_MAGIC);
      w.put_u32l(this->specific_version);
      w.put_u64l(this->enabled_flags);
      w.put_u32l(this->override_random_seed);
      w.put_u32b(this->proxy_destination_address);
      w.put_u16l(this->proxy_destination_port);
      w.put_u8(this->override_section_id);
      w.put_u8(this->override_lobby_event);
      w.put_u8(this->override_lobby_number);

      const auto& s = w.str();
      for (size_t z = 0; z < s.size(); z++) {
        data[z] = s[z];
      }
      data.clear_after(s.size(), 0xFF);
    }
  };

  std::weak_ptr<Server> server;
  std::weak_ptr<ServerState> server_state;
  uint64_t id;
  PrefixedLogger log;

  // License & account
  std::shared_ptr<License> license;

  // Network
  Channel channel;
  struct sockaddr_storage next_connection_addr;
  ServerBehavior server_behavior;
  bool should_disconnect;
  bool should_send_to_lobby_server;
  bool should_send_to_proxy_server;
  std::unordered_map<std::string, std::function<void()>> disconnect_hooks;
  std::shared_ptr<XBNetworkLocation> xb_netloc;
  parray<le_uint32_t, 3> xb_9E_unknown_a1a;
  uint8_t bb_connection_phase;
  uint64_t ping_start_time;

  // Patch server
  std::vector<PatchFileChecksumRequest> patch_file_checksum_requests;

  // Lobby/positioning
  Config config;
  int32_t sub_version;
  float x;
  float z;
  uint32_t floor;
  std::weak_ptr<Lobby> lobby;
  uint8_t lobby_client_id;
  uint8_t lobby_arrow_color;
  int64_t preferred_lobby_id; // <0 = no preference
  ClientGameData game_data;
  std::unique_ptr<struct event, void (*)(struct event*)> save_game_data_event;
  std::unique_ptr<struct event, void (*)(struct event*)> send_ping_event;
  std::unique_ptr<struct event, void (*)(struct event*)> idle_timeout_event;
  int16_t card_battle_table_number;
  uint16_t card_battle_table_seat_number;
  uint16_t card_battle_table_seat_state;
  std::weak_ptr<Episode3::Tournament::Team> ep3_tournament_team;
  std::shared_ptr<Episode3::BattleRecord> ep3_prev_battle_record;
  std::shared_ptr<const Menu> last_menu_sent;
  struct JoinCommand {
    uint16_t command;
    uint32_t flag;
    std::string data;
  };
  std::unique_ptr<std::deque<JoinCommand>> game_join_command_queue;

  // Miscellaneous (used by chat commands)
  uint32_t next_exp_value; // next EXP value to give
  G_SwitchStateChanged_6x05 last_switch_enabled_command;
  bool can_chat;
  std::shared_ptr<License> pending_bb_save_license;
  uint8_t pending_bb_save_character_index;
  std::deque<std::function<void(uint32_t, uint32_t)>> function_call_response_queue;

  // File loading state
  uint32_t dol_base_addr;
  std::shared_ptr<DOLFileIndex::File> loading_dol_file;
  std::unordered_map<std::string, std::shared_ptr<const std::string>> sending_files;

  Client(
      std::shared_ptr<Server> server,
      struct bufferevent* bev,
      Version version,
      ServerBehavior server_behavior);
  ~Client();

  void reschedule_save_game_data_event();
  void reschedule_ping_and_timeout_events();

  inline Version version() const {
    return this->channel.version;
  }
  inline uint8_t language() const {
    return this->channel.language;
  }

  void set_license(std::shared_ptr<License> l);

  std::shared_ptr<ServerState> require_server_state() const;
  std::shared_ptr<Lobby> require_lobby() const;

  std::shared_ptr<const TeamIndex::Team> team() const;

  bool can_access_quest(std::shared_ptr<const Quest> q, uint8_t difficulty) const;

  static void dispatch_save_game_data(evutil_socket_t, short, void* ctx);
  void save_game_data();
  static void dispatch_send_ping(evutil_socket_t, short, void* ctx);
  void send_ping();
  static void dispatch_idle_timeout(evutil_socket_t, short, void* ctx);
  void idle_timeout();

  void suspend_timeouts();
};
