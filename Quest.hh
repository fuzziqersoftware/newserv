#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Version.hh"



enum class QuestCategory {
  Unknown = -1,
  Retrieval = 0,
  Extermination,
  Event,
  Shop,
  VR,
  Tower,
  GovernmentEpisode1,
  GovernmentEpisode2,
  GovernmentEpisode4,
  Download,
  Battle,
  Challenge,
  Solo,
  Episode3,
};

bool category_is_mode(QuestCategory category);
const char* name_for_category(QuestCategory category);



struct Quest {
  int64_t quest_id;
  QuestCategory category;
  uint8_t episode; // 0 = ep1, 1 = ep2, 2 = ep4, 0xFF = ep3
  bool is_dcv1;
  bool joinable;
  GameVersion version;
  std::string file_basename; // we append -<version>.<bin/dat> when reading
  std::u16string name;
  std::u16string short_description;
  std::u16string long_description;

  // these are populated when requested
  mutable std::shared_ptr<std::string> bin_contents_ptr;
  mutable std::shared_ptr<std::string> dat_contents_ptr;

  Quest(const std::string& file_basename);

  std::string bin_filename() const;
  std::string dat_filename() const;

  std::shared_ptr<const std::string> bin_contents() const;
  std::shared_ptr<const std::string> dat_contents() const;

  std::shared_ptr<Quest> create_download_quest(
      const std::string& file_basename) const;
};

struct QuestIndex {
  std::string directory;

  std::map<std::pair<GameVersion, uint64_t>, std::shared_ptr<Quest>> version_id_to_quest;
  std::map<std::pair<GameVersion, std::u16string>, std::shared_ptr<Quest>> version_name_to_quest;

  std::map<std::string, std::vector<std::shared_ptr<Quest>>> category_to_quests;

  std::map<std::string, std::shared_ptr<std::string>> gba_file_contents;

  QuestIndex(const char* directory);

  std::shared_ptr<const Quest> get(GameVersion version, uint32_t id) const;
  std::shared_ptr<const std::string> get_gba(const std::string& name) const;
  std::vector<std::shared_ptr<const Quest>> filter(GameVersion version,
    bool is_dcv1, QuestCategory category, uint8_t episode) const;
};

Quest create_download_quest(const Quest& src, size_t version);
