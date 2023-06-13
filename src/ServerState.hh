#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <phosg/JSON.hh>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Client.hh"
#include "CommonItemSet.hh"
#include "Episode3/DataIndex.hh"
#include "Episode3/Tournament.hh"
#include "FunctionCompiler.hh"
#include "GSLArchive.hh"
#include "ItemParameterTable.hh"
#include "LevelTable.hh"
#include "License.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"

// Forward declarations due to reference cycles
class ProxyServer;
class Server;

struct PortConfiguration {
  std::string name;
  uint16_t port;
  GameVersion version;
  ServerBehavior behavior;
};

struct ServerState {
  enum class RunShellBehavior {
    DEFAULT = 0,
    ALWAYS,
    NEVER,
  };
  enum class CheatModeBehavior {
    OFF = 0,
    OFF_BY_DEFAULT,
    ON_BY_DEFAULT,
  };

  std::string config_filename;
  bool is_replay;

  std::u16string name;
  std::unordered_map<std::string, std::shared_ptr<PortConfiguration>> name_to_port_config;
  std::unordered_map<uint16_t, std::shared_ptr<PortConfiguration>> number_to_port_config;
  std::string username;
  uint16_t dns_server_port;
  std::vector<std::string> ip_stack_addresses;
  bool ip_stack_debug;
  bool allow_unregistered_users;
  bool allow_saving;
  bool item_tracking_enabled;
  bool drops_enabled;
  bool episode_3_send_function_call_enabled;
  bool enable_dol_compression;
  bool catch_handler_exceptions;
  uint32_t ep3_behavior_flags;
  RunShellBehavior run_shell_behavior;
  CheatModeBehavior cheat_mode_behavior;
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> bb_private_keys;
  std::shared_ptr<const FunctionCodeIndex> function_code_index;
  std::shared_ptr<const PatchFileIndex> pc_patch_file_index;
  std::shared_ptr<const PatchFileIndex> bb_patch_file_index;
  std::shared_ptr<const DOLFileIndex> dol_file_index;
  std::shared_ptr<const Episode3::DataIndex> ep3_data_index;
  std::shared_ptr<const QuestCategoryIndex> quest_category_index;
  std::shared_ptr<const QuestIndex> quest_index;
  std::shared_ptr<const LevelTable> level_table;
  std::shared_ptr<const BattleParamsIndex> battle_params;
  std::shared_ptr<const GSLArchive> bb_data_gsl;
  std::shared_ptr<const RareItemSet> rare_item_set;
  std::shared_ptr<const CommonItemSet> common_item_set;
  std::shared_ptr<const ArmorRandomSet> armor_random_set;
  std::shared_ptr<const ToolRandomSet> tool_random_set;
  std::array<std::shared_ptr<const WeaponRandomSet>, 4> weapon_random_sets;
  std::shared_ptr<const ItemParameterTable> item_parameter_table;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table;

  std::shared_ptr<Episode3::TournamentIndex> ep3_tournament_index;

  uint16_t ep3_card_auction_points;
  uint16_t ep3_card_auction_min_size;
  uint16_t ep3_card_auction_max_size;
  std::unordered_map<std::string, std::pair<uint64_t, uint16_t>> ep3_card_auction_pool;

  std::shared_ptr<LicenseManager> license_manager;

  std::shared_ptr<const Menu> information_menu_v2;
  std::shared_ptr<const Menu> information_menu_v3;
  std::shared_ptr<std::vector<std::u16string>> information_contents;
  std::shared_ptr<const Menu> proxy_destinations_menu_dc;
  std::shared_ptr<const Menu> proxy_destinations_menu_pc;
  std::shared_ptr<const Menu> proxy_destinations_menu_gc;
  std::shared_ptr<const Menu> proxy_destinations_menu_xb;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_dc;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_pc;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_gc;
  std::vector<std::pair<std::string, uint16_t>> proxy_destinations_xb;
  std::pair<std::string, uint16_t> proxy_destination_patch;
  std::pair<std::string, uint16_t> proxy_destination_bb;
  std::u16string welcome_message;
  std::u16string pc_patch_server_message;
  std::u16string bb_patch_server_message;

  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::vector<std::shared_ptr<Lobby>> public_lobby_search_order_v1;
  std::vector<std::shared_ptr<Lobby>> public_lobby_search_order_non_v1;
  std::vector<std::shared_ptr<Lobby>> public_lobby_search_order_ep3;
  std::atomic<int32_t> next_lobby_id;
  uint8_t pre_lobby_event;
  int32_t ep3_menu_song;

  std::map<std::string, uint32_t> all_addresses;
  uint32_t local_address;
  uint32_t external_address;

  bool proxy_allow_save_files;
  bool proxy_enable_login_options;

  std::shared_ptr<ProxyServer> proxy_server;
  std::shared_ptr<Server> game_server;

  ServerState(const char* config_filename, bool is_replay);

  void add_client_to_available_lobby(std::shared_ptr<Client> c);
  void remove_client_from_lobby(std::shared_ptr<Client> c);
  bool change_client_lobby(
      std::shared_ptr<Client> c,
      std::shared_ptr<Lobby> new_lobby,
      bool send_join_notification = true,
      ssize_t required_client_id = -1);

  void send_lobby_join_notifications(std::shared_ptr<Lobby> l,
      std::shared_ptr<Client> joining_client);

  std::shared_ptr<Lobby> find_lobby(uint32_t lobby_id);
  std::vector<std::shared_ptr<Lobby>> all_lobbies();

  std::shared_ptr<Lobby> create_lobby();
  void remove_lobby(uint32_t lobby_id);

  std::shared_ptr<Client> find_client(
      const std::u16string* identifier = nullptr,
      uint64_t serial_number = 0,
      std::shared_ptr<Lobby> l = nullptr);

  uint32_t connect_address_for_client(std::shared_ptr<Client> c);

  std::shared_ptr<const Menu> information_menu_for_version(GameVersion version);
  std::shared_ptr<const Menu> proxy_destinations_menu_for_version(GameVersion version);
  const std::vector<std::pair<std::string, uint16_t>>& proxy_destinations_for_version(GameVersion version);

  void set_port_configuration(
      const std::vector<PortConfiguration>& port_configs);

  std::shared_ptr<const std::string> load_bb_file(
      const std::string& patch_index_filename,
      const std::string& gsl_filename = "",
      const std::string& bb_directory_filename = "") const;

  std::shared_ptr<JSONObject> load_config() const;
  void collect_network_addresses();
  void parse_config(std::shared_ptr<const JSONObject> config_json);
  void load_licenses();
  void load_patch_indexes();
  void load_battle_params();
  void load_level_table();
  void load_item_tables();
  void load_ep3_data();
  void load_quest_index();
  void compile_functions();
  void load_dol_files();
  void create_menus(std::shared_ptr<const JSONObject> config_json);
};
