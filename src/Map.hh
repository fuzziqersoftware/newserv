#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <random>
#include <string>
#include <vector>

#include "BattleParamsIndex.hh"
#include "ItemNameIndex.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct Map {
  struct SectionHeader {
    enum class Type {
      END = 0,
      OBJECTS = 1,
      ENEMIES = 2,
      WAVE_EVENTS = 3,
      RANDOM_ENEMY_LOCATIONS = 4,
      RANDOM_ENEMY_DEFINITIONS = 5,
    };
    le_uint32_t le_type;
    le_uint32_t section_size; // Includes this header
    le_uint32_t floor;
    le_uint32_t data_size;

    inline Type type() const {
      return static_cast<Type>(this->le_type.load());
    }
  } __attribute__((packed));

  struct ObjectEntry { // Section type 1 (OBJECTS)
    /* 00 */ le_uint16_t base_type;
    /* 02 */ le_uint16_t flags;
    /* 04 */ le_uint16_t index;
    /* 06 */ le_uint16_t unknown_a2;
    /* 08 */ le_uint16_t entity_id; // == index + 0x4000
    /* 0A */ le_uint16_t group;
    /* 0C */ le_uint16_t section;
    /* 0E */ le_uint16_t unknown_a3;
    /* 10 */ le_float x;
    /* 14 */ le_float y;
    /* 18 */ le_float z;
    /* 1C */ le_uint32_t x_angle;
    /* 20 */ le_uint32_t y_angle;
    /* 24 */ le_uint32_t z_angle;
    /* 28 */ le_float param1; // If <= 0, this is a specialized box, and the specialization is in param4/5/6
    /* 2C */ le_float param2;
    /* 30 */ le_float param3;
    /* 34 */ le_uint32_t param4;
    /* 38 */ le_uint32_t param5;
    /* 3C */ le_uint32_t param6;
    /* 40 */ le_uint32_t unused; // Reserved for pointer in client's memory; unused by server
    /* 44 */

    std::string str() const;
  } __attribute__((packed));

  struct EnemyEntry { // Section type 2 (ENEMIES)
    /* 00 */ le_uint16_t base_type;
    /* 02 */ le_uint16_t flags;
    /* 04 */ le_uint16_t index;
    /* 06 */ le_uint16_t num_children;
    /* 08 */ le_uint16_t floor;
    /* 0A */ le_uint16_t entity_id; // == index + 0x1000
    /* 0C */ le_uint16_t section;
    /* 0E */ le_uint16_t wave_number;
    /* 10 */ le_uint16_t wave_number2;
    /* 12 */ le_uint16_t unknown_a1;
    /* 14 */ le_float x;
    /* 18 */ le_float y;
    /* 1C */ le_float z;
    /* 20 */ le_uint32_t x_angle;
    /* 24 */ le_uint32_t y_angle;
    /* 28 */ le_uint32_t z_angle;
    /* 2C */ le_float fparam1;
    /* 30 */ le_float fparam2;
    /* 34 */ le_float fparam3;
    /* 38 */ le_float fparam4;
    /* 3C */ le_float fparam5;
    /* 40 */ le_uint16_t uparam1;
    /* 42 */ le_uint16_t uparam2;
    /* 44 */ le_uint32_t unused; // Reserved for pointer in client's memory; unused by server
    /* 48 */

    std::string str() const;
  } __attribute__((packed));

  struct EventsSectionHeader { // Section type 3 (WAVE_EVENTS)
    /* 00 */ le_uint32_t footer_offset;
    /* 04 */ le_uint32_t entries_offset;
    /* 08 */ le_uint32_t entry_count;
    /* 0C */ be_uint32_t format; // 0 or 'evt2'
    /* 10 */
  } __attribute__((packed));

  struct Event1Entry { // Section type 3 (WAVE_EVENTS) if format == 0
    /* 00 */ le_uint32_t event_id;
    /* 04 */ le_uint16_t flags;
    /* 06 */ le_uint16_t unknown_a2;
    /* 08 */ le_uint16_t section;
    /* 0A */ le_uint16_t wave_number;
    /* 0C */ le_uint32_t delay;
    /* 10 */ le_uint32_t clear_events_index;
    /* 14 */
  } __attribute__((packed));

  struct Event2Entry { // Section type 3 (WAVE_EVENTS) if format == 'evt2'
    /* 00 */ le_uint32_t event_id;
    /* 04 */ le_uint16_t flags;
    /* 06 */ le_uint16_t unknown_a2;
    /* 08 */ le_uint16_t section;
    /* 0A */ le_uint16_t wave_number;
    /* 0C */ le_uint16_t min_delay;
    /* 0E */ le_uint16_t max_delay;
    /* 10 */ uint8_t min_enemies;
    /* 11 */ uint8_t max_enemies;
    /* 12 */ le_uint16_t max_waves;
    /* 14 */ le_uint32_t clear_events_index;
    /* 18 */
  } __attribute__((packed));

  struct RandomEnemyLocationsHeader { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint32_t section_table_offset; // Offset to RandomEnemyLocationSegment structs, from start of this struct
    /* 04 */ le_uint32_t entries_offset; // Offset to RandomEnemyLocationEntry structs, from start of this struct
    /* 08 */ le_uint32_t num_sections;
    /* 0C */
  } __attribute__((packed));

  struct RandomEnemyLocationSection { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint16_t section;
    /* 02 */ le_uint16_t count;
    /* 04 */ le_uint32_t offset;
    /* 08 */
  } __attribute__((packed));

  struct RandomEnemyLocationEntry { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_float x;
    /* 04 */ le_float y;
    /* 08 */ le_float z;
    /* 0C */ le_uint32_t x_angle;
    /* 10 */ le_uint32_t y_angle;
    /* 14 */ le_uint32_t z_angle;
    /* 18 */ uint16_t unknown_a9;
    /* 1A */ uint16_t unknown_a10;
    /* 1C */
  } __attribute__((packed));

  struct RandomEnemyDefinitionsHeader { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    /* 00 */ le_uint32_t entries_offset; // Offset to RandomEnemyDefinition structs, from start of this struct
    /* 04 */ le_uint32_t weight_entries_offset; // Offset to RandomEnemyDefinitionWeights structs, from start of this struct
    /* 08 */ le_uint32_t entry_count;
    /* 0C */ le_uint32_t weight_entry_count;
    /* 10 */
  } __attribute__((packed));

  struct RandomEnemyDefinition { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    // All fields through entry_num map to the corresponding fields in
    // EnemyEntry. Note that the order of the uparam fields is switched!
    /* 00 */ le_float fparam1;
    /* 04 */ le_float fparam2;
    /* 08 */ le_float fparam3;
    /* 0C */ le_float fparam4;
    /* 10 */ le_float fparam5;
    /* 14 */ le_uint16_t uparam2;
    /* 16 */ le_uint16_t uparam1;
    /* 18 */ le_uint32_t entry_num;
    /* 1C */ le_uint16_t min_children;
    /* 1E */ le_uint16_t max_children;
    /* 20 */
  } __attribute__((packed));

  struct RandomEnemyWeight { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    /* 00 */ uint8_t base_type_index;
    /* 01 */ uint8_t definition_entry_num;
    /* 02 */ uint8_t weight;
    /* 03 */ uint8_t unknown_a4;
    /* 04 */
  } __attribute__((packed));

  struct RareEnemyRates {
    uint32_t hildeblue; // HILDEBEAR -> HILDEBLUE
    uint32_t rappy; // RAG_RAPPY -> {AL_RAPPY or seasonal rappies}; SAND_RAPPY -> DEL_RAPPY
    uint32_t nar_lily; // POISON_LILY -> NAR_LILY
    uint32_t pouilly_slime; // POFUILLY_SLIME -> POUILLY_SLIME
    uint32_t merissa_aa; // MERISSA_A -> MERISSA_AA
    uint32_t pazuzu; // ZU -> PAZUZU (and _ALT variants)
    uint32_t dorphon_eclair; // DORPHON -> DORPHON_ECLAIR
    uint32_t kondrieu; // {SAINT_MILLION, SHAMBERTIN} -> KONDRIEU

    RareEnemyRates(uint32_t enemy_rate, uint32_t boss_rate);
    explicit RareEnemyRates(const JSON& json);

    JSON json() const;
  };

  static const std::shared_ptr<const RareEnemyRates> NO_RARE_ENEMIES;
  static const std::shared_ptr<const RareEnemyRates> DEFAULT_RARE_ENEMIES;

  struct Object {
    // TODO: Add more fields in here if we ever care about them. Currently we
    // only care about boxes with fixed item drops.
    size_t source_index;
    uint16_t base_type;
    uint16_t section;
    float param1; // If <= 0, this is a specialized box, and the specialization is in param4/5/6
    float param3; // If == 0, the item should be varied by difficulty and area
    uint32_t param4;
    uint32_t param5;
    uint32_t param6;
    uint8_t floor;
    bool item_drop_checked;

    std::string str(std::shared_ptr<const ItemNameIndex> name_index) const;
  };

  struct Enemy {
    enum Flag {
      EXP_REQUESTED_BY_PLAYER0 = 0x01,
      EXP_REQUESTED_BY_PLAYER1 = 0x02,
      EXP_REQUESTED_BY_PLAYER2 = 0x04,
      EXP_REQUESTED_BY_PLAYER3 = 0x08,
      ITEM_DROPPED = 0x10,
    };
    size_t source_index;
    EnemyType type;
    uint8_t floor;
    uint8_t state_flags;
    uint8_t last_hit_by_client_id;

    Enemy(size_t source_index, uint8_t floor, EnemyType type);

    std::string str() const;
  } __attribute__((packed));

  std::vector<Object> objects;
  std::vector<Enemy> enemies;
  std::vector<size_t> rare_enemy_indexes;

  void clear();

  void add_objects_from_map_data(uint8_t floor, const void* data, size_t size);

  bool check_and_log_rare_enemy(bool default_is_rare, uint32_t rare_rate);
  void add_enemy(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      uint8_t floor,
      size_t index,
      const EnemyEntry& e,
      std::shared_ptr<const RareEnemyRates> rare_rates = DEFAULT_RARE_ENEMIES);
  void add_enemies_from_map_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      uint8_t floor,
      const void* data,
      size_t size,
      std::shared_ptr<const RareEnemyRates> rare_rates = DEFAULT_RARE_ENEMIES);
  void add_random_enemies_from_map_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      uint8_t floor,
      StringReader wave_events_r,
      StringReader random_enemy_locations_r,
      StringReader random_enemy_definitions_r,
      uint32_t rare_seed,
      std::shared_ptr<const RareEnemyRates> rare_rates = DEFAULT_RARE_ENEMIES);

  struct DATSectionsForFloor {
    uint32_t objects = 0xFFFFFFFF;
    uint32_t enemies = 0xFFFFFFFF;
    uint32_t wave_events = 0xFFFFFFFF;
    uint32_t random_enemy_locations = 0xFFFFFFFF;
    uint32_t random_enemy_definitions = 0xFFFFFFFF;
  };
  static std::vector<DATSectionsForFloor> collect_quest_map_data_sections(const void* data, size_t size);

  void add_enemies_and_objects_from_quest_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      const void* data,
      size_t size,
      uint32_t rare_seed,
      std::shared_ptr<const RareEnemyRates> rare_rates = Map::DEFAULT_RARE_ENEMIES);

  static std::string disassemble_quest_data(const void* data, size_t size);
};

// TODO: This class is currently unused. It would be nice if we could use this
// to generate variations and link to the corresponding map filenames, but it
// seems that SetDataTable.rel files link to map filenames that don't actually
// exist in some cases, so we can't just directly use this data structure.
class SetDataTable {
public:
  struct SetEntry {
    std::string object_list_basename;
    std::string enemy_list_basename;
    std::string event_list_basename;
  };

  SetDataTable(std::shared_ptr<const std::string> data, bool big_endian);

  inline const std::vector<std::vector<std::vector<SetEntry>>> get() const {
    return this->entries;
  }

  void print(FILE* stream) const;

private:
  template <bool IsBigEndian>
  void load_table_t(std::shared_ptr<const std::string> data);

  // Indexes are [floor][variation1][variation2]
  // floor is cumulative per episode, so Ep2 starts at floor=18.
  std::vector<std::vector<std::vector<SetEntry>>> entries;
};

void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    std::shared_ptr<PSOLFGEncryption> random,
    Episode episode,
    bool is_solo);
void generate_variations_dc_nte(
    parray<le_uint32_t, 0x20>& variations,
    std::shared_ptr<PSOLFGEncryption> random);
std::vector<std::string> map_filenames_for_variation(
    Episode episode, bool is_solo, uint8_t floor, uint32_t var1, uint32_t var2, bool is_enemies);
