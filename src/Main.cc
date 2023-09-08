#include <event2/event.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Math.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <set>
#include <thread>
#include <unordered_map>

#include "BMLArchive.hh"
#include "CatSession.hh"
#include "Compression.hh"
#include "DNSServer.hh"
#include "GSLArchive.hh"
#include "GVMEncoder.hh"
#include "IPStackSimulator.hh"
#include "Loggers.hh"
#include "NetworkAddresses.hh"
#include "PSOGCObjectGraph.hh"
#include "Product.hh"
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

using namespace std;

bool use_terminal_colors = false;

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
the output to INPUT-FILENAME.dec or a similarly-named file; if OUTPUT-FILENAME\n\
is '-', newserv writes the output to stdout. If stdout is a terminal and the\n\
output is not text, the data written to stdout is formatted in a hex/ASCII\n\
view; in any other case, the raw output is written to stdout, which (for most\n\
actions) may include arbitrary binary data.\n\
\n\
The actions are:\n\
  help\n\
    You\'re reading it now.\n\
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
    the --optimal option.\n\
  decompress-prs [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-pr2 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decompress-bc0 [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Decompress data compressed using the PRS, PR2, or BC0 algorithms.\n\
  recompress-prs-optimal [INPUT-AND-OUTPUT-FILENAME]\n\
    Recompress the input PRS file optimally, overwriting it with the optimally-\n\
    compressed result.\n\
  prs-size [INPUT-FILENAME]\n\
    Compute the decompressed size of the PRS-compressed input data, but don\'t\n\
    write the decompressed data anywhere.\n\
  disassemble-prs [INPUT-FILENAME]\n\
  disassemble-bc0 [INPUT-FILENAME]\n\
    Write a textual representation of the commands contained in a PRS or BC0\n\
    command stream. The output is written to stdout. This is mainly useful for\n\
    debugging the compressors and decompressors themselves.\n\
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
  encrypt-trivial-data --seed=BASIS [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-trivial-data [--seed=BASIS] [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt data using the Episode 3 trivial algorithm. When\n\
    encrypting, --seed=BASIS is required; BASIS should be a single byte\n\
    specified in hexadecimal. When decrypting, BASIS should be specified the\n\
    same way, but if it is not given, newserv will try all possible basis\n\
    values and return the one that results in the greatest number of zero bytes\n\
    in the output.\n\
  encrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
  decrypt-challenge-data [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Encrypt or decrypt data using the challenge mode trivial algorithm.\n\
  encrypt-gci-save CRYPT-OPTION INPUT-FILENAME [OUTPUT-FILENAME]\n\
  decrypt-gci-save CRYPT-OPTION INPUT-FILENAME [OUTPUT-FILENAME]\n\
    Encrypt or decrypt a character or Guild Card file in GCI format. If\n\
    encrypting, the checksum is also recomputed and stored in the encrypted\n\
    file. CRYPT-OPTION is required; it can be either --sys=SYSTEM-FILENAME\n\
    (specifying the name of the corresponding PSO_SYSTEM .gci file) or\n\
    --seed=ROUND1-SEED (specified as a 32-bit hexadecimal number).\n\
  salvage-gci INPUT-FILENAME [--round2] [CRYPT-OPTION] [--bytes=SIZE]\n\
    Attempt to find either the round-1 or round-2 decryption seed for a\n\
    corrupted GCI file. If --round2 is given, then CRYPT-OPTION must be given\n\
    (and should specify either a valid system file or the round1 seed).\n\
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
  encode-qst INPUT-FILENAME [OUTPUT-FILENAME] [OPTIONS...]\n\
    Encode the input quest file (in .bin/.dat format) into a .qst file. If\n\
    --download is given, generates a download .qst instead of an online .qst.\n\
    Specify the quest\'s game version with one of the --dc-nte, --dc-v1,\n\
    --dc-v2, --pc, --gc-nte, --gc, --gc-ep3, --xb, or --bb options.\n\
  disassemble-quest-script [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Disassemble the input quest script (.bin file) into a text representation\n\
    of the commands and metadata it contains. Specify the quest\'s game version\n\
    with one of the --dc-nte, --dc-v1, --dc-v2, --pc, --gc-nte, --gc, --gc-ep3,\n\
    --xb, or --bb options.\n\
  cat-client ADDR:PORT\n\
    Connect to the given server and simulate a PSO client. newserv will then\n\
    print all the received commands to stdout, and forward any commands typed\n\
    into stdin to the remote server. It is assumed that the input and output\n\
    are terminals, so all commands are hex-encoded. The --patch, --dc, --pc,\n\
    --gc, and --bb options can be used to select the command format and\n\
    encryption. If --bb is used, the --key=KEY-NAME option is also required (as\n\
    in decrypt-data above).\n\
  show-ep3-maps\n\
    Print the Episode 3 maps from the system/ep3 directory in a (sort of)\n\
    human-readable format.\n\
  show-ep3-cards\n\
    Print the Episode 3 card definitions from the system/ep3 directory in a\n\
    human-readable format.\n\
  describe-item DATA\n\
    Print the name of the item given by DATA (in hex). DATA must not contain\n\
    spaces. If DATA is 20 bytes, newserv assumes it contains an unused item ID\n\
    field; if it is fewer bytes, up to 16 bytes are used.\n\
  encode-item DESCRIPTION\n\
    Encode the description of an item into its corresponding ItemData (hex)\n\
    representation. If DESCRIPTION contains spaces, it must be quoted, such as\n\
    \"L&K14 COMBAT +10 0/10/15/0/35\".\n\
  replay-log [INPUT-FILENAME] [OPTIONS...]\n\
    Replay a terminal log as if it were a client session. input-filename may be\n\
    specified for this option. This is used for regression testing, to make\n\
    sure client sessions are repeatable and code changes don\'t affect existing\n\
    (working) functionality.\n\
  extract-gsl [INPUT-FILENAME] [--big-endian]\n\
  extract-bml [INPUT-FILENAME] [--big-endian]\n\
    Extract all files from a GSL or BML archive into the current directory.\n\
    input-filename may be specified. If output-filename is specified, then it\n\
    is treated as a prefix which is prepended to the filename of each file\n\
    contained in the archive. If --big-endian is given, the archive header is\n\
    read in GameCube format; otherwise it is read in PC/BB format.\n\
  format-rare-item-set [--json] [INPUT-FILENAME]\n\
    Print the contents of a rare item table in a human-readable format. If\n\
    --json is given, the input is parsed as a JSON rare item set (see\n\
    system/blueburst/rare-table.json for an example of this format). If --json\n\
    is not given, the input is parsed as a REL rare item set.\n\
  convert-itemrt-rel-to-json [INPUT-FILENAME [OUTPUT-FILENAME]]\n\
    Convert a REL rare table to a JSON rare item set. The resulting JSON has\n\
    the same structure as system/blueburst/rare-table.json.\n\
\n\
A few options apply to multiple modes described above:\n\
  --parse-data\n\
      For modes that take input (from a file or from stdin), parse the input as\n\
      a hex string before encrypting/decoding/etc.\n\
  --config=FILENAME\n\
      Use this file instead of system/config.json.\n\
",
      stderr);
}

enum class Behavior {
  RUN_SERVER = 0,
  COMPRESS_PRS,
  DECOMPRESS_PRS,
  RECOMPRESS_PRS_OPTIMAL,
  COMPRESS_PR2,
  DECOMPRESS_PR2,
  COMPRESS_BC0,
  DECOMPRESS_BC0,
  PRS_SIZE,
  DISASSEMBLE_PRS,
  DISASSEMBLE_BC0,
  ENCRYPT_DATA,
  DECRYPT_DATA,
  ENCRYPT_TRIVIAL_DATA,
  DECRYPT_TRIVIAL_DATA,
  ENCRYPT_CHALLENGE_DATA,
  DECRYPT_CHALLENGE_DATA,
  ENCRYPT_GCI_SAVE,
  DECRYPT_GCI_SAVE,
  DECODE_GCI_SNAPSHOT,
  ENCODE_GVM,
  FIND_DECRYPTION_SEED,
  SALVAGE_GCI,
  DECODE_QUEST_FILE,
  ENCODE_QST,
  DISASSEMBLE_QUEST_SCRIPT,
  DECODE_SJIS,
  EXTRACT_GSL,
  EXTRACT_BML,
  FORMAT_RARE_ITEM_SET,
  CONVERT_ITEMRT_REL_TO_JSON,
  SHOW_EP3_MAPS,
  SHOW_EP3_CARDS,
  DESCRIBE_ITEM,
  ENCODE_ITEM,
  PARSE_OBJECT_GRAPH,
  REPLAY_LOG,
  CAT_CLIENT,
  GENERATE_PRODUCT,
  GENERATE_ALL_PRODUCTS,
  INSPECT_PRODUCT,
  PRODUCT_SPEED_TEST,
};

static bool behavior_takes_input_filename(Behavior b) {
  return (b == Behavior::COMPRESS_PRS) ||
      (b == Behavior::DECOMPRESS_PRS) ||
      (b == Behavior::RECOMPRESS_PRS_OPTIMAL) ||
      (b == Behavior::COMPRESS_PR2) ||
      (b == Behavior::DECOMPRESS_PR2) ||
      (b == Behavior::COMPRESS_BC0) ||
      (b == Behavior::DECOMPRESS_BC0) ||
      (b == Behavior::PRS_SIZE) ||
      (b == Behavior::DISASSEMBLE_PRS) ||
      (b == Behavior::DISASSEMBLE_BC0) ||
      (b == Behavior::ENCRYPT_DATA) ||
      (b == Behavior::DECRYPT_DATA) ||
      (b == Behavior::ENCRYPT_TRIVIAL_DATA) ||
      (b == Behavior::DECRYPT_TRIVIAL_DATA) ||
      (b == Behavior::ENCRYPT_CHALLENGE_DATA) ||
      (b == Behavior::DECRYPT_CHALLENGE_DATA) ||
      (b == Behavior::DECRYPT_GCI_SAVE) ||
      (b == Behavior::DECODE_GCI_SNAPSHOT) ||
      (b == Behavior::ENCODE_GVM) ||
      (b == Behavior::SALVAGE_GCI) ||
      (b == Behavior::ENCRYPT_GCI_SAVE) ||
      (b == Behavior::DECODE_QUEST_FILE) ||
      (b == Behavior::ENCODE_QST) ||
      (b == Behavior::DISASSEMBLE_QUEST_SCRIPT) ||
      (b == Behavior::DECODE_SJIS) ||
      (b == Behavior::FORMAT_RARE_ITEM_SET) ||
      (b == Behavior::CONVERT_ITEMRT_REL_TO_JSON) ||
      (b == Behavior::EXTRACT_GSL) ||
      (b == Behavior::EXTRACT_BML) ||
      (b == Behavior::DESCRIBE_ITEM) ||
      (b == Behavior::ENCODE_ITEM) ||
      (b == Behavior::PARSE_OBJECT_GRAPH) ||
      (b == Behavior::REPLAY_LOG) ||
      (b == Behavior::CAT_CLIENT) ||
      (b == Behavior::INSPECT_PRODUCT);
}

static bool behavior_takes_output_filename(Behavior b) {
  return (b == Behavior::COMPRESS_PRS) ||
      (b == Behavior::DECOMPRESS_PRS) ||
      (b == Behavior::COMPRESS_PR2) ||
      (b == Behavior::DECOMPRESS_PR2) ||
      (b == Behavior::COMPRESS_BC0) ||
      (b == Behavior::DECOMPRESS_BC0) ||
      (b == Behavior::ENCRYPT_DATA) ||
      (b == Behavior::DECRYPT_DATA) ||
      (b == Behavior::ENCRYPT_TRIVIAL_DATA) ||
      (b == Behavior::DECRYPT_TRIVIAL_DATA) ||
      (b == Behavior::ENCRYPT_CHALLENGE_DATA) ||
      (b == Behavior::DECRYPT_CHALLENGE_DATA) ||
      (b == Behavior::DECRYPT_GCI_SAVE) ||
      (b == Behavior::ENCRYPT_GCI_SAVE) ||
      (b == Behavior::DECODE_GCI_SNAPSHOT) ||
      (b == Behavior::ENCODE_GVM) ||
      (b == Behavior::ENCODE_QST) ||
      (b == Behavior::DISASSEMBLE_QUEST_SCRIPT) ||
      (b == Behavior::CONVERT_ITEMRT_REL_TO_JSON) ||
      (b == Behavior::DECODE_SJIS) ||
      (b == Behavior::EXTRACT_GSL) ||
      (b == Behavior::EXTRACT_BML);
}

int main(int argc, char** argv) {
  Behavior behavior = Behavior::RUN_SERVER;
  GameVersion cli_version = GameVersion::GC;
  QuestScriptVersion cli_quest_version = QuestScriptVersion::GC_V3;
  Quest::FileFormat quest_file_type = Quest::FileFormat::BIN_DAT_GCI;
  string seed;
  string key_file_name;
  const char* config_filename = "system/config.json";
  bool parse_data = false;
  bool big_endian = false;
  bool skip_little_endian = false;
  bool skip_big_endian = false;
  bool round2 = false;
  bool skip_checksum = false;
  uint64_t override_round2_seed = 0xFFFFFFFFFFFFFFFF;
  size_t offset = 0;
  size_t stride = 1;
  size_t num_threads = 0;
  size_t bytes = 0;
  ssize_t compression_level = 0;
  bool expect_decompressed = false;
  bool compress_optimal = false;
  bool json = false;
  bool download = false;
  bool one_line = false;
  const char* find_decryption_seed_ciphertext = nullptr;
  vector<const char*> find_decryption_seed_plaintexts;
  const char* input_filename = nullptr;
  const char* output_filename = nullptr;
  const char* system_filename = nullptr;
  const char* replay_required_access_key = "";
  const char* replay_required_password = "";
  uint32_t root_object_address = 0;
  uint8_t domain = 1;
  uint8_t subdomain = 0xFF;
  for (int x = 1; x < argc; x++) {
    if (!strcmp(argv[x], "--help")) {
      print_usage();
      return 0;
    } else if (!strncmp(argv[x], "--threads=", 10)) {
      num_threads = strtoull(&argv[x][10], nullptr, 0);
    } else if (!strcmp(argv[x], "--one-line")) {
      one_line = true;
    } else if (!strcmp(argv[x], "--download")) {
      download = true;
    } else if (!strcmp(argv[x], "--patch")) {
      cli_version = GameVersion::PATCH;
      cli_quest_version = QuestScriptVersion::PC_V2;
    } else if (!strcmp(argv[x], "--dc-nte")) {
      cli_version = GameVersion::DC;
      cli_quest_version = QuestScriptVersion::DC_NTE;
    } else if (!strcmp(argv[x], "--dc-v1")) {
      cli_version = GameVersion::DC;
      cli_quest_version = QuestScriptVersion::DC_V1;
    } else if (!strcmp(argv[x], "--dc-v2") || !strcmp(argv[x], "--dc")) {
      cli_version = GameVersion::DC;
      cli_quest_version = QuestScriptVersion::DC_V2;
    } else if (!strcmp(argv[x], "--pc")) {
      cli_version = GameVersion::PC;
      cli_quest_version = QuestScriptVersion::PC_V2;
    } else if (!strcmp(argv[x], "--gc")) {
      cli_version = GameVersion::GC;
      cli_quest_version = QuestScriptVersion::GC_V3;
    } else if (!strcmp(argv[x], "--gc-nte")) {
      cli_version = GameVersion::GC;
      cli_quest_version = QuestScriptVersion::GC_NTE;
    } else if (!strcmp(argv[x], "--gc-ep3")) {
      cli_version = GameVersion::GC;
      cli_quest_version = QuestScriptVersion::GC_EP3;
    } else if (!strcmp(argv[x], "--xb")) {
      cli_version = GameVersion::XB;
      cli_quest_version = QuestScriptVersion::XB_V3;
    } else if (!strcmp(argv[x], "--bb")) {
      cli_version = GameVersion::BB;
      cli_quest_version = QuestScriptVersion::BB_V4;
    } else if (!strncmp(argv[x], "--compression-level=", 20)) {
      compression_level = strtoll(&argv[x][20], nullptr, 0);
    } else if (!strcmp(argv[x], "--optimal")) {
      compress_optimal = true;
    } else if (!strcmp(argv[x], "--decompressed")) {
      expect_decompressed = true;
    } else if (!strcmp(argv[x], "--round2")) {
      round2 = true;
    } else if (!strncmp(argv[x], "--bytes=", 8)) {
      bytes = strtoull(&argv[x][8], nullptr, 0);
    } else if (!strncmp(argv[x], "--offset=", 9)) {
      offset = strtoull(&argv[x][9], nullptr, 0);
    } else if (!strncmp(argv[x], "--stride=", 9)) {
      stride = strtoull(&argv[x][9], nullptr, 0);
    } else if (!strcmp(argv[x], "--skip-checksum")) {
      skip_checksum = true;
    } else if (!strncmp(argv[x], "--seed=", 7)) {
      seed = &argv[x][7];
    } else if (!strncmp(argv[x], "--round2-seed=", 14)) {
      override_round2_seed = strtoull(&argv[x][14], nullptr, 16);
    } else if (!strncmp(argv[x], "--key=", 6)) {
      key_file_name = &argv[x][6];
    } else if (!strncmp(argv[x], "--sys=", 6)) {
      system_filename = &argv[x][6];
    } else if (!strncmp(argv[x], "--domain=", 9)) {
      domain = atoi(&argv[x][9]);
    } else if (!strncmp(argv[x], "--subdomain=", 12)) {
      subdomain = atoi(&argv[x][12]);
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
    } else if (!strcmp(argv[x], "--json")) {
      json = true;
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
        }
        if (!strcmp(argv[x], "compress-prs")) {
          behavior = Behavior::COMPRESS_PRS;
        } else if (!strcmp(argv[x], "decompress-prs")) {
          behavior = Behavior::DECOMPRESS_PRS;
        } else if (!strcmp(argv[x], "recompress-prs-optimal")) {
          behavior = Behavior::RECOMPRESS_PRS_OPTIMAL;
        } else if (!strcmp(argv[x], "compress-pr2")) {
          behavior = Behavior::COMPRESS_PR2;
        } else if (!strcmp(argv[x], "decompress-pr2")) {
          behavior = Behavior::DECOMPRESS_PR2;
        } else if (!strcmp(argv[x], "compress-bc0")) {
          behavior = Behavior::COMPRESS_BC0;
        } else if (!strcmp(argv[x], "decompress-bc0")) {
          behavior = Behavior::DECOMPRESS_BC0;
        } else if (!strcmp(argv[x], "prs-size")) {
          behavior = Behavior::PRS_SIZE;
        } else if (!strcmp(argv[x], "disassemble-prs")) {
          behavior = Behavior::DISASSEMBLE_PRS;
        } else if (!strcmp(argv[x], "disassemble-bc0")) {
          behavior = Behavior::DISASSEMBLE_BC0;
        } else if (!strcmp(argv[x], "encrypt-data")) {
          behavior = Behavior::ENCRYPT_DATA;
        } else if (!strcmp(argv[x], "decrypt-data")) {
          behavior = Behavior::DECRYPT_DATA;
        } else if (!strcmp(argv[x], "encrypt-trivial-data")) {
          behavior = Behavior::ENCRYPT_TRIVIAL_DATA;
        } else if (!strcmp(argv[x], "decrypt-trivial-data")) {
          behavior = Behavior::DECRYPT_TRIVIAL_DATA;
        } else if (!strcmp(argv[x], "encrypt-challenge-data")) {
          behavior = Behavior::ENCRYPT_CHALLENGE_DATA;
        } else if (!strcmp(argv[x], "decrypt-challenge-data")) {
          behavior = Behavior::DECRYPT_CHALLENGE_DATA;
        } else if (!strcmp(argv[x], "decrypt-gci-save")) {
          behavior = Behavior::DECRYPT_GCI_SAVE;
        } else if (!strcmp(argv[x], "encrypt-gci-save")) {
          behavior = Behavior::ENCRYPT_GCI_SAVE;
        } else if (!strcmp(argv[x], "decode-gci-snapshot")) {
          behavior = Behavior::DECODE_GCI_SNAPSHOT;
        } else if (!strcmp(argv[x], "encode-gvm")) {
          behavior = Behavior::ENCODE_GVM;
        } else if (!strcmp(argv[x], "find-decryption-seed")) {
          behavior = Behavior::FIND_DECRYPTION_SEED;
        } else if (!strcmp(argv[x], "salvage-gci")) {
          behavior = Behavior::SALVAGE_GCI;
        } else if (!strcmp(argv[x], "decode-sjis")) {
          behavior = Behavior::DECODE_SJIS;
        } else if (!strcmp(argv[x], "decode-gci")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = Quest::FileFormat::BIN_DAT_GCI;
        } else if (!strcmp(argv[x], "decode-vms")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = Quest::FileFormat::BIN_DAT_VMS;
        } else if (!strcmp(argv[x], "decode-dlq")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = Quest::FileFormat::BIN_DAT_DLQ;
        } else if (!strcmp(argv[x], "decode-qst")) {
          behavior = Behavior::DECODE_QUEST_FILE;
          quest_file_type = Quest::FileFormat::QST;
        } else if (!strcmp(argv[x], "encode-qst")) {
          behavior = Behavior::ENCODE_QST;
        } else if (!strcmp(argv[x], "disassemble-quest-script")) {
          behavior = Behavior::DISASSEMBLE_QUEST_SCRIPT;
        } else if (!strcmp(argv[x], "cat-client")) {
          behavior = Behavior::CAT_CLIENT;
        } else if (!strcmp(argv[x], "format-rare-item-set")) {
          behavior = Behavior::FORMAT_RARE_ITEM_SET;
        } else if (!strcmp(argv[x], "convert-itemrt-rel-to-json")) {
          behavior = Behavior::CONVERT_ITEMRT_REL_TO_JSON;
        } else if (!strcmp(argv[x], "show-ep3-maps")) {
          behavior = Behavior::SHOW_EP3_MAPS;
        } else if (!strcmp(argv[x], "show-ep3-cards")) {
          behavior = Behavior::SHOW_EP3_CARDS;
        } else if (!strcmp(argv[x], "describe-item")) {
          behavior = Behavior::DESCRIBE_ITEM;
        } else if (!strcmp(argv[x], "encode-item")) {
          behavior = Behavior::ENCODE_ITEM;
        } else if (!strcmp(argv[x], "parse-object-graph")) {
          behavior = Behavior::PARSE_OBJECT_GRAPH;
        } else if (!strcmp(argv[x], "replay-log")) {
          behavior = Behavior::REPLAY_LOG;
        } else if (!strcmp(argv[x], "extract-gsl")) {
          behavior = Behavior::EXTRACT_GSL;
        } else if (!strcmp(argv[x], "extract-bml")) {
          behavior = Behavior::EXTRACT_BML;
        } else if (!strcmp(argv[x], "generate-product")) {
          behavior = Behavior::GENERATE_PRODUCT;
        } else if (!strcmp(argv[x], "generate-all-products")) {
          behavior = Behavior::GENERATE_ALL_PRODUCTS;
        } else if (!strcmp(argv[x], "inspect-product")) {
          behavior = Behavior::INSPECT_PRODUCT;
        } else if (!strcmp(argv[x], "product-speed-test")) {
          behavior = Behavior::PRODUCT_SPEED_TEST;
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
    if (output_filename && strcmp(output_filename, "-")) {
      // If the output is to a specified file, write it there
      save_file(output_filename, data, size);

    } else if (!output_filename && input_filename && strcmp(input_filename, "-")) {
      // If no output filename is given and an input filename is given, write to
      // <input-filename>.dec (or an appropriate extension, if it can be
      // autodetected)
      string filename = input_filename;
      if (behavior == Behavior::COMPRESS_PRS) {
        if (ends_with(filename, ".bind") ||
            ends_with(filename, ".datd") ||
            ends_with(filename, ".mnmd")) {
          filename.resize(filename.size() - 1);
        } else {
          filename += ".prs";
        }
      } else if (behavior == Behavior::DECOMPRESS_PRS) {
        if (ends_with(filename, ".bin") ||
            ends_with(filename, ".dat") ||
            ends_with(filename, ".mnm")) {
          filename += "d";
        } else {
          filename += ".dec";
        }
      } else if (behavior == Behavior::ENCRYPT_GCI_SAVE) {
        if (ends_with(filename, ".gcid")) {
          filename.resize(filename.size() - 1);
        } else {
          filename += ".gci";
        }
      } else if (behavior == Behavior::DECRYPT_GCI_SAVE) {
        if (ends_with(filename, ".gci")) {
          filename += "d";
        } else {
          filename += ".dec";
        }
      } else if (behavior == Behavior::DECODE_GCI_SNAPSHOT) {
        filename += ".bmp";
      } else if (behavior == Behavior::ENCODE_GVM) {
        filename += ".gvm";
      } else if (behavior == Behavior::DISASSEMBLE_QUEST_SCRIPT) {
        filename += ".txt";
      } else if (behavior == Behavior::CONVERT_ITEMRT_REL_TO_JSON) {
        filename += ".json";
      } else {
        filename += ".dec";
      }
      save_file(filename, data, size);

    } else if (isatty(fileno(stdout)) &&
        (behavior != Behavior::DISASSEMBLE_QUEST_SCRIPT) &&
        (behavior != Behavior::CONVERT_ITEMRT_REL_TO_JSON)) {
      // If stdout is a terminal and the data is not known to be text, use
      // print_data to write the result
      print_data(stdout, data, size);
      fflush(stdout);

    } else {
      // If stdout is not a terminal, write the data as-is
      fwritex(stdout, data, size);
      fflush(stdout);
    }
  };

  switch (behavior) {
    case Behavior::COMPRESS_PRS:
    case Behavior::DECOMPRESS_PRS:
    case Behavior::COMPRESS_PR2:
    case Behavior::DECOMPRESS_PR2:
    case Behavior::COMPRESS_BC0:
    case Behavior::DECOMPRESS_BC0: {
      string data = read_input_data();

      size_t pr2_expected_size = 0;
      if (behavior == Behavior::DECOMPRESS_PR2) {
        if (data.size() < 8) {
          throw runtime_error("not enough data for PR2 header");
        }
        data.resize((data.size() + 3) & (~3));
        StringReader r(data);
        pr2_expected_size = big_endian ? r.get_u32b() : r.get_u32l();
        PSOV2Encryption crypt(big_endian ? r.get_u32b() : r.get_u32l());
        if (big_endian) {
          crypt.encrypt_big_endian(data.data() + 8, data.size() - 8);
        } else {
          crypt.decrypt(data.data() + 8, data.size() - 8);
        }
        data = data.substr(8);
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
      if ((behavior == Behavior::COMPRESS_PRS) || (behavior == Behavior::COMPRESS_PR2)) {
        if (compress_optimal) {
          data = prs_compress_optimal(data.data(), data.size(), optimal_progress_fn);
        } else {
          data = prs_compress(data, compression_level, progress_fn);
        }
      } else if ((behavior == Behavior::DECOMPRESS_PRS) || (behavior == Behavior::DECOMPRESS_PR2)) {
        data = prs_decompress(data);
      } else if (behavior == Behavior::COMPRESS_BC0) {
        if (compress_optimal) {
          data = bc0_compress_optimal(data.data(), data.size(), optimal_progress_fn);
        } else if (compression_level < 0) {
          data = bc0_encode(data.data(), data.size());
        } else {
          data = bc0_compress(data, progress_fn);
        }
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

      if ((behavior == Behavior::DECOMPRESS_PR2) && (data.size() != pr2_expected_size)) {
        log_warning("Result data size (%zu bytes) does not match expected size from PR2 header (%zu bytes)", data.size(), pr2_expected_size);
      } else if (behavior == Behavior::COMPRESS_PR2) {
        uint32_t pr2_seed = seed.empty() ? random_object<uint32_t>() : stoul(seed, nullptr, 16);
        size_t orig_size = data.size();
        data.resize((data.size() + 3) & (~3));
        PSOV2Encryption crypt(pr2_seed);
        if (big_endian) {
          crypt.encrypt_big_endian(data.data(), data.size());
        } else {
          crypt.encrypt(data.data(), data.size());
        }
        data.resize(orig_size);
        StringWriter w;
        if (big_endian) {
          w.put_u32b(input_bytes);
          w.put_u32b(pr2_seed);
        } else {
          w.put_u32l(input_bytes);
          w.put_u32l(pr2_seed);
        }
        w.write(data);
        data = std::move(w.str());
      }

      write_output_data(data.data(), data.size());
      break;
    }

    case Behavior::RECOMPRESS_PRS_OPTIMAL: {
      string input_data = read_input_data();
      string decompressed_data = prs_decompress(input_data);

      auto progress_fn = [&](auto phase, size_t input_progress, size_t input_bytes, size_t output_progress) -> void {
        const char* phase_name = name_for_enum(phase);
        float progress = static_cast<float>(input_progress * 100) / decompressed_data.size();
        float size_ratio = static_cast<float>(output_progress * 100) / input_progress;
        fprintf(stderr, "... [%s] %zu/%zu (%g%%) => %zu (%g%%)    \r",
            phase_name, input_progress, input_bytes, progress, output_progress, size_ratio);
      };

      uint64_t start = now();
      string output_data = prs_compress_optimal(decompressed_data.data(), decompressed_data.size(), progress_fn);
      uint64_t end = now();
      string time_str = format_duration(end - start);

      float output_size_ratio = static_cast<float>(output_data.size() * 100) / decompressed_data.size();
      float input_size_ratio = static_cast<float>(input_data.size() * 100) / decompressed_data.size();
      ssize_t size_difference = output_data.size() - input_data.size();
      log_info("%zu (0x%zX) bytes input (%g%%) => %zu (0x%zX) bytes decompressed => %zu (0x%zX) bytes output (%g%%; %+zd bytes)",
          input_data.size(), input_data.size(), input_size_ratio, decompressed_data.size(), decompressed_data.size(), output_data.size(), output_data.size(), output_size_ratio, size_difference);

      output_filename = input_filename;
      write_output_data(output_data.data(), output_data.size());
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

    case Behavior::DISASSEMBLE_PRS: {
      prs_disassemble(stdout, read_input_data());
      break;
    }
    case Behavior::DISASSEMBLE_BC0: {
      bc0_disassemble(stdout, read_input_data());
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

    case Behavior::ENCRYPT_TRIVIAL_DATA:
    case Behavior::DECRYPT_TRIVIAL_DATA: {
      if (seed.empty() && behavior == Behavior::ENCRYPT_TRIVIAL_DATA) {
        throw logic_error("--seed is required when encrypting data");
      }
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
        fprintf(stderr, "Basis appears to be %02hhX (%zu zero bytes in output)\n",
            best_seed, best_seed_score);
        basis = best_seed;
      } else {
        basis = stoul(seed, nullptr, 16);
      }
      decrypt_trivial_gci_data(data.data(), data.size(), basis);
      write_output_data(data.data(), data.size());
      break;
    }

    case Behavior::ENCRYPT_CHALLENGE_DATA:
    case Behavior::DECRYPT_CHALLENGE_DATA: {
      string data = read_input_data();
      string result = (behavior == Behavior::DECRYPT_CHALLENGE_DATA)
          ? decrypt_challenge_rank_text(data)
          : encrypt_challenge_rank_text(data);
      write_output_data(result.data(), result.size());
      break;
    }

    case Behavior::ENCRYPT_GCI_SAVE:
    case Behavior::DECRYPT_GCI_SAVE: {
      uint32_t round1_seed;
      if (system_filename) {
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

      bool is_decrypt = (behavior == Behavior::DECRYPT_GCI_SAVE);

      auto data = read_input_data();
      StringReader r(data);
      const auto& header = r.get<PSOGCIFileHeader>();
      header.check();

      size_t data_start_offset = r.where();

      auto process_file = [&]<typename StructT>() {
        if (is_decrypt) {
          const void* data_section = r.getv(header.data_size);
          auto decrypted = decrypt_gci_fixed_size_file_data_section<StructT>(
              data_section, header.data_size, round1_seed, skip_checksum, override_round2_seed);
          *reinterpret_cast<StructT*>(data.data() + data_start_offset) = decrypted;
        } else {
          const auto& s = r.get<StructT>();
          auto encrypted = encrypt_gci_fixed_size_file_data_section<StructT>(
              s, round1_seed);
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

      write_output_data(data.data(), data.size());

      break;
    }

    case Behavior::DECODE_GCI_SNAPSHOT: {
      auto data = read_input_data();
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
      write_output_data(saved.data(), saved.size());
      break;
    }

    case Behavior::ENCODE_GVM: {
      Image img;
      if (input_filename && strcmp(input_filename, "-")) {
        img = Image(input_filename);
      } else {
        img = Image(stdin);
      }
      string encoded = encode_gvm(img, img.get_has_alpha() ? GVRDataFormat::RGB5A3 : GVRDataFormat::RGB565);
      write_output_data(encoded.data(), encoded.size());
      break;
    }

    case Behavior::SALVAGE_GCI: {
      uint64_t likely_round1_seed = 0xFFFFFFFFFFFFFFFF;
      if (system_filename) {
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

      auto data = read_input_data();
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
                string decrypted = decrypt_gci_fixed_size_file_data_section_for_salvage(
                    data_section, header.data_size, likely_round1_seed, seed, bytes);
                zero_count = count_zeroes(
                    decrypted.data() + offset,
                    decrypted.size() - offset,
                    stride);
              } else {
                auto decrypted = decrypt_gci_fixed_size_file_data_section<StructT>(
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
        plaintexts.emplace_back(std::move(data), std::move(mask));
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
      },
          0, 0x100000000, num_threads);

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
      if (quest_file_type == Quest::FileFormat::BIN_DAT_GCI) {
        int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
        auto decoded = Quest::decode_gci_file(input_filename, num_threads, dec_seed);
        save_file(output_filename_base + ".dec", decoded);
      } else if (quest_file_type == Quest::FileFormat::BIN_DAT_VMS) {
        int64_t dec_seed = seed.empty() ? -1 : stoul(seed, nullptr, 16);
        auto decoded = Quest::decode_vms_file(input_filename, num_threads, dec_seed);
        save_file(output_filename_base + ".dec", decoded);
      } else if (quest_file_type == Quest::FileFormat::BIN_DAT_DLQ) {
        auto decoded = Quest::decode_dlq_file(input_filename);
        save_file(output_filename_base + ".dec", decoded);
      } else if (quest_file_type == Quest::FileFormat::QST) {
        auto data = Quest::decode_qst_file(input_filename);
        save_file(output_filename_base + ".bin", data.first);
        save_file(output_filename_base + ".dat", data.second);
      } else {
        throw logic_error("invalid quest file format");
      }
      break;
    }

    case Behavior::ENCODE_QST: {
      if (!input_filename || !strcmp(input_filename, "-")) {
        throw invalid_argument("an input filename is required");
      }

      shared_ptr<Quest> q(new Quest(input_filename, cli_quest_version, nullptr));
      if (download) {
        q = q->create_download_quest();
      }
      string qst_data = q->encode_qst();

      write_output_data(qst_data.data(), qst_data.size());
      break;
    }

    case Behavior::DISASSEMBLE_QUEST_SCRIPT: {
      if (!input_filename || !strcmp(input_filename, "-")) {
        throw invalid_argument("an input filename is required");
      }

      string data = read_input_data();
      if (!expect_decompressed) {
        data = prs_decompress(data);
      }
      string result = disassemble_quest_script(data.data(), data.size(), cli_quest_version);
      write_output_data(result.data(), result.size());
      break;
    }

    case Behavior::DECODE_SJIS: {
      string data = read_input_data();
      auto decoded = decode_sjis(data);
      write_output_data(decoded.data(), decoded.size() * sizeof(decoded[0]));
      break;
    }

    case Behavior::EXTRACT_GSL:
    case Behavior::EXTRACT_BML: {
      string output_prefix;
      if (!output_filename) {
        output_prefix = input_filename;
        output_prefix.push_back('_');
      } else if (!strcmp(output_filename, "-")) {
        throw invalid_argument("output prefix cannot be stdout");
      }

      string data = read_input_data();
      shared_ptr<string> data_shared(new string(std::move(data)));

      if (behavior == Behavior::EXTRACT_GSL) {
        GSLArchive arch(data_shared, big_endian);
        for (const auto& entry_it : arch.all_entries()) {
          auto e = arch.get(entry_it.first);
          string out_file = output_prefix + entry_it.first;
          save_file(out_file.c_str(), e.first, e.second);
          fprintf(stderr, "... %s\n", out_file.c_str());
        }
      } else {
        BMLArchive arch(data_shared, big_endian);
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

    case Behavior::FORMAT_RARE_ITEM_SET: {
      shared_ptr<string> data(new string(read_input_data()));
      shared_ptr<RareItemSet> rs;
      if (json) {
        rs.reset(new JSONRareItemSet(JSON::parse(read_input_data())));
      } else {
        rs.reset(new RELRareItemSet(data));
      }

      auto format_drop = +[](const RareItemSet::ExpandedDrop& r) -> string {
        ItemData item;
        item.data1[0] = r.item_code[0];
        item.data1[1] = r.item_code[1];
        item.data1[2] = r.item_code[2];
        string name = item.name(false);

        auto frac = reduce_fraction<uint64_t>(r.probability, 0x100000000);
        return string_printf(
            "(%08" PRIX32 " => %" PRIu64 "/%" PRIu64 ") %02hhX%02hhX%02hhX (%s)",
            r.probability, frac.first, frac.second, r.item_code[0], r.item_code[1], r.item_code[2], name.c_str());
      };

      auto print_collection = [&](GameMode mode, Episode episode, uint8_t difficulty, uint8_t section_id) -> void {
        string secid_name = name_for_section_id(section_id);
        fprintf(stdout, "%s %s %s %s\n",
            name_for_mode(mode),
            name_for_episode(episode),
            name_for_difficulty(difficulty),
            secid_name.c_str());

        fprintf(stdout, "  Monster rares:\n");
        for (size_t z = 0; z < 0x65; z++) {
          string enemy_types;
          for (size_t w = 0; w < static_cast<size_t>(EnemyType::MAX_ENEMY_TYPE); w++) {
            auto enemy_type = static_cast<EnemyType>(w);
            try {
              if (rare_table_index_for_enemy_type(enemy_type) == z &&
                  enemy_type_valid_for_episode(episode, enemy_type)) {
                enemy_types += name_for_enum(enemy_type);
                enemy_types += ",";
              }
            } catch (const exception&) {
            }
          }
          if (!enemy_types.empty()) {
            enemy_types.resize(enemy_types.size() - 1);
          }
          for (const auto& spec : rs->get_enemy_specs(mode, episode, difficulty, section_id, z)) {
            string s = format_drop(spec);
            fprintf(stdout, "    %02zX: %s (%s)\n", z, s.c_str(), enemy_types.c_str());
          }
        }

        fprintf(stdout, "  Box rares:\n");
        for (size_t area = 0; area < 0x12; area++) {
          for (const auto& spec : rs->get_box_specs(mode, episode, difficulty, section_id, area)) {
            string s = format_drop(spec);
            fprintf(stdout, "    (area %02zX) %s\n", area, s.c_str());
          }
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

    case Behavior::CONVERT_ITEMRT_REL_TO_JSON: {
      shared_ptr<string> data(new string(read_input_data()));
      RELRareItemSet rs(data);

      // Compute the mapping of {rt_index: EnemyType} for each episode
      const auto& generate_table = +[](Episode episode) -> vector<vector<EnemyType>> {
        vector<vector<EnemyType>> ret;
        for (size_t z = 0; z < static_cast<size_t>(EnemyType::MAX_ENEMY_TYPE); z++) {
          EnemyType t = static_cast<EnemyType>(z);
          try {
            uint8_t rt_index = rare_table_index_for_enemy_type(t);
            if (enemy_type_valid_for_episode(episode, t)) {
              if (rt_index >= ret.size()) {
                ret.resize(rt_index + 1);
              }
              ret[rt_index].emplace_back(t);
            }
          } catch (const exception&) {
          }
        }
        return ret;
      };

      auto episodes_dict = JSON::dict();
      static const array<pair<Episode, vector<vector<EnemyType>>>, 3> episodes = {
          make_pair(Episode::EP1, generate_table(Episode::EP1)),
          make_pair(Episode::EP2, generate_table(Episode::EP2)),
          make_pair(Episode::EP4, generate_table(Episode::EP4)),
      };
      for (const auto& episode_it : episodes) {
        Episode episode = episode_it.first;
        const auto& rt_index_to_enemy_type = episode_it.second;
        auto difficulty_dict = JSON::dict();
        for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
          auto section_id_dict = JSON::dict();
          for (uint8_t section_id = 0; section_id < 10; section_id++) {
            auto collection_dict = JSON::dict();

            for (size_t rt_index = 0; rt_index < rt_index_to_enemy_type.size(); rt_index++) {
              const auto& enemy_types = rt_index_to_enemy_type[rt_index];
              if (enemy_types.empty()) {
                continue;
              }

              for (const auto& spec : rs.get_enemy_specs(GameMode::NORMAL, episode, difficulty, section_id, rt_index)) {
                uint32_t primary_identifier = (spec.item_code[0] << 16) | (spec.item_code[1] << 8) | spec.item_code[2];
                if (primary_identifier == 0) {
                  continue;
                }

                auto frac = reduce_fraction<uint64_t>(spec.probability, 0x100000000);
                auto specs_json = JSON::list({JSON::list({string_printf("%" PRIu64 "/%" PRIu64, frac.first, frac.second), primary_identifier})});
                for (const auto& enemy_type : enemy_types) {
                  if (enemy_type_valid_for_episode(episode, enemy_type)) {
                    collection_dict.emplace(name_for_enum(enemy_type), std::move(specs_json));
                  }
                }
              }
            }

            for (size_t area = 0; area < 0x12; area++) {
              auto area_list = JSON::list();

              for (const auto& spec : rs.get_box_specs(GameMode::NORMAL, episode, difficulty, section_id, area)) {
                uint32_t primary_identifier = (spec.item_code[0] << 16) | (spec.item_code[1] << 8) | spec.item_code[2];
                if (primary_identifier == 0) {
                  continue;
                }
                auto frac = reduce_fraction<uint64_t>(spec.probability, 0x100000000);
                area_list.emplace_back(JSON::list({string_printf("%" PRIu64 "/%" PRIu64, frac.first, frac.second), primary_identifier}));
              }

              if (!area_list.empty()) {
                collection_dict.emplace(
                    string_printf("Box-%s", name_for_area(episode, area)),
                    std::move(area_list));
              }
            }

            if (!collection_dict.empty()) {
              section_id_dict.emplace(name_for_section_id(section_id), std::move(collection_dict));
            }
          }
          difficulty_dict.emplace(token_name_for_difficulty(difficulty), std::move(section_id_dict));
        }
        episodes_dict.emplace(token_name_for_episode(episode), std::move(difficulty_dict));
      }

      auto root_json = JSON::dict({{"Normal", std::move(episodes_dict)}});
      string json_data = root_json.serialize(
          JSON::SerializeOption::FORMAT |
          JSON::SerializeOption::HEX_INTEGERS |
          JSON::SerializeOption::SORT_DICT_KEYS);

      write_output_data(json_data.data(), json_data.size());
      break;
    }

    case Behavior::DESCRIBE_ITEM: {
      string data = parse_data_string(input_filename);

      ItemData item;
      if (data.size() == sizeof(ItemData)) {
        item = *reinterpret_cast<const ItemData*>(data.data());
      } else {
        memcpy(&item.data1[0], data.data(), min<size_t>(sizeof(item.data1), data.size()));
        if (data.size() > sizeof(item.data1)) {
          memcpy(&item.data2[0], data.data() + sizeof(item.data1), min<size_t>(sizeof(item.data2), data.size() - sizeof(item.data1)));
        }
      }

      string desc = item.name(false);
      log_info("Item: %s", desc.c_str());
      break;
    }

    case Behavior::ENCODE_ITEM: {
      ItemData item(input_filename, false);
      string desc = item.name(false);
      log_info("Data: %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX -------- %02hhX%02hhX%02hhX%02hhX",
          item.data1[0], item.data1[1], item.data1[2], item.data1[3],
          item.data1[4], item.data1[5], item.data1[6], item.data1[7],
          item.data1[8], item.data1[9], item.data1[10], item.data1[11],
          item.data2[0], item.data2[1], item.data2[2], item.data2[3]);
      log_info("Description: %s", desc.c_str());
      break;
    }

    case Behavior::SHOW_EP3_CARDS: {
      Episode3::CardIndex card_index("system/ep3/card-definitions.mnr", "system/ep3/card-definitions.mnrd", "system/ep3/card-text.mnr");

      auto card_ids = card_index.all_ids();
      log_info("%zu card definitions", card_ids.size());
      for (uint32_t card_id : card_ids) {
        auto entry = card_index.definition_for_id(card_id);
        string s = entry->def.str(one_line);
        if (one_line) {
          fprintf(stdout, "%s\n", s.c_str());
        } else {
          string tags = entry->debug_tags.empty() ? "(none)" : join(entry->debug_tags, ", ");
          string text = entry->text.empty() ? "(No text available)" : str_replace_all(entry->text, "\n", "\n    ");
          fprintf(stdout, "%s\n  Tags: %s\n  Text:\n    %s\n\n", s.c_str(), tags.c_str(), text.c_str());
        }
      }
      break;
    }

    case Behavior::SHOW_EP3_MAPS: {
      config_log.info("Collecting Episode 3 data");
      Episode3::MapIndex map_index("system/ep3");
      Episode3::CardIndex card_index("system/ep3/card-definitions.mnr", "system/ep3/card-definitions.mnrd", "system/ep3/card-text.mnr");

      auto map_ids = map_index.all_numbers();
      log_info("%zu maps", map_ids.size());
      for (uint32_t map_id : map_ids) {
        auto map = map_index.definition_for_number(map_id);
        string s = map->map.str(&card_index);
        fprintf(stdout, "%s\n", s.c_str());
      }
      break;
    }

    case Behavior::PARSE_OBJECT_GRAPH: {
      string data = read_input_data();
      PSOGCObjectGraph g(data, root_object_address);
      g.print(stdout);
      break;
    }

    case Behavior::GENERATE_PRODUCT: {
      auto product = generate_product(domain, subdomain);
      fprintf(stdout, "%s\n", product.c_str());
      break;
    }

    case Behavior::GENERATE_ALL_PRODUCTS: {
      auto products = generate_all_products();
      fprintf(stdout, "%zu (0x%zX) products found\n", products.size(), products.size());
      for (const auto& it : products) {
        fprintf(stdout, "Valid product: %08" PRIX32, it.first);
        for (uint8_t where : it.second) {
          fprintf(stdout, " (domain=%hhu, subdomain=%hhu)",
              static_cast<uint8_t>((where >> 2) & 3),
              static_cast<uint8_t>(where & 3));
        }
        fputc('\n', stdout);
      }

      atomic<uint64_t> num_valid_products = 0;
      mutex output_lock;
      auto thread_fn = [&](uint64_t product, size_t) -> bool {
        for (uint8_t domain = 0; domain < 3; domain++) {
          for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
            if (product_is_valid_fast(product, domain, subdomain)) {
              num_valid_products++;
              lock_guard g(output_lock);
              fprintf(stdout, "Valid product: %08" PRIX64 " (domain=%hhu, subdomain=%hhu)\n", product, domain, subdomain);
            }
          }
        }
        return false;
      };
      auto progress_fn = [&](uint64_t, uint64_t, uint64_t current_value, uint64_t) -> void {
        uint64_t num_found = num_valid_products.load();
        fprintf(stderr, "... %08" PRIX64 " %" PRId64 " (0x%" PRIX64 ") found\r",
            current_value, num_found, num_found);
      };
      parallel_range<uint64_t>(thread_fn, 0, 0x100000000, num_threads, progress_fn);
      break;
    }

    case Behavior::INSPECT_PRODUCT: {
      if (!input_filename) {
        throw invalid_argument("no product given");
      }
      size_t num_valid_subdomains = 0;
      for (uint8_t domain = 0; domain < 3; domain++) {
        for (uint8_t subdomain = 0; subdomain < 3; subdomain++) {
          if (product_is_valid_fast(input_filename, domain, subdomain)) {
            fprintf(stdout, "%s is valid in domain %hhu subdomain %hhu\n", input_filename, domain, subdomain);
            num_valid_subdomains++;
          }
        }
      }
      if (num_valid_subdomains == 0) {
        fprintf(stdout, "%s is not valid in any domain\n", input_filename);
      }
      break;
    }

    case Behavior::PRODUCT_SPEED_TEST:
      if (seed.empty()) {
        product_speed_test();
      } else {
        product_speed_test(stoul(seed, nullptr, 16));
      }
      break;

    case Behavior::REPLAY_LOG:
    case Behavior::RUN_SERVER: {
      bool is_replay = behavior == Behavior::REPLAY_LOG;
      signal(SIGPIPE, SIG_IGN);

      if (isatty(fileno(stderr))) {
        use_terminal_colors = true;
      }

      if (is_replay) {
        set_function_compiler_available(false);
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

        shared_ptr<FILE> log_f(
            stdin, +[](FILE*) {});
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
                auto [ss, size] = make_sockaddr_storage(
                    state->proxy_destination_patch.first,
                    state->proxy_destination_patch.second);
                state->proxy_server->listen(pc->port, pc->version, &ss);
              } else if (pc->version == GameVersion::BB) {
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
