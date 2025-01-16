#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IntegralExpression.hh"
#include "Map.hh"
#include "PlayerSubordinates.hh"
#include "QuestScript.hh"
#include "StaticGameData.hh"
#include "TeamIndex.hh"

enum class QuestFileFormat {
  BIN_DAT = 0,
  BIN_DAT_UNCOMPRESSED,
  BIN_DAT_GCI,
  BIN_DAT_VMS,
  BIN_DAT_DLQ,
  QST,
};

enum class QuestMenuType {
  NORMAL = 0,
  BATTLE = 1,
  CHALLENGE = 2,
  SOLO = 3,
  GOVERNMENT = 4,
  DOWNLOAD = 5,
  EP3_DOWNLOAD = 6,
  // 7 can't be used as a menu type (it enables the per-episode filter)
};

struct QuestCategoryIndex {
  struct Category {
    uint32_t category_id;
    uint16_t enabled_flags;
    std::string directory_name;
    std::string name;
    std::string description;

    explicit Category(uint32_t category_id, const phosg::JSON& json);

    [[nodiscard]] inline bool check_flag(QuestMenuType menu_type) const {
      return this->enabled_flags & (1 << static_cast<uint8_t>(menu_type));
    }
    [[nodiscard]] inline bool enable_episode_filter() const {
      return this->enabled_flags & 0x080;
    }
    [[nodiscard]] inline bool use_ep2_icon() const {
      return this->enabled_flags & 0x100;
    }
  };

  std::vector<std::shared_ptr<Category>> categories;

  explicit QuestCategoryIndex(const phosg::JSON& json);

  std::shared_ptr<const Category> at(uint32_t category_id) const;
};

struct VersionedQuest {
  uint32_t quest_number = 0;
  uint32_t category_id = 0;
  Episode episode = Episode::NONE;
  bool allow_start_from_chat_command = false;
  bool joinable = false;
  int16_t lock_status_register = -1;
  std::string name;
  Version version = Version::UNKNOWN;
  uint8_t language = 1;
  bool is_dlq_encoded = false;
  std::string short_description;
  std::string long_description;
  std::shared_ptr<const std::string> bin_contents;
  std::shared_ptr<const std::string> dat_contents;
  std::shared_ptr<const MapFile> map_file;
  std::shared_ptr<const std::string> pvr_contents;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index = -1;
  uint8_t description_flag = 0;
  std::shared_ptr<const IntegralExpression> available_expression;
  std::shared_ptr<const IntegralExpression> enabled_expression;

  VersionedQuest(
      uint32_t quest_number,
      uint32_t category_id,
      Version version,
      uint8_t language,
      std::shared_ptr<const std::string> bin_contents,
      std::shared_ptr<const std::string> dat_contents,
      std::shared_ptr<const MapFile> map_file,
      std::shared_ptr<const std::string> pvr_contents,
      std::shared_ptr<const BattleRules> battle_rules = nullptr,
      ssize_t challenge_template_index = -1,
      uint8_t description_flag = 0,
      std::shared_ptr<const IntegralExpression> available_expression = nullptr,
      std::shared_ptr<const IntegralExpression> enabled_expression = nullptr,
      bool allow_start_from_chat_command = false,
      bool force_joinable = false,
      int16_t lock_status_register = -1);

  std::string bin_filename() const;
  std::string dat_filename() const;
  std::string pvr_filename() const;
  std::string xb_filename() const;

  std::shared_ptr<VersionedQuest> create_download_quest(uint8_t override_language = 0xFF) const;
  std::string encode_qst() const;
};

struct Quest {
  uint32_t quest_number;
  uint32_t category_id;
  Episode episode;
  bool allow_start_from_chat_command;
  bool joinable;
  int16_t lock_status_register;
  std::string name;
  mutable std::shared_ptr<const SuperMap> supermap;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index;
  uint8_t description_flag;
  std::shared_ptr<const IntegralExpression> available_expression;
  std::shared_ptr<const IntegralExpression> enabled_expression;
  std::map<uint32_t, std::shared_ptr<const VersionedQuest>> versions;

  Quest() = delete;
  explicit Quest(std::shared_ptr<const VersionedQuest> initial_version);
  Quest(const Quest&) = default;
  Quest(Quest&&) = default;
  Quest& operator=(const Quest&) = default;
  Quest& operator=(Quest&&) = default;

  phosg::JSON json() const;

  std::shared_ptr<const SuperMap> get_supermap(int64_t random_seed) const;

  void add_version(std::shared_ptr<const VersionedQuest> vq);
  bool has_version(Version v, uint8_t language) const;
  bool has_version_any_language(Version v) const;
  std::shared_ptr<const VersionedQuest> version(Version v, uint8_t language) const;

  static uint32_t versions_key(Version v, uint8_t language);
};

struct QuestIndex {
  enum class IncludeState {
    HIDDEN = 0,
    AVAILABLE,
    DISABLED,
  };
  using IncludeCondition = std::function<IncludeState(std::shared_ptr<const Quest>)>;

  std::string directory;
  std::shared_ptr<const QuestCategoryIndex> category_index;

  std::map<uint32_t, std::shared_ptr<Quest>> quests_by_number;
  std::map<std::string, std::shared_ptr<Quest>> quests_by_name;
  std::map<uint32_t, std::map<uint32_t, std::shared_ptr<Quest>>> quests_by_category_id_and_number;

  QuestIndex(const std::string& directory, std::shared_ptr<const QuestCategoryIndex> category_index, bool is_ep3);
  phosg::JSON json() const;

  std::shared_ptr<const Quest> get(uint32_t quest_number) const;
  std::shared_ptr<const Quest> get(const std::string& name) const;

  std::vector<std::shared_ptr<const QuestCategoryIndex::Category>> categories(
      QuestMenuType menu_type,
      Episode episode,
      uint16_t version_flags,
      IncludeCondition include_condition = nullptr) const;
  std::vector<std::pair<QuestIndex::IncludeState, std::shared_ptr<const Quest>>> filter(
      Episode episode,
      uint16_t version_flags,
      uint32_t category_id,
      IncludeCondition include_condition = nullptr,
      size_t limit = 0) const;
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
    Version version,
    bool is_dlq_encoded);
