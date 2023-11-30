#include "CommonItemSet.hh"

#include "AFSArchive.hh"
#include "GSLArchive.hh"
#include "StaticGameData.hh"

using namespace std;

CommonItemSet::Table::Table(
    shared_ptr<const string> owned_data, const StringReader& r, bool is_big_endian, bool is_v3)
    : owned_data(owned_data),
      r(r),
      is_big_endian(is_big_endian),
      is_v3(is_v3) {
  if (is_big_endian) {
    const auto& be_offsets = r.pget<Offsets<true>>(r.pget_u32b(this->r.size() - 0x10));
    this->offsets.base_weapon_type_prob_table_offset = be_offsets.base_weapon_type_prob_table_offset.load();
    this->offsets.subtype_base_table_offset = be_offsets.subtype_base_table_offset.load();
    this->offsets.subtype_area_length_table_offset = be_offsets.subtype_area_length_table_offset.load();
    this->offsets.grind_prob_table_offset = be_offsets.grind_prob_table_offset.load();
    this->offsets.armor_shield_type_index_prob_table_offset = be_offsets.armor_shield_type_index_prob_table_offset.load();
    this->offsets.armor_slot_count_prob_table_offset = be_offsets.armor_slot_count_prob_table_offset.load();
    this->offsets.enemy_meseta_ranges_offset = be_offsets.enemy_meseta_ranges_offset.load();
    this->offsets.enemy_type_drop_probs_offset = be_offsets.enemy_type_drop_probs_offset.load();
    this->offsets.enemy_item_classes_offset = be_offsets.enemy_item_classes_offset.load();
    this->offsets.box_meseta_ranges_offset = be_offsets.box_meseta_ranges_offset.load();
    this->offsets.bonus_value_prob_table_offset = be_offsets.bonus_value_prob_table_offset.load();
    this->offsets.nonrare_bonus_prob_spec_offset = be_offsets.nonrare_bonus_prob_spec_offset.load();
    this->offsets.bonus_type_prob_table_offset = be_offsets.bonus_type_prob_table_offset.load();
    this->offsets.special_mult_offset = be_offsets.special_mult_offset.load();
    this->offsets.special_percent_offset = be_offsets.special_percent_offset.load();
    this->offsets.tool_class_prob_table_offset = be_offsets.tool_class_prob_table_offset.load();
    this->offsets.technique_index_prob_table_offset = be_offsets.technique_index_prob_table_offset.load();
    this->offsets.technique_level_ranges_offset = be_offsets.technique_level_ranges_offset.load();
    this->offsets.armor_or_shield_type_bias = be_offsets.armor_or_shield_type_bias;
    this->offsets.unit_max_stars_offset = be_offsets.unit_max_stars_offset.load();
    this->offsets.box_item_class_prob_table_offset = be_offsets.box_item_class_prob_table_offset.load();
  } else {
    this->offsets = r.pget<Offsets<false>>(r.pget_u32l(this->r.size() - 0x10));
  }
}

const parray<uint8_t, 0x0C>& CommonItemSet::Table::base_weapon_type_prob_table() const {
  return this->r.pget<parray<uint8_t, 0x0C>>(this->offsets.base_weapon_type_prob_table_offset);
}
const parray<int8_t, 0x0C>& CommonItemSet::Table::subtype_base_table() const {
  return this->r.pget<parray<int8_t, 0x0C>>(this->offsets.subtype_base_table_offset);
}
const parray<uint8_t, 0x0C>& CommonItemSet::Table::subtype_area_length_table() const {
  return this->r.pget<parray<uint8_t, 0x0C>>(this->offsets.subtype_area_length_table_offset);
}
const parray<parray<uint8_t, 4>, 9>& CommonItemSet::Table::grind_prob_table() const {
  return this->r.pget<parray<parray<uint8_t, 4>, 9>>(this->offsets.grind_prob_table_offset);
}
const parray<uint8_t, 0x05>& CommonItemSet::Table::armor_shield_type_index_prob_table() const {
  return this->r.pget<parray<uint8_t, 0x05>>(this->offsets.armor_shield_type_index_prob_table_offset);
}
const parray<uint8_t, 0x05>& CommonItemSet::Table::armor_slot_count_prob_table() const {
  return this->r.pget<parray<uint8_t, 0x05>>(this->offsets.armor_slot_count_prob_table_offset);
}
const parray<CommonItemSet::Table::Range<uint16_t>, 0x64>& CommonItemSet::Table::enemy_meseta_ranges() const {
  if (!this->parsed_enemy_meseta_ranges_populated) {
    if (this->is_big_endian) {
      const auto& data = r.pget<parray<Range<be_uint16_t>, 0x64>>(this->offsets.enemy_meseta_ranges_offset);
      for (size_t z = 0; z < data.size(); z++) {
        this->parsed_enemy_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
      }
    } else {
      const auto& data = r.pget<parray<Range<le_uint16_t>, 0x64>>(this->offsets.enemy_meseta_ranges_offset);
      for (size_t z = 0; z < data.size(); z++) {
        this->parsed_enemy_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
      }
    }
    this->parsed_enemy_meseta_ranges_populated = true;
  }
  return this->parsed_enemy_meseta_ranges;
}
const parray<uint8_t, 0x64>& CommonItemSet::Table::enemy_type_drop_probs() const {
  return this->r.pget<parray<uint8_t, 0x64>>(this->offsets.enemy_type_drop_probs_offset);
}
const parray<uint8_t, 0x64>& CommonItemSet::Table::enemy_item_classes() const {
  return this->r.pget<parray<uint8_t, 0x64>>(this->offsets.enemy_item_classes_offset);
}
const parray<CommonItemSet::Table::Range<uint16_t>, 0x0A>& CommonItemSet::Table::box_meseta_ranges() const {
  if (!this->parsed_box_meseta_ranges_populated) {
    if (this->is_big_endian) {
      const auto& data = r.pget<parray<Range<be_uint16_t>, 0x0A>>(this->offsets.box_meseta_ranges_offset);
      for (size_t z = 0; z < data.size(); z++) {
        this->parsed_box_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
      }
    } else {
      const auto& data = r.pget<parray<Range<le_uint16_t>, 0x0A>>(this->offsets.box_meseta_ranges_offset);
      for (size_t z = 0; z < data.size(); z++) {
        this->parsed_box_meseta_ranges[z] = Range<uint16_t>{data[z].min, data[z].max};
      }
    }
    this->parsed_box_meseta_ranges_populated = true;
  }
  return this->parsed_box_meseta_ranges;
}
bool CommonItemSet::Table::has_rare_bonus_value_prob_table() const {
  return this->is_v3;
}
const parray<parray<uint16_t, 6>, 0x17>& CommonItemSet::Table::bonus_value_prob_table() const {
  if (!this->parsed_bonus_value_prob_table_populated) {
    if (!this->is_v3) { // V2
      const auto& data = r.pget<parray<parray<uint8_t, 5>, 0x17>>(this->offsets.bonus_value_prob_table_offset);
      for (size_t z = 0; z < data.size(); z++) {
        for (size_t x = 0; x < data[z].size(); x++) {
          this->parsed_bonus_value_prob_table[z][x] = data[z][x];
        }
      }
    } else if (this->is_big_endian) { // BE V3
      const auto& data = r.pget<parray<parray<be_uint16_t, 6>, 0x17>>(this->offsets.bonus_value_prob_table_offset);
      for (size_t z = 0; z < data.size(); z++) {
        for (size_t x = 0; x < data[z].size(); x++) {
          this->parsed_bonus_value_prob_table[z][x] = data[z][x];
        }
      }
    } else { // LE V3
      const auto& data = r.pget<parray<parray<le_uint16_t, 6>, 0x17>>(this->offsets.bonus_value_prob_table_offset);
      for (size_t z = 0; z < data.size(); z++) {
        for (size_t x = 0; x < data[z].size(); x++) {
          this->parsed_bonus_value_prob_table[z][x] = data[z][x];
        }
      }
    }
    this->parsed_bonus_value_prob_table_populated = true;
  }
  return this->parsed_bonus_value_prob_table;
}
const parray<parray<uint8_t, 10>, 3>& CommonItemSet::Table::nonrare_bonus_prob_spec() const {
  return this->r.pget<parray<parray<uint8_t, 10>, 3>>(this->offsets.nonrare_bonus_prob_spec_offset);
}
const parray<parray<uint8_t, 10>, 6>& CommonItemSet::Table::bonus_type_prob_table() const {
  return this->r.pget<parray<parray<uint8_t, 10>, 6>>(this->offsets.bonus_type_prob_table_offset);
}
const parray<uint8_t, 0x0A>& CommonItemSet::Table::special_mult() const {
  return this->r.pget<parray<uint8_t, 0x0A>>(this->offsets.special_mult_offset);
}
const parray<uint8_t, 0x0A>& CommonItemSet::Table::special_percent() const {
  return this->r.pget<parray<uint8_t, 0x0A>>(this->offsets.special_percent_offset);
}
const parray<parray<uint16_t, 0x0A>, 0x1C>& CommonItemSet::Table::tool_class_prob_table() const {
  if (!this->parsed_tool_class_prob_table_populated) {
    if (this->is_big_endian) {
      const auto& data = r.pget<parray<parray<be_uint16_t, 0x0A>, 0x1C>>(this->offsets.tool_class_prob_table_offset);
      for (size_t z = 0; z < data.size(); z++) {
        for (size_t x = 0; x < data[z].size(); x++) {
          this->parsed_tool_class_prob_table[z][x] = data[z][x];
        }
      }
    } else {
      const auto& data = r.pget<parray<parray<le_uint16_t, 0x0A>, 0x1C>>(this->offsets.tool_class_prob_table_offset);
      for (size_t z = 0; z < data.size(); z++) {
        for (size_t x = 0; x < data[z].size(); x++) {
          this->parsed_tool_class_prob_table[z][x] = data[z][x];
        }
      }
    }
    this->parsed_tool_class_prob_table_populated = true;
  }
  return this->parsed_tool_class_prob_table;
}
const parray<parray<uint8_t, 0x0A>, 0x13>& CommonItemSet::Table::technique_index_prob_table() const {
  return this->r.pget<parray<parray<uint8_t, 0x0A>, 0x13>>(this->offsets.technique_index_prob_table_offset);
}
const parray<parray<CommonItemSet::Table::Range<uint8_t>, 0x0A>, 0x13>& CommonItemSet::Table::technique_level_ranges() const {
  return this->r.pget<parray<parray<Range<uint8_t>, 0x0A>, 0x13>>(this->offsets.technique_level_ranges_offset);
}
uint8_t CommonItemSet::Table::armor_or_shield_type_bias() const {
  return this->offsets.armor_or_shield_type_bias;
}
const parray<uint8_t, 0x0A>& CommonItemSet::Table::unit_max_stars_table() const {
  return this->r.pget<parray<uint8_t, 0x0A>>(this->offsets.unit_max_stars_offset);
}
const parray<parray<uint8_t, 10>, 7>& CommonItemSet::Table::box_item_class_prob_table() const {
  return this->r.pget<parray<parray<uint8_t, 10>, 7>>(this->offsets.box_item_class_prob_table_offset);
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
      auto table = make_shared<Table>(pt_afs_data, r, false, false);
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
    auto table = make_shared<Table>(ct_afs_data, r, false, false);
    for (size_t section_id = 0; section_id < 10; section_id++) {
      this->tables.emplace(this->key_for_table(Episode::EP1, GameMode::CHALLENGE, difficulty, section_id), table);
    }
  }
}

GSLV3CommonItemSet::GSLV3CommonItemSet(std::shared_ptr<const std::string> gsl_data, bool is_big_endian) {
  GSLArchive gsl(gsl_data, is_big_endian);

  vector<Episode> episodes = {Episode::EP1, Episode::EP2};
  for (Episode episode : episodes) {
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        string filename = string_printf(
            "ItemPT%s%c%1zu.rel",
            ((episode == Episode::EP2) ? "l" : ""),
            tolower(abbreviation_for_difficulty(difficulty)),
            section_id);
        auto r = gsl.get_reader(filename);
        auto table = make_shared<Table>(gsl_data, r, is_big_endian, true);
        this->tables.emplace(this->key_for_table(episode, GameMode::NORMAL, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::BATTLE, difficulty, section_id), table);
        this->tables.emplace(this->key_for_table(episode, GameMode::SOLO, difficulty, section_id), table);
        // TODO: These tables don't exist for Episode 4, and the GC version is
        // the closest we have, so we use the Ep1 data from GC for Ep4.
        if (episode == Episode::EP1) {
          this->tables.emplace(this->key_for_table(Episode::EP4, GameMode::NORMAL, difficulty, section_id), table);
          this->tables.emplace(this->key_for_table(Episode::EP4, GameMode::BATTLE, difficulty, section_id), table);
          this->tables.emplace(this->key_for_table(Episode::EP4, GameMode::SOLO, difficulty, section_id), table);
        }
      }
    }
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      string filename = string_printf(
          "ItemPTc%s%c0.rel",
          ((episode == Episode::EP2) ? "l" : ""),
          tolower(abbreviation_for_difficulty(difficulty)));
      auto r = gsl.get_reader(filename);
      auto table = make_shared<Table>(gsl_data, r, is_big_endian, true);
      for (size_t section_id = 0; section_id < 10; section_id++) {
        this->tables.emplace(this->key_for_table(episode, GameMode::CHALLENGE, difficulty, section_id), table);
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
