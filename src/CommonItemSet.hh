#pragma once

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>

#include "EnemyType.hh"
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
    Table(std::shared_ptr<const Table> prev_table, const phosg::JSON& json, Episode episode);
    Table(const phosg::StringReader& r, bool big_endian, bool is_v3, Episode episode);

    bool operator==(const Table& other) const = default;
    bool operator!=(const Table& other) const = default;

    template <typename IntT>
    struct Range {
      IntT min = 0;
      IntT max = 0;

      bool operator==(const Range& other) const = default;
      bool operator!=(const Range& other) const = default;

      inline bool empty() const {
        return ((this->min | this->max) == 0);
      }
    } __attribute__((packed));

    Episode episode;
    parray<uint8_t, 0x0C> base_weapon_type_prob_table;
    parray<int8_t, 0x0C> subtype_base_table;
    parray<uint8_t, 0x0C> subtype_area_length_table;
    parray<parray<uint8_t, 4>, 9> grind_prob_table;
    parray<uint8_t, 0x05> armor_shield_type_index_prob_table;
    parray<uint8_t, 0x05> armor_slot_count_prob_table;
    // Note: PSO originally uses arrays indexed by rt_index here, but we index enemies by the EnemyType enum instead
    std::unordered_map<EnemyType, Range<uint16_t>> enemy_type_meseta_ranges;
    std::unordered_map<EnemyType, uint8_t> enemy_type_drop_probs;
    std::unordered_map<EnemyType, uint8_t> enemy_type_item_classes;
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

    phosg::JSON json(std::shared_ptr<const Table> prev_table) const;
    void print(FILE* stream) const;
    void print_diff(FILE* stream, const Table& other) const;

  private:
    template <bool BE>
    void parse_itempt_t(const phosg::StringReader& r, bool is_v3);

    template <bool BE>
    struct RootT {
      // This data structure uses index probability tables in multiple places. An index probability table is a table
      // where each entry holds the probability that that entry's index is used. For example, if the armor slot count
      // probability table contains [77, 17, 5, 1, 0], this means there is a 77% chance of no slots, 17% chance of 1
      // slot, 5% chance of 2 slots, 1% chance of 3 slots, and no chance of 4 slots. The values in index probability
      // tables do not have to add up to 100; the game sums all of them and chooses a random number less than that
      // maximum.

      // The area (floor) number is used in many places as well. Unlike the normal area numbers, which start with
      // Pioneer 2, the area numbers in this structure start with Forest 1, and boss areas are treated as the first
      // area of the next section (so De Rol Le has Mines 1 drops, for example). Final boss areas are treated as the
      // last non-boss area (so Dark Falz boxes are like Ruins 3 boxes). We refer to these adjusted area numbers as
      // (area - 1), or area_norm.

      // This index probability table determines the types of non-rare weapons. The indexes in this table correspond to
      // the non-rare weapon types 01 through 0C (Saber through Wand).
      // V2/V3: -> parray<uint8_t, 0x0C>
      /* 00 */ U32T<BE> base_weapon_type_prob_table_offset;

      // This table specifies the base subtype for each weapon type. Negative values here mean that the weapon cannot
      // be found in the first N areas (so -2, for example, means that the weapon never appears in Forest 1 or 2 at
      // all). Nonnegative values here mean the subtype can be found in all areas, and specify the base subtype
      // (usually in the range [0, 4]). The subtype of weapon that actually appears depends on this value and a value
      // from the following table.
      // V2/V3: -> parray<int8_t, 0x0C>
      /* 04 */ U32T<BE> subtype_base_table_offset;

      // This table specifies how many areas each weapon subtype appears in. For example, if Sword (subtype 02, which
      // is index 1 in this table and the table above) has a subtype base of -2 and a subtype area length of 4, then
      // Sword items can be found when area - 1 is 2, 3, 4, or 5 (Cave 1 through Mine 1), and Gigush (the next sword
      // subtype) can be found in Mine 1 through Ruins 3.
      // V2/V3: -> parray<uint8_t, 0x0C>
      /* 08 */ U32T<BE> subtype_area_length_table_offset;

      // This index probability table specifies how likely each possible grind value is. The table is indexed as
      // [grind][subtype_area_index], where the subtype area index is how many areas the player is beyond the first
      // area in which the subtype can first be found (clamped to [0, 3]). To continue the example above, in Cave 3,
      // subtype_area_index would be 2, since Swords can first be found two areas earlier in Cave 1.
      // For example, this table could look like this:
      //   [64 1E 19 14] // Chance of getting a grind +0
      //   [00 1E 17 0F] // Chance of getting a grind +1
      //   [00 14 14 0E] // Chance of getting a grind +2
      //    ...
      //    C1 C2 C3 M1  // (Episode 1 area values from the example for reference)
      // V2/V3: -> parray<parray<uint8_t, 4>, 9>
      /* 0C */ U32T<BE> grind_prob_table_offset;

      // This index probability table specifies how likely each type of armor or shield is. The general formula is:
      //   data1[2] = max((area_norm + (result from this table) + armor_or_shield_type_bias - 3), 0)
      // In this way, (armor_or_shield_type_bias + area_norm - 3) can be thought of as the "base" value for each area,
      // and this table specifies how likely the armor/shield is to be "upgraded" from that value.
      // V2/V3: -> parray<uint8_t, 0x05>
      /* 10 */ U32T<BE> armor_shield_type_index_prob_table_offset;

      // This index probability table specifies how common each possible slot count is for armor drops.
      // V2/V3: -> parray<uint8_t, 0x05>
      /* 14 */ U32T<BE> armor_slot_count_prob_table_offset;

      // This array (indexed by rt_index) specifies the range of meseta values that each enemy can drop.
      // V2/V3: -> parray<Range<U16T>, NUM_RT_INDEXES_V3>
      /* 18 */ U32T<BE> enemy_rt_index_meseta_ranges_offset;

      // Each byte in this table (indexed by rt_index) represents the percent chance that the enemy drops anything at
      // all. (This check is done before the rare drop check, so the chance of getting a rare item from an enemy is
      // essentially this probability multiplied by the rare drop rate.)
      // V2/V3: -> parray<uint8_t, NUM_RT_INDEXES_V3>
      /* 1C */ U32T<BE> enemy_rt_index_drop_probs_offset;

      // Each byte in this table (indexed by rt_index) represents the class of item that can drop. The values are:
      //   00 = weapon
      //   01 = armor
      //   02 = shield
      //   03 = unit
      //   04 = tool
      //   05 = meseta
      //   Anything else = no item
      // V2/V3: -> parray<uint8_t, NUM_RT_INDEXES_V3>
      /* 20 */ U32T<BE> enemy_rt_index_item_classes_offset;

      // This table (indexed by area - 1) specifies the ranges of meseta values that can drop from boxes.
      // V2/V3: -> parray<Range<U16T>, 0x0A>
      /* 24 */ U32T<BE> box_meseta_ranges_offset;

      // This array specifies the chance that a rare weapon will have each possible bonus value. This is indexed as
      // [(bonus_value - 10 / 5)][spec], so the first row refers the probability of getting a -10% bonus, the next row
      // is the chance of getting -5%, etc., all the way up to +100%. For non-rare items (or all items on v1/v2), spec
      // is determined randomly based on the following field; for rare items on v3+, spec is always 5.
      // V2: -> parray<parray<uint8_t, 5>, 0x17>
      // V3: -> parray<parray<U16T, 6>, 0x17>
      /* 28 */ U32T<BE> bonus_value_prob_table_offset;

      // This array specifies the value of spec to be used in the above lookup for non-rare items. This is NOT an index
      // probability table; this is a direct lookup with indexes [bonus_index][area - 1]. A value of 0xFF in any byte
      // of this array prevents any weapon from having a bonus in that slot. An example table might look like this:
      //   [00 00 00 01 01 01 01 02 02 02]
      //   [FF FF FF 00 00 00 01 01 01 01]
      //   [FF FF FF FF FF FF FF FF FF 00]
      //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
      // In this example, spec is 0, 1, or 2 in all cases where a weapon can have a bonus. In Forest 1 and 2 and Cave
      // 1, weapons may have at most one bonus; in all other areas except Ruins 3, they can have at most two bonuses,
      // and in Ruins 3, they can have up to three bonuses.
      // V2/V3: // -> parray<parray<uint8_t, 10>, 3>
      /* 2C */ U32T<BE> nonrare_bonus_prob_spec_offset;

      // This array specifies the chance that a weapon will have each bonus type. The table is indexed as
      // [bonus_type][area - 1] for non-rare items; for rare items, a random value in the range [0, 9] is used instead
      // of (area - 1).
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

      // This array (indexed by area - 1) specifies a parameter used in weapon special generation. If the sampled value
      // from this table is 0, no special is generated. Otherwise, a random floating-point value W in the range [0,
      // special_mult] is generated and truncated to an integer. If this value is greater than 3, no special is
      // generated; otherwise, a random special worth (W + 1) stars is chosen. It seems Sega only intended special_mult
      // to be in the range [0, 4], but values greater than 4 will work, and will simply increase the probability of
      // getting no special.
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 34 */ U32T<BE> special_mult_offset;

      // This array (indexed by area - 1) specifies the probability that a non-rare weapon will have a special ability.
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 38 */ U32T<BE> special_percent_offset;

      // This index probability table is indexed by [tool_class][area - 1]. The tool class refers to an entry in
      // ItemPMT, which links it to the actual item code.
      // V2/V3: -> parray<parray<U16T, 0x0A>, 0x1C>
      /* 3C */ U32T<BE> tool_class_prob_table_offset;

      // This index probability table determines how likely each technique is to appear. The table is indexed as
      // [technique_num][area - 1].
      // V2/V3: -> parray<parray<uint8_t, 0x0A>, 0x13>
      /* 40 */ U32T<BE> technique_index_prob_table_offset;

      // This table specifies the ranges for technique disk levels. The table is indexed as [technique_num][area - 1].
      // If either min or max in the range is 0xFF, or if max < min, technique disks are not dropped for that technique
      // and area pair.
      // V2/V3: -> parray<parray<Range<uint8_t>, 0x0A>, 0x13>
      /* 44 */ U32T<BE> technique_level_ranges_offset;

      // See comments on armor_shield_type_index_prob_table_offset for how this is used.
      /* 48 */ uint8_t armor_or_shield_type_bias;
      /* 49 */ parray<uint8_t, 3> unused1;

      // These values specify the maximum number of stars any generated unit can have in each area. The values here are
      // not inclusive; that is, a value of 7 means that only units with 1-6 stars can drop in that area. The game
      // uniformly chooses a random number of stars in the acceptable range, then uniformly chooses a random unit with
      // that many stars.
      // V2/V3: -> parray<uint8_t, 0x0A>
      /* 4C */ U32T<BE> unit_max_stars_offset;

      // This index probability table determines which type of items drop from boxes. The table is indexed as
      // [item_class][area - 1], with item_class as the result value (that is, in the example below, the game looks at
      // a single column and sums the values going down, then the chosen item class is one of the row indexes based on
      // the weight values in the column.) The resulting value has the same meaning as in enemy_rt_index_item_classes.
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
    } __packed_ws_be__(RootT, 0x54);
    using Root = RootT<false>;
    using RootBE = RootT<true>;
  };

  bool operator==(const CommonItemSet& other) const = default;
  bool operator!=(const CommonItemSet& other) const = default;

  std::shared_ptr<const Table> get_table(
      Episode episode, GameMode mode, Difficulty difficulty, uint8_t section_id) const;
  std::shared_ptr<const Table> get_prev_table(
      Episode episode, GameMode mode, Difficulty difficulty, uint8_t section_id) const;

  phosg::JSON json() const;
  void print(FILE* stream) const;
  void print_diff(FILE* stream, const CommonItemSet& other) const;

protected:
  CommonItemSet() = default;

  static uint16_t key_for_table(Episode episode, GameMode mode, Difficulty difficulty, uint8_t section_id);
  static std::string json_key_for_table(Episode episode, GameMode mode, Difficulty difficulty, uint8_t section_id);

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
