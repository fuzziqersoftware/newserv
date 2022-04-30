#pragma once

#include <inttypes.h>

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

struct Lobby {
  enum Flag {
    GAME = 0x01,
    CHEATS_ENABLED = 0x02, // game only
    PUBLIC = 0x04, // lobby only
    EPISODE_3_ONLY = 0x08, // lobby & game
    QUEST_IN_PROGRESS = 0x10, // game only
    JOINABLE_QUEST_IN_PROGRESS = 0x20, // game only
    DEFAULT = 0x40, // lobby only; not set for games and private lobbies
    PERSISTENT = 0x80, // if not set, lobby is deleted when empty
  };

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
  std::shared_ptr<const RareItemSet> rare_item_set;
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
  uint8_t mode;
  std::u16string password;
  std::u16string name;
  uint32_t rare_seed;

  //EP3_GAME_CONFIG* ep3; // only present if this is an Episode 3 game

  // lobby stuff
  uint8_t event;
  uint8_t block;
  uint8_t type; // number to give to PSO for the lobby number
  uint8_t leader_id;
  uint8_t max_clients;
  uint32_t flags;
  uint32_t loading_quest_id; // for use with joinable quests
  std::array<std::shared_ptr<Client>, 12> clients;

  Lobby();

  inline bool is_game() const {
    return this->flags & Flag::GAME;
  }

  void reassign_leader_on_client_departure(size_t leaving_client_id);
  size_t count_clients() const;
  bool any_client_loading() const;

  void add_client(std::shared_ptr<Client> c, bool reverse_indexes = true);
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
