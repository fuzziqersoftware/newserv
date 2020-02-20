#pragma once

#include <inttypes.h>

#include <vector>
#include <phosg/Concurrency.hh>

#include "Client.hh"
#include "Player.hh"
#include "Map.hh"
#include "RareItemSet.hh"

enum LobbyFlag {
  // games and lobbies are actually the same thing - games have negative IDs,
  // lobbies have IDs >= 0
  IsGame = 0x01,
  CheatsEnabled = 0x02, // game only
  Public = 0x04, // lobby only
  Episode3 = 0x08, // lobby only
  QuestInProgress = 0x10, // game only
  JoinableQuestInProgress = 0x20, // game only
  Default = 0x40, // lobby only; false for private lobbies
  Persistent = 0x80, // if not set, lobby is deleted when empty
};

struct Lobby {
  uint32_t lobby_id;

  uint32_t min_level;
  uint32_t max_level;

  // item info
  std::vector<PSOEnemy> enemies;
  std::shared_ptr<const RareItemSet> rare_item_set;
  uint32_t next_item_id[12];
  uint32_t next_game_item_id;
  PlayerInventoryItem next_drop_item;
  std::unordered_map<uint32_t, PlayerInventoryItem> item_id_to_floor_item;
  uint32_t variations[0x20];

  // game config
  GameVersion version;
  uint8_t section_id;
  uint8_t episode;
  uint8_t difficulty;
  uint8_t mode;
  char16_t password[0x24];
  char16_t name[0x24];
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
  std::shared_ptr<Client> clients[12];

  Lobby();

  bool is_game() const;

  void reassign_leader_on_client_departure(size_t leaving_client_id);
  size_t count_clients() const;
  bool any_client_loading() const;

  void add_client(std::shared_ptr<Client> c);
  void remove_client(std::shared_ptr<Client> c);

  void move_client_to_lobby(std::shared_ptr<Lobby> dest_lobby,
      std::shared_ptr<Client> c);

  std::shared_ptr<Client> find_client(const char16_t* identifier = NULL,
      uint64_t serial_number = 0);

  void add_item(const PlayerInventoryItem& item);
  void remove_item(uint32_t item_id, PlayerInventoryItem* item);
  size_t find_item(uint32_t item_id);
  uint32_t generate_item_id(uint32_t client_id);

  void assign_item_ids_for_player(uint32_t client_id, PlayerInventory& inv);

  static uint8_t game_event_for_lobby_event(uint8_t lobby_event);
};
