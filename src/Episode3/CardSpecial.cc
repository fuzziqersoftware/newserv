#include "Server.hh"

#include <math.h>

using namespace std;

namespace Episode3 {

static uint16_t ref_for_card(shared_ptr<const Card> card) {
  if (card) {
    return card->get_card_ref();
  } else {
    return 0xFFFF;
  }
}

template <typename T>
static string refs_str_for_cards_vector(const vector<shared_ptr<T>>& cards) {
  string ret;
  for (const auto& card : cards) {
    if (!ret.empty()) {
      ret += ", ";
    }
    ret += string_printf("%04hX", ref_for_card(card));
  }
  return ret;
}

CardSpecial::DiceRoll::DiceRoll() {
  this->clear();
}

void CardSpecial::DiceRoll::clear() {
  this->client_id = 0;
  this->unknown_a2 = 0;
  this->value = 0;
  this->value_used_in_expr = 0;
  this->unknown_a5 = 0xFFFF;
}

CardSpecial::AttackEnvStats::AttackEnvStats() {
  this->clear();
}

void CardSpecial::AttackEnvStats::clear() {
  this->num_set_cards = 0;
  this->dice_roll_value1 = 0;
  this->effective_ap = 0;
  this->effective_tp = 0;
  this->current_hp = 0;
  this->max_hp = 0;
  this->effective_ap_if_not_tech = 0;
  this->effective_ap_if_not_physical = 0;
  this->player_num_destroyed_fcs = 0;
  this->player_num_atk_points = 0;
  this->defined_max_hp = 0;
  this->dice_roll_value2 = 0;
  this->card_cost = 0;
  this->total_num_set_cards = 0;
  this->action_cards_ap = 0;
  this->action_cards_tp = 0;
  this->unknown_a1 = 0;
  this->num_item_or_creature_cards_in_hand = 0;
  this->num_destroyed_ally_fcs = 0;
  this->target_team_num_set_cards = 0;
  this->condition_giver_team_num_set_cards = 0;
  this->num_native_creatures = 0;
  this->num_a_beast_creatures = 0;
  this->num_machine_creatures = 0;
  this->num_dark_creatures = 0;
  this->num_sword_type_items = 0;
  this->num_gun_type_items = 0;
  this->num_cane_type_items = 0;
  this->effective_ap_if_not_tech2 = 0;
  this->team_dice_boost = 0;
  this->sc_effective_ap = 0;
  this->attack_bonus = 0;
  this->num_sword_type_items_on_team = 0;
  this->target_attack_bonus = 0;
  this->last_attack_preliminary_damage = 0;
  this->last_attack_damage = 0;
  this->total_last_attack_damage = 0;
  this->last_attack_damage_count = 0;
  this->target_current_hp = 0;
}

uint32_t CardSpecial::AttackEnvStats::at(size_t offset) const {
  constexpr size_t count = sizeof(*this) / sizeof(uint32_t);
  return reinterpret_cast<const parray<uint32_t, count>*>(this)->at(offset);
}

CardSpecial::CardSpecial(shared_ptr<Server> server)
    : w_server(server),
      unknown_a2(0) {}

shared_ptr<Server> CardSpecial::server() {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const Server> CardSpecial::server() const {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

void CardSpecial::adjust_attack_damage_due_to_conditions(
    shared_ptr<const Card> target_card, int16_t* inout_damage, uint16_t attacker_card_ref) {
  shared_ptr<const Card> attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
  auto attack_medium = attacker_card ? attacker_card->action_chain.chain.attack_medium : AttackMedium::UNKNOWN;

  for (size_t z = 0; z < 9; z++) {
    const auto& cond = target_card->action_chain.conditions[z];
    if (cond.type == ConditionType::NONE) {
      continue;
    }
    if (this->card_ref_has_ability_trap(cond)) {
      continue;
    }

    if (!this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
            cond.card_ref,
            target_card->get_card_ref(),
            attacker_card_ref,
            cond.card_definition_effect_index,
            attack_medium)) {
      continue;
    }

    switch (cond.type) {
      case ConditionType::WEAK_HIT_BLOCK:
        if (*inout_damage <= cond.value) {
          *inout_damage = 0;
        }
        break;

      case ConditionType::EXP_DECOY: {
        auto target_ps = target_card->player_state();
        if (target_ps) {
          uint8_t target_team_id = target_ps->get_team_id();
          int16_t exp_deduction = this->server()->team_exp[target_team_id];
          if (exp_deduction < *inout_damage) {
            *inout_damage = *inout_damage - exp_deduction;
            this->server()->team_exp[target_team_id] = 0;
          } else {
            this->server()->team_exp[target_team_id] = exp_deduction - *inout_damage;
            exp_deduction = *inout_damage;
            *inout_damage = 0;
          }
          this->send_6xB4x06_for_exp_change(
              target_card, attacker_card_ref, -exp_deduction, true);
          this->compute_team_dice_boost(target_team_id);
        }
        break;
      }

      case ConditionType::UNKNOWN_73:
        if (cond.value <= *inout_damage) {
          *inout_damage = 0;
        }
        break;

      case ConditionType::HALFGUARD:
        if (cond.value <= *inout_damage) {
          *inout_damage /= 2;
        }
        break;

      default:
        break;
    }
  }
}

void CardSpecial::adjust_dice_boost_if_team_has_condition_52(
    uint8_t team_id, uint8_t* inout_dice_boost, shared_ptr<const Card> card) {
  if (!card || (team_id == 0xFF) || !inout_dice_boost || (card->card_flags & 3)) {
    return;
  }
  auto ps = card->player_state();
  if (!ps || (ps->get_team_id() != team_id)) {
    return;
  }

  for (size_t z = 0; z < 9; z++) {
    if (!this->card_ref_has_ability_trap(card->action_chain.conditions[z]) &&
        (card->action_chain.conditions[z].type == ConditionType::UNKNOWN_52)) {
      *inout_dice_boost = *inout_dice_boost * card->action_chain.conditions[z].value8;
    }
  }
}

void CardSpecial::apply_action_conditions(
    uint8_t when,
    shared_ptr<const Card> attacker_card,
    shared_ptr<Card> defender_card,
    uint32_t flags,
    const ActionState* as) {
  ActionState temp_as;

  if (attacker_card == defender_card) {
    temp_as = this->create_attack_state_from_card_action_chain(attacker_card);
    if (as) {
      temp_as = *as;
    }
  } else {
    temp_as = this->create_defense_state_for_card_pair_action_chains(
        attacker_card, defender_card);
  }
  this->apply_defense_conditions(temp_as, when, defender_card, flags);
}

bool CardSpecial::apply_attribute_guard_if_possible(
    uint32_t flags,
    CardClass card_class,
    shared_ptr<Card> card,
    uint16_t condition_giver_card_ref,
    uint16_t attacker_card_ref) {
  shared_ptr<const Card> condition_giver_card = this->server()->card_for_set_card_ref(condition_giver_card_ref);
  if (condition_giver_card) {
    auto ce = condition_giver_card->get_definition();
    if (ce && (ce->def.card_class() == card_class)) {
      if (flags & 2) {
        card->action_chain.reset();
      }
      if (flags & 0x10) {
        card->action_metadata.defense_power = 99;
        card->action_metadata.defense_bonus = 0;
      }
    }
  }

  shared_ptr<const Card> attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
  if (attacker_card) {
    auto ce = attacker_card->get_definition();
    if (ce && (ce->def.card_class() == card_class) && (flags & 0x10)) {
      card->action_metadata.defense_power = 99;
      card->action_metadata.defense_bonus = 0;
    }
  }
  return true;
}

bool CardSpecial::apply_defense_condition(
    uint8_t when,
    Condition* defender_cond,
    uint8_t cond_index,
    const ActionState& defense_state,
    shared_ptr<Card> defender_card,
    uint32_t flags,
    bool unknown_p8) {
  if (defender_cond->type == ConditionType::NONE) {
    return false;
  }

  auto orig_eff = this->original_definition_for_condition(*defender_cond);

  uint16_t attacker_card_ref = defense_state.attacker_card_ref;
  if (attacker_card_ref == 0xFFFF) {
    attacker_card_ref = defense_state.original_attacker_card_ref;
  }

  bool defender_has_ability_trap = this->card_ref_has_ability_trap(*defender_cond);

  if (!(flags & 4) ||
      this->is_card_targeted_by_condition(*defender_cond, defense_state, defender_card)) {
    if ((when == 2) && (defender_cond->type == ConditionType::GUOM) && (flags & 4)) {
      CardShortStatus stat = defender_card->get_short_status();
      if (stat.card_flags & 4) {
        G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
        cmd.effect.flags = 0x04;
        cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x0E);
        cmd.effect.target_card_ref = defender_card->get_card_ref();
        cmd.effect.value = 0;
        cmd.effect.operation = -static_cast<int8_t>(defender_cond->type);
        cmd.effect.condition_index = cond_index;
        this->server()->send(cmd);
        this->apply_stat_deltas_to_card_from_condition_and_clear_cond(
            *defender_cond, defender_card);
        defender_card->send_6xB4x4E_4C_4D_if_needed();
        return false;
      }
    }

    if ((when == 4) && (flags & 4) && !defender_has_ability_trap &&
        (defender_cond->type == ConditionType::ACID)) {
      int16_t hp = defender_card->get_current_hp();
      if (hp > 0) {
        this->send_6xB4x06_for_stat_delta(
            defender_card, defender_cond->card_ref, 0x20, -1, 0, 1);
        defender_card->set_current_hp(hp - 1);
        this->destroy_card_if_hp_zero(defender_card, defender_cond->condition_giver_card_ref);
      }
    }

    if (!orig_eff || (orig_eff->when != when)) {
      flags = flags & 0xFFFFFFFB;
    }

    if ((flags == 0) || defender_has_ability_trap) {
      return false;
    }

    DiceRoll dice_roll;
    dice_roll.client_id = defender_card->get_client_id();
    dice_roll.unknown_a2 = 3;
    dice_roll.value = defender_cond->dice_roll_value;
    dice_roll.value_used_in_expr = false;
    uint8_t original_cond_flags = defender_cond->flags;

    auto astats = this->compute_attack_env_stats(
        defense_state, defender_card, dice_roll, defender_cond->card_ref,
        defender_cond->condition_giver_card_ref);

    string expr = orig_eff->expr;
    int16_t expr_value = this->evaluate_effect_expr(astats, expr.c_str(), dice_roll);
    this->execute_effect(
        *defender_cond, defender_card, expr_value, defender_cond->value,
        orig_eff->type, flags, attacker_card_ref);
    if (flags & 4) {
      if (!(defender_card->card_flags & 2)) {
        defender_card->compute_action_chain_results(true, false);
      }
      defender_card->action_chain.chain.card_ap = defender_card->ap;
      defender_card->action_chain.chain.card_tp = defender_card->tp;
      defender_card->send_6xB4x4E_4C_4D_if_needed();
    }

    if (dice_roll.value_used_in_expr && !(original_cond_flags & 1) && !unknown_p8) {
      defender_cond->flags |= 1;
      G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x08;
      cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x10);
      cmd.effect.target_card_ref = defender_cond->card_ref;
      cmd.effect.dice_roll_value = dice_roll.value;
      this->server()->send(cmd);
    }
    return true;

  } else {
    if (defender_cond->type != ConditionType::NONE) {
      G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x04;
      cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x0D);
      cmd.effect.target_card_ref = defender_card->get_card_ref();
      cmd.effect.value = 0;
      cmd.effect.operation = -static_cast<int8_t>(defender_cond->type);
      this->server()->send(cmd);
    }
    this->apply_stat_deltas_to_card_from_condition_and_clear_cond(
        *defender_cond, defender_card);
    defender_card->send_6xB4x4E_4C_4D_if_needed();
    return false;
  }
}

bool CardSpecial::apply_defense_conditions(
    const ActionState& as,
    uint8_t when,
    shared_ptr<Card> defender_card,
    uint32_t flags) {
  for (size_t z = 0; z < 9; z++) {
    this->apply_defense_condition(
        when, &defender_card->action_chain.conditions[z], z, as, defender_card, flags, 0);
  }
  return true;
}

bool CardSpecial::apply_stat_deltas_to_all_cards_from_all_conditions_with_card_ref(
    uint16_t card_ref) {
  bool ret = false;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (!ps) {
      continue;
    }
    auto sc_card = ps->get_sc_card();
    if (sc_card) {
      ret |= this->apply_stats_deltas_to_card_from_all_conditions_with_card_ref(
          card_ref, sc_card);
    }
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto set_card = ps->get_set_card(set_index);
      if (set_card) {
        ret |= this->apply_stats_deltas_to_card_from_all_conditions_with_card_ref(
            card_ref, set_card);
      }
    }
  }

  return ret;
}

bool CardSpecial::apply_stat_deltas_to_card_from_condition_and_clear_cond(
    Condition& cond, shared_ptr<Card> card) {
  ConditionType cond_type = cond.type;
  int16_t cond_value = clamp<int16_t>(cond.value, -99, 99);
  uint8_t cond_flags = cond.flags;
  uint16_t cond_card_ref = card->get_card_ref();
  cond.clear();

  switch (cond_type) {
    case ConditionType::UNKNOWN_0C:
      if (cond_flags & 2) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t tp = clamp<int16_t>(card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, tp - ap, 0, 0);
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, ap - tp, 0, 0);
        card->ap = tp;
        card->tp = ap;
      }
      break;
    case ConditionType::A_H_SWAP:
      if (cond_flags & 2) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        if (hp != ap) {
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, hp - ap, 0, 0);
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x20, ap - hp, 0, 0);
          card->set_current_hp(ap, 1, 1);
          card->ap = hp;
          this->destroy_card_if_hp_zero(card, cond_card_ref);
        }
      }
      break;
    case ConditionType::AP_OVERRIDE:
      if (cond_flags & 2) {
        // Note: The original code calls a function here that returns a
        // Condition pointer; however, the called function searches the card's
        // condition list and then ignores the result and unconditionally
        // returns null, completely obviating the non-null case here. We
        // implement the non-null case for documentation purposes, but it
        // appears to be completely dead code. It's unclear if this is a legit
        // bug in the original code, or if it was a debug feature or
        // late-development intentional change.
        Condition* other_cond = nullptr; // return_null???(card, ConditionType::AP_OVERRIDE);
        if (!other_cond) {
          this->send_6xB4x06_for_stat_delta(
              card, cond_card_ref, 0xA0, -cond_value, 0, 0);
          card->ap = max<int16_t>(card->ap - cond_value, 0);
        } else {
          other_cond->value = clamp<int16_t>(other_cond->value + cond_value, -99, 99);
        }
      }
      break;
    case ConditionType::TP_OVERRIDE:
      if (cond_flags & 2) {
        // Like AP_OVERRIDE above, the non-null case here is dead code in the
        // original code as well.
        Condition* other_cond = nullptr; // return_null???(card, ConditionType::TP_OVERRIDE)
        if (!other_cond) {
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, -cond_value, 0, 0);
          card->tp = max<int16_t>(card->tp - cond_value, 0);
        } else {
          other_cond->value = clamp<int16_t>(other_cond->value + cond_value, -99, 99);
        }
      }
      break;
    case ConditionType::MISC_AP_BONUSES:
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, -cond_value, 0, 0);
        card->ap = max<int16_t>(card->ap - cond_value, 0);
      }
      break;
    case ConditionType::MISC_TP_BONUSES:
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, -cond_value, 0, 0);
        card->tp = max<int16_t>(card->tp - cond_value, 0);
      }
      break;
    case ConditionType::AP_SILENCE:
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, cond_value, 0, 0);
        card->ap = max<int16_t>(card->ap + cond_value, 0);
      }
      break;
    case ConditionType::TP_SILENCE:
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, cond_value, 0, 0);
        card->tp = max<int16_t>(card->tp + cond_value, 0);
      }
      break;
    default:
      break;
  }

  return true;
}

bool CardSpecial::apply_stats_deltas_to_card_from_all_conditions_with_card_ref(
    uint16_t card_ref, shared_ptr<Card> card) {
  bool ret = false;
  for (ssize_t z = 8; z >= 0; z--) {
    auto& cond = card->action_chain.conditions[z];
    if ((cond.type != ConditionType::NONE) && (cond.card_ref == card_ref)) {
      ret |= this->apply_stat_deltas_to_card_from_condition_and_clear_cond(
          cond, card);
    }
  }
  return ret;
}

bool CardSpecial::card_has_condition_with_ref(
    shared_ptr<const Card> card,
    ConditionType cond_type,
    uint16_t card_ref,
    uint16_t match_card_ref) const {
  size_t z = 0;
  while ((z < 9) &&
      ((card->action_chain.conditions[z].type != cond_type) ||
          (card->action_chain.conditions[z].card_ref == card_ref))) {
    z++;
  }
  if (z >= 9) {
    return false;
  }
  return (match_card_ref != 0xFFFF) ? (card_ref == match_card_ref) : true;
}

bool CardSpecial::card_is_destroyed(shared_ptr<const Card> card) const {
  if (card->card_flags & 3) {
    return true;
  }
  if (card->get_current_hp() > 0) {
    return false;
  }
  return !this->server()->ruler_server->card_ref_or_any_set_card_has_condition_46(
      card->get_card_ref());
}

void CardSpecial::compute_attack_ap(
    shared_ptr<const Card> target_card,
    int16_t* out_value,
    uint16_t attacker_card_ref) {
  auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
  AttackMedium attacker_sc_attack_medium = attacker_card
      ? attacker_card->action_chain.chain.attack_medium
      : AttackMedium::UNKNOWN;
  uint16_t target_card_ref = target_card->get_card_ref();

  auto check_card = [&](shared_ptr<Card> card) -> void {
    if (!card || (card->card_flags & 3)) {
      return;
    }
    for (size_t cond_index = 0; cond_index < 9; cond_index++) {
      auto& cond = card->action_chain.conditions[cond_index];
      if (cond.type == ConditionType::NONE ||
          this->card_ref_has_ability_trap(cond) ||
          !this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
              card->action_chain.conditions[cond_index].card_ref,
              target_card->get_card_ref(),
              attacker_card_ref,
              card->action_chain.conditions[cond_index].card_definition_effect_index,
              attacker_sc_attack_medium)) {
        continue;
      }

      auto cond_type = card->action_chain.conditions[cond_index].type;
      if (((cond_type == ConditionType::UNKNOWN_5F) &&
              (target_card_ref == card->action_chain.conditions[cond_index].condition_giver_card_ref)) ||
          ((cond_type == ConditionType::UNKNOWN_60) &&
              (target_card_ref == card->action_chain.conditions[cond_index].card_ref))) {
        *out_value = card->action_chain.conditions[cond_index].value8;
      }
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        check_card(ps->get_set_card(set_index));
      }
      check_card(ps->get_sc_card());
    }
  }

  if (attacker_card &&
      attacker_card->get_attack_condition_value(ConditionType::UNKNOWN_7D, 0xFFFF, 0xFF, 0xFFFF, nullptr)) {
    *out_value = *out_value * 1.5f;
  }
  if (target_card &&
      target_card->get_attack_condition_value(ConditionType::UNKNOWN_7D, 0xFFFF, 0xFF, 0xFFFF, nullptr)) {
    *out_value = 0;
  }
}

CardSpecial::AttackEnvStats CardSpecial::compute_attack_env_stats(
    const ActionState& pa,
    shared_ptr<const Card> card,
    const DiceRoll& dice_roll,
    uint16_t target_card_ref,
    uint16_t condition_giver_card_ref) {
  this->action_state = pa;
  auto attacker_card = this->server()->card_for_set_card_ref(pa.attacker_card_ref);
  if (!attacker_card && (pa.original_attacker_card_ref != 0xFFFF)) {
    attacker_card = this->server()->card_for_set_card_ref(pa.original_attacker_card_ref);
  }

  AttackEnvStats ast;

  auto ps = card->player_state();
  ast.num_set_cards = ps->count_set_cards();
  auto condition_giver_card = this->server()->card_for_set_card_ref(condition_giver_card_ref);
  auto target_card = this->server()->card_for_set_card_ref(target_card_ref);
  if (!target_card) {
    target_card = condition_giver_card;
  }

  size_t ps_num_set_cards = 0;
  for (size_t z = 0; z < 4; z++) {
    auto other_ps = this->server()->get_player_state(z);
    if (other_ps) {
      ps_num_set_cards += other_ps->count_set_cards();
    }
  }
  ast.total_num_set_cards = ps_num_set_cards;

  uint8_t target_card_team_id = target_card
      ? target_card->player_state()->get_team_id()
      : 0xFF;

  size_t target_team_num_set_cards = 0;
  size_t condition_giver_team_num_set_cards = 0;
  for (size_t z = 0; z < 4; z++) {
    auto other_ps = this->server()->get_player_state(z);
    if (other_ps) {
      if (target_card_team_id == other_ps->get_team_id()) {
        target_team_num_set_cards += other_ps->count_set_cards();
      } else {
        condition_giver_team_num_set_cards += other_ps->count_set_cards();
      }
    }
  }
  ast.target_team_num_set_cards = target_team_num_set_cards;
  ast.condition_giver_team_num_set_cards = condition_giver_team_num_set_cards;

  ast.num_native_creatures = this->get_all_set_cards_by_team_and_class(
                                     CardClass::NATIVE_CREATURE, 0xFF, true)
                                 .size();
  ast.num_a_beast_creatures = this->get_all_set_cards_by_team_and_class(
                                      CardClass::A_BEAST_CREATURE, 0xFF, true)
                                  .size();
  ast.num_machine_creatures = this->get_all_set_cards_by_team_and_class(
                                      CardClass::MACHINE_CREATURE, 0xFF, true)
                                  .size();
  ast.num_dark_creatures = this->get_all_set_cards_by_team_and_class(
                                   CardClass::DARK_CREATURE, 0xFF, true)
                               .size();
  ast.num_sword_type_items = this->get_all_set_cards_by_team_and_class(
                                     CardClass::SWORD_ITEM, 0xFF, true)
                                 .size();
  ast.num_gun_type_items = this->get_all_set_cards_by_team_and_class(
                                   CardClass::GUN_ITEM, 0xFF, true)
                               .size();
  ast.num_cane_type_items = this->get_all_set_cards_by_team_and_class(
                                    CardClass::CANE_ITEM, 0xFF, true)
                                .size();
  ast.num_sword_type_items_on_team = card
      ? this->get_all_set_cards_by_team_and_class(CardClass::SWORD_ITEM, card->get_team_id(), true).size()
      : 0;

  size_t num_item_or_creature_cards_in_hand = 0;
  for (size_t z = 0; z < 6; z++) {
    uint16_t card_ref = ps->card_ref_for_hand_index(z);
    if (card_ref == 0xFFFF) {
      continue;
    }
    auto ce = this->server()->definition_for_card_id(card_ref);
    if (ce && ((ce->def.type == CardType::ITEM) || (ce->def.type == CardType::CREATURE))) {
      num_item_or_creature_cards_in_hand++;
    }
  }
  ast.num_item_or_creature_cards_in_hand = num_item_or_creature_cards_in_hand;

  ast.num_destroyed_ally_fcs = card->num_destroyed_ally_fcs;
  // Note: The original implementation has dice_roll as optional, but since it's
  // provided at all callsites, we require it (and hence don't check for nullptr
  // here)
  ast.dice_roll_value1 = dice_roll.value;
  ast.dice_roll_value2 = dice_roll.value;
  ast.effective_ap = card->action_chain.chain.effective_ap;
  ast.effective_tp = card->action_chain.chain.effective_tp;
  ast.current_hp = card->get_current_hp();
  ast.max_hp = card->get_max_hp();
  ast.team_dice_boost = card ? this->server()->team_dice_boost[card->get_team_id()] : 0;

  ast.effective_ap_if_not_tech = (!attacker_card || (attacker_card->action_chain.chain.attack_medium == AttackMedium::TECH))
      ? 0
      : attacker_card->action_chain.chain.damage;
  ast.effective_ap_if_not_tech2 = (!attacker_card || (attacker_card->action_chain.chain.attack_medium == AttackMedium::TECH))
      ? 0
      : attacker_card->action_chain.chain.damage;
  ast.effective_ap_if_not_physical = (!attacker_card || (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL))
      ? 0
      : attacker_card->action_chain.chain.damage;
  ast.sc_effective_ap = attacker_card ? attacker_card->action_chain.chain.damage : 0;
  ast.attack_bonus = card->action_metadata.attack_bonus;
  ast.last_attack_preliminary_damage = card->last_attack_preliminary_damage;
  ast.last_attack_damage = card->last_attack_final_damage;

  int32_t total_last_attack_damage;
  size_t last_attack_damage_count;
  this->sum_last_attack_damage(nullptr, &total_last_attack_damage, &last_attack_damage_count);
  ast.total_last_attack_damage = total_last_attack_damage;
  ast.last_attack_damage_count = last_attack_damage_count;

  if (!target_card) {
    ast.target_attack_bonus = 0;
    ast.target_current_hp = 0;
  } else {
    ast.target_attack_bonus = target_card->action_metadata.attack_bonus;
    ast.target_current_hp = target_card->get_current_hp();
  }
  ast.player_num_destroyed_fcs = ps->num_destroyed_fcs;
  ast.player_num_atk_points = ps->get_atk_points();

  auto ce = card->get_definition();
  ast.card_cost = ce->def.self_cost;
  ast.defined_max_hp = ast.max_hp;

  size_t z;
  // Note: The (z < 9) conditions in these two loops are not present in the
  // original code.
  for (z = 0;
       ((target_card_ref != pa.attacker_card_ref) && (z < 9) && (pa.action_card_refs[z] != 0xFFFF));
       z++) {
  }
  ast.action_cards_ap = 0;
  ast.action_cards_tp = 0;
  for (; (z < 9) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_a2 = pa.action_card_refs[z];
    auto ce = this->server()->definition_for_card_ref(pa.action_card_refs[z]);
    if (ce) {
      if (ce->def.ap.type != CardDefinition::Stat::Type::MINUS_STAT) {
        ast.action_cards_ap += ce->def.ap.stat;
      }
      if (ce->def.tp.type != CardDefinition::Stat::Type::MINUS_STAT) {
        ast.action_cards_tp += ce->def.tp.stat;
      }
    }
  }

  return ast;
}

shared_ptr<Card> CardSpecial::compute_replaced_target_based_on_conditions(
    uint16_t target_card_ref,
    int unknown_p3,
    int unknown_p4,
    uint16_t attacker_card_ref,
    uint16_t set_card_ref,
    int unknown_p7,
    uint32_t* unknown_p9,
    uint8_t def_effect_index,
    uint32_t* unknown_p11,
    uint16_t sc_card_ref) {
  auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
  auto target_card = this->server()->card_for_set_card_ref(target_card_ref);
  uint8_t target_client_id = client_id_for_card_ref(target_card_ref);
  uint8_t target_team_id = 0xFF;
  if (unknown_p9) {
    *unknown_p9 = 0;
  }
  if (target_card) {
    target_team_id = target_card->get_team_id();
  }
  if (unknown_p11) {
    *unknown_p11 = 0;
  }

  Location target_card_loc;
  if (!target_card) {
    target_card_loc.x = 0;
    target_card_loc.y = 0;
    target_card_loc.direction = Direction::RIGHT;
  } else {
    this->get_card1_loc_with_card2_opposite_direction(
        &target_card_loc, target_card, attacker_card);
  }

  auto attack_medium = attacker_card ? attacker_card->action_chain.chain.attack_medium : AttackMedium::INVALID_FF;

  if ((this->server()->get_battle_phase() != BattlePhase::ACTION) ||
      (this->server()->get_current_action_subphase() == ActionSubphase::ATTACK)) {
    return nullptr;
  }
  if (target_card_ref == attacker_card_ref) {
    return nullptr;
  }
  if (target_card_ref == set_card_ref) {
    return nullptr;
  }

  bool has_pierce = ((target_client_id != 0xFF) &&
      attacker_card &&
      (attacker_card->action_chain.check_flag(0x00002000 << target_client_id)));

  // Handle Parry if present
  if (target_card && !(target_card->card_flags & 3)) {
    for (size_t x = 0; x < 9; x++) {
      auto& cond = target_card->action_chain.conditions[x];
      if ((unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
        continue;
      }
      if (cond.type == ConditionType::NONE) {
        continue;
      }
      if (!this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
              target_card->action_chain.conditions[x].card_ref,
              target_card->get_card_ref(),
              attacker_card_ref,
              target_card->action_chain.conditions[x].card_definition_effect_index,
              attack_medium)) {
        continue;
      }
      if (target_card->action_chain.conditions[x].type != ConditionType::PARRY) {
        continue;
      }
      auto target_ps = target_card->player_state();
      if (has_pierce || (unknown_p7 != 0) || !target_ps) {
        continue;
      }

      // Parry forwards the attack to a random FC within one tile of the
      // original target. Note that Sega's implementation (used here) hardcodes
      // the Gifoie card's ID (00D9) for compute_effective_range.
      // TODO: We should fix this so it doesn't rely on a fixed card definition.
      parray<uint8_t, 9 * 9> range;
      compute_effective_range(range, this->server()->card_index, 0x00D9, target_card_loc, this->server()->map_and_rules1);
      auto card_refs_in_parry_range = target_ps->get_all_cards_within_range(
          range, target_card_loc, 0xFF);

      // Filter out the attacker card ref, the set card ref, the original
      // target, and any SCs within the range
      vector<uint16_t> candidate_card_refs;
      for (uint16_t card_ref : card_refs_in_parry_range) {
        if (attacker_card_ref == card_ref) {
          continue;
        }
        if (set_card_ref == card_ref) {
          continue;
        }
        if (target_card_ref == card_ref) {
          continue;
        }
        auto ce = this->server()->definition_for_card_ref(card_ref);
        if (ce && ((ce->def.type == CardType::HUNTERS_SC) || (ce->def.type == CardType::ARKZ_SC))) {
          continue;
        }
        candidate_card_refs.emplace_back(card_ref);
      }

      size_t num_candidates = candidate_card_refs.size();
      if (num_candidates > 0) {
        uint8_t a = target_ps->roll_dice_with_effects(2);
        uint8_t b = target_ps->roll_dice_with_effects(1);
        return this->server()->card_for_set_card_ref(
            candidate_card_refs[(a + b) - ((a + b) / num_candidates) * num_candidates]);
      }
    }
  }

  // Note: Some vestigial functionality was removed here. The original code has
  // a parallel array of booleans that seem to specify a priority: if any of the
  // candidate cards has true in the priority array, then the first candidate
  // card with a true value is returned instead of a random entry from the
  // entire array. The original code only puts false values into the priority
  // array, effectively rendering it unused, so we've omitted it entirely.
  vector<shared_ptr<Card>> candidate_cards;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = this->server()->get_player_state(client_id);
    if (!other_ps) {
      continue;
    }

    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto other_set_card = other_ps->get_set_card(set_index);
      if (!other_set_card || (other_set_card->card_flags & 3)) {
        continue;
      }

      for (size_t z = 0; (z < 9) && (candidate_cards.size() < 36); z++) {
        auto& cond = other_set_card->action_chain.conditions[z];
        if ((unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
          continue;
        }
        if (cond.type == ConditionType::NONE) {
          continue;
        }
        if (!this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                other_set_card->action_chain.conditions[z].card_ref,
                other_set_card->get_card_ref(),
                attacker_card_ref,
                other_set_card->action_chain.conditions[z].card_definition_effect_index,
                attack_medium)) {
          continue;
        }

        switch (other_set_card->action_chain.conditions[z].type) {
          case ConditionType::GUARD_CREATURE:
            if (!has_pierce &&
                (unknown_p7 != 0) &&
                ((unknown_p3 != 0) || (unknown_p4 != 0)) &&
                (target_client_id == client_id) &&
                target_card &&
                target_card->get_definition()->def.is_sc()) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::DEFENDER:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                (unknown_p4 != 0) &&
                (target_card_ref == other_set_card->action_chain.conditions[z].condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_set_card);
              if (unknown_p11 && (def_effect_index != 0xFF) && (set_card_ref != 0xFFFF) &&
                  !this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                      set_card_ref, sc_card_ref, other_set_card->get_card_ref(), def_effect_index, attack_medium)) {
                *unknown_p11 = 1;
              }
            }
            break;
          case ConditionType::UNKNOWN_39:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                (unknown_p3 != 0) &&
                (target_card_ref == other_set_card->action_chain.conditions[z].condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::SURVIVAL_DECOYS:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                attacker_card &&
                (attacker_card->action_chain.chain.target_card_ref_count > 1) &&
                (unknown_p3 != 0) &&
                (other_set_card->get_team_id() == target_team_id)) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::REFLECT:
            if ((unknown_p7 == 0) && (unknown_p3 != 0)) {
              if (target_card_ref == other_set_card->action_chain.conditions[z].condition_giver_card_ref) {
                if (unknown_p9) {
                  *unknown_p9 = 0;
                }
                return other_set_card;
              } else if (unknown_p9) {
                *unknown_p9 = 1;
              }
            }
            break;
          default:
            break;
        }
      }
    }

    auto other_sc = other_ps->get_sc_card();
    if (other_sc && !(other_sc->card_flags & 3)) {
      for (size_t z = 0; (z < 9) && (candidate_cards.size() < 36); z++) {
        auto& cond = other_sc->action_chain.conditions[z];
        if ((unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
          continue;
        }
        if (cond.type == ConditionType::NONE) {
          continue;
        }
        if (!this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                cond.card_ref,
                other_sc->get_card_ref(),
                attacker_card_ref,
                cond.card_definition_effect_index,
                attack_medium)) {
          continue;
        }

        switch (cond.type) {
          case ConditionType::GUARD_CREATURE:
            if (!has_pierce &&
                (unknown_p7 != 0) &&
                ((unknown_p3 != 0) || (unknown_p4 != 0)) &&
                (target_client_id == client_id) &&
                target_card &&
                target_card->get_definition()->def.is_sc()) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::DEFENDER:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                (unknown_p4 != 0) &&
                (target_card_ref == cond.condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_sc);
              if (unknown_p11 && (def_effect_index != 0xFF) && (set_card_ref != 0xFFFF) &&
                  !this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                      set_card_ref, sc_card_ref, other_sc->get_card_ref(), def_effect_index, attack_medium)) {
                *unknown_p11 = 1;
              }
            }
            break;
          case ConditionType::UNKNOWN_39:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                (unknown_p3 != 0) &&
                (target_card_ref == cond.condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::SURVIVAL_DECOYS:
            if (!has_pierce &&
                (unknown_p7 == 0) &&
                attacker_card &&
                (attacker_card->action_chain.chain.target_card_ref_count > 1) &&
                (unknown_p3 != 0) &&
                (other_sc->get_team_id() == target_team_id)) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::REFLECT:
            if ((unknown_p7 == 0) && (unknown_p3 != 0)) {
              if (target_card_ref == cond.condition_giver_card_ref) {
                if (unknown_p9) {
                  *unknown_p9 = 0;
                }
                return other_sc;
              } else if (unknown_p9) {
                *unknown_p9 = 1;
              }
            }
            break;
          default:
            break;
        }
      }
    }
  }

  if (candidate_cards.empty()) {
    return nullptr;
  }

  // If the set card is a candidate (or the attacker is, if there's no set
  // card), don't redirect the attack at all
  for (size_t z = 0; z < candidate_cards.size(); z++) {
    auto candidate_card = candidate_cards[z];
    uint16_t candidate_card_ref = candidate_card->get_card_ref();
    if ((set_card_ref == candidate_card_ref) ||
        ((set_card_ref == 0xFFFF) && (attacker_card_ref == candidate_card_ref))) {
      return nullptr;
    }
  }

  if (candidate_cards.size() == 1) {
    return candidate_cards[0];
  }

  uint8_t index = 0;
  auto target_ps = target_card->player_state();
  if (target_ps && (unknown_p7 == 0)) {
    uint8_t a = target_ps->roll_dice_with_effects(2);
    uint8_t b = target_ps->roll_dice_with_effects(1);
    index = (a + b) - ((a + b) / candidate_cards.size()) * candidate_cards.size();
  }
  return candidate_cards[index];
}

StatSwapType CardSpecial::compute_stat_swap_type(shared_ptr<const Card> card) const {
  if (!card) {
    return StatSwapType::NONE;
  }

  StatSwapType ret = StatSwapType::NONE;
  for (size_t cond_index = 0; cond_index < 9; cond_index++) {
    auto& cond = card->action_chain.conditions[cond_index];
    if (cond.type != ConditionType::NONE) {
      if (!this->card_ref_has_ability_trap(cond)) {
        if (cond.type == ConditionType::UNKNOWN_75) {
          if (ret == StatSwapType::A_H_SWAP) {
            ret = StatSwapType::NONE;
          } else {
            ret = StatSwapType::A_H_SWAP;
          }
        } else if (cond.type == ConditionType::A_T_SWAP) {
          if (ret == StatSwapType::A_T_SWAP) {
            ret = StatSwapType::NONE;
          } else {
            ret = StatSwapType::A_T_SWAP;
          }
        }
      }
    }
  }
  return ret;
}

void CardSpecial::compute_team_dice_boost(uint8_t team_id) {
  uint8_t value = this->server()->team_exp[team_id] / (this->server()->team_client_count[team_id] * 12);
  this->adjust_dice_boost_if_team_has_condition_52(team_id, &value, 0);
  this->server()->team_dice_boost[team_id] = min<uint8_t>(value, 8);
}

bool CardSpecial::condition_has_when_20_or_21(const Condition& cond) const {
  auto ce = this->server()->definition_for_card_ref(cond.card_ref);
  if (!ce) {
    return false;
  }
  uint8_t when = ce->def.effects[cond.card_definition_effect_index].when;
  return ((when == 0x20) || (when == 0x21));
}

size_t CardSpecial::count_action_cards_with_condition_for_all_current_attacks(
    ConditionType cond_type, uint16_t card_ref) const {
  size_t ret = 0;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      ret += this->count_action_cards_with_condition_for_current_attack(
          ps->get_sc_card(), cond_type, card_ref);
      for (size_t set_index = 0; set_index < 8; set_index++) {
        ret += this->count_action_cards_with_condition_for_current_attack(
            ps->get_set_card(set_index), cond_type, card_ref);
      }
    }
  }
  return ret;
}

size_t CardSpecial::count_action_cards_with_condition_for_current_attack(
    shared_ptr<const Card> card, ConditionType cond_type, uint16_t card_ref) const {
  if (!card) {
    return 0;
  }

  size_t ret = 0;

  auto check_card_ref = [&](uint16_t other_card_ref) {
    if (other_card_ref == card_ref) {
      return;
    }
    auto ce = this->server()->definition_for_card_ref(other_card_ref);
    if (!ce) {
      return;
    }
    for (size_t cond_index = 0; cond_index < 3; cond_index++) {
      if (ce->def.effects[cond_index].type == ConditionType::NONE) {
        break;
      }
      if (ce->def.effects[cond_index].type == cond_type) {
        ret++;
        break;
      }
    }
  };

  for (size_t z = 0; z < card->action_chain.chain.attack_action_card_ref_count; z++) {
    check_card_ref(card->action_chain.chain.attack_action_card_refs[z]);
  }
  for (size_t z = 0; z < card->action_metadata.defense_card_ref_count; z++) {
    check_card_ref(card->action_metadata.defense_card_refs[z]);
  }

  return ret;
}

size_t CardSpecial::count_cards_with_card_id_set_by_player_except_card_ref(
    uint16_t card_id, uint16_t card_ref) const {
  size_t ret = 0;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (!ps) {
      continue;
    }
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = ps->get_set_card(set_index);
      if (card &&
          (card->get_card_ref() != card_ref) &&
          (card->get_definition()->def.card_id == card_id)) {
        ret++;
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::get_all_set_cards_by_team_and_class(
    CardClass card_class, uint8_t team_id, bool exclude_destroyed_cards) const {
  vector<shared_ptr<const Card>> ret;
  auto check_card = [&](shared_ptr<const Card> card) -> void {
    if (card &&
        (!exclude_destroyed_cards || !(card->card_flags & 2)) &&
        (card->get_definition()->def.card_class() == card_class) &&
        ((team_id == 0xFF) || (card->get_team_id() == team_id))) {
      ret.emplace_back(card);
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (!ps) {
      continue;
    }
    check_card(ps->get_sc_card());
    for (size_t set_index = 0; set_index < 8; set_index++) {
      check_card(ps->get_set_card(set_index));
    }
  }

  return ret;
}

ActionState CardSpecial::create_attack_state_from_card_action_chain(
    shared_ptr<const Card> attacker_card) const {
  ActionState ret;
  if (attacker_card) {
    ret.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
        attacker_card->get_card_ref(), 4);
    for (size_t z = 0; z < attacker_card->action_chain.chain.attack_action_card_ref_count; z++) {
      ret.action_card_refs[z] = this->send_6xB4x06_if_card_ref_invalid(
          attacker_card->action_chain.chain.attack_action_card_refs[z], 5);
    }
    for (size_t z = 0; z < attacker_card->action_chain.chain.target_card_ref_count; z++) {
      ret.target_card_refs[z] = this->send_6xB4x06_if_card_ref_invalid(
          attacker_card->action_chain.chain.target_card_refs[z], 6);
    }
  }
  return ret;
}

ActionState CardSpecial::create_defense_state_for_card_pair_action_chains(
    shared_ptr<const Card> attacker_card,
    shared_ptr<const Card> defender_card) const {
  ActionState ret;
  if (defender_card && attacker_card) {
    size_t count = 0;
    for (size_t z = 0; z < defender_card->action_metadata.defense_card_ref_count; z++) {
      if ((defender_card->action_metadata.defense_card_refs[z] != 0xFFFF) &&
          (defender_card->action_metadata.original_attacker_card_refs[z] == attacker_card->get_card_ref())) {
        ret.action_card_refs[count++] = this->send_6xB4x06_if_card_ref_invalid(
            defender_card->action_metadata.defense_card_refs[z], 7);
      }
    }
  }
  if (defender_card) {
    ret.target_card_refs[0] = this->send_6xB4x06_if_card_ref_invalid(
        defender_card->get_card_ref(), 8);
  }
  if (attacker_card) {
    ret.original_attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
        attacker_card->get_card_ref(), 9);
  }
  return ret;
}

void CardSpecial::destroy_card_if_hp_zero(
    shared_ptr<Card> card, uint16_t attacker_card_ref) {
  if (card && (card->get_current_hp() <= 0)) {
    card->destroy_set_card(this->server()->card_for_set_card_ref(attacker_card_ref));
  }
}

bool CardSpecial::evaluate_effect_arg2_condition(
    const ActionState& as,
    shared_ptr<const Card> card,
    const char* arg2_text,
    DiceRoll& dice_roll,
    uint16_t set_card_ref,
    uint16_t sc_card_ref,
    uint8_t random_percent,
    uint8_t when) const {
  // Note: In the original code, as and dice_roll were optional pointers, but
  // they are non-null at all callsites, so we've replaced them with references
  // (and eliminated the null checks within this function).

  uint16_t attacker_card_ref = as.attacker_card_ref;
  if (attacker_card_ref == 0xFFFF) {
    attacker_card_ref = as.original_attacker_card_ref;
  }

  auto set_card = this->server()->card_for_set_card_ref(set_card_ref);
  bool set_card_has_ability_trap = (set_card &&
      (this->card_has_condition_with_ref(set_card, ConditionType::ABILITY_TRAP, 0xFFFF, 0xFFFF)));

  switch (arg2_text[0]) {
    case 'C':
      card = this->server()->card_for_set_card_ref(set_card_ref);
      if (!card) {
        card = this->server()->card_for_set_card_ref(sc_card_ref);
      }
      if (!card) {
        return false;
      }
      [[fallthrough]];
    case 'c': {
      uint8_t ch1 = arg2_text[1] - '0';
      uint8_t ch2 = arg2_text[2] - '0';
      if ((ch1 > 9) || (ch2 > 9)) {
        return false;
      }
      auto ps = this->server()->get_player_state(client_id_for_card_ref(card->get_card_ref()));
      if (!ps) {
        return false;
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (!card) {
          continue;
        }
        auto ce = card->get_definition();
        if (!ce) {
          continue;
        }
        for (size_t cond_index = 0; cond_index < 3; cond_index++) {
          if (ce->def.effects[cond_index].type == ConditionType::NONE) {
            break;
          }
          uint8_t arg2_command = ce->def.effects[cond_index].arg2[0];
          if ((arg2_command == 'c') || (arg2_command == 'C')) {
            uint8_t other_ch1 = ce->def.effects[cond_index].arg2[1] - 0x30;
            if ((other_ch1 > 9)) {
              return false;
            }
            if (other_ch1 == ch2) {
              return true;
            }
          }
        }
      }
      return false;
    }

    case 'b': {
      auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
      return (attacker_card && (attacker_card->action_chain.chain.damage <= atoi(arg2_text + 1)));
    }

    case 'd': {
      if (set_card_has_ability_trap) {
        return false;
      }
      uint8_t low = arg2_text[1] - '0';
      uint8_t high = arg2_text[2] - '0';
      if ((low < 10) && (high < 10)) {
        if (high < low) {
          uint8_t t = high;
          high = low;
          low = t;
        }
        dice_roll.value_used_in_expr = true;
        return ((low <= dice_roll.value) && (dice_roll.value <= high));
      }
      return false;
    }

    case 'h':
      return (atoi(arg2_text + 1) <= card->get_current_hp());

    case 'i':
      return (atoi(arg2_text + 1) >= card->get_current_hp());

    case 'm': {
      auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
      return (attacker_card && (attacker_card->action_chain.chain.damage >= atoi(arg2_text + 1)));
    }

    case 'n':
      switch (atoi(arg2_text + 1)) {
        case 0:
          return true;
        case 1:
          return (!card || (card->get_definition()->def.type == CardType::HUNTERS_SC));
        case 2:
          for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
            auto target_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
            if (target_card && target_card->check_card_flag(2)) {
              return true;
            }
          }
          return false;
        case 3:
          for (size_t z = 0; z < 8; z++) {
            uint16_t action_card_ref = as.action_card_refs[z];
            if (action_card_ref != 0xFFFF) {
              auto ce = this->server()->definition_for_card_ref(action_card_ref);
              if (card_class_is_tech_like(ce->def.card_class())) {
                return true;
              }
            }
          }
          return false;
        case 4:
          return card->action_chain.check_flag(0x0001E000);
        case 5:
          return card->action_chain.check_flag(0x00001E00);
        case 6:
          return (card->get_definition()->def.card_class() == CardClass::NATIVE_CREATURE);
        case 7:
          return (card->get_definition()->def.card_class() == CardClass::A_BEAST_CREATURE);
        case 8:
          return (card->get_definition()->def.card_class() == CardClass::MACHINE_CREATURE);
        case 9:
          return (card->get_definition()->def.card_class() == CardClass::DARK_CREATURE);
        case 10:
          return (card->get_definition()->def.card_class() == CardClass::SWORD_ITEM);
        case 11:
          return (card->get_definition()->def.card_class() == CardClass::GUN_ITEM);
        case 12:
          return (card->get_definition()->def.card_class() == CardClass::CANE_ITEM);
        case 13: {
          auto ce = card->get_definition();
          return ((ce->def.card_class() == CardClass::GUARD_ITEM) ||
              (ce->def.card_class() == CardClass::MAG_ITEM) ||
              this->server()->ruler_server->find_condition_on_card_ref(
                  card->get_card_ref(), ConditionType::GUARD_CREATURE, 0, 0, 0));
        }
        case 14:
          return card->get_definition()->def.is_sc();
        case 15:
          return ((card->action_chain.chain.attack_action_card_ref_count == 0) &&
              (card->action_metadata.defense_card_ref_count == 0));
        case 16:
          return this->server()->ruler_server->card_ref_is_aerial(card->get_card_ref());
        case 17: {
          auto sc_card = this->server()->card_for_set_card_ref(sc_card_ref);
          int16_t this_ap = card->ap;
          int16_t other_ap = -1;
          if (!sc_card) {
            auto ce = this->server()->definition_for_card_ref(sc_card_ref);
            if (ce) {
              other_ap = ce->def.ap.stat;
            }
          } else {
            other_ap = sc_card->ap;
          }
          return (other_ap == this_ap);
        }
        case 18:
          for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
            auto target_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
            if (target_card && target_card->get_definition()->def.is_sc()) {
              return true;
            }
          }
          return false;
        case 19:
          return this->server()->ruler_server->find_condition_on_card_ref(
              card->get_card_ref(), ConditionType::PARALYZE, 0, 0, 0);
        case 20:
          return this->server()->ruler_server->find_condition_on_card_ref(
              card->get_card_ref(), ConditionType::FREEZE, 0, 0, 0);
        case 21: {
          uint8_t client_id = client_id_for_card_ref(sc_card_ref);
          if (client_id != 0xFF) {
            return card->action_chain.check_flag(0x00002000 << client_id);
          }
          return false;
        }
        case 22: {
          uint8_t client_id = client_id_for_card_ref(sc_card_ref);
          if (client_id != 0xFF) {
            return card->action_chain.check_flag(0x00000200 << client_id);
          }
          return false;
        }
        default:
          return false;
      }
      throw logic_error("this should be impossible");

    case 'o': {
      uint8_t v = atoi(arg2_text + 1);
      if ((v / 10) == 1) {
        auto new_card = this->server()->card_for_set_card_ref(set_card_ref);
        if (!new_card) {
          new_card = this->server()->card_for_set_card_ref(sc_card_ref);
        }
        if (new_card) {
          card = new_card;
        }
      }
      return (this->find_condition_with_parameters(
                  card, ConditionType::ANY, set_card_ref, ((v % 10) == 0) ? 0xFF : (v % 10)) != nullptr);
    }
    case 'r':
      return !set_card_has_ability_trap && (random_percent < atoi(arg2_text + 1));
    case 's': {
      auto ce = card->get_definition();
      return ((ce->def.self_cost >= arg2_text[1] - '0') &&
          (ce->def.self_cost <= arg2_text[2] - '0'));
    }
    case 't': {
      auto set_card = this->server()->card_for_set_card_ref(set_card_ref);
      if (!set_card) {
        return false;
      }
      uint8_t v = atoi(arg2_text + 1);
      // TODO: Figure out what this logic actually does and rename the variables
      // or comment it appropriately.
      if (when == 4) {
        uint32_t y = set_card->unknown_a9 & 0xFFFFFFFE;
        if ((set_card->unknown_a9 > 0) &&
            (y == (y / (v & 0xFFFFFFFE)) * (v & 0xFFFFFFFE))) {
          return true;
        }
      } else {
        uint32_t y = set_card->unknown_a9;
        if ((set_card->unknown_a9 > 0) &&
            (y == (y / (v + 1)) * (v + 1))) {
          return true;
        }
      }
      return false;
    }
    default:
      return false;
  }
  throw logic_error("this should be impossible");
}

int32_t CardSpecial::evaluate_effect_expr(
    const AttackEnvStats& ast,
    const char* expr,
    DiceRoll& dice_roll) const {
  auto log = this->server()->log.sub("evaluate_effect_expr: ");
  log.debug("ast, expr=\"%s\", dice_roll=(client_id=%02hhX, a2=%02hhX, value=%02hhX, value_used_in_expr=%s, a5=%04hX)", expr, dice_roll.client_id, dice_roll.unknown_a2, dice_roll.value, dice_roll.value_used_in_expr ? "true" : "false", dice_roll.unknown_a5);

  // Note: This implementation is not based on the original code because the
  // original code was hard to follow - it used a look-behind approach with lots
  // of local variables instead of the look-ahead approach that this
  // implementation uses. Hopefully this implementation is easier to follow.
  vector<pair<ExpressionTokenType, int32_t>> tokens;
  while (expr) {
    ExpressionTokenType type;
    int32_t value = 0;
    expr = this->get_next_expr_token(expr, &type, &value);
    if (expr) {
      if (type == ExpressionTokenType::SPACE) {
        throw runtime_error("expression contains space token");
      }
      // Turn references into numbers, so only numbers and operators can appear
      // in the tokens vector
      if (type == ExpressionTokenType::REFERENCE) {
        if ((value == 1) || (value == 11)) {
          dice_roll.value_used_in_expr = true;
        }
        tokens.emplace_back(make_pair(ExpressionTokenType::NUMBER, ast.at(value)));
      } else {
        tokens.emplace_back(make_pair(type, value));
      }
    }
  }

  // Operators are evaluated left-to-right - there are no operator precedence
  // rules
  int32_t value = 0;
  log.debug("value=%" PRId32 " (start)", value);
  for (size_t token_index = 0; token_index < tokens.size(); token_index++) {
    auto token_type = tokens[token_index].first;
    int32_t token_value = tokens[token_index].second;
    if ((token_type == ExpressionTokenType::SPACE) || (token_type == ExpressionTokenType::REFERENCE)) {
      throw logic_error("space or reference token present in expr evaluation phase 2");
    }
    if (token_type == ExpressionTokenType::NUMBER) {
      value = token_value;
      log.debug("value=%" PRId32 " (token_type=NUMBER, token_value=%" PRId32 ")", value, token_value);
    } else {
      if (token_index >= tokens.size() - 1) {
        throw runtime_error("no token on right side of binary operator");
      }
      token_index++;
      auto right_token_type = tokens[token_index].first;
      auto right_value = tokens[token_index].second;
      if (right_token_type != ExpressionTokenType::NUMBER) {
        throw runtime_error("non-number, non-reference token on right side of operator");
      }
      switch (token_type) {
        case ExpressionTokenType::ROUND_DIVIDE:
          value = lround(static_cast<double>(value) / right_value);
          log.debug("value=%" PRId32 " (token_type=ROUND_DIVIDE, right_token_value=%" PRId32 ")", value, right_value);
          break;
        case ExpressionTokenType::SUBTRACT:
          value -= right_value;
          log.debug("value=%" PRId32 " (token_type=SUBTRACT, right_token_value=%" PRId32 ")", value, right_value);
          break;
        case ExpressionTokenType::ADD:
          value += right_value;
          log.debug("value=%" PRId32 " (token_type=ADD, right_token_value=%" PRId32 ")", value, right_value);
          break;
        case ExpressionTokenType::MULTIPLY:
          value *= right_value;
          log.debug("value=%" PRId32 " (token_type=MULTIPLY, right_token_value=%" PRId32 ")", value, right_value);
          break;
        case ExpressionTokenType::FLOOR_DIVIDE:
          value = floor(value / right_value);
          log.debug("value=%" PRId32 " (token_type=FLOOR_DIVIDE, right_token_value=%" PRId32 ")", value, right_value);
          break;
        default:
          throw logic_error("invalid binary operator");
      }
    }
  }

  log.debug("value=%" PRId32 " (result)", value);
  return value;
}

bool CardSpecial::execute_effect(
    Condition& cond,
    shared_ptr<Card> card,
    int16_t expr_value,
    int16_t unknown_p5,
    ConditionType cond_type,
    uint32_t unknown_p7,
    uint16_t attacker_card_ref) {
  auto log = this->server()->log.sub("execute_effect: ");
  {
    string cond_str = cond.str();
    log.debug("cond=%s, card=%04hX, expr_value=%hd, unknown_p5=%hd, cond_type=%s, unknown_p7=%" PRIu32 ", attacker_card_ref=%04hX", cond_str.c_str(), ref_for_card(card), expr_value, unknown_p5, name_for_condition_type(cond_type), unknown_p7, attacker_card_ref);
  }
  int16_t clamped_expr_value = clamp<int16_t>(expr_value, -99, 99);
  int16_t clamped_unknown_p5 = clamp<int16_t>(unknown_p5, -99, 99);

  cond.value8 = clamped_expr_value;
  if (this->card_ref_has_ability_trap(cond)) {
    return false;
  }
  if (card->card_flags & 1) {
    return false;
  }

  if ((card->card_flags & 3) ||
      (card->action_metadata.check_flag(0x10) &&
          (cond.card_ref != card->get_card_ref()) &&
          (cond.condition_giver_card_ref != card->get_card_ref()))) {
    unknown_p7 = unknown_p7 & 0xFFFFFFFB;
  }
  if (unknown_p7 == 0) {
    return false;
  }

  int16_t positive_expr_value = max<int16_t>(0, clamped_expr_value);
  clamped_unknown_p5 = max<int16_t>(0, clamped_unknown_p5);
  auto attacker_sc = this->server()->card_for_set_card_ref(attacker_card_ref);
  auto attack_medium = attacker_sc ? attacker_sc->action_chain.chain.attack_medium : AttackMedium::UNKNOWN;

  switch (cond_type) {
    case ConditionType::RAMPAGE:
    case ConditionType::IMMOBILE:
    case ConditionType::HOLD:
    case ConditionType::UNKNOWN_07:
    case ConditionType::GUOM:
    case ConditionType::PARALYZE:
    case ConditionType::PIERCE:
    case ConditionType::UNKNOWN_0F:
    case ConditionType::UNKNOWN_12:
    case ConditionType::UNKNOWN_13:
    case ConditionType::ACID:
    case ConditionType::UNKNOWN_15:
    case ConditionType::ABILITY_TRAP:
    case ConditionType::FREEZE:
    case ConditionType::MAJOR_PIERCE:
    case ConditionType::HEAVY_PIERCE:
    case ConditionType::MAJOR_RAMPAGE:
    case ConditionType::HEAVY_RAMPAGE:
    case ConditionType::DEF_DISABLE_BY_COST:
    default:
      return false;

    case ConditionType::UNKNOWN_39:
    case ConditionType::DEFENDER:
    case ConditionType::SURVIVAL_DECOYS:
    case ConditionType::EXP_DECOY:
    case ConditionType::SET_MV:
    case ConditionType::MV_BONUS:
      return true;

    case ConditionType::AP_BOOST:
      if (unknown_p7 & 1) {
        card->action_chain.chain.ap_effect_bonus = clamp<int8_t>(
            card->action_chain.chain.ap_effect_bonus + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::MULTI_STRIKE:
      if (unknown_p7 & 1) {
        card->action_chain.chain.strike_count = positive_expr_value;
      }
      return true;

    case ConditionType::DAMAGE_MOD_1:
      if (unknown_p7 & 2) {
        card->action_chain.chain.damage = positive_expr_value;
      }
      return true;

    case ConditionType::TP_BOOST:
      if (unknown_p7 & 1) {
        card->action_chain.chain.tp_effect_bonus = clamp<int8_t>(
            card->action_chain.chain.tp_effect_bonus + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::GIVE_DAMAGE:
      if ((unknown_p7 & 4) != 0) {
        int16_t current_hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = clamp<int16_t>(current_hp - positive_expr_value, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, -positive_expr_value, 0, 1);
        new_hp = max<int16_t>(new_hp, 0);
        if (new_hp != current_hp) {
          card->set_current_hp(new_hp);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::UNKNOWN_0C:
    case ConditionType::A_T_SWAP_PERM:
      if (unknown_p7 & 4) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t tp = clamp<int16_t>(card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, tp - ap, 0, 0);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, ap - tp, 0, 0);
        card->ap = tp;
        card->tp = ap;
        cond.flags |= 2;
      }
      return true;

    case ConditionType::A_H_SWAP:
    case ConditionType::A_H_SWAP_PERM:
      if (unknown_p7 & 4) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, hp - ap, 0, 0);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, ap - hp, 1, 0);
        cond.flags |= 2;
        if (ap != hp) {
          card->set_current_hp(ap);
          card->ap = hp;
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::HEAL:
      if (unknown_p7 & 4) {
        int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = clamp<int16_t>(hp + positive_expr_value, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, new_hp - hp, 1, 1);
        if (new_hp != hp) {
          card->set_current_hp(new_hp);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::RETURN_TO_HAND:
      if (unknown_p7 & 4) {
        uint8_t client_id = client_id_for_card_ref(card->get_card_ref());
        if (client_id == 0xFF) {
          return false;
        }
        auto ps = this->server()->player_states[client_id];
        if (!ps) {
          return false;
        }
        if ((card->card_flags & 2) || this->card_is_destroyed(card)) {
          return true;
        }
        this->send_6xB4x06_for_card_destroyed(card, attacker_card_ref);
        card->unknown_802380C0();
        if (!ps->return_set_card_to_hand1(card->get_card_ref())) {
          return ps->discard_card_or_add_to_draw_pile(card->get_card_ref(), false);
        }
      }
      return false;

    case ConditionType::MIGHTY_KNUCKLE: {
      auto ps = card->player_state();
      uint8_t atk = ps->get_atk_points();
      if (unknown_p7 & 1) {
        card->action_chain.chain.ap_effect_bonus = clamp<int16_t>(
            card->action_chain.chain.ap_effect_bonus + clamped_unknown_p5, -99, 99);
      }
      if (unknown_p7 & 4) {
        ps->subtract_atk_points(atk);
      }
      return true;
    }

    case ConditionType::UNIT_BLOW:
      if (unknown_p7 & 1) {
        int16_t count = clamp<int16_t>(this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::UNIT_BLOW, 0xFFFF), -99, 99);
        card->action_chain.chain.ap_effect_bonus = clamp<int16_t>(card->action_chain.chain.ap_effect_bonus + count * positive_expr_value, -99, 99);
      }
      return false;

    case ConditionType::CURSE:
      if (unknown_p7 & 4) {
        for (size_t z = 0; z < card->action_chain.chain.target_card_ref_count; z++) {
          auto target_card = this->server()->card_for_set_card_ref(
              card->action_chain.chain.target_card_refs[z]);
          if (target_card) {
            CardShortStatus stat = target_card->get_short_status();
            if (stat.card_flags & 2) {
              int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
              int16_t new_hp = max<int16_t>(0, hp - 1);
              this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, -1, 0, 1);
              if (hp != new_hp) {
                card->set_current_hp(new_hp);
                this->destroy_card_if_hp_zero(card, attacker_card_ref);
              }
            }
          }
        }
      }
      return true;

    case ConditionType::COMBO_AP:
      if (unknown_p7 & 1) {
        int16_t count = clamp<int16_t>(this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::COMBO_AP, 0xFFFF), -99, 99);
        card->action_chain.chain.ap_effect_bonus = clamp<int16_t>(
            card->action_chain.chain.ap_effect_bonus + count * count, -99, 99);
      }
      return false;

    case ConditionType::PIERCE_RAMPAGE_BLOCK:
      if (unknown_p7 & 4) {
        card->action_chain.set_flags(0x40);
      }
      if (unknown_p7 & 3) {
        card->action_chain.reset();
      }
      return true;

    case ConditionType::ANTI_ABNORMALITY_1:
      if (unknown_p7 & 4) {
        for (ssize_t z = 8; z >= 0; z--) {
          auto& cond = card->action_chain.conditions[z];
          if ((cond.type == ConditionType::IMMOBILE) ||
              (cond.type == ConditionType::HOLD) ||
              (cond.type == ConditionType::UNKNOWN_07) ||
              (cond.type == ConditionType::GUOM) ||
              (cond.type == ConditionType::PARALYZE) ||
              (cond.type == ConditionType::UNKNOWN_13) ||
              (cond.type == ConditionType::ACID) ||
              (cond.type == ConditionType::UNKNOWN_15) ||
              (cond.type == ConditionType::CURSE) ||
              (cond.type == ConditionType::PIERCE_RAMPAGE_BLOCK) ||
              (cond.type == ConditionType::FREEZE) ||
              (cond.type == ConditionType::UNKNOWN_1E) ||
              (cond.type == ConditionType::DROP)) {
            G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
            cmd.effect.flags = 0x04;
            cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x0C);
            cmd.effect.target_card_ref = card->get_card_ref();
            cmd.effect.value = 0;
            cmd.effect.operation = -static_cast<int8_t>(cond.type);
            cmd.effect.condition_index = z;
            this->server()->send(cmd);
            this->apply_stat_deltas_to_card_from_condition_and_clear_cond(
                cond, card);
            card->send_6xB4x4E_4C_4D_if_needed();
          }
        }
      }
      return false;

    case ConditionType::UNKNOWN_1E:
      if (unknown_p7 & 4) {
        auto sc_card = this->server()->card_for_set_card_ref(attacker_card_ref);
        if (!sc_card || (sc_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
          int16_t new_hp = lround(hp * 0.5f);
          this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, new_hp - hp, 0, 1);
          if (new_hp != hp) {
            card->set_current_hp(new_hp);
            this->destroy_card_if_hp_zero(card, attacker_card_ref);
          }
        }
      }
      return true;

    case ConditionType::EXPLOSION:
      if (unknown_p7 & 0x40) {
        int16_t count = clamp<int16_t>(this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::EXPLOSION, 0xFFFF), -99, 99);
        card->action_metadata.attack_bonus = clamp<int16_t>(count * count, -99, 99);
      }
      return false;

    case ConditionType::UNKNOWN_22:
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, positive_expr_value - card->tp, 0, 1);
        card->tp = positive_expr_value;
      }
      return true;

    case ConditionType::RETURN_TO_DECK: {
      if (!(unknown_p7 & 4)) {
        return true;
      }
      uint8_t client_id = client_id_for_card_ref(card->get_card_ref());
      if (client_id == 0xFF) {
        return false;
      }
      auto ps = this->server()->player_states[client_id];
      if (!ps) {
        return false;
      }
      card->unknown_802380C0();
      return ps->discard_card_or_add_to_draw_pile(card->get_card_ref(), true);
    }

    case ConditionType::AP_LOSS:
      if (unknown_p7 & 1) {
        card->action_chain.chain.ap_effect_bonus = clamp<int16_t>(
            card->action_chain.chain.ap_effect_bonus - positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::BONUS_FROM_LEADER:
      if (unknown_p7 & 1) {
        clamped_unknown_p5 = this->count_cards_with_card_id_set_by_player_except_card_ref(expr_value, 0xFFFF) + (card->action_chain).chain.ap_effect_bonus;
        (card->action_chain).chain.ap_effect_bonus = clamp<int16_t>(clamped_unknown_p5, -99, 99);
      }
      return true;

    case ConditionType::FILIAL: {
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, positive_expr_value, 0, 1);
        if (positive_expr_value != 0) {
          int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
          int16_t new_hp = clamp<int16_t>(hp + positive_expr_value, -99, 99);
          card->set_current_hp(new_hp, true, false);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;
    }

    case ConditionType::SNATCH:
      if (unknown_p7 & 4) {
        uint8_t attacker_client_id = client_id_for_card_ref(cond.card_ref);
        uint8_t target_client_id = client_id_for_card_ref(card->get_card_ref());
        if ((attacker_client_id != 0xFF) && (target_client_id != 0xFF)) {
          auto attacker_ps = this->server()->player_states[attacker_client_id];
          auto target_ps = this->server()->player_states[target_client_id];
          if (attacker_ps && target_ps) {
            uint8_t attacker_team_id = attacker_ps->get_team_id();
            uint8_t target_team_id = target_ps->get_team_id();
            if (positive_expr_value < this->server()->team_exp[target_team_id]) {
              this->server()->team_exp[attacker_team_id] += positive_expr_value;
              this->server()->team_exp[target_team_id] -= positive_expr_value;
            } else {
              positive_expr_value = this->server()->team_exp[target_team_id];
              this->server()->team_exp[attacker_team_id] += this->server()->team_exp[target_team_id];
              this->server()->team_exp[target_team_id] = 0;
            }
            this->compute_team_dice_boost(attacker_team_id);
            this->compute_team_dice_boost(target_team_id);
            this->send_6xB4x06_for_exp_change(card, attacker_card_ref, -positive_expr_value, 1);
            this->server()->update_battle_state_flags_and_send_6xB4x03_if_needed();
          }
        }
      }
      return true;

    case ConditionType::HAND_DISRUPTER: {
      if (unknown_p7 & 4) {
        auto ps = card->player_state();
        for (; positive_expr_value > 0; positive_expr_value--) {
          size_t hand_size = ps->get_hand_size();
          if (hand_size > 0) {
            uint8_t a = ps->roll_dice_with_effects(2);
            uint8_t b = ps->roll_dice_with_effects(1);
            uint16_t card_ref = ps->card_ref_for_hand_index(
                (a + b) - ((a + b) / hand_size) * hand_size);
            if (card_ref != 0xFFFF) {
              ps->discard_ref_from_hand(card_ref);
            }
          } else {
            break;
          }
        }
        ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
      return true;
    }

    case ConditionType::DROP:
      if (unknown_p7 & 4) {
        auto ps = card->player_state();
        if (ps) {
          uint8_t team_id = ps->get_team_id();
          int16_t delta = 0;
          if (this->server()->team_exp[team_id] < 4) {
            this->server()->team_exp[team_id] = 0;
          } else {
            delta = -3;
            this->server()->team_exp[team_id] -= 3;
          }
          this->compute_team_dice_boost(team_id);
          this->send_6xB4x06_for_exp_change(card, attacker_card_ref, delta, 1);
        }
      }
      return true;

    case ConditionType::ACTION_DISRUPTER:
      if (unknown_p7 & 4) {
        for (size_t z = 0; z < card->action_chain.chain.attack_action_card_ref_count; z++) {
          this->apply_stat_deltas_to_all_cards_from_all_conditions_with_card_ref(card->action_chain.chain.attack_action_card_refs[z]);
        }
        card->action_chain.chain.attack_action_card_ref_count = 0;
      }
      return true;

    case ConditionType::SET_HP: {
      if ((unknown_p7 & 4) && (card->action_metadata.defense_power < 99)) {
        int16_t hp = card->get_current_hp();
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, positive_expr_value - hp, 0, 1);
        if (hp != positive_expr_value) {
          card->set_current_hp(positive_expr_value, true, false);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;
    }

    case ConditionType::NATIVE_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::NATIVE_CREATURE, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::A_BEAST_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::A_BEAST_CREATURE, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::MACHINE_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::MACHINE_CREATURE, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::DARK_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::DARK_CREATURE, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::SWORD_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::SWORD_ITEM, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::GUN_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::GUN_ITEM, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::CANE_SHIELD:
      return this->apply_attribute_guard_if_possible(
          unknown_p7, CardClass::CANE_ITEM, card, cond.condition_giver_card_ref, attacker_card_ref);

    case ConditionType::UNKNOWN_38: {
      auto ps = card->player_state();
      if (ps && (unknown_p7 & 4)) {
        ps->subtract_def_points(ps->get_def_points() - positive_expr_value);
        ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
      return true;
    }

    case ConditionType::GIVE_OR_TAKE_EXP:
      if (unknown_p7 & 4) {
        uint8_t client_id = client_id_for_card_ref(card->get_card_ref());
        if ((client_id != 0xFF) && this->server()->player_states[client_id]) {
          uint8_t team_id = this->server()->player_states[client_id]->get_team_id();
          int32_t existing_exp = this->server()->team_exp[team_id];
          if ((clamped_expr_value + existing_exp) < 0) {
            clamped_expr_value = -existing_exp;
            this->server()->team_exp[team_id] = 0;
          } else {
            this->server()->team_exp[team_id] = existing_exp + clamped_expr_value;
          }
          this->send_6xB4x06_for_exp_change(card, attacker_card_ref, clamped_expr_value, 1);
          this->compute_team_dice_boost(team_id);
          this->server()->update_battle_state_flags_and_send_6xB4x03_if_needed();
        }
      }
      return true;

    case ConditionType::UNKNOWN_3D:
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, positive_expr_value - card->ap, 0, 1);
        card->ap = positive_expr_value;
      }
      return true;

    case ConditionType::DEATH_COMPANION:
      if (attacker_sc && (unknown_p7 & 4)) {
        vector<uint16_t> card_refs;
        card_refs.emplace_back(attacker_sc->get_card_ref());
        if (attacker_sc != card) {
          card_refs.emplace_back(card->get_card_ref());
        }

        for (uint16_t card_ref : card_refs) {
          auto sc_card = this->server()->card_for_set_card_ref(card_ref);
          if (sc_card && (sc_card->get_current_hp() > 0)) {
            if (this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                    cond.card_ref, cond.condition_giver_card_ref,
                    sc_card->get_card_ref(), cond.card_definition_effect_index,
                    attack_medium)) {
              this->send_6xB4x06_for_stat_delta(sc_card, attacker_card_ref, 0x20, -sc_card->get_current_hp(), 0, 1);
              sc_card->set_current_hp(0);
              this->destroy_card_if_hp_zero(sc_card, attacker_card_ref);
            }
          }
        }
      }
      return false;

    case ConditionType::GROUP:
      if (unknown_p7 & 1) {
        auto ce = card->get_definition();
        if (ce) {
          int16_t count = clamp<int16_t>(
              this->count_cards_with_card_id_set_by_player_except_card_ref(ce->def.card_id, card->get_card_ref()), -99, 99);
          card->action_chain.chain.ap_effect_bonus = clamp<int16_t>(
              card->action_chain.chain.ap_effect_bonus + count * positive_expr_value, -99, 99);
        }
      }
      return true;

    case ConditionType::BERSERK:
      if (unknown_p7 & 4) {
        int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = clamp<int16_t>(hp - this->max_all_attack_bonuses(nullptr), -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, new_hp - hp, 0, 1);
        new_hp = max<int16_t>(new_hp, 0);
        if (new_hp != hp) {
          card->set_current_hp(new_hp);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::UNKNOWN_49:
      if (unknown_p7 & 4) {
        auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
        if (attacker_card && (attacker_card != card)) {
          for (ssize_t z = 8; z >= 0; z--) {
            this->apply_stat_deltas_to_card_from_condition_and_clear_cond(
                attacker_card->action_chain.conditions[z], attacker_card);
          }
          for (size_t z = 0; z < 9; z++) {
            attacker_card->action_chain.conditions[z] = card->action_chain.conditions[z];
          }
          for (size_t z = 0; z < 9; z++) {
            auto& cond = attacker_card->action_chain.conditions[z];
            if (cond.type != ConditionType::UNKNOWN_49) {
              this->execute_effect(
                  cond, attacker_card, positive_expr_value, clamped_unknown_p5, cond.type, unknown_p7, attacker_card_ref);
            }
          }
        }
      }
      return true;

    case ConditionType::AP_GROWTH:
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, positive_expr_value, 0, 1);
        card->ap = clamp<int16_t>(card->ap + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::TP_GROWTH:
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, positive_expr_value, 0, 1);
        card->tp = clamp<int16_t>(card->tp + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::COPY:
      if (unknown_p7 & 4) {
        auto attacker_card = this->server()->card_for_set_card_ref(attacker_card_ref);
        if (attacker_card && (attacker_card != card)) {
          int16_t new_ap = clamp<int16_t>((positive_expr_value < 51) ? (card->ap / 2) : card->ap, -99, 99);
          int16_t new_tp = clamp<int16_t>((positive_expr_value < 51) ? (card->tp / 2) : card->tp, -99, 99);
          this->send_6xB4x06_for_stat_delta(
              attacker_card, attacker_card_ref, 0xA0, new_ap - attacker_card->ap, 0, 0);
          this->send_6xB4x06_for_stat_delta(
              attacker_card, attacker_card_ref, 0x80, new_tp - attacker_card->tp, 0, 0);
          attacker_card->ap = new_ap;
          attacker_card->tp = new_tp;
        }
      }
      return true;

    case ConditionType::MISC_GUARDS:
      if (unknown_p7 & 8) {
        card->action_metadata.defense_bonus = clamp<int16_t>(
            positive_expr_value + card->action_metadata.defense_bonus, -99, 99);
      }
      return true;

    case ConditionType::AP_OVERRIDE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        cond.value = clamp<int16_t>(positive_expr_value - card->ap, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, cond.value, 0, 0);
        card->ap = positive_expr_value;
        cond.flags |= 2;
      }
      return true;

    case ConditionType::TP_OVERRIDE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        cond.value = clamp<int16_t>(positive_expr_value - card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, cond.value, 0, 0);
        card->tp = positive_expr_value;
        cond.flags |= 2;
      }
      return true;

    case ConditionType::SLAYERS_ASSASSINS:
    case ConditionType::UNKNOWN_64:
    case ConditionType::FORWARD_DAMAGE:
      if (unknown_p7 & 0x20) {
        card->action_metadata.attack_bonus = clamp<int16_t>(
            positive_expr_value + card->action_metadata.attack_bonus, -99, 99);
      }
      return true;

    case ConditionType::BLOCK_ATTACK:
      if (unknown_p7 & 4) {
        card->action_metadata.set_flags(0x10);
      }
      return true;

    case ConditionType::COMBO_TP:
      if (unknown_p7 & 1) {
        ssize_t count = this->count_cards_with_card_id_set_by_player_except_card_ref(
            expr_value, 0xFFFF);
        card->action_chain.chain.tp_effect_bonus = clamp<int16_t>(
            count + card->action_chain.chain.tp_effect_bonus, -99, 99);
      }
      return true;

    case ConditionType::MISC_AP_BONUSES:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t orig_ap = clamp<int16_t>(card->ap, -99, 99);
        card->ap = clamp<int16_t>(positive_expr_value + card->ap, 0, 99);
        cond.value = clamp<int16_t>(card->ap - orig_ap, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::MISC_TP_BONUSES:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t orig_tp = clamp<int16_t>(card->tp, -99, 99);
        card->tp = clamp<int16_t>(positive_expr_value + card->tp, 0, 99);
        cond.value = clamp<int16_t>(card->tp - orig_tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::MISC_DEFENSE_BONUSES:
    case ConditionType::WEAK_SPOT_INFLUENCE:
      if (unknown_p7 & 0x20) {
        card->action_metadata.attack_bonus = clamp<int16_t>(
            card->action_metadata.attack_bonus - positive_expr_value, 0, 99);
      }
      return true;

    case ConditionType::MOSTLY_HALFGUARDS:
    case ConditionType::DAMAGE_MODIFIER_2:
      if (unknown_p7 & 0x40) {
        card->action_metadata.attack_bonus = positive_expr_value;
      }
      return true;

    case ConditionType::PERIODIC_FIELD:
      if ((unknown_p7 & 0x40) &&
          (static_cast<uint16_t>(attack_medium) == ((this->server()->get_round_num() >> 1) & 1) + 1)) {
        card->action_metadata.attack_bonus = 0;
      }
      return true;

    case ConditionType::AP_SILENCE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t prev_ap = clamp<int16_t>(card->ap, -99, 99);
        card->ap = clamp<int16_t>(card->ap - positive_expr_value, 0, 99);
        cond.value = clamp<int16_t>(prev_ap - card->ap, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, -cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::TP_SILENCE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t prev_ap = clamp<int16_t>(card->tp, -99, 99);
        card->tp = clamp<int16_t>(card->tp - positive_expr_value, 0, 99);
        cond.value = clamp<int16_t>(prev_ap - card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, -cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::RAMPAGE_AP_LOSS:
      if (unknown_p7 & 1) {
        card->action_chain.chain.tp_effect_bonus = clamp<int16_t>(
            card->action_chain.chain.tp_effect_bonus - positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::UNKNOWN_77:
      if (attacker_sc && (unknown_p7 & 4)) {
        vector<uint16_t> card_refs;
        card_refs.emplace_back(attacker_sc->get_card_ref());
        for (size_t z = 0; z < attacker_sc->action_chain.chain.target_card_ref_count; z++) {
          card_refs.emplace_back(attacker_sc->action_chain.chain.target_card_refs[z]);
        }

        for (uint16_t card_ref : card_refs) {
          auto set_card = this->server()->card_for_set_card_ref(card_ref);
          if (set_card && (set_card->get_current_hp() > 0)) {
            if (this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
                    cond.card_ref,
                    cond.condition_giver_card_ref,
                    set_card->get_card_ref(),
                    cond.card_definition_effect_index,
                    attack_medium)) {
              this->send_6xB4x06_for_stat_delta(
                  set_card, attacker_card_ref, 0x20, -set_card->get_current_hp(), 0, 1);
              set_card->set_current_hp(0);
              this->destroy_card_if_hp_zero(set_card, attacker_card_ref);
            }
          }
        }
      }
      return false;
  }
}

const Condition* CardSpecial::find_condition_with_parameters(
    shared_ptr<const Card> card,
    ConditionType cond_type,
    uint16_t set_card_ref,
    uint8_t def_effect_index) const {
  const Condition* ret = nullptr;
  uint8_t max_order = 9;
  for (size_t z = 0; z < 9; z++) {
    if (card->action_chain.conditions[z].type == ConditionType::NONE) {
      continue;
    }
    auto& cond = card->action_chain.conditions[z];
    auto orig_eff = this->original_definition_for_condition(cond);
    if (!this->card_ref_has_ability_trap(cond) &&
        ((cond_type == ConditionType::ANY) || (cond.type == cond_type)) &&
        ((set_card_ref == 0xFFFF) || (cond.card_ref == set_card_ref)) &&
        ((def_effect_index == 0xFF) || (orig_eff && (orig_eff->effect_num == def_effect_index))) &&
        (!ret || (max_order < cond.order))) {
      max_order = cond.order;
      ret = &cond;
    }
  }
  return ret;
}

Condition* CardSpecial::find_condition_with_parameters(
    shared_ptr<Card> card,
    ConditionType cond_type,
    uint16_t set_card_ref,
    uint8_t def_effect_index) const {
  return const_cast<Condition*>(this->find_condition_with_parameters(
      static_cast<shared_ptr<const Card>>(card), cond_type, set_card_ref, def_effect_index));
}

void CardSpecial::get_card1_loc_with_card2_opposite_direction(
    Location* out_loc,
    shared_ptr<const Card> card1,
    shared_ptr<const Card> card2) {
  if (card1) {
    if (!card2 || (static_cast<uint8_t>(card2->facing_direction) & 0x80)) {
      *out_loc = card1->loc;
    } else if ((card2->loc.x == card1->loc.x) && (card2->loc.y == card1->loc.y)) {
      *out_loc = card1->loc;
      out_loc->direction = card2->facing_direction;
    } else {
      *out_loc = card1->loc;
      out_loc->direction = turn_around(card2->facing_direction);
    }
  }
}

uint16_t CardSpecial::get_card_id_with_effective_range(
    shared_ptr<const Card> card1, uint16_t default_card_id, shared_ptr<const Card> card2) const {
  if (card2 && !(static_cast<uint8_t>(card2->facing_direction) & 0x80)) {
    return this->server()->ruler_server->get_card_id_with_effective_range(
        card1 ? card1->get_card_ref() : 0xFFFF, default_card_id, 0);
  }
  return default_card_id;
}

void CardSpecial::get_effective_ap_tp(
    StatSwapType type,
    int16_t* effective_ap,
    int16_t* effective_tp,
    int16_t hp,
    int16_t ap,
    int16_t tp) {
  switch (type) {
    case StatSwapType::NONE:
      *effective_ap = ap;
      *effective_tp = tp;
      break;
    case StatSwapType::A_T_SWAP:
      *effective_ap = tp;
      *effective_tp = ap;
      break;
    case StatSwapType::A_H_SWAP:
      *effective_ap = hp;
      *effective_tp = tp;
      break;
    default:
      throw logic_error("invalid stat swap state");
  }
}

const char* CardSpecial::get_next_expr_token(
    const char* expr, ExpressionTokenType* out_type, int32_t* out_value) const {
  switch (*expr) {
    case '\0':
      *out_type = ExpressionTokenType::SPACE;
      return nullptr;
    case ' ':
      *out_type = ExpressionTokenType::SPACE;
      return expr + 1;
    case '+':
      *out_type = ExpressionTokenType::ADD;
      return expr + 1;
    case '-':
      *out_type = ExpressionTokenType::SUBTRACT;
      return expr + 1;
    case '*':
      *out_type = ExpressionTokenType::MULTIPLY;
      return expr + 1;
    case '/':
      if (expr[1] == '/') {
        *out_type = ExpressionTokenType::FLOOR_DIVIDE;
        return expr + 2;
      } else {
        *out_type = ExpressionTokenType::ROUND_DIVIDE;
        return expr + 1;
      }
  }

  if ((*expr >= 'a') && (*expr <= 'z')) {
    string token_buf;
    for (; ('a' <= *expr) && (*expr < 'z'); expr++) {
      token_buf.push_back(*expr);
    }

    *out_type = ExpressionTokenType::SPACE;
    *out_value = 0x27;

    static const vector<const char*> tokens = {
        "f", "d", "ap", "tp", "hp", "mhp", "dm", "tdm", "tf", "ac", "php",
        "dc", "cs", "a", "kap", "ktp", "dn", "hf", "df", "ff", "ef", "bi",
        "ab", "mc", "dk", "sa", "gn", "wd", "tt", "lv", "adm", "ddm", "sat",
        "edm", "ldm", "rdm", "fdm", "ndm", "ehp"};
    for (size_t z = 0; z < tokens.size(); z++) {
      if (token_buf == tokens[z]) {
        *out_type = ExpressionTokenType::REFERENCE;
        *out_value = z;
        return expr;
      }
    }
    return expr;
  }

  if ((*expr >= '0') && (*expr <= '9')) {
    *out_type = ExpressionTokenType::NUMBER;
    *out_value = strtol(expr, const_cast<char**>(&expr), 10);
    return expr;
  }

  throw runtime_error("invalid card effect expression");
}

vector<shared_ptr<const Card>> CardSpecial::get_targeted_cards_for_condition(
    uint16_t card_ref,
    uint8_t def_effect_index,
    uint16_t setter_card_ref,
    const ActionState& as,
    int16_t p_target_type,
    bool apply_usability_filters) const {
  auto log = this->server()->log.sub("get_targeted_cards_for_condition: ");
  log.debug("card_ref=%04hX, def_effect_index=%02hhX, setter_card_ref=%04hX, as, p_target_type=%hd, apply_usability_filters=%s", card_ref, def_effect_index, setter_card_ref, p_target_type, apply_usability_filters ? "true" : "false");

  vector<shared_ptr<const Card>> ret;

  uint8_t client_id = client_id_for_card_ref(card_ref);
  auto card1 = this->server()->card_for_set_card_ref(card_ref);
  if (!card1) {
    card1 = this->server()->card_for_set_card_ref(setter_card_ref);
  }
  log.debug("card1=%04hX", ref_for_card(card1));

  auto card2 = this->server()->card_for_set_card_ref((as.attacker_card_ref == 0xFFFF)
          ? as.original_attacker_card_ref
          : as.attacker_card_ref);
  log.debug("card2=%04hX", ref_for_card(card2));

  Location card1_loc;
  if (!card1) {
    card1_loc.x = 0;
    card1_loc.y = 0;
    card1_loc.direction = Direction::RIGHT;
  } else {
    this->get_card1_loc_with_card2_opposite_direction(&card1_loc, card1, card2);

    string card1_loc_str = card1_loc.str();
    log.debug("card1_loc=%s", card1_loc_str.c_str());
  }

  AttackMedium attack_medium = card2
      ? card2->action_chain.chain.attack_medium
      : AttackMedium::UNKNOWN;
  log.debug("attack_medium=%s", name_for_attack_medium(attack_medium));

  auto add_card_refs = [&](const vector<uint16_t>& result_card_refs) -> void {
    for (uint16_t result_card_ref : result_card_refs) {
      auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
      if (result_card) {
        ret.emplace_back(result_card);
      }
    }
  };

  switch (p_target_type) {
    case 1:
    case 5: {
      auto result_card = this->server()->card_for_set_card_ref(setter_card_ref);
      if (result_card) {
        log.debug("(p01/p05) result_card=%04hX", ref_for_card(result_card));
        ret.emplace_back(result_card);
      } else {
        log.debug("(p01/p05) result_card=null");
      }
      break;
    }
    case 2:
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          auto result_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
          if (result_card) {
            ret.emplace_back(result_card);
          }
        }
      } else if (card2) {
        ret.emplace_back(card2);
      }
      break;
    case 3:
      if (card1) {
        auto ce = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, ce->def.card_id, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          add_card_refs(ps->get_card_refs_within_range_from_all_players(range, card1_loc, CardType::ITEM));
        }
      }
      break;
    case 4:
      size_t z;
      for (z = 0; (z < 9) && (as.action_card_refs[z] != 0xFFFF) && (as.action_card_refs[z] != card_ref); z++) {
      }
      for (; (z < 9) && (as.action_card_refs[z] != 0xFFFF); z++) {
        auto result_card = this->server()->card_for_set_card_ref(as.action_card_refs[z]);
        if (result_card) {
          ret.emplace_back(result_card);
        }
      }
      break;
    case 6:
      ret = this->get_attacker_card_and_sc_if_item(as);
      break;
    case 7: {
      auto card = this->get_attacker_card(as);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 8: {
      auto card = this->sc_card_for_client_id(client_id);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 9:
      if (card1) {
        auto ce = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, ce->def.card_id, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          add_card_refs(ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id()));
        }
      }
      break;
    case 10:
      ret = this->find_all_cards_on_same_or_other_team(client_id, true);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 11:
      ret = this->find_all_set_cards_on_client_team(client_id);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 12:
      ret = this->find_all_cards_by_aerial_attribute(false);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 13:
      ret = this->find_cards_by_condition_inc_exc(ConditionType::FREEZE);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 14:
      ret = this->find_cards_in_hp_range(-1000, 3);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 15:
      ret = this->get_all_set_cards();
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 16: {
      ret = this->find_cards_in_hp_range(8, 1000);
      string range_refs_str = refs_str_for_cards_vector(ret);
      log.debug("(p16) candidate cards = [%s]", range_refs_str.c_str());
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      range_refs_str = refs_str_for_cards_vector(ret);
      log.debug("(p16) filtered cards = [%s]", range_refs_str.c_str());
      break;
    }
    case 17: {
      auto result_card = this->server()->card_for_set_card_ref(card_ref);
      if (result_card) {
        ret.emplace_back(result_card);
      }
      break;
    }
    case 18: {
      auto card = this->sc_card_for_client_id(client_id);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 19:
      ret = this->find_all_sc_cards_of_class(CardClass::HU_SC);
      break;
    case 20:
      ret = this->find_all_sc_cards_of_class(CardClass::RA_SC);
      break;
    case 21:
      ret = this->find_all_sc_cards_of_class(CardClass::FO_SC);
      break;
    case 22:
      if (card1) {
        auto def = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again, Sega hardcodes the Gifoie card's ID here... we
          // should fix this eventually.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card &&
                (result_card->get_definition()->def.type != CardType::ITEM) &&
                (card1 != result_card)) {
              ret.emplace_back(result_card);
            }
          }
          if (card1) {
            ret.emplace_back(card1);
          }
        }
      }
      break;
    case 23: {
      auto log23 = log.sub("(p23) ");
      if (card1) {
        auto def = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again with the Gifoie hardcoding...
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          log23.debug("effective range card ID is %04hX", range_card_id);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1, &log23);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          log23.debug("%zu result card refs", result_card_refs.size());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_log = log23.subf("(result ref %04hX) ", result_card_ref);
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (!result_card) {
              result_log.debug("result card not found");
            } else if (result_card->get_definition()->def.type == CardType::ITEM) {
              result_log.debug("result card is item");
            } else {
              result_log.debug("result card found and is not item");
              ret.emplace_back(result_card);
            }
          }
        } else {
          log23.debug("def or ps is missing");
        }
      } else {
        log23.debug("card1 is missing");
      }
      break;
    }
    case 24:
      ret = this->find_cards_by_condition_inc_exc(ConditionType::PARALYZE);
      break;
    case 25:
      ret = this->find_all_cards_by_aerial_attribute(true);
      break;
    case 26:
      ret = this->find_cards_damaged_by_at_least(1);
      break;
    case 27:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::NATIVE_CREATURE, 0xFF, false);
      break;
    case 28:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::A_BEAST_CREATURE, 0xFF, false);
      break;
    case 29:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::MACHINE_CREATURE, 0xFF, false);
      break;
    case 30:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::DARK_CREATURE, 0xFF, false);
      break;
    case 31:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::SWORD_ITEM, 0xFF, false);
      break;
    case 32:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::GUN_ITEM, 0xFF, false);
      break;
    case 33:
      ret = this->get_all_set_cards_by_team_and_class(CardClass::CANE_ITEM, 0xFF, false);
      break;
    case 34:
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          auto result_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
          if (result_card &&
              result_card->get_definition() &&
              !result_card->get_definition()->def.is_sc()) {
            ret.emplace_back(result_card);
          }
        }
      } else if (card2 &&
          card2->get_definition() &&
          !card2->get_definition()->def.is_sc()) {
        ret.emplace_back(card2);
      }
      break;
    case 35:
      if (card1) {
        auto def = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again with the Gifoie hardcoding...
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card) {
              auto ce = result_card->get_definition();
              if (ce->def.type == CardType::HUNTERS_SC) {
                bool should_add = true;
                for (uint16_t other_result_card_ref : result_card_refs) {
                  if ((other_result_card_ref != result_card_ref) &&
                      (client_id_for_card_ref(other_result_card_ref) == client_id_for_card_ref(result_card_ref))) {
                    should_add = false;
                    break;
                  }
                }
                if (should_add) {
                  ret.emplace_back(result_card);
                }
              } else {
                ret.emplace_back(result_card);
              }
            }
          }
        }
      }
      break;
    case 36:
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          auto result_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
          if (result_card &&
              result_card->get_definition() &&
              result_card->get_definition()->def.is_sc()) {
            ret.emplace_back(result_card);
          }
        }
      } else if (card2 &&
          card2->get_definition() &&
          card2->get_definition()->def.is_sc()) {
        ret.emplace_back(card2);
      }
      break;
    case 37:
      ret = this->find_all_cards_on_same_or_other_team(client_id, false);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 38:
      if (card1) {
        auto def = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Yet another Gifoie hardcode location :(
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card &&
                (result_card->get_definition()->def.type != CardType::ITEM) &&
                (result_card->get_card_ref() != card_ref)) {
              ret.emplace_back(result_card);
            }
          }
        }
      }
      break;
    case 39:
      ret = this->find_all_set_cards_with_cost_in_range(4, 99);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 40:
      ret = this->find_all_set_cards_with_cost_in_range(0, 3);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 41: {
      auto ps = card1->player_state();
      if (card1 && ps) {
        // TODO: Sigh. Gifoie again.
        uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
        parray<uint8_t, 9 * 9> range;
        compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
        auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
        for (uint16_t result_card_ref : result_card_refs) {
          auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
          if (result_card &&
              (result_card != card1) &&
              (result_card->get_card_ref() != card_ref) &&
              (result_card->get_definition()->def.is_fc())) {
            ret.emplace_back(result_card);
          }
        }

        for (size_t z = 0; z < 8; z++) {
          auto result_card = ps->get_set_card(z);
          if (result_card && (card1 != result_card) &&
              (result_card->get_definition()->def.type == CardType::ITEM)) {
            bool already_in_ret = false;
            for (auto c : ret) {
              if (c == result_card) {
                already_in_ret = true;
                break;
              }
            }
            if (!already_in_ret) {
              ret.emplace_back(result_card);
            }
          }
        }
      }
      break;
    }
    case 42: {
      auto check_card = [&](shared_ptr<const Card> result_card) -> void {
        if (result_card) {
          ret.emplace_back(result_card);
          auto ce = result_card->get_definition();
          auto ps = result_card->player_state();
          if ((ce->def.type == CardType::ITEM) && ps) {
            result_card = ps->get_sc_card();
            if (result_card) {
              ret.emplace_back(result_card);
            }
          }
        }
      };
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          check_card(this->server()->card_for_set_card_ref(as.target_card_refs[z]));
        }
      } else if (card2) {
        check_card(card2);
      }
      break;
    }
    case 43:
      for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
        auto result_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
        if (!result_card) {
          continue;
        }
        auto ce = result_card->get_definition();
        auto ps = result_card->player_state();
        if (ce && !ce->def.is_sc() && result_card->check_card_flag(2) && ps) {
          auto result_sc_card = ps->get_sc_card();
          if (result_sc_card) {
            ret.emplace_back(result_sc_card);
          }
        }
      }
      break;
    case 44: {
      auto ps = this->server()->get_player_state(client_id);
      if (ps) {
        for (size_t z = 0; z < 8; z++) {
          auto result_card = ps->get_set_card(z);
          if (result_card) {
            ret.emplace_back(result_card);
          }
        }
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    }
    case 45:
      this->sum_last_attack_damage(&ret, 0, 0);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 46:
      if (card1) {
        auto def = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Yet another hardcoded card ID... but this time it's Cross
          // Slay instead of Gifoie
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x009C, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card && (result_card->get_definition()->def.type != CardType::ITEM)) {
              ret.emplace_back(result_card);
            }
          }
        }
      }
      break;
    case 47: {
      uint8_t client_id = client_id_for_card_ref(as.original_attacker_card_ref);
      if (client_id != 0xFF) {
        auto card = this->sc_card_for_client_id(client_id);
        if (card) {
          ret.emplace_back(card);
        }
      }
      break;
    }
    case 48:
      if (card1) {
        auto ce = this->server()->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          // TODO: Sigh. Gifoie. Sigh.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card) {
              auto def = result_card->get_definition();
              if (ce->def.type == CardType::HUNTERS_SC) {
                bool should_add = true;
                for (uint16_t other_result_card_ref : result_card_refs) {
                  if (other_result_card_ref != result_card_ref) {
                    if (client_id_for_card_ref(other_result_card_ref) == client_id_for_card_ref(result_card_ref)) {
                      should_add = false;
                      break;
                    }
                  }
                }
                if (should_add) {
                  ret.emplace_back(result_card);
                }
              } else {
                ret.emplace_back(result_card);
              }
            }
          }
        }
        auto result_card = this->server()->card_for_set_card_ref(setter_card_ref);
        if (result_card) {
          ret.emplace_back(result_card);
        }
      }
      break;
    case 49:
      if (card1) {
        auto ps = card1->player_state();
        if (ps) {
          // TODO: One more Gifoie here.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, this->server()->card_index, range_card_id, card1_loc, this->server()->map_and_rules1);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = this->server()->card_for_set_card_ref(result_card_ref);
            if (result_card && (result_card != card1) &&
                (result_card->get_card_ref() != card_ref) &&
                result_card->get_definition()->def.is_fc()) {
              ret.emplace_back(result_card);
            }
          }

          for (size_t set_index = 0; set_index < 8; set_index++) {
            auto result_card = ps->get_set_card(set_index);
            if (result_card && (card1 != result_card) &&
                (result_card->get_definition()->def.type == CardType::ITEM)) {
              bool should_add = true;
              for (auto c : ret) {
                if (c == result_card) {
                  should_add = false;
                  break;
                }
              }
              if (should_add) {
                ret.emplace_back(result_card);
              }
            }
          }
        }
      }
  }

  if (apply_usability_filters) {
    vector<shared_ptr<const Card>> filtered_ret;
    for (auto c : ret) {
      if (this->server()->ruler_server->check_usability_or_apply_condition_for_card_refs(
              card_ref, setter_card_ref, c->get_card_ref(), def_effect_index, attack_medium)) {
        filtered_ret.emplace_back(c);
        log.debug("usability filter: kept card %04hX", ref_for_card(c));
      } else {
        log.debug("usability filter: removed card %04hX", ref_for_card(c));
      }
    }
    return filtered_ret;
  } else {
    return ret;
  }
}

vector<shared_ptr<Card>> CardSpecial::get_targeted_cards_for_condition(
    uint16_t card_ref,
    uint8_t def_effect_index,
    uint16_t setter_card_ref,
    const ActionState& as,
    int16_t p_target_type,
    bool apply_usability_filters) {
  return this->server()->const_cast_set_cards_v(as_const(*this).get_targeted_cards_for_condition(
      card_ref, def_effect_index, setter_card_ref, as, p_target_type, apply_usability_filters));
}

bool CardSpecial::is_card_targeted_by_condition(
    const Condition& cond,
    const ActionState& as,
    shared_ptr<const Card> card) const {
  auto ce = this->server()->definition_for_card_ref(cond.card_ref);
  auto sc_card = this->server()->card_for_set_card_ref(cond.card_ref);
  if (cond.type != ConditionType::NONE) {
    if ((!sc_card || ((sc_card != card) && (sc_card->card_flags & 2))) &&
        ce &&
        ((ce->def.type == CardType::ITEM) || ce->def.is_sc()) &&
        (cond.remaining_turns != 100) &&
        (client_id_for_card_ref(card->get_card_ref()) == client_id_for_card_ref(cond.card_ref))) {
      return false;
    }
    if (cond.remaining_turns == 102) {
      if (sc_card && ((sc_card == card) || !(sc_card->card_flags & 2))) {
        auto target_cards = this->get_targeted_cards_for_condition(
            cond.card_ref,
            cond.card_definition_effect_index,
            cond.condition_giver_card_ref,
            as,
            atoi(&ce->def.effects[cond.card_definition_effect_index].arg3[1]),
            0);
        for (auto c : target_cards) {
          if (c == card) {
            return true;
          }
        }
      }
      return false;
    } else {
      return true;
    }
  }
  return true;
}

void CardSpecial::on_card_set(shared_ptr<PlayerState> ps, uint16_t card_ref) {
  auto sc_card = ps->get_sc_card();
  uint16_t sc_card_ref = sc_card ? sc_card->get_card_ref() : 0xFFFF;

  ActionState as;
  this->unknown_8024C2B0(1, card_ref, as, sc_card_ref);
}

const CardDefinition::Effect* CardSpecial::original_definition_for_condition(
    const Condition& cond) const {
  auto ce = this->server()->definition_for_card_ref(cond.card_ref);
  if (!ce) {
    return nullptr;
  }
  const auto* eff = &ce->def.effects[cond.card_definition_effect_index];
  return (eff->type == ConditionType::NONE) ? nullptr : eff;
}

bool CardSpecial::card_ref_has_ability_trap(const Condition& cond) const {
  auto card = this->server()->card_for_set_card_ref(cond.card_ref);
  if (!card) {
    return false;
  } else {
    return this->card_has_condition_with_ref(
        card, ConditionType::ABILITY_TRAP, 0xFFFF, 0xFFFF);
  }
}

void CardSpecial::send_6xB4x06_for_exp_change(
    shared_ptr<const Card> card,
    uint16_t attacker_card_ref,
    uint8_t dice_roll_value,
    bool unknown_p5) const {
  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x02;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 10);
  cmd.effect.target_card_ref = card->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.dice_roll_value = dice_roll_value;
  cmd.effect.ap = clamp<int16_t>(card->ap, 0, 99);
  cmd.effect.current_hp = clamp<int16_t>(card->get_current_hp(), 0, 99);
  if (unknown_p5 == 0) {
    cmd.effect.current_hp |= 0x80;
  }
  // NOTE: The original code appears to have a copy/paste error here: if
  // card->tp > 99, then it sets cmd.effect.ap = 99 instead of cmd.effect.tp.
  // We implement the presumably intended behavior here instead.
  cmd.effect.tp = clamp<int16_t>(card->tp, 0, 99);
  this->server()->send(cmd);
}

void CardSpecial::send_6xB4x06_for_card_destroyed(
    shared_ptr<const Card> destroyed_card, uint16_t attacker_card_ref) const {
  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
      attacker_card_ref, 0x13);
  cmd.effect.target_card_ref = destroyed_card->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = 0x7E;
  this->server()->send(cmd);
}

uint16_t CardSpecial::send_6xB4x06_if_card_ref_invalid(
    uint16_t card_ref, int16_t value) const {
  if (!this->server()->card_ref_is_empty_or_has_valid_card_id(card_ref)) {
    if (value != 0) {
      G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x04;
      cmd.effect.attacker_card_ref = 0xFFFF;
      cmd.effect.target_card_ref = 0xFFFF;
      cmd.effect.value = value;
      cmd.effect.operation = 0x7E;
      this->server()->send(cmd);
    }
    card_ref = 0xFFFF;
  }
  return card_ref;
}

void CardSpecial::send_6xB4x06_for_stat_delta(
    shared_ptr<const Card> card,
    uint16_t attacker_card_ref,
    uint32_t flags,
    int16_t hp_delta,
    bool unknown_p6,
    bool unknown_p7) const {
  if (((hp_delta > 50) || (hp_delta < -50)) && (flags == 0x20)) {
    if (hp_delta < 0) {
      hp_delta = -card->get_current_hp();
    } else {
      hp_delta = card->get_max_hp() - card->get_current_hp();
    }
  }

  if (unknown_p6) {
    hp_delta = min<int16_t>(hp_delta + card->get_current_hp(), card->get_max_hp()) - card->get_current_hp();
    if (hp_delta == 0) {
      return;
    }
  }

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = flags | 2;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 10);
  cmd.effect.target_card_ref = card->get_card_ref();
  cmd.effect.value = -hp_delta;
  cmd.effect.ap = clamp<int16_t>(card->ap, 0, 99);
  cmd.effect.current_hp = clamp<int16_t>(card->get_current_hp(), 0, 99);
  cmd.effect.tp = clamp<int16_t>(card->tp, 0, 99);
  if (!unknown_p7) {
    cmd.effect.current_hp |= 0x80;
  }
  this->server()->send(cmd);
}

bool CardSpecial::should_cancel_condition_due_to_anti_abnormality(
    const CardDefinition::Effect& eff,
    shared_ptr<const Card> card,
    uint16_t target_card_ref,
    uint16_t sc_card_ref) const {
  if (!card) {
    return false;
  }
  if ((card->card_flags & 3) ||
      (card->action_metadata.check_flag(0x10) &&
          (card->get_card_ref() != target_card_ref) &&
          (card->get_card_ref() != sc_card_ref))) {
    return true;
  }
  auto ce = card->get_definition();
  if (ce->def.is_sc() && (eff.type == ConditionType::FREEZE)) {
    return true;
  }
  switch (eff.type) {
    case ConditionType::IMMOBILE:
    case ConditionType::HOLD:
    case ConditionType::GUOM:
    case ConditionType::PARALYZE:
    case ConditionType::ACID:
    case ConditionType::CURSE:
    case ConditionType::FREEZE:
    case ConditionType::DROP: {
      const auto* cond = this->find_condition_with_parameters(card, ConditionType::ANTI_ABNORMALITY_2, 0xFFFF, 0xFF);
      return (cond != nullptr) ||
          this->server()->ruler_server->card_ref_is_boss_sc(card->get_card_ref());
    }
    default:
      return false;
  }
}

bool CardSpecial::should_return_card_ref_to_hand_on_destruction(
    uint16_t card_ref) const {
  if (card_ref == 0xFFFF) {
    return false;
  }
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (client_id == 0xFF) {
    return false;
  }
  auto ce = this->server()->definition_for_card_ref(card_ref);
  if (!ce) {
    return false;
  }
  auto ps = (client_id == 0xFF) ? nullptr : this->server()->get_player_state(client_id);
  if (!ps) {
    return false;
  }

  auto check_card = [&](shared_ptr<const Card> card) -> bool {
    if (!card) {
      return false;
    }
    for (size_t cond_index = 0; cond_index < 9; cond_index++) {
      if (this->card_ref_has_ability_trap(card->action_chain.conditions[cond_index])) {
        continue;
      }
      auto cond_type = card->action_chain.conditions[cond_index].type;
      if ((cond_type == ConditionType::RETURN) &&
          !(card->card_flags & 1) &&
          (card->get_card_ref() == card_ref)) {
        return true;
      } else if ((cond_type == ConditionType::REBORN) &&
          !(card->card_flags & 3) &&
          (ce->def.card_id == static_cast<uint16_t>(card->action_chain.conditions[cond_index].value))) {
        return true;
      }
    }
    return false;
  };

  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (check_card(ps->get_set_card(set_index))) {
      return true;
    }
  }
  return check_card(ps->get_sc_card());
}

size_t CardSpecial::sum_last_attack_damage(
    vector<shared_ptr<const Card>>* out_cards,
    int32_t* out_damage_sum,
    size_t* out_damage_count) const {
  size_t damage_count = 0;
  auto check_card = [&](shared_ptr<const Card> c) -> void {
    if (c && (c->last_attack_final_damage > 0)) {
      if (out_damage_sum) {
        *out_damage_sum += c->last_attack_final_damage;
      }
      if (out_cards) {
        out_cards->emplace_back(c);
      }
      damage_count++;
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (!ps) {
      continue;
    }
    check_card(ps->get_sc_card());
    for (size_t set_index = 0; set_index < 8; set_index++) {
      check_card(ps->get_set_card(set_index));
    }
  }

  if (out_damage_count) {
    *out_damage_count += damage_count;
  }
  return damage_count;
}

void CardSpecial::update_condition_orders(shared_ptr<Card> card) {
  vector<size_t> cond_indexes;
  for (size_t z = 0; z < 9; z++) {
    if (card->action_chain.conditions[z].type != ConditionType::NONE) {
      cond_indexes.emplace_back(z);
    }
  }

  bool modified = true;
  while (modified) {
    modified = false;
    for (size_t index_offset = 0; index_offset < cond_indexes.size() - 1; index_offset++) {
      size_t this_index = cond_indexes[index_offset];
      size_t next_index = cond_indexes[index_offset + 1];
      uint8_t this_cond_order = card->action_chain.conditions[this_index].order;
      uint8_t next_cond_order = card->action_chain.conditions[next_index].order;
      if (next_cond_order < this_cond_order) {
        card->action_chain.conditions[this_index].order = next_cond_order;
        card->action_chain.conditions[next_index].order = this_cond_order;
        modified = true;
      }
    }
  }

  size_t cond_order = 0;
  for (size_t index : cond_indexes) {
    card->action_chain.conditions[index].order = cond_order++;
  }
}

int16_t CardSpecial::max_all_attack_bonuses(size_t* out_count) const {
  int16_t max_attack_bonus = 0;
  size_t num_attack_bonuses = 0;
  auto check_card = [&](shared_ptr<const Card> c) {
    if (!c) {
      return;
    }
    if (c->action_metadata.attack_bonus > max_attack_bonus) {
      max_attack_bonus = c->action_metadata.attack_bonus;
    }
    if (c->action_metadata.attack_bonus > 0) {
      num_attack_bonuses++;
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      check_card(ps->get_sc_card());
      for (size_t set_index = 0; set_index < 8; set_index++) {
        check_card(ps->get_set_card(set_index));
      }
    }
  }

  if (out_count) {
    *out_count = num_attack_bonuses;
  }
  return max_attack_bonus;
}

void CardSpecial::unknown_80244AA8(shared_ptr<Card> card) {
  ActionState as = this->create_attack_state_from_card_action_chain(card);

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->player_states[client_id];
    if (ps) {
      auto other_card = ps->get_sc_card();
      if (other_card) {
        this->clear_invalid_conditions_on_card(other_card, as);
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto other_card = ps->get_set_card(set_index);
        if (other_card) {
          this->clear_invalid_conditions_on_card(other_card, as);
        }
      }
    }
  }

  this->apply_defense_conditions(as, 0x27, card, 4);
  this->unknown_8024C2B0(0x27, card->get_card_ref(), as, 0xFFFF);
  this->apply_defense_conditions(as, 0x13, card, 4);
  this->unknown_8024C2B0(0x13, card->get_card_ref(), as, 0xFFFF);
}

void CardSpecial::check_for_defense_interference(
    shared_ptr<const Card> attacker_card,
    shared_ptr<Card> target_card,
    int16_t* inout_unknown_p4) {
  // Note: This check is not part of the original implementation.
  if (this->server()->behavior_flags & BehaviorFlag::DISABLE_INTERFERENCE) {
    return;
  }

  if (!inout_unknown_p4) {
    return;
  }
  if (target_card->get_current_hp() > *inout_unknown_p4) {
    return;
  }

  uint16_t ally_sc_card_ref = this->server()->ruler_server->get_ally_sc_card_ref(
      target_card->get_card_ref());
  if (ally_sc_card_ref == 0xFFFF) {
    return;
  }

  auto ally_sc = this->server()->card_for_set_card_ref(ally_sc_card_ref);
  if (!ally_sc || (ally_sc->card_flags & 2)) {
    return;
  }

  uint8_t target_ally_client_id = client_id_for_card_ref(ally_sc_card_ref);
  if (target_ally_client_id == 0xFF) {
    return;
  }

  uint8_t target_client_id = client_id_for_card_ref(target_card->get_card_ref());
  if (target_client_id == 0xFF) {
    return;
  }

  auto ally_hes = this->server()->ruler_server->get_hand_and_equip_state_for_client_id(target_ally_client_id);
  if (!ally_hes || !ally_hes->is_cpu_player) {
    return;
  }

  uint16_t target_card_id = this->server()->card_id_for_card_ref(target_card->get_card_ref());
  if (target_card_id == 0xFFFF) {
    return;
  }

  uint16_t ally_sc_card_id = this->server()->card_id_for_card_ref(ally_sc_card_ref);
  if (ally_sc_card_id == 0xFFFF) {
    return;
  }

  auto target_ps = target_card->player_state();
  if (!target_ps) {
    return;
  }
  if (target_ps->unknown_a17 >= 1) {
    return;
  }
  auto entry = get_interference_probability_entry(
      target_card_id, ally_sc_card_id, false);
  if (!entry || (this->server()->get_random(99) >= entry->defense_probability)) {
    return;
  }

  target_ps->unknown_a17++;

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card->get_card_ref(), 0x12);
  cmd.effect.target_card_ref = target_card->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = 0x7D;
  this->server()->send(cmd);
  if (inout_unknown_p4) {
    *inout_unknown_p4 = 0;
    target_card->action_metadata.set_flags(0x10);
  }
}

void CardSpecial::unknown_8024C2B0(
    uint32_t when,
    uint16_t set_card_ref,
    const ActionState& as,
    uint16_t sc_card_ref,
    bool apply_defense_condition_to_all_cards,
    uint16_t apply_defense_condition_to_card_ref) {
  auto log = this->server()->log.sub("unknown_8024C2B0: ");
  {
    string as_str = as.str();
    log.debug("when=%02" PRIX32 ", set_card_ref=%04hX, as=%s, sc_card_ref=%04hX, apply_defense_condition_to_all_cards=%s, apply_defense_condition_to_card_ref=%04hX",
        when, set_card_ref, as_str.c_str(), sc_card_ref, apply_defense_condition_to_all_cards ? "true" : "false", apply_defense_condition_to_card_ref);
  }

  set_card_ref = this->send_6xB4x06_if_card_ref_invalid(set_card_ref, 1);
  auto ce = this->server()->definition_for_card_ref(set_card_ref);
  if (!ce) {
    log.debug("ce missing");
    return;
  }

  uint16_t as_attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 2);
  if (as_attacker_card_ref == 0xFFFF) {
    as_attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as.original_attacker_card_ref, 3);
  }

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 dice_cmd;
  dice_cmd.effect.target_card_ref = set_card_ref;
  bool as_action_card_refs_contains_set_card_ref = false;
  bool as_action_card_refs_contains_duplicate_of_set_card = false;
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    if (as.action_card_refs[z] == dice_cmd.effect.target_card_ref) {
      as_action_card_refs_contains_set_card_ref = true;
      break;
    }
    auto action_ce = this->server()->definition_for_card_ref(as.action_card_refs[z]);
    if (action_ce && (action_ce->def.card_id == action_ce->def.card_id)) {
      as_action_card_refs_contains_duplicate_of_set_card = true;
    }
  }

  bool unknown_v1 = as_action_card_refs_contains_duplicate_of_set_card && as_action_card_refs_contains_set_card_ref;

  uint8_t random_percent = this->server() ? this->server()->get_random(99) : 0;
  bool any_expr_used_dice_roll = false;

  DiceRoll dice_roll;
  uint8_t client_id = client_id_for_card_ref(dice_cmd.effect.target_card_ref);
  auto set_card_ps = (client_id == 0xFF) ? nullptr : this->server()->player_states[client_id];

  dice_roll.value = 1;
  if (set_card_ps) {
    dice_roll.value = set_card_ps->roll_dice_with_effects(1);
  }
  dice_roll.client_id = client_id;
  dice_roll.unknown_a2 = 3;
  dice_roll.value_used_in_expr = false;

  log.debug("inputs: dice_roll=%02hhX, random_percent=%hhu, unknown_v1=%s", dice_roll.value, random_percent, unknown_v1 ? "true" : "false");

  for (size_t def_effect_index = 0; (def_effect_index < 3) && !unknown_v1 && (ce->def.effects[def_effect_index].type != ConditionType::NONE); def_effect_index++) {
    auto effect_log = log.sub(string_printf("(effect:%zu) ", def_effect_index));
    const auto& card_effect = ce->def.effects[def_effect_index];
    string card_effect_str = card_effect.str();
    effect_log.debug("effect: %s", card_effect_str.c_str());
    if (card_effect.when != when) {
      effect_log.debug("does not apply (effect.when=%02hhX, when=%02" PRIX32 ")", card_effect.when, when);
      continue;
    }

    int16_t arg3_value = atoi(&card_effect.arg3[1]);
    effect_log.debug("arg3_value=%hd", arg3_value);
    auto targeted_cards = this->get_targeted_cards_for_condition(
        set_card_ref, def_effect_index, sc_card_ref, as, arg3_value, 1);
    string refs_str = refs_str_for_cards_vector(targeted_cards);
    effect_log.debug("targeted_cards=[%s]", refs_str.c_str());
    bool all_targets_matched = false;
    if (!targeted_cards.empty() &&
        ((card_effect.type == ConditionType::UNKNOWN_64) ||
            (card_effect.type == ConditionType::MISC_DEFENSE_BONUSES) ||
            (card_effect.type == ConditionType::MOSTLY_HALFGUARDS))) {
      effect_log.debug("special targeting applies");
      size_t count = 0;
      for (size_t z = 0; z < targeted_cards.size(); z++) {
        dice_roll.value_used_in_expr = false;
        string arg2_text = card_effect.arg2;
        if (this->evaluate_effect_arg2_condition(
                as, targeted_cards[z], arg2_text.c_str(), dice_roll,
                set_card_ref, sc_card_ref, random_percent, when)) {
          count++;
        }
        if (dice_roll.value_used_in_expr) {
          any_expr_used_dice_roll = true;
        }
      }
      if (count == targeted_cards.size()) {
        auto set_card = this->server()->card_for_set_card_ref(set_card_ref);
        if (!set_card) {
          set_card = this->server()->card_for_set_card_ref(sc_card_ref);
        }
        targeted_cards.clear();
        if (set_card != nullptr) {
          targeted_cards.emplace_back(set_card);
        }
        all_targets_matched = true;
      } else {
        targeted_cards.clear();
      }
    } else {
      effect_log.debug("special targeting does not apply");
    }

    for (size_t z = 0; z < targeted_cards.size(); z++) {
      auto target_log = effect_log.sub(string_printf("(target:%04hX) ", targeted_cards[z]->get_card_ref()));
      dice_roll.value_used_in_expr = false;
      string arg2_str = card_effect.arg2;
      target_log.debug("arg2_str = %s", arg2_str.c_str());
      if (all_targets_matched ||
          this->evaluate_effect_arg2_condition(
              as, targeted_cards[z], arg2_str.c_str(), dice_roll, set_card_ref, sc_card_ref, random_percent, when)) {
        target_log.debug("arg2 condition passed");
        auto env_stats = this->compute_attack_env_stats(
            as, targeted_cards[z], dice_roll, set_card_ref, sc_card_ref);
        string expr_str = card_effect.expr;
        int16_t value = this->evaluate_effect_expr(env_stats, expr_str.c_str(), dice_roll);
        target_log.debug("expr = %s, value = %hd", expr_str.c_str(), value);

        uint32_t unknown_v1 = 0;
        auto target_card = this->compute_replaced_target_based_on_conditions(
            targeted_cards[z]->get_card_ref(),
            0,
            1,
            as_attacker_card_ref,
            set_card_ref,
            0,
            nullptr,
            def_effect_index,
            &unknown_v1,
            sc_card_ref);
        if (!target_card) {
          target_card = targeted_cards[z];
          target_log.debug("target card (not replaced) = %04hX", target_card->get_card_ref());
        } else {
          target_log.debug("target card (replaced) = %04hX", target_card->get_card_ref());
        }

        ssize_t applied_cond_index = -1;
        if ((unknown_v1 == 0) && !this->should_cancel_condition_due_to_anti_abnormality(card_effect, target_card, dice_cmd.effect.target_card_ref, sc_card_ref)) {
          applied_cond_index = target_card->apply_abnormal_condition(
              card_effect, def_effect_index, dice_cmd.effect.target_card_ref, sc_card_ref, value, dice_roll.value, random_percent);
          target_log.debug("applied abnormal condition");
          // This debug_print call is in the original code.
          // this->debug_print(when, 4, &env_stats, "!set_abnormal..", target_card, card_effect.type);
        }

        if (applied_cond_index >= 0) {
          G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
          cmd.effect.flags = 0x04;
          cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as_attacker_card_ref, 0x14);
          cmd.effect.target_card_ref = target_card->get_card_ref();
          cmd.effect.value = (target_card->action_chain).conditions[applied_cond_index].remaining_turns;
          cmd.effect.operation = static_cast<int8_t>(card_effect.type);
          this->server()->send(cmd);
        }

        if (dice_roll.value_used_in_expr) {
          target_card->action_chain.conditions[applied_cond_index].flags |= 1;
        }
        if ((applied_cond_index >= 0) &&
            (apply_defense_condition_to_all_cards || (apply_defense_condition_to_card_ref == targeted_cards[z]->get_card_ref()))) {
          this->apply_defense_condition(
              when, &target_card->action_chain.conditions[applied_cond_index], applied_cond_index, as, target_card, 4, 1);
          target_log.debug("applied defense condition");
        }
        target_card->send_6xB4x4E_4C_4D_if_needed(0);
      } else {
        target_log.debug("arg2 condition failed");
      }
      if (dice_roll.value_used_in_expr) {
        any_expr_used_dice_roll = true;
      }
    }
  }

  if (any_expr_used_dice_roll) {
    dice_cmd.effect.flags = 0x08;
    dice_cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
        as_attacker_card_ref, 0x15);
    dice_cmd.effect.dice_roll_value = dice_roll.value;
    this->server()->send(dice_cmd);
  }
}

vector<shared_ptr<const Card>> CardSpecial::get_all_set_cards() const {
  vector<shared_ptr<const Card>> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto set_card = ps->get_set_card(set_index);
        if (set_card) {
          ret.emplace_back(set_card);
        }
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_cards_by_condition_inc_exc(
    ConditionType include_cond,
    ConditionType exclude_cond,
    AssistEffect include_eff,
    AssistEffect exclude_eff) const {
  vector<shared_ptr<const Card>> ret;
  auto check_card = [&](uint8_t client_id, shared_ptr<const Card> c) -> void {
    if (c) {
      bool should_include = false;
      bool should_exclude = false;
      for (size_t z = 0; z < 9; z++) {
        auto type = c->action_chain.conditions[z].type;
        if ((type == include_cond) || (include_cond == ConditionType::ANY_FF)) {
          should_include = true;
        }
        if ((type == exclude_cond) && (exclude_cond != ConditionType::NONE)) {
          should_exclude = true;
        }
      }
      size_t num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(client_id);
      for (size_t z = 0; z < num_assists; z++) {
        auto eff = this->server()->assist_server->get_active_assist_by_index(z);
        if ((exclude_eff != AssistEffect::NONE) &&
            ((include_eff == AssistEffect::ANY) || (include_eff == eff))) {
          should_include = true;
        }
        if ((exclude_eff != AssistEffect::NONE) && (exclude_eff == eff)) {
          should_exclude = true;
        }
      }
      if (should_include && !should_exclude) {
        ret.emplace_back(c);
      }
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (!ps) {
      continue;
    }
    for (size_t set_index = 0; set_index < 8; set_index++) {
      check_card(client_id, ps->get_set_card(set_index));
    }
    check_card(client_id, ps->get_sc_card());
  }

  return ret;
}

void CardSpecial::clear_invalid_conditions_on_card(
    shared_ptr<Card> card, const ActionState& as) {
  for (size_t cond_index = 0; cond_index < 9; cond_index++) {
    auto& cond = card->action_chain.conditions[cond_index];
    if (cond.type != ConditionType::NONE) {
      if (!this->is_card_targeted_by_condition(cond, as, card)) {
        if (cond.type != ConditionType::NONE) {
          G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
          cmd.effect.flags = 0x04;
          cmd.effect.attacker_card_ref = 0xFFFF;
          cmd.effect.target_card_ref = card->get_card_ref();
          cmd.effect.value = 0;
          cmd.effect.operation = -static_cast<int8_t>(cond.type);
          this->server()->send(cmd);
        }
        this->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, card);
        card->send_6xB4x4E_4C_4D_if_needed();
      }
    }
  }
}

const InterferenceProbabilityEntry* get_interference_probability_entry(
    uint16_t row_card_id,
    uint16_t column_card_id,
    bool is_attack) {
  static const InterferenceProbabilityEntry entries[] = {
      {0x0004, 0xFF, 0xFF},
      {0x0002, 0x04, 0x00},
      {0x0002, 0x00, 0x0F},
      {0x0003, 0x03, 0x00},
      {0x0003, 0x00, 0x0A},
      {0x0006, 0x01, 0x00},
      {0x0006, 0x00, 0x05},
      {0x0111, 0x01, 0x00},
      {0x0111, 0x00, 0x05},
      {0x0001, 0x03, 0x00},
      {0x0001, 0x00, 0x0A},
      {0x0002, 0xFF, 0xFF},
      {0x0004, 0x04, 0x00},
      {0x0004, 0x00, 0x0F},
      {0x0003, 0x06, 0x00},
      {0x0003, 0x00, 0x14},
      {0x0006, 0x04, 0x00},
      {0x0006, 0x00, 0x0F},
      {0x0003, 0xFF, 0xFF},
      {0x0004, 0x04, 0x00},
      {0x0004, 0x00, 0x0F},
      {0x0002, 0x04, 0x00},
      {0x0002, 0x00, 0x0F},
      {0x0006, 0xFF, 0xFF},
      {0x0002, 0x06, 0x00},
      {0x0002, 0x00, 0x14},
      {0x0111, 0xFF, 0xFF},
      {0x0004, 0x01, 0x00},
      {0x0004, 0x00, 0x05},
      {0x0001, 0x06, 0x00},
      {0x0001, 0x00, 0x14},
      {0x0001, 0xFF, 0xFF},
      {0x0111, 0x04, 0x00},
      {0x0111, 0x00, 0x0F},
      {0x0112, 0xFF, 0xFF},
      {0x0113, 0x06, 0x00},
      {0x0113, 0x00, 0x14},
      {0x0110, 0x06, 0x00},
      {0x0110, 0x00, 0x14},
      {0x0114, 0x01, 0x00},
      {0x0114, 0x00, 0x05},
      {0x011D, 0x02, 0x00},
      {0x011D, 0x00, 0x07},
      {0x0113, 0xFF, 0xFF},
      {0x0003, 0x03, 0x00},
      {0x0003, 0x00, 0x0A},
      {0x0112, 0x03, 0x00},
      {0x0112, 0x00, 0x0A},
      {0x0110, 0xFF, 0xFF},
      {0x0005, 0x03, 0x00},
      {0x0005, 0x00, 0x0A},
      {0x0112, 0x04, 0x00},
      {0x0112, 0x00, 0x0F},
      {0x0005, 0xFF, 0xFF},
      {0x0110, 0x03, 0x00},
      {0x0110, 0x00, 0x0A},
      {0x0114, 0xFF, 0xFF},
      {0x0005, 0x03, 0x00},
      {0x0005, 0x00, 0x0A},
      {0x0110, 0x01, 0x00},
      {0x0110, 0x00, 0x05},
      {0x0115, 0x06, 0x00},
      {0x0115, 0x00, 0x14},
      {0x0115, 0xFF, 0xFF},
      {0x0004, 0x01, 0x00},
      {0x0004, 0x00, 0x05},
      {0x0003, 0x01, 0x00},
      {0x0003, 0x00, 0x05},
      {0x0006, 0x01, 0x00},
      {0x0006, 0x00, 0x05},
      {0x0112, 0x01, 0x00},
      {0x0112, 0x00, 0x05},
      {0x0110, 0x01, 0x00},
      {0x0110, 0x00, 0x05},
      {0x0114, 0x04, 0x00},
      {0x0114, 0x00, 0x0F},
      {0x0008, 0xFF, 0xFF},
      {0x0007, 0x06, 0x00},
      {0x0007, 0x00, 0x14},
      {0x0116, 0x01, 0x00},
      {0x0116, 0x00, 0x05},
      {0x011E, 0x03, 0x00},
      {0x011E, 0x00, 0x0A},
      {0x0118, 0x06, 0x00},
      {0x0118, 0x00, 0x14},
      {0x0007, 0xFF, 0xFF},
      {0x0008, 0x06, 0x00},
      {0x0008, 0x00, 0x14},
      {0x0118, 0x01, 0x00},
      {0x0118, 0x00, 0x05},
      {0x011B, 0x03, 0x00},
      {0x011B, 0x00, 0x0A},
      {0x0116, 0xFF, 0xFF},
      {0x0008, 0x01, 0x00},
      {0x0008, 0x00, 0x05},
      {0x011C, 0x03, 0x00},
      {0x011C, 0x00, 0x0A},
      {0x011A, 0xFF, 0xFF},
      {0x0119, 0x04, 0x00},
      {0x0119, 0x00, 0x0F},
      {0x011D, 0x04, 0x00},
      {0x011D, 0x00, 0x0F},
      {0x0119, 0xFF, 0xFF},
      {0x011A, 0x04, 0x00},
      {0x011A, 0x00, 0x0F},
      {0x011D, 0x04, 0x00},
      {0x011D, 0x00, 0x0F},
      {0x011D, 0xFF, 0xFF},
      {0x0119, 0x04, 0x00},
      {0x0119, 0x00, 0x0F},
      {0x011A, 0x04, 0x00},
      {0x011A, 0x00, 0x0F},
      {0x0112, 0x01, 0x00},
      {0x0112, 0x00, 0x07},
      {0x011E, 0xFF, 0xFF},
      {0x0008, 0x03, 0x00},
      {0x0008, 0x00, 0x0A},
      {0x0118, 0x06, 0x00},
      {0x0118, 0x00, 0x14},
      {0x011C, 0xFF, 0xFF},
      {0x0116, 0x04, 0x00},
      {0x0116, 0x00, 0x0F},
      {0x011E, 0x01, 0x00},
      {0x011E, 0x00, 0x05},
      {0x0118, 0xFF, 0xFF},
      {0x011E, 0x06, 0x00},
      {0x011E, 0x00, 0x14},
      {0x011B, 0xFF, 0xFF},
      {0x0007, 0x03, 0x00},
      {0x0007, 0x00, 0x0A},
      {0x0117, 0x03, 0x00},
      {0x0117, 0x00, 0x0A},
      {0x011F, 0x06, 0x00},
      {0x011F, 0x00, 0x14},
      {0x0117, 0xFF, 0xFF},
      {0x011F, 0x03, 0x00},
      {0x011F, 0x00, 0x0A},
      {0x011B, 0x04, 0x00},
      {0x011B, 0x00, 0x0F},
      {0x011F, 0xFF, 0xFF},
      {0x0007, 0x01, 0x00},
      {0x0007, 0x00, 0x05},
      {0x011B, 0x06, 0x00},
      {0x011B, 0x00, 0x14},
      {0x0117, 0x04, 0x00},
      {0x0117, 0x00, 0x0F},
  };
  constexpr size_t num_entries = sizeof(entries) / sizeof(entries[0]);

  const InterferenceProbabilityEntry* ret_entry = nullptr;
  int16_t current_max = -1;
  uint16_t current_row_card_id = 0xFFFF;
  for (size_t z = 0; z < num_entries; z++) {
    const auto& entry = entries[z];
    uint16_t current_column_card_id = entry.card_id;
    if ((entry.attack_probability != 0xFF) || (entry.defense_probability != 0xFF)) {
      if ((row_card_id == current_row_card_id) &&
          (column_card_id == current_column_card_id)) {
        uint8_t v = is_attack ? entry.attack_probability : entry.defense_probability;
        if (current_max <= v) {
          ret_entry = &entry;
          current_max = v;
        }
      }
    } else {
      current_row_card_id = current_column_card_id;
    }
  }

  return ret_entry;
}

void CardSpecial::on_card_destroyed(
    shared_ptr<Card> attacker_card, shared_ptr<Card> destroyed_card) {
  ActionState attack_as = this->create_attack_state_from_card_action_chain(attacker_card);
  ActionState defense_as = this->create_defense_state_for_card_pair_action_chains(
      attacker_card, destroyed_card);

  uint16_t destroyed_card_ref = destroyed_card->get_card_ref();
  this->unknown_8024C2B0(5, destroyed_card_ref, defense_as, 0xFFFF);
  for (size_t z = 0; (z < 8) && (defense_as.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_8024C2B0(
        5, defense_as.action_card_refs[z], defense_as, destroyed_card->get_card_ref());
  }

  if (attacker_card) {
    for (size_t cond_index = 0; cond_index < 9; cond_index++) {
      auto& cond = attacker_card->action_chain.conditions[cond_index];
      if (cond.type == ConditionType::CURSE) {
        this->execute_effect(cond, attacker_card, 0, 0, ConditionType::CURSE, 4, 0xFFFF);
      }
    }
  }
  this->send_6xB4x06_for_card_destroyed(destroyed_card, attack_as.attacker_card_ref);
}

vector<shared_ptr<const Card>> CardSpecial::find_cards_in_hp_range(
    int16_t min, int16_t max) const {
  vector<shared_ptr<const Card>> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      auto card = ps->get_sc_card();
      if (card) {
        int16_t hp = card->get_current_hp();
        if ((min <= hp) && (hp <= max)) {
          ret.emplace_back(card);
        }
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card) {
          int16_t hp = card->get_current_hp();
          if ((min <= hp) && (hp <= max)) {
            ret.emplace_back(card);
          }
        }
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_all_cards_by_aerial_attribute(bool is_aerial) const {
  vector<shared_ptr<const Card>> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card && (this->server()->ruler_server->card_ref_is_aerial(card->get_card_ref()) == is_aerial)) {
          ret.emplace_back(card);
        }
      }
      auto card = ps->get_sc_card();
      if (card && (this->server()->ruler_server->card_ref_is_aerial(card->get_card_ref()) == is_aerial)) {
        ret.emplace_back(card);
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_cards_damaged_by_at_least(int16_t damage) const {
  vector<shared_ptr<const Card>> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card && (damage + card->get_current_hp() <= card->get_max_hp())) {
          ret.emplace_back(card);
        }
      }
      auto card = ps->get_sc_card();
      if (card) {
        if (damage + card->get_current_hp() <= card->get_max_hp()) {
          ret.emplace_back(card);
        }
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_all_set_cards_on_client_team(uint8_t client_id) const {
  vector<shared_ptr<const Card>> ret;
  auto ps = this->server()->get_player_state(client_id);
  if (!ps) {
    return ret;
  }
  for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
    auto other_ps = this->server()->get_player_state(other_client_id);
    if (other_ps && (other_ps->get_team_id() == ps->get_team_id())) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = other_ps->get_set_card(set_index);
        if (card) {
          ret.emplace_back(card);
        }
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_all_cards_on_same_or_other_team(uint8_t client_id, bool same_team) const {
  vector<shared_ptr<const Card>> ret;
  auto ps = this->server()->get_player_state(client_id);
  if (!ps) {
    return ret;
  }

  for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
    auto other_ps = this->server()->get_player_state(other_client_id);
    if (other_ps) {
      bool should_collect = false;
      if (!same_team) {
        if ((other_ps->get_team_id() != 0xFF) && (ps->get_team_id() != other_ps->get_team_id())) {
          should_collect = true;
        }
      } else {
        if (ps->get_team_id() == other_ps->get_team_id()) {
          should_collect = true;
        }
      }
      if (should_collect) {
        auto card = other_ps->get_sc_card();
        if (card) {
          ret.emplace_back(card);
        }
        for (size_t set_index = 0; set_index < 8; set_index++) {
          auto card = other_ps->get_set_card(set_index);
          if (card) {
            ret.emplace_back(card);
          }
        }
      }
    }
  }
  return ret;
}

shared_ptr<const Card> CardSpecial::sc_card_for_client_id(uint8_t client_id) const {
  auto ps = this->server()->get_player_state(client_id);
  return ps ? ps->get_sc_card() : nullptr;
}

shared_ptr<const Card> CardSpecial::get_attacker_card(const ActionState& as) const {
  uint32_t card_ref = as.attacker_card_ref;
  if (card_ref == 0xFFFF) {
    card_ref = as.original_attacker_card_ref;
  }

  auto card = this->server()->card_for_set_card_ref(card_ref);
  if (card) {
    auto ce = card->get_definition();
    if ((ce->def.type == CardType::ITEM) || (ce->def.type == CardType::CREATURE)) {
      return card;
    }
  }
  return nullptr;
}

vector<shared_ptr<const Card>> CardSpecial::get_attacker_card_and_sc_if_item(const ActionState& as) const {
  vector<shared_ptr<const Card>> ret;
  uint16_t card_ref = as.attacker_card_ref;
  if (card_ref == 0xFFFF) {
    card_ref = as.original_attacker_card_ref;
  }
  auto card = this->server()->card_for_set_card_ref(card_ref);
  if (card) {
    auto ce = card->get_definition();
    if (ce->def.type == CardType::ITEM) {
      auto ps = card->player_state();
      if (ps) {
        ret.emplace_back(ps->get_sc_card());
      }
      ret.emplace_back(card);
    } else {
      if ((ce->def.type == CardType::HUNTERS_SC) ||
          (ce->def.type == CardType::ARKZ_SC) ||
          (ce->def.type == CardType::CREATURE)) {
        ret.emplace_back(card);
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::find_all_set_cards_with_cost_in_range(uint8_t min_cost, uint8_t max_cost) const {
  vector<shared_ptr<const Card>> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card) {
          auto ce = card->get_definition();
          if (ce && (min_cost <= ce->def.self_cost) && (ce->def.self_cost <= max_cost)) {
            ret.emplace_back(card);
          }
        }
      }
    }
  }
  return ret;
}

vector<shared_ptr<const Card>> CardSpecial::filter_cards_by_range(
    const vector<shared_ptr<const Card>>& cards,
    shared_ptr<const Card> card1,
    const Location& card1_loc,
    shared_ptr<const Card> card2) const {
  vector<shared_ptr<const Card>> ret;
  if (!card1 || cards.empty()) {
    return ret;
  }

  auto ps = card1->player_state();
  if (!ps) {
    return ret;
  }

  // TODO: Remove hardcoded card ID here (Earthquake)
  uint16_t card_id = this->get_card_id_with_effective_range(card1, 0x00ED, card2);
  parray<uint8_t, 9 * 9> range;
  compute_effective_range(range, this->server()->card_index, card_id, card1_loc, this->server()->map_and_rules1);
  auto card_refs_in_range = ps->get_card_refs_within_range_from_all_players(range, card1_loc, CardType::ITEM);

  for (auto card : cards) {
    if (!card || (card->get_card_ref() == 0xFFFF)) {
      continue;
    }
    for (uint16_t card_ref_in_range : card_refs_in_range) {
      if (card_ref_in_range == card->get_card_ref()) {
        ret.emplace_back(card);
        break;
      }
    }
  }
  return ret;
}

void CardSpecial::unknown_8024AAB8(const ActionState& as) {
  this->unknown_action_state_a1 = as;

  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(
        as.action_card_refs[z], 0x1E);
    if (card_ref == 0xFFFF) {
      break;
    }

    if (this->send_6xB4x06_if_card_ref_invalid(as.original_attacker_card_ref, 0x1F) == 0xFFFF) {
      this->unknown_8024C2B0(
          1,
          as.action_card_refs[z],
          as,
          this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x21));
      this->unknown_8024C2B0(
          0xb,
          as.action_card_refs[z],
          as,
          this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x22));
    } else {
      uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(as.target_card_refs[0], 0x20);
      if (card_ref != 0xFFFF) {
        this->unknown_8024C2B0(1, as.action_card_refs[z], as, card_ref);
        this->unknown_8024C2B0(0x15, as.action_card_refs[z], as, card_ref);
      }
    }
  }

  if (as.original_attacker_card_ref == 0xffff) {
    uint16_t card_ref1 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x23);
    uint16_t card_ref2 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x25);
    this->unknown_8024C2B0(0x33, card_ref2, as, card_ref1);
    card_ref1 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x24);
    card_ref2 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x26);
    this->unknown_8024C2B0(0x34, card_ref2, as, card_ref1);
    for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
      uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(
          as.action_card_refs[z], 0x27);
      if (card_ref == 0xFFFF) {
        break;
      }
      this->unknown_8024C2B0(0x35, as.target_card_refs[z], as, as.attacker_card_ref);
    }
  }
}

void CardSpecial::unknown_80244BE4(shared_ptr<Card> card) {
  ActionState as = this->create_attack_state_from_card_action_chain(card);
  this->apply_defense_conditions(as, 9, card, 4);
  this->unknown_8024C2B0(9, card->get_card_ref(), as, 0xFFFF);
  this->apply_defense_conditions(as, 0x27, card, 4);
  this->unknown_8024C2B0(0x27, card->get_card_ref(), as, 0xFFFF);
}

void CardSpecial::unknown_80244CA8(shared_ptr<Card> card) {
  ActionState as;
  auto ps = card->player_state();
  as.attacker_card_ref = card->get_card_ref();
  as.action_card_refs = card->action_chain.chain.attack_action_card_refs;
  as.target_card_refs = card->action_chain.chain.target_card_refs;

  uint16_t sc_card_ref = 0xFFFF;
  if (ps) {
    auto sc_card = ps->get_sc_card();
    if (sc_card) {
      sc_card_ref = sc_card->get_card_ref();
    }
  }

  this->apply_defense_conditions(as, 0x46, card, 4);
  this->unknown_8024C2B0(0x46, card->get_card_ref(), as, sc_card_ref);
  if (ps->is_team_turn()) {
    this->apply_defense_conditions(as, 4, card, 4);
    this->unknown_8024C2B0(4, card->get_card_ref(), as, sc_card_ref);
  }
}

template <uint8_t When1, uint8_t When2>
void CardSpecial::unknown1_t(
    shared_ptr<Card> unknown_p2, const ActionState* existing_as) {
  ActionState as;
  if (!existing_as) {
    as = this->create_attack_state_from_card_action_chain(unknown_p2);
  } else {
    as = *existing_as;
  }
  this->apply_defense_conditions(as, When1, unknown_p2, 4);
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->apply_defense_conditions(target_as, When1, card, 4);
    }
  }
  auto card = this->sc_card_for_card(unknown_p2);
  this->unknown_8024C2B0(When1, unknown_p2->get_card_ref(), as, card ? card->get_card_ref() : 0xFFFF);
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_8024C2B0(When1, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(
          unknown_p2, card);
      this->unknown_8024C2B0(When2, as.target_card_refs[z], target_as, unknown_p2->get_card_ref());
      for (size_t w = 0; (w < 8) && (target_as.action_card_refs[w] != 0xFFFF); w++) {
        this->unknown_8024C2B0(When1, target_as.action_card_refs[w], target_as, card->get_card_ref());
      }
    }
  }
}

void CardSpecial::unknown_80249060(shared_ptr<Card> unknown_p2) {
  this->unknown1_t<0x0F, 0x0A>(unknown_p2);
}

void CardSpecial::unknown_80249254(shared_ptr<Card> unknown_p2) {
  if (unknown_p2->player_state()->is_team_turn()) {
    this->unknown1_t<0x0E, 0x0A>(unknown_p2);
  }
}

void CardSpecial::unknown_8024945C(shared_ptr<Card> unknown_p2, const ActionState& existing_as) {
  this->unknown1_t<0x0A, 0x0A>(unknown_p2, &existing_as);
}

void CardSpecial::unknown_8024966C(shared_ptr<Card> unknown_p2, const ActionState* existing_as) {
  ActionState as;
  if (!existing_as) {
    as = this->create_attack_state_from_card_action_chain(unknown_p2);
  } else {
    as = *existing_as;
  }

  auto card = this->sc_card_for_card(unknown_p2);
  uint16_t card_ref = card ? card->get_card_ref() : 0xFFFF;

  auto ce = unknown_p2->get_definition();
  auto defender_card = (ce && (ce->def.type == CardType::ITEM) && card) ? card : unknown_p2;

  this->apply_defense_conditions(as, 0x3D, unknown_p2, 4);
  this->apply_defense_conditions(as, 0x3E, unknown_p2, 4);
  if (defender_card) {
    this->apply_defense_conditions(as, 0x22, defender_card, 4);
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState defense_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->apply_defense_conditions(defense_as, 0x3D, card, 4);
      this->apply_defense_conditions(defense_as, 0x3F, card, 4);
    }
  }

  this->unknown_8024C2B0(0x3D, unknown_p2->get_card_ref(), as, card_ref);
  this->unknown_8024C2B0(0x3E, unknown_p2->get_card_ref(), as, card_ref);
  if (defender_card) {
    this->unknown_8024C2B0(0x22, defender_card->get_card_ref(), as, card_ref);
  }

  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_8024C2B0(0x3D, as.action_card_refs[z], as, unknown_p2->get_card_ref());
    this->unknown_8024C2B0(0x3E, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState defense_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->unknown_8024C2B0(0x3D, card->get_card_ref(), defense_as, unknown_p2->get_card_ref());
      this->unknown_8024C2B0(0x3F, card->get_card_ref(), defense_as, unknown_p2->get_card_ref());
    }
  }
}

shared_ptr<Card> CardSpecial::sc_card_for_card(shared_ptr<Card> unknown_p2) {
  auto ps = unknown_p2->player_state();
  return ps ? ps->get_sc_card() : nullptr;
}

void CardSpecial::unknown_8024A9D8(const ActionState& pa, uint16_t action_card_ref) {
  for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    if ((action_card_ref == 0xFFFF) || (action_card_ref == pa.action_card_refs[z])) {
      if (pa.original_attacker_card_ref == 0xFFFF) {
        this->unknown_8024C2B0(0x29, pa.action_card_refs[z], pa, pa.attacker_card_ref);
        this->unknown_8024C2B0(0x2A, pa.action_card_refs[z], pa, pa.attacker_card_ref);
      } else {
        this->unknown_8024C2B0(0x29, pa.action_card_refs[z], pa, pa.target_card_refs[0]);
        this->unknown_8024C2B0(0x2B, pa.action_card_refs[z], pa, pa.target_card_refs[0]);
      }
    }
  }
}

void CardSpecial::check_for_attack_interference(shared_ptr<Card> unknown_p2) {
  // Note: This check is not part of the original implementation.
  if (this->server()->behavior_flags & BehaviorFlag::DISABLE_INTERFERENCE) {
    return;
  }

  if (unknown_p2->action_chain.chain.damage <= 0) {
    return;
  }

  uint16_t ally_sc_card_ref = this->server()->ruler_server->get_ally_sc_card_ref(
      unknown_p2->get_card_ref());
  if (ally_sc_card_ref == 0xFFFF) {
    return;
  }

  uint8_t ally_client_id = client_id_for_card_ref(ally_sc_card_ref);
  if (ally_client_id == 0xFF) {
    return;
  }
  auto ally_sc_card = this->server()->card_for_set_card_ref(ally_sc_card_ref);
  if (!ally_sc_card || (ally_sc_card->card_flags & 2)) {
    return;
  }

  uint8_t client_id = client_id_for_card_ref(unknown_p2->get_card_ref());
  if (client_id == 0xFF) {
    return;
  }

  auto ally_hes = this->server()->ruler_server->get_hand_and_equip_state_for_client_id(ally_client_id);
  if (!ally_hes || !ally_hes->is_cpu_player) {
    return;
  }

  this->server()->ruler_server->get_hand_and_equip_state_for_client_id(client_id);
  auto ps = unknown_p2->player_state();
  if (!ps || (ps->unknown_a16 >= 1)) {
    return;
  }

  uint16_t card_ref = unknown_p2->get_card_ref();
  if ((unknown_p2->get_definition()->def.type == CardType::ITEM) && ps->get_sc_card()) {
    card_ref = ps->get_sc_card()->get_card_ref();
  }

  uint16_t row_card_id = this->server()->card_id_for_card_ref(card_ref);
  if (row_card_id == 0xFFFF) {
    return;
  }

  uint16_t ally_sc_card_id = this->server()->card_id_for_card_ref(ally_sc_card_ref);
  if (ally_sc_card_id == 0xFFFF) {
    return;
  }

  const auto* entry = get_interference_probability_entry(
      row_card_id, ally_sc_card_id, true);
  if (!entry || (this->server()->get_random(99) >= entry->attack_probability)) {
    return;
  }

  ps->unknown_a16++;
  unknown_p2->action_chain.set_flags(0x100);

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
      unknown_p2->get_card_ref(), 0x11);
  cmd.effect.target_card_ref = unknown_p2->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = 0x7D;
  this->server()->send(cmd);
}

template <uint8_t When1, uint8_t When2, uint8_t When3, uint8_t When4>
void CardSpecial::unknown_t2(shared_ptr<Card> unknown_p2) {
  ActionState as = this->create_attack_state_from_card_action_chain(unknown_p2);

  auto sc_card = this->sc_card_for_card(unknown_p2);
  uint16_t sc_card_ref = 0xFFFF;
  if (sc_card) {
    sc_card_ref = sc_card->get_card_ref();
  }

  auto defender_card = unknown_p2;
  if (unknown_p2->get_definition() &&
      (unknown_p2->get_definition()->def.type == CardType::ITEM) &&
      sc_card) {
    defender_card = sc_card;
  }

  this->apply_defense_conditions(as, When1, unknown_p2, 4);
  this->apply_defense_conditions(as, When2, unknown_p2, 4);
  if (defender_card) {
    this->apply_defense_conditions(as, When3, defender_card, 4);
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto set_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (set_card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(
          unknown_p2, set_card);
      this->apply_defense_conditions(target_as, When1, set_card, 4);
      this->apply_defense_conditions(target_as, When4, set_card, 4);
    }
  }

  this->unknown_8024C2B0(When1, unknown_p2->get_card_ref(), as, sc_card_ref);
  this->unknown_8024C2B0(When2, unknown_p2->get_card_ref(), as, sc_card_ref);
  if (defender_card) {
    this->unknown_8024C2B0(When3, defender_card->get_card_ref(), as, sc_card_ref);
  }
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_8024C2B0(When1, as.action_card_refs[z], as, unknown_p2->get_card_ref());
    this->unknown_8024C2B0(When2, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto set_card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (set_card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, set_card);
      this->unknown_8024C2B0(When1, set_card->get_card_ref(), target_as, unknown_p2->get_card_ref());
      this->unknown_8024C2B0(When4, set_card->get_card_ref(), target_as, unknown_p2->get_card_ref());
      for (size_t z = 0; (z < 8) && (target_as.action_card_refs[z] != 0xFFFF); z++) {
        this->unknown_8024C2B0(When1, target_as.action_card_refs[z], target_as, set_card->get_card_ref());
        this->unknown_8024C2B0(When4, target_as.action_card_refs[z], target_as, set_card->get_card_ref());
      }
    }
  }
}

void CardSpecial::unknown_8024997C(shared_ptr<Card> card) {
  return this->unknown_t2<0x03, 0x0D, 0x21, 0x17>(card);
}

void CardSpecial::unknown_8024A394(shared_ptr<Card> card) {
  return this->unknown_t2<0x02, 0x0C, 0x20, 0x16>(card);
}

bool CardSpecial::client_has_atk_dice_boost_condition(uint8_t client_id) {
  auto ps = this->server()->get_player_state(client_id);
  if (ps) {
    auto card = ps->get_sc_card();
    if (card) {
      for (size_t z = 0; z < 9; z++) {
        if (!this->card_ref_has_ability_trap(card->action_chain.conditions[z]) &&
            (card->action_chain.conditions[z].type == ConditionType::ATK_DICE_BOOST)) {
          return true;
        }
      }
    }
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = ps->get_set_card(set_index);
      if (card) {
        for (size_t z = 0; z < 9; z++) {
          if (!this->card_ref_has_ability_trap(card->action_chain.conditions[z]) &&
              (card->action_chain.conditions[z].type == ConditionType::ATK_DICE_BOOST)) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void CardSpecial::unknown_8024A6DC(
    shared_ptr<Card> unknown_p2, shared_ptr<Card> unknown_p3) {
  ActionState as = this->create_defense_state_for_card_pair_action_chains(
      unknown_p2, unknown_p3);
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->unknown_8024C2B0(1, as.action_card_refs[z], as, unknown_p3->get_card_ref());
    this->unknown_8024C2B0(0x15, as.action_card_refs[z], as, unknown_p3->get_card_ref());
  }
}

vector<shared_ptr<const Card>> CardSpecial::find_all_sc_cards_of_class(
    CardClass card_class) const {
  vector<shared_ptr<const Card>> ret;
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->server()->get_player_state(z);
    if (ps) {
      auto sc_card = ps->get_sc_card();
      if (sc_card && (sc_card->get_definition()->def.card_class() == card_class)) {
        ret.emplace_back(sc_card);
      }
    }
  }
  return ret;
}

} // namespace Episode3
