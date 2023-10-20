#include "CommonItemSet.hh"

#include "StaticGameData.hh"

using namespace std;

CommonItemSet::CommonItemSet(shared_ptr<const string> data)
    : gsl(data, true) {}

const CommonItemSet::Table<true>& CommonItemSet::get_table(
    Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const {
  // TODO: What should we do for Ep4?
  string filename = string_printf(
      "ItemPT%s%s%c%1d.rel",
      ((mode == GameMode::CHALLENGE) ? "c" : ""),
      ((episode == Episode::EP2) ? "l" : ""),
      tolower(abbreviation_for_difficulty(difficulty)),
      (mode == GameMode::CHALLENGE) ? 0 : secid);
  auto data = this->gsl.get(filename);
  if (data.second < sizeof(Table<true>)) {
    throw runtime_error(string_printf(
        "ItemPT entry %s is too small (received %zX bytes, expected %zX bytes)",
        filename.c_str(), data.second, sizeof(Table<true>)));
  }
  return *reinterpret_cast<const Table<true>*>(data.first);
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
