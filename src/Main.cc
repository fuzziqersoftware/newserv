#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Math.hh>
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



template <typename T>
vector<T> parse_int_vector(shared_ptr<const JSONObject> o) {
  vector<T> ret;
  for (const auto& x : o->as_list()) {
    ret.emplace_back(x->as_int());
  }
  return ret;
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
Usage:\n\
  newserv [ACTION [OPTIONS...]]\n\
\n\
If ACTION is not specified, newserv runs in server mode. PSO clients can\n\
connect normally, join lobbies, play games, and use the proxy server. See\n\
README.md and system/config.json for more information.\n\
\n\
When ACTION is given, newserv will do things other than running the server.\n\
\n\
Some actions accept input and/or output filenames; see the descriptions below\n\
for details. If INPUT-FILENAME is missing or is '-', newserv reads from stdin.\n\
If OUTPUT-FILENAME is missing and the input is not from stdin, newserv writes\n\
the output to INPUT-FILENAME.dec; if OUTPUT-FILENAME is '-', newserv writes the\n\
output to stdout. If stdout is a terminal, data written there is formatted in a\n\
hex/ASCII view; otherwise, raw (binary) data is written there.\n\
\n\
The actions are:\n\
  help\n\
    You\'re reading it now.\n\
  compress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Compress or decompress data using the PRS or BC0 algorithms.\n\
  prs-size [INPUT-FILENAME]\n\
    Compute the decompressed size of the PRS-compressed input data, but don\'t\n\
    write the decompressed data anywhere.\n\
  encrypt-data [INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS...]]\n\
  decrypt-data [INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS...]]\n\
    Encrypt or decrypt data using PSO\'s standard network protocol encryption.\n\
    By default, PSO V3 (GameCube/XBOX) encryption is used, but this can be\n\
    overridden with the --pc or --bb options. The --seed=SEED option specifies\n\
    the encryption seed (4 hex bytes for PC or GC, or 48 hex bytes for BB). For\n\
    BB, the --key=KEY-NAME option is required as well, and refers to a .nsk\n\
    file in system/blueburst/keys (without the directory or .nsk extension).\n\
    For non-BB ciphers, the --big-endian option applies the cipher masks as\n\
    big-endian instead of little-endian, which is necessary for some GameCube\n\
    file formats.\n\
  decrypt-trivial-data [--seed=SEED] [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decrypt (or encrypt; the algorithm is symmetric) data using the Episode 3\n\
    trivial algorithm. If SEED is given, it should be specified as one hex\n\
    byte. If SEED is not given, newserv will try all possible seeds and return\n\
    the one that results in the greatest number of zero bytes in the output.\n\
  find-decryption-seed <OPTIONS...>\n\
    Perform a brute-force search for a decryption seed of the given data. The\n\
    ciphertext is specified with the --encrypted=DATA option and the expected\n\
    plaintext is specified with the --decrypted=DATA option. The plaintext may\n\
    include unmatched bytes (specified with the Phosg parse_data_string ?\n\
    operator), but overall it must be the same length as the ciphertext. By\n\
    default, this option uses PSO V3 encryption, but this can be overridden\n\
    with --pc. (BB encryption seeds are too long to be searched for with this\n\
    function.) By default, the number of worker threads is equal the the number\n\
    of CPU cores in the system, but this can be overridden with the\n\
    --threads=NUM-THREADS option.\n\
  decode-sjis [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Apply newserv\'s text decoding algorithm to the input data, producing\n\
    little-endian UTF-16 output data.\n\
  decode-gci INPUT-FILENAME [OPTIONS...]\n\
  decode-vms INPUT-FILENAME [OPTIONS...]\n\
  decode-dlq INPUT-FILENAME\n\
  decode-qst INPUT-FILENAME\n\
    Decode the input quest file into a compressed, unencrypted .bin or .dat\n\
    file (or in the case of decode-qst, both a .bin and a .dat file).\n\
    INPUT-FILENAME must be specified, but there is no OUTPUT-FILENAME; the\n\
    output is written to INPUT-FILENAME.dec (or .bin, or .dat). If the output\n\
    is a .dec file, you can rename it to .bin or .dat manually. DLQ and QST\n\
    decoding are relatively simple operations, but GCI and VMS decoding can be\n\
    computationally expensive if the file is encrypted and doesn\'t contain an\n\
    embedded seed. If you know the player\'s serial number who generated the\n\
    GCI or VMS file, use the --seed=SEED option and give the serial number (as\n\
    a hex-encoded 32-bit integer). If you don\'t know the serial number,\n\
    newserv will find it via a brute-force search, which will take a long time.\n\
  cat-client ADDR:PORT\n\
    Connect to the given server and simulate a PSO client. newserv will then\n\
    print all the received commands to stdout, and forward any commands typed\n\
    into stdin to the remote server. It is assumed that the input and output\n\
    are terminals, so all commands are hex-encoded. The --patch, --dc, --pc,\n\
    --gc, and --bb options can be used to select the command format and\n\
    encryption. If --bb is used, the --key=KEY-NAME option is also required (as\n\
    in decrypt-data above).\n\
  show-ep3-data\n\
    Print the Episode 3 maps and card definitions from the system/ep3 directory\n\
    in a (sort of) human-readable format.\n\
  replay-log [INPUT-FILENAME] [OPTIONS...]\n\
    Replay a terminal log as if it were a client session. input-filename may be\n\
    specified for this option. This is used for regression testing, to make\n\
    sure client sessions are repeatable and code changes don\'t affect existing\n\
    (working) functionality.\n\
  extract-gsl [INPUT-FILENAME] [--big-endian]\n\
    Extract all files from a GSL archive into the current directory.\n\
    input-filename may be specified. If output-filename is specified, then it\n\
    is treated as a prefix which is prepended to the filename of each file\n\
    contained in the GSL archive. If --big-endian is given, the GSL header is\n\
    read in GameCube format; otherwise it is read in PC/BB format.\n\
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
  PRS_DISASSEMBLE,
  ENCRYPT_DATA,
  DECRYPT_DATA,
  DECRYPT_TRIVIAL_DATA,
  FIND_DECRYPTION_SEED,
  DECODE_QUEST_FILE,
  DECODE_SJIS,
  EXTRACT_GSL,
  FORMAT_ITEMRT_ENTRY,
  FORMAT_ITEMRT_REL,
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
         (b == Behavior::PRS_DISASSEMBLE) ||
         (b == Behavior::ENCRYPT_DATA) ||
         (b == Behavior::DECRYPT_DATA) ||
         (b == Behavior::DECRYPT_TRIVIAL_DATA) ||
         (b == Behavior::DECODE_QUEST_FILE) ||
         (b == Behavior::DECODE_SJIS) ||
         (b == Behavior::FORMAT_ITEMRT_ENTRY) ||
         (b == Behavior::FORMAT_ITEMRT_REL) ||
         (b == Behavior::EXTRACT_GSL) ||
         (b == Behavior::PARSE_OBJECT_GRAPH) ||
         (b == Behavior::REPLAY_LOG) ||
         (b == Behavior::CAT_CLIENT);
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
  VMS,
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
  for (int x = 1; x < argc; x++) {
    if (!strcmp(argv[x], "--help")) {
      print_usage();
      return 0;
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
    } else if (!strncmp(argv[x], "--require-password=", 19)) {
      replay_required_password = &argv[x][19];
    } else if (!strncmp(argv[x], "--require-access-key=", 21)) {
      replay_required_access_key = &argv[x][21];
    } else if (!strncmp(argv[x], "--root-addr=", 12)) {
      root_object_address = strtoul(&argv[x][12], nullptr, 16);
    } else if (!strncmp(argv[x], "--config=", 9)) {
      config_filename = &argv[x][9];

    } else if (!strcmp(argv[x], "-") || argv[x][0] != '-') {
      if (behavior == Behavior::RUN_SERVER) {
        if (!strcmp(argv[x], "help")) {
          print_usage();
          return 0;
        } if (!strcmp(argv[x], "compress-prs")) {
          behavior = Behavior::COMPRESS_PRS;
        } else if (!strcmp(argv[x], "decompress-prs")) {
          behavior = Behavior::DECOMPRESS_PRS;
        } else if (!strcmp(argv[x], "compress-bc0")) {
          behavior = Behavior::COMPRESS_BC0;
        } else if (!strcmp(argv[x], "decompress-bc0")) {
          behavior = Behavior::DECOMPRESS_BC0;
        } else if (!strcmp(argv[x], "prs-size")) {
          behavior = Behavior::PRS_SIZE;
        } else if (!strcmp(argv[x], "disassemble-prs")) {
          behavior = Behavior::PRS_DISASSEMBLE;
        } else if (!strcmp(argv[x], "encrypt-data")) {
          behavior = Behavior::ENCRYPT_DATA;
        } else if (!strcmp(argv[x], "decrypt-data")) {
          behavior = Behavior::DECRYPT_DATA;
        } else if (!strcmp(argv[x], "decrypt-trivial-data")) {
          behavior = Behavior::DECRYPT_TRIVIAL_DATA;
        } else if (!strcmp(argv[x], "find-decryption-seed")) {
          behavior = Behavior::FIND_DECRYPTION_SEED;
        } else if (!strcmp(argv[x], "decode-sjis")) {
          behavior = Behavior::DECODE_SJIS;
        } else if (!strcmp(argv[x], "decode-gci")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = QuestFileFormat::GCI;
        } else if (!strcmp(argv[x], "decode-vms")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = QuestFileFormat::VMS;
        } else if (!strcmp(argv[x], "decode-dlq")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = QuestFileFormat::DLQ;
        } else if (!strcmp(argv[x], "decode-qst")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = QuestFileFormat::QST;
        } else if (!strcmp(argv[x], "cat-client")) {
          behavior = Behavior::CAT_CLIENT;
        } else if (!strcmp(argv[x], "format-itemrt-entry")) {
          behavior = Behavior::FORMAT_ITEMRT_ENTRY;
        } else if (!strcmp(argv[x], "format-itemrt-rel")) {
          behavior = Behavior::FORMAT_ITEMRT_REL;
        } else if (!strcmp(argv[x], "show-ep3-data")) {
          behavior = Behavior::SHOW_EP3_DATA;
        } else if (!strcmp(argv[x], "parse-object-graph")) {
          behavior = Behavior::PARSE_OBJECT_GRAPH;
        } else if (!strcmp(argv[x], "replay-log")) {
          behavior = Behavior::REPLAY_LOG;
        } else if (!strcmp(argv[x], "extract-gsl")) {
          behavior = Behavior::EXTRACT_GSL;
        } else {
          throw invalid_argument(string_printf("unknown command: %s (try --help)", argv[x]));
        }
      } else if (!input_filename && behavior_takes_input_filename(behavior)) {
        input_filename = argv[x];
      } else if (!output_filename && behavior_takes_output_filename(behavior)) {
        output_filename = argv[x];
      } else {
        throw invalid_argument(string_printf("unknown option: %s (try --help)", argv[x]));
      }

    } else {
      throw invalid_argument(string_printf("unknown option: %s (try --help)", argv[x]));
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
      data = parse_data_string(data, nullptr, ParseDataFlags::ALLOW_FILES);
    }
    return data;
  };

  auto write_output_data = [&](const void* data, size_t size) {
    // If the output is to a specified file, write it there
    if (output_filename && strcmp(output_filename, "-")) {
      save_file(output_filename, data, size);
    // If no output filename is given and an input filename is given, write to
    // <input-filename>.dec (or an appropriate extension, if it can be
    // autodetected)
    } else if (!output_filename && input_filename && strcmp(input_filename, "-")) {
      string filename = input_filename;
      if (behavior == Behavior::COMPRESS_PRS) {
        if (ends_with(filename, ".bind") || ends_with(filename, ".datd") || ends_with(filename, ".mnmd")) {
          filename.resize(filename.size() - 1);
        } else {
          filename += ".prs";
        }
      } else if (behavior == Behavior::DECOMPRESS_PRS) {
        if (ends_with(filename, ".bin") || ends_with(filename, ".dat") || ends_with(filename, ".mnm")) {
          filename += "d";
        } else {
          filename += ".dec";
        }
      } else {
        filename += ".dec";
      }
      save_file(filename, data, size);
    // If stdout is a terminal, use print_data to write the result
    } else if (isatty(fileno(stdout))) {
      print_data(stdout, data, size);
      fflush(stdout);
    // If stdout is not a terminal, write the data as-is
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
      auto progress_fn = [&](size_t input_progress, size_t output_progress) -> void {
        float progress = static_cast<float>(input_progress * 100) / input_bytes;
        float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
        fprintf(stderr, "... %zu/%zu (%g%%) => %zu (%g%%)    \r",
            input_progress, input_bytes, progress, output_progress, size_ratio);
      };

      uint64_t start = now();
      if (behavior == Behavior::COMPRESS_PRS) {
        data = prs_compress(data, progress_fn);
      } else if (behavior == Behavior::DECOMPRESS_PRS) {
        data = prs_decompress(data);
      } else if (behavior == Behavior::COMPRESS_BC0) {
        data = bc0_compress(data, progress_fn);
      } else if (behavior == Behavior::DECOMPRESS_BC0) {
        data = bc0_decompress(data);
      } else {
        throw logic_error("invalid behavior");
      }
      uint64_t end = now();
      string time_str = format_duration(end - start);

      float size_ratio = static_cast<float>(data.size() * 100) / input_bytes;
      double bytes_per_sec = input_bytes / (static_cast<double>(end - start) / 1000000.0);
      string bytes_per_sec_str = format_size(bytes_per_sec);
      log_info("%zu (0x%zX) bytes input => %zu (0x%zX) bytes output (%g%%) in %s (%s / sec)",
          input_bytes, input_bytes, data.size(), data.size(), size_ratio, time_str.c_str(), bytes_per_sec_str.c_str());

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

    case Behavior::PRS_DISASSEMBLE: {
      prs_disassemble(stdout, read_input_data());
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
          seed = parse_data_string(seed, nullptr, ParseDataFlags::ALLOW_FILES);
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
        string data = parse_data_string(plaintext_ascii, &mask, ParseDataFlags::ALLOW_FILES);
        if (data.size() != mask.size()) {
          throw logic_error("plaintext and mask are not the same size");
        }
        max_plaintext_size = max<size_t>(max_plaintext_size, data.size());
        plaintexts.emplace_back(move(data), move(mask));
      }
      string ciphertext = parse_data_string(find_decryption_seed_ciphertext, nullptr, ParseDataFlags::ALLOW_FILES);

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
      } else if (quest_file_type == QuestFileFormat::VMS) {
        int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
        save_file(output_filename_base + ".dec", Quest::decode_vms(
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
      GSLArchive gsl(data_shared, big_endian);
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
      auto cat_client_remote = make_sockaddr_storage(parse_netloc(input_filename)).first;
      CatSession session(base, cat_client_remote, cli_version, key);
      event_base_dispatch(base.get());
      break;
    }

    case Behavior::FORMAT_ITEMRT_ENTRY: {
      string data = read_input_data();
      if (data.size() < sizeof(RareItemSet::Table)) {
        throw runtime_error("input data too small");
      }
      const auto& table = *reinterpret_cast<const RareItemSet::Table*>(data.data());

      auto format_drop = +[](const RareItemSet::Table::Drop& r) -> string {
        ItemData item;
        item.data1[0] = r.item_code[0];
        item.data1[1] = r.item_code[1];
        item.data1[2] = r.item_code[2];
        string name = item.name(false);

        uint32_t expanded_probability = RareItemSet::expand_rate(r.probability);
        auto frac = reduce_fraction<uint64_t>(expanded_probability, 0x100000000);
        return string_printf("(%02hhX => %08" PRIX32 " => %" PRIu64 "/%" PRIu64 ") %02hhX%02hhX%02hhX (%s)",
            r.probability, expanded_probability, frac.first, frac.second, r.item_code[0], r.item_code[1], r.item_code[2], name.c_str());
      };

      fprintf(stdout, "Monster rares:\n");
      for (size_t z = 0; z < 0x65; z++) {
        const auto& r = table.monster_rares[z];
        if (r.item_code[0] == 0 && r.item_code[1] == 0 && r.item_code[2] == 0) {
          continue;
        }
        string s = format_drop(r);
        fprintf(stdout, "  %02zX: %s\n", z, s.c_str());
      }

      fprintf(stdout, "Box rares:\n");
      for (size_t z = 0; z < 0x1E; z++) {
        const auto& r = table.box_rares[z];
        if (r.item_code[0] == 0 && r.item_code[1] == 0 && r.item_code[2] == 0) {
          continue;
        }
        string s = format_drop(r);
        fprintf(stdout, "  %02zX: area %02hhX %s\n", z, table.box_areas[z], s.c_str());
      }
      break;
    }

    case Behavior::FORMAT_ITEMRT_REL: {
      shared_ptr<string> data(new string(read_input_data()));
      RELRareItemSet rs(data);

      auto format_drop = +[](const RareItemSet::Table::Drop& r) -> string {
        ItemData item;
        item.data1[0] = r.item_code[0];
        item.data1[1] = r.item_code[1];
        item.data1[2] = r.item_code[2];
        string name = item.name(false);

        uint32_t expanded_probability = RareItemSet::expand_rate(r.probability);
        auto frac = reduce_fraction<uint64_t>(expanded_probability, 0x100000000);
        return string_printf("(%02hhX => %08" PRIX32 " => %" PRIu64 "/%" PRIu64 ") %02hhX%02hhX%02hhX (%s)",
            r.probability, expanded_probability, frac.first, frac.second, r.item_code[0], r.item_code[1], r.item_code[2], name.c_str());
      };

      auto print_collection = [&](GameMode mode, Episode episode, uint8_t difficulty, uint8_t section_id) -> void {
        const auto& table = rs.get_table(episode, mode, difficulty, section_id);

        string secid_name = name_for_section_id(section_id);
        fprintf(stdout, "%s %s %s %s\n",
            name_for_mode(mode),
            name_for_episode(episode),
            name_for_difficulty(difficulty),
            secid_name.c_str());

        fprintf(stdout, "  Monster rares:\n");
        for (size_t z = 0; z < 0x65; z++) {
          const auto& r = table.monster_rares[z];
          if (r.item_code[0] == 0 && r.item_code[1] == 0 && r.item_code[2] == 0) {
            continue;
          }
          string s = format_drop(r);
          fprintf(stdout, "    %02zX: %s\n", z, s.c_str());
        }

        fprintf(stdout, "  Box rares:\n");
        for (size_t z = 0; z < 0x1E; z++) {
          const auto& r = table.box_rares[z];
          if (r.item_code[0] == 0 && r.item_code[1] == 0 && r.item_code[2] == 0) {
            continue;
          }
          string s = format_drop(r);
          fprintf(stdout, "    %02zX: area %02hhX %s\n", z, table.box_areas[z], s.c_str());
        }
      };

      static const vector<Episode> episodes = {
        Episode::EP1,
        Episode::EP2,
        Episode::EP4,
      };
      for (Episode episode : episodes) {
        for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
          for (uint8_t section_id = 0; section_id < 10; section_id++) {
            print_collection(GameMode::NORMAL, episode, difficulty, section_id);
          }
        }
      }
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
      bool is_replay = behavior == Behavior::REPLAY_LOG;
      signal(SIGPIPE, SIG_IGN);

      if (isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      shared_ptr<ServerState> state(new ServerState(config_filename, is_replay));

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

#ifndef PHOSG_WINDOWS
        if (!state->ip_stack_addresses.empty()) {
          config_log.info("Starting IP stack simulator");
          ip_stack_simulator.reset(new IPStackSimulator(base, state));
          for (const auto& it : state->ip_stack_addresses) {
            auto netloc = parse_netloc(it);
            ip_stack_simulator->listen(netloc.first, netloc.second);
          }
        }
#endif

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
