#pragma once

#include <stdint.h>

#include <memory>

#include "AssistServer.hh"
#include "DataIndexes.hh"
#include "DeckState.hh"
#include "PlayerState.hh"

namespace Episode3 {

class Server;

void compute_effective_range(
    parray<uint8_t, 9 * 9>& ret,
    std::shared_ptr<const CardIndex> card_index,
    uint16_t card_id,
    const Location& loc,
    std::shared_ptr<const MapAndRulesState> map_and_rules,
    phosg::PrefixedLogger* log = nullptr);

bool card_linkage_is_valid(
    std::shared_ptr<const CardIndex::CardEntry> right_def,
    std::shared_ptr<const CardIndex::CardEntry> left_def,
    std::shared_ptr<const CardIndex::CardEntry> sc_def,
    bool has_permission_effect);

class RulerServer {
public:
  struct MovePath {
    int32_t length;
    uint32_t remaining_distance;
    Location end_loc;
    parray<Location, 11> step_locs;
    uint32_t num_occupied_tiles;
    uint32_t cost;

    MovePath();
    void add_step(const Location& loc);
    uint32_t get_cost() const;
    uint32_t get_length_plus1() const;
    void reset_totals();
    bool is_valid() const;
  };

  explicit RulerServer(std::shared_ptr<Server> server);
  std::shared_ptr<Server> server();
  std::shared_ptr<const Server> server() const;

  ActionChainWithConds* action_chain_with_conds_for_card_ref(uint16_t card_ref);
  const ActionChainWithConds* action_chain_with_conds_for_card_ref(uint16_t card_ref) const;
  bool any_attack_action_card_is_support_tech_or_support_pb(const ActionState& pa) const;
  bool card_has_pierce_or_rampage(
      uint8_t client_id,
      ConditionType cond_type,
      bool* out_has_rampage,
      uint16_t attacker_card_ref,
      uint16_t action_card_ref,
      uint8_t def_effect_index,
      AttackMedium attack_medium) const;
  bool attack_action_has_rampage_and_not_pierce(const ActionState& pa, uint16_t card_ref) const;
  bool attack_action_has_pierce_and_not_rampage(const ActionState& pa, uint8_t client_id) const;
  size_t count_targets_with_rampage_and_not_pierce_nte(const ActionState& as) const;
  size_t count_targets_with_pierce_and_not_rampage_nte(const ActionState& as) const;
  bool card_exists_by_status(const CardShortStatus& stat) const;
  bool card_has_mighty_knuckle(uint32_t card_ref) const;
  uint16_t card_id_for_card_ref(uint16_t card_ref) const;
  static bool card_id_is_boss_sc(uint16_t card_id);
  static bool card_id_is_support_tech_or_support_pb(uint16_t card_id);
  bool card_ref_can_attack(uint16_t card_ref);
  bool card_ref_can_move(uint8_t client_id, uint16_t card_ref, bool ignore_atk_points) const;
  bool card_ref_has_class_usability_condition(uint16_t card_ref) const;
  bool card_ref_has_free_maneuver(uint16_t card_ref) const;
  bool card_ref_is_aerial(uint16_t card_ref) const;
  bool card_ref_is_aerial_or_has_free_maneuver(uint16_t card_ref) const;
  bool card_ref_is_boss_sc(uint32_t card_ref) const;
  bool card_ref_or_any_set_card_has_condition_46(uint16_t card_ref) const;
  bool card_ref_or_sc_has_fixed_range(uint16_t card_ref) const;
  bool check_move_path_and_get_cost(
      uint8_t client_id,
      uint16_t card_ref,
      parray<uint8_t, 0x100>* visited_map,
      MovePath* out_path,
      uint32_t* out_cost) const;
  bool check_pierce_and_rampage(
      uint16_t card_ref,
      ConditionType cond_type,
      bool* out_has_pierce,
      uint16_t attacker_card_ref,
      uint16_t action_card_ref,
      uint8_t def_effect_index,
      AttackMedium attack_medium) const;
  bool check_usability_or_apply_condition_for_card_refs(
      uint16_t card_ref1,
      uint16_t card_ref2,
      uint16_t card_ref3,
      uint8_t def_effect_index,
      AttackMedium attack_medium) const;
  bool check_usability_or_condition_apply(
      uint8_t client_id1,
      uint16_t card_id1,
      uint8_t client_id2,
      uint16_t card_id2,
      uint16_t card_id3,
      uint8_t def_effect_index,
      bool is_condition_check,
      AttackMedium attack_medium) const;
  uint16_t compute_attack_or_defense_costs(
      const ActionState& pa,
      bool allow_mighty_knuckle,
      uint8_t* out_ally_cost) const;
  bool compute_effective_range_and_target_mode_for_attack(
      const ActionState& pa,
      uint16_t* out_effective_card_id,
      TargetMode* out_effective_target_mode,
      uint16_t* out_orig_card_ref) const;
  size_t count_rampage_targets_for_attack(const ActionState& pa, uint8_t client_id) const;
  bool defense_card_can_apply_to_attack(
      uint16_t defense_card_ref,
      uint16_t attacker_card_ref,
      uint16_t attacker_sc_card_ref) const;
  bool defense_card_matches_any_attack_card_top_color(const ActionState& pa) const;
  std::shared_ptr<const CardIndex::CardEntry> definition_for_card_ref(uint16_t card_ref) const;
  int32_t error_code_for_client_setting_card(
      uint8_t client_id,
      uint16_t card_ref,
      const Location* loc,
      uint8_t assist_target_client_id) const;
  bool find_condition_on_card_ref(
      uint16_t card_ref,
      ConditionType cond_type,
      Condition* out_se = nullptr,
      size_t* out_value_sum = nullptr,
      bool find_first_instead_of_max = false) const;
  bool flood_fill_move_path(
      const ActionChainWithConds& chain,
      int8_t x,
      int8_t y,
      Direction direction,
      uint8_t max_atk_points,
      int16_t max_distance,
      bool is_free_maneuver_or_aerial,
      bool is_aerial,
      parray<uint8_t, 0x100>* visited_map,
      MovePath* path,
      size_t num_occupied_tiles,
      size_t num_vacant_tiles) const;
  uint16_t get_ally_sc_card_ref(uint16_t card_ref) const;
  std::shared_ptr<const CardIndex::CardEntry> definition_for_card_id(uint32_t card_id) const;
  uint32_t get_card_id_with_effective_range(
      uint16_t card_ref, uint16_t card_id_override, TargetMode* out_target_mode) const;
  uint8_t get_card_ref_max_hp(uint16_t card_ref) const;
  bool get_creature_summon_area(
      uint8_t client_id, Location* out_loc, uint8_t* out_region_size) const;
  std::shared_ptr<HandAndEquipState> get_hand_and_equip_state_for_client_id(uint8_t client_id);
  std::shared_ptr<const HandAndEquipState> get_hand_and_equip_state_for_client_id(uint8_t client_id) const;
  bool get_move_path_length_and_cost(
      uint32_t client_id,
      uint32_t card_ref,
      const Location& loc,
      uint32_t* out_length,
      uint32_t* out_cost) const;
  ssize_t get_path_cost(
      const ActionChainWithConds& chain,
      ssize_t path_length,
      ssize_t cost_penalty) const;
  ActionType get_pending_action_type(const ActionState& pa) const;
  bool is_attack_valid(const ActionState& pa);
  bool is_attack_or_defense_valid(const ActionState& pa);
  bool is_card_ref_in_hand(uint16_t card_ref) const;
  bool is_defense_valid(const ActionState& pa);
  void link_objects(
      std::shared_ptr<MapAndRulesState> map_and_rules,
      std::shared_ptr<StateFlags> state_flags,
      std::shared_ptr<AssistServer> assist_server);
  size_t max_move_distance_for_card_ref(uint32_t card_ref) const;
  static void offsets_for_direction(const Location& loc, int32_t* out_x_offset, int32_t* out_y_offset);
  void register_player(
      uint8_t client_id,
      std::shared_ptr<HandAndEquipState> hes,
      std::shared_ptr<parray<CardShortStatus, 0x10>> short_statuses,
      std::shared_ptr<DeckEntry> deck_entry,
      std::shared_ptr<parray<ActionChainWithConds, 9>> set_card_action_chains,
      std::shared_ptr<parray<ActionMetadata, 9>> set_card_action_metadatas);
  void replace_D1_D2_rank_cards_with_Attack(parray<le_uint16_t, 0x1F>& card_ids) const;
  AttackMedium get_attack_medium(const ActionState& pa) const;
  void set_client_team_id(uint8_t client_id, uint8_t team_id);
  int32_t set_cost_for_card(uint8_t client_id, uint16_t card_ref) const;
  const CardShortStatus* short_status_for_card_ref(uint16_t card_ref) const;
  bool should_allow_attacks_on_current_turn() const;
  int32_t verify_deck(
      const parray<le_uint16_t, 0x1F>& card_ids,
      const parray<uint8_t, 0x2F0>* owned_card_counts = nullptr) const;

private:
  std::weak_ptr<Server> w_server;

public:
  bcarray<std::shared_ptr<HandAndEquipState>, 4> hand_and_equip_states;
  bcarray<std::shared_ptr<parray<CardShortStatus, 0x10>>, 4> short_statuses;
  bcarray<std::shared_ptr<DeckEntry>, 4> deck_entries;
  bcarray<std::shared_ptr<parray<ActionChainWithConds, 9>>, 4> set_card_action_chains;
  bcarray<std::shared_ptr<parray<ActionMetadata, 9>>, 4> set_card_action_metadatas;
  std::shared_ptr<MapAndRulesState> map_and_rules;
  std::shared_ptr<StateFlags> state_flags;
  std::shared_ptr<AssistServer> assist_server;
  parray<uint8_t, 4> team_id_for_client_id;
  int32_t error_code1;
  int32_t error_code2;
  int32_t error_code3;
};

} // namespace Episode3
