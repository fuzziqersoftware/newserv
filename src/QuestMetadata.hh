#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CommonItemSet.hh"
#include "IntegralExpression.hh"
#include "Map.hh"
#include "PlayerSubordinates.hh"
#include "RareItemSet.hh"

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

  std::string name;
  std::string short_description;
  std::string long_description;

  void assign_default_areas(Version version, Episode episode);
  void assert_compatible(const QuestMetadata& other) const;
  phosg::JSON json() const;
  std::string areas_str() const;
};
