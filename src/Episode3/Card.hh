#pragma once

#include <stdint.h>

#include <memory>

#include "../CommandFormats.hh"
#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

class Server;
class PlayerState;

class Card : public std::enable_shared_from_this<Card> {
public:
  Card(uint16_t card_id, uint16_t card_ref, uint16_t client_id, std::shared_ptr<Server> server);
  void init();
  std::shared_ptr<Server> server();
  std::shared_ptr<const Server> server() const;
  std::shared_ptr<PlayerState> player_state();
  std::shared_ptr<const PlayerState> player_state() const;

  ssize_t apply_abnormal_condition(
      const CardDefinition::Effect& eff,
      uint8_t def_effect_index,
      uint16_t target_card_ref,
      uint16_t sc_card_ref,
      int16_t value,
      int8_t dice_roll_value,
      int8_t random_percent);
  void apply_ap_and_tp_adjust_assists_to_attack(
      std::shared_ptr<const Card> attacker_card,
      int16_t* inout_attacker_ap,
      int16_t* in_defense_power,
      int16_t* inout_attacker_tp) const;
  bool card_type_is_sc_or_creature() const;
  bool check_card_flag(uint32_t flags) const;
  void commit_attack(
      int16_t damage,
      std::shared_ptr<Card> attacker_card,
      G_ApplyConditionEffect_Ep3_6xB4x06* cmd,
      size_t strike_number,
      int16_t* out_effective_damage);
  int16_t compute_defense_power_for_attacker_card(std::shared_ptr<const Card> attacker_card);
  void destroy_set_card(std::shared_ptr<Card> attacker_card);
  int32_t error_code_for_move_to_location(const Location& loc) const;
  void execute_attack(std::shared_ptr<Card> attacker_card);
  bool get_condition_value(
      ConditionType cond_type,
      uint16_t card_ref = 0xFFFF,
      uint8_t def_effect_index = 0xFF,
      uint16_t value = 0xFFFF,
      uint16_t* out_value = nullptr) const;
  Condition* find_condition(ConditionType cond_type);
  const Condition* find_condition(ConditionType cond_type) const;
  std::shared_ptr<const CardIndex::CardEntry> get_definition() const;
  uint16_t get_card_ref() const;
  uint16_t get_card_id() const;
  uint8_t get_client_id() const;
  uint8_t get_current_hp() const;
  uint8_t get_max_hp() const;
  CardShortStatus get_short_status();
  uint8_t get_team_id() const;
  int32_t move_to_location(const Location& loc);
  void propagate_shared_hp_if_needed();
  void send_6xB4x4E_4C_4D_if_needed(bool always_send = false);
  void send_6xB4x4E_if_needed(bool always_send = false);
  void set_current_and_max_hp(int16_t hp);
  void set_current_hp(uint32_t new_hp, bool propagate_shared_hp = true, bool enforce_max_hp = true);
  void update_stats_on_destruction();
  void clear_action_chain_and_metadata_and_most_flags();
  void compute_action_chain_results(bool apply_action_conditions, bool ignore_this_card_ap_tp);
  void unknown_802380C0();
  void unknown_80237F98(bool require_condition_20_or_21);
  void unknown_80237F88();
  void draw_phase_before();
  void action_phase_before();
  void move_phase_before();
  void unknown_80236374(std::shared_ptr<Card> other_card, const ActionState* as);
  void unknown_802379BC(uint16_t card_ref);
  void unknown_802379DC(const ActionState& pa);
  void unknown_80237A90(const ActionState& pa, uint16_t action_card_ref);
  void dice_phase_before();
  bool is_guard_item() const;
  bool unknown_80236554(std::shared_ptr<Card> other_card, const ActionState* as);
  void execute_attack_on_all_valid_targets(std::shared_ptr<Card> attacker_card);
  void apply_attack_result();

private:
  std::weak_ptr<Server> w_server;
  std::weak_ptr<PlayerState> w_player_state;

public:
  int16_t max_hp;
  int16_t current_hp;
  std::shared_ptr<const CardIndex::CardEntry> def_entry;
  uint8_t client_id;
  uint16_t card_id;
  uint16_t card_ref;
  uint16_t sc_card_ref;
  std::shared_ptr<const CardIndex::CardEntry> sc_def_entry;
  CardType sc_card_type;
  uint8_t team_id;
  uint32_t card_flags;
  Location loc;
  Direction facing_direction;
  ActionChainWithConds action_chain;
  ActionMetadata action_metadata;
  int16_t ap;
  int16_t tp;
  uint32_t num_ally_fcs_destroyed_at_set_time;
  uint32_t num_cards_destroyed_by_team_at_set_time;
  uint32_t unknown_a9;
  int16_t last_attack_preliminary_damage;
  int16_t last_attack_final_damage;
  uint32_t num_destroyed_ally_fcs;
  std::weak_ptr<Card> w_destroyer_sc_card;
  int16_t current_defense_power;
};

} // namespace Episode3
