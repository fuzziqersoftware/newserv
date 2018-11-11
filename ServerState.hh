#pragma once

#include <atomic>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <phosg/Concurrency.hh>
#include <vector>
#include <set>

#include "Client.hh"
#include "Items.hh"
#include "LevelTable.hh"
#include "License.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"



struct PortConfiguration {
  uint16_t port;
  GameVersion version;
  ServerBehavior behavior;
};

struct ServerState {
  std::u16string name;
  std::unordered_map<std::string, PortConfiguration> port_configuration;
  std::shared_ptr<const QuestIndex> quest_index;
  std::shared_ptr<const LevelTable> level_table;
  std::shared_ptr<const BattleParamTable> battle_params;
  std::shared_ptr<const CommonItemCreator> common_item_creator;

  std::shared_ptr<LicenseManager> license_manager;

  std::vector<MenuItem> main_menu;
  std::shared_ptr<std::vector<MenuItem>> information_menu;
  std::shared_ptr<std::vector<std::u16string>> information_contents;

  size_t num_threads;

  rw_lock lobbies_lock;
  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::unordered_map<std::u16string, std::shared_ptr<Lobby>> name_to_lobby;

  std::atomic<int64_t> next_lobby_id;
  std::atomic<int64_t> next_game_id;

  std::set<uint32_t> all_addresses;
  uint32_t local_address;
  uint32_t external_address;

  ServerState();

  void add_client_to_available_lobby(std::shared_ptr<Client> c);
  void remove_client_from_lobby(std::shared_ptr<Client> c);
  void change_client_lobby(std::shared_ptr<Client> c,
      std::shared_ptr<Lobby> new_lobby);

  void send_lobby_join_notifications(std::shared_ptr<Lobby> l,
      std::shared_ptr<Client> joining_client);

  std::shared_ptr<Lobby> find_lobby(int64_t lobby_id);
  std::shared_ptr<Lobby> find_lobby(const std::u16string& name);
  std::vector<std::shared_ptr<Lobby>> all_lobbies();

  void add_lobby(std::shared_ptr<Lobby> l);
  void remove_lobby(int64_t lobby_id);

  std::shared_ptr<Client> find_client(const char16_t* identifier = NULL,
    uint64_t serial_number = 0, std::shared_ptr<Lobby> l = NULL);

  uint32_t connect_address_for_client(std::shared_ptr<Client> c);
};
