#include "ServerState.hh"

#include <string.h>

#include <filesystem>
#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Image.hh>
#include <phosg/Network.hh>
#include <phosg/Platform.hh>

#include "Compression.hh"
#include "FileContentsCache.hh"
#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "ImageEncoder.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "Text.hh"
#include "TextIndex.hh"

using namespace std;

#ifdef PHOSG_WINDOWS
static constexpr bool IS_WINDOWS = true;
#else
static constexpr bool IS_WINDOWS = false;
#endif

CheatFlags::CheatFlags(const phosg::JSON& json) : CheatFlags() {
  unordered_set<std::string> enabled_keys;
  for (const auto& it : json.as_list()) {
    enabled_keys.emplace(it->as_string());
  }

  this->create_items = enabled_keys.count("CreateItems");
  this->edit_section_id = enabled_keys.count("EditSectionID");
  this->edit_stats = enabled_keys.count("EditStats");
  this->ep3_replace_assist = enabled_keys.count("Ep3ReplaceAssist");
  this->ep3_unset_field_character = enabled_keys.count("Ep3UnsetFieldCharacter");
  this->infinite_hp_tp = enabled_keys.count("InfiniteHPTP");
  this->fast_kills = enabled_keys.count("FastKills");
  this->insufficient_minimum_level = enabled_keys.count("InsufficientMinimumLevel");
  this->override_random_seed = enabled_keys.count("OverrideRandomSeed");
  this->override_section_id = enabled_keys.count("OverrideSectionID");
  this->override_variations = enabled_keys.count("OverrideVariations");
  this->proxy_override_drops = enabled_keys.count("ProxyOverrideDrops");
  this->reset_materials = enabled_keys.count("ResetMaterials");
  this->warp = enabled_keys.count("Warp");
}

ServerState::QuestF960Result::QuestF960Result(const phosg::JSON& json, shared_ptr<const ItemNameIndex> name_index) {
  static const array<string, 7> day_names = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  this->meseta_cost = json.get_int("MesetaCost", 0);
  this->base_probability = json.get_int("BaseProbability", 0);
  this->probability_upgrade = json.get_int("ProbabilityUpgrade", 0);
  for (size_t day = 0; day < 7; day++) {
    for (const auto& item_it : json.get_list(day_names[day])) {
      try {
        this->results[day].emplace_back(name_index->parse_item_description(item_it->as_string()));
      } catch (const exception& e) {
        config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", item_it->as_string(), e.what());
      }
    }
  }
}

ServerState::ServerState(const string& config_filename)
    : creation_time(phosg::now()),
      io_context(make_shared<asio::io_context>(1)),
      config_filename(config_filename),
      thread_pool(make_unique<asio::thread_pool>()),
      bb_stream_files_cache(new FileContentsCache(3600000000ULL)),
      bb_system_cache(new FileContentsCache(3600000000ULL)),
      gba_files_cache(new FileContentsCache(3600000000ULL)) {}

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
      primary_l->log.info_f("Unlinking watcher lobby {:X}", l->lobby_id);
      primary_l->watcher_lobbies.erase(l);
    } else {
      l->log.info_f("No watched lobby to unlink");
    }
    l->watched_lobby.reset();
  } else {
    send_ep3_disband_watcher_lobbies(l);
  }

  l->log.info_f("Unlinking lobby from index");
  this->id_to_lobby.erase(lobby_it);
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
  {
    auto ipss_channel = dynamic_pointer_cast<IPSSChannel>(c->channel);
    if (ipss_channel) {
      auto ipss_c = ipss_channel->ipss_client.lock();
      if (!ipss_c) {
        throw runtime_error("IPSS client is expired");
      }
      return IPStackSimulator::connect_address_for_remote_address(ipss_c->ipv4_addr);
    }
  }

  {
    auto socket_channel = dynamic_pointer_cast<SocketChannel>(c->channel);
    if (socket_channel) {
      uint32_t addr = ipv4_addr_for_asio_addr(socket_channel->remote_addr.address());
      uint32_t ret = is_local_address(addr) ? this->local_address : this->external_address;
      return ret ? ret : addr;
    }
  }

  {
    auto peer_channel = dynamic_pointer_cast<PeerChannel>(c->channel);
    if (peer_channel) {
      // This is used during replays; the "client" will ignore this and
      // reconnect via another PeerChannel
      return 0xEEEEEEEE;
    }
  }

  throw runtime_error("no connect address available");
}

uint16_t ServerState::game_server_port_for_version(Version v) const {
  switch (v) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->name_to_port_config.at("gc-us3")->port;
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->name_to_port_config.at("pc")->port;
    case Version::XB_V3:
      return this->name_to_port_config.at("xb")->port;
    case Version::BB_V4:
      return this->name_to_port_config.at("xb")->port;
    default:
      throw runtime_error("unknown version");
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
    case Version::DC_11_2000:
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
    case Version::DC_11_2000:
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

size_t ServerState::default_min_level_for_game(Version version, Episode episode, Difficulty difficulty) const {
  const auto& min_levels = is_v4(version)
      ? this->min_levels_v4
      : is_v3(version)
      ? this->min_levels_v3
      : this->min_levels_v1_v2;
  switch (episode) {
    case Episode::EP1:
      return min_levels[0].at(static_cast<size_t>(difficulty));
    case Episode::EP2:
      return min_levels[1].at(static_cast<size_t>(difficulty));
    case Episode::EP3:
      return 0;
    case Episode::EP4:
      return min_levels[2].at(static_cast<size_t>(difficulty));
    default:
      throw runtime_error("invalid episode");
  }
}

shared_ptr<const SetDataTableBase> ServerState::set_data_table(
    Version version, Episode episode, GameMode mode, Difficulty difficulty) const {
  bool use_ult_tables = ((episode == Episode::EP1) && (difficulty == Difficulty::ULTIMATE) && !is_v1(version) && (version != Version::PC_NTE));
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
    case Version::DC_11_2000:
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

shared_ptr<const MagEvolutionTable> ServerState::mag_evolution_table(Version version) const {
  if (is_v1_or_v2(version)) {
    return this->mag_evolution_table_v1_v2;
  } else if (!is_v4(version)) {
    return this->mag_evolution_table_v3;
  } else {
    return this->mag_evolution_table_v4;
  }
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

string ServerState::describe_item(Version version, const ItemData& item, uint8_t flags) const {
  if (is_v1(version)) {
    ItemData encoded = item;
    encoded.encode_for_version(version, this->item_parameter_table(version));
    return this->item_name_index(version)->describe_item(encoded, flags);
  } else {
    return this->item_name_index(version)->describe_item(item, flags);
  }
}

ItemData ServerState::parse_item_description(Version version, const string& description) const {
  return this->item_name_index(version)->parse_item_description(description);
}

shared_ptr<const CommonItemSet> ServerState::common_item_set(Version logic_version, shared_ptr<const Quest> q) const {
  if (q && !q->meta.common_item_set_name.empty()) {
    return this->common_item_sets.at(q->meta.common_item_set_name);
  } else if (is_v1_or_v2(logic_version) && (logic_version != Version::GC_NTE)) {
    // TODO: We should probably have a v1 common item set at some point too
    return this->common_item_sets.at("common-table-v1-v2");
  } else if ((logic_version == Version::GC_NTE) || is_v3(logic_version) || is_v4(logic_version)) {
    return this->common_item_sets.at("common-table-v3-v4");
  } else {
    throw runtime_error(std::format("no default common item set is available for {}", phosg::name_for_enum(logic_version)));
  }
}

shared_ptr<const RareItemSet> ServerState::rare_item_set(Version logic_version, shared_ptr<const Quest> q) const {
  if (q && !q->meta.rare_item_set_name.empty()) {
    return this->rare_item_sets.at(q->meta.rare_item_set_name);
  } else if (is_v1(logic_version)) {
    return this->rare_item_sets.at("rare-table-v1");
  } else if (is_v2(logic_version) && (logic_version != Version::GC_NTE)) {
    return this->rare_item_sets.at("rare-table-v2");
  } else if (is_v3(logic_version) || (logic_version == Version::GC_NTE)) {
    return this->rare_item_sets.at("rare-table-v3");
  } else if (is_v4(logic_version)) {
    return this->rare_item_sets.at("rare-table-v4");
  } else {
    throw runtime_error(std::format("no default rare item set is available for {}", phosg::name_for_enum(logic_version)));
  }
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
    if (!this->name_to_port_config.count("pc")) {
      throw runtime_error("pc port is not defined, but some ports use the pc_console_detect behavior");
    }
    if (!this->name_to_port_config.count("gc-us3")) {
      throw runtime_error("gc-us3 port is not defined, but some ports use the pc_console_detect behavior");
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
  try {
    auto ret = this->bb_system_cache->get_or_load("system/blueburst/" + effective_bb_directory_filename);
    return ret.file->data;
  } catch (const exception& e) {
    throw phosg::cannot_open_file(patch_index_filename);
  }
}

shared_ptr<const string> ServerState::load_map_file(Version version, const string& filename) const {
  if (version == Version::BB_V4) {
    try {
      return this->load_bb_file(filename);
    } catch (const exception& e) {
    }
  } else if (version == Version::PC_V2) {
    try {
      string path = "system/patch-pc/Media/PSO/" + filename;
      auto ret = make_shared<string>(phosg::load_file(path));
      return ret;
    } catch (const exception& e) {
    }
  }
  try {
    string path = std::format("system/maps/{}/{}", file_path_token_for_version(version), filename);
    auto ret = make_shared<string>(phosg::load_file(path));
    return ret;
  } catch (const exception& e) {
  }
  return nullptr;
}

pair<string, uint16_t> ServerState::parse_port_spec(const phosg::JSON& json) const {
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

vector<PortConfiguration> ServerState::parse_port_configuration(const phosg::JSON& json) const {
  vector<PortConfiguration> ret;
  for (const auto& item_json_it : json.as_dict()) {
    const auto& item_list = item_json_it.second;
    PortConfiguration& pc = ret.emplace_back();
    pc.name = item_json_it.first;
    auto spec = this->parse_port_spec(item_list->at(0));
    pc.addr = std::move(spec.first);
    pc.port = spec.second;
    pc.version = phosg::enum_for_name<Version>(item_list->at(1).as_string());
    pc.behavior = phosg::enum_for_name<ServerBehavior>(item_list->at(2).as_string());
  }
  return ret;
}

void ServerState::collect_network_addresses() {
  config_log.info_f("Reading network addresses");
  this->all_addresses = get_local_addresses();
  for (const auto& it : this->all_addresses) {
    string addr_str = string_for_address(it.second);
    config_log.info_f("Found interface: {} = {}", it.first, addr_str);
  }
}

void ServerState::load_config_early() {
  if (this->config_filename.empty()) {
    throw logic_error("configuration filename is missing");
  }

  config_log.info_f("Loading configuration");
  this->config_json = make_shared<phosg::JSON>(phosg::JSON::parse(phosg::load_file(this->config_filename)));

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
  this->num_worker_threads = this->config_json->at("WorkerThreads").as_int();

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
          this->ip_stack_addresses.emplace_back(std::format("0.0.0.0:{}", item->as_int()));
        } else if (!IS_WINDOWS) {
          this->ip_stack_addresses.emplace_back(item->as_string());
        } else {
          config_log.warning_f("Unix sockets are not supported on Windows; skipping address {}", item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("PPPStackListen").as_list()) {
        if (item->is_int()) {
          this->ppp_stack_addresses.emplace_back(std::format("0.0.0.0:{}", item->as_int()));
        } else if (!IS_WINDOWS) {
          this->ppp_stack_addresses.emplace_back(item->as_string());
        } else {
          config_log.warning_f("Unix sockets are not supported on Windows; skipping address {}", item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("PPPRawListen").as_list()) {
        if (item->is_int()) {
          this->ppp_raw_addresses.emplace_back(std::format("0.0.0.0:{}", item->as_int()));
        } else if (!IS_WINDOWS) {
          this->ppp_raw_addresses.emplace_back(item->as_string());
        } else {
          config_log.warning_f("Unix sockets are not supported on Windows; skipping address {}", item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }
    try {
      for (const auto& item : this->config_json->at("HTTPListen").as_list()) {
        if (item->is_int()) {
          this->http_addresses.emplace_back(std::format("0.0.0.0:{}", item->as_int()));
        } else if (!IS_WINDOWS) {
          this->http_addresses.emplace_back(item->as_string());
        } else {
          config_log.warning_f("Unix sockets are not supported on Windows; skipping address {}", item->as_string());
        }
      }
    } catch (const out_of_range&) {
    }

    this->one_time_config_loaded = true;
  }

  try {
    auto local_address_str = this->config_json->at("LocalAddress").as_string();
    try {
      this->local_address = this->all_addresses.at(local_address_str);
      string addr_str = string_for_address(this->local_address);
      config_log.info_f("Added local address: {} ({})", addr_str,
          local_address_str);
    } catch (const out_of_range&) {
      this->local_address = address_for_string(local_address_str.c_str());
      config_log.info_f("Added local address: {}", local_address_str);
    }
    this->all_addresses.erase("<local>");
    this->all_addresses.emplace("<local>", this->local_address);
  } catch (const out_of_range&) {
    for (const auto& it : this->all_addresses) {
      // Choose any local interface except the loopback interface
      if (!is_loopback_address(it.second) && is_local_address(it.second)) {
        this->local_address = it.second;
      }
    }
    if (this->local_address) {
      string addr_str = string_for_address(this->local_address);
      config_log.warning_f("Local address not specified; using {} as default", addr_str);
    } else {
      config_log.warning_f("Local address not specified and no default is available");
    }
  }

  try {
    auto external_address_str = this->config_json->at("ExternalAddress").as_string();
    try {
      this->external_address = this->all_addresses.at(external_address_str);
      string addr_str = string_for_address(this->external_address);
      config_log.info_f("Added external address: {} ({})", addr_str,
          external_address_str);
    } catch (const out_of_range&) {
      this->external_address = address_for_string(external_address_str.c_str());
      config_log.info_f("Added external address: {}", external_address_str);
    }
    this->all_addresses.erase("<external>");
    this->all_addresses.emplace("<external>", this->external_address);
  } catch (const out_of_range&) {
    for (const auto& it : this->all_addresses) {
      // Choose any non-local address, if any exist
      if (!is_local_address(it.second)) {
        this->external_address = it.second;
        break;
      }
    }
    if (this->external_address) {
      string addr_str = string_for_address(this->external_address);
      config_log.warning_f("External address not specified; using {} as default", addr_str);
    } else {
      config_log.warning_f("External address not specified and no default is available; only local clients will be able to connect");
    }
  }

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
  this->allow_saving_accounts = this->config_json->get_bool("AllowSavingAccounts", true);
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
  this->default_drop_mode_v1_v2_normal = this->config_json->get_enum("DefaultDropModeV1V2Normal", ServerDropMode::CLIENT);
  this->default_drop_mode_v1_v2_battle = this->config_json->get_enum("DefaultDropModeV1V2Battle", ServerDropMode::CLIENT);
  this->default_drop_mode_v1_v2_challenge = this->config_json->get_enum("DefaultDropModeV1V2Challenge", ServerDropMode::CLIENT);
  this->default_drop_mode_v3_normal = this->config_json->get_enum("DefaultDropModeV3Normal", ServerDropMode::CLIENT);
  this->default_drop_mode_v3_battle = this->config_json->get_enum("DefaultDropModeV3Battle", ServerDropMode::CLIENT);
  this->default_drop_mode_v3_challenge = this->config_json->get_enum("DefaultDropModeV3Challenge", ServerDropMode::CLIENT);
  this->default_drop_mode_v4_normal = this->config_json->get_enum("DefaultDropModeV4Normal", ServerDropMode::SERVER_SHARED);
  this->default_drop_mode_v4_battle = this->config_json->get_enum("DefaultDropModeV4Battle", ServerDropMode::SERVER_SHARED);
  this->default_drop_mode_v4_challenge = this->config_json->get_enum("DefaultDropModeV4Challenge", ServerDropMode::SERVER_SHARED);
  if ((this->default_drop_mode_v4_normal == ServerDropMode::CLIENT) ||
      (this->default_drop_mode_v4_battle == ServerDropMode::CLIENT) ||
      (this->default_drop_mode_v4_challenge == ServerDropMode::CLIENT)) {
    throw runtime_error("default V4 drop mode cannot be CLIENT");
  }
  if ((this->allowed_drop_modes_v4_normal & (1 << static_cast<size_t>(ServerDropMode::CLIENT))) ||
      (this->allowed_drop_modes_v4_battle & (1 << static_cast<size_t>(ServerDropMode::CLIENT))) || (this->allowed_drop_modes_v4_challenge & (1 << static_cast<size_t>(ServerDropMode::CLIENT)))) {
    throw runtime_error("CLIENT drop mode cannot be allowed in V4");
  }

  auto parse_quest_flag_rewrites = [&json = this->config_json](const char* key) -> unordered_map<uint16_t, IntegralExpression> {
    unordered_map<uint16_t, IntegralExpression> ret;
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
  this->enable_send_function_call_quest_numbers.clear();
  try {
    for (const auto& it : this->config_json->get_dict("EnableSendFunctionCallQuestNumbers")) {
      if (it.first.size() != 4) {
        throw runtime_error(std::format(
            "specific_version {} in EnableSendFunctionCallQuestNumbers is not a 4-byte string",
            it.first));
      }
      uint32_t specific_version = phosg::StringReader(it.first).get_u32b();
      int64_t quest_num = it.second->as_int();
      this->enable_send_function_call_quest_numbers.emplace(specific_version, quest_num);
    }
  } catch (const out_of_range&) {
  }
  this->enable_v3_v4_protected_subcommands = this->config_json->get_bool("EnableV3V4ProtectedSubcommands", false);

  auto parse_int_list = +[](const phosg::JSON& json) -> vector<uint32_t> {
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
    this->ep3_defeat_com_meseta_rewards = parse_int_list(this->config_json->get("Episode3DefeatCOMMeseta", phosg::JSON::list()));
  } catch (const out_of_range&) {
    this->ep3_defeat_com_meseta_rewards = {100, 200, 300, 400, 500};
  }
  this->ep3_final_round_meseta_bonus = this->config_json->get_int("Episode3FinalRoundMesetaBonus", 300);
  this->ep3_jukebox_is_free = this->config_json->get_bool("Episode3JukeboxIsFree", false);
  this->ep3_behavior_flags = this->config_json->get_int("Episode3BehaviorFlags", 0);
  this->ep3_card_auction_points = this->config_json->get_int("CardAuctionPoints", 0);
  this->hide_download_commands = this->config_json->get_bool("HideDownloadCommands", true);
  this->proxy_allow_save_files = this->config_json->get_bool("ProxyAllowSaveFiles", true);

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

  this->ep3_lobby_banners.clear();
  size_t banner_index = 0;
  for (const auto& it : this->config_json->get("Episode3LobbyBanners", phosg::JSON::list()).as_list()) {
    string path = "system/ep3/banners/" + it->at(2).as_string();

    string compressed_gvm_data;
    string decompressed_gvm_data;
    string lower_path = phosg::tolower(path);
    if (lower_path.ends_with(".gvm.prs")) {
      compressed_gvm_data = phosg::load_file(path);
    } else if (lower_path.ends_with(".gvm")) {
      decompressed_gvm_data = phosg::load_file(path);
    } else if (lower_path.ends_with(".bmp")) {
      auto img = phosg::ImageRGBA8888N::from_file_data(phosg::load_file(path));
      decompressed_gvm_data = encode_gvm(
          img,
          has_any_transparent_pixels(img) ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565,
          std::format("bnr{}", banner_index),
          0x80 | banner_index);
      banner_index++;
    } else {
      throw runtime_error(std::format("banner {} is in an unknown format", path));
    }

    size_t decompressed_size = decompressed_gvm_data.empty()
        ? prs_decompress_size(compressed_gvm_data)
        : decompressed_gvm_data.size();
    if (decompressed_size > 0x37000) {
      throw runtime_error(std::format("banner {} is too large (0x{:X} bytes; maximum size is 0x37000 bytes)", path, decompressed_size));
    }

    if (compressed_gvm_data.empty()) {
      compressed_gvm_data = prs_compress_optimal(decompressed_gvm_data);
    }
    if (compressed_gvm_data.size() > 0x3800) {
      throw runtime_error(std::format("banner {} cannot be compressed small enough (0x{:X} bytes; maximum size is 0x3800 bytes compressed)", it->at(2).as_string(), compressed_gvm_data.size()));
    }
    config_log.info_f("Loaded Episode 3 lobby banner {} (0x{:X} -> 0x{:X} bytes)", path, decompressed_size, compressed_gvm_data.size());
    this->ep3_lobby_banners.emplace_back(
        Ep3LobbyBannerEntry{.type = static_cast<uint32_t>(it->at(0).as_int()),
            .which = static_cast<uint32_t>(it->at(1).as_int()),
            .data = std::move(compressed_gvm_data)});
  }

  {
    auto parse_ep3_ex_result_cmd = [&](const phosg::JSON& src) -> shared_ptr<G_SetEXResultValues_Ep3_6xB4x4B> {
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

  this->bb_max_bank_items = this->config_json->get_int("BBMaxBankItems", 200);
  this->bb_max_bank_meseta = this->config_json->get_int("BBMaxBankMeseta", 999999);

  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    if (!this->item_stack_limits_tables[v_s]) {
      Version v = static_cast<Version>(v_s);
      if ((v == Version::DC_NTE) || (v == Version::DC_11_2000)) {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_DC_NTE, 999999);
      } else if (v_s < static_cast<size_t>(Version::GC_NTE)) {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V1_V2, 999999);
      } else {
        this->item_stack_limits_tables[v_s] = make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V3_V4, 999999);
      }
    }
  }

  this->bb_global_exp_multiplier = this->config_json->get_float("BBGlobalEXPMultiplier", 1.0f);
  this->exp_share_multiplier = this->config_json->get_float("BBEXPShareMultiplier", 0.5f);
  this->server_global_drop_rate_multiplier = this->config_json->get_float("ServerGlobalDropRateMultiplier", 1.0f);

  if (this->is_debug) {
    set_all_log_levels(phosg::LogLevel::L_DEBUG);
  } else {
    set_log_levels_from_json(this->config_json->get("LogLevels", phosg::JSON::dict()));
  }

  try {
    this->run_shell_behavior = this->config_json->at("RunInteractiveShell").as_bool()
        ? ServerState::RunShellBehavior::ALWAYS
        : ServerState::RunShellBehavior::NEVER;
  } catch (const out_of_range&) {
  }

  try {
    const auto& groups = this->config_json->get_list("CompatibilityGroups");
    this->compatibility_groups.fill(0);
    for (size_t v_s = 0; v_s < groups.size(); v_s++) {
      this->compatibility_groups[v_s] = groups[v_s]->as_int();
    }
  } catch (const out_of_range&) {
    static_assert(NUM_VERSIONS == 14, "Don't forget to update the default compatibility groups");
    this->compatibility_groups = {
        0x0000, // PC_PATCH
        0x0000, // BB_PATCH
        0x0004, // DC_NTE compatible only with itself
        0x0008, // DC_11_2000 compatible only with itself
        0x00B0, // DC_V1 compatible with DC_V1, DC_V2, and PC_V2
        0x00B0, // DC_V2 compatible with DC_V1, DC_V2, and PC_V2
        0x0040, // PC_NTE compatible only with itself
        0x00B0, // PC_V2 compatible with DC_V1, DC_V2, and PC_V2
        0x0100, // GC_NTE compatible only with itself
        0x1200, // GC_V3 compatible with GC_V3 and XB_V3
        0x0400, // GC_EP3_NTE compatible only with itself
        0x0800, // GC_EP3 compatible only with itself
        0x1200, // XB_V3 compatible with GC_V3 and XB_V3
        0x2000, // BB_V4 compatible only with itself
    };
  }

  this->enable_chat_commands = this->config_json->get_bool("EnableChatCommands", true);
  this->num_backup_character_slots = this->config_json->get_int("BackupCharacterSlots", 16);

  this->version_name_colors.reset();
  this->client_customization_name_color = 0;
  try {
    const auto& colors_json = this->config_json->get_list("VersionNameColors");
    if (colors_json.size() != NUM_NON_PATCH_VERSIONS) {
      throw runtime_error("VersionNameColors list length is incorrect");
    }
    auto new_colors = make_unique<array<uint32_t, NUM_NON_PATCH_VERSIONS>>();
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
    throw runtime_error(std::format(
        "QuestCategories is missing or invalid in config.json ({}) - see config.example.json for an example", e.what()));
  }

  config_log.info_f("Creating menus");

  auto information_menu_v2 = make_shared<Menu>(MenuID::INFORMATION, "Information");
  auto information_menu_v3 = make_shared<Menu>(MenuID::INFORMATION, "Information");
  shared_ptr<vector<string>> information_contents_v2 = make_shared<vector<string>>();
  shared_ptr<vector<string>> information_contents_v3 = make_shared<vector<string>>();

  information_menu_v2->items.emplace_back(InformationMenuItemID::GO_BACK, "Go back",
      "Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  information_menu_v3->items.emplace_back(InformationMenuItemID::GO_BACK, "Go back",
      "Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  {
    auto blank_json = phosg::JSON::list();
    const phosg::JSON& default_json = this->config_json->get("InformationMenuContents", blank_json);
    const phosg::JSON& v2_json = this->config_json->get("InformationMenuContentsV1V2", default_json);
    const phosg::JSON& v3_json = this->config_json->get("InformationMenuContentsV3", default_json);

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
      map<string, const phosg::JSON&> sorted_jsons;
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
        ret_pds.emplace_back(phosg::parse_netloc(netloc_str));
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
    this->proxy_destination_patch = phosg::parse_netloc(netloc_str);
    config_log.info_f("Patch server proxy is enabled with destination {}", netloc_str);
  } catch (const out_of_range&) {
    this->proxy_destination_patch.reset();
  }
  try {
    const string& netloc_str = this->config_json->get_string("ProxyDestination-BB");
    this->proxy_destination_bb = phosg::parse_netloc(netloc_str);
    config_log.info_f("BB proxy is enabled with destination {}", netloc_str);
  } catch (const out_of_range&) {
    this->proxy_destination_bb.reset();
  }

  this->welcome_message = this->config_json->get_string("WelcomeMessage", "");
  this->pc_patch_server_message = this->config_json->get_string("PCPatchServerMessage", "");
  this->bb_patch_server_message = this->config_json->get_string("BBPatchServerMessage", "");

  this->team_reward_defs_json = nullptr;
  try {
    this->team_reward_defs_json = std::move(this->config_json->at("TeamRewards"));
  } catch (const out_of_range&) {
  }

  shared_ptr<const MapState::RareEnemyRates> prev = MapState::DEFAULT_RARE_ENEMIES;
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    size_t diff_index = static_cast<size_t>(difficulty);
    try {
      string key = "RareEnemyRates-";
      key += token_name_for_difficulty(difficulty);
      this->rare_enemy_rates_by_difficulty[diff_index] = make_shared<MapState::RareEnemyRates>(this->config_json->at(key));
      prev = this->rare_enemy_rates_by_difficulty[diff_index];
    } catch (const out_of_range&) {
      this->rare_enemy_rates_by_difficulty[diff_index] = prev;
    }
  }
  try {
    this->rare_enemy_rates_challenge = make_shared<MapState::RareEnemyRates>(this->config_json->at("RareEnemyRates-Challenge"));
  } catch (const out_of_range&) {
    this->rare_enemy_rates_challenge = MapState::DEFAULT_RARE_ENEMIES;
  }

  this->min_levels_v1_v2[0] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v1_v2[1] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v1_v2[2] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v3[0] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v3[1] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v3[2] = DEFAULT_MIN_LEVELS_V123;
  this->min_levels_v4[0] = DEFAULT_MIN_LEVELS_V4_EP1;
  this->min_levels_v4[1] = DEFAULT_MIN_LEVELS_V4_EP2;
  this->min_levels_v4[2] = DEFAULT_MIN_LEVELS_V4_EP4;
  auto populate_min_levels = [&](std::array<std::array<size_t, 4>, 3>& dest, const char* key_name) -> void {
    try {
      for (const auto& ep_it : this->config_json->get_dict(key_name)) {
        array<size_t, 4> levels({0, 0, 0, 0});
        for (size_t z = 0; z < 4; z++) {
          levels[z] = ep_it.second->get_int(z) - 1;
        }
        switch (episode_for_token_name(ep_it.first)) {
          case Episode::EP1:
            dest[0] = levels;
            break;
          case Episode::EP2:
            dest[1] = levels;
            break;
          case Episode::EP4:
            dest[2] = levels;
            break;
          default:
            throw runtime_error("unknown episode");
        }
      }
    } catch (const out_of_range&) {
    }
  };
  populate_min_levels(this->min_levels_v1_v2, "V1V2MinimumLevels");
  populate_min_levels(this->min_levels_v3, "V3MinimumLevels");
  populate_min_levels(this->min_levels_v4, "BBMinimumLevels");

  this->bb_required_patches.clear();
  try {
    for (const auto& it : this->config_json->get_list("BBRequiredPatches")) {
      this->bb_required_patches.emplace(it->as_string());
    }
  } catch (const out_of_range&) {
  }
  this->auto_patches.clear();
  try {
    for (const auto& it : this->config_json->get_list("AutoPatches")) {
      this->auto_patches.emplace(it->as_string());
    }
  } catch (const out_of_range&) {
  }

  try {
    this->cheat_flags = CheatFlags(this->config_json->at("CheatingBehaviors"));
  } catch (const out_of_range&) {
    this->cheat_flags = CheatFlags();
  }
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
        throw runtime_error(std::format("Ep3 card \"{}\" in auction pool does not exist", it.first));
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
                throw runtime_error(std::format("Ep3 card \"{}\" in trap card list is not an assist card", card_name));
              }
              trap_card_ids.emplace_back(card->def.card_id);
            } catch (const out_of_range&) {
              throw runtime_error(std::format("Ep3 card \"{}\" in trap card list does not exist", card_name));
            }
          }
        }
      }
    } catch (const out_of_range&) {
    }
  } else {
    config_log.warning_f("Episode 3 card definitions missing; cannot set trap card IDs from config");
  }

  this->quest_F95E_results.clear();
  this->quest_F95F_results.clear();
  this->quest_F960_success_results.clear();
  this->quest_F960_failure_results = QuestF960Result();
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
              config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", item_it->as_string(), e.what());
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
          config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", list.at(1)->as_string(), e.what());
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

    auto parse_primary_identifier_list = [&](const char* key, Version v) -> unordered_set<uint32_t> {
      unordered_set<uint32_t> ret;
      try {
        for (const auto& pi_json : this->config_json->get_list(key)) {
          if (pi_json->is_int()) {
            ret.emplace(pi_json->as_int());
          } else {
            try {
              auto item = this->parse_item_description(v, pi_json->as_string());
              ret.emplace(item.primary_identifier());
            } catch (const exception& e) {
              config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", pi_json->as_string(), e.what());
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
    config_log.warning_f("BB item name index is missing; cannot load quest reward lists from config");
  }
}

void ServerState::load_bb_private_keys() {
  vector<shared_ptr<const PSOBBEncryption::KeyFile>> new_keys;
  for (const auto& item : std::filesystem::directory_iterator("system/blueburst/keys")) {
    string filename = item.path().filename().string();
    if (!filename.ends_with(".nsk")) {
      continue;
    }
    new_keys.emplace_back(make_shared<PSOBBEncryption::KeyFile>(
        phosg::load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.debug_f("Loaded Blue Burst key file: {}", filename);
  }
  this->bb_private_keys = std::move(new_keys);
}

void ServerState::load_bb_system_defaults() {
  try {
    this->bb_default_keyboard_config = make_shared<parray<uint8_t, 0x16C>>(phosg::load_object_file<parray<uint8_t, 0x16C>>("system/blueburst/default-keyboard-config.bin"));
    config_log.info_f("Default Blue Burst keyboard config is present");
  } catch (const phosg::cannot_open_file&) {
  }
  try {
    this->bb_default_joystick_config = make_shared<parray<uint8_t, 0x38>>(phosg::load_object_file<parray<uint8_t, 0x38>>("system/blueburst/default-joystick-config.bin"));
    config_log.info_f("Default Blue Burst joystick config is present");
  } catch (const phosg::cannot_open_file&) {
  }
}

void ServerState::load_accounts() {
  config_log.info_f("Indexing accounts");
  this->account_index = make_shared<AccountIndex>(!this->allow_saving_accounts);
}

void ServerState::load_teams() {
  config_log.info_f("Indexing teams");
  this->team_index = make_shared<TeamIndex>("system/teams", this->team_reward_defs_json);
}

void ServerState::load_patch_indexes() {
  shared_ptr<const GSLArchive> bb_data_gsl;
  shared_ptr<PatchFileIndex> pc_patch_file_index;
  shared_ptr<PatchFileIndex> bb_patch_file_index;

  if (std::filesystem::is_directory("system/patch-pc")) {
    config_log.info_f("Indexing PSO PC patch files");
    pc_patch_file_index = make_shared<PatchFileIndex>("system/patch-pc");
  } else {
    config_log.info_f("PSO PC patch files not present");
  }
  if (std::filesystem::is_directory("system/patch-bb")) {
    config_log.info_f("Indexing PSO BB patch files");
    bb_patch_file_index = make_shared<PatchFileIndex>("system/patch-bb");
    try {
      auto gsl_file = bb_patch_file_index->get("./data/data.gsl");
      bb_data_gsl = make_shared<GSLArchive>(gsl_file->load_data(), false);
      config_log.info_f("data.gsl found in BB patch files");
    } catch (const out_of_range&) {
      config_log.info_f("data.gsl is not present in BB patch files");
    }
  } else {
    config_log.info_f("PSO BB patch files not present");
  }

  this->bb_data_gsl = std::move(bb_data_gsl);
  this->pc_patch_file_index = std::move(pc_patch_file_index);
  this->bb_patch_file_index = std::move(bb_patch_file_index);
}

void ServerState::load_maps() {
  using SDT = SetDataTable;

  config_log.info_f("Loading map layouts");
  auto new_room_layout_index = make_shared<RoomLayoutIndex>(
      phosg::JSON::parse(phosg::load_file("system/maps/room-layout-index.json")));

  config_log.info_f("Loading Episode 3 Morgue maps");
  unordered_map<uint64_t, shared_ptr<const MapFile>> new_map_file_for_source_hash;
  map<uint32_t, array<shared_ptr<const MapFile>, NUM_VERSIONS>> new_map_files_for_free_play_key;
  {
    // TODO: Ep3 NTE loads map_city00_on, but it appears there are some
    // variants. Figure this out and load those maps too.
    auto objects_data = this->load_map_file(Version::GC_EP3, "map_city_on_battle_o.dat");
    auto enemies_data = this->load_map_file(Version::GC_EP3, "map_city_on_battle_e.dat");
    if (objects_data || enemies_data) {
      uint32_t free_play_key = this->free_play_key(Episode::EP3, GameMode::NORMAL, Difficulty::NORMAL, 0, 0, 0);
      auto map_file = make_shared<MapFile>(0, objects_data, enemies_data, nullptr);
      new_map_file_for_source_hash.emplace(map_file->source_hash(), map_file);
      new_map_files_for_free_play_key[free_play_key].at(static_cast<size_t>(Version::GC_EP3)) = map_file;
      config_log.info_f("Episode 3 map files loaded with free play key {:08X}", free_play_key);
    } else {
      config_log.info_f("Episode 3 map files not found; skipping");
    }
  }

  config_log.info_f("Loading free play map files");
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    for (Episode episode : ALL_EPISODES_V4) {
      if ((episode == Episode::EP2 && is_v1_or_v2(v) && (v != Version::GC_NTE)) ||
          (episode == Episode::EP4 && !is_v4(v))) {
        continue;
      }

      for (GameMode mode : ALL_GAME_MODES_V4) {
        if ((mode == GameMode::BATTLE) && is_pre_v1(v)) {
          continue;
        }
        if ((mode == GameMode::CHALLENGE) && is_v1(v)) {
          continue;
        }
        if ((mode == GameMode::SOLO && !is_v4(v))) {
          continue;
        }
        for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
          if ((difficulty == Difficulty::ULTIMATE) && is_v1(v)) {
            continue;
          }
          auto sdt = this->set_data_table(v, episode, mode, difficulty);
          for (uint8_t floor = 0; floor < 0x12; floor++) {
            auto variation_maxes = sdt->num_free_play_variations_for_floor(episode, mode == GameMode::SOLO, floor);
            for (size_t var_layout = 0; var_layout < variation_maxes.layout; var_layout++) {
              for (size_t var_entities = 0; var_entities < variation_maxes.entities; var_entities++) {
                uint32_t free_play_key = this->free_play_key(episode, mode, difficulty, floor, var_layout, var_entities);

                auto objects_filename = sdt->map_filename_for_variation(
                    episode, mode, floor, var_layout, var_entities, SDT::FilenameType::OBJECT_SETS);
                auto enemies_filename = sdt->map_filename_for_variation(
                    episode, mode, floor, var_layout, var_entities, SDT::FilenameType::ENEMY_SETS);
                auto events_filename = sdt->map_filename_for_variation(
                    episode, mode, floor, var_layout, var_entities, SDT::FilenameType::EVENTS);
                auto objects_data = objects_filename.empty() ? nullptr : this->load_map_file(v, objects_filename);
                auto enemies_data = enemies_filename.empty() ? nullptr : this->load_map_file(v, enemies_filename);
                auto events_data = enemies_filename.empty() ? nullptr : this->load_map_file(v, events_filename);

                if (objects_data || enemies_data || events_data) {
                  // TODO: This is ugly; the hash computation probably should be factored into MapFile
                  uint64_t source_hash = ((objects_data ? phosg::fnv1a64(*objects_data) : 0) ^
                      (enemies_data ? phosg::fnv1a64(*enemies_data) : 0) ^
                      (events_data ? phosg::fnv1a64(*events_data) : 0));
                  shared_ptr<const MapFile> map_file;
                  try {
                    map_file = new_map_file_for_source_hash.at(source_hash);
                  } catch (const out_of_range&) {
                    map_file = make_shared<MapFile>(floor, objects_data, enemies_data, events_data);
                    if (map_file->source_hash() != source_hash) {
                      throw logic_error("incorrect source hash");
                    }
                    new_map_file_for_source_hash.emplace(map_file->source_hash(), map_file);
                  }

                  // Uncomment for debugging
                  // config_log.info_f("Maps for {} {} {} {} {:02X} {:02} {:02} ({:08X} => {:016X}): objects={}({})+0x{:X} enemies={}({})+0x{:X} events={}({})+0x{:X}",
                  //     phosg::name_for_enum(v),
                  //     name_for_episode(episode),
                  //     name_for_mode(mode),
                  //     name_for_difficulty(difficulty),
                  //     floor,
                  //     var_layout,
                  //     var_entities,
                  //     free_play_key,
                  //     map_file->source_hash(),
                  //     objects_filename.empty() ? "(none)" : objects_filename,
                  //     objects_data ? "present" : "missing",
                  //     map_file->count_object_sets(),
                  //     enemies_filename.empty() ? "(none)" : enemies_filename,
                  //     enemies_data ? "present" : "missing",
                  //     map_file->count_enemy_sets(),
                  //     events_filename.empty() ? "(none)" : events_filename,
                  //     events_data ? "present" : "missing",
                  //     map_file->count_events());

                  new_map_files_for_free_play_key[free_play_key].at(static_cast<size_t>(v)) = map_file;
                }
              }
            }
          }
        }
      }
    }
  }

  this->map_file_for_source_hash = std::move(new_map_file_for_source_hash);
  this->map_files_for_free_play_key = std::move(new_map_files_for_free_play_key);
  this->room_layout_index = new_room_layout_index;
  this->supermap_for_source_hash_sum.clear();
  this->supermap_for_free_play_key.clear();
}

shared_ptr<const SuperMap> ServerState::get_free_play_supermap(
    Episode episode, GameMode mode, Difficulty difficulty, uint8_t floor, uint32_t layout, uint32_t entities) {
  uint32_t free_play_key = this->free_play_key(episode, mode, difficulty, floor, layout, entities);
  try {
    return this->supermap_for_free_play_key.at(free_play_key);
  } catch (const out_of_range&) {
  }

  const array<shared_ptr<const MapFile>, NUM_VERSIONS>* map_files;
  try {
    map_files = &this->map_files_for_free_play_key.at(free_play_key);
  } catch (const out_of_range&) {
    static_game_data_log.info_f("No maps exist for key {:08X}; cannot construct supermap", free_play_key);
    this->supermap_for_free_play_key.emplace(free_play_key, nullptr);
    return nullptr;
  }

  uint64_t source_hash_sum = 0;
  for (auto map_file : *map_files) {
    source_hash_sum += map_file ? map_file->source_hash() : 0;
  }

  // Uncomment for debugging
  // phosg::fwrite_fmt(stderr, "SuperMap for {} {} {} {:02X} {:02X} {:02X} ({:08X}): {:016X} from",
  //     name_for_episode(episode),
  //     name_for_mode(mode),
  //     name_for_difficulty(difficulty),
  //     floor,
  //     layout,
  //     entities,
  //     free_play_key,
  //     source_hash_sum);
  // for (const auto& map_file : it.second) {
  //   if (map_file) {
  //     phosg::fwrite_fmt(stderr, " {:016X}", map_file->source_hash());
  //   } else {
  //     phosg::fwrite_fmt(stderr, " ----------------");
  //   }
  // }
  // fputc('\n', stderr);

  shared_ptr<const SuperMap> supermap;
  try {
    supermap = this->supermap_for_source_hash_sum.at(source_hash_sum);
    static_game_data_log.info_f("Linking existing free play supermap {:016X} for key {:08X}", source_hash_sum, free_play_key);
  } catch (const out_of_range&) {
    supermap = make_shared<SuperMap>(*map_files, SetDataTableBase::default_floor_to_area(Version::BB_V4, episode));
    this->supermap_for_source_hash_sum.emplace(source_hash_sum, supermap);
    static_game_data_log.info_f("Constructed free play supermap {:016X} for key {:08X}", source_hash_sum, free_play_key);
  }
  this->supermap_for_free_play_key.emplace(free_play_key, supermap);
  return supermap;
}

vector<shared_ptr<const SuperMap>> ServerState::supermaps_for_variations(
    Episode episode, GameMode mode, Difficulty difficulty, const Variations& variations) {
  vector<shared_ptr<const SuperMap>> ret;
  for (size_t floor = 0; floor < 0x12; floor++) {
    Variations::Entry e;
    if (floor < variations.entries.size()) {
      e = variations.entries[floor];
    }
    ret.push_back(this->get_free_play_supermap(episode, mode, difficulty, floor, e.layout, e.entities));
    if (ret.back()) {
      static_game_data_log.info_f("Using supermap {:08X} for floor {:02X} layout {:X} entities {:X}",
          this->free_play_key(episode, mode, difficulty, floor, e.layout, e.entities),
          floor, e.layout, e.entities);
    } else {
      static_game_data_log.info_f("No supermap available for floor {:02X} layout {:X} entities {:X}",
          floor, e.layout, e.entities);
    }
  }
  return ret;
}

void ServerState::clear_file_caches() {
  config_log.info_f("Clearing BB stream file cache");
  this->bb_stream_files_cache.reset(new FileContentsCache(3600000000ULL));
  config_log.info_f("Clearing BB system cache");
  this->bb_system_cache.reset(new FileContentsCache(3600000000ULL));
  config_log.info_f("Clearing GBA file cache");
  this->gba_files_cache.reset(new FileContentsCache(300 * 1000 * 1000));
}

void ServerState::load_set_data_tables() {
  config_log.info_f("Loading set data tables");

  array<shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables;
  array<shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables_ep1_ult;
  shared_ptr<const SetDataTableBase> new_table_bb_solo;
  shared_ptr<const SetDataTableBase> new_table_bb_solo_ep1_ult;

  auto load_table = [&](Version version) -> void {
    auto data = this->load_map_file(version, "SetDataTableOn.rel");
    new_tables[static_cast<size_t>(version)] = make_shared<SetDataTable>(version, *data);
    if (!is_v1(version) && (version != Version::PC_NTE)) {
      auto data_ep1_ult = this->load_map_file(version, "SetDataTableOnUlti.rel");
      new_tables_ep1_ult[static_cast<size_t>(version)] = make_shared<SetDataTable>(version, *data_ep1_ult);
    }
  };

  new_tables[static_cast<size_t>(Version::DC_NTE)] = make_shared<SetDataTableDCNTE>();
  new_tables[static_cast<size_t>(Version::DC_11_2000)] = make_shared<SetDataTableDC112000>();
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

  this->set_data_tables = std::move(new_tables);
  this->set_data_tables_ep1_ult = std::move(new_tables_ep1_ult);
  this->bb_solo_set_data_table = std::move(new_table_bb_solo);
  this->bb_solo_set_data_table_ep1_ult = std::move(new_table_bb_solo_ep1_ult);
}

void ServerState::load_battle_params() {
  config_log.info_f("Loading battle parameters");
  this->battle_params = make_shared<BattleParamsIndex>(
      this->load_bb_file("BattleParamEntry_on.dat"),
      this->load_bb_file("BattleParamEntry_lab_on.dat"),
      this->load_bb_file("BattleParamEntry_ep4_on.dat"),
      this->load_bb_file("BattleParamEntry.dat"),
      this->load_bb_file("BattleParamEntry_lab.dat"),
      this->load_bb_file("BattleParamEntry_ep4.dat"));
}

void ServerState::load_level_tables() {
  config_log.info_f("Loading level tables");
  this->level_table_v1_v2 = make_shared<LevelTableV2>(phosg::load_file("system/level-tables/PlayerTable-pc-v2.prs"), true);
  this->level_table_v3 = make_shared<LevelTableV3BE>(phosg::load_file("system/level-tables/PlyLevelTbl-gc-v3.cpt"), true);
  this->level_table_v4 = make_shared<LevelTableV4>(*this->load_bb_file("PlyLevelTbl.prs"), true);
}

void ServerState::load_text_index() {
  this->text_index = make_shared<TextIndex>("system/text-sets", [&](Version version, const string& filename) -> shared_ptr<const string> {
    try {
      if (version == Version::BB_V4) {
        return this->load_bb_file(filename);
      } else {
        return this->pc_patch_file_index->get("Media/PSO/" + filename)->load_data();
      }
    } catch (const out_of_range&) {
      return nullptr;
    } catch (const phosg::cannot_open_file&) {
      return nullptr;
    }
  });
}

void ServerState::load_word_select_table() {
  config_log.info_f("Loading Word Select table");

  vector<vector<string>> name_alias_lists;
  auto json = phosg::JSON::parse(phosg::load_file("system/text-sets/ws-name-alias-lists.json"));
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
    config_log.debug_f("(Word select) Using PC_V2 unitxt_e.prs from text index");
    pc_unitxt_collection = &this->text_index->get(Version::PC_V2, Language::ENGLISH, 35);
  } else {
    config_log.debug_f("(Word select) Loading PC_V2 unitxt_e.prs");
    pc_unitxt_data = make_unique<UnicodeTextSet>(phosg::load_file("system/text-sets/pc-v2/unitxt_e.prs"));
    pc_unitxt_collection = &pc_unitxt_data->get(35);
  }
  config_log.debug_f("(Word select) Loading BB_V4 unitxt_ws_e.prs");
  auto bb_unitxt_data = make_unique<UnicodeTextSet>(phosg::load_file("system/text-sets/bb-v4/unitxt_ws_e.prs"));
  bb_unitxt_collection = &bb_unitxt_data->get(0);

  config_log.debug_f("(Word select) Loading DC_NTE data");
  WordSelectSet dc_nte_ws(phosg::load_file("system/text-sets/dc-nte/ws_data.bin"), Version::DC_NTE, nullptr, true);
  config_log.debug_f("(Word select) Loading DC_11_2000 data");
  WordSelectSet dc_112000_ws(phosg::load_file("system/text-sets/dc-11-2000/ws_data.bin"), Version::DC_11_2000, nullptr, false);
  config_log.debug_f("(Word select) Loading DC_V1 data");
  WordSelectSet dc_v1_ws(phosg::load_file("system/text-sets/dc-v1/ws_data.bin"), Version::DC_V1, nullptr, false);
  config_log.debug_f("(Word select) Loading DC_V2 data");
  WordSelectSet dc_v2_ws(phosg::load_file("system/text-sets/dc-v2/ws_data.bin"), Version::DC_V2, nullptr, false);
  config_log.debug_f("(Word select) Loading PC_NTE data");
  WordSelectSet pc_nte_ws(phosg::load_file("system/text-sets/pc-nte/ws_data.bin"), Version::PC_NTE, pc_unitxt_collection, false);
  config_log.debug_f("(Word select) Loading PC_V2 data");
  WordSelectSet pc_v2_ws(phosg::load_file("system/text-sets/pc-v2/ws_data.bin"), Version::PC_V2, pc_unitxt_collection, false);
  config_log.debug_f("(Word select) Loading GC_NTE data");
  WordSelectSet gc_nte_ws(phosg::load_file("system/text-sets/gc-nte/ws_data.bin"), Version::GC_NTE, nullptr, false);
  config_log.debug_f("(Word select) Loading GC_V3 data");
  WordSelectSet gc_v3_ws(phosg::load_file("system/text-sets/gc-v3/ws_data.bin"), Version::GC_V3, nullptr, false);
  config_log.debug_f("(Word select) Loading GC_EP3_NTE data");
  WordSelectSet gc_ep3_nte_ws(phosg::load_file("system/text-sets/gc-ep3-nte/ws_data.bin"), Version::GC_EP3_NTE, nullptr, false);
  config_log.debug_f("(Word select) Loading GC_EP3 data");
  WordSelectSet gc_ep3_ws(phosg::load_file("system/text-sets/gc-ep3/ws_data.bin"), Version::GC_EP3, nullptr, false);
  config_log.debug_f("(Word select) Loading XB_V3 data");
  WordSelectSet xb_v3_ws(phosg::load_file("system/text-sets/xb-v3/ws_data.bin"), Version::XB_V3, nullptr, false);
  config_log.debug_f("(Word select) Loading BB_V4 data");
  WordSelectSet bb_v4_ws(phosg::load_file("system/text-sets/bb-v4/ws_data.bin"), Version::BB_V4, bb_unitxt_collection, false);

  config_log.debug_f("(Word select) Generating table");
  this->word_select_table = make_shared<WordSelectTable>(
      dc_nte_ws, dc_112000_ws, dc_v1_ws, dc_v2_ws,
      pc_nte_ws, pc_v2_ws, gc_nte_ws, gc_v3_ws,
      gc_ep3_nte_ws, gc_ep3_ws, xb_v3_ws, bb_v4_ws,
      name_alias_lists);
}

shared_ptr<ItemNameIndex> ServerState::create_item_name_index_for_version(
    shared_ptr<const ItemParameterTable> pmt,
    shared_ptr<const ItemData::StackLimits> limits,
    shared_ptr<const TextIndex> text_index) const {
  switch (limits->version) {
    case Version::DC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_NTE, Language::JAPANESE, 2));
    case Version::DC_11_2000:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_11_2000, Language::ENGLISH, 2));
    case Version::DC_V1:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V1, Language::ENGLISH, 2));
    case Version::DC_V2:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V2, Language::ENGLISH, 3));
    case Version::PC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_NTE, Language::ENGLISH, 3));
    case Version::PC_V2:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_V2, Language::ENGLISH, 3));
    case Version::GC_NTE:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_NTE, Language::ENGLISH, 0));
    case Version::GC_V3:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_V3, Language::ENGLISH, 0));
    case Version::XB_V3:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::XB_V3, Language::ENGLISH, 0));
    case Version::BB_V4:
      return make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::BB_V4, Language::ENGLISH, 1));
    default:
      return nullptr;
  }
}

void ServerState::load_item_name_indexes() {
  config_log.info_f("Generating item name indexes");
  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s);
    config_log.debug_f("Generating item name index for {}", phosg::name_for_enum(v));
    this->item_name_indexes[v_s] = this->create_item_name_index_for_version(
        this->item_parameter_table(v), this->item_stack_limits(v), this->text_index);
  }
  this->item_name_indexes[static_cast<size_t>(Version::GC_EP3)] = this->item_name_indexes[static_cast<size_t>(Version::GC_V3)];
  this->item_name_indexes[static_cast<size_t>(Version::GC_EP3_NTE)] = this->item_name_indexes[static_cast<size_t>(Version::GC_V3)];
}

void ServerState::load_drop_tables() {
  config_log.info_f("Loading item sets");

  unordered_map<string, shared_ptr<const RareItemSet>> new_rare_item_sets;
  unordered_map<string, shared_ptr<const CommonItemSet>> new_common_item_sets;
  for (const auto& item : std::filesystem::directory_iterator("system/item-tables")) {
    string filename = item.path().filename().string();

    if (filename.starts_with("common-table-") || filename.starts_with("ItemPT-")) {
      string path = "system/item-tables/" + filename;
      size_t ext_offset = filename.rfind('.');
      string basename = (ext_offset == string::npos) ? filename : filename.substr(0, ext_offset);

      // AFSV2CommonItemSet(std::shared_ptr<const std::string> pt_afs_data, std::shared_ptr<const std::string> ct_afs_data);

      if (filename.ends_with(".json")) {
        config_log.info_f("Loading JSON common item table {}", filename);
        new_common_item_sets.emplace(basename, make_shared<JSONCommonItemSet>(phosg::JSON::parse(phosg::load_file(path))));
      } else if (filename.ends_with(".afs")) {
        string ct_filename;
        if (filename.starts_with("ItemPT-")) {
          ct_filename = "ItemCT-" + filename.substr(7);
        } else if (filename.starts_with("common-table-")) {
          ct_filename = "challenge-common-table-" + filename.substr(13);
        } else {
          throw std::runtime_error(std::format("cannot determine challenge table filename for common table file: {}", filename));
        }
        auto data = make_shared<string>(phosg::load_file(path));
        shared_ptr<string> ct_data;
        try {
          string ct_path = "system/item-tables/" + ct_filename;
          ct_data = make_shared<string>(phosg::load_file(ct_path));
          config_log.info_f("Loading AFS common item table {} with challenge table {}", filename, ct_filename);
        } catch (const phosg::cannot_open_file&) {
          config_log.info_f("Loading AFS common item table {} without challenge table", filename);
        }
        new_common_item_sets.emplace(basename, make_shared<AFSV2CommonItemSet>(data, ct_data));
      } else if (filename.ends_with(".gsl")) {
        config_log.info_f("Loading little-endian GSL common item table {}", filename);
        auto data = make_shared<string>(phosg::load_file(path));
        new_common_item_sets.emplace(basename, make_shared<GSLV3V4CommonItemSet>(data, false));
      } else if (filename.ends_with(".gslb")) {
        config_log.info_f("Loading big-endian GSL common item table {}", filename);
        auto data = make_shared<string>(phosg::load_file(path));
        new_common_item_sets.emplace(basename, make_shared<GSLV3V4CommonItemSet>(data, true));
      } else {
        throw std::runtime_error(std::format("unknown format for common table file: {}", filename));
      }

    } else if (filename.starts_with("rare-table-") || filename.starts_with("ItemRT-")) {
      string path = "system/item-tables/" + filename;
      size_t ext_offset = filename.rfind('.');
      string basename = (ext_offset == string::npos) ? filename : filename.substr(0, ext_offset);

      shared_ptr<RareItemSet> rare_set;
      if (filename.ends_with("-v1.json")) {
        config_log.info_f("Loading v1 JSON rare item table {}", filename);
        rare_set = make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::DC_V1));
      } else if (filename.ends_with("-v2.json")) {
        config_log.info_f("Loading v2 JSON rare item table {}", filename);
        rare_set = make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::PC_V2));
      } else if (filename.ends_with("-v3.json")) {
        config_log.info_f("Loading v3 JSON rare item table {}", filename);
        rare_set = make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::GC_V3));
      } else if (filename.ends_with("-v4.json")) {
        config_log.info_f("Loading v4 JSON rare item table {}", filename);
        rare_set = make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::BB_V4));

      } else if (filename.ends_with(".afs")) {
        config_log.info_f("Loading AFS rare item table {}", filename);
        auto data = make_shared<string>(phosg::load_file(path));
        rare_set = make_shared<RareItemSet>(AFSArchive(data), false);

      } else if (filename.ends_with(".gsl")) {
        config_log.info_f("Loading GSL rare item table {}", filename);
        auto data = make_shared<string>(phosg::load_file(path));
        rare_set = make_shared<RareItemSet>(GSLArchive(data, false), false);

      } else if (filename.ends_with(".gslb")) {
        config_log.info_f("Loading GSL rare item table {}", filename);
        auto data = make_shared<string>(phosg::load_file(path));
        rare_set = make_shared<RareItemSet>(GSLArchive(data, true), true);

      } else if (filename.ends_with(".rel")) {
        config_log.info_f("Loading REL rare item table {}", filename);
        rare_set = make_shared<RareItemSet>(phosg::load_file(path), true);

      } else {
        throw std::runtime_error(std::format("unknown format for rare table file: {}", filename));
      }

      if (this->server_global_drop_rate_multiplier != 1.0) {
        rare_set->multiply_all_rates(this->server_global_drop_rate_multiplier);
      }
      new_rare_item_sets.emplace(basename, std::move(rare_set));
    }
  }

  config_log.info_f("Loading armor table");
  auto armor_data = make_shared<string>(phosg::load_file("system/item-tables/ArmorRandom-gc-v3.rel"));
  auto new_armor_random_set = make_shared<ArmorRandomSet>(armor_data);

  config_log.info_f("Loading tool table");
  auto tool_data = make_shared<string>(phosg::load_file("system/item-tables/ToolRandom-gc-v3.rel"));
  auto new_tool_random_set = make_shared<ToolRandomSet>(tool_data);

  config_log.info_f("Loading weapon tables");
  array<shared_ptr<const WeaponRandomSet>, 4> new_weapon_random_sets;
  const char* filenames[4] = {
      "system/item-tables/WeaponRandomNormal-gc-v3.rel",
      "system/item-tables/WeaponRandomHard-gc-v3.rel",
      "system/item-tables/WeaponRandomVeryHard-gc-v3.rel",
      "system/item-tables/WeaponRandomUltimate-gc-v3.rel",
  };
  for (size_t z = 0; z < 4; z++) {
    auto weapon_data = make_shared<string>(phosg::load_file(filenames[z]));
    new_weapon_random_sets[z] = make_shared<WeaponRandomSet>(weapon_data);
  }

  config_log.info_f("Loading tekker adjustment table");
  auto tekker_data = make_shared<string>(phosg::load_file("system/item-tables/JudgeItem-gc-v3.rel"));
  auto new_tekker_adjustment_set = make_shared<TekkerAdjustmentSet>(tekker_data);

  this->rare_item_sets = std::move(new_rare_item_sets);
  this->common_item_sets = std::move(new_common_item_sets);
  this->armor_random_set = std::move(new_armor_random_set);
  this->tool_random_set = std::move(new_tool_random_set);
  this->weapon_random_sets = std::move(new_weapon_random_sets);
  this->tekker_adjustment_set = std::move(new_tekker_adjustment_set);
}

void ServerState::load_item_definitions() {
  array<shared_ptr<const ItemParameterTable>, NUM_VERSIONS> new_item_parameter_tables;
  config_log.info_f("Loading item definition tables");
  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s);
    string path = std::format("system/item-tables/ItemPMT-{}.prs", file_path_token_for_version(v));
    config_log.debug_f("Loading item definition table {}", path);
    auto data = make_shared<string>(prs_decompress(phosg::load_file(path)));
    new_item_parameter_tables[v_s] = make_shared<ItemParameterTable>(data, v);
  }

  auto json = phosg::JSON::parse(phosg::load_file("system/item-tables/translation-table.json"));
  auto new_item_translation_table = make_shared<ItemTranslationTable>(json, new_item_parameter_tables);

  // TODO: We should probably load the tables for other versions too.
  config_log.info_f("Loading v1/v2 mag evolution table");
  auto mag_data_v1_v2 = make_shared<string>(prs_decompress(phosg::load_file("system/item-tables/ItemMagEdit-dc-v2.prs")));
  auto new_table_v1_v2 = make_shared<MagEvolutionTable>(mag_data_v1_v2, 0x3A);
  config_log.info_f("Loading v3 mag evolution table");
  auto mag_data_v3 = make_shared<string>(prs_decompress(phosg::load_file("system/item-tables/ItemMagEdit-xb-v3.prs")));
  auto new_table_v3 = make_shared<MagEvolutionTable>(mag_data_v3, 0x43);
  config_log.info_f("Loading v4 mag evolution table");
  auto mag_data_v4 = make_shared<string>(prs_decompress(phosg::load_file("system/item-tables/ItemMagEdit-bb-v4.prs")));
  auto new_table_v4 = make_shared<MagEvolutionTable>(mag_data_v4, 0x53);

  this->item_parameter_tables = std::move(new_item_parameter_tables);
  this->item_translation_table = std::move(new_item_translation_table);
  this->mag_evolution_table_v1_v2 = std::move(new_table_v1_v2);
  this->mag_evolution_table_v3 = std::move(new_table_v3);
  this->mag_evolution_table_v4 = std::move(new_table_v4);
}

void ServerState::load_ep3_cards() {
  config_log.info_f("Loading Episode 3 card definitions");
  this->ep3_card_index = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions.mnr",
      "system/ep3/card-definitions.mnrd",
      "system/ep3/card-text.mnr",
      "system/ep3/card-text.mnrd",
      "system/ep3/card-dice-text.mnr",
      "system/ep3/card-dice-text.mnrd");
  config_log.info_f("Loading Episode 3 trial card definitions");
  this->ep3_card_index_trial = make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions-trial.mnr",
      "system/ep3/card-definitions-trial.mnrd",
      "system/ep3/card-text-trial.mnr",
      "system/ep3/card-text-trial.mnrd",
      "system/ep3/card-dice-text-trial.mnr",
      "system/ep3/card-dice-text-trial.mnrd");
  config_log.info_f("Loading Episode 3 COM decks");
  this->ep3_com_deck_index = make_shared<Episode3::COMDeckIndex>("system/ep3/com-decks.json");
}

void ServerState::load_ep3_maps(bool raise_on_any_failure) {
  config_log.info_f("Collecting Episode 3 maps");
  this->ep3_map_index = make_shared<Episode3::MapIndex>("system/ep3/maps", raise_on_any_failure);
}

void ServerState::load_ep3_tournament_state() {
  config_log.info_f("Loading Episode 3 tournament state");
  const string& tournament_state_filename = "system/ep3/tournament-state.json";
  this->ep3_tournament_index = make_shared<Episode3::TournamentIndex>(
      this->ep3_map_index, this->ep3_com_deck_index, tournament_state_filename);
  this->ep3_tournament_index->link_all_clients(this->shared_from_this());
}

void ServerState::load_quest_index(bool raise_on_any_failure) {
  config_log.info_f("Collecting quests");
  this->quest_index = make_shared<QuestIndex>("system/quests", this->quest_category_index, raise_on_any_failure);
}

void ServerState::compile_functions(bool raise_on_any_failure) {
  config_log.info_f("Compiling client functions");
  this->function_code_index = make_shared<FunctionCodeIndex>("system/client-functions", raise_on_any_failure);
}

void ServerState::load_dol_files() {
  config_log.info_f("Loading DOL files");
  this->dol_file_index = make_shared<DOLFileIndex>("system/dol");
}

void ServerState::create_default_lobbies() {
  if (this->default_lobbies_created) {
    return;
  }
  this->default_lobbies_created = true;

  vector<shared_ptr<Lobby>> non_v1_only_lobbies;
  vector<shared_ptr<Lobby>> ep3_only_lobbies;

  for (size_t x = 0; x < 20; x++) {
    auto lobby_name = std::format("LOBBY{}", x + 1);
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
        l->allow_version(Version::DC_11_2000);
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

void ServerState::load_all(bool enable_thread_pool) {
  this->collect_network_addresses();
  this->load_config_early();
  if (enable_thread_pool) {
    if (this->num_worker_threads > 0) {
      config_log.info_f("Starting thread pool with {} threads", this->num_worker_threads);
      this->thread_pool = make_unique<asio::thread_pool>(this->num_worker_threads);
    } else {
      config_log.warning_f("WorkerThreads is zero or not set; using default thread count");
    }
  }
  this->load_bb_private_keys();
  this->load_bb_system_defaults();
  this->load_accounts();
  this->clear_file_caches();
  this->load_patch_indexes();
  this->load_ep3_cards();
  this->load_ep3_maps();
  this->load_ep3_tournament_state();
  this->compile_functions();
  this->load_dol_files();
  this->create_default_lobbies();
  this->load_set_data_tables();
  this->load_maps();
  this->load_battle_params();
  this->load_level_tables();
  this->load_text_index();
  this->load_word_select_table();
  this->load_item_definitions();
  this->load_item_name_indexes();
  this->load_drop_tables();
  this->load_config_late();
  this->load_teams();
  this->load_quest_index();
}

void ServerState::disconnect_all_banned_clients() {
  uint64_t now_usecs = phosg::now();

  if (this->game_server) {
    for (const auto& c : this->game_server->all_clients()) {
      uint32_t addr = 0;
      auto ipss_channel = dynamic_pointer_cast<IPSSChannel>(c->channel);
      if (ipss_channel) {
        auto ipss_c = ipss_channel->ipss_client.lock();
        if (ipss_c) {
          addr = ipss_c->ipv4_addr;
        }
      } else {
        auto socket_channel = dynamic_pointer_cast<SocketChannel>(c->channel);
        if (socket_channel) {
          addr = ipv4_addr_for_asio_addr(socket_channel->local_addr.address());
        }
      }
      if ((c->login && (c->login->account->ban_end_time > now_usecs)) ||
          this->banned_ipv4_ranges->check(addr)) {
        c->channel->disconnect();
      }
    }
  }
}
