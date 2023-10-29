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
      GameVersion version,
      Episode episode,
      GameMode mode,
      uint8_t difficulty,
      uint8_t section_id,
      uint32_t random_seed,
      std::shared_ptr<const BattleRules> restrictions = nullptr);
  ~ItemCreator() = default;

  void clear_destroyed_entities();

  ItemData on_monster_item_drop(uint16_t entity_id, uint32_t enemy_type, uint8_t area);
  ItemData on_box_item_drop(uint16_t entity_id, uint8_t area);
  ItemData on_specialized_box_item_drop(uint16_t entity_id, uint32_t def0, uint32_t def1, uint32_t def2);

  std::vector<ItemData> generate_armor_shop_contents(size_t player_level);
  std::vector<ItemData> generate_tool_shop_contents(size_t player_level);
  std::vector<ItemData> generate_weapon_shop_contents(size_t player_level);

  // This function adjusts the item in-place, and returns the luck value.
  // See the comments in TekkerAdjustmentSet for what this value means.
  ssize_t apply_tekker_deltas(ItemData& item, uint8_t section_id);

  inline void set_restrictions(std::shared_ptr<const BattleRules> restrictions) {
    this->restrictions = restrictions;
  }

private:
  PrefixedLogger log;
  GameVersion version;
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
  std::shared_ptr<const CommonItemSet::Table> pt;
  std::shared_ptr<const BattleRules> restrictions;

  std::vector<uint8_t> unit_weights_table1;
  parray<int8_t, 0x0D> unit_weights_table2;

  // Note: The original implementation uses 17 different random states for some
  // reason. We forego that and use only one for simplicity.
  PSOV2Encryption random_crypt;
  std::unordered_set<uint16_t> destroyed_monsters;
  std::unordered_set<uint16_t> destroyed_boxes;

  inline bool is_v3() const {
    return (this->version != GameVersion::DC) && (this->version != GameVersion::PC);
  }

  bool are_rare_drops_allowed() const;
  uint8_t normalize_area_number(uint8_t area) const;

  ItemData on_monster_item_drop_with_norm_area(
      uint32_t enemy_type, uint8_t norm_area);
  ItemData on_box_item_drop_with_norm_area(uint8_t area_norm);

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

  void generate_common_item_variances(uint32_t norm_area, ItemData& item);
  void generate_common_armor_slots_and_bonuses(ItemData& item);
  void generate_common_armor_slot_count(ItemData& item);
  void generate_common_armor_or_shield_type_and_variances(
      char area_norm, ItemData& item);
  void generate_common_tool_variances(uint32_t area_norm, ItemData& item);
  uint8_t generate_tech_disk_level(uint32_t tech_num, uint32_t area_norm);
  void generate_common_tool_type(uint8_t tool_class, ItemData& item) const;
  void generate_common_mag_variances(ItemData& item) const;
  void generate_common_weapon_variances(uint8_t area_norm, ItemData& item);
  void generate_common_weapon_grind(ItemData& item,
      uint8_t offset_within_subtype_range);
  void generate_common_weapon_bonuses(ItemData& item, uint8_t area_norm);
  void generate_common_weapon_special(ItemData& item, uint8_t area_norm);
  uint8_t choose_weapon_special(uint8_t det);
  void generate_unit_weights_tables();
  void generate_common_unit_variances(uint8_t det, ItemData& item);
  void choose_tech_disk_level_for_tool_shop(
      ItemData& item, size_t player_level, uint8_t tech_num_index);
  static void clear_tool_item_if_invalid(ItemData& item);
  void clear_item_if_restricted(ItemData& item) const;

  static size_t get_table_index_for_armor_shop(size_t player_level);
  static bool shop_does_not_contain_duplicate_armor(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_tech_disk(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_or_too_many_similar_weapons(
      const std::vector<ItemData>& shop, const ItemData& item);
  static bool shop_does_not_contain_duplicate_item_by_primary_identifier(
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
