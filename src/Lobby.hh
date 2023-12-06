#pragma once

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
#include "Player.hh"
#include "Quest.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct ServerState;

struct Lobby : public std::enable_shared_from_this<Lobby> {
  enum class Flag {
    GAME = 0x00000001,
    PERSISTENT = 0x00000002,

    // Flags used only for games
    CHEATS_ENABLED = 0x00000100,
    QUEST_IN_PROGRESS = 0x00000200,
    BATTLE_IN_PROGRESS = 0x00000400,
    JOINABLE_QUEST_IN_PROGRESS = 0x00000800,
    ITEM_TRACKING_ENABLED = 0x00001000,
    IS_SPECTATOR_TEAM = 0x00002000, // episode must be EP3 also
    SPECTATORS_FORBIDDEN = 0x00004000,
    START_BATTLE_PLAYER_IMMEDIATELY = 0x00008000,
    DROPS_ENABLED = 0x00020000,
    CANNOT_CHANGE_DROPS_ENABLED = 0x00040000,
    CANNOT_CHANGE_ITEM_TABLE = 0x00080000,
    CANNOT_CHANGE_CHEAT_MODE = 0x00100000,

    // Flags used only for lobbies
    PUBLIC = 0x01000000,
    DEFAULT = 0x02000000,
    IS_OVERFLOW = 0x08000000,
  };

  std::weak_ptr<ServerState> server_state;
  PrefixedLogger log;

  uint32_t lobby_id;

  uint32_t min_level;
  uint32_t max_level;

  // Item info
  struct FloorItem {
    ItemData data;
    float x;
    float z;
    uint8_t floor;
  };
  std::shared_ptr<const Map::RareEnemyRates> rare_enemy_rates;
  std::shared_ptr<Map> map;
  std::array<uint32_t, 12> next_item_id;
  uint32_t next_game_item_id;
  std::unordered_map<uint32_t, FloorItem> item_id_to_floor_item;
  parray<le_uint32_t, 0x20> variations;

  // Game config
  Version base_version;
  // Bits in allowed_versions specify who is allowed to join this game. The
  // bits are indexed as (1 << version), where version is a value from the
  // Version enum.
  uint16_t allowed_versions;
  uint8_t section_id;
  Episode episode;
  GameMode mode;
  uint8_t difficulty; // 0-3
  uint16_t base_exp_multiplier;
  float challenge_exp_multiplier;
  std::string password;
  std::string name;
  // This seed is also sent to the client for rare enemy generation
  uint32_t random_seed;
  std::shared_ptr<PSOLFGEncryption> random_crypt;
  std::shared_ptr<ItemCreator> item_creator;

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
  // There are three kinds of Episode 3 games. All of these types have the flag
  // EPISODE_3_ONLY; types 2 and 3 additionally have the IS_SPECTATOR_TEAM flag.
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
  std::shared_ptr<const G_SetEXResultValues_GC_Ep3_6xB4x4B> ep3_ex_result_values;

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

  Lobby(std::shared_ptr<ServerState> s, uint32_t id);
  Lobby(const Lobby&) = delete;
  Lobby(Lobby&&) = delete;
  Lobby& operator=(const Lobby&) = delete;
  Lobby& operator=(Lobby&&) = delete;

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
  void create_item_creator();
  void create_ep3_server();

  [[nodiscard]] inline bool is_game() const {
    return this->check_flag(Flag::GAME);
  }
  [[nodiscard]] inline bool is_ep3() const {
    return this->episode == Episode::EP3;
  }

  [[nodiscard]] inline bool version_is_allowed(Version v) const {
    return this->allowed_versions & (1 << static_cast<size_t>(v));
  }
  inline void allow_version(Version v) {
    this->allowed_versions |= (1 << static_cast<size_t>(v));
  }

  void reassign_leader_on_client_departure(size_t leaving_client_id);
  size_t count_clients() const;
  bool any_client_loading() const;

  void add_client(std::shared_ptr<Client> c, ssize_t required_client_id = -1);
  void remove_client(std::shared_ptr<Client> c);

  void move_client_to_lobby(
      std::shared_ptr<Lobby> dest_lobby,
      std::shared_ptr<Client> c,
      ssize_t required_client_id = -1);

  std::shared_ptr<Client> find_client(
      const std::string* identifier = nullptr,
      uint64_t serial_number = 0);

  bool item_exists(uint32_t item_id) const;
  const FloorItem& find_item(uint32_t item_id) const;
  void add_item(const ItemData& item, uint8_t floor, float x, float z);
  ItemData remove_item(uint32_t item_id);
  uint32_t generate_item_id(uint8_t client_id);
  void on_item_id_generated_externally(uint32_t item_id);
  void assign_inventory_and_bank_item_ids(std::shared_ptr<Client> c);

  QuestIndex::IncludeCondition quest_include_condition() const;

  static uint8_t game_event_for_lobby_event(uint8_t lobby_event);

  std::unordered_map<uint32_t, std::shared_ptr<Client>> clients_by_serial_number() const;
};
