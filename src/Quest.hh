#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "PlayerSubordinates.hh"
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
    std::string directory_name;
    std::string name;
    std::string description;

    explicit Category(uint32_t category_id, const JSON& json);

    bool matches_flags(uint8_t request) const;
  };

  std::vector<Category> categories;

  explicit QuestCategoryIndex(const JSON& json);

  const Category& at(uint32_t category_id) const;
};

struct VersionedQuest {
  uint32_t quest_number;
  uint32_t category_id;
  Episode episode;
  bool joinable;
  std::string name;
  QuestScriptVersion version;
  uint8_t language;
  bool is_dlq_encoded;
  std::string short_description;
  std::string long_description;
  std::shared_ptr<const std::string> bin_contents;
  std::shared_ptr<const std::string> dat_contents;
  std::shared_ptr<const std::string> pvr_contents;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index;

  VersionedQuest(
      uint32_t quest_number,
      uint32_t category_id,
      QuestScriptVersion version,
      uint8_t language,
      std::shared_ptr<const std::string> bin_contents,
      std::shared_ptr<const std::string> dat_contents,
      std::shared_ptr<const std::string> pvr_contents,
      std::shared_ptr<const BattleRules> battle_rules = nullptr,
      ssize_t challenge_template_index = -1);

  std::string bin_filename() const;
  std::string dat_filename() const;
  std::string xb_filename() const;

  std::shared_ptr<VersionedQuest> create_download_quest(uint8_t override_language = 0xFF) const;
  std::string encode_qst() const;
};

class Quest {
public:
  Quest() = delete;
  explicit Quest(std::shared_ptr<const VersionedQuest> initial_version);
  Quest(const Quest&) = default;
  Quest(Quest&&) = default;
  Quest& operator=(const Quest&) = default;
  Quest& operator=(Quest&&) = default;

  void add_version(std::shared_ptr<const VersionedQuest> vq);
  bool has_version(QuestScriptVersion v, uint8_t language) const;
  bool has_version_any_language(QuestScriptVersion v) const;
  std::shared_ptr<const VersionedQuest> version(QuestScriptVersion v, uint8_t language) const;

  static uint16_t versions_key(QuestScriptVersion v, uint8_t language);

  uint32_t quest_number;
  uint32_t category_id;
  Episode episode;
  bool joinable;
  std::string name;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index;
  std::map<uint16_t, std::shared_ptr<const VersionedQuest>> versions;
};

struct QuestIndex {
  std::string directory;
  std::shared_ptr<const QuestCategoryIndex> category_index;

  std::map<uint32_t, std::shared_ptr<Quest>> quests_by_number;

  QuestIndex(const std::string& directory, std::shared_ptr<const QuestCategoryIndex> category_index, bool is_ep3);

  std::shared_ptr<const Quest> get(uint32_t quest_number) const;
  std::vector<std::shared_ptr<const Quest>> filter(uint32_t category_id, QuestScriptVersion version) const;
};

std::string encode_download_quest_data(
    const std::string& compressed_data,
    size_t decompressed_size = 0,
    uint32_t encryption_seed = 0);

std::string decode_gci_data(
    const std::string& data,
    ssize_t find_seed_num_threads = -1,
    int64_t known_seed = -1,
    bool skip_checksum = false);
std::string decode_vms_data(
    const std::string& data,
    ssize_t find_seed_num_threads = -1,
    int64_t known_seed = -1,
    bool skip_checksum = false);
std::string decode_dlq_data(const std::string& data);
std::unordered_map<std::string, std::string> decode_qst_data(const std::string& data);

std::string encode_qst_file(
    const std::unordered_map<std::string, std::shared_ptr<const std::string>>& files,
    const std::string& name,
    uint32_t quest_number,
    const std::string& xb_filename,
    QuestScriptVersion version,
    bool is_dlq_encoded);
