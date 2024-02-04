#pragma once

#include <event2/event.h>

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
#include "ItemNameIndex.hh"
#include "ItemParameterTable.hh"
#include "LevelTable.hh"
#include "License.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "PlayerFilesManager.hh"
#include "Quest.hh"
#include "StepGraph.hh"
#include "TeamIndex.hh"
#include "WordSelectTable.hh"

// Forward declarations due to reference cycles
class ProxyServer;
class Server;

struct PortConfiguration {
  std::string name;
  std::string addr; // Blank = listen on all interfaces (default)
  uint16_t port;
  Version version;
  ServerBehavior behavior;
};

struct ServerState : public std::enable_shared_from_this<ServerState> {
  enum class RunShellBehavior {
    DEFAULT = 0,
    ALWAYS,
    NEVER,
  };
  enum class BehaviorSwitch {
    OFF = 0,
    OFF_BY_DEFAULT,
    ON_BY_DEFAULT,
    ON,
  };

  static inline bool behavior_enabled(BehaviorSwitch b) {
    return (b == BehaviorSwitch::ON_BY_DEFAULT) || (b == BehaviorSwitch::ON);
  }
  static inline bool behavior_can_be_overridden(BehaviorSwitch b) {
    return (b == BehaviorSwitch::OFF_BY_DEFAULT) || (b == BehaviorSwitch::ON_BY_DEFAULT);
  }

  uint64_t creation_time;
  std::shared_ptr<struct event_base> base;

  std::string config_filename;
  bool is_replay = false;
  bool config_loaded = false;
  bool default_lobbies_created = false;

  StepGraph load_step_graph;

  std::string name;
  std::unordered_map<std::string, std::shared_ptr<PortConfiguration>> name_to_port_config;
  std::unordered_map<uint16_t, std::shared_ptr<PortConfiguration>> number_to_port_config;
  std::string username;
  std::string dns_server_addr;
  uint16_t dns_server_port = 0;
  std::vector<std::string> ip_stack_addresses;
  std::vector<std::string> ppp_stack_addresses;
  uint64_t client_ping_interval_usecs = 30000000;
  uint64_t client_idle_timeout_usecs = 60000000;
  bool ip_stack_debug = false;
  bool allow_unregistered_users = false;
  bool allow_pc_nte = false;
  bool use_temp_licenses_for_prototypes = true;
  bool allow_dc_pc_games = true;
  bool allow_gc_xb_games = true;
  uint8_t allowed_drop_modes_v1_v2_normal = 0x1F;
  uint8_t allowed_drop_modes_v1_v2_battle = 0x07;
  uint8_t allowed_drop_modes_v1_v2_challenge = 0x07;
  uint8_t allowed_drop_modes_v3_normal = 0x1F;
  uint8_t allowed_drop_modes_v3_battle = 0x07;
  uint8_t allowed_drop_modes_v3_challenge = 0x07;
  uint8_t allowed_drop_modes_v4_normal = 0x1D; // CLIENT not allowed
  uint8_t allowed_drop_modes_v4_battle = 0x05;
  uint8_t allowed_drop_modes_v4_challenge = 0x05;
  Lobby::DropMode default_drop_mode_v1_v2_normal = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v1_v2_battle = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v1_v2_challenge = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v3_normal = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v3_battle = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v3_challenge = Lobby::DropMode::CLIENT;
  Lobby::DropMode default_drop_mode_v4_normal = Lobby::DropMode::SERVER_SHARED;
  Lobby::DropMode default_drop_mode_v4_battle = Lobby::DropMode::SERVER_SHARED;
  Lobby::DropMode default_drop_mode_v4_challenge = Lobby::DropMode::SERVER_SHARED;
  QuestFlagsForDifficulty quest_flag_persist_mask;
  uint64_t persistent_game_idle_timeout_usecs = 0;
  bool ep3_send_function_call_enabled = false;
  bool catch_handler_exceptions = true;
  bool ep3_infinite_meseta = false;
  std::vector<uint32_t> ep3_defeat_player_meseta_rewards = {400, 500, 600, 700, 800};
  std::vector<uint32_t> ep3_defeat_com_meseta_rewards = {100, 200, 300, 400, 500};
  uint32_t ep3_final_round_meseta_bonus = 300;
  bool ep3_jukebox_is_free = false;
  uint32_t ep3_behavior_flags = 0;
  bool hide_download_commands = true;
  RunShellBehavior run_shell_behavior = RunShellBehavior::DEFAULT;
  BehaviorSwitch cheat_mode_behavior = BehaviorSwitch::OFF_BY_DEFAULT;
  bool default_rare_notifs_enabled = false;
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> bb_private_keys;
  std::shared_ptr<const FunctionCodeIndex> function_code_index;
  std::shared_ptr<const PatchFileIndex> pc_patch_file_index;
  std::shared_ptr<const PatchFileIndex> bb_patch_file_index;
  std::array<std::shared_ptr<ThreadSafeFileCache>, NUM_VERSIONS> map_file_caches;
  std::shared_ptr<const DOLFileIndex> dol_file_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index_trial;
  std::shared_ptr<const Episode3::MapIndex> ep3_map_index;
  std::shared_ptr<const Episode3::COMDeckIndex> ep3_com_deck_index;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_default_ex_values;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_tournament_ex_values;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_tournament_final_round_ex_values;
  std::shared_ptr<const QuestCategoryIndex> quest_category_index;
  std::shared_ptr<const QuestIndex> default_quest_index;
  std::shared_ptr<const QuestIndex> ep3_download_quest_index;
  std::shared_ptr<const LevelTable> level_table;
  std::shared_ptr<const BattleParamsIndex> battle_params;
  std::shared_ptr<const GSLArchive> bb_data_gsl;
  std::unordered_map<std::string, std::shared_ptr<const RareItemSet>> rare_item_sets;
  std::shared_ptr<const CommonItemSet> common_item_set_v2;
  std::shared_ptr<const CommonItemSet> common_item_set_v3_v4;
  std::shared_ptr<const ArmorRandomSet> armor_random_set;
  std::shared_ptr<const ToolRandomSet> tool_random_set;
  std::array<std::shared_ptr<const WeaponRandomSet>, 4> weapon_random_sets;
  std::shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set;
  std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS> item_parameter_tables;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table;
  std::shared_ptr<const TextIndex> text_index;
  std::array<std::shared_ptr<const ItemNameIndex>, NUM_VERSIONS> item_name_indexes;
  std::shared_ptr<const WordSelectTable> word_select_table;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> set_data_tables;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> set_data_tables_ep1_ult;
  std::shared_ptr<const SetDataTableBase> bb_solo_set_data_table;
  std::shared_ptr<const SetDataTableBase> bb_solo_set_data_table_ep1_ult;
  std::array<std::shared_ptr<const Map::RareEnemyRates>, 4> rare_enemy_rates_by_difficulty;
  std::shared_ptr<const Map::RareEnemyRates> rare_enemy_rates_challenge;
  std::array<std::array<size_t, 4>, 3> min_levels_v4; // Indexed as [episode][difficulty]

  struct QuestF960Result {
    uint32_t meseta_cost = 0;
    uint32_t base_probability = 0;
    uint32_t probability_upgrade = 0;
    std::array<std::vector<ItemData>, 7> results;

    QuestF960Result() = default;
    QuestF960Result(const JSON& json, std::shared_ptr<const ItemNameIndex> name_index);
  };

  // Indexed as [type][difficulty][random_choice]
  std::vector<std::vector<std::vector<ItemData>>> quest_F95E_results;
  std::vector<std::pair<size_t, ItemData>> quest_F95F_results; // [(num_photon_tickets, item)]
  std::vector<QuestF960Result> quest_F960_success_results;
  QuestF960Result quest_F960_failure_results;
  std::vector<ItemData> secret_lottery_results;
  uint16_t bb_global_exp_multiplier = 1;

  std::shared_ptr<Episode3::TournamentIndex> ep3_tournament_index;

  uint16_t ep3_card_auction_points = 0;
  uint16_t ep3_card_auction_min_size = 0;
  uint16_t ep3_card_auction_max_size = 0;
  struct CardAuctionPoolEntry {
    uint64_t probability;
    uint16_t card_id;
    uint16_t min_price;
  };
  std::vector<CardAuctionPoolEntry> ep3_card_auction_pool;
  std::array<std::vector<uint16_t>, 5> ep3_trap_card_ids;
  struct Ep3LobbyBannerEntry {
    uint32_t type = 1;
    uint32_t which; // See B9 documentation in CommandFormats.hh
    std::string data;
  };
  std::vector<Ep3LobbyBannerEntry> ep3_lobby_banners;

  std::shared_ptr<LicenseIndex> license_index;
  std::shared_ptr<TeamIndex> team_index;
  JSON team_reward_defs_json;

  std::shared_ptr<const Menu> information_menu_v2;
  std::shared_ptr<const Menu> information_menu_v3;
  std::shared_ptr<std::vector<std::string>> information_contents_v2;
  std::shared_ptr<std::vector<std::string>> information_contents_v3;
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
  std::string welcome_message;
  std::string pc_patch_server_message;
  std::string bb_patch_server_message;

  std::shared_ptr<PlayerFilesManager> player_files_manager;
  std::unordered_map<Channel*, std::shared_ptr<Client>> channel_to_client;
  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::unordered_set<std::shared_ptr<Lobby>> lobbies_to_destroy;
  std::shared_ptr<struct event> destroy_lobbies_event;
  std::array<std::vector<uint32_t>, NUM_VERSIONS> public_lobby_search_orders;
  std::atomic<int32_t> next_lobby_id = 1;
  uint8_t pre_lobby_event = 0;
  int32_t ep3_menu_song = -1;

  std::map<std::string, uint32_t> all_addresses;
  uint32_t local_address = 0;
  uint32_t external_address = 0;

  bool proxy_allow_save_files = true;
  bool proxy_enable_login_options = false;

  std::shared_ptr<ProxyServer> proxy_server;
  std::shared_ptr<Server> game_server;

  explicit ServerState(const std::string& config_filename = "");
  ServerState(std::shared_ptr<struct event_base> base, const std::string& config_filename, bool is_replay);
  ServerState(const ServerState&) = delete;
  ServerState(ServerState&&) = delete;
  ServerState& operator=(const ServerState&) = delete;
  ServerState& operator=(ServerState&&) = delete;

  void load_objects_and_downstream_dependents(const std::string& what);
  void load_objects_and_downstream_dependents(const std::vector<std::string>& what);
  void load_objects_and_upstream_dependents(const std::string& what);
  void load_objects_and_upstream_dependents(const std::vector<std::string>& what);

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

  std::shared_ptr<Lobby> create_lobby(bool is_game);
  void remove_lobby(std::shared_ptr<Lobby> l);
  void on_player_left_lobby(std::shared_ptr<Lobby> l, uint8_t leaving_client_id);

  std::shared_ptr<Client> find_client(
      const std::string* identifier = nullptr,
      uint64_t serial_number = 0,
      std::shared_ptr<Lobby> l = nullptr);

  uint32_t connect_address_for_client(std::shared_ptr<Client> c) const;

  std::shared_ptr<const Menu> information_menu(Version version) const;
  std::shared_ptr<const Menu> proxy_destinations_menu(Version version) const;
  const std::vector<std::pair<std::string, uint16_t>>& proxy_destinations(Version version) const;

  std::shared_ptr<const SetDataTableBase> set_data_table(Version version, Episode episode, GameMode mode, uint8_t difficulty) const;

  std::shared_ptr<const ItemParameterTable> item_parameter_table(Version version) const;
  std::shared_ptr<const ItemParameterTable> item_parameter_table_for_encode(Version version) const;
  void set_item_parameter_table(Version version, std::shared_ptr<const ItemParameterTable> table);
  std::shared_ptr<const ItemNameIndex> item_name_index(Version version) const;
  void set_item_name_index(Version version, std::shared_ptr<const ItemNameIndex> index);
  std::string describe_item(Version version, const ItemData& item, bool include_color_codes) const;
  ItemData parse_item_description(Version version, const std::string& description) const;

  const std::vector<uint32_t> public_lobby_search_order(Version version) const;

  std::shared_ptr<const std::vector<std::string>> information_contents_for_client(std::shared_ptr<const Client> c) const;
  std::shared_ptr<const QuestIndex> quest_index(Version version) const;

  size_t default_min_level_for_game(Version version, Episode episode, uint8_t difficulty) const;

  void set_port_configuration(const std::vector<PortConfiguration>& port_configs);

  std::shared_ptr<const std::string> load_bb_file(
      const std::string& patch_index_filename,
      const std::string& gsl_filename = "",
      const std::string& bb_directory_filename = "") const;
  std::shared_ptr<const std::string> load_map_file(Version version, const std::string& filename) const;
  std::shared_ptr<const std::string> load_map_file_uncached(Version version, const std::string& filename) const;

  std::pair<std::string, uint16_t> parse_port_spec(const JSON& json) const;
  std::vector<PortConfiguration> parse_port_configuration(const JSON& json) const;

  void create_load_step_graph();
  void create_default_lobbies();
  void collect_network_addresses();
  void load_config();
  void load_bb_private_keys();
  void load_licenses();
  void load_teams();
  void load_patch_indexes();
  void clear_map_file_caches();
  void load_battle_params();
  void load_level_table();
  void load_text_index();
  static std::shared_ptr<ItemNameIndex> create_item_name_index_for_version(
      Version version, std::shared_ptr<const ItemParameterTable> pmt, std::shared_ptr<const TextIndex> text_index);
  void load_item_name_indexes();
  void load_drop_tables();
  void load_item_definitions();
  void load_set_data_tables();
  void load_word_select_table();
  void load_ep3_data();
  void load_quest_index();
  void compile_functions();
  void load_dol_files();

  void enqueue_destroy_lobbies();
  static void dispatch_destroy_lobbies(evutil_socket_t, short, void* ctx);
};
