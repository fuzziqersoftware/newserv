#include "ServerState.hh"

#include <string.h>

#include <memory>
#include <phosg/Image.hh>
#include <phosg/Network.hh>

#include "Compression.hh"
#include "FileContentsCache.hh"
#include "GVMEncoder.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "Text.hh"
#include "UnicodeTextSet.hh"

using namespace std;

ServerState::QuestF960Result::QuestF960Result(const JSON& json, std::shared_ptr<const ItemNameIndex> name_index) {
  static const array<string, 7> day_names = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  this->meseta_cost = json.get_int("MesetaCost", 0);
  this->base_probability = json.get_int("BaseProbability", 0);
  this->probability_upgrade = json.get_int("ProbabilityUpgrade", 0);
  for (size_t day = 0; day < 7; day++) {
    for (const auto& item_it : json.get_list(day_names[day])) {
      this->results[day].emplace_back(name_index->parse_item_description(Version::BB_V4, item_it->as_string()));
    }
  }
}

ServerState::ServerState(shared_ptr<struct event_base> base, const string& config_filename, bool is_replay)
    : creation_time(now()),
      base(base),
      config_filename(config_filename),
      is_replay(is_replay),
      dns_server_port(0),
      ip_stack_debug(false),
      allow_unregistered_users(false),
      allow_pc_nte(false),
      allow_dc_pc_games(false),
      allow_gc_xb_games(true),
      allowed_drop_modes_v1_v2_normal(0x1F),
      allowed_drop_modes_v1_v2_battle(0x07),
      allowed_drop_modes_v1_v2_challenge(0x07),
      allowed_drop_modes_v3_normal(0x1F),
      allowed_drop_modes_v3_battle(0x07),
      allowed_drop_modes_v3_challenge(0x07),
      allowed_drop_modes_v4_normal(0x1D), // CLIENT not allowed
      allowed_drop_modes_v4_battle(0x05),
      allowed_drop_modes_v4_challenge(0x05),
      default_drop_mode_v1_v2_normal(Lobby::DropMode::CLIENT),
      default_drop_mode_v1_v2_battle(Lobby::DropMode::CLIENT),
      default_drop_mode_v1_v2_challenge(Lobby::DropMode::CLIENT),
      default_drop_mode_v3_normal(Lobby::DropMode::CLIENT),
      default_drop_mode_v3_battle(Lobby::DropMode::CLIENT),
      default_drop_mode_v3_challenge(Lobby::DropMode::CLIENT),
      default_drop_mode_v4_normal(Lobby::DropMode::SERVER_SHARED),
      default_drop_mode_v4_battle(Lobby::DropMode::SERVER_SHARED),
      default_drop_mode_v4_challenge(Lobby::DropMode::SERVER_SHARED),
      persistent_game_idle_timeout_usecs(0),
      ep3_send_function_call_enabled(false),
      catch_handler_exceptions(true),
      ep3_infinite_meseta(false),
      ep3_defeat_player_meseta_rewards({400, 500, 600, 700, 800}),
      ep3_defeat_com_meseta_rewards({100, 200, 300, 400, 500}),
      ep3_final_round_meseta_bonus(300),
      ep3_jukebox_is_free(false),
      ep3_behavior_flags(0),
      hide_download_commands(true),
      run_shell_behavior(RunShellBehavior::DEFAULT),
      cheat_mode_behavior(BehaviorSwitch::OFF_BY_DEFAULT),
      bb_global_exp_multiplier(1),
      ep3_card_auction_points(0),
      ep3_card_auction_min_size(0),
      ep3_card_auction_max_size(0),
      player_files_manager(make_shared<PlayerFilesManager>(base)),
      destroy_lobbies_event(event_new(base.get(), -1, EV_TIMEOUT, &ServerState::dispatch_destroy_lobbies, this), event_free),
      next_lobby_id(1),
      pre_lobby_event(0),
      ep3_menu_song(-1),
      local_address(0),
      external_address(0),
      proxy_allow_save_files(true),
      proxy_enable_login_options(false) {}

void ServerState::init() {
  vector<shared_ptr<Lobby>> non_v1_only_lobbies;
  vector<shared_ptr<Lobby>> ep3_only_lobbies;

  for (size_t x = 0; x < 20; x++) {
    auto lobby_name = string_printf("LOBBY%zu", x + 1);
    bool allow_v1 = (x <= 9);
    bool allow_non_ep3 = (x <= 14);

    shared_ptr<Lobby> l = this->create_lobby(false);
    l->set_flag(Lobby::Flag::PUBLIC);
    l->set_flag(Lobby::Flag::DEFAULT);
    l->set_flag(Lobby::Flag::PERSISTENT);
    if (allow_non_ep3) {
      if (allow_v1) {
        l->allow_version(Version::DC_NTE);
        l->allow_version(Version::DC_V1_11_2000_PROTOTYPE);
        l->allow_version(Version::DC_V1);
      }
      l->allow_version(Version::DC_V2);
      l->allow_version(Version::PC_NTE);
      l->allow_version(Version::PC_V2);
      l->allow_version(Version::GC_NTE);
      l->allow_version(Version::GC_V3);
      l->allow_version(Version::XB_V3);
      l->allow_version(Version::BB_V4);
    }
    l->allow_version(Version::GC_EP3_NTE);
    l->allow_version(Version::GC_EP3);

    l->block = x + 1;
    l->name = lobby_name;
    l->max_clients = 12;
    if (!allow_non_ep3) {
      l->episode = Episode::EP3;
    }

    if (allow_non_ep3) {
      this->public_lobby_search_order.emplace_back(l);
    } else {
      ep3_only_lobbies.emplace_back(l);
    }
  }

  // Annoyingly, the CARD lobbies should be searched first, but are sent at the
  // end of the lobby list command, so we have to change the search order
  // manually here.
  this->public_lobby_search_order.insert(
      this->public_lobby_search_order.begin(),
      ep3_only_lobbies.begin(),
      ep3_only_lobbies.end());

  // Load all the necessary data
  auto config = this->load_config();
  this->collect_network_addresses();
  this->load_item_name_index();
  this->parse_config(config, false);
  this->load_bb_private_keys();
  this->load_licenses();
  this->load_teams();
  this->load_patch_indexes();
  this->load_battle_params();
  this->load_level_table();
  this->load_item_tables();
  this->load_word_select_table();
  this->load_ep3_data();
  this->resolve_ep3_card_names();
  this->load_quest_index();
  this->compile_functions();
  this->load_dol_files();
}

void ServerState::add_client_to_available_lobby(shared_ptr<Client> c) {
  shared_ptr<Lobby> added_to_lobby;

  if (c->preferred_lobby_id >= 0) {
    try {
      auto l = this->find_lobby(c->preferred_lobby_id);
      if (l &&
          !l->is_game() &&
          l->check_flag(Lobby::Flag::PUBLIC) &&
          l->version_is_allowed(c->version())) {
        l->add_client(c);
        added_to_lobby = l;
      }
    } catch (const out_of_range&) {
    }
  }

  if (!added_to_lobby.get()) {
    for (const auto& l : this->public_lobby_search_order) {
      try {
        if (l &&
            !l->is_game() &&
            l->check_flag(Lobby::Flag::PUBLIC) &&
            l->version_is_allowed(c->version())) {
          l->add_client(c);
          added_to_lobby = l;
          break;
        }
      } catch (const out_of_range&) {
      }
    }
  }

  if (!added_to_lobby) {
    added_to_lobby = this->create_lobby(false);
    added_to_lobby->set_flag(Lobby::Flag::PUBLIC);
    added_to_lobby->set_flag(Lobby::Flag::IS_OVERFLOW);
    added_to_lobby->block = 100;
    added_to_lobby->name = "Overflow";
    added_to_lobby->max_clients = 12;
    added_to_lobby->event = this->pre_lobby_event;
    added_to_lobby->allow_version(c->version());
    added_to_lobby->add_client(c);
  }

  // Send a join message to the joining player, and notifications to all others
  this->send_lobby_join_notifications(added_to_lobby, c);
}

void ServerState::remove_client_from_lobby(shared_ptr<Client> c) {
  auto l = c->lobby.lock();
  if (l) {
    uint8_t old_client_id = c->lobby_client_id;
    l->remove_client(c);
    this->on_player_left_lobby(l, old_client_id);
  }
}

bool ServerState::change_client_lobby(
    shared_ptr<Client> c,
    shared_ptr<Lobby> new_lobby,
    bool send_join_notification,
    ssize_t required_client_id) {
  uint8_t old_lobby_client_id = c->lobby_client_id;

  auto current_lobby = c->lobby.lock();
  try {
    if (current_lobby) {
      current_lobby->move_client_to_lobby(new_lobby, c, required_client_id);
    } else {
      new_lobby->add_client(c, required_client_id);
    }
  } catch (const out_of_range&) {
    return false;
  }

  if (current_lobby) {
    this->on_player_left_lobby(current_lobby, old_lobby_client_id);
  }
  if (send_join_notification) {
    this->send_lobby_join_notifications(new_lobby, c);
  }
  return true;
}

void ServerState::send_lobby_join_notifications(shared_ptr<Lobby> l,
    shared_ptr<Client> joining_client) {
  for (auto& other_client : l->clients) {
    if (!other_client) {
      continue;
    } else if (other_client == joining_client) {
      send_join_lobby(joining_client, l);
    } else {
      send_player_join_notification(other_client, l, joining_client);
    }
  }
  for (auto& watcher_l : l->watcher_lobbies) {
    for (auto& watcher_c : watcher_l->clients) {
      if (!watcher_c) {
        continue;
      }
      send_player_join_notification(watcher_c, watcher_l, joining_client);
    }
  }
}

shared_ptr<Lobby> ServerState::find_lobby(uint32_t lobby_id) {
  try {
    return this->id_to_lobby.at(lobby_id);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

vector<shared_ptr<Lobby>> ServerState::all_lobbies() {
  vector<shared_ptr<Lobby>> ret;
  for (auto& it : this->id_to_lobby) {
    ret.emplace_back(it.second);
  }
  return ret;
}

shared_ptr<Lobby> ServerState::create_lobby(bool is_game) {
  while (this->id_to_lobby.count(this->next_lobby_id)) {
    this->next_lobby_id++;
  }
  auto l = make_shared<Lobby>(this->shared_from_this(), this->next_lobby_id++, is_game);
  this->id_to_lobby.emplace(l->lobby_id, l);
  l->idle_timeout_usecs = this->persistent_game_idle_timeout_usecs;
  return l;
}

void ServerState::remove_lobby(shared_ptr<Lobby> l) {
  auto lobby_it = this->id_to_lobby.find(l->lobby_id);
  if (lobby_it == this->id_to_lobby.end()) {
    throw logic_error("lobby not registered");
  }
  if (lobby_it->second != l) {
    throw logic_error("incorrect lobby ID in registry");
  }

  if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    auto primary_l = l->watched_lobby.lock();
    if (primary_l) {
      primary_l->log.info("Unlinking watcher lobby %" PRIX32, l->lobby_id);
      primary_l->watcher_lobbies.erase(l);
    } else {
      l->log.info("No watched lobby to unlink");
    }
    l->watched_lobby.reset();
  } else {
    send_ep3_disband_watcher_lobbies(l);
  }

  this->lobbies_to_destroy.emplace(l);
  auto tv = usecs_to_timeval(0);
  event_add(this->destroy_lobbies_event.get(), &tv);

  this->id_to_lobby.erase(lobby_it);
  l->log.info("Enqueued for deletion");
}

void ServerState::on_player_left_lobby(shared_ptr<Lobby> l, uint8_t leaving_client_id) {
  if (l->count_clients() > 0) {
    send_player_leave_notification(l, leaving_client_id);
  } else if (!l->check_flag(Lobby::Flag::PERSISTENT)) {
    this->remove_lobby(l);
  }
}

shared_ptr<Client> ServerState::find_client(const string* identifier, uint64_t serial_number, shared_ptr<Lobby> l) {
  // WARNING: There are multiple callsites where we assume this function never
  // returns a client that isn't in any lobby. If this behavior changes, we will
  // need to audit all callsites to ensure correctness.

  if ((serial_number == 0) && identifier) {
    try {
      serial_number = stoull(*identifier, nullptr, 0);
    } catch (const exception&) {
    }
  }

  if (l) {
    try {
      return l->find_client(identifier, serial_number);
    } catch (const out_of_range&) {
    }
  }

  for (auto& other_l : this->all_lobbies()) {
    if (l == other_l) {
      continue; // don't bother looking again
    }
    try {
      return other_l->find_client(identifier, serial_number);
    } catch (const out_of_range&) {
    }
  }

  throw out_of_range("client not found");
}

uint32_t ServerState::connect_address_for_client(shared_ptr<Client> c) const {
  if (c->channel.is_virtual_connection) {
    if (c->channel.remote_addr.ss_family != AF_INET) {
      throw logic_error("virtual connection is missing remote IPv4 address");
    }
    const auto* sin = reinterpret_cast<const sockaddr_in*>(&c->channel.remote_addr);
    return IPStackSimulator::connect_address_for_remote_address(
        ntohl(sin->sin_addr.s_addr));
  } else {
    // TODO: we can do something smarter here, like use the sockname to find
    // out which interface the client is connected to, and return that address
    if (is_local_address(c->channel.remote_addr)) {
      return this->local_address;
    } else {
      return this->external_address;
    }
  }
}

shared_ptr<const Menu> ServerState::information_menu_for_version(Version version) const {
  if (is_v1_or_v2(version)) {
    return this->information_menu_v2;
  } else if (is_v3(version)) {
    return this->information_menu_v3;
  }
  throw out_of_range("no information menu exists for this version");
}

shared_ptr<const Menu> ServerState::proxy_destinations_menu_for_version(Version version) const {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
      return this->proxy_destinations_menu_dc;
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->proxy_destinations_menu_pc;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->proxy_destinations_menu_gc;
    case Version::XB_V3:
      return this->proxy_destinations_menu_xb;
    default:
      throw out_of_range("no proxy destinations menu exists for this version");
  }
}

const vector<pair<string, uint16_t>>& ServerState::proxy_destinations_for_version(Version version) const {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
      return this->proxy_destinations_dc;
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->proxy_destinations_pc;
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->proxy_destinations_gc;
    case Version::XB_V3:
      return this->proxy_destinations_xb;
    default:
      throw out_of_range("no proxy destinations menu exists for this version");
  }
}

shared_ptr<const ItemParameterTable> ServerState::item_parameter_table_for_version(Version version) const {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->item_parameter_table_v2;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      return this->item_parameter_table_v3;
    case Version::BB_V4:
      return this->item_parameter_table_v4;
    default:
      throw out_of_range("no item parameter table exists for this version");
  }
}

string ServerState::describe_item(Version version, const ItemData& item, bool include_color_codes) const {
  return this->item_name_index->describe_item(
      version,
      item,
      include_color_codes ? this->item_parameter_table_for_version(version) : nullptr);
}

void ServerState::set_port_configuration(
    const vector<PortConfiguration>& port_configs) {
  this->name_to_port_config.clear();
  this->number_to_port_config.clear();

  bool any_port_is_pc_console_detect = false;
  for (const auto& pc : port_configs) {
    auto spc = make_shared<PortConfiguration>(pc);
    if (!this->name_to_port_config.emplace(spc->name, spc).second) {
      // Note: This is a logic_error instead of a runtime_error because
      // port_configs comes from a JSON map, so the names should already all be
      // unique. In contrast, the user can define port configurations with the
      // same number while still writing valid JSON, so only one of these cases
      // can reasonably occur as a result of user behavior.
      throw logic_error("duplicate name in port configuration");
    }
    if (!this->number_to_port_config.emplace(spc->port, spc).second) {
      throw runtime_error("duplicate number in port configuration");
    }
    if (spc->behavior == ServerBehavior::PC_CONSOLE_DETECT) {
      any_port_is_pc_console_detect = true;
    }
  }

  if (any_port_is_pc_console_detect) {
    if (!this->name_to_port_config.count("pc-login")) {
      throw runtime_error("pc-login port is not defined, but some ports use the pc_console_detect behavior");
    }
    if (!this->name_to_port_config.count("console-login")) {
      throw runtime_error("console-login port is not defined, but some ports use the pc_console_detect behavior");
    }
  }
}

shared_ptr<const string> ServerState::load_bb_file(
    const string& patch_index_filename,
    const string& gsl_filename,
    const string& bb_directory_filename) const {

  if (this->bb_patch_file_index) {
    // First, look in the patch tree's data directory
    string patch_index_path = "./data/" + patch_index_filename;
    try {
      auto ret = this->bb_patch_file_index->get(patch_index_path)->load_data();
      static_game_data_log.info("Loaded %s from file in BB patch tree", patch_index_path.c_str());
      return ret;
    } catch (const out_of_range&) {
      static_game_data_log.info("%s missing from BB patch tree", patch_index_path.c_str());
    }
  }

  if (this->bb_data_gsl) {
    // Second, look in the patch tree's data.gsl file
    const string& effective_gsl_filename = gsl_filename.empty() ? patch_index_filename : gsl_filename;
    try {
      // TODO: It's kinda not great that we copy the data here; find a way to
      // avoid doing this (also in the below case)
      auto ret = make_shared<string>(this->bb_data_gsl->get_copy(effective_gsl_filename));
      static_game_data_log.info("Loaded %s from data.gsl in BB patch tree", effective_gsl_filename.c_str());
      return ret;
    } catch (const out_of_range&) {
      static_game_data_log.info("%s missing from data.gsl in BB patch tree", effective_gsl_filename.c_str());
    }

    // Third, look in data.gsl without the filename extension
    size_t dot_offset = effective_gsl_filename.rfind('.');
    if (dot_offset != string::npos) {
      string no_ext_gsl_filename = effective_gsl_filename.substr(0, dot_offset);
      try {
        auto ret = make_shared<string>(this->bb_data_gsl->get_copy(no_ext_gsl_filename));
        static_game_data_log.info("Loaded %s from data.gsl in BB patch tree", no_ext_gsl_filename.c_str());
        return ret;
      } catch (const out_of_range&) {
        static_game_data_log.info("%s missing from data.gsl in BB patch tree", no_ext_gsl_filename.c_str());
      }
    }
  }

  // Finally, look in system/blueburst
  const string& effective_bb_directory_filename = bb_directory_filename.empty() ? patch_index_filename : bb_directory_filename;
  static FileContentsCache cache(10 * 60 * 1000 * 1000); // 10 minutes
  try {
    auto ret = cache.get_or_load("system/blueburst/" + effective_bb_directory_filename);
    static_game_data_log.info("Loaded %s", effective_bb_directory_filename.c_str());
    return ret.file->data;
  } catch (const exception& e) {
    static_game_data_log.info("%s missing from system/blueburst", effective_bb_directory_filename.c_str());
    static_game_data_log.error("%s not found in any source", patch_index_filename.c_str());
    throw cannot_open_file(patch_index_filename);
  }
}

void ServerState::collect_network_addresses() {
  config_log.info("Reading network addresses");
  this->all_addresses = get_local_addresses();
  for (const auto& it : this->all_addresses) {
    string addr_str = string_for_address(it.second);
    config_log.info("Found interface: %s = %s", it.first.c_str(), addr_str.c_str());
  }
}

JSON ServerState::load_config() const {
  config_log.info("Loading configuration");
  return JSON::parse(load_file(this->config_filename));
}

static vector<PortConfiguration> parse_port_configuration(const JSON& json) {
  vector<PortConfiguration> ret;
  for (const auto& item_json_it : json.as_dict()) {
    const auto& item_list = item_json_it.second;
    PortConfiguration& pc = ret.emplace_back();
    pc.name = item_json_it.first;
    pc.port = item_list->at(0).as_int();
    pc.version = enum_for_name<Version>(item_list->at(1).as_string().c_str());
    pc.behavior = enum_for_name<ServerBehavior>(item_list->at(2).as_string().c_str());
  }
  return ret;
}

void ServerState::parse_config(const JSON& json, bool is_reload) {
  config_log.info("Parsing configuration");

  auto parse_behavior_switch = [&](const string& json_key, BehaviorSwitch default_value) -> ServerState::BehaviorSwitch {
    try {
      string behavior = json.get_string(json_key);
      if (behavior == "Off") {
        return ServerState::BehaviorSwitch::OFF;
      } else if (behavior == "OffByDefault") {
        return ServerState::BehaviorSwitch::OFF_BY_DEFAULT;
      } else if (behavior == "OnByDefault") {
        return ServerState::BehaviorSwitch::ON_BY_DEFAULT;
      } else if (behavior == "On") {
        return ServerState::BehaviorSwitch::ON;
      } else {
        throw runtime_error("invalid value for " + json_key);
      }
    } catch (const out_of_range&) {
      return default_value;
    }
  };

  this->name = json.at("ServerName").as_string();

  if (!is_reload) {
    try {
      this->username = json.at("User").as_string();
      if (this->username == "$SUDO_USER") {
        const char* user_from_env = getenv("SUDO_USER");
        if (!user_from_env) {
          throw runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
        }
        this->username = user_from_env;
      }
    } catch (const out_of_range&) {
    }

    this->set_port_configuration(parse_port_configuration(json.at("PortConfiguration")));
    this->dns_server_port = json.get_int("DNSServerPort", this->dns_server_port);
    try {
      for (const auto& item : json.at("IPStackListen").as_list()) {
        if (item->is_int()) {
          this->ip_stack_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->ip_stack_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : json.at("PPPStackListen").as_list()) {
        if (item->is_int()) {
          this->ppp_stack_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->ppp_stack_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
  }

  auto local_address_str = json.at("LocalAddress").as_string();
  try {
    this->local_address = this->all_addresses.at(local_address_str);
    string addr_str = string_for_address(this->local_address);
    config_log.info("Added local address: %s (%s)", addr_str.c_str(),
        local_address_str.c_str());
  } catch (const out_of_range&) {
    this->local_address = address_for_string(local_address_str.c_str());
    config_log.info("Added local address: %s", local_address_str.c_str());
  }
  this->all_addresses.erase("<local>");
  this->all_addresses.emplace("<local>", this->local_address);

  auto external_address_str = json.at("ExternalAddress").as_string();
  try {
    this->external_address = this->all_addresses.at(external_address_str);
    string addr_str = string_for_address(this->external_address);
    config_log.info("Added external address: %s (%s)", addr_str.c_str(),
        external_address_str.c_str());
  } catch (const out_of_range&) {
    this->external_address = address_for_string(external_address_str.c_str());
    config_log.info("Added external address: %s", external_address_str.c_str());
  }
  this->all_addresses.erase("<external>");
  this->all_addresses.emplace("<external>", this->external_address);

  this->ip_stack_debug = json.get_bool("IPStackDebug", this->ip_stack_debug);
  this->allow_unregistered_users = json.get_bool("AllowUnregisteredUsers", this->allow_unregistered_users);
  this->allow_pc_nte = json.get_bool("AllowPCNTE", this->allow_pc_nte);
  this->allowed_drop_modes_v1_v2_normal = json.get_int("AllowedDropModesV1V2Normal", this->allowed_drop_modes_v1_v2_normal);
  this->allowed_drop_modes_v1_v2_battle = json.get_int("AllowedDropModesV1V2Battle", this->allowed_drop_modes_v1_v2_battle);
  this->allowed_drop_modes_v1_v2_challenge = json.get_int("AllowedDropModesV1V2Challenge", this->allowed_drop_modes_v1_v2_challenge);
  this->allowed_drop_modes_v3_normal = json.get_int("AllowedDropModesV3Normal", this->allowed_drop_modes_v3_normal);
  this->allowed_drop_modes_v3_battle = json.get_int("AllowedDropModesV3Battle", this->allowed_drop_modes_v3_battle);
  this->allowed_drop_modes_v3_challenge = json.get_int("AllowedDropModesV3Challenge", this->allowed_drop_modes_v3_challenge);
  this->allowed_drop_modes_v4_normal = json.get_int("AllowedDropModesV4Normal", this->allowed_drop_modes_v4_normal);
  this->allowed_drop_modes_v4_battle = json.get_int("AllowedDropModesV4Battle", this->allowed_drop_modes_v4_battle);
  this->allowed_drop_modes_v4_challenge = json.get_int("AllowedDropModesV4Challenge", this->allowed_drop_modes_v4_challenge);
  this->default_drop_mode_v1_v2_normal = json.get_enum("DefaultDropModeV1V2Normal", this->default_drop_mode_v1_v2_normal);
  this->default_drop_mode_v1_v2_battle = json.get_enum("DefaultDropModeV1V2Battle", this->default_drop_mode_v1_v2_battle);
  this->default_drop_mode_v1_v2_challenge = json.get_enum("DefaultDropModeV1V2Challenge", this->default_drop_mode_v1_v2_challenge);
  this->default_drop_mode_v3_normal = json.get_enum("DefaultDropModeV3Normal", this->default_drop_mode_v3_normal);
  this->default_drop_mode_v3_battle = json.get_enum("DefaultDropModeV3Battle", this->default_drop_mode_v3_battle);
  this->default_drop_mode_v3_challenge = json.get_enum("DefaultDropModeV3Challenge", this->default_drop_mode_v3_challenge);
  this->default_drop_mode_v4_normal = json.get_enum("DefaultDropModeV4Normal", this->default_drop_mode_v4_normal);
  this->default_drop_mode_v4_battle = json.get_enum("DefaultDropModeV4Battle", this->default_drop_mode_v4_battle);
  this->default_drop_mode_v4_challenge = json.get_enum("DefaultDropModeV4Challenge", this->default_drop_mode_v4_challenge);
  if ((this->default_drop_mode_v4_normal == Lobby::DropMode::CLIENT) ||
      (this->default_drop_mode_v4_battle == Lobby::DropMode::CLIENT) ||
      (this->default_drop_mode_v4_challenge == Lobby::DropMode::CLIENT)) {
    throw runtime_error("default V4 drop mode cannot be CLIENT");
  }
  if ((this->allowed_drop_modes_v4_normal & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT))) ||
      (this->allowed_drop_modes_v4_battle & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT))) || (this->allowed_drop_modes_v4_challenge & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT)))) {
    throw runtime_error("CLIENT drop mode cannot be allowed in V4");
  }

  this->quest_flag_persist_mask.update_all(true);
  try {
    for (const auto& flag_id_json : json.get_list("PreventPersistQuestFlags")) {
      this->quest_flag_persist_mask.clear(flag_id_json->as_int());
    }
  } catch (const out_of_range&) {
  }

  this->persistent_game_idle_timeout_usecs = json.get_int("PersistentGameIdleTimeout", this->persistent_game_idle_timeout_usecs);
  this->cheat_mode_behavior = parse_behavior_switch("CheatModeBehavior", this->cheat_mode_behavior);
  this->ep3_send_function_call_enabled = json.get_bool("EnableEpisode3SendFunctionCall", this->ep3_send_function_call_enabled);
  this->catch_handler_exceptions = json.get_bool("CatchHandlerExceptions", this->catch_handler_exceptions);

  auto parse_int_list = +[](const JSON& json) -> vector<uint32_t> {
    vector<uint32_t> ret;
    for (const auto& item : json.as_list()) {
      ret.emplace_back(item->as_int());
    }
    return ret;
  };

  this->ep3_infinite_meseta = json.get_bool("Episode3InfiniteMeseta", this->ep3_infinite_meseta);
  this->ep3_defeat_player_meseta_rewards = parse_int_list(json.get("Episode3DefeatPlayerMeseta", JSON::list()));
  this->ep3_defeat_com_meseta_rewards = parse_int_list(json.get("Episode3DefeatCOMMeseta", JSON::list()));
  this->ep3_final_round_meseta_bonus = json.get_int("Episode3FinalRoundMesetaBonus", this->ep3_final_round_meseta_bonus);
  this->ep3_jukebox_is_free = json.get_bool("Episode3JukeboxIsFree", this->ep3_jukebox_is_free);
  this->ep3_behavior_flags = json.get_int("Episode3BehaviorFlags", this->ep3_behavior_flags);
  this->ep3_card_auction_points = json.get_int("CardAuctionPoints", this->ep3_card_auction_points);
  this->hide_download_commands = json.get_bool("HideDownloadCommands", this->hide_download_commands);
  this->proxy_allow_save_files = json.get_bool("ProxyAllowSaveFiles", this->proxy_allow_save_files);
  this->proxy_enable_login_options = json.get_bool("ProxyEnableLoginOptions", this->proxy_enable_login_options);

  try {
    const auto& i = json.at("CardAuctionSize");
    if (i.is_int()) {
      this->ep3_card_auction_min_size = i.as_int();
      this->ep3_card_auction_max_size = this->ep3_card_auction_min_size;
    } else {
      this->ep3_card_auction_min_size = i.at(0).as_int();
      this->ep3_card_auction_max_size = i.at(1).as_int();
    }
  } catch (const out_of_range&) {
    this->ep3_card_auction_min_size = 0;
    this->ep3_card_auction_max_size = 0;
  }

  try {
    for (const auto& it : json.get_dict("CardAuctionPool")) {
      this->ep3_card_auction_pool.emplace_back(
          CardAuctionPoolEntry{
              .probability = static_cast<uint64_t>(it.second->at(0).as_int()),
              .card_id = 0,
              .min_price = static_cast<uint16_t>(it.second->at(1).as_int()),
              .card_name = it.first});
    }
  } catch (const out_of_range&) {
  }

  try {
    const auto& ep3_trap_cards_json = json.get_list("Episode3TrapCards");
    if (!ep3_trap_cards_json.empty()) {
      if (ep3_trap_cards_json.size() != 5) {
        throw runtime_error("Episode3TrapCards must be a list of 5 lists");
      }
      this->ep3_trap_card_names.clear();
      for (const auto& trap_type_it : ep3_trap_cards_json) {
        auto& names = this->ep3_trap_card_names.emplace_back();
        for (const auto& card_it : trap_type_it->as_list()) {
          names.emplace_back(card_it->as_string());
        }
      }
    }
  } catch (const out_of_range&) {
  }

  if (!this->is_replay) {
    for (const auto& it : json.get("Episode3LobbyBanners", JSON::list()).as_list()) {
      Image img("system/ep3/banners/" + it->at(2).as_string());
      string gvm = encode_gvm(img, img.get_has_alpha() ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565);
      if (gvm.size() > 0x37000) {
        throw runtime_error(string_printf("banner %s is too large (0x%zX bytes; maximum size is 0x37000 bytes)", it->at(2).as_string().c_str(), gvm.size()));
      }
      string compressed = prs_compress_optimal(gvm.data(), gvm.size());
      if (compressed.size() > 0x3800) {
        throw runtime_error(string_printf("banner %s cannot be compressed small enough (0x%zX bytes; maximum size is 0x3800 bytes compressed)", it->at(2).as_string().c_str(), compressed.size()));
      }
      config_log.info("Loaded Episode 3 lobby banner %s (0x%zX -> 0x%zX bytes)", it->at(2).as_string().c_str(), gvm.size(), compressed.size());
      this->ep3_lobby_banners.emplace_back(
          Ep3LobbyBannerEntry{.type = static_cast<uint32_t>(it->at(0).as_int()),
              .which = static_cast<uint32_t>(it->at(1).as_int()),
              .data = std::move(compressed)});
    }
  }

  {
    auto parse_ep3_ex_result_cmd = [&](const JSON& src) -> shared_ptr<G_SetEXResultValues_GC_Ep3_6xB4x4B> {
      auto ret = make_shared<G_SetEXResultValues_GC_Ep3_6xB4x4B>();
      const auto& win_json = src.at("Win");
      for (size_t z = 0; z < min<size_t>(win_json.size(), 10); z++) {
        ret->win_entries[z].threshold = win_json.at(z).at(0).as_int();
        ret->win_entries[z].value = win_json.at(z).at(1).as_int();
      }
      const auto& lose_json = src.at("Lose");
      for (size_t z = 0; z < min<size_t>(lose_json.size(), 10); z++) {
        ret->lose_entries[z].threshold = lose_json.at(z).at(0).as_int();
        ret->lose_entries[z].value = lose_json.at(z).at(1).as_int();
      }
      return ret;
    };
    const auto& categories_json = json.at("Episode3EXResultValues");
    this->ep3_default_ex_values = parse_ep3_ex_result_cmd(categories_json.at("Default"));
    try {
      this->ep3_tournament_ex_values = parse_ep3_ex_result_cmd(categories_json.at("Tournament"));
    } catch (const out_of_range&) {
      this->ep3_tournament_ex_values = this->ep3_default_ex_values;
    }
    try {
      this->ep3_tournament_ex_values = parse_ep3_ex_result_cmd(categories_json.at("TournamentFinalMatch"));
    } catch (const out_of_range&) {
      this->ep3_tournament_final_round_ex_values = this->ep3_tournament_ex_values;
    }
  }

  try {
    this->quest_F95E_results.clear();
    for (const auto& type_it : json.get_list("QuestF95EResultItems")) {
      auto& type_res = this->quest_F95E_results.emplace_back();
      for (const auto& difficulty_it : type_it->as_list()) {
        auto& difficulty_res = type_res.emplace_back();
        for (const auto& item_it : difficulty_it->as_list()) {
          difficulty_res.emplace_back(this->item_name_index->parse_item_description(Version::BB_V4, item_it->as_string()));
        }
      }
    }
  } catch (const out_of_range&) {
  }
  try {
    this->quest_F95F_results.clear();
    for (const auto& it : json.get_list("QuestF95FResultItems")) {
      auto& list = it->as_list();
      size_t price = list.at(0)->as_int();
      this->quest_F95F_results.emplace_back(make_pair(price, this->item_name_index->parse_item_description(Version::BB_V4, list.at(1)->as_string())));
    }
  } catch (const out_of_range&) {
  }
  try {
    this->quest_F960_success_results.clear();
    this->quest_F960_failure_results = QuestF960Result(json.at("QuestF960FailureResultItems"), this->item_name_index);
    for (const auto& it : json.get_list("QuestF960SuccessResultItems")) {
      this->quest_F960_success_results.emplace_back(*it, this->item_name_index);
    }
  } catch (const out_of_range&) {
  }
  try {
    this->secret_lottery_results.clear();
    for (const auto& it : json.get_list("SecretLotteryResultItems")) {
      this->secret_lottery_results.emplace_back(this->item_name_index->parse_item_description(Version::BB_V4, it->as_string()));
    }
  } catch (const out_of_range&) {
  }

  this->bb_global_exp_multiplier = json.get_int("BBGlobalEXPMultiplier", this->bb_global_exp_multiplier);

  set_log_levels_from_json(json.get("LogLevels", JSON::dict()));

  if (!is_reload) {
    try {
      this->run_shell_behavior = json.at("RunInteractiveShell").as_bool()
          ? ServerState::RunShellBehavior::ALWAYS
          : ServerState::RunShellBehavior::NEVER;
    } catch (const out_of_range&) {
    }
  }

  this->allow_dc_pc_games = json.get_bool("AllowDCPCGames", this->allow_dc_pc_games);
  this->allow_gc_xb_games = json.get_bool("AllowGCXBGames", this->allow_gc_xb_games);

  try {
    auto v = json.at("LobbyEvent");
    uint8_t event = v.is_int() ? v.as_int() : event_for_name(v.as_string());
    this->pre_lobby_event = event;
    for (const auto& l : this->all_lobbies()) {
      l->event = event;
    }
  } catch (const out_of_range&) {
  }

  this->ep3_menu_song = json.get_int("Episode3MenuSong", this->ep3_menu_song);

  if (!is_reload) {
    try {
      this->quest_category_index = make_shared<QuestCategoryIndex>(json.at("QuestCategories"));
    } catch (const exception& e) {
      throw runtime_error(string_printf(
          "QuestCategories is missing or invalid in config.json (%s) - see config.example.json for an example", e.what()));
    }
  }

  config_log.info("Creating menus");

  auto information_menu_v2 = make_shared<Menu>(MenuID::INFORMATION, "Information");
  auto information_menu_v3 = make_shared<Menu>(MenuID::INFORMATION, "Information");
  shared_ptr<vector<string>> information_contents_v2 = make_shared<vector<string>>();
  shared_ptr<vector<string>> information_contents_v3 = make_shared<vector<string>>();

  information_menu_v2->items.emplace_back(InformationMenuItemID::GO_BACK, "Go back",
      "Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  information_menu_v3->items.emplace_back(InformationMenuItemID::GO_BACK, "Go back",
      "Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  {
    auto blank_json = JSON::list();
    const JSON& default_json = json.get("InformationMenuContents", blank_json);
    const JSON& v2_json = json.get("InformationMenuContentsV1V2", default_json);
    const JSON& v3_json = json.get("InformationMenuContentsV3", default_json);

    uint32_t item_id = 0;
    for (const auto& item : v2_json.as_list()) {
      string name = item->get_string(0);
      string short_desc = item->get_string(1);
      information_menu_v2->items.emplace_back(item_id, name, short_desc, 0);
      information_contents_v2->emplace_back(item->get_string(2));
      item_id++;
    }

    item_id = 0;
    for (const auto& item : v3_json.as_list()) {
      string name = item->get_string(0);
      string short_desc = item->get_string(1);
      information_menu_v3->items.emplace_back(item_id, name, short_desc, MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
      information_contents_v3->emplace_back(item->get_string(2));
      item_id++;
    }
  }
  this->information_menu_v2 = information_menu_v2;
  this->information_menu_v3 = information_menu_v3;
  this->information_contents_v2 = information_contents_v2;
  this->information_contents_v3 = information_contents_v3;

  auto generate_proxy_destinations_menu = [&](vector<pair<string, uint16_t>>& ret_pds, const char* key) -> shared_ptr<const Menu> {
    auto ret = make_shared<Menu>(MenuID::PROXY_DESTINATIONS, "Proxy server");
    ret_pds.clear();

    try {
      map<string, const JSON&> sorted_jsons;
      for (const auto& it : json.at(key).as_dict()) {
        sorted_jsons.emplace(it.first, *it.second);
      }

      ret->items.emplace_back(ProxyDestinationsMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);
      ret->items.emplace_back(ProxyDestinationsMenuItemID::OPTIONS, "Options", "Set proxy session\noptions", 0);

      uint32_t item_id = 0;
      for (const auto& item : sorted_jsons) {
        const string& netloc_str = item.second.as_string();
        const string& description = "$C7Remote server:\n$C6" + netloc_str;
        ret->items.emplace_back(item_id, item.first, description, 0);
        ret_pds.emplace_back(parse_netloc(netloc_str));
        item_id++;
      }
    } catch (const out_of_range&) {
    }
    return ret;
  };

  this->proxy_destinations_menu_dc = generate_proxy_destinations_menu(this->proxy_destinations_dc, "ProxyDestinations-DC");
  this->proxy_destinations_menu_pc = generate_proxy_destinations_menu(this->proxy_destinations_pc, "ProxyDestinations-PC");
  this->proxy_destinations_menu_gc = generate_proxy_destinations_menu(this->proxy_destinations_gc, "ProxyDestinations-GC");
  this->proxy_destinations_menu_xb = generate_proxy_destinations_menu(this->proxy_destinations_xb, "ProxyDestinations-XB");

  try {
    const string& netloc_str = json.get_string("ProxyDestination-Patch");
    this->proxy_destination_patch = parse_netloc(netloc_str);
    config_log.info("Patch server proxy is enabled with destination %s", netloc_str.c_str());
    for (auto& it : this->name_to_port_config) {
      if (is_patch(it.second->version)) {
        it.second->behavior = ServerBehavior::PROXY_SERVER;
      }
    }
  } catch (const out_of_range&) {
    this->proxy_destination_patch.first = "";
    this->proxy_destination_patch.second = 0;
  }
  try {
    const string& netloc_str = json.get_string("ProxyDestination-BB");
    this->proxy_destination_bb = parse_netloc(netloc_str);
    config_log.info("BB proxy is enabled with destination %s", netloc_str.c_str());
    for (auto& it : this->name_to_port_config) {
      if (it.second->version == Version::BB_V4) {
        it.second->behavior = ServerBehavior::PROXY_SERVER;
      }
    }
  } catch (const out_of_range&) {
    this->proxy_destination_bb.first = "";
    this->proxy_destination_bb.second = 0;
  }

  this->welcome_message = json.get_string("WelcomeMessage", "");
  this->pc_patch_server_message = json.get_string("PCPatchServerMessage", "");
  this->bb_patch_server_message = json.get_string("BBPatchServerMessage", "");

  try {
    this->team_reward_defs_json = std::move(json.at("TeamRewards"));
  } catch (const out_of_range&) {
  }

  for (size_t z = 0; z < 4; z++) {
    shared_ptr<const Map::RareEnemyRates> prev = Map::DEFAULT_RARE_ENEMIES;
    try {
      string key = "RareEnemyRates-";
      key += token_name_for_difficulty(z);
      this->rare_enemy_rates_by_difficulty[z] = make_shared<Map::RareEnemyRates>(json.at(key));
      prev = this->rare_enemy_rates_by_difficulty[z];
    } catch (const out_of_range&) {
      this->rare_enemy_rates_by_difficulty[z] = prev;
    }
  }
  try {
    this->rare_enemy_rates_challenge = make_shared<Map::RareEnemyRates>(json.at("RareEnemyRates-Challenge"));
  } catch (const out_of_range&) {
    this->rare_enemy_rates_challenge = Map::DEFAULT_RARE_ENEMIES;
  }

  this->min_levels_v4[0] = DEFAULT_MIN_LEVELS_V4_EP1;
  this->min_levels_v4[1] = DEFAULT_MIN_LEVELS_V4_EP2;
  this->min_levels_v4[2] = DEFAULT_MIN_LEVELS_V4_EP4;
  try {
    for (const auto& ep_it : json.get_dict("BBMinimumLevels")) {
      array<size_t, 4> levels({0, 0, 0, 0});
      for (size_t z = 0; z < 4; z++) {
        levels[z] = ep_it.second->get_int(z) - 1;
      }
      switch (episode_for_token_name(ep_it.first)) {
        case Episode::EP1:
          this->min_levels_v4[0] = levels;
          break;
        case Episode::EP2:
          this->min_levels_v4[1] = levels;
          break;
        case Episode::EP4:
          this->min_levels_v4[2] = levels;
          break;
        default:
          throw runtime_error("unknown episode");
      }
    }
  } catch (const out_of_range&) {
  }
}

void ServerState::load_bb_private_keys() {
  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    this->bb_private_keys.emplace_back(make_shared<PSOBBEncryption::KeyFile>(
        load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.info("Loaded Blue Burst key file: %s", filename.c_str());
  }
  config_log.info("%zu Blue Burst key file(s) loaded", this->bb_private_keys.size());
}

void ServerState::load_licenses() {
  config_log.info("Indexing licenses");
  this->license_index = this->is_replay ? make_shared<LicenseIndex>() : make_shared<DiskLicenseIndex>();
}

void ServerState::load_teams() {
  config_log.info("Indexing teams");
  this->team_index = make_shared<TeamIndex>("system/teams", this->team_reward_defs_json);
  this->team_reward_defs_json = nullptr;
}

void ServerState::load_patch_indexes() {
  if (isdir("system/patch-pc")) {
    config_log.info("Indexing PSO PC patch files");
    this->pc_patch_file_index = make_shared<PatchFileIndex>("system/patch-pc");
  } else {
    config_log.info("PSO PC patch files not present");
  }
  if (isdir("system/patch-bb")) {
    config_log.info("Indexing PSO BB patch files");
    this->bb_patch_file_index = make_shared<PatchFileIndex>("system/patch-bb");
    try {
      auto gsl_file = this->bb_patch_file_index->get("./data/data.gsl");
      this->bb_data_gsl = make_shared<GSLArchive>(gsl_file->load_data(), false);
      config_log.info("data.gsl found in BB patch files");
    } catch (const out_of_range&) {
      config_log.info("data.gsl is not present in BB patch files");
    }
  } else {
    config_log.info("PSO BB patch files not present");
  }
}

void ServerState::load_battle_params() {
  config_log.info("Loading battle parameters");
  this->battle_params = make_shared<BattleParamsIndex>(
      this->load_bb_file("BattleParamEntry_on.dat"),
      this->load_bb_file("BattleParamEntry_lab_on.dat"),
      this->load_bb_file("BattleParamEntry_ep4_on.dat"),
      this->load_bb_file("BattleParamEntry.dat"),
      this->load_bb_file("BattleParamEntry_lab.dat"),
      this->load_bb_file("BattleParamEntry_ep4.dat"));
}

void ServerState::load_level_table() {
  config_log.info("Loading level table");
  this->level_table = make_shared<LevelTableV4>(*this->load_bb_file("PlyLevelTbl.prs"), true);
}

shared_ptr<WordSelectTable> ServerState::load_word_select_table_from_system() {
  vector<vector<string>> name_alias_lists;
  auto json = JSON::parse(load_file("system/word-select/name-alias-lists.json"));
  for (const auto& coll_it : json.as_list()) {
    auto& coll = name_alias_lists.emplace_back();
    for (const auto& str_it : coll_it->as_list()) {
      coll.emplace_back(str_it->as_string());
    }
  }

  config_log.info("(Word select) Loading pc_unitxt.prs");
  vector<vector<string>> pc_unitxt_data = parse_unicode_text_set(load_file("system/word-select/pc_unitxt.prs"));
  config_log.info("(Word select) Loading bb_unitxt_ws.prs");
  vector<vector<string>> bb_unitxt_data = parse_unicode_text_set(load_file("system/word-select/bb_unitxt_ws.prs"));
  vector<string> pc_unitxt_collection = std::move(pc_unitxt_data.at(35));
  vector<string> bb_unitxt_collection = std::move(bb_unitxt_data.at(0));

  config_log.info("(Word select) Loading DC_NTE data");
  WordSelectSet dc_nte_ws(load_file("system/word-select/dc_nte_ws_data.bin"), Version::DC_NTE, nullptr, true);
  config_log.info("(Word select) Loading DC_V1_11_2000_PROTOTYPE data");
  WordSelectSet dc_112000_ws(load_file("system/word-select/dc_112000_ws_data.bin"), Version::DC_V1_11_2000_PROTOTYPE, nullptr, false);
  config_log.info("(Word select) Loading DC_V1 data");
  WordSelectSet dc_v1_ws(load_file("system/word-select/dcv1_ws_data.bin"), Version::DC_V1, nullptr, false);
  config_log.info("(Word select) Loading DC_V2 data");
  WordSelectSet dc_v2_ws(load_file("system/word-select/dcv2_ws_data.bin"), Version::DC_V2, nullptr, false);
  config_log.info("(Word select) Loading PC_NTE data");
  WordSelectSet pc_nte_ws(load_file("system/word-select/pc_nte_ws_data.bin"), Version::PC_NTE, &pc_unitxt_collection, false);
  config_log.info("(Word select) Loading PC_V2 data");
  WordSelectSet pc_v2_ws(load_file("system/word-select/pc_ws_data.bin"), Version::PC_V2, &pc_unitxt_collection, false);
  config_log.info("(Word select) Loading GC_NTE data");
  WordSelectSet gc_nte_ws(load_file("system/word-select/gc_nte_ws_data.bin"), Version::GC_NTE, nullptr, false);
  config_log.info("(Word select) Loading GC_V3 data");
  WordSelectSet gc_v3_ws(load_file("system/word-select/gc_ws_data.bin"), Version::GC_V3, nullptr, false);
  config_log.info("(Word select) Loading GC_EP3_NTE data");
  WordSelectSet gc_ep3_nte_ws(load_file("system/word-select/gc_ep3_nte_ws_data.bin"), Version::GC_EP3_NTE, nullptr, false);
  config_log.info("(Word select) Loading GC_EP3 data");
  WordSelectSet gc_ep3_ws(load_file("system/word-select/gc_ep3_ws_data.bin"), Version::GC_EP3, nullptr, false);
  config_log.info("(Word select) Loading XB_V3 data");
  WordSelectSet xb_v3_ws(load_file("system/word-select/xb_ws_data.bin"), Version::XB_V3, nullptr, false);
  config_log.info("(Word select) Loading BB_V4 data");
  WordSelectSet bb_v4_ws(load_file("system/word-select/bb_ws_data.bin"), Version::BB_V4, &bb_unitxt_collection, false);

  config_log.info("(Word select) Generating table");
  return make_shared<WordSelectTable>(
      dc_nte_ws, dc_112000_ws, dc_v1_ws, dc_v2_ws,
      pc_nte_ws, pc_v2_ws, gc_nte_ws, gc_v3_ws,
      gc_ep3_nte_ws, gc_ep3_ws, xb_v3_ws, bb_v4_ws,
      name_alias_lists);
}

void ServerState::load_word_select_table() {
  config_log.info("Loading Word Select table");
  this->word_select_table = this->load_word_select_table_from_system();
}

void ServerState::load_item_name_index() {
  config_log.info("Loading item name index");
  this->item_name_index = make_shared<ItemNameIndex>(
      JSON::parse(load_file("system/item-tables/names-v2.json")),
      JSON::parse(load_file("system/item-tables/names-v3.json")),
      JSON::parse(load_file("system/item-tables/names-v4.json")));
}

void ServerState::load_item_tables() {
  config_log.info("Loading rare item sets");
  unordered_map<string, shared_ptr<const RareItemSet>> new_rare_item_sets;
  for (const auto& filename : list_directory_sorted("system/item-tables")) {
    if (!starts_with(filename, "rare-table-")) {
      continue;
    }

    string path = "system/item-tables/" + filename;
    size_t ext_offset = filename.rfind('.');
    string basename = (ext_offset == string::npos) ? filename : filename.substr(0, ext_offset);

    if (ends_with(filename, "-v1.json")) {
      config_log.info("Loading v1 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), Version::DC_V1, this->item_name_index));
    } else if (ends_with(filename, "-v2.json")) {
      config_log.info("Loading v2 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), Version::PC_V2, this->item_name_index));
    } else if (ends_with(filename, "-v3.json")) {
      config_log.info("Loading v3 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), Version::GC_V3, this->item_name_index));
    } else if (ends_with(filename, "-v4.json")) {
      config_log.info("Loading v4 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), Version::BB_V4, this->item_name_index));

    } else if (ends_with(filename, ".afs")) {
      config_log.info("Loading AFS rare item table %s", filename.c_str());
      auto data = make_shared<string>(load_file(path));
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(AFSArchive(data), false));

    } else if (ends_with(filename, ".gsl")) {
      config_log.info("Loading GSL rare item table %s", filename.c_str());
      auto data = make_shared<string>(load_file(path));
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(GSLArchive(data, false), false));

    } else if (ends_with(filename, ".gslb")) {
      config_log.info("Loading GSL rare item table %s", filename.c_str());
      auto data = make_shared<string>(load_file(path));
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(GSLArchive(data, true), true));

    } else if (ends_with(filename, ".rel")) {
      config_log.info("Loading REL rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(load_file(path), true));
    }
  }

  if (!new_rare_item_sets.count("rare-table-v4")) {
    config_log.info("rare-table-v4 rare item set is not available; loading from BB data");
    new_rare_item_sets.emplace("rare-table-v4", make_shared<RareItemSet>(load_file("system/blueburst/ItemRT.rel"), true));
  }
  this->rare_item_sets.swap(new_rare_item_sets);

  config_log.info("Loading v2 common item table");
  auto ct_data_v2 = make_shared<string>(load_file("system/item-tables/ItemCT-v2.afs"));
  auto pt_data_v2 = make_shared<string>(load_file("system/item-tables/ItemPT-v2.afs"));
  this->common_item_set_v2 = make_shared<AFSV2CommonItemSet>(pt_data_v2, ct_data_v2);
  config_log.info("Loading v3+v4 common item table");
  auto pt_data_v3_v4 = make_shared<string>(load_file("system/item-tables/ItemPT-gc-v4.gsl"));
  this->common_item_set_v3_v4 = make_shared<GSLV3V4CommonItemSet>(pt_data_v3_v4, true);

  config_log.info("Loading armor table");
  auto armor_data = make_shared<string>(load_file("system/item-tables/ArmorRandom-gc.rel"));
  this->armor_random_set = make_shared<ArmorRandomSet>(armor_data);

  config_log.info("Loading tool table");
  auto tool_data = make_shared<string>(load_file("system/item-tables/ToolRandom-gc.rel"));
  this->tool_random_set = make_shared<ToolRandomSet>(tool_data);

  config_log.info("Loading weapon tables");
  const char* filenames[4] = {
      "system/item-tables/WeaponRandomNormal-gc.rel",
      "system/item-tables/WeaponRandomHard-gc.rel",
      "system/item-tables/WeaponRandomVeryHard-gc.rel",
      "system/item-tables/WeaponRandomUltimate-gc.rel",
  };
  for (size_t z = 0; z < 4; z++) {
    auto weapon_data = make_shared<string>(load_file(filenames[z]));
    this->weapon_random_sets[z] = make_shared<WeaponRandomSet>(weapon_data);
  }

  config_log.info("Loading tekker adjustment table");
  auto tekker_data = make_shared<string>(load_file("system/item-tables/JudgeItem-gc.rel"));
  this->tekker_adjustment_set = make_shared<TekkerAdjustmentSet>(tekker_data);

  config_log.info("Loading item definition tables");
  auto pmt_data_v2 = make_shared<string>(prs_decompress(load_file("system/item-tables/ItemPMT-v2.prs")));
  this->item_parameter_table_v2 = make_shared<ItemParameterTable>(pmt_data_v2, ItemParameterTable::Version::V2);
  auto pmt_data_v3 = make_shared<string>(prs_decompress(load_file("system/item-tables/ItemPMT-gc.prs")));
  this->item_parameter_table_v3 = make_shared<ItemParameterTable>(pmt_data_v3, ItemParameterTable::Version::V3);
  auto pmt_data_v4 = make_shared<string>(prs_decompress(load_file("system/item-tables/ItemPMT-bb.prs")));
  this->item_parameter_table_v4 = make_shared<ItemParameterTable>(pmt_data_v4, ItemParameterTable::Version::V4);

  config_log.info("Loading mag evolution table");
  auto mag_data = make_shared<string>(prs_decompress(load_file("system/item-tables/ItemMagEdit-bb.prs")));
  this->mag_evolution_table = make_shared<MagEvolutionTable>(mag_data);
}

void ServerState::load_ep3_data() {
  config_log.info("Collecting Episode 3 maps");
  this->ep3_map_index = make_shared<Episode3::MapIndex>("system/ep3/maps");
  config_log.info("Loading Episode 3 card definitions");
  this->ep3_card_index = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions.mnr",
      "system/ep3/card-definitions.mnrd",
      "system/ep3/card-text.mnr",
      "system/ep3/card-text.mnrd",
      "system/ep3/card-dice-text.mnr",
      "system/ep3/card-dice-text.mnrd");
  config_log.info("Loading Episode 3 trial card definitions");
  this->ep3_card_index_trial = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions-trial.mnr",
      "system/ep3/card-definitions-trial.mnrd",
      "system/ep3/card-text-trial.mnr",
      "system/ep3/card-text-trial.mnrd",
      "system/ep3/card-dice-text-trial.mnr",
      "system/ep3/card-dice-text-trial.mnrd");
  config_log.info("Loading Episode 3 COM decks");
  this->ep3_com_deck_index = make_shared<Episode3::COMDeckIndex>("system/ep3/com-decks.json");

  const string& tournament_state_filename = "system/ep3/tournament-state.json";
  this->ep3_tournament_index = make_shared<Episode3::TournamentIndex>(
      this->ep3_map_index, this->ep3_com_deck_index, tournament_state_filename);
  this->ep3_tournament_index->link_all_clients(this->shared_from_this());
  config_log.info("Loaded Episode 3 tournament state");
}

void ServerState::resolve_ep3_card_names() {
  config_log.info("Resolving Episode 3 card names");
  for (auto& e : this->ep3_card_auction_pool) {
    try {
      const auto& card = this->ep3_card_index->definition_for_name_normalized(e.card_name);
      e.card_id = card->def.card_id;
    } catch (const out_of_range&) {
      throw runtime_error(string_printf("Ep3 card \"%s\" in auction pool does not exist", e.card_name.c_str()));
    }
  }

  for (size_t z = 0; z < this->ep3_trap_card_ids.size(); z++) {
    auto& ids = this->ep3_trap_card_ids[z];
    ids.clear();
    if (z < this->ep3_trap_card_names.size()) {
      auto& names = this->ep3_trap_card_names[z];
      for (const auto& name : names) {
        try {
          const auto& card = this->ep3_card_index->definition_for_name_normalized(name);
          if (card->def.type != Episode3::CardType::ASSIST) {
            throw runtime_error(string_printf("Ep3 card \"%s\" in trap card list is not an assist card", name.c_str()));
          }
          ids.emplace_back(card->def.card_id);
        } catch (const out_of_range&) {
          throw runtime_error(string_printf("Ep3 card \"%s\" in trap card list does not exist", name.c_str()));
        }
      }
    }
  }
}

void ServerState::load_quest_index() {
  config_log.info("Collecting quests");
  this->default_quest_index = make_shared<QuestIndex>("system/quests", this->quest_category_index, false);
  config_log.info("Collecting Episode 3 download quests");
  this->ep3_download_quest_index = make_shared<QuestIndex>("system/ep3/maps-download", this->quest_category_index, true);
}

void ServerState::compile_functions() {
  config_log.info("Compiling client functions");
  this->function_code_index = make_shared<FunctionCodeIndex>("system/ppc");
}

void ServerState::load_dol_files() {
  config_log.info("Loading DOL files");
  this->dol_file_index = make_shared<DOLFileIndex>("system/dol");
}

shared_ptr<const vector<string>> ServerState::information_contents_for_client(shared_ptr<const Client> c) const {
  return is_v1_or_v2(c->version()) ? this->information_contents_v2 : this->information_contents_v3;
}

shared_ptr<const QuestIndex> ServerState::quest_index_for_version(Version version) const {
  return is_ep3(version) ? this->ep3_download_quest_index : this->default_quest_index;
}

void ServerState::dispatch_destroy_lobbies(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<ServerState*>(ctx)->lobbies_to_destroy.clear();
}
