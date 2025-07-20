#include <phosg/Platform.hh>

#include <signal.h>
#include <string.h>
#ifndef PHOSG_WINDOWS
#include <pwd.h>
#endif

#include <asio.hpp>
#include <filesystem>
#include <mutex>
#include <phosg/Arguments.hh>
#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Math.hh>
#include <phosg/Network.hh>
#include <phosg/Platform.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <set>
#include <thread>
#include <unordered_map>

#include "AddressTranslator.hh"
#include "BMLArchive.hh"
#include "Compression.hh"
#include "DCSerialNumbers.hh"
#include "DNSServer.hh"
#include "DownloadSession.hh"
#include "GSLArchive.hh"
#include "GameServer.hh"
#include "HTTPServer.hh"
#include "IPStackSimulator.hh"
#include "ImageEncoder.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "PPKArchive.hh"
#include "PSOGCObjectGraph.hh"
#include "PSOProtocol.hh"
#include "Quest.hh"
#include "QuestScript.hh"
#include "ReplaySession.hh"
#include "Revision.hh"
#include "SaveFileFormats.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "ServerShell.hh"
#include "ServerState.hh"
#include "SignalWatcher.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "TextIndex.hh"

using namespace std;

#ifdef PHOSG_WINDOWS
static constexpr bool IS_WINDOWS = true;
#else
static constexpr bool IS_WINDOWS = false;
#endif

bool use_terminal_colors = false;

void print_version_info();
void print_usage();

std::string get_config_filename(phosg::Arguments& args) {
  string config_filename = args.get<string>("config");
  return config_filename.empty() ? "system/config.json" : config_filename;
}

template <typename T>
vector<T> parse_int_vector(const phosg::JSON& o) {
  vector<T> ret;
  for (const auto& x : o.as_list()) {
    ret.emplace_back(x->as_int());
  }
  return ret;
}

#ifndef PHOSG_WINDOWS
void drop_privileges(const string& username) {
  if ((getuid() != 0) || (getgid() != 0)) {
    throw runtime_error(std::format(
        "newserv was not started as root; can\'t switch to user {}",
        username));
  }

  struct passwd* pw = getpwnam(username.c_str());
  if (!pw) {
    string error = phosg::string_for_error(errno);
    throw runtime_error(std::format("user {} not found ({})",
        username, error));
  }

  if (setgid(pw->pw_gid) != 0) {
    string error = phosg::string_for_error(errno);
    throw runtime_error(std::format("can\'t switch to group {} ({})",
        pw->pw_gid, error));
  }
  if (setuid(pw->pw_uid) != 0) {
    string error = phosg::string_for_error(errno);
    throw runtime_error(std::format("can\'t switch to user {} ({})",
        pw->pw_uid, error));
  }
  config_log.info_f("Switched to user {} ({}:{})", username, pw->pw_uid, pw->pw_gid);
}
#endif

Version get_cli_version(phosg::Arguments& args, Version default_value = Version::UNKNOWN) {
  if (args.get<bool>("pc-patch")) {
    return Version::PC_PATCH;
  } else if (args.get<bool>("bb-patch")) {
    return Version::BB_PATCH;
  } else if (args.get<bool>("dc-nte")) {
    return Version::DC_NTE;
  } else if (args.get<bool>("dc-proto") || args.get<bool>("dc-11-2000")) {
    return Version::DC_11_2000;
  } else if (args.get<bool>("dc-v1")) {
    return Version::DC_V1;
  } else if (args.get<bool>("dc-v2") || args.get<bool>("dc")) {
    return Version::DC_V2;
  } else if (args.get<bool>("pc-nte")) {
    return Version::PC_NTE;
  } else if (args.get<bool>("pc") || args.get<bool>("pc-v2")) {
    return Version::PC_V2;
  } else if (args.get<bool>("gc-nte")) {
    return Version::GC_NTE;
  } else if (args.get<bool>("gc") || args.get<bool>("gc-v3")) {
    return Version::GC_V3;
  } else if (args.get<bool>("xb") || args.get<bool>("xb-v3")) {
    return Version::XB_V3;
  } else if (args.get<bool>("ep3-nte") || args.get<bool>("gc-ep3-nte")) {
    return Version::GC_EP3_NTE;
  } else if (args.get<bool>("ep3") || args.get<bool>("gc-ep3")) {
    return Version::GC_EP3;
  } else if (args.get<bool>("bb") || args.get<bool>("bb-v4")) {
    return Version::BB_V4;
  } else if (default_value != Version::UNKNOWN) {
    return default_value;
  } else {
    throw runtime_error("a version option is required");
  }
}

Episode get_cli_episode(phosg::Arguments& args) {
  if (args.get<bool>("ep1")) {
    return Episode::EP1;
  } else if (args.get<bool>("ep2")) {
    return Episode::EP2;
  } else if (args.get<bool>("ep3")) {
    return Episode::EP3;
  } else if (args.get<bool>("ep4")) {
    return Episode::EP4;
  } else {
    throw runtime_error("an episode option is required");
  }
}

GameMode get_cli_game_mode(phosg::Arguments& args) {
  if (args.get<bool>("battle")) {
    return GameMode::BATTLE;
  } else if (args.get<bool>("challenge")) {
    return GameMode::CHALLENGE;
  } else if (args.get<bool>("solo")) {
    return GameMode::SOLO;
  } else {
    return GameMode::NORMAL;
  }
}

uint8_t get_cli_difficulty(phosg::Arguments& args) {
  if (args.get<bool>("hard")) {
    return 1;
  } else if (args.get<bool>("very-hard")) {
    return 2;
  } else if (args.get<bool>("ultimate")) {
    return 3;
  } else {
    return 0;
  }
}

string read_input_data(phosg::Arguments& args) {
  const string& input_filename = args.get<string>(1, false);

  string data;
  if (!input_filename.empty() && (input_filename != "-")) {
    data = phosg::load_file(input_filename);
  } else {
    data = phosg::read_all(stdin);
  }
  if (args.get<bool>("parse-data")) {
    data = phosg::parse_data_string(data, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
  }
  return data;
}

bool is_text_extension(const char* extension) {
  return (!strcmp(extension, "txt") || !strcmp(extension, "json") || !strcmp(extension, "reg"));
}

void write_output_data(phosg::Arguments& args, const void* data, size_t size, const char* extension) {
  const string& input_filename = args.get<string>(1, false);
  const string& output_filename = args.get<string>(2, false);

  if (!output_filename.empty() && (output_filename != "-")) {
    // If the output is to a specified file, write it there
    phosg::save_file(output_filename, data, size);

  } else if (output_filename.empty() && (output_filename != "-") && !input_filename.empty() && (input_filename != "-")) {
    // If no output filename is given and an input filename is given, write to
    // <input_filename>.<extension>
    if (!extension) {
      throw runtime_error("an output filename is required");
    }
    string filename = input_filename;
    filename += ".";
    filename += extension;
    phosg::save_file(filename, data, size);

  } else if (isatty(fileno(stdout)) && (!extension || !is_text_extension(extension))) {
    // If stdout is a terminal and the data is not known to be text, use
    // print_data to write the result
    phosg::print_data(stdout, data, size);
    fflush(stdout);

  } else {
    // If stdout is not a terminal, write the data as-is
    phosg::fwritex(stdout, data, size);
    fflush(stdout);
  }
}

struct Action;
unordered_map<string, const Action*> all_actions;
vector<const Action*> action_order;

struct Action {
  const char* name;
  const char* help_text; // May be null
  function<void(phosg::Arguments& args)> run;

  Action(
      const char* name,
      const char* help_text,
      function<void(phosg::Arguments& args)> run)
      : name(name),
        help_text(help_text),
        run(run) {
    auto emplace_ret = all_actions.emplace(this->name, this);
    if (!emplace_ret.second) {
      throw logic_error(std::format("multiple actions with the same name: {}", this->name));
    }
    action_order.emplace_back(this);
  }
};

Action a_help(
    "help", "\
  help\n\
    You\'re reading it now.\n",
    +[](phosg::Arguments&) -> void {
      print_usage();
    });

Action a_version(
    "version", "\
  version\n\
    Show newserv\'s revision and build date.\n",
    +[](phosg::Arguments&) -> void {
      print_version_info();
    });

static void a_compress_decompress_fn(phosg::Arguments& args) {
  const auto& action = args.get<string>(0);
  bool is_prs = action.ends_with("-prs");
  bool is_bc0 = action.ends_with("-bc0");
  bool is_pr2 = action.ends_with("-pr2");
  bool is_prc = action.ends_with("-prc");
  bool is_decompress = action.starts_with("decompress-");
  bool is_big_endian = args.get<bool>("big-endian");
  bool is_optimal = args.get<bool>("optimal");
  bool is_pessimal = args.get<bool>("pessimal");
  int8_t compression_level = args.get<int8_t>("compression-level", 0);
  size_t bytes = args.get<size_t>("bytes", 0);
  string seed = args.get<string>("seed");

  string data = read_input_data(args);

  size_t pr2_expected_size = 0;
  if (is_decompress && (is_pr2 || is_prc)) {
    auto decrypted = is_big_endian ? decrypt_pr2_data<true>(data) : decrypt_pr2_data<false>(data);
    pr2_expected_size = decrypted.decompressed_size;
    data = std::move(decrypted.compressed_data);
  }

  size_t input_bytes = data.size();
  auto progress_fn = [&](auto, size_t input_progress, size_t, size_t output_progress) -> void {
    float progress = static_cast<float>(input_progress * 100) / input_bytes;
    float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
    phosg::fwrite_fmt(stderr, "... {}/{} ({:g}%) => {} ({:g}%)    \r",
        input_progress, input_bytes, progress, output_progress, size_ratio);
  };
  auto optimal_progress_fn = [&](auto phase, size_t input_progress, size_t input_bytes, size_t output_progress) -> void {
    const char* phase_name = phosg::name_for_enum(phase);
    float progress = static_cast<float>(input_progress * 100) / input_bytes;
    float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
    phosg::fwrite_fmt(stderr, "... [{}] {}/{} ({:g}%) => {} ({:g}%)    \r",
        phase_name, input_progress, input_bytes, progress, output_progress, size_ratio);
  };

  uint64_t start = phosg::now();
  if (!is_decompress && (is_prs || is_pr2 || is_prc)) {
    if (is_optimal) {
      data = prs_compress_optimal(data.data(), data.size(), optimal_progress_fn);
    } else if (is_pessimal) {
      data = prs_compress_pessimal(data.data(), data.size());
    } else {
      data = prs_compress(data, compression_level, progress_fn);
    }
  } else if (is_decompress && (is_prs || is_pr2 || is_prc)) {
    data = prs_decompress(data, bytes, (bytes != 0));
  } else if (!is_decompress && is_bc0) {
    if (is_optimal) {
      data = bc0_compress_optimal(data.data(), data.size(), optimal_progress_fn);
    } else if (compression_level < 0) {
      data = bc0_encode(data.data(), data.size());
    } else {
      data = bc0_compress(data, progress_fn);
    }
  } else if (is_decompress && is_bc0) {
    data = bc0_decompress(data);
  } else {
    throw logic_error("invalid behavior");
  }
  uint64_t end = phosg::now();
  string time_str = phosg::format_duration(end - start);

  float size_ratio = static_cast<float>(data.size() * 100) / input_bytes;
  double bytes_per_sec = input_bytes / (static_cast<double>(end - start) / 1000000.0);
  string bytes_per_sec_str = phosg::format_size(bytes_per_sec);
  phosg::log_info_f("{} (0x{:X}) bytes input => {} (0x{:X}) bytes output ({:g}%) in {} ({} / sec)",
      input_bytes, input_bytes, data.size(), data.size(), size_ratio, time_str, bytes_per_sec_str);

  if (is_pr2 || is_prc) {
    if (is_decompress && (data.size() != pr2_expected_size)) {
      phosg::log_warning_f("Result data size ({} bytes) does not match expected size from PR2 header ({} bytes)", data.size(), pr2_expected_size);
    } else if (!is_decompress) {
      uint32_t pr2_seed = seed.empty() ? phosg::random_object<uint32_t>() : stoul(seed, nullptr, 16);
      data = is_big_endian
          ? encrypt_pr2_data<true>(data, input_bytes, pr2_seed)
          : encrypt_pr2_data<false>(data, input_bytes, pr2_seed);
    }
  }

  const char* extension;
  if (is_decompress) {
    extension = "dec";
  } else if (is_prs) {
    extension = "prs";
  } else if (is_bc0) {
    extension = "bc0";
  } else if (is_prc) {
    extension = "prc";
  } else if (is_pr2) {
    extension = "pr2";
  } else {
    throw logic_error("unknown action");
  }
  write_output_data(args, data.data(), data.size(), extension);
}

Action a_compress_prs("compress-prs", nullptr, a_compress_decompress_fn);
Action a_compress_bc0("compress-bc0", nullptr, a_compress_decompress_fn);
Action a_compress_pr2("compress-pr2", nullptr, a_compress_decompress_fn);
Action a_compress_prc("compress-prc", "\
  compress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-pr2 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-prc [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Compress data using the PRS, PR2, PRC, or BC0 algorithms. By default, the\n\
    heuristic-based compressor is used, which gives a good balance between\n\
    memory usage, CPU usage, and output size. For PRS and PR2, this compressor\n\
    can be tuned with the --compression-level=N option, which specifies how\n\
    aggressive the compressor should be in searching for literal sequences. The\n\
    default level is 0; a higher value generally means slower compression and a\n\
    smaller output size. If the compression level is -1, the input data is\n\
    encoded in a PRS-compatible format but not actually compressed, resulting\n\
    in valid PRS data which is about 9/8 the size of the input.\n\
    There is also a compressor which produces the absolute smallest output\n\
    size, but uses much more memory and CPU time. To use this compressor, use\n\
    the --optimal option.\n",
    a_compress_decompress_fn);
Action a_decompress_prs("decompress-prs", nullptr, a_compress_decompress_fn);
Action a_decompress_bc0("decompress-bc0", nullptr, a_compress_decompress_fn);
Action a_decompress_pr2("decompress-pr2", nullptr, a_compress_decompress_fn);
Action a_decompress_prc("decompress-prc", "\
  decompress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-pr2 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-prc [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decompress data compressed using the PRS, PR2, PRC, or BC0 algorithms.\n",
    a_compress_decompress_fn);

Action a_prs_size(
    "prs-size", "\
  prs-size [INPUT-FILENAME]\n\
    Compute the decompressed size of the PRS-compressed input data, but don\'t\n\
    write the decompressed data anywhere.\n",
    +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      size_t input_bytes = data.size();
      size_t output_bytes = prs_decompress_size(data);
      phosg::log_info_f("{} (0x{:X}) bytes input => {} (0x{:X}) bytes output",
          input_bytes, input_bytes, output_bytes, output_bytes);
    });

Action a_disassemble_prs(
    "disassemble-prs", nullptr, +[](phosg::Arguments& args) {
      prs_disassemble(stdout, read_input_data(args));
    });
Action a_disassemble_bc0(
    "disassemble-bc0", "\
  disassemble-prs [INPUT-FILENAME]\n\
  disassemble-bc0 [INPUT-FILENAME]\n\
    Write a textual representation of the commands contained in a PRS or BC0\n\
    command stream. The output is written to stdout. This is mainly useful for\n\
    debugging the compressors and decompressors themselves.\n",
    +[](phosg::Arguments& args) {
      bc0_disassemble(stdout, read_input_data(args));
    });

static void a_encrypt_decrypt_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-data");
  string seed = args.get<string>("seed");
  bool is_big_endian = args.get<bool>("big-endian");
  auto version = get_cli_version(args);

  shared_ptr<PSOEncryption> crypt;
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE:
      crypt = make_shared<PSOV2Encryption>(stoul(seed, nullptr, 16));
      break;
    case Version::GC_V3:
    case Version::XB_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      crypt = make_shared<PSOV3Encryption>(stoul(seed, nullptr, 16));
      break;
    case Version::BB_V4: {
      string key_name = args.get<string>("key");
      if (key_name.empty()) {
        throw runtime_error("the --key option is required for BB");
      }
      seed = phosg::parse_data_string(seed, nullptr, phosg::ParseDataFlags::ALLOW_FILES);
      auto key = phosg::load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + key_name + ".nsk");
      crypt = make_shared<PSOBBEncryption>(key, seed.data(), seed.size());
      break;
    }
    default:
      throw logic_error("invalid game version");
  }

  string data = read_input_data(args);

  size_t original_size = data.size();
  data.resize((data.size() + 7) & (~7), '\0');

  if (is_big_endian) {
    uint32_t* dwords = reinterpret_cast<uint32_t*>(data.data());
    for (size_t x = 0; x < (data.size() >> 2); x++) {
      dwords[x] = phosg::bswap32(dwords[x]);
    }
  }

  if (is_decrypt) {
    crypt->decrypt(data.data(), data.size());
  } else {
    crypt->encrypt(data.data(), data.size());
  }

  if (is_big_endian) {
    uint32_t* dwords = reinterpret_cast<uint32_t*>(data.data());
    for (size_t x = 0; x < (data.size() >> 2); x++) {
      dwords[x] = phosg::bswap32(dwords[x]);
    }
  }

  data.resize(original_size);

  write_output_data(args, data.data(), data.size(), "dec");
}

Action a_encrypt_data("encrypt-data", nullptr, a_encrypt_decrypt_fn);
Action a_decrypt_data("decrypt-data", "\
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
    file formats.\n",
    a_encrypt_decrypt_fn);

static void a_encrypt_decrypt_trivial_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-trivial-data");
  string seed = args.get<string>("seed");

  if (seed.empty() && !is_decrypt) {
    throw logic_error("--seed is required when encrypting data");
  }
  string data = read_input_data(args);
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
    phosg::fwrite_fmt(stderr, "Basis appears to be {:02X} ({} zero bytes in output)\n",
        best_seed, best_seed_score);
    basis = best_seed;
  } else {
    basis = stoul(seed, nullptr, 16);
  }
  decrypt_trivial_gci_data(data.data(), data.size(), basis);
  write_output_data(args, data.data(), data.size(), "dec");
}

Action a_encrypt_trivial_data("encrypt-trivial-data", nullptr, a_encrypt_decrypt_trivial_fn);
Action a_decrypt_trivial_data("decrypt-trivial-data", "\
  encrypt-trivial-data --seed=BASIS [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-trivial-data [--seed=BASIS] [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt data using the Episode 3 trivial algorithm. When\n\
    encrypting, --seed=BASIS is required; BASIS should be a single byte\n\
    specified in hexadecimal. When decrypting, BASIS should be specified the\n\
    same way, but if it is not given, newserv will try all possible basis\n\
    values and return the one that results in the greatest number of zero bytes\n\
    in the output.\n",
    a_encrypt_decrypt_trivial_fn);

Action a_decrypt_registry_value(
    "decrypt-registry-value", nullptr, +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      string out_data = decrypt_v2_registry_value(data.data(), data.size());
      write_output_data(args, out_data.data(), out_data.size(), "dec");
    });

Action a_parse_pc_v2_registry(
    "parse-pc-v2-registry", "\
  parse-pc-v2-registry [INPUT-FILENAME]\n\
    Decrypt and show the encrypted serial number, access key, and email fields\n\
    from the given registry export. The input file should be a .reg file\n\
    exported from the HKEY_CURRENT_USER\\Software\\SonicTeam\\PSOV2 key.\n",
    +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      if (data.starts_with("\xFF\xFE")) {
        data = tt_utf16_to_utf8(data.substr(2));
      }
      data = phosg::str_replace_all(data, "\r", "");
      data = phosg::str_replace_all(data, "\\\n", "");

      bool in_psov2_section = false;
      string serial_data, access_data, email_data;
      for (string line : phosg::split(data, '\n')) {
        if (line.starts_with("[")) {
          in_psov2_section = (line == "[HKEY_CURRENT_USER\\Software\\SonicTeam\\PSOV2]");
        } else if (!in_psov2_section) {
          // Wrong section; skip the line
        } else if (line.starts_with("\"SERIAL\"=hex:")) {
          serial_data = phosg::parse_data_string(line.substr(13));
        } else if (line.starts_with("\"ACCESS\"=hex:")) {
          access_data = phosg::parse_data_string(line.substr(13));
        } else if (line.starts_with("\"E-MAIL\"=hex:")) {
          email_data = phosg::parse_data_string(line.substr(13));
        }
      }

      if (serial_data.size() != 8) {
        throw std::runtime_error("serial number data is missing or incorrect size");
      }
      if (access_data.size() != 8) {
        throw std::runtime_error("access key data is missing or incorrect size");
      }
      if (email_data.size() != 0x40) {
        throw std::runtime_error("email data is missing or incorrect size");
      }

      serial_data = decrypt_v2_registry_value(serial_data);
      access_data = decrypt_v2_registry_value(access_data);
      email_data = decrypt_v2_registry_value(email_data);
      uint32_t serial_number = stoul(serial_data, nullptr, 16);
      phosg::strip_trailing_zeroes(access_data);
      phosg::strip_trailing_zeroes(email_data);
      phosg::fwrite_fmt(stderr, "Serial number (decimal): {}\nSerial number (hex): {:08X}\nAccess key: {}\nEmail address: {}\n",
          serial_number, serial_number, access_data, email_data);
    });

Action a_generate_pc_v2_registry(
    "generate-pc-v2-registry", "\
  generate-pc-v2-registry <OPTIONS> [OUTPUT-FILENAME]\n\
    Generate a .reg file containing PSO PC v2 credentials suitable for\n\
    importing into the Windows registry. The following options are required:\n\
      --serial-number=SERIAL-NUMBER (decimal serial number)\n\
      --access-key=ACCESS-KEY (access key, 8 characters)\n\
      --email=EMAIL (email address)\n",
    +[](phosg::Arguments& args) {
      auto hex_str_for_data = +[](const string& data) -> string {
        if (data.size() == 0) {
          return string();
        }
        string ret = std::format("{:02x}", data[0]);
        for (size_t z = 1; z < data.size(); z++) {
          ret += std::format(",{:02x}", data[z]);
        }
        return ret;
      };

      uint32_t serial_number = args.get<uint32_t>("serial-number", 0);
      string access_key = args.get<string>("access-key", true);
      string email = args.get<string>("email", true);
      if (access_key.size() != 8) {
        throw std::runtime_error("access key is not exactly 8 characters");
      }
      if (email.size() > 0x40) {
        throw std::runtime_error("email address is too long");
      }
      email.resize(0x40, '\0');

      string serial_data = decrypt_v2_registry_value(std::format("{:08X}", serial_number));
      string access_data = decrypt_v2_registry_value(access_key);
      string email_data = decrypt_v2_registry_value(email);

      string serial_hex = hex_str_for_data(serial_data);
      string access_hex = hex_str_for_data(access_data);
      string email_hex = hex_str_for_data(email_data);

      string output_data = std::format("Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\SonicTeam\\PSOV2]\r\n\r\n\"SERIAL\"=hex:{}\r\n\"ACCESS\"=hex:{}\r\n\"E-MAIL\"=hex:{}\r\n",
          serial_hex, access_hex, email_hex);

      write_output_data(args, output_data.data(), output_data.size(), "reg");
    });

Action a_encrypt_challenge_time(
    "encrypt-challenge-time", nullptr, +[](phosg::Arguments& args) {
      uint16_t time = args.get<uint16_t>(1);
      uint32_t ret = encrypt_challenge_time(time);
      phosg::fwrite_fmt(stderr, "{} => {:08X}\n", phosg::format_duration(time * 1000000), ret);
    });
Action a_decrypt_challenge_time(
    "decrypt-challenge-time", nullptr, +[](phosg::Arguments& args) {
      uint32_t time = args.get<uint32_t>(1, phosg::Arguments::IntFormat::HEX);
      uint16_t ret = decrypt_challenge_time(time);
      phosg::fwrite_fmt(stderr, "{:08X} => {}\n", time, phosg::format_duration(ret * 1000000));
    });

Action a_encrypt_challenge_data(
    "encrypt-challenge-data", nullptr, +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      encrypt_challenge_rank_text_t<uint8_t>(data.data(), data.size());
      write_output_data(args, data.data(), data.size(), "dec");
    });
Action a_decrypt_challenge_data(
    "decrypt-challenge-data", "\
  encrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt data using the challenge mode trivial algorithm.\n",
    +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      decrypt_challenge_rank_text_t<uint8_t>(data.data(), data.size());
      write_output_data(args, data.data(), data.size(), "dec");
    });

static void a_encrypt_decrypt_vms_save_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-vms-save");
  bool skip_checksum = args.get<bool>("skip-checksum");
  string serial_number_str = args.get<string>("serial-number");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, phosg::Arguments::IntFormat::HEX);

  int64_t round1_seed = serial_number_str.empty() ? -1 : stoul(serial_number_str, nullptr, 16);

  auto data = read_input_data(args);
  phosg::StringReader r(data);
  const auto& header = r.get<PSOVMSFileHeader>();
  header.check();
  r.skip(header.icon_data_size());

  size_t data_start_offset = r.where();

  auto process_file = [&]<typename StructT, bool UseIterator, size_t ChecksumLength = sizeof(StructT)>() {
    if (is_decrypt) {
      const void* data_section = r.getv(header.data_size);
      if (round1_seed < 0) {
        size_t num_threads = args.get<size_t>("threads", 0);
        if (num_threads == 0) {
          num_threads = thread::hardware_concurrency();
        }

        mutex output_lock;
        if (UseIterator) {
          DCSerialNumberIterator iter;
          mutex iter_lock;
          atomic<bool> seed_found = false;
          auto thread_fn = [&]() -> void {
            for (;;) {
              uint32_t serial_number;
              {
                lock_guard g(iter_lock);
                serial_number = iter.next();
              }
              if (serial_number == 0) {
                return;
              }
              try {
                auto decrypted = decrypt_fixed_size_data_section_t<StructT, false, ChecksumLength>(
                    data_section, sizeof(StructT), serial_number, skip_checksum, override_round2_seed);

                seed_found = true;
                {
                  lock_guard g(iter_lock);
                  iter.complete = true;
                }
                lock_guard g(output_lock);
                phosg::fwrite_fmt(stderr, "\nFound serial number: {:08X}\n", serial_number);
                *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;

              } catch (const runtime_error&) {
              }
            }
          };

          vector<thread> threads;
          while (threads.size() < num_threads) {
            threads.emplace_back(thread_fn);
          }
          for (;;) {
            usleep(1000000);
            lock_guard g(iter_lock);
            size_t progress = iter.progress();
            size_t total_count = iter.total_count();
            float progress_percent = static_cast<float>(progress * 100) / total_count;
            phosg::fwrite_fmt(stderr, "... {}/{} ({:g}%, domain {:02X}, subdomain {:02X}, index2 {:04X}, index3 {:04X})\r",
                progress, total_count, progress_percent, iter.domain, iter.subdomain, iter.index2, iter.index3);
            if (iter.complete) {
              break;
            }
          }
          for (auto& th : threads) {
            th.join();
          }
          if (!seed_found) {
            throw runtime_error("no seed found");
          }

        } else {
          uint64_t seed = phosg::parallel_range_blocks<uint64_t>([&](uint64_t serial_number, size_t) -> bool {
            try {
              auto decrypted = decrypt_fixed_size_data_section_t<StructT, false, ChecksumLength>(
                  data_section, sizeof(StructT), serial_number, skip_checksum, override_round2_seed);

              lock_guard g(output_lock);
              phosg::fwrite_fmt(stderr, "\nFound serial number: {:08X}\n", serial_number);
              *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;
              return true;

            } catch (const runtime_error&) {
              return false;
            }
          },
              0, 0x100000000, 0x1000, num_threads);
          if (seed >= 0x100000000) {
            throw runtime_error("no seed found");
          }
        }

      } else {
        auto decrypted = decrypt_fixed_size_data_section_t<StructT, false, ChecksumLength>(
            data_section, sizeof(StructT), round1_seed, skip_checksum, override_round2_seed);
        *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;
      }

    } else {
      const auto& s = r.get<StructT>();
      auto encrypted = encrypt_fixed_size_data_section_t<StructT, false, ChecksumLength>(s, round1_seed);
      if (data_start_offset + encrypted.size() > data.size()) {
        throw runtime_error("encrypted result exceeds file size");
      }
      memcpy(data.data() + data_start_offset, encrypted.data(), encrypted.size());
    }
  };

  bool is_v2 = header.is_v2();
  if (!is_v2 && (header.data_size == sizeof(PSODCNTECharacterFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC NTE character\n");
    process_file.template operator()<PSODCNTECharacterFile, false>();
  } else if (!is_v2 && (header.data_size == sizeof(PSODCNTEGuildCardFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC NTE Guild Card list\n");
    throw runtime_error("DC NTE Guild Card files are not encrypted");
  } else if (!is_v2 && (header.data_size == sizeof(PSODC112000CharacterFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC 11/2000 character\n");
    process_file.template operator()<PSODC112000CharacterFile, false>();
  } else if (!is_v2 && (header.data_size == sizeof(PSODC112000GuildCardFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC 11/2000 Guild Card list\n");
    throw runtime_error("DC 11/2000 Guild Card files are not encrypted");
  } else if (!is_v2 && (header.data_size == sizeof(PSODCV1CharacterFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC v1 character\n");
    process_file.template operator()<PSODCV1CharacterFile, true>();
  } else if (is_v2 && (header.data_size == sizeof(PSODCV2CharacterFile))) {
    phosg::fwrite_fmt(stderr, "File type: DC v2 character\n");
    process_file.template operator()<PSODCV2CharacterFile, true>();
  } else if (header.data_size == sizeof(PSODCV1V2GuildCardFile)) {
    // There appears to be a copy/paste error here: the game uses the character
    // file size when checksumming the Guild Card file, so we must do the same
    if (!is_v2) {
      phosg::fwrite_fmt(stderr, "File type: DC v1 Guild Card list\n");
      static_assert(sizeof(PSODCV1CharacterFile) <= sizeof(PSODCV1V2GuildCardFile::EncryptedSection));
      process_file.template operator()<PSODCV1V2GuildCardFile::EncryptedSection, true, sizeof(PSODCV1CharacterFile)>();
    } else {
      phosg::fwrite_fmt(stderr, "File type: DC v2 Guild Card list\n");
      static_assert(sizeof(PSODCV2CharacterFile) <= sizeof(PSODCV1V2GuildCardFile::EncryptedSection));
      process_file.template operator()<PSODCV1V2GuildCardFile::EncryptedSection, true, sizeof(PSODCV2CharacterFile)>();
    }
  } else {
    throw runtime_error("unrecognized save type");
  }

  write_output_data(args, data.data(), data.size(), is_decrypt ? "vmsd" : "vms");
}

Action a_decrypt_vms_save("decrypt-vms-save", nullptr, a_encrypt_decrypt_vms_save_fn);
Action a_encrypt_vms_save("encrypt-vms-save", "\
  encrypt-vms-save --seed=SEED INPUT-FILENAME [OUTPUT-FILENAME]\n\
  decrypt-vms-save [--seed=SEED] INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Encrypt or decrypt a character or Guild Card file in VMS format. If\n\
    encrypting, the checksum is also recomputed and stored in the encrypted\n\
    file. --seed is the encryption seed (serial number) specified as a 32-bit\n\
    hexadecimal value.\n",
    a_encrypt_decrypt_vms_save_fn);

static void a_encrypt_decrypt_pc_save_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-pc-save");
  bool skip_checksum = args.get<bool>("skip-checksum");
  string seed = args.get<string>("seed");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, phosg::Arguments::IntFormat::HEX);

  if (seed.empty()) {
    throw runtime_error("--seed must be given to specify the serial number");
  }
  uint32_t round1_seed = stoul(seed, nullptr, 16);

  auto data = read_input_data(args);
  if (data.size() == sizeof(PSOPCGuildCardFile)) {
    if (is_decrypt) {
      data = decrypt_fixed_size_data_section_s<false>(
          data.data(), offsetof(PSOPCGuildCardFile, end_padding), round1_seed, skip_checksum, override_round2_seed);
    } else {
      data = encrypt_fixed_size_data_section_s<false>(
          data.data(), offsetof(PSOPCGuildCardFile, end_padding), round1_seed);
    }
    data.resize((sizeof(PSOPCGuildCardFile) + 0x1FF) & (~0x1FF), '\0');
  } else if (data.size() == sizeof(PSOPCCharacterFile)) {
    PSOPCCharacterFile* charfile = reinterpret_cast<PSOPCCharacterFile*>(data.data());
    if (is_decrypt) {
      for (size_t z = 0; z < charfile->entries.size(); z++) {
        if (charfile->entries[z].present) {
          try {
            charfile->entries[z].encrypted = decrypt_fixed_size_data_section_t<PSOPCCharacterFile::CharacterEntry::EncryptedSection, false>(
                &charfile->entries[z].encrypted, sizeof(charfile->entries[z].encrypted), round1_seed, skip_checksum, override_round2_seed);
          } catch (const exception& e) {
            phosg::fwrite_fmt(stderr, "warning: cannot decrypt character {}: {}\n", z, e.what());
          }
        }
      }
    } else {
      for (size_t z = 0; z < charfile->entries.size(); z++) {
        if (charfile->entries[z].present) {
          string encrypted = encrypt_fixed_size_data_section_t<PSOPCCharacterFile::CharacterEntry::EncryptedSection, false>(
              charfile->entries[z].encrypted, round1_seed);
          if (encrypted.size() != sizeof(PSOPCCharacterFile::CharacterEntry::EncryptedSection)) {
            throw logic_error("incorrect encrypted result size");
          }
          charfile->entries[z].encrypted = *reinterpret_cast<const PSOPCCharacterFile::CharacterEntry::EncryptedSection*>(encrypted.data());
        }
      }
    }
  } else if (data.size() == sizeof(PSOPCCreationTimeFile)) {
    throw runtime_error("the PSO______FLS file is not encrypted; it is just random data");
  } else if (data.size() == sizeof(PSOPCSystemFile)) {
    throw runtime_error("the PSO______COM file is not encrypted");
  } else {
    throw runtime_error("unknown save file type");
  }

  write_output_data(args, data.data(), data.size(), "dec");
}

Action a_decrypt_pc_save("decrypt-pc-save", nullptr, a_encrypt_decrypt_pc_save_fn);
Action a_encrypt_pc_save("encrypt-pc-save", "\
  encrypt-pc-save --seed=SEED [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-pc-save --seed=SEED [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt a PSO PC character file (PSO______SYS or PSO______SYD)\n\
    or Guild Card file (PSO______GUD). SEED should be the serial number\n\
    associated with the save file, as a 32-bit hexadecimal integer.\n",
    a_encrypt_decrypt_pc_save_fn);

static void a_encrypt_decrypt_save_data_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-save-data");
  bool skip_checksum = args.get<bool>("skip-checksum");
  bool is_big_endian = args.get<bool>("big-endian");
  string seed = args.get<string>("seed");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, phosg::Arguments::IntFormat::HEX);
  size_t bytes = args.get<size_t>("bytes", 0);

  if (seed.empty()) {
    throw runtime_error("--seed must be given to specify the round1 seed");
  }
  uint32_t round1_seed = stoul(seed, nullptr, 16);

  auto data = read_input_data(args);
  phosg::StringReader r(data);

  string output_data;
  size_t effective_size = bytes ? min<size_t>(bytes, data.size()) : data.size();
  if (is_decrypt) {
    output_data = is_big_endian
        ? decrypt_fixed_size_data_section_s<true>(data.data(), effective_size, round1_seed, skip_checksum, override_round2_seed)
        : decrypt_fixed_size_data_section_s<false>(data.data(), effective_size, round1_seed, skip_checksum, override_round2_seed);
  } else {
    output_data = is_big_endian
        ? encrypt_fixed_size_data_section_s<true>(data.data(), effective_size, round1_seed)
        : encrypt_fixed_size_data_section_s<false>(data.data(), effective_size, round1_seed);
  }
  write_output_data(args, output_data.data(), output_data.size(), "dec");
}

static void a_encrypt_decrypt_gci_save_fn(phosg::Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-gci-save");
  bool skip_checksum = args.get<bool>("skip-checksum");
  string seed = args.get<string>("seed");
  string system_filename = args.get<string>("sys");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, phosg::Arguments::IntFormat::HEX);

  uint32_t round1_seed;
  if (!system_filename.empty()) {
    string system_data = phosg::load_file(system_filename);
    phosg::StringReader r(system_data);
    const auto& header = r.get<PSOGCIFileHeader>();
    header.check();
    const auto& system = r.get<PSOGCSystemFile>();
    round1_seed = system.creation_timestamp;
  } else if (!seed.empty()) {
    round1_seed = stoul(seed, nullptr, 16);
  } else {
    throw runtime_error("either --sys or --seed must be given");
  }

  auto data = read_input_data(args);
  phosg::StringReader r(data);
  const auto& header = r.get<PSOGCIFileHeader>();
  header.check();

  size_t data_start_offset = r.where();

  auto process_file = [&]<typename StructT>() {
    if (is_decrypt) {
      const void* data_section = r.getv(header.data_size);
      auto decrypted = decrypt_fixed_size_data_section_t<StructT, true>(
          data_section, header.data_size, round1_seed, skip_checksum, override_round2_seed);
      *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;
    } else {
      const auto& s = r.get<StructT>();
      auto encrypted = encrypt_fixed_size_data_section_t<StructT, true>(s, round1_seed);
      if (data_start_offset + encrypted.size() > data.size()) {
        throw runtime_error("encrypted result exceeds file size");
      }
      memcpy(data.data() + data_start_offset, encrypted.data(), encrypted.size());
    }
  };

  if (header.data_size == sizeof(PSOGCGuildCardFile)) {
    process_file.template operator()<PSOGCGuildCardFile>();
  } else if (header.is_ep12() && (header.data_size == sizeof(PSOGCCharacterFile))) {
    process_file.template operator()<PSOGCCharacterFile>();
  } else if (header.is_ep3() && (header.data_size == sizeof(PSOGCEp3CharacterFile))) {
    auto* charfile = reinterpret_cast<PSOGCEp3CharacterFile*>(data.data() + data_start_offset);
    if (!is_decrypt) {
      for (size_t z = 0; z < charfile->characters.size(); z++) {
        charfile->characters[z].ep3_config.encrypt(phosg::random_object<uint8_t>());
      }
    }
    process_file.template operator()<PSOGCEp3CharacterFile>();
    if (is_decrypt) {
      for (size_t z = 0; z < charfile->characters.size(); z++) {
        charfile->characters[z].ep3_config.decrypt();
      }
    }
  } else {
    throw runtime_error("unrecognized save type");
  }

  write_output_data(args, data.data(), data.size(), is_decrypt ? "gcid" : "gci");
}

Action a_decrypt_gci_save("decrypt-gci-save", nullptr, a_encrypt_decrypt_gci_save_fn);
Action a_encrypt_gci_save("encrypt-gci-save", "\
  encrypt-gci-save CRYPT-OPTION INPUT-FILENAME [OUTPUT-FILENAME]\n\
  decrypt-gci-save CRYPT-OPTION INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Encrypt or decrypt a character or Guild Card file in GCI format. If\n\
    encrypting, the checksum is also recomputed and stored in the encrypted\n\
    file. CRYPT-OPTION is required; it can be either --sys=SYSTEM-FILENAME\n\
    (specifying the name of the corresponding PSO_SYSTEM .gci file) or\n\
    --seed=ROUND1-SEED (specified as a 32-bit hexadecimal number).\n",
    a_encrypt_decrypt_gci_save_fn);

Action a_decrypt_xbox_save(
    "decrypt-xbox-save", "\
  decrypt-xbox-save CRYPT-OPTION INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Decrypt a character or Guild Card file in Xbox format. CRYPT-OPTION is\n\
    required; it can be either --sys=SYSTEM-FILENAME (specifying the name of\n\
    the corresponding PSO_SYSTEM file) or --seed=ROUND1-SEED (specified as a\n\
    32-bit hexadecimal number).\n",
    +[](phosg::Arguments& args) {
      bool skip_checksum = args.get<bool>("skip-checksum");
      string seed = args.get<string>("seed");
      string system_filename = args.get<string>("sys");
      int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, phosg::Arguments::IntFormat::HEX);

      uint32_t round1_seed;
      if (!system_filename.empty()) {
        string system_data = phosg::load_file(system_filename);
        phosg::StringReader r(system_data);
        const auto& header = r.get<PSOXBFileHeader>();
        header.check();
        const auto& system = r.get<PSOXBSystemFile>();
        round1_seed = system.creation_timestamp;
      } else if (!seed.empty()) {
        round1_seed = stoul(seed, nullptr, 16);
      } else {
        throw runtime_error("either --sys or --seed must be given");
      }

      auto data = read_input_data(args);
      phosg::StringReader r(data);
      const auto& header = r.get<PSOXBFileHeader>();
      header.check();

      size_t data_start_offset = r.where();

      auto process_file = [&]<typename StructT>() {
        const void* data_section = r.getv(header.data_size);
        auto decrypted = decrypt_fixed_size_data_section_t<StructT, false>(
            data_section, header.data_size, round1_seed, skip_checksum, override_round2_seed);
        *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;
      };

      if (header.data_size == sizeof(PSOXBGuildCardFile)) {
        process_file.template operator()<PSOXBGuildCardFile>();
      } else if (header.data_size == sizeof(PSOXBCharacterFile)) {
        process_file.template operator()<PSOXBCharacterFile>();
      } else {
        throw runtime_error("unrecognized save type");
      }

      write_output_data(args, data.data(), data.size(), "dec");
    });

// TODO: Write usage text for these actions
Action a_decrypt_save_data("decrypt-save-data", nullptr, a_encrypt_decrypt_save_data_fn);
Action a_encrypt_save_data("encrypt-save-data", nullptr, a_encrypt_decrypt_save_data_fn);

Action a_decrypt_dcv2_executable(
    "decrypt-dcv2-executable", "\
  decrypt-dcv2-executable --executable=EXEC --indexes=INDEXES --values=VALUES\n\
  decrypt-dcv2-executable --executable=EXEC --simple [--seed=SEED]\n\
    Decrypt a PSO DC v2 executable file. EXEC should be the path to the\n\
    executable (DP_ADDRESS.JPN), INDEXES should be the path to the index fixup\n\
    table (KATSUO.SEA), and VALUES should be the path to the value fixup table\n\
    (IWASHI.SEA). The output is written to EXEC.dec.\n\
    If --simple is given, uses the simpler encryption method used in some\n\
    community modifications of the game (Enhancement Pack, for example). In\n\
    this case, --seed is not required; if not given, finds the seed\n\
    automatically, and prints it to stderr so you will be able to use it when\n\
    re-encrypting.\n",
    +[](phosg::Arguments& args) {
      string executable_filename = args.get<string>("executable", true);
      string executable_data = phosg::load_file(executable_filename);
      string decrypted;
      if (args.get<bool>("simple")) {
        string seed_str = args.get<string>("seed");
        int64_t seed = seed_str.empty() ? -1 : stoull(seed_str, nullptr, 16);
        decrypted = crypt_dp_address_jpn_simple(executable_data, seed);
      } else {
        string values_filename = args.get<string>("values", true);
        string indexes_filename = args.get<string>("indexes", true);
        string values_data = phosg::load_file(values_filename);
        string indexes_data = phosg::load_file(indexes_filename);
        decrypted = decrypt_dp_address_jpn(executable_data, values_data, indexes_data);
      }
      phosg::save_file(executable_filename + ".dec", decrypted);
    });
Action a_encrypt_dcv2_executable(
    "encrypt-dcv2-executable", "\
  encrypt-dcv2-executable --executable=EXEC --indexes=INDEXES\n\
  encrypt-dcv2-executable --executable=EXEC --simple --seed=SEED\n\
    Encrypt a PSO DC v2 executable file. EXEC should be the path to the\n\
    executable (DP_ADDRESS.JPN) and INDEXES should be the path to the index\n\
    fixup table (KATSUO.SEA). The output is written to EXEC.enc and\n\
    INDEXES.enc.\n\
    If --simple is given, uses the simpler encryption method used in some\n\
    community modifications of the game. In this case, --seed is required.\n",
    +[](phosg::Arguments& args) {
      string executable_filename = args.get<string>("executable", true);
      string executable_data = phosg::load_file(executable_filename);
      string encrypted_executable;
      if (args.get<bool>("simple")) {
        int64_t seed = stoull(args.get<string>("seed", true), nullptr, 16);
        encrypted_executable = crypt_dp_address_jpn_simple(executable_data, seed);
      } else {
        string indexes_filename = args.get<string>("indexes", true);
        string indexes_data = phosg::load_file(indexes_filename);
        auto encrypted = encrypt_dp_address_jpn(executable_data, indexes_data);
        phosg::save_file(indexes_filename + ".enc", encrypted.indexes);
        encrypted_executable = std::move(encrypted.executable);
      }
      phosg::save_file(executable_filename + ".enc", encrypted_executable);
    });

Action a_decode_gci_snapshot(
    "decode-gci-snapshot", "\
  decode-gci-snapshot [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a PSO GC snapshot file into a Windows BMP image.\n",
    +[](phosg::Arguments& args) {
      auto data = read_input_data(args);
      phosg::StringReader r(data);
      const auto& header = r.get<PSOGCIFileHeader>();
      try {
        header.check();
      } catch (const exception& e) {
        phosg::log_warning_f("File header failed validation ({})", e.what());
      }
      const auto& file = r.get<PSOGCSnapshotFile>();
      if (!file.checksum_correct()) {
        phosg::log_warning_f("File internal checksum is incorrect");
      }

      auto img = file.decode_image();
      string saved = img.serialize(phosg::ImageFormat::WINDOWS_BITMAP);
      write_output_data(args, saved.data(), saved.size(), "bmp");
    });

Action a_encode_gvm(
    "encode-gvm", "\
  encode-gvm [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encode an image in BMP or PPM/PNM format into a GVM texture. The resulting\n\
    GVM file can be used as an Episode 3 lobby banner.\n",
    +[](phosg::Arguments& args) {
      const string& input_filename = args.get<string>(1, false);
      string data;
      if (!input_filename.empty() && (input_filename != "-")) {
        data = phosg::load_file(input_filename);
      } else {
        data = phosg::read_all(stdin);
      }
      auto img = phosg::ImageRGBA8888N::from_file_data(data);
      // If the image has any transparent pixels at all, use RGB5A3
      string encoded = encode_gvm(
          img, has_any_transparent_pixels(img) ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565, "image.gvr", 0);
      write_output_data(args, encoded.data(), encoded.size(), "gvm");
    });

Action a_decode_bitmap_font(
    "decode-bitmap-font", "\
  decode-bitmap-font --width=WIDTH [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a 2-bit bitmap font file (.fon) into a BMP image. The --width\n\
    option is required; if the output looks wrong, try increasing or\n\
    decreasing this number. For S18all04.fon, the width should be 20.\n",
    +[](phosg::Arguments& args) {
      std::string data = read_input_data(args);
      size_t width = args.get<size_t>("width");
      std::string bmp_data = decode_fon(data, width).serialize(phosg::ImageFormat::WINDOWS_BITMAP);
      write_output_data(args, bmp_data.data(), bmp_data.size(), "bmp");
    });
Action a_encode_bitmap_font(
    "encode-bitmap-font", "\
  encode-bitmap-font [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encode an image in BMP or PPM/PNM format into a bitmap font file for use\n\
    with the console PSO versions. The image dimensions must match the\n\
    original fon\'s dimensions.\n",
    +[](phosg::Arguments& args) {
      const string& input_filename = args.get<string>(1, false);
      string data;
      if (!input_filename.empty() && (input_filename != "-")) {
        data = phosg::load_file(input_filename);
      } else {
        data = phosg::read_all(stdin);
      }
      auto img = phosg::ImageRGB888::from_file_data(data);
      string encoded = encode_fon(img);
      write_output_data(args, encoded.data(), encoded.size(), "fon");
    });

Action a_salvage_gci(
    "salvage-gci", "\
  salvage-gci INPUT-FILENAME [--round2] [CRYPT-OPTION] [--bytes=SIZE]\n\
    Attempt to find either the round-1 or round-2 decryption seed for a\n\
    corrupted GCI file. If --round2 is given, then CRYPT-OPTION must be given\n\
    (and should specify either a valid system file or the round1 seed).\n",
    +[](phosg::Arguments& args) {
      bool round2 = args.get<bool>("round2");
      bool exhaustive = args.get<bool>("exhaustive");
      string seed = args.get<string>("seed");
      string system_filename = args.get<string>("sys");
      size_t num_threads = args.get<size_t>("threads", 0);
      size_t offset = args.get<size_t>("offset", 0);
      size_t stride = args.get<size_t>("stride", 1);
      size_t bytes = args.get<size_t>("bytes", 0);

      uint64_t likely_round1_seed = 0xFFFFFFFFFFFFFFFF;
      if (!system_filename.empty()) {
        try {
          string system_data = phosg::load_file(system_filename);
          phosg::StringReader r(system_data);
          const auto& header = r.get<PSOGCIFileHeader>();
          header.check();
          const auto& system = r.get<PSOGCSystemFile>();
          likely_round1_seed = system.creation_timestamp;
          phosg::log_info_f("System file appears to be in order; round1 seed is {:08X}", likely_round1_seed);
        } catch (const exception& e) {
          phosg::log_warning_f("Cannot parse system file ({}); ignoring it", e.what());
        }
      } else if (!seed.empty()) {
        likely_round1_seed = stoul(seed, nullptr, 16);
        phosg::log_info_f("Specified round1 seed is {:08X}", likely_round1_seed);
      }

      if (round2 && likely_round1_seed > 0x100000000) {
        throw invalid_argument("cannot find round2 seed without known round1 seed");
      }

      auto data = read_input_data(args);
      phosg::StringReader r(data);
      const auto& header = r.get<PSOGCIFileHeader>();
      header.check();

      const void* data_section = r.getv(header.data_size);

      string round1_decrypted;
      if (round2) {
        round1_decrypted = decrypt_data_section<true>(data_section, header.data_size, likely_round1_seed, 0);
        if (bytes > 0) {
          round1_decrypted.resize(bytes);
        }
      }

      auto process_file = [&]<typename StructT>() {
        vector<multimap<size_t, uint32_t>> top_seeds_by_thread(
            num_threads ? num_threads : thread::hardware_concurrency());
        auto add_top_seed = +[](multimap<size_t, uint32_t>& top_seeds, uint32_t seed, size_t zero_count) -> void {
          if (top_seeds.size() < 10 || (zero_count >= top_seeds.begin()->first)) {
            top_seeds.emplace(zero_count, seed);
            if (top_seeds.size() > 10) {
              top_seeds.erase(top_seeds.begin());
            }
          }
        };
        auto merge_top_seeds = +[](const vector<multimap<size_t, uint32_t>>& top_seeds_by_thread) -> multimap<size_t, uint32_t> {
          multimap<size_t, uint32_t> ret;
          for (const auto& thread_top_seeds : top_seeds_by_thread) {
            for (const auto& it : thread_top_seeds) {
              ret.emplace(it.first, it.second);
            }
          }
          return ret;
        };
        auto print_top_seeds = [&](const multimap<size_t, uint32_t>& top_seeds) -> void {
          for (const auto& it : top_seeds) {
            const char* sys_seed_str = (!round2 && (it.second == likely_round1_seed))
                ? " (this is the seed from the system file)"
                : "";
            phosg::log_info_f("Round {} seed {:08X} resulted in {} zero bytes{}",
                round2 ? '2' : '1', it.second, it.first, sys_seed_str);
          }
        };

        uint32_t round2_lower_half = 0;
        auto try_round2_seed = [&](uint64_t seed, size_t thread_num) -> bool {
          seed |= round2_lower_half;
          string decrypted = round1_decrypted;
          PSOV2Encryption(seed).encrypt_big_endian(decrypted.data(), decrypted.size());
          size_t zero_count = phosg::count_zeroes(decrypted.data() + offset, decrypted.size() - offset, stride);
          add_top_seed(top_seeds_by_thread[thread_num], seed, zero_count);
          return false;
        };

        if (!round2) {
          phosg::parallel_range_blocks<uint64_t>(
              [&](uint64_t seed, size_t thread_num) -> bool {
                auto decrypted = decrypt_fixed_size_data_section_t<StructT, true>(
                    data_section, header.data_size, seed, true);
                size_t zero_count = phosg::count_zeroes(
                    reinterpret_cast<const uint8_t*>(&decrypted) + offset,
                    sizeof(decrypted) - offset,
                    stride);
                add_top_seed(top_seeds_by_thread[thread_num], seed, zero_count);
                return false;
              },
              0, 0x100000000, 0x1000, num_threads);

        } else if (!exhaustive) {
          // The pseudorandom number generator used by PSO to encrypt its save
          // files has a weakness: if the low bits of the seed are correct, the
          // low bits of each 32-bit integer in the plaintext will also be
          // correct, even if the high bits of the seed are wrong. Using this,
          // we can brute-force the low half of the seed, then the high half,
          // which is much faster than trying all possible seeds.
          // Unfortunately, this relies on the distribution of values in the
          // plaintext, so it only works for the round-2 seed - the decrypted
          // data after round 1 is still essentially random.
          phosg::parallel_range_blocks<uint64_t>(try_round2_seed, 0, 0x100000, 0x1000, num_threads);
          auto intermediate_top_seeds = merge_top_seeds(top_seeds_by_thread);
          if (intermediate_top_seeds.empty()) {
            throw logic_error("no intermediate seeds were found");
          }
          print_top_seeds(intermediate_top_seeds);
          round2_lower_half = intermediate_top_seeds.rbegin()->second & 0xFFFF;
          phosg::log_info_f("Lower half of seed is likely {:04X} ({} zero bytes)", round2_lower_half, intermediate_top_seeds.rbegin()->first);
          for (auto& top_seeds : top_seeds_by_thread) {
            top_seeds.clear();
          }
          phosg::parallel_range_blocks<uint64_t>(
              [&](uint64_t seed, size_t thread_num) -> bool {
                return try_round2_seed((seed << 16) | round2_lower_half, thread_num);
              },
              0, 0x10000, 0x80, num_threads);

        } else {
          // The user requested not to take any shortcuts, so burn a lot of CPU
          // power
          phosg::parallel_range_blocks<uint64_t>(try_round2_seed, 0, 0x100000000, 0x1000, num_threads);
        }

        print_top_seeds(merge_top_seeds(top_seeds_by_thread));
      };

      if (header.data_size == sizeof(PSOGCGuildCardFile)) {
        process_file.template operator()<PSOGCGuildCardFile>();
      } else if (header.is_ep12() && (header.data_size == sizeof(PSOGCCharacterFile))) {
        process_file.template operator()<PSOGCCharacterFile>();
      } else if (header.is_ep3() && (header.data_size == sizeof(PSOGCEp3CharacterFile))) {
        process_file.template operator()<PSOGCEp3CharacterFile>();
      } else {
        throw runtime_error("unrecognized save type");
      }
    });

Action a_find_decryption_seed(
    "find-decryption-seed", "\
  find-decryption-seed OPTIONS...\n\
    Perform a brute-force search for a decryption seed of the given data. The\n\
    ciphertext is specified with the --encrypted=DATA option and the expected\n\
    plaintext is specified with the --decrypted=DATA option. The plaintext may\n\
    include unmatched bytes (specified with the Phosg parse_data_string ?\n\
    operator), but overall it must be the same length as the ciphertext. By\n\
    default, this option uses PSO V3 encryption, but this can be overridden\n\
    with --pc. (BB encryption seeds are too long to be searched for with this\n\
    function.) By default, the number of worker threads is equal to the number\n\
    of CPU cores in the system, but this can be overridden with the\n\
    --threads=NUM-THREADS option.\n",
    +[](phosg::Arguments& args) {
      const auto& plaintexts_ascii = args.get_multi<string>("decrypted");
      const auto& ciphertext_ascii = args.get<string>("encrypted");
      auto version = get_cli_version(args);
      if (plaintexts_ascii.empty() || ciphertext_ascii.empty()) {
        throw runtime_error("both --encrypted and --decrypted must be specified");
      }
      if (uses_v4_encryption(version)) {
        throw runtime_error("--find-decryption-seed cannot be used for BB ciphers");
      }
      bool skip_little_endian = args.get<bool>("skip-little-endian");
      bool skip_big_endian = args.get<bool>("skip-big-endian");
      size_t num_threads = args.get<size_t>("threads", 0);

      size_t max_plaintext_size = 0;
      vector<pair<string, string>> plaintexts;
      for (const auto& plaintext_ascii : plaintexts_ascii) {
        string mask;
        string data = phosg::parse_data_string(plaintext_ascii, &mask, phosg::ParseDataFlags::ALLOW_FILES);
        if (data.size() != mask.size()) {
          throw logic_error("plaintext and mask are not the same size");
        }
        max_plaintext_size = max<size_t>(max_plaintext_size, data.size());
        plaintexts.emplace_back(std::move(data), std::move(mask));
      }
      string ciphertext = phosg::parse_data_string(ciphertext_ascii, nullptr, phosg::ParseDataFlags::ALLOW_FILES);

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

      uint64_t seed = phosg::parallel_range_blocks<uint64_t>([&](uint64_t seed, size_t) -> bool {
        string be_decrypt_buf = ciphertext.substr(0, max_plaintext_size);
        string le_decrypt_buf = ciphertext.substr(0, max_plaintext_size);
        if (uses_v3_encryption(version)) {
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
      },
          0, 0x100000000, 0x1000, num_threads);

      if (seed < 0x100000000) {
        phosg::log_info_f("Found seed {:08X}", seed);
      } else {
        phosg::log_error_f("No seed found");
      }
    });

Action a_decode_gci(
    "decode-gci", nullptr, +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      string seed = args.get<string>("seed");
      size_t num_threads = args.get<size_t>("threads", 0);
      bool skip_checksum = args.get<bool>("skip-checksum");
      int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
      auto decoded = decode_gci_data(read_input_data(args), num_threads, dec_seed, skip_checksum);
      phosg::save_file(input_filename + ".dec", decoded);
    });
Action a_decode_vms(
    "decode-vms", nullptr, +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      string seed = args.get<string>("seed");
      size_t num_threads = args.get<size_t>("threads", 0);
      bool skip_checksum = args.get<bool>("skip-checksum");
      int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
      auto decoded = decode_vms_data(read_input_data(args), num_threads, dec_seed, skip_checksum);
      phosg::save_file(input_filename + ".dec", decoded);
    });
Action a_decode_dlq(
    "decode-dlq", nullptr, +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      auto decoded = decode_dlq_data(read_input_data(args));
      phosg::save_file(input_filename + ".dec", decoded);
    });
Action a_decode_qst(
    "decode-qst", "\
  decode-gci INPUT-FILENAME [OPTIONS...]\n\
  decode-vms INPUT-FILENAME [OPTIONS...]\n\
  decode-dlq INPUT-FILENAME [OPTIONS...]\n\
  decode-qst INPUT-FILENAME [OPTIONS...]\n\
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
    newserv will find it via a brute-force search, which will take a long time.\n",
    +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      bool decompress = args.get<bool>("decompress");
      auto files = decode_qst_data(read_input_data(args));
      for (const auto& it : files) {
        phosg::save_file(
            input_filename + "-" + it.first + (decompress ? "d" : ""),
            (decompress ? prs_decompress(it.second) : it.second));
      }
    });

Action a_encode_qst(
    "encode-qst", "\
  encode-qst INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS...]\n\
    Encode the input quest file (in .bin/.dat format) into a .qst file. If\n\
    --download is given, generates a download .qst instead of an online .qst.\n\
    Specify the quest\'s game version with one of the --dc-nte, --dc-v1,\n\
    --dc-v2, --pc, --gc-nte, --gc, --gc-ep3, --xb, or --bb options.\n",
    +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      auto version = get_cli_version(args);
      bool download = args.get<bool>("download");

      string bin_filename = input_filename;
      string dat_filename = bin_filename.ends_with(".bin")
          ? (bin_filename.substr(0, bin_filename.size() - 3) + "dat")
          : (bin_filename + ".dat");
      string pvr_filename = bin_filename.ends_with(".bin")
          ? (bin_filename.substr(0, bin_filename.size() - 3) + "pvr")
          : (bin_filename + ".pvr");
      auto bin_data = make_shared<string>(phosg::load_file(bin_filename));
      auto dat_data = make_shared<string>(phosg::load_file(dat_filename));
      shared_ptr<string> pvr_data;
      try {
        pvr_data = make_shared<string>(phosg::load_file(pvr_filename));
      } catch (const phosg::cannot_open_file&) {
      }

      auto vq = make_shared<VersionedQuest>();
      vq->version = version;
      vq->bin_contents = bin_data;
      vq->dat_contents = dat_data;
      vq->pvr_contents = pvr_data;
      if (download) {
        vq = vq->create_download_quest();
      }
      string qst_data = vq->encode_qst();

      write_output_data(args, qst_data.data(), qst_data.size(), "qst");
    });

Action a_check_quest_opcodes(
    "check-quest-opcodes", nullptr,
    +[](phosg::Arguments&) {
      check_opcode_definitions();
    });
Action a_disassemble_quest_script(
    "disassemble-quest-script", "\
  disassemble-quest-script [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Disassemble the input quest script (.bin file) into a text representation\n\
    of the commands and metadata it contains. Specify the quest\'s game version\n\
    with one of the --dc-nte, --dc-v1, --dc-v2, --pc, --gc-nte, --gc, --gc-ep3,\n\
    --xb, or --bb options. newserv uses more descriptive opcode mnemonics by\n\
    default; the --qedit option will result in names matching those used by\n\
    QEdit. If you intend to reassemble the script, after editing it, use the\n\
    --reassembly option to add explicit label numbers and remove offsets and\n\
    data in code sections.\n",
    +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      auto version = get_cli_version(args);
      if (!args.get<bool>("decompressed")) {
        data = prs_decompress(data);
      }
      uint8_t override_language = args.get<uint8_t>("language", 0xFF);
      bool reassembly_mode = args.get<bool>("reassembly");
      bool use_qedit_names = args.get<bool>("qedit");
      string result = disassemble_quest_script(data.data(), data.size(), version, override_language, reassembly_mode, use_qedit_names);
      write_output_data(args, result.data(), result.size(), "txt");
    });
Action a_disassemble_quest_map(
    "disassemble-quest-map", "\
  disassemble-quest-map [OPTIONS] [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Disassemble the input quest map (.dat file) into a text representation of\n\
    the data it contains. If --decompressed is given, don\'t decompress before\n\
    disassembling.\n",
    +[](phosg::Arguments& args) {
      auto data = make_shared<string>(read_input_data(args));
      if (!args.get<bool>("decompressed")) {
        *data = prs_decompress(*data);
      }
      bool reassembly = args.get<bool>("reassembly");
      string result = MapFile(data).disassemble(reassembly, get_cli_version(args, Version::UNKNOWN));
      write_output_data(args, result.data(), result.size(), "txt");
    });
Action a_disassemble_free_map(
    "disassemble-free-map", "\
  disassemble-free-map INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Disassemble the input free-play map (.dat or .evt file) into a text\n\
    representation of the data it contains. Unlike other disassembly actions,\n\
    this action expects its input to be already decompressed. If the input is\n\
    compressed, use the --compressed option. Also unlike other options, the\n\
    input must be from a file (that is, INPUT-FILENAME is required and cannot\n\
    be \"-\").\n",
    +[](phosg::Arguments& args) {
      const string& input_filename = args.get<string>(1, true);
      string input_filename_lower = phosg::tolower(input_filename);
      bool is_events = input_filename_lower.ends_with(".evt");
      bool is_enemies = input_filename_lower.ends_with("e.dat") || input_filename_lower.ends_with("e_s.dat") || input_filename_lower.ends_with("e_c1.dat") || input_filename_lower.ends_with("e_d.dat");
      bool is_objects = input_filename_lower.ends_with("o.dat") || input_filename_lower.ends_with("o_s.dat") || input_filename_lower.ends_with("o_c1.dat") || input_filename_lower.ends_with("o_d.dat");
      if (!is_objects && !is_enemies && !is_events) {
        throw runtime_error("cannot determine input file type");
      }

      auto data = make_shared<string>(read_input_data(args));
      if (args.get<bool>("compressed")) {
        *data = prs_decompress(*data);
      }

      uint8_t floor = args.get<uint8_t>("floor", 0);
      bool reassembly = args.get<bool>("reassembly");
      string result;
      if (is_objects) {
        result = MapFile(floor, data, nullptr, nullptr).disassemble(reassembly, get_cli_version(args, Version::UNKNOWN));
      } else if (is_enemies) {
        result = MapFile(floor, nullptr, data, nullptr).disassemble(reassembly, get_cli_version(args, Version::UNKNOWN));
      } else if (is_events) {
        result = MapFile(floor, nullptr, nullptr, data).disassemble(reassembly, get_cli_version(args, Version::UNKNOWN));
      } else {
        throw logic_error("unhandled input type");
      }
      result.push_back('\n');

      write_output_data(args, result.data(), result.size(), "txt");
    });
Action a_disassemble_set_data_table(
    "disassemble-set-data-table", "\
  disassemble-set-data-table [INPUT-FILENAME]\n\
    Show the contents of a SetDataTable.rel file. A version option is required.\n",
    +[](phosg::Arguments& args) {
      Version version = get_cli_version(args);
      SetDataTable sdt(version, read_input_data(args));
      string str = sdt.str();
      write_output_data(args, str.data(), str.size(), "txt");
    });

Action a_assemble_quest_script(
    "assemble-quest-script", "\
  assemble-quest-script [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Assemble the input quest script (.txt file) into a compressed .bin file\n\
    usable as an online quest script. If --decompressed is given, produces an\n\
    uncompressed .bind file instead.\n",
    +[](phosg::Arguments& args) {
      string text = read_input_data(args);

      const string& input_filename = args.get<string>(1, false);
      string include_dir = (!input_filename.empty() && (input_filename != "-"))
          ? phosg::dirname(input_filename)
          : ".";

      auto result = assemble_quest_script(
          text,
          {include_dir, "system/quests/includes"},
          {include_dir, "system/quests/includes", "system/client-functions/System"});
      string result_data = std::move(result.data);
      bool compress = !args.get<bool>("decompressed");
      if (compress) {
        if (args.get<bool>("optimal")) {
          result_data = prs_compress_optimal(result_data);
        } else {
          result_data = prs_compress(result_data);
        }
      }
      write_output_data(args, result_data.data(), result_data.size(), compress ? "bin" : "bind");
    });

Action a_assemble_all_patches(
    "assemble-all-patches", "\
  assemble-all-patches\n\
    Assemble all patches in the system/client-functions directory, and produce\n\
    two compiled .bin files for each patch (one unencrypted, for most PSO\n\
    versions, and one encrypted, for PSO GC JP v1.4, JP Ep3, and Ep3 Trial\n\
    Edition). The output files are saved in system/client-functions.\n",
    +[](phosg::Arguments&) {
      auto fci = make_shared<FunctionCodeIndex>("system/client-functions");

      auto process_code = +[](shared_ptr<const CompiledFunctionCode> code,
                               uint32_t checksum_addr,
                               uint32_t checksum_size,
                               uint32_t override_start_addr) -> void {
        for (uint8_t encrypted = 0; encrypted < 2; encrypted++) {
          phosg::StringWriter w;
          string data = prepare_send_function_call_data(
              code, {}, nullptr, 0, checksum_addr, checksum_size, override_start_addr, encrypted);
          w.put(PSOCommandHeaderDCV3{.command = 0xB2, .flag = 0x00, .size = data.size() + 4});
          w.write(data);
          string out_path = std::format("{}.{}.{}.bin",
              code->source_path, str_for_specific_version(code->specific_version), (encrypted ? "enc" : "std"));
          phosg::save_file(out_path, w.str());
          phosg::fwrite_fmt(stderr, "... {}\n", out_path);
        }
      };

      for (const auto& it : fci->name_and_specific_version_to_patch_function) {
        process_code(it.second, 0, 0, 0);
      }
      try {
        process_code(fci->name_to_function.at("VersionDetectDC"), 0, 0, 0);
      } catch (const out_of_range&) {
      }
      try {
        process_code(fci->name_to_function.at("VersionDetectGC"), 0, 0, 0);
      } catch (const out_of_range&) {
      }
      try {
        process_code(fci->name_to_function.at("VersionDetectXB"), 0, 0, 0);
      } catch (const out_of_range&) {
      }
      try {
        process_code(fci->name_to_function.at("CacheClearFix-Phase1"), 0x80000000, 8, 0x7F2734EC);
      } catch (const out_of_range&) {
      }
      try {
        process_code(fci->name_to_function.at("CacheClearFix-Phase2"), 0, 0, 0);
      } catch (const out_of_range&) {
      }
    });

Action a_generate_gsl(
    "generate-gsl", "\
  generate-gsl INPUT-DIRECTORY [OUTPUT-FILENAME] [--big-endian]\n\
    Generate a .gsl archive from the contents of a directory. If --big-endian\n\
    is given, the archive header is generated in GameCube format; otherwise it\n\
    is generated in PC/BB format.\n",
    +[](phosg::Arguments& args) {
      string input_directory = args.get<string>(1, true);
      string output_filename = args.get<string>(2, false);
      if (output_filename.empty()) {
        output_filename = input_directory;
        while (output_filename.ends_with("/")) {
          output_filename.pop_back();
        }
        output_filename += ".gsl";
      }

      unordered_map<string, string> files;
      for (const auto& item : std::filesystem::directory_iterator(input_directory)) {
        string filename = item.path().filename().string();
        string file_path = input_directory + "/" + filename;
        if (!std::filesystem::is_regular_file(file_path)) {
          throw std::runtime_error(std::format(
              "input directory contains {} which is not a file", filename));
        }
        files.emplace(filename, phosg::load_file(file_path));
      }
      string gsl_contents = GSLArchive::generate(files, args.get<bool>("big-endian"));
      phosg::save_file(output_filename, gsl_contents);
    });

void a_extract_archive_fn(phosg::Arguments& args) {
  string output_prefix = args.get<string>(2, false);
  if (output_prefix == "-") {
    throw invalid_argument("output prefix cannot be stdout");
  } else if (output_prefix.empty()) {
    output_prefix = args.get<string>(1, false);
    if (output_prefix.empty() || (output_prefix == "-")) {
      throw invalid_argument("an input filename must be given");
    }
    output_prefix += "_";
  }

  string data = read_input_data(args);
  auto data_shared = make_shared<string>(std::move(data));

  if (args.get<string>(0) == "extract-afs") {
    AFSArchive arch(data_shared);
    const auto& all_entries = arch.all_entries();
    for (size_t z = 0; z < all_entries.size(); z++) {
      auto e = arch.get(z);
      string out_file = std::format("{}-{}", output_prefix, z);
      phosg::save_file(out_file, e.first, e.second);
      phosg::fwrite_fmt(stderr, "... {}\n", out_file);
    }
  } else if (args.get<string>(0) == "extract-gsl") {
    GSLArchive arch(data_shared, args.get<bool>("big-endian"));
    for (const auto& entry_it : arch.all_entries()) {
      auto e = arch.get(entry_it.first);
      string out_file = output_prefix + entry_it.first;
      phosg::save_file(out_file, e.first, e.second);
      phosg::fwrite_fmt(stderr, "... {}\n", out_file);
    }
  } else if (args.get<string>(0) == "extract-bml") {
    BMLArchive arch(data_shared, args.get<bool>("big-endian"));
    for (const auto& entry_it : arch.all_entries()) {
      {
        auto e = arch.get(entry_it.first);
        string data = prs_decompress(e.first, e.second);
        string out_file = output_prefix + entry_it.first;
        phosg::save_file(out_file, data);
        phosg::fwrite_fmt(stderr, "... {}\n", out_file);
      }

      auto gvm_e = arch.get_gvm(entry_it.first);
      if (gvm_e.second) {
        string data = prs_decompress(gvm_e.first, gvm_e.second);
        string out_file = output_prefix + entry_it.first + ".gvm";
        phosg::save_file(out_file, data);
        phosg::fwrite_fmt(stderr, "... {}\n", out_file);
      }
    }
  } else if (args.get<string>(0) == "extract-ppk") {
    auto files = decode_ppk_file(*data_shared, args.get<string>("password", true));
    for (const auto& [filename, data] : files) {
      string out_file = output_prefix + filename;
      phosg::save_file(out_file, data);
      phosg::fwrite_fmt(stderr, "... {}\n", out_file);
    }
  } else {
    throw logic_error("unimplemented archive type");
  }
}

Action a_extract_afs("extract-afs", nullptr, a_extract_archive_fn);
Action a_extract_gsl("extract-gsl", nullptr, a_extract_archive_fn);
Action a_extract_bml("extract-bml", nullptr, a_extract_archive_fn);
Action a_extract_ppk("extract-ppk", "\
  extract-afs [INPUT-FILENAME] [--big-endian]\n\
  extract-gsl [INPUT-FILENAME] [--big-endian]\n\
  extract-bml [INPUT-FILENAME] [--big-endian]\n\
  extract-ppk [INPUT-FILENAME] [--big-endian]\n\
    Extract all files from an AFS, GSL, BML, or PPK archive into the current\n\
    directory. input-filename may be specified. If output-filename is\n\
    specified, then it is treated as a prefix which is prepended to the\n\
    filename of each file contained in the archive. If --big-endian is given,\n\
    the archive header is read in GameCube format; otherwise it is read in\n\
    PC/BB format. For PPK archives, the --password= option is required.\n",
    a_extract_archive_fn);

Action a_encode_sjis(
    "encode-sjis", nullptr, +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      string result = tt_utf8_to_sega_sjis(data);
      write_output_data(args, result.data(), result.size(), "txt");
    });
Action a_decode_sjis(
    "decode-sjis", nullptr, +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      string result = tt_sega_sjis_to_utf8(data);
      write_output_data(args, result.data(), result.size(), "txt");
    });

Action a_decode_text_archive(
    "decode-text-archive", "\
  decode-text-archive [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a text archive to JSON. --collections=NUM_COLLECTIONS is given,\n\
    expects a fixed number of collections in the input (this is needed for DC\n\
    NTE and 11/2000). If --has-pr3 is given, expects the input not to have a\n\
    REL footer.\n",
    +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      bool is_sjis = args.get<bool>("japanese");

      unique_ptr<TextSet> ts;
      size_t collection_count = args.get<size_t>("collections", 0);
      if (collection_count) {
        ts = make_unique<BinaryTextSet>(data, collection_count, !args.get<bool>("has-pr3"), is_sjis);
      } else {
        ts = make_unique<BinaryTextAndKeyboardsSet>(data, args.get<bool>("big-endian"), is_sjis);
      }
      phosg::JSON j = ts->json();
      string out_data = j.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY);
      write_output_data(args, out_data.data(), out_data.size(), "json");
    });
Action a_encode_text_archive(
    "encode-text-archive", "\
  encode-text-archive [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encode a text archive. Currently only supports GC and Xbox format.\n",
    +[](phosg::Arguments& args) {
      const string& input_filename = args.get<string>(1, false);
      const string& output_filename = args.get<string>(2, false);
      bool is_sjis = args.get<bool>("japanese");

      auto json = phosg::JSON::parse(read_input_data(args));
      BinaryTextAndKeyboardsSet a(json);
      auto result = a.serialize(args.get<bool>("big-endian"), is_sjis);
      if (output_filename.empty()) {
        if (input_filename.empty() || (input_filename == "-")) {
          throw runtime_error("encoded text archive cannot be written to stdout");
        }
        phosg::save_file(std::format("{}.pr2", input_filename), result.first);
        phosg::save_file(std::format("{}.pr3", input_filename), result.second);
      } else if (output_filename == "-") {
        throw runtime_error("encoded text archive cannot be written to stdout");
      } else {
        string out_filename = output_filename;
        if (out_filename.ends_with(".pr2")) {
          phosg::save_file(out_filename, result.first);
          out_filename[out_filename.size() - 1] = '3';
          phosg::save_file(out_filename, result.second);
        } else {
          phosg::save_file(out_filename + ".pr2", result.first);
          phosg::save_file(out_filename + ".pr3", result.second);
        }
      }
    });

Action a_decode_unicode_text_set(
    "decode-unicode-text-set", nullptr, +[](phosg::Arguments& args) {
      UnicodeTextSet uts(read_input_data(args));
      phosg::JSON j = uts.json();
      string out_data = j.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY);
      write_output_data(args, out_data.data(), out_data.size(), "json");
    });
Action a_encode_unicode_text_set(
    "encode-unicode-text-set", "\
  decode-unicode-text-set [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  encode-unicode-text-set [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a Unicode text set (e.g. unitxt_e.prs) to JSON for easy editing, or\n\
    encode a JSON file to a Unicode text set.\n",
    +[](phosg::Arguments& args) {
      UnicodeTextSet uts(phosg::JSON::parse(read_input_data(args)));
      string encoded = uts.serialize();
      write_output_data(args, encoded.data(), encoded.size(), "prs");
    });

Action a_decode_word_select_set(
    "decode-word-select-set", "\
  decode-word-select-set [INPUT-FILENAME]\n\
    Decode a Word Select data file and print all the tokens. A version option\n\
    (e.g. --gc-ep3) is required. If the Word Select set is for PC or BB, the\n\
    --unitxt option is also required, and must point to a unitxt file in prs\n\
    or JSON format. For PC (V2), the unitxt_e.prs file should be used; for BB,\n\
    the unitxt_ws_e.prs file should be used.\n",
    +[](phosg::Arguments& args) {
      auto version = get_cli_version(args);

      string unitxt_filename = args.get<string>("unitxt");
      const vector<string>* unitxt_collection;
      if (!unitxt_filename.empty()) {
        unique_ptr<UnicodeTextSet> uts;
        if (unitxt_filename.ends_with(".prs")) {
          uts = make_unique<UnicodeTextSet>(phosg::load_file(unitxt_filename));
        } else if (unitxt_filename.ends_with(".json")) {
          uts = make_unique<UnicodeTextSet>(phosg::JSON::parse(phosg::load_file(unitxt_filename)));
        } else {
          throw runtime_error("unitxt filename must end in .prs or .json");
        }
        unitxt_collection = &uts->get((version == Version::BB_V4) ? 0 : 35);
      } else {
        unitxt_collection = nullptr;
      }

      WordSelectSet ws(read_input_data(args), version, unitxt_collection, args.get<bool>("japanese"));
      ws.print(stdout);
    });
Action a_print_word_select_table(
    "print-word-select-table", "\
  print-word-select-table\n\
    Print the Word Select token translation table. If a version option is\n\
    given, prints the table sorted by token ID for that version. If no version\n\
    option is given, prints the token table sorted by canonical name.\n",
    +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_patch_indexes();
      s->load_text_index();
      s->load_word_select_table();
      Version v;
      try {
        v = get_cli_version(args);
      } catch (const runtime_error&) {
        v = Version::UNKNOWN;
      }
      if (v != Version::UNKNOWN) {
        s->word_select_table->print_index(stdout, v);
      } else {
        s->word_select_table->print(stdout);
      }
    });

Action a_download_files(
    "download-files", nullptr,
    +[](phosg::Arguments& args) {
      auto version = get_cli_version(args);
      shared_ptr<PSOBBEncryption::KeyFile> key;
      if (uses_v4_encryption(version)) {
        string key_file_name = args.get<string>("key");
        if (key_file_name.empty()) {
          throw runtime_error("a key filename is required for BB client emulation");
        }
        key = make_shared<PSOBBEncryption::KeyFile>(
            phosg::load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + key_file_name + ".nsk"));
      }
      auto [remote_host, remote_port] = phosg::parse_netloc(args.get<string>(1));
      auto character = PSOCHARFile::load_shared(args.get<string>("character", true), false).character_file;
      auto ship_menu_selections_str = args.get<string>("ship-menu-selections", false);

      unordered_set<string> ship_menu_selections;
      if (!ship_menu_selections_str.empty()) {
        for (const string& s : phosg::split(ship_menu_selections_str, ',')) {
          ship_menu_selections.emplace(s);
        }
      }

      vector<string> on_request_complete_commands;
      string on_request_complete_arg = args.get<string>("on-request-complete-command", false);
      if (!on_request_complete_arg.empty()) {
        for (const string& command : phosg::split(on_request_complete_arg, ',')) {
          on_request_complete_commands.emplace_back(phosg::parse_data_string(command));
        }
      }

      uint32_t serial_number = args.get<uint32_t>(
          "serial-number",
          0,
          is_v1_or_v2(version) ? phosg::Arguments::IntFormat::HEX : phosg::Arguments::IntFormat::DEFAULT);
      auto io_context = make_shared<asio::io_context>();
      DownloadSession session(
          io_context,
          remote_host,
          remote_port,
          args.get<string>("output-dir", true),
          version,
          args.get<uint8_t>("language"),
          key,
          phosg::random_object<uint32_t>(),
          serial_number,
          args.get<string>("access-key", false),
          args.get<string>("username", false),
          args.get<string>("password", false),
          args.get<string>("xb-gamertag", false),
          args.get<uint64_t>("xb-user-id", 0, phosg::Arguments::IntFormat::HEX),
          args.get<uint64_t>("xb-account-id", 0, phosg::Arguments::IntFormat::HEX),
          character,
          ship_menu_selections,
          on_request_complete_commands,
          args.get<bool>("interactive"),
          args.get<bool>("show-command-data"));
      io_context->run();
    });

Action a_convert_rare_item_set(
    "convert-rare-item-set", "\
  convert-rare-item-set INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS]\n\
    If OUTPUT-FILENAME is not given, print the contents of a rare item table in\n\
    a human-readable format. Otherwise, convert the input rare item set to a\n\
    different format and write it to OUTPUT-FILENAME. Both filenames must end\n\
    in one of the following extensions:\n\
      .json (newserv JSON rare item table)\n\
      .gsl (PSO BB little-endian GSL archive)\n\
      .gslb (PSO GC big-endian GSL archive)\n\
      .afs (PSO V2 little-endian AFS archive)\n\
      .rel (Schtserv rare table; cannot be used in output filename)\n\
      .html (HTML rare table; cannot be used in input filename)\n\
    If the --multiply=X option is given, multiplies all drop rates by X (given\n\
    as a decimal value).\n",
    +[](phosg::Arguments& args) {
      double rate_factor = args.get<double>("multiply", 1.0);
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_config_early();
      s->load_patch_indexes();
      s->load_text_index();
      s->load_item_definitions();
      s->load_item_name_indexes();

      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw runtime_error("input filename must be given");
      }

      string input_filename_lower = phosg::tolower(input_filename);
      auto data = make_shared<string>(read_input_data(args));
      shared_ptr<RareItemSet> rs;
      if (input_filename_lower.ends_with(".json")) {
        rs = make_shared<RareItemSet>(phosg::JSON::parse(*data), s->item_name_index_opt(get_cli_version(args, Version::BB_V4)));
      } else if (input_filename_lower.ends_with(".gsl")) {
        rs = make_shared<RareItemSet>(GSLArchive(data, false), false);
      } else if (input_filename_lower.ends_with(".gslb")) {
        rs = make_shared<RareItemSet>(GSLArchive(data, true), true);
      } else if (input_filename_lower.ends_with(".afs")) {
        rs = make_shared<RareItemSet>(AFSArchive(data), is_v1(get_cli_version(args, Version::DC_V2)));
      } else if (input_filename_lower.ends_with(".rel")) {
        rs = make_shared<RareItemSet>(*data, true);
      } else {
        throw runtime_error("cannot determine input format; use a filename ending with .json, .gsl, .gslb, .afs, or .rel");
      }

      if (rate_factor != 1.0) {
        rs->multiply_all_rates(rate_factor);
      }

      string output_filename = args.get<string>(2, false);
      string output_filename_lower = phosg::tolower(output_filename);
      if (output_filename.empty() || (output_filename == "-")) {
        rs->print_all_collections(stdout, s->item_name_index_opt(get_cli_version(args, Version::BB_V4)));
      } else if (output_filename_lower.ends_with(".json")) {
        auto json = rs->json(s->item_name_index_opt(get_cli_version(args, Version::BB_V4)));
        string data = json.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::HEX_INTEGERS | phosg::JSON::SerializeOption::SORT_DICT_KEYS);
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (output_filename_lower.ends_with(".gsl")) {
        string data = rs->serialize_gsl(args.get<bool>("big-endian"));
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (output_filename_lower.ends_with(".gslb")) {
        string data = rs->serialize_gsl(true);
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (output_filename_lower.ends_with(".afs")) {
        bool is_v1 = ::is_v1(get_cli_version(args, Version::DC_V2));
        string data = rs->serialize_afs(is_v1);
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (output_filename_lower.ends_with(".html")) {
        bool is_v1 = ::is_v1(get_cli_version(args, Version::BB_V4));
        static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
        for (GameMode mode : modes) {
          static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
          for (Episode episode : episodes) {
            for (size_t difficulty = 0; difficulty < (is_v1 ? 3 : 4); difficulty++) {
              if (!rs->has_entries_for_game_config(mode, episode, difficulty)) {
                continue;
              }
              auto item_name_index = s->item_name_index(get_cli_version(args, Version::BB_V4));
              string data = rs->serialize_html(mode, episode, difficulty, item_name_index);
              string out_filename = output_filename.substr(0, output_filename.size() - 5) + "." + name_for_mode(mode) + "." + abbreviation_for_episode(episode) + "." + abbreviation_for_difficulty(difficulty) + output_filename.substr(output_filename.size() - 5);
              phosg::save_file(out_filename, data);
              phosg::log_info_f("... {}", out_filename);
            }
          }
        }
      } else {
        throw runtime_error("cannot determine output format; use a filename ending with .json, .gsl, .gslb, or .afs");
      }
    });

static shared_ptr<CommonItemSet> load_common_item_set(const std::string& filename, bool big_endian) {
  auto data = make_shared<string>(phosg::load_file(filename));
  if (filename.ends_with(".json")) {
    return make_shared<JSONCommonItemSet>(phosg::JSON::parse(*data));
  } else if (filename.ends_with(".gsl")) {
    return make_shared<GSLV3V4CommonItemSet>(data, big_endian);
  } else if (filename.ends_with(".gslb")) {
    return make_shared<GSLV3V4CommonItemSet>(data, true);
  } else {
    throw runtime_error("cannot determine input format; use a filename ending with .json, .gsl, or .gslb");
  }
}

Action a_convert_common_item_set(
    "convert-common-item-set", "\
  convert-common-item-set INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Convert the input rare item set to a JSON representation and write it to\n\
    OUTPUT-FILENAME or stdout. The input filename must end in one of the\n\
    following extensions:\n\
      .json (newserv JSON common item table)\n\
      .gsl (PSO BB little-endian GSL archive)\n\
      .gslb (PSO GC big-endian GSL archive)\n",
    +[](phosg::Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw runtime_error("input filename must be given");
      }

      auto cs = load_common_item_set(input_filename, args.get<bool>("big-endian"));
      const string& output_filename = args.get<string>(2, false);
      if (output_filename.empty()) {
        cs->print(stdout);
      } else {
        auto json = cs->json();
        string json_data = json.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::SORT_DICT_KEYS);
        write_output_data(args, json_data.data(), json_data.size(), "json");
      }
    });
Action a_compare_common_item_set(
    "compare-common-item-set", nullptr,
    +[](phosg::Arguments& args) {
      string input_filename1 = args.get<string>(1, false);
      if (input_filename1.empty() || (input_filename1 == "-")) {
        throw runtime_error("two input filenames must be given");
      }
      string input_filename2 = args.get<string>(2, false);
      if (input_filename2.empty() || (input_filename2 == "-")) {
        throw runtime_error("two input filenames must be given");
      }

      auto cs1 = load_common_item_set(input_filename1, args.get<bool>("big-endian1"));
      auto cs2 = load_common_item_set(input_filename2, args.get<bool>("big-endian2"));
      cs1->print_diff(stdout, *cs2);
    });

Action a_describe_item(
    "describe-item", "\
  describe-item DATA-OR-DESCRIPTION\n\
    Describe an item. The argument may be the item\'s raw hex code or a textual\n\
    description of the item. If the description contains spaces, it must be\n\
    quoted, such as \"L&K14 COMBAT +10 0/10/15/0/35\".\n",
    +[](phosg::Arguments& args) {
      string description = args.get<string>(1);
      auto version = get_cli_version(args);

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_config_early();
      s->load_patch_indexes();
      s->load_text_index();
      s->load_item_definitions();
      s->load_item_name_indexes();
      auto name_index = s->item_name_index(version);

      ItemData item = name_index->parse_item_description(description);

      if (args.get<bool>("decode")) {
        item.decode_for_version(version);
      }

      string desc = name_index->describe_item(item);
      string desc_colored = name_index->describe_item(item, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);

      phosg::log_info_f("Data (decoded):        {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} -------- {:02X}{:02X}{:02X}{:02X}",
          item.data1[0], item.data1[1], item.data1[2], item.data1[3],
          item.data1[4], item.data1[5], item.data1[6], item.data1[7],
          item.data1[8], item.data1[9], item.data1[10], item.data1[11],
          item.data2[0], item.data2[1], item.data2[2], item.data2[3]);

      ItemData item_v2 = item;
      item_v2.encode_for_version(Version::PC_V2, s->item_parameter_table_for_encode(Version::PC_V2));
      ItemData item_v2_decoded = item_v2;
      item_v2_decoded.decode_for_version(Version::PC_V2);

      phosg::log_info_f("Data (V2-encoded):     {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} -------- {:02X}{:02X}{:02X}{:02X}",
          item_v2.data1[0], item_v2.data1[1], item_v2.data1[2], item_v2.data1[3],
          item_v2.data1[4], item_v2.data1[5], item_v2.data1[6], item_v2.data1[7],
          item_v2.data1[8], item_v2.data1[9], item_v2.data1[10], item_v2.data1[11],
          item_v2.data2[0], item_v2.data2[1], item_v2.data2[2], item_v2.data2[3]);
      if (item_v2_decoded != item) {
        phosg::log_warning_f("V2-decoded data does not match original data");
        phosg::log_warning_f("Data (V2-decoded):     {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} -------- {:02X}{:02X}{:02X}{:02X}",
            item_v2_decoded.data1[0], item_v2_decoded.data1[1], item_v2_decoded.data1[2], item_v2_decoded.data1[3],
            item_v2_decoded.data1[4], item_v2_decoded.data1[5], item_v2_decoded.data1[6], item_v2_decoded.data1[7],
            item_v2_decoded.data1[8], item_v2_decoded.data1[9], item_v2_decoded.data1[10], item_v2_decoded.data1[11],
            item_v2_decoded.data2[0], item_v2_decoded.data2[1], item_v2_decoded.data2[2], item_v2_decoded.data2[3]);
      }

      ItemData item_gc = item;
      item_gc.encode_for_version(Version::GC_V3, s->item_parameter_table_for_encode(Version::GC_V3));
      ItemData item_gc_decoded = item_gc;
      item_gc_decoded.decode_for_version(Version::GC_V3);

      phosg::log_info_f("Data (GC-encoded):     {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} -------- {:02X}{:02X}{:02X}{:02X}",
          item_gc.data1[0], item_gc.data1[1], item_gc.data1[2], item_gc.data1[3],
          item_gc.data1[4], item_gc.data1[5], item_gc.data1[6], item_gc.data1[7],
          item_gc.data1[8], item_gc.data1[9], item_gc.data1[10], item_gc.data1[11],
          item_gc.data2[0], item_gc.data2[1], item_gc.data2[2], item_gc.data2[3]);
      if (item_gc_decoded != item) {
        phosg::log_warning_f("GC-decoded data does not match original data");
        phosg::log_warning_f("Data (GC-decoded):     {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} {:02X}{:02X}{:02X}{:02X} -------- {:02X}{:02X}{:02X}{:02X}",
            item_gc_decoded.data1[0], item_gc_decoded.data1[1], item_gc_decoded.data1[2], item_gc_decoded.data1[3],
            item_gc_decoded.data1[4], item_gc_decoded.data1[5], item_gc_decoded.data1[6], item_gc_decoded.data1[7],
            item_gc_decoded.data1[8], item_gc_decoded.data1[9], item_gc_decoded.data1[10], item_gc_decoded.data1[11],
            item_gc_decoded.data2[0], item_gc_decoded.data2[1], item_gc_decoded.data2[2], item_gc_decoded.data2[3]);
      }

      phosg::log_info_f("Description: {}", desc);
      phosg::log_info_f("Description (in-game): {}", desc_colored);

      size_t purchase_price = s->item_parameter_table(Version::BB_V4)->price_for_item(item);
      size_t sale_price = purchase_price >> 3;
      phosg::log_info_f("Purchase price: {}; sale price: {}", purchase_price, sale_price);
    });

Action a_name_all_items(
    "name-all-items", nullptr, +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->clear_file_caches();
      s->load_config_early();
      s->load_patch_indexes();
      s->load_text_index();
      s->load_item_definitions();
      s->load_item_name_indexes();
      s->load_ep3_cards();
      s->load_config_late();

      set<uint32_t> all_primary_identifiers;
      for (const auto& index : s->item_name_indexes) {
        if (index) {
          for (const auto& it : index->all_by_primary_identifier()) {
            all_primary_identifiers.emplace(it.first);
          }
        }
      }

      if (args.get<bool>("list")) {
        for (uint32_t primary_identifier : all_primary_identifiers) {
          phosg::fwrite_fmt(stdout, "{:08X}\n", primary_identifier);
          for (Version v : ALL_VERSIONS) {
            const auto& index = s->item_name_index_opt(v);
            if (index) {
              auto pmt = s->item_parameter_table(v);
              ItemData item = ItemData::from_primary_identifier(*s->item_stack_limits(v), primary_identifier);
              string name = index->describe_item(item);
              try {
                bool is_rare = pmt->is_item_rare(item);
                phosg::fwrite_fmt(stdout, "  {:10}: {} {}\n", phosg::name_for_enum(v), is_rare ? "+++" : "---", name);
              } catch (const out_of_range&) {
                phosg::fwrite_fmt(stdout, "  {:10}:     (missing)\n", phosg::name_for_enum(v));
              }
            }
          }
          fputc('\n', stdout);
        }
      } else {
        bool separate_classes = args.get<bool>("separate-classes");

        auto print_header = [&]() -> void {
          phosg::fwrite_fmt(stdout, "IDENT   :");
          for (Version v : ALL_VERSIONS) {
            const auto& index = s->item_name_index_opt(v);
            if (index) {
              phosg::fwrite_fmt(stdout, " {:30}    ", phosg::name_for_enum(v));
            }
          }
          fputc('\n', stdout);
        };

        print_header();
        uint32_t prev_ident = 0;
        for (uint32_t primary_identifier : all_primary_identifiers) {
          if (separate_classes & ((primary_identifier & 0xFFFF0000) != (prev_ident & 0xFFFF0000))) {
            fputc('\n', stdout);
            print_header();
          }
          prev_ident = primary_identifier;

          phosg::fwrite_fmt(stdout, "{:08X}:", primary_identifier);
          for (Version v : ALL_VERSIONS) {
            const auto& index = s->item_name_index_opt(v);
            if (index) {
              auto pmt = s->item_parameter_table(v);
              ItemData item = ItemData::from_primary_identifier(*s->item_stack_limits(v), primary_identifier);
              if (index->exists(item)) {
                string name = index->describe_item(item);
                bool is_rare = pmt->is_item_rare(item);
                phosg::fwrite_fmt(stdout, " {:30}{}", name, is_rare ? " ***" : " ...");
              } else {
                phosg::fwrite_fmt(stdout, " ------------------------------ ---");
              }
            }
          }
          fputc('\n', stdout);
        }
      }
    });

Action a_print_level_stats(
    "show-level-tables", "\
  show-level-tables\n\
    Print the level tables for each version in a semi-human-reatable format.\n",
    +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_config_early();
      s->clear_file_caches();
      s->load_patch_indexes();
      s->load_level_tables();

      vector<PlayerStats> level_1_v1_v2;
      vector<PlayerStats> level_100_v1_v2;
      vector<PlayerStats> level_100_limit_v1_v2;
      vector<PlayerStats> level_200_v1_v2;
      vector<PlayerStats> level_200_limit_v1_v2;
      vector<PlayerStats> level_1_v3;
      vector<PlayerStats> level_200_v3;
      vector<PlayerStats> level_200_limit_v3;
      vector<PlayerStats> level_1_v4;
      vector<PlayerStats> level_200_v4;
      vector<PlayerStats> level_200_limit_v4;
      for (size_t z = 0; z < 12; z++) {
        if (z < 9) {
          level_1_v1_v2.emplace_back().char_stats = s->level_table_v1_v2->base_stats_for_class(z);
          level_100_limit_v1_v2.emplace_back(s->level_table_v1_v2->level_100_stats_for_class(z));
          level_200_limit_v1_v2.emplace_back(s->level_table_v1_v2->max_stats_for_class(z));
          s->level_table_v1_v2->advance_to_level(level_100_v1_v2.emplace_back(level_1_v1_v2.back()), 99, z);
          s->level_table_v1_v2->advance_to_level(level_200_v1_v2.emplace_back(level_1_v1_v2.back()), 199, z);
        }

        level_1_v3.emplace_back().char_stats = s->level_table_v3->base_stats_for_class(z);
        s->level_table_v3->advance_to_level(level_200_v3.emplace_back(level_1_v3.back()), 199, z);
        level_200_limit_v3.emplace_back(s->level_table_v3->max_stats_for_class(z));

        level_1_v4.emplace_back().char_stats = s->level_table_v4->base_stats_for_class(z);
        s->level_table_v4->advance_to_level(level_200_v4.emplace_back(level_1_v3.back()), 199, z);
        level_200_limit_v4.emplace_back(s->level_table_v4->max_stats_for_class(z));
      }

      auto print_stats_set = [](const vector<PlayerStats>& stats_vec, const char* name) -> void {
        phosg::fwrite_fmt(stdout, "{}      ", name);
        for (size_t z = 0; z < stats_vec.size(); z++) {
          phosg::fwrite_fmt(stdout, "  {}", abbreviation_for_char_class(z));
        }

        phosg::fwrite_fmt(stdout, "\n{}   ATP", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.atp);
        }
        phosg::fwrite_fmt(stdout, "\n{}   DFP", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.dfp);
        }
        phosg::fwrite_fmt(stdout, "\n{}   MST", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.mst);
        }
        phosg::fwrite_fmt(stdout, "\n{}   ATA", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.ata);
        }
        phosg::fwrite_fmt(stdout, "\n{}   EVP", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.evp);
        }
        phosg::fwrite_fmt(stdout, "\n{}   LCK", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.lck);
        }
        phosg::fwrite_fmt(stdout, "\n{}    HP", name);
        for (const auto& stats : stats_vec) {
          phosg::fwrite_fmt(stdout, "  {:4}", stats.char_stats.hp);
        }
        fputc('\n', stdout);
      };

      print_stats_set(level_1_v1_v2, "v1/v2 Lv.1  ");
      print_stats_set(level_100_v1_v2, "v1/v2 Lv.100");
      print_stats_set(level_100_limit_v1_v2, "v1 limit    ");
      print_stats_set(level_200_v1_v2, "v2 Lv.200   ");
      print_stats_set(level_200_limit_v1_v2, "v2 limit    ");
      print_stats_set(level_1_v3, "v3 Lv.1     ");
      print_stats_set(level_200_v3, "v3 Lv.200   ");
      print_stats_set(level_200_limit_v3, "v3 limit    ");
      print_stats_set(level_1_v4, "v4 Lv.1     ");
      print_stats_set(level_200_v4, "v4 Lv.200   ");
      print_stats_set(level_200_limit_v4, "v4 limit    ");
    });

Action a_print_item_parameter_tables(
    "show-item-tables", "\
  show-item-tables\n\
    Print the item parameter tables for each version in a semi-human-reatable\n\
    format.\n",
    +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_all(false);
      for (Version v : ALL_VERSIONS) {
        const auto& index = s->item_name_index_opt(v);
        if (index) {
          phosg::fwrite_fmt(stdout, "======== {}\n", phosg::name_for_enum(v));
          index->print_table(stdout);
        }
      }
    });

Action a_show_ep3_cards(
    "show-ep3-cards", "\
  show-ep3-cards\n\
    Print the Episode 3 card definitions from the system/ep3 directory in a\n\
    human-readable format.\n",
    +[](phosg::Arguments& args) {
      bool one_line = args.get<bool>("one-line");

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_ep3_cards();

      unique_ptr<BinaryTextSet> text_english;
      try {
        phosg::JSON json = phosg::JSON::parse(phosg::load_file("system/ep3/text-english.json"));
        text_english = make_unique<BinaryTextSet>(json);
      } catch (const exception& e) {
      }

      auto card_ids = s->ep3_card_index->all_ids();
      phosg::log_info_f("{} card definitions", card_ids.size());
      for (uint32_t card_id : card_ids) {
        auto entry = s->ep3_card_index->definition_for_id(card_id);
        string def_str = entry->def.str(one_line, text_english.get());
        if (one_line) {
          phosg::fwrite_fmt(stdout, "{}\n", def_str);
        } else {
          phosg::fwrite_fmt(stdout, "{}\n", def_str);
          if (!entry->debug_tags.empty()) {
            string tags = phosg::join(entry->debug_tags, ", ");
            phosg::fwrite_fmt(stdout, "  Tags: {}\n", tags);
          }
          if (!entry->dice_caption.empty()) {
            phosg::fwrite_fmt(stdout, "  Dice caption: {}\n", entry->dice_caption);
          }
          if (!entry->dice_caption.empty()) {
            phosg::fwrite_fmt(stdout, "  Dice text: {}\n", entry->dice_text);
          }
          if (!entry->text.empty()) {
            string text = phosg::str_replace_all(entry->text, "\n", "\n    ");
            phosg::strip_trailing_whitespace(text);
            phosg::fwrite_fmt(stdout, "  Text:\n    {}\n", text);
          }
          fputc('\n', stdout);
        }
      }
    });

Action a_generate_ep3_cards_html(
    "generate-ep3-cards-html", "\
  generate-ep3-cards-html [--ep3-nte] [--compare] [--threads=N] [--no-images]\n\
      [--no-disassembly]\n\
    Generate an HTML file describing all Episode 3 card definitions from the\n\
    system/ep3 directory. If --ep3-nte is given, use the Trial Edition card\n\
    definitions instead. If --no-images is given, omit the card images.\n",
    +[](phosg::Arguments& args) {
      size_t num_threads = args.get<size_t>("threads", 0);

      bool include_nte = (get_cli_version(args, Version::GC_EP3) == Version::GC_EP3_NTE) || args.get<bool>("compare");
      bool include_final = (get_cli_version(args, Version::GC_EP3) == Version::GC_EP3) || args.get<bool>("compare");
      bool no_images = args.get<bool>("no-images");
      bool no_large_images = args.get<bool>("no-large-images");
      bool no_disassembly = args.get<bool>("no-disassembly");

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->clear_file_caches();
      s->load_patch_indexes();
      s->load_text_index();
      s->load_ep3_cards();

      shared_ptr<const TextSet> text_english;
      try {
        text_english = s->text_index->get(Version::GC_EP3, 1);
      } catch (const out_of_range&) {
      }

      struct VersionInfo {
        struct CardInfo {
          shared_ptr<const Episode3::CardIndex::CardEntry> ce;
          string small_filename;
          string medium_filename;
          string large_filename;
          string small_data_url;
          string medium_data_url;
          string large_data_url;

          bool is_empty() const {
            return (this->ce == nullptr) && this->small_data_url.empty() && this->medium_data_url.empty() && this->large_data_url.empty();
          }
        };

        const char* name;
        vector<CardInfo> card_infos;
        bool show_large_column = false;
        bool show_medium_column = false;
        bool show_small_column = false;
        size_t num_output_columns = 2;

        VersionInfo(
            const char* name,
            shared_ptr<const Episode3::CardIndex> card_index,
            const char* cardtex_directory,
            bool no_large_images,
            size_t num_threads,
            bool no_disassembly)
            : name(name) {
          for (uint32_t card_id : card_index->all_ids()) {
            if (this->card_infos.size() <= card_id) {
              this->card_infos.resize(card_id + 1);
            }
            this->card_infos[card_id].ce = card_index->definition_for_id(card_id);
          }

          if (cardtex_directory) {
            for (const auto& item : std::filesystem::directory_iterator(cardtex_directory)) {
              string filename = item.path().filename().string();
              if ((filename[0] == 'C' || filename[0] == 'M' || filename[0] == 'L') && (filename[1] == '_')) {
                size_t card_id = stoull(filename.substr(2, 3), nullptr, 10);
                if (this->card_infos.size() <= card_id) {
                  this->card_infos.resize(card_id + 1);
                }
                auto& info = this->card_infos[card_id];
                if (filename[0] == 'C' && !no_large_images) {
                  info.large_filename = string(cardtex_directory) + "/" + filename;
                  this->show_large_column = true;
                } else if (filename[0] == 'L') {
                  info.medium_filename = string(cardtex_directory) + "/" + filename;
                  this->show_medium_column = true;
                } else if (filename[0] == 'M') {
                  info.small_filename = string(cardtex_directory) + "/" + filename;
                  this->show_small_column = true;
                }
              }
            }

            phosg::parallel_range<uint32_t>([&](uint32_t index, size_t) -> bool {
              auto& info = this->card_infos[index];
              if (!info.large_filename.empty()) {
                auto img = phosg::ImageRGBA8888N::from_file_data(phosg::load_file(info.large_filename));
                img.resize(512, 399);
                info.large_data_url = img.serialize(phosg::ImageFormat::PNG_DATA_URL);
              }
              if (!info.medium_filename.empty()) {
                auto img = phosg::ImageRGBA8888N::from_file_data(phosg::load_file(info.medium_filename));
                img.resize(184, 144);
                info.medium_data_url = img.serialize(phosg::ImageFormat::PNG_DATA_URL);
              }
              if (!info.small_filename.empty()) {
                auto img = phosg::ImageRGBA8888N::from_file_data(phosg::load_file(info.small_filename));
                img.resize(58, 43);
                info.small_data_url = img.serialize(phosg::ImageFormat::PNG_DATA_URL);
              }
              return false;
            },
                0, this->card_infos.size(), num_threads);
          }

          this->num_output_columns = 1 + (!no_disassembly) + this->show_small_column + this->show_medium_column + this->show_large_column;
        }

        const CardInfo* get_entry(size_t card_id) const {
          if (card_id >= this->card_infos.size()) {
            return nullptr;
          }
          const auto* entry = &this->card_infos[card_id];
          return entry->is_empty() ? nullptr : entry;
        }
      };

      vector<VersionInfo> version_infos;
      if (include_nte) {
        version_infos.emplace_back("NTE", s->ep3_card_index_trial, no_images ? nullptr : "system/ep3/cardtex-trial", no_large_images, num_threads, no_disassembly);
      }
      if (include_final) {
        version_infos.emplace_back("Final", s->ep3_card_index, no_images ? nullptr : "system/ep3/cardtex", no_large_images, num_threads, no_disassembly);
      }

      deque<string> blocks;
      blocks.emplace_back("<html><head><title>Phantasy Star Online Episode III cards</title></head><body style=\"background-color:#222222; color: #EEEEEE\">");
      blocks.emplace_back("<table><tr><th style=\"text-align: left\">Legend:</th></tr><tr style=\"background-color: #663333\"><td>Card has no definition and is obviously incomplete</td></tr><tr style=\"background-color: #336633\"><td>Card is unobtainable in random draws but may be a quest or event reward</td></tr><tr style=\"background-color: #333333\"><td>Card is obtainable in random draws</td></tr></table><br /><br />");
      blocks.emplace_back("<table><tr><th rowspan=\"2\" style=\"text-align: left; padding: 4px\">ID</th>");

      for (const auto& vi : version_infos) {
        blocks.emplace_back(std::format("<th colspan=\"{}\" style=\"text-align: left; padding: 4px\">{}</th>",
            vi.num_output_columns, vi.name));
      }
      blocks.emplace_back("</tr><tr>");
      for (const auto& vi : version_infos) {
        if (vi.show_small_column) {
          blocks.emplace_back("<th style=\"text-align: left; padding: 4px\">Small</th>");
        }
        if (vi.show_medium_column) {
          blocks.emplace_back("<th style=\"text-align: left; padding: 4px\">Medium</th>");
        }
        if (vi.show_large_column) {
          blocks.emplace_back("<th style=\"text-align: left; padding: 4px\">Large</th>");
        }
        blocks.emplace_back("<th style=\"text-align: left; padding: 4px\">Text</th><th style=\"text-align: left; padding: 4px\">Disassembly</th>");
      }
      blocks.emplace_back("</tr>");

      size_t num_infos = 0;
      for (const auto& vi : version_infos) {
        num_infos = std::max<size_t>(num_infos, vi.card_infos.size());
      }

      for (size_t card_id = 0; card_id < num_infos; card_id++) {
        bool any_vi_has_entry = false;
        for (const auto& vi : version_infos) {
          if (vi.get_entry(card_id)) {
            any_vi_has_entry = true;
            break;
          }
        }
        if (!any_vi_has_entry) {
          continue;
        }

        blocks.emplace_back(std::format("<tr><td style=\"padding: 4px; vertical-align: top\"><pre>{:04X}</pre></td>", card_id));

        for (const auto& vi : version_infos) {
          const VersionInfo::CardInfo* entry = vi.get_entry(card_id);
          if (!entry) {
            blocks.emplace_back(std::format("<td colspan=\"{}\" style=\"padding: 4px; vertical-align: top\"><pre>No entry</pre></td>",
                vi.num_output_columns));
            continue;
          }

          const char* background_color;
          if (!entry->ce) {
            background_color = "#663333";
          } else if (entry->ce->def.cannot_drop ||
              ((entry->ce->def.rank == Episode3::CardRank::D1) || (entry->ce->def.rank == Episode3::CardRank::D2) || (entry->ce->def.rank == Episode3::CardRank::D3)) ||
              ((entry->ce->def.card_class() == Episode3::CardClass::BOSS_ATTACK_ACTION) || (entry->ce->def.card_class() == Episode3::CardClass::BOSS_TECH)) ||
              ((entry->ce->def.drop_rates[0] == 6) && (entry->ce->def.drop_rates[1] == 6))) {
            background_color = "#336633";
          } else {
            background_color = "#333333";
          }

          string td_tag = std::format("<td style=\"padding: 4px; vertical-align: top; background-color: {}\">", background_color);
          if (vi.show_small_column) {
            blocks.emplace_back(td_tag);
            if (!entry->small_data_url.empty()) {
              blocks.emplace_back("<img src=\"");
              blocks.emplace_back(std::move(entry->small_data_url));
              blocks.emplace_back("\" />");
            }
            blocks.emplace_back("</td>");
          }
          if (vi.show_medium_column) {
            blocks.emplace_back(td_tag);
            if (!entry->medium_data_url.empty()) {
              blocks.emplace_back("<img src=\"");
              blocks.emplace_back(std::move(entry->medium_data_url));
              blocks.emplace_back("\" />");
            }
            blocks.emplace_back("</td>");
          }
          if (vi.show_large_column) {
            blocks.emplace_back(td_tag);
            if (!entry->large_data_url.empty()) {
              blocks.emplace_back("<img src=\"");
              blocks.emplace_back(std::move(entry->large_data_url));
              blocks.emplace_back("\" />");
            }
            blocks.emplace_back("</td>");
          }
          blocks.emplace_back(td_tag);
          if (entry->ce) {
            blocks.emplace_back("<pre>");
            blocks.emplace_back(entry->ce->text);
            blocks.emplace_back("</pre></td>");
            if (!no_disassembly) {
              blocks.emplace_back(td_tag);
              blocks.emplace_back("<pre>");
              blocks.emplace_back(entry->ce->def.str(false, text_english.get()));
              blocks.emplace_back("</pre></td>");
            }
          } else {
            blocks.emplace_back("</td>");
            if (!no_disassembly) {
              blocks.emplace_back(td_tag);
              blocks.emplace_back("<pre>Definition is missing</pre>");
              blocks.emplace_back("</td>");
            }
          }
        }
        blocks.emplace_back("</tr>");
      }
      blocks.emplace_back("</table></body></html>");

      phosg::save_file("cards.html", phosg::join(blocks, ""));
    });

Action a_show_ep3_maps(
    "show-ep3-maps", "\
  show-ep3-maps\n\
    Print the Episode 3 maps from the system/ep3 directory in a (sort of)\n\
    human-readable format.\n",
    +[](phosg::Arguments& args) {
      config_log.info_f("Collecting Episode 3 data");

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_ep3_cards();
      s->load_ep3_maps();

      auto map_ids = s->ep3_map_index->all_numbers();
      phosg::log_info_f("{} maps", map_ids.size());
      for (uint32_t map_id : map_ids) {
        auto map = s->ep3_map_index->for_number(map_id);
        const auto& vms = map->all_versions();
        for (size_t language = 0; language < vms.size(); language++) {
          if (!vms[language]) {
            continue;
          }
          string map_s = vms[language]->map->str(s->ep3_card_index.get(), language);
          phosg::fwrite_fmt(stdout, "({}) {}\n", char_for_language_code(language), map_s);
        }
      }
    });

Action a_show_battle_params(
    "show-battle-params", "\
  show-battle-params\n\
    Print the Blue Burst battle parameters from the system/blueburst directory\n\
    in a human-readable format.\n",
    +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_patch_indexes();
      s->load_battle_params();

      phosg::fwrite_fmt(stdout, "Episode 1 multi\n");
      s->battle_params->get_table(false, Episode::EP1).print(stdout);
      phosg::fwrite_fmt(stdout, "Episode 1 solo\n");
      s->battle_params->get_table(true, Episode::EP1).print(stdout);
      phosg::fwrite_fmt(stdout, "Episode 2 multi\n");
      s->battle_params->get_table(false, Episode::EP2).print(stdout);
      phosg::fwrite_fmt(stdout, "Episode 2 solo\n");
      s->battle_params->get_table(true, Episode::EP2).print(stdout);
      phosg::fwrite_fmt(stdout, "Episode 4 multi\n");
      s->battle_params->get_table(false, Episode::EP4).print(stdout);
      phosg::fwrite_fmt(stdout, "Episode 4 solo\n");
      s->battle_params->get_table(true, Episode::EP4).print(stdout);
    });

Action a_check_supermaps(
    "check-supermaps", "\
  check-supermaps [--disassemble] [--generate-enemy-stats]\n\
    Checks the supermaps for all free-play areas and quests. If --disassemble\n\
    is given, saves the contents of each supermap to text files in the current\n\
    directory. If --generate-enemy-stats is given, saves tables of enemy\n\
    counts for each quest to text files in the current directory.\n",
    +[](phosg::Arguments& args) {
      bool save_disassembly = args.get<bool>("disassemble");
      bool generate_enemy_stats = args.get<bool>("generate-enemy-stats");

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_config_early();
      s->clear_file_caches();
      s->load_patch_indexes();
      s->load_set_data_tables();
      s->load_maps();

      auto rand_crypt = make_shared<MT19937Generator>(phosg::random_object<uint32_t>());

      SuperMap::EfficiencyStats all_free_maps_eff;
      for (const auto& it : s->supermap_for_free_play_key) {
        auto episode = static_cast<Episode>((it.first >> 28) & 7);
        auto mode = static_cast<GameMode>((it.first >> 26) & 3);
        uint8_t difficulty = (it.first >> 24) & 3;
        uint8_t floor = (it.first >> 16) & 0xFF;
        uint8_t layout = (it.first >> 8) & 0xFF;
        uint8_t entities = (it.first >> 0) & 0xFF;

        string filename_token;
        if (save_disassembly) {
          string filename = std::format(
              "supermap_{}_{}_{}_{:02X}_{:02X}_{:02X}.txt",
              abbreviation_for_episode(episode),
              abbreviation_for_mode(mode),
              abbreviation_for_difficulty(difficulty),
              floor, layout, entities);
          auto f = phosg::fopen_unique(filename, "wt");
          it.second->print(f.get());
          filename_token = " => " + filename;
        }

        auto eff = it.second->efficiency();
        all_free_maps_eff += eff;
        auto eff_str = eff.str();
        phosg::fwrite_fmt(stderr, "FREE MAP: {:08X} => {} {} {} floor={:02X} layout={:02X} entities={:02X} => {}{}\n",
            it.first,
            abbreviation_for_episode(episode),
            abbreviation_for_mode(mode),
            abbreviation_for_difficulty(difficulty),
            floor, layout, entities, eff_str, filename_token);
      }
      string all_free_maps_eff_str = all_free_maps_eff.str();
      phosg::fwrite_fmt(stderr, "ALL FREE MAPS: {}\n", all_free_maps_eff_str);

      // Generate MapStates for a few random variations
      for (size_t z = 0; z < 0x20; z++) {
        static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
        static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};

        Episode episode = episodes[phosg::random_object<uint32_t>() % episodes.size()];
        GameMode mode = modes[phosg::random_object<uint32_t>() % modes.size()];
        uint8_t difficulty = phosg::random_object<uint32_t>() % 4;
        uint8_t event = phosg::random_object<uint32_t>() % 8;
        uint32_t random_seed = phosg::random_object<uint32_t>();
        phosg::fwrite_fmt(stderr, "FREE MAP STATE TEST: {} {} {}\n",
            abbreviation_for_episode(episode),
            abbreviation_for_mode(mode),
            abbreviation_for_difficulty(difficulty));

        auto sdt = s->set_data_table(Version::BB_V4, episode, mode, difficulty);
        auto variations = sdt->generate_variations(episode, (mode == GameMode::SOLO), rand_crypt);
        auto supermaps = s->supermaps_for_variations(episode, mode, difficulty, variations);
        auto map_state = make_shared<MapState>(
            0, difficulty, event, random_seed, MapState::DEFAULT_RARE_ENEMIES, rand_crypt, supermaps);
        map_state->verify();

        phosg::fwrite_fmt(stderr, "  map state ok: 0x{:X} objects, 0x{:X} enemies, 0x{:X} enemy sets, 0x{:X} events\n",
            map_state->object_states.size(),
            map_state->enemy_states.size(),
            map_state->enemy_set_states.size(),
            map_state->event_states.size());
      }

      s->load_quest_index();

      SuperMap::EfficiencyStats all_quests_eff;
      uint32_t random_seed = args.get<uint32_t>("random-seed", 0, phosg::Arguments::IntFormat::HEX);
      for (const auto& it : s->default_quest_index->quests_by_number) {
        auto supermap = it.second->get_supermap(random_seed);
        if (!supermap) {
          throw logic_error("quest does not have a supermap, even with a specified random seed");
        }

        string filename_token;
        if (save_disassembly) {
          string filename = std::format("supermap_quest_{}_{:08X}.txt", it.first, random_seed);
          auto f = phosg::fopen_unique(filename, "wt");
          phosg::fwrite_fmt(f.get(), "QUEST {} ({})\n", it.first, it.second->name);
          supermap->print(f.get());
          filename_token = " => " + filename;
        }
        if (generate_enemy_stats) {
          array<unordered_map<EnemyType, size_t>, NUM_VERSIONS> counts_for_version;
          for (Version v : ALL_NON_PATCH_VERSIONS) {
            counts_for_version[static_cast<size_t>(v)] = supermap->count_enemy_sets_for_version(v);
          }
          string filename = std::format("supermap_quest_{}_{:08X}_enemy_counts.txt", it.first, random_seed);
          auto f = phosg::fopen_unique(filename, "wt");
          phosg::fwrite_fmt(f.get(), "QUEST {} ({})\n", it.first, it.second->name);
          phosg::fwrite_fmt(f.get(), "ENEMY---------------  DCNTE  11/2K  DC-V1  DC-V2  PCNTE  PC-V2  GCNTE  GC-V3  XB-V3  BB-V4\n");
          for (size_t type_ss = 0; type_ss < static_cast<size_t>(EnemyType::MAX_ENEMY_TYPE); type_ss++) {
            EnemyType type = static_cast<EnemyType>(type_ss);
            bool any_count_nonzero = false;
            array<size_t, NUM_VERSIONS> counts;
            for (Version v : ALL_NON_PATCH_VERSIONS) {
              size_t& count = counts[static_cast<size_t>(v)];
              try {
                count = counts_for_version[static_cast<size_t>(v)].at(type);
              } catch (const out_of_range&) {
                count = 0;
              }
              if (count != 0) {
                any_count_nonzero = true;
              }
            }
            if (any_count_nonzero) {
              phosg::fwrite_fmt(f.get(), "{:20}", phosg::name_for_enum(type));
              for (Version v : ALL_NON_PATCH_VERSIONS) {
                size_t count = counts[static_cast<size_t>(v)];
                if (count > 0) {
                  phosg::fwrite_fmt(f.get(), "  {:5}", count);
                } else {
                  fputs("       ", f.get());
                }
              }
              fputc('\n', f.get());
            }
          }
        }
        auto eff = supermap->efficiency();
        all_quests_eff += eff;
        auto eff_str = eff.str();
        phosg::fwrite_fmt(stderr, "QUEST MAP: {:08X} => {}{}\n", it.first, eff_str, filename_token);

        auto map_state = make_shared<MapState>(
            0,
            phosg::random_object<uint8_t>() & 3,
            0,
            phosg::random_object<uint32_t>(),
            MapState::DEFAULT_RARE_ENEMIES,
            rand_crypt,
            supermap);
        map_state->verify();

        phosg::fwrite_fmt(stderr, "  map state ok: 0x{:X} objects, 0x{:X} enemies, 0x{:X} enemy sets, 0x{:X} events\n",
            map_state->object_states.size(),
            map_state->enemy_states.size(),
            map_state->enemy_set_states.size(),
            map_state->event_states.size());
      }
      string all_quests_eff_str = all_quests_eff.str();
      phosg::fwrite_fmt(stderr, "ALL QUEST MAPS: {}\n", all_quests_eff_str);
    });

Action a_parse_object_graph(
    "parse-object-graph", nullptr, +[](phosg::Arguments& args) {
      uint32_t root_object_address = args.get<uint32_t>("root", phosg::Arguments::IntFormat::HEX);
      string data = read_input_data(args);
      PSOGCObjectGraph g(data, root_object_address);
      g.print(stdout);
    });

Action a_generate_dc_serial_number(
    "generate-dc-serial-number", "\
  generate-dc-serial-number DOMAIN SUBDOMAIN\n\
    Generate a PSO DC serial number. DOMAIN should be 0 for Japanese, 1 for\n\
    USA, or 2 for Europe. SUBDOMAIN should be 0 for v1, or 1 for v2.\n",
    +[](phosg::Arguments& args) {
      uint8_t domain = args.get<uint8_t>(1);
      uint8_t subdomain = args.get<uint8_t>(2);
      string serial_number = generate_dc_serial_number(domain, subdomain);
      phosg::fwrite_fmt(stdout, "{}\n", serial_number);
    });
Action a_generate_all_dc_serial_numbers(
    "dc-serial-number-generator-test", nullptr,
    +[](phosg::Arguments& args) {
      size_t num_threads = args.get<size_t>("threads", 0);

      vector<unordered_set<uint32_t>> serial_numbers;
      serial_numbers.resize(9);

      DCSerialNumberIterator iter;
      uint32_t serial_number;
      size_t num_serial_numbers = 0;
      while ((serial_number = iter.next()) != 0) {
        serial_numbers[iter.domain * 3 + iter.subdomain].emplace(serial_number);
        if (((++num_serial_numbers) % 0x10000) == 0) {
          phosg::fwrite_fmt(stderr, "... {:08X} (domain={:02X}, subdomain={:02X}, index2={:04X}, index3={:04X}) counts=[{}, {}, {}, {}, {}, {}, {}, {}, {}]\n",
              num_serial_numbers, iter.domain, iter.subdomain, iter.index2, iter.index3,
              serial_numbers[0].size(), serial_numbers[1].size(), serial_numbers[2].size(),
              serial_numbers[3].size(), serial_numbers[4].size(), serial_numbers[5].size(),
              serial_numbers[6].size(), serial_numbers[7].size(), serial_numbers[8].size());
        }
      }

      array<atomic<size_t>, 9> found_counts = {0, 0, 0, 0, 0, 0, 0, 0, 0};
      atomic<uint64_t> num_mismatches = 0;
      mutex output_lock;
      auto thread_fn = [&](uint64_t serial_number, size_t) -> bool {
        for (uint8_t domain = 0; domain < 3; domain++) {
          for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
            bool is_valid = dc_serial_number_is_valid_fast(serial_number, domain, subdomain);
            bool was_iterated = serial_numbers[domain * 3 + subdomain].count(serial_number);
            if (is_valid != was_iterated) {
              lock_guard g(output_lock);
              phosg::fwrite_fmt(stdout, "Mismatch at {:08X} (domain={}, subdomain={}): is_valid={}, was_iterated={}\n",
                  serial_number, domain, subdomain, is_valid ? "true" : "false", was_iterated ? "true" : "false");
            } else if (is_valid && was_iterated) {
              found_counts[domain * 3 + subdomain]++;
            }
          }
        }
        return false;
      };
      auto progress_fn = [&](uint64_t, uint64_t, uint64_t current_value, uint64_t) -> void {
        phosg::fwrite_fmt(stderr, "... {:08X} {} mismatches; counts: [{}/{}, {}/{}, {}/{}, {}/{}, {}/{}, {}/{}, {}/{}, {}/{}, {}/{}]\r", current_value, num_mismatches.load(),
            found_counts[0].load(), serial_numbers[0].size(),
            found_counts[1].load(), serial_numbers[1].size(),
            found_counts[2].load(), serial_numbers[2].size(),
            found_counts[3].load(), serial_numbers[3].size(),
            found_counts[4].load(), serial_numbers[4].size(),
            found_counts[5].load(), serial_numbers[5].size(),
            found_counts[6].load(), serial_numbers[6].size(),
            found_counts[7].load(), serial_numbers[7].size(),
            found_counts[8].load(), serial_numbers[8].size());
      };
      phosg::parallel_range_blocks<uint64_t>(thread_fn, 0, 0x100000000, 0x1000, num_threads, progress_fn);

      if (num_mismatches > 0) {
        throw logic_error("mismatches occurred during test");
      }
    });

Action a_inspect_dc_serial_number(
    "inspect-dc-serial-number", "\
  inspect-dc-serial-number SERIAL-NUMBER\n\
    Show which domain and subdomain the serial number belongs to. (As with\n\
    generate-dc-serial-number, described above, this will tell you which PSO\n\
    version it is valid for.)\n",
    +[](phosg::Arguments& args) {
      const string& serial_number_str = args.get<string>(1, false);
      if (serial_number_str.empty()) {
        throw invalid_argument("no serial number given");
      }
      size_t num_valid_subdomains = 0;
      for (uint8_t domain = 0; domain < 3; domain++) {
        for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
          if (dc_serial_number_is_valid_fast(serial_number_str, domain, subdomain)) {
            phosg::fwrite_fmt(stdout, "{} is valid in domain {} subdomain {}\n", serial_number_str, domain, subdomain);
            num_valid_subdomains++;
          }
        }
      }
      if (num_valid_subdomains == 0) {
        phosg::fwrite_fmt(stdout, "{} is not valid in any domain\n", serial_number_str);
      }
    });
Action a_dc_serial_number_speed_test(
    "dc-serial-number-speed-test", "\
  dc-serial-number-speed-test\n\
    Run a speed test of the two DC serial number validation functions.\n",
    +[](phosg::Arguments& args) {
      const string& seed = args.get<string>("seed");
      if (seed.empty()) {
        dc_serial_number_speed_test();
      } else {
        dc_serial_number_speed_test(stoul(seed, nullptr, 16));
      }
    });

Action a_address_translator(
    "address-translator", nullptr, +[](phosg::Arguments& args) {
      const string& dir = args.get<string>(1, false);
      if (dir.empty() || (dir == "-")) {
        throw invalid_argument("a directory name is required");
      }
      run_address_translator(dir, args.get<string>(2, false), args.get<string>(3, false));
    });

Action a_diff_executables(
    "diff-executables", nullptr, +[](phosg::Arguments& args) {
      const string& a_filename = args.get<string>(1);
      const string& b_filename = args.get<string>(2);
      bool show_pre = args.get<bool>("show-pre");
      bool a_is_dol = a_filename.ends_with(".dol");
      bool b_is_dol = b_filename.ends_with(".dol");
      bool a_is_xbe = a_filename.ends_with(".xbe");
      bool b_is_xbe = b_filename.ends_with(".xbe");
      std::vector<DiffEntry> result;
      if (a_is_dol && b_is_dol) {
        result = diff_dol_files(a_filename, b_filename);
      } else if (a_is_xbe && b_is_xbe) {
        result = diff_xbe_files(a_filename, b_filename);
      } else {
        throw runtime_error("the two files are not the same type of executable, or are neither dol nor xbe");
      }
      for (const auto& it : result) {
        string b_str = phosg::format_data_string(it.b_data, nullptr, phosg::FormatDataFlags::HEX_ONLY);
        if (show_pre) {
          string a_str = phosg::format_data_string(it.a_data, nullptr, phosg::FormatDataFlags::HEX_ONLY);
          phosg::fwrite_fmt(stdout, "{:08X}: {} => {}\n", it.address, a_str, b_str);
        } else {
          phosg::fwrite_fmt(stdout, "{:08X} {}\n", it.address, b_str);
        }
      }
    });

Action a_generate_hangame_creds(
    "generate-hangame-creds", nullptr, +[](phosg::Arguments& args) {
      const string& user_id = args.get<string>(1);
      const string& token = args.get<string>(2);
      const string& unused = args.get<string>(3, false);
      string hex = phosg::format_data_string(encode_psobb_hangame_credentials(user_id, token, unused));
      phosg::fwrite_fmt(stdout, "psobb.exe 1196310600 {}\n", hex);
    });

Action a_format_ep3_battle_record(
    "format-ep3-battle-record", nullptr, +[](phosg::Arguments& args) {
      string data = read_input_data(args);
      Episode3::BattleRecord rec(data);
      rec.print(stdout);
    });

Action a_replay_ep3_battle_commands(
    "replay-ep3-battle-commands", nullptr, +[](phosg::Arguments& args) {
      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_ep3_cards();
      s->load_ep3_maps();

      int64_t base_seed = args.get<int64_t>("seed", -1);
      bool is_trial = (get_cli_version(args, Version::GC_EP3) == Version::GC_EP3_NTE);

      auto input = read_input_data(args);
      vector<string> commands;
      for (const auto& line : phosg::split(input, '\n')) {
        string data = phosg::parse_data_string(line);
        if (!data.empty()) {
          commands.emplace_back(std::move(data));
        }
      }

      auto run_replay = [&](int64_t seed, size_t) {
        Episode3::Server::Options options = {
            .card_index = s->ep3_card_index,
            .map_index = s->ep3_map_index,
            .behavior_flags = 0x0092,
            .opt_rand_stream = nullptr,
            .rand_crypt = make_shared<MT19937Generator>(seed),
            .tournament = nullptr,
            .trap_card_ids = {},
        };
        if (is_trial) {
          options.behavior_flags |= Episode3::BehaviorFlag::IS_TRIAL_EDITION;
        }
        if (base_seed >= 0) {
          options.behavior_flags |= Episode3::BehaviorFlag::LOG_COMMANDS_IF_LOBBY_MISSING;
        }
        auto server = make_shared<Episode3::Server>(nullptr, std::move(options));
        server->init();
        for (const auto& command : commands) {
          server->on_server_data_input(nullptr, command);
        }
        return false;
      };

      if (base_seed >= 0) {
        run_replay(base_seed, 0);
      } else {
        size_t num_threads = args.get<size_t>("threads", 0);
        phosg::parallel_range_blocks<int64_t>(run_replay, 0, 0x100000000, 0x1000, num_threads);
      }
    });

Action a_replay_ep3_battle_record(
    "replay-ep3-battle-record", nullptr, +[](phosg::Arguments& args) {
      auto rec = make_shared<Episode3::BattleRecord>(read_input_data(args));

      auto s = make_shared<ServerState>(get_config_filename(args));
      s->load_ep3_cards();
      s->load_ep3_maps();

      bool is_trial = (get_cli_version(args, Version::GC_EP3) == Version::GC_EP3_NTE);

      Episode3::Server::Options options = {
          .card_index = s->ep3_card_index,
          .map_index = s->ep3_map_index,
          .behavior_flags = (Episode3::BehaviorFlag::IGNORE_CARD_COUNTS |
              Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES |
              Episode3::BehaviorFlag::DISABLE_MASKING |
              Episode3::BehaviorFlag::LOG_COMMANDS_IF_LOBBY_MISSING),
          .opt_rand_stream = make_shared<phosg::StringReader>(rec->get_random_stream()),
          .rand_crypt = make_shared<DisabledRandomGenerator>(),
          .tournament = nullptr,
          .trap_card_ids = {},
      };
      if (is_trial) {
        options.behavior_flags |= Episode3::BehaviorFlag::IS_TRIAL_EDITION;
      }
      options.behavior_flags |= Episode3::BehaviorFlag::LOG_COMMANDS_IF_LOBBY_MISSING;
      auto server = make_shared<Episode3::Server>(nullptr, std::move(options));
      server->init();
      for (const auto& command : rec->get_all_server_data_commands()) {
        phosg::log_info_f("Server data command");
        phosg::print_data(stderr, command, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);
        server->on_server_data_input(nullptr, command);
      }
    });

Action a_disassemble_ep3_battle_record(
    "disassemble-ep3-battle-record", nullptr, +[](phosg::Arguments& args) {
      Episode3::BattleRecord(read_input_data(args)).print(stdout);
    });

Action a_run_server_replay_log(
    "", nullptr, +[](phosg::Arguments& args) {
      {
        string build_date = phosg::format_time(BUILD_TIMESTAMP);
        config_log.info_f("newserv {} compiled at {}", GIT_REVISION_HASH, build_date);
      }

      if (!std::filesystem::is_directory("system/players")) {
        config_log.info_f("Players directory does not exist; creating it");
        std::filesystem::create_directories("system/players");
      }

      const string& replay_log_filename = args.get<string>("replay-log");

#ifndef PHOSG_WINDOWS
      signal(SIGPIPE, SIG_IGN);
#endif
      if (!phosg::is_windows() && isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      auto state = make_shared<ServerState>(get_config_filename(args));
      if (args.get<bool>("debug")) {
        state->is_debug = true;
      }
      state->load_all(true);

      if (state->dns_server_port) {
        if (!state->dns_server_addr.empty()) {
          config_log.info_f("Starting DNS server on {}:{}", state->dns_server_addr, state->dns_server_port);
        } else {
          config_log.info_f("Starting DNS server on port {}", state->dns_server_port);
        }
        state->dns_server = make_shared<DNSServer>(state);
        state->dns_server->listen(state->dns_server_addr, state->dns_server_port);
      } else {
        config_log.info_f("DNS server is disabled");
      }

      shared_ptr<ServerShell> shell;
      shared_ptr<ReplaySession> replay_session;
      shared_ptr<SignalWatcher> signal_watcher;
      if (!replay_log_filename.empty()) {
        config_log.info_f("Starting game server");
        state->game_server = make_shared<GameServer>(state);

        // TODO: Do this properly via a config option, you lazy bum
        state->dol_file_index = make_shared<DOLFileIndex>();

        auto log_f = phosg::fopen_shared(replay_log_filename, "rt");

        replay_session = make_shared<ReplaySession>(state, log_f.get(), false);
        asio::co_spawn(*state->io_context, replay_session->run(), asio::detached);

      } else {
        config_log.info_f("Opening sockets");
        for (const auto& [_, pc] : state->name_to_port_config) {
          if (!state->game_server.get()) {
            config_log.info_f("Starting game server");
            state->game_server = make_shared<GameServer>(state);
          }
          string spec = std::format("TG-{}-{}-{}-{}",
              pc->port, phosg::name_for_enum(pc->version), pc->name, phosg::name_for_enum(pc->behavior));
          state->game_server->listen(spec, pc->addr, pc->port, pc->version, pc->behavior);
        }

        if (!state->ip_stack_addresses.empty() || !state->ppp_stack_addresses.empty() || !state->ppp_raw_addresses.empty()) {
          config_log.info_f("Starting IP/PPP stack simulator");
          state->ip_stack_simulator = make_shared<IPStackSimulator>(state);
          for (const auto& it : state->ip_stack_addresses) {
            auto netloc = phosg::parse_netloc(it);
            string spec = (netloc.second == 0) ? ("T-IPS-" + netloc.first) : std::format("T-IPS-{}", netloc.second);
            state->ip_stack_simulator->listen(
                spec, netloc.first, netloc.second, VirtualNetworkProtocol::ETHERNET_TAPSERVER);
          }
          for (const auto& it : state->ppp_stack_addresses) {
            auto netloc = phosg::parse_netloc(it);
            string spec = (netloc.second == 0) ? ("T-PPPST-" + netloc.first) : std::format("T-PPPST-{}", netloc.second);
            state->ip_stack_simulator->listen(
                spec, netloc.first, netloc.second, VirtualNetworkProtocol::HDLC_TAPSERVER);
          }
          for (const auto& it : state->ppp_raw_addresses) {
            auto netloc = phosg::parse_netloc(it);
            string spec = (netloc.second == 0) ? ("T-PPPSR-" + netloc.first) : std::format("T-PPPSR-{}", netloc.second);
            state->ip_stack_simulator->listen(
                spec, netloc.first, netloc.second, VirtualNetworkProtocol::HDLC_RAW);
            if (netloc.second) {
              if (state->local_address == 0 && state->external_address == 0) {
                config_log.info_f(
                    "Cannot generate Devolution phone numbers for {} because LocalAddress and ExternalAddress are not specified in the configuration",
                    spec);
              } else if (state->local_address == 0) {
                config_log.info_f(
                    "Note: The Devolution phone number for {} is {} (external)",
                    spec, devolution_phone_number_for_netloc(state->external_address, netloc.second));
              } else if (state->external_address == 0) {
                config_log.info_f(
                    "Note: The Devolution phone number for {} is {} (local)",
                    spec, devolution_phone_number_for_netloc(state->local_address, netloc.second));
              } else if (state->local_address == state->external_address) {
                config_log.info_f(
                    "Note: The Devolution phone number for {} is {} (local+external)",
                    spec, devolution_phone_number_for_netloc(state->local_address, netloc.second));
              } else {
                config_log.info_f(
                    "Note: The Devolution phone numbers for {} are {} (local) and {} (external)",
                    spec,
                    devolution_phone_number_for_netloc(state->local_address, netloc.second),
                    devolution_phone_number_for_netloc(state->external_address, netloc.second));
              }
            }
          }
        }

        if (!state->http_addresses.empty() || !state->http_addresses.empty()) {
          config_log.info_f("Starting HTTP server");
          state->http_server = make_shared<HTTPServer>(state);
          for (const auto& it : state->http_addresses) {
            auto netloc = phosg::parse_netloc(it);
            state->http_server->listen(netloc.first, netloc.second);
          }
        }

#ifndef PHOSG_WINDOWS
        config_log.info_f("Enabling signal watcher");
        signal_watcher = make_shared<SignalWatcher>(state);
#endif
      }

#ifndef PHOSG_WINDOWS
      if (!state->username.empty()) {
        config_log.info_f("Switching to user {}", state->username);
        drop_privileges(state->username);
      }
#endif

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

      config_log.info_f("Ready");
      if (should_run_shell) {
        shell = make_shared<ServerShell>(state);
      }

      state->io_context->run();
      config_log.info_f("Normal shutdown");

      if (replay_session && replay_session->failed()) {
        throw runtime_error("Replay failed");
      }
    });

void print_version_info() {
  string build_date = phosg::format_time(BUILD_TIMESTAMP);
  phosg::fwrite_fmt(stderr, "newserv-{} built {} UTC\n", GIT_REVISION_HASH, build_date);
}

void print_usage() {
  print_version_info();
  fputs("\n\
Usage:\n\
  newserv [ACTION] [OPTIONS...]\n\
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
the output to INPUT-FILENAME.dec or a similarly-named file; if OUTPUT-FILENAME\n\
is '-', newserv writes the output to stdout. If stdout is a terminal and the\n\
output is not text or JSON, the data written to stdout is formatted in a\n\
hex/ASCII view; in any other case, the raw output is written to stdout, which\n\
(for most actions) may include arbitrary binary data.\n\
\n\
The actions are:\n",
      stderr);
  for (const auto& a : action_order) {
    if (a->help_text) {
      fputs(a->help_text, stderr);
    }
  }
  fputs("\n\
Most options that take data as input also accept the following option:\n\
  --parse-data\n\
      For modes that take input (from a file or from stdin), parse the input as\n\
      a hex string before encrypting/decoding/etc.\n\
\n\
Many versions also accept or require a version option. The version options are:\n\
  --pc-patch: PC patch server\n\
  --bb-patch: BB patch server\n\
  --dc-nte: DC Network Trial Edition\n\
  --dc-proto or --dc-11-2000: DC 11/2000 prototype\n\
  --dc-v1: DC v1\n\
  --dc-v2 or --dc: DC v2\n\
  --pc-nte: PC Network Trial Edition\n\
  --pc: PC v2\n\
  --gc-nte: GC Episodes 1&2 Trial Edition\n\
  --gc: GC Episodes 1&2\n\
  --xb: Xbox Episodes 1&2\n\
  --ep3-nte: GC Episode 3 Trial Edition\n\
  --ep3: GC Episode 3\n\
  --bb: Blue Burst\n\
\n",
      stderr);
}

int main(int argc, char** argv) {
  phosg::Arguments args(&argv[1], argc - 1);
  if (args.get<bool>("help")) {
    print_usage();
    return 0;
  }

  string action_name = args.get<string>(0, false);
  const Action* a;
  try {
    a = all_actions.at(action_name);
  } catch (const out_of_range&) {
    phosg::log_error_f("Unknown or invalid action; try --help");
    return 1;
  }
  if (IS_WINDOWS) {
    // Cygwin just gives a stackdump when an exception falls out of main(), so
    // unlike Linux and macOS, we have to manually catch exceptions here just to
    // see what the exception message was.
    try {
      a->run(args);
    } catch (const phosg::cannot_open_file& e) {
      phosg::log_error_f("Top-level exception (cannot_open_file): {}", e.what());
      throw;
    } catch (const invalid_argument& e) {
      phosg::log_error_f("Top-level exception (invalid_argument): {}", e.what());
      throw;
    } catch (const out_of_range& e) {
      phosg::log_error_f("Top-level exception (out_of_range): {}", e.what());
      throw;
    } catch (const runtime_error& e) {
      phosg::log_error_f("Top-level exception (runtime_error): {}", e.what());
      throw;
    } catch (const exception& e) {
      phosg::log_error_f("Top-level exception: {}", e.what());
      throw;
    }
  } else {
    a->run(args);
  }
  return 0;
}
