#pragma once

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <phosg/Encoding.hh>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "ItemData.hh"
#include "Text.hh"
#include "Types.hh"
#include "Version.hh"

class ItemParameterTable {
public:
  // TODO: This implementation is ugly. We should use real classes and virtual
  // functions instead of manually branching on various offset table pointers
  // being null or not in each public function. Rewrite this and make it better.

  template <bool BE>
  struct ItemBaseV2T {
    // id specifies several things; notably, it doubles as the index of the
    // item's name in the text archive (e.g. TextEnglish) collection 0.
    /* 00 */ U32T<BE> id = 0xFFFFFFFF;
    /* 04 */
  } __packed__;
  template <bool BE>
  struct ItemBaseV3T : ItemBaseV2T<BE> {
    /* 04 */ U16T<BE> type = 0; // "Model" in Soly's ItemPMT editor
    /* 06 */ U16T<BE> skin = 0; // "Texture" in Soly's ItemPMT editor
    /* 08 */
  } __packed__;
  template <bool BE>
  struct ItemBaseV4T : ItemBaseV3T<BE> {
    /* 08 */ U32T<BE> team_points = 0;
    /* 0C */
  } __packed__;

  struct WeaponV4;
  struct WeaponDCProtos {
    /* 00 */ ItemBaseV2T<false> base;
    /* 04 */ le_uint16_t class_flags = 0;
    /* 06 */ le_uint16_t atp_min = 0;
    /* 08 */ le_uint16_t atp_max = 0;
    /* 0A */ le_uint16_t atp_required = 0;
    /* 0C */ le_uint16_t mst_required = 0;
    /* 0E */ le_uint16_t ata_required = 0;
    /* 10 */ uint8_t max_grind = 0;
    /* 11 */ uint8_t photon = 0;
    /* 12 */ uint8_t special = 0;
    /* 13 */ uint8_t ata = 0;
    /* 14 */

    WeaponV4 to_v4() const;
  } __packed_ws__(WeaponDCProtos, 0x14);

  struct WeaponV1V2 {
    /* 00 */ ItemBaseV2T<false> base;
    /* 04 */ le_uint16_t class_flags = 0;
    /* 06 */ le_uint16_t atp_min = 0;
    /* 08 */ le_uint16_t atp_max = 0;
    /* 0A */ le_uint16_t atp_required = 0;
    /* 0C */ le_uint16_t mst_required = 0;
    /* 0E */ le_uint16_t ata_required = 0;
    /* 10 */ uint8_t max_grind = 0;
    /* 11 */ uint8_t photon = 0;
    /* 12 */ uint8_t special = 0;
    /* 13 */ uint8_t ata = 0;
    /* 14 */ uint8_t stat_boost = 0; // TODO: This could be larger (16 or 32 bits)
    /* 15 */ parray<uint8_t, 3> unknown_a9;
    /* 18 */

    WeaponV4 to_v4() const;
  } __packed_ws__(WeaponV1V2, 0x18);

  struct WeaponGCNTE {
    /* 00 */ ItemBaseV3T<true> base;
    /* 08 */ be_uint16_t class_flags = 0;
    /* 0A */ be_uint16_t atp_min = 0;
    /* 0C */ be_uint16_t atp_max = 0;
    /* 0E */ be_uint16_t atp_required = 0;
    /* 10 */ be_uint16_t mst_required = 0;
    /* 12 */ be_uint16_t ata_required = 0;
    /* 14 */ be_uint16_t mst = 0;
    /* 16 */ uint8_t max_grind = 0;
    /* 17 */ uint8_t photon = 0;
    /* 18 */ uint8_t special = 0;
    /* 19 */ uint8_t ata = 0;
    /* 1A */ uint8_t stat_boost = 0;
    /* 1B */ uint8_t projectile = 0;
    /* 1C */ int8_t trail1_x = 0;
    /* 1D */ int8_t trail1_y = 0;
    /* 1E */ int8_t trail2_x = 0;
    /* 1F */ int8_t trail2_y = 0;
    /* 20 */ int8_t color = 0;
    /* 21 */ uint8_t unknown_a1 = 0;
    /* 22 */ uint8_t unknown_a2 = 0;
    /* 23 */ uint8_t unknown_a3 = 0;
    /* 24 */

    WeaponV4 to_v4() const;
  } __packed_ws__(WeaponGCNTE, 0x24);

  template <bool BE>
  struct WeaponV3T {
    /* 00 */ ItemBaseV3T<BE> base;
    /* 08 */ U16T<BE> class_flags = 0;
    /* 0A */ U16T<BE> atp_min = 0;
    /* 0C */ U16T<BE> atp_max = 0;
    /* 0E */ U16T<BE> atp_required = 0;
    /* 10 */ U16T<BE> mst_required = 0;
    /* 12 */ U16T<BE> ata_required = 0;
    /* 14 */ U16T<BE> mst = 0;
    /* 16 */ uint8_t max_grind = 0;
    /* 17 */ uint8_t photon = 0;
    /* 18 */ uint8_t special = 0;
    /* 19 */ uint8_t ata = 0;
    /* 1A */ uint8_t stat_boost = 0;
    /* 1B */ uint8_t projectile = 0;
    /* 1C */ int8_t trail1_x = 0;
    /* 1D */ int8_t trail1_y = 0;
    /* 1E */ int8_t trail2_x = 0;
    /* 1F */ int8_t trail2_y = 0;
    /* 20 */ int8_t color = 0;
    /* 21 */ uint8_t unknown_a1 = 0;
    /* 22 */ uint8_t unknown_a2 = 0;
    /* 23 */ uint8_t unknown_a3 = 0;
    /* 24 */ uint8_t unknown_a4 = 0;
    /* 25 */ uint8_t unknown_a5 = 0;
    /* 26 */ uint8_t tech_boost = 0;
    /* 27 */ uint8_t combo_type = 0;
    /* 28 */

    WeaponV4 to_v4() const;
  } __packed__;

  using WeaponV3 = WeaponV3T<false>;
  using WeaponV3BE = WeaponV3T<true>;
  check_struct_size(WeaponV3, 0x28);
  check_struct_size(WeaponV3BE, 0x28);

  struct WeaponV4 {
    /* 00 */ ItemBaseV4T<false> base;
    /* 0C */ le_uint16_t class_flags = 0x00FF;
    /* 0E */ le_uint16_t atp_min = 0;
    /* 10 */ le_uint16_t atp_max = 0;
    /* 12 */ le_uint16_t atp_required = 0;
    /* 14 */ le_uint16_t mst_required = 0;
    /* 16 */ le_uint16_t ata_required = 0;
    /* 18 */ le_uint16_t mst = 0;
    /* 1A */ uint8_t max_grind = 0;
    /* 1B */ uint8_t photon = 0;
    /* 1C */ uint8_t special = 0;
    /* 1D */ uint8_t ata = 0;
    /* 1E */ uint8_t stat_boost = 0;
    /* 1F */ uint8_t projectile = 0;
    /* 20 */ int8_t trail1_x = 0;
    /* 21 */ int8_t trail1_y = 0;
    /* 22 */ int8_t trail2_x = 0;
    /* 23 */ int8_t trail2_y = 0;
    /* 24 */ int8_t color = 0;
    /* 25 */ uint8_t unknown_a1 = 0;
    /* 26 */ uint8_t unknown_a2 = 0;
    /* 27 */ uint8_t unknown_a3 = 0;
    /* 28 */ uint8_t unknown_a4 = 0;
    /* 29 */ uint8_t unknown_a5 = 0;
    /* 2A */ uint8_t tech_boost = 0;
    /* 2B */ uint8_t combo_type = 0;
    /* 2C */
  } __packed_ws__(WeaponV4, 0x2C);

  template <typename BaseT, bool BE>
  struct ArmorOrShieldT {
    /* V1/V2 offsets */
    /* 00 */ BaseT base;
    /* 04 */ U16T<BE> dfp = 0;
    /* 06 */ U16T<BE> evp = 0;
    /* 08 */ uint8_t block_particle = 0;
    /* 09 */ uint8_t block_effect = 0;
    /* 0A */ U16T<BE> class_flags = 0x00FF;
    /* 0C */ uint8_t required_level = 0;
    /* 0D */ uint8_t efr = 0;
    /* 0E */ uint8_t eth = 0;
    /* 0F */ uint8_t eic = 0;
    /* 10 */ uint8_t edk = 0;
    /* 11 */ uint8_t elt = 0;
    /* 12 */ uint8_t dfp_range = 0;
    /* 13 */ uint8_t evp_range = 0;
    /* 14 */
  } __packed__;

  template <typename BaseT, bool BE>
  struct ArmorOrShieldFinalT : ArmorOrShieldT<BaseT, BE> {
    /* 14 */ uint8_t stat_boost = 0;
    /* 15 */ uint8_t tech_boost = 0;
    /* 16 */ U16T<BE> unknown_a2 = 0;
    /* 18 */
  } __packed__;
  using ArmorOrShieldV4 = ArmorOrShieldFinalT<ItemBaseV4T<false>, false>;
  check_struct_size(ArmorOrShieldV4, 0x20);
  struct ArmorOrShieldDCProtos : ArmorOrShieldT<ItemBaseV2T<false>, false> {
    ArmorOrShieldV4 to_v4() const;
  } __packed_ws__(ArmorOrShieldDCProtos, 0x14);

  struct ArmorOrShieldV1V2 : ArmorOrShieldFinalT<ItemBaseV2T<false>, false> {
    ArmorOrShieldV4 to_v4() const;
  } __packed_ws__(ArmorOrShieldV1V2, 0x18);
  template <bool BE>
  struct ArmorOrShieldV3T : ArmorOrShieldFinalT<ItemBaseV3T<BE>, BE> {
    ArmorOrShieldV4 to_v4() const;
  } __packed__;
  using ArmorOrShieldV3 = ArmorOrShieldV3T<false>;
  using ArmorOrShieldV3BE = ArmorOrShieldV3T<true>;
  check_struct_size(ArmorOrShieldV3, 0x1C);
  check_struct_size(ArmorOrShieldV3BE, 0x1C);

  template <typename BaseT, bool BE>
  struct UnitT {
    /* V1/V2 offsets */
    /* 00 */ BaseT base;
    /* 04 */ U16T<BE> stat = 0;
    /* 06 */ U16T<BE> stat_amount = 0;
    /* 08 */
  } __packed__;

  template <typename BaseT, bool BE>
  struct UnitFinalT : UnitT<BaseT, BE> {
    /* 08 */ S16T<BE> modifier_amount = 0;
    /* 0A */ parray<uint8_t, 2> unused;
    /* 0C */
  } __packed__;
  using UnitV4 = UnitFinalT<ItemBaseV4T<false>, false>;
  check_struct_size(UnitV4, 0x14);
  struct UnitDCProtos : UnitT<ItemBaseV2T<false>, false> {
    UnitV4 to_v4() const;
  } __packed_ws__(UnitDCProtos, 0x08);
  struct UnitV1V2 : UnitFinalT<ItemBaseV2T<false>, false> {
    UnitV4 to_v4() const;
  } __packed_ws__(UnitV1V2, 0x0C);
  template <bool BE>
  struct UnitV3T : UnitFinalT<ItemBaseV3T<BE>, BE> {
    UnitV4 to_v4() const;
  } __packed__;
  using UnitV3 = UnitV3T<false>;
  using UnitV3BE = UnitV3T<true>;
  check_struct_size(UnitV3, 0x10);
  check_struct_size(UnitV3BE, 0x10);

  template <typename BaseT, bool BE>
  struct MagT {
    /* V1/V2 offsets */
    /* 00 */ BaseT base;
    /* 04 */ U16T<BE> feed_table = 0;
    /* 06 */ uint8_t photon_blast = 0;
    /* 07 */ uint8_t activation = 0;
    /* 08 */ uint8_t on_pb_full = 0;
    /* 09 */ uint8_t on_low_hp = 0;
    /* 0A */ uint8_t on_death = 0;
    /* 0B */ uint8_t on_boss = 0;
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
    /* 0C */ uint8_t on_pb_full_flag = 0;
    /* 0D */ uint8_t on_low_hp_flag = 0;
    /* 0E */ uint8_t on_death_flag = 0;
    /* 0F */ uint8_t on_boss_flag = 0;
    /* 10 */
  } __packed__;

  struct MagV4 : MagT<ItemBaseV4T<false>, false> {
    le_uint16_t class_flags = 0x00FF;
    parray<uint8_t, 2> unused;
  } __packed_ws__(MagV4, 0x1C);
  struct MagV1 : MagT<ItemBaseV2T<false>, false> {
    MagV4 to_v4() const;
  } __packed_ws__(MagV1, 0x10);
  struct MagV2 : MagT<ItemBaseV2T<false>, false> {
    /* 10 */ le_uint16_t class_flags = 0x00FF;
    /* 12 */ parray<uint8_t, 2> unused;
    /* 14 */

    MagV4 to_v4() const;
  } __packed_ws__(MagV2, 0x14);
  template <bool BE>
  struct MagV3T : MagT<ItemBaseV3T<BE>, BE> {
    /* 10 */ U16T<BE> class_flags = 0x00FF;
    /* 12 */ parray<uint8_t, 2> unused;
    /* 14 */

    MagV4 to_v4() const;
  } __packed__;
  using MagV3 = MagV3T<false>;
  using MagV3BE = MagV3T<true>;
  check_struct_size(MagV3, 0x18);
  check_struct_size(MagV3BE, 0x18);

  template <typename BaseT, bool BE>
  struct ToolT {
    /* V1/V2 offsets */
    /* 00 */ BaseT base;
    /* 04 */ U16T<BE> amount = 0;
    /* 06 */ U16T<BE> tech = 0;
    /* 08 */ S32T<BE> cost = 0;
    // Bits in item_flags:
    //   00000001 - ever usable by player ("Use" appears in inventory menu)
    //   00000002 - unknown
    //   00000004 - unknown
    //   00000008 - usable by android characters
    //   00000010 - usable in Pioneer 2 / Lab
    //   00000020 - usable in boss arenas
    //   00000040 - usable in Challenge mode
    //   00000080 - is rare (renders as red box; V3+)
    /* 0C */ U32T<BE> item_flags = 0;
    /* 10 */
  } __packed__;

  using ToolV4 = ToolT<ItemBaseV4T<false>, false>;
  check_struct_size(ToolV4, 0x18);
  struct ToolV1V2 : ToolT<ItemBaseV2T<false>, false> {
    ToolV4 to_v4() const;
  } __packed_ws__(ToolV1V2, 0x10);
  template <bool BE>
  struct ToolV3T : ToolT<ItemBaseV3T<BE>, BE> {
    ToolV4 to_v4() const;
  } __packed__;
  using ToolV3 = ToolV3T<false>;
  using ToolV3BE = ToolV3T<true>;
  check_struct_size(ToolV3, 0x14);
  check_struct_size(ToolV3BE, 0x14);

  struct MagFeedResult {
    int8_t def = 0;
    int8_t pow = 0;
    int8_t dex = 0;
    int8_t mind = 0;
    int8_t iq = 0;
    int8_t synchro = 0;
    parray<uint8_t, 2> unused;
  } __packed_ws__(MagFeedResult, 8);

  using MagFeedResultsList = parray<MagFeedResult, 11>;

  template <bool BE>
  struct MagFeedResultsListOffsetsT {
    parray<U32T<BE>, 8> offsets; // Offsets of MagFeedResultsList objects
  } __packed__;
  using MagFeedResultsListOffsets = MagFeedResultsListOffsetsT<false>;
  using MagFeedResultsListOffsetsBE = MagFeedResultsListOffsetsT<true>;
  check_struct_size(MagFeedResultsListOffsets, 0x20);
  check_struct_size(MagFeedResultsListOffsetsBE, 0x20);

  template <bool BE>
  struct SpecialT {
    U16T<BE> type = 0xFFFF;
    U16T<BE> amount = 0;
  } __packed__;
  using Special = SpecialT<false>;
  using SpecialBE = SpecialT<true>;
  check_struct_size(Special, 4);
  check_struct_size(SpecialBE, 4);

  template <bool BE>
  struct StatBoostT {
    uint8_t stat1 = 0;
    uint8_t stat2 = 0;
    U16T<BE> amount1 = 0;
    U16T<BE> amount2 = 0;
  } __packed__;
  using StatBoost = StatBoostT<false>;
  using StatBoostBE = StatBoostT<true>;
  check_struct_size(StatBoost, 6);
  check_struct_size(StatBoostBE, 6);

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
  } __packed_ws__(ItemCombination, 0x10);

  template <bool BE>
  struct TechniqueBoostT {
    U32T<BE> tech1 = 0;
    F32T<BE> boost1 = 0.0f;
    U32T<BE> tech2 = 0;
    F32T<BE> boost2 = 0.0f;
    U32T<BE> tech3 = 0;
    F32T<BE> boost3 = 0.0f;
  } __packed__;
  using TechniqueBoost = TechniqueBoostT<false>;
  using TechniqueBoostBE = TechniqueBoostT<true>;
  check_struct_size(TechniqueBoost, 0x18);
  check_struct_size(TechniqueBoostBE, 0x18);

  struct EventItem {
    parray<uint8_t, 3> item;
    uint8_t probability = 0;
  } __packed_ws__(EventItem, 4);

  struct UnsealableItem {
    parray<uint8_t, 3> item;
    uint8_t unused = 0;
  } __packed_ws__(UnsealableItem, 4);

  template <bool BE>
  struct NonWeaponSaleDivisorsT {
    F32T<BE> armor_divisor = 0.0f;
    F32T<BE> shield_divisor = 0.0f;
    F32T<BE> unit_divisor = 0.0f;
    F32T<BE> mag_divisor = 0.0f;
  } __packed__;
  using NonWeaponSaleDivisors = NonWeaponSaleDivisorsT<false>;
  using NonWeaponSaleDivisorsBE = NonWeaponSaleDivisorsT<true>;
  check_struct_size(NonWeaponSaleDivisors, 0x10);
  check_struct_size(NonWeaponSaleDivisorsBE, 0x10);

  ItemParameterTable(std::shared_ptr<const std::string> data, Version version);
  ~ItemParameterTable() = default;

  std::set<uint32_t> compute_all_valid_primary_identifiers() const;

  size_t num_weapons_in_class(uint8_t data1_1) const;
  const WeaponV4& get_weapon(uint8_t data1_1, uint8_t data1_2) const;
  size_t num_armors_or_shields_in_class(uint8_t data1_1) const;
  const ArmorOrShieldV4& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const;
  size_t num_units() const;
  const UnitV4& get_unit(uint8_t data1_2) const;
  size_t num_mags() const;
  const MagV4& get_mag(uint8_t data1_1) const;
  size_t num_tools_in_class(uint8_t data1_1) const;
  const ToolV4& get_tool(uint8_t data1_1, uint8_t data1_2) const;
  std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const;

  std::variant<const WeaponV4*, const ArmorOrShieldV4*, const UnitV4*, const MagV4*, const ToolV4*>
  definition_for_primary_identifier(uint32_t primary_identifier) const;

  float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const;
  const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t which) const;
  uint8_t get_item_stars(uint32_t id) const;
  uint8_t get_special_stars(uint8_t special) const;
  const Special& get_special(uint8_t special) const;
  uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const;
  uint8_t get_weapon_v1_replacement(uint8_t data1_1) const;

  uint32_t get_item_id(const ItemData& item) const;
  uint32_t get_item_team_points(const ItemData& item) const;
  uint8_t get_item_base_stars(const ItemData& item) const;
  uint8_t get_item_adjusted_stars(const ItemData& item, bool ignore_unidentified = false) const;
  bool is_item_rare(const ItemData& item) const;
  bool is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const;
  bool is_unsealable_item(const ItemData& param_1) const;
  const ItemCombination& get_item_combination(const ItemData& used_item, const ItemData& equipped_item) const;
  const std::vector<ItemCombination>& get_all_combinations_for_used_item(const ItemData& used_item) const;
  const std::map<uint32_t, std::vector<ItemCombination>>& get_all_item_combinations() const;
  size_t num_events() const;
  std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const;

  size_t price_for_item(const ItemData& item) const;

  size_t num_weapon_classes;
  size_t num_tool_classes;
  size_t item_stars_first_id;
  size_t item_stars_last_id;
  size_t special_stars_begin_index;
  size_t num_specials;
  size_t first_rare_mag_index;

protected:
  struct TableOffsetsDCProtos {
    /* ## / NTE  / 11/2000 */
    /* 00 / 0013 / 0013 */ le_uint32_t unknown_a0;
    /* 04 / 2E1C / 2EB8 */ le_uint32_t weapon_table;
    /* 08 / 2D94 / 2E28 */ le_uint32_t armor_table;
    /* 0C / 2DA4 / 2E38 */ le_uint32_t unit_table;
    /* 10 / 2DB4 / 2E48 */ le_uint32_t tool_table;
    /* 14 / 2DAC / 2E40 */ le_uint32_t mag_table;
    /* 18 / 1F98 / 202C */ le_uint32_t v1_replacement_table;
    /* 1C / 1994 / 1A28 */ le_uint32_t photon_color_table;
    /* 20 / 1C64 / 1CF8 */ le_uint32_t weapon_range_table;
    /* 24 / 1FBF / 2053 */ le_uint32_t weapon_sale_divisor_table;
    /* 28 / 1FE6 / 207A */ le_uint32_t sale_divisor_table;
    /* 2C / 2F54 / 2FF0 */ le_uint32_t mag_feed_table;
    /* 30 / 22A9 / 233D */ le_uint32_t star_value_table;
    /* 34 / 23EE / 2484 */ le_uint32_t unknown_a1;
    /* 38 / 275E / 27F4 */ le_uint32_t special_data_table;
    /* 3C / 2804 / 2898 */ le_uint32_t stat_boost_table;
    /* 40 / 1908 / 199C */ le_uint32_t shield_effect_table;
    /* 44 / 0668 / 0668 */ le_uint32_t unknown_a2;
    /* 48 / 030C / 030C */ le_uint32_t unknown_a3;
    /* 4C / 2CE4 / 2D78 */ le_uint32_t unknown_a4;
  } __packed_ws__(TableOffsetsDCProtos, 0x50);

  struct TableOffsetsV1V2 {
    // TODO: Is weapon count 0x89 or 0x8A? It could be that the last entry in
    // weapon_table is used for ???? items.
    /* ## / V1   / V2*/
    /* 00 / 0013 / 0013 */ le_uint32_t unknown_a0;
    /* 04 / 32E8 / 5AFC */ le_uint32_t weapon_table; // -> [{count, offset -> [WeaponV2]}](0x89)
    /* 08 / 3258 / 5A5C */ le_uint32_t armor_table; // -> [{count, offset -> [ArmorOrShieldV2]}](2; armors and shields)
    /* 0C / 3268 / 5A6C */ le_uint32_t unit_table; // -> {count, offset -> [UnitV2]} (last if out of range)
    /* 10 / 3278 / 5A7C */ le_uint32_t tool_table; // -> [{count, offset -> [ToolV2]}](0x10) (last if out of range)
    /* 14 / 3270 / 5A74 */ le_uint32_t mag_table; // -> {count, offset -> [MagV2]}
    /* 18 / 23C8 / 3DF8 */ le_uint32_t v1_replacement_table; // -> [uint8_t](0x89)
    /* 1C / 1DB0 / 2E4C */ le_uint32_t photon_color_table; // -> [0x24-byte structs](0x20)
    /* 20 / 2080 / 32CC */ le_uint32_t weapon_range_table; // -> ???
    /* 24 / 23F0 / 3E84 */ le_uint32_t weapon_sale_divisor_table; // -> [float](0x89)
    /* 28 / 248C / 40A8 */ le_uint32_t sale_divisor_table; // -> NonWeaponSaleDivisors
    /* 2C / 3420 / 5F4C */ le_uint32_t mag_feed_table; // -> MagFeedResultsTable
    /* 30 / 275C / 4378 */ le_uint32_t star_value_table; // -> [uint8_t](0x1C7)
    /* 34 / 28A2 / 45E4 */ le_uint32_t unknown_a1;
    /* 38 / 2C12 / 4540 */ le_uint32_t special_data_table; // -> [Special](0x29)
    /* 3C / 2CB8 / 58DC */ le_uint32_t stat_boost_table; // -> [StatBoost]
    /* 40 / 3198 / 5704 */ le_uint32_t shield_effect_table; // -> [8-byte structs]
  } __packed_ws__(TableOffsetsV1V2, 0x44);

  struct TableOffsetsGCNTE {
    /* 00 / 6F0C */ be_uint32_t weapon_table; // -> [{count, offset -> [WeaponV3/WeaponV4]}](0xED)
    /* 04 / 6E4C */ be_uint32_t armor_table; // -> [{count, offset -> [ArmorOrShieldV3/ArmorOrShieldV4]}](2; armors and shields)
    /* 08 / 6E5C */ be_uint32_t unit_table; // -> {count, offset -> [UnitV3/UnitV4]} (last if out of range)
    /* 0C / 6E6C */ be_uint32_t tool_table; // -> [{count, offset -> [ToolV3/ToolV4]}](0x1A) (last if out of range)
    /* 10 / 6E64 */ be_uint32_t mag_table; // -> {count, offset -> [MagV3/MagV4]}
    /* 14 / 47BC */ be_uint32_t v1_replacement_table; // -> [uint8_t](0xED)
    /* 18 / 37A4 */ be_uint32_t photon_color_table; // -> [0x24-byte structs](0x20)
    /* 1C / 3A74 */ be_uint32_t weapon_range_table; // -> ???
    /* 20 / 484C */ be_uint32_t weapon_sale_divisor_table; // -> [float](0xED)
    /* 24 / 4A80 */ be_uint32_t sale_divisor_table; // -> NonWeaponSaleDivisors
    /* 28 / 7384 */ be_uint32_t mag_feed_table; // -> MagFeedResultsTable
    /* 2C / 4D50 */ be_uint32_t star_value_table; // -> [uint8_t](0x330) (indexed by .id from weapon, armor, etc.)
    /* 30 / 4F72 */ be_uint32_t special_data_table; // -> [Special]
    /* 34 / 5018 */ be_uint32_t weapon_effect_table; // -> [16-byte structs]
    /* 38 / 68B8 */ be_uint32_t stat_boost_table; // -> [StatBoost]
    /* 3C / 61B8 */ be_uint32_t shield_effect_table; // -> [8-byte structs]
    /* 40 / 69D8 */ be_uint32_t max_tech_level_table; // -> MaxTechniqueLevels
    /* 44 / 737C */ be_uint32_t combination_table; // -> {count, offset -> [ItemCombination]}
    /* 48 / 68B0 */ be_uint32_t unknown_a1;
    /* 4C / 6B1C */ be_uint32_t tech_boost_table; // -> [TechniqueBoost] (always 0x2C of them? from counts struct?)
  } __packed_ws__(TableOffsetsGCNTE, 0x50);

  template <bool BE>
  struct TableOffsetsV3V4T {
    /* ## / GC   / BB */
    /* 00 / F078 / 14884 */ U32T<BE> weapon_table; // -> [{count, offset -> [WeaponV3/WeaponV4]}](0xED)
    /* 04 / EF90 / 1478C */ U32T<BE> armor_table; // -> [{count, offset -> [ArmorOrShieldV3/ArmorOrShieldV4]}](2; armors and shields)
    /* 08 / EFA0 / 1479C */ U32T<BE> unit_table; // -> {count, offset -> [UnitV3/UnitV4]} (last if out of range)
    /* 0C / EFB0 / 147AC */ U32T<BE> tool_table; // -> [{count, offset -> [ToolV3/ToolV4]}](0x1A) (last if out of range)
    /* 10 / EFA8 / 147A4 */ U32T<BE> mag_table; // -> {count, offset -> [MagV3/MagV4]}
    /* 14 / B88C / 0F4B8 */ U32T<BE> v1_replacement_table; // -> [uint8_t](0xED)
    /* 18 / A7FC / 0DE7C */ U32T<BE> photon_color_table; // -> [0x24-byte structs](0x20)
    /* 1C / AACC / 0E194 */ U32T<BE> weapon_range_table; // -> ???
    /* 20 / B938 / 0F5A8 */ U32T<BE> weapon_sale_divisor_table; // -> [float](0xED)
    /* 24 / BBCC / 0F83C */ U32T<BE> sale_divisor_table; // -> NonWeaponSaleDivisors
    /* 28 / F608 / 1502C */ U32T<BE> mag_feed_table; // -> MagFeedResultsTable
    /* 2C / BE9C / 0FB0C */ U32T<BE> star_value_table; // -> [uint8_t](0x330) (indexed by .id from weapon, armor, etc.)
    /* 30 / C100 / 0FE3C */ U32T<BE> special_data_table; // -> [Special]
    /* 34 / C1A4 / 0FEE0 */ U32T<BE> weapon_effect_table; // -> [16-byte structs]
    /* 38 / DE50 / 1275C */ U32T<BE> stat_boost_table; // -> [StatBoost]
    /* 3C / D6E4 / 11C80 */ U32T<BE> shield_effect_table; // -> [8-byte structs]
    /* 40 / DF88 / 12894 */ U32T<BE> max_tech_level_table; // -> MaxTechniqueLevels
    /* 44 / F5D0 / 14FF4 */ U32T<BE> combination_table; // -> {count, offset -> [ItemCombination]}
    /* 48 / DE48 / 12754 */ U32T<BE> unknown_a1;
    /* 4C / EB8C / 14278 */ U32T<BE> tech_boost_table; // -> [TechniqueBoost] (always 0x2C of them? from counts struct?)
    /* 50 / F5F0 / 15014 */ U32T<BE> unwrap_table; // -> {count, offset -> [{count, offset -> [EventItem]}]}
    /* 54 / F5F8 / 1501C */ U32T<BE> unsealable_table; // -> {count, offset -> [UnsealableItem]}
    /* 58 / F600 / 15024 */ U32T<BE> ranged_special_table; // -> {count, offset -> [4-byte structs]}
  } __packed__;
  using TableOffsetsV3V4 = TableOffsetsV3V4T<false>;
  using TableOffsetsV3V4BE = TableOffsetsV3V4T<true>;
  check_struct_size(TableOffsetsV3V4, 0x5C);
  check_struct_size(TableOffsetsV3V4BE, 0x5C);

  Version version;
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const TableOffsetsDCProtos* offsets_dc_protos;
  const TableOffsetsV1V2* offsets_v1_v2;
  const TableOffsetsGCNTE* offsets_gc_nte;
  const TableOffsetsV3V4* offsets_v3_le;
  const TableOffsetsV3V4BE* offsets_v3_be;
  const TableOffsetsV3V4* offsets_v4;

  // These are unused if offsets_v4 is not null (in that case, we just return
  // references pointing inside the data string)
  mutable std::unordered_map<uint16_t, WeaponV4> parsed_weapons;
  mutable std::vector<ArmorOrShieldV4> parsed_armors;
  mutable std::vector<ArmorOrShieldV4> parsed_shields;
  mutable std::vector<UnitV4> parsed_units;
  mutable std::vector<MagV4> parsed_mags;
  mutable std::unordered_map<uint16_t, ToolV4> parsed_tools;
  mutable std::vector<Special> parsed_specials;

  // Key is used_item. We can't index on (used_item, equipped_item) because
  // equipped_item may contain wildcards, and the matching order matters.
  mutable std::map<uint32_t, std::vector<ItemCombination>> item_combination_index;

  template <typename ToolDefT, bool BE>
  std::pair<uint8_t, uint8_t> find_tool_by_id_t(uint32_t tool_table_offset, uint32_t id) const;
  template <bool BE, typename OffsetsT>
  float get_sale_divisor_t(const OffsetsT* offsets, uint8_t data1_0, uint8_t data1_1) const;
  template <bool BE>
  size_t num_events_t(uint32_t base_offset) const;
  template <bool BE>
  std::pair<const ItemParameterTable::EventItem*, size_t> get_event_items_t(uint32_t base_offset, uint8_t event_number) const;
};

class MagEvolutionTable {
public:
  struct TableOffsets {
    // num_mags = 0x53 in BB, 0x43 in V3
    /* 00 / 0400 */ le_uint32_t unknown_a1; // -> [offset -> (0xC-byte struct)[num_mags], offset -> (same as first offset)]
    /* 04 / 0408 */ le_uint32_t unknown_a2; // -> (2-byte struct, or single word)[num_mags]
    /* 08 / 04AE */ le_uint32_t unknown_a3; // -> (0xA8 bytes; possibly (8-byte struct)[0x15])
    /* 0C / 0556 */ le_uint32_t unknown_a4; // -> (uint8_t)[num_mags]
    /* 10 / 05AC */ le_uint32_t unknown_a5; // -> (float)[0x48]
    /* 14 / 06CC */ le_uint32_t evolution_number; // -> (uint8_t)[num_mags]
  } __packed_ws__(TableOffsets, 0x18);

  struct EvolutionNumberTable {
    parray<uint8_t, 0x53> values;
  } __packed_ws__(EvolutionNumberTable, 0x53);

  MagEvolutionTable(std::shared_ptr<const std::string> data);
  ~MagEvolutionTable() = default;

  uint8_t get_evolution_number(uint8_t data1_1) const;

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const TableOffsets* offsets;
};
