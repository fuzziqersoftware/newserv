#pragma once

#include <stdint.h>

#include <unordered_map>

#include "FileContentsCache.hh"
#include "Player.hh"

enum class Episode {
  NONE = 0,
  EP1 = 1,
  EP2 = 2,
  EP3 = 3,
  EP4 = 4,
};

size_t area_limit_for_episode(Episode ep);
bool episode_has_arpg_semantics(Episode ep);
const char* name_for_episode(Episode ep);
const char* abbreviation_for_episode(Episode ep);

enum class GameMode {
  NORMAL = 0,
  BATTLE = 1,
  CHALLENGE = 2,
  SOLO = 3,
};

const char* name_for_mode(GameMode mode);
const char* abbreviation_for_mode(GameMode mode);

size_t max_stack_size_for_item(uint8_t data0, uint8_t data1);

extern const vector<string> tech_id_to_name;
extern const unordered_map<string, uint8_t> name_to_tech_id;

const std::string& name_for_technique(uint8_t tech);
std::u16string u16name_for_technique(uint8_t tech);
uint8_t technique_for_name(const std::string& name);
uint8_t technique_for_name(const std::u16string& name);

const std::string& name_for_section_id(uint8_t section_id);
std::u16string u16name_for_section_id(uint8_t section_id);
uint8_t section_id_for_name(const std::string& name);
uint8_t section_id_for_name(const std::u16string& name);

const std::string& name_for_event(uint8_t event);
std::u16string u16name_for_event(uint8_t event);
uint8_t event_for_name(const std::string& name);
uint8_t event_for_name(const std::u16string& name);

const std::string& name_for_lobby_type(uint8_t type);
std::u16string u16name_for_lobby_type(uint8_t type);
uint8_t lobby_type_for_name(const std::string& name);
uint8_t lobby_type_for_name(const std::u16string& name);

const std::string& name_for_npc(uint8_t npc);
std::u16string u16name_for_npc(uint8_t npc);
uint8_t npc_for_name(const std::string& name);
uint8_t npc_for_name(const std::u16string& name);

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
char abbreviation_for_difficulty(uint8_t difficulty);

char char_for_language_code(uint8_t language);
