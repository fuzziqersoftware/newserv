#include "MagEvolutionTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

template <bool BE>
struct MotionReferenceTables {
  // It seems that there are two definition tables, but only the first is used on any version of PSO. On v3 and later,
  // the two offsets point to the same table, but on v2 they don't and the second table contains different data.
  // TODO: Figure out what the deal is with the different v2 tables.
  U32T<BE> ref_table;
  U32T<BE> unused_ref_table;
} __packed_ws_be__(MotionReferenceTables, 0x08);

template <bool BE>
struct ColorEntry {
  // Colors are specified as 4 floats, each in the range [0, 1], for each color channel. The default colors are:
  //          alpha   red     green   blue    color (see StaticGameData.cc)
  //   00 =>  1.0     1.0     0.2     0.1     red
  //   01 =>  1.0     0.2     0.2     1.0     blue
  //   02 =>  1.0     1.0     0.9     0.1     yellow
  //   03 =>  1.0     0.1     1.0     0.1     green
  //   04 =>  1.0     0.8     0.1     1.0     purple
  //   05 =>  1.0     0.1     0.1     0.2     black
  //   06 =>  1.0     0.9     1.0     1.0     white
  //   07 =>  1.0     0.1     0.9     1.0     cyan
  //   08 =>  1.0     0.5     0.3     0.2     brown
  //   09 =>  1.0     1.0     0.4     0.0     orange (v3+)
  //   0A =>  1.0     0.502   0.545   0.977   light-blue (v3+)
  //   0B =>  1.0     0.502   0.502   0.0     olive (v3+)
  //   0C =>  1.0     0.0     0.941   0.714   turquoise (v3+)
  //   0D =>  1.0     0.8     0.098   0.392   fuchsia (v3+)
  //   0E =>  1.0     0.498   0.498   0.498   grey (v3+)
  //   0F =>  1.0     0.996   0.996   0.832   cream (v3+)
  //   10 =>  1.0     0.996   0.498   0.784   pink (v3+)
  //   11 =>  1.0     0.0     0.498   0.322   dark-green (v3+)
  // If a mag's color index is invalid (>= 0x12), it is reassigned at equip time using the following logic:
  //   - Set base_index to player->visual.skin if player is an android, or player->visual.costume otherwise
  //   - If (base_index % 9) < 7 (that is, if their costume or body color is one of the colored slots on the character
  //     creation screen), then set the mag color to either (base_index % 9) or (base_index % 9) + 9, with equal
  //     probability.
  //   - If (base_index % 9) >= 7 (that is, if their costume or body color is one of the last two blank-colored slots
  //     on the character creation screen), then set the mag color to any of the available colors, chosen at random.
  F32T<BE> alpha;
  F32T<BE> red;
  F32T<BE> green;
  F32T<BE> blue;
  ColorEntry(const VectorXYZTF& c) : alpha(c.t), red(c.x), green(c.y), blue(c.z) {}
  operator VectorXYZTF() const {
    return VectorXYZTF{this->red.load(), this->green.load(), this->blue.load(), this->alpha.load()};
  }
} __packed_ws_be__(ColorEntry, 0x10);

template <bool BE>
struct UnknownA3EntryT {
  uint8_t flags;
  uint8_t unknown_a2;
  U16T<BE> unknown_a3;
  U16T<BE> unknown_a4;
  U16T<BE> unknown_a5;
  UnknownA3EntryT(const MagEvolutionTable::UnknownA3Entry& e)
      : flags(e.flags),
        unknown_a2(e.unknown_a2),
        unknown_a3(e.unknown_a3),
        unknown_a4(e.unknown_a4),
        unknown_a5(e.unknown_a5) {}
  operator MagEvolutionTable::UnknownA3Entry() const {
    return MagEvolutionTable::UnknownA3Entry{
        this->flags, this->unknown_a2, this->unknown_a3, this->unknown_a4, this->unknown_a5};
  }
} __packed_ws_be__(UnknownA3EntryT, 0x08);

struct HeaderV1 {
  parray<uint8_t, 4> unknown_a1 = {0x0F, 0xF0, 0x00, 0x00};
  le_uint32_t unknown_a2 = 0x00000003;
  le_uint16_t unknown_a3 = 0x00C8;
  le_uint16_t unknown_a4 = 0x0078;
  // unknown_a5 added in V2
  le_float unknown_a6 = 0.25;
  le_float unknown_a7 = 0.1;
  le_uint32_t unknown_a8 = 0x00000C00;
} __packed_ws__(HeaderV1, 0x18);

template <bool BE>
struct HeaderV2V3V4 {
  parray<uint8_t, 4> unknown_a1 = {0x0F, 0xF0, 0x00, 0x00};
  U32T<BE> unknown_a2 = 0x00000003;
  U16T<BE> unknown_a3 = 0x00C8;
  U16T<BE> unknown_a4 = 0x0078;
  parray<uint8_t, 4> unknown_a5 = {0xC8, 0x00, 0x00, 0x00};
  F32T<BE> unknown_a6 = 0.25;
  F32T<BE> unknown_a7 = 0.1;
  U32T<BE> unknown_a8 = 0x00000C00;
} __packed_ws_be__(HeaderV2V3V4, 0x1C);

// Fields:
// 112K /   V1 /   V2 /   V3 /   BB R
// 0018 / 0018 / 001C / 001C / 001C   motion_tables.ref_table // -> MotionReference[NumMags]
// 0228 / 0228 / 02D4 / 001C / 001C   motion_tables.unused_ref_table // -> MotionReference[NumMags]
// 0438 / 0438 / 05BC / 0340 / 0400 * motion_tables; // -> MotionReferenceTables
// 0440 / 0440 / 0594 / 0348 / 0408 * unknown_a2; // -> (uint8_t[2])[NumMags] (references into unknown_a3)
// 0498 / 0498 / 0608 / 03CE / 04AE * unknown_a3; // -> UnknownA3Entry[max(unknown_a2) + 1]
// 0510 / 0520 / 06B0 / 0476 / 0556 * unknown_a4; // -> uint8_t[NumMags]
// 053C / 054C / 06EC / 04BC / 05AC * color_table; // -> ColorEntry[NumColors]
// ---- / ---- / 077C / 05DC / 06CC * evolution_number_table; // -> uint8_t[NumMags]

template <bool BE>
struct RootV1 {
  U32T<BE> motion_tables;
  U32T<BE> unknown_a2;
  U32T<BE> unknown_a3;
  U32T<BE> unknown_a4;
  U32T<BE> color_table;
} __packed_ws_be__(RootV1, 0x14);

template <bool BE>
struct RootV2V3V4 : RootV1<BE> {
  U32T<BE> evolution_number_table;
} __packed_ws_be__(RootV2V3V4, 0x18);

static uint8_t get_v1_mag_evolution_number(uint8_t data1_1) {
  static const std::array<uint8_t, 0x2C> v1_evolution_number_table{
      /* 00 */ 0, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 2, 2,
      /* 10 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 2, 2, 3, 4, 3, 3,
      /* 20 */ 3, 4, 4, 3, 3, 3, 3, 4, 4, 4, 4, 4};
  if (data1_1 >= v1_evolution_number_table.size()) {
    throw runtime_error("invalid mag number");
  }
  return v1_evolution_number_table[data1_1];
}

template <typename HeaderT, typename RootT, size_t NumMags, size_t NumColors, bool BE>
class BinaryMagEvolutionTableT : public MagEvolutionTable {
public:
  explicit BinaryMagEvolutionTableT(std::shared_ptr<const std::string> data)
      : data(data), r(*data), root(&r.pget<RootT>(this->r.pget_u32l(this->data->size() - 0x10))) {}
  virtual ~BinaryMagEvolutionTableT() = default;

  template <typename RawT, typename ParsedT>
  const ParsedT& add_to_vector_cache(std::vector<ParsedT>& cache, size_t base_offset, size_t index) const {
    while (cache.size() <= index) {
      cache.emplace_back(this->r.pget<RawT>(base_offset + sizeof(RawT) * cache.size()));
    }
    return cache[index];
  }

  virtual size_t num_mags() const {
    return NumMags;
  }

  virtual size_t num_motion_entries(bool use_second_table) const {
    const auto& tables = this->r.pget<MotionReferenceTables<BE>>(this->root->motion_tables);
    return get_rel_array_count<MotionReference>(
        this->all_start_offsets(), use_second_table ? tables.unused_ref_table : tables.ref_table);
  }

  virtual const MotionReference& get_motion_reference(bool use_second_table, size_t index) const {
    if (index >= this->num_motion_entries(use_second_table)) {
      throw std::logic_error("Invalid motion reference index");
    }
    const auto& tables = this->r.pget<MotionReferenceTables<BE>>(this->root->motion_tables);
    uint32_t array_offset = use_second_table ? tables.unused_ref_table : tables.ref_table;
    return this->r.pget<MotionReference>(array_offset + sizeof(MotionReference) * index);
  }

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t index) const {
    if (index >= this->num_mags()) {
      throw std::logic_error("Invalid unknown_a2 index");
    }
    uint32_t base_offset = this->root->unknown_a2 + (index * 2);
    return std::make_pair(this->r.pget_u8(base_offset), this->r.pget_u8(base_offset + 1));
  }

  virtual size_t num_unknown_a3_entries() const {
    return get_rel_array_count<UnknownA3EntryT<BE>>(this->all_start_offsets(), this->root->unknown_a3);
  }

  virtual const UnknownA3Entry& get_unknown_a3(size_t index) const {
    if (index >= this->num_unknown_a3_entries()) {
      throw std::logic_error("Invalid unknown_a2 index");
    }
    return this->add_to_vector_cache<UnknownA3EntryT<BE>>(this->unknown_a3_entries, this->root->unknown_a3, index);
  }

  virtual uint8_t get_unknown_a4(size_t index) const {
    if (index >= this->num_mags()) {
      throw std::logic_error("Invalid unknown_a4 index");
    }
    return this->r.pget_u8(this->root->unknown_a2 + index);
  }

  virtual size_t num_colors() const {
    return NumColors;
  }

  virtual const VectorXYZTF& get_color_rgba(size_t index) const {
    if (index >= NumColors) {
      throw runtime_error("invalid mag color index");
    }
    return this->add_to_vector_cache<ColorEntry<BE>>(this->colors, this->root->color_table, index);
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    if (data1_1 >= this->num_mags()) {
      throw std::logic_error("Invalid unknown_a4 index");
    }
    if constexpr (requires { this->root->evolution_number_table; }) {
      return this->r.pget_u8(this->root->evolution_number_table + data1_1);
    } else {
      return get_v1_mag_evolution_number(data1_1);
    }
  }

  const std::set<uint32_t>& all_start_offsets() const {
    if (this->start_offsets.empty()) {
      this->start_offsets = all_relocation_offsets_for_rel_file<BE>(r.pgetv(0, r.size()), r.size());
    }
    return this->start_offsets;
  }

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const RootT* root;
  mutable std::set<uint32_t> start_offsets;
  mutable std::vector<UnknownA3Entry> unknown_a3_entries;
  mutable std::vector<VectorXYZTF> colors;
};

class MagEvolutionTableDCNTE : public MagEvolutionTable {
public:
  MagEvolutionTableDCNTE() = default;
  virtual ~MagEvolutionTableDCNTE() = default;

  virtual size_t num_mags() const {
    return 0x2C;
  }

  virtual size_t num_motion_entries(bool) const {
    return 0;
  }
  virtual const MotionReference& get_motion_reference(bool, size_t) const {
    throw runtime_error("Mag tables not available on DC NTE");
  }

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t) const {
    throw runtime_error("Mag tables not available on DC NTE");
  }

  virtual size_t num_unknown_a3_entries() const {
    return 0;
  }
  virtual const UnknownA3Entry& get_unknown_a3(size_t) const {
    throw runtime_error("Mag tables not available on DC NTE");
  }

  virtual uint8_t get_unknown_a4(size_t) const {
    throw runtime_error("Mag tables not available on DC NTE");
  }

  virtual size_t num_colors() const {
    return 0;
  }
  virtual const VectorXYZTF& get_color_rgba(size_t) const {
    throw runtime_error("Mag tables not available on DC NTE");
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    return get_v1_mag_evolution_number(data1_1);
  }
};

using MagEvolutionTableDC112000 = BinaryMagEvolutionTableT<HeaderV1, RootV1<false>, 0x28, 0x09, false>;
using MagEvolutionTableV1 = BinaryMagEvolutionTableT<HeaderV1, RootV1<false>, 0x28, 0x09, false>;
using MagEvolutionTableV2 = BinaryMagEvolutionTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x3A, 0x09, false>;
using MagEvolutionTableGCNTE = BinaryMagEvolutionTableT<HeaderV2V3V4<true>, RootV2V3V4<true>, 0x3A, 0x09, true>;
using MagEvolutionTableGC = BinaryMagEvolutionTableT<HeaderV2V3V4<true>, RootV2V3V4<true>, 0x43, 0x12, true>;
using MagEvolutionTableXB = BinaryMagEvolutionTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x43, 0x12, false>;
using MagEvolutionTableV4 = BinaryMagEvolutionTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x53, 0x12, false>;

std::shared_ptr<MagEvolutionTable> MagEvolutionTable::create(
    std::shared_ptr<const std::string> data, Version version) {
  switch (version) {
    case Version::DC_NTE:
      return std::make_shared<MagEvolutionTableDCNTE>();
    case Version::DC_11_2000:
      return std::make_shared<MagEvolutionTableDC112000>(data);
    case Version::DC_V1:
      return std::make_shared<MagEvolutionTableV1>(data);
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return std::make_shared<MagEvolutionTableV2>(data);
    case Version::GC_NTE:
      return std::make_shared<MagEvolutionTableGCNTE>(data);
    case Version::GC_V3:
    case Version::GC_EP3:
    case Version::GC_EP3_NTE:
      return std::make_shared<MagEvolutionTableGC>(data);
    case Version::XB_V3:
      return std::make_shared<MagEvolutionTableXB>(data);
    case Version::BB_V4:
      return std::make_shared<MagEvolutionTableV4>(data);
    default:
      throw std::logic_error("Cannot create mag evolution table for this version");
  }
}
