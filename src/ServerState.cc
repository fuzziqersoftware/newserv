#include "ServerState.hh"

#include <string.h>

#include <memory>
#include <phosg/Image.hh>
#include <phosg/Network.hh>

#include "Compression.hh"
#include "EventUtils.hh"
#include "FileContentsCache.hh"
#include "GVMEncoder.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "Text.hh"
#include "TextIndex.hh"

using namespace std;

ServerState::QuestF960Result::QuestF960Result(const JSON& json, std::shared_ptr<const ItemNameIndex> name_index) {
  static const array<string, 7> day_names = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  this->meseta_cost = json.get_int("MesetaCost", 0);
  this->base_probability = json.get_int("BaseProbability", 0);
  this->probability_upgrade = json.get_int("ProbabilityUpgrade", 0);
  for (size_t day = 0; day < 7; day++) {
    for (const auto& item_it : json.get_list(day_names[day])) {
      try {
        this->results[day].emplace_back(name_index->parse_item_description(item_it->as_string()));
      } catch (const exception& e) {
        config_log.warning("Cannot parse item description \"%s\": %s (skipping entry)", item_it->as_string().c_str(), e.what());
      }
    }
  }
}

ServerState::ServerState(const string& config_filename)
    : creation_time(now()),
      config_filename(config_filename) {}

ServerState::ServerState(shared_ptr<struct event_base> base, const string& config_filename, bool is_replay)
    : creation_time(now()),
      base(base),
      config_filename(config_filename),
      is_replay(is_replay),
      player_files_manager(this->base ? make_shared<PlayerFilesManager>(base) : nullptr),
      destroy_lobbies_event(this->base ? event_new(base.get(), -1, EV_TIMEOUT, &ServerState::dispatch_destroy_lobbies, this) : nullptr, event_free) {}

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
    for (const auto& lobby_id : this->public_lobby_search_order(c)) {
      try {
        auto l = this->find_lobby(lobby_id);
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

shared_ptr<Client> ServerState::find_client(const string* identifier, uint64_t account_id, shared_ptr<Lobby> l) {
  // WARNING: There are multiple callsites where we assume this function never
  // returns a client that isn't in any lobby. If this behavior changes, we will
  // need to audit all callsites to ensure correctness.

  if ((account_id == 0) && identifier) {
    try {
      account_id = stoull(*identifier, nullptr, 0);
    } catch (const exception&) {
    }
  }

  if (l) {
    try {
      return l->find_client(identifier, account_id);
    } catch (const out_of_range&) {
    }
  }

  for (auto& other_l : this->all_lobbies()) {
    if (l == other_l) {
      continue; // don't bother looking again
    }
    try {
      return other_l->find_client(identifier, account_id);
    } catch (const out_of_range&) {
    }
  }

  throw out_of_range("client not found");
}

uint32_t ServerState::connect_address_for_client(shared_ptr<Client> c) const {
  if (c->channel.virtual_network_id) {
    if (c->channel.remote_addr.ss_family != AF_INET) {
      throw logic_error("virtual connection is missing remote IPv4 address");
    }
    const auto* sin = reinterpret_cast<const sockaddr_in*>(&c->channel.remote_addr);
    return IPStackSimulator::connect_address_for_remote_address(ntohl(sin->sin_addr.s_addr));
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

shared_ptr<const Menu> ServerState::information_menu(Version version) const {
  if (is_v1_or_v2(version)) {
    return this->information_menu_v2;
  } else if (is_v3(version)) {
    return this->information_menu_v3;
  }
  throw out_of_range("no information menu exists for this version");
}

shared_ptr<const Menu> ServerState::proxy_destinations_menu(Version version) const {
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

const vector<pair<string, uint16_t>>& ServerState::proxy_destinations(Version version) const {
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

const vector<uint32_t>& ServerState::public_lobby_search_order(Version version, bool is_client_customization) const {
  static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the public lobby search orders in config.json");
  if (is_client_customization && !this->client_customization_public_lobby_search_order.empty()) {
    return this->client_customization_public_lobby_search_order;
  }
  return this->public_lobby_search_orders.at(static_cast<size_t>(version));
}

shared_ptr<const vector<string>> ServerState::information_contents_for_client(shared_ptr<const Client> c) const {
  return is_v1_or_v2(c->version()) ? this->information_contents_v2 : this->information_contents_v3;
}

shared_ptr<const QuestIndex> ServerState::quest_index(Version version) const {
  return is_ep3(version) ? this->ep3_download_quest_index : this->default_quest_index;
}

size_t ServerState::default_min_level_for_game(Version version, Episode episode, uint8_t difficulty) const {
  // A player's actual level is their displayed level - 1, so the minimums for
  // Episode 1 (for example) are actually 1, 20, 40, 80.
  switch (episode) {
    case Episode::EP1: {
      const auto& min_levels = (version == Version::BB_V4) ? this->min_levels_v4[0] : DEFAULT_MIN_LEVELS_V3;
      return min_levels.at(difficulty);
    }
    case Episode::EP2: {
      const auto& min_levels = (version == Version::BB_V4) ? this->min_levels_v4[1] : DEFAULT_MIN_LEVELS_V3;
      return min_levels.at(difficulty);
    }
    case Episode::EP3:
      return 0;
    case Episode::EP4: {
      const auto& min_levels = (version == Version::BB_V4) ? this->min_levels_v4[2] : DEFAULT_MIN_LEVELS_V3;
      return min_levels.at(difficulty);
    }
    default:
      throw runtime_error("invalid episode");
  }
}

void ServerState::dispatch_destroy_lobbies(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<ServerState*>(ctx)->lobbies_to_destroy.clear();
}

shared_ptr<const SetDataTableBase> ServerState::set_data_table(
    Version version, Episode episode, GameMode mode, uint8_t difficulty) const {
  bool use_ult_tables = ((episode == Episode::EP1) && (difficulty == 3) && !is_v1(version) && (version != Version::PC_NTE));
  if (mode == GameMode::SOLO && is_v4(version)) {
    return use_ult_tables ? this->bb_solo_set_data_table_ep1_ult : this->bb_solo_set_data_table;
  }

  const auto& tables = use_ult_tables ? this->set_data_tables_ep1_ult : this->set_data_tables;
  auto ret = tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw runtime_error("no set data table exists for this version");
  }
  return ret;
}

shared_ptr<const LevelTable> ServerState::level_table(Version version) const {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE: // TODO: Does NTE use the v2 table, the v3 table, or neither?
      return this->level_table_v1_v2;
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      return this->level_table_v3;
    case Version::BB_V4:
      return this->level_table_v4;
    default:
      throw logic_error("level table not available for version");
  }
}

shared_ptr<const ItemParameterTable> ServerState::item_parameter_table(Version version) const {
  auto ret = this->item_parameter_tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw runtime_error("no item parameter table exists for this version");
  }
  return ret;
}

shared_ptr<const ItemParameterTable> ServerState::item_parameter_table_for_encode(Version version) const {
  return this->item_parameter_table(is_v1(version) ? Version::PC_V2 : version);
}

shared_ptr<const ItemData::StackLimits> ServerState::item_stack_limits(Version version) const {
  auto ret = this->item_stack_limits_tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw runtime_error("no item stack limits table exists for this version");
  }
  return ret;
}

shared_ptr<const ItemNameIndex> ServerState::item_name_index_opt(Version version) const {
  return this->item_name_indexes.at(static_cast<size_t>(version));
}

shared_ptr<const ItemNameIndex> ServerState::item_name_index(Version version) const {
  auto ret = this->item_name_index_opt(version);
  if (ret == nullptr) {
    throw runtime_error("no item name index exists for this version");
  }
  return ret;
}

string ServerState::describe_item(Version version, const ItemData& item, bool include_color_codes) const {
  if (is_v1(version)) {
    ItemData encoded = item;
    encoded.encode_for_version(version, this->item_parameter_table(version));
    return this->item_name_index(version)->describe_item(encoded, include_color_codes);
  } else {
    return this->item_name_index(version)->describe_item(item, include_color_codes);
  }
}

ItemData ServerState::parse_item_description(Version version, const string& description) const {
  return this->item_name_index(version)->parse_item_description(description);
}

void ServerState::set_port_configuration(const vector<PortConfiguration>& port_configs) {
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
      return this->bb_patch_file_index->get(patch_index_path)->load_data();
    } catch (const out_of_range&) {
    }
  }

  if (this->bb_data_gsl) {
    // Second, look in the patch tree's data.gsl file
    const string& effective_gsl_filename = gsl_filename.empty() ? patch_index_filename : gsl_filename;
    try {
      // TODO: It's kinda not great that we copy the data here; find a way to
      // avoid doing this (also in the below case)
      return make_shared<string>(this->bb_data_gsl->get_copy(effective_gsl_filename));
    } catch (const out_of_range&) {
    }

    // Third, look in data.gsl without the filename extension
    size_t dot_offset = effective_gsl_filename.rfind('.');
    if (dot_offset != string::npos) {
      string no_ext_gsl_filename = effective_gsl_filename.substr(0, dot_offset);
      try {
        return make_shared<string>(this->bb_data_gsl->get_copy(no_ext_gsl_filename));
      } catch (const out_of_range&) {
      }
    }
  }

  // Finally, look in system/blueburst
  const string& effective_bb_directory_filename = bb_directory_filename.empty() ? patch_index_filename : bb_directory_filename;
  static FileContentsCache cache(10 * 60 * 1000 * 1000); // 10 minutes
  try {
    auto ret = cache.get_or_load("system/blueburst/" + effective_bb_directory_filename);
    return ret.file->data;
  } catch (const exception& e) {
    throw cannot_open_file(patch_index_filename);
  }
}

shared_ptr<const string> ServerState::load_map_file(Version version, const string& filename) const {
  auto& cache = this->map_file_caches.at(static_cast<size_t>(version));
  return cache->get(filename, bind(&ServerState::load_map_file_uncached, this, version, placeholders::_1));
}

shared_ptr<const string> ServerState::load_map_file_uncached(Version version, const string& filename) const {
  if (version == Version::BB_V4) {
    try {
      return this->load_bb_file(filename);
    } catch (const exception& e) {
    }
  } else if (version == Version::PC_V2) {
    try {
      string path = "system/patch-pc/Media/PSO/" + filename;
      auto ret = make_shared<string>(load_file(path));
      return ret;
    } catch (const exception& e) {
    }
  }
  try {
    string path = string_printf("system/maps/%s/%s", file_path_token_for_version(version), filename.c_str());
    auto ret = make_shared<string>(load_file(path));
    return ret;
  } catch (const exception& e) {
  }
  return nullptr;
}

pair<string, uint16_t> ServerState::parse_port_spec(const JSON& json) const {
  if (json.is_list()) {
    string addr = json.at(0).as_string();
    try {
      addr = string_for_address(this->all_addresses.at(addr));
    } catch (const out_of_range&) {
    }
    return make_pair(addr, json.at(1).as_int());
  } else {
    return make_pair("", json.as_int());
  }
}

vector<PortConfiguration> ServerState::parse_port_configuration(const JSON& json) const {
  vector<PortConfiguration> ret;
  for (const auto& item_json_it : json.as_dict()) {
    const auto& item_list = item_json_it.second;
    PortConfiguration& pc = ret.emplace_back();
    pc.name = item_json_it.first;
    auto spec = this->parse_port_spec(item_list->at(0));
    pc.addr = std::move(spec.first);
    pc.port = spec.second;
    pc.version = enum_for_name<Version>(item_list->at(1).as_string().c_str());
    pc.behavior = enum_for_name<ServerBehavior>(item_list->at(2).as_string().c_str());
  }
  return ret;
}

void ServerState::collect_network_addresses() {
  config_log.info("Reading network addresses");
  this->all_addresses = get_local_addresses();
  for (const auto& it : this->all_addresses) {
    string addr_str = string_for_address(it.second);
    config_log.info("Found interface: %s = %s", it.first.c_str(), addr_str.c_str());
  }
}

void ServerState::load_config_early() {
  if (this->config_filename.empty()) {
    throw logic_error("configuration filename is missing");
  }

  config_log.info("Loading configuration");
  this->config_json = make_shared<JSON>(JSON::parse(load_file(this->config_filename)));

  auto parse_behavior_switch = [&](const string& json_key, BehaviorSwitch default_value) -> ServerState::BehaviorSwitch {
    try {
      string behavior = this->config_json->get_string(json_key);
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

  this->name = this->config_json->at("ServerName").as_string();

  if (!this->one_time_config_loaded) {
    try {
      this->username = this->config_json->at("User").as_string();
      if (this->username == "$SUDO_USER") {
        const char* user_from_env = getenv("SUDO_USER");
        if (!user_from_env) {
          throw runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
        }
        this->username = user_from_env;
      }
    } catch (const out_of_range&) {
    }

    this->set_port_configuration(parse_port_configuration(this->config_json->at("PortConfiguration")));
    try {
      auto spec = this->parse_port_spec(this->config_json->at("DNSServerPort"));
      this->dns_server_addr = std::move(spec.first);
      this->dns_server_port = spec.second;
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("IPStackListen").as_list()) {
        if (item->is_int()) {
          this->ip_stack_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->ip_stack_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("PPPStackListen").as_list()) {
        if (item->is_int()) {
          this->ppp_stack_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->ppp_stack_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("PPPRawListen").as_list()) {
        if (item->is_int()) {
          this->ppp_raw_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->ppp_raw_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("HTTPListen").as_list()) {
        if (item->is_int()) {
          this->http_addresses.emplace_back(string_printf("0.0.0.0:%" PRId64, item->as_int()));
        } else {
          this->http_addresses.emplace_back(item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }

    this->one_time_config_loaded = true;
  }

  auto local_address_str = this->config_json->at("LocalAddress").as_string();
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

  auto external_address_str = this->config_json->at("ExternalAddress").as_string();
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

  try {
    this->banned_ipv4_ranges = make_shared<IPV4RangeSet>(this->config_json->at("BannedIPV4Ranges"));
    this->disconnect_all_banned_clients();
  } catch (const out_of_range&) {
    this->banned_ipv4_ranges = make_shared<IPV4RangeSet>();
  }

  this->client_ping_interval_usecs = this->config_json->get_int("ClientPingInterval", 30000000);
  this->client_idle_timeout_usecs = this->config_json->get_int("ClientIdleTimeout", 60000000);
  this->patch_client_idle_timeout_usecs = this->config_json->get_int("PatchClientIdleTimeout", 300000000);

  this->ip_stack_debug = this->config_json->get_bool("IPStackDebug", false);
  this->allow_unregistered_users = this->config_json->get_bool("AllowUnregisteredUsers", false);
  this->allow_pc_nte = this->config_json->get_bool("AllowPCNTE", false);
  this->use_temp_accounts_for_prototypes = this->config_json->get_bool("UseTemporaryAccountsForPrototypes", true);
  this->notify_server_for_max_level_achieved = this->config_json->get_bool("NotifyServerForMaxLevelAchieved", false);
  this->allowed_drop_modes_v1_v2_normal = this->config_json->get_int("AllowedDropModesV1V2Normal", 0x1F);
  this->allowed_drop_modes_v1_v2_battle = this->config_json->get_int("AllowedDropModesV1V2Battle", 0x07);
  this->allowed_drop_modes_v1_v2_challenge = this->config_json->get_int("AllowedDropModesV1V2Challenge", 0x07);
  this->allowed_drop_modes_v3_normal = this->config_json->get_int("AllowedDropModesV3Normal", 0x1F);
  this->allowed_drop_modes_v3_battle = this->config_json->get_int("AllowedDropModesV3Battle", 0x07);
  this->allowed_drop_modes_v3_challenge = this->config_json->get_int("AllowedDropModesV3Challenge", 0x07);
  this->allowed_drop_modes_v4_normal = this->config_json->get_int("AllowedDropModesV4Normal", 0x1D);
  this->allowed_drop_modes_v4_battle = this->config_json->get_int("AllowedDropModesV4Battle", 0x05);
  this->allowed_drop_modes_v4_challenge = this->config_json->get_int("AllowedDropModesV4Challenge", 0x05);
  this->default_drop_mode_v1_v2_normal = this->config_json->get_enum("DefaultDropModeV1V2Normal", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v1_v2_battle = this->config_json->get_enum("DefaultDropModeV1V2Battle", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v1_v2_challenge = this->config_json->get_enum("DefaultDropModeV1V2Challenge", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v3_normal = this->config_json->get_enum("DefaultDropModeV3Normal", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v3_battle = this->config_json->get_enum("DefaultDropModeV3Battle", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v3_challenge = this->config_json->get_enum("DefaultDropModeV3Challenge", Lobby::DropMode::CLIENT);
  this->default_drop_mode_v4_normal = this->config_json->get_enum("DefaultDropModeV4Normal", Lobby::DropMode::SERVER_SHARED);
  this->default_drop_mode_v4_battle = this->config_json->get_enum("DefaultDropModeV4Battle", Lobby::DropMode::SERVER_SHARED);
  this->default_drop_mode_v4_challenge = this->config_json->get_enum("DefaultDropModeV4Challenge", Lobby::DropMode::SERVER_SHARED);
  if ((this->default_drop_mode_v4_normal == Lobby::DropMode::CLIENT) ||
      (this->default_drop_mode_v4_battle == Lobby::DropMode::CLIENT) ||
      (this->default_drop_mode_v4_challenge == Lobby::DropMode::CLIENT)) {
    throw runtime_error("default V4 drop mode cannot be CLIENT");
  }
  if ((this->allowed_drop_modes_v4_normal & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT))) ||
      (this->allowed_drop_modes_v4_battle & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT))) || (this->allowed_drop_modes_v4_challenge & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT)))) {
    throw runtime_error("CLIENT drop mode cannot be allowed in V4");
  }

  auto parse_quest_flag_rewrites = [&json = this->config_json](const char* key) -> std::unordered_map<uint16_t, IntegralExpression> {
    std::unordered_map<uint16_t, IntegralExpression> ret;
    try {
      for (const auto& it : json->get_dict(key)) {
        if (!it.first.starts_with("F_")) {
          throw runtime_error("invalid flag reference: " + it.first);
        }
        uint16_t flag = stoul(it.first.substr(2), nullptr, 16);
        if (it.second->is_bool()) {
          ret.emplace(flag, it.second->as_bool() ? "true" : "false");
        } else {
          ret.emplace(flag, it.second->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    return ret;
  };
  this->quest_flag_rewrites_v1_v2 = parse_quest_flag_rewrites("QuestFlagRewritesV1V2");
  this->quest_flag_rewrites_v3 = parse_quest_flag_rewrites("QuestFlagRewritesV3");
  this->quest_flag_rewrites_v4 = parse_quest_flag_rewrites("QuestFlagRewritesV4");

  this->quest_counter_fields.clear();
  try {
    for (const auto& it : this->config_json->get_dict("QuestCounterFields")) {
      const auto& def = it.second->as_list();
      this->quest_counter_fields.emplace(it.first, make_pair(def.at(0)->as_int(), def.at(1)->as_int()));
    }
  } catch (const out_of_range&) {
  }

  this->persistent_game_idle_timeout_usecs = this->config_json->get_int("PersistentGameIdleTimeout", 0);
  this->cheat_mode_behavior = parse_behavior_switch("CheatModeBehavior", BehaviorSwitch::OFF_BY_DEFAULT);
  this->default_switch_assist_enabled = this->config_json->get_bool("EnableSwitchAssistByDefault", false);
  this->use_game_creator_section_id = this->config_json->get_bool("UseGameCreatorSectionID", false);
  this->rare_notifs_enabled_for_client_drops = this->config_json->get_bool("RareNotificationsEnabledForClientDrops", false);
  this->default_rare_notifs_enabled_v1_v2 = this->config_json->get_bool("RareNotificationsEnabledByDefault", false);
  this->default_rare_notifs_enabled_v3_v4 = this->default_rare_notifs_enabled_v1_v2;
  this->default_rare_notifs_enabled_v1_v2 = this->config_json->get_bool("RareNotificationsEnabledByDefaultV1V2", this->default_rare_notifs_enabled_v1_v2);
  this->default_rare_notifs_enabled_v3_v4 = this->config_json->get_bool("RareNotificationsEnabledByDefaultV3V4", this->default_rare_notifs_enabled_v3_v4);
  this->enable_send_function_call_quest_num = this->config_json->get_int("EnableSendFunctionCallQuestNumber", -1);
  this->enable_v3_v4_protected_subcommands = this->config_json->get_bool("EnableV3V4ProtectedSubcommands", false);
  this->catch_handler_exceptions = this->config_json->get_bool("CatchHandlerExceptions", true);

  auto parse_int_list = +[](const JSON& json) -> vector<uint32_t> {
    vector<uint32_t> ret;
    for (const auto& item : json.as_list()) {
      ret.emplace_back(item->as_int());
    }
    return ret;
  };

  this->ep3_infinite_meseta = this->config_json->get_bool("Episode3InfiniteMeseta", false);
  try {
    this->ep3_defeat_player_meseta_rewards = parse_int_list(this->config_json->at("Episode3DefeatPlayerMeseta"));
  } catch (const out_of_range&) {
    this->ep3_defeat_player_meseta_rewards = {300, 400, 500, 600, 700};
  }
  try {
    this->ep3_defeat_com_meseta_rewards = parse_int_list(this->config_json->get("Episode3DefeatCOMMeseta", JSON::list()));
  } catch (const out_of_range&) {
    this->ep3_defeat_com_meseta_rewards = {100, 200, 300, 400, 500};
  }
  this->ep3_final_round_meseta_bonus = this->config_json->get_int("Episode3FinalRoundMesetaBonus", 300);
  this->ep3_jukebox_is_free = this->config_json->get_bool("Episode3JukeboxIsFree", false);
  this->ep3_behavior_flags = this->config_json->get_int("Episode3BehaviorFlags", 0);
  this->ep3_card_auction_points = this->config_json->get_int("CardAuctionPoints", 0);
  this->hide_download_commands = this->config_json->get_bool("HideDownloadCommands", true);
  this->proxy_allow_save_files = this->config_json->get_bool("ProxyAllowSaveFiles", true);
  this->proxy_enable_login_options = this->config_json->get_bool("ProxyEnableLoginOptions", false);

  try {
    const auto& i = this->config_json->at("CardAuctionSize");
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

  if (!this->is_replay) {
    this->ep3_lobby_banners.clear();
    size_t banner_index = 0;
    for (const auto& it : this->config_json->get("Episode3LobbyBanners", JSON::list()).as_list()) {
      string path = "system/ep3/banners/" + it->at(2).as_string();

      string compressed_gvm_data;
      string decompressed_gvm_data;
      string lower_path = tolower(path);
      if (ends_with(lower_path, ".gvm.prs")) {
        compressed_gvm_data = load_file(path);
      } else if (ends_with(lower_path, ".gvm")) {
        decompressed_gvm_data = load_file(path);
      } else if (ends_with(lower_path, ".bmp")) {
        Image img(path);
        decompressed_gvm_data = encode_gvm(
            img,
            img.get_has_alpha() ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565,
            string_printf("bnr%zu", banner_index),
            0x80 | banner_index);
        banner_index++;
      } else {
        throw runtime_error(string_printf("banner %s is in an unknown format", path.c_str()));
      }

      size_t decompressed_size = decompressed_gvm_data.empty()
          ? prs_decompress_size(compressed_gvm_data)
          : decompressed_gvm_data.size();
      if (decompressed_size > 0x37000) {
        throw runtime_error(string_printf("banner %s is too large (0x%zX bytes; maximum size is 0x37000 bytes)", path.c_str(), decompressed_size));
      }

      if (compressed_gvm_data.empty()) {
        compressed_gvm_data = prs_compress_optimal(decompressed_gvm_data);
      }
      if (compressed_gvm_data.size() > 0x3800) {
        throw runtime_error(string_printf("banner %s cannot be compressed small enough (0x%zX bytes; maximum size is 0x3800 bytes compressed)", it->at(2).as_string().c_str(), compressed_gvm_data.size()));
      }
      config_log.info("Loaded Episode 3 lobby banner %s (0x%zX -> 0x%zX bytes)", path.c_str(), decompressed_size, compressed_gvm_data.size());
      this->ep3_lobby_banners.emplace_back(
          Ep3LobbyBannerEntry{.type = static_cast<uint32_t>(it->at(0).as_int()),
              .which = static_cast<uint32_t>(it->at(1).as_int()),
              .data = std::move(compressed_gvm_data)});
    }
  }

  {
    auto parse_ep3_ex_result_cmd = [&](const JSON& src) -> shared_ptr<G_SetEXResultValues_Ep3_6xB4x4B> {
      auto ret = make_shared<G_SetEXResultValues_Ep3_6xB4x4B>();
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
    const auto& categories_json = this->config_json->at("Episode3EXResultValues");
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
    const auto& stack_limits_tables_json = this->config_json->at("ItemStackLimits");
    for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
      try {
        Version v = static_cast<Version>(v_s);
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, stack_limits_tables_json.at(v_s - NUM_PATCH_VERSIONS));
      } catch (const out_of_range&) {
      }
    }
  } catch (const out_of_range&) {
  }

  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    if (!this->item_stack_limits_tables[v_s]) {
      Version v = static_cast<Version>(v_s);
      if (v == Version::DC_V1_11_2000_PROTOTYPE) {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            Version::DC_V1_11_2000_PROTOTYPE, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_DC_11_2000, 999999);
      } else if (v_s < static_cast<size_t>(Version::GC_NTE)) {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V1_V2, 999999);
      } else {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V3_V4, 999999);
      }
    }
  }

  this->bb_global_exp_multiplier = this->config_json->get_int("BBGlobalEXPMultiplier", 1);
  this->exp_share_multiplier = this->config_json->get_float("BBEXPShareMultiplier", 0.5);
  this->server_global_drop_rate_multiplier = this->config_json->get_float("ServerGlobalDropRateMultiplier", 1);

  set_log_levels_from_json(this->config_json->get("LogLevels", JSON::dict()));

  try {
    this->run_shell_behavior = this->config_json->at("RunInteractiveShell").as_bool()
        ? ServerState::RunShellBehavior::ALWAYS
        : ServerState::RunShellBehavior::NEVER;
  } catch (const out_of_range&) {
  }

  this->allow_dc_pc_games = this->config_json->get_bool("AllowDCPCGames", true);
  this->allow_gc_xb_games = this->config_json->get_bool("AllowGCXBGames", true);
  this->enable_chat_commands = this->config_json->get_bool("EnableChatCommands", true);

  this->version_name_colors.reset();
  this->client_customization_name_color = 0;
  try {
    const auto& colors_json = this->config_json->get_list("VersionNameColors");
    if (colors_json.size() != NUM_NON_PATCH_VERSIONS) {
      throw runtime_error("VersionNameColors list length is incorrect");
    }
    auto new_colors = make_unique<std::array<uint32_t, NUM_NON_PATCH_VERSIONS>>();
    for (size_t z = 0; z < NUM_NON_PATCH_VERSIONS; z++) {
      new_colors->at(z) = colors_json.at(z)->as_int();
    }
    this->version_name_colors = std::move(new_colors);
  } catch (const out_of_range&) {
  }
  try {
    this->client_customization_name_color = this->config_json->get_int("ClientCustomizationNameColor");
  } catch (const out_of_range&) {
  }

  for (auto& order : this->public_lobby_search_orders) {
    order.clear();
  }
  this->client_customization_public_lobby_search_order.clear();
  try {
    const auto& orders_json = this->config_json->get_list("LobbySearchOrders");
    for (size_t v_s = 0; v_s < orders_json.size(); v_s++) {
      auto& order = this->public_lobby_search_orders.at(v_s);
      const auto& order_json = orders_json.at(v_s);
      for (const auto& it : order_json->as_list()) {
        order.emplace_back(it->as_int());
      }
    }
  } catch (const out_of_range&) {
  }
  try {
    const auto& order_json = this->config_json->get_list("ClientCustomizationLobbySearchOrder");
    auto& order = this->client_customization_public_lobby_search_order;
    for (const auto& it : order_json) {
      order.emplace_back(it->as_int());
    }
  } catch (const out_of_range&) {
  }

  this->pre_lobby_event = 0;
  try {
    auto v = this->config_json->at("MenuEvent");
    this->pre_lobby_event = v.is_int() ? v.as_int() : event_for_name(v.as_string());
  } catch (const out_of_range&) {
  }

  this->ep3_menu_song = this->config_json->get_int("Episode3MenuSong", -1);

  try {
    this->quest_category_index = make_shared<QuestCategoryIndex>(this->config_json->at("QuestCategories"));
  } catch (const exception& e) {
    throw runtime_error(string_printf(
        "QuestCategories is missing or invalid in config.json (%s) - see config.example.json for an example", e.what()));
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
    const JSON& default_json = this->config_json->get("InformationMenuContents", blank_json);
    const JSON& v2_json = this->config_json->get("InformationMenuContentsV1V2", default_json);
    const JSON& v3_json = this->config_json->get("InformationMenuContentsV3", default_json);

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
      for (const auto& it : this->config_json->at(key).as_dict()) {
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
    const string& netloc_str = this->config_json->get_string("ProxyDestination-Patch");
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
    const string& netloc_str = this->config_json->get_string("ProxyDestination-BB");
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

  this->welcome_message = this->config_json->get_string("WelcomeMessage", "");
  this->pc_patch_server_message = this->config_json->get_string("PCPatchServerMessage", "");
  this->bb_patch_server_message = this->config_json->get_string("BBPatchServerMessage", "");

  this->team_reward_defs_json = nullptr;
  try {
    this->team_reward_defs_json = std::move(this->config_json->at("TeamRewards"));
  } catch (const out_of_range&) {
  }

  for (size_t z = 0; z < 4; z++) {
    shared_ptr<const Map::RareEnemyRates> prev = Map::DEFAULT_RARE_ENEMIES;
    try {
      string key = "RareEnemyRates-";
      key += token_name_for_difficulty(z);
      this->rare_enemy_rates_by_difficulty[z] = make_shared<Map::RareEnemyRates>(this->config_json->at(key));
      prev = this->rare_enemy_rates_by_difficulty[z];
    } catch (const out_of_range&) {
      this->rare_enemy_rates_by_difficulty[z] = prev;
    }
  }
  try {
    this->rare_enemy_rates_challenge = make_shared<Map::RareEnemyRates>(this->config_json->at("RareEnemyRates-Challenge"));
  } catch (const out_of_range&) {
    this->rare_enemy_rates_challenge = Map::DEFAULT_RARE_ENEMIES;
  }

  this->min_levels_v4[0] = DEFAULT_MIN_LEVELS_V4_EP1;
  this->min_levels_v4[1] = DEFAULT_MIN_LEVELS_V4_EP2;
  this->min_levels_v4[2] = DEFAULT_MIN_LEVELS_V4_EP4;
  try {
    for (const auto& ep_it : this->config_json->get_dict("BBMinimumLevels")) {
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

  this->update_dependent_server_configs();
}

void ServerState::load_config_late() {
  for (size_t z = 1; z <= 20; z++) {
    auto l = this->find_lobby(z);
    if (l) {
      l->event = 0;
    }
  }
  try {
    const auto& events_json = this->config_json->get_list("LobbyEvents");
    for (size_t z = 0; z < events_json.size(); z++) {
      const auto& v = events_json.at(z);
      uint8_t event = v->is_int() ? v->as_int() : event_for_name(v->as_string());
      const auto& l = this->find_lobby(z + 1);
      if (l && l->check_flag(Lobby::Flag::DEFAULT)) {
        l->event = event;
        send_change_event(l, l->event);
      }
    }
  } catch (const out_of_range&) {
  }

  this->ep3_card_auction_pool.clear();
  try {
    for (const auto& it : this->config_json->get_dict("CardAuctionPool")) {
      uint16_t card_id;
      try {
        card_id = this->ep3_card_index->definition_for_name_normalized(it.first)->def.card_id;
      } catch (const out_of_range&) {
        throw runtime_error(string_printf("Ep3 card \"%s\" in auction pool does not exist", it.first.c_str()));
      }
      this->ep3_card_auction_pool.emplace_back(
          CardAuctionPoolEntry{
              .probability = static_cast<uint64_t>(it.second->at(0).as_int()),
              .card_id = card_id,
              .min_price = static_cast<uint16_t>(it.second->at(1).as_int())});
    }
  } catch (const out_of_range&) {
  }

  for (auto& trap_card_ids : this->ep3_trap_card_ids) {
    trap_card_ids.clear();
  }
  if (this->ep3_card_index) {
    try {
      const auto& ep3_trap_cards_json = this->config_json->get_list("Episode3TrapCards");
      if (!ep3_trap_cards_json.empty()) {
        if (ep3_trap_cards_json.size() != 5) {
          throw runtime_error("Episode3TrapCards must be a list of 5 lists");
        }
        for (size_t trap_type = 0; trap_type < 5; trap_type++) {
          auto& trap_card_ids = this->ep3_trap_card_ids[trap_type];
          for (const auto& card_it : ep3_trap_cards_json.at(trap_type)->as_list()) {
            const string& card_name = card_it->as_string();
            try {
              const auto& card = this->ep3_card_index->definition_for_name_normalized(card_name);
              if (card->def.type != Episode3::CardType::ASSIST) {
                throw runtime_error(string_printf("Ep3 card \"%s\" in trap card list is not an assist card", card_name.c_str()));
              }
              trap_card_ids.emplace_back(card->def.card_id);
            } catch (const out_of_range&) {
              throw runtime_error(string_printf("Ep3 card \"%s\" in trap card list does not exist", card_name.c_str()));
            }
          }
        }
      }
    } catch (const out_of_range&) {
    }
  } else {
    config_log.warning("Episode 3 card definitions missing; cannot set trap card IDs from config");
  }

  this->quest_F95E_results.clear();
  this->quest_F95F_results.clear();
  this->quest_F960_success_results.clear();
  this->quest_F960_failure_results = QuestF960Result();
  this->secret_lottery_results.clear();
  if (this->item_name_index(Version::BB_V4)) {
    try {
      for (const auto& type_it : this->config_json->get_list("QuestF95EResultItems")) {
        auto& type_res = this->quest_F95E_results.emplace_back();
        for (const auto& difficulty_it : type_it->as_list()) {
          auto& difficulty_res = type_res.emplace_back();
          for (const auto& item_it : difficulty_it->as_list()) {
            try {
              difficulty_res.emplace_back(this->parse_item_description(Version::BB_V4, item_it->as_string()));
            } catch (const exception& e) {
              config_log.warning("Cannot parse item description \"%s\": %s (skipping entry)", item_it->as_string().c_str(), e.what());
            }
          }
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& it : this->config_json->get_list("QuestF95FResultItems")) {
        auto& list = it->as_list();
        size_t price = list.at(0)->as_int();
        try {
          this->quest_F95F_results.emplace_back(make_pair(price, this->parse_item_description(Version::BB_V4, list.at(1)->as_string())));
        } catch (const exception& e) {
          config_log.warning("Cannot parse item description \"%s\": %s (skipping entry)", list.at(1)->as_string().c_str(), e.what());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      this->quest_F960_failure_results = QuestF960Result(this->config_json->at("QuestF960FailureResultItems"), this->item_name_index(Version::BB_V4));
      for (const auto& it : this->config_json->get_list("QuestF960SuccessResultItems")) {
        this->quest_F960_success_results.emplace_back(*it, this->item_name_index(Version::BB_V4));
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& it : this->config_json->get_list("SecretLotteryResultItems")) {
        try {
          this->secret_lottery_results.emplace_back(this->parse_item_description(Version::BB_V4, it->as_string()));
        } catch (const exception& e) {
          config_log.warning("Cannot parse item description \"%s\": %s (skipping entry)", it->as_string().c_str(), e.what());
        }
      }
    } catch (const out_of_range&) {
    }

    auto parse_primary_identifier_list = [&](const char* key, Version base_version) -> std::unordered_set<uint32_t> {
      std::unordered_set<uint32_t> ret;
      try {
        for (const auto& pi_json : this->config_json->get_list(key)) {
          if (pi_json->is_int()) {
            ret.emplace(pi_json->as_int());
          } else {
            try {
              auto item = this->parse_item_description(base_version, pi_json->as_string());
              ret.emplace(item.primary_identifier());
            } catch (const exception& e) {
              config_log.warning("Cannot parse item description \"%s\": %s (skipping entry)", pi_json->as_string().c_str(), e.what());
            }
          }
        }
      } catch (const out_of_range&) {
      }
      return ret;
    };
    this->notify_game_for_item_primary_identifiers_v1_v2 = parse_primary_identifier_list(
        "NotifyGameForItemPrimaryIdentifiersV1V2", Version::PC_V2);
    this->notify_game_for_item_primary_identifiers_v3 = parse_primary_identifier_list(
        "NotifyGameForItemPrimaryIdentifiersV3", Version::GC_V3);
    this->notify_game_for_item_primary_identifiers_v4 = parse_primary_identifier_list(
        "NotifyGameForItemPrimaryIdentifiersV4", Version::BB_V4);
    this->notify_server_for_item_primary_identifiers_v1_v2 = parse_primary_identifier_list(
        "NotifyServerForItemPrimaryIdentifiersV1V2", Version::PC_V2);
    this->notify_server_for_item_primary_identifiers_v3 = parse_primary_identifier_list(
        "NotifyServerForItemPrimaryIdentifiersV3", Version::GC_V3);
    this->notify_server_for_item_primary_identifiers_v4 = parse_primary_identifier_list(
        "NotifyServerForItemPrimaryIdentifiersV4", Version::BB_V4);

  } else {
    config_log.warning("BB item name index is missing; cannot load quest reward lists from config");
  }
}

void ServerState::load_bb_private_keys(bool from_non_event_thread) {
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> new_keys;
  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    new_keys.emplace_back(make_shared<PSOBBEncryption::KeyFile>(
        load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.info("Loaded Blue Burst key file: %s", filename.c_str());
  }
  config_log.info("%zu Blue Burst key file(s) loaded", this->bb_private_keys.size());

  auto set = [s = this->shared_from_this(), new_keys = std::move(new_keys)]() {
    s->bb_private_keys = std::move(new_keys);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_accounts(bool from_non_event_thread) {
  config_log.info("Indexing accounts");
  shared_ptr<AccountIndex> new_index = make_shared<AccountIndex>(this->is_replay);

  auto set = [s = this->shared_from_this(), new_index = std::move(new_index)]() {
    s->account_index = std::move(new_index);
    s->update_dependent_server_configs();
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_teams(bool from_non_event_thread) {
  config_log.info("Indexing teams");
  shared_ptr<TeamIndex> new_index = make_shared<TeamIndex>("system/teams", this->team_reward_defs_json);

  auto set = [s = this->shared_from_this(), new_index = std::move(new_index)]() {
    s->team_index = std::move(new_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_patch_indexes(bool from_non_event_thread) {
  shared_ptr<const GSLArchive> bb_data_gsl;
  shared_ptr<PatchFileIndex> pc_patch_file_index;
  shared_ptr<PatchFileIndex> bb_patch_file_index;

  if (isdir("system/patch-pc")) {
    config_log.info("Indexing PSO PC patch files");
    pc_patch_file_index = make_shared<PatchFileIndex>("system/patch-pc");
  } else {
    config_log.info("PSO PC patch files not present");
  }
  if (isdir("system/patch-bb")) {
    config_log.info("Indexing PSO BB patch files");
    bb_patch_file_index = make_shared<PatchFileIndex>("system/patch-bb");
    try {
      auto gsl_file = bb_patch_file_index->get("./data/data.gsl");
      bb_data_gsl = make_shared<GSLArchive>(gsl_file->load_data(), false);
      config_log.info("data.gsl found in BB patch files");
    } catch (const out_of_range&) {
      config_log.info("data.gsl is not present in BB patch files");
    }
  } else {
    config_log.info("PSO BB patch files not present");
  }

  auto set = [s = this->shared_from_this(),
                 bb_data_gsl = std::move(bb_data_gsl),
                 pc_patch_file_index = std::move(pc_patch_file_index),
                 bb_patch_file_index = std::move(bb_patch_file_index)]() {
    s->bb_data_gsl = std::move(bb_data_gsl);
    s->pc_patch_file_index = std::move(pc_patch_file_index);
    s->bb_patch_file_index = std::move(bb_patch_file_index);
    s->update_dependent_server_configs();
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::clear_map_file_caches() {
  config_log.info("Clearing map file caches");
  for (auto& cache : this->map_file_caches) {
    cache = make_shared<ThreadSafeFileCache>();
  }
}

void ServerState::load_set_data_tables(bool from_non_event_thread) {
  config_log.info("Loading set data tables");

  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables_ep1_ult;
  std::shared_ptr<const SetDataTableBase> new_table_bb_solo;
  std::shared_ptr<const SetDataTableBase> new_table_bb_solo_ep1_ult;

  auto load_table = [&](Version version) -> void {
    auto data = this->load_map_file(version, "SetDataTableOn.rel");
    new_tables[static_cast<size_t>(version)] = make_shared<SetDataTable>(version, *data);
    if (!is_v1(version) && (version != Version::PC_NTE)) {
      auto data_ep1_ult = this->load_map_file(version, "SetDataTableOnUlti.rel");
      new_tables_ep1_ult[static_cast<size_t>(version)] = make_shared<SetDataTable>(version, *data_ep1_ult);
    }
  };

  new_tables[static_cast<size_t>(Version::DC_NTE)] = make_shared<SetDataTableDCNTE>();
  new_tables[static_cast<size_t>(Version::DC_V1_11_2000_PROTOTYPE)] = make_shared<SetDataTableDC112000>();
  load_table(Version::DC_V1);
  load_table(Version::DC_V2);
  load_table(Version::PC_NTE);
  load_table(Version::PC_V2);
  load_table(Version::GC_NTE);
  load_table(Version::GC_V3);
  load_table(Version::XB_V3);
  load_table(Version::BB_V4);

  auto bb_solo_data = this->load_map_file(Version::BB_V4, "SetDataTableOff.rel");
  new_table_bb_solo = make_shared<SetDataTable>(Version::BB_V4, *bb_solo_data);
  auto bb_solo_data_ep1_ult = this->load_map_file(Version::BB_V4, "SetDataTableOffUlti.rel");
  new_table_bb_solo_ep1_ult = make_shared<SetDataTable>(Version::BB_V4, *bb_solo_data_ep1_ult);

  auto set = [s = this->shared_from_this(),
                 new_tables = std::move(new_tables),
                 new_tables_ep1_ult = std::move(new_tables_ep1_ult),
                 new_table_bb_solo = std::move(new_table_bb_solo),
                 new_table_bb_solo_ep1_ult = std::move(new_table_bb_solo_ep1_ult)]() {
    s->set_data_tables = std::move(new_tables);
    s->set_data_tables_ep1_ult = std::move(new_tables_ep1_ult);
    s->bb_solo_set_data_table = std::move(new_table_bb_solo);
    s->bb_solo_set_data_table_ep1_ult = std::move(new_table_bb_solo_ep1_ult);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_battle_params(bool from_non_event_thread) {
  config_log.info("Loading battle parameters");
  auto new_battle_params = make_shared<BattleParamsIndex>(
      this->load_bb_file("BattleParamEntry_on.dat"),
      this->load_bb_file("BattleParamEntry_lab_on.dat"),
      this->load_bb_file("BattleParamEntry_ep4_on.dat"),
      this->load_bb_file("BattleParamEntry.dat"),
      this->load_bb_file("BattleParamEntry_lab.dat"),
      this->load_bb_file("BattleParamEntry_ep4.dat"));

  auto set = [s = this->shared_from_this(), new_battle_params = std::move(new_battle_params)]() {
    s->battle_params = std::move(new_battle_params);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_level_tables(bool from_non_event_thread) {
  config_log.info("Loading level tables");
  auto new_table_v1_v2 = make_shared<LevelTableV2>(load_file("system/level-tables/PlayerTable-pc-v2.prs"), true);
  auto new_table_v3 = make_shared<LevelTableV3BE>(load_file("system/level-tables/PlyLevelTbl-gc-v3.cpt"), true);
  auto new_table_v4 = make_shared<LevelTableV4>(*this->load_bb_file("PlyLevelTbl.prs"), true);

  auto set = [s = this->shared_from_this(), new_table_v1_v2 = std::move(new_table_v1_v2), new_table_v3 = std::move(new_table_v3), new_table_v4 = std::move(new_table_v4)]() {
    s->level_table_v1_v2 = std::move(new_table_v1_v2);
    s->level_table_v3 = std::move(new_table_v3);
    s->level_table_v4 = std::move(new_table_v4);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_text_index(bool from_non_event_thread) {
  auto new_index = make_shared<TextIndex>("system/text-sets", [&](Version version, const string& filename) -> shared_ptr<const string> {
    try {
      if (version == Version::BB_V4) {
        return this->load_bb_file(filename);
      } else {
        return this->pc_patch_file_index->get("Media/PSO/" + filename)->load_data();
      }
    } catch (const out_of_range&) {
      return nullptr;
    } catch (const cannot_open_file&) {
      return nullptr;
    }
  });

  auto set = [s = this->shared_from_this(), new_index = std::move(new_index)]() {
    s->text_index = std::move(new_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_word_select_table(bool from_non_event_thread) {
  config_log.info("Loading Word Select table");

  vector<vector<string>> name_alias_lists;
  auto json = JSON::parse(load_file("system/text-sets/ws-name-alias-lists.json"));
  for (const auto& coll_it : json.as_list()) {
    auto& coll = name_alias_lists.emplace_back();
    for (const auto& str_it : coll_it->as_list()) {
      coll.emplace_back(str_it->as_string());
    }
  }

  const vector<string>* pc_unitxt_collection = nullptr;
  const vector<string>* bb_unitxt_collection = nullptr;
  unique_ptr<UnicodeTextSet> pc_unitxt_data;
  if (this->text_index) {
    config_log.info("(Word select) Using PC_V2 unitxt_e.prs from text index");
    pc_unitxt_collection = &this->text_index->get(Version::PC_V2, 1, 35);
  } else {
    config_log.info("(Word select) Loading PC_V2 unitxt_e.prs");
    pc_unitxt_data = make_unique<UnicodeTextSet>(load_file("system/text-sets/pc-v2/unitxt_e.prs"));
    pc_unitxt_collection = &pc_unitxt_data->get(35);
  }
  config_log.info("(Word select) Loading BB_V4 unitxt_ws_e.prs");
  auto bb_unitxt_data = make_unique<UnicodeTextSet>(load_file("system/text-sets/bb-v4/unitxt_ws_e.prs"));
  bb_unitxt_collection = &bb_unitxt_data->get(0);

  config_log.info("(Word select) Loading DC_NTE data");
  WordSelectSet dc_nte_ws(load_file("system/text-sets/dc-nte/ws_data.bin"), Version::DC_NTE, nullptr, true);
  config_log.info("(Word select) Loading DC_V1_11_2000_PROTOTYPE data");
  WordSelectSet dc_112000_ws(load_file("system/text-sets/dc-11-2000/ws_data.bin"), Version::DC_V1_11_2000_PROTOTYPE, nullptr, false);
  config_log.info("(Word select) Loading DC_V1 data");
  WordSelectSet dc_v1_ws(load_file("system/text-sets/dc-v1/ws_data.bin"), Version::DC_V1, nullptr, false);
  config_log.info("(Word select) Loading DC_V2 data");
  WordSelectSet dc_v2_ws(load_file("system/text-sets/dc-v2/ws_data.bin"), Version::DC_V2, nullptr, false);
  config_log.info("(Word select) Loading PC_NTE data");
  WordSelectSet pc_nte_ws(load_file("system/text-sets/pc-nte/ws_data.bin"), Version::PC_NTE, pc_unitxt_collection, false);
  config_log.info("(Word select) Loading PC_V2 data");
  WordSelectSet pc_v2_ws(load_file("system/text-sets/pc-v2/ws_data.bin"), Version::PC_V2, pc_unitxt_collection, false);
  config_log.info("(Word select) Loading GC_NTE data");
  WordSelectSet gc_nte_ws(load_file("system/text-sets/gc-nte/ws_data.bin"), Version::GC_NTE, nullptr, false);
  config_log.info("(Word select) Loading GC_V3 data");
  WordSelectSet gc_v3_ws(load_file("system/text-sets/gc-v3/ws_data.bin"), Version::GC_V3, nullptr, false);
  config_log.info("(Word select) Loading GC_EP3_NTE data");
  WordSelectSet gc_ep3_nte_ws(load_file("system/text-sets/gc-ep3-nte/ws_data.bin"), Version::GC_EP3_NTE, nullptr, false);
  config_log.info("(Word select) Loading GC_EP3 data");
  WordSelectSet gc_ep3_ws(load_file("system/text-sets/gc-ep3/ws_data.bin"), Version::GC_EP3, nullptr, false);
  config_log.info("(Word select) Loading XB_V3 data");
  WordSelectSet xb_v3_ws(load_file("system/text-sets/xb-v3/ws_data.bin"), Version::XB_V3, nullptr, false);
  config_log.info("(Word select) Loading BB_V4 data");
  WordSelectSet bb_v4_ws(load_file("system/text-sets/bb-v4/ws_data.bin"), Version::BB_V4, bb_unitxt_collection, false);

  config_log.info("(Word select) Generating table");
  auto new_table = make_shared<WordSelectTable>(
      dc_nte_ws, dc_112000_ws, dc_v1_ws, dc_v2_ws,
      pc_nte_ws, pc_v2_ws, gc_nte_ws, gc_v3_ws,
      gc_ep3_nte_ws, gc_ep3_ws, xb_v3_ws, bb_v4_ws,
      name_alias_lists);

  auto set = [s = this->shared_from_this(), new_table = std::move(new_table)]() {
    s->word_select_table = std::move(new_table);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

shared_ptr<ItemNameIndex> ServerState::create_item_name_index_for_version(
    shared_ptr<const ItemParameterTable> pmt,
    shared_ptr<const ItemData::StackLimits> limits,
    shared_ptr<const TextIndex> text_index) const {
  switch (limits->version) {
    case Version::DC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_NTE, 0, 2));
    case Version::DC_V1_11_2000_PROTOTYPE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V1_11_2000_PROTOTYPE, 1, 2));
    case Version::DC_V1:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V1, 1, 2));
    case Version::DC_V2:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V2, 1, 3));
    case Version::PC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_NTE, 1, 3));
    case Version::PC_V2:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_V2, 1, 3));
    case Version::GC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_NTE, 1, 0));
    case Version::GC_V3:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_V3, 1, 0));
    case Version::XB_V3:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::XB_V3, 1, 0));
    case Version::BB_V4:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::BB_V4, 1, 1));
    default:
      return nullptr;
  }
}

void ServerState::load_item_name_indexes(bool from_non_event_thread) {
  std::array<std::shared_ptr<const ItemNameIndex>, NUM_VERSIONS> new_indexes;

  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s);
    config_log.info("Generating item name index for %s", name_for_enum(v));
    new_indexes[v_s] = this->create_item_name_index_for_version(
        this->item_parameter_table(v), this->item_stack_limits(v), this->text_index);
  }
  new_indexes[static_cast<size_t>(Version::GC_EP3)] = new_indexes[static_cast<size_t>(Version::GC_V3)];
  new_indexes[static_cast<size_t>(Version::GC_EP3_NTE)] = new_indexes[static_cast<size_t>(Version::GC_V3)];

  auto set = [s = this->shared_from_this(), new_indexes = std::move(new_indexes)]() {
    s->item_name_indexes = std::move(new_indexes);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_drop_tables(bool from_non_event_thread) {
  config_log.info("Loading rare item sets");

  unordered_map<string, shared_ptr<RareItemSet>> new_rare_item_sets;
  for (const auto& filename : list_directory_sorted("system/item-tables")) {
    if (!starts_with(filename, "rare-table-")) {
      continue;
    }

    string path = "system/item-tables/" + filename;
    size_t ext_offset = filename.rfind('.');
    string basename = (ext_offset == string::npos) ? filename : filename.substr(0, ext_offset);

    if (ends_with(filename, "-v1.json")) {
      config_log.info("Loading v1 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), this->item_name_index(Version::DC_V1)));
    } else if (ends_with(filename, "-v2.json")) {
      config_log.info("Loading v2 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), this->item_name_index(Version::PC_V2)));
    } else if (ends_with(filename, "-v3.json")) {
      config_log.info("Loading v3 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), this->item_name_index(Version::GC_V3)));
    } else if (ends_with(filename, "-v4.json")) {
      config_log.info("Loading v4 JSON rare item table %s", filename.c_str());
      new_rare_item_sets.emplace(basename, make_shared<RareItemSet>(JSON::parse(load_file(path)), this->item_name_index(Version::BB_V4)));

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

  config_log.info("Loading v2 common item table");
  auto ct_data_v2 = make_shared<string>(load_file("system/item-tables/ItemCT-pc-v2.afs"));
  auto pt_data_v2 = make_shared<string>(load_file("system/item-tables/ItemPT-pc-v2.afs"));
  auto new_common_item_set_v2 = make_shared<AFSV2CommonItemSet>(pt_data_v2, ct_data_v2);
  config_log.info("Loading v3+v4 common item table");
  auto pt_data_v3_v4 = make_shared<string>(load_file("system/item-tables/ItemPT-gc-v3.gsl"));
  auto new_common_item_set_v3_v4 = make_shared<GSLV3V4CommonItemSet>(pt_data_v3_v4, true);

  config_log.info("Loading armor table");
  auto armor_data = make_shared<string>(load_file("system/item-tables/ArmorRandom-gc-v3.rel"));
  auto new_armor_random_set = make_shared<ArmorRandomSet>(armor_data);

  config_log.info("Loading tool table");
  auto tool_data = make_shared<string>(load_file("system/item-tables/ToolRandom-gc-v3.rel"));
  auto new_tool_random_set = make_shared<ToolRandomSet>(tool_data);

  config_log.info("Loading weapon tables");
  std::array<std::shared_ptr<const WeaponRandomSet>, 4> new_weapon_random_sets;
  const char* filenames[4] = {
      "system/item-tables/WeaponRandomNormal-gc-v3.rel",
      "system/item-tables/WeaponRandomHard-gc-v3.rel",
      "system/item-tables/WeaponRandomVeryHard-gc-v3.rel",
      "system/item-tables/WeaponRandomUltimate-gc-v3.rel",
  };
  for (size_t z = 0; z < 4; z++) {
    auto weapon_data = make_shared<string>(load_file(filenames[z]));
    new_weapon_random_sets[z] = make_shared<WeaponRandomSet>(weapon_data);
  }

  config_log.info("Loading tekker adjustment table");
  auto tekker_data = make_shared<string>(load_file("system/item-tables/JudgeItem-gc-v3.rel"));
  auto new_tekker_adjustment_set = make_shared<TekkerAdjustmentSet>(tekker_data);

  auto set = [s = this->shared_from_this(),
                 new_rare_item_sets = std::move(new_rare_item_sets),
                 new_common_item_set_v2 = std::move(new_common_item_set_v2),
                 new_common_item_set_v3_v4 = std::move(new_common_item_set_v3_v4),
                 new_armor_random_set = std::move(new_armor_random_set),
                 new_tool_random_set = std::move(new_tool_random_set),
                 new_weapon_random_sets = std::move(new_weapon_random_sets),
                 new_tekker_adjustment_set = std::move(new_tekker_adjustment_set)]() {
    if (s->server_global_drop_rate_multiplier != 1.0) {
      for (auto& it : new_rare_item_sets) {
        it.second->multiply_all_rates(s->server_global_drop_rate_multiplier);
      }
    }
    // We can't just move() new_rare_item_sets into place because its values are
    // not const :(
    s->rare_item_sets.clear();
    for (auto& it : new_rare_item_sets) {
      s->rare_item_sets.emplace(it.first, std::move(it.second));
    }
    s->common_item_set_v2 = std::move(new_common_item_set_v2);
    s->common_item_set_v3_v4 = std::move(new_common_item_set_v3_v4);
    s->armor_random_set = std::move(new_armor_random_set);
    s->tool_random_set = std::move(new_tool_random_set);
    s->weapon_random_sets = std::move(new_weapon_random_sets);
    s->tekker_adjustment_set = std::move(new_tekker_adjustment_set);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_item_definitions(bool from_non_event_thread) {
  std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS> new_item_parameter_tables;
  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s);
    string path = string_printf("system/item-tables/ItemPMT-%s.prs", file_path_token_for_version(v));
    config_log.info("Loading item definition table %s", path.c_str());
    auto data = make_shared<string>(prs_decompress(load_file(path)));
    new_item_parameter_tables[v_s] = make_shared<ItemParameterTable>(data, v);
  }

  // TODO: We should probably load the tables for other versions too.
  config_log.info("Loading mag evolution table");
  auto mag_data = make_shared<string>(prs_decompress(load_file("system/item-tables/ItemMagEdit-bb-v4.prs")));
  auto new_mag_evolution_table = make_shared<MagEvolutionTable>(mag_data);

  auto set = [s = this->shared_from_this(),
                 new_item_parameter_tables = std::move(new_item_parameter_tables),
                 new_mag_evolution_table = std::move(new_mag_evolution_table)]() {
    s->item_parameter_tables = std::move(new_item_parameter_tables);
    s->mag_evolution_table = std::move(new_mag_evolution_table);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_ep3_cards(bool from_non_event_thread) {
  config_log.info("Loading Episode 3 card definitions");
  auto new_ep3_card_index = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions.mnr",
      "system/ep3/card-definitions.mnrd",
      "system/ep3/card-text.mnr",
      "system/ep3/card-text.mnrd",
      "system/ep3/card-dice-text.mnr",
      "system/ep3/card-dice-text.mnrd");
  config_log.info("Loading Episode 3 trial card definitions");
  auto new_ep3_card_index_trial = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions-trial.mnr",
      "system/ep3/card-definitions-trial.mnrd",
      "system/ep3/card-text-trial.mnr",
      "system/ep3/card-text-trial.mnrd",
      "system/ep3/card-dice-text-trial.mnr",
      "system/ep3/card-dice-text-trial.mnrd");
  config_log.info("Loading Episode 3 COM decks");
  auto new_ep3_com_deck_index = make_shared<Episode3::COMDeckIndex>("system/ep3/com-decks.json");

  auto set = [s = this->shared_from_this(),
                 new_ep3_card_index = std::move(new_ep3_card_index),
                 new_ep3_card_index_trial = std::move(new_ep3_card_index_trial),
                 new_ep3_com_deck_index = std::move(new_ep3_com_deck_index)]() {
    s->ep3_card_index = std::move(new_ep3_card_index);
    s->ep3_card_index_trial = std::move(new_ep3_card_index_trial);
    s->ep3_com_deck_index = std::move(new_ep3_com_deck_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_ep3_maps(bool from_non_event_thread) {
  config_log.info("Collecting Episode 3 maps");
  auto new_ep3_map_index = make_shared<Episode3::MapIndex>("system/ep3/maps");

  auto set = [s = this->shared_from_this(), new_ep3_map_index = std::move(new_ep3_map_index)]() {
    s->ep3_map_index = std::move(new_ep3_map_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_ep3_tournament_state(bool from_non_event_thread) {
  config_log.info("Loading Episode 3 tournament state");
  const string& tournament_state_filename = "system/ep3/tournament-state.json";
  auto new_ep3_tournament_index = make_shared<Episode3::TournamentIndex>(
      this->ep3_map_index, this->ep3_com_deck_index, tournament_state_filename);

  auto set = [s = this->shared_from_this(),
                 new_ep3_tournament_index = std::move(new_ep3_tournament_index)]() {
    s->ep3_tournament_index = std::move(new_ep3_tournament_index);
    s->ep3_tournament_index->link_all_clients(s);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_quest_index(bool from_non_event_thread) {
  config_log.info("Collecting quests");
  auto new_default_quest_index = make_shared<QuestIndex>("system/quests", this->quest_category_index, false);
  config_log.info("Collecting Episode 3 download quests");
  auto new_ep3_download_quest_index = make_shared<QuestIndex>("system/ep3/maps-download", this->quest_category_index, true);

  auto set = [s = this->shared_from_this(),
                 new_default_quest_index = std::move(new_default_quest_index),
                 new_ep3_download_quest_index = std::move(new_ep3_download_quest_index)]() {
    s->default_quest_index = std::move(new_default_quest_index);
    s->ep3_download_quest_index = std::move(new_ep3_download_quest_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::compile_functions(bool from_non_event_thread) {
  config_log.info("Compiling client functions");
  auto new_function_code_index = make_shared<FunctionCodeIndex>("system/client-functions");

  auto set = [s = this->shared_from_this(), new_function_code_index = std::move(new_function_code_index)]() {
    s->function_code_index = std::move(new_function_code_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::load_dol_files(bool from_non_event_thread) {
  config_log.info("Loading DOL files");
  auto new_dol_file_index = make_shared<DOLFileIndex>("system/dol");

  auto set = [s = this->shared_from_this(), new_dol_file_index = std::move(new_dol_file_index)]() {
    s->dol_file_index = std::move(new_dol_file_index);
  };
  this->forward_or_call(from_non_event_thread, std::move(set));
}

void ServerState::create_default_lobbies() {
  if (this->default_lobbies_created) {
    return;
  }
  this->default_lobbies_created = true;

  vector<shared_ptr<Lobby>> non_v1_only_lobbies;
  vector<shared_ptr<Lobby>> ep3_only_lobbies;

  for (size_t x = 0; x < 20; x++) {
    auto lobby_name = string_printf("LOBBY%zu", x + 1);
    bool allow_v1 = (x <= 9);
    bool allow_non_ep3 = (x <= 14);

    shared_ptr<Lobby> l = this->create_lobby(false);
    l->event = this->pre_lobby_event;
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
  }
}

void ServerState::load_all() {
  this->collect_network_addresses();
  this->load_config_early();
  this->load_bb_private_keys(false);
  this->load_accounts(false);
  this->clear_map_file_caches();
  this->load_patch_indexes(false);
  this->load_ep3_cards(false);
  this->load_ep3_maps(false);
  this->load_ep3_tournament_state(false);
  this->compile_functions(false);
  this->load_dol_files(false);
  this->create_default_lobbies();
  this->load_set_data_tables(false);
  this->load_battle_params(false);
  this->load_level_tables(false);
  this->load_text_index(false);
  this->load_word_select_table(false);
  this->load_item_definitions(false);
  this->load_item_name_indexes(false);
  this->load_drop_tables(false);
  this->load_config_late();
  this->load_teams(false);
  this->load_quest_index(false);
}

shared_ptr<PatchServer::Config> ServerState::generate_patch_server_config(bool is_bb) const {
  auto ret = make_shared<PatchServer::Config>();
#ifdef PHOSG_WINDOWS
  // libevent doesn't play nice with Cygwin, so we run the patch server on the
  // main thread there. The problem seems to be that the locking structures are
  // never set up, presumably since we call event_use_pthreads() since
  // event_use_windows_threads() doesn't exist. (Does literally no one else use
  // libevent with Cygwin??)
  ret->shared_base = this->base;
#endif
  ret->allow_unregistered_users = this->allow_unregistered_users;
  ret->hide_data_from_logs = this->hide_download_commands;
  ret->idle_timeout_usecs = this->patch_client_idle_timeout_usecs;
  ret->message = is_bb ? this->bb_patch_server_message : this->pc_patch_server_message;
  ret->account_index = this->account_index;
  ret->banned_ipv4_ranges = this->banned_ipv4_ranges;
  ret->patch_file_index = is_bb ? this->bb_patch_file_index : this->pc_patch_file_index;
  return ret;
}

void ServerState::update_dependent_server_configs() const {
  if (this->pc_patch_server) {
    this->pc_patch_server->set_config(this->generate_patch_server_config(false));
  }
  if (this->bb_patch_server) {
    this->bb_patch_server->set_config(this->generate_patch_server_config(true));
  }
  if (this->dns_server) {
    this->dns_server->set_banned_ipv4_ranges(this->banned_ipv4_ranges);
  }
}

void ServerState::disconnect_all_banned_clients() {
  uint64_t now_usecs = now();

  if (this->game_server) {
    for (const auto& c : this->game_server->all_clients()) {
      if ((c->login && (c->login->account->ban_end_time > now_usecs)) ||
          this->banned_ipv4_ranges->check(c->channel.remote_addr)) {
        this->game_server->disconnect_client(c);
      }
    }
  }

  // Proxy server
  if (this->proxy_server) {
    vector<uint32_t> sessions_to_close;
    for (const auto& it : this->proxy_server->all_sessions()) {
      auto ses = it.second;
      if ((ses->login && (ses->login->account->ban_end_time > now_usecs)) ||
          this->banned_ipv4_ranges->check(ses->client_channel.remote_addr)) {
        sessions_to_close.emplace_back(it.first);
      }
    }
    for (uint32_t ses_id : sessions_to_close) {
      this->proxy_server->delete_session(ses_id);
    }
  }

  // IP stack simulator (IP bans only; account bans will presumably be handled
  // by one of the above cases)
  if (this->ip_stack_simulator) {
    vector<uint64_t> ids_to_disconnect;
    for (const auto& it : this->ip_stack_simulator->all_networks()) {
      int fd = bufferevent_getfd(it.second->bev.get());
      if (fd < 0) {
        continue;
      }
      struct sockaddr_storage remote_ss;
      get_socket_addresses(fd, nullptr, &remote_ss);
      if (this->banned_ipv4_ranges->check(remote_ss)) {
        ids_to_disconnect.emplace_back(it.second->network_id);
      }
    }
    for (uint64_t id : ids_to_disconnect) {
      this->ip_stack_simulator->disconnect_client(id);
    }
  }
}

string ServerState::format_address_for_channel_name(
    const struct sockaddr_storage& remote_ss, uint64_t virtual_network_id) {
  if (!virtual_network_id) {
    if (remote_ss.ss_family == 0) {
      return "__invalid_address__";
    } else {
      return "ipv4:" + render_sockaddr_storage(remote_ss);
    }
  } else {
    if (this->ip_stack_simulator) {
      auto network = this->ip_stack_simulator->get_network(virtual_network_id);
      int fd = bufferevent_getfd(network->bev.get());
      if (fd < 0) {
        return string_printf("ipss:N-%" PRIu64 ":__unknown_address__", network->network_id);
      } else {
        struct sockaddr_storage remote_ss;
        get_socket_addresses(fd, nullptr, &remote_ss);
        string addr_str = render_sockaddr_storage(remote_ss);
        return string_printf("ipss:N-%" PRIu64 ":%s", network->network_id, addr_str.c_str());
      }
    } else {
      return "__unknown_address__";
    }
  }
}
