#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "FileContentsCache.hh"
#include "Version.hh"

enum class Episode {
  NONE = 0,
  EP1 = 1,
  EP2 = 2,
  EP3 = 3,
  EP4 = 4,
};

enum class Difficulty : uint8_t {
  NORMAL = 0,
  HARD = 1,
  VERY_HARD = 2,
  ULTIMATE = 3,
  UNKNOWN = 0xFF,
};

enum class Language : uint8_t {
  JAPANESE = 0,
  ENGLISH = 1,
  GERMAN = 2,
  FRENCH = 3,
  SPANISH = 4,
  SIMPLIFIED_CHINESE = 5,
  TRADITIONAL_CHINESE = 6,
  KOREAN = 7,
  UNKNOWN = 0xFF,
};

bool episode_has_arpg_semantics(Episode ep);
const char* name_for_episode(Episode ep);
const char* token_name_for_episode(Episode ep);
const char* abbreviation_for_episode(Episode ep);
Episode episode_for_token_name(const std::string& name);
Episode episode_for_area(uint8_t area);

enum class GameMode {
  NORMAL = 0,
  BATTLE = 1,
  CHALLENGE = 2,
  SOLO = 3,
};

const char* name_for_mode(GameMode mode);
const char* abbreviation_for_mode(GameMode mode);

extern const std::vector<std::string> tech_id_to_name;
extern const std::unordered_map<std::string, uint8_t> name_to_tech_id;

const std::string& name_for_technique(uint8_t tech);
uint8_t technique_for_name(const std::string& name);

const char* abbreviation_for_section_id(uint8_t section_id);
const char* name_for_section_id(uint8_t section_id);
uint8_t section_id_for_name(const std::string& name);

const std::string& name_for_event(uint8_t event);
uint8_t event_for_name(const std::string& name);

const std::string& name_for_lobby_type(uint8_t type);
uint8_t lobby_type_for_name(const std::string& name);

const std::string& name_for_npc(uint8_t npc);
uint8_t npc_for_name(const std::string& name, Version version);
bool npc_valid_for_version(uint8_t npc, Version version);

const char* name_for_char_class(uint8_t cls);
const char* abbreviation_for_char_class(uint8_t cls);
bool char_class_is_male(uint8_t cls);
bool char_class_is_human(uint8_t cls);
bool char_class_is_newman(uint8_t cls);
bool char_class_is_android(uint8_t cls);
bool char_class_is_hunter(uint8_t cls);
bool char_class_is_ranger(uint8_t cls);
bool char_class_is_force(uint8_t cls);

const char* name_for_difficulty(Difficulty difficulty);
const char* token_name_for_difficulty(Difficulty difficulty);
char abbreviation_for_difficulty(Difficulty difficulty);

const char* name_for_language(Language language);
char char_for_language(Language language);
Language language_for_char(char language_char);

extern const std::vector<const char*> name_for_mag_color;
extern const std::unordered_map<std::string, uint8_t> mag_color_for_name;

struct FloorDefinition {
  enum Flag {
    CITY = 0x01,
    LOBBY = 0x02,
    BOSS_ARENA = 0x04,
    EXISTS_ON_V1 = 0x08,
    EXISTS_ON_V2 = 0x10,
    EXISTS_ON_GC_NTE = 0x20,
    EXISTS_ON_V3 = 0x40,
    EXISTS_ON_V4 = 0x80,
  };
  Episode episode;
  uint8_t floor;
  uint8_t gc_nte_area;
  uint8_t area;
  uint8_t drop_area_norm;
  uint8_t flags;
  const char* json_name;
  const char* short_name;
  const char* in_game_name;
  std::vector<const char*> aliases;

  static const FloorDefinition& get(Episode episode, uint8_t floor);
  static const FloorDefinition& get(Episode episode, const std::string& name);
  static const FloorDefinition& get_by_drop_area_norm(Episode episode, uint8_t area_norm);
  static size_t limit_for_episode(Episode ep);
};

uint32_t class_flags_for_class(uint8_t char_class);

char char_for_challenge_rank(uint8_t rank);

extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V123;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP1;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP2;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP4;

extern const std::array<GameMode, 2> ALL_GAME_MODES_V1;
extern const std::array<GameMode, 3> ALL_GAME_MODES_V23;
extern const std::array<GameMode, 4> ALL_GAME_MODES_V4;
extern const std::array<Episode, 1> ALL_EPISODES_V12;
extern const std::array<Episode, 2> ALL_EPISODES_V3;
extern const std::array<Episode, 3> ALL_EPISODES_V4;
extern const std::array<Difficulty, 3> ALL_DIFFICULTIES_V1;
extern const std::array<Difficulty, 4> ALL_DIFFICULTIES_V234;
