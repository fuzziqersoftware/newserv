#include "DataIndex.hh"

#include <string.h>

#include <filesystem>
#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Image.hh>
#include <phosg/Network.hh>
#include <phosg/Platform.hh>

#include "Compression.hh"
#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "ImageEncoder.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "Text.hh"
#include "TextIndex.hh"

#ifdef PHOSG_WINDOWS
static constexpr bool IS_WINDOWS = true;
#else
static constexpr bool IS_WINDOWS = false;
#endif

DataIndex::CheatFlags::CheatFlags(const phosg::JSON& json) : CheatFlags() {
  std::unordered_set<std::string> enabled_keys;
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

DataIndex::DataIndex::QuestF960Result::QuestF960Result(
    const phosg::JSON& json, std::shared_ptr<const ItemNameIndex> name_index, const ItemData::StackLimits& limits) {
  static const std::array<std::string, 7> day_names = {
      "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  this->meseta_cost = json.get_int("MesetaCost", 0);
  this->base_probability = json.get_int("BaseProbability", 0);
  this->probability_upgrade = json.get_int("ProbabilityUpgrade", 0);
  for (size_t day = 0; day < 7; day++) {
    for (const auto& item_it : json.get_list(day_names[day])) {
      if (item_it->is_int()) {
        this->results[day].emplace_back(ItemData::from_primary_identifier(limits, item_it->as_int()));
      } else {
        try {
          this->results[day].emplace_back(name_index->parse_item_description(item_it->as_string()));
        } catch (const std::exception& e) {
          config_log.warning_f(
              "Cannot parse item description \"{}\": {} (skipping entry)", item_it->as_string(), e.what());
        }
      }
    }
  }
}

DataIndex::DataIndex(const std::string& config_filename)
    // creation_time is reported as the server StartTime by the /y/summary
    // endpoint (HTTPServer.cc). Upstream declares it but never initializes
    // it, so it was 0 — making StartTime read as the Unix epoch and uptime
    // as ~now(). Stamp it at construction (= server boot / data reload).
    : creation_time(phosg::now()),
      config_filename(config_filename) {}

uint32_t DataIndex::connect_address_for_client(std::shared_ptr<Client> c) const {
  {
    auto ipss_channel = dynamic_pointer_cast<IPSSChannel>(c->channel);
    if (ipss_channel) {
      auto ipss_c = ipss_channel->ipss_client.lock();
      if (!ipss_c) {
        throw std::runtime_error("IPSS client is expired");
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
      // This is used during replays; the "client" will ignore this and reconnect via another PeerChannel
      return 0xEEEEEEEE;
    }
  }

  throw std::runtime_error("no connect address available");
}

uint16_t DataIndex::game_server_port_for_version(Version v) const {
  switch (v) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->name_to_port_config.at("gc-us3").port;
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->name_to_port_config.at("pc").port;
    case Version::XB_V3:
      return this->name_to_port_config.at("xb").port;
    case Version::BB_V4:
      return this->name_to_port_config.at("bb-data1").port;
    default:
      throw std::runtime_error("unknown version");
  }
}

std::shared_ptr<const Menu> DataIndex::information_menu(Version version) const {
  if (is_v1_or_v2(version)) {
    return this->information_menu_v2;
  } else if (is_v3(version)) {
    return this->information_menu_v3;
  }
  throw std::out_of_range("no information menu exists for this version");
}

std::shared_ptr<const Menu> DataIndex::proxy_destinations_menu(Version version) const {
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
      throw std::out_of_range("no proxy destinations menu exists for this version");
  }
}

const std::vector<std::pair<std::string, uint16_t>>& DataIndex::proxy_destinations(Version version) const {
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
      throw std::out_of_range("no proxy destinations menu exists for this version");
  }
}

const std::vector<uint32_t>& DataIndex::public_lobby_search_order(Version version, bool is_client_customization) const {
  static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the public lobby search orders in config.json");
  if (is_client_customization && !this->client_customization_public_lobby_search_order.empty()) {
    return this->client_customization_public_lobby_search_order;
  }
  return this->public_lobby_search_orders.at(static_cast<size_t>(version));
}

std::shared_ptr<const std::vector<std::string>> DataIndex::information_contents_for_client(std::shared_ptr<const Client> c) const {
  return is_v1_or_v2(c->version()) ? this->information_contents_v2 : this->information_contents_v3;
}

size_t DataIndex::default_min_level_for_game(Version version, Episode episode, Difficulty difficulty) const {
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
      throw std::runtime_error("invalid episode");
  }
}

std::shared_ptr<const SetDataTableBase> DataIndex::set_data_table(
    Version version, Episode episode, GameMode mode, Difficulty difficulty) const {
  bool use_ult_tables = ((episode == Episode::EP1) && (difficulty == Difficulty::ULTIMATE) && !is_v1(version) && (version != Version::PC_NTE));
  if (mode == GameMode::SOLO && is_v4(version)) {
    return use_ult_tables ? this->bb_solo_set_data_table_ep1_ult : this->bb_solo_set_data_table;
  }

  const auto& tables = use_ult_tables ? this->set_data_tables_ep1_ult : this->set_data_tables;
  auto ret = tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw std::runtime_error("no set data table exists for this version");
  }
  return ret;
}

std::shared_ptr<const LevelTable> DataIndex::level_table(Version version) const {
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
      throw std::logic_error("level table not available for version");
  }
}

std::shared_ptr<const ItemParameterTable> DataIndex::item_parameter_table(Version version) const {
  auto ret = this->item_parameter_tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw std::runtime_error("no item parameter table exists for this version");
  }
  return ret;
}

std::shared_ptr<const ItemParameterTable> DataIndex::item_parameter_table_for_encode(Version version) const {
  return this->item_parameter_table(is_v1(version) ? Version::PC_V2 : version);
}

std::shared_ptr<const MagMetadataTable> DataIndex::mag_metadata_table(Version version) const {
  if (version == Version::DC_NTE) {
    return this->mag_metadata_table_dc_nte;
  } else if (version == Version::DC_11_2000) {
    return this->mag_metadata_table_dc_11_2000;
  } else if (is_v1(version)) {
    return this->mag_metadata_table_v1;
  } else if (is_v2(version)) {
    return this->mag_metadata_table_v2;
  } else if (!is_v4(version)) {
    return this->mag_metadata_table_v3;
  } else {
    return this->mag_metadata_table_v4;
  }
}

std::shared_ptr<const ItemData::StackLimits> DataIndex::item_stack_limits(Version version) const {
  auto ret = this->item_stack_limits_tables.at(static_cast<size_t>(version));
  if (ret == nullptr) {
    throw std::runtime_error("no item stack limits table exists for this version");
  }
  return ret;
}

std::shared_ptr<const ItemNameIndex> DataIndex::item_name_index_opt(Version version) const {
  return this->item_name_indexes.at(static_cast<size_t>(version));
}

std::shared_ptr<const ItemNameIndex> DataIndex::item_name_index(Version version) const {
  auto ret = this->item_name_index_opt(version);
  if (ret == nullptr) {
    throw std::runtime_error("no item name index exists for this version");
  }
  return ret;
}

std::string DataIndex::describe_item(Version version, const ItemData& item, uint8_t flags) const {
  if (is_v1(version)) {
    ItemData encoded = item;
    encoded.encode_for_version(version, this->item_parameter_table(version));
    return this->item_name_index(version)->describe_item(encoded, flags);
  } else {
    return this->item_name_index(version)->describe_item(item, flags);
  }
}

ItemData DataIndex::parse_item_description(Version version, const std::string& description) const {
  return this->item_name_index(version)->parse_item_description(description);
}

std::shared_ptr<const CommonItemSet> DataIndex::common_item_set(Version logic_version, std::shared_ptr<const Quest> q) const {
  if (q && !q->meta.common_item_set_name.empty()) {
    try {
      return this->common_item_sets.at(q->meta.common_item_set_name);
    } catch (const std::out_of_range&) {
      throw std::runtime_error(std::format("common item set {} for quest {} does not exist",
          q->meta.common_item_set_name, q->meta.name));
    }
  } else if (is_v1_or_v2(logic_version) && (logic_version != Version::GC_NTE)) {
    // TODO: We should probably have a v1 common item set at some point too
    return this->common_item_sets.at("common-table-v1-v2");
  } else if ((logic_version == Version::GC_NTE) || is_v3(logic_version) || is_v4(logic_version)) {
    return this->common_item_sets.at("common-table-v3-v4");
  } else {
    throw std::runtime_error(std::format(
        "no default common item set is available for {}", phosg::name_for_enum(logic_version)));
  }
}

std::shared_ptr<const RareItemSet> DataIndex::rare_item_set(Version logic_version, std::shared_ptr<const Quest> q) const {
  if (q && !q->meta.rare_item_set_name.empty()) {
    try {
      return this->rare_item_sets.at(q->meta.rare_item_set_name);
    } catch (const std::out_of_range&) {
      throw std::runtime_error(std::format("rare item set {} for quest {} does not exist",
          q->meta.rare_item_set_name, q->meta.name));
    }
  } else if (is_v1(logic_version)) {
    return this->rare_item_sets.at("rare-table-v1");
  } else if (is_v2(logic_version) && (logic_version != Version::GC_NTE)) {
    return this->rare_item_sets.at("rare-table-v2");
  } else if (is_v3(logic_version) || (logic_version == Version::GC_NTE)) {
    return this->rare_item_sets.at("rare-table-v3");
  } else if (is_v4(logic_version)) {
    return this->rare_item_sets.at("rare-table-v4");
  } else {
    throw std::runtime_error(std::format("no default rare item set is available for {}", phosg::name_for_enum(logic_version)));
  }
}

void DataIndex::set_port_configuration(const std::vector<PortConfiguration>& port_configs) {
  this->name_to_port_config.clear();
  this->number_to_port_config.clear();

  bool any_port_is_pc_console_detect = false;
  for (const auto& pc : port_configs) {
    if (!this->name_to_port_config.emplace(pc.name, pc).second) {
      // Note: This is a logic_error instead of a runtime_error because port_configs comes from a JSON map, so the
      // names should already all be unique. In contrast, the user can define port configurations with the same number
      // while still writing valid JSON, so only one of these cases can reasonably occur as a result of user behavior.
      throw std::logic_error("duplicate name in port configuration");
    }
    if (!this->number_to_port_config.emplace(pc.port, pc).second) {
      throw std::runtime_error("duplicate number in port configuration");
    }
    if (pc.behavior == ServerBehavior::PC_CONSOLE_DETECT) {
      any_port_is_pc_console_detect = true;
    }
  }

  if (any_port_is_pc_console_detect) {
    if (!this->name_to_port_config.count("pc")) {
      throw std::runtime_error("pc port is not defined, but some ports use the pc_console_detect behavior");
    }
    if (!this->name_to_port_config.count("gc-us3")) {
      throw std::runtime_error("gc-us3 port is not defined, but some ports use the pc_console_detect behavior");
    }
  }
}

std::shared_ptr<const std::string> DataIndex::load_bb_file(const std::string& filename) const {

  if (this->bb_patch_file_index) {
    // First, look in the patch tree's data directory
    std::string patch_index_path = "./data/" + filename;
    try {
      return this->bb_patch_file_index->get(patch_index_path)->load_data();
    } catch (const std::out_of_range&) {
    }
  }

  if (this->bb_data_gsl) {
    // Second, look in the patch tree's data.gsl file
    try {
      // TODO: It's kinda not great that we copy the data here; find a way to avoid doing this (also in the below case)
      return std::make_shared<std::string>(this->bb_data_gsl->get_copy(filename));
    } catch (const std::out_of_range&) {
    }

    // Third, look in data.gsl without the filename extension
    size_t dot_offset = filename.rfind('.');
    if (dot_offset != std::string::npos) {
      std::string no_ext_gsl_filename = filename.substr(0, dot_offset);
      try {
        return std::make_shared<std::string>(this->bb_data_gsl->get_copy(no_ext_gsl_filename));
      } catch (const std::out_of_range&) {
      }
    }
  }

  // Finally, look in system/blueburst
  return std::make_shared<std::string>(phosg::load_file("system/blueburst/" + filename));
}

std::shared_ptr<const std::string> DataIndex::load_map_file(Version version, const std::string& filename) const {
  if (version == Version::BB_V4) {
    try {
      return this->load_bb_file(filename);
    } catch (const std::exception& e) {
    }
  } else if (version == Version::PC_V2) {
    try {
      return std::make_shared<std::string>(phosg::load_file("system/patch-pc/Media/PSO/" + filename));
    } catch (const std::exception& e) {
    }
  }
  try {
    std::string path = std::format("system/maps/{}/{}", file_path_token_for_version(version), filename);
    return std::make_shared<std::string>(phosg::load_file(path));
  } catch (const std::exception& e) {
  }
  return nullptr;
}

std::pair<std::string, uint16_t> DataIndex::parse_port_spec(const phosg::JSON& json) const {
  if (json.is_list()) {
    std::string addr = json.at(0).as_string();
    try {
      addr = string_for_address(this->all_addresses.at(addr));
    } catch (const std::out_of_range&) {
    }
    return std::make_pair(addr, json.at(1).as_int());
  } else {
    return std::make_pair("", json.as_int());
  }
}

std::vector<DataIndex::PortConfiguration> DataIndex::parse_port_configuration(const phosg::JSON& json) const {
  std::vector<PortConfiguration> ret;
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

void DataIndex::collect_network_addresses() {
  config_log.info_f("Reading network addresses");
  this->all_addresses = get_local_addresses();
  for (const auto& it : this->all_addresses) {
    config_log.info_f("Found interface: {} = {}", it.first, string_for_address(it.second));
  }
}

void DataIndex::load_config_early() {
  if (this->config_filename.empty()) {
    throw std::logic_error("configuration filename is missing");
  }

  config_log.info_f("Loading configuration");
  this->config_json = std::make_shared<phosg::JSON>(phosg::JSON::parse(phosg::load_file(this->config_filename)));

  auto parse_behavior_switch = [&](const std::string& json_key, BehaviorSwitch default_value) -> DataIndex::BehaviorSwitch {
    try {
      std::string behavior = this->config_json->get_string(json_key);
      if (behavior == "Off") {
        return DataIndex::BehaviorSwitch::OFF;
      } else if (behavior == "OffByDefault") {
        return DataIndex::BehaviorSwitch::OFF_BY_DEFAULT;
      } else if (behavior == "OnByDefault") {
        return DataIndex::BehaviorSwitch::ON_BY_DEFAULT;
      } else if (behavior == "On") {
        return DataIndex::BehaviorSwitch::ON;
      } else {
        throw std::runtime_error("invalid value for " + json_key);
      }
    } catch (const std::out_of_range&) {
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
          throw std::runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
        }
        this->username = user_from_env;
      }
    } catch (const std::out_of_range&) {
    }

    this->set_port_configuration(parse_port_configuration(this->config_json->at("PortConfiguration")));
    try {
      auto spec = this->parse_port_spec(this->config_json->at("DNSServerPort"));
      this->dns_server_addr = std::move(spec.first);
      this->dns_server_port = spec.second;
    } catch (const std::out_of_range&) {
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
    } catch (const std::out_of_range&) {
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
    } catch (const std::out_of_range&) {
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
    } catch (const std::out_of_range&) {
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
    } catch (const std::out_of_range&) {
    }

    this->one_time_config_loaded = true;
  }

  try {
    auto local_address_str = this->config_json->at("LocalAddress").as_string();
    try {
      this->local_address = this->all_addresses.at(local_address_str);
      config_log.info_f("Added local address: {} ({})", string_for_address(this->local_address), local_address_str);
    } catch (const std::out_of_range&) {
      this->local_address = address_for_string(local_address_str.c_str());
      config_log.info_f("Added local address: {}", local_address_str);
    }
    this->all_addresses.erase("<local>");
    this->all_addresses.emplace("<local>", this->local_address);
  } catch (const std::out_of_range&) {
    for (const auto& it : this->all_addresses) {
      // Choose any local interface except the loopback interface
      if (!is_loopback_address(it.second) && is_local_address(it.second)) {
        this->local_address = it.second;
      }
    }
    if (this->local_address) {
      config_log.warning_f("Local address not specified; using {} as default", string_for_address(this->local_address));
    } else {
      config_log.warning_f("Local address not specified and no default is available");
    }
  }

  try {
    auto external_address_str = this->config_json->at("ExternalAddress").as_string();
    try {
      this->external_address = this->all_addresses.at(external_address_str);
      config_log.info_f("Added external address: {} ({})",
          string_for_address(this->external_address), external_address_str);
    } catch (const std::out_of_range&) {
      this->external_address = address_for_string(external_address_str.c_str());
      config_log.info_f("Added external address: {}", external_address_str);
    }
    this->all_addresses.erase("<external>");
    this->all_addresses.emplace("<external>", this->external_address);
  } catch (const std::out_of_range&) {
    for (const auto& it : this->all_addresses) {
      // Choose any non-local address, if any exist
      if (!is_local_address(it.second)) {
        this->external_address = it.second;
        break;
      }
    }
    if (this->external_address) {
      config_log.warning_f("External address not specified; using {} as default",
          string_for_address(this->external_address));
    } else {
      config_log.warning_f(
          "External address not specified and no default is available; only local clients will be able to connect");
    }
  }

  try {
    this->banned_ipv4_ranges = std::make_shared<IPV4RangeSet>(this->config_json->at("BannedIPV4Ranges"));
  } catch (const std::out_of_range&) {
    this->banned_ipv4_ranges = std::make_shared<IPV4RangeSet>();
  }

  this->client_ping_interval_usecs = this->config_json->get_int("ClientPingInterval", 30000000);
  this->client_idle_timeout_usecs = this->config_json->get_int("ClientIdleTimeout", 60000000);
  this->patch_client_idle_timeout_usecs = this->config_json->get_int("PatchClientIdleTimeout", 300000000);

  this->ip_stack_debug = this->config_json->get_bool("IPStackDebug", false);
  this->allow_unregistered_users = this->config_json->get_bool("AllowUnregisteredUsers", false);
  this->allow_pc_nte = this->config_json->get_bool("AllowPCNTE", false);
  this->allow_same_account_concurrent_logins = this->config_json->get_bool("AllowSameAccountConcurrentLogins", false);
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
    throw std::runtime_error("default V4 drop mode cannot be CLIENT");
  }
  if ((this->allowed_drop_modes_v4_normal & (1 << static_cast<size_t>(ServerDropMode::CLIENT))) ||
      (this->allowed_drop_modes_v4_battle & (1 << static_cast<size_t>(ServerDropMode::CLIENT))) || (this->allowed_drop_modes_v4_challenge & (1 << static_cast<size_t>(ServerDropMode::CLIENT)))) {
    throw std::runtime_error("CLIENT drop mode cannot be allowed in V4");
  }

  auto parse_quest_flag_rewrites = [&json = this->config_json](const char* key) -> std::unordered_map<uint16_t, IntegralExpression> {
    std::unordered_map<uint16_t, IntegralExpression> ret;
    try {
      for (const auto& it : json->get_dict(key)) {
        if (!it.first.starts_with("F_")) {
          throw std::runtime_error("invalid flag reference: " + it.first);
        }
        uint16_t flag = stoul(it.first.substr(2), nullptr, 16);
        if (it.second->is_bool()) {
          ret.emplace(flag, it.second->as_bool() ? "true" : "false");
        } else {
          ret.emplace(flag, it.second->as_string());
        }
      }
    } catch (const std::out_of_range&) {
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
      this->quest_counter_fields.emplace(it.first, std::make_pair(def.at(0)->as_int(), def.at(1)->as_int()));
    }
  } catch (const std::out_of_range&) {
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
        throw std::runtime_error(std::format(
            "specific_version {} in EnableSendFunctionCallQuestNumbers is not a 4-byte string",
            it.first));
      }
      uint32_t specific_version = phosg::StringReader(it.first).get_u32b();
      int64_t quest_num = it.second->as_int();
      this->enable_send_function_call_quest_numbers.emplace(specific_version, quest_num);
    }
  } catch (const std::out_of_range&) {
  }
  this->enable_v3_v4_protected_subcommands = this->config_json->get_bool("EnableV3V4ProtectedSubcommands", false);

  auto parse_int_list = +[](const phosg::JSON& json) -> std::vector<uint32_t> {
    std::vector<uint32_t> ret;
    for (const auto& item : json.as_list()) {
      ret.emplace_back(item->as_int());
    }
    return ret;
  };

  this->ep3_infinite_meseta = this->config_json->get_bool("Episode3InfiniteMeseta", false);
  try {
    this->ep3_defeat_player_meseta_rewards = parse_int_list(this->config_json->at("Episode3DefeatPlayerMeseta"));
  } catch (const std::out_of_range&) {
    this->ep3_defeat_player_meseta_rewards = {300, 400, 500, 600, 700};
  }
  try {
    this->ep3_defeat_com_meseta_rewards = parse_int_list(this->config_json->get("Episode3DefeatCOMMeseta", phosg::JSON::list()));
  } catch (const std::out_of_range&) {
    this->ep3_defeat_com_meseta_rewards = {100, 200, 300, 400, 500};
  }
  this->ep3_final_round_meseta_bonus = this->config_json->get_int("Episode3FinalRoundMesetaBonus", 300);
  this->ep3_jukebox_is_free = this->config_json->get_bool("Episode3JukeboxIsFree", false);
  this->ep3_behavior_flags = this->config_json->get_int("Episode3BehaviorFlags", 0);
  this->ep3_card_auction_points = this->config_json->get_int("CardAuctionPoints", 0);
  this->hide_download_commands = this->config_json->get_bool("HideDownloadCommands", true);
  this->censor_credentials = this->config_json->get_bool("CensorCredentials", true);
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
  } catch (const std::out_of_range&) {
    this->ep3_card_auction_min_size = 0;
    this->ep3_card_auction_max_size = 0;
  }

  this->ep3_lobby_banners.clear();
  size_t banner_index = 0;
  for (const auto& it : this->config_json->get("Episode3LobbyBanners", phosg::JSON::list()).as_list()) {
    std::string path = "system/ep3/banners/" + it->at(2).as_string();

    std::string compressed_gvm_data;
    std::string decompressed_gvm_data;
    std::string lower_path = phosg::tolower(path);
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
      throw std::runtime_error(std::format("banner {} is in an unknown format", path));
    }

    size_t decompressed_size = decompressed_gvm_data.empty()
        ? prs_decompress_size(compressed_gvm_data)
        : decompressed_gvm_data.size();
    if (decompressed_size > 0x37000) {
      throw std::runtime_error(std::format(
          "banner {} is too large (0x{:X} bytes; maximum size is 0x37000 bytes)", path, decompressed_size));
    }

    if (compressed_gvm_data.empty()) {
      compressed_gvm_data = prs_compress_optimal(decompressed_gvm_data);
    }
    if (compressed_gvm_data.size() > 0x3800) {
      throw std::runtime_error(std::format(
          "banner {} cannot be compressed small enough (0x{:X} bytes; maximum size is 0x3800 bytes compressed)",
          it->at(2).as_string(), compressed_gvm_data.size()));
    }
    config_log.info_f(
        "Loaded Episode 3 lobby banner {} (0x{:X} -> 0x{:X} bytes)",
        path, decompressed_size, compressed_gvm_data.size());
    this->ep3_lobby_banners.emplace_back(
        Ep3LobbyBannerEntry{.type = static_cast<uint32_t>(it->at(0).as_int()),
            .which = static_cast<uint32_t>(it->at(1).as_int()),
            .data = std::move(compressed_gvm_data)});
  }

  {
    auto parse_ep3_ex_result_cmd = [&](const phosg::JSON& src) -> std::shared_ptr<G_SetEXResultValues_Ep3_6xB4x4B> {
      auto ret = std::make_shared<G_SetEXResultValues_Ep3_6xB4x4B>();
      const auto& win_json = src.at("Win");
      for (size_t z = 0; z < std::min<size_t>(win_json.size(), 10); z++) {
        ret->win_entries[z].threshold = win_json.at(z).at(0).as_int();
        ret->win_entries[z].value = win_json.at(z).at(1).as_int();
      }
      const auto& lose_json = src.at("Lose");
      for (size_t z = 0; z < std::min<size_t>(lose_json.size(), 10); z++) {
        ret->lose_entries[z].threshold = lose_json.at(z).at(0).as_int();
        ret->lose_entries[z].value = lose_json.at(z).at(1).as_int();
      }
      return ret;
    };
    const auto& categories_json = this->config_json->at("Episode3EXResultValues");
    this->ep3_default_ex_values = parse_ep3_ex_result_cmd(categories_json.at("Default"));
    try {
      this->ep3_tournament_ex_values = parse_ep3_ex_result_cmd(categories_json.at("Tournament"));
    } catch (const std::out_of_range&) {
      this->ep3_tournament_ex_values = this->ep3_default_ex_values;
    }
    try {
      this->ep3_tournament_ex_values = parse_ep3_ex_result_cmd(categories_json.at("TournamentFinalMatch"));
    } catch (const std::out_of_range&) {
      this->ep3_tournament_final_round_ex_values = this->ep3_tournament_ex_values;
    }
  }

  try {
    const auto& stack_limits_tables_json = this->config_json->at("ItemStackLimits");
    for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
      try {
        Version v = static_cast<Version>(v_s);
        this->item_stack_limits_tables[v_s] = std::make_shared<ItemData::StackLimits>(
            v, stack_limits_tables_json.at(v_s - NUM_PATCH_VERSIONS));
      } catch (const std::out_of_range&) {
      }
    }
  } catch (const std::out_of_range&) {
  }

  this->bb_max_bank_items = this->config_json->get_int("BBMaxBankItems", 200);
  this->bb_max_bank_meseta = this->config_json->get_int("BBMaxBankMeseta", 999999);

  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    if (!this->item_stack_limits_tables[v_s]) {
      Version v = static_cast<Version>(v_s);
      if ((v == Version::DC_NTE) || (v == Version::DC_11_2000)) {
        this->item_stack_limits_tables[v_s] = std::make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_DC_NTE, 999999);
      } else if (v_s < static_cast<size_t>(Version::GC_NTE)) {
        this->item_stack_limits_tables[v_s] = std::make_shared<ItemData::StackLimits>(
            v, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V1_V2, 999999);
      } else {
        this->item_stack_limits_tables[v_s] = std::make_shared<ItemData::StackLimits>(
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
        ? DataIndex::RunShellBehavior::ALWAYS
        : DataIndex::RunShellBehavior::NEVER;
  } catch (const std::out_of_range&) {
  }

  try {
    const auto& groups = this->config_json->get_list("CompatibilityGroups");
    this->compatibility_groups.fill(0);
    for (size_t v_s = 0; v_s < groups.size(); v_s++) {
      this->compatibility_groups[v_s] = groups[v_s]->as_int();
    }
  } catch (const std::out_of_range&) {
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
  try {
    const auto& s = this->config_json->get_string("ChatCommandSentinel");
    if (s.size() != 1) {
      throw std::runtime_error("ChatCommandSentinel must be a string of length 1");
    }
    this->chat_command_sentinel = s[0];
  } catch (const std::out_of_range&) {
  }
  this->num_backup_character_slots = this->config_json->get_int("BackupCharacterSlots", 16);

  this->version_name_colors.reset();
  this->client_customization_name_color = 0;
  try {
    const auto& colors_json = this->config_json->get_list("VersionNameColors");
    if (colors_json.size() != NUM_NON_PATCH_VERSIONS) {
      throw std::runtime_error("VersionNameColors list length is incorrect");
    }
    auto new_colors = std::make_unique<std::array<uint32_t, NUM_NON_PATCH_VERSIONS>>();
    for (size_t z = 0; z < NUM_NON_PATCH_VERSIONS; z++) {
      new_colors->at(z) = colors_json.at(z)->as_int();
    }
    this->version_name_colors = std::move(new_colors);
  } catch (const std::out_of_range&) {
  }
  try {
    this->client_customization_name_color = this->config_json->get_int("ClientCustomizationNameColor");
  } catch (const std::out_of_range&) {
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
  } catch (const std::out_of_range&) {
  }
  try {
    const auto& order_json = this->config_json->get_list("ClientCustomizationLobbySearchOrder");
    auto& order = this->client_customization_public_lobby_search_order;
    for (const auto& it : order_json) {
      order.emplace_back(it->as_int());
    }
  } catch (const std::out_of_range&) {
  }

  this->pre_lobby_event = 0;
  try {
    auto v = this->config_json->at("MenuEvent");
    this->pre_lobby_event = v.is_int() ? v.as_int() : event_for_name(v.as_string());
  } catch (const std::out_of_range&) {
  }
  const auto& events_json = this->config_json->get_list("LobbyEvents");
  this->per_lobby_events.clear();
  try {
    for (size_t z = 0; z < events_json.size(); z++) {
      const auto& v = events_json.at(z);
      per_lobby_events.emplace_back(v->is_int() ? v->as_int() : event_for_name(v->as_string()));
    }
  } catch (const std::out_of_range&) {
  }

  this->ep3_menu_song = this->config_json->get_int("Episode3MenuSong", -1);

  try {
    this->quest_category_index = std::make_shared<QuestCategoryIndex>(this->config_json->at("QuestCategories"));
  } catch (const std::exception& e) {
    throw std::runtime_error(std::format(
        "QuestCategories is missing or invalid in config ({}); see config.example.json for an example", e.what()));
  }

  config_log.info_f("Creating menus");

  auto information_menu_v2 = std::make_shared<Menu>(MenuID::INFORMATION, "Information");
  auto information_menu_v3 = std::make_shared<Menu>(MenuID::INFORMATION, "Information");
  std::shared_ptr<std::vector<std::string>> information_contents_v2 = std::make_shared<std::vector<std::string>>();
  std::shared_ptr<std::vector<std::string>> information_contents_v3 = std::make_shared<std::vector<std::string>>();

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
      std::string name = item->get_string(0);
      std::string short_desc = item->get_string(1);
      information_menu_v2->items.emplace_back(item_id, name, short_desc, 0);
      information_contents_v2->emplace_back(item->get_string(2));
      item_id++;
    }

    item_id = 0;
    for (const auto& item : v3_json.as_list()) {
      std::string name = item->get_string(0);
      std::string short_desc = item->get_string(1);
      information_menu_v3->items.emplace_back(item_id, name, short_desc, MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
      information_contents_v3->emplace_back(item->get_string(2));
      item_id++;
    }
  }
  this->information_menu_v2 = information_menu_v2;
  this->information_menu_v3 = information_menu_v3;
  this->information_contents_v2 = information_contents_v2;
  this->information_contents_v3 = information_contents_v3;

  auto generate_proxy_destinations_menu = [&](std::vector<std::pair<std::string, uint16_t>>& ret_pds, const char* key) -> std::shared_ptr<const Menu> {
    auto ret = std::make_shared<Menu>(MenuID::PROXY_DESTINATIONS, "Proxy server");
    ret_pds.clear();

    try {
      std::map<std::string, const phosg::JSON&> sorted_jsons;
      for (const auto& it : this->config_json->at(key).as_dict()) {
        sorted_jsons.emplace(it.first, *it.second);
      }

      ret->items.emplace_back(ProxyDestinationsMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);
      ret->items.emplace_back(ProxyDestinationsMenuItemID::OPTIONS, "Options", "Set proxy session\noptions", 0);

      uint32_t item_id = 0;
      for (const auto& item : sorted_jsons) {
        const std::string& netloc_str = item.second.as_string();
        const std::string& description = "$C7Remote server:\n$C6" + netloc_str;
        ret->items.emplace_back(item_id, item.first, description, 0);
        ret_pds.emplace_back(phosg::parse_netloc(netloc_str));
        item_id++;
      }
    } catch (const std::out_of_range&) {
    }
    return ret;
  };

  this->proxy_destinations_menu_dc = generate_proxy_destinations_menu(this->proxy_destinations_dc, "ProxyDestinations-DC");
  this->proxy_destinations_menu_pc = generate_proxy_destinations_menu(this->proxy_destinations_pc, "ProxyDestinations-PC");
  this->proxy_destinations_menu_gc = generate_proxy_destinations_menu(this->proxy_destinations_gc, "ProxyDestinations-GC");
  this->proxy_destinations_menu_xb = generate_proxy_destinations_menu(this->proxy_destinations_xb, "ProxyDestinations-XB");

  try {
    const std::string& netloc_str = this->config_json->get_string("ProxyDestination-Patch");
    this->proxy_destination_patch = phosg::parse_netloc(netloc_str);
    config_log.info_f("Patch server proxy is enabled with destination {}", netloc_str);
  } catch (const std::out_of_range&) {
    this->proxy_destination_patch.reset();
  }
  try {
    const std::string& netloc_str = this->config_json->get_string("ProxyDestination-BB");
    this->proxy_destination_bb = phosg::parse_netloc(netloc_str);
    config_log.info_f("BB proxy is enabled with destination {}", netloc_str);
  } catch (const std::out_of_range&) {
    this->proxy_destination_bb.reset();
  }

  this->welcome_message = this->config_json->get_string("WelcomeMessage", "");
  this->pc_patch_server_message = this->config_json->get_string("PCPatchServerMessage", "");
  this->bb_patch_server_message = this->config_json->get_string("BBPatchServerMessage", "");

  this->team_reward_defs_json = nullptr;
  try {
    this->team_reward_defs_json = std::move(this->config_json->at("TeamRewards"));
  } catch (const std::out_of_range&) {
  }

  std::shared_ptr<const MapState::RareEnemyRates> prev = MapState::DEFAULT_RARE_ENEMIES;
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    size_t diff_index = static_cast<size_t>(difficulty);
    try {
      std::string key = "RareEnemyRates-";
      key += token_name_for_difficulty(difficulty);
      this->rare_enemy_rates_by_difficulty[diff_index] = std::make_shared<MapState::RareEnemyRates>(
          this->config_json->at(key));
      prev = this->rare_enemy_rates_by_difficulty[diff_index];
    } catch (const std::out_of_range&) {
      this->rare_enemy_rates_by_difficulty[diff_index] = prev;
    }
  }
  try {
    this->rare_enemy_rates_challenge = std::make_shared<MapState::RareEnemyRates>(this->config_json->at("RareEnemyRates-Challenge"));
  } catch (const std::out_of_range&) {
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
        std::array<size_t, 4> levels({0, 0, 0, 0});
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
            throw std::runtime_error("unknown episode");
        }
      }
    } catch (const std::out_of_range&) {
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
  } catch (const std::out_of_range&) {
  }
  this->auto_patches.clear();
  try {
    for (const auto& it : this->config_json->get_list("AutoPatches")) {
      this->auto_patches.emplace(it->as_string());
    }
  } catch (const std::out_of_range&) {
  }

  try {
    this->cheat_flags = CheatFlags(this->config_json->at("CheatingBehaviors"));
  } catch (const std::out_of_range&) {
    this->cheat_flags = CheatFlags();
  }
}

void DataIndex::load_config_late() {
  this->ep3_card_auction_pool.clear();
  try {
    for (const auto& it : this->config_json->get_dict("CardAuctionPool")) {
      uint16_t card_id;
      try {
        card_id = this->ep3_card_index->definition_for_name_normalized(it.first)->def.card_id;
      } catch (const std::out_of_range&) {
        throw std::runtime_error(std::format("Ep3 card \"{}\" in auction pool does not exist", it.first));
      }
      this->ep3_card_auction_pool.emplace_back(
          CardAuctionPoolEntry{
              .probability = static_cast<uint64_t>(it.second->at(0).as_int()),
              .card_id = card_id,
              .min_price = static_cast<uint16_t>(it.second->at(1).as_int())});
    }
  } catch (const std::out_of_range&) {
  }

  for (auto& trap_card_ids : this->ep3_trap_card_ids) {
    trap_card_ids.clear();
  }
  if (this->ep3_card_index) {
    try {
      const auto& ep3_trap_cards_json = this->config_json->get_list("Episode3TrapCards");
      if (!ep3_trap_cards_json.empty()) {
        if (ep3_trap_cards_json.size() != 5) {
          throw std::runtime_error("Episode3TrapCards must be a list of 5 lists");
        }
        for (size_t trap_type = 0; trap_type < 5; trap_type++) {
          auto& trap_card_ids = this->ep3_trap_card_ids[trap_type];
          for (const auto& card_it : ep3_trap_cards_json.at(trap_type)->as_list()) {
            if (card_it->is_int()) {
              int64_t card_id = card_it->as_int();
              try {
                const auto& card = this->ep3_card_index->definition_for_id(card_id);
                if (card->def.type != Episode3::CardType::ASSIST) {
                  throw std::runtime_error(std::format(
                      "Ep3 card \"{}\" ({:04X}) in trap card list is not an assist card",
                      card->def.en_name.decode(), card->def.card_id));
                }
                trap_card_ids.emplace_back(card->def.card_id);
              } catch (const std::out_of_range&) {
                throw std::runtime_error(std::format("Ep3 card {:04X} in trap card list does not exist", card_id));
              }
            } else {
              const std::string& card_name = card_it->as_string();
              try {
                const auto& card = this->ep3_card_index->definition_for_name_normalized(card_name);
                if (card->def.type != Episode3::CardType::ASSIST) {
                  throw std::runtime_error(std::format(
                      "Ep3 card \"{}\" ({:04X}) in trap card list is not an assist card",
                      card->def.en_name.decode(), card->def.card_id));
                }
                trap_card_ids.emplace_back(card->def.card_id);
              } catch (const std::out_of_range&) {
                throw std::runtime_error(std::format("Ep3 card \"{}\" in trap card list does not exist", card_name));
              }
            }
          }
        }
      }
    } catch (const std::out_of_range&) {
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
            if (item_it->is_int()) {
              difficulty_res.emplace_back(ItemData::from_primary_identifier(
                  *this->item_stack_limits(Version::BB_V4), item_it->as_int()));
            } else {
              try {
                difficulty_res.emplace_back(this->parse_item_description(Version::BB_V4, item_it->as_string()));
              } catch (const std::exception& e) {
                config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", item_it->as_string(), e.what());
              }
            }
          }
        }
      }
    } catch (const std::out_of_range&) {
    }
    try {
      for (const auto& it : this->config_json->get_list("QuestF95FResultItems")) {
        auto& list = it->as_list();
        size_t price = list.at(0)->as_int();
        const auto& desc = list.at(1);
        if (desc->is_int()) {
          this->quest_F95F_results.emplace_back(std::make_pair(
              price, ItemData::from_primary_identifier(*this->item_stack_limits(Version::BB_V4), desc->as_int())));
        } else {
          try {
            this->quest_F95F_results.emplace_back(std::make_pair(
                price, this->parse_item_description(Version::BB_V4, list.at(1)->as_string())));
          } catch (const std::exception& e) {
            config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", list.at(1)->as_string(), e.what());
          }
        }
      }
    } catch (const std::out_of_range&) {
    }
    try {
      auto name_index = this->item_name_index(Version::BB_V4);
      auto stack_limits = this->item_stack_limits(Version::BB_V4);
      this->quest_F960_failure_results = QuestF960Result(
          this->config_json->at("QuestF960FailureResultItems"), name_index, *stack_limits);
      for (const auto& it : this->config_json->get_list("QuestF960SuccessResultItems")) {
        this->quest_F960_success_results.emplace_back(*it, name_index, *stack_limits);
      }
    } catch (const std::out_of_range&) {
    }

    auto parse_primary_identifier_list = [&](const char* key, Version v) -> std::unordered_set<uint32_t> {
      std::unordered_set<uint32_t> ret;
      try {
        for (const auto& pi_json : this->config_json->get_list(key)) {
          if (pi_json->is_int()) {
            ret.emplace(pi_json->as_int());
          } else {
            try {
              auto item = this->parse_item_description(v, pi_json->as_string());
              ret.emplace(item.primary_identifier());
            } catch (const std::exception& e) {
              config_log.warning_f("Cannot parse item description \"{}\": {} (skipping entry)", pi_json->as_string(), e.what());
            }
          }
        }
      } catch (const std::out_of_range&) {
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

void DataIndex::load_bb_private_keys() {
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> new_keys;
  for (const auto& item : std::filesystem::directory_iterator("system/blueburst/keys")) {
    std::string filename = item.path().filename().string();
    if (!filename.ends_with(".nsk")) {
      continue;
    }
    new_keys.emplace_back(std::make_shared<PSOBBEncryption::KeyFile>(
        phosg::load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.debug_f("Loaded Blue Burst key file: {}", filename);
  }
  this->bb_private_keys = std::move(new_keys);
}

void DataIndex::load_bb_system_defaults() {
  try {
    this->bb_default_keyboard_config = std::make_shared<parray<uint8_t, 0x16C>>(
        phosg::load_object_file<parray<uint8_t, 0x16C>>("system/blueburst/default-keyboard-config.bin"));
    config_log.info_f("Default Blue Burst keyboard config is present");
  } catch (const phosg::cannot_open_file&) {
  }
  try {
    this->bb_default_joystick_config = std::make_shared<parray<uint8_t, 0x38>>(
        phosg::load_object_file<parray<uint8_t, 0x38>>("system/blueburst/default-joystick-config.bin"));
    config_log.info_f("Default Blue Burst joystick config is present");
  } catch (const phosg::cannot_open_file&) {
  }
}

void DataIndex::load_patch_indexes() {
  std::shared_ptr<const GSLArchive> bb_data_gsl;
  std::shared_ptr<PatchFileIndex> pc_patch_file_index;
  std::shared_ptr<PatchFileIndex> bb_patch_file_index;

  if (std::filesystem::is_directory("system/patch-pc")) {
    config_log.info_f("Indexing PSO PC patch files");
    pc_patch_file_index = std::make_shared<PatchFileIndex>("system/patch-pc");
  } else {
    config_log.info_f("PSO PC patch files not present");
  }
  if (std::filesystem::is_directory("system/patch-bb")) {
    config_log.info_f("Indexing PSO BB patch files");
    bb_patch_file_index = std::make_shared<PatchFileIndex>("system/patch-bb");
    try {
      auto gsl_file = bb_patch_file_index->get("./data/data.gsl");
      bb_data_gsl = std::make_shared<GSLArchive>(gsl_file->load_data(), false);
      config_log.info_f("data.gsl found in BB patch files");
    } catch (const std::out_of_range&) {
      config_log.info_f("data.gsl is not present in BB patch files");
    }
  } else {
    config_log.info_f("PSO BB patch files not present");
  }

  this->bb_data_gsl = std::move(bb_data_gsl);
  this->pc_patch_file_index = std::move(pc_patch_file_index);
  this->bb_patch_file_index = std::move(bb_patch_file_index);
}

void DataIndex::load_maps() {
  using SDT = SetDataTable;

  config_log.info_f("Loading map layouts");
  auto new_room_layout_index = std::make_shared<RoomLayoutIndex>(
      phosg::JSON::parse(phosg::load_file("system/maps/room-layout-index.json")));

  config_log.info_f("Loading Episode 3 Morgue maps");
  std::unordered_map<uint64_t, std::shared_ptr<const MapFile>> new_map_file_for_source_hash;
  std::map<uint32_t, std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>> new_map_files_for_free_play_key;
  {
    // TODO: Ep3 NTE loads map_city00_on, but it appears there are variants. Figure this out and load those maps too.
    auto objects_data = this->load_map_file(Version::GC_EP3, "map_city_on_battle_o.dat");
    auto enemies_data = this->load_map_file(Version::GC_EP3, "map_city_on_battle_e.dat");
    if (objects_data || enemies_data) {
      uint32_t free_play_key = this->free_play_key(Episode::EP3, GameMode::NORMAL, Difficulty::NORMAL, 0, 0, 0);
      auto map_file = std::make_shared<MapFile>(0, objects_data, enemies_data, nullptr);
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
                  std::shared_ptr<const MapFile> map_file;
                  try {
                    map_file = new_map_file_for_source_hash.at(source_hash);
                  } catch (const std::out_of_range&) {
                    map_file = std::make_shared<MapFile>(floor, objects_data, enemies_data, events_data);
                    if (map_file->source_hash() != source_hash) {
                      throw std::logic_error("incorrect source hash");
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

std::shared_ptr<const SuperMap> DataIndex::get_free_play_supermap(
    Episode episode, GameMode mode, Difficulty difficulty, uint8_t floor, uint32_t layout, uint32_t entities) {
  uint32_t free_play_key = this->free_play_key(episode, mode, difficulty, floor, layout, entities);
  try {
    return this->supermap_for_free_play_key.at(free_play_key);
  } catch (const std::out_of_range&) {
  }

  const std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>* map_files;
  try {
    map_files = &this->map_files_for_free_play_key.at(free_play_key);
  } catch (const std::out_of_range&) {
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

  std::shared_ptr<const SuperMap> supermap;
  try {
    supermap = this->supermap_for_source_hash_sum.at(source_hash_sum);
    static_game_data_log.info_f("Linking existing free play supermap {:016X} for key {:08X}", source_hash_sum, free_play_key);
  } catch (const std::out_of_range&) {
    supermap = std::make_shared<SuperMap>(*map_files, SetDataTableBase::default_floor_to_area(Version::BB_V4, episode));
    this->supermap_for_source_hash_sum.emplace(source_hash_sum, supermap);
    static_game_data_log.info_f("Constructed free play supermap {:016X} for key {:08X}", source_hash_sum, free_play_key);
  }
  this->supermap_for_free_play_key.emplace(free_play_key, supermap);
  return supermap;
}

std::vector<std::shared_ptr<const SuperMap>> DataIndex::supermaps_for_variations(
    Episode episode, GameMode mode, Difficulty difficulty, const Variations& variations) {
  std::vector<std::shared_ptr<const SuperMap>> ret;
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

void DataIndex::load_set_data_tables() {
  config_log.info_f("Loading set data tables");

  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables;
  std::array<std::shared_ptr<const SetDataTableBase>, NUM_VERSIONS> new_tables_ep1_ult;
  std::shared_ptr<const SetDataTableBase> new_table_bb_solo;
  std::shared_ptr<const SetDataTableBase> new_table_bb_solo_ep1_ult;

  auto load_table = [&](Version version) -> void {
    auto data = this->load_map_file(version, "SetDataTableOn.rel");
    new_tables[static_cast<size_t>(version)] = std::make_shared<SetDataTable>(version, *data);
    if (!is_v1(version) && (version != Version::PC_NTE)) {
      auto data_ep1_ult = this->load_map_file(version, "SetDataTableOnUlti.rel");
      new_tables_ep1_ult[static_cast<size_t>(version)] = std::make_shared<SetDataTable>(version, *data_ep1_ult);
    }
  };

  new_tables[static_cast<size_t>(Version::DC_NTE)] = std::make_shared<SetDataTableDCNTE>();
  new_tables[static_cast<size_t>(Version::DC_11_2000)] = std::make_shared<SetDataTableDC112000>();
  load_table(Version::DC_V1);
  load_table(Version::DC_V2);
  load_table(Version::PC_NTE);
  load_table(Version::PC_V2);
  load_table(Version::GC_NTE);
  load_table(Version::GC_V3);
  load_table(Version::XB_V3);
  load_table(Version::BB_V4);

  auto bb_solo_data = this->load_map_file(Version::BB_V4, "SetDataTableOff.rel");
  new_table_bb_solo = std::make_shared<SetDataTable>(Version::BB_V4, *bb_solo_data);
  auto bb_solo_data_ep1_ult = this->load_map_file(Version::BB_V4, "SetDataTableOffUlti.rel");
  new_table_bb_solo_ep1_ult = std::make_shared<SetDataTable>(Version::BB_V4, *bb_solo_data_ep1_ult);

  this->set_data_tables = std::move(new_tables);
  this->set_data_tables_ep1_ult = std::move(new_tables_ep1_ult);
  this->bb_solo_set_data_table = std::move(new_table_bb_solo);
  this->bb_solo_set_data_table_ep1_ult = std::move(new_table_bb_solo_ep1_ult);
}

void DataIndex::load_battle_params() {
  config_log.info_f("Loading JSON battle parameters");
  this->battle_params = std::make_shared<JSONBattleParamsIndex>(phosg::JSON::parse(phosg::load_file(
      "system/tables/battle-params.json")));
}

void DataIndex::load_level_tables() {
  config_log.info_f("Loading level tables");
  this->level_table_v1_v2 = std::make_shared<JSONLevelTable>(phosg::JSON::parse(phosg::load_file(
      "system/tables/level-table-v1-v2.json")));
  this->level_table_v3 = std::make_shared<JSONLevelTable>(phosg::JSON::parse(phosg::load_file(
      "system/tables/level-table-v3.json")));
  this->level_table_v4 = std::make_shared<JSONLevelTable>(phosg::JSON::parse(phosg::load_file(
      "system/tables/level-table-v4.json")));
}

void DataIndex::load_text_index() {
  this->text_index = std::make_shared<TextIndex>("system/text-sets", [&](Version version, const std::string& filename) -> std::shared_ptr<const std::string> {
    try {
      if (version == Version::BB_V4) {
        return this->load_bb_file(filename);
      } else {
        return this->pc_patch_file_index->get("Media/PSO/" + filename)->load_data();
      }
    } catch (const std::out_of_range&) {
      return nullptr;
    } catch (const phosg::cannot_open_file&) {
      return nullptr;
    }
  });
}

void DataIndex::load_word_select_table() {
  config_log.info_f("Loading Word Select table");

  std::vector<std::vector<std::string>> name_alias_lists;
  auto json = phosg::JSON::parse(phosg::load_file("system/text-sets/ws-name-alias-lists.json"));
  for (const auto& coll_it : json.as_list()) {
    auto& coll = name_alias_lists.emplace_back();
    for (const auto& str_it : coll_it->as_list()) {
      coll.emplace_back(str_it->as_string());
    }
  }

  const std::vector<std::string>* pc_unitxt_collection = nullptr;
  const std::vector<std::string>* bb_unitxt_collection = nullptr;
  std::unique_ptr<UnicodeTextSet> pc_unitxt_data;
  if (this->text_index) {
    config_log.debug_f("(Word select) Using PC_V2 unitxt_e.prs from text index");
    pc_unitxt_collection = &this->text_index->get(Version::PC_V2, Language::ENGLISH, 35);
  } else {
    config_log.debug_f("(Word select) Loading PC_V2 unitxt_e.prs");
    pc_unitxt_data = std::make_unique<UnicodeTextSet>(phosg::load_file("system/text-sets/pc-v2/unitxt_e.prs"));
    pc_unitxt_collection = &pc_unitxt_data->get(35);
  }
  config_log.debug_f("(Word select) Loading BB_V4 unitxt_ws_e.prs");
  auto bb_unitxt_data = std::make_unique<UnicodeTextSet>(phosg::load_file("system/text-sets/bb-v4/unitxt_ws_e.prs"));
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
  this->word_select_table = std::make_shared<WordSelectTable>(
      dc_nte_ws, dc_112000_ws, dc_v1_ws, dc_v2_ws,
      pc_nte_ws, pc_v2_ws, gc_nte_ws, gc_v3_ws,
      gc_ep3_nte_ws, gc_ep3_ws, xb_v3_ws, bb_v4_ws,
      name_alias_lists);
}

std::shared_ptr<ItemNameIndex> DataIndex::create_item_name_index_for_version(
    std::shared_ptr<const ItemParameterTable> pmt,
    std::shared_ptr<const ItemData::StackLimits> limits,
    std::shared_ptr<const TextIndex> text_index) const {
  switch (limits->version) {
    case Version::DC_NTE:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_NTE, Language::JAPANESE, 2));
    case Version::DC_11_2000:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_11_2000, Language::ENGLISH, 2));
    case Version::DC_V1:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V1, Language::ENGLISH, 2));
    case Version::DC_V2:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::DC_V2, Language::ENGLISH, 3));
    case Version::PC_NTE:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_NTE, Language::ENGLISH, 3));
    case Version::PC_V2:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::PC_V2, Language::ENGLISH, 3));
    case Version::GC_NTE:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_NTE, Language::ENGLISH, 0));
    case Version::GC_V3:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::GC_V3, Language::ENGLISH, 0));
    case Version::XB_V3:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::XB_V3, Language::ENGLISH, 0));
    case Version::BB_V4:
      return std::make_shared<ItemNameIndex>(pmt, limits, text_index->get(Version::BB_V4, Language::ENGLISH, 1));
    default:
      return nullptr;
  }
}

void DataIndex::load_item_name_indexes() {
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

void DataIndex::load_drop_tables() {
  config_log.info_f("Loading item sets");

  std::unordered_map<std::string, std::shared_ptr<const RareItemSet>> new_rare_item_sets;
  std::unordered_map<std::string, std::shared_ptr<const CommonItemSet>> new_common_item_sets;
  for (const auto& item : std::filesystem::directory_iterator("system/tables")) {
    std::string filename = item.path().filename().string();

    if (filename.starts_with("common-table-") || filename.starts_with("ItemPT-")) {
      std::string path = "system/tables/" + filename;
      size_t ext_offset = filename.rfind('.');
      std::string basename = (ext_offset == std::string::npos) ? filename : filename.substr(0, ext_offset);

      if (filename.ends_with(".json")) {
        config_log.info_f("Loading JSON common item table {}", filename);
        new_common_item_sets.emplace(basename, std::make_shared<JSONCommonItemSet>(phosg::JSON::parse(phosg::load_file(path))));
      } else if (filename.ends_with(".afs")) {
        std::string ct_filename;
        if (filename.starts_with("ItemPT-")) {
          ct_filename = "ItemCT-" + filename.substr(7);
        } else if (filename.starts_with("common-table-")) {
          ct_filename = "challenge-common-table-" + filename.substr(13);
        } else {
          throw std::runtime_error(std::format("cannot determine challenge table filename for common table file: {}", filename));
        }
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        std::shared_ptr<std::string> ct_data;
        try {
          std::string ct_path = "system/tables/" + ct_filename;
          ct_data = std::make_shared<std::string>(phosg::load_file(ct_path));
          config_log.info_f("Loading AFS common item table {} with challenge table {}", filename, ct_filename);
        } catch (const phosg::cannot_open_file&) {
          config_log.info_f("Loading AFS common item table {} without challenge table", filename);
        }
        new_common_item_sets.emplace(basename, std::make_shared<AFSV2CommonItemSet>(data, ct_data));
      } else if (filename.ends_with(".gsl")) {
        config_log.info_f("Loading little-endian GSL common item table {}", filename);
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        new_common_item_sets.emplace(basename, std::make_shared<GSLV3V4CommonItemSet>(data, false));
      } else if (filename.ends_with(".gslb")) {
        config_log.info_f("Loading big-endian GSL common item table {}", filename);
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        new_common_item_sets.emplace(basename, std::make_shared<GSLV3V4CommonItemSet>(data, true));
      } else {
        throw std::runtime_error(std::format("unknown format for common table file: {}", filename));
      }

    } else if (filename.starts_with("rare-table-") || filename.starts_with("ItemRT-")) {
      std::string path = "system/tables/" + filename;
      size_t ext_offset = filename.rfind('.');
      std::string basename = (ext_offset == std::string::npos) ? filename : filename.substr(0, ext_offset);

      std::shared_ptr<RareItemSet> rare_set;
      if (filename.ends_with("-v1.json")) {
        config_log.info_f("Loading v1 JSON rare item table {}", filename);
        rare_set = std::make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::DC_V1));
      } else if (filename.ends_with("-v2.json")) {
        config_log.info_f("Loading v2 JSON rare item table {}", filename);
        rare_set = std::make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::PC_V2));
      } else if (filename.ends_with("-v3.json")) {
        config_log.info_f("Loading v3 JSON rare item table {}", filename);
        rare_set = std::make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::GC_V3));
      } else if (filename.ends_with("-v4.json")) {
        config_log.info_f("Loading v4 JSON rare item table {}", filename);
        rare_set = std::make_shared<RareItemSet>(phosg::JSON::parse(phosg::load_file(path)), this->item_name_index(Version::BB_V4));

      } else if (filename.ends_with(".afs")) {
        config_log.info_f("Loading AFS rare item table {}", filename);
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        rare_set = std::make_shared<RareItemSet>(AFSArchive(data), false);

      } else if (filename.ends_with(".gsl")) {
        config_log.info_f("Loading GSL rare item table {}", filename);
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        rare_set = std::make_shared<RareItemSet>(GSLArchive(data, false), false);

      } else if (filename.ends_with(".gslb")) {
        config_log.info_f("Loading GSL rare item table {}", filename);
        auto data = std::make_shared<std::string>(phosg::load_file(path));
        rare_set = std::make_shared<RareItemSet>(GSLArchive(data, true), true);

      } else if (filename.ends_with(".rel")) {
        config_log.info_f("Loading REL rare item table {}", filename);
        rare_set = std::make_shared<RareItemSet>(phosg::load_file(path), true);

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
  auto armor_json = phosg::JSON::parse(phosg::load_file("system/tables/armor-shop-random-set.json"));
  auto new_armor_random_set = std::make_shared<ArmorShopRandomSet>(armor_json);

  config_log.info_f("Loading tool table");
  auto tool_json = phosg::JSON::parse(phosg::load_file("system/tables/tool-shop-random-set.json"));
  auto new_tool_random_set = std::make_shared<ToolShopRandomSet>(tool_json);

  config_log.info_f("Loading weapon tables");
  std::array<std::shared_ptr<const WeaponShopRandomSet>, 4> new_weapon_random_sets;
  const char* filenames[4] = {
      "system/tables/weapon-shop-random-set-normal.json",
      "system/tables/weapon-shop-random-set-hard.json",
      "system/tables/weapon-shop-random-set-very-hard.json",
      "system/tables/weapon-shop-random-set-ultimate.json",
  };
  for (size_t z = 0; z < 4; z++) {
    new_weapon_random_sets[z] = std::make_shared<WeaponShopRandomSet>(
        phosg::JSON::parse(phosg::load_file(filenames[z])));
  }

  config_log.info_f("Loading tekker adjustment set");
  auto tekker_data = phosg::JSON::parse(phosg::load_file("system/tables/tekker-adjustment-set.json"));
  auto new_tekker_adjustment_set = std::make_shared<TekkerAdjustmentSet>(tekker_data);

  this->rare_item_sets = std::move(new_rare_item_sets);
  this->common_item_sets = std::move(new_common_item_sets);
  this->armor_random_set = std::move(new_armor_random_set);
  this->tool_random_set = std::move(new_tool_random_set);
  this->weapon_random_sets = std::move(new_weapon_random_sets);
  this->tekker_adjustment_set = std::move(new_tekker_adjustment_set);
}

void DataIndex::load_item_definitions() {
  std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS> new_item_parameter_tables;
  config_log.info_f("Loading item definition tables");
  for (size_t v_s = NUM_PATCH_VERSIONS; v_s < NUM_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s);
    std::string json_path = std::format("system/tables/item-parameter-table-{}.json", file_path_token_for_version(v));
    try {
      config_log.debug_f("Loading item definition table {}", json_path);
      new_item_parameter_tables[v_s] = ItemParameterTable::from_json(phosg::JSON::parse(phosg::load_file(json_path)));
    } catch (const std::exception& e) {
      std::string path = std::format("system/tables/ItemPMT-{}.prs", file_path_token_for_version(v));
      config_log.debug_f("Cannot load {} ({}); loading item definition table {}", json_path, e.what(), path);
      auto data = std::make_shared<std::string>(prs_decompress(phosg::load_file(path)));
      new_item_parameter_tables[v_s] = ItemParameterTable::from_binary(data, v);
    }
  }

  auto json = phosg::JSON::parse(phosg::load_file("system/tables/translation-table.json"));
  auto new_item_translation_table = std::make_shared<ItemTranslationTable>(json, new_item_parameter_tables);

  config_log.info_f("Creating DC NTE mag metadata table");
  auto new_table_dc_nte = MagMetadataTable::from_binary(nullptr, Version::DC_NTE);
  config_log.info_f("Loading DC 11/2000 mag metadata table");
  auto new_table_11_2000 = MagMetadataTable::from_json(phosg::JSON::parse(phosg::load_file(
      "system/tables/mag-metadata-table-dc-11-2000.json")));
  config_log.info_f("Loading v1 mag metadata table");
  auto new_table_v1 = MagMetadataTable::from_json(phosg::JSON::parse(phosg::load_file(
      "system/tables/mag-metadata-table-v1.json")));
  config_log.info_f("Loading v2 mag metadata table");
  auto new_table_v2 = MagMetadataTable::from_json(phosg::JSON::parse(phosg::load_file(
      "system/tables/mag-metadata-table-v2.json")));
  config_log.info_f("Loading v3 mag metadata table");
  auto new_table_v3 = MagMetadataTable::from_json(phosg::JSON::parse(phosg::load_file(
      "system/tables/mag-metadata-table-v3.json")));
  config_log.info_f("Loading v4 mag metadata table");
  auto new_table_v4 = MagMetadataTable::from_json(phosg::JSON::parse(phosg::load_file(
      "system/tables/mag-metadata-table-v4.json")));

  this->item_parameter_tables = std::move(new_item_parameter_tables);
  this->item_translation_table = std::move(new_item_translation_table);
  this->mag_metadata_table_dc_nte = std::move(new_table_dc_nte);
  this->mag_metadata_table_dc_11_2000 = std::move(new_table_11_2000);
  this->mag_metadata_table_v1 = std::move(new_table_v1);
  this->mag_metadata_table_v2 = std::move(new_table_v2);
  this->mag_metadata_table_v3 = std::move(new_table_v3);
  this->mag_metadata_table_v4 = std::move(new_table_v4);
}

void DataIndex::load_ep3_cards() {
  config_log.info_f("Loading Episode 3 card definitions");
  this->ep3_card_index = std::make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions.mnr",
      "system/ep3/card-definitions.mnrd",
      "system/ep3/card-text.mnr",
      "system/ep3/card-text.mnrd",
      "system/ep3/card-dice-text.mnr",
      "system/ep3/card-dice-text.mnrd");
  config_log.info_f("Loading Episode 3 trial card definitions");
  this->ep3_card_index_trial = std::make_shared<Episode3::CardIndex>(
      "system/ep3/card-definitions-trial.mnr",
      "system/ep3/card-definitions-trial.mnrd",
      "system/ep3/card-text-trial.mnr",
      "system/ep3/card-text-trial.mnrd",
      "system/ep3/card-dice-text-trial.mnr",
      "system/ep3/card-dice-text-trial.mnrd");
  config_log.info_f("Loading Episode 3 COM decks");
  this->ep3_com_deck_index = std::make_shared<Episode3::COMDeckIndex>("system/ep3/com-decks.json");
}

void DataIndex::load_ep3_maps(bool raise_on_any_failure) {
  config_log.info_f("Collecting Episode 3 maps");
  this->ep3_map_index = std::make_shared<Episode3::MapIndex>("system/ep3/maps", raise_on_any_failure);
}

void DataIndex::load_quest_index(bool raise_on_any_failure) {
  config_log.info_f("Collecting quests");
  this->quest_index = std::make_shared<QuestIndex>("system/quests", this->quest_category_index, raise_on_any_failure);
}

void DataIndex::compile_functions(bool raise_on_any_failure) {
  config_log.info_f("Compiling client functions");
  this->client_functions = std::make_shared<ClientFunctionIndex>("system/client-functions", raise_on_any_failure);
}

void DataIndex::load_dol_files() {
  config_log.info_f("Loading DOL files");
  this->dol_file_index = std::make_shared<DOLFileIndex>("system/dol");
}

void DataIndex::generate_bb_stream_file() {
  config_log.info_f("Generating BB stream file");
  auto sf = std::make_shared<BBStreamFile>();

  auto add_file = [&](const std::string& filename, const void* data, size_t size) -> void {
    auto& e = sf->entries.emplace_back();
    e.offset = sf->data.size();
    e.filename = filename;
    e.size = size;
    e.checksum = phosg::crc32(data, size);
    sf->data.append(reinterpret_cast<const char*>(data), size);
    config_log.debug_f(
        "[BBStreamFile] Added file {} at offset {:08X} ({:08X} bytes) with checksum {:08X}; total size is now {:08X}",
        filename, e.offset, e.size, e.checksum, sf->data.size());
  };

  auto level_table_data = prs_compress_optimal(this->level_table_v4->serialize_binary_v4());
  auto pmt_data = prs_compress_optimal(this->item_parameter_table(Version::BB_V4)->serialize_binary(Version::BB_V4));
  auto mag_data = prs_compress_optimal(this->mag_metadata_table(Version::BB_V4)->serialize_binary(Version::BB_V4));

  const auto& bps = *this->battle_params;
  add_file("BattleParamEntry.dat", &bps.get_table(true, Episode::EP1), sizeof(BattleParamsIndex::Table));
  add_file("BattleParamEntry_lab.dat", &bps.get_table(true, Episode::EP2), sizeof(BattleParamsIndex::Table));
  add_file("BattleParamEntry_ep4.dat", &bps.get_table(true, Episode::EP4), sizeof(BattleParamsIndex::Table));
  add_file("BattleParamEntry_on.dat", &bps.get_table(false, Episode::EP1), sizeof(BattleParamsIndex::Table));
  add_file("BattleParamEntry_lab_on.dat", &bps.get_table(false, Episode::EP2), sizeof(BattleParamsIndex::Table));
  add_file("BattleParamEntry_ep4_on.dat", &bps.get_table(false, Episode::EP4), sizeof(BattleParamsIndex::Table));
  add_file("PlyLevelTbl.prs", level_table_data.data(), level_table_data.size());
  add_file("ItemMagEdit.prs", mag_data.data(), mag_data.size());
  add_file("ItemPMT.prs", pmt_data.data(), pmt_data.size());

  this->bb_stream_file = sf;
}

void DataIndex::load_all() {
  this->collect_network_addresses();
  this->load_config_early();
  this->load_bb_private_keys();
  this->load_bb_system_defaults();
  this->load_patch_indexes();
  this->load_ep3_cards();
  this->load_ep3_maps();
  this->compile_functions();
  this->load_dol_files();
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
  this->load_quest_index();
  this->generate_bb_stream_file();
}
