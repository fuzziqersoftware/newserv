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

QuestCategoryIndex::Category::Category(uint32_t category_id, const phosg::JSON& json)
    : category_id(category_id) {
  this->enabled_flags = json.get_int(0);
  this->directory_name = json.get_string(1);
  this->name = json.get_string(2);
  this->description = json.get_string(3);
}

QuestCategoryIndex::QuestCategoryIndex(const phosg::JSON& json) {
  uint32_t next_category_id = 1;
  for (const auto& it : json.as_list()) {
    this->categories.emplace_back(make_shared<Category>(next_category_id++, *it));
  }
}

shared_ptr<const QuestCategoryIndex::Category> QuestCategoryIndex::at(uint32_t category_id) const {
  return this->categories.at(category_id - 1);
}

// GCI decoding logic

template <bool BE>
struct PSOMemCardDLQFileEncryptedHeaderT {
  U32T<BE> round2_seed;
  // To compute checksum, set checksum to zero, then compute the CRC32 of the
  // entire data section, including this header struct (but not the unencrypted
  // header struct).
  U32T<BE> checksum;
  le_uint32_t decompressed_size;
  le_uint32_t round3_seed;
  // Data follows here.
} __packed__;
using PSOVMSDLQFileEncryptedHeader = PSOMemCardDLQFileEncryptedHeaderT<false>;
using PSOGCIDLQFileEncryptedHeader = PSOMemCardDLQFileEncryptedHeaderT<true>;
check_struct_size(PSOVMSDLQFileEncryptedHeader, 0x10);
check_struct_size(PSOGCIDLQFileEncryptedHeader, 0x10);

template <bool BE>
string decrypt_download_quest_data_section(
    const void* data_section, size_t size, uint32_t seed, bool skip_checksum = false, bool is_ep3_trial = false) {
  string decrypted = decrypt_data_section<BE>(data_section, size, seed);

  size_t orig_size = decrypted.size();
  decrypted.resize((decrypted.size() + 3) & (~3));

  // Note: Other PSO save files have the round 2 seed at the end of the data,
  // not at the beginning. Presumably they did this because the system,
  // character, and Guild Card files are a constant size, but download quest
  // files can vary in size.
  using HeaderT = PSOMemCardDLQFileEncryptedHeaderT<BE>;
  auto* header = reinterpret_cast<HeaderT*>(decrypted.data());
  PSOV2Encryption round2_crypt(header->round2_seed);
  round2_crypt.encrypt_t<BE>(
      decrypted.data() + 4, (decrypted.size() - 4));

  if (is_ep3_trial) {
    phosg::StringReader r(decrypted);
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
      throw runtime_error(phosg::string_printf(
          "decompressed size (%zu) does not match expected size (%zu)",
          decompressed_size, sizeof(Episode3::MapDefinitionTrial)));
    }
    return decrypted.substr(0x28);

  } else {
    if (header->decompressed_size & 0xFFF00000) {
      throw runtime_error(phosg::string_printf(
          "decompressed_size too large (%08" PRIX32 ")", header->decompressed_size.load()));
    }

    if (!skip_checksum) {
      uint32_t expected_crc = header->checksum;
      header->checksum = 0;
      uint32_t actual_crc = phosg::crc32(decrypted.data(), orig_size);
      header->checksum = expected_crc;
      if (expected_crc != actual_crc && expected_crc != phosg::bswap32(actual_crc)) {
        throw runtime_error(phosg::string_printf(
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
      throw runtime_error(phosg::string_printf(
          "decompressed size (%zu) does not match expected size (%zu)",
          decompressed_size, expected_decompressed_size));
    }

    return decrypted.substr(sizeof(HeaderT));
  }
}

string decrypt_vms_v1_data_section(const void* data_section, size_t size) {
  phosg::StringReader r(data_section, size);
  uint32_t expected_decompressed_size = r.get_u32l();
  uint32_t seed = r.get_u32l();

  string data = r.read(r.remaining());

  size_t orig_size = data.size();
  data.resize((orig_size + 3) & (~3));
  PSOV2Encryption(seed).decrypt(data.data(), data.size());
  data.resize(orig_size);

  size_t actual_decompressed_size = prs_decompress_size(data);
  if (actual_decompressed_size != expected_decompressed_size) {
    throw runtime_error(phosg::string_printf(
        "decompressed size (%zu) does not match size in header (%" PRId32 ")",
        actual_decompressed_size, expected_decompressed_size));
  }

  return data;
}

template <bool BE>
string find_seed_and_decrypt_download_quest_data_section(
    const void* data_section, size_t size, bool skip_checksum, bool is_ep3_trial, size_t num_threads) {
  mutex result_lock;
  string result;
  uint64_t result_seed = phosg::parallel_range_blocks<uint64_t>([&](uint64_t seed, size_t) {
    try {
      string ret = decrypt_download_quest_data_section<BE>(
          data_section, size, seed, skip_checksum, is_ep3_trial);
      lock_guard<mutex> g(result_lock);
      result = std::move(ret);
      return true;
    } catch (const runtime_error& e) {
      return false;
    }
  },
      0, 0x100000000, 0x1000, num_threads);

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
} __packed_ws__(PSODownloadQuestHeader, 8);

VersionedQuest::VersionedQuest(
    uint32_t quest_number,
    uint32_t category_id,
    Version version,
    uint8_t language,
    std::shared_ptr<const std::string> bin_contents,
    std::shared_ptr<const std::string> dat_contents,
    std::shared_ptr<const MapFile> map_file,
    std::shared_ptr<const std::string> pvr_contents,
    std::shared_ptr<const BattleRules> battle_rules,
    ssize_t challenge_template_index,
    uint8_t description_flag,
    std::shared_ptr<const IntegralExpression> available_expression,
    std::shared_ptr<const IntegralExpression> enabled_expression,
    bool allow_start_from_chat_command,
    bool force_joinable,
    int16_t lock_status_register)
    : quest_number(quest_number),
      category_id(category_id),
      episode(Episode::NONE),
      allow_start_from_chat_command(allow_start_from_chat_command),
      joinable(force_joinable),
      lock_status_register(lock_status_register),
      version(version),
      language(language),
      is_dlq_encoded(false),
      bin_contents(bin_contents),
      dat_contents(dat_contents),
      map_file(map_file),
      pvr_contents(pvr_contents),
      battle_rules(battle_rules),
      challenge_template_index(challenge_template_index),
      description_flag(description_flag),
      available_expression(available_expression),
      enabled_expression(enabled_expression) {

  auto bin_decompressed = prs_decompress(*this->bin_contents);

  switch (this->version) {
    case Version::DC_NTE: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderDCNTE)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderDCNTE*>(bin_decompressed.data());
      this->episode = Episode::EP1;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = phosg::fnv1a32(header, sizeof(header)) & 0xFFFF;
      }
      this->name = header->name.decode(this->language);
      break;
    }

    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderDC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderDC*>(bin_decompressed.data());
      this->episode = Episode::EP1;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
      break;
    }

    case Version::PC_NTE:
    case Version::PC_V2: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderPC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderPC*>(bin_decompressed.data());
      this->episode = Episode::EP1;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
      break;
    }

    case Version::GC_EP3_NTE:
    case Version::GC_EP3: {
      // Note: This codepath handles Episode 3 download quests, which are not
      // the same as Episode 3 quest scripts. The latter are only used offline
      // in story mode, but can be disassembled with disassemble_quest_script.
      // It's unfortunate that Version::GC_EP3 is used here for Episode 3
      // download quests (maps) and there for offline story mode scripts, but
      // it's probably not worth refactoring this logic, at least right now.
      if (bin_decompressed.size() != sizeof(Episode3::MapDefinition)) {
        throw invalid_argument("file is incorrect size");
      }
      auto* map = reinterpret_cast<const Episode3::MapDefinition*>(bin_decompressed.data());
      this->episode = Episode::EP3;
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = map->map_number;
      }
      this->name = map->name.decode(this->language);
      this->short_description = map->quest_name.decode(this->language);
      this->long_description = map->description.decode(this->language);
      break;
    }

    case Version::XB_V3:
    case Version::GC_NTE:
    case Version::GC_V3: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderGC)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderGC*>(bin_decompressed.data());
      this->episode = find_quest_episode_from_script(bin_decompressed.data(), bin_decompressed.size(), this->version);
      if (this->quest_number == 0xFFFFFFFF) {
        this->quest_number = header->quest_number;
      }
      this->name = header->name.decode(this->language);
      this->short_description = header->short_description.decode(this->language);
      this->long_description = header->long_description.decode(this->language);
      break;
    }

    case Version::BB_V4: {
      if (bin_decompressed.size() < sizeof(PSOQuestHeaderBB)) {
        throw invalid_argument("file is too small for header");
      }
      auto* header = reinterpret_cast<const PSOQuestHeaderBB*>(bin_decompressed.data());
      this->joinable |= header->joinable;
      this->episode = find_quest_episode_from_script(bin_decompressed.data(), bin_decompressed.size(), this->version);
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
    return phosg::string_printf("m%06" PRIu32 "p_e.bin", this->quest_number);
  } else {
    return phosg::string_printf("quest%" PRIu32 ".bin", this->quest_number);
  }
}

string VersionedQuest::dat_filename() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have .dat files");
  } else {
    return phosg::string_printf("quest%" PRIu32 ".dat", this->quest_number);
  }
}

string VersionedQuest::pvr_filename() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have .pvr files");
  } else {
    return phosg::string_printf("quest%" PRIu32 ".pvr", this->quest_number);
  }
}

string VersionedQuest::xb_filename() const {
  if (this->episode == Episode::EP3) {
    throw logic_error("Episode 3 quests do not have Xbox filenames");
  } else {
    return phosg::string_printf("quest%" PRIu32 "_%c.dat", this->quest_number, tolower(char_for_language_code(this->language)));
  }
}

string VersionedQuest::encode_qst() const {
  unordered_map<string, shared_ptr<const string>> files;
  files.emplace(phosg::string_printf("quest%" PRIu32 ".bin", this->quest_number), this->bin_contents);
  files.emplace(phosg::string_printf("quest%" PRIu32 ".dat", this->quest_number), this->dat_contents);
  if (this->pvr_contents) {
    files.emplace(phosg::string_printf("quest%" PRIu32 ".pvr", this->quest_number), this->pvr_contents);
  }
  string xb_filename = phosg::string_printf("quest%" PRIu32 "_%c.dat", quest_number, tolower(char_for_language_code(language)));
  return encode_qst_file(files, this->name, this->quest_number, xb_filename, this->version, this->is_dlq_encoded);
}

Quest::Quest(shared_ptr<const VersionedQuest> initial_version)
    : quest_number(initial_version->quest_number),
      category_id(initial_version->category_id),
      episode(initial_version->episode),
      allow_start_from_chat_command(initial_version->allow_start_from_chat_command),
      joinable(initial_version->joinable),
      lock_status_register(initial_version->lock_status_register),
      name(initial_version->name),
      supermap(nullptr),
      battle_rules(initial_version->battle_rules),
      challenge_template_index(initial_version->challenge_template_index),
      description_flag(initial_version->description_flag),
      available_expression(initial_version->available_expression),
      enabled_expression(initial_version->enabled_expression) {
  this->add_version(initial_version);
}

phosg::JSON Quest::json() const {
  auto versions_json = phosg::JSON::list();
  for (const auto& [_, vq] : this->versions) {
    versions_json.emplace_back(phosg::JSON::dict({
        {"Version", phosg::name_for_enum(vq->version)},
        {"Language", name_for_language_code(vq->language)},
        {"ShortDescription", vq->short_description},
        {"LongDescription", vq->long_description},
        {"BINFileSize", vq->bin_contents ? vq->bin_contents->size() : phosg::JSON(nullptr)},
        {"DATFileSize", vq->dat_contents ? vq->dat_contents->size() : phosg::JSON(nullptr)},
        {"PVRFileSize", vq->pvr_contents ? vq->pvr_contents->size() : phosg::JSON(nullptr)},
    }));
  }

  auto battle_rules_json = this->battle_rules ? this->battle_rules->json() : nullptr;
  auto challenge_template_index_json = (this->challenge_template_index >= 0)
      ? this->challenge_template_index
      : phosg::JSON(nullptr);
  return phosg::JSON::dict({
      {"Number", this->quest_number},
      {"CategoryID", this->category_id},
      {"Episode", name_for_episode(this->episode)},
      {"AllowStartFromChatCommand", this->allow_start_from_chat_command},
      {"Joinable", this->joinable},
      {"LockStatusRegister", (this->lock_status_register >= 0) ? this->lock_status_register : phosg::JSON(nullptr)},
      {"Name", this->name},
      {"BattleRules", std::move(battle_rules_json)},
      {"ChallengeTemplateIndex", std::move(challenge_template_index_json)},
      {"DescriptionFlag", this->description_flag},
      {"AvailableExpression", this->available_expression ? this->available_expression->str() : phosg::JSON(nullptr)},
      {"EnabledExpression", this->available_expression ? this->available_expression->str() : phosg::JSON(nullptr)},
      {"Versions", std::move(versions_json)},
  });
}

uint32_t Quest::versions_key(Version v, uint8_t language) {
  return (static_cast<uint32_t>(v) << 8) | language;
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
  if (this->allow_start_from_chat_command != vq->allow_start_from_chat_command) {
    throw runtime_error("quest version has a different allow_start_from_chat_command state");
  }
  if (this->joinable != vq->joinable) {
    throw runtime_error("quest version has a different joinability state");
  }
  if (this->lock_status_register != vq->lock_status_register) {
    throw runtime_error("quest version has a different lock status register");
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
  if (this->description_flag != vq->description_flag) {
    throw runtime_error("quest version has different description flag");
  }
  if (!this->available_expression != !vq->available_expression) {
    throw runtime_error("quest version has available expression but root quest does not, or vice versa");
  }
  if (this->available_expression && *this->available_expression != *vq->available_expression) {
    throw runtime_error("quest version has a different available expression");
  }
  if (!this->enabled_expression != !vq->enabled_expression) {
    throw runtime_error("quest version has enabled expression but root quest does not, or vice versa");
  }
  if (this->enabled_expression && *this->enabled_expression != *vq->enabled_expression) {
    throw runtime_error("quest version has a different enabled expression");
  }

  this->versions.emplace(this->versions_key(vq->version, vq->language), vq);
}

std::shared_ptr<const SuperMap> Quest::get_supermap(int64_t random_seed) const {
  if (this->supermap) {
    return this->supermap;
  }

  bool save_to_cache = true;
  bool any_map_file_present = false;
  array<shared_ptr<const MapFile>, NUM_VERSIONS> map_files;
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    auto vq = this->version(v, 1);
    if (vq && vq->map_file) {
      auto map_file = vq->map_file;
      if (map_file->has_random_sections()) {
        if (random_seed < 0) {
          return nullptr;
        }
        save_to_cache = false;
        map_file = map_file->materialize_random_sections(random_seed);
      }
      map_files.at(static_cast<size_t>(v)) = map_file;
      any_map_file_present = true;
    }
  }

  if (!any_map_file_present) {
    return nullptr;
  }

  auto supermap = make_shared<SuperMap>(this->episode, map_files);
  if (save_to_cache) {
    this->supermap = supermap;
  }
  static_game_data_log.info("Constructed %s supermap for quest %" PRIu32 " (%s)",
      save_to_cache ? "cacheable" : "temporary", this->quest_number, this->name.c_str());

  return supermap;
}

bool Quest::has_version(Version v, uint8_t language) const {
  return this->versions.count(this->versions_key(v, language));
}

bool Quest::has_version_any_language(Version v) const {
  uint32_t k = this->versions_key(v, 0);
  auto it = this->versions.lower_bound(k);
  return ((it != this->versions.end()) && ((it->first & 0xFF00) == k));
}

shared_ptr<const VersionedQuest> Quest::version(Version v, uint8_t language) const {
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
    shared_ptr<const QuestCategoryIndex> category_index,
    bool is_ep3)
    : directory(directory),
      category_index(category_index) {

  struct FileData {
    string filename;
    shared_ptr<const string> data;
  };
  struct DATFileData {
    string filename;
    shared_ptr<const string> data;
    shared_ptr<const MapFile> map_file;
  };
  map<string, FileData> bin_files;
  map<string, DATFileData> dat_files;
  map<string, FileData> pvr_files;
  map<string, FileData> json_files;
  map<string, uint32_t> categories;
  for (const auto& cat : this->category_index->categories) {
    // Don't index Ep3 download categories for non-Ep3 quest indexing, and vice
    // versa
    if (is_ep3 != cat->check_flag(QuestMenuType::EP3_DOWNLOAD)) {
      continue;
    }

    auto add_file = [&](map<string, FileData>& files, const string& basename, const string& filename, string&& value, bool check_chunk_size) {
      if (categories.emplace(basename, cat->category_id).first->second != cat->category_id) {
        throw runtime_error("file " + basename + " exists in multiple categories");
      }
      auto data_ptr = make_shared<string>(std::move(value));
      if (!files.emplace(basename, FileData{filename, data_ptr}).second) {
        throw runtime_error("file " + basename + " already exists");
      }
      // There is a bug in the client that prevents quests from loading properly
      // if any file's size is a multiple of 0x400. See the comments on the 13
      // command in CommandFormats.hh for more details.
      if (check_chunk_size && !(data_ptr->size() & 0x3FF)) {
        data_ptr->push_back(0x00);
      }
    };

    auto add_dat_file = [&](const string& basename, const string& filename, string&& value) {
      if (categories.emplace(basename, cat->category_id).first->second != cat->category_id) {
        throw runtime_error("file " + basename + " exists in multiple categories");
      }
      auto data_ptr = make_shared<string>(std::move(value));
      auto map_file = make_shared<MapFile>(make_shared<string>(prs_decompress(*data_ptr)));
      if (!dat_files.emplace(basename, DATFileData{filename, data_ptr, map_file}).second) {
        throw runtime_error("file " + basename + " already exists");
      }
      // There is a bug in the client that prevents quests from loading properly
      // if any file's size is a multiple of 0x400. See the comments on the 13
      // command in CommandFormats.hh for more details.
      if (!(data_ptr->size() & 0x3FF)) {
        data_ptr->push_back(0x00);
      }
    };

    string cat_path = directory + "/" + cat->directory_name;
    if (!phosg::isdir(cat_path)) {
      static_game_data_log.warning("Quest category directory %s is missing; skipping it", cat_path.c_str());
      continue;
    }
    for (string filename : phosg::list_directory_sorted(cat_path)) {
      if (filename == ".DS_Store") {
        continue;
      }

      string file_path = cat_path + "/" + filename;
      try {
        string orig_filename = filename;
        string file_data;
        if (phosg::ends_with(filename, ".gci")) {
          file_data = decode_gci_data(phosg::load_file(file_path));
          filename.resize(filename.size() - 4);
        } else if (phosg::ends_with(filename, ".vms")) {
          file_data = decode_vms_data(phosg::load_file(file_path));
          filename.resize(filename.size() - 4);
        } else if (phosg::ends_with(filename, ".dlq")) {
          file_data = decode_dlq_data(phosg::load_file(file_path));
          filename.resize(filename.size() - 4);
        } else if (phosg::ends_with(filename, ".txt")) {
          string include_dir = phosg::dirname(file_path);
          file_data = assemble_quest_script(phosg::load_file(file_path), include_dir);
          filename.resize(filename.size() - 4);
          if (phosg::ends_with(filename, ".bin")) {
            filename.push_back('d');
          }
        } else {
          file_data = phosg::load_file(file_path);
        }

        size_t dot_pos = filename.rfind('.');
        string file_basename;
        string extension;
        if (dot_pos != string::npos) {
          file_basename = phosg::tolower(filename.substr(0, dot_pos));
          extension = phosg::tolower(filename.substr(dot_pos + 1));
        } else {
          file_basename = phosg::tolower(filename);
        }

        if (extension == "json") {
          add_file(json_files, file_basename, orig_filename, std::move(file_data), false);
        } else if (extension == "bin" || extension == "mnm") {
          add_file(bin_files, file_basename, orig_filename, std::move(file_data), true);
        } else if (extension == "bind" || extension == "mnmd") {
          add_file(bin_files, file_basename, orig_filename, prs_compress_optimal(file_data), true);
        } else if (extension == "dat") {
          add_dat_file(file_basename, orig_filename, std::move(file_data));
        } else if (extension == "datd") {
          add_dat_file(file_basename, orig_filename, prs_compress_optimal(file_data));
        } else if (extension == "pvr") {
          add_file(pvr_files, file_basename, orig_filename, std::move(file_data), true);
        } else if (extension == "qst") {
          auto files = decode_qst_data(file_data);
          for (auto& it : files) {
            if (phosg::ends_with(it.first, ".bin")) {
              add_file(bin_files, file_basename, orig_filename, std::move(it.second), true);
            } else if (phosg::ends_with(it.first, ".dat")) {
              add_dat_file(file_basename, orig_filename, std::move(it.second));
            } else if (phosg::ends_with(it.first, ".pvr")) {
              add_file(pvr_files, file_basename, orig_filename, std::move(it.second), true);
            } else {
              throw runtime_error("qst file contains unsupported file type: " + it.first);
            }
          }
        }

      } catch (const exception& e) {
        static_game_data_log.warning("(%s) Failed to load quest file: (%s)", filename.c_str(), e.what());
      }
    }
  }

  // All quests have a bin file (even in Episode 3, though its format is
  // different), so we use bin_files as the primary list of all quests that
  // should be indexed
  for (auto& bin_it : bin_files) {
    const string& basename = bin_it.first;
    const auto* bin_filedata = &bin_it.second;

    try {
      // Quest .bin filenames are like K###-VERS-LANG.EXT, where:
      //   K can be any character (usually it's q)
      //   # = quest number (does not have to match the internal quest number)
      //   VERS = PSO version that the quest is for (dc, pc, gc, etc.)
      //   LANG = client language (j, e, g, f, s)
      //   EXT = file type (bin, bind, bin.dlq, qst, etc.)
      // EXT has already been stripped off by the time we get here, so we just
      // parse the remaining fields.
      string quest_number_token, version_token, language_token;
      {
        vector<string> filename_tokens = phosg::split(basename, '-');
        if (filename_tokens.size() != 3) {
          throw invalid_argument("incorrect filename format");
        }
        quest_number_token = std::move(filename_tokens[0]);
        version_token = std::move(filename_tokens[1]);
        language_token = std::move(filename_tokens[2]);
      }

      uint32_t category_id = categories.at(basename);

      // Get the number from the first token
      if (quest_number_token.empty()) {
        throw runtime_error("quest number token is missing");
      }
      uint32_t quest_number = strtoull(quest_number_token.c_str() + 1, nullptr, 10);

      // Get the version from the second token
      static const unordered_map<string, Version> name_to_version({
          {"dn", Version::DC_NTE},
          {"dp", Version::DC_11_2000},
          {"d1", Version::DC_V1},
          {"dc", Version::DC_V2},
          {"pcn", Version::PC_NTE},
          {"pc", Version::PC_V2},
          {"gcn", Version::GC_NTE},
          {"gc", Version::GC_V3},
          {"gc3t", Version::GC_EP3_NTE},
          {"gc3", Version::GC_EP3},
          {"xb", Version::XB_V3},
          {"bb", Version::BB_V4},
      });
      auto version = name_to_version.at(version_token);

      // Get the language from the last token
      if (language_token.size() != 1) {
        throw runtime_error("language token is not a single character");
      }
      uint8_t language = language_code_for_char(language_token[0]);

      // Find the corresponding dat and pvr files
      const DATFileData* dat_filedata = nullptr;
      const FileData* pvr_filedata = nullptr;
      if (!::is_ep3(version)) {
        // Look for dat and pvr files with the same basename as the bin file; if
        // not found, look for them without the language suffix
        try {
          dat_filedata = &dat_files.at(basename);
        } catch (const out_of_range&) {
          try {
            dat_filedata = &dat_files.at(quest_number_token + "-" + version_token);
          } catch (const out_of_range&) {
            throw runtime_error("no dat file found for bin file " + basename);
          }
        }
        try {
          pvr_filedata = &pvr_files.at(basename);
        } catch (const out_of_range&) {
          try {
            pvr_filedata = &pvr_files.at(quest_number_token + "-" + version_token);
          } catch (const out_of_range&) {
            // pvr files aren't required (and most quests do not have them), so
            // don't fail if it's missing
          }
        }
      }

      // Load the quest's metadata phosg::JSON file, if it exists
      const FileData* json_filedata = nullptr;
      shared_ptr<BattleRules> battle_rules;
      ssize_t challenge_template_index = -1;
      uint8_t description_flag = 0;
      shared_ptr<const IntegralExpression> available_expression;
      shared_ptr<const IntegralExpression> enabled_expression;
      bool allow_start_from_chat_command = false;
      bool force_joinable = false;
      int16_t lock_status_register = -1;
      try {
        json_filedata = &json_files.at(basename);
      } catch (const out_of_range&) {
        try {
          json_filedata = &json_files.at(quest_number_token + "-" + version_token);
        } catch (const out_of_range&) {
          try {
            json_filedata = &json_files.at(quest_number_token);
          } catch (const out_of_range&) {
          }
        }
      }
      if (json_filedata) {
        auto metadata_json = phosg::JSON::parse(*json_filedata->data);
        try {
          battle_rules = make_shared<BattleRules>(metadata_json.at("BattleRules"));
        } catch (const out_of_range&) {
        }
        try {
          challenge_template_index = metadata_json.at("ChallengeTemplateIndex").as_int();
        } catch (const out_of_range&) {
        }
        try {
          description_flag = metadata_json.at("DescriptionFlag").as_int();
        } catch (const out_of_range&) {
        }
        try {
          available_expression = make_shared<IntegralExpression>(metadata_json.get_string("AvailableIf"));
        } catch (const out_of_range&) {
        }
        try {
          enabled_expression = make_shared<IntegralExpression>(metadata_json.get_string("EnabledIf"));
        } catch (const out_of_range&) {
        }
        try {
          allow_start_from_chat_command = metadata_json.get_bool("AllowStartFromChatCommand");
        } catch (const out_of_range&) {
        }
        try {
          force_joinable = metadata_json.get_bool("Joinable");
        } catch (const out_of_range&) {
        }
        try {
          lock_status_register = metadata_json.get_int("LockStatusRegister");
        } catch (const out_of_range&) {
        }
      }

      auto vq = make_shared<VersionedQuest>(
          quest_number,
          category_id,
          version,
          language,
          bin_filedata->data,
          dat_filedata ? dat_filedata->data : nullptr,
          dat_filedata ? dat_filedata->map_file : nullptr,
          pvr_filedata ? pvr_filedata->data : nullptr,
          battle_rules,
          challenge_template_index,
          description_flag,
          available_expression,
          enabled_expression,
          allow_start_from_chat_command,
          force_joinable,
          lock_status_register);

      auto category_name = this->category_index->at(vq->category_id)->name;
      string filenames_str = bin_filedata->filename;
      if (dat_filedata) {
        filenames_str += phosg::string_printf("/%s", dat_filedata->filename.c_str());
      }
      if (pvr_filedata) {
        filenames_str += phosg::string_printf("/%s", pvr_filedata->filename.c_str());
      }
      if (json_filedata) {
        filenames_str += phosg::string_printf("/%s", json_filedata->filename.c_str());
      }
      auto q_it = this->quests_by_number.find(vq->quest_number);
      if (q_it != this->quests_by_number.end()) {
        q_it->second->add_version(vq);
        static_game_data_log.info("(%s) Added %s %c version of quest %" PRIu32 " (%s)",
            filenames_str.c_str(),
            phosg::name_for_enum(vq->version),
            char_for_language_code(vq->language),
            vq->quest_number,
            vq->name.c_str());
      } else {
        auto q = make_shared<Quest>(vq);
        this->quests_by_number.emplace(vq->quest_number, q);
        this->quests_by_name.emplace(vq->name, q);
        this->quests_by_category_id_and_number[q->category_id].emplace(vq->quest_number, q);
        static_game_data_log.info("(%s) Created %s %c quest %" PRIu32 " (%s) (%s, %s (%" PRIu32 "), %s)",
            filenames_str.c_str(),
            phosg::name_for_enum(vq->version),
            char_for_language_code(vq->language),
            vq->quest_number,
            vq->name.c_str(),
            name_for_episode(vq->episode),
            category_name.c_str(),
            vq->category_id,
            vq->joinable ? "joinable" : "not joinable");
      }
    } catch (const exception& e) {
      static_game_data_log.warning("(%s) Failed to index quest file: (%s)", basename.c_str(), e.what());
    }
  }

  // Create supermaps for all quests that need them (all non-Ep3 quests)
  for (const auto& it : this->quests_by_number) {
    it.second->get_supermap(-1);
  }
}

phosg::JSON QuestIndex::json() const {
  auto categories_json = phosg::JSON::dict();
  for (const auto& cat : this->category_index->categories) {
    auto dict = phosg::JSON::dict({
        {"CategoryID", cat->category_id},
        {"Flags", cat->enabled_flags},
        {"DirectoryName", cat->directory_name},
        {"Name", cat->name},
        {"Description", cat->description},
    });
    categories_json.emplace(cat->name, std::move(dict));
  }

  auto quests_json = phosg::JSON::list();
  for (const auto& [_, q] : this->quests_by_number) {
    quests_json.emplace_back(q->json());
  }

  return phosg::JSON::dict({
      {"Directory", this->directory},
      {"Categories", std::move(categories_json)},
      {"Quests", std::move(quests_json)},
  });
  // std::map<uint32_t, std::shared_ptr<Quest>> quests_by_number;
  // std::map<std::string, std::shared_ptr<Quest>> quests_by_name;
  // std::map<uint32_t, std::map<uint32_t, std::shared_ptr<Quest>>> quests_by_category_id_and_number;
}

shared_ptr<const Quest> QuestIndex::get(uint32_t quest_number) const {
  try {
    return this->quests_by_number.at(quest_number);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<const Quest> QuestIndex::get(const std::string& name) const {
  try {
    return this->quests_by_name.at(name);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

vector<shared_ptr<const QuestCategoryIndex::Category>> QuestIndex::categories(
    QuestMenuType menu_type,
    Episode episode,
    uint16_t version_flags,
    IncludeCondition include_condition) const {
  vector<shared_ptr<const QuestCategoryIndex::Category>> ret;
  for (const auto& cat : this->category_index->categories) {
    if (cat->check_flag(menu_type) && !this->filter(episode, version_flags, cat->category_id, include_condition, 1).empty()) {
      ret.emplace_back(cat);
    }
  }
  return ret;
}

vector<pair<QuestIndex::IncludeState, shared_ptr<const Quest>>> QuestIndex::filter(
    Episode episode,
    uint16_t version_flags,
    uint32_t category_id,
    IncludeCondition include_condition,
    size_t limit) const {
  auto cat = this->category_index->at(category_id);
  Episode effective_episode = cat->enable_episode_filter() ? episode : Episode::NONE;

  vector<pair<IncludeState, shared_ptr<const Quest>>> ret;
  auto category_it = this->quests_by_category_id_and_number.find(category_id);
  if (category_it == this->quests_by_category_id_and_number.end()) {
    return ret;
  }
  for (auto it : category_it->second) {
    if ((effective_episode != Episode::NONE) && (it.second->episode != effective_episode)) {
      continue;
    }
    bool all_required_versions_present = true;
    for (size_t v_s = 0; v_s < NUM_VERSIONS; v_s++) {
      if ((version_flags & (1 << v_s)) && !it.second->has_version_any_language(static_cast<Version>(v_s))) {
        all_required_versions_present = false;
        break;
      }
    }
    if (!all_required_versions_present) {
      continue;
    }
    IncludeState state = include_condition ? include_condition(it.second) : IncludeState::AVAILABLE;
    if (state == IncludeState::HIDDEN) {
      continue;
    }
    ret.emplace_back(make_pair(state, it.second));
    if (limit && (ret.size() >= limit)) {
      break;
    }
  }
  return ret;
}

string encode_download_quest_data(const string& compressed_data, size_t decompressed_size, uint32_t encryption_seed) {
  // Download quest files are like normal (PRS-compressed) quest files, but they
  // are encrypted with PSO V2 encryption (even on V3 / PSO GC), and a small
  // header (PSODownloadQuestHeader) is prepended to the encrypted data.

  if (encryption_seed == 0) {
    encryption_seed = phosg::random_object<uint32_t>();
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

shared_ptr<VersionedQuest> VersionedQuest::create_download_quest(uint8_t override_language) const {
  // The download flag needs to be set in the bin header, or else the client
  // will ignore it when scanning for download quests in an offline game. To set
  // this flag, we need to decompress the quest's .bin file, set the flag, then
  // recompress it again.

  // This function should not be used for Episode 3 quests (they should be sent
  // to the client as-is, without any encryption or other preprocessing)
  if (this->episode == Episode::EP3 || is_ep3(this->version)) {
    throw logic_error("Episode 3 quests cannot be converted to download quests");
  }

  string decompressed_bin = prs_decompress(*this->bin_contents);

  void* data_ptr = decompressed_bin.data();
  switch (this->version) {
    case Version::DC_NTE:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderDCNTE)) {
        throw runtime_error("bin file is too small for header");
      }
      // There's no known language field in this version, so we don't write
      // anything here
      break;
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderDC)) {
        throw runtime_error("bin file is too small for header");
      }
      if (override_language != 0xFF) {
        reinterpret_cast<PSOQuestHeaderDC*>(data_ptr)->language = override_language;
      }
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderPC)) {
        throw runtime_error("bin file is too small for header");
      }
      if (override_language != 0xFF) {
        reinterpret_cast<PSOQuestHeaderPC*>(data_ptr)->language = override_language;
      }
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::XB_V3:
      if (decompressed_bin.size() < sizeof(PSOQuestHeaderGC)) {
        throw runtime_error("bin file is too small for header");
      }
      if (override_language != 0xFF) {
        reinterpret_cast<PSOQuestHeaderGC*>(data_ptr)->language = override_language;
      }
      break;
    case Version::BB_V4:
      throw invalid_argument("PSOBB does not support download quests");
    default:
      throw invalid_argument("unknown game version");
  }

  string compressed_bin = prs_compress(decompressed_bin);

  // Return a new VersionedQuest object with appropriately-processed .bin and
  // .dat file contents
  auto dlq = make_shared<VersionedQuest>(*this);
  dlq->bin_contents = make_shared<string>(encode_download_quest_data(compressed_bin, decompressed_bin.size()));
  dlq->dat_contents = make_shared<string>(encode_download_quest_data(*this->dat_contents));
  dlq->pvr_contents = this->pvr_contents;
  dlq->is_dlq_encoded = true;
  return dlq;
}

string decode_gci_data(
    const string& data,
    ssize_t find_seed_num_threads,
    int64_t known_seed,
    bool skip_checksum) {
  phosg::StringReader r(data);
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
        throw runtime_error(phosg::string_printf(
            "GCI decompressed data is smaller than expected size (have 0x%zX bytes, expected 0x%zX bytes)",
            decompressed_bytes, expected_decompressed_bytes));
      }

      return compressed_data;
    }

  } else if (header.is_ep3()) {
    if (header.is_nte()) {
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
        throw runtime_error(phosg::string_printf(
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
  phosg::StringReader r(data);
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
  phosg::StringReader r(data);
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
static unordered_map<string, string> decode_qst_data_t(const string& data) {
  phosg::StringReader r(data);

  unordered_map<string, string> files;
  unordered_map<string, size_t> file_remaining_bytes;
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

      if (!files.emplace(internal_filename, "").second) {
        throw runtime_error("qst opens the same file multiple times: " + internal_filename);
      }
      if (!file_remaining_bytes.emplace(internal_filename, cmd.file_size).second) {
        throw runtime_error("qst opens the same file multiple times: " + internal_filename);
      }

    } else if (header.command == 0x13 || header.command == 0xA7) {
      // We have to allow larger commands here, because it seems some tools
      // encoded QST files with BB's extra 4 padding bytes included in the
      // command size.
      if (header.size < sizeof(HeaderT) + sizeof(S_WriteFile_13_A7)) {
        throw runtime_error("qst write file command has incorrect size");
      }
      const auto& cmd = r.get<S_WriteFile_13_A7>();
      if (cmd.data_size > 0x400) {
        throw runtime_error("qst contains invalid write command");
      }
      string filename = cmd.filename.decode();

      string& file_data = files.at(filename);
      size_t& remaining_bytes = file_remaining_bytes.at(filename);

      if (file_data.size() & 0x3FF) {
        throw runtime_error("qst contains uneven chunks out of order");
      }
      if (header.flag != file_data.size() / 0x400) {
        throw runtime_error("qst contains chunks out of order");
      }
      file_data.append(reinterpret_cast<const char*>(cmd.data.data()), cmd.data_size);
      remaining_bytes -= cmd.data_size;

    } else {
      throw runtime_error("invalid command in qst file");
    }
  }

  for (const auto& it : file_remaining_bytes) {
    if (it.second) {
      throw runtime_error(phosg::string_printf("expected %zu (0x%zX) more bytes for file %s", it.second, it.second, it.first.c_str()));
    }
  }

  if (subformat == QuestFileFormat::BIN_DAT_DLQ) {
    for (auto& it : files) {
      it.second = decode_dlq_data(it.second);
    }
  }

  return files;
}

unordered_map<string, string> decode_qst_data(const string& data) {
  // QST files start with an open file command, but the format differs depending
  // on the PSO version that the qst file is for. We can detect the format from
  // the first 4 bytes in the file:
  // - BB:    58 00 44 00 or 58 00 A6 00
  // - PC:    3C 00 44 ?? or 3C 00 A6 ??
  // - DC/GC: 44 ?? 3C 00 or A6 ?? 3C 00
  // - XB:    44 ?? 54 00 or A6 ?? 54 00
  phosg::StringReader r(data);
  uint32_t signature = r.get_u32b();
  if ((signature == 0x58004400) || (signature == 0x5800A600)) {
    return decode_qst_data_t<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(data);
  } else if (((signature & 0xFFFFFF00) == 0x3C004400) || ((signature & 0xFFFFFF00) == 0x3C00A600)) {
    return decode_qst_data_t<PSOCommandHeaderPC, S_OpenFile_PC_GC_44_A6>(data);
  } else if (((signature & 0xFF00FFFF) == 0x44003C00) || ((signature & 0xFF00FFFF) == 0xA6003C00)) {
    // In PSO DC, the type field is only one byte, but in V3 it's two bytes and
    // the filename was shifted over by one byte. To detect this, we check if
    // the V3 type field has a reasonable value, and if not, we assume the file
    // is for PSO DC.
    if (r.pget_u16l(sizeof(PSOCommandHeaderDCV3) + offsetof(S_OpenFile_PC_GC_44_A6, type)) > 3) {
      return decode_qst_data_t<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(data);
    } else {
      return decode_qst_data_t<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(data);
    }
  } else if (((signature & 0xFF00FFFF) == 0x44005400) || ((signature & 0xFF00FFFF) == 0xA6005400)) {
    return decode_qst_data_t<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(data);
  } else {
    throw runtime_error("invalid qst file format");
  }
}

template <typename HeaderT>
void add_command_header(phosg::StringWriter& w, uint8_t command, uint8_t flag, uint16_t size) {
  HeaderT header;
  header.command = command;
  header.flag = flag;
  header.size = sizeof(HeaderT) + size;
  w.put(header);
}

template <typename HeaderT, typename CmdT>
void add_open_file_command_t(
    phosg::StringWriter& w,
    const std::string& name,
    const std::string& filename,
    const std::string&,
    uint32_t,
    size_t file_size,
    bool is_download) {
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

template <>
void add_open_file_command_t<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(
    phosg::StringWriter& w,
    const std::string& name,
    const std::string& filename,
    const std::string& xb_filename,
    uint32_t quest_number,
    size_t file_size,
    bool is_download) {
  add_command_header<PSOCommandHeaderDCV3>(w, is_download ? 0xA6 : 0x44, 0x00, sizeof(S_OpenFile_XB_44_A6));
  S_OpenFile_XB_44_A6 cmd;
  cmd.name.assign_raw("PSO/" + name);
  cmd.filename.encode(filename);
  cmd.type = 0;
  cmd.file_size = file_size;
  cmd.xb_filename.encode(xb_filename);
  cmd.content_meta = 0x30000000 | quest_number;
  w.put(cmd);
}

template <typename HeaderT>
void add_write_file_commands_t(
    phosg::StringWriter& w,
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
    const unordered_map<string, shared_ptr<const string>>& files,
    const string& name,
    uint32_t quest_number,
    const string& xb_filename,
    Version version,
    bool is_dlq_encoded) {
  phosg::StringWriter w;

  // Some tools expect both open file commands at the beginning, hence this
  // unfortunate abstraction-breaking.
  switch (version) {
    case Version::DC_NTE: // DC NTE doesn't support quests, but we support encoding QST files anyway
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      for (const auto& it : files) {
        add_open_file_command_t<PSOCommandHeaderDCV3, S_OpenFile_DC_44_A6>(w, name, it.first, xb_filename, quest_number, it.second->size(), is_dlq_encoded);
      }
      for (const auto& it : files) {
        add_write_file_commands_t<PSOCommandHeaderDCV3>(w, it.first, *it.second, is_dlq_encoded, false);
      }
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      for (const auto& it : files) {
        add_open_file_command_t<PSOCommandHeaderPC, S_OpenFile_PC_GC_44_A6>(w, name, it.first, xb_filename, quest_number, it.second->size(), is_dlq_encoded);
      }
      for (const auto& it : files) {
        add_write_file_commands_t<PSOCommandHeaderPC>(w, it.first, *it.second, is_dlq_encoded, false);
      }
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      for (const auto& it : files) {
        add_open_file_command_t<PSOCommandHeaderDCV3, S_OpenFile_PC_GC_44_A6>(w, name, it.first, xb_filename, quest_number, it.second->size(), is_dlq_encoded);
      }
      for (const auto& it : files) {
        add_write_file_commands_t<PSOCommandHeaderDCV3>(w, it.first, *it.second, is_dlq_encoded, false);
      }
      break;
    case Version::XB_V3:
      for (const auto& it : files) {
        add_open_file_command_t<PSOCommandHeaderDCV3, S_OpenFile_XB_44_A6>(w, name, it.first, xb_filename, quest_number, it.second->size(), is_dlq_encoded);
      }
      for (const auto& it : files) {
        add_write_file_commands_t<PSOCommandHeaderDCV3>(w, it.first, *it.second, is_dlq_encoded, false);
      }
      break;
    case Version::BB_V4:
      for (const auto& it : files) {
        add_open_file_command_t<PSOCommandHeaderBB, S_OpenFile_BB_44_A6>(w, name, it.first, xb_filename, quest_number, it.second->size(), is_dlq_encoded);
      }
      for (const auto& it : files) {
        add_write_file_commands_t<PSOCommandHeaderBB>(w, it.first, *it.second, is_dlq_encoded, true);
      }
      break;
    default:
      throw logic_error("invalid game version");
  }

  return std::move(w.str());
}
