#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

struct MapState {
  /* 0000 */ le_uint16_t width;
  /* 0002 */ le_uint16_t height;
  /* 0004 */ parray<parray<uint8_t, 0x10>, 0x10> tiles;
  /* 0104 */ parray<parray<uint8_t, 6>, 2> start_tile_definitions;
  /* 0110 */

  MapState();
  void clear();

  void print(FILE* stream) const;
} __attribute__((packed));

struct MapAndRulesState {
  /* 0000 */ MapState map;
  /* 0110 */ uint8_t num_players;
  /* 0111 */ uint8_t unused1;
  /* 0112 */ uint8_t environment_number;
  /* 0113 */ uint8_t num_players_per_team;
  /* 0114 */ uint8_t num_team0_players;
  /* 0115 */ uint8_t unused2;
  /* 0116 */ le_uint16_t start_facing_directions;
  /* 0118 */ uint32_t unused3;
  /* 011C */ le_uint32_t map_number;
  /* 0120 */ uint32_t unused4;
  /* 0124 */ Rules rules;
  /* 0138 */

  MapAndRulesState();
  void clear();

  bool loc_is_within_bounds(uint8_t x, uint8_t y) const;
  bool tile_is_vacant(uint8_t x, uint8_t y);

  void set_occupied_bit_for_tile(uint8_t x, uint8_t y);
  void clear_occupied_bit_for_tile(uint8_t x, uint8_t y);
} __attribute__((packed));

struct OverlayState {
  parray<parray<uint8_t, 0x10>, 0x10> tiles;
  parray<le_uint32_t, 5> unused1;
  parray<le_uint32_t, 0x10> unused2;
  parray<le_uint16_t, 0x10> unused3;

  OverlayState();
  void clear();
} __attribute__((packed));

} // namespace Episode3
