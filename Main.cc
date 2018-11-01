#include <arpa/inet.h>
#include <signal.h>

#include <unordered_map>
#include <phosg/JSON.hh>
#include <phosg/Strings.hh>
#include <set>

#include "NetworkAddresses.hh"
#include "DNSServer.hh"
#include "ServerState.hh"
#include "Server.hh"
#include "FileContentsCache.hh"
#include "Text.hh"

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

  s->name = d.at("ServerName")->as_string();

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
  shared_ptr<unordered_map<uint32_t, u16string>> id_to_information_contents(
      new unordered_map<uint32_t, u16string>());

  uint32_t item_id = 1;
  for (const auto& item : d.at("InformationMenuContents")->as_list()) {
    auto& v = item->as_list();
    information_menu->emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
        decode_sjis(v.at(1)->as_string()), MenuItemFlag::RequiresMessageBoxes);
    id_to_information_contents->emplace(item_id, decode_sjis(v.at(2)->as_string()));
    item_id++;
  }
  s->information_menu = information_menu;
  s->id_to_information_contents = id_to_information_contents;

  s->num_threads = d.at("Threads")->as_int();

  auto local_address_str = d.at("LocalAddress")->as_string();
  uint32_t local_address = inet_addr(local_address_str.c_str());
  if (s->all_addresses.emplace(local_address).second) {
    log(INFO, "added local address: %hhu.%hhu.%hhu.%hhu",
        static_cast<uint8_t>(local_address >> 24), static_cast<uint8_t>(local_address >> 16),
        static_cast<uint8_t>(local_address >> 8), static_cast<uint8_t>(local_address));
  }

  auto external_address_str = d.at("LocalAddress")->as_string();
  uint32_t external_address = inet_addr(external_address_str.c_str());
  if (s->all_addresses.emplace(external_address).second) {
    log(INFO, "added external address: %hhu.%hhu.%hhu.%hhu",
        static_cast<uint8_t>(external_address >> 24), static_cast<uint8_t>(external_address >> 16),
        static_cast<uint8_t>(external_address >> 8), static_cast<uint8_t>(external_address));
  }
}



int main(int argc,char* argv[]) {
  log(INFO, "fuzziqer software newserv");

  signal(SIGPIPE, SIG_IGN);

  log(INFO, "creating server state");
  shared_ptr<ServerState> state(new ServerState());

  log(INFO, "reading network addresses");
  state->all_addresses = get_local_address_list();
  for (uint32_t addr : state->all_addresses) {
    log(INFO, "found address: %hhu.%hhu.%hhu.%hhu",
        static_cast<uint8_t>(addr >> 24), static_cast<uint8_t>(addr >> 16),
        static_cast<uint8_t>(addr >> 8), static_cast<uint8_t>(addr));
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

  log(INFO, "starting dns server");
  DNSServer dns_server(state->local_address, state->external_address);
  // TODO: call dns_server.listen appropriately
  dns_server.start();

  log(INFO, "starting game server");
  Server game_server(state);
  // TODO: call game_server.listen appropriately
  game_server.start();

  for (;;) {
    sigset_t s;
    sigemptyset(&s);
    sigsuspend(&s);
  }

  log(INFO, "waiting for servers to terminate");
  dns_server.schedule_stop();
  game_server.schedule_stop();
  dns_server.wait_for_stop();
  game_server.wait_for_stop();

  return 0;
}

