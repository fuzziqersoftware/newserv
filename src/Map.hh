#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <random>
#include <string>
#include <vector>

#include "BattleParamsIndex.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"

struct Map {
  static const char* name_for_object_type(uint16_t type);

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
  } __packed_ws__(SectionHeader, 0x10);

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
    /* 28 */ le_float param1; // Boxes: if <= 0, this is a specialized box, and the specialization is in param4/5/6
    /* 2C */ le_float param2;
    /* 30 */ le_float param3; // Boxes: if == 0, the item should be varied by difficulty and area
    /* 34 */ le_uint32_t param4;
    /* 38 */ le_uint32_t param5;
    /* 3C */ le_uint32_t param6;
    /* 40 */ le_uint32_t unused; // Reserved for pointer in client's memory; unused by server
    /* 44 */

    std::string str() const;
  } __packed_ws__(ObjectEntry, 0x44);

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
  } __packed_ws__(EnemyEntry, 0x48);

  struct EventsSectionHeader { // Section type 3 (WAVE_EVENTS)
    /* 00 */ le_uint32_t action_stream_offset;
    /* 04 */ le_uint32_t entries_offset;
    /* 08 */ le_uint32_t entry_count;
    /* 0C */ be_uint32_t format; // 0 or 'evt2'
    /* 10 */
  } __packed_ws__(EventsSectionHeader, 0x10);

  struct Event1Entry { // Section type 3 (WAVE_EVENTS) if format == 0
    /* 00 */ le_uint32_t event_id;
    // Bits in flags:
    //   0004 = is active
    //   0008 = post-wave actions have been run
    //   0010 = all enemies killed
    /* 04 */ le_uint16_t flags;
    /* 06 */ le_uint16_t event_type;
    /* 08 */ le_uint16_t section;
    /* 0A */ le_uint16_t wave_number;
    /* 0C */ le_uint32_t delay;
    /* 10 */ le_uint32_t action_stream_offset;
    /* 14 */
  } __packed_ws__(Event1Entry, 0x14);

  struct Event2Entry { // Section type 3 (WAVE_EVENTS) if format == 'evt2'
    /* 00 */ le_uint32_t event_id;
    /* 04 */ le_uint16_t flags;
    /* 06 */ le_uint16_t event_type;
    /* 08 */ le_uint16_t section;
    /* 0A */ le_uint16_t wave_number;
    /* 0C */ le_uint16_t min_delay;
    /* 0E */ le_uint16_t max_delay;
    /* 10 */ uint8_t min_enemies;
    /* 11 */ uint8_t max_enemies;
    /* 12 */ le_uint16_t max_waves;
    /* 14 */ le_uint32_t action_stream_offset;
    /* 18 */
  } __packed_ws__(Event2Entry, 0x18);

  struct RandomEnemyLocationsHeader { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint32_t section_table_offset; // Offset to RandomEnemyLocationSegment structs, from start of this struct
    /* 04 */ le_uint32_t entries_offset; // Offset to RandomEnemyLocationEntry structs, from start of this struct
    /* 08 */ le_uint32_t num_sections;
    /* 0C */
  } __packed_ws__(RandomEnemyLocationsHeader, 0x0C);

  struct RandomEnemyLocationSection { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint16_t section;
    /* 02 */ le_uint16_t count;
    /* 04 */ le_uint32_t offset;
    /* 08 */
  } __packed_ws__(RandomEnemyLocationSection, 8);

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
  } __packed_ws__(RandomEnemyLocationEntry, 0x1C);

  struct RandomEnemyDefinitionsHeader { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    /* 00 */ le_uint32_t entries_offset; // Offset to RandomEnemyDefinition structs, from start of this struct
    /* 04 */ le_uint32_t weight_entries_offset; // Offset to RandomEnemyDefinitionWeights structs, from start of this struct
    /* 08 */ le_uint32_t entry_count;
    /* 0C */ le_uint32_t weight_entry_count;
    /* 10 */
  } __packed_ws__(RandomEnemyDefinitionsHeader, 0x10);

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
  } __packed_ws__(RandomEnemyDefinition, 0x20);

  struct RandomEnemyWeight { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    /* 00 */ uint8_t base_type_index;
    /* 01 */ uint8_t definition_entry_num;
    /* 02 */ uint8_t weight;
    /* 03 */ uint8_t unknown_a4;
    /* 04 */
  } __packed_ws__(RandomEnemyWeight, 4);

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
    explicit RareEnemyRates(const phosg::JSON& json);

    phosg::JSON json() const;
  };

  static const std::shared_ptr<const RareEnemyRates> NO_RARE_ENEMIES;
  static const std::shared_ptr<const RareEnemyRates> DEFAULT_RARE_ENEMIES;

  struct Object {
    // TODO: Add more fields in here if we ever care about them. Currently we
    // only care about boxes with fixed item drops.
    const ObjectEntry* args;
    size_t source_index;
    uint8_t floor;
    uint16_t object_id;
    uint16_t game_flags;
    // Technically set_flags shouldn't be part of the Object struct, but all
    // object entries always generate exactly one object, so we store it here.
    uint16_t set_flags;
    bool item_drop_checked;

    std::string str() const;
  };

  struct Enemy {
    enum Flag {
      LAST_HIT_MASK = 0x03,
      EXP_GIVEN = 0x04,
      ITEM_DROPPED = 0x08,
      ALL_HITS_MASK_FIRST = 0x10,
      ALL_HITS_MASK = 0xF0,
    };
    size_t source_index;
    size_t set_index;
    uint16_t enemy_id;
    uint16_t total_damage;
    uint32_t game_flags; // From 6x0A
    uint16_t section;
    uint16_t wave_number;
    EnemyType type;
    uint8_t floor;
    uint8_t server_flags;
    uint16_t alias_entity_id;

    Enemy(
        uint16_t enemy_id,
        size_t source_index,
        size_t set_index,
        uint8_t floor,
        uint16_t section,
        uint16_t wave_number,
        EnemyType type,
        uint16_t alias_entity_id);

    std::string str() const;

    inline bool ever_hit_by_client_id(uint8_t client_id) const {
      return this->server_flags & (Flag::ALL_HITS_MASK_FIRST << client_id);
    }
    inline bool last_hit_by_client_id(uint8_t client_id) const {
      return (this->server_flags & Flag::LAST_HIT_MASK) == client_id;
    }
    inline void set_last_hit_by_client_id(uint8_t client_id) {
      this->server_flags = (this->server_flags & (~Flag::LAST_HIT_MASK)) | (Flag::ALL_HITS_MASK_FIRST << client_id) | (client_id & 3);
    }
  };

  struct Event {
    uint32_t event_id;
    uint16_t flags;
    uint16_t section;
    uint16_t wave_number;
    uint8_t floor;
    uint32_t action_stream_offset;
    std::vector<size_t> enemy_indexes;

    std::string str() const;
  };

  struct DATParserRandomState {
    PSOV2Encryption random;
    PSOV2Encryption location_table_random;
    std::array<uint32_t, 0x20> location_index_table;
    uint32_t location_indexes_populated;
    uint32_t location_indexes_used;
    uint32_t location_entries_base_offset;

    DATParserRandomState(uint32_t rare_seed);
    size_t rand_int_biased(size_t min_v, size_t max_v);
    uint32_t next_location_index();
    void generate_shuffled_location_table(const Map::RandomEnemyLocationsHeader& header, phosg::StringReader r, uint16_t section);
  };

  Map(Version version, uint32_t lobby_id, uint32_t rare_seed, std::shared_ptr<PSOLFGEncryption> opt_rand_crypt);
  ~Map() = default;

  void clear();

  inline void link_owned_data(std::shared_ptr<const std::string> data) {
    this->linked_data.emplace(data);
  }

  void add_objects_from_owned_map_data(uint8_t floor, const void* data, size_t size);
  void add_objects_from_map_data(uint8_t floor, std::shared_ptr<const std::string> data);

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
      phosg::StringReader wave_events_r,
      phosg::StringReader random_enemy_locations_r,
      phosg::StringReader random_enemy_definitions_r,
      std::shared_ptr<DATParserRandomState> random_state,
      std::shared_ptr<const RareEnemyRates> rare_rates = DEFAULT_RARE_ENEMIES);

  void add_event(
      uint32_t event_id,
      uint16_t flags,
      uint8_t floor,
      uint16_t section,
      uint16_t wave_number,
      uint32_t action_stream_offset);
  std::vector<Event*> get_events(uint8_t floor, uint32_t event_id);
  std::vector<const Event*> get_events(uint8_t floor, uint32_t event_id) const;
  void add_events_from_map_data(uint8_t floor, const void* data, size_t size);

  struct DATSectionsForFloor {
    uint32_t objects = 0xFFFFFFFF;
    uint32_t enemies = 0xFFFFFFFF;
    uint32_t wave_events = 0xFFFFFFFF;
    uint32_t random_enemy_locations = 0xFFFFFFFF;
    uint32_t random_enemy_definitions = 0xFFFFFFFF;
  };
  static std::vector<DATSectionsForFloor> collect_quest_map_data_sections(const void* data, size_t size);

  void add_entities_from_quest_data(
      Episode episode,
      uint8_t difficulty,
      uint8_t event,
      std::shared_ptr<const std::string> data,
      std::shared_ptr<const RareEnemyRates> rare_rates = Map::DEFAULT_RARE_ENEMIES);

  const Enemy& find_enemy(uint16_t enemy_id) const;
  Enemy& find_enemy(uint16_t enemy_id);
  const Enemy& find_enemy(uint8_t floor, EnemyType type) const;
  Enemy& find_enemy(uint8_t floor, EnemyType type);
  std::vector<Object*> get_objects(uint8_t floor, uint16_t section, uint16_t wave_number);
  std::vector<Enemy*> get_enemies(uint8_t floor, uint16_t section, uint16_t wave_number);
  std::vector<Event*> get_events(uint8_t floor, uint16_t section, uint16_t wave_number);
  std::vector<Event*> get_events(uint8_t floor);

  static std::string disassemble_objects_data(const void* data, size_t size, size_t* object_number = nullptr);
  static std::string disassemble_enemies_data(const void* data, size_t size, size_t* enemy_number = nullptr);
  static std::string disassemble_wave_events_data(const void* data, size_t size, uint8_t floor = 0xFF);
  static std::string disassemble_quest_data(const void* data, size_t size);

  phosg::PrefixedLogger log;
  Version version;
  uint32_t rare_seed;
  std::unordered_set<std::shared_ptr<const std::string>> linked_data;
  std::shared_ptr<PSOLFGEncryption> opt_rand_crypt;
  std::vector<Object> objects;
  std::vector<Enemy> enemies;
  std::vector<uint16_t> enemy_set_flags;
  std::vector<size_t> rare_enemy_indexes;
  std::vector<Event> events;
  std::string event_action_stream;
  std::multimap<uint64_t, size_t> floor_and_event_id_to_index;
  std::unordered_multimap<uint64_t, size_t> floor_section_and_group_to_object_index;
  std::unordered_multimap<uint64_t, size_t> floor_section_and_wave_number_to_enemy_index;
  std::unordered_multimap<uint64_t, size_t> floor_section_and_wave_number_to_event_index;
};

class SetDataTableBase {
public:
  virtual ~SetDataTableBase() = default;

  parray<le_uint32_t, 0x20> generate_variations(
      Episode episode,
      bool is_solo,
      std::shared_ptr<PSOLFGEncryption> opt_rand_crypt = nullptr) const;
  virtual std::pair<uint32_t, uint32_t> num_available_variations_for_floor(Episode episode, uint8_t floor) const = 0;
  virtual std::pair<uint32_t, uint32_t> num_free_roam_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const = 0;

  enum class FilenameType {
    OBJECTS = 0,
    ENEMIES,
    EVENTS,
  };

  virtual std::string map_filename_for_variation(
      uint8_t floor, uint32_t var1, uint32_t var2, Episode episode, GameMode mode, FilenameType type) const = 0;
  std::vector<std::string> map_filenames_for_variations(
      const parray<le_uint32_t, 0x20>& variations, Episode episode, GameMode mode, FilenameType type) const;

  uint8_t default_area_for_floor(Episode episode, uint8_t floor) const;

protected:
  explicit SetDataTableBase(Version version);

  Version version;
};

class SetDataTable : public SetDataTableBase {
public:
  struct SetEntry {
    std::string object_list_basename;
    std::string enemy_and_event_list_basename;
    std::string area_setup_filename;
  };

  SetDataTable(Version version, const std::string& data);
  virtual ~SetDataTable() = default;

  virtual std::pair<uint32_t, uint32_t> num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual std::pair<uint32_t, uint32_t> num_free_roam_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      uint8_t floor, uint32_t var1, uint32_t var2, Episode episode, GameMode mode, FilenameType type) const;

  std::string str() const;

private:
  template <bool BE>
  void load_table_t(const std::string& data);

  // Indexes are [floor][variation1][variation2]
  // floor is cumulative per episode, so Ep2 starts at floor=18.
  std::vector<std::vector<std::vector<SetEntry>>> entries;
};

class SetDataTableDCNTE : public SetDataTableBase {
public:
  SetDataTableDCNTE();
  virtual ~SetDataTableDCNTE() = default;

  virtual std::pair<uint32_t, uint32_t> num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual std::pair<uint32_t, uint32_t> num_free_roam_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      uint8_t floor, uint32_t var1, uint32_t var2, Episode episode, GameMode mode, FilenameType type) const;

private:
  static const std::array<std::vector<std::vector<std::string>>, 0x12> NAMES;
};

class SetDataTableDC112000 : public SetDataTableBase {
public:
  SetDataTableDC112000();
  virtual ~SetDataTableDC112000() = default;

  virtual std::pair<uint32_t, uint32_t> num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual std::pair<uint32_t, uint32_t> num_free_roam_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      uint8_t floor, uint32_t var1, uint32_t var2, Episode episode, GameMode mode, FilenameType type) const;

private:
  static const std::array<std::vector<std::vector<std::string>>, 0x12> NAMES;
};

void generate_variations_deprecated(
    parray<le_uint32_t, 0x20>& variations,
    std::shared_ptr<PSOLFGEncryption> random,
    Version version,
    Episode episode,
    bool is_solo);

parray<le_uint32_t, 0x20> variation_maxes_deprecated(Version version, Episode episode, bool is_solo);
bool next_variation_deprecated(parray<le_uint32_t, 0x20>& variations, Version version, Episode episode, bool is_solo);

std::vector<std::string> map_filenames_for_variation_deprecated(
    uint8_t floor, uint32_t var1, uint32_t var2, Version version, Episode episode, GameMode mode, bool is_enemies);
std::vector<std::vector<std::string>> map_filenames_for_variations_deprecated(
    const parray<le_uint32_t, 0x20>& variations,
    Version version,
    Episode episode,
    GameMode mode,
    bool is_enemies);
