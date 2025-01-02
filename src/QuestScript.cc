#include "QuestScript.hh"

#include <stdint.h>
#include <string.h>

#include <array>
#include <deque>
#include <map>
#include <phosg/Math.hh>
#include <phosg/Strings.hh>
#include <set>
#include <unordered_map>
#include <vector>

#ifdef HAVE_RESOURCE_FILE
#include <resource_file/Emulators/PPC32Emulator.hh>
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>
#endif

#include "BattleParamsIndex.hh"
#include "CommandFormats.hh"
#include "Compression.hh"
#include "StaticGameData.hh"

using namespace std;

// This file documents PSO's quest script execution system.

// The quest execution system has several relevant data structures:
// - The quest script. This is a stream of binary data containing opcodes (as
//   defined below), each followed by their arguments.
// - The function table. This is a list of offsets into the quest script which
//   can be used as targets for jumps and calls, as well as references to large
//   data structures that don't fit in quest opcode arguments.
// - The registers. There are 256 registers, referred to as r0-r255. In later
//   versions, registers may contain floating-point values, in which case
//   they're referred to as f0-f255 (but they still occupy the same memory as
//   r0-255).
// - The args list. This is a list of up to 8 values used for many quest
//   opcodes in v3 and later. These opcodes are preceded by one or more
//   arg_push opcodes, which allow scripts the ability to pass values from
//   immediate data, registers, labels, or even pointers to registers. Opcodes
//   that use the args list are tagged with F_ARGS below.
// - The stack. This is an array of 64 32-bit integers, which is used by the
//   call and ret opcodes (which push and pop offsets into the quest script),
//   but may also be used by the stack_push and stack_pop opcodes to work with
//   arbitrary data. There is protection from stack underflows (the caller
//   receives the value 0, or the thread terminates in case of the ret opcode),
//   but there is no protection from overflows.
// - Quest flags. These are a per-character array of 1024 single-bit flags
//   saved with the character data. (On Episode 3, there are 8192 instead.)
// - Quest counters. These are a per-character array of 16 32-bit values saved
//   with the character data. (On Episode 3, there are 48 instead.)
// - Event flags. These are an array of 0x100 bytes stored in the system file
//   (not the character file).

using AttackData = BattleParamsIndex::AttackData;
using ResistData = BattleParamsIndex::ResistData;
using MovementData = BattleParamsIndex::MovementData;

// bit_cast isn't in the standard place on macOS (it is apparently implicitly
// included by resource_dasm, but newserv can be built without resource_dasm)
// and I'm too lazy to go find the right header to include
template <typename ToT, typename FromT>
ToT as_type(const FromT& v) {
  static_assert(sizeof(FromT) == sizeof(ToT), "types are not the same size");
  ToT ret;
  memcpy(&ret, &v, sizeof(ToT));
  return ret;
}

static const char* name_for_header_episode_number(uint8_t episode) {
  static const array<const char*, 3> names = {"Episode1", "Episode2", "Episode4"};
  try {
    return names.at(episode);
  } catch (const out_of_range&) {
    return "Episode1  # invalid value in header";
  }
}

static TextEncoding encoding_for_language(uint8_t language) {
  return (language ? TextEncoding::ISO8859 : TextEncoding::SJIS);
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
      ret += phosg::string_printf("\\x%02hhX", ch);
    } else if (ch == '\'') {
      ret += "\\\'";
    } else if (ch == '\"') {
      ret += "\\\"";
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
      CSTRING,
      PLAYER_STATS,
      PLAYER_VISUAL_CONFIG,
      RESIST_DATA,
      ATTACK_DATA,
      MOVEMENT_DATA,
      IMAGE_DATA,
      BEZIER_CONTROL_POINT_DATA,
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

  uint16_t opcode;
  const char* name;
  const char* qedit_name;
  std::vector<Argument> args;
  uint16_t flags;

  QuestScriptOpcodeDefinition(
      uint16_t opcode,
      const char* name,
      const char* qedit_name,
      std::vector<Argument> args,
      uint16_t flags)
      : opcode(opcode),
        name(name),
        qedit_name(qedit_name),
        args(args),
        flags(flags) {}

  std::string str() const {
    string name_str = this->qedit_name ? phosg::string_printf("%s (qedit: %s)", this->name, this->qedit_name) : this->name;
    return phosg::string_printf("%04hX: %s flags=%04hX", this->opcode, name_str.c_str(), this->flags);
  }
};

constexpr uint16_t v_flag(Version v) {
  return (1 << static_cast<uint16_t>(v));
}

using Arg = QuestScriptOpcodeDefinition::Argument;

static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the QuestScript flags and opcode definitions table");

// F_PASS means the argument list isn't cleared after this opcode executes
static constexpr uint16_t F_PASS = 0x0001; // Version::PC_PATCH (unused for quests)
// F_ARGS means this opcode takes its arguments via the argument list on v3 and
// later. It has no effect on v2 and earlier.
static constexpr uint16_t F_ARGS = 0x0002; // Version::BB_PATCH (unused for quests)
// The following flags are used to specify which versions support each opcode.
static constexpr uint16_t F_DC_NTE = 0x0004; // Version::DC_NTE
static constexpr uint16_t F_DC_112000 = 0x0008; // Version::DC_11_2000
static constexpr uint16_t F_DC_V1 = 0x0010; // Version::DC_V1
static constexpr uint16_t F_DC_V2 = 0x0020; // Version::DC_V2
static constexpr uint16_t F_PC_NTE = 0x0040; // Version::PC_NTE
static constexpr uint16_t F_PC_V2 = 0x0080; // Version::PC_V2
static constexpr uint16_t F_GC_NTE = 0x0100; // Version::GC_NTE
static constexpr uint16_t F_GC_V3 = 0x0200; // Version::GC_V3
static constexpr uint16_t F_GC_EP3TE = 0x0400; // Version::GC_EP3_NTE
static constexpr uint16_t F_GC_EP3 = 0x0800; // Version::GC_EP3
static constexpr uint16_t F_XB_V3 = 0x1000; // Version::XB_V3
static constexpr uint16_t F_BB_V4 = 0x2000; // Version::BB_V4
// This flag specifies that the opcode ends a function (returns).
static constexpr uint16_t F_RET = 0x4000;
// This flag specifies that the opcode sets the current episode. This is used
// to automatically detect a quest's episode from its script.
static constexpr uint16_t F_SET_EPISODE = 0x8000;

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
static constexpr uint16_t F_V0_V2  = F_DC_NTE | F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint16_t F_V0_V4  = F_DC_NTE | F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint16_t F_V05_V2 =            F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint16_t F_V05_V4 =            F_DC_112000 | F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint16_t F_V1_V2  =                          F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint16_t F_V1_V4  =                          F_DC_V1 | F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint16_t F_V2     =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE;
static constexpr uint16_t F_V2_V3  =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3;
static constexpr uint16_t F_V2_V4  =                                    F_DC_V2 | F_PC_NTE | F_PC_V2 | F_GC_NTE | F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint16_t F_V3     =                                                                              F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3;
static constexpr uint16_t F_V3_V4  =                                                                              F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_XB_V3 | F_BB_V4;
static constexpr uint16_t F_V4     =                                                                                                                          F_BB_V4;
// clang-format on
static constexpr uint16_t F_HAS_ARGS = F_V3_V4;

// These are the argument data types. All values are stored little-endian in
// the script data, even on the GameCube.

// LABEL16 is a 16-bit index into the function table
static constexpr auto LABEL16 = Arg::Type::LABEL16;
// LABEL16_SET is a single byte specifying how many labels follow, followed by
// that many 16-bit indexes into the function table.
static constexpr auto LABEL16_SET = Arg::Type::LABEL16_SET;
// LABEL32 is a 32-bit index into the function table
static constexpr auto LABEL32 = Arg::Type::LABEL32;
// REG is a single byte specifying a register number (rXX or fXX)
static constexpr auto REG = Arg::Type::REG;
// REG_SET is a single byte specifying how many registers follow, followed by
// that many bytes specifying individual register numbers.
static constexpr auto REG_SET = Arg::Type::REG_SET;
// REG_SET_FIXED is a single byte specifying a register number, but the opcode
// implicitly uses the following registers as well. For example, if an opcode
// takes a {REG_SET_FIXED, 4} and the value 100 was passed to that opcode, only
// the byte 0x64 would appear in the script data, but the opcode would use
// r100, r101, r102, and r103.
static constexpr auto REG_SET_FIXED = Arg::Type::REG_SET_FIXED;
// REG32 is a 32-bit register number. The high 24 bits are unused.
static constexpr auto REG32 = Arg::Type::REG32;
// REG32_SET_FIXED is like REG_SET_FIXED, but uses a 32-bit register number.
// The high 24 bits are unused.
static constexpr auto REG32_SET_FIXED = Arg::Type::REG32_SET_FIXED;
// INT8, INT16, and INT32 are unsigned integers of various sizes
static constexpr auto INT8 = Arg::Type::INT8;
static constexpr auto INT16 = Arg::Type::INT16;
static constexpr auto INT32 = Arg::Type::INT32;
// FLOAT32 is a standard 32-bit float
static constexpr auto FLOAT32 = Arg::Type::FLOAT32;
// CSTRING is a sequence of nonzero bytes ending with a zero byte
static constexpr auto CSTRING = Arg::Type::CSTRING;

// These are shortcuts for the above types with some extra metadata, which the
// disassembler uses to annotate arguments or data sections.
static const Arg SCRIPT16(LABEL16, Arg::DataType::SCRIPT);
static const Arg SCRIPT16_SET(LABEL16_SET, Arg::DataType::SCRIPT);
static const Arg SCRIPT32(LABEL32, Arg::DataType::SCRIPT);
static const Arg DATA16(LABEL16, Arg::DataType::DATA);
static const Arg CSTRING_LABEL16(LABEL16, Arg::DataType::CSTRING);

static const Arg CLIENT_ID(INT32, 0, "client_id");
static const Arg ITEM_ID(INT32, 0, "item_id");
static const Arg FLOOR(INT32, 0, "floor");

static const QuestScriptOpcodeDefinition opcode_defs[] = {
    // The quest opcodes are defined below. Two-byte opcodes begin with F8 or
    // F9; all other opcodes are one byte. Unlike network commands and
    // subcommands, all versions use the same values for almost all opcodes
    // (there is one exception), but not all opcodes are supported on all
    // versions. The flags denote which versions support each opcode; opcodes
    // are defined multiple times below if their call signatures are different
    // across versions.

    // In the comments below, arguments are referred to with letters. The first
    // argument to an opcode would be regA (if it's a REG), the second is regB,
    // etc. The individual registers within a REG_SET_FIXED argument are
    // referred to as an array, as regsA[0], regsA[1], etc.

    // Does nothing
    {0x00, "nop", nullptr, {}, F_V0_V4},

    // Pops new PC off stack
    {0x01, "ret", nullptr, {}, F_V0_V4 | F_RET},

    // Stops execution for the current frame. Execution resumes immediately
    // after this opcode on the next frame.
    {0x02, "sync", nullptr, {}, F_V0_V4},

    // Exits entirely
    {0x03, "exit", nullptr, {INT32}, F_V0_V4},

    // Starts a new thread at labelA
    {0x04, "thread", nullptr, {SCRIPT16}, F_V0_V4},

    // Pushes r1-r7 to the stack
    {0x05, "va_start", nullptr, {}, F_V3_V4},

    // Pops r7-r1 from the stack
    {0x06, "va_end", nullptr, {}, F_V3_V4},

    // Replaces r1-r7 with the args stack, then calls labelA
    {0x07, "va_call", nullptr, {SCRIPT16}, F_V3_V4},

    // Copies a value from regB to regA
    {0x08, "let", nullptr, {REG, REG}, F_V0_V4},

    // Sets regA to valueB
    {0x09, "leti", nullptr, {REG, INT32}, F_V0_V4},

    // Sets regA to the memory address of regB. Note that this opcode was moved
    // to 0C in v3 and later.
    {0x0A, "leta", nullptr, {REG, REG}, F_V0_V2},

    // Sets regA to valueB
    {0x0A, "letb", nullptr, {REG, INT8}, F_V3_V4},

    // Sets regA to valueB
    {0x0B, "letw", nullptr, {REG, INT16}, F_V3_V4},

    // Sets regA to the memory address of regB
    {0x0C, "leta", nullptr, {REG, REG}, F_V3_V4},

    // Sets regA to the address of labelB
    {0x0D, "leto", nullptr, {REG, SCRIPT16}, F_V3_V4},

    // Sets regA to 1
    {0x10, "set", nullptr, {REG}, F_V0_V4},

    // Sets regA to 0
    {0x11, "clear", nullptr, {REG}, F_V0_V4},

    // Sets a regA to 0 if it's nonzero and vice versa
    {0x12, "rev", nullptr, {REG}, F_V0_V4},

    // Sets flagA to 1
    {0x13, "gset", nullptr, {INT16}, F_V0_V4},

    // Clears flagA to 0
    {0x14, "gclear", nullptr, {INT16}, F_V0_V4},

    // Inverts flagA
    {0x15, "grev", nullptr, {INT16}, F_V0_V4},

    // If regB is nonzero, sets flagA; otherwise, clears it
    {0x16, "glet", nullptr, {INT16, REG}, F_V0_V4},

    // Sets regB to the value of flagA
    {0x17, "gget", nullptr, {INT16, REG}, F_V0_V4},

    // regA += regB
    {0x18, "add", nullptr, {REG, REG}, F_V0_V4},

    // regA += valueB
    {0x19, "addi", nullptr, {REG, INT32}, F_V0_V4},

    // regA -= regB
    {0x1A, "sub", nullptr, {REG, REG}, F_V0_V4},

    // regA -= valueB
    {0x1B, "subi", nullptr, {REG, INT32}, F_V0_V4},

    // regA *= regB
    {0x1C, "mul", nullptr, {REG, REG}, F_V0_V4},

    // regA *= valueB
    {0x1D, "muli", nullptr, {REG, INT32}, F_V0_V4},

    // regA /= regB
    {0x1E, "div", nullptr, {REG, REG}, F_V0_V4},

    // regA /= valueB
    {0x1F, "divi", nullptr, {REG, INT32}, F_V0_V4},

    // regA &= regB
    {0x20, "and", nullptr, {REG, REG}, F_V0_V4},

    // regA &= valueB
    {0x21, "andi", nullptr, {REG, INT32}, F_V0_V4},

    // regA |= regB
    {0x22, "or", nullptr, {REG, REG}, F_V0_V4},

    // regA |= valueB
    {0x23, "ori", nullptr, {REG, INT32}, F_V0_V4},

    // regA ^= regB
    {0x24, "xor", nullptr, {REG, REG}, F_V0_V4},

    // regA ^= valueB
    {0x25, "xori", nullptr, {REG, INT32}, F_V0_V4},

    // regA %= regB
    {0x26, "mod", nullptr, {REG, REG}, F_V3_V4},

    // regA %= valueB
    {0x27, "modi", nullptr, {REG, INT32}, F_V3_V4},

    // Jumps to labelA
    {0x28, "jmp", nullptr, {SCRIPT16}, F_V0_V4},

    // Pushes the script offset immediately after this opcode and jumps to
    // labelA
    {0x29, "call", nullptr, {SCRIPT16}, F_V0_V4},

    // If all values in regsB are nonzero, jumps to labelA
    {0x2A, "jmp_on", nullptr, {SCRIPT16, REG_SET}, F_V0_V4},

    // If all values in regsB are zero, jumps to labelA
    {0x2B, "jmp_off", nullptr, {SCRIPT16, REG_SET}, F_V0_V4},

    // If regA == regB, jumps to labelC
    {0x2C, "jmp_eq", "jmp_=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA == valueB, jumps to labelC
    {0x2D, "jmpi_eq", "jmpi_=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA != regB, jumps to labelC
    {0x2E, "jmp_ne", "jmp_!=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA != valueB, jumps to labelC
    {0x2F, "jmpi_ne", "jmpi_!=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA > regB (unsigned), jumps to labelC
    {0x30, "ujmp_gt", "ujmp_>", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA > valueB (unsigned), jumps to labelC
    {0x31, "ujmpi_gt", "ujmpi_>", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA > regB (signed), jumps to labelC
    {0x32, "jmp_gt", "jmp_>", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA > valueB (signed), jumps to labelC
    {0x33, "jmpi_gt", "jmpi_>", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA < regB (unsigned), jumps to labelC
    {0x34, "ujmp_lt", "ujmp_<", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA < valueB (unsigned), jumps to labelC
    {0x35, "ujmpi_lt", "ujmpi_<", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA < regB (signed), jumps to labelC
    {0x36, "jmp_lt", "jmp_<", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA < valueB (signed), jumps to labelC
    {0x37, "jmpi_lt", "jmpi_<", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA >= regB (unsigned), jumps to labelC
    {0x38, "ujmp_ge", "ujmp_>=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA >= valueB (unsigned), jumps to labelC
    {0x39, "ujmpi_ge", "ujmpi_>=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA >= regB (signed), jumps to labelC
    {0x3A, "jmp_ge", "jmp_>=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA >= valueB (signed), jumps to labelC
    {0x3B, "jmpi_ge", "jmpi_>=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA <= regB (unsigned), jumps to labelC
    {0x3C, "ujmp_le", "ujmp_<=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA <= valueB (unsigned), jumps to labelC
    {0x3D, "ujmpi_le", "ujmpi_<=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // If regA <= regB (signed), jumps to labelC
    {0x3E, "jmp_le", "jmp_<=", {REG, REG, SCRIPT16}, F_V0_V4},

    // If regA <= valueB (signed), jumps to labelC
    {0x3F, "jmpi_le", "jmpi_<=", {REG, INT32, SCRIPT16}, F_V0_V4},

    // Jumps to labelsB[regA]
    {0x40, "switch_jmp", nullptr, {REG, SCRIPT16_SET}, F_V0_V4},

    // Calls labelsB[regA]
    {0x41, "switch_call", nullptr, {REG, SCRIPT16_SET}, F_V0_V4},

    // Does nothing
    {0x42, "nop_42", nullptr, {INT32}, F_V0_V2},

    // Pushes the value in regA to the stack
    {0x42, "stack_push", nullptr, {REG}, F_V3_V4},

    // Pops a value from the stack and puts it into regA
    {0x43, "stack_pop", nullptr, {REG}, F_V3_V4},

    // Pushes (valueB) regs in increasing order starting at regA
    {0x44, "stack_pushm", nullptr, {REG, INT32}, F_V3_V4},

    // Pops (valueB) regs in decreasing order ending at regA
    {0x45, "stack_popm", nullptr, {REG, INT32}, F_V3_V4},

    // Appends regA to the args list
    {0x48, "arg_pushr", nullptr, {REG}, F_V3_V4 | F_PASS},

    // Appends valueA to the args list
    {0x49, "arg_pushl", nullptr, {INT32}, F_V3_V4 | F_PASS},
    {0x4A, "arg_pushb", nullptr, {INT8}, F_V3_V4 | F_PASS},
    {0x4B, "arg_pushw", nullptr, {INT16}, F_V3_V4 | F_PASS},

    // Appends the memory address of regA to the args list
    {0x4C, "arg_pusha", nullptr, {REG}, F_V3_V4 | F_PASS},

    // Appends the script offset of labelA to the args list
    {0x4D, "arg_pusho", nullptr, {LABEL16}, F_V3_V4 | F_PASS},

    // Appends strA to the args list
    {0x4E, "arg_pushs", nullptr, {CSTRING}, F_V3_V4 | F_PASS},

    // Creates dialogue with an object/NPC (valueA) starting with message strB
    {0x50, "message", nullptr, {INT32, CSTRING}, F_V0_V4 | F_ARGS},

    // Prompts the player with a list of choices (strB; items separated by
    // newlines) and returns the index of their choice in regA
    {0x51, "list", nullptr, {REG, CSTRING}, F_V0_V4 | F_ARGS},

    // Fades from black
    {0x52, "fadein", nullptr, {}, F_V0_V4},

    // Fades to black
    {0x53, "fadeout", nullptr, {}, F_V0_V4},

    // Plays a sound effect
    {0x54, "sound_effect", "se", {INT32}, F_V0_V4 | F_ARGS},

    // Plays a fanfare (clear.adx if valueA is 0, or miniclear.adx if it's 1).
    // Note: There is no bounds check on this; values other than 0 or 1 will
    // result in undefined behavior.
    {0x55, "bgm", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Does nothing
    {0x56, "nop_56", nullptr, {}, F_V0_V2},
    {0x57, "nop_57", nullptr, {}, F_V0_V2},
    {0x58, "nop_58", "enable", {INT32}, F_V0_V2},
    {0x59, "nop_59", "disable", {INT32}, F_V0_V2},

    // Displays a message. Special tokens are interpolated within the string.
    // These special tokens are:
    // <rXX> => value of rXX as %d (signed integer)
    // <fXX> => value of rXX as %f (floating-point) (v3 and later)
    // <color X> => changes text color like $CX would (supported on 11/2000 and
    //   later); X must be numeric, so <color G> does not work
    // <cr> => newline
    // <hero name> or <name hero> => character's name
    // <hero job> or <name job> => character's class
    // <time> => always "01:12" (seems like an oversight that was never fixed)
    // <award item> => name of the chosen challenge mode reward (v2 and later)
    // <challenge title> => character's challenge rank text (v2 and later)
    // <pl_name> => name of character selected with get_pl_name (v2 and later)
    // <pl_job> => class of character selected with get_pl_job (v2 and later)
    // <last_word> => challenge mode grave message (v2 and later)
    // <team_name> => name of the game (set by 8A command) (v2 and later)
    // <last_chat> => last chat message (v2 and later)
    // <meseta_slot_prize> => the description of the last item sent by the
    //   server in a 6xE3 command (BB only)
    {0x5A, "window_msg", nullptr, {CSTRING}, F_V0_V4 | F_ARGS},

    // Adds a message to an existing message (or window_msg). Tokens are
    // interpolated as for window_msg.
    {0x5B, "add_msg", nullptr, {CSTRING}, F_V0_V4 | F_ARGS},

    // Closes the current message box
    {0x5C, "message_end", "mesend", {}, F_V0_V4},

    // Gets the current time, in seconds since 00:00:00 on 1 January 2000
    {0x5D, "gettime", nullptr, {REG}, F_V0_V4},

    // Closes a window_msg
    {0x5E, "window_msg_end", "winend", {}, F_V0_V4},

    // Creates an NPC as client ID 1. Sends 6x69 with command = 0.
    // valueA = initial state (see npc_crp below)
    // valueB = template index (see 6x69 in CommandFormats.hh)
    {0x60, "npc_crt", "npc_crt_V1", {INT32, INT32}, F_V0_V2 | F_ARGS},
    {0x60, "npc_crt", "npc_crt_V3", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Tells an NPC (by client ID) to stop following
    {0x61, "npc_stop", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Tells an NPC (by client ID) to follow the player
    {0x62, "npc_play", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Destroys an NPC (by client ID)
    {0x63, "npc_kill", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Disables or enables the ability to talk to NPCs
    {0x64, "npc_talk_off", "npc_nont", {}, F_V0_V4},
    {0x65, "npc_talk_on", "npc_talk", {}, F_V0_V4},

    // Creates an NPC as client ID 1. Sends 6x69 with command = 0.
    // regsA[0-2] = position (x, y, z as integers)
    // regsA[3] = angle
    // regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box,
    //   according to qedit.info)
    // regsA[5] = template index (see 6x69 in CommandFormats.hh)
    // valueB is required in pre-v3 but is ignored
    {0x66, "npc_crp", "npc_crp_V1", {{REG_SET_FIXED, 6}, INT32}, F_V0_V2},
    {0x66, "npc_crp", "npc_crp_V3", {{REG_SET_FIXED, 6}}, F_V3_V4},

    // Creates a pipe. valueA is client ID
    {0x68, "create_pipe", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Checks player HP, but not in a straightforward manner.
    // Specifically, sets regA to 1 if (current_hp / max_hp) < (1 / valueB).
    // Sets regA to 0 otherwise.
    {0x69, "p_hpstat", "p_hpstat_V1", {REG, CLIENT_ID}, F_V0_V2 | F_ARGS},
    {0x69, "p_hpstat", "p_hpstat_V3", {REG, CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Sets regA to 1 if player in slot (valueB) is dead, or 0 if alive.
    {0x6A, "p_dead", "p_dead_V1", {REG, CLIENT_ID}, F_V0_V2 | F_ARGS},
    {0x6A, "p_dead", "p_dead_V3", {REG, CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Disables/enables telepipes/Ryuker
    {0x6B, "p_disablewarp", nullptr, {}, F_V0_V4},
    {0x6C, "p_enablewarp", nullptr, {}, F_V0_V4},

    // Moves player. Second argument is ignored
    // regsA[0-2] = destination (x, y, z as integers)
    // regsA[3] = angle
    // regsA[4] = client ID
    // valueB is required in pre-v3 but is ignored
    {0x6D, "p_move", "p_move_v1", {{REG_SET_FIXED, 5}, INT32}, F_V0_V2},
    {0x6D, "p_move", "p_move_V3", {{REG_SET_FIXED, 5}}, F_V3_V4},

    // Causes the player with client ID valueA to look at an unspecified other
    // player. The specified player looks at the player with the lowest client
    // ID (except for the specified player).
    // TODO: TObjPlayer::state is involved in determining which player to look
    // at; figure out exactly what this does
    {0x6E, "p_look", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Disables/enables attacks for all players
    {0x70, "p_action_disable", nullptr, {}, F_V0_V4},
    {0x71, "p_action_enable", nullptr, {}, F_V0_V4},

    // Disables/enables movement for the given player
    {0x72, "disable_movement1", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},
    {0x73, "enable_movement1", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // These appear to do nothing at all. On v3, they set and clear a flag that
    // is never read. On DC NTE and v1, code exists to read this flag, but it
    // belongs to an object that appears to never be constructed anywhere.
    {0x74, "clear_unused_flag_74", "p_noncol", {}, F_V0_V4},
    {0x75, "set_unused_flag_75", "p_col", {}, F_V0_V4},

    // Sets a player's starting position.
    // valueA = client ID
    // regsB[0-2] = position (x, y, z as integers)
    // regsB[3] = angle
    {0x76, "set_player_start_position", "p_setpos", {CLIENT_ID, {REG_SET_FIXED, 4}}, F_V0_V4 | F_ARGS},

    // Returns players to the Hunter's Guild counter.
    {0x77, "p_return_guild", nullptr, {}, F_V0_V4},

    // Opens the Hunter's Guild counter menu. valueA should be the player's
    // client ID.
    {0x78, "p_talk_guild", nullptr, {CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Creates an NPC which only appears near a given location
    // regsA[0-2] = position (x, y, z as integers)
    // regsA[3] = visibility radius
    // regsA[4] = angle
    // regsA[5] = template index (see 6x69 in CommandFormats.hh)
    // regsA[6] = initial state (0 = alive, 1 = dead, 2 = invisible text box,
    //   according to qedit.info)
    // regsA[7] = client ID
    {0x79, "npc_talk_pl", "npc_talk_pl_V1", {{REG32_SET_FIXED, 8}}, F_V0_V2},
    {0x79, "npc_talk_pl", "npc_talk_pl_V3", {{REG_SET_FIXED, 8}}, F_V3_V4},

    // Destroys an NPC created with npc_talk_pl. This opcode cannot be executed
    // multiple times on the same frame; if it is, only the last one will take
    // effect.
    {0x7A, "npc_talk_kill", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Creates attacker NPC
    {0x7B, "npc_crtpk", "npc_crtpk_V1", {INT32, INT32}, F_V0_V2 | F_ARGS},
    {0x7B, "npc_crtpk", "npc_crtpk_V3", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Creates attacker NPC
    {0x7C, "npc_crppk", "npc_crppk_V1", {{REG32_SET_FIXED, 7}, INT32}, F_V0_V2},
    {0x7C, "npc_crppk", "npc_crppk_V3", {{REG_SET_FIXED, 7}}, F_V3_V4},

    // Creates an NPC with client ID 1. It is not recommended to use this
    // opcode if a player can be in that slot - use npc_crptalk_id instead.
    // regsA[0-2] = position (x, y, z as integers)
    // regsA[3] = angle
    // regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box,
    //   according to qedit.info)
    // regsA[5] = template index (see 6x69 in CommandFormats.hh)
    {0x7D, "npc_crptalk", "npc_crptalk_v1", {{REG32_SET_FIXED, 6}, INT32}, F_V0_V2},
    {0x7D, "npc_crptalk", "npc_crptalk_V3", {{REG_SET_FIXED, 6}}, F_V3_V4},

    // Causes client ID valueA to look at client ID valueB. Sends 6x3E.
    {0x7E, "p_look_at", nullptr, {CLIENT_ID, CLIENT_ID}, F_V0_V4 | F_ARGS},

    // Creates an NPC.
    // regsA[0-2] = position (x, y, z as integers)
    // regsA[3] = angle
    // regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box,
    //   according to qedit.info)
    // regsA[5] = client ID
    // regsA[6] = template index (see 6x69 in CommandFormats.hh)
    {0x7F, "npc_crp_id", "npc_crp_id_V1", {{REG32_SET_FIXED, 7}, INT32}, F_V0_V2},
    {0x7F, "npc_crp_id", "npc_crp_id_v3", {{REG_SET_FIXED, 7}}, F_V3_V4},

    // Causes the camera to shake
    {0x80, "cam_quake", nullptr, {}, F_V0_V4},

    // Moves the camera to where your character is looking
    {0x81, "cam_adj", nullptr, {}, F_V0_V4},

    // Zooms the camera in or out
    {0x82, "cam_zmin", nullptr, {}, F_V0_V4},
    {0x83, "cam_zmout", nullptr, {}, F_V0_V4},

    // Pans the camera
    // regsA[0-2] = destination (x, y, z as integers)
    // regsA[3] = pan time (in frames; 30 frames/sec)
    // regsA[4] = end time (in frames; 30 frames/sec)
    {0x84, "cam_pan", "cam_pan_V1", {{REG32_SET_FIXED, 5}, INT32}, F_V0_V2},
    {0x84, "cam_pan", "cam_pan_V3", {{REG_SET_FIXED, 5}}, F_V3_V4},

    // Temporarily sets the game's difficulty to Very Hard (even on v2). On v3
    // and later, does nothing.
    {0x85, "game_lev_super", nullptr, {}, F_V0_V2},
    {0x85, "nop_85", nullptr, {}, F_V3_V4},

    // Restores the previous difficulty level after game_lev_super. On v3 and
    // later, does nothing.
    {0x86, "game_lev_reset", nullptr, {}, F_V0_V2},
    {0x86, "nop_86", nullptr, {}, F_V3_V4},

    // Creates a telepipe. The telepipe disappears upon being used.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = owner client ID (player or NPC must exist in the game)
    {0x87, "pos_pipe", "pos_pipe_V1", {{REG32_SET_FIXED, 4}, INT32}, F_V0_V2},
    {0x87, "pos_pipe", "pos_pipe_V3", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Checks if all set events (enemies) have been destroyed in a given room.
    // regA = result (0 = not cleared, 1 = cleared)
    // regsB[0] = floor number
    // regsB[1] = room ID
    {0x88, "if_zone_clear", nullptr, {REG, {REG_SET_FIXED, 2}}, F_V0_V4},

    // Returns the number of enemies destroyed so far in this game (since the
    // quest began).
    {0x89, "chk_ene_num", nullptr, {REG}, F_V0_V4},

    // Constructs all objects or enemies that match the conditions:
    // regsA[0] = floor
    // regsA[1] = section
    // regsA[2] = group
    {0x8A, "unhide_obj", nullptr, {{REG_SET_FIXED, 3}}, F_V0_V4},
    {0x8B, "unhide_ene", nullptr, {{REG_SET_FIXED, 3}}, F_V0_V4},

    // Starts a new thread when the player is close enough to the given point.
    // The collision is created on the current floor; the thread is created
    // when the player enters the given radius.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = radius
    // regsA[4] = label index where thread should start
    {0x8C, "at_coords_call", nullptr, {{REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but the thread is not started automatically.
    // Instead, the player's primary action button becomes "talk" within the
    // radius, and the label is called when the player presses that button.
    {0x8D, "at_coords_talk", nullptr, {{REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but only triggers if an NPC enters the radius.
    {0x8E, "npc_coords_call", "walk_to_coord_call", {{REG_SET_FIXED, 5}}, F_V0_V4},

    // Like at_coords_call, but triggers when a player within the event radius
    // is also within the player radius of any other player.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = event radius (centered at x, y, z defined above)
    // regsA[4] = player radius (centered at player)
    // regsA[5] = label index where thread should start
    {0x8F, "party_coords_call", "col_npcinr", {{REG_SET_FIXED, 6}}, F_V0_V4},

    // Enables/disables a switch flag (valueA). Does NOT send 6x05, so other
    // players will not know about this change! Use sw_send instead to keep
    // switch flag state synced.
    {0x90, "switch_on", nullptr, {INT32}, F_V0_V4 | F_ARGS},
    {0x91, "switch_off", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Plays a BGM. Values for valueA:
    //    1: epi1.adx           2: epi2.adx           3: ED_PIANO.adx
    //    4: matter.adx         5: open.adx           6: dreams.adx
    //    7: mambo.adx          8: carnaval.adx       9: hearts.adx
    //   10: smile.adx         11: nomal.adx         12: chu_f.adx
    //   13: ENDING_LOOP.adx   14: DreamS_KIDS.adx   15: ESCAPE.adx
    //   16: LIVE.adx          17: MILES.adx
    {0x92, "playbgm_epi", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Enables access to a floor (valueA) via the Pioneer 2 Ragol warp.
    // Floors are 0-17 for Episode 1, 18-35 for Episode 2, 36-46 for Episode 4.
    {0x93, "set_mainwarp", nullptr, {INT32}, F_V0_V4 | F_ARGS},

    // Creates an object that the player can talk to. A reticle appears when
    // the player is nearby.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = target radius
    // regsA[4] = label number to call
    // regsA[5] = distance from ground to target
    // regB = returned object token (can be used with del_obj_param)
    {0x94, "set_obj_param", nullptr, {{REG_SET_FIXED, 6}, REG}, F_V0_V4},

    // Causes the labelB to be called on a new thread when the player warps to
    // floorA. If the given floor already has a registered handler, it is
    // replaced with the new one (each floor may have at most one handler).
    {0x95, "set_floor_handler", nullptr, {FLOOR, SCRIPT32}, F_V0_V2},
    {0x95, "set_floor_handler", nullptr, {FLOOR, SCRIPT16}, F_V3_V4 | F_ARGS},

    // Deletes the handler for floorA.
    {0x96, "clr_floor_handler", nullptr, {FLOOR}, F_V0_V4 | F_ARGS},

    // Creates a collision object that checks if the NPC with client ID 1 is
    // too far away from the player, when the player enters its check radius.
    // regsA[0-2] = check location (x, y, z as integers)
    // regsA[3] = check radius
    // regsA[4] = label index for triggered function
    // regsA[5] = player radius (if NPC is closer, label is not called)
    // regsA[6-8] = warp location (x, y, z as integers) for use with
    //   npc_chkwarp within the triggered function
    {0x97, "check_npc_straggle", "col_plinaw", {{REG_SET_FIXED, 9}}, F_V1_V4},

    // Hides or shows the HUD.
    {0x98, "hud_hide", nullptr, {}, F_V0_V4},
    {0x99, "hud_show", nullptr, {}, F_V0_V4},

    // Enables/disables the cinema effect (black bars above/below screen)
    {0x9A, "cine_enable", nullptr, {}, F_V0_V4},
    {0x9B, "cine_disable", nullptr, {}, F_V0_V4},

    // Unused opcode. It's not clear what this was supposed to do. The behavior
    // appears to be the same on all versions of PSO, from DC NTE through BB.
    // argA is ignored. The game constructs a message list object from strB;
    // the game will softlock unless this string contains exactly 2 messages
    // (separated by \n). After doing this, it destroys the message list and
    // does nothing else.
    {0xA0, "nop_A0_debug", "broken_list", {INT32, CSTRING}, F_V0_V4 | F_ARGS},

    // Sets a function to be called (on a new thread) when the quest is failed.
    // The quest is considered failed when you talk to the Hunter's Guild
    // counter and r253 has the value 1 (specifically 1; other nonzero values
    // do not trigger this function).
    {0xA1, "set_qt_failure", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA1, "set_qt_failure", nullptr, {SCRIPT16}, F_V3_V4},

    // Like set_qt_failure, but uses r255 instead. If r255 and r253 both have
    // the value 1 when the player talks to the Hunter's Guild counter, the
    // success label is called and the failure label is not called.
    {0xA2, "set_qt_success", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA2, "set_qt_success", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest failure handler (opposite of set_qt_failure).
    {0xA3, "clr_qt_failure", nullptr, {}, F_V0_V4},

    // Clears the quest success handler (opposite of set_qt_success).
    {0xA4, "clr_qt_success", nullptr, {}, F_V0_V4},

    // Sets a function to be called when the quest is cancelled via the
    // Hunter's Guild counter.
    {0xA5, "set_qt_cancel", nullptr, {SCRIPT32}, F_V0_V2},
    {0xA5, "set_qt_cancel", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest cancel handler (opposite of set_qt_cancel).
    {0xA6, "clr_qt_cancel", nullptr, {}, F_V0_V4},

    // Makes a player or NPC walk to a location.
    // regsA[0-2] = location (x, y, z as integers; y is ignored)
    // regsA[3] = client ID
    {0xA8, "pl_walk", "pl_walk_V1", {{REG32_SET_FIXED, 4}, INT32}, F_V0_V2},
    {0xA8, "pl_walk", "pl_walk_V3", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Gives valueB Meseta to the player with client ID valueA. Negative values
    // do not appear to be handled properly; if this opcode attempts to take
    // more meseta than the player has, the player ends up with 999999 Meseta.
    {0xB0, "pl_add_meseta", nullptr, {CLIENT_ID, INT32}, F_V0_V4 | F_ARGS},

    // Starts a new thread at labelA in the quest script.
    {0xB1, "thread_stg", nullptr, {SCRIPT16}, F_V0_V4},

    // Deletes an interactable object previously created by set_obj_param.
    // valueA is the object's token (returned by set_obj_param in regB).
    {0xB2, "del_obj_param", nullptr, {REG}, F_V0_V4},

    // Creates an item in the player's inventory. If the item is successfully
    // created, this opcode sends 6x2B on all versions except BB. On BB, this
    // opcode sends 6xCA, and the server sends 6xBE to create the item.
    // regsA[0-2] = item.data1[0-2]
    // regB = returned item ID, or FFFFFFFF if item can't be created
    {0xB3, "item_create", nullptr, {{REG_SET_FIXED, 3}, REG}, F_V0_V4},

    // Like item_create, but regsA specify all of item.data1 instead of only
    // the first 3 bytes.
    {0xB4, "item_create2", nullptr, {{REG_SET_FIXED, 12}, REG}, F_V0_V4},

    // Deletes an item from the player's inventory. Sends 6x29 if ths item is
    // found and deleted. If the item is stackable, only one of it is deleted;
    // the rest of the stack is not deleted.
    // regA = item ID
    // regsB[0-11] = item.data1[0-11] for deleted item
    // regsB must not wrap around (that is, the first register in regsB cannot
    // be in the range [r245, r255]).
    {0xB5, "item_delete", nullptr, {REG, {REG_SET_FIXED, 12}}, F_V0_V4},

    // Like item_delete, but searches by item.data1[0-2] instead of by item ID.
    // regsA[0-2] = item.data1[0-2] to search for
    // regsB[0-11] = item.data1[0-11] for deleted item
    {0xB6, "item_delete_by_type", "item_delete2", {{REG_SET_FIXED, 3}, {REG_SET_FIXED, 12}}, F_V0_V4},

    // Searches the player's inventory for an item and returns its item ID.
    // regsA[0-2] = item.data1[0-2] to search for
    // regB = found item ID, or FFFFFFFF if not found
    {0xB7, "find_inventory_item", "item_check", {{REG_SET_FIXED, 3}, REG}, F_V0_V4},

    // Triggers set event valueA on the current floor. Sends 6x67.
    {0xB8, "setevt", nullptr, {INT32}, F_V05_V4 | F_ARGS},

    // Returns the current difficulty level in regA. If game_lev_super has been
    // executed, returns 2.
    // This opcode only returns 0-2, even in Ultimate (which results in 2 as
    // well). All non-v1 quests should use get_difficulty_level_v2 instead.
    {0xB9, "get_difficulty_level_v1", "get_difflvl", {REG}, F_V05_V4},

    // Sets a label to be called (in a new thread) when the quest exits.
    // This happens during the unload procedure when leaving the game, so most
    // opcodes cannot be used in these handlers. Generally they should only be
    // used for setting quest flags, event flags, or quest counters.
    {0xBA, "set_qt_exit", nullptr, {SCRIPT32}, F_V05_V2},
    {0xBA, "set_qt_exit", nullptr, {SCRIPT16}, F_V3_V4},

    // Clears the quest exit handler (opposite of set_qt_exit).
    {0xBB, "clr_qt_exit", nullptr, {}, F_V05_V4},

    // This opcode does nothing, even on 11/2000.
    {0xBC, "nop_BC", "unknownBC", {CSTRING}, F_V05_V4},

    // Creates a timed particle effect.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = effect type
    // regsA[4] = duration (in frames; 30 frames/sec)
    {0xC0, "particle", "particle_V1", {{REG32_SET_FIXED, 5}, INT32}, F_V05_V2},
    {0xC0, "particle", "particle_V3", {{REG_SET_FIXED, 5}}, F_V3_V4},

    // Specifies what NPCs should say in various situations. This opcode sets
    // strings for all NPCs; to set strings for only specific NPCs, use
    // npc_text_id (on v3 and later).
    // valueA = situation number:
    //   00: NPC engaging in combat
    //   01: NPC in danger (HP <= 75% if near player, <= 25% if far away?)
    //   02: NPC casting any technique except those in cases 16 and 17 below
    //   03: NPC has 20% or less TP
    //   04: NPC died
    //   05: NPC has been dead for a while
    //   06: Unknown (possibly unused)
    //   07: NPC lost sight of player
    //   08: NPC locked on to an enemy
    //   09: Player received a status effect
    //   0A: Unknown (possibly unused)
    //   0B: NPC standing still for 3 minutes
    //   0C: Player in danger? (Like 01 but players reversed?)
    //   0D: NPC completed 3-hit combo
    //   0E: NPC hit for > 30% of max HP
    //   0F: NPC hit for > 20% of max HP (and <= 30%)
    //   10: NPC hit for > 1% of max HP (and <= 20%)
    //   11: NPC healed by another player
    //   12: Room cleared (set event cleared which did not trigger another set)
    //   13: NPC used a recovery item
    //   14: NPC cannot heal / recover
    //   15: Wave but not room cleared (set event triggered by another set)
    //   16: NPC casting Resta or Anti
    //   17: NPC casting Foie, Zonde, or Barta
    //   18: NPC regained sight of player (not valid on 11/2000)
    // strB = string for NPC to say (up to 52 characters)
    {0xC1, "npc_text", nullptr, {INT32, CSTRING}, F_V05_V4 | F_ARGS},

    // Warps an NPC to a predetermined location. See npc_check_straggle for
    // more details.
    {0xC2, "npc_chkwarp", nullptr, {}, F_V05_V4},

    // Disables PK mode (battle mode) for a specific player. Sends 6x1C.
    {0xC3, "pl_pkoff", nullptr, {}, F_V05_V4},

    // Specifies how objects and enemies should be populated for a floor. On
    // v2, the ability to reassign areas was added, which can be done with
    // map_designate_ex (F80D).
    // regsA[0] = floor number
    // regsA[1] = type (0: use layout, 1: use offline template, 2: use online
    //   template, 3: nothing)
    // regsA[2] = major variation (minor variation is set to zero)
    // regsA[3] = ignored
    {0xC4, "map_designate", nullptr, {{REG_SET_FIXED, 4}}, F_V05_V4},

    // Locks (masterkey_on) or unlocks (masterkey_off) all doors
    {0xC5, "masterkey_on", nullptr, {}, F_V05_V4},
    {0xC6, "masterkey_off", nullptr, {}, F_V05_V4},

    // Enables/disables the timer window
    {0xC7, "show_timer", "window_time", {}, F_V05_V4},
    {0xC8, "hide_timer", "winend_time", {}, F_V05_V4},

    // Sets the time displayed in the timer window. The value in regA should be
    // a number of seconds.
    {0xC9, "winset_time", nullptr, {REG}, F_V05_V4},

    // Returns the time from the system clock.
    // - On DC, reads from hardware registers
    // - On GC, reads from the TBRs
    // - On PCv2, XB, and BB, calls QueryPerformanceCounter
    {0xCA, "getmtime", nullptr, {REG}, F_V05_V4},

    // Creates an item in the quest board.
    // valueA = index of item (0-5)
    // labelB = label to call when selected
    // strC = name of item in the Quest Board window
    // Executing this opcode is not enough for the item to appear! The item
    // only appears if its corresponding display register is also set to 1.
    // The display registers are r74-r79 (for QB indexes 0-5, respectively).
    {0xCB, "set_quest_board_handler", nullptr, {INT32, SCRIPT32, CSTRING}, F_V05_V2},
    {0xCB, "set_quest_board_handler", nullptr, {INT32, SCRIPT16, CSTRING}, F_V3_V4 | F_ARGS},

    // Deletes an item by index from the quest board.
    {0xCC, "clear_quest_board_handler", nullptr, {INT32}, F_V05_V4 | F_ARGS},

    // Creates a particle effect on a given entity.
    // regsA[0] = effect type
    // regsA[1] = duration in frames
    // regsA[2] = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    // regsA[3] = y offset (as integer)
    // valueB is required in pre-v3 but is ignored
    {0xCD, "particle_id", "particle_id_V1", {{REG32_SET_FIXED, 4}, INT32}, F_V05_V2},
    {0xCD, "particle_id", "particle_id_V3", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Creates an NPC.
    // regsA[0-2] = position (x, y, z as integers)
    // regsA[3] = angle
    // regsA[4] = initial state (0 = alive, 1 = dead, 2 = invisible text box,
    //   according to qedit.info)
    // regsA[5] = template index (see 6x69 in CommandFormats.hh)
    // regsA[6] = client ID
    {0xCE, "npc_crptalk_id", "npc_crptalk_id_V1", {{REG32_SET_FIXED, 7}, INT32}, F_V05_V2},
    {0xCE, "npc_crptalk_id", "npc_crptalk_id_V3", {{REG_SET_FIXED, 7}}, F_V3_V4},

    // Deletes all strings registered with npc_text.
    {0xCF, "npc_text_clear_all", "npc_lang_clean", {}, F_V05_V4},

    // Enables PK mode (battle mode) for a specific player. Sends 6x1B.
    {0xD0, "pl_pkon", nullptr, {}, F_V1_V4},

    // Like find_inventory_item, but regsA specifies data1[0-2] as well as
    // data1[4]. Returns the item ID in regB, or FFFFFFFF if not found.
    {0xD1, "pl_chk_item2", nullptr, {{REG_SET_FIXED, 4}, REG}, F_V1_V4},

    // Enables/disables the main menu and shortcut menu.
    {0xD2, "enable_mainmenu", nullptr, {}, F_V1_V4},
    {0xD3, "disable_mainmenu", nullptr, {}, F_V1_V4},

    // Enables or disables battle music override. When enabled, the battle
    // segments of the BGM will play regardless of whether there are enemies
    // nearby. Changing this value only takes effect after any currently-queued
    // music segments are done playing. The override is cleared upon changing
    // areas.
    {0xD4, "start_battlebgm", nullptr, {}, F_V1_V4},
    {0xD5, "end_battlebgm", nullptr, {}, F_V1_V4},

    // Shows a message in the Quest Board message window
    {0xD6, "disp_msg_qb", nullptr, {CSTRING}, F_V1_V4 | F_ARGS},

    // Closes the Quest Board message window
    {0xD7, "close_msg_qb", nullptr, {}, F_V1_V4},

    // Writes the valueB (a single byte) to the event flag (valueA)
    {0xD8, "set_eventflag", "set_eventflag_v1", {INT32, INT32}, F_V1_V2 | F_ARGS},
    {0xD8, "set_eventflag", "set_eventflag_v3", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Sets regA to valueB, and sends 6x77 so other clients will also set their
    // local regA to valueB.
    {0xD9, "sync_register", "sync_leti", {REG32, INT32}, F_V1_V2 | F_ARGS},
    {0xD9, "sync_register", "sync_leti", {REG, INT32}, F_V3_V4 | F_ARGS},

    // TODO: Document these
    {0xDA, "set_returnhunter", nullptr, {}, F_V1_V4},
    {0xDB, "set_returncity", nullptr, {}, F_V1_V4},

    // TODO: Document this
    {0xDC, "load_pvr", nullptr, {}, F_V1_V4},
    // TODO: Document this
    // Does nothing on all non-DC versions.
    {0xDD, "load_midi", nullptr, {}, F_V1_V4},

    // Finds an item in the player's bank, and clears its entry in the bank.
    // regsA[0-5] = item.data1[0-5]
    // regB = 1 if item was found and cleared, 0 if not
    {0xDE, "item_detect_bank", "unknownDE", {{REG_SET_FIXED, 6}, REG}, F_V1_V4},

    // Sets NPC AI behaviors.
    // regsA[0] = unknown (TODO)
    // regsA[1] = base level. NPC's actual level depends on difficulty:
    //   Normal: min(base level + 1, 199)
    //   Hard: min(base level + 26, 199)
    //   Very Hard: min(base level + 51, 199)
    //   Ultimate: min(base level + 151, 199)
    // regsA[2] = technique flags; bit field:
    //   04 = has Foie and Gifoie (overrides 08 and 10)
    //   08 = has Barta and Gibarta (overrides 10)
    //   10 = has Zonde and Razonde
    //   40 = unknown (TODO)
    //   80 = unknown (TODO)
    // regsA[3] = enemy lock-on range
    // regsA[4] = unknown (TODO)
    // regsA[5] = max distance from player
    // regsA[6] = enemy unlock range
    // regsA[7] = block range
    // regsA[8] = attack range (not necessarily equal to weapon range)
    // regsA[9] = attack technique level (specified by technique flags)
    // regsA[10] = support technique level (Resta/Anti)
    // regsA[11] = attack probability (in range [0, 100])
    // regsA[12] = attack technique probability (in range [0, 100])
    // regsA[13] = unknown (TODO); appears to be a distance range
    // valueB = NPC template to modify (00-3F)
    {0xDF, "npc_param", "npc_param_V1", {{REG32_SET_FIXED, 14}, INT32}, F_V1_V2},
    {0xDF, "npc_param", "npc_param_V3", {{REG_SET_FIXED, 14}, INT32}, F_V3_V4 | F_ARGS},

    // TODO(DX): Document this. It enables a flag that affects some logic in
    // TBoss1Dragon::update. The flag is disabled when the Dragon's boss arena
    // unloads, but not when it loads, so it can be set when the player is in
    // a different area. It appears the flag is not cleared if the player never
    // enters the Dragon arena, so it seems the only advisable place to use
    // this would be immediately after the player enters the Dragon arena.
    {0xE0, "pad_dragon", nullptr, {}, F_V1_V4},

    // Disables access to a floor (valueA) via the Pioneer 2 Ragol warp. This
    // is the logical opposite of set_mainwarp.
    {0xE1, "clear_mainwarp", nullptr, {INT32}, F_V1_V4 | F_ARGS},

    // Sets camera parameters for the current frame.
    // regsA[0-2] = relative location of focus point from player
    // regsA[3-5] = relative location of camera from player
    {0xE2, "pcam_param", "pcam_param_V1", {{REG32_SET_FIXED, 6}}, F_V1_V2},
    {0xE2, "pcam_param", "pcam_param_V3", {{REG_SET_FIXED, 6}}, F_V3_V4},

    // Triggers set event (valueB) on floor (valueA). Sends 6x67.
    {0xE3, "start_setevt", "start_setevt_v1", {INT32, INT32}, F_V1_V2 | F_ARGS},
    {0xE3, "start_setevt", "start_setevt_v3", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Enables or disables warps
    {0xE4, "warp_on", nullptr, {}, F_V1_V4},
    {0xE5, "warp_off", nullptr, {}, F_V1_V4},

    // Returns the client ID of the local client
    {0xE6, "get_client_id", "get_slotnumber", {REG}, F_V1_V4},

    // Returns the client ID of the lobby/game leader
    {0xE7, "get_leader_id", "get_servernumber", {REG}, F_V1_V4},

    // Sets an event flag from a register. In v3 and later, this is not needed,
    // since set_eventflag can be called with F_ARGS, but it still exists.
    {0xE8, "set_eventflag2", nullptr, {INT32, REG}, F_V1_V4 | F_ARGS},

    // regA %= regB
    {0xE9, "mod2", "res", {REG, REG}, F_V1_V4},

    // regA %= valueB
    {0xEA, "modi2", "unknownEA", {REG, INT32}, F_V1_V4},

    // Changes the background music. create_bgmctrl must be run before doing
    // this. The values for valueA are the same as for playbgm_epi.
    {0xEB, "set_bgm", "enable_bgmctrl", {INT32}, F_V1_V4 | F_ARGS},

    // Changes the state of a switch flag and sends the update to all players
    // (unlike switch_on/switch_off).
    // regsA[0] = switch flag number
    // regsA[1] = floor number
    // regsA[2] = flags (see 6x05 definition in CommandFormats.hh)
    {0xEC, "sw_send", nullptr, {{REG_SET_FIXED, 3}}, F_V1_V4},

    // Creates a BGM controller object. Use this before set_bgm.
    {0xED, "create_bgmctrl", nullptr, {}, F_V1_V4},

    // Like pl_add_meseta, but can only give Meseta to the local player.
    {0xEE, "pl_add_meseta2", nullptr, {INT32}, F_V1_V4 | F_ARGS},

    // Like sync_register, but takes the value from another register rather
    // than using an immediate value. On v3 and later, this is identical to
    // sync_register.
    {0xEF, "sync_register2", "sync_let", {INT32, REG32}, F_V1_V2},
    {0xEF, "sync_register2", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},
    // Same as sync_register2, but sends the value via UDP if UDP is enabled.
    // This opcode was removed after GC NTE and is missing from v3 and v4.
    {0xF0, "sync_register2_udp", "send_regwork", {REG32, REG32}, F_V1_V2},

    // Sets the camera's location and angle.
    // regsA[0-2] = camera location (x, y, z as integers)
    // regsA[3-5] = camera focus location (x, y, z as integers)
    {0xF1, "leti_fixed_camera", "leti_fixed_camera_V1", {{REG32_SET_FIXED, 6}}, F_V2},
    {0xF1, "leti_fixed_camera", "leti_fixed_camera_V3", {{REG_SET_FIXED, 6}}, F_V3_V4},

    // Resets the camera to non-fixed (default behavior).
    {0xF2, "default_camera_pos1", nullptr, {}, F_V2_V4},

    // Same as 50, but uses fixed arguments - with the string "俺は節政だ！！",
    // which Google Translate translates as "I am frugal!!"
    {0xF800, "debug_F800", nullptr, {}, F_V2},

    // Creates a region that calls a label if a specified string is said (via
    // chat) by a player within the region.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = radius
    // regsA[4] = label index
    // strB = trigger string (up to 31 characters)
    // If the trigger string contains any newlines (\n), it is truncated at the
    // first newline.
    // Before matching, the game prepends and appends a single space to the
    // chat message (presumably so that callers can use strings like " word "
    // to avoid matching substrings of longer words) and transforms the chat
    // message to lowercase, so strB should not contain any uppercase letters.
    // The lowercasing logic (probably accidentally) also affects some symbols,
    // as follows: [ => {    \ => |    ] => }    ^ => ~    _ => <DEL>
    // Curiously, the function that checks for matches appears to be used
    // incorrectly. The function takes an array of char[0x20] and returns the
    // index of the string that matched, or -1 if none matched. This list of
    // match strings is expected to end with an empty string, but TOChatSensor
    // doesn't correctly terminate it this way. However, the following field is
    // the TQuestThread pointer, which is non-null only if the sensor has
    // already triggered, so it doesn't misbehave.
    {0xF801, "set_chat_callback", "set_chat_callback?", {{REG32_SET_FIXED, 5}, CSTRING}, F_V2_V4 | F_ARGS},

    // Returns the difficulty level. Unlike get_difficulty_level_v1, this
    // correctly returns 3 in Ultimate.
    {0xF808, "get_difficulty_level_v2", "get_difflvl2", {REG}, F_V2_V4},

    // Returns the number of players in the game.
    {0xF809, "get_number_of_players", "get_number_of_player1", {REG}, F_V2_V4},

    // Returns the location of the specified player.
    // regsA[0-2] = returned location (x, y, z as integers)
    // regB = client ID
    {0xF80A, "get_coord_of_player", nullptr, {{REG_SET_FIXED, 3}, REG}, F_V2_V4},

    // Enables or disables the area map and minimap.
    {0xF80B, "enable_map", nullptr, {}, F_V2_V4},
    {0xF80C, "disable_map", nullptr, {}, F_V2_V4},

    // Like map_designate, but allows changing the area assignment.
    // regsA[0] = floor number
    // regsA[1] = area number
    // regsA[2] = type (0: use layout, 1: use offline template, 2: use online
    //   template, 3: nothing)
    // regsA[3] = major variation
    // regsA[4] = minor variation
    {0xF80D, "map_designate_ex", nullptr, {{REG_SET_FIXED, 5}}, F_V2_V4},

    // Enables or disables weapon dropping upon death for a player.
    // Sends 6x81 (disable) or 6x82 (enable).
    {0xF80E, "disable_weapon_drop", "unknownF80E", {CLIENT_ID}, F_V2_V4 | F_ARGS},
    {0xF80F, "enable_weapon_drop", "unknownF80F", {CLIENT_ID}, F_V2_V4 | F_ARGS},

    // Sets the floor where the players will start. This generally should be
    // used in the start label (where map_designate, etc. are used).
    // valueA specifies which floor to start on, but indirectly:
    //   valueA = 0: Temple (floor 0x10)
    //   valueA = 1: Spaceship (floor 0x11)
    //   valueA > 1 and < 0x12: Episode 1 areas (Pioneer 2, Forest 1, etc.)
    //   valueA >= 0x12: No effect
    {0xF810, "ba_initial_floor", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Clears all battle rules.
    {0xF811, "clear_ba_rules", "set_ba_rules", {}, F_V2_V4},

    // Sets the tech disk mode in battle. valueA (does NOT match enum):
    // 0 => FORBID_ALL
    // 1 => ALLOW
    // 2 => LIMIT_LEVEL
    {0xF812, "ba_set_tech_disk_mode", "ba_set_tech", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the weapon and armor mode in battle. valueA (does NOT match enum):
    // 0 => FORBID_ALL
    // 1 => ALLOW
    // 2 => CLEAR_AND_ALLOW
    // 3 => FORBID_RARES
    {0xF813, "ba_set_weapon_and_armor_mode", "ba_set_equip", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the mag mode in battle. valueA (does NOT match enum):
    // 0 => FORBID_ALL
    // 1 => ALLOW
    {0xF814, "ba_set_forbid_mags", "ba_set_mag", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the tool mode in battle. valueA (does NOT match enum):
    // 0 => FORBID_ALL
    // 1 => ALLOW
    // 2 => CLEAR_AND_ALLOW
    {0xF815, "ba_set_tool_mode", "ba_set_item", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the trap mode in battle. valueA (matches enum):
    // 0 => DEFAULT
    // 1 => ALL_PLAYERS
    {0xF816, "ba_set_trap_mode", "ba_set_trapmenu", {INT32}, F_V2_V4 | F_ARGS},

    // This appears to be unused - the value is copied into the main battle
    // rules struct, but the field is never read from there. This may have been
    // an early implementation of F851 that affected all trap types, but this
    // field is no longer used.
    {0xF817, "ba_set_unused_F817", "unknownF817", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the respawn mode in battle. valueA (does NOT match enum):
    // 0 => FORBID
    // 1 => ALLOW
    // 2 => LIMIT_LIVES
    {0xF818, "ba_set_respawn", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) character replacement in battle mode
    {0xF819, "ba_set_replace_char", "ba_set_char", {INT32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) weapon dropping upon death in battle
    {0xF81A, "ba_dropwep", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) teams in battle
    {0xF81B, "ba_teams", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Shows the rules window and starts the battle. This should be used after
    // setting up all the rules with the various ba_* opcodes.
    {0xF81C, "ba_start", "ba_disp_msg", {CSTRING}, F_V2_V4 | F_ARGS},

    // Sets the number of levels to gain upon respawn in battle
    {0xF81D, "death_lvl_up", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Sets the Meseta mode in battle. valueA (matches enum):
    // 0 => ALLOW
    // 1 => FORBID_ALL
    // 2 => CLEAR_AND_ALLOW
    {0xF81E, "ba_set_meseta_drop_mode", "ba_set_meseta", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the challenge mode stage number
    {0xF820, "cmode_stage", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // regsA[3-8] specify first 6 bytes of an ItemData. This opcode consumes an
    // item ID, but does nothing else.
    {0xF821, "nop_F821", nullptr, {{REG_SET_FIXED, 9}}, F_V2_V4},

    // This opcode does nothing. It has two branches (one for online, one for
    // offline), but both branches do nothing.
    {0xF822, "nop_F822", nullptr, {REG}, F_V2_V4},

    // Sets the challenge template index. See Client::create_challenge_overlay
    // for details on how the template is used.
    {0xF823, "set_cmode_char_template", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Sets the game difficulty (0-3) in challenge mode. Does nothing in modes
    // other than challenge.
    {0xF824, "set_cmode_difficulty", "set_cmode_diff", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the factor by which all EXP is multiplied in challenge mode.
    // The multiplier value is regsA[0] + (regsA[1] / regsA[2]).
    {0xF825, "exp_multiplication", nullptr, {{REG_SET_FIXED, 3}}, F_V2_V4},

    // Checks if any player is still alive in challenge mode. Returns 1 if all
    // players are dead, or 0 if not.
    {0xF826, "cmode_check_all_players_dead", "if_player_alive_cm", {REG}, F_V2_V4},

    // Checks if all players are still alive in challenge mode. Returns 1 if any
    // player is dead, or 0 if not.
    {0xF827, "cmode_check_any_player_dead", "get_user_is_dead?", {REG}, F_V2_V4},

    // Sends the player with client ID regA to floor regB. Does nothing if regA
    // doesn't refer to the local player.
    {0xF828, "go_floor", nullptr, {REG, REG}, F_V2_V4},

    // Returns the number of enemies killed (in regB) by the player whose
    // client ID is regA.
    {0xF829, "get_num_kills", nullptr, {REG, REG}, F_V2_V4},

    // Resets the kill count for the player specified by regA.
    {0xF82A, "reset_kills", nullptr, {REG}, F_V2_V4},

    // Sets or clears a switch flag, and synchronizes the value to all players.
    // valueA = floor
    // valueB = switch flag index
    {0xF82B, "set_switch_flag_sync", "unlock_door2", {INT32, INT32}, F_V2_V4 | F_ARGS},
    {0xF82C, "clear_switch_flag_sync", "lock_door2", {INT32, INT32}, F_V2_V4 | F_ARGS},

    // Checks a switch flag on the current floor
    // regsA[0] = switch flag index
    // regsA[1] = result (0 or 1)
    {0xF82D, "read_switch_flag", "if_switch_not_pressed", {{REG_SET_FIXED, 2}}, F_V2_V4},

    // Checks a switch flag on any floor
    // regsA[0] = floor
    // regsA[1] = switch flag index
    // regsA[2] = result (0 or 1)
    {0xF82E, "read_switch_flag_on_floor", "if_switch_pressed", {{REG_SET_FIXED, 3}}, F_V2_V4},

    // Enables a player to control the Dragon. valueA specifies the client ID.
    {0xF830, "control_dragon", nullptr, {REG}, F_V2_V4},

    // Disables player control of the Dragon.
    {0xF831, "release_dragon", nullptr, {}, F_V2_V4},

    // Shrinks a player or returns them to normal size. regA specifies the
    // client ID.
    {0xF838, "shrink", nullptr, {REG}, F_V2_V4},
    {0xF839, "unshrink", nullptr, {REG}, F_V2_V4},

    // These set some camera parameters for the specified player. These
    // parameters appear to be unused, so these opcodes essentially do nothing.
    // regsA[0] = client ID
    // regsA[1-3] = a Vector3F (x, y, z as integers)
    {0xF83A, "set_shrink_cam1", nullptr, {{REG_SET_FIXED, 4}}, F_V2_V4},
    {0xF83B, "set_shrink_cam2", nullptr, {{REG_SET_FIXED, 4}}, F_V2_V4},

    // Shows the timer window in challenge mode. regA is the time value to
    // display, in seconds.
    {0xF83C, "disp_time_cmode", nullptr, {REG}, F_V2_V4},

    // Sets the total number of areas across all challenge quests for the
    // current episode
    {0xF83D, "set_area_total", "unknownF83D", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the number of the current challenge mode area
    {0xF83E, "set_current_area_number", "delete_area_title?", {INT32}, F_V2_V4 | F_ARGS},

    // Loads a custom visual config for creating NPCs. Generally the sequence
    // should go like this:
    //   prepare_npc_visual   label_containing_PlayerVisualConfig
    //   enable_npc_visual
    //   <opcode that creates an NPC>
    // After any NPC is created, the effects of these opcodes are undone; if
    // the script wants to create another NPC with a custom visual config, it
    // must run these opcodes again.
    {0xF840, "enable_npc_visual", "load_npc_data", {}, F_V2_V4},
    {0xF841, "prepare_npc_visual", "get_npc_data", {{LABEL16, Arg::DataType::PLAYER_VISUAL_CONFIG, "visual_config"}}, F_V2_V4},

    // These are used to set the scores for each type of action in battle mode.
    // The value used in each of these are regsA[0] + (regsA[1] / regsA[2]),
    // treated as a floating-point value. For example, one way to specify the
    // value 3.4 would be to set regsA to {3, 2, 5}.
    // These values are not reset between battle quests or upon joining/leaving
    // games, so battle quests should always explicitly set these!
    // The scores which can be set are:
    //   ba_player_give_damage_score: Sets the score earned per point of damage
    //     given to other players. (Default 0.05)
    //   ba_player_take_damage_score: Sets the score lost per point of damage
    //     taken from other players. (Default 0.02)
    //   ba_enemy_give_damage_score: Sets the score earned per point of damage
    //     given to non-player enemies. (Default 0.01)
    //   ba_enemy_take_damage_score: Sets the score lost per point of damage
    //     taken from non-player enemies. (Default 0)
    //   ba_player_kill_score: Sets the score earned by killing another player.
    //     (Default 10)
    //   ba_player_death_score: Sets the score lost by dying to another player.
    //     (Default 7)
    //   ba_enemy_kill_score: Sets the score earned by killing a non-player
    //     enemy. (Default 3)
    //   ba_enemy_death_score: Sets the score lost by dying to a non-player
    //     enemy. (Default 7)
    //   ba_meseta_score: Sets the score earned per Meseta in the player's
    //     inventory. (Default 0)
    {0xF848, "ba_player_give_damage_score", "give_damage_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF849, "ba_player_take_damage_score", "take_damage_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84A, "ba_enemy_give_damage_score", "enemy_give_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84B, "ba_enemy_take_damage_score", "enemy_take_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84C, "ba_player_kill_score", "kill_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84D, "ba_player_death_score", "death_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84E, "ba_enemy_kill_score", "enemy_kill_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF84F, "ba_enemy_death_score", "enemy_death_score", {{REG_SET_FIXED, 3}}, F_V2_V4},
    {0xF850, "ba_meseta_score", "meseta_score", {{REG_SET_FIXED, 3}}, F_V2_V4},

    // Sets the number of traps players can use in battle mode. regsA[1] is the
    // amount; regsA[0] is the trap type:
    //   0 = Damage trap (internal type 0)
    //   1 = Slow trap (internal type 2)
    //   2 = Confuse trap (internal type 3)
    //   3 = Freeze trap (internal type 1)
    {0xF851, "ba_set_trap_count", "ba_set_trap", {{REG_SET_FIXED, 2}}, F_V2_V4},

    // Enables (0) or disables (1) the targeting reticle in battle
    {0xF852, "ba_hide_target_reticle", "ba_set_target", {INT32}, F_V2_V4 | F_ARGS},

    // Enables or disables overrides of warp destination floors. When enabled,
    // area warps will always go to the next floor (current floor + 1); when
    // disabled, they will go to the floor specified in their constructor args.
    {0xF853, "override_warp_dest_floor", "reverse_warps", {}, F_V2_V4},
    {0xF854, "restore_warp_dest_floor", "unreverse_warps", {}, F_V2_V4},

    // Enables or disables overriding graphical features with those used in
    // Ultimate
    {0xF855, "set_ult_map", nullptr, {}, F_V2_V4},
    {0xF856, "unset_ult_map", nullptr, {}, F_V2_V4},

    // Sets the area title in challenge mode
    {0xF857, "set_area_title", nullptr, {CSTRING}, F_V2_V4 | F_ARGS},

    // Enables or disables the ability to see your own traps.
    {0xF858, "ba_show_self_traps", "BA_Show_Self_Traps", {}, F_V2_V4},
    {0xF859, "ba_hide_self_traps", "BA_Hide_Self_Traps", {}, F_V2_V4},

    // Creates an item in a player's inventory and equips it. Sends 6x2B
    // followed by 6x25. It appears Sega did not intend for this to be used on
    // BB, since the behavior wasn't changed - normally, item-related functions
    // should be done by the server on BB.
    // regsA[0] = client ID
    // regsA[1-3] = item.data1[0-2]
    {0xF85A, "equip_item", "equip_item_v2", {{REG32_SET_FIXED, 4}}, F_V2},
    {0xF85A, "equip_item", "equip_item_v3", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Unequips an item from a client. Sends 6x26 if an item is unequipped.
    // valueA = client ID
    // valueB = equip slot, but not the same as the EquipSlot enum:
    //   0 = weapon
    //   1 = armor
    //   2 = shield
    //   3 = mag
    //   4-7 = units 1-4
    {0xF85B, "unequip_item", "unequip_item_V2", {CLIENT_ID, INT32}, F_V2 | F_ARGS},
    {0xF85B, "unequip_item", "unequip_item_V3", {CLIENT_ID, INT32}, F_V3_V4 | F_ARGS},

    // Same as p_talk_guild except it always refers to the local player. valueA
    // is ignored.
    {0xF85C, "p_talk_guild_local", "QEXIT2", {INT32}, F_V2_V4},

    // Sets flags that forbid types of items from being used. To forbid
    // multiple types of items, use this opcode multiple times. valueA:
    //   0 = allow normal item usage (undoes all of the following)
    //   1 = disallow weapons
    //   2 = disallow armors
    //   3 = disallow shields
    //   4 = disallow units
    //   5 = disallow mags
    //   6 = disallow tools
    {0xF85D, "set_allow_item_flags", "allow_weapons", {INT32}, F_V2_V4 | F_ARGS},

    // Enables (1) or disables (0) sonar in battle
    {0xF85E, "ba_enable_sonar", "unknownF85E", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the number of sonar uses per character in battle
    {0xF85F, "ba_sonar_count", "ba_use_sonar", {INT32}, F_V2_V4 | F_ARGS},

    // Specifies when score announcements should occur during battle. The
    // values are measured in minutes remaining. There can be up to 8 score
    // announcements; any further set_score_announce calls are ignored.
    // clear_score_announce deletes all announcement times.
    {0xF860, "clear_score_announce", "unknownF860", {}, F_V2_V4},
    {0xF861, "set_score_announce", "unknownF861", {INT32}, F_V2_V4 | F_ARGS},

    // Creates an S-rank weapon in the player's inventory. This opcode is not
    // used in challenge mode, presumably since it doesn't offer a mechanism
    // for the player to choose their weapon's name. The award_item_give_to
    // opcode is used instead.
    // regA/valueA = client ID (must match local client ID)
    // regB (v2) = item.data1[1]
    // valueB (v3/v4) = register number of register containing item.data1[1]
    // strC = custom name
    {0xF862, "give_s_rank_weapon", nullptr, {REG32, REG32, CSTRING}, F_V2},
    {0xF862, "give_s_rank_weapon", nullptr, {INT32, INT32, CSTRING}, F_V3_V4 | F_ARGS},

    // Returns the currently-equipped mag's levels. If no mag is equipped,
    // regsA are unaffected! Make sure to initialize them before using this.
    // regsA[0] = returned DEF level
    // regsA[1] = returned POW level
    // regsA[2] = returned DEX level
    // regsA[3] = returned MIND level
    {0xF863, "get_mag_levels", nullptr, {{REG32_SET_FIXED, 4}}, F_V2},
    {0xF863, "get_mag_levels", nullptr, {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Sets the color and rank text if the player manages to complete the
    // current challenge stage.
    // valueA = color as XRGB8888
    // strB = rank text (up to 11 characters)
    {0xF864, "set_cmode_rank_result", "cmode_rank", {INT32, CSTRING}, F_V2_V4 | F_ARGS},

    // Shows the item name entry window and suspends the calling thread
    {0xF865, "award_item_name", "award_item_name?", {}, F_V2_V4},

    // Shows the item choice window and suspends the calling thread
    {0xF866, "award_item_select", "award_item_select?", {}, F_V2_V4},

    // Creates an item in the player's inventory, chosen via the previous
    // award_item_name and award_item_select opcodes, and updates the player's
    // challenge rank text and color according to the rank they achieved.
    // Sends 07DF on BB; on other versions, sends nothing.
    // regA = return value (1 if item successfully created; 0 otherwise)
    {0xF867, "award_item_give", "award_item_give_to?", {REG}, F_V2_V4},

    // Specifies where the time threshold is for a challenge rank.
    // regsA[0] = rank (0 = B, 1 = A, 2 = S)
    // regsA[1] = time in seconds (times faster than this are considered to be
    //   this rank or better)
    // regsA[2] = award flags mask (generally should be (1 << regsA[0]))
    // regB = result (0 = failed, 1 = success)
    {0xF868, "set_cmode_rank_threshold", "set_cmode_rank", {{REG_SET_FIXED, 3}, REG}, F_V2_V4},

    // Registers a timing result of (regA) seconds for the current challenge
    // mode stage. Returns a result code in regB:
    //   0 = player achieved rank B and has not completed this stage before
    //   1 = player achieved rank A on this stage for the first time
    //   2 = player achieved rank S on this stage for the first time
    //   3 = can't create results object (internal error)
    //   4 = player did not achieve a new rank this time
    //   5 = player's inventory is full and can't receive the prize
    //   6 = internal errors (e.g. save file is missing, stage number not set)
    {0xF869, "check_rank_time", nullptr, {REG, REG}, F_V2_V4},

    // Creates an item in the local player's bank, and saves the player's
    // challenge rank and title color. Sends 07DF on BB. This is used for the A
    // and B rank prizes.
    // regsA = item.data1[0-5]
    // regB = returned success code (1 = success, 0 = failed)
    {0xF86A, "item_create_cmode", nullptr, {{REG_SET_FIXED, 6}, REG}, F_V2_V4},

    // Sets the effective area for item drops in battle. valueA should be in
    // the range [1, 10].
    {0xF86B, "ba_set_box_drop_area", "ba_box_drops", {REG}, F_V2_V4},

    // Shows a confirmation window asking if the player is satsified with their
    // choice of S rank prize and weapon name.
    // regA = result code (0 = OK, 1 = reconsider)
    {0xF86C, "award_item_ok", "award_item_ok?", {REG}, F_V2_V4},

    // Enables or disables traps' ability to hurt the player who set them
    {0xF86D, "ba_set_trapself", nullptr, {}, F_V2_V4},
    {0xF86E, "ba_clear_trapself", "ba_ignoretrap", {}, F_V2_V4},

    // Sets the number of lives each player gets in battle when the LIMIT_LIVES
    // respawn mode is used.
    {0xF86F, "ba_set_lives", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Sets the maximum level for any technique in battle
    {0xF870, "ba_set_max_tech_level", "ba_set_tech_lvl", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the character's overlay level in battle
    {0xF871, "ba_set_char_level", "ba_set_lvl", {INT32}, F_V2_V4 | F_ARGS},

    // Sets the battle time limit. valueA is measured in minutes
    {0xF872, "ba_set_time_limit", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Sets regA to 1 if Dark Falz has been defeated, or 0 otherwise.
    {0xF873, "dark_falz_is_dead", "falz_is_dead", {REG}, F_V2_V4},

    // Sets an override for the challenge rank text. At the time the rank is
    // checked via check_rank_time, these overrides are applied in the order
    // they were created. For each one, if the existing rank matches the check
    // string, it is replaced with the override string and the color is
    // replaced with the override color.
    // valueA = override color (XRGB8888)
    // strB = check string and override string (separated by \t or \n)
    {0xF874, "set_cmode_rank_override", "unknownF874", {INT32, CSTRING}, F_V2_V4 | F_ARGS},

    // Enables or disables the transparency effect, similar to the Stealth
    // Suit. regA is the client ID.
    {0xF875, "enable_stealth_suit_effect", nullptr, {REG}, F_V2_V4},
    {0xF876, "disable_stealth_suit_effect", nullptr, {REG}, F_V2_V4},

    // Enables or disables the use of techniques for a player. regA is the
    // client ID.
    {0xF877, "enable_techs", nullptr, {REG}, F_V2_V4},
    {0xF878, "disable_techs", nullptr, {REG}, F_V2_V4},

    // Returns the gender of a character.
    // regA = client ID
    // regB = returned gender (0 = male, 1 = female, 2 = no player present or
    //   invalid class flags)
    {0xF879, "get_gender", nullptr, {REG, REG}, F_V2_V4},

    // Returns the race and class of a character.
    // regA = client ID
    // regsB[0] = returned race (0 = human, 1 = newman, 2 = android, 3 = no
    //   player present or invalid class flags)
    // regsB[1] = returned class (0 = hunter, 1 = ranger, 2 = force, 3 = no
    //   player present or invalid class flags)
    {0xF87A, "get_chara_class", nullptr, {REG, {REG_SET_FIXED, 2}}, F_V2_V4},

    // Removes Meseta from a player. Sends 6xC9 on BB.
    // regsA[0] = client ID
    // regsA[1] = amount to subtract (positive integer)
    // regsB[0] = result code if player is present (1 = taken, 0 = player
    //   didn't have enough)
    // regsB[1] = result code if player not present (0 is written here if there
    //   is no player with the specified client ID)
    // Note that only one of regsB[0] and regsB[1] is written; the other is
    // unchanged. Make sure to initialize these registers properly.
    {0xF87B, "take_slot_meseta", nullptr, {{REG_SET_FIXED, 2}, REG}, F_V2_V4},

    // Returns the Guild Card file creation time in seconds since 00:00:00 on
    // 1 January 2000.
    {0xF87C, "get_guild_card_file_creation_time", "get_encryption_key", {REG}, F_V2_V4},

    // Kills the player whose client ID is regA.
    {0xF87D, "kill_player", nullptr, {REG}, F_V2_V4},

    // Returns (in regA) the player's serial number. On BB, returns 0.
    {0xF87E, "get_serial_number", nullptr, {REG}, F_V2_V3},
    {0xF87E, "return_0_F87E", nullptr, {REG}, F_V4},

    // Reads an event flag from the system file.
    // regA = event flag index (0x00-0xFF)
    // regB = returned event flag value (1 byte)
    {0xF87F, "get_eventflag", "read_guildcard_flag", {REG, REG}, F_V2_V4},

    // Normally, trap damage is computed with the following formula:
    //   (700.0 * area_factor[area] * 2.0 * (0.01 * level + 0.1))
    // This opcode overrides that computation. The value is specified with the
    // integer and fractional parts split up: the actual value used by the game
    // will be regsA[0] + (regsA[1] / regsA[2]).
    {0xF880, "set_trap_damage", "ba_set_dmgtrap", {{REG_SET_FIXED, 3}}, F_V2_V4},

    // Loads the name of the player whose client ID is regA into a static
    // buffer, which can later be referred to with "<pl_name>" in message
    // strings.
    {0xF881, "get_pl_name", "get_pl_name?", {REG}, F_V2_V4},

    // Loads the job (Hunter, Ranger, or Force) of the player whose client ID
    // is regA into a static buffer, which can later be referred to with
    // "<pl_job>" in message strings.
    {0xF882, "get_pl_job", nullptr, {REG}, F_V2_V4},

    // Counts the number of players near the specified player.
    // regsA[0] = client ID
    // regsA[1] = radius (as integer)
    // regB = count
    {0xF883, "get_player_proximity", "players_in_range", {{REG_SET_FIXED, 2}, REG}, F_V2_V4},

    // Writes 2 bytes to the event flags in the system file.
    // valueA = flag index (must be 254 or less)
    // regB/valueB = value
    {0xF884, "set_eventflag16", "write_guild_flagw", {INT32, REG}, F_V2},
    {0xF884, "set_eventflag16", "write_guild_flagw", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Writes 4 bytes to the event flags in the system file.
    // valueA = flag index (must be 252 or less)
    // regB/valueB = value
    {0xF885, "set_eventflag32", "write_guild_flagl", {INT32, REG}, F_V2},
    {0xF885, "set_eventflag32", "write_guild_flagl", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Returns (in regB) the battle result place (1, 2, 3, or 4) of the player
    // specified by regA.
    {0xF886, "ba_get_place", nullptr, {REG, REG}, F_V2_V4},

    // Returns (in regB) the battle score of the player specified by regA.
    {0xF887, "ba_get_score", nullptr, {REG, REG}, F_V2_V4},

    // TODO: Document these
    {0xF888, "enable_win_pfx", "ba_close_msg", {}, F_V2_V4},
    {0xF889, "disable_win_pfx", nullptr, {}, F_V2_V4},

    // Returns (in regB) the state of the player specified by regA.
    // State values:
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
    {0xF88A, "get_player_state", "get_player_status", {REG, REG}, F_V2_V4},

    // Sends a Simple Mail message to the player.
    // regA (v2) = sender's Guild Card number
    // valueA (v3+) = number of register that holds sender's Guild Card number
    // strB = sender name and message (separated by \n)
    {0xF88B, "send_mail", nullptr, {REG, CSTRING}, F_V2_V4 | F_ARGS},

    // Returns the game's major version (2 on DCv2/PC, 3 on GC, 4 on XB and BB)
    {0xF88C, "get_game_version", nullptr, {REG}, F_V2_V4},

    // Sets the local player's stage completion time in challenge mode.
    // regA = time in seconds
    // regB = value to be used in computation of token_v4 (BB only; see 6x95 in
    //   CommandFormats.hh for details)
    {0xF88D, "chl_set_timerecord", "chl_set_timerecord?", {REG}, F_V2 | F_V3},
    {0xF88D, "chl_set_timerecord", "chl_set_timerecord?", {REG, REG}, F_V4},

    // Gets the current player's completion time for the current challenge
    // stage in seconds. If the player's time is invalid or faster than the
    // time set by chl_set_min_time_online (or 5 minutes, if offline), returns
    // -2. If used in non-challenge mode, returns -1.
    {0xF88E, "chl_get_timerecord", "chl_get_timerecord?", {REG}, F_V2_V4},

    // Sets the probabilities of getting recovery items from challenge mode
    // graves. There are 10 floating-point values, specified as fractions in an
    // array of 20 registers (pairs of numerator and denominator). The number
    // of items generated is capped by the number of players present; for
    // example, if the game chooses to generate 4 items but only 2 players are
    // present, 2 items are generated.
    // Counts array (these 4 values should sum to 1.0 or less):
    //   regsA[0] / regsA[1]: Chance of getting 1 recovery item
    //   regsA[2] / regsA[3]: Chance of getting 2 recovery items
    //   regsA[4] / regsA[5]: Chance of getting 3 recovery items
    //   regsA[6] / regsA[7]: Chance of getting 4 recovery items
    // Types array (these 6 value should sum to 1.0 or less):
    //   regsA[8] / regsA[9]: Chance of getting Monomate x1
    //   regsA[10] / regsA[11]: Chance of getting Dimate x1
    //   regsA[12] / regsA[13]: Chance of getting Trimate x1
    //   regsA[14] / regsA[15]: Chance of getting Monofluid x1
    //   regsA[16] / regsA[17]: Chance of getting Difluid x1
    //   regsA[18] / regsA[19]: Chance of getting Trifluid x1
    {0xF88F, "set_cmode_grave_rates", nullptr, {{REG_SET_FIXED, 20}}, F_V2_V4},

    // Clears all levels from the main warp.
    {0xF890, "clear_mainwarp_all", "clear_area_list", {}, F_V2_V4},

    // Specifies which enemy should be affected by subsequent get_*_data
    // opcodes (the following 4 definitions). valueA is the battle parameter
    // index for the desired enemy.
    {0xF891, "load_enemy_data", nullptr, {INT32}, F_V2_V4 | F_ARGS},

    // Replaces enemy stats with the given structures (PlayerStats, AttackData,
    // ResistData, or MovementData) for the enemy previously specified with
    // load_enemy_data.
    {0xF892, "get_physical_data", nullptr, {{LABEL16, Arg::DataType::PLAYER_STATS, "stats"}}, F_V2_V4},
    {0xF893, "get_attack_data", nullptr, {{LABEL16, Arg::DataType::ATTACK_DATA, "attack_data"}}, F_V2_V4},
    {0xF894, "get_resist_data", nullptr, {{LABEL16, Arg::DataType::RESIST_DATA, "resist_data"}}, F_V2_V4},
    {0xF895, "get_movement_data", nullptr, {{LABEL16, Arg::DataType::MOVEMENT_DATA, "movement_data"}}, F_V2_V4},

    // Reads 2 bytes or 4 bytes from the event flags in the system file.
    // regA = event flag index
    // regB = returned value
    {0xF896, "get_eventflag16", "read_guildflag_16b", {REG, REG}, F_V2_V4},
    {0xF897, "get_eventflag32", "read_guildflag_32b", {REG, REG}, F_V2_V4},

    // regA <<= regB
    {0xF898, "shift_left", nullptr, {REG, REG}, F_V2_V4},

    // regA >>= regB
    {0xF899, "shift_right", nullptr, {REG, REG}, F_V2_V4},

    // Generates a random number by calling rand(). Note that the returned
    // value is not uniform! The algorithm generates a uniform random number,
    // scales it to the range [0, max], then clamps it to [min, max]. So, if
    // the minimum value is not 0, the minimum value is more likely than all
    // other possible results. To get an unbiased result with a minimum not
    // equal to zero, use this with a minimum of zero, then use add or addi.
    // regsA[0] = minimum value
    // regsA[1] = maximum value
    // regB = generated random value
    {0xF89A, "get_random", nullptr, {{REG_SET_FIXED, 2}, REG}, F_V2_V4},

    // Clears all game state, including all floor items, set states (enemy and
    // object), enemy and object states, wave event flags, and switch flags.
    // Also destroys all running quest threads.
    {0xF89B, "reset_map", nullptr, {}, F_V2_V4},

    // Returns the leader's choice when a challenge is failed in regA. Values:
    //   0 = not chosen yet
    //   1 = no (releases players to interact on Pioneer 2)
    //   2 = yes (restarts the stage)
    {0xF89C, "get_chl_retry_choice", "retry_menu", {REG}, F_V2_V4},

    // Creates the retry menu when a challenge stage is failed
    {0xF89D, "chl_create_retry_menu", "chl_enable_retry", {}, F_V2_V4},

    // Enables (0) or disables (1) the use of scape dolls in battle
    {0xF89E, "ba_forbid_scape_dolls", "ba_forbid_scape_doll", {INT32}, F_V2_V4 | F_ARGS},

    // Restores a player's HP and TP, clears status effects, and revives the
    // player if dead.
    // regA = client ID
    {0xF89F, "player_recovery", "unknownF89F", {REG}, F_V2_V4},

    // These opcodes set, clear, and check (respectively) a flag that appears
    // to do nothing at all.
    {0xF8A0, "disable_bosswarp_option", "unknownF8A0", {}, F_V2_V4},
    {0xF8A1, "enable_bosswarp_option", "unknownF8A1", {}, F_V2_V4},
    {0xF8A2, "is_bosswarp_opt_disabled", "get_bosswarp_option", {REG}, F_V2_V4},

    // Loads the player's serial number into the "flag buffer", which is a
    // 4-byte buffer that can be written to event flags. (It's not obvious why
    // this can't just be done with get_serial_number and set_eventflag32...)
    // This opcode loads 0 to the flag buffer on BB.
    {0xF8A3, "load_serial_number_to_flag_buf", "init_online_key?", {}, F_V2_V4},

    // Writes the flag buffer to event flags. regA specifies which event flag
    // (the first of 4 consecutive flags).
    {0xF8A4, "write_flag_buf_to_event_flags", "encrypt_gc_entry_auto", {REG}, F_V2_V4},

    // Like set_chat_callback, but without a filter string. The meanings of
    // regsA are the same as for set_chat_callback.
    {0xF8A5, "set_chat_callback_no_filter", "chat_detect", {{REG_SET_FIXED, 5}}, F_V2_V4},

    // This opcode creates an object that triggers symbol chats when the
    // players are nearby and certain switch flags are NOT set. If a player is
    // within the radius, the object checks the switch flags in reverse order,
    // and triggers the symbol chat for the latest one that is NOT set. So, for
    // example:
    // - If all 3 switch flags are not set, the symbol chat in regsA[7] is used
    // - If the switch flag in regsA[5] is set, the symbol chat in regsA[9] is
    //   used regardless of whether the switch flags in regsA[4] is set
    // - If the switch flags in regsA[6] is set, no symbol chat appears at all
    //   regardless of the value of the other two switch flags
    // Arguments:
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = radius
    // regsA[4-6] = specs 0-2; for each spec, the high 16 bits are a switch
    //   flag number (0-255), and the low 16 bits are an entry index from
    //   symbolchatcolli.prs; the entry index is ignored if the corresponding
    //   data label (in the next 3 arguments) is not null. Quest scripts don't
    //   have a good way to pass null for regsA[7-9], so the logic that looks
    //   up entries in symbolchatcolli.prs can be ignored in quests.
    // regsA[7-9] = data labels for symbol chats 0-2; each points to a
    //   SymbolChat structure (see SymbolChatT in PlayerSubordinates.hh). Note
    //   that this structure is not byteswapped properly, so GameCube quests
    //   that use this opcode should use the big-endian version of the struct.
    //   (Practically, this means the first 32-bit field and the following 4
    //   16-bit fields must be byteswapped.)
    {0xF8A6, "set_symbol_chat_collision", "symbol_chat_create", {{REG_SET_FIXED, 10}}, F_V2_V4},

    // Sets the size that a player shrinks to when using the shrink opcode.
    // regA specified the client ID. The actual shrink size used is
    // regsB[0] + (regsB[1] / regsB[2]). If regsB[2] is 0, the fractional part
    // is considered to be zero and not used.
    {0xF8A7, "set_shrink_size", nullptr, {REG, {REG_SET_FIXED, 3}}, F_V2_V4},

    // Sets the amount by which techniques level up upon respawn in battle.
    {0xF8A8, "ba_death_tech_level_up", "death_tech_lvl_up2", {INT32}, F_V2_V4 | F_ARGS},

    // Returns 1 if Vol Opt has been defeated in the current game/quest.
    {0xF8A9, "vol_opt_is_dead", "volopt_is_dead", {REG}, F_V2_V4},

    // Returns 1 if the local player has a challenge mode grave message.
    {0xF8AA, "is_there_grave_message", nullptr, {REG}, F_V2_V4},

    // Returns the local player's battle mode records. The values returned are
    // the first 7 fields of the PlayerRecordsBattle structure (see
    // PlayerSubordinates.hh). These are:
    // regsA[0-3] = number of times placed 1st, 2nd, 3rd, 4th respectively
    // regsA[4] = number of disconnects
    // regsA[5-6] = unknown (TODO)
    {0xF8AB, "get_ba_record", nullptr, {{REG_SET_FIXED, 7}}, F_V2_V4},

    // Returns the current player's challenge mode rank. Reads from the state
    // corresponding to the current game mode (that is, reads from online Ep1
    // records in online Ep1 challenge mode, reads from offline Ep1 records in
    // offline challenge mode, reads from Ep2 records in Ep2). Return values:
    //   0 = challenge mode not completed
    //   1 = B rank
    //   2 = A rank
    //   3 = S rank
    {0xF8AC, "get_cmode_prize_rank", nullptr, {REG}, F_V2_V4},

    // Returns the number of players (in regA). Unlike get_number_of_players,
    // this counts the number of objects that have entity IDs assigned in the
    // players' ID space, whereas get_number_of_players counds the number of
    // TObjPlayer objects. For all practical purposes, these should result in
    // the same number.
    {0xF8AD, "get_number_of_players2", "get_number_of_player2", {REG}, F_V2_V4},

    // Returns 1 (in regA) if the current game has a nonempty name. The game
    // name is set by command 8A from the server.
    {0xF8AE, "party_has_name", nullptr, {REG}, F_V2_V4},

    // Returns 1 (in regA) if there is a chat message available (that is, if
    // anyone has sent a chat message in the current game).
    {0xF8AF, "someone_has_spoken", nullptr, {REG}, F_V2_V4},

    // Reads a 1-byte, 2-byte, or 4-byte value from the address (regB/valueB)
    // and places it in regA
    {0xF8B0, "read1", nullptr, {REG, REG}, F_V2},
    {0xF8B0, "read1", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},
    {0xF8B1, "read2", nullptr, {REG, REG}, F_V2},
    {0xF8B1, "read2", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},
    {0xF8B2, "read4", nullptr, {REG, REG}, F_V2},
    {0xF8B2, "read4", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},

    // Writes a 1-byte, 2-byte, or 4-byte value from regB/valueB to the address
    // (regA/valueA)
    {0xF8B3, "write1", nullptr, {REG, REG}, F_V2},
    {0xF8B3, "write1", nullptr, {INT32, INT32}, F_V3_V4 | F_ARGS},
    {0xF8B4, "write2", nullptr, {REG, REG}, F_V2},
    {0xF8B4, "write2", nullptr, {INT32, INT32}, F_V3_V4 | F_ARGS},
    {0xF8B5, "write4", nullptr, {REG, REG}, F_V2},
    {0xF8B5, "write4", nullptr, {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Returns a bitmask of 5 different types of detectable hacking. This
    // opcode only works on DCv2 - it crashes on all other versions, since it
    // tries to access memory at 8C007220, which is only a valid address on DC.
    // The bits are:
    //   0x01 = any byte in [8C007220, 8C0072E0) is nonzero (crashes on non-DC)
    //   0x02 = has cheat device save files on VMU (always zero on non-DC);
    //     looks for names: CDX_CODES_00, CDX_SETTINGS, XT-CHEATS, FCDCHEATS
    //   0x04 = any of the first 3 VMU-like devices (with function_code & 0x0E
    //     not equal to zero) on any of the 4 Maple buses reports power usage
    //     (standby or max) above 4 amps; always zero on non-DC systems
    //   0x08 = hacked item flag has been set (see implementation of
    //     are_rare_drops_allowed in ItemCreator.cc)
    //   0x10 = any bits in validation_flags in the character file are set (see
    //     PSOGCCharacterFile::Character in SaveFileFormats.hh)
    {0xF8B6, "check_for_hacking", "is_mag_hacked", {REG}, F_V2_V4},

    // Challenge mode cannot be completed unless this many seconds have passed
    // since the stage began. If not set or if offline, 5 minutes is used as
    // the threshold instead.
    {0xF8B7, "chl_set_min_time_online", "unknownF8B7", {REG}, F_V2_V4},

    // Disables the challenge mode retry menu
    {0xF8B8, "disable_retry_menu", "unknownF8B8", {}, F_V2_V4},

    // Shows the list of dead players in challenge mode
    {0xF8B9, "chl_show_dead_player_list", "chl_death_recap", {}, F_V2_V4},

    // Loads the Guild Card file creation time to the flag buffer. (See F8A3
    // and F8A4 for more details.)
    {0xF8BA, "load_guild_card_file_creation_time_to_flag_buf", "encrypt_gc_entry_auto2", {}, F_V2_V4},

    // Behaves exactly the same as write_flag_buf_to_event_flags (F8A4). This
    // is the last opcode implemented before v3.
    {0xF8BB, "write_flag_buf_to_event_flags2", "unknownF8BB", {REG}, F_V2_V4},

    // Sets the current episode. Must be used in the start label. valueA should
    // be 0 for Episode 1 (which is the default), 1 for Episode 2, or 2 for
    // Episode 4 (BB only).
    {0xF8BC, "set_episode", nullptr, {INT32}, F_V3_V4 | F_SET_EPISODE},

    // Requests a file from the server by sending a D7 command. valueA
    // specifies header.flag, strB is the file name (up to 16 characters).
    // This opcode works on Xbox, but the GBA opcodes do not, so it's
    // ultimately not useful there. This opcode does nothing on BB.
    {0xF8C0, "file_dl_req", nullptr, {INT32, CSTRING}, F_V3 | F_ARGS},
    {0xF8C0, "nop_F8C0", nullptr, {INT32, CSTRING}, F_V4 | F_ARGS},

    // Returns the status of the download requested with file_dl_req. Return
    // values (in regA):
    //   0 = failed (server sent a D7 command)
    //   1 = pending
    //   2 = complete
    {0xF8C1, "get_dl_status", nullptr, {REG}, F_V3},
    {0xF8C1, "nop_F8C1", nullptr, {REG}, F_V4},

    // Prepares to load a GBA ROM from a previous file_dl_req opcode. Does
    // nothing on Xbox and BB.
    {0xF8C2, "prepare_gba_rom_from_download", "gba_unknown4?", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF8C2, "nop_F8C2", nullptr, {}, F_XB_V3 | F_V4},

    // Starts loading a GBA ROM to a connected GBA, or checks the status of a
    // previous load request. One of prepare_gba_rom_from_download or
    // prepare_gba_rom_from_disk must be called before calling this, then this
    // should be called repeatedly until it succeeds or fails. Return values:
    //   0 = not started
    //   1 = failed
    //   2 = timed out
    //   3 = in progress
    //   4 = complete
    // This opcode always returns 0 on Xbox, and does nothing (doesn't even
    // affect regA) on BB.
    {0xF8C3, "start_or_update_gba_joyboot", "get_gba_state?", {REG}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF8C3, "return_0_F8C3", nullptr, {REG}, F_XB_V3},
    {0xF8C3, "nop_F8C3", nullptr, {REG}, F_V4},

    // Shows the challenge mode result window in split-screen mode. Does
    // nothing on BB.
    // regA = completion time in seconds, as returned by chl_get_timerecord
    {0xF8C4, "congrats_msg_multi_cm", "unknownF8C4", {REG}, F_V3},
    {0xF8C4, "nop_F8C4", nullptr, {REG}, F_V4},

    // Checks if the stage is done in offline challenge mode. Returns 1 if the
    // stage is still in progress, or 0 if it's completed or failed.
    {0xF8C5, "stage_in_progress_multi_cm", "stage_end_multi_cm", {REG}, F_V3},
    {0xF8C5, "nop_F8C5", nullptr, {REG}, F_V4},

    // Causes a fade to black, then exits the game. This is the same result as
    // receiving a 6x73 command.
    {0xF8C6, "qexit", "QEXIT", {}, F_V3_V4},

    // Causes a player to perform an animation.
    // regA = client ID
    // regB = animation number (TODO: document these)
    {0xF8C7, "use_animation", nullptr, {REG, REG}, F_V3_V4},

    // Stops an animation started with use_animation.
    // regA = client ID
    {0xF8C8, "stop_animation", nullptr, {REG}, F_V3_V4},

    // Causes a player to run to a location, as 6x42 does. Sends 6x42.
    // regsA[0-2] = location (x, y, z as integers; y is ignored)
    // regsA[3] = client ID
    {0xF8C9, "run_to_coord", nullptr, {{REG_SET_FIXED, 4}, REG}, F_V3_V4},

    // Makes a player invincible, or removes their invincibility.
    // regA = client ID
    // regB = enable invicibility (1 = enable, 0 = disable)
    {0xF8CA, "set_slot_invincible", nullptr, {REG, REG}, F_V3_V4},

    // Removes a player's invicibility. clear_slot_invincible rXX is equivalent
    // to set_slot_invincible rXX, 0.
    // regA = client ID
    {0xF8CB, "clear_slot_invincible", "set_slot_targetable?", {REG}, F_V3_V4},

    // These opcodes inflict various status conditions on a player. In the case
    // of Shifta/Deband/Jellen/Zalure, the effective technicuqe level is 21.
    // regA = client ID
    {0xF8CC, "set_slot_poison", nullptr, {REG}, F_V3_V4},
    {0xF8CD, "set_slot_paralyze", nullptr, {REG}, F_V3_V4},
    {0xF8CE, "set_slot_shock", nullptr, {REG}, F_V3_V4},
    {0xF8CF, "set_slot_freeze", nullptr, {REG}, F_V3_V4},
    {0xF8D0, "set_slot_slow", nullptr, {REG}, F_V3_V4},
    {0xF8D1, "set_slot_confuse", nullptr, {REG}, F_V3_V4},
    {0xF8D2, "set_slot_shifta", nullptr, {REG}, F_V3_V4},
    {0xF8D3, "set_slot_deband", nullptr, {REG}, F_V3_V4},
    {0xF8D4, "set_slot_jellen", nullptr, {REG}, F_V3_V4},
    {0xF8D5, "set_slot_zalure", nullptr, {REG}, F_V3_V4},

    // Same as leti_fixed_camera, but takes floating-point arguments.
    // regsA[0-2] = camera location (x, y, z as floats)
    // regsA[3-5] = camera focus location (x, y, z as floats)
    {0xF8D6, "fleti_fixed_camera", nullptr, {{REG_SET_FIXED, 6}}, F_V3_V4 | F_ARGS},

    // Sets the camera to follow the player at a fixed angle.
    // valueA = client ID
    // regsB[0-2] = camera angle (x, y, z as floats)
    {0xF8D7, "fleti_locked_camera", nullptr, {INT32, {REG_SET_FIXED, 3}}, F_V3_V4 | F_ARGS},

    // This opcode appears to be exactly the same as default_camera_pos.
    // TODO: Is there any difference?
    {0xF8D8, "default_camera_pos2", nullptr, {}, F_V3_V4},

    // Enables a motion blur visual effect.
    {0xF8D9, "set_motion_blur", nullptr, {}, F_V3_V4},

    // Enables a monochrome visual effect.
    {0xF8DA, "set_screen_bw", "set_screen_b&w", {}, F_V3_V4},

    // Computes a point along a path composed of multiple control points.
    // valueA = number of control points
    // valueB = speed along path
    // valueC = current position along path
    // valueD = loop flag (0 = no, 1 = yes)
    // regsE[0-2] = result point (x, y, z as floats)
    // regsE[3] = the result code (0 = failed, 1 = success)
    // labelF = control point entries (array of valueA VectorXYZTF structures)
    {0xF8DB, "get_vector_from_path", "unknownF8DB", {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, SCRIPT16}, F_V3_V4 | F_ARGS},

    // Same as npc_text, but only applies to a specific player slot.
    // valueA = client ID
    // valueB = situation number (same as for npc_text)
    // strC = string for NPC to say (up to 52 characters)
    {0xF8DC, "npc_text_id", "NPC_action_string", {REG, REG, CSTRING_LABEL16}, F_V3_V4},

    // Returns a bitmask of the buttons which are currently pressed or held on
    // this frame.
    // regA = controller port number
    // regB = returned button flags
    {0xF8DD, "get_held_buttons", "get_pad_cond", {REG, REG}, F_V3_V4},

    // Returns a bitmask of the buttons which were newly pressed on this frame.
    // Buttons which were pressed on prevous frames and still held down are not
    // returned. Same arguments as get_held_buttons.
    {0xF8DE, "get_pressed_buttons", "get_button_cond", {REG, REG}, F_V3_V4},

    // Freezes enemies and makes them untargetable, or unfreezes them and makes
    // them targetable again. Internally, this toggles a flag that disables
    // updates for every child object of TL_04.
    {0xF8DF, "toggle_freeze_enemies", "freeze_enemies", {}, F_V3_V4},

    // Unfreezes enemies and makes them targetable again.
    {0xF8E0, "unfreeze_enemies", nullptr, {}, F_V3_V4},

    // Freezes (almost) everything, or unfreezes (almost) everything.
    // Internally, this toggles the disable-updates flag for every child of all
    // root objects except TL_SU, TL_00, TL_CAMERA, and TL_01.
    {0xF8E1, "toggle_freeze_everything", "freeze_everything", {}, F_V3_V4},

    // Unfreezes (almost) everything, if toggle_freeze_everything has been
    // executed before.
    {0xF8E2, "unfreeze_everything", nullptr, {}, F_V3_V4},

    // Sets a player's HP or TP to their maximum HP or TP.
    // regA = client ID
    {0xF8E3, "restore_hp", nullptr, {REG}, F_V3_V4},
    {0xF8E4, "restore_tp", nullptr, {REG}, F_V3_V4},

    // Closes a chat bubble for a player, if one is open.
    // regA = client ID
    {0xF8E5, "close_chat_bubble", nullptr, {REG}, F_V3_V4},

    // Moves a dynamic collision object.
    // regA = object token (returned by set_obj_param, etc.)
    // regsB[0-2] = location (x, y, z as integers)
    {0xF8E6, "move_coords_object", nullptr, {REG, {REG_SET_FIXED, 3}}, F_V3_V4},

    // These are the same as their counterparts without _ex, but these return
    // an object token in regB which can be used with del_obj_param,
    // move_coords_object, etc. set_obj_param_ex is the same as set_obj_param,
    // since set_obj_param already returns an object token.
    {0xF8E7, "at_coords_call_ex", nullptr, {{REG_SET_FIXED, 5}, REG}, F_V3_V4},
    {0xF8E8, "at_coords_talk_ex", nullptr, {{REG_SET_FIXED, 5}, REG}, F_V3_V4},
    {0xF8E9, "npc_coords_call_ex", "walk_to_coord_call_ex", {{REG_SET_FIXED, 5}, REG}, F_V3_V4},
    {0xF8EA, "party_coords_call_ex", "col_npcinr_ex", {{REG_SET_FIXED, 6}, REG}, F_V3_V4},
    {0xF8EB, "set_obj_param_ex", "unknownF8EB", {{REG_SET_FIXED, 6}, REG}, F_V3_V4},
    {0xF8EC, "npc_check_straggle_ex", "col_plinaw_ex", {{REG_SET_FIXED, 9}, REG}, F_V3_V4},

    // Returns 1 if the player is doing certain animations. (TODO: Which ones?)
    // regA = client ID
    // regB = returned value
    {0xF8ED, "animation_check", nullptr, {REG, REG}, F_V3_V4},

    // Specifies which image to use for the image board in Pioneer 2 (if
    // placed). Only one image may be loaded at a time. Images must be square
    // and dimensions must be a power of two, similar to the B9 command on Ep3.
    // On DC or PCv2, the image data is expected to be a PVR file; on GC, it
    // should be a GVR file; on Xbox and BB it should be an XVR file.
    // regA = decompressed size
    // labelB = label containing PRS-compressed image file data
    {0xF8EE, "call_image_data", nullptr, {INT32, {LABEL16, Arg::DataType::IMAGE_DATA}}, F_V3_V4 | F_ARGS},

    // This opcode does nothing on all versions where it's implemented.
    {0xF8EF, "nop_F8EF", "unknownF8EF", {}, F_V3_V4},

    // Disables or enables the background music on Pioneer 2. If the BGM has
    // not been overridden with create_bgmctrl and set_bgm, the music will
    // resume at the points where it would normally change (e.g. entering or
    // leaving the shop area). turn_off_bgm_p2 may be repeated every frame to
    // avoid this consequence.
    {0xF8F0, "turn_off_bgm_p2", nullptr, {}, F_V3_V4},
    {0xF8F1, "turn_on_bgm_p2", nullptr, {}, F_V3_V4},

    // Computes a point along a Bezier curve defined by a sequence of vectors.
    // This is similar to get_vector_from_path, but results in a smoother
    // curve. Arguments:
    // valueA = number of control points
    // valueB = speed along path
    // valueC = current position along path
    // valueD = loop flag (0 = no, 1 = yes)
    // regsE[0-2] = result point (x, y, z as floats)
    // regsE[3] = the result code (0 = failed, 1 = success)
    // labelF = control point entries (array of valueA VectorXYZTF structures)
    {0xF8F2, "compute_bezier_curve_point", "load_unk_data", {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, {LABEL16, Arg::DataType::BEZIER_CONTROL_POINT_DATA}}, F_V3_V4 | F_ARGS},

    // Creates a timed particle effect. Like the particle opcode, but the
    // location (and duration, for some reason) are floats.
    // regsA[0-2] = location (x, y, z as floats)
    // valueB = effect type
    // valueC = duration as float (in frames; 30 frames/sec)
    {0xF8F3, "particle2", nullptr, {{REG_SET_FIXED, 3}, INT32, FLOAT32}, F_V3_V4 | F_ARGS},

    // Converts the integer in regB into a float in regA.
    {0xF901, "dec2float", nullptr, {REG, REG}, F_V3_V4},

    // Converts the float in regB into an integer in regA.
    {0xF902, "float2dec", nullptr, {REG, REG}, F_V3_V4},

    // These are the same as let and leti. Nominally regB/valueB should be a
    // float, but the implementation treats it as an int (which is still
    // correct).
    {0xF903, "flet", "floatlet", {REG, REG}, F_V3_V4},
    {0xF904, "fleti", "floati", {REG, FLOAT32}, F_V3_V4},

    // regA += regB (or valueB), as floats
    {0xF908, "fadd", nullptr, {REG, REG}, F_V3_V4},
    {0xF909, "faddi", nullptr, {REG, FLOAT32}, F_V3_V4},

    // regA -= regB (or valueB), as floats
    {0xF90A, "fsub", nullptr, {REG, REG}, F_V3_V4},
    {0xF90B, "fsubi", nullptr, {REG, FLOAT32}, F_V3_V4},

    // regA *= regB (or valueB), as floats
    {0xF90C, "fmul", nullptr, {REG, REG}, F_V3_V4},
    {0xF90D, "fmuli", nullptr, {REG, FLOAT32}, F_V3_V4},

    // regA /= regB (or valueB), as floats
    {0xF90E, "fdiv", nullptr, {REG, REG}, F_V3_V4},
    {0xF90F, "fdivi", nullptr, {REG, FLOAT32}, F_V3_V4},

    // Returns the number of times a player has ever died - not just in the
    // current quest/game/session!
    // regA = client ID
    // regB = returned death count
    {0xF910, "get_total_deaths", "get_unknown_count?", {CLIENT_ID, REG}, F_V3_V4 | F_ARGS},

    // Returns the stack size of the specified item in the player's inventory.
    // regsA[0] = client ID
    // regsA[1-3] = item.data1[0-2]
    // regB = returned amount of item present in player's inventory
    // If the item is not present, returns 0.
    {0xF911, "get_item_count", "get_stackable_item_count", {{REG_SET_FIXED, 4}, REG}, F_V3_V4},

    // Freezes a character and hides their equips, or does the opposite.
    // Internally, this toggles the disable-update flag on TL_03.
    {0xF912, "toggle_freeze_and_hide_equip", "freeze_and_hide_equip", {}, F_V3_V4},

    // Undoes the effect of toggle_freeze_and_hide_equip, if it has been run an
    // odd number of times. (Otherwise, does nothing.)
    {0xF913, "thaw_and_show_equip", nullptr, {}, F_V3_V4},

    // This opcode defines which label should be called when the X button is
    // overridden by the following opcodes. This doesn't activate the logic by
    // itself, however. It is generally used in the following sequence:
    //   set_palettex_callback   r:client_id, callback_label
    //   activate_palettex       r:client_id
    //   enable_palettex         r:client_id
    //   ... (X button now runs a quest function when pressed)
    //   disable_palettex        r:client_id
    //   restore_palettex        r:client_id
    //   ... (X button now behaves normally)
    // Arguments:
    // regA = client ID
    // labelB = function to call when X button is pressed
    {0xF914, "set_palettex_callback", "set_paletteX_callback", {CLIENT_ID, SCRIPT16}, F_V3_V4 | F_ARGS},
    {0xF915, "activate_palettex", "activate_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF916, "enable_palettex", "enable_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF917, "restore_palettex", "restore_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF918, "disable_palettex", "disable_paletteX", {CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Checks if activate_palettex has been run for a player.
    // regA = client ID
    // regB = returned flag (0 = not overridden, 1 = overridden)
    {0xF919, "get_palettex_activated", "get_paletteX_activated", {CLIENT_ID, REG}, F_V3_V4 | F_ARGS},

    // Checks if activate_palettex has been run for a player.
    // regA = client ID
    // valueB = ignored
    // regC = returned flag (0 = not overridden, 1 = overridden)
    // The ignored argument here seems to be a bug. At least one official quest
    // uses this opcode preceded by only two arg_push opcodes, implying that it
    // was intended to take two arguments, but the client really does only use
    // quest_arg_stack[0] and quest_arg_stack[2].
    {0xF91A, "get_palettex_enabled", "get_unknown_paletteX_status?", {CLIENT_ID, INT32, REG}, F_V3_V4 | F_ARGS},

    // Disables/enables movement for a player. Unlike disable_movement1, this
    // does not send 6x2C.
    // regA = client ID
    {0xF91B, "disable_movement2", nullptr, {CLIENT_ID}, F_V3_V4 | F_ARGS},
    {0xF91C, "enable_movement2", nullptr, {CLIENT_ID}, F_V3_V4 | F_ARGS},

    // Returns the local player's play time in seconds.
    {0xF91D, "get_time_played", nullptr, {REG}, F_V3_V4},

    // Returns the number of Guild Cards saved to the Guild Card file.
    {0xF91E, "get_guildcard_total", nullptr, {REG}, F_V3_V4},

    // Returns the amount of Meseta the player has in both their inventory and
    // bank.
    // regA = returned Meseta amount in inventory
    // regB = returned Meseta amount in bank
    {0xF91F, "get_slot_meseta", nullptr, {{REG_SET_FIXED, 2}}, F_V3_V4},

    // Returns a player's level.
    // valueA = client ID
    // regB = returned level
    {0xF920, "get_player_level", nullptr, {CLIENT_ID, REG}, F_V3_V4 | F_ARGS},

    // Returns a player's section ID.
    // valueA = client ID
    // regB = returned section ID (see name_to_section_id in StaticGameData.cc)
    {0xF921, "get_section_id", "get_Section_ID", {CLIENT_ID, REG}, F_V3_V4 | F_ARGS},

    // Returns a player's maximum and current HP and TP.
    // valueA = client ID
    // regsB[0] = returned maximum HP
    // regsB[1] = returned current HP
    // regsB[2] = returned maximum TP
    // regsB[3] = returned current TP
    // If there's no player in the given slot, the returned values are all
    // FFFFFFFF.
    {0xF922, "get_player_hp_tp", "get_player_hp", {CLIENT_ID, {REG_SET_FIXED, 4}}, F_V3_V4 | F_ARGS},

    // Returns the floor and room ID of the given player.
    // valueA = client ID
    // regsB[0] = returned floor number
    // regsB[1] = returned room number
    // If there's no player in the given slot, the returned values are both
    // FFFFFFFF.
    {0xF923, "get_player_room", "get_floor_number", {CLIENT_ID, {REG_SET_FIXED, 2}}, F_V3_V4 | F_ARGS},

    // Checks if each player (individually) is near the given location.
    // regsA[0-1] = location (x, z as integers; y not included)
    // regsA[2] = radius as integer
    // regsB[0-3] = returned results for each player slot (0 = player not
    //   present or outside radius; 1 = player within radius)
    {0xF924, "get_coord_player_detect", nullptr, {{REG_SET_FIXED, 3}, {REG_SET_FIXED, 4}}, F_V3_V4},

    // Reads the value of a quest counter.
    // valueA = counter index (0-15)
    // regB = returned value
    {0xF925, "read_counter", "read_global_flag", {INT32, REG}, F_V3_V4 | F_ARGS},

    // Writes a value to a quest counter.
    // valueA = counter index (0-15)
    // valueB = value to write
    {0xF926, "write_counter", "write_global_flag", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Checks if an item exists in the local player's bank.
    // regsA[0-2] = item.data1[0-2]
    // regsA[3] = item.data1[4]
    // regB = 1 if item was found, 0 if not
    {0xF927, "item_detect_bank2", "item_check_bank", {{REG_SET_FIXED, 4}, REG}, F_V3_V4},

    // Returns whether each player is present.
    // regsA[0-3] = returned flags (for each player: 0 if absent, 1 if present)
    {0xF928, "get_players_present", "floor_player_detect", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Prepares to load a GBA ROM from a local GSL file. Does nothing on Xbox
    // and BB.
    {0xF929, "prepare_gba_rom_from_disk", "read_disk_file?", {CSTRING}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_ARGS},
    {0xF929, "nop_F929", nullptr, {CSTRING}, F_XB_V3 | F_V4 | F_ARGS},

    // Opens a window for the player to choose an item.
    {0xF92A, "open_pack_select", nullptr, {}, F_V3_V4},

    // Prevents the player from choosing a specific item in the item select
    // window opened by open_pack_select. Generally used for subsequent item
    // choices when trading multiple items.
    // regA = item ID
    {0xF92B, "prevent_item_select", "item_select", {REG}, F_V3_V4},

    // Returns the item chosen by the player in an open_pack_select window, or
    // FFFFFFFF if they canceled it.
    {0xF92C, "get_item_id", nullptr, {REG}, F_V3_V4},

    // Adds a color overlay on the player's screen. The overlay fades in
    // linearly over the given number of frames. The overlay is not deleted
    // until the player changes areas or leaves the game, but it can be
    // overwritten with another overlay with this same opcode. The overlay is
    // under 2-dimensional objects like the HUD, pause menu, minimap, and text
    // messages from the server, but is above everything else.
    // regA, regB, regC, regD = red, green, blue, alpha components of color
    //   (00-FF each)
    // regE = fade speed (number of frames; 30 frames/sec)
    {0xF92D, "add_color_overlay", "color_change", {INT32, INT32, INT32, INT32, INT32}, F_V3_V4 | F_ARGS},

    // Sends a statistic to the server via the AA command. The server is
    // expected to respond with an AB command containing one of the label
    // indexes set by prepare_statistic. The arguments to this opcode are sent
    // verbatim in the params field of the AA command; the other fields in the
    // AA command come from prepare_statistic.
    {0xF92E, "send_statistic", "send_statistic?", {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, F_V3_V4 | F_ARGS},

    // Enables patching a GBA ROM before sending it to the GBA.
    // valueA is ignored. If valueB is 1, the game writes two 32-bit values to
    // the GBA ROM data before sending it:
    //   At offset 0x2C0, writes system_file->creation_timestamp
    //   At offset 0x2C4, writes current_time + rand(0, 100)
    // current_time is in seconds since 00:00:00 on 1 January 2000.
    // On Xbox and BB, this opcode does nothing.
    {0xF92F, "gba_write_identifiers", "gba_unknown5", {INT32, INT32}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3 | F_ARGS},
    {0xF92F, "nop_F92F", nullptr, {INT32, INT32}, F_XB_V3 | F_V4 | F_ARGS},

    // Shows a message in a chat window. Can be closed with the winend opcode.
    // valueA = X position on screen
    // valueB = Y position on screen
    // valueC = window width
    // valueD = window height
    // valueE = window style (0 = white window, 1 = chat box, anything else =
    //   no window, just text)
    // strF = initial message
    {0xF930, "chat_box", nullptr, {INT32, INT32, INT32, INT32, INT32, CSTRING}, F_V3_V4 | F_ARGS},

    // Shows a chat bubble linked to the given entity ID.
    // valueA = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    // strB = message
    {0xF931, "chat_bubble", nullptr, {INT32, CSTRING}, F_V3_V4 | F_ARGS},

    // Sets the episode to be loaded the next time an area is loaded. ValueA is
    // the same as for set_episode.
    {0xF932, "set_episode2", nullptr, {REG}, F_V3_V4},

    // Sets the rank prizes in offline challenge mode.
    // regsA[0] = rank (unusual value order: 0 = S, 1 = B, 2 = A)
    // regsA[1-6] = item.data1[0-5]
    {0xF933, "item_create_multi_cm", "unknownF933", {{REG_SET_FIXED, 7}}, F_V3},
    {0xF933, "nop_F933", nullptr, {{REG_SET_FIXED, 7}}, F_V4},

    // Shows a scrolling text window.
    // valueA = X position on screen
    // valueB = Y position on screen
    // valueC = window width
    // valueD = window height
    // valueE = window style (same as for chat_box)
    // valueF = scrolling speed
    // regG = set to 1 when message has entirely scrolled past
    // strH = message
    {0xF934, "scroll_text", nullptr, {INT32, INT32, INT32, INT32, INT32, FLOAT32, REG, CSTRING}, F_V3_V4 | F_ARGS},

    // Creates, destroys, or updates the GBA loading progress bar (same as the
    // quest download progress bar). These opcodes do nothing on Xbox and BB.
    {0xF935, "gba_create_dl_graph", "gba_unknown1", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF935, "nop_F935", nullptr, {}, F_XB_V3 | F_V4},
    {0xF936, "gba_destroy_dl_graph", "gba_unknown2", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF936, "nop_F936", nullptr, {}, F_XB_V3 | F_V4},
    {0xF937, "gba_update_dl_graph", "gba_unknown3", {}, F_GC_V3 | F_GC_EP3TE | F_GC_EP3},
    {0xF937, "nop_F937", nullptr, {}, F_XB_V3 | F_V4},

    // Damages a player.
    // regA = client ID
    // regB = amount
    {0xF938, "add_damage_to", "add_damage_to?", {INT32, INT32}, F_V3_V4 | F_ARGS},

    // Deletes an item from the local player's inventory. Like item_delete, but
    // doesn't return anything.
    // valueA = item ID
    {0xF939, "item_delete_noreturn", "item_delete_slot", {INT32}, F_V3_V4 | F_ARGS},

    // Returns the item data for an item chosen with open_pack_select.
    // valueA = item ID
    // regsB[0-11] = returned item.data1[0-11]
    {0xF93A, "get_item_info", nullptr, {ITEM_ID, {REG_SET_FIXED, 12}}, F_V3_V4 | F_ARGS},

    // Wraps an item in the player's inventory. The specified item is deleted
    // and a new one is created with the wrapped flag set. The new item has a
    // different item ID, which is not returned. Sends 6x29, then 6x26 if
    // deleting the item causes the player's equipped weapon to no longer be
    // usable, then 6x2B. It appears Sega did not intend for this to be used on
    // BB, since the behavior wasn't changed - normally, item-related functions
    // should be done by the server on BB, as the following opcode does.
    // valueA = item ID
    {0xF93B, "wrap_item", "item_packing1", {ITEM_ID}, F_V3_V4 | F_ARGS},

    // Like wrap_item, but also sets the present color. On BB, sends 6xD6, and
    // the server should respond with the sequence of 6x29, maybe 6x26, then
    // 6xBE.
    // valueA = item ID
    // valueB = present color (0-15; high 4 bits are masked out)
    {0xF93C, "wrap_item_with_color", "item_packing2", {ITEM_ID, INT32}, F_V3_V4 | F_ARGS},

    // Returns the local player's language setting. For values, see
    // name_for_language_code in StaticGameData.cc.
    {0xF93D, "get_lang_setting", "get_lang_setting?", {REG}, F_V3_V4 | F_ARGS},

    // Sets some values to be sent to the server with send_statistic.
    // valueA = stat_id (used in send_statistic)
    // labelB = label1 (used in send_statistic)
    // labelC = label2 (used in send_statistic)
    {0xF93E, "prepare_statistic", "prepare_statistic?", {INT32, LABEL32, LABEL32}, F_V3_V4 | F_ARGS},

    // Enables use of the check_for_keyword opcode.
    {0xF93F, "enable_keyword_detect", "keyword_detect", {}, F_V3_V4},

    // Checks if a word was said by a specific player. All of the same
    // behaviors surrounding the message string apply as for set_chat_callback.
    // Generally, this should be called every frame until the keyword is found
    // (or the quest no longer cares if it is), since it will only return a
    // match on the first frame that a string is said.
    // regA = result (0 = word not said, 1 = word said)
    // valueB = client ID
    // strC = string to match (same semantics as for set_chat_callback)
    {0xF940, "check_for_keyword", "keyword", {REG, CLIENT_ID, CSTRING}, F_V3_V4 | F_ARGS},

    // Returns a player's Guild Card number.
    // valueA = client ID
    // regB = returned Guild Card number
    {0xF941, "get_guildcard_num", nullptr, {CLIENT_ID, REG}, F_V3_V4 | F_ARGS},

    // Returns the last symbol chat that a player said (at least, since the
    // capture buffer was created). Use create_symbol_chat_capture_buffer
    // before using this opcode. The data is returned as raw values in a
    // sequence of 15 registers, in the same internal format as the game uses.
    // On GC, this is SymbolChatBE; on all other platforms, it is SymbolChat
    // (see SymbolChatT in PlayerSubordinates.hh for details).
    // valueA = client ID
    // regsB[0-14] = returned symbol chat data
    {0xF942, "get_recent_symbol_chat", "symchat_unknown", {INT32, {REG_SET_FIXED, 15}}, F_V3_V4 | F_ARGS},

    // Creates the capture buffer required by get_recent_symbol_chat.
    {0xF943, "create_symbol_chat_capture_buffer", "unknownF943", {}, F_V3_V4},

    // Checks whether an item is stackable.
    // valueA = item ID
    // regB = result (0 = not stackable, 1 = stackable, FFFFFFFF = not found)
    {0xF944, "get_item_stackability", "get_wrap_status", {ITEM_ID, REG}, F_V3_V4 | F_ARGS},

    // Sets the floor where the players will start. This generally should be
    // used in the start label (where map_designate, etc. are used). This is
    // exactly the same as ba_initial_floor, except the floor numbers are not
    // remapped: in Episode 1, 0 means Pioneer 2, 1 means Forest 1, etc.
    {0xF945, "initial_floor", nullptr, {INT32}, F_V3_V4 | F_ARGS},

    // Computes the sine, cosine, or tangent of the input angle. Angles are
    // measured as numbers in the range [0, 65536].
    // regA = result value
    // valueB = input angle
    {0xF946, "sin", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},
    {0xF947, "cos", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},
    {0xF948, "tan", nullptr, {REG, INT32}, F_V3_V4 | F_ARGS},

    // Computes the arctangent of the input ratio. Equivalent C:
    //   regA = (int)((atan2(valueB, valueC) * 65536.0) / (2 * M_PI))
    // regA = result (integer angle, 0-65535)
    // valueB = numerator as float
    // valueC = denominator as float
    {0xF949, "atan2_int", "atan", {REG, FLOAT32, FLOAT32}, F_V3_V4 | F_ARGS},

    // Sets regA to 1 if Olga Flow has been defeated, or 0 otherwise.
    {0xF94A, "olga_flow_is_dead", "olga_is_dead", {REG}, F_V3_V4},

    // Creates a timed particle effect. Similar to the particle opcode, but the
    // created particles have no draw distance, so they are visible from very
    // far away.
    // regsA[0-2] = location (x, y, z as integers)
    // regsA[3] = effect type
    // regsA[4] = duration (in frames; 30 frames/sec)
    {0xF94B, "particle_effect_nc", "particle3", {{REG_SET_FIXED, 5}}, F_V3_V4},

    // Creates a particle effect on a given entity. Similar to the particle_id
    // opcode, but the created particles have no draw distance, so they are
    // visible from very far away.
    // regsA[0] = effect type
    // regsA[1] = duration in frames
    // regsA[2] = entity (client ID, 0x1000 + enemy ID, or 0x4000 + object ID)
    // regsA[3] = y offset (as integer)
    {0xF94C, "player_effect_nc", "particle3f_id", {{REG_SET_FIXED, 4}}, F_V3_V4},

    // Returns 1 in regA if a file named PSO3_CHARACTER is present on either
    // memory card. This opcode is only available on PSO Plus on GC; that is,
    // it only exists on JP v1.4, JP v1.5, and US v1.2.
    {0xF94D, "has_ep3_save_file", nullptr, {REG}, F_GC_V3 | F_ARGS},

    // Gives the player one copy of a card. regA is the card ID.
    {0xF94D, "give_card", "is_there_cardbattle?", {REG}, F_GC_EP3TE},

    // Gives the player one copy of a card, or takes one copy away.
    // regsA[0] = card_id
    // regsA[1] = action (give card if >= 0, take card if < 0)
    {0xF94D, "give_or_take_card", "is_there_cardbattle?", {{REG_SET_FIXED, 2}}, F_GC_EP3},

    // TODO(DX): Related to voice chat, but functionality is unknown. valueA is
    // a client ID; a value is read from that player's TVoiceChatClient object
    // and (!!value) is placed in regB. This value is set by the 6xB3 command.
    {0xF94D, "unknown_F94D", nullptr, {INT32, REG}, F_XB_V3 | F_ARGS},

    // These opcodes all do nothing on BB. F94D is presumably the voice chat
    // opcode from Xbox, which was removed, but it's not clear what the other
    // two might have originally been.
    {0xF94D, "nop_F94D", nullptr, {}, F_V4},
    {0xF94E, "nop_F94E", nullptr, {}, F_V4},
    {0xF94F, "nop_F94F", nullptr, {}, F_V4},

    // Opens one of the Pioneer 2 counter menus, specified by valueA. Values:
    // 0 = weapon shop
    // 1 = medical center
    // 2 = armor shop
    // 3 = item shop
    // 4 = Hunter's Guild quest counter
    // 5 = bank
    // 6 = tekker
    // 7 = government quest counter
    // valueA is not bounds-checked, so it could be used to write a byte with
    // the value 1 anywhere in memory.
    {0xF950, "bb_p2_menu", "BB_p2_menu", {INT32}, F_V4 | F_ARGS},

    // Behaves exactly the same as map_designate_ex, but the arguments are
    // specified as immediate values and not via registers or arg_push.
    // valueA = floor number
    // valueB = area number
    // valueC = type (0: use layout, 1: use offline template, 2: use online
    //   template, 3: nothing)
    // valueD = major variation
    // valueE = minor variation
    {0xF951, "bb_map_designate", "BB_Map_Designate", {INT8, INT8, INT8, INT8, INT8}, F_V4},

    // Returns the number of items in the player's inventory.
    {0xF952, "bb_get_number_in_pack", "BB_get_number_in_pack", {REG}, F_V4},

    // Requests an item exchange in the player's inventory. Sends 6xD5.
    // valueA/valueB/valueC = item.data1[0-2] to search for
    // valueD/valueE/valueF = item.data1[0-2] to replace it with
    // labelG = label to call on success
    // labelH = label to call on failure
    {0xF953, "bb_swap_item", "BB_swap_item", {INT32, INT32, INT32, INT32, INT32, INT32, SCRIPT16, SCRIPT16}, F_V4 | F_ARGS},

    // Checks if an item can be wrapped.
    // valueA = item ID
    // regB = returned status (0 = can't be wrapped, 1 = can be wrapped,
    //   2 = item not found)
    {0xF954, "bb_check_wrap", "BB_check_wrap", {INT32, REG}, F_V4 | F_ARGS},

    // Requests an item exchange for Photon Drops. Sends 6xD7.
    // valueA/valueB/valueC = item.data1[0-2] for requested item
    // labelD = label to call on success
    // labelE = label to call on failure
    {0xF955, "bb_exchange_pd_item", "BB_exchange_PD_item", {INT32, INT32, INT32, LABEL16, LABEL16}, F_V4 | F_ARGS},

    // Requests an S-rank special upgrade in exchange for Photon Drops. Sends
    // 6xD8.
    // valueA = item ID
    // valueB/valueC/valueD = item.data1[0-2]
    // valueE = special type
    // labelF = label to call on success
    // labelG = label to call on failure
    {0xF956, "bb_exchange_pd_srank", "BB_exchange_PD_srank", {INT32, INT32, INT32, INT32, INT32, LABEL16, LABEL16}, F_V4 | F_ARGS},

    // Requests a weapon attribute upgrade in exchange for Photon Drops. Sends
    // 6xDA.
    // valueA = item ID
    // valueB/valueC/valueD = item.data1[0-2]
    // valueE = attribute to upgrade
    // valueF = payment count (number of PDs)
    // labelG = label to call on success
    // labelH = label to call on failure
    {0xF957, "bb_exchange_pd_percent", "BB_exchange_PD_special", {INT32, INT32, INT32, INT32, INT32, INT32, LABEL16, LABEL16}, F_V4 | F_ARGS},

    // Requests a weapon attribute upgrade in exchange for Photon Spheres.
    // Sends 6xDA. Same arguments as bb_exchange_pd_percent, except Photon
    // Spheres are used instead.
    {0xF958, "bb_exchange_ps_percent", "BB_exchange_PS_percent", {INT32, INT32, INT32, INT32, INT32, INT32, LABEL16, LABEL16}, F_V4 | F_ARGS},

    // Determines whether the Episode 4 boss can escape if undefeated after 20
    // minutes.
    // valueA = boss can escape (0 = no, 1 = yes (default))
    {0xF959, "bb_set_ep4_boss_can_escape", "BB_set_ep4boss_can_escape", {INT32}, F_V4 | F_ARGS},

    // Returns 1 if the Episode 4 boss death cutscene is playing, or 0 if not
    // (even if the boss has already been defeated).
    {0xF95A, "bb_is_ep4_boss_dying", nullptr, {REG}, F_V4},

    // Requests an item exchange. Sends 6xD9.
    // valueA = find_item.data1[0-2] (low 3 bytes; high byte unused)
    // valueB = replace_item.data1[0-2] (low 3 bytes; high byte unused)
    // valueC = token1 (see 6xD9 in CommandFormats.hh)
    // valueD = token2 (see 6xD9 in CommandFormats.hh)
    // labelE = label to call on success
    // labelF = label to call on failure
    {0xF95B, "bb_send_6xD9", nullptr, {INT32, INT32, INT32, INT32, LABEL16, LABEL16}, F_V4 | F_ARGS},

    // Requests an exchange of Secret Lottery Tickets for items. Sends 6xDE.
    // See SecretLotteryResultItems in config.json for the item pool used by
    // this opcode.
    // valueA = index
    // valueB = unknown_a1
    // labelC = label to call on success
    // labelD = label to call on failure (unused because of a client bug; see
    //   6xDE description in CommandFormats.hh for details)
    {0xF95C, "bb_exchange_slt", "BB_exchange_SLT", {INT32, INT32, LABEL32, LABEL32}, F_V4 | F_ARGS},

    // Removes a single Photon Crystal from the player's inventory, and
    // disables drops for the rest of the quest. Sends 6xDF.
    {0xF95D, "bb_exchange_pc", "BB_exchange_PC", {}, F_V4},

    // Requests an item drop within a quest. Sends 6xE0.
    // valueA = type (corresponds to QuestF95EResultItems in config.json)
    // valueB/valueC = x, z coordinates as floats
    {0xF95E, "bb_box_create_bp", "BB_box_create_BP", {INT32, FLOAT32, FLOAT32}, F_V4 | F_ARGS},

    // Requests an exchange of Photon Tickets for items. Sends 6xE1.
    // valueA = unknown_a1
    // valueB = unknown_a2
    // valueC = result index (index into QuestF95FResultItems in config.json)
    // labelD = label to call on success
    // labelE = label to call on failure
    {0xF95F, "bb_exchange_pt", "BB_exchage_PT", {INT32, INT32, INT32, INT32, INT32}, F_V4 | F_ARGS},

    // Requests a prize from the Meseta gambling prize list. Sends 6xE2. The
    // server responds with 6xE3, which sets the <meseta_slot_prize>
    // replacement token in message strings. The status of this can be checked
    // with bb_get_6xE3_status.
    // valueA = result tier (see QuestF960SuccessResultItems and
    //   QuestF960FailureResultItems in config.json)
    {0xF960, "bb_send_6xE2", "unknownF960", {INT32}, F_V4 | F_ARGS},

    // Returns the status of the expected 6xE3 command from a preceding
    // bb_send_6xE2 opcode. Return values:
    //   0 = 6xE3 hasn't been received
    //   1 = the received item is valid
    //   2 = the received item is invalid, or the item ID was already in use
    {0xF961, "bb_get_6xE3_status", "unknownF961", {REG}, F_V4},
};

static const unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>&
opcodes_for_version(Version v) {
  static array<
      unordered_map<uint16_t, const QuestScriptOpcodeDefinition*>,
      static_cast<size_t>(Version::BB_V4) + 1>
      indexes;

  auto& index = indexes.at(static_cast<size_t>(v));
  if (index.empty()) {
    uint16_t vf = v_flag(v);
    for (size_t z = 0; z < sizeof(opcode_defs) / sizeof(opcode_defs[0]); z++) {
      const auto& def = opcode_defs[z];
      if (!(def.flags & vf)) {
        continue;
      }
      if (!index.emplace(def.opcode, &def).second) {
        throw logic_error(phosg::string_printf("duplicate definition for opcode %04hX", def.opcode));
      }
    }
  }
  return index;
}

static const unordered_map<string, const QuestScriptOpcodeDefinition*>&
opcodes_by_name_for_version(Version v) {
  static array<
      unordered_map<string, const QuestScriptOpcodeDefinition*>,
      static_cast<size_t>(Version::BB_V4) + 1>
      indexes;

  auto& index = indexes.at(static_cast<size_t>(v));
  if (index.empty()) {
    uint16_t vf = v_flag(v);
    for (size_t z = 0; z < sizeof(opcode_defs) / sizeof(opcode_defs[0]); z++) {
      const auto& def = opcode_defs[z];
      if (!(def.flags & vf)) {
        continue;
      }
      if (def.name && !index.emplace(def.name, &def).second) {
        throw logic_error(phosg::string_printf("duplicate definition for opcode %04hX", def.opcode));
      }
      if (def.qedit_name) {
        string lower_qedit_name = phosg::tolower(def.qedit_name);
        if ((lower_qedit_name != def.name) && !index.emplace(lower_qedit_name, &def).second) {
          throw logic_error(phosg::string_printf("duplicate definition for opcode %04hX", def.opcode));
        }
      }
    }
  }
  return index;
}

void check_opcode_definitions() {
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
    phosg::log_info("Version %s has %zu opcodes with %zu mnemonics", phosg::name_for_enum(v), opcodes.size(), opcodes_by_name.size());
  }
}

std::string disassemble_quest_script(
    const void* data,
    size_t size,
    Version version,
    uint8_t override_language,
    bool reassembly_mode,
    bool use_qedit_names) {
  phosg::StringReader r(data, size);
  deque<string> lines;
  lines.emplace_back(phosg::string_printf(".version %s", phosg::name_for_enum(version)));

  bool use_wstrs = false;
  size_t code_offset = 0;
  size_t function_table_offset = 0;
  uint8_t language;
  switch (version) {
    case Version::DC_NTE: {
      const auto& header = r.get<PSOQuestHeaderDCNTE>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      language = 0;
      lines.emplace_back(".name " + escape_string(header.name.decode(0)));
      break;
    }
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2: {
      const auto& header = r.get<PSOQuestHeaderDC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      if (override_language != 0xFF) {
        language = override_language;
      } else if (header.language < 5) {
        language = header.language;
      } else {
        language = 1;
      }
      lines.emplace_back(phosg::string_printf(".quest_num %hu", header.quest_number.load()));
      lines.emplace_back(phosg::string_printf(".language %hhu", header.language));
      lines.emplace_back(".name " + escape_string(header.name.decode(language)));
      lines.emplace_back(".short_desc " + escape_string(header.short_description.decode(language)));
      lines.emplace_back(".long_desc " + escape_string(header.long_description.decode(language)));
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      use_wstrs = true;
      const auto& header = r.get<PSOQuestHeaderPC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      if (override_language != 0xFF) {
        language = override_language;
      } else if (header.language < 8) {
        language = header.language;
      } else {
        language = 1;
      }
      lines.emplace_back(phosg::string_printf(".quest_num %hu", header.quest_number.load()));
      lines.emplace_back(phosg::string_printf(".language %hhu", header.language));
      lines.emplace_back(".name " + escape_string(header.name.decode(language)));
      lines.emplace_back(".short_desc " + escape_string(header.short_description.decode(language)));
      lines.emplace_back(".long_desc " + escape_string(header.long_description.decode(language)));
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      const auto& header = r.get<PSOQuestHeaderGC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      if (override_language != 0xFF) {
        language = override_language;
      } else if (header.language < 5) {
        language = header.language;
      } else {
        language = 1;
      }
      lines.emplace_back(phosg::string_printf(".quest_num %hhu", header.quest_number));
      lines.emplace_back(phosg::string_printf(".language %hhu", header.language));
      lines.emplace_back(phosg::string_printf(".episode %s", name_for_header_episode_number(header.episode)));
      lines.emplace_back(".name " + escape_string(header.name.decode(language)));
      lines.emplace_back(".short_desc " + escape_string(header.short_description.decode(language)));
      lines.emplace_back(".long_desc " + escape_string(header.long_description.decode(language)));
      break;
    }
    case Version::BB_V4: {
      use_wstrs = true;
      const auto& header = r.get<PSOQuestHeaderBB>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      if (override_language != 0xFF) {
        language = override_language;
      } else {
        language = 1;
      }
      lines.emplace_back(phosg::string_printf(".quest_num %hu", header.quest_number.load()));
      lines.emplace_back(phosg::string_printf(".episode %s", name_for_header_episode_number(header.episode)));
      lines.emplace_back(phosg::string_printf(".max_players %hhu", header.max_players ? header.max_players : 4));
      if (header.joinable) {
        lines.emplace_back(".joinable");
      }
      lines.emplace_back(".name " + escape_string(header.name.decode(language)));
      lines.emplace_back(".short_desc " + escape_string(header.short_description.decode(language)));
      lines.emplace_back(".long_desc " + escape_string(header.long_description.decode(language)));
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }

  const auto& opcodes = opcodes_for_version(version);
  phosg::StringReader cmd_r = r.sub(code_offset, function_table_offset - code_offset);

  struct Label {
    string name;
    uint32_t offset;
    uint32_t label_index; // 0xFFFFFFFF = no label
    uint64_t type_flags;
    set<size_t> references;

    Label(const string& name, uint32_t offset, int64_t label_index = -1, uint64_t type_flags = 0)
        : name(name),
          offset(offset),
          label_index(label_index),
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
  phosg::StringReader function_table_r = r.sub(function_table_offset);
  while (!function_table_r.eof()) {
    try {
      uint32_t label_index = function_table.size();
      string name = (label_index == 0) ? "start" : phosg::string_printf("label%04" PRIX32, label_index);
      uint32_t offset = function_table_r.get_u32l();
      auto l = make_shared<Label>(name, offset, label_index);
      if (label_index == 0) {
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

  bool version_has_args = F_HAS_ARGS & v_flag(version);
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
          dasm_line = phosg::string_printf(".unknown %04hX", opcode);
        } else {
          const char* op_name = (use_qedit_names && def->qedit_name) ? def->qedit_name : def->name;
          dasm_line = op_name ? op_name : phosg::string_printf("[%04hX]", opcode);
          if (!version_has_args || !(def->flags & F_ARGS)) {
            dasm_line.resize(0x20, ' ');
            bool is_first_arg = true;
            for (const auto& arg : def->args) {
              using Type = QuestScriptOpcodeDefinition::Argument::Type;
              string dasm_arg;
              switch (arg.type) {
                case Type::LABEL16:
                case Type::LABEL32: {
                  uint32_t label_id = (arg.type == Type::LABEL32) ? cmd_r.get_u32l() : cmd_r.get_u16l();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::LABEL, label_id);
                  }
                  if (label_id >= function_table.size()) {
                    dasm_arg = phosg::string_printf("label%04" PRIX32, label_id);
                  } else {
                    auto& l = function_table.at(label_id);
                    if (reassembly_mode) {
                      dasm_arg = phosg::string_printf("label%04" PRIX32, label_id);
                    } else {
                      dasm_arg = phosg::string_printf("label%04" PRIX32 " /* %04" PRIX32 " */", label_id, l->offset);
                    }
                    l->references.emplace(opcode_start_offset);
                    l->add_data_type(arg.data_type);
                    if (arg.data_type == Arg::DataType::SCRIPT) {
                      pending_dasm_start_offsets.emplace(l->offset);
                    }
                  }
                  break;
                }
                case Type::LABEL16_SET: {
                  if (def->flags & F_PASS) {
                    throw logic_error("LABEL16_SET cannot be pushed to arg stack");
                  }
                  uint8_t num_functions = cmd_r.get_u8();
                  for (size_t z = 0; z < num_functions; z++) {
                    dasm_arg += (dasm_arg.empty() ? "[" : ", ");
                    uint32_t label_id = cmd_r.get_u16l();
                    if (label_id >= function_table.size()) {
                      dasm_arg += phosg::string_printf("label%04" PRIX32, label_id);
                    } else {
                      auto& l = function_table.at(label_id);
                      if (reassembly_mode) {
                        dasm_arg += phosg::string_printf("label%04" PRIX32, label_id);
                      } else {
                        dasm_arg += phosg::string_printf("label%04" PRIX32 " /* %04" PRIX32 " */", label_id, l->offset);
                      }
                      l->references.emplace(opcode_start_offset);
                      l->add_data_type(arg.data_type);
                      if (arg.data_type == Arg::DataType::SCRIPT) {
                        pending_dasm_start_offsets.emplace(l->offset);
                      }
                    }
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "[]";
                  } else {
                    dasm_arg += "]";
                  }
                  break;
                }
                case Type::REG: {
                  uint8_t reg = cmd_r.get_u8();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back((def->opcode == 0x004C) ? ArgStackValue::Type::REG_PTR : ArgStackValue::Type::REG, reg);
                  }
                  dasm_arg = phosg::string_printf("r%hhu", reg);
                  break;
                }
                case Type::REG_SET: {
                  if (def->flags & F_PASS) {
                    throw logic_error("REG_SET cannot be pushed to arg stack");
                  }
                  uint8_t num_regs = cmd_r.get_u8();
                  for (size_t z = 0; z < num_regs; z++) {
                    dasm_arg += phosg::string_printf("%sr%hhu", (dasm_arg.empty() ? "[" : ", "), cmd_r.get_u8());
                  }
                  if (dasm_arg.empty()) {
                    dasm_arg = "[]";
                  } else {
                    dasm_arg += "]";
                  }
                  break;
                }
                case Type::REG_SET_FIXED: {
                  if (def->flags & F_PASS) {
                    throw logic_error("REG_SET_FIXED cannot be pushed to arg stack");
                  }
                  uint8_t first_reg = cmd_r.get_u8();
                  dasm_arg = phosg::string_printf("r%hhu-r%hhu", first_reg, static_cast<uint8_t>(first_reg + arg.count - 1));
                  break;
                }
                case Type::REG32_SET_FIXED: {
                  if (def->flags & F_PASS) {
                    throw logic_error("REG32_SET_FIXED cannot be pushed to arg stack");
                  }
                  uint32_t first_reg = cmd_r.get_u32l();
                  dasm_arg = phosg::string_printf("r%" PRIu32 "-r%" PRIu32, first_reg, static_cast<uint32_t>(first_reg + arg.count - 1));
                  break;
                }
                case Type::INT8: {
                  uint8_t v = cmd_r.get_u8();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = phosg::string_printf("0x%02hhX", v);
                  break;
                }
                case Type::INT16: {
                  uint16_t v = cmd_r.get_u16l();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = phosg::string_printf("0x%04hX", v);
                  break;
                }
                case Type::INT32: {
                  uint32_t v = cmd_r.get_u32l();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, v);
                  }
                  dasm_arg = phosg::string_printf("0x%08" PRIX32, v);
                  break;
                }
                case Type::FLOAT32: {
                  float v = cmd_r.get_f32l();
                  if (def->flags & F_PASS) {
                    arg_stack_values.emplace_back(ArgStackValue::Type::INT, as_type<uint32_t>(v));
                  }
                  dasm_arg = phosg::string_printf("%g", v);
                  break;
                }
                case Type::CSTRING:
                  if (use_wstrs) {
                    phosg::StringWriter w;
                    for (uint16_t ch = cmd_r.get_u16l(); ch; ch = cmd_r.get_u16l()) {
                      w.put_u16l(ch);
                    }
                    if (def->flags & F_PASS) {
                      arg_stack_values.emplace_back(tt_utf16_to_utf8(w.str()));
                    }
                    dasm_arg = escape_string(w.str(), TextEncoding::UTF16);
                  } else {
                    string s = cmd_r.get_cstr();
                    if (def->flags & F_PASS) {
                      arg_stack_values.emplace_back(language ? tt_8859_to_utf8(s) : tt_sega_sjis_to_utf8(s));
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
            dasm_line.resize(0x20, ' ');
            if (reassembly_mode) {
              dasm_line += "...";
            } else {
              dasm_line += "... ";

              if (def->args.size() != arg_stack_values.size()) {
                dasm_line += phosg::string_printf("/* matching error: expected %zu arguments, received %zu arguments */",
                    def->args.size(), arg_stack_values.size());
              } else {
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
                          dasm_arg = phosg::string_printf("r%" PRIu32 "/* warning: cannot determine label data type */", arg_value.as_int);
                          break;
                        case ArgStackValue::Type::LABEL:
                        case ArgStackValue::Type::INT:
                          dasm_arg = phosg::string_printf("label%04" PRIX32, arg_value.as_int);
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
                          dasm_arg = phosg::string_printf("regs[r%" PRIu32 "]", arg_value.as_int);
                          break;
                        case ArgStackValue::Type::INT:
                          dasm_arg = phosg::string_printf("r%" PRIu32, arg_value.as_int);
                          break;
                        default:
                          dasm_arg = "/* invalid-type */";
                      }
                      break;
                    case Arg::Type::REG_SET_FIXED:
                    case Arg::Type::REG32_SET_FIXED:
                      switch (arg_value.type) {
                        case ArgStackValue::Type::REG:
                          dasm_arg = phosg::string_printf("regs[r%" PRIu32 "]-regs[r%" PRIu32 "+%hhu]", arg_value.as_int, arg_value.as_int, static_cast<uint8_t>(arg_def.count - 1));
                          break;
                        case ArgStackValue::Type::INT:
                          dasm_arg = phosg::string_printf("r%" PRIu32 "-r%hhu", arg_value.as_int, static_cast<uint8_t>(arg_value.as_int + arg_def.count - 1));
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
                          dasm_arg = phosg::string_printf("r%" PRIu32, arg_value.as_int);
                          break;
                        case ArgStackValue::Type::REG_PTR:
                          dasm_arg = phosg::string_printf("&r%" PRIu32, arg_value.as_int);
                          break;
                        case ArgStackValue::Type::INT:
                          dasm_arg = phosg::string_printf("0x%" PRIX32 " /* %" PRIu32 " */", arg_value.as_int, arg_value.as_int);
                          break;
                        default:
                          dasm_arg = "/* invalid-type */";
                      }
                      break;
                    case Arg::Type::FLOAT32:
                      switch (arg_value.type) {
                        case ArgStackValue::Type::REG:
                          dasm_arg = phosg::string_printf("f%" PRIu32, arg_value.as_int);
                          break;
                        case ArgStackValue::Type::INT:
                          dasm_arg = phosg::string_printf("%g", as_type<float>(arg_value.as_int));
                          break;
                        default:
                          dasm_arg = "/* invalid-type */";
                      }
                      break;
                    case Arg::Type::CSTRING:
                      if (arg_value.type == ArgStackValue::Type::CSTRING) {
                        dasm_arg = escape_string(arg_value.as_string);
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
          }

          if (!(def->flags & F_PASS)) {
            arg_stack_values.clear();
          }
        }
      } catch (const exception& e) {
        dasm_line = phosg::string_printf(".failed (%s)", e.what());
      }
      phosg::strip_trailing_whitespace(dasm_line);

      string line_text;
      if (reassembly_mode) {
        line_text = phosg::string_printf("  %s", dasm_line.c_str());
      } else {
        string hex_data = phosg::format_data_string(cmd_r.preadx(opcode_start_offset, cmd_r.where() - opcode_start_offset), nullptr, phosg::FormatDataFlags::HEX_ONLY);
        if (hex_data.size() > 14) {
          hex_data.resize(12);
          hex_data += "...";
        }
        hex_data.resize(16, ' ');
        line_text = phosg::string_printf("  %04zX  %s  %s", opcode_start_offset, hex_data.c_str(), dasm_line.c_str());
      }
      dasm_lines.emplace(opcode_start_offset, DisassemblyLine(std::move(line_text), cmd_r.where()));
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
    if (reassembly_mode) {
      lines.emplace_back(phosg::string_printf("%s@0x%04" PRIX32 ":", l->name.c_str(), l->label_index));
    } else {
      lines.emplace_back(phosg::string_printf("%s:", l->name.c_str()));
      if (l->references.size() == 1) {
        lines.emplace_back(phosg::string_printf("  // Referenced by instruction at %04zX", *l->references.begin()));
      } else if (!l->references.empty()) {
        vector<string> tokens;
        tokens.reserve(l->references.size());
        for (size_t reference_offset : l->references) {
          tokens.emplace_back(phosg::string_printf("%04zX", reference_offset));
        }
        lines.emplace_back("  // Referenced by instructions at " + phosg::join(tokens, ", "));
      }
    }

    if (l->type_flags == 0) {
      lines.emplace_back(phosg::string_printf("  // Could not determine data type; disassembling as code"));
      l->add_data_type(Arg::DataType::SCRIPT);
    }

    auto add_disassembly_lines = [&](size_t start_offset, size_t size) -> void {
      for (size_t z = start_offset; z < start_offset + size;) {
        const auto& l = dasm_lines.at(z);
        lines.emplace_back(l.line);
        if (l.next_offset <= z) {
          throw logic_error("line points backward or to itself");
        }
        z = l.next_offset;
      }
    };

    // Print data interpretations of the label (if any)
    if (reassembly_mode) {
      if (l->has_data_type(Arg::DataType::SCRIPT)) {
        add_disassembly_lines(l->offset, size);
      } else {
        lines.emplace_back(".data " + phosg::format_data_string(cmd_r.pgetv(l->offset, size), size));
      }

    } else {
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
            lines.emplace_back(phosg::string_printf("  // As raw data (0x%zX bytes; too small for referenced type)", size));
            lines.emplace_back(format_and_indent_data(cmd_r.pgetv(l->offset, size), size, l->offset));
          }
        }
      };

      if (l->has_data_type(Arg::DataType::DATA)) {
        lines.emplace_back(phosg::string_printf("  // As raw data (0x%zX bytes)", size));
        lines.emplace_back(format_and_indent_data(cmd_r.pgetv(l->offset, size), size, l->offset));
      }
      if (l->has_data_type(Arg::DataType::CSTRING)) {
        lines.emplace_back(phosg::string_printf("  // As C string (0x%zX bytes)", size));
        string str_data = cmd_r.pread(l->offset, size);
        phosg::strip_trailing_zeroes(str_data);
        string formatted;
        if (use_wstrs) {
          if (str_data.size() & 1) {
            str_data.push_back(0);
          }
          formatted = escape_string(str_data, TextEncoding::UTF16);
        } else {
          formatted = escape_string(str_data, encoding_for_language(language));
        }
        lines.emplace_back(phosg::string_printf("  %04" PRIX32 "  %s", l->offset, formatted.c_str()));
      }
      print_as_struct.template operator()<Arg::DataType::PLAYER_VISUAL_CONFIG, PlayerVisualConfig>([&](const PlayerVisualConfig& visual) -> void {
        lines.emplace_back("  // As PlayerVisualConfig");
        string name = escape_string(visual.name.decode(language));
        lines.emplace_back(phosg::string_printf("  %04zX  name              %s", l->offset + offsetof(PlayerVisualConfig, name), name.c_str()));
        lines.emplace_back(phosg::string_printf("  %04zX  name_color        %08" PRIX32, l->offset + offsetof(PlayerVisualConfig, name_color), visual.name_color.load()));
        string a2_str = phosg::format_data_string(visual.unknown_a2.data(), sizeof(visual.unknown_a2));
        lines.emplace_back(phosg::string_printf("  %04zX  a2                %s", l->offset + offsetof(PlayerVisualConfig, unknown_a2), a2_str.c_str()));
        lines.emplace_back(phosg::string_printf("  %04zX  extra_model       %02hhX", l->offset + offsetof(PlayerVisualConfig, extra_model), visual.extra_model));
        string unused = phosg::format_data_string(visual.unused.data(), visual.unused.bytes());
        lines.emplace_back(phosg::string_printf("  %04zX  unused            %s", l->offset + offsetof(PlayerVisualConfig, unused), unused.c_str()));
        lines.emplace_back(phosg::string_printf("  %04zX  name_color_cs     %08" PRIX32, l->offset + offsetof(PlayerVisualConfig, name_color_checksum), visual.name_color_checksum.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  section_id        %02hhX (%s)", l->offset + offsetof(PlayerVisualConfig, section_id), visual.section_id, name_for_section_id(visual.section_id)));
        lines.emplace_back(phosg::string_printf("  %04zX  char_class        %02hhX (%s)", l->offset + offsetof(PlayerVisualConfig, char_class), visual.char_class, name_for_char_class(visual.char_class)));
        lines.emplace_back(phosg::string_printf("  %04zX  validation_flags  %02hhX", l->offset + offsetof(PlayerVisualConfig, validation_flags), visual.validation_flags));
        lines.emplace_back(phosg::string_printf("  %04zX  version           %02hhX", l->offset + offsetof(PlayerVisualConfig, version), visual.version));
        lines.emplace_back(phosg::string_printf("  %04zX  class_flags       %08" PRIX32, l->offset + offsetof(PlayerVisualConfig, class_flags), visual.class_flags.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  costume           %04hX", l->offset + offsetof(PlayerVisualConfig, costume), visual.costume.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  skin              %04hX", l->offset + offsetof(PlayerVisualConfig, skin), visual.skin.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  face              %04hX", l->offset + offsetof(PlayerVisualConfig, face), visual.face.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  head              %04hX", l->offset + offsetof(PlayerVisualConfig, head), visual.head.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  hair              %04hX", l->offset + offsetof(PlayerVisualConfig, hair), visual.hair.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  hair_color        %04hX, %04hX, %04hX", l->offset + offsetof(PlayerVisualConfig, hair_r), visual.hair_r.load(), visual.hair_g.load(), visual.hair_b.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  proportion        %g, %g", l->offset + offsetof(PlayerVisualConfig, proportion_x), visual.proportion_x.load(), visual.proportion_y.load()));
      });
      print_as_struct.template operator()<Arg::DataType::PLAYER_STATS, PlayerStats>([&](const PlayerStats& stats) -> void {
        lines.emplace_back("  // As PlayerStats");
        lines.emplace_back(phosg::string_printf("  %04zX  atp               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.atp), stats.char_stats.atp.load(), stats.char_stats.atp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  mst               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.mst), stats.char_stats.mst.load(), stats.char_stats.mst.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  evp               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.evp), stats.char_stats.evp.load(), stats.char_stats.evp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  hp                %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.hp), stats.char_stats.hp.load(), stats.char_stats.hp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  dfp               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.dfp), stats.char_stats.dfp.load(), stats.char_stats.dfp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  ata               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.ata), stats.char_stats.ata.load(), stats.char_stats.ata.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  lck               %04hX /* %hu */", l->offset + offsetof(PlayerStats, char_stats.lck), stats.char_stats.lck.load(), stats.char_stats.lck.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  esp               %04hX /* %hu */", l->offset + offsetof(PlayerStats, esp), stats.esp.load(), stats.esp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  height            %08" PRIX32 " /* %g */", l->offset + offsetof(PlayerStats, height), stats.height.load_raw(), stats.height.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a3                %08" PRIX32 " /* %g */", l->offset + offsetof(PlayerStats, unknown_a3), stats.unknown_a3.load_raw(), stats.unknown_a3.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  level             %08" PRIX32 " /* level %" PRIu32 " */", l->offset + offsetof(PlayerStats, level), stats.level.load(), stats.level.load() + 1));
        lines.emplace_back(phosg::string_printf("  %04zX  experience        %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(PlayerStats, experience), stats.experience.load(), stats.experience.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  meseta            %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(PlayerStats, meseta), stats.meseta.load(), stats.meseta.load()));
      });
      print_as_struct.template operator()<Arg::DataType::RESIST_DATA, ResistData>([&](const ResistData& resist) -> void {
        lines.emplace_back("  // As ResistData");
        lines.emplace_back(phosg::string_printf("  %04zX  evp_bonus         %04hX /* %hu */", l->offset + offsetof(ResistData, evp_bonus), resist.evp_bonus.load(), resist.evp_bonus.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  efr               %04hX /* %hu */", l->offset + offsetof(ResistData, efr), resist.efr.load(), resist.efr.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  eic               %04hX /* %hu */", l->offset + offsetof(ResistData, eic), resist.eic.load(), resist.eic.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  eth               %04hX /* %hu */", l->offset + offsetof(ResistData, eth), resist.eth.load(), resist.eth.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  elt               %04hX /* %hu */", l->offset + offsetof(ResistData, elt), resist.elt.load(), resist.elt.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  edk               %04hX /* %hu */", l->offset + offsetof(ResistData, edk), resist.edk.load(), resist.edk.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a6                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(ResistData, unknown_a6), resist.unknown_a6.load(), resist.unknown_a6.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a7                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(ResistData, unknown_a7), resist.unknown_a7.load(), resist.unknown_a7.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a8                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(ResistData, unknown_a8), resist.unknown_a8.load(), resist.unknown_a8.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a9                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(ResistData, unknown_a9), resist.unknown_a9.load(), resist.unknown_a9.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  dfp_bonus         %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(ResistData, dfp_bonus), resist.dfp_bonus.load(), resist.dfp_bonus.load()));
      });
      print_as_struct.template operator()<Arg::DataType::ATTACK_DATA, AttackData>([&](const AttackData& attack) -> void {
        lines.emplace_back("  // As AttackData");
        lines.emplace_back(phosg::string_printf("  %04zX  a1                %04hX /* %hd */", l->offset + offsetof(AttackData, unknown_a1), attack.unknown_a1.load(), attack.unknown_a1.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  atp               %04hX /* %hd */", l->offset + offsetof(AttackData, atp), attack.atp.load(), attack.atp.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  ata_bonus         %04hX /* %hd */", l->offset + offsetof(AttackData, ata_bonus), attack.ata_bonus.load(), attack.ata_bonus.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a4                %04hX /* %hu */", l->offset + offsetof(AttackData, unknown_a4), attack.unknown_a4.load(), attack.unknown_a4.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  distance_x        %08" PRIX32 " /* %g */", l->offset + offsetof(AttackData, distance_x), attack.distance_x.load_raw(), attack.distance_x.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  angle_x           %08" PRIX32 " /* %" PRIu32 "/65536 */", l->offset + offsetof(AttackData, angle_x), attack.angle_x.load_raw(), attack.angle_x.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  distance_y        %08" PRIX32 " /* %g */", l->offset + offsetof(AttackData, distance_y), attack.distance_y.load_raw(), attack.distance_y.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a8                %04hX /* %hu */", l->offset + offsetof(AttackData, unknown_a8), attack.unknown_a8.load(), attack.unknown_a8.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a9                %04hX /* %hu */", l->offset + offsetof(AttackData, unknown_a9), attack.unknown_a9.load(), attack.unknown_a9.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a10               %04hX /* %hu */", l->offset + offsetof(AttackData, unknown_a10), attack.unknown_a10.load(), attack.unknown_a10.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a11               %04hX /* %hu */", l->offset + offsetof(AttackData, unknown_a11), attack.unknown_a11.load(), attack.unknown_a11.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a12               %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(AttackData, unknown_a12), attack.unknown_a12.load(), attack.unknown_a12.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a13               %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(AttackData, unknown_a13), attack.unknown_a13.load(), attack.unknown_a13.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a14               %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(AttackData, unknown_a14), attack.unknown_a14.load(), attack.unknown_a14.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a15               %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(AttackData, unknown_a15), attack.unknown_a15.load(), attack.unknown_a15.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a16               %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(AttackData, unknown_a16), attack.unknown_a16.load(), attack.unknown_a16.load()));
      });
      print_as_struct.template operator()<Arg::DataType::MOVEMENT_DATA, MovementData>([&](const MovementData& movement) -> void {
        lines.emplace_back("  // As MovementData");
        lines.emplace_back(phosg::string_printf("  %04zX  idle_move_speed   %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, idle_move_speed), movement.idle_move_speed.load_raw(), movement.idle_move_speed.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  idle_anim_speed   %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, idle_animation_speed), movement.idle_animation_speed.load_raw(), movement.idle_animation_speed.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  move_speed        %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, move_speed), movement.move_speed.load_raw(), movement.move_speed.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  animation_speed   %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, animation_speed), movement.animation_speed.load_raw(), movement.animation_speed.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a1                %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, unknown_a1), movement.unknown_a1.load_raw(), movement.unknown_a1.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a2                %08" PRIX32 " /* %g */", l->offset + offsetof(MovementData, unknown_a2), movement.unknown_a2.load_raw(), movement.unknown_a2.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a3                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a3), movement.unknown_a3.load(), movement.unknown_a3.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a4                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a4), movement.unknown_a4.load(), movement.unknown_a4.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a5                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a5), movement.unknown_a5.load(), movement.unknown_a5.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a6                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a6), movement.unknown_a6.load(), movement.unknown_a6.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a7                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a7), movement.unknown_a7.load(), movement.unknown_a7.load()));
        lines.emplace_back(phosg::string_printf("  %04zX  a8                %08" PRIX32 " /* %" PRIu32 " */", l->offset + offsetof(MovementData, unknown_a8), movement.unknown_a8.load(), movement.unknown_a8.load()));
      });
      if (l->has_data_type(Arg::DataType::IMAGE_DATA)) {
        const void* data = cmd_r.pgetv(l->offset, size);
        auto decompressed = prs_decompress_with_meta(data, size);
        lines.emplace_back(phosg::string_printf("  // As decompressed image data (0x%zX bytes)", decompressed.data.size()));
        lines.emplace_back(format_and_indent_data(decompressed.data.data(), decompressed.data.size(), 0));
        if (decompressed.input_bytes_used < size) {
          size_t compressed_end_offset = l->offset + decompressed.input_bytes_used;
          size_t remaining_size = size - decompressed.input_bytes_used;
          lines.emplace_back("  // Extra data after compressed data");
          lines.emplace_back(format_and_indent_data(cmd_r.pgetv(compressed_end_offset, remaining_size), remaining_size, compressed_end_offset));
        }
      }
      if (l->has_data_type(Arg::DataType::BEZIER_CONTROL_POINT_DATA)) {
        phosg::StringReader r = cmd_r.sub(l->offset, size);
        lines.emplace_back("  // As VectorXYZTF");
        while (r.remaining() >= sizeof(VectorXYZTF)) {
          size_t offset = l->offset + cmd_r.where();
          const auto& e = r.get<VectorXYZTF>();
          lines.emplace_back(phosg::string_printf("  %04zX  vector       x=%g, y=%g, z=%g, t=%g", offset, e.x.load(), e.y.load(), e.z.load(), e.t.load()));
        }
        if (r.remaining() > 0) {
          size_t struct_end_offset = l->offset + r.where();
          size_t remaining_size = r.remaining();
          lines.emplace_back("  // Extra data after structures");
          lines.emplace_back(format_and_indent_data(r.getv(remaining_size), remaining_size, struct_end_offset));
        }
      }
      if (l->has_data_type(Arg::DataType::SCRIPT)) {
        add_disassembly_lines(l->offset, size);
      }
    }
  }

  lines.emplace_back(); // Add a \n on the end
  return phosg::join(lines, "\n");
}

Episode find_quest_episode_from_script(const void* data, size_t size, Version version) {
  phosg::StringReader r(data, size);

  bool use_wstrs = false;
  size_t code_offset = 0;
  size_t function_table_offset = 0;
  Episode header_episode = Episode::NONE;
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return Episode::EP1;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      const auto& header = r.get<PSOQuestHeaderGC>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      header_episode = episode_for_quest_episode_number(header.episode);
      break;
    }
    case Version::BB_V4: {
      use_wstrs = true;
      const auto& header = r.get<PSOQuestHeaderBB>();
      code_offset = header.code_offset;
      function_table_offset = header.function_table_offset;
      header_episode = episode_for_quest_episode_number(header.episode);
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }

  unordered_set<Episode> found_episodes;

  try {
    const auto& opcodes = opcodes_for_version(version);
    // The set_episode opcode should always be in the first function (0)
    phosg::StringReader cmd_r = r.sub(code_offset + r.pget_u32l(function_table_offset));

    while (!cmd_r.eof()) {
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
        throw runtime_error(phosg::string_printf("unknown quest opcode %04hX", opcode));
      }

      if (def->flags & F_RET) {
        break;
      }

      if (!(def->flags & F_ARGS)) {
        for (const auto& arg : def->args) {
          using Type = QuestScriptOpcodeDefinition::Argument::Type;
          string dasm_arg;
          switch (arg.type) {
            case Type::LABEL16:
              cmd_r.skip(2);
              break;
            case Type::LABEL32:
              cmd_r.skip(4);
              break;
            case Type::LABEL16_SET:
              if (def->flags & F_PASS) {
                throw logic_error("LABEL16_SET cannot be pushed to arg stack");
              }
              cmd_r.skip(cmd_r.get_u8() * 2);
              break;
            case Type::REG:
              cmd_r.skip(1);
              break;
            case Type::REG_SET:
              if (def->flags & F_PASS) {
                throw logic_error("REG_SET cannot be pushed to arg stack");
              }
              cmd_r.skip(cmd_r.get_u8());
              break;
            case Type::REG_SET_FIXED:
              if (def->flags & F_PASS) {
                throw logic_error("REG_SET_FIXED cannot be pushed to arg stack");
              }
              cmd_r.skip(1);
              break;
            case Type::REG32_SET_FIXED:
              if (def->flags & F_PASS) {
                throw logic_error("REG32_SET_FIXED cannot be pushed to arg stack");
              }
              cmd_r.skip(4);
              break;
            case Type::INT8:
              cmd_r.skip(1);
              break;
            case Type::INT16:
              cmd_r.skip(2);
              break;
            case Type::INT32:
              if (def->flags & F_SET_EPISODE) {
                found_episodes.emplace(episode_for_quest_episode_number(cmd_r.get_u32l()));
              } else {
                cmd_r.skip(4);
              }
              break;
            case Type::FLOAT32:
              cmd_r.skip(4);
              break;
            case Type::CSTRING:
              if (use_wstrs) {
                for (uint16_t ch = cmd_r.get_u16l(); ch; ch = cmd_r.get_u16l()) {
                }
              } else {
                for (uint8_t ch = cmd_r.get_u8(); ch; ch = cmd_r.get_u8()) {
                }
              }
              break;
            default:
              throw logic_error("invalid argument type");
          }
        }
      }
    }
  } catch (const exception& e) {
    phosg::log_warning("Cannot determine episode from quest script (%s)", e.what());
  }

  if (found_episodes.size() > 1) {
    throw runtime_error("multiple episodes found");
  } else if (found_episodes.size() == 1) {
    return *found_episodes.begin();
  } else {
    return header_episode;
  }
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
      throw runtime_error(phosg::string_printf("invalid episode number %02hhX", episode_number));
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
      return phosg::string_printf("Register(%p, name=\"%s\", number=%hd)", this, this->name.c_str(), this->number);
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
        throw runtime_error(phosg::string_printf("register %s is assigned multiple numbers", reg->name.c_str()));
      }
    }

    if (!name.empty()) {
      if (reg->name.empty()) {
        reg->name = name;
        if (!this->named_regs.emplace(reg->name, reg).second) {
          throw runtime_error(phosg::string_printf("name %s is already assigned to a different register", reg->name.c_str()));
        }
      } else if (reg->name != name) {
        throw runtime_error(phosg::string_printf("register %hd is assigned multiple names", reg->number));
      }
    }

    return reg;
  }

  void assign_number(shared_ptr<Register> reg, uint8_t number) {
    if (reg->number < 0) {
      reg->number = number;
      if (this->numbered_regs.at(reg->number)) {
        throw logic_error(phosg::string_printf("register number %hd assigned multiple times", reg->number));
      }
      this->numbered_regs.at(reg->number) = reg;
    } else if (reg->number != static_cast<int16_t>(number)) {
      throw runtime_error(phosg::string_printf("assigning different register number %hhu over existing register number %hd", number, reg->number));
    }
  }

  void constrain(shared_ptr<Register> first_reg, shared_ptr<Register> second_reg) {
    if (!first_reg->next) {
      first_reg->next = second_reg;
    } else if (first_reg->next != second_reg) {
      throw runtime_error(phosg::string_printf("register %s must come after %s, but is already constrained to another register", second_reg->name.c_str(), first_reg->name.c_str()));
    }
    if (!second_reg->prev) {
      second_reg->prev = first_reg;
    } else if (second_reg->prev != first_reg) {
      throw runtime_error(phosg::string_printf("register %s must come before %s, but is already constrained to another register", first_reg->name.c_str(), second_reg->name.c_str()));
    }
    if ((first_reg->number >= 0) && (second_reg->number >= 0) && (first_reg->number != ((second_reg->number - 1) & 0xFF))) {
      throw runtime_error(phosg::string_printf("register %s must come before %s, but both registers already have non-consecutive numbers", first_reg->name.c_str(), second_reg->name.c_str()));
    }
  }

  void assign_all() {
    // TODO: Technically, we should assign the biggest blocks first to minimize
    // fragmentation. I am lazy and haven't implemented this yet.
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

      // No prev or next register is assigned; find an interval in the register
      // number space that fits this block of registers. The total number of
      // register numbers needed is (prev_delta - 1) + (next_delta - 1) + 1.
      size_t num_regs = prev_delta + next_delta - 1;
      this->assign_number(reg, (this->find_register_number_space(num_regs) + (prev_delta - 1)) & 0xFF);

      // We don't need to assign the prev and next registers; they should also
      // be in the unassigned set and will be assigned by the above logic
    }

    // At this point, all registers should be assigned
    for (const auto& it : this->named_regs) {
      if (it.second->number < 0) {
        throw logic_error(phosg::string_printf("register %s was not assigned", it.second->name.c_str()));
      }
    }
    for (size_t z = 0; z < 0x100; z++) {
      auto reg = this->numbered_regs[z];
      if (reg && (reg->number != static_cast<int16_t>(z))) {
        throw logic_error(phosg::string_printf("register %zu has incorrect number %hd", z, reg->number));
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

std::string assemble_quest_script(const std::string& text, const std::string& include_directory) {
  auto lines = phosg::split(text, '\n');

  // Strip comments and whitespace
  for (auto& line : lines) {
    size_t comment_start = line.find("/*");
    while (comment_start != string::npos) {
      size_t comment_end = line.find("*/", comment_start + 2);
      if (comment_end == string::npos) {
        throw runtime_error("unterminated inline comment");
      }
      line.erase(comment_start, comment_end + 2 - comment_start);
      comment_start = line.find("/*");
    }
    comment_start = line.find("//");
    if (comment_start != string::npos) {
      line.resize(comment_start);
    }
    phosg::strip_trailing_whitespace(line);
    phosg::strip_leading_whitespace(line);
  }

  // Collect metadata directives
  Version quest_version = Version::UNKNOWN;
  string quest_name;
  string quest_short_desc;
  string quest_long_desc;
  int64_t quest_num = -1;
  uint8_t quest_language = 1;
  Episode quest_episode = Episode::EP1;
  uint8_t quest_max_players = 4;
  bool quest_joinable = false;
  for (const auto& line : lines) {
    if (line.empty()) {
      continue;
    }
    if (line[0] == '.') {
      if (phosg::starts_with(line, ".version ")) {
        string name = line.substr(9);
        quest_version = phosg::enum_for_name<Version>(name.c_str());
      } else if (phosg::starts_with(line, ".name ")) {
        quest_name = phosg::parse_data_string(line.substr(6));
      } else if (phosg::starts_with(line, ".short_desc ")) {
        quest_short_desc = phosg::parse_data_string(line.substr(12));
      } else if (phosg::starts_with(line, ".long_desc ")) {
        quest_long_desc = phosg::parse_data_string(line.substr(11));
      } else if (phosg::starts_with(line, ".quest_num ")) {
        quest_num = stoul(line.substr(11), nullptr, 0);
      } else if (phosg::starts_with(line, ".language ")) {
        quest_language = stoul(line.substr(10), nullptr, 0);
      } else if (phosg::starts_with(line, ".episode ")) {
        quest_episode = episode_for_token_name(line.substr(9));
      } else if (phosg::starts_with(line, ".max_players ")) {
        quest_max_players = stoul(line.substr(12), nullptr, 0);
      } else if (phosg::starts_with(line, ".joinable ")) {
        quest_joinable = true;
      }
    }
  }
  if (quest_version == Version::PC_PATCH || quest_version == Version::BB_PATCH || quest_version == Version::UNKNOWN) {
    throw runtime_error(".version directive is missing or invalid");
  }
  if (quest_num < 0) {
    throw runtime_error(".quest_num directive is missing or invalid");
  }
  if (quest_name.empty()) {
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
  for (size_t line_num = 1; line_num <= lines.size(); line_num++) {
    const auto& line = lines[line_num - 1];
    if (phosg::ends_with(line, ":")) {
      auto label = make_shared<Label>();
      label->name = line.substr(0, line.size() - 1);
      size_t at_offset = label->name.find('@');
      if (at_offset != string::npos) {
        try {
          label->index = stoul(label->name.substr(at_offset + 1), nullptr, 0);
        } catch (const exception& e) {
          throw runtime_error(phosg::string_printf("(line %zu) invalid index in label (%s)", line_num, e.what()));
        }
        label->name.resize(at_offset);
        if (label->name == "start" && label->index != 0) {
          throw runtime_error("start label cannot have a nonzero label ID");
        }
      } else if (label->name == "start") {
        label->index = 0;
      }
      if (!labels_by_name.emplace(label->name, label).second) {
        throw runtime_error(phosg::string_printf("(line %zu) duplicate label name: %s", line_num, label->name.c_str()));
      }
      if (label->index >= 0) {
        auto index_emplace_ret = labels_by_index.emplace(label->index, label);
        if (label->index >= 0 && !index_emplace_ret.second) {
          throw runtime_error(phosg::string_printf("(line %zu) duplicate label index: %zd (0x%zX) from %s and %s", line_num, label->index, label->index, label->name.c_str(), index_emplace_ret.first->second->name.c_str()));
        }
      }
    }
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
  bool version_has_args = F_HAS_ARGS & v_flag(quest_version);
  const auto& opcodes = opcodes_by_name_for_version(quest_version);
  phosg::StringWriter code_w;
  for (size_t line_num = 1; line_num <= lines.size(); line_num++) {
    try {
      const auto& line = lines[line_num - 1];
      if (line.empty()) {
        continue;
      }

      if (phosg::ends_with(line, ":")) {
        size_t at_offset = line.find('@');
        string label_name = line.substr(0, (at_offset == string::npos) ? (line.size() - 1) : at_offset);
        labels_by_name.at(label_name)->offset = code_w.size();
        continue;
      }

      if (line[0] == '.') {
        if (phosg::starts_with(line, ".data ")) {
          code_w.write(phosg::parse_data_string(line.substr(6)));
        } else if (phosg::starts_with(line, ".zero ")) {
          size_t size = stoull(line.substr(6), nullptr, 0);
          code_w.extend_by(size, 0x00);
        } else if (phosg::starts_with(line, ".zero_until ")) {
          size_t size = stoull(line.substr(12), nullptr, 0);
          code_w.extend_to(size, 0x00);
        } else if (phosg::starts_with(line, ".align ")) {
          size_t alignment = stoull(line.substr(7), nullptr, 0);
          while (code_w.size() % alignment) {
            code_w.put_u8(0);
          }
        } else if (phosg::starts_with(line, ".include_bin ")) {
          string filename = line.substr(13);
          phosg::strip_whitespace(filename);
          code_w.write(phosg::load_file(include_directory + "/" + filename));
        } else if (phosg::starts_with(line, ".include_native ")) {
#ifdef HAVE_RESOURCE_FILE
          string filename = line.substr(16);
          phosg::strip_whitespace(filename);
          string native_text = phosg::load_file(include_directory + "/" + filename);
          string code;
          if (is_ppc(quest_version)) {
            code = std::move(ResourceDASM::PPC32Emulator::assemble(native_text).code);
          } else if (is_x86(quest_version)) {
            code = std::move(ResourceDASM::X86Emulator::assemble(native_text).code);
          } else if (is_sh4(quest_version)) {
            code = std::move(ResourceDASM::SH4Emulator::assemble(native_text).code);
          } else {
            throw runtime_error("unknown architecture");
          }
          code_w.write(code);
#else
          throw runtime_error("native code cannot be compiled; rebuild newserv with libresource_file");
#endif
        }
        continue;
      }

      auto line_tokens = phosg::split(line, ' ', 1);
      const auto& opcode_def = opcodes.at(phosg::tolower(line_tokens.at(0)));

      bool use_args = version_has_args && (opcode_def->flags & F_ARGS);
      if (!use_args) {
        if ((opcode_def->opcode & 0xFF00) == 0x0000) {
          code_w.put_u8(opcode_def->opcode);
        } else {
          code_w.put_u16b(opcode_def->opcode);
        }
      }

      if (opcode_def->args.empty()) {
        if (line_tokens.size() > 1) {
          throw runtime_error(phosg::string_printf("(line %zu) arguments not allowed for %s", line_num, opcode_def->name));
        }
        continue;
      }

      if (line_tokens.size() < 2) {
        throw runtime_error(phosg::string_printf("(line %zu) arguments required for %s", line_num, opcode_def->name));
      }
      phosg::strip_trailing_whitespace(line_tokens[1]);
      phosg::strip_leading_whitespace(line_tokens[1]);

      if (phosg::starts_with(line_tokens[1], "...")) {
        if (!use_args) {
          throw runtime_error(phosg::string_printf("(line %zu) \'...\' can only be used with F_ARGS opcodes", line_num));
        }

      } else { // Not "..."
        auto args = phosg::split_context(line_tokens[1], ',');
        if (args.size() != opcode_def->args.size()) {
          throw runtime_error(phosg::string_printf("(line %zu) incorrect argument count for %s", line_num, opcode_def->name));
        }

        for (size_t z = 0; z < args.size(); z++) {
          using Type = QuestScriptOpcodeDefinition::Argument::Type;

          string& arg = args[z];
          const auto& arg_def = opcode_def->args[z];
          phosg::strip_trailing_whitespace(arg);
          phosg::strip_leading_whitespace(arg);

          try {
            auto add_cstr = [&](const string& text, bool bin) -> void {
              switch (quest_version) {
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
                  code_w.write(bin ? text : (quest_language ? tt_utf8_to_8859(text) : tt_utf8_to_sega_sjis(text)));
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
                // If the corresponding argument is a REG or REG_SET_FIXED, push
                // the register number, not the register's value, since it's an
                // out-param
                if ((arg_def.type == Type::REG) || (arg_def.type == Type::REG32)) {
                  code_w.put_u8(0x4A); // arg_pushb
                  auto reg = parse_reg(arg);
                  reg->offsets.emplace(code_w.size());
                  code_w.put_u8(reg->number);
                } else if (
                    (arg_def.type == Type::REG_SET_FIXED) ||
                    (arg_def.type == Type::REG32_SET_FIXED)) {
                  auto regs = parse_reg_set_fixed(arg, arg_def.count);
                  code_w.put_u8(0x4A); // arg_pushb
                  regs[0]->offsets.emplace(code_w.size());
                  code_w.put_u8(regs[0]->number);
                } else {
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
                    if (phosg::starts_with(arg, "bin:")) {
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
                if (!labels_by_name.count(name)) {
                  throw runtime_error("label not defined: " + name);
                }
                if (is32) {
                  code_w.put_u32(labels_by_name.at(name)->index);
                } else {
                  code_w.put_u16(labels_by_name.at(name)->index);
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
                if (!phosg::starts_with(text, "[") || !phosg::ends_with(text, "]")) {
                  throw runtime_error("incorrect syntax for set-valued argument");
                }
                auto values = phosg::split(text.substr(1, text.size() - 2), ',');
                if (values.size() > 0xFF) {
                  throw runtime_error("too many labels in set-valued argument");
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
                case Type::REG:
                case Type::REG32:
                  add_reg(parse_reg(arg), arg_def.type == Type::REG32);
                  break;
                case Type::REG_SET_FIXED:
                case Type::REG32_SET_FIXED: {
                  auto regs = parse_reg_set_fixed(arg, arg_def.count);
                  add_reg(regs[0], arg_def.type == Type::REG32_SET_FIXED);
                  break;
                }
                case Type::REG_SET: {
                  auto regs = split_set(arg);
                  code_w.put_u8(regs.size());
                  for (auto reg_arg : regs) {
                    phosg::strip_trailing_whitespace(reg_arg);
                    phosg::strip_leading_whitespace(reg_arg);
                    add_reg(parse_reg(reg_arg), false);
                  }
                  break;
                }
                case Type::INT8:
                  code_w.put_u8(stol(arg, nullptr, 0));
                  break;
                case Type::INT16:
                  code_w.put_u16l(stol(arg, nullptr, 0));
                  break;
                case Type::INT32:
                  code_w.put_u32l(stol(arg, nullptr, 0));
                  break;
                case Type::FLOAT32:
                  code_w.put_u32l(stof(arg, nullptr));
                  break;
                case Type::CSTRING:
                  if (phosg::starts_with(arg, "bin:")) {
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
            throw runtime_error(phosg::string_printf("(arg %zu) %s", z + 1, e.what()));
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

    } catch (const exception& e) {
      throw runtime_error(phosg::string_printf("(line %zu) %s", line_num, e.what()));
    }
  }
  while (code_w.size() & 3) {
    code_w.put_u8(0);
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

  // Generate function table
  ssize_t function_table_size = labels_by_index.rbegin()->first + 1;
  vector<le_uint32_t> function_table;
  function_table.reserve(function_table_size);
  {
    auto it = labels_by_index.begin();
    for (ssize_t z = 0; z < function_table_size; z++) {
      if (it == labels_by_index.end()) {
        throw logic_error("function table size exceeds maximum function ID");
      } else if (it->first > z) {
        function_table.emplace_back(0xFFFFFFFF);
      } else if (it->first == z) {
        if (it->second->offset < 0) {
          throw runtime_error("label " + it->second->name + " does not have a valid offset");
        }
        function_table.emplace_back(it->second->offset);
        it++;
      } else if (it->first < z) {
        throw logic_error("missed label " + it->second->name + " when compiling function table");
      }
    }
  }

  // Generate header
  phosg::StringWriter w;
  switch (quest_version) {
    case Version::DC_NTE: {
      PSOQuestHeaderDCNTE header;
      header.code_offset = sizeof(header);
      header.function_table_offset = sizeof(header) + code_w.size();
      header.size = header.function_table_offset + function_table.size() * sizeof(function_table[0]);
      header.unused = 0;
      header.name.encode(quest_name, 0);
      w.put(header);
      break;
    }
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2: {
      PSOQuestHeaderDC header;
      header.code_offset = sizeof(header);
      header.function_table_offset = sizeof(header) + code_w.size();
      header.size = header.function_table_offset + function_table.size() * sizeof(function_table[0]);
      header.unused = 0;
      header.language = quest_language;
      header.unknown1 = 0;
      header.quest_number = quest_num;
      header.name.encode(quest_name, quest_language);
      header.short_description.encode(quest_short_desc, quest_language);
      header.long_description.encode(quest_long_desc, quest_language);
      w.put(header);
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      PSOQuestHeaderPC header;
      header.code_offset = sizeof(header);
      header.function_table_offset = sizeof(header) + code_w.size();
      header.size = header.function_table_offset + function_table.size() * sizeof(function_table[0]);
      header.unused = 0;
      header.language = quest_language;
      header.unknown1 = 0;
      header.quest_number = quest_num;
      header.name.encode(quest_name, quest_language);
      header.short_description.encode(quest_short_desc, quest_language);
      header.long_description.encode(quest_long_desc, quest_language);
      w.put(header);
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      PSOQuestHeaderGC header;
      header.code_offset = sizeof(header);
      header.function_table_offset = sizeof(header) + code_w.size();
      header.size = header.function_table_offset + function_table.size() * sizeof(function_table[0]);
      header.unused = 0;
      header.language = quest_language;
      header.unknown1 = 0;
      header.quest_number = quest_num;
      header.episode = (quest_episode == Episode::EP2) ? 1 : 0;
      header.name.encode(quest_name, quest_language);
      header.short_description.encode(quest_short_desc, quest_language);
      header.long_description.encode(quest_long_desc, quest_language);
      w.put(header);
      break;
    }
    case Version::BB_V4: {
      PSOQuestHeaderBB header;
      header.code_offset = sizeof(header);
      header.function_table_offset = sizeof(header) + code_w.size();
      header.size = header.function_table_offset + function_table.size() * sizeof(function_table[0]);
      header.unused = 0;
      header.quest_number = quest_num;
      header.unused2 = 0;
      if (quest_episode == Episode::EP4) {
        header.episode = 2;
      } else if (quest_episode == Episode::EP2) {
        header.episode = 1;
      } else {
        header.episode = 0;
      }
      header.max_players = quest_max_players;
      header.joinable = quest_joinable ? 1 : 0;
      header.unknown = 0;
      header.name.encode(quest_name, quest_language);
      header.short_description.encode(quest_short_desc, quest_language);
      header.long_description.encode(quest_long_desc, quest_language);
      w.put(header);
      break;
    }
    default:
      throw logic_error("invalid quest version");
  }
  w.write(code_w.str());
  w.write(function_table.data(), function_table.size() * sizeof(function_table[0]));
  return std::move(w.str());
}
