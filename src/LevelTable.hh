#pragma once

#include <stdint.h>

#include <string>

#include "Player.hh"

// information on a single level for a single class
struct LevelStats {
  uint8_t atp; // atp to add on level up
  uint8_t mst; // mst to add on level up
  uint8_t evp; // evp to add on level up
  uint8_t hp; // hp to add on level up
  uint8_t dfp; // dfp to add on level up
  uint8_t ata; // ata to add on level up
  uint8_t unknown[2];
  uint32_t experience; // EXP value of this level

  void apply(PlayerStats& ps) const;
} __attribute__((packed));

// level table format (PlyLevelTbl.prs)
struct LevelTable {
  PlayerStats base_stats[12];
  uint32_t unknown[12];
  LevelStats levels[12][200];

  LevelTable(const std::string& filename, bool compressed);

  const PlayerStats& base_stats_for_class(uint8_t char_class) const;
  const LevelStats& stats_for_level(uint8_t char_class, uint8_t level) const;
} __attribute__((packed));
