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
    ret += phosg::string_printf("%02zX:[%" PRIX32 ",%" PRIX32 "]", z, e.layout.load(), e.entities.load());
  }
  return ret;
}

phosg::JSON Variations::json() const {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < this->entries.size(); z++) {
    const auto& e = this->entries[z];
    ret.emplace_back(phosg::JSON::dict({
        {"layout", e.layout.load()},
        {"entities", e.entities.load()},
    }));
  }
  return ret;
}

SetDataTableBase::SetDataTableBase(Version version) : version(version) {}

Variations SetDataTableBase::generate_variations(
    Episode episode, bool is_solo, shared_ptr<PSOLFGEncryption> opt_rand_crypt) const {
  Variations ret;
  for (size_t floor = 0; floor < ret.entries.size(); floor++) {
    auto& e = ret.entries[floor];
    auto num_vars = this->num_free_play_variations_for_floor(episode, is_solo, floor);
    e.layout = (num_vars.layout > 1) ? (random_from_optional_crypt(opt_rand_crypt) % num_vars.layout) : 0;
    e.entities = (num_vars.entities > 1) ? (random_from_optional_crypt(opt_rand_crypt) % num_vars.entities) : 0;
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

uint8_t SetDataTableBase::default_area_for_floor(Episode episode, uint8_t floor) const {
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
      const auto& areas = ((this->version == Version::GC_NTE) ? areas_ep2_gc_nte : areas_ep2);
      return areas.at(floor);
    }
    case Episode::EP4:
      return areas_ep4.at(floor);
    default:
      throw logic_error("incorrect episode");
  }
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
  lines.emplace_back(phosg::string_printf("FL/V1/V2 => ----------------------OBJECT -----------------ENEMY+EVENT -----------------------SETUP\n"));
  for (size_t a = 0; a < this->entries.size(); a++) {
    const auto& v1_v = this->entries[a];
    for (size_t v1 = 0; v1 < v1_v.size(); v1++) {
      const auto& v2_v = v1_v[v1];
      for (size_t v2 = 0; v2 < v2_v.size(); v2++) {
        const auto& e = v2_v[v2];
        lines.emplace_back(phosg::string_printf("%02zX/%02zX/%02zX => %28s %28s %28s\n",
            a, v1, v2, e.object_list_basename.c_str(), e.enemy_and_event_list_basename.c_str(), e.area_setup_filename.c_str()));
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

const char* MapFile::name_for_object_type(uint16_t type) {
  static const unordered_map<uint16_t, const char*> names({
      // Defines where a player should start when entering a floor. Params:
      //   param1 = client ID
      //   param4 = source type:
      //     0 = use this set when advancing from a lower floor
      //     1 = use this set when returning from a higher floor
      //     anything else = set is unused
      {0x0000, "TObjPlayerSet"},

      // Displays a particle effect. Params:
      //   param1 = particle type (truncated to int, clamped to nonnegative)
      //   param3 = TODO
      //   param4 = TODO
      //   param5 = TODO
      //   param6 = TODO
      {0x0001, "TObjParticle"},

      // Standard (triangular) cross-floor warp object. Params:
      //   param4 = destination floor
      //   param6 = color (0 = blue, 1 = red); if this is 0 in Challenge mode,
      //     the warp is destroyed immediately
      {0x0002, "TObjAreaWarpForest"},

      // Standard (triangular) intra-floor warp object. Params:
      //   param1-3 = destination coordinates (x, y, z); players are supposed to
      //     be offset from this location in different directions depending on
      //     their client ID, but there is a bug that causes all players to use
      //     the same offsets: x + 10 and z + 10
      //   param4 = destination angle (about y axis)
      {0x0003, "TObjMapWarpForest"},

      // Light collision. Params:
      //   param1 = TODO (in range 0-10000; if above 10000, (param1 - 10000) is
      //     used and a flag is set)
      //   param2 = TODO (in range 0-10000; if above 10000, (param2 - 10000) is
      //     used and a flag is set)
      //   param3 = TODO
      //   param4 = TODO
      //   param5 = TODO
      //   param6 = TODO
      {0x0004, "TObjLight"},

      // Arbitrary item. The parameters specify the item data; see
      // ItemCreator::base_item_for_specialized_box for how the encoding works.
      // Availability: v1/v2 only
      {0x0005, "TItem"},

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
      {0x0006, "TObjEnvSound"},

      // Fog collision object. Params:
      //   param1 = radius
      //   param4 = TODO
      //   param5 = TODO (only checked for zero vs. nonzero)
      {0x0007, "TObjFogCollision"},

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
      {0x0008, "TObjEvtCollision"},

      // TODO: Describe this object. Params:
      //   param1 = TODO
      //   param2 = TODO
      //   param3 = TODO (it only matters whether this is negative or not)
      {0x0009, "TObjCollision"},

      // Elemental trap. Params:
      //   param1 = trigger radius delta (actual radius is param1 / 2 + 30)
      //   param2 = explosion radius delta (actual radius is param2 / 2 + 60)
      //   param3 = trap group number:
      //     negative = trap triggers and explodes alone
      //     00 = trap follows player who triggered it (online only; when
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
      //     02 = cold (can freeze; damage = power * (100 - EIC) / 500)
      //       chance of freezing = ((((power - 250) / 40) + 5) / 40) clamped
      //       to [0, 0.4], or to [0.2, 0.4] on Ultimate
      //     03 = electric (can shock; damage = power * (100 - EIC) / 500)
      //       chance of shock = 1/15, or 1/40 on Ultimate
      //     04 = light (damage = power * (100 - ELT) / 500)
      //     05 = dark (instantly kills with chance (power - EDK) / 100); if
      //       used in a boss arena and in non-Ultimate mode, cannot kill
      //   param6 = number of frames between trigger and explosion
      {0x000A, "TOMineIcon01"},

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
      {0x000B, "TOMineIcon02"},

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
      {0x000C, "TOMineIcon03"},

      // Large elemental trap. Params:
      //   param1 = trigger radius delta (actual radius is param1 / 2 + 60)
      //   param2 = explosion radius delta (actual radius is param2 / 2 + 120)
      //   param3 = trap group number (same as in TOMineIcon01)
      //   param4 = trap power (same as in TOMineIcon01)
      //   param5 = trap type (same as in TOMineIcon01)
      //   param6 = number of frames between trigger and explosion
      {0x000D, "TOMineIcon04"},

      // Room ID. Params:
      //   param1 = radius (actual radius = (param1 * 10) + 30)
      //   param2 = next room ID
      //   param3 = previous room ID
      //   param5 = angle
      //   param6 = TODO (debug info calls this "BLOCK ID"; seems it only
      //     matters whether this is 0x10000 or not)
      {0x000E, "TObjRoomId"},

      // Sensor of some kind (TODO). Params:
      //   param1 = activation radius delta (actual radius = param1 + 50)
      //   param4 = switch flag number
      //   param5 = update mode switch (TODO; param5 < 0 sets update_mode =
      //     PERMANENTLY_ON, otherwise TEMPORARY; see TOSensor_vF)
      {0x000F, "TOSensorGeneral01"},

      // Lens flare effect. This object is not constructed in offline multi
      // mode. Params:
      //   param1 = visibility radius (if negative, visible everywhere)
      {0x0011, "TEF_LensFlare"},

      // Quest script trigger. Starts a quest thread at a specific label when
      // the local player goes near the object. Params:
      //   param1 = radius
      //   param4 = label number to call when local player is within radius
      {0x0012, "TObjQuestCol"},

      // Healing ring. Removes all negative status conditions and adds 9 HP and
      // 9 TP per frame until max HP/TP are both reached or the player leaves
      // the radius. The radius is a fixed size.
      {0x0013, "TOHealGeneral"},

      // Invisible map collision. Params:
      //   param1-3 = box dimensions (x, y, z; rotated by angle fields)
      //   param4 = wall type:
      //     00 = custom (see param5)
      //     01 = blocks enemies only (as if param5 = 00008000)
      //     02 = blocks enemies and players (as if param5 = 0x00008900)
      //     03 = blocks enemies and players, but enemies can see targets
      //       through the collision (as if param5 = 0x00000800)
      //     04 = blocks players only (as if param5 = 00002000)
      //     05 = undefined behavior due to missing bounds check
      //     anything else = same as 01
      //   param5 = flags (bit field; used if param4 = 0) (TODO: describe bits)
      {0x0014, "TObjMapCsn"},

      // Like TObjQuestCol, but requires the player to press a controller
      // button to trigger the call. Parameters are the same as for
      // TObjQuestCol.
      {0x0015, "TObjQuestColA"},

      // TODO: Describe this object. Params:
      //   param1 = radius (if negative, 30 is used)
      {0x0016, "TObjItemLight"},

      // Radar collision. Params:
      //   param1 = radius
      //   param4 = TODO
      {0x0017, "TObjRaderCol"},

      // Fog collision. Params:
      //   param1 = radius
      //   param3 = if >= 1, fog is on when switch flag is on; otherwise, fog
      //     is on when switch flag is off
      //   param4 = fog entry number (TODO: the client handles this value being
      //     >= 0x1000 by subtracting 0x1000; what is this for?)
      //   param5 = transition type (0 = fade in, 1 = instantly switch)
      //   param6 = switch flag number
      {0x0018, "TObjFogCollisionSwitch"},

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
      {0x0019, "TObjWarpBossMulti(off)/TObjWarpBoss(on)"},

      // Sign board. This shows the loaded image from a quest (via load_pvr).
      // Params:
      //   param1-3 = scale factors (x, y, z)
      {0x001A, "TObjSinBoard"},

      // Quest floor warp. This appears similar to TObjAreaWarpForest except
      // that the object is not destroyed immediately if it's blue and the game
      // is Challenge mode. Params:
      //   param1 = player set ID (TODO: what exactly does this do? Looks like it
      //     does nothing unless it's >= 2)
      //   param4 = destination floor
      //   param6 = color (0 = blue, 1 = red)
      {0x001B, "TObjAreaWarpQuest"},

      // Ending movie warp (used in final boss arenas after the boss is
      // defeated). Params:
      //   param6 = color (0 = blue, 1 = red)
      {0x001C, "TObjAreaWarpEnding"},

      // Star light effect.
      // This object renders from -100 to 740 over x and -100 to 580 over y.
      // Params:
      //   param1 = TODO
      //   param2 = TODO
      //   param3 = TODO
      //   param4 = TODO
      //   param5 = TODO
      //   param6 = TODO
      {0x001D, "TEffStarLight2D_Base"},

      // VR Temple Beta / Barba Ray lens flare effect.
      // This object renders from -10 to 650 over x and -10 to 490 over y.
      {0x001E, "__LENS_FLARE__"},

      // Hides the area map when the player is near this object. Params:
      //   param1 = radius
      // TODO: Test this.
      {0x001F, "TObjRaderHideCol"},

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
      {0x0020, "TOSwitchItem"},

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
      {0x0021, "TOSymbolchatColli"},

      // Invisible collision switch. Params:
      //   param1 = radius delta (actual radius is param1 + 10)
      //   param4 = switch flag number
      //   param5 = sticky flag (if negative, switch flag is unset when player
      //     leaves; if zero or positive, switch flag remains on)
      {0x0022, "TOKeyCol"},

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
      //   param5 = if in the range [100, 999], uses the default free play
      //     script instead of the loaded quest? (TODO: verify this; see
      //     TOAttackableCol_on_attack for usage)
      //   param6 = quest label to call when required number of hits is taken
      //     (if zero, switch flag in param3 is set instead)
      {0x0023, "TOAttackableCol"},

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
      {0x0024, "TOSwitchAttack"},

      // Switch flag timer. This object watches for a switch flag to be
      // activated, then once it is, waits for a specified time, then disables
      // that switch flag and activates up to three other switch flags. Note
      // that this object just provides the timer functionality; the trigger
      // switch flag must be activated by some other object. Params:
      //   angle.x = trigger mode:
      //     0 = disable watched switch flag when timer expires
      //     any other value = enable up to 3 other switch flags when timer
      //       expires and don't disable watched switch flag
      //   angle.y = if this is 1, play tick sound effect every second after
      //     activation (if any other value, no tick sound is played)
      //   angle.z = timer duration in frames
      //   param4 = switch flag to watch for activation in low 16 bits, switch
      //     flag 1 to activate when timer expires (if angle.x = 0) in high 16
      //     bits (>= 0x100 if not needed)
      //   param5 = switch flag 2 to activate when timer expires (if
      //     angle.x = 0) in high 16 bits (>= 0x100 if not needed)
      //   param6 = switch flag 3 to activate when timer expires (if
      //     angle.x = 0) in high 16 bits (>= 0x100 if not needed)
      {0x0025, "TOSwitchTimer"},

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
      {0x0026, "TOChatSensor"},

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
      {0x0027, "TObjRaderIcon"},

      // Environmental sound. This object is not constructed in offline multi
      // mode. This is essentially identical to TObjEnvSound, except the sound
      // fades in and out instead of abruptly starting or stopping when
      // entering or leaving its radius. Params:
      //   param2 = volume fade speed (units per frame)
      //   param3 = audibility radius (same as for TObjEnvSound)
      //   param4 = sound ID (same as for TObjEnvSound)
      //   param5 = volume (same as for TObjEnvSound)
      {0x0028, "TObjEnvSoundEx"},

      // Environmental sound. This object is not constructed in offline multi
      // mode. This is essentially identical to TObjEnvSound, except there is
      // no radius: the sound is audible everywhere. Params:
      //   param4 = sound ID (same as for TObjEnvSound)
      //   param5 = volume (same as for TObjEnvSound)
      {0x0029, "TObjEnvSoundGlobal"},

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
      //   Episode 3:
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
      {0x0040, "TShopGenerator"},

      // Telepipe city location. Params:
      //   param4 = owner client ID (0-3)
      {0x0041, "TObjLuker"},

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
      {0x0042, "TObjBgmCol"},

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
      {0x0043, "TObjCityMainWarp"},

      // Lobby teleporter. When used, this object immediately ends the current
      // game and sends the player back to the lobby. If constructed offline,
      // this object will do nothing and not render.
      // This object takes no parameters.
      {0x0044, "TObjCityAreaWarp"},

      // Warp to another location on the same map. Used for the Principal's
      // office warp. This warp is visible in all game modes, but cannot be
      // used in Battle or Challenge mode. Params:
      //   param1-3 = destination (same as for TObjMapWarpForest)
      //   param4 = destination angle (same as for TObjMapWarpForest)
      //   param6 = destination text (clamped to [0, 2]):
      //     00 = "The Principal"
      //     01 = "Pioneer 2"
      //     02 = "Lab"
      {0x0045, "TObjCityMapWarp"},

      // City doors. None of these take any parameters. Respectively:
      // - Door to the shop area
      // - Door to the Hunter's Guild area
      // - Door to the Ragol warp
      // - Door to the Medical Center
      {0x0046, "TObjCityDoor_Shop"},
      {0x0047, "TObjCityDoor_Guild"},
      {0x0048, "TObjCityDoor_Warp"},
      {0x0049, "TObjCityDoor_Med"},

      // TODO: Describe this object. There appear to be no parameters.
      {0x004A, "__ELEVATOR__"}, // Not named in the client

      // Holiday event decorations. There appear to be no parameters, except
      // TObjCity_Season_SonicAdv2, which takes param4 = model index (clamped
      // to [0, 3]).
      {0x004B, "TObjCity_Season_EasterEgg"},
      {0x004C, "TObjCity_Season_ValentineHeart"},
      {0x004D, "TObjCity_Season_XmasTree"},
      {0x004E, "TObjCity_Season_XmasWreath"},
      {0x004F, "TObjCity_Season_HalloweenPumpkin"},
      {0x0050, "TObjCity_Season_21_21"},
      {0x0051, "TObjCity_Season_SonicAdv2"},
      {0x0052, "TObjCity_Season_Board"},

      // Fireworks effect. Params:
      //   param1 = area width
      //   param2 = base height
      //   param3 = area depth
      //   param4 = launch frequency (when a firework is launched, the game
      //     generates a random number R in range [0, 0x7FFF] and waits
      //     ((param4 + 60) * (r / 0x8000) * 3.0)) frames before launching the
      //     next firework)
      {0x0053, "TObjCity_Season_FireWorkCtrl"},

      // Door that blocks the lobby teleporter in offline mode. There appear to
      // be no parameters.
      {0x0054, "TObjCityDoor_Lobby"},

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
      // Availability: v2+ only
      // TODO: This thing has a lot of code; figure out if there are any other
      // parameters
      {0x0055, "TObjCityMainWarpChallenge"},

      // Episode 2 Lab door. Params:
      //   param4 = switch flag and activation mode:
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
      // Availability: v3+ only
      {0x0056, "TODoorLabo"},

      // Enables the Trade Window when the player is near this object. Both
      // players must be near a TObjTradeCollision object (not necessarily the
      // same one) to be able to use the Trade Window with each other. Params:
      //   param1 = radius
      // Availability: v3+ only
      {0x0057, "TObjTradeCollision"},

      // TODO: Describe this object. Presumably similar to TObjTradeCollision
      // but enables the deck edit counter? Params:
      //   param1 = radius
      // Availability: Ep3 only
      {0x0058, "TObjDeckCollision"},

      // Forest door. Params:
      //   param4 = switch flag number (low byte) and number to appear on door
      //     (second-lowest byte, modulo 0x0A)
      //   param6 = TODO (expected to be 0 or 1)
      {0x0080, "TObjDoor"},

      // Forest switch. Params:
      //   param4 = switch flag number
      //   param6 = color (clamped to range [0, 9])
      {0x0081, "TObjDoorKey"},

      // Laser fence and square laser fence. Params:
      //   param1 = color (range [0, 3])
      //   param4 = switch flag number
      //   param6 = model (TODO)
      {0x0082, "TObjLazerFenceNorm"},
      {0x0083, "TObjLazerFence4"},

      // Forest laser fence switch. Params:
      //   param4 = switch flag number
      //   param6 = color
      {0x0084, "TLazerFenceSw"},

      // Light rays. Params:
      //   param1-3 = scale (x, y, z)
      {0x0085, "TKomorebi"},

      // Butterfly. Params:
      //   param1-3 = TODO
      {0x0086, "TButterfly"},

      // TODO: Describe this object. Params:
      //   param1 = model number
      {0x0087, "TMotorcycle"},

      // Item box. Params:
      //   param1 = if positive, box is specialized to drop a specific item or
      //     type of item; if zero or negative, box drops any common item or
      //     none at all (and param3-6 are all ignored)
      //   param3 = if zero, then bonuses, grinds, etc. are applied to the item
      //     after it's generated; if nonzero, the item is not randomized at
      //     all and drops exactly as specified in param4-6
      //   param4-6 = item definition. see base_item_for_specialized_box in
      //     ItemCreator.cc for how these values are decoded
      {0x0088, "TObjContainerBase2"},

      // Elevated cylindrical tank. Params:
      //   param1-3 = TODO
      {0x0089, "TObjTank"},

      // TODO: Describe this object. Params:
      //   param1-3 = TODO
      // Availability: vv1/v2+ only
      {0x008A, "TObjBattery"},

      // Forest console. Params:
      //   param4 = quest label to call when activated (inherited from
      //     TObjMesBase)
      //   param5 = model (clamped to [0, 1])
      //   param6 = type (clamped to [0, 1]; 0 = "QUEST", 1 = "RICO")
      //     (inherited from TObjMesBase)
      {0x008B, "TObjComputer"},

      // Black sliding door. Params:
      //   param1 = total distance (divided evenly by the number of switch
      //     flags, from param4)
      //   param2 = speed
      //   param4 = base switch flag (the actual switch flags used are param4,
      //     param4 + 1, param4 + 2, etc.)
      //   param5 = number of switch flags (clamped to [1, 4])
      //   param6 = TODO (only matters if this is zero or nonzero)
      {0x008C, "TObjContainerIdo"},

      // Rico message pod. This object immediately destroys itself in Challenge
      // mode or split-screen mode. Params:
      //   param4 = enable condition:
      //     negative = enabled when player is within 70 units
      //     range [0x00, 0xFF] = enabled by corresponding switch flag
      //     0x100 and above = never enabled
      //   param5 = message number (used with message quest opcode; TODO: has
      //     the same [100, 999] check as some other objects)
      //   param6 = quest label to call when activated
      {0x008D, "TOCapsuleAncient01"},

      // Energy barrier. Params:
      //   param4 = switch flag and activation mode (same as for TODoorLabo)
      {0x008E, "TOBarrierEnergy01"},

      // Forest rising bridge. Once enabled (risen), this object cannot be
      // disabled; that is, there is no way to make the bridge retract. Params:
      //   param4 = switch flag number
      {0x008F, "TObjHashi"},

      // Generic switch. Visually, this is the type usually used for objects
      // other than doors, such as lights, poison rooms, and the Forest 1
      // bridge. Params:
      //   param1 = activation mode:
      //     negative = temporary (TODO: test this)
      //     zero or positive = permanent (normal)
      //   param4 = switch flag number
      {0x0090, "TOKeyGenericSw"},

      // Box that triggers a wave event when opened. Params:
      //   param4 = event number
      {0x0091, "TObjContainerEnemy"},

      // Large box (usually used for specialized drops). See TObjContainerBase2
      // for parameters.
      {0x0092, "TObjContainerBase"},

      // Large enemy box. See TObjContainerEnemy for parameters.
      {0x0093, "TObjContainerAbeEnemy"},

      // Always-empty box.
      // Availability: v2+ only
      {0x0095, "TObjContainerNoItem"},

      // Laser fence. This object is only available in v2 and later. Params:
      //   param1 = color (clamped to [0, 3])
      //   param2 = depth of collision box (transverse to lasers)
      //   param3 = length of collision box (parallel to lasers)
      //   param4 = switch flag number
      //   param6 = model:
      //     0 = short fence
      //     1 = long fence
      //     anything else = invisible
      // Availability: v2+ only
      {0x0096, "TObjLazerFenceExtra"},

      // Caves floor button. The activation radius is always 10 units. Params:
      //   param4 = switch flag number
      //   param5 = activation mode:
      //     negative = temporary (disables flag when player leaves)
      //     zero or positive = permanent
      {0x00C0, "TOKeyCave01"},

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
      {0x00C1, "TODoorCave01"},

      // Caves standard door. Params:
      //   param4 = switch flag number (negative = always unlocked; >0x100 =
      //     always locked)
      {0x00C2, "TODoorCave02"},

      // Caves ceiling piston trap. There are three types of this object, which
      // can be choseby via param6. If param6 is not 0, 1, or 2, no object is
      // created.
      // Params for TOHangceilingCave01Normal (param6 = 0):
      //   param1 = TODO (radius delta? value used is param1 + 29)
      //   param2 = TODO (value used is 1 - param2)
      //   param3 = TODO (value used is param3 + 100)
      // Params for TOHangceilingCave01Key (param6 = 1):
      //   param1-3 = same as for TOHangceilingCave01Normal
      //   param4 = switch flag number (drops when switch flag is activated;
      //     when it has finished dropping, it disables the switch flag)
      // Params for TOHangceilingCave01KeyQuick (param6 = 2):
      //   param1-4 = same as for TOHangceilingCave01Key, but unlike that
      //     object, does not disable the switch flag automatically
      {0x00C3, "TOHangceilingCave01Normal/TOHangceilingCave01Key/TOHangceilingCave01KeyQuick"},

      // Caves signs. There appear to be no parameters.
      {0x00C4, "TOSignCave01"},
      {0x00C5, "TOSignCave02"},
      {0x00C6, "TOSignCave03"},

      // Hexagonal tank. There appear to be no parameters.
      {0x00C7, "TOAirconCave01"},

      // Brown platform. There appear to be no parameters.
      {0x00C8, "TOAirconCave02"},

      // Revolving warning light. Params:
      //   param1 = rotation speed in degrees per frame
      {0x00C9, "TORevlightCave01"},

      // Caves rainbow. Params:
      //   param1-3 = scale factors (x, y, z)
      //   param4 = TODO (value used is 1 / (param4 + 30))
      //   param6 = visibility radius? (TODO; value used is param6 + 40000)
      {0x00CB, "TORainbowCave01"},

      // Floating jellyfish. Params:
      //   param1 = visibility radius; visible when any player is within this
      //     distance of the object
      //   param2 = move radius (according to debug strings)
      //   param3 = rebirth radius (according to debug strings); like param1,
      //     checks against all players, not only the local player
      {0x00CC, "TOKurage"},

      // Floating dragonfly. Params:
      //   param1 = TODO
      //   param2 = TODO
      //   param3 = max distance from home?
      //   param4 = TODO
      //   param5 = TODO
      {0x00CD, "TODragonflyCave01"},

      // Caves door. Params:
      //   param4 = switch flag number
      {0x00CE, "TODoorCave03"},

      // Robot recharge station. Params:
      //   param4 = quest register number; activates when this register
      //     contains a nonzero value; does not deactivate if it becomes zero
      //     again
      {0x00CF, "TOBind"},

      // Caves cake shop. There appear to be no parameters.
      {0x00D0, "TOCakeshopCave01"},

      // Various solid rock objects used in the Cave areas. There are small,
      // medium, and large variations of each, and for the 02 variations, there
      // are also "Simple" variations (00D7-00D9). None of these objects take
      // any parameters.
      {0x00D1, "TORockCaveS01"},
      {0x00D2, "TORockCaveM01"},
      {0x00D3, "TORockCaveL01"},
      {0x00D4, "TORockCaveS02"},
      {0x00D5, "TORockCaveM02"},
      {0x00D6, "TORockCaveL02"},
      {0x00D7, "TORockCaveSS02"},
      {0x00D8, "TORockCaveSM02"},
      {0x00D9, "TORockCaveSL02"},
      {0x00DA, "TORockCaveS03"},
      {0x00DB, "TORockCaveM03"},
      {0x00DC, "TORockCaveL03"},

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
      // Availability: v2+ only
      {0x00DE, "TODummyKeyCave01"},

      // Breakable rocks, in small, medium, and large variations. All of these
      // take the following parameter:
      //   param4 = switch flag number
      // Availability: v2+ only
      {0x00DF, "TORockCaveBL01"},
      {0x00E0, "TORockCaveBL02"},
      {0x00E1, "TORockCaveBL03"},

      // Mines multi-switch door. Params:
      //   param4 = base switch flag number (the actual switch flags used are
      //     param4, param4 + 1, param4 + 2, etc.; if this is negative, the
      //     door is always unlocked)
      //   param5 = 4 - number of switch flags (so if e.g. door should require
      //     only 3 switch flags, set param5 to 1)
      {0x0100, "TODoorMachine01"},

      // Mines floor button. The activation radius is always 10 units. Params:
      //   param4 = switch flag number
      //   param5 = activation mode:
      //     negative = temporary (disables flag when player leaves)
      //     zero or positive = permanent
      {0x0101, "TOKeyMachine01"},

      // Mines single-switch door. Params:
      //   param4 = switch flag number
      {0x0102, "TODoorMachine02"},

      // Large cryo-tube. There appear to be no parameters.
      {0x0103, "TOCapsuleMachine01"},

      // Computer. Same parameters as 0x008D (TOCapsuleAncient01).
      {0x0104, "TOComputerMachine01"},

      // Green monitor. Params:
      //   param4 = initial state? (clamped to [0, 3]; appears to cycle through
      //     those 4 values on its own)
      {0x0105, "TOMonitorMachine01"},

      // Floating robot. Same params as 0x00CD (TODragonflyCave01), though it
      // appears that some may have different scale factors or offsets (TODO).
      {0x0106, "TODragonflyMachine01"},

      // Floating blue light. Params:
      //   param4 = TODO
      //   param5 = TODO
      //   param6 = TODO
      {0x0107, "TOLightMachine01"},

      // Self-destructing objects. Params:
      //   param1 = radius delta (actual radius is param1 + 30)
      {0x0108, "TOExplosiveMachine01"},
      {0x0109, "TOExplosiveMachine02"},
      {0x010A, "TOExplosiveMachine03"},

      // Spark machine. Params:
      //   param1 = TODO (actual value used is param1 - 0.98)
      //   param2 = TODO (seems it only matters if this is <= 0 or not)
      {0x010B, "TOSparkMachine01"},

      // Large flashing box. Params:
      //   param2 = TODO (seems it only matters if this is < 0 or not)
      {0x010C, "TOHangerMachine01"},

      // Ruins entrance door (after Vol Opt). This object reads quest flags
      // 0x2C, 0x2D, and 0x2E to determine the state of each seal on the door
      // (for Forest, Caves, and Mines respectively). It then checks quest flag
      // 0x2F; if this flag is set, then all seals are unlocked regardless of
      // the preceding three flags' values. Curiously, it seems that these
      // flags are checked every frame, even though it's normally impossible to
      // change their values in the area where this object appears.
      // No parameters.
      {0x0130, "TODoorVoShip"},

      // Ruins floor warp. Params:
      //   param4 = destination floor
      //   param6 = color (negative = red, zero or positive = blue); if this is
      //     >= 0 in Challenge mode, the warp is destroyed immediately
      {0x0140, "TObjGoalWarpAncient"},

      // Ruins intra-area warp. Same parameters as 0x0003 (TObjMapWarpForest),
      // but also:
      //   param5 = type (negative = one-way, zero or positive = normal)
      {0x0141, "TObjMapWarpAncient"},

      // Ruins switch. Same parameters as 0x00C0 (TOKeyCave01).
      {0x0142, "TOKeyAncient02"},

      // Ruins floor button. Same parameters as 0x00C0 (TOKeyCave01).
      {0x0143, "TOKeyAncient03"},

      // Ruins doors. These all take the same params as 0x00C2 (TODoorCave02).
      {0x0144, "TODoorAncient01"}, // Usually used in Ruins 1
      {0x0145, "TODoorAncient03"}, // Usually used in Ruins 3
      {0x0146, "TODoorAncient04"}, // Usually used in Ruins 2
      {0x0147, "TODoorAncient05"}, // Usually used in Ruins 1
      {0x0148, "TODoorAncient06"}, // Usually used in Ruins 2
      {0x0149, "TODoorAncient07"}, // Usually used in Ruins 3

      // Ruins 4-player door. Params:
      //   param4 = base switch flag number (the actual switch flags used are
      //     param4, param4 + 1, param4 + 2, and param4 + 3); param4 is clamped
      //     to [0, 0xFC]
      //   param6 = activation mode; same as for 0x00C1 (TODoorCave01)
      {0x014A, "TODoorAncient08"},

      // Ruins 2-player door. Params:
      //   param4 = base switch flag number (the actual switch flags used are
      //     param4 and param4 + 1); param4 is clamped to [0, 0xFE]
      //   param6 = activation mode; same as for 0x00C1 (TODoorCave01)
      {0x014B, "TODoorAncient09"},

      // Ruins sensor. Params:
      //   param1 = activation radius delta (actual radius is param1 + 50)
      //   param4 = switch flag number
      //   param5 = if negative, sensor is always on
      //   param6 = TODO (clamped to [0, 1] - model index?)
      {0x014C, "TOSensorAncient01"},

      // Ruins laser fence switch. Params:
      //   param1 = if negative, switch's effect is temporary; if zero or
      //     positive, it's permanent
      //   param4 = switch flag number
      //   param5 = color (clamped to [0, 3])
      {0x014D, "TOKeyAncient01"},

      // TODO: Describe the rest of the object types.
      {0x014E, "TOFenceAncient01"}, // Constructor in 3OE1: 80166A4C
      {0x014F, "TOFenceAncient02"}, // Constructor in 3OE1: 80166E2C
      {0x0150, "TOFenceAncient03"}, // Constructor in 3OE1: 801671D0
      {0x0151, "TOFenceAncient04"}, // Constructor in 3OE1: 80167574
      {0x0152, "TContainerAncient01"}, // Constructor in 3OE1: 8018B9B0
      {0x0153, "TOTrapAncient01"}, // Constructor in 3OE1: 80179C90
      {0x0154, "TOTrapAncient02"}, // Constructor in 3OE1: 8017B348
      {0x0155, "TOMonumentAncient01"}, // Constructor in 3OE1: 801725B4
      {0x0156, "TOMonumentAncient02"}, // Constructor in 3OE1: 80172BAC
      {0x0159, "TOWreckAncient01"}, // Constructor in 3OE1: 8017DA9C
      {0x015A, "TOWreckAncient02"}, // Constructor in 3OE1: 8017DDF0
      {0x015B, "TOWreckAncient03"}, // Constructor in 3OE1: 8017DF10
      {0x015C, "TOWreckAncient04"}, // Constructor in 3OE1: 8017D978
      {0x015D, "TOWreckAncient05"}, // Constructor in 3OE1: 8017D854
      {0x015E, "TOWreckAncient06"}, // Constructor in 3OE1: 8017D730
      {0x015F, "TOWreckAncient07"}, // Constructor in 3OE1: 8017D60C
      {0x0160, "TObjFogCollisionPoison/TObjWarpBoss03"}, // Constructor in 3OE1: 80153768 OR 801A29C8
      {0x0161, "TOContainerAncientItemCommon"}, // Constructor in 3OE1: 8015BFE8
      {0x0162, "TOContainerAncientItemRare"}, // Constructor in 3OE1: 801A2A14
      {0x0163, "TOContainerAncientEnemyCommon"}, // Constructor in 3OE1: 8015B69C
      {0x0164, "TOContainerAncientEnemyRare"}, // Constructor in 3OE1: 801A2A5C
      {0x0165, "TOContainerAncientItemNone"}, // Constructor in 3OE1: 8015BD78 (v2+ only)
      {0x0166, "TOWreckAncientBrakable05"}, // Constructor in 3OE1: 8017D1DC (v2+ only)
      {0x0167, "TOTrapAncient02R"}, // Constructor in 3OE1: 8017A96C (v2+ only)
      {0x0170, "TOBoss4Bird"}, // Constructor in 3OE1: 8015982C
      {0x0171, "TOBoss4Tower"}, // Constructor in 3OE1: 801592E0
      {0x0172, "TOBoss4Rock"}, // Constructor in 3OE1: 80158D90
      {0x0173, "TOSoulDF"}, // Constructor in 2OEF: 8C1DBA94 (v1/v2 only)
      {0x0174, "TOButterflyDF"}, // Constructor in 2OEF: 8C1DE660 (v1/v2 only)
      {0x0180, "TObjInfoCol"}, // Constructor in 3OE1: 801A27B4
      {0x0181, "TObjWarpLobby"}, // Constructor in 3OE1: 801A2800
      {0x0182, "TObjLobbyMain"}, // Constructor in 3OE1: 80350B84 (v3+ only)
      {0x0183, "__LOBBY_PIGEON__"}, // Formerly __TObjPathObj_subclass_0183__ // Constructor in 3OE1: 802BF420 (v3+ only)
      {0x0184, "TObjButterflyLobby"}, // Constructor in 3OE1: 8034FA8C (v3+ only)
      {0x0185, "TObjRainbowLobby"}, // Constructor in 3OE1: 8034EB9C (v3+ only)
      {0x0186, "TObjKabochaLobby"}, // Constructor in 3OE1: 80351A18 (v3+ only)
      {0x0187, "TObjStendGlassLobby"}, // Constructor in 3OE1: 80357CD8 (v3+ only)
      {0x0188, "TObjCurtainLobby"}, // Constructor in 3OE1: 80359DF4 (v3+ only)
      {0x0189, "TObjWeddingLobby"}, // Constructor in 3OE1: 8035A1E0 (v3+ only)
      {0x018A, "TObjTreeLobby"}, // Constructor in 3OE1: 80362D44 (v3+ only)
      {0x018B, "TObjSuisouLobby"}, // Constructor in 3OE1: 80368118 (v3+ only)
      {0x018C, "TObjParticleLobby"}, // Constructor in 3OE1: 80367DC0 (v3+ only)
      {0x018D, "TObjLobbyTable"}, // Constructor in 3SE0: 802C07E4 (Ep3 only)
      {0x018E, "TObjJukeBox"}, // Constructor in 3SE0: 8030D8A8 (Ep3 only)
      {0x0190, "TObjCamera"}, // Constructor in 3OE1: 8017FAC0 (v2+ only)
      {0x0191, "TObjTuitate"}, // Constructor in 3OE1: 8019AF20 (v2+ only)
      {0x0192, "TObjDoaEx01"}, // Constructor in 3OE1: 8018E02C (v2+ only)
      {0x0193, "TObjBigTuitate"}, // Constructor in 3OE1: 8019AB9C (v2+ only)
      {0x01A0, "TODoorVS2Door01"}, // Constructor in 3OE1: 80164084 (v2+ only)
      {0x01A1, "TOVS2Wreck01"}, // Constructor in 3OE1: 8017C520 (v2+ only)
      {0x01A2, "TOVS2Wreck02"}, // Constructor in 3OE1: 8017C438 (v2+ only)
      {0x01A3, "TOVS2Wreck03"}, // Constructor in 3OE1: 8017C350 (v2+ only)
      {0x01A4, "TOVS2Wreck04"}, // Constructor in 3OE1: 8017C268 (v2+ only)
      {0x01A5, "TOVS2Wreck05"}, // Constructor in 3OE1: 8017C180 (v2+ only)
      {0x01A6, "TOVS2Wreck06"}, // Constructor in 3OE1: 8017C098 (v2+ only)
      {0x01A7, "TOVS2Wall01"}, // Constructor in 3OE1: 8017BEC8 (v2+ only)
      {0x01A8, "__OBJECT_MAP_DETECT_TEMPLE__"}, // Name is from qedit; object class has no name in the client // Constructor in 3OE1: 80085794 (v2+ only)
      {0x01A9, "TObjHashiVersus1"}, // Constructor in 3OE1: 80191388 (v2+ only)
      {0x01AA, "TObjHashiVersus2"}, // Constructor in 3OE1: 8019118C (v2+ only)
      {0x01AB, "TODoorFourLightRuins"}, // Constructor in 3OE1: 801A271C (v3+ only)
      {0x01C0, "TODoorFourLightSpace"}, // Constructor in 3OE1: 801A2768 (v3+ only)
      {0x0200, "TObjContainerJung"}, // Constructor in 3OE1: 8018D2CC (v3+ only)
      {0x0201, "TObjWarpJung"}, // Constructor in 3OE1: 8019FF00 (v3+ only)
      {0x0202, "TObjDoorJung"}, // Constructor in 3OE1: 8018F2DC (v3+ only)
      {0x0203, "TObjContainerJungEx"}, // Constructor in 3OE1: 8018CE58 (v3+ only)
      {0x0204, "TODoorJungleMain"}, // Constructor in 3OE1: 80299E20 (v3+ only)
      {0x0205, "TOKeyJungleMain"}, // Constructor in 3OE1: 8029BA64 (v3+ only)
      {0x0206, "TORockJungleS01"}, // Constructor in 3OE1: 8029B3F8 (v3+ only)
      {0x0207, "TORockJungleM01"}, // Constructor in 3OE1: 8029AFAC (v3+ only)
      {0x0208, "TORockJungleL01"}, // Constructor in 3OE1: 8029AC38 (v3+ only)
      {0x0209, "TOGrassJungle"}, // Constructor in 3OE1: 8029B764 (v3+ only)
      {0x020A, "TObjWarpJungMain"}, // Constructor in 3OE1: 8019FA1C (v3+ only)
      {0x020B, "TBGLightningCtrl"}, // Constructor in 3OE1: 802A8750 (v3+ only)
      {0x020C, "__WHITE_BIRD__"}, // Formerly __TObjPathObj_subclass_020C__ // Constructor in 3OE1: 802C0C64 (v3+ only)
      {0x020D, "__ORANGE_BIRD__"}, // Formerly __TObjPathObj_subclass_020D__ // Constructor in 3OE1: 802C05BC (v3+ only)
      {0x020E, "TObjContainerJungEnemy"}, // Constructor in 3OE1: 8018CCF8 (v3+ only)
      {0x020F, "TOTrapChainSawDamage"}, // Constructor in 3OE1: 802C7748 (v3+ only)
      {0x0210, "TOTrapChainSawKey"}, // Constructor in 3OE1: 802C7234 (v3+ only)
      {0x0211, "TOBiwaMushi"}, // Constructor in 3OE1: 802A8D98 (v3+ only)
      {0x0212, "__SEAGULL__"}, // Formerly __TObjPathObj_subclass_0212__ // Constructor in 3OE1: 802BFDE8 (v3+ only)
      {0x0213, "TOJungleDesign"}, // Constructor in 3OE1: 802FD478 (v3+ only)
      {0x0220, "TObjFish"}, // Constructor in 3OE1: 8029D04C (v3+ only)
      {0x0221, "TODoorFourLightSeabed"}, // Constructor in 3OE1: 801A25EC (v3+ only)
      {0x0222, "TODoorFourLightSeabedU"}, // Constructor in 3OE1: 801A2638 (v3+ only)
      {0x0223, "TObjSeabedSuiso_CH"}, // Constructor in 3OE1: 802A5290 (v3+ only)
      {0x0224, "TObjSeabedSuisoBrakable"}, // Constructor in 3OE1: 802A507C (v3+ only)
      {0x0225, "TOMekaFish00"}, // Constructor in 3OE1: 802A9378 (v3+ only)
      {0x0226, "TOMekaFish01"}, // Constructor in 3OE1: 802A9088 (v3+ only)
      {0x0227, "__DOLPHIN__"}, // Formerly __TObjPathObj_subclass_0227__ // Constructor in 3OE1: 802C1378 (v3+ only)
      {0x0228, "TOTrapSeabed01"}, // Constructor in 3OE1: 802C9154 (v3+ only)
      {0x0229, "TOCapsuleLabo"}, // Constructor in 3OE1: 802ADD40 (v3+ only)
      {0x0240, "TObjParticle"}, // Constructor in 3OE1: 801954E4 (v3+ only)
      {0x0280, "__BARBA_RAY_TELEPORTER__"}, // Formerly __TObjAreaWarpForest_subclass_0280__ // Constructor in 3OE1: 802EF620 (v3+ only)
      {0x02A0, "TObjLiveCamera"}, // Constructor in 3OE1: 80309D5C (v3+ only)
      {0x02B0, "TContainerAncient01R"}, // Constructor in 3OE1: 8018ADF8 (v3+ only)
      {0x02B1, "TObjLaboDesignBase(0)"}, // Constructor in 3OE1: 803631D4 (v3+ only)
      {0x02B2, "TObjLaboDesignBase(1)"}, // Constructor in 3OE1: 80363184 (v3+ only)
      {0x02B3, "TObjLaboDesignBase(2)"}, // Constructor in 3OE1: 80363134 (v3+ only)
      {0x02B4, "TObjLaboDesignBase(3)"}, // Constructor in 3OE1: 803630E4 (v3+ only)
      {0x02B5, "TObjLaboDesignBase(4)"}, // Constructor in 3OE1: 80363094 (v3+ only)
      {0x02B6, "TObjLaboDesignBase(5)"}, // Constructor in 3OE1: 80363044 (v3+ only)
      {0x02B7, "TObjGbAdvance"}, // Constructor in 3OE1: 80187C10 (v3+ only)
      {0x02B8, "TObjQuestColALock2"}, // Constructor in 3OE1: 80195824 (v3+ only)
      {0x02B9, "TObjMapForceWarp"}, // Constructor in 3OE1: 801A297C (v3+ only)
      {0x02BA, "TObjQuestCol2"}, // Constructor in 3OE1: 80195680 (v3+ only)
      {0x02BB, "TODoorLaboNormal"}, // Constructor in 3OE1: 801A26D0 (v3+ only)
      {0x02BC, "TObjAreaWarpEndingJung"}, // Constructor in 3OE1: 8019AFF4 (v3+ only)
      {0x02BD, "TObjLaboMapWarp"}, // Constructor in 3OE1: 80185430 (v3+ only)
      {0x02D0, "TObjKazariCard"}, // Constructor in 3SE0: 8026C79C (Ep3 only; battle only)
      {0x02D1, "TObj_FloatingCardMaterial_Dark"}, // Constructor in 3SE0: 800C5F30 (Ep3 only; city only)
      {0x02D2, "TObj_FloatingCardMaterial_Hero"}, // Constructor in 3SE0: 800C5F7C (Ep3 only; city only)
      {0x02D3, "TObjCardCityMapWarp(0)"}, // Constructor in 3SE0: 800B9528 (Ep3 only; city only)
      {0x02D4, "TObjCardCityDoor(0)"}, // Constructor in 3SE0: 800B8C40 (Ep3 only; city only)
      {0x02D5, "TObjCardCityDoor(1)"}, // Constructor in 3SE0: 800B8BF0 (Ep3 only; city only)
      {0x02D6, "TObjKazariGeyserMizu"}, // Constructor in 3SE0: 80278E08 (Ep3 only; battle only)
      {0x02D7, "TObjSetCardColi"}, // Constructor in 3SE0: 802BCE80 (Ep3 only; battle only)
      {0x02D8, "TObjCardCityDoor(2)"}, // Constructor in 3SE0: 800B8BA0 (Ep3 only; city only)
      {0x02D9, "TObjCardCityMapWarp(1)"}, // Constructor in 3SE0: 800B94D8 (Ep3 only; city only)
      {0x02DA, "TOFlyMekaHero"}, // Constructor in 3SE0: 802DFD18 (Ep3 only; city only)
      {0x02DB, "TOFlyMekaDark"}, // Constructor in 3SE0: 802DFAAC (Ep3 only; city only)
      {0x02DC, "TObjCardCityDoor_Closed(0)"}, // Constructor in 3SE0: 800B884C (Ep3 only; city only)
      {0x02DD, "TObjCardCityDoor_Closed(1)"}, // Constructor in 3SE0: 800B87FC (Ep3 only; city only)
      {0x02DE, "TObjCardCityDoor_Closed(2)"}, // Constructor in 3SE0: 800B87AC (Ep3 only; city only)
      {0x02DF, "TObjCardCityDoor(3)"}, // Constructor in 3SE0: 800B8B50 (Ep3 only; city only)
      {0x02E0, "TObjCardCityDoor(4)"}, // Constructor in 3SE0: 800B8B00 (Ep3 only; city only)
      {0x02E1, "TObjCardCityDoor_Closed(3)"}, // Constructor in 3SE0: 800B875C (Ep3 only; city only)
      {0x02E2, "TObjCardCityDoor_Closed(4)"}, // Constructor in 3SE0: 800B870C (Ep3 only; city only)
      {0x02E3, "TObjCardCityMapWarp(2)"}, // Constructor in 3SE0: 800B9488 (Ep3 only; city only)
      {0x02E4, "TObjSinBoardCard"}, // Constructor in 3SE0: 80309608; cit (Ep3 only; lobby only)
      {0x02E5, "TObjCityMoji"}, // Constructor in 3SE0: 8030DE8C (Ep3 only; city only)
      {0x02E6, "TObjCityWarpOff"}, // Constructor in 3SE0: 8030DB4C (Ep3 only; city only)
      {0x02E7, "TObjFlyCom"}, // Constructor in 3SE0: 80310BEC (Ep3 only; city only)
      {0x02E8, "__UNKNOWN_02E8__"}, // Subclass of TObjPathObj; 3SE0: 8019A638 (Ep3 only; city only)
      {0x0300, "__EP4_LIGHT__"}, // Constructor in 59NL: 00661158 (v4 only)
      {0x0301, "__WILDS_CRATER_CACTUS__"}, // Constructor in 59NL: 0067612C (v4 only)
      {0x0302, "__WILDS_CRATER_BROWN_ROCK__"}, // Constructor in 59NL: 00675748 (v4 only)
      {0x0303, "__WILDS_CRATER_BROWN_ROCK_DESTRUCTIBLE__"}, // Constructor in 59NL: 00675BF8 (v4 only)
      {0x0340, "__UNKNOWN_0340__"}, // Constructor in 59NL: 00673FB8 (v4 only)
      {0x0341, "__UNKNOWN_0341__"}, // Constructor in 59NL: 00674118 (v4 only)
      {0x0380, "__POISON_PLANT__"}, // Constructor in 59NL: 0067927C (v4 only)
      {0x0381, "__UNKNOWN_0381__"}, // Constructor in 59NL: 00679678 (v4 only)
      {0x0382, "__UNKNOWN_0382__"}, // Constructor in 59NL: 0067A264 (v4 only)
      {0x0383, "__DESERT_OOZE_PLANT__"}, // Constructor in 59NL: 006781EC (v4 only)
      {0x0385, "__UNKNOWN_0385__"}, // Constructor in 59NL: 006785C8 (v4 only)
      {0x0386, "__WILDS_CRATER_BLACK_ROCKS__"}, // Constructor in 59NL: 00677DE4 (v4 only)
      {0x0387, "__UNKNOWN_0387__"}, // Constructor in 59NL: 006119E4 (v4 only)
      {0x0388, "__UNKNOWN_0388__"}, // Constructor in 59NL: 00635D1C (v4 only)
      {0x0389, "__UNKNOWN_0389__"}, // Constructor in 59NL: 0063810C (v4 only)
      {0x038A, "__UNKNOWN_038A__"}, // Constructor in 59NL: 00619604 (v4 only)
      {0x038B, "__FALLING_ROCK__"}, // Constructor in 59NL: 00679F58 (v4 only)
      {0x038C, "__DESERT_PLANT_SOLID__"}, // Constructor in 59NL: 0067A548 (v4 only)
      {0x038D, "__DESERT_CRYSTALS_BOX__"}, // Constructor in 59NL: 00677610 (v4 only)
      {0x038E, "__EP4_TEST_DOOR__"}, // Constructor in 59NL: 00677A80 (v4 only)
      {0x038F, "__BEE_HIVE__"}, // Constructor in 59NL: 00676ADC (v4 only)
      {0x0390, "__EP4_TEST_PARTICLE__"}, // Constructor in 59NL: 00678C00 (v4 only)
      {0x0391, "__HEAT__"}, // Constructor in 59NL: 005C2820 (v4 only)
      {0x03C0, "__EP4_BOSS_EGG__"}, // Constructor in 59NL: 0076FB74 (v4 only)
      {0x03C1, "__EP4_BOSS_ROCK_SPAWNER__"}, // Constructor in 59NL: 00770028 (v4 only)
  });
  try {
    return names.at(type);
  } catch (const out_of_range&) {
    return "__UNKNOWN__";
  }
}

const char* MapFile::name_for_enemy_type(uint16_t type) {
  static const unordered_map<uint16_t, const char*> names({
      {0x0001, "TObjNpcFemaleBase"},
      {0x0002, "TObjNpcFemaleChild"},
      {0x0003, "TObjNpcFemaleDwarf"},
      {0x0004, "TObjNpcFemaleFat"},
      {0x0005, "TObjNpcFemaleMacho"},
      {0x0006, "TObjNpcFemaleOld"},
      {0x0007, "TObjNpcFemaleTall"},
      {0x0008, "TObjNpcMaleBase"},
      {0x0009, "TObjNpcMaleChild"},
      {0x000A, "TObjNpcMaleDwarf"},
      {0x000B, "TObjNpcMaleFat"},
      {0x000C, "TObjNpcMaleMacho"},
      {0x000D, "TObjNpcMaleOld"},
      {0x000E, "TObjNpcMaleTall"},
      {0x0019, "TObjNpcSoldierBase"},
      {0x001A, "TObjNpcSoldierMacho"},
      {0x001B, "TObjNpcGovernorBase"},
      {0x001C, "TObjNpcConnoisseur"},
      {0x001D, "TObjNpcCloakroomBase"},
      {0x001E, "TObjNpcExpertBase"},
      {0x001F, "TObjNpcNurseBase"},
      {0x0020, "TObjNpcSecretaryBase"},
      {0x0021, "TObjNpcHHM00"},
      {0x0022, "TObjNpcNHW00"},
      {0x0024, "TObjNpcHRM00"},
      {0x0025, "TObjNpcARM00"},
      {0x0026, "TObjNpcARW00"},
      {0x0027, "TObjNpcHFW00"},
      {0x0028, "TObjNpcNFM00"},
      {0x0029, "TObjNpcNFW00"},
      {0x002B, "TObjNpcNHW01"},
      {0x002C, "TObjNpcAHM01"},
      {0x002D, "TObjNpcHRM01"},
      {0x0030, "TObjNpcHFW01"},
      {0x0031, "TObjNpcNFM01"},
      {0x0032, "TObjNpcNFW01"},
      {0x0033, "TObjNpcEnemy"}, // v3+ only
      {0x0040, "TObjEneMoja"},
      {0x0041, "TObjEneLappy"},
      {0x0042, "TObjEneBm3FlyNest"},
      {0x0043, "TObjEneBm5Wolf"},
      {0x0044, "TObjEneBeast"},
      {0x0045, "TObjNpcLappy"},
      {0x0046, "TObjNpcMoja"},
      {0x0047, "TObjNpcRico"}, // v2 only (not v1 nor v3+)
      {0x0060, "TObjGrass"},
      {0x0061, "TObjEneRe2Flower"},
      {0x0062, "TObjEneNanoDrago"},
      {0x0063, "TObjEneShark"},
      {0x0064, "TObjEneSlime"},
      {0x0065, "TObjEnePanarms"},
      {0x0080, "TObjEneDubchik"},
      {0x0081, "TObjEneGyaranzo"},
      {0x0082, "TObjEneMe3ShinowaReal"},
      {0x0083, "TObjEneMe1Canadin"},
      {0x0084, "TObjEneMe1CanadinLeader"},
      {0x0085, "TOCtrlDubchik"},
      {0x00A0, "TObjEneSaver"},
      {0x00A1, "TObjEneRe4Sorcerer"},
      {0x00A2, "TObjEneDarkGunner"},
      {0x00A3, "TObjEneDarkGunCenter"},
      {0x00A4, "TObjEneDf2Bringer"},
      {0x00A5, "TObjEneRe7Berura"},
      {0x00A6, "TObjEneDimedian"},
      {0x00A7, "TObjEneBalClawBody"},
      {0x00A8, "__TObjEneBalClawClaw_SUBCLASS__"},
      {0x00A9, "TObjNpcBringer"},
      {0x00C0, "TBoss1Dragon/TBoss5Gryphon"},
      {0x00C1, "TBoss2DeRolLe"},
      {0x00C2, "TBoss3Volopt"},
      {0x00C3, "TBoss3VoloptP01"},
      {0x00C4, "TBoss3VoloptCore/SUBCLASS"},
      {0x00C5, "__TObjEnemyCustom_SUBCLASS__"},
      {0x00C6, "TBoss3VoloptMonitor"},
      {0x00C7, "TBoss3VoloptHiraisin"},
      {0x00C8, "TBoss4DarkFalz"},
      {0x00CA, "TBoss6PlotFalz"}, // v3+ only
      {0x00CB, "TBoss7DeRolLeC"}, // v3+ only
      {0x00CC, "TBoss8Dragon"}, // v3+ only
      {0x00D0, "TObjNpcKenkyu"}, // v3+ only
      {0x00D1, "TObjNpcSoutokufu"}, // v3+ only
      {0x00D2, "TObjNpcHosa"}, // v3+ only
      {0x00D3, "TObjNpcKenkyuW"}, // v3+ only
      {0x00D4, "TObjEneMe3StelthReal"}, // v3+ only
      {0x00D5, "TObjEneMerillLia"}, // v3+ only
      {0x00D6, "TObjEneBm9Mericarol"}, // v3+ only
      {0x00D7, "TObjEneBm5GibonU"}, // v3+ only
      {0x00D8, "TObjEneGibbles"}, // v3+ only
      {0x00D9, "TObjEneMe1Gee"}, // v3+ only
      {0x00DA, "TObjEneMe1GiGue"}, // v3+ only
      {0x00DB, "TObjEneDelDepth"}, // v3+ only
      {0x00DC, "TObjEneDellBiter"}, // v3+ only
      {0x00DD, "TObjEneDolmOlm"}, // v3+ only
      {0x00DE, "TObjEneMorfos"}, // v3+ only
      {0x00DF, "TObjEneRecobox"}, // v3+ only
      {0x00E0, "TObjEneMe3SinowZoaReal/TObjEneEpsilonBody"}, // v3+ only
      {0x00E1, "TObjEneIllGill"}, // v3+ only
      {0x00F0, "TObjNpcHosa2"}, // v3+ only
      {0x00F1, "TObjNpcKenkyu2"}, // v3+ only
      {0x00F2, "TObjNpcNgcBase"}, // v3+ only
      {0x00F3, "TObjNpcNgcBase"}, // v3+ only
      {0x00F4, "TObjNpcNgcBase"}, // v3+ only
      {0x00F5, "TObjNpcNgcBase"}, // v3+ only
      {0x00F6, "TObjNpcNgcBase"}, // v3+ only
      {0x00F7, "TObjNpcNgcBase"}, // v3+ only
      {0x00F8, "TObjNpcNgcBase"}, // v3+ only
      {0x00F9, "TObjNpcNgcBase"}, // v3+ only
      {0x00FA, "TObjNpcNgcBase"}, // v3+ only
      {0x00FB, "TObjNpcNgcBase"}, // v3+ only
      {0x00FC, "TObjNpcNgcBase"}, // v3+ only
      {0x00FD, "TObjNpcNgcBase"}, // v3+ only
      {0x00FE, "TObjNpcNgcBase"}, // v3+ only
      {0x00FF, "TObjNpcNgcBase"}, // v3+ only
      {0x0100, "__UNKNOWN_NPC_0100__"}, // v4 only
      {0x0110, "__ASTARK__/TObjNpcWalkingMeka_Hero"}, // Ep3/v4 only
      {0x0111, "__YOWIE__/__SATELLITE_LIZARD__/TObjNpcWalkingMeka_Dark"}, // Ep3/v4 only
      {0x0112, "__MERISSA_A__/TObjNpcHeroAide"}, // Ep3/v4 only
      {0x0113, "__GIRTABLULU__"}, // v4 only
      {0x0114, "__ZU__"}, // v4 only
      {0x0115, "__BOOTA_FAMILY__"}, // v4 only
      {0x0116, "__DORPHON__"}, // v4 only
      {0x0117, "__GORAN_FAMILY__"}, // v4 only
      {0x0118, "__UNKNOWN_0118__"}, // v4 only
      {0x0119, "__EPISODE_4_BOSS__"}, // v4 only
  });
  try {
    return names.at(type);
  } catch (const out_of_range&) {
    return "__UNKNOWN__";
  }
}

string MapFile::ObjectSetEntry::str() const {
  string name_str = MapFile::name_for_object_type(this->base_type);
  return phosg::string_printf("[ObjectSetEntry type=%04hX \"%s\" set_flags=%04hX index=%04hX a2=%04hX entity_id=%04hX group=%04hX room=%04hX a3=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 "] unused=%08" PRIX32 "]",
      this->base_type.load(),
      name_str.c_str(),
      this->set_flags.load(),
      this->index.load(),
      this->unknown_a2.load(),
      this->entity_id.load(),
      this->group.load(),
      this->room.load(),
      this->unknown_a3.load(),
      this->pos.x.load(),
      this->pos.y.load(),
      this->pos.z.load(),
      this->angle.x.load(),
      this->angle.y.load(),
      this->angle.z.load(),
      this->fparam1.load(),
      this->fparam2.load(),
      this->fparam3.load(),
      this->iparam4.load(),
      this->iparam5.load(),
      this->iparam6.load(),
      this->unused.load());
}

uint64_t MapFile::ObjectSetEntry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->group, sizeof(this->group), ret);
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->fparam1, sizeof(this->fparam1), ret);
  ret = phosg::fnv1a64(&this->fparam2, sizeof(this->fparam2), ret);
  ret = phosg::fnv1a64(&this->fparam3, sizeof(this->fparam3), ret);
  ret = phosg::fnv1a64(&this->iparam4, sizeof(this->iparam4), ret);
  ret = phosg::fnv1a64(&this->iparam5, sizeof(this->iparam5), ret);
  ret = phosg::fnv1a64(&this->iparam6, sizeof(this->iparam6), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

string MapFile::EnemySetEntry::str() const {
  return phosg::string_printf("[EnemySetEntry type=%04hX \"%s\" set_flags=%04hX index=%04hX num_children=%04hX floor=%04hX entity_id=%04hX room=%04hX wave_number=%04hX wave_number2=%04hX a1=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %g %g %04hX %04hX] unused=%08" PRIX32 "]",
      this->base_type.load(),
      MapFile::name_for_enemy_type(this->base_type),
      this->set_flags.load(),
      this->index.load(),
      this->num_children.load(),
      this->floor.load(),
      this->entity_id.load(),
      this->room.load(),
      this->wave_number.load(),
      this->wave_number2.load(),
      this->unknown_a1.load(),
      this->pos.x.load(),
      this->pos.y.load(),
      this->pos.z.load(),
      this->angle.x.load(),
      this->angle.y.load(),
      this->angle.z.load(),
      this->fparam1.load(),
      this->fparam2.load(),
      this->fparam3.load(),
      this->fparam4.load(),
      this->fparam5.load(),
      this->iparam6.load(),
      this->iparam7.load(),
      this->unused.load());
}

uint64_t MapFile::EnemySetEntry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->num_children, sizeof(this->num_children), ret);
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  ret = phosg::fnv1a64(&this->wave_number2, sizeof(this->wave_number2), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->fparam1, sizeof(this->fparam1), ret);
  ret = phosg::fnv1a64(&this->fparam2, sizeof(this->fparam2), ret);
  ret = phosg::fnv1a64(&this->fparam3, sizeof(this->fparam3), ret);
  ret = phosg::fnv1a64(&this->fparam4, sizeof(this->fparam4), ret);
  ret = phosg::fnv1a64(&this->fparam5, sizeof(this->fparam5), ret);
  ret = phosg::fnv1a64(&this->iparam6, sizeof(this->iparam6), ret);
  ret = phosg::fnv1a64(&this->iparam7, sizeof(this->iparam7), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

string MapFile::Event1Entry::str() const {
  return phosg::string_printf("[Event1Entry event_id=%08" PRIX32 " flags=%04hX event_type=%04hX room=%04hX wave_number=%04hX delay=%08" PRIX32 " action_stream_offset=%08" PRIX32 "]",
      this->event_id.load(),
      this->flags.load(),
      this->event_type.load(),
      this->room.load(),
      this->wave_number.load(),
      this->delay.load(),
      this->action_stream_offset.load());
}

uint64_t MapFile::Event1Entry::semantic_hash(uint8_t floor) const {
  uint64_t ret = phosg::fnv1a64(&this->event_id, sizeof(this->event_id));
  ret = phosg::fnv1a64(&this->room, sizeof(this->room), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  ret = phosg::fnv1a64(&floor, sizeof(floor), ret);
  return ret;
}

string MapFile::Event2Entry::str() const {
  return phosg::string_printf("[Event2Entry event_id=%08" PRIX32 " flags=%04hX event_type=%04hX room=%04hX wave_number=%04hX min_delay=%08" PRIX32 " max_delay=%08" PRIX32 " min_enemies=%02hhX max_enemies=%02hhX max_waves=%04hX action_stream_offset=%08" PRIX32 "]",
      this->event_id.load(),
      this->flags.load(),
      this->event_type.load(),
      this->room.load(),
      this->wave_number.load(),
      this->min_delay.load(),
      this->max_delay.load(),
      this->min_enemies,
      this->max_enemies,
      this->max_waves.load(),
      this->action_stream_offset.load());
}

string MapFile::RandomEnemyLocationEntry::str() const {
  return phosg::string_printf("[RandomEnemyLocationEntry x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 "a9=%04hX a10=%04hX]",
      this->pos.x.load(),
      this->pos.y.load(),
      this->pos.z.load(),
      this->angle.x.load(),
      this->angle.y.load(),
      this->angle.z.load(),
      this->unknown_a9.load(),
      this->unknown_a10.load());
}

string MapFile::RandomEnemyDefinition::str() const {
  return phosg::string_printf("[RandomEnemyDefinition params=[%g %g %g %g %g %04hX %04hX] entry_num=%08" PRIX32 " min_children=%04hX max_children=%04hX]",
      this->fparam1.load(),
      this->fparam2.load(),
      this->fparam3.load(),
      this->fparam4.load(),
      this->fparam5.load(),
      this->iparam6.load(),
      this->iparam7.load(),
      this->entry_num.load(),
      this->min_children.load(),
      this->max_children.load());
}

string MapFile::RandomEnemyWeight::str() const {
  return phosg::string_printf("[RandomEnemyWeight base_type_index=%02hhX def_entry_num=%02hhX weight=%02hhX a4=%02hhX]",
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
      throw runtime_error(phosg::string_printf("quest entities list has invalid section header at offset 0x%zX", r.where() - sizeof(header)));
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
                    e.fparam1 = def.fparam1;
                    e.fparam2 = def.fparam2;
                    e.fparam3 = def.fparam3;
                    e.fparam4 = def.fparam4;
                    e.fparam5 = def.fparam5;
                    e.iparam6 = def.iparam6;
                    e.iparam7 = def.iparam7;
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
        ret.emplace_back(phosg::string_printf("  00            nop"));
        break;
      case 0x01:
        ret.emplace_back(phosg::string_printf("  01            stop"));
        r.go(r.size());
        break;
      case 0x08: {
        uint16_t room = r.get_u16l();
        uint16_t group = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  08 %04hX %04hX  construct_objects       room=%04hX group=%04hX",
            room, group, room, group));
        break;
      }
      case 0x09: {
        uint16_t room = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  09 %04hX %04hX  construct_enemies       room=%04hX wave_number=%04hX",
            room, wave_number, room, wave_number));
        break;
      }
      case 0x0A: {
        uint16_t id = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  0A %04hX       enable_switch_flag      id=%04hX", id, id));
        break;
      }
      case 0x0B: {
        uint16_t id = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  0B %04hX       disable_switch_flag     id=%04hX", id, id));
        break;
      }
      case 0x0C: {
        uint32_t event_id = r.get_u32l();
        ret.emplace_back(phosg::string_printf("  0C %08" PRIX32 "   trigger_event           event_id=%08" PRIX32, event_id, event_id));
        break;
      }
      case 0x0D: {
        uint16_t room = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  0D %04hX %04hX  construct_enemies_stop  room=%04hX wave_number=%04hX",
            room, wave_number, room, wave_number));
        r.go(r.size());
        break;
      }
      default:
        ret.emplace_back(phosg::string_printf("  %02hhX            .invalid", opcode));
    }
  }

  return phosg::join(ret, "\n");
}

string MapFile::disassemble(bool reassembly) const {
  deque<string> ret;
  for (uint8_t floor = 0; floor < this->sections_for_floor.size(); floor++) {
    const auto& sf = this->sections_for_floor[floor];
    phosg::StringReader as_r(sf.event_action_stream, sf.event_action_stream_bytes);

    if (sf.object_sets) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".object_sets %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(".object_sets %hhu /* 0x%zX in file; 0x%zX bytes */",
            floor, sf.object_sets_file_offset, sf.object_sets_file_size));
      }
      for (size_t z = 0; z < sf.object_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.object_sets[z].str());
        } else {
          size_t k_id = z + sf.first_object_set_index;
          ret.emplace_back(phosg::string_printf("/* K-%03zX */ ", k_id) + sf.object_sets[z].str());
        }
      }
    }
    if (sf.enemy_sets) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".enemy_sets %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(".enemy_sets %hhu /* 0x%zX in file; 0x%zX bytes */",
            floor, sf.enemy_sets_file_offset, sf.enemy_sets_file_size));
      }
      for (size_t z = 0; z < sf.enemy_set_count; z++) {
        if (reassembly) {
          ret.emplace_back(sf.enemy_sets[z].str());
        } else {
          size_t s_id = z + sf.first_enemy_set_index;
          ret.emplace_back(phosg::string_printf("/* S-%03zX */ ", s_id) + sf.enemy_sets[z].str());
        }
      }
    }
    if (sf.events1) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".events %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(".events %hhu /* 0x%zX in file; 0x%zX bytes; 0x%zX bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events1[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          size_t w_id = z + sf.first_event_set_index;
          ret.emplace_back(phosg::string_printf("/* W-%03zX */ ", w_id) + ev.str());
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(phosg::string_printf(
              "  // WARNING: Event action stream offset (0x%" PRIX32 ") is outside of this section",
              ev.action_stream_offset.load()));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.events2) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".random_events %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(
            ".random_events %hhu /* 0x%zX in file; 0x%zX bytes; 0x%zX bytes in action stream */",
            floor, sf.events_file_offset, sf.events_file_size, sf.event_action_stream_bytes));
      }
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events2[z];
        if (reassembly) {
          ret.emplace_back(ev.str());
        } else {
          ret.emplace_back(phosg::string_printf("/* index %zu */", z) + ev.str());
        }
        if (ev.action_stream_offset >= sf.event_action_stream_bytes) {
          ret.emplace_back(phosg::string_printf(
              "  // WARNING: Event action stream offset (0x%" PRIX32 ") is outside of this section",
              ev.action_stream_offset.load()));
        }
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.random_enemy_locations_data) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".random_enemy_locations %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(".random_enemy_locations %hhu /* 0x%zX in file; 0x%zX bytes */",
            floor, sf.random_enemy_locations_file_offset, sf.random_enemy_locations_file_size));
      }
      ret.emplace_back(phosg::format_data(sf.random_enemy_locations_data, sf.random_enemy_locations_data_size));
    }
    if (sf.random_enemy_definitions_data) {
      if (reassembly) {
        ret.emplace_back(phosg::string_printf(".random_enemy_definitions %hhu", floor));
      } else {
        ret.emplace_back(phosg::string_printf(".random_enemy_definitions %hhu /* 0x%zX in file; 0x%zX bytes */",
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
  return phosg::string_printf("KS-%02hhX-%03zX", this->floor, this->super_id);
}

string SuperMap::Object::str() const {
  string ret = "[Object " + this->id_str();
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_object_index != 0xFFFF) {
      string args_str = def.set_entry->str();
      ret += phosg::string_printf(
          " %s:[%04hX => %s]", phosg::name_for_enum(v), def.relative_object_index, args_str.c_str());
    }
  }
  ret += "]";
  return ret;
}

string SuperMap::Enemy::id_str() const {
  return phosg::string_printf("ES-%02hhX-%03zX-%03zX", this->floor, this->super_set_id, this->super_id);
}

string SuperMap::Enemy::str() const {
  string ret = phosg::string_printf("[Enemy ES-%02hhX-%03zX-%03zX type=%s child_index=%hX alias_enemy_index_delta=%hX is_default_rare_v123=%s is_default_rare_bb=%s",
      this->floor,
      this->super_set_id,
      this->super_id,
      phosg::name_for_enum(this->type),
      this->child_index,
      this->alias_enemy_index_delta,
      this->is_default_rare_v123 ? "true" : "false",
      this->is_default_rare_bb ? "true" : "false");
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_enemy_index != 0xFFFF) {
      string args_str = def.set_entry->str();
      ret += phosg::string_printf(
          " %s:[%04hX/%04hX => %s]",
          phosg::name_for_enum(v),
          def.relative_set_index,
          def.relative_enemy_index,
          args_str.c_str());
    }
  }
  ret += "]";
  return ret;
}

string SuperMap::Event::id_str() const {
  return phosg::string_printf("WS-%02hhX-%03zX", this->floor, this->super_id);
}

string SuperMap::Event::str() const {
  string ret = "[Event " + this->id_str();
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_event_index != 0xFFFF) {
      string action_stream_str = phosg::format_data_string(def.action_stream, def.action_stream_size);
      string args_str = def.set_entry->str();
      ret += phosg::string_printf(
          " %s:[%04hX => %s+%s]",
          phosg::name_for_enum(v),
          def.relative_event_index,
          args_str.c_str(),
          action_stream_str.c_str());
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

  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
  uint32_t base_switch_flag = set_entry->iparam4;
  uint32_t num_switch_flags = 0;
  switch (set_entry->base_type) {
    case 0x01AB: // TODoorFourLightRuins
    case 0x01C0: // TODoorFourLightSpace
    case 0x0202: // TObjDoorJung
    case 0x0221: // TODoorFourLightSeabed
    case 0x0222: // TODoorFourLightSeabedU
      num_switch_flags = set_entry->iparam5;
      break;
    case 0x00C1: // TODoorCave01
    case 0x0100: // TODoorMachine01
      num_switch_flags = (4 - clamp<size_t>(set_entry->iparam5, 0, 4));
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
    Version version,
    uint8_t floor,
    const MapFile::EnemySetEntry* set_entry) {

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
      bool is_rare = (set_entry->iparam6.load() >= 1);
      add(EnemyType::HILDEBEAR, is_rare, is_rare);
      break;
    }
    case 0x0041: { // TObjEneLappy
      bool is_rare_v123 = (set_entry->iparam6 != 0);
      bool is_rare_bb = (set_entry->iparam6 & 1);
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
      add((set_entry->fparam2 >= 1) ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
      break;
    case 0x0044: { // TObjEneBeast
      static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
      add(types[clamp<int16_t>(set_entry->iparam6, 0, 2)]);
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
      add(types[clamp<int16_t>(set_entry->iparam6, 0, 2)]);
      break;
    }
    case 0x0064: { // TObjEneSlime
      // Unlike all other versions, BB doesn't have a way to force slimes to be
      // rare via constructor args
      bool is_rare_v123 = (set_entry->iparam7 & 1);
      default_num_children = -1; // Skip adding children later (because we do it here)
      size_t num_children = set_entry->num_children ? set_entry->num_children.load() : 4;
      for (size_t z = 0; z < num_children + 1; z++) {
        add(EnemyType::POFUILLY_SLIME, is_rare_v123, false);
      }
      break;
    }
    case 0x0065: // TObjEnePanarms
      if ((set_entry->num_children != 0) && (set_entry->num_children != 2)) {
        this->log.warning("PAN_ARMS has an unusual num_children (0x%hX)", set_entry->num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::PAN_ARMS);
      add(EnemyType::HIDOOM);
      add(EnemyType::MIGIUM);
      break;
    case 0x0080: // TObjEneDubchik
      add((set_entry->iparam6 != 0) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
      break;
    case 0x0081: // TObjEneGyaranzo
      add(EnemyType::GARANZ);
      break;
    case 0x0082: // TObjEneMe3ShinowaReal
      add((set_entry->fparam2 >= 1) ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT);
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
        this->log.warning("CHAOS_SORCERER has an unusual num_children (0x%hX)", set_entry->num_children.load());
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
      add(EnemyType::DEATH_GUNNER);
      break;
    case 0x00A4: // TObjEneDf2Bringer
      add(EnemyType::CHAOS_BRINGER);
      break;
    case 0x00A5: // TObjEneRe7Berura
      add(EnemyType::DARK_BELRA);
      break;
    case 0x00A6: { // TObjEneDimedian
      static const EnemyType types[3] = {EnemyType::DIMENIAN, EnemyType::LA_DIMENIAN, EnemyType::SO_DIMENIAN};
      add(types[clamp<int16_t>(set_entry->iparam6, 0, 2)]);
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
        this->log.warning("DE_ROL_LE has an unusual num_children (0x%hX)", set_entry->num_children.load());
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
        this->log.warning("VOL_OPT has an unusual num_children (0x%hX)", set_entry->num_children.load());
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
        this->log.warning("DARK_FALZ has an unusual num_children (0x%hX)", set_entry->num_children.load());
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
      add((set_entry->iparam6 > 0) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL);
      default_num_children = 4;
      break;
    case 0x00D5: // TObjEneMerillLia
      add((set_entry->iparam6 > 0) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      break;
    case 0x00D6: { // TObjEneBm9Mericarol
      switch (set_entry->iparam6) {
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
      break;
    }
    case 0x00D7: // TObjEneBm5GibonU
      add((set_entry->iparam6 > 0) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
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
      add((set_entry->iparam6 > 0) ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
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
        add((set_entry->iparam6 > 0) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
      }
      break;
    case 0x00E1: // TObjEneIllGill
      add(EnemyType::ILL_GILL);
      break;
    case 0x0110:
      add(EnemyType::ASTARK);
      break;
    case 0x0111:
      if (floor > 0x05) {
        add(set_entry->fparam2 ? EnemyType::YOWIE_DESERT : EnemyType::SATELLITE_LIZARD_DESERT);
      } else {
        add(set_entry->fparam2 ? EnemyType::YOWIE_CRATER : EnemyType::SATELLITE_LIZARD_CRATER);
      }
      break;
    case 0x0112: {
      bool is_rare = (set_entry->iparam6 & 1);
      add(EnemyType::MERISSA_A, is_rare, is_rare);
      break;
    }
    case 0x0113:
      add(EnemyType::GIRTABLULU);
      break;
    case 0x0114: {
      bool is_rare = (set_entry->iparam6 & 1);
      add((floor > 0x05) ? EnemyType::ZU_DESERT : EnemyType::ZU_CRATER, is_rare, is_rare);
      break;
    }
    case 0x0115: {
      static const EnemyType types[3] = {EnemyType::BOOTA, EnemyType::ZE_BOOTA, EnemyType::BA_BOOTA};
      add(types[clamp<int16_t>(set_entry->iparam6, 0, 2)]);
      break;
    }
    case 0x0116: {
      bool is_rare = (set_entry->iparam6 & 1);
      add(EnemyType::DORPHON, is_rare, is_rare);
      break;
    }
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      add(types[clamp<int16_t>(set_entry->iparam6, 0, 2)]);
      break;
    }
    case 0x0119:
      // There isn't a way to force the Episode 4 boss to be rare via
      // constructor args
      add((set_entry->iparam6 & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILION);
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
      this->log.warning("Invalid enemy type %04hX", set_entry->base_type.load());
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
      case 0x0A: // enable_switch_flag(uint16_t flag_num)
      case 0x0B: // disable_switch_flag(uint16_t flag_num)
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
    throw runtime_error(phosg::string_printf(
        "action stream offset 0x%" PRIX32 " is beyond end of action stream (0x%zX) for event %s",
        entry->action_stream_offset.load(), map_file_action_stream_size, s.c_str()));
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
          fprintf(stream, "  %c %03.2g", action_ch, entry.cost);
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
      ((prev.fparam1 != current.fparam1) * 10.0) +
      ((prev.fparam2 != current.fparam2) * 10.0) +
      ((prev.fparam3 != current.fparam3) * 10.0) +
      ((prev.iparam4 != current.iparam4) * 10.0) +
      ((prev.iparam5 != current.iparam5) * 10.0) +
      ((prev.iparam6 != current.iparam6) * 10.0));
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
      ((prev.fparam1 != current.fparam1) * 10.0) +
      ((prev.fparam2 != current.fparam2) * 10.0) +
      ((prev.fparam3 != current.fparam3) * 10.0) +
      ((prev.fparam4 != current.fparam4) * 10.0) +
      ((prev.fparam5 != current.fparam5) * 10.0) +
      ((prev.iparam6 != current.iparam6) * 10.0) +
      ((prev.iparam7 != current.iparam7) * 10.0));
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
  return phosg::string_printf(
      "EfficiencyStats[K = %zu/%zu (%lg%%), E = %zu/%zu (%lg%%), W = %zu/%zu (%g%%)]",
      this->filled_object_slots, this->total_object_slots, object_eff,
      this->filled_enemy_set_slots, this->total_enemy_set_slots, enemy_set_eff,
      this->filled_event_slots, this->total_event_slots, event_eff);
}

SuperMap::EfficiencyStats SuperMap::efficiency() const {
  EfficiencyStats ret;

  for (const auto& obj : this->objects) {
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& obj_ver = obj->version(v);
      if (obj_ver.relative_object_index != 0xFFFF) {
        ret.filled_object_slots++;
      }
    }
  }
  ret.total_object_slots = this->objects.size() * ALL_ARPG_SEMANTIC_VERSIONS.size();

  for (const auto& ene : this->enemy_sets) {
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ene_ver = ene->version(v);
      if (ene_ver.relative_enemy_index != 0xFFFF) {
        ret.filled_enemy_set_slots++;
      }
    }
  }
  ret.total_enemy_set_slots = this->enemy_sets.size() * ALL_ARPG_SEMANTIC_VERSIONS.size();

  for (const auto& ev : this->events) {
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ev_ver = ev->version(v);
      if (ev_ver.relative_event_index != 0xFFFF) {
        ret.filled_event_slots++;
      }
    }
  }
  ret.total_event_slots = this->events.size() * ALL_ARPG_SEMANTIC_VERSIONS.size();

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
        throw logic_error(phosg::string_printf(
            "enemy super_set_id is incorrect; expected S-%03zX, received S-%03zX",
            super_set_id, ene->super_set_id));
      }
    }
    if (super_set_id != this->enemy_sets.size() - 1) {
      throw logic_error(phosg::string_printf(
          "not all enemy sets are in the enemies list; ended with 0x%zX, expected 0x%zX",
          super_set_id, this->enemy_sets.size()));
    }
  }
  for (size_t super_id = 0; super_id < this->events.size(); super_id++) {
    if (this->events[super_id]->super_id != super_id) {
      throw logic_error("event super_id is incorrect");
    }
  }

  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
  fprintf(stream, "SuperMap %s random=%08" PRIX64 "\n", name_for_episode(this->episode), this->random_seed);

  fprintf(stream, "               DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 XBV3 BBV4\n");
  fprintf(stream, "  MAP         ");
  for (const auto& v : ALL_ARPG_SEMANTIC_VERSIONS) {
    const auto& entities = this->version(v);
    fprintf(stream, " %s", entities.map_file ? "++++" : "----");
  }
  fputc('\n', stream);
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    fprintf(stream, "  KS  START %02hhX", floor);
    for (const auto& v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& entities = this->version(v);
      fprintf(stream, " %04zX", entities.object_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    fprintf(stream, "  ES  START %02hhX", floor);
    for (const auto& v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& entities = this->version(v);
      fprintf(stream, " %04zX", entities.enemy_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    fprintf(stream, "  ESS START %02hhX", floor);
    for (const auto& v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& entities = this->version(v);
      fprintf(stream, " %04zX", entities.enemy_set_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }
  for (uint8_t floor = 0; floor < 0x12; floor++) {
    fprintf(stream, "  WS  START %02hhX", floor);
    for (const auto& v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& entities = this->version(v);
      fprintf(stream, " %04zX", entities.event_floor_start_indexes[floor]);
    }
    fputc('\n', stream);
  }

  fprintf(stream, "  KS-FL-ID  DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 XBV3 BBV4 DEFINITION\n");
  for (const auto& obj : this->objects) {
    fprintf(stream, "  KS-%02hhX-%03zX", obj->floor, obj->super_id);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& obj_ver = obj->version(v);
      if (obj_ver.relative_object_index == 0xFFFF) {
        fprintf(stream, " ----");
      } else {
        fprintf(stream, " %04hX", obj_ver.relative_object_index);
      }
    }
    auto obj_str = obj->str();
    fprintf(stream, " %s\n", obj_str.c_str());
  }

  fprintf(stream, "  ES-FL-ID  DCTE----- DCPR----- DCV1----- DCV2----- PCTE----- PCV2----- GCTE----- GCV3----- XBV3----- BBV4----- DEFINITION\n");
  for (const auto& ene : this->enemies) {
    fprintf(stream, "  ES-%02hhX-%03zX", ene->floor, ene->super_id);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ene_ver = ene->version(v);
      if (ene_ver.relative_enemy_index == 0xFFFF) {
        fprintf(stream, " ----:----");
      } else {
        fprintf(stream, " %04hX:%04hX", ene_ver.relative_set_index, ene_ver.relative_enemy_index);
      }
    }

    auto ene_str = ene->str();
    fprintf(stream, " %s\n", ene_str.c_str());
  }

  fprintf(stream, "  WS-FL-ID  DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 XBV3 BBV4 DEFINITION\n");
  for (const auto& ev : this->events) {
    fprintf(stream, "  WS-%02hhX-%03zX", ev->floor, ev->super_id);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ev_ver = ev->version(v);
      if (ev_ver.relative_event_index == 0xFFFF) {
        fprintf(stream, " ----");
      } else {
        fprintf(stream, " %04hX", ev_ver.relative_event_index);
      }
    }
    auto ev_str = ev->str();
    fprintf(stream, " %s\n", ev_str.c_str());
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
  return phosg::string_printf("RareEnemyRates(hildeblue=%08" PRIX32 ", rappy=%08" PRIX32 ", nar_lily=%08" PRIX32 ", pouilly_slime=%08" PRIX32 ", mericarand=%08" PRIX32 ", merissa_aa=%08" PRIX32 ", pazuzu=%08" PRIX32 ", dorphon_eclair=%08" PRIX32 ", kondrieu=%08" PRIX32 ")",
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

uint32_t MapState::RareEnemyRates::for_enemy_type(EnemyType type) const {
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

const shared_ptr<const MapState::RareEnemyRates> MapState::NO_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(
    0, 0, 0);
const shared_ptr<const MapState::RareEnemyRates> MapState::DEFAULT_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(
    MapState::RareEnemyRates::DEFAULT_RARE_ENEMY_RATE_V3,
    MapState::RareEnemyRates::DEFAULT_MERICARAND_RATE_V3,
    MapState::RareEnemyRates::DEFAULT_RARE_BOSS_RATE_V4);

uint32_t MapState::EnemyState::convert_game_flags(uint32_t game_flags, bool to_v3) {
  // The format of game_flags was changed significantly between v2 and v3, and
  // not accounting for this results in odd effects like other characters not
  // appearing when joining a game. Unfortunately, some bits were deleted on v3
  // and other bits were added, so it doesn't suffice to simply store the most
  // complete format of this field - we have to be able to convert between the
  // two.

  // Bits on v2: ?IHCBAzy xwvutsrq ponmlkji hgfedcba
  // Bits on v3: ?IHGFEDC BAzyxwvu srqponkj hgfedcba
  // The bits ilmt were removed in v3 and the bits to their left were shifted
  // right. The bits DEFG were added in v3 and do not exist on v2.
  // Known meanings for these bits:
  //   o = is dead
  //   n = should play hit animation
  //   y = is near enemy
  //   H = is enemy?
  //   I = is object? (some entities have both H and I set though)
  // It could be that the flags 0x70000000 are actually a 3-bit integer rather
  // than individual flags. TODO: Investigate this.

  if (to_v3) {
    return (game_flags & 0xE00000FF) |
        ((game_flags & 0x00000600) >> 1) |
        ((game_flags & 0x0007E000) >> 3) |
        ((game_flags & 0x1FF00000) >> 4);
  } else {
    return (game_flags & 0xE00000FF) |
        ((game_flags << 1) & 0x00000600) |
        ((game_flags << 3) & 0x0007E000) |
        ((game_flags << 4) & 0x1FF00000);
  }
}

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
    uint8_t difficulty,
    uint8_t event,
    uint32_t random_seed,
    std::shared_ptr<const RareEnemyRates> bb_rare_rates,
    std::shared_ptr<PSOLFGEncryption> opt_rand_crypt,
    std::vector<std::shared_ptr<const SuperMap>> floor_map_defs)
    : log(phosg::string_printf("[MapState(free):%08" PRIX64 "] ", lobby_or_session_id), lobby_log.min_level),
      difficulty(difficulty),
      event(event),
      random_seed(random_seed),
      bb_rare_rates(bb_rare_rates) {

  this->floor_config_entries.resize(0x12);
  for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
    auto& this_fc = this->floor_config_entries[floor];
    this_fc.super_map = (floor < floor_map_defs.size()) ? floor_map_defs[floor] : nullptr;
    if (this_fc.super_map) {
      this->index_super_map(this_fc, opt_rand_crypt);
    }

    if (floor < this->floor_config_entries.size() - 1) {
      auto& next_fc = this->floor_config_entries[floor + 1];
      if (this_fc.super_map) {
        for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
        for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
    uint8_t difficulty,
    uint8_t event,
    uint32_t random_seed,
    std::shared_ptr<const RareEnemyRates> bb_rare_rates,
    std::shared_ptr<PSOLFGEncryption> opt_rand_crypt,
    std::shared_ptr<const SuperMap> quest_map_def)
    : log(phosg::string_printf("[MapState(free):%08" PRIX64 "] ", lobby_or_session_id), lobby_log.min_level),
      difficulty(difficulty),
      event(event),
      random_seed(random_seed),
      bb_rare_rates(bb_rare_rates) {
  FloorConfig& fc = this->floor_config_entries.emplace_back();
  fc.super_map = quest_map_def;
  this->index_super_map(fc, opt_rand_crypt);
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

void MapState::index_super_map(const FloorConfig& fc, shared_ptr<PSOLFGEncryption> opt_rand_crypt) {
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
        type = ((this->difficulty == 0) && (ene->alias_enemy_index_delta == 0))
            ? EnemyType::DARK_FALZ_2
            : EnemyType::DARK_FALZ_3;
        break;
      case EnemyType::DARVANT:
        type = (this->difficulty == 3) ? EnemyType::DARVANT_ULTIMATE : EnemyType::DARVANT;
        break;
      default:
        type = ene->type;
    }

    auto rare_type = type_definition_for_enemy(type).rare_type(fc.super_map->get_episode(), this->event, ene->floor);
    if ((type == EnemyType::MERICARAND) || (rare_type != type)) {
      unordered_map<uint32_t, float> det_cache;
      uint32_t bb_rare_rate = this->bb_rare_rates->for_enemy_type(type);
      for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
            // On v3, Mericarols that have iparam6 > 2 are randomized to be
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

        } else if ((bb_rare_rate > 0) &&
            (this->bb_rare_enemy_indexes.size() < 0x10) &&
            (random_from_optional_crypt(opt_rand_crypt) < bb_rare_rate)) {
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
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
  this->log.info("Importing object state from sync command");
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
        throw runtime_error(phosg::string_printf(
            "the map has more objects (at least 0x%zX) than the client has (0x%zX)",
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
        this->log.warning("(%04zX => K-%03zX) Game flags from client (%04hX) do not match game flags from map (%04hX)",
            object_index, obj_st->k_id, entry.flags.load(), obj_st->game_flags);
        obj_st->game_flags = entry.flags;
      }
    }
  }
  if (object_index < entry_count) {
    throw runtime_error(phosg::string_printf("the client has more objects (0x%zX) than the map has (0x%zX)",
        entry_count, object_index));
  }
}

void MapState::import_enemy_states_from_sync(Version from_version, const SyncEnemyStateEntry* entries, size_t entry_count) {
  this->log.info("Importing enemy state from sync command");
  size_t enemy_index = 0;
  bool is_v3 = !is_v1_or_v2(from_version);
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
      throw runtime_error(phosg::string_printf("the map has more enemies than the client has (0x%zX)", entry_count));
    }
    for (; enemy_index < min<size_t>(fc_end_enemy_index, entry_count); enemy_index++) {
      const auto& entry = entries[enemy_index];
      const auto& ene = entities.enemies.at(enemy_index - base_indexes.base_enemy_index);
      auto& ene_st = this->enemy_states.at(fc.base_super_ids.base_enemy_index + ene->super_id);
      if (ene_st->super_ene != ene) {
        throw logic_error("super enemy link is incorrect");
      }
      if (ene_st->get_game_flags(is_v3) != entry.flags) {
        this->log.warning("(%04zX => E-%03zX) Flags from client (%08" PRIX32 "(%s)) do not match game flags from map (%08" PRIX32 "(%s))",
            enemy_index,
            ene_st->e_id,
            entry.flags.load(),
            is_v3 ? "v3" : "v2",
            ene_st->game_flags,
            (ene_st->server_flags & MapState::EnemyState::Flag::GAME_FLAGS_IS_V3) ? "v3" : "v2");
        ene_st->set_game_flags(entry.flags, !is_v1_or_v2(from_version));
      }
      if (ene_st->total_damage != entry.total_damage) {
        this->log.warning("(%04zX => E-%03zX) Total damage from client (%hu) does not match total damage from map (%hu)",
            enemy_index, ene_st->e_id, entry.total_damage.load(), ene_st->total_damage);
        ene_st->total_damage = entry.total_damage;
      }
    }
  }
  if (enemy_index < entry_count) {
    throw runtime_error(phosg::string_printf("the client has more enemies (0x%zX) than the map has (0x%zX)",
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
    this->log.info("Importing object set flags from sync command");
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
          throw runtime_error(phosg::string_printf(
              "the map has more objects (at least 0x%zX) than the client has (0x%zX)",
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
          this->log.warning("(%04zX => K-%03zX) Set flags from client (%04hX) do not match set flags from map (%04hX)",
              object_index, obj_st->k_id, set_flags, obj_st->set_flags);
          obj_st->set_flags = set_flags;
        }
      }
    }
    if (object_index < object_set_flags_count) {
      throw runtime_error(phosg::string_printf("the client has more objects (0x%zX) than the map has (0x%zX)",
          object_set_flags_count, object_index));
    }
  }

  {
    this->log.info("Importing enemy set flags from sync command");
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
          this->log.warning("(%04zX => E-%03zX) Set flags from client (%04hX) do not match set flags from map (%04hX)",
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
    this->log.info("Importing event flags from sync command");
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
          this->log.warning("(%04zX => W-%03zX) Set flags from client (%04hX) do not match flags from map (%04hX)",
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
      throw logic_error(phosg::string_printf(
          "map state object count (0x%zX) does not match supermap object count (0x%zX)",
          this->object_states.size(), total_object_count));
    }
    if (this->enemy_states.size() != total_enemy_count) {
      throw logic_error(phosg::string_printf(
          "map state enemy count (0x%zX) does not match supermap enemy count (0x%zX)",
          this->enemy_states.size(), total_enemy_count));
    }
    if (this->enemy_set_states.size() != total_enemy_set_count) {
      throw logic_error(phosg::string_printf(
          "map state enemy set count (0x%zX) does not match supermap enemy set count (0x%zX)",
          this->enemy_set_states.size(), total_enemy_set_count));
    }
    if (this->event_states.size() != total_event_count) {
      throw logic_error(phosg::string_printf(
          "map state event count (0x%zX) does not match supermap event count (0x%zX)",
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

    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
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
        throw logic_error(phosg::string_printf("BB random rare enemy index %04zX not present in indexes set", enemy_index));
      }
    }
    if (!remaining_bb_rare_indexes.empty()) {
      vector<string> indexes;
      for (uint16_t index : remaining_bb_rare_indexes) {
        indexes.emplace_back(phosg::string_printf("%04hX", index));
      }
      throw logic_error("not all BB random rare enemies were accounted for; remaining: " + phosg::join(indexes, ", "));
    }
  } catch (const exception&) {
    this->print(stderr);
    throw;
  }
}

void MapState::print(FILE* stream) const {
  fprintf(stream, "Difficulty %s, event %02hhX, state random seed %08" PRIX32 "\n",
      name_for_difficulty(this->difficulty), this->event, this->random_seed);
  auto rare_rates_str = this->bb_rare_rates->str();
  fprintf(stream, "BB rare rates: %s\n", rare_rates_str.c_str());

  fprintf(stream, "Base indexes:\n");
  fprintf(stream, "  FL DCTE----------- DCPR----------- DCV1----------- DCV2----------- PCTE----------- PCV2----------- GCTE----------- GCV3----------- XBV3----------- BBV4-----------\n");
  fprintf(stream, "  FL KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT KST EST ESS EVT\n");
  for (size_t floor = 0; floor < this->floor_config_entries.size(); floor++) {
    auto fc = this->floor_config_entries[floor];
    if (fc.super_map) {
      fprintf(stream, "  %02zX", floor);
      for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
        const auto& indexes = fc.base_indexes_for_version(v);
        fprintf(stream, " %03zX %03zX %03zX %03zX", indexes.base_object_index, indexes.base_enemy_index, indexes.base_enemy_set_index, indexes.base_event_index);
      }
      fputc('\n', stream);
    } else {
      fprintf(stream, "  %02zX --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- --------------- ---------------\n", floor);
    }
  }

  fprintf(stream, "Objects:\n");
  fprintf(stream, "  FL OBJID DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 XBV3 BBV4 OBJECT\n");
  for (const auto& obj_st : this->object_states) {
    fprintf(stream, "  %02hhX K-%03zX", obj_st->super_obj->floor, obj_st->k_id);
    const auto& fc = this->floor_config(obj_st->super_obj->floor);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& obj_v = obj_st->super_obj->version(v);
      if (obj_v.relative_object_index == 0xFFFF) {
        fputs(" ----", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_object_index + obj_v.relative_object_index;
        fprintf(stream, " %04hX", index);
      }
    }
    string obj_str = obj_st->super_obj->str();
    fprintf(stream, " %s game_flags=%04hX set_flags=%04hX item_drop_checked=%s\n",
        obj_str.c_str(), obj_st->game_flags, obj_st->set_flags, obj_st->item_drop_checked ? "true" : "false");
  }

  fprintf(stream, "Enemies:\n");
  fprintf(stream, "  FL ENEID DCTE----- DCPR----- DCV1----- DCV2----- PCTE----- PCV2----- GCTE----- GCV3----- XBV3----- BBV4----- ENEMY\n");
  for (const auto& ene_st : this->enemy_states) {
    fprintf(stream, "  %02hhX E-%03zX", ene_st->super_ene->floor, ene_st->e_id);
    const auto& fc = this->floor_config(ene_st->super_ene->floor);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ene_v = ene_st->super_ene->version(v);
      if (ene_v.relative_enemy_index == 0xFFFF) {
        fputs(" ---------", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_enemy_index + ene_v.relative_enemy_index;
        uint16_t set_index = fc.base_indexes_for_version(v).base_enemy_set_index + ene_v.relative_set_index;
        fprintf(stream, " %04hX-%04hX", index, set_index);
      }
    }
    string ene_str = ene_st->super_ene->str();
    fprintf(stream, " %s total_damage=%04hX rare_flags=%04hX game_flags=%08" PRIX32 "(%s) set_flags=%04hX server_flags=%04hX\n",
        ene_str.c_str(),
        ene_st->total_damage,
        ene_st->rare_flags,
        ene_st->game_flags,
        (ene_st->server_flags & MapState::EnemyState::Flag::GAME_FLAGS_IS_V3) ? "v3" : "v2",
        ene_st->set_flags,
        ene_st->server_flags);
  }

  if (this->bb_rare_enemy_indexes.empty()) {
    fprintf(stream, "BB rare enemy indexes: (none)\n");
  } else {
    string s;
    for (auto index : this->bb_rare_enemy_indexes) {
      s += phosg::string_printf(" %04zX", index);
    }
    fprintf(stream, "BB rare enemy indexes:%s\n", s.c_str());
  }

  fprintf(stream, "Events:\n");
  fprintf(stream, "  FL EVTID DCTE DCPR DCV1 DCV2 PCTE PCV2 GCTE GCV3 XBV3 BBV4 EVENT\n");
  for (const auto& ev_st : this->event_states) {
    fprintf(stream, "  %02hhX W-%03zX", ev_st->super_ev->floor, ev_st->w_id);
    const auto& fc = this->floor_config(ev_st->super_ev->floor);
    for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
      const auto& ev_v = ev_st->super_ev->version(v);
      if (ev_v.relative_event_index == 0xFFFF) {
        fputs(" ----", stream);
      } else {
        uint16_t index = fc.base_indexes_for_version(v).base_event_index + ev_v.relative_event_index;
        fprintf(stream, " %04hX", index);
      }
    }
    string ev_str = ev_st->super_ev->str();
    fprintf(stream, " %s set_flags=%04hX\n", ev_str.c_str(), ev_st->flags);
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
