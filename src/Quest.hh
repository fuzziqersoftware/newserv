#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "StaticGameData.hh"
#include "Version.hh"

enum class QuestCategory {
  UNKNOWN = -1,
  RETRIEVAL = 0,
  EXTERMINATION,
  EVENT,
  SHOP,
  VR,
  TOWER,
  GOVERNMENT_EPISODE_1,
  GOVERNMENT_EPISODE_2,
  GOVERNMENT_EPISODE_4,
  DOWNLOAD,
  BATTLE,
  CHALLENGE,
  SOLO,
  EPISODE_3,
};

bool category_is_mode(QuestCategory category);
const char* name_for_category(QuestCategory category);

class Quest {
public:
  enum class FileFormat {
    BIN_DAT = 0,
    BIN_DAT_UNCOMPRESSED,
    BIN_DAT_GCI,
    BIN_DAT_VMS,
    BIN_DAT_DLQ,
    QST,
  };
  int64_t internal_id;
  uint32_t menu_item_id;
  QuestCategory category;
  Episode episode;
  bool is_dcv1;
  bool joinable;
  GameVersion version;
  std::string file_basename; // we append -<version>.<bin/dat> when reading
  FileFormat file_format;
  bool has_mnm_extension;
  bool is_dlq_encoded;
  std::u16string name;
  std::u16string short_description;
  std::u16string long_description;

  Quest(const std::string& file_basename);
  Quest(const Quest&) = default;
  Quest(Quest&&) = default;
  Quest& operator=(const Quest&) = default;
  Quest& operator=(Quest&&) = default;

  std::string bin_filename() const;
  std::string dat_filename() const;

  std::shared_ptr<const std::string> bin_contents() const;
  std::shared_ptr<const std::string> dat_contents() const;

  std::shared_ptr<Quest> create_download_quest() const;

  static std::string decode_gci(
      const std::string& filename,
      ssize_t find_seed_num_threads = -1,
      int64_t known_seed = -1);
  static std::string decode_vms(
      const std::string& filename,
      ssize_t find_seed_num_threads = -1,
      int64_t known_seed = -1);
  static std::string decode_dlq(const std::string& filename);
  static std::pair<std::string, std::string> decode_qst(const std::string& filename);

  std::string export_qst(GameVersion version) const;

private:
  // these are populated when requested
  mutable std::shared_ptr<std::string> bin_contents_ptr;
  mutable std::shared_ptr<std::string> dat_contents_ptr;
};

struct QuestIndex {
  std::string directory;

  std::map<std::pair<GameVersion, uint64_t>, std::shared_ptr<Quest>> version_menu_item_id_to_quest;

  std::map<std::string, std::vector<std::shared_ptr<Quest>>> category_to_quests;

  std::map<std::string, std::shared_ptr<std::string>> gba_file_contents;

  QuestIndex(const std::string& directory);

  std::shared_ptr<const Quest> get(GameVersion version, uint32_t id) const;
  std::shared_ptr<const std::string> get_gba(const std::string& name) const;
  std::vector<std::shared_ptr<const Quest>> filter(GameVersion version,
      bool is_dcv1, QuestCategory category) const;
};
