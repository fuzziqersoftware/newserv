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

using namespace std;

ServerState::ServerState(const char* config_filename, bool is_replay)
    : config_filename(config_filename),
      is_replay(is_replay),
      dns_server_port(0),
      ip_stack_debug(false),
      allow_unregistered_users(false),
      allow_saving(true),
      allow_dc_pc_games(false),
      allow_gc_xb_games(true),
      item_tracking_enabled(true),
      drops_enabled(true),
      ep3_send_function_call_enabled(false),
      catch_handler_exceptions(true),
      ep3_infinite_meseta(false),
      ep3_defeat_player_meseta_rewards({400, 500, 600, 700, 800}),
      ep3_defeat_com_meseta_rewards({100, 200, 300, 400, 500}),
      ep3_final_round_meseta_bonus(300),
      ep3_jukebox_is_free(false),
      ep3_behavior_flags(0),
      run_shell_behavior(RunShellBehavior::DEFAULT),
      cheat_mode_behavior(CheatModeBehavior::OFF_BY_DEFAULT),
      ep3_card_auction_points(0),
      ep3_card_auction_min_size(0),
      ep3_card_auction_max_size(0),
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
    auto lobby_name = decode_sjis(string_printf("LOBBY%zu", x + 1));
    bool v2_and_later_only = (x > 9);
    bool is_ep3_only = (x > 14);

    shared_ptr<Lobby> l = this->create_lobby();
    l->flags |=
        Lobby::Flag::PUBLIC |
        Lobby::Flag::DEFAULT |
        Lobby::Flag::PERSISTENT |
        (v2_and_later_only ? Lobby::Flag::V2_AND_LATER : 0);
    l->block = x + 1;
    l->name = lobby_name;
    l->max_clients = 12;
    if (is_ep3_only) {
      l->episode = Episode::EP3;
    }

    if (!v2_and_later_only) {
      this->public_lobby_search_order_v1.emplace_back(l);
    }
    if (!is_ep3_only) {
      this->public_lobby_search_order_non_v1.emplace_back(l);
    } else {
      ep3_only_lobbies.emplace_back(l);
    }
  }

  // Annoyingly, the CARD lobbies should be searched first, but are sent at the
  // end of the lobby list command, so we have to change the search order
  // manually here.
  this->public_lobby_search_order_ep3 = this->public_lobby_search_order_non_v1;
  this->public_lobby_search_order_ep3.insert(
      this->public_lobby_search_order_ep3.begin(),
      ep3_only_lobbies.begin(),
      ep3_only_lobbies.end());

  // Load all the necessary data
  auto config = this->load_config();
  this->collect_network_addresses();
  this->parse_config(config, false);
  this->load_bb_private_keys();
  this->load_licenses();
  this->load_patch_indexes();
  this->load_battle_params();
  this->load_level_table();
  this->load_item_tables();
  this->load_ep3_data();
  this->resolve_ep3_card_names();
  this->load_quest_index();
  this->compile_functions();
  this->load_dol_files();

  if (this->is_replay) {
    this->allow_saving = false;
    config_log.info("Saving disabled because this is a replay session");
  }
}

void ServerState::add_client_to_available_lobby(shared_ptr<Client> c) {
  shared_ptr<Lobby> added_to_lobby;

  if (c->preferred_lobby_id >= 0) {
    try {
      auto l = this->find_lobby(c->preferred_lobby_id);
      if (l &&
          !l->is_game() &&
          (l->flags & Lobby::Flag::PUBLIC) &&
          ((c->flags & Client::Flag::IS_EPISODE_3) || (l->episode != Episode::EP3))) {
        l->add_client(c);
        added_to_lobby = l;
      }
    } catch (const out_of_range&) {
    }
  }

  if (!added_to_lobby.get()) {
    const auto* search_order = &this->public_lobby_search_order_non_v1;
    if (c->flags & Client::Flag::IS_DC_V1) {
      search_order = &this->public_lobby_search_order_v1;
    } else if (c->flags & Client::Flag::IS_EPISODE_3) {
      search_order = &this->public_lobby_search_order_ep3;
    }
    for (const auto& l : *search_order) {
      try {
        l->add_client(c);
        added_to_lobby = l;
        break;
      } catch (const out_of_range&) {
      }
    }
  }

  if (!added_to_lobby) {
    added_to_lobby = this->create_lobby();
    added_to_lobby->flags |= Lobby::Flag::PUBLIC | Lobby::Flag::IS_OVERFLOW;
    added_to_lobby->block = 100;
    added_to_lobby->name = u"Overflow";
    added_to_lobby->max_clients = 12;
    added_to_lobby->event = this->pre_lobby_event;
    added_to_lobby->add_client(c);
  }

  // Send a join message to the joining player, and notifications to all others
  this->send_lobby_join_notifications(added_to_lobby, c);
}

void ServerState::remove_client_from_lobby(shared_ptr<Client> c) {
  auto l = c->lobby.lock();
  if (l) {
    l->remove_client(c);
    if (!(l->flags & Lobby::Flag::PERSISTENT) && (l->count_clients() == 0)) {
      this->remove_lobby(l->lobby_id);
    } else {
      send_player_leave_notification(l, c->lobby_client_id);
    }
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
    if (!(current_lobby->flags & Lobby::Flag::PERSISTENT) && (current_lobby->count_clients() == 0)) {
      this->remove_lobby(current_lobby->lobby_id);
    } else {
      send_player_leave_notification(current_lobby, old_lobby_client_id);
    }
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

shared_ptr<Lobby> ServerState::create_lobby() {
  while (this->id_to_lobby.count(this->next_lobby_id)) {
    this->next_lobby_id++;
  }
  shared_ptr<Lobby> l(new Lobby(this->shared_from_this(), this->next_lobby_id++));
  this->id_to_lobby.emplace(l->lobby_id, l);
  l->log.info("Created lobby");
  return l;
}

void ServerState::remove_lobby(uint32_t lobby_id) {
  auto lobby_it = this->id_to_lobby.find(lobby_id);
  if (lobby_it == this->id_to_lobby.end()) {
    throw logic_error("attempted to remove nonexistent lobby");
  }

  auto l = lobby_it->second;
  if (l->count_clients() != 0) {
    throw logic_error("attempted to delete lobby with clients in it");
  }

  if (l->flags & Lobby::Flag::IS_SPECTATOR_TEAM) {
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

  l->log.info("Deleted lobby");
  this->id_to_lobby.erase(lobby_it);
}

shared_ptr<Client> ServerState::find_client(const std::u16string* identifier,
    uint64_t serial_number, shared_ptr<Lobby> l) {

  if ((serial_number == 0) && identifier) {
    try {
      serial_number = stoull(encode_sjis(*identifier), nullptr, 0);
    } catch (const exception&) {
    }
  }

  // look in the current lobby first
  if (l) {
    try {
      return l->find_client(identifier, serial_number);
    } catch (const out_of_range&) {
    }
  }

  // look in all lobbies if not found
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

uint32_t ServerState::connect_address_for_client(std::shared_ptr<Client> c) const {
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

std::shared_ptr<const Menu> ServerState::information_menu_for_version(GameVersion version) const {
  if ((version == GameVersion::DC) || (version == GameVersion::PC)) {
    return this->information_menu_v2;
  } else if ((version == GameVersion::GC) || (version == GameVersion::XB)) {
    return this->information_menu_v3;
  }
  throw out_of_range("no information menu exists for this version");
}

shared_ptr<const Menu> ServerState::proxy_destinations_menu_for_version(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
      return this->proxy_destinations_menu_dc;
    case GameVersion::PC:
      return this->proxy_destinations_menu_pc;
    case GameVersion::GC:
      return this->proxy_destinations_menu_gc;
    case GameVersion::XB:
      return this->proxy_destinations_menu_xb;
    default:
      throw out_of_range("no proxy destinations menu exists for this version");
  }
}

const vector<pair<string, uint16_t>>& ServerState::proxy_destinations_for_version(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
      return this->proxy_destinations_dc;
    case GameVersion::PC:
      return this->proxy_destinations_pc;
    case GameVersion::GC:
      return this->proxy_destinations_gc;
    case GameVersion::XB:
      return this->proxy_destinations_xb;
    default:
      throw out_of_range("no proxy destinations menu exists for this version");
  }
}

void ServerState::set_port_configuration(
    const vector<PortConfiguration>& port_configs) {
  this->name_to_port_config.clear();
  this->number_to_port_config.clear();

  bool any_port_is_pc_console_detect = false;
  for (const auto& pc : port_configs) {
    shared_ptr<PortConfiguration> spc(new PortConfiguration(pc));
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
    const std::string& patch_index_filename,
    const std::string& gsl_filename,
    const std::string& bb_directory_filename) const {

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
      shared_ptr<string> ret(new string(this->bb_data_gsl->get_copy(effective_gsl_filename)));
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
        shared_ptr<string> ret(new string(this->bb_data_gsl->get_copy(no_ext_gsl_filename)));
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
    pc.version = version_for_name(item_list->at(1).as_string().c_str());
    pc.behavior = server_behavior_for_name(item_list->at(2).as_string().c_str());
  }
  return ret;
}

void ServerState::parse_config(const JSON& json, bool is_reload) {
  config_log.info("Parsing configuration");

  this->name = decode_sjis(json.at("ServerName").as_string());

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
  this->item_tracking_enabled = json.get_bool("EnableItemTracking", this->item_tracking_enabled);
  this->drops_enabled = json.get_bool("EnableDrops", this->drops_enabled);
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
      shared_ptr<G_SetEXResultValues_GC_Ep3_6xB4x4B> ret(new G_SetEXResultValues_GC_Ep3_6xB4x4B());
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

  set_log_levels_from_json(json.get("LogLevels", JSON::dict()));

  if (!is_reload) {
    try {
      this->run_shell_behavior = json.at("RunInteractiveShell").as_bool()
          ? ServerState::RunShellBehavior::ALWAYS
          : ServerState::RunShellBehavior::NEVER;
    } catch (const out_of_range&) {
    }
  }

  try {
    const string& behavior = json.at("CheatModeBehavior").as_string();
    if (behavior == "Off") {
      this->cheat_mode_behavior = CheatModeBehavior::OFF;
    } else if (behavior == "OffByDefault") {
      this->cheat_mode_behavior = CheatModeBehavior::OFF_BY_DEFAULT;
    } else if (behavior == "OnByDefault") {
      this->cheat_mode_behavior = CheatModeBehavior::ON_BY_DEFAULT;
    } else {
      throw runtime_error("invalid value for CheatModeBehavior");
    }
  } catch (const out_of_range&) {
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
      this->quest_category_index.reset(new QuestCategoryIndex(json.at("QuestCategories")));
    } catch (const exception& e) {
      throw runtime_error(string_printf(
          "QuestCategories is missing or invalid in config.json (%s) - see config.example.json for an example", e.what()));
    }
  }

  config_log.info("Creating menus");

  shared_ptr<Menu> information_menu_v2(new Menu(MenuID::INFORMATION, u"Information"));
  shared_ptr<Menu> information_menu_v3(new Menu(MenuID::INFORMATION, u"Information"));
  shared_ptr<vector<u16string>> information_contents(new vector<u16string>());

  information_menu_v2->items.emplace_back(InformationMenuItemID::GO_BACK, u"Go back",
      u"Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  information_menu_v3->items.emplace_back(InformationMenuItemID::GO_BACK, u"Go back",
      u"Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  {
    uint32_t item_id = 0;
    for (const auto& item : json.at("InformationMenuContents").as_list()) {
      u16string name = decode_sjis(item->get_string(0));
      u16string short_desc = decode_sjis(item->get_string(1));
      information_menu_v2->items.emplace_back(item_id, name, short_desc, 0);
      information_menu_v3->items.emplace_back(item_id, name, short_desc, MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
      information_contents->emplace_back(decode_sjis(item->get_string(2)));
      item_id++;
    }
  }
  this->information_menu_v2 = information_menu_v2;
  this->information_menu_v3 = information_menu_v3;
  this->information_contents = information_contents;

  auto generate_proxy_destinations_menu = [&](vector<pair<string, uint16_t>>& ret_pds, const char* key) -> shared_ptr<const Menu> {
    shared_ptr<Menu> ret(new Menu(MenuID::PROXY_DESTINATIONS, u"Proxy server"));
    ret_pds.clear();

    try {
      map<string, const JSON&> sorted_jsons;
      for (const auto& it : json.at(key).as_dict()) {
        sorted_jsons.emplace(it.first, *it.second);
      }

      ret->items.emplace_back(ProxyDestinationsMenuItemID::GO_BACK, u"Go back",
          u"Return to the\nmain menu", 0);
      ret->items.emplace_back(ProxyDestinationsMenuItemID::OPTIONS, u"Options",
          u"Set proxy session\noptions", 0);

      uint32_t item_id = 0;
      for (const auto& item : sorted_jsons) {
        const string& netloc_str = item.second.as_string();
        const string& description = "$C7Remote server:\n$C6" + netloc_str;
        ret->items.emplace_back(item_id, decode_sjis(item.first), decode_sjis(description), 0);
        ret_pds.emplace_back(parse_netloc(netloc_str));
        item_id++;
      }
    } catch (const out_of_range&) {
    }
    return ret;
  };

  this->proxy_destinations_menu_dc = generate_proxy_destinations_menu(
      this->proxy_destinations_dc, "ProxyDestinations-DC");
  this->proxy_destinations_menu_pc = generate_proxy_destinations_menu(
      this->proxy_destinations_pc, "ProxyDestinations-PC");
  this->proxy_destinations_menu_gc = generate_proxy_destinations_menu(
      this->proxy_destinations_gc, "ProxyDestinations-GC");
  this->proxy_destinations_menu_xb = generate_proxy_destinations_menu(
      this->proxy_destinations_xb, "ProxyDestinations-XB");

  try {
    const string& netloc_str = json.get_string("ProxyDestination-Patch");
    this->proxy_destination_patch = parse_netloc(netloc_str);
    config_log.info("Patch server proxy is enabled with destination %s", netloc_str.c_str());
    for (auto& it : this->name_to_port_config) {
      if (it.second->version == GameVersion::PATCH) {
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
      if (it.second->version == GameVersion::BB) {
        it.second->behavior = ServerBehavior::PROXY_SERVER;
      }
    }
  } catch (const out_of_range&) {
    this->proxy_destination_bb.first = "";
    this->proxy_destination_bb.second = 0;
  }

  this->welcome_message = decode_sjis(json.get_string("WelcomeMessage", ""));
  this->pc_patch_server_message = decode_sjis(json.get_string("PCPatchServerMessage", ""));
  this->bb_patch_server_message = decode_sjis(json.get_string("BBPatchServerMessage", ""));
}

void ServerState::load_bb_private_keys() {
  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    this->bb_private_keys.emplace_back(new PSOBBEncryption::KeyFile(
        load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.info("Loaded Blue Burst key file: %s", filename.c_str());
  }
  config_log.info("%zu Blue Burst key file(s) loaded", this->bb_private_keys.size());
}

void ServerState::load_licenses() {
  config_log.info("Loading license list");
  this->license_index.reset(new LicenseIndex());
}

void ServerState::load_patch_indexes() {
  if (isdir("system/patch-pc")) {
    config_log.info("Indexing PSO PC patch files");
    this->pc_patch_file_index.reset(new PatchFileIndex("system/patch-pc"));
  } else {
    config_log.info("PSO PC patch files not present");
  }
  if (isdir("system/patch-bb")) {
    config_log.info("Indexing PSO BB patch files");
    this->bb_patch_file_index.reset(new PatchFileIndex("system/patch-bb"));
    try {
      auto gsl_file = this->bb_patch_file_index->get("./data/data.gsl");
      this->bb_data_gsl.reset(new GSLArchive(gsl_file->load_data(), false));
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
  this->battle_params.reset(new BattleParamsIndex(
      this->load_bb_file("BattleParamEntry_on.dat"),
      this->load_bb_file("BattleParamEntry_lab_on.dat"),
      this->load_bb_file("BattleParamEntry_ep4_on.dat"),
      this->load_bb_file("BattleParamEntry.dat"),
      this->load_bb_file("BattleParamEntry_lab.dat"),
      this->load_bb_file("BattleParamEntry_ep4.dat")));
}

void ServerState::load_level_table() {
  config_log.info("Loading level table");
  this->level_table.reset(new LevelTable(
      this->load_bb_file("PlyLevelTbl.prs"), true));
}

void ServerState::load_item_tables() {
  config_log.info("Loading rare item sets");
  for (const auto& filename : list_directory_sorted("system/rare-tables")) {
    string path = "system/rare-tables/" + filename;
    size_t ext_offset = filename.rfind('.');
    string basename = (ext_offset == string::npos) ? filename : filename.substr(0, ext_offset);

    if (ends_with(filename, ".json")) {
      config_log.info("Loading JSON rare item table %s", filename.c_str());
      this->rare_item_sets.emplace(basename, new JSONRareItemSet(JSON::parse(load_file(path))));

    } else if (ends_with(filename, ".afs")) {
      config_log.info("Loading AFS rare item table %s", filename.c_str());
      shared_ptr<string> data(new string(load_file(path)));
      this->rare_item_sets.emplace(basename, new AFSRareItemSet(data));

    } else if (ends_with(filename, ".gsl")) {
      config_log.info("Loading GSL rare item table %s", filename.c_str());
      shared_ptr<string> data(new string(load_file(path)));
      this->rare_item_sets.emplace(basename, new GSLRareItemSet(data, false));

    } else if (ends_with(filename, ".gslb")) {
      config_log.info("Loading GSL rare item table %s", filename.c_str());
      shared_ptr<string> data(new string(load_file(path)));
      this->rare_item_sets.emplace(basename, new GSLRareItemSet(data, true));

    } else if (ends_with(filename, ".reg")) {
      config_log.info("Loading REL rare item table %s", filename.c_str());
      shared_ptr<string> data(new string(load_file(path)));
      this->rare_item_sets.emplace(basename, new RELRareItemSet(data));
    }
  }

  if (!this->rare_item_sets.count("default-v4")) {
    config_log.info("default-v4 rare item set is not available; loading from BB data");
    this->rare_item_sets.emplace("default-v4", new RELRareItemSet(this->load_bb_file("ItemRT.rel")));
  }

  // Note: These files don't exist in BB, so we use the GC versions of them
  // instead. This doesn't include Episode 4 of course, so we use Episode 1
  // parameters for Episode 4 implicitly.
  config_log.info("Loading common item table");
  shared_ptr<string> pt_data(new string(load_file(
      "system/blueburst/ItemPT_GC.gsl")));
  this->common_item_set.reset(new CommonItemSet(pt_data));

  config_log.info("Loading armor table");
  shared_ptr<string> armor_data(new string(load_file(
      "system/blueburst/ArmorRandom_GC.rel")));
  this->armor_random_set.reset(new ArmorRandomSet(armor_data));

  config_log.info("Loading tool table");
  shared_ptr<string> tool_data(new string(load_file(
      "system/blueburst/ToolRandom_GC.rel")));
  this->tool_random_set.reset(new ToolRandomSet(tool_data));

  config_log.info("Loading weapon tables");
  const char* filenames[4] = {
      "system/blueburst/WeaponRandomNormal_GC.rel",
      "system/blueburst/WeaponRandomHard_GC.rel",
      "system/blueburst/WeaponRandomVeryHard_GC.rel",
      "system/blueburst/WeaponRandomUltimate_GC.rel",
  };
  for (size_t z = 0; z < 4; z++) {
    shared_ptr<string> weapon_data(new string(load_file(filenames[z])));
    this->weapon_random_sets[z].reset(new WeaponRandomSet(weapon_data));
  }

  config_log.info("Loading tekker adjustment table");
  shared_ptr<string> tekker_data(new string(load_file(
      "system/blueburst/JudgeItem_GC.rel")));
  this->tekker_adjustment_set.reset(new TekkerAdjustmentSet(tekker_data));

  config_log.info("Loading item definition table");
  shared_ptr<string> pmt_data(new string(prs_decompress(load_file(
      "system/blueburst/ItemPMT.prs"))));
  this->item_parameter_table.reset(new ItemParameterTable(pmt_data));

  config_log.info("Loading mag evolution table");
  shared_ptr<string> mag_data(new string(prs_decompress(load_file(
      "system/blueburst/ItemMagEdit.prs"))));
  this->mag_evolution_table.reset(new MagEvolutionTable(mag_data));
}

void ServerState::load_ep3_data() {
  config_log.info("Collecting Episode 3 maps");
  this->ep3_map_index.reset(new Episode3::MapIndex("system/ep3/maps"));
  config_log.info("Loading Episode 3 card definitions");
  this->ep3_card_index.reset(new Episode3::CardIndex(
      "system/ep3/card-definitions.mnr",
      "system/ep3/card-definitions.mnrd",
      "system/ep3/card-text.mnr",
      "system/ep3/card-text.mnrd",
      "system/ep3/card-dice-text.mnr",
      "system/ep3/card-dice-text.mnrd"));
  config_log.info("Loading Episode 3 trial card definitions");
  this->ep3_card_index_trial.reset(new Episode3::CardIndex(
      "system/ep3/card-definitions-trial.mnr",
      "system/ep3/card-definitions-trial.mnrd",
      "system/ep3/card-text-trial.mnr",
      "system/ep3/card-text-trial.mnrd",
      "system/ep3/card-dice-text-trial.mnr",
      "system/ep3/card-dice-text-trial.mnrd"));
  config_log.info("Loading Episode 3 COM decks");
  this->ep3_com_deck_index.reset(new Episode3::COMDeckIndex("system/ep3/com-decks.json"));

  const string& tournament_state_filename = "system/ep3/tournament-state.json";
  this->ep3_tournament_index.reset(new Episode3::TournamentIndex(
      this->ep3_map_index, this->ep3_com_deck_index, tournament_state_filename));
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
  this->default_quest_index.reset(new QuestIndex("system/quests", this->quest_category_index));
  config_log.info("Collecting Episode 3 download quests");
  this->ep3_download_quest_index.reset(new QuestIndex("system/ep3/maps-download", this->quest_category_index));
}

void ServerState::compile_functions() {
  config_log.info("Compiling client functions");
  this->function_code_index.reset(new FunctionCodeIndex("system/ppc"));
}

void ServerState::load_dol_files() {
  config_log.info("Loading DOL files");
  this->dol_file_index.reset(new DOLFileIndex("system/dol"));
}

shared_ptr<const QuestIndex> ServerState::quest_index_for_client(shared_ptr<Client> c) const {
  return (c->flags & Client::Flag::IS_EPISODE_3)
      ? this->ep3_download_quest_index
      : this->default_quest_index;
}
