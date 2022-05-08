#include <signal.h>
#include <pwd.h>
#include <event2/event.h>
#include <string.h>

#include <unordered_map>
#include <phosg/JSON.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Filesystem.hh>
#include <set>

#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "DNSServer.hh"
#include "ProxyServer.hh"
#include "ServerState.hh"
#include "Server.hh"
#include "FileContentsCache.hh"
#include "Text.hh"
#include "ServerShell.hh"
#include "IPStackSimulator.hh"

using namespace std;



FileContentsCache file_cache;
bool use_terminal_colors = false;



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

template <typename T>
vector<T> parse_int_vector(shared_ptr<const JSONObject> o) {
  vector<T> ret;
  for (const auto& x : o->as_list()) {
    ret.emplace_back(x->as_int());
  }
  return ret;
}



void populate_state_from_config(shared_ptr<ServerState> s,
    shared_ptr<JSONObject> config_json) {
  const auto& d = config_json->as_dict();

  s->name = decode_sjis(d.at("ServerName")->as_string());

  try {
    s->username = d.at("User")->as_string();
    if (s->username == "$SUDO_USER") {
      const char* user_from_env = getenv("SUDO_USER");
      if (!user_from_env) {
        throw runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
      }
      s->username = user_from_env;
    }
  } catch (const out_of_range&) { }

  s->set_port_configuration(parse_port_configuration(d.at("PortConfiguration")));

  auto enemy_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Enemy"));
  auto box_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Box"));
  vector<vector<uint8_t>> unit_types;
  for (const auto& item : d.at("CommonUnitTypes")->as_list()) {
    unit_types.emplace_back(parse_int_vector<uint8_t>(item));
  }
  s->common_item_creator.reset(new CommonItemCreator(enemy_categories,
      box_categories, unit_types));

  shared_ptr<vector<MenuItem>> information_menu_pc(new vector<MenuItem>());
  shared_ptr<vector<MenuItem>> information_menu_gc(new vector<MenuItem>());
  shared_ptr<vector<u16string>> information_contents(new vector<u16string>());

  information_menu_gc->emplace_back(INFORMATION_MENU_GO_BACK, u"Go back",
      u"Return to the\nmain menu", 0);
  {
    uint32_t item_id = 0;
    for (const auto& item : d.at("InformationMenuContents")->as_list()) {
      auto& v = item->as_list();
      information_menu_pc->emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
          decode_sjis(v.at(1)->as_string()), 0);
      information_menu_gc->emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
          decode_sjis(v.at(1)->as_string()), MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
      information_contents->emplace_back(decode_sjis(v.at(2)->as_string()));
      item_id++;
    }
  }
  s->information_menu_pc = information_menu_pc;
  s->information_menu_gc = information_menu_gc;
  s->information_contents = information_contents;

  s->proxy_destinations_menu_pc.emplace_back(PROXY_DESTINATIONS_MENU_GO_BACK,
      u"Go back", u"Return to the\nmain menu", 0);
  s->proxy_destinations_menu_gc.emplace_back(PROXY_DESTINATIONS_MENU_GO_BACK,
      u"Go back", u"Return to the\nmain menu", 0);
  {
    uint32_t item_id = 0;
    for (const auto& item : d.at("ProxyDestinations-GC")->as_dict()) {
      const string& netloc_str = item.second->as_string();
      s->proxy_destinations_menu_gc.emplace_back(item_id, decode_sjis(item.first),
          decode_sjis(netloc_str), 0);
      s->proxy_destinations_gc.emplace_back(parse_netloc(netloc_str));
      item_id++;
    }
  }
  {
    uint32_t item_id = 0;
    for (const auto& item : d.at("ProxyDestinations-PC")->as_dict()) {
      const string& netloc_str = item.second->as_string();
      s->proxy_destinations_menu_pc.emplace_back(item_id, decode_sjis(item.first),
          decode_sjis(netloc_str), 0);
      s->proxy_destinations_pc.emplace_back(parse_netloc(netloc_str));
      item_id++;
    }
  }
  try {
    const string& netloc_str = d.at("ProxyDestination-Patch")->as_string();
    s->proxy_destination_patch = parse_netloc(netloc_str);
    log(INFO, "Patch server proxy is enabled with destination %s", netloc_str.c_str());
    for (auto& it : s->name_to_port_config) {
      if (it.second->version == GameVersion::PATCH) {
        it.second->behavior = ServerBehavior::PROXY_SERVER;
      }
    }
  } catch (const out_of_range&) {
    s->proxy_destination_patch.first = "";
    s->proxy_destination_patch.second = 0;
  }
  try {
    const string& netloc_str = d.at("ProxyDestination-BB")->as_string();
    s->proxy_destination_bb = parse_netloc(netloc_str);
    log(INFO, "BB proxy is enabled with destination %s", netloc_str.c_str());
    for (auto& it : s->name_to_port_config) {
      if (it.second->version == GameVersion::BB) {
        it.second->behavior = ServerBehavior::PROXY_SERVER;
      }
    }
  } catch (const out_of_range&) {
    s->proxy_destination_bb.first = "";
    s->proxy_destination_bb.second = 0;
  }

  s->main_menu.emplace_back(MAIN_MENU_GO_TO_LOBBY, u"Go to lobby",
      u"Join the lobby", 0);
  s->main_menu.emplace_back(MAIN_MENU_INFORMATION, u"Information",
      u"View server information", MenuItem::Flag::REQUIRES_MESSAGE_BOXES);
  if (!s->proxy_destinations_pc.empty()) {
    s->main_menu.emplace_back(MAIN_MENU_PROXY_DESTINATIONS, u"Proxy server",
        u"Connect to another\nserver", MenuItem::Flag::PC_ONLY);
  }
  if (!s->proxy_destinations_gc.empty()) {
    s->main_menu.emplace_back(MAIN_MENU_PROXY_DESTINATIONS, u"Proxy server",
        u"Connect to another\nserver", MenuItem::Flag::GC_ONLY);
  }
  s->main_menu.emplace_back(MAIN_MENU_DOWNLOAD_QUESTS, u"Download quests",
      u"Download quests", 0);
  s->main_menu.emplace_back(MAIN_MENU_DISCONNECT, u"Disconnect",
      u"Disconnect", 0);

  try {
    s->welcome_message = decode_sjis(d.at("WelcomeMessage")->as_string());
  } catch (const out_of_range&) { }

  auto local_address_str = d.at("LocalAddress")->as_string();
  try {
    s->local_address = s->all_addresses.at(local_address_str);
    string addr_str = string_for_address(s->local_address);
    log(INFO, "Added local address: %s (%s)", addr_str.c_str(),
        local_address_str.c_str());
  } catch (const out_of_range&) {
    s->local_address = address_for_string(local_address_str.c_str());
    log(INFO, "Added local address: %s", local_address_str.c_str());
  }
  s->all_addresses.emplace("<local>", s->local_address);

  auto external_address_str = d.at("ExternalAddress")->as_string();
  try {
    s->external_address = s->all_addresses.at(external_address_str);
    string addr_str = string_for_address(s->external_address);
    log(INFO, "Added external address: %s (%s)", addr_str.c_str(),
        external_address_str.c_str());
  } catch (const out_of_range&) {
    s->external_address = address_for_string(external_address_str.c_str());
    log(INFO, "Added external address: %s", external_address_str.c_str());
  }
  s->all_addresses.emplace("<external>", s->external_address);

  try {
    s->dns_server_port = d.at("DNSServerPort")->as_int();
  } catch (const out_of_range&) {
    s->dns_server_port = 0;
  }

  try {
    for (const auto& item : d.at("IPStackListen")->as_list()) {
      s->ip_stack_addresses.emplace_back(item->as_string());
    }
  } catch (const out_of_range&) { }
  try {
    s->ip_stack_debug = d.at("IPStackDebug")->as_bool();
  } catch (const out_of_range&) { }

  try {
    s->allow_unregistered_users = d.at("AllowUnregisteredUsers")->as_bool();
  } catch (const out_of_range&) {
    s->allow_unregistered_users = true;
  }

  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    string contents = load_file("system/blueburst/keys/" + filename);
    if (contents.size() != sizeof(PSOBBEncryption::KeyFile)) {
      log(WARNING, "Blue Burst key file %s is the wrong size (%zu bytes; should be %zu bytes)",
          filename.c_str(), contents.size(), sizeof(PSOBBEncryption::KeyFile));
    } else {
      shared_ptr<PSOBBEncryption::KeyFile> k(new PSOBBEncryption::KeyFile());
      memcpy(k.get(), contents.data(), sizeof(PSOBBEncryption::KeyFile));
      s->bb_private_keys.emplace_back(k);
      log(INFO, "Loaded Blue Burst key file: %s", filename.c_str());
    }
  }
  log(INFO, "%zu Blue Burst key file(s) loaded", s->bb_private_keys.size());

  try {
    bool run_shell = d.at("RunInteractiveShell")->as_bool();
    s->run_shell_behavior = run_shell ?
        ServerState::RunShellBehavior::ALWAYS :
        ServerState::RunShellBehavior::NEVER;
  } catch (const out_of_range&) { }
}



void drop_privileges(const string& username) {
  if ((getuid() != 0) || (getgid() != 0)) {
    throw runtime_error(string_printf(
        "newserv was not started as root; can\'t switch to user %s",
        username.c_str()));
  }

  struct passwd* pw = getpwnam(username.c_str());
  if (!pw) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("user %s not found (%s)",
        username.c_str(), error.c_str()));
  }

  if (setgid(pw->pw_gid) != 0) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("can\'t switch to group %d (%s)",
        pw->pw_gid, error.c_str()));
  }
  if (setuid(pw->pw_uid) != 0) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("can\'t switch to user %d (%s)",
        pw->pw_uid, error.c_str()));
  }
  log(INFO, "Switched to user %s (%d:%d)",  username.c_str(), pw->pw_uid,
      pw->pw_gid);
}



int main(int, char**) {
  if (true) {
    static const string seed("\x33\xF7\x40\xA2\xB4\xFD\x5C\x07\xD8\x94\x09\x9F\x8B\x76\x35\xF9\x97\x76\x8B\x16\x5C\x73\x9F\x2E\xF1\x1F\x1A\xC0\xB9\x53\xFE\x59\xE4\xDD\xC5\xC8\x11\xA0\x78\xD5\x56\x5A\xF7\xC3\x47\xD5\xCA\x67", 0x30);
    static const string encrypted_data("\x83\x9A\xE7\xE1\xDD\xB2\x41\x38", 0x08);
    string decrypted_data = encrypted_data;
    auto private_key = load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/default.nsk");
    PSOBBEncryption crypt(private_key, seed.data(), seed.size());
    crypt.decrypt(decrypted_data.data(), decrypted_data.size());
    fprintf(stderr, "decrypted\n");
    print_data(stderr, decrypted_data);
  }

  signal(SIGPIPE, SIG_IGN);

  if (isatty(fileno(stderr))) {
    use_terminal_colors = true;
  }

  shared_ptr<ServerState> state(new ServerState());

  shared_ptr<struct event_base> base(event_base_new(), event_base_free);

  log(INFO, "Reading network addresses");
  state->all_addresses = get_local_addresses();
  for (const auto& it : state->all_addresses) {
    string addr_str = string_for_address(it.second);
    log(INFO, "Found interface: %s = %s", it.first.c_str(), addr_str.c_str());
  }

  log(INFO, "Loading configuration");
  auto config_json = JSONObject::parse(load_file("system/config.json"));
  populate_state_from_config(state, config_json);

  log(INFO, "Loading license list");
  state->license_manager.reset(new LicenseManager("system/licenses.nsi"));

  log(INFO, "Loading battle parameters");
  state->battle_params.reset(new BattleParamTable("system/blueburst/BattleParamEntry"));

  log(INFO, "Loading level table");
  state->level_table.reset(new LevelTable("system/blueburst/PlyLevelTbl.prs", true));

  log(INFO, "Collecting quest metadata");
  state->quest_index.reset(new QuestIndex("system/quests"));

  shared_ptr<DNSServer> dns_server;
  if (state->dns_server_port) {
    log(INFO, "Starting DNS server");
    dns_server.reset(new DNSServer(base, state->local_address,
        state->external_address));
    dns_server->listen("", state->dns_server_port);
  } else {
    log(INFO, "DNS server is disabled");
  }

  shared_ptr<Server> game_server;

  log(INFO, "Opening sockets");
  for (const auto& it : state->name_to_port_config) {
    const auto& pc = it.second;
    if (pc->behavior == ServerBehavior::PROXY_SERVER) {
      if (!state->proxy_server.get()) {
        log(INFO, "Starting proxy server");
        state->proxy_server.reset(new ProxyServer(base, state));
      }
      if (state->proxy_server.get()) {
        // For PC and GC, proxy sessions are dynamically created when a client
        // picks a destination from the menu. For patch and BB clients, there's
        // no way to ask the client which destination they want, so only one
        // destination is supported, and we have to manually specify the
        // destination netloc here.
        if (pc->version == GameVersion::PATCH) {
          struct sockaddr_storage ss = make_sockaddr_storage(
              state->proxy_destination_patch.first,
              state->proxy_destination_patch.second).first;
          state->proxy_server->listen(pc->port, pc->version, &ss);
        } else if (pc->version == GameVersion::BB) {
          struct sockaddr_storage ss = make_sockaddr_storage(
              state->proxy_destination_bb.first,
              state->proxy_destination_bb.second).first;
          state->proxy_server->listen(pc->port, pc->version, &ss);
        } else {
          state->proxy_server->listen(pc->port, pc->version);
        }
      }
    } else {
      if (!game_server.get()) {
        log(INFO, "Starting game server");
        game_server.reset(new Server(base, state));
      }
      string name = string_printf("%s (%s, %s) on port %hu",
          pc->name.c_str(), name_for_version(pc->version),
          name_for_server_behavior(pc->behavior), pc->port);
      game_server->listen(name, "", pc->port, pc->version, pc->behavior);
    }
  }

  shared_ptr<IPStackSimulator> ip_stack_simulator;
  if (!state->ip_stack_addresses.empty()) {
    log(INFO, "Starting IP stack simulator");
    ip_stack_simulator.reset(new IPStackSimulator(
        base, game_server, state));
    for (const auto& it : state->ip_stack_addresses) {
      auto netloc = parse_netloc(it);
      ip_stack_simulator->listen(netloc.first, netloc.second);
    }
  }

  if (!state->username.empty()) {
    log(INFO, "Switching to user %s", state->username.c_str());
    drop_privileges(state->username);
  }

  bool should_run_shell = (state->run_shell_behavior == ServerState::RunShellBehavior::ALWAYS);
  if (state->run_shell_behavior == ServerState::RunShellBehavior::DEFAULT) {
    should_run_shell = isatty(fileno(stdin));
  }

  shared_ptr<Shell> shell;
  if (should_run_shell) {
    shell.reset(new ServerShell(base, state));
  }

  log(INFO, "Ready");
  event_base_dispatch(base.get());

  log(INFO, "Normal shutdown");
  state->proxy_server.reset(); // Break reference cycle
  return 0;
}
