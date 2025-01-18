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
      {0x0000, "TObjPlayerSet"},
      {0x0001, "TObjParticle"},
      {0x0002, "TObjAreaWarpForest"},
      {0x0003, "TObjMapWarpForest"},
      {0x0004, "TObjLight"},
      {0x0006, "TObjEnvSound"},
      {0x0007, "TObjFogCollision"},
      {0x0008, "TObjEvtCollision"},
      {0x0009, "TObjCollision"},
      {0x000A, "TOMineIcon01"},
      {0x000B, "TOMineIcon02"},
      {0x000C, "TOMineIcon03"},
      {0x000D, "TOMineIcon04"},
      {0x000E, "TObjRoomId"},
      {0x000F, "TOSensorGeneral01"},
      {0x0011, "TEF_LensFlare"},
      {0x0012, "TObjQuestCol"},
      {0x0013, "TOHealGeneral"},
      {0x0014, "TObjMapCsn"},
      {0x0015, "TObjQuestColA"},
      {0x0016, "TObjItemLight"},
      {0x0017, "TObjRaderCol"},
      {0x0018, "TObjFogCollisionSwitch"},
      {0x0019, "TObjWarpBossMulti(off)/TObjWarpBoss(on)"},
      {0x001A, "TObjSinBoard"},
      {0x001B, "TObjAreaWarpQuest"},
      {0x001C, "TObjAreaWarpEnding"},
      {0x001D, "__UNNAMED_001D__"},
      {0x001E, "__UNNAMED_001E__"},
      {0x001F, "TObjRaderHideCol"},
      {0x0020, "TOSwitchItem"},
      {0x0021, "TOSymbolchatColli"},
      {0x0022, "TOKeyCol"},
      {0x0023, "TOAttackableCol"},
      {0x0024, "TOSwitchAttack"},
      {0x0025, "TOSwitchTimer"},
      {0x0026, "TOChatSensor"},
      {0x0027, "TObjRaderIcon"},
      {0x0028, "TObjEnvSoundEx"},
      {0x0029, "TObjEnvSoundGlobal"},
      {0x0040, "TShopGenerator"},
      {0x0041, "TObjLuker"},
      {0x0042, "TObjBgmCol"},
      {0x0043, "TObjCityMainWarp"},
      {0x0044, "TObjCityAreaWarp"},
      {0x0045, "TObjCityMapWarp"},
      {0x0046, "TObjCityDoor_Shop"},
      {0x0047, "TObjCityDoor_Guild"},
      {0x0048, "TObjCityDoor_Warp"},
      {0x0049, "TObjCityDoor_Med"},
      {0x004A, "__UNNAMED_004A__"},
      {0x004B, "TObjCity_Season_EasterEgg"},
      {0x004C, "TObjCity_Season_ValentineHeart"},
      {0x004D, "TObjCity_Season_XmasTree"},
      {0x004E, "TObjCity_Season_XmasWreath"},
      {0x004F, "TObjCity_Season_HalloweenPumpkin"},
      {0x0050, "TObjCity_Season_21_21"},
      {0x0051, "TObjCity_Season_SonicAdv2"},
      {0x0052, "TObjCity_Season_Board"},
      {0x0053, "TObjCity_Season_FireWorkCtrl"},
      {0x0054, "TObjCityDoor_Lobby"},
      {0x0055, "TObjCityMainWarpChallenge"},
      {0x0056, "TODoorLabo"},
      {0x0057, "TObjTradeCollision"},
      {0x0080, "TObjDoor"},
      {0x0081, "TObjDoorKey"},
      {0x0082, "TObjLazerFenceNorm"},
      {0x0083, "TObjLazerFence4"},
      {0x0084, "TLazerFenceSw"},
      {0x0085, "TKomorebi"},
      {0x0086, "TButterfly"},
      {0x0087, "TMotorcycle"},
      {0x0088, "TObjContainerItem"},
      {0x0089, "TObjTank"},
      {0x008B, "TObjComputer"},
      {0x008C, "TObjContainerIdo"},
      {0x008D, "TOCapsuleAncient01"},
      {0x008E, "TOBarrierEnergy01"},
      {0x008F, "TObjHashi"},
      {0x0090, "TOKeyGenericSw"},
      {0x0091, "TObjContainerEnemy"},
      {0x0092, "TObjContainerBase"},
      {0x0093, "TObjContainerAbeEnemy"},
      {0x0095, "TObjContainerNoItem"},
      {0x0096, "TObjLazerFenceExtra"},
      {0x00C0, "TOKeyCave01"},
      {0x00C1, "TODoorCave01"},
      {0x00C2, "TODoorCave02"},
      {0x00C3, "TOHangceilingCave01Key/TOHangceilingCave01Normal/TOHangceilingCave01KeyQuick"},
      {0x00C4, "TOSignCave01"},
      {0x00C5, "TOSignCave02"},
      {0x00C6, "TOSignCave03"},
      {0x00C7, "TOAirconCave01"},
      {0x00C8, "TOAirconCave02"},
      {0x00C9, "TORevlightCave01"},
      {0x00CB, "TORainbowCave01"},
      {0x00CC, "TOKurage"},
      {0x00CD, "TODragonflyCave01"},
      {0x00CE, "TODoorCave03"},
      {0x00CF, "TOBind"},
      {0x00D0, "TOCakeshopCave01"},
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
      {0x00DE, "TODummyKeyCave01"},
      {0x00DF, "TORockCaveBL01"},
      {0x00E0, "TORockCaveBL02"},
      {0x00E1, "TORockCaveBL03"},
      {0x0100, "TODoorMachine01"},
      {0x0101, "TOKeyMachine01"},
      {0x0102, "TODoorMachine02"},
      {0x0103, "TOCapsuleMachine01"},
      {0x0104, "TOComputerMachine01"},
      {0x0105, "TOMonitorMachine01"},
      {0x0106, "TODragonflyMachine01"},
      {0x0107, "TOLightMachine01"},
      {0x0108, "TOExplosiveMachine01"},
      {0x0109, "TOExplosiveMachine02"},
      {0x010A, "TOExplosiveMachine03"},
      {0x010B, "TOSparkMachine01"},
      {0x010C, "TOHangerMachine01"},
      {0x0130, "TODoorVoShip"},
      {0x0140, "TObjGoalWarpAncient"},
      {0x0141, "TObjMapWarpAncient"},
      {0x0142, "TOKeyAncient02"},
      {0x0143, "TOKeyAncient03"},
      {0x0144, "TODoorAncient01"},
      {0x0145, "TODoorAncient03"},
      {0x0146, "TODoorAncient04"},
      {0x0147, "TODoorAncient05"},
      {0x0148, "TODoorAncient06"},
      {0x0149, "TODoorAncient07"},
      {0x014A, "TODoorAncient08"},
      {0x014B, "TODoorAncient09"},
      {0x014C, "TOSensorAncient01"},
      {0x014D, "TOKeyAncient01"},
      {0x014E, "TOFenceAncient01"},
      {0x014F, "TOFenceAncient02"},
      {0x0150, "TOFenceAncient03"},
      {0x0151, "TOFenceAncient04"},
      {0x0152, "TContainerAncient01"},
      {0x0153, "TOTrapAncient01"},
      {0x0154, "TOTrapAncient02"},
      {0x0155, "TOMonumentAncient01"},
      {0x0156, "TOMonumentAncient02"},
      {0x0159, "TOWreckAncient01"},
      {0x015A, "TOWreckAncient02"},
      {0x015B, "TOWreckAncient03"},
      {0x015C, "TOWreckAncient04"},
      {0x015D, "TOWreckAncient05"},
      {0x015E, "TOWreckAncient06"},
      {0x015F, "TOWreckAncient07"},
      {0x0160, "TObjFogCollisionPoison/TObjWarpBoss03"},
      {0x0161, "TOContainerAncientItemCommon"},
      {0x0162, "TOContainerAncientItemRare"},
      {0x0163, "TOContainerAncientEnemyCommon"},
      {0x0164, "TOContainerAncientEnemyRare"},
      {0x0165, "TOContainerAncientItemNone"},
      {0x0166, "TOWreckAncientBrakable05"},
      {0x0167, "TOTrapAncient02R"},
      {0x0170, "TOBoss4Bird"},
      {0x0171, "TOBoss4Tower"},
      {0x0172, "TOBoss4Rock"},
      {0x0180, "TObjInfoCol"},
      {0x0181, "TObjWarpLobby"},
      {0x0182, "TObjLobbyMain"},
      {0x0183, "__TObjPathObj_subclass_0183__"},
      {0x0184, "TObjButterflyLobby"},
      {0x0185, "TObjRainbowLobby"},
      {0x0186, "TObjKabochaLobby"},
      {0x0187, "TObjStendGlassLobby"},
      {0x0188, "TObjCurtainLobby"},
      {0x0189, "TObjWeddingLobby"},
      {0x018A, "TObjTreeLobby"},
      {0x018B, "TObjSuisouLobby"},
      {0x018C, "TObjParticleLobby"},
      {0x0190, "TObjCamera"},
      {0x0191, "TObjTuitate"},
      {0x0192, "TObjDoaEx01"},
      {0x0193, "TObjBigTuitate"},
      {0x01A0, "TODoorVS2Door01"},
      {0x01A1, "TOVS2Wreck01"},
      {0x01A2, "TOVS2Wreck02"},
      {0x01A3, "TOVS2Wreck03"},
      {0x01A4, "TOVS2Wreck04"},
      {0x01A5, "TOVS2Wreck05"},
      {0x01A6, "TOVS2Wreck06"},
      {0x01A7, "TOVS2Wall01"},
      {0x01A8, "__UNNAMED_01A8__"},
      {0x01A9, "TObjHashiVersus1"},
      {0x01AA, "TObjHashiVersus2"},
      {0x01AB, "TODoorFourLightRuins"},
      {0x01C0, "TODoorFourLightSpace"},
      {0x0200, "TObjContainerJung"},
      {0x0201, "TObjWarpJung"},
      {0x0202, "TObjDoorJung"},
      {0x0203, "TObjContainerJungEx"},
      {0x0204, "TODoorJungleMain"},
      {0x0205, "TOKeyJungleMain"},
      {0x0206, "TORockJungleS01"},
      {0x0207, "TORockJungleM01"},
      {0x0208, "TORockJungleL01"},
      {0x0209, "TOGrassJungle"},
      {0x020A, "TObjWarpJungMain"},
      {0x020B, "TBGLightningCtrl"},
      {0x020C, "__TObjPathObj_subclass_020C__"},
      {0x020D, "__TObjPathObj_subclass_020D__"},
      {0x020E, "TObjContainerJungEnemy"},
      {0x020F, "TOTrapChainSawDamage"},
      {0x0210, "TOTrapChainSawKey"},
      {0x0211, "TOBiwaMushi"},
      {0x0212, "__TObjPathObj_subclass_0212__"},
      {0x0213, "TOJungleDesign"},
      {0x0220, "TObjFish"},
      {0x0221, "TODoorFourLightSeabed"},
      {0x0222, "TODoorFourLightSeabedU"},
      {0x0223, "TObjSeabedSuiso_CH"},
      {0x0224, "TObjSeabedSuisoBrakable"},
      {0x0225, "TOMekaFish00"},
      {0x0226, "TOMekaFish01"},
      {0x0227, "__TObjPathObj_subclass_0227__"},
      {0x0228, "TOTrapSeabed01"},
      {0x0229, "TOCapsuleLabo"},
      {0x0240, "TObjParticle"},
      {0x0280, "__TObjAreaWarpForest_subclass_0280__"},
      {0x02A0, "TObjLiveCamera"},
      {0x02B0, "TContainerAncient01R"},
      {0x02B1, "TObjLaboDesignBase"},
      {0x02B2, "TObjLaboDesignBase"},
      {0x02B3, "TObjLaboDesignBase"},
      {0x02B4, "TObjLaboDesignBase"},
      {0x02B5, "TObjLaboDesignBase"},
      {0x02B6, "TObjLaboDesignBase"},
      {0x02B7, "TObjGbAdvance"},
      {0x02B8, "TObjQuestColALock2"},
      {0x02B9, "TObjMapForceWarp"},
      {0x02BA, "TObjQuestCol2"},
      {0x02BB, "TODoorLaboNormal"},
      {0x02BC, "TObjAreaWarpEndingJung"},
      {0x02BD, "TObjLaboMapWarp"},
      {0x0300, "__EP4_LIGHT__"},
      {0x0301, "__WILDS_CRATER_CACTUS__"},
      {0x0302, "__WILDS_CRATER_BROWN_ROCK__"},
      {0x0303, "__WILDS_CRATER_BROWN_ROCK_DESTRUCTIBLE__"},
      {0x0340, "__UNKNOWN_0340__"},
      {0x0341, "__UNKNOWN_0341__"},
      {0x0380, "__POISON_PLANT__"},
      {0x0381, "__UNKNOWN_0381__"},
      {0x0382, "__UNKNOWN_0382__"},
      {0x0383, "__DESERT_OOZE_PLANT__"},
      {0x0385, "__UNKNOWN_0385__"},
      {0x0386, "__WILDS_CRATER_BLACK_ROCKS__"},
      {0x0387, "__UNKNOWN_0387__"},
      {0x0388, "__UNKNOWN_0388__"},
      {0x0389, "__UNKNOWN_0389__"},
      {0x038A, "__UNKNOWN_038A__"},
      {0x038B, "__FALLING_ROCK__"},
      {0x038C, "__DESERT_PLANT_SOLID__"},
      {0x038D, "__DESERT_CRYSTALS_BOX__"},
      {0x038E, "__UNKNOWN_038E__"},
      {0x038F, "__BEE_HIVE__"},
      {0x0390, "__UNKNOWN_0390__"},
      {0x0391, "__HEAT__"},
      {0x03C0, "__EP4_BOSS_EGG__"},
      {0x03C1, "__UNKNOWN_03C1__"},
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
      {0x0033, "TObjNpcEnemy"},
      {0x0045, "TObjNpcLappy"},
      {0x0046, "TObjNpcMoja"},
      {0x00A9, "TObjNpcBringer"},
      {0x00D0, "TObjNpcKenkyu"},
      {0x00D1, "TObjNpcSoutokufu"},
      {0x00D2, "TObjNpcHosa"},
      {0x00D3, "TObjNpcKenkyuW"},
      {0x00F0, "TObjNpcHosa2"},
      {0x00F1, "TObjNpcKenkyu2"},
      {0x00F2, "TObjNpcNgcBase"},
      {0x00F3, "TObjNpcNgcBase"},
      {0x00F4, "TObjNpcNgcBase"},
      {0x00F5, "TObjNpcNgcBase"},
      {0x00F6, "TObjNpcNgcBase"},
      {0x00F7, "TObjNpcNgcBase"},
      {0x00F8, "TObjNpcNgcBase"},
      {0x00F9, "TObjNpcNgcBase"},
      {0x00FA, "TObjNpcNgcBase"},
      {0x00FB, "TObjNpcNgcBase"},
      {0x00FC, "TObjNpcNgcBase"},
      {0x00FD, "TObjNpcNgcBase"},
      {0x00FE, "TObjNpcNgcBase"},
      {0x00FF, "TObjNpcNgcBase"},
      {0x0100, "__UNKNOWN_NPC_0100__"},
      {0x0040, "TObjEneMoja"},
      {0x0041, "TObjEneLappy"},
      {0x0042, "TObjEneBm3FlyNest"},
      {0x0043, "TObjEneBm5Wolf"},
      {0x0044, "TObjEneBeast"},
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
      {0x00C0, "TBoss1Dragon/TBoss5Gryphon"},
      {0x00C1, "TBoss2DeRolLe"},
      {0x00C2, "TBoss3Volopt"},
      {0x00C3, "TBoss3VoloptP01"},
      {0x00C4, "TBoss3VoloptCore/SUBCLASS"},
      {0x00C5, "__TObjEnemyCustom_SUBCLASS__"},
      {0x00C6, "TBoss3VoloptMonitor"},
      {0x00C7, "TBoss3VoloptHiraisin"},
      {0x00C8, "TBoss4DarkFalz"},
      {0x00CA, "TBoss6PlotFalz"},
      {0x00CB, "TBoss7DeRolLeC"},
      {0x00CC, "TBoss8Dragon"},
      {0x00D4, "TObjEneMe3StelthReal"},
      {0x00D5, "TObjEneMerillLia"},
      {0x00D6, "TObjEneBm9Mericarol"},
      {0x00D7, "TObjEneBm5GibonU"},
      {0x00D8, "TObjEneGibbles"},
      {0x00D9, "TObjEneMe1Gee"},
      {0x00DA, "TObjEneMe1GiGue"},
      {0x00DB, "TObjEneDelDepth"},
      {0x00DC, "TObjEneDellBiter"},
      {0x00DD, "TObjEneDolmOlm"},
      {0x00DE, "TObjEneMorfos"},
      {0x00DF, "TObjEneRecobox"},
      {0x00E0, "TObjEneMe3SinowZoaReal/TObjEneEpsilonBody"},
      {0x00E1, "TObjEneIllGill"},
      {0x0110, "__ASTARK__"},
      {0x0111, "__YOWIE__/__SATELLITE_LIZARD__"},
      {0x0112, "__MERISSA_A__"},
      {0x0113, "__GIRTABLULU__"},
      {0x0114, "__ZU__"},
      {0x0115, "__BOOTA_FAMILY__"},
      {0x0116, "__DORPHON__"},
      {0x0117, "__GORAN_FAMILY__"},
      {0x0118, "__UNKNOWN_0118__"},
      {0x0119, "__EPISODE_4_BOSS__"},
  });
  try {
    return names.at(type);
  } catch (const out_of_range&) {
    return "__UNKNOWN__";
  }
}

string MapFile::ObjectSetEntry::str() const {
  string name_str = MapFile::name_for_object_type(this->base_type);
  return phosg::string_printf("[ObjectEntry type=%04hX \"%s\" set_flags=%04hX index=%04hX a2=%04hX entity_id=%04hX group=%04hX section=%04hX a3=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 "] unused=%08" PRIX32 "]",
      this->base_type.load(),
      name_str.c_str(),
      this->set_flags.load(),
      this->index.load(),
      this->unknown_a2.load(),
      this->entity_id.load(),
      this->group.load(),
      this->section.load(),
      this->unknown_a3.load(),
      this->pos.x.load(),
      this->pos.y.load(),
      this->pos.z.load(),
      this->angle.x.load(),
      this->angle.y.load(),
      this->angle.z.load(),
      this->param1.load(),
      this->param2.load(),
      this->param3.load(),
      this->param4.load(),
      this->param5.load(),
      this->param6.load(),
      this->unused.load());
}

uint64_t MapFile::ObjectSetEntry::semantic_hash() const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->group, sizeof(this->group), ret);
  ret = phosg::fnv1a64(&this->section, sizeof(this->section), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->param1, sizeof(this->param1), ret);
  ret = phosg::fnv1a64(&this->param2, sizeof(this->param2), ret);
  ret = phosg::fnv1a64(&this->param3, sizeof(this->param3), ret);
  ret = phosg::fnv1a64(&this->param4, sizeof(this->param4), ret);
  ret = phosg::fnv1a64(&this->param5, sizeof(this->param5), ret);
  ret = phosg::fnv1a64(&this->param6, sizeof(this->param6), ret);
  return ret;
}

string MapFile::EnemySetEntry::str() const {
  return phosg::string_printf("[EnemyEntry type=%04hX \"%s\" set_flags=%04hX index=%04hX num_children=%04hX floor=%04hX entity_id=%04hX section=%04hX wave_number=%04hX wave_number2=%04hX a1=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %g %g %04hX %04hX] unused=%08" PRIX32 "]",
      this->base_type.load(),
      MapFile::name_for_enemy_type(this->base_type),
      this->set_flags.load(),
      this->index.load(),
      this->num_children.load(),
      this->floor.load(),
      this->entity_id.load(),
      this->section.load(),
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
      this->uparam1.load(),
      this->uparam2.load(),
      this->unused.load());
}

uint64_t MapFile::EnemySetEntry::semantic_hash() const {
  uint64_t ret = phosg::fnv1a64(&this->base_type, sizeof(this->base_type));
  ret = phosg::fnv1a64(&this->num_children, sizeof(this->num_children), ret);
  ret = phosg::fnv1a64(&this->section, sizeof(this->section), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  ret = phosg::fnv1a64(&this->wave_number2, sizeof(this->wave_number2), ret);
  ret = phosg::fnv1a64(&this->pos, sizeof(this->pos), ret);
  ret = phosg::fnv1a64(&this->angle, sizeof(this->angle), ret);
  ret = phosg::fnv1a64(&this->fparam1, sizeof(this->fparam1), ret);
  ret = phosg::fnv1a64(&this->fparam2, sizeof(this->fparam2), ret);
  ret = phosg::fnv1a64(&this->fparam3, sizeof(this->fparam3), ret);
  ret = phosg::fnv1a64(&this->fparam4, sizeof(this->fparam4), ret);
  ret = phosg::fnv1a64(&this->fparam5, sizeof(this->fparam5), ret);
  ret = phosg::fnv1a64(&this->uparam1, sizeof(this->uparam1), ret);
  ret = phosg::fnv1a64(&this->uparam2, sizeof(this->uparam2), ret);
  return ret;
}

string MapFile::Event1Entry::str() const {
  return phosg::string_printf("[Event1Entry event_id=%08" PRIX32 " flags=%04hX event_type=%04hX section=%04hX wave_number=%04hX delay=%08" PRIX32 " action_stream_offset=%08" PRIX32 "]",
      this->event_id.load(),
      this->flags.load(),
      this->event_type.load(),
      this->section.load(),
      this->wave_number.load(),
      this->delay.load(),
      this->action_stream_offset.load());
}

uint64_t MapFile::Event1Entry::semantic_hash() const {
  uint64_t ret = phosg::fnv1a64(&this->event_id, sizeof(this->event_id));
  ret = phosg::fnv1a64(&this->section, sizeof(this->section), ret);
  ret = phosg::fnv1a64(&this->wave_number, sizeof(this->wave_number), ret);
  return ret;
}

string MapFile::Event2Entry::str() const {
  return phosg::string_printf("[Event2Entry event_id=%08" PRIX32 " flags=%04hX event_type=%04hX section=%04hX wave_number=%04hX min_delay=%08" PRIX32 " max_delay=%08" PRIX32 " min_enemies=%02hhX max_enemies=%02hhX max_waves=%04hX action_stream_offset=%08" PRIX32 "]",
      this->event_id.load(),
      this->flags.load(),
      this->event_type.load(),
      this->section.load(),
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
      this->uparam1.load(),
      this->uparam2.load(),
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
    const RandomEnemyLocationsHeader& header, phosg::StringReader r, uint16_t section) {
  if (header.num_sections == 0) {
    throw runtime_error("no locations defined");
  }

  phosg::StringReader sections_r = r.sub(header.section_table_offset, header.num_sections * sizeof(RandomEnemyLocationSection));

  size_t bs_min = 0;
  size_t bs_max = header.num_sections - 1;
  do {
    size_t bs_mid = (bs_min + bs_max) / 2;
    if (sections_r.pget<RandomEnemyLocationSection>(bs_mid * sizeof(RandomEnemyLocationSection)).section < section) {
      bs_min = bs_mid + 1;
    } else {
      bs_max = bs_mid;
    }
  } while (bs_min < bs_max);

  const auto& sec = sections_r.pget<RandomEnemyLocationSection>(bs_min * sizeof(RandomEnemyLocationSection));
  if (section != sec.section) {
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

    switch (header.type()) {
      case SectionHeader::Type::OBJECT_SETS:
        this->set_object_sets_for_floor(header.floor, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::ENEMY_SETS:
        this->set_enemy_sets_for_floor(header.floor, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::EVENTS:
        this->set_events_for_floor(header.floor, r.getv(header.data_size), header.data_size, true);
        break;
      case SectionHeader::Type::RANDOM_ENEMY_LOCATIONS:
        this->set_random_enemy_locations_for_floor(header.floor, r.getv(header.data_size), header.data_size);
        break;
      case SectionHeader::Type::RANDOM_ENEMY_DEFINITIONS:
        this->set_random_enemy_definitions_for_floor(header.floor, r.getv(header.data_size), header.data_size);
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
    this->set_object_sets_for_floor(floor, objects_data->data(), objects_data->size());
  }
  if (enemies_data) {
    this->link_data(enemies_data);
    this->set_enemy_sets_for_floor(floor, enemies_data->data(), enemies_data->size());
  }
  if (events_data) {
    this->link_data(events_data);
    this->set_events_for_floor(floor, events_data->data(), events_data->size(), false);
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

void MapFile::set_object_sets_for_floor(uint8_t floor, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.object_sets) {
    throw runtime_error("multiple object sets sections for same floor");
  }
  if (size % sizeof(ObjectSetEntry)) {
    throw runtime_error("object sets section size is not a multiple of entry size");
  }
  floor_sections.object_sets = reinterpret_cast<const ObjectSetEntry*>(data);
  floor_sections.object_set_count = size / sizeof(ObjectSetEntry);
}

void MapFile::set_enemy_sets_for_floor(uint8_t floor, const void* data, size_t size) {
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
}

void MapFile::set_events_for_floor(uint8_t floor, const void* data, size_t size, bool allow_evt2) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.events_data || floor_sections.events1 || floor_sections.events2 || floor_sections.event_action_stream) {
    throw runtime_error("multiple events sections for same floor");
  }

  floor_sections.events_data = data;
  floor_sections.events_data_size = size;
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

void MapFile::set_random_enemy_locations_for_floor(uint8_t floor, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.random_enemy_locations_data) {
    throw runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_locations_data = data;
  floor_sections.random_enemy_locations_data_size = size;
  this->has_any_random_sections = true;
}

void MapFile::set_random_enemy_definitions_for_floor(uint8_t floor, const void* data, size_t size) {
  auto& floor_sections = this->sections_for_floor.at(floor);
  if (floor_sections.random_enemy_definitions_data) {
    throw runtime_error("multiple random enemy locations sections for same floor");
  }

  floor_sections.random_enemy_definitions_data = data;
  floor_sections.random_enemy_definitions_data_size = size;
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
      new_map->set_object_sets_for_floor(floor, this_sf.object_sets, this_sf.object_set_count * sizeof(ObjectSetEntry));
    }

    if (this_sf.enemy_sets) {
      new_map->set_enemy_sets_for_floor(floor, this_sf.enemy_sets, this_sf.enemy_set_count * sizeof(EnemySetEntry));
    }

    if (this_sf.events1) {
      new_map->set_events_for_floor(floor, this_sf.events_data, this_sf.events_data_size, false);
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

          random_state.generate_shuffled_location_table(locations_header, locations_sec_r, source_event2.section);
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
                  e.section = source_event2.section;
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
                    e.uparam1 = def.uparam1;
                    e.uparam2 = def.uparam2;
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
            event.section = source_event2.section;
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
        event.section = source_event2.section;
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
      new_map->set_enemy_sets_for_floor(floor, enemy_sets_sec_data->data(), enemy_sets_sec_data->size());

      auto events1_sec_data = make_shared<string>(std::move(events1_sec_w.str()));
      new_map->link_data(events1_sec_data);
      new_map->set_events_for_floor(floor, events1_sec_data->data(), events1_sec_data->size(), false);
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
        uint16_t section = r.get_u16l();
        uint16_t group = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  08 %04hX %04hX  construct_objects       section=%04hX group=%04hX",
            section, group, section, group));
        break;
      }
      case 0x09: {
        uint16_t section = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  09 %04hX %04hX  construct_enemies       section=%04hX wave_number=%04hX",
            section, wave_number, section, wave_number));
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
        uint16_t section = r.get_u16l();
        uint16_t wave_number = r.get_u16l();
        ret.emplace_back(phosg::string_printf("  0D %04hX %04hX  construct_enemies_stop  section=%04hX wave_number=%04hX",
            section, wave_number, section, wave_number));
        r.go(r.size());
        break;
      }
      default:
        ret.emplace_back(phosg::string_printf("  %02hhX            .invalid", opcode));
    }
  }

  return phosg::join(ret, "\n");
}

string MapFile::disassemble() const {
  deque<string> ret;
  for (uint8_t floor = 0; floor < this->sections_for_floor.size(); floor++) {
    const auto& sf = this->sections_for_floor[floor];
    phosg::StringReader as_r(sf.event_action_stream, sf.event_action_stream_bytes);

    if (sf.object_sets) {
      ret.emplace_back(phosg::string_printf(".object_sets %hhu", floor));
      for (size_t z = 0; z < sf.object_set_count; z++) {
        size_t k_id = z + sf.first_object_set_index;
        ret.emplace_back(phosg::string_printf("/* K-%03zX */ ", k_id) + sf.object_sets[z].str());
      }
    }
    if (sf.enemy_sets) {
      ret.emplace_back(phosg::string_printf(".enemy_sets %hhu", floor));
      for (size_t z = 0; z < sf.enemy_set_count; z++) {
        size_t s_id = z + sf.first_enemy_set_index;
        ret.emplace_back(phosg::string_printf("/* S-%03zX */ ", s_id) + sf.enemy_sets[z].str());
      }
    }
    if (sf.events1) {
      ret.emplace_back(phosg::string_printf(".events %hhu", floor));
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events2[z];
        size_t w_id = z + sf.first_event_set_index;
        ret.emplace_back(phosg::string_printf("/* W-%03zX */ ", w_id) + ev.str());
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.events2) {
      ret.emplace_back(phosg::string_printf(".random_events %hhu", floor));
      for (size_t z = 0; z < sf.event_count; z++) {
        const auto& ev = sf.events2[z];
        ret.emplace_back(phosg::string_printf("/* index %zu */", z) + ev.str());
        size_t as_size = as_r.size() - ev.action_stream_offset;
        ret.emplace_back(this->disassemble_action_stream(as_r.pgetv(ev.action_stream_offset, as_size), as_size));
      }
    }
    if (sf.random_enemy_locations_data) {
      ret.emplace_back(phosg::string_printf(".random_enemy_locations %hhu", floor));
      ret.emplace_back(phosg::format_data(sf.random_enemy_locations_data, sf.random_enemy_locations_data_size));
    }
    if (sf.random_enemy_definitions_data) {
      ret.emplace_back(phosg::string_printf(".random_enemy_definitions %hhu", floor));
      ret.emplace_back(phosg::format_data(sf.random_enemy_definitions_data, sf.random_enemy_definitions_data_size));
    }
  }
  return phosg::join(ret, "\n");
}

////////////////////////////////////////////////////////////////////////////////
// Super map

string SuperMap::Object::str() const {
  string ret = phosg::string_printf("[Object KS-%02hhX-%03zX", this->floor, this->super_id);
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

string SuperMap::Event::str() const {
  string ret = phosg::string_printf("[Event WS-%02hhX-%03zX", this->floor, this->super_id);
  for (Version v : ALL_ARPG_SEMANTIC_VERSIONS) {
    const auto& def = this->version(v);
    if (def.relative_event_index != 0xFFFF) {
      string action_stream_str = phosg::format_data_string(def.action_stream, def.action_stream_size);
      string args_str = def.entry->str();
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

static uint64_t section_index_key(uint8_t floor, uint16_t section, uint16_t wave_number) {
  return (static_cast<uint64_t>(floor) << 32) | (static_cast<uint64_t>(section) << 16) | static_cast<uint64_t>(wave_number);
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

  // Add to section/group index
  uint64_t k = section_index_key(obj->floor, set_entry->section, set_entry->group);
  entities.object_for_floor_section_and_group.emplace(k, obj);

  // Add to door index
  uint32_t base_switch_flag = 0;
  uint32_t num_switch_flags = 0;
  switch (set_entry->base_type) {
    case 0x00C1: // TODoorCave01
      base_switch_flag = set_entry->param4;
      num_switch_flags = (4 - clamp<size_t>(set_entry->param5, 0, 4));
      break;

    case 0x14A: // TODoorAncient08
    case 0x14B: // TODoorAncient09
      base_switch_flag = set_entry->param4;
      num_switch_flags = (set_entry->base_type == 0x14A) ? 4 : 2;
      break;

    case 0x1AB: // TODoorFourLightRuins
    case 0x1C0: // TODoorFourLightSpace
    case 0x221: // TODoorFourLightSeabed
    case 0x222: // TODoorFourLightSeabedU
      base_switch_flag = set_entry->param4;
      num_switch_flags = set_entry->param5;
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

    // Add to section/group index
    uint64_t k = section_index_key(ene->floor, set_entry->section, set_entry->wave_number);
    entities.enemy_for_floor_section_and_wave_number.emplace(k, ene);
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
      bool is_rare = (static_cast<int16_t>(set_entry->uparam1.load()) >= 1);
      add(EnemyType::HILDEBEAR, is_rare, is_rare);
      break;
    }
    case 0x0041: { // TObjEneLappy
      bool is_rare_v123 = (set_entry->uparam1 != 0);
      bool is_rare_bb = (set_entry->uparam1 & 1);
      switch (this->episode) {
        case Episode::EP1:
        case Episode::EP2:
          add(EnemyType::RAG_RAPPY, is_rare_v123, is_rare_bb);
          break;
        case Episode::EP4:
          add((floor > 0x05) ? EnemyType::SAND_RAPPY_ALT : EnemyType::SAND_RAPPY, is_rare_v123, is_rare_bb);
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
      add(set_entry->fparam2 ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
      break;
    case 0x0044: { // TObjEneBeast
      static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
      add(types[set_entry->uparam1 % 3]);
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
      add(types[set_entry->uparam1 % 3]);
      break;
    }
    case 0x0064: { // TObjEneSlime
      // TODO: It appears BB doesn't have a way to force slimes to be rare via
      // constructor args. Is this true?
      bool is_rare_v123 = (set_entry->uparam2 & 1);
      if ((set_entry->num_children != 0) && (set_entry->num_children != 4)) {
        this->log.warning("POFUILLY_SLIME has an unusual num_children (0x%hX)", set_entry->num_children.load());
      }
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
      add((set_entry->uparam1 & 0x01) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
      break;
    case 0x0081: // TObjEneGyaranzo
      add(EnemyType::GARANZ);
      break;
    case 0x0082: // TObjEneMe3ShinowaReal
      add(set_entry->fparam2 ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT);
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
      add(types[set_entry->uparam1 % 3]);
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
      add((set_entry->uparam1 & 1) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL);
      default_num_children = 4;
      break;
    case 0x00D5: // TObjEneMerillLia
      add((set_entry->uparam1 & 0x01) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      break;
    case 0x00D6: // TObjEneBm9Mericarol
      if (set_entry->uparam1 == 0) {
        add(EnemyType::MERICAROL);
      } else {
        add(((set_entry->uparam1 % 3) == 2) ? EnemyType::MERICUS : EnemyType::MERIKLE);
      }
      break;
    case 0x00D7: // TObjEneBm5GibonU
      add((set_entry->uparam1 & 0x01) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
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
      add(set_entry->uparam1 ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
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
        child_type = EnemyType::EPSIGUARD;
      } else {
        add((set_entry->uparam1 & 0x01) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
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
        add(set_entry->fparam2 ? EnemyType::YOWIE_ALT : EnemyType::SATELLITE_LIZARD_ALT);
      } else {
        add(set_entry->fparam2 ? EnemyType::YOWIE : EnemyType::SATELLITE_LIZARD);
      }
      break;
    case 0x0112: {
      bool is_rare = (set_entry->uparam1 & 0x01);
      add(EnemyType::MERISSA_A, is_rare, is_rare);
      break;
    }
    case 0x0113:
      add(EnemyType::GIRTABLULU);
      break;
    case 0x0114: {
      bool is_rare = (set_entry->uparam1 & 0x01);
      add((floor > 0x05) ? EnemyType::ZU_ALT : EnemyType::ZU, is_rare, is_rare);
      break;
    }
    case 0x0115:
      if (set_entry->uparam1 & 2) {
        add(EnemyType::BA_BOOTA);
      } else {
        add((set_entry->uparam1 & 1) ? EnemyType::ZE_BOOTA : EnemyType::BOOTA);
      }
      break;
    case 0x0116: {
      bool is_rare = (set_entry->uparam1 & 0x01);
      add(EnemyType::DORPHON, is_rare, is_rare);
      break;
    }
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      add(types[set_entry->uparam1 % 3]);
      break;
    }
    case 0x0119: {
      // TODO: It appears BB doesn't have a way to force Kondrieu to appear via
      // constructor args. Is this true?
      add((set_entry->uparam1 & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILLION);
      default_num_children = 0x18;
      break;
    }

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
    }

    // Add to section/group index
    uint64_t k = section_index_key(ene->floor, set_entry->section, set_entry->wave_number);
    entities.enemy_for_floor_section_and_wave_number.emplace(k, ene);

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
      case 0x08: // construct_objects(uint16_t section, uint16_t group)
      case 0x09: // construct_enemies(uint16_t section, uint16_t wave_number)
      case 0x0C: // trigger_event(uint32_t event_id)
      case 0x0D: // construct_enemies_stop(uint16_t section, uint16_t wave_number)
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
    throw runtime_error("event entry action stream offset is beyond end of action stream");
  }
  const void* ev_action_stream_start = reinterpret_cast<const uint8_t*>(map_file_action_stream) +
      entry->action_stream_offset;
  size_t ev_action_stream_size = get_action_stream_size(
      ev_action_stream_start, map_file_action_stream_size - entry->action_stream_offset);

  auto& entities = this->version(version);
  auto& ev_ver = ev->version(version);
  if (ev_ver.entry) {
    throw logic_error("event already linked to version");
  }
  ev_ver.entry = entry;
  ev_ver.relative_event_index = entities.events.size();
  ev_ver.action_stream = ev_action_stream_start;
  ev_ver.action_stream_size = ev_action_stream_size;

  entities.events.emplace_back(ev);

  uint64_t k = section_index_key(ev->floor, entry->section, entry->wave_number);
  entities.event_for_floor_section_and_wave_number.emplace(k, ev);
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
  // Group or section changes are pretty bad, but small variances in position
  // and params are tolerated
  return (
      ((prev.group != current.group) * 50.0) +
      ((prev.section != current.section) * 50.0) +
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
  // Section or wave_number changes are pretty bad, but small variances in
  // position and params are tolerated
  return (
      ((prev.section != current.section) * 50.0) +
      ((prev.wave_number != current.wave_number) * 50.0) +
      (prev.pos - current.pos).norm() +
      ((prev.fparam1 != current.fparam1) * 10.0) +
      ((prev.fparam2 != current.fparam2) * 10.0) +
      ((prev.fparam3 != current.fparam3) * 10.0) +
      ((prev.fparam4 != current.fparam4) * 10.0) +
      ((prev.fparam5 != current.fparam5) * 10.0) +
      ((prev.uparam1 != current.uparam1) * 10.0) +
      ((prev.uparam2 != current.uparam2) * 10.0));
}

static double event_add_cost(const MapFile::Event1Entry&) {
  return 1.0;
}
static double event_delete_cost(const MapFile::Event1Entry&) {
  return 1.0;
}
static double event_edit_cost(const MapFile::Event1Entry& prev, const MapFile::Event1Entry& current) {
  // Unlike object and enemy sets, event matching is essentially binary: no
  // variance is tolerated in some parameters, but others are entirely ignored.
  bool is_same = ((prev.event_id == current.event_id) &&
      (prev.section == current.section) &&
      (prev.wave_number == current.wave_number));
  return is_same ? 0.0 : 5.0;
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
      auto edit_path = compute_edit_path(
          prev_sf.object_sets,
          prev_sf.object_set_count,
          this_sf.object_sets,
          this_sf.object_set_count,
          object_set_add_cost,
          object_set_delete_cost,
          object_set_edit_cost);

      auto& prev_entities = this->version(prev_v);

      size_t prev_entities_offset = prev_entities.object_floor_start_indexes.at(floor);
      size_t this_sf_offset = 0;
      for (auto action : edit_path) {
        switch (action) {
          case EditAction::ADD:
            // This object doesn't match any object from the previous version; create a new object
            this->add_object(this_v, floor, this_sf.object_sets + this_sf_offset);
            this_sf_offset++;
            break;
          case EditAction::DELETE:
            // There is an object in the previous version that doesn't match any in this version; skip it
            prev_entities_offset++;
            break;
          case EditAction::EDIT: {
            // The current object in this_sf matches the current object in prev_sf; link them together
            auto prev_obj = prev_entities.objects.at(prev_entities_offset);
            this->link_object_version(prev_obj, this_v, this_sf.object_sets + this_sf_offset);
            prev_entities_offset++;
            this_sf_offset++;
            break;
          }
          default:
            throw logic_error("invalid edit path action");
        }
      }
    }

    if (!prev_map_file || !prev_map_file->floor(floor).enemy_sets) {
      for (size_t z = 0; z < this_sf.enemy_set_count; z++) {
        this->add_enemy_and_children(this_v, floor, this_sf.enemy_sets + z);
      }
    } else if (this_sf.enemy_sets) {
      const auto& prev_sf = prev_map_file->floor(floor);
      auto edit_path = compute_edit_path(
          prev_sf.enemy_sets,
          prev_sf.enemy_set_count,
          this_sf.enemy_sets,
          this_sf.enemy_set_count,
          enemy_set_add_cost,
          enemy_set_delete_cost,
          enemy_set_edit_cost);

      auto& prev_entities = this->version(prev_v);

      size_t prev_entities_offset = prev_entities.enemy_set_floor_start_indexes.at(floor);
      size_t this_sf_offset = 0;
      for (auto action : edit_path) {
        switch (action) {
          case EditAction::ADD:
            // This object doesn't match any object from the previous version; create a new object
            this->add_enemy_and_children(this_v, floor, this_sf.enemy_sets + this_sf_offset);
            this_sf_offset++;
            break;
          case EditAction::DELETE:
            // There is an object in the previous version that doesn't match any in this version; skip it
            prev_entities_offset++;
            break;
          case EditAction::EDIT: {
            // The current object in this_sf matches the current object in prev_sf; link them together
            auto prev_ene = prev_entities.enemy_sets.at(prev_entities_offset);
            this->link_enemy_version_and_children(prev_ene, this_v, this_sf.enemy_sets + this_sf_offset);
            prev_entities_offset++;
            this_sf_offset++;
            break;
          }
          default:
            throw logic_error("invalid edit path action");
        }
      }
    }

    if (!prev_map_file || !prev_map_file->floor(floor).events1) {
      for (size_t z = 0; z < this_sf.event_count; z++) {
        this->add_event(this_v, floor, this_sf.events1 + z, this_sf.event_action_stream, this_sf.event_action_stream_bytes);
      }
    } else if (this_sf.events1) {
      const auto& prev_sf = prev_map_file->floor(floor);
      auto edit_path = compute_edit_path(
          prev_sf.events1,
          prev_sf.event_count,
          this_sf.events1,
          this_sf.event_count,
          event_add_cost,
          event_delete_cost,
          event_edit_cost);

      auto& prev_entities = this->version(prev_v);

      size_t prev_entities_offset = prev_entities.event_floor_start_indexes.at(floor);
      size_t this_sf_offset = 0;
      for (auto action : edit_path) {
        switch (action) {
          case EditAction::ADD:
            // This object doesn't match any object from the previous version; try to look it up in the semantic hash index
            this->add_event(
                this_v,
                floor,
                this_sf.events1 + this_sf_offset,
                this_sf.event_action_stream,
                this_sf.event_action_stream_bytes);
            this_sf_offset++;
            break;
          case EditAction::DELETE:
            // There is an object in the previous version that doesn't match any in this version; skip it
            prev_entities_offset++;
            break;
          case EditAction::EDIT: {
            // The current object in this_sf matches the current object in prev_sf; link them together
            auto prev_ev = prev_entities.events.at(prev_entities_offset);
            this->link_event_version(
                prev_ev,
                this_v,
                this_sf.events1 + this_sf_offset,
                this_sf.event_action_stream,
                this_sf.event_action_stream_bytes);
            prev_entities_offset++;
            this_sf_offset++;
            break;
          }
          default:
            throw logic_error("invalid edit path action");
        }
      }
    }
  }
}

vector<shared_ptr<const SuperMap::Object>> SuperMap::objects_for_floor_section_group(
    Version version, uint8_t floor, uint16_t section, uint16_t group) const {
  const auto& entities = this->version(version);
  uint64_t k = section_index_key(floor, section, group);
  vector<shared_ptr<const Object>> ret;
  for (auto its = entities.object_for_floor_section_and_group.equal_range(k); its.first != its.second; its.first++) {
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

vector<shared_ptr<const SuperMap::Enemy>> SuperMap::enemies_for_floor_section_wave(
    Version version, uint8_t floor, uint16_t section, uint16_t wave_number) const {
  const auto& entities = this->version(version);

  uint64_t k = section_index_key(floor, section, wave_number);
  vector<shared_ptr<const Enemy>> ret;
  for (auto its = entities.enemy_for_floor_section_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
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

vector<shared_ptr<const SuperMap::Event>> SuperMap::events_for_floor_section_wave(
    Version version, uint8_t floor, uint16_t section, uint16_t wave_number) const {
  const auto& entities = this->version(version);
  uint64_t k = section_index_key(floor, section, wave_number);
  vector<shared_ptr<const Event>> ret;
  for (auto its = entities.event_for_floor_section_and_wave_number.equal_range(k); its.first != its.second; its.first++) {
    ret.emplace_back(its.first->second);
  }
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
      if (!ev_ver.entry) {
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

MapState::RareEnemyRates::RareEnemyRates(uint32_t enemy_rate, uint32_t boss_rate)
    : hildeblue(enemy_rate),
      rappy(enemy_rate),
      nar_lily(enemy_rate),
      pouilly_slime(enemy_rate),
      merissa_aa(enemy_rate),
      pazuzu(enemy_rate),
      dorphon_eclair(enemy_rate),
      kondrieu(boss_rate) {}

MapState::RareEnemyRates::RareEnemyRates(const phosg::JSON& json)
    : hildeblue(json.get_int("Hildeblue")),
      rappy(json.get_int("Rappy")),
      nar_lily(json.get_int("NarLily")),
      pouilly_slime(json.get_int("PouillySlime")),
      merissa_aa(json.get_int("MerissaAA")),
      pazuzu(json.get_int("Pazuzu")),
      dorphon_eclair(json.get_int("DorphonEclair")),
      kondrieu(json.get_int("Kondrieu")) {}

string MapState::RareEnemyRates::str() const {
  return phosg::string_printf("RareEnemyRates(hildeblue=%08" PRIX32 ", rappy=%08" PRIX32 ", nar_lily=%08" PRIX32 ", pouilly_slime=%08" PRIX32 ", merissa_aa=%08" PRIX32 ", pazuzu=%08" PRIX32 ", dorphon_eclair=%08" PRIX32 ", kondrieu=%08" PRIX32 ")",
      this->hildeblue, this->rappy, this->nar_lily, this->pouilly_slime,
      this->merissa_aa, this->pazuzu, this->dorphon_eclair, this->kondrieu);
}

phosg::JSON MapState::RareEnemyRates::json() const {
  return phosg::JSON::dict({
      {"Hildeblue", this->hildeblue},
      {"Rappy", this->rappy},
      {"NarLily", this->nar_lily},
      {"PouillySlime", this->pouilly_slime},
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
    case EnemyType::SAND_RAPPY:
    case EnemyType::SAND_RAPPY_ALT:
      return this->rappy;
    case EnemyType::POISON_LILY:
      return this->nar_lily;
    case EnemyType::POFUILLY_SLIME:
      return this->pouilly_slime;
    case EnemyType::MERISSA_A:
      return this->merissa_aa;
    case EnemyType::ZU:
    case EnemyType::ZU_ALT:
      return this->pazuzu;
    case EnemyType::DORPHON:
      return this->dorphon_eclair;
    case EnemyType::SAINT_MILLION:
    case EnemyType::SHAMBERTIN:
      return this->kondrieu;
    default:
      return 0;
  }
}

const shared_ptr<const MapState::RareEnemyRates> MapState::NO_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(0, 0);
const shared_ptr<const MapState::RareEnemyRates> MapState::DEFAULT_RARE_ENEMIES = make_shared<MapState::RareEnemyRates>(0x0083126E, 0x1999999A);

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

    auto rare_type = rare_type_for_enemy_type(type, fc.super_map->get_episode(), this->event, ene->floor);
    if (rare_type != type) {
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

          // On v1 and v2 (and GC NTE), the rare rate is 0.1% instead of 0.2%.
          if (det < (is_v1_or_v2(v) ? 0.001f : 0.002f)) {
            ene_st->set_rare(v);
          }
        } else if ((bb_rare_rate > 0) &&
            (this->bb_rare_enemy_indexes.size() < 0x10) &&
            (random_from_optional_crypt(opt_rand_crypt) < bb_rare_rate)) {
          this->bb_rare_enemy_indexes.emplace_back(enemy_index);
          ene_st->set_rare(v);
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

vector<shared_ptr<MapState::ObjectState>> MapState::object_states_for_floor_section_group(
    Version version, uint8_t floor, uint16_t section, uint16_t group) {
  vector<shared_ptr<ObjectState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& obj : fc.super_map->objects_for_floor_section_group(version, floor, section, group)) {
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

vector<shared_ptr<MapState::EnemyState>> MapState::enemy_states_for_floor_section_wave(
    Version version, uint8_t floor, uint16_t section, uint16_t wave_number) {
  vector<shared_ptr<EnemyState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ene : fc.super_map->enemies_for_floor_section_wave(version, floor, section, wave_number)) {
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

vector<shared_ptr<MapState::EventState>> MapState::event_states_for_floor_section_wave(
    Version version, uint8_t floor, uint16_t section, uint16_t wave_number) {
  vector<shared_ptr<EventState>> ret;
  auto& fc = this->floor_config(floor);
  if (fc.super_map) {
    for (const auto& ev : fc.super_map->events_for_floor_section_wave(version, floor, section, wave_number)) {
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
      if (ene_st->game_flags != entry.flags) {
        this->log.warning("(%04zX => E-%03zX) Flags from client (%08" PRIX32 ") do not match game flags from map (%08" PRIX32 ")",
            enemy_index, ene_st->e_id, entry.flags.load(), ene_st->game_flags);
        ene_st->game_flags = entry.flags;
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
    fprintf(stream, " %s total_damage=%04hX rare_flags=%04hX game_flags=%08" PRIX32 " set_flags=%04hX server_flags=%02hhX\n",
        ene_str.c_str(), ene_st->total_damage, ene_st->rare_flags, ene_st->game_flags, ene_st->set_flags, ene_st->server_flags);
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
