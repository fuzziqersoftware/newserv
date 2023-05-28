#include "Version.hh"

#include <strings.h>

#include <stdexcept>

#include "Client.hh"

using namespace std;

const vector<string> version_to_login_port_name = {
    "bb-patch", "console-login", "pc-login", "console-login", "console-login", "bb-init"};
const vector<string> version_to_lobby_port_name = {
    "bb-patch", "console-lobby", "pc-lobby", "console-lobby", "console-lobby", "bb-lobby"};
const vector<string> version_to_proxy_port_name = {
    "", "dc-proxy", "pc-proxy", "gc-proxy", "xb-proxy", "bb-proxy"};

uint32_t flags_for_version(GameVersion version, int64_t sub_version) {
  switch (sub_version) {
    case -1: // Initial check (before sub_version recognition)
      switch (version) {
        case GameVersion::DC:
          return Client::Flag::NO_D6 |
              Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
        case GameVersion::GC:
          return 0;
        case GameVersion::XB:
          return Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
        case GameVersion::PC:
          return Client::Flag::NO_D6 |
              Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY |
              Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
        case GameVersion::PATCH:
          return Client::Flag::NO_D6 |
              Client::Flag::NO_SEND_FUNCTION_CALL;
        case GameVersion::BB:
          return Client::Flag::NO_D6 |
              Client::Flag::SAVE_ENABLED |
              Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
      }
      break;

    // TODO: Which other sub_versions of DC v1 and v2 exist?
    case 0x20: // DCNTE
               // In the case of DCNTE, the IS_TRIAL_EDITION flag is already set when we
               // get here, so the remaining flags are the same as DCv1
    case 0x21: // DCv1 US
      return Client::Flag::IS_DC_V1 |
          Client::Flag::NO_D6 |
          Client::Flag::NO_SEND_FUNCTION_CALL;
    case 0x23: // DCv1 EU?
      return Client::Flag::IS_DC_V1 |
          Client::Flag::NO_D6 |
          Client::Flag::NO_SEND_FUNCTION_CALL;

    case 0x26: // DCv2 US
      return Client::Flag::NO_D6 |
          Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;

    case 0x29: // PC
      return Client::Flag::NO_D6 |
          Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY |
          Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;

    case 0x30: // GC Ep1&2 JP v1.02, at least one version of PSO XB
    case 0x31: // GC Ep1&2 US v1.00, GC US v1.01, GC EU v1.00, GC JP v1.00
    case 0x34: // GC Ep1&2 JP v1.03
      return 0;
    case 0x32: // GC Ep1&2 EU 50Hz
    case 0x33: // GC Ep1&2 EU 60Hz
      return Client::Flag::NO_D6_AFTER_LOBBY;
    case 0x35: // GC Ep1&2 JP v1.04 (Plus)
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL;
    case 0x36: // GC Ep1&2 US v1.02 (Plus)
    case 0x39: // GC Ep1&2 JP v1.05 (Plus)
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::NO_SEND_FUNCTION_CALL;

    case 0x40: // GC Ep3 trial
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::IS_EPISODE_3 |
          Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL;
    case 0x42: // GC Ep3 JP
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::IS_EPISODE_3 |
          Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL;
    case 0x41: // GC Ep3 US
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::IS_EPISODE_3 |
          Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL |
          Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH;
    case 0x43: // GC Ep3 EU
      return Client::Flag::NO_D6_AFTER_LOBBY |
          Client::Flag::IS_EPISODE_3 |
          Client::Flag::NO_SEND_FUNCTION_CALL;
  }
  throw runtime_error(string_printf("unknown sub_version %" PRIX64, sub_version));
}

const char* name_for_version(GameVersion version) {
  switch (version) {
    case GameVersion::GC:
      return "GC";
    case GameVersion::XB:
      return "XB";
    case GameVersion::PC:
      return "PC";
    case GameVersion::BB:
      return "BB";
    case GameVersion::DC:
      return "DC";
    case GameVersion::PATCH:
      return "Patch";
    default:
      return "Unknown";
  }
}

GameVersion version_for_name(const char* name) {
  if (!strcasecmp(name, "DC") || !strcasecmp(name, "DreamCast")) {
    return GameVersion::DC;
  } else if (!strcasecmp(name, "PC")) {
    return GameVersion::PC;
  } else if (!strcasecmp(name, "GC") || !strcasecmp(name, "GameCube")) {
    return GameVersion::GC;
  } else if (!strcasecmp(name, "XB") || !strcasecmp(name, "Xbox")) {
    return GameVersion::XB;
  } else if (!strcasecmp(name, "BB") || !strcasecmp(name, "BlueBurst") ||
      !strcasecmp(name, "Blue Burst")) {
    return GameVersion::BB;
  } else if (!strcasecmp(name, "Patch")) {
    return GameVersion::PATCH;
  } else {
    throw invalid_argument("incorrect version name");
  }
}

const char* name_for_server_behavior(ServerBehavior behavior) {
  switch (behavior) {
    case ServerBehavior::PC_CONSOLE_DETECT:
      return "pc_console_detect";
    case ServerBehavior::LOGIN_SERVER:
      return "login_server";
    case ServerBehavior::LOBBY_SERVER:
      return "lobby_server";
    case ServerBehavior::DATA_SERVER_BB:
      return "data_server_bb";
    case ServerBehavior::PATCH_SERVER_PC:
      return "patch_server_pc";
    case ServerBehavior::PATCH_SERVER_BB:
      return "patch_server_bb";
    case ServerBehavior::PROXY_SERVER:
      return "proxy_server";
    default:
      throw logic_error("invalid server behavior");
  }
}

ServerBehavior server_behavior_for_name(const char* name) {
  if (!strcasecmp(name, "pc_console_detect")) {
    return ServerBehavior::PC_CONSOLE_DETECT;
  } else if (!strcasecmp(name, "login_server") || !strcasecmp(name, "login")) {
    return ServerBehavior::LOGIN_SERVER;
  } else if (!strcasecmp(name, "lobby_server") || !strcasecmp(name, "lobby")) {
    return ServerBehavior::LOBBY_SERVER;
  } else if (!strcasecmp(name, "data_server_bb") || !strcasecmp(name, "data_server") || !strcasecmp(name, "data")) {
    return ServerBehavior::DATA_SERVER_BB;
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

uint32_t default_specific_version_for_version(GameVersion version, int64_t sub_version) {
  uint32_t base_specific_version = (static_cast<uint32_t>(version) + '0') << 24;
  if (version == GameVersion::GC) {
    // For versions that don't support send_function_call by default, we need
    // to set the specific_version based on sub_version. Fortunately, all
    // versions that share sub_version values also support send_function_call,
    // so for those versions we get the specific_version later by sending the
    // VersionDetect call.
    switch (sub_version) {
      case 0x36: // GC Ep1&2 US v1.02 (Plus)
        return 0x334F4532; // 3OE2
      case 0x39: // GC Ep1&2 JP v1.05 (Plus)
        return 0x334F4A35; // 3OJ5
      case 0x41: // GC Ep3 US
        return 0x33534530; // 3SE0
      case 0x43: // GC Ep3 EU
        return 0x33535030; // 3SP0
      case -1: // Initial check (before sub_version recognition)
      case 0x30: // GC Ep1&2 JP v1.02, at least one version of PSO XB
      case 0x31: // GC Ep1&2 US v1.00, GC US v1.01, GC EU v1.00, GC JP v1.00
      case 0x32: // GC Ep1&2 EU 50Hz
      case 0x33: // GC Ep1&2 EU 60Hz
      case 0x34: // GC Ep1&2 JP v1.03
      case 0x35: // GC Ep1&2 JP v1.04 (Plus)
      case 0x40: // GC Ep3 trial
      case 0x42: // GC Ep3 JP
      default:
        return base_specific_version;
    }
  } else {
    return base_specific_version;
  }
}
