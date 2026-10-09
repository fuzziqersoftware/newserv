#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Tools.hh>

#include "QuestMetadata.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

struct QuestOpcodeIndex {
  struct OpcodeDefinition {
    struct Argument {
      enum class Type {
        LABEL16 = 0,
        LABEL16_SET,
        LABEL32,
        R_REG,
        W_REG,
        R_REG_SET,
        R_REG_SET_FIXED, // Sequence of N consecutive regs
        W_REG_SET_FIXED, // Sequence of N consecutive regs
        R_REG32,
        W_REG32,
        R_REG32_SET_FIXED, // Sequence of N consecutive regs
        W_REG32_SET_FIXED, // Sequence of N consecutive regs
        I8,
        I16,
        I32,
        FLOAT32,
        CSTRING,
      };

      enum class DataType {
        NONE = 0,
        SCRIPT,
        DATA,
        CSTRING,
        PLAYER_STATS,
        PLAYER_VISUAL_CONFIG,
        RESIST_DATA,
        ATTACK_DATA,
        MOVEMENT_DATA,
        IMAGE_DATA,
        VECTOR4F_LIST,
      };

      Type type;
      size_t count;
      DataType data_type;
      std::string name;

      Argument(Type type, size_t count = 0, const char* name = "");
      Argument(Type type, DataType data_type, const char* name = "");
      explicit Argument(const phosg::JSON& json);
      phosg::JSON json() const;
    };

    uint16_t opcode;
    // By convention, the first of these names is the canonical name (used when disassembling) and the last is the
    // Qedit name. When assembling, all names in this list are considered valid.
    std::vector<std::string> names;
    std::vector<Argument> args;
    uint32_t flags;

    OpcodeDefinition(
        uint16_t opcode,
        std::initializer_list<std::string> names,
        std::initializer_list<Argument> args,
        uint32_t flags);
    explicit OpcodeDefinition(const phosg::JSON& json);
    phosg::JSON json() const;
    std::string str() const;
  };

  static const std::array<OpcodeDefinition, 602> DEFAULT_OPCODE_DEFS;

  std::vector<OpcodeDefinition> custom_opcodes;
  std::array<std::unordered_map<uint16_t, const OpcodeDefinition*>, NUM_VERSIONS> by_opcode;
  std::array<std::unordered_map<std::string, const OpcodeDefinition*>, NUM_VERSIONS> by_name;

  explicit QuestOpcodeIndex(std::vector<OpcodeDefinition>&& custom_opcodes = {});
  explicit QuestOpcodeIndex(const phosg::JSON& custom_opcodes_json);
  void add_opcodes(const OpcodeDefinition* opcodes, size_t count, bool overwrite);
  const std::unordered_map<uint16_t, const OpcodeDefinition*>& by_opcode_for_version(Version v) const;
  const std::unordered_map<std::string, const OpcodeDefinition*>& by_name_for_version(Version v) const;
};

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

struct PSOQuestHeaderV3 {
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ Language language = Language::JAPANESE;
  /* 0011 */ uint8_t unknown_a3 = 0;
  // Note: The GC client byteswaps this field, then loads it as a byte, so technically the high byte of this is what
  // the client uses as the quest number. In practice, this only matters if the quest runs send_statistic without
  // running prepare_statistic first, which is not the intended usage.
  /* 0012 */ le_uint16_t quest_number = 0;
  /* 0014 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0034 */ pstring<TextEncoding::MARKED, 0x80> short_description;
  /* 00B4 */ pstring<TextEncoding::MARKED, 0x120> long_description;
  /* 01D4 */
} __packed_ws__(PSOQuestHeaderV3, 0x1D4);

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

// Some quest authoring tools don't generate the full quest header, hence the split structure here.
struct PSOQuestHeaderBBBase {
  /* 0000 */ le_uint32_t text_offset = 0;
  /* 0004 */ le_uint32_t label_table_offset = 0;
  /* 0008 */ le_uint32_t size = 0;
  /* 000C */ le_uint16_t unknown_a1 = 0xFFFF;
  /* 000E */ le_uint16_t unknown_a2 = 0xFFFF;
  /* 0010 */ le_uint16_t quest_number = 0; // 0xFFFF for challenge quests
  /* 0012 */ le_uint16_t unknown_a6 = 0;
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
  struct FloorAssignment {
    // These fields match the bb_map_designate arguments (see QuestScript.cc). Unused AreaAssignment structures should
    // have all fields set to 0xFF.
    uint8_t floor = 0xFF;
    uint8_t area = 0xFF;
    uint8_t type = 0xFF;
    uint8_t layout_var = 0xFF;
    uint8_t entities_var = 0xFF;
    parray<uint8_t, 3> unused = 0xFF;
  } __packed_ws__(FloorAssignment, 8);

  /* 0398 */ le_uint32_t unknown_a5;
  /* 039C */ parray<le_uint16_t, 8> solo_unlock_flags;
  /* 03AC */ parray<FloorAssignment, 0x10> floor_assignments;
  /* 042C */ parray<CreateItemMaskEntry, 0x40> create_item_mask_entries;
  /* 122C */
} __packed_ws__(PSOQuestHeaderBB, 0x122C);

void check_default_quest_opcode_definitions();

Episode episode_for_quest_episode_number(uint8_t episode_number);

std::string disassemble_quest_script(
    const QuestOpcodeIndex& opcode_index,
    const void* bin_data, // Must not be null
    size_t bin_size,
    Version version,
    Language language = Language::UNKNOWN,
    std::shared_ptr<const MapFile> dat = nullptr,
    bool reassembly_mode = false,
    bool use_qedit_names = false);

struct AssembledQuestScript {
  std::string data;
  QuestMetadata meta;
};
AssembledQuestScript assemble_quest_script(
    const QuestOpcodeIndex& opcode_index,
    std::string_view text,
    const std::vector<std::string>& script_include_directories,
    const std::vector<std::string>& native_include_directories,
    bool strict = true);

void populate_quest_metadata_from_script(
    const QuestOpcodeIndex& opcode_index,
    QuestMetadata& meta,
    const void* data,
    size_t size,
    Version version,
    Language language);
