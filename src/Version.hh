#pragma once

#include <inttypes.h>



enum class GameVersion {
  DC = 0,
  PC,
  PATCH,
  GC,
  BB,
};

uint16_t flags_for_version(GameVersion version, uint8_t sub_version);
const char* name_for_version(GameVersion version);
GameVersion version_for_name(const char* name);
