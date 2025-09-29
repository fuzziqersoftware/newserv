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
#include "CommonItemSet.hh"
#include "DNSServer.hh"
#include "Episode3/DataIndexes.hh"
#include "Episode3/Tournament.hh"
#include "FunctionCompiler.hh"
#include "GSLArchive.hh"
#include "IPV4RangeSet.hh"
#include "ItemNameIndex.hh"
#include "ItemParameterTable.hh"
#include "ItemTranslationTable.hh"
#include "LevelTable.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "TeamIndex.hh"
#include "WordSelectTable.hh"

// Forward declarations due to reference cycles
class GameServer;
class IPStackSimulator;
class HTTPServer;

struct PortConfiguration {
  std::string name;
  std::string addr; // Blank = listen on all interfaces (default)
  uint16_t port;
  Version version;
  ServerBehavior behavior;
};

struct CheatFlags {
  // This structure describes which behaviors are considered cheating (that is,
  // require cheat mode to be enabled or the user to have the CHEAT_ANYWHERE
  // account flag). A false value here means that that particular behavior is
  // NOT cheating, so cheat mode is NOT required.
  bool create_items = true;
  bool edit_section_id = true;
  bool edit_stats = true;
  bool ep3_replace_assist = true;
  bool ep3_unset_field_character = true;
  bool infinite_hp_tp = true;
  bool insufficient_minimum_level = true;
  bool override_random_seed = true;
  bool override_section_id = true;
  bool override_variations = true;
  bool proxy_override_drops = true;
  bool reset_materials = false;
  bool warp = true;

  CheatFlags() = default;
  explicit CheatFlags(const phosg::JSON& json);
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
  std::shared_ptr<asio::io_context> io_context;

  std::string config_filename;
  std::shared_ptr<const phosg::JSON> config_json;
  bool one_time_config_loaded = false;
  bool default_lobbies_created = false;

  size_t num_worker_threads = 0;
  std::unique_ptr<asio::thread_pool> thread_pool;

  std::string name;
  std::unordered_map<std::string, std::shared_ptr<PortConfiguration>> name_to_port_config;
  std::unordered_map<uint16_t, std::shared_ptr<PortConfiguration>> number_to_port_config;
  std::string username;
  std::string dns_server_addr;
  uint16_t dns_server_port = 0;
  std::vector<std::string> ip_stack_addresses;
  std::vector<std::string> ppp_stack_addresses;
  std::vector<std::string> ppp_raw_addresses;
  std::vector<std::string> http_addresses;
  uint64_t client_ping_interval_usecs = 30000000;
  uint64_t client_idle_timeout_usecs = 60000000;
  uint64_t patch_client_idle_timeout_usecs = 300000000;
  bool is_debug = false;
  bool ip_stack_debug = false;
  bool allow_unregistered_users = false;
  bool allow_pc_nte = false;
  bool use_temp_accounts_for_prototypes = true;
  std::array<uint16_t, NUM_VERSIONS> compatibility_groups = {};
  bool enable_chat_commands = true;
  size_t num_backup_character_slots = 16;
  std::unique_ptr<std::array<uint32_t, NUM_NON_PATCH_VERSIONS>> version_name_colors;
  uint32_t client_customization_name_color = 0x00000000;
  uint8_t allowed_drop_modes_v1_v2_normal = 0x1F;
  uint8_t allowed_drop_modes_v1_v2_battle = 0x07;
  uint8_t allowed_drop_modes_v1_v2_challenge = 0x07;
  uint8_t allowed_drop_modes_v3_normal = 0x1F;
  uint8_t allowed_drop_modes_v3_battle = 0x07;
  uint8_t allowed_drop_modes_v3_challenge = 0x07;
  uint8_t allowed_drop_modes_v4_normal = 0x1D; // CLIENT not allowed
  uint8_t allowed_drop_modes_v4_battle = 0x05;
  uint8_t allowed_drop_modes_v4_challenge = 0x05;
  ServerDropMode default_drop_mode_v1_v2_normal = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v1_v2_battle = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v1_v2_challenge = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v3_normal = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v3_battle = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v3_challenge = ServerDropMode::CLIENT;
  ServerDropMode default_drop_mode_v4_normal = ServerDropMode::SERVER_SHARED;
  ServerDropMode default_drop_mode_v4_battle = ServerDropMode::SERVER_SHARED;
  ServerDropMode default_drop_mode_v4_challenge = ServerDropMode::SERVER_SHARED;
  std::unordered_map<uint16_t, IntegralExpression> quest_flag_rewrites_v1_v2;
  std::unordered_map<uint16_t, IntegralExpression> quest_flag_rewrites_v3;
  std::unordered_map<uint16_t, IntegralExpression> quest_flag_rewrites_v4;
  std::unordered_map<std::string, std::pair<uint8_t, uint32_t>> quest_counter_fields; // For $qfread command
  uint64_t persistent_game_idle_timeout_usecs = 0;
  std::unordered_map<uint32_t, int64_t> enable_send_function_call_quest_numbers;
  bool enable_v3_v4_protected_subcommands = false;
  bool ep3_infinite_meseta = false;
  std::vector<uint32_t> ep3_defeat_player_meseta_rewards = {400, 500, 600, 700, 800};
  std::vector<uint32_t> ep3_defeat_com_meseta_rewards = {100, 200, 300, 400, 500};
  uint32_t ep3_final_round_meseta_bonus = 300;
  bool ep3_jukebox_is_free = false;
  uint32_t ep3_behavior_flags = 0;
  bool hide_download_commands = true;
  RunShellBehavior run_shell_behavior = RunShellBehavior::DEFAULT;
  BehaviorSwitch cheat_mode_behavior = BehaviorSwitch::OFF_BY_DEFAULT;
  bool default_switch_assist_enabled = false;
  bool use_game_creator_section_id = false;
  bool use_psov2_rand_crypt = false; // Used in some tests
  bool rare_notifs_enabled_for_client_drops = false;
  bool default_rare_notifs_enabled_v1_v2 = false;
  bool default_rare_notifs_enabled_v3_v4 = false;
  std::unordered_set<uint32_t> notify_game_for_item_primary_identifiers_v1_v2;
  std::unordered_set<uint32_t> notify_game_for_item_primary_identifiers_v3;
  std::unordered_set<uint32_t> notify_game_for_item_primary_identifiers_v4;
  std::unordered_set<uint32_t> notify_server_for_item_primary_identifiers_v1_v2;
  std::unordered_set<uint32_t> notify_server_for_item_primary_identifiers_v3;
  std::unordered_set<uint32_t> notify_server_for_item_primary_identifiers_v4;
  bool notify_server_for_max_level_achieved = false;
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> bb_private_keys;
  std::shared_ptr<const parray<uint8_t, 0x16C>> bb_default_keyboard_config;
  std::shared_ptr<const parray<uint8_t, 0x38>> bb_default_joystick_config;
  std::shared_ptr<const FunctionCodeIndex> function_code_index;
  std::shared_ptr<const PatchFileIndex> pc_patch_file_index;
  std::shared_ptr<const PatchFileIndex> bb_patch_file_index;
  std::unordered_map<uint64_t, std::shared_ptr<const MapFile>> map_file_for_source_hash;
  std::map<uint32_t, std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>> map_files_for_free_play_key;
  std::unordered_map<uint64_t, std::shared_ptr<const SuperMap>> supermap_for_source_hash_sum;
  std::unordered_map<uint32_t, std::shared_ptr<const SuperMap>> supermap_for_free_play_key;
  std::shared_ptr<const RoomLayoutIndex> room_layout_index;
  std::shared_ptr<FileContentsCache> bb_stream_files_cache;
  std::shared_ptr<FileContentsCache> bb_system_cache;
  std::shared_ptr<FileContentsCache> gba_files_cache;
  std::shared_ptr<const DOLFileIndex> dol_file_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index;
  std::shared_ptr<const Episode3::CardIndex> ep3_card_index_trial;
  std::shared_ptr<const Episode3::MapIndex> ep3_map_index;
  std::shared_ptr<const Episode3::MapIndex> ep3_download_map_index;
  std::shared_ptr<const Episode3::COMDeckIndex> ep3_com_deck_index;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_default_ex_values;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_tournament_ex_values;
  std::shared_ptr<const G_SetEXResultValues_Ep3_6xB4x4B> ep3_tournament_final_round_ex_values;
  std::shared_ptr<const QuestCategoryIndex> quest_category_index;
  std::shared_ptr<const QuestIndex> quest_index;
  std::shared_ptr<const LevelTableV2> level_table_v1_v2;
  std::shared_ptr<const LevelTable> level_table_v3;
  std::shared_ptr<const LevelTable> level_table_v4;
  std::shared_ptr<const BattleParamsIndex> battle_params;
  std::shared_ptr<const GSLArchive> bb_data_gsl;
  std::unordered_map<std::string, std::shared_ptr<const CommonItemSet>> common_item_sets;
  std::unordered_map<std::string, std::shared_ptr<const RareItemSet>> rare_item_sets;
  std::shared_ptr<const ArmorRandomSet> armor_random_set;
  std::shared_ptr<const ToolRandomSet> tool_random_set;
  std::array<std::shared_ptr<const WeaponRandomSet>, 4> weapon_random_sets;
  std::shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set;
  std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS> item_parameter_tables;
  std::shared_ptr<const ItemTranslationTable> item_translation_table;
  std::array<std::shared_ptr<const ItemData::StackLimits>, NUM_VERSIONS> item_stack_limits_tables;
  size_t bb_max_bank_items = 200;
  size_t bb_max_bank_meseta = 999999;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table_v1_v2;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table_v3;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table_v4;
  std::shared_ptr<const TextIndex> text_index;
  std::array<std::shared_ptr<const ItemNameIndex>, NUM_VERSIONS> item_name_indexes;
  std::shared_ptr<const WordSelectTable> word_select_table;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> set_data_tables;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> set_data_tables_ep1_ult;
  std::shared_ptr<const SetDataTableBase> bb_solo_set_data_table;
  std::shared_ptr<const SetDataTableBase> bb_solo_set_data_table_ep1_ult;
  std::array<std::shared_ptr<const MapState::RareEnemyRates>, 4> rare_enemy_rates_by_difficulty;
  std::shared_ptr<const MapState::RareEnemyRates> rare_enemy_rates_challenge;
  std::array<std::array<size_t, 4>, 3> min_levels_v1_v2; // Indexed as [episode][difficulty]
  std::array<std::array<size_t, 4>, 3> min_levels_v3; // Indexed as [episode][difficulty]
  std::array<std::array<size_t, 4>, 3> min_levels_v4; // Indexed as [episode][difficulty]
  std::unordered_set<std::string> bb_required_patches;
  std::unordered_set<std::string> auto_patches;
  CheatFlags cheat_flags;

  struct QuestF960Result {
    uint32_t meseta_cost = 0;
    uint32_t base_probability = 0;
    uint32_t probability_upgrade = 0;
    std::array<std::vector<ItemData>, 7> results;

    QuestF960Result() = default;
    QuestF960Result(const phosg::JSON& json, std::shared_ptr<const ItemNameIndex> name_index);
  };

  // Indexed as [type][difficulty][random_choice]
  std::vector<std::vector<std::vector<ItemData>>> quest_F95E_results;
  std::vector<std::pair<size_t, ItemData>> quest_F95F_results; // [(num_photon_tickets, item)]
  std::vector<QuestF960Result> quest_F960_success_results;
  QuestF960Result quest_F960_failure_results;
  std::vector<ItemData> secret_lottery_results;
  float bb_global_exp_multiplier = 1.0f;
  float exp_share_multiplier = 0.5f;
  float server_global_drop_rate_multiplier = 1.0f;

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

  std::shared_ptr<AccountIndex> account_index;
  bool allow_saving_accounts = true;
  std::shared_ptr<IPV4RangeSet> banned_ipv4_ranges;
  std::shared_ptr<TeamIndex> team_index;
  phosg::JSON team_reward_defs_json;

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
  std::optional<std::pair<std::string, uint16_t>> proxy_destination_patch;
  std::optional<std::pair<std::string, uint16_t>> proxy_destination_bb;
  std::string welcome_message;
  std::string pc_patch_server_message;
  std::string bb_patch_server_message;

  std::map<int64_t, std::shared_ptr<Lobby>> id_to_lobby;
  std::array<std::vector<uint32_t>, NUM_VERSIONS> public_lobby_search_orders;
  std::vector<uint32_t> client_customization_public_lobby_search_order;
  std::atomic<int32_t> next_lobby_id = 1;
  uint8_t pre_lobby_event = 0;
  int32_t ep3_menu_song = -1;

  std::map<std::string, uint32_t> all_addresses;
  uint32_t local_address = 0;
  uint32_t external_address = 0;

  bool proxy_allow_save_files = true;

  std::shared_ptr<IPStackSimulator> ip_stack_simulator;
  std::shared_ptr<DNSServer> dns_server;
  std::shared_ptr<GameServer> game_server;
  std::shared_ptr<HTTPServer> http_server;

  std::unordered_map<uint32_t, ProxySession::PersistentConfig> proxy_persistent_configs;

  explicit ServerState(const std::string& config_filename = "");
  ServerState(const ServerState&) = delete;
  ServerState(ServerState&&) = delete;
  ServerState& operator=(const ServerState&) = delete;
  ServerState& operator=(ServerState&&) = delete;

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
      uint64_t account_id = 0,
      std::shared_ptr<Lobby> l = nullptr);

  uint32_t connect_address_for_client(std::shared_ptr<Client> c) const;
  uint16_t game_server_port_for_version(Version v) const;

  std::shared_ptr<const Menu> information_menu(Version version) const;
  std::shared_ptr<const Menu> proxy_destinations_menu(Version version) const;
  const std::vector<std::pair<std::string, uint16_t>>& proxy_destinations(Version version) const;

  std::shared_ptr<const SetDataTableBase> set_data_table(Version version, Episode episode, GameMode mode, uint8_t difficulty) const;

  std::shared_ptr<const LevelTable> level_table(Version version) const;
  std::shared_ptr<const ItemParameterTable> item_parameter_table(Version version) const;
  std::shared_ptr<const ItemParameterTable> item_parameter_table_for_encode(Version version) const;
  std::shared_ptr<const MagEvolutionTable> mag_evolution_table(Version version) const;
  std::shared_ptr<const ItemData::StackLimits> item_stack_limits(Version version) const;
  std::shared_ptr<const ItemNameIndex> item_name_index_opt(Version version) const; // Returns null if missing
  std::shared_ptr<const ItemNameIndex> item_name_index(Version version) const; // Throws if missing
  std::string describe_item(Version version, const ItemData& item, uint8_t flags = 0) const;
  ItemData parse_item_description(Version version, const std::string& description) const;

  std::shared_ptr<const CommonItemSet> common_item_set(Version logic_version, std::shared_ptr<const Quest> q) const;
  std::shared_ptr<const RareItemSet> rare_item_set(Version logic_version, std::shared_ptr<const Quest> q) const;

  const std::vector<uint32_t>& public_lobby_search_order(Version version, bool is_client_customization) const;
  inline const std::vector<uint32_t>& public_lobby_search_order(std::shared_ptr<const Client> c) const {
    return this->public_lobby_search_order(c->version(), c->check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION));
  }

  inline uint32_t name_color_for_client(Version v, bool is_client_customization) const {
    if (is_client_customization && this->client_customization_name_color) {
      return this->client_customization_name_color;
    }
    return this->version_name_colors ? this->version_name_colors->at(static_cast<size_t>(v) - NUM_PATCH_VERSIONS) : 0;
  }
  inline uint32_t name_color_for_client(std::shared_ptr<const Client> c) const {
    return this->name_color_for_client(c->version(), c->check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION));
  }

  std::shared_ptr<const std::vector<std::string>> information_contents_for_client(std::shared_ptr<const Client> c) const;

  size_t default_min_level_for_game(Version version, Episode episode, uint8_t difficulty) const;

  void set_port_configuration(const std::vector<PortConfiguration>& port_configs);

  std::shared_ptr<const std::string> load_bb_file(
      const std::string& patch_index_filename,
      const std::string& gsl_filename = "",
      const std::string& bb_directory_filename = "") const;
  std::shared_ptr<const std::string> load_map_file(Version version, const std::string& filename) const;
  std::shared_ptr<const std::string> load_map_file_uncached(Version version, const std::string& filename) const;

  std::pair<std::string, uint16_t> parse_port_spec(const phosg::JSON& json) const;
  std::vector<PortConfiguration> parse_port_configuration(const phosg::JSON& json) const;

  static constexpr uint32_t free_play_key(
      Episode episode, GameMode mode, uint8_t difficulty, uint8_t floor, uint32_t layout, uint32_t entities) {
    return (static_cast<uint32_t>(episode) << 28) |
        (static_cast<uint32_t>(mode) << 26) |
        (static_cast<uint32_t>(difficulty) << 24) |
        (static_cast<uint32_t>(floor) << 16) |
        (static_cast<uint32_t>(layout) << 8) |
        (static_cast<uint32_t>(entities) << 0);
  }
  std::shared_ptr<const SuperMap> get_free_play_supermap(
      Episode episode, GameMode mode, uint8_t difficulty, uint8_t floor, uint32_t layout, uint32_t entities);
  std::vector<std::shared_ptr<const SuperMap>> supermaps_for_variations(
      Episode episode,
      GameMode mode,
      uint8_t difficulty,
      const Variations& variations);

  void create_default_lobbies();
  void collect_network_addresses();
  void load_config_early();
  void load_config_late();
  void load_bb_private_keys();
  void load_bb_system_defaults();
  void load_accounts();
  void load_teams();
  void load_patch_indexes();
  void load_maps();
  void clear_file_caches();
  void load_battle_params();
  void load_level_tables();
  void load_text_index();
  std::shared_ptr<ItemNameIndex> create_item_name_index_for_version(
      std::shared_ptr<const ItemParameterTable> pmt,
      std::shared_ptr<const ItemData::StackLimits> limits,
      std::shared_ptr<const TextIndex> text_index) const;
  void load_item_name_indexes();
  void load_drop_tables();
  void load_item_definitions();
  void load_set_data_tables();
  void load_word_select_table();
  void load_ep3_cards();
  void load_ep3_maps();
  void load_ep3_tournament_state();
  void load_quest_index();
  void compile_functions();
  void load_dol_files();

  void load_all(bool enable_thread_pool);

  void disconnect_all_banned_clients();
};
