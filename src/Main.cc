#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <set>
#include <thread>
#include <unordered_map>

#include "CatSession.hh"
#include "Compression.hh"
#include "DNSServer.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "ProxyServer.hh"
#include "PSOEncryptionSeedFinder.hh"
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
  --compress-data\n\
  --decompress-data\n\
      Compress or decompress data using the PRS algorithm.\n\
  --decrypt-data\n\
  --encrypt-data\n\
      Read from stdin, encrypt or decrypt the data, and write the result to\n\
      stdout. By default, PSO V3 encryption is used, but this can be overridden\n\
      with --pc or --bb. The --seed option specifies the encryption seed (4 hex\n\
      bytes for PC or GC, or 48 hex bytes for BB). For BB, the --key option is\n\
      required as well, and refers to a .nsk file in system/blueburst/keys\n\
      (without the directory or .nsk extension). For non-BB ciphers, the\n\
      --big-endian option applies the cipher masks as big-endian instead of\n\
      little-endian, which is necessary for some GameCube file formats.\n\
  --find-decryption-seed\n\
      Perform a brute-force search for a decryption seed of the given data.\n\
      The ciphertext is specified with the --encrypted= option and the expected\n\
      plaintext is specified with the --decrypted= option. The plaintext may\n\
      include unmatched bytes (specified with the ? operator), but overall it\n\
      must be the same length as the ciphertext. By default, this option uses\n\
      PSO V3 encryption, but this can be overridden with --pc. (BB encryption\n\
      seeds are too long to be searched for with this function.) By default,\n\
      the number of worker threads is equal the the number of CPU cores in the\n\
      system, but this can be overridden with the --threads= option. To use a\n\
      rainbow table instead of computing the cipherstreams inline, use the\n\
      --rainbow-table=FILENAME option.\n\
  --generate-rainbow-table=FILENAME\n\
      Generate a decryption table for V3 encryption (or V2 if --pc is given).\n\
      The --match-length= option must be given, which specifies the match\n\
      length for the table. The total table size is the match length * 4 GB.\n\
      As for --encrypt-data, the --big-endian option specifies that the table\n\
      uses big-endian encryption. As for --find-decryption-seed, the --threads\n\
      option specifies the parallelism for generating the table.\n\
  --decode-sjis\n\
      Apply newserv\'s text decoding algorithm to the data on stdin, producing\n\
      little-endian UTF-16 data on stdout.\n\
  --decode-gci=FILENAME\n\
  --decode-dlq=FILENAME\n\
  --decode-qst=FILENAME\n\
      Decode the given quest file into a compressed, unencrypted .bin or .dat\n\
      file (or in the case of --decode-qst, both a .bin and a .dat file). The\n\
      --decode-gci option can be used to decrypt encrypted GCI files. If you\n\
      know the player\'s serial number who generated the GCI file, use the\n\
      --seed= option and give the serial number (as a hex-encoded integer). If\n\
      you don\'t know the serial number, newserv will find it via a brute-force\n\
      search, but this will likely take a long time.\n\
  --cat-client=ADDR:PORT\n\
      Connect to the given server and simulate a PSO client. newserv will then\n\
      print all the received commands to stdout, and forward any commands typed\n\
      into stdin to the remote server. It is assumed that the input and output\n\
      are terminals, so all commands are hex-encoded. The --patch, --dc, --pc,\n\
      --gc, and --bb options can be used to select the command format and\n\
      encryption. If --bb is used, the --key option is also required (as in\n\
      --decrypt-data above).\n\
  --replay-log=FILENAME\n\
      Replay a terminal log as if it were a client session. This is used for\n\
      regression testing, to make sure client sessions are repeatable and code\n\
      changes don\'t affect existing (working) functionality.\n\
  --extract-gsl=FILENAME\n\
      Extract all files from a GSL archive into the current directory.\n\
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
  DECOMPRESS_DATA,
  COMPRESS_DATA,
  DECRYPT_DATA,
  ENCRYPT_DATA,
  FIND_DECRYPTION_SEED,
  GENERATE_RAINBOW_TABLE,
  DECODE_QUEST_FILE,
  DECODE_SJIS,
  EXTRACT_GSL,
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
  string rainbow_table_filename;
  bool parse_data = false;
  bool big_endian = false;
  bool skip_little_endian = false;
  bool skip_big_endian = false;
  size_t num_threads = 0;
  size_t match_length = 0;
  const char* find_decryption_seed_ciphertext = nullptr;
  vector<const char*> find_decryption_seed_plaintexts;
  const char* replay_log_filename = nullptr;
  const char* extract_gsl_filename = nullptr;
  const char* replay_required_access_key = "";
  const char* replay_required_password = "";
  struct sockaddr_storage cat_client_remote;
  for (int x = 1; x < argc; x++) {
    if (!strcmp(argv[x], "--help")) {
      print_usage();
      return 0;
    } else if (!strcmp(argv[x], "--decompress-data")) {
      behavior = Behavior::DECOMPRESS_DATA;
    } else if (!strcmp(argv[x], "--compress-data")) {
      behavior = Behavior::COMPRESS_DATA;
    } else if (!strcmp(argv[x], "--decrypt-data")) {
      behavior = Behavior::DECRYPT_DATA;
    } else if (!strcmp(argv[x], "--encrypt-data")) {
      behavior = Behavior::ENCRYPT_DATA;
    } else if (!strcmp(argv[x], "--find-decryption-seed")) {
      behavior = Behavior::FIND_DECRYPTION_SEED;
    } else if (!strncmp(argv[x], "--generate-rainbow-table=", 25)) {
      behavior = Behavior::GENERATE_RAINBOW_TABLE;
      rainbow_table_filename = &argv[x][25];
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
    } else if (!strncmp(argv[x], "--threads=", 10)) {
      num_threads = strtoull(&argv[x][13], nullptr, 0);
    } else if (!strncmp(argv[x], "--match-length=", 15)) {
      match_length = strtoull(&argv[x][15], nullptr, 0);
    } else if (!strncmp(argv[x], "--rainbow-table=", 16)) {
      rainbow_table_filename = &argv[x][16];
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
    } else if (!strncmp(argv[x], "--encrypted=", 12)) {
      find_decryption_seed_ciphertext = &argv[x][12];
    } else if (!strncmp(argv[x], "--decrypted=", 12)) {
      find_decryption_seed_plaintexts.emplace_back(&argv[x][12]);
    } else if (!strcmp(argv[x], "--parse-data")) {
      parse_data = true;
    } else if (!strcmp(argv[x], "--big-endian")) {
      big_endian = true;
    } else if (!strcmp(argv[x], "--skip-little-endian")) {
      skip_little_endian = true;
    } else if (!strcmp(argv[x], "--skip-big-endian")) {
      skip_big_endian = true;
    } else if (!strncmp(argv[x], "--replay-log=", 13)) {
      behavior = Behavior::REPLAY_LOG;
      replay_log_filename = &argv[x][13];
    } else if (!strncmp(argv[x], "--extract-gsl=", 14)) {
      behavior = Behavior::EXTRACT_GSL;
      extract_gsl_filename = &argv[x][14];
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
    case Behavior::DECOMPRESS_DATA:
    case Behavior::COMPRESS_DATA: {
      string data = read_all(stdin);
      if (parse_data) {
        data = parse_data_string(data);
      }

      if (behavior == Behavior::DECOMPRESS_DATA) {
        data = prs_decompress(data);
      } else if (behavior == Behavior::COMPRESS_DATA) {
        data = prs_compress(data);
      } else {
        throw logic_error("invalid behavior");
      }

      if (isatty(fileno(stdout))) {
        print_data(stdout, data);
      } else {
        fwritex(stdout, data);
      }
      fflush(stdout);

      break;
    }

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

      if (big_endian) {
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

      if (big_endian) {
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

    case Behavior::FIND_DECRYPTION_SEED: {
      if (find_decryption_seed_plaintexts.empty() || !find_decryption_seed_ciphertext) {
        throw runtime_error("both --encrypted and --decrypted must be specified");
      }
      if (cli_version == GameVersion::BB) {
        throw runtime_error("--find-decryption-seed cannot be used for BB ciphers");
      }

      vector<pair<string, string>> plaintexts;
      for (const auto& plaintext_ascii : find_decryption_seed_plaintexts) {
        string mask;
        string data = parse_data_string(plaintext_ascii, &mask);
        plaintexts.emplace_back(move(data), move(mask));
      }
      string ciphertext = parse_data_string(find_decryption_seed_ciphertext);

      if (num_threads == 0) {
        num_threads = thread::hardware_concurrency();
      }

      PSOEncryptionSeedFinder finder(ciphertext, plaintexts, num_threads);
      PSOEncryptionSeedFinder::ThreadResults results;
      if (!rainbow_table_filename.empty()) {
        results = finder.find_seed(rainbow_table_filename);
      } else {
        using Flag = PSOEncryptionSeedFinder::Flag;
        uint64_t flags =
            (((cli_version == GameVersion::GC) || (cli_version == GameVersion::XB)) ? Flag::V3 : 0) |
            (skip_little_endian ? Flag::SKIP_LITTLE_ENDIAN : 0) |
            (skip_big_endian ? Flag::SKIP_BIG_ENDIAN : 0);
        results = finder.find_seed(flags);
      }

      log_info("Minimum differences: %zu", results.min_differences);
      for (auto result : results.results) {
        if (result.differences != results.min_differences) {
          throw logic_error("incorrect difference count in result");
        }
        if (result.is_indeterminate) {
          log_info("Example match: %08" PRIX32 " (%zu)",
              result.seed, result.differences);
        } else {
          log_info("Example match: %08" PRIX32 " (%zu; %s, %s)",
              result.seed,
              result.differences,
              result.is_v3 ? "v3" : "v2",
              result.is_big_endian ? "big-endian" : "little-endian");
        }
      }
      for (size_t z = 0; z < results.difference_histogram.size(); z++) {
        log_info("(Difference histogram) %zu => %zu results",
            z, results.difference_histogram[z]);
      }
      break;
    }

    case Behavior::GENERATE_RAINBOW_TABLE: {
      if (num_threads == 0) {
        num_threads = thread::hardware_concurrency();
      }
      bool is_v3 = ((cli_version == GameVersion::GC) || (cli_version == GameVersion::XB));
      PSOEncryptionSeedFinder::generate_rainbow_table(
          rainbow_table_filename, is_v3, big_endian, match_length, num_threads);
      break;
    }

    case Behavior::DECODE_QUEST_FILE:
      if (quest_file_type == QuestFileFormat::GCI) {
        int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
        save_file(quest_filename + ".dec", Quest::decode_gci(quest_filename, num_threads, dec_seed));
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

    case Behavior::EXTRACT_GSL: {
      shared_ptr<string> data(new string(load_file(extract_gsl_filename)));
      GSLArchive gsl(data);
      for (const auto& entry_it : gsl.all_entries()) {
        auto e = gsl.get(entry_it.first);
        save_file(entry_it.first, e.first, e.second);
        fprintf(stderr, "... %s\n", entry_it.first.c_str());
      }
      break;
    }

    case Behavior::CAT_CLIENT: {
      shared_ptr<PSOBBEncryption::KeyFile> key;
      if (cli_version == GameVersion::BB) {
        if (key_file_name.empty()) {
          throw runtime_error("a key filename is required for BB client emulation");
        }
        key.reset(new PSOBBEncryption::KeyFile(
            load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + key_file_name + ".nsk")));
      }
      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      CatSession session(base, cat_client_remote, cli_version, key);
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

      if (isdir("system/patch-pc")) {
        config_log.info("Indexing PSO PC patch files");
        state->pc_patch_file_index.reset(new PatchFileIndex("system/patch-pc"));
      } else {
        config_log.info("PSO PC patch files not present");
      }
      if (isdir("system/patch-bb")) {
        config_log.info("Indexing PSO BB patch files");
        state->bb_patch_file_index.reset(new PatchFileIndex("system/patch-bb"));
        try {
          auto gsl_file = state->bb_patch_file_index->get("./data/data.gsl");
          state->bb_data_gsl.reset(new GSLArchive(gsl_file->load_data()));
          config_log.info("data.gsl found in BB patch files");
        } catch (const out_of_range&) {
          config_log.info("data.gsl is not present in BB patch files");
        }
      } else {
        config_log.info("PSO BB patch files not present");
      }

      config_log.info("Loading battle parameters");
      state->battle_params.reset(new BattleParamsIndex(
          state->load_bb_file("BattleParamEntry_on.dat"),
          state->load_bb_file("BattleParamEntry_lab_on.dat"),
          state->load_bb_file("BattleParamEntry_ep4_on.dat"),
          state->load_bb_file("BattleParamEntry.dat"),
          state->load_bb_file("BattleParamEntry_lab.dat"),
          state->load_bb_file("BattleParamEntry_ep4.dat")));

      config_log.info("Loading level table");
      state->level_table.reset(new LevelTable(
          state->load_bb_file("PlyLevelTbl.prs"), true));

      config_log.info("Loading rare table");
      state->rare_item_set.reset(new RareItemSet(
          state->load_bb_file("ItemRT.rel")));

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
