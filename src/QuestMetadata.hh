#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CommonItemSet.hh"
#include "EnemyType.hh"
#include "IntegralExpression.hh"
#include "Map.hh"
#include "PlayerSubordinates.hh"
#include "RareItemSet.hh"
#include "StaticGameData.hh"

struct QuestMetadata {
  // This structure contains configuration that should be the same across all versions of the quest, except for the
  // name and description strings. This is used in both the Quest and VersionedQuest structures; in Quest, the name and
  // description are used only internally.

  Version version;
  Language language;

  // Fields that must match across all quest versions
  struct FloorAssignment {
    uint8_t floor = 0xFF;
    uint8_t area = 0xFF;
    uint8_t type = 0xFF;
    uint8_t layout_var = 0xFF;
    uint8_t entities_var = 0xFF;

    bool operator==(const FloorAssignment& other) const = default;
    bool operator!=(const FloorAssignment& other) const = default;

    phosg::JSON json() const;
    std::string str() const;
  };
  uint32_t category_id = 0xFFFFFFFF;
  uint32_t quest_number = 0xFFFFFFFF;
  Episode episode = Episode::NONE;
  std::array<FloorAssignment, 0x12> floor_assignments;
  bool joinable = false;
  uint8_t max_players = 4;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index = -1;
  float challenge_exp_multiplier = -1.0f;
  Difficulty challenge_difficulty = Difficulty::UNKNOWN;
  uint8_t description_flag = 0x00;
  std::shared_ptr<const IntegralExpression> available_expression;
  std::shared_ptr<const IntegralExpression> enabled_expression;
  std::string common_item_set_name; // blank = use default
  std::string rare_item_set_name; // blank = use default
  uint8_t allowed_drop_modes = 0x00; // 0 = use server default
  ServerDropMode default_drop_mode = ServerDropMode::CLIENT; // Ignored if allowed_drop_modes == 0
  bool allow_start_from_chat_command = false;
  int16_t lock_status_register = -1;
  std::unordered_map<uint32_t, uint32_t> enemy_exp_overrides;
  bool enable_schtserv_commands = false;

  // Extra header fields (only used on BB)
  struct CreateItemMask {
    struct Range {
      uint8_t min = 0x00;
      uint8_t max = 0x00;

      bool operator==(const Range& other) const = default;
      bool operator!=(const Range& other) const = default;
    };
    std::array<Range, 12> data1_ranges;

    CreateItemMask() = default;
    CreateItemMask(const CreateItemMask& other) = default;
    CreateItemMask(CreateItemMask&& other) = default;
    CreateItemMask& operator=(const CreateItemMask& other) = default;
    CreateItemMask& operator=(CreateItemMask&& other) = default;
    bool operator==(const CreateItemMask& other) const = default;
    bool operator!=(const CreateItemMask& other) const = default;

    explicit CreateItemMask(const std::string& s); // Inverse of str()
    std::string str() const;

    bool match(const ItemData& item) const;
    uint32_t primary_identifier() const; // Raises if any of data1[0-2] are ambiguous
  };
  std::vector<FloorAssignment> bb_map_designate_opcodes;
  std::vector<CreateItemMask> create_item_mask_entries;
  std::vector<uint16_t> solo_unlock_flags;

  // Unknown header fields. These are not used by the client, so they are not required to match across quest versions;
  // however, we still parse them in case we later discover that they had some server-side meaning.
  uint16_t header_unknown_a1 = 0xFFFF; // All versions
  uint16_t header_unknown_a2 = 0xFFFF; // All versions
  uint8_t header_unknown_a3 = 0; // DCv1 - V3
  uint8_t header_unknown_a4 = 0; // BB only
  uint16_t header_unknown_a6 = 0; // BB only
  uint32_t header_unknown_a5 = 0; // BB only
  int16_t header_episode = -1; // -1 = unspecified; BB only; newserv uses script analysis instead
  int16_t header_language = -1; // -1 = unspecified; DCv1 and later; newserv uses the filename instead

  // Fields that may be different across quest versions (and are only used on VersionedQuest, not Quest)
  std::string name;
  std::string short_description;
  std::string long_description;
  size_t text_offset;
  size_t label_table_offset;
  size_t total_size;

  static std::unordered_map<uint32_t, uint32_t> parse_enemy_exp_overrides(const phosg::JSON& json);
  static inline uint32_t exp_override_key(Difficulty difficulty, uint8_t floor, EnemyType enemy_type) {
    return (static_cast<uint32_t>(difficulty) << 24) | (static_cast<uint32_t>(floor) << 16) | static_cast<uint32_t>(enemy_type);
  }

  void apply_json_overrides(const phosg::JSON& json);

  void assign_default_floors();
  inline std::array<uint8_t, 0x12> get_floor_to_area() const {
    std::array<uint8_t, 0x12> ret;
    for (size_t z = 0; z < 0x12; z++) {
      ret[z] = this->floor_assignments[z].area;
    }
    return ret;
  }

  void assert_compatible(const QuestMetadata& other) const;
  phosg::JSON json() const;
  std::string areas_str() const;
};
