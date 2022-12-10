#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
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
#include "PSOGCObjectGraph.hh"
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

  try {
    s->episode_3_send_function_call_enabled = d.at("EnableEpisode3SendFunctionCall")->as_bool();
  } catch (const out_of_range&) {
    s->episode_3_send_function_call_enabled = false;
  }

  try {
    s->catch_handler_exceptions = d.at("CatchHandlerExceptions")->as_bool();
  } catch (const out_of_range&) {
    s->catch_handler_exceptions = true;
  }

  try {
    s->ep3_behavior_flags = d.at("Episode3BehaviorFlags")->as_int();
  } catch (const out_of_range&) {
    s->ep3_behavior_flags = 0;
  }

  try {
    s->ep3_card_auction_points = d.at("CardAuctionPoints")->as_int();
  } catch (const out_of_range&) {
    s->ep3_card_auction_points = 0;
  }
  try {
    auto i = d.at("CardAuctionSize");
    if (i->is_int()) {
      s->ep3_card_auction_min_size = i->as_int();
      s->ep3_card_auction_max_size = s->ep3_card_auction_min_size;
    } else {
      s->ep3_card_auction_min_size = i->as_list().at(0)->as_int();
      s->ep3_card_auction_max_size = i->as_list().at(1)->as_int();
    }
  } catch (const out_of_range&) {
    s->ep3_card_auction_min_size = 0;
    s->ep3_card_auction_max_size = 0;
  }

  try {
    for (const auto& it : d.at("CardAuctionPool")->as_dict()) {
      const auto& card_name = it.first;
      const auto& card_cfg_json = it.second->as_list();
      s->ep3_card_auction_pool.emplace(card_name, make_pair(
          card_cfg_json.at(0)->as_int(), card_cfg_json.at(1)->as_int()));
    }
  } catch (const out_of_range&) { }

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
  newserv [options] [input-filename [output-filename]]\n\
\n\
With no options, newserv runs in server mode. PSO clients can connect normally,\n\
join lobbies, play games, and use the proxy server. See README.md and\n\
system/config.json for more information.\n\
\n\
When options are given, newserv will do things other than running the server.\n\
\n\
Some modes accept input and/or output filenames; see the descriptions below for\n\
details. If input-filename is missing or is '-', newserv reads from stdin;\n\
similarly, if output-filename is missing or is '-', newserv writes to stdout.\n\
\n\
The options are:\n\
  --compress-prs\n\
  --decompress-prs\n\
  --compress-bc0 [input-filename [output-filename]]\n\
  --decompress-bc0 [input-filename [output-filename]]\n\
      Compress or decompress data using the PRS or BC0 algorithms. Both\n\
      input-filename and output-filename may be specified.\n\
  --prs-size\n\
      Compute the decompressed size of the PRS-compressed input data.\n\
  --encrypt-data\n\
  --decrypt-data\n\
      Encrypt or decrypt data using PSO's standard network protocol encryption.\n\
      Both input-filename and output-filename may be specified. By default, PSO\n\
      V3 (GameCube/XBOX) encryption is used, but this can be overridden with\n\
      the --pc or --bb options. The --seed= option specifies the encryption\n\
      seed (4 hex bytes for PC or GC, or 48 hex bytes for BB). For BB, the\n\
      --key option is required as well, and refers to a .nsk file in\n\
      system/blueburst/keys (without the directory or .nsk extension). For\n\
      non-BB ciphers, the --big-endian option applies the cipher masks as\n\
      big-endian instead of little-endian, which is necessary for some GameCube\n\
      file formats.\n\
  --decrypt-trivial-data\n\
      Decrypt (or encrypt - the algorithm is symmetric) data using the Episode\n\
      3 trivial algorithm. --seed should be specified as one hex byte. If\n\
      --seed is not given, newserv will truy all possible seeds and return the\n\
      one that results in the greatest number of zero bytes in the output.\n\
  --find-decryption-seed\n\
      Perform a brute-force search for a decryption seed of the given data.\n\
      The ciphertext is specified with the --encrypted= option and the expected\n\
      plaintext is specified with the --decrypted= option. The plaintext may\n\
      include unmatched bytes (specified with the ? operator), but overall it\n\
      must be the same length as the ciphertext. By default, this option uses\n\
      PSO V3 encryption, but this can be overridden with --pc. (BB encryption\n\
      seeds are too long to be searched for with this function.) By default,\n\
      the number of worker threads is equal the the number of CPU cores in the\n\
      system, but this can be overridden with the --threads= option.\n\
  --decode-sjis\n\
      Apply newserv\'s text decoding algorithm to the data on stdin, producing\n\
      little-endian UTF-16 data on stdout. Both input-filename and\n\
      output-filename may be specified.\n\
  --decode-gci\n\
  --decode-dlq\n\
  --decode-qst\n\
      Decode the given quest file into a compressed, unencrypted .bin or .dat\n\
      file (or in the case of --decode-qst, both a .bin and a .dat file).\n\
      input-filename must be specified, but output-filename msut not be; the\n\
      output is written to <input-filename>.dec (or .bin, or .dat). DLQ and QST\n\
      decoding is a relatively simple operation, but GCI decoding can be\n\
      computationally expensive if the file is encrypted and doesn't contain an\n\
      embedded seed. If you know the player\'s serial number who generated the\n\
      GCI file, use the --seed= option and give the serial number (as a\n\
      hex-encoded 32-bit integer). If you don\'t know the serial number, newserv\n\
      will find it via a brute-force search, but this will take a long time.\n\
  --cat-client=ADDR:PORT\n\
      Connect to the given server and simulate a PSO client. newserv will then\n\
      print all the received commands to stdout, and forward any commands typed\n\
      into stdin to the remote server. It is assumed that the input and output\n\
      are terminals, so all commands are hex-encoded. The --patch, --dc, --pc,\n\
      --gc, and --bb options can be used to select the command format and\n\
      encryption. If --bb is used, the --key option is also required (as in\n\
      --decrypt-data above).\n\
  --show-ep3-data\n\
      Print the Episode 3 data files (maps and card definitions) from the\n\
      system/ep3 directory in a human-readable format.\n\
  --show-ep3-card=ID\n\
      Describe the Episode 3 card with the given ID.\n\
  --replay-log\n\
      Replay a terminal log as if it were a client session. input-filename may\n\
      be specified for this option. This is used for regression testing, to\n\
      make sure client sessions are repeatable and code changes don\'t affect\n\
      existing (working) functionality.\n\
  --extract-gsl\n\
      Extract all files from a GSL archive into the current directory.\n\
      input-filename may be specified. If output-filename is specified, then it\n\
      is treated as a prefix which is prepended to the filename of each file\n\
      contained in the GSL archive. Importantly, if you want to put the files\n\
      into a directory, you'll have to create the directory first, and include\n\
      a trailing / on output-filename.\n\
\n\
A few options apply to multiple modes described above:\n\
  --parse-data\n\
      For modes that take input (from a file or from stdin), parse the input as\n\
      a hex string before encrypting/decoding/etc.\n\
  --config=FILENAME\n\
      Use this file instead of system/config.json.\n\
", stderr);
}

enum class Behavior {
  RUN_SERVER = 0,
  COMPRESS_PRS,
  DECOMPRESS_PRS,
  COMPRESS_BC0,
  DECOMPRESS_BC0,
  PRS_SIZE,
  ENCRYPT_DATA,
  DECRYPT_DATA,
  DECRYPT_TRIVIAL_DATA,
  FIND_DECRYPTION_SEED,
  DECODE_QUEST_FILE,
  DECODE_SJIS,
  EXTRACT_GSL,
  SHOW_EP3_DATA,
  PARSE_OBJECT_GRAPH,
  REPLAY_LOG,
  CAT_CLIENT,
};

static bool behavior_takes_input_filename(Behavior b) {
  return (b == Behavior::COMPRESS_PRS) ||
         (b == Behavior::DECOMPRESS_PRS) ||
         (b == Behavior::COMPRESS_BC0) ||
         (b == Behavior::DECOMPRESS_BC0) ||
         (b == Behavior::PRS_SIZE) ||
         (b == Behavior::ENCRYPT_DATA) ||
         (b == Behavior::DECRYPT_DATA) ||
         (b == Behavior::DECRYPT_TRIVIAL_DATA) ||
         (b == Behavior::DECODE_QUEST_FILE) ||
         (b == Behavior::DECODE_SJIS) ||
         (b == Behavior::EXTRACT_GSL) ||
         (b == Behavior::PARSE_OBJECT_GRAPH) ||
         (b == Behavior::REPLAY_LOG);
}

static bool behavior_takes_output_filename(Behavior b) {
  return (b == Behavior::COMPRESS_PRS) ||
         (b == Behavior::DECOMPRESS_PRS) ||
         (b == Behavior::COMPRESS_BC0) ||
         (b == Behavior::DECOMPRESS_BC0) ||
         (b == Behavior::ENCRYPT_DATA) ||
         (b == Behavior::DECRYPT_DATA) ||
         (b == Behavior::DECRYPT_TRIVIAL_DATA) ||
         (b == Behavior::DECODE_SJIS);
}

enum class QuestFileFormat {
  GCI = 0,
  DLQ,
  QST,
};

int main(int argc, char** argv) {
  Behavior behavior = Behavior::RUN_SERVER;
  GameVersion cli_version = GameVersion::GC;
  QuestFileFormat quest_file_type = QuestFileFormat::GCI;
  string seed;
  string key_file_name;
  const char* config_filename = "system/config.json";
  bool parse_data = false;
  bool big_endian = false;
  bool skip_little_endian = false;
  bool skip_big_endian = false;
  size_t num_threads = 0;
  const char* find_decryption_seed_ciphertext = nullptr;
  vector<const char*> find_decryption_seed_plaintexts;
  const char* input_filename = nullptr;
  const char* output_filename = nullptr;
  const char* replay_required_access_key = "";
  const char* replay_required_password = "";
  uint32_t root_object_address = 0;
  uint16_t ep3_card_id = 0xFFFF;
  struct sockaddr_storage cat_client_remote;
  for (int x = 1; x < argc; x++) {
    if (!strcmp(argv[x], "--help")) {
      print_usage();
      return 0;
    } else if (!strcmp(argv[x], "--compress-prs")) {
      behavior = Behavior::COMPRESS_PRS;
    } else if (!strcmp(argv[x], "--decompress-prs")) {
      behavior = Behavior::DECOMPRESS_PRS;
    } else if (!strcmp(argv[x], "--compress-bc0")) {
      behavior = Behavior::COMPRESS_BC0;
    } else if (!strcmp(argv[x], "--decompress-bc0")) {
      behavior = Behavior::DECOMPRESS_BC0;
    } else if (!strcmp(argv[x], "--prs-size")) {
      behavior = Behavior::PRS_SIZE;
    } else if (!strcmp(argv[x], "--encrypt-data")) {
      behavior = Behavior::ENCRYPT_DATA;
    } else if (!strcmp(argv[x], "--decrypt-data")) {
      behavior = Behavior::DECRYPT_DATA;
    } else if (!strcmp(argv[x], "--decrypt-trivial-data")) {
      behavior = Behavior::DECRYPT_TRIVIAL_DATA;
    } else if (!strcmp(argv[x], "--find-decryption-seed")) {
      behavior = Behavior::FIND_DECRYPTION_SEED;
    } else if (!strcmp(argv[x], "--decode-sjis")) {
      behavior = Behavior::DECODE_SJIS;
    } else if (!strcmp(argv[x], "--decode-gci")) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::GCI;
    } else if (!strcmp(argv[x], "--decode-dlq")) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::DLQ;
    } else if (!strcmp(argv[x], "--decode-qst")) {
      behavior = Behavior::DECODE_QUEST_FILE;
      quest_file_type = QuestFileFormat::QST;
    } else if (!strncmp(argv[x], "--cat-client=", 13)) {
      behavior = Behavior::CAT_CLIENT;
      cat_client_remote = make_sockaddr_storage(parse_netloc(&argv[x][13])).first;
    } else if (!strncmp(argv[x], "--threads=", 10)) {
      num_threads = strtoull(&argv[x][10], nullptr, 0);
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
    } else if (!strcmp(argv[x], "--show-ep3-data")) {
      behavior = Behavior::SHOW_EP3_DATA;
    } else if (!strncmp(argv[x], "--show-ep3-card=", 16)) {
      behavior = Behavior::SHOW_EP3_DATA;
      ep3_card_id = strtoul(&argv[x][16], nullptr, 16);
    } else if (!strcmp(argv[x], "--parse-object-graph")) {
      behavior = Behavior::PARSE_OBJECT_GRAPH;
    } else if (!strcmp(argv[x], "--replay-log")) {
      behavior = Behavior::REPLAY_LOG;
    } else if (!strcmp(argv[x], "--extract-gsl")) {
      behavior = Behavior::EXTRACT_GSL;
    } else if (!strncmp(argv[x], "--require-password=", 19)) {
      replay_required_password = &argv[x][19];
    } else if (!strncmp(argv[x], "--require-access-key=", 21)) {
      replay_required_access_key = &argv[x][21];
    } else if (!strncmp(argv[x], "--root-addr=", 12)) {
      root_object_address = strtoul(&argv[x][12], nullptr, 16);
    } else if (!strncmp(argv[x], "--config=", 9)) {
      config_filename = &argv[x][9];
    } else if (!strcmp(argv[x], "-") || argv[x][0] != '-') {
      if (!input_filename && behavior_takes_input_filename(behavior)) {
        input_filename = argv[x];
      } else if (!output_filename && behavior_takes_output_filename(behavior)) {
        output_filename = argv[x];
      } else {
        throw invalid_argument(string_printf("unknown option: %s", argv[x]));
      }
    } else {
      throw invalid_argument(string_printf("unknown option: %s", argv[x]));
    }
  }

  auto read_input_data = [&]() -> string {
    string data;
    if (input_filename && strcmp(input_filename, "-")) {
      data = load_file(input_filename);
    } else {
      data = read_all(stdin);
    }
    if (parse_data) {
      data = parse_data_string(data);
    }
    return data;
  };

  auto write_output_data = [&](const void* data, size_t size) {
    if (output_filename && strcmp(output_filename, "-")) {
      save_file(output_filename, data, size);
    } else if (isatty(fileno(stdout))) {
      print_data(stdout, data, size);
      fflush(stdout);
    } else {
      fwritex(stdout, data, size);
      fflush(stdout);
    }
  };

  switch (behavior) {
    case Behavior::COMPRESS_PRS:
    case Behavior::DECOMPRESS_PRS:
    case Behavior::COMPRESS_BC0:
    case Behavior::DECOMPRESS_BC0: {
      string data = read_input_data();
      size_t input_bytes = data.size();
      if (behavior == Behavior::COMPRESS_PRS) {
        data = prs_compress(data);
      } else if (behavior == Behavior::DECOMPRESS_PRS) {
        data = prs_decompress(data);
      } else if (behavior == Behavior::COMPRESS_BC0) {
        data = bc0_compress(data);
      } else if (behavior == Behavior::DECOMPRESS_BC0) {
        data = bc0_decompress(data);
      } else {
        throw logic_error("invalid behavior");
      }
      log_info("%zu (0x%zX) bytes input => %zu (0x%zX) bytes output",
          input_bytes, input_bytes, data.size(), data.size());

      write_output_data(data.data(), data.size());
      break;
    }

    case Behavior::PRS_SIZE: {
      string data = read_input_data();
      size_t input_bytes = data.size();
      size_t output_bytes = prs_decompress_size(data);
      log_info("%zu (0x%zX) bytes input => %zu (0x%zX) bytes output",
          input_bytes, input_bytes, output_bytes, output_bytes);
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

      string data = read_input_data();

      size_t original_size = data.size();
      data.resize((data.size() + 7) & (~7), '\0');

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

      data.resize(original_size);

      write_output_data(data.data(), data.size());

      break;
    }

    case Behavior::DECRYPT_TRIVIAL_DATA: {
      string data = read_input_data();
      uint8_t basis;
      if (seed.empty()) {
        uint8_t best_seed = 0x00;
        size_t best_seed_score = 0;
        for (size_t z = 0; z < 0x100; z++) {
          string decrypted = data;
          decrypt_trivial_gci_data(decrypted.data(), decrypted.size(), z);
          size_t score = 0;
          for (size_t x = 0; x < decrypted.size(); x++) {
            if (decrypted[x] == '\0') {
              score++;
            }
          }
          if (score > best_seed_score) {
            best_seed = z;
            best_seed_score = score;
          }
        }
        fprintf(stderr, "Basis appears to be %02hhX\n", best_seed);
        basis = best_seed;
      } else {
        basis = stoul(seed, nullptr, 16);
      }
      decrypt_trivial_gci_data(data.data(), data.size(), basis);
      write_output_data(data.data(), data.size());
      break;
    }

    case Behavior::FIND_DECRYPTION_SEED: {
      if (find_decryption_seed_plaintexts.empty() || !find_decryption_seed_ciphertext) {
        throw runtime_error("both --encrypted and --decrypted must be specified");
      }
      if (cli_version == GameVersion::BB) {
        throw runtime_error("--find-decryption-seed cannot be used for BB ciphers");
      }

      size_t max_plaintext_size = 0;
      vector<pair<string, string>> plaintexts;
      for (const auto& plaintext_ascii : find_decryption_seed_plaintexts) {
        string mask;
        string data = parse_data_string(plaintext_ascii, &mask);
        if (data.size() != mask.size()) {
          throw logic_error("plaintext and mask are not the same size");
        }
        max_plaintext_size = max<size_t>(max_plaintext_size, data.size());
        plaintexts.emplace_back(move(data), move(mask));
      }
      string ciphertext = parse_data_string(find_decryption_seed_ciphertext);

      auto mask_match = +[](const void* a, const void* b, const void* m, size_t size) -> bool {
        const uint8_t* a8 = reinterpret_cast<const uint8_t*>(a);
        const uint8_t* b8 = reinterpret_cast<const uint8_t*>(b);
        const uint8_t* m8 = reinterpret_cast<const uint8_t*>(m);
        for (size_t z = 0; z < size; z++) {
          if ((a8[z] & m8[z]) != (b8[z] & m8[z])) {
            return false;
          }
        }
        return true;
      };

      bool is_v3 = ((cli_version == GameVersion::GC) || (cli_version == GameVersion::XB));
      uint64_t seed = parallel_range<uint64_t>([&](uint64_t seed, size_t) -> bool {
        string be_decrypt_buf = ciphertext.substr(0, max_plaintext_size);
        string le_decrypt_buf = ciphertext.substr(0, max_plaintext_size);
        if (is_v3) {
          PSOV3Encryption(seed).encrypt_both_endian(
              le_decrypt_buf.data(),
              be_decrypt_buf.data(),
              be_decrypt_buf.size());
        } else {
          PSOV2Encryption(seed).encrypt_both_endian(
              le_decrypt_buf.data(),
              be_decrypt_buf.data(),
              be_decrypt_buf.size());
        }

        for (const auto& plaintext : plaintexts) {
          if (!skip_little_endian) {
            if (mask_match(le_decrypt_buf.data(), plaintext.first.data(), plaintext.second.data(), plaintext.second.size())) {
              return true;
            }
          }
          if (!skip_big_endian) {
            if (mask_match(be_decrypt_buf.data(), plaintext.first.data(), plaintext.second.data(), plaintext.second.size())) {
              return true;
            }
          }
        }
        return false;
      }, 0, 0x100000000, num_threads);

      if (seed < 0x100000000) {
        log_info("Found seed %08" PRIX64, seed);
      } else {
        log_error("No seed found");
      }
      break;
    }

    case Behavior::DECODE_QUEST_FILE: {
      if (!input_filename || !strcmp(input_filename, "-")) {
        throw invalid_argument("an input filename is required");
      }

      string output_filename_base = input_filename;
      if (quest_file_type == QuestFileFormat::GCI) {
        int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
        save_file(output_filename_base + ".dec", Quest::decode_gci(
            input_filename, num_threads, dec_seed));
      } else if (quest_file_type == QuestFileFormat::DLQ) {
        save_file(output_filename_base + ".dec", Quest::decode_dlq(
            input_filename));
      } else if (quest_file_type == QuestFileFormat::QST) {
        auto data = Quest::decode_qst(input_filename);
        save_file(output_filename_base + ".bin", data.first);
        save_file(output_filename_base + ".dat", data.second);
      } else {
        throw logic_error("invalid quest file format");
      }
      break;
    }

    case Behavior::DECODE_SJIS: {
      string data = read_input_data();
      auto decoded = decode_sjis(data);
      write_output_data(decoded.data(), decoded.size() * sizeof(decoded[0]));
      break;
    }

    case Behavior::EXTRACT_GSL: {
      if (!output_filename) {
        output_filename = "";
      } else if (!strcmp(output_filename, "-")) {
        throw invalid_argument("output prefix cannot be stdout");
      }

      string data = read_input_data();

      shared_ptr<string> data_shared(new string(move(data)));
      GSLArchive gsl(data_shared);
      for (const auto& entry_it : gsl.all_entries()) {
        auto e = gsl.get(entry_it.first);
        save_file(output_filename + entry_it.first, e.first, e.second);
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

    case Behavior::SHOW_EP3_DATA: {
      config_log.info("Collecting Episode 3 data");
      Episode3::DataIndex index("system/ep3", Episode3::BehaviorFlag::LOAD_CARD_TEXT);

      if (ep3_card_id == 0xFFFF) {
        auto map_ids = index.all_map_ids();
        log_info("%zu maps", map_ids.size());
        for (uint32_t map_id : map_ids) {
          auto map = index.definition_for_map_number(map_id);
          string s = map->map.str(&index);
          fprintf(stdout, "%s\n", s.c_str());
        }

        auto card_ids = index.all_card_ids();
        log_info("%zu card definitions", card_ids.size());
        for (uint32_t card_id : card_ids) {
          auto entry = index.definition_for_card_id(card_id);
          string s = entry->def.str();
          string tags = entry->debug_tags.empty() ? "(none)" : join(entry->debug_tags, ", ");
          string text = entry->text.empty() ? "(No text available)" : entry->text;
          fprintf(stdout, "%s\nTags: %s\n%s\n\n", s.c_str(), tags.c_str(), text.c_str());
        }

      } else {
        auto entry = index.definition_for_card_id(ep3_card_id);
        string s = entry->def.str();
        string tags = entry->debug_tags.empty() ? "(none)" : join(entry->debug_tags, ", ");
        string text = entry->text.empty() ? "(No text available)" : entry->text;
        fprintf(stdout, "%s\nTags: %s\n%s\n", s.c_str(), tags.c_str(), text.c_str());
      }

      break;
    }

    case Behavior::PARSE_OBJECT_GRAPH: {
      string data = read_input_data();
      PSOGCObjectGraph g(data, root_object_address);
      g.print(stdout);
      break;
    }

    case Behavior::REPLAY_LOG:
    case Behavior::RUN_SERVER: {
      signal(SIGPIPE, SIG_IGN);

      if (isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      shared_ptr<ServerState> state(new ServerState());

      config_log.info("Reading network addresses");
      state->all_addresses = get_local_addresses();
      for (const auto& it : state->all_addresses) {
        string addr_str = string_for_address(it.second);
        config_log.info("Found interface: %s = %s", it.first.c_str(), addr_str.c_str());
      }

      config_log.info("Loading configuration");
      auto config_json = JSONObject::parse(load_file(config_filename));
      populate_state_from_config(state, config_json);

      if (behavior != Behavior::REPLAY_LOG) {
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
      state->ep3_data_index.reset(new Episode3::DataIndex(
          "system/ep3", state->ep3_behavior_flags));

      const string& tournament_state_filename = "system/ep3/tournament-state.json";
      try {
        state->ep3_tournament_index.reset(new Episode3::TournamentIndex(
            state->ep3_data_index, tournament_state_filename));
        config_log.info("Loaded Episode 3 tournament state");
      } catch (const exception& e) {
        config_log.warning("Cannot load Episode 3 tournament state: %s", e.what());
        state->ep3_tournament_index.reset(new Episode3::TournamentIndex(
            state->ep3_data_index, tournament_state_filename, true));
      }

      config_log.info("Collecting quest metadata");
      state->quest_index.reset(new QuestIndex("system/quests"));

      if (behavior != Behavior::REPLAY_LOG) {
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

      if (behavior == Behavior::REPLAY_LOG) {
        state->allow_saving = false;
        state->license_manager->set_autosave(false);
        config_log.info("Saving disabled because this is a replay session");
      }

      shared_ptr<DNSServer> dns_server;
      if (state->dns_server_port && (behavior != Behavior::REPLAY_LOG)) {
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

        shared_ptr<FILE> log_f(stdin, +[](FILE*) { });
        if (input_filename && strcmp(input_filename, "-")) {
          log_f = fopen_shared(input_filename, "rt");
        }

        replay_session.reset(new ReplaySession(
            base, log_f.get(), state, replay_required_access_key, replay_required_password));
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
