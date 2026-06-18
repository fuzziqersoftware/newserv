#include "MagMetadataTable.hh"

#include "CommonFileFormats.hh"

MagMetadataTable::MotionReferences MagMetadataTable::MotionReferences::from_json(const phosg::JSON& json) {
  auto parse_side = [](Side& side, const phosg::JSON& side_json) -> void {
    side.eff_1 = side_json.at("Eff1").as_int();
    side.eff_2 = side_json.at("Eff2").as_int();
    side.eff_3 = side_json.at("Eff3").as_int();
    side.eff_4_8 = side_json.at("Eff48").as_int();
    side.eff_5 = side_json.at("Eff5").as_int();
    side.eff_6_7 = side_json.at("Eff67").as_int();
  };
  MagMetadataTable::MotionReferences ret;
  parse_side(ret.sides[0], json.at("Left"));
  parse_side(ret.sides[1], json.at("Right"));
  return ret;
}

phosg::JSON MagMetadataTable::MotionReferences::json() const {
  auto serialize_side = [](const Side& side) -> phosg::JSON {
    return phosg::JSON::dict({
        {"Eff1", side.eff_1},
        {"Eff2", side.eff_2},
        {"Eff3", side.eff_3},
        {"Eff48", side.eff_4_8},
        {"Eff5", side.eff_5},
        {"Eff67", side.eff_6_7},
    });
  };
  return phosg::JSON::dict({{"Left", serialize_side(this->sides[0])}, {"Right", serialize_side(this->sides[1])}});
}

MagMetadataTable::UnknownA3Entry MagMetadataTable::UnknownA3Entry::from_json(const phosg::JSON& json) {
  return UnknownA3Entry{
      .flags = static_cast<uint8_t>(json.get_int("Flags")),
      .unknown_a2 = static_cast<uint8_t>(json.get_int("UnknownA2")),
      .unknown_a3 = static_cast<int16_t>(json.get_int("UnknownA3")),
      .unknown_a4 = static_cast<int16_t>(json.get_int("UnknownA4")),
      .unknown_a5 = static_cast<int16_t>(json.get_int("UnknownA5")),
  };
}

phosg::JSON MagMetadataTable::UnknownA3Entry::json() const {
  return phosg::JSON::dict({
      {"Flags", this->flags},
      {"UnknownA2", this->unknown_a2},
      {"UnknownA3", this->unknown_a3},
      {"UnknownA4", this->unknown_a4},
      {"UnknownA5", this->unknown_a5},
  });
}

static VectorXYZTF color_for_json(const phosg::JSON& json) {
  return VectorXYZTF{
      .x = json.get_float(0),
      .y = json.get_float(1),
      .z = json.get_float(2),
      .t = json.get_float(3),
  };
}

static phosg::JSON json_for_color(const VectorXYZTF& color) {
  return phosg::JSON::list({color.x.load(), color.y.load(), color.z.load(), color.t.load()});
}

template <bool BE>
struct MotionReferenceTables {
  // It seems that there are two definition tables, but only the first is used on any version of PSO. On v3 and later,
  // the two offsets point to the same table, but on v2 they don't and the second table contains different data.
  // TODO: Figure out what the deal is with the different v2 tables.
  U32T<BE> first_ref_table;
  U32T<BE> second_ref_table;
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
  //   - Set base_index to player->visual.sh.skin if player is an android, or player->visual.costume otherwise
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
  S16T<BE> unknown_a3;
  S16T<BE> unknown_a4;
  S16T<BE> unknown_a5;
  UnknownA3EntryT(const MagMetadataTable::UnknownA3Entry& e)
      : flags(e.flags),
        unknown_a2(e.unknown_a2),
        unknown_a3(e.unknown_a3),
        unknown_a4(e.unknown_a4),
        unknown_a5(e.unknown_a5) {}
  operator MagMetadataTable::UnknownA3Entry() const {
    return MagMetadataTable::UnknownA3Entry{
        this->flags, this->unknown_a2, this->unknown_a3, this->unknown_a4, this->unknown_a5};
  }
} __packed_ws_be__(UnknownA3EntryT, 0x08);

class JSONMagMetadataTable : public MagMetadataTable {
public:
  explicit JSONMagMetadataTable(const phosg::JSON& json) {
    for (const auto& mag_json : json.at("Mags").as_list()) {
      const auto& unknown_a2_json = mag_json->at("UnknownA2").as_list();
      auto& mag = this->mags.emplace_back();
      mag.first_motion_refs = MotionReferences::from_json(mag_json->at("MotionRefs1"));
      mag.second_motion_refs = MotionReferences::from_json(mag_json->at("MotionRefs2"));
      mag.unknown_a2 = std::make_pair(unknown_a2_json.at(0)->as_int(), unknown_a2_json.at(1)->as_int());
      mag.render_flags = mag_json->at("RenderFlags").as_int();
      mag.evolution_number = mag_json->at("EvolutionNumber").as_int();
    }
    for (const auto& a3_json : json.at("UnknownA3").as_list()) {
      this->unknown_a3.emplace_back(UnknownA3Entry::from_json(*a3_json));
    }
    for (const auto& color_json : json.at("Colors").as_list()) {
      this->colors.emplace_back(color_for_json(*color_json));
    }
  }

  virtual ~JSONMagMetadataTable() = default;

  virtual size_t num_mags() const {
    return this->mags.size();
  }

  virtual size_t num_motion_entries(bool) const {
    return this->mags.size();
  }
  virtual const MotionReferences& get_motion_references(bool use_second_table, size_t data1_1) const {
    const auto& mag = this->mags.at(data1_1);
    return use_second_table ? mag.second_motion_refs : mag.first_motion_refs;
  }

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t data1_1) const {
    return this->mags.at(data1_1).unknown_a2;
  }

  virtual size_t num_unknown_a3_entries() const {
    return this->unknown_a3.size();
  }
  virtual const UnknownA3Entry& get_unknown_a3(size_t index) const {
    return this->unknown_a3.at(index);
  }

  virtual uint8_t get_render_flags(size_t data1_1) const {
    return this->mags.at(data1_1).render_flags;
  }

  virtual size_t num_colors() const {
    return this->colors.size();
  }
  virtual const VectorXYZTF& get_color_rgba(size_t index) const {
    return this->colors.at(index);
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    return this->mags.at(data1_1).evolution_number;
  }

protected:
  struct Mag {
    MotionReferences first_motion_refs;
    MotionReferences second_motion_refs;
    std::pair<uint8_t, uint8_t> unknown_a2 = std::make_pair(0, 0);
    uint8_t render_flags = 0;
    uint8_t evolution_number = 0;
  };
  std::vector<Mag> mags;
  std::vector<UnknownA3Entry> unknown_a3;
  std::vector<VectorXYZTF> colors;
};

struct HeaderV1 {
  parray<uint8_t, 4> unknown_a1 = {0x0F, 0xF0, 0x00, 0x00};
  le_uint32_t unknown_a2 = 0x00000003;
  le_uint16_t unknown_a3 = 0x00C8;
  le_uint16_t unknown_a4 = 0x0078;
  // unknown_a5 added in V2
  le_float unknown_a6 = 0.25; // 3E800000
  le_float unknown_a7 = 0.099999994; // 3DCCCCCC
  le_uint32_t unknown_a8 = 0x00000C00;
} __packed_ws__(HeaderV1, 0x18);

template <bool BE>
struct HeaderV2V3V4 {
  parray<uint8_t, 4> unknown_a1 = {0x0F, 0xF0, 0x00, 0x00};
  U32T<BE> unknown_a2 = 0x00000003;
  U16T<BE> unknown_a3 = 0x00C8;
  U16T<BE> unknown_a4 = 0x0078;
  parray<uint8_t, 4> unknown_a5 = {0xC8, 0x00, 0x00, 0x00};
  F32T<BE> unknown_a6 = 0.25; // 3E800000
  F32T<BE> unknown_a7 = 0.099999994; // 3DCCCCCC
  U32T<BE> unknown_a8 = 0x00000C00;
} __packed_ws_be__(HeaderV2V3V4, 0x1C);

// Fields:
// 112K /   V1 /   V2 /   V3 /   BB R
// 0018 / 0018 / 001C / 001C / 001C   motion_tables.first_ref_table // -> MotionReferences[NumMags]
// 0228 / 0228 / 02D4 / 001C / 001C   motion_tables.second_ref_table // -> MotionReferences[NumMags]
// 0438 / 0438 / 05BC / 0340 / 0400 * motion_tables; // -> MotionReferenceTables
// 0440 / 0440 / 0594 / 0348 / 0408 * unknown_a2; // -> (uint8_t[2])[NumMags] (references into unknown_a3)
// 0498 / 0498 / 0608 / 03CE / 04AE * unknown_a3; // -> UnknownA3Entry[max(unknown_a2) + 1]
// 0510 / 0520 / 06B0 / 0476 / 0556 * render_flags; // -> uint8_t[NumMags]
// 053C / 054C / 06EC / 04BC / 05AC * color_table; // -> ColorEntry[NumColors]
// ---- / ---- / 077C / 05DC / 06CC * evolution_number_table; // -> uint8_t[NumMags]

template <bool BE>
struct RootV1 {
  U32T<BE> motion_tables;
  U32T<BE> unknown_a2;
  U32T<BE> unknown_a3;
  U32T<BE> render_flags;
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
    throw std::runtime_error("invalid mag number");
  }
  return v1_evolution_number_table[data1_1];
}

template <typename HeaderT, typename RootT, size_t NumMags, size_t NumColors, bool BE>
class BinaryMagMetadataTableT : public MagMetadataTable {
public:
  explicit BinaryMagMetadataTableT(std::shared_ptr<const std::string> data)
      : data(data), r(*data), root(&r.pget<RootT>(this->r.pget<U32T<BE>>(this->data->size() - 0x10))) {}
  virtual ~BinaryMagMetadataTableT() = default;

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
    return get_rel_array_count<MotionReferences>(
        this->all_start_offsets(), use_second_table ? tables.second_ref_table : tables.first_ref_table);
  }

  virtual const MotionReferences& get_motion_references(bool use_second_table, size_t index) const {
    if (index >= this->num_motion_entries(use_second_table)) {
      throw std::logic_error("Invalid motion reference index");
    }
    const auto& tables = this->r.pget<MotionReferenceTables<BE>>(this->root->motion_tables);
    uint32_t array_offset = use_second_table ? tables.second_ref_table : tables.first_ref_table;
    return this->r.pget<MotionReferences>(array_offset + sizeof(MotionReferences) * index);
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

  virtual uint8_t get_render_flags(size_t index) const {
    if (index >= this->num_mags()) {
      throw std::logic_error("Invalid render_flags index");
    }
    return this->r.pget_u8(this->root->render_flags + index);
  }

  virtual size_t num_colors() const {
    return NumColors;
  }

  virtual const VectorXYZTF& get_color_rgba(size_t index) const {
    if (index >= NumColors) {
      throw std::runtime_error("invalid mag color index");
    }
    return this->add_to_vector_cache<ColorEntry<BE>>(this->colors, this->root->color_table, index);
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    if (data1_1 >= this->num_mags()) {
      throw std::logic_error("Invalid evolution_number index");
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

  static std::string serialize(const MagMetadataTable& table) {
    RELFileWriter<BE> rel;
    RootT root;

    rel.template put<HeaderT>(HeaderT());

    MotionReferenceTables<BE> motion_ref_tables;
    motion_ref_tables.first_ref_table = rel.w.size();
    bool alias_motion_ref_tables = true;
    for (size_t z = 0; z < table.num_motion_entries(false); z++) {
      const auto& refs = table.get_motion_references(false, z);
      rel.template put<MotionReferences>(refs);
      if (refs != table.get_motion_references(true, z)) {
        alias_motion_ref_tables = false;
      }
    }

    if (alias_motion_ref_tables) {
      motion_ref_tables.second_ref_table = motion_ref_tables.first_ref_table;
    } else {
      motion_ref_tables.second_ref_table = rel.w.size();
      for (size_t z = 0; z < table.num_motion_entries(true); z++) {
        rel.template put<MotionReferences>(table.get_motion_references(true, z));
      }
    }

    root.motion_tables = rel.w.size();
    rel.template put<MotionReferenceTables<BE>>(motion_ref_tables);
    rel.relocations.emplace(root.motion_tables);
    rel.relocations.emplace(root.motion_tables + 4);

    root.unknown_a2 = rel.w.size();
    for (size_t z = 0; z < table.num_mags(); z++) {
      auto [left, right] = table.get_unknown_a2(z);
      rel.template put<uint8_t>(left);
      rel.template put<uint8_t>(right);
    }

    root.unknown_a3 = rel.w.size();
    for (size_t z = 0; z < table.num_unknown_a3_entries(); z++) {
      rel.template put<UnknownA3EntryT<BE>>(table.get_unknown_a3(z));
    }

    root.render_flags = rel.w.size();
    for (size_t z = 0; z < table.num_mags(); z++) {
      rel.template put<uint8_t>(table.get_render_flags(z));
    }

    rel.align(4);
    root.color_table = rel.w.size();
    for (size_t z = 0; z < table.num_colors(); z++) {
      rel.template put<ColorEntry<BE>>(table.get_color_rgba(z));
    }

    if constexpr (requires { root.evolution_number_table; }) {
      root.evolution_number_table = rel.w.size();
      for (size_t z = 0; z < table.num_mags(); z++) {
        rel.template put<uint8_t>(table.get_evolution_number(z));
      }
    }

    rel.align(4);
    uint32_t root_offset = rel.template put<RootT>(root);
    for (size_t z = 1; z <= sizeof(RootT) / 4; z++) {
      rel.relocations.emplace(rel.w.size() - (z * 4));
    }

    return rel.finalize(root_offset);
  }

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const RootT* root;
  mutable std::set<uint32_t> start_offsets;
  mutable std::vector<UnknownA3Entry> unknown_a3_entries;
  mutable std::vector<VectorXYZTF> colors;
};

class MagMetadataTableDCNTE : public MagMetadataTable {
public:
  MagMetadataTableDCNTE() = default;
  virtual ~MagMetadataTableDCNTE() = default;

  virtual size_t num_mags() const {
    return 0x2C;
  }

  virtual size_t num_motion_entries(bool) const {
    return 0;
  }
  virtual const MotionReferences& get_motion_references(bool, size_t) const {
    throw std::runtime_error("Mag tables not available on DC NTE");
  }

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t) const {
    throw std::runtime_error("Mag tables not available on DC NTE");
  }

  virtual size_t num_unknown_a3_entries() const {
    return 0;
  }
  virtual const UnknownA3Entry& get_unknown_a3(size_t) const {
    throw std::runtime_error("Mag tables not available on DC NTE");
  }

  virtual uint8_t get_render_flags(size_t) const {
    throw std::runtime_error("Mag tables not available on DC NTE");
  }

  virtual size_t num_colors() const {
    return 0;
  }
  virtual const VectorXYZTF& get_color_rgba(size_t) const {
    throw std::runtime_error("Mag tables not available on DC NTE");
  }

  virtual uint8_t get_evolution_number(uint8_t data1_1) const {
    return get_v1_mag_evolution_number(data1_1);
  }
};

using MagMetadataTableDC112000 = BinaryMagMetadataTableT<HeaderV1, RootV1<false>, 0x2C, 0x09, false>;
using MagMetadataTableV1 = BinaryMagMetadataTableT<HeaderV1, RootV1<false>, 0x2C, 0x09, false>;
using MagMetadataTableV2 = BinaryMagMetadataTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x3A, 0x09, false>;
using MagMetadataTableGCNTE = BinaryMagMetadataTableT<HeaderV2V3V4<true>, RootV2V3V4<true>, 0x3A, 0x09, true>;
using MagMetadataTableGC = BinaryMagMetadataTableT<HeaderV2V3V4<true>, RootV2V3V4<true>, 0x43, 0x12, true>;
using MagMetadataTableXB = BinaryMagMetadataTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x43, 0x12, false>;
using MagMetadataTableV4 = BinaryMagMetadataTableT<HeaderV2V3V4<false>, RootV2V3V4<false>, 0x53, 0x12, false>;

std::shared_ptr<MagMetadataTable> MagMetadataTable::from_binary(
    std::shared_ptr<const std::string> data, Version version) {
  switch (version) {
    case Version::DC_NTE:
      return std::make_shared<MagMetadataTableDCNTE>();
    case Version::DC_11_2000:
      return std::make_shared<MagMetadataTableDC112000>(data);
    case Version::DC_V1:
      return std::make_shared<MagMetadataTableV1>(data);
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return std::make_shared<MagMetadataTableV2>(data);
    case Version::GC_NTE:
      return std::make_shared<MagMetadataTableGCNTE>(data);
    case Version::GC_V3:
    case Version::GC_EP3:
    case Version::GC_EP3_NTE:
      return std::make_shared<MagMetadataTableGC>(data);
    case Version::XB_V3:
      return std::make_shared<MagMetadataTableXB>(data);
    case Version::BB_V4:
      return std::make_shared<MagMetadataTableV4>(data);
    default:
      throw std::logic_error("Cannot create mag metadata table for this version");
  }
}

std::shared_ptr<MagMetadataTable> MagMetadataTable::from_json(const phosg::JSON& json) {
  return std::make_shared<JSONMagMetadataTable>(json);
}

phosg::JSON MagMetadataTable::json() const {
  if (this->num_motion_entries(true) != this->num_motion_entries(false)) {
    throw std::runtime_error("Motion entry counts differ across tables");
  }
  if (this->num_motion_entries(false) != this->num_mags()) {
    throw std::runtime_error(std::format("Motion entry count {} does not match mag count {}",
        this->num_motion_entries(false), this->num_mags()));
  }

  auto mags_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_mags(); z++) {
    auto mag_json = phosg::JSON::dict();
    mag_json.emplace("MotionRefs1", this->get_motion_references(false, z).json());
    mag_json.emplace("MotionRefs2", this->get_motion_references(true, z).json());
    auto unknown_a2 = this->get_unknown_a2(z);
    mag_json.emplace("UnknownA2", phosg::JSON::list({unknown_a2.first, unknown_a2.second}));
    mag_json.emplace("RenderFlags", this->get_render_flags(z));
    mag_json.emplace("EvolutionNumber", this->get_evolution_number(z));
    mags_json.emplace_back(std::move(mag_json));
  }

  auto unknown_a3_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_unknown_a3_entries(); z++) {
    unknown_a3_json.emplace_back(this->get_unknown_a3(z).json());
  }

  auto colors_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_colors(); z++) {
    colors_json.emplace_back(json_for_color(this->get_color_rgba(z)));
  }

  return phosg::JSON::dict({
      {"Mags", std::move(mags_json)},
      {"UnknownA3", std::move(unknown_a3_json)},
      {"Colors", std::move(colors_json)},
  });
}

std::string MagMetadataTable::serialize_binary(Version version) const {
  switch (version) {
    case Version::DC_NTE:
      throw std::runtime_error("DC NTE does not have a an ItemMagEdit format");
    case Version::DC_11_2000:
      return MagMetadataTableDC112000::serialize(*this);
    case Version::DC_V1:
      return MagMetadataTableV1::serialize(*this);
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return MagMetadataTableV2::serialize(*this);
    case Version::GC_NTE:
      return MagMetadataTableGCNTE::serialize(*this);
    case Version::GC_V3:
    case Version::GC_EP3:
    case Version::GC_EP3_NTE:
      return MagMetadataTableGC::serialize(*this);
    case Version::XB_V3:
      return MagMetadataTableXB::serialize(*this);
    case Version::BB_V4:
      return MagMetadataTableV4::serialize(*this);
    default:
      throw std::logic_error("Cannot create item parameter table for this version");
  }
}
