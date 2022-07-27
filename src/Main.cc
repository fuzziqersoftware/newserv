#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <set>
#include <unordered_map>

#include "CatSession.hh"
#include "DNSServer.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "ProxyServer.hh"
#include "ReplaySession.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "ServerShell.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;



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

  {
    auto enemy_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Enemy"));
    auto box_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Box"));
    vector<vector<uint8_t>> unit_types;
    for (const auto& item : d.at("CommonUnitTypes")->as_list()) {
      unit_types.emplace_back(parse_int_vector<uint8_t>(item));
    }
    s->common_item_data.reset(new CommonItemData(
        move(enemy_categories), move(box_categories), move(unit_types)));
  }

  auto local_address_str = d.at("LocalAddress")->as_string();
  try {
    s->local_address = s->all_addresses.at(local_address_str);
    string addr_str = string_for_address(s->local_address);
    config_log.info("Added local address: %s (%s)", addr_str.c_str(),
        local_address_str.c_str());
  } catch (const out_of_range&) {
    s->local_address = address_for_string(local_address_str.c_str());
    config_log.info("Added local address: %s", local_address_str.c_str());
  }
  s->all_addresses.emplace("<local>", s->local_address);

  auto external_address_str = d.at("ExternalAddress")->as_string();
  try {
    s->external_address = s->all_addresses.at(external_address_str);
    string addr_str = string_for_address(s->external_address);
    config_log.info("Added external address: %s (%s)", addr_str.c_str(),
        external_address_str.c_str());
  } catch (const out_of_range&) {
    s->external_address = address_for_string(external_address_str.c_str());
    config_log.info("Added external address: %s", external_address_str.c_str());
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

  try {
    s->item_tracking_enabled = d.at("EnableItemTracking")->as_bool();
  } catch (const out_of_range&) {
    s->item_tracking_enabled = true;
  }

  shared_ptr<JSONObject> log_levels_json;
  try {
    log_levels_json = d.at("LogLevels");
  } catch (const out_of_range&) { }
  if (log_levels_json.get()) {
    set_log_levels_from_json(log_levels_json);
  }

  for (const string& filename : list_directory("system/blueburst/keys")) {
    if (!ends_with(filename, ".nsk")) {
      continue;
    }
    s->bb_private_keys.emplace_back(new PSOBBEncryption::KeyFile(
        load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + filename)));
    config_log.info("Loaded Blue Burst key file: %s", filename.c_str());
  }
  config_log.info("%zu Blue Burst key file(s) loaded", s->bb_private_keys.size());

  try {
    bool run_shell = d.at("RunInteractiveShell")->as_bool();
    s->run_shell_behavior = run_shell ?
        ServerState::RunShellBehavior::ALWAYS :
        ServerState::RunShellBehavior::NEVER;
  } catch (const out_of_range&) { }

  try {
    auto v = d.at("LobbyEvent");
    uint8_t event = v->is_int() ? v->as_int() : event_for_name(v->as_string());
    s->pre_lobby_event = event;
    for (const auto& l : s->all_lobbies()) {
      l->event = event;
    }
  } catch (const out_of_range&) { }

  try {
    s->ep3_menu_song = d.at("Episode3MenuSong")->as_int();
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
  config_log.info("Switched to user %s (%d:%d)",  username.c_str(), pw->pw_uid, pw->pw_gid);
}



void print_usage() {
  fputs("\
newserv - a Phantasy Star Online Swiss Army knife\n\
\n\
Usage:\n\
  newserv [options]\n\
\n\
With no options, newserv runs in server mode. PSO clients can connect normally,\n\
join lobbies, play games, and use the proxy server. See README.md and\n\
system/config.json for more information.\n\
\n\
When options are given, newserv will do things other than running the server.\n\
Specifically:\n\
  --decrypt-data\n\
  --encrypt-data\n\
      If either of these options is given, newserv will read from stdin and\n\
      write the encrypted or decrypted result to stdout. By default, PSO GC\n\
      encryption is used, but this can be overridden with --pc or --bb. The\n\
      --seed option specifies the ecryption seed (4 hex bytes for PC or GC, or\n\
      48 hex bytes for BB). For BB, the --key option is requires as well, and\n\
      should refer to a .nsk file in system/blueburst/keys (without the\n\
      directory or .nsk extension).\n\
  --decode-sjis\n\
      If this option is given, newserv applies its text decoding algorithm to\n\
      the data on stding, producing little-endian UTF-16 data on stdout.\n\
  --decode-gci=FILENAME\n\
  --decode-dlq=FILENAME\n\
  --decode-qst=FILENAME\n\
      If any of these options are given, newserv will decode the given quest\n\
      file into a compressed, unencrypted .bin or .dat file (or in the case of)\n\
      --decode-qst, both a .bin and a .dat file).\n\
  --cat-client=ADDR:PORT\n\
      If this option is given, newserv will behave as if it's a PSO client, and\n\
      will connect to the given server. It will then print all the received\n\
      commands to stdout, and forward any commands typed into stdin to the\n\
      remote server. It is assumed that the input and output are terminals, so\n\
      all commands are hex-encoded. The --patch, --dc, --pc, --gc, and --bb\n\
      options can be used to select the command format end encryption.\n\
  --replay-log=FILENAME\n\
      This option makes newserv replay terminal log as if it were a client\n\
      session. This is used for regression testing, to make sure client\n\
      sessions are repeatable and code changes don\'t affect existing (working)\n\
      functionality.\n\
\n\
A few options apply to multiple modes described above:\n\
  --parse-data\n\
      For modes that take input on stdin, parse the input as a hex string\n\
      before encrypting/decoding/etc.\n\
  --config=FILENAME\n\
      Use this file instead of system/config.json.\n\
", stderr);
}

enum class Behavior {
  RUN_SERVER = 0,
  DECRYPT_DATA,
  ENCRYPT_DATA,
  DECODE_QUEST_FILE,
  DECODE_SJIS,
  REPLAY_LOG,
  CAT_CLIENT,
};

enum class QuestFileFormat {
  GCI = 0,
  DLQ,
  QST,
};

int main(int argc, char** argv) {
  Behavior behavior = Behavior::RUN_SERVER;
  GameVersion cli_version = GameVersion::GC;
  QuestFileFormat quest_file_type = QuestFileFormat::GCI;
  string quest_filename;
  string seed;
  string key_file_name;
  const char* config_filename = "system/config.json";
  bool parse_data = false;
  bool byteswap_data = false;
  const char* replay_log_filename = nullptr;
  const char* replay_required_access_key = "";
  const char* replay_required_password = "";
  struct sockaddr_storage cat_client_remote;
  for (int x = 1; x < argc; x++) {
    if (!strcmp(argv[x], "--help")) {
      print_usage();
      return 0;
    } else if (!strcmp(argv[x], "--decrypt-data")) {
      behavior = Behavior::DECRYPT_DATA;
    } else if (!strcmp(argv[x], "--encrypt-data")) {
      behavior = Behavior::ENCRYPT_DATA;
    } else if (!strcmp(argv[x], "--decode-sjis")) {
      behavior = Behavior::DECODE_SJIS;
    } else if (!strncmp(argv[x], "--decode-gci=", 13)) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::GCI;
      quest_filename = &argv[x][13];
    } else if (!strncmp(argv[x], "--decode-dlq=", 13)) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::DLQ;
      quest_filename = &argv[x][13];
    } else if (!strncmp(argv[x], "--decode-qst=", 13)) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::QST;
      quest_filename = &argv[x][13];
    } else if (!strncmp(argv[x], "--cat-client=", 13)) {
      behavior = Behavior::CAT_CLIENT;
      cat_client_remote = make_sockaddr_storage(parse_netloc(&argv[x][13])).first;
    } else if (!strcmp(argv[x], "--patch")) {
      cli_version = GameVersion::PATCH;
    } else if (!strcmp(argv[x], "--dc")) {
      cli_version = GameVersion::DC;
    } else if (!strcmp(argv[x], "--pc")) {
      cli_version = GameVersion::PC;
    } else if (!strcmp(argv[x], "--gc")) {
      cli_version = GameVersion::GC;
    } else if (!strcmp(argv[x], "--xb")) {
      cli_version = GameVersion::XB;
    } else if (!strcmp(argv[x], "--bb")) {
      cli_version = GameVersion::BB;
    } else if (!strncmp(argv[x], "--seed=", 7)) {
      seed = &argv[x][7];
    } else if (!strncmp(argv[x], "--key=", 6)) {
      key_file_name = &argv[x][6];
    } else if (!strcmp(argv[x], "--parse-data")) {
      parse_data = true;
    } else if (!strcmp(argv[x], "--byteswap-data")) {
      byteswap_data = true;
    } else if (!strncmp(argv[x], "--replay-log=", 13)) {
      behavior = Behavior::REPLAY_LOG;
      replay_log_filename = &argv[x][13];
    } else if (!strncmp(argv[x], "--require-password=", 19)) {
      replay_required_password = &argv[x][19];
    } else if (!strncmp(argv[x], "--require-access-key=", 21)) {
      replay_required_access_key = &argv[x][21];
    } else if (!strncmp(argv[x], "--config=", 9)) {
      config_filename = &argv[x][9];
    } else {
      throw invalid_argument(string_printf("unknown option: %s", argv[x]));
    }
  }

  switch (behavior) {
    case Behavior::DECRYPT_DATA:
    case Behavior::ENCRYPT_DATA: {
      shared_ptr<PSOEncryption> crypt;
      switch (cli_version) {
        case GameVersion::PATCH:
        case GameVersion::DC:
        case GameVersion::PC:
          crypt.reset(new PSOV2Encryption(stoul(seed, nullptr, 16)));
          break;
        case GameVersion::GC:
        case GameVersion::XB:
          crypt.reset(new PSOV3Encryption(stoul(seed, nullptr, 16)));
          break;
        case GameVersion::BB: {
          seed = parse_data_string(seed);
          auto key = load_object_file<PSOBBEncryption::KeyFile>(
              "system/blueburst/keys/" + key_file_name + ".nsk");
          crypt.reset(new PSOBBEncryption(key, seed.data(), seed.size()));
          break;
        }
        default:
          throw logic_error("invalid game version");
      }

      string data = read_all(stdin);
      if (parse_data) {
        data = parse_data_string(data);
      }

      if (byteswap_data) {
        uint32_t* dwords = reinterpret_cast<uint32_t*>(data.data());
        for (size_t x = 0; x < (data.size() >> 2); x++) {
          dwords[x] = bswap32(dwords[x]);
        }
      }

      if (behavior == Behavior::DECRYPT_DATA) {
        crypt->decrypt(data.data(), data.size());
      } else if (behavior == Behavior::ENCRYPT_DATA) {
        crypt->encrypt(data.data(), data.size());
      } else {
        throw logic_error("invalid behavior");
      }

      if (byteswap_data) {
        uint32_t* dwords = reinterpret_cast<uint32_t*>(data.data());
        for (size_t x = 0; x < (data.size() >> 2); x++) {
          dwords[x] = bswap32(dwords[x]);
        }
      }

      if (isatty(fileno(stdout))) {
        print_data(stdout, data);
      } else {
        fwritex(stdout, data);
      }
      fflush(stdout);

      break;
    }

    case Behavior::DECODE_QUEST_FILE:
      if (quest_file_type == QuestFileFormat::GCI) {
        save_file(quest_filename + ".dec", Quest::decode_gci(quest_filename));
      } else if (quest_file_type == QuestFileFormat::DLQ) {
        save_file(quest_filename + ".dec", Quest::decode_dlq(quest_filename));
      } else if (quest_file_type == QuestFileFormat::QST) {
        auto data = Quest::decode_qst(quest_filename);
        save_file(quest_filename + ".bin", data.first);
        save_file(quest_filename + ".dat", data.second);
      } else {
        throw logic_error("invalid quest file format");
      }

      break;

    case Behavior::DECODE_SJIS: {
      string data = read_all(stdin);
      if (parse_data) {
        data = parse_data_string(data);
      }
      auto decoded = decode_sjis(data);
      print_data(stderr, decoded.data(), decoded.size() * sizeof(decoded[0]));
      break;
    }

    case Behavior::CAT_CLIENT: {
      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      CatSession session(base, cat_client_remote, cli_version);
      event_base_dispatch(base.get());
      break;
    }

    case Behavior::REPLAY_LOG:
    case Behavior::RUN_SERVER: {
      signal(SIGPIPE, SIG_IGN);

      if (isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      shared_ptr<ServerState> state(new ServerState());

      shared_ptr<struct event_base> base(event_base_new(), event_base_free);

      config_log.info("Reading network addresses");
      state->all_addresses = get_local_addresses();
      for (const auto& it : state->all_addresses) {
        string addr_str = string_for_address(it.second);
        config_log.info("Found interface: %s = %s", it.first.c_str(), addr_str.c_str());
      }

      config_log.info("Loading configuration");
      auto config_json = JSONObject::parse(load_file(config_filename));
      populate_state_from_config(state, config_json);

      if (!replay_log_filename) {
        config_log.info("Loading license list");
        state->license_manager.reset(new LicenseManager("system/licenses.nsi"));
      } else {
        state->license_manager.reset(new LicenseManager());
      }

      config_log.info("Loading battle parameters");
      state->battle_params.reset(new BattleParamTable("system/blueburst/BattleParamEntry"));

      config_log.info("Loading level table");
      state->level_table.reset(new LevelTable("system/blueburst/PlyLevelTbl.prs", true));

      config_log.info("Collecting Episode 3 data");
      state->ep3_data_index.reset(new Ep3DataIndex("system/ep3"));

      config_log.info("Collecting quest metadata");
      state->quest_index.reset(new QuestIndex("system/quests"));

      if (!replay_log_filename) {
        config_log.info("Compiling client functions");
        state->function_code_index.reset(new FunctionCodeIndex("system/ppc"));
        config_log.info("Loading DOL files");
        state->dol_file_index.reset(new DOLFileIndex("system/dol"));
      } else {
        state->function_code_index.reset(new FunctionCodeIndex());
        state->dol_file_index.reset(new DOLFileIndex());
      }

      config_log.info("Creating menus");
      state->create_menus(config_json);

      if (replay_log_filename) {
        state->allow_saving = false;
        state->license_manager->set_autosave(false);
        config_log.info("Saving disabled because this is a replay session");
      }

      shared_ptr<DNSServer> dns_server;
      if (state->dns_server_port && !replay_log_filename) {
        config_log.info("Starting DNS server");
        dns_server.reset(new DNSServer(base, state->local_address,
            state->external_address));
        dns_server->listen("", state->dns_server_port);
      } else {
        config_log.info("DNS server is disabled");
      }

      shared_ptr<Shell> shell;
      shared_ptr<ReplaySession> replay_session;
      shared_ptr<IPStackSimulator> ip_stack_simulator;
      if (behavior == Behavior::REPLAY_LOG) {
        config_log.info("Starting proxy server");
        state->proxy_server.reset(new ProxyServer(base, state));
        config_log.info("Starting game server");
        state->game_server.reset(new Server(base, state));

        auto f = fopen_unique(replay_log_filename, "rt");
        replay_session.reset(new ReplaySession(
            base, f.get(), state, replay_required_access_key, replay_required_password));
        replay_session->start();

      } else if (behavior == Behavior::RUN_SERVER) {
        config_log.info("Opening sockets");
        for (const auto& it : state->name_to_port_config) {
          const auto& pc = it.second;
          if (pc->behavior == ServerBehavior::PROXY_SERVER) {
            if (!state->proxy_server.get()) {
              config_log.info("Starting proxy server");
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
            if (!state->game_server.get()) {
              config_log.info("Starting game server");
              state->game_server.reset(new Server(base, state));
            }
            string spec = string_printf("T-%hu-%s-%s-%s",
                pc->port, name_for_version(pc->version), pc->name.c_str(),
                name_for_server_behavior(pc->behavior));
            state->game_server->listen(spec, "", pc->port, pc->version, pc->behavior);
          }
        }

        if (!state->ip_stack_addresses.empty()) {
          config_log.info("Starting IP stack simulator");
          ip_stack_simulator.reset(new IPStackSimulator(base, state));
          for (const auto& it : state->ip_stack_addresses) {
            auto netloc = parse_netloc(it);
            ip_stack_simulator->listen(netloc.first, netloc.second);
          }
        }

      } else {
        throw logic_error("invalid behavior");
      }

      if (!state->username.empty()) {
        config_log.info("Switching to user %s", state->username.c_str());
        drop_privileges(state->username);
      }

      bool should_run_shell;
      if (state->run_shell_behavior == ServerState::RunShellBehavior::DEFAULT) {
        should_run_shell = isatty(fileno(stdin));
      } else if (state->run_shell_behavior == ServerState::RunShellBehavior::ALWAYS) {
        should_run_shell = true;
      } else {
        should_run_shell = false;
      }
      if (should_run_shell) {
        should_run_shell = !replay_session.get();
      }
      if (should_run_shell) {
        shell.reset(new ServerShell(base, state));
      }

      config_log.info("Ready");
      event_base_dispatch(base.get());

      config_log.info("Normal shutdown");
      state->proxy_server.reset(); // Break reference cycle
      break;
    }

    default:
      throw logic_error("invalid behavior");
  }

  return 0;
}
