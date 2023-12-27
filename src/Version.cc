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
    case Version::DC_V1_11_2000_PROTOTYPE:
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
    case Version::DC_V1_11_2000_PROTOTYPE:
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
    case Version::DC_V1_11_2000_PROTOTYPE:
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
const char* name_for_enum<Version>(Version v) {
  switch (v) {
    case Version::PC_PATCH:
      return "PC_PATCH";
    case Version::BB_PATCH:
      return "BB_PATCH";
    case Version::DC_NTE:
      return "DC_NTE";
    case Version::DC_V1_11_2000_PROTOTYPE:
      return "DC_V1_11_2000_PROTOTYPE";
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
Version enum_for_name<Version>(const char* name) {
  if (!strcmp(name, "PC_PATCH") || !strcasecmp(name, "patch")) {
    return Version::PC_PATCH;
  } else if (!strcmp(name, "BB_PATCH")) {
    return Version::BB_PATCH;
  } else if (!strcmp(name, "DC_NTE")) {
    return Version::DC_NTE;
  } else if (!strcmp(name, "DC_V1_11_2000_PROTOTYPE")) {
    return Version::DC_V1_11_2000_PROTOTYPE;
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
const char* name_for_enum<ServerBehavior>(ServerBehavior behavior) {
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
ServerBehavior enum_for_name<ServerBehavior>(const char* name) {
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

uint32_t default_specific_version_for_version(Version version, int64_t sub_version) {
  // For versions that don't support send_function_call by default, we need
  // to set the specific_version based on sub_version. Fortunately, all
  // versions that share sub_version values also support send_function_call,
  // so for those versions we get the specific_version later by sending the
  // VersionDetect call.
  switch (version) {
    case Version::GC_NTE:
      return 0x334F4A54; // 3OJT
    case Version::GC_V3:
      switch (sub_version) {
        case 0x32: // GC Ep1&2 EU 50Hz
        case 0x33: // GC Ep1&2 EU 60Hz
          return 0x334F5030; // 3OP0
        case 0x36: // GC Ep1&2 US v1.02 (Plus)
          return 0x334F4532; // 3OE2
        case 0x39: // GC Ep1&2 JP v1.05 (Plus)
          return 0x334F4A35; // 3OJ5
        case 0x34: // GC Ep1&2 JP v1.03
          return 0x334F4A33; // 3OJ3
        case 0x35: // GC Ep1&2 JP v1.04 (Plus)
          return 0x334F4A34; // 3OJ4
        case -1: // Initial check (before sub_version recognition)
        case 0x30: // GC Ep1&2 GameJam demo, GC Ep1&2 Trial Edition, GC Ep1&2 JP v1.02, at least one version of PSO XB
        case 0x31: // GC Ep1&2 US v1.00, GC US v1.01, XB US
        default:
          return 0x33000000;
      }
      throw logic_error("this should be impossible");
    case Version::GC_EP3_NTE:
      return 0x33534A54; // 3SJT
    case Version::GC_EP3:
      switch (sub_version) {
        case 0x41: // GC Ep3 US
          return 0x33534530; // 3SE0
        case 0x42: // GC Ep3 EU 50Hz
        case 0x43: // GC Ep3 EU 60Hz
          return 0x33535030; // 3SP0
        case -1: // Initial check (before sub_version recognition)
        case 0x40: // GC Ep3 trial and GC Ep3 JP
        default:
          return 0x33000000;
      }
    default:
      return 0x00000000;
  }
}
