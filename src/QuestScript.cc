#include "QuestScript.hh"

#include <stdint.h>

#include <array>
#include <deque>
#include <map>
#include <phosg/Strings.hh>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;

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

struct QuestScriptOpcodeDefinition {
  struct Argument {
    enum class Type {
      FUNCTION16 = 0,
      FUNCTION16_SET,
      FUNCTION32,
      DATA_LABEL,
      REG,
      REG_SET,
      REG_SET_FIXED, // Sequence of N consecutive regs
      INT8,
      INT16,
      INT32,
      FLOAT32,
      CSTRING,
    };

    Type type;
    size_t count;
    const char* name;

    Argument(Type type, size_t count = 0, const char* name = nullptr)
        : type(type),
          count(count),
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

static constexpr auto V1 = QuestScriptOpcodeDefinition::Version::V1;
static constexpr auto V2 = QuestScriptOpcodeDefinition::Version::V2;
static constexpr auto V3 = QuestScriptOpcodeDefinition::Version::V3;
static constexpr auto V4 = QuestScriptOpcodeDefinition::Version::V4;

static constexpr auto FUNCTION16 = QuestScriptOpcodeDefinition::Argument::Type::FUNCTION16;
static constexpr auto FUNCTION16_SET = QuestScriptOpcodeDefinition::Argument::Type::FUNCTION16_SET;
static constexpr auto FUNCTION32 = QuestScriptOpcodeDefinition::Argument::Type::FUNCTION32;
static constexpr auto DATA_LABEL = QuestScriptOpcodeDefinition::Argument::Type::DATA_LABEL;
static constexpr auto REG = QuestScriptOpcodeDefinition::Argument::Type::REG;
static constexpr auto REG_SET = QuestScriptOpcodeDefinition::Argument::Type::REG_SET;
static constexpr auto REG_SET_FIXED = QuestScriptOpcodeDefinition::Argument::Type::REG_SET_FIXED;
static constexpr auto INT8 = QuestScriptOpcodeDefinition::Argument::Type::INT8;
static constexpr auto INT16 = QuestScriptOpcodeDefinition::Argument::Type::INT16;
static constexpr auto INT32 = QuestScriptOpcodeDefinition::Argument::Type::INT32;
static constexpr auto FLOAT32 = QuestScriptOpcodeDefinition::Argument::Type::FLOAT32;
static constexpr auto CSTRING = QuestScriptOpcodeDefinition::Argument::Type::CSTRING;
static constexpr uint8_t PRESERVE_ARG_STACK = QuestScriptOpcodeDefinition::Flag::PRESERVE_ARG_STACK;

static const QuestScriptOpcodeDefinition::Argument CLIENT_ID(INT32, 0, "client_id");
static const QuestScriptOpcodeDefinition::Argument ITEM_ID(INT32, 0, "item_id");
static const QuestScriptOpcodeDefinition::Argument AREA(INT32, 0, "area");

static const QuestScriptOpcodeDefinition opcode_defs[] = {
    {0x0000, "nop", {}, {}, V1, V4},
    {0x0001, "ret", {}, {}, V1, V4},
    {0x0002, "sync", {}, {}, V1, V4},
    {0x0003, "exit", {INT32}, {}, V1, V4},
    {0x0004, "thread", {FUNCTION16}, {}, V1, V4},
    {0x0005, "va_start", {}, {}, V3, V4},
    {0x0006, "va_end", {}, {}, V3, V4},
    {0x0007, "va_call", {FUNCTION16}, {}, V3, V4},
    {0x0008, "let", {REG, REG}, {}, V1, V4},
    {0x0009, "leti", {REG, INT32}, {}, V1, V4},
    {0x000A, "letb", {REG, INT16}, {}, V1, V2},
    {0x000A, "letb", {REG, INT8}, {}, V3, V4},
    {0x000B, "letw", {REG, INT16}, {}, V3, V4},
    {0x000C, "leta", {REG, REG}, {}, V3, V4},
    {0x000D, "leto", {REG, FUNCTION16}, {}, V3, V4},
    {0x0010, "set", {REG}, {}, V1, V4},
    {0x0011, "clear", {REG}, {}, V1, V4},
    {0x0012, "rev", {REG}, {}, V1, V4},
    {0x0013, "gset", {INT16}, {}, V1, V4},
    {0x0014, "gclear", {INT16}, {}, V1, V4},
    {0x0015, "grev", {INT16}, {}, V1, V4},
    {0x0016, "glet", {INT16, REG}, {}, V1, V4},
    {0x0017, "gget", {INT16, REG}, {}, V1, V4},
    {0x0018, "add", {REG, REG}, {}, V1, V4},
    {0x0019, "addi", {REG, INT32}, {}, V1, V4},
    {0x001A, "sub", {REG, REG}, {}, V1, V4},
    {0x001B, "subi", {REG, INT32}, {}, V1, V4},
    {0x001C, "mul", {REG, REG}, {}, V1, V4},
    {0x001D, "muli", {REG, INT32}, {}, V1, V4},
    {0x001E, "div", {REG, REG}, {}, V1, V4},
    {0x001F, "divi", {REG, INT32}, {}, V1, V4},
    {0x0020, "and", {REG, REG}, {}, V1, V4},
    {0x0021, "andi", {REG, INT32}, {}, V1, V4},
    {0x0022, "or", {REG, REG}, {}, V1, V4},
    {0x0023, "ori", {REG, INT32}, {}, V1, V4},
    {0x0024, "xor", {REG, REG}, {}, V1, V4},
    {0x0025, "xori", {REG, INT32}, {}, V1, V4},
    {0x0026, "mod", {REG, REG}, {}, V3, V4},
    {0x0027, "modi", {REG, INT32}, {}, V3, V4},
    {0x0028, "jmp", {FUNCTION16}, {}, V1, V4},
    {0x0029, "call", {FUNCTION16}, {}, V1, V4},
    {0x002A, "jmp_on", {FUNCTION16, REG_SET}, {}, V1, V4},
    {0x002B, "jmp_off", {FUNCTION16, REG_SET}, {}, V1, V4},
    {0x002C, "jmp_=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x002D, "jmpi_=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x002E, "jmp_!=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x002F, "jmpi_!=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0030, "ujmp_>", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x0031, "ujmpi_>", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0032, "jmp_>", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x0033, "jmpi_>", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0034, "ujmp_<", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x0035, "ujmpi_<", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0036, "jmp_<", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x0037, "jmpi_<", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0038, "ujmp_>=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x0039, "ujmpi_>=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x003A, "jmp_>=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x003B, "jmpi_>=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x003C, "ujmp_<=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x003D, "ujmpi_<=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x003E, "jmp_<=", {REG, REG, FUNCTION16}, {}, V1, V4},
    {0x003F, "jmpi_<=", {REG, INT32, FUNCTION16}, {}, V1, V4},
    {0x0040, "switch_jmp", {REG, FUNCTION16_SET}, {}, V1, V4},
    {0x0041, "switch_call", {REG, FUNCTION16_SET}, {}, V1, V4},
    {0x0042, "stack_push", {INT32}, {}, V1, V2},
    {0x0042, "stack_push", {REG}, {}, V3, V4},
    {0x0043, "stack_pop", {REG}, {}, V3, V4},
    {0x0044, "stack_pushm", {REG, INT32}, {}, V3, V4},
    {0x0045, "stack_popm", {REG, INT32}, {}, V3, V4},
    {0x0048, "arg_pushr", {REG}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x0049, "arg_pushl", {INT32}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x004A, "arg_pushb", {INT8}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x004B, "arg_pushw", {INT16}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x004C, "arg_pusha", {REG}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x004D, "arg_pusho", {FUNCTION16}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x004E, "arg_pushs", {CSTRING}, {}, V3, V4, PRESERVE_ARG_STACK},
    {0x0050, "message", {INT32, CSTRING}, {}, V1, V2},
    {0x0050, "message", {}, {INT32, CSTRING}, V3, V4},
    {0x0051, "list", {REG, CSTRING}, {}, V1, V2},
    {0x0051, "list", {}, {REG, CSTRING}, V3, V4},
    {0x0052, "fadein", {}, {}, V1, V4},
    {0x0053, "fadeout", {}, {}, V1, V4},
    {0x0054, "se", {INT32}, {}, V1, V2},
    {0x0054, "se", {}, {INT32}, V3, V4},
    {0x0055, "bgm", {INT32}, {}, V1, V2},
    {0x0055, "bgm", {}, {INT32}, V3, V4},
    {0x0056, nullptr, {}, {}, V1, V2},
    {0x0057, nullptr, {}, {}, V1, V2},
    {0x0058, "enable", {INT32}, {}, V1, V2},
    {0x0059, "disable", {INT32}, {}, V1, V2},
    {0x005A, "window_msg", {CSTRING}, {}, V1, V2},
    {0x005A, "window_msg", {}, {CSTRING}, V3, V4},
    {0x005B, "add_msg", {CSTRING}, {}, V1, V2},
    {0x005B, "add_msg", {}, {CSTRING}, V3, V4},
    {0x005C, "mesend", {}, {}, V1, V4},
    {0x005D, "gettime", {REG}, {}, V1, V4},
    {0x005E, "winend", {}, {}, V1, V4},
    {0x0060, "npc_crt", {INT32, INT32}, {}, V1, V2},
    {0x0060, "npc_crt", {}, {INT32, INT32}, V3, V4},
    {0x0061, "npc_stop", {INT32}, {}, V1, V2},
    {0x0061, "npc_stop", {}, {INT32}, V3, V4},
    {0x0062, "npc_play", {INT32}, {}, V1, V2},
    {0x0062, "npc_play", {}, {INT32}, V3, V4},
    {0x0063, "npc_kill", {INT32}, {}, V1, V2},
    {0x0063, "npc_kill", {}, {INT32}, V3, V4},
    {0x0064, "npc_nont", {}, {}, V1, V4},
    {0x0065, "npc_talk", {}, {}, V1, V4},
    {0x0066, "npc_crp", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x0066, "npc_crp", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x0068, "create_pipe", {INT32}, {}, V1, V2},
    {0x0068, "create_pipe", {}, {INT32}, V3, V4},
    {0x0069, "p_hpstat", {REG, CLIENT_ID}, {}, V1, V2},
    {0x0069, "p_hpstat", {}, {REG, CLIENT_ID}, V3, V4},
    {0x006A, "p_dead", {REG, CLIENT_ID}, {}, V1, V2},
    {0x006A, "p_dead", {}, {REG, CLIENT_ID}, V3, V4},
    {0x006B, "p_disablewarp", {}, {}, V1, V4},
    {0x006C, "p_enablewarp", {}, {}, V1, V4},
    {0x006D, "p_move", {REG, INT32}, {}, V1, V2},
    {0x006D, "p_move", {{REG_SET_FIXED, 5}}, {}, V3, V4},
    {0x006E, "p_look", {CLIENT_ID}, {}, V1, V2},
    {0x006E, "p_look", {}, {CLIENT_ID}, V3, V4},
    {0x0070, "p_action_disable", {}, {}, V1, V4},
    {0x0071, "p_action_enable", {}, {}, V1, V4},
    {0x0072, "disable_movement1", {CLIENT_ID}, {}, V1, V2},
    {0x0072, "disable_movement1", {}, {CLIENT_ID}, V3, V4},
    {0x0073, "enable_movement1", {CLIENT_ID}, {}, V1, V2},
    {0x0073, "enable_movement1", {}, {CLIENT_ID}, V3, V4},
    {0x0074, "p_noncol", {}, {}, V1, V4},
    {0x0075, "p_col", {}, {}, V1, V4},
    {0x0076, "p_setpos", {CLIENT_ID, {REG_SET_FIXED, 4}}, {}, V1, V2},
    {0x0076, "p_setpos", {}, {CLIENT_ID, {REG_SET_FIXED, 4}}, V3, V4},
    {0x0077, "p_return_guild", {}, {}, V1, V4},
    {0x0078, "p_talk_guild", {CLIENT_ID}, {}, V1, V2},
    {0x0078, "p_talk_guild", {}, {CLIENT_ID}, V3, V4},
    {0x0079, "npc_talk_pl", {INT32}, {}, V1, V2}, // REG?
    {0x0079, "npc_talk_pl", {REG}, {}, V3, V4},
    {0x007A, "npc_talk_kill", {INT32}, {}, V1, V2},
    {0x007A, "npc_talk_kill", {}, {INT32}, V3, V4},
    {0x007B, "npc_crtpk", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x007B, "npc_crtpk", {}, {INT32, INT32}, V3, V4},
    {0x007C, "npc_crppk", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x007C, "npc_crppk", {{REG_SET_FIXED, 7}}, {}, V3, V4},
    {0x007D, "npc_crptalk", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x007D, "npc_crptalk", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x007E, "p_look_at", {CLIENT_ID, CLIENT_ID}, {}, V1, V2},
    {0x007E, "p_look_at", {}, {CLIENT_ID, CLIENT_ID}, V3, V4},
    {0x007F, "npc_crp_id", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x007F, "npc_crp_id", {{REG_SET_FIXED, 7}}, {}, V3, V4},
    {0x0080, "cam_quake", {}, {}, V1, V4},
    {0x0081, "cam_adj", {}, {}, V1, V4},
    {0x0082, "cam_zmin", {}, {}, V1, V4},
    {0x0083, "cam_zmout", {}, {}, V1, V4},
    {0x0084, "cam_pan", {INT32, INT32}, {}, V1, V2}, // REG_SET_FIXED?
    {0x0084, "cam_pan", {{REG_SET_FIXED, 5}}, {}, V3, V4},
    {0x0085, "game_lev_super", {}, {}, V1, V4},
    {0x0086, "game_lev_reset", {}, {}, V1, V4},
    {0x0087, "pos_pipe", {INT32, INT32}, {}, V1, V2}, // REG_SET_FIXED?
    {0x0087, "pos_pipe", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0x0088, "if_zone_clear", {REG, {REG_SET_FIXED, 2}}, {}, V1, V4},
    {0x0089, "chk_ene_num", {REG}, {}, V1, V4},
    {0x008A, "unhide_obj", {{REG_SET_FIXED, 3}}, {}, V1, V4},
    {0x008B, "unhide_ene", {{REG_SET_FIXED, 4}}, {}, V1, V4},
    {0x008C, "at_coords_call", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008D, "at_coords_talk", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008E, "col_npcin", {{REG_SET_FIXED, 5}}, {}, V1, V4},
    {0x008F, "col_npcinr", {REG}, {}, V1, V4},
    {0x0090, "switch_on", {INT32}, {}, V1, V2},
    {0x0090, "switch_on", {}, {INT32}, V3, V4},
    {0x0091, "switch_off", {INT32}, {}, V1, V2},
    {0x0091, "switch_off", {}, {INT32}, V3, V4},
    {0x0092, "playbgm_epi", {INT32}, {}, V1, V2},
    {0x0092, "playbgm_epi", {}, {INT32}, V3, V4},
    {0x0093, "set_mainwarp", {INT32}, {}, V1, V2},
    {0x0093, "set_mainwarp", {}, {INT32}, V3, V4},
    {0x0094, "set_obj_param", {{REG_SET_FIXED, 6}, REG}, {}, V1, V4},
    {0x0095, "set_floor_handler", {AREA, FUNCTION32}, {}, V1, V2},
    {0x0095, "set_floor_handler", {}, {AREA, FUNCTION16}, V3, V4},
    {0x0096, "clr_floor_handler", {AREA}, {}, V1, V2},
    {0x0096, "clr_floor_handler", {}, {AREA}, V3, V4},
    {0x0097, "col_plinaw", {REG}, {}, V1, V4},
    {0x0098, "hud_hide", {}, {}, V1, V4},
    {0x0099, "hud_show", {}, {}, V1, V4},
    {0x009A, "cine_enable", {}, {}, V1, V4},
    {0x009B, "cine_disable", {}, {}, V1, V4},
    {0x00A0, nullptr, {INT32, CSTRING}, {}, V1, V2},
    {0x00A0, nullptr, {}, {INT32, CSTRING}, V3, V4},
    {0x00A1, "set_qt_failure", {INT32}, {}, V1, V2}, // FUNCTION16?
    {0x00A1, "set_qt_failure", {FUNCTION16}, {}, V3, V4},
    {0x00A2, "set_qt_success", {INT32}, {}, V1, V2}, // FUNCTION16?
    {0x00A2, "set_qt_success", {FUNCTION16}, {}, V3, V4},
    {0x00A3, "clr_qt_failure", {}, {}, V1, V4},
    {0x00A4, "clr_qt_success", {}, {}, V1, V4},
    {0x00A5, "set_qt_cancel", {INT32}, {}, V1, V2}, // FUNCTION16?
    {0x00A5, "set_qt_cancel", {FUNCTION16}, {}, V3, V4},
    {0x00A6, "clr_qt_cancel", {}, {}, V1, V4},
    {0x00A8, "pl_walk", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x00A8, "pl_walk", {REG}, {}, V3, V4},
    {0x00B0, "pl_add_meseta", {CLIENT_ID, INT32}, {}, V1, V2},
    {0x00B0, "pl_add_meseta", {}, {CLIENT_ID, INT32}, V3, V4},
    {0x00B1, "thread_stg", {FUNCTION16}, {}, V1, V4},
    {0x00B2, "del_obj_param", {REG}, {}, V1, V4},
    {0x00B3, "item_create", {REG, REG}, {}, V1, V4},
    {0x00B4, "item_create2", {REG, REG}, {}, V1, V4},
    {0x00B5, "item_delete", {REG, REG}, {}, V1, V4},
    {0x00B6, "item_delete2", {{REG_SET_FIXED, 3}, REG}, {}, V1, V4},
    {0x00B7, "item_check", {REG, REG}, {}, V1, V4},
    {0x00B8, "setevt", {INT32}, {}, V1, V2},
    {0x00B8, "setevt", {}, {INT32}, V3, V4},
    {0x00B9, "get_difflvl", {REG}, {}, V1, V4},
    {0x00BA, "set_qt_exit", {INT32}, {}, V1, V2}, // FUNCTION16?
    {0x00BA, "set_qt_exit", {FUNCTION16}, {}, V3, V4},
    {0x00BB, "clr_qt_exit", {}, {}, V1, V4},
    {0x00BC, nullptr, {CSTRING}, {}, V1, V4},
    {0x00C0, "particle", {INT32, INT32}, {}, V1, V2}, // REG?
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
    {0x00CB, "set_quest_board_handler", {INT32, FUNCTION32, CSTRING}, {}, V1, V2},
    {0x00CB, "set_quest_board_handler", {}, {INT32, FUNCTION16, CSTRING}, V3, V4},
    {0x00CC, "clear_quest_board_handler", {INT32}, {}, V1, V2},
    {0x00CC, "clear_quest_board_handler", {}, {INT32}, V3, V4},
    {0x00CD, "particle_id", {INT32, INT32}, {}, V1, V2}, // REG_SET_FIXED?
    {0x00CD, "particle_id", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0x00CE, "npc_crptalk_id", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x00CE, "npc_crptalk_id", {{REG_SET_FIXED, 7}}, {}, V3, V4},
    {0x00CF, "npc_lang_clean", {}, {}, V1, V4},
    {0x00D0, "pl_pkon", {}, {}, V1, V4},
    {0x00D1, "pl_chk_item2", {REG, REG}, {}, V1, V4},
    {0x00D2, "enable_mainmenu", {}, {}, V1, V4},
    {0x00D3, "disable_mainmenu", {}, {}, V1, V4},
    {0x00D4, "start_battlebgm", {}, {}, V1, V4},
    {0x00D5, "end_battlebgm", {}, {}, V1, V4},
    {0x00D6, "disp_msg_qb", {CSTRING}, {}, V1, V2},
    {0x00D6, "disp_msg_qb", {}, {CSTRING}, V3, V4},
    {0x00D7, "close_msg_qb", {}, {}, V1, V4},
    {0x00D8, "set_eventflag", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x00D8, "set_eventflag", {}, {INT32, INT32}, V3, V4},
    {0x00D9, "sync_register", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x00D9, "sync_register", {}, {INT32, INT32}, V3, V4},
    {0x00DA, "set_returnhunter", {}, {}, V1, V4},
    {0x00DB, "set_returncity", {}, {}, V1, V4},
    {0x00DC, "load_pvr", {}, {}, V1, V4},
    {0x00DD, "load_midi", {}, {}, V1, V4},
    {0x00DE, nullptr, {{REG_SET_FIXED, 6}, REG}, {}, V1, V4},
    {0x00DF, "npc_param", {INT32, INT32}, {}, V1, V2}, // REG?
    {0x00DF, "npc_param", {{REG_SET_FIXED, 14}, INT32}, {}, V3, V4},
    {0x00E0, "pad_dragon", {}, {}, V1, V4},
    {0x00E1, "clear_mainwarp", {INT32}, {}, V1, V2},
    {0x00E1, "clear_mainwarp", {}, {INT32}, V3, V4},
    {0x00E2, "pcam_param", {INT32}, {}, V1, V2}, // REG?
    {0x00E2, "pcam_param", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x00E3, "start_setevt", {INT32, INT32}, {}, V1, V2},
    {0x00E3, "start_setevt", {}, {INT32, INT32}, V3, V4},
    {0x00E4, "warp_on", {}, {}, V1, V4},
    {0x00E5, "warp_off", {}, {}, V1, V4},
    {0x00E6, "get_slotnumber", {REG}, {}, V1, V4},
    {0x00E7, "get_servernumber", {REG}, {}, V1, V4},
    {0x00E8, "set_eventflag2", {INT32, REG}, {}, V1, V2},
    {0x00E8, "set_eventflag2", {}, {INT32, REG}, V3, V4},
    {0x00E9, "res", {REG, REG}, {}, V1, V4},
    {0x00EA, nullptr, {REG, INT32}, {}, V1, V4}, // INT8?
    {0x00EB, "enable_bgmctrl", {INT32}, {}, V1, V2},
    {0x00EB, "enable_bgmctrl", {}, {INT32}, V3, V4},
    {0x00EC, "sw_send", {{REG_SET_FIXED, 3}}, {}, V1, V4},
    {0x00ED, "create_bgmctrl", {}, {}, V1, V4},
    {0x00EE, "pl_add_meseta2", {INT32}, {}, V1, V2},
    {0x00EE, "pl_add_meseta2", {}, {INT32}, V3, V4},
    {0x00EF, "sync_to_server", {INT32, INT32}, {}, V1, V2},
    {0x00EF, "sync_to_server", {}, {INT32, INT32}, V3, V4},
    {0x00F0, "send_regwork", {}, {}, V1, V2},
    {0x00F1, "leti_fixed_camera", {INT32}, {}, V2, V2}, // REG_SET_FIXED?
    {0x00F1, "leti_fixed_camera", {{REG_SET_FIXED, 6}}, {}, V3, V4},
    {0x00F2, "default_camera_pos1", {}, {}, V2, V4},
    {0xF800, nullptr, {}, {}, V2, V2},
    {0xF801, "set_chat_callback", {REG, CSTRING}, {}, V2, V2},
    {0xF801, "set_chat_callback", {}, {REG, CSTRING}, V3, V4},
    {0xF808, "get_difficulty_level2", {REG}, {}, V2, V4},
    {0xF809, "get_number_of_player1", {REG}, {}, V2, V4},
    {0xF80A, "get_coord_of_player", {{REG_SET_FIXED, 3}, REG}, {}, V2, V4},
    {0xF80B, "enable_map", {}, {}, V2, V4},
    {0xF80C, "disable_map", {}, {}, V2, V4},
    {0xF80D, "map_designate_ex", {{REG_SET_FIXED, 5}}, {}, V2, V4},
    {0xF80E, nullptr, {CLIENT_ID}, {}, V2, V2},
    {0xF80E, nullptr, {}, {CLIENT_ID}, V3, V4},
    {0xF80F, nullptr, {CLIENT_ID}, {}, V2, V2},
    {0xF80F, nullptr, {}, {CLIENT_ID}, V3, V4},
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
    {0xF817, nullptr, {INT32}, {}, V2, V2},
    {0xF817, nullptr, {}, {INT32}, V3, V4},
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
    {0xF821, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF822, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF823, "set_cmode_char_template", {INT32}, {}, V2, V2},
    {0xF823, "set_cmode_char_template", {}, {INT32}, V3, V4},
    {0xF824, "set_cmode_diff", {INT32}, {}, V2, V2},
    {0xF824, "set_cmode_diff", {}, {INT32}, V3, V4},
    {0xF825, "exp_multiplication", {REG}, {}, V2, V4},
    {0xF826, "exp_division", {REG}, {}, V2, V4},
    {0xF827, "get_user_is_dead", {REG}, {}, V2, V4},
    {0xF828, "go_floor", {REG, REG}, {}, V2, V4},
    {0xF829, "get_num_kills", {REG, REG}, {}, V2, V4},
    {0xF82A, "reset_kills", {REG}, {}, V2, V4},
    {0xF82B, "unlock_door2", {INT32, INT32}, {}, V2, V2},
    {0xF82B, "unlock_door2", {}, {INT32, INT32}, V3, V4},
    {0xF82C, "lock_door2", {INT32, INT32}, {}, V3, V4},
    {0xF82C, "lock_door2", {}, {INT32, INT32}, V2, V2},
    {0xF82D, "if_switch_not_pressed", {{REG_SET_FIXED, 2}}, {}, V2, V4},
    {0xF82E, "if_switch_pressed", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF830, "control_dragon", {REG}, {}, V2, V4},
    {0xF831, "release_dragon", {}, {}, V2, V4},
    {0xF838, "shrink", {REG}, {}, V2, V4},
    {0xF839, "unshrink", {REG}, {}, V2, V4},
    {0xF83A, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF83B, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF83C, "display_clock2", {REG}, {}, V2, V4},
    {0xF83D, nullptr, {INT32}, {}, V2, V2},
    {0xF83D, nullptr, {}, {INT32}, V3, V4},
    {0xF83E, "delete_area_title", {INT32}, {}, V2, V2},
    {0xF83E, "delete_area_title", {}, {INT32}, V3, V4},
    {0xF840, "load_npc_data", {}, {}, V2, V4},
    {0xF841, "get_npc_data", {DATA_LABEL}, {}, V2, V4},
    {0xF848, "give_damage_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF849, "take_damage_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84A, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84B, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84C, "kill_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84D, "death_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84E, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF84F, "enemy_death_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF850, "meseta_score", {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF851, nullptr, {{REG_SET_FIXED, 2}}, {}, V2, V4},
    {0xF852, nullptr, {INT32}, {}, V2, V2},
    {0xF852, nullptr, {}, {INT32}, V3, V4},
    {0xF853, "reverse_warps", {}, {}, V2, V4},
    {0xF854, "unreverse_warps", {}, {}, V2, V4},
    {0xF855, "set_ult_map", {}, {}, V2, V4},
    {0xF856, "unset_ult_map", {}, {}, V2, V4},
    {0xF857, "set_area_title", {CSTRING}, {}, V2, V2},
    {0xF857, "set_area_title", {}, {CSTRING}, V3, V4},
    {0xF858, "ba_show_self_traps", {}, {}, V2, V4},
    {0xF859, "ba_hide_self_traps", {}, {}, V2, V4},
    {0xF85A, "equip_item", {INT32}, {}, V2, V2}, // REG?
    {0xF85A, "equip_item", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF85B, "unequip_item", {CLIENT_ID, INT32}, {}, V2, V2}, // REG?
    {0xF85B, "unequip_item", {}, {CLIENT_ID, INT32}, V3, V4},
    {0xF85C, "qexit2", {INT32}, {}, V2, V4},
    {0xF85D, nullptr, {INT32}, {}, V2, V2},
    {0xF85D, nullptr, {}, {INT32}, V3, V4},
    {0xF85E, nullptr, {INT32}, {}, V2, V2},
    {0xF85E, nullptr, {}, {INT32}, V3, V4},
    {0xF85F, nullptr, {INT32}, {}, V2, V2},
    {0xF85F, nullptr, {}, {INT32}, V3, V4},
    {0xF860, nullptr, {}, {}, V2, V4},
    {0xF861, nullptr, {INT32}, {}, V2, V2},
    {0xF861, nullptr, {}, {INT32}, V3, V4},
    {0xF862, "give_s_rank_weapon", {INT32, INT32, CSTRING}, {}, V2, V2}, // [1] is REG?
    {0xF862, "give_s_rank_weapon", {}, {INT32, REG, CSTRING}, V3, V4},
    {0xF863, nullptr, {INT32}, {}, V2, V2}, // REG?
    {0xF863, nullptr, {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF864, "cmode_rank", {INT32, CSTRING}, {}, V2, V2},
    {0xF864, "cmode_rank", {}, {INT32, CSTRING}, V3, V4},
    {0xF865, "award_item_name", {}, {}, V2, V4},
    {0xF866, "award_item_select", {}, {}, V2, V4},
    {0xF867, "award_item_give_to", {REG}, {}, V2, V4},
    {0xF868, "set_cmode_rank", {REG, REG}, {}, V2, V4},
    {0xF869, "check_rank_time", {REG, REG}, {}, V2, V4},
    {0xF86A, "item_create_cmode", {REG, REG}, {}, V2, V4},
    {0xF86B, "ba_box_drops", {REG}, {}, V2, V4},
    {0xF86C, "award_item_ok", {REG}, {}, V2, V4},
    {0xF86D, "ba_set_trapself", {}, {}, V2, V4},
    {0xF86E, nullptr, {}, {}, V2, V4},
    {0xF86F, "ba_set_lives", {INT32}, {}, V2, V2},
    {0xF86F, "ba_set_lives", {}, {INT32}, V3, V4},
    {0xF870, "ba_set_tech_lvl", {INT32}, {}, V2, V2},
    {0xF870, "ba_set_tech_lvl", {}, {INT32}, V3, V4},
    {0xF871, "ba_set_lvl", {INT32}, {}, V2, V2},
    {0xF871, "ba_set_lvl", {}, {INT32}, V3, V4},
    {0xF872, "ba_set_time_limit", {INT32}, {}, V2, V2},
    {0xF872, "ba_set_time_limit", {}, {INT32}, V3, V4},
    {0xF873, "boss_is_dead", {REG}, {}, V2, V4},
    {0xF874, nullptr, {INT32, CSTRING}, {}, V2, V2},
    {0xF874, nullptr, {}, {INT32, CSTRING}, V3, V4},
    {0xF875, "enable_stealth_suit_effect", {REG}, {}, V2, V4},
    {0xF876, "disable_stealth_suit_effect", {REG}, {}, V2, V4},
    {0xF877, "enable_techs", {REG}, {}, V2, V4},
    {0xF878, "disable_techs", {REG}, {}, V2, V4},
    {0xF879, "get_gender", {REG, REG}, {}, V2, V4},
    {0xF87A, "get_chara_class", {REG, {REG_SET_FIXED, 2}}, {}, V2, V4},
    {0xF87B, "take_slot_meseta", {{REG_SET_FIXED, 2}, REG}, {}, V2, V4},
    {0xF87C, nullptr, {REG}, {}, V2, V4},
    {0xF87D, nullptr, {REG}, {}, V2, V4},
    {0xF87E, nullptr, {REG}, {}, V2, V4},
    {0xF87F, "read_guildcard_flag", {REG, REG}, {}, V2, V4},
    {0xF880, nullptr, {{REG_SET_FIXED, 3}}, {}, V2, V4},
    {0xF881, "get_pl_name", {REG}, {}, V2, V4},
    {0xF882, "get_pl_job", {REG}, {}, V2, V4},
    {0xF883, nullptr, {REG, REG}, {}, V2, V4},
    {0xF884, "set_eventflag16", {INT32, INT8}, {}, V2, V2}, // 16? (Doesn't match what we see in V3)
    {0xF884, "set_eventflag16", {}, {INT32, INT32}, V3, V4},
    {0xF885, "set_eventflag32", {INT32, INT8}, {}, V2, V2}, // 32?
    {0xF885, "set_eventflag32", {}, {INT32, INT8}, V3, V4},
    {0xF886, nullptr, {REG, REG}, {}, V2, V4}, // INT8?
    {0xF887, nullptr, {REG, REG}, {}, V2, V4}, // INT8?
    {0xF888, "ba_close_msg", {}, {}, V2, V4},
    {0xF889, nullptr, {}, {}, V2, V4},
    {0xF88A, "get_player_status", {REG, REG}, {}, V2, V4},
    {0xF88B, "send_mail", {REG, CSTRING}, {}, V2, V2},
    {0xF88B, "send_mail", {}, {REG, CSTRING}, V3, V4},
    {0xF88C, "get_game_version", {REG}, {}, V2, V4}, // TODO: or is this online_check?
    {0xF88D, "chl_set_timerecord", {REG}, {}, V2, V3},
    {0xF88D, "chl_set_timerecord", {REG, REG}, {}, V4, V4},
    {0xF88E, "chl_get_timerecord", {REG}, {}, V2, V4},
    {0xF88F, "set_cmode_grave_rates", {REG}, {}, V2, V4},
    {0xF890, nullptr, {}, {}, V2, V4},
    {0xF891, "load_enemy_data", {INT32}, {}, V2, V2},
    {0xF891, "load_enemy_data", {}, {INT32}, V3, V4},
    {0xF892, "get_physical_data", {INT16}, {}, V2, V4},
    {0xF893, "get_attack_data", {INT16}, {}, V2, V4},
    {0xF894, "get_resist_data", {INT16}, {}, V2, V4},
    {0xF895, "get_movement_data", {INT16}, {}, V2, V4},
    {0xF896, nullptr, {REG, REG}, {}, V2, V4}, // INT8?
    {0xF897, nullptr, {REG, REG}, {}, V2, V4}, // INT8?
    {0xF898, "shift_left", {REG, REG}, {}, V2, V4},
    {0xF899, "shift_right", {REG, REG}, {}, V2, V4},
    {0xF89A, "get_random", {{REG_SET_FIXED, 2}, REG}, {}, V2, V4},
    {0xF89B, "reset_map", {}, {}, V2, V4},
    {0xF89C, "disp_chl_retry_menu", {REG}, {}, V2, V4},
    {0xF89D, "chl_reverser", {}, {}, V2, V4},
    {0xF89E, nullptr, {INT32}, {}, V2, V2},
    {0xF89E, nullptr, {}, {INT32}, V3, V4},
    {0xF89F, nullptr, {REG}, {}, V2, V4},
    {0xF8A0, nullptr, {}, {}, V2, V4},
    {0xF8A1, nullptr, {}, {}, V2, V4},
    {0xF8A2, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF8A3, "init_online_key", {}, {}, V2, V4},
    {0xF8A4, "encrypt_gc_entry_auto", {REG}, {}, V2, V4},
    {0xF8A5, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF8A6, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF8A7, "set_shrink_size", {REG, REG}, {}, V2, V4},
    {0xF8A8, "death_tech_lvl_up2", {INT32}, {}, V2, V2},
    {0xF8A8, "death_tech_lvl_up2", {}, {INT32}, V3, V4},
    {0xF8A9, nullptr, {REG}, {}, V2, V4},
    {0xF8AA, "is_there_grave_message", {REG}, {}, V2, V4},
    {0xF8AB, "get_ba_record", {REG}, {}, V2, V4},
    {0xF8AC, "get_cmode_prize_rank", {REG}, {}, V2, V4},
    {0xF8AD, "get_number_of_player2", {REG}, {}, V2, V4},
    {0xF8AE, "party_has_name", {REG}, {}, V2, V4},
    {0xF8AF, "someone_has_spoken", {REG}, {}, V2, V4},
    {0xF8B0, "read1", {REG, REG}, {}, V2, V2},
    {0xF8B0, "read1", {}, {REG, INT32}, V3, V4},
    {0xF8B1, "read2", {REG, REG}, {}, V2, V2},
    {0xF8B1, "read2", {}, {REG, INT32}, V3, V4},
    {0xF8B2, "read4", {REG, REG}, {}, V2, V2},
    {0xF8B2, "read4", {}, {REG, INT32}, V3, V4},
    {0xF8B3, "write1", {INT32, REG}, {}, V2, V2},
    {0xF8B3, "write1", {}, {INT32, REG}, V3, V4},
    {0xF8B4, "write2", {INT32, REG}, {}, V2, V2},
    {0xF8B4, "write2", {}, {INT32, REG}, V3, V4},
    {0xF8B5, "write4", {INT32, REG}, {}, V2, V2},
    {0xF8B5, "write4", {}, {INT32, REG}, V3, V4},
    {0xF8B6, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF8B7, nullptr, {REG}, {}, V2, V4}, // INT8?
    {0xF8B8, nullptr, {}, {}, V2, V4},
    {0xF8B9, "chl_recovery", {}, {}, V2, V4},
    {0xF8BA, nullptr, {}, {}, V2, V4},
    {0xF8BB, nullptr, {REG}, {}, V2, V4},
    {0xF8BC, "set_episode", {INT32}, {}, V3, V4},
    {0xF8C0, "file_dl_req", {}, {INT32, CSTRING}, V3, V4},
    {0xF8C1, "get_dl_status", {REG}, {}, V3, V4},
    {0xF8C2, "gba_unknown4", {}, {}, V3, V4},
    {0xF8C3, "get_gba_state", {REG}, {}, V3, V4},
    {0xF8C4, nullptr, {REG}, {}, V3, V4},
    {0xF8C5, nullptr, {REG}, {}, V3, V4},
    {0xF8C6, "qexit", {}, {}, V3, V4},
    {0xF8C7, "use_animation", {REG, {REG_SET_FIXED, 2}}, {}, V3, V4},
    {0xF8C8, "stop_animation", {REG}, {}, V3, V4},
    {0xF8C9, "run_to_coord", {REG, REG}, {}, V3, V4},
    {0xF8CA, "set_slot_invincible", {REG, REG}, {}, V3, V4},
    {0xF8CB, nullptr, {REG}, {}, V3, V4},
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
    {0xF8DB, nullptr, {}, {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, FUNCTION16}, V3, V4},
    {0xF8DC, "npc_action_string", {REG, REG, DATA_LABEL}, {}, V3, V4},
    {0xF8DD, "get_pad_cond", {REG, REG}, {}, V3, V4},
    {0xF8DE, "get_button_cond", {REG, REG}, {}, V3, V4},
    {0xF8DF, "freeze_enemies", {}, {}, V3, V4},
    {0xF8E0, "unfreeze_enemies", {}, {}, V3, V4},
    {0xF8E1, "freeze_everything", {}, {}, V3, V4},
    {0xF8E2, "unfreeze_everything", {}, {}, V3, V4},
    {0xF8E3, "restore_hp", {REG}, {}, V3, V4},
    {0xF8E4, "restore_tp", {REG}, {}, V3, V4},
    {0xF8E5, "close_chat_bubble", {REG}, {}, V3, V4},
    {0xF8E6, "move_coords_object", {REG, REG}, {}, V3, V4},
    {0xF8E7, "at_coords_call_ex", {REG, REG}, {}, V3, V4},
    {0xF8E8, nullptr, {REG, REG}, {}, V3, V4},
    {0xF8E9, nullptr, {REG, REG}, {}, V3, V4},
    {0xF8EA, nullptr, {REG, REG}, {}, V3, V4},
    {0xF8EB, nullptr, {REG, REG}, {}, V3, V4},
    {0xF8EC, nullptr, {REG, REG}, {}, V3, V4},
    {0xF8ED, "animation_check", {REG, REG}, {}, V3, V4},
    {0xF8EE, "call_image_data", {}, {INT32, DATA_LABEL}, V3, V4},
    {0xF8EF, nullptr, {}, {}, V3, V4},
    {0xF8F0, "turn_off_bgm_p2", {}, {}, V3, V4},
    {0xF8F1, "turn_on_bgm_p2", {}, {}, V3, V4},
    {0xF8F2, nullptr, {}, {INT32, FLOAT32, FLOAT32, INT32, {REG_SET_FIXED, 4}, DATA_LABEL}, V3, V4},
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
    {0xF910, "get_unknown_count", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF911, "get_stackable_item_count", {{REG_SET_FIXED, 4}, REG}, {}, V3, V4},
    {0xF912, "freeze_and_hide_equip", {}, {}, V3, V4},
    {0xF913, "thaw_and_show_equip", {}, {}, V3, V4},
    {0xF914, "set_palettex_callback", {}, {CLIENT_ID, FUNCTION16}, V3, V4},
    {0xF915, "activate_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF916, "enable_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF917, "restore_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF918, "disable_palettex", {}, {CLIENT_ID}, V3, V4},
    {0xF919, "get_palettex_activated", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF91A, "get_unknown_palettex_status", {}, {CLIENT_ID, INT32, REG}, V3, V4},
    {0xF91B, "disable_movement2", {}, {CLIENT_ID}, V3, V4},
    {0xF91C, "enable_movement2", {}, {CLIENT_ID}, V3, V4},
    {0xF91D, "get_time_played", {REG}, {}, V3, V4},
    {0xF91E, "get_guildcard_total", {REG}, {}, V3, V4},
    {0xF91F, "get_slot_meseta", {REG}, {}, V3, V4},
    {0xF920, "get_player_level", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF921, "get_section_id", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF922, "get_player_hp", {}, {CLIENT_ID, {REG_SET_FIXED, 4}}, V3, V4},
    {0xF923, "get_floor_number", {}, {CLIENT_ID, {REG_SET_FIXED, 2}}, V3, V4},
    {0xF924, "get_coord_player_detect", {REG, REG}, {}, V3, V4},
    {0xF925, "read_global_flag", {}, {INT32, REG}, V3, V4},
    {0xF926, "write_global_flag", {}, {INT32, INT32}, V3, V4},
    {0xF927, nullptr, {REG, REG}, {}, V3, V4},
    {0xF928, "floor_player_detect", {{REG_SET_FIXED, 4}}, {}, V3, V4},
    {0xF929, "read_disk_file", {}, {CSTRING}, V3, V4},
    {0xF92A, "open_pack_select", {}, {}, V3, V4},
    {0xF92B, "item_select", {REG}, {}, V3, V4},
    {0xF92C, "get_item_id", {REG}, {}, V3, V4},
    {0xF92D, "color_change", {}, {INT32, INT32, INT32, INT32, INT32}, V3, V4},
    {0xF92E, "send_statistic", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V3, V4},
    {0xF92F, nullptr, {}, {INT32, INT32}, V3, V4},
    {0xF930, "chat_box", {}, {INT32, INT32, INT32, INT32, INT32, CSTRING}, V3, V4},
    {0xF931, "chat_bubble", {}, {INT32, CSTRING}, V3, V4},
    {0xF932, nullptr, {REG}, {}, V3, V4},
    {0xF933, nullptr, {REG}, {}, V3, V4},
    {0xF934, "scroll_text", {}, {INT32, INT32, INT32, INT32, INT32, FLOAT32, REG, CSTRING}, V3, V4},
    {0xF935, "gba_create_dl_graph", {}, {}, V3, V4},
    {0xF936, "gba_destroy_dl_graph", {}, {}, V3, V4},
    {0xF937, "gba_update_dl_graph", {}, {}, V3, V4},
    {0xF938, "add_damage_to", {}, {INT32, INT32}, V3, V4},
    {0xF939, "item_delete3", {}, {INT32}, V3, V4},
    {0xF93A, "get_item_info", {}, {ITEM_ID, {REG_SET_FIXED, 12}}, V3, V4},
    {0xF93B, "item_packing1", {}, {ITEM_ID}, V3, V4},
    {0xF93C, "item_packing2", {}, {ITEM_ID, INT32}, V3, V4},
    {0xF93D, "get_lang_setting", {}, {REG}, V3, V4},
    {0xF93E, "prepare_statistic", {}, {INT32, INT32, INT32}, V3, V4},
    {0xF93F, "keyword_detect", {}, {}, V3, V4},
    {0xF940, "keyword", {}, {REG, INT32, CSTRING}, V3, V4},
    {0xF941, "get_guildcard_num", {}, {CLIENT_ID, REG}, V3, V4},
    {0xF942, nullptr, {}, {INT32, {REG_SET_FIXED, 15}}, V3, V4},
    {0xF943, nullptr, {}, {}, V3, V4},
    {0xF944, "get_item_stackability", {}, {ITEM_ID, REG}, V3, V4},
    {0xF945, "initial_floor", {}, {INT32}, V3, V4},
    {0xF946, "sin", {}, {REG, INT32}, V3, V4},
    {0xF947, "cos", {}, {REG, INT32}, V3, V4},
    {0xF948, "tan", {}, {REG, INT32}, V3, V4},
    {0xF949, nullptr, {}, {REG, FLOAT32, FLOAT32}, V3, V4},
    {0xF94A, "boss_is_dead2", {REG}, {}, V3, V4},
    {0xF94B, "particle3", {REG}, {}, V3, V4},
    {0xF94C, nullptr, {REG}, {}, V3, V4},
    {0xF94D, "give_or_take_card", {{REG_SET_FIXED, 2}}, {}, V3, V3},
    {0xF94D, "nop2", {}, {}, V4, V4},
    {0xF94E, "nop3", {}, {}, V4, V4},
    {0xF94F, "nop4", {}, {}, V4, V4},
    {0xF950, "bb_p2_menu", {}, {INT32}, V4, V4},
    {0xF951, "bb_map_designate", {INT8, INT8, INT8, INT8, INT8}, {}, V4, V4},
    {0xF952, "bb_get_number_in_pack", {REG}, {}, V4, V4},
    {0xF953, "bb_swap_item", {}, {INT32, INT32, INT32, INT32, INT32, INT32, FUNCTION16, FUNCTION16}, V4, V4},
    {0xF954, "bb_check_wrap", {}, {INT32, REG}, V4, V4},
    {0xF955, "bb_exchange_pd_item", {}, {INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF956, "bb_exchange_pd_srank", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF957, "bb_exchange_pd_special", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF958, "bb_exchange_pd_percent", {}, {INT32, INT32, INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF959, nullptr, {}, {INT32}, V4, V4},
    {0xF95A, nullptr, {REG}, {}, V4, V4},
    {0xF95B, nullptr, {}, {INT32, INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF95C, "bb_exchange_slt", {}, {INT32, INT32, INT32, INT32}, V4, V4},
    {0xF95D, "bb_exchange_pc", {}, {}, V4, V4},
    {0xF95E, "bb_box_create_bp", {}, {INT32, INT32, INT32}, V4, V4},
    {0xF95F, "bb_exchange_pt", {}, {INT32, INT32, INT32, INT32, INT32}, V4, V4},
    {0xF960, nullptr, {}, {INT32}, V4, V4},
    {0xF961, nullptr, {REG}, {}, V4, V4},
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

  vector<le_uint32_t> function_table;
  StringReader function_table_r = r.sub(function_table_offset);
  while (!function_table_r.eof()) {
    try {
      function_table.emplace_back(function_table_r.get_u32l());
    } catch (const out_of_range&) {
      function_table_r.skip(function_table_r.remaining());
    }
  }

  map<size_t, string> dasm_lines;
  multimap<size_t, string> named_labels;
  named_labels.emplace(function_table.at(0), "start");
  set<size_t> unnamed_labels;

  StringReader cmd_r = r.sub(code_offset, function_table_offset - code_offset);
  while (!cmd_r.eof()) {
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
              case Type::FUNCTION16:
              case Type::FUNCTION32:
              case Type::DATA_LABEL: {
                uint32_t function_id = (arg.type == Type::FUNCTION32) ? cmd_r.get_u32l() : cmd_r.get_u16l();
                if (function_id >= function_table.size()) {
                  dasm_arg = string_printf("function%04" PRIX32 " /* invalid */", function_id);
                } else {
                  uint32_t label_offset = function_table.at(function_id);
                  dasm_arg = string_printf("function%04" PRIX32 " /* %04" PRIX32 " */", function_id, label_offset);
                }
                break;
              }
              case Type::FUNCTION16_SET: {
                uint8_t num_functions = cmd_r.get_u8();
                for (size_t z = 0; z < num_functions; z++) {
                  dasm_arg += (dasm_arg.empty() ? "(" : ",");
                  uint32_t function_id = cmd_r.get_u16l();
                  if (function_id >= function_table.size()) {
                    dasm_arg += string_printf("function%04" PRIX32 " /* invalid */", function_id);
                  } else {
                    uint32_t label_offset = function_table.at(function_id);
                    dasm_arg += string_printf("function%04" PRIX32 " /* %04" PRIX32 " */", function_id, label_offset);
                  }
                }
                dasm_arg += ")";
                break;
              }
              case Type::REG:
                dasm_arg = string_printf("r%hhu", cmd_r.get_u8());
                break;
              case Type::REG_SET: {
                uint8_t num_regs = cmd_r.get_u8();
                for (size_t z = 0; z < num_regs; z++) {
                  dasm_arg += string_printf("%cr%hhu", (dasm_arg.empty() ? '(' : ','), cmd_r.get_u8());
                }
                dasm_arg += ")";
                break;
              }
              case Type::REG_SET_FIXED: {
                uint8_t first_reg = cmd_r.get_u8();
                dasm_arg = string_printf("r%hhu-r%hhu", first_reg, static_cast<uint8_t>(first_reg + arg.count - 1));
                break;
              }
              case Type::INT8:
                dasm_arg = string_printf("0x%02hhX", cmd_r.get_u8());
                break;
              case Type::INT16:
                dasm_arg = string_printf("0x%04hX", cmd_r.get_u16l());
                break;
              case Type::INT32:
                dasm_arg = string_printf("0x%08" PRIX32, cmd_r.get_u32l());
                break;
              case Type::FLOAT32:
                dasm_arg = string_printf("%g", cmd_r.get_f32l());
                break;
              case Type::CSTRING:
                if (use_wstrs) {
                  u16string s;
                  for (char16_t ch = cmd_r.get_u16l(); ch; ch = cmd_r.get_u16l()) {
                    s.push_back(ch);
                  }
                  dasm_arg = dasm_u16string(s.data(), s.size());
                } else {
                  dasm_arg = format_data_string(cmd_r.get_cstr());
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
        string_printf("%04zX (+%04zX)  %s  %s", opcode_start_offset, opcode_start_offset + code_offset, hex_data.c_str(), dasm_line.c_str()));
  }

  multimap<size_t, size_t> function_labels;
  for (size_t z = 0; z < function_table.size(); z++) {
    size_t offset = function_table[z];
    if (offset != 0xFFFFFFFF) {
      function_labels.emplace(offset, z);
    }
  }

  auto dasm_line_it = dasm_lines.begin();
  auto named_label_it = named_labels.begin();
  auto unnamed_label_it = unnamed_labels.begin();
  auto function_label_it = function_labels.begin();
  size_t z = 0;
  while (
      (dasm_line_it != dasm_lines.end()) ||
      (named_label_it != named_labels.end()) ||
      (unnamed_label_it != unnamed_labels.end()) ||
      (function_label_it != function_labels.end())) {

    size_t next_z = static_cast<size_t>(-1);

    if (named_label_it != named_labels.end()) {
      if (named_label_it->first <= z && (next_z != z)) {
        lines.emplace_back(string_printf("%s:", named_label_it->second.c_str()));
        if ((dasm_line_it != dasm_lines.end()) && (dasm_line_it->first != named_label_it->first)) {
          lines.back() += string_printf(" /* misaligned; at %04zX */", named_label_it->first);
        }
        named_label_it++;
        next_z = z;
      } else {
        next_z = min<size_t>(next_z, named_label_it->first);
      }
    }

    if (unnamed_label_it != unnamed_labels.end()) {
      if (*unnamed_label_it <= z && (next_z != z)) {
        lines.emplace_back(string_printf("label%04zX:", *unnamed_label_it));
        if ((dasm_line_it != dasm_lines.end()) && (dasm_line_it->first != *unnamed_label_it)) {
          lines.back() += string_printf(" /* misaligned; at %04zX */", *unnamed_label_it);
        }
        unnamed_label_it++;
        next_z = z;
      } else {
        next_z = min<size_t>(next_z, *unnamed_label_it);
      }
    }

    if (function_label_it != function_labels.end()) {
      if (function_label_it->first <= z && (next_z != z)) {
        lines.emplace_back(string_printf("function%04zX:", function_label_it->second));
        if ((dasm_line_it != dasm_lines.end()) && (dasm_line_it->first != function_label_it->first)) {
          lines.back() += string_printf(" /* misaligned; at %04zX */", function_label_it->first);
        }
        function_label_it++;
        next_z = z;
      } else {
        next_z = min<size_t>(next_z, function_label_it->first);
      }
    }

    if (dasm_line_it != dasm_lines.end()) {
      if (dasm_line_it->first <= z && (next_z != z)) {
        lines.emplace_back(dasm_line_it->second);
        dasm_line_it++;
        next_z = z;
      } else {
        next_z = min<size_t>(next_z, dasm_line_it->first);
      }
    }

    z = next_z;
  }

  lines.emplace_back(); // Add a \n on the end
  return join(lines, "\n");
}
