#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Tools.hh>

#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

struct PSOQuestHeaderDCNTE {
  /* 0000 */ le_uint32_t code_offset = 0;
  /* 0004 */ le_uint32_t function_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0;
  /* 000E */ le_uint16_t unknown_a2 = 0;
  /* 0010 */ pstring<TextEncoding::SJIS, 0x10> name;
  /* 0020 */
} __packed_ws__(PSOQuestHeaderDCNTE, 0x20);

struct PSOQuestHeaderDC { // Same format for DC v1 and v2
  /* 0000 */ le_uint32_t code_offset = 0;
  /* 0004 */ le_uint32_t function_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0;
  /* 000E */ le_uint16_t unknown_a2 = 0;
  /* 0010 */ uint8_t language = 0;
  /* 0011 */ uint8_t unknown_a3 = 0;
  /* 0012 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __packed_ws__(PSOQuestHeaderDC, 0x1D4);

struct PSOQuestHeaderPC {
  /* 0000 */ le_uint32_t code_offset;
  /* 0004 */ le_uint32_t function_table_offset;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0;
  /* 000E */ le_uint16_t unknown_a2 = 0;
  /* 0010 */ uint8_t language = 0;
  /* 0011 */ uint8_t unknown_a3 = 0;
  /* 0012 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0014 */ pstring<TextEncoding::UTF16, 0x20> name;
  /* 0054 */ pstring<TextEncoding::UTF16, 0x80> short_description;
  /* 0154 */ pstring<TextEncoding::UTF16, 0x120> long_description;
  /* 0394 */
} __packed_ws__(PSOQuestHeaderPC, 0x394);

// TODO: Is the XB quest header format the same as on GC? If not, make a
// separate struct; if so, rename this struct to V3.
struct PSOQuestHeaderGC {
  /* 0000 */ le_uint32_t code_offset = 0;
  /* 0004 */ le_uint32_t function_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0;
  /* 000E */ le_uint16_t unknown_a2 = 0;
  /* 0010 */ uint8_t language = 0;
  /* 0011 */ uint8_t unknown_a3 = 0;
  // Note: The GC client byteswaps this field, then loads it as a byte, so
  // technically the high byte of this is what the client uses as the quest
  // number. In practice, this only matters if the quest runs send_statistic
  // without running prepare_statistic first, which is not the intended usage.
  /* 0012 */ le_uint16_t quest_number = 0;
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __packed_ws__(PSOQuestHeaderGC, 0x1D4);

struct PSOQuestHeaderBB {
  /* 0000 */ le_uint32_t code_offset = 0;
  /* 0004 */ le_uint32_t function_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0;
  /* 000E */ le_uint16_t unknown_a2 = 0;
  /* 0010 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0012 */ le_uint16_t unknown_a3 = 0;
  /* 0014 */ uint8_t episode = 0; // 0 = Ep1, 1 = Ep2, 2 = Ep4
  /* 0015 */ uint8_t max_players = 0;
  /* 0016 */ uint8_t joinable = 0;
  /* 0017 */ uint8_t unknown_a4 = 0;
  /* 0018 */ pstring<TextEncoding::UTF16, 0x20> name;
  /* 0058 */ pstring<TextEncoding::UTF16, 0x80> short_description;
  /* 0158 */ pstring<TextEncoding::UTF16, 0x120> long_description;
  /* 0398 */
} __packed_ws__(PSOQuestHeaderBB, 0x398);

void check_opcode_definitions();

Episode episode_for_quest_episode_number(uint8_t episode_number);

std::string disassemble_quest_script(
    const void* data,
    size_t size,
    Version version,
    uint8_t override_language = 0xFF,
    bool reassembly_mode = false,
    bool use_qedit_names = false);

struct QuestMetadata {
  int64_t quest_number = -1;
  Version version = Version::UNKNOWN;
  uint8_t language = 0xFF;
  Episode episode = Episode::NONE;
  bool joinable = false;
  uint8_t max_players = 0x00;
  std::string name;
  std::string short_description;
  std::string long_description;
};
struct AssembledQuestScript {
  std::string data;
  QuestMetadata metadata;
};
AssembledQuestScript assemble_quest_script(
    const std::string& text,
    const std::vector<std::string>& script_include_directories,
    const std::vector<std::string>& native_include_directories);

Episode find_quest_episode_from_script(const void* data, size_t size, Version version);
