#include "Version.hh"

#include <strings.h>

#include <stdexcept>

#include "Client.hh"

using namespace std;



uint16_t flags_for_version(GameVersion version, uint8_t sub_version) {
  switch (sub_version) {
    case 0x00: // initial check (before 9E recognition)
      switch (version) {
        case GameVersion::DC:
          return Client::Flag::DEFAULT_V2_DC;
        case GameVersion::GC:
          return Client::Flag::DEFAULT_V3_GC;
        case GameVersion::PC:
          return Client::Flag::DEFAULT_V2_PC;
        case GameVersion::PATCH:
          return Client::Flag::DEFAULT_V2_PC;
        case GameVersion::BB:
          return Client::Flag::DEFAULT_V4_BB;
      }
      break;
    case 0x29: // PSO PC
      return Client::Flag::DEFAULT_V2_PC;
    case 0x30: // ???
    case 0x31: // PSO Ep1&2 US10, US11, EU10, JP10
    case 0x33: // PSO Ep1&2 EU50HZ
    case 0x34: // PSO Ep1&2 JP11
      return Client::Flag::DEFAULT_V3_GC;
    // TODO: Which of these is the first version of JP PSO Plus? That version
    // supports encrypted send_function_call; we should set the appropriate flag
    // here.
    case 0x32: // PSO Ep1&2 US12, JP12
    case 0x35: // PSO Ep1&2 US12, JP12
    case 0x36: // PSO Ep1&2 US12, JP12
    case 0x39: // PSO Ep1&2 US12, JP12
      return Client::Flag::DEFAULT_V3_GC_PLUS_NO_SFC;
    case 0x40: // PSO Ep3 trial
    case 0x41: // PSO Ep3 US
    case 0x43: // PSO Ep3 UK
      return Client::Flag::DEFAULT_V3_GC_EP3_NO_SFC;
    case 0x42: // PSO Ep3 JP
      return Client::Flag::DEFAULT_V3_GC_PLUS;
  }
  return 0;
}

const char* name_for_version(GameVersion version) {
  switch (version) {
    case GameVersion::GC:
      return "GC";
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
    case ServerBehavior::SPLIT_RECONNECT:
      return "split_reconnect";
    case ServerBehavior::LOGIN_SERVER:
      return "login_server";
    case ServerBehavior::LOBBY_SERVER:
      return "lobby_server";
    case ServerBehavior::DATA_SERVER_BB:
      return "data_server_bb";
    case ServerBehavior::PATCH_SERVER:
      return "patch_server";
    case ServerBehavior::PROXY_SERVER:
      return "proxy_server";
    default:
      throw logic_error("invalid server behavior");
  }
}

ServerBehavior server_behavior_for_name(const char* name) {
  if (!strcasecmp(name, "split_reconnect")) {
    return ServerBehavior::SPLIT_RECONNECT;
  } else if (!strcasecmp(name, "login_server") || !strcasecmp(name, "login")) {
    return ServerBehavior::LOGIN_SERVER;
  } else if (!strcasecmp(name, "lobby_server") || !strcasecmp(name, "lobby")) {
    return ServerBehavior::LOBBY_SERVER;
  } else if (!strcasecmp(name, "data_server_bb") || !strcasecmp(name, "data_server") || !strcasecmp(name, "data")) {
    return ServerBehavior::DATA_SERVER_BB;
  } else if (!strcasecmp(name, "patch_server") || !strcasecmp(name, "patch")) {
    return ServerBehavior::PATCH_SERVER;
  } else if (!strcasecmp(name, "proxy_server") || !strcasecmp(name, "proxy")) {
    return ServerBehavior::PROXY_SERVER;
  } else {
    throw invalid_argument("incorrect server behavior name");
  }
}