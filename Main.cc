#include <signal.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <unordered_map>
#include <phosg/JSON.hh>
#include <phosg/Strings.hh>
#include <phosg/Filesystem.hh>
#include <set>
#include <thread>

#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "DNSServer.hh"
#include "ServerState.hh"
#include "Server.hh"
#include "FileContentsCache.hh"
#include "Text.hh"
#include "Shell.hh"

using namespace std;



FileContentsCache file_cache;



static const unordered_map<string, PortConfiguration> default_port_to_behavior({
  {"gc-jp10",  {9000,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-jp11",  {9001,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-jp3",   {9003,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-us10",  {9100,  GameVersion::PC,    ServerBehavior::SplitReconnect}},
  {"gc-us3",   {9103,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu10",  {9200,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu11",  {9201,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu3",   {9203,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"pc-login", {9300,  GameVersion::PC,    ServerBehavior::LoginServer}},
  {"pc-patch", {10000, GameVersion::Patch, ServerBehavior::PatchServer}},
  {"bb-patch", {11000, GameVersion::Patch, ServerBehavior::PatchServer}},
  {"bb-data",  {12000, GameVersion::BB,    ServerBehavior::DataServerBB}},

  // these aren't hardcoded in any games; user can override them
  {"bb-data1", {12004, GameVersion::BB,    ServerBehavior::DataServerBB}},
  {"bb-data2", {12005, GameVersion::BB,    ServerBehavior::DataServerBB}},
  {"bb-login", {12008, GameVersion::BB,    ServerBehavior::LoginServer}},
  {"pc-lobby", {9420,  GameVersion::PC,    ServerBehavior::LobbyServer}},
  {"gc-lobby", {9421,  GameVersion::GC,    ServerBehavior::LobbyServer}},
  {"bb-lobby", {9422,  GameVersion::BB,    ServerBehavior::LobbyServer}},
});



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

  // TODO: make this configurable
  s->port_configuration = default_port_to_behavior;

  auto enemy_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Enemy"));
  auto box_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Box"));
  vector<vector<uint8_t>> unit_types;
  for (const auto& item : d.at("CommonUnitTypes")->as_list()) {
    unit_types.emplace_back(parse_int_vector<uint8_t>(item));
  }
  s->common_item_creator.reset(new CommonItemCreator(enemy_categories,
      box_categories, unit_types));

  shared_ptr<vector<MenuItem>> information_menu(new vector<MenuItem>());
  shared_ptr<vector<u16string>> information_contents(new vector<u16string>());

  information_menu->emplace_back(INFORMATION_MENU_GO_BACK, u"Go back",
      u"Return to the\nmain menu", 0);

  uint32_t item_id = 0;
  for (const auto& item : d.at("InformationMenuContents")->as_list()) {
    auto& v = item->as_list();
    information_menu->emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
        decode_sjis(v.at(1)->as_string()), MenuItemFlag::RequiresMessageBoxes);
    information_contents->emplace_back(decode_sjis(v.at(2)->as_string()));
    item_id++;
  }
  s->information_menu = information_menu;
  s->information_contents = information_contents;

  s->num_threads = d.at("Threads")->as_int();
  if (s->num_threads == 0) {
    s->num_threads = thread::hardware_concurrency();
  }

  auto local_address_str = d.at("LocalAddress")->as_string();
  s->local_address = address_for_string(local_address_str.c_str());
  s->all_addresses.emplace(s->local_address);
  log(INFO, "added local address: %s", local_address_str.c_str());

  auto external_address_str = d.at("ExternalAddress")->as_string();
  s->external_address = address_for_string(external_address_str.c_str());
  s->all_addresses.emplace(s->external_address);
  log(INFO, "added external address: %s", external_address_str.c_str());

  try {
    s->run_dns_server = d.at("RunDNSServer")->as_bool();
  } catch (const out_of_range&) {
    s->run_dns_server = true;
  }

  try {
    bool run_shell = d.at("RunInteractiveShell")->as_bool();
    s->run_shell_behavior = run_shell ?
        ServerState::RunShellBehavior::Always :
        ServerState::RunShellBehavior::Never;
  } catch (const out_of_range&) { }
}



int main(int argc, char* argv[]) {
  log(INFO, "fuzziqer software newserv");

  signal(SIGPIPE, SIG_IGN);
  if (evthread_use_pthreads()) {
    log(ERROR, "cannot enable multithreading in libevent");
  }

  log(INFO, "creating server state");
  shared_ptr<ServerState> state(new ServerState());

  log(INFO, "reading network addresses");
  state->all_addresses = get_local_address_list();
  for (uint32_t addr : state->all_addresses) {
    string addr_str = string_for_address(addr);
    log(INFO, "found address: %s", addr_str.c_str());
  }

  log(INFO, "loading configuration");
  auto config_json = JSONObject::load("system/config.json");
  populate_state_from_config(state, config_json);

  log(INFO, "loading license list");
  state->license_manager.reset(new LicenseManager("system/licenses.nsi"));

  log(INFO, "loading battle parameters");
  state->battle_params.reset(new BattleParamTable("system/blueburst/BattleParamEntry"));

  log(INFO, "loading level table");
  state->level_table.reset(new LevelTable("system/blueburst/PlyLevelTbl.prs", true));

  log(INFO, "collecting quest metadata");
  state->quest_index.reset(new QuestIndex("system/quests"));

  shared_ptr<DNSServer> dns_server;
  if (state->run_dns_server) {
    log(INFO, "starting dns server on port 53");
    dns_server.reset(new DNSServer(state->local_address, state->external_address));
    dns_server->listen("", 53);
    dns_server->start();
  }

  log(INFO, "starting game server");
  shared_ptr<Server> game_server(new Server(state));
  for (const auto& it : state->port_configuration) {
    game_server->listen("", it.second.port, it.second.version, it.second.behavior);
  }
  game_server->start();

  bool should_run_shell = (state->run_shell_behavior == ServerState::RunShellBehavior::Always);
  if (state->run_shell_behavior == ServerState::RunShellBehavior::Default) {
    should_run_shell = isatty(fileno(stdin));
  }

  if (should_run_shell) {
    log(INFO, "starting interactive shell");
    run_shell(state);

  } else {
    for (;;) {
      sigset_t s;
      sigemptyset(&s);
      sigsuspend(&s);
    }
  }

  log(INFO, "waiting for servers to terminate");
  dns_server->schedule_stop();
  game_server->schedule_stop();
  dns_server->wait_for_stop();
  game_server->wait_for_stop();

  return 0;
}
