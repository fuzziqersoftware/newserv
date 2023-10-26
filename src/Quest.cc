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

QuestCategoryIndex::Category::Category(uint32_t category_id, const JSON& json)
    : category_id(category_id) {
  this->flags = json.get_int(0);
  this->type = json.get_string(1).at(0);
  this->short_token = json.get_string(2);
  this->name = json.get_string(3);
  this->description = json.get_string(4);
}

bool QuestCategoryIndex::Category::matches_flags(uint8_t request) const {
  // If the request is for v1 or v2 (hence it has the HIDE_ON_PRE_V3 flag set)
  // and the category also has that flag set, it never matches
  if (request & this->flags & Flag::HIDE_ON_PRE_V3) {
    return false;
  }
  return request & this->flags;
}

QuestCategoryIndex::QuestCategoryIndex(const JSON& json) {
  uint32_t next_category_id = 1;
  for (const auto& it : json.as_list()) {
    this->categories.emplace_back(next_category_id++, *it);
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
string decrypt_download_quest_data_section(
    const void* data_section, size_t size, uint32_t seed, bool skip_checksum = false, bool is_ep3_trial = false) {
  string decrypted = decrypt_data_section<IsBigEndian>(data_section, size, seed);

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

  if (is_ep3_trial) {
    StringReader r(decrypted);
    r.skip(16);
    if (r.readx(15) != "SONICTEAM,SEGA.") {
      throw runtime_error("Episode 3 GCI file is not a quest");
    }
    r.skip(9);

    // Some Ep3 trial download quests don't have a stop opcode in the PRS
    // stream; it seems the client just automatically stops when the correct
    // amount of data has been produced. To handle this, we allow the PRS stream
    // to be unterminated here.
    size_t decompressed_size = prs_decompress_size(
        r.getv(r.remaining(), false), r.remaining(), sizeof(Episode3::MapDefinitionTrial), true);
    if (decompressed_size < sizeof(Episode3::MapDefinitionTrial)) {
      throw runtime_error(string_printf(
          "decompressed size (%zu) does not match expected size (%zu)",
          decompressed_size, sizeof(Episode3::MapDefinitionTrial)));
    }
    return decrypted.substr(0x28);

  } else {
    if (header->decompressed_size & 0xFFF00000) {
      throw runtime_error(string_printf(
          "decompressed_size too large (%08" PRIX32 ")", header->decompressed_size.load()));
    }

    if (!skip_checksum) {
      uint32_t expected_crc = header->checksum;
      header->checksum = 0;
      uint32_t actual_crc = crc32(decrypted.data(), orig_size);
      header->checksum = expected_crc;
      if (expected_crc != actual_crc && expected_crc != bswap32(actual_crc)) {
        throw runtime_error(string_printf(
            "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
            expected_crc, actual_crc));
      }
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
    size_t expected_decompressed_size = header->decompressed_size.load();
    if ((decompressed_size != expected_decompressed_size) &&
        (decompressed_size != expected_decompressed_size - 8)) {
      throw runtime_error(string_printf(
          "decompressed size (%zu) does not match expected size (%zu)",
          decompressed_size, expected_decompressed_size));
    }

    return decrypted.substr(sizeof(HeaderT));
  }
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
string find_seed_and_decrypt_download_quest_data_section(
    const void* data_section, size_t size, bool skip_checksum, bool is_ep3_trial, size_t num_threads) {
  mutex result_lock;
  string result;
  uint64_t result_seed = parallel_range<uint64_t>([&](uint64_t seed, size_t) {
    try {
      string ret = decrypt_download_quest_data_section<IsBigEndian>(
          data_section, size, seed, skip_checksum, is_ep3_trial);
      lock_guard<mutex> g(result_lock);
      result = std::move(ret);
      return true;
    } catch (const runtime_error& e) {
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

struct PSODownloadQuestHeader {
  le_uint32_t size;
  le_uint32_t encryption_seed;
} __attribute__((packed));

VersionedQuest::VersionedQuest(
    uint32_t quest_number,
    uint32_t category_id,
    QuestScriptVersion version,
    uint8_t language,
    std::shared_ptr<const std::string> bin_contents,
    std::shared_ptr<const std::string> dat_contents,
    std::shared_ptr<const BattleRules> battle_rules,
    ssize_t challenge_template_index)
    : quest_number(quest_number),
      category_id(category_id),
      episode(Episode::NONE),
      joinable(false),
      version(version),
      language(language),
      is_dlq_encoded(false),
      bin_contents(bin_contents),
      dat_contents(dat_contents),
      battle_rules(battle_rules),
      challenge_template_index(challenge_template_index) {

  auto bin_decompressed = prs_decompress(*this->bin_contents);

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
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
      break;
    }

    case QuestScriptVersion::PC_V2: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderPC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderPC*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP1;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
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
      auto* map = reinterpret_cast<const Episode3::MapDefinition*>(bin_decompressed.data());
      this->joinable = false;
      this->episode = Episode::EP3;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = map->map_number;
      }
      this->name = map->name.decode(this->language);
      this->short_description = map->quest_name.decode(this->language);
      this->long_description = map->description.decode(this->language);
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
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
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
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
      break;
    }

    default:
      throw logic_error("invalid quest game version");
  }
}

string VersionedQuest::bin_filename() const {
  if (this->episode == Episode::EP3) {
    return string_printf("m%06" PRIu32 "p_e.bin", this->quest_number);
  } else {
    return string_printf("q%" PRIu32 ".bin", this->quest_number);
  }
}

string VersionedQuest::dat_filename() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have .dat files");
  } else {
    return string_printf("q%" PRIu32 ".dat", this->quest_number);
  }
}

string VersionedQuest::encode_qst() const {
  return encode_qst_file(
      *this->bin_contents,
      *this->dat_contents,
      this->name,
      this->quest_number,
      this->version,
      this->is_dlq_encoded);
}

Quest::Quest(shared_ptr<const VersionedQuest> initial_version)
    : quest_number(initial_version->quest_number),
      category_id(initial_version->category_id),
      episode(initial_version->episode),
      joinable(initial_version->joinable),
      name(initial_version->name),
      battle_rules(initial_version->battle_rules),
      challenge_template_index(initial_version->challenge_template_index) {
  this->versions.emplace(this->versions_key(initial_version->version, initial_version->language), initial_version);
}

uint16_t Quest::versions_key(QuestScriptVersion v, uint8_t language) {
  return (static_cast<uint16_t>(v) << 8) | language;
}

void Quest::add_version(shared_ptr<const VersionedQuest> vq) {
  if (this->quest_number != vq->quest_number) {
    throw logic_error("incorrect versioned quest number");
  }
  if (this->category_id != vq->category_id) {
    throw runtime_error("quest version is in a different category");
  }
  if (this->episode != vq->episode) {
    throw runtime_error("quest version is in a different episode");
  }
  if (this->joinable != vq->joinable) {
    throw runtime_error("quest version has a different joinability state");
  }
  if (!this->battle_rules != !vq->battle_rules) {
    throw runtime_error("quest version has a different battle rules presence state");
  }
  if (this->battle_rules && (*this->battle_rules != *vq->battle_rules)) {
    throw runtime_error("quest version has different battle rules");
  }
  if (this->challenge_template_index != vq->challenge_template_index) {
    throw runtime_error("quest version has different challenge template index");
  }

  this->versions.emplace(this->versions_key(vq->version, vq->language), vq);
}

bool Quest::has_version(QuestScriptVersion v, uint8_t language) const {
  return this->versions.count(this->versions_key(v, language));
}

shared_ptr<const VersionedQuest> Quest::version(QuestScriptVersion v, uint8_t language) const {
  // Return the requested version, if it exists
  try {
    return this->versions.at(this->versions_key(v, language));
  } catch (const out_of_range&) {
  }
  // Return the English version, if it exists
  try {
    return this->versions.at(this->versions_key(v, 1));
  } catch (const out_of_range&) {
  }
  // Return the first language, if it exists
  auto it = this->versions.lower_bound(this->versions_key(v, 0));
  if ((it == this->versions.end()) || ((it->first & 0xFF00) != this->versions_key(v, 0))) {
    return nullptr;
  }
  return it->second;
}

QuestIndex::QuestIndex(
    const string& directory,
    std::shared_ptr<const QuestCategoryIndex> category_index)
    : directory(directory),
      category_index(category_index) {

  unordered_map<string, shared_ptr<const string>> dat_cache;
  unordered_map<string, shared_ptr<const JSON>> metadata_json_cache;

  for (const auto& bin_filename : list_directory_sorted(directory)) {
    string bin_path = this->directory + "/" + bin_filename;

    if (ends_with(bin_filename, ".gba")) {
      shared_ptr<string> contents(new string(load_file(bin_path)));
      this->gba_file_contents.emplace(make_pair(bin_filename, contents));
      continue;
    }

    try {
      QuestFileFormat format;
      string basename;
      if (ends_with(bin_filename, ".bin.gci") || ends_with(bin_filename, ".mnm.gci")) {
        format = QuestFileFormat::BIN_DAT_GCI;
        basename = bin_filename.substr(0, bin_filename.size() - 8);
      } else if (ends_with(bin_filename, ".bin.vms")) {
        format = QuestFileFormat::BIN_DAT_VMS;
        basename = bin_filename.substr(0, bin_filename.size() - 8);
      } else if (ends_with(bin_filename, ".bin.dlq") || ends_with(bin_filename, ".mnm.dlq")) {
        format = QuestFileFormat::BIN_DAT_DLQ;
        basename = bin_filename.substr(0, bin_filename.size() - 8);
      } else if (ends_with(bin_filename, ".qst")) {
        format = QuestFileFormat::QST;
        basename = bin_filename.substr(0, bin_filename.size() - 4);
      } else if (ends_with(bin_filename, ".bin") || ends_with(bin_filename, ".mnm")) {
        format = QuestFileFormat::BIN_DAT;
        basename = bin_filename.substr(0, bin_filename.size() - 4);
      } else if (ends_with(bin_filename, ".bind") || ends_with(bin_filename, ".mnmd")) {
        format = QuestFileFormat::BIN_DAT_UNCOMPRESSED;
        basename = bin_filename.substr(0, bin_filename.size() - 5);
      } else {
        continue; // Silently skip file
      }
      if (basename.empty()) {
        throw invalid_argument("empty filename");
      }
      if (basename.size() < 2) {
        throw logic_error("basename too short for language trim");
      }

      // Quest .bin filenames are like K###-CAT-VERS-LANG.EXT, where:
      //   K = class (quest, battle, challenge, etc.)
      //   # = quest number (does not have to match the internal quest number)
      //   CAT = menu category in which quest should appear (optional)
      //   VERS = PSO version that the quest is for
      //   LANG = client language (j, e, g, f, s)
      //   EXT = file type (bin, bind, bin.dlq, qst, etc.)
      // Quest .dat filenames are like K###-CAT-VERS.EXT (same as for .bin except
      // the LANG token is omitted)
      vector<string> filename_tokens = split(basename, '-');

      string category_token;
      if (filename_tokens.size() == 4) {
        category_token = std::move(filename_tokens[1]);
        filename_tokens.erase(filename_tokens.begin() + 1);
      } else if (filename_tokens.size() != 3) {
        throw invalid_argument("incorrect filename format");
      }

      uint32_t category_id = this->category_index
          ? this->category_index->find(basename[0], category_token).category_id
          : 0;

      // Parse the number out of the first token
      uint32_t quest_number = strtoull(filename_tokens[0].c_str() + 1, nullptr, 10);

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
      auto version = name_to_version.at(filename_tokens[1]);

      // Get the language from the last token
      if (filename_tokens[2].size() != 1) {
        throw runtime_error("language token is not a single character");
      }
      uint8_t language = language_code_for_char(filename_tokens[2][0]);

      shared_ptr<const string> bin_contents;
      shared_ptr<const string> dat_contents;
      string bin_data = load_file(bin_path);
      switch (format) {
        case QuestFileFormat::BIN_DAT:
          bin_contents.reset(new string(std::move(bin_data)));
          break;
        case QuestFileFormat::BIN_DAT_UNCOMPRESSED:
          bin_contents.reset(new string(prs_compress(bin_data)));
          break;
        case QuestFileFormat::BIN_DAT_GCI:
          bin_contents.reset(new string(decode_gci_data(bin_data)));
          break;
        case QuestFileFormat::BIN_DAT_VMS:
          bin_contents.reset(new string(decode_vms_data(bin_data)));
          break;
        case QuestFileFormat::BIN_DAT_DLQ:
          bin_contents.reset(new string(decode_dlq_data(bin_data)));
          break;
        case QuestFileFormat::QST: {
          auto result = decode_qst_data(bin_data);
          bin_contents.reset(new string(std::move(result.first)));
          dat_contents.reset(new string(std::move(result.second)));
          dat_cache.emplace(basename, dat_contents);
          break;
        }
        default:
          throw logic_error("invalid quest file format");
      }

      string dat_filename;
      if (!dat_contents && (version != QuestScriptVersion::GC_EP3)) {
        if (basename.size() < 2) {
          throw logic_error("basename too short for language trim");
        }

        // Look for dat file with the same basename as the bin file; if not
        // found, look for a dat file without the language suffix
        string dat_basename;
        for (size_t z = 0; z < 2; z++) {
          dat_basename = z ? basename.substr(0, basename.size() - 2) : basename;
          dat_filename = dat_basename;
          try {
            dat_contents = dat_cache.at(dat_basename);
            break;
          } catch (const out_of_range&) {
          }

          dat_filename = dat_basename + ".dat";
          string dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_path)) {
            dat_contents.reset(new string(load_file(dat_path)));
            break;
          }

          dat_filename = dat_basename + ".datd";
          dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_path)) {
            string decompressed = load_file(dat_path);
            dat_contents.reset(new string(prs_compress_optimal(decompressed.data(), decompressed.size())));
            break;
          }

          dat_filename = dat_basename + ".dat.gci";
          dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_path)) {
            dat_contents.reset(new string(decode_gci_data(load_file(dat_path))));
            break;
          }

          dat_filename = dat_basename + ".dat.vms";
          dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_path)) {
            dat_contents.reset(new string(decode_vms_data(load_file(dat_path))));
            break;
          }

          dat_filename = dat_basename + ".dat.dlq";
          dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_path)) {
            dat_contents.reset(new string(decode_dlq_data(load_file(dat_path))));
            break;
          }

          dat_filename = dat_basename + ".qst";
          dat_path = this->directory + "/" + dat_filename;
          if (isfile(dat_basename + ".qst")) {
            dat_contents.reset(new string(decode_dlq_data(load_file(dat_basename + ".dat.dlq"))));
            break;
          }
        }
        if (dat_contents) {
          dat_cache.emplace(dat_basename, dat_contents);
        } else {
          throw runtime_error("no dat file found");
        }
      }

      // Look for a JSON file with the same basename as the bin file; if not
      // found, look for a JSON file without the language suffix
      shared_ptr<const JSON> metadata_json;
      string json_filename;
      for (size_t z = 0; z < 3; z++) {
        string json_basename;
        if (z == 0) {
          json_filename = basename + ".json";
        } else if (z == 1) {
          json_filename = basename.substr(0, basename.size() - 2) + ".json"; // Strip off language prefix
        } else if (z == 2) {
          json_filename = basename.substr(0, basename.find('-')) + ".json"; // Look only at base token (e.g. "b88001")
        }

        try {
          metadata_json = metadata_json_cache.at(json_filename);
          break;
        } catch (const out_of_range&) {
        }

        string json_path = this->directory + "/" + json_filename;
        if (isfile(json_path)) {
          metadata_json.reset(new JSON(JSON::parse(load_file(json_path))));
          break;
        }
      }
      metadata_json_cache.emplace(json_filename, metadata_json);

      shared_ptr<BattleRules> battle_rules;
      ssize_t challenge_template_index = -1;
      if (metadata_json) {
        try {
          battle_rules.reset(new BattleRules(metadata_json->at("battle_rules")));
        } catch (const out_of_range&) {
        }
        try {
          challenge_template_index = metadata_json->at("challenge_template_index").as_int();
        } catch (const out_of_range&) {
        }
      }

      shared_ptr<VersionedQuest> vq(new VersionedQuest(
          quest_number, category_id, version, language, bin_contents, dat_contents, battle_rules, challenge_template_index));

      auto category_name = this->category_index->at(vq->category_id).name;

      string dat_str = dat_filename.empty() ? "" : (" with layout " + dat_filename);
      string battle_rules_str = battle_rules ? (" with battle rules from " + json_filename) : "";
      string challenge_template_str = (challenge_template_index >= 0) ? string_printf(" with challenge template index %zd", vq->challenge_template_index) : "";
      auto q_it = this->quests_by_number.find(vq->quest_number);
      if (q_it != this->quests_by_number.end()) {
        q_it->second->add_version(vq);
        static_game_data_log.info("(%s) Added %s %c version of quest %" PRIu32 " (%s)%s%s%s",
            bin_filename.c_str(),
            name_for_enum(vq->version),
            char_for_language_code(vq->language),
            vq->quest_number,
            vq->name.c_str(),
            dat_str.c_str(),
            battle_rules_str.c_str(),
            challenge_template_str.c_str());
      } else {
        this->quests_by_number.emplace(vq->quest_number, new Quest(vq));
        static_game_data_log.info("(%s) Created %s %c quest %" PRIu32 " (%s) (%s, %s (%" PRIu32 "), %s)%s%s%s",
            bin_filename.c_str(),
            name_for_enum(vq->version),
            char_for_language_code(vq->language),
            vq->quest_number,
            vq->name.c_str(),
            name_for_episode(vq->episode),
            category_name.c_str(),
            vq->category_id,
            vq->joinable ? "joinable" : "not joinable",
            dat_str.c_str(),
            battle_rules_str.c_str(),
            challenge_template_str.c_str());
      }
    } catch (const exception& e) {
      static_game_data_log.warning("(%s) Failed to index quest file: (%s)", bin_filename.c_str(), e.what());
    }
  }
}

shared_ptr<const Quest> QuestIndex::get(uint32_t quest_number) const {
  try {
    return this->quests_by_number.at(quest_number);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<const string> QuestIndex::get_gba(const string& name) const {
  try {
    return this->gba_file_contents.at(name);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

vector<shared_ptr<const Quest>> QuestIndex::filter(uint32_t category_id, QuestScriptVersion version, uint8_t language) const {
  vector<shared_ptr<const Quest>> ret;
  for (auto it : this->quests_by_number) {
    if (it.second->category_id == category_id && it.second->has_version(version, language)) {
      ret.emplace_back(it.second);
    }
  }
  return ret;
}

string encode_download_quest_data(const string& compressed_data, size_t decompressed_size, uint32_t encryption_seed) {
  // Download quest files are like normal (PRS-compressed) quest files, but they
  // are encrypted with PSO V2 encryption (even on V3 / PSO GC), and a small
  // header (PSODownloadQuestHeader) is prepended to the encrypted data.

  if (encryption_seed == 0) {
    encryption_seed = random_object<uint32_t>();
  }
  if (decompressed_size == 0) {
    decompressed_size = prs_decompress_size(compressed_data);
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

shared_ptr<VersionedQuest> VersionedQuest::create_download_quest() const {
  // The download flag needs to be set in the bin header, or else the client
  // will ignore it when scanning for download quests in an offline game. To set
  // this flag, we need to decompress the quest's .bin file, set the flag, then
  // recompress it again.

  // This function should not be used for Episode 3 quests (they should be sent
  // to the client as-is, without any encryption or other preprocessing)
  if (this->episode == Episode::EP3 || this->version == QuestScriptVersion::GC_EP3) {
    throw logic_error("Episode 3 quests cannot be converted to download quests");
  }

  string decompressed_bin = prs_decompress(*this->bin_contents);

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

  // Return a new VersionedQuest object with appropriately-processed .bin and
  // .dat file contents
  shared_ptr<VersionedQuest> dlq(new VersionedQuest(*this));
  dlq->bin_contents.reset(new string(encode_download_quest_data(compressed_bin, decompressed_bin.size())));
  dlq->dat_contents.reset(new string(encode_download_quest_data(*this->dat_contents)));
  dlq->is_dlq_encoded = true;
  return dlq;
}

string decode_gci_data(
    const string& data,
    ssize_t find_seed_num_threads,
    int64_t known_seed,
    bool skip_checksum) {
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
        return decrypt_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, known_seed, skip_checksum, false);

      } else if (header.embedded_seed != 0) {
        return decrypt_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, header.embedded_seed, skip_checksum, false);

      } else {
        if (find_seed_num_threads < 0) {
          throw runtime_error("file is encrypted");
        }
        if (find_seed_num_threads == 0) {
          find_seed_num_threads = thread::hardware_concurrency();
        }
        return find_seed_and_decrypt_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, skip_checksum, false, find_seed_num_threads);
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

  } else if (header.is_ep3()) {
    if (header.is_trial()) {
      if (known_seed >= 0) {
        return decrypt_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, known_seed, true, true);
      } else {
        if (find_seed_num_threads < 0) {
          throw runtime_error("file is encrypted");
        }
        if (find_seed_num_threads == 0) {
          find_seed_num_threads = thread::hardware_concurrency();
        }
        return find_seed_and_decrypt_download_quest_data_section<true>(
            r.getv(header.data_size), header.data_size, true, true, find_seed_num_threads);
      }

    } else {
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

      string decrypted = r.readx(header.data_size - 40);

      // For some reason, Sega decided not to encrypt Episode 3 quest files in the
      // same way as Episodes 1&2 quest files (see above). Instead, they just
      // wrote a fairly trivial XOR loop over the first 0x100 bytes, leaving the
      // remaining bytes completely unencrypted (but still compressed).
      size_t unscramble_size = min<size_t>(0x100, data.size());
      decrypt_trivial_gci_data(decrypted.data(), unscramble_size, 0);

      size_t decompressed_size = prs_decompress_size(decrypted);
      if (decompressed_size != sizeof(Episode3::MapDefinition)) {
        throw runtime_error(string_printf(
            "decompressed quest is 0x%zX bytes; expected 0x%zX bytes",
            decompressed_size, sizeof(Episode3::MapDefinition)));
      }
      return decrypted;
    }

  } else {
    throw runtime_error("unknown game name in GCI header");
  }
}

string decode_vms_data(
    const string& data,
    ssize_t find_seed_num_threads,
    int64_t known_seed,
    bool skip_checksum) {
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
    return decrypt_download_quest_data_section<false>(
        data_section, header.data_size, known_seed);

  } else {
    if (find_seed_num_threads < 0) {
      throw runtime_error("file is encrypted");
    }
    if (find_seed_num_threads == 0) {
      find_seed_num_threads = thread::hardware_concurrency();
    }
    return find_seed_and_decrypt_download_quest_data_section<false>(
        data_section, header.data_size, skip_checksum, 0, find_seed_num_threads);
  }
}

string decode_dlq_data(const string& data) {
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

template <typename HeaderT, typename OpenFileT>
static pair<string, string> decode_qst_data_t(const string& data) {
  StringReader r(data);

  string bin_contents;
  string dat_contents;
  string internal_bin_filename;
  string internal_dat_filename;
  uint32_t bin_file_size = 0;
  uint32_t dat_file_size = 0;
  QuestFileFormat subformat = QuestFileFormat::QST; // Stand-in for unknown
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
      if (subformat == QuestFileFormat::QST) {
        subformat = QuestFileFormat::BIN_DAT;
      } else if (subformat != QuestFileFormat::BIN_DAT) {
        throw runtime_error("QST file contains mixed download and non-download commands");
      }
    } else if (header.command == 0xA6 || header.command == 0xA7) {
      if (subformat == QuestFileFormat::QST) {
        subformat = QuestFileFormat::BIN_DAT_DLQ;
      } else if (subformat != QuestFileFormat::BIN_DAT_DLQ) {
        throw runtime_error("QST file contains mixed download and non-download commands");
      }
    }

    if (header.command == 0x44 || header.command == 0xA6) {
      if (header.size != sizeof(HeaderT) + sizeof(OpenFileT)) {
        throw runtime_error("qst open file command has incorrect size");
      }
      const auto& cmd = r.get<OpenFileT>();
      string internal_filename = cmd.filename.decode();

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
      // We have to allow larger commands here, because it seems some tools
      // encoded QST files with BB's extra 4 padding bytes included in the
      // command size.
      if (header.size < sizeof(HeaderT) + sizeof(S_WriteFile_13_A7)) {
        throw runtime_error("qst write file command has incorrect size");
      }
      const auto& cmd = r.get<S_WriteFile_13_A7>();
      string filename = cmd.filename.decode();

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

  if (subformat == QuestFileFormat::BIN_DAT_DLQ) {
    bin_contents = decode_dlq_data(bin_contents);
    dat_contents = decode_dlq_data(dat_contents);
  }

  return make_pair(bin_contents, dat_contents);
}

pair<string, string> decode_qst_data(const string& data) {
  // QST files start with an open file command, but the format differs depending
  // on the PSO version that the qst file is for. We can detect the format from
  // the first 4 bytes in the file:
  // - BB:    58 00 44 00 or 58 00 A6 00
  // - PC:    3C 00 44 ?? or 3C 00 A6 ??
  // - DC/V3: 44 ?? 3C 00 or A6 ?? 3C 00
  StringReader r(data);
  uint32_t signature = r.get_u32b();
  if (signature == 0x58004400 || signature == 0x5800A600) {
    return decode_qst_data_t<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(data);
  } else if ((signature & 0xFFFFFF00) == 0x3C004400 || (signature & 0xFFFFFF00) == 0x3C00A600) {
    return decode_qst_data_t<PSOCommandHeaderPC, S_OpenFile_PC_GC_44_A6>(data);
  } else if ((signature & 0xFF00FFFF) == 0x44003C00 || (signature & 0xFF00FFFF) == 0xA6003C00) {
    return decode_qst_data_t<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(data);
  } else if ((signature & 0xFF00FFFF) == 0x44005400 || (signature & 0xFF00FFFF) == 0xA6005400) {
    return decode_qst_data_t<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(data);
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
void add_open_file_command(StringWriter& w, const std::string& name, const std::string& filename, size_t file_size, bool is_download) {
  add_command_header<HeaderT>(w, is_download ? 0xA6 : 0x44, 0x00, sizeof(CmdT));
  CmdT cmd;
  cmd.name.assign_raw("PSO/" + name);
  cmd.filename.encode(filename);
  cmd.type = 0;
  cmd.file_size = file_size;
  // TODO: It'd be nice to have something like w.emplace(...) to avoid copying
  // the command structs into the StringWriter.
  w.put(cmd);
}

template <typename HeaderT>
void add_write_file_commands(
    StringWriter& w,
    const string& filename,
    const string& data,
    bool is_download,
    bool bb_alignment) {
  for (size_t z = 0; z < data.size(); z += 0x400) {
    size_t chunk_size = min<size_t>(data.size() - z, 0x400);
    add_command_header<HeaderT>(w, is_download ? 0xA7 : 0x13, z >> 10, sizeof(S_WriteFile_13_A7));
    S_WriteFile_13_A7 cmd;
    cmd.filename.encode(filename);
    memcpy(cmd.data.data(), &data[z], chunk_size);
    cmd.data_size = chunk_size;
    w.put(cmd);
    // On BB, the write file command size is a multiple of 4 but not a multiple
    // of 8; in QST format the implicit extra 4 bytes are apparently stored in
    // the file.
    if (bb_alignment) {
      w.put_u32(0);
    }
  }
}

string encode_qst_file(
    const string& bin_data,
    const string& dat_data,
    const string& name,
    uint32_t quest_number,
    QuestScriptVersion version,
    bool is_dlq_encoded) {
  StringWriter w;

  string bin_filename = string_printf("q%" PRIu32 ".bin", quest_number);
  string dat_filename = string_printf("q%" PRIu32 ".dat", quest_number);

  // Some tools expect both open file commands at the beginning, hence this
  // unfortunate abstraction-breaking.
  switch (version) {
    case QuestScriptVersion::DC_NTE:
    case QuestScriptVersion::DC_V1:
    case QuestScriptVersion::DC_V2:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, name, dat_filename, dat_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, bin_filename, bin_data, is_dlq_encoded, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, dat_filename, dat_data, is_dlq_encoded, false);
      break;
    case QuestScriptVersion::PC_V2:
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_GC_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_open_file_command<PSOCommandHeaderPC, S_OpenFile_PC_GC_44_A6>(w, name, dat_filename, dat_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderPC>(w, bin_filename, bin_data, is_dlq_encoded, false);
      add_write_file_commands<PSOCommandHeaderPC>(w, dat_filename, dat_data, is_dlq_encoded, false);
      break;
    case QuestScriptVersion::GC_NTE:
    case QuestScriptVersion::GC_V3:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(w, name, dat_filename, dat_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, bin_filename, bin_data, is_dlq_encoded, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, dat_filename, dat_data, is_dlq_encoded, false);
      break;
    case QuestScriptVersion::GC_EP3:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, bin_filename, bin_data, is_dlq_encoded, false);
      break;
    case QuestScriptVersion::XB_V3:
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_open_file_command<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(w, name, dat_filename, dat_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, bin_filename, bin_data, is_dlq_encoded, false);
      add_write_file_commands<PSOCommandHeaderDCV3>(w, dat_filename, dat_data, is_dlq_encoded, false);
      break;
    case QuestScriptVersion::BB_V4:
      add_open_file_command<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(w, name, bin_filename, bin_data.size(), is_dlq_encoded);
      add_open_file_command<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(w, name, dat_filename, dat_data.size(), is_dlq_encoded);
      add_write_file_commands<PSOCommandHeaderBB>(w, bin_filename, bin_data, is_dlq_encoded, true);
      add_write_file_commands<PSOCommandHeaderBB>(w, dat_filename, dat_data, is_dlq_encoded, true);
      break;
    default:
      throw logic_error("invalid game version");
  }

  return std::move(w.str());
}
