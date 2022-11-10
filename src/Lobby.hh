#pragma once

#include <inttypes.h>

#include <random>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <phosg/Encoding.hh>

#include "Client.hh"
#include "Player.hh"
#include "Map.hh"
#include "RareItemSet.hh"
#include "Text.hh"
#include "Quest.hh"
#include "Items.hh"
#include "Episode3/Server.hh"

struct Lobby {
  enum Flag {
    GAME                       = 0x00000001,
    EPISODE_3_ONLY             = 0x00000002,
    NON_V1_ONLY                = 0x00000004, // DC NTE and DCv1 not allowed

    // Flags used only for games
    CHEATS_ENABLED             = 0x00000100,
    QUEST_IN_PROGRESS          = 0x00000200,
    JOINABLE_QUEST_IN_PROGRESS = 0x00000400,
    ITEM_TRACKING_ENABLED      = 0x00000800,
    BATTLE_MODE                = 0x00002000,
    CHALLENGE_MODE             = 0x00004000,
    SOLO_MODE                  = 0x00008000,

    // Flags used only for lobbies
    PUBLIC                     = 0x00010000,
    DEFAULT                    = 0x00020000,
    PERSISTENT                 = 0x00040000,
  };

  PrefixedLogger log;

  uint32_t lobby_id;

  uint32_t min_level;
  uint32_t max_level;

  // item info
  struct FloorItem {
    PlayerInventoryItem inv_item;
    float x;
    float z;
    uint8_t area;
  };
  std::vector<PSOEnemy> enemies;
  std::array<uint32_t, 12> next_item_id;
  uint32_t next_game_item_id;
  PlayerInventoryItem next_drop_item;
  std::unordered_map<uint32_t, FloorItem> item_id_to_floor_item;
  parray<le_uint32_t, 0x20> variations;

  // game config
  GameVersion version;
  uint8_t section_id;
  uint8_t episode; // 1 = Ep1, 2 = Ep2, 3 = Ep4, 0xFF = Ep3
  uint8_t difficulty;
  std::u16string password;
  std::u16string name;
  // This seed is also sent to the client for rare enemy generation
  uint32_t random_seed;
  std::shared_ptr<std::mt19937> random;
  std::shared_ptr<const CommonItemCreator> common_item_creator;
  std::shared_ptr<Episode3::ServerBase> ep3_server_base;

  // lobby stuff
  uint8_t event;
  uint8_t block;
  uint8_t type; // number to give to PSO for the lobby number
  uint8_t leader_id;
  uint8_t max_clients;
  uint32_t flags;
  std::shared_ptr<const Quest> loading_quest;
  std::array<std::shared_ptr<Client>, 12> clients;

  explicit Lobby(uint32_t id);

  inline bool is_game() const {
    return this->flags & Flag::GAME;
  }

  void reassign_leader_on_client_departure(size_t leaving_client_id);
  size_t count_clients() const;
  bool any_client_loading() const;

  void add_client(std::shared_ptr<Client> c);
  void remove_client(std::shared_ptr<Client> c);

  void move_client_to_lobby(std::shared_ptr<Lobby> dest_lobby,
      std::shared_ptr<Client> c);

  std::shared_ptr<Client> find_client(
      const std::u16string* identifier = nullptr,
      uint64_t serial_number = 0);

  void add_item(const PlayerInventoryItem& item, uint8_t area, float x, float z);
  PlayerInventoryItem remove_item(uint32_t item_id);
  size_t find_item(uint32_t item_id);
  uint32_t generate_item_id(uint8_t client_id);

  static uint8_t game_event_for_lobby_event(uint8_t lobby_event);
};
