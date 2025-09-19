#include "CommonItemSet.hh"

#include "AFSArchive.hh"
#include "EnemyType.hh"
#include "GSLArchive.hh"
#include "StaticGameData.hh"
#include "Types.hh"

using namespace std;

template <typename IntT, size_t Count>
phosg::JSON to_json(const parray<IntT, Count>& v) {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < Count; z++) {
    ret.emplace_back(v[z]);
  }
  return ret;
}

template <typename IntT, size_t Count>
void from_json_into(const phosg::JSON& json, parray<IntT, Count>& ret) {
  if (json.size() != Count) {
    throw runtime_error("incorrect array length");
  }
  for (size_t z = 0; z < Count; z++) {
    ret[z] = json.at(z).as_int();
  }
}

template <typename IntT, size_t Count>
phosg::JSON to_json(const parray<CommonItemSet::Table::Range<IntT>, Count>& v) {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < Count; z++) {
    ret.emplace_back(to_json(v[z]));
  }
  return ret;
}

template <typename IntT, size_t Count>
void from_json_into(const phosg::JSON& json, parray<CommonItemSet::Table::Range<IntT>, Count>& ret) {
  if (json.size() != Count) {
    throw runtime_error("incorrect array length");
  }
  for (size_t z = 0; z < Count; z++) {
    from_json_into(json.at(z), ret[z]);
  }
}

template <typename IntT>
phosg::JSON to_json(const CommonItemSet::Table::Range<IntT>& v) {
  return (v.min == v.max) ? phosg::JSON(v.min) : phosg::JSON::list({v.min, v.max});
}

template <typename IntT>
void from_json_into(const phosg::JSON& json, CommonItemSet::Table::Range<IntT>& ret) {
  if (json.is_int()) {
    IntT v = json.as_int();
    ret.min = v;
    ret.max = v;
  } else {
    const auto& l = json.as_list();
    if (l.size() != 2) {
      throw runtime_error("incorrect range list length");
    }
    ret.min = l.at(0)->as_int();
    ret.max = l.at(1)->as_int();
  }
}

template <typename IntT, size_t Count1, size_t Count2>
phosg::JSON to_json(const parray<parray<IntT, Count2>, Count1>& v) {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < Count1; z++) {
    ret.emplace_back(to_json(v[z]));
  }
  return ret;
}

template <typename IntT, size_t Count1, size_t Count2>
void from_json_into(const phosg::JSON& json, parray<parray<IntT, Count2>, Count1>& ret) {
  if (json.size() != Count1) {
    throw runtime_error("incorrect array length");
  }
  for (size_t z = 0; z < Count1; z++) {
    from_json_into(json.at(z), ret[z]);
  }
}

template <typename IntT, size_t Count1, size_t Count2>
void from_json_into(const phosg::JSON& json, parray<parray<CommonItemSet::Table::Range<IntT>, Count2>, Count1>& ret) {
  if (json.size() != Count1) {
    throw runtime_error("incorrect array length");
  }
  for (size_t z = 0; z < Count1; z++) {
    from_json_into(json.at(z), ret[z]);
  }
}

CommonItemSet::Table::Table(const phosg::JSON& json, Episode episode)
    : episode(episode) {
  from_json_into(json.at("BaseWeaponTypeProbTable"), this->base_weapon_type_prob_table);
  from_json_into(json.at("SubtypeBaseTable"), this->subtype_base_table);
  from_json_into(json.at("SubtypeAreaLengthTable"), this->subtype_area_length_table);
  from_json_into(json.at("GrindProbTable"), this->grind_prob_table);
  from_json_into(json.at("ArmorShieldTypeIndexProbTable"), this->armor_shield_type_index_prob_table);
  from_json_into(json.at("ArmorSlotCountProbTable"), this->armor_slot_count_prob_table);
  from_json_into(json.at("BoxMesetaRanges"), this->box_meseta_ranges);
  this->has_rare_bonus_value_prob_table = json.at("HasRareBonusValueProbTable").as_bool();
  from_json_into(json.at("BonusValueProbTable"), this->bonus_value_prob_table);
  from_json_into(json.at("NonRareBonusProbSpec"), this->nonrare_bonus_prob_spec);
  from_json_into(json.at("BonusTypeProbTable"), this->bonus_type_prob_table);
  from_json_into(json.at("SpecialMult"), this->special_mult);
  from_json_into(json.at("SpecialPercent"), this->special_percent);
  from_json_into(json.at("ToolClassProbTable"), this->tool_class_prob_table);
  from_json_into(json.at("TechniqueIndexProbTable"), this->technique_index_prob_table);
  from_json_into(json.at("TechniqueLevelRanges"), this->technique_level_ranges);
  this->armor_or_shield_type_bias = json.at("ArmorOrShieldTypeBias").as_int();
  from_json_into(json.at("UnitMaxStarsTable"), this->unit_max_stars_table);
  from_json_into(json.at("BoxItemClassProbTable"), this->box_item_class_prob_table);

  const auto& enemy_meseta_ranges_json = json.at("EnemyMesetaRanges").as_dict();
  const auto& enemy_type_drop_probs_json = json.at("EnemyTypeDropProbs").as_dict();
  const auto& enemy_item_classes_json = json.at("EnemyItemClasses").as_dict();
  for (size_t z = 0; z < 0x64; z++) {
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (Episode episode : episodes) {
      auto types = enemy_types_for_rare_table_index(episode, z);
      vector<string> names;
      if (types.empty()) {
        names.emplace_back(std::format("{}:!{:02X}", abbreviation_for_episode(episode), z));
      }
      for (auto type : enemy_types_for_rare_table_index(episode, z)) {
        names.emplace_back(std::format("{}:{}", abbreviation_for_episode(episode), phosg::name_for_enum(type)));
      }
      for (const auto& name : names) {
        from_json_into(*enemy_meseta_ranges_json.at(name), this->enemy_meseta_ranges[z]);
        this->enemy_type_drop_probs[z] = enemy_type_drop_probs_json.at(name)->as_int();
        this->enemy_item_classes[z] = enemy_item_classes_json.at(name)->as_int();
      }
    }
  }
}

static const char* name_for_common_item_class(uint8_t item_class) {
  switch (item_class) {
    case 0x00:
      return "WEAPON ";
    case 0x01:
      return "ARMOR  ";
    case 0x02:
      return "SHIELD ";
    case 0x03:
      return "UNIT   ";
    case 0x04:
      return "TOOL   ";
    case 0x05:
      return "MESETA ";
    case 0x06:
      return "NOTHING";
    default:
      return "UNKNOWN";
  }
}

void CommonItemSet::Table::print(FILE* stream) const {
  const auto& meseta_ranges = this->enemy_meseta_ranges;
  const auto& drop_probs = this->enemy_type_drop_probs;
  const auto& item_classes = this->enemy_item_classes;
  phosg::fwrite_fmt(stream, "Enemy tables:\n");
  phosg::fwrite_fmt(stream, "  ##   $LOW  $HIGH  DAR%  ITEM        ENEMIES\n");
  for (size_t z = 0; z < 0x64; z++) {
    string enemies_str;
    for (EnemyType enemy_type : enemy_types_for_rare_table_index(this->episode, z)) {
      if (!enemies_str.empty()) {
        enemies_str += ", ";
      }
      enemies_str += phosg::name_for_enum(enemy_type);
    }
    if (drop_probs[z]) {
      phosg::fwrite_fmt(stream, "  {:02X}  {:5}  {:5}  {:3}%  {:02X}:{}  {}\n",
          z, meseta_ranges[z].min, meseta_ranges[z].max, drop_probs[z], item_classes[z],
          name_for_common_item_class(item_classes[z]), enemies_str);
    } else {
      phosg::fwrite_fmt(stream, "  {:02X}  -----  -----    0%  --          {}\n", z, enemies_str);
    }
  }

  static const array<const char*, 12> base_weapon_type_names = {
      "SABER   ",
      "SWORD   ",
      "DAGGER  ",
      "PARTISAN",
      "SLICER  ",
      "HANDGUN ",
      "RIFLE   ",
      "MECHGUN ",
      "SHOT    ",
      "CANE    ",
      "ROD     ",
      "WAND    ",
  };
  phosg::fwrite_fmt(stream, "Base weapon config:\n");
  phosg::fwrite_fmt(stream, "  TYPE         PROB  [SB  AL]  FLOORS\n");
  for (size_t z = 0; z < 12; z++) {
    uint8_t floor_to_class[10];
    if (this->subtype_base_table[z] < 0) {
      size_t start_floor = std::min<size_t>(-this->subtype_area_length_table[z], 10);
      for (size_t x = 0; x < start_floor; x++) {
        floor_to_class[x] = 0xFF;
      }
      for (size_t x = start_floor; x < 10; x++) {
        floor_to_class[x] = (x - start_floor) / this->subtype_area_length_table[z];
      }
    } else {
      for (size_t x = 0; x < 10; x++) {
        floor_to_class[x] = this->subtype_base_table[z] + (x / this->subtype_area_length_table[z]);
      }
    }
    phosg::fwrite_fmt(stream, "  {:02X}:{}  {:3}%  [{:02X}  {:02X}]  {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}\n",
        z, base_weapon_type_names[z], this->base_weapon_type_prob_table[z],
        this->subtype_base_table[z], this->subtype_area_length_table[z],
        floor_to_class[0], floor_to_class[1], floor_to_class[2], floor_to_class[3], floor_to_class[4],
        floor_to_class[5], floor_to_class[6], floor_to_class[7], floor_to_class[8], floor_to_class[9]);
  }

  phosg::fwrite_fmt(stream, "Box configuration:\n");
  phosg::fwrite_fmt(stream, "  AR   $LOW  $HIGH  WEP%  ARM%  SHD%  UNI%   TL%  MST%   NO%\n");
  for (size_t z = 0; z < 10; z++) {
    phosg::fwrite_fmt(stream, "  {:02X}  {:5}  {:5}  {:3}%  {:3}%  {:3}%  {:3}%  {:3}%  {:3}%  {:3}%\n",
        z, this->box_meseta_ranges[z].min, this->box_meseta_ranges[z].max,
        this->box_item_class_prob_table[0][z],
        this->box_item_class_prob_table[1][z],
        this->box_item_class_prob_table[2][z],
        this->box_item_class_prob_table[3][z],
        this->box_item_class_prob_table[4][z],
        this->box_item_class_prob_table[5][z],
        this->box_item_class_prob_table[6][z]);
  }

  phosg::fwrite_fmt(stream, "Weapon drops:\n");
  phosg::fwrite_fmt(stream, "  Grinds:\n");
  phosg::fwrite_fmt(stream, "    GD  AR0%  AR1%  AR2%  AR3%\n");
  for (size_t z = 0; z < 9; z++) {
    phosg::fwrite_fmt(stream, "    +{}  {:3}%  {:3}%  {:3}%  {:3}%\n", z,
        this->grind_prob_table[z][0], this->grind_prob_table[z][1],
        this->grind_prob_table[z][2], this->grind_prob_table[z][3]);
  }
  phosg::fwrite_fmt(stream, "  Bonus value table:\n");
  phosg::fwrite_fmt(stream, "    ID");
  for (int8_t v = -10; v <= 100; v += 5) {
    phosg::fwrite_fmt(stream, "  {:5}%", v);
  }
  fputc('\n', stream);
  for (size_t z = 0; z < (this->has_rare_bonus_value_prob_table ? 6 : 5); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}", z);
    for (size_t x = 0; x < 0x17; x++) {
      phosg::fwrite_fmt(stream, "  {:5}#", this->bonus_value_prob_table[x][z]);
    }
    fputc('\n', stream);
  }
  phosg::fwrite_fmt(stream, "  Area config tables:\n");
  phosg::fwrite_fmt(stream, "    AR  BONUS  SP   NO%  NTV%   AB%  MAC%  DRK%  HIT%  SM  SPC%\n");
  for (size_t z = 0; z < 10; z++) {
    phosg::fwrite_fmt(stream, "    {:02X}  {:02X} {:02X}  {:02X}  {:3}%  {:3}%  {:3}%  {:3}%  {:3}%  {:3}%  {:02X}  {:3}%\n",
        z, this->nonrare_bonus_prob_spec[0][z], this->nonrare_bonus_prob_spec[1][z], this->nonrare_bonus_prob_spec[2][z],
        this->bonus_type_prob_table[0][z], this->bonus_type_prob_table[1][z], this->bonus_type_prob_table[2][z],
        this->bonus_type_prob_table[3][z], this->bonus_type_prob_table[4][z], this->bonus_type_prob_table[5][z],
        this->special_mult[z], this->special_percent[z]);
  }

  phosg::fwrite_fmt(stream, "  Tool class table:\n");
  phosg::fwrite_fmt(stream, "    CS     A1     A2     A3     A4     A5     A6     A7     A8     A9    A10\n");
  for (size_t tool_class = 0; tool_class < this->tool_class_prob_table.size(); tool_class++) {
    phosg::fwrite_fmt(stream, "    {:02X}", tool_class);
    for (size_t area_norm = 0; area_norm < 10; area_norm++) {
      phosg::fwrite_fmt(stream, "  {:5}", this->tool_class_prob_table[tool_class][area_norm]);
    }
    fputc('\n', stream);
  }

  static const array<const char*, 19> technique_names = {
      "FOIE    ",
      "GIFOIE  ",
      "RAFOIE  ",
      "BARTA   ",
      "GIBARTA ",
      "RABARTA ",
      "ZONDE   ",
      "GIZONDE ",
      "RAZONDE ",
      "GRANTS  ",
      "DEBAND  ",
      "JELLEN  ",
      "ZALURE  ",
      "SHIFTA  ",
      "RYUKER  ",
      "RESTA   ",
      "ANTI    ",
      "REVERSER",
      "MEGID   ",
  };

  phosg::fwrite_fmt(stream, "  Technique table:\n");
  phosg::fwrite_fmt(stream, "    TECH                   A1            A2            A3            A4            A5            A6            A7            A8            A9           A10\n");
  for (size_t tech_num = 0; tech_num < this->technique_index_prob_table.size(); tech_num++) {
    phosg::fwrite_fmt(stream, "    {:02X}:{}", tech_num, technique_names[tech_num]);
    for (size_t area_norm = 0; area_norm < 10; area_norm++) {
      uint16_t prob = this->technique_index_prob_table[tech_num][area_norm];
      if (prob) {
        const auto& level_range = this->technique_level_ranges[tech_num][area_norm];
        size_t min_level = level_range.min + 1;
        size_t max_level = level_range.max + 1;
        phosg::fwrite_fmt(stream, "  {:5}[{:2}-{:2}]", prob, min_level, max_level);
      } else {
        phosg::fwrite_fmt(stream, "      0[-----]");
      }
    }
    fputc('\n', stream);
  }

  phosg::fwrite_fmt(stream, "  Armor/shield type bias: {}\n", this->armor_or_shield_type_bias);

  phosg::fwrite_fmt(stream, "  Armor/shield type index table:\n");
  phosg::fwrite_fmt(stream, "    TY  PROB\n");
  for (size_t z = 0; z < 5; z++) {
    phosg::fwrite_fmt(stream, "    {:02X}  {:3}%\n", z, this->armor_shield_type_index_prob_table[z]);
  }

  phosg::fwrite_fmt(stream, "  Armor/shield slot count table:\n");
  phosg::fwrite_fmt(stream, "    #S  PROB\n");
  for (size_t z = 0; z < 5; z++) {
    phosg::fwrite_fmt(stream, "    {:02X}  {:3}%\n", z, this->armor_slot_count_prob_table[z]);
  }

  phosg::fwrite_fmt(stream, "  Unit maximum stars table:\n");
  phosg::fwrite_fmt(stream, "    AR   #*\n");
  for (size_t z = 0; z < 10; z++) {
    phosg::fwrite_fmt(stream, "    {:02X}  {:3}\n", z, this->unit_max_stars_table[z]);
  }
}

void CommonItemSet::Table::print_diff(FILE* stream, const Table& other) const {
  if (this->episode != other.episode) {
    phosg::fwrite_fmt(stream, ">   Episode: {} -> {}\n", name_for_episode(this->episode), name_for_episode(other.episode));
  }
  if (this->base_weapon_type_prob_table != other.base_weapon_type_prob_table) {
    phosg::fwrite_fmt(stream, ">   base_weapon_type_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->base_weapon_type_prob_table, sizeof(this->base_weapon_type_prob_table)),
        phosg::format_data_string(&other.base_weapon_type_prob_table, sizeof(other.base_weapon_type_prob_table)));
  }
  if (this->subtype_base_table != other.subtype_base_table) {
    phosg::fwrite_fmt(stream, ">   subtype_base_table: {} -> {}\n",
        phosg::format_data_string(&this->subtype_base_table, sizeof(this->subtype_base_table)),
        phosg::format_data_string(&other.subtype_base_table, sizeof(other.subtype_base_table)));
  }
  if (this->subtype_area_length_table != other.subtype_area_length_table) {
    phosg::fwrite_fmt(stream, ">   subtype_area_length_table: {} -> {}\n",
        phosg::format_data_string(&this->subtype_area_length_table, sizeof(this->subtype_area_length_table)),
        phosg::format_data_string(&other.subtype_area_length_table, sizeof(other.subtype_area_length_table)));
  }
  if (this->grind_prob_table != other.grind_prob_table) {
    phosg::fwrite_fmt(stream, ">   grind_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->grind_prob_table, sizeof(this->grind_prob_table)),
        phosg::format_data_string(&other.grind_prob_table, sizeof(other.grind_prob_table)));
  }
  if (this->armor_shield_type_index_prob_table != other.armor_shield_type_index_prob_table) {
    phosg::fwrite_fmt(stream, ">   armor_shield_type_index_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->armor_shield_type_index_prob_table, sizeof(this->armor_shield_type_index_prob_table)),
        phosg::format_data_string(&other.armor_shield_type_index_prob_table, sizeof(other.armor_shield_type_index_prob_table)));
  }
  if (this->armor_slot_count_prob_table != other.armor_slot_count_prob_table) {
    phosg::fwrite_fmt(stream, ">   armor_slot_count_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->armor_slot_count_prob_table, sizeof(this->armor_slot_count_prob_table)),
        phosg::format_data_string(&other.armor_slot_count_prob_table, sizeof(other.armor_slot_count_prob_table)));
  }
  if (this->enemy_meseta_ranges != other.enemy_meseta_ranges) {
    phosg::fwrite_fmt(stream, ">   enemy_meseta_ranges: {} -> {}\n",
        phosg::format_data_string(&this->enemy_meseta_ranges, sizeof(this->enemy_meseta_ranges)),
        phosg::format_data_string(&other.enemy_meseta_ranges, sizeof(other.enemy_meseta_ranges)));
  }
  if (this->enemy_type_drop_probs != other.enemy_type_drop_probs) {
    phosg::fwrite_fmt(stream, ">   enemy_type_drop_probs: {} -> {}\n",
        phosg::format_data_string(&this->enemy_type_drop_probs, sizeof(this->enemy_type_drop_probs)),
        phosg::format_data_string(&other.enemy_type_drop_probs, sizeof(other.enemy_type_drop_probs)));
  }
  if (this->enemy_item_classes != other.enemy_item_classes) {
    phosg::fwrite_fmt(stream, ">   enemy_item_classes: {} -> {}\n",
        phosg::format_data_string(&this->enemy_item_classes, sizeof(this->enemy_item_classes)),
        phosg::format_data_string(&other.enemy_item_classes, sizeof(other.enemy_item_classes)));
  }
  if (this->box_meseta_ranges != other.box_meseta_ranges) {
    phosg::fwrite_fmt(stream, ">   box_meseta_ranges: {} -> {}\n",
        phosg::format_data_string(&this->box_meseta_ranges, sizeof(this->box_meseta_ranges)),
        phosg::format_data_string(&other.box_meseta_ranges, sizeof(other.box_meseta_ranges)));
  }
  if (this->has_rare_bonus_value_prob_table != other.has_rare_bonus_value_prob_table) {
    phosg::fwrite_fmt(stream, ">   Has rare bonus value prob table: {} -> {}\n",
        this->has_rare_bonus_value_prob_table ? "true" : "false",
        other.has_rare_bonus_value_prob_table ? "true" : "false");
  }
  if (this->bonus_value_prob_table != other.bonus_value_prob_table) {
    phosg::fwrite_fmt(stream, ">   bonus_value_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->bonus_value_prob_table, sizeof(this->bonus_value_prob_table)),
        phosg::format_data_string(&other.bonus_value_prob_table, sizeof(other.bonus_value_prob_table)));
  }
  if (this->nonrare_bonus_prob_spec != other.nonrare_bonus_prob_spec) {
    phosg::fwrite_fmt(stream, ">   nonrare_bonus_prob_spec: {} -> {}\n",
        phosg::format_data_string(&this->nonrare_bonus_prob_spec, sizeof(this->nonrare_bonus_prob_spec)),
        phosg::format_data_string(&other.nonrare_bonus_prob_spec, sizeof(other.nonrare_bonus_prob_spec)));
  }
  if (this->bonus_type_prob_table != other.bonus_type_prob_table) {
    phosg::fwrite_fmt(stream, ">   bonus_type_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->bonus_type_prob_table, sizeof(this->bonus_type_prob_table)),
        phosg::format_data_string(&other.bonus_type_prob_table, sizeof(other.bonus_type_prob_table)));
  }
  if (this->special_mult != other.special_mult) {
    phosg::fwrite_fmt(stream, ">   special_mult: {} -> {}\n",
        phosg::format_data_string(&this->special_mult, sizeof(this->special_mult)),
        phosg::format_data_string(&other.special_mult, sizeof(other.special_mult)));
  }
  if (this->special_percent != other.special_percent) {
    phosg::fwrite_fmt(stream, ">   special_percent: {} -> {}\n",
        phosg::format_data_string(&this->special_percent, sizeof(this->special_percent)),
        phosg::format_data_string(&other.special_percent, sizeof(other.special_percent)));
  }
  if (this->tool_class_prob_table != other.tool_class_prob_table) {
    phosg::fwrite_fmt(stream, ">   tool_class_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->tool_class_prob_table, sizeof(this->tool_class_prob_table)),
        phosg::format_data_string(&other.tool_class_prob_table, sizeof(other.tool_class_prob_table)));
  }
  if (this->technique_index_prob_table != other.technique_index_prob_table) {
    phosg::fwrite_fmt(stream, ">   technique_index_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->technique_index_prob_table, sizeof(this->technique_index_prob_table)),
        phosg::format_data_string(&other.technique_index_prob_table, sizeof(other.technique_index_prob_table)));
  }
  if (this->technique_level_ranges != other.technique_level_ranges) {
    phosg::fwrite_fmt(stream, ">   technique_level_ranges: {} -> {}\n",
        phosg::format_data_string(&this->technique_level_ranges, sizeof(this->technique_level_ranges)),
        phosg::format_data_string(&other.technique_level_ranges, sizeof(other.technique_level_ranges)));
  }
  if (this->armor_or_shield_type_bias != other.armor_or_shield_type_bias) {
    phosg::fwrite_fmt(stream, ">   Armor/shield type bias: {} -> {}\n",
        this->armor_or_shield_type_bias ? "true" : "false",
        other.armor_or_shield_type_bias ? "true" : "false");
  }
  if (this->unit_max_stars_table != other.unit_max_stars_table) {
    phosg::fwrite_fmt(stream, ">   unit_max_stars_table: {} -> {}\n",
        phosg::format_data_string(&this->unit_max_stars_table, sizeof(this->unit_max_stars_table)),
        phosg::format_data_string(&other.unit_max_stars_table, sizeof(other.unit_max_stars_table)));
  }
  if (this->box_item_class_prob_table != other.box_item_class_prob_table) {
    phosg::fwrite_fmt(stream, ">   box_item_class_prob_table: {} -> {}\n",
        phosg::format_data_string(&this->box_item_class_prob_table, sizeof(this->box_item_class_prob_table)),
        phosg::format_data_string(&other.box_item_class_prob_table, sizeof(other.box_item_class_prob_table)));
  }
}

phosg::JSON CommonItemSet::Table::json() const {
  phosg::JSON enemy_meseta_ranges_json = phosg::JSON::dict();
  phosg::JSON enemy_type_drop_probs_json = phosg::JSON::dict();
  phosg::JSON enemy_item_classes_json = phosg::JSON::dict();
  for (size_t z = 0; z < 0x64; z++) {
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (Episode episode : episodes) {
      auto types = enemy_types_for_rare_table_index(episode, z);
      vector<string> names;
      if (types.empty()) {
        names.emplace_back(std::format("{}:!{:02X}", abbreviation_for_episode(episode), z));
      } else {
        for (auto type : types) {
          names.emplace_back(std::format("{}:{}", abbreviation_for_episode(episode), phosg::name_for_enum(type)));
        }
      }
      for (const auto& name : names) {
        enemy_meseta_ranges_json.emplace(name, to_json(this->enemy_meseta_ranges[z]));
        enemy_type_drop_probs_json.emplace(name, this->enemy_type_drop_probs[z]);
        enemy_item_classes_json.emplace(name, this->enemy_item_classes[z]);
      }
    }
  }
  return phosg::JSON::dict({
      {"BaseWeaponTypeProbTable", to_json(this->base_weapon_type_prob_table)},
      {"SubtypeBaseTable", to_json(this->subtype_base_table)},
      {"SubtypeAreaLengthTable", to_json(this->subtype_area_length_table)},
      {"GrindProbTable", to_json(this->grind_prob_table)},
      {"ArmorShieldTypeIndexProbTable", to_json(this->armor_shield_type_index_prob_table)},
      {"ArmorSlotCountProbTable", to_json(this->armor_slot_count_prob_table)},
      {"EnemyMesetaRanges", std::move(enemy_meseta_ranges_json)},
      {"EnemyTypeDropProbs", std::move(enemy_type_drop_probs_json)},
      {"EnemyItemClasses", std::move(enemy_item_classes_json)},
      {"BoxMesetaRanges", to_json(this->box_meseta_ranges)},
      {"HasRareBonusValueProbTable", this->has_rare_bonus_value_prob_table},
      {"BonusValueProbTable", to_json(this->bonus_value_prob_table)},
      {"NonRareBonusProbSpec", to_json(this->nonrare_bonus_prob_spec)},
      {"BonusTypeProbTable", to_json(this->bonus_type_prob_table)},
      {"SpecialMult", to_json(this->special_mult)},
      {"SpecialPercent", to_json(this->special_percent)},
      {"ToolClassProbTable", to_json(this->tool_class_prob_table)},
      {"TechniqueIndexProbTable", to_json(this->technique_index_prob_table)},
      {"TechniqueLevelRanges", to_json(this->technique_level_ranges)},
      {"ArmorOrShieldTypeBias", this->armor_or_shield_type_bias},
      {"UnitMaxStarsTable", to_json(this->unit_max_stars_table)},
      {"BoxItemClassProbTable", to_json(this->box_item_class_prob_table)},
  });
}

phosg::JSON CommonItemSet::json() const {
  auto modes_dict = phosg::JSON::dict();
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (const auto& mode : modes) {
    auto episodes_dict = phosg::JSON::dict();
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (const auto& episode : episodes) {
      auto difficulty_dict = phosg::JSON::dict();
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        auto section_id_dict = phosg::JSON::dict();
        for (uint8_t section_id = 0; section_id < 10; section_id++) {
          try {
            auto table = this->get_table(episode, mode, difficulty, section_id);
            section_id_dict.emplace(name_for_section_id(section_id), table->json());
          } catch (const runtime_error&) {
          }
        }
        difficulty_dict.emplace(token_name_for_difficulty(difficulty), std::move(section_id_dict));
      }
      episodes_dict.emplace(token_name_for_episode(episode), std::move(difficulty_dict));
    }
    modes_dict.emplace(name_for_mode(mode), std::move(episodes_dict));
  }

  return modes_dict;
}

void CommonItemSet::print(FILE* stream) const {
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (const auto& mode : modes) {
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (const auto& episode : episodes) {
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        for (uint8_t section_id = 0; section_id < 10; section_id++) {
          try {
            auto table = this->get_table(episode, mode, difficulty, section_id);
            phosg::fwrite_fmt(stream, "============ {} {} {} {}\n",
                name_for_mode(mode), name_for_episode(episode), name_for_difficulty(difficulty), name_for_section_id(section_id));
            table->print(stream);
          } catch (const runtime_error&) {
          }
        }
      }
    }
  }
}

void CommonItemSet::print_diff(FILE* stream, const CommonItemSet& other) const {
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (const auto& mode : modes) {
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (const auto& episode : episodes) {
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        for (uint8_t section_id = 0; section_id < 10; section_id++) {
          shared_ptr<const Table> this_table;
          shared_ptr<const Table> other_table;
          try {
            this_table = this->get_table(episode, mode, difficulty, section_id);
          } catch (const runtime_error&) {
          }
          try {
            other_table = other.get_table(episode, mode, difficulty, section_id);
          } catch (const runtime_error&) {
          }

          if (!this_table && !other_table) {
            continue;
          } else if (!this_table) {
            phosg::fwrite_fmt(stream, "> Table present in other but not this: {} {} {} {}\n",
                name_for_mode(mode), name_for_episode(episode), name_for_difficulty(difficulty), name_for_section_id(section_id));
          } else if (!other_table) {
            phosg::fwrite_fmt(stream, "> Table present in this but not other: {} {} {} {}\n",
                name_for_mode(mode), name_for_episode(episode), name_for_difficulty(difficulty), name_for_section_id(section_id));
          } else if (*this_table != *other_table) {
            phosg::fwrite_fmt(stream, "> Tables do not match: {} {} {} {}\n",
                name_for_mode(mode), name_for_episode(episode), name_for_difficulty(difficulty), name_for_section_id(section_id));
            this_table->print_diff(stream, *other_table);
          }
        }
      }
    }
  }
}

CommonItemSet::Table::Table(const phosg::StringReader& r, bool is_big_endian, bool is_v3, Episode episode)
    : episode(episode) {
  if (is_big_endian) {
    this->parse_itempt_t<true>(r, is_v3);
  } else {
    this->parse_itempt_t<false>(r, is_v3);
  }
}

template <bool BE>
void CommonItemSet::Table::parse_itempt_t(const phosg::StringReader& r, bool is_v3) {
  const auto& offsets = r.pget<OffsetsT<BE>>(r.pget<U32T<BE>>(r.size() - 0x10));

  this->base_weapon_type_prob_table = r.pget<parray<uint8_t, 0x0C>>(offsets.base_weapon_type_prob_table_offset);
  this->subtype_base_table = r.pget<parray<int8_t, 0x0C>>(offsets.subtype_base_table_offset);
  this->subtype_area_length_table = r.pget<parray<uint8_t, 0x0C>>(offsets.subtype_area_length_table_offset);
  this->grind_prob_table = r.pget<parray<parray<uint8_t, 4>, 9>>(offsets.grind_prob_table_offset);
  this->armor_shield_type_index_prob_table = r.pget<parray<uint8_t, 0x05>>(offsets.armor_shield_type_index_prob_table_offset);
  this->armor_slot_count_prob_table = r.pget<parray<uint8_t, 0x05>>(offsets.armor_slot_count_prob_table_offset);
  const auto& data = r.pget<parray<Range<U16T<BE>>, 0x64>>(offsets.enemy_meseta_ranges_offset);
  for (size_t z = 0; z < data.size(); z++) {
    this->enemy_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
  }
  this->enemy_type_drop_probs = r.pget<parray<uint8_t, 0x64>>(offsets.enemy_type_drop_probs_offset);
  this->enemy_item_classes = r.pget<parray<uint8_t, 0x64>>(offsets.enemy_item_classes_offset);
  {
    const auto& data = r.pget<parray<Range<U16T<BE>>, 0x0A>>(offsets.box_meseta_ranges_offset);
    for (size_t z = 0; z < data.size(); z++) {
      this->box_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
    }
  }
  this->has_rare_bonus_value_prob_table = is_v3;
  if (!this->has_rare_bonus_value_prob_table) { // V2
    const auto& data = r.pget<parray<parray<uint8_t, 5>, 0x17>>(offsets.bonus_value_prob_table_offset);
    for (size_t z = 0; z < data.size(); z++) {
      for (size_t x = 0; x < data[z].size(); x++) {
        this->bonus_value_prob_table[z][x] = data[z][x];
      }
    }
  } else { // V3
    const auto& data = r.pget<parray<parray<U16T<BE>, 6>, 0x17>>(offsets.bonus_value_prob_table_offset);
    for (size_t z = 0; z < data.size(); z++) {
      for (size_t x = 0; x < data[z].size(); x++) {
        this->bonus_value_prob_table[z][x] = data[z][x];
      }
    }
  }
  this->nonrare_bonus_prob_spec = r.pget<parray<parray<uint8_t, 10>, 3>>(offsets.nonrare_bonus_prob_spec_offset);
  this->bonus_type_prob_table = r.pget<parray<parray<uint8_t, 10>, 6>>(offsets.bonus_type_prob_table_offset);
  this->special_mult = r.pget<parray<uint8_t, 0x0A>>(offsets.special_mult_offset);
  this->special_percent = r.pget<parray<uint8_t, 0x0A>>(offsets.special_percent_offset);
  {
    const auto& data = r.pget<parray<parray<U16T<BE>, 0x0A>, 0x1C>>(offsets.tool_class_prob_table_offset);
    for (size_t z = 0; z < data.size(); z++) {
      for (size_t x = 0; x < data[z].size(); x++) {
        this->tool_class_prob_table[z][x] = data[z][x];
      }
    }
  }
  this->technique_index_prob_table = r.pget<parray<parray<uint8_t, 0x0A>, 0x13>>(offsets.technique_index_prob_table_offset);
  this->technique_level_ranges = r.pget<parray<parray<Range<uint8_t>, 0x0A>, 0x13>>(offsets.technique_level_ranges_offset);
  this->armor_or_shield_type_bias = offsets.armor_or_shield_type_bias;
  this->unit_max_stars_table = r.pget<parray<uint8_t, 0x0A>>(offsets.unit_max_stars_offset);
  this->box_item_class_prob_table = r.pget<parray<parray<uint8_t, 10>, 7>>(offsets.box_item_class_prob_table_offset);
}

uint16_t CommonItemSet::key_for_table(Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) {
  // Bits: -----EEEMMDDSSSS
  return (((static_cast<uint16_t>(episode) << 8) & 0x0700) |
      ((static_cast<uint16_t>(mode) << 6) & 0x00C0) |
      ((static_cast<uint16_t>(difficulty) << 4) & 0x0030) |
      (static_cast<uint16_t>(secid) & 0x000F));
}

shared_ptr<const CommonItemSet::Table> CommonItemSet::get_table(
    Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const {
  try {
    return this->tables.at(this->key_for_table(episode, mode, difficulty, secid));
  } catch (const out_of_range&) {
    throw runtime_error(std::format("common item table not available for episode={}, mode={}, difficulty={}, secid={}",
        name_for_episode(episode), name_for_mode(mode), difficulty, secid));
  }
}

AFSV2CommonItemSet::AFSV2CommonItemSet(
    std::shared_ptr<const std::string> pt_afs_data, std::shared_ptr<const std::string> ct_afs_data) {
  // Each AFS file has 40 entries (30 on v1); the first 10 are for Normal, then
  // Hard, etc.
  {
    AFSArchive pt_afs(pt_afs_data);
    size_t max_difficulty;
    if (pt_afs.num_entries() >= 40) {
      max_difficulty = 4;
    } else if (pt_afs.num_entries() >= 30) {
      max_difficulty = 3;
    } else {
      throw std::runtime_error(std::format("PT AFS file has unexpected entry count ({})", pt_afs.num_entries()));
    }
    for (size_t difficulty = 0; difficulty < max_difficulty; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        auto entry = pt_afs.get(difficulty * 10 + section_id);
        phosg::StringReader r(entry.first, entry.second);
        auto table = make_shared<Table>(r, false, false, Episode::EP1);
        this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::NORMAL, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::BATTLE, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::SOLO, difficulty, section_id), table);
      }
    }
  }

  // ItemCT AFS files also have 40 entries, but only the 0th, 10th, 20th, and
  // 30th are used (section_id is ignored)
  if (ct_afs_data) {
    AFSArchive ct_afs(ct_afs_data);
    size_t max_difficulty;
    if (ct_afs.num_entries() >= 40) {
      max_difficulty = 4;
    } else if (ct_afs.num_entries() >= 30) {
      max_difficulty = 3;
    } else {
      throw std::runtime_error(std::format("CT AFS file has unexpected entry count ({})", ct_afs.num_entries()));
    }
    for (size_t difficulty = 0; difficulty < max_difficulty; difficulty++) {
      auto r = ct_afs.get_reader(difficulty * 10);
      auto table = make_shared<Table>(r, false, false, Episode::EP1);
      for (size_t section_id = 0; section_id < 10; section_id++) {
        this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::CHALLENGE, difficulty, section_id), table);
      }
    }
  }
}

GSLV3V4CommonItemSet::GSLV3V4CommonItemSet(std::shared_ptr<const std::string> gsl_data, bool is_big_endian) {
  GSLArchive gsl(gsl_data, is_big_endian);

  auto filename_for_table = +[](Episode episode, uint8_t difficulty, uint8_t section_id, bool is_challenge) -> string {
    const char* episode_token = "";
    switch (episode) {
      case Episode::EP1:
        episode_token = "";
        break;
      case Episode::EP2:
        episode_token = "l";
        break;
      case Episode::EP4:
        episode_token = "s";
        break;
      default:
        throw runtime_error("invalid episode");
    }
    return std::format(
        "ItemPT{}{}{}{}.rel",
        is_challenge ? "c" : "",
        episode_token,
        static_cast<char>(tolower(abbreviation_for_difficulty(difficulty))),
        section_id);
  };

  vector<Episode> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
  for (Episode episode : episodes) {
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        phosg::StringReader r;
        try {
          r = gsl.get_reader(filename_for_table(episode, difficulty, section_id, false));
        } catch (const exception&) {
          // Fall back to Episode 1 if Episode 4 data is missing
          if (episode == Episode::EP4) {
            auto ep1_table = this->tables.at(this->key_for_table(Episode::EP1, GameMode::NORMAL, difficulty, section_id));
            this->tables.emplace(this->key_for_table(episode, GameMode::NORMAL, difficulty, section_id), ep1_table);
            this->tables.emplace(this->key_for_table(episode, GameMode::BATTLE, difficulty, section_id), ep1_table);
            this->tables.emplace(this->key_for_table(episode, GameMode::SOLO, difficulty, section_id), ep1_table);
            continue;
          } else {
            throw;
          }
        }
        auto table = make_shared<Table>(r, is_big_endian, true, episode);
        this->tables.emplace(this->key_for_table(episode, GameMode::NORMAL, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::BATTLE, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::SOLO, difficulty, section_id), table);
      }
    }

    if (episode != Episode::EP4) {
      for (size_t difficulty = 0; difficulty < 4; difficulty++) {
        try {
          auto r = gsl.get_reader(filename_for_table(episode, difficulty, 0, true));
          auto table = make_shared<Table>(r, is_big_endian, true, episode);
          for (size_t section_id = 0; section_id < 10; section_id++) {
            this->tables.emplace(this->key_for_table(episode, GameMode::CHALLENGE, difficulty, section_id), table);
          }
        } catch (const out_of_range&) {
          // GC NTE doesn't have Ep2 challenge; just skip adding the table
        }
      }
    }
  }
}

JSONCommonItemSet::JSONCommonItemSet(const phosg::JSON& json) {
  for (const auto& mode_it : json.as_dict()) {
    static const unordered_map<string, GameMode> mode_keys(
        {{"Normal", GameMode::NORMAL}, {"Battle", GameMode::BATTLE}, {"Challenge", GameMode::CHALLENGE}, {"Solo", GameMode::SOLO}});
    GameMode mode = mode_keys.at(mode_it.first);

    for (const auto& episode_it : mode_it.second->as_dict()) {
      static const unordered_map<string, Episode> episode_keys(
          {{"Episode1", Episode::EP1}, {"Episode2", Episode::EP2}, {"Episode4", Episode::EP4}});
      Episode episode = episode_keys.at(episode_it.first);

      for (const auto& difficulty_it : episode_it.second->as_dict()) {
        static const unordered_map<string, uint8_t> difficulty_keys(
            {{"Normal", 0}, {"Hard", 1}, {"VeryHard", 2}, {"Ultimate", 3}});
        uint8_t difficulty = difficulty_keys.at(difficulty_it.first);

        for (const auto& section_id_it : difficulty_it.second->as_dict()) {
          uint8_t section_id = section_id_for_name(section_id_it.first);
          this->tables.emplace(
              this->key_for_table(episode, mode, difficulty, section_id),
              make_shared<Table>(*section_id_it.second, episode));
        }
      }
    }
  }
}

RELFileSet::RELFileSet(std::shared_ptr<const std::string> data)
    : data(data), r(*this->data) {}

ArmorRandomSet::ArmorRandomSet(std::shared_ptr<const std::string> data)
    : RELFileSet(data) {
  // For some reason the footer tables are doubly indirect in this file
  uint32_t specs_offset_offset = this->r.pget_u32b(data->size() - 0x10);
  uint32_t specs_offset = this->r.pget_u32b(specs_offset_offset);
  this->tables = &this->r.pget<parray<TableSpec, 3>>(specs_offset);
}

std::pair<const ArmorRandomSet::WeightTableEntry8*, size_t>
ArmorRandomSet::get_armor_table(size_t index) const {
  return this->get_table<WeightTableEntry8>(this->tables->at(0), index);
}

std::pair<const ArmorRandomSet::WeightTableEntry8*, size_t>
ArmorRandomSet::get_shield_table(size_t index) const {
  return this->get_table<WeightTableEntry8>(this->tables->at(1), index);
}

std::pair<const ArmorRandomSet::WeightTableEntry8*, size_t>
ArmorRandomSet::get_unit_table(size_t index) const {
  return this->get_table<WeightTableEntry8>(this->tables->at(2), index);
}

ToolRandomSet::ToolRandomSet(std::shared_ptr<const std::string> data)
    : RELFileSet(data) {
  uint32_t specs_offset = r.pget_u32b(data->size() - 0x10);
  this->common_recovery_table_spec = &r.pget<TableSpec>(r.pget_u32b(specs_offset));
  this->rare_recovery_table_spec = &r.pget<TableSpec>(r.pget_u32b(specs_offset + sizeof(uint32_t)), 2 * sizeof(TableSpec));
  this->tech_disk_table_spec = this->rare_recovery_table_spec + 1;
  this->tech_disk_level_table_spec = &r.pget<TableSpec>(r.pget_u32b(specs_offset + 2 * sizeof(uint32_t)));
}

pair<const uint8_t*, size_t> ToolRandomSet::get_common_recovery_table(size_t index) const {
  return this->get_table<uint8_t>(*this->common_recovery_table_spec, index);
}

pair<const ToolRandomSet::WeightTableEntry8*, size_t>
ToolRandomSet::get_rare_recovery_table(size_t index) const {
  return this->get_table<WeightTableEntry8>(*this->rare_recovery_table_spec, index);
}

pair<const ToolRandomSet::WeightTableEntry8*, size_t>
ToolRandomSet::get_tech_disk_table(size_t index) const {
  return this->get_table<WeightTableEntry8>(*this->tech_disk_table_spec, index);
}

pair<const ToolRandomSet::TechDiskLevelEntry*, size_t>
ToolRandomSet::get_tech_disk_level_table(size_t index) const {
  return this->get_table<TechDiskLevelEntry>(*this->tech_disk_level_table_spec, index);
}

WeaponRandomSet::WeaponRandomSet(std::shared_ptr<const std::string> data)
    : RELFileSet(data) {
  uint32_t offsets_offset = this->r.pget_u32b(data->size() - 0x10);
  this->offsets = &this->r.pget<Offsets>(offsets_offset);
}

std::pair<const WeaponRandomSet::WeightTableEntry8*, size_t>
WeaponRandomSet::get_weapon_type_table(size_t index) const {
  const auto& spec = this->r.pget<TableSpec>(this->offsets->weapon_type_table + index * sizeof(TableSpec));
  const auto* data = &this->r.pget<WeightTableEntry8>(spec.offset, spec.entries_per_table * sizeof(WeightTableEntry8));
  return make_pair(data, spec.entries_per_table);
}

const parray<WeaponRandomSet::WeightTableEntry32, 6>*
WeaponRandomSet::get_bonus_type_table(size_t which, size_t index) const {
  uint32_t base_offset = which ? this->offsets->bonus_type_table2 : this->offsets->bonus_type_table1;
  return &this->r.pget<parray<WeightTableEntry32, 6>>(base_offset + sizeof(parray<WeightTableEntry32, 6>) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_bonus_range(size_t which, size_t index) const {
  uint32_t base_offset = which ? this->offsets->bonus_range_table2 : this->offsets->bonus_range_table1;
  return &this->r.pget<RangeTableEntry>(base_offset + sizeof(RangeTableEntry) * index);
}

const parray<WeaponRandomSet::WeightTableEntry32, 3>*
WeaponRandomSet::get_special_mode_table(size_t index) const {
  return &this->r.pget<parray<WeightTableEntry32, 3>>(
      this->offsets->special_mode_table + sizeof(parray<WeightTableEntry32, 3>) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_standard_grind_range(size_t index) const {
  return &this->r.pget<RangeTableEntry>(this->offsets->standard_grind_range_table + sizeof(RangeTableEntry) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_favored_grind_range(size_t index) const {
  return &this->r.pget<RangeTableEntry>(this->offsets->favored_grind_range_table + sizeof(RangeTableEntry) * index);
}

TekkerAdjustmentSet::TekkerAdjustmentSet(std::shared_ptr<const std::string> data)
    : data(data), r(*data) {
  this->offsets = &this->r.pget<Offsets>(this->r.pget_u32b(this->r.size() - 0x10));
}

const ProbabilityTable<uint8_t, 100>& TekkerAdjustmentSet::get_table(
    std::array<ProbabilityTable<uint8_t, 100>, 10>& tables_default,
    std::array<ProbabilityTable<uint8_t, 100>, 10>& tables_favored,
    uint32_t offset_and_count_offset,
    bool favored,
    uint8_t section_id) const {
  if (section_id >= 10) {
    throw runtime_error("invalid section ID");
  }
  ProbabilityTable<uint8_t, 100>& table = favored ? tables_favored[section_id] : tables_default[section_id];
  if (table.count == 0) {
    uint32_t offset = r.pget_u32b(offset_and_count_offset);
    uint32_t count_per_section_id = r.pget_u32b(offset_and_count_offset + 4);
    auto* entries = &r.pget<DeltaProbabilityEntry>(offset, sizeof(DeltaProbabilityEntry) * count_per_section_id * 10);
    for (size_t z = count_per_section_id * section_id; z < count_per_section_id * (section_id + 1); z++) {
      size_t count = favored ? entries[z].count_favored : entries[z].count_default;
      for (size_t w = 0; w < count; w++) {
        table.push(entries[z].delta_index);
      }
    }
  }
  return table;
}

const ProbabilityTable<uint8_t, 100>& TekkerAdjustmentSet::get_special_upgrade_prob_table(uint8_t section_id, bool favored) const {
  return this->get_table(
      this->special_upgrade_prob_tables_default,
      this->special_upgrade_prob_tables_favored,
      this->offsets->special_upgrade_prob_table_offset,
      favored, section_id);
}

const ProbabilityTable<uint8_t, 100>& TekkerAdjustmentSet::get_grind_delta_prob_table(uint8_t section_id, bool favored) const {
  return this->get_table(
      this->grind_delta_prob_tables_default,
      this->grind_delta_prob_tables_favored,
      this->offsets->grind_delta_prob_table_offset,
      favored, section_id);
}

const ProbabilityTable<uint8_t, 100>& TekkerAdjustmentSet::get_bonus_delta_prob_table(uint8_t section_id, bool favored) const {
  return this->get_table(
      this->bonus_delta_prob_tables_default,
      this->bonus_delta_prob_tables_favored,
      this->offsets->bonus_delta_prob_table_offset,
      favored, section_id);
}

int8_t TekkerAdjustmentSet::get_luck(uint32_t start_offset, uint8_t delta_index) const {
  phosg::StringReader sub_r = r.sub(start_offset);
  while (!sub_r.eof()) {
    const auto& entry = sub_r.get<LuckTableEntry>();
    if (entry.delta_index == 0xFF) {
      return 0;
    } else if (entry.delta_index == delta_index) {
      return entry.luck;
    }
  }
  return 0;
}

int8_t TekkerAdjustmentSet::get_luck_for_special_upgrade(uint8_t delta_index) const {
  return this->get_luck(this->offsets->special_upgrade_luck_table_offset, delta_index);
}

int8_t TekkerAdjustmentSet::get_luck_for_grind_delta(uint8_t delta_index) const {
  return this->get_luck(this->offsets->grind_delta_luck_table_offset, delta_index);
}

int8_t TekkerAdjustmentSet::get_luck_for_bonus_delta(uint8_t delta_index) const {
  return this->get_luck(this->offsets->bonus_delta_luck_offset, delta_index);
}
