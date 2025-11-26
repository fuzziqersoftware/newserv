#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Tools.hh>

#include "QuestMetadata.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

struct PSOQuestHeaderDCNTE {
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ pstring<TextEncoding::SJIS, 0x10> name;
  /* 0020 */
} __packed_ws__(PSOQuestHeaderDCNTE, 0x20);

struct PSOQuestHeaderDC112000 {
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0030 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B0 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D0 */
} __packed_ws__(PSOQuestHeaderDC112000, 0x1D0);

struct PSOQuestHeaderDC { // Same format for DC v1 and v2
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ Language language = Language::JAPANESE;
  /* 0011 */ uint8_t unknown_a3 = 0;
  /* 0012 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __packed_ws__(PSOQuestHeaderDC, 0x1D4);

struct PSOQuestHeaderPC {
  /* 0000 */ le_uint32_t text_offset;
  /* 0004 */ le_uint32_t label_table_offset;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ Language language = Language::JAPANESE;
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
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ Language language = Language::JAPANESE;
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

struct CreateItemMaskEntry {
  parray<le_int32_t, 12> data1_fields;
  le_uint32_t present = 0;
  le_uint32_t unknown_a3 = 0;

  bool is_valid() const {
    return (this->data1_fields[0] || this->data1_fields[1] || this->data1_fields[2]);
  }

  CreateItemMaskEntry() = default;
  CreateItemMaskEntry(const QuestMetadata::CreateItemMask& mask);
  operator QuestMetadata::CreateItemMask() const;
} __packed_ws__(CreateItemMaskEntry, 0x38);

struct PSOQuestHeaderBBBase {
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0012 */ le_uint16_t unknown_a3 = 0;
  /* 0014 */ uint8_t episode = 0; // 0 = Ep1, 1 = Ep2, 2 = Ep4
  /* 0015 */ uint8_t max_players = 0; // 0 means no limit (that is, 4)
  /* 0016 */ uint8_t joinable = 0;
  /* 0017 */ uint8_t unknown_a4 = 0;
  /* 0018 */ pstring<TextEncoding::UTF16, 0x20> name;
  /* 0058 */ pstring<TextEncoding::UTF16, 0x80> short_description;
  /* 0158 */ pstring<TextEncoding::UTF16, 0x120> long_description;
  /* 0398 */
} __packed_ws__(PSOQuestHeaderBBBase, 0x0398);

struct PSOQuestHeaderBB : PSOQuestHeaderBBBase {
  /* 0398 */ parray<uint8_t, 0x94> unknown_a5;
  /* 042C */ parray<CreateItemMaskEntry, 0x40> create_item_mask_entries;
  /* 122C */
} __packed_ws__(PSOQuestHeaderBB, 0x122C);

void check_opcode_definitions();

Episode episode_for_quest_episode_number(uint8_t episode_number);

std::string disassemble_quest_script(
    const void* bin_data, // Must not be null
    size_t bin_size,
    Version version,
    Language language = Language::UNKNOWN,
    std::shared_ptr<const MapFile> dat = nullptr,
    bool reassembly_mode = false,
    bool use_qedit_names = false);

struct AssembledQuestScript {
  std::string data;
  int64_t quest_number = -1;
  Version version = Version::UNKNOWN;
  Language language = Language::UNKNOWN;
  Episode episode = Episode::NONE;
  bool joinable = false;
  uint8_t max_players = 4;
  std::string name;
  std::string short_description;
  std::string long_description;
};
AssembledQuestScript assemble_quest_script(
    const std::string& text,
    const std::vector<std::string>& script_include_directories,
    const std::vector<std::string>& native_include_directories,
    bool strict = true);

void populate_quest_metadata_from_script(QuestMetadata& meta, const void* data, size_t size, Version version, Language language);
