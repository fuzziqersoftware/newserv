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
  this->environment_number = 0;
  this->num_players_per_team = 0;
  this->num_team0_players = 0;
  this->unused2 = 0;
  this->start_facing_directions = 0;
  this->unknown_a3 = 0;
  this->map_number = 0;
  this->unused4 = 0;
  this->rules.clear();
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

MapAndRulesStateTrial::MapAndRulesStateTrial(const MapAndRulesState& state)
    : map(state.map),
      num_players(state.num_players),
      unused1(state.unused1),
      environment_number(state.environment_number),
      num_players_per_team(state.num_players_per_team),
      num_team0_players(state.num_team0_players),
      unused2(state.unused2),
      unused5(state.start_facing_directions),
      unknown_a3(state.unknown_a3),
      map_number(state.map_number),
      unused4(state.unused4),
      rules(state.rules) {}

MapAndRulesStateTrial::operator MapAndRulesState() const {
  MapAndRulesState ret;
  ret.map = this->map;
  ret.num_players = this->num_players;
  ret.unused1 = this->unused1;
  ret.environment_number = this->environment_number;
  ret.num_players_per_team = this->num_players_per_team;
  ret.num_team0_players = this->num_team0_players;
  ret.unused2 = this->unused2;
  ret.start_facing_directions = this->unused5;
  ret.unknown_a3 = this->unknown_a3;
  ret.map_number = this->map_number;
  ret.unused4 = this->unused4;
  ret.rules = this->rules;
  return ret;
}

} // namespace Episode3
