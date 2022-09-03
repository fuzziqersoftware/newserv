#pragma once

#include <stdint.h>

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

struct LevelTable { // from PlyLevelTbl.prs
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

  PlayerStats base_stats[12];
  le_uint32_t unknown[12];
  LevelStats levels[12][200];

  LevelTable(const std::string& filename, bool compressed);

  const PlayerStats& base_stats_for_class(uint8_t char_class) const;
  const LevelStats& stats_for_level(uint8_t char_class, uint8_t level) const;

  static std::shared_ptr<LevelTable> load_shared(
        const std::string& filename, bool compressed);
} __attribute__((packed));
