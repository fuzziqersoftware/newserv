#include "QuestMetadata.hh"

using namespace std;

phosg::JSON QuestMetadata::FloorAssignment::json() const {
  return phosg::JSON::dict({
      {"Floor", this->floor},
      {"Area", this->area},
      {"Type", this->type},
      {"LayoutVariation", this->layout_var},
      {"EntitiesVariation", this->entities_var},
  });
}

std::string QuestMetadata::FloorAssignment::str() const {
  return std::format(
      "FloorAssignment(floor=0x{:02X}, area=0x{:02X}, type=0x{:02X}, layout_var=0x{:02X}, entities_var=0x{:02X})",
      this->floor, this->area, this->type, this->layout_var, this->entities_var);
}

void QuestMetadata::apply_json_overrides(const phosg::JSON& json) {
  try {
    this->description_flag = json.at("DescriptionFlag").as_int();
  } catch (const out_of_range&) {
  }
  try {
    this->available_expression = make_shared<IntegralExpression>(json.get_string("AvailableIf"));
  } catch (const out_of_range&) {
  }
  try {
    this->enabled_expression = make_shared<IntegralExpression>(json.get_string("EnabledIf"));
  } catch (const out_of_range&) {
  }
  try {
    this->allow_start_from_chat_command = json.get_bool("AllowStartFromChatCommand");
  } catch (const out_of_range&) {
  }
  try {
    this->joinable = json.get_bool("Joinable");
  } catch (const out_of_range&) {
  }
  try {
    this->lock_status_register = json.get_int("LockStatusRegister");
  } catch (const out_of_range&) {
  }
  try {
    this->enemy_exp_overrides = QuestMetadata::parse_enemy_exp_overrides(json.at("EnemyEXPOverrides"));
  } catch (const out_of_range&) {
  }
  try {
    this->common_item_set_name = json.at("CommonItemSetName").as_string();
  } catch (const out_of_range&) {
  }
  try {
    this->rare_item_set_name = json.at("RareItemSetName").as_string();
  } catch (const out_of_range&) {
  }
  try {
    this->allowed_drop_modes = json.at("AllowedDropModes").as_int();
  } catch (const out_of_range&) {
  }
  try {
    this->default_drop_mode = phosg::enum_for_name<ServerDropMode>(json.at("DefaultDropMode").as_string());
  } catch (const out_of_range&) {
  }
  try {
    this->enable_schtserv_commands = json.at("EnableSchtservCommands").as_bool();
  } catch (const out_of_range&) {
  }
}

void QuestMetadata::assign_default_floors() {
  for (size_t z = 0; z < 0x12; z++) {
    auto& fa = this->floor_assignments[z];
    fa.floor = z;
    fa.area = SetDataTableBase::default_floor_to_area(this->version, this->episode)[z];
    fa.type = 0;
    fa.layout_var = 0;
    fa.entities_var = 0;
  }
}

void QuestMetadata::assert_compatible(const QuestMetadata& other) const {
  if (this->quest_number != other.quest_number) {
    throw logic_error(std::format(
        "incorrect versioned quest number (existing: {:08X}, new: {:08X})", this->quest_number, other.quest_number));
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
        this->allow_start_from_chat_command ? "true" : "false",
        other.allow_start_from_chat_command ? "true" : "false"));
  }
  if (this->joinable != other.joinable) {
    throw runtime_error(std::format(
        "quest version has a different joinability state (existing: {}, new: {})",
        this->joinable ? "true" : "false", other.joinable ? "true" : "false"));
  }
  bool this_has_player_limit = (this->max_players != 0) && (this->max_players != 4);
  bool other_has_player_limit = (other.max_players != 0) && (other.max_players != 4);
  if ((this_has_player_limit || other_has_player_limit) && (this->max_players != other.max_players)) {
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
  if (this->solo_unlock_flags != other.solo_unlock_flags) {
    throw runtime_error(std::format("quest version has a different set of solo unlock flags"));
  }
  if (!this->create_item_mask_entries.empty() && !other.create_item_mask_entries.empty() &&
      this->create_item_mask_entries != other.create_item_mask_entries) {
    string this_str, other_str;
    for (const auto& item : this->create_item_mask_entries) {
      if (!this_str.empty()) {
        this_str += ", ";
      }
      this_str += item.str();
    }
    for (const auto& item : other.create_item_mask_entries) {
      if (!other_str.empty()) {
        other_str += ", ";
      }
      other_str += item.str();
    }
    throw runtime_error(std::format(
        "quest version has a different set of create item masks (existing: {}, new: {})", this_str, other_str));
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
        "quest version has different battle rules (existing: {}, new: {})", existing_str, new_str));
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
  for (size_t z = 0; z < this->floor_assignments.size(); z++) {
    const auto& this_fa = this->floor_assignments[z];
    const auto& other_fa = other.floor_assignments[z];
    if (this_fa.area != other_fa.area) {
      throw runtime_error(std::format(
          "quest version has different area on floor 0x{:02X} (existing: {}, new: {})",
          z, this_fa.str(), other_fa.str()));
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
        "quest version has a different available expression (existing: {}, new: {})", existing_str, new_str));
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
        "quest version has a different enabled expression (existing: {}, new: {})", existing_str, new_str));
  }
  if (this->common_item_set_name != other.common_item_set_name) {
    throw runtime_error(std::format(
        "quest version has different common table name (existing: {}, new: {})",
        this->common_item_set_name, other.common_item_set_name));
  }
  if (this->rare_item_set_name != other.rare_item_set_name) {
    throw runtime_error(std::format(
        "quest version has different rare table name (existing: {}, new: {})",
        this->rare_item_set_name, other.rare_item_set_name));
  }
  if (this->allowed_drop_modes != other.allowed_drop_modes) {
    throw runtime_error(format("quest version has different allowed drop modes (existing: {:02X}, new: {:02X})",
        this->allowed_drop_modes, other.allowed_drop_modes));
  }
  if (this->default_drop_mode != other.default_drop_mode) {
    throw runtime_error(format("quest version has different default drop mode (existing: {}, new: {})",
        phosg::name_for_enum(this->default_drop_mode), phosg::name_for_enum(other.default_drop_mode)));
  }
  if (this->enable_schtserv_commands != other.enable_schtserv_commands) {
    throw runtime_error(format(
        "quest version has different value for enable_schtserv_commands (existing: {}, new: {})",
        this->enable_schtserv_commands ? "true" : "false", other.enable_schtserv_commands ? "true" : "false"));
  }
}

phosg::JSON QuestMetadata::json() const {
  auto floors_json = phosg::JSON::list();
  for (const auto& fa : this->floor_assignments) {
    floors_json.emplace_back(fa.json());
  }
  auto enemy_exp_overrides_json = phosg::JSON::dict();
  for (const auto& [key, exp_override] : this->enemy_exp_overrides) {
    auto difficulty = static_cast<Difficulty>((key >> 24) & 3);
    auto floor = static_cast<uint8_t>((key >> 16) & 0xFF);
    auto enemy_type = static_cast<EnemyType>(key & 0xFFFF);
    auto key_str = std::format(
        "{}:0x{:02X}:{}", name_for_difficulty(difficulty), floor, phosg::name_for_enum(enemy_type));
    enemy_exp_overrides_json.emplace(key_str, exp_override);
  }

  auto create_item_mask_entries_json = phosg::JSON::list();
  for (const auto& item : this->create_item_mask_entries) {
    create_item_mask_entries_json.emplace_back(item.str());
  }

  auto solo_unlock_flags_json = phosg::JSON::list();
  for (uint16_t flag : this->solo_unlock_flags) {
    solo_unlock_flags_json.emplace_back(flag);
  }

  return phosg::JSON::dict({
      {"CategoryID", this->category_id},
      {"QuestNumber", this->quest_number},
      {"Episode", name_for_episode(this->episode)},
      {"FloorAssignments", std::move(floors_json)},
      {"Joinable", this->joinable},
      {"MaxPlayers", this->max_players ? 4 : this->max_players},
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
      {"CreateItemMasks", std::move(create_item_mask_entries_json)},
      {"SoloUnlockFlags", std::move(solo_unlock_flags_json)},
  });
}

QuestMetadata::CreateItemMask::CreateItemMask(const std::string& s) {
  phosg::StringReader r(s);
  // Ignore all whitespace
  auto get_ch = [&]() -> char {
    char ch = r.get_s8();
    while ((ch == ' ') || (ch == '\t')) {
      ch = r.get_s8();
    }
    return ch;
  };
  for (size_t z = 0; z < 12 && !r.eof(); z++) {
    auto& range = this->data1_ranges[z];
    char c = get_ch();
    if (c == '[') {
      c = get_ch();
      range.min = (phosg::value_for_hex_char(c) << 4) | phosg::value_for_hex_char(get_ch());
      if (get_ch() != '-') {
        throw std::runtime_error("invalid range spec");
      }
      c = get_ch();
      range.max = (phosg::value_for_hex_char(c) << 4) | phosg::value_for_hex_char(get_ch());
      if (get_ch() != ']') {
        throw std::runtime_error("invalid range spec");
      }
    } else {
      range.min = (phosg::value_for_hex_char(c) << 4) | phosg::value_for_hex_char(get_ch());
      range.max = range.min;
    }
  }
}

std::string QuestMetadata::CreateItemMask::str() const {
  std::string ret;
  for (size_t z = 0; z < 12; z++) {
    const auto& r = this->data1_ranges[z];
    if (r.min == r.max) {
      ret += std::format("{:02X}", r.min);
    } else {
      ret += std::format("[{:02X}-{:02X}]", r.min, r.max);
    }
  }
  return ret;
}

bool QuestMetadata::CreateItemMask::match(const ItemData& item) const {
  for (size_t z = 0; z < 12; z++) {
    const auto& r = this->data1_ranges[z];
    uint8_t v = item.data1[z];
    if (v < r.min || v > r.max) {
      return false;
    }
  }
  return true;
}

uint32_t QuestMetadata::CreateItemMask::primary_identifier() const {
  uint32_t ret = 0;
  for (size_t z = 0; z < 3; z++) {
    const auto& r = this->data1_ranges[z];
    if (r.min != r.max) {
      throw std::runtime_error("create item mask is ambiguous; cannot compute primary identifier");
    }
    ret = (ret << 8) | r.min;
  }
  return ret << 8;
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
