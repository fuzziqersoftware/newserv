#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <random>
#include <string>
#include <vector>

#include "StaticGameData.hh"
#include "Text.hh"

class BattleParamsIndex {
public:
  struct Entry {
    le_uint16_t atp; // attack power
    le_uint16_t psv; // perseverance (intelligence?)
    le_uint16_t evp; // evasion
    le_uint16_t hp; // hit points
    le_uint16_t dfp; // defense
    le_uint16_t ata; // accuracy
    le_uint16_t lck; // luck
    le_uint16_t esp; // ???
    parray<uint8_t, 0x0C> unknown_a1;
    le_uint32_t experience;
    le_uint32_t difficulty;

    std::string str() const;
  } __attribute__((packed));

  struct Table {
    parray<parray<Entry, 0x60>, 4> difficulty;

    void print(FILE* stream) const;
  } __attribute__((packed));

  BattleParamsIndex(
      std::shared_ptr<const std::string> data_on_ep1, // BattleParamEntry_on.dat
      std::shared_ptr<const std::string> data_on_ep2, // BattleParamEntry_lab_on.dat
      std::shared_ptr<const std::string> data_on_ep4, // BattleParamEntry_ep4_on.dat
      std::shared_ptr<const std::string> data_off_ep1, // BattleParamEntry.dat
      std::shared_ptr<const std::string> data_off_ep2, // BattleParamEntry_lab.dat
      std::shared_ptr<const std::string> data_off_ep4); // BattleParamEntry_ep4.dat

  const Entry& get(bool solo, Episode episode, uint8_t difficulty,
      uint8_t monster_type) const;

private:
  struct LoadedFile {
    std::shared_ptr<const std::string> data;
    const Table* table;
  };

  // online/offline, episode
  LoadedFile files[2][3];
};

struct PSOEnemy {
  uint64_t id;
  uint16_t source_type;
  uint8_t hit_flags;
  uint8_t last_hit;
  uint32_t experience;
  uint32_t rt_index;
  size_t num_clones;
  const char* type_name;

  explicit PSOEnemy(uint64_t id);
  PSOEnemy(
      uint64_t id,
      uint16_t source_type,
      uint32_t experience,
      uint32_t rt_index,
      size_t num_clones,
      const char* type_name);

  std::string str() const;
} __attribute__((packed));

std::vector<PSOEnemy> parse_map(
    std::shared_ptr<const BattleParamsIndex> battle_params,
    bool is_solo,
    Episode episode,
    uint8_t difficulty,
    std::shared_ptr<const std::string> data,
    bool alt_enemies);

// TODO: This class is currently unused. It would be nice if we could use this
// to generate variations and link to the corresponding map filenames, but it
// seems that SetDataTable.rel files link to map filenames that don't actually
// exist in some cases, so we can't just directly use this data structure.
class SetDataTable {
public:
  struct SetEntry {
    std::string name1;
    std::string enemy_list_basename;
    std::string name3;
  };

  SetDataTable(std::shared_ptr<const std::string> data, bool big_endian);

  inline const std::vector<std::vector<std::vector<SetEntry>>> get() const {
    return this->entries;
  }

  void print(FILE* stream) const;

private:
  template <bool IsBigEndian>
  void load_table_t(std::shared_ptr<const std::string> data);

  // Indexes are [area_id][variation1][variation2]
  // area_id is cumulative per episode, so Ep2 starts at area_id=18.
  std::vector<std::vector<std::vector<SetEntry>>> entries;
};

void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    std::shared_ptr<std::mt19937> random,
    Episode episode,
    bool is_solo);
std::vector<std::string> map_filenames_for_variation(
    Episode episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2);
void load_map_files();
