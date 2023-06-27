#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <string>

struct CharacterStats {
  le_uint16_t atp = 0;
  le_uint16_t mst = 0;
  le_uint16_t evp = 0;
  le_uint16_t hp = 0;
  le_uint16_t dfp = 0;
  le_uint16_t ata = 0;
  le_uint16_t lck = 0;
} __attribute__((packed));

class LevelTable { // from PlyLevelTbl.prs
public:
  struct LevelStats {
    uint8_t atp;
    uint8_t mst;
    uint8_t evp;
    uint8_t hp;
    uint8_t dfp;
    uint8_t ata;
    uint8_t lck;
    uint8_t tp;
    le_uint32_t experience;

    void apply(CharacterStats& ps) const;
  } __attribute__((packed));

  struct Table {
    CharacterStats base_stats[12];
    le_uint32_t unknown[12];
    LevelStats levels[12][200];
  } __attribute__((packed));

  LevelTable(std::shared_ptr<const std::string> data, bool compressed);

  const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  const LevelStats& stats_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::shared_ptr<const std::string> data;
  const Table* table;
};
