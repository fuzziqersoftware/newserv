#include "MagEvolutionTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

struct MotionReference {
  struct Side {
    // This specifies which entry in ItemMagMotion.dat is used. The file is just a list of 0x64-byte structures.
    // 0xFF = no TItemMagSub is created
    uint8_t motion_table_entry = 0xFF;
    parray<uint8_t, 5> unknown_a1 = 0;
  } __packed_ws__(Side, 0x06);
  parray<Side, 2> sides; // [0] = right side, [1] = left side
} __packed_ws__(MotionReference, 0x0C);

template <bool BE>
struct MotionReferenceTables {
  // It seems that there are two definition tables, but only the first is used on any version of PSO. On v3 and later,
  // the two offsets point to the same table, but on v2 they don't and the second table contains different data.
  // TODO: Figure out what the deal is with the different v2 tables.
  U32T<BE> ref_table; // -> MotionReference[num_mags]
  U32T<BE> unused_ref_table; // -> MotionReference[num_mags]
} __packed_ws_be__(MotionReferenceTables, 0x08);

template <bool BE>
struct ColorEntry {
  // Colors are specified as 4 floats, each in the range [0, 1], for each color channel. The default colors are:
  //   alpha   red     green   blue    color (see StaticGameData.cc)
  //   1.0     1.0     0.2     0.1     red
  //   1.0     0.2     0.2     1.0     blue
  //   1.0     1.0     0.9     0.1     yellow
  //   1.0     0.1     1.0     0.1     green
  //   1.0     0.8     0.1     1.0     purple
  //   1.0     0.1     0.1     0.2     black
  //   1.0     0.9     1.0     1.0     white
  //   1.0     0.1     0.9     1.0     cyan
  //   1.0     0.5     0.3     0.2     brown
  //   1.0     1.0     0.4     0.0     orange (v3+)
  //   1.0     0.502   0.545   0.977   light-blue (v3+)
  //   1.0     0.502   0.502   0.0     olive (v3+)
  //   1.0     0.0     0.941   0.714   turquoise (v3+)
  //   1.0     0.8     0.098   0.392   fuchsia (v3+)
  //   1.0     0.498   0.498   0.498   grey (v3+)
  //   1.0     0.996   0.996   0.832   cream (v3+)
  //   1.0     0.996   0.498   0.784   pink (v3+)
  //   1.0     0.0     0.498   0.322   dark-green (v3+)
  F32T<BE> alpha;
  F32T<BE> red;
  F32T<BE> green;
  F32T<BE> blue;
} __packed_ws_be__(ColorEntry, 0x10);

template <bool BE>
struct UnknownA3Entry {
  uint8_t flags;
  uint8_t unknown_a2;
  U16T<BE> unknown_a3;
  U16T<BE> unknown_a4;
  U16T<BE> unknown_a5;
} __packed_ws_be__(UnknownA3Entry, 0x08);

template <bool BE>
struct RootV2V3V4 {
  /* -- / 112K / V1   / V2   / V3   / BB */
  /* 00 / 0438 / 0438 / 05BC / 0340 / 0400 */ U32T<BE> motion_tables; // -> MotionReferenceTables
  /* 04 / 0440 / 0440 / 0594 / 0348 / 0408 */ U32T<BE> unknown_a2; // -> (uint8_t[2])[NumMags] (references into unknown_a3)
  /* 08 / 0498 / 0498 / 0608 / 03CE / 04AE */ U32T<BE> unknown_a3; // -> UnknownA3Entry[max(unknown_a2) + 1]
  /* 0C / 0510 / 0520 / 06B0 / 0476 / 0556 */ U32T<BE> unknown_a4; // -> uint8_t[NumMags]
  /* 10 / 053C / 054C / 06EC / 04BC / 05AC */ U32T<BE> color_table; // -> ColorEntry[NumColors]
  /* 14 /      /      / 077C / 05DC / 06CC */ U32T<BE> evolution_number_table; // -> uint8_t[NumMags]
} __packed_ws_be__(RootV2V3V4, 0x18);

struct RootV1 {
  le_uint32_t motion_tables;
  le_uint32_t unknown_a2;
  le_uint32_t unknown_a3;
  le_uint32_t unknown_a4;
  le_uint32_t color_table;
} __packed_ws__(RootV1, 0x14);

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

template <typename RootT, size_t NumMags, size_t NumColors, bool BE>
class MagEvolutionTableT : public MagEvolutionTable {
public:
  explicit MagEvolutionTableT(std::shared_ptr<const std::string> data)
      : data(data), r(*data), root(&r.pget<RootT>(this->r.pget_u32l(this->data->size() - 0x10))) {}
  virtual ~MagEvolutionTableT() = default;

  virtual VectorXYZTF get_color_rgba(size_t index) const {
    if (index >= NumColors) {
      throw runtime_error("invalid mag color index");
    }
    const auto& color = this->r.pget<ColorEntry<BE>>(this->root->color_table + sizeof(ColorEntry<BE>) * index);
    return {color.red.load(), color.green.load(), color.blue.load(), color.alpha.load()};
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    if constexpr (requires { this->root->evolution_number_table; }) {
      return this->r.pget_u8(this->root->evolution_number_table + data1_1);
    } else {
      return get_v1_mag_evolution_number(data1_1);
    }
  }

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const RootT* root;
};

class MagEvolutionTableDCNTE : public MagEvolutionTable {
public:
  MagEvolutionTableDCNTE() = default;
  virtual ~MagEvolutionTableDCNTE() = default;

  virtual VectorXYZTF get_color_rgba(size_t) const {
    throw runtime_error("mag colors not available on DC NTE");
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    return get_v1_mag_evolution_number(data1_1);
  }
};

using MagEvolutionTableDC112000 = MagEvolutionTableT<RootV2V3V4<false>, 0x28, 0x09, false>;
using MagEvolutionTableV1 = MagEvolutionTableT<RootV2V3V4<false>, 0x28, 0x09, false>;
using MagEvolutionTableV2 = MagEvolutionTableT<RootV2V3V4<false>, 0x3A, 0x09, false>;
using MagEvolutionTableGCNTE = MagEvolutionTableT<RootV2V3V4<true>, 0x3A, 0x09, true>;
using MagEvolutionTableGC = MagEvolutionTableT<RootV2V3V4<true>, 0x43, 0x12, true>;
using MagEvolutionTableXB = MagEvolutionTableT<RootV2V3V4<false>, 0x43, 0x12, false>;
using MagEvolutionTableV4 = MagEvolutionTableT<RootV2V3V4<false>, 0x53, 0x12, false>;

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
