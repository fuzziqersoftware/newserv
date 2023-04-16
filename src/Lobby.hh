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
#include "Episode3/BattleRecord.hh"
#include "Episode3/Server.hh"
#include "ItemCreator.hh"
#include "Map.hh"
#include "Player.hh"
#include "Quest.hh"
#include "RareItemSet.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct Lobby : public std::enable_shared_from_this<Lobby> {
  enum Flag {
    GAME = 0x00000001,
    NON_V1_ONLY = 0x00000002, // DC NTE and DCv1 not allowed
    PERSISTENT = 0x00000004,

    // Flags used only for games
    CHEATS_ENABLED = 0x00000100,
    QUEST_IN_PROGRESS = 0x00000200,
    BATTLE_IN_PROGRESS = 0x00000400,
    JOINABLE_QUEST_IN_PROGRESS = 0x00000800,
    ITEM_TRACKING_ENABLED = 0x00001000,
    IS_SPECTATOR_TEAM = 0x00002000, // episode must be EP3 also
    SPECTATORS_FORBIDDEN = 0x00004000,
    START_BATTLE_PLAYER_IMMEDIATELY = 0x00008000,

    // Flags used only for lobbies
    PUBLIC = 0x01000000,
    DEFAULT = 0x02000000,
  };

  PrefixedLogger log;

  uint32_t lobby_id;

  uint32_t min_level;
  uint32_t max_level;

  // Item info
  struct FloorItem {
    PlayerInventoryItem inv_item;
    float x;
    float z;
    uint8_t area;
  };
  std::vector<PSOEnemy> enemies;
  std::array<uint32_t, 12> next_item_id;
  uint32_t next_game_item_id;
  std::unordered_map<uint32_t, FloorItem> item_id_to_floor_item;
  parray<le_uint32_t, 0x20> variations;

  // Game config
  GameVersion version;
  uint8_t section_id;
  Episode episode;
  GameMode mode;
  uint8_t difficulty; // 0-3
  std::u16string password;
  std::u16string name;
  // This seed is also sent to the client for rare enemy generation
  uint32_t random_seed;
  std::shared_ptr<std::mt19937> random;
  std::shared_ptr<ItemCreator> item_creator;

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
  std::shared_ptr<Episode3::ServerBase> ep3_server_base; // Only used in primary games
  std::weak_ptr<Lobby> watched_lobby; // Only used in watcher games
  std::unordered_set<shared_ptr<Lobby>> watcher_lobbies; // Only used in primary games
  std::shared_ptr<Episode3::BattleRecord> battle_record; // Not used in watcher games
  std::shared_ptr<Episode3::BattleRecord> prev_battle_record; // Only used in primary games
  std::shared_ptr<Episode3::BattleRecordPlayer> battle_player; // Only used in replay games
  std::shared_ptr<Episode3::Tournament::Match> tournament_match;

  // Lobby stuff
  uint8_t event;
  uint8_t block;
  uint8_t type; // number to give to PSO for the lobby number
  uint8_t leader_id;
  uint8_t max_clients;
  uint32_t flags;
  std::shared_ptr<const Quest> quest;
  std::array<std::shared_ptr<Client>, 12> clients;
  // Keys in this map are client_id
  std::unordered_map<size_t, std::weak_ptr<Client>> clients_to_add;

  explicit Lobby(uint32_t id);

  inline bool is_game() const {
    return this->flags & Flag::GAME;
  }
  inline bool is_ep3() const {
    return this->episode == Episode::EP3;
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
      const std::u16string* identifier = nullptr,
      uint64_t serial_number = 0);

  void add_item(const PlayerInventoryItem& item, uint8_t area, float x, float z);
  PlayerInventoryItem remove_item(uint32_t item_id);
  size_t find_item(uint32_t item_id);
  uint32_t generate_item_id(uint8_t client_id);

  static uint8_t game_event_for_lobby_event(uint8_t lobby_event);

  std::unordered_map<uint32_t, std::shared_ptr<Client>> clients_by_serial_number() const;
};
