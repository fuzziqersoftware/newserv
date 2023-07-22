#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Tools.hh>

#include "Text.hh"
#include "Version.hh"

enum class QuestScriptVersion {
  DC_NTE = 0,
  DC_V1 = 1,
  DC_V2 = 2,
  PC_V2 = 3,
  GC_NTE = 4,
  GC_V3 = 5,
  XB_V3 = 6,
  GC_EP3 = 7,
  BB_V4 = 8,
  UNKNOWN = 15,
};

template <>
const char* name_for_enum<QuestScriptVersion>(QuestScriptVersion v);

struct PSOQuestHeaderDC { // Same format for DC v1 and v2
  le_uint32_t code_offset;
  le_uint32_t function_table_offset;
  le_uint32_t size;
  le_uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  le_uint16_t quest_number; // 0xFFFF for challenge quests
  ptext<char, 0x20> name;
  ptext<char, 0x80> short_description;
  ptext<char, 0x120> long_description;
} __attribute__((packed));

struct PSOQuestHeaderPC {
  le_uint32_t code_offset;
  le_uint32_t function_table_offset;
  le_uint32_t size;
  le_uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  le_uint16_t quest_number; // 0xFFFF for challenge quests
  ptext<char16_t, 0x20> name;
  ptext<char16_t, 0x80> short_description;
  ptext<char16_t, 0x120> long_description;
} __attribute__((packed));

// TODO: Is the XB quest header format the same as on GC? If not, make a
// separate struct; if so, rename this struct to V3.
struct PSOQuestHeaderGC {
  le_uint32_t code_offset;
  le_uint32_t function_table_offset;
  le_uint32_t size;
  le_uint32_t unused;
  uint8_t is_download;
  uint8_t unknown1;
  uint8_t quest_number;
  uint8_t episode; // 1 = Ep2. Apparently some quests have 0xFF here, which means ep1 (?)
  ptext<char, 0x20> name;
  ptext<char, 0x80> short_description;
  ptext<char, 0x120> long_description;
} __attribute__((packed));

struct PSOQuestHeaderBB {
  le_uint32_t code_offset;
  le_uint32_t function_table_offset;
  le_uint32_t size;
  le_uint32_t unused;
  le_uint16_t quest_number; // 0xFFFF for challenge quests
  le_uint16_t unused2;
  uint8_t episode; // 0 = Ep1, 1 = Ep2, 2 = Ep4
  uint8_t max_players;
  uint8_t joinable_in_progress;
  uint8_t unknown;
  ptext<char16_t, 0x20> name;
  ptext<char16_t, 0x80> short_description;
  ptext<char16_t, 0x120> long_description;
} __attribute__((packed));

std::string disassemble_quest_script(const void* data, size_t size, QuestScriptVersion version);
