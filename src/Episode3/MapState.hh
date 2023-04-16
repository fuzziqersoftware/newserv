#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndex.hh"

namespace Episode3 {

struct MapState {
  le_uint16_t width;
  le_uint16_t height;
  parray<parray<uint8_t, 0x10>, 0x10> tiles;
  parray<parray<uint8_t, 6>, 2> start_tile_definitions;

  MapState();
  void clear();

  void print(FILE* stream) const;
} __attribute__((packed));

struct MapAndRulesState {
  MapState map;
  uint8_t num_players;
  uint8_t unused1;
  uint8_t unused_by_server;
  uint8_t num_players_per_team;
  uint8_t num_team0_players;
  uint8_t unused2;
  le_uint16_t start_facing_directions;
  uint32_t unused3;
  le_uint32_t map_number;
  uint32_t unused4;
  Rules rules;
  uint32_t unused5;

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
