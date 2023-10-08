#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "QuestScript.hh"
#include "StaticGameData.hh"

enum class QuestFileFormat {
  BIN_DAT = 0,
  BIN_DAT_UNCOMPRESSED,
  BIN_DAT_GCI,
  BIN_DAT_VMS,
  BIN_DAT_DLQ,
  QST,
};

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

    explicit Category(uint32_t category_id, const JSON& json);

    bool matches_flags(uint8_t request) const;
  };

  std::vector<Category> categories;

  explicit QuestCategoryIndex(const JSON& json);

  const Category& find(char type, const std::string& short_token) const;
  const Category& at(uint32_t category_id) const;
};

class VersionedQuest {
public:
  struct DATSectionHeader {
    le_uint32_t type; // 1 = objects, 2 = enemies. There are other types too
    le_uint32_t section_size; // Includes this header
    le_uint32_t area;
    le_uint32_t data_size;
  } __attribute__((packed));

  uint32_t quest_number;
  uint32_t category_id;
  Episode episode;
  bool joinable;
  QuestScriptVersion version;
  std::string file_basename; // we append -<version>.<bin/dat> when reading
  QuestFileFormat file_format;
  bool has_mnm_extension;
  bool is_dlq_encoded;
  std::u16string name;
  std::u16string short_description;
  std::u16string long_description;

  VersionedQuest(const std::string& file_basename, QuestScriptVersion version, std::shared_ptr<const QuestCategoryIndex> category_index);
  VersionedQuest(const VersionedQuest&) = default;
  VersionedQuest(VersionedQuest&&) = default;
  VersionedQuest& operator=(const VersionedQuest&) = default;
  VersionedQuest& operator=(VersionedQuest&&) = default;

  std::string bin_filename() const;
  std::string dat_filename() const;

  std::shared_ptr<const std::string> bin_contents() const;
  std::shared_ptr<const std::string> dat_contents() const;

  std::shared_ptr<VersionedQuest> create_download_quest() const;
  std::string encode_qst() const;

private:
  // these are populated when requested
  mutable std::shared_ptr<std::string> bin_contents_ptr;
  mutable std::shared_ptr<std::string> dat_contents_ptr;
};

class Quest {
public:
  Quest() = delete;
  explicit Quest(std::shared_ptr<const VersionedQuest> initial_version);
  Quest(const Quest&) = default;
  Quest(Quest&&) = default;
  Quest& operator=(const Quest&) = default;
  Quest& operator=(Quest&&) = default;

  void add_version(shared_ptr<const VersionedQuest> vq);
  bool has_version(QuestScriptVersion v) const;
  shared_ptr<const VersionedQuest> version(QuestScriptVersion v) const;

  uint32_t quest_number;
  uint32_t category_id;
  Episode episode;
  bool joinable;
  std::u16string name;

  uint16_t versions_present;
  std::unordered_map<QuestScriptVersion, std::shared_ptr<const VersionedQuest>> versions;
};

struct QuestIndex {
  std::string directory;
  std::shared_ptr<const QuestCategoryIndex> category_index;

  std::map<uint32_t, std::shared_ptr<Quest>> quests_by_number;

  std::map<std::string, std::shared_ptr<std::string>> gba_file_contents;

  QuestIndex(const std::string& directory, std::shared_ptr<const QuestCategoryIndex> category_index);

  std::shared_ptr<const Quest> get(uint32_t quest_number) const;
  std::shared_ptr<const std::string> get_gba(const std::string& name) const;
  std::vector<std::shared_ptr<const Quest>> filter(uint32_t category_id, QuestScriptVersion version) const;
};

std::string encode_download_quest_file(
    const std::string& compressed_data,
    size_t decompressed_size = 0,
    uint32_t encryption_seed = 0);

std::string decode_gci_file(
    const std::string& filename,
    ssize_t find_seed_num_threads = -1,
    int64_t known_seed = -1,
    bool skip_checksum = false);
std::string decode_vms_file(
    const std::string& filename,
    ssize_t find_seed_num_threads = -1,
    int64_t known_seed = -1,
    bool skip_checksum = false);

std::string decode_dlq_file(const std::string& filename);
std::string decode_dlq_data(const std::string& data);

std::pair<std::string, std::string> decode_qst_file(const std::string& filename);
std::string encode_qst_file(
    const std::string& bin_data,
    const std::string& dat_data,
    const std::u16string& name,
    uint32_t quest_number,
    QuestScriptVersion version,
    bool is_dlq_encoded);
