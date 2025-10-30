#include "Map.hh"

#include <stdio.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "CommonFileFormats.hh"
#include "ItemCreator.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "Quest.hh"
#include "StaticGameData.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Set data table (variations index)

string Variations::str() const {
  string ret = "";
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
    Episode episode, bool is_solo, shared_ptr<RandomGenerator> rand_crypt) const {
  Variations ret;
  for (size_t floor = 0; floor < ret.entries.size(); floor++) {
    auto& e = ret.entries[floor];
    auto num_vars = this->num_free_play_variations_for_floor(episode, is_solo, floor);
    e.layout = (num_vars.layout > 1) ? (rand_crypt->next() % num_vars.layout) : 0;
    e.entities = (num_vars.entities > 1) ? (rand_crypt->next() % num_vars.entities) : 0;
  }
  return ret;
}

vector<string> SetDataTableBase::map_filenames_for_variations(
    Episode episode, GameMode mode, const Variations& variations, FilenameType type) const {
  vector<string> ret;
  for (uint8_t floor = 0; floor < 0x10; floor++) {
    const auto& e = variations.entries[floor];
    ret.emplace_back(this->map_filename_for_variation(episode, mode, floor, e.layout, e.entities, type));
  }
  for (uint8_t floor = 0x10; floor < 0x12; floor++) {
    ret.emplace_back(this->map_filename_for_variation(episode, mode, floor, 0, 0, type));
  }
  return ret;
}

uint8_t SetDataTableBase::default_area_for_floor(Version version, Episode episode, uint8_t floor) {
  // For some inscrutable reason, Pioneer 2's area number in Episode 4 is
  // discontiguous with all the rest. Why, Sega??
  static const array<uint8_t, 0x12> areas_ep1 = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};
  static const array<uint8_t, 0x12> areas_ep2_gc_nte = {
      0x00, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0xFF, 0xFF};
  static const array<uint8_t, 0x12> areas_ep2 = {
      0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23};
  static const array<uint8_t, 0x12> areas_ep4 = {
      0x2D, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  switch (episode) {
    case Episode::EP1:
      return areas_ep1.at(floor);
    case Episode::EP2: {
      const auto& areas = ((version == Version::GC_NTE) ? areas_ep2_gc_nte : areas_ep2);
      return areas.at(floor);
    }
    case Episode::EP4:
      return areas_ep4.at(floor);
    default:
      throw logic_error("incorrect episode");
  }
}

uint8_t SetDataTableBase::default_area_for_floor(Episode episode, uint8_t floor) const {
  return this->default_area_for_floor(this->version, episode, floor);
}

SetDataTable::SetDataTable(Version version, const string& data) : SetDataTableBase(version) {
  if (is_big_endian(this->version)) {
    this->load_table_t<true>(data);
  } else {
    this->load_table_t<false>(data);
  }
}

template <bool BE>
void SetDataTable::load_table_t(const string& data) {
  using FooterT = RELFileFooterT<BE>;

  phosg::StringReader r(data);

  if (r.size() < sizeof(FooterT)) {
    throw runtime_error("set data table is too small");
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
  uint8_t area = this->default_area_for_floor(episode, floor);
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
  uint8_t area = this->default_area_for_floor(episode, floor);
  if (area == 0xFF) {
    return Variations::Entry{.layout = 1, .entities = 1};
  }
  static const array<uint32_t, 0x2F * 2> counts_on = {
      // Episode 1 (00-11)
      // P2 -F1-, -F2-, -C1-, -C2-, -C3-, -M1-, -M2-, -R1-, -R2-, -R3-, DRGN, DRL-, -VO-, -DF-, LOBBY, VS1-, VS2-,
      1, 1, 1, 5, 1, 5, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 10, 1, 1, 1, 1, 1,
      // Episode 2 (12-23)
      // P2 VRTA, VRTB, VRSA, VRSB, CCA-, -JN-, -JS-, MNTN, SEAS, SBU-, SBL-, -GG-, -OF-, -BR-, -GD-, SSN-, TWR-,
      1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 1, 3, 1, 3, 2, 2, 1, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      // Episode 4 (24-2E)
      // CE -CW-, -CS-, -CN-, -CI-, DES1, DES2, DES3, SMIL, -P2-, TEST
      1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1};
  static const array<uint32_t, 0x2F * 2> counts_off = {
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
        .layout = min<uint32_t>(available.layout, data[area * 2]),
        .entities = min<uint32_t>(available.entities, data[area * 2 + 1]),
    };
  }
  throw runtime_error("invalid area");
}

string SetDataTable::map_filename_for_variation(
    Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  uint8_t area = this->default_area_for_floor(episode, floor);
  if (area == 0xFF) {
    return "";
  }

  if (area >= this->entries.size()) {
    return "";
  }

  const auto& entry = this->entries.at(area).at(layout).at(entities);

  string filename;
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
      throw logic_error("invalid map filename type");
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
      throw logic_error("invalid game mode");
  }

  return filename;
}

string SetDataTable::str() const {
  vector<string> lines;
  lines.emplace_back(std::format("FL/V1/V2 => ----------------------OBJECT -----------------ENEMY+EVENT -----------------------SETUP\n"));
  for (size_t a = 0; a < this->entries.size(); a++) {
    const auto& v1_v = this->entries[a];
    for (size_t v1 = 0; v1 < v1_v.size(); v1++) {
      const auto& v2_v = v1_v[v1];
      for (size_t v2 = 0; v2 < v2_v.size(); v2++) {
        const auto& e = v2_v[v2];
        lines.emplace_back(std::format("{:02X}/{:02X}/{:02X} => {:28} {:28} {:28}\n",
            a, v1, v2, e.object_list_basename, e.enemy_and_event_list_basename, e.area_setup_filename));
      }
    }
  }
  return phosg::join(lines, "");
}

struct AreaMapFileInfo {
  const char* name_token;
  vector<uint32_t> variation1_values;
  vector<uint32_t> variation2_values;

  AreaMapFileInfo(
      const char* name_token,
      vector<uint32_t> variation1_values,
      vector<uint32_t> variation2_values)
      : name_token(name_token),
        variation1_values(variation1_values),
        variation2_values(variation2_values) {}
};

const array<vector<vector<string>>, 0x12> SetDataTableDCNTE::NAMES = {{
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

string SetDataTableDCNTE::map_filename_for_variation(
    Episode, GameMode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  try {
    string basename = this->NAMES.at(floor).at(layout).at(entities);
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
        throw logic_error("invalid map filename type");
    }
    return basename;
  } catch (const out_of_range&) {
    return "";
  }
}

const array<vector<vector<string>>, 0x12> SetDataTableDC112000::NAMES = {{
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

string SetDataTableDC112000::map_filename_for_variation(
    Episode, GameMode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const {
  try {
    string basename = this->NAMES.at(floor).at(layout).at(entities);
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
        throw logic_error("invalid map filename type");
    }
    return basename;
  } catch (const out_of_range&) {
    return "";
  }
}

static const vector<AreaMapFileInfo> map_file_info_dc_nte = {
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

static const vector<vector<AreaMapFileInfo>> map_file_info_gc_nte = {
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
static const vector<vector<vector<AreaMapFileInfo>>> map_file_info = {
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

////////////////////////////////////////////////////////////////////////////////
// DAT file structure

struct DATEntityDefinition {
  // This field directly maps to the base_type field in ObjectSetEntry and
  // EnemySetEntry.
  uint16_t base_type;
  // Each bit in this field indicates whether the definition applies to that
  // version or not. Earlier versions are in less-significant bits.
  uint16_t version_flags;
  // Each bit in this field indicates whether the definition applies to that
  // area or not. Ep1 Pioneer 2 is the least-significant bit. Note that Episode
  // 3 only has Morgue (0x1), battle (0x2), and lobby (0x8000) areas, so only
  // those bits can be set here if version_flags is F_EP3.
  uint64_t area_flags;
  // This is the internal name of the class as specified in the client, if it's
  // available (if not, this is a somewhat-descriptive made-up name).
  const char* name;
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

static const vector<DATEntityDefinition> dat_object_definitions({
    // This is newserv's canonical list of map object types.

    // Objects defined in map files take arguments in the form of an
    // ObjectSetEntry structure (see Map.hh). Most objects take parameters
    // only in param1-3 (floats) and param4-6 (ints), but a few of them use
    // the angle fields as additional int parameters. All objects are
    // available on all versions of the game (except Episode 3) unless
    // otherwise noted, but most objects are available only on specific
    // floors unless an omnispawn patch is used.

    // Defines where a player should start when entering a floor. Params:
    //   param1 = client ID
    //   param4 = source type:
    //     0 = use this set when advancing from a lower floor
    //     1 = use this set when returning from a higher floor
    //     anything else = set is unused
    {0x0000, F_V0_V4, 0x00007FFFFFFFFFFF, "TObjPlayerSet"},
    {0x0000, F_EP3, 0x0000000000008001, "TObjPlayerSet"},

    // Displays a particle effect. This object is not constructed in
    // split-screen mode. Params:
    //   param1 = particle type (truncated to int, clamped to nonnegative)
    //   param3 = TODO
    //   param4 = if equal to 1, increase draw distance from 200 to 1500; if
    //     any other value, no effect
    //   param5 = TODO
    //   param6 = TODO
    {0x0001, F_V0_V4, 0x00006FFFFFFFFFFF, "TObjParticle"},
    {0x0001, F_EP3, 0x0000000000008003, "TObjParticle"},

    // Standard (triangular) cross-floor warp object. Params:
    //   param4 = destination floor
    //   param6 = color (0 = blue, 1 = red); if this is 0 in Challenge mode,
    //     the warp is destroyed immediately
    {0x0002, F_V0_V4, 0x00007FF3C07C78FF, "TObjAreaWarpForest"},

    // Standard (triangular) intra-floor warp object. Params:
    //   param1-3 = destination coordinates (x, y, z); players are supposed
    //     to be offset from this location in different directions depending
    //     on their client ID, but there is a bug that causes all players to
    //     use the same offsets: x + 10 and z + 10
    //   param4 = destination angle (about y axis)
    {0x0003, F_V0_V4, 0x00007FFC3FFF78FF, "TObjMapWarpForest"},

    // Light collision. Params:
    //   param1 = TODO (in range 0-10000; if above 10000, (param1 - 10000) is
    //     used and a flag is set)
    //   param2 = TODO (in range 0-10000; if above 10000, (param2 - 10000) is
    //     used and a flag is set)
    //   param3 = TODO
    //   param4 = TODO
    //   param5 = TODO
    //   param6 = TODO
    {0x0004, F_V0_V4, 0x00006FFC3FFF87FF, "TObjLight"},
    {0x0004, F_EP3, 0x0000000000008003, "TObjLight"},

    // Arbitrary item. The parameters specify the item data; see
    // ItemCreator::base_item_for_specialized_box for how the encoding works.
    {0x0005, F_V0_V2, 0x000000000000073F, "TItem"},

    // Environmental sound. This object is not constructed in offline multi
    // mode. Params:
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
    //       02 = bubbles GC: (0004BC0E0
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
    //   param5 = volume (in range -0x7F to 0x7F)
    {0x0006, F_V0_V2, 0x0000000000037FFF, "TObjEnvSound"},
    {0x0006, F_V3_V4, 0x00006FF0BFFF27FF, "TObjEnvSound"},
    {0x0006, F_EP3, 0x0000000000000001, "TObjEnvSound"},

    // Fog collision object. Params:
    //   param1 = radius
    //   param4 = fog entry number; if lower than the existing fog number,
    //     does nothing (if param4 is >= 0x1000, the game subtracts 0x1000
    //     from it, but only after comparing it to the current fog number;
    //     this can be used to override a later fog with an earlier fog)
    //   param5 = transition type (0 = fade in, 1 = instantly switch)
    {0x0007, F_V0_V4, 0x00006FFFFFFF7FFF, "TObjFogCollision"},
    {0x0007, F_EP3, 0x0000000000000001, "TObjFogCollision"},

    // Event collision object. This object triggers a wave event (W-XXX) when
    // any local player (in split-screen play, there may be multiple) steps
    // within its radius. The object is inactive for 3 frames after it is
    // constructed, and will not detect any player during that time.
    // Curiously, the frame counter does not appear to be bounded, so if the
    // player waits approximately 828 days, the counter will overflow to a
    // positive number and the object won't be able to trigger for another
    // 828 days until it gets to zero again. It is unlikely this has ever
    // happened for any player. Params:
    //   param1 = radius
    //   param4 = event ID
    {0x0008, F_V0_V4, 0x00007FFFFFFF7FFF, "TObjEvtCollision"},
    {0x0008, F_EP3, 0x0000000000000001, "TObjEvtCollision"},

    // TODO: Describe this object. Params:
    //   param1 = TODO
    //   param2 = TODO
    //   param3 = TODO (it only matters whether this is negative or not)
    {0x0009, F_V0_V4, 0x000060000004073F, "TObjCollision"},
    {0x0009, F_EP3, 0x0000000000000001, "TObjCollision"},

    // Elemental trap. Params:
    //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
    //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
    //   param3 = trap group number:
    //     negative = trap triggers and explodes alone
    //     0 = trap follows player who triggered it (online only; when
    //       offline, these act as if the group number were negative, and
    //       param6 is overwritten with 30 (1 second))
    //     positive = trap is part of a group that all trigger and explode
    //       simultaneously
    //   param4 = trap power (clamped below to 0), scaled by difficulty:
    //     Normal: power = param4 * 1
    //     Hard: power = param4 * 3
    //     Very Hard: power = param4 * 5
    //     Ultimate: power = param4 * 14
    //   param5 = damage type (clamped to [0, 5])
    //     00 = direct damage (damage = power / 5)
    //     01 = fire (damage = power * (100 - EFR) / 500)
    //     02 = cold (damage = power * (100 - EIC) / 500; chance of freezing =
    //       ((((power - 250) / 40) + 5) / 40) clamped to [0, 0.4], or to [0.2,
    //       0.4] on Ultimate)
    //     03 = electric (damage = power * (100 - EIC) / 500; chance of shock =
    //       1/15, or 1/40 on Ultimate)
    //     04 = light (damage = power * (100 - ELT) / 500)
    //     05 = dark (instantly kills with chance (power - EDK) / 100; if used
    //       in a boss arena and in non-Ultimate mode, cannot kill)
    //   param6 = number of frames between trigger and explosion
    {0x000A, F_V0_V4, 0x00005FFC3FFB07FE, "TOMineIcon01"},

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
    {0x000B, F_V0_V4, 0x00005FFC3FFB07FE, "TOMineIcon02"},

    // Heal trap. Params:
    //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
    //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
    //   param3 = trap group number (same as in TOMineIcon01)
    //   param4 = trap power (same as in TOMineIcon01)
    //   param5 = trap type (clamped to [6, 8])
    //     06 = heal HP (amount = power)
    //     07 = clear negative status effects
    //     08 = does nothing? (TODO: calls player->vtable[0x40], but that
    //       function does nothing on v3. Did this do something in v1 or v2?)
    //   param6 = number of frames between trigger and explosion
    {0x000C, F_V0_V4, 0x00005FFC3FFB07FE, "TOMineIcon03"},

    // Large elemental trap. Params:
    //   param1 = trigger radius delta (actual radius is param1 / 2 + 60)
    //   param2 = explosion radius delta (actual radius is param2 / 2 + 120)
    //   param3 = trap group number (same as in TOMineIcon01)
    //   param4 = trap power (same as in TOMineIcon01)
    //   param5 = trap type (same as in TOMineIcon01)
    //   param6 = number of frames between trigger and explosion
    {0x000D, F_V0_V4, 0x00005FFC3FFB07FE, "TOMineIcon04"},

    // Room ID. Params:
    //   param1 = radius (actual radius = (param1 * 10) + 30)
    //   param2 = next room ID
    //   param3 = previous room ID
    //   param5 = angle
    //   param6 = TODO (debug info calls this "BLOCK ID"; seems it only
    //     matters whether this is 0x10000 or not)
    {0x000E, F_V0_V4, 0x00005FFFFFF83FFE, "TObjRoomId"},

    // Sensor of some kind (TODO). Params:
    //   param1 = activation radius delta (actual radius = param1 + 50)
    //   param4 = switch flag number
    //   param5 = update mode switch (TODO; param5 < 0 sets update_mode =
    //     PERMANENTLY_ON, otherwise TEMPORARY; see TOSensor_vF)
    {0x000F, F_V0_V4, 0x00004000000000F6, "TOSensorGeneral01"},

    // Lens flare effect. This object is not constructed in offline multi
    // mode. Params:
    //   param1 = visibility radius (if negative, visible everywhere)
    {0x0011, F_V0_V4, 0x000040000000411E, "TEF_LensFlare"},

    // Quest script trigger. Starts a quest thread at a specific label when
    // the local player goes near the object. Params:
    //   param1 = radius
    //   param4 = label number to call when local player is within radius
    {0x0012, F_V0_V4, 0x00006FFFFFFC7FFF, "TObjQuestCol"},
    {0x0012, F_EP3, 0x0000000000000001, "TObjQuestCol"},

    // Healing ring. Removes all negative status conditions and adds 9 HP and
    // 9 TP per frame until max HP/TP are both reached or the player leaves
    // the radius. The radius is a fixed size.
    {0x0013, F_V0_V4, 0x00004FFC3FF807FE, "TOHealGeneral"},

    // Invisible map collision. Params:
    //   param1-3 = box dimensions (x, y, z; rotated by angle fields)
    //   param4 = wall type:
    //     00 = custom (see param5)
    //     01 = blocks enemies only (as if param5 = 0x00008000)
    //     02 = blocks enemies and players (as if param5 = 0x00008900)
    //     03 = blocks enemies and players, but enemies can see targets
    //       through the collision (as if param5 = 0x00000800)
    //     04 = blocks players only (as if param5 = 0x00002000)
    //     05 = undefined behavior due to missing bounds check
    //     anything else = same as 01
    //   param5 = flags (bit field; used if param4 = 0) (TODO: describe bits)
    {0x0014, F_V0_V4, 0x0000600C3F87073F, "TObjMapCsn"},
    {0x0014, F_EP3, 0x0000000000000001, "TObjMapCsn"},

    // Like TObjQuestCol, but requires the player to press a controller
    // button to trigger the call. Parameters are the same as for
    // TObjQuestCol.
    {0x0015, F_V0_V4, 0x00006FFFFFFC7FFF, "TObjQuestColA"},
    {0x0015, F_EP3, 0x0000000000000001, "TObjQuestColA"},

    // TODO: Describe this object. Params:
    //   param1 = radius (if negative, 30 is used)
    {0x0016, F_V0_V4, 0x00006FFFFFFCFFFF, "TObjItemLight"},
    {0x0016, F_EP3, 0x0000000000008001, "TObjItemLight"},

    // Radar collision. Params:
    //   param1 = radius
    //   param4 = TODO
    {0x0017, F_V0_V4, 0x00004FFFFFF8FFFE, "TObjRaderCol"},
    {0x0017, F_EP3, 0x0000000000008000, "TObjRaderCol"},

    // Fog collision. Same params as 0x0007 (TObjFogCollision), but also:
    //   param3 = if >= 1, fog is on when switch flag is on; otherwise, fog
    //     is on when switch flag is off
    //   param6 = switch flag number
    {0x0018, F_V0_V4, 0x00004FFFFFF87FFE, "TObjFogCollisionSwitch"},

    // Boss teleporter. The destination cannot be changed; it always
    // teleports the player to the boss arena for the current area. In
    // Challenge mode, the game uses the current area number to determine the
    // destination floor, but in other modes, it uses the current episode
    // number and floor number (not area number). Assuming areas haven't been
    // reassigned from the defaults in non-Challenge mode, the mapping is:
    //   Challenge (indexed by area number, not floor number):
    //     Episode 1:
    //       Pioneer 2, boss arenas, lobby, battle areas => Pioneer 2
    //       Forest 1 and 2 => Dragon
    //       Cave 1, 2, and 3 => De Rol Le
    //       Mine 1 and 2 => Vol Opt
    //       Ruins 1, 2, and 3 => Dark Falz
    //     Episode 2:
    //       Lab, boss arenas, Seaside Night, Tower => Lab
    //       VR Temple A and B => Barba Ray
    //       VR Spaceship A and B => Gol Dragon
    //       Central Control Area (but not Seaside Night) => Gal Gryphon
    //       Seabed => Olga Flow
    //     Episode 4:
    //       Pioneer 2, boss arena => Pioneer 2
    //       Crater, Desert, test map => Saint-Milion arena
    // Params:
    //   param5 = switch flag number required to activate warp (>= 0x100 = no
    //     switch flag required; ignored if on Pioneer 2)
    // In offline mode, this object constructs TObjWarpBossMulti instead.
    {0x0019, F_V0_V4, 0x00006FFC3FFC04A5, "TObjWarpBoss"},

    // Sign board. This shows the loaded image from a quest (via load_pvr). On
    // the final version of PCv2, this object doesn't render at all; my guess
    // is that Sega hardcoded the PVR filenames for the various events in the
    // executable, then after those events ended, they just deleted the
    // load_pvr code entirely, leaving this object nonfunctional.
    // Params:
    //   param1-3 = scale factors (x, y, z)
    {0x001A, F_V1_V4, 0x0000600000040001, "TObjSinBoard"},
    {0x001A, F_EP3, 0x0000000000000001, "TObjSinBoard"},

    // Quest floor warp. This appears similar to TObjAreaWarpForest except
    // that the object is not destroyed immediately if it's blue and the game
    // is Challenge mode. Params:
    //   param1 = player set ID (TODO: what exactly does this do? Looks like it
    //     does nothing unless it's >= 2)
    //   param4 = destination floor
    //   param6 = color (0 = blue, 1 = red)
    {0x001B, F_V1_V4, 0x00005000000078FE, "TObjAreaWarpQuest"},

    // Ending movie warp (used in final boss arenas after the boss is
    // defeated). Params:
    //   param6 = color (0 = blue, 1 = red)
    {0x001C, F_V1_V4, 0x0000500080004000, "TObjAreaWarpEnding"},

    // Star light effect.
    // This object renders from -100 to 740 over x and -100 to 580 over y.
    // Params:
    //   param1 = TODO
    //   param2 = TODO
    //   param3 = TODO
    //   param4 = TODO
    //   param5 = TODO
    //   param6 = TODO
    {0x001D, F_V2_V4, 0x0000400000000002, "TEffStarLight2D_Base"},

    // VR Temple Beta / Barba Ray lens flare effect.
    // This object renders from -10 to 650 over x and -10 to 490 over y.
    {0x001E, F_V2_V4, 0x000041F1001A0006, "__LENS_FLARE__"},

    // Hides the area map when the player is near this object. Params:
    //   param1 = radius
    // TODO: Test this.
    {0x001F, F_V2_V4, 0x00004FFC3FFB07FE, "TObjRaderHideCol"},

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
    //   param6 = item amount required minus 1 (0 = 1 item, 1 = 2 items, etc.);
    //     for tools and meseta, each dropped item counts for its stack size
    {0x0020, F_V2_V4, 0x00006FFC3FFF07FF, "TOSwitchItem"},

    // Symbol chat collision object. This object triggers symbol chats when
    // the players are nearby and certain switch flags are NOT set. If a
    // player is within the radius, the object checks the switch flags in
    // reverse order, and triggers the symbol chat for the latest one that is
    // NOT set. So, the logic is:
    // - If all 3 switch flags are not set, the symbol chat in spec1 is used
    // - If the switch flag in spec1 is set and those in spec2 and spec3 are
    //   not set, the symbol chat in spec2 is used
    // - If the switch flag in spec2 is set and the flag in spec3 is not set,
    //   the symbol chat in spec3 is used regardless of whether the switch
    //   flags in spec1 is set
    // - If the switch flags in spec3 is set, no symbol chat appears at all
    //   regardless of the values of the other two switch flags
    // Each spec is a 32-bit integer consisting of two 16-bit fields. The
    // high 16 bits are a switch flag number (0-255), and the low 16 bits are
    // an entry index from symbolchatcolli.prs. The entry index is ignored if
    // the corresponding data label from the F8A6 quest opcode is not null
    // (and the data from the label is used instead). Quest scripts don't
    // have a good way to pass null for regsA[7-9], so the logic that looks
    // up entries in symbolchatcolli.prs can be ignored in quests.
    // Default symbol chat numbers (from qedit.info):
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
    // This object can also be created by the quest opcode F8A6; see the
    // description of that opcode in QuestScript.cc for more information.
    {0x0021, F_V2_V4, 0x00006FFC3FFF07FF, "TOSymbolchatColli"},

    // Invisible collision switch. Params:
    //   param1 = radius delta (actual radius is param1 + 10)
    //   param4 = switch flag number
    //   param5 = sticky flag (if negative, switch flag is unset when player
    //     leaves; if zero or positive, switch flag remains on)
    {0x0022, F_V2_V4, 0x00004FFC3FFB07FE, "TOKeyCol"},

    // Attackable collision. Params:
    //   param1 = enable switch flag (the object is only attackable if this
    //     switch flag is enabled); any value > 0xFF disables this behavior
    //     so the object is always attackable
    //   param2 = if negative, no target reticle appears; if zero or
    //     positive, a reticle appears
    //   param3 = switch flag number to set when required number of hits is
    //     taken (ignored if param6 is nonzero)
    //   param4 = number of hits required to activate, minus 1 (so e.g. a
    //     value of 4 here means 5 hits needed)
    //   param5 = object number (if outside the range [100, 999], uses the
    //     free play script when looking up param6 instead of the quest)
    //   param6 = quest label to call when required number of hits is taken
    //     (if zero, switch flag in param3 is set instead)
    {0x0023, F_V2_V4, 0x00004FFC3FFB07FE, "TOAttackableCol"},

    // Damage effect. Params:
    //   angle.x = effect type (in range [0x00, 0x14]; TODO: list these)
    //   param1 = damage radius
    //   param2 = damage value, scaled by difficulty:
    //     Normal: param2 * 2
    //     Hard: param2 * 4
    //     Very Hard: param2 * 6
    //     Ultimate: param2 * 8
    //   param3 = effect visual size
    //   param4 = enable switch flag number (effect is off unless this flag
    //     is set)
    //   param5 = disable switch flag number (effect is off if this flag is
    //     set, even if the enable switch flag is also set), or >= 0x100 if
    //     this functionality isn't needed
    //   param6 = persistence flag (if nonzero, effect stays on once enabled)
    {0x0024, F_V2_V4, 0x0000600FFF9F07FF, "TOSwitchAttack"},

    // Switch flag timer. This object watches for a switch flag to be
    // activated, then once it is, waits for a specified time, then disables
    // that switch flag and activates up to three other switch flags. Note
    // that this object just provides the timer functionality; the trigger
    // switch flag must be activated by some other object. Params:
    //   angle.x = trigger mode:
    //     0 = disable watched switch flag when timer expires
    //     any positive number = enable up to 3 other switch flags when timer
    //       expires and don't disable watched switch flag
    //   angle.y = if this is 1, play tick sound effect every second after
    //     activation (if any other value, no tick sound is played)
    //   angle.z = timer duration in frames
    //   param4 = switch flag to watch for activation in low 16 bits, switch
    //     flag 1 to activate when timer expires (if angle.x != 0) in high 16
    //     bits (>= 0x100 if not needed)
    //   param5 = switch flag 2 to activate when timer expires (if
    //     angle.x != 0) in high 16 bits (>= 0x100 if not needed)
    //   param6 = switch flag 3 to activate when timer expires (if
    //     angle.x != 0) in high 16 bits (>= 0x100 if not needed)
    {0x0025, F_V2_V4, 0x00006FFC3FFF07FF, "TOSwitchTimer"},

    // Chat sensor. This object watches for chat messages said by players
    // within its radius, optionally filtering for specific words. When a
    // message matches, it either activates a switch flag or calls a quest
    // function. When created via the F801 quest opcode, param5 is ignored,
    // and the string specified by the quest is always used. Params:
    //   angle.x = activation type (0 = quest function, 1 = switch flag)
    //   angle.y = match mode:
    //     0 = match a specific string (defined by param5)
    //     1 = match any string
    //     anything else = never match anything
    //   param1 = radius
    //   param4 = switch flag number or quest label number
    //   param5 = trigger string
    //     0 = any string (TODO: Is this true?)
    //     1 = "YES"
    //     2 = "COOL"
    //     3 = "NO"
    {0x0026, F_V2_V4, 0x00006FFC3FFF07FF, "TOChatSensor"},

    // Radar map icon. Shows an icon on the map that is optionally locked or
    // unlocked, depending on the values of one or more sequential switch
    // flags. The icon is considered unlocked if all of the specified switch
    // flags are set. There can be at most 12 switch flags used with this
    // object and the switch flag numbers cannot wrap around from 0xFF to 0.
    // Params:
    //   param1 = scale (1.0 = normal size)
    //   param4 = check switch flags for color:
    //     positive value = ignore param6; color is red (FFFF0000) if any of
    //       the switch flags are off, or green (FF00FF00) if all are on
    //     zero or negative = ignore param5; always render in the color
    //       specified by param6
    //   param5 = switch flag spec; upper 16 bits are number of switch flags,
    //     lower 16 bits are first switch flag number
    //   param6 = color as ARGB8888
    {0x0027, F_V3_V4, 0x00004FFFFFFC0000, "TObjRaderIcon"},

    // Environmental sound. This object is not constructed in offline multi
    // mode. This is essentially identical to TObjEnvSound, except the sound
    // fades in and out instead of abruptly starting or stopping when
    // entering or leaving its radius. Params:
    //   param2 = volume fade speed (units per frame)
    //   param3 = audibility radius (same as for TObjEnvSound)
    //   param4 = sound ID (same as for TObjEnvSound)
    //   param5 = volume (same as for TObjEnvSound)
    {0x0028, F_V3_V4, 0x00006FFCBFFF27F7, "TObjEnvSoundEx"},
    {0x0028, F_EP3, 0x0000000000000001, "TObjEnvSoundEx"},

    // Environmental sound. This object is not constructed in offline multi
    // mode. This is essentially identical to TObjEnvSound, except there is
    // no radius: the sound is audible everywhere. Params:
    //   param4 = sound ID (same as for TObjEnvSound)
    //   param5 = volume (same as for TObjEnvSound)
    {0x0029, F_V3_V4, 0x00006FFCBFFF27F7, "TObjEnvSoundGlobal"},
    {0x0029, F_EP3, 0x0000000000000001, "TObjEnvSoundGlobal"},

    // Counter sequence activator. Used for Hunter's Guild, shops, bank, etc.
    // Params:
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
    //       08 = "GIVEAWAY" (BB only)
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
    {0x0040, F_V0_V4, 0x0000600000040001, "TShopGenerator"},
    {0x0040, F_EP3, 0x0000000000000001, "TShopGenerator"},

    // Telepipe city location. Params:
    //   param4 = owner client ID (0-3)
    {0x0041, F_V0_V4, 0x0000600000040001, "TObjLuker"},
    {0x0041, F_EP3, 0x0000000000000001, "TObjLuker"},

    // BGM collision. Changes the background music when the player enters the
    // object's radius. Params:
    //   param1 = radius
    //   param4 = which music to play:
    //     00 = SHOP.adx
    //     01 = GUILD.adx
    //     02 = MEDICAL.adx
    //     03 = soutoku.adx
    //     04 = city.adx
    //     05 = labo.adx
    //     anything else = value is taken modulo 6 and used as above
    {0x0042, F_V0_V4, 0x0000600000040001, "TObjBgmCol"},
    {0x0042, F_EP3, 0x0000000000000001, "TObjBgmCol"},

    // Main warp to other floors from Pioneer 2.
    // Certain floors are available by default, determined by checking the
    // game's mode and quest flags. A different set of flags is checked on BB
    // than on other versions, presumably since government quests are used to
    // unlock areas instead of offline story progression. On later versions
    // of BB, all floors are available by default; this table reflects the
    // behavior before that change.
    // Required flag for mode: Online/multi   Offline       BB flag
    //   Episode 1:
    //     Forest 1:           Always open   Always open   Always open
    //     Cave 1:             0x17          0x18          0x1F9
    //     Mine 1:             0x20          0x21          0x201
    //     Ruins 1:            0x30          0x2A          0x207
    //   Episode 2:
    //     VR Temple Alpha:    Always open   Always open   Always open
    //     VR Spaceship Alpha: 0x4C          0x4D          0x21B
    //     CCA:                0x4F          0x50          0x225
    //     Seabed Upper:       0x52          0x53          0x22F
    //   Episode 4:
    //     Crater East                                     Always open
    //     Crater West                                     0x2BD
    //     Crater South                                    0x2BE
    //     Crater North                                    0x2BF
    //     Crater Interior                                 0x2C0
    //     Subterranean Desert 1                           0x2C1
    // Params:
    //   param5 = main warp type:
    //     00 = Episode 1 / Episode 4
    //     01 = Ep2 VR Temple / VR Spaceship (CCA and Seabed not available)
    //     02 = Ep2 CCA (VR Temple and Spaceship not available)
    {0x0043, F_V0_V4, 0x0000600000040001, "TObjCityMainWarp"},

    // Lobby teleporter. When used, this object immediately ends the current
    // game and sends the player back to the lobby. If constructed offline,
    // this object will do nothing and not render.
    // This object takes no parameters.
    {0x0044, F_V0_V4, 0x0000600000040001, "TObjCityAreaWarp"},
    {0x0044, F_EP3, 0x0000000000000001, "TObjCityAreaWarp"},

    // Warp to another location on the same map. Used for the Principal's
    // office warp. This warp is visible in all game modes, but cannot be
    // used in Battle or Challenge mode. Params:
    //   param1-3 = destination (same as for TObjMapWarpForest)
    //   param4 = destination angle (same as for TObjMapWarpForest)
    //   param6 = destination text (clamped to [0, 2]):
    //     00 = "The Principal"
    //     01 = "Pioneer 2"
    //     02 = "Lab"
    {0x0045, F_V0_V4, 0x0000600000040001, "TObjCityMapWarp"},

    // City doors. None of these take any parameters.
    {0x0046, F_V0_V4, 0x0000600000000001, "TObjCityDoor_Shop"}, // Door to shop area
    {0x0047, F_V0_V4, 0x0000600000000001, "TObjCityDoor_Guild"}, // Door to Hunter's Guild
    {0x0048, F_V0_V4, 0x0000600000000001, "TObjCityDoor_Warp"}, // Door to Ragol warp
    {0x0049, F_V0_V4, 0x0000600000000001, "TObjCityDoor_Med"}, // Door to Medical Center

    // Elevator visible in Pioneer 2. There appear to be no parameters.
    {0x004A, F_V0_V4, 0x0000600000000001, "__ELEVATOR__"},

    // Holiday event decorations. There appear to be no parameters, except
    // TObjCity_Season_SonicAdv2, which takes param4 = model index (clamped
    // to [0, 3]).
    {0x004B, F_V0_V4, 0x0000600000040001, "TObjCity_Season_EasterEgg"},
    {0x004C, F_V0_V4, 0x0000600000040001, "TObjCity_Season_ValentineHeart"},
    {0x004D, F_V0_V4, 0x0000600000040001, "TObjCity_Season_XmasTree"},
    {0x004E, F_V0_V4, 0x0000600000040001, "TObjCity_Season_XmasWreath"},
    {0x004F, F_V0_V4, 0x0000600000040001, "TObjCity_Season_HalloweenPumpkin"},
    {0x0050, F_V0_V4, 0x0000600000040001, "TObjCity_Season_21_21"},
    {0x0051, F_V0_V4, 0x0000600000040001, "TObjCity_Season_SonicAdv2"},
    {0x0052, F_V0_V4, 0x0000600000040001, "TObjCity_Season_Board"},

    // Fireworks effect. Params:
    //   param1 = area width
    //   param2 = base height
    //   param3 = area depth
    //   param4 = launch frequency (when a firework is launched, the game
    //     generates a random number r in range [0, 0x7FFF] and waits
    //     ((param4 + 60) * (r / 0x8000) * 3.0)) frames before launching the
    //     next firework)
    {0x0053, F_V0_V4, 0x0000600400040001, "TObjCity_Season_FireWorkCtrl"},

    // Door that blocks the lobby teleporter in offline mode. There appear to
    // be no parameters.
    {0x0054, F_V0_V4, 0x0000600000000001, "TObjCityDoor_Lobby"},

    // Version of the main warp for Challenge mode? This object seems to
    // behave similarly to boss teleporters; it shows the player a Yes/No
    // confirmation menu and sends 6x6A to synchronize state. There is a
    // global named last_set_mainwarp_value which is set to param4 when this
    // object is constructed, but may be changed by a set_mainwarp quest
    // opcode after that. If that happens, this object replaces its
    // dest_floor with the floor specified in the last set_mainwarp quest
    // opcode. Params:
    //   param4 = destination floor
    //   param5 = switch flag number
    // TODO: This thing has a lot of code; figure out if there are any other
    // parameters
    {0x0055, F_V2_V4, 0x0000600000040001, "TObjCityMainWarpChallenge"},

    // Episode 2 Lab door. Params:
    //   param4 = switch flag number and activation mode:
    //     negative value = always unlocked
    //     value in [0x00, 0x100] = unlocked by a switch flag (the bounds
    //       check appears to be a bug; the range should be [0x00, 0xFF] but
    //       the game has an off-by-one error)
    //     value > 0x100 = always locked
    //   param5 = model (green or red; clamped to [0, 1])
    //   param6 = if negative, all switches must be active simultaneously to
    //     unlock the door; if zero or positive, they may be activated
    //     sequentially instead (in offline solo mode, this is ignored and
    //     the sequential behavior is always used); this is somewhat obviated
    //     for this door type since it can have only one switch flag, but
    //     other door types may have multiple, for which this is relevant
    {0x0056, F_V3_V4, 0x0000400000040000, "TODoorLabo"},

    // Enables the Trade Window when the player is near this object. Both
    // players must be near a TObjTradeCollision object (not necessarily the
    // same one) to be able to use the Trade Window with each other. Params:
    //   param1 = radius
    {0x0057, F_V3_V4, 0x0000600000040001, "TObjTradeCollision"},
    {0x0057, F_EP3, 0x0000000000000001, "TObjTradeCollision"},

    // TODO: Describe this object. Presumably similar to TObjTradeCollision
    // but enables the deck edit counter? Params:
    //   param1 = radius
    {0x0058, F_EP3, 0x0000000000000001, "TObjDeckCollision"},

    // Forest door. Params:
    //   param4 = switch flag number (low byte) and number to appear on door
    //     (second-lowest byte, modulo 0x0A)
    //   param6 = TODO (expected to be 0 or 1)
    {0x0080, F_V0_V4, 0x0000400000000006, "TObjDoor"},

    // Forest switch. Params:
    //   param4 = switch flag number
    //   param6 = color (clamped to range [0, 9])
    {0x0081, F_V0_V4, 0x00004FF00078003E, "TObjDoorKey"},

    // Laser fence and square laser fence. Params:
    //   param1 = color (range [0, 3])
    //   param4 = switch flag number
    //   param6 = model (TODO)
    {0x0082, F_V0_V4, 0x00004FF0000300FE, "TObjLazerFenceNorm"},
    {0x0083, F_V0_V4, 0x00004FF03FFB00FE, "TObjLazerFence4"},

    // Forest laser fence switch. Params:
    //   param4 = switch flag number
    //   param6 = color
    {0x0084, F_V0_V4, 0x00004FFC3FFB00FE, "TLazerFenceSw"},

    // Light rays. Params:
    //   param1 = TODO
    //   param2 = vertical scale (y)
    //   param3 = horizontal scale (x, z)
    {0x0085, F_V0_V4, 0x00004E000F800006, "TKomorebi"},

    // Butterfly. Params:
    //   param1-3 = TODO
    {0x0086, F_V0_V4, 0x00004E0000000006, "TButterfly"},

    // TODO: Describe this object. Params:
    //   param1 = model number
    {0x0087, F_V0_V4, 0x0000400000000006, "TMotorcycle"},

    // Item box. Params:
    //   param1 = if positive, box is specialized to drop a specific item or
    //     type of item; if zero or negative, box drops any common item or
    //     none at all (and param3-6 are all ignored)
    //   param3 = if zero, then bonuses, grinds, etc. are applied to the item
    //     after it's generated; if nonzero, the item is not randomized at
    //     all and drops exactly as specified in param4-6
    //   param4-6 = item definition (see base_item_for_specialized_box in
    //     ItemCreator.cc for how these values are decoded)
    // In the non-specialized case (param1 <= 0), param3-6 are still sent via
    // the 6xA2 command when the box is opened on v3 and later, and the
    // server may choose to use those parameters for some purpose. The client
    // implementation ignores them when param1 <= 0, and newserv does too.
    {0x0088, F_V0_V4, 0x00004FF0B00000FE, "TObjContainerBase2"},
    {0x0088, F_EP3, 0x0000000000000002, "TObjContainerBase2"},

    // Elevated cylindrical tank. Params:
    //   param1-3 = TODO
    {0x0089, F_V0_V4, 0x0000400000000006, "TObjTank"},

    // TODO: Describe this object. Params:
    //   param1-3 = TODO
    {0x008A, F_V0_V2, 0x0000000000000006, "TObjBattery"},

    // Forest console. Params:
    //   param4 = quest label to call when activated (inherited from
    //     TObjMesBase)
    //   param5 = model (clamped to [0, 1])
    //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO")
    //     (inherited from TObjMesBase)
    {0x008B, F_V0_V1, 0x0000000000000406, "TObjComputer"},
    {0x008B, F_V2_V4, 0x00004FFC3FFB07FE, "TObjComputer"},

    // Black sliding door. Params:
    //   param1 = total distance (divided evenly by the number of switch
    //     flags, from param4)
    //   param2 = speed
    //   param4 = base switch flag (the actual switch flags used are param4,
    //     param4 + 1, param4 + 2, etc.)
    //   param5 = number of switch flags (clamped to [1, 4])
    //   param6 = TODO (only matters if this is zero or nonzero)
    {0x008C, F_V0_V1, 0x0000000000000006, "TObjContainerIdo"},
    {0x008C, F_V2_V4, 0x000040000000000E, "TObjContainerIdo"},

    // Rico message pod. This object immediately destroys itself in Challenge
    // mode or split-screen mode. Params:
    //   param4 = enable condition:
    //     negative = enabled when player is within 70 units
    //     range [0x00, 0xFF] = enabled by corresponding switch flag
    //     0x100 and above = never enabled
    //   param5 = message number (used with message quest opcode; TODO: has
    //     the same [100, 999] check as some other objects)
    //   param6 = quest label to call when activated
    {0x008D, F_V0_V4, 0x00004000000027FE, "TOCapsuleAncient01"},

    // Energy barrier. Params:
    //   param4 = switch flag number and activation mode (same as for
    //     TODoorLabo)
    {0x008E, F_V0_V4, 0x00004FF0000000F6, "TOBarrierEnergy01"},

    // Forest rising bridge. Once enabled (risen), this object cannot be
    // disabled; that is, there is no way to make the bridge retract. When
    // disabled, the bridge is 30 units below its initial position; when
    // enabled, it rises to its initial position. Params:
    //   param2 = rise speed in units per frame
    //   param4 = switch flag number
    {0x008F, F_V0_V4, 0x0000400000000006, "TObjHashi"},

    // Generic switch. Visually, this is the type usually used for objects
    // other than doors, such as lights, poison rooms, and the Forest 1
    // bridge. Params:
    //   param1 = activation mode:
    //     negative = temporary (TODO: test this)
    //     zero or positive = permanent (normal)
    //   param4 = switch flag number
    {0x0090, F_V0_V4, 0x00004FFC3FFB00C6, "TOKeyGenericSw"},

    // Box that triggers a wave event when opened. Params:
    //   param4 = event number
    {0x0091, F_V0_V4, 0x00004FF0300000FE, "TObjContainerEnemy"},

    // Large box (usually used for specialized drops). Same parameters as
    // 0x0088 (TObjContainerBase2)
    {0x0092, F_V0_V4, 0x00005E00B00078FE, "TObjContainerBase"},

    // Large enemy box. Same parameters as 0x0091 (TObjContainerEnemy).
    {0x0093, F_V0_V4, 0x00004FF0300000FE, "TObjContainerAbeEnemy"},

    // Always-empty box. There are no parameters.
    {0x0095, F_V0_V4, 0x00004FF0000000FE, "TObjContainerNoItem"},

    // Laser fence. This object is only available in v2 and later. Params:
    //   param1 = color (clamped to [0, 3])
    //   param2 = depth of collision box (transverse to lasers)
    //   param3 = length of collision box (parallel to lasers)
    //   param4 = switch flag number
    //   param6 = model:
    //     0 = short fence
    //     1 = long fence
    //     anything else = invisible
    {0x0096, F_V0_V4, 0x00004FFC3FFB07FE, "TObjLazerFenceExtra"},

    // Caves floor button. The activation radius is always 10 units. Params:
    //   param4 = switch flag number
    //   param5 = activation mode:
    //     negative = temporary (disables flag when player leaves)
    //     zero or positive = permanent
    {0x00C0, F_V0_V4, 0x00004FFC3FFB0038, "TOKeyCave01"},

    // Caves multiplayer door. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
    //     door is always unlocked)
    //   param5 = 4 - number of switch flags (so if e.g. door should require
    //     only 3 switch flags, set param5 to 1)
    //   param6 = synchronization mode:
    //     negative = when all switch flags are enabled, door is permanently
    //       unlocked via 6x0B even if some switch flags are disabled later
    //     zero or positive = door only stays unlocked while all of the
    //       switch flags are active and locks again when any are disabled
    //       (no effect in single-player offline mode; the negative behavior
    //       is used instead)
    {0x00C1, F_V0_V4, 0x0000400000000038, "TODoorCave01"},

    // Caves standard door. Params:
    //   param4 = switch flag number (negative = always unlocked; >0x100 =
    //     always locked)
    {0x00C2, F_V0_V4, 0x0000400000000038, "TODoorCave02"},

    // Caves ceiling piston trap. There are three types of this object, which
    // can be chosen via param6. If param6 is not 0, 1, or 2, no object is
    // created.
    // Params for TOHangceilingCave01Normal (param6 = 0):
    //   param1 = TODO (radius delta? value is param1 + 29)
    //   param2 = TODO (value is 1 - param2)
    //   param3 = TODO (value is param3 + 100)
    // Params for TOHangceilingCave01Key (param6 = 1):
    //   param1-3 = same as for TOHangceilingCave01Normal
    //   param4 = switch flag number (drops when switch flag is activated;
    //     when it has finished dropping, it disables the switch flag)
    // Params for TOHangceilingCave01KeyQuick (param6 = 2):
    //   param1-4 = same as for TOHangceilingCave01Key, but unlike that
    //     object, does not disable the switch flag automatically
    {0x00C3, F_V0_V4, 0x0000400800780038, "TOHangceilingCave01*"},

    // Caves signs. There appear to be no parameters.
    {0x00C4, F_V0_V4, 0x0000400000000030, "TOSignCave01"},
    {0x00C5, F_V0_V4, 0x0000400000000030, "TOSignCave02"},
    {0x00C6, F_V0_V4, 0x0000400000000030, "TOSignCave03"},

    // Hexagonal tank. There appear to be no parameters.
    {0x00C7, F_V0_V4, 0x0000400000000030, "TOAirconCave01"},

    // Brown platform. There appear to be no parameters.
    {0x00C8, F_V0_V4, 0x0000400000000030, "TOAirconCave02"},

    // Revolving warning light. Params:
    //   param1 = rotation speed in degrees per frame
    {0x00C9, F_V0_V4, 0x000041F000000030, "TORevlightCave01"},

    // Caves rainbow. Params:
    //   param1-3 = scale factors (x, y, z)
    //   param4 = TODO (value is 1 / (param4 + 30))
    //   param6 = visibility radius? (TODO; value is param6 + 40000)
    {0x00CB, F_V0_V4, 0x0000400000000010, "TORainbowCave01"},

    // Floating jellyfish. Params:
    //   param1 = visibility radius; visible when any player is within this
    //     distance of the object
    //   param2 = move radius (according to debug strings)
    //   param3 = rebirth radius (according to debug strings); like param1,
    //     checks against all players, not only the local player
    {0x00CC, F_V0_V4, 0x0000400030000010, "TOKurage"},

    // Floating dragonfly. Params:
    //   param1 = TODO
    //   param2 = TODO
    //   param3 = max distance from home?
    //   param4 = TODO
    //   param5 = TODO
    {0x00CD, F_V0_V4, 0x00004E0000610010, "TODragonflyCave01"},

    // Caves door. Params:
    //   param4 = switch flag number
    {0x00CE, F_V0_V4, 0x0000400000000038, "TODoorCave03"},

    // Robot recharge station. Params:
    //   param4 = quest register number; activates when this register
    //     contains a nonzero value; does not deactivate if it becomes zero
    //     again
    {0x00CF, F_V0_V4, 0x00004008000000F8, "TOBind"},

    // Caves cake shop. There appear to be no parameters.
    {0x00D0, F_V0_V4, 0x0000400000000020, "TOCakeshopCave01"},

    // Various solid rock objects used in the Cave areas. There are small,
    // medium, and large variations of each, and for the 02 variations, there
    // are also "Simple" variations (00D7-00D9). None of these objects take
    // any parameters.
    {0x00D1, F_V0_V4, 0x0000400000000008, "TORockCaveS01"},
    {0x00D2, F_V0_V4, 0x0000400000000008, "TORockCaveM01"},
    {0x00D3, F_V0_V4, 0x00004FF000000008, "TORockCaveL01"},
    {0x00D4, F_V0_V4, 0x0000000000000010, "TORockCaveS02"},
    {0x00D5, F_V0_V4, 0x0000000000000010, "TORockCaveM02"},
    {0x00D6, F_V0_V4, 0x0000000000000010, "TORockCaveL02"},
    {0x00D7, F_V0_V4, 0x0000000000000010, "TORockCaveSS02"},
    {0x00D8, F_V0_V4, 0x0000000000000010, "TORockCaveSM02"},
    {0x00D9, F_V0_V4, 0x0000000000000010, "TORockCaveSL02"},
    {0x00DA, F_V0_V4, 0x0000000000000020, "TORockCaveS03"},
    {0x00DB, F_V0_V4, 0x0000000000000020, "TORockCaveM03"},
    {0x00DC, F_V0_V4, 0x0000000000000020, "TORockCaveL03"},

    // Caves floor button 2. Params:
    //   param1-3 = scale factors (visual only)
    //   param4 = switch flag number
    //   param5 high word = sound delay in frames after activate/deactivate
    //   param5 low word = model index for VR Temple / VR Spaceship? (there
    //     are only two: this word may be zero or nonzero)
    //   param6 high word = sound activation mode:
    //     1 = play sound only when switch activated
    //     anything else = play sound when activated and when deactivated
    //   param6 low word = sound to play (1-6) (TODO: Describe these):
    //     1 => 4005281F (GC)
    //     2 => 00053F06 (GC)
    //     3 => 00052812 (GC)
    //     4 => 00052813 (GC)
    //     5 => 4001C112 (GC)
    //     6 => 4002A60B (GC)
    //     TODO: Are there more sounds available here on BB?
    {0x00DE, F_V2_V4, 0x00004FFC3FFB07FE, "TODummyKeyCave01"},

    // Breakable rocks, in small, medium, and large variations. All of these
    // take the following parameter:
    //   param4 = switch flag number
    {0x00DF, F_V2_V4, 0x0000400000000008, "TORockCaveBL01"},
    {0x00E0, F_V2_V4, 0x0000400000000010, "TORockCaveBL02"},
    {0x00E1, F_V2_V4, 0x0000400000000020, "TORockCaveBL03"},

    // Mines multi-switch door. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
    //     door is always unlocked)
    //   param5 = 4 - number of switch flags (so if e.g. door should require
    //     only 3 switch flags, set param5 to 1)
    {0x0100, F_V0_V4, 0x00004000000000C0, "TODoorMachine01"},

    // Mines floor button. The activation radius is always 10 units. Params:
    //   param4 = switch flag number
    //   param5 = activation mode:
    //     negative = temporary (disables flag when player leaves)
    //     zero or positive = permanent
    {0x0101, F_V0_V1, 0x00000000000000C0, "TOKeyMachine01"},
    {0x0101, F_V2_V4, 0x00004FF0007B00C6, "TOKeyMachine01"},

    // Mines single-switch door, or Ep4 test door if in Episode 4. Params (for
    // both object types):
    //   param4 = switch flag number
    {0x0102, F_V0_V4, 0x00000000000000C0, "TODoorMachine02"},
    {0x0102, F_V4, 0x00004E0000000000, "__EP4_TEST_DOOR__"},

    // Large cryotube. There appear to be no parameters.
    {0x0103, F_V0_V4, 0x00004008000000C0, "TOCapsuleMachine01"},

    // Computer. Same parameters as 0x008D (TOCapsuleAncient01).
    {0x0104, F_V0_V4, 0x00004008000000C0, "TOComputerMachine01"},

    // Green monitor. Params:
    //   param4 = initial state? (clamped to [0, 3]; appears to cycle through
    //     those 4 values on its own)
    {0x0105, F_V0_V4, 0x00004008000000C0, "TOMonitorMachine01"},

    // Floating robot. Same params as 0x00CD (TODragonflyCave01), though it
    // appears that some may have different scale factors or offsets (TODO).
    {0x0106, F_V0_V4, 0x00004000000000C0, "TODragonflyMachine01"},

    // Floating blue light. Params:
    //   param4 = TODO
    //   param5 = TODO
    //   param6 = TODO
    {0x0107, F_V0_V4, 0x00004000000000C0, "TOLightMachine01"},

    // Self-destructing objects. Params:
    //   param1 = radius delta (actual radius is param1 + 30)
    {0x0108, F_V0_V4, 0x00004000000000C0, "TOExplosiveMachine01"},
    {0x0109, F_V0_V4, 0x00004000000000C0, "TOExplosiveMachine02"},
    {0x010A, F_V0_V4, 0x00004000000000C0, "TOExplosiveMachine03"},

    // Spark machine. This looks like it's intended to appear in the bridge
    // rooms in the Mines, to create an effect of the columnar machines
    // sparking. This is implemented as four columns that randomly change their
    // visibility. Each column has an accumulator value, which is initially
    // zero. Every frame, the value (param1 - 0.98) is added to each column's
    // accumulator and a random number between 0 and 1 is chosen; if the random
    // number is less than the column's accumulator, its visibility state is
    // changed and its accumulator is reset to zero. In this manner, param1 can
    // be thought of as the frequency of state changes - 0.98 would mean they
    // never change state, 1.98 would mean they change every frame. Params:
    //   param1 = state change accumulation per frame (value is param1 - 0.98)
    //   param2 = if <= 0, only one column flickers and the other are always
    //     visible; if > 0, all columns flicker
    {0x010B, F_V0_V4, 0x00004000000000C0, "TOSparkMachine01"},

    // Large flashing box. Params:
    //   param2 = TODO (seems it only matters if this is < 0 or not)
    {0x010C, F_V0_V4, 0x00004000000000C0, "TOHangerMachine01"},

    // Ruins entrance door (after Vol Opt). This object reads quest flags
    // 0x2C, 0x2D, and 0x2E to determine the state of each seal on the door
    // (for Forest, Caves, and Mines respectively). It then checks quest flag
    // 0x2F; if this flag is set, then all seals are unlocked regardless of
    // the preceding three flags' values. All of these flags are checked every
    // frame, not only at construction time.
    // No parameters.
    {0x0130, F_V0_V4, 0x0000400000002000, "TODoorVoShip"},

    // Ruins floor warp. Params:
    //   param4 = destination floor
    //   param6 = color (negative = red, zero or positive = blue); if this is
    //     >= 0 in Challenge mode, the warp is destroyed immediately
    {0x0140, F_V0_V4, 0x0000400000000700, "TObjGoalWarpAncient"},

    // Ruins intra-area warp. Params:
    //   param1-3 = destination (same as for TObjMapWarpForest)
    //   param4 = destination angle (same as for TObjMapWarpForest)
    //   param5 = if negative, no warp lines render (only the floor pad
    //     appears) and the player cannot use the warp; if zero or positive,
    //     the warp functions normally
    {0x0141, F_V0_V4, 0x0000400000000700, "TObjMapWarpAncient"},

    // Ruins switch. Same parameters as 0x00C0 (TOKeyCave01).
    {0x0142, F_V0_V4, 0x0000400000000700, "TOKeyAncient02"},

    // Ruins floor button. Same parameters as 0x00C0 (TOKeyCave01).
    {0x0143, F_V0_V4, 0x0000400000000700, "TOKeyAncient03"},

    // Ruins doors. These all take the same params as 0x00C2 (TODoorCave02).
    {0x0144, F_V0_V4, 0x0000400000000100, "TODoorAncient01"}, // Usually used in Ruins 1
    {0x0145, F_V0_V4, 0x0000400000000400, "TODoorAncient03"}, // Usually used in Ruins 3
    {0x0146, F_V0_V4, 0x0000400000000200, "TODoorAncient04"}, // Usually used in Ruins 2
    {0x0147, F_V0_V4, 0x0000400000000100, "TODoorAncient05"}, // Usually used in Ruins 1
    {0x0148, F_V0_V4, 0x0000400000000200, "TODoorAncient06"}, // Usually used in Ruins 2
    {0x0149, F_V0_V4, 0x0000400000000400, "TODoorAncient07"}, // Usually used in Ruins 3

    // Ruins 4-player door. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, and param4 + 3); param4 is clamped
    //     to [0, 0xFC]
    //   param6 = activation mode; same as for 0x00C1 (TODoorCave01)
    {0x014A, F_V0_V4, 0x0000400000000700, "TODoorAncient08"},

    // Ruins 2-player door. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4 and param4 + 1); param4 is clamped to [0, 0xFE]
    //   param6 = activation mode; same as for 0x00C1 (TODoorCave01)
    {0x014B, F_V0_V4, 0x0000400000000700, "TODoorAncient09"},

    // Ruins sensor. Params:
    //   param1 = activation radius delta (actual radius is param1 + 50)
    //   param4 = switch flag number
    //   param5 = if negative, sensor is always on
    //   param6 = texture index; uses fs_obj_o_sensor01r if <= 0, uses
    //     fs_obj_o_sensor02r if positive; the two texture files are identical
    //     (at least on GC) so this has no user-visible effects
    {0x014C, F_V0_V4, 0x0000400000000700, "TOSensorAncient01"},

    // Ruins laser fence switch. Params:
    //   param1 = if negative, switch's effect is temporary; if zero or
    //     positive, it's permanent
    //   param4 = switch flag number
    //   param5 = color (clamped to [0, 3])
    {0x014D, F_V0_V4, 0x0000400000000700, "TOKeyAncient01"},

    // Ruins fence objects. Params:
    //   param4 = switch flag number (negative = always unlocked)
    //   param5 = color (clamped to [0, 3])
    {0x014E, F_V0_V4, 0x00004FF000000700, "TOFenceAncient01"}, // 4x2
    {0x014F, F_V0_V4, 0x00004FF000000700, "TOFenceAncient02"}, // 6x2
    {0x0150, F_V0_V4, 0x0000400000000700, "TOFenceAncient03"}, // 4x4
    {0x0151, F_V0_V4, 0x0000400000000700, "TOFenceAncient04"}, // 6x4

    // Ruins poison-spewing blob. This object is technically an item box, and
    // drops an item when destroyed. Unlike most other item boxes, it cannot
    // be specialized (ignore_def is always true). Params:
    //   param1 = TODO (value is param1 + 299)
    //   param2 = TODO (value is param2 + 209)
    //   param3 = TODO (value is param3 + 399)
    //   param6 = TODO (value is param6 + 4)
    {0x0152, F_V0_V4, 0x00004E000F800700, "TContainerAncient01"},

    // Ruins falling trap. Trap power seems to be scaled by difficulty
    // (Normal = x1, Hard = x2, Very Hard = x3, Ultimate = x6). Params:
    //   param1 = trigger radius delta (value is (param1 / 2) + 30)
    //   param2 = TODO (clamped below to 0)
    //   param3 = TODO (clamped below to 1)
    //   param4 = TODO (seems it only matters if this is negative or not in
    //     the base class (TOTrap), but TOTrapAncient01 clamps it below to 0)
    //   param5 = TODO (clamped to [0, 5]; calls player->vtable[0x19] on
    //     explode unless this is 5)
    //   param6 = TODO (value is param6 + 30, multipled by 0.33 if offline)
    {0x0153, F_V0_V4, 0x0000400000780700, "TOTrapAncient01"},

    // Ruins pop-up trap. Params:
    //   param1 = trigger radius delta (value is (param1 / 2) + 30)
    //   param4 = delay (value is param4 + 30; clamped below to 0)
    //   angle.z = TODO (seems it only matters if angle.z is > 0 or not)
    {0x0154, F_V0_V4, 0x0000400000000700, "TOTrapAncient02"},

    // Ruins crystal monument. Same parameters as TOCapsuleAncient01.
    {0x0155, F_V0_V4, 0x0000400000000700, "TOMonumentAncient01"},

    // Non-Ruins monument. Sets a quest flag when activated. The quest flag
    // depends on which area the object appears in:
    //   Mine 2 = 0x2E
    //   Cave 2 = 0x2D
    //   Any other area = 0x2C
    // When all three of the above quest flags are active, this object also
    // sets quest flag 0x2F.
    // There appear to be no parameters.
    {0x0156, F_V0_V4, 0x0000400000000094, "TOMonumentAncient02"},

    // Ruins rocks. None of these take any parameters.
    {0x0159, F_V0_V4, 0x0000400000000700, "TOWreckAncient01"},
    {0x015A, F_V0_V4, 0x0000400000000700, "TOWreckAncient02"},
    {0x015B, F_V0_V4, 0x0000400000000700, "TOWreckAncient03"},
    {0x015C, F_V0_V4, 0x0000400000000700, "TOWreckAncient04"},
    {0x015D, F_V0_V4, 0x0000400000000700, "TOWreckAncient05"},
    {0x015E, F_V0_V4, 0x0000400000000700, "TOWreckAncient06"},
    {0x015F, F_V0_V4, 0x0000400000000700, "TOWreckAncient07"},

    // 0x0160 constructs different objects depending on where it's used. On
    // floor 0D (Vol Opt), it constructs TObjWarpBoss03; on other floors
    // where it's valid, it constructs TObjFogCollisionPoison.
    // TObjWarpBoss03 creates an invisible warp. This is used for the warp
    // behind the door to Ruins after defeating Vol Opt. Params:
    //   param4 = destination floor
    {0x0160, F_V0_V4, 0x0000400000002000, "TObjWarpBoss03"},

    // TObjFogCollisionPoison creates a switchable, foggy area that's visible
    // and hurts the player if the switch flag isn't on. Params are the same
    // as for 0x0018 (TObjFogCollisionSwitch), but there is also:
    //   param2 = poison power (scaled by difficulty: Normal = x1, Hard = x2,
    //     Very Hard = x3, Ultimate = x6)
    {0x0160, F_V0_V4, 0x00004FF030600700, "TObjFogCollisionPoison"},

    // Ruins specialized box. Same parameters as 0x0088 (TObjContainerBase2).
    {0x0161, F_V0_V4, 0x00004003007B0700, "TOContainerAncientItemCommon"},

    // Ruins random box. Same parameters as 0x0088 (TObjContainerBase2).
    {0x0162, F_V0_V4, 0x00004003007B0700, "TOContainerAncientItemRare"},

    // Ruins enemy boxes, disguised as either of the above two objects.
    // Params:
    //   param4 = event number
    {0x0163, F_V0_V4, 0x00004000007B0700, "TOContainerAncientEnemyCommon"},
    {0x0164, F_V0_V4, 0x00004000007B0700, "TOContainerAncientEnemyRare"},

    // Ruins always-empty box. There are no parameters.
    {0x0165, F_V2_V4, 0x00004000007B0700, "TOContainerAncientItemNone"},

    // Ruins breakable rock. Params:
    //   param4 = switch flag number (when enabled, destroys this object)
    {0x0166, F_V2_V4, 0x0000400000000700, "TOWreckAncientBrakable05"},

    // Ruins pop-up trap with techs. Params:
    //   param1 = trigger radius delta (value is (param1 / 2) + 30)
    //   param2 = number of hits to destroy trap (clamped to [1, 256])
    //   param3 = tech level modifier:
    //     Normal: level = param3 + 1 (clamped to [0, 14])
    //     Hard: level = (param3 * 2) + 1 (clamped to [0, 14])
    //     Very Hard: level = (param3 * 4) + 1 (clamped to [0, 14])
    //     Ultimate: level = (param3 * 6) + 1 (clamped to [0, 29])
    //     (Note: level is offset by 1, so a value of 0 means tech level 1)
    //   param4 = delay (value is param4 + 30; clamped below to 0)
    //   param5 = switch flag number (negative = always active)
    //   param6 = tech number:
    //     0 = Foie
    //     1 = Gizonde
    //     2 = Gibarta
    //     3 = Megid
    //     anything else = Gifoie
    //   angle.z = TODO (seems it only matters if angle.z is > 0 or not)
    {0x0167, F_V2_V4, 0x0000400C3FF807C0, "TOTrapAncient02R"},

    // Flying white bird. Params:
    //   param1 = TODO (value is param1 + 1)
    //   param2 = TODO (value is param2 + 50)
    //   param3 = TODO (value is param3 + 100)
    //   param4 = number of birds? (value is param4 + 3, clamped to [1, 6])
    //   param5 = TODO (value is (param5 / 10) + 1)
    //   param6 = TODO (value is (param6 / 10) + 1)
    {0x0170, F_V0_V4, 0x0000400000614000, "TOBoss4Bird"},

    // Dark Falz obelisk. There appear to be no parameters.
    {0x0171, F_V0_V4, 0x0000400000004000, "TOBoss4Tower"},

    // Floating rocks. Params:
    //   param1 = x/z range delta? (value is param1 + 50)
    //   param2 = TODO (value is abs(param2))
    //   param4 = number of rocks? (clamped to [1, 8])
    {0x0172, F_V0_V4, 0x0000400000004000, "TOBoss4Rock"},

    // Floating soul. Params:
    //   param1 = TODO
    //   param2 = TODO
    //   param3 = TODO
    //   param4 = TODO (seems it only matters if this is negative or not)
    //   param5 = TODO
    //   param6 = TODO
    {0x0173, F_V0_V2, 0x0000000000004000, "TOSoulDF"},

    // Butterfly. This is a subclass of TODragonfly and takes the same params
    // as 0x00CD (TODragonflyCave01), but also:
    //   param6 = model number? (clamped to [0, 2])
    {0x0174, F_V0_V2, 0x0000000000004000, "TOButterflyDF"},

    // Lobby information counter (game menu) collision. Params:
    //   param1 = radius
    {0x0180, F_V0_V4, 0x0000400000008000, "TObjInfoCol"},
    {0x0180, F_EP3, 0x0000000000008000, "TObjInfoCol"},

    // Warp between lobbies (not warp out of game to lobby). Params:
    //   param5 = hide beams (beams shown if <= 0, hidden if > 0)
    {0x0181, F_V0_V4, 0x0000400000008000, "TObjWarpLobby"},
    {0x0181, F_EP3, 0x0000000000008000, "TObjWarpLobby"},

    // Lobby 1 event object (tree). Params:
    //   param4 = default decorations when there is no event:
    //     0x01 = flowers (Lobby 2)
    //     0x02 = fountains (Lobby 4)
    //     0x03 = very tall aquariums (Lobby 5)
    //     0x04 = green trees (Lobby 1)
    //     0x05 = stained glass windows (Lobby 9)
    //     0x06 = snowy evergreen trees (Lobby 10)
    //     0x07 = windmills (Lobby 3)
    //     0x08 = spectral columns (Lobby 8)
    //     0x09 = planetary models (Lobby 7)
    //     Anything else = grass (Lobby 6)
    {0x0182, F_V3_V4, 0x0000400000008000, "TObjLobbyMain"},
    {0x0182, F_EP3, 0x0000000000008000, "TObjLobbyMain"},

    // Lobby pigeon. Params:
    //   param4 = model number? (clamped to [0, 2])
    //   param5 = TODO (3 different behaviors: <= 0, == 1, or >= 2)
    //   param6 = TODO (it only matters if this is > 0 or not)
    // Visibility depends on the current season event in a complicated way:
    //   If param5 <= 0, visible only when there's no season event
    //   If param5 == 1 and param6 <= 0, visible during Wedding event
    //   If param5 == 2 and param6 <= 0, visible during Valentine's Day and
    //     White Day events
    //   If none of the above, visible during Halloween event
    {0x0183, F_V3_V4, 0x0000400000008000, "__LOBBY_PIGEON__"},
    {0x0183, F_EP3, 0x0000000000008002, "__LOBBY_PIGEON__"},

    // Lobby butterfly. Params:
    //   param4 = model number (only two models; <= 0 or > 0)
    // TODO: Is this object's visibility affected by season events? It may
    // only be affected by the event when the object loads, and not when the
    // season is changed via the DA command.
    {0x0184, F_V3_V4, 0x0000400000008000, "TObjButterflyLobby"},
    {0x0184, F_EP3, 0x0000000000008002, "TObjButterflyLobby"},

    // Lobby rainbow. Visible only when there is no season event. There are
    // no parameters.
    {0x0185, F_V3_V4, 0x0000400000008000, "TObjRainbowLobby"},
    {0x0185, F_EP3, 0x0000000000008002, "TObjRainbowLobby"},

    // Lobby pumpkin. Visible only during Halloween season event. There are
    // no parameters.
    {0x0186, F_V3_V4, 0x0000400000008000, "TObjKabochaLobby"},
    {0x0186, F_EP3, 0x0000000000008000, "TObjKabochaLobby"},

    // Lobby stained-glass windows. Params:
    //   param4 = event flag:
    //     zero or negative = visible only when no season event is active
    //     positive = visible only during Christmas season event
    {0x0187, F_V3_V4, 0x0000400000008000, "TObjStendGlassLobby"},
    {0x0187, F_EP3, 0x0000000000008000, "TObjStendGlassLobby"},

    // Lobby red and white striped curtain. Visible only during the spring
    // and summer season events (12 and 13). No parameters.
    {0x0188, F_V3_V4, 0x0000400000008000, "TObjCurtainLobby"},
    {0x0188, F_EP3, 0x0000000000008000, "TObjCurtainLobby"},

    // Lobby wedding arch. Visible only during the wedding season event. No
    // parameters.
    {0x0189, F_V3_V4, 0x0000400000008000, "TObjWeddingLobby"},
    {0x0189, F_EP3, 0x0000000000008000, "TObjWeddingLobby"},

    // Lobby snowy evergreen tree (Lobby 10). No parameters.
    {0x018A, F_V3_V4, 0x0000400000008000, "TObjTreeLobby"},
    {0x018A, F_EP3, 0x0000000000008000, "TObjTreeLobby"},

    // Lobby aquarium (Lobby 5). Visible only when there is no season event.
    // No parameters.
    {0x018B, F_V3_V4, 0x0000400000008000, "TObjSuisouLobby"},
    {0x018B, F_EP3, 0x0000000000008000, "TObjSuisouLobby"},

    // Lobby particles. Params:
    //   param1 = TODO (clamped to [0, 575])
    //   param3 = same as param3 from 0001 (TObjParticle)
    //   param4 = particle type (0-9; any other value treated as 0)
    //   param5 = same as param4 (not param5!) from 0001 (TObjParticle)
    //   param6 = same as param5 (not param6!) from 0001 (TObjParticle)
    {0x018C, F_V3_V4, 0x0000400000008000, "TObjParticleLobby"},
    {0x018C, F_EP3, 0x0000000000008000, "TObjParticleLobby"},

    // Episode 3 lobby battle table. This object is responsible for the red
    // panels on the floor next to the battle table that turn green when you
    // step on them; it also shows the confirmation window and sends the
    // necessary commands to the server. The actual table model and the non-lit
    // parts of the floor panels are part of the lobby geometry, not this
    // object. Params:
    //   param4 = player count
    //     1 = 1 player (doesn't work properly - there's no way to confirm)
    //     2 = 2 players
    //     3 = 3 players (unused, but works)
    //     4 = 4 players
    //     anything else = object doesn't load
    //   param5 = table number (used in E4 and E5 commands)
    {0x018D, F_EP3, 0x0000000000008000, "TObjLobbyTable"},

    // Episode 3 lobby jukebox. No parameters.
    {0x018E, F_EP3, 0x0000000000008000, "TObjJukeBox"},

    // TODO: Describe this object. There appear to be no parameters.
    {0x0190, F_V2_V4, 0x0000400000610000, "TObjCamera"},

    // Short Spaceship wall. There appear to be no parameters.
    {0x0191, F_V2_V4, 0x0000400800610000, "TObjTuitate"},

    // Spaceship door. Params:
    //   param4 = switch flag number (if this is negative, the door is always
    //     unlocked)
    {0x0192, F_V2_V4, 0x0000400000610000, "TObjDoaEx01"},

    // Tall Spaceship wall. There appear to be no parameters.
    {0x0193, F_V2_V4, 0x0000400800610000, "TObjBigTuitate"},

    // Temple door. Params:
    //   param4 = switch flag number (if this is negative, the door is always
    //     unlocked)
    {0x01A0, F_V2_V4, 0x00004000001A0000, "TODoorVS2Door01"},

    // Temple rubble. None of these take any parameters.
    {0x01A1, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck01"}, // Partly-broken wall (like breakable wall)
    {0x01A2, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck02"}, // Broken column
    {0x01A3, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck03"}, // Broken wall pieces lying flat
    {0x01A4, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck04"}, // Column
    {0x01A5, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck05"}, // Broken toppled column
    {0x01A6, F_V2_V4, 0x00004000001A0000, "TOVS2Wreck06"}, // Truncated conic monument

    // Temple breakable wall, which looks like 0x01A1 (TOVS2Wreck01). Params:
    //   param4 = number of hits, minus 256 for some reason (for example, for
    //     a 6-hit wall, this should be -250, or 0xFFFFFF06)
    {0x01A7, F_V2_V4, 0x00004000001A0000, "TOVS2Wall01"},

    // Lens flare enable/disable switch. This object triggers when the local
    // player is within 20 units, and sets a global which determines whether
    // objects of type 0x001E (__LENS_FLARE__) should render anything.
    // Params:
    //   param1 = if > 0, enable lens flare rendering; if <= 0, disable it
    // This object isn't constructed in split-screen mode.
    {0x01A8, F_V2_V4, 0x000041F1001A0000, "__LENS_FLARE_SWITCH_COLLISION__"},

    // Rising bridges. Similar to 0x008F (TObjHashi). Params:
    //   param1 = extra depth when lowered (this is added to TObjHashiBase's
    //     30 unit displacement if the bridge is lowered when constructed)
    //   param2 = rise speed in units per frame
    //   param4 = switch flag number
    {0x01A9, F_V2_V4, 0x00004000001A0000, "TObjHashiVersus1"}, // Small brown rising bridge
    {0x01AA, F_V2_V4, 0x00004000001A0000, "TObjHashiVersus2"}, // Long rising bridge

    // Multiplayer Temple/Spaceship doors. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
    //     door is always unlocked)
    //   param5 = number of switch flags
    //   param6 = synchronization mode:
    //     negative = when all switch flags are enabled, door is permanently
    //       unlocked via 6x0B even if some switch flags are disabled later
    //     zero or positive = door only stays unlocked while all of the
    //       switch flags are active and locks again when any are disabled
    //       (no effect in single-player offline mode; the negative behavior
    //       is used instead)
    {0x01AB, F_V3_V4, 0x0000400000180000, "TODoorFourLightRuins"}, // Temple
    {0x01C0, F_V3_V4, 0x0000000000600000, "TODoorFourLightSpace"}, // Spaceship

    // CCA item box. It seems this box type cannot be specialized. There are
    // no parameters.
    {0x0200, F_V3_V4, 0x000041FC4F800000, "TObjContainerJung"},

    // CCA cross-floor warp. Params:
    //   param4 = destination floor
    //   param6 = color (0 = blue, 1 = red); if this is 0 in Challenge mode,
    //     the warp is destroyed immediately
    {0x0201, F_V3_V4, 0x0000400CFF800000, "TObjWarpJung"},

    // CCA door. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
    //     door is always unlocked)
    //   param5 = number of switch flags
    {0x0202, F_V3_V4, 0x0000400C0F800000, "TObjDoorJung"},

    // CCA item box. Same parameters as 0x0088 (TObjContainerBase2).
    // In the Episode 4 Crater areas, this object constructs 0x0092
    // (TObjContainerBase) instead.
    {0x0203, F_V3_V4, 0x0000400C4F800000, "TObjContainerJungEx"},
    {0x0203, F_V4, 0x000001F000000000, "TObjContainerBase(0203)"},

    // CCA main door. This door checks quest flags 0x0046, 0x0047, and 0x0048
    // and opens when all are enabled. There are no parameters.
    {0x0204, F_V3_V4, 0x0000400000800000, "TODoorJungleMain"},

    // CCA main door switch. This switch sets one of the quest flags checked
    // by 0x0204 (TODoorJungleMain). Params:
    //   param4 = quest flag index (0 = 0x0046, 1 = 0x0047, 2 = 0x0048)
    {0x0205, F_V3_V4, 0x0000400C0F800000, "TOKeyJungleMain"},

    // Jungle breakable rocks. Params:
    //   param4 = switch flag number (object is passable when enabled)
    {0x0206, F_V3_V4, 0x000040040F800000, "TORockJungleS01"}, // Small rock
    {0x0207, F_V3_V4, 0x000040040F800000, "TORockJungleM01"}, // Small 3-rock wall

    // Jungle large 3-rock wall. Unlike the above, this takes no parameters
    // and cannot be opened.
    {0x0208, F_V3_V4, 0x000040040F800000, "TORockJungleL01"},

    // Jungle plant. Params:
    //   param4 = model number? (clamped to [0, 1])
    {0x0209, F_V3_V4, 0x000040040F800000, "TOGrassJungle"},

    // CCA warp outside main gate. Unlike other warps on Ragol, this one
    // presents the player with a choice of areas: Jungle North (6), Mountain
    // (8), or Seaside (9). Params:
    //   param6 = color (0 = blue, 1 = red); if this is 0 in Challenge mode,
    //     the warp is destroyed immediately
    {0x020A, F_V3_V4, 0x0000400C0F800000, "TObjWarpJungMain"},

    // Background lightning generator. Each strike lasts for 11 frames and
    // strikes at a random angle around the player. Params:
    //   param1 = lightning distance from player
    //   param2 = lightning height
    //   param3 = TODO (value is ((param3 / 32768) * 0.3) + 1)
    //   param4 = minimum frames between strikes
    //   param5 = interval randomness (after each strike, a random number is
    //     chosen between param4 and (param4 + param5) to determine how many
    //     frames to wait until the next strike)
    {0x020B, F_V3_V4, 0x0000400040800000, "TBGLightningCtrl"},

    // Bird objects. Params:
    //   param4 = model number? (clamped to [0, 2])
    {0x020C, F_V3_V4, 0x00004E0C0B000000, "__WHITE_BIRD__"},
    {0x020D, F_V3_V4, 0x000040080B000000, "__ORANGE_BIRD__"},

    // Jungle box that triggers a wave event when opened. Params:
    //   param4 = event number
    //   param5 = model number (clamped to [0, 1])
    {0x020E, F_V3_V4, 0x0000400C0F800000, "TObjContainerJungEnemy"},

    // Chain saw damage trap. Params:
    //   param2 = base damage (multiplied by difficulty: Normal = x1/5, Hard
    //     = x2/5, Very Hard = x3/5, Ultimate = x6/5)
    //   param3 = model number (<= 0 for small saw, > 0 for large saw)
    //   param4 = switch flag number (disabled when switch flag is enabled)
    //   param5 high word = rotation range (16-bit angle)
    //   param5 low word = rotation speed (angle units per frame)
    //   param6 high word = if nonzero, ignore rotation range and rotate in a
    //     full circle instead
    //   param6 low word = delay between cycles (seconds)
    {0x020F, F_V3_V4, 0x0000400C3F800000, "TOTrapChainSawDamage"},

    // Laser detector trap. Params:
    //   param3 = model number (<= 0 for small laser, > 0 for large laser)
    //   param4 = switch flag number (enables this flag when triggered)
    //   param5-6 = same as 0x020F (TOTrapChainSawDamage)
    {0x0210, F_V3_V4, 0x0000400C3F800000, "TOTrapChainSawKey"},

    // TODO: Describe this object. It's a subclass of TODragonfly and has the
    // same params as 0x00CD (TODragonflyCave01), though it appears that some
    // may have different scale factors or offsets.
    {0x0211, F_V3_V4, 0x00004E0003800000, "TOBiwaMushi"},
    {0x0211, F_EP3, 0x0000000000000002, "TOBiwaMushi"},

    // Seagull. Params:
    //   param4 = model number? (clamped to [0, 2])
    {0x0212, F_V3_V4, 0x000040080F800000, "__SEAGULL__"},
    {0x0212, F_EP3, 0x0000000000000002, "__SEAGULL__"},

    // TODO: Describe this object. Params:
    //   param4 = model number (clamped to [0, 2])
    {0x0213, F_V3_V4, 0x00004E040F000000, "TOJungleDesign"},

    // Fish. This object is not constructed in split-screen mode. Params:
    //   param1-3 = TODO (Vector3F)
    //   param4 = TODO
    //   param5 = TODO
    //   param6 = TODO
    {0x0220, F_V3_V4, 0x0000400439008000, "TObjFish"},
    {0x0220, F_EP3, 0x0000000000008002, "TObjFish"},

    // Seabed multiplayer doors. Params:
    //   param4 = base switch flag number (the actual switch flags used are
    //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
    //     door is always unlocked)
    //   param5 = number of switch flags (clamped to [0, 4])
    //   param6 = synchronization mode:
    //     negative = when all switch flags are enabled, door is permanently
    //       unlocked via 6x0B even if some switch flags are disabled later
    //     zero or positive = door only stays unlocked while all of the
    //       switch flags are active and locks again when any are disabled
    //       (no effect in single-player offline mode; the negative behavior
    //       is used instead)
    {0x0221, F_V3_V4, 0x0000400030000000, "TODoorFourLightSeabed"}, // Blue edges
    {0x0222, F_V3_V4, 0x0000400030000000, "TODoorFourLightSeabedU"},

    // Small cryotube. Params:
    //   param4 = model number (clamped to [0, 3])
    {0x0223, F_V3_V4, 0x0000400830000000, "TObjSeabedSuiso_CH"},

    // Breakable glass wall. Params:
    //   param4 = switch flag number
    //   param5 = model number (clamped to [0, 2])
    {0x0224, F_V3_V4, 0x0000400030000000, "TObjSeabedSuisoBrakable"},

    // Small floating robots. These are subclasses of TODragonfly and have
    // the same params as 0x00CD (TODragonflyCave01), though it appears that
    // some may have different scale factors or offsets (TODO).
    {0x0225, F_V3_V4, 0x0000400030000000, "TOMekaFish00"}, // Blue
    {0x0226, F_V3_V4, 0x0000400030000000, "TOMekaFish01"}, // Red

    // Dolphin. Params:
    //   param4 = model number (clamped to [0, 4])
    {0x0227, F_V3_V4, 0x0000400030000000, "__DOLPHIN__"},

    // Seabed capturing trap, similar to 0x0153 (TOTrapAncient01) in
    // function. Triggers when a player is within 15 units. Params:
    //   param1 = TODO (clamped to [0.1, 10])
    //   param4 = hold time after trigger (in seconds; clamped to [1, 60])
    //   param5 = hide in split-screen:
    //     zero or negative = always visible
    //     positive = invisible in split-screen mode
    //   param6 = same as param5 from 0x0153 (TOTrapAncient01)
    {0x0228, F_V3_V4, 0x0000400C3F800000, "TOTrapSeabed01"},

    // VR link object. This object is destroyed immediately in Challenge mode
    // and split-screen mode. Same parameters as 0x008D (TOCapsuleAncient01).
    {0x0229, F_V3_V4, 0x0000400FFFF80000, "TOCapsuleLabo"},

    // Alias for 0x0001 (TObjParticle). The constructor function is exactly
    // the same as for 0x0001, so this object has all the same paarameters
    // and behavior as that object.
    {0x0240, F_V3_V4, 0x0000400040000000, "TObjParticle"},

    // Teleporter after Barba Ray. This object behaves exactly the same as
    // 0x0002 (TObjAreaWarpForest), except it's invisible until the boss is
    // defeated.
    {0x0280, F_V3_V4, 0x0000400100000000, "__BARBA_RAY_TELEPORTER__"},

    // TODO: Describe this object. There appear to be no parameters.
    {0x02A0, F_V3_V4, 0x0000400200000000, "TObjLiveCamera"},

    // Gee nest. This object is technically an item box, and drops an item
    // when destroyed. Unlike most other item boxes, it cannot be specialized
    // (ignore_def is always true). Params are the same as for 0x0152
    // (TContainerAncient01), but there is also:
    //   angle.z = number of hits to destroy
    {0x02B0, F_V3_V4, 0x00004E0C0F800700, "TContainerAncient01R"},

    // Lab objects. None of these take any parameters.
    {0x02B1, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(0)"}, // Computer console
    {0x02B1, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(0)"}, // Computer console
    {0x02B2, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(1)"}, // Computer console (alternate colors)
    {0x02B2, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(1)"}, // Computer console (alternate colors)
    {0x02B3, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(2)"}, // Chair
    {0x02B3, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(2)"}, // Chair
    {0x02B4, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(3)"}, // Orange wall
    {0x02B4, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(3)"}, // Orange wall
    {0x02B5, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(4)"}, // Gray/blue wall
    {0x02B5, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(4)"}, // Gray/blue wall
    {0x02B6, F_V3_V4, 0x0000400000040000, "TObjLaboDesignBase(5)"}, // Long table
    {0x02B6, F_EP3, 0x0000000000000001, "TObjLaboDesignBase(5)"}, // Long table

    // Game Boy Advance. Params:
    //   param4 = quest label to call when activated (inherited from
    //     TObjMesBase)
    //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO")
    //     (inherited from TObjMesBase)
    {0x02B7, F_GC, 0x0000000000040001, "TObjGbAdvance"},

    // Like TObjQuestColA (TODO: In what ways is it different?). Parameters
    // are the same as for TObjQuestCol, but also:
    //   param2 = TODO
    //   param5 = quest script manager to use (zero or negative = quest,
    //     positive = free play)
    {0x02B8, F_V3_V4, 0x00006FFFFFFC7FFF, "TObjQuestColALock2"},
    {0x02B8, F_EP3, 0x0000000000000001, "TObjQuestColALock2"},

    // Like 0x0003 (TObjMapWarpForest), but is invisible and automatically
    // warps players when they enter its radius. This is used to simulate
    // floor warps in the Control Tower. Has the same parameters as
    // TObjMapWarpForest, but also:
    //   param5 = destination "floor" number (this is an intra-map warp and
    //     doesn't actually change floors; the "floor" number is only visual)
    //   param6 = if <= 0, shows "floor" number (param5 formatted as "%xF")
    //     after warp; if > 0, param5 is ignored
    {0x02B9, F_V3_V4, 0x00007FFC3FFF78FF, "TObjMapForceWarp"},
    {0x02B9, F_EP3, 0x0000000000000001, "TObjMapForceWarp"},

    // Behaves like 0x0012 (TObjQuestCol), but also has the param5 behavior
    // from 0x02B8 (TObjQuestColALock2).
    {0x02BA, F_V3_V4, 0x00006FFFFFFC7FFF, "TObjQuestCol2"},
    {0x02BA, F_EP3, 0x0000000000000001, "TObjQuestCol2"},

    // Episode 2 Lab door with glass window. Same parameters as
    // TODoorLaboNormal, except param5 is unused.
    {0x02BB, F_V3_V4, 0x0000400000040000, "TODoorLaboNormal"},

    // Episode 2 ending movie warp (used in final boss arenas after the boss
    // is defeated). Same params as 0x001C (TObjAreaWarpEnding).
    {0x02BC, F_V3_V4, 0x0000400080000000, "TObjAreaWarpEndingJung"},

    // Warp to another location on the same map. Used for the Lab/city warp.
    // This warp is visible in all game modes, but cannot be used in Episode
    // 1, Battle mode, or Challenge mode. Params:
    //   param1-3 = destination (same as for TObjMapWarpForest)
    //   param4 = destination angle (same as for TObjMapWarpForest)
    //   param6 = destination text (clamped to [0, 2]):
    //     00 = "The Principal"
    //     01 = "Pioneer 2"
    //     02 = "Lab"
    {0x02BD, F_V3_V4, 0x0000400000040000, "TObjLaboMapWarp"},

    // This object is used internally by Episode 3 during battles as the
    // visual implementation for some overlay tiles.
    // Params:
    //   param1-3 = TODO
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
    //   param6 second-low byte = TODO
    //   param6 low byte = TODO
    {0x02D0, F_EP3, 0x0000000000000002, "TObjKazariCard"},

    // TODO: Describe these objects. There appear to be no parameters.
    {0x02D1, F_EP3, 0x0000000000000001, "TObj_FloatingCardMaterial_Dark"},
    {0x02D2, F_EP3, 0x0000000000000001, "TObj_FloatingCardMaterial_Hero"},

    // Morgue warps. These don't actually do anything; they just look like a
    // warp. The actual warping is done by another object (TObjCityAreaWarp
    // for the lobby teleporter, or TShopGenerator for the battle counter).
    // TObjCardCityMapWarp takes no parameters.
    {0x02D3, F_EP3, 0x0000000000000001, "TObjCardCityMapWarp(0)"}, // Battle counter warp (blue lines)
    {0x02D9, F_EP3, 0x0000000000000001, "TObjCardCityMapWarp(1)"}, // TODO
    {0x02E3, F_EP3, 0x0000000000000001, "TObjCardCityMapWarp(2)"}, // Lobby warp (yellow lines)

    // Morgue doors. None of these take any parameters. Unsurprisingly, the
    // _Closed variants don't open.
    {0x02D4, F_EP3, 0x0000000000000001, "TObjCardCityDoor(0)"}, // Yellow (to deck edit room)
    {0x02D5, F_EP3, 0x0000000000000001, "TObjCardCityDoor(1)"}, // Blue (to battle entry counter)
    {0x02D8, F_EP3, 0x0000000000000001, "TObjCardCityDoor(2)"}, // TODO
    {0x02DF, F_EP3, 0x0000000000000001, "TObjCardCityDoor(3)"}, // Solid (always closed in normal Morgue)
    {0x02E0, F_EP3, 0x0000000000000001, "TObjCardCityDoor(4)"}, // Gray (to chief)
    {0x02DC, F_EP3, 0x0000000000000001, "TObjCardCityDoor_Closed(0)"}, // TODO
    {0x02DD, F_EP3, 0x0000000000000001, "TObjCardCityDoor_Closed(1)"}, // TODO
    {0x02DE, F_EP3, 0x0000000000000001, "TObjCardCityDoor_Closed(2)"}, // TODO
    {0x02E1, F_EP3, 0x0000000000000001, "TObjCardCityDoor_Closed(3)"}, // TODO
    {0x02E2, F_EP3, 0x0000000000000001, "TObjCardCityDoor_Closed(4)"}, // TODO

    // Mortis Fons geyser. Params:
    //   param1-3 = TODO
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
    //   param5 = TODO (value is param5 % 7)
    {0x02D6, F_EP3, 0x0000000000000002, "TObjKazariGeyserMizu"},

    // TODO: Describe this object. It appears to be created by many creatures
    // and probably SCs as well, but it's not obvious what it's used for,
    // since the logic of tiles being blocked or free is implemented in
    // TCardServer, which doesn't interact with this object. Further research
    // is needed here. Params:
    //   param1-3 = TODO
    //   param4 = TODO (expected to be 0 or 1)
    {0x02D7, F_EP3, 0x0000000000000002, "TObjSetCardColi"},

    // Floating robots, presumably. These are both subclasses of 0x0106
    // (TODragonflyMachine01) and take all the same params as that object.
    {0x02DA, F_EP3, 0x0000000000000001, "TOFlyMekaHero"},
    {0x02DB, F_EP3, 0x0000000000000001, "TOFlyMekaDark"},

    // Lobby banner or model display object. This implements display of media
    // sent with the B9 command. Params:
    //   param1-3 = scale factors (x, y, z)
    //   param4 = index into location bit field (0-31 where 0 is the least-
    //     significant bit; see Episode3LobbyBanners in config.example.json
    //     for the bits' meanings)
    //   param5 = TODO
    {0x02E4, F_EP3, 0x0000000000008001, "TObjSinBoardCard"},

    // TODO: Describe this object. Params:
    //   param4 = model number (0 or 1)
    {0x02E5, F_EP3, 0x0000000000000001, "TObjCityMoji"},

    // Like TObjCardCityMapWarp(2) (the warp to the lobby from the Morgue)
    // but doesn't render the circles. Used in offline mode where that warp
    // is disabled. No parameters.
    {0x02E6, F_EP3, 0x0000000000000001, "TObjCityWarpOff"},

    // Small flying robot. There appear to be no parameters.
    {0x02E7, F_EP3, 0x0000000000000001, "TObjFlyCom"},

    // TODO: Describe this object. Params:
    //   param4 = TODO (used in vtable[0x0E])
    {0x02E8, F_EP3, 0x0000000000000001, "__UNKNOWN_02E8__"},

    // Episode 4 light source.
    // TODO: Find and document this object's parameters.
    {0x0300, F_V4, 0x00005FF000000000, "__EP4_LIGHT__"},

    // Wilds/Crater cactus. Params:
    //   param1 = horizontal scale (x, z)
    //   param2 = vertical scale (y)
    //   param4 = model number (0-2, not bounds-checked)
    //   param5 = TODO
    {0x0301, F_V4, 0x00004FF000000000, "__WILDS_CRATER_CACTUS__"},

    // Wilds/Crater brown rock. Params:
    //   param1-3 = scale factors (x, y, z); z factor also scales hitbox size
    //   param4 = model number (0-2, not bounds-checked)
    {0x0302, F_V4, 0x00004FF000000000, "__WILDS_CRATER_BROWN_ROCK__"},

    // Wilds/Crater destructible brown rock. Params:
    //   param4 = switch flag number
    {0x0303, F_V4, 0x00004FF000000000, "__WILDS_CRATER_BROWN_ROCK_DESTRUCTIBLE__"},

    // TODO: Construct this object and see what it is. Params:
    //   param4 = object identifier (must be in range [0, 15]; used in 6xD4
    //     command)
    {0x0340, F_V4, 0x0000400000000000, "__UNKNOWN_0340__"},

    // TODO: Construct this object and see what it is. It looks like some
    // kind of child object of 0340. Params:
    //   param4 = object identifier (must be in range [0, 15]; used in 6xD4
    //     command; looks like it should match an existing 0340's identifier)
    //   param5 = child index? (must be in range [0, 3])
    {0x0341, F_V4, 0x0000400000000000, "__UNKNOWN_0341__"},

    // Poison plant. Base damage is 10 (Normal), 20 (Hard), 30 (Very Hard),
    // or 60 (Ultimate). There appear to be no parameters.
    {0x0380, F_V4, 0x00004E0000000000, "__POISON_PLANT__"},

    // TODO: Describe this object. Params:
    //   param4 = model number (clamped to [0, 1])
    {0x0381, F_V4, 0x00004E0000000000, "__UNKNOWN_0381__"},

    // TODO: Describe this object. There appear to be no parameters.
    {0x0382, F_V4, 0x00004E0000000000, "__UNKNOWN_0382__"},

    // Desert ooze plant. Params:
    //   param1 = animation speed?
    //   param2 = scale factor
    //   param4 = model number (clamped to [0, 1])
    {0x0383, F_V4, 0x00004E0000000000, "__DESERT_OOZE_PLANT__"},

    // TODO: Describe this object. Params:
    //   param1 = animation speed?
    //   param4 = TODO (clamped to [0, 1])
    {0x0385, F_V4, 0x00004E0000000000, "__UNKNOWN_0385__"},

    // Wilds/Crater black rocks. Params:
    //   param4 = model number (0-2, not bounds-checked)
    {0x0386, F_V4, 0x00004FF000000000, "__WILDS_CRATER_BLACK_ROCKS__"},

    // TODO: Describe this object. Params (names come from debug strings):
    //   angle.x = dest x
    //   angle.y = dest y
    //   angle.z = dest z
    //   param1 = area radius
    //   param2 = area power (value is param2 * 0.8)
    //   param4 = hole radius (value is param4 / 100)
    //   param5 = hole power (value is param5 / 100)
    {0x0387, F_V4, 0x00004E0000000000, "__UNKNOWN_0387__"},

    // TODO: Describe this object. Params:
    //   param1 = hitbox width (x; only used if param6 == 0)
    //   param2 = hitbox radius (only used if param6 != 0)
    //   param3 = hitbox depth (z; only used if param6 == 0)
    //   param4 = TODO (value is param4 / 100)
    //   param6 = hitbox type (0 = rectangular, anything else = cylindrical)
    {0x0388, F_V4, 0x00004E0000000000, "__UNKNOWN_0388__"},

    // Game flag set/clear zone. This sets and clears game flags (the flags
    // sent in 6x0A) when the player enters the object's hitbox. Params:
    //   param1-3 = same as for 0x0388
    //   param4 = game flags to set (low 8 bits only)
    //   param5 = game flags to clear (low 8 bits only)
    //   param6 = same as for 0x0388
    // There appears to be a bug that causes the game to always set all 8 of
    // the low game flags, regardless of the value of param4. The clearing
    // logic (param5) appears to work correctly.
    {0x0389, F_V4, 0x0000400000000000, "__GAME_FLAG_SET_CLEAR_ZONE__"},

    // HP drain zone. When a player is within this object's hitbox, it
    // subtracts 0.66% of the player's current HP at a regular interval. The
    // amount of damage per interval is capped below at 1 HP, so it will
    // always do a nonzero amount of damage each time. Params:
    //   param1-3 = same as for 0x0388
    //   param5 = interval (in frames) between damage applications
    //   param6 = same as for 0x0388
    {0x038A, F_V4, 0x0000400000000000, "__HP_DRAIN_ZONE__"},

    // Falling stalactite. Activates when any player is within 50 units. Base
    // damage is 100 on Normal, 200 on Hard, 300 on Very Hard, or 600 on
    // Ultimate. There appear to be no parameters.
    {0x038B, F_V4, 0x00004E0000000000, "__FALLING_STALACTITE__"},

    // Solid desert plant. Params:
    //   param1 = horizontal scale factor (x, z)
    //   param2 = vertical scale factor (y)
    {0x038C, F_V4, 0x00004E0000000000, "__DESERT_PLANT_SOLID__"},

    // Desert crystals-style box. Params:
    //   param1 = contents type:
    //     0 = always empty
    //     1 = standard random item (ignore_def = 1)
    //     2 = customized item (ignore_def = 0)
    //     3 = trigger set event
    // If param1 is 0 or 1, no other parameters are used. (In the case of 1,
    // param3-6 are sent to the server in the 6xA2 command; however, the
    // standard implementation ignores them.)
    // If param1 is 2, the other parameters have the same meanings as for
    // 0x0088 (TObjContainerBase2).
    // If param1 is 3, the event number is specified in param4.
    {0x038D, F_V4, 0x00004E0000000000, "__DESERT_CRYSTALS_BOX__"},

    // Episode 4 test door. param4 and param6 are the same as for 0x0056
    // (TODoorLabo).
    {0x038E, F_V4, 0x0000400000000000, "__EP4_TEST_DOOR__"},

    // Beehive. Params:
    //   param1 = horizontal scale factor (x, z)
    //   param2 = vertical scale factor (y)
    //   param4 = model number (clamped to [0, 1])
    {0x038F, F_V4, 0x00004E0000000000, "__BEEHIVE__"},

    // Episode 4 test particles. Generates particles at a specific location
    // (TODO) at a regular interval. Params:
    //   angle.x = TODO
    //   angle.y = TODO
    //   param1 = particle distance? (TODO)
    //   param2 = TODO
    //   param4 = frames between effects
    {0x0390, F_V4, 0x00004E0000000000, "__EP4_TEST_PARTICLE__"},

    // Heat (implemented as a type of poison fog). Has the same parameters as
    // TObjFogCollisionPoison.
    {0x0391, F_V4, 0x00004E0000000000, "__HEAT__"},

    // Episode 4 boss egg. There appear to be no parameters.
    {0x03C0, F_V4, 0x0000500000000000, "__EP4_BOSS_EGG__"},

    // Episode 4 boss rock spawner. Params:
    //   param4 = type (clamped to [0, 2])
    {0x03C1, F_V4, 0x0000500000000000, "__EP4_BOSS_ROCK_SPAWNER__"},
});

static const vector<DATEntityDefinition> dat_enemy_definitions({
    // This is newserv's canonical definition of map enemy and NPC types.

    // Enemies and NPCs take a similar arguments structure as objects:
    // objects use ObjectSetEntry, enemies use EnemySetEntry. Unlike objects,
    // some IDs are reused across game versions or areas, so the same enemy
    // type can generate a completely different entity on different game
    // versions. This is why some enemies have multiple entries with the same
    // type and different names.

    // Some enemies have params that the game's code references, but only in
    // places where their effects can't be seen (for example, in normally-
    // unused debug menus). These may have been used to test frame-by-frame
    // animations for some enemies; see TObjEneMe3Shinowa_v76 for an example
    // of this usage. The enemies with params like this are:
    // - TObjEneMe3ShinowaReal (param3, param4)
    // - TObjEneDf2Bringer (param1, param2)
    // - TObjEneRe7Berura (param1, param2)
    // - TBoss1Dragon (param1, param2)
    // - TBoss5Gryphon (param1, param2)
    // - TBoss2DeRolLe (param1, param2)
    // - TBoss8Dragon (param1, param2)
    // - TObjEneBm5GibonU (param4, param5; these params also have non-debug
    //     meanings)
    // - TObjEneMorfos (oaram1, param2; these params also have non-debug
    //     meanings)

    // NPCs. Params:
    //   param1 = action parameter (depends on param6; see below)
    //   param2 = visibility register number (if this is > 0, the NPC will
    //     only be visible when this register is nonzero; if this is >= 1000,
    //     the effective register is param2 - 1000 and register values for
    //     both param2 and param3 are read from the free play script
    //     environment instead of the quest script environment)
    //   param3 = hide override register number (if this is > 0, the NPC will
    //     not be visible when this register is nonzero, regardless of the
    //     state of the register specified by param2; if this is >= 1000, the
    //     effective register is param3 - 1000 and register values for both
    //     param2 and param3 are read from the free play script environment
    //     instead of the quest script environment)
    //   param4 = object number ("character ID" in qedit; if this is outside
    //     the range [100, 999], the quest label in param5 is called in the
    //     free play script instead of the quest script)
    //   param5 = quest label to call when interacted with (if zero, NPC does
    //     nothing upon interaction)
    //   param6 = specifies what NPC does when idle:
    //     0 = stand still (param1 is ignored)
    //     1 = walk around randomly (param1 = max walk distance from home)
    //     2 = TODO (Ep3 only; appears to be unused)
    //     3 = TODO (Ep3 only; appears to be unused)
    // TODO: setting param4 to 0 changes something else about the NPC; figure
    // out what this does (see TObjNpcBase_v57_set_config_from_params)
    {0x0001, F_V0_V4, 0x0000200000000001, "TObjNpcFemaleBase"}, // Woman with red hair and purple outfit
    {0x0001, F_EP3, 0x0000000000000001, "TObjNpcFemaleBase"}, // Woman with red hair and purple outfit
    {0x0002, F_V0_V4, 0x0000200000000001, "TObjNpcFemaleChild"}, // Shorter version of the above
    {0x0002, F_EP3, 0x0000000000000001, "TObjNpcFemaleChild"}, // Shorter version of the above
    {0x0003, F_V0_V4, 0x0000200000040001, "TObjNpcFemaleDwarf"}, // Woman wearing green outfit
    {0x0003, F_EP3, 0x0000000000000001, "TObjNpcFemaleDwarf"}, // Woman wearing green outfit
    {0x0004, F_V0_V4, 0x0000200000000001, "TObjNpcFemaleFat"}, // Woman outside Hunter's Guild
    {0x0004, F_EP3, 0x0000000000000001, "TObjNpcFemaleFat"}, // Woman outside Hunter's Guild
    {0x0005, F_V0_V4, 0x0000200000000001, "TObjNpcFemaleMacho"}, // Tool shop woman
    {0x0005, F_EP3, 0x0000000000000001, "TObjNpcFemaleMacho"}, // Tool shop woman
    {0x0006, F_V0_V4, 0x0000200000040001, "TObjNpcFemaleOld"}, // Older woman with yellow/red outfit
    {0x0006, F_EP3, 0x0000000000000001, "TObjNpcFemaleOld"}, // Older woman with yellow/red outfit
    {0x0007, F_V0_V4, 0x0000200000000001, "TObjNpcFemaleTall"}, // Woman walking around inside shop area
    {0x0007, F_EP3, 0x0000000000000001, "TObjNpcFemaleTall"}, // Woman walking around inside shop area
    {0x0008, F_V0_V4, 0x0000200000008001, "TObjNpcMaleBase"}, // Similar appearance to weapon shop man
    {0x0008, F_EP3, 0x0000000000008001, "TObjNpcMaleBase"}, // Similar appearance to weapon shop man
    {0x0009, F_V0_V4, 0x0000200000040001, "TObjNpcMaleChild"}, // Kid wearing purple
    {0x0009, F_EP3, 0x0000000000000001, "TObjNpcMaleChild"}, // Kid wearing purple
    {0x000A, F_V0_V4, 0x0000200000000001, "TObjNpcMaleDwarf"}, // Man outside Medical Center
    {0x000A, F_EP3, 0x0000000000000001, "TObjNpcMaleDwarf"}, // Man outside Medical Center
    {0x000B, F_V0_V4, 0x0000200000040001, "TObjNpcMaleFat"}, // Armor shop man
    {0x000B, F_EP3, 0x0000000000000001, "TObjNpcMaleFat"}, // Armor shop man
    {0x000C, F_V0_V4, 0x0000200000000001, "TObjNpcMaleMacho"}, // Weapon shop man
    {0x000C, F_EP3, 0x0000000000000001, "TObjNpcMaleMacho"}, // Weapon shop man
    {0x000D, F_V0_V4, 0x0000200000040001, "TObjNpcMaleOld"}, // Man near telepipe locations
    {0x000D, F_EP3, 0x0000000000000001, "TObjNpcMaleOld"}, // Man near telepipe locations
    {0x000E, F_V0_V4, 0x0000200000040001, "TObjNpcMaleTall"}, // Man wearing turquoise
    {0x000E, F_EP3, 0x0000000000000001, "TObjNpcMaleTall"}, // Man wearing turquoise
    {0x0019, F_V0_V4, 0x00003FF000040001, "TObjNpcSoldierBase"}, // Man right of the Ragol warp door
    {0x0019, F_EP3, 0x0000000000000001, "TObjNpcSoldierBase"}, // Man right of the Ragol warp door
    {0x001A, F_V0_V4, 0x0000200000000001, "TObjNpcSoldierMacho"}, // Man left of the Ragol warp door
    {0x001A, F_EP3, 0x0000000000000001, "TObjNpcSoldierMacho"}, // Man left of the Ragol warp door
    {0x001B, F_V0_V4, 0x0000200000040001, "TObjNpcGovernorBase"}, // Principal Tyrell
    {0x001B, F_EP3, 0x0000000000000001, "TObjNpcGovernorBase"}, // Principal Tyrell
    {0x001C, F_V0_V4, 0x0000200000040001, "TObjNpcConnoisseur"}, // Tekker
    {0x001D, F_V0_V4, 0x0000200000040021, "TObjNpcCloakroomBase"}, // Bank woman
    {0x001E, F_V0_V4, 0x0000200000000001, "TObjNpcExpertBase"}, // Man in front of bank
    {0x001F, F_V0_V4, 0x0000200000040001, "TObjNpcNurseBase"}, // Nurses in Medical Center
    {0x0020, F_V0_V4, 0x0000200000040001, "TObjNpcSecretaryBase"}, // Irene
    {0x0020, F_EP3, 0x0000000000000001, "TObjNpcSecretaryBase"}, // Karen
    {0x0021, F_V0_V4, 0x0000200000000001, "TObjNpcHHM00"}, // TODO
    {0x0021, F_EP3, 0x0000000000000001, "TObjNpcHHM00"}, // TODO
    {0x0022, F_V0_V4, 0x0000200000000001, "TObjNpcNHW00"}, // TODO
    {0x0022, F_EP3, 0x0000000000000001, "TObjNpcNHW00"}, // TODO
    {0x0024, F_V0_V4, 0x0000200000000001, "TObjNpcHRM00"}, // TODO
    {0x0025, F_V0_V4, 0x0000200000040001, "TObjNpcARM00"}, // TODO
    {0x0026, F_V0_V4, 0x0000200000040001, "TObjNpcARW00"}, // TODO
    {0x0026, F_EP3, 0x0000000000000001, "TObjNpcARW00"}, // TODO
    {0x0027, F_V0_V4, 0x0000200000040001, "TObjNpcHFW00"}, // TODO
    {0x0027, F_EP3, 0x0000000000000001, "TObjNpcHFW00"}, // TODO
    {0x0028, F_V0_V4, 0x0000200000040001, "TObjNpcNFM00"}, // TODO
    {0x0028, F_EP3, 0x0000000000000001, "TObjNpcNFM00"}, // TODO
    {0x0029, F_V0_V4, 0x00003C0000000001, "TObjNpcNFW00"}, // TODO
    {0x0029, F_EP3, 0x0000000000000001, "TObjNpcNFW00"}, // TODO
    {0x002B, F_V0_V4, 0x0000200000000001, "TObjNpcNHW01"}, // TODO
    {0x002C, F_V0_V4, 0x0000200000000001, "TObjNpcAHM01"}, // TODO
    {0x002D, F_V0_V4, 0x0000200000000001, "TObjNpcHRM01"}, // TODO
    {0x0030, F_V0_V4, 0x0000200000000001, "TObjNpcHFW01"}, // TODO
    {0x0031, F_V0_V4, 0x0000200000040001, "TObjNpcNFM01"}, // TODO
    {0x0031, F_EP3, 0x0000000000000001, "TObjNpcNFM01"}, // TODO
    {0x0032, F_V0_V4, 0x00002C0000000001, "TObjNpcNFW01"}, // TODO
    {0x0045, F_V0_V4, 0x00000FF40F800006, "TObjNpcLappy"}, // Rappy NPC
    {0x0046, F_V0_V4, 0x0000000000000004, "TObjNpcMoja"}, // Small Hildebear NPC
    {0x0047, F_V2, 0x0000000000000004, "TObjNpcRico"}, // Rico
    {0x00A9, F_V0_V4, 0x0000000000000600, "TObjNpcBringer"}, // Dark Bringer NPC
    {0x00D0, F_V3_V4, 0x0000200000040001, "TObjNpcKenkyu"}, // Ep2 armor shop man
    {0x00D1, F_V3_V4, 0x0000200000040001, "TObjNpcSoutokufu"}, // Natasha Milarose
    {0x00D2, F_V3_V4, 0x0000000000040000, "TObjNpcHosa"}, // Dan
    {0x00D3, F_V3_V4, 0x000000F000040000, "TObjNpcKenkyuW"}, // Ep2 tool shop woman
    {0x00D6, F_EP3, 0x0000000000000001, "TObjNpcHeroGovernor"}, // Morgue Chief
    {0x00D7, F_EP3, 0x0000000000000001, "TObjNpcHeroGovernor"}, // Morgue Chief (direct alias of 00D6)
    {0x00F0, F_V3_V4, 0x0000000000040000, "TObjNpcHosa2"}, // Man next to room with warp to Lab
    {0x00F1, F_V3_V4, 0x0000000000040000, "TObjNpcKenkyu2"}, // Ep2 weapon shop man
    {0x00F2, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00F2)"}, // TODO
    {0x00F3, F_V3_V4, 0x00003FF000040000, "TObjNpcNgcBase(0x00F3)"}, // TODO
    {0x00F4, F_V3_V4, 0x00003FF030040000, "TObjNpcNgcBase(0x00F4)"}, // TODO
    {0x00F5, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00F5)"}, // TODO
    {0x00F6, F_V3_V4, 0x000000080F840000, "TObjNpcNgcBase(0x00F6)"}, // TODO
    {0x00F7, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00F7)"}, // Nol
    {0x00F8, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00F8)"}, // Elly
    {0x00F9, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00F9)"}, // Woman with cyan hair down the ramp from Ep2 Medical Center
    {0x00FA, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00FA)"}, // Woman with bright red hair down the ramp from Ep2 Medical Center
    {0x00FB, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00FB)"}, // Man with blue hair near the Ep2 Medical Center
    {0x00FC, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00FC)"}, // Man in room next to Ep2 Hunter's Guild
    {0x00FD, F_V3_V4, 0x000000040F840000, "TObjNpcNgcBase(0x00FD)"}, // TODO
    {0x00FE, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00FE)"}, // Episode 2 Hunter's Guild woman
    {0x00FF, F_V3_V4, 0x0000000000040000, "TObjNpcNgcBase(0x00FF)"}, // Woman near room with teleporter to VR areas
    {0x0100, F_V4, 0x0000200000040001, "__MOMOKA__"}, // Momoka
    {0x0110, F_EP3, 0x0000000000000001, "TObjNpcWalkingMeka_Hero"}, // Small talking robot in Morgue
    {0x0111, F_EP3, 0x0000000000000001, "TObjNpcWalkingMeka_Dark"}, // Small talking robot in Morgue

    // Episode 3 scientist and aide NPCs. These NPCs take all the same params
    // as the NPCs defined above, but also:
    //   angle.x = model number (clamped to [0, 3] for scientists, [0, 2] for
    //     aides)
    // The two type values for scientists (00D4 and 00D5) are direct aliases
    // for each other; there is no difference between their in-game appearance
    // or behavior.
    {0x00D4, F_EP3, 0x0000000000000001, "TObjNpcHeroScientist"},
    {0x00D5, F_EP3, 0x0000000000000001, "TObjNpcHeroScientist"},
    {0x0112, F_EP3, 0x0000000000000001, "TObjNpcHeroAide"},

    // Quest NPC. Params are the same as for the standard NPCs above, except:
    //   param6 low byte = flags (bit field):
    //     01 = same as param6 above (0 = stand still; 1 = walk around)
    //     10 = TODO
    //   param6 high byte = NPC index in npcplayerchar.dat
    {0x0118, F_V4, 0x00007FF000000000, "__QUEST_NPC__"},

    // Enemy that behaves like an NPC. Has all the same params as the
    // standard NPC types, but also:
    //   angle.x = definition index
    // The definition index is an integer from 0 to 15 (decimal) specifying
    // which model, animations, and hitbox to use. The available choices depend
    // on which assets are loaded, which in turn depend on the area (not floor)
    // in which the NPC appears. To choose the right definition index, first
    // look up the enemy you want in the models list below, then find the
    // corresponding number in the areas table below that, and use the (zero-
    // based) index of the number in that table for angle.x. For example, to
    // place a Sinow Gold in Mine 2, angle.x should be 8 since Sinow Gold is
    // 27, and there are 8 other values before 27 in the Mine 2 list.
    // The available models are:
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
    //   19: Dark Gunner (must be constructed after a Dark Gunner enemy, since
    //       the definition is populated in the enemy constructor, not in the
    //       asset loader)
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
    //   32: TODO (from bm_ene_bm2_moja_bml) (Ep1/2), Astark (Ep4)
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
    //   Episode 4 (note that these are not usable without a client patch,
    //     since this NPC is only available on Pioneer 2 in Episode 4):
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
    // This NPC exists in Episode 3, but is only available in the Morgue and in
    // the lobby. There are no available models in those areas, which makes it
    // essentially useless.
    {0x0033, F_V3_V4, 0x0000200FFFFFFFFF, "TObjNpcEnemy"},
    {0x0033, F_EP3, 0x0000000000008001, "TObjNpcEnemy"},

    // Hildebear. Params:
    //   param1 = initial location (zero or negative = ground, positive = jump)
    //   param2 = chance to use tech (value is param3 + 0.6, clamped to [0,
    //     1]; TODO: it's not clear when exactly the decision points are)
    //   param3 = chance to jump when more than 150 units away (value is
    //     param2 + 0.3, clamped to [0, 1])
    //   param6 = if >= 1, always rare
    {0x0040, F_V0_V4, 0x00000000001B0004, "TObjEneMoja"},

    // Rappy. Params:
    //   param1 = initial location (zero or negative = ground, positive = sky;
    //     ignored if wave_number is > 0 in which case it's always sky)
    //   param6 = rare flag (on v1-v3, rappy is rare if param6 != 0; on v4,
    //     rappy is rare if (param6 & 1) != 0)
    //   param7 = TODO
    // Exactly which rappy is constructed depends on param6 (or the random
    // rare check) and the current season event:
    //   Ep1/Ep2 non-rare = Rag Rappy
    //   Ep4 non-rare = Sand Rappy (Crater or Desert variation)
    //   Ep1 rare = Al Rappy
    //   Ep2 rare, Christmas = Saint Rappy
    //   Ep2 rare, Easter = Egg Rappy
    //   Ep2 rare, Halloween = Hallo Rappy
    //   Ep2 rare, any other season event (or none) = Love Rappy
    //   Ep4 rare = Del Rappy (Crater or Desert variation)
    {0x0041, F_V0_V4, 0x00004FF000180006, "TObjEneLappy"},

    // Monest (and Mothmants). Params:
    //   param2 = number of Mothmants to expel at start (clamped to [0, 6])
    //   param3 = total Mothmants (clamped to [0, min(30, num_children)]
    //     where num_children comes from the EnemySetEntry; if this is less
    //     than param2, then param2 will take precedence but no further
    //     Mothmants will emerge after the first group)
    // Note: In map_forest01_02e.dat in the vanilla map files there is a
    // Monest that has param1 = 3 and param2 = 10. This looks like just an
    // off-by-one error on Sega's part where they accidentally shifted the
    // parameters down by one place. As described above, this Monest expels
    // 6 Mothmants immediately, then no more after those 6 are killed.
    {0x0042, F_V0_V4, 0x0000000000180006, "TObjEneBm3FlyNest"},

    // Savage Wolf or Barbarous Wolf. Params:
    //   param1 = group number (when a Barbarous Wolf dies, all wolves with
    //     the same group number howl and trigger their buffs or weaknesses)
    //   param2 = if less than 1, this is a Savage Wolf; otherwise it's a
    //     Barbarous Wolf
    {0x0043, F_V0_V4, 0x0000000000600006, "TObjEneBm5Wolf"},

    // Booma, Gobooma, or Gigobooma. Params:
    //   param1 = TODO (fraction of max HP; see TObjEnemyV8048ee80_v5A)
    //   param2 = idle walk radius (when there's no target, it will walk
    //     around its spawn location within this radius; if this is zero, it
    //     stands still instead)
    //   param6 = type (0 = Booma, 1 = Gobooma, 2 = Gigobooma)
    //   param7 = group ID (if nonzero, it looks like this is used to cause
    //     groups of enemies to band together and all attack the same player,
    //     chosen by the highest-ranking enemy (by param6) in the group;
    //     TODO: this explanation is unverified and param7 was never used by
    //     Sega; see client code at 3OE1:800F6F3C)
    {0x0044, F_V0_V4, 0x0000000000000006, "TObjEneBeast"},

    // Grass Assassin. Params:
    //   param1 = TODO
    //   param2 = TODO (some state is set based on whether this is <= 0 or
    //     not, but the value is also used directly in some places)
    //   param3 = TODO (see TObjGrass_update_case8)
    //   param4 = TODO (see TObjGrass_update_case8)
    {0x0060, F_V0_V4, 0x00000000001B0018, "TObjGrass"},

    // Poison Lily or Del Lily. Del Lily is constructed if the current area
    // is 0x23 (Control Tower); otherwise, Poison Lily is constructed. There
    // appear to be no parameters.
    {0x0061, F_V0_V4, 0x0000000800180038, "TObjEneRe2Flower"},

    // Nano Dragon. Params:
    //   param1 = TODO (seems it only matters if this is 1 or not)
    //   param2 = TODO (defaults to 50 if param2 < 1)
    //   param7 = TODO (set in init)
    {0x0062, F_V0_V4, 0x0000000000000038, "TObjEneNanoDrago"},

    // Evil Shark, Pal Shark, or Guil Shark. Same params as 0x0044
    // (TObjEneBeast), except:
    //   param6 = type (0 = Evil Shark, 1 = Pal Shark, 2 = Guil Shark)
    {0x0063, F_V0_V4, 0x0000000000030038, "TObjEneShark"},

    // Pofuilly Slime. num_children is clamped to [0, 4]. Params:
    //   param7 = rare flag (if the lowest bit is set, this is a Pouilly
    //     Slime instead; on BB, this is ignored)
    {0x0064, F_V0_V4, 0x0000000000000030, "TObjEneSlime"},

    // Pan Arms (Hidoom + Migium). There appear to be no parameters.
    {0x0065, F_V0_V4, 0x0000000000600028, "TObjEnePanarms"},

    // Gillchic or Dubchic. Params:
    //   param1 = rapid fire count (number of lasers fired before moving
    //     again; if this is 0, the default of 2 is used)
    //   param6 = type (0 = Dubchic, 1 = Gillchic)
    //   param7 = TODO
    {0x0080, F_V0_V4, 0x00000000006000C0, "TObjEneDubchik"},

    // Garanz. There appear to be no parameters.
    // TODO: There is some behavior difference if wave_number is 0 vs. any
    // other value. Figure out what exactly this does.
    {0x0081, F_V0_V4, 0x00000000002000C0, "TObjEneGyaranzo"},

    // Sinow Beat. Params:
    //   param1 = disable mirage effect if >= 1.0
    //   param2 = is Sinow Gold if >= 1.0
    // Note: All params are on the base class (TObjEneMe3Shinowa).
    {0x0082, F_V0_V4, 0x00000000000300C0, "TObjEneMe3ShinowaReal"},

    // Canadine. Params:
    //   param1 = behavior (0 = in fighter, 1 = out fighter; this controls
    //     whether the Canadine will use its direct attack or stay high off
    //     the ground instead)
    {0x0083, F_V0_V4, 0x00000000000000C0, "TObjEneMe1Canadin"},

    // Canane. There appear to be no parameters. There are always 8 followers
    // arranged in a ring around the Canane.
    {0x0084, F_V0_V4, 0x00000000000000C0, "TObjEneMe1CanadinLeader"},

    // Dubwitch. Destroying a Dubwitch destroys all Dubchics in the same
    // room. There appear to be no parameters.
    {0x0085, F_V0_V4, 0x00000000006000C0, "TOCtrlDubchik"},

    // Delsaber. Params:
    //   param1 = jump distance delta (value used is param1 + 100)
    //   param2 = prejudice flag (these values directly correspond to the
    //     bits in PlayerVisualConfig::class_flags; see below for details):
    //     0 = males
    //     1 = females
    //     2 = humans
    //     3 = newmans
    //     4 = androids
    //     5 = hunters
    //     6 = rangers
    //     7 = forces
    //     8 = no prejudice
    // If any player is within 30 units of a Delsaber, it will target that
    // player. Otherwise, the Delsaber will target the nearest player that
    // matches its prejudice flag; if no player matches this flag, it will
    // target the nearest player.
    {0x00A0, F_V0_V4, 0x0000000000630300, "TObjEneSaver"},

    // Chaos Sorceror. There appear to be no parameters.
    {0x00A1, F_V0_V4, 0x0000000000400500, "TObjEneRe4Sorcerer"},

    // Dark Gunner. Params:
    //   param1 = group number (there should be between 1 and 16 Dark Gunners
    //     and one control enemy with the same group number in the same room)
    //   param2 = TODO (number within group? possibly unused?)
    //   param7 = TODO
    {0x00A2, F_V0_V4, 0x0000000000000600, "TObjEneDarkGunner"},

    // Dark Gunner control enemy. This enemy doesn't actually exist in-game;
    // it only has logic for choosing a Dark Gunner from its group to be the
    // leader, and then changing this leader periodically. Params:
    //   param1 = group number (see above)
    {0x00A3, F_V0_V4, 0x0000000000000600, "TObjEneDarkGunCenter"},

    // Dark Bringer. There appear to be no parameters.
    {0x00A4, F_V0_V4, 0x0000000000030600, "TObjEneDf2Bringer"},

    // Dark Belra. There appear to be no parameters.
    {0x00A5, F_V0_V4, 0x0000000000180500, "TObjEneRe7Berura"},

    // Dimenian / La Dimenian / So Dimenian. Same parameters as 0x0044
    // (TObjEneBeast), except:
    //   param6 = type (0 = Dimenian, 1 = La Dimenian, 2 = So Dimenian)
    {0x00A6, F_V0_V4, 0x0000000000180700, "TObjEneDimedian"},

    // Bulclaw. There appear to be no parameters.
    {0x00A7, F_V0_V4, 0x0000000000000700, "TObjEneBalClawBody"},

    // Claw. There appear to be no parameters.
    {0x00A8, F_V0_V4, 0x0000000000000700, "TObjEneBalClawClaw"},

    // Early bosses. None of these take any parameters.
    {0x00C0, F_V0_V4, 0x0000000000000800, "TBoss1Dragon"}, // Dragon
    {0x00C0, F_V3_V4, 0x0000000040000000, "TBoss5Gryphon"}, // Gal Gryphon
    {0x00C1, F_V0_V4, 0x0000000000001000, "TBoss2DeRolLe"}, // De Rol Le

    // Vol Opt and various pieces thereof. Generally only TBoss3Volopt and
    // TBoss3VoloptP02 should be specified in map files; the other enemies
    // are automatically created by TBoss3Volopt. None of these take any
    // parameters.
    {0x00C2, F_V0_V4, 0x0000000000002000, "TBoss3Volopt"}, // Main control object
    {0x00C3, F_V0_V4, 0x0000000000002000, "TBoss3VoloptP01"}, // Phase 1 (x6; one for each big monitor)
    {0x00C4, F_V0_V4, 0x0000000000002000, "TBoss3VoloptCore"}, // Core
    {0x00C5, F_V0_V4, 0x0000000000002000, "TBoss3VoloptP02"}, // Phase 2
    {0x00C6, F_V0_V4, 0x0000000000002000, "TBoss3VoloptMonitor"}, // Monitor (x24; 4 for each wall)
    {0x00C7, F_V0_V4, 0x0000000000002000, "TBoss3VoloptHiraisin"}, // Pillar (lightning rod)

    // More bosses. None of these take any parameters.
    {0x00C8, F_V0_V4, 0x0000000000004000, "TBoss4DarkFalz"}, // Dark Falz
    {0x00CA, F_V3_V4, 0x0000000080000000, "TBoss6PlotFalz"}, // Olga Flow
    {0x00CB, F_V3_V4, 0x0000000100000000, "TBoss7DeRolLeC"}, // Barba Ray
    {0x00CC, F_V3_V4, 0x0000000200000000, "TBoss8Dragon"}, // Gol Dragon

    // Sinow Berill or Sinow Spigell. Params:
    //   param1 = spawn type:
    //     0 = invisible + ground
    //     1 = invisible + ceiling
    //     2 = visible + ground
    //     3 = visible + ceiling
    //   param2 = chance to enable stealth (value used is param2 + 0.3)
    //   param3 = chance to cast technique (value used is param3 + 0.4)
    //   param4 = chance to teleport (value used is param4 + 0.5)
    //   param5 = chance to disable stealth (value used is param5 + 0.1;
    //     applies when hit, but (TODO) also some other events)
    //   param6 = type:
    //     zero or negative = Sinow Berill
    //     positive = Sinow Spigell
    // param2, param3, and param4 are evaluated in that order after the Sinow
    // jumps back. That is, the game first generates a random float between 0
    // and 1, and compares it to param2 to decide whether to enable stealth.
    // If it does, the other params are ignored. If it doesn't, the game then
    // checks param3 in the same manner to determine whether to cast a tech;
    // if that doesn't happen either, the game checks param4 to determine
    // whether to teleport. If none of those happen, the Sinow just walks
    // forward again and attacks.
    {0x00D4, F_V3_V4, 0x000000000F800000, "TObjEneMe3StelthReal"},

    // Merillia / Meriltas. Params:
    //   param1 = chance to run away after being hit (value used is param1 -
    //     0.2, clamped below to 0)
    //   param3 = chance to do poison attack after being hit (value used is
    //     param3 - 0.2, clamped below to 0)
    //   param4 = distance to run away (value used is param4 + 300)
    //   param5 = wakeup radius delta (value used is param5 + 100, clamped
    //     below to 15; enemy will wake up when any player is nearby)
    //   param6 = type (0 = Merillia, 1 = Meriltas)
    {0x00D5, F_V3_V4, 0x000000040F800000, "TObjEneMerillLia"},

    // Mericarol / Mericus / Merikle / Mericarand. Params:
    //   param1 = chance of doing run attack after being hit when HP is less
    //     than half of max (value used is param1 + 0.5)
    //   param2 = speed during run attack (units per frame; value used is
    //     param2 + 3, clamped below to 1)
    //   param3 = chance of doing spit attack when player is nearby (actual
    //     probability is param3 + 0.1; if the check fails, it will do the
    //     slash attack instead)
    //   param6 = subtype:
    //     0 = Mericarol
    //     1 = Mericus
    //     2 = Merikle
    //     anything else = Mericarand (see below)
    // If this is a Mericarand, it is "randomly" chosen to be one of the
    // three subtypes at construction time. On v1-v3, the client chooses
    // randomly (but consistently, based on the entity ID) between Mericarol
    // (80%), Mericus (10%) or Merikle (10%). On v4, if the entity ID isn't
    // marked rare by the server, the Mericarand becomes a Mericarol;
    // otherwise, it becomes a Mericus if its entity ID is even or a Merikle
    // if it's odd.
    {0x00D6, F_V3_V4, 0x000000080F800000, "TObjEneBm9Mericarol"},

    // Ul Gibbon / Zol Gibbon. Params:
    //   param1 = group number
    //   param2 = appear type (<1 = spot appear; >=1 = jump appear)
    //   param3 = chance of jumping forward or back at each decision point
    //     (value used is param3 + 0.4)
    //   param4 = chance of casting a tech when not near any player (value
    //     used is param4 + 0.3)
    //   param5 = chance of casting a tech immediately after jumping forward
    //     or back (value used is param5 + 0.3; does not apply after jumps
    //     that are attacks)
    //   param6 = type (zero or negative = Ul Gibbon, positive = Zol Gibbon)
    {0x00D7, F_V3_V4, 0x000000040F800000, "TObjEneBm5GibonU"},

    // Gibbles. Params:
    //   param1 = jump distance delta (value used is param1 + 100)
    //   param2 = prejudice flag (see 0x00A0 (TObjEneSaver); the behavior
    //     here is exactly the same)
    //   param3 = chance to jump at each decision point (0-1)
    //   param4 = chance to jump after being hit (0-1)
    {0x00D8, F_V3_V4, 0x000000080F800000, "TObjEneGibbles"},

    // Gee. Params:
    //   param1 = in fighter / out fighter setting (see TObjEneMe1Canadin)
    //   param2 = appear height (0 = low, 1 = high)
    //   param3 = appear speed (value is param3 + 1, clamped below to 1)
    //   param4 = needle speed (value is param4 + 6, clamped below to 0.01)
    // Note: The client's debug strings say "APPEAR SPEED(+1.f Min:0.01f)"
    // for param3 and "NEEDLE SPEED(+6.f Min:1.f)" for param4. The "Min:"
    // parts of those strings are incorrect; the comments above are correct.
    {0x00D9, F_V3_V4, 0x000000040F800000, "TObjEneMe1Gee"},

    // Gi Gue. Params:
    //   param1 = TODO (only matters if >= 1 or not)
    //   param2 = TODO (only used if param1 >= 1; defaults to 50 if param2 <
    //     1; maybe same as Nano Dragon's param2?)
    //   param3 = TODO (value is param3 + 150)
    //   param4 = TODO (value is param4 + 0.5)
    //   param5 = TODO (value is param5 + 0.5)
    //   param7 = TODO
    {0x00DA, F_V3_V4, 0x000000080F800000, "TObjEneMe1GiGue"},

    // Deldepth. Params:
    //   param1 = TODO (value is param1 + 0.6, clamped below to 0; seems to
    //     be unused?)
    {0x00DB, F_V3_V4, 0x0000000030000000, "TObjEneDelDepth"},

    // Delbiter. Params:
    //   param1 = chance to howl (value is param1 + 0.3)
    //   param2 = chance to cause confusion via howl (value is param2 + 0.3)
    //   param3 = maximum distance at which howl can cause confusion (value
    //     is param3 + 30, clamped below to 10)
    //   param4 = chance to fire laser (value is param4 + 0.3)
    //   param5 = chance to charge (value is param5 + 0.05)
    //   param6 = type (0 = stand, 1 = run)
    {0x00DC, F_V3_V4, 0x0000000830000000, "TObjEneDellBiter"},

    // Dolmolm / Dolmdarl. Same parameters as TObjEneBeast, but also:
    //   param3 = TODO (value is param3 + 0.3; probability of some sort)
    //   param4 = TODO (value is param4 + 0.3; probability of some sort)
    //   param6 = type (zero or negative = Dolmolm; positive = Dolmdarl)
    {0x00DD, F_V3_V4, 0x0000000430000000, "TObjEneDolmOlm"},

    // Morfos. Params:
    //   param1 = TODO (value is param1 + 0.28)
    //   param2 = TODO (value is param2 + 20)
    //   param3 = TODO (value is param3 + 0.1; probability; used when current
    //     HP is less than half of max)
    //   param4 = TODO (value is param4 + 0.1; probability; used when current
    //     HP is less than half of max)
    //   param6 = TODO
    {0x00DE, F_V3_V4, 0x0000000030000000, "TObjEneMorfos"},

    // Recobox. Params:
    //   param1 = Recon floating height (value used for each Recon is 45 +
    //     rand(-2, 2) + param1)
    //   param2 = Recon target radius (distance from target; value used for
    //     each Recon is 50 + rand(-5, 5) + param2; param2 is clamped below
    //     to -30)
    //   param4 = maximum number of concurrently-active Recons (clamped to
    //     [0, min(6, num_children - 1)])
    //   param6 = type:
    //     zero or negative = floor (Recons exit upward)
    //     1 = ceiling (Recons exit downward)
    //     2 or greater = wall (Recons exit horizontally)
    // Note: The debug strings in TObjEneRecobox_v6A seem to imply that the
    // total Recon count in the box is (param7 + 1); however, this is not
    // true. The total Recon count in the box is actually (num_children - 1).
    {0x00DF, F_V3_V4, 0x0000000C30000000, "TObjEneRecobox"},

    // Sinow Zoa / Sinow Zele. It appears to take the same params as
    // TObjEneMe3StelthReal (Sinow Berill / Sinow Spigell), except (of course):
    //   param6 = type:
    //     zero or negative = Sinow Zoa
    //     positive = Sinow Zele
    {0x00E0, F_V3_V4, 0x0000000030000000, "TObjEneMe3SinowZoaReal"},

    // Epsilon. Params:
    //   param1 = TODO (value is param1 + 0.5, clamped below to 0)
    //   param2 = TODO (value is param2 + 512; it appears this was supposed
    //     to be clamped below to 0, but due to a copy/paste error it isn't)
    //   param3 = TODO (value is (param3 + 20) * 5, clamped below to 150)
    //   param4 = TODO (value is (param4 + 20) * 5, clamped below to 150)
    {0x00E0, F_V3_V4, 0x0000000800000000, "TObjEneEpsilonBody"},

    // Ill Gill. Params:
    //   param1 = TODO (seems it only matters if this is zero or not; are there other uses?)
    //   param2 = TODO (used in TObjEneIllGill_update phase 3)
    {0x00E1, F_V3_V4, 0x0000000800000000, "TObjEneIllGill"},

    // Astark. Params:
    //   param1 = TODO (only matters if this is 1 or not)
    //   param2 = TODO (value is param2 + 0.3, clamped to [0, 1])
    //   param3 = TODO (value is param3 + 0.6, clamped to [0, 1])
    {0x0110, F_V4, 0x000041F000000000, "__ASTARK__"},

    // Satellite Lizard / Yowie. Params:
    //   param1 = TODO (rounded to integer)
    //   param2 = type (<1 = Satellite Lizard, >=1 = Yowie)
    {0x0111, F_V4, 0x00004FF000000000, "__SATELLITE_LIZARD_YOWIE__"},

    // Merissa A. Params:
    //   param6 = flags (bit field):
    //     0001 = always rare (Merissa AA)
    {0x0112, F_V4, 0x00004E0000000000, "__MERISSA_A__"},

    // Girtablulu. There appear to be no parameters.
    {0x0113, F_V4, 0x00004E0000000000, "__GIRTABLULU__"},

    // Zu. Params:
    //   param6 = flags (bit field):
    //     0001 = always rare (Pazuzu)
    //   param7 = TODO
    {0x0114, F_V4, 0x00004FF000000000, "__ZU__"},

    // Boota / Ze Boota / Ba Boota. Same parameters as TObjEneBeast, but also:
    //   param3 = TODO (see v20)
    //   param6 = type (0 = Boota, 1 = Ze Boota, 2 = Ba Boota)
    {0x0115, F_V4, 0x000041F000000000, "__BOOTA_FAMILY__"},

    // Dorphon. Params:
    //   param1 = TODO (value is param1 + 0.3)
    //   param2 = TODO (value is param2 + 0.3)
    //   param3 = TODO (value is param3 + 30, clamped below to 10)
    //   param4 = TODO (value is param4 + 0.3)
    //   param5 = TODO (value is param5 + 0.05)
    //   param6 = flags (bit field):
    //     0001 = always rare (Dorphon Eclair)
    // TODO: The values above make it look like param1-5 are the same as for
    // TObjEneDellBiter. Verify if this is the case.
    {0x0116, F_V4, 0x000041F000000000, "__DORPHON__"},

    // Goran / Pyro Goran / Goran Detonator. Same parameters as TObjEneBeast,
    // but also:
    //   param3 = TODO (see v58, v67)
    //   param6 = type (0 = Goran, 1 = Pyro Goran, 2 = Goran Detonator)
    {0x0117, F_V4, 0x00004E0000000000, "__GORAN_FAMILY__"},

    // Saint-Milion / Shambertin / Kondrieu. Params:
    //   param1 = TODO (see TObjEneV00b43ca0::set_params; seems it only matters
    //     if this is zero or not)
    //   param6 = flags (bit field):
    //     0001 = type (0 = Saint-Milion, 1 = Shambertin; ignored if enemy is
    //       set as rare by the server, in which case it's Kondrieu)
    {0x0119, F_V4, 0x0000100000000000, "__EPISODE_4_BOSS__"},
});

static string name_for_entity_type(
    unordered_multimap<uint16_t, const DATEntityDefinition*>& index,
    const vector<DATEntityDefinition>& defs,
    uint16_t type,
    Version version,
    uint8_t area) {

  if (index.size() == 0) {
    for (const auto& def : defs) {
      index.emplace(def.base_type, &def);
    }
  }

  auto its = index.equal_range(type);
  uint16_t version_mask = (1 << static_cast<size_t>(version));
  uint64_t area_mask = static_cast<uint64_t>(1ULL << area);

  if ((version != Version::UNKNOWN) && (area != 0xFF)) {
    for (auto [it, end_it] = its; it != end_it; it++) {
      const auto* def = it->second;
      if ((def->area_flags & area_mask) && (def->version_flags & version_mask)) {
        return def->name;
      }
    }
  }

  // When matching only by type or by (type, version), we can expect multiple
  // matches
  if (version != Version::UNKNOWN) {
    string ret;
    for (auto [it, end_it] = its; it != end_it; it++) {
      const auto* def = it->second;
      if (def->version_flags & version_mask) {
        if (!ret.empty()) {
          ret.push_back('/');
        }
        ret += def->name;
      }
    }
    if (!ret.empty()) {
      return ret;
    }
  }

  string ret;
  for (auto [it, end_it] = its; it != end_it; it++) {
    const auto* def = it->second;
    if (!ret.empty()) {
      ret.push_back('/');
    }
    ret += def->name;
  }

  return ret.empty()
      ? std::format("__UNKNOWN_ENTITY_{:04X}__", type)
      : ret;
}

string MapFile::name_for_object_type(uint16_t type, Version version, uint8_t area) {
  static unordered_multimap<uint16_t, const DATEntityDefinition*> index;
  return name_for_entity_type(index, dat_object_definitions, type, version, area);
}
string MapFile::name_for_enemy_type(uint16_t type, Version version, uint8_t area) {
  static unordered_multimap<uint16_t, const DATEntityDefinition*> index;
  return name_for_entity_type(index, dat_enemy_definitions, type, version, area);
}

string MapFile::ObjectSetEntry::str(Version version, uint8_t area) const {
  string name_str = MapFile::name_for_object_type(this->base_type, version, area);
  return std::format("[ObjectSetEntry type={:04X} \"{}\" floor={:04X} group={:04X} room={:04X} a3={:04X} x={:g} y={:g} z={:g} x_angle={:08X} y_angle={:08X} z_angle={:08X} params=[{:g} {:g} {:g} {:08X} {:08X} {:08X}]]",
      this->base_type,
      name_str,
      this->floor,
      this->group,
      this->room,
      this->unknown_a3,
      this->pos.x,
      this->pos.y,
      this->pos.z,
      this->angle.x,
      this->angle.y,
      this->angle.z,
      this->param1,
      this->param2,
      this->param3,
      this->param4,
      this->param5,
      this->param6);
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

string MapFile::EnemySetEntry::str(Version version, uint8_t area) const {
  auto type_name = MapFile::name_for_enemy_type(this->base_type, version, area);
  return std::format("[EnemySetEntry type={:04X} \"{}\" num_children={:04X} floor={:04X} room={:04X} wave_number={:04X} wave_number2={:04X} a1={:04X} x={:g} y={:g} z={:g} x_angle={:08X} y_angle={:08X} z_angle={:08X} params=[{:g} {:g} {:g} {:g} {:g} {:04X} {:04X}]]",
      this->base_type,
      type_name,
      this->num_children,
      this->floor,
      this->room,
      this->wave_number,
      this->wave_number2,
      this->unknown_a1,
      this->pos.x,
      this->pos.y,
      this->pos.z,
      this->angle.x,
      this->angle.y,
      this->angle.z,
      this->param1,
      this->param2,
      this->param3,
      this->param4,
      this->param5,
      this->param6,
      this->param7);
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

string MapFile::Event1Entry::str() const {
  return std::format("[Event1Entry event_id={:08X} flags={:04X} event_type={:04X} room={:04X} wave_number={:04X} delay={:08X} action_stream_offset={:08X}]",
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

string MapFile::Event2Entry::str() const {
  return std::format("[Event2Entry event_id={:08X} flags={:04X} event_type={:04X} room={:04X} wave_number={:04X} min_delay={:08X} max_delay={:08X} min_enemies={:02X} max_enemies={:02X} max_waves={:04X} action_stream_offset={:08X}]",
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

string MapFile::RandomEnemyLocationEntry::str() const {
  return std::format("[RandomEnemyLocationEntry x={:g} y={:g} z={:g} x_angle={:08X} y_angle={:08X} z_angle={:08X}a9={:04X} a10={:04X}]",
      this->pos.x,
      this->pos.y,
      this->pos.z,
      this->angle.x,
      this->angle.y,
      this->angle.z,
      this->unknown_a9,
      this->unknown_a10);
}

string MapFile::RandomEnemyDefinition::str() const {
  return std::format("[RandomEnemyDefinition params=[{:g} {:g} {:g} {:g} {:g} {:04X} {:04X}] entry_num={:08X} min_children={:04X} max_children={:04X}]",
      this->param1,
      this->param2,
      this->param3,
      this->param4,
      this->param5,
      this->param6,
      this->param7,
      this->entry_num,
      this->min_children,
      this->max_children);
}

string MapFile::RandomEnemyWeight::str() const {
  return std::format("[RandomEnemyWeight base_type_index={:02X} def_entry_num={:02X} weight={:02X} a4={:02X}]",
      this->base_type_index,
      this->def_entry_num,
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
  return max<size_t>(floorf((max_f * det_f) / UINT32_MAX_AS_FLOAT), min_v);
}

uint32_t MapFile::RandomState::next_location_index() {
  if (this->location_indexes_used < this->location_indexes_populated) {
    return this->location_index_table.at(this->location_indexes_used++);
  }
  return 0;
}

void MapFile::RandomState::generate_shuffled_location_table(
    const RandomEnemyLocationsHeader& header, phosg::StringReader r, uint16_t room) {
  if (header.num_rooms == 0) {
    throw runtime_error("no locations defined");
  }

  phosg::StringReader rooms_r = r.sub(header.room_table_offset, header.num_rooms * sizeof(RandomEnemyLocationSection));

  size_t bs_min = 0;
  size_t bs_max = header.num_rooms - 1;
  do {
    size_t bs_mid = (bs_min + bs_max) / 2;
    if (rooms_r.pget<RandomEnemyLocationSection>(bs_mid * sizeof(RandomEnemyLocationSection)).room < room) {
      bs_min = bs_mid + 1;
    } else {
      bs_max = bs_mid;
    }
  } while (bs_min < bs_max);

  const auto& sec = rooms_r.pget<RandomEnemyLocationSection>(bs_min * sizeof(RandomEnemyLocationSection));
  if (room != sec.room) {
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

MapFile::MapFile(std::shared_ptr<const std::string> data) {
  this->quest_data = data;
  this->link_data(data);

  phosg::StringReader r(data->data(), data->size());
  while (!r.eof()) {
    const auto& header = r.get<SectionHeader>();

    if (header.type() == SectionHeader::Type::END && header.section_size == 0) {
      break;
    }
    if (header.section_size < sizeof(header)) {
      throw runtime_error(std::format("quest entities list has invalid section header at offset 0x{:X}", r.where() - sizeof(header)));
    }

    if (header.floor >= this->sections_for_floor.size()) {
      throw runtime_error("section floor number too large");
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
        throw runtime_error("invalid section type");
    }
  }
  this->compute_floor_start_indexes();
}

MapFile::MapFile(
    uint8_t floor,
    std::shared_ptr<const std::string> objects_data,
    std::shared_ptr<const std::string> enemies_data,
    std::shared_ptr<const std::string> events_data) {
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

MapFile::MapFile(uint32_t generated_with_random_seed)
    : generated_with_random_seed(generated_with_random_seed) {}

void MapFile::link_data(std::shared_ptr<const string> data) {
  if (this->linked_data.emplace(data).second) {
    this->linked_data_hash ^= phosg::fnv1a64(*data);
  }
}

void MapFile::set_object_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.object_sets) {
    throw runtime_error("multiple object sets sections for same floor");
  }
  if (size % sizeof(ObjectSetEntry)) {
    throw runtime_error("object sets section size is not a multiple of entry size");
  }
  floor_sections.object_sets = reinterpret_cast<const ObjectSetEntry*>(data);
  floor_sections.object_set_count = size / sizeof(ObjectSetEntry);
  floor_sections.object_sets_file_offset = file_offset;
  floor_sections.object_sets_file_size = size;
}

void MapFile::set_enemy_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.enemy_sets) {
    throw runtime_error("multiple enemy sets sections for same floor");
  }
  if (floor_sections.events2 || floor_sections.random_enemy_locations_data || floor_sections.random_enemy_definitions_data) {
    throw runtime_error("floor already has random enemies and cannot also have fixed enemies");
  }
  if (size % sizeof(EnemySetEntry)) {
    throw runtime_error("enemy sets section size is not a multiple of entry size");
  }
  floor_sections.enemy_sets = reinterpret_cast<const EnemySetEntry*>(data);
  floor_sections.enemy_set_count = size / sizeof(EnemySetEntry);
  floor_sections.enemy_sets_file_offset = file_offset;
  floor_sections.enemy_sets_file_size = size;
}

void MapFile::set_events_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size, bool allow_evt2) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.events_data || floor_sections.events1 || floor_sections.events2 || floor_sections.event_action_stream) {
    throw runtime_error("multiple events sections for same floor");
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
      throw runtime_error("random events cannot be used in this context");
    }
    if (floor_sections.enemy_sets) {
      throw runtime_error("floor already has fixed enemies and cannot also have random enemies");
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
    throw runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_locations_data = data;
  floor_sections.random_enemy_locations_data_size = size;
  floor_sections.random_enemy_locations_file_offset = file_offset;
  floor_sections.random_enemy_locations_file_size = size;
  this->has_any_random_sections = true;
}

void MapFile::set_random_enemy_definitions_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.random_enemy_definitions_data) {
    throw runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_definitions_data = data;
  floor_sections.random_enemy_definitions_data_size = size;
  floor_sections.random_enemy_definitions_file_offset = file_offset;
  floor_sections.random_enemy_definitions_file_size = size;
  this->has_any_random_sections = true;
}

std::shared_ptr<const MapFile> MapFile::materialize_random_sections(uint32_t random_seed) const {
  return const_cast<MapFile*>(this)->materialize_random_sections(random_seed);
}

std::shared_ptr<MapFile> MapFile::materialize_random_sections(uint32_t random_seed) {
  if (!this->has_any_random_sections) {
    return this->shared_from_this();
  }

  static const array<uint32_t, 41> rand_enemy_base_types = {
      0x44, 0x43, 0x41, 0x42, 0x40, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x80,
      0x81, 0x82, 0x83, 0x84, 0x85, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
      0xA7, 0xA8, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD,
      0xDE, 0xDF, 0xE0, 0xE0, 0xE1};

  auto new_map = make_shared<MapFile>(random_seed);
  RandomState random_state(random_seed);

  for (uint8_t floor = 0; floor < 0x12; floor++) {
    const auto& this_sf = this->sections_for_floor[floor];

    if (this_sf.object_sets) {
      new_map->set_object_sets_for_floor(floor, 0, this_sf.object_sets, this_sf.object_set_count * sizeof(ObjectSetEntry));
    }

    if (this_sf.enemy_sets) {
      new_map->set_enemy_sets_for_floor(floor, 0, this_sf.enemy_sets, this_sf.enemy_set_count * sizeof(EnemySetEntry));
    }

    if (this_sf.events1) {
      new_map->set_events_for_floor(floor, 0, this_sf.events_data, this_sf.events_data_size, false);
    } else if (this_sf.events2) {
      if (!this_sf.random_enemy_locations_data || !this_sf.random_enemy_definitions_data) {
        throw runtime_error("cannot materialize random enemies; evt2 section present but one or both random data sections are missing");
      }
      if (!this_sf.event_action_stream) {
        throw runtime_error("cannot materialize random enemies; action stream is missing");
      }

      phosg::StringReader locations_sec_r(
          this_sf.random_enemy_locations_data, this_sf.random_enemy_locations_data_size);
      phosg::StringReader definitions_sec_r(
          this_sf.random_enemy_definitions_data, this_sf.random_enemy_definitions_data_size);
      const auto& locations_header = locations_sec_r.get<RandomEnemyLocationsHeader>();
      const auto& definitions_header = definitions_sec_r.get<RandomEnemyDefinitionsHeader>();
      auto definitions_r = definitions_sec_r.sub(
          definitions_header.entries_offset,
          definitions_header.entry_count * sizeof(RandomEnemyDefinition));
      auto weights_r = definitions_sec_r.sub(
          definitions_header.weight_entries_offset,
          definitions_header.weight_entry_count * sizeof(RandomEnemyWeight));

      phosg::StringWriter enemy_sets_w;
      phosg::StringWriter events1_w;
      phosg::StringWriter action_stream_w;
      action_stream_w.write(this_sf.event_action_stream, this_sf.event_action_stream_bytes);

      for (size_t source_event_index = 0; source_event_index < this_sf.event_count; source_event_index++) {
        const auto& source_event2 = this_sf.events2[source_event_index];

        size_t remaining_waves = random_state.rand_int_biased(1, source_event2.max_waves);
        // Trace: at 0080E125 EAX is wave count

        le_uint32_t wave_next_event_id = source_event2.event_id;
        uint32_t wave_number = source_event2.wave_number;
        while (remaining_waves) {
          remaining_waves--;

          size_t remaining_enemies = random_state.rand_int_biased(source_event2.min_enemies, source_event2.max_enemies);
          // Trace: at 0080E208 EDI is enemy count

          random_state.generate_shuffled_location_table(locations_header, locations_sec_r, source_event2.room);
          // Trace: at 0080EBB0 *(EBP + 4) points to table (0x20 uint32_ts)

          while (remaining_enemies) {
            remaining_enemies--;

            // TODO: Factor this sum out of the loops
            weights_r.go(0);
            size_t weight_total = 0;
            while (!weights_r.eof()) {
              weight_total += weights_r.get<RandomEnemyWeight>().weight;
            }
            // Trace: at 0080E2C2 EBX is weight_total

            size_t det = random_state.rand_int_biased(0, weight_total - 1);
            // Trace: at 0080E300 EDX is det

            weights_r.go(0);
            while (!weights_r.eof()) {
              const auto& weight_entry = weights_r.get<RandomEnemyWeight>();
              if (det < weight_entry.weight) {
                if ((weight_entry.base_type_index != 0xFF) && (weight_entry.def_entry_num != 0xFF)) {
                  EnemySetEntry e;
                  e.base_type = rand_enemy_base_types.at(weight_entry.base_type_index);
                  e.wave_number = wave_number;
                  e.room = source_event2.room;
                  e.floor = floor;

                  size_t bs_min = 0;
                  size_t bs_max = definitions_header.entry_count - 1;
                  if (bs_max == 0) {
                    throw runtime_error("no available random enemy definitions");
                  }
                  do {
                    size_t bs_mid = (bs_min + bs_max) / 2;
                    if (definitions_r.pget<RandomEnemyDefinition>(bs_mid * sizeof(RandomEnemyDefinition)).entry_num < weight_entry.def_entry_num) {
                      bs_min = bs_mid + 1;
                    } else {
                      bs_max = bs_mid;
                    }
                  } while (bs_min < bs_max);

                  const auto& def = definitions_r.pget<RandomEnemyDefinition>(bs_min * sizeof(RandomEnemyDefinition));
                  if (def.entry_num == weight_entry.def_entry_num) {
                    e.param1 = def.param1;
                    e.param2 = def.param2;
                    e.param3 = def.param3;
                    e.param4 = def.param4;
                    e.param5 = def.param5;
                    e.param6 = def.param6;
                    e.param7 = def.param7;
                    e.num_children = random_state.rand_int_biased(def.min_children, def.max_children);
                  } else {
                    throw runtime_error("random enemy definition not found");
                  }

                  const auto& loc = locations_sec_r.pget<RandomEnemyLocationEntry>(
                      locations_header.entries_offset + sizeof(RandomEnemyLocationEntry) * random_state.next_location_index());
                  e.pos = loc.pos;
                  e.angle = loc.angle;

                  // Trace: at 0080E6FE CX is base_type
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

      auto enemy_sets_sec_data = make_shared<string>(std::move(enemy_sets_w.str()));
      new_map->link_data(enemy_sets_sec_data);
      new_map->set_enemy_sets_for_floor(floor, 0, enemy_sets_sec_data->data(), enemy_sets_sec_data->size());

      auto events1_sec_data = make_shared<string>(std::move(events1_sec_w.str()));
      new_map->link_data(events1_sec_data);
      new_map->set_events_for_floor(floor, 0, events1_sec_data->data(), events1_sec_data->size(), false);
    }
  }

  // Add everything in this->linked_data to the new map's linked_data, since it
  // likely is referenced by pointers in sections_for_floor
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

string MapFile::disassemble_action_stream(const void* data, size_t size) {
  deque<string> ret;
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

string MapFile::disassemble(bool reassembly, Version version) const {
  deque<string> ret;
  for (uint8_t floor = 0; floor < this->sections_for_floor.size(); floor++) {
    const auto& sf = this->sections_for_floor[floor];
    phosg::StringReader as_r(sf.event_action_stream, sf.event_action_stream_bytes);

    if (sf.object_sets) {
      if (reassembly) {
        ret.emplace_back(std::format(".object_sets {}", floor));
      } else {
        ret.emplace_back(std::format(".object_sets {} /* 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.object_sets_file_offset, sf.object_sets_file_size));
      }
      for (size_t z = 0; z < sf.object_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.object_sets[z].str(version));
        } else {
          size_t k_id = z + sf.first_object_set_index;
          ret.emplace_back(std::format("/* K-{:03X} */ ", k_id) + sf.object_sets[z].str(version));
        }
      }
    }
    if (sf.enemy_sets) {
      if (reassembly) {
        ret.emplace_back(std::format(".enemy_sets {}", floor));
      } else {
        ret.emplace_back(std::format(".enemy_sets {} /* 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.enemy_sets_file_offset, sf.enemy_sets_file_size));
      }
      for (size_t z = 0; z < sf.enemy_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.enemy_sets[z].str(version));
        } else {
          size_t s_id = z + sf.first_enemy_set_index;
          ret.emplace_back(std::format("/* S-{:03X} */ ", s_id) + sf.enemy_sets[z].str(version));
        }
      }
    }
    if (sf.events1) {
      if (reassembly) {
        ret.emplace_back(std::format(".events {}", floor));
      } else {
        ret.emplace_back(std::format(".events {} /* 0x{:X} in file; 0x{:X} bytes; 0x{:X} bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events1[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          size_t w_id = z + sf.first_event_set_index;
          ret.emplace_back(std::format("/* W-{:03X} */ ", w_id) + ev.str());
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(std::format(
              "  // WARNING: Event action stream offset (0x{:X}) is outside of this section",
              ev.action_stream_offset));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.events2) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_events {}", floor));
      } else {
        ret.emplace_back(std::format(
            ".random_events {} /* 0x{:X} in file; 0x{:X} bytes; 0x{:X} bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events2[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          ret.emplace_back(std::format("/* index {} */", z) + ev.str());
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(std::format(
              "  // WARNING: Event action stream offset (0x{:X}) is outside of this section",
              ev.action_stream_offset));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.random_enemy_locations_data) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_enemy_locations {}", floor));
      } else {
        ret.emplace_back(std::format(".random_enemy_locations {} /* 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.random_enemy_locations_file_offset, sf.random_enemy_locations_file_size));
      }
      ret.emplace_back(phosg::format_data(sf.random_enemy_locations_data, sf.random_enemy_locations_data_size));
    }
    if (sf.random_enemy_definitions_data) {
      if (reassembly) {
        ret.emplace_back(std::format(".random_enemy_definitions {}", floor));
      } else {
        ret.emplace_back(std::format(".random_enemy_definitions {} /* 0x{:X} in file; 0x{:X} bytes */",
            floor, sf.random_enemy_definitions_file_offset, sf.random_enemy_definitions_file_size));
      }
      ret.emplace_back(phosg::format_data(sf.random_enemy_definitions_data, sf.random_enemy_definitions_data_size));
    }
  }
  return phosg::join(ret, "\n");
}

////////////////////////////////////////////////////////////////////////////////
// Super map

string SuperMap::Object::id_str() const {
  return std::format("KS-{:02X}-{:03X}", this->floor, this->super_id);
}

string SuperMap::Object::str() const {
  string ret = "[Object " + this->id_str();
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_object_index != 0xFFFF) {
      string args_str = def.set_entry->str(v);
      ret += std::format(
          " {}:[{:04X} => {}]", phosg::name_for_enum(v), def.relative_object_index, args_str);
    }
  }
  ret += "]";
  return ret;
}

string SuperMap::Enemy::id_str() const {
  return std::format("ES-{:02X}-{:03X}-{:03X}", this->floor, this->super_set_id, this->super_id);
}

string SuperMap::Enemy::str() const {
  string ret = std::format("[Enemy ES-{:02X}-{:03X}-{:03X} type={} child_index={:X} alias_enemy_index_delta={:X} is_default_rare_v123={} is_default_rare_bb={}",
      this->floor,
      this->super_set_id,
      this->super_id,
      phosg::name_for_enum(this->type),
      this->child_index,
      this->alias_enemy_index_delta,
      this->is_default_rare_v123 ? "true" : "false",
      this->is_default_rare_bb ? "true" : "false");
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_enemy_index != 0xFFFF) {
      string args_str = def.set_entry->str(v);
      ret += std::format(
          " {}:[{:04X}/{:04X} => {}]",
          phosg::name_for_enum(v),
          def.relative_set_index,
          def.relative_enemy_index,
          args_str);
    }
  }
  ret += "]";
  return ret;
}

string SuperMap::Event::id_str() const {
  return std::format("WS-{:02X}-{:03X}", this->floor, this->super_id);
}

string SuperMap::Event::str() const {
  string ret = "[Event " + this->id_str();
  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_event_index != 0xFFFF) {
      string action_stream_str = phosg::format_data_string(def.action_stream, def.action_stream_size);
      string args_str = def.set_entry->str();
      ret += std::format(
          " {}:[{:04X} => {}+{}]",
          phosg::name_for_enum(v),
          def.relative_event_index,
          args_str,
          action_stream_str);
    }
  }
  ret += "]";
  return ret;
}

SuperMap::SuperMap(Episode episode, const std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>& map_files)
    : log("[SuperMap] "),
      episode(episode) {
  for (const auto& map_file : map_files) {
    if (!map_file) {
      continue;
    }
    if (map_file->has_random_sections()) {
      throw logic_error("supermap cannot be constructed from map files that contain random sections");
    }

    if (map_file->random_seed() >= 0) {
      if (this->random_seed < 0) {
        this->random_seed = map_file->random_seed();
      } else if (this->random_seed != map_file->random_seed()) {
        throw logic_error("supermap cannot be constructed from map files with different random seeds");
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

shared_ptr<SuperMap::Object> SuperMap::add_object(
    Version version,
    uint8_t floor,
    const MapFile::ObjectSetEntry* set_entry) {
  auto obj = make_shared<Object>();
  obj->super_id = this->objects.size();
  obj->floor = floor;

  this->objects.emplace_back(obj);
  this->link_object_version(obj, version, set_entry);

  return obj;
}

void SuperMap::link_object_version(std::shared_ptr<Object> obj, Version version, const MapFile::ObjectSetEntry* set_entry) {
  // Add to version's entities list and set the object's per-version info
  auto& entities = this->version(version);
  auto& obj_ver = obj->version(version);
  if (obj_ver.set_entry) {
    throw logic_error("object already linked to version");
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
      num_switch_flags = (4 - clamp<size_t>(set_entry->param5, 0, 4));
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

shared_ptr<SuperMap::Enemy> SuperMap::add_enemy_and_children(
    Version version, uint8_t floor, const MapFile::EnemySetEntry* set_entry) {

  shared_ptr<Enemy> head_ene = nullptr;
  size_t next_child_index = 0;
  auto add = [&](EnemyType type,
                 bool is_default_rare_v123 = false,
                 bool is_default_rare_bb = false,
                 int16_t alias_enemy_index_delta = 0) -> void {
    auto& entities = this->version(version);

    // TODO: It'd be nice to share some code between this function and
    // link_enemy_version_and_children

    // Create enemy
    auto ene = make_shared<Enemy>();
    ene->super_id = this->enemies.size();
    ene->child_index = next_child_index++;
    ene->super_set_id = this->enemy_sets.size() - (ene->child_index != 0);
    ene->floor = floor;
    ene->type = type;
    ene->is_default_rare_v123 = is_default_rare_v123;
    ene->is_default_rare_bb = is_default_rare_bb;
    ene->alias_enemy_index_delta = alias_enemy_index_delta;
    auto& ene_ver = ene->version(version);
    ene_ver.set_entry = set_entry;
    ene_ver.relative_enemy_index = entities.enemies.size();
    // If child_index > 0, then the head enemy was already created, so we need
    // to subtract 1 from the set index because this new enemy should have the
    // same set index as the head enemy, but the head enemy was already added
    // to the enemy sets list.
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
  };

  // The following logic was originally based on the public version of
  // Tethealla, created by Sodaboy. I've augmented it with findings from my own
  // research.

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
    case 0x0100: // Unknown NPC
      // All of these have a default child count of zero
      add(EnemyType::NON_ENEMY_NPC);
      break;
    case 0x0040: { // TObjEneMoja
      bool is_rare = (set_entry->param6 >= 1);
      add(EnemyType::HILDEBEAR, is_rare, is_rare);
      break;
    }
    case 0x0041: { // TObjEneLappy
      bool is_rare_v123 = (set_entry->param6 != 0);
      bool is_rare_bb = (set_entry->param6 & 1);
      switch (this->episode) {
        case Episode::EP1:
        case Episode::EP2:
          add(EnemyType::RAG_RAPPY, is_rare_v123, is_rare_bb);
          break;
        case Episode::EP4:
          add((floor > 0x05) ? EnemyType::SAND_RAPPY_DESERT : EnemyType::SAND_RAPPY_CRATER, is_rare_v123, is_rare_bb);
          break;
        default:
          throw logic_error("invalid episode");
      }
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
      add(types[clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0060: // TObjGrass
      add(EnemyType::GRASS_ASSASSIN);
      break;
    case 0x0061: // TObjEneRe2Flower
      add(((episode == Episode::EP2) && (floor == 0x11)) ? EnemyType::DEL_LILY : EnemyType::POISON_LILY);
      break;
    case 0x0062: // TObjEneNanoDrago
      add(EnemyType::NANO_DRAGON);
      break;
    case 0x0063: { // TObjEneShark
      static const EnemyType types[3] = {EnemyType::EVIL_SHARK, EnemyType::PAL_SHARK, EnemyType::GUIL_SHARK};
      add(types[clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0064: { // TObjEneSlime
      // Unlike all other versions, BB doesn't have a way to force slimes to be
      // rare via constructor args
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
      add(types[clamp<int16_t>(set_entry->param6, 0, 2)]);
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
    case 0x00C0: // TBoss1Dragon or TBoss5Gryphon
      if (episode == Episode::EP1) {
        add(EnemyType::DRAGON);
      } else if (episode == Episode::EP2) {
        add(EnemyType::GAL_GRYPHON);
      } else {
        throw runtime_error("DRAGON placed outside of Episode 1 or 2");
      }
      break;
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
    case 0x00C8: // TBoss4DarkFalz
      if ((set_entry->num_children != 0) && (set_entry->num_children != 0x200)) {
        this->log.warning_f("DARK_FALZ has an unusual num_children (0x{:X})", set_entry->num_children);
      }
      add(EnemyType::DARK_FALZ_3);
      default_num_children = -1; // Skip adding children (because we do it here)
      for (size_t x = 0; x < 0x1FD; x++) {
        add(EnemyType::DARVANT);
      }
      add(EnemyType::DARK_FALZ_3, false, false, -0x1FE);
      add(EnemyType::DARK_FALZ_2, false, false, -0x1FF);
      add(EnemyType::DARK_FALZ_1, false, false, -0x200);
      break;
    case 0x00CA: // TBoss6PlotFalz
      add(EnemyType::OLGA_FLOW_2);
      default_num_children = -1; // Skip adding children (because we do it here)
      for (size_t x = 0; x < 0x200; x++) {
        add(EnemyType::OLGA_FLOW_2, false, false, -(x + 1));
      }
      break;
    case 0x00CB: // TBoss7DeRolLeC
      add(EnemyType::BARBA_RAY);
      child_type = EnemyType::PIG_RAY;
      default_num_children = 0x2F;
      break;
    case 0x00CC: // TBoss8Dragon
      add(EnemyType::GOL_DRAGON);
      default_num_children = 5;
      break;
    case 0x00D4: // TObjEneMe3StelthReal
      if (this->episode == Episode::EP3) {
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add((set_entry->param6 > 0) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL);
        default_num_children = 4;
      }
      break;
    case 0x00D5: // TObjEneMerillLia
      if (this->episode == Episode::EP3) {
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add((set_entry->param6 > 0) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      }
      break;
    case 0x00D6: // TObjEneBm9Mericarol
      if (this->episode == Episode::EP3) {
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
      if (this->episode == Episode::EP3) {
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
    case 0x00E0: // TObjEneMe3SinowZoaReal or TObjEneEpsilonBody
      if ((episode == Episode::EP2) && (floor > 0x0F)) {
        add(EnemyType::EPSILON);
        default_num_children = 4;
        child_type = EnemyType::EPSIGARD;
      } else {
        add((set_entry->param6 > 0) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
      }
      break;
    case 0x00E1: // TObjEneIllGill
      add(EnemyType::ILL_GILL);
      break;
    case 0x0110:
      if (this->episode == Episode::EP3) {
        add(EnemyType::NON_ENEMY_NPC);
      } else {
        add(EnemyType::ASTARK);
      }
      break;
    case 0x0111:
      if (this->episode == Episode::EP3) {
        add(EnemyType::NON_ENEMY_NPC);
      } else if (floor > 0x05) {
        add(set_entry->param2 ? EnemyType::YOWIE_DESERT : EnemyType::SATELLITE_LIZARD_DESERT);
      } else {
        add(set_entry->param2 ? EnemyType::YOWIE_CRATER : EnemyType::SATELLITE_LIZARD_CRATER);
      }
      break;
    case 0x0112:
      if (this->episode == Episode::EP3) {
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
      add((floor > 0x05) ? EnemyType::ZU_DESERT : EnemyType::ZU_CRATER, is_rare, is_rare);
      break;
    }
    case 0x0115: {
      static const EnemyType types[3] = {EnemyType::BOOTA, EnemyType::ZE_BOOTA, EnemyType::BA_BOOTA};
      add(types[clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0116: {
      bool is_rare = (set_entry->param6 & 1);
      add(EnemyType::DORPHON, is_rare, is_rare);
      break;
    }
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      add(types[clamp<int16_t>(set_entry->param6, 0, 2)]);
      break;
    }
    case 0x0119:
      // There isn't a way to create Kondrieu via constructor args
      add((set_entry->param6 & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILION);
      default_num_children = 0x18;
      break;

    case 0x00C3: // TBoss3VoloptP01
    case 0x00C4: // TBoss3VoloptCore or subclass
    case 0x00C6: // TBoss3VoloptMonitor
    case 0x00C7: // TBoss3VoloptHiraisin
    case 0x0118:
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
    throw logic_error("no enemy was created");
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
      throw logic_error("enemy already linked to version");
    }

    ene_ver.set_entry = set_entry;
    ene_ver.relative_enemy_index = entities.enemies.size();
    // If child_index > 0, then the head enemy was already created, so we need
    // to subtract 1 from the set index because this new enemy should have the
    // same set index as the head enemy, but the head enemy was already added
    // to the enemy sets list.
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
    } catch (const out_of_range&) {
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
  auto ev = make_shared<Event>();
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
    string s = entry->str();
    throw runtime_error(std::format(
        "action stream offset 0x{:X} is beyond end of action stream (0x{:X}) for event {}",
        entry->action_stream_offset, map_file_action_stream_size, s));
  }
  const void* ev_action_stream_start = reinterpret_cast<const uint8_t*>(map_file_action_stream) +
      entry->action_stream_offset;
  size_t ev_action_stream_size = get_action_stream_size(
      ev_action_stream_start, map_file_action_stream_size - entry->action_stream_offset);

  auto& entities = this->version(version);
  auto& ev_ver = ev->version(version);
  if (ev_ver.set_entry) {
    throw logic_error("event already linked to version");
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

// This is a modified version of a simple dynamic programming edit distance
// algorithm. Conceptually, the previous map file's entries go down the left
// side of the matrix, and the current map file's entries go across the top.
// To save time, we run it once per floor, since we never expect objects on
// different floors in different versions to logically be the same object.

enum class EditAction {
  STOP = 0, // Reverse path ends here (at end, this should only be at (0, 0))
  ADD, // Reverse path goes left
  EDIT, // Reverse path goes up-left
  DELETE, // Reverse path goes up
};

template <typename EntityT, typename Score1, typename Score2>
vector<EditAction> compute_edit_path(
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

  // Along the top and left edges, there is only one possible reverse path, so
  // fill those in first
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
  vector<EditAction> reverse_path;
  size_t x = curr_count;
  size_t y = prev_count;
  while (x > 0 || y > 0) {
    EditAction action = mtx.at(x, y).action;
    reverse_path.emplace_back(action);
    switch (action) {
      case EditAction::STOP:
        mtx.print(stderr); // TODO: delete this when no longer needed
        throw logic_error("STOP action left after edit distance computation");
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
vector<shared_ptr<EntityT>> compute_prev_entities(
    const vector<shared_ptr<EntityT>>& existing_prev_entities,
    size_t prev_entities_offset,
    const vector<EditAction>& edit_path) {
  vector<shared_ptr<EntityT>> ret;
  for (auto action : edit_path) {
    switch (action) {
      case EditAction::ADD:
        // This object doesn't match any object from the previous version
        ret.emplace_back(nullptr);
        break;
      case EditAction::DELETE:
        // There is an object in the previous version that doesn't match any in this version; skip it
        prev_entities_offset++;
        break;
      case EditAction::EDIT: {
        // The current object in this_sf matches the current object in prev_sf; link them together
        ret.emplace_back(existing_prev_entities.at(prev_entities_offset));
        prev_entities_offset++;
        break;
      }
      default:
        throw logic_error("invalid edit path action");
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
  // Group or room changes are pretty bad, but small variances in position
  // and params are tolerated
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
  // A change of type or num_children is not tolerated and should never be
  // better than an add + delete
  if ((prev.base_type != current.base_type) || (prev.num_children != current.num_children)) {
    return 500.0;
  }
  // Room or wave_number changes are pretty bad, but small variances in
  // position and params are tolerated
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

void SuperMap::add_map_file(Version this_v, shared_ptr<const MapFile> this_map_file) {
  auto& this_entities = this->version(this_v);
  if (this_entities.map_file) {
    throw logic_error("a map file is already present for this version");
  }
  this_entities.map_file = this_map_file;

  for (uint8_t floor = 0; floor < 0x12; floor++) {
    const auto& fc = this_map_file->floor(floor);
    if (fc.events2 || fc.random_enemy_locations_data || fc.random_enemy_definitions_data) {
      throw logic_error("cannot add map file with random segments to a supermap");
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
                                    const vector<shared_ptr<EntityT>>& prev_entities,
                                    size_t prev_entities_start_index,
                                    auto&& link_existing,
                                    auto&& add_new,
                                    const unordered_map<uint64_t, vector<shared_ptr<EntityT>>>& semantic_hash_index) {
      auto edit_path = compute_edit_path(
          prev_sets, prev_set_count, this_sets, this_set_count, add_cost, delete_cost, edit_cost);

      auto used_prev_entities = compute_prev_entities(prev_entities, prev_entities_start_index, edit_path);
      if (used_prev_entities.size() != this_set_count) {
        throw std::logic_error("incorrect previous entity list length");
      }
      unordered_set<const EntityT*> used_prev_entities_set;
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
          } catch (const out_of_range&) {
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
      // All objects were added in this version, or there was no previous
      // version. Add all the objects as new objects.
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
          bind(&SuperMap::link_object_version, this, placeholders::_1, placeholders::_2, placeholders::_3),
          bind(&SuperMap::add_object, this, placeholders::_1, placeholders::_2, placeholders::_3),
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
          bind(&SuperMap::link_enemy_version_and_children, this, placeholders::_1, placeholders::_2, placeholders::_3),
          bind(&SuperMap::add_enemy_and_children, this, placeholders::_1, placeholders::_2, placeholders::_3),
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
          bind(&SuperMap::link_event_version, this, placeholders::_1, placeholders::_2, placeholders::_3, this_sf.event_action_stream, this_sf.event_action_stream_bytes),
          bind(&SuperMap::add_event, this, placeholders::_1, placeholders::_2, placeholders::_3, this_sf.event_action_stream, this_sf.event_action_stream_bytes),
          this->events_for_semantic_hash);
    }
  }
}

vector<shared_ptr<const SuperMap::Object>> SuperMap::objects_for_floor_room_group(
    Version version, uint8_t floor, uint16_t room, uint16_t group) const {
  const auto& entities = this->version(version);
  uint64_t k = room_index_key(floor, room, group);
  vector<shared_ptr<const Object>> ret;
  for (auto its = entities.object_for_floor_room_and_group.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

vector<shared_ptr<const SuperMap::Object>> SuperMap::doors_for_switch_flag(
    Version version, uint8_t floor, uint8_t switch_flag) const {
  vector<shared_ptr<const Object>> ret;
  const auto& entities = this->version(version);
  for (auto its = entities.door_for_floor_and_switch_flag.equal_range((floor << 8) | switch_flag);
      its.first != its.second;
      its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

shared_ptr<const SuperMap::Enemy> SuperMap::enemy_for_index(Version version, uint16_t enemy_id, bool follow_alias) const {
  const auto& entities = this->version(version);

  if (entities.enemies.empty()) {
    throw out_of_range("no enemies defined");
  }
  if (enemy_id >= entities.enemies.size()) {
    throw out_of_range("enemy ID out of range");
  }
  auto& enemy = entities.enemies[enemy_id];
  if (follow_alias && (enemy->alias_enemy_index_delta != 0)) {
    uint16_t target_id = enemy_id + enemy->alias_enemy_index_delta;
    if (target_id >= entities.enemies.size()) {
      throw out_of_range("aliased enemy ID out of range");
    }
    return entities.enemies[target_id];
  } else {
    return enemy;
  }
}

shared_ptr<const SuperMap::Enemy> SuperMap::enemy_for_floor_type(Version version, uint8_t floor, EnemyType type) const {
  const auto& entities = this->version(version);

  if (entities.enemies.empty()) {
    throw out_of_range("no enemies defined");
  }
  // TODO: Linear search is bad here. Do something better, like binary search
  // for the floor start and just linear search through the floor enemies.
  for (auto& ene : entities.enemies) {
    if ((ene->floor == floor) && (ene->type == type)) {
      return ene;
    }
  }
  throw out_of_range("enemy not found");
}

vector<shared_ptr<const SuperMap::Enemy>> SuperMap::enemies_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const {
  const auto& entities = this->version(version);

  uint64_t k = room_index_key(floor, room, wave_number);
  vector<shared_ptr<const Enemy>> ret;
  for (auto its = entities.enemy_for_floor_room_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

vector<shared_ptr<const SuperMap::Event>> SuperMap::events_for_id(Version version, uint8_t floor, uint32_t event_id) const {
  const auto& entities = this->version(version);
  uint64_t k = (static_cast<uint64_t>(floor) << 32) | event_id;
  vector<shared_ptr<const Event>> ret;
  for (auto its = entities.event_for_floor_and_event_id.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

vector<shared_ptr<const SuperMap::Event>> SuperMap::events_for_floor(Version version, uint8_t floor) const {
  const auto& entities = this->version(version);
  uint64_t k_start = (static_cast<uint64_t>(floor) << 32);
  uint64_t k_end = (static_cast<uint64_t>(floor + 1) << 32);
  vector<shared_ptr<const Event>> ret;
  for (auto it = entities.event_for_floor_and_event_id.lower_bound(k_start);
      (it != entities.event_for_floor_and_event_id.end()) && (it->first < k_end);
      it++) {
    ret.emplace_back(it->second);
  }
  return ret;
}

vector<shared_ptr<const SuperMap::Event>> SuperMap::events_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const {
  const auto& entities = this->version(version);
  uint64_t k = room_index_key(floor, room, wave_number);
  vector<shared_ptr<const Event>> ret;
  for (auto its = entities.event_for_floor_room_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
  return ret;
}

unordered_map<EnemyType, size_t> SuperMap::count_enemy_sets_for_version(Version version) const {
  unordered_map<EnemyType, size_t> ret;
  for (const auto& ene : this->version(version).enemy_sets) {
    try {
      ret.at(ene->type) += 1;
    } catch (const out_of_range&) {
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
      throw logic_error("object super_id is incorrect");
    }
  }

  {
    size_t super_set_id = static_cast<size_t>(-1);
    size_t prev_child_index = 0;
    for (size_t super_id = 0; super_id < this->enemies.size(); super_id++) {
      const auto& ene = this->enemies[super_id];
      if (ene->super_id != super_id) {
        throw logic_error("enemy super_id is incorrect");
      }
      if (ene->child_index == 0) {
        super_set_id++;
        prev_child_index = 0;
      } else {
        if (ene->child_index != ++prev_child_index) {
          throw logic_error("enemy child indexes out of order");
        }
      }
      if (ene->super_set_id != super_set_id) {
        throw logic_error(std::format(
            "enemy super_set_id is incorrect; expected S-{:03X}, received S-{:03X}",
            super_set_id, ene->super_set_id));
      }
    }
    if (super_set_id != this->enemy_sets.size() - 1) {
      throw logic_error(std::format(
          "not all enemy sets are in the enemies list; ended with 0x{:X}, expected 0x{:X}",
          super_set_id, this->enemy_sets.size()));
    }
  }
  for (size_t super_id = 0; super_id < this->events.size(); super_id++) {
    if (this->events[super_id]->super_id != super_id) {
      throw logic_error("event super_id is incorrect");
    }
  }

  for (Version v : ALL_NON_PATCH_VERSIONS) {
    const auto& entities = this->version(v);

    if (entities.object_floor_start_indexes.at(0) != 0) {
      throw logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.enemy_floor_start_indexes.at(0) != 0) {
      throw logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.enemy_set_floor_start_indexes.at(0) != 0) {
      throw logic_error("object floor start index for floor 0 is incorrect");
    }
    if (entities.event_floor_start_indexes.at(0) != 0) {
      throw logic_error("object floor start index for floor 0 is incorrect");
    }

    uint8_t floor = 0;
    for (size_t object_index = 0; object_index < entities.objects.size(); object_index++) {
      const auto& obj = entities.objects[object_index];
      if (obj->floor < floor) {
        throw logic_error("objects out of floor order");
      }
      while (floor < obj->floor) {
        floor++;
        if (entities.object_floor_start_indexes.at(floor) != object_index) {
          throw logic_error("object floor start index is incorrect");
        }
      }
      const auto& obj_ver = obj->version(v);
      if (!obj_ver.set_entry) {
        throw logic_error("object set entry is missing");
      }
      if (obj_ver.relative_object_index != object_index) {
        throw logic_error("object relative index is incorrect");
      }
    }
    while (floor < 0x12) {
      floor++;
      if ((floor < 0x12) && (entities.object_floor_start_indexes.at(floor) != entities.objects.size())) {
        throw logic_error("object floor start index is incorrect");
      }
    }

    floor = 0;
    size_t enemy_set_index = static_cast<size_t>(-1);
    for (size_t enemy_index = 0; enemy_index < entities.enemies.size(); enemy_index++) {
      const auto& ene = entities.enemies[enemy_index];
      if (ene->child_index == 0) {
        enemy_set_index++;
        if (entities.enemy_sets.at(enemy_set_index) != ene) {
          throw logic_error("enemy set does not match expected enemy");
        }
      }
      if (ene->floor < floor) {
        throw logic_error("enemies out of floor order");
      }
      while (floor < ene->floor) {
        floor++;
        if (entities.enemy_floor_start_indexes.at(floor) != enemy_index) {
          throw logic_error("enemy floor start index is incorrect");
        }
        if (entities.enemy_set_floor_start_indexes.at(floor) != enemy_set_index) {
          throw logic_error("enemy set floor start index is incorrect");
        }
      }
      const auto& ene_ver = ene->version(v);
      if (!ene_ver.set_entry) {
        throw logic_error("enemy set entry is missing");
      }
      if (ene_ver.relative_enemy_index != enemy_index) {
        throw logic_error("enemy relative index is incorrect");
      }
      if (ene_ver.relative_set_index != enemy_set_index) {
        throw logic_error("enemy relative set index is incorrect");
      }
    }
    if (enemy_set_index != entities.enemy_sets.size() - 1) {
      throw logic_error("not all enemy sets were checked");
    }
    while (floor < 0x12) {
      floor++;
      if (floor < 0x12) {
        if (entities.enemy_floor_start_indexes.at(floor) != entities.enemies.size()) {
          throw logic_error("enemy floor start index is incorrect");
        }
        if (entities.enemy_set_floor_start_indexes.at(floor) != entities.enemy_sets.size()) {
          throw logic_error("enemy set floor start index is incorrect");
        }
      }
    }

    floor = 0;
    for (size_t event_index = 0; event_index < entities.events.size(); event_index++) {
      const auto& ev = entities.events[event_index];
      if (ev->floor < floor) {
        throw logic_error("events out of floor order");
      }
      while (floor < ev->floor) {
        floor++;
        if (entities.event_floor_start_indexes.at(floor) != event_index) {
          throw logic_error("event floor start index is incorrect");
        }
      }
      const auto& ev_ver = ev->version(v);
      if (!ev_ver.set_entry) {
        throw logic_error("event entry is missing");
      }
      if (ev_ver.relative_event_index != event_index) {
        throw logic_error("event relative index is incorrect");
      }
    }
    while (floor < 0x12) {
      floor++;
      if ((floor < 0x12) && (entities.event_floor_start_indexes.at(floor) != entities.events.size())) {
        throw logic_error("event floor start index is incorrect");
      }
    }
  }
}

void SuperMap::print(FILE* stream) const {
  phosg::fwrite_fmt(stream, "SuperMap {} random={:08X}\n", name_for_episode(this->episode), this->random_seed);

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

////////////////////////////////////////////////////////////////////////////////
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

string MapState::RareEnemyRates::str() const {
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

const shared_ptr<const MapState::RareEnemyRates> MapState::NO_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(0, 0, 0);
const shared_ptr<const MapState::RareEnemyRates> MapState::DEFAULT_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(
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
    : log(std::format("[MapState(free):{:08X}] ", lobby_or_session_id), lobby_log.min_level),
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

void MapState::index_super_map(const FloorConfig& fc, shared_ptr<RandomGenerator> rand_crypt) {
  if (!fc.super_map) {
    throw logic_error("cannot index floor config with no map definition");
  }

  for (const auto& obj : fc.super_map->all_objects()) {
    auto& obj_st = this->object_states.emplace_back(make_shared<ObjectState>());
    obj_st->k_id = this->object_states.size() - 1;
    obj_st->super_obj = obj;
  }

  for (const auto& ene : fc.super_map->all_enemies()) {
    auto& ene_st = this->enemy_states.emplace_back(make_shared<EnemyState>());
    if (ene->alias_enemy_index_delta) {
      ene_st->alias_ene_st = this->enemy_states.at((this->enemy_states.size() - 1) + ene->alias_enemy_index_delta);
      if (ene_st->alias_ene_st->alias_ene_st) {
        throw std::runtime_error("target for enemy state alias is itself an alias");
      }
    }
    if (ene->child_index == 0) {
      this->enemy_set_states.emplace_back(ene_st);
    }
    ene_st->e_id = this->enemy_states.size() - 1;
    ene_st->set_id = this->enemy_set_states.size() - 1;
    ene_st->super_ene = ene;

    // Handle random rare enemies and difficulty-based effects
    EnemyType type;
    switch (ene->type) {
      case EnemyType::DARK_FALZ_3:
        type = ((this->difficulty == Difficulty::NORMAL) && (ene->alias_enemy_index_delta == 0))
            ? EnemyType::DARK_FALZ_2
            : EnemyType::DARK_FALZ_3;
        break;
      case EnemyType::DARVANT:
        type = (this->difficulty == Difficulty::ULTIMATE) ? EnemyType::DARVANT_ULTIMATE : EnemyType::DARVANT;
        break;
      default:
        type = ene->type;
    }

    auto rare_type = type_definition_for_enemy(type).rare_type(fc.super_map->get_episode(), this->event, ene->floor);
    if ((type == EnemyType::MERICARAND) || (rare_type != type)) {
      unordered_map<uint32_t, float> det_cache;
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

        // On BB, rare enemy indexes are generated by the server and sent to
        // the client, so we can use any method we want to choose rares. On
        // other versions, we must match the client's logic, even though it's
        // more computationally expensive.
        if (!is_v4(v)) {
          uint32_t seed = this->random_seed + 0x1000 + enemy_index;
          float det;
          try {
            det = det_cache.at(seed);
          } catch (const out_of_range&) {
            // TODO: We only need the first value from this crypt, so it's
            // unfortunate that we have to initialize the entire thing. Find
            // a way to make this faster.
            PSOV2Encryption crypt(seed);
            det = (static_cast<float>((crypt.next() >> 16) & 0xFFFF) / 65536.0f);
            det_cache.emplace(seed, det);
          }

          if (type == EnemyType::MERICARAND) {
            // On v3, Mericarols that have param6 > 2 are randomized to be
            // Mericus, Merikle, or Mericarol, but the former two are not
            // considered rare. (We use rare_flags anyway to distinguish them
            // from Mericarol.)
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
    auto& ev_st = this->event_states.emplace_back(make_shared<EventState>());
    ev_st->w_id = this->event_states.size() - 1;
    ev_st->super_ev = ev;
  }
}

void MapState::compute_dynamic_object_base_indexes() {
  this->dynamic_obj_base_k_id = this->object_states.size();

  // Compute the maximum object ID for each version. We can't just use the last
  // object because that object may not exist on all versions, and we can't
  // just look at the last floor, because that floor may be empty on some
  // versions.
  this->dynamic_obj_base_index_for_version.fill(0);
  for (const auto& fc : this->floor_config_entries) {
    for (Version v : ALL_NON_PATCH_VERSIONS) {
      if (fc.super_map) {
        auto& last_obj_index = this->dynamic_obj_base_index_for_version[static_cast<size_t>(v)];
        size_t base_index = fc.base_indexes_for_version(v).base_object_index;
        last_obj_index = max<size_t>(base_index + fc.super_map->version(v).objects.size(), last_obj_index);
      }
    }
  }
}

uint16_t MapState::index_for_object_state(Version version, shared_ptr<const ObjectState> obj_st) const {
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

uint16_t MapState::index_for_enemy_state(Version version, shared_ptr<const EnemyState> ene_st) const {
  uint16_t relative_index = ene_st->super_ene->version(version).relative_enemy_index;
  return (relative_index == 0xFFFF)
      ? 0xFFFF
      : (relative_index + this->floor_config(ene_st->super_ene->floor).base_indexes_for_version(version).base_enemy_index);
}

uint16_t MapState::set_index_for_enemy_state(Version version, shared_ptr<const EnemyState> ene_st) const {
  uint16_t relative_set_index = ene_st->super_ene->version(version).relative_set_index;
  return (relative_set_index == 0xFFFF)
      ? 0xFFFF
      : (relative_set_index + this->floor_config(ene_st->super_ene->floor).base_indexes_for_version(version).base_enemy_set_index);
}

uint16_t MapState::index_for_event_state(Version version, shared_ptr<const EventState> ev_st) const {
  uint16_t relative_index = ev_st->super_ev->version(version).relative_event_index;
  return (relative_index == 0xFFFF)
      ? 0xFFFF
      : (relative_index + this->floor_config(ev_st->super_ev->floor).base_indexes_for_version(version).base_event_index);
}

shared_ptr<MapState::ObjectState> MapState::object_state_for_index(Version version, uint8_t floor, uint16_t object_index) {
  size_t dynamic_obj_base_index = this->dynamic_obj_base_index_for_version.at(static_cast<size_t>(version));
  if (object_index < dynamic_obj_base_index) {
    const auto& fc = this->floor_config(floor);
    size_t base_object_index = fc.base_indexes_for_version(version).base_object_index;
    if (object_index < base_object_index) {
      throw runtime_error("object is not on the specified floor");
    }
    if (!fc.super_map) {
      throw out_of_range("there are no objects on the specified floor");
    }
    const auto& obj = fc.super_map->version(version).objects.at(object_index - base_object_index);
    return this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);

  } else {
    size_t k_id_delta = object_index - dynamic_obj_base_index;
    auto obj_st = make_shared<ObjectState>();
    obj_st->k_id = this->dynamic_obj_base_k_id + k_id_delta;
    obj_st->super_obj = nullptr;
    return obj_st;
  }
}

vector<shared_ptr<MapState::ObjectState>> MapState::object_states_for_floor_room_group(
    Version version, uint8_t floor, uint16_t room, uint16_t group) {
  vector<shared_ptr<ObjectState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& obj : fc.super_map->objects_for_floor_room_group(version, floor, room, group)) {
      ret.emplace_back(this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id));
    }
  }
  return ret;
}

vector<shared_ptr<MapState::ObjectState>> MapState::door_states_for_switch_flag(
    Version version, uint8_t floor, uint8_t switch_flag) {
  vector<shared_ptr<ObjectState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& obj : fc.super_map->doors_for_switch_flag(version, floor, switch_flag)) {
      ret.emplace_back(this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id));
    }
  }
  return ret;
}

shared_ptr<MapState::EnemyState> MapState::enemy_state_for_index(Version version, uint8_t floor, uint16_t enemy_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_enemy_index = fc.base_indexes_for_version(version).base_enemy_index;
  if (enemy_index < base_enemy_index) {
    throw runtime_error("enemy is not on the specified floor");
  }
  if (!fc.super_map) {
    throw out_of_range("there are no enemies on the specified floor");
  }
  const auto& ene = fc.super_map->version(version).enemies.at(enemy_index - base_enemy_index);
  return this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
}

shared_ptr<MapState::EnemyState> MapState::enemy_state_for_set_index(Version version, uint8_t floor, uint16_t enemy_set_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_enemy_set_index = fc.base_indexes_for_version(version).base_enemy_set_index;
  if (enemy_set_index < base_enemy_set_index) {
    throw runtime_error("enemy is not on the specified floor");
  }
  if (!fc.super_map) {
    throw out_of_range("there are no enemies on the specified floor");
  }
  const auto& ene = fc.super_map->version(version).enemies.at(enemy_set_index - base_enemy_set_index);
  return this->enemy_states.at(fc.base_super_ids.base_enemy_set_index + ene->super_set_id);
}

shared_ptr<MapState::EnemyState> MapState::enemy_state_for_floor_type(Version version, uint8_t floor, EnemyType type) {
  vector<shared_ptr<EnemyState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    const auto& ene = fc.super_map->enemy_for_floor_type(version, floor, type);
    return this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
  }
  throw out_of_range("map definition missing for floor");
}

vector<shared_ptr<MapState::EnemyState>> MapState::enemy_states_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) {
  vector<shared_ptr<EnemyState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ene : fc.super_map->enemies_for_floor_room_wave(version, floor, room, wave_number)) {
      ret.emplace_back(this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id));
    }
  }
  return ret;
}

shared_ptr<MapState::EventState> MapState::event_state_for_index(Version version, uint8_t floor, uint16_t event_index) {
  const auto& fc = this->floor_config(floor);
  size_t base_event_index = fc.base_indexes_for_version(version).base_event_index;
  if (event_index < base_event_index) {
    throw runtime_error("event is not on the specified floor");
  }
  if (!fc.super_map) {
    throw out_of_range("there are no events on the specified floor");
  }
  const auto& ev = fc.super_map->version(version).events.at(event_index - base_event_index);
  return this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id);
}

vector<shared_ptr<MapState::EventState>> MapState::event_states_for_id(Version version, uint8_t floor, uint32_t event_id) {
  vector<shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_id(version, floor, event_id)) {
      ret.emplace_back(this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id));
    }
  }
  return ret;
}

vector<shared_ptr<MapState::EventState>> MapState::event_states_for_floor(Version version, uint8_t floor) {
  vector<shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_floor(version, floor)) {
      ret.emplace_back(this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id));
    }
  }
  return ret;
}

vector<shared_ptr<MapState::EventState>> MapState::event_states_for_floor_room_wave(
    Version version, uint8_t floor, uint16_t room, uint16_t wave_number) {
  vector<shared_ptr<EventState>> ret;
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
      throw logic_error("floor config has incorrect base object index");
    }
    const auto& entities = fc.super_map->version(from_version);
    size_t fc_end_object_index = base_indexes.base_object_index + entities.objects.size();
    if (fc_end_object_index > entry_count) {
      // DC NTE sometimes has fewer objects than the map, but only by 1.
      // TODO: Figure out why this happens.
      if (from_version == Version::DC_NTE) {
        fc_end_object_index = entry_count;
      } else {
        throw runtime_error(std::format(
            "the map has more objects (at least 0x{:X}) than the client has (0x{:X})",
            fc_end_object_index, entry_count));
      }
    }
    for (; object_index < fc_end_object_index; object_index++) {
      const auto& entry = entries[object_index];
      const auto& obj = entities.objects.at(object_index - base_indexes.base_object_index);
      auto& obj_st = this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
      if (obj_st->super_obj != obj) {
        throw logic_error("super object link is incorrect");
      }
      if (obj_st->game_flags != entry.flags) {
        this->log.warning_f("({:04X} => K-{:03X}) Game flags from client ({:04X}) do not match game flags from map ({:04X})",
            object_index, obj_st->k_id, entry.flags, obj_st->game_flags);
        obj_st->game_flags = entry.flags;
      }
    }
  }
  if (object_index < entry_count) {
    throw runtime_error(std::format("the client has more objects (0x{:X}) than the map has (0x{:X})",
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
      throw logic_error("floor config has incorrect base enemy index");
    }
    const auto& entities = fc.super_map->version(from_version);
    size_t fc_end_enemy_index = base_indexes.base_enemy_index + entities.enemies.size();
    if (fc_end_enemy_index > entry_count) {
      throw runtime_error(std::format("the map has more enemies than the client has (0x{:X})", entry_count));
    }
    for (; enemy_index < min<size_t>(fc_end_enemy_index, entry_count); enemy_index++) {
      const auto& entry = entries[enemy_index];
      const auto& ene = entities.enemies.at(enemy_index - base_indexes.base_enemy_index);
      auto& ene_st = this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
      if (ene_st->super_ene != ene) {
        throw logic_error("super enemy link is incorrect");
      }
      if (ene_st->game_flags != entry.flags) {
        this->log.warning_f("({:04X} => E-{:03X}) Flags from client ({:08X}) do not match game flags from map ({:08X})",
            enemy_index, ene_st->e_id, entry.flags, ene_st->game_flags);
        ene_st->game_flags = entry.flags;
      }
      if (ene_st->total_damage != entry.total_damage) {
        this->log.warning_f("({:04X} => E-{:03X}) Total damage from client ({}) does not match total damage from map ({})",
            enemy_index, ene_st->e_id, entry.total_damage, ene_st->total_damage);
        ene_st->total_damage = entry.total_damage;
      }
    }
  }
  if (enemy_index < entry_count) {
    throw runtime_error(std::format("the client has more enemies (0x{:X}) than the map has (0x{:X})",
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
        throw logic_error("floor config has incorrect base object index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_object_index = base_indexes.base_object_index + entities.objects.size();
      if (fc_end_object_index > object_set_flags_count) {
        // DC NTE sometimes has fewer objects than the map, but only by 1.
        // TODO: Figure out why this happens.
        if (from_version == Version::DC_NTE) {
          fc_end_object_index = object_set_flags_count;
        } else {
          throw runtime_error(std::format(
              "the map has more objects (at least 0x{:X}) than the client has (0x{:X})",
              fc_end_object_index, object_set_flags_count));
        }
      }
      for (; object_index < min<size_t>(fc_end_object_index, object_set_flags_count); object_index++) {
        uint16_t set_flags = object_set_flags[object_index];
        const auto& obj = entities.objects.at(object_index - base_indexes.base_object_index);
        auto& obj_st = this->object_states.at(fc.base_super_ids.base_object_index + obj->super_id);
        if (obj_st->super_obj != obj) {
          throw logic_error("super object link is incorrect");
        }
        if (obj_st->set_flags != set_flags) {
          this->log.warning_f("({:04X} => K-{:03X}) Set flags from client ({:04X}) do not match set flags from map ({:04X})",
              object_index, obj_st->k_id, set_flags, obj_st->set_flags);
          obj_st->set_flags = set_flags;
        }
      }
    }
    if (object_index < object_set_flags_count) {
      throw runtime_error(std::format("the client has more objects (0x{:X}) than the map has (0x{:X})",
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
        throw logic_error("floor config has incorrect base enemy index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_enemy_set_index = base_indexes.base_enemy_set_index + entities.enemy_sets.size();
      if (fc_end_enemy_set_index > enemy_set_flags_count) {
        throw runtime_error("the map has more enemy sets than the client has");
      }
      for (; enemy_set_index < min<size_t>(fc_end_enemy_set_index, enemy_set_flags_count); enemy_set_index++) {
        uint16_t set_flags = enemy_set_flags[enemy_set_index];
        const auto& ene = entities.enemy_sets.at(enemy_set_index - base_indexes.base_enemy_set_index);
        auto& ene_st = this->enemy_set_states.at(fc.base_super_ids.base_enemy_set_index + ene->super_set_id);
        if (ene_st->super_ene != ene) {
          throw logic_error("super enemy link is incorrect");
        }
        if (ene_st->set_flags != set_flags) {
          this->log.warning_f("({:04X} => E-{:03X}) Set flags from client ({:04X}) do not match set flags from map ({:04X})",
              enemy_set_index, ene_st->e_id, set_flags, ene_st->set_flags);
          ene_st->set_flags = set_flags;
        }
      }
    }
    if (enemy_set_index < enemy_set_flags_count) {
      throw runtime_error("the client has more enemy sets than the map has");
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
        throw logic_error("floor config has incorrect base event index");
      }
      const auto& entities = fc.super_map->version(from_version);
      size_t fc_end_event_index = base_indexes.base_event_index + entities.events.size();
      if (fc_end_event_index > event_flags_count) {
        throw runtime_error("the map has more events than the client has");
      }
      for (; event_index < min<size_t>(fc_end_event_index, event_flags_count); event_index++) {
        uint16_t flags = event_flags[event_index];
        const auto& ev = entities.events.at(event_index - base_indexes.base_event_index);
        auto& ev_st = this->event_states.at(fc.base_super_ids.base_event_index + ev->super_id);
        if (ev_st->flags != flags) {
          this->log.warning_f("({:04X} => W-{:03X}) Set flags from client ({:04X}) do not match flags from map ({:04X})",
              event_index, ev_st->w_id, flags, ev_st->flags);
          ev_st->flags = flags;
        }
      }
    }
    if (event_index < event_flags_count) {
      throw runtime_error("the client has more events than the map has");
    }
  }
}

void MapState::verify() const {
  try {
    size_t total_object_count = 0;
    size_t total_enemy_count = 0;
    size_t total_enemy_set_count = 0;
    size_t total_event_count = 0;
    unordered_set<shared_ptr<const SuperMap>> verified_super_maps;
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
      throw logic_error(std::format(
          "map state object count (0x{:X}) does not match supermap object count (0x{:X})",
          this->object_states.size(), total_object_count));
    }
    if (this->enemy_states.size() != total_enemy_count) {
      throw logic_error(std::format(
          "map state enemy count (0x{:X}) does not match supermap enemy count (0x{:X})",
          this->enemy_states.size(), total_enemy_count));
    }
    if (this->enemy_set_states.size() != total_enemy_set_count) {
      throw logic_error(std::format(
          "map state enemy set count (0x{:X}) does not match supermap enemy set count (0x{:X})",
          this->enemy_set_states.size(), total_enemy_set_count));
    }
    if (this->event_states.size() != total_event_count) {
      throw logic_error(std::format(
          "map state event count (0x{:X}) does not match supermap event count (0x{:X})",
          this->event_states.size(), total_event_count));
    }

    for (size_t k_id = 0; k_id < this->object_states.size(); k_id++) {
      const auto& obj_st = this->object_states[k_id];
      if (obj_st->k_id != k_id) {
        throw logic_error("mismatched object state k_id");
      }
      const auto& fc = this->floor_config(obj_st->super_obj->floor);
      if (fc.base_super_ids.base_object_index + obj_st->super_obj->super_id != k_id) {
        throw logic_error("mismatched object state super_id");
      }
    }
    for (size_t e_id = 0; e_id < this->enemy_states.size(); e_id++) {
      const auto& ene_st = this->enemy_states[e_id];
      if (ene_st->e_id != e_id) {
        throw logic_error("mismatched enemy state e_id");
      }
      const auto& fc = this->floor_config(ene_st->super_ene->floor);
      if (fc.base_super_ids.base_enemy_index + ene_st->super_ene->super_id != e_id) {
        throw logic_error("mismatched enemy state super_id");
      }
    }
    for (size_t set_id = 0; set_id < this->enemy_set_states.size(); set_id++) {
      const auto& ene_st = this->enemy_set_states[set_id];
      if (ene_st->set_id != set_id) {
        throw logic_error("mismatched enemy set state set_id");
      }
      const auto& fc = this->floor_config(ene_st->super_ene->floor);
      if (fc.base_super_ids.base_enemy_set_index + ene_st->super_ene->super_set_id != set_id) {
        throw logic_error("mismatched enemy set state super_set_id");
      }
    }
    for (size_t w_id = 0; w_id < this->event_states.size(); w_id++) {
      const auto& ev_st = this->event_states[w_id];
      if (ev_st->w_id != w_id) {
        throw logic_error("mismatched event state w_id");
      }
      const auto& fc = this->floor_config(ev_st->super_ev->floor);
      if (fc.base_super_ids.base_event_index + ev_st->super_ev->super_id != w_id) {
        throw logic_error("mismatched event state super_id");
      }
    }

    for (Version v : ALL_NON_PATCH_VERSIONS) {
      FloorConfig::EntityBaseIndexes base_indexes;
      for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
        const auto& fc = this->floor_config_entries[floor];
        const auto& fc_base_indexes = fc.base_indexes_for_version(v);
        if (base_indexes.base_object_index != fc_base_indexes.base_object_index) {
          throw logic_error("base object index does not match expected value");
        }
        if (base_indexes.base_enemy_index != fc_base_indexes.base_enemy_index) {
          throw logic_error("base enemy index does not match expected value");
        }
        if (base_indexes.base_enemy_set_index != fc_base_indexes.base_enemy_set_index) {
          throw logic_error("base enemy set index does not match expected value");
        }
        if (base_indexes.base_event_index != fc_base_indexes.base_event_index) {
          throw logic_error("base event set index does not match expected value");
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

    unordered_set<size_t> remaining_bb_rare_indexes;
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
        throw logic_error(std::format("BB random rare enemy index {:04X} not present in indexes set", enemy_index));
      }
    }
    if (!remaining_bb_rare_indexes.empty()) {
      vector<string> indexes;
      for (uint16_t index : remaining_bb_rare_indexes) {
        indexes.emplace_back(std::format("{:04X}", index));
      }
      throw logic_error("not all BB random rare enemies were accounted for; remaining: " + phosg::join(indexes, ", "));
    }
  } catch (const exception&) {
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
    string obj_str = obj_st->super_obj->str();
    phosg::fwrite_fmt(stream, " {} game_flags={:04X} set_flags={:04X} item_drop_checked={}\n",
        obj_str, obj_st->game_flags, obj_st->set_flags, obj_st->item_drop_checked ? "true" : "false");
  }

  phosg::fwrite_fmt(stream, "Enemies:\n");
  phosg::fwrite_fmt(stream, "  FL ENEID ALIAS DCTE----- DCPR----- DCV1----- DCV2----- PCTE----- PCV2----- GCTE----- GCV3----- EP3TE---- GCEP3---- XBV3----- BBV4----- ENEMY\n");
  for (const auto& ene_st : this->enemy_states) {
    std::string alias_str = ene_st->super_ene->alias_enemy_index_delta
        ? std::format("E-{:03X}", ene_st->e_id + ene_st->super_ene->alias_enemy_index_delta)
        : "-----";
    phosg::fwrite_fmt(stream, "  {:02X} E-{:03X} {}", ene_st->super_ene->floor, ene_st->e_id, alias_str);
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
    string ene_str = ene_st->super_ene->str();
    phosg::fwrite_fmt(stream, " {} total_damage={:04X} rare_flags={:04X} game_flags={:08X} set_flags={:04X} server_flags={:04X}\n",
        ene_str, ene_st->total_damage, ene_st->rare_flags, ene_st->game_flags, ene_st->set_flags, ene_st->server_flags);
  }

  if (this->bb_rare_enemy_indexes.empty()) {
    phosg::fwrite_fmt(stream, "BB rare enemy indexes: (none)\n");
  } else {
    string s;
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
    string ev_str = ev_st->super_ev->str();
    phosg::fwrite_fmt(stream, " {} set_flags={:04X}\n", ev_str, ev_st->flags);
  }
}

static constexpr uint64_t room_layout_key(uint8_t area, uint8_t major_var, uint32_t room_id) {
  return ((static_cast<uint64_t>(area) << 40) |
      (static_cast<uint64_t>(major_var) << 32) |
      static_cast<uint64_t>(room_id));
}

RoomLayoutIndex::RoomLayoutIndex(const phosg::JSON& json) {
  for (const auto& [area_and_major_var, rooms_json] : json.as_dict()) {
    auto tokens = phosg::split(area_and_major_var, '-');
    uint8_t area = stoul(tokens.at(0), nullptr, 16);
    uint8_t major_var = stoul(tokens.at(1), nullptr, 16);
    for (const auto& [room_id_str, room_json] : rooms_json->as_dict()) {
      uint32_t room_id = stoul(room_id_str, nullptr, 16);
      auto emplace_ret = this->rooms.emplace(room_layout_key(area, major_var, room_id), Room());
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

const RoomLayoutIndex::Room& RoomLayoutIndex::get_room(uint8_t area, uint8_t major_var, uint32_t room_id) const {
  return this->rooms.at(room_layout_key(area, major_var, room_id));
}
