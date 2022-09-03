#pragma once

#include <stdint.h>

#include <memory>
#include <string>
#include <phosg/Encoding.hh>



struct PlayerStats {
  le_uint16_t atp;
  le_uint16_t mst;
  le_uint16_t evp;
  le_uint16_t hp;
  le_uint16_t dfp;
  le_uint16_t ata;
  le_uint16_t lck;

  PlayerStats() noexcept;
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

    void apply(PlayerStats& ps) const;
  } __attribute__((packed));

  struct Table {
    PlayerStats base_stats[12];
    le_uint32_t unknown[12];
    LevelStats levels[12][200];
  } __attribute__((packed));

  LevelTable(std::shared_ptr<const std::string> data, bool compressed);

  const PlayerStats& base_stats_for_class(uint8_t char_class) const;
  const LevelStats& stats_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::shared_ptr<const std::string> data;
  const Table* table;
};
