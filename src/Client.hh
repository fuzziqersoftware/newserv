#pragma once

#include <netinet/in.h>

#include <memory>
#include <stdexcept>

#include "Account.hh"
#include "Channel.hh"
#include "CommandFormats.hh"
#include "Episode3/BattleRecord.hh"
#include "Episode3/Tournament.hh"
#include "FileContentsCache.hh"
#include "FunctionCompiler.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "PatchFileIndex.hh"
#include "Quest.hh"
#include "QuestScript.hh"
#include "TeamIndex.hh"
#include "Text.hh"

extern const uint64_t CLIENT_CONFIG_MAGIC;

class Server;
struct Lobby;
class Parsed6x70Data;

class Client : public std::enable_shared_from_this<Client> {
public:
  enum class Flag : uint64_t {
    // clang-format off

    // This mask specifies which flags are sent to the client
    // TODO: It'd be nice to use a pattern here (e.g. all server-side flags are
    // in the high bits) but that would require re-recording or manually
    // rewriting all the tests
    CLIENT_SIDE_MASK                           = 0xE73CFFFF7C0BFFFB,

    // Version-related flags
    CHECKED_FOR_DC_V1_PROTOTYPE                = 0x0000000000000002,
    NO_D6_AFTER_LOBBY                          = 0x0000000000000100,
    NO_D6                                      = 0x0000000000000200,
    FORCE_ENGLISH_LANGUAGE_BB                  = 0x0000000000000400,

    // Flags describing the behavior for send_function_call
    HAS_SEND_FUNCTION_CALL                     = 0x0000000000001000,
    ENCRYPTED_SEND_FUNCTION_CALL               = 0x0000000000002000,
    SEND_FUNCTION_CALL_CHECKSUM_ONLY           = 0x0000000000004000,
    SEND_FUNCTION_CALL_NO_CACHE_PATCH          = 0x0000000000008000,
    CAN_RECEIVE_ENABLE_B2_QUEST                = 0x0000000000020000,
    AWAITING_ENABLE_B2_QUEST                   = 0x0000000000040000, // Server-side only

    // State flags
    LOADING                                    = 0x0000000000100000, // Server-side only
    LOADING_QUEST                              = 0x0000000000200000, // Server-side only
    LOADING_RUNNING_JOINABLE_QUEST             = 0x0000000000400000, // Server-side only
    LOADING_TOURNAMENT                         = 0x0000000000800000, // Server-side only
    IN_INFORMATION_MENU                        = 0x0000000001000000, // Server-side only
    AT_WELCOME_MESSAGE                         = 0x0000000002000000, // Server-side only
    SAVE_ENABLED                               = 0x0000000004000000,
    HAS_EP3_CARD_DEFS                          = 0x0000000008000000,
    HAS_EP3_MEDIA_UPDATES                      = 0x0000000010000000,
    USE_OVERRIDE_RANDOM_SEED                   = 0x0000000020000000,
    HAS_GUILD_CARD_NUMBER                      = 0x0000000040000000,
    HAS_AUTO_PATCHES                           = 0x0000004000000000,
    AT_BANK_COUNTER                            = 0x0000000080000000, // Server-side only
    SHOULD_SEND_ARTIFICIAL_ITEM_STATE          = 0x0001000000000000, // Server-side only
    SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE = 0x0040000000000000, // Server-side only
    SHOULD_SEND_ARTIFICIAL_OBJECT_STATE        = 0x0080000000000000, // Server-side only
    SHOULD_SEND_ARTIFICIAL_FLAG_STATE          = 0x0002000000000000, // Server-side only
    SHOULD_SEND_ARTIFICIAL_PLAYER_STATES       = 0x0200000000000000, // Server-side only
    SHOULD_SEND_ENABLE_SAVE                    = 0x0004000000000000,
    SWITCH_ASSIST_ENABLED                      = 0x0000000100000000,
    IS_CLIENT_CUSTOMIZATION                    = 0x0100000000000000,
    EP3_ALLOW_6xBC                             = 0x1000000000000000, // Server-side only

    // Cheat mode and option flags
    INFINITE_HP_ENABLED                        = 0x0000000200000000,
    INFINITE_TP_ENABLED                        = 0x0000000400000000,
    DEBUG_ENABLED                              = 0x0000000800000000,
    ITEM_DROP_NOTIFICATIONS_1                  = 0x0010000000000000,
    ITEM_DROP_NOTIFICATIONS_2                  = 0x0020000000000000,
    FORCE_BATTLE_MODE_GAME                     = 0x0800000000000000, // Server-side only

    // Proxy option flags
    PROXY_SAVE_FILES                           = 0x0000001000000000,
    PROXY_CHAT_COMMANDS_ENABLED                = 0x0000002000000000,
    PROXY_PLAYER_NOTIFICATIONS_ENABLED         = 0x0000008000000000,
    PROXY_SUPPRESS_CLIENT_PINGS                = 0x0000010000000000,
    PROXY_SUPPRESS_REMOTE_LOGIN                = 0x0000020000000000,
    PROXY_ZERO_REMOTE_GUILD_CARD               = 0x0000040000000000,
    PROXY_EP3_INFINITE_MESETA_ENABLED          = 0x0000080000000000,
    PROXY_EP3_INFINITE_TIME_ENABLED            = 0x0000100000000000,
    PROXY_RED_NAME_ENABLED                     = 0x0000200000000000,
    PROXY_BLANK_NAME_ENABLED                   = 0x0000400000000000,
    PROXY_BLOCK_FUNCTION_CALLS                 = 0x0000800000000000,
    PROXY_EP3_UNMASK_WHISPERS                  = 0x0008000000000000,
    PROXY_VIRTUAL_CLIENT                       = 0x0400000000000000,
    // clang-format on
  };
  enum class ItemDropNotificationMode {
    NOTHING = 0,
    RARES_ONLY = 1,
    ALL_ITEMS = 2,
    ALL_ITEMS_INCLUDING_MESETA = 3,
  };

  static constexpr uint64_t DEFAULT_FLAGS = static_cast<uint64_t>(Flag::PROXY_CHAT_COMMANDS_ENABLED);

  struct Config {
    uint64_t enabled_flags = DEFAULT_FLAGS; // Client::Flag enum
    uint32_t specific_version = 0;
    int32_t override_random_seed = 0;
    uint8_t override_section_id = 0xFF; // FF = no override
    uint8_t override_lobby_event = 0xFF; // FF = no override
    uint8_t override_lobby_number = 0x80; // 80 = no override
    uint32_t proxy_destination_address = 0;
    uint16_t proxy_destination_port = 0;

    Config() = default;

    bool operator==(const Config& other) const = default;
    bool operator!=(const Config& other) const = default;

    bool should_update_vs(const Config& other) const;

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

    ItemDropNotificationMode get_drop_notification_mode() const;
    void set_drop_notification_mode(ItemDropNotificationMode new_mode);

    template <size_t Bytes>
    void parse_from(const parray<uint8_t, Bytes>& data) {
      phosg::StringReader r(data.data(), data.size());
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
      phosg::StringWriter w;
      w.put_u32l(CLIENT_CONFIG_MAGIC);
      w.put_u32l(this->specific_version);
      w.put_u64l(this->enabled_flags & static_cast<uint64_t>(Flag::CLIENT_SIDE_MASK));
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
  uint64_t id;
  phosg::PrefixedLogger log;

  std::shared_ptr<Login> login;

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

  // Lobby/positioning
  Config config;
  Config synced_config;
  std::unique_ptr<Variations> override_variations;
  int32_t sub_version;
  VectorXZF pos;
  uint32_t floor;
  std::weak_ptr<Lobby> lobby;
  uint8_t lobby_client_id;
  uint8_t lobby_arrow_color;
  int64_t preferred_lobby_id; // <0 = no preference

  std::unique_ptr<struct event, void (*)(struct event*)> save_game_data_event;
  std::unique_ptr<struct event, void (*)(struct event*)> send_ping_event;
  std::unique_ptr<struct event, void (*)(struct event*)> idle_timeout_event;
  int16_t card_battle_table_number;
  uint16_t card_battle_table_seat_number;
  uint16_t card_battle_table_seat_state;
  std::weak_ptr<Episode3::Tournament::Team> ep3_tournament_team;
  std::shared_ptr<const Episode3::BattleRecord> ep3_prev_battle_record;
  std::shared_ptr<const Menu> last_menu_sent;
  uint32_t last_game_info_requested;
  struct JoinCommand {
    uint16_t command;
    uint32_t flag;
    std::string data;
  };
  std::unique_ptr<std::deque<JoinCommand>> game_join_command_queue;

  // Character / game data
  struct PendingItemTrade {
    uint8_t other_client_id;
    bool confirmed; // true if client has sent a D2 command
    std::vector<ItemData> items;
  };
  struct PendingCardTrade {
    uint8_t other_client_id;
    bool confirmed; // true if client has sent an EE D2 command
    std::vector<std::pair<uint32_t, uint32_t>> card_to_count;
  };
  bool should_update_play_time;
  std::unordered_set<uint32_t> blocked_senders;
  std::unique_ptr<PlayerDispDataDCPCV3> v1_v2_last_reported_disp;
  std::shared_ptr<Parsed6x70Data> last_reported_6x70;
  // These are null unless the client is within the trade sequence (D0-D4 or EE commands)
  std::unique_ptr<PendingItemTrade> pending_item_trade;
  std::unique_ptr<PendingCardTrade> pending_card_trade;
  uint32_t telepipe_lobby_id;
  G_SetTelepipeState_6x68 telepipe_state;
  std::shared_ptr<Episode3::PlayerConfig> ep3_config; // Null for non-Ep3
  int8_t bb_character_index;
  ItemData bb_identify_result;
  std::array<std::vector<ItemData>, 3> bb_shop_contents;

  // Miscellaneous (used by chat commands)
  uint32_t next_exp_value; // next EXP value to give
  bool can_chat;
  struct PendingCharacterExport {
    std::shared_ptr<const Account> dest_account;
    ssize_t character_index = -1;
    std::shared_ptr<const BBLicense> dest_bb_license; // Only used for $bbchar; null for $savechar
  };
  std::unique_ptr<PendingCharacterExport> pending_character_export;
  std::deque<std::function<void(uint32_t, uint32_t)>> function_call_response_queue;

  // File loading state
  uint32_t dol_base_addr;
  std::shared_ptr<DOLFileIndex::File> loading_dol_file;
  std::unordered_map<std::string, std::shared_ptr<const std::string>> sending_files;

  Client(
      std::shared_ptr<Server> server,
      struct bufferevent* bev,
      uint64_t virtual_network_id,
      Version version,
      ServerBehavior server_behavior);
  ~Client();

  void update_channel_name();

  void reschedule_save_game_data_event();
  void reschedule_ping_and_timeout_events();

  inline Version version() const {
    return this->channel.version;
  }
  inline uint8_t language() const {
    return this->channel.language;
  }

  void convert_account_to_temporary_if_nte();

  void sync_config();

  std::shared_ptr<ServerState> require_server_state() const;
  std::shared_ptr<Lobby> require_lobby() const;

  std::shared_ptr<const TeamIndex::Team> team() const;

  bool evaluate_quest_availability_expression(
      std::shared_ptr<const IntegralExpression> expr,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      uint8_t difficulty,
      size_t num_players,
      bool v1_present) const;
  bool can_see_quest(
      std::shared_ptr<const Quest> q,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      uint8_t difficulty,
      size_t num_players,
      bool v1_present) const;
  bool can_play_quest(
      std::shared_ptr<const Quest> q,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      uint8_t difficulty,
      size_t num_players,
      bool v1_present) const;

  bool can_use_chat_commands() const;

  static void dispatch_save_game_data(evutil_socket_t, short, void* ctx);
  void save_game_data();
  static void dispatch_send_ping(evutil_socket_t, short, void* ctx);
  void send_ping();
  static void dispatch_idle_timeout(evutil_socket_t, short, void* ctx);
  void idle_timeout();

  void suspend_timeouts();

  void set_login(std::shared_ptr<Login> login);

  void create_battle_overlay(std::shared_ptr<const BattleRules> rules, std::shared_ptr<const LevelTable> level_table);
  void create_challenge_overlay(Version version, size_t template_index, std::shared_ptr<const LevelTable> level_table);
  inline void delete_overlay() {
    this->overlay_character_data.reset();
  }
  inline bool has_overlay() const {
    return this->overlay_character_data.get() != nullptr;
  }

  void import_blocked_senders(const parray<le_uint32_t, 30>& blocked_senders);

  std::shared_ptr<PSOBBBaseSystemFile> system_file(bool allow_load = true);
  std::shared_ptr<PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true);
  std::shared_ptr<PSOBBGuildCardFile> guild_card_file(bool allow_load = true);
  std::shared_ptr<const PSOBBBaseSystemFile> system_file(bool allow_load = true) const;
  std::shared_ptr<const PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true) const;
  std::shared_ptr<const PSOBBGuildCardFile> guild_card_file(bool allow_load = true) const;

  void create_character_file(
      uint32_t guild_card_number,
      uint8_t language,
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);

  std::string system_filename() const;
  static std::string character_filename(const std::string& bb_username, int8_t index);
  static std::string backup_character_filename(uint32_t account_id, size_t index, bool is_ep3);
  std::string character_filename(int8_t index = -1) const;
  std::string guild_card_filename() const;
  std::string shared_bank_filename() const;

  std::string legacy_player_filename() const;
  std::string legacy_account_filename() const;

  void save_all();
  void save_system_file() const;
  static void save_character_file(
      const std::string& filename,
      std::shared_ptr<const PSOBBBaseSystemFile> sys,
      std::shared_ptr<const PSOBBCharacterFile> character);
  static void save_ep3_character_file(
      const std::string& filename,
      const PSOGCEp3CharacterFile::Character& character);
  // Note: This function is not const because it updates the player's play time.
  void save_character_file();
  void save_guild_card_file() const;

  void load_backup_character(uint32_t account_id, size_t index);
  std::shared_ptr<PSOGCEp3CharacterFile::Character> load_ep3_backup_character(uint32_t account_id, size_t index);
  void save_and_unload_character();

  PlayerBank200& current_bank();
  const PlayerBank200& current_bank() const;
  std::shared_ptr<PSOBBCharacterFile> current_bank_character();
  bool use_shared_bank(); // Returns true if the bank exists; false if it was created
  void use_character_bank(int8_t bb_character_index);
  void use_default_bank();

  void print_inventory(FILE* stream) const;
  void print_bank(FILE* stream) const;

private:
  // The overlay character data is used in battle and challenge modes, when
  // character data is temporarily replaced in-game. In other play modes and in
  // lobbies, overlay_character_data is null.
  std::shared_ptr<PSOBBBaseSystemFile> system_data;
  std::shared_ptr<PSOBBCharacterFile> overlay_character_data;
  std::shared_ptr<PSOBBCharacterFile> character_data;
  std::shared_ptr<PSOBBGuildCardFile> guild_card_data;
  std::shared_ptr<PlayerBank200> external_bank;
  std::shared_ptr<PSOBBCharacterFile> external_bank_character;
  int8_t external_bank_character_index;
  uint64_t last_play_time_update;

  void save_and_clear_external_bank();

  void load_all_files();
  void update_character_data_after_load(std::shared_ptr<PSOBBCharacterFile> character_data);
};
