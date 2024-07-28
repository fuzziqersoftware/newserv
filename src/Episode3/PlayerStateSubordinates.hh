#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

class Server;
class Card;

struct Condition {
  /* 00 */ ConditionType type;
  /* 01 */ uint8_t remaining_turns;
  /* 02 */ int8_t a_arg_value;
  /* 03 */ uint8_t dice_roll_value;
  /* 04 */ uint8_t flags;
  /* 05 */ uint8_t card_definition_effect_index;
  /* 06 */ le_uint16_t card_ref;
  /* 08 */ le_int16_t value;
  /* 0A */ le_uint16_t condition_giver_card_ref;
  /* 0C */ uint8_t random_percent;
  /* 0D */ int8_t value8;
  /* 0E */ uint8_t order;
  /* 0F */ uint8_t unknown_a8;
  /* 10 */

  Condition();
  bool operator==(const Condition& other) const;
  bool operator!=(const Condition& other) const;

  void clear();
  void clear_FF();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(Condition, 0x10);

struct EffectResult {
  /* 00 */ le_uint16_t attacker_card_ref;
  /* 02 */ le_uint16_t target_card_ref;
  /* 04 */ int8_t value;
  /* 05 */ int8_t current_hp;
  /* 06 */ int8_t ap;
  /* 07 */ int8_t tp;
  /* 08 */ uint8_t flags;
  /* 09 */ int8_t operation; // May be a negative condition number
  /* 0A */ uint8_t condition_index;
  /* 0B */ uint8_t dice_roll_value;
  /* 0C */

  EffectResult();
  bool operator==(const EffectResult& other) const;
  bool operator!=(const EffectResult& other) const;

  void clear();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(EffectResult, 0x0C);

struct CardShortStatus {
  /* 00 */ le_uint16_t card_ref;
  /* 02 */ le_uint16_t current_hp;
  /* 04 */ le_uint32_t card_flags;
  /* 08 */ Location loc;
  /* 0C */ le_uint16_t unused1;
  /* 0E */ int8_t max_hp;
  /* 0F */ uint8_t unused2;
  /* 10 */

  CardShortStatus();
  bool operator==(const CardShortStatus& other) const;
  bool operator!=(const CardShortStatus& other) const;

  void clear();
  void clear_FF();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(CardShortStatus, 0x10);

struct ActionState {
  /* 00 */ le_uint16_t client_id;
  /* 02 */ uint8_t unused;
  /* 03 */ Direction facing_direction;
  /* 04 */ le_uint16_t attacker_card_ref;
  /* 06 */ le_uint16_t defense_card_ref;
  /* 08 */ parray<le_uint16_t, 4 * 9> target_card_refs;
  /* 50 */ parray<le_uint16_t, 8> action_card_refs;
  /* 60 */ le_uint16_t unused2;
  /* 62 */ le_uint16_t original_attacker_card_ref;
  /* 64 */

  ActionState();
  bool operator==(const ActionState& other) const;
  bool operator!=(const ActionState& other) const;

  void clear();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(ActionState, 0x64);

struct ActionChain {
  // Note: Episode 3 Trial Edition has a different format for this structure.
  // See ActionChainWithCondsTrial for details.
  /* 00 */ int8_t effective_ap;
  /* 01 */ int8_t effective_tp;
  /* 02 */ int8_t ap_effect_bonus;
  /* 03 */ int8_t damage;
  /* 04 */ le_uint16_t acting_card_ref;
  /* 06 */ le_uint16_t unknown_card_ref_a3;
  /* 08 */ parray<le_uint16_t, 8> attack_action_card_refs;
  /* 18 */ uint8_t attack_action_card_ref_count;
  /* 19 */ AttackMedium attack_medium;
  /* 1A */ uint8_t target_card_ref_count;
  /* 1B */ ActionSubphase action_subphase;
  /* 1C */ uint8_t strike_count;
  /* 1D */ int8_t damage_multiplier;
  /* 1E */ uint8_t attack_number;
  /* 1F */ int8_t tp_effect_bonus;
  /* 20 */ int8_t physical_attack_bonus_nte;
  /* 21 */ int8_t tech_attack_bonus_nte;
  /* 22 */ int8_t card_ap;
  /* 23 */ int8_t card_tp;
  /* 24 */ le_uint32_t flags;
  /* 28 */ parray<le_uint16_t, 4 * 9> target_card_refs;
  /* 70 */

  ActionChain();
  bool operator==(const ActionChain& other) const;
  bool operator!=(const ActionChain& other) const;

  void clear();
  void clear_FF();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(ActionChain, 0x70);

struct ActionChainWithConds {
  /* 0000 */ ActionChain chain;
  /* 0070 */ parray<Condition, 9> conditions;
  /* 0100 */

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
  bool can_apply_attack() const;

  uint8_t get_adjusted_move_ability_nte(uint8_t ability) const;

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(ActionChainWithConds, 0x100);

struct ActionChainWithCondsTrial {
  /* 0000 */ int8_t effective_ap;
  /* 0001 */ int8_t effective_tp;
  /* 0002 */ int8_t ap_effect_bonus;
  /* 0003 */ int8_t damage;
  /* 0004 */ le_uint16_t acting_card_ref;
  /* 0006 */ le_uint16_t unknown_card_ref_a3;
  /* 0008 */ parray<le_uint16_t, 8> attack_action_card_refs;
  /* 0018 */ uint8_t attack_action_card_ref_count;
  /* 0019 */ AttackMedium attack_medium;
  /* 001A */ uint8_t target_card_ref_count;
  /* 001B */ ActionSubphase action_subphase;
  /* 001C */ uint8_t strike_count;
  /* 001D */ int8_t damage_multiplier;
  /* 001E */ uint8_t attack_number;
  /* 001F */ int8_t tp_effect_bonus;
  /* 0020 */ int8_t physical_attack_bonus_nte;
  /* 0021 */ int8_t tech_attack_bonus_nte;
  /* 0022 */ int8_t card_ap;
  /* 0023 */ int8_t card_tp;
  /* 0024 */ le_uint32_t flags;
  // The only difference between this structure and ActionChainWithConds is that
  // these two fields are in the opposite order.
  /* 0028 */ parray<Condition, 9> conditions;
  /* 00B8 */ parray<le_uint16_t, 4 * 9> target_card_refs;
  /* 0100 */

  ActionChainWithCondsTrial() = default;
  ActionChainWithCondsTrial(const ActionChainWithConds& src);
  operator ActionChainWithConds() const;
} __packed_ws__(ActionChainWithCondsTrial, 0x100);

struct ActionMetadata {
  /* 00 */ le_uint16_t card_ref;
  /* 02 */ uint8_t target_card_ref_count;
  /* 03 */ uint8_t defense_card_ref_count;
  /* 04 */ ActionSubphase action_subphase;
  /* 05 */ int8_t defense_power;
  /* 06 */ int8_t defense_bonus;
  /* 07 */ int8_t attack_bonus;
  /* 08 */ le_uint32_t flags;
  /* 0C */ parray<le_uint16_t, 4 * 9> target_card_refs;
  /* 54 */ parray<le_uint16_t, 8> defense_card_refs;
  /* 64 */ parray<le_uint16_t, 8> original_attacker_card_refs;
  /* 74 */

  ActionMetadata();
  bool operator==(const ActionMetadata& other) const;
  bool operator!=(const ActionMetadata& other) const;

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

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(ActionMetadata, 0x74);

struct HandAndEquipState {
  /* 00 */ parray<uint8_t, 2> dice_results;
  /* 02 */ uint8_t atk_points;
  /* 03 */ uint8_t def_points;
  /* 04 */ uint8_t atk_points2; // TODO: rename this to something more appropriate
  /* 05 */ uint8_t unknown_a1;
  /* 06 */ uint8_t total_set_cards_cost;
  /* 07 */ uint8_t is_cpu_player;
  /* 08 */ le_uint32_t assist_flags;
  /* 0C */ parray<le_uint16_t, 6> hand_card_refs;
  /* 18 */ le_uint16_t assist_card_ref;
  /* 1A */ parray<le_uint16_t, 8> set_card_refs;
  /* 2A */ le_uint16_t sc_card_ref;
  /* 2C */ parray<le_uint16_t, 6> hand_card_refs2;
  /* 38 */ parray<le_uint16_t, 8> set_card_refs2;
  /* 48 */ le_uint16_t assist_card_ref2;
  /* 4A */ le_uint16_t assist_card_set_number;
  /* 4C */ le_uint16_t assist_card_id;
  /* 4E */ uint8_t assist_remaining_turns;
  /* 4F */ uint8_t assist_delay_turns;
  /* 50 */ uint8_t atk_bonuses;
  /* 51 */ uint8_t def_bonuses;
  /* 52 */ parray<uint8_t, 2> unused2;
  /* 54 */

  HandAndEquipState();
  bool operator==(const HandAndEquipState& other) const;
  bool operator!=(const HandAndEquipState& other) const;

  void clear();
  void clear_FF();

  std::string str(std::shared_ptr<const Server> s) const;
} __packed_ws__(HandAndEquipState, 0x54);

struct PlayerBattleStats {
  /* 00 */ le_uint16_t damage_given;
  /* 02 */ le_uint16_t damage_taken;
  /* 04 */ le_uint16_t num_opponent_cards_destroyed;
  /* 06 */ le_uint16_t num_owned_cards_destroyed;
  /* 08 */ le_uint16_t total_move_distance;
  /* 0A */ le_uint16_t num_cards_set;
  /* 0C */ le_uint16_t num_item_or_creature_cards_set;
  /* 0E */ le_uint16_t num_attack_actions_set;
  /* 10 */ le_uint16_t num_tech_cards_set;
  /* 12 */ le_uint16_t num_assist_cards_set;
  /* 14 */ le_uint16_t defense_actions_set_on_self;
  /* 16 */ le_uint16_t defense_actions_set_on_ally;
  /* 18 */ le_uint16_t num_cards_drawn;
  /* 1A */ le_uint16_t max_attack_damage;
  /* 1C */ le_uint16_t max_attack_combo_size;
  /* 1E */ le_uint16_t num_attacks_given;
  /* 20 */ le_uint16_t num_attacks_taken;
  /* 22 */ le_uint16_t sc_damage_taken;
  /* 24 */ le_uint16_t action_card_negated_damage;
  /* 26 */ le_uint16_t unused;
  /* 28 */

  PlayerBattleStats();
  void clear();

  float score(size_t num_rounds) const;
  uint8_t rank(size_t num_rounds) const;
  const char* rank_name(size_t num_rounds) const;

  static uint8_t rank_for_score(float score);
  static const char* name_for_rank(uint8_t rank);
} __packed_ws__(PlayerBattleStats, 0x28);

struct PlayerBattleStatsTrial {
  /* 00 */ le_uint32_t damage_given = 0;
  /* 04 */ le_uint32_t damage_taken = 0;
  /* 08 */ le_uint32_t num_opponent_cards_destroyed = 0;
  /* 0C */ le_uint32_t num_owned_cards_destroyed = 0;
  /* 10 */ le_uint32_t total_move_distance = 0;
  /* 14 */

  PlayerBattleStatsTrial() = default;
  PlayerBattleStatsTrial(const PlayerBattleStats& data);
  operator PlayerBattleStats() const;
} __packed_ws__(PlayerBattleStatsTrial, 0x14);

std::vector<uint16_t> get_card_refs_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    const parray<CardShortStatus, 0x10>& short_statuses,
    phosg::PrefixedLogger* log = nullptr);

} // namespace Episode3
