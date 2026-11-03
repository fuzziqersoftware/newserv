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
        // These are the argument data types. All values are stored little-endian in the script data, even on GC.
        LABEL16 = 0, // 16-bit index into the function table
        LABEL16_SET, // Byte specifying how many labels, followed by that many 16-bit indexes into the function table
        LABEL32, // 32-bit index into the function table
        R_REG, // Byte specifying a register number (rXX or fXX) which is read by the opcode and not modified
        W_REG, // Same as above, but register is modified by the opcode and not read
        RW_REG, // Same as above, but register is both read by the opcode and then modified
        R_REG_SET, // Byte specifying how many registers, followed by that many bytes specifying register numbers
        R_REG32, // Same as R_REG, but register number specified as 32-bit value
        W_REG32, // Same as W_REG, but register number specified as 32-bit value
        RW_REG32, // Same as RW_REG, but register number specified as 32-bit value
        // REG_SET_FIXED is a fixed-size set of registers with heterogenous meanings; the meanings, types, names, etc.
        // are specified in sub_args
        REG_SET_FIXED, // Sequence of consecutive regs (meanings defined in sub_args) specified by the first reg number
        REG32_SET_FIXED, // Same as above, but register number specified as 32-bit value
        // REG_TUPLE_FIXED is a fixed-size set of registers with homogenous meanings (e.g. bytes in item.data1); the
        // meaning of each is specified by this Argument struct (e.g. data_type applies to all of them) and they do not
        // have individual names. The count is specified by reg_count
        R_REG_TUPLE_FIXED, // Sequence of consecutive regs specified by the first reg number
        R_REG32_TUPLE_FIXED, // Same as above, but register number specified as 32-bit value
        W_REG_TUPLE_FIXED, // Sequence of consecutive regs specified by the first reg number
        W_REG32_TUPLE_FIXED, // Same as above, but register number specified as 32-bit value
        I8, // 1-byte unsigned immediate integer
        I16, // 2-byte unsigned immediate integer
        I32, // 4-byte unsigned immediate integer
        FLOAT32, // 4-byte immediate floating-point value
        CSTRING, // Sequence of nonzero bytes ending with a zero byte
      };

      enum class DataType {
        // This specifies how the data following a label should be interpreted by the disassembler
        NONE = 0, // Unknown data type
        FLOAT, // Only valid for REG/REG32 argument types; means the value is encoded as a float instead of an int
        SCRIPT, // Quest script opcodes
        DATA, // Raw data
        CSTRING, // Text
        PLAYER_STATS, // PlayerStats structure
        PLAYER_VISUAL_CONFIG, // PlayerVisualConfig structure
        RESIST_DATA, // ResistData structure
        ATTACK_DATA, // AttackData structure
        MOVEMENT_DATA, // MovementData structure
        IMAGE_DATA, // Compressed image data
        VECTOR4F_LIST, // List of VectorXYZTF structures
      };

      static constexpr int64_t NO_DEFAULT_VALUE = -0x8000000000000000LL;
      Type type;
      DataType data_type;
      std::vector<Argument> sub_args; // Only used for REG_SET_FIXED types
      size_t reg_count = 0; // Only used for REG_TUPLE_FIXED types
      std::string name;
      std::string write_name; // Only used for RW_ types
      int64_t default_value = NO_DEFAULT_VALUE;

      Argument(Type type, const char* name, int64_t default_value = NO_DEFAULT_VALUE);
      Argument(Type type, const char* name, const char* write_name);
      Argument(Type type, DataType data_type, const char* name, int64_t default_value = NO_DEFAULT_VALUE);
      Argument(Type type, std::vector<Argument> sub_args);
      Argument(Type type, std::initializer_list<Argument> sub_args);
      Argument(Type type, std::vector<const char*> ro_names);
      Argument(Type type, std::initializer_list<const char*> ro_names);
      explicit Argument(const phosg::JSON& json);
      phosg::JSON json() const;

      inline bool is_output_arg() const {
        if ((this->type == Type::W_REG) ||
            (this->type == Type::RW_REG) ||
            (this->type == Type::W_REG32) ||
            (this->type == Type::RW_REG32) ||
            (this->type == Type::W_REG_TUPLE_FIXED) ||
            (this->type == Type::W_REG32_TUPLE_FIXED)) {
          return true;
        }
        for (const auto& sub_arg : this->sub_args) {
          if (sub_arg.is_output_arg()) {
            return true;
          }
        }
        return false;
      }
      inline bool is_reg32_type() const {
        return (this->type == Type::R_REG32) ||
            (this->type == Type::W_REG32) ||
            (this->type == Type::RW_REG32) ||
            (this->type == Type::REG32_SET_FIXED) ||
            (this->type == Type::R_REG32_TUPLE_FIXED) ||
            (this->type == Type::W_REG32_TUPLE_FIXED);
      }
      inline bool is_reg_tuple_type() const {
        return (this->type == Type::R_REG_TUPLE_FIXED) ||
            (this->type == Type::R_REG32_TUPLE_FIXED) ||
            (this->type == Type::W_REG_TUPLE_FIXED) ||
            (this->type == Type::W_REG32_TUPLE_FIXED);
      }
      inline size_t effective_reg_count() const {
        return ((this->type == Type::REG_SET_FIXED) || (this->type == Type::REG32_SET_FIXED))
            ? this->sub_args.size()
            : this->reg_count;
      }
    };

    uint16_t opcode;
    // By convention, the first of these names is the canonical name (used when disassembling) and the last is the
    // Qedit name. When assembling, all names in this list are considered valid.
    std::vector<std::string> names;
    std::vector<Argument> args;
    uint32_t flags;

    OpcodeDefinition() = default;
    explicit OpcodeDefinition(const phosg::JSON& json);
    phosg::JSON json() const;
    std::string str() const;
  };

  static std::vector<OpcodeDefinition> create_default_opcode_defs();

  std::vector<OpcodeDefinition> default_opcodes;
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

template <>
const char* phosg::name_for_enum<QuestOpcodeIndex::OpcodeDefinition::Argument::Type>(
    QuestOpcodeIndex::OpcodeDefinition::Argument::Type v);
template <>
QuestOpcodeIndex::OpcodeDefinition::Argument::Type
phosg::enum_for_name<QuestOpcodeIndex::OpcodeDefinition::Argument::Type>(const char* name);

template <>
const char* phosg::name_for_enum<QuestOpcodeIndex::OpcodeDefinition::Argument::DataType>(
    QuestOpcodeIndex::OpcodeDefinition::Argument::DataType v);
template <>
QuestOpcodeIndex::OpcodeDefinition::Argument::DataType
phosg::enum_for_name<QuestOpcodeIndex::OpcodeDefinition::Argument::DataType>(const char* name);
