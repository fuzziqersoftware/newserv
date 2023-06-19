#include "StaticGameData.hh"

#include <array>

using namespace std;

size_t area_limit_for_episode(Episode ep) {
  switch (ep) {
    case Episode::EP1:
    case Episode::EP2:
      return 17;
    case Episode::EP4:
      return 10;
    default:
      return 0;
  }
}

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

const vector<string> section_id_to_name = {
    "Viridia", "Greennill", "Skyly", "Bluefull", "Purplenum",
    "Pinkal", "Redria", "Oran", "Yellowboze", "Whitill"};

const unordered_map<string, uint8_t> name_to_section_id({
    {"viridia", 0},
    {"greennill", 1},
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
    {0xF4, "some?"},
    {0xF5, "dmorgue"},
    {0xF6, "caelum"},
    {0xF8, "digital"},
    {0xF9, "boss1"},
    {0xFA, "boss2"},
    {0xFB, "boss3"},
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
    {"some?", 0xF4},
    {"dmorgue", 0xF5},
    {"caelum", 0xF6},
    {"digital", 0xF8},
    {"boss1", 0xF9},
    {"boss2", 0xFA},
    {"boss3", 0xFB},
    {"knight", 0xFC},
    {"sky", 0xFE},
    {"morgue", 0xFF},
});

const vector<string> npc_id_to_name({"ninja", "rico", "sonic", "knuckles", "tails", "flowen", "elly"});

const unordered_map<string, uint8_t> name_to_npc_id = {
    {"ninja", 0}, {"rico", 1}, {"sonic", 2}, {"knuckles", 3}, {"tails", 4}, {"flowen", 5}, {"elly", 6}};

const string& name_for_section_id(uint8_t section_id) {
  if (section_id < section_id_to_name.size()) {
    return section_id_to_name[section_id];
  } else {
    static const string ret = "<Unknown section id>";
    return ret;
  }
}

u16string u16name_for_section_id(uint8_t section_id) {
  return decode_sjis(name_for_section_id(section_id));
}

uint8_t section_id_for_name(const string& name) {
  string lower_name = tolower(name);
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

uint8_t section_id_for_name(const u16string& name) {
  return section_id_for_name(encode_sjis(name));
}

const string& name_for_event(uint8_t event) {
  if (event < lobby_event_to_name.size()) {
    return lobby_event_to_name[event];
  } else {
    static const string ret = "<Unknown lobby event>";
    return ret;
  }
}

u16string u16name_for_event(uint8_t event) {
  return decode_sjis(name_for_event(event));
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

uint8_t event_for_name(const u16string& name) {
  return event_for_name(encode_sjis(name));
}

const string& name_for_lobby_type(uint8_t type) {
  try {
    return lobby_type_to_name.at(type);
  } catch (const out_of_range&) {
    static const string ret = "<Unknown lobby type>";
    return ret;
  }
}

u16string u16name_for_lobby_type(uint8_t type) {
  return decode_sjis(name_for_lobby_type(type));
}

uint8_t lobby_type_for_name(const string& name) {
  try {
    return name_to_lobby_type.at(name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name);
    if (x < lobby_type_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0x80;
}

uint8_t lobby_type_for_name(const u16string& name) {
  return lobby_type_for_name(encode_sjis(name));
}

const string& name_for_npc(uint8_t npc) {
  try {
    return npc_id_to_name.at(npc);
  } catch (const out_of_range&) {
    static const string ret = "<Unknown NPC>";
    return ret;
  }
}

u16string u16name_for_npc(uint8_t npc) {
  return decode_sjis(name_for_npc(npc));
}

uint8_t npc_for_name(const string& name) {
  try {
    return name_to_npc_id.at(name);
  } catch (const out_of_range&) {
  }
  try {
    uint64_t x = stoul(name);
    if (x < npc_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) {
  }
  return 0xFF;
}

uint8_t npc_for_name(const u16string& name) {
  return npc_for_name(encode_sjis(name));
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
      "HUcs",
      "RAmr",
      "RAcs",
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
      "Normal",
      "Hard",
      "Very Hard",
      "Ultimate",
  };
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

char char_for_language_code(uint8_t language) {
  switch (language) {
    case 0:
      return 'J';
    case 1:
      return 'E';
    case 2:
      return 'G';
    case 3:
      return 'F';
    case 4:
      return 'S';
    default:
      return '?';
  }
}

size_t max_stack_size_for_item(uint8_t data0, uint8_t data1) {
  if (data0 == 4) {
    return 999999;
  }
  if (data0 == 3) {
    if ((data1 < 9) && (data1 != 2)) {
      return 10;
    } else if (data1 == 0x10) {
      return 99;
    }
  }
  return 1;
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
    static const string ret = "<Unknown technique>";
    return ret;
  }
}

u16string u16name_for_technique(uint8_t tech) {
  return decode_sjis(name_for_technique(tech));
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

uint8_t technique_for_name(const u16string& name) {
  return technique_for_name(encode_sjis(name));
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

uint8_t drop_area_for_name(const std::string& name) {
  static const unordered_map<string, uint8_t> areas({
      {"forest1", 0},
      {"forest2", 1},
      {"dragon", 2},
      {"caves1", 2},
      {"cave1", 2},
      {"caves2", 3},
      {"cave2", 3},
      {"caves3", 4},
      {"cave3", 4},
      {"derolle", 5},
      {"mines1", 5},
      {"mine1", 5},
      {"mines2", 6},
      {"mine2", 6},
      {"volopt", 7},
      {"ruins1", 7},
      {"ruin1", 7},
      {"ruins2", 8},
      {"ruin2", 8},
      {"ruins3", 9},
      {"ruin3", 9},
      {"darkfalz", 9},

      {"vrtemplealpha", 0},
      {"vrtemplebeta", 1},
      {"barbaray", 2},
      {"vrspaceshipalpha", 2},
      {"vrspaceshipbeta", 3},
      {"goldragon", 5},
      {"centralcontrolarea", 4},
      {"cca", 4},
      {"jungleareanorth", 5},
      {"junglenorth", 5},
      {"jungleareaeast", 5},
      {"jungleeast", 5},
      {"mountain", 6},
      {"seaside", 7},
      {"galgryphon", 8},
      {"seabedupper", 8},
      {"seabedlower", 9},
      {"olgaflow", 9},
      {"seasidenight", 7},
      {"tower", 9},

      {"cratereast", 2},
      {"craterwest", 3},
      {"cratersouth", 4},
      {"craternorth", 5},
      {"craterinterior", 6},
      {"subdesert1", 7},
      {"subdesert2", 8},
      {"subdesert3", 9},
      {"saintmillion", 9},
  });
  return areas.at(tolower(name));
}
