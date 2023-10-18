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

struct PlayerStats {
  /* 00 */ CharacterStats char_stats;
  /* 0E */ le_uint16_t unknown_a1 = 0;
  /* 10 */ le_float unknown_a2 = 0.0;
  /* 14 */ le_float unknown_a3 = 0.0;
  /* 18 */ le_uint32_t level = 0;
  /* 1C */ le_uint32_t experience = 0;
  /* 20 */ le_uint32_t meseta = 0;
  /* 24 */
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
  const LevelStats& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  // TODO: Currently we only support the BB version of this file. It'd be nice
  // to support non-BB versions, but their formats are very different:
  //
  // BB:
  //   root:
  //     u32 offset:
  //       u32[12] unknown
  //     u32 offset:
  //       u32[12] offsets:
  //         LevelStats[200] level_stats
  //     u32 offset:
  //       CharacterStats[12] base_stats
  // GC:
  //   root:
  //     u32 offset:
  //       u32[12] offsets:
  //         LevelStats[200] level_stats
  // PC:
  //   root:
  //     u32 offset:
  //       u32 offset[9]:
  //         LevelStats[200] level_stats
  //     u32 offset:
  //       (0x18 bytes)
  //     u32 offset:
  //       PlayerStats[9] max_stats
  //     u32 offset:
  //       PlayerStats[9] level100_stats
  //     u32 offset:
  //       u32 offset[9]:
  //         CharacterStats level1_stats
  //     (11 more pointers)
  std::shared_ptr<const std::string> data;
  const Table* table;
};
