#include "Quest.hh"

#include <algorithm>
#include <mutex>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <string>
#include <unordered_map>

#include "CommandFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "QuestScript.hh"
#include "SaveFileFormats.hh"
#include "Text.hh"

using namespace std;

QuestCategoryIndex::Category::Category(uint32_t category_id, std::shared_ptr<const JSONObject> json)
    : category_id(category_id) {
  const auto& l = json->as_list();
  this->flags = l.at(0)->as_int();
  this->type = l.at(1)->as_string().at(0);
  this->short_token = l.at(2)->as_string();
  this->name = decode_sjis(l.at(3)->as_string());
  this->description = decode_sjis(l.at(4)->as_string());
}

bool QuestCategoryIndex::Category::matches_flags(uint8_t request) const {
  // If the request is for v1 or v2 (hence it has the HIDE_ON_PRE_V3 flag set)
  // and the category also has that flag set, it never matches
  if (request & this->flags & Flag::HIDE_ON_PRE_V3) {
    return false;
  }
  return request & this->flags;
}

QuestCategoryIndex::QuestCategoryIndex(std::shared_ptr<const JSONObject> json) {
  uint32_t next_category_id = 1;
  for (const auto& it : json->as_list()) {
    this->categories.emplace_back(next_category_id++, it);
  }
}

const QuestCategoryIndex::Category& QuestCategoryIndex::find(char type, const std::string& short_token) const {
  // Technically we should index these and do a map lookup, but there will
  // probably always only be a small constant number of them
  for (const auto& it : this->categories) {
    if (it.type == type && it.short_token == short_token) {
      return it;
    }
  }
  throw out_of_range(string_printf("no category with type %c and short_token %s", type, short_token.c_str()));
}

const QuestCategoryIndex::Category& QuestCategoryIndex::at(uint32_t category_id) const {
  return this->categories.at(category_id - 1);
}

// GCI decoding logic

template <bool IsBigEndian>
struct PSOMemCardDLQFileEncryptedHeader {
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  U32T round2_seed;
  // To compute checksum, set checksum to zero, then compute the CRC32 of the
  // entire data section, including this header struct (but not the unencrypted
  // header struct).
  U32T checksum;
  le_uint32_t decompressed_size;
  le_uint32_t round3_seed;
  // Data follows here.
} __attribute__((packed));

struct PSOVMSDLQFileEncryptedHeader : PSOMemCardDLQFileEncryptedHeader<false> {
} __attribute__((packed));
struct PSOGCIDLQFileEncryptedHeader : PSOMemCardDLQFileEncryptedHeader<true> {
} __attribute__((packed));

template <bool IsBigEndian>
string decrypt_gci_or_vms_v2_download_quest_data_section(
    const void* data_section, size_t size, uint32_t seed) {
  string decrypted = decrypt_gci_or_vms_v2_data_section<IsBigEndian>(
      data_section, size, seed);

  size_t orig_size = decrypted.size();
  decrypted.resize((decrypted.size() + 3) & (~3));

  // Note: Other PSO save files have the round 2 seed at the end of the data,
  // not at the beginning. Presumably they did this because the system,
  // character, and Guild Card files are a constant size, but download quest
  // files can vary in size.
  using HeaderT = PSOMemCardDLQFileEncryptedHeader<IsBigEndian>;
  auto* header = reinterpret_cast<HeaderT*>(decrypted.data());
  PSOV2Encryption round2_crypt(header->round2_seed);
  round2_crypt.encrypt_t<IsBigEndian>(
      decrypted.data() + 4, (decrypted.size() - 4));

  if (header->decompressed_size & 0xFFF00000) {
    throw runtime_error(string_printf(
        "decompressed_size too large (%08" PRIX32 ")", header->decompressed_size.load()));
  }

  uint32_t expected_crc = header->checksum;
  header->checksum = 0;
  uint32_t actual_crc = crc32(decrypted.data(), orig_size);
  header->checksum = expected_crc;
  if (expected_crc != actual_crc && expected_crc != bswap32(actual_crc)) {
    throw runtime_error(string_printf(
        "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
        expected_crc, actual_crc));
  }

  // Unlike the above rounds, round 3 is always little-endian (it corresponds to
  // the round of encryption done on the server before sending the file to the
  // client in the first place)
  PSOV2Encryption(header->round3_seed).decrypt(decrypted.data() + sizeof(HeaderT), decrypted.size() - sizeof(HeaderT));
  decrypted.resize(orig_size);

  // Some download quest GCI files have decompressed_size fields that are 8
  // bytes smaller than the actual decompressed size of the data. They seem to
  // work fine, so we accept both cases as correct.
  size_t decompressed_size = prs_decompress_size(
      decrypted.data() + sizeof(HeaderT),
      decrypted.size() - sizeof(HeaderT));
  if ((decompressed_size != header->decompressed_size) &&
      (decompressed_size != header->decompressed_size - 8)) {
    throw runtime_error(string_printf(
        "decompressed size (%zu) does not match size in header (%" PRId32 ")",
        decompressed_size, header->decompressed_size.load()));
  }

  return decrypted.substr(sizeof(HeaderT));
}

string decrypt_vms_v1_data_section(const void* data_section, size_t size) {
  StringReader r(data_section, size);
  uint32_t expected_decompressed_size = r.get_u32l();
  uint32_t seed = r.get_u32l();

  string data = r.read(r.remaining());

  size_t orig_size = data.size();
  data.resize((orig_size + 3) & (~3));
  PSOV2Encryption(seed).decrypt(data.data(), data.size());
  data.resize(orig_size);

  size_t actual_decompressed_size = prs_decompress_size(data);
  if (actual_decompressed_size != expected_decompressed_size) {
    throw runtime_error(string_printf(
        "decompressed size (%zu) does not match size in header (%" PRId32 ")",
        actual_decompressed_size, expected_decompressed_size));
  }

  return data;
}

template <bool IsBigEndian>
string find_seed_and_decrypt_gci_or_vms_v2_download_quest_data_section(
    const void* data_section, size_t size, size_t num_threads) {
  mutex result_lock;
  string result;
  uint64_t result_seed = parallel_range<uint64_t>([&](uint64_t seed, size_t) {
    try {
      string ret = decrypt_gci_or_vms_v2_download_quest_data_section<IsBigEndian>(
          data_section, size, seed);
      lock_guard<mutex> g(result_lock);
      result = std::move(ret);
      return true;
    } catch (const runtime_error&) {
      return false;
    }
  },
      0, 0x100000000, num_threads);

  if (!result.empty() && (result_seed < 0x100000000)) {
    static_game_data_log.info("Found seed %08" PRIX64, result_seed);
    return result;
  } else {
    throw runtime_error("no seed found");
  }
}

struct PSOVMSFileHeader {
  ptext<char, 0x10> short_desc; // "PSO/DOWNLOAD    " or "PSOV2/DOWNLOAD  "
  ptext<char, 0x20> long_desc; // Usually quest name
  ptext<char, 0x10> creator_id;
  le_uint16_t num_icons;
  le_uint16_t animation_speed;
  le_uint16_t eyecatch_type;
  le_uint16_t crc;
  le_uint32_t data_size; // Not including header and icons
  parray<uint8_t, 0x14> unused;
  parray<le_uint16_t, 0x10> icon_palette;

  // Variable-length field follows here:
  // parray<parray<uint8_t, 0x200>, num_icons> icon;

  bool checksum_correct() const {
    auto add_data = +[](const void* data, size_t size, uint16_t crc) -> uint16_t {
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
      for (size_t z = 0; z < size; z++) {
        crc ^= (static_cast<uint16_t>(bytes[z]) << 8);
        for (uint8_t bit = 0; bit < 8; bit++) {
          if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
          } else {
            crc = (crc << 1);
          }
        }
      }
      return crc;
    };

    uint16_t crc = add_data(this, offsetof(PSOVMSFileHeader, crc), 0);
    crc = add_data("\0\0", 2, crc);
    crc = add_data(&this->data_size,
        sizeof(PSOVMSFileHeader) - offsetof(PSOVMSFileHeader, data_size) + this->num_icons * 0x200 + this->data_size, crc);
    return (crc == this->crc);
  }
} __attribute__((packed));

struct PSODownloadQuestHeader {
  le_uint32_t size;
  le_uint32_t encryption_seed;
} __attribute__((packed));

Quest::Quest(const string& bin_filename, shared_ptr<const QuestCategoryIndex> category_index)
    : internal_id(-1),
      menu_item_id(0),
      category_id(0),
      episode(Episode::NONE),
      joinable(false),
      file_format(FileFormat::BIN_DAT),
      has_mnm_extension(false),
      is_dlq_encoded(false) {

  if (ends_with(bin_filename, ".bin.gci") || ends_with(bin_filename, ".mnm.gci")) {
    this->file_format = FileFormat::BIN_DAT_GCI;
    this->has_mnm_extension = ends_with(bin_filename, ".mnm.gci");
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 8);
  } else if (ends_with(bin_filename, ".bin.vms")) {
    this->file_format = FileFormat::BIN_DAT_VMS;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 8);
  } else if (ends_with(bin_filename, ".bin.dlq") || ends_with(bin_filename, ".mnm.dlq")) {
    this->file_format = FileFormat::BIN_DAT_DLQ;
    this->has_mnm_extension = ends_with(bin_filename, ".mnm.dlq");
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 8);
  } else if (ends_with(bin_filename, ".qst")) {
    this->file_format = FileFormat::QST;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 4);
  } else if (ends_with(bin_filename, ".bin") || ends_with(bin_filename, ".mnm")) {
    this->file_format = FileFormat::BIN_DAT;
    this->has_mnm_extension = ends_with(bin_filename, ".mnm");
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 4);
  } else if (ends_with(bin_filename, ".bind") || ends_with(bin_filename, ".mnmd")) {
    this->file_format = FileFormat::BIN_DAT_UNCOMPRESSED;
    this->has_mnm_extension = ends_with(bin_filename, ".mnmd");
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 5);
  } else {
    throw runtime_error("quest does not have a valid .bin or .mnm file");
  }

  string basename;
  {
    size_t slash_pos = this->file_basename.rfind('/');
    if (slash_pos != string::npos) {
      basename = this->file_basename.substr(slash_pos + 1);
    } else {
      basename = this->file_basename;
    }
  }

  if (basename.empty()) {
    throw invalid_argument("empty filename");
  }

  vector<string> tokens = split(basename, '-');

  string category_token;
  if (tokens.size() == 3) {
    category_token = std::move(tokens[1]);
    tokens.erase(tokens.begin() + 1);
  } else if (tokens.size() != 2) {
    throw invalid_argument("incorrect filename format");
  }

  auto& category = category_index->find(basename[0], category_token);
  this->category_id = category.category_id;

  // Parse the number out of the first token
  this->internal_id = strtoull(tokens[0].c_str() + 1, nullptr, 10);

  // Get the version from the second (or previously third) token
  static const unordered_map<string, QuestScriptVersion> name_to_version({
      {"dn", QuestScriptVersion::DC_NTE},
      {"d1", QuestScriptVersion::DC_V1},
      {"dc", QuestScriptVersion::DC_V2},
      {"pc", QuestScriptVersion::PC_V2},
      {"gcn", QuestScriptVersion::GC_NTE},
      {"gc", QuestScriptVersion::GC_V3},
      {"gc3", QuestScriptVersion::GC_EP3},
      {"xb", QuestScriptVersion::XB_V3},
      {"bb", QuestScriptVersion::BB_V4},
  });
  this->version = name_to_version.at(tokens[1]);

  // The rest of the information needs to be fetched from the .bin file's
  // contents

  auto bin_compressed = this->bin_contents();
  auto bin_decompressed = prs_decompress(*bin_compressed);

  switch (this->version) {
    case QuestScriptVersion::DC_NTE:
    case QuestScriptVersion::DC_V1:
    case QuestScriptVersion::DC_V2: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderDC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderDC*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP1;
      this->name = decode_sjis(header->name);
      this->short_description = decode_sjis(header->short_description);
      this->long_description = decode_sjis(header->long_description);
      break;
    }

    case QuestScriptVersion::PC_V2: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderPC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderPC*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP1;
      this->name = header->name;
      this->short_description = header->short_description;
      this->long_description = header->long_description;
      break;
    }

    case QuestScriptVersion::GC_EP3: {
      // Note: This codepath handles Episode 3 download quests, which are not
      // the same as Episode 3 quest scripts. The latter are only used offline
      // in story mode, but can be disassembled with disassemble_quest_script.
      // It's unfortunate that the QuestScriptVersion::GC_EP3 value is used
      // here for Episode 3 download quests (maps) and there for offline story
      // mode scripts, but it's probably not worth refactoring this logic, at
      // least right now.
      if (bin_decompressed.size() != sizeof(Episode3::MapDefinition)) {
        throw invalid_argument("file is incorrect size");
      }
      auto* header = reinterpret_cast<const Episode3::MapDefinition*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP3;
      this->name = decode_sjis(header->name);
      this->short_description = decode_sjis(header->quest_name);
      this->long_description = decode_sjis(header->description);
      break;
    }

    case QuestScriptVersion::XB_V3:
    case QuestScriptVersion::GC_NTE:
    case QuestScriptVersion::GC_V3: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderGC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderGC*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = (header->episode == 1) ? Episode::EP2 : Episode::EP1;
      this->name = decode_sjis(header->name);
      this->short_description = decode_sjis(header->short_description);
      this->long_description = decode_sjis(header->long_description);
      break;
    }

    case QuestScriptVersion::BB_V4: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderBB)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderBB*>(bin_decompressed.data());
      this->joinable = header->joinable_in_progress;
      switch (header->episode) {
        case 0:
          this->episode = Episode::EP1;
          break;
        case 1:
          this->episode = Episode::EP2;
          break;
        case 2:
          this->episode = Episode::EP4;
          break;
        default:
          throw runtime_error("invalid episode number");
      }
      this->name = header->name;
      this->short_description = header->short_description;
      this->long_description = header->long_description;
      break;
    }

    default:
      throw logic_error("invalid quest game version");
  }

  if (this->has_mnm_extension && this->episode != Episode::EP3) {
    throw runtime_error("non-Episode 3 quest has .mnm extension");
  }
}

static string basename_for_filename(const string& filename) {
  size_t slash_pos = filename.rfind('/');
  if (slash_pos != string::npos) {
    return filename.substr(slash_pos + 1);
  }
  return filename;
}

string Quest::bin_filename() const {
  if (this->episode == Episode::EP3) {
    return string_printf("m%06" PRId64 "p_e.bin", this->internal_id);
  } else {
    return basename_for_filename(this->file_basename + ".bin");
  }
}

string Quest::dat_filename() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have .dat files");
  } else {
    return basename_for_filename(this->file_basename + ".dat");
  }
}

shared_ptr<const string> Quest::bin_contents() const {
  if (!this->bin_contents_ptr) {
    switch (this->file_format) {
      case FileFormat::BIN_DAT:
        this->bin_contents_ptr.reset(new string(load_file(
            this->file_basename + (this->has_mnm_extension ? ".mnm" : ".bin"))));
        break;
      case FileFormat::BIN_DAT_UNCOMPRESSED:
        this->bin_contents_ptr.reset(new string(prs_compress(load_file(
            this->file_basename + (this->has_mnm_extension ? ".mnmd" : ".bind")))));
        break;
      case FileFormat::BIN_DAT_GCI:
        this->bin_contents_ptr.reset(new string(this->decode_gci_file(
            this->file_basename + (this->has_mnm_extension ? ".mnm.gci" : ".bin.gci"))));
        break;
      case FileFormat::BIN_DAT_VMS:
        this->bin_contents_ptr.reset(new string(this->decode_vms_file(
            this->file_basename + (this->has_mnm_extension ? ".mnm.vms" : ".bin.vms"))));
        break;
      case FileFormat::BIN_DAT_DLQ:
        this->bin_contents_ptr.reset(new string(this->decode_dlq_file(
            this->file_basename + (this->has_mnm_extension ? ".mnm.dlq" : ".bin.dlq"))));
        break;
      case FileFormat::QST: {
        auto result = this->decode_qst_file(this->file_basename + ".qst");
        this->bin_contents_ptr.reset(new string(std::move(result.first)));
        this->dat_contents_ptr.reset(new string(std::move(result.second)));
        break;
      }
      default:
        throw logic_error("invalid quest file format");
    }
  }
  return this->bin_contents_ptr;
}

shared_ptr<const string> Quest::dat_contents() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have .dat files");
  }
  if (!this->dat_contents_ptr) {
    switch (this->file_format) {
      case FileFormat::BIN_DAT:
        this->dat_contents_ptr.reset(new string(load_file(this->file_basename + ".dat")));
        break;
      case FileFormat::BIN_DAT_UNCOMPRESSED:
        this->dat_contents_ptr.reset(new string(prs_compress(load_file(this->file_basename + ".datd"))));
        break;
      case FileFormat::BIN_DAT_GCI:
        this->dat_contents_ptr.reset(new string(this->decode_gci_file(this->file_basename + ".dat.gci")));
        break;
      case FileFormat::BIN_DAT_VMS:
        this->dat_contents_ptr.reset(new string(this->decode_vms_file(this->file_basename + ".dat.vms")));
        break;
      case FileFormat::BIN_DAT_DLQ:
        this->dat_contents_ptr.reset(new string(this->decode_dlq_file(this->file_basename + ".dat.dlq")));
        break;
      case FileFormat::QST: {
        auto result = this->decode_qst_file(this->file_basename + ".qst");
        this->bin_contents_ptr.reset(new string(std::move(result.first)));
        this->dat_contents_ptr.reset(new string(std::move(result.second)));
        break;
      }
      default:
        throw logic_error("invalid quest file format");
    }
  }
  return this->dat_contents_ptr;
}

string Quest::decode_gci_file(
    const string& filename, ssize_t find_seed_num_threads, int64_t known_seed) {
  string data = load_file(filename);

  StringReader r(data);
  const auto& header = r.get<PSOGCIFileHeader>();
  header.check();

  if (header.is_ep12()) {
    const auto& dlq_header = r.get<PSOGCIDLQFileEncryptedHeader>(false);
    // Unencrypted GCI files appear to always have zeroes in these fields.
    // Encrypted GCI files are highly unlikely to have zeroes in ALL of these
    // fields, so assume it's encrypted if any of them are nonzero.
    if (dlq_header.round2_seed || dlq_header.checksum || dlq_header.round3_seed) {
      if (known_seed >= 0) {
        return decrypt_gci_or_vms_v2_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, known_seed);

      } else if (header.embedded_seed != 0) {
        return decrypt_gci_or_vms_v2_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, header.embedded_seed);

      } else {
        if (find_seed_num_threads < 0) {
          throw runtime_error("file is encrypted");
        }
        if (find_seed_num_threads == 0) {
          find_seed_num_threads = thread::hardware_concurrency();
        }
        return find_seed_and_decrypt_gci_or_vms_v2_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, find_seed_num_threads);
      }

    } else { // Unencrypted GCI format
      r.skip(sizeof(PSOGCIDLQFileEncryptedHeader));
      string compressed_data = r.readx(header.data_size - sizeof(PSOGCIDLQFileEncryptedHeader));
      size_t decompressed_bytes = prs_decompress_size(compressed_data);

      size_t expected_decompressed_bytes = dlq_header.decompressed_size - 8;
      if (decompressed_bytes < expected_decompressed_bytes) {
        throw runtime_error(string_printf(
            "GCI decompressed data is smaller than expected size (have 0x%zX bytes, expected 0x%zX bytes)",
            decompressed_bytes, expected_decompressed_bytes));
      }

      return compressed_data;
    }

  } else if (header.game_id[2] == 'S') { // Episode 3
    // The first 0x10 bytes in the data segment appear to be unused. In most
    // files I've seen, the last half of it (8 bytes) are duplicates of the
    // first 8 bytes of the unscrambled, compressed data, though this is the
    // result of an uninitialized memory bug when the client encodes the file
    // and not an actual constraint on what should be in these 8 bytes.
    r.skip(16);
    // The game treats this field as a 16-byte string (including the \0). The 8
    // bytes after it appear to be completely unused.
    if (r.readx(15) != "SONICTEAM,SEGA.") {
      throw runtime_error("Episode 3 GCI file is not a quest");
    }
    r.skip(9);

    data = r.readx(header.data_size - 40);

    // For some reason, Sega decided not to encrypt Episode 3 quest files in the
    // same way as Episodes 1&2 quest files (see above). Instead, they just
    // wrote a fairly trivial XOR loop over the first 0x100 bytes, leaving the
    // remaining bytes completely unencrypted (but still compressed).
    size_t unscramble_size = min<size_t>(0x100, data.size());
    decrypt_trivial_gci_data(data.data(), unscramble_size, 0);

    size_t decompressed_size = prs_decompress_size(data);
    if (decompressed_size != sizeof(Episode3::MapDefinition)) {
      throw runtime_error(string_printf(
          "decompressed quest is 0x%zX bytes; expected 0x%zX bytes",
          decompressed_size, sizeof(Episode3::MapDefinition)));
    }
    return data;

  } else {
    throw runtime_error("unknown game name in GCI header");
  }
}

string Quest::decode_vms_file(
    const string& filename, ssize_t find_seed_num_threads, int64_t known_seed) {
  string data = load_file(filename);

  StringReader r(data);
  const auto& header = r.get<PSOVMSFileHeader>();
  if (!header.checksum_correct()) {
    throw runtime_error("VMS file unencrypted header checksum is incorrect");
  }
  r.skip(header.num_icons * 0x200);

  const void* data_section = r.getv(header.data_size);
  try {
    return decrypt_vms_v1_data_section(data_section, header.data_size);
  } catch (const exception& e) {
  }

  if (known_seed >= 0) {
    return decrypt_gci_or_vms_v2_download_quest_data_section<false>(
        data_section, header.data_size, known_seed);

  } else {
    if (find_seed_num_threads < 0) {
      throw runtime_error("file is encrypted");
    }
    if (find_seed_num_threads == 0) {
      find_seed_num_threads = thread::hardware_concurrency();
    }
    return find_seed_and_decrypt_gci_or_vms_v2_download_quest_data_section<false>(
        data_section, header.data_size, find_seed_num_threads);
  }
}

string Quest::decode_dlq_data(const string& data) {
  StringReader r(data);
  uint32_t decompressed_size = r.get_u32l();
  uint32_t key = r.get_u32l();

  // The compressed data size does not need to be a multiple of 4, but the V2
  // encryption (which is used for all download quests, even in V3) requires the
  // data size to be a multiple of 4. We'll just temporarily stick a few bytes
  // on the end, then throw them away later if needed.
  string decrypted = r.read(r.remaining());
  PSOV2Encryption encr(key);
  size_t original_size = data.size();
  decrypted.resize((decrypted.size() + 3) & (~3));
  encr.decrypt(decrypted);
  decrypted.resize(original_size);

  if (prs_decompress_size(decrypted) != decompressed_size) {
    throw runtime_error("decompressed size does not match size in header");
  }

  return decrypted;
}

string Quest::decode_dlq_file(const string& filename) {
  auto f = fopen_unique(filename, "rb");
  return Quest::decode_dlq_data(read_all(f.get()));
}

template <typename HeaderT, typename OpenFileT>
static pair<string, string> decode_qst_t(FILE* f) {
  string qst_data = read_all(f);
  StringReader r(qst_data);

  string bin_contents;
  string dat_contents;
  string internal_bin_filename;
  string internal_dat_filename;
  uint32_t bin_file_size = 0;
  uint32_t dat_file_size = 0;
  Quest::FileFormat subformat = Quest::FileFormat::QST; // Stand-in for unknown
  while (!r.eof()) {
    // Handle BB's implicit 8-byte command alignment
    static constexpr size_t alignment = sizeof(HeaderT);
    size_t next_command_offset = (r.where() + (alignment - 1)) & ~(alignment - 1);
    r.go(next_command_offset);
    if (r.eof()) {
      break;
    }

    const auto& header = r.get<HeaderT>();

    if (header.command == 0x44 || header.command == 0x13) {
      if (subformat == Quest::FileFormat::QST) {
        subformat = Quest::FileFormat::BIN_DAT;
      } else if (subformat != Quest::FileFormat::BIN_DAT) {
        throw runtime_error("QST file contains mixed download and non-download commands");
      }
    } else if (header.command == 0xA6 || header.command == 0xA7) {
      if (subformat == Quest::FileFormat::QST) {
        subformat = Quest::FileFormat::BIN_DAT_DLQ;
      } else if (subformat != Quest::FileFormat::BIN_DAT_DLQ) {
        throw runtime_error("QST file contains mixed download and non-download commands");
      }
    }

    if (header.command == 0x44 || header.command == 0xA6) {
      if (header.size != sizeof(HeaderT) + sizeof(OpenFileT)) {
        throw runtime_error("qst open file command has incorrect size");
      }
      const auto& cmd = r.get<OpenFileT>(f);
      string internal_filename = cmd.filename;

      if (ends_with(internal_filename, ".bin")) {
        if (internal_bin_filename.empty()) {
          internal_bin_filename = internal_filename;
        } else {
          throw runtime_error("qst contains multiple bin files");
        }
        bin_file_size = cmd.file_size;

      } else if (ends_with(internal_filename, ".dat")) {
        if (internal_dat_filename.empty()) {
          internal_dat_filename = internal_filename;
        } else {
          throw runtime_error("qst contains multiple dat files");
        }
        dat_file_size = cmd.file_size;

      } else {
        throw runtime_error("qst contains non-bin, non-dat file");
      }

    } else if (header.command == 0x13 || header.command == 0xA7) {
      if (header.size != sizeof(HeaderT) + sizeof(S_WriteFile_13_A7)) {
        throw runtime_error("qst write file command has incorrect size");
      }
      const auto& cmd = r.get<S_WriteFile_13_A7>();
      string filename = cmd.filename;

      string* dest = nullptr;
      if (filename == internal_bin_filename) {
        dest = &bin_contents;
      } else if (filename == internal_dat_filename) {
        dest = &dat_contents;
      } else {
        throw runtime_error("qst contains write commnd for non-open file");
      }

      if (cmd.data_size > 0x400) {
        throw runtime_error("qst contains invalid write command");
      }
      if (dest->size() & 0x3FF) {
        throw runtime_error("qst contains uneven chunks out of order");
      }
      if (header.flag != dest->size() / 0x400) {
        throw runtime_error("qst contains chunks out of order");
      }
      dest->append(reinterpret_cast<const char*>(cmd.data.data()), cmd.data_size);

    } else {
      throw runtime_error("invalid command in qst file");
    }
  }

  if (bin_contents.size() != bin_file_size) {
    throw runtime_error("bin file does not match expected size");
  }
  if (dat_contents.size() != dat_file_size) {
    throw runtime_error("dat file does not match expected size");
  }

  if (subformat == Quest::FileFormat::BIN_DAT_DLQ) {
    bin_contents = Quest::decode_dlq_file(bin_contents);
    dat_contents = Quest::decode_dlq_file(dat_contents);
  }

  return make_pair(bin_contents, dat_contents);
}

pair<string, string> Quest::decode_qst_file(const string& filename) {
  auto f = fopen_unique(filename, "rb");

  // QST files start with an open file command, but the format differs depending
  // on the PSO version that the qst file is for. We can detect the format from
  // the first 4 bytes in the file:
  // - BB:    58 00 44 00 or 58 00 A6 00
  // - PC:    3C 00 44 ?? or 3C 00 A6 ??
  // - DC/V3: 44 ?? 3C 00 or A6 ?? 3C 00
  uint32_t signature = freadx<be_uint32_t>(f.get());
  fseek(f.get(), 0, SEEK_SET);
  if (signature == 0x58004400 || signature == 0x5800A600) {
    return decode_qst_t<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(f.get());
  } else if ((signature & 0xFFFFFF00) == 0x3C004400 || (signature & 0xFFFFFF00) == 0x3C00A600) {
    return decode_qst_t<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(f.get());
  } else if ((signature & 0xFF00FFFF) == 0x44003C00 || (signature & 0xFF00FFFF) == 0xA6003C00) {
    return decode_qst_t<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(f.get());
  } else {
    throw runtime_error("invalid qst file format");
  }
}

template <typename HeaderT>
void add_command_header(
    StringWriter& w, uint8_t command, uint8_t flag, uint16_t size) {
  HeaderT header;
  header.command = command;
  header.flag = flag;
  header.size = sizeof(HeaderT) + size;
  w.put(header);
}

template <typename HeaderT, typename CmdT>
void add_open_file_command(StringWriter& w, const Quest& q, bool is_bin) {
  add_command_header<HeaderT>(
      w, q.is_dlq_encoded ? 0xA6 : 0x44, q.internal_id,
      sizeof(S_OpenFile_DC_44_A6));
  CmdT cmd;
  cmd.name = "PSO/" + encode_sjis(q.name);
  cmd.filename = q.file_basename + (is_bin ? ".bin" : ".dat");
  cmd.type = 0;
  // TODO: It'd be nice to have something like w.emplace(...) to avoid copying
  // the command structs into the StringWriter.
  w.put(cmd);
}

template <typename HeaderT>
void add_write_file_commands(
    StringWriter& w,
    const string& filename,
    const string& data,
    bool is_dlq_encoded) {
  for (size_t z = 0; z < data.size(); z += 0x400) {
    size_t chunk_size = min<size_t>(data.size() - z, 0x400);
    add_command_header<HeaderT>(w, is_dlq_encoded ? 0xA7 : 0x13, z >> 10, sizeof(S_WriteFile_13_A7));
    S_WriteFile_13_A7 cmd;
    cmd.filename = filename;
    memcpy(cmd.data.data(), &data[z], chunk_size);
    cmd.data_size = chunk_size;
    w.put(cmd);
  }
}

string Quest::export_qst() const {
  bool is_ep3 = this->episode == Episode::EP3;
  if (is_ep3 && !this->is_dlq_encoded) {
    throw runtime_error("Episode 3 quests can only be encoded in download QST format");
  }

  StringWriter w;

  // Some tools expect both open file commands at the beginning, hence this
  // unfortunate abstraction-breaking.
  switch (this->version) {
    case QuestScriptVersion::DC_NTE:
    case QuestScriptVersion::DC_V1:
    case QuestScriptVersion::DC_V2:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case QuestScriptVersion::PC_V2:
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderPC>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderPC>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case QuestScriptVersion::GC_NTE:
    case QuestScriptVersion::GC_V3:
    case QuestScriptVersion::XB_V3:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case QuestScriptVersion::GC_EP3:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(w, *this, true);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      break;
    case QuestScriptVersion::BB_V4:
      add_open_file_command<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderBB>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderBB>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    default:
      throw logic_error("invalid game version");
  }

  return std::move(w.str());
}

QuestIndex::QuestIndex(
    const string& directory,
    std::shared_ptr<const QuestCategoryIndex> category_index)
    : directory(directory),
      category_index(category_index) {

  uint32_t next_menu_item_id = 1;
  for (const auto& filename : list_directory_sorted(this->directory)) {
    string full_path = this->directory + "/" + filename;

    if (ends_with(filename, ".gba")) {
      shared_ptr<string> contents(new string(load_file(full_path)));
      this->gba_file_contents.emplace(make_pair(filename, contents));
      continue;
    }

    if (ends_with(filename, ".bin") ||
        ends_with(filename, ".bind") ||
        ends_with(filename, ".bin.gci") ||
        ends_with(filename, ".bin.vms") ||
        ends_with(filename, ".bin.dlq") ||
        ends_with(filename, ".mnm") ||
        ends_with(filename, ".mnmd") ||
        ends_with(filename, ".mnm.gci") ||
        ends_with(filename, ".mnm.dlq") ||
        ends_with(filename, ".qst")) {
      try {
        shared_ptr<Quest> q(new Quest(full_path, this->category_index));
        q->menu_item_id = next_menu_item_id++;
        string ascii_name = encode_sjis(q->name);
        if (!this->version_menu_item_id_to_quest.emplace(make_pair(q->version, q->menu_item_id), q).second) {
          throw logic_error("duplicate quest menu item id");
        }
        auto category_name = encode_sjis(this->category_index->at(q->category_id).name);
        static_game_data_log.info("Indexed quest %s (%s => %s-%" PRId64 " (%" PRIu32 "), %s, %s (%" PRIu32 "), joinable=%s)",
            ascii_name.c_str(),
            filename.c_str(),
            name_for_enum(q->version),
            q->internal_id,
            q->menu_item_id,
            name_for_episode(q->episode),
            category_name.c_str(),
            q->category_id,
            q->joinable ? "true" : "false");
      } catch (const exception& e) {
        static_game_data_log.warning("Failed to index quest file %s (%s)", filename.c_str(), e.what());
      }
    }
  }
}

shared_ptr<const Quest> QuestIndex::get(
    QuestScriptVersion version, uint32_t menu_item_id) const {
  return this->version_menu_item_id_to_quest.at(make_pair(version, menu_item_id));
}

shared_ptr<const string> QuestIndex::get_gba(const string& name) const {
  return this->gba_file_contents.at(name);
}

vector<shared_ptr<const Quest>> QuestIndex::filter(
    QuestScriptVersion version, uint32_t category_id) const {
  auto it = this->version_menu_item_id_to_quest.lower_bound(make_pair(version, 0));
  auto end_it = this->version_menu_item_id_to_quest.upper_bound(make_pair(version, 0xFFFFFFFF));

  vector<shared_ptr<const Quest>> ret;
  for (; it != end_it; it++) {
    if (it->second->category_id == category_id) {
      ret.emplace_back(it->second);
    }
  }

  return ret;
}

static string create_download_quest_file(const string& compressed_data,
    size_t decompressed_size, uint32_t encryption_seed = 0) {
  // Download quest files are like normal (PRS-compressed) quest files, but they
  // are encrypted with PSO V2 encryption (even on V3 / PSO GC), and a small
  // header (PSODownloadQuestHeader) is prepended to the encrypted data.

  if (encryption_seed == 0) {
    encryption_seed = random_object<uint32_t>();
  }

  string data(8, '\0');
  auto* header = reinterpret_cast<PSODownloadQuestHeader*>(data.data());
  header->size = decompressed_size;
  header->encryption_seed = encryption_seed;
  data += compressed_data;

  // Add temporary extra bytes if necessary so encryption won't fail - the data
  // size must be a multiple of 4 for PSO V2 encryption.
  size_t original_size = data.size();
  data.resize((data.size() + 3) & (~3));

  PSOV2Encryption encr(encryption_seed);
  encr.encrypt(data.data() + sizeof(PSODownloadQuestHeader),
      data.size() - sizeof(PSODownloadQuestHeader));
  data.resize(original_size);

  return data;
}

shared_ptr<Quest> Quest::create_download_quest() const {
  // The download flag needs to be set in the bin header, or else the client
  // will ignore it when scanning for download quests in an offline game. To set
  // this flag, we need to decompress the quest's .bin file, set the flag, then
  // recompress it again.

  // This function should not be used for Episode 3 quests (they should be sent
  // to the client as-is, without any encryption or other preprocessing)
  if (this->episode == Episode::EP3 || this->version == QuestScriptVersion::GC_EP3) {
    throw logic_error("Episode 3 quests cannot be converted to download quests");
  }

  string decompressed_bin = prs_decompress(*this->bin_contents());

  void* data_ptr = decompressed_bin.data();
  switch (this->version) {
    case QuestScriptVersion::DC_NTE:
    case QuestScriptVersion::DC_V1:
    case QuestScriptVersion::DC_V2:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderDC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderDC*>(data_ptr)->is_download = 0x01;
      break;
    case QuestScriptVersion::PC_V2:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderPC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderPC*>(data_ptr)->is_download = 0x01;
      break;
    case QuestScriptVersion::GC_NTE:
    case QuestScriptVersion::GC_V3:
    case QuestScriptVersion::XB_V3:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderGC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderGC*>(data_ptr)->is_download = 0x01;
      break;
    case QuestScriptVersion::BB_V4:
      throw invalid_argument("PSOBB does not support download quests");
    case QuestScriptVersion::GC_EP3:
      throw logic_error("Episode 3 quests cannot be converted to download quests");
    default:
      throw invalid_argument("unknown game version");
  }

  string compressed_bin = prs_compress(decompressed_bin);

  // Return a new Quest object with appropriately-processed .bin and .dat file
  // contents
  shared_ptr<Quest> dlq(new Quest(*this));
  dlq->bin_contents_ptr.reset(new string(create_download_quest_file(
      compressed_bin, decompressed_bin.size())));
  dlq->dat_contents_ptr.reset(new string(create_download_quest_file(
      *this->dat_contents(), prs_decompress_size(*this->dat_contents()))));
  dlq->is_dlq_encoded = true;
  return dlq;
}
