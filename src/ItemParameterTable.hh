#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <phosg/Encoding.hh>
#include <string>
#include <vector>

#include "ItemData.hh"
#include "Text.hh"

class ItemParameterTable {
public:
  template <bool IsBigEndian>
  struct ArrayRef {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    U32T count;
    U32T offset;
  } __attribute__((packed));
  struct ArrayRefLE : ArrayRef<false> {
  } __attribute__((packed));
  struct ArrayRefBE : ArrayRef<true> {
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct ItemBaseV2 {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    // id specifies several things; notably, it doubles as the index of the
    // item's name in the text archive (e.g. TextEnglish) collection 0.
    U32T id = 0xFFFFFFFF;
  } __attribute__((packed));
  template <bool IsBigEndian>
  struct ItemBaseV3 : ItemBaseV2<IsBigEndian> {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    U16T type = 0;
    U16T skin = 0;
  } __attribute__((packed));
  template <bool IsBigEndian>
  struct ItemBaseV4 : ItemBaseV3<IsBigEndian> {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    U32T team_points = 0;
  } __attribute__((packed));

  struct WeaponV2 {
    ItemBaseV2<false> base;
    le_uint16_t class_flags = 0;
    le_uint16_t atp_min = 0;
    le_uint16_t atp_max = 0;
    le_uint16_t atp_required = 0;
    le_uint16_t mst_required = 0;
    le_uint16_t ata_required = 0;
    uint8_t max_grind = 0;
    uint8_t photon = 0;
    uint8_t special = 0;
    uint8_t ata = 0;
    parray<uint8_t, 4> unknown_a9;
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct WeaponV3 {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    ItemBaseV3<IsBigEndian> base;
    U16T class_flags = 0;
    U16T atp_min = 0;
    U16T atp_max = 0;
    U16T atp_required = 0;
    U16T mst_required = 0;
    U16T ata_required = 0;
    U16T mst = 0;
    uint8_t max_grind = 0;
    uint8_t photon = 0;
    uint8_t special = 0;
    uint8_t ata = 0;
    uint8_t stat_boost = 0;
    uint8_t projectile = 0;
    int8_t trail1_x = 0;
    int8_t trail1_y = 0;
    int8_t trail2_x = 0;
    int8_t trail2_y = 0;
    int8_t color = 0;
    uint8_t unknown_a1 = 0;
    uint8_t unknown_a2 = 0;
    uint8_t unknown_a3 = 0;
    uint8_t unknown_a4 = 0;
    uint8_t unknown_a5 = 0;
    uint8_t tech_boost = 0;
    uint8_t combo_type = 0;
  } __attribute__((packed));

  struct WeaponV4 {
    ItemBaseV4<false> base;
    le_uint16_t class_flags = 0x00FF;
    le_uint16_t atp_min = 0;
    le_uint16_t atp_max = 0;
    le_uint16_t atp_required = 0;
    le_uint16_t mst_required = 0;
    le_uint16_t ata_required = 0;
    le_uint16_t mst = 0;
    uint8_t max_grind = 0;
    uint8_t photon = 0;
    uint8_t special = 0;
    uint8_t ata = 0;
    uint8_t stat_boost = 0;
    uint8_t projectile = 0;
    int8_t trail1_x = 0;
    int8_t trail1_y = 0;
    int8_t trail2_x = 0;
    int8_t trail2_y = 0;
    int8_t color = 0;
    uint8_t unknown_a1 = 0;
    uint8_t unknown_a2 = 0;
    uint8_t unknown_a3 = 0;
    uint8_t unknown_a4 = 0;
    uint8_t unknown_a5 = 0;
    uint8_t tech_boost = 0;
    uint8_t combo_type = 0;
  } __attribute__((packed));

  template <typename BaseT, bool IsBigEndian>
  struct ArmorOrShield {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    BaseT base;
    U16T dfp = 0;
    U16T evp = 0;
    uint8_t block_particle = 0;
    uint8_t block_effect = 0;
    U16T class_flags = 0x00FF;
    uint8_t required_level = 0;
    uint8_t efr = 0;
    uint8_t eth = 0;
    uint8_t eic = 0;
    uint8_t edk = 0;
    uint8_t elt = 0;
    uint8_t dfp_range = 0;
    uint8_t evp_range = 0;
    uint8_t stat_boost = 0;
    uint8_t tech_boost = 0;
    U16T unknown_a2 = 0;
  } __attribute__((packed));
  struct ArmorOrShieldV2 : ArmorOrShield<ItemBaseV2<false>, false> {
  } __attribute__((packed));
  struct ArmorOrShieldV3 : ArmorOrShield<ItemBaseV3<true>, true> {
  } __attribute__((packed));
  struct ArmorOrShieldV4 : ArmorOrShield<ItemBaseV4<false>, false> {
  } __attribute__((packed));

  template <typename BaseT, bool IsBigEndian>
  struct Unit {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    using S16T = typename std::conditional<IsBigEndian, be_int16_t, le_int16_t>::type;
    BaseT base;
    U16T stat = 0;
    U16T stat_amount = 0;
    S16T modifier_amount = 0;
    parray<uint8_t, 2> unused;
  } __attribute__((packed));
  struct UnitV2 : Unit<ItemBaseV2<false>, false> {
  } __attribute__((packed));
  struct UnitV3 : Unit<ItemBaseV3<true>, true> {
  } __attribute__((packed));
  struct UnitV4 : Unit<ItemBaseV4<false>, false> {
  } __attribute__((packed));

  template <typename BaseT, bool IsBigEndian>
  struct Mag {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    BaseT base;
    U16T feed_table = 0;
    uint8_t photon_blast = 0;
    uint8_t activation = 0;
    uint8_t on_pb_full = 0;
    uint8_t on_low_hp = 0;
    uint8_t on_death = 0;
    uint8_t on_boss = 0;
    // These flags control how likely each effect is to activate. First, the
    // game computes step_synchro as follows:
    //   if synchro in [0, 30], step_synchro = 0
    //   if synchro in [31, 60], step_synchro = 15
    //   if synchro in [61, 80], step_synchro = 25
    //   if synchro in [81, 100], step_synchro = 30
    //   if synchro in [101, 120], step_synchro = 35
    // Then, the percent chance of the effect occurring upon its trigger (e.g.
    // entering a boss arena) is:
    //   flag == 0 => activation
    //   flag == 1 => activation + step_synchro
    //   flag == 2 => step_synchro
    //   flag == 3 => activation - 10
    //   flag == 4 => step_synchro - 10
    //   anything else => 0 (effect never occurs)
    uint8_t on_pb_full_flag = 0;
    uint8_t on_low_hp_flag = 0;
    uint8_t on_death_flag = 0;
    uint8_t on_boss_flag = 0;
    U16T class_flags = 0x00FF;
    parray<uint8_t, 2> unused;
  } __attribute__((packed));
  struct MagV2 : Mag<ItemBaseV2<false>, false> {
  } __attribute__((packed));
  struct MagV3 : Mag<ItemBaseV3<true>, true> {
  } __attribute__((packed));
  struct MagV4 : Mag<ItemBaseV4<false>, false> {
  } __attribute__((packed));

  template <typename BaseT, bool IsBigEndian>
  struct Tool {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    using S32T = typename std::conditional<IsBigEndian, be_int32_t, le_int32_t>::type;
    BaseT base;
    U16T amount = 0;
    U16T tech = 0;
    S32T cost = 0;
    uint8_t item_flag = 0;
    parray<uint8_t, 3> unused;
  } __attribute__((packed));
  struct ToolV2 : Tool<ItemBaseV2<false>, false> {
  } __attribute__((packed));
  struct ToolV3 : Tool<ItemBaseV3<true>, true> {
  } __attribute__((packed));
  struct ToolV4 : Tool<ItemBaseV4<false>, false> {
  } __attribute__((packed));

  struct MagFeedResult {
    int8_t def = 0;
    int8_t pow = 0;
    int8_t dex = 0;
    int8_t mind = 0;
    int8_t iq = 0;
    int8_t synchro = 0;
    parray<uint8_t, 2> unused;
  } __attribute__((packed));

  using MagFeedResultsList = parray<MagFeedResult, 11>;

  template <bool IsBigEndian>
  struct MagFeedResultsListOffsets {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    parray<U32T, 8> offsets; // Offsets of MagFeedResultsList objects
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct Special {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    U16T type = 0xFFFF;
    U16T amount = 0;
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct StatBoost {
    using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
    uint8_t stat1 = 0;
    uint8_t stat2 = 0;
    U16T amount1 = 0;
    U16T amount2 = 0;
  } __attribute__((packed));

  // Indexed as [tech_num][char_class]
  using MaxTechniqueLevels = parray<parray<uint8_t, 12>, 19>;

  struct ItemCombination {
    parray<uint8_t, 3> used_item;
    parray<uint8_t, 3> equipped_item;
    parray<uint8_t, 3> result_item;
    uint8_t mag_level = 0;
    uint8_t grind = 0;
    uint8_t level = 0;
    uint8_t char_class = 0;
    parray<uint8_t, 3> unused;
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct TechniqueBoost {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    using FloatT = typename std::conditional<IsBigEndian, be_float, le_float>::type;
    U32T tech1 = 0;
    FloatT boost1 = 0.0f;
    U32T tech2 = 0;
    FloatT boost2 = 0.0f;
    U32T tech3 = 0;
    FloatT boost3 = 0.0f;
  } __attribute__((packed));

  struct EventItem {
    parray<uint8_t, 3> item;
    uint8_t probability = 0;
  } __attribute__((packed));

  struct UnsealableItem {
    parray<uint8_t, 3> item;
    uint8_t unused = 0;
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct NonWeaponSaleDivisors {
    using FloatT = typename std::conditional<IsBigEndian, be_float, le_float>::type;
    FloatT armor_divisor = 0.0f;
    FloatT shield_divisor = 0.0f;
    FloatT unit_divisor = 0.0f;
    FloatT mag_divisor = 0.0f;
  } __attribute__((packed));

  enum class Version {
    V2,
    V3,
    V4,
  };

  ItemParameterTable(std::shared_ptr<const std::string> data, Version version);
  ~ItemParameterTable() = default;

  const WeaponV4& get_weapon(uint8_t data1_1, uint8_t data1_2) const;
  const ArmorOrShieldV4& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const;
  const UnitV4& get_unit(uint8_t data1_2) const;
  const MagV4& get_mag(uint8_t data1_1) const;
  const ToolV4& get_tool(uint8_t data1_1, uint8_t data1_2) const;
  std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const;
  float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const;
  const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t which) const;
  uint8_t get_item_stars(uint32_t id) const;
  uint8_t get_special_stars(uint8_t det) const;
  const Special<false>& get_special(uint8_t special) const;
  uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const;
  uint8_t get_weapon_v1_replacement(uint8_t data1_1) const;

  uint32_t get_item_id(const ItemData& item) const;
  uint8_t get_item_base_stars(const ItemData& item) const;
  uint8_t get_item_adjusted_stars(const ItemData& item) const;
  bool is_item_rare(const ItemData& item) const;
  bool is_unsealable_item(const ItemData& param_1) const;
  const ItemCombination& get_item_combination(const ItemData& used_item, const ItemData& equipped_item) const;
  const std::vector<ItemCombination>& get_all_combinations_for_used_item(const ItemData& used_item) const;
  const std::map<uint32_t, std::vector<ItemCombination>>& get_all_item_combinations() const;
  std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const;

  size_t price_for_item(const ItemData& item) const;

  size_t num_weapon_classes;
  size_t num_tool_classes;
  size_t item_stars_first_id;
  size_t item_stars_last_id;
  size_t special_stars_begin_index;
  size_t num_specials;
  size_t first_rare_mag_index;
  size_t star_value_table_size;

private:
  struct TableOffsetsV2 {
    // TODO: Is weapon count 0x89 or 0x8A? It could be that the last entry in
    // weapon_table is used for ???? items.
    /* 00 / 0013 */ le_uint32_t unknown_a0;
    /* 04 / 5AFC */ le_uint32_t weapon_table; // -> [{count, offset -> [WeaponV2]}](0x89)
    /* 08 / 5A5C */ le_uint32_t armor_table; // -> [{count, offset -> [ArmorOrShieldV2]}](2; armors and shields)
    /* 0C / 5A6C */ le_uint32_t unit_table; // -> {count, offset -> [UnitV2]} (last if out of range)
    /* 10 / 5A7C */ le_uint32_t tool_table; // -> [{count, offset -> [ToolV2]}](0x10) (last if out of range)
    /* 14 / 5A74 */ le_uint32_t mag_table; // -> {count, offset -> [MagV2]}
    /* 18 / 3DF8 */ le_uint32_t v1_replacement_table; // -> [uint8_t](0x89)
    /* 1C / 2E4C */ le_uint32_t photon_color_table; // -> [0x24-byte structs](0x20)
    /* 20 / 32CC */ le_uint32_t weapon_range_table; // -> ???
    /* 24 / 3E84 */ le_uint32_t weapon_sale_divisor_table; // -> [float](0x89)
    /* 28 / 40A8 */ le_uint32_t sale_divisor_table; // -> NonWeaponSaleDivisors
    /* 2C / 5F4C */ le_uint32_t mag_feed_table; // -> MagFeedResultsTable
    /* 30 / 4378 */ le_uint32_t star_value_table; // -> [uint8_t](0x1C7)
    /* 34 / 4540 */ le_uint32_t special_data_table; // -> [Special](0x29)
    /* 38 / 45E4 */ le_uint32_t weapon_effect_table; // -> [16-byte structs]
    /* 3C / 58DC */ le_uint32_t stat_boost_table; // -> [StatBoost]
    /* 40 / 5704 */ le_uint32_t shield_effect_table; // -> [8-byte structs]
  } __attribute__((packed));

  template <bool IsBigEndian>
  struct TableOffsetsV3V4 {
    using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
    /* ## / GC   / BB */
    /* 00 / F078 / 14884 */ U32T weapon_table; // -> [{count, offset -> [WeaponV3/WeaponV4]}](0xED)
    /* 04 / EF90 / 1478C */ U32T armor_table; // -> [{count, offset -> [ArmorOrShieldV3/ArmorOrShieldV4]}](2; armors and shields)
    /* 08 / EFA0 / 1479C */ U32T unit_table; // -> {count, offset -> [UnitV3/UnitV4]} (last if out of range)
    /* 0C / EFB0 / 147AC */ U32T tool_table; // -> [{count, offset -> [ToolV3/ToolV4]}](0x1A) (last if out of range)
    /* 10 / EFA8 / 147A4 */ U32T mag_table; // -> {count, offset -> [MagV3/MagV4]}
    /* 14 / B88C / 0F4B8 */ U32T v1_replacement_table; // -> [uint8_t](0xED)
    /* 18 / A7FC / 0DE7C */ U32T photon_color_table; // -> [0x24-byte structs](0x20)
    /* 1C / AACC / 0E194 */ U32T weapon_range_table; // -> ???
    /* 20 / B938 / 0F5A8 */ U32T weapon_sale_divisor_table; // -> [float](0xED)
    /* 24 / BBCC / 0F83C */ U32T sale_divisor_table; // -> NonWeaponSaleDivisors
    /* 28 / F608 / 1502C */ U32T mag_feed_table; // -> MagFeedResultsTable
    /* 2C / BE9C / 0FB0C */ U32T star_value_table; // -> [uint8_t](0x330) (indexed by .id from weapon, armor, etc.)
    /* 30 / C100 / 0FE3C */ U32T special_data_table; // -> [Special]
    /* 34 / C1A4 / 0FEE0 */ U32T weapon_effect_table; // -> [16-byte structs]
    /* 38 / DE50 / 1275C */ U32T stat_boost_table; // -> [StatBoost]
    /* 3C / D6E4 / 11C80 */ U32T shield_effect_table; // -> [8-byte structs]
    /* 40 / DF88 / 12894 */ U32T max_tech_level_table; // -> MaxTechniqueLevels
    /* 44 / F5D0 / 14FF4 */ U32T combination_table; // -> {count, offset -> [ItemCombination]}
    /* 48 / DE48 / 12754 */ U32T unknown_a1;
    /* 4C / EB8C / 14278 */ U32T tech_boost_table; // -> [TechniqueBoost] (always 0x2C of them? from counts struct?)
    /* 50 / F5F0 / 15014 */ U32T unwrap_table; // -> {count, offset -> [{count, offset -> [EventItem]}]}
    /* 54 / F5F8 / 1501C */ U32T unsealable_table; // -> {count, offset -> [UnsealableItem]}
    /* 58 / F600 / 15024 */ U32T ranged_special_table; // -> {count, offset -> [4-byte structs]}
  } __attribute__((packed));

  std::shared_ptr<const std::string> data;
  StringReader r;
  const TableOffsetsV2* offsets_v2;
  const TableOffsetsV3V4<true>* offsets_v3;
  const TableOffsetsV3V4<false>* offsets_v4;

  // These are unused if offsets_v4 is not null (in that case, we just return
  // references pointing inside the data string)
  mutable std::unordered_map<uint16_t, WeaponV4> parsed_weapons;
  mutable std::vector<ArmorOrShieldV4> parsed_armors;
  mutable std::vector<ArmorOrShieldV4> parsed_shields;
  mutable std::vector<UnitV4> parsed_units;
  mutable std::vector<MagV4> parsed_mags;
  mutable std::unordered_map<uint16_t, ToolV4> parsed_tools;
  mutable std::vector<Special<false>> parsed_specials;

  // Key is used_item. We can't index on (used_item, equipped_item) because
  // equipped_item may contain wildcards, and the matching order matters.
  mutable std::map<uint32_t, std::vector<ItemCombination>> item_combination_index;

  template <typename ToolT, bool IsBigEndian>
  std::pair<uint8_t, uint8_t> find_tool_by_id_t(uint32_t tool_table_offset, uint32_t id) const;
  template <bool IsBigEndian>
  float get_sale_divisor_t(uint32_t weapon_table_offset, uint32_t non_weapon_table_offset, uint8_t data1_0, uint8_t data1_1) const;
  template <bool IsBigEndian>
  std::pair<const ItemParameterTable::EventItem*, size_t> get_event_items_t(uint32_t base_offset, uint8_t event_number) const;
};

class MagEvolutionTable {
public:
  struct TableOffsets {
    /* 00 / 0400 */ le_uint32_t unknown_a1; // -> [offset -> (0xC-byte struct)[0x53], offset -> (same as first offset)]
    /* 04 / 0408 */ le_uint32_t unknown_a2; // -> (2-byte struct, or single word)[0x53]
    /* 08 / 04AE */ le_uint32_t unknown_a3; // -> (0xA8 bytes; possibly (8-byte struct)[0x15])
    /* 0C / 0556 */ le_uint32_t unknown_a4; // -> (uint8_t)[0x53]
    /* 10 / 05AC */ le_uint32_t unknown_a5; // -> (float)[0x48]
    /* 14 / 06CC */ le_uint32_t evolution_number; // -> (uint8_t)[0x53]
  } __attribute__((packed));

  struct EvolutionNumberTable {
    parray<uint8_t, 0x53> values;
  } __attribute__((packed));

  MagEvolutionTable(std::shared_ptr<const std::string> data);
  ~MagEvolutionTable() = default;

  uint8_t get_evolution_number(uint8_t data1_1) const;

private:
  std::shared_ptr<const std::string> data;
  StringReader r;
  const TableOffsets* offsets;
};
