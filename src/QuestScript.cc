#include "QuestScript.hh"

#include <stdint.h>

#include <array>
#include <deque>
#include <map>
#include <phosg/Strings.hh>
#include <set>
#include <unordered_map>
#include <vector>

#include "CommandFormats.hh"
#include "Compression.hh"
#include "StaticGameData.hh"

using namespace std;

static string format_and_indent_data(const void* data, size_t size, uint64_t start_address) {
  struct iovec iov;
  iov.iov_base = const_cast<void*>(data);
  iov.iov_len = size;

  string ret = "  ";
  format_data(
      [&ret](const void* vdata, size_t size) -> void {
        const char* data = reinterpret_cast<const char*>(vdata);
        for (size_t z = 0; z < size; z++) {
          if (data[z] == '\n') {
            ret += "\n  ";
          } else {
            ret.push_back(data[z]);
          }
        }
      },
      &iov, 1, start_address, nullptr, 0, PrintDataFlags::PRINT_ASCII);

  strip_trailing_whitespace(ret);
  return ret;
}

static string dasm_u16string(const char16_t* data, size_t size) {
  try {
    return format_data_string(encode_sjis(data, size));
  } catch (const runtime_error& e) {
    return "/* undecodable */ " + format_data_string(data, size * sizeof(char16_t));
  }
}

template <size_t Size>
static string dasm_u16string(const parray<char16_t, Size>& data) {
  return dasm_u16string(data.data(), data.size());
}

struct ResistData {
  le_int16_t evp_bonus;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  le_uint32_t unknown_a6;
  le_uint32_t unknown_a7;
  le_uint32_t unknown_a8;
  le_uint32_t unknown_a9;
  le_int32_t dfp_bonus;
} __attribute__((packed));

struct AttackData {
  le_int16_t unknown_a1;
  le_int16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_float unknown_a5;
  le_uint32_t unknown_a6;
  le_float unknown_a7;
  le_uint16_t unknown_a8;
  le_uint16_t unknown_a9;
  le_uint16_t unknown_a10;
  le_uint16_t unknown_a11;
  le_uint32_t unknown_a12;
  le_uint32_t unknown_a13;
  le_uint32_t unknown_a14;
  le_uint32_t unknown_a15;
  le_uint32_t unknown_a16;
} __attribute__((packed));

struct MovementData {
  parray<le_float, 6> unknown_a1;
  parray<le_float, 6> unknown_a2;
} __attribute__((packed));

struct UnknownF8F2Entry {
  parray<le_float, 4> unknown_a1;
} __attribute__((packed));

struct QuestScriptOpcodeDefinition {
  struct Argument {
    enum class Type {
      LABEL16 = 0,
      LABEL16_SET,
      LABEL32,
      REG,
      REG_SET,
      REG_SET_FIXED, // Sequence of N consecutive regs
      REG32,
      REG32_SET_FIXED, // Sequence of N consecutive regs
      INT8,
      INT16,
      INT32,
      FLOAT32,
      CSTRING,
    };

    enum class DataType {
      NONE = 0,
      SCRIPT,
      DATA,
      PLAYER_STATS,
      PLAYER_VISUAL_CONFIG,
      RESIST_DATA,
      ATTACK_DATA,
      MOVEMENT_DATA,
      IMAGE_DATA,
      UNKNOWN_F8F2_DATA,
    };

    Type type;
    size_t count;
    DataType data_type;
    const char* name;

    Argument(Type type, size_t count = 0, const char* name = nullptr)
        : type(type),
          count(count),
          data_type(DataType::NONE),
          name(name) {}
    Argument(Type type, DataType data_type, const char* name = nullptr)
        : type(type),
          count(0),
          data_type(data_type),
          name(name) {}
  };

  enum class Version {
    V1 = 0,
    V2,
    V3,
    V4,
    NUM_VERSIONS,
  };

  enum Flag {
    PRESERVE_ARG_STACK = 0x01,
  };

  uint16_t opcode;
  const char* name;
  std::vector<Argument> imm_args;
  std::vector<Argument> stack_args;
  Version first_version;
  Version last_version;
  uint8_t flags;

  QuestScriptOpcodeDefinition(
      uint16_t opcode,
      const char* name,
      std::vector<Argument> imm_args,
      std::vector<Argument> stack_args,
      Version first_version,
      Version last_version,
      uint8_t flags = 0)
      : opcode(opcode),
        name(name),
        imm_args(imm_args),
        stack_args(stack_args),
        first_version(first_version),
        last_version(last_version),
        flags(flags) {}
};

using Arg = QuestScriptOpcodeDefinition::Argument;

static constexpr auto V1 = QuestScriptOpcodeDefinition::Version::V1;
static constexpr auto V2 = QuestScriptOpcodeDefinition::Version::V2;
static constexpr auto V3 = QuestScriptOpcodeDefinition::Version::V3;
static constexpr auto V4 = QuestScriptOpcodeDefinition::Version::V4;

static constexpr auto LABEL16 = Arg::Type::LABEL16;
static constexpr auto LABEL16_SET = Arg::Type::LABEL16_SET;
static constexpr auto LABEL32 = Arg::Type::LABEL32;
static constexpr auto REG = Arg::Type::REG;
static constexpr auto REG_SET = Arg::Type::REG_SET;
static constexpr auto REG_SET_FIXED = Arg::Type::REG_SET_FIXED;
static constexpr auto REG32 = Arg::Type::REG32;
static constexpr auto REG32_SET_FIXED = Arg::Type::REG32_SET_FIXED;
static constexpr auto INT8 = Arg::Type::INT8;
static constexpr auto INT16 = Arg::Type::INT16;
static constexpr auto INT32 = Arg::Type::INT32;
static constexpr auto FLOAT32 = Arg::Type::FLOAT32;
static constexpr auto CSTRING = Arg::Type::CSTRING;
static constexpr uint8_t PRESERVE_ARG_STACK = QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK;

static const Arg SCRIPT16(LABEL16, Arg::DataType::SCRIPT);
static const Arg SCRIPT16_SET(LABEL16_SET, Arg::DataType::SCRIPT);
static const Arg SCRIPT32(LABEL32, Arg::DataType::SCRIPT);
static const Arg DATA16(LABEL16, Arg::DataType::DATA);

static const Arg CLIENT_ID(INT32, 0, "client_id");
static const Arg ITEM_ID(INT32, 0, "item_id");
static const Arg AREA(INT32, 0, "area");

static const QuestScriptOpcodeDefinition opcode_defs[] = {
    {0x0000, "nop", {}, {}, V1, V4}, // Does nothing
    {0x0001, "ret", {}, {}, V1, V4}, // Pops new PC off stack
    {0x0002, "sync", {}, {}, V1, V4}, // Stops execution for the current frame
    {0x0003, "exit", {INT32}, {}, V1, V4}, // Exits entirely
    {0x0004, "thread", {SCRIPT16}, {}, V1, V4}, // Starts a new thread
    {0x0005, "va_start", {}, {}, V3, V4}, // Pushes r1-r7 to the stack
    {0x0006, "va_end", {}, {}, V3, V4}, // Pops r7-r1 from the stack
    {0x0007, "va_call", {SCRIPT16}, {}, V3, V4}, // Replaces r1-r7 with the args stack, then calls the function
    {0x0008, "let", {REG, REG}, {}, V1, V4}, // Copies a value from regB to regA
    {0x0009, "leti", {REG, INT32}, {}, V1, V4}, // Sets register to a fixed value (int32)
    {0x000A, "leta", {REG, REG}, {}, V1, V2}, // Sets regA to the memory address of regB
    {0x000A, "letb", {REG, INT8}, {}, V3, V4}, // Sets register to a fixed value (int8)
    {0x000B, "letw", {REG, INT16}, {}, V3, V4}, // Sets register to a fixed value (int16)
    {0x000C, "leta", {REG, REG}, {}, V3, V4}, // Sets regA to the memory address of regB
    {0x000D, "leto", {REG, SCRIPT16}, {}, V3, V4}, // Sets register to the offset (NOT memory address) of a function
    {0x0010, "set", {REG}, {}, V1, V4}, // Sets a register to 1
    {0x0011, "clear", {REG}, {}, V1, V4}, // Sets a register to 0
    {0x0012, "rev", {REG}, {}, V1, V4}, // Sets a register to 0 if it's nonzero and vice versa
    {0x0013, "gset", {INT16}, {}, V1, V4}, // Sets a global flag
    {0x0014, "gclear", {INT16}, {}, V1, V4}, // Clears a global flag
    {0x0015, "grev", {INT16}, {}, V1, V4}, // Flips a global flag
    {0x0016, "glet", {INT16, REG}, {}, V1, V4}, // Sets a global flag to a specific value
    {0x0017, "gget", {INT16, REG}, {}, V1, V4}, // Gets a global flag
    {0x0018, "add", {REG, REG}, {}, V1, V4}, // regA += regB
    {0x0019, "addi", {REG, INT32}, {}, V1, V4}, // regA += imm
    {0x001A, "sub", {REG, REG}, {}, V1, V4}, // regA -= regB
    {0x001B, "subi", {REG, INT32}, {}, V1, V4}, // regA -= imm
    {0x001C, "mul", {REG, REG}, {}, V1, V4}, // regA *= regB
    {0x001D, "muli", {REG, INT32}, {}, V1, V4}, // regA *= imm
    {0x001E, "div", {REG, REG}, {}, V1, V4}, // regA /= regB
    {0x001F, "divi", {REG, INT32}, {}, V1, V4}, // regA /= imm
    {0x0020, "and", {REG, REG}, {}, V1, V4}, // regA &= regB
    {0x0021, "andi", {REG, INT32}, {}, V1, V4}, // regA &= imm
    {0x0022, "or", {REG, REG}, {}, V1, V4}, // regA |= regB
    {0x0023, "ori", {REG, INT32}, {}, V1, V4}, // regA |= imm
    {0x0024, "xor", {REG, REG}, {}, V1, V4}, // regA ^= regB
    {0x0025, "xori", {REG, INT32}, {}, V1, V4}, // regA ^= imm
    {0x0026, "mod", {REG, REG}, {}, V3, V4}, // regA %= regB
    {0x0027, "modi", {REG, INT32}, {}, V3, V4}, // regA %= imm
    {0x0028, "jmp", {SCRIPT16}, {}, V1, V4}, // Jumps to function_table[fn_id]
    {0x0029, "call", {SCRIPT16}, {}, V1, V4}, // Pushes the offset after this opcode and jumps to function_table[fn_id]
    {0x002A, "jmp_on", {SCRIPT16, REG_SET}, {}, V1, V4}, // If all given registers are nonzero, jumps to function_table[fn_id]
    {0x002B, "jmp_off", {SCRIPT16, REG_SET}, {}, V1, V4}, // If all given registers are zero, jumps to function_table[fn_id]
    {0x002C, "jmp_eq", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA == regB, jumps to function_table[fn_id]
    {0x002D, "jmpi_eq", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA == regB, jumps to function_table[fn_id]
    {0x002E, "jmp_ne", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA != regB, jumps to function_table[fn_id]
    {0x002F, "jmpi_ne", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA != regB, jumps to function_table[fn_id]
    {0x0030, "ujmp_gt", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA > regB, jumps to function_table[fn_id]
    {0x0031, "ujmpi_gt", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA > regB, jumps to function_table[fn_id]
    {0x0032, "jmp_gt", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA > regB, jumps to function_table[fn_id]
    {0x0033, "jmpi_gt", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA > regB, jumps to function_table[fn_id]
    {0x0034, "ujmp_lt", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA < regB, jumps to function_table[fn_id]
    {0x0035, "ujmpi_lt", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA < regB, jumps to function_table[fn_id]
    {0x0036, "jmp_lt", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA < regB, jumps to function_table[fn_id]
    {0x0037, "jmpi_lt", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA < regB, jumps to function_table[fn_id]
    {0x0038, "ujmp_ge", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA >= regB, jumps to function_table[fn_id]
    {0x0039, "ujmpi_ge", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA >= regB, jumps to function_table[fn_id]
    {0x003A, "jmp_ge", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA >= regB, jumps to function_table[fn_id]
    {0x003B, "jmpi_ge", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA >= regB, jumps to function_table[fn_id]
    {0x003C, "ujmp_le", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA <= regB, jumps to function_table[fn_id]
    {0x003D, "ujmpi_le", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA <= regB, jumps to function_table[fn_id]
    {0x003E, "jmp_le", {REG, REG, SCRIPT16}, {}, V1, V4}, // If regA <= regB, jumps to function_table[fn_id]
    {0x003F, "jmpi_le", {REG, INT32, SCRIPT16}, {}, V1, V4}, // If regA <= regB, jumps to function_table[fn_id]
    {0x0040, "switch_jmp", {REG, SCRIPT16_SET}, {}, V1, V4}, // Jumps to function_table[fn_ids[regA]]
    {0x0041, "switch_call", {REG, SCRIPT16_SET}, {}, V1, V4}, // Calls function_table[fn_ids[regA]]
    {0x0042, "nop_42", {INT32}, {}, V1, V2}, // Does nothing
    {0x0042, "stack_push", {REG}, {}, V3, V4}, // Pushes regA
    {0x0043, "stack_pop", {REG}, {}, V3, V4}, // Pops regA
    {0x0044, "stack_pushm", {REG, INT32}, {}, V3, V4}, // Pushes N regs in increasing order starting at regA
    {0x0045, "stack_popm", {REG, INT32}, {}, V3, V4}, // Pops N regs in decreasing order ending at regA
    {0x0048, "arg_pushr", {REG}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes regA to the args list
    {0x0049, "arg_pushl", {INT32}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes imm to the args list
    {0x004A, "arg_pushb", {INT8}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes imm to the args list
    {0x004B, "arg_pushw", {INT16}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes imm to the args list
    {0x004C, "arg_pusha", {REG}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes memory address of regA to the args list
    {0x004D, "arg_pusho", {LABEL16}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes function_table[fn_id] to the args list
    {0x004E, "arg_pushs", {CSTRING}, {}, V3, V4, PRESERVE_ARG_STACK}, // Pushes memory address of str to the args list
    {0x0050, "message", {INT32, CSTRING}, {}, V1, V2}, // Creates a dialogue with object/NPC N starting with message str
    {0x0050, "message", {}, {INT32, CSTRING}, V3, V4}, // Creates a dialogue with object/NPC N starting with message str
    {0x0051, "list", {REG, CSTRING}, {}, V1, V2}, // Prompts the player with a list of choices, returning the index of their choice in regA
    {0x0051, "list", {}, {REG, CSTRING}, V3, V4}, // Prompts the player with a list of choices, returning the index of their choice in regA
    {0x0052, "fadein", {}, {}, V1, V4}, // Fades from black
    {0x0053, "fadeout", {}, {}, V1, V4}, // Fades to black
    {0x0054, "se", {INT32}, {}, V1, V2}, // Plays a sound effect
    {0x0054, "se", {}, {INT32}, V3, V4}, // Plays a sound effect
    {0x0055, "bgm", {INT32}, {}, V1, V2}, // Plays a fanfare (clear.adx or miniclear.adx)
    {0x0055, "bgm", {}, {INT32}, V3, V4}, // Plays a fanfare (clear.adx or miniclear.adx)
    {0x0056, "nop_56", {}, {}, V1, V2}, // Does nothing
    {0x0057, "nop_57", {}, {}, V1, V2}, // Does nothing
    {0x0058, "nop_58", {INT32}, {}, V1, V2}, // Does nothing
    {0x0059, "nop_59", {INT32}, {}, V1, V2}, // Does nothing
    {0x005A, "window_msg", {CSTRING}, {}, V1, V2}, // Displays a message
    {0x005A, "window_msg", {}, {CSTRING}, V3, V4}, // Displays a message
    {0x005B, "add_msg", {CSTRING}, {}, V1, V2}, // Adds a message to an existing window
    {0x005B, "add_msg", {}, {CSTRING}, V3, V4}, // Adds a message to an existing window
    {0x005C, "mesend", {}, {}, V1, V4}, // Closes a message box
    {0x005D, "gettime", {REG}, {}, V1, V4}, // Gets the current time
    {0x005E, "winend", {}, {}, V1, V4}, // Closes a window_msg
    {0x0060, "npc_crt", {INT32, INT32}, {}, V1, V2}, // Creates an NPC
    {0x0060, "npc_crt", {}, {INT32, INT32}, V3, V4}, // Creates an NPC
    {0x0061, "npc_stop", {INT32}, {}, V1, V2}, // Tells an NPC to stop following
    {0x0061, "npc_stop", {}, {INT32}, V3, V4}, // Tells an NPC to stop following
    {0x0062, "npc_play", {INT32}, {}, V1, V2}, // Tells an NPC to follow the player
    {0x0062, "npc_play", {}, {INT32}, V3, V4}, // Tells an NPC to follow the player
    {0x0063, "npc_kill", {INT32}, {}, V1, V2}, // Destroys an NPC
    {0x0063, "npc_kill", {}, {INT32}, V3, V4}, // Destroys an NPC
    {0x0064, "npc_nont", {}, {}, V1, V4},
    {0x0065, "npc_talk", {}, {}, V1, V4},
    {0x0066, "npc_crp", {{REG_SET_FIXED, 6}, INT32}, {}, V1, V2}, // Creates an NPC. Second argument is ignored
    {0x0066, "npc_crp", {{REG_SET_FIXED, 6}}, {}, V3, V4}, // Creates an NPC
    {0x0068, "create_pipe", {INT32}, {}, V1, V2}, // Creates a pipe
    {0x0068, "create_pipe", {}, {INT32}, V3, V4}, // Creates a pipe
    {0x0069, "p_hpstat", {REG, CLIENT_ID}, {}, V1, V2}, // Compares player HP with a given value
    {0x0069, "p_hpstat", {}, {REG, CLIENT_ID}, V3, V4}, // Compares player HP with a given value
    {0x006A, "p_dead", {REG, CLIENT_ID}, {}, V1, V2}, // Checks if player is dead
    {0x006A, "p_dead", {}, {REG, CLIENT_ID}, V3, V4}, // Checks if player is dead
    {0x006B, "p_disablewarp", {}, {}, V1, V4}, // Disables telepipes/Ryuker
    {0x006C, "p_enablewarp", {}, {}, V1, V4}, // Enables telepipes/Ryuker
    {0x006D, "p_move", {{REG_SET_FIXED, 5}, INT32}, {}, V1, V2}, // Moves player. Second argument is ignored
    {0x006D, "p_move", {{REG_SET_FIXED, 5}}, {}, V3, V4}, // Moves player
    {0x006E, "p_look", {CLIENT_ID}, {}, V1, V2},
    {0x006E, "p_look", {}, {CLIENT_ID}, V3, V4},
    {0x0070, "p_action_disable", {}, {}, V1, V4}, // Disables attacks for all players
    {0x0071, "p_action_enable", {}, {}, V1, V4}, // Enables attacks for all players
    {0x0072, "disable_movement1", {CLIENT_ID}, {}, V1, V2}, // Disables movement for the given player
    {0x0072, "disable_movement1", {}, {CLIENT_ID}, V3, V4}, // Disables movement for the given player
    {0x0073, "enable_movement1", {CLIENT_ID}, {}, V1, V2}, // Enables movement for the given player
    {0x0073, "enable_movement1", {}, {CLIENT_ID}, V3, V4}, // Enables movement for the given player
    {0x0074, "p_noncol", {}, {}, V1, V4},
    {0x0075, "p_col", {}, {}, V1, V4},
    {0x0076, "p_setpos", {CLIENT_ID, {REG_SET_FIXED, 4}}, {}, V1, V2},
    {0x0076, "p_setpos", {}, {CLIENT_ID, {REG_SET_FIXED, 4}}, V3, V4},
    {0x0077, "p_return_guild", {}, {}, V1, V4},
    {0x0078, "p_talk_guild", {CLIENT_ID}, {}, V1, V2},
    {0x0078, "p_talk_guild", {}, {CLIENT_ID}, V3, V4},
    {0x0079, "npc_talk_pl", {{REG32_SET_FIXED, 8}}, {}, V1, V2},
    {0x0079, "npc_talk_pl", {{REG_SET_FIXED, 8}}, {}, V3, V4},
    {0x007A, "npc_talk_kill", {INT32}, {}, V1, V2},
    {0x007A, "npc_talk_kill", {}, {INT32}, V3, V4},
    {0x007B, "npc_crtpk", {INT32, INT32}, {}, V1, V2}, // Creates attacker NPC
    {0x007B, "npc_crtpk", {}, {INT32, INT32}, V3, V4}, // Creates attacker NPC
    {0x007C, "npc_crppk", {{REG32_SET_FIXED, 7}, INT32}, {}, V1, V2}, // Creates attacker NPC
    {0x007C, "npc_crppk", {{REG_SET_FIXED, 7}}, {}, V3, V4}, // Creates attacker NPC
    {0x007D, "npc_crptalk", {{REG32_SET_FIXED, 6}, INT32}, {}, V1, V2},
    {0x007D, "npc_crptalk", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x007E, "p_look_at", {CLIENT_ID, CLIENT_ID}, {}, V1, V2},
    {0x007E, "p_look_at", {}, {CLIENT_ID, CLIENT_ID}, V3, V4},
    {0x007F, "npc_crp_id", {{REG32_SET_FIXED, 7}, INT32}, {}, V1, V2},
    {0x007F, "npc_crp_id", {{REG_SET_FIXED, 7}}, {}, V3, V4},
    {0x0080, "cam_quake", {}, {}, V1, V4},
    {0x0081, "cam_adj", {}, {}, V1, V4},
    {0x0082, "cam_zmin", {}, {}, V1, V4},
    {0x0083, "cam_zmout", {}, {}, V1, V4},
    {0x0084, "cam_pan", {{REG32_SET_FIXED, 5}, INT32}, {}, V1, V2},
    {0x0084, "cam_pan", {{REG_SET_FIXED, 5}}, {}, V3, V4},
    {0x0085, "game_lev_super", {}, {}, V1, V2},
    {0x0085, "nop_85", {}, {}, V3, V4},
    {0x0086, "game_lev_reset", {}, {}, V1, V2},
    {0x0086, "nop_86", {}, {}, V3, V4},
    {0x0087, "pos_pipe", {{REG32_SET_FIXED, 4}, INT32}, {}, V1, V2},
    {0x0087, "pos_pipe", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0x0088, "if_zone_clear", {REG, {REG_SET_FIXED, 2}}, {}, V1, V4},
    {0x0089, "chk_ene_num", {REG}, {}, V1, V4},
    {0x008A, "unhide_obj", {{REG_SET_FIXED, 3}}, {}, V1, V4},
    {0x008B, "unhide_ene", {{REG_SET_FIXED, 3}}, {}, V1, V4},
    {0x008C, "at_coords_call", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008D, "at_coords_talk", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008E, "col_npcin", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008F, "col_npcinr", {{REG_SET_FIXED, 6}}, {}, V1, V4},
    {0x0090, "switch_on", {INT32}, {}, V1, V2},
    {0x0090, "switch_on", {}, {INT32}, V3, V4},
    {0x0091, "switch_off", {INT32}, {}, V1, V2},
    {0x0091, "switch_off", {}, {INT32}, V3, V4},
    {0x0092, "playbgm_epi", {INT32}, {}, V1, V2},
    {0x0092, "playbgm_epi", {}, {INT32}, V3, V4},
    {0x0093, "set_mainwarp", {INT32}, {}, V1, V2},
    {0x0093, "set_mainwarp", {}, {INT32}, V3, V4},
    {0x0094, "set_obj_param", {{REG_SET_FIXED, 6}, REG}, {}, V1, V4},
    {0x0095, "set_floor_handler", {AREA, SCRIPT32}, {}, V1, V2},
    {0x0095, "set_floor_handler", {}, {AREA, SCRIPT16}, V3, V4},
    {0x0096, "clr_floor_handler", {AREA}, {}, V1, V2},
    {0x0096, "clr_floor_handler", {}, {AREA}, V3, V4},
    {0x0097, "col_plinaw", {{REG_SET_FIXED, 9}}, {}, V1, V4},
    {0x0098, "hud_hide", {}, {}, V1, V4},
    {0x0099, "hud_show", {}, {}, V1, V4},
    {0x009A, "cine_enable", {}, {}, V1, V4},
    {0x009B, "cine_disable", {}, {}, V1, V4},
    {0x00A0, "nop_A0_debug", {INT32, CSTRING}, {}, V1, V2}, // argA appears unused; game will softlock unless argB contains exactly 2 messages
    {0x00A0, "nop_A0_debug", {}, {INT32, CSTRING}, V3, V4}, // argA appears unused; game will softlock unless argB contains exactly 2 messages
    {0x00A1, "set_qt_failure", {SCRIPT32}, {}, V1, V2},
    {0x00A1, "set_qt_failure", {SCRIPT16}, {}, V3, V4},
    {0x00A2, "set_qt_success", {SCRIPT32}, {}, V1, V2},
    {0x00A2, "set_qt_success", {SCRIPT16}, {}, V3, V4},
    {0x00A3, "clr_qt_failure", {}, {}, V1, V4},
    {0x00A4, "clr_qt_success", {}, {}, V1, V4},
    {0x00A5, "set_qt_cancel", {SCRIPT32}, {}, V1, V2},
    {0x00A5, "set_qt_cancel", {SCRIPT16}, {}, V3, V4},
    {0x00A6, "clr_qt_cancel", {}, {}, V1, V4},
    {0x00A8, "pl_walk", {{REG32_SET_FIXED, 4}, INT32}, {}, V1, V2},
    {0x00A8, "pl_walk", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0x00B0, "pl_add_meseta", {CLIENT_ID, INT32}, {}, V1, V2},
    {0x00B0, "pl_add_meseta", {}, {CLIENT_ID, INT32}, V3, V4},
    {0x00B1, "thread_stg", {SCRIPT16}, {}, V1, V4},
    {0x00B2, "del_obj_param", {REG}, {}, V1, V4},
    {0x00B3, "item_create", {{REG_SET_FIXED, 3}, REG}, {}, V1, V4}, // Creates an item; regsA holds item data1, regB receives item ID
    {0x00B4, "item_create2", {{REG_SET_FIXED, 12}, REG}, {}, V1, V4}, // Like item_create but input regs each specify 1 byte
    {0x00B5, "item_delete", {REG, {REG_SET_FIXED, 12}}, {}, V1, V4},
    {0x00B6, "item_delete2", {{REG_SET_FIXED, 3}, {REG_SET_FIXED, 12}}, {}, V1, V4},
    {0x00B7, "item_check", {{REG_SET_FIXED, 3}, REG}, {}, V1, V4},
    {0x00B8, "setevt", {INT32}, {}, V1, V2},
    {0x00B8, "setevt", {}, {INT32}, V3, V4},
    {0x00B9, "get_difficulty_level_v1", {REG}, {}, V1, V4}, // Only returns 0-2, even in Ultimate
    {0x00BA, "set_qt_exit", {SCRIPT32}, {}, V1, V2},
    {0x00BA, "set_qt_exit", {SCRIPT16}, {}, V3, V4},
    {0x00BB, "clr_qt_exit", {}, {}, V1, V4},
    {0x00BC, "nop_BC", {CSTRING}, {}, V1, V4},
    {0x00C0, "particle", {{REG32_SET_FIXED, 5}, INT32}, {}, V1, V2},
    {0x00C0, "particle", {{REG_SET_FIXED, 5}}, {}, V3, V4},
    {0x00C1, "npc_text", {INT32, CSTRING}, {}, V1, V2},
    {0x00C1, "npc_text", {}, {INT32, CSTRING}, V3, V4},
    {0x00C2, "npc_chkwarp", {}, {}, V1, V4},
    {0x00C3, "pl_pkoff", {}, {}, V1, V4},
    {0x00C4, "map_designate", {{REG_SET_FIXED, 4}}, {}, V1, V4},
    {0x00C5, "masterkey_on", {}, {}, V1, V4},
    {0x00C6, "masterkey_off", {}, {}, V1, V4},
    {0x00C7, "window_time", {}, {}, V1, V4},
    {0x00C8, "winend_time", {}, {}, V1, V4},
    {0x00C9, "winset_time", {REG}, {}, V1, V4},
    {0x00CA, "getmtime", {REG}, {}, V1, V4},
    {0x00CB, "set_quest_board_handler", {INT32, SCRIPT32, CSTRING}, {}, V1, V2},
    {0x00CB, "set_quest_board_handler", {}, {INT32, SCRIPT16, CSTRING}, V3, V4},
    {0x00CC, "clear_quest_board_handler", {INT32}, {}, V1, V2},
    {0x00CC, "clear_quest_board_handler", {}, {INT32}, V3, V4},
    {0x00CD, "particle_id", {{REG32_SET_FIXED, 4}, INT32}, {}, V1, V2},
    {0x00CD, "particle_id", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0x00CE, "npc_crptalk_id", {{REG32_SET_FIXED, 7}, INT32}, {}, V1, V2},
    {0x00CE, "npc_crptalk_id", {{REG_SET_FIXED, 7}}, {}, V3, V4},
    {0x00CF, "npc_lang_clean", {}, {}, V1, V4},
    {0x00D0, "pl_pkon", {}, {}, V1, V4},
    {0x00D1, "pl_chk_item2", {{REG_SET_FIXED, 4}, REG}, {}, V1, V4}, // Presumably like item_check but also checks data2
    {0x00D2, "enable_mainmenu", {}, {}, V1, V4},
    {0x00D3, "disable_mainmenu", {}, {}, V1, V4},
    {0x00D4, "start_battlebgm", {}, {}, V1, V4},
    {0x00D5, "end_battlebgm", {}, {}, V1, V4},
    {0x00D6, "disp_msg_qb", {CSTRING}, {}, V1, V2},
    {0x00D6, "disp_msg_qb", {}, {CSTRING}, V3, V4},
    {0x00D7, "close_msg_qb", {}, {}, V1, V4},
    {0x00D8, "set_eventflag", {INT32, INT32}, {}, V1, V2},
    {0x00D8, "set_eventflag", {}, {INT32, INT32}, V3, V4},
    {0x00D9, "sync_register", {INT32, INT32}, {}, V1, V2},
    {0x00D9, "sync_register", {}, {INT32, INT32}, V3, V4},
    {0x00DA, "set_returnhunter", {}, {}, V1, V4},
    {0x00DB, "set_returncity", {}, {}, V1, V4},
    {0x00DC, "load_pvr", {}, {}, V1, V4},
    {0x00DD, "load_midi", {}, {}, V1, V4}, // Seems incomplete on V3 - has some similar codepaths as load_pvr, but the main function seems to do nothing
    {0x00DE, "item_detect_bank", {{REG_SET_FIXED, 6}, REG}, {}, V1, V4}, // regsA specifies the first 6 bytes of an ItemData
    {0x00DF, "npc_param", {{REG32_SET_FIXED, 14}, INT32}, {}, V1, V2},
    {0x00DF, "npc_param", {{REG_SET_FIXED, 14}, INT32}, {}, V3, V4},
    {0x00E0, "pad_dragon", {}, {}, V1, V4},
    {0x00E1, "clear_mainwarp", {INT32}, {}, V1, V2},
    {0x00E1, "clear_mainwarp", {}, {INT32}, V3, V4},
    {0x00E2, "pcam_param", {{REG32_SET_FIXED, 6}}, {}, V1, V2},
    {0x00E2, "pcam_param", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x00E3, "start_setevt", {INT32, INT32}, {}, V1, V2},
    {0x00E3, "start_setevt", {}, {INT32, INT32}, V3, V4},
    {0x00E4, "warp_on", {}, {}, V1, V4},
    {0x00E5, "warp_off", {}, {}, V1, V4},
    {0x00E6, "get_client_id", {REG}, {}, V1, V4},
    {0x00E7, "get_leader_id", {REG}, {}, V1, V4},
    {0x00E8, "set_eventflag2", {INT32, REG}, {}, V1, V2},
    {0x00E8, "set_eventflag2", {}, {INT32, REG}, V3, V4},
    {0x00E9, "mod2", {REG, REG}, {}, V1, V4},
    {0x00EA, "modi2", {REG, INT32}, {}, V1, V4},
    {0x00EB, "enable_bgmctrl", {INT32}, {}, V1, V2},
    {0x00EB, "enable_bgmctrl", {}, {INT32}, V3, V4},
    {0x00EC, "sw_send", {{REG_SET_FIXED, 3}}, {}, V1, V4},
    {0x00ED, "create_bgmctrl", {}, {}, V1, V4},
    {0x00EE, "pl_add_meseta2", {INT32}, {}, V1, V2},
    {0x00EE, "pl_add_meseta2", {}, {INT32}, V3, V4},
    {0x00EF, "sync_register2", {INT32, REG32}, {}, V1, V2},
    {0x00EF, "sync_register2", {}, {INT32, INT32}, V3, V4},
    {0x00F0, "send_regwork", {INT32, REG32}, {}, V1, V2},
    {0x00F1, "leti_fixed_camera", {{REG32_SET_FIXED, 6}}, {}, V2, V2},
    {0x00F1, "leti_fixed_camera", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x00F2, "default_camera_pos1", {}, {}, V2, V4},
    {0xF800, "debug_F800", {}, {}, V2, V2}, // Same as 50, but uses fixed arguments - with a Japanese string that Google Translate translates as "I'm frugal!!"
    {0xF801, "set_chat_callback", {{REG32_SET_FIXED, 5}, CSTRING}, {}, V2, V2},
    {0xF801, "set_chat_callback", {}, {{REG_SET_FIXED, 5}, CSTRING}, V3, V4},
    {0xF808, "get_difficulty_level2", {REG}, {}, V2, V4},
    {0xF809, "get_number_of_players", {REG}, {}, V2, V4},
    {0xF80A, "get_coord_of_player", {{REG_SET_FIXED, 3}, REG}, {}, V2, V4},
    {0xF80B, "enable_map", {}, {}, V2, V4},
    {0xF80C, "disable_map", {}, {}, V2, V4},
    {0xF80D, "map_designate_ex", {{REG_SET_FIXED, 5}}, {}, V2, V4},
    {0xF80E, "disable_weapon_drop", {CLIENT_ID}, {}, V2, V2},
    {0xF80E, "disable_weapon_drop", {}, {CLIENT_ID}, V3, V4},
    {0xF80F, "enable_weapon_drop", {CLIENT_ID}, {}, V2, V2},
    {0xF80F, "enable_weapon_drop", {}, {CLIENT_ID}, V3, V4},
    {0xF810, "ba_initial_floor", {AREA}, {}, V2, V2},
    {0xF810, "ba_initial_floor", {}, {AREA}, V3, V4},
    {0xF811, "set_ba_rules", {}, {}, V2, V4},
    {0xF812, "ba_set_tech", {INT32}, {}, V2, V2},
    {0xF812, "ba_set_tech", {}, {INT32}, V3, V4},
    {0xF813, "ba_set_equip", {INT32}, {}, V2, V2},
    {0xF813, "ba_set_equip", {}, {INT32}, V3, V4},
    {0xF814, "ba_set_mag", {INT32}, {}, V2, V2},
    {0xF814, "ba_set_mag", {}, {INT32}, V3, V4},
    {0xF815, "ba_set_item", {INT32}, {}, V2, V2},
    {0xF815, "ba_set_item", {}, {INT32}, V3, V4},
    {0xF816, "ba_set_trapmenu", {INT32}, {}, V2, V2},
    {0xF816, "ba_set_trapmenu", {}, {INT32}, V3, V4},
    {0xF817, "ba_set_unused_F817", {INT32}, {}, V2, V2}, // This appears to be unused - it's copied into the main battle rules struct, but then the field appears never to be read
    {0xF817, "ba_set_unused_F817", {}, {INT32}, V3, V4},
    {0xF818, "ba_set_respawn", {INT32}, {}, V2, V2},
    {0xF818, "ba_set_respawn", {}, {INT32}, V3, V4},
    {0xF819, "ba_set_char", {INT32}, {}, V2, V2},
    {0xF819, "ba_set_char", {}, {INT32}, V3, V4},
    {0xF81A, "ba_dropwep", {INT32}, {}, V2, V2},
    {0xF81A, "ba_dropwep", {}, {INT32}, V3, V4},
    {0xF81B, "ba_teams", {INT32}, {}, V2, V2},
    {0xF81B, "ba_teams", {}, {INT32}, V3, V4},
    {0xF81C, "ba_disp_msg", {CSTRING}, {}, V2, V2},
    {0xF81C, "ba_disp_msg", {}, {CSTRING}, V3, V4},
    {0xF81D, "death_lvl_up", {INT32}, {}, V2, V2},
    {0xF81D, "death_lvl_up", {}, {INT32}, V3, V4},
    {0xF81E, "ba_set_meseta", {INT32}, {}, V2, V2},
    {0xF81E, "ba_set_meseta", {}, {INT32}, V3, V4},
    {0xF820, "cmode_stage", {INT32}, {}, V2, V2},
    {0xF820, "cmode_stage", {}, {INT32}, V3, V4},
    {0xF821, "nop_F821", {{REG_SET_FIXED, 9}}, {}, V2, V4}, // regsA[3-8] specify first 6 bytes of an ItemData. This opcode consumes an item ID, but does nothing else.
    {0xF822, "nop_F822", {REG}, {}, V2, V4},
    {0xF823, "set_cmode_char_template", {INT32}, {}, V2, V2},
    {0xF823, "set_cmode_char_template", {}, {INT32}, V3, V4},
    {0xF824, "set_cmode_diff", {INT32}, {}, V2, V2},
    {0xF824, "set_cmode_diff", {}, {INT32}, V3, V4},
    {0xF825, "exp_multiplication", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF826, nullptr, {REG}, {}, V2, V4}, // TODO (DX) - Challenge-related
    {0xF827, "get_user_is_dead", {REG}, {}, V2, V4},
    {0xF828, "go_floor", {REG, REG}, {}, V2, V4},
    {0xF829, "get_num_kills", {REG, REG}, {}, V2, V4},
    {0xF82A, "reset_kills", {REG}, {}, V2, V4},
    {0xF82B, "unlock_door2", {INT32, INT32}, {}, V2, V2},
    {0xF82B, "unlock_door2", {}, {INT32, INT32}, V3, V4},
    {0xF82C, "lock_door2", {INT32, INT32}, {}, V2, V2},
    {0xF82C, "lock_door2", {}, {INT32, INT32}, V3, V4},
    {0xF82D, "if_switch_not_pressed", {{REG_SET_FIXED, 2}}, {}, V2, V4},
    {0xF82E, "if_switch_pressed", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF830, "control_dragon", {REG}, {}, V2, V4},
    {0xF831, "release_dragon", {}, {}, V2, V4},
    {0xF838, "shrink", {REG}, {}, V2, V4},
    {0xF839, "unshrink", {REG}, {}, V2, V4},
    {0xF83A, "set_shrink_cam1", {{REG_SET_FIXED, 4}}, {}, V2, V4},
    {0xF83B, "set_shrink_cam2", {{REG_SET_FIXED, 4}}, {}, V2, V4},
    {0xF83C, "display_clock2", {REG}, {}, V2, V4},
    {0xF83D, "set_area_total", {INT32}, {}, V2, V2},
    {0xF83D, "set_area_total", {}, {INT32}, V3, V4},
    {0xF83E, "delete_area_title", {INT32}, {}, V2, V2},
    {0xF83E, "delete_area_title", {}, {INT32}, V3, V4},
    {0xF840, "load_npc_data", {}, {}, V2, V4},
    {0xF841, "get_npc_data", {{LABEL16, Arg::DataType::PLAYER_VISUAL_CONFIG, "visual_config"}}, {}, V2, V4},
    {0xF848, "give_damage_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF849, "take_damage_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84A, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4}, // TODO (DX) - computation is regsA[0] + (regsA[1] / regsA[2])
    {0xF84B, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4}, // TODO (DX) - computation is regsA[0] + (regsA[1] / regsA[2])
    {0xF84C, "kill_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84D, "death_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84E, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4}, // TODO (DX) - computation is regsA[0] + (regsA[1] / regsA[2])
    {0xF84F, "enemy_death_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF850, "meseta_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF851, "ba_set_trap_count", {{REG_SET_FIXED, 2}}, {}, V2, V4}, // regsA is [trap_type, trap_count]
    {0xF852, nullptr, {INT32}, {}, V2, V2}, // TODO (DX) - battle rules. Value should be 0 or 1
    {0xF852, nullptr, {}, {INT32}, V3, V4}, // TODO (DX)
    {0xF853, "reverse_warps", {}, {}, V2, V4},
    {0xF854, "unreverse_warps", {}, {}, V2, V4},
    {0xF855, "set_ult_map", {}, {}, V2, V4},
    {0xF856, "unset_ult_map", {}, {}, V2, V4},
    {0xF857, "set_area_title", {CSTRING}, {}, V2, V2},
    {0xF857, "set_area_title", {}, {CSTRING}, V3, V4},
    {0xF858, "ba_show_self_traps", {}, {}, V2, V4},
    {0xF859, "ba_hide_self_traps", {}, {}, V2, V4},
    {0xF85A, "equip_item", {{REG32_SET_FIXED, 4}}, {}, V2, V2}, // regsA are {client_id, item.data1[0-2]}
    {0xF85A, "equip_item", {{REG_SET_FIXED, 4}}, {}, V3, V4}, // regsA are {client_id, item.data1[0-2]}
    {0xF85B, "unequip_item", {CLIENT_ID, INT32}, {}, V2, V2},
    {0xF85B, "unequip_item", {}, {CLIENT_ID, INT32}, V3, V4},
    {0xF85C, "qexit2", {INT32}, {}, V2, V4},
    {0xF85D, "set_allow_item_flags", {INT32}, {}, V2, V2}, // Same as on v3
    {0xF85D, "set_allow_item_flags", {}, {INT32}, V3, V4}, // 0 = allow normal item usage (undoes all of the following), 1 = disallow weapons, 2 = disallow armors, 3 = disallow shields, 4 = disallow units, 5 = disallow mags, 6 = disallow tools
    {0xF85E, nullptr, {INT32}, {}, V2, V2}, // TODO (DX) - battle rules. Value should be 0 or 1
    {0xF85E, nullptr, {}, {INT32}, V3, V4}, // TODO (DX)
    {0xF85F, nullptr, {INT32}, {}, V2, V2}, // TODO (DX) - battle rules. Does nothing unless F85E was executed with value 1. Value should be in the range [0, 99]
    {0xF85F, nullptr, {}, {INT32}, V3, V4}, // TODO (DX)
    {0xF860, "clear_score_announce", {}, {}, V2, V4},
    {0xF861, "set_score_announce", {INT32}, {}, V2, V2},
    {0xF861, "set_score_announce", {}, {INT32}, V3, V4},
    {0xF862, "give_s_rank_weapon", {REG32, REG32, CSTRING}, {}, V2, V2},
    {0xF862, "give_s_rank_weapon", {}, {INT32, REG, CSTRING}, V3, V4},
    {0xF863, "get_mag_levels", {{REG32_SET_FIXED, 4}}, {}, V2, V2},
    {0xF863, "get_mag_levels", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF864, "cmode_rank", {INT32, CSTRING}, {}, V2, V2},
    {0xF864, "cmode_rank", {}, {INT32, CSTRING}, V3, V4},
    {0xF865, "award_item_name", {}, {}, V2, V4},
    {0xF866, "award_item_select", {}, {}, V2, V4},
    {0xF867, "award_item_give_to", {REG}, {}, V2, V4},
    {0xF868, "set_cmode_rank", {REG, REG}, {}, V2, V4},
    {0xF869, "check_rank_time", {REG, REG}, {}, V2, V4},
    {0xF86A, "item_create_cmode", {{REG_SET_FIXED, 6}, REG}, {}, V2, V4}, // regsA specifies item.data1[0-5]
    {0xF86B, "ba_box_drops", {REG}, {}, V2, V4}, // TODO: This sets override_area in TItemDropSub; use this in ItemCreator
    {0xF86C, "award_item_ok", {REG}, {}, V2, V4},
    {0xF86D, "ba_set_trapself", {}, {}, V2, V4},
    {0xF86E, "ba_clear_trapself", {}, {}, V2, V4},
    {0xF86F, "ba_set_lives", {INT32}, {}, V2, V2},
    {0xF86F, "ba_set_lives", {}, {INT32}, V3, V4},
    {0xF870, "ba_set_tech_lvl", {INT32}, {}, V2, V2},
    {0xF870, "ba_set_tech_lvl", {}, {INT32}, V3, V4},
    {0xF871, "ba_set_lvl", {INT32}, {}, V2, V2},
    {0xF871, "ba_set_lvl", {}, {INT32}, V3, V4},
    {0xF872, "ba_set_time_limit", {INT32}, {}, V2, V2},
    {0xF872, "ba_set_time_limit", {}, {INT32}, V3, V4},
    {0xF873, "dark_falz_is_dead", {REG}, {}, V2, V4},
    {0xF874, nullptr, {INT32, CSTRING}, {}, V2, V2}, // TODO (DX) - Similar to A0, but does something with the two strings in non-4P challenge mode
    {0xF874, nullptr, {}, {INT32, CSTRING}, V3, V4}, // TODO (DX)
    {0xF875, "enable_stealth_suit_effect", {REG}, {}, V2, V4},
    {0xF876, "disable_stealth_suit_effect", {REG}, {}, V2, V4},
    {0xF877, "enable_techs", {REG}, {}, V2, V4},
    {0xF878, "disable_techs", {REG}, {}, V2, V4},
    {0xF879, "get_gender", {REG, REG}, {}, V2, V4},
    {0xF87A, "get_chara_class", {REG, {REG_SET_FIXED, 2}}, {}, V2, V4},
    {0xF87B, "take_slot_meseta", {{REG_SET_FIXED, 2}, REG}, {}, V2, V4},
    {0xF87C, "get_guild_card_file_creation_time", {REG}, {}, V2, V4},
    {0xF87D, "kill_player", {REG}, {}, V2, V4},
    {0xF87E, "get_serial_number", {REG}, {}, V2, V4}, // Returns 0 on BB
    {0xF87F, "get_eventflag", {REG, REG}, {}, V2, V4},
    {0xF880, "set_trap_damage", {{REG_SET_FIXED, 3}}, {}, V2, V4}, // Normally trap damage is (700.0 * area_factor[area] * 2.0 * (0.01 * level + 0.1)); this overrides that computation. The value is specified with integer and fractional parts split up: the actual value is regsA[0] + (regsA[1] / regsA[2]).
    {0xF881, "get_pl_name", {REG}, {}, V2, V4},
    {0xF882, "get_pl_job", {REG}, {}, V2, V4},
    {0xF883, "get_player_proximity", {{REG_SET_FIXED, 2}, REG}, {}, V2, V4},
    {0xF884, "set_eventflag16", {INT32, REG}, {}, V2, V2},
    {0xF884, "set_eventflag16", {}, {INT32, INT32}, V3, V4},
    {0xF885, "set_eventflag32", {INT32, REG}, {}, V2, V2},
    {0xF885, "set_eventflag32", {}, {INT32, INT32}, V3, V4},
    {0xF886, "ba_get_place", {REG, REG}, {}, V2, V4},
    {0xF887, "ba_get_score", {REG, REG}, {}, V2, V4},
    {0xF888, "enable_win_pfx", {}, {}, V2, V4},
    {0xF889, "disable_win_pfx", {}, {}, V2, V4},
    {0xF88A, "get_player_status", {REG, REG}, {}, V2, V4},
    {0xF88B, "send_mail", {REG, CSTRING}, {}, V2, V2},
    {0xF88B, "send_mail", {}, {REG, CSTRING}, V3, V4},
    {0xF88C, "get_game_version", {REG}, {}, V2, V4}, // Returns 2 on DCv2/PC, 3 on GC, 4 on BB
    {0xF88D, "chl_set_timerecord", {REG}, {}, V2, V3},
    {0xF88D, "chl_set_timerecord", {REG, REG}, {}, V4, V4},
    {0xF88E, "chl_get_timerecord", {REG}, {}, V2, V4},
    {0xF88F, "set_cmode_grave_rates", {{REG_SET_FIXED, 20}}, {}, V2, V4},
    {0xF890, "clear_mainwarp_all", {}, {}, V2, V4},
    {0xF891, "load_enemy_data", {INT32}, {}, V2, V2},
    {0xF891, "load_enemy_data", {}, {INT32}, V3, V4},
    {0xF892, "get_physical_data", {{LABEL16, Arg::DataType::PLAYER_STATS, "stats"}}, {}, V2, V4},
    {0xF893, "get_attack_data", {{LABEL16, Arg::DataType::ATTACK_DATA, "attack_data"}}, {}, V2, V4},
    {0xF894, "get_resist_data", {{LABEL16, Arg::DataType::RESIST_DATA, "resist_data"}}, {}, V2, V4},
    {0xF895, "get_movement_data", {{LABEL16, Arg::DataType::MOVEMENT_DATA, "movement_data"}}, {}, V2, V4},
    {0xF896, "get_eventflag16", {REG, REG}, {}, V2, V4},
    {0xF897, "get_eventflag32", {REG, REG}, {}, V2, V4},
    {0xF898, "shift_left", {REG, REG}, {}, V2, V4},
    {0xF899, "shift_right", {REG, REG}, {}, V2, V4},
    {0xF89A, "get_random", {{REG_SET_FIXED, 2}, REG}, {}, V2, V4},
    {0xF89B, "reset_map", {}, {}, V2, V4},
    {0xF89C, "disp_chl_retry_menu", {REG}, {}, V2, V4},
    {0xF89D, "chl_reverser", {}, {}, V2, V4},
    {0xF89E, "ba_forbid_scape_dolls", {INT32}, {}, V2, V2},
    {0xF89E, "ba_forbid_scape_dolls", {}, {INT32}, V3, V4},
    {0xF89F, "player_recovery", {REG}, {}, V2, V4}, // regA = client ID
    {0xF8A0, "disable_bosswarp_option", {}, {}, V2, V4},
    {0xF8A1, "enable_bosswarp_option", {}, {}, V2, V4},
    {0xF8A2, "is_bosswarp_opt_disabled", {REG}, {}, V2, V4},
    {0xF8A3, "load_serial_number_to_flag_buf", {}, {}, V2, V4}, // Loads 0 on BB
    {0xF8A4, "write_flag_buf_to_event_flags", {REG}, {}, V2, V4},
    {0xF8A5, "set_chat_callback_no_filter", {{REG_SET_FIXED, 5}}, {}, V2, V4},
    {0xF8A6, "set_symbol_chat_collision", {{REG_SET_FIXED, 10}}, {}, V2, V4},
    {0xF8A7, "set_shrink_size", {REG, {REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF8A8, "death_tech_lvl_up2", {INT32}, {}, V2, V2},
    {0xF8A8, "death_tech_lvl_up2", {}, {INT32}, V3, V4},
    {0xF8A9, "vol_opt_is_dead", {REG}, {}, V2, V4},
    {0xF8AA, "is_there_grave_message", {REG}, {}, V2, V4},
    {0xF8AB, "get_ba_record", {{REG_SET_FIXED, 7}}, {}, V2, V4},
    {0xF8AC, "get_cmode_prize_rank", {REG}, {}, V2, V4},
    {0xF8AD, "get_number_of_players2", {REG}, {}, V2, V4},
    {0xF8AE, "party_has_name", {REG}, {}, V2, V4},
    {0xF8AF, "someone_has_spoken", {REG}, {}, V2, V4},
    {0xF8B0, "read1", {REG, REG}, {}, V2, V2},
    {0xF8B0, "read1", {}, {REG, INT32}, V3, V4},
    {0xF8B1, "read2", {REG, REG}, {}, V2, V2},
    {0xF8B1, "read2", {}, {REG, INT32}, V3, V4},
    {0xF8B2, "read4", {REG, REG}, {}, V2, V2},
    {0xF8B2, "read4", {}, {REG, INT32}, V3, V4},
    {0xF8B3, "write1", {REG, REG}, {}, V2, V2},
    {0xF8B3, "write1", {}, {INT32, REG}, V3, V4},
    {0xF8B4, "write2", {REG, REG}, {}, V2, V2},
    {0xF8B4, "write2", {}, {INT32, REG}, V3, V4},
    {0xF8B5, "write4", {REG, REG}, {}, V2, V2},
    {0xF8B5, "write4", {}, {INT32, REG}, V3, V4},
    {0xF8B6, "check_for_hacking", {REG}, {}, V2, V2}, // Returns a bitmask of 5 different types of detectable hacking. But it only works on DCv2 - it crashes on all other versions.
    {0xF8B7, nullptr, {REG}, {}, V2, V4}, // TODO (DX) - Challenge mode. Appears to be timing-related; regA is expected to be in [60, 3600]. Encodes the value with some strange masking key, even though it's never sent over the network and is only decoded locally.
    {0xF8B8, "disable_retry_menu", {}, {}, V2, V4},
    {0xF8B9, "chl_recovery", {}, {}, V2, V4},
    {0xF8BA, "load_guild_card_file_creation_time_to_flag_buf", {}, {}, V2, V4},
    {0xF8BB, "write_flag_buf_to_event_flags2", {REG}, {}, V2, V4},
    {0xF8BC, "set_episode", {INT32}, {}, V3, V4},
    {0xF8C0, "file_dl_req", {}, {INT32, CSTRING}, V3, V3},
    {0xF8C0, "nop_F8C0", {}, {INT32, CSTRING}, V4, V4},
    {0xF8C1, "get_dl_status", {REG}, {}, V3, V3},
    {0xF8C1, "nop_F8C1", {REG}, {}, V4, V4},
    {0xF8C2, nullptr, {}, {}, V3, V3}, // TODO (DX) - related to GBA functionality
    {0xF8C2, "nop_F8C2", {}, {}, V4, V4},
    {0xF8C3, "get_gba_state", {REG}, {}, V3, V3},
    {0xF8C3, "nop_F8C3", {REG}, {}, V4, V4},
    {0xF8C4, "congrats_msg_multi_cm", {REG}, {}, V3, V3},
    {0xF8C4, "nop_F8C4", {REG}, {}, V4, V4},
    {0xF8C5, "stage_end_multi_cm", {REG}, {}, V3, V3},
    {0xF8C5, "nop_F8C5", {REG}, {}, V4, V4},
    {0xF8C6, "qexit", {}, {}, V3, V4},
    {0xF8C7, "use_animation", {REG, REG}, {}, V3, V4},
    {0xF8C8, "stop_animation", {REG}, {}, V3, V4},
    {0xF8C9, "run_to_coord", {{REG_SET_FIXED, 4}, REG}, {}, V3, V4},
    {0xF8CA, "set_slot_invincible", {REG, REG}, {}, V3, V4},
    {0xF8CB, "clear_slot_invincible", {REG}, {}, V3, V4},
    {0xF8CC, "set_slot_poison", {REG}, {}, V3, V4},
    {0xF8CD, "set_slot_paralyze", {REG}, {}, V3, V4},
    {0xF8CE, "set_slot_shock", {REG}, {}, V3, V4},
    {0xF8CF, "set_slot_freeze", {REG}, {}, V3, V4},
    {0xF8D0, "set_slot_slow", {REG}, {}, V3, V4},
    {0xF8D1, "set_slot_confuse", {REG}, {}, V3, V4},
    {0xF8D2, "set_slot_shifta", {REG}, {}, V3, V4},
    {0xF8D3, "set_slot_deband", {REG}, {}, V3, V4},
    {0xF8D4, "set_slot_jellen", {REG}, {}, V3, V4},
    {0xF8D5, "set_slot_zalure", {REG}, {}, V3, V4},
    {0xF8D6, "fleti_fixed_camera", {}, {{REG_SET_FIXED, 6}}, V3, V4},
    {0xF8D7, "fleti_locked_camera", {}, {INT32, {REG_SET_FIXED, 3}}, V3, V4},
    {0xF8D8, "default_camera_pos2", {}, {}, V3, V4},
    {0xF8D9, "set_motion_blur", {}, {}, V3, V4},
    {0xF8DA, "set_screen_bw", {}, {}, V3, V4},
    {0xF8DB, "get_vector_from_path", {}, {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, SCRIPT16}, V3, V4},
    {0xF8DC, "npc_action_string", {REG, REG, DATA16}, {}, V3, V4},
    {0xF8DD, "get_pad_cond", {REG, REG}, {}, V3, V4},
    {0xF8DE, "get_button_cond", {REG, REG}, {}, V3, V4},
    {0xF8DF, "freeze_enemies", {}, {}, V3, V4},
    {0xF8E0, "unfreeze_enemies", {}, {}, V3, V4},
    {0xF8E1, "freeze_everything", {}, {}, V3, V4},
    {0xF8E2, "unfreeze_everything", {}, {}, V3, V4},
    {0xF8E3, "restore_hp", {REG}, {}, V3, V4},
    {0xF8E4, "restore_tp", {REG}, {}, V3, V4},
    {0xF8E5, "close_chat_bubble", {REG}, {}, V3, V4},
    {0xF8E6, "move_coords_object", {REG, {REG_SET_FIXED, 3}}, {}, V3, V4},
    {0xF8E7, "at_coords_call_ex", {{REG_SET_FIXED, 5}, REG}, {}, V3, V4},
    {0xF8E8, "at_coords_talk_ex", {{REG_SET_FIXED, 5}, REG}, {}, V3, V4},
    {0xF8E9, "walk_to_coord_call_ex", {{REG_SET_FIXED, 5}, REG}, {}, V3, V4},
    {0xF8EA, "col_npcinr_ex", {{REG_SET_FIXED, 6}, REG}, {}, V3, V4},
    {0xF8EB, "set_obj_param_ex", {{REG_SET_FIXED, 6}, REG}, {}, V3, V4},
    {0xF8EC, "col_plinaw_ex", {{REG_SET_FIXED, 9}, REG}, {}, V3, V4},
    {0xF8ED, "animation_check", {REG, REG}, {}, V3, V4},
    {0xF8EE, "call_image_data", {}, {INT32, {LABEL16, Arg::DataType::IMAGE_DATA}}, V3, V4},
    {0xF8EF, "nop_F8EF", {}, {}, V3, V4},
    {0xF8F0, "turn_off_bgm_p2", {}, {}, V3, V4},
    {0xF8F1, "turn_on_bgm_p2", {}, {}, V3, V4},
    {0xF8F2, nullptr, {}, {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, {LABEL16, Arg::DataType::UNKNOWN_F8F2_DATA}}, V3, V4}, // TODO (DX)
    {0xF8F3, "particle2", {}, {{REG_SET_FIXED, 3}, INT32, FLOAT32}, V3, V4},
    {0xF901, "dec2float", {REG, REG}, {}, V3, V4},
    {0xF902, "float2dec", {REG, REG}, {}, V3, V4},
    {0xF903, "flet", {REG, REG}, {}, V3, V4},
    {0xF904, "fleti", {REG, FLOAT32}, {}, V3, V4},
    {0xF908, "fadd", {REG, REG}, {}, V3, V4},
    {0xF909, "faddi", {REG, FLOAT32}, {}, V3, V4},
    {0xF90A, "fsub", {REG, REG}, {}, V3, V4},
    {0xF90B, "fsubi", {REG, FLOAT32}, {}, V3, V4},
    {0xF90C, "fmul", {REG, REG}, {}, V3, V4},
    {0xF90D, "fmuli", {REG, FLOAT32}, {}, V3, V4},
    {0xF90E, "fdiv", {REG, REG}, {}, V3, V4},
    {0xF90F, "fdivi", {REG, FLOAT32}, {}, V3, V4},
    {0xF910, "get_total_deaths", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF911, "get_stackable_item_count", {{REG_SET_FIXED, 4}, REG}, {}, V3, V4}, // regsA[0] is client ID
    {0xF912, "freeze_and_hide_equip", {}, {}, V3, V4},
    {0xF913, "thaw_and_show_equip", {}, {}, V3, V4},
    {0xF914, "set_palettex_callback", {}, {CLIENT_ID, SCRIPT16}, V3, V4},
    {0xF915, "activate_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF916, "enable_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF917, "restore_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF918, "disable_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF919, "get_palettex_activated", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF91A, "get_unknown_palettex_status", {}, {CLIENT_ID, INT32, REG}, V3, V4}, // Middle arg is unused
    {0xF91B, "disable_movement2", {}, {CLIENT_ID}, V3, V4},
    {0xF91C, "enable_movement2", {}, {CLIENT_ID}, V3, V4},
    {0xF91D, "get_time_played", {REG}, {}, V3, V4},
    {0xF91E, "get_guildcard_total", {REG}, {}, V3, V4},
    {0xF91F, "get_slot_meseta", {REG}, {}, V3, V4},
    {0xF920, "get_player_level", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF921, "get_section_id", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF922, "get_player_hp", {}, {CLIENT_ID, {REG_SET_FIXED, 4}}, V3, V4},
    {0xF923, "get_floor_number", {}, {CLIENT_ID, {REG_SET_FIXED, 2}}, V3, V4},
    {0xF924, "get_coord_player_detect", {{REG_SET_FIXED, 3}, {REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF925, "read_global_flag", {}, {INT32, REG}, V3, V4},
    {0xF926, "write_global_flag", {}, {INT32, INT32}, V3, V4},
    {0xF927, "item_detect_bank2", {{REG_SET_FIXED, 4}, REG}, {}, V3, V4},
    {0xF928, "floor_player_detect", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF929, "read_disk_file", {}, {CSTRING}, V3, V3},
    {0xF929, "nop_F929", {}, {CSTRING}, V4, V4},
    {0xF92A, "open_pack_select", {}, {}, V3, V4},
    {0xF92B, "item_select", {REG}, {}, V3, V4},
    {0xF92C, "get_item_id", {REG}, {}, V3, V4},
    {0xF92D, "color_change", {}, {INT32, INT32, INT32, INT32, INT32}, V3, V4},
    {0xF92E, "send_statistic", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V3, V4},
    {0xF92F, nullptr, {}, {INT32, INT32}, V3, V3}, // TODO (DX) - related to GBA functionality
    {0xF92F, "nop_F92F", {}, {INT32, INT32}, V4, V4},
    {0xF930, "chat_box", {}, {FLOAT32, FLOAT32, FLOAT32, FLOAT32, INT32, CSTRING}, V3, V4},
    {0xF931, "chat_bubble", {}, {INT32, CSTRING}, V3, V4},
    {0xF932, "set_episode2", {REG}, {}, V3, V4},
    {0xF933, "item_create_multi_cm", {{REG_SET_FIXED, 7}}, {}, V3, V3}, // regsA[1-6] form an ItemData's data1[0-5]
    {0xF933, "nop_F933", {{REG_SET_FIXED, 7}}, {}, V4, V4},
    {0xF934, "scroll_text", {}, {INT32, INT32, INT32, INT32, INT32, FLOAT32, REG, CSTRING}, V3, V4},
    {0xF935, "gba_create_dl_graph", {}, {}, V3, V3},
    {0xF935, "nop_F935", {}, {}, V4, V4},
    {0xF936, "gba_destroy_dl_graph", {}, {}, V3, V3},
    {0xF936, "nop_F936", {}, {}, V4, V4},
    {0xF937, "gba_update_dl_graph", {}, {}, V3, V3},
    {0xF937, "nop_F937", {}, {}, V4, V4},
    {0xF938, "add_damage_to", {}, {INT32, FLOAT32}, V3, V4},
    {0xF939, "item_delete3", {}, {INT32}, V3, V4},
    {0xF93A, "get_item_info", {}, {ITEM_ID, {REG_SET_FIXED, 12}}, V3, V4}, // regsB are item.data1
    {0xF93B, "item_packing1", {}, {ITEM_ID}, V3, V4},
    {0xF93C, "item_packing2", {}, {ITEM_ID, INT32}, V3, V4}, // Sends 6xD6 on BB
    {0xF93D, "get_lang_setting", {}, {REG}, V3, V4},
    {0xF93E, "prepare_statistic", {}, {INT32, INT32, INT32}, V3, V4},
    {0xF93F, "keyword_detect", {}, {}, V3, V4},
    {0xF940, "keyword", {}, {REG, INT32, CSTRING}, V3, V4},
    {0xF941, "get_guildcard_num", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF942, "get_recent_symbol_chat", {}, {INT32, {REG_SET_FIXED, 15}}, V3, V4}, // argA = client ID, regsB = symbol chat data (out)
    {0xF943, "create_symbol_chat_capture_buffer", {}, {}, V3, V4},
    {0xF944, "get_item_stackability", {}, {ITEM_ID, REG}, V3, V4},
    {0xF945, "initial_floor", {}, {INT32}, V3, V4},
    {0xF946, "sin", {}, {REG, INT32}, V3, V4},
    {0xF947, "cos", {}, {REG, INT32}, V3, V4},
    {0xF948, "tan", {}, {REG, INT32}, V3, V4},
    {0xF949, "atan2_int", {}, {REG, FLOAT32, FLOAT32}, V3, V4},
    {0xF94A, "olga_flow_is_dead", {REG}, {}, V3, V4},
    {0xF94B, "particle_effect_nc", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF94C, "player_effect_nc", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF94D, "give_or_take_card", {{REG_SET_FIXED, 2}}, {}, V3, V3}, // Ep3 only; regsA[0] is card_id; card is given if regsA[1] >= 0, otherwise it's taken
    {0xF94D, "nop_F94D", {}, {}, V4, V4},
    {0xF94E, "nop_F94E", {}, {}, V4, V4},
    {0xF94F, "nop_F94F", {}, {}, V4, V4},
    {0xF950, "bb_p2_menu", {}, {INT32}, V4, V4},
    {0xF951, "bb_map_designate", {INT8, INT8, INT8, INT8, INT8}, {}, V4, V4},
    {0xF952, "bb_get_number_in_pack", {REG}, {}, V4, V4},
    {0xF953, "bb_swap_item", {}, {INT32, INT32, INT32, INT32, INT32, INT32, SCRIPT16, SCRIPT16}, V4, V4}, // Sends 6xD5
    {0xF954, "bb_check_wrap", {}, {INT32, REG}, V4, V4},
    {0xF955, "bb_exchange_pd_item", {}, {INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xD7
    {0xF956, "bb_exchange_pd_srank", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xD8
    {0xF957, "bb_exchange_pd_special", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xDA
    {0xF958, "bb_exchange_pd_percent", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xDA
    {0xF959, "bb_set_ep4_boss_can_escape", {}, {INT32}, V4, V4},
    {0xF95A, nullptr, {REG}, {}, V4, V4}, // TODO (DX) - related to Episode 4 boss
    {0xF95B, "bb_send_6xD9", {}, {INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xD9
    {0xF95C, "bb_exchange_slt", {}, {INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xDE
    {0xF95D, "bb_exchange_pc", {}, {}, V4, V4}, // Sends 6xDF
    {0xF95E, "bb_box_create_bp", {}, {INT32, INT32, INT32}, V4, V4}, // Sends 6xE0
    {0xF95F, "bb_exchange_pt", {}, {INT32, INT32, INT32, INT32, INT32}, V4, V4}, // Sends 6xE1
    {0xF960, "bb_send_6xE2", {}, {INT32}, V4, V4}, // Sends 6xE2
    {0xF961, "bb_get_6xE3_status", {REG}, {}, V4, V4}, // Returns 0 if 6xE3 hasn't been received, 1 if the received item is valid, 2 if the received item is invalid
};

static const unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>&
opcodes_for_version(QuestScriptOpcodeDefinition::Version v) {
  static array<
      unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>,
      static_cast<size_t>(QuestScriptOpcodeDefinition::Version::NUM_VERSIONS)>
      indexes;

  size_t v_index = static_cast<size_t>(v);
  auto& index = indexes.at(v_index);
  if (index.empty()) {
    for (size_t z = 0; z < sizeof(opcode_defs) / sizeof(opcode_defs[0]); z++) {
      const auto& def = opcode_defs[z];
      if (v_index < static_cast<size_t>(def.first_version)) {
        continue;
      }
      if (v_index > static_cast<size_t>(def.last_version)) {
        continue;
      }
      if (!index.emplace(def.opcode, &def).second) {
        throw logic_error(string_printf("duplicate definition for opcode %04hX", def.opcode));
      }
    }
  }
  return index;
}

std::string disassemble_quest_script(const void* data, size_t size, GameVersion version, bool is_dcv1) {
  StringReader r(data, size);
  deque<string> lines;

  bool use_wstrs = false;
  size_t code_offset = 0;
  size_t function_table_offset = 0;
  QuestScriptOpcodeDefinition::Version v;
  switch (version) {
    case GameVersion::DC: {
      v = is_dcv1 ? V1 : V2;
      const auto& header = r.get<PSOQuestHeaderDC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      lines.emplace_back(string_printf(".quest_num %hu", header.quest_number.load()));
      if (header.is_download) {
        lines.emplace_back(string_printf(".is_download_quest"));
      }
      lines.emplace_back(".name " + format_data_string(header.name.data(), header.name.len()));
      lines.emplace_back(".short_desc " + format_data_string(header.short_description.data(), header.short_description.len()));
      lines.emplace_back(".long_desc " + format_data_string(header.long_description.data(), header.long_description.len()));
      break;
    }
    case GameVersion::PC: {
      v = V2;
      use_wstrs = true;
      const auto& header = r.get<PSOQuestHeaderPC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      lines.emplace_back(string_printf(".quest_num %hu", header.quest_number.load()));
      if (header.is_download) {
        lines.emplace_back(string_printf(".is_download_quest"));
      }
      lines.emplace_back(".name " + dasm_u16string(header.name));
      lines.emplace_back(".short_desc " + dasm_u16string(header.short_description));
      lines.emplace_back(".long_desc " + dasm_u16string(header.long_description));
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      v = V3;
      const auto& header = r.get<PSOQuestHeaderGC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      lines.emplace_back(string_printf(".quest_num %hhu", header.quest_number));
      if (header.is_download) {
        lines.emplace_back(string_printf(".is_download_quest"));
      }
      lines.emplace_back(string_printf(".episode %hhu", header.episode));
      lines.emplace_back(".name " + format_data_string(header.name.data(), header.name.len()));
      lines.emplace_back(".short_desc " + format_data_string(header.short_description.data(), header.short_description.len()));
      lines.emplace_back(".long_desc " + format_data_string(header.long_description.data(), header.long_description.len()));
      break;
    }
    case GameVersion::BB: {
      v = V4;
      use_wstrs = true;
      const auto& header = r.get<PSOQuestHeaderBB>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      lines.emplace_back(string_printf(".quest_num %hu", header.quest_number.load()));
      lines.emplace_back(string_printf(".episode %hhu", header.episode));
      lines.emplace_back(string_printf(".max_players %hhu", header.episode));
      if (header.joinable_in_progress) {
        lines.emplace_back(".joinable_in_progress");
      }
      lines.emplace_back(".name " + dasm_u16string(header.name));
      lines.emplace_back(".short_desc " + dasm_u16string(header.short_description));
      lines.emplace_back(".long_desc " + dasm_u16string(header.long_description));
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }

  const auto& opcodes = opcodes_for_version(v);
  StringReader cmd_r = r.sub(code_offset, function_table_offset - code_offset);

  struct Label {
    string name;
    uint32_t offset;
    uint32_t function_id; // 0xFFFFFFFF = no function ID
    uint64_t type_flags;
    set<size_t> references;

    Label(const string& name, uint32_t offset, int64_t function_id = -1, uint64_t type_flags = 0)
        : name(name),
          offset(offset),
          function_id(function_id),
          type_flags(type_flags) {}
    void add_data_type(Arg::DataType type) {
      this->type_flags |= (1 << static_cast<size_t>(type));
    }
    bool has_data_type(Arg::DataType type) const {
      return this->type_flags & (1 << static_cast<size_t>(type));
    }
  };

  vector<shared_ptr<Label>> function_table;
  multimap<size_t, shared_ptr<Label>> offset_to_label;
  StringReader function_table_r = r.sub(function_table_offset);
  while (!function_table_r.eof()) {
    try {
      uint32_t function_id = function_table.size();
      string name = string_printf("label%04" PRIX32, function_id);
      uint32_t offset = function_table_r.get_u32l();
      shared_ptr<Label> l(new Label(name, offset, function_id));
      if (function_id == 0) {
        l->add_data_type(Arg::DataType::SCRIPT);
      }
      function_table.emplace_back(l);
      if (l->offset < cmd_r.size()) {
        offset_to_label.emplace(l->offset, l);
      }
    } catch (const out_of_range&) {
      function_table_r.skip(function_table_r.remaining());
    }
  }

  struct DisassemblyLine {
    string line;
    size_t next_offset;

    DisassemblyLine(string&& line, size_t next_offset)
        : line(std::move(line)),
          next_offset(next_offset) {}
  };

  struct ArgStackValue {
    enum class Type {
      REG,
      REG_PTR,
      LABEL,
      INT,
      CSTRING,
    };
    Type type;
    uint32_t as_int;
    std::string as_string;

    ArgStackValue(Type type, uint32_t value) {
      this->type = type;
      this->as_int = value;
    }
    ArgStackValue(const std::string& value) {
      this->type = Type::CSTRING;
      this->as_string = value;
    }
  };

  map<size_t, DisassemblyLine> dasm_lines;
  set<size_t> pending_dasm_start_offsets;
  for (const auto& l : function_table) {
    if (l->offset < cmd_r.size()) {
      pending_dasm_start_offsets.emplace(l->offset);
    }
  }

  while (!pending_dasm_start_offsets.empty()) {
    auto dasm_start_offset_it = pending_dasm_start_offsets.begin();
    cmd_r.go(*dasm_start_offset_it);
    pending_dasm_start_offsets.erase(dasm_start_offset_it);

    vector<ArgStackValue> arg_stack_values;
    while (!cmd_r.eof() && !dasm_lines.count(cmd_r.where())) {
      size_t opcode_start_offset = cmd_r.where();
      string dasm_line;
      try {
        uint16_t opcode = cmd_r.get_u8();
        if ((opcode & 0xFE) == 0xF8) {
          opcode = (opcode << 8) | cmd_r.get_u8();
        }

        const QuestScriptOpcodeDefinition* def = nullptr;
        try {
          def = opcodes.at(opcode);
        } catch (const out_of_range&) {
        }

        if (def == nullptr) {
          dasm_line = string_printf(".unknown %04hX", opcode);
        } else {
          dasm_line = def->name ? def->name : string_printf("[%04hX]", opcode);
          if (!def->imm_args.empty()) {
            dasm_line.resize(0x20, ' ');
            bool is_first_arg = true;
            for (const auto& arg : def->imm_args) {
              using Type = QuestScriptOpcodeDefinition::Argument::Type;
              string dasm_arg;
              switch (arg.type) {
                case Type::LABEL16:
                case Type::LABEL32: {
                  uint32_t label_id = (arg.type == Type::LABEL32) ? cmd_r.get_u32l() : cmd_r.get_u16l();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::LABEL, label_id);
                  }
                  if (label_id >= function_table.size()) {
                    dasm_arg = string_printf("label%04" PRIX32 " /* invalid */", label_id);
                  } else {
                    auto& l = function_table.at(label_id);
                    dasm_arg = string_printf("label%04" PRIX32 " /* %04" PRIX32 " */", label_id, l->offset);
                    l->references.emplace(opcode_start_offset);
                    l->add_data_type(arg.data_type);
                    if (arg.data_type == Arg::DataType::SCRIPT) {
                      pending_dasm_start_offsets.emplace(l->offset);
                    }
                  }
                  break;
                }
                case Type::LABEL16_SET: {
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    throw logic_error("LABEL16_SET cannot be pushed to arg stack");
                  }
                  uint8_t num_functions = cmd_r.get_u8();
                  for (size_t z = 0; z < num_functions; z++) {
                    dasm_arg += (dasm_arg.empty() ? "(" : ", ");
                    uint32_t label_id = cmd_r.get_u16l();
                    if (label_id >= function_table.size()) {
                      dasm_arg += string_printf("function%04" PRIX32 " /* invalid */", label_id);
                    } else {
                      auto& l = function_table.at(label_id);
                      dasm_arg += string_printf("label%04" PRIX32 " /* %04" PRIX32 " */", label_id, l->offset);
                      l->references.emplace(opcode_start_offset);
                      l->add_data_type(arg.data_type);
                      if (arg.data_type == Arg::DataType::SCRIPT) {
                        pending_dasm_start_offsets.emplace(l->offset);
                      }
                    }
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "()";
                  } else {
                    dasm_arg += ")";
                  }
                  break;
                }
                case Type::REG: {
                  uint8_t reg = cmd_r.get_u8();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back((def->opcode == 0x004C) ? ArgStackValue::Type::REG_PTR : ArgStackValue::Type::REG, reg);
                  }
                  dasm_arg = string_printf("r%hhu", reg);
                  break;
                }
                case Type::REG_SET: {
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    throw logic_error("REG_SET cannot be pushed to arg stack");
                  }
                  uint8_t num_regs = cmd_r.get_u8();
                  for (size_t z = 0; z < num_regs; z++) {
                    dasm_arg += string_printf("%sr%hhu", (dasm_arg.empty() ? "(" : ", "), cmd_r.get_u8());
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "()";
                  } else {
                    dasm_arg += ")";
                  }
                  break;
                }
                case Type::REG_SET_FIXED: {
                  uint8_t first_reg = cmd_r.get_u8();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    throw logic_error("REG_SET_FIXED cannot be pushed to arg stack");
                  }
                  dasm_arg = string_printf("r%hhu-r%hhu", first_reg, static_cast<uint8_t>(first_reg + arg.count - 1));
                  break;
                }
                case Type::INT8: {
                  uint8_t v = cmd_r.get_u8();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = string_printf("0x%02hhX", v);
                  break;
                }
                case Type::INT16: {
                  uint16_t v = cmd_r.get_u16l();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = string_printf("0x%04hX", v);
                  break;
                }
                case Type::INT32: {
                  uint32_t v = cmd_r.get_u32l();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = string_printf("0x%08" PRIX32, v);
                  break;
                }
                case Type::FLOAT32: {
                  float v = cmd_r.get_f32l();
                  if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, *reinterpret_cast<const uint32_t*>(&v));
                  }
                  dasm_arg = string_printf("%g", v);
                  break;
                }
                case Type::CSTRING:
                  if (use_wstrs) {
                    u16string s;
                    for (char16_t ch = cmd_r.get_u16l(); ch; ch = cmd_r.get_u16l()) {
                      s.push_back(ch);
                    }
                    if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                      arg_stack_values.emplace_back(encode_sjis(s));
                    }
                    dasm_arg = dasm_u16string(s.data(), s.size());
                  } else {
                    string s = cmd_r.get_cstr();
                    if (def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK) {
                      arg_stack_values.emplace_back(s);
                    }
                    dasm_arg = format_data_string(s);
                  }
                  break;
                default:
                  throw logic_error("invalid argument type");
              }
              if (!is_first_arg) {
                dasm_line += ", ";
              } else {
                is_first_arg = false;
              }
              dasm_line += dasm_arg;
            }
          }

          if (!def->stack_args.empty()) {
            if (!def->imm_args.empty()) {
              throw logic_error("opcode has both imm_args and stack_args");
            }
            dasm_line.resize(0x20, ' ');
            dasm_line += "... ";

            if (def->stack_args.size() != arg_stack_values.size()) {
              dasm_line += string_printf("/* matching error: expected %zu arguments, received %zu arguments */",
                  def->stack_args.size(), arg_stack_values.size());
            } else {
              bool is_first_arg = true;
              for (size_t z = 0; z < def->stack_args.size(); z++) {
                const auto& arg_def = def->stack_args[z];
                const auto& arg_value = arg_stack_values[z];

                string dasm_arg;
                switch (arg_def.type) {
                  case Arg::Type::LABEL16:
                  case Arg::Type::LABEL32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = string_printf("r%" PRIu32 "/* warning: cannot determine label data type */", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::LABEL:
                      case ArgStackValue::Type::INT:
                        dasm_arg = string_printf("label%04" PRIX32, arg_value.as_int);
                        try {
                          auto l = function_table.at(arg_value.as_int);
                          l->add_data_type(arg_def.data_type);
                          l->references.emplace(opcode_start_offset);
                        } catch (const out_of_range&) {
                        }
                        break;
                      default:
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::REG:
                  case Arg::Type::REG32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = string_printf("regs[r%" PRIu32 "]", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = string_printf("r%" PRIu32, arg_value.as_int);
                        break;
                      default:
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::REG_SET_FIXED:
                  case Arg::Type::REG32_SET_FIXED:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = string_printf("regs[r%" PRIu32 "]-regs[r%" PRIu32 "+%hhu]", arg_value.as_int, arg_value.as_int, static_cast<uint8_t>(arg_def.count - 1));
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = string_printf("r%" PRIu32 "-r%hhu", arg_value.as_int, static_cast<uint8_t>(arg_value.as_int + arg_def.count - 1));
                        break;
                      default:
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::INT8:
                  case Arg::Type::INT16:
                  case Arg::Type::INT32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = string_printf("r%" PRIu32, arg_value.as_int);
                        break;
                      case ArgStackValue::Type::REG_PTR:
                        dasm_arg = string_printf("&r%" PRIu32, arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = string_printf("0x%" PRIX32 " /* %" PRIu32 " */", arg_value.as_int, arg_value.as_int);
                        break;
                      default:
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::FLOAT32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = string_printf("(float)r%" PRIu32, arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = string_printf("%g", *reinterpret_cast<const float*>(&arg_value.as_int));
                        break;
                      default:
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::CSTRING:
                    if (arg_value.type == ArgStackValue::Type::CSTRING) {
                      dasm_arg = format_data_string(arg_value.as_string);
                    } else {
                      dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::LABEL16_SET:
                  case Arg::Type::REG_SET:
                  default:
                    throw logic_error("set-type arg found on arg stack");
                }

                if (!is_first_arg) {
                  dasm_line += ", ";
                } else {
                  is_first_arg = false;
                }
                dasm_line += dasm_arg;
              }
            }
          }

          if (!(def->flags & QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK)) {
            arg_stack_values.clear();
          }
        }
      } catch (const exception& e) {
        dasm_line = string_printf(".failed (%s)", e.what());
      }

      string hex_data = format_data_string(cmd_r.preadx(opcode_start_offset, cmd_r.where() - opcode_start_offset), nullptr, FormatDataFlags::HEX_ONLY);
      if (hex_data.size() > 14) {
        hex_data.resize(12);
        hex_data += "...";
      }
      hex_data.resize(16, ' ');

      dasm_lines.emplace(
          opcode_start_offset,
          DisassemblyLine(
              string_printf("  %04zX (+%04zX)  %s  %s", opcode_start_offset, opcode_start_offset + code_offset, hex_data.c_str(), dasm_line.c_str()),
              cmd_r.where()));
    }
  }

  auto label_it = offset_to_label.begin();
  while (label_it != offset_to_label.end()) {
    auto l = label_it->second;
    label_it++;
    size_t size = ((label_it == offset_to_label.end()) ? cmd_r.size() : label_it->second->offset) - l->offset;
    if (size > 0) {
      lines.emplace_back();
    }
    if (l->function_id == 0) {
      lines.emplace_back("start:");
    }
    lines.emplace_back(string_printf("label%04" PRIX32 ":", l->function_id));
    if (l->references.size() == 1) {
      lines.emplace_back(string_printf("  // Referenced by instruction at %04zX", *l->references.begin()));
    } else if (!l->references.empty()) {
      vector<string> tokens;
      tokens.reserve(l->references.size());
      for (size_t reference_offset : l->references) {
        tokens.emplace_back(string_printf("%04zX", reference_offset));
      }
      lines.emplace_back("  // Referenced by instructions at " + join(tokens, ", "));
    }

    auto print_as_struct = [&]<Arg::DataType data_type, typename StructT>(function<void(const StructT&)> print_fn) {
      if (l->has_data_type(data_type)) {
        if (size >= sizeof(StructT)) {
          print_fn(cmd_r.pget<StructT>(l->offset));
          if (size > sizeof(StructT)) {
            size_t struct_end_offset = l->offset + sizeof(StructT);
            size_t remaining_size = size - sizeof(StructT);
            lines.emplace_back("  // Extra data after structure");
            lines.emplace_back(format_and_indent_data(cmd_r.pgetv(struct_end_offset, remaining_size), remaining_size, struct_end_offset));
          }
        } else {
          lines.emplace_back(string_printf("  // As raw data (0x%zX bytes; too small for referenced type)", size));
          lines.emplace_back(format_and_indent_data(cmd_r.pgetv(l->offset, size), size, l->offset));
        }
      }
    };

    if (l->type_flags == 0) {
      lines.emplace_back(string_printf("  // Could not determine data type; disassembling as code and raw data"));
      l->add_data_type(Arg::DataType::SCRIPT);
      l->add_data_type(Arg::DataType::DATA);
    }

    // Print data interpretations of the label (if any)
    if (l->has_data_type(Arg::DataType::DATA)) {
      // TODO: We should produce a print_data-like view here
      lines.emplace_back(string_printf("  // As raw data (0x%zX bytes)", size));
      lines.emplace_back(format_and_indent_data(cmd_r.pgetv(l->offset, size), size, l->offset));
    }
    print_as_struct.template operator()<Arg::DataType::PLAYER_VISUAL_CONFIG, PlayerVisualConfig>([&](const PlayerVisualConfig& visual) -> void {
      lines.emplace_back("  // As PlayerVisualConfig");
      string name = format_data_string(visual.name);
      lines.emplace_back(string_printf("  name         %s", name.c_str()));
      lines.emplace_back(string_printf("  name_color   %08" PRIX32, visual.name_color.load()));
      lines.emplace_back(string_printf("  a2           %016" PRIX64, visual.unknown_a2.load()));
      lines.emplace_back(string_printf("  extra_model  %02hhX", visual.extra_model));
      string unused = format_data_string(visual.unused.data(), visual.unused.bytes());
      lines.emplace_back("  unused       " + unused);
      lines.emplace_back(string_printf("  a3           %08" PRIX32, visual.unknown_a3.load()));
      string secid_name = name_for_section_id(visual.section_id);
      lines.emplace_back(string_printf("  section_id   %02hhX (%s)", visual.section_id, secid_name.c_str()));
      lines.emplace_back(string_printf("  char_class   %02hhX (%s)", visual.char_class, name_for_char_class(visual.char_class)));
      lines.emplace_back(string_printf("  v2_flags     %02hhX", visual.v2_flags));
      lines.emplace_back(string_printf("  version      %02hhX", visual.version));
      lines.emplace_back(string_printf("  v1_flags     %08" PRIX32, visual.v1_flags.load()));
      lines.emplace_back(string_printf("  costume      %04hX", visual.costume.load()));
      lines.emplace_back(string_printf("  skin         %04hX", visual.skin.load()));
      lines.emplace_back(string_printf("  face         %04hX", visual.face.load()));
      lines.emplace_back(string_printf("  head         %04hX", visual.head.load()));
      lines.emplace_back(string_printf("  hair         %04hX", visual.hair.load()));
      lines.emplace_back(string_printf("  hair_color   %04hX, %04hX, %04hX", visual.hair_r.load(), visual.hair_g.load(), visual.hair_b.load()));
      lines.emplace_back(string_printf("  proportion   %g, %g", visual.proportion_x.load(), visual.proportion_y.load()));
    });
    print_as_struct.template operator()<Arg::DataType::PLAYER_STATS, PlayerStats>([&](const PlayerStats& stats) -> void {
      lines.emplace_back("  // As PlayerStats");
      lines.emplace_back(string_printf("  atp          %04hX /* %hu */", stats.char_stats.atp.load(), stats.char_stats.atp.load()));
      lines.emplace_back(string_printf("  mst          %04hX /* %hu */", stats.char_stats.mst.load(), stats.char_stats.mst.load()));
      lines.emplace_back(string_printf("  evp          %04hX /* %hu */", stats.char_stats.evp.load(), stats.char_stats.evp.load()));
      lines.emplace_back(string_printf("  hp           %04hX /* %hu */", stats.char_stats.hp.load(), stats.char_stats.hp.load()));
      lines.emplace_back(string_printf("  dfp          %04hX /* %hu */", stats.char_stats.dfp.load(), stats.char_stats.dfp.load()));
      lines.emplace_back(string_printf("  ata          %04hX /* %hu */", stats.char_stats.ata.load(), stats.char_stats.ata.load()));
      lines.emplace_back(string_printf("  lck          %04hX /* %hu */", stats.char_stats.lck.load(), stats.char_stats.lck.load()));
      lines.emplace_back(string_printf("  a1           %04hX /* %hu */", stats.unknown_a1.load(), stats.unknown_a1.load()));
      lines.emplace_back(string_printf("  a2           %08" PRIX32 " /* %g */", stats.unknown_a2.load_raw(), stats.unknown_a2.load()));
      lines.emplace_back(string_printf("  a3           %08" PRIX32 " /* %g */", stats.unknown_a3.load_raw(), stats.unknown_a3.load()));
      lines.emplace_back(string_printf("  level        %08" PRIX32 " /* level %" PRIu32 " */", stats.level.load(), stats.level.load() + 1));
      lines.emplace_back(string_printf("  experience   %08" PRIX32 " /* %" PRIu32 " */", stats.experience.load(), stats.experience.load()));
      lines.emplace_back(string_printf("  meseta       %08" PRIX32 " /* %" PRIu32 " */", stats.meseta.load(), stats.meseta.load()));
    });
    print_as_struct.template operator()<Arg::DataType::RESIST_DATA, ResistData>([&](const ResistData& resist) -> void {
      lines.emplace_back("  // As ResistData");
      lines.emplace_back(string_printf("  evp_bonus    %04hX /* %hu */", resist.evp_bonus.load(), resist.evp_bonus.load()));
      lines.emplace_back(string_printf("  a1           %04hX /* %hu */", resist.unknown_a1.load(), resist.unknown_a1.load()));
      lines.emplace_back(string_printf("  a2           %04hX /* %hu */", resist.unknown_a2.load(), resist.unknown_a2.load()));
      lines.emplace_back(string_printf("  a3           %04hX /* %hu */", resist.unknown_a3.load(), resist.unknown_a3.load()));
      lines.emplace_back(string_printf("  a4           %04hX /* %hu */", resist.unknown_a4.load(), resist.unknown_a4.load()));
      lines.emplace_back(string_printf("  a5           %04hX /* %hu */", resist.unknown_a5.load(), resist.unknown_a5.load()));
      lines.emplace_back(string_printf("  a6           %08" PRIX32 " /* %" PRIu32 " */", resist.unknown_a6.load(), resist.unknown_a6.load()));
      lines.emplace_back(string_printf("  a7           %08" PRIX32 " /* %" PRIu32 " */", resist.unknown_a7.load(), resist.unknown_a7.load()));
      lines.emplace_back(string_printf("  a8           %08" PRIX32 " /* %" PRIu32 " */", resist.unknown_a8.load(), resist.unknown_a8.load()));
      lines.emplace_back(string_printf("  a9           %08" PRIX32 " /* %" PRIu32 " */", resist.unknown_a9.load(), resist.unknown_a9.load()));
      lines.emplace_back(string_printf("  dfp_bonus    %08" PRIX32 " /* %" PRIu32 " */", resist.dfp_bonus.load(), resist.dfp_bonus.load()));
    });
    print_as_struct.template operator()<Arg::DataType::ATTACK_DATA, AttackData>([&](const AttackData& attack) -> void {
      lines.emplace_back("  // As AttackData");
      lines.emplace_back(string_printf("  a1           %04hX /* %hd */", attack.unknown_a1.load(), attack.unknown_a1.load()));
      lines.emplace_back(string_printf("  a2           %04hX /* %hd */", attack.unknown_a2.load(), attack.unknown_a2.load()));
      lines.emplace_back(string_printf("  a3           %04hX /* %hu */", attack.unknown_a3.load(), attack.unknown_a3.load()));
      lines.emplace_back(string_printf("  a4           %04hX /* %hu */", attack.unknown_a4.load(), attack.unknown_a4.load()));
      lines.emplace_back(string_printf("  a5           %08" PRIX32 " /* %g */", attack.unknown_a5.load_raw(), attack.unknown_a5.load()));
      lines.emplace_back(string_printf("  a6           %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a6.load(), attack.unknown_a6.load()));
      lines.emplace_back(string_printf("  a7           %08" PRIX32 " /* %g */", attack.unknown_a7.load_raw(), attack.unknown_a7.load()));
      lines.emplace_back(string_printf("  a8           %04hX /* %hu */", attack.unknown_a8.load(), attack.unknown_a8.load()));
      lines.emplace_back(string_printf("  a9           %04hX /* %hu */", attack.unknown_a9.load(), attack.unknown_a9.load()));
      lines.emplace_back(string_printf("  a10          %04hX /* %hu */", attack.unknown_a10.load(), attack.unknown_a10.load()));
      lines.emplace_back(string_printf("  a11          %04hX /* %hu */", attack.unknown_a11.load(), attack.unknown_a11.load()));
      lines.emplace_back(string_printf("  a12          %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a12.load(), attack.unknown_a12.load()));
      lines.emplace_back(string_printf("  a13          %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a13.load(), attack.unknown_a13.load()));
      lines.emplace_back(string_printf("  a14          %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a14.load(), attack.unknown_a14.load()));
      lines.emplace_back(string_printf("  a15          %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a15.load(), attack.unknown_a15.load()));
      lines.emplace_back(string_printf("  a16          %08" PRIX32 " /* %" PRIu32 " */", attack.unknown_a16.load(), attack.unknown_a16.load()));
    });
    print_as_struct.template operator()<Arg::DataType::MOVEMENT_DATA, MovementData>([&](const MovementData& movement) -> void {
      lines.emplace_back("  // As MovementData");
      for (size_t z = 0; z < 6; z++) {
        lines.emplace_back(string_printf("  a1[%zu]        %08" PRIX32 " /* %g */",
            z, movement.unknown_a1[z].load_raw(), movement.unknown_a1[z].load()));
      }
      for (size_t z = 0; z < 6; z++) {
        lines.emplace_back(string_printf("  a2[%zu]        %08" PRIX32 " /* %g */",
            z, movement.unknown_a2[z].load_raw(), movement.unknown_a2[z].load()));
      }
    });
    if (l->has_data_type(Arg::DataType::IMAGE_DATA)) {
      const void* data = cmd_r.pgetv(l->offset, size);
      auto decompressed = prs_decompress_with_meta(data, size);
      lines.emplace_back(string_printf("  // As decompressed image data (0x%zX bytes)", decompressed.data.size()));
      lines.emplace_back(format_and_indent_data(decompressed.data.data(), decompressed.data.size(), 0));
      if (decompressed.input_bytes_used < size) {
        size_t compressed_end_offset = l->offset + decompressed.input_bytes_used;
        size_t remaining_size = size - decompressed.input_bytes_used;
        lines.emplace_back("  // Extra data after compressed data");
        lines.emplace_back(format_and_indent_data(cmd_r.pgetv(compressed_end_offset, remaining_size), remaining_size, compressed_end_offset));
      }
    }
    if (l->has_data_type(Arg::DataType::UNKNOWN_F8F2_DATA)) {
      StringReader r = cmd_r.sub(l->offset, size);
      lines.emplace_back("  // As F8F2 entries");
      while (r.remaining() >= sizeof(UnknownF8F2Entry)) {
        const auto& e = r.get<UnknownF8F2Entry>();
        lines.emplace_back(string_printf("  entry        %g, %g, %g, %g", e.unknown_a1[0].load(), e.unknown_a1[1].load(), e.unknown_a1[2].load(), e.unknown_a1[3].load()));
      }
      if (r.remaining() > 0) {
        size_t struct_end_offset = l->offset + r.where();
        size_t remaining_size = r.remaining();
        lines.emplace_back("  // Extra data after structures");
        lines.emplace_back(format_and_indent_data(r.getv(remaining_size), remaining_size, struct_end_offset));
      }
    }
    if (l->has_data_type(Arg::DataType::SCRIPT)) {
      for (size_t z = l->offset; z < l->offset + size;) {
        const auto& l = dasm_lines.at(z);
        lines.emplace_back(l.line);
        if (l.next_offset <= z) {
          throw logic_error("line points backward or to itself");
        }
        z = l.next_offset;
      }
    }
  }

  lines.emplace_back(); // Add a \n on the end
  return join(lines, "\n");
}
