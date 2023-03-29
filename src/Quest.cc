#include "Quest.hh"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <phosg/Filesystem.hh>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>

#include "Loggers.hh"
#include "CommandFormats.hh"
#include "Compression.hh"
#include "PSOEncryption.hh"
#include "Text.hh"

using namespace std;



// GCI decoding logic

struct ShuffleTables {
  uint8_t forward_table[0x100];
  uint8_t reverse_table[0x100];

  ShuffleTables(PSOV2Encryption& crypt) {
    for (size_t x = 0; x < 0x100; x++) {
      this->forward_table[x] = x;
    }

    int32_t r28 = 0xFF;
    uint8_t* r31 = &this->forward_table[0xFF];
    while (r28 >= 0) {
      uint32_t r3 = this->pseudorand(crypt, r28 + 1);
      if (r3 >= 0x100) {
        throw logic_error("bad r3");
      }
      uint8_t t = this->forward_table[r3];
      this->forward_table[r3] = *r31;
      *r31 = t;

      this->reverse_table[t] = r28;
      r31--;
      r28--;
    }
  }

  static uint32_t pseudorand(PSOV2Encryption& crypt, uint32_t prev) {
    return (((prev & 0xFFFF) * ((crypt.next() >> 16) & 0xFFFF)) >> 16) & 0xFFFF;
  }

  void shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) {
    uint8_t* dest = reinterpret_cast<uint8_t*>(vdest);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(vsrc);
    const uint8_t* table = reverse ? this->reverse_table : this->forward_table;

    for (size_t block_offset = 0; block_offset < (size & 0xFFFFFF00); block_offset += 0x100) {
      for (size_t z = 0; z < 0x100; z++) {
        dest[block_offset + table[z]] = src[block_offset + z];
      }
    }

    // Any remaining bytes that don't fill an entire block are not shuffled
    memcpy(&dest[size & 0xFFFFFF00], &src[size & 0xFFFFFF00], size & 0xFF);
  }
};

struct PSOGCIFileHeader {
  parray<char, 4> game_id; // 'GPOE', 'GPSP', etc.
  parray<char, 2> developer_id; // '8P' for Sega
  parray<uint8_t, 0x3A> remaining_gci_header; // There is a structure for this but we don't use it
  ptext<char, 0x1C> game_name; // e.g. "PSO EPISODE I & II" or "PSO EPISODE III"
  be_uint32_t embedded_seed; // Used in some of Ralf's quest packs
  ptext<char, 0x20> quest_name;
  parray<uint8_t, 0x2000> banner_and_icon;
  // data_size specifies the number of bytes in the encrypted section, including
  // the encrypted header (below) and all encrypted data after it.
  be_uint32_t data_size;
  // To compute checksum, set checksum to zero, then compute the CRC32 of all
  // fields in this struct starting with game_name. (Yes, including the checksum
  // field, which is temporarily zero.)
  be_uint32_t checksum;

  bool checksum_correct() const {
    uint32_t cs = crc32(&this->game_name, sizeof(this->game_name));
    cs = crc32(&this->embedded_seed, sizeof(this->embedded_seed), cs);
    cs = crc32(&this->quest_name, sizeof(this->quest_name), cs);
    cs = crc32(&this->banner_and_icon, sizeof(this->banner_and_icon), cs);
    cs = crc32(&this->data_size, sizeof(this->data_size), cs);
    cs = crc32("\0\0\0\0", 4, cs);
    return (cs == this->checksum);
  }
} __attribute__((packed));

template <typename U32T>
struct PSOMemCardFileEncryptedHeader {
  U32T round2_seed;
  // To compute checksum, set checksum to zero, then compute the CRC32 of the
  // entire data section, including this header struct (but not the unencrypted
  // header struct).
  U32T checksum;
  le_uint32_t decompressed_size;
  le_uint32_t round3_seed;
  // Data follows here.
} __attribute__((packed));

struct PSOVMSFileEncryptedHeader : PSOMemCardFileEncryptedHeader<le_uint32_t> { } __attribute__((packed));
struct PSOGCIFileEncryptedHeader : PSOMemCardFileEncryptedHeader<be_uint32_t> { } __attribute__((packed));

template <bool IsBigEndian>
string decrypt_gci_or_vms_v2_data_section(
    const void* data_section, size_t size, uint32_t seed) {

  string decrypted(size, '\0');
  {
    PSOV2Encryption shuf_crypt(seed);
    ShuffleTables shuf(shuf_crypt);
    shuf.shuffle(decrypted.data(), data_section, size, true);
  }

  size_t orig_size = decrypted.size();
  decrypted.resize((decrypted.size() + 3) & (~3));

  PSOV2Encryption crypt(seed);
  if (IsBigEndian) {
    auto* be_dwords = reinterpret_cast<be_uint32_t*>(decrypted.data());
    for (size_t z = 0; z < decrypted.size() / sizeof(be_uint32_t); z++) {
      be_dwords[z] = crypt.next() - be_dwords[z];
    }
  } else {
    auto* le_dwords = reinterpret_cast<le_uint32_t*>(decrypted.data());
    for (size_t z = 0; z < decrypted.size() / sizeof(le_uint32_t); z++) {
      le_dwords[z] = crypt.next() - le_dwords[z];
    }
  }

  // Note: Other PSO save files have the round 2 seed at the end of the data,
  // not at the beginning. Presumably they did this because the system,
  // character, and Guild Card files are a constant size, but download quest
  // files can vary in size.
  using HeaderT = typename conditional<IsBigEndian, PSOMemCardFileEncryptedHeader<be_uint32_t>, PSOMemCardFileEncryptedHeader<le_uint32_t>>::type;
  auto* header = reinterpret_cast<HeaderT*>(decrypted.data());
  PSOV2Encryption round2_crypt(header->round2_seed);
  if (IsBigEndian) {
    round2_crypt.encrypt_big_endian(
        decrypted.data() + 4, (decrypted.size() - 4));
  } else {
    round2_crypt.decrypt(
        decrypted.data() + 4, (decrypted.size() - 4));
  }

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

  PSOV2Encryption(header->round3_seed).decrypt(
      decrypted.data() + sizeof(HeaderT),
      decrypted.size() - sizeof(HeaderT));
  decrypted.resize(orig_size);

  // Some GCI files have decompressed_size fields that are 8 bytes smaller than
  // the actual decompressed size of the data. They seem to work fine, so we
  // accept both cases as correct.
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
string find_seed_and_decrypt_gci_or_vms_v2_data_section(
    const void* data_section, size_t size, size_t num_threads) {
  mutex result_lock;
  string result;
  uint64_t result_seed = parallel_range<uint64_t>([&](uint64_t seed, size_t) {
    try {
      string ret = decrypt_gci_or_vms_v2_data_section<IsBigEndian>(
          data_section, size, seed);
      lock_guard<mutex> g(result_lock);
      result = move(ret);
      return true;
    } catch (const runtime_error&) {
      return false;
    }
  }, 0, 0x100000000, num_threads);

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



bool category_is_mode(QuestCategory category) {
  return (category == QuestCategory::BATTLE) ||
         (category == QuestCategory::CHALLENGE) ||
         (category == QuestCategory::EPISODE_3);
}

const char* name_for_category(QuestCategory category) {
  switch (category) {
    case QuestCategory::RETRIEVAL:
      return "Retrieval";
    case QuestCategory::EXTERMINATION:
      return "Extermination";
    case QuestCategory::EVENT:
      return "Event";
    case QuestCategory::SHOP:
      return "Shop";
    case QuestCategory::VR:
      return "VR";
    case QuestCategory::TOWER:
      return "Tower";
    case QuestCategory::GOVERNMENT_EPISODE_1:
      return "GovernmentEp1";
    case QuestCategory::GOVERNMENT_EPISODE_2:
      return "GovernmentEp2";
    case QuestCategory::GOVERNMENT_EPISODE_4:
      return "GovernmentEp4";
    case QuestCategory::DOWNLOAD:
      return "Download";
    case QuestCategory::BATTLE:
      return "Battle";
    case QuestCategory::CHALLENGE:
      return "Challenge";
    case QuestCategory::SOLO:
      return "Solo";
    case QuestCategory::EPISODE_3:
      return "Episode3";
    default:
      return "Unknown";
  }
}



struct PSOQuestHeaderDC { // Same format for DC v1 and v2, thankfully
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  uint16_t quest_number; // 0xFFFF for challenge quests
  ptext<char, 0x20> name;
  ptext<char, 0x80> short_description;
  ptext<char, 0x120> long_description;
} __attribute__((packed));

struct PSOQuestHeaderPC {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  uint16_t quest_number; // 0xFFFF for challenge quests
  ptext<char16_t, 0x20> name;
  ptext<char16_t, 0x80> short_description;
  ptext<char16_t, 0x120> long_description;
} __attribute__((packed));

// TODO: Is the XB quest header format the same as on GC? If not, make a
// separate struct; if so, rename this struct to V3.
struct PSOQuestHeaderGC {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  uint8_t quest_number;
  uint8_t episode; // 1 = Ep2. Apparently some quests have 0xFF here, which means ep1 (?)
  ptext<char, 0x20> name;
  ptext<char, 0x80> short_description;
  ptext<char, 0x120> long_description;
} __attribute__((packed));

struct PSOQuestHeaderBB {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint16_t quest_number; // 0xFFFF for challenge quests
  uint16_t unused2;
  uint8_t episode; // 0 = Ep1, 1 = Ep2, 2 = Ep4
  uint8_t max_players;
  uint8_t joinable_in_progress;
  uint8_t unknown;
  ptext<char16_t, 0x20> name;
  ptext<char16_t, 0x80> short_description;
  ptext<char16_t, 0x120> long_description;
} __attribute__((packed));



Quest::Quest(const string& bin_filename)
  : internal_id(-1),
    menu_item_id(0),
    category(QuestCategory::UNKNOWN),
    episode(Episode::NONE),
    is_dcv1(false),
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

  // Quest filenames are like:
  // b###-VV.bin for battle mode
  // c###-VV.bin for challenge mode
  // e###-gc3.mnm (or .bin) for episode 3
  // q###-CAT-VV.bin for normal quests

  if (basename.empty()) {
    throw invalid_argument("empty filename");
  }

  if (basename[0] == 'b') {
    this->category = QuestCategory::BATTLE;
  } else if (basename[0] == 'c') {
    this->category = QuestCategory::CHALLENGE;
  } else if (basename[0] == 'e') {
    this->category = QuestCategory::EPISODE_3;
  } else if (basename[0] != 'q') {
    throw invalid_argument("filename does not indicate mode");
  }

  if (this->category != QuestCategory::EPISODE_3 && this->has_mnm_extension) {
    throw invalid_argument("non-Ep3 quest has .mnm extension");
  }

  // If the quest category is still unknown, expect 3 tokens (one of them will
  // tell us the category)
  vector<string> tokens = split(basename, '-');
  if (tokens.size() != (2 + (this->category == QuestCategory::UNKNOWN))) {
    throw invalid_argument("incorrect filename format");
  }

  // Parse the number out of the first token
  this->internal_id = strtoull(tokens[0].c_str() + 1, nullptr, 10);

  // Get the category from the second token if needed
  if (this->category == QuestCategory::UNKNOWN) {
    static const unordered_map<string, QuestCategory> name_to_category({
      {"ret", QuestCategory::RETRIEVAL},
      {"ext", QuestCategory::EXTERMINATION},
      {"evt", QuestCategory::EVENT},
      {"shp", QuestCategory::SHOP},
      {"vr",  QuestCategory::VR},
      {"twr", QuestCategory::TOWER},
      // Note: This will be overwritten later for Episode 2 & 4 quests - we
      // haven't parsed the episode number from the quest script yet
      {"gov", QuestCategory::GOVERNMENT_EPISODE_1},
      {"dl",  QuestCategory::DOWNLOAD},
      {"1p",  QuestCategory::SOLO},
    });
    this->category = name_to_category.at(tokens[1]);
    tokens.erase(tokens.begin() + 1);
  }

  static const unordered_map<string, GameVersion> name_to_version({
    {"d1",  GameVersion::DC},
    {"dc",  GameVersion::DC},
    {"pc",  GameVersion::PC},
    {"gc",  GameVersion::GC},
    {"gc3", GameVersion::GC},
    {"xb",  GameVersion::XB},
    {"bb",  GameVersion::BB},
  });
  this->version = name_to_version.at(tokens[1]);

  // The rest of the information needs to be fetched from the .bin file's
  // contents

  auto bin_compressed = this->bin_contents();
  auto bin_decompressed = prs_decompress(*bin_compressed);

  switch (this->version) {
    case GameVersion::PATCH:
      throw invalid_argument("patch server quests are not valid");
      break;

    case GameVersion::DC: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderDC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderDC*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP1;
      this->name = decode_sjis(header->name);
      this->short_description = decode_sjis(header->short_description);
      this->long_description = decode_sjis(header->long_description);
      this->is_dcv1 = (tokens[1] == "d1");
      break;
    }

    case GameVersion::PC: {
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

    case GameVersion::XB:
    case GameVersion::GC: {
      if (this->category == QuestCategory::EPISODE_3) {
        if (bin_decompressed.size() != sizeof(Episode3::MapDefinition)) {
          throw invalid_argument("file is incorrect size");
        }
        auto* header = reinterpret_cast<const Episode3::MapDefinition*>(bin_decompressed.data());
        this->joinable = false;
        this->episode = Episode::EP3;
        this->name = decode_sjis(header->name);
        this->short_description = decode_sjis(header->quest_name);
        this->long_description = decode_sjis(header->description);
      } else {
        if (bin_decompressed.size() < sizeof(PSOQuestHeaderGC)) {
          throw invalid_argument("file is too small for header");
        }
        auto* header = reinterpret_cast<const PSOQuestHeaderGC*>(bin_decompressed.data());
        this->joinable = false;
        this->episode = (header->episode == 1) ? Episode::EP2 : Episode::EP1;
        this->name = decode_sjis(header->name);
        this->short_description = decode_sjis(header->short_description);
        this->long_description = decode_sjis(header->long_description);
      }
      break;
    }

    case GameVersion::BB: {
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
      if (this->category == QuestCategory::GOVERNMENT_EPISODE_1) {
        if (this->episode == Episode::EP2) {
          this->category = QuestCategory::GOVERNMENT_EPISODE_2;
        } else if (this->episode == Episode::EP4) {
          this->category = QuestCategory::GOVERNMENT_EPISODE_4;
        } else if (this->episode != Episode::EP1) {
          throw invalid_argument("government quest has invalid episode number");
        }
      }
      break;
    }

    default:
      throw logic_error("invalid quest game version");
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
  if (this->category == QuestCategory::EPISODE_3) {
    return string_printf("m%06" PRId64 "p_e.bin", this->internal_id);
  } else {
    return basename_for_filename(this->file_basename + ".bin");
  }
}

string Quest::dat_filename() const {
  if (this->category == QuestCategory::EPISODE_3) {
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
        this->bin_contents_ptr.reset(new string(this->decode_gci(
            this->file_basename + (this->has_mnm_extension ? ".mnm.gci" : ".bin.gci"))));
        break;
      case FileFormat::BIN_DAT_VMS:
        this->bin_contents_ptr.reset(new string(this->decode_vms(
            this->file_basename + (this->has_mnm_extension ? ".mnm.vms" : ".bin.vms"))));
        break;
      case FileFormat::BIN_DAT_DLQ:
        this->bin_contents_ptr.reset(new string(this->decode_dlq(
            this->file_basename + (this->has_mnm_extension ? ".mnm.dlq" : ".bin.dlq"))));
        break;
      case FileFormat::QST: {
        auto result = this->decode_qst(this->file_basename + ".qst");
        this->bin_contents_ptr.reset(new string(move(result.first)));
        this->dat_contents_ptr.reset(new string(move(result.second)));
        break;
      }
      default:
        throw logic_error("invalid quest file format");
    }
  }
  return this->bin_contents_ptr;
}

shared_ptr<const string> Quest::dat_contents() const {
  if (this->category == QuestCategory::EPISODE_3) {
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
        this->dat_contents_ptr.reset(new string(this->decode_gci(this->file_basename + ".dat.gci")));
        break;
      case FileFormat::BIN_DAT_VMS:
        this->dat_contents_ptr.reset(new string(this->decode_vms(this->file_basename + ".dat.vms")));
        break;
      case FileFormat::BIN_DAT_DLQ:
        this->dat_contents_ptr.reset(new string(this->decode_dlq(this->file_basename + ".dat.dlq")));
        break;
      case FileFormat::QST: {
        auto result = this->decode_qst(this->file_basename + ".qst");
        this->bin_contents_ptr.reset(new string(move(result.first)));
        this->dat_contents_ptr.reset(new string(move(result.second)));
        break;
      }
      default:
        throw logic_error("invalid quest file format");
    }
  }
  return this->dat_contents_ptr;
}

string Quest::decode_gci(
    const string& filename, ssize_t find_seed_num_threads, int64_t known_seed) {
  string data = load_file(filename);

  StringReader r(data);
  const auto& header = r.get<PSOGCIFileHeader>();
  if (!header.checksum_correct()) {
    throw runtime_error("GCI file unencrypted header checksum is incorrect");
  }

  if (header.developer_id[0] != '8' || header.developer_id[1] != 'P') {
    throw runtime_error("GCI file is not for a Sega game");
  }
  if (header.game_id[0] != 'G') {
    throw runtime_error("GCI file is not for a GameCube game");
  }
  if (header.game_id[1] != 'P') {
    throw runtime_error("GCI file is not for Phantasy Star Online");
  }

  if (header.game_id[2] == 'O') { // Episodes 1&2 (GPO*)
    const auto& encrypted_header = r.get<PSOGCIFileEncryptedHeader>(false);
    // Unencrypted GCI files appear to always have zeroes in these fields.
    // Encrypted GCI files are highly unlikely to have zeroes in ALL of these
    // fields, so assume it's encrypted if any of them are nonzero.
    if (encrypted_header.round2_seed || encrypted_header.checksum || encrypted_header.round3_seed) {
      if (known_seed >= 0) {
        return decrypt_gci_or_vms_v2_data_section<true>(
            r.getv(header.data_size), header.data_size, known_seed);

      } else if (header.embedded_seed != 0) {
        return decrypt_gci_or_vms_v2_data_section<true>(
            r.getv(header.data_size), header.data_size, header.embedded_seed);

      } else {
        if (find_seed_num_threads < 0) {
          throw runtime_error("file is encrypted");
        }
        if (find_seed_num_threads == 0) {
          find_seed_num_threads = thread::hardware_concurrency();
        }
        return find_seed_and_decrypt_gci_or_vms_v2_data_section<true>(
            r.getv(header.data_size), header.data_size, find_seed_num_threads);
      }

    } else { // Unencrypted GCI format
      r.skip(sizeof(PSOGCIFileEncryptedHeader));
      string compressed_data = r.readx(header.data_size - sizeof(PSOGCIFileEncryptedHeader));
      size_t decompressed_bytes = prs_decompress_size(compressed_data);

      size_t expected_decompressed_bytes = encrypted_header.decompressed_size - 8;
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

string Quest::decode_vms(
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
  } catch (const exception& e) { }

  if (known_seed >= 0) {
    return decrypt_gci_or_vms_v2_data_section<false>(
        data_section, header.data_size, known_seed);

  } else {
    if (find_seed_num_threads < 0) {
      throw runtime_error("file is encrypted");
    }
    if (find_seed_num_threads == 0) {
      find_seed_num_threads = thread::hardware_concurrency();
    }
    return find_seed_and_decrypt_gci_or_vms_v2_data_section<false>(
        data_section, header.data_size, find_seed_num_threads);
  }
}

string Quest::decode_dlq(const string& filename) {
  uint32_t decompressed_size;
  uint32_t key;
  string data;
  {
    auto f = fopen_unique(filename, "rb");
    decompressed_size = freadx<le_uint32_t>(f.get());
    key = freadx<le_uint32_t>(f.get());
    data = read_all(f.get());
  }

  // The compressed data size does not need to be a multiple of 4, but the V2
  // encryption (which is used for all download quests, even in V3) requires the
  // data size to be a multiple of 4. We'll just temporarily stick a few bytes
  // on the end, then throw them away later if needed.
  PSOV2Encryption encr(key);
  size_t original_size = data.size();
  data.resize((data.size() + 3) & (~3));
  encr.decrypt(data);
  data.resize(original_size);

  if (prs_decompress_size(data) != decompressed_size) {
    throw runtime_error("decompressed size does not match size in header");
  }

  return data;
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
    bin_contents = Quest::decode_dlq(bin_contents);
    dat_contents = Quest::decode_dlq(dat_contents);
  }

  return make_pair(bin_contents, dat_contents);
}

pair<string, string> Quest::decode_qst(const string& filename) {
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

string Quest::export_qst(GameVersion version) const {
  if (this->category == QuestCategory::EPISODE_3) {
    throw runtime_error("Episode 3 quests cannot be encoded in QST format");
  }

  StringWriter w;

  switch (version) {
    case GameVersion::DC:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case GameVersion::PC:
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderPC>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderPC>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case GameVersion::GC:
    case GameVersion::XB:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(w, *this, true);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(w, *this, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".bin", *this->bin_contents(), this->is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(
          w, this->file_basename + ".dat", *this->dat_contents(), this->is_dlq_encoded);
      break;
    case GameVersion::BB:
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

  return move(w.str());
}



QuestIndex::QuestIndex(const string& directory) : directory(directory) {
  auto filename_set = list_directory(this->directory);
  vector<string> filenames(filename_set.begin(), filename_set.end());
  sort(filenames.begin(), filenames.end());
  uint32_t next_menu_item_id = 1;
  for (const auto& filename : filenames) {
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
        shared_ptr<Quest> q(new Quest(full_path));
        q->menu_item_id = next_menu_item_id++;
        string ascii_name = encode_sjis(q->name);
        if (!this->version_menu_item_id_to_quest.emplace(
            make_pair(q->version, q->menu_item_id), q).second) {
          throw logic_error("duplicate quest menu item id");
        }
        static_game_data_log.info("Indexed quest %s (%s => %s-%" PRId64 " (%" PRIu32 "), %s, %s, joinable=%s, dcv1=%s)",
            ascii_name.c_str(),
            filename.c_str(),
            name_for_version(q->version),
            q->internal_id,
            q->menu_item_id,
            name_for_category(q->category),
            name_for_episode(q->episode),
            q->joinable ? "true" : "false",
            q->is_dcv1 ? "true" : "false");
      } catch (const exception& e) {
        static_game_data_log.warning("Failed to parse quest file %s (%s)", filename.c_str(), e.what());
      }
    }
  }
}

shared_ptr<const Quest> QuestIndex::get(GameVersion version,
    uint32_t menu_item_id) const {
  return this->version_menu_item_id_to_quest.at(make_pair(version, menu_item_id));
}

shared_ptr<const string> QuestIndex::get_gba(const string& name) const {
  return this->gba_file_contents.at(name);
}

vector<shared_ptr<const Quest>> QuestIndex::filter(
    GameVersion version, bool is_dcv1, QuestCategory category) const {
  auto it = this->version_menu_item_id_to_quest.lower_bound(make_pair(version, 0));
  auto end_it = this->version_menu_item_id_to_quest.upper_bound(make_pair(version, 0xFFFFFFFF));

  vector<shared_ptr<const Quest>> ret;
  for (; it != end_it; it++) {
    shared_ptr<const Quest> q = it->second;
    if ((q->is_dcv1 != is_dcv1) || (q->category != category)) {
      continue;
    }
    ret.emplace_back(q);
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
  if (this->category == QuestCategory::EPISODE_3) {
    throw logic_error("Episode 3 quests cannot be converted to download quests");
  }

  string decompressed_bin = prs_decompress(*this->bin_contents());

  void* data_ptr = decompressed_bin.data();
  switch (this->version) {
    case GameVersion::DC:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderDC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderDC*>(data_ptr)->is_download = 0x01;
      break;
    case GameVersion::PC:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderPC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderPC*>(data_ptr)->is_download = 0x01;
      break;
    case GameVersion::XB:
    case GameVersion::GC:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderGC)) {
        throw runtime_error("bin file is too small for header");
      }
      reinterpret_cast<PSOQuestHeaderGC*>(data_ptr)->is_download = 0x01;
      break;
    case GameVersion::BB:
      throw invalid_argument("PSOBB does not support download quests");
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
