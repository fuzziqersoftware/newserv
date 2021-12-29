#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "Compression.hh"

using namespace std;



LevelTable::LevelTable(const char* filename, bool compressed) {

  string data = load_file(filename);
  if (compressed) {
    data = prs_decompress(data);
  }

  if (data.size() < sizeof(*this)) {
    throw invalid_argument("level table size is incorrect");
  }

  memcpy(this, data.data(), sizeof(*this));
}

const PlayerStats& LevelTable::base_stats_for_class(uint8_t char_class) const {
  if (char_class >= 12) {
    throw out_of_range("invalid character class");
  }
  return this->base_stats[char_class];
}

 const LevelStats& LevelTable::stats_for_level(uint8_t char_class,
    uint8_t level) const {
  if (char_class >= 12) {
    throw invalid_argument("invalid character class");
  }
  if (level >= 200) {
    throw invalid_argument("invalid character level");
  }
  return this->levels[char_class][level];
}

// Levels up a character by adding the level-up bonuses to the player's stats.
void LevelStats::apply(PlayerStats& ps) const {
  ps.ata += this->ata;
  ps.atp += this->atp;
  ps.dfp += this->dfp;
  ps.evp += this->evp;
  ps.hp += this->hp;
  ps.mst += this->mst;
}
