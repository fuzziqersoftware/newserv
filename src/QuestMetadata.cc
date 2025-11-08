#include "QuestMetadata.hh"

using namespace std;

void QuestMetadata::assign_default_areas(Version version, Episode episode) {
  for (size_t z = 0; z < 0x12; z++) {
    this->area_for_floor[z] = SetDataTableBase::default_area_for_floor(version, episode, z);
  }
}

void QuestMetadata::assert_compatible(const QuestMetadata& other) const {
  if (this->quest_number != other.quest_number) {
    throw logic_error(std::format(
        "incorrect versioned quest number (existing: {:08X}, new: {:08X})",
        this->quest_number, other.quest_number));
  }
  if (this->category_id != other.category_id) {
    throw runtime_error(std::format(
        "quest version is in a different category (existing: {:08X}, new: {:08X})",
        this->category_id, other.category_id));
  }
  if (this->episode != other.episode) {
    throw runtime_error(std::format(
        "quest version is in a different episode (existing: {}, new: {})",
        name_for_episode(this->episode), name_for_episode(other.episode)));
  }
  if (this->allow_start_from_chat_command != other.allow_start_from_chat_command) {
    throw runtime_error(std::format(
        "quest version has a different allow_start_from_chat_command state (existing: {}, new: {})",
        this->allow_start_from_chat_command ? "true" : "false", other.allow_start_from_chat_command ? "true" : "false"));
  }
  if (this->joinable != other.joinable) {
    throw runtime_error(std::format(
        "quest version has a different joinability state (existing: {}, new: {})",
        this->joinable ? "true" : "false", other.joinable ? "true" : "false"));
  }
  if (this->max_players != other.max_players) {
    throw runtime_error(std::format(
        "quest version has a different maximum player count (existing: {}, new: {})",
        this->max_players, other.max_players));
  }
  if (this->lock_status_register != other.lock_status_register) {
    throw runtime_error(std::format(
        "quest version has a different lock status register (existing: {:04X}, new: {:04X})",
        this->lock_status_register, other.lock_status_register));
  }
  if (this->enemy_exp_overrides != other.enemy_exp_overrides) {
    throw runtime_error("quest version has different enemy EXP overrides");
  }
  if (!this->battle_rules != !other.battle_rules) {
    throw runtime_error(std::format(
        "quest version has a different battle rules presence state (existing: {}, new: {})",
        this->battle_rules ? "present" : "absent", other.battle_rules ? "present" : "absent"));
  }
  if (this->battle_rules && (*this->battle_rules != *other.battle_rules)) {
    string existing_str = this->battle_rules->json().serialize();
    string new_str = other.battle_rules->json().serialize();
    throw runtime_error(std::format(
        "quest version has different battle rules (existing: {}, new: {})",
        existing_str, new_str));
  }
  if (this->challenge_template_index != other.challenge_template_index) {
    throw runtime_error(std::format(
        "quest version has different challenge template index (existing: {}, new: {})",
        this->challenge_template_index, other.challenge_template_index));
  }
  if (this->challenge_exp_multiplier != other.challenge_exp_multiplier) {
    throw runtime_error(std::format(
        "quest version has different challenge EXP multiplier (existing: {}, new: {})",
        this->challenge_exp_multiplier, other.challenge_exp_multiplier));
  }
  if (this->challenge_difficulty != other.challenge_difficulty) {
    throw runtime_error(std::format(
        "quest version has different challenge difficulty (existing: {}, new: {})",
        name_for_difficulty(this->challenge_difficulty), name_for_difficulty(other.challenge_difficulty)));
  }
  for (size_t z = 0; z < this->area_for_floor.size(); z++) {
    const auto& this_fa = this->area_for_floor[z];
    const auto& other_fa = other.area_for_floor[z];
    if (this_fa != other_fa) {
      throw runtime_error(std::format(
          "quest version has different area on floor 0x{:02X} (existing: {}, new: {})",
          z, phosg::format_data_string(this->area_for_floor.data(), 0x12), phosg::format_data_string(other.area_for_floor.data(), 0x12)));
    }
  }
  if (this->description_flag != other.description_flag) {
    throw runtime_error(std::format(
        "quest version has different description flag (existing: {:02X}, new: {:02X})",
        this->description_flag, other.description_flag));
  }
  if (!this->available_expression != !other.available_expression) {
    throw runtime_error(std::format(
        "quest version has available expression but root quest does not, or vice versa (existing: {}, new: {})",
        this->available_expression ? "present" : "absent", other.available_expression ? "present" : "absent"));
  }
  if (this->available_expression && *this->available_expression != *other.available_expression) {
    string existing_str = this->available_expression->str();
    string new_str = other.available_expression->str();
    throw runtime_error(std::format(
        "quest version has a different available expression (existing: {}, new: {})",
        existing_str, new_str));
  }
  if (!this->enabled_expression != !other.enabled_expression) {
    throw runtime_error(std::format(
        "quest version has enabled expression but root quest does not, or vice versa (existing: {}, new: {})",
        this->enabled_expression ? "present" : "absent", other.enabled_expression ? "present" : "absent"));
  }
  if (this->enabled_expression && *this->enabled_expression != *other.enabled_expression) {
    string existing_str = this->enabled_expression->str();
    string new_str = other.enabled_expression->str();
    throw runtime_error(std::format(
        "quest version has a different enabled expression (existing: {}, new: {})",
        existing_str, new_str));
  }
  if (this->common_item_set_name != other.common_item_set_name) {
    throw runtime_error(std::format(
        "quest version has different common table name (existing: {}, new: {})",
        this->common_item_set_name, other.common_item_set_name));
  }
  if (this->common_item_set != other.common_item_set) {
    throw runtime_error("quest version has different common table");
  }
  if (this->rare_item_set_name != other.rare_item_set_name) {
    throw runtime_error(std::format(
        "quest version has different rare table name (existing: {}, new: {})",
        this->rare_item_set_name, other.rare_item_set_name));
  }
  if (this->rare_item_set != other.rare_item_set) {
    throw runtime_error("quest version has different rare table");
  }
  if (this->allowed_drop_modes != other.allowed_drop_modes) {
    throw runtime_error(format("quest version has different allowed drop modes (existing: {:02X}, new: {:02X})",
        this->allowed_drop_modes, other.allowed_drop_modes));
  }
  if (this->default_drop_mode != other.default_drop_mode) {
    throw runtime_error(format("quest version has different default drop mode (existing: {}, new: {})",
        phosg::name_for_enum(this->default_drop_mode), phosg::name_for_enum(other.default_drop_mode)));
  }
}

phosg::JSON QuestMetadata::json() const {
  auto floors_json = phosg::JSON::list();
  for (const auto& fa : this->area_for_floor) {
    floors_json.emplace_back(fa);
  }
  auto enemy_exp_overrides_json = phosg::JSON::dict();
  for (const auto& [key, exp_override] : this->enemy_exp_overrides) {
    auto difficulty = static_cast<Difficulty>((key >> 24) & 3);
    auto floor = static_cast<uint8_t>((key >> 16) & 0xFF);
    auto enemy_type = static_cast<EnemyType>(key & 0xFFFF);
    auto key_str = std::format("{}:0x{:02X}:{}", name_for_difficulty(difficulty), floor, phosg::name_for_enum(enemy_type));
    enemy_exp_overrides_json.emplace(key_str, exp_override);
  }
  return phosg::JSON::dict({
      {"CategoryID", this->category_id},
      {"Number", this->quest_number},
      {"Episode", name_for_episode(this->episode)},
      {"FloorAssignments", std::move(floors_json)},
      {"Joinable", this->joinable},
      {"MaxPlayers", this->max_players},
      {"BattleRules", this->battle_rules ? this->battle_rules->json() : phosg::JSON(nullptr)},
      {"ChallengeTemplateIndex", (this->challenge_template_index >= 0) ? this->challenge_template_index : phosg::JSON(nullptr)},
      {"ChallengeEXPMultiplier", (this->challenge_exp_multiplier >= 0) ? this->challenge_exp_multiplier : phosg::JSON(nullptr)},
      {"ChallengeDifficulty", (this->challenge_difficulty != Difficulty::UNKNOWN) ? name_for_difficulty(this->challenge_difficulty) : phosg::JSON(nullptr)},
      {"DescriptionFlag", this->description_flag},
      {"AvailableExpression", this->available_expression ? this->available_expression->str() : phosg::JSON(nullptr)},
      {"EnabledExpression", this->available_expression ? this->available_expression->str() : phosg::JSON(nullptr)},
      {"CommonItemSetName", this->common_item_set_name.empty() ? phosg::JSON(nullptr) : this->common_item_set_name},
      {"RareItemSetName", this->rare_item_set_name.empty() ? phosg::JSON(nullptr) : this->rare_item_set_name},
      {"AllowedDropModes", this->allowed_drop_modes},
      {"DefaultDropMode", phosg::name_for_enum(this->default_drop_mode)},
      {"AllowStartFromChatCommand", this->allow_start_from_chat_command},
      {"LockStatusRegister", (this->lock_status_register >= 0) ? this->lock_status_register : phosg::JSON(nullptr)},
      {"EnemyEXPOverrides", std::move(enemy_exp_overrides_json)},
  });
}

std::unordered_map<uint32_t, uint32_t> QuestMetadata::parse_enemy_exp_overrides(const phosg::JSON& json) {
  try {
    std::unordered_map<uint32_t, uint32_t> ret;
    for (const auto& [key, exp_value_json] : json.as_dict()) {
      // Key is like "Difficulty:Floor:EnemyType" or "Difficulty:EnemyType"
      auto key_tokens = phosg::split(key, ':');

      static const unordered_map<string, Difficulty> difficulty_keys(
          {{"Normal", Difficulty::NORMAL}, {"Hard", Difficulty::HARD}, {"VeryHard", Difficulty::VERY_HARD}, {"Ultimate", Difficulty::ULTIMATE}});

      Difficulty difficulty = Difficulty::NORMAL;
      EnemyType enemy_type = EnemyType::UNKNOWN;
      uint8_t floor = 0xFF;
      if (key_tokens.size() == 2) {
        enemy_type = phosg::enum_for_name<EnemyType>(key_tokens[1]);
      } else if (key_tokens.size() == 3) {
        floor = stoul(key_tokens[1], nullptr, 0);
        enemy_type = phosg::enum_for_name<EnemyType>(key_tokens[2]);
      } else {
        throw runtime_error("malformatted key: " + key);
      }
      difficulty = difficulty_keys.at(key_tokens[0]);
      if (floor == 0xFF) {
        for (size_t floor = 0; floor < 0x12; floor++) {
          ret.emplace(QuestMetadata::exp_override_key(difficulty, floor, enemy_type), exp_value_json->as_int());
        }
      } else {
        ret.emplace(QuestMetadata::exp_override_key(difficulty, floor, enemy_type), exp_value_json->as_int());
      }
    }
    return ret;

  } catch (const exception& e) {
    throw std::runtime_error(std::format("invalid enemy EXP overrides: ", e.what()));
  }
}
