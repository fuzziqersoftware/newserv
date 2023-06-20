#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <random>
#include <string>
#include <vector>

#include "BattleParamsIndex.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct Map {
  struct RareEnemyRates {
    uint32_t hildeblue; // HILDEBEAR -> HILDEBLUE
    uint32_t rappy; // RAG_RAPPY -> {AL_RAPPY or seasonal rappies}; SAND_RAPPY -> DEL_RAPPY
    uint32_t nar_lily; // POISON_LILY -> NAR_LILY
    uint32_t pouilly_slime; // POFUILLY_SLIME -> POUILLY_SLIME
    uint32_t merissa_aa; // MERISSA_A -> MERISSA_AA
    uint32_t pazuzu; // ZU -> PAZUZU (and _ALT variants)
    uint32_t dorphon_eclair; // DORPHON -> DORPHON_ECLAIR
    uint32_t kondrieu; // {SAINT_MILLION, SHAMBERTIN} -> KONDRIEU
  };

  struct Enemy {
    enum Flag {
      HIT_BY_PLAYER0 = 0x01,
      HIT_BY_PLAYER1 = 0x02,
      HIT_BY_PLAYER2 = 0x04,
      HIT_BY_PLAYER3 = 0x08,
      DEFEATED = 0x10,
      ITEM_DROPPED = 0x20,
    };
    EnemyType type;
    uint8_t flags;
    uint8_t last_hit_by_client_id;

    explicit Enemy(EnemyType type);

    std::string str() const;
  } __attribute__((packed));

  std::vector<Enemy> enemies;
  std::vector<size_t> rare_enemy_indexes;

  void clear();
  void add_enemies_from_map_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      const void* data,
      size_t size,
      const RareEnemyRates* rare_rates = nullptr);
  void add_enemies_from_quest_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      const void* data,
      size_t size);
};

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
    std::shared_ptr<PSOLFGEncryption> random,
    Episode episode,
    bool is_solo);
std::vector<std::string> map_filenames_for_variation(
    Episode episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2);
void load_map_files();
