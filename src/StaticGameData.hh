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

bool episode_has_arpg_semantics(Episode ep);
const char* name_for_episode(Episode ep);
const char* token_name_for_episode(Episode ep);
const char* abbreviation_for_episode(Episode ep);
Episode episode_for_token_name(const std::string& name);

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

const char* name_for_difficulty(uint8_t difficulty);
const char* token_name_for_difficulty(uint8_t difficulty);
char abbreviation_for_difficulty(uint8_t difficulty);

const char* name_for_language_code(uint8_t language_code);
char char_for_language_code(uint8_t language_code);
uint8_t language_code_for_char(char language_char);

extern const std::vector<const char*> name_for_mag_color;
extern const std::unordered_map<std::string, uint8_t> mag_color_for_name;

size_t floor_limit_for_episode(Episode ep);
uint8_t floor_for_name(const std::string& name);
const char* name_for_floor(Episode episode, uint8_t floor);
const char* short_name_for_floor(Episode episode, uint8_t floor);
bool floor_is_boss_arena(Episode episode, uint8_t floor);

uint32_t class_flags_for_class(uint8_t char_class);

char char_for_challenge_rank(uint8_t rank);

extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V3;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP1;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP2;
extern const std::array<size_t, 4> DEFAULT_MIN_LEVELS_V4_EP4;
