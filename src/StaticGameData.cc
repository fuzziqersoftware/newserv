#include "StaticGameData.hh"

#include <array>

#include "Text.hh"

using namespace std;

bool episode_has_arpg_semantics(Episode ep) {
  return (ep == Episode::EP1) || (ep == Episode::EP2) || (ep == Episode::EP4);
}

const char* name_for_episode(Episode ep) {
  switch (ep) {
    case Episode::NONE:
      return "No episode";
    case Episode::EP1:
      return "Episode 1";
    case Episode::EP2:
      return "Episode 2";
    case Episode::EP3:
      return "Episode 3";
    case Episode::EP4:
      return "Episode 4";
    default:
      return "Unknown episode";
  }
}

const char* token_name_for_episode(Episode ep) {
  switch (ep) {
    case Episode::EP1:
      return "Episode1";
    case Episode::EP2:
      return "Episode2";
    case Episode::EP3:
      return "Episode3";
    case Episode::EP4:
      return "Episode4";
    default:
      throw logic_error("invalid episode");
  }
}

Episode episode_for_token_name(const string& name) {
  if (name == "Episode1") {
    return Episode::EP1;
  }
  if (name == "Episode2") {
    return Episode::EP2;
  }
  if (name == "Episode3") {
    return Episode::EP3;
  }
  if (name == "Episode4") {
    return Episode::EP4;
  }
  throw runtime_error("unknown episode");
}

const char* abbreviation_for_episode(Episode ep) {
  switch (ep) {
    case Episode::NONE:
      return "None";
    case Episode::EP1:
      return "Ep1";
    case Episode::EP2:
      return "Ep2";
    case Episode::EP3:
      return "Ep3";
    case Episode::EP4:
      return "Ep4";
    default:
      return "UnkEp";
  }
}

const char* name_for_mode(GameMode mode) {
  switch (mode) {
    case GameMode::NORMAL:
      return "Normal";
    case GameMode::BATTLE:
      return "Battle";
    case GameMode::CHALLENGE:
      return "Challenge";
    case GameMode::SOLO:
      return "Solo";
    default:
      return "Unknown mode";
  }
}

const char* abbreviation_for_mode(GameMode mode) {
  switch (mode) {
    case GameMode::NORMAL:
      return "Nml";
    case GameMode::BATTLE:
      return "Btl";
    case GameMode::CHALLENGE:
      return "Chl";
    case GameMode::SOLO:
      return "Solo";
    default:
      return "UnkMd";
  }
}

static const array<const char*, 10> section_id_to_name = {
    "Viridia", "Greennill", "Skyly", "Bluefull", "Purplenum",
    "Pinkal", "Redria", "Oran", "Yellowboze", "Whitill"};

static const array<const char*, 10> section_id_to_abbreviation = {
    "Vir", "Grn", "Sky", "Blu", "Prp", "Pnk", "Red", "Orn", "Ylw", "Wht"};

const unordered_map<string, uint8_t> name_to_section_id({
    {"viridia", 0},
    {"greennill", 1},
    {"greenill", 1},
    {"skyly", 2},
    {"bluefull", 3},
    {"purplenum", 4},
    {"pinkal", 5},
    {"redria", 6},
    {"oran", 7},
    {"yellowboze", 8},
    {"whitill", 9},

    // Shortcuts for chat commands
    {"b", 3},
    {"g", 1},
    {"o", 7},
    {"pi", 5},
    {"pu", 4},
    {"r", 6},
    {"s", 2},
    {"v", 0},
    {"w", 9},
    {"y", 8},
});

const vector<string> lobby_event_to_name = {
    "none", "xmas", "none", "val", "easter", "hallo", "sonic", "newyear",
    "summer", "white", "wedding", "fall", "s-spring", "s-summer", "spring"};

const unordered_map<string, uint8_t> name_to_lobby_event({
    {"none", 0},
    {"xmas", 1},
    {"val", 3},
    {"easter", 4},
    {"hallo", 5},
    {"sonic", 6},
    {"newyear", 7},
    {"summer", 8},
    {"white", 9},
    {"wedding", 10},
    {"fall", 11},
    {"s-spring", 12},
    {"s-summer", 13},
    {"spring", 14},
});

const unordered_map<uint8_t, string> lobby_type_to_name({
    {0x00, "normal"},
    {0x0F, "inormal"},
    {0x10, "ipc"},
    {0x11, "iball"},
    {0x67, "cave2u"},
    {0xD4, "cave1"},
    {0xE9, "planet"},
    {0xEA, "clouds"},
    {0xED, "cave"},
    {0xEE, "jungle"},
    {0xEF, "forest2-2"},
    {0xF0, "forest2-1"},
    {0xF1, "windpower"},
    {0xF2, "overview"},
    {0xF3, "seaside"},
    {0xF4, "fons"},
    {0xF5, "dmorgue"},
    {0xF6, "caelum"},
    {0xF8, "cyber"},
    {0xF9, "boss1"},
    {0xFA, "boss2"},
    {0xFB, "dolor"},
    {0xFC, "dragon"},
    {0xFD, "derolle"},
    {0xFE, "volopt"},
    {0xFF, "darkfalz"},
});

const unordered_map<string, uint8_t> name_to_lobby_type({
    {"normal", 0x00},
    {"inormal", 0x0F},
    {"ipc", 0x10},
    {"iball", 0x11},
    {"cave1", 0xD4},
    {"cave2u", 0x67},
    {"dragon", 0xFC},
    {"derolle", 0xFD},
    {"volopt", 0xFE},
    {"darkfalz", 0xFF},
    {"planet", 0xE9},
    {"clouds", 0xEA},
    {"cave", 0xED},
    {"jungle", 0xEE},
    {"forest2-2", 0xEF},
    {"forest2-1", 0xF0},
    {"windpower", 0xF1},
    {"overview", 0xF2},
    {"seaside", 0xF3},
    {"fons", 0xF4},
    {"dmorgue", 0xF5},
    {"caelum", 0xF6},
    {"cyber", 0xF8},
    {"boss1", 0xF9},
    {"boss2", 0xFA},
    {"dolor", 0xFB},
    {"ravum", 0xFC},
    {"sky", 0xFE},
    {"morgue", 0xFF},
});

const vector<string> npc_id_to_name({
    "ninja",
    "rico",
    "sonic",
    "knuckles",
    "tails",
    "flowen",
    "elly",
    "momoka",
    "irene",
    "guild",
    "nurse",
});

const unordered_map<string, uint8_t> name_to_npc_id = {
    {"ninja", 0},
    {"rico", 1},
    {"sonic", 2},
    {"knuckles", 3},
    {"tails", 4},
    {"flowen", 5},
    {"elly", 6},
    {"momoka", 7},
    {"irene", 8},
    {"guild", 9},
    {"nurse", 10},
};

bool npc_valid_for_version(uint8_t npc, Version version) {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
      return false;
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return (npc < 5);
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      return (npc < 7);
    case Version::BB_V4:
      return (npc < 11);
    default:
      return false;
  }
}

const char* abbreviation_for_section_id(uint8_t section_id) {
  if (section_id < section_id_to_abbreviation.size()) {
    return section_id_to_abbreviation[section_id];
  } else {
    return "Unknown";
  }
}

const char* name_for_section_id(uint8_t section_id) {
  if (section_id < section_id_to_name.size()) {
    return section_id_to_name[section_id];
  } else {
    return "Unknown";
  }
}

uint8_t section_id_for_name(const string& name) {
  string lower_name = phosg::tolower(name);
  try {
    return name_to_section_id.at(lower_name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name);
    if (x < section_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0xFF;
}

const string& name_for_event(uint8_t event) {
  if (event < lobby_event_to_name.size()) {
    return lobby_event_to_name[event];
  } else {
    static const string ret = "Unknown lobby event";
    return ret;
  }
}

uint8_t event_for_name(const string& name) {
  try {
    return name_to_lobby_event.at(name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name);
    if (x < lobby_event_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0xFF;
}

const string& name_for_lobby_type(uint8_t type) {
  try {
    return lobby_type_to_name.at(type);
  } catch (const out_of_range&) {
    static const string ret = "Unknown lobby type";
    return ret;
  }
}

uint8_t lobby_type_for_name(const string& name) {
  try {
    return name_to_lobby_type.at(name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name, nullptr, 0);
    if (lobby_type_to_name.count(x)) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0x80;
}

const string& name_for_npc(uint8_t npc) {
  try {
    return npc_id_to_name.at(npc);
  } catch (const out_of_range&) {
    static const string ret = "Unknown NPC";
    return ret;
  }
}

uint8_t npc_for_name(const string& name, Version version) {
  uint8_t npc_id = 0xFF;
  try {
    npc_id = name_to_npc_id.at(name);
  } catch (const out_of_range&) {
  }
  if (npc_id == 0xFF) {
    try {
      npc_id = stoul(name);
    } catch (const invalid_argument&) {
    } catch (const out_of_range&) {
    }
  }
  return npc_valid_for_version(npc_id, version) ? npc_id : 0xFF;
}

const char* name_for_char_class(uint8_t cls) {
  static const array<const char*, 12> names = {
      "HUmar",
      "HUnewearl",
      "HUcast",
      "RAmar",
      "RAcast",
      "RAcaseal",
      "FOmarl",
      "FOnewm",
      "FOnewearl",
      "HUcaseal",
      "FOmar",
      "RAmarl",
  };
  try {
    return names.at(cls);
  } catch (const out_of_range&) {
    return "Unknown";
  }
}

const char* abbreviation_for_char_class(uint8_t cls) {
  static const array<const char*, 12> names = {
      "HUmr",
      "HUnl",
      "HUct",
      "RAmr",
      "RAct",
      "RAcl",
      "FOml",
      "FOnm",
      "FOnl",
      "HUcl",
      "FOmr",
      "RAml",
  };
  try {
    return names.at(cls);
  } catch (const out_of_range&) {
    return "???";
  }
}

enum ClassFlag {
  MALE = 0x01,
  HUMAN = 0x02,
  NEWMAN = 0x04,
  ANDROID = 0x08,
  HUNTER = 0x10,
  RANGER = 0x20,
  FORCE = 0x40,
};

static array<uint8_t, 12> class_flags = {
    ClassFlag::HUNTER | ClassFlag::HUMAN | ClassFlag::MALE, // HUmar
    ClassFlag::HUNTER | ClassFlag::NEWMAN, // HUnewearl
    ClassFlag::HUNTER | ClassFlag::ANDROID | ClassFlag::MALE, // HUcast
    ClassFlag::RANGER | ClassFlag::HUMAN | ClassFlag::MALE, // RAmar
    ClassFlag::RANGER | ClassFlag::ANDROID | ClassFlag::MALE, // RAcast
    ClassFlag::RANGER | ClassFlag::ANDROID, // RAcaseal
    ClassFlag::FORCE | ClassFlag::HUMAN, // FOmarl
    ClassFlag::FORCE | ClassFlag::NEWMAN | ClassFlag::MALE, // FOnewm
    ClassFlag::FORCE | ClassFlag::NEWMAN, // FOnewearl
    ClassFlag::HUNTER | ClassFlag::ANDROID, // HUcaseal
    ClassFlag::FORCE | ClassFlag::HUMAN | ClassFlag::MALE, // FOmar
    ClassFlag::RANGER | ClassFlag::HUMAN, // RAmarl
};

bool char_class_is_male(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::MALE;
}

bool char_class_is_human(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::HUMAN;
}

bool char_class_is_newman(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::NEWMAN;
}

bool char_class_is_android(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::ANDROID;
}

bool char_class_is_hunter(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::HUNTER;
}

bool char_class_is_ranger(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::RANGER;
}

bool char_class_is_force(uint8_t cls) {
  return class_flags.at(cls) & ClassFlag::FORCE;
}

const char* name_for_difficulty(uint8_t difficulty) {
  static const array<const char*, 4> names = {
      "Normal", "Hard", "Very Hard", "Ultimate"};
  try {
    return names.at(difficulty);
  } catch (const out_of_range&) {
    return "Unknown";
  }
}

const char* token_name_for_difficulty(uint8_t difficulty) {
  static const array<const char*, 4> names = {
      "Normal", "Hard", "VeryHard", "Ultimate"};
  try {
    return names.at(difficulty);
  } catch (const out_of_range&) {
    return "Unknown";
  }
}

char abbreviation_for_difficulty(uint8_t difficulty) {
  static const array<char, 4> names = {'N', 'H', 'V', 'U'};
  try {
    return names.at(difficulty);
  } catch (const out_of_range&) {
    return '?';
  }
}

const char* name_for_language_code(uint8_t language_code) {
  array<const char*, 8> names = {{"Japanese",
      "English",
      "German",
      "French",
      "Spanish",
      "Simplified Chinese",
      "Traditional Chinese",
      "Korean"}};
  return (language_code < 8) ? names[language_code] : "Unknown";
}

char char_for_language_code(uint8_t language_code) {
  return (language_code < 8) ? "JEGFSBTK"[language_code] : '?';
}

uint8_t language_code_for_char(char language_char) {
  switch (language_char) {
    case 'J':
    case 'j':
      return 0;
    case 'E':
    case 'e':
      return 1;
    case 'G':
    case 'g':
      return 2;
    case 'F':
    case 'f':
      return 3;
    case 'S':
    case 's':
      return 4;
    case 'B':
    case 'b':
      return 5;
    case 'T':
    case 't':
      return 6;
    case 'K':
    case 'k':
      return 7;
    default:
      throw runtime_error("unknown language");
  }
}

const vector<string> tech_id_to_name = {
    "foie", "gifoie", "rafoie",
    "barta", "gibarta", "rabarta",
    "zonde", "gizonde", "razonde",
    "grants", "deband", "jellen", "zalure", "shifta",
    "ryuker", "resta", "anti", "reverser", "megid"};

const unordered_map<string, uint8_t> name_to_tech_id({
    {"foie", 0},
    {"gifoie", 1},
    {"rafoie", 2},
    {"barta", 3},
    {"gibarta", 4},
    {"rabarta", 5},
    {"zonde", 6},
    {"gizonde", 7},
    {"razonde", 8},
    {"grants", 9},
    {"deband", 10},
    {"jellen", 11},
    {"zalure", 12},
    {"shifta", 13},
    {"ryuker", 14},
    {"resta", 15},
    {"anti", 16},
    {"reverser", 17},
    {"megid", 18},
});

const string& name_for_technique(uint8_t tech) {
  try {
    return tech_id_to_name.at(tech);
  } catch (const out_of_range&) {
    static const string ret = "Unknown technique";
    return ret;
  }
}

uint8_t technique_for_name(const string& name) {
  try {
    return name_to_tech_id.at(name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name);
    if (x < tech_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0xFF;
}

const vector<const char*> name_for_mag_color({
    /* 00 */ "red",
    /* 01 */ "blue",
    /* 02 */ "yellow",
    /* 03 */ "green",
    /* 04 */ "purple",
    /* 05 */ "black",
    /* 06 */ "white",
    /* 07 */ "cyan",
    /* 08 */ "brown",
    /* 09 */ "orange",
    /* 0A */ "light-blue",
    /* 0B */ "olive",
    /* 0C */ "light-cyan",
    /* 0D */ "dark-purple",
    /* 0E */ "grey",
    /* 0F */ "light-grey",
    /* 10 */ "pink",
    /* 11 */ "dark-cyan",
    /* 12 */ "costume",
});

const unordered_map<string, uint8_t> mag_color_for_name({
    {"red", 0x00},
    {"blue", 0x01},
    {"yellow", 0x02},
    {"green", 0x03},
    {"purple", 0x04},
    {"black", 0x05},
    {"white", 0x06},
    {"cyan", 0x07},
    {"brown", 0x08},
    {"orange", 0x09},
    {"light-blue", 0x0A},
    {"olive", 0x0B},
    {"light-cyan", 0x0C},
    {"dark-purple", 0x0D},
    {"grey", 0x0E},
    {"light-grey", 0x0F},
    {"pink", 0x10},
    {"dark-cyan", 0x11},
    {"costume-color", 0x12},
});

uint8_t floor_for_name(const std::string& name) {
  static const unordered_map<string, uint8_t> floors({
      {"pioneer2", 0x00},
      {"p2", 0x00},
      {"forest1", 0x01},
      {"f1", 0x01},
      {"forest2", 0x02},
      {"f2", 0x02},
      {"caves1", 0x03},
      {"cave1", 0x03},
      {"c1", 0x03},
      {"caves2", 0x04},
      {"cave2", 0x04},
      {"c2", 0x04},
      {"caves3", 0x05},
      {"cave3", 0x05},
      {"c3", 0x05},
      {"mines1", 0x06},
      {"mine1", 0x06},
      {"m1", 0x06},
      {"mines2", 0x07},
      {"mine2", 0x07},
      {"m2", 0x07},
      {"ruins1", 0x08},
      {"ruin1", 0x08},
      {"r1", 0x08},
      {"ruins2", 0x09},
      {"ruin2", 0x09},
      {"r2", 0x09},
      {"ruins3", 0x0A},
      {"ruin3", 0x0A},
      {"r3", 0x0A},
      {"dragon", 0x0B},
      {"derolle", 0x0C},
      {"volopt", 0x0D},
      {"darkfalz", 0x0E},
      {"lobby", 0x0F},
      {"battle1", 0x10},
      {"battle2", 0x11},

      {"pioneer2", 0x00},
      {"p2", 0x00},
      {"vrtemplealpha", 0x01},
      {"templealpha", 0x01},
      {"vrtemplebeta", 0x02},
      {"templebeta", 0x02},
      {"vrspaceshipalpha", 0x03},
      {"spaceshipalpha", 0x03},
      {"vrspaceshipbeta", 0x04},
      {"spaceshipbeta", 0x04},
      {"centralcontrolarea", 0x05},
      {"cca", 0x05},
      {"junglenorth", 0x06},
      {"jungleeast", 0x07},
      {"mountain", 0x08},
      {"seaside", 0x09},
      {"seabedupper", 0x0A},
      {"seabedlower", 0x0B},
      {"galgryphon", 0x0C},
      {"olgaflow", 0x0D},
      {"barbaray", 0x0E},
      {"goldragon", 0x0F},
      {"seasidenight", 0x10},
      {"tower", 0x11},

      {"pioneer2", 0x00},
      {"p2", 0x00},
      {"cratereast", 0x01},
      {"ce", 0x01},
      {"craterwest", 0x02},
      {"cw", 0x02},
      {"cratersouth", 0x03},
      {"cs", 0x03},
      {"craternorth", 0x04},
      {"cn", 0x04},
      {"craterinterior", 0x05},
      {"ci", 0x05},
      {"desert1", 0x06},
      {"d1", 0x06},
      {"desert2", 0x07},
      {"d2", 0x07},
      {"desert3", 0x08},
      {"d3", 0x08},
      {"saintmillion", 0x09},
      {"purgatory", 0x0A},
  });
  return floors.at(phosg::tolower(name));
}

static const array<const char*, 0x12> ep1_floor_names = {
    "Pioneer2",
    "Forest1",
    "Forest2",
    "Caves1",
    "Caves2",
    "Caves3",
    "Mines1",
    "Mines2",
    "Ruins1",
    "Ruins2",
    "Ruins3",
    "Dragon",
    "DeRolLe",
    "VolOpt",
    "DarkFalz",
    "Lobby",
    "Battle1",
    "Battle2",
};

static const array<const char*, 0x12> ep2_floor_names = {
    "Pioneer2",
    "VRTempleAlpha",
    "VRTempleBeta",
    "VRSpaceshipAlpha",
    "VRSpaceshipBeta",
    "CentralControlArea",
    "JungleNorth",
    "JungleEast",
    "Mountain",
    "Seaside",
    "SeabedUpper",
    "SeabedLower",
    "GalGryphon",
    "OlgaFlow",
    "BarbaRay",
    "GolDragon",
    "SeasideNight",
    "Tower",
};

static const array<const char*, 0x0B> ep4_floor_names = {
    "Pioneer2",
    "CraterEast",
    "CraterWest",
    "CraterSouth",
    "CraterNorth",
    "CraterInterior",
    "Desert1",
    "Desert2",
    "Desert3",
    "SaintMillion",
    "Purgatory",
};

static const array<const char*, 0x12> ep1_floor_names_short = {
    "P2", "F1", "F2", "C1", "C2", "C3", "M1", "M2", "R1", "R2", "R3", "Dgn", "DRL", "VO", "DF", "Lby", "B1", "B2"};

static const array<const char*, 0x12> ep2_floor_names_short = {
    "Lab", "VRTA", "VRTB", "VRSA", "VRSB", "CCA", "JN", "JE", "Mtn", "SS", "SU", "SL", "GG", "OF", "BR", "GD", "SSN", "Twr"};

static const array<const char*, 0x0B> ep4_floor_names_short = {
    "P2", "CE", "CW", "CS", "CN", "CI", "D1", "D2", "D3", "SM", "Pg"};

size_t floor_limit_for_episode(Episode ep) {
  switch (ep) {
    case Episode::EP1:
      return ep1_floor_names.size() - 1;
    case Episode::EP2:
      return ep2_floor_names.size() - 1;
    case Episode::EP4:
      return ep4_floor_names.size() - 1;
    default:
      return 0;
  }
}

const char* name_for_floor(Episode episode, uint8_t floor) {
  switch (episode) {
    case Episode::EP1:
      return ep1_floor_names.at(floor);
    case Episode::EP2:
      return ep2_floor_names.at(floor);
    case Episode::EP4:
      return ep4_floor_names.at(floor);
    default:
      throw logic_error("invalid episode for drop floor");
  }
}

const char* short_name_for_floor(Episode episode, uint8_t floor) {
  switch (episode) {
    case Episode::EP1:
      return ep1_floor_names_short.at(floor);
    case Episode::EP2:
      return ep2_floor_names_short.at(floor);
    case Episode::EP4:
      return ep4_floor_names_short.at(floor);
    default:
      throw logic_error("invalid episode for floor");
  }
}

bool floor_is_boss_arena(Episode episode, uint8_t floor) {
  switch (episode) {
    case Episode::EP1:
      return (floor >= 0x0B) && (floor <= 0x0E);
    case Episode::EP2:
      return (floor >= 0x0C) && (floor <= 0x0F);
    case Episode::EP4:
      return (floor == 0x09);
    default:
      return false;
  }
}

uint32_t class_flags_for_class(uint8_t char_class) {
  static constexpr uint8_t flags[12] = {
      0x25, 0x2A, 0x31, 0x45, 0x51, 0x52, 0x86, 0x89, 0x8A, 0x32, 0x85, 0x46};
  if (char_class >= 12) {
    throw runtime_error("invalid character class");
  }
  return flags[char_class];
}

char char_for_challenge_rank(uint8_t rank) {
  if (rank > 2) {
    return '?';
  }
  return "BAS"[rank];
}

const array<size_t, 4> DEFAULT_MIN_LEVELS_V3({0, 19, 39, 79});
const array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP1({0, 19, 39, 79});
const array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP2({0, 29, 49, 89});
const array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP4({0, 39, 79, 109});
