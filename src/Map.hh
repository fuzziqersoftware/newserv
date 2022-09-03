#pragma once

#include <inttypes.h>

#include <phosg/Encoding.hh>
#include <vector>
#include <string>

#include "Text.hh"



struct BattleParams {
    le_uint16_t atp; // attack power
    le_uint16_t psv; // perseverance (intelligence?)
    le_uint16_t evp; // evasion
    le_uint16_t hp;  // hit points
    le_uint16_t dfp; // defense
    le_uint16_t ata; // accuracy
    le_uint16_t lck; // luck
    le_uint16_t esp; // ???
    uint8_t unknown_a1[0x0C];
    le_uint32_t experience;
    le_uint32_t difficulty;
} __attribute__((packed));

class BattleParamsIndex {
public:
  using TableT = parray<BattleParams, 0x60>;

  BattleParamsIndex(const char* filename_prefix);

  const BattleParams& get(bool solo, uint8_t episode, uint8_t difficulty,
      uint8_t monster_type) const;
  std::shared_ptr<const TableT> get_subtable(
      bool solo, uint8_t episode, uint8_t difficulty) const;

private:
  // online/offline, episode, difficulty
  std::shared_ptr<TableT> entries[2][3][4];
};



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

std::vector<PSOEnemy> parse_map(
    uint8_t episode,
    uint8_t difficulty,
    std::shared_ptr<const BattleParamsIndex::TableT> battle_params,
    const void* data,
    size_t size,
    bool alt_enemies);
