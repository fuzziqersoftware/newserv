#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

struct MapState {
  /* 0000 */ le_uint16_t width = 0;
  /* 0002 */ le_uint16_t height = 0;
  /* 0004 */ parray<parray<uint8_t, 0x10>, 0x10> tiles;
  /* 0104 */ parray<parray<uint8_t, 6>, 2> start_tile_definitions;
  /* 0110 */

  MapState();
  void clear();

  void print(FILE* stream) const;
} __packed_ws__(MapState, 0x110);

struct MapAndRulesState {
  /* 0000 */ MapState map;
  /* 0110 */ uint8_t num_players = 0;
  /* 0111 */ uint8_t unused1 = 0;
  /* 0112 */ uint8_t environment_number = 0;
  /* 0113 */ uint8_t num_players_per_team = 0;
  /* 0114 */ uint8_t num_team0_players = 0;
  /* 0115 */ uint8_t unused2 = 0;
  /* 0116 */ le_uint16_t start_facing_directions = 0;
  /* 0118 */ be_uint32_t unknown_a3 = 0;
  /* 011C */ le_uint32_t map_number = 0;
  /* 0120 */ be_uint32_t unused4 = 0;
  /* 0124 */ Rules rules;
  /* 0138 */

  MapAndRulesState();
  void clear();

  bool loc_is_within_bounds(uint8_t x, uint8_t y) const;
  bool tile_is_vacant(uint8_t x, uint8_t y);

  void set_occupied_bit_for_tile(uint8_t x, uint8_t y);
  void clear_occupied_bit_for_tile(uint8_t x, uint8_t y);
} __packed_ws__(MapAndRulesState, 0x138);

struct MapAndRulesStateTrial {
  /* 0000 */ MapState map;
  /* 0110 */ uint8_t num_players = 0;
  /* 0111 */ uint8_t unused1 = 0;
  /* 0112 */ uint8_t environment_number = 0;
  /* 0113 */ uint8_t num_players_per_team = 0;
  /* 0114 */ uint8_t num_team0_players = 0;
  /* 0115 */ uint8_t unused2 = 0;
  /* 0116 */ le_uint16_t unused5 = 0;
  /* 0118 */ be_uint32_t unknown_a3 = 0;
  /* 011C */ le_uint32_t map_number = 0;
  /* 0120 */ be_uint32_t unused4 = 0;
  /* 0124 */ RulesTrial rules;
  /* 0130 */

  MapAndRulesStateTrial() = default;
  MapAndRulesStateTrial(const MapAndRulesState& state);
  operator MapAndRulesState() const;
} __packed_ws__(MapAndRulesStateTrial, 0x130);

} // namespace Episode3
