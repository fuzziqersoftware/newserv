#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

class ServerBase;
class Server;
class Card;

struct Condition {
  ConditionType type;
  uint8_t remaining_turns;
  int8_t a_arg_value;
  uint8_t dice_roll_value;
  uint8_t flags;
  uint8_t card_definition_effect_index;
  le_uint16_t card_ref;
  le_int16_t value;
  le_uint16_t condition_giver_card_ref;
  uint8_t random_percent;
  int8_t value8;
  uint8_t order;
  uint8_t unknown_a8;

  Condition();
  bool operator==(const Condition& other) const;
  bool operator!=(const Condition& other) const;

  void clear();
  void clear_FF();

  std::string str() const;
} __attribute__((packed));

struct EffectResult {
  le_uint16_t attacker_card_ref;
  le_uint16_t target_card_ref;
  int8_t value;
  int8_t current_hp;
  int8_t ap;
  int8_t tp;
  uint8_t flags;
  int8_t operation; // May be a negative condition number
  uint8_t condition_index;
  uint8_t dice_roll_value;

  EffectResult();
  bool operator==(const EffectResult& other) const;
  bool operator!=(const EffectResult& other) const;

  std::string str() const;

  void clear();
} __attribute__((packed));

struct CardShortStatus {
  le_uint16_t card_ref;
  le_uint16_t current_hp;
  le_uint32_t card_flags;
  Location loc;
  le_uint16_t unused1;
  int8_t max_hp;
  uint8_t unused2;

  CardShortStatus();
  bool operator==(const CardShortStatus& other) const;
  bool operator!=(const CardShortStatus& other) const;

  void clear();
  void clear_FF();

  std::string str() const;
} __attribute__((packed));

struct ActionState {
  le_uint16_t client_id;
  uint8_t unused;
  Direction facing_direction;
  le_uint16_t attacker_card_ref;
  le_uint16_t defense_card_ref;
  parray<le_uint16_t, 4 * 9> target_card_refs;
  parray<le_uint16_t, 9> action_card_refs;
  le_uint16_t original_attacker_card_ref;

  ActionState();
  bool operator==(const ActionState& other) const;
  bool operator!=(const ActionState& other) const;

  void clear();

  std::string str() const;
} __attribute__((packed));

struct ActionChain {
  int8_t effective_ap;
  int8_t effective_tp;
  int8_t ap_effect_bonus;
  int8_t damage;
  le_uint16_t acting_card_ref;
  le_uint16_t unknown_card_ref_a3;
  parray<le_uint16_t, 8> attack_action_card_refs;
  uint8_t attack_action_card_ref_count;
  AttackMedium attack_medium;
  uint8_t target_card_ref_count;
  ActionSubphase action_subphase;
  uint8_t strike_count;
  int8_t damage_multiplier;
  uint8_t attack_number;
  int8_t tp_effect_bonus;
  uint8_t unused1;
  uint8_t unused2;
  int8_t card_ap;
  int8_t card_tp;
  le_uint32_t flags;
  parray<le_uint16_t, 4 * 9> target_card_refs;

  ActionChain();
  bool operator==(const ActionChain& other) const;
  bool operator!=(const ActionChain& other) const;

  void clear();
  void clear_FF();

  std::string str() const;
} __attribute__((packed));

struct ActionChainWithConds {
  ActionChain chain;
  parray<Condition, 9> conditions;

  ActionChainWithConds();
  bool operator==(const ActionChainWithConds& other) const;
  bool operator!=(const ActionChainWithConds& other) const;

  void clear();
  void clear_FF();
  void clear_inner();
  void clear_target_card_refs();
  void reset();

  bool check_flag(uint32_t flags) const;
  void clear_flags(uint32_t flags);
  void set_flags(uint32_t flags);

  void add_attack_action_card_ref(uint16_t card_ref, std::shared_ptr<Server> server);
  void add_target_card_ref(uint16_t card_ref);

  void compute_attack_medium(std::shared_ptr<Server> server);
  bool get_condition_value(
      ConditionType cond_type,
      uint16_t card_ref,
      uint8_t def_effect_index,
      uint16_t value,
      uint16_t* out_value) const;

  void set_action_subphase_from_card(std::shared_ptr<const Card> card);
  bool unknown_8024DEC4() const;

  std::string str() const;
} __attribute__((packed));

struct ActionMetadata {
  le_uint16_t card_ref;
  uint8_t target_card_ref_count;
  uint8_t defense_card_ref_count;
  ActionSubphase action_subphase;
  int8_t defense_power;
  int8_t defense_bonus;
  int8_t attack_bonus;
  le_uint32_t flags;
  parray<le_uint16_t, 4 * 9> target_card_refs;
  parray<le_uint16_t, 8> defense_card_refs;
  parray<le_uint16_t, 8> original_attacker_card_refs;

  ActionMetadata();
  bool operator==(const ActionMetadata& other) const;
  bool operator!=(const ActionMetadata& other) const;

  std::string str() const;

  void clear();
  void clear_FF();

  bool check_flag(uint32_t mask) const;
  void set_flags(uint32_t flags);
  void clear_flags(uint32_t flags);

  void clear_defense_and_attacker_card_refs();
  void clear_target_card_refs();
  void add_target_card_ref(uint16_t card_ref);
  void add_defense_card_ref(
      uint16_t defense_card_ref,
      std::shared_ptr<Card> card,
      uint16_t original_attacker_card_ref);
} __attribute__((packed));

struct HandAndEquipState {
  parray<uint8_t, 2> dice_results;
  uint8_t atk_points;
  uint8_t def_points;
  uint8_t atk_points2; // TODO: rename this to something more appropriate
  uint8_t unknown_a1;
  uint8_t total_set_cards_cost;
  uint8_t is_cpu_player;
  le_uint32_t assist_flags;
  parray<le_uint16_t, 6> hand_card_refs;
  le_uint16_t assist_card_ref;
  parray<le_uint16_t, 8> set_card_refs;
  le_uint16_t sc_card_ref;
  parray<le_uint16_t, 6> hand_card_refs2;
  parray<le_uint16_t, 8> set_card_refs2;
  le_uint16_t assist_card_ref2;
  le_uint16_t assist_card_set_number;
  le_uint16_t assist_card_id;
  uint8_t assist_remaining_turns;
  uint8_t assist_delay_turns;
  uint8_t atk_bonuses;
  uint8_t def_bonuses;
  parray<uint8_t, 2> unused2;

  HandAndEquipState();
  bool operator==(const HandAndEquipState& other) const;
  bool operator!=(const HandAndEquipState& other) const;

  void clear();
  void clear_FF();

  std::string str() const;
} __attribute__((packed));

struct PlayerBattleStats {
  le_uint16_t damage_given;
  le_uint16_t damage_taken;
  le_uint16_t num_opponent_cards_destroyed;
  le_uint16_t num_owned_cards_destroyed;
  le_uint16_t total_move_distance;
  le_uint16_t num_cards_set;
  le_uint16_t num_item_or_creature_cards_set;
  le_uint16_t num_attack_actions_set;
  le_uint16_t num_tech_cards_set;
  le_uint16_t num_assist_cards_set;
  le_uint16_t defense_actions_set_on_self;
  le_uint16_t defense_actions_set_on_ally;
  le_uint16_t num_cards_drawn;
  le_uint16_t max_attack_damage;
  le_uint16_t max_attack_combo_size;
  le_uint16_t num_attacks_given;
  le_uint16_t num_attacks_taken;
  le_uint16_t sc_damage_taken;
  le_uint16_t action_card_negated_damage;
  le_uint16_t unused;

  PlayerBattleStats();
  void clear();

  float score(size_t num_rounds) const;
  uint8_t rank(size_t num_rounds) const;
  const char* rank_name(size_t num_rounds) const;

  static uint8_t rank_for_score(float score);
  static const char* name_for_rank(uint8_t rank);
} __attribute__((packed));

std::vector<uint16_t> get_card_refs_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    const parray<CardShortStatus, 0x10>& short_statuses);

} // namespace Episode3
