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
    /* 0C */ "turquoise",
    /* 0D */ "fuchsia",
    /* 0E */ "grey",
    /* 0F */ "cream",
    /* 10 */ "pink",
    /* 11 */ "dark-green",
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
    {"turquoise", 0x0C},
    {"fuchsia", 0x0D},
    {"grey", 0x0E},
    {"cream", 0x0F},
    {"pink", 0x10},
    {"dark-green", 0x11},
    {"costume-color", 0x12},
});

static constexpr uint8_t F_CITY = FloorDefinition::Flag::CITY;
static constexpr uint8_t F_LOBBY = FloorDefinition::Flag::LOBBY;
static constexpr uint8_t F_BOSS = FloorDefinition::Flag::BOSS_ARENA;
static constexpr uint8_t F_V1 = FloorDefinition::Flag::EXISTS_ON_V1 | FloorDefinition::Flag::EXISTS_ON_V2 | FloorDefinition::Flag::EXISTS_ON_GC_NTE | FloorDefinition::Flag::EXISTS_ON_V3 | FloorDefinition::Flag::EXISTS_ON_V4;
static constexpr uint8_t F_V2 = FloorDefinition::Flag::EXISTS_ON_V2 | FloorDefinition::Flag::EXISTS_ON_GC_NTE | FloorDefinition::Flag::EXISTS_ON_V3 | FloorDefinition::Flag::EXISTS_ON_V4;
static constexpr uint8_t F_GCN = FloorDefinition::Flag::EXISTS_ON_GC_NTE | FloorDefinition::Flag::EXISTS_ON_V3 | FloorDefinition::Flag::EXISTS_ON_V4;
static constexpr uint8_t F_V3 = FloorDefinition::Flag::EXISTS_ON_V3 | FloorDefinition::Flag::EXISTS_ON_V4;
static constexpr uint8_t F_V4 = FloorDefinition::Flag::EXISTS_ON_V4;

static const std::vector<FloorDefinition> floor_defs{
    // clang-format off
    {Episode::EP1, 0x00, 0x00, 0x00, 0xFF, F_V1 | F_CITY,  "Pioneer2",            "P2",   "Pioneer 2", {"pioneer2", "p2", "city"}},
    {Episode::EP1, 0x01, 0x01, 0x01, 0x00, F_V1,           "Forest1",             "F1",   "Forest 1", {"forest1", "f1"}},
    {Episode::EP1, 0x02, 0x02, 0x02, 0x01, F_V1,           "Forest2",             "F2",   "Forest 2", {"forest2", "f2"}},
    {Episode::EP1, 0x03, 0x03, 0x03, 0x02, F_V1,           "Cave1",               "C1",   "Cave 1", {"caves1", "cave1", "c1"}},
    {Episode::EP1, 0x04, 0x04, 0x04, 0x03, F_V1,           "Cave2",               "C2",   "Cave 2", {"caves2", "cave2", "c2"}},
    {Episode::EP1, 0x05, 0x05, 0x05, 0x04, F_V1,           "Cave3",               "C3",   "Cave 3", {"caves3", "cave3", "c3"}},
    {Episode::EP1, 0x06, 0x06, 0x06, 0x05, F_V1,           "Mine1",               "M1",   "Mine 1", {"mines1", "mine1", "m1"}},
    {Episode::EP1, 0x07, 0x07, 0x07, 0x06, F_V1,           "Mine2",               "M2",   "Mine 2", {"mines2", "mine2", "m2"}},
    {Episode::EP1, 0x08, 0x08, 0x08, 0x07, F_V1,           "Ruins1",              "R1",   "Ruins 1", {"ruins1", "ruin1", "r1"}},
    {Episode::EP1, 0x09, 0x09, 0x09, 0x08, F_V1,           "Ruins2",              "R2",   "Ruins 2", {"ruins2", "ruin2", "r2"}},
    {Episode::EP1, 0x0A, 0x0A, 0x0A, 0x09, F_V1,           "Ruins3",              "R3",   "Ruins 3", {"ruins3", "ruin3", "r3"}},
    {Episode::EP1, 0x0B, 0x0B, 0x0B, 0x02, F_V1 | F_BOSS,  "Dragon",              "Dgn",  "Under the Dome (Dragon)", {"dragon"}},
    {Episode::EP1, 0x0C, 0x0C, 0x0C, 0x05, F_V1 | F_BOSS,  "DeRolLe",             "DRL",  "Underground Channel (De Rol Le)", {"derolle"}},
    {Episode::EP1, 0x0D, 0x0D, 0x0D, 0x07, F_V1 | F_BOSS,  "VolOpt",              "VO",   "Monitor Room (Vol Opt)", {"volopt"}},
    {Episode::EP1, 0x0E, 0x0E, 0x0E, 0x09, F_V1 | F_BOSS,  "DarkFalz",            "DF",   "???? (Dark Falz)", {"darkfalz"}},
    {Episode::EP1, 0x0F, 0x0F, 0x0F, 0xFF, F_V1 | F_LOBBY, "Lobby",               "Lby",  "Lobby", {"lobby"}},
    {Episode::EP1, 0x10, 0x10, 0x10, 0x09, F_V2,           "Battle1",             "B1",   "Spaceship", {"battle1"}},
    {Episode::EP1, 0x11, 0x11, 0x11, 0x09, F_V2,           "Battle2",             "B2",   "Palace", {"battle2"}},

    {Episode::EP2, 0x00, 0x00, 0x12, 0xFF, F_GCN | F_CITY, "Pioneer2",            "Lab",  "Lab", {"pioneer2", "p2", "lab", "labo"}},
    {Episode::EP2, 0x01, 0x13, 0x13, 0x00, F_GCN,          "VRTempleAlpha",       "VRTA", "VR Temple Alpha", {"vrtemplealpha", "templealpha", "vrta"}},
    {Episode::EP2, 0x02, 0x14, 0x14, 0x01, F_GCN,          "VRTempleBeta",        "VRTB", "VR Temple Beta", {"vrtemplebeta", "templebeta", "vrtb"}},
    {Episode::EP2, 0x03, 0x15, 0x15, 0x02, F_GCN,          "VRSpaceshipAlpha",    "VRSA", "VR Spaceship Alpha", {"vrspaceshipalpha", "spaceshipalpha", "vrsa"}},
    {Episode::EP2, 0x04, 0x16, 0x16, 0x03, F_GCN,          "VRSpaceshipBeta",     "VRSB", "VR Spaceship Beta", {"vrspaceshipbeta", "spaceshipbeta", "vrsb"}},
    {Episode::EP2, 0x05, 0x17, 0x17, 0x07, F_GCN,          "CentralControlArea",  "CCA",  "Central Control Area", {"centralcontrolarea", "cca"}},
    {Episode::EP2, 0x06, 0x18, 0x18, 0x04, F_GCN,          "JungleNorth",         "JN",   "Jungle Area North", {"junglenorth"}},
    {Episode::EP2, 0x07, 0x19, 0x19, 0x05, F_GCN,          "JungleEast",          "JE",   "Jungle Area East", {"jungleeast"}},
    {Episode::EP2, 0x08, 0x1A, 0x1A, 0x06, F_GCN,          "Mountain",            "Mtn",  "Mountain Area", {"mountain"}},
    {Episode::EP2, 0x09, 0x1B, 0x1B, 0x07, F_GCN,          "Seaside",             "SS",   "Seaside Area", {"seaside"}},
    {Episode::EP2, 0x0A, 0x1C, 0x1C, 0x08, F_GCN,          "SeabedUpper",         "SU",   "Seabed Upper Levels", {"seabedupper"}},
    {Episode::EP2, 0x0B, 0x1D, 0x1D, 0x09, F_GCN,          "SeabedLower",         "SL",   "Seabed Lower Levels", {"seabedlower"}},
    {Episode::EP2, 0x0C, 0x1E, 0x1E, 0x08, F_GCN | F_BOSS, "GalGryphon",          "GG",   "Cliffs of Gal Da Val (Gal Gryphon)", {"galgryphon"}},
    {Episode::EP2, 0x0D, 0x1F, 0x1F, 0x09, F_GCN | F_BOSS, "OlgaFlow",            "OF",   "Test Subject Disposal Area (Olga Flow)", {"olgaflow"}},
    {Episode::EP2, 0x0E, 0x20, 0x20, 0x02, F_GCN | F_BOSS, "BarbaRay",            "BR",   "VR Temple Final (Barba Ray)", {"barbaray"}},
    {Episode::EP2, 0x0F, 0x21, 0x21, 0x04, F_GCN | F_BOSS, "GolDragon",           "GD",   "VR Spaceship Final (Gol Dragon)", {"goldragon"}},
    {Episode::EP2, 0x10, 0xFF, 0x22, 0x07, F_V3,           "SeasideNight",        "SSN",  "Seaside Area (night)", {"seasidenight"}},
    {Episode::EP2, 0x11, 0xFF, 0x23, 0x09, F_V3,           "Tower",               "Twr",  "Control Tower", {"tower"}},

    {Episode::EP4, 0x00, 0xFF, 0x2D, 0xFF, F_V4 | F_CITY,  "Pioneer2",            "P2",   "Pioneer 2", {"pioneer2", "p2", "city"}},
    {Episode::EP4, 0x01, 0xFF, 0x24, 0x01, F_V4,           "CraterEast",          "CE",   "Crater (Eastern Route)", {"cratereast", "ce"}},
    {Episode::EP4, 0x02, 0xFF, 0x25, 0x02, F_V4,           "CraterWest",          "CW",   "Crater (Western Route)", {"craterwest", "cw"}},
    {Episode::EP4, 0x03, 0xFF, 0x26, 0x03, F_V4,           "CraterSouth",         "CS",   "Crater (Southern Route)", {"cratersouth", "cs"}},
    {Episode::EP4, 0x04, 0xFF, 0x27, 0x04, F_V4,           "CraterNorth",         "CN",   "Crater (Northern Route)", {"craternorth", "cn"}},
    {Episode::EP4, 0x05, 0xFF, 0x28, 0x05, F_V4,           "CraterInterior",      "CI",   "Crater Interior", {"craterinterior", "ci"}},
    {Episode::EP4, 0x06, 0xFF, 0x29, 0x06, F_V4,           "Desert1",             "D1",   "Subterranean Desert 1", {"desert1", "d1"}},
    {Episode::EP4, 0x07, 0xFF, 0x2A, 0x07, F_V4,           "Desert2",             "D2",   "Subterranean Desert 2", {"desert2", "d2"}},
    {Episode::EP4, 0x08, 0xFF, 0x2B, 0x08, F_V4,           "Desert3",             "D3",   "Subterranean Desert 3", {"desert3", "d3"}},
    {Episode::EP4, 0x09, 0xFF, 0x2C, 0x09, F_V4 | F_BOSS,  "SaintMilion",         "SM",   "Meteor Impact Site", {"saintmilion"}},
    {Episode::EP4, 0x0A, 0xFF, 0x2E, 0xFF, F_V4,           "TestMap",             "TM",   "Test Map", {"purgatory"}},
    // clang-format on
};

const FloorDefinition& FloorDefinition::get(Episode episode, uint8_t floor) {
  if (floor >= FloorDefinition::limit_for_episode(episode)) {
    throw runtime_error("invalid floor number");
  }
  switch (episode) {
    case Episode::EP1:
      return floor_defs.at(floor);
    case Episode::EP2:
      return floor_defs.at(floor + 0x12);
    case Episode::EP4:
      return floor_defs.at(floor + 0x24);
    default:
      throw logic_error("invalid episode");
  }
}

const FloorDefinition& FloorDefinition::get_by_drop_area_norm(Episode episode, uint8_t area_norm) {
  switch (episode) {
    case Episode::EP1:
      return floor_defs[area_norm + 1];
    case Episode::EP2:
      return floor_defs[0x13 + (area_norm >= 4) + area_norm];
    case Episode::EP4:
      return floor_defs[area_norm + 0x24];
    default:
      throw logic_error("invalid episode number");
  }
}

const FloorDefinition& FloorDefinition::get(Episode episode, const std::string& name) {
  static unordered_map<std::string, size_t> index_ep1;
  static unordered_map<std::string, size_t> index_ep2;
  static unordered_map<std::string, size_t> index_ep4;

  unordered_map<std::string, size_t>* index;
  switch (episode) {
    case Episode::EP1:
      index = &index_ep1;
      break;
    case Episode::EP2:
      index = &index_ep2;
      break;
    case Episode::EP4:
      index = &index_ep4;
      break;
    default:
      throw logic_error("invalid episode");
  }

  if (index->empty()) {
    for (size_t z = 0; z < floor_defs.size(); z++) {
      const auto& def = floor_defs[z];
      if (def.episode != episode) {
        continue;
      }
      for (const auto& alias : def.aliases) {
        index->emplace(alias, z);
      }
    }
  }

  return floor_defs[index->at(phosg::tolower(name))];
}

size_t FloorDefinition::limit_for_episode(Episode ep) {
  switch (ep) {
    case Episode::EP1:
    case Episode::EP2:
      return 0x12;
    case Episode::EP4:
      return 0x0B;
    default:
      return 0x00;
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
