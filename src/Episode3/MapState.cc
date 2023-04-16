#include "MapState.hh"

using namespace std;

namespace Episode3 {

MapState::MapState() {
  this->clear();
}

void MapState::clear() {
  this->width = 0;
  this->height = 0;
  for (size_t y = 0; y < this->tiles.size(); y++) {
    this->tiles[y].clear(0);
  }
  for (size_t z = 0; z < 2; z++) {
    this->start_tile_definitions[z].clear(0);
  }
}

void MapState::print(FILE* stream) const {
  fprintf(stream, "[Map: w=%hu h=%hu]\n", this->width.load(), this->height.load());
  for (size_t y = 0; y < this->height; y++) {
    fputc(' ', stream);
    for (size_t x = 0; x < this->width; x++) {
      fprintf(stream, " %02hhX", this->tiles[y][x]);
    }
    fputc('\n', stream);
  }
}

MapAndRulesState::MapAndRulesState() {
  this->clear();
}

void MapAndRulesState::clear() {
  this->map.clear();
  this->num_players = 0;
  this->unused1 = 0;
  this->unused_by_server = 0;
  this->num_players_per_team = 0;
  this->num_team0_players = 0;
  this->unused2 = 0;
  this->start_facing_directions = 0;
  this->unused3 = 0;
  this->map_number = 0;
  this->unused4 = 0;
  this->rules.clear();
  this->unused5 = 0;
}

bool MapAndRulesState::loc_is_within_bounds(uint8_t x, uint8_t y) const {
  return (x < this->map.width) && (y < this->map.height);
}

bool MapAndRulesState::tile_is_vacant(uint8_t x, uint8_t y) {
  if (!this->loc_is_within_bounds(x, y)) {
    return false;
  }
  return (this->map.tiles[y][x] == 1);
}

void MapAndRulesState::set_occupied_bit_for_tile(uint8_t x, uint8_t y) {
  this->map.tiles[y][x] |= 0x10;
}

void MapAndRulesState::clear_occupied_bit_for_tile(uint8_t x, uint8_t y) {
  this->map.tiles[y][x] &= 0xEF;
}

OverlayState::OverlayState() {
  this->clear();
}

void OverlayState::clear() {
  for (size_t y = 0; y < this->tiles.size(); y++) {
    this->tiles[y].clear(0);
  }
  this->unused1.clear(0);
  this->unused2.clear(0);
  this->unused3.clear(0);
}

} // namespace Episode3
