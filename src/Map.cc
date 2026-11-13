#include "Map.hh"

#include <stdio.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <resource_file/Emulators/Expression.hh>

#include "CommonFileFormats.hh"
#include "ItemCreator.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "Quest.hh"
#include "StaticGameData.hh"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set data table (variations index)

std::string Variations::str() const {
  std::string ret = "";
  for (size_t z = 0; z < this->entries.size(); z++) {
    const auto& e = this->entries[z];
    if (!ret.empty()) {
      ret += ",";
    }
    ret += std::format("{:02X}:[{:X},{:X}]", z, e.layout, e.entities);
  }
  return ret;
}

phosg::JSON Variations::json() const {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < this->entries.size(); z++) {
    const auto& e = this->entries[z];
    ret.emplace_back(phosg::JSON::dict({{"layout", e.layout.load()}, {"entities", e.entities.load()}}));
  }
  return ret;
}

SetDataTableBase::SetDataTableBase(Version version) : version(version) {}

Variations SetDataTableBase::generate_variations(
    Episode episode, bool is_solo, std::shared_ptr<RandomGenerator> rand_crypt) const {
  Variations ret;
  for (size_t floor = 0; floor < ret.entries.size(); floor++) {
    auto& e = ret.entries[floor];
    auto num_vars = this->num_free_play_variations_for_floor(episode, is_solo, floor);
    e.layout = (num_vars.layout > 1) ? (rand_crypt->next() % num_vars.layout) : 0;
    e.entities = (num_vars.entities > 1) ? (rand_crypt->next() % num_vars.entities) : 0;
  }
  return ret;
}

std::vector<std::string> SetDataTableBase::map_filenames_for_variations(
    Episode episode, GameMode mode, const Variations& variations, FilenameType type) const {
  std::vector<std::string> ret;
  for (uint8_t floor = 0; floor < 0x10; floor++) {
    const auto& e = variations.entries[floor];
    ret.emplace_back(this->map_filename_for_variation(episode, mode, floor, e.layout, e.entities, type));
  }
  for (uint8_t floor = 0x10; floor < 0x12; floor++) {
    ret.emplace_back(this->map_filename_for_variation(episode, mode, floor, 0, 0, type));
  }
  return ret;
}

std::array<uint8_t, 0x12> SetDataTableBase::default_floor_to_area(Version version, Episode episode) {
  // For some inscrutable reason, Pioneer 2's area number in Episode 4 is discontiguous with all the rest. Why, Sega??
  static const std::array<uint8_t, 0x12> areas_ep1{
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};
  static const std::array<uint8_t, 0x12> areas_ep2_gc_nte{
      0x00, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0xFF, 0xFF};
  static const std::array<uint8_t, 0x12> areas_ep2{
      0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23};
  static const std::array<uint8_t, 0x12> areas_ep3{
      0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF};
  static const std::array<uint8_t, 0x12> areas_ep4{
      0x2D, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  switch (episode) {
    case Episode::EP1:
      return areas_ep1;
    case Episode::EP2:
      return (version == Version::GC_NTE) ? areas_ep2_gc_nte : areas_ep2;
    case Episode::EP3:
      return areas_ep3;
    case Episode::EP4:
      return areas_ep4;
    default:
      throw std::logic_error("incorrect episode");
  }
}

std::array<uint8_t, 0x12> SetDataTableBase::default_floor_to_area(Episode episode) const {
  return this->default_floor_to_area(this->version, episode);
}

SetDataTable::SetDataTable(Version version, const std::string& data) : SetDataTableBase(version) {
  if (is_big_endian(this->version)) {
    this->load_table_t<true>(data);
  } else {
    this->load_table_t<false>(data);
  }
}

template <bool BE>
void SetDataTable::load_table_t(const std::string& data) {
  using FooterT = RELFileFooterT<BE>;

  phosg::StringReader r(data);

  if (r.size() < sizeof(FooterT)) {
    throw std::runtime_error("set data table is too small");
  }
  auto& footer = r.pget<FooterT>(r.size() - sizeof(FooterT));

  uint32_t root_table_offset = r.pget<U32T<BE>>(footer.root_offset);
  auto root_r = r.sub(root_table_offset, footer.root_offset - root_table_offset);
  while (!root_r.eof()) {
    auto& layout_v = this->entries.emplace_back();
    uint32_t layout_table_offset = root_r.template get<U32T<BE>>();
    uint32_t layout_table_count = root_r.template get<U32T<BE>>();
    auto layout_r = r.sub(layout_table_offset, layout_table_count * 0x08);
    while (!layout_r.eof()) {
      auto& entities_v = layout_v.emplace_back();
      uint32_t entities_table_offset = layout_r.get<U32T<BE>>();
      uint32_t entities_table_count = layout_r.get<U32T<BE>>();
      auto entities_r = r.sub(entities_table_offset, entities_table_count * 0x0C);
      while (!entities_r.eof()) {
        auto& entry = entities_v.emplace_back();
        entry.object_list_basename = r.pget_cstr(entities_r.get<U32T<BE>>());
        entry.enemy_and_event_list_basename = r.pget_cstr(entities_r.get<U32T<BE>>());
        entry.area_setup_filename = r.pget_cstr(entities_r.get<U32T<BE>>());
      }
    }
  }
}

Variations::Entry SetDataTable::num_available_variations_for_floor(Episode episode, uint8_t floor) const {
  uint8_t area = this->default_floor_to_area(episode).at(floor);
  if (area == 0xFF) {
    return Variations::Entry{.layout = 1, .entities = 1};
  } else {
    if (area >= this->entries.size()) {
      return Variations::Entry{.layout = 1, .entities = 1};
    }
    const auto& e = this->entries[area];
    return Variations::Entry{.layout = e.size(), .entities = e.at(0).size()};
  }
}

Variations::Entry SetDataTable::num_free_play_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const {
  uint8_t area = this->default_floor_to_area(episode).at(floor);
  if (area == 0xFF) {
    return Variations::Entry{.layout = 1, .entities = 1};
  }
  static const std::array<uint32_t, 0x2F * 2> counts_on{
      // Episode 1 (00-11)
      // P2 -F1-, -F2-, -C1-, -C2-, -C3-, -M1-, -M2-, -R1-, -R2-, -R3-, DRGN, DRL-, -VO-, -DF-, LOBBY, VS1-, VS2-,
      1, 1, 1, 5, 1, 5, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 10, 1, 1, 1, 1, 1,
      // Episode 2 (12-23)
      // P2 VRTA, VRTB, VRSA, VRSB, CCA-, -JN-, -JS-, MNTN, SEAS, SBU-, SBL-, -GG-, -OF-, -BR-, -GD-, SSN-, TWR-,
      1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 1, 3, 1, 3, 2, 2, 1, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      // Episode 4 (24-2E)
      // CE -CW-, -CS-, -CN-, -CI-, DES1, DES2, DES3, SMIL, -P2-, TEST
      1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1};
  static const std::array<uint32_t, 0x2F * 2> counts_off{
      // Episode 1 (00-11)
      // P2 -F1-, -F2-, -C1-, -C2-, -C3-, -M1-, -M2-, -R1-, -R2-, -R3-, DRGN, DRL-, -VO-, -DF-, LOBBY, VS1-, VS2-,
      1, 1, 1, 3, 1, 3, 3, 1, 3, 1, 3, 1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 10, 1, 1, 1, 1, 1,
      // Episode 2 (12-23)
      // P2 VRTA, VRTB, VRSA, VRSB, CCA-, -JN-, -JS-, MNTN, SEAS, SBU-, SBL-, -GG-, -OF-, -BR-, -GD-, SSN-, TWR-,
      1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 1, 3, 1, 3, 2, 2, 1, 3, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      // Episode 4 (24-2E)
      // CE -CW-, -CS-, -CN-, -CI-, DES1, DES2, DES3, SMIL, -P2-, TEST
      1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1};
  const auto& data = is_solo ? counts_off : counts_on;
  if (static_cast<size_t>(floor * 2 + 1) < data.size()) {
    auto available = this->num_available_variations_for_floor(episode, floor);
    return Variations::Entry{
        .layout = std::min<uint32_t>(available.layout, data[area * 2]),
        .entities = std::min<uint32_t>(available.entities, data[area * 2 + 1]),
    };
  }
  throw std::runtime_error("invalid area");
}

std::string SetDataTable::map_filename_for_variation(
    Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  uint8_t area = this->default_floor_to_area(episode).at(floor);
  if (area == 0xFF) {
    return "";
  }

  if (area >= this->entries.size()) {
    return "";
  }

  const auto& entry = this->entries.at(area).at(layout).at(entities);

  std::string filename;
  switch (type) {
    case FilenameType::OBJECT_SETS:
      filename = entry.object_list_basename + "o";
      break;
    case FilenameType::ENEMY_SETS:
      filename = entry.enemy_and_event_list_basename + "e";
      break;
    case FilenameType::EVENTS:
      filename = entry.enemy_and_event_list_basename;
      break;
    default:
      throw std::logic_error("invalid map filename type");
  }

  bool is_events = (type == FilenameType::EVENTS);
  switch ((floor != 0) ? GameMode::NORMAL : mode) {
    case GameMode::NORMAL:
      filename += is_events ? ".evt" : ".dat";
      break;
    case GameMode::SOLO:
      filename += is_events ? "_s.evt" : "_s.dat";
      break;
    case GameMode::CHALLENGE:
      filename += is_events ? "_c1.evt" : "_c1.dat";
      break;
    case GameMode::BATTLE:
      filename += is_events ? "_d.evt" : "_d.dat";
      break;
    default:
      throw std::logic_error("invalid game mode");
  }

  return filename;
}

std::string SetDataTable::str() const {
  std::vector<std::string> lines;
  lines.emplace_back(std::format("FL/V1/V2 => ----------------------OBJECT -----------------ENEMY+EVENT -----------------------SETUP\n"));
  for (size_t a = 0; a < this->entries.size(); a++) {
    const auto& v1_v = this->entries[a];
    for (size_t v1 = 0; v1 < v1_v.size(); v1++) {
      const auto& v2_v = v1_v[v1];
      for (size_t v2 = 0; v2 < v2_v.size(); v2++) {
        const auto& e = v2_v[v2];
        lines.emplace_back(std::format("{:02X}/{:02X}/{:02X} => {:>28} {:>28} {:>28}\n",
            a, v1, v2, e.object_list_basename, e.enemy_and_event_list_basename, e.area_setup_filename));
      }
    }
  }
  return phosg::join(lines, "");
}

struct AreaMapFileInfo {
  const char* name_token;
  std::vector<uint32_t> variation1_values;
  std::vector<uint32_t> variation2_values;

  AreaMapFileInfo(
      const char* name_token,
      std::vector<uint32_t> variation1_values,
      std::vector<uint32_t> variation2_values)
      : name_token(name_token),
        variation1_values(variation1_values),
        variation2_values(variation2_values) {}
};

const std::array<std::vector<std::vector<std::string>>, 0x12> SetDataTableDCNTE::NAMES = {{
    /* 00 */ {{"map_city00_00"}},
    /* 01 */ {{"map_forest01_00", "map_forest01_01"}},
    /* 02 */ {{"map_forest02_00", "map_forest02_03"}},
    /* 03 */ {{"map_cave01_00_00", "map_cave01_00_01"}, {"map_cave01_01_00", "map_cave01_01_01"}},
    /* 04 */ {{"map_cave02_00_00", "map_cave02_00_01"}, {"map_cave02_01_00", "map_cave02_01_01"}},
    /* 05 */ {{"map_cave03_00_00", "map_cave03_00_01"}, {"map_cave03_01_00", "map_cave03_01_01"}},
    /* 06 */ {{"map_machine01_00_00", "map_machine01_00_01"}},
    /* 07 */ {{"map_machine02_00_00", "map_machine02_00_01"}},
    /* 08 */ {{"map_ancient01_00_00", "map_ancient01_00_01"}, {"map_ancient01_01_00", "map_ancient01_01_01"}},
    /* 09 */ {{"map_ancient02_00_00", "map_ancient02_00_01"}, {"map_ancient02_01_00", "map_ancient02_01_01"}},
    /* 0A */ {{"map_ancient03_00_00", "map_ancient03_00_01"}, {"map_ancient03_01_00", "map_ancient03_01_01"}},
    /* 0B */ {{"map_boss01"}},
    /* 0C */ {{"map_boss02"}},
    /* 0D */ {{"map_boss03"}},
    /* 0E */ {{"map_boss04"}},
    /* 0F */ {{"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}},
    /* 10 */ {},
    /* 11 */ {},
}};

SetDataTableDCNTE::SetDataTableDCNTE() : SetDataTableBase(Version::DC_NTE) {}

Variations::Entry SetDataTableDCNTE::num_available_variations_for_floor(Episode, uint8_t floor) const {
  const auto& floor_names = this->NAMES.at(floor);
  return Variations::Entry{
      .layout = floor_names.size(),
      .entities = floor_names.empty() ? 0 : this->NAMES.at(floor)[0].size(),
  };
}

Variations::Entry SetDataTableDCNTE::num_free_play_variations_for_floor(Episode episode, bool, uint8_t floor) const {
  return this->num_available_variations_for_floor(episode, floor);
}

std::string SetDataTableDCNTE::map_filename_for_variation(
    Episode, GameMode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  try {
    std::string basename = this->NAMES.at(floor).at(layout).at(entities);
    switch (type) {
      case FilenameType::ENEMY_SETS:
        basename += "e.dat";
        break;
      case FilenameType::OBJECT_SETS:
        basename += "o.dat";
        break;
      case FilenameType::EVENTS:
        basename += ".evt";
        break;
      default:
        throw std::logic_error("invalid map filename type");
    }
    return basename;
  } catch (const std::out_of_range&) {
    return "";
  }
}

const std::array<std::vector<std::vector<std::string>>, 0x12> SetDataTableDC112000::NAMES = {{
    /* 00 */ {{"map_city00_00"}},
    /* 01 */ {{"map_forest01_00", "map_forest01_01", "map_forest01_02", "map_forest01_03", "map_forest01_04"}},
    /* 02 */ {{"map_forest02_00", "map_forest02_01", "map_forest02_02", "map_forest02_03", "map_forest02_04"}},
    /* 03 */ {{"map_cave01_00_00", "map_cave01_00_01"}, {"map_cave01_01_00", "map_cave01_01_01"}, {"map_cave01_02_00", "map_cave01_02_01"}},
    /* 04 */ {{"map_cave02_00_00", "map_cave02_00_01"}, {"map_cave02_01_00", "map_cave02_01_01"}, {"map_cave02_02_00", "map_cave02_02_01"}},
    /* 05 */ {{"map_cave03_00_00", "map_cave03_00_01"}, {"map_cave03_01_00", "map_cave03_01_01"}, {"map_cave03_02_00", "map_cave03_02_01"}},
    /* 06 */ {{"map_machine01_00_00", "map_machine01_00_01"}, {"map_machine01_01_00", "map_machine01_01_01"}, {"map_machine01_02_00", "map_machine01_02_01"}},
    /* 07 */ {{"map_machine02_00_00", "map_machine02_00_01"}, {"map_machine02_01_00", "map_machine02_01_01"}, {"map_machine02_02_00", "map_machine02_02_01"}},
    /* 08 */ {{"map_ancient01_00_00", "map_ancient01_00_01"}, {"map_ancient01_01_00", "map_ancient01_01_01"}, {"map_ancient01_02_00", "map_ancient01_02_01"}},
    /* 09 */ {{"map_ancient02_00_00", "map_ancient02_00_01"}, {"map_ancient02_01_00", "map_ancient02_01_01"}, {"map_ancient02_02_00", "map_ancient02_02_01"}},
    /* 0A */ {{"map_ancient03_00_00", "map_ancient03_00_01"}, {"map_ancient03_01_00", "map_ancient03_01_01"}, {"map_ancient03_02_00", "map_ancient03_02_01"}},
    /* 0B */ {{"map_boss01"}},
    /* 0C */ {{"map_boss02"}},
    /* 0D */ {{"map_boss03"}},
    /* 0E */ {{"map_boss04"}},
    /* 0F */ {{"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}, {"map_visuallobby"}},
    /* 10 */ {},
    /* 11 */ {},
}};

SetDataTableDC112000::SetDataTableDC112000() : SetDataTableBase(Version::DC_11_2000) {}

Variations::Entry SetDataTableDC112000::num_available_variations_for_floor(Episode, uint8_t floor) const {
  const auto& floor_names = this->NAMES.at(floor);
  return Variations::Entry{
      .layout = floor_names.size(),
      .entities = floor_names.empty() ? 0 : this->NAMES.at(floor)[0].size(),
  };
}

Variations::Entry SetDataTableDC112000::num_free_play_variations_for_floor(Episode episode, bool, uint8_t floor) const {
  return this->num_available_variations_for_floor(episode, floor);
}

std::string SetDataTableDC112000::map_filename_for_variation(
    Episode, GameMode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  try {
    std::string basename = this->NAMES.at(floor).at(layout).at(entities);
    switch (type) {
      case FilenameType::ENEMY_SETS:
        basename += "e.dat";
        break;
      case FilenameType::OBJECT_SETS:
        basename += "o.dat";
        break;
      case FilenameType::EVENTS:
        basename += ".evt";
        break;
      default:
        throw std::logic_error("invalid map filename type");
    }
    return basename;
  } catch (const std::out_of_range&) {
    return "";
  }
}

static const std::vector<AreaMapFileInfo> map_file_info_dc_nte = {
    {"city00", {}, {0}},
    {"forest01", {}, {0, 1}},
    {"forest02", {}, {0, 3}},
    {"cave01", {0, 1}, {0, 1}},
    {"cave02", {0, 1}, {0, 1}},
    {"cave03", {0, 1}, {0, 1}},
    {"machine01", {0}, {0, 1}},
    {"machine02", {0}, {0, 1}},
    {"ancient01", {0}, {0, 1}},
    {"ancient02", {0}, {0, 1}},
    {"ancient03", {0}, {0, 1}},
    {"boss01", {}, {}},
    {"boss02", {}, {}},
    {"boss03", {}, {}},
    {"boss04", {}, {}},
    {"map_visuallobby", {}, {}},
};

static const std::vector<std::vector<AreaMapFileInfo>> map_file_info_gc_nte = {
    {
        // Episode 1 Non-solo
        {"city00", {}, {0}},
        {"forest01", {}, {0, 1, 2, 3, 4}},
        {"forest02", {}, {0, 1, 2, 3, 4}},
        {"cave01", {0, 1, 2}, {0, 1}},
        {"cave02", {0, 1, 2}, {0, 1}},
        {"cave03", {0, 1, 2}, {0, 1}},
        {"machine01", {0, 1, 2}, {0, 1}},
        {"machine02", {0, 1, 2}, {0, 1}},
        {"ancient01", {0, 1, 2}, {0, 1}},
        {"ancient02", {0, 1, 2}, {0, 1}},
        {"ancient03", {0, 1, 2}, {0, 1}},
        {"boss01", {}, {}},
        {"boss02", {}, {}},
        {"boss03", {}, {}},
        {"boss04", {}, {}},
        {"lobby_01", {}, {}},
    },
    {
        // Episode 2 Non-solo
        {"labo00", {}, {0}},
        {"ruins01", {0}, {0}},
        {"ruins02", {0}, {0}},
        {"space01", {0, 1}, {0}},
        {"space02", {0, 1}, {0}},
        {"jungle01", {}, {0, 1}},
        {"jungle02", {}, {0, 1}},
        {"jungle03", {}, {0, 1}},
        {"jungle04", {0, 1}, {0}},
        {"jungle05", {}, {0, 1}},
        {"seabed01", {0, 1}, {0}},
        {"seabed02", {0}, {0}},
        {"boss05", {}, {}},
        {"boss06", {}, {}},
        {"boss07", {}, {}},
        {"boss08", {}, {}},
    },
};

// These are indexed as [episode][is_solo][floor], where episode is 0-2
static const std::vector<std::vector<std::vector<AreaMapFileInfo>>> map_file_info = {
    {
        // Episode 1
        {
            // Non-solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 1, 2, 3, 4}},
            {"forest02", {}, {0, 1, 2, 3, 4}},
            {"cave01", {0, 1, 2}, {0, 1}},
            {"cave02", {0, 1, 2}, {0, 1}},
            {"cave03", {0, 1, 2}, {0, 1}},
            {"machine01", {0, 1, 2}, {0, 1}},
            {"machine02", {0, 1, 2}, {0, 1}},
            {"ancient01", {0, 1, 2}, {0, 1}},
            {"ancient02", {0, 1, 2}, {0, 1}},
            {"ancient03", {0, 1, 2}, {0, 1}},
            {"boss01", {}, {}},
            {"boss02", {}, {}},
            {"boss03", {}, {}},
            {"boss04", {}, {}},
            {"lobby_01", {}, {}},
        },
        {
            // Solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 2, 4}},
            {"forest02", {}, {0, 3, 4}},
            {"cave01", {0, 1, 2}, {0}},
            {"cave02", {0, 1, 2}, {0}},
            {"cave03", {0, 1, 2}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
    {
        // Episode 2
        {
            // Non-solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0, 1}},
            {"seabed02", {0, 1}, {0, 1}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
        {
            // Solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0}},
            {"seabed02", {0, 1}, {0}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
    },
    {
        // Episode 4
        {
            // Non-solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {"test01", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
        {
            // Solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {"test01", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DAT file structure

struct DATEntityDefinition {
  // This field directly maps to the base_type field in ObjectSetEntry and EnemySetEntry.
  uint16_t base_type;
  // Each bit in this field indicates whether the definition applies to that version or not. Earlier versions are in
  // less-significant bits.
  uint16_t version_flags;
  // Each bit in this field indicates whether the definition applies to that area or not. Ep1 Pioneer 2 is the least-
  // significant bit. Note that Episode 3 only has Morgue (0x1), battle (0x2), and lobby (0x8000) areas, so only those
  // bits can be set here if version_flags is F_EP3.
  uint64_t area_flags;
  // This is the internal name of the class as specified in the client, if it's available (if not, this is a somewhat-
  // descriptive made-up name).
  const char* name;
  // These are the names for the parameters used during map disassembly. The pair variant is used for params whose high
  // and low words have different meanings. The expressions may refer to e.g. param1, param4, etc.
  struct ParamDef {
    const char* name;
    std::shared_ptr<const ResourceDASM::Expression::Node> expr;
    std::shared_ptr<const ResourceDASM::Expression::Node> requires_expr = nullptr;
    bool render_as_hex = false;
  };
  std::vector<ParamDef> params;
};

// TODO: Figure out whether GC NTE should be included in the various groups.
static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the DATEntityDefinition tables");
static constexpr uint16_t F_V0_V1 = 0x001C;
static constexpr uint16_t F_V0_V2 = 0x01FC;
static constexpr uint16_t F_V0_V4 = 0x33FC;
static constexpr uint16_t F_V1_V4 = 0x33F0;
static constexpr uint16_t F_V2 = 0x01E0;
static constexpr uint16_t F_V2_V4 = 0x33E0;
static constexpr uint16_t F_V3_V4 = 0x3200;
static constexpr uint16_t F_V4 = 0x2000;
static constexpr uint16_t F_GC = 0x0F00;
static constexpr uint16_t F_EP3 = 0x0C00;

// clang-format off
static constexpr uint64_t A_E1_P2            = (1ULL << 0x00); // 0000000000000001
static constexpr uint64_t A_E1_FOREST_1      = (1ULL << 0x01); // 0000000000000002
static constexpr uint64_t A_E1_FOREST_2      = (1ULL << 0x02); // 0000000000000004
static constexpr uint64_t A_E1_CAVE_1        = (1ULL << 0x03); // 0000000000000008
static constexpr uint64_t A_E1_CAVE_2        = (1ULL << 0x04); // 0000000000000010
static constexpr uint64_t A_E1_CAVE_3        = (1ULL << 0x05); // 0000000000000020
static constexpr uint64_t A_E1_MINE_1        = (1ULL << 0x06); // 0000000000000040
static constexpr uint64_t A_E1_MINE_2        = (1ULL << 0x07); // 0000000000000080
static constexpr uint64_t A_E1_RUINS_1       = (1ULL << 0x08); // 0000000000000100
static constexpr uint64_t A_E1_RUINS_2       = (1ULL << 0x09); // 0000000000000200
static constexpr uint64_t A_E1_RUINS_3       = (1ULL << 0x0A); // 0000000000000400
static constexpr uint64_t A_E1_DRAGON        = (1ULL << 0x0B); // 0000000000000800
static constexpr uint64_t A_E1_DE_ROL_LE     = (1ULL << 0x0C); // 0000000000001000
static constexpr uint64_t A_E1_VOL_OPT       = (1ULL << 0x0D); // 0000000000002000
static constexpr uint64_t A_E1_DARK_FALZ     = (1ULL << 0x0E); // 0000000000004000
static constexpr uint64_t A_LOBBY            = (1ULL << 0x0F); // 0000000000008000
static constexpr uint64_t A_E1_SPACESHIP     = (1ULL << 0x10); // 0000000000010000
static constexpr uint64_t A_E1_PALACE        = (1ULL << 0x11); // 0000000000020000
static constexpr uint64_t A_E2_LAB           = (1ULL << 0x12); // 0000000000040000
static constexpr uint64_t A_E2_TEMPLE_A      = (1ULL << 0x13); // 0000000000080000
static constexpr uint64_t A_E2_TEMPLE_B      = (1ULL << 0x14); // 0000000000100000
static constexpr uint64_t A_E2_SPACESHIP_A   = (1ULL << 0x15); // 0000000000200000
static constexpr uint64_t A_E2_SPACESHIP_B   = (1ULL << 0x16); // 0000000000400000
static constexpr uint64_t A_E2_CCA           = (1ULL << 0x17); // 0000000000800000
static constexpr uint64_t A_E2_JUNGLE_N      = (1ULL << 0x18); // 0000000001000000
static constexpr uint64_t A_E2_JUNGLE_E      = (1ULL << 0x19); // 0000000002000000
static constexpr uint64_t A_E2_MOUNTAIN      = (1ULL << 0x1A); // 0000000004000000
static constexpr uint64_t A_E2_SEASIDE       = (1ULL << 0x1B); // 0000000008000000
static constexpr uint64_t A_E2_SEABED_U      = (1ULL << 0x1C); // 0000000010000000
static constexpr uint64_t A_E2_SEABED_L      = (1ULL << 0x1D); // 0000000020000000
static constexpr uint64_t A_E2_GAL_GRYPHON   = (1ULL << 0x1E); // 0000000040000000
static constexpr uint64_t A_E2_OLGA_FLOW     = (1ULL << 0x1F); // 0000000080000000
static constexpr uint64_t A_E2_BARBA_RAY     = (1ULL << 0x20); // 0000000100000000
static constexpr uint64_t A_E2_GOL_DRAGON    = (1ULL << 0x21); // 0000000200000000
static constexpr uint64_t A_E2_SEASIDE_NIGHT = (1ULL << 0x22); // 0000000400000000
static constexpr uint64_t A_E2_TOWER         = (1ULL << 0x23); // 0000000800000000
static constexpr uint64_t A_E4_CRATER_E      = (1ULL << 0x24); // 0000001000000000
static constexpr uint64_t A_E4_CRATER_W      = (1ULL << 0x25); // 0000002000000000
static constexpr uint64_t A_E4_CRATER_S      = (1ULL << 0x26); // 0000004000000000
static constexpr uint64_t A_E4_CRATER_N      = (1ULL << 0x27); // 0000008000000000
static constexpr uint64_t A_E4_CRATER_I      = (1ULL << 0x28); // 0000010000000000
static constexpr uint64_t A_E4_DESERT_1      = (1ULL << 0x29); // 0000020000000000
static constexpr uint64_t A_E4_DESERT_2      = (1ULL << 0x2A); // 0000040000000000
static constexpr uint64_t A_E4_DESERT_3      = (1ULL << 0x2B); // 0000080000000000
static constexpr uint64_t A_E4_BOSS          = (1ULL << 0x2C); // 0000100000000000
static constexpr uint64_t A_E4_P2            = (1ULL << 0x2D); // 0000200000000000
static constexpr uint64_t A_E4_TEST          = (1ULL << 0x2E); // 0000400000000000
static constexpr uint64_t A_E3_MORGUE        = (1ULL << 0x2F); // 0000800000000000
static constexpr uint64_t A_E3_BATTLE        = (1ULL << 0x30); // 0001000000000000
// clang-format on

static constexpr uint64_t A_ALL = 0x00007FFFFFFFFFFF;
static constexpr uint64_t A_E3_ALL = A_E3_MORGUE | A_E3_BATTLE | A_LOBBY;
static constexpr uint64_t A_CITY = A_E1_P2 | A_E2_LAB | A_E4_P2;
static constexpr uint64_t A_FINAL_BOSS = A_E1_DARK_FALZ | A_E2_OLGA_FLOW | A_E4_BOSS;
static constexpr uint64_t A_E1_FOREST = A_E1_FOREST_1 | A_E1_FOREST_2;
static constexpr uint64_t A_E1_CAVE = A_E1_CAVE_1 | A_E1_CAVE_2 | A_E1_CAVE_3;
static constexpr uint64_t A_E1_MINE = A_E1_MINE_1 | A_E1_MINE_2;
static constexpr uint64_t A_E1_RUINS = A_E1_RUINS_1 | A_E1_RUINS_2 | A_E1_RUINS_3;
static constexpr uint64_t A_E1_FREE_PLAY = A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_RUINS;
static constexpr uint64_t A_E1_QUEST_ONLY = A_E1_SPACESHIP | A_E1_PALACE;
static constexpr uint64_t A_E1_NON_BOSS = A_E1_FREE_PLAY | A_E1_QUEST_ONLY;
static constexpr uint64_t A_E1_BOSS = A_E1_DRAGON | A_E1_DE_ROL_LE | A_E1_VOL_OPT | A_E1_DARK_FALZ;
static constexpr uint64_t A_E1_NON_CITY = A_E1_NON_BOSS | A_E1_BOSS;
static constexpr uint64_t A_E2_TEMPLE = A_E2_TEMPLE_A | A_E2_TEMPLE_B;
static constexpr uint64_t A_E2_SPACESHIP = A_E2_SPACESHIP_A | A_E2_SPACESHIP_B;
static constexpr uint64_t A_E2_VR = A_E2_TEMPLE | A_E2_SPACESHIP;
static constexpr uint64_t A_E2_CCA_ALL = A_E2_CCA | A_E2_JUNGLE_N | A_E2_JUNGLE_E | A_E2_MOUNTAIN | A_E2_SEASIDE;
static constexpr uint64_t A_E2_SEABED = A_E2_SEABED_U | A_E2_SEABED_L;
static constexpr uint64_t A_E2_FREE_PLAY = A_E2_VR | A_E2_CCA_ALL | A_E2_SEABED;
static constexpr uint64_t A_E2_QUEST_ONLY = A_E2_SEASIDE_NIGHT | A_E2_TOWER;
static constexpr uint64_t A_E2_NON_BOSS = A_E2_FREE_PLAY | A_E2_QUEST_ONLY;
static constexpr uint64_t A_E2_BOSS = A_E2_GAL_GRYPHON | A_E2_OLGA_FLOW | A_E2_BARBA_RAY | A_E2_GOL_DRAGON;
static constexpr uint64_t A_E2_NON_CITY = A_E2_NON_BOSS | A_E2_BOSS;
static constexpr uint64_t A_E4_CRATER = A_E4_CRATER_E | A_E4_CRATER_W | A_E4_CRATER_S | A_E4_CRATER_N | A_E4_CRATER_I;
static constexpr uint64_t A_E4_DESERT = A_E4_DESERT_1 | A_E4_DESERT_2 | A_E4_DESERT_3;
static constexpr uint64_t A_E4_NON_BOSS = A_E4_CRATER | A_E4_DESERT;
static constexpr uint64_t A_E4_NON_CITY = A_E4_NON_BOSS | A_E4_BOSS;
static constexpr uint64_t A_NON_BOSS = A_E1_NON_BOSS | A_E2_NON_BOSS | A_E4_NON_BOSS;

struct DATEntityDefinitionsBuilder {
  struct ParamDefArgs {
    const char* name;
    std::string text;
    std::string requires_text = "";
    bool render_as_hex = false;
  };

  std::map<std::string, std::shared_ptr<ResourceDASM::Expression::Node>> expr_cache;
  std::unordered_multimap<uint16_t, DATEntityDefinition> ret;

  std::shared_ptr<const ResourceDASM::Expression::Node> get_cached_expr(const std::string& text) {
    if (text.empty()) {
      return nullptr;
    } else if (auto it = this->expr_cache.find(text); it != this->expr_cache.end()) {
      return it->second;
    } else {
      std::shared_ptr<ResourceDASM::Expression::Node> expr(ResourceDASM::Expression::Node::parse(text));
      this->expr_cache.emplace(text, expr);
      return expr;
    }
  };

  void add(
      uint16_t base_type,
      uint16_t version_flags,
      uint64_t area_flags,
      uint64_t ep3_area_flags,
      const char* name,
      std::vector<ParamDefArgs> params) {
    std::vector<DATEntityDefinition::ParamDef> param_defs;
    param_defs.reserve(params.size());
    for (const auto& pda : params) {
      param_defs.emplace_back(DATEntityDefinition::ParamDef{
          pda.name, get_cached_expr(pda.text), get_cached_expr(pda.requires_text), pda.render_as_hex});
    }

    auto& def = this->ret.emplace(base_type, DATEntityDefinition{})->second;
    def.base_type = base_type;
    def.version_flags = version_flags;
    def.area_flags = area_flags;
    def.name = name;
    if (ep3_area_flags) {
      auto& ep3_def = this->ret.emplace(base_type, DATEntityDefinition{})->second;
      ep3_def.base_type = base_type;
      ep3_def.version_flags = F_EP3;
      ep3_def.area_flags = ep3_area_flags;
      ep3_def.name = name;
      def.params = param_defs;
      ep3_def.params = std::move(param_defs);
    } else {
      def.params = std::move(param_defs);
    }
  };
};

static std::unordered_multimap<uint16_t, DATEntityDefinition> create_default_dat_object_definitions() {
  using PA = DATEntityDefinitionsBuilder::ParamDefArgs;

  DATEntityDefinitionsBuilder b;
  static constexpr std::string ax = "angle.x";
  static constexpr std::string ay = "angle.y";
  static constexpr std::string az = "angle.z";
  static constexpr std::string p1 = "param1";
  static constexpr std::string p2 = "param2";
  static constexpr std::string p3 = "param3";
  static constexpr std::string p4 = "param4";
  static constexpr std::string p5 = "param5";
  static constexpr std::string p6 = "param6";

  // This is newserv's canonical list of map object types.

  // Objects defined in map files take arguments in the form of an ObjectSetEntry structure (see Map.hh). Most objects
  // take parameters only in param1-3 (floats) and param4-6 (ints), but a few of them use the angle fields as
  // additional int parameters. All objects are available on all versions of the game (except Episode 3) unless
  // otherwise noted, but most objects are available only on specific floors unless an omnispawn patch is used.

  // Defines where a player should start when entering a floor. Params:
  //   param1 = client ID
  //   param4 = source type (clamped below to 0):
  //     0 = use this set when advancing from a lower floor
  //     1 = use this set when returning from a higher floor
  //     any other positive number = set is unused by default, but may be used by TObjAreaWarpQuest
  b.add(0x0000, F_V0_V4, A_ALL, A_E3_MORGUE | A_LOBBY, "TObjPlayerSet", {{"client_id", "int(param1)"}, {"source_type", "max(0, param4)"}});

  // Displays a particle effect. This object is not constructed in split-screen mode. Params:
  //   param1 = particle type
  //   param3 = flags: negative = 2, 2 => 8, 3 => 4, anything else => 0 (TODO: describe what these do)
  //   param4/param6 = sets maximum distance limit (if the object is further away than this, it is destroyed):
  //     param4 = 1 and param6 = 1: limit is 10000
  //     (param4 = 1 or area is 0x0E (Dark Falz arena)) and param6 != 1: limit is 1500
  //     all other cases: limit is 400
  //   param5 = if equal to 1, create the particle under TL_PARTICLEMODEL instead of TL_07 (TODO: this probably affects
  //     its update or render behavior; describe exactly how these are affected)
  std::vector<PA> TObjParticle_params{{"type", p1}, {"flags_type", p3}, {"dist_flag1", p4}, {"layer", p5}, {"dist_flag2", p6}};
  b.add(0x0001, F_V0_V4, A_ALL & (~A_E4_BOSS), A_E3_ALL, "TObjParticle", TObjParticle_params);

  // Standard (triangular) cross-floor warp object. Params:
  //   param4 = destination floor
  //   param6 = color (0 = blue, 1 = red); if this is 0 in Challenge mode, the warp is destroyed immediately
  std::vector<PA> TObjAreaWarpForest_params{{"dest_floor", p4}, {"is_red", "param6 == 1"}};
  b.add(0x0002, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_BOSS | A_E2_VR | A_E2_BOSS | A_E4_NON_CITY | A_CITY | A_E4_TEST, 0, "TObjAreaWarpForest", TObjAreaWarpForest_params);

  // Standard (triangular) intra-floor warp object. Params:
  //   param1-3 = destination coordinates (x, y, z); players are supposed to be offset from this location in different
  //     directions depending on their client ID, but there is a bug that causes all players to use the same offsets:
  //     x + 10 and z + 10
  //   param4 = destination angle (about y axis)
  std::vector<PA> TObjMapWarpForest_params{{"dest_x", p1}, {"dest_y", p2}, {"dest_z", p3}, {"dest_angle", p4}};
  b.add(0x0003, F_V0_V4, A_ALL & (~A_E1_RUINS) & (~A_LOBBY) & (~A_E2_BOSS), 0, "TObjMapWarpForest", TObjMapWarpForest_params);

  // Directional light override for nearby entities. This object is not constructed in split-screen mode. Params:
  //   param1 = red component of light color; if >= 10000, light rotates around z axis (and red is param1 - 10000)
  //   param2 = green component of light color; if >= 10000, light z angle oscillates by +/- 70 degrees using a sine
  //     wave (and green is param2 - 10000); rotation takes precedence if both param1 and param2 are >= 10000
  //   param3 = blue component of light color
  //   param4 high word = influence radius factor (actual radius is high word * 5)
  //   param4 low word = angular step per frame (if param1 or param2 >= 10000)
  //   param5 high word = radial blend strength delta (value is high word / 10 + 1)
  //   param5 low word = initial light angle about y
  //   param6 = initial light angle about z axis
  std::vector<PA> TObjLight_args{
      {"color_r", "(param1 >= 10000) ? (param1 - 10000) : param1"},
      {"color_g", "(param2 >= 10000) ? (param2 - 10000) : param2"},
      {"color_b", p3},
      {"enable_rotate_z", "param1 >= 10000"},
      {"enable_angle_oscillate", "(param1 < 10000) && (param2 >= 10000)"},
      {"influence_radius", "((param4 >> 16) & 0xFFFF) * 5"},
      {"angular_velocity", "param4 & 0xFFFF", "(param1 >= 10000) || (param2 >= 10000)"},
      {"blend_strength", "(((param5 >> 16) & 0xFFFF) / 10.0) + 1"},
      {"initial_angle_y", "param5 & 0xFFFF"},
      {"initial_angle_z", p6}};
  b.add(0x0004, F_V0_V4, A_LOBBY | A_NON_BOSS | A_CITY | A_E4_TEST, A_E3_ALL, "TObjLight", std::move(TObjLight_args));

  // Arbitrary item. The parameters specify the item data; see TObjContainerBase for how the encoding works.
  b.add(0x0005, F_V0_V2, A_E1_P2 | A_E1_FOREST | A_E1_CAVE | A_E1_RUINS, 0, "TItem", {{"item_def1", p4}, {"item_def2", p5}, {"item_def3", p6}});

  // Environmental sound. This object is not constructed in offline multi mode. Params:
  //   param3 = audibility radius (if <= 0 uses default of 200)
  //   param4 = sound ID:
  //     Pioneer 2 / Lab (volume param is ignored on this floor):
  //       00 = distant crowd noises (GC: 4004B200)
  //       01 = ship passing by (GC: 4004C101)
  //       02 = siren passing by (GC: 4004CB02)
  //     Forest:
  //       00 = waterfall (GC: 0004A801)
  //       01 = stream (GC: 0004B202)
  //       02 = birds chirping (GC: 0004BC00)
  //       03 = light rain on ground (GC: 00043704)
  //       04 = light rain on water (GC: 0004BC05)
  //       05 = wind (GC: 0004AD06)
  //     Caves:
  //       00 = nothing? (GC: 00046400)
  //       01 = volcano? (GC: 00042A01)
  //       02 = lava flow (GC: 00042F02)
  //       03 = stream (GC: 00043C03)
  //       04 = waterfall (GC: 00043C04)
  //       05 = echoing drips (GC: 00042D05)
  //     Mines:
  //       00 = mechanical whir (GC: 4004B715)
  //       01 = rotary machine (GC: 4004BC16)
  //     Ruins:
  //       00 = wind/water (GC: 4004B200)
  //       01 = swishing (GC: 4004CB01)
  //       02 = repeating sliding? (GC: 4004BC02)
  //       03 = repeating punching? (GC: 4004BC03)
  //       04 = hissing (GC: 4004C604)
  //       05 = heartbeat (GC: 4004AA05)
  //       06 = squishing heartbeat (GC: 4004C606)
  //     Battle 1 (Spaceship):
  //       00 = low beeping (GC: 4004B40C)
  //       01 = high beeping (GC: 4004B40D)
  //       02 = robot arm movement 1 (GC: 4004C10E)
  //       03 = robot arm movement 2 (GC: 4004C10F)
  //     Battle 2 (Temple):
  //       00 = waterfall (GC: 0004CB0C)
  //       01 = waves (GC: 0004BC0D)
  //       02 = bubbles (GC: 0004BC0E)
  //     VR Temple:
  //       00 = waterfall (GC: 0004CB0C)
  //       01 = waves (GC: 0004BC0D)
  //       02 = bubbles (GC: 0004BC0E)
  //     VR Spaceship:
  //       00 = low beeping (GC: 4004B40C)
  //       01 = high beeping (GC: 4004B40D)
  //       02 = robot arm movement 1 (GC: 4004C10E)
  //       03 = robot arm movement 2 (GC: 4004C10F)
  //     Central Control Area:
  //       00 = wind (GC: 0004CB05)
  //       01 = waterfall (GC: 0004D506)
  //       02 = stream (GC: 0004BC07)
  //       03 = waves crashing (GC: 0004C60A)
  //       04 = ocean wind (GC: 0004CB0B)
  //       05 = higher wind (GC: 0004CB0C)
  //       06 = light ocean wind (GC: 0004CB0D)
  //       07 = ominous mechanical sound (GC: 0004BC0E)
  //     Seabed and Olga Flow:
  //       00 = water leak (GC: 4004BC00)
  //       01 = water drip (GC: 4004BC01)
  //       02 = waterfall (GC: 4004C603)
  //       03 = light waterfall (GC: 4004D504)
  //       04 = creaking metal 1 (GC: 4004B205)
  //       05 = creaking metal 2 (GC: 4004B206)
  //       06 = creaking metal 3 (GC: 4004AD07)
  //       07 = creaking metal 4 (GC: 4004B208)
  //       08 = creaking metal 5 (GC: 4004AD09)
  //       09 = gas leak (GC: 4004B20A)
  //       0A = distant metallic thud (GC: 4004B20B)
  //     Crater:
  //       00 = TODO (BB: 000005FD)
  //       01 = TODO (BB: 000005FE)
  //       02 = TODO (BB: 000005FF)
  //       03 = TODO (BB: 00000600)
  //       04 = TODO (BB: 00000601)
  //       05 = TODO (BB: 00000602)
  //       06 = TODO (BB: 00000603)
  //       07 = TODO (BB: 00000604)
  //       08 = TODO (BB: 00000605)
  //       09 = TODO (BB: 00000606)
  //       0A = TODO (BB: 00000607)
  //     Crater Interior:
  //       00 = TODO (BB: 00000658)
  //       01 = TODO (BB: 00000659)
  //       02 = TODO (BB: 0000065A)
  //       03 = TODO (BB: 0000065B)
  //       04 = TODO (BB: 0000065C)
  //       05 = TODO (BB: 0000065D)
  //       06 = TODO (BB: 0000065E)
  //       07 = TODO (BB: 0000065F)
  //       08 = TODO (BB: 00000660)
  //       09 = TODO (BB: 00000661)
  //       0A = TODO (BB: 00000662)
  //     Desert:
  //       00 = TODO (BB: 000006AE)
  //       01 = TODO (BB: 000006AF)
  //       02 = TODO (BB: 000006B0)
  //       03 = TODO (BB: 000006B1)
  //       04 = TODO (BB: 000006B2)
  //       05 = TODO (BB: 000006B3)
  //       06 = TODO (BB: 000006B4)
  //       07 = TODO (BB: 000006B5)
  //       08 = TODO (BB: 000006B6)
  //       09 = TODO (BB: 000006B7)
  //       0A = TODO (BB: 000006B8)
  //       0B = TODO (BB: 000006B9)
  //       0C = TODO (BB: 000006BA)
  //       0D = TODO (BB: 000006BB)
  //       0E = TODO (BB: 000006BC)
  //       0F = TODO (BB: 000006BD)
  //       10 = TODO (BB: 000006BE)
  //   param5 = volume (clamped to [-0x7F, 0x7F])
  std::vector<PA> TObjEnvSound_base_params{{"sound_id", p4}, {"volume", "clamp(param5, -0x7F, 0x7F)"}};
  std::vector<PA> TObjEnvSound_params = TObjEnvSound_base_params;
  TObjEnvSound_params.emplace_back(PA{"radius", "(param3 <= 0) ? 200 : param3"});
  b.add(0x0006, F_V0_V2, A_E1_P2 | A_E1_NON_CITY, 0, "TObjEnvSound", TObjEnvSound_params);
  b.add(0x0006, F_V3_V4, A_E1_VOL_OPT | A_E1_NON_BOSS | A_E2_FREE_PLAY | A_E2_OLGA_FLOW | A_E4_NON_BOSS | A_CITY | A_E4_TEST, A_E3_MORGUE, "TObjEnvSound", TObjEnvSound_params);

  // Fog collision object. Params:
  //   param1 = radius
  //   param4 = fog entry number; if lower than the existing fog number, does nothing (if param4 is >= 0x1000, the game
  //     subtracts 0x1000 from it, but only after comparing it to the current fog number; this can be used to override
  //     a later fog with an earlier fog)
  //   param5 = transition type (0 = fade in, 1 = instantly switch)
  std::vector<PA> TObjFogCollision_params{{"radius", p1}, {"fog_entry_index", "(param4 >= 0x1000) ? (param4 - 0x1000) : param4"}, {"override_flag", "param4 >= 0x1000"}, {"switch_instantly", p5}};
  b.add(0x0007, F_V0_V4, A_ALL & (~A_LOBBY) & (~A_E4_BOSS), A_E3_MORGUE, "TObjFogCollision", TObjFogCollision_params);

  // Event collision object. This object triggers a wave event (W-XXX) when any local player (in split-screen play,
  // there may be multiple) steps within its radius. The object is inactive for 3 frames after it is constructed, and
  // will not detect any player during that time. Curiously, the frame counter does not appear to be bounded, so if the
  // player waits approximately 828 days, the counter will overflow to a positive number and the object won't be able
  // to trigger for another 828 days until it gets to zero again. It is unlikely this has ever happened for any player.
  // Params:
  //   param1 = radius
  //   param4 = event ID
  b.add(0x0008, F_V0_V4, A_ALL & (~A_LOBBY), A_E3_MORGUE, "TObjEvtCollision", {{"radius", p1}, {"event_id", p4, "", true}});

  // Invisible collision. Params:
  //   param1 = radius
  //   param2 = half-width
  //   param3 = shape/flags (TODO: negative/nonnegative chooses which shape; abs(value) >= 1000 sets flag 0x100.
  //     Describe what these do)
  b.add(0x0009, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_RUINS | A_CITY | A_E4_TEST, A_E3_MORGUE, "TObjCollision", {{"radius", p1}, {"width", "param2 * 2"}, {"flags", p3}});

  // Elemental trap. Params:
  //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
  //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
  //   param3 = trap group number:
  //     negative = trap triggers and explodes alone
  //     0 = trap follows player who triggered it (online only; when offline, these act as if the group number were
  //       negative, and param6 is overwritten with 30 (1 second))
  //     positive = trap is part of a group that all trigger simultaneously (but they only explode simultaneously if
  //       their param6 values are the same)
  //   param4 = trap power (clamped below to 0), scaled by difficulty:
  //     Normal: power = param4 * 1
  //     Hard: power = param4 * 3
  //     Very Hard: power = param4 * 5
  //     Ultimate: power = param4 * 14
  //   param5 = damage type (clamped to [0, 5])
  //     00 = direct damage (damage = power / 5)
  //     01 = fire (damage = power * (100 - EFR) / 500)
  //     02 = cold (damage = power * (100 - EIC) / 500)
  //       chance to freeze enemy = ((((power - 250) / 40) + 5) / 40) clamped to [0, 0.4], or [0.2, 0.4] on Ultimate)
  //       chance to freeze player = max(0, 0.75 - (EIC / 100))
  //     03 = electric (damage = power * (100 - ETH) / 500)
  //       chance to shock enemy = 1/15, or 1/40 on Ultimate
  //       chance to shock player = max(0, 0.75 - (ETH / 100))
  //     04 = light (damage = power * (100 - ELT) / 500)
  //     05 = dark (instantly kills with chance (power - EDK) / 100; if used in a boss arena and in non-Ultimate mode,
  //       cannot kill; this damage type does not cause knockback)
  //   param6 = number of frames between trigger and explosion
  b.add(0x000A, F_V0_V4, A_NON_BOSS | A_E4_BOSS | A_E4_TEST, 0, "TOMineIcon01", {{"trigger_radius", "(param1 / 2) + 30"}, {"explosion_radius", "(param2 / 2) + 60"}, {"group_number", p3}, {"power", "max(0, param4)"}, {"damage_type", "clamp(param5, 0, 5)"}, {"explosion_delay_frames", p6}});

  // Status trap. Params:
  //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
  //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
  //   param3 = trap group number (same as in TOMineIcon01)
  //   param4 = trap power (same as in TOMineIcon01)
  //   param5 = trap type (clamped to [0x0F, 0x12])
  //     0F = poison
  //     10 = paralysis
  //     11 = slow
  //     12 = confuse
  //   param6 = number of frames between trigger and explosion
  b.add(0x000B, F_V0_V4, A_NON_BOSS | A_E4_BOSS | A_E4_TEST, 0, "TOMineIcon02", {{"trigger_radius", "(param1 / 2) + 30"}, {"explosion_radius", "(param2 / 2) + 60"}, {"group_number", p3}, {"power", "max(0, param4)"}, {"damage_type", "clamp(param5, 0x0F, 0x12)"}, {"explosion_delay_frames", p6}});

  // Heal trap. Params:
  //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
  //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
  //   param3 = trap group number (same as in TOMineIcon01)
  //   param4 = trap power (same as in TOMineIcon01)
  //   param5 = trap type (clamped to [6, 8])
  //     06 = heal HP (amount = power)
  //     07 = clear negative status effects
  //     08 = does nothing? (TODO: calls player->vtable[0x40], but that function does nothing on v3. Did this do
  //       something in v1 or v2?)
  //   param6 = number of frames between trigger and explosion
  b.add(0x000C, F_V0_V4, A_NON_BOSS | A_E4_BOSS | A_E4_TEST, 0, "TOMineIcon03", {{"trigger_radius", "(param1 / 2) + 30"}, {"explosion_radius", "(param2 / 2) + 60"}, {"group_number", p3}, {"power", "max(0, param4)"}, {"damage_type", "clamp(param5, 6, 8)"}, {"explosion_delay_frames", p6}});

  // Large elemental trap. Params:
  //   param1 = trigger radius delta (actual radius is param1 / 2 + 60)
  //   param2 = explosion radius delta (actual radius is param2 / 2 + 120)
  //   param3 = trap group number (same as in TOMineIcon01)
  //   param4 = trap power (same as in TOMineIcon01)
  //   param5 = trap type (same as in TOMineIcon01)
  //   param6 = number of frames between trigger and explosion
  b.add(0x000D, F_V0_V4, A_NON_BOSS | A_E4_BOSS | A_E4_TEST, 0, "TOMineIcon04", {{"trigger_radius", "(param1 / 2) + 60"}, {"explosion_radius", "(param2 / 2) + 120"}, {"group_number", p3}, {"power", "max(0, param4)"}, {"damage_type", "clamp(param5, 0, 5)"}, {"explosion_delay_frames", p6}});

  // Room ID. This object sets each player's room ID to one of two different values when they're within the effective
  // radius. Params:
  //   param1 = radius delta (actual radius = param1 * 10)
  //   param2 = room ID to use if player is facing the same direction as this object (as determined by the dot product
  //     of the vector from this object to the player, and the vector (0, 0, -1) rotated by the angle in param5; if the
  //     dot product is positive, param2 is used)
  //   param3 = room ID to use if the dot product described above is zero or negative
  //   param5 = angle (see param2)
  //   param6 = on v2 and before, this is ignored; on v3 and later, if param6 is equal to 0x00010000, only the player's
  //     room_id field is set, and player flag 0x02000000 is set on the player (which disables TObjRaderCol effects);
  //     if not equal to 0x00010000, then both room_id and room_id_in_custom_area (if in a non-rearrangeable area, like
  //     Pioneer 2 or Forest) are set and no game flag is set (TODO: what are the player-visible effects of these
  //     differences?)
  b.add(0x000E, F_V0_V4, A_ALL & (~A_E1_DARK_FALZ) & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_CITY), 0, "TObjRoomId", {{"radius", "param1 * 10"}, {"forward_room_id", p2}, {"reverse_room_id", p3}, {"angle", p5}, {"use_secondary_id", "param6 != 0x00010000", "is_v3 || is_v4"}});

  // Sensor of some kind (TODO). Params:
  //   param1 = activation radius delta (actual radius = param1 + 50, clamped below to 0)
  //   param4 = switch flag number
  //   param5 = activation condition:
  //     negative = require proximity and local client ID condition (TODO: is it the current chat-bubble speaker?)
  //     zero or positive = require proximity and game flag 0x00400000 on local player (TODO: position valid?)
  b.add(0x000F, F_V0_V4, A_E1_FOREST | A_E1_CAVE_2 | A_E1_CAVE_3 | A_E1_MINE | A_E4_TEST, 0, "TOSensorGeneral01", {{"radius", "max(0, param1 + 50)"}, {"switch_flag", p4}, {"activation_type", "param5 >= 0"}});

  // Lens flare effect. This object is not constructed in offline multi mode. Params:
  //   param1 = visibility radius factor (actual radius is param1 * 10; if negative, visible everywhere)
  b.add(0x0011, F_V0_V4, A_E1_FOREST | A_E1_CAVE_1 | A_E1_CAVE_2 | A_E1_RUINS_1 | A_E1_DARK_FALZ | A_E4_TEST, 0, "TEF_LensFlare", {{"visibility_radius", "param1 * 10"}});

  // Quest script trigger. Starts a quest thread at a floor-specific label when the local player enters; if that thread
  // terminates and the player is still within the radius, it will start another one. Params:
  //   param1 = radius
  //   param4 = label number to call when local player is within radius
  std::vector<PA> TObjQuestCol_params{{"radius", p1}, {"label_number", p4}};
  b.add(0x0012, F_V0_V4, A_ALL & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS), A_E3_MORGUE, "TObjQuestCol", TObjQuestCol_params);

  // Healing ring. Removes all negative status conditions, refreshes traps for androids (if not in Battle mode) and
  // adds 9 HP and 9 TP per frame until max HP/TP are both reached or the player leaves the radius. The radius is a
  // fixed size.
  b.add(0x0013, F_V0_V4, A_E1_FREE_PLAY | A_E2_NON_BOSS | A_E4_NON_BOSS | A_E4_TEST, 0, "TOHealGeneral", {});

  // Invisible map collision. Params:
  //   param1-3 = box dimensions (x, y, z; rotated by angle.y)
  //   param4 = wall type:
  //     00 = custom (see param5)
  //     01 = blocks enemies only (as if param5 = 0x00008000)
  //     02 = blocks enemies and players (as if param5 = 0x00008900)
  //     03 = blocks enemies and players, but enemies can see targets through it (as if param5 = 0x00000800)
  //     04 = blocks players only (as if param5 = 0x00002000)
  //     05 = undefined behavior due to missing bounds check
  //     anything else = same as 01
  //   param5 = flags (bit field; used if param4 = 0) (TODO: describe bits)
  b.add(0x0014, F_V0_V4, A_ALL & (~A_E1_MINE) & (~A_E1_BOSS) & (~A_LOBBY) & (~A_E2_VR) & (~A_E2_BOSS) & (~A_E4_NON_CITY), A_E3_MORGUE, "TObjMapCsn", {{"size_x", p1}, {"size_y", p2}, {"size_z", p3}, {"wall_type", p4}, {"flags", p5, "param4 == 0"}});

  // Like TObjQuestCol, but requires the player to press a controller button to trigger the call. Parameters are the
  // same as for TObjQuestCol.
  b.add(0x0015, F_V0_V4, A_ALL & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS), A_E3_MORGUE, "TObjQuestColA", TObjQuestCol_params);

  // Temporary light effect. Contrary to its name, it doesn't seem to have anything to do with items. Params:
  //   param1 = radius (if negative, 30 is used)
  b.add(0x0016, F_V0_V4, A_ALL & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS), A_E3_MORGUE | A_LOBBY, "TObjItemLight", {{"radius", "(param1 < 0) ? 30 : param1"}});

  // Radar collision. This object marks a minimap segment as discovered when the player approaches it. Params:
  //   param1 = radius
  //   param4 = minimap segment ID
  b.add(0x0017, F_V0_V4, A_ALL & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS) & (~A_CITY), A_LOBBY, "TObjRaderCol", {{"radius", p1}, {"minimap_segment_id", p4}});

  // Fog collision. Same params as 0x0007 (TObjFogCollision), but also:
  //   param3 = if >= 1, fog is on when switch flag is on; otherwise, fog is on when switch flag is off
  //   param6 = switch flag number
  std::vector<PA> TObjFogCollisionSwitch_params = TObjFogCollision_params;
  TObjFogCollisionSwitch_params.emplace_back(PA{"invert_enabled_behavior", "param3 < 1"});
  TObjFogCollisionSwitch_params.emplace_back(PA{"switch_flag", p6});
  b.add(0x0018, F_V0_V4, A_ALL & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS) & (~A_CITY), 0, "TObjFogCollisionSwitch", TObjFogCollisionSwitch_params);

  // Boss teleporter. The destination cannot be changed; it always teleports the player to the boss arena for the
  // current area. In Challenge mode, the game uses the current area number to determine the destination floor, but in
  // other modes, it uses the current episode number and floor number (not area number). This means that the lists
  // below may be incorrect if floor areas have been reassigned in non-Challenge mode. The mapping is:
  //   Episode 1:
  //     Pioneer 2, boss arenas, lobby, battle areas => Pioneer 2
  //     Forest 1 and 2 => Dragon
  //     Cave 1, 2, and 3 => De Rol Le
  //     Mine 1 and 2 => Vol Opt
  //     Ruins 1, 2, and 3 => Dark Falz
  //   Episode 2:
  //     Lab, boss arenas, Seaside Night, Tower => Lab
  //     VR Temple A and B => Barba Ray
  //     VR Spaceship A and B => Gol Dragon
  //     Central Control Area (but not Seaside Night) => Gal Gryphon
  //     Seabed => Olga Flow
  //   Episode 4:
  //     Pioneer 2, boss arena => Pioneer 2
  //     Crater, Desert, test map => Saint-Milion arena
  // Params:
  //   param5 = switch flag number required to activate (negative or >= 0x100 = no switch flag required; ignored if on
  //     Pioneer 2)
  // In offline mode, this object constructs TObjWarpBossMulti instead.
  b.add(0x0019, F_V0_V4, A_E1_FOREST_2 | A_E1_CAVE_3 | A_E1_MINE_2 | A_E1_RUINS_3 | A_E2_NON_BOSS | A_E4_NON_BOSS | A_CITY | A_E4_TEST, 0, "TObjWarpBoss", {{"switch_flag", p5}});

  // Sign board. This shows the loaded image from a quest (via load_pvr). On the final version of PCv2, this object
  // doesn't render at all; my guess is that Sega hardcoded the PVR filenames for the various events in the executable,
  // then after those events ended, they just deleted the load_pvr code entirely, leaving this object nonfunctional.
  // Params:
  //   param1-3 = scale factors (x, y, z)
  b.add(0x001A, F_V1_V4, A_CITY, A_E3_MORGUE, "TObjSinBoard", {{"scale_x", p1}, {"scale_y", p2}, {"scale_z", p3}});

  // Quest floor warp. This appears similar to TObjAreaWarpForest except that the object is not destroyed immediately
  // if it's blue and the game is Challenge mode. Params:
  //   param1 = player set ID (if this is >= 2, specifies which set of TObjPlayerSets to use on the destination floor,
  //     which overrides the default behavior of 0/1 depending on the source and destination floor numbers; see
  //     TObjPlayerSet for details)
  //   param4 = destination floor
  //   param6 = color (0 = blue, 1 = red)
  b.add(0x001B, F_V1_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_BOSS | A_E4_BOSS | A_E4_TEST, 0, "TObjAreaWarpQuest", {{"player_set_id", p1}, {"dest_floor", p4}, {"is_red", "param6 == 1"}});

  // Ending movie warp (used in final boss arenas after the boss is defeated). Params:
  //   param6 = color (0 = blue, 1 = red)
  b.add(0x001C, F_V1_V4, A_FINAL_BOSS | A_E4_TEST, 0, "TObjAreaWarpEnding", {{"is_red", "param6 == 1"}});

  // Star light effect. This object renders from -100 to 740 over x and -100 to 580 over y. Params:
  //   param1 = half-width of the trails, in pixels
  //   param2 = duration in frames (clamped below at 1)
  //   param3 = color alpha (0-255)
  //   param4 = color red (0-255)
  //   param5 = color green (0-255)
  //   param6 = color blue (0-255)
  b.add(0x001D, F_V2_V4, A_E1_FOREST_1 | A_E4_TEST, 0, "TEffStarLight2D_Base", {{"trail_width", "param1 * 2"}, {"duration_frames", "max(1, param2)"}, {"color_a", p3}, {"color_r", p4}, {"color_g", p5}, {"color_b", p6}});

  // VR Temple Beta / Barba Ray lens flare effect. This object renders from -10 to 650 over x and -10 to 490 over y.
  // Params:
  //   param1 = color (<0 = gray (808080), [0, 10) = orange (FF8040), >=10 = brown (552300))
  b.add(0x001E, F_V2_V4, A_E1_FOREST | A_E1_PALACE | A_E2_TEMPLE | A_E2_BARBA_RAY | A_E4_CRATER | A_E4_TEST, 0, "__LENS_FLARE__", {{"color_type", p1}});

  // Hides the area map when the player is near this object. Params:
  //   param1 = radius
  b.add(0x001F, F_V2_V4, A_NON_BOSS | A_E4_TEST, 0, "TObjRaderHideCol", {{"radius", p1}});

  // Item-detecting floor switch. Params:
  //   param1 = radius
  //   param4 = switch flag number
  //   param5 = item type:
  //     0 or any value not listed below = any item
  //     1 = weapon
  //     2 = armor
  //     3 = shield
  //     4 = unit
  //     5 = mag
  //     6 = tool
  //     7 = meseta
  //   param6 = item amount required minus 1 (0 = 1 item, 1 = 2 items, etc.); for tools and meseta, each dropped item
  //     counts for its stack size
  b.add(0x0020, F_V2_V4, A_NON_BOSS | A_CITY | A_E4_TEST, 0, "TOSwitchItem", {{"radius", p1}, {"switch_flag", p4}, {"item_type", p5}, {"required_count", "param6 + 1"}});

  // Symbol chat collision object. This object triggers symbol chats when the players are nearby and certain switch
  // flags are set. If a player is within the radius, the object checks the switch flags in reverse order, and
  // triggers the symbol chat for the latest one (in that order) which is set. So, the logic is:
  // - If the switch flag in spec3 is set, no symbol chat appears at all regardless of the values of the other two
  //   switch flags
  // - If the switch flag in spec3 is not set and the flag in spec2 is set, the symbol chat in spec3 is used regardless
  //   of whether the switch flag in spec1 is set
  // - If the switch flags in spec2 and spec3 are not set and the switch flag in spec1 is set, the symbol chat in spec2
  //   is used
  // - If all 3 switch flags are not set, the symbol chat in spec1 is used
  // Each spec is a 32-bit integer consisting of two 16-bit fields. The high 16 bits are a switch flag number (0-255),
  // and the low 16 bits are an entry index from symbolchatcolli.prs. The entry index is ignored if the corresponding
  // data label from the F8A6 quest opcode is not null (and the data from the label is used instead). In that case, see
  // SymbolChatT in PlayerSubordinates.hh for the format; on GC, the structure is not byteswapped, so GameCube quests
  // should use the big-endian version of SymbolChatT. Quest scripts don't have a good way to pass null for the data
  // labels, so the logic that looks up entries in symbolchatcolli.prs can be ignored in quests.
  // The default symbol chat numbers are (from qedit.info):
  //   00 = Drop Meseta
  //   01 = Meseta has been dropped
  //   02 = Drop 1 weapon
  //   03 = Drop 4 weapons
  //   04 = Drop 1 shield
  //   05 = Drop 4 shields
  //   06 = Drop 1 mag
  //   07 = Drop 4 mags
  //   08 = Drop tool item
  //   09 = ????
  //   0A = XXXX
  //   0B = All circles like OK
  //   0C = Key with Yes
  //   0D = Key with Cool
  //   0E = Key with ...
  //   0F = Go right
  //   10 = Go left
  //   11 = Push button with gun
  //   12 = Key icons
  //   13 = Key has been pressed
  //   14 = Run/go
  //   15 = Push 1 button on
  //   16 = Push 2 button on
  //   17 = Clock (hurry)
  // Params:
  //   param1 = radius
  //   param4 = spec1
  //   param5 = spec2
  //   param6 = spec3
  // This object can also be created by the quest opcode F8A6; see the description of that opcode in QuestScript.cc.
  // Note that although this object is available in all episodes, the presets file (symbolchatcolli.prs) is only loaded
  // in the following areas:
  // - All Episode 1 areas except boss arenas and the lobby
  // - Lab in Episode 2
  // - The Saint-Milion arena and the test area in Episode 4
  // Using this object in other areas in Episode 2 or Episode 4 will crash.
  b.add(0x0021, F_V2_V4, A_NON_BOSS | A_CITY | A_E4_TEST, 0, "TOSymbolchatColli", {{"radius", p1}, {"switch_flag1", "(param4 >> 16) & 0xFFFF"}, {"entry_index1", "param4 & 0xFFFF"}, {"switch_flag2", "(param5 >> 16) & 0xFFFF"}, {"entry_index2", "param5 & 0xFFFF"}, {"switch_flag3", "(param6 >> 16) & 0xFFFF"}, {"entry_index3", "param6 & 0xFFFF"}});

  // Invisible collision switch. Params:
  //   param1 = radius delta (actual radius is param1 + 10)
  //   param4 = switch flag number
  //   param5 = sticky flag (if negative, switch flag is unset when player leaves; if zero or positive, switch flag
  //     remains on)
  b.add(0x0022, F_V2_V4, A_NON_BOSS | A_E4_TEST, 0, "TOKeyCol", {{"radius", "param1 + 10"}, {"switch_flag", p4}, {"is_sticky", "param5 >= 0"}});

  // Attackable collision. Params:
  //   param1 = enable switch flag (the object is only attackable if this switch flag is enabled); any value not in
  //     [0x00, 0xFF] disables this behavior so the object is always attackable
  //   param2 = if negative, no target reticle appears; if zero or positive, a reticle appears
  //   param3 = switch flag number to set when required number of hits is taken (ignored if param6 is positive)
  //   param4 = number of hits required to activate, minus 1 (so e.g. a value of 4 here means 5 hits needed; clamped to
  //     [1, 256] after adding 1)
  //   param5 = object number (if outside the range [100, 999] or there is no quest loaded, uses the free play script
  //     when looking up param6 instead of the quest)
  //   param6 = quest label to call when all required hits are taken (if zero, switch flag in param3 is set instead)
  b.add(0x0023, F_V2_V4, A_NON_BOSS | A_E4_TEST, 0, "TOAttackableCol", {{"enable_switch_flag", p1}, {"show_target_reticle", "param2 >= 0"}, {"destroyed_switch_flag", p3, "param6 <= 0"}, {"required_hits", "clamp(param4 + 1, 1, 0x100)"}, {"object_number", p5}, {"use_free_play_script", "((param5 < 100) || (param5 > 999))"}, {"quest_label", p6}});

  // Damage effect. Params:
  //   angle.x = effect type (in range [0x00, 0x14]; the following were tested in Forest 1 and may have different
  //     effects in different areas):
  //     00 = fiery explosion with vertical camera shake
  //     01 = vertical camera shake only
  //     02 = bluish energy ball
  //     03 = expanding white flash
  //     04 = contracting white flash
  //     05 = nothing
  //     06 = rising fireball
  //     07 = blue flash
  //     08 = purple flash
  //     09 = teal flash
  //     0A = nothing
  //     0B = big orange flash
  //     0C = nothing
  //     0D = big white flash
  //     0E = black smoky flash
  //     0F = nothing
  //     10 = nothing
  //     11 = smaller black smoke
  //     12 = bluish-purple flash
  //     13 = quick blue flash
  //     14 = lavender-gray flash
  //   param1 = damage radius
  //   param2 = damage value, scaled by difficulty:
  //     Normal: param2 * 2
  //     Hard: param2 * 4
  //     Very Hard: param2 * 6
  //     Ultimate: param2 * 8
  //   param3 = effect visual size
  //   param4 = enable switch flag number (effect is off unless this flag is set)
  //   param5 = disable switch flag number (effect is off if this flag is set, even if the enable switch flag is also
  //     set), or >= 0x100 if this functionality isn't needed
  //   param6 = persistence flag (if nonzero, effect can cause damage again later)
  b.add(0x0024, F_V2_V4, A_ALL & (~A_E1_BOSS) & (~A_LOBBY) & (~A_E2_SPACESHIP) & (~A_E4_NON_CITY), 0, "TOSwitchAttack", {{"effect_type", "clamp(angle.x, 0, 0x14)"}, {"damage_radius", p1}, {"damage_value", p2}, {"visual_size", p3}, {"enable_switch_flag", p4}, {"disable_switch_flag", p5}, {"is_persistent", "param6 != 0"}});

  // Switch flag timer. This object watches for a switch flag to be activated, then once it is, waits for a specified
  // time, then disables that switch flag or activates up to three other switch flags. Note that this object just
  // provides the timer functionality; the trigger switch flag must be activated by some other object. Params:
  //   angle.x = trigger mode:
  //     0 = disable watched switch flag when timer expires
  //     nonzero = enable up to 3 other flags when timer expires and don't disable the watched flag
  //   angle.y = if this is 1, play tick sound effect every second after activation (if any other value, no tick sound
  //     is played)
  //   angle.z = timer duration in frames
  //   param4 = switch flag to watch for activation in low 16 bits, switch flag 1 to activate when timer expires (if
  //     angle.x != 0) in high 16 bits (>= 0x100 if not needed)
  //   param5 = switch flag 2 to activate when timer expires (if angle.x != 0) in high 16 bits (>= 0x100 if none)
  //   param6 = switch flag 3 to activate when timer expires (if angle.x != 0) in high 16 bits (>= 0x100 if none)
  b.add(0x0025, F_V2_V4, A_NON_BOSS | A_CITY | A_E4_TEST, 0, "TOSwitchTimer", {{"trigger_mode", "angle.x != 0"}, {"play_tick_sound", "angle.y == 1"}, {"timer_duration", "max(0, angle.z)"}, {"activate_switch_flag", "param4 & 0xFFFF"}, {"expiration_switch_flag1", "(param4 >> 16) & 0xFFFF", "angle.x != 0"}, {"expiration_switch_flag2", "(param5 >> 16) & 0xFFFF", "angle.x != 0"}, {"expiration_switch_flag3", "(param6 >> 16) & 0xFFFF", "angle.x != 0"}});

  // Chat sensor. This object watches for chat messages said by players within its radius, optionally filtering for
  // specific words. When a message matches, it either activates a switch flag or calls a quest function. When created
  // via the F801 quest opcode, param5 is ignored, and the string specified by the quest is always used. Params:
  //   angle.x = activation type (0 = quest function, 1 = switch flag)
  //   angle.y = match mode:
  //     0 = match a specific string (defined by param5)
  //     1 = match any string
  //     anything else = never match anything, and also set activation type (angle.x) to quest mode due to an apparent
  //       copy-paste error
  //   param1 = radius
  //   param4 = switch flag number or quest label number
  //   param5 = trigger string
  //     0 = no predefined trigger (should be set with the set_chat_callback quest opcode)
  //     1 = "YES"
  //     2 = "COOL"
  //     3 = "NO"
  b.add(0x0026, F_V2_V4, A_NON_BOSS | A_CITY | A_E4_TEST, 0, "TOChatSensor", {{"activate_on_switch_flag", "(angle.x == 1) && ((angle.y == 0) || (angle.y == 1))"}, {"match_mode", ay}, {"radius", p1}, {"label_number", p4, "!((angle.x == 1) && ((angle.y == 0) || (angle.y == 1)))"}, {"switch_flag", p4, "(angle.x == 1) && ((angle.y == 0) || (angle.y == 1))"}, {"trigger_string", p5, "angle.y == 0"}});

  // Radar map icon. Shows an icon on the map that is optionally locked or unlocked, depending on the values of one or
  // more sequential switch flags. The icon is considered unlocked if all of the specified switch flags are set. There
  // can be at most 12 switch flags used with this object and the switch flag numbers cannot wrap around from 0xFF to
  // zero. Params:
  //   param1 = scale (1.0 = normal size)
  //   param4 = color mode:
  //     positive = always render in the color specified by param6
  //     zero or negative = color is red (FFFF0000) if any of the switch flags specified by param5 are off, or green
  //       (FF00FF00) if all are on
  //   param5 = switch flag spec; high 16 bits are number of switch flags, low 16 bits are first switch flag number
  //   param6 = color as ARGB8888
  b.add(0x0027, F_V3_V4, A_E2_LAB | A_E2_NON_CITY | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjRaderIcon", {{"scale", p1}, {"use_custom_color", "param4 > 0"}, {"switch_flag_count", "(param5 >> 16) & 0xFFFF"}, {"first_switch_flag", "param5 & 0xFFFF"}, {"custom_color_argb", p6, "", true}});

  // Environmental sound. This object is not constructed in offline multi mode. This is essentially identical to
  // TObjEnvSound, except the sound fades in and out instead of abruptly starting or stopping when entering or leaving
  // its radius. Params:
  //   param2 = volume fade speed (units per frame)
  //   param3 = audibility radius (same as for TObjEnvSound)
  //   param4 = sound ID (same as for TObjEnvSound)
  //   param5 = volume (same as for TObjEnvSound)
  std::vector<PA> TObjEnvSoundEx_params = TObjEnvSound_params;
  TObjEnvSoundEx_params.emplace_back(PA{"volume_fade_speed", p2});
  b.add(0x0028, F_V3_V4, A_ALL & (~A_E1_CAVE_1) & (~A_E1_DRAGON) & (~A_E1_DE_ROL_LE) & (~A_E1_DARK_FALZ) & (~A_LOBBY) & (~A_E2_GAL_GRYPHON) & (~A_E2_BARBA_RAY) & (~A_E2_GOL_DRAGON) & (~A_E4_BOSS), A_E3_MORGUE, "TObjEnvSoundEx", TObjEnvSoundEx_params);

  // Environmental sound. This object is not constructed in offline multi mode. This is essentially identical to
  // TObjEnvSound, except there is no radius: the sound is audible everywhere. Params:
  //   param4 = sound ID (same as for TObjEnvSound)
  //   param5 = volume (same as for TObjEnvSound)
  b.add(0x0029, F_V3_V4, A_ALL & (~A_E1_CAVE_1) & (~A_E1_DRAGON) & (~A_E1_DE_ROL_LE) & (~A_E1_DARK_FALZ) & (~A_LOBBY) & (~A_E2_GAL_GRYPHON) & (~A_E2_BARBA_RAY) & (~A_E2_GOL_DRAGON) & (~A_E4_BOSS), A_E3_MORGUE, "TObjEnvSoundGlobal", TObjEnvSound_base_params);

  // Counter sequence activator. Used for Hunter's Guild, shops, bank, etc. Params:
  //   param4 = shop sequence number:
  //     Episodes 1, 2, and 4:
  //       00 = weapon shop
  //       01 = medical center
  //       02 = armor shop
  //       03 = tool shop
  //       04 = Hunter's Guild
  //       05 = check room
  //       06 = tekker
  //       07 = government counter (BB only)
  //       08 = Momoka item exchange counter (BB only)
  //     Episode 3:
  //       00 = unused (DUMMY0)
  //       01 = unused (DUMMY1)
  //       02 = unused (DUMMY2)
  //       03 = unused (DUMMY3)
  //       04 = unused (DUMMY4)
  //       05 = unused (DUMMY5)
  //       06 = unused (DUMMY6)
  //       07 = deck edit counter
  //       08 = entry counter
  //       09 = left-side card trade kiosk
  //       0A = right-side card trade kiosk
  //       0B = auction counter
  //       0C = hidden entry counter
  //       0D = Pinz's Shop
  b.add(0x0040, F_V0_V4, A_CITY, A_E3_MORGUE, "TShopGenerator", {{"shop_type", p4}});

  // Telepipe city location. Params:
  //   param4 = owner client ID (0-4; 4 is likely just an oversight and not a useful value)
  b.add(0x0041, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjLuker", {{"client_id", p4}});

  // BGM collision. Changes the background music when the player enters the object's radius. Does nothing in offline
  // multi mode. Params:
  //   param1 = radius
  //   param4 = which music to play:
  //     00 = SHOP.adx
  //     01 = GUILD.adx
  //     02 = MEDICAL.adx
  //     03 = soutoku.adx
  //     04 = city.adx or current holiday event variant
  //     05 = labo.adx
  //     anything else = value is taken modulo 6 and used as above
  b.add(0x0042, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjBgmCol", {{"radius", p1}, {"bgm_index", "param4 % 6"}});

  // Main warp to other floors from Pioneer 2. Certain floors are available by default, determined by checking the
  // game's mode and quest flags. A different set of flags is checked on BB than on other versions, presumably since
  // government quests are used to unlock areas instead of offline story progression. On later versions of BB, all
  // floors are available by default; this table reflects the behavior before that change.
  // Required flag for mode:    Online/multi   Offline       BB (all modes)
  //   Episode 1:
  //     Forest 1               Always open    Always open   Always open
  //     Cave 1                 0x17           0x18          0x1F9
  //     Mine 1                 0x20           0x21          0x201
  //     Ruins 1                0x30           0x2A          0x207
  //   Episode 2:
  //     VR Temple Alpha        Always open    Always open   Always open
  //     VR Spaceship Alpha     0x4C           0x4D          0x21B
  //     CCA                    0x4F           0x50          0x225
  //     Seabed Upper           0x52           0x53          0x22F
  //   Episode 4:
  //     Crater East                                         Always open
  //     Crater West                                         0x2BD
  //     Crater South                                        0x2BE
  //     Crater North                                        0x2BF
  //     Crater Interior                                     0x2C0
  //     Subterranean Desert 1                               0x2C1
  // Params:
  //   param5 = main warp type:
  //     00 = All levels
  //     01 = Ep2 VR Temple / VR Spaceship (CCA and Seabed not available)
  //     02 = Ep2 CCA (VR Temple and Spaceship not available)
  b.add(0x0043, F_V0_V4, A_CITY, 0, "TObjCityMainWarp", {{"type", p5}});

  // Lobby teleporter. When used, this object immediately ends the current game and sends the player back to the lobby.
  // If constructed offline, this object will do nothing and not render. This object takes no parameters.
  b.add(0x0044, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjCityAreaWarp", {});

  // Warp to another location on the same map. Used for the Principal's office warp. This warp is visible in all game
  // modes, but cannot be used in Episode 1 Battle or Challenge modes. Params:
  //   param1-3 = destination (same as for TObjMapWarpForest)
  //   param4 = destination angle (same as for TObjMapWarpForest)
  //   param5 = shape (0 = hexagonal, nonzero = "round" (12-sided))
  //   param6 = destination text (default 0 if negative, default 1 if >2):
  //     00 = "The Principal"
  //     01 = "Pioneer 2"
  //     02 = "Lab"
  std::vector<PA> TObjCityMapWarp_params = TObjMapWarpForest_params;
  TObjCityMapWarp_params.emplace_back(PA{"num_sides", "(param5 != 0) ? 12 : 6"});
  TObjCityMapWarp_params.emplace_back(PA{"dest_text", "(param6 > 2) ? 1 : max(0, param6)"});
  b.add(0x0045, F_V0_V4, A_CITY, 0, "TObjCityMapWarp", std::move(TObjCityMapWarp_params));

  // City doors. None of these take any parameters. TObjCityDoor_Warp is always closed in Challenge mode.
  b.add(0x0046, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "TObjCityDoor_Shop", {}); // Door to shop area
  b.add(0x0047, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "TObjCityDoor_Guild", {}); // Door to Hunter's Guild
  b.add(0x0048, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "TObjCityDoor_Warp", {}); // Door to Ragol warp
  b.add(0x0049, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "TObjCityDoor_Med", {}); // Door to Medical Center

  // Elevator visible in Pioneer 2. There are no parameters.
  b.add(0x004A, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "__ELEVATOR__", {});

  // Holiday event decorations. None of these take parameters except TObjCity_Season_SonicAdv2, which takes:
  //   param4 = model index (clamped to [0, 3])
  // The seasonal models are loaded only in online games; these objects will do nothing in offline games. The board's
  // models, including holiday models, are always loaded in both offline and online games.
  b.add(0x004B, F_V0_V4, A_CITY, 0, "TObjCity_Season_EasterEgg", {});
  b.add(0x004C, F_V0_V4, A_CITY, 0, "TObjCity_Season_ValentineHeart", {});
  b.add(0x004D, F_V0_V4, A_CITY, 0, "TObjCity_Season_XmasTree", {});
  b.add(0x004E, F_V0_V4, A_CITY, 0, "TObjCity_Season_XmasWreath", {});
  b.add(0x004F, F_V0_V4, A_CITY, 0, "TObjCity_Season_HalloweenPumpkin", {});
  b.add(0x0050, F_V0_V4, A_CITY, 0, "TObjCity_Season_21_21", {});
  b.add(0x0051, F_V0_V4, A_CITY, 0, "TObjCity_Season_SonicAdv2", {{"model_index", "clamp(param4, 0, 3)"}});
  b.add(0x0052, F_V0_V4, A_CITY, 0, "TObjCity_Season_Board", {});

  // Fireworks effect. Enabled when any season event is set. Params:
  //   param1 = spawn area x half-width (fireworks spawn at x = random(-1, 1) * param1)
  //   param2 = rise height (each firework adds random(0, 50) to this for its final max height)
  //   param3 = spawn area z half-width (fireworks spawn at z = random(-1, 1) * param1)
  //   param4 = launch period scale (when a firework is launched, the game generates a random number r in range [0,
  //     0x7FFF] and waits ((param4 + 60) * (r / 0x8000) * 3)) frames before launching the next firework)
  b.add(0x0053, F_V0_V4, A_E2_SEASIDE_NIGHT | A_CITY | A_E4_TEST, 0, "TObjCity_Season_FireWorkCtrl", {{"spawn_area_x_width", "param1 * 2"}, {"min_rise_height", "param2"}, {"spawn_area_z_width", "param3 * 2"}, {"launch_period_scale", "(param4 + 60) * 3"}});

  // Door that blocks the lobby teleporter in offline mode. There appear to be no parameters.
  b.add(0x0054, F_V0_V4, A_E1_P2 | A_E4_P2 | A_E4_TEST, 0, "TObjCityDoor_Lobby", {});

  // Version of the main warp for Challenge mode. This object looks like the main Ragol warp on Pioneer 2, but behaves
  // similarly to boss teleporters: it shows the player a Yes/No confirmation menu and sends 6x6A to synchronize state.
  // There is a global named last_set_mainwarp_value which is set to param4 when this object is constructed, but may be
  // changed by a set_mainwarp quest opcode after that. If that happens, this object replaces its dest_floor with the
  // floor specified in the last set_mainwarp quest opcode. Params:
  //   param4 = destination floor
  //   param5 = switch flag number (on GC, negative or >= 0x100 means no switch requirement; this is also ignored when
  //     the current area is 0, so a main warp on Episode 1 Pioneer 2 is unlocked regardless of this parameter)
  b.add(0x0055, F_V2_V4, A_CITY, 0, "TObjCityMainWarpChallenge", {{"dest_floor", p4}, {"switch_flag", p5}});

  // Episode 2 Lab door. Params:
  //   param4 = switch flag number and activation mode:
  //     negative value = always unlocked
  //     value in [0x00, 0x100] = unlocked by a switch flag (the bounds check appears to be a bug; the range should be
  //       [0x00, 0xFF] but the game has an off-by-one error)
  //     value > 0x100 = always locked
  //   param5 = model (green or red; clamped to [0, 1])
  //   param6 = permanent unlock flag (see TODoorCave01)
  b.add(0x0056, F_V3_V4, A_E2_LAB | A_E4_TEST, 0, "TODoorLabo", {{"switch_flag", p4}, {"model", "clamp(param5, 0, 1)"}, {"unlocks_permanently", "param6 < 0"}});

  // Disables the Trade Window when the player is near this object. Both players must not be near a TObjTradeCollision
  // object to be able to use the Trade Window with each other. Params:
  //   param1 = radius
  b.add(0x0057, F_V3_V4, A_CITY, A_E3_MORGUE, "TObjTradeCollision", {{"radius", p1}});

  // This object appears to be unused in both Ep3 NTE and the final release. It may have been an early version of Deck
  // Edit or Entry Counter sequence, perhaps responsible for setting the players' statuses when they're near either of
  // those counters. Params:
  //   param1 = radius
  b.add(0x0058, F_EP3, A_E3_MORGUE, 0, "TObjDeckCollision", {{"radius", p1}});

  // Forest door. Params:
  //   param1-3 = x, y, z coordinates for unlock "cutscene" (see param6)
  //   param4 = switch flag number (low byte) and number to appear on door (second-lowest byte, modulo 10)
  //   param6 = if set to 1, enables unlock "cutscene" (when the door is unlocked, the camera will snap to the
  //     coordinates in param1-3, pointing toward the door, for 2 seconds)
  b.add(0x0080, F_V0_V4, A_E1_FOREST | A_E4_TEST, 0, "TObjDoor", {{"cutscene_x", p1, "param6 == 1"}, {"cutscene_y", p2, "param6 == 1"}, {"cutscene_z", p3, "param6 == 1"}, {"door_number", "((param4 >> 8) & 0xFF) % 10"}, {"switch_flag", "param4 & 0xFF"}, {"enable_unlock_cutscene", "param6 == 1"}});

  // Forest switch. Params:
  //   param4 = switch flag number
  //   param6 = color (clamped to range [0, 9])
  b.add(0x0081, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E2_VR | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjDoorKey", {{"switch_flag", p4}, {"color_type", "clamp(param6, 0, 9)"}});

  // Laser fence and square laser fence. Params:
  //   param1 = color (<=0 = red, 1 = cyan, 2 = green, >=3 = magenta)
  //   param4 = switch flag number
  //   param6 = model (even value = short, odd value = long)
  std::vector<PA> TObjLazerFence_params{{"color_type", "clamp(int(param1), 0, 3)"}, {"switch_flag", p4}, {"model", "param6 & 1"}};
  b.add(0x0082, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_QUEST_ONLY | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjLazerFenceNorm", TObjLazerFence_params);
  b.add(0x0083, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_QUEST_ONLY | A_E2_FREE_PLAY | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjLazerFence4", TObjLazerFence_params);

  // Forest laser fence switch. Params:
  //   param4 = switch flag number
  //   param6 = color (0-3)
  b.add(0x0084, F_V0_V4, A_ALL & (~A_E1_RUINS) & (~A_E1_BOSS) & (~A_LOBBY) & (~A_E2_BOSS) & (~A_E4_BOSS) & (~A_CITY), 0, "TLazerFenceSw", {{"switch_flag", p4}, {"color_type", p6}});

  // Light rays. Params:
  //   param1 = texture vertical scroll speed
  //   param2 = vertical scale (y)
  //   param3 = horizontal scale (x, z)
  b.add(0x0085, F_V0_V4, A_E1_FOREST | A_E2_CCA_ALL | A_E4_DESERT | A_E4_TEST, 0, "TKomorebi", {{"vert_scroll_speed", p1}, {"vert_scale", p2}, {"horiz_scale", p3}});

  // Butterfly. There appear to be no parameters. param1-3 are copied into the object, but seem to never be used.
  b.add(0x0086, F_V0_V4, A_E1_FOREST | A_E4_DESERT | A_E4_TEST, 0, "TButterfly", {});

  // Small vehicle. Params:
  //   param1 = model number (even value = crashed, odd value = intact)
  //   param4 = quest label to call when activated (inherited from TObjMesBase)
  //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO") (inherited from TObjMesBase)
  b.add(0x0087, F_V0_V4, A_E1_FOREST | A_E4_TEST, 0, "TMotorcycle", {{"is_crashed", "!(int(param1) & 1)"}, {"label_number", p4}, {"type", "clamp(param6, 0, 1)"}});

  // Random item box. There are no parameters.
  b.add(0x0088, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E2_SEABED | A_E2_OLGA_FLOW | A_E4_NON_BOSS | A_E4_TEST, A_E3_BATTLE, "TObjContainerItem", {});

  // Elevated cylindrical tank. There appear to be no parameters; param1-3 are copied into the object instance as a
  // Vector3F, but it appears that that vector is never used.
  b.add(0x0089, F_V0_V4, A_E1_FOREST | A_E4_TEST, 0, "TObjTank", {});

  // TODO: Describe this object. Params:
  //   param1 = model (clamped to [0, 1])
  b.add(0x008A, F_V0_V2, A_E1_FOREST, 0, "TObjBattery", {{"model_index", "clamp(param1, 0, 1)"}});

  // Forest console. Params:
  //   param4 = quest label to call when activated (inherited from TObjMesBase)
  //   param5 = model (clamped to [0, 1])
  //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO") (inherited from TObjMesBase)
  std::vector<PA> TObjComputer_args{
      {"label_number", p4}, {"model", "clamp(param5, 0, 1)"}, {"type", "clamp(param6, 0, 1)"}};
  b.add(0x008B, F_V0_V1, A_E1_FOREST | A_E1_RUINS_3, 0, "TObjComputer", TObjComputer_args);
  b.add(0x008B, F_V2_V4, A_NON_BOSS | A_E4_TEST, 0, "TObjComputer", TObjComputer_args);

  // Black sliding door. Params:
  //   param1 = total distance (divided evenly by the number of switch flags, from param4)
  //   param2 = speed
  //   param4 = base switch flag (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.)
  //   param5 = number of switch flags (clamped to [1, 4])
  //   param6 = movement effect (0 = sparks + thud, 1 = grinding sound)
  std::vector<PA> TObjContainerIdo_params{
      {"total_distance", p1},
      {"speed", p2},
      {"first_switch_flag", p4},
      {"switch_flag_count", "clamp(param5, 1, 4)"},
      {"movement_effect", p6}};
  b.add(0x008C, F_V0_V1, A_E1_FOREST, 0, "TObjContainerIdo", TObjContainerIdo_params);
  b.add(0x008C, F_V2_V4, A_E1_FOREST | A_E1_CAVE_1 | A_E4_TEST, 0, "TObjContainerIdo", TObjContainerIdo_params);

  // Rico message pod. This object immediately destroys itself in Challenge mode or split-screen mode. Params:
  //   param4 = enable condition:
  //     negative = enabled when player is within 70 units
  //     in range [0x00, 0xFF] = enabled by corresponding switch flag
  //     0x100 and above = never enabled
  //   param5 = message number ("character ID" in qedit; if this is outside the range [100, 999] or there is no quest
  //     loaded, the quest label in param6 is called in the free play script instead of the quest script)
  //   param6 = quest label to call when activated (0 disables interactions with the object)
  std::vector<PA> TOCapsuleAncient01_params{{"switch_flag", p4}, {"message_number", p5}, {"use_free_play_script", "((param5 < 100) || (param5 > 999))"}, {"label_number", p6}};
  b.add(0x008D, F_V0_V4, A_E1_FREE_PLAY | A_E1_VOL_OPT | A_E4_TEST, 0, "TOCapsuleAncient01", TOCapsuleAncient01_params);

  // Energy barrier. Params:
  //   param4 = switch flag number and activation mode (same as for TODoorLabo)
  b.add(0x008E, F_V0_V4, A_E1_FOREST | A_E1_CAVE_2 | A_E1_CAVE_3 | A_E1_MINE | A_E4_NON_BOSS | A_E4_TEST, 0, "TOBarrierEnergy01", {{"switch_flag", p4}});

  // Forest rising bridge. Once enabled (risen), this object cannot be disabled; that is, there is no way to make the
  // bridge retract. When disabled, the bridge is 30 units below its initial position; when enabled, it rises to its
  // initial position. Params:
  //   param4 = switch flag number
  b.add(0x008F, F_V0_V4, A_E1_FOREST | A_E4_TEST, 0, "TObjHashi", {{"switch_flag", p4}});

  // Generic switch. Visually, this is the type usually used for objects other than doors, such as lights, poison
  // rooms, and the Forest 2 bridge. Params:
  //   param1 = activation mode:
  //     negative = allow interaction even when switch flag is enabled
  //     zero or positive = forbid interaction when switch flag is enabled (normal)
  //   param4 = switch flag number
  b.add(0x0090, F_V0_V4, A_E1_FOREST | A_E1_MINE | A_E1_QUEST_ONLY | A_E2_NON_BOSS | A_E4_NON_BOSS | A_E4_TEST, 0, "TOKeyGenericSw", {{"allow_interaction_after_enable", "param1 < 0"}, {"switch_flag", p4}});

  // Box that triggers a wave event when opened. Params:
  //   param4 = event number
  std::vector<PA> TObjContainerEnemy_params{{"event_id", p4, "", true}};
  b.add(0x0091, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E2_SEABED | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjContainerEnemy", TObjContainerEnemy_params);

  // Large box (usually used for specialized drops). Params:
  //   param1 = if zero or negative, box is specialized to drop a specific item or type of item; if positive, box drops
  //     any item (including box rares for the current floor), or nothing at all, and param3-6 are all ignored
  //   param3 = if zero, then only data1[0-1] (the high 2 bytes of param4) are used and the rest of the ItemData is
  //     cleared, then the usual randomized bonuses, grinds, etc. are applied to the item; if nonzero, the item is not
  //     randomized at all and drops exactly as specified in param4-6
  //   param4-6 = item definition (see below)
  // Not all fields in ItemData can be specified in the item definition for this object. The field order here does not
  // match the field order in ItemData! The item definition is encoded as follows:
  //               -param4-  -param5-  -param6-
  //   Weapon:     00wwZZSS  GG--PPQQ  PPQQPPQQ
  //   Armor:      0100ZZTT  00VV----  --------
  //   Shield:     0101ZZTT  00VV----  --------
  //   Unit:       0102ZZ00  00VV----  --------
  //   Mag:        02zz----  --------  --------
  //   Tool:       03zzZZ--  --------  --------
  //   Tech disk:  0302&&%%  --------  --------
  //   Meseta:     040000--  $$$$----  --------
  // Fields in the above table:
  //   - = ignored
  //   G = weapon grind
  //   P = attribute type (for S-ranks, custom name; last pair is kill count for some weapons)
  //   Q = attribute amount (for S-ranks, custom name; last pair is kill count for some weapons)
  //   S = weapon flags (80=untekked, 40=present) and special (low 6 bits)
  //   T = slot count
  //   U = tool flags (40=present; unused if item is stackable)
  //   V = armor/shield/unit flags (40=present; low 4 bits are present color)
  //   w = weapon class - 1 (second byte of item code, offset by 1, so e.g. Mechgun is 07 here, not 08)
  //   z = item class (second byte of item code)
  //   Z = item subclass (third byte of item code)
  //   & = technique level
  //   % = technique number
  //   $ = meseta amount, in increments of 10 (so max possible amount is 655350)
  // See base_item_for_specialized_box in ItemCreator.cc for newserv's implementation of decoding this format. In the
  // non-specialized case (param1 <= 0), param3-6 are still sent via the 6xA2 command when the box is opened on v3 and
  // later, and the server may choose to use those parameters for some purpose. The client implementation ignores them
  // when param1 > 0, and newserv does too.
  std::vector<PA> TObjContainerBase_params{{"ignore_def", "param1 > 0"}, {"use_item_type_only", "param3 == 0"}, {"item_def1", p4, "", true}, {"item_def2", p5, "", true}, {"item_def3", p6, "", true}};
  b.add(0x0092, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E1_BOSS | A_E2_SEABED | A_E2_OLGA_FLOW | A_E4_DESERT | A_E4_BOSS | A_E4_TEST, 0, "TObjContainerBase", TObjContainerBase_params);

  // Large enemy box. Same parameters as 0x0091 (TObjContainerEnemy).
  b.add(0x0093, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E2_SEABED | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjContainerAbeEnemy", TObjContainerEnemy_params);

  // Always-empty box. There are no parameters.
  b.add(0x0095, F_V0_V4, A_E1_FOREST | A_E1_CAVE | A_E1_MINE | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjContainerNoItem", {});

  // Laser fence. This object is only available in v2 and later. Params:
  //   param1 = color (clamped to [0, 3])
  //   param2 = half-depth of collision box (transverse to lasers)
  //   param3 = half-length of collision box (parallel to lasers)
  //   param4 = switch flag number
  //   param6 = model:
  //     0 = short fence
  //     1 = long fence
  //     anything else = invisible
  b.add(0x0096, F_V0_V4, A_NON_BOSS | A_E4_TEST, 0, "TObjLazerFenceExtra", {{"color_type", "clamp(param1, 0, 3)"}, {"collision_depth", "param2 * 2"}, {"collision_length", "param3 * 2"}, {"switch_flag", p4}, {"model_index", p6}});

  // Caves floor button. The activation radius is always 10 units. Params:
  //   param4 = switch flag number
  //   param5 = activation mode (ignored in offline solo, where all floor buttons use the zero/positive behavior):
  //     negative = temporary (disables flag when player leaves)
  //     zero or positive = permanent
  std::vector<PA> TOKeyCave01_params{{"switch_flag", p4}, {"is_sticky", "param5 >= 0"}};
  b.add(0x00C0, F_V0_V4, A_E1_CAVE | A_E1_QUEST_ONLY | A_E2_NON_BOSS | A_E4_NON_BOSS | A_E4_TEST, 0, "TOKeyCave01", TOKeyCave01_params);

  // Caves multiplayer door. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.; if this
  //     is negative, the door is always unlocked)
  //   param5 = 4 - number of switch flags (clamped to [0, 4]; if e.g. door should require only 3 switch flags, set
  //     param5 to 1)
  //   param6 = permanent unlock flag:
  //     negative = when all switch flags are enabled, door is permanently unlocked via 6x0B even if some switch flags
  //       are disabled later
  //     zero or positive = door only stays unlocked while all of the switch flags are active and locks again when any
  //       are disabled (no effect in single-player offline mode; the negative behavior is used instead)
  b.add(0x00C1, F_V0_V4, A_E1_CAVE | A_E4_TEST, 0, "TODoorCave01", {{"first_switch_flag", p4}, {"switch_flag_count", "4 - clamp(param5, 0, 4)"}, {"unlocks_permanently", "param6 < 0"}});

  // Caves standard door. Params:
  //   param4 = switch flag number (negative = always unlocked; 0x00-0xFF = switch flag number, 0x100 = same behavior
  //     as negative due to incorrect bounds check, >0x100 = always locked)
  std::vector<PA> TODoorCave02_params{{"switch_flag", p4}};
  b.add(0x00C2, F_V0_V4, A_E1_CAVE | A_E4_TEST, 0, "TODoorCave02", TODoorCave02_params);

  // Caves ceiling piston trap. There are three types of this object, which can be chosen via param6. If param6 is not
  // 0, 1, or 2, no object is created.
  // Params for TOHangceilingCave01Normal (param6 = 0):
  //   param1 = 1/4 time between crushes, in frames (value is param1 + 29, so total time between crushes is (param1 +
  //     29) * 4 frames)
  //   param2 = wait time in frames after construction until first crush (value is 1 - param2, so a more negative value
  //     means a longer wait time)
  //   param3 = base damage (value is param3 + 100; scaled by difficulty: Normal = 1x, Hard = 2x, Very Hard = 3x,
  //     Ultimate = 6x)
  // Params for TOHangceilingCave01Key (param6 = 1):
  //   param1-3 = same as for TOHangceilingCave01Normal
  //   param4 = switch flag number (drops when switch flag is activated; when it has finished dropping, it disables the
  //     switch flag)
  // Params for TOHangceilingCave01KeyQuick (param6 = 2):
  //   param1-4 = same as for TOHangceilingCave01Key, but unlike that object, does not disable the switch flag
  b.add(0x00C3, F_V0_V4, A_E1_CAVE | A_E2_VR | A_E2_TOWER | A_E4_TEST, 0, "TOHangceilingCave01_Variants", {{"crush_interval", "(param1 + 29) * 4"}, {"initial_crush_delay", "1 - param2"}, {"base_damage", "param3 + 100"}, {"switch_flag", p4, "param6 != 0"}, {"type", p6}});

  // Caves signs. There are no parameters.
  b.add(0x00C4, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_TEST, 0, "TOSignCave01", {});
  b.add(0x00C5, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_TEST, 0, "TOSignCave02", {});
  b.add(0x00C6, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_TEST, 0, "TOSignCave03", {});

  // Hexagonal tank. There appear to be no parameters.
  b.add(0x00C7, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_TEST, 0, "TOAirconCave01", {});

  // Brown platform. There appear to be no parameters.
  b.add(0x00C8, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_TEST, 0, "TOAirconCave02", {});

  // Revolving warning light. Params:
  //   param1 = rotation speed in degrees per frame
  b.add(0x00C9, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3 | A_E4_CRATER | A_E4_TEST, 0, "TORevlightCave01", {{"angular_velocity", p1}});

  // Caves rainbow. Params:
  //   param1-3 = scale factors (x, y, z)
  //   param4 = fade duration delta (the full fade takes (param4 / 1024 + 30) frames)
  //   param6 = radius delta squared (actual radius squared = param6 + 40000); entering this radius fades the rainbow
  //     out, leaving it fades the rainbow in
  b.add(0x00CB, F_V0_V4, A_E1_CAVE_2 | A_E4_TEST, 0, "TORainbowCave01", {{"scale_x", p1}, {"scale_y", p2}, {"scale_z", p3}, {"fade_duration_frames", "(param4 / 1024.0) + 30"}, {"radius", "sqrt(param6 + 40000)"}});

  // Floating jellyfish. It wanders around the initial position and return toward it after leaving the movement
  // radius. If a player approaches, it will retreat and fade away. Params:
  //   param1 = retreat trigger radius (any player within this distance causes it to retreat)
  //   param2 = movement radius (if it leaves this radius, it will move toward its home position until it's within
  //     (param2 / 5) of there)
  //   param3 = rebirth radius (it will un-fade only if no player is within this radius)
  b.add(0x00CC, F_V0_V4, A_E1_CAVE_2 | A_E2_SEABED | A_E4_TEST, 0, "TOKurage", {{"retreat_radius", p1}, {"movement_radius", p2}, {"rebirth_radius", p3}});

  // Floating dragonfly. Params:
  //   param1 = apparently intended to be random waypoint horizontal radius delta (value would be param1 + 50), but the
  //     base class constructor computes this too early, so the actual radius is always 51
  //   param2 = waypoint height offset: random flights use param2 * random(-1, 1); player avoidance uses param2 + 30
  //   param3 = movement step and waypoint arrival tolerance
  //   param4 = vertical bob frequency delta (cycles per 30 frames = param4 / 10 + 1)
  //   param5 = vertical bob amplitude delta (value is param5 / 10 + 1)
  std::vector<PA> TODragonflyCave01_params{{"waypoint_height_offset", p2}, {"movement_step", p3}, {"vert_bob_frequency", "(param4 / 10.0) + 1"}, {"vert_bob_amplitude", "(param5 / 10.0) + 1"}};
  b.add(0x00CD, F_V0_V4, A_E1_CAVE_2 | A_E1_SPACESHIP | A_E2_SPACESHIP | A_E4_DESERT | A_E4_TEST, 0, "TODragonflyCave01", TODragonflyCave01_params);

  // Caves door. Params:
  //   param4 = switch flag number (see TODoorCave02)
  b.add(0x00CE, F_V0_V4, A_E1_CAVE | A_E4_TEST, 0, "TODoorCave03", TODoorCave02_params);

  // Robot recharge station. Params:
  //   param4 = quest register number; activates when this register contains a nonzero value; does not deactivate if it
  //     becomes zero again
  b.add(0x00CF, F_V0_V4, A_E1_CAVE | A_E1_MINE | A_E2_TOWER | A_E4_TEST, 0, "TOBind", {{"quest_reg", p4}});

  // Caves cake shop. There are no parameters.
  b.add(0x00D0, F_V0_V4, A_E1_CAVE_3 | A_E4_TEST, 0, "TOCakeshopCave01", {});

  // Various solid rock objects used in the Cave areas. There are small, medium, and large variations of each, and for
  // the 02 variations, there are also "Simple" variations (00D7-00D9). None of these objects take any parameters.
  b.add(0x00D1, F_V0_V4, A_E1_CAVE_1 | A_E4_TEST, 0, "TORockCaveS01", {});
  b.add(0x00D2, F_V0_V4, A_E1_CAVE_1 | A_E4_TEST, 0, "TORockCaveM01", {});
  b.add(0x00D3, F_V0_V4, A_E1_CAVE_1 | A_E4_NON_BOSS | A_E4_TEST, 0, "TORockCaveL01", {});
  b.add(0x00D4, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveS02", {});
  b.add(0x00D5, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveM02", {});
  b.add(0x00D6, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveL02", {});
  b.add(0x00D7, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveSS02", {});
  b.add(0x00D8, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveSM02", {});
  b.add(0x00D9, F_V0_V4, A_E1_CAVE_2, 0, "TORockCaveSL02", {});
  b.add(0x00DA, F_V0_V4, A_E1_CAVE_3, 0, "TORockCaveS03", {});
  b.add(0x00DB, F_V0_V4, A_E1_CAVE_3, 0, "TORockCaveM03", {});
  b.add(0x00DC, F_V0_V4, A_E1_CAVE_3, 0, "TORockCaveL03", {});

  // Floor button that can't be pressed (it only shows the state of a switch flag). Params:
  //   param1-3 = scale factors (visual only)
  //   param4 = switch flag number
  //   param5 low word = model index (VR Temple / VR Spaceship: 0 = Caves button, nonzero = Mines button)
  //   param6 high word = sound activation mode:
  //     1 = play sound only when switch activated
  //     anything else = play sound when activated and when deactivated
  //   param6 low word = sound to play (TODO: describe these):
  //     1 => 4005281F (GC)
  //     2 => 00053F06 (GC)
  //     3 => 00052812 (GC; does not play in Ep1 Palace/Spaceship)
  //     4 => 00052813 (GC; does not play in Ep1 Palace/Spaceship)
  //     5 => 4001C112 (GC)
  //     6 => 4002A60B (GC)
  //     anything else (including 0) => no sound
  //     TODO: Are there more sounds available here on BB?
  b.add(0x00DE, F_V2_V4, A_NON_BOSS | A_E4_TEST, 0, "TODummyKeyCave01", {{"scale_x", p1}, {"scale_y", p2}, {"scale_z", p3}, {"switch_flag", p4}, {"model_index", "clamp(param5 & 0xFFFF, 0, 1)"}, {"enable_deactivate_sound", "((param6 >> 16) & 0xFFFF) != 1"}, {"sound_index", "param6 & 0xFFFF"}});

  // Breakable rocks, in small, medium, and large variations. All of these take the following parameter:
  //   param4 = switch flag number
  b.add(0x00DF, F_V2_V4, A_E1_CAVE_1 | A_E4_TEST, 0, "TORockCaveBL01", {{"switch_flag", p4}});
  b.add(0x00E0, F_V2_V4, A_E1_CAVE_2 | A_E4_TEST, 0, "TORockCaveBL02", {{"switch_flag", p4}});
  b.add(0x00E1, F_V2_V4, A_E1_CAVE_3 | A_E4_TEST, 0, "TORockCaveBL03", {{"switch_flag", p4}});

  // Mines multi-switch door. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.; if this
  //     is negative, the door is always unlocked)
  //   param5 = 4 - number of switch flags (clamped to [0, 4]; if e.g. door should require only 3 switch flags, set
  //     param5 to 1)
  b.add(0x0100, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TODoorMachine01", {{"first_switch_flag", p4}, {"switch_flag_count", "4 - clamp(param5, 0, 4)"}});

  // Mines floor button. The activation radius is always 10 units. Same params as TOKeyCave01.
  b.add(0x0101, F_V0_V1, A_E1_MINE, 0, "TOKeyMachine01", TOKeyCave01_params);
  b.add(0x0101, F_V2_V4, A_E1_FOREST | A_E1_MINE | A_E1_QUEST_ONLY | A_E2_VR | A_E4_NON_BOSS | A_E4_TEST, 0, "TOKeyMachine01", TOKeyCave01_params);

  // Mines single-switch door, or Subterranean Desert door if in Episode 4. Params (for both object types):
  //   param4 = switch flag number (see TODoorCave02)
  b.add(0x0102, F_V0_V4, A_E1_MINE, 0, "TODoorMachine02", TODoorCave02_params);
  b.add(0x0102, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__EP4_DOOR__", TODoorCave02_params);

  // Large cryotube. There appear to be no parameters.
  b.add(0x0103, F_V0_V4, A_E1_MINE | A_E2_TOWER | A_E4_TEST, 0, "TOCapsuleMachine01", {});

  // Computer. Same parameters as TOCapsuleAncient01. Unlike that object, this object does not immediately destroy
  // itself in Challenge or split-screen mode.
  b.add(0x0104, F_V0_V4, A_E1_MINE | A_E2_TOWER | A_E4_TEST, 0, "TOComputerMachine01", TOCapsuleAncient01_params);

  // Green monitor. Params:
  //   param4 = initial state (clamped to [0, 3]; cycles through those 4 values on its own)
  b.add(0x0105, F_V0_V4, A_E1_MINE | A_E2_TOWER | A_E4_TEST, 0, "TOMonitorMachine01", {{"initial_state", "clamp(param4, 0, 3)"}});

  // Floating robot. Same params as 0x00CD (TODragonflyCave01).
  b.add(0x0106, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TODragonflyMachine01", TODragonflyCave01_params);

  // Floating rotating blue light (often appears next to doors). The light rotates about its vertical axis and floats
  // up and down sinusoidally. Params:
  //   param4 = float cycles per second (value is param4 * 0.1 + 1.0)
  //   param5 = max float distance (value is param5 * 0.1 + 0.5)
  //   param6 = rotation speed in angle units per frame (0x10000 = 1 complete rotation)
  b.add(0x0107, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOLightMachine01", {{"float_cycles_per_sec", "(param4 * 0.1) + 1"}, {"max_float_distance", "(param5 * 0.1) + 0.5"}, {"angular_velocity", p6}});

  // Self-destructing objects. Params:
  //   param1 = apparently intended to be radius delta (value would be param1 + 30), but the base class constructor
  //     computes this too early, so the actual radius is always 31
  b.add(0x0108, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOExplosiveMachine01", {});
  b.add(0x0109, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOExplosiveMachine02", {});
  b.add(0x010A, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOExplosiveMachine03", {});

  // Spark machine. This looks like it's intended to appear in the bridge room in Mine 1, to create an effect of the
  // columnar machines sparking. This is implemented as four columns that randomly change their visibility, intended to
  // be in the same position as the map geometry. Each column has an accumulator value, which is initially zero. Every
  // frame, the value (param1 - 0.98) is added to each column's accumulator and a random number between 0 and 1 is
  // chosen; if the random number is less than the column's accumulator, its visibility state is changed and its
  // accumulator is reset to zero. In this manner, param1 can be thought of as the frequency of state changes - 0.98
  // would mean they never change state, 1.98 would mean they change every frame. Params:
  //   param1 = state change accumulation per frame (value is param1 - 0.98)
  //   param2 = if <= 0, only column 1 flickers and columns 0, 2, and 3 are hidden; if > 0, all four columns flicker
  b.add(0x010B, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOSparkMachine01", {{"state_change_per_frame", "param1 - 0.98"}, {"one_column_only", "param2 <= 0"}});

  // Open stall with a spinning red light on either side. Params:
  //   param1 = if > 0, an additional model is rendered (TODO: describe it)
  //   param2 = if > 0, a gray box is present in the left half of the stall
  b.add(0x010C, F_V0_V4, A_E1_MINE | A_E4_TEST, 0, "TOHangerMachine01", {{"is_occupied", "param1 > 0"}, {"has_gray_box", "param2 > 0"}});

  // Ruins entrance door (after Vol Opt). This object reads quest flags 0x2C, 0x2D, and 0x2E to determine the state of
  // each seal on the door (for Forest, Caves, and Mines respectively). It then checks quest flag 0x2F; if this flag is
  // set, then all seals are unlocked regardless of the preceding three flags' values. All of these flags are checked
  // every frame, not only at construction time. In online mode, the door is always open regardless of the quest flag
  // state. This object takes no parameters.
  b.add(0x0130, F_V0_V4, A_E1_VOL_OPT | A_E4_TEST, 0, "TODoorVoShip", {});

  // Ruins floor warp. Params:
  //   param4 = destination floor
  //   param6 = color (negative = red, zero or positive = blue); if this is >= 0 in Challenge mode, the warp is
  //     destroyed immediately
  b.add(0x0140, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TObjGoalWarpAncient", {{"dest_floor", p4}, {"is_red", "param6 < 0"}});

  // Ruins intra-area warp. Params:
  //   param1-3 = destination (same as for TObjMapWarpForest)
  //   param4 = destination angle (same as for TObjMapWarpForest)
  //   param5 = if negative, no warp lines render (only the floor pad appears) and the player cannot use the warp; if
  //     zero or positive, the warp functions normally
  std::vector<PA> TObjMapWarpAncient_params = TObjMapWarpForest_params;
  TObjMapWarpAncient_params.emplace_back(PA{"disabled", "param5 < 0"});
  b.add(0x0141, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TObjMapWarpAncient", TObjMapWarpAncient_params);

  // Ruins switch. Params:
  //   param4 = switch flag number
  //   param5 = activation mode (same as TOKeyGenericSw's param1)
  b.add(0x0142, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOKeyAncient02", {{"switch_flag", p4}, {"allow_interaction_after_enable", "param5 < 0"}});

  // Ruins floor button. Same parameters as 0x00C0 (TOKeyCave01).
  b.add(0x0143, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOKeyAncient03", TOKeyCave01_params);

  // Ruins doors. These all take the same params as 0x00C2 (TODoorCave02).
  b.add(0x0144, F_V0_V4, A_E1_RUINS_1 | A_E4_TEST, 0, "TODoorAncient01", TODoorCave02_params);
  b.add(0x0145, F_V0_V4, A_E1_RUINS_3 | A_E4_TEST, 0, "TODoorAncient03", TODoorCave02_params);
  b.add(0x0146, F_V0_V4, A_E1_RUINS_2 | A_E4_TEST, 0, "TODoorAncient04", TODoorCave02_params);
  b.add(0x0147, F_V0_V4, A_E1_RUINS_1 | A_E4_TEST, 0, "TODoorAncient05", TODoorCave02_params);
  b.add(0x0148, F_V0_V4, A_E1_RUINS_2 | A_E4_TEST, 0, "TODoorAncient06", TODoorCave02_params);
  b.add(0x0149, F_V0_V4, A_E1_RUINS_3 | A_E4_TEST, 0, "TODoorAncient07", TODoorCave02_params);

  // Ruins 4-player door. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, and param4 +
  //     3); param4 is clamped to [0, 0xFC]
  //   param6 = permanent unlock flag; same as for 0x00C1 (TODoorCave01)
  b.add(0x014A, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TODoorAncient08", {{"first_switch_flag", "clamp(param4, 0, 0xFC)"}, {"unlocks_permanently", "param6 < 0"}});

  // Ruins 2-player door. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4 and param4 + 1); param4 is clamped to
  //     [0, 0xFE]
  //   param6 = permanent unlock flag; same as for 0x00C1 (TODoorCave01)
  b.add(0x014B, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TODoorAncient09", {{"first_switch_flag", "clamp(param4, 0, 0xFE)"}, {"unlocks_permanently", "param6 < 0"}});

  // Ruins sensor. Params:
  //   param1 = activation radius delta (actual radius is param1 + 50, clamped below to 0)
  //   param4 = switch flag number
  //   param5 = activation condition (see TOSensorGeneral01)
  //   param6 = texture index; uses fs_obj_o_sensor01r if <= 0, uses fs_obj_o_sensor02r if > 0; the two texture files
  //     are identical (at least on GC) so this has no user-visible effects
  b.add(0x014C, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOSensorAncient01", {{"radius", "max(0, param1 + 50)"}, {"switch_flag", p4}, {"activation_type", "param5 >= 0"}, {"texture_index", "param6 > 0"}});

  // Ruins laser fence switch. Params:
  //   param1 = activation mode (same as TOKeyGenericSw's param1)
  //   param4 = switch flag number
  //   param5 = color (clamped to [0, 3])
  b.add(0x014D, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOKeyAncient01", {{"allow_interaction_after_enable", "param1 < 0"}, {"switch_flag", p4}, {"color_index", "clamp(param5, 0, 3)"}});

  // Ruins fence objects. Params:
  //   param4 = activation mode / switch flag number:
  //     negative = proximity: open when any player is within 70 units; close when no player is within 100 units
  //     0x00-0xFF = open when corresponding switch flag is enabled
  //     0x100 = always open (probably unintentional)
  //     >=0x101 = always closed
  //   param5 = color (clamped to [0, 3])
  std::vector<PA> TOFenceAncient_params{{"switch_flag", p4}, {"color_index", "clamp(param5, 0, 3)"}};
  b.add(0x014E, F_V0_V4, A_E1_RUINS | A_E4_NON_BOSS | A_E4_TEST, 0, "TOFenceAncient01", TOFenceAncient_params); // 4x2
  b.add(0x014F, F_V0_V4, A_E1_RUINS | A_E4_NON_BOSS | A_E4_TEST, 0, "TOFenceAncient02", TOFenceAncient_params); // 6x2
  b.add(0x0150, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOFenceAncient03", TOFenceAncient_params); // 4x4
  b.add(0x0151, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOFenceAncient04", TOFenceAncient_params); // 6x4

  // Ruins poison-spewing blob. This object is technically an item box, and drops an item when destroyed. It cannot be
  // specialized (ignore_def is always true). It cycles through the following 3 phases until it's destroyed:
  //   Phase 0: idle (duration depends on param1 and players' positions)
  //   Phase 1: preparing to spew poison (always lasts 60 frames)
  //   Phase 2: spewing poison (duration specified by param2)
  // Params:
  //   param1 = maximum phase 0 duration in frames (value is param1 + 299; it advances to phase 1 early if a player is
  //     within 80 units of it)
  //   param2 = duration of phase 2 in frames (value is param2 + 209)
  //   param3 = poison radius squared (value is param3 + 399, so if param3 = 1 for example, the poison radius is 20)
  //   param6 = how often to create more particles during phase 2 (in frames; value is param6 + 4)
  b.add(0x0152, F_V0_V4, A_E1_RUINS | A_E2_CCA_ALL | A_E4_DESERT | A_E4_TEST, 0, "TContainerAncient01", {{"max_idle_frames", "param1 + 299"}, {"poison_duration_frames", "param2 + 209"}, {"poison_radius", "sqrt(param3 + 399)"}, {"frames_between_poison_particles", "param6 + 4"}});

  // Ruins falling trap. Params:
  //   param1 = trigger radius delta (value is (param1 / 2) + 30)
  //   param2 = initial height above target position (clamped below to 0)
  //   param3 = falling duration in frames (clamped below to 1)
  //   param4 = base damage power (clamped below to 0, then scaled by difficulty: Normal = x1, Hard = x2, Very Hard =
  //     x3, Ultimate = x6)
  //   param5 = damage type (clamped to [0, 5]; see TOMineIcon01 param5 for damage type values)
  //   param6 = explosion delay in frames (value is param6 + 30, clamped below to 0; this is multiplied by 0.33 in
  //     offline games)
  b.add(0x0153, F_V0_V4, A_E1_RUINS | A_E2_VR | A_E4_TEST, 0, "TOTrapAncient01", {{"trigger_radius", "(param1 / 2) + 30"}, {"initial_height", "max(0, param2)"}, {"fall_duration_frames", "max(1, param3)"}, {"base_damage", "max(0, param4)"}, {"damage_type", "clamp(param5, 0, 5)"}, {"explosion_delay_frames", "max(0, param6 + 30)"}});

  // Ruins pop-up trap. Params:
  //   param1 = trigger radius delta (value is (param1 / 2) + 30)
  //   param4 = player hold time, in frames (value is param4 + 30; clamped below to 0)
  //   angle.z = hide body (positive = only the blue highlights appear; zero or negative = normal visibility)
  b.add(0x0154, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOTrapAncient02", {{"trigger_radius", "(param1 / 2) + 30"}, {"player_hold_frames", "max(0, param4 + 30)"}, {"hide_body", "angle.z > 0"}});

  // Ruins crystal monument. Same parameters as TOCapsuleAncient01. Unlike that object, this object does not
  // immediately destroy itself in Challenge or split-screen mode.
  b.add(0x0155, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOMonumentAncient01", TOCapsuleAncient01_params);

  // Non-Ruins monument. This object destroys itself in Challenge mode and is not targetable in offline multiplayer.
  // Sets a quest flag when activated; the quest flag depends on which area the object is in:
  //   Mine 2 = 0x2E
  //   Cave 2 = 0x2D
  //   Any other area = 0x2C
  // When all three of the above quest flags are active, this object also sets quest flag 0x2F. Params:
  //   param2 = vertical offset after activation
  b.add(0x0156, F_V0_V4, A_E1_FOREST_2 | A_E1_CAVE_2 | A_E1_MINE_2 | A_E4_TEST, 0, "TOMonumentAncient02", {{"active_vert_offset", p2}});

  // Ruins rocks. None of these take any parameters.
  b.add(0x0159, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient01", {});
  b.add(0x015A, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient02", {});
  b.add(0x015B, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient03", {});
  b.add(0x015C, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient04", {});
  b.add(0x015D, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient05", {});
  b.add(0x015E, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient06", {});
  b.add(0x015F, F_V0_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncient07", {});

  // Invisible cross-floor warp. This is used for the warp behind the door to Ruins after defeating Vol Opt, and is
  // only available on that floor (on any other floor, 0x0160 constructs TObjFogCollisionPoison instead.) Params:
  //   param4 = destination floor
  b.add(0x0160, F_V0_V4, A_E1_VOL_OPT | A_E4_TEST, 0, "TObjWarpBoss03", {{"dest_floor", p4}});

  // Switchable, foggy area that's visible and hurts the player if the switch flag isn't on. Params are the same as for
  // 0x0018 (TObjFogCollisionSwitch), but there is also:
  //   param2 = poison power (scaled by difficulty: Normal = x1, Hard = x2, Very Hard = x3, Ultimate = x6)
  std::vector<PA> TObjFogCollisionPoison_params = TObjFogCollisionSwitch_params;
  TObjFogCollisionPoison_params.emplace_back(PA{"poison_power", p2});
  b.add(0x0160, F_V0_V4, A_E1_RUINS | A_E2_SPACESHIP | A_E2_SEABED | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjFogCollisionPoison", TObjFogCollisionPoison_params);

  // Ruins specializable box. Same parameters as 0x0092 (TObjContainerBase).
  b.add(0x0161, F_V0_V4, A_E1_RUINS | A_E1_QUEST_ONLY | A_E2_VR | A_E2_BARBA_RAY | A_E2_GOL_DRAGON | A_E4_TEST, 0, "TOContainerAncientItemCommon", TObjContainerBase_params);

  // Ruins random box. There are no parameters.
  b.add(0x0162, F_V0_V4, A_E1_RUINS | A_E1_QUEST_ONLY | A_E2_VR | A_E2_BARBA_RAY | A_E2_GOL_DRAGON | A_E4_TEST, 0, "TOContainerAncientItemRare", {});

  // Ruins enemy boxes, disguised as either of the above two objects. Params:
  //   param4 = event number
  b.add(0x0163, F_V0_V4, A_E1_RUINS | A_E1_QUEST_ONLY | A_E2_VR | A_E4_TEST, 0, "TOContainerAncientEnemyCommon", {{"event_id", p4, "", true}});
  b.add(0x0164, F_V0_V4, A_E1_RUINS | A_E1_QUEST_ONLY | A_E2_VR | A_E4_TEST, 0, "TOContainerAncientEnemyRare", {{"event_id", p4, "", true}});

  // Ruins always-empty box. There are no parameters.
  b.add(0x0165, F_V2_V4, A_E1_RUINS | A_E1_QUEST_ONLY | A_E2_VR | A_E4_TEST, 0, "TOContainerAncientItemNone", {});

  // Ruins breakable rock. Params:
  //   param4 = switch flag number (when enabled, destroys this object)
  b.add(0x0166, F_V2_V4, A_E1_RUINS | A_E4_TEST, 0, "TOWreckAncientBrakable05", {{"switch_flag", p4}});

  // Ruins pop-up trap with techs. Params:
  //   param1 = trigger radius delta (value is (param1 / 2) + 30)
  //   param2 = number of hits to destroy trap (clamped to [1, 256])
  //   param3 = tech level modifier (note: level is offset by 1, so a value of 0 means tech level 1):
  //     Normal: level = param3 + 1 (clamped to [0, 14])
  //     Hard: level = param3 + 3 (clamped to [0, 14])
  //     Very Hard: level = param3 + 5 (clamped to [0, 14])
  //     Ultimate: level = param3 + 7 (clamped to [0, 29])
  //   param4 = frames to wait after casting tech (value is param4 + 30; clamped below to 0)
  //   param5 = switch flag number (always active if value is not in [0x00, 0xFF])
  //   param6 = tech number:
  //     0 or negative = Foie
  //     1 = Gizonde
  //     2 = Gibarta
  //     3 = Megid
  //     4 or greater = Gifoie
  //   angle.z = hide body (same as for TOTrapAncient02)
  b.add(0x0167, F_V2_V4, A_E1_MINE | A_E1_RUINS | A_E2_NON_BOSS | A_E4_TEST, 0, "TOTrapAncient02R", {{"trigger_radius", "(param1 / 2) + 30"}, {"hits_required", "clamp(param2, 1, 256)"}, {"base_tech_level", "param3 + 1"}, {"delay_frames_after_cast", "max(0, param4 + 30)"}, {"switch_flag", p5}, {"tech_index", "clamp(param6, 0, 4)"}, {"hide_body", "angle.z > 0"}});

  // Flying white birds. Params:
  //   param1 = movement speed delta (units per frame = param1 + 1)
  //   param2 = random waypoint vertical range delta (value is param2 + 50)
  //   param3 = random waypoint horizontal radius delta (value is param3 + 100)
  //   param4 = bird count delta (count is param4 + 3; clamped to [1, 6], but 0 is also allowed if param4 == -3)
  //   param5 = vertical bob frequency delta (cycles per 30 frames = param5 / 10 + 1)
  //   param6 = vertical bob amplitude delta (value is param6 / 10 + 1)
  b.add(0x0170, F_V0_V4, A_E1_DARK_FALZ | A_E1_SPACESHIP | A_E2_SPACESHIP | A_E4_TEST, 0, "TOBoss4Bird", {{"movement_speed", "param1 + 1"}, {"waypoint_vert_range", "param2 + 50"}, {"waypoint_horiz_range", "param3 + 100"}, {"bird_count", "(param4 == -3) ? 0 : clamp(param4 + 3, 1, 6)"}, {"vert_bob_frequency", "(param5 / 10.0) + 1"}, {"vert_bob_amplitude", "(param6 / 10.0) + 1"}});

  // Dark Falz obelisk. There appear to be no parameters.
  b.add(0x0171, F_V0_V4, A_E1_DARK_FALZ | A_E4_TEST, 0, "TOBoss4Tower", {});

  // Floating rocks. Params:
  //   param1 = x/z spawn range delta (value is param1 + 50)
  //   param2 = upward speed scale (each rock chooses a speed in [abs(param2) / 2, abs(param2)] at (re)spawn time)
  //   param4 = number of rocks (clamped to [1, 8])
  b.add(0x0172, F_V0_V4, A_E1_DARK_FALZ | A_E4_TEST, 0, "TOBoss4Rock", {{"xz_spawn_range", "param1 + 50"}, {"upward_speed_scale", p2}, {"rock_count", "clamp(param4, 1, 8)"}});

  // Floating soul. Params:
  //   param1-3 = x/y/z range
  //   param4 = animation variant (negative = variant 0, nonnegative = variant 1)
  //   param5-6 = x/z oscillation speed per frame, where 0x10000 is a full cycle
  b.add(0x0173, F_V0_V2, A_E1_DARK_FALZ, 0, "TOSoulDF", {{"range_x", p1}, {"range_y", p2}, {"range_z", p3}, {"animation_variant", "param4 >= 0"}, {"osc_speed_x", p5}, {"osc_speed_z", p6}});

  // Butterfly. This is a subclass of TODragonfly and takes the same params as 0x00CD (TODragonflyCave01), but also:
  //   param6 = model number? (clamped to [0, 2])
  std::vector<PA> TOButterflyDF_params = TODragonflyCave01_params;
  TOButterflyDF_params.emplace_back(PA{"model_index", "clamp(param6, 0, 2)"});
  b.add(0x0174, F_V0_V2, A_E1_DARK_FALZ, 0, "TOButterflyDF", TOButterflyDF_params);

  // Lobby information counter (game menu) collision. Params:
  //   param1 = radius
  b.add(0x0180, F_V0_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjInfoCol", {{"radius", p1}});

  // Warp between lobbies (not warp out of game to lobby). Params:
  //   param5 = hide beams (beams shown if <= 0, hidden if > 0)
  b.add(0x0181, F_V0_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjWarpLobby", {{"hide_beams", "param5 > 0"}});

  // Main lobby decoration. Params:
  //   param4 = default decorations when there is no event:
  //     1 = flowers (Lobby 2)
  //     2 = fountains (Lobby 4)
  //     3 = very tall aquariums (Lobby 5)
  //     4 = green trees (Lobby 1)
  //     5 = stained glass windows (Lobby 9)
  //     6 = snowy evergreen trees (Lobby 10)
  //     7 = windmills (Lobby 3)
  //     8 = spectral columns (Lobby 8)
  //     9 = planetary models (Lobby 7)
  //     Anything else = grass (Lobby 6)
  b.add(0x0182, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjLobbyMain", {{"decoration_index", p4}});

  // Lobby pigeon. Params:
  //   param4 = movement/path pattern (clamped to [0, 2])
  //   param5 = season: <= 0 = no season event, 1 = Wedding, >= 2 = Valentine's Day or White Day (but see param6)
  //   param6 = model (<=0 = model 0, >0 = model 1; if positive and param5 >= 1, show only during Halloween)
  b.add(0x0183, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY | A_E3_BATTLE, "__LOBBY_PIGEON__", {{"path_pattern", "clamp(param4, 0, 2)"}, {"season", p5}, {"model_index", "param6 > 0"}});

  // Lobby butterfly. Invisible if the current seasonal event is not 0 (none). Params:
  //   param4 = model number (clamped to [0, 1])
  b.add(0x0184, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY | A_E3_BATTLE, "TObjButterflyLobby", {{"model_index", "clamp(param4, 0, 1)"}});

  // Lobby rainbow. Visible only when there is no season event. There are no parameters.
  b.add(0x0185, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY | A_E3_BATTLE, "TObjRainbowLobby", {});

  // Lobby pumpkin. Visible only during Halloween season event. There are no parameters.
  b.add(0x0186, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjKabochaLobby", {});

  // Lobby stained-glass windows. Params:
  //   param4 = event flag:
  //     zero or negative = visible only when no season event is active
  //     positive = visible only during Christmas season event
  b.add(0x0187, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjStendGlassLobby", {{"christmas", "param4 > 0"}});

  // Lobby red and white striped curtain. Visible only during the spring and summer season events (12 and 13). There
  // are no parameters.
  b.add(0x0188, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjCurtainLobby", {});

  // Lobby wedding arch. Visible only during the wedding season event (10). There are no parameters.
  b.add(0x0189, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjWeddingLobby", {});

  // Lobby snowy evergreen tree (Lobby 10). Visible only when there is no season event. There are no parameters.
  b.add(0x018A, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjTreeLobby", {});

  // Lobby aquarium (Lobby 5). Visible only when there is no season event. There are no parameters.
  b.add(0x018B, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjSuisouLobby", {});

  // Lobby particles. This is like TObjParticle, but has logic to (dis)appear when the season event changes. Params:
  //   param1 = particle effect ID (clamped to [0, 575] on v3)
  //   param3 = same as param3 from 0001 (TObjParticle)
  //   param4 = season visibility; annoyingly, the values here aren't the same as the standard event values:
  //     1 = Christmas (1)
  //     2 = Halloween (5)
  //     3 = New Year (7)
  //     4 = Valentine's Day (3)
  //     5 = White Day (9)
  //     6 = Summer (8)
  //     7 = Easter (4)
  //     8 = Wedding (10)
  //     9 = Fall (11)
  //     anything else (including 0) = no event (0)
  //   param5 = same as param4 (not param5!) from 0001 (TObjParticle)
  //   param6 = same as param5 (not param6!) from 0001 (TObjParticle)
  b.add(0x018C, F_V3_V4, A_LOBBY | A_E4_TEST, A_LOBBY, "TObjParticleLobby", {{{"type", p1}, {"flags_type", p3}, {"season_type", p4}, {"dist_flag1", p5}, {"layer", p6}}});

  // Episode 3 lobby battle table. This object is responsible for the red panels on the floor next to the battle table
  // that turn green when you step on them; it also shows the confirmation window and sends the necessary commands to
  // the server. The actual table model and the non-lit parts of the floor panels are part of the lobby geometry, not
  // this object. Params:
  //   param4 = seat count
  //     1 = 1 seat (doesn't work properly - there's no way to confirm)
  //     2 = 2 seats
  //     3 = 3 seats (normally unused, but works)
  //     4 = 4 seats
  //     anything else = object doesn't load
  //   param5 = table number (used in E4 and E5 commands)
  b.add(0x018D, F_EP3, A_LOBBY, 0, "TObjLobbyTable", {{"seat_count", p4}, {"table_number", p5}});

  // Episode 3 lobby jukebox. There are no parameters.
  b.add(0x018E, F_EP3, A_LOBBY, 0, "TObjJukeBox", {});

  // Spaceship overhead camera with red lights and annoying gear noises. There appear to be no parameters.
  b.add(0x0190, F_V2_V4, A_E1_SPACESHIP | A_E2_SPACESHIP | A_E4_TEST, 0, "TObjCamera", {});

  // Short Spaceship wall. Creates a TObjMapCsn child with dimensions (32, 20, 12) and passes param4 and param5 through
  // to it; see TObjMapCsn for descriptions of those parameters. There are no other parameters.
  std::vector<PA> TObjMapCsn_flags_params{{"wall_type", p4}, {"flags", p5}};
  b.add(0x0191, F_V2_V4, A_E1_SPACESHIP | A_E2_SPACESHIP | A_E2_TOWER | A_E4_TEST, 0, "TObjTuitate", TObjMapCsn_flags_params);

  // Spaceship door. Params:
  //   param4 = switch flag number (see TODoorCave02)
  b.add(0x0192, F_V2_V4, A_E1_SPACESHIP | A_E2_SPACESHIP | A_E4_TEST, 0, "TObjDoaEx01", TODoorCave02_params);

  // Tall Spaceship wall. Same params as TObjTuitate, but the TObjMapCsn child has dimensions (32, 40, 12).
  b.add(0x0193, F_V2_V4, A_E1_SPACESHIP | A_E2_SPACESHIP | A_E2_TOWER | A_E4_TEST, 0, "TObjBigTuitate", TObjMapCsn_flags_params);

  // Temple door. Params:
  //   param4 = switch flag number (see TODoorCave02)
  b.add(0x01A0, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TODoorVS2Door01", TODoorCave02_params);

  // Temple rubble. Each of these has a child TObjMapCsn object, to which the params are passed unchanged - so each of
  // these effectively takes the same params as TObjMapCsn.
  // Partly-broken wall (like breakable wall)
  b.add(0x01A1, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck01", TObjMapCsn_flags_params);
  // Broken column
  b.add(0x01A2, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck02", TObjMapCsn_flags_params);
  // Broken wall pieces lying flat
  b.add(0x01A3, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck03", TObjMapCsn_flags_params);
  // Column
  b.add(0x01A4, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck04", TObjMapCsn_flags_params);
  // Broken toppled column
  b.add(0x01A5, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck05", TObjMapCsn_flags_params);
  // Truncated conic monument
  b.add(0x01A6, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wreck06", TObjMapCsn_flags_params);

  // Temple breakable wall, which looks like 0x01A1 (TOVS2Wreck01). This object synchronizes its hit counter with other
  // clients via the 6x86 command. Params:
  //   param4 = number of hits, minus 256 for some reason (for example, for a 6-hit wall, this should be -250)
  b.add(0x01A7, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TOVS2Wall01", {{"hits_required", "param4 + 256"}});

  // Lens flare enable/disable switch. This object triggers when the local player is within 20 units in the x/z
  // dimensions, and sets a global which determines whether objects of type 0x001E (__LENS_FLARE__) should render
  // anything. Params:
  //   param1 = if > 0, enable lens flare rendering; if <= 0, disable it
  // This object isn't constructed in split-screen mode.
  b.add(0x01A8, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E2_BARBA_RAY | A_E4_CRATER | A_E4_TEST, 0, "__LENS_FLARE_SWITCH_COLLISION__", {{"enable", "param1 > 0"}});

  // Rising bridges. Similar to 0x008F (TObjHashi). Params:
  //   param1 = extra depth when lowered (this is added to TObjHashiBase's 30 unit displacement if the bridge is
  //     lowered when constructed)
  //   param2 = rise speed in units per frame
  //   param4 = switch flag number
  std::vector<PA> TObjHashiVersus_params{{"extra_lower_depth", p1}, {"rise_speed", p2}, {"switch_flag", p4}};
  // Small brown rising bridge
  b.add(0x01A9, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TObjHashiVersus1", TObjHashiVersus_params);
  // Long rising bridge
  b.add(0x01AA, F_V2_V4, A_E1_PALACE | A_E2_TEMPLE | A_E4_TEST, 0, "TObjHashiVersus2", TObjHashiVersus_params);

  // Multiplayer Temple/Spaceship doors. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.; if this
  //     is negative, the door is always unlocked)
  //   param5 = number of switch flags (should be in range [0, 4] but is not clamped; values above 4 can cause
  //     undefined behavior)
  //   param6 = permanent unlock flag (see TODoorCave01)
  std::vector<PA> TODoorFourLight_params{
      {"first_switch_flag", p4}, {"switch_flag_count", p5}, {"unlocks_permanently", "param6 < 0"}};
  b.add(0x01AB, F_V3_V4, A_E2_TEMPLE | A_E4_TEST, 0, "TODoorFourLightRuins", TODoorFourLight_params);
  b.add(0x01C0, F_V3_V4, A_E2_SPACESHIP, 0, "TODoorFourLightSpace", TODoorFourLight_params);

  // CCA random item box. There are no parameters.
  b.add(0x0200, F_V3_V4, A_E2_CCA_ALL | A_E2_GAL_GRYPHON | A_E2_QUEST_ONLY | A_E4_CRATER | A_E4_TEST, 0, "TObjContainerJung", {});

  // CCA cross-floor warp. Params:
  //   param4 = destination floor
  //   param6 = color (0 = red, 1 = blue); if this is 1 in Challenge mode, the warp is destroyed immediately
  b.add(0x0201, F_V3_V4, A_E2_CCA_ALL | A_E2_SEABED | A_E2_GAL_GRYPHON | A_E2_OLGA_FLOW | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TObjWarpJung", {{"dest_floor", p4}, {"is_red", "param6 == 0"}});

  // CCA door. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.; if this
  //     is negative, the door is always unlocked)
  //   param5 = number of switch flags (clamped to [0, 4])
  b.add(0x0202, F_V3_V4, A_E2_CCA_ALL | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TObjDoorJung", {{"first_switch_flag", p4}, {"switch_flag_count", "clamp(param5, 0, 4)"}});

  // CCA item box. Same parameters as 0x0092 (TObjContainerBase). In the Episode 4 Crater areas, this object constructs
  // 0x0092 (TObjContainerBase) instead.
  b.add(0x0203, F_V3_V4, A_E2_CCA_ALL | A_E2_GAL_GRYPHON | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TObjContainerJungEx", TObjContainerBase_params);
  b.add(0x0203, F_V4, A_E4_CRATER, 0, "TObjContainerBase(0203)", TObjContainerBase_params);

  // CCA main door. This door checks quest flags 0x0046, 0x0047, and 0x0048 and opens when all are enabled and any
  // player is nearby. There are no parameters.
  b.add(0x0204, F_V3_V4, A_E2_CCA | A_E4_TEST, 0, "TODoorJungleMain", {});

  // CCA main door switch. This switch sets one of the quest flags checked by 0x0204 (TODoorJungleMain). Params:
  //   param4 = quest flag index (clamped to [0, 2]; 0 = 0x0046, 1 = 0x0047, 2 = 0x0048)
  b.add(0x0205, F_V3_V4, A_E2_CCA_ALL | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TOKeyJungleMain", {{"quest_flag", "0x0046 + clamp(param4, 0, 2)"}});

  // Jungle breakable rocks. Creates a TObjMapCsn child with dimensions (32, 20, 12) and passes param4 and param5
  // through to it; see TObjMapCsn for descriptions of those parameters. These objects also use param4 for some of
  // their own logic, so beware of its dual meaning! Param behaviors for these objects:
  //   param4 = switch flag number
  std::vector<PA> TORockJungle_params = TObjMapCsn_flags_params;
  TORockJungle_params.emplace_back(PA{"switch_flag", p4});
  // Small rock
  b.add(0x0206, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT | A_E4_TEST, 0, "TORockJungleS01", TORockJungle_params);
  // Small 3-rock wall
  b.add(0x0207, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT | A_E4_TEST, 0, "TORockJungleM01", TORockJungle_params);

  // Jungle large 3-rock wall. Creates a TObjMapCsn child with dimensions (32, 20, 12) and passes param4 and param5
  // through to it; see TObjMapCsn for descriptions of those parameters. There are no other parameters.
  b.add(0x0208, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT | A_E4_TEST, 0, "TORockJungleL01", TObjMapCsn_flags_params);

  // Jungle plant. Params:
  //   param4 = model number (clamped to [0, 1])
  b.add(0x0209, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT | A_E4_TEST, 0, "TOGrassJungle", {{"model_index", "clamp(param4, 0, 1)"}});

  // CCA warp outside main gate. Unlike other warps on Ragol, this one presents the player with a choice of areas:
  // Jungle North (6), Mountain (8), or Seaside (9). Params:
  //   param6 = color (on GC, 0 = red, 1 = blue); if this is 0 in Challenge mode, the warp is destroyed immediately
  b.add(0x020A, F_V3_V4, A_E2_CCA_ALL | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TObjWarpJungMain", {{"is_red", "param6 == 0"}});

  // Background lightning generator. Each strike lasts for 11 frames and strikes at a random angle around the player.
  // Params:
  //   param1 = lightning distance from player
  //   param2 = lightning height
  //   param3 = lightning size scale (each strike uses param3 * (1 + 0.3 * rand(0, 1)) * 64)
  //   param4 = minimum frames between strikes
  //   param5 = interval randomness (after each strike, a random number is chosen between param4 and (param4 + param5)
  //     to determine how many frames to wait until the next strike)
  b.add(0x020B, F_V3_V4, A_E2_CCA | A_E2_GAL_GRYPHON | A_E4_TEST, 0, "TBGLightningCtrl", {{"distance", p1}, {"height", p2}, {"size_scale", "param3 * 64"}, {"min_interval_frames", p4}, {"interval_randomness", p5}});

  // Bird objects. Params:
  //   param4 = movement/path pattern (clamped to [0, 2])
  b.add(0x020C, F_V3_V4, A_E2_JUNGLE_N | A_E2_JUNGLE_E | A_E2_SEASIDE | A_E2_QUEST_ONLY | A_E4_DESERT | A_E4_TEST, 0, "__WHITE_BIRD__", {{"path_pattern", "clamp(param4, 0, 2)"}});
  b.add(0x020D, F_V3_V4, A_E2_JUNGLE_N | A_E2_JUNGLE_E | A_E2_SEASIDE | A_E2_TOWER | A_E4_TEST, 0, "__ORANGE_BIRD__", {{"path_pattern", "clamp(param4, 0, 2)"}});

  // Jungle box that triggers a wave event when opened. Params:
  //   param4 = event number
  //   param5 = model number (clamped to [0, 1])
  b.add(0x020E, F_V3_V4, A_E2_CCA_ALL | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TObjContainerJungEnemy", {{"event_id", p4, "", true}, {"model_index", "clamp(param5, 0, 1)"}});

  // Chain saw damage trap. Params:
  //   param2 = base damage (multiplied by difficulty: Normal = x1/5, Hard = x2/5, Very Hard = x3/5, Ultimate = x6/5)
  //   param3 = model number (<= 0 for small saw, > 0 for large saw)
  //   param4 = switch flag number (disabled when switch flag is enabled)
  //   param5 high word = rotation range (16-bit angle)
  //   param5 low word = rotation speed (angle units per frame)
  //   param6 high word = if nonzero, ignore rotation range and rotate in a full circle instead
  //   param6 low word = delay between cycles in seconds (ignored if high word is nonzero)
  b.add(0x020F, F_V3_V4, A_E2_CCA_ALL | A_E2_SEABED | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TOTrapChainSawDamage", {{"base_damage", "param2 / 5"}, {"use_large_model", "param3 > 0"}, {"disable_switch_flag", p4}, {"rotation_range", "(param5 >> 16) & 0xFFFF"}, {"rotation_speed", "param5 & 0xFFFF"}, {"sweep_full_circle", "(param6 & 0xFFFF0000) != 0"}, {"delay_seconds_between_cycles", "param6 & 0xFFFF"}});

  // Laser detector trap. Params:
  //   param3 = model number (<= 0 for small laser, > 0 for large laser)
  //   param4 = switch flag number (enables this flag when triggered)
  //   param5-6 = same as 0x020F (TOTrapChainSawDamage)
  b.add(0x0210, F_V3_V4, A_E2_CCA_ALL | A_E2_SEABED | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TOTrapChainSawKey", {{"use_large_model", "param3 > 0"}, {"trigger_switch_flag", p4}, {"rotation_range", "(param5 >> 16) & 0xFFFF"}, {"rotation_speed", "param5 & 0xFFFF"}, {"sweep_full_circle", "(param6 & 0xFFFF0000) != 0"}, {"delay_seconds_between_cycles", "param6 & 0xFFFF"}});

  // Jungle flying insect. Same params as TODragonflyCave01.
  b.add(0x0211, F_V3_V4, A_E2_CCA | A_E2_JUNGLE_N | A_E2_JUNGLE_E | A_E4_DESERT | A_E4_TEST, A_E3_BATTLE, "TOBiwaMushi", TODragonflyCave01_params);

  // Seagull. Params:
  //   param4 = movement/path pattern (clamped to [0, 2])
  b.add(0x0212, F_V3_V4, A_E2_CCA_ALL | A_E2_TOWER | A_E4_TEST, A_E3_BATTLE, "__SEAGULL__", {{"path_pattern", "clamp(param4, 0, 2)"}});

  // Jungle wooden objects. Params:
  //   param4 = model number: (<1 = long branch, 1 = shorter curved branch, >1 = small log)
  b.add(0x0213, F_V3_V4, A_E2_JUNGLE_N | A_E2_JUNGLE_E | A_E2_MOUNTAIN | A_E2_SEASIDE | A_E2_SEASIDE_NIGHT | A_E4_DESERT | A_E4_TEST, 0, "TOJungleDesign", {{"model_index", "clamp(param4, 0, 2)"}});

  // School of fish. This object is not constructed in split-screen mode. Params:
  //   param1-3 = school size in each coordinate (x, y, z; each fish chooses its coordinates as (school_pos +
  //     rand(-paramN, +paramN)))
  //   param4 = number of fish using model 0
  //   param5 = number of fish using model 1
  //   param6 = number of fish using model 2
  b.add(0x0220, F_V3_V4, A_LOBBY | A_E2_JUNGLE_N | A_E2_SEASIDE | A_E2_SEABED | A_E2_SEASIDE_NIGHT | A_E4_TEST, A_LOBBY | A_E3_BATTLE, "TObjFish", {{"range_x", p1}, {"range_y", p2}, {"range_z", p3}, {"type0_count", p4}, {"type1_count", p5}, {"type2_count", p6}});

  // Seabed multiplayer doors. Params:
  //   param4 = base switch flag number (the actual switch flags used are param4, param4 + 1, param4 + 2, etc.; if this
  //     is negative, the door is always unlocked)
  //   param5 = number of switch flags (clamped to [0, 4])
  //   param6 = permanent unlock flag (see TODoorCave01)
  std::vector<PA> TODoorFourLightSeabed_params{
      {"first_switch_flag", p4}, {"switch_flag_count", "clamp(param5, 0, 4)"}, {"unlocks_permanently", "param6 < 0"}};
  b.add(0x0221, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "TODoorFourLightSeabed", TODoorFourLightSeabed_params); // Blue edges
  b.add(0x0222, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "TODoorFourLightSeabedU", TODoorFourLightSeabed_params);

  // Small cryotube. Params:
  //   param4 = model number (clamped to [0, 3])
  //   param5 high word = number of consecutive switch flags (only used if param4 = 3)
  //   param5 low word = first switch flag number (clamped to [0, 0x100 - num flags]; only used if param4 = 3)
  b.add(0x0223, F_V3_V4, A_E2_SEABED | A_E2_TOWER | A_E4_TEST, 0, "TObjSeabedSuiso_CH", {{"model_index", "clamp(param4, 0, 3)"}, {"switch_flag_count", "(param5 >> 16) & 0xFFFF", "param4 >= 3"}, {"first_switch_flag", "clamp(param5 & 0xFFFF, 0, 0x100 - ((param5 >> 16) & 0xFFFF))", "param4 >= 3"}});

  // Breakable glass wall. Params:
  //   param4 = switch flag number (negative or >= 0x100 disables breaking via a switch flag)
  //   param5 = model number (clamped to [0, 2])
  b.add(0x0224, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "TObjSeabedSuisoBrakable", {{"switch_flag", p4}, {"model_index", "clamp(param5, 0, 2)"}});

  // Small floating robots. Same params as TODragonflyCave01.
  b.add(0x0225, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "TOMekaFish00", TODragonflyCave01_params); // Blue
  b.add(0x0226, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "TOMekaFish01", TODragonflyCave01_params); // Red

  // Dolphin. Params:
  //   param4 = movement/path pattern (clamped to [0, 4])
  b.add(0x0227, F_V3_V4, A_E2_SEABED | A_E4_TEST, 0, "__DOLPHIN__", {{"path_pattern", "clamp(param4, 0, 4)"}});

  // Seabed capturing trap, similar to 0x0153 (TOTrapAncient01) in function. Triggers when a player is within 15
  // units. Params:
  //   param1 = animation speed (clamped to [0.1, 10])
  //   param2 = base damage power (clamped below to 0, then scaled by difficulty: Normal = 1x, Hard = 2x, Very Hard =
  //     3x, Ultimate = 6x)
  //   param4 = hold time after trigger (in seconds; clamped to [1, 60])
  //   param5 = visibility when inactive (<=0 = visible, >0 = invisible)
  //   param6 = same as param5 from 0x0153 (TOTrapAncient01)
  b.add(0x0228, F_V3_V4, A_E2_CCA_ALL | A_E2_SEABED | A_E2_QUEST_ONLY | A_E4_TEST, 0, "TOTrapSeabed01", {{"animation_speed", "clamp(param1, 0.1, 10)"}, {"base_damage", "max(0, param2)"}, {"hold_seconds_after_trigger", "clamp(param4, 1, 60)"}, {"visible_when_inactive", "param5 <= 0"}, {"damage_type", "clamp(param6, 0, 5)"}});

  // VR link object. This object is destroyed immediately in Challenge mode and split-screen mode. Same parameters as
  // 0x008D (TOCapsuleAncient01).
  b.add(0x0229, F_V3_V4, A_E2_NON_CITY | A_E4_TEST, 0, "TOCapsuleLabo", TOCapsuleAncient01_params);

  // Alias for 0x0001 (TObjParticle). The constructor function is exactly the same as for 0x0001, so this object has
  // all the same parameters and behavior as that object.
  b.add(0x0240, F_V3_V4, A_E2_GAL_GRYPHON | A_E4_TEST, 0, "TObjParticle", TObjParticle_params);

  // Teleporter after Barba Ray. This object behaves exactly the same as 0x0002 (TObjAreaWarpForest), except it's
  // invisible until the platform has docked after the boss is defeated.
  b.add(0x0280, F_V3_V4, A_E2_BARBA_RAY | A_E4_TEST, 0, "__BARBA_RAY_TELEPORTER__", TObjAreaWarpForest_params);

  // This appears to be intended to control where the video on the screen in the Gol Dragon arena can be rendered from
  // (or perhaps what its camera can can look at), but the actual renderer seems not to use these objects. There are no
  // parameters.
  b.add(0x02A0, F_V3_V4, A_E2_GOL_DRAGON | A_E4_TEST, 0, "TObjLiveCamera", {});

  // Gee nest. This object is technically an item box, and drops an item when destroyed. It cannot be specialized
  // (ignore_def is always true). Params are the same as for 0x0152 (TContainerAncient01), but there is also:
  //   angle.z = number of hits required to destroy
  b.add(0x02B0, F_V3_V4, A_E1_RUINS | A_E2_CCA_ALL | A_E2_QUEST_ONLY | A_E4_DESERT | A_E4_TEST, 0, "TContainerAncient01R", {{"max_idle_frames", "param1 + 299"}, {"poison_duration_frames", "param2 + 209"}, {"poison_radius", "sqrt(param3 + 399)"}, {"frames_between_poison_particles", "param6 + 4"}, {"hits_required", az}});

  // Lab objects. Variants 3 and 4 (walls) also create a TObjMapCsn with fixed dimensions (40, 20, 10) at the same
  // position, angle, and params. Those variants therefore inherit param4 and param5's meanings from TObjMapCsn; other
  // variants have no parameters.
  // Computer console
  b.add(0x02B1, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(0)", {});
  // Computer console (alternate colors)
  b.add(0x02B2, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(1)", {});
  // Chair
  b.add(0x02B3, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(2)", {});
  // Orange wall
  b.add(0x02B4, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(3)", TObjMapCsn_flags_params);
  // Gray/blue wall
  b.add(0x02B5, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(4)", TObjMapCsn_flags_params);
  // Long table
  b.add(0x02B6, F_V3_V4, A_E2_LAB | A_E4_TEST, A_E3_MORGUE, "TObjLaboDesignBase(5)", {});

  // Game Boy Advance. Functions similarly to the other message pod objects (e.g. TOCapsuleAncient01); actual GBA
  // interactions are implemented via quest opcodes. Params:
  //   param4 = quest label to call when activated (inherited from TObjMesBase)
  //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO") (inherited from TObjMesBase)
  b.add(0x02B7, F_GC, A_E1_P2 | A_E2_LAB, 0, "TObjGbAdvance", {{"quest_label", p4}, {"type", "clamp(param6, 0, 1)"}});

  // Like TObjQuestColA, but is targetable (shows a reticle). Params are the same as for TObjQuestColA, but also:
  //   param2 = hitbox (reticle) vertical offset
  //   param5 = quest script manager to use (zero or negative = quest, positive = free play)
  std::vector<PA> TObjQuestColALock2_params = TObjQuestCol_params;
  TObjQuestColALock2_params.emplace_back(PA{"reticle_vert_offset", p2});
  TObjQuestColALock2_params.emplace_back(PA{"use_free_play_script", "param5 > 0"});
  b.add(0x02B8, F_V3_V4, A_ALL & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS), A_E3_MORGUE, "TObjQuestColALock2", TObjQuestColALock2_params);

  // Like 0x0003 (TObjMapWarpForest), but is invisible and automatically warps players when they enter its radius. This
  // is used to simulate floor warps in the Control Tower. Has the same params as TObjMapWarpForest, but also:
  //   param5 = destination "floor" number (this is an intra-map warp and doesn't actually change floors; the "floor"
  //     number is only visual)
  //   param6 = if <= 0, shows "floor" number (param5 formatted as "%xF") after warp; if > 0, param5 is ignored
  std::vector<PA> TObjMapForceWarp_params = TObjMapWarpForest_params;
  TObjMapForceWarp_params.emplace_back(PA{"dest_floor_number", p5});
  TObjMapForceWarp_params.emplace_back(PA{"show_floor_number", "param6 <= 0"});
  b.add(0x02B9, F_V3_V4, A_ALL & (~A_E1_RUINS) & (~A_LOBBY) & (~A_E2_BOSS), A_E3_MORGUE, "TObjMapForceWarp", TObjMapForceWarp_params);

  // Behaves like 0x0012 (TObjQuestCol), but can also use the free-play script, like 0x02B8 (TObjQuestColALock2).
  std::vector<PA> TObjQuestCol2_params = TObjQuestCol_params;
  TObjQuestCol2_params.emplace_back(PA{"use_free_play_script", "param5 > 0"});
  b.add(0x02BA, F_V3_V4, A_ALL & (~A_LOBBY) & (~A_E1_QUEST_ONLY) & (~A_E4_BOSS), A_E3_MORGUE, "TObjQuestCol2", TObjQuestCol2_params);

  // Episode 2 Lab door with glass window. Same parameters as TODoorLabo, except param5 is unused.
  b.add(0x02BB, F_V3_V4, A_E2_LAB | A_E4_TEST, 0, "TODoorLaboNormal", {{"switch_flag", p4}, {"unlocks_permanently", "param6 < 0"}});

  // Episode 2 ending movie warp (used in final boss arenas after the boss is defeated). Same params as 0x001C
  // (TObjAreaWarpEnding), except the colors are reversed (param6: 0 = red, 1 = blue).
  b.add(0x02BC, F_V3_V4, A_E2_OLGA_FLOW | A_E4_TEST, 0, "TObjAreaWarpEndingJung", {{"is_red", "param6 == 0"}});

  // Warp to another location on the same map. Used for the Lab/City warp. This warp is visible in all game modes, but
  // cannot be used in Episode 1 Battle or Challenge modes. Params:
  //   param1-3 = destination (same as for TObjMapWarpForest)
  //   param4 = destination angle (same as for TObjMapWarpForest)
  //   param5 = model type (0 = render normally; nonzero = render warp lines only)
  //   param6 = destination text (clamped to [0, 2]):
  //     00 = "The Principal"
  //     01 = "Pioneer 2"
  //     02 = "Lab"
  std::vector<PA> TObjLaboMapWarp_params = TObjMapWarpForest_params;
  TObjLaboMapWarp_params.emplace_back(PA{"base_visible", "param5 == 0"});
  TObjLaboMapWarp_params.emplace_back(PA{"dest_text", "clamp(param6, 0, 2)"});
  b.add(0x02BD, F_V3_V4, A_E2_LAB | A_E4_TEST, 0, "TObjLaboMapWarp", TObjLaboMapWarp_params);

  // This object is used internally by Episode 3 during battles as the visual implementation for some overlay tiles.
  // Params:
  //   param4 = model number:
  //     00 = TODO (iwa.bml)
  //     01 = TODO (l_kusa.bml)
  //     02 = TODO (s_kusa.bml)
  //     03 = TODO (crash_car.bml)
  //     04 = TODO (crash_warp.bml)
  //     05 = TODO (new_marker.bml)
  //     06 = TODO (r_guard.bml)
  //     07 = TODO (ryuuboku.bml)
  //     08 = TODO (ryuuboku_l.bml)
  //     09 = Rock (overlay = 0x10; new_iwa_1.bml)
  //     0A = TODO (new_iwa_2.bml)
  //     0B = Purple warp (overlay = 0x30; warp_p.bml)
  //     0C = Red warp (overlay = 0x31; warp_r.bml)
  //     0D = Green warp (overlay = 0x32; warp_g.bml)
  //     0E = Blue warp (overlay = 0x33; warp_b.bml)
  //     0F = Fence (overlay = 0x20; shareobj_new_guard.bml)
  //   param5 = if low bit is set, model is hidden when camera is too close (TODO: exactly how close?)
  //   param6 second-low byte = u texture coordinate scroll speed (signed)
  //   param6 low byte = v texture coordinate scroll speed (signed)
  b.add(0x02D0, F_EP3, A_E3_BATTLE, 0, "TObjKazariCard", {{"model_index", p4}, {"hide_when_near_camera", "param5 & 1"}, {"tex_scroll_speed_u", "sign_extend((param6 >> 8) & 0xFF, 8)"}, {"tex_scroll_speed_v", "sign_extend(param6 & 0xFF, 8)"}});

  // Effects visible in the central column in the Morgue when cards transform. There appear to be no parameters.
  b.add(0x02D1, F_EP3, A_E3_MORGUE, 0, "TObj_FloatingCardMaterial_Dark", {});
  b.add(0x02D2, F_EP3, A_E3_MORGUE, 0, "TObj_FloatingCardMaterial_Hero", {});

  // Morgue warps. These don't actually do anything; they just look like a warp. The actual warping is done by another
  // object (TObjCityAreaWarp for the lobby teleporter, or TShopGenerator for the battle counter). These take no
  // parameters.
  b.add(0x02D3, F_EP3, A_E3_MORGUE, 0, "TObjCardCityMapWarp(0)", {}); // Battle counter warp (blue lines)
  b.add(0x02D9, F_EP3, A_E3_MORGUE, 0, "TObjCardCityMapWarp(1)", {}); // Battle counter warp (green lines; unused)
  b.add(0x02E3, F_EP3, A_E3_MORGUE, 0, "TObjCardCityMapWarp(2)", {}); // Lobby warp (yellow lines)

  // Morgue doors. Unsurprisingly, the closed variants don't open. The closed variants also have a red light in the
  // center instead of a blue light. Curiously, the (3) variant is opaque when closed, unlike the other closed doors.
  // None of these take any parameters.
  b.add(0x02D4, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor(0)", {}); // Yellow V-pattern (to deck edit room)
  b.add(0x02D5, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor(1)", {}); // Blue V-pattern (to battle entry counter)
  b.add(0x02D8, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor(2)", {}); // Green V-pattern (unused)
  b.add(0x02DF, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor(3)", {}); // Blue X-pattern (to lobby teleporter)
  b.add(0x02E0, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor(4)", {}); // Gray (to chief)
  b.add(0x02DC, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor_Closed(0)", {}); // Yellow V-pattern (to deck edit room)
  b.add(0x02DD, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor_Closed(1)", {}); // Blue V-pattern (to battle entry counter)
  b.add(0x02DE, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor_Closed(2)", {}); // Green V-pattern (unused)
  b.add(0x02E1, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor_Closed(3)", {}); // Opaque gray X-pattern
  b.add(0x02E2, F_EP3, A_E3_MORGUE, 0, "TObjCardCityDoor_Closed(4)", {}); // Gray (to chief)

  // Mortis Fons geyser. Params:
  //   param2 = timing offset
  //   param4 = mode (value is param4 % 0x0B):
  //     00 = MIZUMODE_NONE
  //     01 = MIZUMODE_LOW
  //     02 = MIZUMODE_MIDDLE
  //     03 = MIZUMODE_HIGH
  //     04 = MIZUMODE_RAIN
  //     05 = MIZUMODE_WAVE
  //     06 = MIZUMODE_HIGHTYNY
  //     07 = MIZUMODE_REFLECT
  //     08 = MIZUMODE_FOG
  //     09 = MIZUMODE_FOG2
  //     0A = MIZUMODE_LIGHT
  //   param5 = group (value is param5 % 7)
  b.add(0x02D6, F_EP3, A_E3_BATTLE, 0, "TObjKazariGeyserMizu", {{"timing_offset", p2}, {"mode", "param4 % 11"}, {"group", "param5 % 7"}});

  // Lens-flare occlusion. SCs and creatures create these so their bodies obscure lens flares on stages where it's
  // present. Params:
  //   param1 = radius
  //   param2 = half-length of center cylindrical section (ignored if param4 is 1)
  //   param4 = shape (0 = cylinder with conic ends; 1 = sphere)
  b.add(0x02D7, F_EP3, A_E3_BATTLE, 0, "TObjCardSetColi", {{"radius", p1}, {"cylinder_length", "param2 * 2", "param4 != 1"}, {"shape", p4}});

  // Floating robots, presumably. These are both subclasses of 0x0106 (TODragonflyMachine01) and take all the same
  // params as that object.
  b.add(0x02DA, F_EP3, A_E3_MORGUE, 0, "TOFlyMekaHero", TODragonflyCave01_params);
  b.add(0x02DB, F_EP3, A_E3_MORGUE, 0, "TOFlyMekaDark", TODragonflyCave01_params);

  // Lobby banner or model display object. This implements display of media sent with the B9 command. Params:
  //   param1-3 = scale factors (x, y, z)
  //   param4 = index into location bit field (0-31 where 0 is the least-significant bit; see Episode3LobbyBanners in
  //     config.example.json for the bits' meanings)
  //   param5 = per-axis mirror flags (the low 3 nybbles of this value specify whether to invert each axis of the
  //     model; if any nybble is 1 then the model is inverted along the corresponding axis; the lowest nybble
  //     corresponds to the x axis)
  b.add(0x02E4, F_EP3, A_E3_MORGUE | A_LOBBY, 0, "TObjSinBoardCard", {{"scale_x", p1}, {"scale_y", p2}, {"scale_z", p3}, {"location_bit", p4}, {"mirror_x", "(param5 & 0xF) == 1"}, {"mirror_y", "(param5 & 0xF0) == 0x10"}, {"mirror_z", "(param5 & 0xF00) == 0x100"}});

  // Morgue info screen. Params:
  //   param4 = model number:
  //     0 = tall, yellow, vertical-scrolling
  //     1 = short, blue, horizontal-scrolling
  b.add(0x02E5, F_EP3, A_E3_MORGUE, 0, "TObjCityMoji", {{"model_index", p4}});

  // Like TObjCardCityMapWarp(2) (the warp to the lobby from the Morgue) but doesn't render the circles. Used in
  // offline mode where that warp is disabled. There are no parameters.
  b.add(0x02E6, F_EP3, A_E3_MORGUE, 0, "TObjCityWarpOff", {});

  // Small flying robot. There appear to be no parameters.
  b.add(0x02E7, F_EP3, A_E3_MORGUE, 0, "TObjFlyCom", {});

  // Flying-ship scenery around the Morgue. Params:
  //   param4 = model number (0-3)
  b.add(0x02E8, F_EP3, A_E3_MORGUE, 0, "__MORGUE_SHIP__", {{"model_index", p4}});

  // Episode 4 particle emitter. Params:
  //   param1 = particle type
  //   param2 = size scale
  //   param3 = emission rate scale
  //   param4 low byte = visibility radius (1 = 1500, 2 = 10000, 3 = 200, 4 = 100, anything else = 400)
  //   param4 next 12 bits = particle speed scale (4.8 fixed-point; 0 means normal speed)
  //   param4 high 12 bits = particle animation speed scale (4.8 fixed-point; 0 means normal speed)
  //   param5 low word = on/off cycle length in frames
  //   param5 high word = number of frames to emit particles for (from start of cycle)
  //   param6 = color override (ARGB8888)
  b.add(0x0300, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_BOSS | A_E4_TEST, 0, "__EP4_PARTICLE__", {{"type", p1}, {"size_scale", p2}, {"emission_rate_scale", p3}, {"visibility_radius_type", "param4 & 0xFF"}, {"particle_speed_scale", "(param4 & 0xFFF00) ? (((param4 >> 8) & 0xFFF) / 256.0) : 1.0"}, {"particle_animation_speed_scale", "(param4 & 0xFFF00000) ? (((param4 >> 20) & 0xFFF) / 256.0) : 1.0"}, {"cycle_length_frames", "param5 & 0xFFFF"}, {"emit_duration_frames", "(param5 >> 16) & 0xFFFF"}, {"color_argb", p6, "", true}});

  // Wilds/Crater cactus. Params:
  //   param1 = horizontal scale (x, z)
  //   param2 = vertical scale (y)
  //   param4 = model number (0-2, not bounds-checked)
  //   param5 = damage scale (damage = max(0, param5 + 10) * difficulty factor / 5); difficulty factor is 1 in Normal,
  //     2 in Hard, 3 in Very Hard, and 6 in Ultimate)
  b.add(0x0301, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__WILDS_CRATER_CACTUS__", {{"horiz_scale", p1}, {"vert_scale", p2}, {"model_index", p4}, {"base_damage", "max(0, param5 + 10) / 5.0"}});

  // Wilds/Crater brown rock. Params:
  //   param1-3 = scale factors (x, y, z); z factor also scales hitbox size
  //   param4 = model number (0-2, not bounds-checked)
  b.add(0x0302, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__WILDS_CRATER_BROWN_ROCK__", {{"scale_x", p1}, {"scale_y", p2}, {"scale_z", p3}, {"model_index", p4}});

  // Wilds/Crater destructible brown rock. Params:
  //   param4 = switch flag number
  b.add(0x0303, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__WILDS_CRATER_BROWN_ROCK_DESTRUCTIBLE__", {{"switch_flag", p4}});

  // Moving platform, presumably used in testing. This object uses command 6xD4 to synchronize its state across
  // clients. Params:
  //   param4 = group ID (must be in range [0, 15]; used in 6xD4 command)
  b.add(0x0340, F_V4, A_E4_TEST, 0, "__TEST_MOVING_PLATFORM__", {{"group_id", p4}});

  // Trigger object for the above object. Params:
  //   param4 = group ID (must be in range [0, 15]; should match an existing 0340 object's identifier)
  //   param5 = trigger ID (must be in range [0, 3])
  b.add(0x0341, F_V4, A_E4_TEST, 0, "__TEST_MOVING_PLATFORM_TRIGGER__", {{"group_id", p4}, {"trigger_id", p5}});

  // Poison plant. Base damage is 10 (Normal), 20 (Hard), 30 (Very Hard), or 60 (Ultimate). There appear to be no
  // parameters.
  b.add(0x0380, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__POISON_PLANT__", {});

  // TODO: Describe this object. Params:
  //   param4 = model number (clamped to [0, 1])
  b.add(0x0381, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__UNKNOWN_0381__", {{"model_index", "clamp(param4, 0, 1)"}});

  // TODO: Describe this object. There appear to be no parameters.
  b.add(0x0382, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__UNKNOWN_0382__", {});

  // Desert ooze plant. Params:
  //   param1 = animation speed
  //   param2 = scale factor
  //   param4 = model number (clamped to [0, 1])
  b.add(0x0383, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__DESERT_OOZE_PLANT__", {{"animation_speed", p1}, {"scale", p2}, {"model_index", "clamp(param4, 0, 1)"}});

  // Desert mushroom. Params:
  //   param1 = animation speed
  //   param4 = model number (clamped to [0, 1])
  b.add(0x0385, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__DESERT_MUSHROOM__", {{"animation_speed", p1}, {"model_index", "clamp(param4, 0, 1)"}});

  // Wilds/Crater black rocks. Params:
  //   param4 = model number (0-2, not bounds-checked)
  b.add(0x0386, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__WILDS_CRATER_BLACK_ROCKS__", {{"model_index", p4}});

  // Desert quicksand trap. Within the pull radius, the player is pulled toward the center; when they enter the hole,
  // they take damage and are relocated to the destination coordinates. Params:
  //   angle.x = dest x (value is angle.x / 100)
  //   angle.z = dest z (value is angle.z / 100)
  //   param1 = area radius
  //   param2 = area power (value is param2 * 0.8)
  //   param4 = hole radius (value is param4 / 100)
  //   param5 = hole power (value is param5 / 100)
  b.add(0x0387, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__UNKNOWN_0387__", {{"dest_x", "angle.x / 100.0"}, {"dest_z", "angle.z / 100.0"}, {"pull_radius", p1}, {"pull_power", "param2 * 0.8"}, {"hole_radius", "param4 / 100.0"}, {"hole_power", "param5 / 100.0"}});

  // Push region. When players are within this object's hitbox, it pushes them in the specified direction. Params:
  //   angle.y = push direction
  //   param1 = hitbox width (x; only used if param6 == 0)
  //   param2 = hitbox radius (only used if param6 != 0)
  //   param3 = hitbox depth (z; only used if param6 == 0)
  //   param4 = push power (value is param4 / 100)
  //   param6 = hitbox type (0 = rectangular, anything else = cylindrical)
  std::vector<PA> ep4_zone_object_hitbox_params{
      {"hitbox_width", "param1 * 2", "param6 == 0"},
      {"hitbox_depth", "param3 * 2", "param6 == 0"},
      {"hitbox_radius", p2, "param6 != 0"},
      {"hitbox_is_cylinder", "param6 != 0"}};
  std::vector<PA> __PUSH_REGION___params = ep4_zone_object_hitbox_params;
  __PUSH_REGION___params.emplace_back(PA{"push_direction", ay});
  __PUSH_REGION___params.emplace_back(PA{"push_power", "param4 / 100.0"});
  b.add(0x0388, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__PUSH_REGION__", __PUSH_REGION___params);

  // Game flag set/clear zone. This sets and clears any of the low 8 bits in game_flags (the flags sent in 6x0A) when
  // the player enters the object's hitbox. These bits correspond to various status ailments; see the description of
  // 6x0A in CommandFormats.hh for details on what the flags mean. Params:
  //   param1-3 = same as for 0x0388
  //   param4 = game flags to set (low 8 bits only)
  //   param5 = game flags to clear (low 8 bits only)
  //   param6 = same as for 0x0388
  std::vector<PA> __GAME_FLAG_SET_CLEAR_ZONE___params = ep4_zone_object_hitbox_params;
  __GAME_FLAG_SET_CLEAR_ZONE___params.emplace_back(PA{"flags_to_set", "param4 & 0xFF", "", true});
  __GAME_FLAG_SET_CLEAR_ZONE___params.emplace_back(PA{"flags_to_clear", "param5 & 0xFF", "", true});
  b.add(0x0389, F_V4, A_E4_TEST, 0, "__GAME_FLAG_SET_CLEAR_ZONE__", __GAME_FLAG_SET_CLEAR_ZONE___params);

  // HP drain zone. When a player is within this object's hitbox, it subtracts 0.66% of the player's current HP at a
  // regular interval. The amount of damage per interval is capped below at 1 HP, so it will always do a nonzero amount
  // of damage each time. Params:
  //   param1-3 = same as for 0x0388
  //   param5 = interval (in frames) between damage applications
  //   param6 = same as for 0x0388
  std::vector<PA> __HP_DRAIN_ZONE___params = ep4_zone_object_hitbox_params;
  __HP_DRAIN_ZONE___params.emplace_back(PA{"damage_interval_frames", p5});
  b.add(0x038A, F_V4, A_E4_TEST, 0, "__HP_DRAIN_ZONE__", __HP_DRAIN_ZONE___params);

  // Falling stalactite. Activates when any player is within 50 units. Base damage is 100 on Normal, 200 on Hard, 300
  // on Very Hard, or 600 on Ultimate. There appear to be no parameters.
  b.add(0x038B, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__FALLING_STALACTITE__", {});

  // Solid desert plant. Params:
  //   param1 = horizontal scale factor (x, z)
  //   param2 = vertical scale factor (y)
  b.add(0x038C, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__DESERT_PLANT_SOLID__", {{"horiz_scale", p1}, {"vert_scale", p2}});

  // Desert crystals-style box. Params:
  //   param1 = contents type:
  //     0 = always empty
  //     1 = standard random item (ignore_def = 1)
  //     2 = specialized item (ignore_def = 0)
  //     3 = trigger set event
  // If param1 is 0 or 1, no other parameters are used. (In the case of 1, param3-6 are sent to the server in the 6xA2
  // command; however, the standard implementation ignores them.)
  // If param1 is 2, the other parameters have the same meanings as for 0x0092 (TObjContainerBase).
  // If param1 is 3, the event number is specified in param4.
  std::vector<PA> __DESERT_CRYSTALS_BOX___params{
      {"type", p1},
      {"use_item_type_only", "param3 == 0", "param1 == 2"},
      {"item_def1", p4, "param1 == 2", true},
      {"item_def2", p5, "param1 == 2", true},
      {"item_def3", p6, "param1 == 2", true},
      {"event_id", p4, "param1 == 3", true}};
  b.add(0x038D, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__DESERT_CRYSTALS_BOX__", __DESERT_CRYSTALS_BOX___params);

  // Episode 4 test door. param4 and param6 are the same as for 0x0056 (TODoorLabo).
  b.add(0x038E, F_V4, A_E4_TEST, 0, "__EP4_TEST_DOOR__", {{"switch_flag", p4}, {"unlocks_permanently", "param6 < 0"}});

  // Beehive. Params:
  //   param1 = horizontal scale factor (x, z)
  //   param2 = vertical scale factor (y)
  //   param4 = model number (clamped to [0, 1])
  b.add(0x038F, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__BEEHIVE__", {{"horiz_scale", p1}, {"vert_scale", p2}, {"model_index", "clamp(param4, 0, 1)"}});

  // Fireball launcher. The fireballs do 100 damage on Normal, 200 on Hard, 300 on Very Hard, or 600 on Ultimate.
  // Params:
  //   angle.x = launch pitch
  //   angle.y = launch yaw
  //   param1 = initial speed
  //   param2 = deletion height delta (relative to launcer's height; a negative value here allows fireballs to fall
  //     below the laucher's height)
  //   param4 = frames between launches
  b.add(0x0390, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__EP4_TEST_PARTICLE__", {{"launch_pitch", ax}, {"launch_yaw", ay}, {"initial_speed", p1}, {"deletion_height_delta", p2}, {"launch_interval_frames", p4}});

  // Heat (implemented as a type of poison fog). Same parameters as TObjFogCollisionPoison.
  b.add(0x0391, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__HEAT__", TObjFogCollisionPoison_params);

  // Episode 4 boss egg. There appear to be no parameters.
  b.add(0x03C0, F_V4, A_E4_BOSS | A_E4_TEST, 0, "__EP4_BOSS_EGG__", {});

  // Episode 4 boss rock spawner. Params:
  //   param4 = type (clamped to [0, 2])
  b.add(0x03C1, F_V4, A_E4_BOSS | A_E4_TEST, 0, "__EP4_BOSS_ROCK_SPAWNER__", {{"type", "clamp(param4, 0, 2)"}});

  return std::move(b.ret);
}

static const std::unordered_multimap<uint16_t, DATEntityDefinition>& default_object_definitions() {
  static const auto ret = create_default_dat_object_definitions();
  return ret;
}

static std::unordered_multimap<uint16_t, DATEntityDefinition> create_default_dat_enemy_definitions() {
  using PA = DATEntityDefinitionsBuilder::ParamDefArgs;

  DATEntityDefinitionsBuilder b;
  static constexpr std::string ax = "angle.x";
  static constexpr std::string ay = "angle.y";
  static constexpr std::string az = "angle.z";
  static constexpr std::string p1 = "param1";
  static constexpr std::string p2 = "param2";
  static constexpr std::string p3 = "param3";
  static constexpr std::string p4 = "param4";
  static constexpr std::string p5 = "param5";
  static constexpr std::string p6 = "param6";
  static constexpr std::string p7 = "param7";

  // This is newserv's canonical definition of map enemy and NPC types.

  // Enemies and NPCs take a similar arguments structure as objects: objects use ObjectSetEntry, enemies use
  // EnemySetEntry. Like objects, some IDs are reused across game versions or areas, so the same enemy type can
  // generate a completely different entity on different game versions. This is why some enemies have multiple entries
  // with the same type and different names.

  // Some enemies have params that the game's code references, but only in places where their effects can't be seen
  // (for example, in normally-unused debug menus). These may have been used to test frame-by-frame animations for some
  // enemies; see TObjEneMe3Shinowa_v76 for an example of this usage. The enemies with params like this are:
  // - TObjEneMe3ShinowaReal (param3, param4)
  // - TObjEneDf2Bringer (param1, param2)
  // - TObjEneRe7Berura (param1, param2)
  // - TBoss1Dragon (param1, param2)
  // - TBoss5Gryphon (param1, param2)
  // - TBoss2DeRolLe (param1, param2, param3)
  // - TBoss8Dragon (param1, param2)
  // - TObjEneBm5GibonU (param4, param5; these params also have non-debug meanings)
  // - TObjEneMorfos (param1, param2; these params also have non-debug meanings)

  // NPCs. Params:
  //   param1 = action parameter (depends on param6; see below)
  //   param2 = visibility register number (if this is > 0, the NPC will only be visible when this register is nonzero;
  //     if this is >= 1000, the effective register is param2 - 1000 and register values for both param2 and param3 are
  //     read from the free play script environment instead of the quest script environment)
  //   param3 = hide override register number (if this is > 0, the NPC will not be visible when this register is
  //     nonzero, regardless of the state of the register specified by param2; if this is >= 1000, the effective
  //     register is param3 - 1000 and register values for both param2 and param3 are read from the free play script
  //     environment instead of the quest script environment)
  //   param4 = object number ("character ID" in qedit); if this is in [100, 999], param5 is called in the quest
  //     script; if this is >999, param5 is called in the free play script; if this is <100, no quest label is called
  //     (param5 is ignored); if this is <1, the draw distance is reduced from 400 to 250
  //   param5 = quest label to call when interacted with (if <= 0, NPC does nothing upon interaction)
  //   param6 = specifies what NPC does when idle:
  //     0 = stand still (param1 is ignored)
  //     1 = walk around randomly (param1 = max walk distance from home)
  //     2 = TODO (Ep3 only; appears to be unused)
  //     3 = TODO (Ep3 only; appears to be unused)
  std::vector<PA> base_npc_params{
      {"visibility_reg", "(param2 >= 1000) ? (param2 - 1000) : param2"},
      {"hide_override_reg", "(param3 >= 1000) ? (param3 - 1000) : param3"},
      {"use_free_play_script_for_regs", "(param2 >= 1000) || (param3 >= 1000)"},
      {"object_number", p4},
      {"use_free_play_script_for_label", "param4 > 999"},
      {"label_number", p5}};
  std::vector<PA> npc_params = base_npc_params;
  npc_params.emplace_back(PA{"idle_action", p6});
  npc_params.emplace_back(PA{"max_walk_distance", p1, "param6 == 1"});
  b.add(0x0001, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcFemaleBase", npc_params); // Woman with red hair and purple outfit
  b.add(0x0002, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcFemaleChild", npc_params); // Shorter version of the above
  b.add(0x0003, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcFemaleDwarf", npc_params); // Woman wearing green outfit
  b.add(0x0004, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcFemaleFat", npc_params); // Woman outside Hunter's Guild
  b.add(0x0005, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcFemaleMacho", npc_params); // Tool shop woman
  b.add(0x0006, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcFemaleOld", npc_params); // Older woman with yellow/red outfit
  b.add(0x0007, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcFemaleTall", npc_params); // Woman walking around inside shop area
  b.add(0x0008, F_V0_V4, A_E1_P2 | A_LOBBY | A_E4_P2, A_E3_MORGUE | A_LOBBY, "TObjNpcMaleBase", npc_params); // Similar appearance to weapon shop man
  b.add(0x0009, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcMaleChild", npc_params); // Kid wearing purple
  b.add(0x000A, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcMaleDwarf", npc_params); // Man outside Medical Center
  b.add(0x000B, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcMaleFat", npc_params); // Armor shop man
  b.add(0x000C, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcMaleMacho", npc_params); // Weapon shop man
  b.add(0x000D, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcMaleOld", npc_params); // Man near telepipe locations
  b.add(0x000E, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcMaleTall", npc_params); // Man wearing turquoise
  b.add(0x0019, F_V0_V4, A_E4_NON_CITY | A_CITY, A_E3_MORGUE, "TObjNpcSoldierBase", npc_params); // Man right of the Ragol warp door
  b.add(0x001A, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcSoldierMacho", npc_params); // Man left of the Ragol warp door
  b.add(0x001B, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcGovernorBase", npc_params); // Principal Tyrell
  b.add(0x001C, F_V0_V4, A_CITY, 0, "TObjNpcConnoisseur", npc_params); // Tekker
  b.add(0x001D, F_V0_V4, A_E1_CAVE_3 | A_CITY, 0, "TObjNpcCloakroomBase", npc_params); // Bank woman
  b.add(0x001E, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcExpertBase", npc_params); // Man in front of bank
  b.add(0x001F, F_V0_V4, A_CITY, 0, "TObjNpcNurseBase", npc_params); // Nurses in Medical Center
  b.add(0x0020, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcSecretaryBase", npc_params); // Irene
  b.add(0x0021, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcHHM00", npc_params); // TODO
  b.add(0x0022, F_V0_V4, A_E1_P2 | A_E4_P2, A_E3_MORGUE, "TObjNpcNHW00", npc_params); // TODO
  b.add(0x0024, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcHRM00", npc_params); // TODO
  b.add(0x0025, F_V0_V4, A_CITY, 0, "TObjNpcARM00", npc_params); // TODO
  b.add(0x0026, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcARW00", npc_params); // TODO
  b.add(0x0027, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcHFW00", npc_params); // TODO
  b.add(0x0028, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcNFM00", npc_params); // TODO
  b.add(0x0029, F_V0_V4, A_E1_P2 | A_E4_DESERT_2 | A_E4_DESERT_3 | A_E4_BOSS | A_E4_P2, A_E3_MORGUE, "TObjNpcNFW00", npc_params); // TODO
  b.add(0x002B, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcNHW01", npc_params); // TODO
  b.add(0x002C, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcAHM01", npc_params); // TODO
  b.add(0x002D, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcHRM01", npc_params); // TODO
  b.add(0x0030, F_V0_V4, A_E1_P2 | A_E4_P2, 0, "TObjNpcHFW01", npc_params); // TODO
  b.add(0x0031, F_V0_V4, A_CITY, A_E3_MORGUE, "TObjNpcNFM01", npc_params); // TODO
  b.add(0x0032, F_V0_V4, A_E1_P2 | A_E4_DESERT_2 | A_E4_DESERT_3 | A_E4_P2, 0, "TObjNpcNFW01", npc_params); // TODO
  b.add(0x0045, F_V0_V4, A_E1_FOREST | A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT | A_E4_NON_BOSS, 0, "TObjNpcLappy", npc_params); // Rappy NPC
  b.add(0x0046, F_V0_V4, A_E1_FOREST_2, 0, "TObjNpcMoja", npc_params); // Small Hildebear NPC
  b.add(0x0047, F_V2, A_E1_FOREST_2, 0, "TObjNpcRico", npc_params); // Rico
  b.add(0x00A9, F_V0_V4, A_E1_RUINS_2 | A_E1_RUINS_3, 0, "TObjNpcBringer", npc_params); // Dark Bringer NPC
  b.add(0x00D0, F_V3_V4, A_CITY, 0, "TObjNpcKenkyu", npc_params); // Ep2 armor shop man
  b.add(0x00D1, F_V3_V4, A_CITY, 0, "TObjNpcSoutokufu", npc_params); // Natasha Milarose
  b.add(0x00D2, F_V3_V4, A_E2_LAB, 0, "TObjNpcHosa", npc_params); // Dan
  b.add(0x00D3, F_V3_V4, A_E2_LAB | A_E4_CRATER_E | A_E4_CRATER_W | A_E4_CRATER_S | A_E4_CRATER_N, 0, "TObjNpcKenkyuW", npc_params); // Ep2 tool shop woman
  b.add(0x00D6, F_EP3, A_E3_MORGUE, 0, "TObjNpcHeroGovernor", npc_params); // Morgue Chief
  b.add(0x00D7, F_EP3, A_E3_MORGUE, 0, "TObjNpcHeroGovernor", npc_params); // Morgue Chief (direct alias of 00D6)
  b.add(0x00F0, F_V3_V4, A_E2_LAB, 0, "TObjNpcHosa2", npc_params); // Man next to room with warp to Lab
  b.add(0x00F1, F_V3_V4, A_E2_LAB, 0, "TObjNpcKenkyu2", npc_params); // Ep2 weapon shop man
  b.add(0x00F2, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00F2)", npc_params); // TODO
  b.add(0x00F3, F_V3_V4, A_E2_LAB | A_E4_NON_CITY | A_E4_P2, 0, "TObjNpcNgcBase(0x00F3)", npc_params); // TODO
  b.add(0x00F4, F_V3_V4, A_E2_LAB | A_E2_SEABED | A_E4_NON_CITY | A_E4_P2, 0, "TObjNpcNgcBase(0x00F4)", npc_params); // TODO
  b.add(0x00F5, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00F5)", npc_params); // TODO
  b.add(0x00F6, F_V3_V4, A_E2_LAB | A_E2_CCA_ALL | A_E2_TOWER, 0, "TObjNpcNgcBase(0x00F6)", npc_params); // TODO
  b.add(0x00F7, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00F7)", npc_params); // Nol
  b.add(0x00F8, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00F8)", npc_params); // Elly
  b.add(0x00F9, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00F9)", npc_params); // Woman with cyan hair
  b.add(0x00FA, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00FA)", npc_params); // Woman with bright red hair
  b.add(0x00FB, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00FB)", npc_params); // Man with blue hair near the Ep2 Medical Center
  b.add(0x00FC, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00FC)", npc_params); // Man in room next to Ep2 Hunter's Guild
  b.add(0x00FD, F_V3_V4, A_E2_LAB | A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT, 0, "TObjNpcNgcBase(0x00FD)", npc_params); // TODO
  b.add(0x00FE, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00FE)", npc_params); // Episode 2 Hunter's Guild woman
  b.add(0x00FF, F_V3_V4, A_E2_LAB, 0, "TObjNpcNgcBase(0x00FF)", npc_params); // Woman near room with teleporter to VR areas
  b.add(0x0100, F_V4, A_CITY, 0, "__MOMOKA__", npc_params); // Momoka
  b.add(0x0110, F_EP3, A_E3_MORGUE, 0, "TObjNpcWalkingMeka_Hero", npc_params); // Small talking robot in Morgue
  b.add(0x0111, F_EP3, A_E3_MORGUE, 0, "TObjNpcWalkingMeka_Dark", npc_params); // Small talking robot in Morgue

  // Episode 3 scientist and aide NPCs. These NPCs take all the same params as the NPCs defined above, but also:
  //   angle.x = model number (clamped to [0, 3] for scientists, [0, 2] for aides)
  // The two type values for scientists (00D4 and 00D5) are direct aliases for each other; there is no difference
  // between their in-game appearance or behavior.
  std::vector<PA> ep3_scientist_npc_params = npc_params;
  ep3_scientist_npc_params.emplace_back(PA{"model_index", "clamp(angle.x, 0, 3)"});
  std::vector<PA> ep3_aide_npc_params = npc_params;
  ep3_aide_npc_params.emplace_back(PA{"model_index", "clamp(angle.x, 0, 2)"});
  b.add(0x00D4, F_EP3, A_E3_MORGUE, 0, "TObjNpcHeroScientist", ep3_scientist_npc_params);
  b.add(0x00D5, F_EP3, A_E3_MORGUE, 0, "TObjNpcHeroScientist", ep3_scientist_npc_params);
  b.add(0x0112, F_EP3, A_E3_MORGUE, 0, "TObjNpcHeroAide", ep3_aide_npc_params);

  // Quest NPC. Params are the same as for the standard NPCs above, except:
  //   param6 low byte = flags (bit field):
  //     0x01 = same as param6 above (0 = stand still; 1 = walk around)
  //     0x10 = use alternate animations
  //   param6 high byte = NPC index in npcplayerchar.dat
  std::vector<PA> quest_npc_params = base_npc_params;
  quest_npc_params.emplace_back(PA{"walk_when_idle", "param6 & 0x01"});
  quest_npc_params.emplace_back(PA{"max_walk_distance", p1, "param6 & 0x01"});
  quest_npc_params.emplace_back(PA{"use_alternate_animations", "param6 & 0x10"});
  quest_npc_params.emplace_back(PA{"npc_template_index", "(param6 >> 8) & 0xFF"});
  b.add(0x0118, F_V4, A_E4_NON_CITY | A_E4_P2 | A_E4_TEST, 0, "__QUEST_NPC__", quest_npc_params);

  // Enemy that behaves like an NPC. Has all the same params as the standard NPC types, but also:
  //   angle.x = definition index
  //   angle.z = animation index used in motion debugger (no effect on gameplay)
  // The definition index is an integer from 0 to 15 (decimal) specifying which model, animations, and hitbox to use.
  // The available choices depend on which assets are loaded, which in turn depend on the area (not floor) in which
  // the NPC appears. To choose the right definition index, first look up the enemy you want in the models list
  // below, then find the corresponding number in the areas table below that, and use the (zero-based) index of the
  // number in that table for angle.x. For example, to place a Sinow Gold in Mine 2, angle.x should be 8 since Sinow
  // Gold is 27, and there are 8 other values before 27 in the Mine 2 list. The available models are:
  //   01: Box (appearance depends on area)
  //   02: Booma (Ep1/2), Boota (Ep4)
  //   03: Gobooma (Ep1/2), Ze Boota (Ep4)
  //   04: Gigobooma (Ep1/2), Ba Boota (Ep4)
  //   05: Rappy (common)
  //   06: Rappy (rare; appearance depends on episode and season event)
  //   07: Mothmant
  //   08: Monest
  //   09: Barbarous Wolf (Ep1/2), Satellite Lizard (Ep4)
  //   0A: Savage Wolf (Ep1/2), Yowie (Ep4)
  //   0B: Chao
  //   0C: Recon
  //   0D: Recobox
  //   0E: Ul Gibbon
  //   0F: Zol Gibbon
  //   10: Gee
  //   11: NiGHTS? (TODO; from bm_ene_npc_nights_bml)
  //   12: NiGHTS? (TODO; from bm_ene_npc_nights_bml)
  //   13: Bulclaw
  //   14: Claw
  //   15: Dark Belra
  //   16: Dark Bringer
  //   17: Canadine
  //   18: Canune
  //   19: Dark Gunner (must be constructed after a Dark Gunner enemy, since the definition is populated in the enemy
  //       constructor, not in the asset loader)
  //   1A: Delsaber
  //   1B: So Dimenian (Ep1/2), Goran (Ep4)
  //   1C: La Dimenian (Ep1/2), Pyro Goran (Ep4)
  //   1D: Dimenian (Ep1/2), Goran Detonator (Ep4)
  //   1E: Dubchic (TODO: may be swapped)
  //   1F: Gillchic (TODO: may be swapped)
  //   20: Garanz
  //   21: TODO (from bm_ene_gyaranzo_bml)
  //   22: Nano Dragon (Ep1/2), Zu (Ep4)
  //   23: Pan Arms
  //   24: Hidoom
  //   25: Migium
  //   26: Sinow Beat
  //   27: Sinow Gold
  //   28: Pofuilly Slime
  //   29: Pouilly Slime
  //   2A: Chaos Sorcerer
  //   2B: Sinow Berill
  //   2C: Merillia
  //   2D: Sinow Spigell (TODO: Could be Sinow Berill, but cloaked)
  //   2E: Meriltas
  //   2F: Evil Shark
  //   30: Pal Shark
  //   31: Guil Shark
  //   32: Small Hildebear (same model as TObjNpcMoja) (Ep1/2), Astark (Ep4)
  //   33: Hildebear
  //   34: Hildeblue
  //   35: Grass Assassin
  //   36: TODO (from bm_ene_cgrass_bml)
  //   37: Poison Lily (non-Tower) or Del Lily (Tower)
  //   38: Floating robot (same as TODragonflyMachine01)
  //   39: Ruins falling trap (TOTrapAncient01)
  //   3A: Truncated column (TOVS2Wreck06)
  //   3B: Ruins poison-spewing blob (Ep1) or Gee nest (Ep2)
  //   3C: TODO (TMotorcycle with model number 0)
  //   3D: TODO (TMotorcycle with model number 1)
  //   3E: Jungle small rock (TORockJungleS01)
  //   3F: Jungle plant (TOGrassJungle)
  //   40: Seabed dolphin (same as __DOLPHIN__)
  //   41: Gi Gue
  //   42: Mericarol (ep2), Girtablulu (ep4)
  //   43: Ill Gill
  //   44: Gibbles
  //   45: Dolmolm
  //   46: Delbiter (ep2), Dorphon (ep4)
  //   47: Deldepth
  //   48: Deldepth
  //   49: Sinow Zoa (visible; TODO: verify this)
  //   4A: Sinow Zele (visible; TODO: verify this)
  //   4B: Sinow Zoa (cloaked; TODO: verify this)
  //   4C: Sinow Zele (cloaked; TODO: verify this)
  //   4D: Morfos
  //   4E: Epsilon
  //   4F: Dolmdarl
  // This table shows which choices are available in each area:
  //   Episode 1:
  //     00 (Pioneer 2):     (none)
  //     01 (Forest 1):      01 02 03 04 05 06 07 08 09 0A 0B 3C 3D
  //     02 (Forest 2):      01 02 03 04 05 06 07 08 09 0A 0B 32 33 34 3C 3D
  //     03 (Cave 1):        01 22 23 24 25 2F 30 31 35 36 37
  //     04 (Cave 2):        01 22 28 29 2F 30 31 35 36 37
  //     05 (Cave 3):        01 22 23 24 25 28 29 2F 30 31 37
  //     06 (Mine 1):        01 17 18 1E 1F 20 21 26 27 38
  //     07 (Mine 2):        01 17 18 1E 1F 20 21 26 27 38
  //     08 (Ruins 1):       13 14 15 1A 1B 1C 1D 2A 39 3B
  //     09 (Ruins 2):       16 1A 1B 1C 1D 39 3B
  //     0A (Ruins 3):       15 16 1B 1C 1D 2A 39 3B
  //     0B (Dragon):        01
  //     0C (De Rol Le):     01
  //     0D (Vol Opt):       01
  //     0E (Dark Falz):     01
  //     0F (Lobby):         (none)
  //     10 (Spaceship):     16 1A 26 27 2F 30 31 32 33 34 35 36 3A
  //     11 (Temple):        16 1A 26 27 2F 30 31 32 33 34 35 36 3A
  //   Episode 2:
  //     12 (Lab):           (none)
  //     13 (Temple A):      05 06 07 08 15 1B 1C 1D 32 33 34 35 36 37 39 3A
  //     14 (Temple B):      05 06 07 08 15 1B 1C 1D 32 33 34 35 36 37 39 3A
  //     15 (Spaceship A):   09 0A 1A 1E 1F 20 21 23 24 25 39
  //     16 (Spaceship B):   09 0A 1A 1E 1F 23 24 25 2A 39
  //     17 (CCA):           05 06 0E 0F 10 2B 2C 2D 2E 3B 3E 3F 41 42 44
  //     18 (Jungle N):      05 06 0E 0F 10 2B 2C 2D 2E 3B 3E 3F 41 42 44
  //     19 (Jungle E):      05 06 0E 0F 10 2B 2C 2D 2E 3B 3E 3F 41 42 44
  //     1A (Mountain):      05 06 0E 0F 10 2B 2C 2D 2E 3B 3E 3F 41 42 44
  //     1B (Seaside):       05 06 0E 0F 10 2B 2C 2D 2E 3B 3E 3F 41 42 44
  //     1C (Seabed U):      01 0C 0D 40 45 46 47 48 49 4A 4B 4C 4D 4F
  //     1D (Seabed L):      01 0C 0D 40 45 46 47 48 49 4A 4B 4C 4D 4F
  //     1E (Gal Gryphon):   (none)
  //     1F (Olga Flow):     01
  //     20 (Barba Ray):     (none)
  //     21 (Gol Dragon):    (none)
  //     22 (Seaside Night): 05 06 0C 0D 0E 0F 10 11 12 2C 2E 3B 3E 3F 45 4F
  //     23 (Tower):         0C 0D 37 3B 41 42 43 44 46 4E
  //   Episode 4 (these are not usable without a client patch, since this NPC is only available on Pioneer 2):
  //     24 (Crater East):   02 03 04 05 06 09 0A 22 32 46
  //     25 (Crater West):   02 03 04 05 06 09 0A 22 32 46
  //     26 (Crater South):  02 03 04 05 06 09 0A 22 32 46
  //     27 (Crater North):  02 03 04 05 06 09 0A 22 32 46
  //     28 (Crater Int):    02 03 04 05 06 09 0A 22 32 46
  //     29 (Desert 1):      05 06 09 0A 1B 1C 1D 22 3B 42
  //     2A (Desert 2):      05 06 09 0A 1B 1C 1D 22 3B 42
  //     2B (Desert 3):      05 06 09 0A 1B 1C 1D 22 3B 42
  //     2C (Saint-Milion):  38 39 3A 3B 3C 3D 3E 3F 40
  //     2D (Pioneer 2):     (none)
  //     2E (Test area):     01 02 09 0A 22 32 38 39 3A 3B 3C 3D 3E 3F 40 42
  // This NPC exists in Episode 3, but is only available in the Morgue and in the lobby. There are no available models
  // in those areas, which makes it essentially useless.
  std::vector<PA> enemy_npc_params = npc_params;
  enemy_npc_params.emplace_back(PA{"definition_index", ax});
  b.add(0x0033, F_V3_V4, A_ALL & (~A_E4_NON_CITY) & (~A_E4_TEST), A_E3_MORGUE | A_LOBBY, "TObjNpcEnemy", enemy_npc_params);

  // Hildebear. Params:
  //   param1 = initial location (<1 = ground, >=1 = jump)
  //   param2 = chance to use tech (value is param2 + 0.3 clamped to [0, 1])
  //   param3 = chance to jump when more than 150 units away (value is param3 + 0.6 clamped to [0, 1])
  //   param6 = if >= 1, always rare
  b.add(0x0040, F_V0_V4, A_E1_FOREST_2 | A_E1_QUEST_ONLY | A_E2_TEMPLE, 0, "TObjEneMoja", {{"spawn_as_jump", "param1 >= 1"}, {"technique_chance", "clamp(param2 + 0.3, 0, 1)"}, {"jump_chance", "clamp(param3 + 0.6, 0, 1)"}, {"force_rare", "param6 >= 1"}});

  // Rappy. Params:
  //   param1 = initial idle behavior (0 = wander, nonzero = stationary idle; wave_number > 0 causes Rappy to fall from
  //     the sky at construction time and use stationary idle behavior)
  //   param6 = rare flag (on v1-v3, rappy is rare if param6 != 0; on v4, rappy is rare if (param6 & 1) != 0)
  //   param7 = unused; this sets a field on the enemy object but that field appears not to be used anywhere
  // Exactly which rappy is constructed depends on param6 (or the random rare check) and the current season event:
  //   Ep1/Ep2 non-rare = Rag Rappy
  //   Ep4 non-rare = Sand Rappy (Crater or Desert variation)
  //   Ep1 rare = Al Rappy
  //   Ep2 rare, Christmas = Saint Rappy
  //   Ep2 rare, Easter = Egg Rappy
  //   Ep2 rare, Halloween = Hallo Rappy
  //   Ep2 rare, any other season event (or none) = Love Rappy
  //   Ep4 rare = Del Rappy (Crater or Desert variation)
  b.add(0x0041, F_V0_V4, A_E1_FOREST | A_E2_TEMPLE | A_E4_NON_BOSS | A_E4_TEST, 0, "TObjEneLappy", {{"wander_when_idle", "(param1 == 0) && (wave_number == 0)"}, {"force_rare", "is_v4 ? (param6 & 1) : (param6 != 0)"}});

  // Monest (and Mothmants). Params:
  //   param2 = number of Mothmants to expel at start (clamped to [0, 6])
  //   param3 = total Mothmants (clamped to [0, min(30, num_children)] where num_children comes from the EnemySetEntry;
  //     if this is less than param2, then param2 will take precedence but no further Mothmants will emerge after the
  //     first group)
  // Note: In map_forest01_02e.dat in the vanilla map files there is a Monest that has param1 = 3 and param2 = 10.
  // This looks like just an off-by-one error on Sega's part where they accidentally shifted the parameters down by
  // one place. As described above, this Monest expels 6 Mothmants immediately, then none after those 6 are killed.
  b.add(0x0042, F_V0_V4, A_E1_FOREST | A_E2_TEMPLE, 0, "TObjEneBm3FlyNest", {{"initial_mothmants", "clamp(int(param2), 0, 6)"}, {"total_mothmants", "max(clamp(int(param2), 0, 6), clamp(int(param3), 0, min(30, (num_children ? num_children : 30))))"}});

  // Savage Wolf or Barbarous Wolf. Params:
  //   param1 = group number (when a Barbarous Wolf dies, all wolves with the same group number howl and trigger their
  //     buffs or weaknesses)
  //   param2 = if less than 1, this is a Savage Wolf; otherwise it's a Barbarous Wolf
  b.add(0x0043, F_V0_V4, A_E1_FOREST | A_E2_SPACESHIP, 0, "TObjEneBm5Wolf", {{"group_id", p1}, {"is_barbarous_wolf", "param2 >= 1"}});

  // Booma, Gobooma, or Gigobooma. The activation radius is fixed and cannot be changed: 50 for Hunters, 100 for
  // Rangers and Forces; the deactivation radius is 100 for Hunters and 150 for Rangers and Forces. Params:
  //   param1 = low-HP threshold (only used for Booma; HP below (max_hp * param1) is considered low)
  //   param2 = idle walk radius (when there's no target, it will walk around its spawn location within this radius;
  //     if this is zero, it stands still instead)
  //   param6 = type (clampes to [0, 2]; 0 = Booma, 1 = Gobooma, 2 = Gigobooma)
  //   param7 = group ID (<=0 = independent enemy, >0 = member of a group; enemies in the same group and room will
  //     band together and all attack the same player, chosen by the highest-ranking enemy (by param6) in the group)
  // Note that the group ID is not unique to Booma-type enemies - Booma-type enemies consider all enemies in the same
  // room with the same param7 value to be part of the group, regardless of the enemy's type. They may therefore make
  // decisions based on what non-Booma-type enemies in the room do.
  std::vector<PA> TObjEneBeast_params{
      {"low_hp_threshold", p1}, {"idle_walk_radius", p2}, {"type", "clamp(param6, 0, 2)"}, {"group_id", p7}};
  b.add(0x0044, F_V0_V4, A_E1_FOREST, 0, "TObjEneBeast", TObjEneBeast_params);

  // Grass Assassin. Params:
  //   param1 = low-HP threshold (moves faster if HP is below (max_hp * param1))
  //   param2 = initial forward walk distance (if positive, its initial movement target is this far forward from its
  //     start position; otherwise it begins with normal movement behavior)
  //   param3 = model Y offset (visual only)
  //   param4 = baby count delta (on death, (param4 + 12) babies run away from the dying Grass Assassin)
  b.add(0x0060, F_V0_V4, A_E1_CAVE_1 | A_E1_CAVE_2 | A_E1_QUEST_ONLY | A_E2_TEMPLE, 0, "TObjGrass", {{"low_hp_threshold", p1}, {"initial_walk_distance", p2}, {"model_y_offset", p3}, {"death_baby_count", "param4 + 12"}});

  // Poison Lily or Del Lily. Del Lily is constructed if the current area is 0x23 (Control Tower); otherwise, Poison
  // Lily is constructed. There appear to be no parameters.
  b.add(0x0061, F_V0_V4, A_E1_CAVE | A_E2_TEMPLE | A_E2_TOWER, 0, "TObjEneRe2Flower", {});

  // Nano Dragon. Params:
  //   param1 = spawn type (1 = fly in from behind, anything else = start on ground)
  //   param2 = idle wander radius around its spawn position (defaults to 50 if param2 < 1 and param1 == 1)
  b.add(0x0062, F_V0_V4, A_E1_CAVE, 0, "TObjEneNanoDrago", {{"spawn_flying", "param1 == 1"}, {"idle_wander_radius", "((param1 == 1) && (param2 < 1)) ? 50 : param2"}});

  // Evil Shark, Pal Shark, or Guil Shark. Same params as 0x0044 (TObjEneBeast), except:
  //   param6 = type (clamped to [0, 2]; 0 = Evil Shark, 1 = Pal Shark, 2 = Guil Shark)
  b.add(0x0063, F_V0_V4, A_E1_CAVE | A_E1_QUEST_ONLY, 0, "TObjEneShark", TObjEneBeast_params);

  // Pofuilly Slime. num_children is clamped to [0, 4]. Params:
  //   param7 = rare flag (if the lowest bit is set, this is a Pouilly Slime instead; on BB, this is ignored)
  b.add(0x0064, F_V0_V4, A_E1_CAVE_2 | A_E1_CAVE_3, 0, "TObjEneSlime", {{"force_rare", "param7 & 1", "!is_v4"}});

  // Pan Arms (Hidoom + Migium). There appear to be no parameters.
  b.add(0x0065, F_V0_V4, A_E1_CAVE_1 | A_E1_CAVE_3 | A_E2_SPACESHIP, 0, "TObjEnePanarms", {});

  // Gillchic or Dubchic. Params:
  //   param1 = rapid fire count (number of lasers fired before moving again; if this is 0, the default of 2 is used;
  //     the low bit of this value (as an integer) sets the target scan radius: 170 if zero, 220 if nonzero; if
  //     negative, laser attack is not used)
  //   param6 = type (0 = Dubchic, any nonzero value = Gillchic)
  b.add(0x0080, F_V0_V4, A_E1_MINE | A_E2_SPACESHIP, 0, "TObjEneDubchik", {{"rapid_fire_count", "(int(param1) == 0) ? 2 : int(param1)"}, {"laser_target_scan_radius", "(int(param1) & 1) ? 220 : 170"}, {"is_gillchic", "param6 != 0"}});

  // Garanz. There appear to be no parameters.
  b.add(0x0081, F_V0_V4, A_E1_MINE | A_E2_SPACESHIP_A, 0, "TObjEneGyaranzo", {});

  // Sinow Beat. Params:
  //   param1 = disable mirage effect if >= 1.0
  //   param2 = is Sinow Gold if >= 1.0
  //   param3 = debug mode animation index (no effect in gameplay; clamped to 0-10)
  //   param4 = debug mode animation frame (no effect in gameplay; clamped to the animation's frame range)
  // Note: All params are on the base class (TObjEneMe3Shinowa).
  b.add(0x0082, F_V0_V4, A_E1_MINE | A_E1_QUEST_ONLY, 0, "TObjEneMe3ShinowaReal", {{"disable_mirage", "param1 >= 1"}, {"is_sinow_gold", "param2 >= 1"}});

  // Canadine. Params:
  //   param1 = behavior (0 = in fighter, anything else = out fighter; this controls whether the Canadine will use
  //     its direct attack or stay high off the ground instead)
  b.add(0x0083, F_V0_V4, A_E1_MINE, 0, "TObjEneMe1Canadin", {{"uses_direct_attack", "param1 == 0"}});

  // Canane. There appear to be no parameters. There are always 8 followers arranged in a ring around the Canane.
  b.add(0x0084, F_V0_V4, A_E1_MINE, 0, "TObjEneMe1CanadinLeader", {});

  // Dubwitch. Destroying a Dubwitch destroys all Dubchics in the same room. Dubwitches have a max HP of 300
  // regardless of the game's difficulty level. There appear to be no parameters.
  b.add(0x0085, F_V0_V4, A_E1_MINE | A_E2_SPACESHIP, 0, "TOCtrlDubchik", {});

  // Delsaber. Params:
  //   param1 = shield max HP delta (value is param1 + 100)
  //   param2 = prejudice flag (these correspond to the bits in PlayerVisualConfigV123T::class_flags):
  //     0 = males
  //     1 = females
  //     2 = humans
  //     3 = newmans
  //     4 = androids
  //     5 = hunters
  //     6 = rangers
  //     7 = forces
  //     8 = no prejudice
  // By default, the Delsaber will target the nearest player that matches its prejudice flag. If no players match the
  // flag or there is any player less than 30 units away from the Delsaber, then the Delsaber will target the nearest
  // player overall and ignore its prejudice flag.
  b.add(0x00A0, F_V0_V4, A_E1_RUINS_1 | A_E1_RUINS_2 | A_E1_QUEST_ONLY | A_E2_SPACESHIP, 0, "TObjEneSaver", {{"shield_max_hp", "param1 + 100"}, {"prejudice_flag", p2}});

  // Chaos Sorcerer. There appear to be no parameters.
  b.add(0x00A1, F_V0_V4, A_E1_RUINS_1 | A_E1_RUINS_3 | A_E2_SPACESHIP_B, 0, "TObjEneRe4Sorcerer", {});

  // Dark Gunner. Params:
  //   param1 = group number (there should be between 1 and 16 Dark Gunners and one control enemy with the same group
  //     number in the same room)
  //   param2 = leader rotation order within the group (higher values later)
  //   param7 = TODO
  b.add(0x00A2, F_V0_V4, A_E1_RUINS_2 | A_E1_RUINS_3, 0, "TObjEneDarkGunner", {{"group_id", p1}, {"leader_order", p2}});

  // Dark Gunner control enemy. This enemy doesn't actually exist in-game; it only has logic for assigning the group
  // leader periodically and coordinating movement and laser attacks. Params:
  //   param1 = group number (see TObjEneDarkGunner above)
  b.add(0x00A3, F_V0_V4, A_E1_RUINS_2 | A_E1_RUINS_3, 0, "TObjEneDarkGunCenter", {{"group_id", p1}});

  // Dark Bringer. There appear to be no parameters.
  b.add(0x00A4, F_V0_V4, A_E1_RUINS_2 | A_E1_RUINS_3 | A_E1_QUEST_ONLY, 0, "TObjEneDf2Bringer", {});

  // Dark Belra. There appear to be no parameters.
  b.add(0x00A5, F_V0_V4, A_E1_RUINS_1 | A_E1_RUINS_3 | A_E2_TEMPLE, 0, "TObjEneRe7Berura", {});

  // Dimenian / La Dimenian / So Dimenian. Same parameters as 0x0044 (TObjEneBeast), except:
  //   param6 = type (clamped to [0, 2]; 0 = Dimenian, 1 = La Dimenian, 2 = So Dimenian)
  b.add(0x00A6, F_V0_V4, A_E1_RUINS | A_E2_TEMPLE, 0, "TObjEneDimedian", TObjEneBeast_params);

  // Bulclaw. There appear to be no parameters.
  b.add(0x00A7, F_V0_V4, A_E1_RUINS, 0, "TObjEneBalClawBody", {});

  // Claw. There appear to be no parameters.
  b.add(0x00A8, F_V0_V4, A_E1_RUINS, 0, "TObjEneBalClawClaw", {});

  // Dragon-like bosses. None of these take any parameters.
  b.add(0x00C0, F_V0_V4, A_E1_DRAGON, 0, "TBoss1Dragon", {}); // Dragon
  b.add(0x00C0, F_V3_V4, A_E2_GAL_GRYPHON, 0, "TBoss5Gryphon", {}); // Gal Gryphon
  b.add(0x00CC, F_V3_V4, A_E2_GOL_DRAGON, 0, "TBoss8Dragon", {}); // Gol Dragon

  // De Rol Le-like bosses. Neither of these take any parameters.
  b.add(0x00C1, F_V0_V4, A_E1_DE_ROL_LE, 0, "TBoss2DeRolLe", {}); // De Rol Le
  b.add(0x00CB, F_V3_V4, A_E2_BARBA_RAY, 0, "TBoss7DeRolLeC", {}); // Barba Ray

  // Vol Opt and various pieces thereof. Generally only TBoss3Volopt and TBoss3VoloptP02 should be specified in map
  // files; the other enemies are automatically created by TBoss3Volopt. None of these take any parameters.
  b.add(0x00C2, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3Volopt", {}); // Main control object
  b.add(0x00C3, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3VoloptP01", {}); // Phase 1 (x6; one for each big monitor)
  b.add(0x00C4, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3VoloptCore", {}); // Core
  b.add(0x00C5, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3VoloptP02", {}); // Phase 2
  b.add(0x00C6, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3VoloptMonitor", {}); // Monitor (x24; 4 for each wall)
  b.add(0x00C7, F_V0_V4, A_E1_VOL_OPT, 0, "TBoss3VoloptHiraisin", {}); // Pillar (lightning rod)

  // Dark Falz-like bosses. Neither of these take any parameters.
  b.add(0x00C8, F_V0_V4, A_E1_DARK_FALZ, 0, "TBoss4DarkFalz", {}); // Dark Falz
  b.add(0x00CA, F_V3_V4, A_E2_OLGA_FLOW, 0, "TBoss6PlotFalz", {}); // Olga Flow

  // Sinow Berill or Sinow Spigell. Params:
  //   param1 = spawn type:
  //     0 = invisible + ground
  //     1 = invisible + ceiling
  //     2 = visible + ground
  //     3 = visible + ceiling
  //   param2 = chance to enable stealth (value used is param2 + 0.3)
  //   param3 = chance to cast technique (value used is param3 + 0.4)
  //   param4 = chance to teleport (value used is param4 + 0.5)
  //   param5 = chance to disable stealth (value used is param5 + 0.1; checked when hit by a player and also when hit
  //     animation completes)
  //   param6 = type:
  //     zero or negative = Sinow Berill
  //     positive = Sinow Spigell
  // param2, param3, and param4 are evaluated in that order after the Sinow jumps back. That is, the game first
  // generates a random float between 0 and 1, and compares it to param2 to decide whether to enable stealth (if not
  // already enabled). If it does, the other params are ignored. If it doesn't enable stealth, the game then checks
  // param3 in the same manner to determine whether to cast a tech; if that doesn't happen either, the game checks
  // param4 to determine whether to teleport. If none of those happen, the Sinow walks forward again and attacks.
  std::vector<PA> ep2_sinow_params_base{{"visible_spawn", "param1 >= 2"}, {"ceiling_spawn", "int(param1) & 1"}, {"enable_stealth_chance", "param2 + 0.3"}, {"cast_technique_chance", "param3 + 0.4"}, {"teleport_chance", "param4 + 0.5"}, {"disable_stealth_chance", "param5 + 0.1"}};
  std::vector<PA> TObjEneMe3StelthReal_params = ep2_sinow_params_base;
  TObjEneMe3StelthReal_params.emplace_back(PA{"is_sinow_spigell", "param6 > 0"});
  b.add(0x00D4, F_V3_V4, A_E2_CCA_ALL, 0, "TObjEneMe3StelthReal", TObjEneMe3StelthReal_params);

  // Merillia / Meriltas. Params:
  //   param1 = chance to run away after being hit (value used is param1 - 0.2, clamped below to 0; only applies when
  //     HP is at or below the threshold (movement data 4A or 4B iparam1; see notes/movement-data.txt) or after the
  //     poison attack)
  //   param2 = idle wandering radius around the spawn position (positive = wanders after returning home; zero or
  //     negative = burrows instead)
  //   param3 = chance to do the poison attack after being hit (value used is param3 - 0.2, clamped below to 0;
  //     only applies when HP is low as in param1 description; param1 return-home chance is checked first)
  //   param4 = retreat duration in frames (value used is param4 + 300; directly proportional to retreat distance)
  //   param5 = wakeup radius delta (value used is param5 + 100, clamped below to 15; enemy will wake up when any
  //     player is nearby)
  //   param6 = type (clamped to [0, 1]; 0 = Merillia, 1 = Meriltas)
  //   param7 = group number (similar to TObjEneBeast's logic)
  b.add(0x00D5, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT, 0, "TObjEneMerillLia", {{"run_away_chance_after_hit", "max(0, param1 - 0.2)"}, {"idle_wander_radius", p2}, {"poison_attack_chance_after_hit", "max(0, param3 - 0.2)"}, {"retreat_duration_frames", "param4 + 300"}, {"wakeup_radius", "max(15, param5 + 100)"}, {"is_meriltas", "param6 >= 1"}, {"group_id", p7}});

  // Mericarol / Mericus / Merikle / Mericarand. Params:
  //   param1 = chance of run attack after being hit when HP is in the range [max HP / 5, max HP / 2] (value used is
  //     param1 + 0.5)
  //   param2 = speed during run attack (units per frame; value used is param2 + 3, clamped below to 1)
  //   param3 = chance of doing spit attack when player is nearby (actual probability is param3 + 0.1; if the check
  //     fails, it will do the slash attack instead)
  //   param6 = subtype:
  //     0 = Mericarol
  //     1 = Mericus
  //     2 = Merikle
  //     anything else = Mericarand (see below)
  // If this is a Mericarand, it is "randomly" chosen to be one of the three subtypes at construction time. On v3,
  // the client chooses randomly (but consistently, based on the entity ID) between Mericarol (80%), Mericus (10%) or
  // Merikle (10%). On v4, if the entity ID isn't marked rare by the server, the Mericarand becomes a Mericarol;
  // otherwise, it becomes a Mericus if its entity ID is even or a Merikle if it's odd.
  b.add(0x00D6, F_V3_V4, A_E2_CCA_ALL | A_E2_TOWER, 0, "TObjEneBm9Mericarol", {{"run_attack_chance_after_hit", "param1 + 0.5"}, {"run_attack_speed", "max(1, param2 + 3)"}, {"spit_attack_chance", "param3 + 0.1"}, {"type", p6}});

  // Ul Gibbon / Zol Gibbon. Params:
  //   param1 = group number (Gibbons in the same group can activate each other and receive status effects from their
  //     Zol Gibbon leader)
  //   param2 = spawn type (<1 = spot spawn; >=1 = jump spawn)
  //   param3 = chance jumping backward instead of forward (value used is param3 + 0.4)
  //   param4 = chance of casting a tech when not near a player (value used is param4 + 0.3; checked every 50 units)
  //   param5 = chance of casting a tech immediately after a backward jump (value used is param5 + 0.3)
  //   param6 = type (zero or negative = Ul Gibbon, positive = Zol Gibbon)
  b.add(0x00D7, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT, 0, "TObjEneBm5GibonU", {{"group_id", p1}, {"spawn_as_jump", "param2 >= 1"}, {"jump_back_chance", "param3 + 0.4"}, {"distance_cast_tech_chance", "param4 + 0.3"}, {"jump_back_cast_tech_chance", "param5 + 0.3"}, {"is_zol_gibbon", "param6 > 0"}});

  // Gibbles. Params:
  //   param1 = guard max HP delta (value is param1 + 100; similar to TObjEneSaver's shield)
  //   param2 = prejudice flag (see 0x00A0 (TObjEneSaver); the behavior here is exactly the same)
  //   param3 = chance to jump at each decision point (0-1)
  //   param4 = chance to jump after being hit (0-1)
  b.add(0x00D8, F_V3_V4, A_E2_CCA_ALL | A_E2_TOWER, 0, "TObjEneGibbles", {{"guard_max_hp", "param1 + 100"}, {"prejudice_flag", p2}, {"jump_chance", p3}, {"jump_chance_after_hit", p4}});

  // Gee. Params:
  //   param1 = in fighter / out fighter setting (see TObjEneMe1Canadin)
  //   param2 = appear height (0 = low, 1 = high)
  //   param3 = appear speed (value is param3 + 1, clamped below to 1)
  //   param4 = needle speed (value is param4 + 6, clamped below to 0.01)
  // Note: The client's debug strings say "APPEAR SPEED(+1.f Min:0.01f)" for param3 and "NEEDLE SPEED(+6.f Min:1.f)"
  // for param4. The "Min:" parts of those strings are incorrect; the comments above are correct.
  b.add(0x00D9, F_V3_V4, A_E2_CCA_ALL | A_E2_SEASIDE_NIGHT, 0, "TObjEneMe1Gee", {{"uses_direct_attack", "param1 == 0"}, {"appear_high", "param2 == 1"}, {"appear_speed", "max(1, param3 + 1)"}, {"needle_speed", "max(0.01, param4 + 6)"}});

  // Gi Gue. Params:
  //   param1 = spawn type (<1 = fade in on the ground; >=1 = descend from above)
  //   param2 = idle wandering radius multiplier (defaults to 50 if param1 >= 1 and param2 < 1; each movement is in
  //     range [0.4, 0.8] * param2)
  //   param3 = flight height delta (value is param3 + 150, clamped below to 0)
  //   param4 = chance to create barrier (value is param4 + 0.5)
  //   param5 = chance to use Jellen/Zalure attack (value is param5 + 0.5)
  b.add(0x00DA, F_V3_V4, A_E2_CCA_ALL | A_E2_TOWER, 0, "TObjEneMe1GiGue", {{"spawn_from_above", "param1 >= 1"}, {"idle_wander_radius_factor", "((param1 >= 1) && (param2 < 1)) ? 50 : param2"}, {"flight_height", "max(0, param3 + 150)"}, {"create_barrier_chance", "param4 + 0.5"}, {"jellen_zalure_chance", "param5 + 0.5"}});

  // Deldepth. Params:
  //   param1 = Rabarta bomb speed delta (value is param1 + 0.6, clamped below to 0)
  b.add(0x00DB, F_V3_V4, A_E2_SEABED, 0, "TObjEneDelDepth", {{"rabarta_bomb_speed", "max(0, param1 + 0.6)"}});

  // Delbiter. Params:
  //   param1 = chance to howl (value is param1 + 0.3)
  //   param2 = chance to cause confusion via howl (value is param2 + 0.3)
  //   param3 = maximum distance at which howl can cause confusion (value is param3 + 30, clamped below to 10)
  //   param4 = chance to fire laser (value is param4 + 0.3)
  //   param5 = chance to charge (value is param5 + 0.05)
  //   param6 = spawn type (<=0 = stand; >0 = run)
  std::vector<PA> TObjEneDellBiter_base_params{{"howl_chance", "param1 + 0.3"}, {"howl_confusion_chance", "param2 + 0.3"}, {"howl_confusion_range", "max(10, param3 + 30)"}, {"laser_chance", "param4 + 0.3"}, {"charge_chance", "param5 + 0.05"}};
  std::vector<PA> TObjEneDellBiter_params = TObjEneDellBiter_base_params;
  TObjEneDellBiter_params.emplace_back(PA{"spawn_running", "param6 > 0"});
  b.add(0x00DC, F_V3_V4, A_E2_SEABED | A_E2_TOWER, 0, "TObjEneDellBiter", TObjEneDellBiter_params);

  // Dolmolm / Dolmdarl. Same parameters as TObjEneBeast, but also:
  //   param3 = chance to retreat backward (value is param3 + 0.3)
  //   param4 = chance to jump-attack instead of using normal melee (value is param4 + 0.3; requires target to be
  //     within fparam5 distance (see notes/movement-data.txt entries 4E, 4F))
  //   param6 = type (zero or negative = Dolmolm; positive = Dolmdarl)
  //   param7 = group number (similar to TObjEneBeast's logic)
  b.add(0x00DD, F_V3_V4, A_E2_SEABED | A_E2_SEASIDE_NIGHT, 0, "TObjEneDolmOlm", {{"low_hp_threshold", p1}, {"idle_walk_radius", p2}, {"retreat_chance", "param3 + 0.3"}, {"jump_attack_chance", "param4 + 0.3"}, {"is_dolmdarl", "param6 > 0"}, {"group_id", p7}});

  // Morfos. Params:
  //   param1 = movement speed delta (value is param1 + 0.28, clamped below to 0.1)
  //   param2 = movement leg distance delta (value is param2 + 20, clamped below to 5)
  //   param3 = chance to enable front shield when current HP <= (max HP / 2) (value used is param3 + 0.1)
  //   param4 = chance to use laser volley when current HP <= (max HP / 2) (value used is param4 + 0.1)
  //   param6 = translation flag: >=1 disables x/z translation movement in non-Ultimate; <1 allows translation
  b.add(0x00DE, F_V3_V4, A_E2_SEABED, 0, "TObjEneMorfos", {{"movement_speed", "max(0.1, param1 + 0.28)"}, {"movement_distance", "max(5, param2 + 20)"}, {"low_hp_shield_chance", "param3 + 0.1"}, {"low_hp_laser_volley_chance", "param4 + 0.1"}, {"disable_translation_movement", "param6 >= 1"}});

  // Recobox. Params:
  //   param1 = Recon floating height (value used for each Recon is 45 + rand(-2, 2) + param1)
  //   param2 = Recon target radius (distance from target; value used for each Recon is 50 + rand(-5, 5) + param2;
  //     param2 is clamped below to -30)
  //   param4 = maximum number of concurrently-active Recons (clamped to [0, min(6, num_children - 1)])
  //   param6 = type:
  //     zero or negative = floor (Recons exit upward)
  //     1 = ceiling (Recons exit downward)
  //     2 or greater = wall (Recons exit horizontally)
  // Note: The debug strings in TObjEneRecobox_v6A seem to imply that the total Recon count in the box is (param7 + 1);
  //  however, this is not true. The total Recon count in the box is actually min(30, num_children - 1).
  b.add(0x00DF, F_V3_V4, A_E2_SEABED | A_E2_QUEST_ONLY, 0, "TObjEneRecobox", {{"recon_float_height", "param1 + 45"}, {"recon_target_radius", "max(-30, param2) + 50"}, {"max_active_recons", "clamp(int(param4), 0, clamp(num_children - 1, 0, 6))"}, {"type", "clamp(param6, 0, 2)"}});

  // Sinow Zoa / Sinow Zele. Same params as TObjEneMe3StelthReal (Sinow Berill / Sinow Spigell), except:
  //   param6 = type:
  //     zero or negative = Sinow Zoa
  //     positive = Sinow Zele
  std::vector<PA> TObjEneMe3SinowZoaReal_params = ep2_sinow_params_base;
  TObjEneMe3SinowZoaReal_params.emplace_back(PA{"is_sinow_zele", "param6 > 0"});
  b.add(0x00E0, F_V3_V4, A_E2_SEABED, 0, "TObjEneMe3SinowZoaReal", TObjEneMe3SinowZoaReal_params);

  // Epsilon. Params:
  //   param1 = movement speed (value is param1 + 0.5, clamped below to 0)
  //   param2 = turn speed in angle units / frame (value is param2 + 512; it appears this was supposed to be clamped
  //     below to 0, but due to a copy/paste error it isn't)
  //   param3 = first attack timer period (value is max(param3 + 20, 5) * 30 frames)
  //   param4 = second attack timer period (value is max(param4 + 20, 5) * 30 frames)
  b.add(0x00E0, F_V3_V4, A_E2_TOWER, 0, "TObjEneEpsilonBody", {{"movement_speed", "max(0, param1 + 0.5)"}, {"turn_speed", "param2 + 512"}, {"first_attack_period_frames", "max(param3 + 20, 5) * 30"}, {"second_attack_period_frames", "max(param4 + 20, 5) * 30"}});

  // Ill Gill. Params:
  //   param1 = if nonzero, uses charge attack initially
  //   param2 = scythe special type:
  //     0 = random (35% instant death, 32.5% freeze, 32.5% shock)
  //     1 = shock
  //     2 = freeze
  //     3 = instant death
  //     anything else = physical
  b.add(0x00E1, F_V3_V4, A_E2_TOWER, 0, "TObjEneIllGill", {{"use_initial_charge_attack", "param1 != 0"}, {"scythe_special_type", p2}});

  // Astark. Params:
  //   param1 = spawn type (1 = jumps in from behind; anything else = emerges from ground)
  //   param2 = poison-spray chance (value is param2 + 0.3, clamped to [0, 1])
  //   param3 = jump/slam attack selection chance (value is param3 + 0.6, clamped to [0, 1])
  b.add(0x0110, F_V4, A_E4_CRATER | A_E4_TEST, 0, "__ASTARK__", {{"spawn_as_jump", "param1 == 1"}, {"poison_spray_chance", "clamp(param2 + 0.3, 0, 1)"}, {"jump_slam_attack_chance", "clamp(param3 + 0.6, 0, 1)"}});

  // Satellite Lizard / Yowie. Params:
  //   param1 = group_id (converted to integer with floor())
  //   param2 = type (<1 = Satellite Lizard, >=1 = Yowie)
  b.add(0x0111, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__SATELLITE_LIZARD_YOWIE__", {{"group_id", "floor(param1)"}, {"is_yowie", "param2 >= 1"}});

  // Merissa A. Params:
  //   param6 = flags (bit field):
  //     0x0001 = always rare (Merissa AA)
  b.add(0x0112, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__MERISSA_A__", {{"force_rare", "param6 & 1"}});

  // Girtablulu. There appear to be no parameters.
  b.add(0x0113, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__GIRTABLULU__", {});

  // Zu. Params:
  //   param6 = flags (bit field):
  //     0x0001 = always rare (Pazuzu)
  b.add(0x0114, F_V4, A_E4_CRATER | A_E4_DESERT | A_E4_TEST, 0, "__ZU__", {{"force_rare", "param6 & 1"}});

  // Boota / Ze Boota / Ba Boota. Same parameters as TObjEneBeast, but also:
  //   param3 = delta chance to cast Foie (Ba Boota only; actual chance is (param3 + 10) * 0.01)
  //   param6 = type (clamped to [0, 2]; 0 = Boota, 1 = Ze Boota, 2 = Ba Boota)
  std::vector<PA> __BOOTA_FAMILY___params = TObjEneBeast_params;
  __BOOTA_FAMILY___params.emplace_back(PA{"cast_foie_chance", "(param3 + 10) * 0.01", "param6 >= 2"});
  b.add(0x0115, F_V4, A_E4_CRATER | A_E4_TEST, 0, "__BOOTA_FAMILY__", __BOOTA_FAMILY___params);

  // Dorphon. Params:
  //   param1-5 = same as TObjEneDellBiter
  //   param6 = flags (bit field):
  //     0x0001 = always rare (Dorphon Eclair)
  std::vector<PA> __DORPHON___params = TObjEneDellBiter_base_params;
  __DORPHON___params.emplace_back(PA{"force_rare", "param6 & 1"});
  b.add(0x0116, F_V4, A_E4_CRATER | A_E4_TEST, 0, "__DORPHON__", __DORPHON___params);

  // Goran / Pyro Goran / Goran Detonator. Same parameters as TObjEneBeast, but also:
  //   param3 = delta chance to teleport (actual chance is (param3 + 50) * 0.01)
  //   param6 = type (clamped to [0, 2]; 0 = Goran, 1 = Pyro Goran, 2 = Goran Detonator)
  std::vector<PA> __GORAN_FAMILY___params = TObjEneBeast_params;
  __GORAN_FAMILY___params.emplace_back(PA{"teleport_chance", "(param3 + 50) * 0.01"});
  b.add(0x0117, F_V4, A_E4_DESERT | A_E4_TEST, 0, "__GORAN_FAMILY__", __GORAN_FAMILY___params);

  // Saint-Milion / Shambertin / Kondrieu. Params:
  //   param6 = flags (bit field):
  //     0x0001 = type (0 = Saint-Milion, 1 = Shambertin; ignored if enemy is set as rare by the server, in which case
  //       it's Kondrieu)
  b.add(0x0119, F_V4, A_E4_BOSS, 0, "__EPISODE_4_BOSS__", {{"is_shambertin", "param6 & 1"}});

  return std::move(b.ret);
};

static const std::unordered_multimap<uint16_t, DATEntityDefinition>& default_enemy_definitions() {
  static const auto ret = create_default_dat_enemy_definitions();
  return ret;
}

static std::vector<const DATEntityDefinition*> definitions_for_entity_type(
    const std::unordered_multimap<uint16_t, DATEntityDefinition>& index,
    uint16_t type,
    Version version,
    uint8_t area) {
  auto its = index.equal_range(type);
  uint16_t version_mask = (1 << static_cast<size_t>(version));

  // If matching by (type, version, area), there should be only one match
  if ((version != Version::UNKNOWN) && (area != 0xFF)) {
    // Ep3's Morgue and battle are after all the other areas in the bitmask, so we have to special-case them here
    uint64_t area_mask = 1ULL << ((is_ep3(version) && (area < 2)) ? (area + 0x2F) : area);
    for (auto [it, end_it] = its; it != end_it; it++) {
      const auto& def = it->second;
      if ((def.area_flags & area_mask) && (def.version_flags & version_mask)) {
        return {&def};
      }
    }
    return {};
  } else {
    // If matching only by type or by (type, version), we can expect multiple matches
    std::vector<const DATEntityDefinition*> ret;
    for (auto [it, end_it] = its; it != end_it; it++) {
      const auto& def = it->second;
      if ((version == Version::UNKNOWN) || (def.version_flags & version_mask)) {
        ret.emplace_back(&def);
      }
    }
    return ret;
  }
}

static std::string name_for_entity_definitions(uint16_t type, const std::vector<const DATEntityDefinition*>& defs) {
  if (defs.empty()) {
    return std::format("__UNKNOWN_{:04X}__", type);
  } else {
    std::string ret;
    for (size_t z = 0; z < defs.size(); z++) {
      if (z) {
        ret.push_back('/');
      }
      ret += defs[z]->name;
    }
    return ret;
  }
}

std::string MapFile::name_for_object_type(uint16_t type, Version version, uint8_t area) {
  auto defs = definitions_for_entity_type(default_object_definitions(), type, version, area);
  return name_for_entity_definitions(type, defs);
}
std::string MapFile::name_for_enemy_type(uint16_t type, Version version, uint8_t area) {
  auto defs = definitions_for_entity_type(default_enemy_definitions(), type, version, area);
  return name_for_entity_definitions(type, defs);
}

std::string MapFile::ObjectSetEntry::str(Version version, uint8_t area) const {
  auto defs = definitions_for_entity_type(default_object_definitions(), this->base_type, version, area);

  std::vector<std::string> tokens;
  tokens.emplace_back(std::format("type=0x{:04X}", this->base_type));
  if (!defs.empty()) {
    tokens.emplace_back(std::format("({})", name_for_entity_definitions(this->base_type, defs)));
  }
  tokens.emplace_back(std::format("floor=0x{:02X}", this->floor));
  if (this->group) {
    tokens.emplace_back(std::format("group={}", this->group));
  }
  tokens.emplace_back(std::format("room={}", this->room));
  tokens.emplace_back(std::format("pos=({:g}, {:g}, {:g})", this->pos.x, this->pos.y, this->pos.z));
  tokens.emplace_back(std::format("angle=({}, {}, {})", this->angle.x, this->angle.y, this->angle.z));

  auto format_param_tokens = [this, version](const DATEntityDefinition* def) -> std::vector<std::string> {
    auto env_lookup = [this, version](const std::string& name) -> ResourceDASM::Expression::Value {
      if (name == "floor") {
        return static_cast<int64_t>(this->floor.load());
      } else if (name == "group") {
        return static_cast<int64_t>(this->group.load());
      } else if (name == "angle.x") {
        return static_cast<int64_t>(this->angle.x.load());
      } else if (name == "angle.y") {
        return static_cast<int64_t>(this->angle.y.load());
      } else if (name == "angle.z") {
        return static_cast<int64_t>(this->angle.z.load());
      } else if (name == "param1") {
        return static_cast<double>(this->param1.load());
      } else if (name == "param2") {
        return static_cast<double>(this->param2.load());
      } else if (name == "param3") {
        return static_cast<double>(this->param3.load());
      } else if (name == "param4") {
        return static_cast<int64_t>(this->param4.load());
      } else if (name == "param5") {
        return static_cast<int64_t>(this->param5.load());
      } else if (name == "param6") {
        return static_cast<int64_t>(this->param6.load());
      } else if (name == "is_v1") {
        return static_cast<int64_t>(is_v1(version));
      } else if (name == "is_v2") {
        return static_cast<int64_t>(is_v2(version));
      } else if (name == "is_v3") {
        return static_cast<int64_t>(is_v3(version));
      } else if (name == "is_v4") {
        return static_cast<int64_t>(is_v4(version));
      } else {
        throw std::runtime_error(std::format("Invalid object parameter expression: {}", name));
      }
    };
    std::vector<std::string> tokens;
    for (const auto& param_def : def->params) {
      if (param_def.requires_expr && !param_def.requires_expr->evaluate(env_lookup).truth_value()) {
        continue;
      }
      auto v = param_def.expr->evaluate(env_lookup);
      if (param_def.render_as_hex && v.is_int()) {
        tokens.emplace_back(std::format("{}=0x{:08X}", param_def.name, v.as_int()));
      } else {
        tokens.emplace_back(std::format("{}={}", param_def.name, v.str()));
      }
    }
    return tokens;
  };

  if (defs.size() == 0) {
    tokens.emplace_back(std::format("params=[{:g} {:g} {:g} 0x{:08X} 0x{:08X} 0x{:08X}]]",
        this->param1, this->param2, this->param3, this->param4, this->param5, this->param6));
  } else if (defs.size() == 1) {
    auto param_tokens = format_param_tokens(defs[0]);
    tokens.insert(
        tokens.end(), std::make_move_iterator(param_tokens.begin()), std::make_move_iterator(param_tokens.end()));
  } else {
    for (const auto& def : defs) {
      tokens.emplace_back(std::format("{}:[{}]", def->name, phosg::join(format_param_tokens(def), " ")));
    }
  }

  return std::format("[ObjectSetEntry {}]", phosg::join(tokens, " "));
}

uint64_t MapFile::ObjectSetEntry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->group, sizeof(this->group), ret);
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->param1, sizeof(this->param1), ret);
  ret = phosg::fnv1a64(&this->param2, sizeof(this->param2), ret);
  ret = phosg::fnv1a64(&this->param3, sizeof(this->param3), ret);
  ret = phosg::fnv1a64(&this->param4, sizeof(this->param4), ret);
  ret = phosg::fnv1a64(&this->param5, sizeof(this->param5), ret);
  ret = phosg::fnv1a64(&this->param6, sizeof(this->param6), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

std::string MapFile::EnemySetEntry::str(Version version, uint8_t area) const {
  auto defs = definitions_for_entity_type(default_enemy_definitions(), this->base_type, version, area);

  std::vector<std::string> tokens;
  tokens.emplace_back(std::format("type=0x{:04X}", this->base_type));
  if (!defs.empty()) {
    tokens.emplace_back(std::format("({})", name_for_entity_definitions(this->base_type, defs)));
  }
  tokens.emplace_back(std::format("num_children={}", this->num_children));
  tokens.emplace_back(std::format("floor=0x{:02X}", this->floor));
  tokens.emplace_back(std::format("room={}", this->room));
  tokens.emplace_back(std::format("wave_number={}", this->wave_number));
  if (this->wave_number2 != this->wave_number) {
    tokens.emplace_back(std::format("wave_number2={}", this->wave_number2));
  }
  tokens.emplace_back(std::format("pos=({:g}, {:g}, {:g})", this->pos.x, this->pos.y, this->pos.z));
  tokens.emplace_back(std::format("angle=({}, {}, {})", this->angle.x, this->angle.y, this->angle.z));

  auto format_param_tokens = [this, version](const DATEntityDefinition* def) -> std::vector<std::string> {
    auto env_lookup = [this, version](const std::string& name) -> ResourceDASM::Expression::Value {
      if (name == "floor") {
        return static_cast<int64_t>(this->floor.load());
      } else if (name == "num_children") {
        return static_cast<int64_t>(this->num_children.load());
      } else if (name == "wave_number") {
        return static_cast<int64_t>(this->wave_number.load());
      } else if (name == "angle.x") {
        return static_cast<int64_t>(this->angle.x.load());
      } else if (name == "angle.y") {
        return static_cast<int64_t>(this->angle.y.load());
      } else if (name == "angle.z") {
        return static_cast<int64_t>(this->angle.z.load());
      } else if (name == "param1") {
        return static_cast<double>(this->param1.load());
      } else if (name == "param2") {
        return static_cast<double>(this->param2.load());
      } else if (name == "param3") {
        return static_cast<double>(this->param3.load());
      } else if (name == "param4") {
        return static_cast<double>(this->param4.load());
      } else if (name == "param5") {
        return static_cast<double>(this->param5.load());
      } else if (name == "param6") {
        return static_cast<int64_t>(this->param6.load());
      } else if (name == "param7") {
        return static_cast<int64_t>(this->param7.load());
      } else if (name == "is_v1") {
        return static_cast<int64_t>(is_v1(version));
      } else if (name == "is_v2") {
        return static_cast<int64_t>(is_v2(version));
      } else if (name == "is_v3") {
        return static_cast<int64_t>(is_v3(version));
      } else if (name == "is_v4") {
        return static_cast<int64_t>(is_v4(version));
      } else {
        throw std::runtime_error(std::format("Invalid enemy parameter expression: {}", name));
      }
    };
    std::vector<std::string> tokens;
    for (const auto& param_def : def->params) {
      if (param_def.requires_expr && !param_def.requires_expr->evaluate(env_lookup).truth_value()) {
        continue;
      }
      auto v = param_def.expr->evaluate(env_lookup);
      if (param_def.render_as_hex && v.is_int()) {
        tokens.emplace_back(std::format("{}=0x{:08X}", param_def.name, v.as_int()));
      } else {
        tokens.emplace_back(std::format("{}={}", param_def.name, v.str()));
      }
    }
    return tokens;
  };

  if (defs.size() == 0) {
    tokens.emplace_back(std::format("params=[{:g} {:g} {:g} {:g} {:g} 0x{:04X} 0x{:04X}]",
        this->param1, this->param2, this->param3, this->param4, this->param5, this->param6, this->param7));
  } else if (defs.size() == 1) {
    auto param_tokens = format_param_tokens(defs[0]);
    tokens.insert(
        tokens.end(), std::make_move_iterator(param_tokens.begin()), std::make_move_iterator(param_tokens.end()));
  } else {
    for (const auto& def : defs) {
      tokens.emplace_back(std::format("{}:[{}]", def->name, phosg::join(format_param_tokens(def), " ")));
    }
  }

  return std::format("[EnemySetEntry {}]", phosg::join(tokens, " "));
}

uint64_t MapFile::EnemySetEntry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->num_children, sizeof(this->num_children), ret);
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  ret = phosg::fnv1a64(&this->wave_number2, sizeof(this->wave_number2), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->param1, sizeof(this->param1), ret);
  ret = phosg::fnv1a64(&this->param2, sizeof(this->param2), ret);
  ret = phosg::fnv1a64(&this->param3, sizeof(this->param3), ret);
  ret = phosg::fnv1a64(&this->param4, sizeof(this->param4), ret);
  ret = phosg::fnv1a64(&this->param5, sizeof(this->param5), ret);
  ret = phosg::fnv1a64(&this->param6, sizeof(this->param6), ret);
  ret = phosg::fnv1a64(&this->param7, sizeof(this->param7), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

std::string MapFile::Event1Entry::str() const {
  return std::format("[Event1Entry event_id=0x{:08X} flags=0x{:04X} event_type={} room={} wave_number={} delay={} action_stream_offset=0x{:X}]",
      this->event_id,
      this->flags,
      this->event_type,
      this->room,
      this->wave_number,
      this->delay,
      this->action_stream_offset);
}

uint64_t MapFile::Event1Entry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->event_id, sizeof(this->event_id));
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

std::string MapFile::Event2Entry::str() const {
  return std::format("[Event2Entry event_id=0x{:08X} flags=0x{:04X} event_type={} room={} wave_number={} min_delay={} max_delay={} min_enemies={} max_enemies={} max_waves={} action_stream_offset=0x{:X}]",
      this->event_id,
      this->flags,
      this->event_type,
      this->room,
      this->wave_number,
      this->min_delay,
      this->max_delay,
      this->min_enemies,
      this->max_enemies,
      this->max_waves,
      this->action_stream_offset);
}

std::string MapFile::RandomEnemyRoom::str() const {
  const char* count_warning_str = "";
  if (count > 0x20) {
    count_warning_str = " /* warning: count is too large */";
  }
  return std::format("[RandomEnemyRoom room={} count={}{} offset=0x{:X}(index=0x{:X})]",
      this->room_id, this->count, count_warning_str, this->offset, this->offset / sizeof(RandomEnemyRoom));
}

std::string MapFile::RandomEnemyLocation::str() const {
  return std::format("[RandomEnemyLocation pos=({:g}, {:g}, {:g}) angle=({}, {}, {}) a9={:04X} a10={:04X}]",
      this->pos.x,
      this->pos.y,
      this->pos.z,
      this->angle.x,
      this->angle.y,
      this->angle.z,
      this->unknown_a9,
      this->unknown_a10);
}

std::string MapFile::RandomEnemyDefinition::str() const {
  return std::format("[RandomEnemyDefinition params=[{:g} {:g} {:g} {:g} {:g} 0x{:04X} 0x{:04X}] entry_index={} min_children={} max_children={}]",
      this->param1,
      this->param2,
      this->param3,
      this->param4,
      this->param5,
      this->param6,
      this->param7,
      this->entry_index,
      this->min_children,
      this->max_children);
}

std::string MapFile::RandomEnemyWeight::str() const {
  std::string base_type_index_str;
  try {
    base_type_index_str = std::format("(->0x{:04X})", MapFile::RAND_ENEMY_BASE_TYPES.at(this->base_type_index));
  } catch (const std::out_of_range&) {
  }
  return std::format("[RandomEnemyWeight base_type_index={}{} def_entry_index={} weight={} a4=0x{:02X}]",
      this->base_type_index,
      base_type_index_str,
      this->def_entry_index,
      this->weight,
      this->unknown_a4);
}

MapFile::RandomState::RandomState(uint32_t random_seed)
    : random(random_seed),
      location_table_random(0),
      location_indexes_populated(0),
      location_indexes_used(0),
      location_entries_base_offset(0) {
  this->location_index_table.fill(0);
}

static constexpr float UINT32_MAX_AS_FLOAT = 4294967296.0f;

size_t MapFile::RandomState::rand_int_biased(size_t min_v, size_t max_v) {
  float max_f = static_cast<float>(max_v + 1);
  uint32_t crypt_v = this->random.next();
  float det_f = static_cast<float>(crypt_v);
  return std::max<size_t>(floorf((max_f * det_f) / UINT32_MAX_AS_FLOAT), min_v);
}

uint32_t MapFile::RandomState::next_location_index() {
  if (this->location_indexes_used < this->location_indexes_populated) {
    return this->location_index_table.at(this->location_indexes_used++);
  }
  return 0;
}

void MapFile::RandomState::generate_shuffled_location_table(
    const RandomEnemyLocationsHeader& header, phosg::StringReader r, uint16_t room_id) {
  if (header.num_rooms == 0) {
    throw std::runtime_error("no locations defined");
  }

  phosg::StringReader rooms_r = r.sub(header.room_table_offset, header.num_rooms * sizeof(RandomEnemyRoom));

  size_t bs_min = 0;
  size_t bs_max = header.num_rooms - 1;
  do {
    size_t bs_mid = (bs_min + bs_max) / 2;
    if (rooms_r.pget<RandomEnemyRoom>(bs_mid * sizeof(RandomEnemyRoom)).room_id < room_id) {
      bs_min = bs_mid + 1;
    } else {
      bs_max = bs_mid;
    }
  } while (bs_min < bs_max);

  const auto& sec = rooms_r.pget<RandomEnemyRoom>(bs_min * sizeof(RandomEnemyRoom));
  if (room_id != sec.room_id) {
    return;
  }

  this->location_indexes_populated = sec.count;
  this->location_indexes_used = 0;
  this->location_entries_base_offset = sec.offset;
  for (size_t z = 0; z < sec.count; z++) {
    this->location_index_table.at(z) = z;
  }

  for (size_t z = 0; z < 4; z++) {
    for (size_t x = 0; x < sec.count; x++) {
      uint32_t crypt_v = this->location_table_random.next();
      size_t choice = floorf((static_cast<float>(sec.count) * static_cast<float>(crypt_v)) / UINT32_MAX_AS_FLOAT);
      uint32_t t = this->location_index_table[x];
      this->location_index_table[x] = this->location_index_table[choice];
      this->location_index_table[choice] = t;
    }
  }
}

const std::array<uint32_t, 41> MapFile::RAND_ENEMY_BASE_TYPES = {
    0x0044, /* TObjEneBeast */
    0x0043, /* TObjEneBm5Wolf */
    0x0041, /* TObjEneLappy */
    0x0042, /* TObjEneBm3FlyNest */
    0x0040, /* TObjEneMoja */
    0x0060, /* TObjGrass */
    0x0061, /* TObjEneRe2Flower */
    0x0062, /* TObjEneNanoDrago */
    0x0063, /* TObjEneShark */
    0x0064, /* TObjEneSlime */
    0x0065, /* TObjEnePanarms */
    0x0080, /* TObjEneDubchik */
    0x0081, /* TObjEneGyaranzo */
    0x0082, /* TObjEneMe3ShinowaReal */
    0x0083, /* TObjEneMe1Canadin */
    0x0084, /* TObjEneMe1CanadinLeader */
    0x0085, /* TOCtrlDubchik */
    0x00A0, /* TObjEneSaver */
    0x00A1, /* TObjEneRe4Sorcerer */
    0x00A2, /* TObjEneDarkGunner */
    0x00A3, /* TObjEneDarkGunCenter */
    0x00A4, /* TObjEneDf2Bringer */
    0x00A5, /* TObjEneRe7Berura */
    0x00A6, /* TObjEneDimedian */
    0x00A7, /* TObjEneBalClawBody */
    0x00A8, /* TObjEneBalClawClaw */
    0x00D4, /* TObjEneMe3StelthReal */
    0x00D5, /* TObjEneMerillLia */
    0x00D6, /* TObjEneBm9Mericarol */
    0x00D7, /* TObjEneBm5GibonU */
    0x00D8, /* TObjEneGibbles */
    0x00D9, /* TObjEneMe1Gee */
    0x00DA, /* TObjEneMe1GiGue */
    0x00DB, /* TObjEneDelDepth */
    0x00DC, /* TObjEneDellBiter */
    0x00DD, /* TObjEneDolmOlm */
    0x00DE, /* TObjEneMorfos */
    0x00DF, /* TObjEneRecobox */
    // This is not a bug; 0x00E0 really does appear twice in this list on the client.
    0x00E0, /* TObjEneMe3SinowZoaReal/TObjEneEpsilonBody (depending on area) */
    0x00E0, /* TObjEneMe3SinowZoaReal/TObjEneEpsilonBody (depending on area) */
    0x00E1, /* TObjEneIllGill */
};

MapFile::MapFile(std::shared_ptr<const std::string> data) {
  for (size_t z = 0; z < this->sections_for_floor.size(); z++) {
    this->sections_for_floor[z].floor = z;
  }

  this->quest_data = data;
  this->link_data(data);

  phosg::StringReader r(data->data(), data->size());
  while (!r.eof()) {
    const auto& header = r.get<SectionHeader>();

    if (header.type() == SectionHeader::Type::END && header.section_size == 0) {
      break;
    }
    if (header.section_size < sizeof(header)) {
      throw std::runtime_error(std::format("quest entities list has invalid section header at offset 0x{:X}", r.where() - sizeof(header)));
    }

    if (header.floor >= this->sections_for_floor.size()) {
      throw std::runtime_error("section floor number too large");
    }

    size_t section_offset = r.where();
    switch (header.type()) {
      case SectionHeader::Type::OBJECT_SETS:
        this->set_object_sets_for_floor(header.floor, section_offset, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::ENEMY_SETS:
        this->set_enemy_sets_for_floor(header.floor, section_offset, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::EVENTS:
        this->set_events_for_floor(header.floor, section_offset, r.getv(header.data_size), header.data_size, true);
        break;
      case SectionHeader::Type::RANDOM_ENEMY_LOCATIONS:
        this->set_random_enemy_locations_for_floor(header.floor, section_offset, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::RANDOM_ENEMY_DEFINITIONS:
        this->set_random_enemy_definitions_for_floor(header.floor, section_offset, r.getv(header.data_size), header.data_size);
        break;
      default:
        throw std::runtime_error("invalid section type");
    }
  }
  this->compute_floor_start_indexes();
}

MapFile::MapFile(
    uint8_t floor,
    std::shared_ptr<const std::string> objects_data,
    std::shared_ptr<const std::string> enemies_data,
    std::shared_ptr<const std::string> events_data) {
  for (size_t z = 0; z < this->sections_for_floor.size(); z++) {
    this->sections_for_floor[z].floor = z;
  }
  if (objects_data) {
    this->link_data(objects_data);
    this->set_object_sets_for_floor(floor, 0, objects_data->data(), objects_data->size());
  }
  if (enemies_data) {
    this->link_data(enemies_data);
    this->set_enemy_sets_for_floor(floor, 0, enemies_data->data(), enemies_data->size());
  }
  if (events_data) {
    this->link_data(events_data);
    this->set_events_for_floor(floor, 0, events_data->data(), events_data->size(), false);
  }
  this->compute_floor_start_indexes();
}

MapFile::MapFile(uint32_t generated_with_random_seed) : generated_with_random_seed(generated_with_random_seed) {
  for (size_t z = 0; z < this->sections_for_floor.size(); z++) {
    this->sections_for_floor[z].floor = z;
  }
}

void MapFile::link_data(std::shared_ptr<const std::string> data) {
  if (this->linked_data.emplace(data).second) {
    this->linked_data_hash ^= phosg::fnv1a64(*data);
  }
}

void MapFile::set_object_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.object_sets) {
    throw std::runtime_error("multiple object sets sections for same floor");
  }
  if (size % sizeof(ObjectSetEntry)) {
    throw std::runtime_error("object sets section size is not a multiple of entry size");
  }
  floor_sections.object_sets = reinterpret_cast<const ObjectSetEntry*>(data);
  floor_sections.object_set_count = size / sizeof(ObjectSetEntry);
  floor_sections.object_sets_file_offset = file_offset;
  floor_sections.object_sets_file_size = size;
}

void MapFile::set_enemy_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.enemy_sets) {
    throw std::runtime_error("multiple enemy sets sections for same floor");
  }
  if (floor_sections.events2 || floor_sections.random_enemy_definitions || floor_sections.random_enemy_locations) {
    throw std::runtime_error("floor already has random enemies and cannot also have fixed enemies");
  }
  if (size % sizeof(EnemySetEntry)) {
    throw std::runtime_error("enemy sets section size is not a multiple of entry size");
  }
  floor_sections.enemy_sets = reinterpret_cast<const EnemySetEntry*>(data);
  floor_sections.enemy_set_count = size / sizeof(EnemySetEntry);
  floor_sections.enemy_sets_file_offset = file_offset;
  floor_sections.enemy_sets_file_size = size;
}

void MapFile::set_events_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size, bool allow_evt2) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.events_data || floor_sections.events1 || floor_sections.events2 || floor_sections.event_action_stream) {
    throw std::runtime_error("multiple events sections for same floor");
  }

  floor_sections.events_data = data;
  floor_sections.events_data_size = size;
  floor_sections.events_file_offset = file_offset;
  floor_sections.events_file_size = size;

  phosg::StringReader r(data, size);
  const auto& events_header = r.get<EventsSectionHeader>();
  floor_sections.event_count = events_header.entry_count;
  if (events_header.is_evt2()) {
    if (!allow_evt2) {
      throw std::runtime_error("random events cannot be used in this context");
    }
    if (floor_sections.enemy_sets) {
      throw std::runtime_error("floor already has fixed enemies and cannot also have random enemies");
    }
    floor_sections.events2 = &r.pget<Event2Entry>(
        events_header.entries_offset, events_header.entry_count * sizeof(Event2Entry));
    this->has_any_random_sections = true;
  } else {
    floor_sections.events1 = &r.pget<Event1Entry>(
        events_header.entries_offset, events_header.entry_count * sizeof(Event1Entry));
  }
  floor_sections.event_action_stream_bytes = size - events_header.action_stream_offset;
  floor_sections.event_action_stream = r.pgetv(
      events_header.action_stream_offset, floor_sections.event_action_stream_bytes);
}

void MapFile::set_random_enemy_locations_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.random_enemy_locations_data) {
    throw std::runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_locations_data = data;
  floor_sections.random_enemy_locations_data_size = size;
  floor_sections.random_enemy_locations_file_offset = file_offset;
  floor_sections.random_enemy_locations_file_size = size;
  this->has_any_random_sections = true;

  phosg::StringReader r(data, size);
  const auto& header = r.get<RandomEnemyLocationsHeader>();
  floor_sections.random_enemy_room_count = header.num_rooms;
  floor_sections.random_enemy_rooms = &r.pget<RandomEnemyRoom>(
      header.room_table_offset, floor_sections.random_enemy_room_count * sizeof(RandomEnemyRoom));
  size_t max_offset = 0;
  for (size_t z = 0; z < floor_sections.random_enemy_room_count; z++) {
    const auto& sec = floor_sections.random_enemy_rooms[z];
    if (sec.offset % sizeof(RandomEnemyLocation)) {
      // TODO: We probably could support this if it's actually needed somewhere
      throw std::runtime_error(std::format("random enemy location offset {:08X} is not a multiple of struct size {:08X}",
          sec.offset, sizeof(RandomEnemyLocation)));
    }
    max_offset = std::max<size_t>(max_offset, sec.offset + sec.count * sizeof(RandomEnemyLocation));
  }
  floor_sections.random_enemy_location_count = max_offset / sizeof(RandomEnemyLocation);
  floor_sections.random_enemy_locations = &r.pget<RandomEnemyLocation>(
      header.entries_offset, floor_sections.random_enemy_location_count * sizeof(RandomEnemyLocation));
}

void MapFile::set_random_enemy_definitions_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.random_enemy_definitions_data) {
    throw std::runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_definitions_data = data;
  floor_sections.random_enemy_definitions_data_size = size;
  floor_sections.random_enemy_definitions_file_offset = file_offset;
  floor_sections.random_enemy_definitions_file_size = size;
  this->has_any_random_sections = true;

  phosg::StringReader r(data, size);
  const auto& header = r.get<RandomEnemyDefinitionsHeader>();
  floor_sections.random_enemy_definition_count = header.entry_count;
  floor_sections.random_enemy_definitions = &r.pget<RandomEnemyDefinition>(
      header.entries_offset, floor_sections.random_enemy_definition_count * sizeof(RandomEnemyDefinition));
  floor_sections.random_enemy_weight_count = header.weight_entry_count;
  floor_sections.random_enemy_weights = &r.pget<RandomEnemyWeight>(
      header.weight_entries_offset, floor_sections.random_enemy_weight_count * sizeof(RandomEnemyWeight));
}

std::shared_ptr<const MapFile> MapFile::materialize_random_sections(uint32_t random_seed) const {
  return const_cast<MapFile*>(this)->materialize_random_sections(random_seed);
}

std::shared_ptr<MapFile> MapFile::materialize_random_sections(uint32_t random_seed) {
  if (!this->has_any_random_sections) {
    return this->shared_from_this();
  }

  static_game_data_log.debug_f("Generating random enemies using seed {:08X}", random_seed);
  auto new_map = std::make_shared<MapFile>(random_seed);
  RandomState random_state(random_seed);

  for (uint8_t floor = 0; floor < 0x12; floor++) {
    const auto& this_sf = this->sections_for_floor[floor];

    if (this_sf.object_sets) {
      static_game_data_log.debug_f("(Floor {}) Using existing object sets", floor);
      new_map->set_object_sets_for_floor(floor, 0, this_sf.object_sets, this_sf.object_set_count * sizeof(ObjectSetEntry));
    }

    if (this_sf.enemy_sets) {
      static_game_data_log.debug_f("(Floor {}) Using existing enemy sets", floor);
      new_map->set_enemy_sets_for_floor(floor, 0, this_sf.enemy_sets, this_sf.enemy_set_count * sizeof(EnemySetEntry));
    }

    if (this_sf.events1) {
      static_game_data_log.debug_f("(Floor {}) Using existing events (format 1; fixed)", floor);
      new_map->set_events_for_floor(floor, 0, this_sf.events_data, this_sf.events_data_size, false);

    } else if (this_sf.events2) {
      static_game_data_log.debug_f("(Floor {}) Using existing events (format 2; random)", floor);

      if (!this_sf.random_enemy_locations_data || !this_sf.random_enemy_definitions_data) {
        throw std::runtime_error("cannot materialize random enemies; evt2 section present but one or both random data sections are missing");
      }
      if (!this_sf.event_action_stream) {
        throw std::runtime_error("cannot materialize random enemies; action stream is missing");
      }

      phosg::StringReader locations_sec_r(this_sf.random_enemy_locations_data, this_sf.random_enemy_locations_data_size);
      phosg::StringReader definitions_sec_r(this_sf.random_enemy_definitions_data, this_sf.random_enemy_definitions_data_size);
      const auto& locations_header = locations_sec_r.get<RandomEnemyLocationsHeader>();
      const auto& definitions_header = definitions_sec_r.get<RandomEnemyDefinitionsHeader>();
      auto definitions_r = definitions_sec_r.sub(
          definitions_header.entries_offset, definitions_header.entry_count * sizeof(RandomEnemyDefinition));
      auto weights_r = definitions_sec_r.sub(
          definitions_header.weight_entries_offset, definitions_header.weight_entry_count * sizeof(RandomEnemyWeight));

      // Note: The original implementation (Sega's) computes this sum within the `while (remaining_enemies)` loop
      // below; however, this sum depends only on the event data section and not the wave number, room ID, or anything
      // else that could change during the below code, so compute it only once here instead.
      weights_r.go(0);
      size_t weight_total = 0;
      while (!weights_r.eof()) {
        weight_total += weights_r.get<RandomEnemyWeight>().weight;
      }

      phosg::StringWriter enemy_sets_w;
      phosg::StringWriter events1_w;
      phosg::StringWriter action_stream_w;
      action_stream_w.write(this_sf.event_action_stream, this_sf.event_action_stream_bytes);
      if (static_game_data_log.debug_f("(Floor {}) Added existing action stream data:", floor)) {
        phosg::print_data(stderr, this_sf.event_action_stream, this_sf.event_action_stream_bytes);
      }

      for (size_t source_event_index = 0; source_event_index < this_sf.event_count; source_event_index++) {
        const auto& source_event2 = this_sf.events2[source_event_index];
        static_game_data_log.debug_f("(Floor {} event {}) ... {}", floor, source_event_index, source_event2.str());

        size_t remaining_waves = random_state.rand_int_biased(1, source_event2.max_waves);
        static_game_data_log.debug_f("(Floor {} event {}) Chose {} waves", floor, source_event_index, remaining_waves);

        le_uint32_t wave_next_event_id = source_event2.event_id;
        uint32_t wave_number = source_event2.wave_number;
        while (remaining_waves) {
          remaining_waves--;

          size_t remaining_enemies = random_state.rand_int_biased(source_event2.min_enemies, source_event2.max_enemies);
          static_game_data_log.debug_f("(Floor {} event {} wave {}) Chose {} enemies",
              floor, source_event_index, remaining_waves, remaining_enemies);

          random_state.generate_shuffled_location_table(locations_header, locations_sec_r, source_event2.room);

          while (remaining_enemies) {
            remaining_enemies--;

            size_t det = random_state.rand_int_biased(0, weight_total - 1);
            static_game_data_log.debug_f("(Floor {} event {} wave {} enemy {}) det={}, weight_total={}",
                floor, source_event_index, remaining_waves, remaining_enemies, det, weight_total);

            weights_r.go(0);
            while (!weights_r.eof()) {
              const auto& weight_entry = weights_r.get<RandomEnemyWeight>();
              if (det < weight_entry.weight) {
                static_game_data_log.debug_f("(Floor {} event {} wave {} enemy {}) This results in weight entry {}",
                    floor, source_event_index, remaining_waves, remaining_enemies, weight_entry.str());
                if ((weight_entry.base_type_index != 0xFF) && (weight_entry.def_entry_index != 0xFF)) {
                  if (definitions_header.entry_count == 0) {
                    throw std::runtime_error("no available random enemy definitions");
                  }

                  EnemySetEntry e;
                  e.base_type = this->RAND_ENEMY_BASE_TYPES.at(weight_entry.base_type_index);
                  e.wave_number = wave_number;
                  e.room = source_event2.room;
                  e.floor = floor;

                  size_t bs_min = 0;
                  size_t bs_max = definitions_header.entry_count - 1;
                  do {
                    size_t bs_mid = (bs_min + bs_max) / 2;
                    if (definitions_r.pget<RandomEnemyDefinition>(bs_mid * sizeof(RandomEnemyDefinition)).entry_index < weight_entry.def_entry_index) {
                      bs_min = bs_mid + 1;
                    } else {
                      bs_max = bs_mid;
                    }
                  } while (bs_min < bs_max);

                  const auto& def = definitions_r.pget<RandomEnemyDefinition>(bs_min * sizeof(RandomEnemyDefinition));
                  if (def.entry_index == weight_entry.def_entry_index) {
                    static_game_data_log.debug_f("(Floor {} event {} wave {} enemy {}) Using parameters from {}",
                        floor, source_event_index, remaining_waves, remaining_enemies, def.str());
                    e.param1 = def.param1;
                    e.param2 = def.param2;
                    e.param3 = def.param3;
                    e.param4 = def.param4;
                    e.param5 = def.param5;
                    e.param6 = def.param6;
                    e.param7 = def.param7;
                    // Note: The original code calls this with (min_children, max_children + 1); we add 1 inside the
                    // function instead, which is functionally equivalent.
                    e.num_children = random_state.rand_int_biased(def.min_children, def.max_children);
                  } else {
                    throw std::runtime_error("random enemy definition not found");
                  }

                  const auto& loc = locations_sec_r.pget<RandomEnemyLocation>(
                      locations_header.entries_offset + sizeof(RandomEnemyLocation) * random_state.next_location_index());
                  e.pos = loc.pos;
                  e.angle = loc.angle;

                  enemy_sets_w.put<EnemySetEntry>(e);
                }
                break;
              } else {
                det -= weight_entry.weight;
              }
            }
          }
          if (remaining_waves) {
            Event1Entry event;
            event.event_id = wave_next_event_id;
            event.flags = source_event2.flags;
            event.event_type = source_event2.event_type;
            event.room = source_event2.room;
            event.wave_number = wave_number;
            event.delay = random_state.rand_int_biased(source_event2.min_delay, source_event2.max_delay);
            event.action_stream_offset = action_stream_w.size();
            events1_w.put<Event1Entry>(event);

            wave_next_event_id = source_event2.event_id + wave_number + 10000;
            action_stream_w.put_u8(0x0C); // trigger_event
            action_stream_w.put_u32l(wave_next_event_id);
            action_stream_w.put_u8(0x01); // stop

            static_game_data_log.debug_f("(Floor {} event {} wave {}) Added event {} which triggers {}",
                floor, source_event_index, remaining_waves, event.str(), wave_next_event_id);

            wave_number++;
          }
        }

        Event1Entry event;
        event.event_id = wave_next_event_id;
        event.flags = source_event2.flags;
        event.event_type = source_event2.event_type;
        event.room = source_event2.room;
        event.wave_number = wave_number;
        event.delay = random_state.rand_int_biased(source_event2.min_delay, source_event2.max_delay);
        event.action_stream_offset = source_event2.action_stream_offset;
        events1_w.put<Event1Entry>(event);

        static_game_data_log.debug_f("(Floor {} event {} wave {}) Added final event {}",
            floor, source_event_index, remaining_waves, event.str());

        wave_number++;
      }

      phosg::StringWriter events1_sec_w;
      events1_sec_w.put<EventsSectionHeader>(EventsSectionHeader{
          .action_stream_offset = sizeof(EventsSectionHeader) + events1_w.size(),
          .entries_offset = sizeof(EventsSectionHeader),
          .entry_count = events1_w.size() / sizeof(Event1Entry),
          .format = 0,
      });
      events1_sec_w.write(events1_w.str());
      events1_sec_w.write(action_stream_w.str());

      auto enemy_sets_sec_data = std::make_shared<std::string>(std::move(enemy_sets_w.str()));
      new_map->link_data(enemy_sets_sec_data);
      new_map->set_enemy_sets_for_floor(floor, 0, enemy_sets_sec_data->data(), enemy_sets_sec_data->size());

      auto events1_sec_data = std::make_shared<std::string>(std::move(events1_sec_w.str()));
      new_map->link_data(events1_sec_data);
      new_map->set_events_for_floor(floor, 0, events1_sec_data->data(), events1_sec_data->size(), false);
    }
  }

  // Add everything in this->linked_data to the new map's linked_data, since it likely is referenced by pointers in
  // sections_for_floor
  new_map->quest_data = this->quest_data;
  for (const auto& it : this->linked_data) {
    new_map->link_data(it);
  }
  new_map->compute_floor_start_indexes();

  return new_map;
}

void MapFile::compute_floor_start_indexes() {
  auto& first_sf = this->sections_for_floor[0];
  first_sf.first_object_set_index = 0;
  first_sf.first_enemy_set_index = 0;
  first_sf.first_event_set_index = 0;
  for (size_t floor = 1; floor < this->sections_for_floor.size(); floor++) {
    const auto& prev_sf = this->sections_for_floor[floor - 1];
    auto& this_sf = this->sections_for_floor[floor];
    this_sf.first_object_set_index = prev_sf.first_object_set_index + prev_sf.object_set_count;
    this_sf.first_enemy_set_index = prev_sf.first_enemy_set_index + prev_sf.enemy_set_count;
    this_sf.first_event_set_index = prev_sf.first_event_set_index + prev_sf.event_count;
  }
}

size_t MapFile::count_object_sets() const {
  size_t ret = 0;
  for (const auto& fc : this->sections_for_floor) {
    ret += fc.object_set_count;
  }
  return ret;
}

size_t MapFile::count_enemy_sets() const {
  size_t ret = 0;
  for (const auto& fc : this->sections_for_floor) {
    ret += fc.enemy_set_count;
  }
  return ret;
}

size_t MapFile::count_events() const {
  size_t ret = 0;
  for (const auto& fc : this->sections_for_floor) {
    ret += fc.event_count;
  }
  return ret;
}

std::string MapFile::disassemble_action_stream(const void* data, size_t size) {
  std::deque<std::string> ret;
  phosg::StringReader r(data, size);

  while (!r.eof()) {
    uint8_t opcode = r.get_u8();
    switch (opcode) {
      case 0x00:
        ret.emplace_back(std::format("  00            nop"));
        break;
      case 0x01:
        ret.emplace_back(std::format("  01            stop"));
        r.go(r.size());
        break;
      case 0x08: {
        uint16_t room = r.get_u16l();
        uint16_t group = r.get_u16l();
        ret.emplace_back(std::format("  08 {:04X} {:04X}  construct_objects       room={:04X} group={:04X}",
            room, group, room, group));
        break;
      }
      case 0x09: {
        uint16_t room = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(std::format("  09 {:04X} {:04X}  construct_enemies       room={:04X} wave_number={:04X}",
            room, wave_number, room, wave_number));
        break;
      }
      case 0x0A: {
        uint16_t id = r.get_u16l();
        ret.emplace_back(std::format("  0A {:04X}       set_switch_flag         id={:04X}", id, id));
        break;
      }
      case 0x0B: {
        uint16_t id = r.get_u16l();
        ret.emplace_back(std::format("  0B {:04X}       clear_switch_flag       id={:04X}", id, id));
        break;
      }
      case 0x0C: {
        uint32_t event_id = r.get_u32l();
        ret.emplace_back(std::format("  0C {:08X}   trigger_event           event_id={:08X}", event_id, event_id));
        break;
      }
      case 0x0D: {
        uint16_t room = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(std::format("  0D {:04X} {:04X}  construct_enemies_stop  room={:04X} wave_number={:04X}",
            room, wave_number, room, wave_number));
        r.go(r.size());
        break;
      }
      default:
        ret.emplace_back(std::format("  {:02X}            .invalid", opcode));
    }
  }

  return phosg::join(ret, "\n");
}

std::string MapFile::disassemble(bool reassembly, Version version) const {
  std::deque<std::string> ret;
  for (size_t floor = 0; floor < this->sections_for_floor.size(); floor++) {
    const auto& sf = this->sections_for_floor[floor];
    phosg::StringReader as_r(sf.event_action_stream, sf.event_action_stream_bytes);

    if (sf.object_sets) {
      if (reassembly) {
        ret.emplace_back(std::format(".object_sets {}", floor));
      } else {
        ret.emplace_back(std::format(".object_sets {} /* offset 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.object_sets_file_offset, sf.object_sets_file_size));
      }
      for (size_t z = 0; z < sf.object_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.object_sets[z].str(version));
        } else {
          size_t k_id = z + sf.first_object_set_index;
          ret.emplace_back(std::format("/* K-{:03X} */ {}", k_id, sf.object_sets[z].str(version)));
        }
      }
      ret.emplace_back();
    }

    if (sf.enemy_sets) {
      if (reassembly) {
        ret.emplace_back(std::format(".enemy_sets {}", floor));
      } else {
        ret.emplace_back(std::format(".enemy_sets {} /* offset 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.enemy_sets_file_offset, sf.enemy_sets_file_size));
      }
      for (size_t z = 0; z < sf.enemy_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.enemy_sets[z].str(version));
        } else {
          size_t s_id = z + sf.first_enemy_set_index;
          ret.emplace_back(std::format("/* S-{:03X} */ {}", s_id, sf.enemy_sets[z].str(version)));
        }
      }
      ret.emplace_back();
    }

    if (sf.events1) {
      if (reassembly) {
        ret.emplace_back(std::format(".events {}", floor));
      } else {
        ret.emplace_back(std::format(".events {} /* offset 0x{:X} in file; 0x{:X} bytes; 0x{:X} bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events1[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          size_t w_id = z + sf.first_event_set_index;
          ret.emplace_back(std::format("/* W-{:03X} */ {}", w_id, ev.str()));
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(std::format(
              "  // WARNING: Event action stream offset (0x{:X}) is outside of this section",
              ev.action_stream_offset));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
      ret.emplace_back();
    }

    if (sf.events2) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_events {}", floor));
      } else {
        ret.emplace_back(std::format(
            ".random_events {} /* offset 0x{:X} in file; 0x{:X} bytes; 0x{:X} bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events2[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          ret.emplace_back(std::format("/* index {:04X} */ {}", z, ev.str()));
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(std::format(
              "  // WARNING: Event action stream offset (0x{:X}) is outside of this section",
              ev.action_stream_offset));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
      ret.emplace_back();
    }

    if (sf.random_enemy_locations_file_size > 0) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_enemy_locations {}", floor));
      } else {
        ret.emplace_back(std::format(".random_enemy_locations {} /* offset 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.random_enemy_locations_file_offset, sf.random_enemy_locations_file_size));
      }
      uint16_t last_room_id = 0;
      for (size_t z = 0; z < sf.random_enemy_room_count; z++) {
        const auto& sec = sf.random_enemy_rooms[z];
        const std::string& warning_text = (sec.room_id < last_room_id) ? " /* warning: room IDs out of order */" : "";
        last_room_id = sec.room_id;
        if (reassembly) {
          ret.emplace_back(std::format("{}{}", sec.str(), warning_text));
        } else {
          ret.emplace_back(std::format("/* room index {:04X} */ {}{}", z, sec.str(), warning_text));
        }
      }
      for (size_t z = 0; z < sf.random_enemy_location_count; z++) {
        const auto& ent = sf.random_enemy_locations[z];
        if (reassembly) {
          ret.emplace_back(ent.str());
        } else {
          ret.emplace_back(std::format("/* entry index {:04X} */ {}", z, ent.str()));
        }
      }
      ret.emplace_back();
    }

    if (sf.random_enemy_definitions_file_size > 0) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_enemy_definitions {}", floor));
      } else {
        ret.emplace_back(std::format(".random_enemy_definitions {} /* offset 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.random_enemy_definitions_file_offset, sf.random_enemy_definitions_file_size));
      }
      for (size_t z = 0; z < sf.random_enemy_definition_count; z++) {
        const auto& def = sf.random_enemy_definitions[z];
        if (reassembly) {
          ret.emplace_back(def.str());
        } else {
          ret.emplace_back(std::format("/* definition index {:04X} */ {}", z, def.str()));
        }
      }
      for (size_t z = 0; z < sf.random_enemy_weight_count; z++) {
        const auto& weight = sf.random_enemy_weights[z];
        if (reassembly) {
          ret.emplace_back(weight.str());
        } else {
          ret.emplace_back(std::format("/* weight index {:04X} */ {}", z, weight.str()));
        }
      }
      ret.emplace_back();
    }
  }

  return phosg::join(ret, "\n");
}

std::string MapFile::serialize() const {
  using T = SectionHeader::Type;
  phosg::StringWriter w;

  auto write_section_header = [&](T type, uint32_t floor, uint32_t data_size) -> void {
    uint32_t section_size = data_size + sizeof(SectionHeader);
    w.put<SectionHeader>({static_cast<uint32_t>(type), section_size, floor, data_size});
  };

  // Try to serialize the sections in the same order as the input file; if the offsets are missing, serialize in
  // (floor, type) order
  struct SectionOrderEntry {
    T type;
    uint8_t floor;
    size_t file_offset;
    bool operator<(const SectionOrderEntry& other) const {
      if (this->file_offset < other.file_offset) {
        return true;
      } else if (this->file_offset > other.file_offset) {
        return false;
      }
      if (this->floor < other.floor) {
        return true;
      } else if (this->floor > other.floor) {
        return false;
      }
      return static_cast<uint8_t>(this->type) < static_cast<uint8_t>(other.type);
    }
  };
  std::set<SectionOrderEntry> sections_to_serialize;
  for (size_t floor = 0; floor < this->sections_for_floor.size(); floor++) {
    const auto& sf = this->sections_for_floor[floor];
    if (sf.object_sets) {
      sections_to_serialize.emplace(SectionOrderEntry{
          T::OBJECT_SETS, static_cast<uint8_t>(floor), sf.object_sets_file_offset});
    }
    if (sf.enemy_sets) {
      sections_to_serialize.emplace(SectionOrderEntry{
          T::ENEMY_SETS, static_cast<uint8_t>(floor), sf.enemy_sets_file_offset});
    }
    if (sf.events1 || sf.events2) {
      sections_to_serialize.emplace(SectionOrderEntry{
          T::EVENTS, static_cast<uint8_t>(floor), sf.events_file_offset});
    }
    if (sf.random_enemy_room_count && sf.random_enemy_locations) {
      sections_to_serialize.emplace(SectionOrderEntry{
          T::RANDOM_ENEMY_LOCATIONS, static_cast<uint8_t>(floor), sf.random_enemy_locations_file_offset});
    }
    if (sf.random_enemy_definitions && sf.random_enemy_weights) {
      sections_to_serialize.emplace(SectionOrderEntry{
          T::RANDOM_ENEMY_DEFINITIONS, static_cast<uint8_t>(floor), sf.random_enemy_definitions_file_offset});
    }
  }

  for (const auto& section : sections_to_serialize) {
    const auto& sf = this->sections_for_floor[section.floor];

    switch (section.type) {
      case T::OBJECT_SETS: {
        size_t data_size = sizeof(ObjectSetEntry) * sf.object_set_count;
        write_section_header(T::OBJECT_SETS, section.floor, data_size);
        w.write(sf.object_sets, data_size);
        break;
      }
      case T::ENEMY_SETS: {
        size_t data_size = sizeof(EnemySetEntry) * sf.enemy_set_count;
        write_section_header(T::ENEMY_SETS, section.floor, data_size);
        w.write(sf.enemy_sets, data_size);
        break;
      }
      case T::EVENTS: {
        auto serialize_events_section = [&]<typename EventT>(const EventT* events) -> void {
          EventsSectionHeader ev_header = {
              .action_stream_offset = sizeof(EventsSectionHeader) + sizeof(EventT) * sf.event_count,
              .entries_offset = sizeof(EventsSectionHeader),
              .entry_count = sf.event_count,
              .format = std::is_same_v<EventT, Event2Entry> ? 0x65767432 : 0x00000000,
          };
          size_t data_size = ev_header.action_stream_offset + sf.event_action_stream_bytes;
          data_size = (data_size + 3) & (~3);
          write_section_header(T::EVENTS, section.floor, data_size);
          w.put(ev_header);
          w.write(events, sf.event_count * sizeof(EventT));
          w.write(sf.event_action_stream, sf.event_action_stream_bytes);
          while (w.size() & 3) {
            w.put_u8(0x00);
          }
        };
        if (sf.events1) {
          serialize_events_section(sf.events1);
        } else if (sf.events2) {
          serialize_events_section(sf.events2);
        }
        break;
      }
      case T::RANDOM_ENEMY_LOCATIONS: {
        size_t data_size = sizeof(RandomEnemyLocationsHeader) +
            sf.random_enemy_room_count * sizeof(RandomEnemyRoom) +
            sf.random_enemy_location_count * sizeof(RandomEnemyLocation);
        write_section_header(T::RANDOM_ENEMY_LOCATIONS, section.floor, data_size);
        w.put(RandomEnemyLocationsHeader{
            .room_table_offset = sizeof(RandomEnemyLocationsHeader),
            .entries_offset = sizeof(RandomEnemyLocationsHeader) + sf.random_enemy_room_count * sizeof(RandomEnemyRoom),
            .num_rooms = sf.random_enemy_room_count,
        });
        w.write(sf.random_enemy_rooms, sf.random_enemy_room_count * sizeof(RandomEnemyRoom));
        w.write(sf.random_enemy_locations, sf.random_enemy_location_count * sizeof(RandomEnemyLocation));
        break;
      }
      case T::RANDOM_ENEMY_DEFINITIONS: {
        size_t data_size = sizeof(RandomEnemyDefinitionsHeader) +
            sf.random_enemy_definition_count * sizeof(RandomEnemyDefinition) +
            sf.random_enemy_weight_count * sizeof(RandomEnemyWeight);
        write_section_header(T::RANDOM_ENEMY_DEFINITIONS, section.floor, data_size);
        w.put(RandomEnemyDefinitionsHeader{
            .entries_offset = sizeof(RandomEnemyDefinitionsHeader),
            .weight_entries_offset = sizeof(RandomEnemyDefinitionsHeader) + sf.random_enemy_definition_count * sizeof(RandomEnemyDefinition),
            .entry_count = sf.random_enemy_definition_count,
            .weight_entry_count = sf.random_enemy_weight_count,
        });
        w.write(sf.random_enemy_definitions, sf.random_enemy_definition_count * sizeof(RandomEnemyDefinition));
        w.write(sf.random_enemy_weights, sf.random_enemy_weight_count * sizeof(RandomEnemyWeight));
        break;
      }
      default:
        throw std::logic_error("invalid section type descriptor");
    }
  }

  w.put<SectionHeader>({static_cast<uint32_t>(T::END), 0, 0, 0});
  return std::move(w.str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Super map

std::string SuperMap::Object::id_str() const {
  return std::format("KS-{:02X}-{:03X}", this->floor, this->super_id);
}

std::string SuperMap::Object::str() const {
  std::string ret = "[Object " + this->id_str();
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_object_index != 0xFFFF) {
      ret += std::format(" {}:[{:04X} => {}]",
          phosg::name_for_enum(v), def.relative_object_index, def.set_entry->str(v));
    }
  }
  ret += "]";
  return ret;
}

std::string SuperMap::Enemy::id_str() const {
  return std::format("ES-{:02X}-{:03X}-{:03X}", this->floor, this->super_set_id, this->super_id);
}

std::string SuperMap::Enemy::str() const {
  std::string ret = std::format("[Enemy {} type={} child_index={:X} alias_target_ene={} is_default_rare_v123={} is_default_rare_bb={}",
      this->id_str(),
      phosg::name_for_enum(this->type),
      this->child_index,
      (this->alias_target_ene ? this->alias_target_ene->id_str() : "(none)"),
      this->is_default_rare_v123 ? "true" : "false",
      this->is_default_rare_bb ? "true" : "false");
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_enemy_index != 0xFFFF) {
      ret += std::format(" {}:[{:04X}/{:04X} => {}]",
          phosg::name_for_enum(v), def.relative_set_index, def.relative_enemy_index, def.set_entry->str(v));
    }
  }
  ret += "]";
  return ret;
}

std::string SuperMap::Event::id_str() const {
  return std::format("WS-{:02X}-{:03X}", this->floor, this->super_id);
}

std::string SuperMap::Event::str() const {
  std::string ret = "[Event " + this->id_str();
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_event_index != 0xFFFF) {
      ret += std::format(
          " {}:[{:04X} => {}+{}]",
          phosg::name_for_enum(v),
          def.relative_event_index,
          def.set_entry->str(),
          phosg::format_data_string(def.action_stream, def.action_stream_size));
    }
  }
  ret += "]";
  return ret;
}

SuperMap::SuperMap(
    const std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>& map_files,
    const std::array<uint8_t, 0x12>& floor_to_area)
    : log("[SuperMap] "), floor_to_area(floor_to_area) {
  for (const auto& map_file : map_files) {
    if (!map_file) {
      continue;
    }
    if (map_file->has_random_sections()) {
      throw std::logic_error("supermap cannot be constructed from map files that contain random sections");
    }

    if (map_file->random_seed() >= 0) {
      if (this->random_seed < 0) {
        this->random_seed = map_file->random_seed();
      } else if (this->random_seed != map_file->random_seed()) {
        throw std::logic_error("supermap cannot be constructed from map files with different random seeds");
      }
    }
  }

  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& map_file = map_files.at(static_cast<size_t>(v));
    if (map_file) {
      this->add_map_file(v, map_file);
    }
  }

  this->verify(); // TODO: Remove this when no longer needed
}

static uint64_t room_index_key(uint8_t floor, uint16_t room, uint16_t wave_number) {
  return (static_cast<uint64_t>(floor) << 32) | (static_cast<uint64_t>(room) << 16) | static_cast<uint64_t>(wave_number);
}

std::shared_ptr<SuperMap::Object> SuperMap::add_object(
    Version version, uint8_t floor, const MapFile::ObjectSetEntry* set_entry) {
  auto obj = std::make_shared<Object>();
  obj->super_id = this->objects.size();
  obj->floor = floor;

  this->objects.emplace_back(obj);
  this->link_object_version(obj, version, set_entry);

  return obj;
}

void SuperMap::link_object_version(
    std::shared_ptr<Object> obj, Version version, const MapFile::ObjectSetEntry* set_entry) {
  // Add to version's entities list and set the object's per-version info
  auto& entities = this->version(version);
  auto& obj_ver = obj->version(version);
  if (obj_ver.set_entry) {
    throw std::logic_error("object already linked to version");
  }
  obj_ver.set_entry = set_entry;
  obj_ver.relative_object_index = entities.objects.size();

  entities.objects.emplace_back(obj);

  // Add to semantic hash index
  uint64_t semantic_hash = set_entry->semantic_hash(obj->floor);
  this->objects_for_semantic_hash[semantic_hash].emplace_back(obj);

  // Add to room/group index
  uint64_t k = room_index_key(obj->floor, set_entry->room, set_entry->group);
  entities.object_for_floor_room_and_group.emplace(k, obj);

  // Add to door index
  uint32_t base_switch_flag = set_entry->param4;
  uint32_t num_switch_flags = 0;
  switch (set_entry->base_type) {
    case 0x01AB: // TODoorFourLightRuins
    case 0x01C0: // TODoorFourLightSpace
    case 0x0202: // TObjDoorJung
    case 0x0221: // TODoorFourLightSeabed
    case 0x0222: // TODoorFourLightSeabedU
      num_switch_flags = set_entry->param5;
      break;
    case 0x00C1: // TODoorCave01
    case 0x0100: // TODoorMachine01
      num_switch_flags = (4 - std::clamp<size_t>(set_entry->param5, 0, 4));
      break;
    case 0x014A: // TODoorAncient08
      num_switch_flags = 4;
      break;
    case 0x014B: // TODoorAncient09
      num_switch_flags = 2;
      break;
  }
  if ((num_switch_flags > 1) && !(base_switch_flag & 0xFFFFFF00)) {
    for (size_t z = 0; z < num_switch_flags; z++) {
      entities.door_for_floor_and_switch_flag.emplace((obj->floor << 8) | (base_switch_flag + z), obj);
    }
  }
}

std::shared_ptr<SuperMap::Enemy> SuperMap::add_enemy_and_children(
    Version version, uint8_t floor, const MapFile::EnemySetEntry* set_entry) {

  std::shared_ptr<Enemy> head_ene = nullptr;
  size_t next_child_index = 0;
  auto add = [&](EnemyType type,
                 bool is_default_rare_v123 = false,
                 bool is_default_rare_bb = false,
                 std::shared_ptr<Enemy> alias_target_ene = nullptr) -> std::shared_ptr<Enemy> {
    if (alias_target_ene && alias_target_ene->alias_target_ene) {
      throw std::logic_error("alias may not point to an enemy that also is an alias");
    }

    auto& entities = this->version(version);

    // TODO: It'd be nice to share some code between this function and link_enemy_version_and_children

    // Create enemy
    auto ene = std::make_shared<Enemy>();
    ene->super_id = this->enemies.size();
    ene->child_index = next_child_index++;
    ene->super_set_id = this->enemy_sets.size() - (ene->child_index != 0);
    ene->floor = floor;
    ene->type = type;
    ene->is_default_rare_v123 = is_default_rare_v123;
    ene->is_default_rare_bb = is_default_rare_bb;
    ene->alias_target_ene = alias_target_ene;
    auto& ene_ver = ene->version(version);
    ene_ver.set_entry = set_entry;
    ene_ver.relative_enemy_index = entities.enemies.size();
    // If child_index > 0, then the head enemy was already created, so we need to subtract 1 from the set index because
    // this new enemy should have the same set index as the head enemy, but the head enemy was already added to the
    // enemy sets list.
    ene_ver.relative_set_index = entities.enemy_sets.size() - (ene->child_index != 0);

    // Add to primary enemy lists
    this->enemies.emplace_back(ene);
    entities.enemies.emplace_back(ene);
    if (ene->child_index == 0) {
      head_ene = ene;
      this->enemy_sets.emplace_back(ene);
      entities.enemy_sets.emplace_back(ene);
    }

    // Add to room/group index
    uint64_t k = room_index_key(ene->floor, set_entry->room, set_entry->wave_number);
    entities.enemy_for_floor_room_and_wave_number.emplace(k, ene);
    return ene;
  };

  // The following logic was originally based on the public version of Tethealla, created by Sodaboy. I've augmented it
  // with findings from my own research.

  EnemyType child_type = EnemyType::UNKNOWN;
  ssize_t default_num_children = 0;
  switch (set_entry->base_type) {
    case 0x0001: // TObjNpcFemaleBase
    case 0x0002: // TObjNpcFemaleChild
    case 0x0003: // TObjNpcFemaleDwarf
    case 0x0004: // TObjNpcFemaleFat
    case 0x0005: // TObjNpcFemaleMacho
    case 0x0006: // TObjNpcFemaleOld
    case 0x0007: // TObjNpcFemaleTall
    case 0x0008: // TObjNpcMaleBase
    case 0x0009: // TObjNpcMaleChild
    case 0x000A: // TObjNpcMaleDwarf
    case 0x000B: // TObjNpcMaleFat
    case 0x000C: // TObjNpcMaleMacho
    case 0x000D: // TObjNpcMaleOld
    case 0x000E: // TObjNpcMaleTall
    case 0x0019: // TObjNpcSoldierBase
    case 0x001A: // TObjNpcSoldierMacho
    case 0x001B: // TObjNpcGovernorBase
    case 0x001C: // TObjNpcConnoisseur
    case 0x001D: // TObjNpcCloakroomBase
    case 0x001E: // TObjNpcExpertBase
    case 0x001F: // TObjNpcNurseBase
    case 0x0020: // TObjNpcSecretaryBase
    case 0x0021: // TObjNpcHHM00
    case 0x0022: // TObjNpcNHW00
    case 0x0024: // TObjNpcHRM00
    case 0x0025: // TObjNpcARM00
    case 0x0026: // TObjNpcARW00
    case 0x0027: // TObjNpcHFW00
    case 0x0028: // TObjNpcNFM00
    case 0x0029: // TObjNpcNFW00
    case 0x002B: // TObjNpcNHW01
    case 0x002C: // TObjNpcAHM01
    case 0x002D: // TObjNpcHRM01
    case 0x0030: // TObjNpcHFW01
    case 0x0031: // TObjNpcNFM01
    case 0x0032: // TObjNpcNFW01
    case 0x0033: // TObjNpcEnemy
    case 0x0045: // TObjNpcLappy
    case 0x0046: // TObjNpcMoja
    case 0x00A9: // TObjNpcBringer
    case 0x00D0: // TObjNpcKenkyu
    case 0x00D1: // TObjNpcSoutokufu
    case 0x00D2: // TObjNpcHosa
    case 0x00D3: // TObjNpcKenkyuW
    case 0x00F0: // TObjNpcHosa2
    case 0x00F1: // TObjNpcKenkyu2
    case 0x00F2: // TObjNpcNgcBase
    case 0x00F3: // TObjNpcNgcBase
    case 0x00F4: // TObjNpcNgcBase
    case 0x00F5: // TObjNpcNgcBase
    case 0x00F6: // TObjNpcNgcBase
    case 0x00F7: // TObjNpcNgcBase
    case 0x00F8: // TObjNpcNgcBase
    case 0x00F9: // TObjNpcNgcBase
    case 0x00FA: // TObjNpcNgcBase
    case 0x00FB: // TObjNpcNgcBase
    case 0x00FC: // TObjNpcNgcBase
    case 0x00FD: // TObjNpcNgcBase
    case 0x00FE: // TObjNpcNgcBase
    case 0x00FF: // TObjNpcNgcBase
    case 0x0100: // Momoka
      // All of these have a default child count of zero
      add(EnemyType::NON_ENEMY_NPC);
      break;
    case 0x0040: { // TObjEneMoja
      bool is_rare = (set_entry->param6 >= 1);
      add(EnemyType::HILDEBEAR, is_rare, is_rare);
      break;
    }
    case 0x0041: { // TObjEneLappy
      uint8_t area = this->area_for_floor(floor);
      EnemyType type = (area < 0x24)
          ? EnemyType::RAG_RAPPY
          : (area <= 0x28)
          ? EnemyType::SAND_RAPPY_CRATER
          : EnemyType::SAND_RAPPY_DESERT;
      add(type, (set_entry->param6 != 0), (set_entry->param6 & 1));
      break;
    }
    case 0x0042: // TObjEneBm3FlyNest
      add(EnemyType::MONEST);
      child_type = EnemyType::MOTHMANT;
      default_num_children = 30;
      break;
    case 0x0043: // TObjEneBm5Wolf
      add((set_entry->param2 >= 1) ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
      break;
    case 0x0044: { // TObjEneBeast
      static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
      add(types[std::clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0060: // TObjGrass
      add(EnemyType::GRASS_ASSASSIN);
      break;
    case 0x0061: // TObjEneRe2Flower
      add((this->area_for_floor(floor) == 0x23) ? EnemyType::DEL_LILY : EnemyType::POISON_LILY);
      break;
    case 0x0062: // TObjEneNanoDrago
      add(EnemyType::NANO_DRAGON);
      break;
    case 0x0063: { // TObjEneShark
      static const EnemyType types[3] = {EnemyType::EVIL_SHARK, EnemyType::PAL_SHARK, EnemyType::GUIL_SHARK};
      add(types[std::clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0064: { // TObjEneSlime
      // Unlike all other versions, BB doesn't have a way to force slimes to be rare via constructor args
      bool is_rare_v123 = (set_entry->param7 & 1);
      default_num_children = -1; // Skip adding children later (because we do it here)
      size_t num_children = set_entry->num_children ? set_entry->num_children.load() : 4;
      for (size_t z = 0; z < num_children + 1; z++) {
        add(EnemyType::POFUILLY_SLIME, is_rare_v123, false);
      }
      break;
    }
    case 0x0065: // TObjEnePanarms
      if ((set_entry->num_children != 0) && (set_entry->num_children != 2)) {
        this->log.warning_f("PAN_ARMS has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::PAN_ARMS);
      add(EnemyType::HIDOOM);
      add(EnemyType::MIGIUM);
      break;
    case 0x0080: // TObjEneDubchik
      add((set_entry->param6 != 0) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
      break;
    case 0x0081: // TObjEneGyaranzo
      add(EnemyType::GARANZ);
      break;
    case 0x0082: // TObjEneMe3ShinowaReal
      add((set_entry->param2 >= 1) ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT);
      default_num_children = 4;
      break;
    case 0x0083: // TObjEneMe1Canadin
      add(EnemyType::CANADINE);
      break;
    case 0x0084: // TObjEneMe1CanadinLeader
      add(EnemyType::CANANE);
      child_type = EnemyType::CANADINE_GROUP;
      default_num_children = 8;
      break;
    case 0x0085: // TOCtrlDubchik
      add(EnemyType::DUBWITCH);
      break;
    case 0x00A0: // TObjEneSaver
      add(EnemyType::DELSABER);
      break;
    case 0x00A1: // TObjEneRe4Sorcerer
      if ((set_entry->num_children != 0) && (set_entry->num_children != 2)) {
        this->log.warning_f("CHAOS_SORCERER has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::CHAOS_SORCERER);
      add(EnemyType::BEE_R);
      add(EnemyType::BEE_L);
      break;
    case 0x00A2: // TObjEneDarkGunner
      add(EnemyType::DARK_GUNNER);
      break;
    case 0x00A3: // TObjEneDarkGunCenter
      add(EnemyType::DARK_GUNNER_CONTROL);
      break;
    case 0x00A4: // TObjEneDf2Bringer
      add(EnemyType::CHAOS_BRINGER);
      break;
    case 0x00A5: // TObjEneRe7Berura
      add(EnemyType::DARK_BELRA);
      break;
    case 0x00A6: { // TObjEneDimedian
      static const EnemyType types[3] = {EnemyType::DIMENIAN, EnemyType::LA_DIMENIAN, EnemyType::SO_DIMENIAN};
      add(types[std::clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x00A7: // TObjEneBalClawBody
      add(EnemyType::BULCLAW);
      child_type = EnemyType::CLAW;
      default_num_children = 4;
      break;
    case 0x00A8: // Unnamed subclass of TObjEneBalClawClaw
      add(EnemyType::CLAW);
      break;
    case 0x00C0: { // TBoss1Dragon or TBoss5Gryphon
      uint8_t area = this->area_for_floor(floor);
      if (area < 0x12) {
        add(EnemyType::DRAGON);
      } else if (area < 0x24) {
        add(EnemyType::GAL_GRYPHON);
      } else {
        throw std::runtime_error("DRAGON placed outside of Episode 1 or 2");
      }
      break;
    }
    case 0x00C1: // TBoss2DeRolLe
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x13)) {
        this->log.warning_f("DE_ROL_LE has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::DE_ROL_LE);
      for (size_t z = 0; z < 0x0A; z++) {
        add(EnemyType::DE_ROL_LE_BODY);
      }
      for (size_t z = 0; z < 0x09; z++) {
        add(EnemyType::DE_ROL_LE_MINE);
      }
      break;
    case 0x00C2: // TBoss3Volopt
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x23)) {
        this->log.warning_f("VOL_OPT has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::VOL_OPT_1);
      for (size_t z = 0; z < 0x06; z++) {
        add(EnemyType::VOL_OPT_PILLAR);
      }
      for (size_t z = 0; z < 0x18; z++) {
        add(EnemyType::VOL_OPT_MONITOR);
      }
      for (size_t z = 0; z < 0x02; z++) {
        add(EnemyType::NONE);
      }
      add(EnemyType::VOL_OPT_AMP);
      add(EnemyType::VOL_OPT_CORE);
      add(EnemyType::NONE);
      break;
    case 0x00C5: // Unnamed subclass of TObjEnemyCustom
      add(EnemyType::VOL_OPT_2);
      break;
    case 0x00C8: { // TBoss4DarkFalz
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x200)) {
        this->log.warning_f("DARK_FALZ has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      auto root_ene = add(EnemyType::DARK_FALZ_3);
      default_num_children = -1; // Skip adding children (because we do it here)
      for (size_t x = 0; x < 0x1FD; x++) {
        add(EnemyType::DARVANT);
      }
      add(EnemyType::DARK_FALZ_3, false, false, root_ene);
      add(EnemyType::DARK_FALZ_2, false, false, root_ene);
      add(EnemyType::DARK_FALZ_1, false, false, root_ene);
      break;
    }
    case 0x00CA: { // TBoss6PlotFalz
      auto root_ene = add(EnemyType::OLGA_FLOW_2);
      default_num_children = -1; // Skip adding children (because we do it here)
      for (size_t x = 0; x < 0x200; x++) {
        add(EnemyType::OLGA_FLOW_2, false, false, root_ene);
      }
      break;
    }
    case 0x00CB: // TBoss7DeRolLeC
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x2F)) {
        this->log.warning_f("BARBA_RAY has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::BARBA_RAY);
      for (size_t z = 0; z < 0x0A; z++) {
        add(EnemyType::BARBA_RAY_JOINT);
      }
      for (size_t z = 0; z < 0x24; z++) {
        add(EnemyType::PIG_RAY);
      }
      add(EnemyType::BARBA_RAY); // TODO: What is this actually?
      break;
    case 0x00CC: // TBoss8Dragon
      add(EnemyType::GOL_DRAGON);
      default_num_children = 5;
      break;
    case 0x00D4: // TObjEneMe3StelthReal
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add((set_entry->param6 > 0) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL);
        default_num_children = 4;
      }
      break;
    case 0x00D5: // TObjEneMerillLia
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add((set_entry->param6 > 0) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      }
      break;
    case 0x00D6: // TObjEneBm9Mericarol
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        switch (set_entry->param6) {
          case 0:
            add(EnemyType::MERICAROL);
            break;
          case 1:
            add(EnemyType::MERIKLE);
            break;
          case 2:
            add(EnemyType::MERICUS);
            break;
          default:
            add(EnemyType::MERICARAND);
        }
      }
      break;
    case 0x00D7: // TObjEneBm5GibonU
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add((set_entry->param6 > 0) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
      }
      break;
    case 0x00D8: // TObjEneGibbles
      add(EnemyType::GIBBLES);
      break;
    case 0x00D9: // TObjEneMe1Gee
      add(EnemyType::GEE);
      break;
    case 0x00DA: // TObjEneMe1GiGue
      add(EnemyType::GI_GUE);
      break;
    case 0x00DB: // TObjEneDelDepth
      add(EnemyType::DELDEPTH);
      break;
    case 0x00DC: // TObjEneDellBiter
      add(EnemyType::DELBITER);
      break;
    case 0x00DD: // TObjEneDolmOlm
      add((set_entry->param6 > 0) ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
      break;
    case 0x00DE: // TObjEneMorfos
      add(EnemyType::MORFOS);
      break;
    case 0x00DF: // TObjEneRecobox
      add(EnemyType::RECOBOX);
      child_type = EnemyType::RECON;
      break;
    case 0x00E0: { // TObjEneMe3SinowZoaReal or TObjEneEpsilonBody
      uint8_t area = this->area_for_floor(floor);
      if ((area == 0x22) || (area == 0x23)) {
        add(EnemyType::EPSILON);
        default_num_children = 4;
        child_type = EnemyType::EPSIGARD;
      } else {
        add((set_entry->param6 > 0) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
      }
      break;
    }
    case 0x00E1: // TObjEneIllGill
      add(EnemyType::ILL_GILL);
      break;
    case 0x0110:
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add(EnemyType::ASTARK);
      }
      break;
    case 0x0111: {
      uint8_t area = this->area_for_floor(floor);
      if (area == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else if (area <= 0x28) {
        add(set_entry->param2 ? EnemyType::YOWIE_CRATER : EnemyType::SATELLITE_LIZARD_CRATER);
      } else {
        add(set_entry->param2 ? EnemyType::YOWIE_DESERT : EnemyType::SATELLITE_LIZARD_DESERT);
      }
      break;
    }
    case 0x0112:
      if (this->area_for_floor(floor) == 0xFF) { // Ep3
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        bool is_rare = (set_entry->param6 & 1);
        add(EnemyType::MERISSA_A, is_rare, is_rare);
      }
      break;
    case 0x0113:
      add(EnemyType::GIRTABLULU);
      break;
    case 0x0114: {
      bool is_rare = (set_entry->param6 & 1);
      add((this->area_for_floor(floor) <= 0x28) ? EnemyType::ZU_CRATER : EnemyType::ZU_DESERT, is_rare, is_rare);
      break;
    }
    case 0x0115: {
      static const EnemyType types[3] = {EnemyType::BOOTA, EnemyType::ZE_BOOTA, EnemyType::BA_BOOTA};
      add(types[std::clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0116: {
      bool is_rare = (set_entry->param6 & 1);
      add(EnemyType::DORPHON, is_rare, is_rare);
      break;
    }
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      add(types[std::clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0119: {
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x18)) {
        this->log.warning_f("SAINT_MILION or SHAMBERTIN has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      default_num_children = -1; // Skip adding children (because we do it here)

      // There isn't a way to create Kondrieu via constructor args
      bool is_shambertin = (set_entry->param6 & 1);
      EnemyType base_type = is_shambertin ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILION;
      EnemyType spinner_type = is_shambertin ? EnemyType::SHAMBERTIN_SPINNER : EnemyType::SAINT_MILION_SPINNER;
      auto root_ene = add(base_type);
      for (size_t z = 0; z < 8; z++) {
        add(base_type);
      }
      for (size_t z = 0; z < 0x10; z++) { // 16 spinners
        add(spinner_type);
      }
      break;
    }

    case 0x00C3: // TBoss3VoloptP01
    case 0x00C4: // TBoss3VoloptCore or subclass
    case 0x00C6: // TBoss3VoloptMonitor
    case 0x00C7: // TBoss3VoloptHiraisin
    case 0x0118: // __QUEST_NPC__
      add(EnemyType::UNKNOWN);
      break;

    default:
      add(EnemyType::UNKNOWN);
      this->log.warning_f("Invalid enemy type {:04X}", set_entry->base_type);
      break;
  }

  if (default_num_children >= 0) {
    size_t num_children = set_entry->num_children ? set_entry->num_children.load() : default_num_children;
    if ((child_type == EnemyType::UNKNOWN) && head_ene) {
      child_type = head_ene->type;
    }
    for (size_t x = 0; x < num_children; x++) {
      add(child_type);
    }
  }

  if (!head_ene) {
    throw std::logic_error("no enemy was created");
  }
  return head_ene;
}

void SuperMap::link_enemy_version_and_children(
    std::shared_ptr<Enemy> ene, Version version, const MapFile::EnemySetEntry* set_entry) {
  auto& entities = this->version(version);

  size_t super_set_id = ene->super_set_id;
  do {
    auto& ene_ver = ene->version(version);
    if (ene_ver.set_entry) {
      throw std::logic_error("enemy already linked to version");
    }

    ene_ver.set_entry = set_entry;
    ene_ver.relative_enemy_index = entities.enemies.size();
    // If child_index > 0, then the head enemy was already created, so we need to subtract 1 from the set index because
    // this new enemy should have the same set index as the head enemy, but the head enemy was already added to the
    // enemy sets list.
    ene_ver.relative_set_index = entities.enemy_sets.size() - (ene->child_index != 0);

    // Add to primary enemy lists
    entities.enemies.emplace_back(ene);
    if (ene->child_index == 0) {
      entities.enemy_sets.emplace_back(ene);

      // Add to semantic hash index (but only for the root ene)
      uint64_t semantic_hash = set_entry->semantic_hash(ene->floor);
      this->enemy_sets_for_semantic_hash[semantic_hash].emplace_back(ene);
    }

    // Add to room/group index
    uint64_t k = room_index_key(ene->floor, set_entry->room, set_entry->wave_number);
    entities.enemy_for_floor_room_and_wave_number.emplace(k, ene);

    try {
      ene = this->enemies.at(ene->super_id + 1);
    } catch (const std::out_of_range&) {
      ene = nullptr;
    }
  } while (ene && (ene->super_set_id == super_set_id));
}

static size_t get_action_stream_size(const void* data, size_t size) {
  phosg::StringReader r(data, size);

  bool done = false;
  while (!done && !r.eof()) {
    uint8_t cmd = r.get_u8();
    switch (cmd) {
      case 0x00: // nop()
      case 0x01: // stop()
        done = (cmd == 0x01);
        break;
      case 0x08: // construct_objects(uint16_t room, uint16_t group)
      case 0x09: // construct_enemies(uint16_t room, uint16_t wave_number)
      case 0x0C: // trigger_event(uint32_t event_id)
      case 0x0D: // construct_enemies_stop(uint16_t room, uint16_t wave_number)
        r.skip(4);
        done = (cmd == 0x0D);
        break;
      case 0x0A: // set_switch_flag(uint16_t flag_num)
      case 0x0B: // clear_switch_flag(uint16_t flag_num)
        r.skip(2);
        break;
      default:
        done = true;
        break;
    }
  }

  return r.where();
}

std::shared_ptr<SuperMap::Event> SuperMap::add_event(
    Version version,
    uint8_t floor,
    const MapFile::Event1Entry* entry,
    const void* map_file_action_stream,
    size_t map_file_action_stream_size) {
  auto ev = std::make_shared<Event>();
  ev->super_id = this->events.size();
  ev->floor = floor;

  this->events.emplace_back(ev);
  this->link_event_version(ev, version, entry, map_file_action_stream, map_file_action_stream_size);

  return ev;
}

void SuperMap::link_event_version(
    std::shared_ptr<Event> ev,
    Version version,
    const MapFile::Event1Entry* entry,
    const void* map_file_action_stream,
    size_t map_file_action_stream_size) {
  if (entry->action_stream_offset >= map_file_action_stream_size) {
    throw std::runtime_error(std::format(
        "action stream offset 0x{:X} is beyond end of action stream (0x{:X}) for event {}",
        entry->action_stream_offset, map_file_action_stream_size, entry->str()));
  }
  const void* ev_action_stream_start = reinterpret_cast<const uint8_t*>(map_file_action_stream) +
      entry->action_stream_offset;
  size_t ev_action_stream_size = get_action_stream_size(
      ev_action_stream_start, map_file_action_stream_size - entry->action_stream_offset);

  auto& entities = this->version(version);
  auto& ev_ver = ev->version(version);
  if (ev_ver.set_entry) {
    throw std::logic_error("event already linked to version");
  }
  ev_ver.set_entry = entry;
  ev_ver.relative_event_index = entities.events.size();
  ev_ver.action_stream = ev_action_stream_start;
  ev_ver.action_stream_size = ev_action_stream_size;

  entities.events.emplace_back(ev);

  // Add to semantic hash index
  uint64_t semantic_hash = entry->semantic_hash(ev->floor);
  this->events_for_semantic_hash[semantic_hash].emplace_back(ev);

  // Add to room index
  uint64_t k = room_index_key(ev->floor, entry->room, entry->wave_number);
  entities.event_for_floor_room_and_wave_number.emplace(k, ev);
  k = (static_cast<uint64_t>(ev->floor) << 32) | entry->event_id;
  entities.event_for_floor_and_event_id.emplace(k, ev);
}

// This is a modified version of a simple dynamic programming edit distance algorithm. Conceptually, the previous map
// file's entries go down the left side of the matrix, and the current map file's entries go across the top. To save
// time, we run it once per floor, since we never expect objects on different floors in different versions to logically
// be the same object.

enum class EditAction {
  STOP = 0, // Reverse path ends here (at end, this should only be at (0, 0))
  ADD, // Reverse path goes left
  EDIT, // Reverse path goes up-left
  DELETE, // Reverse path goes up
};

template <typename EntityT, typename Score1, typename Score2>
std::vector<EditAction> compute_edit_path(
    const EntityT* prev,
    size_t prev_count,
    const EntityT* curr,
    size_t curr_count,
    Score1 get_add_cost,
    Score1 get_delete_cost,
    Score2 get_edit_cost) {
  struct Matrix {
    struct Entry {
      double cost = 0.0;
      EditAction action = EditAction::STOP;
    };
    size_t width;
    std::vector<Entry> entries;

    Matrix(size_t w, size_t h) : width(w), entries(w * h) {}

    inline Entry& at(size_t x, size_t y) {
      if (x >= this->width) {
        throw std::out_of_range("x coordinate out of range");
      }
      return this->entries.at(y * this->width + x);
    }
    inline const Entry& at(size_t x, size_t y) const {
      return const_cast<Matrix*>(this)->at(x, y);
    }

    void print(FILE* stream) const {
      for (size_t y = 0; y < this->entries.size() / this->width; y++) {
        for (size_t x = 0; x < this->width; x++) {
          const auto& entry = this->at(x, y);
          char action_ch = '?';
          switch (entry.action) {
            case EditAction::STOP:
              action_ch = 'S';
              break;
            case EditAction::ADD:
              action_ch = 'A';
              break;
            case EditAction::EDIT:
              action_ch = 'E';
              break;
            case EditAction::DELETE:
              action_ch = 'D';
              break;
          }
          phosg::fwrite_fmt(stream, "  {} {:03.2g}", action_ch, entry.cost);
        }
        fputc('\n', stream);
      }
    }
  };

  Matrix mtx(curr_count + 1, prev_count + 1);

  // Along the top and left edges, there is only one possible reverse path, so fill those in first
  for (size_t x = 1; x <= curr_count; x++) {
    auto& e = mtx.at(x, 0);
    e.cost = mtx.at(x - 1, 0).cost + get_add_cost(curr[x - 1]);
    e.action = EditAction::ADD;
  }
  for (size_t y = 1; y <= prev_count; y++) {
    auto& e = mtx.at(0, y);
    e.cost = mtx.at(0, y - 1).cost + get_delete_cost(prev[y - 1]);
    e.action = EditAction::DELETE;
  }

  // Fill in the rest of the cells based on the best cost for each action
  for (size_t y = 1; y <= prev_count; y++) {
    for (size_t x = 1; x <= curr_count; x++) {
      double add_cost = mtx.at(x - 1, y).cost + get_add_cost(curr[x - 1]);
      double delete_cost = mtx.at(x, y - 1).cost + get_delete_cost(prev[y - 1]);
      double edit_cost = mtx.at(x - 1, y - 1).cost + get_edit_cost(prev[y - 1], curr[x - 1]);

      auto& e = mtx.at(x, y);
      if (edit_cost <= add_cost) { // EDIT is better than ADD
        if (edit_cost <= delete_cost) { // EDIT is better than ADD and DELETE
          e.cost = edit_cost;
          e.action = EditAction::EDIT;
        } else { // EDIT is better than ADD but DELETE is better than EDIT
          e.cost = delete_cost;
          e.action = EditAction::DELETE;
        }
      } else { // ADD is better than EDIT
        if (add_cost <= delete_cost) { // ADD is better than EDIT and DELETE
          e.cost = add_cost;
          e.action = EditAction::ADD;
        } else { // ADD is better than EDIT but DELETE is better than ADD
          e.cost = delete_cost;
          e.action = EditAction::DELETE;
        }
      }
    }
  }

  // Trace the reverse path to get the list of edits
  std::vector<EditAction> reverse_path;
  size_t x = curr_count;
  size_t y = prev_count;
  while (x > 0 || y > 0) {
    EditAction action = mtx.at(x, y).action;
    reverse_path.emplace_back(action);
    switch (action) {
      case EditAction::STOP:
        mtx.print(stderr); // TODO: delete this when no longer needed
        throw std::logic_error("STOP action left after edit distance computation");
      case EditAction::ADD:
        x--;
        break;
      case EditAction::EDIT:
        x--;
        y--;
        break;
      case EditAction::DELETE:
        y--;
        break;
    }
  }

  // Reverse the reverse path to get the forward path
  std::reverse(reverse_path.begin(), reverse_path.end());
  return reverse_path;
}

template <typename EntityT>
std::vector<std::shared_ptr<EntityT>> compute_prev_entities(
    const std::vector<std::shared_ptr<EntityT>>& existing_prev_entities,
    size_t prev_entities_offset,
    const std::vector<EditAction>& edit_path) {
  std::vector<std::shared_ptr<EntityT>> ret;
  for (auto action : edit_path) {
    switch (action) {
      case EditAction::ADD:
        // This entity doesn't match any entity from the previous version
        ret.emplace_back(nullptr);
        break;
      case EditAction::DELETE:
        // There is an entity in the previous version that doesn't match any in this version; skip it
        prev_entities_offset++;
        break;
      case EditAction::EDIT: {
        // The current entity in this_sf matches the current entity in prev_sf; link them together
        ret.emplace_back(existing_prev_entities.at(prev_entities_offset));
        prev_entities_offset++;
        break;
      }
      default:
        throw std::logic_error("invalid edit path action");
    }
  }
  return ret;
}

static double object_set_add_cost(const MapFile::ObjectSetEntry&) {
  return 100.0;
}
static double object_set_delete_cost(const MapFile::ObjectSetEntry&) {
  return 100.0;
}
static double object_set_edit_cost(const MapFile::ObjectSetEntry& prev, const MapFile::ObjectSetEntry& current) {
  // A type change should never be better than an add + delete
  if (prev.base_type != current.base_type) {
    return 500.0;
  }
  // Group or room changes are pretty bad, but small variances in position and params are tolerated
  return (
      ((prev.group != current.group) * 50.0) +
      ((prev.room != current.room) * 50.0) +
      (prev.pos - current.pos).norm() +
      ((prev.param1 != current.param1) * 10.0) +
      ((prev.param2 != current.param2) * 10.0) +
      ((prev.param3 != current.param3) * 10.0) +
      ((prev.param4 != current.param4) * 10.0) +
      ((prev.param5 != current.param5) * 10.0) +
      ((prev.param6 != current.param6) * 10.0));
}

static double enemy_set_add_cost(const MapFile::EnemySetEntry&) {
  return 100.0;
}
static double enemy_set_delete_cost(const MapFile::EnemySetEntry&) {
  return 100.0;
}
static double enemy_set_edit_cost(const MapFile::EnemySetEntry& prev, const MapFile::EnemySetEntry& current) {
  // A change of type or num_children is not tolerated and should never be better than an add + delete. Unlike for
  // objects, we have to look at params to determine the type in some cases too
  if ((prev.base_type != current.base_type) || (prev.num_children != current.num_children)) {
    return 500.0;
  }
  bool is_mismatched = false;
  switch (prev.base_type) {
    case 0x0064: // TObjEneSlime
      is_mismatched = ((prev.param7 & 1) != (current.param7 & 1));
      break;
    case 0x0041: // TObjEneLappy
    case 0x0080: // TObjEneDubchik
      is_mismatched = ((prev.param6 == 0) != (current.param6 == 0));
      break;
    case 0x0043: // TObjEneBm5Wolf
    case 0x0082: // TObjEneMe3ShinowaReal
      is_mismatched = ((prev.param2 >= 1) != (current.param2 >= 1));
      break;
    case 0x00D6: // TObjEneBm9Mericarol
      // If 0, 1, or 2, must match exactly; otherwise, both must just be > 2
      is_mismatched = (prev.param6 > 2) ? (current.param6 <= 2) : (prev.param6 != current.param6);
      break;
    case 0x0040: // TObjEneMoja
    case 0x00D4: // TObjEneMe3StelthReal
    case 0x00D5: // TObjEneMerillLia
    case 0x00D7: // TObjEneBm5GibonU
    case 0x00DD: // TObjEneDolmOlm
    case 0x00E0: // TObjEneMe3SinowZoaReal or TObjEneEpsilonBody
      is_mismatched = ((prev.param6 > 0) != (current.param6 > 0));
      break;
    case 0x0111:
      is_mismatched = ((prev.param2 == 0) != (current.param2 == 0));
      break;
    case 0x0044: // TObjEneBeast
    case 0x0063: // TObjEneShark
    case 0x00A6: // TObjEneDimedian
    case 0x0115:
    case 0x0117:
      is_mismatched = (std::clamp<int16_t>(prev.param6, 0, 2) != std::clamp<int16_t>(current.param6, 0, 2));
      break;
    case 0x0112:
    case 0x0114:
    case 0x0116:
    case 0x0119:
      is_mismatched = ((prev.param6 & 1) != (current.param6 & 1));
      break;
  }
  if (is_mismatched) {
    return 500.0;
  }
  // Room or wave_number changes are pretty bad, but small variances in position and params are tolerated
  return (
      ((prev.room != current.room) * 50.0) +
      ((prev.wave_number != current.wave_number) * 50.0) +
      (prev.pos - current.pos).norm() +
      ((prev.param1 != current.param1) * 10.0) +
      ((prev.param2 != current.param2) * 10.0) +
      ((prev.param3 != current.param3) * 10.0) +
      ((prev.param4 != current.param4) * 10.0) +
      ((prev.param5 != current.param5) * 10.0) +
      ((prev.param6 != current.param6) * 10.0) +
      ((prev.param7 != current.param7) * 10.0));
}

static double event_add_cost(const MapFile::Event1Entry&) {
  return 1.0;
}
static double event_delete_cost(const MapFile::Event1Entry&) {
  return 1.0;
}
static double event_edit_cost(const MapFile::Event1Entry& prev, const MapFile::Event1Entry& current) {
  // Unlike objects and enemies, event matching is essentially binary
  return ((prev.event_id != current.event_id) || (prev.room != current.room) || (prev.wave_number != current.wave_number))
      ? 5.0
      : 0.0;
}

void SuperMap::add_map_file(Version this_v, std::shared_ptr<const MapFile> this_map_file) {
  auto& this_entities = this->version(this_v);
  if (this_entities.map_file) {
    throw std::logic_error("a map file is already present for this version");
  }
  this_entities.map_file = this_map_file;

  for (uint8_t floor = 0; floor < 0x12; floor++) {
    const auto& fc = this_map_file->floor(floor);
    if (fc.events2 || fc.random_enemy_locations_data || fc.random_enemy_definitions_data) {
      throw std::logic_error("cannot add map file with random segments to a supermap");
    }
  }

  // Find the previous version that has a map, if any
  Version prev_v = Version::UNKNOWN;
  std::shared_ptr<const MapFile> prev_map_file;
  for (ssize_t prev_v_s = static_cast<size_t>(this_v) - 1; prev_v_s >= 0; prev_v_s--) {
    const auto& prev_entities = this->entities_for_version.at(prev_v_s);
    if (prev_entities.map_file) {
      prev_v = static_cast<Version>(prev_v_s);
      prev_map_file = prev_entities.map_file;
      break;
    }
  }

  for (uint8_t floor = 0; floor < 0x12; floor++) {
    auto link_or_add_entities = [this_v, floor]<typename EntityT, typename EntryT>(
                                    const EntryT* prev_sets,
                                    size_t prev_set_count,
                                    const EntryT* this_sets,
                                    size_t this_set_count,
                                    double (*add_cost)(const EntryT&),
                                    double (*delete_cost)(const EntryT&),
                                    double (*edit_cost)(const EntryT&, const EntryT& current),
                                    const std::vector<std::shared_ptr<EntityT>>& prev_entities,
                                    size_t prev_entities_start_index,
                                    auto&& link_existing,
                                    auto&& add_new,
                                    const std::unordered_map<uint64_t, std::vector<std::shared_ptr<EntityT>>>& semantic_hash_index) {
      auto edit_path = compute_edit_path(
          prev_sets, prev_set_count, this_sets, this_set_count, add_cost, delete_cost, edit_cost);

      auto used_prev_entities = compute_prev_entities(prev_entities, prev_entities_start_index, edit_path);
      if (used_prev_entities.size() != this_set_count) {
        throw std::logic_error("incorrect previous entity list length");
      }
      std::unordered_set<const EntityT*> used_prev_entities_set;
      for (const auto& ent : used_prev_entities) {
        used_prev_entities_set.emplace(ent.get());
      }

      // Fill in all entities found by edit distance
      for (size_t z = 0; z < this_set_count; z++) {
        auto& prev_ent = used_prev_entities[z];

        // Use the semantic hash index to fill in gaps if possible
        if (!prev_ent) {
          try {
            for (const auto& ent : semantic_hash_index.at(this_sets[z].semantic_hash(floor))) {
              if (!ent->version(this_v).set_entry && !used_prev_entities_set.count(ent.get())) {
                prev_ent = ent;
                break;
              }
            }
          } catch (const std::out_of_range&) {
          }
        }

        if (prev_ent) {
          link_existing(prev_ent, this_v, this_sets + z);
        } else {
          add_new(this_v, floor, this_sets + z);
        }
      }
    };

    this_entities.object_floor_start_indexes[floor] = this_entities.objects.size();
    this_entities.enemy_floor_start_indexes[floor] = this_entities.enemies.size();
    this_entities.enemy_set_floor_start_indexes[floor] = this_entities.enemy_sets.size();
    this_entities.event_floor_start_indexes[floor] = this_entities.events.size();

    const auto& this_sf = this_map_file->floor(floor);

    if (!prev_map_file || !prev_map_file->floor(floor).object_sets) {
      // All objects were added in this version, or there was no previous version. Add all the objects as new objects.
      for (size_t z = 0; z < this_sf.object_set_count; z++) {
        this->add_object(this_v, floor, this_sf.object_sets + z);
      }

    } else if (this_sf.object_sets) {
      const auto& prev_sf = prev_map_file->floor(floor);
      auto& prev_entities = this->version(prev_v);

      link_or_add_entities(
          prev_sf.object_sets,
          prev_sf.object_set_count,
          this_sf.object_sets,
          this_sf.object_set_count,
          object_set_add_cost,
          object_set_delete_cost,
          object_set_edit_cost,
          prev_entities.objects,
          prev_entities.object_floor_start_indexes.at(floor),
          bind(&SuperMap::link_object_version, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          bind(&SuperMap::add_object, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          this->objects_for_semantic_hash);
    }

    if (!prev_map_file || !prev_map_file->floor(floor).enemy_sets) {
      for (size_t z = 0; z < this_sf.enemy_set_count; z++) {
        this->add_enemy_and_children(this_v, floor, this_sf.enemy_sets + z);
      }
    } else if (this_sf.enemy_sets) {
      const auto& prev_sf = prev_map_file->floor(floor);
      auto& prev_entities = this->version(prev_v);

      link_or_add_entities(
          prev_sf.enemy_sets,
          prev_sf.enemy_set_count,
          this_sf.enemy_sets,
          this_sf.enemy_set_count,
          enemy_set_add_cost,
          enemy_set_delete_cost,
          enemy_set_edit_cost,
          prev_entities.enemy_sets,
          prev_entities.enemy_set_floor_start_indexes.at(floor),
          bind(&SuperMap::link_enemy_version_and_children, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          bind(&SuperMap::add_enemy_and_children, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          this->enemy_sets_for_semantic_hash);
    }

    if (!prev_map_file || !prev_map_file->floor(floor).events1) {
      for (size_t z = 0; z < this_sf.event_count; z++) {
        this->add_event(this_v, floor, this_sf.events1 + z, this_sf.event_action_stream, this_sf.event_action_stream_bytes);
      }
    } else if (this_sf.events1) {
      const auto& prev_sf = prev_map_file->floor(floor);
      auto& prev_entities = this->version(prev_v);

      link_or_add_entities(
          prev_sf.events1,
          prev_sf.event_count,
          this_sf.events1,
          this_sf.event_count,
          event_add_cost,
          event_delete_cost,
          event_edit_cost,
          prev_entities.events,
          prev_entities.event_floor_start_indexes.at(floor),
          bind(&SuperMap::link_event_version, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, this_sf.event_action_stream, this_sf.event_action_stream_bytes),
          bind(&SuperMap::add_event, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, this_sf.event_action_stream, this_sf.event_action_stream_bytes),
          this->events_for_semantic_hash);
    }
  }
}

std::vector<std::shared_ptr<const SuperMap::Object>> SuperMap::objects_for_floor_room_group(
    Version version, uint8_t floor, uint16_t room, uint16_t group) const {
  const auto& entities = this->version(version);
  uint64_t k = room_index_key(floor, room, group);
  std::vector<std::shared_ptr<const Object>> ret;
  for (auto its = entities.object_for_floor_room_and_group.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

std::vector<std::shared_ptr<const SuperMap::Object>> SuperMap::doors_for_switch_flag(
    Version version, uint8_t floor, uint8_t switch_flag) const {
  std::vector<std::shared_ptr<const Object>> ret;
  const auto& entities = this->version(version);
  for (auto its = entities.door_for_floor_and_switch_flag.equal_range((floor << 8) | switch_flag);
      its.first != its.second;
      its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

std::shared_ptr<const SuperMap::Enemy> SuperMap::enemy_for_index(Version version, uint16_t enemy_index) const {
  const auto& entities = this->version(version);

  if (entities.enemies.empty()) {
    throw std::out_of_range("no enemies defined");
  }
  if (enemy_index >= entities.enemies.size()) {
    throw std::out_of_range("enemy ID out of range");
  }
  return entities.enemies[enemy_index];
}

std::shared_ptr<const SuperMap::Enemy> SuperMap::enemy_for_floor_type(Version version, uint8_t floor, EnemyType type) const {
  const auto& entities = this->version(version);

  if (entities.enemies.empty()) {
    throw std::out_of_range("no enemies defined");
  }
  size_t start_z = entities.enemy_floor_start_indexes.at(floor);
  size_t end_z = (floor < entities.enemy_floor_start_indexes.size() - 1)
      ? entities.enemy_floor_start_indexes[floor + 1]
      : entities.enemy_floor_start_indexes.size();
  for (size_t z = start_z; z < end_z; z++) {
    auto& ene = entities.enemies[z];
    if ((ene->floor == floor) || (ene->type == type)) {
      return ene;
    }
  }
  throw std::out_of_range("enemy not found");
}

std::vector<std::shared_ptr<const SuperMap::Enemy>> SuperMap::enemies_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const {
  const auto& entities = this->version(version);

  uint64_t k = room_index_key(floor, room, wave_number);
  std::vector<std::shared_ptr<const Enemy>> ret;
  for (auto its = entities.enemy_for_floor_room_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

std::vector<std::shared_ptr<const SuperMap::Event>> SuperMap::events_for_id(Version version, uint8_t floor, uint32_t event_id) const {
  const auto& entities = this->version(version);
  uint64_t k = (static_cast<uint64_t>(floor) << 32) | event_id;
  std::vector<std::shared_ptr<const Event>> ret;
  for (auto its = entities.event_for_floor_and_event_id.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

std::vector<std::shared_ptr<const SuperMap::Event>> SuperMap::events_for_floor(Version version, uint8_t floor) const {
  const auto& entities = this->version(version);
  uint64_t k_start = (static_cast<uint64_t>(floor) << 32);
  uint64_t k_end = (static_cast<uint64_t>(floor + 1) << 32);
  std::vector<std::shared_ptr<const Event>> ret;
  for (auto it = entities.event_for_floor_and_event_id.lower_bound(k_start);
      (it != entities.event_for_floor_and_event_id.end()) && (it->first < k_end);
      it++) {
    ret.emplace_back(it->second);
  }
  return ret;
}

std::vector<std::shared_ptr<const SuperMap::Event>> SuperMap::events_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const {
  const auto& entities = this->version(version);
  uint64_t k = room_index_key(floor, room, wave_number);
  std::vector<std::shared_ptr<const Event>> ret;
  for (auto its = entities.event_for_floor_room_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

std::unordered_map<EnemyType, size_t> SuperMap::count_enemy_sets_for_version(Version version) const {
  std::unordered_map<EnemyType, size_t> ret;
  for (const auto& ene : this->version(version).enemy_sets) {
    try {
      ret.at(ene->type) += 1;
    } catch (const std::out_of_range&) {
      ret.emplace(ene->type, 1);
    }
  }
  return ret;
}

SuperMap::EfficiencyStats& SuperMap::EfficiencyStats::operator+=(const EfficiencyStats& other) {
  this->filled_object_slots += other.filled_object_slots;
  this->total_object_slots += other.total_object_slots;
  this->filled_enemy_set_slots += other.filled_enemy_set_slots;
  this->total_enemy_set_slots += other.total_enemy_set_slots;
  this->filled_event_slots += other.filled_event_slots;
  this->total_event_slots += other.total_event_slots;
  return *this;
}

std::string SuperMap::EfficiencyStats::str() const {
  double object_eff = this->total_object_slots
      ? (static_cast<double>(this->filled_object_slots * 100) / static_cast<double>(this->total_object_slots))
      : 0;
  double enemy_set_eff = this->total_enemy_set_slots
      ? (static_cast<double>(this->filled_enemy_set_slots * 100) / static_cast<double>(this->total_enemy_set_slots))
      : 0;
  double event_eff = this->total_event_slots
      ? (static_cast<double>(this->filled_event_slots * 100) / static_cast<double>(this->total_event_slots))
      : 0;
  return std::format(
      "EfficiencyStats[K = {}/{} ({:g}%), E = {}/{} ({:g}%), W = {}/{} ({:g}%)]",
      this->filled_object_slots, this->total_object_slots, object_eff,
      this->filled_enemy_set_slots, this->total_enemy_set_slots, enemy_set_eff,
      this->filled_event_slots, this->total_event_slots, event_eff);
}

SuperMap::EfficiencyStats SuperMap::efficiency() const {
  EfficiencyStats ret;

  for (const auto& obj : this->objects) {
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& obj_ver = obj->version(v);
      if (obj_ver.relative_object_index != 0xFFFF) {
        ret.filled_object_slots++;
      }
    }
  }
  ret.total_object_slots = this->objects.size() * ALL_NON_PATCH_VERSIONS.size();

  for (const auto& ene : this->enemy_sets) {
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ene_ver = ene->version(v);
      if (ene_ver.relative_enemy_index != 0xFFFF) {
        ret.filled_enemy_set_slots++;
      }
    }
  }
  ret.total_enemy_set_slots = this->enemy_sets.size() * ALL_NON_PATCH_VERSIONS.size();

  for (const auto& ev : this->events) {
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ev_ver = ev->version(v);
      if (ev_ver.relative_event_index != 0xFFFF) {
        ret.filled_event_slots++;
      }
    }
  }
  ret.total_event_slots = this->events.size() * ALL_NON_PATCH_VERSIONS.size();

  return ret;
}

void SuperMap::verify() const {
  for (size_t super_id = 0; super_id < this->objects.size(); super_id++) {
    if (this->objects[super_id]->super_id != super_id) {
      throw std::logic_error("object super_id is incorrect");
    }
  }

  {
    size_t super_set_id = static_cast<size_t>(-1);
    size_t prev_child_index = 0;
    for (size_t super_id = 0; super_id < this->enemies.size(); super_id++) {
      const auto& ene = this->enemies[super_id];
      if (ene->super_id != super_id) {
        throw std::logic_error("enemy super_id is incorrect");
      }
      if (ene->child_index == 0) {
        super_set_id++;
        prev_child_index = 0;
      } else {
        if (ene->child_index != ++prev_child_index) {
          throw std::logic_error("enemy child indexes out of order");
        }
      }
      if (ene->super_set_id != super_set_id) {
        throw std::logic_error(std::format(
            "enemy super_set_id is incorrect; expected S-{:03X}, received S-{:03X}", super_set_id, ene->super_set_id));
      }
    }
    if (super_set_id != this->enemy_sets.size() - 1) {
      throw std::logic_error(std::format(
          "not all enemy sets are in the enemies list; ended with 0x{:X}, expected 0x{:X}",
          super_set_id, this->enemy_sets.size()));
    }
  }
  for (size_t super_id = 0; super_id < this->events.size(); super_id++) {
    if (this->events[super_id]->super_id != super_id) {
      throw std::logic_error("event super_id is incorrect");
    }
  }

  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& entities = this->version(v);

    if (entities.object_floor_start_indexes.at(0) != 0) {
      throw std::logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.enemy_floor_start_indexes.at(0) != 0) {
      throw std::logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.enemy_set_floor_start_indexes.at(0) != 0) {
      throw std::logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.event_floor_start_indexes.at(0) != 0) {
      throw std::logic_error("object floor start index for floor 0 is incorrect");
    }

    uint8_t floor = 0;
    for (size_t object_index = 0; object_index < entities.objects.size(); object_index++) {
      const auto& obj = entities.objects[object_index];
      if (obj->floor < floor) {
        throw std::logic_error("objects out of floor order");
      }
      while (floor < obj->floor) {
        floor++;
        if (entities.object_floor_start_indexes.at(floor) != object_index) {
          throw std::logic_error("object floor start index is incorrect");
        }
      }
      const auto& obj_ver = obj->version(v);
      if (!obj_ver.set_entry) {
        throw std::logic_error("object set entry is missing");
      }
      if (obj_ver.relative_object_index != object_index) {
        throw std::logic_error("object relative index is incorrect");
      }
    }
    while (floor < 0x12) {
      floor++;
      if ((floor < 0x12) && (entities.object_floor_start_indexes.at(floor) != entities.objects.size())) {
        throw std::logic_error("object floor start index is incorrect");
      }
    }

    floor = 0;
    size_t enemy_set_index = static_cast<size_t>(-1);
    for (size_t enemy_index = 0; enemy_index < entities.enemies.size(); enemy_index++) {
      const auto& ene = entities.enemies[enemy_index];
      if (ene->child_index == 0) {
        enemy_set_index++;
        if (entities.enemy_sets.at(enemy_set_index) != ene) {
          throw std::logic_error("enemy set does not match expected enemy");
        }
      }
      if (ene->floor < floor) {
        throw std::logic_error("enemies out of floor order");
      }
      while (floor < ene->floor) {
        floor++;
        if (entities.enemy_floor_start_indexes.at(floor) != enemy_index) {
          throw std::logic_error("enemy floor start index is incorrect");
        }
        if (entities.enemy_set_floor_start_indexes.at(floor) != enemy_set_index) {
          throw std::logic_error("enemy set floor start index is incorrect");
        }
      }
      const auto& ene_ver = ene->version(v);
      if (!ene_ver.set_entry) {
        throw std::logic_error("enemy set entry is missing");
      }
      if (ene_ver.relative_enemy_index != enemy_index) {
        throw std::logic_error("enemy relative index is incorrect");
      }
      if (ene_ver.relative_set_index != enemy_set_index) {
        throw std::logic_error("enemy relative set index is incorrect");
      }
    }
    if (enemy_set_index != entities.enemy_sets.size() - 1) {
      throw std::logic_error("not all enemy sets were checked");
    }
    while (floor < 0x12) {
      floor++;
      if (floor < 0x12) {
        if (entities.enemy_floor_start_indexes.at(floor) != entities.enemies.size()) {
          throw std::logic_error("enemy floor start index is incorrect");
        }
        if (entities.enemy_set_floor_start_indexes.at(floor) != entities.enemy_sets.size()) {
          throw std::logic_error("enemy set floor start index is incorrect");
        }
      }
    }

    floor = 0;
    for (size_t event_index = 0; event_index < entities.events.size(); event_index++) {
      const auto& ev = entities.events[event_index];
      if (ev->floor < floor) {
        throw std::logic_error("events out of floor order");
      }
      while (floor < ev->floor) {
        floor++;
        if (entities.event_floor_start_indexes.at(floor) != event_index) {
          throw std::logic_error("event floor start index is incorrect");
        }
      }
      const auto& ev_ver = ev->version(v);
      if (!ev_ver.set_entry) {
        throw std::logic_error("event entry is missing");
      }
      if (ev_ver.relative_event_index != event_index) {
        throw std::logic_error("event relative index is incorrect");
      }
    }
    while (floor < 0x12) {
      floor++;
      if ((floor < 0x12) && (entities.event_floor_start_indexes.at(floor) != entities.events.size())) {
        throw std::logic_error("event floor start index is incorrect");
      }
    }
  }
}

void SuperMap::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "SuperMap areas=[{}] random={:08X}\n",
      phosg::format_data_string(&this->floor_to_area, sizeof(this->floor_to_area)), this->random_seed);

  phosg::fwrite_fmt(stream, "               DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 E3TE GCE3 XBV3 BBV4\n");
  phosg::fwrite_fmt(stream, "  MAP         ");
  for (const auto& v : ALL_NON_PATCH_VERSIONS) {
    const auto& entities = this->version(v);
    phosg::fwrite_fmt(stream, " {}", entities.map_file ? "++++" : "----");
  }
  fputc('\n', stream);
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    phosg::fwrite_fmt(stream, "  KS  START {:02X}", floor);
    for (const auto& v : ALL_NON_PATCH_VERSIONS) {
      const auto& entities = this->version(v);
      phosg::fwrite_fmt(stream, " {:04X}", entities.object_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    phosg::fwrite_fmt(stream, "  ES  START {:02X}", floor);
    for (const auto& v : ALL_NON_PATCH_VERSIONS) {
      const auto& entities = this->version(v);
      phosg::fwrite_fmt(stream, " {:04X}", entities.enemy_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    phosg::fwrite_fmt(stream, "  ESS START {:02X}", floor);
    for (const auto& v : ALL_NON_PATCH_VERSIONS) {
      const auto& entities = this->version(v);
      phosg::fwrite_fmt(stream, " {:04X}", entities.enemy_set_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    phosg::fwrite_fmt(stream, "  WS  START {:02X}", floor);
    for (const auto& v : ALL_NON_PATCH_VERSIONS) {
      const auto& entities = this->version(v);
      phosg::fwrite_fmt(stream, " {:04X}", entities.event_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }

  phosg::fwrite_fmt(stream, "  KS-FL-ID  DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 E3TE GCE3 XBV3 BBV4 DEFINITION\n");
  for (const auto& obj : this->objects) {
    phosg::fwrite_fmt(stream, "  KS-{:02X}-{:03X}", obj->floor, obj->super_id);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& obj_ver = obj->version(v);
      if (obj_ver.relative_object_index == 0xFFFF) {
        phosg::fwrite_fmt(stream, " ----");
      } else {
        phosg::fwrite_fmt(stream, " {:04X}", obj_ver.relative_object_index);
      }
    }
    auto obj_str = obj->str();
    phosg::fwrite_fmt(stream, " {}\n", obj_str);
  }

  phosg::fwrite_fmt(stream, "  ES-FL-ID  DCTE----- DCPR----- DCV1----- DCV2----- PCTE----- PCV2----- GCTE----- GCV3----- EP3TE---- GCEP3---- XBV3----- BBV4----- DEFINITION\n");
  for (const auto& ene : this->enemies) {
    phosg::fwrite_fmt(stream, "  ES-{:02X}-{:03X}", ene->floor, ene->super_id);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ene_ver = ene->version(v);
      if (ene_ver.relative_enemy_index == 0xFFFF) {
        phosg::fwrite_fmt(stream, " ----:----");
      } else {
        phosg::fwrite_fmt(stream, " {:04X}:{:04X}", ene_ver.relative_set_index, ene_ver.relative_enemy_index);
      }
    }

    auto ene_str = ene->str();
    phosg::fwrite_fmt(stream, " {}\n", ene_str);
  }

  phosg::fwrite_fmt(stream, "  WS-FL-ID  DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 E3TE GCE3 XBV3 BBV4 DEFINITION\n");
  for (const auto& ev : this->events) {
    phosg::fwrite_fmt(stream, "  WS-{:02X}-{:03X}", ev->floor, ev->super_id);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ev_ver = ev->version(v);
      if (ev_ver.relative_event_index == 0xFFFF) {
        phosg::fwrite_fmt(stream, " ----");
      } else {
        phosg::fwrite_fmt(stream, " {:04X}", ev_ver.relative_event_index);
      }
    }
    auto ev_str = ev->str();
    phosg::fwrite_fmt(stream, " {}\n", ev_str);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Map state

MapState::RareEnemyRates::RareEnemyRates(uint32_t enemy_rate, uint32_t mericarand_rate, uint32_t boss_rate)
    : hildeblue(enemy_rate),
      rappy(enemy_rate),
      nar_lily(enemy_rate),
      pouilly_slime(enemy_rate),
      mericarand(mericarand_rate),
      merissa_aa(enemy_rate),
      pazuzu(enemy_rate),
      dorphon_eclair(enemy_rate),
      kondrieu(boss_rate) {}

MapState::RareEnemyRates::RareEnemyRates(const phosg::JSON& json)
    : hildeblue(json.get_int("Hildeblue", DEFAULT_RARE_ENEMY_RATE_V3)),
      rappy(json.get_int("Rappy", DEFAULT_RARE_ENEMY_RATE_V3)),
      nar_lily(json.get_int("NarLily", DEFAULT_RARE_ENEMY_RATE_V3)),
      pouilly_slime(json.get_int("PouillySlime", DEFAULT_RARE_ENEMY_RATE_V3)),
      mericarand(json.get_int("Mericarand", DEFAULT_MERICARAND_RATE_V3)),
      merissa_aa(json.get_int("MerissaAA", DEFAULT_RARE_ENEMY_RATE_V3)),
      pazuzu(json.get_int("Pazuzu", DEFAULT_RARE_ENEMY_RATE_V3)),
      dorphon_eclair(json.get_int("DorphonEclair", DEFAULT_RARE_ENEMY_RATE_V3)),
      kondrieu(json.get_int("Kondrieu", DEFAULT_RARE_BOSS_RATE_V4)) {}

std::string MapState::RareEnemyRates::str() const {
  return std::format("RareEnemyRates(hildeblue={:08X}, rappy={:08X}, nar_lily={:08X}, pouilly_slime={:08X}, mericarand={:08X}, merissa_aa={:08X}, pazuzu={:08X}, dorphon_eclair={:08X}, kondrieu={:08X})",
      this->hildeblue, this->rappy, this->nar_lily, this->pouilly_slime, this->mericarand,
      this->merissa_aa, this->pazuzu, this->dorphon_eclair, this->kondrieu);
}

phosg::JSON MapState::RareEnemyRates::json() const {
  return phosg::JSON::dict({
      {"Hildeblue", this->hildeblue},
      {"Rappy", this->rappy},
      {"NarLily", this->nar_lily},
      {"PouillySlime", this->pouilly_slime},
      {"Mericarand", this->mericarand},
      {"MerissaAA", this->merissa_aa},
      {"Pazuzu", this->pazuzu},
      {"DorphonEclair", this->dorphon_eclair},
      {"Kondrieu", this->kondrieu},
  });
}

uint32_t MapState::RareEnemyRates::get(EnemyType type) const {
  switch (type) {
    case EnemyType::HILDEBEAR:
      return this->hildeblue;
    case EnemyType::RAG_RAPPY:
    case EnemyType::SAND_RAPPY_CRATER:
    case EnemyType::SAND_RAPPY_DESERT:
      return this->rappy;
    case EnemyType::POISON_LILY:
      return this->nar_lily;
    case EnemyType::POFUILLY_SLIME:
      return this->pouilly_slime;
    case EnemyType::MERICARAND:
      return this->mericarand;
    case EnemyType::MERISSA_A:
      return this->merissa_aa;
    case EnemyType::ZU_CRATER:
    case EnemyType::ZU_DESERT:
      return this->pazuzu;
    case EnemyType::DORPHON:
      return this->dorphon_eclair;
    case EnemyType::SAINT_MILION:
    case EnemyType::SHAMBERTIN:
      return this->kondrieu;
    default:
      return 0;
  }
}

const std::shared_ptr<const MapState::RareEnemyRates> MapState::NO_RARE_ENEMIES = std::make_shared<MapState::RareEnemyRates>(0, 0, 0);
const std::shared_ptr<const MapState::RareEnemyRates> MapState::DEFAULT_RARE_ENEMIES = std::make_shared<MapState::RareEnemyRates>(
    MapState::RareEnemyRates::DEFAULT_RARE_ENEMY_RATE_V3,
    MapState::RareEnemyRates::DEFAULT_MERICARAND_RATE_V3,
    MapState::RareEnemyRates::DEFAULT_RARE_BOSS_RATE_V4);

MapState::EntityIterator::EntityIterator(MapState* map_state, Version version, bool at_end)
    : map_state(map_state),
      version(version),
      floor(at_end ? this->map_state->floor_config_entries.size() : 0),
      relative_index(0) {
}

void MapState::EntityIterator::prepare() {
  while ((this->floor < this->map_state->floor_config_entries.size()) &&
      (this->num_entities_on_current_floor() == 0)) {
    this->floor++;
  }
}

void MapState::EntityIterator::advance() {
  this->relative_index++;
  while ((this->floor < this->map_state->floor_config_entries.size()) &&
      (this->relative_index >= this->num_entities_on_current_floor())) {
    this->relative_index = 0;
    this->floor++;
  }
}

MapState::EntityIterator& MapState::EntityIterator::operator++() {
  this->advance();
  return *this;
}

bool MapState::EntityIterator::operator==(const EntityIterator& other) const {
  return (this->map_state == other.map_state) &&
      (this->version == other.version) &&
      (this->floor == other.floor) &&
      (this->relative_index == other.relative_index);
}

bool MapState::EntityIterator::operator!=(const EntityIterator& other) const {
  return !this->operator==(other);
}

std::shared_ptr<MapState::ObjectState>& MapState::ObjectIterator::operator*() {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  const auto& obj = fc.super_map->version(this->version).objects.at(this->relative_index);
  return this->map_state->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
}
size_t MapState::ObjectIterator::num_entities_on_current_floor() const {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  return fc.super_map ? fc.super_map->version(this->version).objects.size() : 0;
}

std::shared_ptr<MapState::EnemyState>& MapState::EnemyIterator::operator*() {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  const auto& ene = fc.super_map->version(this->version).enemies.at(this->relative_index);
  return this->map_state->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
}
size_t MapState::EnemyIterator::num_entities_on_current_floor() const {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  return fc.super_map ? fc.super_map->version(this->version).enemies.size() : 0;
}

std::shared_ptr<MapState::EnemyState>& MapState::EnemySetIterator::operator*() {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  const auto& ene = fc.super_map->version(this->version).enemy_sets.at(this->relative_index);
  return this->map_state->enemy_set_states.at(fc.base_super_ids.base_enemy_set_index + ene->super_set_id);
}
size_t MapState::EnemySetIterator::num_entities_on_current_floor() const {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  return fc.super_map ? fc.super_map->version(this->version).enemy_sets.size() : 0;
}

std::shared_ptr<MapState::EventState>& MapState::EventIterator::operator*() {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  const auto& ev = fc.super_map->version(this->version).events.at(this->relative_index);
  return this->map_state->event_states.at(fc.base_super_ids.base_event_index + ev->super_id);
}
size_t MapState::EventIterator::num_entities_on_current_floor() const {
  const auto& fc = this->map_state->floor_config_entries.at(this->floor);
  return fc.super_map ? fc.super_map->version(this->version).events.size() : 0;
}

MapState::MapState(
    uint64_t lobby_or_session_id,
    Difficulty difficulty,
    uint8_t event,
    uint32_t random_seed,
    std::shared_ptr<const RareEnemyRates> bb_rare_rates,
    std::shared_ptr<RandomGenerator> rand_crypt,
    std::vector<std::shared_ptr<const SuperMap>> floor_map_defs)
    : log(std::format("[MapState(free):{:08X}] ", lobby_or_session_id), lobby_log.min_level),
      difficulty(difficulty),
      event(event),
      random_seed(random_seed),
      bb_rare_rates(bb_rare_rates) {

  if (floor_map_defs.empty()) {
    throw std::runtime_error("cannot construct a MapState with no floor maps");
  }
  this->floor_to_area = floor_map_defs[0]->floor_to_area;

  this->floor_config_entries.resize(0x12);
  for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
    auto& this_fc = this->floor_config_entries[floor];
    this_fc.super_map = (floor < floor_map_defs.size()) ? floor_map_defs[floor] : nullptr;
    if (this_fc.super_map) {
      this->index_super_map(this_fc, rand_crypt);
    }

    if (floor < this->floor_config_entries.size() - 1) {
      auto& next_fc = this->floor_config_entries[floor + 1];
      if (this_fc.super_map) {
        for (Version v : ALL_NON_PATCH_VERSIONS) {
          const auto& this_indexes = this_fc.base_indexes_for_version(v);
          auto& next_indexes = next_fc.base_indexes_for_version(v);
          auto& entities = this_fc.super_map->version(v);
          next_indexes.base_object_index = this_indexes.base_object_index + entities.objects.size();
          next_indexes.base_enemy_index = this_indexes.base_enemy_index + entities.enemies.size();
          next_indexes.base_enemy_set_index = this_indexes.base_enemy_set_index + entities.enemy_sets.size();
          next_indexes.base_event_index = this_indexes.base_event_index + entities.events.size();
        }
        next_fc.base_super_ids.base_object_index = this->object_states.size();
        next_fc.base_super_ids.base_enemy_index = this->enemy_states.size();
        next_fc.base_super_ids.base_enemy_set_index = this->enemy_set_states.size();
        next_fc.base_super_ids.base_event_index = this->event_states.size();
      } else {
        for (Version v : ALL_NON_PATCH_VERSIONS) {
          next_fc.base_indexes_for_version(v) = this_fc.base_indexes_for_version(v);
        }
      }
    }
  }

  this->compute_dynamic_object_base_indexes();
  this->verify();
}

MapState::MapState(
    uint64_t lobby_or_session_id,
    Difficulty difficulty,
    uint8_t event,
    uint32_t random_seed,
    std::shared_ptr<const RareEnemyRates> bb_rare_rates,
    std::shared_ptr<RandomGenerator> rand_crypt,
    std::shared_ptr<const SuperMap> quest_map_def)
    : log(std::format("[MapState(quest):{:08X}] ", lobby_or_session_id), lobby_log.min_level),
      floor_to_area(quest_map_def->floor_to_area),
      difficulty(difficulty),
      event(event),
      random_seed(random_seed),
      bb_rare_rates(bb_rare_rates) {
  FloorConfig& fc = this->floor_config_entries.emplace_back();
  fc.super_map = quest_map_def;
  this->index_super_map(fc, rand_crypt);
  this->compute_dynamic_object_base_indexes();
  this->verify();
}

MapState::MapState()
    : log("[MapState(empty)] ", lobby_log.min_level),
      bb_rare_rates(this->DEFAULT_RARE_ENEMIES) {}

void MapState::reset() {
  for (auto& obj_st : this->object_states) {
    obj_st->reset();
  }
  for (auto& ene_st : this->enemy_states) {
    ene_st->reset();
  }
  for (auto& ev_st : this->event_states) {
    ev_st->reset();
  }
}

void MapState::index_super_map(const FloorConfig& fc, std::shared_ptr<RandomGenerator> rand_crypt) {
  if (!fc.super_map) {
    throw std::logic_error("cannot index floor config with no map definition");
  }
  if (fc.super_map->floor_to_area != this->floor_to_area) {
    throw std::runtime_error("supermaps have different floor configs");
  }

  for (const auto& obj : fc.super_map->all_objects()) {
    auto& obj_st = this->object_states.emplace_back(std::make_shared<ObjectState>());
    obj_st->k_id = this->object_states.size() - 1;
    obj_st->super_obj = obj;
  }

  for (const auto& ene : fc.super_map->all_enemies()) {
    auto& ene_st = this->enemy_states.emplace_back(std::make_shared<EnemyState>());

    if (ene->child_index == 0) {
      this->enemy_set_states.emplace_back(ene_st);
    }
    ene_st->e_id = this->enemy_states.size() - 1;
    ene_st->set_id = this->enemy_set_states.size() - 1;
    ene_st->super_ene = ene;

    if (ene->alias_target_ene) {
      ssize_t delta = ene->alias_target_ene->super_id - ene->super_id;
      ene_st->alias_target_ene_st = this->enemy_states.at(ene_st->e_id + delta);
      if (ene_st->alias_target_ene_st->super_ene != ene->alias_target_ene) {
        throw std::logic_error("found incorrect alias target state for enemy");
      }
      if (ene_st->alias_target_ene_st->super_ene->alias_target_ene) {
        throw std::runtime_error("target for enemy state alias is itself an alias");
      }
    }

    // Handle random rare enemies and difficulty-based effects
    EnemyType type = ene->type;
    if ((type == EnemyType::DARK_FALZ_3) && (this->difficulty == Difficulty::NORMAL)) {
      type = EnemyType::DARK_FALZ_2;
    }

    uint8_t area = fc.super_map->area_for_floor(ene->floor);
    auto rare_type = type_definition_for_enemy(type).rare_type(area, this->event);
    if ((type == EnemyType::MERICARAND) || (rare_type != type)) {
      std::unordered_map<uint32_t, float> det_cache;
      uint32_t bb_rare_rate = this->bb_rare_rates->get(type);
      for (Version v : ALL_NON_PATCH_VERSIONS) {
        // Skip this version if the enemy doesn't exist there
        uint16_t relative_enemy_index = ene->version(v).relative_enemy_index;
        if (relative_enemy_index == 0xFFFF) {
          continue;
        }
        // Skip this version if the enemy is default rare
        if (is_v4(v) ? ene->is_default_rare_bb : ene->is_default_rare_v123) {
          continue;
        }

        uint16_t enemy_index = relative_enemy_index + fc.base_indexes_for_version(v).base_enemy_index;

        // On BB, rare enemy indexes are generated by the server and sent to the client, so we can use any method we
        // want to choose rares. On other versions, we must match the client's logic, even though it's more
        // computationally expensive.
        if (!is_v4(v)) {
          uint32_t seed = this->random_seed + 0x1000 + enemy_index;
          float det;
          try {
            det = det_cache.at(seed);
          } catch (const std::out_of_range&) {
            det = (static_cast<float>((PSOV2Encryption::single(seed) >> 16) & 0xFFFF) / 65536.0f);
            det_cache.emplace(seed, det);
          }

          if (type == EnemyType::MERICARAND) {
            // On v3, Mericarols that have param6 > 2 are randomized to be Mericus, Merikle, or Mericarol, but the
            // former two are not considered rare. (We use rare_flags anyway to distinguish them from Mericarol.)
            if (det > 0.9) { // Merikle
              ene_st->set_rare(v);
              ene_st->set_mericarand_variant_flag(v);
            } else if (det > 0.8) { // Mericus
              ene_st->set_rare(v);
            } else {
              // Mericarol (no flags to set)
            }

          } else {
            // On v1 and v2 (and GC NTE), the rare rate is 0.1% instead of 0.2%.
            if (det < (is_v1_or_v2(v) ? 0.001f : 0.002f)) {
              ene_st->set_rare(v);
            }
          }

        } else if ((bb_rare_rate > 0) && (this->bb_rare_enemy_indexes.size() < 0x10) && (rand_crypt->next() < bb_rare_rate)) {
          this->bb_rare_enemy_indexes.emplace_back(enemy_index);
          ene_st->set_rare(v);
          if ((type == EnemyType::MERICARAND) && (enemy_index & 1)) {
            ene_st->set_mericarand_variant_flag(v);
          }
        }
      }
    }
  }

  for (const auto& ev : fc.super_map->all_events()) {
    auto& ev_st = this->event_states.emplace_back(std::make_shared<EventState>());
    ev_st->w_id = this->event_states.size() - 1;
    ev_st->super_ev = ev;
  }
}

void MapState::compute_dynamic_object_base_indexes() {
  this->dynamic_obj_base_k_id = this->object_states.size();

  // Compute the maximum object ID for each version. We can't just use the last object because that object may not
  // exist on all versions, and we can't just look at the last floor, because that floor may be empty on some versions.
  this->dynamic_obj_base_index_for_version.fill(0);
  for (const auto& fc : this->floor_config_entries) {
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      if (fc.super_map) {
        auto& last_obj_index = this->dynamic_obj_base_index_for_version[static_cast<size_t>(v)];
        size_t base_index = fc.base_indexes_for_version(v).base_object_index;
        last_obj_index = std::max<size_t>(base_index + fc.super_map->version(v).objects.size(), last_obj_index);
      }
    }
  }
}

uint16_t MapState::index_for_object_state(Version version, std::shared_ptr<const ObjectState> obj_st) const {
  if (obj_st->super_obj) {
    uint16_t relative_index = obj_st->super_obj->version(version).relative_object_index;
    return (relative_index == 0xFFFF)
        ? 0xFFFF
        : (relative_index + this->floor_config(obj_st->super_obj->floor).base_indexes_for_version(version).base_object_index);
  } else {
    size_t k_id_delta = obj_st->k_id - this->dynamic_obj_base_k_id;
    return this->dynamic_obj_base_index_for_version.at(static_cast<size_t>(version)) + k_id_delta;
  }
}

uint16_t MapState::index_for_enemy_state(Version version, std::shared_ptr<const EnemyState> ene_st) const {
  uint16_t relative_index = ene_st->super_ene->version(version).relative_enemy_index;
  return (relative_index == 0xFFFF)
      ? 0xFFFF
      : (relative_index + this->floor_config(ene_st->super_ene->floor).base_indexes_for_version(version).base_enemy_index);
}

uint16_t MapState::set_index_for_enemy_state(Version version, std::shared_ptr<const EnemyState> ene_st) const {
  uint16_t relative_set_index = ene_st->super_ene->version(version).relative_set_index;
  return (relative_set_index == 0xFFFF)
      ? 0xFFFF
      : (relative_set_index + this->floor_config(ene_st->super_ene->floor).base_indexes_for_version(version).base_enemy_set_index);
}

uint16_t MapState::index_for_event_state(Version version, std::shared_ptr<const EventState> ev_st) const {
  uint16_t relative_index = ev_st->super_ev->version(version).relative_event_index;
  return (relative_index == 0xFFFF)
      ? 0xFFFF
      : (relative_index + this->floor_config(ev_st->super_ev->floor).base_indexes_for_version(version).base_event_index);
}

std::shared_ptr<MapState::ObjectState> MapState::object_state_for_index(Version version, uint16_t object_index) {
  size_t dynamic_obj_base_index = this->dynamic_obj_base_index_for_version.at(static_cast<size_t>(version));
  if (object_index < dynamic_obj_base_index) {
    int8_t floor;
    for (floor = this->floor_config_entries.size() - 1; floor >= 0; floor--) {
      const auto& fc = this->floor_config_entries[floor];
      size_t base_object_index = fc.base_indexes_for_version(version).base_object_index;
      if (object_index >= base_object_index) {
        if (!fc.super_map) {
          throw std::out_of_range("there are no objects on the specified floor");
        }
        const auto& obj = fc.super_map->version(version).objects.at(object_index - base_object_index);
        return this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
      }
    }
    throw std::out_of_range("the specified enemy does not exist");

  } else {
    size_t k_id_delta = object_index - dynamic_obj_base_index;
    auto obj_st = std::make_shared<ObjectState>();
    obj_st->k_id = this->dynamic_obj_base_k_id + k_id_delta;
    obj_st->super_obj = nullptr;
    return obj_st;
  }
}

std::shared_ptr<MapState::ObjectState> MapState::object_state_for_index(Version version, uint8_t floor, uint16_t object_index) {
  size_t dynamic_obj_base_index = this->dynamic_obj_base_index_for_version.at(static_cast<size_t>(version));
  if (object_index < dynamic_obj_base_index) {
    const auto& fc = this->floor_config(floor);
    size_t base_object_index = fc.base_indexes_for_version(version).base_object_index;
    if (object_index < base_object_index) {
      throw std::runtime_error("object is not on the specified floor");
    }
    if (!fc.super_map) {
      throw std::out_of_range("there are no objects on the specified floor");
    }
    const auto& obj = fc.super_map->version(version).objects.at(object_index - base_object_index);
    return this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);

  } else {
    size_t k_id_delta = object_index - dynamic_obj_base_index;
    auto obj_st = std::make_shared<ObjectState>();
    obj_st->k_id = this->dynamic_obj_base_k_id + k_id_delta;
    obj_st->super_obj = nullptr;
    return obj_st;
  }
}

std::vector<std::shared_ptr<MapState::ObjectState>> MapState::object_states_for_floor_room_group(
    Version version, uint8_t floor, uint16_t room, uint16_t group) {
  std::vector<std::shared_ptr<ObjectState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& obj : fc.super_map->objects_for_floor_room_group(version, floor, room, group)) {
      ret.emplace_back(this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id));
    }
  }
  return ret;
}

std::vector<std::shared_ptr<MapState::ObjectState>> MapState::door_states_for_switch_flag(
    Version version, uint8_t floor, uint8_t switch_flag) {
  std::vector<std::shared_ptr<ObjectState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& obj : fc.super_map->doors_for_switch_flag(version, floor, switch_flag)) {
      ret.emplace_back(this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id));
    }
  }
  return ret;
}

std::shared_ptr<MapState::EnemyState> MapState::enemy_state_for_index(Version version, uint16_t enemy_index) {
  int8_t floor;
  for (floor = this->floor_config_entries.size() - 1; floor >= 0; floor--) {
    const auto& fc = this->floor_config_entries[floor];
    size_t base_enemy_index = fc.base_indexes_for_version(version).base_enemy_index;
    if (enemy_index >= base_enemy_index) {
      if (!fc.super_map) {
        throw std::out_of_range("there are no enemies on the specified floor");
      }
      const auto& ene = fc.super_map->version(version).enemies.at(enemy_index - base_enemy_index);
      return this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
    }
  }
  throw std::out_of_range("the specified enemy does not exist");
}

std::shared_ptr<MapState::EnemyState> MapState::enemy_state_for_index(Version version, uint8_t floor, uint16_t enemy_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_enemy_index = fc.base_indexes_for_version(version).base_enemy_index;
  if (enemy_index < base_enemy_index) {
    throw std::runtime_error("enemy is not on the specified floor");
  }
  if (!fc.super_map) {
    throw std::out_of_range("there are no enemies on the specified floor");
  }
  const auto& ene = fc.super_map->version(version).enemies.at(enemy_index - base_enemy_index);
  return this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
}

std::shared_ptr<MapState::EnemyState> MapState::enemy_state_for_set_index(Version version, uint8_t floor, uint16_t enemy_set_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_enemy_set_index = fc.base_indexes_for_version(version).base_enemy_set_index;
  if (enemy_set_index < base_enemy_set_index) {
    throw std::runtime_error("enemy is not on the specified floor");
  }
  if (!fc.super_map) {
    throw std::out_of_range("there are no enemies on the specified floor");
  }
  const auto& ene = fc.super_map->version(version).enemy_sets.at(enemy_set_index - base_enemy_set_index);
  return this->enemy_set_states.at(fc.base_super_ids.base_enemy_set_index + ene->super_set_id);
}

std::shared_ptr<MapState::EnemyState> MapState::enemy_state_for_floor_type(Version version, uint8_t floor, EnemyType type) {
  std::vector<std::shared_ptr<EnemyState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    const auto& ene = fc.super_map->enemy_for_floor_type(version, floor, type);
    return this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
  }
  throw std::out_of_range("map definition missing for floor");
}

std::vector<std::shared_ptr<MapState::EnemyState>> MapState::enemy_states_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) {
  std::vector<std::shared_ptr<EnemyState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ene : fc.super_map->enemies_for_floor_room_wave(version, floor, room, wave_number)) {
      ret.emplace_back(this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id));
    }
  }
  return ret;
}

std::shared_ptr<MapState::EventState> MapState::event_state_for_index(Version version, uint8_t floor, uint16_t event_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_event_index = fc.base_indexes_for_version(version).base_event_index;
  if (event_index < base_event_index) {
    throw std::runtime_error("event is not on the specified floor");
  }
  if (!fc.super_map) {
    throw std::out_of_range("there are no events on the specified floor");
  }
  const auto& ev = fc.super_map->version(version).events.at(event_index - base_event_index);
  return this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id);
}

std::vector<std::shared_ptr<MapState::EventState>> MapState::event_states_for_id(Version version, uint8_t floor, uint32_t event_id) {
  std::vector<std::shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_id(version, floor, event_id)) {
      ret.emplace_back(this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id));
    }
  }
  return ret;
}

std::vector<std::shared_ptr<MapState::EventState>> MapState::event_states_for_floor(Version version, uint8_t floor) {
  std::vector<std::shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_floor(version, floor)) {
      ret.emplace_back(this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id));
    }
  }
  return ret;
}

std::vector<std::shared_ptr<MapState::EventState>> MapState::event_states_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) {
  std::vector<std::shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_floor_room_wave(version, floor, room, wave_number)) {
      ret.emplace_back(this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id));
    }
  }
  return ret;
}

void MapState::import_object_states_from_sync(
    Version from_version, const SyncObjectStateEntry* entries, size_t entry_count) {
  this->log.info_f("Importing object state from sync command");
  size_t object_index = 0;
  for (const auto& fc : this->floor_config_entries) {
    if (!fc.super_map) {
      continue;
    }
    const auto& base_indexes = fc.base_indexes_for_version(from_version);
    if (object_index < base_indexes.base_object_index) {
      throw std::logic_error("floor config has incorrect base object index");
    }
    const auto& entities = fc.super_map->version(from_version);
    size_t fc_end_object_index = base_indexes.base_object_index + entities.objects.size();
    if (fc_end_object_index > entry_count) {
      // DC NTE sometimes has fewer objects than the map, but only by 1. TODO: Figure out why this happens.
      if (from_version == Version::DC_NTE) {
        fc_end_object_index = entry_count;
      } else {
        throw std::runtime_error(std::format(
            "the map has more objects (at least 0x{:X}) than the client has (0x{:X})",
            fc_end_object_index, entry_count));
      }
    }
    for (; object_index < fc_end_object_index; object_index++) {
      const auto& entry = entries[object_index];
      const auto& obj = entities.objects.at(object_index - base_indexes.base_object_index);
      auto& obj_st = this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
      if (obj_st->super_obj != obj) {
        throw std::logic_error("super object link is incorrect");
      }
      if (obj_st->game_flags != entry.flags) {
        this->log.info_f("({:04X} => K-{:03X}) Game flags from client ({:04X}) do not match game flags from map ({:04X})",
            object_index, obj_st->k_id, entry.flags, obj_st->game_flags);
        obj_st->game_flags = entry.flags;
      }
    }
  }
  // Pre-v1 versions have a bug where the client can overflow the decompression output buffer; the server handles the
  // invalid compressed data correctly but the resulting data size is larger than expected. We just ignore the extra
  // data on those versions.
  if (!is_pre_v1(from_version) && (object_index < entry_count)) {
    throw std::runtime_error(std::format("the client has more objects (0x{:X}) than the map has (0x{:X})",
        entry_count, object_index));
  }
}

void MapState::import_enemy_states_from_sync(Version from_version, const SyncEnemyStateEntry* entries, size_t entry_count) {
  this->log.info_f("Importing enemy state from sync command");
  size_t enemy_index = 0;
  for (const auto& fc : this->floor_config_entries) {
    if (!fc.super_map) {
      continue;
    }
    const auto& base_indexes = fc.base_indexes_for_version(from_version);
    if (enemy_index < base_indexes.base_enemy_index) {
      throw std::logic_error("floor config has incorrect base enemy index");
    }
    const auto& entities = fc.super_map->version(from_version);
    size_t fc_end_enemy_index = base_indexes.base_enemy_index + entities.enemies.size();
    if (fc_end_enemy_index > entry_count) {
      throw std::runtime_error(std::format("the map has more enemies than the client has (0x{:X})", entry_count));
    }
    for (; enemy_index < std::min<size_t>(fc_end_enemy_index, entry_count); enemy_index++) {
      const auto& entry = entries[enemy_index];
      const auto& ene = entities.enemies.at(enemy_index - base_indexes.base_enemy_index);
      auto& ene_st = this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
      // Only set the state if it's not an alias
      if (ene_st->super_ene == ene) {
        if (ene_st->game_flags != entry.flags) {
          this->log.info_f("({:04X} => E-{:03X}) Game flags from client ({:08X}) do not match game flags from map ({:08X})",
              enemy_index, ene_st->e_id, entry.flags, ene_st->game_flags);
          ene_st->game_flags = entry.flags;
        }
        if (ene_st->total_damage != entry.total_damage) {
          this->log.info_f("({:04X} => E-{:03X}) Total damage from client ({}) does not match total damage from map ({})",
              enemy_index, ene_st->e_id, entry.total_damage, ene_st->total_damage);
          ene_st->total_damage = entry.total_damage;
        }
      }
    }
  }
  // See note in MapState::import_object_states_from_sync
  if (!is_pre_v1(from_version) && (enemy_index < entry_count)) {
    throw std::runtime_error(std::format("the client has more enemies (0x{:X}) than the map has (0x{:X})",
        entry_count, enemy_index));
  }
}

void MapState::import_flag_states_from_sync(
    Version from_version,
    const le_uint16_t* object_set_flags,
    size_t object_set_flags_count,
    const le_uint16_t* enemy_set_flags,
    size_t enemy_set_flags_count,
    const le_uint16_t* event_flags,
    size_t event_flags_count) {
  {
    this->log.info_f("Importing object set flags from sync command");
    size_t object_index = 0;
    for (const auto& fc : this->floor_config_entries) {
      if (!fc.super_map) {
        continue;
      }
      const auto& base_indexes = fc.base_indexes_for_version(from_version);
      if (object_index < base_indexes.base_object_index) {
        throw std::logic_error("floor config has incorrect base object index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_object_index = base_indexes.base_object_index + entities.objects.size();
      if (fc_end_object_index > object_set_flags_count) {
        // DC NTE sometimes has fewer objects than the map, but only by 1. TODO: Figure out why this happens.
        if (from_version == Version::DC_NTE) {
          fc_end_object_index = object_set_flags_count;
        } else {
          throw std::runtime_error(std::format(
              "the map has more objects (at least 0x{:X}) than the client has (0x{:X})",
              fc_end_object_index, object_set_flags_count));
        }
      }
      for (; object_index < std::min<size_t>(fc_end_object_index, object_set_flags_count); object_index++) {
        uint16_t set_flags = object_set_flags[object_index];
        const auto& obj = entities.objects.at(object_index - base_indexes.base_object_index);
        auto& obj_st = this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
        if (obj_st->super_obj != obj) {
          throw std::logic_error("super object link is incorrect");
        }
        if (obj_st->set_flags != set_flags) {
          this->log.info_f("({:04X} => K-{:03X}) Set flags from client ({:04X}) do not match set flags from map ({:04X})",
              object_index, obj_st->k_id, set_flags, obj_st->set_flags);
          obj_st->set_flags = set_flags;
        }
      }
    }
    if (object_index < object_set_flags_count) {
      throw std::runtime_error(std::format("the client has more objects (0x{:X}) than the map has (0x{:X})",
          object_set_flags_count, object_index));
    }
  }

  {
    this->log.info_f("Importing enemy set flags from sync command");
    size_t enemy_set_index = 0;
    for (const auto& fc : this->floor_config_entries) {
      if (!fc.super_map) {
        continue;
      }
      const auto& base_indexes = fc.base_indexes_for_version(from_version);
      if (enemy_set_index < base_indexes.base_enemy_set_index) {
        throw std::logic_error("floor config has incorrect base enemy index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_enemy_set_index = base_indexes.base_enemy_set_index + entities.enemy_sets.size();
      if (fc_end_enemy_set_index > enemy_set_flags_count) {
        throw std::runtime_error("the map has more enemy sets than the client has");
      }
      for (; enemy_set_index < std::min<size_t>(fc_end_enemy_set_index, enemy_set_flags_count); enemy_set_index++) {
        uint16_t set_flags = enemy_set_flags[enemy_set_index];
        const auto& ene = entities.enemy_sets.at(enemy_set_index - base_indexes.base_enemy_set_index);
        auto& ene_st = this->enemy_set_states.at(fc.base_super_ids.base_enemy_set_index + ene->super_set_id);
        if (ene_st->super_ene != ene) {
          throw std::logic_error("super enemy link is incorrect");
        }
        if (ene_st->set_flags != set_flags) {
          this->log.info_f("({:04X} => E-{:03X}) Set flags from client ({:04X}) do not match set flags from map ({:04X})",
              enemy_set_index, ene_st->e_id, set_flags, ene_st->set_flags);
          ene_st->set_flags = set_flags;
        }
      }
    }
    if (enemy_set_index < enemy_set_flags_count) {
      throw std::runtime_error("the client has more enemy sets than the map has");
    }
  }

  {
    this->log.info_f("Importing event flags from sync command");
    size_t event_index = 0;
    for (const auto& fc : this->floor_config_entries) {
      if (!fc.super_map) {
        continue;
      }
      const auto& base_indexes = fc.base_indexes_for_version(from_version);
      if (event_index < base_indexes.base_event_index) {
        throw std::logic_error("floor config has incorrect base event index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_event_index = base_indexes.base_event_index + entities.events.size();
      if (fc_end_event_index > event_flags_count) {
        throw std::runtime_error("the map has more events than the client has");
      }
      for (; event_index < std::min<size_t>(fc_end_event_index, event_flags_count); event_index++) {
        uint16_t flags = event_flags[event_index];
        const auto& ev = entities.events.at(event_index - base_indexes.base_event_index);
        auto& ev_st = this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id);
        if (ev_st->flags != flags) {
          this->log.info_f("({:04X} => W-{:03X}) Set flags from client ({:04X}) do not match flags from map ({:04X})",
              event_index, ev_st->w_id, flags, ev_st->flags);
          ev_st->flags = flags;
        }
      }
    }
    if (event_index < event_flags_count) {
      throw std::runtime_error("the client has more events than the map has");
    }
  }
}

void MapState::verify() const {
  try {
    size_t total_object_count = 0;
    size_t total_enemy_count = 0;
    size_t total_enemy_set_count = 0;
    size_t total_event_count = 0;
    std::unordered_set<std::shared_ptr<const SuperMap>> verified_super_maps;
    for (const auto& fc : this->floor_config_entries) {
      if (fc.super_map && verified_super_maps.emplace(fc.super_map).second) {
        fc.super_map->verify();
        total_object_count += fc.super_map->all_objects().size();
        total_enemy_count += fc.super_map->all_enemies().size();
        total_enemy_set_count += fc.super_map->all_enemy_sets().size();
        total_event_count += fc.super_map->all_events().size();
      }
    }
    if (this->object_states.size() != total_object_count) {
      throw std::logic_error(std::format(
          "map state object count (0x{:X}) does not match supermap object count (0x{:X})",
          this->object_states.size(), total_object_count));
    }
    if (this->enemy_states.size() != total_enemy_count) {
      throw std::logic_error(std::format(
          "map state enemy count (0x{:X}) does not match supermap enemy count (0x{:X})",
          this->enemy_states.size(), total_enemy_count));
    }
    if (this->enemy_set_states.size() != total_enemy_set_count) {
      throw std::logic_error(std::format(
          "map state enemy set count (0x{:X}) does not match supermap enemy set count (0x{:X})",
          this->enemy_set_states.size(), total_enemy_set_count));
    }
    if (this->event_states.size() != total_event_count) {
      throw std::logic_error(std::format(
          "map state event count (0x{:X}) does not match supermap event count (0x{:X})",
          this->event_states.size(), total_event_count));
    }

    for (size_t k_id = 0; k_id < this->object_states.size(); k_id++) {
      const auto& obj_st = this->object_states[k_id];
      if (obj_st->k_id != k_id) {
        throw std::logic_error("mismatched object state k_id");
      }
      const auto& fc = this->floor_config(obj_st->super_obj->floor);
      if (fc.base_super_ids.base_object_index + obj_st->super_obj->super_id != k_id) {
        throw std::logic_error("mismatched object state super_id");
      }
    }
    for (size_t e_id = 0; e_id < this->enemy_states.size(); e_id++) {
      const auto& ene_st = this->enemy_states[e_id];
      if (ene_st->e_id != e_id) {
        throw std::logic_error("mismatched enemy state e_id");
      }
      const auto& fc = this->floor_config(ene_st->super_ene->floor);
      if (fc.base_super_ids.base_enemy_index + ene_st->super_ene->super_id != e_id) {
        throw std::logic_error("mismatched enemy state super_id");
      }
    }
    for (size_t set_id = 0; set_id < this->enemy_set_states.size(); set_id++) {
      const auto& ene_st = this->enemy_set_states[set_id];
      if (ene_st->set_id != set_id) {
        throw std::logic_error("mismatched enemy set state set_id");
      }
      const auto& fc = this->floor_config(ene_st->super_ene->floor);
      if (fc.base_super_ids.base_enemy_set_index + ene_st->super_ene->super_set_id != set_id) {
        throw std::logic_error("mismatched enemy set state super_set_id");
      }
    }
    for (size_t w_id = 0; w_id < this->event_states.size(); w_id++) {
      const auto& ev_st = this->event_states[w_id];
      if (ev_st->w_id != w_id) {
        throw std::logic_error("mismatched event state w_id");
      }
      const auto& fc = this->floor_config(ev_st->super_ev->floor);
      if (fc.base_super_ids.base_event_index + ev_st->super_ev->super_id != w_id) {
        throw std::logic_error("mismatched event state super_id");
      }
    }

    for (Version v : ALL_NON_PATCH_VERSIONS) {
      FloorConfig::EntityBaseIndexes base_indexes;
      for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
        const auto& fc = this->floor_config_entries[floor];
        const auto& fc_base_indexes = fc.base_indexes_for_version(v);
        if (base_indexes.base_object_index != fc_base_indexes.base_object_index) {
          throw std::logic_error("base object index does not match expected value");
        }
        if (base_indexes.base_enemy_index != fc_base_indexes.base_enemy_index) {
          throw std::logic_error("base enemy index does not match expected value");
        }
        if (base_indexes.base_enemy_set_index != fc_base_indexes.base_enemy_set_index) {
          throw std::logic_error("base enemy set index does not match expected value");
        }
        if (base_indexes.base_event_index != fc_base_indexes.base_event_index) {
          throw std::logic_error("base event set index does not match expected value");
        }
        if (fc.super_map) {
          const auto& entities = fc.super_map->version(v);
          base_indexes.base_object_index += entities.objects.size();
          base_indexes.base_enemy_index += entities.enemies.size();
          base_indexes.base_enemy_set_index += entities.enemy_sets.size();
          base_indexes.base_event_index += entities.events.size();
        }
      }
    }

    std::unordered_set<size_t> remaining_bb_rare_indexes;
    for (size_t index : this->bb_rare_enemy_indexes) {
      remaining_bb_rare_indexes.emplace(index);
    }

    for (const auto& ene : this->enemy_states) {
      if (!ene->is_rare(Version::BB_V4)) {
        continue;
      }
      if (ene->super_ene->is_default_rare_bb) {
        continue;
      }
      size_t base_enemy_index = this->floor_config(ene->super_ene->floor).base_indexes_for_version(Version::BB_V4).base_enemy_index;
      size_t enemy_index = base_enemy_index + ene->super_ene->version(Version::BB_V4).relative_enemy_index;
      if (!remaining_bb_rare_indexes.erase(enemy_index)) {
        throw std::logic_error(std::format("BB random rare enemy index {:04X} not present in indexes set", enemy_index));
      }
    }
    if (!remaining_bb_rare_indexes.empty()) {
      std::vector<std::string> indexes;
      for (uint16_t index : remaining_bb_rare_indexes) {
        indexes.emplace_back(std::format("{:04X}", index));
      }
      throw std::logic_error("not all BB random rare enemies were accounted for; remaining: " + phosg::join(indexes, ", "));
    }
  } catch (const std::exception&) {
    this->print(stderr);
    throw;
  }
}

void MapState::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "Difficulty {}, event {:02X}, state random seed {:08X}\n",
      name_for_difficulty(this->difficulty), this->event, this->random_seed);
  auto rare_rates_str = this->bb_rare_rates->str();
  phosg::fwrite_fmt(stream, "BB rare rates: {}\n", rare_rates_str);

  phosg::fwrite_fmt(stream, "Base indexes:\n");
  phosg::fwrite_fmt(stream, "  FL DC-NTE--------- DC-11-2000----- DC-V1---------- DC-V2---------- PC-NTE--------- PC-V2---------- GC-NTE--------- GC-V3---------- GC-EP3-NTE----- GC-EP3--------- XB-V3---------- BB-V4----------\n");
  phosg::fwrite_fmt(stream, "  FL KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT\n");
  for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
    auto fc = this->floor_config_entries[floor];
    if (fc.super_map) {
      phosg::fwrite_fmt(stream, "  {:02X}", floor);
      for (Version v : ALL_NON_PATCH_VERSIONS) {
        const auto& indexes = fc.base_indexes_for_version(v);
        phosg::fwrite_fmt(stream, " {:03X} {:03X} {:03X} {:03X}", indexes.base_object_index, indexes.base_enemy_index, indexes.base_enemy_set_index, indexes.base_event_index);
      }
      fputc('\n', stream);
    } else {
      phosg::fwrite_fmt(stream, "  {:02X} --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- ---------------\n", floor);
    }
  }

  phosg::fwrite_fmt(stream, "Objects:\n");
  phosg::fwrite_fmt(stream, "  FL OBJID DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 E3TE GCE3 XBV3 BBV4 OBJECT\n");
  for (const auto& obj_st : this->object_states) {
    phosg::fwrite_fmt(stream, "  {:02X} K-{:03X}", obj_st->super_obj->floor, obj_st->k_id);
    const auto& fc = this->floor_config(obj_st->super_obj->floor);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& obj_v = obj_st->super_obj->version(v);
      if (obj_v.relative_object_index == 0xFFFF) {
        fputs(" ----", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_object_index + obj_v.relative_object_index;
        phosg::fwrite_fmt(stream, " {:04X}", index);
      }
    }
    std::string obj_str = obj_st->super_obj->str();
    phosg::fwrite_fmt(stream, " {} game_flags={:04X} set_flags={:04X} item_drop_checked={}\n",
        obj_str, obj_st->game_flags, obj_st->set_flags, obj_st->item_drop_checked ? "true" : "false");
  }

  phosg::fwrite_fmt(stream, "Enemies:\n");
  phosg::fwrite_fmt(stream, "  FL ENEID DCTE----- DCPR----- DCV1----- DCV2----- PCTE----- PCV2----- GCTE----- GCV3----- EP3TE---- GCEP3---- XBV3----- BBV4----- ENEMY\n");
  for (const auto& ene_st : this->enemy_states) {
    phosg::fwrite_fmt(stream, "  {:02X} E-{:03X}", ene_st->super_ene->floor, ene_st->e_id);
    const auto& fc = this->floor_config(ene_st->super_ene->floor);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ene_v = ene_st->super_ene->version(v);
      if (ene_v.relative_enemy_index == 0xFFFF) {
        fputs(" ---------", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_enemy_index + ene_v.relative_enemy_index;
        uint16_t set_index = fc.base_indexes_for_version(v).base_enemy_set_index + ene_v.relative_set_index;
        phosg::fwrite_fmt(stream, " {:04X}-{:04X}", index, set_index);
      }
    }
    std::string ene_str = ene_st->super_ene->str();
    phosg::fwrite_fmt(stream, " {} total_damage={:04X} rare_flags={:04X} game_flags={:08X} set_flags={:04X} server_flags={:04X}\n",
        ene_str, ene_st->total_damage, ene_st->rare_flags, ene_st->game_flags, ene_st->set_flags, ene_st->server_flags);
  }

  if (this->bb_rare_enemy_indexes.empty()) {
    phosg::fwrite_fmt(stream, "BB rare enemy indexes: (none)\n");
  } else {
    std::string s;
    for (auto index : this->bb_rare_enemy_indexes) {
      s += std::format(" {:04X}", index);
    }
    phosg::fwrite_fmt(stream, "BB rare enemy indexes:{}\n", s);
  }

  phosg::fwrite_fmt(stream, "Events:\n");
  phosg::fwrite_fmt(stream, "  FL EVTID DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 E3TE GCE3 XBV3 BBV4 EVENT\n");
  for (const auto& ev_st : this->event_states) {
    phosg::fwrite_fmt(stream, "  {:02X} W-{:03X}", ev_st->super_ev->floor, ev_st->w_id);
    const auto& fc = this->floor_config(ev_st->super_ev->floor);
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      const auto& ev_v = ev_st->super_ev->version(v);
      if (ev_v.relative_event_index == 0xFFFF) {
        fputs(" ----", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_event_index + ev_v.relative_event_index;
        phosg::fwrite_fmt(stream, " {:04X}", index);
      }
    }
    phosg::fwrite_fmt(stream, " {} set_flags={:04X}\n", ev_st->super_ev->str(), ev_st->flags);
  }
}

static constexpr uint64_t room_layout_key(uint8_t area, uint8_t layout_var, uint32_t room_id) {
  return ((static_cast<uint64_t>(area) << 40) |
      (static_cast<uint64_t>(layout_var) << 32) |
      static_cast<uint64_t>(room_id));
}

RoomLayoutIndex::RoomLayoutIndex(const phosg::JSON& json) {
  for (const auto& [area_and_layout_var, rooms_json] : json.as_dict()) {
    auto tokens = phosg::split(area_and_layout_var, '-');
    uint8_t area = stoul(tokens.at(0), nullptr, 16);
    uint8_t layout_var = stoul(tokens.at(1), nullptr, 16);
    for (const auto& [room_id_str, room_json] : rooms_json->as_dict()) {
      uint32_t room_id = stoul(room_id_str, nullptr, 16);
      auto emplace_ret = this->rooms.emplace(room_layout_key(area, layout_var, room_id), Room());
      auto& room = emplace_ret.first->second;
      const auto& l = room_json->as_list();
      room.position.x = l.at(0)->as_float();
      room.position.y = l.at(1)->as_float();
      room.position.z = l.at(2)->as_float();
      room.angle.x = l.at(3)->as_int();
      room.angle.y = l.at(4)->as_int();
      room.angle.z = l.at(5)->as_int();
    }
  }
}

const RoomLayoutIndex::Room& RoomLayoutIndex::get_room(uint8_t area, uint8_t layout_var, uint32_t room_id) const {
  return this->rooms.at(room_layout_key(area, layout_var, room_id));
}
