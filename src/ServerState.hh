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
#include "Episode3/DataIndexes.hh"
#include "Episode3/Tournament.hh"
#include "FunctionCompiler.hh"
#include "GSLArchive.hh"
#include "ItemParameterTable.hh"
#include "LevelTable.hh"
#include "License.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "WordSelectTable.hh"

// Forward declarations due to reference cycles
class ProxyServer;
class Server;

struct PortConfiguration {
  std::string name;
  uint16_t port;
  GameVersion version;
  ServerBehavior behavior;
};

struct ServerState : public std::enable_shared_from_this<ServerState> {
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
  bool allow_dc_pc_games;
  bool allow_gc_xb_games;
  bool item_tracking_enabled;
  bool drops_enabled;
  bool ep3_send_function_call_enabled;
  bool catch_handler_exceptions;
  bool ep3_infinite_meseta;
  std::vector<uint32_t> ep3_defeat_player_meseta_rewards;
  std::vector<uint32_t> ep3_defeat_com_meseta_rewards;
  uint32_t ep3_final_round_meseta_bonus;
  bool ep3_jukebox_is_free;
  uint32_t ep3_behavior_flags;
  RunShellBehavior run_shell_behavior;
  CheatModeBehavior cheat_mode_behavior;
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> bb_private_keys;
  std::shared_ptr<const FunctionCodeIndex> function_code_index;
  std::shared_ptr<const PatchFileIndex> pc_patch_file_index;
  std::shared_ptr<const PatchFileIndex> bb_patch_file_index;
  std::shared_ptr<const DOLFileIndex> dol_file_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index_trial;
  std::shared_ptr<const Episode3::MapIndex> ep3_map_index;
  std::shared_ptr<const Episode3::COMDeckIndex> ep3_com_deck_index;
  std::shared_ptr<const G_SetEXResultValues_GC_Ep3_6xB4x4B> ep3_default_ex_values;
  std::shared_ptr<const G_SetEXResultValues_GC_Ep3_6xB4x4B> ep3_tournament_ex_values;
  std::shared_ptr<const G_SetEXResultValues_GC_Ep3_6xB4x4B> ep3_tournament_final_round_ex_values;
  std::shared_ptr<const QuestCategoryIndex> quest_category_index;
  std::shared_ptr<const QuestIndex> default_quest_index;
  std::shared_ptr<const QuestIndex> ep3_download_quest_index;
  std::shared_ptr<const LevelTable> level_table;
  std::shared_ptr<const BattleParamsIndex> battle_params;
  std::shared_ptr<const GSLArchive> bb_data_gsl;
  std::unordered_map<std::string, std::shared_ptr<const RareItemSet>> rare_item_sets;
  std::shared_ptr<const CommonItemSet> common_item_set;
  std::shared_ptr<const ArmorRandomSet> armor_random_set;
  std::shared_ptr<const ToolRandomSet> tool_random_set;
  std::array<std::shared_ptr<const WeaponRandomSet>, 4> weapon_random_sets;
  std::shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set;
  std::shared_ptr<const ItemParameterTable> item_parameter_table;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table;
  std::shared_ptr<const WordSelectTable> word_select_table;

  std::shared_ptr<Episode3::TournamentIndex> ep3_tournament_index;

  uint16_t ep3_card_auction_points;
  uint16_t ep3_card_auction_min_size;
  uint16_t ep3_card_auction_max_size;
  struct CardAuctionPoolEntry {
    uint64_t probability;
    uint16_t card_id;
    uint16_t min_price;
    std::string card_name;
  };
  std::vector<CardAuctionPoolEntry> ep3_card_auction_pool;
  std::vector<std::vector<std::string>> ep3_trap_card_names;
  std::array<std::vector<uint16_t>, 5> ep3_trap_card_ids;
  struct Ep3LobbyBannerEntry {
    uint32_t type = 1;
    uint32_t which; // See B9 documentation in CommandFormats.hh
    std::string data;
  };
  std::vector<Ep3LobbyBannerEntry> ep3_lobby_banners;

  std::shared_ptr<LicenseIndex> license_index;

  std::shared_ptr<const Menu> information_menu_v2;
  std::shared_ptr<const Menu> information_menu_v3;
  std::shared_ptr<std::vector<std::u16string>> information_contents;
  std::shared_ptr<const Menu> redirect_destinations_menu_dc;
  std::shared_ptr<const Menu> redirect_destinations_menu_pc;
  std::shared_ptr<const Menu> redirect_destinations_menu_gc;
  std::shared_ptr<const Menu> redirect_destinations_menu_xb;
  std::vector<std::pair<std::string, uint16_t>> redirect_destinations_dc;
  std::vector<std::pair<std::string, uint16_t>> redirect_destinations_pc;
  std::vector<std::pair<std::string, uint16_t>> redirect_destinations_gc;
  std::vector<std::pair<std::string, uint16_t>> redirect_destinations_xb;
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

  std::unordered_map<Channel*, std::shared_ptr<Client>> channel_to_client;
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
  ServerState(const ServerState&) = delete;
  ServerState(ServerState&&) = delete;
  ServerState& operator=(const ServerState&) = delete;
  ServerState& operator=(ServerState&&) = delete;
  void init();

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

  uint32_t connect_address_for_client(std::shared_ptr<Client> c) const;

  std::shared_ptr<const Menu> information_menu_for_version(GameVersion version) const;
  std::shared_ptr<const Menu> redirect_destinations_menu_for_version(GameVersion version) const;
  const std::vector<std::pair<std::string, uint16_t>>& redirect_destinations_for_version(GameVersion version) const;
  std::shared_ptr<const Menu> proxy_destinations_menu_for_version(GameVersion version) const;
  const std::vector<std::pair<std::string, uint16_t>>& proxy_destinations_for_version(GameVersion version) const;

  std::shared_ptr<const QuestIndex> quest_index_for_client(std::shared_ptr<Client> c) const;

  void set_port_configuration(const std::vector<PortConfiguration>& port_configs);

  std::shared_ptr<const std::string> load_bb_file(
      const std::string& patch_index_filename,
      const std::string& gsl_filename = "",
      const std::string& bb_directory_filename = "") const;

  JSON load_config() const;
  void collect_network_addresses();
  void parse_config(const JSON& config_json, bool is_reload);
  void load_bb_private_keys();
  void load_licenses();
  void load_patch_indexes();
  void load_battle_params();
  void load_level_table();
  void load_item_tables();
  void load_word_select_table();
  void load_ep3_data();
  void resolve_ep3_card_names();
  void load_quest_index();
  void compile_functions();
  void load_dol_files();
};
