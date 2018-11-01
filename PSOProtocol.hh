#pragma once

#include <inttypes.h>

#include "Version.hh"

struct PSOCommandHeaderPC {
  uint16_t size;
  uint8_t command;
  uint8_t flag;
};

struct PSOCommandHeaderDCGC {
  uint8_t command;
  uint8_t flag;
  uint16_t size;
};

struct PSOCommandHeaderBB {
  uint16_t size;
  uint16_t command;
  uint32_t flag;
};

union PSOCommandHeader {
  PSOCommandHeaderDCGC dc;
  PSOCommandHeaderPC pc;
  PSOCommandHeaderDCGC gc;
  PSOCommandHeaderBB bb;

  uint16_t command(GameVersion version) const;
  uint16_t size(GameVersion version) const;
  uint32_t flag(GameVersion version) const;
};

union PSOSubcommand {
  uint8_t byte[4];
  uint16_t word[2];
  uint32_t dword;
};
