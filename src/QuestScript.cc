#include "QuestScript.hh"

#include <stdint.h>
#include <string.h>

#include <array>
#include <deque>
#include <filesystem>
#include <map>
#include <phosg/Math.hh>
#include <phosg/Strings.hh>
#include <resource_file/Emulators/PPC32Emulator.hh>
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>
#include <set>
#include <unordered_map>
#include <vector>

#include "BattleParamsIndex.hh"
#include "CommandFormats.hh"
#include "Compression.hh"
#include "StaticGameData.hh"

using namespace std;

// This file documents PSO's quest script execution system.

// The quest execution system has several relevant data structures:
// - The quest script is a stream of binary data containing opcodes (as defined below), each followed by their
//   arguments. The offset of the code section of this stream is defined here.
// - The execution state specifies what the client should do on every frame. There are many possible states here, such
//   as waiting for the player to dismiss a chat bubble, choose an item from a menu, etc.
// - The function table is a list of offsets into the quest script which can be used as targets for jumps and calls, as
//   well as references to large data structures that don't fit in quest opcode arguments.
// - The quest registers are 32-bit integers referred to as r0-r255. In later versions, registers may contain floating-
//   point values, in which case they're referred to as f0-f255 (but they still occupy the same memory as r0-255).
// - The args list is a list of up to 8 32-bit values used for many quest opcodes in v3 and later. These opcodes are
//   preceded by one or more arg_push opcodes, which allow scripts the ability to pass values from immediate data,
//   registers, labels, or even pointers to registers. Opcodes that use the args list are tagged with F_ARGS below.
// - The stack is an array of 32-bit integers (16 of them on v1/v2, 64 of them on v3/v4), which is used by the call and
//   ret opcodes (which push and pop offsets into the quest script), but may also be used by the stack_push and
//   stack_pop opcodes to work with arbitrary data. There is protection from stack underflows (the caller receives the
//   value 0, or the thread terminates in case of the ret opcode), but there is no protection from overflows.
// - The quest flags are a per-character array of 1024 single-bit flags saved with the character data. (On Episode 3,
//   there are 8192 instead.)
// - The quest counters are a per-character array of 16 32-bit values saved with the character data. (On Episode 3,
//   there are 48 instead.)
// - The event flags are an array of 0x100 bytes stored in the system file (not the character file).

using AttackData = BattleParamsIndex::AttackData;
using ResistData = BattleParamsIndex::ResistData;
using MovementData = BattleParamsIndex::MovementData;

static TextEncoding encoding_for_language(Language language) {
  return ((language == Language::JAPANESE) ? TextEncoding::SJIS : TextEncoding::ISO8859);
}

static string escape_string(const string& data, TextEncoding encoding = TextEncoding::UTF8) {
  string decoded;
  try {
    switch (encoding) {
      case TextEncoding::UTF8:
        decoded = data;
        break;
      case TextEncoding::UTF16:
      case TextEncoding::UTF16_ALWAYS_MARKED:
        decoded = tt_utf16_to_utf8(data);
        break;
      case TextEncoding::SJIS:
        decoded = tt_sega_sjis_to_utf8(data);
        break;
      case TextEncoding::ISO8859:
        decoded = tt_8859_to_utf8(data);
        break;
      case TextEncoding::ASCII:
        decoded = tt_ascii_to_utf8(data);
        break;
      default:
        return phosg::format_data_string(data);
    }
  } catch (const runtime_error&) {
    return phosg::format_data_string(data);
  }

  string ret = "\"";
  for (char ch : decoded) {
    if (ch == '\n') {
      ret += "\\n";
    } else if (ch == '\r') {
      ret += "\\r";
    } else if (ch == '\t') {
      ret += "\\t";
    } else if (static_cast<uint8_t>(ch) < 0x20) {
      ret += std::format("\\x{:02X}", ch);
    } else if (ch == '\'') {
      ret += "\\\'";
    } else if (ch == '\"') {
      ret += "\\\"";
    } else if (ch == '\\') {
      ret += "\\\\";
    } else {
      ret += ch;
    }
  }
  ret += "\"";
  return ret;
}

static string format_and_indent_data(const void* data, size_t size, uint64_t start_address) {
  struct iovec iov;
  iov.iov_base = const_cast<void*>(data);
  iov.iov_len = size;

  string ret = "  ";
  phosg::format_data(
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
      &iov, 1, start_address, nullptr, 0, phosg::PrintDataFlags::PRINT_ASCII);

  phosg::strip_trailing_whitespace(ret);
  return ret;
}

struct QuestScriptOpcodeDefinition {
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
    const char* name;

    Argument(Type type, size_t count = 0, const char* name = nullptr)
        : type(type), count(count), data_type(DataType::NONE), name(name) {}
    Argument(Type type, DataType data_type, const char* name = nullptr)
        : type(type), count(0), data_type(data_type), name(name) {}
  };

  uint16_t opcode;
  const char* name;
  const char* qedit_name;
  std::vector<Argument> args;
  uint32_t flags;

  QuestScriptOpcodeDefinition(
      uint16_t opcode, const char* name, const char* qedit_name, std::vector<Argument> args, uint32_t flags)
      : opcode(opcode), name(name), qedit_name(qedit_name), args(args), flags(flags) {}

  std::string str() const {
    string name_str = this->qedit_name ? std::format("{} (qedit: {})", this->name, this->qedit_name) : this->name;
    return std::format("{:04X}: {} flags={:04X}", this->opcode, name_str, this->flags);
  }
};

constexpr uint32_t v_flag(Version v) {
  return (1 << static_cast<uint32_t>(v));
}

using Arg = QuestScriptOpcodeDefinition::Argument;

static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the QuestScript flags and opcode definitions table");

static constexpr uint32_t F_PUSH_ARG = 0x00010000;
static constexpr uint32_t F_CLEAR_ARGS = 0x00020000;
// F_ARGS means this opcode uses the argument list on v3 and later; it has no effect on v2 and earlier
static constexpr uint32_t F_ARGS = 0x00040000;
static constexpr uint32_t F_TERMINATOR = 0x00080000;
// The following flags are used to specify which versions support each opcode
static constexpr uint32_t F_DC_NTE = 0x00000004; // Version::DC_NTE
static constexpr uint32_t F_DC_112000 = 0x00000008; // Version::DC_11_2000
static constexpr uint32_t F_DC_V1 = 0x00000010; // Version::DC_V1
static constexpr uint32_t F_DC_V2 = 0x00000020; // Version::DC_V2
static constexpr uint32_t F_PC_NTE = 0x00000040; // Version::PC_NTE
static constexpr uint32_t F_PC_V2 = 0x00000080; // Version::PC_V2
static constexpr uint32_t F_GC_NTE = 0x00000100; // Version::GC_NTE
static constexpr uint32_t F_GC_V3 = 0x00000200; // Version::GC_V3
static constexpr uint32_t F_GC_EP3TE = 0x00000400; // Version::GC_EP3_NTE
static constexpr uint32_t F_GC_EP3 = 0x00000800; // Version::GC_EP3
static constexpr uint32_t F_XB_V3 = 0x00001000; // Version::XB_V3
static constexpr uint32_t F_BB_V4 = 0x00002000; // Version::BB_V4

static_assert(F_DC_NTE == v_flag(Version::DC_NTE));
static_assert(F_DC_112000 == v_flag(Version::DC_11_2000));
static_assert(F_DC_V1 == v_flag(Version::DC_V1));
static_assert(F_DC_V2 == v_flag(Version::DC_V2));
static_assert(F_PC_NTE == v_flag(Version::PC_NTE));
static_assert(F_PC_V2 == v_flag(Version::PC_V2));
static_assert(F_GC_NTE == v_flag(Version::GC_NTE));
static_assert(F_GC_V3 == v_flag(Version::GC_V3));
static_assert(F_GC_EP3TE == v_flag(Version::GC_EP3_NTE));
static_assert(F_GC_EP3 == v_flag(Version::GC_EP3));
static_assert(F_XB_V3 == v_flag(Version::XB_V3));
static_assert(F_BB_V4 == v_flag(Version::BB_V4));

// clang-format off
// These are shortcuts for common version ranges in the definitions below.
static constexpr uint32_t F_V0_V2  = F_DC_NTE | F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint32_t F_V0_V4  = F_DC_NTE | F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint32_t F_V05_V2 =            F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint32_t F_V05_V4 =            F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint32_t F_V1_V2  =                          F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint32_t F_V1_V4  =                          F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint32_t F_V2     =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint32_t F_V2_V3  =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3;
static constexpr uint32_t F_V2_V4  =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint32_t F_V3     =                                                                              F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3;
static constexpr uint32_t F_V3_V4  =                                                                              F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint32_t F_V4     =                                                                                                                          F_BB_V4;
// clang-format on
static constexpr uint32_t F_HAS_ARGS = F_V3_V4;

// These are the argument data types. All values are stored little-endian in the script data, even on the GameCube.

// LABEL16 is a 16-bit index into the function table
static constexpr auto LABEL16 = Arg::Type::LABEL16;
// LABEL16_SET is a single byte specifying how many labels follow, followed by that many 16-bit indexes into the
// function table.
static constexpr auto LABEL16_SET = Arg::Type::LABEL16_SET;
// LABEL32 is a 32-bit index into the function table
static constexpr auto LABEL32 = Arg::Type::LABEL32;
// R_REG is a single byte specifying a register number (rXX or fXX) which is read by the opcode and not modified
static constexpr auto R_REG = Arg::Type::R_REG;
// W_REG is a single byte specifying a register number (rXX or fXX) which is written by the opcode (and maybe also read
// beforehand)
static constexpr auto W_REG = Arg::Type::W_REG;
// R_REG_SET is a single byte specifying how many registers follow, followed by that many bytes specifying individual
// register numbers.
static constexpr auto R_REG_SET = Arg::Type::R_REG_SET;
// R_REG_SET_FIXED is a single byte specifying a register number, but the opcode implicitly reads the following
// registers as well. For example, if an opcode takes a {REG_SET_FIXED, 4} and the value 100 was passed to that opcode,
// only the byte 0x64 would appear in the script data, but the opcode would use r100, r101, r102, and r103.
static constexpr auto R_REG_SET_FIXED = Arg::Type::R_REG_SET_FIXED;
// W_REG_SET_FIXED is like R_REG_SET_FIXED, but is used for registers that are written (and maybe read beforehand) by
// the opcode.
static constexpr auto W_REG_SET_FIXED = Arg::Type::W_REG_SET_FIXED;
// [R/W]_REG32 is a 32-bit register number. The high 24 bits are unused.
static constexpr auto R_REG32 = Arg::Type::R_REG32;
static constexpr auto W_REG32 = Arg::Type::W_REG32;
// [RW]_REG32_SET_FIXED is like [RW]_REG_SET_FIXED, but uses a 32-bit register number. The high 24 bits are unused.
static constexpr auto R_REG32_SET_FIXED = Arg::Type::R_REG32_SET_FIXED;
static constexpr auto W_REG32_SET_FIXED = Arg::Type::W_REG32_SET_FIXED;
// I8, I16, and I32 are unsigned integers of various sizes
static constexpr auto I8 = Arg::Type::I8;
static constexpr auto I16 = Arg::Type::I16;
static constexpr auto I32 = Arg::Type::I32;
// FLOAT32 is a standard 32-bit float
static constexpr auto FLOAT32 = Arg::Type::FLOAT32;
// CSTRING is a sequence of nonzero bytes ending with a zero byte
static constexpr auto CSTRING = Arg::Type::CSTRING;

// These are shortcuts for the above types with some extra metadata, which the disassembler uses to annotate arguments
// or data sections.
static const Arg SCRIPT16(LABEL16, Arg::DataType::SCRIPT);
static const Arg SCRIPT16_SET(LABEL16_SET, Arg::DataType::SCRIPT);
static const Arg SCRIPT32(LABEL32, Arg::DataType::SCRIPT);
static const Arg DATA16(LABEL16, Arg::DataType::DATA);
static const Arg CSTRING_LABEL16(LABEL16, Arg::DataType::CSTRING);

static const Arg CLIENT_ID(I32, 0, "client_id");
static const Arg ITEM_ID(I32, 0, "item_id");
static const Arg FLOOR(I32, 0, "floor");

static const QuestScriptOpcodeDefinition opcode_defs[] = {
    // The quest opcodes are defined below. Two-byte opcodes begin with F8 or F9; all other opcodes are one byte.
    // Unlike network commands and subcommands, all versions use the same values for almost all opcodes (there is one
    // exception), but not all opcodes are supported on all versions. The flags denote which versions support each
    // opcode; opcodes are defined multiple times below if their call signatures are different across versions.

    // In the comments below, arguments are referred to with letters. The first argument to an opcode would be regA (if
    // it's a W_REG or R_REG), the second is regB, etc. The individual registers within a REG_SET_FIXED argument are
    // referred to as an array, as regsA[0], regsA[1], etc.

    // Does nothing
    {0x00, "nop", nullptr, {}, F_V0_V4},

    // Pops new PC off stack
    {0x01, "ret", nullptr, {}, F_V0_V4 | F_TERMINATOR},

    // Stops execution for the current frame. Execution resumes immediately after this opcode on the next frame.
    {0x02, "sync", nullptr, {}, F_V0_V4},

    // Exits entirely
    {0x03, "exit", nullptr, {I32}, F_V0_V4 | F_TERMINATOR},

    // Starts a new thread at labelA
    {0x04, "thread", nullptr, {SCRIPT16}, F_V0_V4},

    // Pushes r1-r7 to the stack
    {0x05, "va_start", nullptr, {}, F_V3_V4 | F_CLEAR_ARGS},

    // Pops r7-r1 from the stack
    {0x06, "va_end", nullptr, {}, F_V3_V4},

    // Replaces r1-r7 with the args list, then calls labelA. This opcode doesn't directly clear the args list, but we
    // assume during disassembly that the code being called does so.
    {0x07, "va_call", nullptr, {SCRIPT16}, F_V3_V4 | F_CLEAR_ARGS},

    // Copies a value from regB to regA
    {0x08, "let", nullptr, {W_REG, R_REG}, F_V0_V4},

    // Sets regA to valueB
    {0x09, "leti", nullptr, {W_REG, I32}, F_V0_V4},

    // Sets regA to the memory address of regB. Note that this opcode was moved to 0C in v3 and later.
    {0x0A, "leta", nullptr, {W_REG, R_REG}, F_V0_V2},

    // Sets regA to valueB
    {0x0A, "letb", nullptr, {W_REG, I8}, F_V3_V4},

    // Sets regA to valueB
    {0x0B, "letw", nullptr, {W_REG, I16}, F_V3_V4},

    // Sets regA to the memory address of regB
    {0x0C, "leta", nullptr, {W_REG, R_REG}, F_V3_V4},

    // Sets regA to the address of the offset of labelB in the function table (to get the offset, use read4 after this)
    {0x0D, "leto", nullptr, {W_REG, LABEL16}, F_V3_V4},

    // Sets regA to 1
    {0x10, "set", nullptr, {W_REG}, F_V0_V4},

    // Sets regA to 0
    {0x11, "clear", nullptr, {W_REG}, F_V0_V4},

    // Sets a regA to 0 if it's nonzero and vice versa
    {0x12, "rev", nullptr, {W_REG}, F_V0_V4},

    // Sets flagA to 1. Sends 6x75.
    {0x13, "gset", nullptr, {I16}, F_V0_V4},

    // Clears flagA to 0. Sends 6x75 on BB, but does not send anything on other versions.
    {0x14, "gclear", nullptr, {I16}, F_V0_V4},

    // Inverts flagA. Like the above two opcodes, sends 6x75 if the flag is set by this opcode. Only BB sends 6x75 if
    // the flag is cleared by this opcode.
    {0x15, "grev", nullptr, {I16}, F_V0_V4},

    // If regB is nonzero, sets flagA; otherwise, clears it
    {0x16, "glet", nullptr, {I16, R_REG}, F_V0_V4},

    // Sets regB to the value of flagA
    {0x17, "gget", nullptr, {I16, R_REG}, F_V0_V4},

    // regA += regB
    {0x18, "add", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA += valueB
    {0x19, "addi", nullptr, {W_REG, I32}, F_V0_V4},

    // regA -= regB
    {0x1A, "sub", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA -= valueB
    {0x1B, "subi", nullptr, {W_REG, I32}, F_V0_V4},

    // regA *= regB
    {0x1C, "mul", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA *= valueB
    {0x1D, "muli", nullptr, {W_REG, I32}, F_V0_V4},

    // regA /= regB
    {0x1E, "div", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA /= valueB
    {0x1F, "divi", nullptr, {W_REG, I32}, F_V0_V4},

    // regA &= regB
    {0x20, "and", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA &= valueB
    {0x21, "andi", nullptr, {W_REG, I32}, F_V0_V4},

    // regA |= regB
    {0x22, "or", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA |= valueB
    {0x23, "ori", nullptr, {W_REG, I32}, F_V0_V4},

    // regA ^= regB
    {0x24, "xor", nullptr, {W_REG, R_REG}, F_V0_V4},

    // regA ^= valueB
    {0x25, "xori", nullptr, {W_REG, I32}, F_V0_V4},

    // regA %= regB
    // Note: This does signed division, so if the value is negative, you might get unexpected results.
    {0x26, "mod", nullptr, {W_REG, R_REG}, F_V3_V4},

    // regA %= valueB
    // Note: Unlike mod, this does unsigned division.
    {0x27, "modi", nullptr, {W_REG, I32}, F_V3_V4},

    // Jumps to labelA
    {0x28, "jmp", nullptr, {SCRIPT16}, F_V0_V4 | F_TERMINATOR},

    // Pushes the script offset immediately after this opcode and jumps to labelA
    // Note: This opcode doesn't directly clear the args list, but we assume during disassembly that the code being
    // called does so.
    {0x29, "call", nullptr, {SCRIPT16}, F_V0_V4 | F_CLEAR_ARGS},

    // If all values in regsB are nonzero, jumps to labelA
    {0x2A, "jmp_on", nullptr, {SCRIPT16, R_REG_SET}, F_V0_V4},

    // If all values in regsB are zero, jumps to labelA
    {0x2B, "jmp_off", nullptr, {SCRIPT16, R_REG_SET}, F_V0_V4},

    // If regA == regB, jumps to labelC
    {0x2C, "jmp_eq", "jmp_=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA == valueB, jumps to labelC
    {0x2D, "jmpi_eq", "jmpi_=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA != regB, jumps to labelC
    {0x2E, "jmp_ne", "jmp_!=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA != valueB, jumps to labelC
    {0x2F, "jmpi_ne", "jmpi_!=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA > regB (unsigned), jumps to labelC
    {0x30, "ujmp_gt", "ujmp_>", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA > valueB (unsigned), jumps to labelC
    {0x31, "ujmpi_gt", "ujmpi_>", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA > regB (signed), jumps to labelC
    {0x32, "jmp_gt", "jmp_>", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA > valueB (signed), jumps to labelC
    {0x33, "jmpi_gt", "jmpi_>", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA < regB (unsigned), jumps to labelC
    {0x34, "ujmp_lt", "ujmp_<", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA < valueB (unsigned), jumps to labelC
    {0x35, "ujmpi_lt", "ujmpi_<", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA < regB (signed), jumps to labelC
    {0x36, "jmp_lt", "jmp_<", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA < valueB (signed), jumps to labelC
    {0x37, "jmpi_lt", "jmpi_<", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA >= regB (unsigned), jumps to labelC
    {0x38, "ujmp_ge", "ujmp_>=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA >= valueB (unsigned), jumps to labelC
    {0x39, "ujmpi_ge", "ujmpi_>=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA >= regB (signed), jumps to labelC
    {0x3A, "jmp_ge", "jmp_>=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA >= valueB (signed), jumps to labelC
    {0x3B, "jmpi_ge", "jmpi_>=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA <= regB (unsigned), jumps to labelC
    {0x3C, "ujmp_le", "ujmp_<=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA <= valueB (unsigned), jumps to labelC
    {0x3D, "ujmpi_le", "ujmpi_<=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // If regA <= regB (signed), jumps to labelC
    {0x3E, "jmp_le", "jmp_<=", {R_REG, R_REG, SCRIPT16}, F_V0_V4},

    // If regA <= valueB (signed), jumps to labelC
    {0x3F, "jmpi_le", "jmpi_<=", {R_REG, I32, SCRIPT16}, F_V0_V4},

    // Jumps to labelsB[regA]; if regA is out of range of labelsB, does nothing
    {0x40, "switch_jmp", nullptr, {R_REG, SCRIPT16_SET}, F_V0_V4},

    // Calls labelsB[regA]; if regA is out of range of labelsB, does nothing
    // Note: This opcode doesn't directly clear the args list, but we assume during disassembly that the code being
    // called does so.
    {0x41, "switch_call", nullptr, {R_REG, SCRIPT16_SET}, F_V0_V4 | F_CLEAR_ARGS},

    // Does nothing
    {0x42, "nop_42", nullptr, {I32}, F_V0_V2},

    // Pushes the value in regA to the stack
    {0x42, "stack_push", nullptr, {R_REG}, F_V3_V4},

    // Pops a value from the stack and puts it into regA
    {0x43, "stack_pop", nullptr, {W_REG}, F_V3_V4},

    // Pushes (valueB) regs in increasing order starting at regA
    {0x44, "stack_pushm", nullptr, {R_REG, I32}, F_V3_V4},

    // Pops (valueB) regs in decreasing order ending at regA
    {0x45, "stack_popm", nullptr, {W_REG, I32}, F_V3_V4},

    // Appends regA to the args list
    {0x48, "arg_pushr", nullptr, {R_REG}, F_V3_V4 | F_PUSH_ARG},

    // Appends valueA to the args list
    {0x49, "arg_pushl", nullptr, {I32}, F_V3_V4 | F_PUSH_ARG},
    {0x4A, "arg_pushb", nullptr, {I8}, F_V3_V4 | F_PUSH_ARG},
    {0x4B, "arg_pushw", nullptr, {I16}, F_V3_V4 | F_PUSH_ARG},

    // Appends the memory address of regA to the args list
    {0x4C, "arg_pusha", nullptr, {R_REG}, F_V3_V4 | F_PUSH_ARG},

    // Appends the script offset of labelA to the args list
    {0x4D, "arg_pusho", nullptr, {LABEL16}, F_V3_V4 | F_PUSH_ARG},

    // Appends strA to the args list
    {0x4E, "arg_pushs", nullptr, {CSTRING}, F_V3_V4 | F_PUSH_ARG},

    // Creates dialogue with an object/NPC (valueA) starting with message strB
    {0x50, "message", nullptr, {I32, CSTRING}, F_V0_V4 | F_ARGS},

    // Prompts the player with a list of choices (strB; items separated by newlines) and returns the index of their
    // choice in regA
    {0x51, "list", nullptr, {W_REG, CSTRING}, F_V0_V4 | F_ARGS},

    // Fades from black
    {0x52, "fadein", nullptr, {}, F_V0_V4},

    // Fades to black
    {0x53, "fadeout", nullptr, {}, F_V0_V4},

    // Plays a sound effect
    {0x54, "sound_effect", "se", {I32}, F_V0_V4 | F_ARGS},

    // Plays a fanfare (clear.adx if valueA is 0, or miniclear.adx if it's 1). There is no bounds check on this; values
    // other than 0 or 1 will result in undefined behavior.
    {0x55, "bgm", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Does nothing
    {0x56, "nop_56", nullptr, {}, F_V0_V2},
    {0x57, "nop_57", nullptr, {}, F_V0_V2},
    {0x58, "nop_58", "enable", {I32}, F_V0_V2},
    {0x59, "nop_59", "disable", {I32}, F_V0_V2},

    // Displays a message. Special tokens are interpolated within the string; these special tokens are:
    //   <rXX> => value of rXX as %d (signed integer)
    //   <fXX> => value of rXX as %f (floating-point) (v3 and later)
    //   <color X> => changes text color like $CX would (supported on 11/2000 and later); X must be numeric and in the
    //     range 0-7, so <color 8>, <color 9>, and <color G> do not work (though \tC8, \tC9, and \tCG can be used
    //     directly in the text, and do work)
    //   <cr> => newline
    //   <hero name> or <name hero> => character's name
    //   <hero job> or <name job> => character's class
    //   <time> => always "01:12" (seems like an oversight that was never fixed)
    //   <award item> => name of the chosen challenge mode reward (v2 and later)
    //   <challenge title> => character's challenge rank text (v2 and later)
    //   <pl_name> => name of character selected with get_pl_name (v2 and later)
    //   <pl_job> => class of character selected with get_pl_job (v2 and later)
    //   <last_word> => challenge mode grave message (v2 and later)
    //   <team_name> => name of the game (set by 8A command) (v2 and later)
    //   <last_chat> => last chat message (v2 and later)
    //   <meseta_slot_prize> => the description of the last item sent by the server in a 6xE3 command (BB only)
    {0x5A, "window_msg", nullptr, {CSTRING}, F_V0_V4 | F_ARGS},

    // Adds a message to an existing message (or window_msg). Tokens are interpolated as for window_msg.
    {0x5B, "add_msg", nullptr, {CSTRING}, F_V0_V4 | F_ARGS},

    // Closes the current message box
    {0x5C, "message_end", "mesend", {}, F_V0_V4},

    // Gets the current time, in seconds since 00:00:00 on 1 January 2000
    {0x5D, "gettime", nullptr, {W_REG}, F_V0_V4},

    // Closes a window_msg
    {0x5E, "window_msg_end", "winend", {}, F_V0_V4},

    // Creates an NPC as client ID 1. Sends 6x69 with command = 0.
    //   valueA = initial state (see npc_crp below)
    //   valueB = template index (see 6x69 in CommandFormats.hh)
    {0x60, "npc_crt", "npc_crt_V1", {I32, I32}, F_V0_V2 | F_ARGS},
    {0x60, "npc_crt", "npc_crt_V3", {I32, I32}, F_V3_V4 | F_ARGS},

    // Tells an NPC (by client ID) to stop following
    {0x61, "npc_stop", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Tells an NPC (by client ID) to follow the player
    {0x62, "npc_play", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Destroys an NPC (by client ID)
    {0x63, "npc_kill", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Disables or enables the ability to talk to NPCs
    {0x64, "npc_talk_off", "npc_nont", {}, F_V0_V4},
    {0x65, "npc_talk_on", "npc_talk", {}, F_V0_V4},

    // Creates an NPC as client ID 1. Sends 6x69 with command = 0.
    //   regsA[0-2] = position (x, y, z as integers)
    //   regsA[3] = angle
    //   regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box, according to qedit.info)
    //   regsA[5] = template index (see 6x69 in CommandFormats.hh)
    //   valueB is required in pre-v3 but is ignored
    {0x66, "npc_crp", "npc_crp_V1", {{R_REG_SET_FIXED, 6}, I32}, F_V0_V2},
    {0x66, "npc_crp", "npc_crp_V3", {{R_REG_SET_FIXED, 6}}, F_V3_V4},

    // Creates a pipe. valueA is client ID
    {0x68, "create_pipe", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Checks player HP, but not in a straightforward manner: sets regA to 1 if (current_hp / max_hp) < (1 / valueA)
    // for the client specified by valueB. Sets regA to 0 otherwise.
    {0x69, "p_hpstat", "p_hpstat_V1", {W_REG, CLIENT_ID}, F_V0_V2 | F_ARGS},
    {0x69, "p_hpstat", "p_hpstat_V3", {W_REG, CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Sets regA to 1 if player in slot (valueB) is dead, or 0 if alive.
    {0x6A, "p_dead", "p_dead_V1", {W_REG, CLIENT_ID}, F_V0_V2 | F_ARGS},
    {0x6A, "p_dead", "p_dead_V3", {W_REG, CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Disables/enables telepipes/Ryuker
    {0x6B, "p_disablewarp", nullptr, {}, F_V0_V4},
    {0x6C, "p_enablewarp", nullptr, {}, F_V0_V4},

    // Moves a player.
    //   regsA[0-2] = destination (x, y, z as integers)
    //   regsA[3] = angle
    //   regsA[4] = client ID
    //   valueB is required in pre-v3 but is ignored
    {0x6D, "p_move", "p_move_v1", {{R_REG_SET_FIXED, 5}, I32}, F_V0_V2},
    {0x6D, "p_move", "p_move_V3", {{R_REG_SET_FIXED, 5}}, F_V3_V4},

    // Causes the player with client ID valueA to look at an unspecified other player. The specified player looks at
    // the player with the lowest client ID (except for the specified player).
    // TODO: TObjPlayer::state is involved in determining which player to look at; figure out exactly what this does
    {0x6E, "p_look", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Disables/enables attacks for all players
    {0x70, "p_action_disable", nullptr, {}, F_V0_V4},
    {0x71, "p_action_enable", nullptr, {}, F_V0_V4},

    // Disables/enables movement for the given player
    {0x72, "disable_movement1", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},
    {0x73, "enable_movement1", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // These appear to do nothing at all. On v3, they set and clear a flag that is never read. On DC NTE and v1, code
    // exists to read this flag, but it belongs to an object that appears to never be constructed anywhere.
    {0x74, "clear_unused_flag_74", "p_noncol", {}, F_V0_V4},
    {0x75, "set_unused_flag_75", "p_col", {}, F_V0_V4},

    // Sets a player's starting position.
    //   valueA = client ID
    //   regsB[0-2] = position (x, y, z as integers)
    // r  egsB[3] = angle
    {0x76, "set_player_start_position", "p_setpos", {CLIENT_ID, {R_REG_SET_FIXED, 4}}, F_V0_V4 | F_ARGS},

    // Returns players to the Hunter's Guild counter.
    {0x77, "p_return_guild", nullptr, {}, F_V0_V4},

    // Opens the Hunter's Guild counter menu. valueA should be the player's client ID.
    {0x78, "p_talk_guild", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Creates an NPC which only appears near a given location
    //   regsA[0-2] = position (x, y, z as integers)
    //   regsA[3] = visibility radius
    //   regsA[4] = angle
    //   regsA[5] = template index (see 6x69 in CommandFormats.hh)
    //   regsA[6] = initial state (0 = alive, 1 = dead, 2 = invisible text box, according to qedit.info)
    //   regsA[7] = client ID
    {0x79, "npc_talk_pl", "npc_talk_pl_V1", {{R_REG32_SET_FIXED, 8}}, F_V0_V2},
    {0x79, "npc_talk_pl", "npc_talk_pl_V3", {{R_REG_SET_FIXED, 8}}, F_V3_V4},

    // Destroys an NPC created with npc_talk_pl. This opcode cannot be executed multiple times on the same frame; if it
    // is, only the last one will take effect.
    {0x7A, "npc_talk_kill", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Creates attacker NPC
    {0x7B, "npc_crtpk", "npc_crtpk_V1", {I32, I32}, F_V0_V2 | F_ARGS},
    {0x7B, "npc_crtpk", "npc_crtpk_V3", {I32, I32}, F_V3_V4 | F_ARGS},

    // Creates attacker NPC
    {0x7C, "npc_crppk", "npc_crppk_V1", {{R_REG32_SET_FIXED, 7}, I32}, F_V0_V2},
    {0x7C, "npc_crppk", "npc_crppk_V3", {{R_REG_SET_FIXED, 7}}, F_V3_V4},

    // Creates an NPC with client ID 1. It is not recommended to use this opcode if a player can be in that slot; use
    // npc_crptalk_id instead.
    //   regsA[0-2] = position (x, y, z as integers)
    //   regsA[3] = angle
    //   regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box, according to qedit.info)
    //   regsA[5] = template index (see 6x69 in CommandFormats.hh)
    {0x7D, "npc_crptalk", "npc_crptalk_v1", {{R_REG32_SET_FIXED, 6}, I32}, F_V0_V2},
    {0x7D, "npc_crptalk", "npc_crptalk_V3", {{R_REG_SET_FIXED, 6}}, F_V3_V4},

    // Causes client ID valueA to look at client ID valueB. Sends 6x3E.
    {0x7E, "p_look_at", nullptr, {CLIENT_ID, CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Creates an NPC.
    //   regsA[0-2] = position (x, y, z as integers)
    //   regsA[3] = angle
    //   regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box, according to qedit.info)
    //   regsA[5] = client ID
    //   regsA[6] = template index (see 6x69 in CommandFormats.hh)
    {0x7F, "npc_crp_id", "npc_crp_id_V1", {{R_REG32_SET_FIXED, 7}, I32}, F_V0_V2},
    {0x7F, "npc_crp_id", "npc_crp_id_v3", {{R_REG_SET_FIXED, 7}}, F_V3_V4},

    // Causes the camera to shake.
    {0x80, "cam_quake", nullptr, {}, F_V0_V4},

    // Moves the camera to where your character is looking.
    {0x81, "cam_adj", nullptr, {}, F_V0_V4},

    // Zooms the camera in or out.
    {0x82, "cam_zmin", nullptr, {}, F_V0_V4},
    {0x83, "cam_zmout", nullptr, {}, F_V0_V4},

    // Pans the camera.
    //   regsA[0-2] = destination (x, y, z as integers)
    //   regsA[3] = pan time (in frames; 30 frames/sec)
    //   regsA[4] = end time (in frames; 30 frames/sec)
    {0x84, "cam_pan", "cam_pan_V1", {{R_REG32_SET_FIXED, 5}, I32}, F_V0_V2},
    {0x84, "cam_pan", "cam_pan_V3", {{R_REG_SET_FIXED, 5}}, F_V3_V4},

    // Temporarily sets the game's difficulty to Very Hard (even on v2). On v3 and later, does nothing.
    {0x85, "game_lev_super", nullptr, {}, F_V0_V2},
    {0x85, "nop_85", nullptr, {}, F_V3_V4},

    // Restores the previous difficulty level after game_lev_super. On v3 and later, does nothing.
    {0x86, "game_lev_reset", nullptr, {}, F_V0_V2},
    {0x86, "nop_86", nullptr, {}, F_V3_V4},

    // Creates a telepipe. The telepipe disappears upon being used.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = owner client ID (player or NPC must exist in the game)
    {0x87, "pos_pipe", "pos_pipe_V1", {{R_REG32_SET_FIXED, 4}, I32}, F_V0_V2},
    {0x87, "pos_pipe", "pos_pipe_V3", {{R_REG_SET_FIXED, 4}}, F_V3_V4},

    // Checks if all set events (enemies) have been destroyed in a given room.
    //   regA = result (0 = not cleared, 1 = cleared)
    //   regsB[0] = floor number
    //   regsB[1] = room ID
    {0x88, "if_zone_clear", nullptr, {W_REG, {R_REG_SET_FIXED, 2}}, F_V0_V4},

    // Returns the number of enemies destroyed so far in this game (since the quest began).
    {0x89, "chk_ene_num", nullptr, {W_REG}, F_V0_V4},

    // Constructs all objects or enemies that match the conditions:
    //   regsA[0] = floor
    //   regsA[1] = room ID
    //   regsA[2] = group
    {0x8A, "construct_delayed_object", "unhide_obj", {{R_REG_SET_FIXED, 3}}, F_V0_V4},
    {0x8B, "construct_delayed_enemy", "unhide_ene", {{R_REG_SET_FIXED, 3}}, F_V0_V4},

    // Starts a new thread when the player is close enough to the given point. The collision is created on the current
    // floor; the thread is created when the player enters the given radius.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = radius
    //   regsA[4] = label index where thread should start
    {0x8C, "at_coords_call", nullptr, {{R_REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but the thread is not started automatically. Instead, the player's primary action button
    // becomes "talk" within the radius, and the label is called when the player presses that button.
    {0x8D, "at_coords_talk", nullptr, {{R_REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but only triggers if an NPC enters the radius.
    {0x8E, "npc_coords_call", "walk_to_coord_call", {{R_REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but triggers when a player within the event radius is also within the player radius of any
    // other player.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = event radius (centered at x, y, z defined above)
    //   regsA[4] = player radius (centered at player)
    //   regsA[5] = label index where thread should start
    {0x8F, "party_coords_call", "col_npcinr", {{R_REG_SET_FIXED, 6}}, F_V0_V4},

    // Enables/disables a switch flag (valueA). Does NOT send 6x05, so other players will not know about this change!
    // Use sw_send instead to keep switch flag state synced.
    {0x90, "switch_on", nullptr, {I32}, F_V0_V4 | F_ARGS},
    {0x91, "switch_off", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Plays a BGM. Values for valueA:
    //    1: epi1.adx           2: epi2.adx           3: ED_PIANO.adx       4: matter.adx         5: open.adx
    //    6: dreams.adx         7: mambo.adx          8: carnaval.adx       9: hearts.adx        10: smile.adx
    //   11: nomal.adx         12: chu_f.adx         13: ENDING_LOOP.adx   14: DreamS_KIDS.adx   15: ESCAPE.adx
    //   16: LIVE.adx          17: MILES.adx
    {0x92, "playbgm_epi", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Enables access to a floor (valueA) via the Pioneer 2 Ragol warp. Floors are 0-17 for Episode 1, 18-35 for
    // Episode 2, 36-46 for Episode 4.
    {0x93, "set_mainwarp", nullptr, {I32}, F_V0_V4 | F_ARGS},

    // Creates an object that the player can talk to. A reticle appears when the player is nearby.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = target radius
    //   regsA[4] = label number to call
    //   regsA[5] = distance from ground to target
    //   regB = returned object token (can be used with del_obj_param)
    {0x94, "set_obj_param", nullptr, {{R_REG_SET_FIXED, 6}, W_REG}, F_V0_V4},

    // Causes the labelB to be called on a new thread when the player warps to floorA. If the given floor already has a
    // registered handler, it is replaced with the new one (each floor may have at most one handler).
    {0x95, "set_floor_handler", nullptr, {FLOOR, SCRIPT32}, F_V0_V2},
    {0x95, "set_floor_handler", nullptr, {FLOOR, SCRIPT16}, F_V3_V4 | F_ARGS},

    // Deletes the handler for floorA.
    {0x96, "clr_floor_handler", nullptr, {FLOOR}, F_V0_V4 | F_ARGS},

    // Creates a collision object that checks if the NPC with client ID 1 is too far away from the player, when the
    // player enters its check radius.
    //   regsA[0-2] = check location (x, y, z as integers)
    //   regsA[3] = check radius
    //   regsA[4] = label index for triggered function
    //   regsA[5] = player radius (if NPC is closer, label is not called)
    //   regsA[6-8] = warp location (x, y, z as integers) for use with npc_chkwarp within the triggered function
    {0x97, "check_npc_straggle", "col_plinaw", {{R_REG_SET_FIXED, 9}}, F_V1_V4},

    // Hides or shows the HUD.
    {0x98, "hud_hide", nullptr, {}, F_V0_V4},
    {0x99, "hud_show", nullptr, {}, F_V0_V4},

    // Enables/disables the cinema effect (black bars above/below screen)
    {0x9A, "cine_enable", nullptr, {}, F_V0_V4},
    {0x9B, "cine_disable", nullptr, {}, F_V0_V4},

    // Unused opcode. It's not clear what this was supposed to do. The behavior appears to be the same on all versions
    // of PSO, from DC NTE through BB. argA is ignored. The game constructs a message list object from strB; the game
    // will softlock unless this string contains exactly 2 messages (separated by \n). After doing this, it destroys
    // the message list and does nothing else.
    {0xA0, "nop_A0_debug", "broken_list", {I32, CSTRING}, F_V0_V4 | F_ARGS},

    // Sets a function to be called (on a new thread) when the quest is failed. The quest is considered failed when you
    // talk to the Hunter's Guild counter and r253 has the value 1 (specifically 1; other nonzero values do not trigger
    // this function).
    {0xA1, "set_qt_failure", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA1, "set_qt_failure", nullptr, {SCRIPT16}, F_V3_V4},

    // Like set_qt_failure, but uses r255 instead. If r255 and r253 both have the value 1 when the player talks to the
    // Hunter's Guild counter, the success label is called and the failure label is not called.
    {0xA2, "set_qt_success", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA2, "set_qt_success", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest failure handler (opposite of set_qt_failure).
    {0xA3, "clr_qt_failure", nullptr, {}, F_V0_V4},

    // Clears the quest success handler (opposite of set_qt_success).
    {0xA4, "clr_qt_success", nullptr, {}, F_V0_V4},

    // Sets a function to be called when the quest is cancelled via the Hunter's Guild counter.
    {0xA5, "set_qt_cancel", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA5, "set_qt_cancel", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest cancel handler (opposite of set_qt_cancel).
    {0xA6, "clr_qt_cancel", nullptr, {}, F_V0_V4},

    // Makes a player or NPC walk to a location.
    //   regsA[0-2] = location (x, y, z as integers; y is ignored)
    //   regsA[3] = client ID
    {0xA8, "pl_walk", "pl_walk_V1", {{R_REG32_SET_FIXED, 4}, I32}, F_V0_V2},
    {0xA8, "pl_walk", "pl_walk_V3", {{R_REG_SET_FIXED, 4}}, F_V3_V4},

    // Gives valueB Meseta to the player with client ID valueA. Negative values do not appear to be handled properly;
    // if this opcode attempts to take more meseta than the player has, the player ends up with 999999 Meseta.
    {0xB0, "pl_add_meseta", nullptr, {CLIENT_ID, I32}, F_V0_V4 | F_ARGS},

    // Starts a new thread at labelA in the quest script.
    {0xB1, "thread_stg", nullptr, {SCRIPT16}, F_V0_V4},

    // Deletes an interactable object previously created by set_obj_param. valueA is the object's token, as returned by
    // regB from set_obj_param.
    {0xB2, "del_obj_param", nullptr, {R_REG}, F_V0_V4},

    // Creates an item in the player's inventory. If the item is successfully created, this opcode sends 6x2B on all
    // versions except BB. On BB, this opcode sends 6xCA, and the server sends 6xBE to create the item; the requested
    // item must match one of the item creation masks in the quest script's header.
    //   regsA[0-2] = item.data1[0-2]
    //   regB = returned item ID, or FFFFFFFF if item can't be created
    {0xB3, "item_create", nullptr, {{R_REG_SET_FIXED, 3}, W_REG}, F_V0_V4},

    // Like item_create, but regsA specify all of item.data1 instead of only the first 3 bytes.
    {0xB4, "item_create2", nullptr, {{R_REG_SET_FIXED, 12}, W_REG}, F_V0_V4},

    // Deletes an item from the player's inventory. Sends 6x29 if ths item is found and deleted. If the item is
    // stackable, only one of it is deleted; the rest of the stack is not deleted.
    //   regA = item ID
    //   regsB[0-11] = item.data1[0-11] for deleted item
    // regsB must not wrap around (that is, the first register in regsB cannot be in the range [r245, r255]). If the
    // item is not found, regsB[0] is set to 0xFFFFFFFF and the rest of regsB are not affected.
    {0xB5, "item_delete", nullptr, {R_REG, {W_REG_SET_FIXED, 12}}, F_V0_V4},

    // Like item_delete, but searches by item.data1[0-2] instead of by item ID.
    //   regsA[0-2] = item.data1[0-2] to search for
    //   regsB[0-11] = item.data1[0-11] for deleted item
    {0xB6, "item_delete_by_type", "item_delete2", {{R_REG_SET_FIXED, 3}, {W_REG_SET_FIXED, 12}}, F_V0_V4},

    // Searches the player's inventory for an item and returns its item ID.
    //   regsA[0-2] = item.data1[0-2] to search for, as above
    //   regB = found item ID, or FFFFFFFF if not found
    // The condition depends on the item's type:
    //   Weapons: data1[0-2] must match, and data1[4] (special/flags) must be 0
    //   Armor/shield/unit: data1[0-2] must match
    //   Mag: data1[0-1] must match; regsA[2] is ignored
    //   Tool: data1[0-2] must match; if it's a tech disk, data1[4] must be 0
    {0xB7, "find_inventory_item", "item_check", {{R_REG_SET_FIXED, 3}, W_REG}, F_V0_V4},

    // Triggers set event valueA on the current floor. Sends 6x67.
    {0xB8, "setevt", nullptr, {I32}, F_V05_V4 | F_ARGS},

    // Returns the current difficulty level in regA. If game_lev_super has been executed, returns 2. This opcode only
    // returns 0-2, even in Ultimate (which results in 2 as well). All non-v1 quests should use get_difficulty_level_v2
    // instead.
    {0xB9, "get_difficulty_level_v1", "get_difflvl", {W_REG}, F_V05_V4},

    // Sets a label to be called (in a new thread) when the quest exits. This happens during the unload procedure when
    // leaving the game, so most opcodes cannot be used in these handlers. Generally they should only be used for
    // setting quest flags, event flags, or quest counters.
    {0xBA, "set_qt_exit", nullptr, {SCRIPT32}, F_V05_V2},
    {0xBA, "set_qt_exit", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest exit handler (opposite of set_qt_exit).
    {0xBB, "clr_qt_exit", nullptr, {}, F_V05_V4},

    // This opcode does nothing, even on 11/2000.
    {0xBC, "nop_BC", "unknownBC", {CSTRING}, F_V05_V4},

    // Creates a timed particle effect.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = effect type
    //   regsA[4] = duration (in frames; 30 frames/sec)
    {0xC0, "particle", "particle_V1", {{R_REG32_SET_FIXED, 5}, I32}, F_V05_V2},
    {0xC0, "particle", "particle_V3", {{R_REG_SET_FIXED, 5}}, F_V3_V4},

    // Specifies what NPCs should say in various situations. This opcode sets strings for all NPCs; to set strings for
    // only specific NPCs, use npc_text_id (on v3 and later).
    //   valueA = situation number:
    //     00: NPC engaging in combat
    //     01: NPC in danger (HP <= 75% if near player, <= 25% if far away?)
    //     02: NPC casting any technique except those in cases 16 and 17 below
    //     03: NPC has 20% or less TP
    //     04: NPC died
    //     05: NPC has been dead for a while
    //     06: Unknown (possibly unused)
    //     07: NPC lost sight of player
    //     08: NPC locked on to an enemy
    //     09: Player received a status effect
    //     0A: Unknown (possibly unused)
    //     0B: NPC standing still for 3 minutes
    //     0C: Player in danger? (Like 01 but players reversed?)
    //     0D: NPC completed 3-hit combo
    //     0E: NPC hit for > 30% of max HP
    //     0F: NPC hit for > 20% of max HP (and <= 30%)
    //     10: NPC hit for > 1% of max HP (and <= 20%)
    //     11: NPC healed by another player
    //     12: Room cleared (wave cleared which did not trigger another wave)
    //     13: NPC used a recovery item
    //     14: NPC cannot heal / recover
    //     15: Wave but not room cleared (wave triggered by another wave)
    //     16: NPC casting Resta or Anti
    //     17: NPC casting Foie, Zonde, or Barta
    //     18: NPC regained sight of player (not valid on 11/2000)
    //   strB = string for NPC to say (up to 52 characters)
    {0xC1, "npc_text", nullptr, {I32, CSTRING}, F_V05_V4 | F_ARGS},

    // Warps an NPC to a predetermined location. See npc_check_straggle for more details.
    {0xC2, "npc_chkwarp", nullptr, {}, F_V05_V4},

    // Disables PK mode (battle mode) for a specific player. Sends 6x1C.
    {0xC3, "pl_pkoff", nullptr, {}, F_V05_V4},

    // Specifies how objects and enemies should be populated for a floor. On v2, the ability to reassign areas was
    // added, which can be done with map_designate_ex (F80D).
    //   regsA[0] = floor number
    //   regsA[1] = type (0: use layout, 1: use offline template, 2: use online template, 3: nothing)
    //   regsA[2] = major variation (minor variation is set to zero)
    //   regsA[3] = ignored
    {0xC4, "map_designate", nullptr, {{R_REG_SET_FIXED, 4}}, F_V05_V4},

    // Locks (masterkey_on) or unlocks (masterkey_off) all doors
    {0xC5, "masterkey_on", nullptr, {}, F_V05_V4},
    {0xC6, "masterkey_off", nullptr, {}, F_V05_V4},

    // Enables/disables the timer window
    {0xC7, "show_timer", "window_time", {}, F_V05_V4},
    {0xC8, "hide_timer", "winend_time", {}, F_V05_V4},

    // Sets the time displayed in the timer window. The value in regA should be a number of seconds.
    {0xC9, "winset_time", nullptr, {R_REG}, F_V05_V4},

    // Returns the time from the system clock. The value returned in regA depends on the client's architecture:
    // - On DC, reads from hardware registers
    // - On GC, reads from the TBRs
    // - On PCv2, XB, and BB, calls QueryPerformanceCounter
    {0xCA, "getmtime", nullptr, {W_REG}, F_V05_V4},

    // Creates an item in the quest board.
    //   valueA = index of item (0-5)
    //   labelB = label to call when selected
    //   strC = name of item in the Quest Board window
    // Executing this opcode is not enough for the item to appear! The item only appears if its corresponding display
    // register is also set to 1. The display registers are r74-r79 (for QB indexes 0-5, respectively).
    {0xCB, "set_quest_board_handler", nullptr, {I32, SCRIPT32, CSTRING}, F_V05_V2},
    {0xCB, "set_quest_board_handler", nullptr, {I32, SCRIPT16, CSTRING}, F_V3_V4 | F_ARGS},

    // Deletes an item by index from the quest board.
    {0xCC, "clear_quest_board_handler", nullptr, {I32}, F_V05_V4 | F_ARGS},

    // Creates a particle effect on a given entity.
    //   regsA[0] = effect type
    //   regsA[1] = duration in frames
    //   regsA[2] = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    //   regsA[3] = y offset (as integer)
    //   valueB is required in pre-v3 but is ignored
    {0xCD, "particle_id", "particle_id_V1", {{R_REG32_SET_FIXED, 4}, I32}, F_V05_V2},
    {0xCD, "particle_id", "particle_id_V3", {{R_REG_SET_FIXED, 4}}, F_V3_V4},

    // Creates an NPC.
    //   regsA[0-2] = position (x, y, z as integers)
    //   regsA[3] = angle
    //   regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box, according to qedit.info)
    //   regsA[5] = template index (see 6x69 in CommandFormats.hh)
    //   regsA[6] = client ID
    {0xCE, "npc_crptalk_id", "npc_crptalk_id_V1", {{R_REG32_SET_FIXED, 7}, I32}, F_V05_V2},
    {0xCE, "npc_crptalk_id", "npc_crptalk_id_V3", {{R_REG_SET_FIXED, 7}}, F_V3_V4},

    // Deletes all strings registered with npc_text.
    {0xCF, "npc_text_clear_all", "npc_lang_clean", {}, F_V05_V4},

    // Enables PK mode (battle mode) for a specific player. Sends 6x1B.
    {0xD0, "pl_pkon", nullptr, {}, F_V1_V4},

    // Like find_inventory_item, but regsA specifies data1[0-2] as well as data1[4]. The matching conditions are the
    // same as in find_inventory_item except that data1[4] must match regsA[3], instead of zero. Returns the item ID in
    // regB, or FFFFFFFF if not found.
    {0xD1, "find_inventory_item_ex", "pl_chk_item2", {{R_REG_SET_FIXED, 4}, W_REG}, F_V1_V4},

    // Enables/disables the main menu and shortcut menu.
    {0xD2, "enable_mainmenu", nullptr, {}, F_V1_V4},
    {0xD3, "disable_mainmenu", nullptr, {}, F_V1_V4},

    // Enables or disables battle music override. When enabled, the battle segments of the BGM will play regardless of
    // whether there are enemies nearby. Changing this value only takes effect after any currently-queued music
    // segments are done playing. The override is cleared upon changing areas.
    {0xD4, "start_battlebgm", nullptr, {}, F_V1_V4},
    {0xD5, "end_battlebgm", nullptr, {}, F_V1_V4},

    // Shows a message in the Quest Board message window
    {0xD6, "disp_msg_qb", nullptr, {CSTRING}, F_V1_V4 | F_ARGS},

    // Closes the Quest Board message window
    {0xD7, "close_msg_qb", nullptr, {}, F_V1_V4},

    // Writes the valueB (a single byte) to the event flag specified by valueA.
    {0xD8, "set_eventflag", "set_eventflag_v1", {I32, I32}, F_V1_V2 | F_ARGS},
    {0xD8, "set_eventflag", "set_eventflag_v3", {I32, I32}, F_V3_V4 | F_ARGS},

    // Sets regA to valueB, and sends 6x77 so other clients will also set their local regA to valueB.
    {0xD9, "sync_register", "sync_leti", {W_REG32, I32}, F_V1_V2 | F_ARGS},
    {0xD9, "sync_register", "sync_leti", {W_REG, I32}, F_V3_V4 | F_ARGS},

    // TODO: Document these
    {0xDA, "set_returnhunter", nullptr, {}, F_V1_V4},
    {0xDB, "set_returncity", nullptr, {}, F_V1_V4},

    // TODO: Document this
    {0xDC, "load_pvr", nullptr, {}, F_V1_V4},
    // TODO: Document this
    // Does nothing on all non-DC versions.
    {0xDD, "load_midi", nullptr, {}, F_V1_V4},

    // Finds an item in the player's bank, and clears its entry in the bank.
    //   regsA[0-5] = item.data1[0-5] (bank item must exactly match all bytes)
    //   regB = set to 1 if item was found and cleared, 0 if not
    {0xDE, "delete_bank_item", "unknownDE", {{R_REG_SET_FIXED, 6}, W_REG}, F_V1_V4},

    // Sets NPC AI behaviors.
    //   regsA[0] = TODO: this doesn't appear to be used anywhere internally; official quests used it for the NPC ID?
    //   regsA[1] = base level. NPC's actual level depends on difficulty:
    //     Normal: min(base level + 1, 199)
    //     Hard: min(base level + 26, 199)
    //     Very Hard: min(base level + 51, 199)
    //     Ultimate: min(base level + 151, 199)
    //   regsA[2] = sets value of technique_flags in the TNpcPlayer object:
    //     00 = technique_flags=40 (no techniques)
    //     01 = technique_flags=04 (has Resta, Anti, Foie, and Gifoie)
    //     02 = technique_flags=08 (has Resta, Anti, Barta, and Gibarta)
    //     03, 0A = technique_flags=C0 (no techniques, doesn't back off (see regsA[13]))
    //     0B = technique_flags=84 (has Resta, Anti, Foie, and Gifoie, doesn't back off (see regsA[13]))
    //     0C = technique_flags=88 (has Resta, Anti, Barta, and Gibarta, doesn't back off (see regsA[13]))
    //     0D = technique_flags=90 (has Resta, Anti, Zonde, and Razonde, doesn't back off (see regsA[13]))
    //     Anything else = uninitialized value written to technique_flags
    //   regsA[3] = enemy lock-on range
    //   regsA[4] = unknown (TODO)
    //   regsA[5] = max distance from player
    //   regsA[6] = enemy unlock range
    //   regsA[7] = block range
    //   regsA[8] = attack range (not necessarily equal to weapon range)
    //   regsA[9] = attack technique level (which techniques is specified by technique_flags)
    //   regsA[10] = support technique level (Resta/Anti)
    //   regsA[11] = attack probability (in range [0, 100])
    //   regsA[12] = attack technique probability (in range [0, 100])
    //   regsA[13] = unknown (TODO: see TNpcPlayer_FUN_801514c0); appears to be a distance range, and applies only for
    //     NPCs that attack the player and do not have technique_flags & 0x80; could be backoff distance when attacked
    //     by a player
    //   valueB = NPC template to modify (00-3F)
    {0xDF, "npc_param", "npc_param_V1", {{R_REG32_SET_FIXED, 14}, I32}, F_V1_V2},
    {0xDF, "npc_param", "npc_param_V3", {{R_REG_SET_FIXED, 14}, I32}, F_V3_V4 | F_ARGS},

    // TODO(DX): Document this. It enables a flag that affects some logic in TBoss1Dragon::update. The flag is disabled
    // when the Dragon's boss arena unloads, but not when it loads, so it can be set when the player is in a different
    // area. It appears the flag is not cleared if the player never enters the Dragon arena, so it seems the only
    // advisable place to use this would be immediately after the player enters the Dragon arena.
    {0xE0, "pad_dragon", nullptr, {}, F_V1_V4},

    // Disables access to a floor (valueA) via the Pioneer 2 Ragol warp. This is the logical opposite of set_mainwarp.
    {0xE1, "clear_mainwarp", nullptr, {I32}, F_V1_V4 | F_ARGS},

    // Sets camera parameters for the current frame.
    //   regsA[0-2] = relative location of focus point from player
    //   regsA[3-5] = relative location of camera from player
    {0xE2, "pcam_param", "pcam_param_V1", {{R_REG32_SET_FIXED, 6}}, F_V1_V2},
    {0xE2, "pcam_param", "pcam_param_V3", {{R_REG_SET_FIXED, 6}}, F_V3_V4},

    // Triggers set event (valueB) on floor (valueA). Sends 6x67.
    {0xE3, "start_setevt", "start_setevt_v1", {I32, I32}, F_V1_V2 | F_ARGS},
    {0xE3, "start_setevt", "start_setevt_v3", {I32, I32}, F_V3_V4 | F_ARGS},

    // Enables or disables warps
    {0xE4, "warp_on", nullptr, {}, F_V1_V4},
    {0xE5, "warp_off", nullptr, {}, F_V1_V4},

    // Returns the client ID of the local client
    {0xE6, "get_client_id", "get_slotnumber", {W_REG}, F_V1_V4},

    // Returns the client ID of the lobby/game leader
    {0xE7, "get_leader_id", "get_servernumber", {W_REG}, F_V1_V4},

    // Sets an event flag from a register. In v3 and later, this is not needed, since set_eventflag can be called with
    // F_ARGS, but it still exists.
    {0xE8, "set_eventflag2", nullptr, {I32, R_REG}, F_V1_V4 | F_ARGS},

    // regA %= regB
    // This is exactly the same as the mod opcode (including its quirk).
    {0xE9, "mod2", "res", {W_REG, R_REG}, F_V1_V4},

    // regA %= valueB
    // This is exactly the same as the modi opcode (including its quirk).
    {0xEA, "modi2", "unknownEA", {W_REG, I32}, F_V1_V4},

    // Changes the background music. create_bgmctrl must be run before doing this. The values for valueA are the same
    // as for playbgm_epi.
    {0xEB, "set_bgm", "enable_bgmctrl", {I32}, F_V1_V4 | F_ARGS},

    // Changes the state of a switch flag and sends the update to all players (unlike switch_on/switch_off).
    //   regsA[0] = switch flag number
    //   regsA[1] = floor number
    //   regsA[2] = flags (see 6x05 definition in CommandFormats.hh)
    {0xEC, "update_switch_flag", "sw_send", {{R_REG_SET_FIXED, 3}}, F_V1_V4},

    // Creates a BGM controller object. Use this before set_bgm.
    {0xED, "create_bgmctrl", nullptr, {}, F_V1_V4},

    // Like pl_add_meseta, but can only give Meseta to the local player.
    {0xEE, "pl_add_meseta2", nullptr, {I32}, F_V1_V4 | F_ARGS},

    // Like sync_register, but takes the value from another register rather than using an immediate value. On v3 and
    // later, this is identical to sync_register.
    {0xEF, "sync_register2", "sync_let", {W_REG32, R_REG32}, F_V1_V2},
    {0xEF, "sync_register2", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},

    // Same as sync_register2, but sends the value via UDP if UDP is enabled. This opcode was removed after GC NTE and
    // is missing from v3 and v4.
    {0xF0, "sync_register2_udp", "send_regwork", {W_REG32, W_REG32}, F_V1_V2},

    // Sets the camera's location and angle.
    //   regsA[0-2] = camera location (x, y, z as integers)
    //   regsA[3-5] = camera focus location (x, y, z as integers)
    {0xF1, "leti_fixed_camera", "leti_fixed_camera_V1", {{R_REG32_SET_FIXED, 6}}, F_V2},
    {0xF1, "leti_fixed_camera", "leti_fixed_camera_V3", {{R_REG_SET_FIXED, 6}}, F_V3_V4},

    // Resets the camera to non-fixed (default behavior).
    {0xF2, "default_camera_pos1", nullptr, {}, F_V2_V4},

    // Same as 50, but uses fixed arguments - with the string "俺は節政だ！！", which Google Translate translates as "I
    // am frugal!!"
    {0xF800, "debug_F800", nullptr, {}, F_V2},

    // Creates a region that calls a label if a specified string is said (via chat) by a player within the region. This
    // is implemented by the TOChatSensor object; see that object's comments on Map.cc for details.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = radius
    //   regsA[4] = label index
    //   strB = trigger string (up to 31 characters)
    // If the trigger string contains any newlines (\n), it is truncated at the first newline.
    // Before matching, the game prepends and appends a single space to the chat message (presumably so that callers
    // can use strings like " word " to avoid matching substrings of longer words) and transforms the chat message to
    // lowercase, so strB should not contain any uppercase letters. The lowercasing logic (probably accidentally) also
    // affects some symbols, as follows: [ => {    \ => |    ] => }    ^ => ~    _ => <DEL>
    // Curiously, the function that checks for matches appears to be used incorrectly. The function takes an array of
    // char[0x20] and returns the index of the string that matched, or -1 if none matched. This list of match strings
    // is expected to end with an empty string, but TOChatSensor doesn't correctly terminate it this way. However, the
    // following field is the TQuestThread pointer, which is non-null only if the sensor has already triggered, so it
    // doesn't misbehave.
    {0xF801, "set_chat_callback", "set_chat_callback?", {{R_REG32_SET_FIXED, 5}, CSTRING}, F_V2_V4 | F_ARGS},

    // Returns the difficulty level. Unlike get_difficulty_level_v1, this correctly returns 3 in Ultimate.
    {0xF808, "get_difficulty_level_v2", "get_difflvl2", {W_REG}, F_V2_V4},

    // Returns the number of players in the game.
    {0xF809, "get_number_of_players", "get_number_of_player1", {W_REG}, F_V2_V4},

    // Returns the location of the specified player.
    //   regsA[0-2] = returned location (x, y, z as integers)
    //   regB = client ID
    {0xF80A, "get_coord_of_player", nullptr, {{W_REG_SET_FIXED, 3}, R_REG}, F_V2_V4},

    // Enables or disables the area map and minimap.
    {0xF80B, "enable_map", nullptr, {}, F_V2_V4},
    {0xF80C, "disable_map", nullptr, {}, F_V2_V4},

    // Like map_designate, but allows changing the area assignment.
    //   regsA[0] = floor number
    //   regsA[1] = area number
    //   regsA[2] = type (0: use layout, 1: use offline template, 2: use online template, 3: nothing)
    //   regsA[3] = major variation
    //   regsA[4] = minor variation
    {0xF80D, "map_designate_ex", nullptr, {{R_REG_SET_FIXED, 5}}, F_V2_V4},

    // Enables or disables weapon dropping upon death for a player. Sends 6x81 (disable) or 6x82 (enable).
    {0xF80E, "disable_weapon_drop", "unknownF80E", {CLIENT_ID}, F_V2_V4 | F_ARGS},
    {0xF80F, "enable_weapon_drop", "unknownF80F", {CLIENT_ID}, F_V2_V4 | F_ARGS},

    // Sets the floor where the players will start. This generally should be used in the start label (where
    // map_designate, etc. are used). valueA specifies which floor to start on, but indirectly:
    //   valueA = 0: Temple (floor 0x10)
    //   valueA = 1: Spaceship (floor 0x11)
    //   valueA > 1 and < 0x12: Episode 1 areas (Pioneer 2, Forest 1, etc.)
    //   valueA >= 0x12: No effect
    // Note that if the player doesn't start in the city (Pioneer 2 or Lab), the client will not reload the common and
    // rare item tables (ItemPT and ItemRT). This means, for example, that starting an Episode 2 quest from an Episode
    // 1 game will result in Episode 2 enemies' drop-anything rates being all zeroes, since the Episode 1 ItemPT is
    // still loaded. This is only an issue in client drop mode; on newserv, you can forbid client drop mode in the
    // quest's metadata JSON file if needed. (See q058.json for documentation on how to do this.)
    {0xF810, "ba_initial_floor", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Clears all battle rules.
    {0xF811, "clear_ba_rules", "set_ba_rules", {}, F_V2_V4},

    // Sets the tech disk mode in battle. valueA (does NOT match enum):
    //   0 => FORBID_ALL
    //   1 => ALLOW
    //   2 => LIMIT_LEVEL
    {0xF812, "ba_set_tech_disk_mode", "ba_set_tech", {I32}, F_V2_V4 | F_ARGS},

    // Sets the weapon and armor mode in battle. valueA (does NOT match enum):
    //   0 => FORBID_ALL
    //   1 => ALLOW
    //   2 => CLEAR_AND_ALLOW
    //   3 => FORBID_RARES
    {0xF813, "ba_set_weapon_and_armor_mode", "ba_set_equip", {I32}, F_V2_V4 | F_ARGS},

    // Sets the mag mode in battle. valueA (does NOT match enum):
    //   0 => FORBID_ALL
    //   1 => ALLOW
    {0xF814, "ba_set_forbid_mags", "ba_set_mag", {I32}, F_V2_V4 | F_ARGS},

    // Sets the tool mode in battle. valueA (does NOT match enum):
    //   0 => FORBID_ALL
    //   1 => ALLOW
    //   2 => CLEAR_AND_ALLOW
    {0xF815, "ba_set_tool_mode", "ba_set_item", {I32}, F_V2_V4 | F_ARGS},

    // Sets the trap mode in battle. valueA (matches enum):
    //   0 => DEFAULT
    //   1 => ALL_PLAYERS
    {0xF816, "ba_set_trap_mode", "ba_set_trapmenu", {I32}, F_V2_V4 | F_ARGS},

    // This appears to be unused - the value is copied into the main battle rules struct, but the field is never read
    // from there. This may have been an early implementation of F851 that affected all trap types, but this field is
    // no longer used.
    {0xF817, "ba_set_unused_F817", "unknownF817", {I32}, F_V2_V4 | F_ARGS},

    // Sets the respawn mode in battle. valueA (does NOT match enum):
    //   0 => FORBID
    //   1 => ALLOW
    //   2 => LIMIT_LIVES
    {0xF818, "ba_set_respawn", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) character replacement in battle mode
    {0xF819, "ba_set_replace_char", "ba_set_char", {I32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) weapon dropping upon death in battle
    {0xF81A, "ba_dropwep", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) teams in battle
    {0xF81B, "ba_teams", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Shows the rules window and starts the battle. This should be used after setting up all the rules with the
    // various ba_* opcodes.
    {0xF81C, "ba_start", "ba_disp_msg", {CSTRING}, F_V2_V4 | F_ARGS},

    // Sets the number of levels to gain upon respawn in battle
    {0xF81D, "ba_death_lvl_up", "death_lvl_up", {I32}, F_V2_V4 | F_ARGS},

    // Sets the Meseta mode in battle. valueA (matches enum):
    //   0 => ALLOW
    //   1 => FORBID_ALL
    //   2 => CLEAR_AND_ALLOW
    {0xF81E, "ba_set_meseta_drop_mode", "ba_set_meseta", {I32}, F_V2_V4 | F_ARGS},

    // Sets the challenge mode stage number
    {0xF820, "cmode_stage", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // regsA[3-8] specify first 6 bytes of an ItemData. This opcode consumes an item ID, but does nothing else.
    {0xF821, "nop_F821", nullptr, {{R_REG_SET_FIXED, 9}}, F_V2_V4},

    // This opcode does nothing. It has two branches (one for online, one for offline), but both branches do nothing.
    {0xF822, "nop_F822", nullptr, {R_REG}, F_V2_V4},

    // Sets the challenge template index. See Client::create_challenge_overlay for details on how the template is used.
    {0xF823, "set_cmode_char_template", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Sets the game difficulty (0-3) in challenge mode. Does nothing in modes other than challenge.
    {0xF824, "set_cmode_difficulty", "set_cmode_diff", {I32}, F_V2_V4 | F_ARGS},

    // Sets the factor by which all EXP is multiplied in challenge mode.
    // The multiplier value is regsA[0] + (regsA[1] / regsA[2]).
    {0xF825, "exp_multiplication", nullptr, {{R_REG_SET_FIXED, 3}}, F_V2_V4},

    // Checks if any player is still alive in challenge mode. Returns 1 if all players are dead, or 0 if not.
    {0xF826, "cmode_check_all_players_dead", "if_player_alive_cm", {W_REG}, F_V2_V4},

    // Checks if all players are still alive in challenge mode. Returns 1 if any player is dead, or 0 if not.
    {0xF827, "cmode_check_any_player_dead", "get_user_is_dead?", {W_REG}, F_V2_V4},

    // Sends the player with client ID regA to floor regB. Does nothing if regA doesn't refer to the local player.
    {0xF828, "go_floor", nullptr, {R_REG, R_REG}, F_V2_V4},

    // Returns the number of enemies killed (in regB) by the player specified by regA. This value is capped to 999.
    {0xF829, "get_num_kills", nullptr, {R_REG, W_REG}, F_V2_V4},

    // Resets the kill count for the player specified by regA.
    {0xF82A, "reset_kills", nullptr, {R_REG}, F_V2_V4},

    // Sets or clears a switch flag, and synchronizes the value to all players.
    //   valueA = floor
    //   valueB = switch flag index
    {0xF82B, "set_switch_flag_sync", "unlock_door2", {I32, I32}, F_V2_V4 | F_ARGS},
    {0xF82C, "clear_switch_flag_sync", "lock_door2", {I32, I32}, F_V2_V4 | F_ARGS},

    // Checks a switch flag on the current floor
    //   regsA[0] = switch flag index
    //   regsA[1] = result (0 or 1)
    {0xF82D, "read_switch_flag", "if_switch_not_pressed", {{W_REG_SET_FIXED, 2}}, F_V2_V4},

    // Checks a switch flag on any floor
    //   regsA[0] = floor
    //   regsA[1] = switch flag index
    //   regsA[2] = result (0 or 1)
    {0xF82E, "read_switch_flag_on_floor", "if_switch_pressed", {{W_REG_SET_FIXED, 3}}, F_V2_V4},

    // Enables a player to control the Dragon. valueA specifies the client ID.
    {0xF830, "control_dragon", nullptr, {R_REG}, F_V2_V4},

    // Disables player control of the Dragon.
    {0xF831, "release_dragon", nullptr, {}, F_V2_V4},

    // Shrinks a player or returns them to normal size. regA specifies the client ID.
    {0xF838, "shrink", nullptr, {R_REG}, F_V2_V4},
    {0xF839, "unshrink", nullptr, {R_REG}, F_V2_V4},

    // These set some camera parameters for the specified player. These parameters appear to be unused, so these
    // opcodes essentially do nothing.
    //   regsA[0] = client ID
    //   regsA[1-3] = a Vector3F (x, y, z as integers)
    {0xF83A, "set_shrink_cam1", nullptr, {{R_REG_SET_FIXED, 4}}, F_V2_V4},
    {0xF83B, "set_shrink_cam2", nullptr, {{R_REG_SET_FIXED, 4}}, F_V2_V4},

    // Shows the timer window in challenge mode. regA is the time value to display, in seconds.
    {0xF83C, "disp_time_cmode", nullptr, {R_REG}, F_V2_V4},

    // Sets the total number of areas across all challenge quests for the current episode.
    {0xF83D, "set_area_total", "unknownF83D", {I32}, F_V2_V4 | F_ARGS},

    // Sets the number of the current challenge mode area.
    {0xF83E, "set_current_area_number", "delete_area_title?", {I32}, F_V2_V4 | F_ARGS},

    // Loads a custom visual config for creating NPCs. Generally the sequence should go like this:
    //   prepare_npc_visual   label_containing_PlayerVisualConfig
    //   enable_npc_visual
    //   <opcode that creates an NPC>
    // After any NPC is created, the effects of these opcodes are undone; if the script wants to create another NPC
    // with a custom visual config, it must run these opcodes again.
    {0xF840, "enable_npc_visual", "load_npc_data", {}, F_V2_V4},
    {0xF841, "prepare_npc_visual", "get_npc_data", {{LABEL16, Arg::DataType::PLAYER_VISUAL_CONFIG, "visual_config"}}, F_V2_V4},

    // These are used to set the scores for each type of action in battle mode. The value used in each of these are
    // regsA[0] + (regsA[1] / regsA[2]), treated as a floating-point value. For example, one way to specify the value
    // 3.4 would be to set regsA to {3, 2, 5}.
    // These values are not reset between battle quests or upon joining/leaving games, so battle quests should always
    // explicitly set these! The scores which can be set are:
    //   ba_player_give_damage_score: Score earned per point of damage given to other players. (Default 0.05)
    //   ba_player_take_damage_score: Score lost per point of damage taken from other players. (Default 0.02)
    //   ba_enemy_give_damage_score: Score earned per point of damage given to non-player enemies. (Default 0.01)
    //   ba_enemy_take_damage_score: Score lost per point of damage taken from non-player enemies. (Default 0)
    //   ba_player_kill_score: Score earned by killing another player. (Default 10)
    //   ba_player_death_score: Score lost by dying to another player. (Default 7)
    //   ba_enemy_kill_score: Score earned by killing a non-player enemy. (Default 3)
    //   ba_enemy_death_score: Score lost by dying to a non-player enemy. (Default 7)
    //   ba_meseta_score: Score earned per Meseta in the player's inventory. (Default 0)
    {0xF848, "ba_player_give_damage_score", "give_damage_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF849, "ba_player_take_damage_score", "take_damage_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84A, "ba_enemy_give_damage_score", "enemy_give_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84B, "ba_enemy_take_damage_score", "enemy_take_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84C, "ba_player_kill_score", "kill_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84D, "ba_player_death_score", "death_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84E, "ba_enemy_kill_score", "enemy_kill_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84F, "ba_enemy_death_score", "enemy_death_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF850, "ba_meseta_score", "meseta_score", {{R_REG_SET_FIXED, 3}}, F_V2_V4},

    // Sets the number of traps players can use in battle mode. regsA[1] is the amount; regsA[0] is the trap type:
    //   0 = Damage trap (internal type 0)
    //   1 = Slow trap (internal type 2)
    //   2 = Confuse trap (internal type 3)
    //   3 = Freeze trap (internal type 1)
    {0xF851, "ba_set_trap_count", "ba_set_trap", {{R_REG_SET_FIXED, 2}}, F_V2_V4},

    // Enables (0) or disables (1) the targeting reticle in battle
    {0xF852, "ba_hide_target_reticle", "ba_set_target", {I32}, F_V2_V4 | F_ARGS},

    // Enables or disables overrides of warp destination floors. When enabled, area warps will always go to the next
    // floor (current floor + 1); when disabled, they will go to the floor specified in their constructor args.
    {0xF853, "override_warp_dest_floor", "reverse_warps", {}, F_V2_V4},
    {0xF854, "restore_warp_dest_floor", "unreverse_warps", {}, F_V2_V4},

    // Enables or disables overriding graphical features with those used in Ultimate
    {0xF855, "set_ult_map", nullptr, {}, F_V2_V4},
    {0xF856, "unset_ult_map", nullptr, {}, F_V2_V4},

    // Sets the area title in challenge mode
    {0xF857, "set_area_title", nullptr, {CSTRING}, F_V2_V4 | F_ARGS},

    // Enables or disables the ability to see your own traps.
    {0xF858, "ba_show_self_traps", "BA_Show_Self_Traps", {}, F_V2_V4},
    {0xF859, "ba_hide_self_traps", "BA_Hide_Self_Traps", {}, F_V2_V4},

    // Creates an item in a player's inventory and equips it.
    //   regsA[0] = client ID
    //   regsA[1-3] = item.data1[0-2]
    {0xF85A, "equip_item", "equip_item_v2", {{R_REG32_SET_FIXED, 4}}, F_V2},
    {0xF85A, "equip_item", "equip_item_v3", {{R_REG_SET_FIXED, 4}}, F_V3_V4},

    // Unequips an item from a client. Sends 6x26 if an item is unequipped.
    //   valueA = client ID
    //   valueB = equip slot, but not the same as the EquipSlot enum:
    //     0 = weapon
    //     1 = armor
    //     2 = shield
    //     3 = mag
    //     4-7 = units 1-4
    {0xF85B, "unequip_item", "unequip_item_V2", {CLIENT_ID, I32}, F_V2 | F_ARGS},
    {0xF85B, "unequip_item", "unequip_item_V3", {CLIENT_ID, I32}, F_V3_V4 | F_ARGS},

    // Same as p_talk_guild except it always refers to the local player. valueA is ignored.
    {0xF85C, "p_talk_guild_local", "QEXIT2", {I32}, F_V2_V4},

    // Sets flags that forbid types of items from being used. To forbid multiple types of items, use this opcode
    // multiple times. valueA:
    //   0 = allow normal item usage (undoes all of the following)
    //   1 = disallow weapons
    //   2 = disallow armors
    //   3 = disallow shields
    //   4 = disallow units
    //   5 = disallow mags
    //   6 = disallow tools
    {0xF85D, "set_allow_item_flags", "allow_weapons", {I32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) sonar in battle
    {0xF85E, "ba_enable_sonar", "unknownF85E", {I32}, F_V2_V4 | F_ARGS},

    // Sets the number of sonar uses per character in battle
    {0xF85F, "ba_sonar_count", "ba_use_sonar", {I32}, F_V2_V4 | F_ARGS},

    // Specifies when score announcements should occur during battle. The values are measured in minutes remaining.
    // There can be up to 8 score announcements; any further set_score_announce calls are ignored. clear_score_announce
    // deletes all announcement times.
    {0xF860, "clear_score_announce", "unknownF860", {}, F_V2_V4},
    {0xF861, "set_score_announce", "unknownF861", {I32}, F_V2_V4 | F_ARGS},

    // Creates an S-rank weapon in the player's inventory. This opcode is not used in challenge mode, presumably since
    // it doesn't offer a mechanism for the player to choose their weapon's name. The award_item_give opcode is used
    // instead.
    //   regA/valueA = client ID (must match local client ID)
    //   regB (must be a register, even on v3/v4) = item.data1[1]
    //   strC = custom name
    {0xF862, "give_s_rank_weapon", nullptr, {R_REG32, R_REG32, CSTRING}, F_V2},
    {0xF862, "give_s_rank_weapon", nullptr, {I32, R_REG, CSTRING}, F_V3_V4 | F_ARGS},

    // Returns the currently-equipped mag's levels. If no mag is equipped, regsA are unaffected! Make sure to
    // initialize them before using this.
    //   regsA[0] = returned DEF level
    //   regsA[1] = returned POW level
    //   regsA[2] = returned DEX level
    //   regsA[3] = returned MIND level
    {0xF863, "get_mag_levels", nullptr, {{W_REG32_SET_FIXED, 4}}, F_V2},
    {0xF863, "get_mag_levels", nullptr, {{W_REG_SET_FIXED, 4}}, F_V3_V4},

    // Sets the color and rank text if the player manages to complete the current challenge stage.
    //   valueA = color as XRGB8888
    //   strB = rank text (up to 11 characters)
    {0xF864, "set_cmode_rank_result", "cmode_rank", {I32, CSTRING}, F_V2_V4 | F_ARGS},

    // Shows the item name entry window and suspends the calling thread
    {0xF865, "award_item_name", "award_item_name?", {}, F_V2_V4},

    // Shows the item choice window and suspends the calling thread
    {0xF866, "award_item_select", "award_item_select?", {}, F_V2_V4},

    // Creates an item in the player's inventory, chosen via the previous award_item_name and award_item_select
    // opcodes, and updates the player's challenge rank text and color according to the rank they achieved. Sends 07DF
    // on BB; on other versions, sends nothing.
    //   regA = return value (1 if item successfully created; 0 otherwise)
    {0xF867, "award_item_give", "award_item_give_to?", {W_REG}, F_V2_V4},

    // Specifies where the time threshold is for a challenge rank.
    //   regsA[0] = rank (0 = B, 1 = A, 2 = S)
    //   regsA[1] = time in seconds (times faster than this are considered to be this rank or better)
    //   regsA[2] = award flags mask (generally should be (1 << regsA[0]))
    //   regB = result (0 = failed, 1 = success)
    {0xF868, "set_cmode_rank_threshold", "set_cmode_rank", {{R_REG_SET_FIXED, 3}, W_REG}, F_V2_V4},

    // Registers a timing result of (regA) seconds for the current challenge mode stage. Returns a result code in regB:
    //   0 = player achieved rank B and has not completed this stage before
    //   1 = player achieved rank A on this stage for the first time
    //   2 = player achieved rank S on this stage for the first time
    //   3 = can't create results object (internal error)
    //   4 = player did not achieve a new rank this time
    //   5 = player's inventory is full and can't receive the prize
    //   6 = internal errors (e.g. save file is missing, stage number not set)
    {0xF869, "check_rank_time", nullptr, {R_REG, W_REG}, F_V2_V4},

    // Creates an item in the local player's bank, and saves the player's challenge rank and title color. Sends 07DF on
    // BB. This is used for the A and B rank prizes.
    //   regsA = item.data1[0-5]
    //   regB = returned success code (1 = success, 0 = failed)
    {0xF86A, "item_create_cmode", nullptr, {{R_REG_SET_FIXED, 6}, W_REG}, F_V2_V4},

    // Sets the effective area for item drops in battle. valueA should be in the range [1, 10].
    {0xF86B, "ba_set_box_drop_area", "ba_box_drops", {R_REG}, F_V2_V4},

    // Shows a confirmation window asking if the player is satsified with their choice of S rank prize and weapon name.
    //   regA = result code (0 = OK, 1 = reconsider)
    {0xF86C, "award_item_ok", "award_item_ok?", {W_REG}, F_V2_V4},

    // Enables or disables traps' ability to hurt the player who set them
    {0xF86D, "ba_set_trapself", nullptr, {}, F_V2_V4},
    {0xF86E, "ba_clear_trapself", "ba_ignoretrap", {}, F_V2_V4},

    // Sets the number of lives each player gets in battle when the LIMIT_LIVES respawn mode is used.
    {0xF86F, "ba_set_lives", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Sets the maximum level for any technique in battle
    {0xF870, "ba_set_max_tech_level", "ba_set_tech_lvl", {I32}, F_V2_V4 | F_ARGS},

    // Sets the character's overlay level in battle
    {0xF871, "ba_set_char_level", "ba_set_lvl", {I32}, F_V2_V4 | F_ARGS},

    // Sets the battle time limit. valueA is measured in minutes
    {0xF872, "ba_set_time_limit", nullptr, {I32}, F_V2_V4 | F_ARGS},

    // Sets regA to 1 if Dark Falz has been defeated, or 0 otherwise.
    {0xF873, "dark_falz_is_dead", "falz_is_dead", {W_REG}, F_V2_V4},

    // Sets an override for the challenge rank text. At the time the rank is checked via check_rank_time, these
    // overrides are applied in the order they were created. For each one, if the existing rank matches the check
    // string, it is replaced with the override string and the color is replaced with the override color.
    //   valueA = override color (XRGB8888)
    //   strB = check string and override string (separated by \t or \n)
    {0xF874, "set_cmode_rank_override", "unknownF874", {I32, CSTRING}, F_V2_V4 | F_ARGS},

    // Enables or disables the transparency effect, similar to the Stealth Suit. regA is the client ID.
    {0xF875, "enable_stealth_suit_effect", nullptr, {R_REG}, F_V2_V4},
    {0xF876, "disable_stealth_suit_effect", nullptr, {R_REG}, F_V2_V4},

    // Enables or disables the use of techniques for a player. regA is the client ID.
    {0xF877, "enable_techs", nullptr, {R_REG}, F_V2_V4},
    {0xF878, "disable_techs", nullptr, {R_REG}, F_V2_V4},

    // Returns the gender of a character.
    //   regA = client ID
    //   regB = returned gender (0 = male, 1 = female, 2 = no player present or invalid class flags)
    {0xF879, "get_gender", nullptr, {R_REG, W_REG}, F_V2_V4},

    // Returns the race and class of a character.
    //   regA = client ID
    //   regsB[0] = returned race (0 = human, 1 = newman, 2 = android, 3 = no player present or invalid class flags)
    //   regsB[1] = returned class (0 = hunter, 1 = ranger, 2 = force, 3 = no player present or invalid class flags)
    {0xF87A, "get_chara_class", nullptr, {R_REG, {W_REG_SET_FIXED, 2}}, F_V2_V4},

    // Removes Meseta from a player. Sends 6xC9 on BB.
    //   regsA[0] = client ID
    //   regsA[1] = amount to subtract (positive integer)
    //   regsB[0] = result code if player is present (1 = taken, 0 = player didn't have enough)
    //   regsB[1] = result code if player not present (set to 0 if there is no player with the specified client ID)
    // Note that only one of regsB[0] and regsB[1] is written; the other is unchanged. Therefore, it's good practice to
    // set regsB[1] to a nonzero value before using this opcode.
    {0xF87B, "take_slot_meseta", nullptr, {{R_REG_SET_FIXED, 2}, {W_REG_SET_FIXED, 2}}, F_V2_V4},

    // Returns the Guild Card file creation time in seconds since 00:00:00 on 1 January 2000.
    {0xF87C, "get_guild_card_file_creation_time", "get_encryption_key", {W_REG}, F_V2_V4},

    // Kills the player whose client ID is regA.
    {0xF87D, "kill_player", nullptr, {R_REG}, F_V2_V4},

    // Returns (in regA) the player's serial number. On BB, returns 0.
    {0xF87E, "get_serial_number", nullptr, {W_REG}, F_V2_V3},
    {0xF87E, "return_0_F87E", nullptr, {W_REG}, F_V4},

    // Reads an event flag from the system file.
    //   regA = event flag index (0x00-0xFF)
    //   regB = returned event flag value (1 byte)
    {0xF87F, "get_eventflag", "read_guildcard_flag", {R_REG, W_REG}, F_V2_V4},

    // Normally, trap damage is computed with the following formula:
    //   (700.0 * area_factor[area] * 2.0 * (0.01 * level + 0.1))
    // This opcode overrides that computation. The value is specified with the integer and fractional parts split up:
    // the actual value used by the game will be regsA[0] + (regsA[1] / regsA[2]).
    {0xF880, "set_trap_damage", "ba_set_dmgtrap", {{R_REG_SET_FIXED, 3}}, F_V2_V4},

    // Loads the name of the player whose client ID is regA into a static buffer, which can later be referred to with
    // "<pl_name>" in message strings.
    {0xF881, "get_pl_name", "get_pl_name?", {R_REG}, F_V2_V4},

    // Loads the job (Hunter, Ranger, or Force) of the player whose client ID is regA into a static buffer, which can
    // later be referred to with "<pl_job>" in message strings.
    {0xF882, "get_pl_job", nullptr, {R_REG}, F_V2_V4},

    // Counts the number of players near the specified player.
    //   regsA[0] = client ID
    //   regsA[1] = radius (as integer)
    //   regB = count
    {0xF883, "get_player_proximity", "players_in_range", {{R_REG_SET_FIXED, 2}, W_REG}, F_V2_V4},

    // Writes 2 bytes to the event flags in the system file.
    //   valueA = flag index (must be 254 or less)
    //   regB/valueB = value
    {0xF884, "set_eventflag16", "write_guild_flagw", {I32, R_REG}, F_V2},
    {0xF884, "set_eventflag16", "write_guild_flagw", {I32, I32}, F_V3_V4 | F_ARGS},

    // Writes 4 bytes to the event flags in the system file.
    //   valueA = flag index (must be 252 or less)
    //   regB/valueB = value
    {0xF885, "set_eventflag32", "write_guild_flagl", {I32, R_REG}, F_V2},
    {0xF885, "set_eventflag32", "write_guild_flagl", {I32, I32}, F_V3_V4 | F_ARGS},

    // Returns (in regB) the battle result place (1, 2, 3, or 4) of the player specified by regA.
    {0xF886, "ba_get_place", nullptr, {R_REG, W_REG}, F_V2_V4},

    // Returns (in regB) the battle score of the player specified by regA.
    {0xF887, "ba_get_score", nullptr, {R_REG, W_REG}, F_V2_V4},

    // TODO: Document these
    {0xF888, "enable_win_pfx", "ba_close_msg", {}, F_V2_V4},
    {0xF889, "disable_win_pfx", nullptr, {}, F_V2_V4},

    // Returns (in regB) the state of the player specified by regA. State values:
    //   0 = no player present, or warping (non-BB)
    //   1 = standing
    //   2 = walking
    //   3 = running
    //   4 = attacking
    //   5 = casting technique
    //   6 = performing photon blast
    //   7 = defending
    //   8 = taking damage / knocked down
    //   9 = dead
    //   10 = cutscene
    //   11 = reviving
    //   12 = frozen
    //   13 = warping (BB only)
    {0xF88A, "get_player_state", "get_player_status", {R_REG, W_REG}, F_V2_V4},

    // Sends a Simple Mail message to the player.
    //   regA (v2) = sender's Guild Card number
    //   valueA (v3+) = number of register that holds sender's Guild Card number
    //   strB = sender name and message (separated by \n)
    {0xF88B, "send_mail", nullptr, {R_REG, CSTRING}, F_V2_V4 | F_ARGS},

    // Returns the game's major version (2 on DCv2/PC/GCNTE, 3 on GC/Ep3, 4 on Xbox and BB)
    {0xF88C, "get_game_version", nullptr, {W_REG}, F_V2_V4},

    // Sets the local player's stage completion time in challenge mode.
    //   regA = time in seconds
    //   regB = value to be used in computation of token_v4 (BB only; see 6x95 in CommandFormats.hh for details)
    {0xF88D, "chl_set_timerecord", "chl_set_timerecord?", {R_REG}, F_V2 | F_V3},
    {0xF88D, "chl_set_timerecord", "chl_set_timerecord?", {R_REG, R_REG}, F_V4},

    // Gets the current player's completion time for the current challenge stage in seconds. If the player's time is
    // invalid or faster than the time set by chl_set_min_time_online (or 5 minutes, if offline), returns -2. If used
    // in non-challenge mode, returns -1.
    {0xF88E, "chl_get_timerecord", "chl_get_timerecord?", {W_REG}, F_V2_V4},

    // Sets the probabilities of getting recovery items from challenge mode graves. There are 10 floating-point values,
    // specified as fractions in an array of 20 registers (pairs of numerator and denominator). The number of items
    // generated is capped by the number of players present; for example, if the game chooses to generate 4 items but
    // only 2 players are present, 2 items are generated.
    // Counts array (these 4 values should sum to 1.0 or less):
    //   regsA[0] / regsA[1]: Chance of getting 1 recovery item
    //   regsA[2] / regsA[3]: Chance of getting 2 recovery items
    //   regsA[4] / regsA[5]: Chance of getting 3 recovery items
    //   regsA[6] / regsA[7]: Chance of getting 4 recovery items
    // Types array (these 6 values should sum to 1.0 or less):
    //   regsA[8] / regsA[9]: Chance of getting Monomate x1
    //   regsA[10] / regsA[11]: Chance of getting Dimate x1
    //   regsA[12] / regsA[13]: Chance of getting Trimate x1
    //   regsA[14] / regsA[15]: Chance of getting Monofluid x1
    //   regsA[16] / regsA[17]: Chance of getting Difluid x1
    //   regsA[18] / regsA[19]: Chance of getting Trifluid x1
    {0xF88F, "set_cmode_grave_rates", nullptr, {{R_REG_SET_FIXED, 20}}, F_V2_V4},

    // Clears all levels from the main warp.
    {0xF890, "clear_mainwarp_all", "clear_area_list", {}, F_V2_V4},

    // Specifies which enemy should be affected by subsequent get_*_data opcodes (the following 4 definitions). valueA
    // is the battle parameter index for the desired enemy.
    {0xF891, "set_override_enemy_bp_index", "load_enemy_data", {I32}, F_V2_V4 | F_ARGS},

    // Replaces enemy stats with the given structures (PlayerStats, AttackData, ResistData, or MovementData) for the
    // enemy previously specified with load_enemy_data.
    {0xF892, "set_enemy_physical_data", "get_physical_data", {{LABEL16, Arg::DataType::PLAYER_STATS, "stats"}}, F_V2_V4},
    {0xF893, "set_enemy_attack_data", "get_attack_data", {{LABEL16, Arg::DataType::ATTACK_DATA, "attack_data"}}, F_V2_V4},
    {0xF894, "set_enemy_resist_data", "get_resist_data", {{LABEL16, Arg::DataType::RESIST_DATA, "resist_data"}}, F_V2_V4},
    {0xF895, "set_enemy_movement_data", "get_movement_data", {{LABEL16, Arg::DataType::MOVEMENT_DATA, "movement_data"}}, F_V2_V4},

    // Reads 2 bytes or 4 bytes from the event flags in the system file.
    //   regA = event flag index
    //   regB = returned value
    {0xF896, "get_eventflag16", "read_guildflag_16b", {R_REG, W_REG}, F_V2_V4},
    {0xF897, "get_eventflag32", "read_guildflag_32b", {R_REG, W_REG}, F_V2_V4},

    // regA <<= regB
    {0xF898, "shift_left", nullptr, {W_REG, R_REG}, F_V2_V4},

    // regA >>= regB
    {0xF899, "shift_right", nullptr, {W_REG, R_REG}, F_V2_V4},

    // Generates a random number by calling rand(). Note that the returned value is not uniform! The algorithm
    // generates a uniform random number, scales it to the range 0 through (max-1) inclusive, then clamps it to the
    // range min through (max-1). So, if the minimum value is not 0, the minimum value is more likely than all other
    // possible results. To get an unbiased result with a minimum not equal to zero, use this with a minimum of zero,
    // then use add or addi.
    // Note also that this is implemented by calling rand(), which returns a 15-bit random integer. If your range of
    // values is larger than 15 bits, call this multiple times and combine the results appropriately.
    //   regsA[0] = minimum value
    //   regsA[1] = maximum value
    //   regB = generated random value
    {0xF89A, "get_random", nullptr, {{R_REG_SET_FIXED, 2}, W_REG}, F_V2_V4},

    // Clears all game state, including all floor items, set states (enemy and object), enemy and object states, wave
    // event flags, and switch flags. Also destroys all running quest threads.
    {0xF89B, "reset_map", nullptr, {}, F_V2_V4},

    // Returns the leader's choice when a challenge is failed in regA. Values:
    //   0 = not chosen yet
    //   1 = no (releases players to interact on Pioneer 2)
    //   2 = yes (restarts the stage)
    {0xF89C, "get_chl_retry_choice", "retry_menu", {W_REG}, F_V2_V4},

    // Creates the retry menu when a challenge stage is failed
    {0xF89D, "chl_create_retry_menu", "chl_enable_retry", {}, F_V2_V4},

    // Enables (0) or disables (1) the use of scape dolls in battle
    {0xF89E, "ba_forbid_scape_dolls", "ba_forbid_scape_doll", {I32}, F_V2_V4 | F_ARGS},

    // Restores a player's HP and TP, clears status effects, and revives the player if dead.
    //   regA = client ID
    {0xF89F, "player_recovery", "unknownF89F", {R_REG}, F_V2_V4},

    // These opcodes set, clear, and check (respectively) a flag that appears to do nothing at all.
    {0xF8A0, "disable_bosswarp_option", "unknownF8A0", {}, F_V2_V4},
    {0xF8A1, "enable_bosswarp_option", "unknownF8A1", {}, F_V2_V4},
    {0xF8A2, "is_bosswarp_opt_disabled", "get_bosswarp_option", {W_REG}, F_V2_V4},

    // Loads the player's serial number into the "flag buffer", which is a 4-byte buffer that can be written to event
    // flags. It's not obvious why this can't just be done with get_serial_number and set_eventflag32. This opcode
    // loads 0 to the flag buffer on BB.
    {0xF8A3, "load_serial_number_to_flag_buf", "init_online_key?", {}, F_V2_V4},

    // Writes the flag buffer to event flags. regA specifies which event flag (the first of 4 consecutive flags).
    {0xF8A4, "write_flag_buf_to_event_flags", "encrypt_gc_entry_auto", {R_REG}, F_V2_V4},

    // Like set_chat_callback, but without a filter string. The meaning of regsA is the same as for set_chat_callback.
    {0xF8A5, "set_chat_callback_no_filter", "chat_detect", {{R_REG_SET_FIXED, 5}}, F_V2_V4},

    // Creates a symbol chat collision object. See the description of TOSymbolchatColli in Map.cc for details on how
    // this object behaves and what these arguments mean.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = radius
    //   regsA[4-6] = specs 0-2 (see TOSymbolchatColli description)
    //   regsA[7-9] = data labels for symbol chats 0-2; each points to a SymbolChat structure (see SymbolChatT in
    //     PlayerSubordinates.hh). Note that this structure is not byteswapped properly, so GameCube quests that use
    //     this opcode should use the big-endian version of the struct. (Practically, this means the first 32-bit field
    //     and the following 4 16-bit fields must be byteswapped.)
    {0xF8A6, "set_symbol_chat_collision", "symbol_chat_create", {{R_REG_SET_FIXED, 10}}, F_V2_V4},

    // Sets the size that a player shrinks to when using the shrink opcode. regA specified the client ID. The actual
    // shrink size used is regsB[0] + (regsB[1] / regsB[2]). If regsB[2] is 0, the fractional part is considered to be
    // zero and not used.
    {0xF8A7, "set_shrink_size", nullptr, {R_REG, {R_REG_SET_FIXED, 3}}, F_V2_V4},

    // Sets the amount by which techniques level up upon respawn in battle.
    {0xF8A8, "ba_death_tech_level_up", "death_tech_lvl_up2", {I32}, F_V2_V4 | F_ARGS},

    // Returns 1 if Vol Opt has been defeated in the current game/quest.
    {0xF8A9, "vol_opt_is_dead", "volopt_is_dead", {W_REG}, F_V2_V4},

    // Returns 1 if the local player has a challenge mode grave message.
    {0xF8AA, "is_there_grave_message", nullptr, {W_REG}, F_V2_V4},

    // Returns the local player's battle mode records. The values returned are the first 7 fields of the
    // PlayerRecordsBattle structure (see PlayerSubordinates.hh). These are:
    //   regsA[0-3] = number of times placed 1st, 2nd, 3rd, and 4th, respectively
    //   regsA[4] = number of disconnects
    //   regsA[5-6] = unknown (TODO)
    {0xF8AB, "get_ba_record", nullptr, {{W_REG_SET_FIXED, 7}}, F_V2_V4},

    // Returns the current player's challenge mode rank. Reads from the state corresponding to the current game mode
    // (that is, reads from online Ep1 records in online Ep1 challenge mode, reads from offline Ep1 records in offline
    // challenge mode, reads from Ep2 records in Ep2). Return values:
    //   0 = challenge mode not completed
    //   1 = B rank
    //   2 = A rank
    //   3 = S rank
    {0xF8AC, "get_cmode_prize_rank", nullptr, {W_REG}, F_V2_V4},

    // Returns the number of players (in regA). Unlike get_number_of_players, this counts the number of objects that
    // have entity IDs assigned in the players' ID space, whereas get_number_of_players counds the number of TObjPlayer
    // objects. For all practical purposes, these should result in the same number.
    {0xF8AD, "get_number_of_players2", "get_number_of_player2", {W_REG}, F_V2_V4},

    // Returns 1 (in regA) if the current game has a nonempty name. The game name is set by command 8A from the server.
    {0xF8AE, "party_has_name", nullptr, {W_REG}, F_V2_V4},

    // Returns 1 (in regA) if there is a chat message available (that is, if anyone has sent a chat message in the
    // current game).
    {0xF8AF, "someone_has_spoken", nullptr, {W_REG}, F_V2_V4},

    // Reads a 1-byte, 2-byte, or 4-byte value from the address (regB/valueB) and places it in regA.
    {0xF8B0, "read1", nullptr, {W_REG, R_REG}, F_V2},
    {0xF8B0, "read1", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},
    {0xF8B1, "read2", nullptr, {W_REG, R_REG}, F_V2},
    {0xF8B1, "read2", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},
    {0xF8B2, "read4", nullptr, {W_REG, R_REG}, F_V2},
    {0xF8B2, "read4", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},

    // Writes a 1-byte, 2-byte, or 4-byte value from regB/valueB to the address (regA/valueA). On v2 and GC NTE, these
    // opcodes have a bug which makes them essentially useless: they ignore regB and instead write the value in regA to
    // the address in regA.
    {0xF8B3, "write1", nullptr, {R_REG, R_REG}, F_V2},
    {0xF8B3, "write1", nullptr, {I32, I32}, F_V3_V4 | F_ARGS},
    {0xF8B4, "write2", nullptr, {R_REG, R_REG}, F_V2},
    {0xF8B4, "write2", nullptr, {I32, I32}, F_V3_V4 | F_ARGS},
    {0xF8B5, "write4", nullptr, {R_REG, R_REG}, F_V2},
    {0xF8B5, "write4", nullptr, {I32, I32}, F_V3_V4 | F_ARGS},

    // Returns a bitmask of 5 different types of detectable hacking. This opcode only works on DCv2 - it crashes on all
    // other versions, since it tries to access memory at 8C007220, which is only a valid address on DC. The bits are:
    //   0x01 = any byte in [8C007220, 8C0072E0) is nonzero (crashes on non-DC)
    //   0x02 = has cheat device save files on VMU (always zero on non-DC); looks for names: CDX_CODES_00,
    //     CDX_SETTINGS, XT-CHEATS, FCDCHEATS
    //   0x04 = any of the first 3 VMU-like devices (with function_code & 0x0E not equal to zero) on any of the 4 Maple
    //     buses reports power usage (standby or max) above 4 amps; always zero on non-DC systems
    //   0x08 = hacked item flag has been set (see implementation of are_rare_drops_allowed in ItemCreator.cc)
    //   0x10 = any bits in validation_flags in the character file are set (see PSOGCCharacterFile::Character)
    {0xF8B6, "check_for_hacking", "is_mag_hacked", {W_REG}, F_V2_V4},

    // Challenge mode cannot be completed unless this many seconds have passed since the stage began. If not set or if
    // offline, 5 minutes is used as the threshold instead.
    {0xF8B7, "chl_set_min_time_online", "unknownF8B7", {R_REG}, F_V2_V4},

    // Disables the challenge mode retry menu
    {0xF8B8, "disable_retry_menu", "unknownF8B8", {}, F_V2_V4},

    // Shows the list of dead players in challenge mode
    {0xF8B9, "chl_show_dead_player_list", "chl_death_recap", {}, F_V2_V4},

    // Loads the Guild Card file creation time to the flag buffer. (See F8A3 and F8A4 for more details.)
    {0xF8BA, "load_guild_card_file_creation_time_to_flag_buf", "encrypt_gc_entry_auto2", {}, F_V2_V4},

    // Behaves exactly the same as write_flag_buf_to_event_flags (F8A4).
    {0xF8BB, "write_flag_buf_to_event_flags2", "unknownF8BB", {R_REG}, F_V2_V4},

    // Returns (in regB) the Guild Card number of the player in the slot specified by regA. If there is no player in
    // that slot, returns FFFFFFFF. This opcode is only implemented on certain later versions of PC v2, and not on any
    // v3 or later versions.
    {0xF8BC, "get_player_guild_card_number", nullptr, {R_REG, W_REG}, F_PC_V2},

    // Sets the current episode. Must be used in the start label. valueA should be 0 for Episode 1 (which is the
    // default), 1 for Episode 2, or 2 for Episode 4 (BB only). This opcode also resets the floor configuration, so it
    // will undo any effects of the map_designate family of opcodes.
    {0xF8BC, "set_episode", nullptr, {I32}, F_V3_V4},

    // This opcode returns (in regsB) the full symbol chat data for the symbol chat currently being said by the player
    // specified in regA. The symbol chat data is only returned for 120 frames (4 seconds) after the corresponding 6x07
    // command is received; after that, this opcode will return a blank symbol chat instead. This opcode only works if
    // create_symbol_chat_monitor is run first.
    // This opcode is only implemented on certain later versions of PC v2, and not on any v3 or later versions.
    {0xF8BD, "get_current_symbol_chat", nullptr, {R_REG, {W_REG_SET_FIXED, 15}}, F_PC_V2},

    // This opcode is enables the usage of get_current_symbol_chat. This opcode is only implemented on certain later
    // versions of PC v2, and not on any v3 or later versions.
    {0xF8BE, "create_symbol_chat_monitor", nullptr, {}, F_PC_V2},

    // This opcode causes the client to immediately save the PSO______COM (system) and PSO______GUD (Guild Card) files
    // to disk. This opcode is only implemented on certain later versions of PC v2, and not on v3 or later versions.
    {0xF8BF, "save_system_and_gc_files", nullptr, {}, F_PC_V2},

    // Requests a file from the server by sending a D7 command. valueA specifies header.flag, strB is the file name (up
    // to 16 characters). This opcode works on Xbox, but the GBA opcodes do not, so it's ultimately not useful there.
    // This opcode does nothing on BB.
    {0xF8C0, "file_dl_req", nullptr, {I32, CSTRING}, F_V3 | F_ARGS},
    {0xF8C0, "nop_F8C0", nullptr, {I32, CSTRING}, F_V4 | F_ARGS},

    // Returns the status of the download requested with file_dl_req. Return values (in regA):
    //   0 = failed (server sent a D7 command)
    //   1 = pending
    //   2 = complete
    {0xF8C1, "get_dl_status", nullptr, {W_REG}, F_V3},
    {0xF8C1, "nop_F8C1", nullptr, {R_REG}, F_V4},

    // Prepares to load a GBA ROM from a previous file_dl_req opcode. Does nothing on Xbox and BB.
    {0xF8C2, "prepare_gba_rom_from_download", "gba_unknown4?", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF8C2, "nop_F8C2", nullptr, {}, F_XB_V3 | F_V4},

    // Starts loading a GBA ROM to a connected GBA, or checks the status of a previous load request. One of
    // prepare_gba_rom_from_download or prepare_gba_rom_from_disk must be called before calling this, then this should
    // be called repeatedly until it succeeds or fails. Return values:
    //   0 = not started
    //   1 = failed
    //   2 = timed out
    //   3 = in progress
    //   4 = complete
    // This opcode always returns 0 on Xbox, and does nothing (doesn't even affect regA) on BB.
    {0xF8C3, "start_or_update_gba_joyboot", "get_gba_state?", {W_REG}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF8C3, "return_0_F8C3", nullptr, {W_REG}, F_XB_V3},
    {0xF8C3, "nop_F8C3", nullptr, {R_REG}, F_V4},

    // Shows the challenge mode result window in split-screen mode. Does nothing on BB.
    //   regA = completion time in seconds, as returned by chl_get_timerecord
    {0xF8C4, "congrats_msg_multi_cm", "unknownF8C4", {R_REG}, F_V3},
    {0xF8C4, "nop_F8C4", nullptr, {R_REG}, F_V4},

    // Checks if the stage is done in offline challenge mode. Returns 1 if the stage is still in progress, or 0 if it's
    // completed or failed.
    {0xF8C5, "stage_in_progress_multi_cm", "stage_end_multi_cm", {W_REG}, F_V3},
    {0xF8C5, "nop_F8C5", nullptr, {R_REG}, F_V4},

    // Causes a fade to black, then exits the game. This is the same result as receiving a 6x73 command.
    {0xF8C6, "qexit", "QEXIT", {}, F_V3_V4},

    // Causes a player to perform an animation.
    //   regA = client ID
    //   regB = animation number (TODO: document these)
    {0xF8C7, "use_animation", nullptr, {R_REG, R_REG}, F_V3_V4},

    // Stops an animation started with use_animation.
    //   regA = client ID
    {0xF8C8, "stop_animation", nullptr, {R_REG}, F_V3_V4},

    // Causes a player to run to a location, as 6x42 does. Sends 6x42.
    //   regsA[0-2] = location (x, y, z as integers; y is ignored)
    //   regsA[3] = client ID
    {0xF8C9, "run_to_coord", nullptr, {{R_REG_SET_FIXED, 4}, R_REG}, F_V3_V4},

    // Makes a player invincible, or removes their invincibility.
    //   regA = client ID
    //   regB = enable invicibility (1 = enable, 0 = disable)
    {0xF8CA, "set_slot_invincible", nullptr, {R_REG, R_REG}, F_V3_V4},

    // Removes a player's invicibility. clear_slot_invincible rXX is equivalent to set_slot_invincible rXX, 0.
    //   regA = client ID
    {0xF8CB, "clear_slot_invincible", "set_slot_targetable?", {R_REG}, F_V3_V4},

    // These opcodes inflict various status conditions on a player. In the case of Shifta/Deband/Jellen/Zalure, the
    // effective technicuqe level is 21.
    //   regA = client ID
    {0xF8CC, "set_slot_poison", nullptr, {R_REG}, F_V3_V4},
    {0xF8CD, "set_slot_paralyze", nullptr, {R_REG}, F_V3_V4},
    {0xF8CE, "set_slot_shock", nullptr, {R_REG}, F_V3_V4},
    {0xF8CF, "set_slot_freeze", nullptr, {R_REG}, F_V3_V4},
    {0xF8D0, "set_slot_slow", nullptr, {R_REG}, F_V3_V4},
    {0xF8D1, "set_slot_confuse", nullptr, {R_REG}, F_V3_V4},
    {0xF8D2, "set_slot_shifta", nullptr, {R_REG}, F_V3_V4},
    {0xF8D3, "set_slot_deband", nullptr, {R_REG}, F_V3_V4},
    {0xF8D4, "set_slot_jellen", nullptr, {R_REG}, F_V3_V4},
    {0xF8D5, "set_slot_zalure", nullptr, {R_REG}, F_V3_V4},

    // Same as leti_fixed_camera, but takes floating-point arguments.
    //   regsA[0-2] = camera location (x, y, z as floats)
    //   regsA[3-5] = camera focus location (x, y, z as floats)
    {0xF8D6, "fleti_fixed_camera", nullptr, {{R_REG_SET_FIXED, 6}}, F_V3_V4 | F_ARGS},

    // Sets the camera to follow the player at a fixed angle.
    //   valueA = client ID
    //   regsB[0-2] = camera angle (x, y, z as floats)
    {0xF8D7, "fleti_locked_camera", nullptr, {I32, {R_REG_SET_FIXED, 3}}, F_V3_V4 | F_ARGS},

    // This opcode appears to be exactly the same as default_camera_pos. TODO: Is there any difference?
    {0xF8D8, "default_camera_pos2", nullptr, {}, F_V3_V4},

    // Enables a motion blur visual effect.
    {0xF8D9, "set_motion_blur", nullptr, {}, F_V3_V4},

    // Enables a monochrome visual effect.
    {0xF8DA, "set_screen_bw", "set_screen_b&w", {}, F_V3_V4},

    // Computes a point along a path composed of multiple control points.
    //   valueA = number of control points
    //   valueB = speed along path
    //   valueC = current position along path
    //   valueD = loop flag (0 = no, 1 = yes)
    //   regsE[0-2] = result point (x, y, z as floats)
    //   regsE[3] = result code (0 = failed, 1 = success)
    //   labelF = control point entries (array of valueA VectorXYZTF structures)
    {0xF8DB, "get_vector_from_path", "unknownF8DB", {I32, FLOAT32, FLOAT32, I32, {W_REG_SET_FIXED, 4}, {LABEL16, Arg::DataType::VECTOR4F_LIST}}, F_V3_V4 | F_ARGS},

    // Same as npc_text, but only applies to a specific player slot.
    //   valueA = client ID
    //   valueB = situation number (same as for npc_text)
    //   strC = string for NPC to say (up to 52 characters)
    {0xF8DC, "npc_text_id", "NPC_action_string", {R_REG, R_REG, CSTRING_LABEL16}, F_V3_V4},

    // Returns a bitmask of the buttons which are currently pressed or held on this frame.
    //   regA = controller port number
    //   regB = returned button flags
    {0xF8DD, "get_held_buttons", "get_pad_cond", {R_REG, W_REG}, F_V3_V4},

    // Returns a bitmask of the buttons which were newly pressed on this frame. Buttons which were pressed on previous
    // frames and still held down are not returned. Same arguments as get_held_buttons.
    {0xF8DE, "get_pressed_buttons", "get_button_cond", {R_REG, W_REG}, F_V3_V4},

    // Freezes enemies and makes them untargetable, or unfreezes them and makes them targetable again. Internally, this
    // toggles a flag that disables updates for every child object of TL_04.
    {0xF8DF, "toggle_freeze_enemies", "freeze_enemies", {}, F_V3_V4},

    // Unfreezes enemies and makes them targetable again.
    {0xF8E0, "unfreeze_enemies", nullptr, {}, F_V3_V4},

    // Freezes (almost) everything, or unfreezes (almost) everything. Internally, this toggles the disable-updates flag
    // for every child of all root objects except TL_SU, TL_00, TL_CAMERA, and TL_01.
    {0xF8E1, "toggle_freeze_everything", "freeze_everything", {}, F_V3_V4},

    // Unfreezes (almost) everything, if toggle_freeze_everything has been executed before.
    {0xF8E2, "unfreeze_everything", nullptr, {}, F_V3_V4},

    // Sets a player's HP or TP to their maximum HP or TP.
    //   regA = client ID
    {0xF8E3, "restore_hp", nullptr, {R_REG}, F_V3_V4},
    {0xF8E4, "restore_tp", nullptr, {R_REG}, F_V3_V4},

    // Closes a chat bubble for a player, if one is open.
    //   regA = client ID
    {0xF8E5, "close_chat_bubble", nullptr, {R_REG}, F_V3_V4},

    // Moves a dynamic collision object.
    //   regA = object token (returned by set_obj_param, etc.)
    //   regsB[0-2] = location (x, y, z as integers)
    {0xF8E6, "move_coords_object", nullptr, {R_REG, {R_REG_SET_FIXED, 3}}, F_V3_V4},

    // These are the same as their counterparts without _ex, but these return an object token in regB which can be used
    // with del_obj_param, move_coords_object, etc. set_obj_param_ex is the same as set_obj_param, since set_obj_param
    // already returns an object token.
    {0xF8E7, "at_coords_call_ex", nullptr, {{R_REG_SET_FIXED, 5}, W_REG}, F_V3_V4},
    {0xF8E8, "at_coords_talk_ex", nullptr, {{R_REG_SET_FIXED, 5}, W_REG}, F_V3_V4},
    {0xF8E9, "npc_coords_call_ex", "walk_to_coord_call_ex", {{R_REG_SET_FIXED, 5}, W_REG}, F_V3_V4},
    {0xF8EA, "party_coords_call_ex", "col_npcinr_ex", {{R_REG_SET_FIXED, 6}, W_REG}, F_V3_V4},
    {0xF8EB, "set_obj_param_ex", "unknownF8EB", {{R_REG_SET_FIXED, 6}, W_REG}, F_V3_V4},
    {0xF8EC, "npc_check_straggle_ex", "col_plinaw_ex", {{R_REG_SET_FIXED, 9}, W_REG}, F_V3_V4},

    // Returns 1 if the player is doing certain animations. (TODO: Which ones?)
    //   regA = client ID
    //   regB = returned value
    {0xF8ED, "animation_check", nullptr, {R_REG, W_REG}, F_V3_V4},

    // Specifies which image to use for the image board in Pioneer 2 (if placed). Only one image may be loaded at a
    // time. Images must be square and dimensions must be a power of two, similar to the B9 command on Ep3. On DC or
    // PCv2, the image data is expected to be a PVR file; on GC, it should be a GVR file; on Xbox and BB it should be
    // an XVR file.
    //   regA = decompressed size
    //   labelB = label containing PRS-compressed image file data
    {0xF8EE, "call_image_data", nullptr, {I32, {LABEL16, Arg::DataType::IMAGE_DATA}}, F_V3_V4 | F_ARGS},

    // This opcode does nothing on all versions where it's implemented.
    {0xF8EF, "nop_F8EF", "unknownF8EF", {}, F_V3_V4},

    // Disables or enables the background music on Pioneer 2. If the BGM has not been overridden with create_bgmctrl
    // and set_bgm, the music will resume at the points where it would normally change (e.g. entering or leaving the
    // shop area). turn_off_bgm_p2 may be repeated every frame to avoid this consequence.
    {0xF8F0, "turn_off_bgm_p2", nullptr, {}, F_V3_V4},
    {0xF8F1, "turn_on_bgm_p2", nullptr, {}, F_V3_V4},

    // Computes a point along a Bezier curve defined by a sequence of vectors. This is similar to get_vector_from_path,
    // but results in a smoother curve. Arguments:
    //   valueA = number of control points
    //   valueB = speed along path
    //   valueC = current position along path
    //   valueD = loop flag (0 = no, 1 = yes)
    //   regsE[0-2] = result point (x, y, z as floats)
    //   regsE[3] = result code (0 = failed, 1 = success)
    //   labelF = control point entries (array of valueA VectorXYZTF structures)
    {0xF8F2, "compute_bezier_curve_point", "load_unk_data", {I32, FLOAT32, FLOAT32, I32, {W_REG_SET_FIXED, 4}, {LABEL16, Arg::DataType::VECTOR4F_LIST}}, F_V3_V4 | F_ARGS},

    // Creates a timed particle effect. Like the particle opcode, but the location duration are floats.
    //   regsA[0-2] = location (x, y, z as floats)
    //   valueB = effect type
    //   valueC = duration as float (in frames; 30 frames/sec)
    {0xF8F3, "particle2", nullptr, {{R_REG_SET_FIXED, 3}, I32, FLOAT32}, F_V3_V4 | F_ARGS},

    // Converts the integer in regB into a float in regA.
    {0xF901, "dec2float", nullptr, {W_REG, R_REG}, F_V3_V4},

    // Converts the float in regB into an integer in regA.
    {0xF902, "float2dec", nullptr, {W_REG, R_REG}, F_V3_V4},

    // These are the same as let and leti. Nominally regB/valueB should be a float, but the implementation treats it as
    // an int (which is still correct, since the float is already encoded before the handler function is called).
    {0xF903, "flet", "floatlet", {W_REG, R_REG}, F_V3_V4},
    {0xF904, "fleti", "floati", {W_REG, FLOAT32}, F_V3_V4},

    // regA += regB (or valueB), as floats
    {0xF908, "fadd", nullptr, {W_REG, R_REG}, F_V3_V4},
    {0xF909, "faddi", nullptr, {W_REG, FLOAT32}, F_V3_V4},

    // regA -= regB (or valueB), as floats
    {0xF90A, "fsub", nullptr, {W_REG, R_REG}, F_V3_V4},
    {0xF90B, "fsubi", nullptr, {W_REG, FLOAT32}, F_V3_V4},

    // regA *= regB (or valueB), as floats
    {0xF90C, "fmul", nullptr, {W_REG, R_REG}, F_V3_V4},
    {0xF90D, "fmuli", nullptr, {W_REG, FLOAT32}, F_V3_V4},

    // regA /= regB (or valueB), as floats
    {0xF90E, "fdiv", nullptr, {W_REG, R_REG}, F_V3_V4},
    {0xF90F, "fdivi", nullptr, {W_REG, FLOAT32}, F_V3_V4},

    // Returns the number of times a player has ever died - not just in the current quest/game/session!
    //   regA = client ID
    //   regB = returned death count
    {0xF910, "get_total_deaths", "get_unknown_count?", {CLIENT_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Returns the stack size of the specified item in the player's inventory.
    //   regsA[0] = client ID
    //   regsA[1-3] = item.data1[0-2]
    //   regB = returned amount of item present in player's inventory, or 0 if not found
    {0xF911, "get_stackable_item_count", "get_item_count", {{R_REG_SET_FIXED, 4}, W_REG}, F_V3_V4},

    // Freezes a character and hides their equips, or does the opposite. Internally, this toggles the disable-update
    // flag on TL_03.
    {0xF912, "toggle_freeze_and_hide_equip", "freeze_and_hide_equip", {}, F_V3_V4},

    // Undoes the effect of toggle_freeze_and_hide_equip, if it has been run an odd number of times. (Otherwise, does
    // nothing.)
    {0xF913, "thaw_and_show_equip", nullptr, {}, F_V3_V4},

    // These opcodes are generally used in the following sequence:
    //   set_palettex_callback   r:client_id, callback_label
    //   activate_palettex       r:client_id
    //   enable_palettex         r:client_id
    //   ... (X button now runs a quest function when pressed)
    //   disable_palettex        r:client_id
    //   restore_palettex        r:client_id
    //   ... (X button now behaves normally)
    // Arguments:
    //   regA = client ID
    //   labelB (for set_palettex_callback only) = function to call when X button is pressed
    {0xF914, "set_palettex_callback", "set_paletteX_callback", {CLIENT_ID, SCRIPT16}, F_V3_V4 | F_ARGS},
    {0xF915, "activate_palettex", "activate_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF916, "enable_palettex", "enable_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF917, "restore_palettex", "restore_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF918, "disable_palettex", "disable_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Checks if activate_palettex has been run for a player.
    //   regA = client ID
    //   regB = returned flag (0 = not overridden, 1 = overridden)
    {0xF919, "get_palettex_activated", "get_paletteX_activated", {CLIENT_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Checks if activate_palettex has been run for a player.
    //   regA = client ID
    //   valueB = ignored
    //   regC = returned flag (0 = not overridden, 1 = overridden)
    // The ignored argument here seems to be a bug. At least one official quest uses this opcode preceded by only two
    // arg_push opcodes, implying that it was intended to take two arguments, but the client really does only use
    // quest_arg_list[0] and quest_arg_list[2].
    {0xF91A, "get_palettex_enabled", "get_unknown_paletteX_status?", {CLIENT_ID, I32, W_REG}, F_V3_V4 | F_ARGS},

    // Disables/enables movement for a player. Unlike disable_movement1, this does not send 6x2C.
    //   regA = client ID
    {0xF91B, "disable_movement2", nullptr, {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF91C, "enable_movement2", nullptr, {CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Returns the local player's play time in seconds.
    {0xF91D, "get_time_played", nullptr, {W_REG}, F_V3_V4},

    // Returns the number of Guild Cards saved to the Guild Card file.
    {0xF91E, "get_guildcard_total", nullptr, {W_REG}, F_V3_V4},

    // Returns the amount of Meseta the player has in both their inventory and bank.
    //   regsA[0] = returned Meseta amount in inventory
    //   regsA[1] = returned Meseta amount in bank
    {0xF91F, "get_slot_meseta", nullptr, {{W_REG_SET_FIXED, 2}}, F_V3_V4},

    // Returns a player's level.
    //   valueA = client ID
    //   regB = returned level
    {0xF920, "get_player_level", nullptr, {CLIENT_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Returns a player's section ID.
    //   valueA = client ID
    //   regB = returned section ID (see name_to_section_id in StaticGameData.cc)
    {0xF921, "get_section_id", "get_Section_ID", {CLIENT_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Returns a player's maximum and current HP and TP.
    //   valueA = client ID
    //   regsB[0] = returned maximum HP
    //   regsB[1] = returned current HP
    //   regsB[2] = returned maximum TP
    //   regsB[3] = returned current TP
    // If there's no player in the given slot, the returned values are all FFFFFFFF.
    {0xF922, "get_player_hp_tp", "get_player_hp", {CLIENT_ID, {W_REG_SET_FIXED, 4}}, F_V3_V4 | F_ARGS},

    // Returns the floor and room ID of the given player.
    //   valueA = client ID
    //   regsB[0] = returned floor number
    //   regsB[1] = returned room number
    // If there's no player in the given slot, the returned values are both FFFFFFFF.
    {0xF923, "get_player_room", "get_floor_number", {CLIENT_ID, {W_REG_SET_FIXED, 2}}, F_V3_V4 | F_ARGS},

    // Checks if each player (individually) is near the given location.
    //   regsA[0-1] = location (x, z as integers; y not included)
    //   regsA[2] = radius as integer
    //   regsB[0-3] = results for each player (0 = player not present or outside radius; 1 = player within radius)
    {0xF924, "get_coord_player_detect", nullptr, {{R_REG_SET_FIXED, 3}, {W_REG_SET_FIXED, 4}}, F_V3_V4},

    // Reads the value of a quest counter.
    //   valueA = counter index (0-15)
    //   regB = returned value
    {0xF925, "read_counter", "read_global_flag", {I32, W_REG}, F_V3_V4 | F_ARGS},

    // Writes a value to a quest counter.
    //   valueA = counter index (0-15)
    //   valueB = value to write
    {0xF926, "write_counter", "write_global_flag", {I32, I32}, F_V3_V4 | F_ARGS},

    // Checks if an item exists in the local player's bank. The matching logic is the same as in find_inventory_item.
    //   regsA[0-2] = item.data1[0-2]
    //   regsA[3] = item.data1[4]
    //   regB = 1 if item was found, 0 if not
    {0xF927, "find_bank_item", "item_check_bank", {{R_REG_SET_FIXED, 4}, W_REG}, F_V3_V4},

    // Returns whether each player is present.
    //   regsA[0-3] = returned flags (for each player: 0 if absent, 1 if present)
    {0xF928, "get_players_present", "floor_player_detect", {{W_REG_SET_FIXED, 4}}, F_V3_V4},

    // Prepares to load a GBA ROM from a local GSL file. Does nothing on Xbox and BB.
    {0xF929, "prepare_gba_rom_from_disk", "read_disk_file?", {CSTRING}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_ARGS},
    {0xF929, "nop_F929", nullptr, {CSTRING}, F_XB_V3 | F_V4 | F_ARGS},

    // Opens a window for the player to choose an item.
    {0xF92A, "open_pack_select", nullptr, {}, F_V3_V4},

    // Prevents the player from choosing a specific item in the item select window opened by open_pack_select.
    // Generally used for subsequent item choices when trading multiple items.
    //   regA = item ID
    {0xF92B, "prevent_item_select", "item_select", {R_REG}, F_V3_V4},

    // Returns the item chosen by the player in an open_pack_select window, or 0xFFFFFFFF if they canceled it.
    {0xF92C, "get_chosen_item_id", "get_item_id", {W_REG}, F_V3_V4},

    // Adds a color overlay on the player's screen. The overlay fades in linearly over the given number of frames. The
    // overlay is not deleted until the player changes areas or leaves the game, but it can be overwritten with another
    // overlay with this same opcode. The overlay is under 2-dimensional objects like the HUD, pause menu, minimap, and
    // text messages from the server, but is above everything else.
    //   valueA, valueB, valueC, valueD = red, green, blue, alpha components of color (00-FF each)
    //   valueE = fade speed (number of frames; 30 frames/sec)
    {0xF92D, "add_color_overlay", "color_change", {I32, I32, I32, I32, I32}, F_V3_V4 | F_ARGS},

    // Sends a statistic to the server via the AA command. The server is expected to respond with an AB command
    // containing one of the label indexes set by prepare_statistic. The arguments to this opcode are sent verbatim in
    // the params field of the AA command; the other fields in the AA command come from prepare_statistic.
    {0xF92E, "send_statistic", "send_statistic?", {I32, I32, I32, I32, I32, I32, I32, I32}, F_V3_V4 | F_ARGS},

    // Enables patching a GBA ROM before sending it to the GBA. valueA is ignored. If valueB is 1, the game writes two
    // 32-bit values to the GBA ROM data before sending it:
    //   At offset 0x2C0, writes system_file->creation_timestamp
    //   At offset 0x2C4, writes current_time + rand(0, 100)
    // current_time is in seconds since 00:00:00 on 1 January 2000. On Xbox and BB, this opcode does nothing.
    {0xF92F, "gba_write_identifiers", "gba_unknown5", {I32, I32}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_ARGS},
    {0xF92F, "nop_F92F", nullptr, {I32, I32}, F_XB_V3 | F_V4 | F_ARGS},

    // Shows a message in a chat window. Can be closed with the winend opcode.
    //   valueA = X position on screen
    //   valueB = Y position on screen
    //   valueC = window width
    //   valueD = window height
    //   valueE = window style (0 = white window, 1 = chat box, anything else = no window, just text)
    //   strF = initial message
    {0xF930, "chat_box", nullptr, {I32, I32, I32, I32, I32, CSTRING}, F_V3_V4 | F_ARGS},

    // Shows a chat bubble linked to the given entity ID.
    //   valueA = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    //   strB = message
    {0xF931, "chat_bubble", nullptr, {I32, CSTRING}, F_V3_V4 | F_ARGS},

    // Sets the episode to be loaded the next time an area is loaded (e.g. by the player changing floors). regA is the
    // same as for set_episode. Like set_episode, it resets the floor configuration to the defaults, but this happens
    // at the time the player changes floors, not when the opcode is executed.
    {0xF932, "delayed_switch_episode", "set_episode2", {R_REG}, F_V3_V4},

    // Sets the rank prizes in offline challenge mode.
    //   regsA[0] = rank (unusual value order: 0 = S, 1 = B, 2 = A)
    //   regsA[1-6] = item.data1[0-5]
    {0xF933, "item_create_multi_cm", "unknownF933", {{R_REG_SET_FIXED, 7}}, F_V3},
    {0xF933, "nop_F933", nullptr, {{R_REG_SET_FIXED, 7}}, F_V4},

    // Shows a scrolling text window.
    //   valueA = X position on screen
    //   valueB = Y position on screen
    //   valueC = window width
    //   valueD = window height
    //   valueE = window style (same as for chat_box)
    //   valueF = scrolling speed
    //   regG = set to 1 when message has entirely scrolled past
    //   strH = message
    {0xF934, "scroll_text", nullptr, {I32, I32, I32, I32, I32, FLOAT32, W_REG, CSTRING}, F_V3_V4 | F_ARGS},

    // Creates, destroys, or updates the GBA loading progress bar (same as the quest download progress bar). These
    // opcodes do nothing on Xbox and BB.
    {0xF935, "gba_create_dl_graph", "gba_unknown1", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF935, "nop_F935", nullptr, {}, F_XB_V3 | F_V4},
    {0xF936, "gba_destroy_dl_graph", "gba_unknown2", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF936, "nop_F936", nullptr, {}, F_XB_V3 | F_V4},
    {0xF937, "gba_update_dl_graph", "gba_unknown3", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF937, "nop_F937", nullptr, {}, F_XB_V3 | F_V4},

    // Damages a player.
    //   regA = client ID
    //   regB = amount
    {0xF938, "add_damage_to", "add_damage_to?", {I32, I32}, F_V3_V4 | F_ARGS},

    // Deletes an item from the local player's inventory. Like item_delete, but doesn't return anything.
    //   valueA = item ID
    {0xF939, "item_delete_noreturn", "item_delete_slot", {I32}, F_V3_V4 | F_ARGS},

    // Returns the item data for an item chosen with open_pack_select.
    //   valueA = item ID
    //   regsB[0-11] = returned item.data1[0-11]
    // If the item doesn't exist, regsB[0] is set to 0xFFFFFFFF and the rest of regsB are unaffected.
    {0xF93A, "get_item_info", nullptr, {ITEM_ID, {W_REG_SET_FIXED, 12}}, F_V3_V4 | F_ARGS},

    // Wraps an item in the player's inventory. The specified item is deleted and a new one is created with the wrapped
    // flag set. The new item has a different item ID, which is not returned. Sends 6x29, then 6x26 if deleting the
    // item causes the player's equipped weapon to no longer be usable, then 6x2B. It appears Sega did not intend for
    // this to be used on BB, since the behavior wasn't changed - normally, item-related functions should be done by
    // the server on BB, as the following opcode does.
    //   valueA = item ID
    {0xF93B, "wrap_item", "item_packing1", {ITEM_ID}, F_V3_V4 | F_ARGS},

    // Like wrap_item, but also sets the present color. On BB, sends 6xD6, and the server should respond with the
    // sequence of 6x29, 6x26 (if it was equipped), then 6xBE.
    //   valueA = item ID
    //   valueB = present color (0-15; high 4 bits are masked out)
    {0xF93C, "wrap_item_with_color", "item_packing2", {ITEM_ID, I32}, F_V3_V4 | F_ARGS},

    // Returns the local player's language setting. For values, see name_for_language in StaticGameData.cc.
    {0xF93D, "get_lang_setting", "get_lang_setting?", {W_REG}, F_V3_V4 | F_ARGS},

    // Sets some values to be sent to the server with send_statistic.
    //   valueA = stat_id (used in send_statistic); this is set to the quest number from the header when the quest
    //     starts, but that is overwritten by prepare_statistic
    //   labelB = label1 (used in send_statistic)
    //   labelC = label2 (used in send_statistic)
    {0xF93E, "prepare_statistic", "prepare_statistic?", {I32, SCRIPT32, SCRIPT32}, F_V3_V4 | F_ARGS},

    // Enables use of the check_for_keyword opcode.
    {0xF93F, "enable_keyword_detect", "keyword_detect", {}, F_V3_V4},

    // Checks if a word was said by a specific player. All of the same behaviors surrounding the message string apply
    // as for set_chat_callback. Generally, this should be called every frame until the keyword is found (or the quest
    // no longer cares if it is), since it will only return a match on the first frame that a string is said.
    //   regA = result (0 = word not said, 1 = word said)
    //   valueB = client ID
    //   strC = string to match (same semantics as for set_chat_callback)
    {0xF940, "check_for_keyword", "keyword", {W_REG, CLIENT_ID, CSTRING}, F_V3_V4 | F_ARGS},

    // Returns a player's Guild Card number.
    //   valueA = client ID
    //   regB = returned Guild Card number
    {0xF941, "get_guildcard_num", nullptr, {CLIENT_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Returns the last symbol chat that a player said (at least, since the capture buffer was created). Use
    // create_symbol_chat_capture_buffer before using this opcode. The data is returned as raw values in a sequence of
    // 15 registers, in the same internal format as the game uses. On GC, this is SymbolChatBE; on all other platforms,
    // it is SymbolChat (see SymbolChatT in PlayerSubordinates.hh for details).
    //   valueA = client ID
    //   regsB[0-14] = returned symbol chat data
    {0xF942, "get_recent_symbol_chat", "symchat_unknown", {I32, {W_REG_SET_FIXED, 15}}, F_V3_V4 | F_ARGS},

    // Creates the capture buffer required by get_recent_symbol_chat.
    {0xF943, "create_symbol_chat_capture_buffer", "unknownF943", {}, F_V3_V4},

    // Checks whether an item is stackable.
    //   valueA = item ID
    //   regB = result (0 = not stackable, 1 = stackable, FFFFFFFF = not found)
    {0xF944, "get_item_stackability", "get_wrap_status", {ITEM_ID, W_REG}, F_V3_V4 | F_ARGS},

    // Sets the floor where the players will start. This generally should be used in the start label (where
    // map_designate, etc. are used). This is exactly the same as ba_initial_floor, except the floor numbers are not
    // remapped: in Episode 1, 0 means Pioneer 2, 1 means Forest 1, etc. The warning on ba_initial_floor about common
    // and rare item tables also applies to this opcode.
    {0xF945, "initial_floor", nullptr, {I32}, F_V3_V4 | F_ARGS},

    // Computes the sine, cosine, or tangent of the input angle. Angles are in the range [0, 65535].
    //   regA = result value
    //   valueB = input angle
    {0xF946, "sin", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},
    {0xF947, "cos", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},
    {0xF948, "tan", nullptr, {W_REG, I32}, F_V3_V4 | F_ARGS},

    // Computes the arctangent of the input ratio.
    //   regA = result (integer angle, 0-65535; C expression: (int)((atan2(valueB, valueC) * 65536.0) / (2 * M_PI)))
    //   valueB = numerator as float
    //   valueC = denominator as float
    {0xF949, "atan2_int", "atan", {W_REG, FLOAT32, FLOAT32}, F_V3_V4 | F_ARGS},

    // Sets regA to 1 if Olga Flow has been defeated, or 0 otherwise.
    {0xF94A, "olga_flow_is_dead", "olga_is_dead", {W_REG}, F_V3_V4},

    // Creates a timed particle effect. Similar to the particle opcode, but the created particles have no draw
    // distance, so they are visible from very far away.
    //   regsA[0-2] = location (x, y, z as integers)
    //   regsA[3] = effect type
    //   regsA[4] = duration (in frames; 30 frames/sec)
    {0xF94B, "particle_effect_nc", "particle3", {{R_REG_SET_FIXED, 5}}, F_V3_V4},

    // Creates a particle effect on a given entity. Similar to the particle_id opcode, but the created particles have
    // no draw distance, so they are visible from very far away.
    //   regsA[0] = effect type
    //   regsA[1] = duration in frames
    //   regsA[2] = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    //   regsA[3] = y offset (as integer)
    {0xF94C, "player_effect_nc", "particle3f_id", {{R_REG_SET_FIXED, 4}}, F_V3_V4},

    // Returns 1 in regA if a file named PSO3_CHARACTER is present on either memory card. This opcode is only available
    // on PSO Plus on GC; that is, it only exists on JP v1.4, JP v1.5, and US v1.2.
    {0xF94D, "has_ep3_save_file", nullptr, {W_REG}, F_GC_V3 | F_ARGS},

    // Gives the player one copy of a card. regA is the card ID.
    {0xF94D, "give_card", "is_there_cardbattle?", {R_REG}, F_GC_EP3TE},

    // Gives the player one copy of a card, or takes one copy away.
    //   regsA[0] = card_id
    //   regsA[1] = action (give card if >= 0, take card if < 0)
    {0xF94D, "give_or_take_card", "is_there_cardbattle?", {{R_REG_SET_FIXED, 2}}, F_GC_EP3},

    // TODO(DX): Related to voice chat, but functionality is unknown. valueA is a client ID; a value is read from that
    // player's TVoiceChatClient object and (!!value) is placed in regB. This value is set by the 6xB3 command.
    {0xF94D, "unknown_F94D", nullptr, {I32, W_REG}, F_XB_V3 | F_ARGS},

    // These opcodes all do nothing on BB. F94D is presumably the voice chat opcode from Xbox, which was removed, but
    // it's not clear what the other two might have originally been.
    {0xF94D, "nop_F94D", nullptr, {}, F_V4},
    {0xF94E, "nop_F94E", nullptr, {}, F_V4},
    {0xF94F, "nop_F94F", nullptr, {}, F_V4},

    // Opens one of the Pioneer 2 counter menus, specified by valueA. Values:
    //   0 = weapon shop
    //   1 = medical center
    //   2 = armor shop
    //   3 = item shop
    //   4 = Hunter's Guild quest counter
    //   5 = bank
    //   6 = tekker
    //   7 = government quest counter
    //   8 = Momoka item exchange (this opens a menu displaying the items specified in the quest header, which sends
    //       6xD9 when the player chooses an item)
    // valueA is not bounds-checked, so it could be used to write a byte with the value 1 anywhere in memory.
    {0xF950, "bb_p2_menu", "BB_p2_menu", {I32}, F_V4 | F_ARGS},

    // Behaves exactly the same as map_designate_ex, but the arguments are specified as immediate values and not via
    // registers or arg_push. Sega probably added this opcode so their quest authoring tools could easily generate the
    // necessary header fields without doing any fancy script analysis.
    //   valueA = floor number
    //   valueB = area number
    //   valueC = type (0: use layout, 1: use offline template, 2: use online template, 3: nothing)
    //   valueD = major variation
    //   valueE = minor variation
    {0xF951, "bb_map_designate", "BB_Map_Designate", {I8, I8, I8, I8, I8}, F_V4},

    // Returns the number of items in the player's inventory.
    {0xF952, "bb_get_number_in_pack", "BB_get_number_in_pack", {W_REG}, F_V4},

    // Requests an item exchange in the player's inventory. Sends 6xD5. The requested item must match one of the item
    // creation masks in the quest script's header.
    //   valueA/valueB/valueC = item.data1[0-2] to search for
    //   valueD/valueE/valueF = item.data1[0-2] to replace it with
    //   labelG = label to call on success
    //   labelH = label to call on failure
    {0xF953, "bb_swap_item", "BB_swap_item", {I32, I32, I32, I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Checks if an item can be wrapped.
    //   valueA = item ID
    //   regB = returned status (0 = can't be wrapped, 1 = can be wrapped, 2 = item not found)
    {0xF954, "bb_check_wrap", "BB_check_wrap", {I32, W_REG}, F_V4 | F_ARGS},

    // Requests an item exchange for Photon Drops. Sends 6xD7. The requested item must match one of the item creation
    // masks in the quest script's header.
    //   valueA/valueB/valueC = item.data1[0-2] for requested item
    //   labelD = label to call on success
    //   labelE = label to call on failure
    {0xF955, "bb_exchange_pd_item", "BB_exchange_PD_item", {I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Requests an S-rank special upgrade in exchange for Photon Drops. Sends 6xD8.
    //   valueA = item ID
    //   valueB/valueC/valueD = item.data1[0-2]
    //   valueE = special type
    //   labelF = label to call on success
    //   labelG = label to call on failure
    {0xF956, "bb_exchange_pd_srank", "BB_exchange_PD_srank", {I32, I32, I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Requests a weapon attribute upgrade in exchange for Photon Drops. Sends 6xDA.
    //   valueA = item ID
    //   valueB/valueC/valueD = item.data1[0-2]
    //   valueE = attribute to upgrade
    //   valueF = payment count (number of PDs)
    //   labelG = label to call on success
    //   labelH = label to call on failure
    {0xF957, "bb_exchange_pd_percent", "BB_exchange_PD_special", {I32, I32, I32, I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Requests a weapon attribute upgrade in exchange for Photon Spheres. Sends 6xDA. Same arguments as
    // bb_exchange_pd_percent, except Photon Spheres are used instead.
    {0xF958, "bb_exchange_ps_percent", "BB_exchange_PS_percent", {I32, I32, I32, I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Determines whether the Episode 4 boss can escape if undefeated after 20 minutes.
    //   valueA = boss can escape (0 = no, 1 = yes (default))
    {0xF959, "bb_set_ep4_boss_can_escape", "BB_set_ep4boss_can_escape", {I32}, F_V4 | F_ARGS},

    // Returns 1 if the Episode 4 boss death cutscene is playing, or 0 if not (even if the boss is already defeated).
    {0xF95A, "bb_is_ep4_boss_dying", nullptr, {W_REG}, F_V4},

    // Requests an item exchange. Sends 6xD9. The requested item must match one of the item creation masks in the quest
    // script's header.
    //   valueA = find_item.data1[0-2] (low 3 bytes; high byte unused)
    //   valueB = replace_item.data1[0-2] (low 3 bytes; high byte unused)
    //   valueC = token1 (see 6xD9 in CommandFormats.hh)
    //   valueD = token2 (see 6xD9 in CommandFormats.hh)
    //   labelE = label to call on success
    //   labelF = label to call on failure
    {0xF95B, "bb_replace_item", "bb_send_6xD9", {I32, I32, I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Requests an exchange of Secret Lottery Tickets for items. Sends 6xDE. The pool of items that can be returned by
    // this opcode is determined by the quest's header; newserv assembles/disassembles this field as .allow_create_item
    // directives. The first entry in the header is the currency item (which is to be deleted from the inventory); the
    // others are the items the server randomly chooses from.
    //   valueA = index
    //   valueB = unknown_a1
    //   labelC = label to call on success
    //   labelD = label to call on failure (unused because of a client bug; see 6xDE description in CommandFormats.hh)
    {0xF95C, "bb_exchange_slt", "BB_exchange_SLT", {I32, I32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Removes a Photon Crystal from the player's inventory, and disables drops for the rest of the quest. Sends 6xDF.
    {0xF95D, "bb_exchange_pc", "BB_exchange_PC", {}, F_V4},

    // Requests an item drop within a quest. Sends 6xE0.
    //   valueA = type (corresponds to QuestF95EResultItems in config.json)
    //   valueB/valueC = x, z coordinates as floats
    {0xF95E, "bb_box_create_bp", "BB_box_create_BP", {I32, FLOAT32, FLOAT32}, F_V4 | F_ARGS},

    // Requests an exchange of Photon Tickets for items. Sends 6xE1.
    //   regA = result code reg (set to result code upon server response); the result codes are:
    //     0 = success
    //     1 = player doesn't have enough Photon Tickets
    //     2 = inventory is full
    //     3, 4, or 5 = "server send error" (from Gallon's Plan script; newserv never sends these)
    //   regB = result index reg (set to valueC upon server response)
    //   valueC = result index (index into QuestF95FResultItems in config.json)
    //   labelD = label to call on success
    //   labelE = label to call on failure
    {0xF95F, "bb_exchange_pt", "BB_exchage_PT", {W_REG32, W_REG32, I32, I32, I32}, F_V4 | F_ARGS},

    // Requests a prize from the Meseta gambling prize list. Sends 6xE2. The server responds with 6xE3, which sets the
    // <meseta_slot_prize> replacement token in message strings. The status of this can be checked with
    // bb_get_6xE3_status.
    //   valueA = result tier (see QuestF960SuccessResultItems and QuestF960FailureResultItems in config.json)
    {0xF960, "bb_send_6xE2", "unknownF960", {I32}, F_V4 | F_ARGS},

    // Returns the status of the expected 6xE3 command from a preceding bb_send_6xE2 opcode. Return values:
    //   0 = 6xE3 hasn't been received
    //   1 = the received item is valid
    //   2 = the received item is invalid, or the item ID was already in use
    {0xF961, "bb_get_6xE3_status", "unknownF961", {W_REG}, F_V4},
};

static const unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>& opcodes_for_version(Version v) {
  static array<
      unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>,
      static_cast<size_t>(Version::BB_V4) + 1>
      indexes;

  auto& index = indexes.at(static_cast<size_t>(v));
  if (index.empty()) {
    uint32_t vf = v_flag(v);
    for (size_t z = 0; z < sizeof(opcode_defs) / sizeof(opcode_defs[0]); z++) {
      const auto& def = opcode_defs[z];
      if (!(def.flags & vf)) {
        continue;
      }
      if (!index.emplace(def.opcode, &def).second) {
        throw logic_error(std::format("duplicate definition for opcode {:04X}", def.opcode));
      }
    }
  }
  return index;
}

static const unordered_map<string, const QuestScriptOpcodeDefinition*>& opcodes_by_name_for_version(Version v) {
  static array<unordered_map<string, const QuestScriptOpcodeDefinition*>, static_cast<size_t>(Version::BB_V4) + 1> indexes;

  auto& index = indexes.at(static_cast<size_t>(v));
  if (index.empty()) {
    uint32_t vf = v_flag(v);
    for (size_t z = 0; z < sizeof(opcode_defs) / sizeof(opcode_defs[0]); z++) {
      const auto& def = opcode_defs[z];
      if (!(def.flags & vf)) {
        continue;
      }
      if (def.name && !index.emplace(phosg::tolower(def.name), &def).second) {
        throw logic_error(std::format("duplicate definition for opcode {:04X}", def.opcode));
      }
      if (def.qedit_name) {
        string lower_qedit_name = phosg::tolower(phosg::tolower(def.qedit_name));
        if ((lower_qedit_name != def.name) && !index.emplace(lower_qedit_name, &def).second) {
          throw logic_error(std::format("duplicate definition for opcode {:04X}", def.opcode));
        }
      }
    }
  }
  return index;
}

void check_quest_opcode_definitions() {
  static const array<Version, 12> versions = {
      Version::DC_NTE,
      Version::DC_11_2000,
      Version::DC_V1,
      Version::DC_V2,
      Version::PC_NTE,
      Version::PC_V2,
      Version::GC_NTE,
      Version::GC_V3,
      Version::GC_EP3_NTE,
      Version::GC_EP3,
      Version::XB_V3,
      Version::BB_V4,
  };
  for (Version v : versions) {
    const auto& opcodes_by_name = opcodes_by_name_for_version(v);
    const auto& opcodes = opcodes_for_version(v);
    phosg::log_info_f("Version {} has {} opcodes with {} mnemonics",
        phosg::name_for_enum(v), opcodes.size(), opcodes_by_name.size());
  }
}

CreateItemMaskEntry::CreateItemMaskEntry(const QuestMetadata::CreateItemMask& mask) {
  for (size_t z = 0; z < 12; z++) {
    auto& r = mask.data1_ranges[z];
    if (r.min == r.max) {
      this->data1_fields[z] = r.min;
    } else if (r.min == 0x00 && r.max == 0xFF) {
      this->data1_fields[z] = -1;
    } else {
      this->data1_fields[z] = 1000000 + (r.max * 1000) + r.min;
    }
  }
  this->present = this->is_valid();
}

CreateItemMaskEntry::operator QuestMetadata::CreateItemMask() const {
  using Range = QuestMetadata::CreateItemMask::Range;

  QuestMetadata::CreateItemMask ret;
  for (size_t z = 0; z < 12; z++) {
    int32_t v = this->data1_fields[z];

    if (v < 0) {
      // If v is negative, any value is allowed in this field
      ret.data1_ranges[z] = Range{.min = 0x00, .max = 0xFF};

    } else if (v < 0x100) {
      // If v fits in an unsigned byte, this field must match exactly
      ret.data1_ranges[z] = Range{.min = static_cast<uint8_t>(v), .max = static_cast<uint8_t>(v)};

    } else if (v >= 1000000 && v <= 2000000) {
      // Otherwise, the allowed range of values is encoded in decimal as 1MMMmmm (m = min, M = max)
      uint32_t min = v % 1000;
      uint32_t max = (v / 1000) % 1000;
      if ((min > 0xFF) || (max > 0xFF) || (min > max)) {
        throw std::runtime_error(std::format("invalid range spec {} (0x{:X})", v, v));
      }
      ret.data1_ranges[z] = Range{.min = static_cast<uint8_t>(min), .max = static_cast<uint8_t>(max)};

    } else {
      throw std::runtime_error(std::format("invalid range spec {} (0x{:X})", v, v));
    }
  }

  return ret;
}

std::string disassemble_quest_script(
    const void* bin_data,
    size_t bin_size,
    Version version,
    Language language,
    shared_ptr<const MapFile> dat,
    bool reassembly_mode,
    bool use_qedit_names) {

  phosg::StringReader r(bin_data, bin_size);
  deque<string> lines;
  lines.emplace_back(std::format(".version {}", phosg::name_for_enum(version)));

  // Phase 0: Parse the header and generate the metadata section

  QuestMetadata meta;
  populate_quest_metadata_from_script(meta, bin_data, bin_size, version, language);
  if (version != Version::DC_NTE) {
    lines.emplace_back(std::format(".quest_num {}", meta.quest_number));
  }
  lines.emplace_back(".name " + escape_string(meta.name));
  if (version != Version::DC_NTE) {
    lines.emplace_back(std::format(".short_desc {}", escape_string(meta.short_description)));
    lines.emplace_back(std::format(".long_desc {}", escape_string(meta.long_description)));
  }
  if (!is_v1_or_v2(version) || (version == Version::GC_NTE)) {
    lines.emplace_back(std::format(".episode {}", token_name_for_episode(meta.episode)));
    if (meta.header_episode >= 0) {
      lines.emplace_back(std::format(".header_episode 0x{:02X}", meta.header_episode));
    }
  }
  lines.emplace_back(std::format(".language {}", char_for_language(meta.language)));
  if (!is_pre_v1(version) && !is_v4(version) && (meta.header_language >= 0) && (static_cast<uint8_t>(meta.language) != meta.header_language)) {
    lines.emplace_back(std::format(".header_language 0x{:02X}", meta.header_language));
  }
  if (is_v4(version)) {
    lines.emplace_back(std::format(".max_players {}", meta.max_players));
    if (meta.joinable) {
      lines.emplace_back(".joinable");
    }
    for (uint16_t flag : meta.solo_unlock_flags) {
      lines.emplace_back(std::format(".solo_unlock_flag 0x{:04X}", flag));
    }
    for (const auto& mask : meta.create_item_mask_entries) {
      lines.emplace_back(std::format(".allow_create_item {}", mask.str()));
    }
  }
  lines.emplace_back(std::format(".header_unknown_a1 0x{:04X}", meta.header_unknown_a1));
  lines.emplace_back(std::format(".header_unknown_a2 0x{:04X}", meta.header_unknown_a2));
  if (!is_pre_v1(version) && !is_v4(version)) {
    lines.emplace_back(std::format(".header_unknown_a3 0x{:02X}", meta.header_unknown_a3));
  }
  if (is_v4(version)) {
    lines.emplace_back(std::format(".header_unknown_a4 0x{:02X}", meta.header_unknown_a4));
    lines.emplace_back(std::format(".header_unknown_a5 0x{:08X}", meta.header_unknown_a5));
    lines.emplace_back(std::format(".header_unknown_a6 0x{:04X}", meta.header_unknown_a6));
  }
  lines.emplace_back();

  // Phase 1: Parse label table

  phosg::StringReader text_r, label_table_r, extra_r;
  if (meta.label_table_offset >= meta.total_size) {
    throw std::runtime_error("label table is beyond end of file");
  }
  if (meta.text_offset >= meta.total_size) {
    throw std::runtime_error("text is beyond end of file");
  }
  if (meta.text_offset < meta.label_table_offset) {
    text_r = r.sub(meta.text_offset, meta.label_table_offset - meta.text_offset);
    label_table_r = r.sub(meta.label_table_offset, meta.total_size - meta.label_table_offset);
  } else {
    label_table_r = r.sub(meta.label_table_offset, meta.text_offset - meta.label_table_offset);
    text_r = r.sub(meta.text_offset, meta.total_size - meta.text_offset);
  }
  extra_r = r.sub(meta.total_size);

  struct Label {
    struct ObjectSetRef {
      uint8_t floor;
      size_t set_index;
      const MapFile::ObjectSetEntry* obj_set;
    };
    struct EnemySetRef {
      uint8_t floor;
      size_t set_index;
      const MapFile::EnemySetEntry* ene_set;
    };

    set<uint32_t> label_nums;
    uint32_t offset;
    uint32_t size = 0;
    set<size_t> script_refs;
    vector<ObjectSetRef> object_refs;
    vector<EnemySetRef> enemy_refs;
    Arg::DataType type = Arg::DataType::NONE;
    deque<string> lines;
  };

  vector<shared_ptr<Label>> label_table;
  map<size_t, shared_ptr<Label>> offset_to_label;
  deque<pair<shared_ptr<Label>, Arg::DataType>> pending_labels;
  while (!label_table_r.eof()) {
    try {
      uint32_t offset = label_table_r.get_u32l();
      if (offset < text_r.size()) {
        shared_ptr<Label> l;
        try {
          l = offset_to_label.at(offset);
        } catch (const std::out_of_range&) {
          l = make_shared<Label>();
          l->offset = offset;
          if (label_table.size() == 0) {
            pending_labels.emplace_back(make_pair(l, Arg::DataType::SCRIPT));
          }
          offset_to_label.emplace(l->offset, l);
        }
        l->label_nums.emplace(label_table.size());
        label_table.emplace_back(l);
      } else {
        label_table.emplace_back(nullptr);
      }
    } catch (const out_of_range&) {
      label_table_r.skip(label_table_r.remaining());
    }
  }
  // If there's no label at offset 0, make a fake one, but don't put it in the label table because the script cannot
  // reference it
  if (!offset_to_label.count(0)) {
    auto l = make_shared<Label>();
    l->offset = 0;
    offset_to_label.emplace(l->offset, l);
  }

  // Phase 2: Calculate the size of each label's data

  {
    auto it = offset_to_label.begin();
    while (it != offset_to_label.end()) {
      auto next_it = it;
      next_it++;
      it->second->size = ((next_it == offset_to_label.end()) ? text_r.size() : next_it->second->offset) - it->second->offset;
      it = next_it;
    }
  }

  // Phase 3: Collect label references from the map, if present

  if (dat) {
    for (uint8_t floor = 0; floor < 0x12; floor++) {
      auto& fs = dat->floor(floor);
      if (fs.object_sets) {
        for (size_t z = 0; z < fs.object_set_count; z++) {
          const auto& obj_set = fs.object_sets[z];
          size_t label_num = label_table.size();
          switch (obj_set.base_type) {
            case 0x0012: // TObjQuestCol
            case 0x0015: // TObjQuestColA
            case 0x008B: // TObjComputer
            case 0x02B7: // TObjGbAdvance
              label_num = obj_set.param4;
              break;
            case 0x02B8: // TObjQuestColALock2
            case 0x02BA: // TObjQuestCol2
              if (obj_set.param5 <= 0) {
                label_num = obj_set.param4;
              }
              break;
            case 0x0023: // TOAttackableCol
              if (obj_set.param6 > 0) {
                label_num = obj_set.param6;
              }
              break;
            case 0x0026: // TOChatSensor
              if (obj_set.angle.x == 0) {
                label_num = obj_set.param4;
              }
              break;
            case 0x008D: // TOCapsuleAncient01
            case 0x0104: // TOComputerMachine01
            case 0x0155: // TOMonumentAncient01
            case 0x0229: // TOCapsuleLabo
              // NOTE: These objects can call quest functions in either the free play or quest script managers. We
              // assume the caller knows what they're doing, and we don't check for this.
              label_num = obj_set.param6;
              break;
          }
          if (label_num < label_table.size()) {
            auto l = label_table[label_num];
            if (l) {
              l->object_refs.emplace_back(Label::ObjectSetRef{.floor = floor, .set_index = z, .obj_set = &obj_set});
              pending_labels.emplace_back(make_pair(l, Arg::DataType::SCRIPT));
            }
          }
        }
      }
      if (fs.enemy_sets) {
        uint8_t area = meta.floor_assignments.at(floor).area;
        for (size_t z = 0; z < fs.enemy_set_count; z++) {
          // Only NPCs use script labels; no other enemies do
          const auto& ene_set = fs.enemy_sets[z];
          if ((((ene_set.base_type >= 0x0001) && (ene_set.base_type <= 0x000E)) ||
                  ((ene_set.base_type >= 0x0010) && (ene_set.base_type <= 0x0022)) ||
                  ((ene_set.base_type >= 0x0024) && (ene_set.base_type <= 0x0029)) ||
                  ((ene_set.base_type >= 0x002B) && (ene_set.base_type <= 0x002D)) ||
                  ((ene_set.base_type >= 0x0030) && (ene_set.base_type <= 0x0033)) ||
                  ((ene_set.base_type >= 0x0045) && (ene_set.base_type <= 0x0047)) ||
                  ((ene_set.base_type >= 0x00D0) && (ene_set.base_type <= 0x00D7) && (area == 0)) ||
                  ((ene_set.base_type >= 0x00F0) && (ene_set.base_type <= 0x0100)) ||
                  ((ene_set.base_type >= 0x0110) && (ene_set.base_type <= 0x0112) && (area == 0)) ||
                  (ene_set.base_type == 0x00A9) ||
                  (ene_set.base_type == 0x0118)) &&
              (ene_set.param5 > 0) && (ene_set.param5 < label_table.size())) {
            auto l = label_table[ene_set.param5];
            if (l) {
              l->enemy_refs.emplace_back(Label::EnemySetRef{.floor = floor, .set_index = z, .ene_set = &ene_set});
              pending_labels.emplace_back(make_pair(l, Arg::DataType::SCRIPT));
            }
          }
        }
      }
    }
  }
  if (!reassembly_mode) {
    lines.emplace_back(std::format("// 0x{:X} data bytes in script section with {} labels", text_r.size(), offset_to_label.size()));
    lines.emplace_back();
  }

  // Phase 4: Disassemble all referenced label regions, starting with label 0

  auto disassemble_label_as_struct = [&]<typename StructT>(shared_ptr<Label> l, auto print_fn) {
    if (reassembly_mode) {
      l->lines.emplace_back("  .data " + phosg::format_data_string(text_r.pgetv(l->offset, l->size), l->size));
    } else if (l->size >= sizeof(StructT)) {
      print_fn(text_r.pget<StructT>(l->offset));
      if (l->size > sizeof(StructT)) {
        size_t struct_end_offset = l->offset + sizeof(StructT);
        size_t remaining_size = l->size - sizeof(StructT);
        l->lines.emplace_back("  // Extra data after structure");
        l->lines.emplace_back(format_and_indent_data(text_r.pgetv(struct_end_offset, remaining_size), remaining_size, struct_end_offset));
      }
    } else {
      l->lines.emplace_back(std::format("  // As raw data (0x{:X} bytes; too small for referenced type)", l->size));
      l->lines.emplace_back(format_and_indent_data(text_r.pgetv(l->offset, l->size), l->size, l->offset));
    }
  };

  auto disassemble_label_as_data = [&](shared_ptr<Label> l) -> void {
    l->lines.emplace_back(std::format("  // As raw data (0x{:X} bytes)", l->size));
    if (reassembly_mode) {
      l->lines.emplace_back("  .data " + phosg::format_data_string(text_r.pgetv(l->offset, l->size), l->size));
    } else {
      l->lines.emplace_back(format_and_indent_data(text_r.pgetv(l->offset, l->size), l->size, l->offset));
    }
  };

  auto disassemble_label_as_cstring = [&](shared_ptr<Label> l) -> void {
    l->lines.emplace_back("  // As C string");

    string str_data = text_r.pread(l->offset, l->size);
    phosg::strip_trailing_zeroes(str_data);

    string formatted;
    size_t extra_zero_bytes;
    if (uses_utf16(version)) {
      if (str_data.size() & 1) {
        str_data.push_back(0);
      }
      extra_zero_bytes = l->size - str_data.size();
      if (extra_zero_bytes >= 2) {
        extra_zero_bytes -= 2;
      }
      formatted = escape_string(str_data, TextEncoding::UTF16);
    } else {
      extra_zero_bytes = l->size - str_data.size();
      if (extra_zero_bytes >= 1) {
        extra_zero_bytes--;
      }
      formatted = escape_string(str_data, encoding_for_language(language));
    }
    if (reassembly_mode) {
      l->lines.emplace_back(std::format("  .cstr {}", formatted));
      if (extra_zero_bytes) {
        l->lines.emplace_back(std::format("  .data {}", string(extra_zero_bytes * 2, '0')));
      }
    } else {
      l->lines.emplace_back(std::format("  {:04X}  .cstr {}", l->offset, formatted));
      if (extra_zero_bytes) {
        l->lines.emplace_back(std::format("  {:04X}  .data {}", l->offset + l->size - extra_zero_bytes, string(extra_zero_bytes * 2, '0')));
      }
    }
  };

  auto disassemble_label_as_player_visual_config = [&](shared_ptr<Label> l) -> void {
    disassemble_label_as_struct.template operator()<PlayerVisualConfig>(l, [&](const PlayerVisualConfig& visual) -> void {
      l->lines.emplace_back("  // As PlayerVisualConfig");
      l->lines.emplace_back(std::format("  {:04X}  name              {}", l->offset + offsetof(PlayerVisualConfig, name), escape_string(visual.name.decode(language))));
      l->lines.emplace_back(std::format("  {:04X}  name_color        {:08X}", l->offset + offsetof(PlayerVisualConfig, name_color), visual.name_color));
      l->lines.emplace_back(std::format("  {:04X}  a2                {}", l->offset + offsetof(PlayerVisualConfig, unknown_a2), phosg::format_data_string(visual.unknown_a2.data(), sizeof(visual.unknown_a2))));
      l->lines.emplace_back(std::format("  {:04X}  extra_model       {:02X}", l->offset + offsetof(PlayerVisualConfig, extra_model), visual.extra_model));
      l->lines.emplace_back(std::format("  {:04X}  unused            {}", l->offset + offsetof(PlayerVisualConfig, unused), phosg::format_data_string(visual.unused.data(), visual.unused.bytes())));
      l->lines.emplace_back(std::format("  {:04X}  name_color_cs     {:08X}", l->offset + offsetof(PlayerVisualConfig, name_color_checksum), visual.name_color_checksum));
      l->lines.emplace_back(std::format("  {:04X}  section_id        {:02X} ({})", l->offset + offsetof(PlayerVisualConfig, section_id), visual.section_id, name_for_section_id(visual.section_id)));
      l->lines.emplace_back(std::format("  {:04X}  char_class        {:02X} ({})", l->offset + offsetof(PlayerVisualConfig, char_class), visual.char_class, name_for_char_class(visual.char_class)));
      l->lines.emplace_back(std::format("  {:04X}  validation_flags  {:02X}", l->offset + offsetof(PlayerVisualConfig, validation_flags), visual.validation_flags));
      l->lines.emplace_back(std::format("  {:04X}  version           {:02X}", l->offset + offsetof(PlayerVisualConfig, version), visual.version));
      l->lines.emplace_back(std::format("  {:04X}  class_flags       {:08X}", l->offset + offsetof(PlayerVisualConfig, class_flags), visual.class_flags));
      l->lines.emplace_back(std::format("  {:04X}  costume           {:04X}", l->offset + offsetof(PlayerVisualConfig, costume), visual.costume));
      l->lines.emplace_back(std::format("  {:04X}  skin              {:04X}", l->offset + offsetof(PlayerVisualConfig, skin), visual.skin));
      l->lines.emplace_back(std::format("  {:04X}  face              {:04X}", l->offset + offsetof(PlayerVisualConfig, face), visual.face));
      l->lines.emplace_back(std::format("  {:04X}  head              {:04X}", l->offset + offsetof(PlayerVisualConfig, head), visual.head));
      l->lines.emplace_back(std::format("  {:04X}  hair              {:04X}", l->offset + offsetof(PlayerVisualConfig, hair), visual.hair));
      l->lines.emplace_back(std::format("  {:04X}  hair_color        {:04X}, {:04X}, {:04X}", l->offset + offsetof(PlayerVisualConfig, hair_r), visual.hair_r, visual.hair_g, visual.hair_b));
      l->lines.emplace_back(std::format("  {:04X}  proportion        {:g}, {:g}", l->offset + offsetof(PlayerVisualConfig, proportion_x), visual.proportion_x, visual.proportion_y));
    });
  };
  auto disassemble_label_as_player_stats = [&](shared_ptr<Label> l) -> void {
    disassemble_label_as_struct.template operator()<PlayerStats>(l, [&](const PlayerStats& stats) -> void {
      l->lines.emplace_back("  // As PlayerStats");
      l->lines.emplace_back(std::format("  {:04X}  atp               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.atp), stats.char_stats.atp, stats.char_stats.atp));
      l->lines.emplace_back(std::format("  {:04X}  mst               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.mst), stats.char_stats.mst, stats.char_stats.mst));
      l->lines.emplace_back(std::format("  {:04X}  evp               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.evp), stats.char_stats.evp, stats.char_stats.evp));
      l->lines.emplace_back(std::format("  {:04X}  hp                {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.hp), stats.char_stats.hp, stats.char_stats.hp));
      l->lines.emplace_back(std::format("  {:04X}  dfp               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.dfp), stats.char_stats.dfp, stats.char_stats.dfp));
      l->lines.emplace_back(std::format("  {:04X}  ata               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.ata), stats.char_stats.ata, stats.char_stats.ata));
      l->lines.emplace_back(std::format("  {:04X}  lck               {:04X} /* {} */", l->offset + offsetof(PlayerStats, char_stats.lck), stats.char_stats.lck, stats.char_stats.lck));
      l->lines.emplace_back(std::format("  {:04X}  esp               {:04X} /* {} */", l->offset + offsetof(PlayerStats, esp), stats.esp, stats.esp));
      l->lines.emplace_back(std::format("  {:04X}  attack_range      {:08X} /* {:g} */", l->offset + offsetof(PlayerStats, attack_range), stats.attack_range.load_raw(), stats.attack_range));
      l->lines.emplace_back(std::format("  {:04X}  knockback_range   {:08X} /* {:g} */", l->offset + offsetof(PlayerStats, knockback_range), stats.knockback_range.load_raw(), stats.knockback_range));
      l->lines.emplace_back(std::format("  {:04X}  level             {:08X} /* level {} */", l->offset + offsetof(PlayerStats, level), stats.level, stats.level + 1));
      l->lines.emplace_back(std::format("  {:04X}  experience        {:08X} /* {} */", l->offset + offsetof(PlayerStats, experience), stats.experience, stats.experience));
      l->lines.emplace_back(std::format("  {:04X}  meseta            {:08X} /* {} */", l->offset + offsetof(PlayerStats, meseta), stats.meseta, stats.meseta));
    });
  };
  auto disassemble_label_as_resist_data = [&](shared_ptr<Label> l) -> void {
    disassemble_label_as_struct.template operator()<ResistData>(l, [&](const ResistData& resist) -> void {
      l->lines.emplace_back("  // As ResistData");
      l->lines.emplace_back(std::format("  {:04X}  evp_bonus         {:04X} /* {} */", l->offset + offsetof(ResistData, evp_bonus), resist.evp_bonus, resist.evp_bonus));
      l->lines.emplace_back(std::format("  {:04X}  efr               {:04X} /* {} */", l->offset + offsetof(ResistData, efr), resist.efr, resist.efr));
      l->lines.emplace_back(std::format("  {:04X}  eic               {:04X} /* {} */", l->offset + offsetof(ResistData, eic), resist.eic, resist.eic));
      l->lines.emplace_back(std::format("  {:04X}  eth               {:04X} /* {} */", l->offset + offsetof(ResistData, eth), resist.eth, resist.eth));
      l->lines.emplace_back(std::format("  {:04X}  elt               {:04X} /* {} */", l->offset + offsetof(ResistData, elt), resist.elt, resist.elt));
      l->lines.emplace_back(std::format("  {:04X}  edk               {:04X} /* {} */", l->offset + offsetof(ResistData, edk), resist.edk, resist.edk));
      l->lines.emplace_back(std::format("  {:04X}  a6                {:08X} /* {} */", l->offset + offsetof(ResistData, unknown_a6), resist.unknown_a6, resist.unknown_a6));
      l->lines.emplace_back(std::format("  {:04X}  a7                {:08X} /* {} */", l->offset + offsetof(ResistData, unknown_a7), resist.unknown_a7, resist.unknown_a7));
      l->lines.emplace_back(std::format("  {:04X}  a8                {:08X} /* {} */", l->offset + offsetof(ResistData, unknown_a8), resist.unknown_a8, resist.unknown_a8));
      l->lines.emplace_back(std::format("  {:04X}  a9                {:08X} /* {} */", l->offset + offsetof(ResistData, unknown_a9), resist.unknown_a9, resist.unknown_a9));
      l->lines.emplace_back(std::format("  {:04X}  dfp_bonus         {:08X} /* {} */", l->offset + offsetof(ResistData, dfp_bonus), resist.dfp_bonus, resist.dfp_bonus));
    });
  };
  auto disassemble_label_as_attack_data = [&](shared_ptr<Label> l) -> void {
    disassemble_label_as_struct.template operator()<AttackData>(l, [&](const AttackData& attack) -> void {
      l->lines.emplace_back("  // As AttackData");
      l->lines.emplace_back(std::format("  {:04X}  atp_min           {:04X} /* {} */", l->offset + offsetof(AttackData, min_atp), attack.min_atp, attack.min_atp));
      l->lines.emplace_back(std::format("  {:04X}  atp_max           {:04X} /* {} */", l->offset + offsetof(AttackData, max_atp), attack.max_atp, attack.max_atp));
      l->lines.emplace_back(std::format("  {:04X}  ata_min           {:04X} /* {} */", l->offset + offsetof(AttackData, min_ata), attack.min_ata, attack.min_ata));
      l->lines.emplace_back(std::format("  {:04X}  ata_max           {:04X} /* {} */", l->offset + offsetof(AttackData, max_ata), attack.max_ata, attack.max_ata));
      l->lines.emplace_back(std::format("  {:04X}  distance_x        {:08X} /* {:g} */", l->offset + offsetof(AttackData, distance_x), attack.distance_x.load_raw(), attack.distance_x));
      l->lines.emplace_back(std::format("  {:04X}  angle             {:08X} /* {}/65536 */", l->offset + offsetof(AttackData, angle), attack.angle.load_raw(), attack.angle));
      l->lines.emplace_back(std::format("  {:04X}  distance_y        {:08X} /* {:g} */", l->offset + offsetof(AttackData, distance_y), attack.distance_y.load_raw(), attack.distance_y));
      l->lines.emplace_back(std::format("  {:04X}  a8                {:04X} /* {} */", l->offset + offsetof(AttackData, unknown_a8), attack.unknown_a8, attack.unknown_a8));
      l->lines.emplace_back(std::format("  {:04X}  a9                {:04X} /* {} */", l->offset + offsetof(AttackData, unknown_a9), attack.unknown_a9, attack.unknown_a9));
      l->lines.emplace_back(std::format("  {:04X}  a10               {:04X} /* {} */", l->offset + offsetof(AttackData, unknown_a10), attack.unknown_a10, attack.unknown_a10));
      l->lines.emplace_back(std::format("  {:04X}  a11               {:04X} /* {} */", l->offset + offsetof(AttackData, unknown_a11), attack.unknown_a11, attack.unknown_a11));
      l->lines.emplace_back(std::format("  {:04X}  a12               {:08X} /* {} */", l->offset + offsetof(AttackData, unknown_a12), attack.unknown_a12, attack.unknown_a12));
      l->lines.emplace_back(std::format("  {:04X}  a13               {:08X} /* {} */", l->offset + offsetof(AttackData, unknown_a13), attack.unknown_a13, attack.unknown_a13));
      l->lines.emplace_back(std::format("  {:04X}  a14               {:08X} /* {} */", l->offset + offsetof(AttackData, unknown_a14), attack.unknown_a14, attack.unknown_a14));
      l->lines.emplace_back(std::format("  {:04X}  a15               {:08X} /* {} */", l->offset + offsetof(AttackData, unknown_a15), attack.unknown_a15, attack.unknown_a15));
      l->lines.emplace_back(std::format("  {:04X}  a16               {:08X} /* {} */", l->offset + offsetof(AttackData, unknown_a16), attack.unknown_a16, attack.unknown_a16));
    });
  };
  auto disassemble_label_as_movement_data = [&](shared_ptr<Label> l) -> void {
    disassemble_label_as_struct.template operator()<MovementData>(l, [&](const MovementData& movement) -> void {
      l->lines.emplace_back("  // As MovementData");
      l->lines.emplace_back(std::format("  {:04X}  fparam1           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam1), movement.fparam1.load_raw(), movement.fparam1));
      l->lines.emplace_back(std::format("  {:04X}  fparam2           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam2), movement.fparam2.load_raw(), movement.fparam2));
      l->lines.emplace_back(std::format("  {:04X}  fparam3           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam3), movement.fparam3.load_raw(), movement.fparam3));
      l->lines.emplace_back(std::format("  {:04X}  fparam4           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam4), movement.fparam4.load_raw(), movement.fparam4));
      l->lines.emplace_back(std::format("  {:04X}  fparam5           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam5), movement.fparam5.load_raw(), movement.fparam5));
      l->lines.emplace_back(std::format("  {:04X}  fparam6           {:08X} /* {:g} */", l->offset + offsetof(MovementData, fparam6), movement.fparam6.load_raw(), movement.fparam6));
      l->lines.emplace_back(std::format("  {:04X}  iparam1           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam1), movement.iparam1, movement.iparam1));
      l->lines.emplace_back(std::format("  {:04X}  iparam2           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam2), movement.iparam2, movement.iparam2));
      l->lines.emplace_back(std::format("  {:04X}  iparam3           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam3), movement.iparam3, movement.iparam3));
      l->lines.emplace_back(std::format("  {:04X}  iparam4           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam4), movement.iparam4, movement.iparam4));
      l->lines.emplace_back(std::format("  {:04X}  iparam5           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam5), movement.iparam5, movement.iparam5));
      l->lines.emplace_back(std::format("  {:04X}  iparam6           {:08X} /* {} */", l->offset + offsetof(MovementData, iparam6), movement.iparam6, movement.iparam6));
    });
  };
  auto disassemble_label_as_image_data = [&](shared_ptr<Label> l) -> void {
    if (reassembly_mode) {
      l->lines.emplace_back(std::format("  // As compressed image data (0x{:X} bytes)", l->size));
      l->lines.emplace_back("  .data " + phosg::format_data_string(text_r.pgetv(l->offset, l->size), l->size));
    } else {
      const void* data = text_r.pgetv(l->offset, l->size);
      auto decompressed = prs_decompress_with_meta(data, l->size);
      l->lines.emplace_back(std::format("  // As decompressed image data (0x{:X} bytes)", decompressed.data.size()));
      l->lines.emplace_back(format_and_indent_data(decompressed.data.data(), decompressed.data.size(), 0));
      if (decompressed.input_bytes_used < l->size) {
        size_t compressed_end_offset = l->offset + decompressed.input_bytes_used;
        size_t remaining_size = l->size - decompressed.input_bytes_used;
        l->lines.emplace_back("  // Extra data after compressed data");
        l->lines.emplace_back(format_and_indent_data(text_r.pgetv(compressed_end_offset, remaining_size), remaining_size, compressed_end_offset));
      }
    }
  };
  auto disassemble_label_as_vector4f_list = [&](shared_ptr<Label> l) -> void {
    if (reassembly_mode) {
      l->lines.emplace_back(std::format("  // As VectorXYZTF list (0x{:X} bytes)", l->size));
      l->lines.emplace_back("  .data " + phosg::format_data_string(text_r.pgetv(l->offset, l->size), l->size));
    } else {
      phosg::StringReader r = text_r.sub(l->offset, l->size);
      l->lines.emplace_back("  // As VectorXYZTF list");
      while (r.remaining() >= sizeof(VectorXYZTF)) {
        size_t offset = l->offset + r.where();
        const auto& e = r.get<VectorXYZTF>();
        l->lines.emplace_back(std::format("  {:04X}  vector       x={:g}, y={:g}, z={:g}, t={:g}", offset, e.x, e.y, e.z, e.t));
      }
      if (r.remaining() > 0) {
        size_t struct_end_offset = l->offset + r.where();
        size_t remaining_size = r.remaining();
        l->lines.emplace_back("  // Extra data after structures");
        l->lines.emplace_back(format_and_indent_data(r.getv(remaining_size), remaining_size, struct_end_offset));
      }
    }
  };
  auto disassemble_label_as_script = [&](shared_ptr<Label> l, bool fail_on_invalid) -> void {
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

    const auto& opcodes = opcodes_for_version(version);
    bool version_has_args = F_HAS_ARGS & v_flag(version);

    auto label_r = text_r.sub(l->offset, l->size);
    vector<ArgStackValue> arg_stack_values;
    while (!label_r.eof()) {
      size_t opcode_start_offset = label_r.where();
      string dasm_line;
      try {
        uint16_t opcode = label_r.get_u8();
        if ((opcode & 0xFE) == 0xF8) {
          opcode = (opcode << 8) | label_r.get_u8();
        }

        const QuestScriptOpcodeDefinition* def = nullptr;
        try {
          def = opcodes.at(opcode);
        } catch (const out_of_range&) {
        }

        auto dasm_label = [&](uint32_t label_id, Arg::DataType type) -> string {
          if (label_id >= label_table.size()) {
            if (fail_on_invalid) {
              throw std::runtime_error(std::format("script refers to label{:04X} which is out of range", label_id));
            }
            return std::format("label{:04X} /* warning: label index out of range */", label_id);
          }
          auto& ref_l = label_table.at(label_id);
          if (!ref_l) {
            if (fail_on_invalid) {
              throw std::runtime_error(std::format("script refers to label{:04X} which is not defined", label_id));
            }
            return std::format("label{:04X} /* warning: label is not defined */", label_id);
          }
          ref_l->script_refs.emplace(l->offset + opcode_start_offset);
          if (type != Arg::DataType::NONE) {
            pending_labels.emplace_back(make_pair(ref_l, type));
          }
          return reassembly_mode
              ? std::format("label{:04X}", label_id)
              : std::format("label{:04X} /* {:04X} */", label_id, ref_l->offset);
        };

        if (def == nullptr) {
          if (fail_on_invalid) {
            throw std::runtime_error(std::format("script uses opcode {:04X} which does not exist", opcode));
          }
          dasm_line = std::format(".unknown {:04X}", opcode);
        } else {
          const char* op_name = (use_qedit_names && def->qedit_name) ? def->qedit_name : def->name;
          dasm_line = op_name ? op_name : std::format("[{:04X}]", opcode);
          if (!version_has_args || !(def->flags & F_ARGS)) {
            dasm_line.resize(std::max<size_t>(dasm_line.size() + 1, 0x20), ' ');
            bool is_first_arg = true;
            for (const auto& arg : def->args) {
              using Type = QuestScriptOpcodeDefinition::Argument::Type;

              string dasm_arg;
              switch (arg.type) {
                case Type::LABEL16:
                case Type::LABEL32: {
                  uint32_t label_id = (arg.type == Type::LABEL32) ? label_r.get_u32l() : label_r.get_u16l();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::LABEL, label_id);
                  }
                  dasm_arg = dasm_label(label_id, arg.data_type);
                  break;
                }
                case Type::LABEL16_SET: {
                  if (def->flags & F_PUSH_ARG) {
                    throw logic_error("LABEL16_SET cannot be pushed to arg list");
                  }
                  uint8_t num_functions = label_r.get_u8();
                  for (size_t z = 0; z < num_functions; z++) {
                    dasm_arg += (dasm_arg.empty() ? "[" : ", ");
                    dasm_arg += dasm_label(label_r.get_u16l(), arg.data_type);
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "[]";
                  } else {
                    dasm_arg += "]";
                  }
                  break;
                }
                case Type::R_REG:
                case Type::R_REG32:
                case Type::W_REG:
                case Type::W_REG32: {
                  uint32_t reg = ((arg.type == Type::R_REG32) || (arg.type == Type::W_REG32))
                      ? label_r.get_u32l()
                      : label_r.get_u8();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back((def->opcode == 0x004C) ? ArgStackValue::Type::REG_PTR : ArgStackValue::Type::REG, reg);
                  }
                  if (reg > 0xFF) {
                    dasm_arg = std::format("r{} /* warning: register number out of range */", reg);
                  } else {
                    dasm_arg = std::format("r{}", reg);
                  }
                  break;
                }
                case Type::R_REG_SET: {
                  if (def->flags & F_PUSH_ARG) {
                    throw logic_error("REG_SET cannot be pushed to arg list");
                  }
                  uint8_t num_regs = label_r.get_u8();
                  for (size_t z = 0; z < num_regs; z++) {
                    dasm_arg += std::format("{}r{}", (dasm_arg.empty() ? "[" : ", "), label_r.get_u8());
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "[]";
                  } else {
                    dasm_arg += "]";
                  }
                  break;
                }
                case Type::R_REG_SET_FIXED:
                case Type::W_REG_SET_FIXED: {
                  if (def->flags & F_PUSH_ARG) {
                    throw logic_error("REG_SET_FIXED cannot be pushed to arg list");
                  }
                  uint8_t first_reg = label_r.get_u8();
                  dasm_arg = std::format("r{}-r{}", first_reg, static_cast<uint8_t>(first_reg + arg.count - 1));
                  break;
                }
                case Type::R_REG32_SET_FIXED:
                case Type::W_REG32_SET_FIXED: {
                  if (def->flags & F_PUSH_ARG) {
                    throw logic_error("REG32_SET_FIXED cannot be pushed to arg list");
                  }
                  uint32_t first_reg = label_r.get_u32l();
                  dasm_arg = std::format("r{}-r{}", first_reg, static_cast<uint32_t>(first_reg + arg.count - 1));
                  break;
                }
                case Type::I8: {
                  uint8_t v = label_r.get_u8();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = std::format("0x{:02X}", v);
                  break;
                }
                case Type::I16: {
                  uint16_t v = label_r.get_u16l();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = std::format("0x{:04X}", v);
                  break;
                }
                case Type::I32: {
                  uint32_t v = label_r.get_u32l();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = std::format("0x{:08X}", v);
                  break;
                }
                case Type::FLOAT32: {
                  float v = label_r.get_f32l();
                  if (def->flags & F_PUSH_ARG) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, std::bit_cast<uint32_t>(v));
                  }
                  dasm_arg = std::format("{}", v);
                  break;
                }
                case Type::CSTRING:
                  if (uses_utf16(version)) {
                    phosg::StringWriter w;
                    for (uint16_t ch = label_r.get_u16l(); ch; ch = label_r.get_u16l()) {
                      w.put_u16l(ch);
                    }
                    if (def->flags & F_PUSH_ARG) {
                      arg_stack_values.emplace_back(tt_utf16_to_utf8(w.str()));
                    }
                    dasm_arg = escape_string(w.str(), TextEncoding::UTF16);
                  } else {
                    string s = label_r.get_cstr();
                    if (def->flags & F_PUSH_ARG) {
                      arg_stack_values.emplace_back((language == Language::JAPANESE) ? tt_sega_sjis_to_utf8(s) : tt_8859_to_utf8(s));
                    }
                    dasm_arg = escape_string(s, encoding_for_language(language));
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

          } else { // (def->flags & F_ARGS)
            dasm_line.resize(std::max<size_t>(0x20, dasm_line.size() + 1), ' ');
            dasm_line += "... ";

            // Can't match if there aren't enough args
            if (def->args.size() > arg_stack_values.size()) {
              if (fail_on_invalid) {
                throw std::runtime_error("received insufficient arguments for F_ARGS-type opcode");
              }
              dasm_line += std::format("/* matching error: expected {} arguments, received {} arguments */",
                  def->args.size(), arg_stack_values.size());
            } else {
              // CAN match if there are too many args, but show a warning
              if (def->args.size() < arg_stack_values.size() && !reassembly_mode) {
                dasm_line += std::format("/* warning: expected {} arguments, received {} arguments */ ",
                    def->args.size(), arg_stack_values.size());
              }

              bool is_first_arg = true;
              for (size_t z = 0; z < def->args.size(); z++) {
                const auto& arg_def = def->args[z];
                const auto& arg_value = arg_stack_values[z];

                string dasm_arg;
                switch (arg_def.type) {
                  case Arg::Type::LABEL16:
                  case Arg::Type::LABEL32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = std::format("r{} /* warning: cannot determine label data type */", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::LABEL:
                      case ArgStackValue::Type::INT:
                        dasm_arg = dasm_label(arg_value.as_int, arg_def.data_type);
                        break;
                      default:
                        if (fail_on_invalid) {
                          throw std::runtime_error("incorrect argument data type (expected REG, LABEL, or INT for LABEL16/LABEL32)");
                        }
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::R_REG:
                  case Arg::Type::W_REG:
                  case Arg::Type::R_REG32:
                  case Arg::Type::W_REG32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = std::format("regs[r{}]", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = std::format("r{}", arg_value.as_int);
                        break;
                      default:
                        if (fail_on_invalid) {
                          throw std::runtime_error("incorrect argument data type (expected REG or INT for [RW]_REG(32)?)");
                        }
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::R_REG_SET_FIXED:
                  case Arg::Type::W_REG_SET_FIXED:
                  case Arg::Type::R_REG32_SET_FIXED:
                  case Arg::Type::W_REG32_SET_FIXED:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = std::format("regs[r{}]-regs[r{}+{}]", arg_value.as_int, arg_value.as_int, static_cast<uint8_t>(arg_def.count - 1));
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = std::format("r{}-r{}", arg_value.as_int, static_cast<uint8_t>(arg_value.as_int + arg_def.count - 1));
                        break;
                      default:
                        if (fail_on_invalid) {
                          throw std::runtime_error("incorrect argument data type (expected REG or INT for [RW]_REG(32)?_SET_FIXED)");
                        }
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::I8:
                  case Arg::Type::I16:
                  case Arg::Type::I32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = std::format("r{}", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::REG_PTR:
                        dasm_arg = std::format("&r{}", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = std::format("0x{:X} /* {} */", arg_value.as_int, arg_value.as_int);
                        break;
                      default:
                        if (fail_on_invalid) {
                          throw std::runtime_error("incorrect argument data type (expected REG, REG_PTR, or INT for I8/I16/I32)");
                        }
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::FLOAT32:
                    switch (arg_value.type) {
                      case ArgStackValue::Type::REG:
                        dasm_arg = std::format("f{}", arg_value.as_int);
                        break;
                      case ArgStackValue::Type::INT:
                        dasm_arg = std::format("{:g}", std::bit_cast<float>(arg_value.as_int));
                        break;
                      default:
                        if (fail_on_invalid) {
                          throw std::runtime_error("incorrect argument data type (expected REG or INT for FLOAT32)");
                        }
                        dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::CSTRING:
                    if (arg_value.type == ArgStackValue::Type::CSTRING) {
                      dasm_arg = escape_string(arg_value.as_string);
                    } else {
                      if (fail_on_invalid) {
                        throw std::runtime_error("incorrect argument data type (expected CSTRING)");
                      }
                      dasm_arg = "/* invalid-type */";
                    }
                    break;
                  case Arg::Type::LABEL16_SET:
                  case Arg::Type::R_REG_SET:
                  default:
                    throw logic_error("set-type arg found on arg list");
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

          if (def->flags & (F_ARGS | F_CLEAR_ARGS)) {
            arg_stack_values.clear();
          }
        }
      } catch (const exception& e) {
        if (fail_on_invalid) {
          throw;
        }
        dasm_line = std::format(".failed ({})", e.what());
      }
      phosg::strip_trailing_whitespace(dasm_line);

      string line_text;
      if (reassembly_mode) {
        line_text = std::format("  {}", dasm_line);
      } else {
        size_t opcode_size = label_r.where() - opcode_start_offset;
        string hex_data = phosg::format_data_string(label_r.preadx(opcode_start_offset, opcode_size), nullptr, phosg::FormatDataFlags::HEX_ONLY);
        if (hex_data.size() > 14) {
          hex_data.resize(12);
          hex_data += "...";
        }
        hex_data.resize(16, ' ');
        line_text = std::format("  {:04X}  {}  {}", l->offset + opcode_start_offset, hex_data, dasm_line);
      }
      l->lines.emplace_back(std::move(line_text));
    }
  };

  // In "parallel", iterate through all labels and process the queue of un-disassembled labels. The queue takes
  // priority, since we know the intended data types of those labels. When the queue is empty, we resume sweeping the
  // label table to disassemble any labels whose types we don't know, either as SCRIPT or DATA. If an unknown label
  // successfully disassembles as SCRIPT, it may enqueue more labels, so we'll switch back to processing the queue. We
  // only need to sweep for unknown labels once, since a label's type never becomes unknown once it's known.
  auto unknown_label_it = offset_to_label.begin();
  while (!pending_labels.empty() || (unknown_label_it != offset_to_label.end())) {
    if (!pending_labels.empty()) {
      auto [l, type] = pending_labels.front();
      pending_labels.pop_front();

      // Only disassemble if the data type has changed, and enforce a priority order for changing data types. The order
      // is NONE < DATA < SCRIPT < (CSTRING and all struct types)
      bool should_disassemble = false;
      switch (type) {
        case Arg::DataType::NONE:
          throw std::logic_error("Cannot disassemble label with no data type");
        case Arg::DataType::DATA:
          should_disassemble = (l->type == Arg::DataType::NONE);
          break;
        case Arg::DataType::SCRIPT:
          should_disassemble = (l->type == Arg::DataType::NONE) || (l->type == Arg::DataType::DATA);
          break;
        case Arg::DataType::CSTRING:
        case Arg::DataType::PLAYER_STATS:
        case Arg::DataType::PLAYER_VISUAL_CONFIG:
        case Arg::DataType::RESIST_DATA:
        case Arg::DataType::ATTACK_DATA:
        case Arg::DataType::MOVEMENT_DATA:
        case Arg::DataType::IMAGE_DATA:
        case Arg::DataType::VECTOR4F_LIST:
          should_disassemble = (l->type == Arg::DataType::NONE) || (l->type == Arg::DataType::DATA) || (l->type == Arg::DataType::SCRIPT);
          break;
      }
      if (!should_disassemble) {
        continue;
      }

      l->type = type;
      l->lines.clear();
      switch (type) {
        case Arg::DataType::NONE:
          throw std::logic_error("Cannot disassemble label with no data type");
        case Arg::DataType::DATA:
          disassemble_label_as_data(l);
          break;
        case Arg::DataType::CSTRING:
          disassemble_label_as_cstring(l);
          break;
        case Arg::DataType::PLAYER_STATS:
          disassemble_label_as_player_stats(l);
          break;
        case Arg::DataType::PLAYER_VISUAL_CONFIG:
          disassemble_label_as_player_visual_config(l);
          break;
        case Arg::DataType::RESIST_DATA:
          disassemble_label_as_resist_data(l);
          break;
        case Arg::DataType::ATTACK_DATA:
          disassemble_label_as_attack_data(l);
          break;
        case Arg::DataType::MOVEMENT_DATA:
          disassemble_label_as_movement_data(l);
          break;
        case Arg::DataType::IMAGE_DATA:
          disassemble_label_as_image_data(l);
          break;
        case Arg::DataType::VECTOR4F_LIST:
          disassemble_label_as_vector4f_list(l);
          break;
        case Arg::DataType::SCRIPT:
          try {
            disassemble_label_as_script(l, reassembly_mode);
          } catch (const std::exception& e) {
            l->lines.clear();
            l->lines.emplace_back(std::format("  // Warning: label is code, but disassembly failed ({})", e.what()));
            disassemble_label_as_data(l);
          }
          break;
      }

    } else { // unknown_label_it != offset_to_label.end()
      auto [_, l] = *unknown_label_it;
      if (l->type == Arg::DataType::NONE) {
        l->lines.clear();
        try {
          disassemble_label_as_script(l, true);
        } catch (const std::exception& e) {
          l->lines.clear();
          l->lines.emplace_back(std::format("  // Label type is not known; could not disassemble as code ({})", e.what()));
          disassemble_label_as_data(l);
        }
      }
      unknown_label_it++;
    }
  }

  // Phase 5: Generate disassembly for all the label sections

  for (const auto& [_, l] : offset_to_label) {
    // Don't include __before__ in reassembly mode
    if (reassembly_mode && (l->label_nums.empty())) {
      continue;
    }

    if (l->label_nums.empty()) {
      lines.emplace_back("__before__:");
    } else {
      for (uint32_t label_num : l->label_nums) {
        if (label_num == 0) {
          lines.emplace_back("start:");
        } else if (reassembly_mode) {
          lines.emplace_back(std::format("label{:04X}@0x{:04X}:", label_num, label_num));
        } else {
          lines.emplace_back(std::format("label{:04X}:", label_num));
        }
      }
    }

    if (!reassembly_mode) {
      // The reassembly-mode script doesn't have offsets, so these reference comments are only useful when it's off
      if (l->script_refs.size() == 1) {
        lines.emplace_back(std::format("  // Referenced by instruction at {:04X}", *l->script_refs.begin()));
      } else if (!l->script_refs.empty()) {
        vector<string> tokens;
        tokens.reserve(l->script_refs.size());
        for (size_t ref_offset : l->script_refs) {
          tokens.emplace_back(std::format("{:04X}", ref_offset));
        }
        lines.emplace_back("  // Referenced by instructions at " + phosg::join(tokens, ", "));
      }
    }

    for (const auto& obj_ref : l->object_refs) {
      lines.emplace_back(std::format("  // Referenced by map object KS-{:02X}-{:03X} {}",
          obj_ref.floor, obj_ref.set_index, obj_ref.obj_set->str()));
    }
    for (const auto& ene_ref : l->enemy_refs) {
      lines.emplace_back(std::format("  // Referenced by map enemy ES-{:02X}-{:03X} {}",
          ene_ref.floor, ene_ref.set_index, ene_ref.ene_set->str()));
    }

    for (auto& line : l->lines) {
      lines.emplace_back(std::move(line));
    }
    lines.emplace_back();
  }

  // Phase 6: Produce any extra (unused!) data if present
  if (extra_r.size() > 0) {
    if (reassembly_mode) {
      lines.emplace_back(std::format(
          "// Warning: 0x{:X} bytes of extra data after quest contents; ignoring it", extra_r.size()));
    } else {
      lines.emplace_back("// Warning: Extra data after quest contents");
      lines.emplace_back(format_and_indent_data(extra_r.getv(extra_r.size()), extra_r.size(), meta.total_size));
    }
    lines.emplace_back();
  }

  return phosg::join(lines, "\n");
}

Episode episode_for_quest_episode_number(uint8_t episode_number) {
  switch (episode_number) {
    case 0x00:
    case 0xFF:
      return Episode::EP1;
    case 0x01:
      return Episode::EP2;
    case 0x02:
      return Episode::EP4;
    default:
      throw runtime_error(std::format("invalid episode number {:02X}", episode_number));
  }
}

struct RegisterAssigner {
  struct Register {
    string name;
    int16_t number = -1; // -1 = unassigned (any number)
    shared_ptr<Register> prev;
    shared_ptr<Register> next;
    unordered_set<size_t> offsets;

    std::string str() const {
      return std::format("Register(name=\"{}\", number={})", this->name, this->number);
    }
  };

  ~RegisterAssigner() {
    for (auto& it : this->named_regs) {
      it.second->prev.reset();
      it.second->next.reset();
    }
    for (auto& reg : this->numbered_regs) {
      if (reg) {
        reg->prev.reset();
        reg->next.reset();
      }
    }
  }

  RegisterAssigner() {
    // Map registers that have hardcoded behaviors so we don't assign named registers to them
    static const map<string, uint8_t> special_regs = {
        // Registers used by va_start and va_end
        {"va_arg1", 1}, {"va_arg2", 2}, {"va_arg3", 3}, {"va_arg4", 4}, {"va_arg5", 5}, {"va_arg6", 6}, {"va_arg7", 7},
        // Registers that control item visibility on the Quest Board
        {"quest_board_item1", 74}, {"quest_board_item2", 75}, {"quest_board_item3", 76}, {"quest_board_item4", 77},
        {"quest_board_item5", 78}, {"quest_board_item6", 79}, {"quest_board_item7", 80},
        // Registers used by the Hunter's Guild counter
        {"quest_failed", 253}, {"quest_succeeded", 255}};

    for (const auto& [name, reg_num] : special_regs) {
      this->get_or_create(name, reg_num);
    }
  }

  shared_ptr<Register> get_or_create(const string& name, int16_t number) {
    if ((number < -1) || (number >= 0x100)) {
      throw runtime_error("invalid register number");
    }

    shared_ptr<Register> reg;
    if (!name.empty()) {
      try {
        reg = this->named_regs.at(name);
      } catch (const out_of_range&) {
      }
    }
    if (!reg && number >= 0) {
      reg = this->numbered_regs.at(number);
    }

    if (!reg) {
      reg = make_shared<Register>();
    }

    if (number >= 0) {
      if (reg->number < 0) {
        reg->number = number;
        auto& numbered_reg = this->numbered_regs.at(reg->number);
        if (numbered_reg) {
          throw runtime_error(reg->str() + " cannot be assigned due to conflict with " + numbered_reg->str());
        }
        this->numbered_regs.at(reg->number) = reg;
      } else if (reg->number != number) {
        throw runtime_error(std::format("register {} is assigned multiple numbers", reg->name));
      }
    }

    if (!name.empty()) {
      if (reg->name.empty()) {
        reg->name = name;
        if (!this->named_regs.emplace(reg->name, reg).second) {
          throw runtime_error(std::format("name {} is already assigned to a different register", reg->name));
        }
      } else if (reg->name != name) {
        throw runtime_error(std::format("register {} is assigned multiple names", reg->number));
      }
    }

    return reg;
  }

  void assign_number(shared_ptr<Register> reg, uint8_t number) {
    if (reg->number < 0) {
      reg->number = number;
      if (this->numbered_regs.at(reg->number)) {
        throw logic_error(std::format("register number {} assigned multiple times", reg->number));
      }
      this->numbered_regs.at(reg->number) = reg;
    } else if (reg->number != static_cast<int16_t>(number)) {
      throw runtime_error(std::format("assigning different register number {} over existing register number {}", number, reg->number));
    }
  }

  void constrain(shared_ptr<Register> first_reg, shared_ptr<Register> second_reg) {
    if (!first_reg->next) {
      first_reg->next = second_reg;
    } else if (first_reg->next != second_reg) {
      throw runtime_error(std::format("register {} must come after {}, but is already constrained to another register", second_reg->name, first_reg->name));
    }
    if (!second_reg->prev) {
      second_reg->prev = first_reg;
    } else if (second_reg->prev != first_reg) {
      throw runtime_error(std::format("register {} must come before {}, but is already constrained to another register", first_reg->name, second_reg->name));
    }
    if ((first_reg->number >= 0) && (second_reg->number >= 0) && (first_reg->number != ((second_reg->number - 1) & 0xFF))) {
      throw runtime_error(std::format("register {} must come before {}, but both registers already have non-consecutive numbers", first_reg->name, second_reg->name));
    }
  }

  void assign_all() {
    // TODO: Technically, we should assign the biggest blocks first to minimize fragmentation. I am lazy and haven't
    // implemented this yet.
    vector<shared_ptr<Register>> unassigned;
    for (auto it : this->named_regs) {
      if (it.second->number < 0) {
        unassigned.emplace_back(it.second);
      }
    }

    for (auto reg : unassigned) {
      // If this register is already assigned, skip it
      if (reg->number >= 0) {
        continue;
      }

      // If any next register is assigned, assign this register
      size_t next_delta = 1;
      for (auto next_reg = reg->next; next_reg; next_reg = next_reg->next, next_delta++) {
        if (next_reg->number >= 0) {
          this->assign_number(reg, (next_reg->number - next_delta) & 0xFF);
          break;
        }
      }
      if (reg->number >= 0) {
        continue;
      }

      // If any prev register is assigned, assign this register
      size_t prev_delta = 1;
      for (auto prev_reg = reg->prev; prev_reg; prev_reg = prev_reg->prev, prev_delta++) {
        if (prev_reg->number >= 0) {
          this->assign_number(reg, (prev_reg->number + prev_delta) & 0xFF);
          break;
        }
      }
      if (reg->number >= 0) {
        continue;
      }

      // No prev or next register is assigned; find an interval in the register number space that fits this block of
      // registers. The total number of register numbers needed is (prev_delta - 1) + (next_delta - 1) + 1.
      size_t num_regs = prev_delta + next_delta - 1;
      this->assign_number(reg, (this->find_register_number_space(num_regs) + (prev_delta - 1)) & 0xFF);

      // We don't need to assign the prev and next registers; they should also be in the unassigned set and will be
      // assigned by the above logic
    }

    // At this point, all registers should be assigned
    for (const auto& it : this->named_regs) {
      if (it.second->number < 0) {
        throw logic_error(std::format("register {} was not assigned", it.second->name));
      }
    }
    for (size_t z = 0; z < 0x100; z++) {
      auto reg = this->numbered_regs[z];
      if (reg && (reg->number != static_cast<int16_t>(z))) {
        throw logic_error(std::format("register {} has incorrect number {}", z, reg->number));
      }
    }
  }

  uint8_t find_register_number_space(size_t num_regs) const {
    for (size_t candidate = 0; candidate < 0x100; candidate++) {
      size_t z;
      for (z = 0; z < num_regs; z++) {
        if (this->numbered_regs[candidate + z]) {
          break;
        }
      }
      if (z == num_regs) {
        return candidate;
      }
    }
    throw runtime_error("not enough space to assign registers");
  }

  map<string, shared_ptr<Register>> named_regs;
  array<shared_ptr<Register>, 0x100> numbered_regs;
};

AssembledQuestScript assemble_quest_script(
    const std::string& text,
    const vector<string>& script_include_directories,
    const vector<string>& native_include_directories,
    bool strict) {

  struct Line {
    string filename; // Empty if this is the main file
    size_t number; // 1-based (there is no line 0)
    string text;
    ssize_t parent_index; // -1 if it's from the root file
  };

  auto wrap_exceptions_with_line_ref = [](const Line& line, auto fn) -> void {
    try {
      fn();
    } catch (const exception& e) {
      if (line.filename.empty()) {
        throw runtime_error(std::format("(__main__:{}) {}", line.number, e.what()));
      } else {
        throw runtime_error(std::format("({}:{}) {}", line.filename, line.number, e.what()));
      }
    }
  };

  std::vector<Line> lines;
  auto include_file = [&](const std::string& filename, const std::string& orig_text, ssize_t parent_index) {
    // Inserts the new lines after the parent line and preprocesses them. The parent line is not modified or deleted.
    std::string text = orig_text;
    phosg::strip_comments_inplace(text);

    vector<Line> new_lines;
    auto text_lines = phosg::split(text, '\n');
    for (size_t z = 0; z < text_lines.size(); z++) {
      auto& line = new_lines.emplace_back();
      line.filename = filename;
      line.number = z + 1;
      line.text = std::move(text_lines[z]);
      line.parent_index = parent_index;
      phosg::strip_trailing_whitespace(line.text);
      phosg::strip_leading_whitespace(line.text);
    }

    if (parent_index < 0) { // Root file
      lines = std::move(new_lines);
    } else {
      lines.insert(
          lines.begin() + (parent_index + 1),
          std::make_move_iterator(new_lines.begin()),
          std::make_move_iterator(new_lines.end()));
    }
  };
  include_file("", text, -1);

  // Process all includes
  for (size_t z = 0; z < lines.size(); z++) {
    if (lines[z].text.starts_with(".include ")) {
      string filename = lines[z].text.substr(9);
      phosg::strip_leading_whitespace(filename);

      // Make sure there's not a cycle
      unordered_set<string> seen_filenames;
      for (ssize_t index = lines[z].parent_index; index >= 0; index = lines[index].parent_index) {
        if (!seen_filenames.emplace(lines.at(index).filename).second) {
          throw runtime_error(std::format("detected cycle while including {}", filename));
        }
      }

      bool found = false;
      for (const auto& include_dir : script_include_directories) {
        string include_path = include_dir + "/" + filename;
        if (std::filesystem::is_regular_file(include_path)) {
          found = true;
          include_file(filename, phosg::load_file(include_path), z);
          break;
        }
      }
      if (!found) {
        throw runtime_error(std::format("included file {} not found in any include directory", filename));
      }

      // We leave the .include line there; it will be ignored in the logic below
    }
  }

  // Collect metadata directives

  std::unordered_set<std::string> metadata_directive_names{
      ".include", ".version", ".name", ".short_desc", ".long_desc", ".allow_create_item", ".solo_unlock_flag",
      ".quest_num", ".language", ".episode", ".max_players", ".joinable", ".header_language", ".header_episode",
      ".header_unknown_a1", ".header_unknown_a2", ".header_unknown_a3", ".header_unknown_a4", ".header_unknown_a5",
      ".header_unknown_a6"};

  AssembledQuestScript ret;
  for (const auto& line : lines) {
    if (line.text.empty()) {
      continue;
    }
    wrap_exceptions_with_line_ref(line, [&]() -> void {
      if (line.text[0] == '.') {
        if (line.text.starts_with(".include ")) {
          // Nothing to do (see above)
        } else if (line.text.starts_with(".version ")) {
          string name = line.text.substr(9);
          phosg::strip_leading_whitespace(name);
          ret.meta.version = phosg::enum_for_name<Version>(name);
          if ((ret.meta.episode == Episode::NONE) && is_v1_or_v2(ret.meta.version) && (ret.meta.version != Version::GC_NTE)) {
            ret.meta.episode = Episode::EP1;
          }
        } else if (line.text.starts_with(".name ")) {
          ret.meta.name = phosg::parse_data_string(line.text.substr(6));
        } else if (line.text.starts_with(".short_desc ")) {
          ret.meta.short_description = phosg::parse_data_string(line.text.substr(12));
        } else if (line.text.starts_with(".long_desc ")) {
          ret.meta.long_description = phosg::parse_data_string(line.text.substr(11));
        } else if (line.text.starts_with(".allow_create_item ")) {
          if (ret.meta.create_item_mask_entries.size() >= 0x40) {
            throw std::runtime_error("too many .allow_create_item directives; at most 64 are allowed");
          }
          ret.meta.create_item_mask_entries.emplace_back(line.text.substr(19));
        } else if (line.text.starts_with(".solo_unlock_flag ")) {
          if (ret.meta.solo_unlock_flags.size() >= 8) {
            throw std::runtime_error("too many .solo_unlock_flag directives; at most 8 are allowed");
          }
          ret.meta.solo_unlock_flags.emplace_back(stoul(line.text.substr(18), nullptr, 0));
        } else if (line.text.starts_with(".quest_num ")) {
          ret.meta.quest_number = stoul(line.text.substr(11), nullptr, 0);
        } else if (line.text.starts_with(".language ")) {
          auto code = line.text.substr(10);
          if (code.size() != 1) {
            throw runtime_error(".language directive argument is invalid");
          }
          ret.meta.language = language_for_char(code[0]);
        } else if (line.text.starts_with(".episode ")) {
          ret.meta.episode = episode_for_token_name(line.text.substr(9));
        } else if (line.text.starts_with(".max_players ")) {
          ret.meta.max_players = stoul(line.text.substr(12), nullptr, 0);
        } else if (line.text.starts_with(".joinable")) {
          ret.meta.joinable = true;
        } else if (line.text.starts_with(".header_language ")) {
          ret.meta.header_language = stoul(line.text.substr(17), nullptr, 0);
        } else if (line.text.starts_with(".header_episode ")) {
          ret.meta.header_episode = stoul(line.text.substr(16), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a1 ")) {
          ret.meta.header_unknown_a1 = stoul(line.text.substr(19), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a2 ")) {
          ret.meta.header_unknown_a2 = stoul(line.text.substr(19), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a3 ")) {
          ret.meta.header_unknown_a3 = stoul(line.text.substr(19), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a4 ")) {
          ret.meta.header_unknown_a4 = stoul(line.text.substr(19), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a5 ")) {
          ret.meta.header_unknown_a5 = stoul(line.text.substr(19), nullptr, 0);
        } else if (line.text.starts_with(".header_unknown_a6 ")) {
          ret.meta.header_unknown_a6 = stoul(line.text.substr(19), nullptr, 0);
        }
      }
    });
  }
  if (ret.meta.version == Version::PC_PATCH || ret.meta.version == Version::BB_PATCH || ret.meta.version == Version::UNKNOWN) {
    throw runtime_error(".version directive is missing or invalid");
  }
  if (ret.meta.quest_number == 0xFFFFFFFF) {
    throw runtime_error(".quest_num directive is missing or invalid");
  }
  if (ret.meta.name.empty()) {
    throw runtime_error(".name directive is missing or invalid");
  }

  // Find all label names
  struct Label {
    std::string name;
    ssize_t index = -1;
    ssize_t offset = -1;
  };
  map<string, shared_ptr<Label>> labels_by_name;
  map<ssize_t, shared_ptr<Label>> labels_by_index;
  for (const auto& line : lines) {
    wrap_exceptions_with_line_ref(line, [&]() -> void {
      if (line.text.ends_with(":")) {
        auto label = make_shared<Label>();
        label->name = line.text.substr(0, line.text.size() - 1);
        size_t at_offset = label->name.find('@');
        if (at_offset != string::npos) {
          try {
            label->index = stoul(label->name.substr(at_offset + 1), nullptr, 0);
          } catch (const exception& e) {
            throw runtime_error(std::format("invalid index in label ({})", e.what()));
          }
          label->name.resize(at_offset);
          if (label->name == "start" && label->index != 0) {
            throw runtime_error("start label cannot have a nonzero label ID");
          }
        } else if (label->name == "start") {
          label->index = 0;
        }
        if (!labels_by_name.emplace(label->name, label).second) {
          throw runtime_error("duplicate label name: " + label->name);
        }
        if (label->index >= 0) {
          auto index_emplace_ret = labels_by_index.emplace(label->index, label);
          if (label->index >= 0 && !index_emplace_ret.second) {
            throw runtime_error(std::format(
                "duplicate label index: {} (0x{:X}) from {} and {}",
                label->index, label->index, label->name, index_emplace_ret.first->second->name));
          }
        }
      }
    });
  }
  if (!labels_by_name.count("start")) {
    throw runtime_error("start label is not defined");
  }

  // Assign indexes to labels without explicit indexes
  {
    size_t next_index = 0;
    for (auto& it : labels_by_name) {
      if (it.second->index >= 0) {
        continue;
      }
      while (labels_by_index.count(next_index)) {
        next_index++;
      }
      it.second->index = next_index++;
      labels_by_index.emplace(it.second->index, it.second);
    }
  }

  // Prepare to collect named registers
  RegisterAssigner reg_assigner;
  auto parse_reg = [&reg_assigner](const string& arg, bool allow_unnumbered = true) -> shared_ptr<RegisterAssigner::Register> {
    if (arg.size() < 2) {
      throw runtime_error("register argument is too short");
    }
    if ((arg[0] != 'r') && (arg[0] != 'f')) {
      throw runtime_error("a register is required");
    }
    string name;
    ssize_t number = -1;
    if (arg[1] == ':') {
      auto tokens = phosg::split(arg.substr(2), '@');
      if (tokens.size() == 1) {
        name = std::move(tokens[0]);
      } else if (tokens.size() == 2) {
        name = std::move(tokens[0]);
        number = stoull(tokens[1], nullptr, 0);
      } else {
        throw runtime_error("invalid register specification");
      }
    } else {
      number = stoull(arg.substr(1), nullptr, 0);
    }
    if (!allow_unnumbered && (number < 0)) {
      throw runtime_error("a numbered register is required");
    }
    if (number > 0xFF) {
      throw runtime_error("invalid register number");
    }
    return reg_assigner.get_or_create(name, number);
  };
  auto parse_reg_set_fixed = [&reg_assigner, &parse_reg](const string& name, size_t expected_count) -> vector<shared_ptr<RegisterAssigner::Register>> {
    if (expected_count == 0) {
      throw logic_error("REG_SET_FIXED argument expects no registers");
    }
    if (name.empty()) {
      throw runtime_error("no register specified for REG_SET_FIXED argument");
    }
    vector<shared_ptr<RegisterAssigner::Register>> regs;
    if ((name[0] == '(') && (name.back() == ')')) {
      auto tokens = phosg::split(name.substr(1, name.size() - 2), ',');
      if (tokens.size() != expected_count) {
        throw runtime_error("incorrect number of registers in REG_SET_FIXED");
      }
      for (auto& token : tokens) {
        phosg::strip_trailing_whitespace(token);
        phosg::strip_leading_whitespace(token);
        regs.emplace_back(parse_reg(token));
        if (regs.size() > 1) {
          reg_assigner.constrain(regs.at(regs.size() - 2), regs.back());
        }
      }
    } else {
      auto tokens = phosg::split(name, '-');
      if (tokens.size() == 1) {
        regs.emplace_back(parse_reg(tokens[0], false));
        while (regs.size() < expected_count) {
          regs.emplace_back(parse_reg("", (regs.back()->number + 1) & 0xFF));
          reg_assigner.constrain(regs.at(regs.size() - 2), regs.back());
        }
      } else if (tokens.size() == 2) {
        regs.emplace_back(parse_reg(tokens[0], false));
        while (regs.size() < expected_count - 1) {
          regs.emplace_back(reg_assigner.get_or_create("", (regs.back()->number + 1) & 0xFF));
          reg_assigner.constrain(regs.at(regs.size() - 2), regs.back());
        }
        regs.emplace_back(parse_reg(tokens[1], false));
        if (static_cast<size_t>(regs.back()->number - regs.front()->number + 1) != expected_count) {
          throw runtime_error("incorrect number of registers used");
        }
        reg_assigner.constrain(regs.at(regs.size() - 2), regs.back());
      } else {
        throw runtime_error("invalid fixed register set syntax");
      }
    }
    if (regs.empty() || regs.size() != expected_count) {
      throw logic_error("incorrect register count in REG_SET_FIXED after parsing");
    }
    return regs;
  };

  // Assemble code segment

  auto get_native_include = [&](const std::string& filename) -> std::string {
    for (const auto& include_dir : native_include_directories) {
      string path = include_dir + "/" + filename;
      if (std::filesystem::is_regular_file(path)) {
        return phosg::load_file(path);
      }
    }
    throw runtime_error("data not found for native include: " + filename);
  };

  bool version_has_args = F_HAS_ARGS & v_flag(ret.meta.version);
  const auto& opcodes = opcodes_by_name_for_version(ret.meta.version);
  phosg::StringWriter code_w;
  std::vector<size_t> bb_map_designate_args_offsets;
  for (const auto& line : lines) {
    wrap_exceptions_with_line_ref(line, [&]() -> void {
      if (line.text.empty()) {
        return;
      }

      if (line.text.ends_with(":")) {
        size_t at_offset = line.text.find('@');
        string label_name = line.text.substr(0, (at_offset == string::npos) ? (line.text.size() - 1) : at_offset);
        labels_by_name.at(label_name)->offset = code_w.size();
        return;
      }

      if (line.text[0] == '.') {
        string directive, args;
        size_t space_loc = line.text.find(' ');
        if (space_loc == string::npos) {
          directive = line.text;
        } else {
          directive = line.text.substr(0, space_loc);
          args = line.text.substr(space_loc + 1);
          phosg::strip_whitespace(args);
        }

        if ((directive == ".data") || (directive == ".binary")) {
          code_w.write(phosg::parse_data_string(args));
        } else if (directive == ".cstr") {
          string data = phosg::parse_data_string(args);
          if (uses_utf16(ret.meta.version)) {
            code_w.write(tt_utf8_to_utf16(data));
            code_w.put_u16l(0);
          } else {
            code_w.write((ret.meta.language == Language::JAPANESE) ? tt_utf8_to_sega_sjis(data) : tt_utf8_to_8859(data));
            code_w.put_u8(0);
          }
        } else if (directive == ".zero") {
          size_t size = stoull(args, nullptr, 0);
          code_w.extend_by(size, 0x00);
        } else if (directive == ".zero_until") {
          size_t size = stoull(args, nullptr, 0);
          code_w.extend_to(size, 0x00);
        } else if (directive == ".align") {
          size_t alignment = stoull(args, nullptr, 0);
          while (code_w.size() % alignment) {
            code_w.put_u8(0);
          }
        } else if (directive == ".include") {
          // This was already handled in a previous phase
        } else if (directive == ".include_bin ") {
          code_w.write(get_native_include(args));
        } else if (directive == ".include_native") {
          string native_text = get_native_include(args);
          string code;
          if (is_ppc(ret.meta.version)) {
            code = std::move(ResourceDASM::PPC32Emulator::assemble(native_text).code);
          } else if (is_x86(ret.meta.version)) {
            code = std::move(ResourceDASM::X86Emulator::assemble(native_text).code);
          } else if (is_sh4(ret.meta.version)) {
            code = std::move(ResourceDASM::SH4Emulator::assemble(native_text).code);
          } else {
            throw runtime_error("unknown architecture");
          }
          code_w.write(code);
        } else if (!metadata_directive_names.count(directive)) { // These were handled in an earlier phase
          throw runtime_error("unknown directive: " + directive);
        }
        return;
      }

      auto line_tokens = phosg::split(line.text, ' ', 1);
      const QuestScriptOpcodeDefinition* opcode_def;
      try {
        opcode_def = opcodes.at(phosg::tolower(line_tokens.at(0)));
      } catch (const out_of_range&) {
        throw std::runtime_error(std::format("invalid opcode name: {}", line_tokens.at(0)));
      }

      bool use_args = version_has_args && (opcode_def->flags & F_ARGS);
      if (!use_args) {
        if ((opcode_def->opcode & 0xFF00) == 0x0000) {
          code_w.put_u8(opcode_def->opcode);
        } else {
          code_w.put_u16b(opcode_def->opcode);
        }
      }

      // Collect bb_map_designate offsets during assembly, to generate the necessary server-side header field afterward
      if (opcode_def->opcode == 0xF951) {
        if (bb_map_designate_args_offsets.size() >= 0x10) {
          throw std::runtime_error("bb_map_designate was used too many times; up to 16 uses are allowed");
        }
        bb_map_designate_args_offsets.emplace_back(code_w.size());
      }

      if (opcode_def->args.empty()) {
        if (line_tokens.size() > 1) {
          throw runtime_error(std::format("arguments not allowed for {}", opcode_def->name));
        }
        return;
      }

      if (line_tokens.size() < 2) {
        throw runtime_error(std::format("arguments required for {}", opcode_def->name));
      }
      phosg::strip_trailing_whitespace(line_tokens[1]);
      phosg::strip_leading_whitespace(line_tokens[1]);

      if (line_tokens[1].starts_with("...")) {
        if (!use_args) {
          throw runtime_error("\'...\' can only be used with F_ARGS opcodes");
        }

      } else { // Not "..."
        auto args = phosg::split_context(line_tokens[1], ',');
        if (args.size() != opcode_def->args.size()) {
          throw runtime_error("incorrect argument count");
        }

        for (size_t z = 0; z < args.size(); z++) {
          using Type = QuestScriptOpcodeDefinition::Argument::Type;

          string& arg = args[z];
          const auto& arg_def = opcode_def->args[z];
          phosg::strip_trailing_whitespace(arg);
          phosg::strip_leading_whitespace(arg);

          try {
            auto add_cstr = [&](const string& text, bool bin) -> void {
              switch (ret.meta.version) {
                case Version::DC_NTE:
                  code_w.write(bin ? text : tt_utf8_to_sega_sjis(text));
                  code_w.put_u8(0);
                  break;
                case Version::DC_11_2000:
                case Version::DC_V1:
                case Version::DC_V2:
                case Version::GC_NTE:
                case Version::GC_V3:
                case Version::GC_EP3_NTE:
                case Version::GC_EP3:
                case Version::XB_V3:
                  code_w.write(bin ? text : ((ret.meta.language == Language::JAPANESE) ? tt_utf8_to_sega_sjis(text) : tt_utf8_to_8859(text)));
                  code_w.put_u8(0);
                  break;
                case Version::PC_NTE:
                case Version::PC_V2:
                case Version::BB_V4:
                  code_w.write(bin ? text : tt_utf8_to_utf16(text));
                  code_w.put_u16(0);
                  break;
                default:
                  throw logic_error("invalid game version");
              }
            };

            if (use_args) {
              auto label_it = labels_by_name.find(arg);
              if (arg.empty()) {
                throw runtime_error("argument is empty");
              } else if (label_it != labels_by_name.end()) {
                code_w.put_u8(0x4B); // arg_pushw
                code_w.put_u16l(label_it->second->index);
              } else if ((arg[0] == 'r') || (arg[0] == 'f') || ((arg[0] == '(') && (arg.back() == ')'))) {
                // If the corresponding argument is a REG or REG_SET_FIXED, push the register number, not the
                // register's value, since it's an out-param
                switch (arg_def.type) {
                  case Type::R_REG:
                  case Type::W_REG:
                  case Type::R_REG32:
                  case Type::W_REG32: {
                    code_w.put_u8(0x4A); // arg_pushb
                    auto reg = parse_reg(arg);
                    reg->offsets.emplace(code_w.size());
                    code_w.put_u8(reg->number);
                    break;
                  }
                  case Type::R_REG_SET_FIXED:
                  case Type::W_REG_SET_FIXED:
                  case Type::R_REG32_SET_FIXED:
                  case Type::W_REG32_SET_FIXED: {
                    auto regs = parse_reg_set_fixed(arg, arg_def.count);
                    code_w.put_u8(0x4A); // arg_pushb
                    regs[0]->offsets.emplace(code_w.size());
                    code_w.put_u8(regs[0]->number);
                    break;
                  }
                  default:
                    code_w.put_u8(0x48); // arg_pushr
                    auto reg = parse_reg(arg);
                    reg->offsets.emplace(code_w.size());
                    code_w.put_u8(reg->number);
                }
              } else if ((arg[0] == '@') && ((arg[1] == 'r') || (arg[1] == 'f'))) {
                code_w.put_u8(0x4C); // arg_pusha
                auto reg = parse_reg(arg.substr(1));
                reg->offsets.emplace(code_w.size());
                code_w.put_u8(reg->number);
              } else if ((arg[0] == '@') && labels_by_name.count(arg.substr(1))) {
                code_w.put_u8(0x4D); // arg_pusho
                code_w.put_u16(labels_by_name.at(arg.substr(1))->index);
              } else {
                bool write_as_str = false;
                try {
                  size_t end_offset;
                  uint64_t value = stoll(arg, &end_offset, 0);
                  if (end_offset != arg.size()) {
                    write_as_str = true;
                  } else if (value > 0xFFFF) {
                    code_w.put_u8(0x49); // arg_pushl
                    code_w.put_u32l(value);
                  } else if (value > 0xFF) {
                    code_w.put_u8(0x4B); // arg_pushw
                    code_w.put_u16l(value);
                  } else {
                    code_w.put_u8(0x4A); // arg_pushb
                    code_w.put_u8(value);
                  }
                } catch (const exception&) {
                  write_as_str = true;
                }
                if (write_as_str) {
                  if (arg[0] == '\"') {
                    code_w.put_u8(0x4E); // arg_pushs
                    if (arg.starts_with("bin:")) {
                      add_cstr(phosg::parse_data_string(arg.substr(4)), true);
                    } else {
                      add_cstr(phosg::parse_data_string(arg), false);
                    }
                  } else {
                    throw runtime_error("invalid argument syntax");
                  }
                }
              }

            } else { // Not use_args
              auto add_label = [&](const string& name, bool is32) -> void {
                size_t label_index;
                auto it = labels_by_name.find(name);
                if (it == labels_by_name.end()) {
                  if (strict || !name.starts_with("label")) {
                    throw runtime_error("label not defined: " + name);
                  } else {
                    size_t used_chars;
                    label_index = std::stoul(name.substr(5), &used_chars, 16);
                    if (used_chars != name.size() - 5) {
                      throw runtime_error("label not defined: " + name);
                    }
                  }
                } else {
                  label_index = it->second->index;
                }

                if (is32) {
                  code_w.put_u32(label_index);
                } else {
                  code_w.put_u16(label_index);
                }
              };
              auto add_reg = [&](shared_ptr<RegisterAssigner::Register> reg, bool is32) -> void {
                reg->offsets.emplace(code_w.size());
                if (is32) {
                  code_w.put_u32l(reg->number & 0xFF);
                } else {
                  code_w.put_u8(reg->number);
                }
              };

              auto split_set = [&](const string& text) -> vector<string> {
                if (!text.starts_with("[") || !text.ends_with("]")) {
                  throw runtime_error("incorrect syntax for set-valued argument");
                }
                auto values = phosg::split(text.substr(1, text.size() - 2), ',');
                if (values.size() > 0xFF) {
                  throw runtime_error("too many labels in set-valued argument");
                }
                for (auto& value : values) {
                  phosg::strip_whitespace(value);
                }
                if (values.size() == 1 && values[0].empty()) {
                  values.clear();
                }
                return values;
              };

              switch (arg_def.type) {
                case Type::LABEL16:
                case Type::LABEL32:
                  add_label(arg, arg_def.type == Type::LABEL32);
                  break;
                case Type::LABEL16_SET: {
                  auto label_names = split_set(arg);
                  code_w.put_u8(label_names.size());
                  for (auto name : label_names) {
                    phosg::strip_trailing_whitespace(name);
                    phosg::strip_leading_whitespace(name);
                    add_label(name, false);
                  }
                  break;
                }
                case Type::R_REG:
                case Type::W_REG:
                case Type::R_REG32:
                case Type::W_REG32:
                  add_reg(parse_reg(arg), (arg_def.type == Type::R_REG32) || (arg_def.type == Type::W_REG32));
                  break;
                case Type::R_REG_SET_FIXED:
                case Type::W_REG_SET_FIXED:
                case Type::R_REG32_SET_FIXED:
                case Type::W_REG32_SET_FIXED: {
                  auto regs = parse_reg_set_fixed(arg, arg_def.count);
                  add_reg(regs[0], (arg_def.type == Type::R_REG32_SET_FIXED) || (arg_def.type == Type::W_REG32_SET_FIXED));
                  break;
                }
                case Type::R_REG_SET: {
                  auto regs = split_set(arg);
                  code_w.put_u8(regs.size());
                  for (auto reg_arg : regs) {
                    phosg::strip_trailing_whitespace(reg_arg);
                    phosg::strip_leading_whitespace(reg_arg);
                    add_reg(parse_reg(reg_arg), false);
                  }
                  break;
                }
                case Type::I8:
                  code_w.put_u8(stoll(arg, nullptr, 0));
                  break;
                case Type::I16:
                  code_w.put_u16l(stoll(arg, nullptr, 0));
                  break;
                case Type::I32:
                  code_w.put_u32l(stoll(arg, nullptr, 0));
                  break;
                case Type::FLOAT32:
                  code_w.put_f32l(stof(arg, nullptr));
                  break;
                case Type::CSTRING:
                  if (arg.starts_with("bin:")) {
                    add_cstr(phosg::parse_data_string(arg.substr(4)), true);
                  } else {
                    add_cstr(phosg::parse_data_string(arg), false);
                  }
                  break;
                default:
                  throw logic_error("unknown argument type");
              }
            }
          } catch (const exception& e) {
            throw runtime_error(std::format("(arg {}) {}", z + 1, e.what()));
          }
        }
      }

      if (use_args) {
        if ((opcode_def->opcode & 0xFF00) == 0x0000) {
          code_w.put_u8(opcode_def->opcode);
        } else {
          code_w.put_u16b(opcode_def->opcode);
        }
      }
    });
  }

  // Allow label table to be misaligned on x86 architectures in non-strict mode (some official quests do this)
  if (strict || !is_x86(ret.meta.version)) {
    while (code_w.size() & 3) {
      code_w.put_u8(0);
    }
  }

  // Assign all register numbers and patch the code section if needed
  reg_assigner.assign_all();
  for (size_t z = 0; z < 0x100; z++) {
    auto reg = reg_assigner.numbered_regs[z];
    if (!reg) {
      continue;
    }
    for (size_t offset : reg->offsets) {
      code_w.pput_u8(offset, reg->number);
    }
  }

  // Generate label table
  ssize_t label_table_size = labels_by_index.rbegin()->first + 1;
  vector<le_uint32_t> label_table;
  label_table.reserve(label_table_size);
  {
    auto it = labels_by_index.begin();
    for (ssize_t z = 0; z < label_table_size; z++) {
      if (it == labels_by_index.end()) {
        throw logic_error("function table size exceeds maximum function ID");
      } else if (it->first > z) {
        label_table.emplace_back(0xFFFFFFFF);
      } else if (it->first == z) {
        if (it->second->offset < 0) {
          throw runtime_error("label " + it->second->name + " does not have a valid offset");
        }
        label_table.emplace_back(it->second->offset);
        it++;
      } else if (it->first < z) {
        throw logic_error("missed label " + it->second->name + " when compiling function table");
      }
    }
  }

  // Generate header
  auto set_basic_header_fields = [&]<typename HeaderT>(HeaderT& header) -> void {
    ret.meta.text_offset = sizeof(header);
    ret.meta.label_table_offset = ret.meta.text_offset + code_w.size();
    ret.meta.total_size = ret.meta.label_table_offset + label_table.size() * sizeof(label_table[0]);
    header.text_offset = ret.meta.text_offset;
    header.label_table_offset = ret.meta.label_table_offset;
    header.size = ret.meta.total_size;
    header.unknown_a1 = ret.meta.header_unknown_a1;
    header.unknown_a2 = ret.meta.header_unknown_a2;
  };
  phosg::StringWriter w;
  switch (ret.meta.version) {
    case Version::DC_NTE: {
      PSOQuestHeaderDCNTE header;
      set_basic_header_fields(header);
      header.name.encode(ret.meta.name, Language::JAPANESE);
      w.put(header);
      break;
    }
    case Version::DC_11_2000: {
      PSOQuestHeaderDC112000 header;
      set_basic_header_fields(header);
      header.name.encode(ret.meta.name, ret.meta.language);
      header.short_description.encode(ret.meta.short_description, ret.meta.language);
      header.long_description.encode(ret.meta.long_description, ret.meta.language);
      break;
    }
    case Version::DC_V1:
    case Version::DC_V2: {
      PSOQuestHeaderDC header;
      set_basic_header_fields(header);
      header.language = (ret.meta.header_language >= 0) ? static_cast<Language>(ret.meta.header_language) : ret.meta.language;
      header.unknown_a3 = ret.meta.header_unknown_a3;
      header.quest_number = ret.meta.quest_number;
      header.name.encode(ret.meta.name, ret.meta.language);
      header.short_description.encode(ret.meta.short_description, ret.meta.language);
      header.long_description.encode(ret.meta.long_description, ret.meta.language);
      w.put(header);
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      PSOQuestHeaderPC header;
      set_basic_header_fields(header);
      header.language = (ret.meta.header_language >= 0) ? static_cast<Language>(ret.meta.header_language) : ret.meta.language;
      header.unknown_a3 = ret.meta.header_unknown_a3;
      header.quest_number = ret.meta.quest_number;
      header.name.encode(ret.meta.name, ret.meta.language);
      header.short_description.encode(ret.meta.short_description, ret.meta.language);
      header.long_description.encode(ret.meta.long_description, ret.meta.language);
      w.put(header);
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      PSOQuestHeaderV3 header;
      set_basic_header_fields(header);
      header.language = (ret.meta.header_language >= 0) ? static_cast<Language>(ret.meta.header_language) : ret.meta.language;
      header.unknown_a3 = ret.meta.header_unknown_a3;
      header.quest_number = ret.meta.quest_number;
      header.name.encode(ret.meta.name, ret.meta.language);
      header.short_description.encode(ret.meta.short_description, ret.meta.language);
      header.long_description.encode(ret.meta.long_description, ret.meta.language);
      w.put(header);
      break;
    }
    case Version::BB_V4: {
      PSOQuestHeaderBB header;
      set_basic_header_fields(header);
      header.quest_number = ret.meta.quest_number;
      header.unknown_a6 = ret.meta.header_unknown_a6;
      if (ret.meta.header_episode >= 0) {
        header.episode = ret.meta.header_episode;
      } else if (ret.meta.episode == Episode::EP4) {
        header.episode = 2;
      } else if (ret.meta.episode == Episode::EP2) {
        header.episode = 1;
      } else {
        header.episode = 0;
      }
      header.max_players = ret.meta.max_players;
      header.joinable = ret.meta.joinable ? 1 : 0;
      header.unknown_a4 = ret.meta.header_unknown_a4;
      header.name.encode(ret.meta.name, ret.meta.language);
      header.short_description.encode(ret.meta.short_description, ret.meta.language);
      header.long_description.encode(ret.meta.long_description, ret.meta.language);
      header.unknown_a5 = ret.meta.header_unknown_a5;
      header.solo_unlock_flags.clear(0xFFFF);
      for (size_t z = 0; z < ret.meta.solo_unlock_flags.size(); z++) {
        header.solo_unlock_flags[z] = ret.meta.solo_unlock_flags[z];
      }
      phosg::StringReader code_r(code_w.str());
      for (size_t z = 0; z < bb_map_designate_args_offsets.size(); z++) {
        code_r.go(bb_map_designate_args_offsets[z]);
        auto& fa = header.floor_assignments[z];
        fa.floor = code_r.get_u8();
        fa.area = code_r.get_u8();
        fa.type = code_r.get_u8();
        fa.layout_var = code_r.get_u8();
        fa.entities_var = code_r.get_u8();
        fa.unused.clear(0);
      }
      for (size_t z = 0; z < ret.meta.create_item_mask_entries.size(); z++) {
        header.create_item_mask_entries[z] = ret.meta.create_item_mask_entries[z];
      }
      w.put(header);
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }
  w.write(code_w.str());
  w.write(label_table.data(), label_table.size() * sizeof(label_table[0]));
  ret.data = std::move(w.str());

  return ret;
}

void populate_quest_metadata_from_script(
    QuestMetadata& meta, const void* data, size_t size, Version version, Language language) {
  meta.version = version;
  meta.language = language;

  phosg::StringReader r(data, size);
  switch (meta.version) {
    case Version::DC_NTE: {
      const auto& header = r.get<PSOQuestHeaderDCNTE>();
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.episode = Episode::EP1;
      meta.max_players = 4;
      meta.name = header.name.decode(language);
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = phosg::fnv1a32(meta.name);
      }
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      meta.language = Language::JAPANESE;
      break;
    }
    case Version::DC_11_2000: {
      const auto& header = r.get<PSOQuestHeaderDC112000>();
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.episode = Episode::EP1;
      meta.max_players = 4;
      meta.language = (language == Language::UNKNOWN) ? Language::JAPANESE : language;
      meta.name = header.name.decode(meta.language);
      meta.short_description = header.short_description.decode(meta.language);
      meta.long_description = header.long_description.decode(meta.language);
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = phosg::fnv1a32(meta.name);
      }
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      break;
    }
    case Version::DC_V1:
    case Version::DC_V2: {
      const auto& header = r.get<PSOQuestHeaderDC>();
      meta.header_language = static_cast<uint8_t>(header.language);
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.header_unknown_a3 = header.unknown_a3;
      meta.episode = Episode::EP1;
      meta.max_players = 4;
      meta.language = (language != Language::UNKNOWN) ? language : ((static_cast<size_t>(header.language) < 5) ? header.language : Language::ENGLISH);
      meta.name = header.name.decode(meta.language);
      meta.short_description = header.short_description.decode(meta.language);
      meta.long_description = header.long_description.decode(meta.language);
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = header.quest_number;
      }
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      const auto& header = r.get<PSOQuestHeaderPC>();
      meta.header_language = static_cast<uint8_t>(header.language);
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.header_unknown_a3 = header.unknown_a3;
      meta.episode = Episode::EP1;
      meta.max_players = 4;
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = header.quest_number;
      }
      meta.language = (language != Language::UNKNOWN) ? language : ((static_cast<size_t>(header.language) < 8) ? header.language : Language::ENGLISH);
      meta.name = header.name.decode(meta.language);
      meta.short_description = header.short_description.decode(meta.language);
      meta.long_description = header.long_description.decode(meta.language);
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      // Note: This codepath handles Episode 3 quest scripts, which are not the same as Episode 3 maps and download
      // quests. Quest scripts (handled here) are only used offline in story mode, but can be disassembled with
      // disassemble_quest_script, hence we need to be able to parse them.
      const auto& header = r.get<PSOQuestHeaderV3>();
      meta.header_language = static_cast<uint8_t>(header.language);
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.header_unknown_a3 = header.unknown_a3;
      meta.episode = Episode::EP1;
      meta.max_players = 4;
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = header.quest_number;
      }
      meta.language = (language != Language::UNKNOWN) ? language : ((static_cast<size_t>(header.language) < 5) ? header.language : Language::ENGLISH);
      meta.name = header.name.decode(meta.language);
      meta.short_description = header.short_description.decode(meta.language);
      meta.long_description = header.long_description.decode(meta.language);
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      break;
    }
    case Version::BB_V4: {
      const auto& header = r.get<PSOQuestHeaderBBBase>();
      meta.header_episode = header.episode;
      meta.header_unknown_a1 = header.unknown_a1;
      meta.header_unknown_a2 = header.unknown_a2;
      meta.header_unknown_a4 = header.unknown_a4;
      meta.header_unknown_a6 = header.unknown_a6;
      meta.episode = episode_for_quest_episode_number(header.episode);
      meta.joinable |= header.joinable;
      meta.max_players = header.max_players;
      if (meta.quest_number == 0xFFFFFFFF) {
        meta.quest_number = header.quest_number;
      }
      meta.language = (language != Language::UNKNOWN) ? language : Language::ENGLISH;
      meta.name = header.name.decode(meta.language);
      meta.short_description = header.short_description.decode(meta.language);
      meta.long_description = header.long_description.decode(meta.language);
      // Quests saved with Qedit may not have the full header, so only parse the full header if the code and function
      // table offsets don't point to space within it
      if ((header.text_offset >= sizeof(PSOQuestHeaderBB)) && (header.label_table_offset >= sizeof(PSOQuestHeaderBB))) {
        r.go(0);
        const auto& header = r.get<PSOQuestHeaderBB>();
        meta.header_unknown_a5 = header.unknown_a5;
        for (size_t z = 0; z < header.solo_unlock_flags.size(); z++) {
          uint16_t flag = header.solo_unlock_flags[z];
          if (flag != 0xFFFF) {
            meta.solo_unlock_flags.emplace_back(flag);
          }
        }
        for (size_t z = 0; z < header.create_item_mask_entries.size(); z++) {
          const auto& item = header.create_item_mask_entries[z];
          if (!item.is_valid()) {
            break;
          }
          meta.create_item_mask_entries.emplace_back(item);
        }
      }
      meta.text_offset = header.text_offset;
      meta.label_table_offset = header.label_table_offset;
      meta.total_size = header.size;
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }

  const auto& opcodes = opcodes_for_version(meta.version);
  bool version_has_args = F_HAS_ARGS & v_flag(meta.version);

  struct RegisterFile {
    // All registers are initially zero
    size_t current_checkpoint = 0;
    struct Register {
      size_t checkpoint = 0;
      uint32_t known_value = 0;
      bool written = false;
    };
    std::array<Register, 0x100> regs;

    std::string str() const {
      string ret = "[";
      for (size_t z = 0; z < this->regs.size(); z++) {
        if (this->is_valid(z) && this->regs[z].written) {
          if (ret.size() > 1) {
            ret += ",";
          }
          ret += std::format("r{}={}", z, this->get(z));
        }
      }
      ret += "]";
      return ret;
    }

    uint32_t get(size_t which) const {
      const auto& reg = this->regs[which & 0xFF];
      if (reg.checkpoint >= this->current_checkpoint) {
        return reg.known_value;
      }
      throw runtime_error(std::format("value for r{} not known", which));
    }
    inline void set(size_t which, uint32_t value) {
      auto& reg = this->regs[which & 0xFF];
      reg.known_value = value;
      reg.checkpoint = this->current_checkpoint;
      reg.written = true;
    }
    inline void invalidate(size_t which) {
      this->regs[which & 0xFF].checkpoint = this->current_checkpoint - 1;
    }
    inline void invalidate_sequence(size_t first, size_t count) {
      if (count > 0x100) {
        throw runtime_error("invalid count in invalidate_sequence");
      }
      for (size_t z = 0; z < count; z++) {
        this->invalidate(first);
      }
    }
    inline void invalidate_all() {
      this->current_checkpoint++;
    }
    inline bool is_valid(size_t which) const {
      return this->regs[which & 0xFF].checkpoint >= this->current_checkpoint;
    }
  };

  auto get_label_offset = [&](size_t label) -> uint32_t {
    return meta.text_offset + r.pget_u32l(meta.label_table_offset + 4 * label);
  };

  // The set_episode opcode and floor remapping opcodes should always be in the first function (0), so we simulate
  // that. But battle and challenge quests can also have setup opcodes in the floor handlers, so we have to simulate
  // those too.
  deque<uint32_t> pending_fn_offsets{get_label_offset(0)};
  unordered_set<uint32_t> done_fn_offsets;
  shared_ptr<BattleRules> battle_rules;
  meta.assign_default_floors();
  while (!pending_fn_offsets.empty()) {
    uint32_t start_offset = pending_fn_offsets.front();
    pending_fn_offsets.pop_front();
    if (!done_fn_offsets.emplace(start_offset).second) {
      continue;
    }
    // phosg::fwrite_fmt(stderr, "Trace: examining function starting at {:X}\n", start_offset - code_offset);

    vector<uint32_t> args_list;
    RegisterFile regs;
    r.go(start_offset);
    try {
      while (!r.eof()) {
        uint16_t opcode = r.get_u8();
        if ((opcode & 0xFE) == 0xF8) {
          opcode = (opcode << 8) | r.get_u8();
        }

        const QuestScriptOpcodeDefinition* def = nullptr;
        try {
          def = opcodes.at(opcode);
        } catch (const out_of_range&) {
        }
        if (def == nullptr) {
          throw runtime_error(std::format("unknown quest opcode {:04X}", opcode));
        }
        // phosg::fwrite_fmt(stderr, "... Trace: {:08X} -> {:04X} {} with {}\n", r.where() - (opcode > 0x100 ? 2 : 1) - code_offset, opcode, def->name, regs.str());

        bool default_args = false;
        bool use_args = version_has_args && (def->flags & F_ARGS);

        auto get_single_int32_arg = [&]() -> uint32_t {
          if (use_args) {
            if (args_list.size() != 1) {
              throw runtime_error(std::format("incorrect argument count to {}", def->name));
            }
            return args_list[0];
          } else {
            return r.get_u32l();
          }
        };

        auto simulate_arithmetic_opcode = [&](bool is_imm, auto fn) {
          uint8_t reg = r.get_u8();
          uint32_t value;
          if (is_imm) {
            value = r.get_u32l();
          } else {
            uint8_t src_reg = r.get_u8();
            if (regs.is_valid(src_reg)) {
              value = regs.get(src_reg);
            } else {
              regs.invalidate(reg);
              return;
            }
          }
          if (regs.is_valid(reg)) {
            regs.set(reg, fn(regs.get(reg), value));
          }
        };

        switch (opcode) {
          case 0x0000: // nop
            break;

          case 0x0001: // ret
          case 0x0002: // sync
            // We stop analyzing at sync because all of the opcodes we care about must happen on the first frame the
            // thread runs (either in the start label or the floor handler).
            r.go(r.size());
            break;

          case 0x0006: // va_end
            regs.invalidate_sequence(1, 7);
            break;

          case 0x0007: // va_call
            r.skip(2);
            regs.invalidate_all();
            break;

          case 0x0008: { // let
            uint8_t a = r.get_u8();
            regs.set(a, regs.get(r.get_u8()));
            break;
          }
          case 0x0009: { // leti
            uint8_t a = r.get_u8();
            regs.set(a, r.get_u32l());
            break;
          }
          case 0x000A: { // leta (v1/v2); letb (v3/v4)
            uint8_t a = r.get_u8();
            if (is_v1_or_v2(meta.version)) { // leta
              regs.invalidate(a);
              r.skip(1);
            } else { // letb
              regs.set(a, r.get_u8());
            }
            break;
          }
          case 0x000B: { // letw
            uint8_t a = r.get_u8();
            regs.set(a, r.get_u16l());
            break;
          }
          case 0x000C: // leta (v3/v4)
            regs.invalidate(r.get_u8());
            r.skip(1);
            break;
          case 0x000D: // leto
            regs.invalidate(r.get_u8());
            r.skip(2);
            break;

          case 0x0010: // set
            regs.set(r.get_u8(), 1);
            break;
          case 0x0011: // clear
            regs.set(r.get_u8(), 0);
            break;
          case 0x0012: { // rev
            uint8_t a = r.get_u8();
            if (regs.is_valid(a)) {
              regs.set(a, !regs.get(a));
            }
            break;
          }

          case 0x0018: // add
          case 0x0019: // addi
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a + b; });
            break;
          case 0x001A: // sub
          case 0x001B: // subi
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a - b; });
            break;
          case 0x001C: // mul
          case 0x001D: // muli
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a * b; });
            break;
          case 0x001E: // div
          case 0x001F: // divi
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a / b; });
            break;
          case 0x0020: // and
          case 0x0021: // andi
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a & b; });
            break;
          case 0x0022: // or
          case 0x0023: // ori
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a | b; });
            break;
          case 0x0024: // xor
          case 0x0025: // xori
            simulate_arithmetic_opcode(opcode & 1, [](uint32_t a, uint32_t b) -> uint32_t { return a ^ b; });
            break;
          case 0x0026: // mod
          case 0x0027: // modi
            simulate_arithmetic_opcode((opcode & 1) ^ (opcode >> 7), [](uint32_t a, uint32_t b) -> uint32_t { return static_cast<int32_t>(a) % static_cast<int32_t>(b); });
            break;
          case 0x00E9: // mod2
          case 0x00EA: // modi2
            simulate_arithmetic_opcode((opcode & 1) ^ (opcode >> 7), [](uint32_t a, uint32_t b) -> uint32_t { return a % b; });
            break;

          case 0x0028: { // jmp
            uint32_t offset = get_label_offset(r.get_u16l());
            if (!done_fn_offsets.emplace(offset).second) {
              r.go(r.size());
            } else {
              r.go(offset);
            }
            break;
          }
          case 0x0029: // call
            r.skip(2);
            regs.invalidate_all();
            break;

          case 0x002C: // jmp_eq
          case 0x002E: // jmp_ne
          case 0x0030: // ujmp_gt
          case 0x0032: // jmp_gt
          case 0x0034: // ujmp_lt
          case 0x0036: // jmp_lt
          case 0x0038: // ujmp_ge
          case 0x003A: // jmp_ge
          case 0x003C: // ujmp_le
          case 0x003E: // jmp_le
            // TODO: We can simulate these if the reg values are known
            r.skip(4); // 2x R_REG, LABEL16
            break;
          case 0x002D: // jmpi_eq
          case 0x002F: // jmpi_ne
          case 0x0031: // ujmpi_gt
          case 0x0033: // jmpi_gt
          case 0x0035: // ujmpi_lt
          case 0x0037: // jmpi_lt
          case 0x0039: // ujmpi_ge
          case 0x003B: // jmpi_ge
          case 0x003D: // ujmpi_le
          case 0x003F: // jmpi_le
            // TODO: We can simulate these if the reg value is known
            r.skip(7); // R_REG, I32, LABEL16
            break;

          case 0x0040: // switch_jmp
          case 0x0041: { // switch_call
            r.get_u8();
            r.skip(2 * r.get_u8());
            if (opcode == 0x0041) {
              regs.invalidate_all();
            }
            break;
          }

          case 0x0045: { // stack_popm
            uint8_t a = r.get_u8();
            regs.invalidate_sequence(a, r.get_u32l());
            break;
          }

          case 0x0048: // arg_pushr
            args_list.emplace_back(regs.get(r.get_u8()));
            break;
          case 0x0049: // arg_pushl
            args_list.emplace_back(r.get_u32l());
            break;
          case 0x004A: // arg_pushb
            args_list.emplace_back(r.get_u8());
            break;
          case 0x004B: // arg_pushw
            args_list.emplace_back(r.get_u16l());
            break;
          case 0x004C: // arg_pusha
            r.skip(1);
            break;
          case 0x004D: // arg_pusho
            args_list.emplace_back(get_label_offset(r.get_u16l()));
            break;
          case 0x004E: // arg_pushs
            args_list.emplace_back(r.where());
            if (uses_utf16(meta.version)) {
              while (r.get_u16l()) {
              }
            } else {
              while (r.get_u8()) {
              }
            }
            break;

          case 0x0095: { // set_floor_handler
            // We have to follow these because battle quests define rules in the floor handler, not in the start label
            if (use_args) {
              if (args_list.size() != 2) {
                throw runtime_error("incorrect argument count for set_floor_handler");
              }
              pending_fn_offsets.emplace_back(get_label_offset(args_list[1]));
            } else {
              r.skip(4); // Floor number
              pending_fn_offsets.emplace_back(get_label_offset(r.get_u16l()));
            }
            break;
          }

          case 0x00C4: { // map_designate
            uint8_t base_reg = r.get_u8();
            uint32_t floor = regs.get(base_reg);
            if (floor < meta.floor_assignments.size()) {
              auto& fa = meta.floor_assignments[floor];
              fa.area = floor;
              fa.type = regs.get(base_reg + 1);
              fa.layout_var = regs.get(base_reg + 2);
              fa.entities_var = 0;
            }
            // phosg::fwrite_fmt(stderr, ">>> Trace: map_designate fa[{}]={}\n", floor, floor);
            break;
          }

          case 0xF80D: { // map_designate_ex
            uint8_t base_reg = r.get_u8();
            uint32_t floor = regs.get(base_reg);
            if (floor < meta.floor_assignments.size()) {
              auto& fa = meta.floor_assignments[floor];
              fa.area = regs.get(base_reg + 1);
              fa.type = regs.get(base_reg + 2);
              fa.layout_var = regs.get(base_reg + 3);
              fa.entities_var = regs.get(base_reg + 4);
            }
            // phosg::fwrite_fmt(stderr, ">>> Trace: map_designate_ex fa[{}]={}\n", floor, area);
            break;
          }

          case 0xF811: // clear_ba_rules
            battle_rules = make_shared<BattleRules>();
            meta.battle_rules = battle_rules;
            break;

          case 0xF812: // ba_set_tech_disk_mode
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->tech_disk_mode = BattleRules::TechDiskMode::FORBID_ALL;
                break;
              case 1:
                battle_rules->tech_disk_mode = BattleRules::TechDiskMode::ALLOW;
                break;
              case 2:
                battle_rules->tech_disk_mode = BattleRules::TechDiskMode::LIMIT_LEVEL;
                break;
              default:
                throw runtime_error("invalid battle tech disk mode");
            }
            break;

          case 0xF813: // ba_set_weapon_and_armor_mode
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->weapon_and_armor_mode = BattleRules::WeaponAndArmorMode::FORBID_ALL;
                break;
              case 1:
                battle_rules->weapon_and_armor_mode = BattleRules::WeaponAndArmorMode::ALLOW;
                break;
              case 2:
                battle_rules->weapon_and_armor_mode = BattleRules::WeaponAndArmorMode::CLEAR_AND_ALLOW;
                break;
              case 3:
                battle_rules->weapon_and_armor_mode = BattleRules::WeaponAndArmorMode::FORBID_RARES;
                break;
              default:
                throw runtime_error("invalid battle weapon and armor mode");
            }
            break;

          case 0xF814: // ba_set_forbid_mags
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->mag_mode = BattleRules::MagMode::FORBID_ALL;
                break;
              case 1:
                battle_rules->mag_mode = BattleRules::MagMode::ALLOW;
                break;
              default:
                throw runtime_error("invalid battle mag mode");
            }
            break;

          case 0xF815: // ba_set_tool_mode
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->tool_mode = BattleRules::ToolMode::FORBID_ALL;
                break;
              case 1:
                battle_rules->tool_mode = BattleRules::ToolMode::ALLOW;
                break;
              case 2:
                battle_rules->tool_mode = BattleRules::ToolMode::CLEAR_AND_ALLOW;
                break;
              default:
                throw runtime_error("invalid battle tool mode");
            }
            break;

          case 0xF816: // ba_set_trap_mode
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->trap_mode = BattleRules::TrapMode::DEFAULT;
                break;
              case 1:
                battle_rules->trap_mode = BattleRules::TrapMode::ALL_PLAYERS;
                break;
              default:
                throw runtime_error("invalid battle trap mode");
            }
            break;

          case 0xF817: // ba_set_unused_F817
            battle_rules->unused_F817 = get_single_int32_arg();
            break;

          case 0xF818: // ba_set_respawn
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->respawn_mode = BattleRules::RespawnMode::FORBID;
                break;
              case 1:
                battle_rules->respawn_mode = BattleRules::RespawnMode::ALLOW;
                break;
              case 2:
                battle_rules->respawn_mode = BattleRules::RespawnMode::LIMIT_LIVES;
                break;
              default:
                throw runtime_error("invalid battle tech disk mode");
            }
            break;

          case 0xF819: // ba_set_replace_char
            battle_rules->replace_char = get_single_int32_arg();
            break;

          case 0xF81A: // ba_dropwep
            battle_rules->drop_weapon = get_single_int32_arg();
            break;

          case 0xF81B: // ba_teams
            battle_rules->is_teams = get_single_int32_arg();
            break;

          case 0xF81D: // ba_death_lvl_up
            battle_rules->death_level_up = get_single_int32_arg();
            break;

          case 0xF81E: // ba_set_meseta_drop_mode
            switch (get_single_int32_arg()) {
              case 0:
                battle_rules->meseta_mode = BattleRules::MesetaMode::ALLOW;
                break;
              case 1:
                battle_rules->meseta_mode = BattleRules::MesetaMode::FORBID_ALL;
                break;
              case 2:
                battle_rules->meseta_mode = BattleRules::MesetaMode::CLEAR_AND_ALLOW;
                break;
              default:
                throw runtime_error("invalid battle meseta mode");
            }
            break;

          case 0xF823: // set_cmode_char_template
            meta.challenge_template_index = get_single_int32_arg();
            // phosg::fwrite_fmt(stderr, ">>> Trace: meta.challenge_template_index = {}\n", meta.challenge_template_index);
            break;

          case 0xF824: // set_cmode_difficulty
            meta.challenge_difficulty = static_cast<Difficulty>(get_single_int32_arg());
            if (static_cast<size_t>(meta.challenge_difficulty) > 3) {
              throw std::runtime_error("invalid challenge mode difficulty");
            }
            // phosg::fwrite_fmt(stderr, ">>> Trace: meta.challenge_difficulty = {}\n", meta.challenge_difficulty);
            break;

          case 0xF825: { // exp_multiplication
            uint8_t a = r.get_u8();
            meta.challenge_exp_multiplier = static_cast<float>(regs.get(a)) + static_cast<float>(regs.get(a + 1)) / static_cast<float>(regs.get(a + 2));
            // phosg::fwrite_fmt(stderr, ">>> Trace: meta.challenge_exp_multiplier = {}\n", meta.challenge_exp_multiplier);
            break;
          }

          case 0xF851: { // ba_set_trap_count
            uint8_t a = r.get_u8();
            // Why did they do this? Why not just make the indexes the same?
            std::array<size_t, 4> trap_types = {0, 2, 3, 1};
            battle_rules->trap_counts.at(trap_types.at(regs.get(a))) = regs.get(a + 1);
            break;
          }

          case 0xF852: // ba_hide_target_reticle
            battle_rules->hide_target_reticle = get_single_int32_arg();
            break;

          case 0xF85E: // ba_enable_sonar
            battle_rules->enable_sonar = get_single_int32_arg();
            break;

          case 0xF85F: // ba_sonar_count
            battle_rules->sonar_count = get_single_int32_arg();
            break;

          case 0xF86B: // ba_set_box_drop_area
            battle_rules->box_drop_area = regs.get(r.get_u8());
            break;

          case 0xF86F: // ba_set_lives
            battle_rules->lives = get_single_int32_arg();
            break;

          case 0xF870: // ba_set_max_tech_level
            battle_rules->max_tech_level = get_single_int32_arg();
            break;

          case 0xF871: // ba_set_char_level
            battle_rules->char_level = get_single_int32_arg();
            break;

          case 0xF872: // ba_set_time_limit
            battle_rules->time_limit = get_single_int32_arg();
            break;

          case 0xF88C: // get_game_version
            if (is_v1_or_v2(meta.version)) {
              regs.set(r.get_u8(), 2);
            } else if (is_gc(meta.version)) {
              regs.set(r.get_u8(), 3);
            } else {
              regs.set(r.get_u8(), 4);
            }
            break;

          case 0xF89E: // ba_forbid_scape_dolls
            battle_rules->forbid_scape_dolls = get_single_int32_arg();
            break;

          case 0xF8A8: // ba_death_tech_level_up
            battle_rules->death_tech_level_up = get_single_int32_arg();
            break;

          case 0xF8BC: // set_episode
            switch (r.get_u32l()) {
              case 0:
                meta.episode = Episode::EP1;
                break;
              case 1:
                meta.episode = Episode::EP2;
                break;
              case 2:
                if (!is_v4(meta.version)) {
                  throw runtime_error("invalid argument to set_episode");
                }
                meta.episode = Episode::EP4;
                break;
              default:
                throw runtime_error("invalid argument to set_episode");
            }
            meta.assign_default_floors();
            break;

          case 0xF932: // set_episode2
            // This takes a register as an argument, so we can't handle it here
            throw runtime_error("quest uses set_episode2");

          case 0xF951: { // bb_map_designate
            uint8_t floor = r.get_u8();
            if (floor < meta.floor_assignments.size()) {
              auto& fa = meta.floor_assignments[floor];
              if (fa.floor != floor) {
                throw std::logic_error("FloorAssignment\'s floor field is incorrect");
              }
              fa.area = r.get_u8();
              fa.type = r.get_u8();
              fa.layout_var = r.get_u8();
              fa.entities_var = r.get_u8();
              meta.bb_map_designate_opcodes.emplace_back(fa);
              // phosg::fwrite_fmt(stderr, ">>> Trace: bb_map_designate fa[{}]={}\n", floor, meta.area_for_floor.at(floor));
            } else {
              // Maybe we should throw in this case?
              r.skip(4);
            }
            break;
          }

          default:
            // phosg::fwrite_fmt(stderr, "Trace: unhandled opcode at {:X}: {:04X} ({})\n", r.where() - code_offset, opcode, def->name);
            default_args = true;
        }

        if (default_args) {
          // Read and skip all immediate args, and invalidate any registers
          for (size_t arg_index = 0; arg_index < def->args.size(); arg_index++) {
            const auto& arg = def->args[arg_index];

            using Type = QuestScriptOpcodeDefinition::Argument::Type;
            switch (arg.type) {
              case Type::W_REG:
                regs.invalidate(use_args ? args_list.at(arg_index) : r.get_u8());
                break;
              case Type::W_REG_SET_FIXED:
                regs.invalidate_sequence(use_args ? args_list.at(arg_index) : r.get_u8(), arg.count);
                break;
              case Type::I8:
              case Type::R_REG:
              case Type::R_REG_SET_FIXED:
                r.skip(use_args ? 0 : 1);
                break;
              case Type::I16:
              case Type::LABEL16:
                r.skip(use_args ? 0 : 2);
                break;
              case Type::W_REG32:
                regs.invalidate(use_args ? args_list.at(arg_index) : r.get_u32l());
                break;
              case Type::W_REG32_SET_FIXED:
                regs.invalidate_sequence(use_args ? args_list.at(arg_index) : r.get_u32l(), arg.count);
                break;
              case Type::I32:
              case Type::FLOAT32:
              case Type::R_REG32:
              case Type::R_REG32_SET_FIXED:
              case Type::LABEL32:
                r.skip(use_args ? 0 : 4);
                break;
              case Type::LABEL16_SET:
                if (use_args) {
                  throw logic_error("LABEL16_SET cannot be encoded with F_ARGS");
                }
                r.skip(r.get_u8() * 2);
                break;
              case Type::R_REG_SET:
                if (use_args) {
                  throw logic_error("R_REG_SET cannot be encoded with F_ARGS");
                }
                r.skip(r.get_u8());
                break;
              case Type::CSTRING:
                if (!use_args) {
                  if (uses_utf16(meta.version)) {
                    while (r.get_u16l()) {
                    }
                  } else {
                    while (r.get_u8()) {
                    }
                  }
                }
                break;
              default:
                throw logic_error("invalid argument type");
            }
          }
        }

        if (!(def->flags & F_PUSH_ARG)) {
          args_list.clear();
        }
      }
    } catch (const runtime_error& e) {
      // phosg::fwrite_fmt(stderr, "!!! Trace: function skipped: {}\n", e.what());
    }
  }
}
