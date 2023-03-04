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



size_t stack_size_for_item(uint8_t data0, uint8_t data1);
size_t stack_size_for_item(const ItemData& item);

extern const std::unordered_map<uint8_t, const char*> name_for_weapon_special;
extern const std::unordered_map<uint8_t, const char*> name_for_s_rank_special;
extern const std::unordered_map<uint32_t, const char*> name_for_primary_identifier;

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

const char* name_for_difficulty(uint8_t difficulty);
char abbreviation_for_difficulty(uint8_t difficulty);

char char_for_language_code(uint8_t language);

std::string name_for_item(const ItemData& item, bool include_color_codes);
