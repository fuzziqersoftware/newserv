#include "PlayerState.hh"

#include "Server.hh"

using namespace std;

namespace Episode3 {



Condition::Condition() {
  this->clear();
}

bool Condition::operator==(const Condition& other) const {
  return (this->type == other.type) &&
         (this->remaining_turns == other.remaining_turns) &&
         (this->a_arg_value == other.a_arg_value) &&
         (this->dice_roll_value == other.dice_roll_value) &&
         (this->flags == other.flags) &&
         (this->card_definition_effect_index == other.card_definition_effect_index) &&
         (this->card_ref == other.card_ref) &&
         (this->value == other.value) &&
         (this->condition_giver_card_ref == other.condition_giver_card_ref) &&
         (this->random_percent == other.random_percent) &&
         (this->value8 == other.value8) &&
         (this->order == other.order) &&
         (this->unknown_a8 == other.unknown_a8);
}
bool Condition::operator!=(const Condition& other) const {
  return !this->operator==(other);
}

void Condition::clear() {
  this->type = ConditionType::NONE;
  this->remaining_turns = 0;
  this->a_arg_value = 0;
  this->dice_roll_value = 0;
  this->flags = 0;
  this->card_definition_effect_index = 0;
  this->card_ref = 0xFFFF;
  this->value = 0;
  this->condition_giver_card_ref = 0xFFFF;
  this->random_percent = 0;
  this->value8 = 0;
  this->order = 0;
  this->unknown_a8 = 0;
}

void Condition::clear_FF() {
  this->type = ConditionType::INVALID_FF;
  this->remaining_turns = 0xFF;
  this->a_arg_value = -1;
  this->dice_roll_value = 0xFF;
  this->flags = 0xFF;
  this->card_definition_effect_index = 0xFF;
  this->card_ref = 0xFFFF;
  this->value = -1;
  this->condition_giver_card_ref = 0xFFFF;
  this->random_percent = 0xFF;
  this->value8 = -1;
  this->order = 0xFF;
  this->unknown_a8 = 0xFF;
}



EffectResult::EffectResult() {
  this->clear();
}

void EffectResult::clear() {
  this->attacker_card_ref = 0xFFFF;
  this->target_card_ref = 0xFFFF;
  this->value = 0;
  this->current_hp = 0;
  this->ap = 0;
  this->tp = 0;
  this->flags = 0;
  this->operation = 0;
  this->condition_index = 0;
  this->dice_roll_value = 0;
}



CardShortStatus::CardShortStatus() {
  this->clear();
}

bool CardShortStatus::operator==(const CardShortStatus& other) const {
  return (this->card_ref == other.card_ref) &&
         (this->current_hp == other.current_hp) &&
         (this->card_flags == other.card_flags) &&
         (this->loc == other.loc) &&
         (this->unused1 == other.unused1) &&
         (this->max_hp == other.max_hp) &&
         (this->unused2 == other.unused2);
}
bool CardShortStatus::operator!=(const CardShortStatus& other) const {
  return !this->operator==(other);
}

void CardShortStatus::clear() {
  this->card_ref = 0xFFFF;
  this->current_hp = 0;
  this->card_flags = 0;
  this->loc.clear();
  this->unused1 = 0xFFFF;
  this->max_hp = 0;
  this->unused2 = 0;
}

void CardShortStatus::clear_FF() {
  this->card_ref = 0xFFFF;
  this->current_hp = 0xFFFF;
  this->card_flags = 0xFFFFFFFF;
  this->loc.clear_FF();
  this->unused1 = 0xFFFF;
  this->max_hp = -1;
  this->unused2 = 0xFF;
}



ActionState::ActionState() {
  this->clear();
}

void ActionState::clear() {
  this->client_id = 0xFFFF;
  this->unused = 0;
  this->facing_direction = Direction::RIGHT;
  this->attacker_card_ref = 0xFFFF;
  this->defense_card_ref = 0xFFFF;
  this->original_attacker_card_ref = 0xFFFF;
  this->target_card_refs.clear(0xFFFF);
  this->action_card_refs.clear(0xFFFF);
}



ActionChain::ActionChain() {
  this->clear();
}

bool ActionChain::operator==(const ActionChain& other) const {
  return (this->effective_ap == other.effective_ap) &&
         (this->effective_tp == other.effective_tp) &&
         (this->ap_effect_bonus == other.ap_effect_bonus) &&
         (this->damage == other.damage) &&
         (this->acting_card_ref == other.acting_card_ref) &&
         (this->unknown_card_ref_a3 == other.unknown_card_ref_a3) &&
         (this->attack_action_card_refs == other.attack_action_card_refs) &&
         (this->attack_action_card_ref_count == other.attack_action_card_ref_count) &&
         (this->attack_medium == other.attack_medium) &&
         (this->target_card_ref_count == other.target_card_ref_count) &&
         (this->action_subphase == other.action_subphase) &&
         (this->strike_count == other.strike_count) &&
         (this->damage_multiplier == other.damage_multiplier) &&
         (this->attack_number == other.attack_number) &&
         (this->tp_effect_bonus == other.tp_effect_bonus) &&
         (this->unused1 == other.unused1) &&
         (this->unused2 == other.unused2) &&
         (this->card_ap == other.card_ap) &&
         (this->card_tp == other.card_tp) &&
         (this->flags == other.flags) &&
         (this->target_card_refs == other.target_card_refs);
}
bool ActionChain::operator!=(const ActionChain& other) const {
  return !this->operator==(other);
}

void ActionChain::clear() {
  this->effective_ap = 0;
  this->effective_tp = 0;
  this->ap_effect_bonus = 0;
  this->damage = 0;
  this->acting_card_ref = 0xFFFF;
  this->unknown_card_ref_a3 = 0xFFFF;
  this->attack_action_card_ref_count = 0;
  this->attack_medium = AttackMedium::UNKNOWN;
  this->target_card_ref_count = 0;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->strike_count = 1;
  this->damage_multiplier = 1;
  this->attack_number = 0xFF;
  this->tp_effect_bonus = 0;
  this->unused1 = 0;
  this->unused2 = 0;
  this->card_ap = 0;
  this->card_tp = 0;
  this->flags = 0;
  this->attack_action_card_refs.clear(0xFFFF);
  this->target_card_refs.clear(0xFFFF);
}

void ActionChain::clear_FF() {
  this->effective_ap = -1;
  this->effective_tp = -1;
  this->ap_effect_bonus = -1;
  this->damage = -1;
  this->acting_card_ref = 0xFFFF;
  this->unknown_card_ref_a3 = 0xFFFF;
  this->attack_action_card_refs.clear(0xFFFF);
  this->attack_action_card_ref_count = 0xFF;
  this->attack_medium = AttackMedium::INVALID_FF;
  this->target_card_ref_count = 0xFF;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->strike_count = 0xFF;
  this->damage_multiplier = -1;
  this->attack_number = 0xFF;
  this->tp_effect_bonus = -1;
  this->unused1 = 0xFF;
  this->unused2 = 0xFF;
  this->card_ap = -1;
  this->card_tp = -1;
  this->flags = 0xFFFFFFFF;
  this->target_card_refs.clear(0xFFFF);
}



ActionChainWithConds::ActionChainWithConds() {
  this->clear();
}

bool ActionChainWithConds::operator==(const ActionChainWithConds& other) const {
  return (this->chain == other.chain && this->conditions == other.conditions);
}
bool ActionChainWithConds::operator!=(const ActionChainWithConds& other) const {
  return !this->operator==(other);
}

void ActionChainWithConds::clear() {
  this->chain.effective_ap = 0;
  this->chain.effective_tp = 0;
  this->chain.ap_effect_bonus = 0;
  this->chain.damage = 0;
  this->clear_inner();
}

void ActionChainWithConds::clear_FF() {
  this->chain.clear_FF();
  for (size_t z = 0; z < 9; z++) {
    this->conditions[z].clear_FF();
  }
}

void ActionChainWithConds::clear_inner() {
  this->chain.unknown_card_ref_a3 = 0xFFFF;
  this->chain.acting_card_ref = 0xFFFF;
  this->chain.attack_medium = AttackMedium::INVALID_FF;
  this->chain.flags = 0;
  this->chain.action_subphase = ActionSubphase::INVALID_FF;
  this->chain.attack_number = 0xFF;
  this->reset();
  this->clear_target_card_refs();
  this->chain.attack_action_card_ref_count = 0;
  this->chain.attack_action_card_refs.clear(0xFFFF);
}

void ActionChainWithConds::clear_target_card_refs() {
  this->chain.target_card_ref_count = 0;
  this->chain.target_card_refs.clear(0xFFFF);
}

void ActionChainWithConds::reset() {
  this->chain.effective_ap = 0;
  this->chain.effective_tp = 0;
  this->chain.ap_effect_bonus = 0;
  this->chain.tp_effect_bonus = 0;
  this->chain.unused1 = 0;
  this->chain.unused2 = 0;
  this->chain.damage = 0;
  this->chain.strike_count = 1;
  this->chain.damage_multiplier = 1;
}

bool ActionChainWithConds::check_flag(uint32_t flags) const {
  return (this->chain.flags & flags) != 0;
}

void ActionChainWithConds::clear_flags(uint32_t flags) {
  this->chain.flags &= ~flags;
}

void ActionChainWithConds::set_flags(uint32_t flags) {
  this->chain.flags |= flags;
}

void ActionChainWithConds::add_attack_action_card_ref(
    uint16_t card_ref, shared_ptr<Server> server) {
  if (card_ref != 0xFFFF) {
    this->chain.attack_action_card_refs[this->chain.attack_action_card_ref_count++] = card_ref;
  }
  this->set_flags(8);
  this->chain.action_subphase = server->get_current_action_subphase();
}

void ActionChainWithConds::add_target_card_ref(uint16_t card_ref) {
  if (card_ref != 0xFFFF &&
      this->chain.target_card_ref_count < this->chain.target_card_refs.size()) {
    this->chain.target_card_refs[this->chain.target_card_ref_count++] = card_ref;
  }
}

void ActionChainWithConds::compute_attack_medium(shared_ptr<Server> server) {
  this->chain.attack_medium = AttackMedium::PHYSICAL;
  for (size_t z = 0; z < this->chain.attack_action_card_ref_count; z++) {
    uint16_t card_ref = this->chain.attack_action_card_refs[z];
    if (card_ref == 0xFFFF) {
      break;
    }
    auto ce = server->definition_for_card_ref(card_ref);
    if (!ce) {
      continue;
    }
    if (card_class_is_tech_like(ce->def.card_class())) {
      this->chain.attack_medium = AttackMedium::TECH;
    }
  }
}

bool ActionChainWithConds::get_condition_value(
    ConditionType cond_type,
    uint16_t card_ref,
    uint8_t def_effect_index,
    uint16_t value,
    uint16_t* out_value) const {
  bool any_found = false;
  uint8_t max_order = 10;
  for (size_t z = 0; z < 9; z++) {
    auto& cond = this->conditions[z];
    if (((cond_type == ConditionType::ANY) || (cond.type == cond_type)) &&
        ((def_effect_index == 0xFF) || (cond.card_definition_effect_index == def_effect_index)) &&
        ((card_ref == 0xFFFF) || (cond.card_ref == card_ref)) &&
        ((value == 0xFFFF) || (cond.value == value))) {
      if (!any_found || (max_order < cond.order)) {
        if (!out_value) {
          return true;
        }
        *out_value = cond.value;
        max_order = cond.order;
      }
      any_found = true;
    }
  }
  return any_found;
}

void ActionChainWithConds::set_action_subphase_from_card(
    shared_ptr<const Card> card) {
  this->chain.action_subphase = card->server()->get_current_action_subphase();
}

bool ActionChainWithConds::unknown_8024DEC4() const {
  return this->check_flag(4) ? false : (this->chain.target_card_ref_count != 0);
}



ActionMetadata::ActionMetadata() {
  this->clear();
}

bool ActionMetadata::operator==(const ActionMetadata& other) const {
  return (this->card_ref == other.card_ref) &&
         (this->target_card_ref_count == other.target_card_ref_count) &&
         (this->defense_card_ref_count == other.defense_card_ref_count) &&
         (this->action_subphase == other.action_subphase) &&
         (this->defense_power == other.defense_power) &&
         (this->defense_bonus == other.defense_bonus) &&
         (this->attack_bonus == other.attack_bonus) &&
         (this->flags == other.flags) &&
         (this->target_card_refs == other.target_card_refs) &&
         (this->defense_card_refs == other.defense_card_refs) &&
         (this->original_attacker_card_refs == other.original_attacker_card_refs);
}
bool ActionMetadata::operator!=(const ActionMetadata& other) const {
  return !this->operator==(other);
}

void ActionMetadata::clear() {
  this->card_ref = 0xFFFF;
  this->target_card_ref_count = 0;
  this->defense_card_ref_count = 0;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->defense_power = 0;
  this->defense_bonus = 0;
  this->attack_bonus = 0;
  this->flags = 0;
  this->target_card_refs.clear(0xFFFF);
  this->defense_card_refs.clear(0xFFFF);
  this->original_attacker_card_refs.clear(0xFFFF);
}

void ActionMetadata::clear_FF() {
  this->card_ref = 0xFFFF;
  this->target_card_ref_count = 0xFF;
  this->defense_card_ref_count = 0xFF;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->defense_power = -1;
  this->defense_bonus = -1;
  this->attack_bonus = -1;
  this->flags = 0xFFFFFFFF;
  this->target_card_refs.clear(0xFFFF);
  this->defense_card_refs.clear(0xFFFF);
  this->original_attacker_card_refs.clear(0xFFFF);
}

bool ActionMetadata::check_flag(uint32_t mask) const {
  return (this->flags & mask) != 0;
}

void ActionMetadata::set_flags(uint32_t flags) {
  this->flags |= flags;
}

void ActionMetadata::clear_flags(uint32_t flags) {
  this->flags &= ~flags;
}

void ActionMetadata::clear_defense_and_attacker_card_refs() {
  this->defense_card_ref_count = 0;
  this->defense_card_refs.clear(0xFFFF);
  this->original_attacker_card_refs.clear(0xFFFF);
}

void ActionMetadata::clear_target_card_refs() {
  this->target_card_ref_count = 0;
  this->target_card_refs.clear(0xFFFF);
}

void ActionMetadata::add_target_card_ref(uint16_t card_ref) {
  if (card_ref != 0xFFFF &&
      this->target_card_ref_count < this->target_card_refs.size()) {
    this->target_card_refs[this->target_card_ref_count++] = card_ref;
  }
}

void ActionMetadata::add_defense_card_ref(
    uint16_t defense_card_ref,
    shared_ptr<Card> card,
    uint16_t original_attacker_card_ref) {
  if ((defense_card_ref != 0xFFFF) && (this->defense_card_ref_count < 8)) {
    this->defense_card_refs[this->defense_card_ref_count] = defense_card_ref;
    this->original_attacker_card_refs[this->defense_card_ref_count] = original_attacker_card_ref;
    this->defense_card_ref_count++;
    this->action_subphase = card->server()->get_current_action_subphase();
  }
}



HandAndEquipState::HandAndEquipState() {
  this->clear();
}

void HandAndEquipState::clear() {
  this->dice_results.clear(0);
  this->atk_points = 0;
  this->def_points = 0;
  this->atk_points2 = 0;
  this->unknown_a1 = 0;
  this->total_set_cards_cost = 0;
  this->is_cpu_player = 0;
  this->assist_flags = 0;
  this->hand_card_refs.clear(0xFFFF);
  this->assist_card_ref = 0xFFFF;
  this->set_card_refs.clear(0xFFFF);
  this->sc_card_ref = 0xFFFF;
  this->hand_card_refs2.clear(0xFFFF);
  this->set_card_refs2.clear(0xFFFF);
  this->assist_card_ref2 = 0xFFFF;
  this->assist_card_set_number = 0;
  this->assist_card_id = 0xFFFF;
  this->assist_remaining_turns = 0;
  this->assist_delay_turns = 0;
  this->atk_bonuses = 0;
  this->def_bonuses = 0;
  this->unused2.clear(0);
}

void HandAndEquipState::clear_FF() {
  this->dice_results.clear(0xFF);
  this->atk_points = 0xFF;
  this->def_points = 0xFF;
  this->atk_points2 = 0xFF;
  this->unknown_a1 = 0xFF;
  this->total_set_cards_cost = 0xFF;
  this->is_cpu_player = 0xFF;
  this->assist_flags = 0xFFFFFFFF;
  this->hand_card_refs.clear(0xFFFF);
  this->assist_card_ref = 0xFFFF;
  this->set_card_refs.clear(0xFFFF);
  this->sc_card_ref = 0xFFFF;
  this->hand_card_refs2.clear(0xFFFF);
  this->set_card_refs2.clear(0xFFFF);
  this->assist_card_ref2 = 0xFFFF;
  this->assist_card_set_number = 0xFFFF;
  this->assist_card_id = 0xFFFF;
  this->assist_remaining_turns = 0xFF;
  this->assist_delay_turns = 0xFF;
  this->atk_bonuses = 0xFF;
  this->def_bonuses = 0xFF;
  this->unused2.clear(0xFF);
}



PlayerStats::PlayerStats() {
  this->clear();
}

void PlayerStats::clear() {
  this->damage_given = 0;
  this->damage_taken = 0;
  this->num_opponent_cards_destroyed = 0;
  this->num_owned_cards_destroyed = 0;
  this->total_move_distance = 0;
  this->num_cards_set = 0;
  this->num_item_or_creature_cards_set = 0;
  this->num_attack_actions_set = 0;
  this->num_tech_cards_set = 0;
  this->num_assist_cards_set = 0;
  this->defense_actions_set_on_self = 0;
  this->defense_actions_set_on_ally = 0;
  this->num_cards_drawn = 0;
  this->max_attack_damage = 0;
  this->max_attack_combo_size = 0;
  this->num_attacks_given = 0;
  this->num_attacks_taken = 0;
  this->sc_damage_taken = 0;
  this->action_card_negated_damage = 0;
  this->unused = 0;
}

float PlayerStats::score(size_t num_rounds) const {
  // Note: This formula doesn't match the formula on PSO-World, which is:
  //     35
  //   + (Attack Damage - Damage Taken)
  //   + (Max Card Combo x 3)
  //   - (Story Character Damage x 1.8)
  //   - (Turns x 2.7)
  //   + (Action Card Negated Damage x 0.8)
  // I don't know where that formula came from, but this one came from the USA
  // Ep3 PsoV3.dol, so it's presumably correct. Is the PSO-World formula simply
  // incorrect, or is it from e.g. the Japanese version, which may have a
  // different rank calculation function?
  return 38.0f
       + 0.8f * this->action_card_negated_damage
       - 2.3f * num_rounds
       - 1.8f * this->sc_damage_taken
       + 3.0f * this->max_attack_combo_size
       + (this->damage_given - this->damage_taken);
}

uint8_t PlayerStats::rank(size_t num_rounds) const {
  return this->rank_for_score(this->score(num_rounds));
}

const char* PlayerStats::rank_name(size_t num_rounds) const {
  return this->name_for_rank(this->rank_for_score(this->score(num_rounds)));
}

constexpr size_t RANK_THRESHOLD_COUNT = 9;
static const float RANK_THRESHOLDS[RANK_THRESHOLD_COUNT] = {
    15.0f, 25.0f, 30.0f, 40.0f, 50.0f, 60.0f, 65.0f, 75.0f, 85.0f};
static const char* RANK_NAMES[RANK_THRESHOLD_COUNT + 1] = {
    "E",   "D",   "D+",  "C",   "C+",  "B",   "B+",  "A",   "A+", "S"};

uint8_t PlayerStats::rank_for_score(float score) {
  size_t rank = 0;
  while (rank < RANK_THRESHOLD_COUNT && RANK_THRESHOLDS[rank] <= score) {
    rank++;
  }
  return rank;
}

const char* PlayerStats::name_for_rank(uint8_t rank) {
  if (rank >= RANK_THRESHOLD_COUNT + 1) {
    throw invalid_argument("invalid rank");
  }
  return RANK_NAMES[rank];
}




bool is_card_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& anchor_loc,
    const CardShortStatus& ss) {
  if (ss.card_ref == 0xFFFF) {
    return false;
  }
  if (range[0] == 2) {
    return true;
  }

  if ((ss.loc.x < anchor_loc.x - 4) || (ss.loc.x > anchor_loc.x + 4)) {
    return false;
  }
  if ((ss.loc.y < anchor_loc.y - 4) || (ss.loc.y > anchor_loc.y + 4)) {
    return false;
  }
  return (range[(ss.loc.x - anchor_loc.x) + ((ss.loc.y - anchor_loc.y) + 4) * 9 + 4] != 0);
}

vector<uint16_t> get_card_refs_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    const parray<CardShortStatus, 0x10>& short_statuses) {
  vector<uint16_t> ret;
  if (is_card_within_range(range, loc, short_statuses[0])) {
    ret.emplace_back(short_statuses[0].card_ref);
  }
  for (size_t card_index = 7; card_index < 15; card_index++) {
    const auto& ss = short_statuses[card_index];
    if (is_card_within_range(range, loc, ss)) {
      ret.emplace_back(ss.card_ref);
    }
  }
  return ret;
}



} // namespace Episode3
