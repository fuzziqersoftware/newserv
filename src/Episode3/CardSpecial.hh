#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

struct InterferenceProbabilityEntry {
  uint16_t card_id;
  uint8_t attack_probability;
  uint8_t defense_probability;
};

const InterferenceProbabilityEntry* get_interference_probability_entry(
    uint16_t row_card_id,
    uint16_t column_card_id,
    bool is_attack);

class CardSpecial {
public:
  enum class ExpressionTokenType {
    SPACE = 0, // Also used for end of string (get_next_expr_token returns null)
    REFERENCE = 1, // Reference to a value from the env stats (e.g. hp)
    NUMBER = 2, // Constant value (e.g. 2)
    SUBTRACT = 3, // "-" in input string
    ADD = 4, // "+" in input string
    ROUND_DIVIDE = 5, // "/" in input string
    FLOOR_DIVIDE = 6, // "//" in input string
    MULTIPLY = 7, // "*" in input string
  };

  struct DiceRoll {
    uint8_t client_id;
    uint8_t unknown_a2;
    uint8_t value;
    bool value_used_in_expr;
    uint16_t unknown_a5;

    DiceRoll();
    void clear();
  };

  struct AttackEnvStats {
    uint32_t num_set_cards; // "f" in expr
    uint32_t dice_roll_value1; // "d" in expr
    uint32_t effective_ap; // "ap" in expr
    uint32_t effective_tp; // "tp" in expr
    uint32_t current_hp; // "hp" in expr
    uint32_t max_hp; // "mhp" in expr
    uint32_t effective_ap_if_not_tech; // "dm" in expr
    uint32_t effective_ap_if_not_physical; // "tdm" in expr
    uint32_t player_num_destroyed_fcs; // "tf" in expr
    uint32_t player_num_atk_points; // "ac" in expr
    uint32_t defined_max_hp; // "php" in expr
    uint32_t dice_roll_value2; // "dc" in expr
    uint32_t card_cost; // "cs" in expr
    uint32_t total_num_set_cards; // "a" in expr
    uint32_t action_cards_ap; // "kap" in expr
    uint32_t action_cards_tp; // "ktp" in expr
    uint32_t unknown_a1; // "dn" in expr
    uint32_t num_item_or_creature_cards_in_hand; // "hf" in expr
    uint32_t num_destroyed_ally_fcs; // "df" in expr
    uint32_t target_team_num_set_cards; // "ff" in expr
    uint32_t condition_giver_team_num_set_cards; // "ef" in expr
    uint32_t num_native_creatures; // "bi" in expr
    uint32_t num_a_beast_creatures; // "ab" in expr
    uint32_t num_machine_creatures; // "mc" in expr
    uint32_t num_dark_creatures; // "dk" in expr
    uint32_t num_sword_type_items; // "sa" in expr
    uint32_t num_gun_type_items; // "gn" in expr
    uint32_t num_cane_type_items; // "wd" in expr
    uint32_t effective_ap_if_not_tech2; // "tt" in expr
    uint32_t team_dice_boost; // "lv" in expr
    uint32_t sc_effective_ap; // "adm" in expr
    uint32_t attack_bonus; // "ddm" in expr
    uint32_t num_sword_type_items_on_team; // "sat" in expr
    uint32_t target_attack_bonus; // "edm" in expr
    uint32_t last_attack_preliminary_damage; // "ldm" in expr
    uint32_t last_attack_damage; // "rdm" in expr
    uint32_t total_last_attack_damage; // "fdm" in expr
    uint32_t last_attack_damage_count; // "ndm" in expr
    uint32_t target_current_hp; // "ehp" in expr

    AttackEnvStats();
    void clear();

    uint32_t at(size_t offset) const;
  } __attribute__((packed));

  CardSpecial(std::shared_ptr<Server> server);
  std::shared_ptr<Server> server();
  std::shared_ptr<const Server> server() const;

  void debug_log(const char* fmt, ...) const ATTR_PRINTF(2, 3);

  void adjust_attack_damage_due_to_conditions(
      std::shared_ptr<const Card> target_card, int16_t* inout_damage, uint16_t attacker_card_ref);
  void adjust_dice_boost_if_team_has_condition_52(
      uint8_t team_id, uint8_t* inout_dice_boost, std::shared_ptr<const Card> card);
  void apply_action_conditions(
      uint8_t when,
      std::shared_ptr<const Card> attacker_card,
      std::shared_ptr<Card> defender_card,
      uint32_t flags,
      const ActionState* as);
  bool apply_attribute_guard_if_possible(
      uint32_t flags,
      CardClass card_class,
      std::shared_ptr<Card> card,
      uint16_t condition_giver_card_ref,
      uint16_t attacker_card_ref);
  bool apply_defense_condition(
      uint8_t when,
      Condition* defender_cond,
      uint8_t cond_index,
      const ActionState& defense_state,
      std::shared_ptr<Card> defender_card,
      uint32_t flags,
      bool unknown_p8);
  bool apply_defense_conditions(
      const ActionState& as,
      uint8_t when,
      std::shared_ptr<Card> defender_card,
      uint32_t flags);
  bool apply_stat_deltas_to_all_cards_from_all_conditions_with_card_ref(
      uint16_t card_ref);
  bool apply_stat_deltas_to_card_from_condition_and_clear_cond(
      Condition& cond, std::shared_ptr<Card> card);
  bool apply_stats_deltas_to_card_from_all_conditions_with_card_ref(
      uint16_t card_ref, std::shared_ptr<Card> card);
  bool card_has_condition_with_ref(
      std::shared_ptr<const Card> card,
      ConditionType cond_type,
      uint16_t card_ref,
      uint16_t match_card_ref) const;
  bool card_is_destroyed(std::shared_ptr<const Card> card) const;
  void compute_attack_ap(
      std::shared_ptr<const Card> target_card,
      int16_t* out_value,
      uint16_t attacker_card_ref);
  AttackEnvStats compute_attack_env_stats(
      const ActionState& pa,
      std::shared_ptr<const Card> card,
      const DiceRoll& dice_roll,
      uint16_t target_card_ref,
      uint16_t condition_giver_card_ref);
  std::shared_ptr<Card> compute_replaced_target_based_on_conditions(
      uint16_t target_card_ref,
      int unknown_p3,
      int unknown_p4,
      uint16_t attacker_card_ref,
      uint16_t set_card_ref,
      int unknown_p7,
      uint32_t* unknown_p9,
      uint8_t def_effect_index,
      uint32_t* unknown_p11,
      uint16_t sc_card_ref);
  StatSwapType compute_stat_swap_type(std::shared_ptr<const Card> card) const;
  void compute_team_dice_boost(uint8_t team_id);
  bool condition_has_when_20_or_21(const Condition& cond) const;
  size_t count_action_cards_with_condition_for_all_current_attacks(
      ConditionType cond_type, uint16_t card_ref) const;
  size_t count_action_cards_with_condition_for_current_attack(
      std::shared_ptr<const Card> card, ConditionType cond_type, uint16_t card_ref) const;
  size_t count_cards_with_card_id_set_by_player_except_card_ref(
      uint16_t card_id, uint16_t card_ref) const;
  std::vector<std::shared_ptr<const Card>> get_all_set_cards_by_team_and_class(
      CardClass card_class, uint8_t team_id, bool exclude_destroyed_cards) const;
  ActionState create_attack_state_from_card_action_chain(
      std::shared_ptr<const Card> attacker_card) const;
  ActionState create_defense_state_for_card_pair_action_chains(
      std::shared_ptr<const Card> attacker_card,
      std::shared_ptr<const Card> defender_card) const;
  void destroy_card_if_hp_zero(
      std::shared_ptr<Card> card, uint16_t attacker_card_ref);
  bool evaluate_effect_arg2_condition(
      const ActionState& as,
      std::shared_ptr<const Card> card,
      const char* arg2_text,
      DiceRoll& dice_roll,
      uint16_t set_card_ref,
      uint16_t sc_card_ref,
      uint8_t random_percent,
      uint8_t when) const;
  int32_t evaluate_effect_expr(
      const AttackEnvStats& ast,
      const char* expr,
      DiceRoll& dice_roll) const;
  bool execute_effect(
      Condition& cond,
      std::shared_ptr<Card> card,
      int16_t expr_value,
      int16_t unknown_p5,
      ConditionType cond_type,
      uint unknown_p7,
      uint16_t attacker_card_ref);
  const Condition* find_condition_with_parameters(
      std::shared_ptr<const Card> card,
      ConditionType cond_type,
      uint16_t set_card_ref,
      uint8_t def_effect_index) const;
  Condition* find_condition_with_parameters(
      std::shared_ptr<Card> card,
      ConditionType cond_type,
      uint16_t set_card_ref,
      uint8_t def_effect_index) const;
  static void get_card1_loc_with_card2_opposite_direction(
      Location* out_loc,
      std::shared_ptr<const Card> card1,
      std::shared_ptr<const Card> card2);
  uint16_t get_card_id_with_effective_range(
      std::shared_ptr<const Card> card1, uint16_t default_card_id, std::shared_ptr<const Card> card2) const;
  static void get_effective_ap_tp(
      StatSwapType type,
      int16_t* effective_ap,
      int16_t* effective_tp,
      int16_t hp,
      int16_t ap,
      int16_t tp);
  const char* get_next_expr_token(
      const char* expr, ExpressionTokenType* out_type, int32_t* out_value) const;
  std::vector<std::shared_ptr<const Card>> get_targeted_cards_for_condition(
      uint16_t card_ref,
      uint8_t def_effect_index,
      uint16_t setter_card_ref,
      const ActionState& as,
      int16_t p_target_type,
      bool apply_usability_filters) const;
  std::vector<std::shared_ptr<Card>> get_targeted_cards_for_condition(
      uint16_t card_ref,
      uint8_t def_effect_index,
      uint16_t setter_card_ref,
      const ActionState& as,
      int16_t p_target_type,
      bool apply_usability_filters);
  bool is_card_targeted_by_condition(
      const Condition& cond, const ActionState& as, std::shared_ptr<const Card> card) const;
  void on_card_set(std::shared_ptr<PlayerState> ps, uint16_t card_ref);
  const CardDefinition::Effect* original_definition_for_condition(
      const Condition& cond) const;
  bool card_ref_has_ability_trap(const Condition& eff) const;
  void send_6xB4x06_for_exp_change(
      std::shared_ptr<const Card> card,
      uint16_t attacker_card_ref,
      uint8_t dice_roll_value,
      bool unknown_p5) const;
  void send_6xB4x06_for_card_destroyed(
      std::shared_ptr<const Card> destroyed_card, uint16_t attacker_card_ref) const;
  uint16_t send_6xB4x06_if_card_ref_invalid(
      uint16_t card_ref, int16_t value) const;
  void send_6xB4x06_for_stat_delta(
      std::shared_ptr<const Card> card,
      uint16_t attacker_card_ref,
      uint32_t flags,
      int16_t hp_delta,
      bool unknown_p6,
      bool unknown_p7) const;
  bool should_cancel_condition_due_to_anti_abnormality(
      const CardDefinition::Effect& eff,
      std::shared_ptr<const Card> card,
      uint16_t target_card_ref,
      uint16_t sc_card_ref) const;
  bool should_return_card_ref_to_hand_on_destruction(
      uint16_t card_ref) const;
  size_t sum_last_attack_damage(
      std::vector<std::shared_ptr<const Card>>* out_cards,
      int32_t* out_damage_sum,
      size_t* out_damage_count) const;
  void update_condition_orders(std::shared_ptr<Card> card);
  int16_t max_all_attack_bonuses(size_t* out_count) const;
  void unknown_80244AA8(std::shared_ptr<Card> card);
  void check_for_defense_interference(
      std::shared_ptr<const Card> attacker_card,
      std::shared_ptr<Card> target_card,
      int16_t* inout_unknown_p4);
  void unknown_8024C2B0(
      uint32_t when,
      uint16_t set_card_ref,
      const ActionState& as,
      uint16_t sc_card_ref,
      bool apply_defense_condition_to_all_cards = true,
      uint16_t apply_defense_condition_to_card_ref = 0xFFFF);
  std::vector<std::shared_ptr<const Card>> get_all_set_cards() const;
  std::vector<std::shared_ptr<const Card>> find_cards_by_condition_inc_exc(
      ConditionType include_cond,
      ConditionType exclude_cond = ConditionType::NONE,
      AssistEffect include_eff = AssistEffect::NONE,
      AssistEffect exclude_eff = AssistEffect::NONE) const;
  void clear_invalid_conditions_on_card(
      std::shared_ptr<Card> card, const ActionState& as);
  void on_card_destroyed(
      std::shared_ptr<Card> attacker_card, std::shared_ptr<Card> destroyed_card);
  std::vector<std::shared_ptr<const Card>> find_cards_in_hp_range(
      int16_t min, int16_t max) const;
  std::vector<std::shared_ptr<const Card>> find_all_cards_by_aerial_attribute(bool is_aerial) const;
  std::vector<std::shared_ptr<const Card>> find_cards_damaged_by_at_least(int16_t damage) const;
  std::vector<std::shared_ptr<const Card>> find_all_set_cards_on_client_team(uint8_t client_id) const;
  std::vector<std::shared_ptr<const Card>> find_all_cards_on_same_or_other_team(uint8_t client_id, bool same_team) const;
  std::shared_ptr<const Card> sc_card_for_client_id(uint8_t client_id) const;
  std::shared_ptr<const Card> get_attacker_card(const ActionState& as) const;
  std::vector<std::shared_ptr<const Card>> get_attacker_card_and_sc_if_item(const ActionState& as) const;
  std::vector<std::shared_ptr<const Card>> find_all_set_cards_with_cost_in_range(uint8_t min_cost, uint8_t max_cost) const;
  std::vector<std::shared_ptr<const Card>> filter_cards_by_range(
      const std::vector<std::shared_ptr<const Card>>& cards,
      std::shared_ptr<const Card> card1,
      const Location& card1_loc,
      std::shared_ptr<const Card> card2) const;
  void unknown_8024AAB8(const ActionState& as);
  void unknown_80244BE4(std::shared_ptr<Card> unknown_p2);
  void unknown_80244CA8(std::shared_ptr<Card> card);
  template <uint8_t When1, uint8_t When2>
  void unknown1_t(
      std::shared_ptr<Card> unknown_p2, const ActionState* existing_as = nullptr);
  void unknown_80249060(std::shared_ptr<Card> unknown_p2);
  void unknown_80249254(std::shared_ptr<Card> unknown_p2);
  void unknown_8024945C(std::shared_ptr<Card> unknown_p2, const ActionState& existing_as);
  void unknown_8024966C(std::shared_ptr<Card> unknown_p2, const ActionState* existing_as);
  static std::shared_ptr<Card> sc_card_for_card(std::shared_ptr<Card> unknown_p2);
  void unknown_8024A9D8(const ActionState& pa, uint16_t action_card_ref);
  void check_for_attack_interference(std::shared_ptr<Card> unknown_p2);
  template <uint8_t When1, uint8_t When2, uint8_t When3, uint8_t When4>
  void unknown_t2(std::shared_ptr<Card> unknown_p2);
  void unknown_8024997C(std::shared_ptr<Card> card);
  void unknown_8024A394(std::shared_ptr<Card> card);
  bool client_has_atk_dice_boost_condition(uint8_t client_id);
  void unknown_8024A6DC(
      std::shared_ptr<Card> unknown_p2, std::shared_ptr<Card> unknown_p3);
  std::vector<std::shared_ptr<const Card>> find_all_sc_cards_of_class(
      CardClass card_class) const;

private:
  std::weak_ptr<Server> w_server;
  ActionState unknown_action_state_a1;
  ActionState action_state;
  uint16_t unknown_a2;
};

} // namespace Episode3
