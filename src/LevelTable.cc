#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "Compression.hh"

using namespace std;

void PlayerStats::reset_to_base(uint8_t char_class, shared_ptr<const LevelTable> level_table) {
  this->level = 0;
  this->experience = 0;
  this->char_stats = level_table->base_stats_for_class(char_class);
}

void PlayerStats::advance_to_level(uint8_t char_class, uint32_t level, shared_ptr<const LevelTable> level_table) {
  for (; this->level < level; this->level++) {
    const auto& level_stats = level_table->stats_delta_for_level(char_class, this->level + 1);
    // The original code clamps the resulting stat values to [0, max_stat]; we
    // don't have max_stat handy so we just allow them to be unbounded
    this->char_stats.atp += level_stats.atp;
    this->char_stats.mst += level_stats.mst;
    this->char_stats.evp += level_stats.evp;
    this->char_stats.hp += level_stats.hp;
    this->char_stats.dfp += level_stats.dfp;
    this->char_stats.ata += level_stats.ata;
    // Note: It is not a bug that lck is ignored here; the original code
    // ignores it too.
    this->experience = level_stats.experience;
  }
}

LevelTable::LevelTable(shared_ptr<const string> data, bool compressed) {
  if (compressed) {
    this->data = make_shared<string>(prs_decompress(*data));
  } else {
    this->data = data;
  }

  if (this->data->size() < sizeof(Table)) {
    throw invalid_argument("level table size is incorrect");
  }
  this->table = reinterpret_cast<const Table*>(this->data->data());
}

const CharacterStats& LevelTable::base_stats_for_class(uint8_t char_class) const {
  if (char_class >= 12) {
    throw out_of_range("invalid character class");
  }
  return this->table->base_stats[char_class];
}

const LevelTable::LevelStats& LevelTable::stats_delta_for_level(
    uint8_t char_class, uint8_t level) const {
  if (char_class >= 12) {
    throw invalid_argument("invalid character class");
  }
  if (level >= 200) {
    throw invalid_argument("invalid character level");
  }
  return this->table->levels[char_class][level];
}

void LevelTable::LevelStats::apply(CharacterStats& ps) const {
  ps.ata += this->ata;
  ps.atp += this->atp;
  ps.dfp += this->dfp;
  ps.evp += this->evp;
  ps.hp += this->hp;
  ps.mst += this->mst;
  ps.lck += this->lck;
}
