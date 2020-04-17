#include "Version.hh"

#include <stdexcept>

#include <strings.h>

using namespace std;



uint16_t flags_for_version(GameVersion version, uint8_t sub_version) {
  switch (sub_version) {
    case 0x00: // initial check (before 9E recognition)
      switch (version) {
        case GameVersion::DC:
          return ClientFlag::DefaultV2DC;
        case GameVersion::GC:
          return ClientFlag::DefaultV3GC;
        case GameVersion::PC:
          return ClientFlag::DefaultV2PC;
        case GameVersion::Patch:
          return ClientFlag::DefaultV2PC;
        case GameVersion::BB:
          return ClientFlag::DefaultV3BB;
      }
      break;
    case 0x29: // PSO PC
      return ClientFlag::DefaultV2PC;
    case 0x30: // ???
    case 0x31: // PSO Ep1&2 US10, US11, EU10, JP10
    case 0x33: // PSO Ep1&2 EU50HZ
    case 0x34: // PSO Ep1&2 JP11
      return ClientFlag::DefaultV3GC;
    case 0x32: // PSO Ep1&2 US12, JP12
    case 0x35: // PSO Ep1&2 US12, JP12
    case 0x36: // PSO Ep1&2 US12, JP12
    case 0x39: // PSO Ep1&2 US12, JP12
      return ClientFlag::DefaultV3GCPlus;
    case 0x40: // PSO Ep3 trial
    case 0x41: // PSO Ep3 US
    case 0x42: // PSO Ep3 JP
    case 0x43: // PSO Ep3 UK
      return ClientFlag::DefaultV4;
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
    case GameVersion::Patch:
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
