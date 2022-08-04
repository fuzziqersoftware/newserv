#pragma once

#include <stdint.h>

#include <unordered_map>
#include <random>

#include "FileContentsCache.hh"
#include "Player.hh"



void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    std::shared_ptr<std::mt19937> random,
    uint8_t episode,
    bool is_solo);
std::shared_ptr<const FileContentsCache::File> map_data_for_variation(
    uint8_t episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2);
void load_map_files();

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

const char* abbreviation_for_game_mode(uint8_t);

std::string name_for_item(const ItemData& item, bool include_color_codes);
