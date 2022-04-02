#include "Version.hh"

#include <stdexcept>

#include <strings.h>

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
    case 0x32: // PSO Ep1&2 US12, JP12
    case 0x35: // PSO Ep1&2 US12, JP12
    case 0x36: // PSO Ep1&2 US12, JP12
    case 0x39: // PSO Ep1&2 US12, JP12
      return Client::Flag::DEFAULT_V3_GC_PLUS;
    case 0x40: // PSO Ep3 trial
    case 0x41: // PSO Ep3 US
    case 0x42: // PSO Ep3 JP
    case 0x43: // PSO Ep3 UK
      return Client::Flag::DEFAULT_V3_GC_EP3;
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
  }
  if (!strcasecmp(name, "PC")) {
    return GameVersion::PC;
  }
  if (!strcasecmp(name, "GC") || !strcasecmp(name, "GameCube")) {
    return GameVersion::GC;
  }
  if (!strcasecmp(name, "BB") || !strcasecmp(name, "BlueBurst") ||
      !strcasecmp(name, "Blue Burst")) {
    return GameVersion::BB;
  }
  throw invalid_argument("incorrect version name");
}
