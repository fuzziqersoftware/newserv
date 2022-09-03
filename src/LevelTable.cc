#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "Compression.hh"

using namespace std;



LevelTable::LevelTable(shared_ptr<const string> data, bool compressed) {
  if (compressed) {
    this->data.reset(new string(prs_decompress(*data)));
  } else {
    this->data = data;
  }

  if (this->data->size() < sizeof(Table)) {
    throw invalid_argument("level table size is incorrect");
  }
  this->table = reinterpret_cast<const Table*>(this->data->data());
}

const PlayerStats& LevelTable::base_stats_for_class(uint8_t char_class) const {
  if (char_class >= 12) {
    throw out_of_range("invalid character class");
  }
  return this->table->base_stats[char_class];
}

const LevelTable::LevelStats& LevelTable::stats_for_level(uint8_t char_class,
    uint8_t level) const {
  if (char_class >= 12) {
    throw invalid_argument("invalid character class");
  }
  if (level >= 200) {
    throw invalid_argument("invalid character level");
  }
  return this->table->levels[char_class][level];
}

void LevelTable::LevelStats::apply(PlayerStats& ps) const {
  ps.ata += this->ata;
  ps.atp += this->atp;
  ps.dfp += this->dfp;
  ps.evp += this->evp;
  ps.hp += this->hp;
  ps.mst += this->mst;
  ps.lck += this->lck;
}
