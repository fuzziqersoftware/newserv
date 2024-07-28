#pragma once

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>

#include "GSLArchive.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Types.hh"

class CommonItemSet {
public:
  class Table {
  public:
    Table() = delete;
    Table(const phosg::JSON& json, Episode episode);
    Table(const phosg::StringReader& r, bool big_endian, bool is_v3, Episode episode);

    template <typename IntT>
    struct Range {
      IntT min;
      IntT max;
    } __packed__;

    Episode episode;
    parray<uint8_t, 0x0C> base_weapon_type_prob_table;
    parray<int8_t, 0x0C> subtype_base_table;
    parray<uint8_t, 0x0C> subtype_area_length_table;
    parray<parray<uint8_t, 4>, 9> grind_prob_table;
    parray<uint8_t, 0x05> armor_shield_type_index_prob_table;
    parray<uint8_t, 0x05> armor_slot_count_prob_table;
    parray<Range<uint16_t>, 0x64> enemy_meseta_ranges;
    parray<uint8_t, 0x64> enemy_type_drop_probs;
    parray<uint8_t, 0x64> enemy_item_classes;
    parray<Range<uint16_t>, 0x0A> box_meseta_ranges;
    bool has_rare_bonus_value_prob_table;
    parray<parray<uint16_t, 6>, 0x17> bonus_value_prob_table;
    parray<parray<uint8_t, 10>, 3> nonrare_bonus_prob_spec;
    parray<parray<uint8_t, 10>, 6> bonus_type_prob_table;
    parray<uint8_t, 0x0A> special_mult;
    parray<uint8_t, 0x0A> special_percent;
    parray<parray<uint16_t, 0x0A>, 0x1C> tool_class_prob_table;
    parray<parray<uint8_t, 0x0A>, 0x13> technique_index_prob_table;
    parray<parray<Range<uint8_t>, 0x0A>, 0x13> technique_level_ranges;
    uint8_t armor_or_shield_type_bias;
    parray<uint8_t, 0x0A> unit_max_stars_table;
    parray<parray<uint8_t, 10>, 7> box_item_class_prob_table;

    phosg::JSON json() const;
    void print(FILE* stream) const;

  private:
    template <bool BE>
    void parse_itempt_t(const phosg::StringReader& r, bool is_v3);

    template <bool BE>
    struct OffsetsT {
      // This data structure uses index probability tables in multiple places. An
      // index probability table is a table where each entry holds the probability
      // that that entry's index is used. For example, if the armor slot count
      // probability table contains [77, 17, 5, 1, 0], this means there is a 77%
      // chance of no slots, 17% chance of 1 slot, 5% chance of 2 slots, 1% chance
      // of 3 slots, and no chance of 4 slots. The values in index probability
      // tables do not have to add up to 100; the game sums all of them and
      // chooses a random number less than that maximum.

      // The area (floor) number is used in many places as well. Unlike the normal
      // area numbers, which start with Pioneer 2, the area numbers in this
      // structure start with Forest 1, and boss areas are treated as the first
      // area of the next section (so De Rol Le has Mines 1 drops, for example).
      // Final boss areas are treated as the last non-boss area (so Dark Falz
      // boxes are like Ruins 3 boxes). We refer to these adjusted area numbers as
      // (area - 1).

      // This index probability table determines the types of non-rare weapons.
      // The indexes in this table correspond to the non-rare weapon types 01
      // through 0C (Saber through Wand).
      // V2/V3: -> parray<uint8_t, 0x0C>
      /* 00 */ U32T<BE> base_weapon_type_prob_table_offset;

      // This table specifies the base subtype for each weapon type. Negative
      // values here mean that the weapon cannot be found in the first N areas (so
      // -2, for example, means that the weapon never appears in Forest 1 or 2 at
      // all). Nonnegative values here mean the subtype can be found in all areas,
      // and specify the base subtype (usually in the range [0, 4]). The subtype
      // of weapon that actually appears depends on this value and a value from
      // the following table.
      // V2/V3: -> parray<int8_t, 0x0C>
      /* 04 */ U32T<BE> subtype_base_table_offset;

      // This table specifies how many areas each weapon subtype appears in. For
      // example, if Sword (subtype 02, which is index 1 in this table and the
      // table above) has a subtype base of -2 and a subtype area length of 4,
      // then Sword items can be found when area - 1 is 2, 3, 4, or 5 (Cave 1
      // through Mine 1), and Gigush (the next sword subtype) can be found in Mine
      // 1 through Ruins 3.
      // V2/V3: -> parray<uint8_t, 0x0C>
      /* 08 */ U32T<BE> subtype_area_length_table_offset;

      // This index probability table specifies how likely each possible grind
      // value is. The table is indexed as [grind][subtype_area_index], where the
      // subtype area index is how many areas the player is beyond the first area
      // in which the subtype can first be found (clamped to [0, 3]). To continue
      // the example above, in Cave 3, subtype_area_index would be 2, since Swords
      // can first be found two areas earlier in Cave 1.
      // For example, this table could look like this:
      //   [64 1E 19 14] // Chance of getting a grind +0
      //   [00 1E 17 0F] // Chance of getting a grind +1
      //   [00 14 14 0E] // Chance of getting a grind +2
      //    ...
      //    C1 C2 C3 M1  // (Episode 1 area values from the example for reference)
      // V2/V3: -> parray<parray<uint8_t, 4>, 9>
      /* 0C */ U32T<BE> grind_prob_table_offset;

      // TODO: Figure out exactly how this table is used. Anchor: 80106D34
      // V2/V3: -> parray<uint8_t, 0x05>
      /* 10 */ U32T<BE> armor_shield_type_index_prob_table_offset;

      // This index probability table specifies how common each possible slot
      // count is for armor drops.
      // V2/V3: -> parray<uint8_t, 0x05>
      /* 14 */ U32T<BE> armor_slot_count_prob_table_offset;

      // This array (indexed by enemy_type) specifies the range of meseta values
      // that each enemy can drop.
      // V2/V3: -> parray<Range<U16T>, 0x64>
      /* 18 */ U32T<BE> enemy_meseta_ranges_offset;

      // Each byte in this table (indexed by enemy_type) represents the percent
      // chance that the enemy drops anything at all. (This check is done before
      // the rare drop check, so the chance of getting a rare item from an enemy
      // is essentially this probability multiplied by the rare drop rate.)
      // V2/V3: -> parray<uint8_t, 0x64>
      /* 1C */ U32T<BE> enemy_type_drop_probs_offset;

      // Each byte in this table (indexed by enemy_type) represents the class of
      // item that the enemy can drop. The values are:
      // 00 = weapon
      // 01 = armor
      // 02 = shield
      // 03 = unit
      // 04 = tool
      // 05 = meseta
      // Anything else = no item
      // V2/V3: -> parray<uint8_t, 0x64>
      /* 20 */ U32T<BE> enemy_item_classes_offset;

      // This table (indexed by area - 1) specifies the ranges of meseta values
      // that can drop from boxes.
      // V2/V3: -> parray<Range<U16T>, 0x0A>
      /* 24 */ U32T<BE> box_meseta_ranges_offset;

      // This array specifies the chance that a rare weapon will have each
      // possible bonus value. This is indexed as [(bonus_value - 10 / 5)][spec],
      // so the first row refers the probability of getting a -10% bonus, the next
      // row is the chance of getting -5%, etc., all the way up to +100%. For
      // non-rare items, spec is determined randomly based on the following field;
      // for rare items, spec is always 5.
      // V2: -> parray<parray<uint8_t, 5>, 0x17>
      // V3: -> parray<parray<U16T, 6>, 0x17>
      /* 28 */ U32T<BE> bonus_value_prob_table_offset;

      // This array specifies the value of spec to be used in the above lookup for
      // non-rare items. This is NOT an index probability table; this is a direct
      // lookup with indexes [bonus_index][area - 1]. A value of 0xFF in any byte
      // of this array prevents any weapon from having a bonus in that slot.
      // For example, the array might look like this:
      //   [00 00 00 01 01 01 01 02 02 02]
      //   [FF FF FF 00 00 00 01 01 01 01]
      //   [FF FF FF FF FF FF FF FF FF 00]
      //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
      // In this example, spec is 0, 1, or 2 in all cases where a weapon can have
      // a bonus. In Forest 1 and 2 and Cave 1, weapons may have at most one
      // bonus; in all other areas except Ruins 3, they can have at most two
      // bonuses, and in Ruins 3, they can have up to three bonuses.
      // V2/V3: // -> parray<parray<uint8_t, 10>, 3>
      /* 2C */ U32T<BE> nonrare_bonus_prob_spec_offset;

      // This array specifies the chance that a weapon will have each bonus type.
      // The table is indexed as [bonus_type][area - 1] for non-rare items; for
      // rare items, a random value in the range [0, 9] is used instead of
      // (area - 1).
      // For example, the table might look like this:
      //   [46 46 3F 3E 3E 3D 3C 3C 3A 3A] // Chance of getting no bonus
      //   [14 14 0A 0A 09 02 02 04 05 05] // Chance of getting Native bonus
      //   [0A 0A 12 11 11 09 09 08 08 08] // Chance of getting A.Beast bonus
      //   [00 00 09 0A 0B 13 12 08 09 09] // Chance of getting Machine bonus
      //   [00 00 00 01 01 08 0A 13 13 13] // Chance of getting Dark bonus
      //   [00 00 00 00 00 01 01 01 01 01] // Chance of getting Hit bonus
      //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
      // V2/V3: -> parray<parray<uint8_t, 10>, 6>
      /* 30 */ U32T<BE> bonus_type_prob_table_offset;

      // This array (indexed by area - 1) specifies a multiplier of used in
      // special ability determination. It seems this uses the star values from
      // ItemPMT, but not yet clear exactly in what way.
      // TODO: Figure out exactly what this does. Anchor: 80106FEC
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 34 */ U32T<BE> special_mult_offset;

      // This array (indexed by area - 1) specifies the probability that any
      // non-rare weapon will have a special ability.
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 38 */ U32T<BE> special_percent_offset;

      // This index probability table is indexed by [tool_class][area - 1]. The
      // tool class refers to an entry in ItemPMT, which links it to the actual
      // item code.
      // V2/V3: -> parray<parray<U16T, 0x0A>, 0x1C>
      /* 3C */ U32T<BE> tool_class_prob_table_offset;

      // This index probability table determines how likely each technique is to
      // appear. The table is indexed as [technique_num][area - 1].
      // V2/V3: -> parray<parray<uint8_t, 0x0A>, 0x13>
      /* 40 */ U32T<BE> technique_index_prob_table_offset;

      // This table specifies the ranges for technique disk levels. The table is
      // indexed as [technique_num][area - 1]. If either min or max in the range
      // is 0xFF, or if max < min, technique disks are not dropped for that
      // technique and area pair.
      // V2/V3: -> parray<parray<Range<uint8_t>, 0x0A>, 0x13>
      /* 44 */ U32T<BE> technique_level_ranges_offset;

      /* 48 */ uint8_t armor_or_shield_type_bias;
      /* 49 */ parray<uint8_t, 3> unused1;

      // These values specify the maximum number of stars any generated unit can
      // have in each area. The values here are not inclusive; that is, a value
      // of 7 means that only units with 1-6 stars can drop in that area. The
      // game uniformly chooses a random number of stars in the acceptable
      // range, then uniformly chooses a random unit with that many stars.
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 4C */ U32T<BE> unit_max_stars_offset;

      // This index probability table determines which type of items drop from
      // boxes. The table is indexed as [item_class][area - 1], with item_class
      // as the result value (that is, in the example below, the game looks at a
      // single column and sums the values going down, then the chosen item
      // class is one of the row indexes based on the weight values in the
      // column.) The resulting item_class value has the same meaning as in
      // enemy_item_classes above.
      // For example, this array might look like the following:
      //   [07 07 08 08 06 07 08 09 09 0A] // Chances per area of a weapon drop
      //   [02 02 02 02 03 02 02 02 03 03] // Chances per area of an armor drop
      //   [02 02 02 02 03 02 02 02 03 03] // Chances per area of a shield drop
      //   [00 00 03 03 03 04 03 04 05 05] // Chances per area of a unit drop
      //   [11 11 12 12 12 12 12 12 12 12] // Chances per area of a tool drop
      //   [32 32 32 32 32 32 32 32 32 32] // Chances per area of a meseta drop
      //   [16 16 11 11 11 11 11 0F 0C 0B] // Chances per area of an empty box
      //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
      // V2/V3: -> parray<parray<uint8_t, 10>, 7>
      /* 50 */ U32T<BE> box_item_class_prob_table_offset;

      // There are several unused fields here.
    } __packed__;
    using Offsets = OffsetsT<false>;
    using OffsetsBE = OffsetsT<true>;
    check_struct_size(Offsets, 0x54);
    check_struct_size(OffsetsBE, 0x54);
  };

  std::shared_ptr<const Table> get_table(Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const;
  phosg::JSON json() const;
  void print(FILE* stream) const;

protected:
  CommonItemSet() = default;

  static uint16_t key_for_table(Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid);

  std::unordered_map<uint16_t, std::shared_ptr<Table>> tables;
};

class AFSV2CommonItemSet : public CommonItemSet {
public:
  AFSV2CommonItemSet(std::shared_ptr<const std::string> pt_afs_data, std::shared_ptr<const std::string> ct_afs_data);
};

class GSLV3V4CommonItemSet : public CommonItemSet {
public:
  GSLV3V4CommonItemSet(std::shared_ptr<const std::string> gsl_data, bool is_big_endian);
};

class JSONCommonItemSet : public CommonItemSet {
public:
  explicit JSONCommonItemSet(const phosg::JSON& json);
};

// Note: There are clearly better ways of doing this, but this implementation
// closely follows what the original code in the client does.
template <typename ItemT, size_t MaxCount>
struct ProbabilityTable {
  ItemT items[MaxCount];
  size_t count;

  ProbabilityTable() : count(0) {}

  void push(ItemT item) {
    if (this->count == MaxCount) {
      throw std::runtime_error("push to full probability table");
    }
    this->items[this->count++] = item;
  }

  ItemT pop() {
    if (this->count == 0) {
      throw std::runtime_error("pop from empty probability table");
    }
    return this->items[--this->count];
  }

  void shuffle(std::shared_ptr<PSOLFGEncryption> opt_rand_crypt) {
    for (size_t z = 1; z < this->count; z++) {
      size_t other_z = random_from_optional_crypt(opt_rand_crypt) % (z + 1);
      ItemT t = this->items[z];
      this->items[z] = this->items[other_z];
      this->items[other_z] = t;
    }
  }

  ItemT sample(std::shared_ptr<PSOLFGEncryption> opt_rand_crypt) const {
    if (this->count == 0) {
      throw std::runtime_error("sample from empty probability table");
    } else if (this->count == 1) {
      return this->items[0];
    } else {
      return this->items[random_from_optional_crypt(opt_rand_crypt) % this->count];
    }
  }
};

class RELFileSet {
public:
  template <typename ValueT, typename WeightT = ValueT>
  struct WeightTableEntry {
    ValueT value;
    WeightT weight;
  } __packed__;

  using WeightTableEntry8 = WeightTableEntry<uint8_t>;
  using WeightTableEntry32 = WeightTableEntry<be_uint32_t>;
  check_struct_size(WeightTableEntry8, 2);
  check_struct_size(WeightTableEntry32, 8);

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;

  struct TableSpec {
    be_uint32_t offset;
    uint8_t entries_per_table;
    parray<uint8_t, 3> unused;
  } __packed_ws__(TableSpec, 8);

  RELFileSet(std::shared_ptr<const std::string> data);

  template <typename T>
  std::pair<const T*, size_t> get_table(
      const TableSpec& spec, size_t index) const {
    const T* entries = &r.pget<T>(
        spec.offset + index * spec.entries_per_table * sizeof(T),
        spec.entries_per_table * sizeof(T));
    return std::make_pair(entries, spec.entries_per_table);
  }
};

class ArmorRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from ArmorRandom.rel
  ArmorRandomSet(std::shared_ptr<const std::string> data);

  std::pair<const WeightTableEntry8*, size_t> get_armor_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_shield_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_unit_table(size_t index) const;

private:
  const parray<TableSpec, 3>* tables;
};

class ToolRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from ToolRandom.rel
  ToolRandomSet(std::shared_ptr<const std::string> data);

  struct TechDiskLevelEntry {
    enum class Mode : uint8_t {
      LEVEL_1 = 0,
      PLAYER_LEVEL_DIVISOR = 1,
      RANDOM_IN_RANGE = 2,
    };
    Mode mode;
    uint8_t player_level_divisor_or_min_level;
    uint8_t max_level;
  } __packed_ws__(TechDiskLevelEntry, 3);

  std::pair<const uint8_t*, size_t> get_common_recovery_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_rare_recovery_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_tech_disk_table(size_t index) const;
  std::pair<const TechDiskLevelEntry*, size_t> get_tech_disk_level_table(size_t index) const;

private:
  const TableSpec* common_recovery_table_spec;
  const TableSpec* rare_recovery_table_spec;
  const TableSpec* tech_disk_table_spec;
  const TableSpec* tech_disk_level_table_spec;
};

class WeaponRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from WeaponRandom*.rel
  WeaponRandomSet(std::shared_ptr<const std::string> data);

  struct RangeTableEntry {
    be_uint32_t min;
    be_uint32_t max;
  } __packed_ws__(RangeTableEntry, 8);

  std::pair<const WeightTableEntry8*, size_t> get_weapon_type_table(size_t index) const;
  const parray<WeightTableEntry32, 6>* get_bonus_type_table(size_t which, size_t index) const;
  const RangeTableEntry* get_bonus_range(size_t which, size_t index) const;
  const parray<WeightTableEntry32, 3>* get_special_mode_table(size_t index) const;
  const RangeTableEntry* get_standard_grind_range(size_t index) const;
  const RangeTableEntry* get_favored_grind_range(size_t index) const;

private:
  struct Offsets {
    be_uint32_t weapon_type_table; // [{c, o -> (table)}](10)
    be_uint32_t bonus_type_table1; // [[{u32 value, u32 weight}](6)](9)
    be_uint32_t bonus_type_table2; // [[{u32 value, u32 weight}](6)](9)
    be_uint32_t bonus_range_table1; // [{u32 min_index, u32 max_index}](9)
    be_uint32_t bonus_range_table2; // [{u32 min_index, u32 max_index}](9)
    be_uint32_t special_mode_table; // [[{u32 value, u32 weight}](3)](8)
    be_uint32_t standard_grind_range_table; // [{u32 min, u32 max}](6)
    be_uint32_t favored_grind_range_table; // [{u32 min, u32 max}](6)
  } __packed_ws__(Offsets, 0x20);

  const Offsets* offsets;
};

class TekkerAdjustmentSet {
public:
  // This class parses and accesses data from JudgeItem.rel
  TekkerAdjustmentSet(std::shared_ptr<const std::string> data);

  const ProbabilityTable<uint8_t, 100>& get_special_upgrade_prob_table(uint8_t section_id, bool favored) const;
  const ProbabilityTable<uint8_t, 100>& get_grind_delta_prob_table(uint8_t section_id, bool favored) const;
  const ProbabilityTable<uint8_t, 100>& get_bonus_delta_prob_table(uint8_t section_id, bool favored) const;
  int8_t get_luck_for_special_upgrade(uint8_t delta_index) const;
  int8_t get_luck_for_grind_delta(uint8_t delta_index) const;
  int8_t get_luck_for_bonus_delta(uint8_t delta_index) const;

private:
  const ProbabilityTable<uint8_t, 100>& get_table(
      std::array<ProbabilityTable<uint8_t, 100>, 10>& tables_default,
      std::array<ProbabilityTable<uint8_t, 100>, 10>& tables_favored,
      uint32_t offset_and_count_offset,
      bool favored,
      uint8_t section_id) const;
  int8_t get_luck(uint32_t start_offset, uint8_t delta_index) const;

  std::shared_ptr<const std::string> data;
  phosg::StringReader r;

  struct DeltaProbabilityEntry {
    uint8_t delta_index;
    uint8_t count_default;
    uint8_t count_favored;
  } __packed_ws__(DeltaProbabilityEntry, 3);
  struct LuckTableEntry {
    uint8_t delta_index;
    int8_t luck;
  } __packed_ws__(LuckTableEntry, 2);

  struct Offsets {
    // Each section ID's favored weapon class has different probabilities than
    // those used for all other weapons. The tables are labeled with (D) for the
    // default values and (F) for the favored-class values.

    // Note that the favored bonuses for Redria are all zero; these values are
    // unused because Redria does not have a favored weapon type. Curiously,
    // Yellowboze also does not have a favored weapon type, but the values for
    // Yellowboze are not all zero.

    // This table specifies how likely a special is to be upgraded or
    // downgraded by one level.
    // In PSO V3, the special upgrade table is:
    //   Viridia    => (D) +1=10%, 0=60%, -1=30%
    //   Viridia    => (F) +1=25%, 0=50%, -1=25%
    //   Greennill  => (D) +1=25%, 0=65%, -1=10%
    //   Greennill  => (F) +1=40%, 0=55%, -1=5%
    //   Skyly      => (D) +1=15%, 0=70%, -1=15%
    //   Skyly      => (F) +1=30%, 0=60%, -1=10%
    //   Bluefull   => (D) +1=10%, 0=60%, -1=30%
    //   Bluefull   => (F) +1=25%, 0=50%, -1=25%
    //   Purplenum  => (D) +1=25%, 0=65%, -1=10%
    //   Purplenum  => (F) +1=40%, 0=55%, -1=5%
    //   Pinkal     => (D) +1=15%, 0=70%, -1=15%
    //   Pinkal     => (F) +1=30%, 0=60%, -1=10%
    //   Redria     => (D) +1=20%, 0=60%, -1=20%
    //   Redria     => (F) +1=0%,  0=0%,  -1=0%
    //   Oran       => (D) +1=15%, 0=70%, -1=15%
    //   Oran       => (F) +1=30%, 0=60%, -1=10%
    //   Yellowboze => (D) +1=25%, 0=65%, -1=10%
    //   Yellowboze => (F) +1=40%, 0=55%, -1=5%
    //   Whitill    => (D) +1=10%, 0=60%, -1=30%
    //   Whitill    => (F) +1=25%, 0=50%, -1=25%
    be_uint32_t special_upgrade_prob_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

    // This table specifies how likely a weapon's grind is to be upgraded or
    // downgraded, and by how much. The final grind value is clamped to the
    // range between 0 and the weapon's maximum grind from ItemPMT, inclusive.
    // In PSO V3, the grind delta table is:
    //   Viridia    => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
    //   Viridia    => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
    //   Greennill  => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
    //   Greennill  => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
    //   Skyly      => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
    //   Skyly      => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
    //   Bluefull   => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
    //   Bluefull   => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
    //   Purplenum  => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
    //   Purplenum  => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
    //   Pinkal     => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
    //   Pinkal     => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
    //   Redria     => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
    //   Redria     => (F) +3=0%,  +2=0%,  +1=0%,  0=0%,  -1=0%,  -2=0%,  -3=0%
    //   Oran       => (D) +3=0%,  +2=7%,  +1=10%, 0=60%, -1=13%, -2=7%,  -3=3%
    //   Oran       => (F) +3=3%,  +2=12%, +1=20%, 0=50%, -1=10%, -2=5%,  -3=0%
    //   Yellowboze => (D) +3=0%,  +2=5%,  +1=10%, 0=70%, -1=10%, -2=5%,  -3=0%
    //   Yellowboze => (F) +3=3%,  +2=7%,  +1=20%, 0=60%, -1=10%, -2=0%,  -3=0%
    //   Whitill    => (D) +3=3%,  +2=7%,  +1=13%, 0=60%, -1=10%, -2=7%,  -3=0%
    //   Whitill    => (F) +3=5%,  +2=13%, +1=25%, 0=50%, -1=7%,  -2=0%,  -3=0%
    be_uint32_t grind_delta_prob_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

    // This table specifies how likely a weapon's bonuses are to be upgraded
    // or downgraded, and by how much. The final bonuses are capped above at
    // 100, but there is no lower limit (so negative results are possible).
    // In PSO V3, the bonus delta table is:
    //   Viridia    => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
    //   Viridia    => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
    //   Greennill  => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
    //   Greennill  => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
    //   Skyly      => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
    //   Skyly      => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
    //   Bluefull   => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
    //   Bluefull   => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
    //   Purplenum  => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
    //   Purplenum  => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
    //   Pinkal     => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
    //   Pinkal     => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
    //   Redria     => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
    //   Redria     => (F) +10=0%,  +5=0%,  0=0%,  -5=0%,  -10=0%
    //   Oran       => (D) +10=10%, +5=25%, 0=50%, -5=10%, -10=5%
    //   Oran       => (F) +10=13%, +5=30%, 0=50%, -5=5%,  -10=2%
    //   Yellowboze => (D) +10=5%,  +5=10%, 0=50%, -5=25%, -10=10%
    //   Yellowboze => (F) +10=8%,  +5=15%, 0=50%, -5=20%, -10=7%
    //   Whitill    => (D) +10=5%,  +5=15%, 0=60%, -5=15%, -10=5%
    //   Whitill    => (F) +10=8%,  +5=20%, 0=60%, -5=10%, -10=2%
    be_uint32_t bonus_delta_prob_table_offset; // [{c, o -> (DeltaProbabilityEntry)[10][c]})

    // There is a secondary computation done during weapon adjustment that
    // appears to determine how "good" the resulting weapon is compared to its
    // original state. If the result of this computation is positive, the game
    // plays a jingle when the tekker result is accepted. These tables describe
    // how much each delta affects this value, which we call luck.

    // In PSO V3, the special upgrade luck table is:
    //   +1 => +20, 0 => 0, -1 => -20
    be_uint32_t special_upgrade_luck_table_offset; // LuckTableEntry[...]; ending with FF FF

    // In PSO V3, the grind delta luck table is:
    //   +3 => +10, +2 => +5, +1 => +3, 0 => 0, -1 => -3, -2 => -5, -3 => -10
    be_uint32_t grind_delta_luck_table_offset; // LuckTableEntry[...]; ending with FF FF

    // In PSO V3, the bonus delta luck table is:
    //   +10 => +15, +5 => +8, 0 => 0, -5 => -8, -10 => -15
    be_uint32_t bonus_delta_luck_offset; // LuckTableEntry[...]; ending with FF FF
  } __packed_ws__(Offsets, 0x18);

  const Offsets* offsets;

  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> special_upgrade_prob_tables_default;
  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> special_upgrade_prob_tables_favored;
  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> grind_delta_prob_tables_default;
  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> grind_delta_prob_tables_favored;
  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> bonus_delta_prob_tables_default;
  mutable std::array<ProbabilityTable<uint8_t, 100>, 10> bonus_delta_prob_tables_favored;
};
