#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <mutex>
#include <phosg/Arguments.hh>
#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Math.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <set>
#include <thread>
#include <unordered_map>

#ifdef HAVE_RESOURCE_FILE
#include "ARCodeTranslator.hh"
#else
#include "ARCodeTranslator-Stub.hh"
#endif
#include "BMLArchive.hh"
#include "CatSession.hh"
#include "Compression.hh"
#include "DCSerialNumbers.hh"
#include "DNSServer.hh"
#include "GSLArchive.hh"
#include "GVMEncoder.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "PSOGCObjectGraph.hh"
#include "ProxyServer.hh"
#include "Quest.hh"
#include "QuestScript.hh"
#include "ReplaySession.hh"
#include "SaveFileFormats.hh"
#include "SendCommands.hh"
#include "Server.hh"
#include "ServerShell.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "TextArchive.hh"
#include "UnicodeTextSet.hh"

using namespace std;

bool use_terminal_colors = false;

void print_usage();

template <typename T>
vector<T> parse_int_vector(const JSON& o) {
  vector<T> ret;
  for (const auto& x : o.as_list()) {
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
  config_log.info("Switched to user %s (%d:%d)", username.c_str(), pw->pw_uid, pw->pw_gid);
}

Version get_cli_version(Arguments& args) {
  if (args.get<bool>("pc-patch")) {
    return Version::PC_PATCH;
  } else if (args.get<bool>("bb-patch")) {
    return Version::BB_PATCH;
  } else if (args.get<bool>("dc-nte")) {
    return Version::DC_NTE;
  } else if (args.get<bool>("dc-proto")) {
    return Version::DC_V1_11_2000_PROTOTYPE;
  } else if (args.get<bool>("dc-v1")) {
    return Version::DC_V1;
  } else if (args.get<bool>("dc-v2") || args.get<bool>("dc")) {
    return Version::DC_V2;
  } else if (args.get<bool>("pc")) {
    return Version::PC_V2;
  } else if (args.get<bool>("gc-nte")) {
    return Version::GC_NTE;
  } else if (args.get<bool>("gc")) {
    return Version::GC_V3;
  } else if (args.get<bool>("xb")) {
    return Version::XB_V3;
  } else if (args.get<bool>("ep3-trial")) {
    return Version::GC_EP3_TRIAL_EDITION;
  } else if (args.get<bool>("ep3")) {
    return Version::GC_EP3;
  } else if (args.get<bool>("bb")) {
    return Version::BB_V4;
  } else {
    throw runtime_error("a version option is required");
  }
}

string read_input_data(Arguments& args) {
  const string& input_filename = args.get<string>(1, false);

  string data;
  if (!input_filename.empty() && (input_filename != "-")) {
    data = load_file(input_filename);
  } else {
    data = read_all(stdin);
  }
  if (args.get<bool>("parse-data")) {
    data = parse_data_string(data, nullptr, ParseDataFlags::ALLOW_FILES);
  }
  return data;
}

bool is_text_extension(const char* extension) {
  return (!strcmp(extension, "txt") || !strcmp(extension, "json"));
}

void write_output_data(Arguments& args, const void* data, size_t size, const char* extension) {
  const string& input_filename = args.get<string>(1, false);
  const string& output_filename = args.get<string>(2, false);

  if (!output_filename.empty() && (output_filename != "-")) {
    // If the output is to a specified file, write it there
    save_file(output_filename, data, size);

  } else if (output_filename.empty() && (output_filename != "-") && !input_filename.empty() && (input_filename != "-")) {
    // If no output filename is given and an input filename is given, write to
    // <input_filename>.<extension>
    if (!extension) {
      throw runtime_error("an output filename is required");
    }
    string filename = input_filename;
    filename += ".";
    filename += extension;
    save_file(filename, data, size);

  } else if (isatty(fileno(stdout)) && (!extension || !is_text_extension(extension))) {
    // If stdout is a terminal and the data is not known to be text, use
    // print_data to write the result
    print_data(stdout, data, size);
    fflush(stdout);

  } else {
    // If stdout is not a terminal, write the data as-is
    fwritex(stdout, data, size);
    fflush(stdout);
  }
}

struct Action;
unordered_map<string, const Action*> all_actions;
vector<const Action*> action_order;

struct Action {
  const char* name;
  const char* help_text; // May be null
  function<void(Arguments& args)> run;

  Action(
      const char* name,
      const char* help_text,
      function<void(Arguments& args)> run)
      : name(name),
        help_text(help_text),
        run(run) {
    auto emplace_ret = all_actions.emplace(this->name, this);
    if (!emplace_ret.second) {
      throw logic_error(string_printf("multiple actions with the same name: %s", this->name));
    }
    action_order.emplace_back(this);
  }
};

Action a_help(
    "help", "\
  help\n\
    You\'re reading it now.\n",
    +[](Arguments&) -> void {
      print_usage();
    });

static void a_compress_decompress_fn(Arguments& args) {
  const auto& action = args.get<string>(0);
  bool is_prs = ends_with(action, "-prs");
  bool is_bc0 = ends_with(action, "-bc0");
  bool is_pr2 = ends_with(action, "-pr2");
  bool is_decompress = starts_with(action, "decompress-");
  bool is_big_endian = args.get<bool>("big-endian");
  bool is_optimal = args.get<bool>("optimal");
  int8_t compression_level = args.get<int8_t>("compression-level", 0);
  size_t bytes = args.get<size_t>("bytes", 0);
  string seed = args.get<string>("seed");

  string data = read_input_data(args);

  size_t pr2_expected_size = 0;
  if (is_decompress && is_pr2) {
    auto decrypted = is_big_endian ? decrypt_pr2_data<true>(data) : decrypt_pr2_data<false>(data);
    pr2_expected_size = decrypted.decompressed_size;
    data = std::move(decrypted.compressed_data);
  }

  size_t input_bytes = data.size();
  auto progress_fn = [&](auto, size_t input_progress, size_t, size_t output_progress) -> void {
    float progress = static_cast<float>(input_progress * 100) / input_bytes;
    float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
    fprintf(stderr, "... %zu/%zu (%g%%) => %zu (%g%%)    \r",
        input_progress, input_bytes, progress, output_progress, size_ratio);
  };
  auto optimal_progress_fn = [&](auto phase, size_t input_progress, size_t input_bytes, size_t output_progress) -> void {
    const char* phase_name = name_for_enum(phase);
    float progress = static_cast<float>(input_progress * 100) / input_bytes;
    float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
    fprintf(stderr, "... [%s] %zu/%zu (%g%%) => %zu (%g%%)    \r",
        phase_name, input_progress, input_bytes, progress, output_progress, size_ratio);
  };

  uint64_t start = now();
  if (!is_decompress && (is_prs || is_pr2)) {
    if (is_optimal) {
      data = prs_compress_optimal(data.data(), data.size(), optimal_progress_fn);
    } else {
      data = prs_compress(data, compression_level, progress_fn);
    }
  } else if (is_decompress && (is_prs || is_pr2)) {
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
  uint64_t end = now();
  string time_str = format_duration(end - start);

  float size_ratio = static_cast<float>(data.size() * 100) / input_bytes;
  double bytes_per_sec = input_bytes / (static_cast<double>(end - start) / 1000000.0);
  string bytes_per_sec_str = format_size(bytes_per_sec);
  log_info("%zu (0x%zX) bytes input => %zu (0x%zX) bytes output (%g%%) in %s (%s / sec)",
      input_bytes, input_bytes, data.size(), data.size(), size_ratio, time_str.c_str(), bytes_per_sec_str.c_str());

  if (is_decompress && is_pr2 && (data.size() != pr2_expected_size)) {
    log_warning("Result data size (%zu bytes) does not match expected size from PR2 header (%zu bytes)", data.size(), pr2_expected_size);
  } else if (!is_decompress && is_pr2) {
    uint32_t pr2_seed = seed.empty() ? random_object<uint32_t>() : stoul(seed, nullptr, 16);
    data = is_big_endian
        ? encrypt_pr2_data<true>(data, input_bytes, pr2_seed)
        : encrypt_pr2_data<false>(data, input_bytes, pr2_seed);
  }

  const char* extension;
  if (is_decompress) {
    extension = "dec";
  } else if (is_prs) {
    extension = "prs";
  } else if (is_bc0) {
    extension = "bc0";
  } else if (is_pr2) {
    extension = "pr2";
  } else {
    throw logic_error("unknown action");
  }
  write_output_data(args, data.data(), data.size(), extension);
}

Action a_compress_prs("compress-prs", nullptr, a_compress_decompress_fn);
Action a_compress_bc0("compress-bc0", nullptr, a_compress_decompress_fn);
Action a_compress_pr2("compress-pr2", "\
  compress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-pr2 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  compress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Compress data using the PRS, PR2, or BC0 algorithms. By default, the\n\
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
Action a_decompress_pr2("decompress-pr2", "\
  decompress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-pr2 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decompress data compressed using the PRS, PR2, or BC0 algorithms.\n",
    a_compress_decompress_fn);

Action a_prs_size(
    "prs-size", "\
  prs-size [INPUT-FILENAME]\n\
    Compute the decompressed size of the PRS-compressed input data, but don\'t\n\
    write the decompressed data anywhere.\n",
    +[](Arguments& args) {
      string data = read_input_data(args);
      size_t input_bytes = data.size();
      size_t output_bytes = prs_decompress_size(data);
      log_info("%zu (0x%zX) bytes input => %zu (0x%zX) bytes output",
          input_bytes, input_bytes, output_bytes, output_bytes);
    });

Action a_disassemble_prs(
    "disassemble-prs", nullptr, +[](Arguments& args) {
      prs_disassemble(stdout, read_input_data(args));
    });
Action a_disassemble_bc0(
    "disassemble-bc0", "\
  disassemble-prs [INPUT-FILENAME]\n\
  disassemble-bc0 [INPUT-FILENAME]\n\
    Write a textual representation of the commands contained in a PRS or BC0\n\
    command stream. The output is written to stdout. This is mainly useful for\n\
    debugging the compressors and decompressors themselves.\n",
    +[](Arguments& args) {
      bc0_disassemble(stdout, read_input_data(args));
    });

static void a_encrypt_decrypt_fn(Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-data");
  string seed = args.get<string>("seed");
  bool is_big_endian = args.get<bool>("big-endian");
  auto version = get_cli_version(args);

  shared_ptr<PSOEncryption> crypt;
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_V2:
    case Version::GC_NTE:
      crypt.reset(new PSOV2Encryption(stoul(seed, nullptr, 16)));
      break;
    case Version::GC_V3:
    case Version::XB_V3:
    case Version::GC_EP3_TRIAL_EDITION:
    case Version::GC_EP3:
      crypt.reset(new PSOV3Encryption(stoul(seed, nullptr, 16)));
      break;
    case Version::BB_V4: {
      string key_name = args.get<string>("key");
      if (key_name.empty()) {
        throw runtime_error("the --key option is required for BB");
      }
      seed = parse_data_string(seed, nullptr, ParseDataFlags::ALLOW_FILES);
      auto key = load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + key_name + ".nsk");
      crypt.reset(new PSOBBEncryption(key, seed.data(), seed.size()));
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
      dwords[x] = bswap32(dwords[x]);
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
      dwords[x] = bswap32(dwords[x]);
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

static void a_encrypt_decrypt_trivial_fn(Arguments& args) {
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
    fprintf(stderr, "Basis appears to be %02hhX (%zu zero bytes in output)\n",
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
    "decrypt-registry-value", nullptr, +[](Arguments& args) {
      string data = read_input_data(args);
      string out_data = decrypt_v2_registry_value(data.data(), data.size());
      write_output_data(args, out_data.data(), out_data.size(), "dec");
    });

Action a_encrypt_challenge_data(
    "encrypt-challenge-data", nullptr, +[](Arguments& args) {
      string data = read_input_data(args);
      encrypt_challenge_rank_text_t<uint8_t>(data.data(), data.size());
      write_output_data(args, data.data(), data.size(), "dec");
    });
Action a_decrypt_challenge_data(
    "decrypt-challenge-data", "\
  encrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt data using the challenge mode trivial algorithm.\n",
    +[](Arguments& args) {
      string data = read_input_data(args);
      decrypt_challenge_rank_text_t<uint8_t>(data.data(), data.size());
      write_output_data(args, data.data(), data.size(), "dec");
    });

static void a_encrypt_decrypt_gci_save_fn(Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-gci-save");
  bool skip_checksum = args.get<bool>("skip-checksum");
  string seed = args.get<string>("seed");
  string system_filename = args.get<string>("sys");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, Arguments::IntFormat::HEX);

  uint32_t round1_seed;
  if (!system_filename.empty()) {
    string system_data = load_file(system_filename);
    StringReader r(system_data);
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
  StringReader r(data);
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
        charfile->characters[z].ep3_config.encrypt(random_object<uint8_t>());
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

static void a_encrypt_decrypt_pc_save_fn(Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-pc-save");
  bool skip_checksum = args.get<bool>("skip-checksum");
  string seed = args.get<string>("seed");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, Arguments::IntFormat::HEX);

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
            charfile->entries[z].character = decrypt_fixed_size_data_section_t<PSOPCCharacterFile::CharacterEntry::Character, false>(
                &charfile->entries[z].character, sizeof(charfile->entries[z].character), round1_seed, skip_checksum, override_round2_seed);
          } catch (const exception& e) {
            fprintf(stderr, "warning: cannot decrypt character %zu: %s\n", z, e.what());
          }
        }
      }
    } else {
      for (size_t z = 0; z < charfile->entries.size(); z++) {
        if (charfile->entries[z].present) {
          string encrypted = encrypt_fixed_size_data_section_t<PSOPCCharacterFile::CharacterEntry::Character, false>(
              charfile->entries[z].character, round1_seed);
          if (encrypted.size() != sizeof(PSOPCCharacterFile::CharacterEntry::Character)) {
            throw logic_error("incorrect encrypted result size");
          }
          charfile->entries[z].character = *reinterpret_cast<const PSOPCCharacterFile::CharacterEntry::Character*>(encrypted.data());
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

static void a_encrypt_decrypt_save_data_fn(Arguments& args) {
  bool is_decrypt = (args.get<string>(0) == "decrypt-save-data");
  bool skip_checksum = args.get<bool>("skip-checksum");
  bool is_big_endian = args.get<bool>("big-endian");
  string seed = args.get<string>("seed");
  int64_t override_round2_seed = args.get<int64_t>("round2-seed", -1, Arguments::IntFormat::HEX);
  size_t bytes = args.get<size_t>("bytes", 0);

  if (seed.empty()) {
    throw runtime_error("--seed must be given to specify the round1 seed");
  }
  uint32_t round1_seed = stoul(seed, nullptr, 16);

  auto data = read_input_data(args);
  StringReader r(data);

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

// TODO: Write usage text for these actions
Action a_decrypt_save_data("decrypt-save-data", nullptr, a_encrypt_decrypt_save_data_fn);
Action a_encrypt_save_data("encrypt-save-data", nullptr, a_encrypt_decrypt_save_data_fn);

Action a_decode_gci_snapshot(
    "decode-gci-snapshot", "\
  decode-gci-snapshot [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a PSO GC snapshot file into a Windows BMP image.\n",
    +[](Arguments& args) {
      auto data = read_input_data(args);
      StringReader r(data);
      const auto& header = r.get<PSOGCIFileHeader>();
      try {
        header.check();
      } catch (const exception& e) {
        log_warning("File header failed validation (%s)", e.what());
      }
      const auto& file = r.get<PSOGCSnapshotFile>();
      if (!file.checksum_correct()) {
        log_warning("File internal checksum is incorrect");
      }

      auto img = file.decode_image();
      string saved = img.save(Image::Format::WINDOWS_BITMAP);
      write_output_data(args, saved.data(), saved.size(), "bmp");
    });

Action a_encode_gvm(
    "encode-gvm", "\
  encode-gvm [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encode an image in BMP or PPM/PNM format into a GVM texture. The resulting\n\
    GVM file can be used as an Episode 3 lobby banner.\n",
    +[](Arguments& args) {
      const string& input_filename = args.get<string>(1, false);
      Image img;
      if (!input_filename.empty() && (input_filename != "-")) {
        img = Image(input_filename);
      } else {
        img = Image(stdin);
      }
      string encoded = encode_gvm(img, img.get_has_alpha() ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565);
      write_output_data(args, encoded.data(), encoded.size(), "gvm");
    });

Action a_salvage_gci(
    "salvage-gci", "\
  salvage-gci INPUT-FILENAME [--round2] [CRYPT-OPTION] [--bytes=SIZE]\n\
    Attempt to find either the round-1 or round-2 decryption seed for a\n\
    corrupted GCI file. If --round2 is given, then CRYPT-OPTION must be given\n\
    (and should specify either a valid system file or the round1 seed).\n",
    +[](Arguments& args) {
      bool round2 = args.get<bool>("round2");
      string seed = args.get<string>("seed");
      string system_filename = args.get<string>("sys");
      size_t num_threads = args.get<size_t>("threads", 0);
      size_t offset = args.get<size_t>("offset", 0);
      size_t stride = args.get<size_t>("stride", 1);
      size_t bytes = args.get<size_t>("bytes", 0);

      uint64_t likely_round1_seed = 0xFFFFFFFFFFFFFFFF;
      if (!system_filename.empty()) {
        try {
          string system_data = load_file(system_filename);
          StringReader r(system_data);
          const auto& header = r.get<PSOGCIFileHeader>();
          header.check();
          const auto& system = r.get<PSOGCSystemFile>();
          likely_round1_seed = system.creation_timestamp;
          log_info("System file appears to be in order; round1 seed is %08" PRIX64, likely_round1_seed);
        } catch (const exception& e) {
          log_warning("Cannot parse system file (%s); ignoring it", e.what());
        }
      } else if (!seed.empty()) {
        likely_round1_seed = stoul(seed, nullptr, 16);
        log_info("Specified round1 seed is %08" PRIX64, likely_round1_seed);
      }

      if (round2 && likely_round1_seed > 0x100000000) {
        throw invalid_argument("cannot find round2 seed without known round1 seed");
      }

      auto data = read_input_data(args);
      StringReader r(data);
      const auto& header = r.get<PSOGCIFileHeader>();
      header.check();

      const void* data_section = r.getv(header.data_size);

      auto process_file = [&]<typename StructT>() {
        vector<multimap<size_t, uint32_t>> top_seeds_by_thread(
            num_threads ? num_threads : thread::hardware_concurrency());
        parallel_range<uint64_t>(
            [&](uint64_t seed, size_t thread_num) -> bool {
              size_t zero_count;
              if (round2) {
                string decrypted = decrypt_gci_fixed_size_data_section_for_salvage(
                    data_section, header.data_size, likely_round1_seed, seed, bytes);
                zero_count = count_zeroes(
                    decrypted.data() + offset,
                    decrypted.size() - offset,
                    stride);
              } else {
                auto decrypted = decrypt_fixed_size_data_section_t<StructT, true>(
                    data_section,
                    header.data_size,
                    seed,
                    true);
                zero_count = count_zeroes(
                    reinterpret_cast<const uint8_t*>(&decrypted) + offset,
                    sizeof(decrypted) - offset,
                    stride);
              }
              auto& top_seeds = top_seeds_by_thread[thread_num];
              if (top_seeds.size() < 10 || (zero_count >= top_seeds.begin()->second)) {
                top_seeds.emplace(zero_count, seed);
                if (top_seeds.size() > 10) {
                  top_seeds.erase(top_seeds.begin());
                }
              }
              return false;
            },
            0,
            0x100000000,
            num_threads);

        multimap<size_t, uint32_t> top_seeds;
        for (const auto& thread_top_seeds : top_seeds_by_thread) {
          for (const auto& it : thread_top_seeds) {
            top_seeds.emplace(it.first, it.second);
          }
        }
        for (const auto& it : top_seeds) {
          const char* sys_seed_str = (!round2 && (it.second == likely_round1_seed))
              ? " (this is the seed from the system file)"
              : "";
          log_info("Round %c seed %08" PRIX32 " resulted in %zu zero bytes%s",
              round2 ? '2' : '1', it.second, it.first, sys_seed_str);
        }
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
    +[](Arguments& args) {
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
        string data = parse_data_string(plaintext_ascii, &mask, ParseDataFlags::ALLOW_FILES);
        if (data.size() != mask.size()) {
          throw logic_error("plaintext and mask are not the same size");
        }
        max_plaintext_size = max<size_t>(max_plaintext_size, data.size());
        plaintexts.emplace_back(std::move(data), std::move(mask));
      }
      string ciphertext = parse_data_string(ciphertext_ascii, nullptr, ParseDataFlags::ALLOW_FILES);

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

      uint64_t seed = parallel_range<uint64_t>([&](uint64_t seed, size_t) -> bool {
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
          0, 0x100000000, num_threads);

      if (seed < 0x100000000) {
        log_info("Found seed %08" PRIX64, seed);
      } else {
        log_error("No seed found");
      }
    });

Action a_decode_gci(
    "decode-gci", nullptr, +[](Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      string seed = args.get<string>("seed");
      size_t num_threads = args.get<size_t>("threads", 0);
      bool skip_checksum = args.get<bool>("skip-checksum");
      int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
      auto decoded = decode_gci_data(read_input_data(args), num_threads, dec_seed, skip_checksum);
      save_file(input_filename + ".dec", decoded);
    });
Action a_decode_vmg(
    "decode-vms", nullptr, +[](Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      string seed = args.get<string>("seed");
      size_t num_threads = args.get<size_t>("threads", 0);
      bool skip_checksum = args.get<bool>("skip-checksum");
      int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
      auto decoded = decode_vms_data(read_input_data(args), num_threads, dec_seed, skip_checksum);
      save_file(input_filename + ".dec", decoded);
    });
Action a_decode_dlq(
    "decode-dlq", nullptr, +[](Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      auto decoded = decode_dlq_data(read_input_data(args));
      save_file(input_filename + ".dec", decoded);
    });
Action a_decode_qst(
    "decode-qst", "\
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
    newserv will find it via a brute-force search, which will take a long time.\n",
    +[](Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      auto files = decode_qst_data(read_input_data(args));
      for (const auto& it : files) {
        save_file(input_filename + "-" + it.first, it.second);
      }
    });

Action a_encode_qst(
    "encode-qst", "\
  encode-qst INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS...]\n\
    Encode the input quest file (in .bin/.dat format) into a .qst file. If\n\
    --download is given, generates a download .qst instead of an online .qst.\n\
    Specify the quest\'s game version with one of the --dc-nte, --dc-v1,\n\
    --dc-v2, --pc, --gc-nte, --gc, --gc-ep3, --xb, or --bb options.\n",
    +[](Arguments& args) {
      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw invalid_argument("an input filename is required");
      }
      auto version = get_cli_version(args);
      bool download = args.get<bool>("download");

      string bin_filename = input_filename;
      string dat_filename = ends_with(bin_filename, ".bin")
          ? (bin_filename.substr(0, bin_filename.size() - 3) + "dat")
          : (bin_filename + ".dat");
      string pvr_filename = ends_with(bin_filename, ".bin")
          ? (bin_filename.substr(0, bin_filename.size() - 3) + "pvr")
          : (bin_filename + ".pvr");
      shared_ptr<string> bin_data(new string(load_file(bin_filename)));
      shared_ptr<string> dat_data(new string(load_file(dat_filename)));
      shared_ptr<string> pvr_data;
      try {
        pvr_data.reset(new string(load_file(pvr_filename)));
      } catch (const cannot_open_file&) {
      }
      shared_ptr<VersionedQuest> vq(new VersionedQuest(0, 0, version, 0, bin_data, dat_data, pvr_data));
      if (download) {
        vq = vq->create_download_quest();
      }
      string qst_data = vq->encode_qst();

      write_output_data(args, qst_data.data(), qst_data.size(), "qst");
    });

Action a_disassemble_quest_script(
    "disassemble-quest-script", "\
  disassemble-quest-script [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Disassemble the input quest script (.bin file) into a text representation\n\
    of the commands and metadata it contains. Specify the quest\'s game version\n\
    with one of the --dc-nte, --dc-v1, --dc-v2, --pc, --gc-nte, --gc, --gc-ep3,\n\
    --xb, or --bb options.\n",
    +[](Arguments& args) {
      string data = read_input_data(args);
      auto version = get_cli_version(args);
      if (!args.get<bool>("decompressed")) {
        data = prs_decompress(data);
      }
      uint8_t language = args.get<bool>("japanese") ? 0 : 1;
      string result = disassemble_quest_script(data.data(), data.size(), version, language);
      write_output_data(args, result.data(), result.size(), "txt");
    });
Action a_disassemble_quest_map(
    "disassemble-quest-map", "\
  disassemble-quest-map [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Disassemble the input quest map (.dat file) into a text representation of\n\
    the data it contains. Specify the quest\'s game version with one of the\n\
    --dc-nte, --dc-v1, --dc-v2, --pc, --gc-nte, --gc, --xb, or --bb options.\n",
    +[](Arguments& args) {
      string data = read_input_data(args);
      if (!args.get<bool>("decompressed")) {
        data = prs_decompress(data);
      }
      string result = Map::disassemble_quest_data(data.data(), data.size());
      write_output_data(args, result.data(), result.size(), "txt");
    });

void a_extract_archive_fn(Arguments& args) {
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
  shared_ptr<string> data_shared(new string(std::move(data)));

  if (args.get<string>(0) == "extract-afs") {
    AFSArchive arch(data_shared);
    const auto& all_entries = arch.all_entries();
    for (size_t z = 0; z < all_entries.size(); z++) {
      auto e = arch.get(z);
      string out_file = string_printf("%s-%zu", output_prefix.c_str(), z);
      save_file(out_file.c_str(), e.first, e.second);
      fprintf(stderr, "... %s\n", out_file.c_str());
    }
  } else if (args.get<string>(0) == "extract-gsl") {
    GSLArchive arch(data_shared, args.get<bool>("big-endian"));
    for (const auto& entry_it : arch.all_entries()) {
      auto e = arch.get(entry_it.first);
      string out_file = output_prefix + entry_it.first;
      save_file(out_file.c_str(), e.first, e.second);
      fprintf(stderr, "... %s\n", out_file.c_str());
    }
  } else if (args.get<string>(0) == "extract-bml") {
    BMLArchive arch(data_shared, args.get<bool>("big-endian"));
    for (const auto& entry_it : arch.all_entries()) {
      {
        auto e = arch.get(entry_it.first);
        string data = prs_decompress(e.first, e.second);
        string out_file = output_prefix + entry_it.first;
        save_file(out_file, data);
        fprintf(stderr, "... %s\n", out_file.c_str());
      }

      auto gvm_e = arch.get_gvm(entry_it.first);
      if (gvm_e.second) {
        string data = prs_decompress(gvm_e.first, gvm_e.second);
        string out_file = output_prefix + entry_it.first + ".gvm";
        save_file(out_file, data);
        fprintf(stderr, "... %s\n", out_file.c_str());
      }
    }
  } else {
    throw logic_error("unimplemented archive type");
  }
}

Action a_extract_afs("extract-afs", nullptr, a_extract_archive_fn);
Action a_extract_gsl("extract-gsl", nullptr, a_extract_archive_fn);
Action a_extract_bml("extract-bml", "\
  extract-afs [INPUT-FILENAME] [--big-endian]\n\
  extract-gsl [INPUT-FILENAME] [--big-endian]\n\
  extract-bml [INPUT-FILENAME] [--big-endian]\n\
    Extract all files from an AFS, GSL, or BML archive into the current\n\
    directory. input-filename may be specified. If output-filename is\n\
    specified, then it is treated as a prefix which is prepended to the\n\
    filename of each file contained in the archive. If --big-endian is given,\n\
    the archive header is read in GameCube format; otherwise it is read in\n\
    PC/BB format.\n",
    a_extract_archive_fn);

Action a_decode_text_archive(
    "decode-text-archive", nullptr, +[](Arguments& args) {
      string data = read_input_data(args);
      TextArchive a(data, args.get<bool>("big-endian"));
      JSON j = a.json();
      string out_data = j.serialize(JSON::SerializeOption::FORMAT);
      write_output_data(args, out_data.data(), out_data.size(), "json");
    });
Action a_encode_text_archive(
    "encode-text-archive", "\
  decode-text-archive [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  encode-text-archive [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a text archive (e.g. TextEnglish.pr2) to JSON for easy editing, or\n\
    encode a JSON file to a text archive.\n",
    +[](Arguments& args) {
      const string& input_filename = args.get<string>(1, false);
      const string& output_filename = args.get<string>(2, false);

      auto json = JSON::parse(read_input_data(args));
      TextArchive a(json);
      auto result = a.serialize(args.get<bool>("big-endian"));
      if (output_filename.empty()) {
        if (input_filename.empty() || (input_filename == "-")) {
          throw runtime_error("encoded text archive cannot be written to stdout");
        }
        save_file(string_printf("%s.pr2", input_filename.c_str()), result.first);
        save_file(string_printf("%s.pr3", input_filename.c_str()), result.second);
      } else if (output_filename == "-") {
        throw runtime_error("encoded text archive cannot be written to stdout");
      } else {
        string out_filename = output_filename;
        if (ends_with(out_filename, ".pr2")) {
          save_file(out_filename, result.first);
          out_filename[out_filename.size() - 1] = '3';
          save_file(out_filename, result.second);
        } else {
          save_file(out_filename + ".pr2", result.first);
          save_file(out_filename + ".pr3", result.second);
        }
      }
    });

Action a_decode_unicode_text_set(
    "decode-unicode-text-set", nullptr, +[](Arguments& args) {
      auto collections = parse_unicode_text_set(read_input_data(args));
      JSON j = JSON::list();
      for (const auto& collection : collections) {
        JSON& coll_j = j.emplace_back(JSON::list());
        for (const auto& s : collection) {
          coll_j.emplace_back(s);
        }
      }
      string out_data = j.serialize(JSON::SerializeOption::FORMAT);
      write_output_data(args, out_data.data(), out_data.size(), "json");
    });
Action a_encode_unicode_text_set(
    "encode-unicode-text-set", "\
  decode-unicode-text-set [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  encode-unicode-text-set [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decode a Unicode text set (e.g. unitxt_e.prs) to JSON for easy editing, or\n\
    encode a JSON file to a Unicode text set.\n",
    +[](Arguments& args) {
      auto json = JSON::parse(read_input_data(args));
      vector<vector<string>> collections;
      for (const auto& coll_json : json.as_list()) {
        auto& collection = collections.emplace_back();
        for (const auto& s_json : coll_json->as_list()) {
          collection.emplace_back(std::move(s_json->as_string()));
        }
      }
      string encoded = serialize_unicode_text_set(collections);
      write_output_data(args, encoded.data(), encoded.size(), "prs");
    });

Action a_cat_client(
    "cat-client", "\
  cat-client ADDR:PORT\n\
    Connect to the given server and simulate a PSO client. newserv will then\n\
    print all the received commands to stdout, and forward any commands typed\n\
    into stdin to the remote server. It is assumed that the input and output\n\
    are terminals, so all commands are hex-encoded. The --patch, --dc, --pc,\n\
    --gc, and --bb options can be used to select the command format and\n\
    encryption. If --bb is used, the --key=KEY-NAME option is also required (as\n\
    in decrypt-data above).\n",
    +[](Arguments& args) {
      auto version = get_cli_version(args);
      shared_ptr<PSOBBEncryption::KeyFile> key;
      if (uses_v4_encryption(version)) {
        string key_file_name = args.get<string>("key");
        if (key_file_name.empty()) {
          throw runtime_error("a key filename is required for BB client emulation");
        }
        key.reset(new PSOBBEncryption::KeyFile(
            load_object_file<PSOBBEncryption::KeyFile>("system/blueburst/keys/" + key_file_name + ".nsk")));
      }
      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      auto cat_client_remote = make_sockaddr_storage(parse_netloc(args.get<string>(1))).first;
      CatSession session(base, cat_client_remote, get_cli_version(args), key);
      event_base_dispatch(base.get());
    });

Action a_convert_rare_item_set(
    "convert-rare-item-set", "\
  convert-rare-item-set INPUT-FILENAME [OUTPUT-FILENAME]\n\
    If OUTPUT-FILENAME is not given, print the contents of a rare item table in\n\
    a human-readable format. Otherwise, convert the input rare item set to a\n\
    different format and write it to OUTPUT-FILENAME. Both filenames must end\n\
    in one of the following extensions:\n\
      .json (newserv JSON rare item table)\n\
      .gsl (PSO BB little-endian GSL archive)\n\
      .gslb (PSO GC big-endian GSL archive)\n\
      .afs (PSO V2 little-endian AFS archive)\n\
      .rel (Schtserv rare table; cannot be used in output filename)\n",
    +[](Arguments& args) {
      auto name_index = make_shared<ItemNameIndex>(
          JSON::parse(load_file("system/item-tables/names-v2.json")),
          JSON::parse(load_file("system/item-tables/names-v3.json")),
          JSON::parse(load_file("system/item-tables/names-v4.json")));

      string input_filename = args.get<string>(1, false);
      if (input_filename.empty() || (input_filename == "-")) {
        throw runtime_error("input filename must be given");
      }
      auto version = get_cli_version(args);

      shared_ptr<string> data(new string(read_input_data(args)));
      shared_ptr<RareItemSet> rs;
      if (ends_with(input_filename, ".json")) {
        rs.reset(new RareItemSet(JSON::parse(*data), version, name_index));
      } else if (ends_with(input_filename, ".gsl")) {
        rs.reset(new RareItemSet(GSLArchive(data, false), false));
      } else if (ends_with(input_filename, ".gslb")) {
        rs.reset(new RareItemSet(GSLArchive(data, true), true));
      } else if (ends_with(input_filename, ".afs")) {
        rs.reset(new RareItemSet(AFSArchive(data), is_v1(version)));
      } else if (ends_with(input_filename, ".rel")) {
        rs.reset(new RareItemSet(*data, true));
      } else {
        throw runtime_error("cannot determine input format; use a filename ending with .json, .gsl, .gslb, .afs, or .rel");
      }

      string output_filename = args.get<string>(2, false);
      if (output_filename.empty() || (output_filename == "-")) {
        rs->print_all_collections(stdout, version, name_index);
      } else if (ends_with(output_filename, ".json")) {
        string data = rs->serialize_json(version, name_index);
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (ends_with(output_filename, ".gsl")) {
        string data = rs->serialize_gsl(args.get<bool>("big-endian"));
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (ends_with(output_filename, ".gslb")) {
        string data = rs->serialize_gsl(true);
        write_output_data(args, data.data(), data.size(), nullptr);
      } else if (ends_with(output_filename, ".afs")) {
        string data = rs->serialize_afs();
        write_output_data(args, data.data(), data.size(), nullptr);
      } else {
        throw runtime_error("cannot determine output format; use a filename ending with .json, .gsl, .gslb, or .afs");
      }
    });

Action a_describe_item(
    "describe-item", "\
  describe-item DATA-OR-DESCRIPTION\n\
    Describe an item. The argument may be the item\'s raw hex code or a textual\n\
    description of the item. If the description contains spaces, it must be\n\
    quoted, such as \"L&K14 COMBAT +10 0/10/15/0/35\".\n",
    +[](Arguments& args) {
      string description = args.get<string>(1);
      auto version = get_cli_version(args);

      auto name_index = make_shared<ItemNameIndex>(
          JSON::parse(load_file("system/item-tables/names-v2.json")),
          JSON::parse(load_file("system/item-tables/names-v3.json")),
          JSON::parse(load_file("system/item-tables/names-v4.json")));
      shared_ptr<string> pmt_data_v2(new string(prs_decompress(load_file("system/item-tables/ItemPMT-v2.prs"))));
      auto pmt_v2 = make_shared<ItemParameterTable>(pmt_data_v2, ItemParameterTable::Version::V2);
      shared_ptr<string> pmt_data_v3(new string(prs_decompress(load_file("system/item-tables/ItemPMT-gc.prs"))));
      auto pmt_v3 = make_shared<ItemParameterTable>(pmt_data_v3, ItemParameterTable::Version::V3);
      shared_ptr<string> pmt_data_v4(new string(prs_decompress(load_file("system/item-tables/ItemPMT-bb.prs"))));
      auto pmt_v4 = make_shared<ItemParameterTable>(pmt_data_v4, ItemParameterTable::Version::V4);

      ItemData item = name_index->parse_item_description(version, description);

      string desc = name_index->describe_item(version, item);
      log_info("Data (decoded):    %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
          item.data1[0], item.data1[1], item.data1[2], item.data1[3],
          item.data1[4], item.data1[5], item.data1[6], item.data1[7],
          item.data1[8], item.data1[9], item.data1[10], item.data1[11],
          item.data2[0], item.data2[1], item.data2[2], item.data2[3]);

      ItemData item_v1 = item;
      item_v1.encode_for_version(Version::PC_V2, pmt_v2);
      ItemData item_v1_decoded = item_v1;
      item_v1_decoded.decode_for_version(Version::PC_V2);

      log_info("Data (V1-encoded): %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
          item_v1.data1[0], item_v1.data1[1], item_v1.data1[2], item_v1.data1[3],
          item_v1.data1[4], item_v1.data1[5], item_v1.data1[6], item_v1.data1[7],
          item_v1.data1[8], item_v1.data1[9], item_v1.data1[10], item_v1.data1[11],
          item_v1.data2[0], item_v1.data2[1], item_v1.data2[2], item_v1.data2[3]);
      if (item_v1_decoded != item) {
        log_warning("V1-decoded data does not match original data");
        log_warning("Data (V1-decoded): %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
            item_v1_decoded.data1[0], item_v1_decoded.data1[1], item_v1_decoded.data1[2], item_v1_decoded.data1[3],
            item_v1_decoded.data1[4], item_v1_decoded.data1[5], item_v1_decoded.data1[6], item_v1_decoded.data1[7],
            item_v1_decoded.data1[8], item_v1_decoded.data1[9], item_v1_decoded.data1[10], item_v1_decoded.data1[11],
            item_v1_decoded.data2[0], item_v1_decoded.data2[1], item_v1_decoded.data2[2], item_v1_decoded.data2[3]);
      }

      ItemData item_gc = item;
      item_gc.encode_for_version(Version::GC_V3, pmt_v3);
      ItemData item_gc_decoded = item_gc;
      item_gc_decoded.decode_for_version(Version::GC_V3);

      log_info("Data (GC-encoded): %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
          item_gc.data1[0], item_gc.data1[1], item_gc.data1[2], item_gc.data1[3],
          item_gc.data1[4], item_gc.data1[5], item_gc.data1[6], item_gc.data1[7],
          item_gc.data1[8], item_gc.data1[9], item_gc.data1[10], item_gc.data1[11],
          item_gc.data2[0], item_gc.data2[1], item_gc.data2[2], item_gc.data2[3]);
      if (item_gc_decoded != item) {
        log_warning("GC-decoded data does not match original data");
        log_warning("Data (GC-decoded): %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
            item_gc_decoded.data1[0], item_gc_decoded.data1[1], item_gc_decoded.data1[2], item_gc_decoded.data1[3],
            item_gc_decoded.data1[4], item_gc_decoded.data1[5], item_gc_decoded.data1[6], item_gc_decoded.data1[7],
            item_gc_decoded.data1[8], item_gc_decoded.data1[9], item_gc_decoded.data1[10], item_gc_decoded.data1[11],
            item_gc_decoded.data2[0], item_gc_decoded.data2[1], item_gc_decoded.data2[2], item_gc_decoded.data2[3]);
      }

      log_info("Description: %s", desc.c_str());

      size_t purchase_price = pmt_v4->price_for_item(item);
      size_t sale_price = purchase_price >> 3;
      log_info("Purchase price: %zu; sale price: %zu", purchase_price, sale_price);
    });

Action a_show_ep3_cards(
    "show-ep3-cards", "\
  show-ep3-cards\n\
    Print the Episode 3 card definitions from the system/ep3 directory in a\n\
    human-readable format.\n",
    +[](Arguments& args) {
      bool one_line = args.get<bool>("one-line");

      Episode3::CardIndex card_index(
          "system/ep3/card-definitions.mnr",
          "system/ep3/card-definitions.mnrd",
          "system/ep3/card-text.mnr",
          "system/ep3/card-text.mnrd",
          "system/ep3/card-dice-text.mnr",
          "system/ep3/card-dice-text.mnrd");
      unique_ptr<TextArchive> text_english;
      try {
        JSON json = JSON::parse(load_file("system/ep3/text-english.json"));
        text_english.reset(new TextArchive(json));
      } catch (const exception& e) {
      }

      auto card_ids = card_index.all_ids();
      log_info("%zu card definitions", card_ids.size());
      for (uint32_t card_id : card_ids) {
        auto entry = card_index.definition_for_id(card_id);
        string s = entry->def.str(one_line, text_english.get());
        if (one_line) {
          fprintf(stdout, "%s\n", s.c_str());
        } else {
          fprintf(stdout, "%s\n", s.c_str());
          if (!entry->debug_tags.empty()) {
            string tags = join(entry->debug_tags, ", ");
            fprintf(stdout, "  Tags: %s\n", tags.c_str());
          }
          if (!entry->dice_caption.empty()) {
            fprintf(stdout, "  Dice caption: %s\n", entry->dice_caption.c_str());
          }
          if (!entry->dice_caption.empty()) {
            fprintf(stdout, "  Dice text: %s\n", entry->dice_text.c_str());
          }
          if (!entry->text.empty()) {
            string text = str_replace_all(entry->text, "\n", "\n    ");
            strip_trailing_whitespace(text);
            fprintf(stdout, "  Text:\n    %s\n", text.c_str());
          }
          fputc('\n', stdout);
        }
      }
    });

Action a_generate_ep3_cards_html(
    "generate-ep3-cards-html", "\
  generate-ep3-cards-html\n\
    Generate an HTML file describing all Episode 3 card definitions from the\n\
    system/ep3 directory.\n",
    +[](Arguments& args) {
      size_t num_threads = args.get<size_t>("threads", 0);

      Episode3::CardIndex card_index(
          "system/ep3/card-definitions.mnr",
          "system/ep3/card-definitions.mnrd",
          "system/ep3/card-text.mnr",
          "system/ep3/card-text.mnrd",
          "system/ep3/card-dice-text.mnr",
          "system/ep3/card-dice-text.mnrd");
      unique_ptr<TextArchive> text_english;
      try {
        JSON json = JSON::parse(load_file("system/ep3/text-english.json"));
        text_english.reset(new TextArchive(json));
      } catch (const exception& e) {
      }

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
      vector<CardInfo> infos;
      for (uint32_t card_id : card_index.all_ids()) {
        if (infos.size() <= card_id) {
          infos.resize(card_id + 1);
        }
        infos[card_id].ce = card_index.definition_for_id(card_id);
      }
      for (const auto& filename : list_directory_sorted("system/ep3/cardtex")) {
        if ((filename[0] == 'C' || filename[0] == 'M' || filename[0] == 'L') && (filename[1] == '_')) {
          size_t card_id = stoull(filename.substr(2, 3), nullptr, 10);
          if (infos.size() <= card_id) {
            infos.resize(card_id + 1);
          }
          auto& info = infos[card_id];
          if (filename[0] == 'C') {
            info.large_filename = "system/ep3/cardtex/" + filename;
          } else if (filename[0] == 'L') {
            info.medium_filename = "system/ep3/cardtex/" + filename;
          } else if (filename[0] == 'M') {
            info.small_filename = "system/ep3/cardtex/" + filename;
          }
        }
      }

      parallel_range<uint32_t>([&](uint32_t index, size_t) -> bool {
        auto& info = infos[index];
        if (!info.large_filename.empty()) {
          Image img(info.large_filename);
          Image cropped(512, 399);
          cropped.blit(img, 0, 0, 512, 399, 0, 0);
          info.large_data_url = cropped.png_data_url();
        }
        if (!info.medium_filename.empty()) {
          Image img(info.medium_filename);
          Image cropped(184, 144);
          cropped.blit(img, 0, 0, 184, 144, 0, 0);
          info.medium_data_url = cropped.png_data_url();
        }
        if (!info.small_filename.empty()) {
          Image img(info.small_filename);
          Image cropped(58, 43);
          cropped.blit(img, 0, 0, 58, 43, 0, 0);
          info.small_data_url = cropped.png_data_url();
        }
        return false;
      },
          0, infos.size(), num_threads);

      deque<string> blocks;
      blocks.emplace_back("<html><head><title>Phantasy Star Online Episode III cards</title></head><body style=\"background-color:#222222; color: #EEEEEE\">");
      blocks.emplace_back("<table><tr><th style=\"text-align: left\">Legend:</th></tr><tr style=\"background-color: #663333\"><td>Card has no definition and is obviously incomplete</td></tr><tr style=\"background-color: #336633\"><td>Card is unobtainable in random draws but may be a quest or event reward</td></tr><tr style=\"background-color: #333333\"><td>Card is obtainable in random draws</td></tr></table><br /><br />");
      blocks.emplace_back("<table><tr><th style=\"text-align: left; padding: 4px\">ID</th><th style=\"text-align: left; padding: 4px\">Small</th><th style=\"text-align: left; padding: 4px\">Medium</th><th style=\"text-align: left; padding: 4px\">Large</th><th style=\"text-align: left; padding: 4px\">Text</th><th style=\"text-align: left; padding: 4px\">Disassembly</th></tr>");
      for (size_t card_id = 0; card_id < infos.size(); card_id++) {
        const auto& entry = infos[card_id];
        if (entry.is_empty()) {
          continue;
        }

        const char* background_color;
        if (!entry.ce) {
          background_color = "#663333";
        } else if (entry.ce->def.cannot_drop ||
            ((entry.ce->def.rank == Episode3::CardRank::D1) || (entry.ce->def.rank == Episode3::CardRank::D2) || (entry.ce->def.rank == Episode3::CardRank::D3)) ||
            ((entry.ce->def.card_class() == Episode3::CardClass::BOSS_ATTACK_ACTION) || (entry.ce->def.card_class() == Episode3::CardClass::BOSS_TECH)) ||
            ((entry.ce->def.drop_rates[0] == 6) && (entry.ce->def.drop_rates[1] == 6))) {
          background_color = "#336633";
        } else {
          background_color = "#333333";
        }

        blocks.emplace_back(string_printf("<tr style=\"background-color: %s\">", background_color));
        blocks.emplace_back(string_printf("<td style=\"padding: 4px; vertical-align: top\"><pre>%04zX</pre></td><td style=\"padding: 4px; vertical-align: top\">", card_id));
        if (!entry.small_data_url.empty()) {
          blocks.emplace_back("<img src=\"");
          blocks.emplace_back(std::move(entry.small_data_url));
          blocks.emplace_back("\" />");
        }
        blocks.emplace_back("</td><td style=\"padding: 4px; vertical-align: top\">");
        if (!entry.medium_data_url.empty()) {
          blocks.emplace_back("<img src=\"");
          blocks.emplace_back(std::move(entry.medium_data_url));
          blocks.emplace_back("\" />");
        }
        blocks.emplace_back("</td><td style=\"padding: 4px; vertical-align: top\">");
        if (!entry.large_data_url.empty()) {
          blocks.emplace_back("<img src=\"");
          blocks.emplace_back(std::move(entry.large_data_url));
          blocks.emplace_back("\" />");
        }
        blocks.emplace_back("</td><td style=\"padding: 4px; vertical-align: top\">");
        if (entry.ce) {
          blocks.emplace_back("<pre>");
          blocks.emplace_back(entry.ce->text);
          blocks.emplace_back("</pre></td><td style=\"padding: 4px; vertical-align: top\"><pre>");
          blocks.emplace_back(entry.ce->def.str(false, text_english.get()));
          blocks.emplace_back("</pre>");
        } else {
          blocks.emplace_back("</td><td style=\"padding: 4px; vertical-align: top\"><pre>Definition is missing</pre>");
        }
        blocks.emplace_back("</td></tr>");
      }
      blocks.emplace_back("</table></body></html>");

      save_file("cards.html", join(blocks, ""));
    });

Action a_show_ep3_maps(
    "show-ep3-maps", "\
  show-ep3-maps\n\
    Print the Episode 3 maps from the system/ep3 directory in a (sort of)\n\
    human-readable format.\n",
    +[](Arguments&) {
      config_log.info("Collecting Episode 3 data");
      Episode3::MapIndex map_index("system/ep3/maps");
      Episode3::CardIndex card_index("system/ep3/card-definitions.mnr", "system/ep3/card-definitions.mnrd");

      auto map_ids = map_index.all_numbers();
      log_info("%zu maps", map_ids.size());
      for (uint32_t map_id : map_ids) {
        auto map = map_index.for_number(map_id);
        const auto& vms = map->all_versions();
        for (size_t language = 0; language < vms.size(); language++) {
          if (!vms[language]) {
            continue;
          }
          string s = vms[language]->map->str(&card_index, language);
          fprintf(stdout, "(%c) %s\n", char_for_language_code(language), s.c_str());
        }
      }
    });

Action a_parse_object_graph(
    "parse-object-graph", nullptr, +[](Arguments& args) {
      uint32_t root_object_address = args.get<uint32_t>("root", Arguments::IntFormat::HEX);
      string data = read_input_data(args);
      PSOGCObjectGraph g(data, root_object_address);
      g.print(stdout);
    });

Action a_generate_dc_serial_number(
    "generate-dc-serial-number", "\
  generate-dc-serial-number DOMAIN SUBDOMAIN\n\
    Generate a PSO DC serial number. DOMAIN should be 0 for Japanese, 1 for\n\
    USA, or 2 for Europe. SUBDOMAIN should be 0 for v1, or 1 for v2.\n",
    +[](Arguments& args) {
      uint8_t domain = args.get<uint8_t>(1);
      uint8_t subdomain = args.get<uint8_t>(2);
      string serial_number = generate_dc_serial_number(domain, subdomain);
      fprintf(stdout, "%s\n", serial_number.c_str());
    });
Action a_generate_all_dc_serial_numbers(
    "generate-all-dc-serial-numbers", "\
  generate-all-dc-serial-numbers\n\
    Generate all possible PSO DC serial numbers.\n",
    +[](Arguments& args) {
      size_t num_threads = args.get<size_t>("threads", 0);

      auto serial_numbers = generate_all_dc_serial_numbers();
      fprintf(stdout, "%zu (0x%zX) serial numbers found\n", serial_numbers.size(), serial_numbers.size());
      for (const auto& it : serial_numbers) {
        fprintf(stdout, "Valid serial number: %08" PRIX32, it.first);
        for (uint8_t where : it.second) {
          fprintf(stdout, " (domain=%hhu, subdomain=%hhu)",
              static_cast<uint8_t>((where >> 2) & 3),
              static_cast<uint8_t>(where & 3));
        }
        fputc('\n', stdout);
      }

      atomic<uint64_t> num_valid_serial_numbers = 0;
      mutex output_lock;
      auto thread_fn = [&](uint64_t serial_number, size_t) -> bool {
        for (uint8_t domain = 0; domain < 3; domain++) {
          for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
            if (dc_serial_number_is_valid_fast(serial_number, domain, subdomain)) {
              num_valid_serial_numbers++;
              lock_guard g(output_lock);
              fprintf(stdout, "Valid serial number: %08" PRIX64 " (domain=%hhu, subdomain=%hhu)\n", serial_number, domain, subdomain);
            }
          }
        }
        return false;
      };
      auto progress_fn = [&](uint64_t, uint64_t, uint64_t current_value, uint64_t) -> void {
        uint64_t num_found = num_valid_serial_numbers.load();
        fprintf(stderr, "... %08" PRIX64 " %" PRId64 " (0x%" PRIX64 ") found\r",
            current_value, num_found, num_found);
      };
      parallel_range<uint64_t>(thread_fn, 0, 0x100000000, num_threads, progress_fn);
    });
Action a_inspect_dc_serial_number(
    "inspect-dc-serial-number", "\
  inspect-dc-serial-number SERIAL-NUMBER\n\
    Show which domain and subdomain the serial number belongs to. (As with\n\
    generate-dc-serial-number, described above, this will tell you which PSO\n\
    version it is valid for.)\n",
    +[](Arguments& args) {
      const string& serial_number_str = args.get<string>(1, false);
      if (serial_number_str.empty()) {
        throw invalid_argument("no serial number given");
      }
      size_t num_valid_subdomains = 0;
      for (uint8_t domain = 0; domain < 3; domain++) {
        for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
          if (dc_serial_number_is_valid_fast(serial_number_str, domain, subdomain)) {
            fprintf(stdout, "%s is valid in domain %hhu subdomain %hhu\n", serial_number_str.c_str(), domain, subdomain);
            num_valid_subdomains++;
          }
        }
      }
      if (num_valid_subdomains == 0) {
        fprintf(stdout, "%s is not valid in any domain\n", serial_number_str.c_str());
      }
    });
Action a_dc_serial_number_speed_test(
    "dc-serial-number-speed-test", "\
  dc-serial-number-speed-test\n\
    Run a speed test of the two DC serial number validation functions.\n",
    +[](Arguments& args) {
      const string& seed = args.get<string>("seed");
      if (seed.empty()) {
        dc_serial_number_speed_test();
      } else {
        dc_serial_number_speed_test(stoul(seed, nullptr, 16));
      }
    });

Action a_ar_code_translator(
    "ar-code-translator", nullptr, +[](Arguments& args) {
      const string& dir = args.get<string>(1, false);
      if (dir.empty() || (dir == "-")) {
        throw invalid_argument("a directory name is required");
      }
      run_ar_code_translator(dir, args.get<string>(2, false), args.get<string>(3, false));
    });

Action a_run_server_replay_log(
    "", nullptr, +[](Arguments& args) {
      string config_filename = args.get<string>("config");
      const string& replay_log_filename = args.get<string>("replay-log");
      bool is_replay = !replay_log_filename.empty();
      if (config_filename.empty()) {
        config_filename = "system/config.json";
      }

      signal(SIGPIPE, SIG_IGN);
      if (isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      if (is_replay) {
        set_function_compiler_available(false);
      }

      shared_ptr<struct event_base> base(event_base_new(), event_base_free);
      shared_ptr<ServerState> state(new ServerState(config_filename, is_replay));
      state->init();

      shared_ptr<DNSServer> dns_server;
      if (state->dns_server_port && !is_replay) {
        config_log.info("Starting DNS server on port %hu", state->dns_server_port);
        dns_server.reset(new DNSServer(base, state->local_address,
            state->external_address));
        dns_server->listen("", state->dns_server_port);
      } else {
        config_log.info("DNS server is disabled");
      }

      shared_ptr<Shell> shell;
      shared_ptr<ReplaySession> replay_session;
      shared_ptr<IPStackSimulator> ip_stack_simulator;
      if (is_replay) {
        config_log.info("Starting proxy server");
        state->proxy_server.reset(new ProxyServer(base, state));
        config_log.info("Starting game server");
        state->game_server.reset(new Server(base, state));

        auto nop_destructor = +[](FILE*) {};
        shared_ptr<FILE> log_f(stdin, nop_destructor);
        if (replay_log_filename != "-") {
          log_f = fopen_shared(replay_log_filename, "rt");
        }

        replay_session.reset(new ReplaySession(base, log_f.get(), state, args.get<bool>("require-basic-credentials")));
        replay_session->start();

      } else {
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
              if (is_patch(pc->version)) {
                auto [ss, size] = make_sockaddr_storage(
                    state->proxy_destination_patch.first,
                    state->proxy_destination_patch.second);
                state->proxy_server->listen(pc->port, pc->version, &ss);
              } else if (is_v4(pc->version)) {
                auto [ss, size] = make_sockaddr_storage(
                    state->proxy_destination_bb.first,
                    state->proxy_destination_bb.second);
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
            string spec = string_printf("T-%hu-%s-%s-%s", pc->port, name_for_enum(pc->version), pc->name.c_str(), name_for_enum(pc->behavior));
            state->game_server->listen(spec, "", pc->port, pc->version, pc->behavior);
          }
        }

        if (!state->ip_stack_addresses.empty()) {
          config_log.info("Starting IP stack simulator");
          ip_stack_simulator.reset(new IPStackSimulator(base, state));
          for (const auto& it : state->ip_stack_addresses) {
            auto netloc = parse_netloc(it);
            string spec = (netloc.second == 0) ? ("T-IPS-" + netloc.first) : string_printf("T-IPS-%hu", netloc.second);
            ip_stack_simulator->listen(spec, netloc.first, netloc.second);
          }
        }
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
      if (replay_session) {
        // If in a replay session, run the event loop for a bit longer to make
        // sure the server doesn't send anything unexpected after the end of
        // the session.
        auto tv = usecs_to_timeval(500000);
        event_base_loopexit(base.get(), &tv);
        event_base_dispatch(base.get());
      }

      config_log.info("Normal shutdown");
      state->proxy_server.reset(); // Break reference cycle
    });

void print_usage() {
  fputs("\
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
\n",
      stderr);
}

int main(int argc, char** argv) {
  Arguments args(&argv[1], argc - 1);
  if (args.get<bool>("help")) {
    print_usage();
    return 0;
  }

  string action_name = args.get<string>(0, false);
  const Action* a;
  try {
    a = all_actions.at(action_name);
  } catch (const out_of_range&) {
    log_error("Unknown or invalid action; try --help");
    return 1;
  }
  a->run(args);
  return 0;
}
