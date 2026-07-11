#include "ShopRandomSets.hh"

#include "CommonFileFormats.hh"
#include "StaticGameData.hh"
#include "Types.hh"

template <bool BE>
struct TableSpecT {
  U32T<BE> offset;
  uint8_t row_size;
  parray<uint8_t, 3> unused;
} __packed_ws_be__(TableSpecT, 8);

template <typename T>
void print_table_2d(FILE* stream, const std::vector<std::vector<T>>& table) {
  for (size_t z = 0; z < table.size(); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}:", z);
    for (const auto& cell : table[z]) {
      if constexpr (std::is_integral_v<T>) {
        phosg::fwrite_fmt(stream, "   {:02X}", cell);
      } else {
        // It should be ShopRandomSetBase::IntPairT<...>
        phosg::fwrite_fmt(stream, "   {:02X} @ {:03}", cell.first, cell.second);
      }
    }
    phosg::fwrite_fmt(stream, "\n");
  }
}

template <typename ParsedT, typename StoredT = ParsedT>
std::vector<std::vector<ParsedT>> parse_table_t(
    const phosg::StringReader& r, uint32_t offset, size_t row_size, const std::set<uint32_t> start_offsets) {
  auto end_offset_it = start_offsets.upper_bound(offset);
  if (end_offset_it == start_offsets.end()) {
    throw std::runtime_error("Cannot determine table end offset");
  }
  uint32_t end_offset = *end_offset_it;

  size_t row_bytes = row_size * sizeof(StoredT);
  size_t row_count = (end_offset - offset) / row_bytes;
  auto sub_r = r.sub(offset, row_bytes * row_count);

  std::vector<std::vector<ParsedT>> ret;
  while (ret.size() < row_count) {
    auto& row = ret.emplace_back();
    while (row.size() < row_size) {
      row.emplace_back(sub_r.get<StoredT>());
    }
  }
  return ret;
}

template <typename StoredT, typename ParsedT = StoredT, bool BE>
TableSpecT<BE> serialize_table_t(RELFileWriter<BE>& rel, const std::vector<std::vector<ParsedT>>& table) {
  if (table.empty()) {
    throw std::runtime_error("Table is empty");
  }

  TableSpecT<BE> ret;
  ret.offset = rel.w.size();
  ret.row_size = table[0].size();
  for (const auto& row : table) {
    if (row.size() != ret.row_size) {
      throw std::runtime_error("Table has different row sizes");
    }
    for (const auto& cell : row) {
      rel.template put<StoredT>(cell);
    }
  }
  return ret;
}

template <size_t RowCount, size_t RowSize, typename ParsedT, typename StoredT = ParsedT>
std::array<std::array<ParsedT, RowSize>, RowCount> parse_fixed_table_t(const phosg::StringReader& r, uint32_t offset) {
  auto sub_r = r.sub(offset, RowSize * RowCount * sizeof(StoredT));
  std::array<std::array<ParsedT, RowSize>, RowCount> ret;
  for (size_t y = 0; y < RowCount; y++) {
    for (size_t x = 0; x < RowSize; x++) {
      ret[y][x] = sub_r.get<StoredT>();
    }
  }
  return ret;
}

template <size_t RowCount, size_t RowSize, typename ParsedT, typename StoredT = ParsedT, bool BE>
uint32_t serialize_fixed_table_t(
    RELFileWriter<BE>& rel, const std::array<std::array<ParsedT, RowSize>, RowCount>& table) {
  uint32_t ret = rel.w.size();
  for (const auto& row : table) {
    for (const auto& cell : row) {
      rel.template put<StoredT>(cell);
    }
  }
  return ret;
}

template <typename T>
std::vector<std::vector<T>> table_for_json_t(const phosg::JSON& table_json) {
  std::vector<std::vector<T>> ret;
  for (const auto& row_json : table_json.as_list()) {
    auto& row = ret.emplace_back();
    for (const auto& cell_json : row_json->as_list()) {
      if constexpr (std::is_integral_v<T>) {
        row.emplace_back(cell_json->as_int());
      } else {
        row.emplace_back(*cell_json);
      }
    }
  }
  return ret;
}

template <typename T>
phosg::JSON json_for_table_t(const std::vector<std::vector<T>>& table) {
  auto table_json = phosg::JSON::list();
  for (const auto& row : table) {
    auto row_json = phosg::JSON::list();
    for (const auto& cell : row) {
      if constexpr (std::is_integral_v<T>) {
        row_json.emplace_back(cell);
      } else {
        row_json.emplace_back(cell.json());
      }
    }
    table_json.emplace_back(std::move(row_json));
  }
  return table_json;
}

template <typename T, size_t Count>
std::array<T, Count> fixed_table_for_json_t(const phosg::JSON& table_json) {
  std::array<T, Count> ret;
  for (size_t y = 0; y < Count; y++) {
    ret[y] = table_json.at(y);
  }
  return ret;
}

template <typename T, size_t RowCount, size_t RowSize>
std::array<std::array<T, RowSize>, RowCount> fixed_table_for_json_t(const phosg::JSON& table_json) {
  std::array<std::array<T, RowSize>, RowCount> ret;
  for (size_t y = 0; y < RowCount; y++) {
    const auto& row_json = table_json.at(y);
    for (size_t x = 0; x < RowSize; x++) {
      ret[y][x] = row_json.at(x);
    }
  }
  return ret;
}

template <typename T, size_t Count>
phosg::JSON json_for_fixed_table_t(const std::array<T, Count>& table) {
  auto ret = phosg::JSON::list();
  for (const auto& cell : table) {
    ret.emplace_back(cell.json());
  }
  return ret;
}

template <typename T, size_t RowCount, size_t RowSize>
phosg::JSON json_for_fixed_table_t(const std::array<std::array<T, RowSize>, RowCount>& table) {
  auto table_json = phosg::JSON::list();
  for (const auto& row : table) {
    auto row_json = phosg::JSON::list();
    for (const auto& cell : row) {
      row_json.emplace_back(cell.json());
    }
    table_json.emplace_back(std::move(row_json));
  }
  return table_json;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Armor shop set

template <bool BE>
struct ArmorSubRootT {
  TableSpecT<BE> armor_table; // -> WeightTableEntry8[...][...]
  TableSpecT<BE> shield_table; // -> WeightTableEntry8[...][...]
  TableSpecT<BE> unit_table; // -> WeightTableEntry8[...][...]
} __packed_ws_be__(ArmorSubRootT, 0x18);

template <bool BE>
struct ArmorRootT {
  U32T<BE> subroot; // -> ArmorSubRootT<BE>
} __packed_ws_be__(ArmorRootT, 4);

ArmorShopRandomSet::ArmorShopRandomSet(const void* data, size_t size, bool big_endian) {
  if (big_endian) {
    this->parse_t<true>(data, size);
  } else {
    this->parse_t<false>(data, size);
  }
}

ArmorShopRandomSet::ArmorShopRandomSet(const std::string& data, bool big_endian)
    : ArmorShopRandomSet(data.data(), data.size(), big_endian) {}

ArmorShopRandomSet::ArmorShopRandomSet(const phosg::JSON& json) {
  this->armor_table = table_for_json_t<IntPairT<uint8_t>>(json.at("ArmorTable"));
  this->shield_table = table_for_json_t<IntPairT<uint8_t>>(json.at("ShieldTable"));
  this->unit_table = table_for_json_t<IntPairT<uint8_t>>(json.at("UnitTable"));
}

template <bool BE>
void ArmorShopRandomSet::parse_t(const void* data, size_t size) {
  std::set<uint32_t> start_offsets;

  phosg::StringReader r(data, size);
  uint32_t root_offset = r.pget<U32T<BE>>(size - 0x10);
  start_offsets.emplace(root_offset);
  const auto& root = r.pget<ArmorRootT<BE>>(root_offset);
  start_offsets.emplace(root.subroot);
  const auto& subroot = r.pget<ArmorSubRootT<BE>>(root.subroot);
  start_offsets.emplace(subroot.armor_table.offset);
  start_offsets.emplace(subroot.shield_table.offset);
  start_offsets.emplace(subroot.unit_table.offset);

  this->armor_table = parse_table_t<IntPairT<uint8_t>>(
      r, subroot.armor_table.offset, subroot.armor_table.row_size, start_offsets);
  this->shield_table = parse_table_t<IntPairT<uint8_t>>(
      r, subroot.shield_table.offset, subroot.shield_table.row_size, start_offsets);
  this->unit_table = parse_table_t<IntPairT<uint8_t>>(
      r, subroot.unit_table.offset, subroot.unit_table.row_size, start_offsets);
}

template <bool BE>
std::string ArmorShopRandomSet::serialize_binary_t() const {
  RELFileWriter<BE> rel;

  ArmorSubRootT<BE> subroot;
  subroot.armor_table = serialize_table_t<IntPairT<uint8_t>>(rel, this->armor_table);
  subroot.shield_table = serialize_table_t<IntPairT<uint8_t>>(rel, this->shield_table);
  subroot.unit_table = serialize_table_t<IntPairT<uint8_t>>(rel, this->unit_table);

  ArmorRootT<BE> root;
  rel.align(4);
  root.subroot = rel.put(subroot);
  rel.relocations.emplace(rel.w.size() - 0x18);
  rel.relocations.emplace(rel.w.size() - 0x10);
  rel.relocations.emplace(rel.w.size() - 0x08);

  uint32_t root_offset = rel.put(root);
  rel.relocations.emplace(rel.w.size() - 4);

  return rel.finalize(root_offset);
}

std::string ArmorShopRandomSet::serialize_binary(bool big_endian) const {
  return big_endian ? this->serialize_binary_t<true>() : this->serialize_binary_t<false>();
}

phosg::JSON ArmorShopRandomSet::json() const {
  return phosg::JSON::dict({
      {"ArmorTable", json_for_table_t(this->armor_table)},
      {"ShieldTable", json_for_table_t(this->shield_table)},
      {"UnitTable", json_for_table_t(this->unit_table)},
  });
}

void ArmorShopRandomSet::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "ArmorShopRandomSet\n");
  phosg::fwrite_fmt(stream, "  Armor table\n");
  print_table_2d(stream, this->armor_table);
  phosg::fwrite_fmt(stream, "  Shield table\n");
  print_table_2d(stream, this->shield_table);
  phosg::fwrite_fmt(stream, "  Unit table\n");
  print_table_2d(stream, this->unit_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tool shop set

ToolShopRandomSet::TechDiskLevelEntry::TechDiskLevelEntry(const phosg::JSON& json) {
  if (json.contains("PlayerLevelDivisor")) {
    this->mode = Mode::PLAYER_LEVEL_DIVISOR;
    this->player_level_divisor_or_min_level = json.get_int("PlayerLevelDivisor");
    this->max_level = 0;
  } else if (json.contains("MinLevel")) {
    this->mode = Mode::RANDOM_IN_RANGE;
    this->player_level_divisor_or_min_level = json.get_int("MinLevel");
    this->max_level = json.get_int("MaxLevel");
  } else {
    this->mode = Mode::LEVEL_1;
    this->player_level_divisor_or_min_level = 0;
    this->max_level = 0;
  }
}

phosg::JSON ToolShopRandomSet::TechDiskLevelEntry::json() const {
  switch (this->mode) {
    case Mode::LEVEL_1:
      return phosg::JSON::dict();
    case Mode::PLAYER_LEVEL_DIVISOR:
      return phosg::JSON::dict({{"PlayerLevelDivisor", this->player_level_divisor_or_min_level}});
    case Mode::RANDOM_IN_RANGE:
      return phosg::JSON::dict({{"MinLevel", this->player_level_divisor_or_min_level}, {"MaxLevel", this->max_level}});
    default:
      throw std::runtime_error("Invalid TechDiskLevelEntry mode");
  }
}

template <bool BE>
struct ToolSubRootT {
  TableSpecT<BE> rare_recovery_table; // -> WeightTableEntry8[...][...]
  TableSpecT<BE> tech_disk_table; // -> WeightTableEntry8[...][...]
} __packed_ws_be__(ToolSubRootT, 0x10);

template <bool BE>
struct ToolRootT {
  U32T<BE> common_recovery_table; // -> TableSpecT<BE> -> WeightTableEntry8[...][...]
  U32T<BE> subroot; // -> ToolSubRootT<BE>
  U32T<BE> tech_disk_level_table; // -> TableSpecT<BE> -> TechDiskLevelEntry[...][...]
} __packed_ws_be__(ToolRootT, 0x0C);

ToolShopRandomSet::ToolShopRandomSet(const void* data, size_t size, bool big_endian) {
  if (big_endian) {
    this->parse_t<true>(data, size);
  } else {
    this->parse_t<false>(data, size);
  }
}

const std::vector<std::pair<uint8_t, uint8_t>> ToolShopRandomSet::item_defs{
    {0x00, 0x00}, // 00 -> Monomate
    {0x00, 0x01}, // 01 -> Dimate
    {0x00, 0x02}, // 02 -> Trimate
    {0x01, 0x00}, // 03 -> Monofluid
    {0x01, 0x01}, // 04 -> Difluid
    {0x01, 0x02}, // 05 -> Trifluid
    {0x06, 0x00}, // 06 -> Antidote
    {0x06, 0x01}, // 07 -> Antiparalysis
    {0x03, 0x00}, // 08 -> Sol Atomizer
    {0x04, 0x00}, // 09 -> Moon Atomizer
    {0x05, 0x00}, // 0A -> Star Atomizer
    {0x07, 0x00}, // 0B -> Telepipe
    {0x08, 0x00}, // 0C -> Trap Vision
    {0x09, 0x00}, // 0D -> Scape Doll
    {0x0A, 0x00}, // 0E -> Monogrinder
    {0xFF, 0xFF}, // 0F -> Nothing
};

const std::array<uint8_t, 0x13> ToolShopRandomSet::tech_num_map{
    0x00, 0x03, 0x06, 0x0F, 0x10, 0x0D, 0x0A, 0x0B, 0x0C, 0x01, 0x04, 0x07, 0x0E, 0x11, 0x02, 0x05, 0x08, 0x09, 0x12};

ToolShopRandomSet::ToolShopRandomSet(const std::string& data, bool big_endian)
    : ToolShopRandomSet(data.data(), data.size(), big_endian) {}

ToolShopRandomSet::ToolShopRandomSet(const phosg::JSON& json) {
  this->common_recovery_table = table_for_json_t<uint8_t>(json.at("CommonRecoveryTable"));
  this->rare_recovery_table = table_for_json_t<IntPairT<uint8_t>>(json.at("RareRecoveryTable"));
  this->tech_disk_table = table_for_json_t<IntPairT<uint8_t>>(json.at("TechDiskTable"));
  this->tech_disk_level_table = table_for_json_t<TechDiskLevelEntry>(json.at("TechDiskLevelTable"));
}

template <bool BE>
void ToolShopRandomSet::parse_t(const void* data, size_t size) {
  std::set<uint32_t> start_offsets;

  phosg::StringReader r(data, size);
  uint32_t root_offset = r.pget<U32T<BE>>(size - 0x10);
  start_offsets.emplace(root_offset);

  const auto& root = r.pget<ToolRootT<BE>>(root_offset);
  start_offsets.emplace(root.common_recovery_table);
  start_offsets.emplace(root.subroot);
  start_offsets.emplace(root.tech_disk_level_table);

  const auto& common_recovery_table_spec = r.pget<TableSpecT<BE>>(root.common_recovery_table);
  start_offsets.emplace(common_recovery_table_spec.offset);

  const auto& subroot = r.pget<ToolSubRootT<BE>>(root.subroot);
  start_offsets.emplace(subroot.rare_recovery_table.offset);
  start_offsets.emplace(subroot.tech_disk_table.offset);

  const auto& tech_disk_level_table_spec = r.pget<TableSpecT<BE>>(root.tech_disk_level_table);
  start_offsets.emplace(tech_disk_level_table_spec.offset);

  this->common_recovery_table = parse_table_t<uint8_t>(
      r, common_recovery_table_spec.offset, common_recovery_table_spec.row_size, start_offsets);
  this->rare_recovery_table = parse_table_t<IntPairT<uint8_t>>(
      r, subroot.rare_recovery_table.offset, subroot.rare_recovery_table.row_size, start_offsets);
  this->tech_disk_table = parse_table_t<IntPairT<uint8_t>>(
      r, subroot.tech_disk_table.offset, subroot.tech_disk_table.row_size, start_offsets);
  this->tech_disk_level_table = parse_table_t<TechDiskLevelEntry>(
      r, tech_disk_level_table_spec.offset, tech_disk_level_table_spec.row_size, start_offsets);
}

template <bool BE>
std::string ToolShopRandomSet::serialize_binary_t() const {
  RELFileWriter<BE> rel;

  ToolSubRootT<BE> subroot;
  auto common_recovery_table_spec = serialize_table_t<uint8_t>(rel, this->common_recovery_table);
  subroot.rare_recovery_table = serialize_table_t<IntPairT<uint8_t>>(rel, this->rare_recovery_table);
  subroot.tech_disk_table = serialize_table_t<IntPairT<uint8_t>>(rel, this->tech_disk_table);
  auto tech_disk_level_table_spec = serialize_table_t<TechDiskLevelEntry>(rel, this->tech_disk_level_table);

  rel.align(4);
  ToolRootT<BE> root;
  root.subroot = rel.put(subroot);
  rel.relocations.emplace(rel.w.size() - 0x10);
  rel.relocations.emplace(rel.w.size() - 0x08);
  root.common_recovery_table = rel.put(common_recovery_table_spec);
  rel.relocations.emplace(rel.w.size() - 0x08);
  root.tech_disk_level_table = rel.put(tech_disk_level_table_spec);
  rel.relocations.emplace(rel.w.size() - 0x08);

  uint32_t root_offset = rel.put(root);
  rel.relocations.emplace(rel.w.size() - 0x0C);
  rel.relocations.emplace(rel.w.size() - 0x08);
  rel.relocations.emplace(rel.w.size() - 0x04);

  return rel.finalize(root_offset);
}

std::string ToolShopRandomSet::serialize_binary(bool big_endian) const {
  return big_endian ? this->serialize_binary_t<true>() : this->serialize_binary_t<false>();
}

phosg::JSON ToolShopRandomSet::json() const {
  return phosg::JSON::dict({
      {"CommonRecoveryTable", json_for_table_t(this->common_recovery_table)},
      {"RareRecoveryTable", json_for_table_t(this->rare_recovery_table)},
      {"TechDiskTable", json_for_table_t(this->tech_disk_table)},
      {"TechDiskLevelTable", json_for_table_t(this->tech_disk_level_table)},
  });
}

void ToolShopRandomSet::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "ToolShopRandomSet\n");

  phosg::fwrite_fmt(stream, "  Common recovery table\n");
  for (size_t z = 0; z < this->common_recovery_table.size(); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}:", z);
    for (uint8_t cell : this->common_recovery_table[z]) {
      if (cell == 0x0F) {
        phosg::fwrite_fmt(stream, "   {:02X} (------)", cell);
      } else {
        const auto& def = this->item_defs.at(cell);
        phosg::fwrite_fmt(stream, "   {:02X} (03{:02X}{:02X})", cell, def.first, def.second);
      }
    }
    phosg::fwrite_fmt(stream, "\n");
  }

  phosg::fwrite_fmt(stream, "  Rare recovery table\n");
  for (size_t z = 0; z < this->rare_recovery_table.size(); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}:", z);
    for (const auto& cell : this->rare_recovery_table[z]) {
      if (cell.value == 0x0F) {
        phosg::fwrite_fmt(stream, "   {:02X} (------) @ {:03}", cell.value, cell.weight);
      } else {
        const auto& def = this->item_defs.at(cell.value);
        phosg::fwrite_fmt(stream, "   {:02X} (03{:02X}{:02X}) @ {:03}", cell.value, def.first, def.second, cell.weight);
      }
    }
    phosg::fwrite_fmt(stream, "\n");
  }

  phosg::fwrite_fmt(stream, "  Tech disk table\n");
  for (size_t z = 0; z < this->tech_disk_table.size(); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}:", z);
    for (const auto& cell : this->tech_disk_table[z]) {
      phosg::fwrite_fmt(stream, "   {:02X}({:>8}) @ {:03}",
          cell.first, name_for_technique(this->tech_num_map.at(cell.value)), cell.second);
    }
    phosg::fwrite_fmt(stream, "\n");
  }

  phosg::fwrite_fmt(stream, "  Tech disk level table\n");
  phosg::fwrite_fmt(stream, "       ");
  for (const auto& tech_num : this->tech_num_map) {
    phosg::fwrite_fmt(stream, "   {:<9}", name_for_technique(tech_num));
  }
  phosg::fwrite_fmt(stream, "\n");
  for (size_t z = 0; z < this->tech_disk_level_table.size(); z++) {
    phosg::fwrite_fmt(stream, "    {:02X}:", z);
    for (const auto& cell : this->tech_disk_level_table[z]) {
      switch (cell.mode) {
        case TechDiskLevelEntry::Mode::LEVEL_1:
          phosg::fwrite_fmt(stream, "   LEVEL_1  ");
          break;
        case TechDiskLevelEntry::Mode::PLAYER_LEVEL_DIVISOR:
          phosg::fwrite_fmt(stream, "   PLV / {:03}", cell.player_level_divisor_or_min_level);
          break;
        case TechDiskLevelEntry::Mode::RANDOM_IN_RANGE:
          phosg::fwrite_fmt(stream, "   {:03} - {:03}", cell.player_level_divisor_or_min_level, cell.max_level);
          break;
      }
    }
    phosg::fwrite_fmt(stream, "\n");
  }
  std::vector<std::vector<TechDiskLevelEntry>> tech_disk_level_table;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Weapon shop set

template <bool BE>
struct RangeTableEntryT {
  U32T<BE> min;
  U32T<BE> max;
} __packed_ws_be__(RangeTableEntryT, 8);

template <bool BE>
struct WeaponRootT {
  U32T<BE> weapon_type_table; // {c, o -> (table)}[...(offsets)]
  U32T<BE> bonus_type_table1; // {u32 value, u32 weight}[9][6]
  U32T<BE> bonus_type_table2; // {u32 value, u32 weight}[9][6]
  U32T<BE> bonus_range_table1; // {u32 min_index, u32 max_index}[9]
  U32T<BE> bonus_range_table2; // {u32 min_index, u32 max_index}[9]
  U32T<BE> special_mode_table; // {u32 value, u32 weight}[8][3]
  U32T<BE> default_grind_range_table; // {u32 min, u32 max}[6]
  U32T<BE> favored_grind_range_table; // {u32 min, u32 max}[6]
} __packed_ws_be__(WeaponRootT, 0x20);

const std::array<std::pair<uint8_t, uint8_t>, 0x48> WeaponShopRandomSet::type_defs{{
    /* 00 */ {0x01, 0x00}, // Saber
    /* 01 */ {0x01, 0x01}, // Brand
    /* 02 */ {0x01, 0x02}, // Buster
    /* 03 */ {0x01, 0x03}, // Pallasch
    /* 04 */ {0x01, 0x04}, // Gladius
    /* 05 */ {0x03, 0x00}, // Dagger
    /* 06 */ {0x03, 0x01}, // Knife
    /* 07 */ {0x03, 0x02}, // Blade
    /* 08 */ {0x03, 0x03}, // Edge
    /* 09 */ {0x03, 0x04}, // Ripper
    /* 0A */ {0x02, 0x00}, // Sword
    /* 0B */ {0x02, 0x01}, // Gigush
    /* 0C */ {0x02, 0x02}, // Breaker
    /* 0D */ {0x02, 0x03}, // Claymore
    /* 0E */ {0x02, 0x04}, // Calibur
    /* 0F */ {0x05, 0x00}, // Slicer
    /* 10 */ {0x05, 0x01}, // Spinner
    /* 11 */ {0x05, 0x02}, // Cutter
    /* 12 */ {0x05, 0x03}, // Sawcer
    /* 13 */ {0x05, 0x04}, // Diska
    /* 14 */ {0x04, 0x00}, // Partisan
    /* 15 */ {0x04, 0x01}, // Halbert
    /* 16 */ {0x04, 0x02}, // Glaive
    /* 17 */ {0x04, 0x03}, // Berdys
    /* 18 */ {0x04, 0x04}, // Gungnir
    /* 19 */ {0x06, 0x00}, // Handgun
    /* 1A */ {0x06, 0x01}, // Autogun
    /* 1B */ {0x06, 0x02}, // Lockgun
    /* 1C */ {0x06, 0x03}, // Railgun
    /* 1D */ {0x06, 0x04}, // Raygun
    /* 1E */ {0x07, 0x00}, // Rifle
    /* 1F */ {0x07, 0x01}, // Sniper
    /* 20 */ {0x07, 0x02}, // Blaster
    /* 21 */ {0x07, 0x03}, // Beam
    /* 22 */ {0x07, 0x04}, // Laser
    /* 23 */ {0x08, 0x00}, // Mechgun
    /* 24 */ {0x08, 0x01}, // Assault
    /* 25 */ {0x08, 0x02}, // Repeater
    /* 26 */ {0x08, 0x03}, // Gatling
    /* 27 */ {0x08, 0x04}, // Vulcan
    /* 28 */ {0x09, 0x00}, // Shot
    /* 29 */ {0x09, 0x01}, // Spread
    /* 2A */ {0x09, 0x02}, // Cannon
    /* 2B */ {0x09, 0x03}, // Launcher
    /* 2C */ {0x09, 0x04}, // Arms
    /* 2D */ {0x0A, 0x00}, // Cane
    /* 2E */ {0x0A, 0x01}, // Stick
    /* 2F */ {0x0A, 0x02}, // Mace
    /* 30 */ {0x0A, 0x03}, // Club
    /* 31 */ {0x0B, 0x00}, // Rod
    /* 32 */ {0x0B, 0x01}, // Pole
    /* 33 */ {0x0B, 0x02}, // Pillar
    /* 34 */ {0x0B, 0x03}, // Striker
    /* 35 */ {0x0C, 0x00}, // Wand
    /* 36 */ {0x0C, 0x01}, // Staff
    /* 37 */ {0x0C, 0x02}, // Baton
    /* 38 */ {0x0C, 0x03}, // Scepter
    /* 39 */ {0xFF, 0xFF}, // Special-cased in type_defs_39 (depends on section ID)
    /* 3A */ {0xFF, 0xFF}, // Special-cased in type_defs_3A (depends on section ID)
    /* 3B */ {0x01, 0x05}, // DB'S SABER
    /* 3C */ {0x02, 0x05}, // FLOWEN'S SWORD
    /* 3D */ {0x06, 0x05}, // VARISTA
    /* 3E */ {0x08, 0x05}, // M&A60 VISE
    /* 3F */ {0x0A, 0x04}, // CLUB OF LACONIUM
    /* 40 */ {0x0C, 0x04}, // FIRE SCEPTER:AGNI
    /* 41 */ {0x0B, 0x04}, // BATTLE VERGE
    /* 42 */ {0x01, 0x06}, // KALADBOLG
    /* 43 */ {0x03, 0x05}, // BLADE DANCE
    /* 44 */ {0x07, 0x05}, // VISK-235W
    /* 45 */ {0x0A, 0x05}, // MACE OF ADAMAN
    /* 46 */ {0x0C, 0x05}, // ICE STAFF:DAGON
    /* 47 */ {0x0B, 0x05}, // BRAVE HAMMER
}};

const std::array<std::pair<uint8_t, uint8_t>, 10> WeaponShopRandomSet::type_defs_39{{
    // Indexed by section_id
    {0x28, 0x00}, // HARISEN BATTLE FAN
    {0x2A, 0x00}, // AKIKO'S WOK
    {0x2B, 0x00}, // TOY HAMMER
    {0x35, 0x00}, // CRAZY TUNE
    {0x52, 0x00}, // FLOWER CANE
    {0x48, 0x00}, // SAMBA MARACAS
    {0x64, 0x00}, // CHAMELEON SCYTHE
    {0x59, 0x00}, // BROOM
    {0x8A, 0x00}, // SANGE
    {0x99, 0x00}, // ANGEL HARP
}};

const std::array<std::pair<uint8_t, uint8_t>, 10> WeaponShopRandomSet::type_defs_3A{{
    // Indexed by section_id
    {0x99, 0x00}, // ANGEL HARP
    {0x64, 0x00}, // CHAMELEON SCYTHE
    {0x8A, 0x00}, // SANGE
    {0x28, 0x00}, // HARISEN BATTLE FAN
    {0x59, 0x00}, // BROOM
    {0x2B, 0x00}, // TOY HAMMER
    {0x52, 0x00}, // FLOWER CANE
    {0x2A, 0x00}, // AKIKO'S WOK
    {0x48, 0x00}, // SAMBA MARACAS
    {0x35, 0x00}, // CRAZY TUNE
}};

const std::array<int8_t, 20> WeaponShopRandomSet::bonus_values{
    -50, -45, -40, -35, -30, -25, -20, -15, -10, -5, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};

WeaponShopRandomSet::WeaponShopRandomSet(const void* data, size_t size, bool big_endian) {
  if (big_endian) {
    this->parse_t<true>(data, size);
  } else {
    this->parse_t<false>(data, size);
  }
}

WeaponShopRandomSet::WeaponShopRandomSet(const std::string& data, bool big_endian)
    : WeaponShopRandomSet(data.data(), data.size(), big_endian) {}

WeaponShopRandomSet::WeaponShopRandomSet(const phosg::JSON& json) {
  for (const auto& it : json.get_list("WeaponTypeWeightTables")) {
    this->weapon_type_weight_tables.emplace_back(table_for_json_t<IntPairT<uint8_t>>(*it));
  }
  this->bonus_type_table1 = fixed_table_for_json_t<IntPairT<uint32_t>, 9, 6>(json.at("BonusTypeTable1"));
  this->bonus_type_table2 = fixed_table_for_json_t<IntPairT<uint32_t>, 9, 6>(json.at("BonusTypeTable2"));
  this->bonus_range_table1 = fixed_table_for_json_t<IntPairT<uint32_t>, 9>(json.at("BonusRangeTable1"));
  this->bonus_range_table2 = fixed_table_for_json_t<IntPairT<uint32_t>, 9>(json.at("BonusRangeTable2"));
  this->special_mode_table = fixed_table_for_json_t<IntPairT<uint32_t>, 8, 3>(json.at("SpecialModeTable"));
  this->default_grind_range_table = fixed_table_for_json_t<IntPairT<uint32_t>, 6>(json.at("DefaultGrindRangeTable"));
  this->favored_grind_range_table = fixed_table_for_json_t<IntPairT<uint32_t>, 6>(json.at("FavoredGrindRangeTable"));
}

template <bool BE>
void WeaponShopRandomSet::parse_t(const void* data, size_t size) {
  std::set<uint32_t> start_offsets;

  phosg::StringReader r(data, size);
  uint32_t root_offset = r.pget<U32T<BE>>(size - 0x10);
  start_offsets.emplace(root_offset);

  const auto& root = r.pget<WeaponRootT<BE>>(root_offset);
  start_offsets.emplace(root.weapon_type_table);
  start_offsets.emplace(root.bonus_type_table1);
  start_offsets.emplace(root.bonus_type_table2);
  start_offsets.emplace(root.bonus_range_table1);
  start_offsets.emplace(root.bonus_range_table2);
  start_offsets.emplace(root.special_mode_table);
  start_offsets.emplace(root.default_grind_range_table);
  start_offsets.emplace(root.favored_grind_range_table);

  // Count the weapon types tables
  if (start_offsets.upper_bound(root.weapon_type_table) == start_offsets.end()) {
    throw std::runtime_error("Weapon type table is out of range");
  }
  for (uint32_t offset = root.weapon_type_table;
      offset < *start_offsets.upper_bound(root.weapon_type_table);
      offset += sizeof(TableSpecT<BE>)) {
    start_offsets.emplace(r.pget<TableSpecT<BE>>(offset).offset);
  }
  size_t num_weapon_types_tables =
      (*start_offsets.upper_bound(root.weapon_type_table) - root.weapon_type_table) / sizeof(TableSpecT<BE>);

  while (this->weapon_type_weight_tables.size() < num_weapon_types_tables) {
    const auto& spec = r.pget<TableSpecT<BE>>(
        root.weapon_type_table + this->weapon_type_weight_tables.size() * sizeof(TableSpecT<BE>));
    this->weapon_type_weight_tables.emplace_back(parse_table_t<IntPairT<uint8_t>>(
        r, spec.offset, spec.row_size, start_offsets));
  }

  auto parse_fixed_table_into_1d = [&]<size_t Count>(std::array<IntPairT<uint32_t>, Count>& ret, uint32_t offset) -> void {
    auto sub_r = r.sub(offset, sizeof(IntPairT<uint32_t>) * Count);
    for (size_t z = 0; z < Count; z++) {
      ret[z] = sub_r.get<IntPairT<U32T<BE>>>();
    }
  };
  auto parse_fixed_table_into_2d = [&]<size_t RowSize, size_t RowCount>(std::array<std::array<IntPairT<uint32_t>, RowSize>, RowCount>& ret, uint32_t offset) -> void {
    auto sub_r = r.sub(offset, sizeof(IntPairT<uint32_t>) * RowSize * RowCount);
    for (size_t y = 0; y < RowCount; y++) {
      for (size_t x = 0; x < RowSize; x++) {
        ret[y][x] = sub_r.get<IntPairT<U32T<BE>>>();
      }
    }
  };

  parse_fixed_table_into_2d(this->bonus_type_table1, root.bonus_type_table1);
  parse_fixed_table_into_2d(this->bonus_type_table2, root.bonus_type_table2);
  parse_fixed_table_into_1d(this->bonus_range_table1, root.bonus_range_table1);
  parse_fixed_table_into_1d(this->bonus_range_table2, root.bonus_range_table2);
  parse_fixed_table_into_2d(this->special_mode_table, root.special_mode_table);
  parse_fixed_table_into_1d(this->default_grind_range_table, root.default_grind_range_table);
  parse_fixed_table_into_1d(this->favored_grind_range_table, root.favored_grind_range_table);
}

template <bool BE>
std::string WeaponShopRandomSet::serialize_binary_t() const {
  RELFileWriter<BE> rel;

  WeaponRootT<BE> root;
  std::vector<TableSpecT<BE>> weapon_type_weight_table_specs;
  weapon_type_weight_table_specs.reserve(this->weapon_type_weight_tables.size());
  for (const auto& table : this->weapon_type_weight_tables) {
    weapon_type_weight_table_specs.emplace_back(serialize_table_t<IntPairT<uint8_t>>(rel, table));
  }

  root.weapon_type_table = rel.w.size();
  for (const auto& spec : weapon_type_weight_table_specs) {
    rel.put(spec);
    rel.relocations.emplace(rel.w.size() - 8);
  }

  auto serialize_fixed_table_1d = [&]<size_t Count>(const std::array<IntPairT<uint32_t>, Count>& table) -> uint32_t {
    uint32_t ret = rel.w.size();
    for (size_t z = 0; z < Count; z++) {
      rel.template put<IntPairT<U32T<BE>>>(table[z]);
    }
    return ret;
  };
  auto serialize_fixed_table_2d = [&]<size_t RowSize, size_t RowCount>(const std::array<std::array<IntPairT<uint32_t>, RowSize>, RowCount>& table) -> uint32_t {
    uint32_t ret = rel.w.size();
    for (size_t y = 0; y < RowCount; y++) {
      for (size_t x = 0; x < RowSize; x++) {
        rel.template put<IntPairT<U32T<BE>>>(table[y][x]);
      }
    }
    return ret;
  };

  rel.align(4);
  root.bonus_type_table1 = serialize_fixed_table_2d(this->bonus_type_table1);
  root.bonus_type_table2 = serialize_fixed_table_2d(this->bonus_type_table2);
  root.special_mode_table = serialize_fixed_table_2d(this->special_mode_table);
  root.bonus_range_table1 = serialize_fixed_table_1d(this->bonus_range_table1);
  root.bonus_range_table2 = serialize_fixed_table_1d(this->bonus_range_table2);
  root.default_grind_range_table = serialize_fixed_table_1d(this->default_grind_range_table);
  root.favored_grind_range_table = serialize_fixed_table_1d(this->favored_grind_range_table);

  rel.align(4);
  uint32_t root_offset = rel.put(root);
  for (size_t z = 1; z <= 8; z++) {
    rel.relocations.emplace(rel.w.size() - (z * 4));
  }

  return rel.finalize(root_offset);
}

std::string WeaponShopRandomSet::serialize_binary(bool big_endian) const {
  return big_endian ? this->serialize_binary_t<true>() : this->serialize_binary_t<false>();
}

phosg::JSON WeaponShopRandomSet::json() const {
  auto weapon_type_weight_tables_json = phosg::JSON::list();
  for (const auto& table : this->weapon_type_weight_tables) {
    weapon_type_weight_tables_json.emplace_back(json_for_table_t(table));
  }

  return phosg::JSON::dict({
      {"WeaponTypeWeightTables", std::move(weapon_type_weight_tables_json)},
      {"BonusTypeTable1", json_for_fixed_table_t(this->bonus_type_table1)},
      {"BonusTypeTable2", json_for_fixed_table_t(this->bonus_type_table2)},
      {"BonusRangeTable1", json_for_fixed_table_t(this->bonus_range_table1)},
      {"BonusRangeTable2", json_for_fixed_table_t(this->bonus_range_table2)},
      {"SpecialModeTable", json_for_fixed_table_t(this->special_mode_table)},
      {"DefaultGrindRangeTable", json_for_fixed_table_t(this->default_grind_range_table)},
      {"FavoredGrindRangeTable", json_for_fixed_table_t(this->favored_grind_range_table)},
  });
}

void WeaponShopRandomSet::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "WeaponShopRandomSet\n");
  for (size_t table_index = 0; table_index < this->weapon_type_weight_tables.size(); table_index++) {
    phosg::fwrite_fmt(stream, "  Weapon type weight table {}\n", table_index);
    for (size_t z = 0; z < this->weapon_type_weight_tables[table_index].size(); z++) {
      phosg::fwrite_fmt(stream, "    {:02X} ({:<10}):", z, name_for_section_id(z));
      for (const auto& cell : this->weapon_type_weight_tables[table_index][z]) {
        if (cell.value == 0x39) {
          phosg::fwrite_fmt(stream, "   {:02X} (SECID1) @ {:03}", cell.value, cell.weight);
        } else if (cell.value == 0x3A) {
          phosg::fwrite_fmt(stream, "   {:02X} (SECID2) @ {:03}", cell.value, cell.weight);
        } else {
          const auto& def = this->type_defs.at(cell.value);
          phosg::fwrite_fmt(stream, "   {:02X} (00{:02X}{:02X}) @ {:03}", cell.value, def.first, def.second, cell.weight);
        }
      }
      phosg::fwrite_fmt(stream, "\n");
    }
  }

  auto print_fixed_table_1d = [&]<size_t Count>(const std::array<IntPairT<uint32_t>, Count>& table) -> void {
    for (size_t z = 0; z < Count; z++) {
      const auto& cell = table[z];
      phosg::fwrite_fmt(stream, "    {:02}:   {:03}-{:03}\n", z, cell.first, cell.second);
    }
  };
  auto print_fixed_table_2d = [&]<size_t RowCount, size_t RowSize>(const std::array<std::array<IntPairT<uint32_t>, RowSize>, RowCount>& table) -> void {
    for (size_t y = 0; y < RowCount; y++) {
      phosg::fwrite_fmt(stream, "    {:02X}:", y);
      for (size_t x = 0; x < RowSize; x++) {
        const auto& cell = table[y][x];
        phosg::fwrite_fmt(stream, "   {:02} @ {:03}", cell.first, cell.second);
      }
      phosg::fwrite_fmt(stream, "\n");
    }
  };

  phosg::fwrite_fmt(stream, "  Bonus type table 1\n");
  print_fixed_table_2d(this->bonus_type_table1);
  phosg::fwrite_fmt(stream, "  Bonus type table 2\n");
  print_fixed_table_2d(this->bonus_type_table2);
  phosg::fwrite_fmt(stream, "  Bonus range table 1\n");
  print_fixed_table_1d(this->bonus_range_table1);
  phosg::fwrite_fmt(stream, "  Bonus range table 2\n");
  print_fixed_table_1d(this->bonus_range_table2);
  phosg::fwrite_fmt(stream, "  Special mode table\n");
  print_fixed_table_2d(this->special_mode_table);
  phosg::fwrite_fmt(stream, "  Default grind range table\n");
  print_fixed_table_1d(this->default_grind_range_table);
  phosg::fwrite_fmt(stream, "  Favored grind range table\n");
  print_fixed_table_1d(this->favored_grind_range_table);
}
