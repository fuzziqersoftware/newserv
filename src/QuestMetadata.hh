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
  // This structure contains configuration that should be the same across all
  // versions of the quest, except for the name and description strings. This
  // is used in both the Quest and VersionedQuest structures; in Quest, the
  // name and description are used only internally.
  uint32_t category_id = 0xFFFFFFFF;
  uint32_t quest_number = 0xFFFFFFFF;
  Episode episode = Episode::NONE;
  std::array<uint8_t, 0x12> area_for_floor;
  bool joinable = false;
  uint8_t max_players = 0x00;
  std::shared_ptr<const BattleRules> battle_rules;
  ssize_t challenge_template_index = -1;
  float challenge_exp_multiplier = -1.0f;
  Difficulty challenge_difficulty = Difficulty::UNKNOWN;
  uint8_t description_flag = 0x00;
  std::shared_ptr<const IntegralExpression> available_expression;
  std::shared_ptr<const IntegralExpression> enabled_expression;
  std::string common_item_set_name; // blank = use default
  std::string rare_item_set_name; // blank = use default
  std::shared_ptr<const CommonItemSet> common_item_set;
  std::shared_ptr<const RareItemSet> rare_item_set;
  uint8_t allowed_drop_modes = 0x00; // 0 = use server default
  ServerDropMode default_drop_mode = ServerDropMode::CLIENT; // Ignored if allowed_drop_modes == 0
  bool allow_start_from_chat_command = false;
  int16_t lock_status_register = -1;
  std::unordered_map<uint32_t, uint32_t> enemy_exp_overrides;

  // Item create allowances (only used on BB)
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
  std::vector<CreateItemMask> create_item_mask_entries;

  std::string name;
  std::string short_description;
  std::string long_description;

  static std::unordered_map<uint32_t, uint32_t> parse_enemy_exp_overrides(const phosg::JSON& json);
  static inline uint32_t exp_override_key(Difficulty difficulty, uint8_t floor, EnemyType enemy_type) {
    return (static_cast<uint32_t>(difficulty) << 24) | (static_cast<uint32_t>(floor) << 16) | static_cast<uint32_t>(enemy_type);
  }

  void assign_default_areas(Version version, Episode episode);
  void assert_compatible(const QuestMetadata& other) const;
  phosg::JSON json() const;
  std::string areas_str() const;
};
