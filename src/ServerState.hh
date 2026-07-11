#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <phosg/JSON.hh>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Account.hh"
#include "Client.hh"
#include "ClientFunctionIndex.hh"
#include "CommonItemSet.hh"
#include "DNSServer.hh"
#include "DOLFileIndex.hh"
#include "DataIndex.hh"
#include "Episode3/DataIndexes.hh"
#include "Episode3/Tournament.hh"
#include "GSLArchive.hh"
#include "IPV4RangeSet.hh"
#include "ItemNameIndex.hh"
#include "ItemParameterTable.hh"
#include "ItemTranslationTable.hh"
#include "LevelTable.hh"
#include "Lobby.hh"
#include "MagMetadataTable.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "ShopRandomSets.hh"
#include "TeamIndex.hh"
#include "TekkerAdjustmentSet.hh"
#include "WordSelectTable.hh"

// Forward declarations due to reference cycles
class GameServer;
class IPStackSimulator;
class HTTPServer;

class ServerState : public std::enable_shared_from_this<ServerState> {
public:
  std::shared_ptr<DataIndex> data;

  std::shared_ptr<asio::io_context> io_context;
  std::shared_ptr<asio::thread_pool> thread_pool;

  bool default_lobbies_created = false;
  bool is_replay = false;
  bool use_psov2_rand_crypt = false; // Used in some tests
  bool use_legacy_item_random_behavior = false; // Used in some tests

  std::shared_ptr<Episode3::TournamentIndex> ep3_tournament_index;

  std::shared_ptr<AccountIndex> account_index;
  std::shared_ptr<TeamIndex> team_index;

  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::atomic<int32_t> next_lobby_id = 1;

  std::unordered_map<uint64_t, std::shared_ptr<Client>> client_for_id;
  std::unordered_map<uint32_t, std::shared_ptr<Client>> client_for_account;

  std::shared_ptr<IPStackSimulator> ip_stack_simulator;
  std::shared_ptr<DNSServer> dns_server;
  std::shared_ptr<GameServer> game_server;
  std::shared_ptr<HTTPServer> http_server;

  std::unordered_map<uint32_t, ProxySession::PersistentConfig> proxy_persistent_configs;

  static std::shared_ptr<ServerState> create_shared(std::shared_ptr<DataIndex> data_index, bool is_replay);
  std::shared_ptr<ServerState> clone_shared();

  void add_client_to_available_lobby(std::shared_ptr<Client> c, bool allow_games);
  void remove_client_from_lobby(std::shared_ptr<Client> c);
  bool change_client_lobby(
      std::shared_ptr<Client> c,
      std::shared_ptr<Lobby> new_lobby,
      bool send_join_notification = true,
      ssize_t required_client_id = -1);

  void send_lobby_join_notifications(std::shared_ptr<Lobby> l, std::shared_ptr<Client> joining_client);

  std::shared_ptr<Lobby> find_lobby(uint32_t lobby_id);
  std::vector<std::shared_ptr<Lobby>> all_lobbies();

  std::shared_ptr<Lobby> create_lobby(bool is_game);
  void remove_lobby(std::shared_ptr<Lobby> l);
  void on_player_left_lobby(std::shared_ptr<Lobby> l, uint8_t leaving_client_id);

  std::shared_ptr<Client> find_client(
      const std::string* identifier = nullptr, uint64_t account_id = 0, std::shared_ptr<Lobby> l = nullptr);

  void create_default_lobbies();
  void load_accounts();
  void load_teams();
  void load_ep3_tournament_state();

  void update_default_lobby_events_from_config();
  void reset_between_replays();

  void disconnect_all_banned_clients();

protected:
  ServerState() = default;
  ServerState(const ServerState&) = delete;
  ServerState(ServerState&&) = delete;
  ServerState& operator=(const ServerState&) = delete;
  ServerState& operator=(ServerState&&) = delete;
};
