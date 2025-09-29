#pragma once

#include <inttypes.h>

#include <array>
#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <random>
#include <string>
#include <vector>

#include "BattleParamsIndex.hh"
#include "CommonFileFormats.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"

////////////////////////////////////////////////////////////////////////////////
// Game structures (used in 6x6B/6x6C/6x6E handlers, and here)

struct SyncEnemyStateEntry {
  le_uint32_t flags = 0; // Same as flags in 6x0A
  le_uint16_t item_drop_id = 0;
  le_uint16_t total_damage = 0; // Same as in 6x0A
  uint8_t red_buff_type = 0;
  uint8_t red_buff_level = 0;
  uint8_t blue_buff_type = 0;
  uint8_t blue_buff_level = 0;
} __packed_ws__(SyncEnemyStateEntry, 0x0C);

struct SyncObjectStateEntry {
  le_uint16_t flags = 0;
  le_uint16_t item_drop_id = 0;
} __packed_ws__(SyncObjectStateEntry, 0x04);

////////////////////////////////////////////////////////////////////////////////
// Set data table (variations index)

struct Variations {
  struct Entry {
    le_uint32_t layout = 0;
    le_uint32_t entities = 0;
  } __packed_ws__(Entry, 0x08);
  parray<Entry, 0x10> entries;

  std::string str() const;
  phosg::JSON json() const;
} __packed_ws__(Variations, 0x80);

class SetDataTableBase {
public:
  virtual ~SetDataTableBase() = default;

  Variations generate_variations(Episode episode, bool is_solo, std::shared_ptr<RandomGenerator> rand_crypt) const;
  virtual Variations::Entry num_available_variations_for_floor(Episode episode, uint8_t floor) const = 0;
  virtual Variations::Entry num_free_play_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const = 0;

  enum class FilenameType {
    OBJECT_SETS = 0,
    ENEMY_SETS,
    EVENTS,
  };

  virtual std::string map_filename_for_variation(
      Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const = 0;
  std::vector<std::string> map_filenames_for_variations(
      Episode episode, GameMode mode, const Variations& variations, FilenameType type) const;

  static uint8_t default_area_for_floor(Version version, Episode episode, uint8_t floor);
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

  virtual Variations::Entry num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual Variations::Entry num_free_play_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const;

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

  virtual Variations::Entry num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual Variations::Entry num_free_play_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const;

private:
  static const std::array<std::vector<std::vector<std::string>>, 0x12> NAMES;
};

class SetDataTableDC112000 : public SetDataTableBase {
public:
  SetDataTableDC112000();
  virtual ~SetDataTableDC112000() = default;

  virtual Variations::Entry num_available_variations_for_floor(Episode episode, uint8_t floor) const;
  virtual Variations::Entry num_free_play_variations_for_floor(Episode episode, bool is_solo, uint8_t floor) const;
  virtual std::string map_filename_for_variation(
      Episode episode, GameMode mode, uint8_t floor, uint32_t layout, uint32_t entities, FilenameType type) const;

private:
  static const std::array<std::vector<std::vector<std::string>>, 0x12> NAMES;
};

////////////////////////////////////////////////////////////////////////////////
// Map (DAT) file parser. This class is responsible for parsing individual
// quest DAT files, or up to three individual free-play DAT files (object sets,
// enemy sets, and events, each of which are optional). In the free-play case,
// the MapFile represents entities for a single floor; in the quest case, it
// represents the lists for all floors.

class MapFile : public std::enable_shared_from_this<MapFile> {
public:
  static std::string name_for_object_type(uint16_t type, Version version = Version::UNKNOWN, uint8_t area = 0xFF);
  static std::string name_for_enemy_type(uint16_t type, Version version = Version::UNKNOWN, uint8_t area = 0xFF);

  struct SectionHeader { // Only used for quest DAT files
    enum class Type {
      END = 0,
      OBJECT_SETS = 1,
      ENEMY_SETS = 2,
      EVENTS = 3,
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

  struct ObjectSetEntry { // Section type 1 (OBJECT_SETS)
    /* 00 */ le_uint16_t base_type = 0;
    /* 02 */ le_uint16_t set_flags = 0; // Used by PSO at runtime, unused in DAT file
    /* 04 */ le_uint16_t index = 0; // Used by PSO at runtime, unused in DAT file
    /* 06 */ le_uint16_t floor = 0;
    /* 08 */ le_uint16_t entity_id = 0; // == index + 0x4000; used by PSO at runtime, unused in DAT file
    /* 0A */ le_uint16_t group = 0;
    /* 0C */ le_uint16_t room = 0;
    /* 0E */ le_uint16_t unknown_a3 = 0;
    // The position is relative to the room in which the object is placed; to
    // get the actual world position, the object's position must be rotated
    // around the room's origin by the room's angles, then translated by the
    // room's offset. The room's angle and offset can be found in the area's
    // n.rel file.
    /* 10 */ VectorXYZF pos;
    // Angles are specified as 16-bit integers, where 0 is no rotation around
    // the axis and FFFF is almost a complete counterclockwise rotation.
    /* 1C */ VectorXYZI angle;
    // See notes in dat_object_definitions in Map.cc for how these are used
    /* 28 */ le_float param1 = 0.0f;
    /* 2C */ le_float param2 = 0.0f;
    /* 30 */ le_float param3 = 0.0f;
    /* 34 */ le_int32_t param4 = 0;
    /* 38 */ le_int32_t param5 = 0;
    /* 3C */ le_int32_t param6 = 0;
    /* 40 */ le_uint32_t unused = 0; // Reserved for pointer in client's memory; unused by server
    /* 44 */

    uint64_t semantic_hash(uint8_t floor) const;
    std::string str(Version version = Version::UNKNOWN, uint8_t area = 0xFF) const;
  } __packed_ws__(ObjectSetEntry, 0x44);

  struct EnemySetEntry { // Section type 2 (ENEMY_SETS)
    /* 00 */ le_uint16_t base_type = 0;
    /* 02 */ le_uint16_t set_flags = 0; // Used by PSO at runtime, unused in DAT file
    /* 04 */ le_uint16_t index = 0; // Used by PSO at runtime, unused in DAT file
    /* 06 */ le_uint16_t num_children = 0; // If == 0, use the default child count from the constructor table (which is often also 0)
    /* 08 */ le_uint16_t floor = 0;
    /* 0A */ le_uint16_t entity_id = 0; // == index + 0x1000; used by PSO at runtime, unused in DAT file
    /* 0C */ le_uint16_t room = 0;
    /* 0E */ le_uint16_t wave_number = 0;
    /* 10 */ le_uint16_t wave_number2 = 0;
    /* 12 */ le_uint16_t unknown_a1 = 0;
    /* 14 */ VectorXYZF pos;
    /* 24 */ VectorXYZI angle;
    // See notes in dat_enemy_definitions in Map.cc for how these are used
    /* 2C */ le_float param1 = 0.0f;
    /* 30 */ le_float param2 = 0.0f;
    /* 34 */ le_float param3 = 0.0f;
    /* 38 */ le_float param4 = 0.0f;
    /* 3C */ le_float param5 = 0.0f;
    /* 40 */ le_int16_t param6 = 0;
    /* 42 */ le_int16_t param7 = 0;
    /* 44 */ le_uint32_t unused = 0; // Reserved for pointer in client's memory; unused by server
    /* 48 */

    uint64_t semantic_hash(uint8_t floor) const;
    std::string str(Version version = Version::UNKNOWN, uint8_t area = 0xFF) const;
  } __packed_ws__(EnemySetEntry, 0x48);

  struct EventsSectionHeader { // Section type 3 (EVENTS)
    /* 00 */ le_uint32_t action_stream_offset;
    /* 04 */ le_uint32_t entries_offset;
    /* 08 */ le_uint32_t entry_count;
    /* 0C */ be_uint32_t format; // 0 or 'evt2'
    /* 10 */

    inline bool is_evt2() const {
      return (this->format == 0x65767432);
    }
  } __packed_ws__(EventsSectionHeader, 0x10);

  struct Event1Entry { // Section type 3 (EVENTS) if format == 0
    /* 00 */ le_uint32_t event_id = 0;
    // Bits in flags:
    //   0004 = is active
    //   0008 = post-wave actions have been run
    //   0010 = all enemies killed
    /* 04 */ le_uint16_t flags = 0; // Used by PSO at runtime, unused in DAT file
    /* 06 */ le_uint16_t event_type = 0;
    /* 08 */ le_uint16_t room = 0;
    /* 0A */ le_uint16_t wave_number = 0;
    /* 0C */ le_uint32_t delay = 0;
    /* 10 */ le_uint32_t action_stream_offset = 0;
    /* 14 */

    uint64_t semantic_hash(uint8_t floor) const;
    std::string str() const;
  } __packed_ws__(Event1Entry, 0x14);

  struct Event2Entry { // Section type 3 (EVENTS) if format == 'evt2'
    /* 00 */ le_uint32_t event_id = 0;
    /* 04 */ le_uint16_t flags = 0; // Used by PSO at runtime, unused in DAT file
    /* 06 */ le_uint16_t event_type = 0;
    /* 08 */ le_uint16_t room = 0;
    /* 0A */ le_uint16_t wave_number = 0;
    /* 0C */ le_uint16_t min_delay = 0;
    /* 0E */ le_uint16_t max_delay = 0;
    /* 10 */ uint8_t min_enemies = 0;
    /* 11 */ uint8_t max_enemies = 0;
    /* 12 */ le_uint16_t max_waves = 0;
    /* 14 */ le_uint32_t action_stream_offset = 0;
    /* 18 */

    std::string str() const;
  } __packed_ws__(Event2Entry, 0x18);

  struct RandomEnemyLocationsHeader { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint32_t room_table_offset; // Offset to RandomEnemyLocationSegment structs, from start of this struct
    /* 04 */ le_uint32_t entries_offset; // Offset to RandomEnemyLocationEntry structs, from start of this struct
    /* 08 */ le_uint32_t num_rooms;
    /* 0C */
  } __packed_ws__(RandomEnemyLocationsHeader, 0x0C);

  struct RandomEnemyLocationSection { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ le_uint16_t room;
    /* 02 */ le_uint16_t count;
    /* 04 */ le_uint32_t offset;
    /* 08 */
  } __packed_ws__(RandomEnemyLocationSection, 8);

  struct RandomEnemyLocationEntry { // Section type 4 (RANDOM_ENEMY_LOCATIONS)
    /* 00 */ VectorXYZF pos;
    /* 0C */ VectorXYZI angle;
    /* 18 */ le_uint16_t unknown_a9; // TODO: Verify these are actually little-endian
    /* 1A */ le_uint16_t unknown_a10; // TODO: Verify these are actually little-endian
    /* 1C */

    std::string str() const;
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
    // EnemySetEntry. Note that the order of param6 and param7 is switched!
    /* 00 */ le_float param1;
    /* 04 */ le_float param2;
    /* 08 */ le_float param3;
    /* 0C */ le_float param4;
    /* 10 */ le_float param5;
    /* 14 */ le_int16_t param7;
    /* 16 */ le_int16_t param6;
    /* 18 */ le_uint32_t entry_num;
    /* 1C */ le_uint16_t min_children;
    /* 1E */ le_uint16_t max_children;
    /* 20 */

    std::string str() const;
  } __packed_ws__(RandomEnemyDefinition, 0x20);

  struct RandomEnemyWeight { // Section type 5 (RANDOM_ENEMY_DEFINITIONS)
    /* 00 */ uint8_t base_type_index;
    /* 01 */ uint8_t def_entry_num;
    /* 02 */ uint8_t weight;
    /* 03 */ uint8_t unknown_a4;
    /* 04 */

    std::string str() const;
  } __packed_ws__(RandomEnemyWeight, 4);

  struct RandomState {
    PSOV2Encryption random;
    PSOV2Encryption location_table_random;
    std::array<uint32_t, 0x20> location_index_table;
    uint32_t location_indexes_populated;
    uint32_t location_indexes_used;
    uint32_t location_entries_base_offset;

    explicit RandomState(uint32_t random_seed);
    size_t rand_int_biased(size_t min_v, size_t max_v);
    uint32_t next_location_index();
    void generate_shuffled_location_table(const RandomEnemyLocationsHeader& header, phosg::StringReader r, uint16_t room);
  };

  struct FloorSections {
    size_t object_sets_file_offset = 0;
    size_t object_sets_file_size = 0;
    const ObjectSetEntry* object_sets = nullptr;
    size_t object_set_count = 0;
    size_t first_object_set_index = 0;

    size_t enemy_sets_file_offset = 0;
    size_t enemy_sets_file_size = 0;
    const EnemySetEntry* enemy_sets = nullptr;
    size_t enemy_set_count = 0;
    size_t first_enemy_set_index = 0;

    size_t events_file_offset = 0;
    size_t events_file_size = 0;
    const void* events_data = nullptr;
    size_t events_data_size = 0;
    const Event1Entry* events1 = nullptr;
    const Event2Entry* events2 = nullptr;
    size_t event_count = 0;
    const void* event_action_stream = nullptr;
    size_t event_action_stream_bytes = 0;
    size_t first_event_set_index = 0;

    size_t random_enemy_locations_file_offset = 0;
    size_t random_enemy_locations_file_size = 0;
    const void* random_enemy_locations_data = nullptr;
    size_t random_enemy_locations_data_size = 0;

    size_t random_enemy_definitions_file_offset = 0;
    size_t random_enemy_definitions_file_size = 0;
    const void* random_enemy_definitions_data = nullptr;
    size_t random_enemy_definitions_data_size = 0;
  };

  // Quest constructor
  MapFile(std::shared_ptr<const std::string> quest_data);
  // Non-quest constructor
  MapFile(
      uint8_t floor,
      std::shared_ptr<const std::string> objects_data,
      std::shared_ptr<const std::string> enemy_sets_data,
      std::shared_ptr<const std::string> events_data);
  // Constructor for materialize_random_sections
  explicit MapFile(uint32_t random_seed);
  ~MapFile() = default;

  inline uint64_t source_hash() const {
    return this->linked_data_hash;
  }

  inline std::shared_ptr<const std::string> get_quest_data() const {
    return this->quest_data;
  }

  inline int64_t random_seed() const { // Returns -1 if not randomized or no randomization required
    return this->generated_with_random_seed;
  }
  inline bool has_random_sections() const {
    return this->has_any_random_sections;
  }

  // If the map file has no random sections, does nothing and returns a
  // shared_ptr to this. If it has any random sections, returns a new map with
  // all non-random sections copied verbatim, and random sections replaced with
  // non-random sections according to the challenge mode generation algorithm.
  std::shared_ptr<MapFile> materialize_random_sections(uint32_t random_seed);
  std::shared_ptr<const MapFile> materialize_random_sections(uint32_t random_seed) const;

  inline const FloorSections& floor(uint8_t floor) const {
    return this->sections_for_floor.at(floor);
  }
  inline FloorSections& floor(uint8_t floor) {
    return this->sections_for_floor.at(floor);
  }

  size_t count_object_sets() const;
  size_t count_enemy_sets() const;
  size_t count_events() const;

  static std::string disassemble_action_stream(const void* data, size_t size);
  std::string disassemble(bool reassembly = false, Version version = Version::UNKNOWN) const;

protected:
  void link_data(std::shared_ptr<const std::string> data);

  void set_object_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size);
  void set_enemy_sets_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size);
  void set_events_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size, bool allow_evt2);
  void set_random_enemy_locations_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size);
  void set_random_enemy_definitions_for_floor(uint8_t floor, size_t file_offset, const void* data, size_t size);

  void compute_floor_start_indexes();

  std::shared_ptr<const std::string> quest_data;
  std::unordered_set<std::shared_ptr<const std::string>> linked_data;
  std::array<FloorSections, 0x12> sections_for_floor;
  uint64_t linked_data_hash = 0;
  bool has_any_random_sections = false;
  int64_t generated_with_random_seed = -1;
};

////////////////////////////////////////////////////////////////////////////////
// Super map. This class is responsible for collecting entity lists across PSO
// versions and diffing them to link together entities that don't line up
// across versions. This class also generates enemy lists from enemy set lists,
// which MapFile doesn't do. Like MapFile, a single SuperMap is either
// responsible for all entities on all floors in a quest, or all entities on a
// single floor in free play. Each entity is assigned a "super ID", which
// uniquely identifies the entity on all PSO versions. (These are the IDs which
// newserv formats as K-XXX, E-XXX, and W-XXX, though they are offset as needed
// for floors beyond the first.)
// There must not be any random enemy sections in any MapFile passed to
// SuperMap; to resolve them, materialize_random_sections must be called on all
// MapFiles first. This generally only is of concern in Challenge mode.

class SuperMap {
public:
  struct Object {
    struct ObjectVersion {
      const MapFile::ObjectSetEntry* set_entry = nullptr;
      uint16_t relative_object_index = 0xFFFF;
    };
    std::array<ObjectVersion, NUM_VERSIONS> def_for_version;
    size_t super_id = 0;
    uint8_t floor = 0xFF;

    inline ObjectVersion& version(Version v) {
      return this->def_for_version.at(static_cast<size_t>(v));
    }
    inline const ObjectVersion& version(Version v) const {
      return this->def_for_version.at(static_cast<size_t>(v));
    }

    std::string id_str() const;
    std::string str() const;
  };

  struct Enemy {
    struct EnemyVersion {
      const MapFile::EnemySetEntry* set_entry = nullptr;
      uint16_t relative_enemy_index = 0xFFFF;
      uint16_t relative_set_index = 0xFFFF;
    };
    std::array<EnemyVersion, NUM_VERSIONS> def_for_version;
    size_t super_id = 0;
    size_t super_set_id = 0;
    uint16_t child_index = 0;
    int16_t alias_enemy_index_delta = 0; // 0 = no alias
    EnemyType type = EnemyType::UNKNOWN;
    bool is_default_rare_v123 = false;
    bool is_default_rare_bb = false;
    uint8_t floor = 0xFF;

    inline EnemyVersion& version(Version v) {
      return this->def_for_version.at(static_cast<size_t>(v));
    }
    inline const EnemyVersion& version(Version v) const {
      return this->def_for_version.at(static_cast<size_t>(v));
    }

    std::string id_str() const;
    std::string str() const;
  };

  struct Event {
    struct EventVersion {
      const MapFile::Event1Entry* set_entry = nullptr;
      uint16_t relative_event_index = 0xFFFF;
      const void* action_stream = nullptr;
      size_t action_stream_size = 0;
    };
    size_t super_id = 0;
    uint8_t floor = 0;
    std::array<EventVersion, NUM_VERSIONS> def_for_version;

    inline EventVersion& version(Version v) {
      return this->def_for_version.at(static_cast<size_t>(v));
    }
    inline const EventVersion& version(Version v) const {
      return this->def_for_version.at(static_cast<size_t>(v));
    }

    std::string id_str() const;
    std::string str() const;
  };

  struct EntitiesForVersion {
    std::shared_ptr<const MapFile> map_file;

    // Entity lists (matching those used in-game)
    std::vector<std::shared_ptr<Object>> objects;
    std::array<size_t, 0x12> object_floor_start_indexes = {};
    std::vector<std::shared_ptr<Enemy>> enemies;
    std::array<size_t, 0x12> enemy_floor_start_indexes = {};
    std::vector<std::shared_ptr<Enemy>> enemy_sets; // Like .enemies, but only includes canonical enemies (no children)
    std::array<size_t, 0x12> enemy_set_floor_start_indexes = {};
    std::vector<std::shared_ptr<Event>> events;
    std::array<size_t, 0x12> event_floor_start_indexes = {};

    // Indexes
    std::unordered_multimap<uint64_t, std::shared_ptr<Object>> object_for_floor_room_and_group;
    std::unordered_multimap<uint64_t, std::shared_ptr<Object>> door_for_floor_and_switch_flag;
    std::unordered_multimap<uint64_t, std::shared_ptr<Enemy>> enemy_for_floor_room_and_wave_number;
    std::unordered_multimap<uint64_t, std::shared_ptr<Event>> event_for_floor_room_and_wave_number;
    std::multimap<uint64_t, std::shared_ptr<Event>> event_for_floor_and_event_id;
  };

  SuperMap(Episode episode, const std::array<std::shared_ptr<const MapFile>, NUM_VERSIONS>& map_files);
  ~SuperMap() = default;

  inline EntitiesForVersion& version(Version v) {
    return this->entities_for_version.at(static_cast<size_t>(v));
  }
  inline const EntitiesForVersion& version(Version v) const {
    return this->entities_for_version.at(static_cast<size_t>(v));
  }

  inline Episode get_episode() const {
    return this->episode;
  }
  inline int64_t get_random_seed() const {
    return this->random_seed;
  }
  inline const std::vector<std::shared_ptr<Object>>& all_objects() const {
    return this->objects;
  }
  inline const std::vector<std::shared_ptr<Enemy>>& all_enemies() const {
    return this->enemies;
  }
  inline const std::vector<std::shared_ptr<Enemy>>& all_enemy_sets() const {
    return this->enemy_sets;
  }
  inline const std::vector<std::shared_ptr<Event>>& all_events() const {
    return this->events;
  }

  std::vector<std::shared_ptr<const Object>> objects_for_floor_room_group(
      Version version, uint8_t floor, uint16_t room, uint16_t group) const;
  std::vector<std::shared_ptr<const Object>> doors_for_switch_flag(
      Version version, uint8_t floor, uint8_t switch_flag) const;

  std::shared_ptr<const Enemy> enemy_for_index(Version version, uint16_t enemy_index, bool follow_alias) const;
  std::shared_ptr<const Enemy> enemy_for_floor_type(Version version, uint8_t floor, EnemyType type) const;
  std::vector<std::shared_ptr<const Enemy>> enemies_for_floor_room_wave(
      Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const;

  std::vector<std::shared_ptr<const Event>> events_for_id(Version version, uint8_t floor, uint32_t event_id) const;
  std::vector<std::shared_ptr<const Event>> events_for_floor(Version version, uint8_t floor) const;
  std::vector<std::shared_ptr<const Event>> events_for_floor_room_wave(
      Version version, uint8_t floor, uint16_t room, uint16_t wave_number) const;

  std::unordered_map<EnemyType, size_t> count_enemy_sets_for_version(Version version) const;

  struct EfficiencyStats {
    size_t filled_object_slots = 0;
    size_t total_object_slots = 0;
    size_t filled_enemy_set_slots = 0;
    size_t total_enemy_set_slots = 0;
    size_t filled_event_slots = 0;
    size_t total_event_slots = 0;

    EfficiencyStats& operator+=(const EfficiencyStats& other);
    std::string str() const;
  };
  EfficiencyStats efficiency() const;
  void verify() const;
  void print(FILE* stream) const;

protected:
  phosg::PrefixedLogger log;

  Episode episode;
  int64_t random_seed = -1;
  std::vector<std::shared_ptr<Object>> objects;
  std::vector<std::shared_ptr<Enemy>> enemies;
  std::vector<std::shared_ptr<Enemy>> enemy_sets;
  std::vector<std::shared_ptr<Event>> events;
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<Object>>> objects_for_semantic_hash;
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<Enemy>>> enemy_sets_for_semantic_hash;
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<Event>>> events_for_semantic_hash;
  std::array<EntitiesForVersion, NUM_VERSIONS> entities_for_version;

  std::shared_ptr<Object> add_object(Version version, uint8_t floor, const MapFile::ObjectSetEntry* set_entry);
  void link_object_version(std::shared_ptr<Object> obj, Version version, const MapFile::ObjectSetEntry* set_entry);
  std::shared_ptr<Enemy> add_enemy_and_children(Version version, uint8_t floor, const MapFile::EnemySetEntry* set_entry);
  void link_enemy_version_and_children(std::shared_ptr<Enemy> ene, Version version, const MapFile::EnemySetEntry* set_entry);
  std::shared_ptr<Event> add_event(
      Version version,
      uint8_t floor,
      const MapFile::Event1Entry* entry,
      const void* map_file_action_stream,
      size_t map_file_action_stream_size);
  void link_event_version(
      std::shared_ptr<Event> ev,
      Version version,
      const MapFile::Event1Entry* entry,
      const void* map_file_action_stream,
      size_t map_file_action_stream_size);

  void add_map_file(Version v, std::shared_ptr<const MapFile> this_map_file);
};

////////////////////////////////////////////////////////////////////////////////
// Map state. This class is responsible for keeping track of the in-game state
// of objects, enemies, and events. This is the only class that's constructed
// for every game; the others are essentially immutable data once loaded, which
// this class refers to.

class MapState {
public:
  struct RareEnemyRates {
    static constexpr uint32_t DEFAULT_RARE_ENEMY_RATE_V1_V2 = 0x00418937; // 1/1000
    static constexpr uint32_t DEFAULT_RARE_ENEMY_RATE_V3 = 0x0083126E; // 1/500
    static constexpr uint32_t DEFAULT_MERICARAND_RATE_V3 = 0x33333333; // 1/5
    static constexpr uint32_t DEFAULT_RARE_BOSS_RATE_V4 = 0x1999999A; // 1/10

    uint32_t hildeblue; // HILDEBEAR -> HILDEBLUE
    uint32_t rappy; // RAG_RAPPY -> {AL_RAPPY or seasonal rappies}; SAND_RAPPY -> DEL_RAPPY
    uint32_t nar_lily; // POISON_LILY -> NAR_LILY
    uint32_t pouilly_slime; // POFUILLY_SLIME -> POUILLY_SLIME
    uint32_t mericarand; // MERICARAND -> MERIKLE or MERICUS (only for those with subtype > 2)
    uint32_t merissa_aa; // MERISSA_A -> MERISSA_AA
    uint32_t pazuzu; // ZU -> PAZUZU (and _ALT variants)
    uint32_t dorphon_eclair; // DORPHON -> DORPHON_ECLAIR
    uint32_t kondrieu; // {SAINT_MILION, SHAMBERTIN} -> KONDRIEU

    RareEnemyRates(uint32_t enemy_rate, uint32_t mericarand_rate, uint32_t boss_rate);
    explicit RareEnemyRates(const phosg::JSON& json);

    uint32_t get(EnemyType type) const;

    std::string str() const;
    phosg::JSON json() const;
  };

  static const std::shared_ptr<const RareEnemyRates> NO_RARE_ENEMIES;
  static const std::shared_ptr<const RareEnemyRates> DEFAULT_RARE_ENEMIES;

  struct ObjectState {
    // WARNING: super_obj CAN BE NULL! This is not the case for enemies and
    // events; their super entities are never null. In the case of objects,
    // dynamic objects like player-set traps have object IDs past the end of
    // the map's object list, and when queried, the MapState will return a
    // temporary ObjectState with a null super_obj. (In these cases, only k_id
    // is needed for correctness.)
    std::shared_ptr<const SuperMap::Object> super_obj;
    size_t k_id = 0;
    uint16_t game_flags = 0;
    uint16_t set_flags = 0;
    bool item_drop_checked = false;

    inline void reset() {
      this->game_flags = 0;
      this->set_flags = 0;
      this->item_drop_checked = false;
    }

    inline std::string type_name(Version v, uint8_t area = 0xFF) const {
      if (!this->super_obj) {
        return "<DYNAMIC>";
      }
      return MapFile::name_for_object_type(this->super_obj->version(v).set_entry->base_type, v, area);
    }
  };

  struct EnemyState {
    std::shared_ptr<const SuperMap::Enemy> super_ene;
    enum Flag {
      LAST_HIT_MASK = 0x0003,
      EXP_GIVEN = 0x0004,
      ITEM_DROPPED = 0x0008,
      ALL_HITS_MASK_FIRST = 0x0010,
      ALL_HITS_MASK = 0x00F0,
    };
    size_t e_id = 0;
    size_t set_id = 0;
    uint32_t game_flags = 0; // From 6x0A
    uint16_t total_damage = 0;
    uint16_t rare_flags = 0;
    uint16_t mericarand_variant_flags = 0;
    uint16_t set_flags = 0; // Only used if super_ene->child_index == 0
    uint16_t server_flags = 0;

    inline void reset() {
      this->total_damage = 0;
      this->rare_flags = 0;
      this->game_flags = 0;
      this->set_flags = 0;
      this->server_flags = 0;
    }

    inline bool is_rare(Version version) const {
      return (((this->rare_flags >> static_cast<size_t>(version)) & 1) ||
          ((version == Version::BB_V4) ? this->super_ene->is_default_rare_bb : this->super_ene->is_default_rare_v123));
    }
    inline void set_rare(Version version) {
      this->rare_flags |= (1 << static_cast<size_t>(version));
    }
    inline void set_mericarand_variant_flag(Version version) {
      this->mericarand_variant_flags |= (1 << static_cast<size_t>(version));
    }
    inline EnemyType type(Version version, Episode episode, uint8_t event) const {
      if (this->super_ene->type == EnemyType::MERICARAND) {
        if (this->is_rare(version)) {
          return ((this->mericarand_variant_flags >> static_cast<size_t>(version)) & 1)
              ? EnemyType::MERIKLE
              : EnemyType::MERICUS;
        } else {
          return EnemyType::MERICAROL;
        }
      } else {
        return this->is_rare(version)
            ? type_definition_for_enemy(this->super_ene->type).rare_type(episode, event, this->super_ene->floor)
            : this->super_ene->type;
      }
    }
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

  struct EventState {
    std::shared_ptr<const SuperMap::Event> super_ev;
    size_t w_id = 0;
    uint16_t flags = 0;

    inline void reset() {
      this->flags = 0;
    }
  };

  class EntityIterator {
  public:
    EntityIterator(MapState* map_state, Version version, bool at_end);
    void prepare();

    void advance();

    EntityIterator& operator++();

    bool operator==(const EntityIterator& other) const;
    bool operator!=(const EntityIterator& other) const;

  protected:
    virtual size_t num_entities_on_current_floor() const = 0;

    MapState* map_state;
    Version version;
    size_t floor;
    size_t relative_index;
  };

  struct FloorConfig {
    struct EntityBaseIndexes {
      size_t base_object_index = 0;
      size_t base_enemy_index = 0;
      size_t base_enemy_set_index = 0;
      size_t base_event_index = 0;
    };
    std::shared_ptr<const SuperMap> super_map;
    std::array<EntityBaseIndexes, NUM_VERSIONS> indexes;
    EntityBaseIndexes base_super_ids;

    EntityBaseIndexes& base_indexes_for_version(Version v) {
      return this->indexes.at(static_cast<size_t>(v));
    }
    const EntityBaseIndexes& base_indexes_for_version(Version v) const {
      return this->indexes.at(static_cast<size_t>(v));
    }
  };

  class ObjectIterator : public EntityIterator {
  public:
    using EntityIterator::EntityIterator;
    std::shared_ptr<ObjectState>& operator*();

  protected:
    virtual size_t num_entities_on_current_floor() const;
  };

  class EnemyIterator : public EntityIterator {
  public:
    using EntityIterator::EntityIterator;
    std::shared_ptr<EnemyState>& operator*();

  protected:
    virtual size_t num_entities_on_current_floor() const;
  };

  class EnemySetIterator : public EntityIterator {
  public:
    using EntityIterator::EntityIterator;
    std::shared_ptr<EnemyState>& operator*();

  protected:
    virtual size_t num_entities_on_current_floor() const;
  };

  class EventIterator : public EntityIterator {
  public:
    using EntityIterator::EntityIterator;
    std::shared_ptr<EventState>& operator*();

  protected:
    virtual size_t num_entities_on_current_floor() const;
  };

  template <typename IteratorT>
  struct Range {
    MapState* map_state;
    Version version;

    IteratorT begin() {
      auto ret = IteratorT(this->map_state, this->version, false);
      ret.prepare();
      return ret;
    }
    IteratorT end() {
      return IteratorT(this->map_state, this->version, true);
    }
  };

  phosg::PrefixedLogger log;
  std::vector<FloorConfig> floor_config_entries;
  uint8_t difficulty = 0;
  uint8_t event = 0;
  uint32_t random_seed = 0;
  std::shared_ptr<const RareEnemyRates> bb_rare_rates;
  std::vector<std::shared_ptr<ObjectState>> object_states;
  std::vector<std::shared_ptr<EnemyState>> enemy_states;
  std::vector<std::shared_ptr<EnemyState>> enemy_set_states;
  std::vector<std::shared_ptr<EventState>> event_states;
  std::vector<size_t> bb_rare_enemy_indexes;
  size_t dynamic_obj_base_k_id = 0;
  std::array<size_t, NUM_VERSIONS> dynamic_obj_base_index_for_version = {};

  // Constructor for free play
  MapState(
      uint64_t lobby_or_session_id,
      uint8_t difficulty,
      uint8_t event,
      uint32_t random_seed, // For client-matched rare enemies (non-BB)
      std::shared_ptr<const RareEnemyRates> bb_rare_rates,
      std::shared_ptr<RandomGenerator> rand_crypt,
      std::vector<std::shared_ptr<const SuperMap>> floor_map_defs);
  // Constructor for quests
  MapState(
      uint64_t lobby_or_session_id,
      uint8_t difficulty,
      uint8_t event,
      uint32_t random_seed, // For client-matched rare enemies (non-BB)
      std::shared_ptr<const RareEnemyRates> bb_rare_rates,
      std::shared_ptr<RandomGenerator> rand_crypt,
      std::shared_ptr<const SuperMap> quest_map_def);
  // Constructor for empty maps (used in challenge mode before a quest starts)
  MapState();

  ~MapState() = default;

  void index_super_map(const FloorConfig& floor_config, std::shared_ptr<RandomGenerator> rand_crypt);
  void compute_dynamic_object_base_indexes();

  inline FloorConfig& floor_config(uint8_t floor) {
    return this->floor_config_entries[std::min<uint8_t>(floor, this->floor_config_entries.size() - 1)];
  }
  inline const FloorConfig& floor_config(uint8_t floor) const {
    return this->floor_config_entries[std::min<uint8_t>(floor, this->floor_config_entries.size() - 1)];
  }
  // Resets states of all entities to their initial values. Used when
  // restarting battles/challenges.
  void reset();

  inline Range<ObjectIterator> iter_object_states(Version version) {
    return Range<ObjectIterator>{.map_state = this, .version = version};
  }
  inline Range<EnemyIterator> iter_enemy_states(Version version) {
    return Range<EnemyIterator>{.map_state = this, .version = version};
  }
  inline Range<EnemySetIterator> iter_enemy_set_states(Version version) {
    return Range<EnemySetIterator>{.map_state = this, .version = version};
  }
  inline Range<EventIterator> iter_event_states(Version version) {
    return Range<EventIterator>{.map_state = this, .version = version};
  }

  uint16_t index_for_object_state(Version version, std::shared_ptr<const ObjectState> obj_st) const;
  uint16_t index_for_enemy_state(Version version, std::shared_ptr<const EnemyState> ene_st) const;
  uint16_t set_index_for_enemy_state(Version version, std::shared_ptr<const EnemyState> ene_st) const;
  uint16_t index_for_event_state(Version version, std::shared_ptr<const EventState> evt_st) const;

  std::shared_ptr<ObjectState> object_state_for_index(Version version, uint8_t floor, uint16_t object_index);
  std::vector<std::shared_ptr<ObjectState>> object_states_for_floor_room_group(
      Version version, uint8_t floor, uint16_t room, uint16_t group);
  std::vector<std::shared_ptr<ObjectState>> door_states_for_switch_flag(
      Version version, uint8_t floor, uint8_t switch_flag);

  std::shared_ptr<EnemyState> enemy_state_for_index(Version version, uint8_t floor, uint16_t enemy_index);
  std::shared_ptr<EnemyState> enemy_state_for_set_index(Version version, uint8_t floor, uint16_t enemy_set_index);
  std::shared_ptr<EnemyState> enemy_state_for_floor_type(Version version, uint8_t floor, EnemyType type);
  std::vector<std::shared_ptr<EnemyState>> enemy_states_for_floor_room_wave(
      Version version, uint8_t floor, uint16_t room, uint16_t wave_number);

  std::shared_ptr<EventState> event_state_for_index(Version version, uint8_t floor, uint16_t event_index);
  std::vector<std::shared_ptr<EventState>> event_states_for_id(Version version, uint8_t floor, uint32_t event_id);
  std::vector<std::shared_ptr<EventState>> event_states_for_floor(Version version, uint8_t floor);
  std::vector<std::shared_ptr<EventState>> event_states_for_floor_room_wave(
      Version version, uint8_t floor, uint16_t room, uint16_t wave_number);

  void import_object_states_from_sync(Version from_version, const SyncObjectStateEntry* entries, size_t entry_count);
  void import_enemy_states_from_sync(Version from_version, const SyncEnemyStateEntry* entries, size_t entry_count);
  void import_flag_states_from_sync(
      Version from_version,
      const le_uint16_t* object_set_flags,
      size_t object_set_flags_count,
      const le_uint16_t* enemy_set_flags,
      size_t enemy_set_flags_count,
      const le_uint16_t* event_flags,
      size_t event_flags_count);

  void verify() const;
  void print(FILE* stream) const;
};

struct RoomLayoutIndex {
  struct Room {
    VectorXYZF position;
    VectorXYZI angle;
  };
  std::unordered_map<uint64_t, Room> rooms;

  explicit RoomLayoutIndex(const phosg::JSON& json);
  const Room& get_room(uint8_t area, uint8_t major_var, uint32_t room_id) const;
};
