#pragma once

#include <event2/event.h>
#include <inttypes.h>

#include <array>
#include <memory>
#include <phosg/Encoding.hh>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Client.hh"
#include "CommandFormats.hh"
#include "Episode3/BattleRecord.hh"
#include "Episode3/Server.hh"
#include "ItemCreator.hh"
#include "Map.hh"
#include "Quest.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct ServerState;

struct Lobby : public std::enable_shared_from_this<Lobby> {
  struct FloorItem {
    ItemData data;
    VectorXZF pos;
    uint64_t drop_number;
    // At most one of the following will be non-null
    std::shared_ptr<const MapState::ObjectState> from_obj;
    std::shared_ptr<const MapState::EnemyState> from_ene;
    // The low 12 bits of flags are visibility flags, specifying which clients
    // can see the item. (In practice, only the lowest 4 of these bits are used,
    // but the game has fields for 12 players so we do too.)
    // The 13th bit (0x1000) specifies whether a rare item notification should
    // be sent to all players when the item is picked up. This has no effect for
    // non-rare items.
    uint16_t flags;

    bool visible_to_client(uint8_t client_id) const;
  };
  struct FloorItemManager {
    phosg::PrefixedLogger log;
    uint64_t next_drop_number;
    // It's important that this is a map and not an unordered_map. See the
    // comment in send_game_item_state for more details.
    std::map<uint32_t, std::shared_ptr<FloorItem>> items; // Keyed on item_id
    std::array<std::map<uint64_t, std::shared_ptr<FloorItem>>, 12> queue_for_client;

    FloorItemManager(uint32_t lobby_id, uint8_t floor);
    ~FloorItemManager() = default;

    bool exists(uint32_t item_id) const;
    std::shared_ptr<FloorItem> find(uint32_t item_id) const;
    void add(
        const ItemData& item,
        const VectorXZF& pos,
        std::shared_ptr<const MapState::ObjectState> from_obj,
        std::shared_ptr<const MapState::EnemyState> from_ene,
        uint16_t flags);
    void add(std::shared_ptr<FloorItem> fi);
    std::shared_ptr<FloorItem> remove(uint32_t item_id, uint8_t client_id);
    std::unordered_set<std::shared_ptr<FloorItem>> evict();
    void clear_inaccessible(uint16_t remaining_clients_mask);
    void clear_private();
    void clear();
    uint32_t reassign_all_item_ids(uint32_t next_item_id);
  };
  enum class Flag {
    // clang-format off
    GAME                            = 0x00000001,
    PERSISTENT                      = 0x00000002,
    DEBUG                           = 0x00000004,
    // Flags used only for games
    CHEATS_ENABLED                  = 0x00000100,
    QUEST_SELECTION_IN_PROGRESS     = 0x00000200,
    QUEST_IN_PROGRESS               = 0x00000400,
    BATTLE_IN_PROGRESS              = 0x00000800,
    JOINABLE_QUEST_IN_PROGRESS      = 0x00001000,
    IS_CLIENT_CUSTOMIZATION         = 0x00002000,
    IS_SPECTATOR_TEAM               = 0x00004000, // .episode must be EP3 also
    SPECTATORS_FORBIDDEN            = 0x00008000,
    START_BATTLE_PLAYER_IMMEDIATELY = 0x00010000,
    CANNOT_CHANGE_CHEAT_MODE        = 0x00020000,
    USE_CREATOR_SECTION_ID          = 0x00040000,
    // Flags used only for lobbies
    PUBLIC                          = 0x01000000,
    DEFAULT                         = 0x02000000,
    IS_OVERFLOW                     = 0x08000000,
    // clang-format on
  };
  enum class DropMode {
    DISABLED = 0,
    CLIENT = 1, // Not allowed for BB games
    SERVER_SHARED = 2,
    SERVER_PRIVATE = 3,
    SERVER_DUPLICATE = 4,
  };

  std::weak_ptr<ServerState> server_state;
  phosg::PrefixedLogger log;

  uint32_t lobby_id;

  uint32_t min_level;
  uint32_t max_level;

  // Game state
  std::array<uint32_t, 12> next_item_id_for_client;
  uint32_t next_game_item_id;
  std::vector<FloorItemManager> floor_item_managers;
  std::shared_ptr<const MapState::RareEnemyRates> rare_enemy_rates;
  std::shared_ptr<MapState> map_state; // Always null for lobbies, never null for games
  Variations variations;
  std::unique_ptr<QuestFlags> quest_flags_known; // If null, ALL quest flags are known
  std::unique_ptr<QuestFlags> quest_flag_values;
  std::unique_ptr<SwitchFlags> switch_flags;

  // Game config
  // Bits in allowed_versions specify who is allowed to join this game. The
  // bits are indexed as (1 << version), where version is a value from the
  // Version enum.
  uint16_t allowed_versions;
  uint8_t creator_section_id;
  uint8_t override_section_id;
  Episode episode;
  GameMode mode;
  uint8_t difficulty; // 0-3
  uint16_t base_exp_multiplier;
  float exp_share_multiplier;
  float challenge_exp_multiplier;
  std::string password;
  std::string name;
  // This seed is also sent to the client for rare enemy generation
  uint32_t random_seed;
  std::shared_ptr<PSOLFGEncryption> opt_rand_crypt;
  uint8_t allowed_drop_modes;
  DropMode drop_mode;
  std::shared_ptr<ItemCreator> item_creator; // Always null for lobbies, never null for games

  struct ChallengeParameters {
    uint8_t stage_number = 0;
    uint32_t rank_color = 0xFFFFFFFF;
    std::string rank_text;
    struct RankThreshold {
      uint32_t bitmask = 0;
      uint32_t seconds = 0;
    };
    std::array<RankThreshold, 3> rank_thresholds;
  };
  std::shared_ptr<ChallengeParameters> challenge_params;

  // Ep3 stuff
  // There are three kinds of Episode 3 games. All of these types have episode
  // set to EP3; types 2 and 3 additionally have the IS_SPECTATOR_TEAM flag.
  // 1. Primary games. These are the lobbies where battles may take place.
  // 2. Watcher games. These lobbies receive all the battle and chat commands
  //    from a primary game. (This the implementation of spectator teams.)
  // 3. Replay games. These lobbies replay a sequence of battle commands and
  //    chat commands from a previous primary game.
  // Types 2 and 3 may be distinguished by the presence of the battle_record
  // field - in replay games, it will be present; in watcher games it will be
  // absent.
  std::shared_ptr<Episode3::Server> ep3_server; // Only used in primary games
  std::weak_ptr<Lobby> watched_lobby; // Only used in watcher games
  std::unordered_set<std::shared_ptr<Lobby>> watcher_lobbies; // Only used in primary games
  std::shared_ptr<Episode3::BattleRecord> battle_record; // Not used in watcher games
  std::shared_ptr<Episode3::BattleRecordPlayer> battle_player; // Only used in replay games
  std::shared_ptr<Episode3::Tournament::Match> tournament_match;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_ex_result_values;

  // Lobby stuff
  uint8_t event;
  uint8_t block;
  uint8_t leader_id;
  uint8_t max_clients;
  uint32_t enabled_flags;
  std::shared_ptr<const Quest> quest;
  std::array<std::shared_ptr<Client>, 12> clients;
  // Keys in this map are client_id
  std::unordered_map<size_t, std::weak_ptr<Client>> clients_to_add;

  // This is only used when the PERSISTENT flag is set and idle_timeout_usecs
  // is not zero
  uint64_t idle_timeout_usecs;
  std::unique_ptr<struct event, void (*)(struct event*)> idle_timeout_event;

  Lobby(std::shared_ptr<ServerState> s, uint32_t id, bool is_game);
  Lobby(const Lobby&) = delete;
  Lobby(Lobby&&) = delete;
  ~Lobby();
  Lobby& operator=(const Lobby&) = delete;
  Lobby& operator=(Lobby&&) = delete;

  void reset_next_item_ids();

  [[nodiscard]] inline bool check_flag(Flag flag) const {
    return !!(this->enabled_flags & static_cast<uint32_t>(flag));
  }
  inline void set_flag(Flag flag) {
    this->enabled_flags |= static_cast<uint32_t>(flag);
  }
  inline void clear_flag(Flag flag) {
    this->enabled_flags &= (~static_cast<uint32_t>(flag));
  }
  inline void toggle_flag(Flag flag) {
    this->enabled_flags ^= static_cast<uint32_t>(flag);
  }

  std::shared_ptr<ServerState> require_server_state() const;
  std::shared_ptr<ChallengeParameters> require_challenge_params() const;
  void create_item_creator(Version logic_version = Version::UNKNOWN);
  uint8_t effective_section_id() const;
  uint16_t quest_version_flags() const;
  void load_maps();
  void create_ep3_server();

  [[nodiscard]] inline bool is_game() const {
    return this->check_flag(Flag::GAME);
  }
  [[nodiscard]] bool is_ep3_nte() const;
  [[nodiscard]] inline bool is_ep3() const {
    return this->episode == Episode::EP3;
  }

  [[nodiscard]] inline bool version_is_allowed(Version v) const {
    return this->allowed_versions & (1 << static_cast<size_t>(v));
  }
  inline void allow_version(Version v) {
    this->allowed_versions |= (1 << static_cast<size_t>(v));
  }
  inline void forbid_version(Version v) {
    this->allowed_versions &= ~(1 << static_cast<size_t>(v));
  }

  void reassign_leader_on_client_departure(size_t leaving_client_id);
  size_t count_clients() const;
  bool any_v1_clients_present() const;
  bool any_client_loading() const;

  void add_client(std::shared_ptr<Client> c, ssize_t required_client_id = -1);
  void remove_client(std::shared_ptr<Client> c);

  void move_client_to_lobby(
      std::shared_ptr<Lobby> dest_lobby,
      std::shared_ptr<Client> c,
      ssize_t required_client_id = -1);

  std::shared_ptr<Client> find_client(const std::string* identifier = nullptr, uint64_t account_id = 0);

  enum class JoinError {
    ALLOWED = 0,
    FULL,
    VERSION_CONFLICT,
    QUEST_SELECTION_IN_PROGRESS,
    QUEST_IN_PROGRESS,
    BATTLE_IN_PROGRESS,
    LOADING,
    SOLO,
    INCORRECT_PASSWORD,
    LEVEL_TOO_LOW,
    LEVEL_TOO_HIGH,
    NO_ACCESS_TO_QUEST,
  };
  JoinError join_error_for_client(std::shared_ptr<Client> c, const std::string* password) const;

  bool item_exists(uint8_t floor, uint32_t item_id) const;
  std::shared_ptr<FloorItem> find_item(uint8_t floor, uint32_t item_id) const;
  void add_item(
      uint8_t floor,
      const ItemData& item,
      const VectorXZF& pos,
      std::shared_ptr<const MapState::ObjectState> from_obj,
      std::shared_ptr<const MapState::EnemyState> from_ene,
      uint16_t flags);
  void add_item(uint8_t floor, std::shared_ptr<FloorItem>);
  void evict_items_from_floor(uint8_t floor);
  std::shared_ptr<FloorItem> remove_item(uint8_t floor, uint32_t item_id, uint8_t requesting_client_id);

  uint32_t generate_item_id(uint8_t client_id);
  void on_item_id_generated_externally(uint32_t item_id);
  void assign_inventory_and_bank_item_ids(std::shared_ptr<Client> c, bool consume_ids);

  QuestIndex::IncludeCondition quest_include_condition() const;

  std::unordered_map<uint32_t, std::shared_ptr<Client>> clients_by_account_id() const;

  static void dispatch_on_idle_timeout(evutil_socket_t, short, void* ctx);

  static bool compare_shared(const std::shared_ptr<const Lobby>& a, const std::shared_ptr<const Lobby>& b);
};

template <>
Lobby::DropMode phosg::enum_for_name<Lobby::DropMode>(const char* name);
template <>
const char* phosg::name_for_enum<Lobby::DropMode>(Lobby::DropMode value);
