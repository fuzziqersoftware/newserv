#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Tools.hh>

#include "StaticGameData.hh"
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
  /* 0000 */ le_uint32_t code_offset;
  /* 0004 */ le_uint32_t function_table_offset;
  /* 0008 */ le_uint32_t size;
  /* 000C */ le_uint32_t unused;
  /* 0010 */ uint8_t language;
  /* 0011 */ uint8_t unknown1;
  /* 0012 */ le_uint16_t quest_number; // 0xFFFF for challenge quests
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __attribute__((packed));

struct PSOQuestHeaderPC {
  /* 0000 */ le_uint32_t code_offset;
  /* 0004 */ le_uint32_t function_table_offset;
  /* 0008 */ le_uint32_t size;
  /* 000C */ le_uint32_t unused;
  /* 0010 */ uint8_t language;
  /* 0011 */ uint8_t unknown1;
  /* 0012 */ le_uint16_t quest_number; // 0xFFFF for challenge quests
  /* 0014 */ pstring<TextEncoding::UTF16, 0x20> name;
  /* 0054 */ pstring<TextEncoding::UTF16, 0x80> short_description;
  /* 0154 */ pstring<TextEncoding::UTF16, 0x120> long_description;
  /* 0394 */
} __attribute__((packed));

// TODO: Is the XB quest header format the same as on GC? If not, make a
// separate struct; if so, rename this struct to V3.
struct PSOQuestHeaderGC {
  /* 0000 */ le_uint32_t code_offset;
  /* 0004 */ le_uint32_t function_table_offset;
  /* 0008 */ le_uint32_t size;
  /* 000C */ le_uint32_t unused;
  /* 0010 */ uint8_t language;
  /* 0011 */ uint8_t unknown1;
  /* 0012 */ uint8_t quest_number;
  /* 0013 */ uint8_t episode; // 1 = Ep2. Apparently some quests have 0xFF here, which means ep1 (?)
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __attribute__((packed));

struct PSOQuestHeaderBB {
  /* 0000 */ le_uint32_t code_offset;
  /* 0004 */ le_uint32_t function_table_offset;
  /* 0008 */ le_uint32_t size;
  /* 000C */ le_uint32_t unused;
  /* 0010 */ le_uint16_t quest_number; // 0xFFFF for challenge quests
  /* 0012 */ le_uint16_t unused2;
  /* 0014 */ uint8_t episode; // 0 = Ep1, 1 = Ep2, 2 = Ep4
  /* 0015 */ uint8_t max_players;
  /* 0016 */ uint8_t joinable_in_progress;
  /* 0017 */ uint8_t unknown;
  /* 0018 */ pstring<TextEncoding::UTF16, 0x20> name;
  /* 0058 */ pstring<TextEncoding::UTF16, 0x80> short_description;
  /* 0158 */ pstring<TextEncoding::UTF16, 0x120> long_description;
  /* 0398 */
} __attribute__((packed));

Episode episode_for_quest_episode_number(uint8_t episode_number);

std::string disassemble_quest_script(
    const void* data, size_t size, QuestScriptVersion version, uint8_t language);

Episode find_quest_episode_from_script(const void* data, size_t size, QuestScriptVersion version);
