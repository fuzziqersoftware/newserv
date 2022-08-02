#pragma once

#include <inttypes.h>

#include <phosg/Encoding.hh>
#include <vector>
#include <string>



struct BattleParams {
    le_uint16_t atp; // attack power
    le_uint16_t psv; // perseverance (intelligence?)
    le_uint16_t evp; // evasion
    le_uint16_t hp;  // hit points
    le_uint16_t dfp; // defense
    le_uint16_t ata; // accuracy
    le_uint16_t lck; // luck
    uint8_t unknown_a1[0x0E];
    le_uint32_t experience;
    le_uint32_t difficulty;
} __attribute__((packed));

struct BattleParamTable {
  BattleParams entries[2][3][4][0x60]; // online/offline, episode, difficulty, monster type

  BattleParamTable(const char* filename_prefix);

  const BattleParams& get(bool solo, uint8_t episode, uint8_t difficulty,
      uint8_t monster_type) const;
  const BattleParams* get_subtable(bool solo, uint8_t episode,
      uint8_t difficulty) const;
} __attribute__((packed));



struct BattleParamIndex {
  BattleParamTable table_for_episode[3];
} __attribute__((packed));

// an enemy entry as loaded by the game
struct PSOEnemy {
  uint64_t id;
  uint16_t source_type;
  uint8_t hit_flags;
  uint8_t last_hit;
  uint32_t experience;
  uint32_t rt_index;

  explicit PSOEnemy(uint64_t id);
  PSOEnemy(uint64_t id, uint16_t source_type, uint32_t experience, uint32_t rt_index);

  std::string str() const;
} __attribute__((packed));

std::vector<PSOEnemy> load_map(const std::string& filename, uint8_t episode,
    uint8_t difficulty, const BattleParams* bp, bool alt_enemies);
