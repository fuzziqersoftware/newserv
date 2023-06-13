#include "ServerState.hh"

#include <string.h>

#include <memory>
#include <phosg/Network.hh>

#include "Compression.hh"
#include "FileContentsCache.hh"
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
      item_tracking_enabled(true),
      drops_enabled(true),
      episode_3_send_function_call_enabled(false),
      enable_dol_compression(false),
      catch_handler_exceptions(true),
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
      proxy_enable_login_options(false) {
  vector<shared_ptr<Lobby>> non_v1_only_lobbies;
  vector<shared_ptr<Lobby>> ep3_only_lobbies;

  for (size_t x = 0; x < 20; x++) {
    auto lobby_name = decode_sjis(string_printf("LOBBY%zu", x + 1));
    bool is_non_v1_only = (x > 9);
    bool is_ep3_only = (x > 14);

    shared_ptr<Lobby> l = this->create_lobby();
    l->flags |=
        Lobby::Flag::PUBLIC |
        Lobby::Flag::DEFAULT |
        Lobby::Flag::PERSISTENT |
        (is_non_v1_only ? Lobby::Flag::NON_V1_ONLY : 0);
    l->block = x + 1;
    l->type = x;
    l->name = lobby_name;
    l->max_clients = 12;
    if (is_ep3_only) {
      l->episode = Episode::EP3;
    }

    if (!is_non_v1_only) {
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
  this->parse_config(config);
  this->load_licenses();
  this->load_patch_indexes();
  this->load_battle_params();
  this->load_level_table();
  this->load_item_tables();
  this->load_ep3_data();
  this->load_quest_index();
  this->compile_functions();
  this->load_dol_files();
  this->create_menus(config);

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
      if (!l->is_game() && (l->flags & Lobby::Flag::PUBLIC)) {
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
    // TODO: Add the user to a dynamically-created private lobby instead
    throw out_of_range("all lobbies full");
  }

  // Send a join message to the joining player, and notifications to all others
  this->send_lobby_join_notifications(added_to_lobby, c);
}

void ServerState::remove_client_from_lobby(shared_ptr<Client> c) {
  auto l = this->id_to_lobby.at(c->lobby_id);
  l->remove_client(c);
  if (!(l->flags & Lobby::Flag::PERSISTENT) && (l->count_clients() == 0)) {
    this->remove_lobby(l->lobby_id);
  } else {
    send_player_leave_notification(l, c->lobby_client_id);
  }
}

bool ServerState::change_client_lobby(
    shared_ptr<Client> c,
    shared_ptr<Lobby> new_lobby,
    bool send_join_notification,
    ssize_t required_client_id) {
  uint8_t old_lobby_client_id = c->lobby_client_id;

  shared_ptr<Lobby> current_lobby = this->find_lobby(c->lobby_id);
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
  return this->id_to_lobby.at(lobby_id);
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
  shared_ptr<Lobby> l(new Lobby(this->next_lobby_id++));
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
      l->log.info("Watched lobby is missing");
    }
    l->watched_lobby.reset();
  } else {
    // Tell all players in all spectator teams to go back to the lobby
    for (auto watcher_l : l->watcher_lobbies) {
      if (!watcher_l->is_ep3()) {
        throw logic_error("spectator team is not an Episode 3 lobby");
      }
      l->log.info("Disbanding watcher lobby %" PRIX32, watcher_l->lobby_id);
      send_command(watcher_l, 0xED, 0x00);
    }
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

uint32_t ServerState::connect_address_for_client(std::shared_ptr<Client> c) {
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

std::shared_ptr<const Menu> ServerState::information_menu_for_version(GameVersion version) {
  if ((version == GameVersion::DC) || (version == GameVersion::PC)) {
    return this->information_menu_v2;
  } else if ((version == GameVersion::GC) || (version == GameVersion::XB)) {
    return this->information_menu_v3;
  }
  throw out_of_range("no information menu exists for this version");
}

shared_ptr<const Menu> ServerState::proxy_destinations_menu_for_version(GameVersion version) {
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

const vector<pair<string, uint16_t>>& ServerState::proxy_destinations_for_version(GameVersion version) {
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

void ServerState::create_menus(shared_ptr<const JSONObject> config_json) {
  config_log.info("Creating menus");
  const auto& d = config_json->as_dict();

  shared_ptr<Menu> information_menu_v2(new Menu(MenuID::INFORMATION, u"Information"));
  shared_ptr<Menu> information_menu_v3(new Menu(MenuID::INFORMATION, u"Information"));
  shared_ptr<vector<u16string>> information_contents(new vector<u16string>());

  information_menu_v2->items.emplace_back(InformationMenuItemID::GO_BACK, u"Go back",
      u"Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  information_menu_v3->items.emplace_back(InformationMenuItemID::GO_BACK, u"Go back",
      u"Return to the\nmain menu", MenuItem::Flag::INVISIBLE_IN_INFO_MENU);
  {
    uint32_t item_id = 0;
    for (const auto& item : d.at("InformationMenuContents")->as_list()) {
      auto& v = item->as_list();
      information_menu_v2->items.emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
          decode_sjis(v.at(1)->as_string()), 0);
      information_menu_v3->items.emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
          decode_sjis(v.at(1)->as_string()), MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
      information_contents->emplace_back(decode_sjis(v.at(2)->as_string()));
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
      map<string, shared_ptr<JSONObject>> sorted_jsons;
      for (const auto& it : d.at(key)->as_dict()) {
        sorted_jsons.emplace(it.first, it.second);
      }

      ret->items.emplace_back(ProxyDestinationsMenuItemID::GO_BACK, u"Go back",
          u"Return to the\nmain menu", 0);
      ret->items.emplace_back(ProxyDestinationsMenuItemID::OPTIONS, u"Options",
          u"Set proxy session\noptions", 0);

      uint32_t item_id = 0;
      for (const auto& item : sorted_jsons) {
        const string& netloc_str = item.second->as_string();
        const string& description = "$C7Remote server:\n$C6" + netloc_str;
        ret->items.emplace_back(item_id, decode_sjis(item.first),
            decode_sjis(description), 0);
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
    const string& netloc_str = d.at("ProxyDestination-Patch")->as_string();
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
    const string& netloc_str = d.at("ProxyDestination-BB")->as_string();
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

  try {
    this->welcome_message = decode_sjis(d.at("WelcomeMessage")->as_string());
  } catch (const out_of_range&) {
  }
  try {
    this->pc_patch_server_message = decode_sjis(d.at("PCPatchServerMessage")->as_string());
  } catch (const out_of_range&) {
  }
  try {
    this->bb_patch_server_message = decode_sjis(d.at("BBPatchServerMessage")->as_string());
  } catch (const out_of_range&) {
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

shared_ptr<JSONObject> ServerState::load_config() const {
  config_log.info("Loading configuration");
  return JSONObject::parse(load_file(this->config_filename));
}

static vector<PortConfiguration> parse_port_configuration(
    shared_ptr<const JSONObject> json) {
  vector<PortConfiguration> ret;
  for (const auto& item_json_it : json->as_dict()) {
    auto item_list = item_json_it.second->as_list();
    PortConfiguration& pc = ret.emplace_back();
    pc.name = item_json_it.first;
    pc.port = item_list[0]->as_int();
    pc.version = version_for_name(item_list[1]->as_string().c_str());
    pc.behavior = server_behavior_for_name(item_list[2]->as_string().c_str());
  }
  return ret;
}

void ServerState::parse_config(shared_ptr<const JSONObject> config_json) {
  config_log.info("Parsing configuration");
  const auto& d = config_json->as_dict();

  this->name = decode_sjis(d.at("ServerName")->as_string());

  try {
    this->username = d.at("User")->as_string();
    if (this->username == "$SUDO_USER") {
      const char* user_from_env = getenv("SUDO_USER");
      if (!user_from_env) {
        throw runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
      }
      this->username = user_from_env;
    }
  } catch (const out_of_range&) {
  }

  this->set_port_configuration(parse_port_configuration(d.at("PortConfiguration")));

  auto local_address_str = d.at("LocalAddress")->as_string();
  try {
    this->local_address = this->all_addresses.at(local_address_str);
    string addr_str = string_for_address(this->local_address);
    config_log.info("Added local address: %s (%s)", addr_str.c_str(),
        local_address_str.c_str());
  } catch (const out_of_range&) {
    this->local_address = address_for_string(local_address_str.c_str());
    config_log.info("Added local address: %s", local_address_str.c_str());
  }
  this->all_addresses.emplace("<local>", this->local_address);

  auto external_address_str = d.at("ExternalAddress")->as_string();
  try {
    this->external_address = this->all_addresses.at(external_address_str);
    string addr_str = string_for_address(this->external_address);
    config_log.info("Added external address: %s (%s)", addr_str.c_str(),
        external_address_str.c_str());
  } catch (const out_of_range&) {
    this->external_address = address_for_string(external_address_str.c_str());
    config_log.info("Added external address: %s", external_address_str.c_str());
  }
  this->all_addresses.emplace("<external>", this->external_address);

  try {
    this->dns_server_port = d.at("DNSServerPort")->as_int();
  } catch (const out_of_range&) {
    this->dns_server_port = 0;
  }

  try {
    for (const auto& item : d.at("IPStackListen")->as_list()) {
      this->ip_stack_addresses.emplace_back(item->as_string());
    }
  } catch (const out_of_range&) {
  }
  try {
    this->ip_stack_debug = d.at("IPStackDebug")->as_bool();
  } catch (const out_of_range&) {
  }

  try {
    this->allow_unregistered_users = d.at("AllowUnregisteredUsers")->as_bool();
  } catch (const out_of_range&) {
    this->allow_unregistered_users = true;
  }

  try {
    this->item_tracking_enabled = d.at("EnableItemTracking")->as_bool();
  } catch (const out_of_range&) {
    this->item_tracking_enabled = true;
  }

  try {
    this->drops_enabled = d.at("EnableDrops")->as_bool();
  } catch (const out_of_range&) {
    this->drops_enabled = true;
  }

  try {
    this->episode_3_send_function_call_enabled = d.at("EnableEpisode3SendFunctionCall")->as_bool();
  } catch (const out_of_range&) {
    this->episode_3_send_function_call_enabled = false;
  }

  try {
    this->enable_dol_compression = d.at("CompressDOLFiles")->as_bool();
  } catch (const out_of_range&) {
    this->enable_dol_compression = false;
  }

  try {
    this->catch_handler_exceptions = d.at("CatchHandlerExceptions")->as_bool();
  } catch (const out_of_range&) {
    this->catch_handler_exceptions = true;
  }

  try {
    this->proxy_allow_save_files = d.at("ProxyAllowSaveFiles")->as_bool();
  } catch (const out_of_range&) {
    this->proxy_allow_save_files = true;
  }
  try {
    this->proxy_enable_login_options = d.at("ProxyEnableLoginOptions")->as_bool();
  } catch (const out_of_range&) {
    this->proxy_enable_login_options = false;
  }

  try {
    this->ep3_behavior_flags = d.at("Episode3BehaviorFlags")->as_int();
  } catch (const out_of_range&) {
    this->ep3_behavior_flags = 0;
  }

  try {
    this->ep3_card_auction_points = d.at("CardAuctionPoints")->as_int();
  } catch (const out_of_range&) {
    this->ep3_card_auction_points = 0;
  }
  try {
    auto i = d.at("CardAuctionSize");
    if (i->is_int()) {
      this->ep3_card_auction_min_size = i->as_int();
      this->ep3_card_auction_max_size = this->ep3_card_auction_min_size;
    } else {
      this->ep3_card_auction_min_size = i->as_list().at(0)->as_int();
      this->ep3_card_auction_max_size = i->as_list().at(1)->as_int();
    }
  } catch (const out_of_range&) {
    this->ep3_card_auction_min_size = 0;
    this->ep3_card_auction_max_size = 0;
  }

  try {
    for (const auto& it : d.at("CardAuctionPool")->as_dict()) {
      const auto& card_name = it.first;
      const auto& card_cfg_json = it.second->as_list();
      this->ep3_card_auction_pool.emplace(card_name, make_pair(card_cfg_json.at(0)->as_int(), card_cfg_json.at(1)->as_int()));
    }
  } catch (const out_of_range&) {
  }

  shared_ptr<JSONObject> log_levels_json;
  try {
    log_levels_json = d.at("LogLevels");
  } catch (const out_of_range&) {
  }
  if (log_levels_json.get()) {
    set_log_levels_from_json(log_levels_json);
  }

  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    this->bb_private_keys.emplace_back(new PSOBBEncryption::KeyFile(
        load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.info("Loaded Blue Burst key file: %s", filename.c_str());
  }
  config_log.info("%zu Blue Burst key file(s) loaded", this->bb_private_keys.size());

  try {
    bool run_shell = d.at("RunInteractiveShell")->as_bool();
    this->run_shell_behavior = run_shell ? ServerState::RunShellBehavior::ALWAYS : ServerState::RunShellBehavior::NEVER;
  } catch (const out_of_range&) {
  }

  try {
    const string& behavior = d.at("CheatModeBehavior")->as_string();
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

  try {
    auto v = d.at("LobbyEvent");
    uint8_t event = v->is_int() ? v->as_int() : event_for_name(v->as_string());
    this->pre_lobby_event = event;
    for (const auto& l : this->all_lobbies()) {
      l->event = event;
    }
  } catch (const out_of_range&) {
  }

  try {
    this->ep3_menu_song = d.at("Episode3MenuSong")->as_int();
  } catch (const out_of_range&) {
  }

  try {
    this->quest_category_index.reset(new QuestCategoryIndex(d.at("QuestCategories")));
  } catch (const exception& e) {
    throw runtime_error(string_printf(
        "QuestCategories is missing or invalid in config.json (%s) - see config.example.json for an example", e.what()));
  }
}

void ServerState::load_licenses() {
  config_log.info("Loading license list");
  this->license_manager.reset(new LicenseManager("system/licenses.nsi"));
  if (this->is_replay) {
    this->license_manager->set_autosave(false);
  }
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
  config_log.info("Loading rare item table");
  this->rare_item_set.reset(new RELRareItemSet(
      this->load_bb_file("ItemRT.rel")));

  // Note: These files don't exist in BB, so we use the GC versions of them
  // instead. This doesn't include Episode 4 of course, so we use Episode 1
  // parameters for Episode 4 implicitly.
  config_log.info("Loading common item tables");
  shared_ptr<string> pt_data(new string(load_file(
      "system/blueburst/ItemPT_GC.gsl")));
  this->common_item_set.reset(new CommonItemSet(pt_data));

  shared_ptr<string> armor_data(new string(load_file(
      "system/blueburst/ArmorRandom_GC.rel")));
  this->armor_random_set.reset(new ArmorRandomSet(armor_data));

  shared_ptr<string> tool_data(new string(load_file(
      "system/blueburst/ToolRandom_GC.rel")));
  this->tool_random_set.reset(new ToolRandomSet(tool_data));

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
  config_log.info("Collecting Episode 3 data");
  this->ep3_data_index.reset(new Episode3::DataIndex(
      "system/ep3", this->ep3_behavior_flags));

  const string& tournament_state_filename = "system/ep3/tournament-state.json";
  try {
    this->ep3_tournament_index.reset(new Episode3::TournamentIndex(
        this->ep3_data_index, tournament_state_filename));
    config_log.info("Loaded Episode 3 tournament state");
  } catch (const exception& e) {
    config_log.warning("Cannot load Episode 3 tournament state: %s", e.what());
    this->ep3_tournament_index.reset(new Episode3::TournamentIndex(
        this->ep3_data_index, tournament_state_filename, true));
  }
}

void ServerState::load_quest_index() {
  config_log.info("Collecting quest metadata");
  this->quest_index.reset(new QuestIndex("system/quests", this->quest_category_index));
}

void ServerState::compile_functions() {
  config_log.info("Compiling client functions");
  this->function_code_index.reset(new FunctionCodeIndex("system/ppc"));
}

void ServerState::load_dol_files() {
  config_log.info("Loading DOL files");
  this->dol_file_index.reset(new DOLFileIndex("system/dol", this->enable_dol_compression));
}
