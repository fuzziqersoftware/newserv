#include "Quest.hh"

#include <string>
#include <unordered_map>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Compression.hh"
#include "Text.hh"

using namespace std;



bool category_is_mode(QuestCategory category) {
  return (category == QuestCategory::Battle) ||
         (category == QuestCategory::Challenge) ||
         (category == QuestCategory::Episode3);
}

const char* name_for_category(QuestCategory category) {
  switch (category) {
    case QuestCategory::Retrieval:
      return "Retrieval";
    case QuestCategory::Extermination:
      return "Extermination";
    case QuestCategory::Event:
      return "Event";
    case QuestCategory::Shop:
      return "Shop";
    case QuestCategory::VR:
      return "VR";
    case QuestCategory::Tower:
      return "Tower";
    case QuestCategory::Government:
      return "Government";
    case QuestCategory::Download:
      return "Download";
    case QuestCategory::Battle:
      return "Battle";
    case QuestCategory::Challenge:
      return "Challenge";
    case QuestCategory::Solo:
      return "Solo";
    case QuestCategory::Episode3:
      return "Episode3";
    default:
      return "Unknown";
  }
}



struct PSOQuestHeaderDC { // same for dc v1 and v2, thankfully
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint8_t unknown1;
  uint8_t unknown2;
  uint16_t quest_number; // 0xFFFF for challenge quests
  char name[0x20];
  char short_description[0x80];
  char long_description[0x120];
};

struct PSOQuestHeaderPC {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint8_t unknown1;
  uint8_t unknown2;
  uint16_t quest_number; // 0xFFFF for challenge quests
  char16_t name[0x20];
  char16_t short_description[0x80];
  char16_t long_description[0x120];
};

struct PSOQuestHeaderGC {
  uint32_t start_offset;
  uint32_t unknown_offset1;
  uint32_t size;
  uint32_t unused;
  uint16_t unknown1;
  uint8_t quest_number;
  uint8_t episode; // 1 = ep2. apparently some quests have 0xFF here, which means ep1 (?)
  char name[0x20];
  char short_description[0x80];
  char long_description[0x120];
};

struct PSOQuestHeaderGCEpisode3 {
  // there's actually a lot of other important stuff in here but I'm lazy. it
  // looks like map data, cutscene data, and maybe special cards used during
  // the quest
  uint8_t unused[0x1DF0];
  char name[0x14];
  char location[0x14];
  char location2[0x3C];
  char description[0x190];
  uint8_t unused2[0x3A34];
};

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
  char16_t name[0x20];
  char16_t short_description[0x80];
  char16_t long_description[0x120];
};



Quest::Quest(const string& bin_filename) : quest_id(-1),
    category(QuestCategory::Unknown), episode(0), is_dcv1(false),
    joinable(false),
    file_basename(bin_filename.substr(0, bin_filename.size() - 4)) {

  string bin_basename;
  {
    size_t slash_pos = bin_filename.rfind('/');
    if (slash_pos != string::npos) {
      bin_basename = bin_filename.substr(slash_pos + 1);
    } else {
      bin_basename = bin_filename;
    }
  }

  // quest filenames are like:
  // b###-VV.bin for battle mode
  // c###-VV.bin for challenge mode
  // e###-gc3.bin for episode 3
  // q###-CAT-VV.bin for normal quests

  if (bin_basename.empty()) {
    throw invalid_argument("empty filename");
  }

  if (bin_basename[0] == 'b') {
    this->category = QuestCategory::Battle;
  } else if (bin_basename[0] == 'c') {
    this->category = QuestCategory::Challenge;
  } else if (bin_basename[0] == 'e') {
    this->category = QuestCategory::Episode3;
  } else if (bin_basename[0] != 'q') {
    throw invalid_argument("filename does not indicate mode");
  }

  // if the quest category is still unknown, expect 3 tokens (one of them will
  // tell us the category)
  vector<string> tokens = split(bin_basename, '-');
  if (tokens.size() != (2 + (this->category == QuestCategory::Unknown))) {
    throw invalid_argument("incorrect filename format");
  }

  // parse the number out of the first token
  this->quest_id = strtoull(tokens[0].c_str() + 1, NULL, 10);

  // get the category from the second token if needed
  if (this->category == QuestCategory::Unknown) {
    static const unordered_map<std::string, QuestCategory> name_to_category({
      {"ret", QuestCategory::Retrieval},
      {"ext", QuestCategory::Extermination},
      {"evt", QuestCategory::Event},
      {"shp", QuestCategory::Shop},
      {"vr",  QuestCategory::VR},
      {"twr", QuestCategory::Tower},
      {"gov", QuestCategory::Government},
      {"dl",  QuestCategory::Download},
      {"1p",  QuestCategory::Solo},
    });
    this->category = name_to_category.at(tokens[1]);
    tokens.erase(tokens.begin() + 1);
  }

  static const unordered_map<std::string, GameVersion> name_to_version({
    {"dc1.bin", GameVersion::DC},
    {"dc.bin",  GameVersion::DC},
    {"pc.bin",  GameVersion::PC},
    {"gc.bin",  GameVersion::GC},
    {"gc3.bin", GameVersion::GC},
    {"bb.bin",  GameVersion::BB},
  });
  this->version = name_to_version.at(tokens[1]);

  // the rest of the information needs to be fetched from the .bin file's
  // contents

  auto bin_compressed = this->bin_contents();
  auto bin_decompressed = prs_decompress(*bin_compressed);

  switch (this->version) {
    case GameVersion::Patch:
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
      this->is_dcv1 = (tokens[1] == "dc1.bin");
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

    case GameVersion::GC: {
      if (this->category == QuestCategory::Episode3) {
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
      break;

    }
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
    this->bin_contents_ptr.reset(new string(load_file(this->file_basename + ".bin")));
  }
  return this->bin_contents_ptr;
}

shared_ptr<const string> Quest::dat_contents() const {
  if (!this->bin_contents_ptr) {
    this->bin_contents_ptr.reset(new string(load_file(this->file_basename + ".dat")));
  }
  return this->bin_contents_ptr;
}



QuestIndex::QuestIndex(const char* directory) : directory(directory) {
  for (const auto& filename : list_directory(this->directory)) {
    string full_path = this->directory + "/" + filename;

    if (ends_with(filename, ".gba")) {
      this->gba_file_contents.emplace(filename, new string(load_file(full_path)));
      continue;
    }

    if (ends_with(filename, ".bin")) {
      try {
        shared_ptr<Quest> q(new Quest(full_path));
        this->version_id_to_quest.emplace(make_pair(q->version, q->quest_id), q);
        this->version_name_to_quest.emplace(make_pair(q->version, q->name), q);
        string ascii_name = encode_sjis(q->name);
        log(INFO, "indexed quest %s (%s-%" PRId64 ", %s, episode=%hhu, joinable=%s, dcv1=%s)",
            ascii_name.c_str(), name_for_version(q->version), q->quest_id,
            name_for_category(q->category), q->episode,
            q->joinable ? "true" : "false", q->is_dcv1 ? "true" : "false");
      } catch (const exception& e) {
        log(WARNING, "failed to parse quest file %s (%s)", filename.c_str(), e.what());
      }
    }
  }
}

shared_ptr<const Quest> QuestIndex::get(GameVersion version,
    uint32_t id) const {
  return this->version_id_to_quest.at(make_pair(version, id));
}

shared_ptr<const string> QuestIndex::get_gba(const string& name) const {
  return this->gba_file_contents.at(name);
}

vector<shared_ptr<const Quest>> QuestIndex::filter(GameVersion version,
    bool is_dcv1, QuestCategory category, uint8_t episode) const {
  auto it = this->version_id_to_quest.lower_bound(make_pair(version, 0));
  auto end_it = this->version_id_to_quest.upper_bound(make_pair(version, 0xFFFFFFFF));

  vector<shared_ptr<const Quest>> ret;
  for (; it != end_it; it++) {
    shared_ptr<const Quest> q = it->second;
    if ((q->is_dcv1 != is_dcv1) || (q->category != category)) {
      continue;
    }

    // only check episode and solo if the category isn't a mode (that is, ignore
    // episode if querying for battle/challange/solo quests)
    if (!category_is_mode(category) && ((q->episode != episode))) {
      continue;
    }

    ret.emplace_back(q);
  }

  return ret;
}
