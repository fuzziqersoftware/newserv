#include "Version.hh"

#include <strings.h>

#include <stdexcept>

#include "Client.hh"

using namespace std;

const char* login_port_name_for_version(Version v) {
  switch (v) {
    case Version::PC_PATCH:
      return "pc-patch";
    case Version::BB_PATCH:
      return "bb-patch";
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return "console-login";
    case Version::PC_NTE:
    case Version::PC_V2:
      return "pc-login";
    case Version::XB_V3:
      return "xb-login";
    case Version::BB_V4:
      return "bb-init";
    default:
      throw runtime_error("unknown version");
  }
}

const char* lobby_port_name_for_version(Version v) {
  switch (v) {
    case Version::PC_PATCH:
      return "pc-patch";
    case Version::BB_PATCH:
      return "bb-patch";
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return "console-lobby";
    case Version::PC_NTE:
    case Version::PC_V2:
      return "pc-lobby";
    case Version::XB_V3:
      return "xb-lobby";
    case Version::BB_V4:
      return "bb-lobby";
    default:
      throw runtime_error("unknown version");
  }
}

const char* proxy_port_name_for_version(Version v) {
  switch (v) {
    case Version::PC_PATCH:
      return "pc-patch";
    case Version::BB_PATCH:
      return "bb-patch";
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
      return "dc-proxy";
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return "gc-proxy";
    case Version::PC_NTE:
    case Version::PC_V2:
      return "pc-proxy";
    case Version::XB_V3:
      return "xb-proxy";
    case Version::BB_V4:
      return "bb-proxy";
    default:
      throw runtime_error("unknown version");
  }
}

template <>
const char* phosg::name_for_enum<Version>(Version v) {
  switch (v) {
    case Version::PC_PATCH:
      return "PC_PATCH";
    case Version::BB_PATCH:
      return "BB_PATCH";
    case Version::DC_NTE:
      return "DC_NTE";
    case Version::DC_11_2000:
      return "DC_11_2000";
    case Version::DC_V1:
      return "DC_V1";
    case Version::DC_V2:
      return "DC_V2";
    case Version::PC_NTE:
      return "PC_NTE";
    case Version::PC_V2:
      return "PC_V2";
    case Version::GC_NTE:
      return "GC_NTE";
    case Version::GC_V3:
      return "GC_V3";
    case Version::GC_EP3_NTE:
      return "GC_EP3_NTE";
    case Version::GC_EP3:
      return "GC_EP3";
    case Version::XB_V3:
      return "XB_V3";
    case Version::BB_V4:
      return "BB_V4";
    default:
      throw runtime_error("unknown version");
  }
}

template <>
Version phosg::enum_for_name<Version>(const char* name) {
  if (!strcmp(name, "PC_PATCH") || !strcasecmp(name, "patch")) {
    return Version::PC_PATCH;
  } else if (!strcmp(name, "BB_PATCH")) {
    return Version::BB_PATCH;
  } else if (!strcmp(name, "DC_NTE")) {
    return Version::DC_NTE;
  } else if (!strcmp(name, "DC_11_2000")) {
    return Version::DC_11_2000;
  } else if (!strcmp(name, "DC_V1")) {
    return Version::DC_V1;
  } else if (!strcmp(name, "DC_V2") || !strcasecmp(name, "dc")) {
    return Version::DC_V2;
  } else if (!strcmp(name, "PC_NTE")) {
    return Version::PC_NTE;
  } else if (!strcmp(name, "PC_V2") || !strcasecmp(name, "pc")) {
    return Version::PC_V2;
  } else if (!strcmp(name, "GC_NTE")) {
    return Version::GC_NTE;
  } else if (!strcmp(name, "GC_V3") || !strcasecmp(name, "gc")) {
    return Version::GC_V3;
  } else if (!strcmp(name, "GC_EP3_NTE")) {
    return Version::GC_EP3_NTE;
  } else if (!strcmp(name, "GC_EP3")) {
    return Version::GC_EP3;
  } else if (!strcmp(name, "XB_V3") || !strcasecmp(name, "xb")) {
    return Version::XB_V3;
  } else if (!strcmp(name, "BB_V4") || !strcasecmp(name, "bb")) {
    return Version::BB_V4;
  } else {
    throw invalid_argument("incorrect version name");
  }
}

template <>
const char* phosg::name_for_enum<ServerBehavior>(ServerBehavior behavior) {
  switch (behavior) {
    case ServerBehavior::PC_CONSOLE_DETECT:
      return "pc_console_detect";
    case ServerBehavior::LOGIN_SERVER:
      return "login_server";
    case ServerBehavior::LOBBY_SERVER:
      return "lobby_server";
    case ServerBehavior::PATCH_SERVER_PC:
      return "patch_server_pc";
    case ServerBehavior::PATCH_SERVER_BB:
      return "patch_server_bb";
    case ServerBehavior::PROXY_SERVER:
      return "proxy_server";
  }
  throw logic_error("invalid server behavior");
}

template <>
ServerBehavior phosg::enum_for_name<ServerBehavior>(const char* name) {
  if (!strcasecmp(name, "pc_console_detect")) {
    return ServerBehavior::PC_CONSOLE_DETECT;
  } else if (!strcasecmp(name, "login_server") || !strcasecmp(name, "login") || !strcasecmp(name, "data_server_bb")) {
    return ServerBehavior::LOGIN_SERVER;
  } else if (!strcasecmp(name, "lobby_server") || !strcasecmp(name, "lobby")) {
    return ServerBehavior::LOBBY_SERVER;
  } else if (!strcasecmp(name, "patch_server_pc") || !strcasecmp(name, "patch_pc")) {
    return ServerBehavior::PATCH_SERVER_PC;
  } else if (!strcasecmp(name, "patch_server_bb") || !strcasecmp(name, "patch_bb")) {
    return ServerBehavior::PATCH_SERVER_BB;
  } else if (!strcasecmp(name, "proxy_server") || !strcasecmp(name, "proxy")) {
    return ServerBehavior::PROXY_SERVER;
  } else {
    throw invalid_argument("incorrect server behavior name");
  }
}

uint32_t default_sub_version_for_version(Version version) {
  switch (version) {
    case Version::DC_NTE:
      return 0x20;
    case Version::DC_11_2000:
      return 0x21;
    case Version::DC_V1:
      return 0x21;
    case Version::DC_V2:
      return 0x26;
    case Version::PC_NTE:
      return 0x28;
    case Version::PC_V2:
      return 0x29;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::XB_V3:
      return 0x30;
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return 0x40;
    case Version::BB_V4:
      return 0x41;
    default:
      return 0x00;
  }
}

uint32_t default_specific_version_for_version(Version version, int64_t sub_version) {
  // For versions that don't support send_function_call by default, we need
  // to set the specific_version based on sub_version. Fortunately, all
  // versions that share sub_version values also support send_function_call,
  // so for those versions we get the specific_version later by sending the
  // VersionDetectDC, VersionDetectGC, or VersionDetectXB call.
  switch (version) {
    case Version::DC_NTE:
      return SPECIFIC_VERSION_DC_NTE; // 1OJ1 (NTE)
    case Version::DC_11_2000:
      return SPECIFIC_VERSION_DC_11_2000_PROTOTYPE; // 1OJ2 (11/2000)
    case Version::DC_V1:
      switch (sub_version) {
        case 0x20:
          return SPECIFIC_VERSION_DC_V1_JP; // 1OJF (1OJ1 and 1OJ2 use 0x20 as well, but are detected without using sub_version)
        case 0x21:
          return SPECIFIC_VERSION_DC_V1_US; // 1OEF
        case 0x22:
        case 0x23:
          return SPECIFIC_VERSION_DC_V1_EU_INDETERMINATE; // 1OPF, 10J3 (12/2000), or 1OJ4 (01/2001)
        default:
          return SPECIFIC_VERSION_DC_V1_INDETERMINATE;
      }
    case Version::DC_V2:
      return SPECIFIC_VERSION_DC_V2_INDETERMINATE; // 2___; need to send VersionDetectDC
    case Version::PC_V2:
      return SPECIFIC_VERSION_PC_V2; // 2OJW
    case Version::GC_NTE:
      return SPECIFIC_VERSION_GC_NTE; // 3OJT
    case Version::GC_V3:
      switch (sub_version) {
        case 0x32: // GC Ep1&2 EU 50Hz
        case 0x33: // GC Ep1&2 EU 60Hz
          return SPECIFIC_VERSION_GC_V3_EU; // 3OP0
        case 0x36: // GC Ep1&2 US v1.2 (Plus)
        case 0x3A: // GC Ep1&2 US v1.2 (Plus) GMK edition
          return SPECIFIC_VERSION_GC_V3_US_12; // 3OE2
        case 0x34: // GC Ep1&2 JP v1.3
          return SPECIFIC_VERSION_GC_V3_JP_13; // 3OJ3
        case 0x35: // GC Ep1&2 JP v1.4 (Plus)
          return SPECIFIC_VERSION_GC_V3_JP_14; // 3OJ4
        case 0x39: // GC Ep1&2 JP v1.5 (Plus)
          return SPECIFIC_VERSION_GC_V3_JP_15; // 3OJ5
        case -1: // Initial check (before sub_version recognition)
        case 0x30: // GC Ep1&2 GameJam demo, GC Ep1&2 Trial Edition, GC Ep1&2 JP v1.2, at least one version of PSO XB
        case 0x31: // GC Ep1&2 US v1.0, GC US v1.1, XB US
        default:
          return SPECIFIC_VERSION_GC_V3_INDETERMINATE; // 3O__; need to send VersionDetectGC
      }
      throw logic_error("this should be impossible");
    case Version::GC_EP3_NTE:
      return SPECIFIC_VERSION_GC_EP3_NTE; // 3SJT
    case Version::GC_EP3:
      switch (sub_version) {
        case 0x41: // GC Ep3 US
          return SPECIFIC_VERSION_GC_EP3_US; // 3SE0
        case 0x42: // GC Ep3 EU 50Hz
        case 0x43: // GC Ep3 EU 60Hz
          return SPECIFIC_VERSION_GC_EP3_EU; // 3SP0
        case -1: // Initial check (before sub_version recognition)
        case 0x40: // GC Ep3 trial and GC Ep3 JP
        default:
          return SPECIFIC_VERSION_GC_EP3_INDETERMINATE; // 3SJ_; need to send VersionDetectGC
      }
    case Version::XB_V3:
      return SPECIFIC_VERSION_XB_V3_INDETERMINATE; // 4O__; need to send VersionDetectXB
    case Version::BB_V4:
      return SPECIFIC_VERSION_BB_V4_INDETERMINATE; // 5___; we should be able to determine version from initial login
    default:
      return SPECIFIC_VERSION_INDETERMINATE;
  }
}

bool specific_version_is_indeterminate(uint32_t specific_version) {
  return ((specific_version & 0x000000FF) == 0);
}

bool specific_version_is_dc(uint32_t specific_version) {
  // All v1 and v2 specific_versions are DC except 324F4A57 (2OJW), which is PC
  uint8_t major_version = specific_version >> 24;
  if (major_version < 0x31 || major_version > 0x32) {
    return false;
  }
  return (specific_version != 0x324F4A57);
}

bool specific_version_is_gc(uint32_t specific_version) {
  // GC specific_versions are 3GRV, where G is [OS], R is [JEP], V is [0-9T]
  if ((specific_version & 0xFF000000) != 0x33000000) {
    return false;
  }
  char game = specific_version >> 16;
  if ((game != 'O') && (game != 'S')) {
    return false;
  }
  char region = specific_version >> 8;
  if ((region != 'J') && (region != 'E') && (region != 'P')) {
    return false;
  }
  char revision = specific_version;
  return (isdigit(revision) || (revision == 'T'));
}

bool specific_version_is_xb(uint32_t specific_version) {
  // XB specific_versions are 4ORV, where R is [JEP], V is [BDU]
  if ((specific_version & 0xFFFF0000) != 0x344F0000) {
    return false;
  }
  char region = specific_version >> 8;
  if ((region != 'J') && (region != 'E') && (region != 'P')) {
    return false;
  }
  char revision = specific_version;
  return ((revision == 'B') || (revision == 'D') || (revision == 'U'));
}

bool specific_version_is_bb(uint32_t specific_version) {
  // BB specific_versions are 5XXX, where X is an encoding of the revision number
  return (specific_version & 0xFF000000) == 0x35000000;
}

const char* file_path_token_for_version(Version version) {
  switch (version) {
    case Version::PC_PATCH:
      return "pc-patch";
    case Version::BB_PATCH:
      return "bb-patch";
    case Version::DC_NTE:
      return "dc-nte";
    case Version::DC_11_2000:
      return "dc-11-2000";
    case Version::DC_V1:
      return "dc-v1";
    case Version::DC_V2:
      return "dc-v2";
    case Version::PC_NTE:
      return "pc-nte";
    case Version::PC_V2:
      return "pc-v2";
    case Version::GC_NTE:
      return "gc-nte";
    case Version::GC_V3:
      return "gc-v3";
    case Version::GC_EP3_NTE:
      return "gc-ep3-nte";
    case Version::GC_EP3:
      return "gc-ep3";
    case Version::XB_V3:
      return "xb-v3";
    case Version::BB_V4:
      return "bb-v4";
    default:
      throw runtime_error("invalid game version");
  }
}

uint64_t generate_random_hardware_id(Version version) {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      return phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    case Version::PC_NTE:
    case Version::PC_V2:
      return 0x0000FFFFFFFFFFFF;
    case Version::GC_NTE:
      // On GC NTE, the low byte is uninitialized memory from the TProtocol
      // constructor's stack
      return phosg::random_object<uint8_t>();
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
    case Version::BB_V4:
      return 0;
    default:
      throw runtime_error("invalid game version");
  }
}
