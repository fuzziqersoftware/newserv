#include "TekkerAdjustmentSet.hh"

#include <algorithm>

#include "CommonFileFormats.hh"
#include "StaticGameData.hh"
#include "Types.hh"

static const std::array<int8_t, 11> delta_table = {-10, -5, -3, -2, -1, 0, 1, 2, 3, 5, 10};
static const std::unordered_map<int8_t, size_t> reverse_delta_table = {
    {-10, 0}, {-5, 1}, {-3, 2}, {-2, 3}, {-1, 4}, {0, 5}, {1, 6}, {2, 7}, {3, 8}, {5, 9}, {10, 10}};

struct DeltaProbabilityEntry {
  uint8_t delta_index;
  uint8_t count_default;
  uint8_t count_favored;
} __packed_ws__(DeltaProbabilityEntry, 3);

struct LuckTableEntry {
  uint8_t delta_index;
  int8_t luck;
} __packed_ws__(LuckTableEntry, 2);

template <bool BE>
struct ProbTableRefT {
  U32T<BE> offset;
  U32T<BE> count;
} __packed_ws_be__(ProbTableRefT, 8);

template <bool BE>
struct RootT {
  // Each section ID's favored weapon class has different probabilities than those used for all other weapons. The
  // tables are labeled with (D) for the default values and (F) for the favored-class values.

  // Note that the favored bonuses for Redria are all zero; these values are unused because Redria does not have a
  // favored weapon type. Curiously, Yellowboze also does not have a favored weapon type, but the values for Yellowboze
  // are not all zero.

  // This table specifies how likely a special is to be upgraded or downgraded by one level.
  // In PSO V3, the special upgrade table is:
  //   Viridia    => (D) +1=10%, 0=60%, -1=30%
  //   Viridia    => (F) +1=25%, 0=50%, -1=25%
  //   Greennill  => (D) +1=25%, 0=65%, -1=10%
  //   Greennill  => (F) +1=40%, 0=55%, -1=5%
  //   Skyly      => (D) +1=15%, 0=70%, -1=15%
  //   Skyly      => (F) +1=30%, 0=60%, -1=10%
  //   Bluefull   => (D) +1=10%, 0=60%, -1=30%
  //   Bluefull   => (F) +1=25%, 0=50%, -1=25%
  //   Purplenum  => (D) +1=25%, 0=65%, -1=10%
  //   Purplenum  => (F) +1=40%, 0=55%, -1=5%
  //   Pinkal     => (D) +1=15%, 0=70%, -1=15%
  //   Pinkal     => (F) +1=30%, 0=60%, -1=10%
  //   Redria     => (D) +1=20%, 0=60%, -1=20%
  //   Redria     => (F) +1=0%,  0=0%,  -1=0%
  //   Oran       => (D) +1=15%, 0=70%, -1=15%
  //   Oran       => (F) +1=30%, 0=60%, -1=10%
  //   Yellowboze => (D) +1=25%, 0=65%, -1=10%
  //   Yellowboze => (F) +1=40%, 0=55%, -1=5%
  //   Whitill    => (D) +1=10%, 0=60%, -1=30%
  //   Whitill    => (F) +1=25%, 0=50%, -1=25%
  U32T<BE> special_delta_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

  // This table specifies how likely a weapon's grind is to be upgraded or downgraded, and by how much. The final grind
  // value is clamped to the range between 0 and the weapon's maximum grind from ItemPMT, inclusive.
  // In PSO V3, the grind delta table is:
  //   Viridia    => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
  //   Viridia    => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
  //   Greennill  => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
  //   Greennill  => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
  //   Skyly      => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
  //   Skyly      => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
  //   Bluefull   => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
  //   Bluefull   => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
  //   Purplenum  => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
  //   Purplenum  => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
  //   Pinkal     => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
  //   Pinkal     => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
  //   Redria     => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
  //   Redria     => (F) +3=0%,  +2=0%,  +1=0%,  0=0%,  -1=0%,  -2=0%,  -3=0%
  //   Oran       => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
  //   Oran       => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
  //   Yellowboze => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
  //   Yellowboze => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
  //   Whitill    => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
  //   Whitill    => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
  U32T<BE> grind_delta_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

  // This table specifies how likely a weapon's bonuses are to be upgraded or downgraded, and by how much. The final
  // bonuses are capped above at 100, but there is no lower limit (so negative results are possible).
  // In PSO V3, the bonus delta table is:
  //   Viridia    => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
  //   Viridia    => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
  //   Greennill  => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
  //   Greennill  => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
  //   Skyly      => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
  //   Skyly      => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
  //   Bluefull   => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
  //   Bluefull   => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
  //   Purplenum  => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
  //   Purplenum  => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
  //   Pinkal     => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
  //   Pinkal     => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
  //   Redria     => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
  //   Redria     => (F) +10=0%,  +5=0%,  0=0%,  -5=0%,  -10=0%
  //   Oran       => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
  //   Oran       => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
  //   Yellowboze => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
  //   Yellowboze => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
  //   Whitill    => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
  //   Whitill    => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
  U32T<BE> bonus_delta_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

  // There is a secondary computation done during weapon adjustment that appears to determine how "good" the resulting
  // weapon is compared to its original state. If the result of this computation is positive, the game plays a jingle
  // when the tekker result is accepted. These tables describe how much each delta affects this value, which we call
  // luck.

  // In PSO V3, the special upgrade luck table is:
  //   +1 => +20, 0 => 0, -1 => -20
  U32T<BE> special_luck_table_offset; // LuckTableEntry[...]; ending with FF FF

  // In PSO V3, the grind delta luck table is:
  //   +3 => +10, +2 => +5, +1 => +3, 0 => 0, -1 => -3, -2 => -5, -3 => -10
  U32T<BE> grind_luck_table_offset; // LuckTableEntry[...]; ending with FF FF

  // In PSO V3, the bonus delta luck table is:
  //   +10 => +15, +5 => +8, 0 => 0, -5 => -8, -10 => -15
  U32T<BE> bonus_luck_table_offset; // LuckTableEntry[...]; ending with FF FF
} __packed_ws_be__(RootT, 0x18);

uint8_t TekkerAdjustmentSet::favored_weapon_type_for_section_id(uint8_t section_id) {
  // The favored weapon type table is hardcoded in the game client. The table is:
  //   Viridia     shots
  //   Greennill   rifles
  //   Skyly       swords
  //   Bluefull    partisans
  //   Purplenum   mechguns
  //   Pinkal      canes
  //   Redria      (none)
  //   Oran        daggers
  //   Yellowboze  (none)
  //   Whitill     slicers
  static const std::array<uint8_t, 10> data{0x09, 0x07, 0x02, 0x04, 0x08, 0x0A, 0xFF, 0x03, 0xFF, 0x05};
  return data.at(section_id);
}

TekkerAdjustmentSet::TekkerAdjustmentSet(const void* data, size_t size, bool big_endian) {
  if (big_endian) {
    this->parse_t<true>(data, size);
  } else {
    this->parse_t<false>(data, size);
  }
}

TekkerAdjustmentSet::TekkerAdjustmentSet(const std::string& data, bool big_endian)
    : TekkerAdjustmentSet(data.data(), data.size(), big_endian) {}

TekkerAdjustmentSet::TekkerAdjustmentSet(const phosg::JSON& json) {
  auto parse_delta_table = [](const phosg::JSON& json) -> std::array<Table, 10> {
    if (!json.is_dict() || json.size() != 10) {
      throw std::runtime_error("Invalid structure for TekkerAdjustmentSet JSON delta table");
    }
    std::array<Table, 10> ret;
    for (size_t section_id = 0; section_id < 10; section_id++) {
      auto& table = ret[section_id];
      for (const auto& [k, v] : json.at(name_for_section_id(section_id)).as_dict()) {
        auto prob = v->as_int();
        table.probs.emplace(stoll(k), prob);
        table.total += prob;
      }
    }
    return ret;
  };

  this->favored_special_delta_table = parse_delta_table(json.at("FavoredSpecialDeltaTable"));
  this->default_special_delta_table = parse_delta_table(json.at("DefaultSpecialDeltaTable"));
  this->favored_grind_delta_table = parse_delta_table(json.at("FavoredGrindDeltaTable"));
  this->default_grind_delta_table = parse_delta_table(json.at("DefaultGrindDeltaTable"));
  this->favored_bonus_delta_table = parse_delta_table(json.at("FavoredBonusDeltaTable"));
  this->default_bonus_delta_table = parse_delta_table(json.at("DefaultBonusDeltaTable"));

  auto parse_luck_table = [](const phosg::JSON& json) -> std::unordered_map<int8_t, int8_t> {
    std::unordered_map<int8_t, int8_t> ret;
    for (const auto& [k, v] : json.as_dict()) {
      ret.emplace(stoll(k), v->as_int());
    }
    return ret;
  };

  this->special_luck_table = parse_luck_table(json.at("SpecialLuckTable"));
  this->grind_luck_table = parse_luck_table(json.at("GrindLuckTable"));
  this->bonus_luck_table = parse_luck_table(json.at("BonusLuckTable"));
}

template <bool BE>
void TekkerAdjustmentSet::parse_t(const void* data, size_t size) {
  phosg::StringReader r(data, size);
  const auto& root = r.pget<RootT<BE>>(r.pget<U32T<BE>>(size - 0x10));

  auto parse_delta_table = [&r](std::array<Table, 10>& favored_tables, std::array<Table, 10>& default_tables, uint32_t ref_offset) -> void {
    const auto& ref = r.pget<ProbTableRefT<BE>>(ref_offset);
    auto* entries = &r.pget<DeltaProbabilityEntry>(ref.offset, sizeof(DeltaProbabilityEntry) * ref.count * 10);
    for (size_t section_id = 0; section_id < 10; section_id++) {
      auto& favored_table = favored_tables[section_id];
      auto& default_table = default_tables[section_id];
      for (size_t z = 0; z < ref.count; z++) {
        const auto& entry = entries[section_id * ref.count + z];
        int8_t delta = delta_table.at(entry.delta_index);
        favored_table.probs.emplace(delta, entry.count_favored);
        favored_table.total += entry.count_favored;
        default_table.probs.emplace(delta, entry.count_default);
        default_table.total += entry.count_default;
      }
    }
  };

  parse_delta_table(this->favored_special_delta_table, this->default_special_delta_table, root.special_delta_table_offset);
  parse_delta_table(this->favored_grind_delta_table, this->default_grind_delta_table, root.grind_delta_table_offset);
  parse_delta_table(this->favored_bonus_delta_table, this->default_bonus_delta_table, root.bonus_delta_table_offset);

  auto parse_luck_table = [&r](uint32_t offset) -> std::unordered_map<int8_t, int8_t> {
    auto sub_r = r.sub(offset);
    std::unordered_map<int8_t, int8_t> ret;
    for (;;) {
      const auto& entry = sub_r.get<LuckTableEntry>();
      if (entry.delta_index == 0xFF) {
        break;
      }
      ret.emplace(delta_table.at(entry.delta_index), entry.luck);
    }
    return ret;
  };

  this->special_luck_table = parse_luck_table(root.special_luck_table_offset);
  this->grind_luck_table = parse_luck_table(root.grind_luck_table_offset);
  this->bonus_luck_table = parse_luck_table(root.bonus_luck_table_offset);
}

template <bool BE>
std::string TekkerAdjustmentSet::serialize_binary_t() const {
  RELFileWriter<BE> rel;

  auto serialize_delta_tables = [&rel](const std::array<Table, 10>& favored_tables, const std::array<Table, 10>& default_tables) -> ProbTableRefT<BE> {
    std::set<int8_t> all_deltas;
    for (size_t section_id = 0; section_id < 10; section_id++) {
      for (const auto& [delta, _] : favored_tables[section_id].probs) {
        all_deltas.emplace(delta);
      }
      for (const auto& [delta, _] : default_tables[section_id].probs) {
        all_deltas.emplace(delta);
      }
    }

    ProbTableRefT<BE> ret{rel.w.size(), all_deltas.size()};

    for (size_t section_id = 0; section_id < 10; section_id++) {
      for (auto delta_it = all_deltas.rbegin(); delta_it != all_deltas.rend(); delta_it++) {
        DeltaProbabilityEntry entry;
        entry.delta_index = reverse_delta_table.at(*delta_it);
        try {
          entry.count_favored = favored_tables[section_id].probs.at(*delta_it);
        } catch (const std::out_of_range&) {
        }
        try {
          entry.count_default = default_tables[section_id].probs.at(*delta_it);
        } catch (const std::out_of_range&) {
        }
        rel.template put<DeltaProbabilityEntry>(entry);
      }
    }

    return ret;
  };

  auto special_delta_ref = serialize_delta_tables(this->favored_special_delta_table, this->default_special_delta_table);
  auto grind_delta_ref = serialize_delta_tables(this->favored_grind_delta_table, this->default_grind_delta_table);
  auto bonus_delta_ref = serialize_delta_tables(this->favored_bonus_delta_table, this->default_bonus_delta_table);

  auto serialize_luck_table = [&rel](const std::unordered_map<int8_t, int8_t>& table) -> uint32_t {
    uint32_t ret = rel.w.size();

    std::vector<std::pair<uint8_t, int8_t>> entries;
    for (const auto& [delta, luck] : table) {
      entries.emplace_back(std::make_pair(reverse_delta_table.at(delta), luck));
    }
    std::sort(entries.begin(), entries.end(), std::greater<std::pair<uint8_t, int8_t>>());

    for (const auto& [delta_index, luck] : entries) {
      rel.w.put_u8(delta_index);
      rel.w.put_s8(luck);
    }
    rel.w.put_u16(0xFFFF);

    return ret;
  };

  RootT<BE> root;
  root.special_luck_table_offset = serialize_luck_table(this->special_luck_table);
  root.grind_luck_table_offset = serialize_luck_table(this->grind_luck_table);
  root.bonus_luck_table_offset = serialize_luck_table(this->bonus_luck_table);

  rel.align(4);
  rel.relocations.emplace(rel.w.size());
  root.special_delta_table_offset = rel.template put<ProbTableRefT<BE>>(special_delta_ref);
  rel.relocations.emplace(rel.w.size());
  root.grind_delta_table_offset = rel.template put<ProbTableRefT<BE>>(grind_delta_ref);
  rel.relocations.emplace(rel.w.size());
  root.bonus_delta_table_offset = rel.template put<ProbTableRefT<BE>>(bonus_delta_ref);

  uint32_t root_offset = rel.template put<RootT<BE>>(root);
  for (size_t z = 1; z <= sizeof(RootT<BE>) / 4; z++) {
    rel.relocations.emplace(rel.w.size() - (z * 4));
  }

  return rel.finalize(root_offset);
}

std::string TekkerAdjustmentSet::serialize_binary(bool big_endian) const {
  return big_endian ? this->serialize_binary_t<true>() : this->serialize_binary_t<false>();
}

phosg::JSON TekkerAdjustmentSet::json() const {
  auto ret = phosg::JSON::dict();

  auto serialize_delta_table = [](const std::array<Table, 10>& table) -> phosg::JSON {
    auto ret = phosg::JSON::dict();
    for (size_t section_id = 0; section_id < 10; section_id++) {
      auto secid_ret = phosg::JSON::dict();
      for (const auto& [k, v] : table[section_id].probs) {
        secid_ret.emplace(std::format("{}", k), v);
      }
      ret.emplace(name_for_section_id(section_id), std::move(secid_ret));
    }
    return ret;
  };

  ret.emplace("FavoredSpecialDeltaTable", serialize_delta_table(this->favored_special_delta_table));
  ret.emplace("DefaultSpecialDeltaTable", serialize_delta_table(this->default_special_delta_table));
  ret.emplace("FavoredGrindDeltaTable", serialize_delta_table(this->favored_grind_delta_table));
  ret.emplace("DefaultGrindDeltaTable", serialize_delta_table(this->default_grind_delta_table));
  ret.emplace("FavoredBonusDeltaTable", serialize_delta_table(this->favored_bonus_delta_table));
  ret.emplace("DefaultBonusDeltaTable", serialize_delta_table(this->default_bonus_delta_table));

  auto serialize_luck_table = [](const std::unordered_map<int8_t, int8_t>& table) -> phosg::JSON {
    auto ret = phosg::JSON::dict();
    for (const auto& [k, v] : table) {
      ret.emplace(std::format("{}", k), v);
    }
    return ret;
  };

  ret.emplace("SpecialLuckTable", serialize_luck_table(this->special_luck_table));
  ret.emplace("GrindLuckTable", serialize_luck_table(this->grind_luck_table));
  ret.emplace("BonusLuckTable", serialize_luck_table(this->bonus_luck_table));

  return ret;
}

void TekkerAdjustmentSet::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "TekkerAdjustmentSet\n");

  auto print_table = [stream](const std::array<Table, 10>& table, const std::unordered_map<int8_t, int8_t>& luck_table) -> void {
    for (size_t section_id = 0; section_id < 10; section_id++) {
      phosg::fwrite_fmt(stream, "    {:<10}:", name_for_section_id(section_id));
      std::vector<std::pair<int8_t, size_t>> sorted_probs;
      for (const auto& [delta, prob] : table[section_id].probs) {
        sorted_probs.emplace_back(delta, prob);
      }
      std::sort(sorted_probs.begin(), sorted_probs.end());
      for (const auto& [delta, prob] : sorted_probs) {
        int8_t luck = luck_table.at(delta);
        phosg::fwrite_fmt(stream, "  {:>2} @ {:>2} ({:>2})", delta, prob, luck);
      }
      phosg::fwrite_fmt(stream, "\n");
    }
  };

  phosg::fwrite_fmt(stream, "  Favored special deltas:\n");
  print_table(this->favored_special_delta_table, this->special_luck_table);
  phosg::fwrite_fmt(stream, "  Default special deltas:\n");
  print_table(this->default_special_delta_table, this->special_luck_table);
  phosg::fwrite_fmt(stream, "  Favored grind deltas:\n");
  print_table(this->favored_grind_delta_table, this->grind_luck_table);
  phosg::fwrite_fmt(stream, "  Default grind deltas:\n");
  print_table(this->default_grind_delta_table, this->grind_luck_table);
  phosg::fwrite_fmt(stream, "  Favored bonus deltas:\n");
  print_table(this->favored_bonus_delta_table, this->bonus_luck_table);
  phosg::fwrite_fmt(stream, "  Default bonus deltas:\n");
  print_table(this->default_bonus_delta_table, this->bonus_luck_table);
}
