#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "Card.hh"
#include "DataIndexes.hh"
#include "DeckState.hh"
#include "PlayerStateSubordinates.hh"

namespace Episode3 {

class Server;

enum AssistFlag : uint32_t {
  // Note: This enum is a uint32_t even though only 16 bits are used because
  // the corresponding field in the protocol is a 32-bit field. There may also
  // be bits used only by the client which are not documented here.

  // clang-format off
  NONE                                 = 0x0000,
  READY_TO_END_PHASE                   = 0x0001,
  DICE_WERE_EXCHANGED                  = 0x0002,
  HAS_WON_BATTLE                       = 0x0004,
  READY_TO_END_STARTER_ROLL_PHASE      = 0x0008,
  FIXED_RANGE                          = 0x0010,
  SUMMONING_IS_FREE                    = 0x0020,
  LIMIT_MOVE_TO_1                      = 0x0040,
  IS_SKIPPING_TURN                     = 0x0080,
  IMMORTAL                             = 0x0100,
  SAME_CARD_BANNED                     = 0x0200,
  CANNOT_SET_FIELD_CHARACTERS          = 0x0400,
  WINNER_DECIDED_BY_DEFEAT             = 0x0800,
  WINNER_DECIDED_BY_RANDOM             = 0x1000,
  READY_TO_END_ACTION_PHASE            = 0x2000,
  BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT = 0x4000,
  ELIGIBLE_FOR_DICE_BOOST              = 0x8000,
  // clang-format on
};

class PlayerState : public std::enable_shared_from_this<PlayerState> {
public:
  PlayerState(uint8_t client_id, std::shared_ptr<Server> server);
  void init();
  std::shared_ptr<Server> server();
  std::shared_ptr<const Server> server() const;

  bool is_alive() const;

  bool draw_cards_allowed() const;
  void apply_assist_card_effect_on_set(std::shared_ptr<PlayerState> setter_ps);
  void apply_dice_effects();
  uint16_t card_ref_for_hand_index(size_t hand_index) const;
  int16_t compute_attack_or_defense_atk_costs(const ActionState& pa) const;
  void compute_total_set_cards_cost();
  size_t count_set_cards_for_env_stats_nte() const;
  size_t count_set_cards() const;
  size_t count_set_refs() const;
  void discard_all_assist_cards_from_hand();
  void discard_all_attack_action_cards_from_hand();
  void discard_all_item_and_creature_cards_from_hand();
  void discard_and_redraw_hand();
  bool discard_card_or_add_to_draw_pile(uint16_t card_ref, bool add_to_draw_pile);
  void discard_random_hand_card();
  bool discard_ref_from_hand(uint16_t card_ref);
  void discard_set_assist_card();
  bool do_mulligan();
  void draw_hand(ssize_t override_count = 0);
  void draw_initial_hand();
  int32_t error_code_for_client_setting_card(
      uint16_t card_ref,
      uint8_t card_index,
      const Location* loc,
      uint8_t assist_target_client_id) const;
  std::vector<uint16_t> get_all_cards_within_range(
      const parray<uint8_t, 9 * 9>& range,
      const Location& loc,
      uint8_t target_team_id) const;
  uint8_t get_atk_points() const;
  void get_short_status_for_card_index_in_hand(size_t hand_index, CardShortStatus* stat) const;
  std::shared_ptr<DeckState> get_deck();
  uint8_t get_def_points() const;
  uint8_t get_dice_result(size_t which) const;
  size_t get_hand_size() const;
  uint16_t get_sc_card_id() const;
  std::shared_ptr<Card> get_sc_card();
  std::shared_ptr<const Card> get_sc_card() const;
  uint16_t get_sc_card_ref() const;
  CardType get_sc_card_type() const;
  std::shared_ptr<Card> get_set_card(size_t set_index);
  std::shared_ptr<const Card> get_set_card(size_t set_index) const;
  uint16_t get_set_ref(size_t set_index) const;
  uint8_t get_team_id() const;
  ssize_t hand_index_for_card_ref(uint16_t card_ref) const;
  size_t set_index_for_card_ref(uint16_t card_ref) const;
  bool is_mulligan_allowed() const;
  bool is_team_turn() const;
  void log_discard(uint16_t card_ref, uint16_t reason);
  uint16_t pop_from_discard_log(uint16_t reason);
  bool move_card_to_location_by_card_index(size_t card_index, const Location& new_loc);
  void move_null_hand_refs_to_end();
  void on_cards_destroyed();
  void replace_all_set_assists_with_random_assists();
  bool replace_assist_card_by_id(uint16_t card_id);
  bool return_set_card_to_hand2(uint16_t card_ref);
  bool return_set_card_to_hand1(uint16_t card_ref);
  uint8_t roll_dice(size_t num_dice);
  uint8_t roll_dice_with_effects(size_t num_dice);
  void send_set_card_updates(bool always_send = false);
  void set_assist_flags_from_assist_effects();
  bool set_card_from_hand(
      uint16_t card_ref,
      uint8_t card_index,
      const Location* loc,
      uint8_t assist_target_client_id,
      bool skip_error_checks_and_atk_sub);
  void set_initial_location();
  void set_map_occupied_bit_for_card_on_warp_tile(std::shared_ptr<const Card> card);
  void set_map_occupied_bits_for_sc_and_creatures();
  void subtract_def_points(uint8_t cost);
  bool subtract_or_check_atk_or_def_points_for_action(const ActionState& pa, bool deduct_points);
  void subtract_atk_points(uint8_t cost);
  G_UpdateHand_Ep3_6xB4x02 prepare_6xB4x02() const;
  void update_hand_and_equip_state_and_send_6xB4x02_if_needed(bool always_send = false);
  void set_random_assist_card_from_hand_for_free();
  G_UpdateShortStatuses_Ep3_6xB4x04 prepare_6xB4x04() const;
  void send_6xB4x04_if_needed(bool always_send = false);
  std::vector<uint16_t> get_card_refs_within_range_from_all_players(
      const parray<uint8_t, 9 * 9>& range,
      const Location& loc,
      CardType type) const;
  void draw_phase_before();
  void action_phase_before();
  void move_phase_before();
  void handle_before_turn_assist_effects();
  int16_t get_assist_turns_remaining();
  bool set_action_cards_for_action_state(const ActionState& pa);
  void dice_phase_before();
  void handle_homesick_assist_effect_from_bomb(std::shared_ptr<Card> card);
  void apply_main_die_assist_effects(uint8_t* die_value) const;
  void roll_main_dice_or_apply_after_effects();
  void unknown_8023C110();
  void compute_team_dice_bonus_after_draw_phase();
  void send_6xB4x0A_for_set_card(size_t set_index);

private:
  std::weak_ptr<Server> w_server;

public:
  std::shared_ptr<Card> sc_card;
  bcarray<std::shared_ptr<Card>, 8> set_cards;
  uint8_t client_id;
  uint16_t num_mulligans_allowed;
  CardType sc_card_type;
  uint8_t team_id;
  uint8_t atk_points;
  uint8_t def_points;
  uint8_t atk_points2;
  uint8_t atk_points2_max;
  uint8_t atk_bonuses;
  uint8_t def_bonuses;
  parray<uint8_t, 2> dice_results;
  uint8_t unknown_a4;
  uint8_t dice_max;
  uint8_t total_set_cards_cost;
  uint16_t sc_card_id;
  uint16_t sc_card_ref;

  // This array is unfortunately heterogeneous; specifically:
  // [0] through [5] are hand refs
  // [6] is the current assist card ref (which may belong to another player)
  // [7] is the previous assist card ref
  // [8] through [15] are set refs
  parray<uint16_t, 0x10> card_refs;

  std::shared_ptr<DeckState> deck_state;
  parray<uint16_t, 0x10> discard_log_card_refs;
  parray<uint16_t, 0x10> discard_log_reasons;
  uint8_t assist_remaining_turns;
  uint16_t assist_card_set_number;
  uint16_t set_assist_card_id;
  bool god_whim_can_use_hidden_cards;
  ActionChainWithConds unknown_a12;
  ActionMetadata unknown_a13;
  uint32_t unknown_a14;
  uint32_t assist_flags;
  uint8_t assist_delay_turns;
  Direction start_facing_direction;
  std::shared_ptr<HandAndEquipState> hand_and_equip;

  // Like card_refs above, these arrays are also heterogeneous, but the indices
  // are not the same as for card_refs! THe indices here are:
  // [0] is the SC card status
  // [1] through [6] are hand cards
  // [7] through [14] are set cards
  // [15] is the assist card
  std::shared_ptr<parray<CardShortStatus, 0x10>> card_short_statuses;
  parray<CardShortStatus, 0x10> prev_card_short_statuses;

  // In these arrays, [0] is the SC card and the rest are the set cards.
  std::shared_ptr<parray<ActionChainWithConds, 9>> set_card_action_chains;
  std::shared_ptr<parray<ActionMetadata, 9>> set_card_action_metadatas;
  parray<ActionChainWithConds, 9> prev_set_card_action_chains;
  parray<ActionMetadata, 9> prev_set_card_action_metadatas;

  uint32_t num_destroyed_fcs;
  uint8_t unknown_a16;
  uint8_t unknown_a17;
  PlayerBattleStats stats;
};

} // namespace Episode3
