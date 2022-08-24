#include "Quest.hh"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <phosg/Filesystem.hh>
#include <phosg/Encoding.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "CommandFormats.hh"
#include "Compression.hh"
#include "PSOEncryption.hh"
#include "Text.hh"

using namespace std;



struct PSODownloadQuestHeader {
  // When sending a DLQ to the client, this is the DECOMPRESSED size. When
  // reading it from a GCI file, this is the COMPRESSED size.
  le_uint32_t size;
  // Note: use PSO PC encryption, even for GC quests.
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
      return "GovernmentEpisode1";
    case QuestCategory::GOVERNMENT_EPISODE_2:
      return "GovernmentEpisode2";
    case QuestCategory::GOVERNMENT_EPISODE_4:
      return "GovernmentEpisode4";
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

static const char* name_for_episode(uint8_t episode) {
  switch (episode) {
    case 0:
      return "Ep1";
    case 1:
      return "Ep2";
    case 2:
      return "Ep4";
    case 0xFF:
      return "Ep3";
    default:
      return "InvalidEpisode";
  }
}



struct PSOQuestHeaderDC { // same for dc v1 and v2, thankfully
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
  uint8_t episode; // 1 = ep2. apparently some quests have 0xFF here, which means ep1 (?)
  ptext<char, 0x20> name;
  ptext<char, 0x80> short_description;
  ptext<char, 0x120> long_description;
} __attribute__((packed));

struct PSOQuestHeaderGCEpisode3 {
  // there's actually a lot of other important stuff in here but I'm lazy. it
  // looks like map data, cutscene data, and maybe special cards used during
  // the quest
  parray<uint8_t, 0x1DF0> unknown_a1;
  ptext<char, 0x14> name;
  ptext<char, 0x14> location;
  ptext<char, 0x3C> location2;
  ptext<char, 0x190> description;
  parray<uint8_t, 0x3A34> unknown_a2;
} __attribute__((packed));

struct PSOQuestHeaderBB {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint16_t quest_number; // 0xFFFF for challenge quests
  uint16_t unused2;
  uint8_t episode; // 0 = ep1, 1 = ep2, 2 = ep4
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
    episode(0),
    is_dcv1(false),
    joinable(false),
    file_format(FileFormat::BIN_DAT) {

  if (ends_with(bin_filename, ".bin.gci")) {
    this->file_format = FileFormat::BIN_DAT_GCI;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 8);
  } else if (ends_with(bin_filename, ".bin.dlq")) {
    this->file_format = FileFormat::BIN_DAT_DLQ;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 8);
  } else if (ends_with(bin_filename, ".qst")) {
    this->file_format = FileFormat::QST;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 4);
  } else if (ends_with(bin_filename, ".bin")) {
    this->file_format = FileFormat::BIN_DAT;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 4);
  } else if (ends_with(bin_filename, ".bind")) {
    this->file_format = FileFormat::BIN_DAT_UNCOMPRESSED;
    this->file_basename = bin_filename.substr(0, bin_filename.size() - 5);
  } else {
    throw runtime_error("quest does not have a valid .bin file");
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

  // quest filenames are like:
  // b###-VV.bin for battle mode
  // c###-VV.bin for challenge mode
  // e###-gc3.bin for episode 3
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

  // if the quest category is still unknown, expect 3 tokens (one of them will
  // tell us the category)
  vector<string> tokens = split(basename, '-');
  if (tokens.size() != (2 + (this->category == QuestCategory::UNKNOWN))) {
    throw invalid_argument("incorrect filename format");
  }

  // parse the number out of the first token
  this->internal_id = strtoull(tokens[0].c_str() + 1, nullptr, 10);

  // get the category from the second token if needed
  if (this->category == QuestCategory::UNKNOWN) {
    static const unordered_map<std::string, QuestCategory> name_to_category({
      {"ret", QuestCategory::RETRIEVAL},
      {"ext", QuestCategory::EXTERMINATION},
      {"evt", QuestCategory::EVENT},
      {"shp", QuestCategory::SHOP},
      {"vr",  QuestCategory::VR},
      {"twr", QuestCategory::TOWER},
      // Note: This will be overwritten later for Episode 2 & 4 quests - we
      // haven't parsed the episode from the quest script yet
      {"gov", QuestCategory::GOVERNMENT_EPISODE_1},
      {"dl",  QuestCategory::DOWNLOAD},
      {"1p",  QuestCategory::SOLO},
    });
    this->category = name_to_category.at(tokens[1]);
    tokens.erase(tokens.begin() + 1);
  }

  static const unordered_map<std::string, GameVersion> name_to_version({
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
      this->episode = 0;
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
      this->episode = 0;
      this->name = header->name;
      this->short_description = header->short_description;
      this->long_description = header->long_description;
      break;
    }

    case GameVersion::XB:
    case GameVersion::GC: {
      if (this->category == QuestCategory::EPISODE_3) {
        // these all appear to be the same size
        if (bin_decompressed.size() != sizeof(PSOQuestHeaderGCEpisode3)) {
          throw invalid_argument("file is incorrect size");
        }
        auto* header = reinterpret_cast<const PSOQuestHeaderGCEpisode3*>(bin_decompressed.data());
        this->joinable = false;
        this->episode = 0xFF;
        this->name = decode_sjis(header->name);
        this->short_description = decode_sjis(header->location2);
        this->long_description = decode_sjis(header->description);
      } else {
        if (bin_decompressed.size() < sizeof(PSOQuestHeaderGC)) {
          throw invalid_argument("file is too small for header");
        }
        auto* header = reinterpret_cast<const PSOQuestHeaderGC*>(bin_decompressed.data());
        this->joinable = false;
        this->episode = (header->episode == 1);
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
      this->episode = header->episode;
      this->name = header->name;
      this->short_description = header->short_description;
      this->long_description = header->long_description;
      if (this->category == QuestCategory::GOVERNMENT_EPISODE_1) {
        if (this->episode == 1) {
          this->category = QuestCategory::GOVERNMENT_EPISODE_2;
        } else if (this->episode == 2) {
          this->category = QuestCategory::GOVERNMENT_EPISODE_4;
        } else if (this->episode != 0) {
          throw invalid_argument("government quest has invalid episode number");
        }
      }
      break;
    }

    default:
      throw logic_error("invalid quest game version");
  }
}

static string basename_for_filename(const std::string& filename) {
  size_t slash_pos = filename.rfind('/');
  if (slash_pos != string::npos) {
    return filename.substr(slash_pos + 1);
  }
  return filename;
}

std::string Quest::bin_filename() const {
  return basename_for_filename(this->file_basename + ".bin");
}

std::string Quest::dat_filename() const {
  return basename_for_filename(this->file_basename + ".dat");
}

shared_ptr<const string> Quest::bin_contents() const {
  if (!this->bin_contents_ptr) {
    switch (this->file_format) {
      case FileFormat::BIN_DAT:
        this->bin_contents_ptr.reset(new string(load_file(this->file_basename + ".bin")));
        break;
      case FileFormat::BIN_DAT_UNCOMPRESSED:
        this->bin_contents_ptr.reset(new string(prs_compress(load_file(this->file_basename + ".bind"))));
        break;
      case FileFormat::BIN_DAT_GCI:
        this->bin_contents_ptr.reset(new string(this->decode_gci(this->file_basename + ".bin.gci")));
        break;
      case FileFormat::BIN_DAT_DLQ:
        this->bin_contents_ptr.reset(new string(this->decode_dlq(this->file_basename + ".bin.dlq")));
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

string Quest::decode_gci(const string& filename) {

  string data = load_file(filename);
  if (data.size() < 0x2080 + sizeof(PSODownloadQuestHeader)) {
    throw runtime_error(string_printf(
        "GCI file is truncated before download quest header (have 0x%zX bytes)", data.size()));
  }
  PSODownloadQuestHeader* h = reinterpret_cast<PSODownloadQuestHeader*>(
      data.data() + 0x2080);

  string compressed_data_with_header = data.substr(0x2088, h->size);

  // For now, we can only load unencrypted quests, unfortunately
  // TODO: Figure out how GCI encryption works and implement it here.

  // Unlike the DLQ header, this one is stored little-endian. The compressed
  // data immediately follows this header.
  struct DecryptedHeader {
    uint32_t unknown1;
    uint32_t unknown2;
    uint32_t decompressed_size;
    uint32_t unknown4;
  } __attribute__((packed));
  if (compressed_data_with_header.size() < sizeof(DecryptedHeader)) {
    throw runtime_error("GCI file compressed data truncated during header");
  }
  DecryptedHeader* dh = reinterpret_cast<DecryptedHeader*>(
      compressed_data_with_header.data());
  if (dh->unknown1 || dh->unknown2 || dh->unknown4) {
    throw runtime_error("GCI file appears to be encrypted");
  }

  string data_to_decompress = compressed_data_with_header.substr(sizeof(DecryptedHeader));
  size_t decompressed_bytes = prs_decompress_size(data_to_decompress);

  size_t expected_decompressed_bytes = dh->decompressed_size - 8;
  if (decompressed_bytes < expected_decompressed_bytes) {
    throw runtime_error(string_printf(
        "GCI decompressed data is smaller than expected size (have 0x%zX bytes, expected 0x%zX bytes)",
        decompressed_bytes, expected_decompressed_bytes));
  }

  // The caller expects to get PRS-compressed data when calling bin_contents()
  // and dat_contents(), so we shouldn't decompress it here.
  return data_to_decompress;
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
  while (!r.eof()) {
    // Handle BB's implicit 8-byte command alignment
    static constexpr size_t alignment = sizeof(HeaderT);
    size_t next_command_offset = (r.where() + (alignment - 1)) & ~(alignment - 1);
    r.go(next_command_offset);
    if (r.eof()) {
      break;
    }

    const auto& header = r.get<HeaderT>();
    if (header.command == 0x44) {
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

    } else if (header.command == 0x13) {
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
      dest->append(reinterpret_cast<const char*>(cmd.data), cmd.data_size);

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

  return make_pair(bin_contents, dat_contents);
}

pair<string, string> Quest::decode_qst(const string& filename) {
  auto f = fopen_unique(filename, "rb");

  // qst files start with an open file command, but the format differs depending
  // on the PSO version that the qst file is for. We can detect the format from
  // the first 4 bytes in the file:
  // - BB: 58 00 44 00
  // - PC: 3C ?? 44 00
  // - DC/V3: 44 ?? 3C 00
  uint32_t signature = freadx<be_uint32_t>(f.get());
  fseek(f.get(), 0, SEEK_SET);
  if (signature == 0x58004400) {
    return decode_qst_t<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(f.get());
  } else if ((signature & 0xFF00FFFF) == 0x3C004400) {
    return decode_qst_t<PSOCommandHeaderPC, S_OpenFile_PC_V3_44_A6>(f.get());
  } else if ((signature & 0xFF00FFFF) == 0x44003C00) {
    return decode_qst_t<PSOCommandHeaderDCV3, S_OpenFile_PC_V3_44_A6>(f.get());
  } else {
    throw runtime_error("invalid qst file format");
  }
}



QuestIndex::QuestIndex(const std::string& directory) : directory(directory) {
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
        ends_with(filename, ".bin.dlq") ||
        ends_with(filename, ".qst")) {
      try {
        shared_ptr<Quest> q(new Quest(full_path));
        q->menu_item_id = next_menu_item_id++;
        string ascii_name = encode_sjis(q->name);
        if (!this->version_menu_item_id_to_quest.emplace(
            make_pair(q->version, q->menu_item_id), q).second) {
          throw logic_error("duplicate quest menu item id");
        }
        static_game_data_log.info("Indexed quest %s (%s-%" PRId64 " => %" PRIu32 ", %s, %s, joinable=%s, dcv1=%s)",
            ascii_name.c_str(), name_for_version(q->version), q->internal_id,
            q->menu_item_id, name_for_category(q->category), name_for_episode(q->episode),
            q->joinable ? "true" : "false", q->is_dcv1 ? "true" : "false");
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

vector<shared_ptr<const Quest>> QuestIndex::filter(GameVersion version,
    bool is_dcv1, QuestCategory category) const {
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
  // are encrypted with the PSOPC encryption (even on V3 / PSO GC), and a small
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
  data.resize((data.size() + 3) & (~3));

  PSOV3Encryption encr(encryption_seed);
  encr.encrypt(data.data() + sizeof(PSODownloadQuestHeader),
      data.size() - sizeof(PSODownloadQuestHeader));

  return data;
}

shared_ptr<Quest> Quest::create_download_quest() const {
  // The download flag needs to be set in the bin header, or else the client
  // will ignore it when scanning for download quests in an offline game. To set
  // this flag, we need to decompress the quest's .bin file, set the flag, then
  // recompress it again.

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

  // We'll create a new Quest object with appropriately-processed .bin and .dat
  // file contents.
  shared_ptr<Quest> dlq(new Quest(*this));
  dlq->bin_contents_ptr.reset(new string(create_download_quest_file(
      compressed_bin, decompressed_bin.size())));
  dlq->dat_contents_ptr.reset(new string(create_download_quest_file(
      *this->dat_contents(), prs_decompress_size(*this->dat_contents()))));
  return dlq;
}
