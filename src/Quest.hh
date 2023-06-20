#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "StaticGameData.hh"
#include "Version.hh"

struct QuestCategoryIndex {
  struct Category {
    enum Flag {
      NORMAL = 0x01,
      BATTLE = 0x02,
      CHALLENGE = 0x04,
      SOLO = 0x08,
      GOVERNMENT = 0x10,
      DOWNLOAD = 0x20,
      EP3_DOWNLOAD = 0x40,
      HIDE_ON_PRE_V3 = 0x80,
    };

    uint32_t category_id;
    uint8_t flags;
    char type;
    std::string short_token;
    std::u16string name;
    std::u16string description;

    explicit Category(uint32_t category_id, std::shared_ptr<const JSONObject> json);

    bool matches_flags(uint8_t request) const;
  };

  std::vector<Category> categories;

  explicit QuestCategoryIndex(std::shared_ptr<const JSONObject> json);

  const Category& find(char type, const std::string& short_token) const;
  const Category& at(uint32_t category_id) const;
};

class Quest {
public:
  struct DATSectionHeader {
    le_uint32_t type; // 1 = objects, 2 = enemies. There are other types too
    le_uint32_t section_size; // Includes this header
    le_uint32_t area;
    le_uint32_t data_size;
  } __attribute__((packed));

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
  uint32_t category_id;
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

  Quest(const std::string& file_basename, std::shared_ptr<const QuestCategoryIndex> category_index);
  Quest(const Quest&) = default;
  Quest(Quest&&) = default;
  Quest& operator=(const Quest&) = default;
  Quest& operator=(Quest&&) = default;

  std::string bin_filename() const;
  std::string dat_filename() const;

  std::shared_ptr<const std::string> bin_contents() const;
  std::shared_ptr<const std::string> dat_contents() const;

  std::shared_ptr<Quest> create_download_quest() const;

  static std::string decode_gci_file(
      const std::string& filename,
      ssize_t find_seed_num_threads = -1,
      int64_t known_seed = -1);
  static std::string decode_vms_file(
      const std::string& filename,
      ssize_t find_seed_num_threads = -1,
      int64_t known_seed = -1);
  static std::string decode_dlq_file(const std::string& filename);
  static std::string decode_dlq_data(const std::string& filename);
  static std::pair<std::string, std::string> decode_qst_file(const std::string& filename);

  std::string export_qst(GameVersion version) const;

private:
  // these are populated when requested
  mutable std::shared_ptr<std::string> bin_contents_ptr;
  mutable std::shared_ptr<std::string> dat_contents_ptr;
};

struct QuestIndex {
  std::string directory;
  std::shared_ptr<const QuestCategoryIndex> category_index;

  std::map<std::pair<GameVersion, uint64_t>, std::shared_ptr<Quest>> version_menu_item_id_to_quest;

  std::map<std::string, std::vector<std::shared_ptr<Quest>>> category_to_quests;

  std::map<std::string, std::shared_ptr<std::string>> gba_file_contents;

  QuestIndex(const std::string& directory, std::shared_ptr<const QuestCategoryIndex> category_index);

  std::shared_ptr<const Quest> get(GameVersion version, uint32_t id) const;
  std::shared_ptr<const std::string> get_gba(const std::string& name) const;
  std::vector<std::shared_ptr<const Quest>> filter(GameVersion version,
      bool is_dcv1, uint32_t category_id) const;
};
