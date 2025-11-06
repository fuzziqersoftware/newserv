#pragma once

#include <memory>
#include <stdexcept>

#include "Account.hh"
#include "AsyncUtils.hh"
#include "Channel.hh"
#include "CommandFormats.hh"
#include "Episode3/BattleRecord.hh"
#include "Episode3/Tournament.hh"
#include "FileContentsCache.hh"
#include "FunctionCompiler.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "PatchFileIndex.hh"
#include "ProxySession.hh"
#include "Quest.hh"
#include "QuestScript.hh"
#include "TeamIndex.hh"
#include "Text.hh"

extern const uint64_t CLIENT_CONFIG_MAGIC;

class GameServer;
struct Lobby;
class Parsed6x70Data;

struct GetPlayerInfoResult {
  // Exactly one of the following two shared_ptrs is not null
  std::shared_ptr<PSOBBCharacterFile> character;
  std::shared_ptr<PSOGCEp3CharacterFile::Character> ep3_character;
  bool is_full_info; // True if the client sent 30; false if it was 61 or 98
};

class Client : public std::enable_shared_from_this<Client> {
public:
  enum class Flag : uint64_t {
    // clang-format off

    // Version-related flags
    CHECKED_FOR_DC_V1_PROTOTYPE                = 0x0000000000000002,
    NO_D6_AFTER_LOBBY                          = 0x0000000000000100,
    NO_D6                                      = 0x0000000000000200,
    FORCE_ENGLISH_LANGUAGE_BB                  = 0x0000000000000400,

    // Flags describing the behavior for send_function_call
    HAS_SEND_FUNCTION_CALL                     = 0x0000000000001000,
    ENCRYPTED_SEND_FUNCTION_CALL               = 0x0000000000002000,
    SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE      = 0x0000000000004000,
    SEND_FUNCTION_CALL_NO_CACHE_PATCH          = 0x0000000000008000,
    CAN_RECEIVE_ENABLE_B2_QUEST                = 0x0000000000020000,
    AWAITING_ENABLE_B2_QUEST                   = 0x0000000000040000,

    // State flags
    LOADING                                    = 0x0000000000100000,
    LOADING_QUEST                              = 0x0000000000200000,
    LOADING_RUNNING_JOINABLE_QUEST             = 0x0000000000400000,
    LOADING_TOURNAMENT                         = 0x0000000000800000,
    IN_INFORMATION_MENU                        = 0x0000000001000000,
    AT_WELCOME_MESSAGE                         = 0x0000000002000000,
    SAVE_ENABLED                               = 0x0000000004000000,
    HAS_EP3_CARD_DEFS                          = 0x0000000008000000,
    HAS_EP3_MEDIA_UPDATES                      = 0x0000000010000000,
    HAS_AUTO_PATCHES                           = 0x0000004000000000,
    AT_BANK_COUNTER                            = 0x0000000080000000,
    SHOULD_SEND_ARTIFICIAL_ITEM_STATE          = 0x0001000000000000,
    SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE = 0x0040000000000000,
    SHOULD_SEND_ARTIFICIAL_OBJECT_STATE        = 0x0080000000000000,
    SHOULD_SEND_ARTIFICIAL_FLAG_STATE          = 0x0002000000000000,
    SHOULD_SEND_ARTIFICIAL_PLAYER_STATES       = 0x0200000000000000,
    SHOULD_SEND_ENABLE_SAVE                    = 0x0004000000000000,
    SWITCH_ASSIST_ENABLED                      = 0x0000000100000000,
    IS_CLIENT_CUSTOMIZATION                    = 0x0100000000000000,
    EP3_ALLOW_6xBC                             = 0x1000000000000000,

    // Cheat mode and option flags
    INFINITE_HP_ENABLED                        = 0x0000000200000000,
    INFINITE_TP_ENABLED                        = 0x0000000400000000,
    DEBUG_ENABLED                              = 0x0000000800000000,
    ITEM_DROP_NOTIFICATIONS_1                  = 0x0010000000000000,
    ITEM_DROP_NOTIFICATIONS_2                  = 0x0020000000000000,
    HAS_ENEMY_DAMAGE_SYNC_PATCH                = 0x2000000000000000, // Must be same as in EnemyDamageSync*.s

    // Proxy option flags
    PROXY_SAVE_FILES                           = 0x0000001000000000,
    PROXY_CHAT_COMMANDS_ENABLED                = 0x0000002000000000,
    PROXY_PLAYER_NOTIFICATIONS_ENABLED         = 0x0000008000000000,
    PROXY_EP3_INFINITE_MESETA_ENABLED          = 0x0000080000000000,
    PROXY_EP3_INFINITE_TIME_ENABLED            = 0x0000100000000000,
    PROXY_BLOCK_FUNCTION_CALLS                 = 0x0000800000000000,
    PROXY_EP3_UNMASK_WHISPERS                  = 0x0008000000000000,
    // clang-format on
  };
  enum class ItemDropNotificationMode {
    NOTHING = 0,
    RARES_ONLY = 1,
    ALL_ITEMS = 2,
    ALL_ITEMS_INCLUDING_MESETA = 3,
  };

  static constexpr uint64_t DEFAULT_FLAGS = static_cast<uint64_t>(Flag::PROXY_CHAT_COMMANDS_ENABLED);

  std::weak_ptr<GameServer> server;
  uint64_t id;
  phosg::PrefixedLogger log;

  // Account information (not all of these are used; depends on game version)
  std::string username;
  std::string password;
  std::string email_address;
  uint64_t hardware_id = 0;
  int32_t sub_version = 0;
  uint8_t bb_client_code = 0;
  uint8_t bb_connection_phase = 0xFF;
  ssize_t bb_character_index = -1; // -1 = not set
  ssize_t bb_bank_character_index = -1; // -1 = shared bank
  uint32_t bb_security_token = 0;
  parray<uint8_t, 0x28> bb_client_config;
  std::string login_character_name;
  std::string serial_number;
  std::string access_key;
  std::string serial_number2;
  std::string access_key2;
  std::string v1_serial_number;
  std::string v1_access_key;
  XBNetworkLocation xb_netloc;
  parray<le_uint32_t, 3> xb_unknown_a1a;
  uint64_t xb_user_id = 0;
  uint32_t xb_unknown_a1b = 0;
  std::shared_ptr<Login> login;
  std::shared_ptr<ProxySession> proxy_session;

  // Patch server state (only used for PC_PATCH and BB_PATCH versions)
  std::vector<PatchFileChecksumRequest> patch_file_checksum_requests;

  // Network
  std::shared_ptr<Channel> channel;
  std::shared_ptr<PSOBBMultiKeyDetectorEncryption> bb_detector_crypt;
  ServerBehavior server_behavior;
  std::unordered_map<std::string, std::function<void()>> disconnect_hooks;
  uint64_t ping_start_time = 0;

  // Basic state
  uint64_t enabled_flags = DEFAULT_FLAGS; // Client::Flag enum
  uint32_t specific_version = 0;
  uint8_t override_section_id = 0xFF; // FF = no override
  uint8_t override_lobby_event = 0xFF; // FF = no override
  uint8_t override_lobby_number = 0x80; // 80 = no override
  int64_t override_random_seed = -1;
  std::unique_ptr<Variations> override_variations;
  VectorXYZF pos;
  uint32_t floor = 0x0F;
  std::weak_ptr<Lobby> lobby;
  uint8_t lobby_client_id = 0;
  uint8_t lobby_arrow_color = 0;
  int64_t preferred_lobby_id = -1; // <0 = no preference

  asio::steady_timer save_game_data_timer;
  asio::steady_timer send_ping_timer;
  asio::steady_timer idle_timeout_timer;
  int16_t card_battle_table_number = -1;
  uint16_t card_battle_table_seat_number = 0;
  uint16_t card_battle_table_seat_state = 0;
  std::weak_ptr<Episode3::Tournament::Team> ep3_tournament_team;
  std::shared_ptr<const Episode3::BattleRecord> ep3_prev_battle_record;
  std::shared_ptr<const Menu> last_menu_sent;
  uint32_t last_game_info_requested = 0;
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
  // These are null unless the client is within the trade sequence (D0-D4 or EE
  // commands)
  std::unique_ptr<PendingItemTrade> pending_item_trade;
  std::unique_ptr<PendingCardTrade> pending_card_trade;
  uint32_t telepipe_lobby_id = 0;
  TelepipeState telepipe_state;
  std::shared_ptr<Episode3::PlayerConfig> ep3_config; // Null for non-Ep3
  ItemData bb_identify_result;
  std::array<std::vector<ItemData>, 3> bb_shop_contents;

  // Miscellaneous (used by chat commands)
  uint32_t next_exp_value = 0; // next EXP value to give
  bool can_chat = true;
  // NOTE: If you add any new optional promises here, make sure to also add
  // them to cancel_pending_promises.
  // NOTE: Entries in this queue can be nullptr; that represents a B2 command
  // sent by the remote server during a proxy session. We can't just omit those
  // from the queue entirely, because if we did, we could end up sending the
  // wrong B3 response back.
  std::deque<std::shared_ptr<AsyncPromise<C_ExecuteCodeResult_B3>>> function_call_response_queue;
  std::shared_ptr<AsyncPromise<GetPlayerInfoResult>> character_data_ready_promise;
  std::shared_ptr<AsyncPromise<void>> enable_save_promise;

  // File loading state
  std::unordered_map<std::string, std::shared_ptr<const std::string>> sending_files;

  Client(
      std::shared_ptr<GameServer> server,
      std::shared_ptr<Channel> channel,
      ServerBehavior server_behavior);
  ~Client();

  void update_channel_name();

  void reschedule_save_game_data_timer();
  void reschedule_ping_and_timeout_timers();

  inline Version version() const {
    return this->channel->version;
  }
  inline Language language() const {
    return this->channel->language;
  }

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

  void convert_account_to_temporary_if_nte();

  void sync_config();

  std::shared_ptr<ServerState> require_server_state() const;
  std::shared_ptr<Lobby> require_lobby() const;

  std::shared_ptr<const TeamIndex::Team> team() const;

  bool evaluate_quest_availability_expression(
      std::shared_ptr<const IntegralExpression> expr,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      Difficulty difficulty,
      size_t num_players,
      bool v1_present) const;
  bool can_see_quest(
      std::shared_ptr<const Quest> q,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      Difficulty difficulty,
      size_t num_players,
      bool v1_present) const;
  bool can_play_quest(
      std::shared_ptr<const Quest> q,
      std::shared_ptr<const Lobby> game,
      uint8_t event,
      Difficulty difficulty,
      size_t num_players,
      bool v1_present) const;

  bool can_use_chat_commands() const;

  void set_login(std::shared_ptr<Login> login);

  void import_blocked_senders(const parray<le_uint32_t, 30>& blocked_senders);

  static std::string system_filename(const std::string& bb_username);
  std::string system_filename() const;
  std::shared_ptr<PSOBBBaseSystemFile> system_file(bool allow_load = true);
  std::shared_ptr<const PSOBBBaseSystemFile> system_file(bool throw_if_missing = true) const;
  void save_system_file() const;

  static std::string guild_card_filename(const std::string& bb_username);
  std::string guild_card_filename() const;
  std::shared_ptr<PSOBBGuildCardFile> guild_card_file(bool allow_load = true);
  std::shared_ptr<const PSOBBGuildCardFile> guild_card_file(bool allow_load = true) const;
  void save_guild_card_file() const;

  static std::string character_filename(const std::string& bb_username, ssize_t index);
  static std::string backup_character_filename(uint32_t account_id, size_t index, bool is_ep3);
  std::string character_filename() const;
  std::shared_ptr<PSOBBCharacterFile> character_file(bool allow_load = true, bool allow_overlay = true);
  std::shared_ptr<const PSOBBCharacterFile> character_file(bool throw_if_missing = true, bool allow_overlay = true) const;
  static void save_character_file(
      const std::string& filename,
      std::shared_ptr<const PSOBBBaseSystemFile> sys,
      std::shared_ptr<const PSOBBCharacterFile> character);
  static void save_ep3_character_file(const std::string& filename, const PSOGCEp3CharacterFile::Character& character);
  void save_character_file();
  void create_character_file(
      uint32_t guild_card_number,
      Language language,
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);
  void create_battle_overlay(std::shared_ptr<const BattleRules> rules, std::shared_ptr<const LevelTable> level_table);
  void create_challenge_overlay(Version version, size_t template_index, std::shared_ptr<const LevelTable> level_table);
  inline void delete_overlay() {
    this->overlay_character_data.reset();
  }
  inline bool has_overlay() const {
    return this->overlay_character_data.get() != nullptr;
  }

  static std::string bank_filename(const std::string& bb_username, ssize_t index);
  std::string bank_filename() const;
  std::shared_ptr<PlayerBank> bank_file(bool allow_load = true);
  std::shared_ptr<const PlayerBank> bank_file(bool throw_if_missing = true) const;
  static void save_bank_file(const std::string& filename, const PlayerBank& bank);
  void save_bank_file() const;
  void change_bank(ssize_t bb_character_index); // -1 = use shared bank

  std::string legacy_account_filename() const;
  std::string legacy_player_filename() const;

  void save_all();

  void load_backup_character(uint32_t account_id, size_t index);
  std::shared_ptr<PSOGCEp3CharacterFile::Character> load_ep3_backup_character(uint32_t account_id, size_t index);
  void unload_character(bool save);

  void print_inventory() const;
  void print_bank() const;

  void cancel_pending_promises();

private:
  // The overlay character data is used in battle and challenge modes, when
  // character data is temporarily replaced in-game. In other play modes and in
  // lobbies, overlay_character_data is null.
  std::shared_ptr<PSOBBBaseSystemFile> system_data;
  std::shared_ptr<PSOBBCharacterFile> overlay_character_data;
  std::shared_ptr<PSOBBCharacterFile> character_data;
  std::shared_ptr<PSOBBGuildCardFile> guild_card_data;
  std::shared_ptr<PlayerBank> bank_data;
  uint64_t last_play_time_update = 0;

  void load_all_files();
  void update_character_data_after_load(std::shared_ptr<PSOBBCharacterFile> character_data);
};
