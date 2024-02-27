#include "CommonItemSet.hh"

#include "AFSArchive.hh"
#include "EnemyType.hh"
#include "GSLArchive.hh"
#include "StaticGameData.hh"

using namespace std;

template <typename IntT, size_t Count>
JSON to_json(const parray<IntT, Count>& v) {
  auto ret = JSON::list();
  for (size_t z = 0; z < Count; z++) {
    ret.emplace_back(v[z]);
  }
  return ret;
}

template <typename IntT, size_t Count>
JSON to_json(const parray<CommonItemSet::Table::Range<IntT>, Count>& v) {
  auto ret = JSON::list();
  for (size_t z = 0; z < Count; z++) {
    ret.emplace_back(to_json(v[z]));
  }
  return ret;
}

template <typename IntT>
JSON to_json(const CommonItemSet::Table::Range<IntT>& v) {
  if (v.min == v.max) {
    return JSON(v.min);
  } else {
    return JSON::list({v.min, v.max});
  }
}

template <typename IntT, size_t Count1, size_t Count2>
JSON to_json(const parray<parray<IntT, Count2>, Count1>& v) {
  auto ret = JSON::list();
  for (size_t z = 0; z < Count1; z++) {
    ret.emplace_back(to_json(v[z]));
  }
  return ret;
}

JSON CommonItemSet::Table::json() const {
  JSON enemy_meseta_ranges_json = JSON::dict();
  JSON enemy_type_drop_probs_json = JSON::dict();
  JSON enemy_item_classes_json = JSON::dict();
  for (size_t z = 0; z < 0x64; z++) {
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (Episode episode : episodes) {
      JSON enemy_meseta_ranges_episode_json = JSON::dict();
      JSON enemy_type_drop_probs_episode_json = JSON::dict();
      JSON enemy_item_classes_episode_json = JSON::dict();
      for (auto type : enemy_types_for_rare_table_index(episode, z)) {
        string type_str = name_for_enum(type);
        enemy_meseta_ranges_episode_json.emplace(type_str, to_json(this->enemy_meseta_ranges[z]));
        enemy_type_drop_probs_episode_json.emplace(type_str, this->enemy_type_drop_probs[z]);
        enemy_item_classes_episode_json.emplace(type_str, this->enemy_item_classes[z]);
      }
      string name = name_for_episode(episode);
      enemy_meseta_ranges_json.emplace(name, std::move(enemy_meseta_ranges_episode_json));
      enemy_type_drop_probs_json.emplace(name, std::move(enemy_type_drop_probs_episode_json));
      enemy_item_classes_json.emplace(name, std::move(enemy_item_classes_episode_json));
    }
  }
  return JSON::dict({
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

JSON CommonItemSet::json() const {
  auto modes_dict = JSON::dict();
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (const auto& mode : modes) {
    auto episodes_dict = JSON::dict();
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (const auto& episode : episodes) {
      auto difficulty_dict = JSON::dict();
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        auto section_id_dict = JSON::dict();
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

CommonItemSet::Table::Table(const StringReader& r, bool is_big_endian, bool is_v3) {
  if (is_big_endian) {
    this->parse_itempt_t<true>(r, is_v3);
  } else {
    this->parse_itempt_t<false>(r, is_v3);
  }
}

template <bool IsBigEndian>
void CommonItemSet::Table::parse_itempt_t(const StringReader& r, bool is_v3) {
  using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  const auto& offsets = r.pget<Offsets<IsBigEndian>>(r.pget<U32T>(r.size() - 0x10));

  this->base_weapon_type_prob_table = r.pget<parray<uint8_t, 0x0C>>(offsets.base_weapon_type_prob_table_offset);
  this->subtype_base_table = r.pget<parray<int8_t, 0x0C>>(offsets.subtype_base_table_offset);
  this->subtype_area_length_table = r.pget<parray<uint8_t, 0x0C>>(offsets.subtype_area_length_table_offset);
  this->grind_prob_table = r.pget<parray<parray<uint8_t, 4>, 9>>(offsets.grind_prob_table_offset);
  this->armor_shield_type_index_prob_table = r.pget<parray<uint8_t, 0x05>>(offsets.armor_shield_type_index_prob_table_offset);
  this->armor_slot_count_prob_table = r.pget<parray<uint8_t, 0x05>>(offsets.armor_slot_count_prob_table_offset);
  const auto& data = r.pget<parray<Range<U16T>, 0x64>>(offsets.enemy_meseta_ranges_offset);
  for (size_t z = 0; z < data.size(); z++) {
    this->enemy_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
  }
  this->enemy_type_drop_probs = r.pget<parray<uint8_t, 0x64>>(offsets.enemy_type_drop_probs_offset);
  this->enemy_item_classes = r.pget<parray<uint8_t, 0x64>>(offsets.enemy_item_classes_offset);
  {
    const auto& data = r.pget<parray<Range<U16T>, 0x0A>>(offsets.box_meseta_ranges_offset);
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
    const auto& data = r.pget<parray<parray<U16T, 6>, 0x17>>(offsets.bonus_value_prob_table_offset);
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
    const auto& data = r.pget<parray<parray<U16T, 0x0A>, 0x1C>>(offsets.tool_class_prob_table_offset);
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

void CommonItemSet::Table::print_enemy_table(FILE* stream) const {
  const auto& meseta_ranges = this->enemy_meseta_ranges;
  const auto& drop_probs = this->enemy_type_drop_probs;
  const auto& item_classes = this->enemy_item_classes;
  // const parray<Range<uint16_t>, 0x64>& enemy_meseta_ranges() const;
  //   const parray<uint8_t, 0x64>& enemy_type_drop_probs() const;
  //   const parray<uint8_t, 0x64>& enemy_item_classes() const;
  fprintf(stream, "##  $_LOW  $_HIGH  DAR  ITEM\n");
  for (size_t z = 0; z < 0x64; z++) {
    const char* item_class_name = "__UNKNOWN__";
    switch (item_classes[z]) {
      case 0x00:
        item_class_name = "WEAPON";
        break;
      case 0x01:
        item_class_name = "ARMOR";
        break;
      case 0x02:
        item_class_name = "SHIELD";
        break;
      case 0x03:
        item_class_name = "UNIT";
        break;
      case 0x04:
        item_class_name = "TOOL";
        break;
      case 0x05:
        item_class_name = "MESETA";
        break;
    }
    fprintf(stream, "%02zX  %5hu   %5hu  %3hhu  %02hX (%s)\n",
        z, meseta_ranges[z].min, meseta_ranges[z].max, drop_probs[z], item_classes[z], item_class_name);
  }
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
    throw runtime_error(string_printf("common item table not available for episode=%s, mode=%s, difficulty=%hu, secid=%hu",
        name_for_episode(episode), name_for_mode(mode), difficulty, secid));
  }
}

AFSV2CommonItemSet::AFSV2CommonItemSet(
    std::shared_ptr<const std::string> pt_afs_data,
    std::shared_ptr<const std::string> ct_afs_data) {
  // ItemPT.afs has 40 entries; the first 10 are for Normal, then Hard, etc.
  AFSArchive pt_afs(pt_afs_data);
  for (size_t difficulty = 0; difficulty < 4; difficulty++) {
    for (size_t section_id = 0; section_id < 10; section_id++) {
      auto entry = pt_afs.get(difficulty * 10 + section_id);
      StringReader r(entry.first, entry.second);
      auto table = make_shared<Table>(r, false, false);
      this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::NORMAL, difficulty, section_id), table);
      this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::BATTLE, difficulty, section_id), table);
      this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::SOLO, difficulty, section_id), table);
    }
  }

  // ItemCT.afs also has 40 entries, but only the 0th, 10th, 20th, and 30th are
  // used (section_id is ignored)
  AFSArchive ct_afs(ct_afs_data);
  for (size_t difficulty = 0; difficulty < 4; difficulty++) {
    auto r = ct_afs.get_reader(difficulty * 10);
    auto table = make_shared<Table>(r, false, false);
    for (size_t section_id = 0; section_id < 10; section_id++) {
      this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::CHALLENGE, difficulty, section_id), table);
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
    return string_printf(
        "ItemPT%s%s%c%1hhu.rel",
        is_challenge ? "c" : "",
        episode_token,
        tolower(abbreviation_for_difficulty(difficulty)),
        section_id);
  };

  vector<Episode> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
  for (Episode episode : episodes) {
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        StringReader r;
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
        auto table = make_shared<Table>(r, is_big_endian, true);
        this->tables.emplace(this->key_for_table(episode, GameMode::NORMAL, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::BATTLE, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::SOLO, difficulty, section_id), table);
      }
    }

    if (episode != Episode::EP4) {
      for (size_t difficulty = 0; difficulty < 4; difficulty++) {
        auto r = gsl.get_reader(filename_for_table(episode, difficulty, 0, true));
        auto table = make_shared<Table>(r, is_big_endian, true);
        for (size_t section_id = 0; section_id < 10; section_id++) {
          this->tables.emplace(this->key_for_table(episode, GameMode::CHALLENGE, difficulty, section_id), table);
        }
      }
    }
  }
}

RELFileSet::RELFileSet(std::shared_ptr<const std::string> data)
    : data(data),
      r(*this->data) {}

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
  this->common_recovery_table_spec = &r.pget<TableSpec>(r.pget_u32b(
      specs_offset));
  this->rare_recovery_table_spec = &r.pget<TableSpec>(
      r.pget_u32b(specs_offset + sizeof(uint32_t)),
      2 * sizeof(TableSpec));
  this->tech_disk_table_spec = this->rare_recovery_table_spec + 1;
  this->tech_disk_level_table_spec = &r.pget<TableSpec>(r.pget_u32b(
      specs_offset + 2 * sizeof(uint32_t)));
}

pair<const uint8_t*, size_t> ToolRandomSet::get_common_recovery_table(
    size_t index) const {
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
  const auto& spec = this->r.pget<TableSpec>(
      this->offsets->weapon_type_table + index * sizeof(TableSpec));
  const auto* data = &this->r.pget<WeightTableEntry8>(
      spec.offset, spec.entries_per_table * sizeof(WeightTableEntry8));
  return make_pair(data, spec.entries_per_table);
}

const parray<WeaponRandomSet::WeightTableEntry32, 6>*
WeaponRandomSet::get_bonus_type_table(size_t which, size_t index) const {
  uint32_t base_offset = which ? this->offsets->bonus_type_table2 : this->offsets->bonus_type_table1;
  return &this->r.pget<parray<WeightTableEntry32, 6>>(
      base_offset + sizeof(parray<WeightTableEntry32, 6>) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_bonus_range(size_t which, size_t index) const {
  uint32_t base_offset = which ? this->offsets->bonus_range_table2 : this->offsets->bonus_range_table1;
  return &this->r.pget<RangeTableEntry>(
      base_offset + sizeof(RangeTableEntry) * index);
}

const parray<WeaponRandomSet::WeightTableEntry32, 3>*
WeaponRandomSet::get_special_mode_table(size_t index) const {
  return &this->r.pget<parray<WeightTableEntry32, 3>>(
      this->offsets->special_mode_table + sizeof(parray<WeightTableEntry32, 3>) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_standard_grind_range(size_t index) const {
  return &this->r.pget<RangeTableEntry>(
      this->offsets->standard_grind_range_table + sizeof(RangeTableEntry) * index);
}

const WeaponRandomSet::RangeTableEntry*
WeaponRandomSet::get_favored_grind_range(size_t index) const {
  return &this->r.pget<RangeTableEntry>(
      this->offsets->favored_grind_range_table + sizeof(RangeTableEntry) * index);
}

TekkerAdjustmentSet::TekkerAdjustmentSet(std::shared_ptr<const std::string> data)
    : data(data),
      r(*data) {
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
  StringReader sub_r = r.sub(start_offset);
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
