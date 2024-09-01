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

std::string Condition::str(shared_ptr<const Server> s) const {
  auto card_ref_str = s->debug_str_for_card_ref(this->card_ref);
  auto giver_ref_str = s->debug_str_for_card_ref(this->condition_giver_card_ref);
  return phosg::string_printf(
      "Condition[type=%s, turns=%hhu, a_arg=%hhd, dice=%hhu, flags=%02hhX, "
      "def_eff_index=%hhu, ref=%s, value=%hd, giver_ref=%s "
      "percent=%hhu value8=%hd order=%hu a8=%hu]",
      phosg::name_for_enum(this->type),
      this->remaining_turns,
      this->a_arg_value,
      this->dice_roll_value,
      this->flags,
      this->card_definition_effect_index,
      card_ref_str.c_str(),
      this->value.load(),
      giver_ref_str.c_str(),
      this->random_percent,
      this->value8,
      this->order,
      this->unknown_a8);
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

std::string EffectResult::str(shared_ptr<const Server> s) const {
  string attacker_ref_str = s->debug_str_for_card_ref(this->attacker_card_ref);
  string target_ref_str = s->debug_str_for_card_ref(this->target_card_ref);
  return phosg::string_printf(
      "EffectResult[att_ref=%s, target_ref=%s, value=%hhd, "
      "cur_hp=%hhd, ap=%hhd, tp=%hhd, flags=%02hhX, op=%hhd, "
      "cond_index=%hhu, dice=%hhu]",
      attacker_ref_str.c_str(),
      target_ref_str.c_str(),
      this->value,
      this->current_hp,
      this->ap,
      this->tp,
      this->flags,
      this->operation,
      this->condition_index,
      this->dice_roll_value);
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

std::string CardShortStatus::str(shared_ptr<const Server> s) const {
  string loc_s = this->loc.str();
  string ref_str = s->debug_str_for_card_ref(this->card_ref);
  return phosg::string_printf(
      "CardShortStatus[ref=%s, cur_hp=%hd, flags=%08" PRIX32 ", loc=%s, "
      "u1=%04hX, max_hp=%hhd, u2=%hhu]",
      ref_str.c_str(),
      this->current_hp.load(),
      this->card_flags.load(),
      loc_s.c_str(),
      this->unused1.load(),
      this->max_hp,
      this->unused2);
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
  this->unused2 = 0xFFFF;
}

std::string ActionState::str(shared_ptr<const Server> s) const {
  string attacker_ref_s = s->debug_str_for_card_ref(this->attacker_card_ref);
  string defense_ref_s = s->debug_str_for_card_ref(this->defense_card_ref);
  string original_attacker_ref_s = s->debug_str_for_card_ref(this->original_attacker_card_ref);
  string target_refs_s = s->debug_str_for_card_refs(this->target_card_refs);
  string action_refs_s = s->debug_str_for_card_refs(this->action_card_refs);
  return phosg::string_printf(
      "ActionState[client=%hX, u=%hhu, facing=%s, attacker_ref=%s, "
      "def_ref=%s, target_refs=%s, action_refs=%s, "
      "orig_attacker_ref=%s]",
      this->client_id.load(),
      this->unused,
      phosg::name_for_enum(this->facing_direction),
      attacker_ref_s.c_str(),
      defense_ref_s.c_str(),
      target_refs_s.c_str(),
      action_refs_s.c_str(),
      original_attacker_ref_s.c_str());
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
      (this->physical_attack_bonus_nte == other.physical_attack_bonus_nte) &&
      (this->tech_attack_bonus_nte == other.tech_attack_bonus_nte) &&
      (this->card_ap == other.card_ap) &&
      (this->card_tp == other.card_tp) &&
      (this->flags == other.flags) &&
      (this->target_card_refs == other.target_card_refs);
}
bool ActionChain::operator!=(const ActionChain& other) const {
  return !this->operator==(other);
}

std::string ActionChain::str(shared_ptr<const Server> s) const {
  string acting_card_ref_s = s->debug_str_for_card_ref(this->acting_card_ref);
  string unknown_card_ref_a3_s = s->debug_str_for_card_ref(this->unknown_card_ref_a3);
  string attack_action_card_refs_s = s->debug_str_for_card_refs(this->attack_action_card_refs);
  string target_card_refs_s = s->debug_str_for_card_refs(this->target_card_refs);
  return phosg::string_printf(
      "ActionChain[eff_ap=%hhd, eff_tp=%hhd, ap_bonus=%hhd, damage=%hhd, "
      "acting_ref=%s, unknown_ref_a3=%s, attack_action_refs=%s, "
      "attack_action_ref_count=%hhu, medium=%s, target_ref_count=%hhu, "
      "subphase=%s, strikes=%hhu, damage_mult=%hhd, attack_num=%hhu, "
      "tp_bonus=%hhd, phys_bonus_nte=%hhu, tech_bonus_nte=%hhu, card_ap=%hhd, "
      "card_tp=%hhd, flags=%08" PRIX32 ", target_refs=%s]",
      this->effective_ap,
      this->effective_tp,
      this->ap_effect_bonus,
      this->damage,
      acting_card_ref_s.c_str(),
      unknown_card_ref_a3_s.c_str(),
      attack_action_card_refs_s.c_str(),
      this->attack_action_card_ref_count,
      phosg::name_for_enum(this->attack_medium),
      this->target_card_ref_count,
      phosg::name_for_enum(this->action_subphase),
      this->strike_count,
      this->damage_multiplier,
      this->attack_number,
      this->tp_effect_bonus,
      this->physical_attack_bonus_nte,
      this->tech_attack_bonus_nte,
      this->card_ap,
      this->card_tp,
      this->flags.load(),
      target_card_refs_s.c_str());
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
  this->physical_attack_bonus_nte = 0;
  this->tech_attack_bonus_nte = 0;
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
  this->physical_attack_bonus_nte = 0xFF;
  this->tech_attack_bonus_nte = 0xFF;
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

std::string ActionChainWithConds::str(shared_ptr<const Server> s) const {
  string ret = "ActionChainWithConds[chain=";
  ret += this->chain.str(s);
  ret += ", conds=[";
  for (size_t z = 0; z < this->conditions.size(); z++) {
    if (this->conditions[z].type != ConditionType::NONE) {
      if (ret.back() != '[') {
        ret += ", ";
      }
      ret += phosg::string_printf("%zu:", z);
      ret += this->conditions[z].str(s);
    }
  }
  ret += "]]";
  return ret;
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
  this->chain.physical_attack_bonus_nte = 0;
  this->chain.tech_attack_bonus_nte = 0;
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
    if (card_class_is_tech_like(ce->def.card_class(), server->options.is_nte())) {
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

bool ActionChainWithConds::can_apply_attack() const {
  return this->check_flag(4) ? false : (this->chain.target_card_ref_count != 0);
}

uint8_t ActionChainWithConds::get_adjusted_move_ability_nte(uint8_t ability) const {
  for (size_t z = 0; z < this->conditions.size(); z++) {
    const auto& cond = this->conditions[z];
    switch (cond.type) {
      case ConditionType::IMMOBILE:
      case ConditionType::FREEZE:
        ability = 0;
        break;
      case ConditionType::SET_MV_COST_TO_0:
        ability = 99;
        break;
      case ConditionType::ADD_1_TO_MV_COST:
        ability--;
        break;
      case ConditionType::SCALE_MV_COST:
        if (cond.value == 0) {
          ability = 99;
        } else {
          ability /= cond.value;
        }
        break;
      default:
        break;
    }
  }
  return ability;
}

ActionChainWithCondsTrial::ActionChainWithCondsTrial(const ActionChainWithConds& src)
    : effective_ap(src.chain.effective_ap),
      effective_tp(src.chain.effective_tp),
      ap_effect_bonus(src.chain.ap_effect_bonus),
      damage(src.chain.damage),
      acting_card_ref(src.chain.acting_card_ref),
      unknown_card_ref_a3(src.chain.unknown_card_ref_a3),
      attack_action_card_refs(src.chain.attack_action_card_refs),
      attack_action_card_ref_count(src.chain.attack_action_card_ref_count),
      attack_medium(src.chain.attack_medium),
      target_card_ref_count(src.chain.target_card_ref_count),
      action_subphase(src.chain.action_subphase),
      strike_count(src.chain.strike_count),
      damage_multiplier(src.chain.damage_multiplier),
      attack_number(src.chain.attack_number),
      tp_effect_bonus(src.chain.tp_effect_bonus),
      physical_attack_bonus_nte(src.chain.physical_attack_bonus_nte),
      tech_attack_bonus_nte(src.chain.tech_attack_bonus_nte),
      card_ap(src.chain.card_ap),
      card_tp(src.chain.card_tp),
      flags(src.chain.flags),
      conditions(src.conditions),
      target_card_refs(src.chain.target_card_refs) {}

ActionChainWithCondsTrial::operator ActionChainWithConds() const {
  ActionChainWithConds ret;
  ret.chain.effective_ap = this->effective_ap;
  ret.chain.effective_tp = this->effective_tp;
  ret.chain.ap_effect_bonus = this->ap_effect_bonus;
  ret.chain.damage = this->damage;
  ret.chain.acting_card_ref = this->acting_card_ref;
  ret.chain.unknown_card_ref_a3 = this->unknown_card_ref_a3;
  ret.chain.attack_action_card_refs = this->attack_action_card_refs;
  ret.chain.attack_action_card_ref_count = this->attack_action_card_ref_count;
  ret.chain.attack_medium = this->attack_medium;
  ret.chain.target_card_ref_count = this->target_card_ref_count;
  ret.chain.action_subphase = this->action_subphase;
  ret.chain.strike_count = this->strike_count;
  ret.chain.damage_multiplier = this->damage_multiplier;
  ret.chain.attack_number = this->attack_number;
  ret.chain.tp_effect_bonus = this->tp_effect_bonus;
  ret.chain.physical_attack_bonus_nte = this->physical_attack_bonus_nte;
  ret.chain.tech_attack_bonus_nte = this->tech_attack_bonus_nte;
  ret.chain.card_ap = this->card_ap;
  ret.chain.card_tp = this->card_tp;
  ret.chain.flags = this->flags;
  ret.chain.target_card_refs = this->target_card_refs;
  ret.conditions = this->conditions;
  return ret;
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

std::string ActionMetadata::str(shared_ptr<const Server> s) const {
  string card_ref_s = s->debug_str_for_card_ref(this->card_ref);
  string target_card_refs_s = s->debug_str_for_card_refs(this->target_card_refs);
  string defense_card_refs_s = s->debug_str_for_card_refs(this->defense_card_refs);
  string original_attacker_card_refs_s = s->debug_str_for_card_refs(this->original_attacker_card_refs);
  return phosg::string_printf(
      "ActionMetadata[ref=%s, target_ref_count=%hhu, def_ref_count=%hhu, "
      "subphase=%s, def_power=%hhd, def_bonus=%hhd, "
      "att_bonus=%hhd, flags=%08" PRIX32 ", target_refs=%s, "
      "defense_refs=%s, original_attacker_refs=%s]",
      card_ref_s.c_str(),
      this->target_card_ref_count,
      this->defense_card_ref_count,
      phosg::name_for_enum(this->action_subphase),
      this->defense_power,
      this->defense_bonus,
      this->attack_bonus,
      this->flags.load(),
      target_card_refs_s.c_str(),
      defense_card_refs_s.c_str(),
      original_attacker_card_refs_s.c_str());
}

void ActionMetadata::clear() {
  this->card_ref = 0xFFFF;
  this->target_card_ref_count = 0;
  this->defense_card_ref_count = 0;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->defense_power = 0;
  this->defense_bonus = 0;
  // TODO: Ep3 NTE doesn't set attack_bonus to zero here. Is the field just
  // unused in NTE?
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

std::string HandAndEquipState::str(shared_ptr<const Server> s) const {
  string assist_card_ref_s = s->debug_str_for_card_ref(this->assist_card_ref);
  string assist_card_ref2_s = s->debug_str_for_card_ref(this->assist_card_ref2);
  string assist_card_id_s = s->debug_str_for_card_id(this->assist_card_id);
  string sc_card_ref_s = s->debug_str_for_card_ref(this->sc_card_ref);
  string hand_card_refs_s = s->debug_str_for_card_refs(this->hand_card_refs);
  string set_card_refs_s = s->debug_str_for_card_refs(this->set_card_refs);
  string hand_card_refs2_s = s->debug_str_for_card_refs(this->hand_card_refs2);
  string set_card_refs2_s = s->debug_str_for_card_refs(this->set_card_refs2);
  return phosg::string_printf(
      "HandAndEquipState[dice=[%hhu, %hhu], atk=%hhu, def=%hhu, atk2=%hhu, "
      "a1=%hhu, total_set_cost=%hhu, is_cpu=%hhu, assist_flags=%08" PRIX32 ", "
      "hand_refs=%s, assist_ref=%s, set_refs=%s, sc_ref=%s, hand_refs2=%s, "
      "set_refs2=%s, assist_ref2=%s, assist_set_num=%hu, assist_card_id=%s, "
      "assist_turns=%hhu, assist_delay=%hhu, atk_bonus=%hhu, def_bonus=%hhu, "
      "u2=[%hhu, %hhu]]",
      this->dice_results[0],
      this->dice_results[1],
      this->atk_points,
      this->def_points,
      this->atk_points2,
      this->unknown_a1,
      this->total_set_cards_cost,
      this->is_cpu_player,
      this->assist_flags.load(),
      hand_card_refs_s.c_str(),
      assist_card_ref_s.c_str(),
      set_card_refs_s.c_str(),
      sc_card_ref_s.c_str(),
      hand_card_refs2_s.c_str(),
      set_card_refs2_s.c_str(),
      assist_card_ref2_s.c_str(),
      this->assist_card_set_number.load(),
      assist_card_id_s.c_str(),
      this->assist_remaining_turns,
      this->assist_delay_turns,
      this->atk_bonuses,
      this->def_bonuses,
      this->unused2[0],
      this->unused2[1]);
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

PlayerBattleStats::PlayerBattleStats() {
  this->clear();
}

void PlayerBattleStats::clear() {
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

float PlayerBattleStats::score(size_t num_rounds) const {
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
  return 38.0f + 0.8f * this->action_card_negated_damage - 2.3f * num_rounds - 1.8f * this->sc_damage_taken + 3.0f * this->max_attack_combo_size + (this->damage_given - this->damage_taken);
}

uint8_t PlayerBattleStats::rank(size_t num_rounds) const {
  return this->rank_for_score(this->score(num_rounds));
}

const char* PlayerBattleStats::rank_name(size_t num_rounds) const {
  return this->name_for_rank(this->rank_for_score(this->score(num_rounds)));
}

constexpr size_t RANK_THRESHOLD_COUNT = 9;
static const float RANK_THRESHOLDS[RANK_THRESHOLD_COUNT] = {
    15.0f, 25.0f, 30.0f, 40.0f, 50.0f, 60.0f, 65.0f, 75.0f, 85.0f};
static const char* RANK_NAMES[RANK_THRESHOLD_COUNT + 1] = {
    "E", "D", "D+", "C", "C+", "B", "B+", "A", "A+", "S"};

uint8_t PlayerBattleStats::rank_for_score(float score) {
  size_t rank = 0;
  while (rank < RANK_THRESHOLD_COUNT && RANK_THRESHOLDS[rank] <= score) {
    rank++;
  }
  return rank;
}

const char* PlayerBattleStats::name_for_rank(uint8_t rank) {
  if (rank >= RANK_THRESHOLD_COUNT + 1) {
    throw invalid_argument("invalid rank");
  }
  return RANK_NAMES[rank];
}

PlayerBattleStatsTrial::PlayerBattleStatsTrial(const PlayerBattleStats& data)
    : damage_given(data.damage_given.load()),
      damage_taken(data.damage_taken.load()),
      num_opponent_cards_destroyed(data.num_opponent_cards_destroyed.load()),
      num_owned_cards_destroyed(data.num_owned_cards_destroyed.load()),
      total_move_distance(data.total_move_distance.load()) {}

PlayerBattleStatsTrial::operator PlayerBattleStats() const {
  PlayerBattleStats ret;
  ret.damage_given = this->damage_given.load();
  ret.damage_taken = this->damage_taken.load();
  ret.num_opponent_cards_destroyed = this->num_opponent_cards_destroyed.load();
  ret.num_owned_cards_destroyed = this->num_owned_cards_destroyed.load();
  ret.total_move_distance = this->total_move_distance.load();
  return ret;
}

static bool is_card_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& anchor_loc,
    const CardShortStatus& ss,
    phosg::PrefixedLogger* log) {
  if (ss.card_ref == 0xFFFF) {
    if (log) {
      log->debug("is_card_within_range: (false) ss.card_ref missing");
    }
    return false;
  }
  if (range[0] == 2) {
    if (log) {
      log->debug("is_card_within_range: (true) range is entire field");
    }
    return true;
  }

  if ((ss.loc.x < anchor_loc.x - 4) || (ss.loc.x > anchor_loc.x + 4)) {
    if (log) {
      log->debug("is_card_within_range: (false) outside x range (ss.loc.x=%hhu, anchor_loc.x=%hhu)", ss.loc.x, anchor_loc.x);
    }
    return false;
  }
  if ((ss.loc.y < anchor_loc.y - 4) || (ss.loc.y > anchor_loc.y + 4)) {
    if (log) {
      log->debug("is_card_within_range: (false) outside y range (ss.loc.y=%hhu, anchor_loc.y=%hhu)", ss.loc.y, anchor_loc.y);
    }
    return false;
  }

  uint8_t y_index = (ss.loc.y - anchor_loc.y) + 4;
  uint8_t x_index = (ss.loc.x - anchor_loc.x) + 4;
  bool ret = (range[y_index * 9 + x_index] != 0);
  if (log) {
    log->debug("is_card_within_range: (%s) (ss.loc=(%hhu,%hhu), anchor_loc=(%hhu,%hhu), indexes=(%hhu,%hhu))",
        ret ? "true" : "false", ss.loc.x, ss.loc.y, anchor_loc.x, anchor_loc.y, x_index, y_index);
  }
  return ret;
}

vector<uint16_t> get_card_refs_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    const parray<CardShortStatus, 0x10>& short_statuses,
    phosg::PrefixedLogger* log) {
  vector<uint16_t> ret;
  if (is_card_within_range(range, loc, short_statuses[0], log)) {
    if (log) {
      log->debug("get_card_refs_within_range: sc card @%04hX within range", short_statuses[0].card_ref.load());
    }
    ret.emplace_back(short_statuses[0].card_ref);
  } else {
    if (log) {
      log->debug("get_card_refs_within_range: sc card @%04hX not within range", short_statuses[0].card_ref.load());
    }
  }
  for (size_t card_index = 7; card_index < 15; card_index++) {
    const auto& ss = short_statuses[card_index];
    if (is_card_within_range(range, loc, ss, log)) {
      if (log) {
        log->debug("get_card_refs_within_range: card @%04hX within range", ss.card_ref.load());
      }
      ret.emplace_back(ss.card_ref);
    } else {
      if (log) {
        log->debug("get_card_refs_within_range: card @%04hX not within range", ss.card_ref.load());
      }
    }
  }
  return ret;
}

} // namespace Episode3
