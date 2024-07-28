#pragma once

#include <memory>

#include "CommonItemSet.hh"
#include "ItemParameterTable.hh"
#include "PSOEncryption.hh"
#include "PlayerSubordinates.hh"
#include "RareItemSet.hh"
#include "StaticGameData.hh"

class ItemCreator {
public:
  ItemCreator(
      std::shared_ptr<const CommonItemSet> common_item_set,
      std::shared_ptr<const RareItemSet> rare_item_set,
      std::shared_ptr<const ArmorRandomSet> armor_random_set,
      std::shared_ptr<const ToolRandomSet> tool_random_set,
      std::shared_ptr<const WeaponRandomSet> weapon_random_set,
      std::shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set,
      std::shared_ptr<const ItemParameterTable> item_parameter_table,
      std::shared_ptr<const ItemData::StackLimits> stack_limits,
      Episode episode,
      GameMode mode,
      uint8_t difficulty,
      uint8_t section_id,
      std::shared_ptr<PSOLFGEncryption> opt_rand_crypt,
      std::shared_ptr<const BattleRules> restrictions = nullptr);
  ~ItemCreator() = default;

  void set_random_crypt(std::shared_ptr<PSOLFGEncryption> new_random_crypt);

  struct DropResult {
    ItemData item;
    bool is_from_rare_table = false;
  };

  DropResult on_monster_item_drop(uint32_t enemy_type, uint8_t area);
  DropResult on_box_item_drop(uint8_t area);
  DropResult on_specialized_box_item_drop(uint8_t area, float def_z, uint32_t def0, uint32_t def1, uint32_t def2);

  ItemData base_item_for_specialized_box(uint32_t def0, uint32_t def1, uint32_t def2) const;

  std::vector<ItemData> generate_armor_shop_contents(size_t player_level);
  std::vector<ItemData> generate_tool_shop_contents(size_t player_level);
  std::vector<ItemData> generate_weapon_shop_contents(size_t player_level);

  // This function adjusts the item in-place, and returns the luck value.
  // See the comments in TekkerAdjustmentSet for what this value means.
  ssize_t apply_tekker_deltas(ItemData& item, uint8_t section_id);

  inline void set_restrictions(std::shared_ptr<const BattleRules> restrictions) {
    this->restrictions = restrictions;
  }
  inline uint8_t get_section_id() const {
    return this->section_id;
  }
  void set_section_id(uint8_t new_section_id);

private:
  phosg::PrefixedLogger log;
  Version logic_version;
  std::shared_ptr<const ItemData::StackLimits> stack_limits;
  Episode episode;
  GameMode mode;
  uint8_t difficulty;
  uint8_t section_id;
  std::shared_ptr<const RareItemSet> rare_item_set;
  std::shared_ptr<const ArmorRandomSet> armor_random_set;
  std::shared_ptr<const ToolRandomSet> tool_random_set;
  std::shared_ptr<const WeaponRandomSet> weapon_random_set;
  std::shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set;
  std::shared_ptr<const ItemParameterTable> item_parameter_table;
  std::shared_ptr<const CommonItemSet> common_item_set;
  std::shared_ptr<const CommonItemSet::Table> pt;
  std::shared_ptr<const BattleRules> restrictions;

  struct UnitResult {
    uint8_t unit;
    int8_t modifier;
  } __packed_ws__(UnitResult, 2);
  std::array<std::vector<UnitResult>, 13> unit_results_by_star_count;

  // Note: The original implementation uses 17 different random states for some
  // reason. We forego that and use only one for simplicity.
  // Originally, the 17 random states were used for:
  //   [0x00] - drop-anything rate check
  //   [0x01] - common item class check
  //   [0x02] - get_rand_from_weighted_tables16 determinants
  //   [0x03] - get_rand_from_weighted_tables8 determinants
  //   [0x04] - tech disk levels
  //   [0x05] - meseta amounts
  //   [0x06] - rare drop rate check
  //   [0x07] - rare weapon special table index
  //   [0x08] - apparently unused
  //   [0x09] - whether to generate a common weapon special
  //   [0x0A] - number of stars for common weapon special
  //   [0x0B] - unit modifiers
  //   [0x0C] - common armor DFP bonuses
  //   [0x0D] - common armor EVP bonuses
  //   [0x0E] - apparently unused
  //   [0x0F] - which common weapon special to generate
  //   [0x10] - apparently unused
  std::shared_ptr<PSOLFGEncryption> opt_rand_crypt;

  bool are_rare_drops_allowed() const;
  uint8_t normalize_area_number(uint8_t area) const;

  DropResult on_monster_item_drop_with_area_norm(uint32_t enemy_type, uint8_t area_norm);
  DropResult on_box_item_drop_with_area_norm(uint8_t area_norm);

  uint32_t rand_int(uint64_t max);
  float rand_float_0_1_from_crypt();

  template <size_t NumRanges>
  uint32_t choose_meseta_amount(
      const parray<CommonItemSet::Table::Range<uint16_t>, NumRanges> ranges,
      size_t table_index);

  bool should_allow_meseta_drops() const;

  ItemData check_rare_spec_and_create_rare_enemy_item(uint32_t enemy_type, uint8_t area_norm);
  ItemData check_rare_specs_and_create_rare_box_item(uint8_t area_norm);
  ItemData check_rate_and_create_rare_item(const RareItemSet::ExpandedDrop& drop, uint8_t area_norm);

  void generate_rare_weapon_bonuses(ItemData& item, uint32_t random_sample);
  void deduplicate_weapon_bonuses(ItemData& item) const;
  void set_item_kill_count_if_unsealable(ItemData& item) const;
  void set_item_unidentified_flag_if_not_challenge(ItemData& item) const;
  void set_tool_item_amount_to_1(ItemData& item) const;

  void generate_common_item_variances(uint32_t area_norm, ItemData& item);
  void generate_common_armor_slots_and_bonuses(ItemData& item);
  void generate_common_armor_slot_count(ItemData& item);
  void generate_common_armor_or_shield_type_and_variances(char area_norm, ItemData& item);
  void generate_common_tool_variances(uint32_t area_norm, ItemData& item);
  uint8_t generate_tech_disk_level(uint32_t tech_num, uint32_t area_norm);
  void generate_common_mag_variances(ItemData& item);
  void generate_common_weapon_variances(uint8_t area_norm, ItemData& item);
  void generate_common_weapon_grind(ItemData& item, uint8_t offset_within_subtype_range);
  void generate_common_weapon_bonuses(ItemData& item, uint8_t area_norm);
  void generate_common_weapon_special(ItemData& item, uint8_t area_norm);
  uint8_t choose_weapon_special(uint8_t det);
  void generate_unit_stars_tables();
  void generate_common_unit_variances(uint8_t stars, ItemData& item);
  void choose_tech_disk_level_for_tool_shop(ItemData& item, size_t player_level, uint8_t tech_num_index);
  static void clear_tool_item_if_invalid(ItemData& item);
  void clear_item_if_restricted(ItemData& item) const;

  static size_t get_table_index_for_armor_shop(size_t player_level);
  static bool shop_does_not_contain_duplicate_armor(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_tech_disk(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_or_too_many_similar_weapons(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_item_by_data1_0_1_2(
      const std::vector<ItemData>& shop, const ItemData& item);
  void generate_armor_shop_armors(
      std::vector<ItemData>& shop, size_t player_level);
  void generate_armor_shop_shields(
      std::vector<ItemData>& shop, size_t player_level);
  void generate_armor_shop_units(
      std::vector<ItemData>& shop, size_t player_level);

  static size_t get_table_index_for_tool_shop(size_t player_level);
  void generate_common_tool_shop_recovery_items(
      std::vector<ItemData>& shop, size_t player_level);
  void generate_rare_tool_shop_recovery_items(
      std::vector<ItemData>& shop, size_t player_level);
  void generate_tool_shop_tech_disks(
      std::vector<ItemData>& shop, size_t player_level);

  void generate_weapon_shop_item_grind(ItemData& item, size_t player_level);
  void generate_weapon_shop_item_special(ItemData& item, size_t player_level);
  void generate_weapon_shop_item_bonus1(ItemData& item, size_t player_level);
  void generate_weapon_shop_item_bonus2(ItemData& item, size_t player_level);

  template <typename IntT>
  IntT get_rand_from_weighted_tables(
      const IntT* tables, size_t offset, size_t num_values, size_t stride);
  template <typename IntT, size_t X>
  IntT get_rand_from_weighted_tables_1d(const parray<IntT, X>& tables);
  template <typename IntT, size_t X, size_t Y>
  IntT get_rand_from_weighted_tables_2d_vertical(
      const parray<parray<IntT, X>, Y>& tables, size_t offset);
};
