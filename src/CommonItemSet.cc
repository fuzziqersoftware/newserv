#include "CommonItemSet.hh"

#include "StaticGameData.hh"

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
      secid);
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
