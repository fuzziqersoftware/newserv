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
    ret += phosg::string_printf("@%04hX", ref_for_card(card));
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
  this->non_target_team_num_set_cards = 0;
  this->num_native_creatures = 0;
  this->num_a_beast_creatures = 0;
  this->num_machine_creatures = 0;
  this->num_dark_creatures = 0;
  this->num_sword_type_items = 0;
  this->num_gun_type_items = 0;
  this->num_cane_type_items = 0;
  this->effective_ap_if_not_tech2 = 0;
  this->team_dice_bonus = 0;
  this->sc_effective_ap = 0;
  this->attack_bonus = 0;
  this->num_sword_type_items_on_team = 0;
  this->target_attack_bonus = 0;
  this->last_attack_preliminary_damage = 0;
  this->last_attack_damage = 0;
  this->final_last_attack_damage = 0;
  this->last_attack_damage_count = 0;
  this->target_current_hp = 0;
}
uint32_t CardSpecial::AttackEnvStats::at(size_t index) const {
  static_assert(sizeof(parray<uint32_t, 39>) == sizeof(AttackEnvStats), "CardSpecial::AttackEnvStats does not have exactly 39 entries");
  return reinterpret_cast<const parray<uint32_t, 39>*>(this)->at(index);
}

void CardSpecial::AttackEnvStats::print(FILE* stream) const {
  fprintf(stream, "(a)   total_num_set_cards                = %" PRIu32 "\n", this->total_num_set_cards);
  fprintf(stream, "(ab)  num_a_beast_creatures              = %" PRIu32 "\n", this->num_a_beast_creatures);
  fprintf(stream, "(ac)  player_num_atk_points              = %" PRIu32 "\n", this->player_num_atk_points);
  fprintf(stream, "(adm) sc_effective_ap                    = %" PRIu32 "\n", this->sc_effective_ap);
  fprintf(stream, "(ap)  effective_ap                       = %" PRIu32 "\n", this->effective_ap);
  fprintf(stream, "(bi)  num_native_creatures               = %" PRIu32 "\n", this->num_native_creatures);
  fprintf(stream, "(cs)  card_cost                          = %" PRIu32 "\n", this->card_cost);
  fprintf(stream, "(d)   dice_roll_value1                   = %" PRIu32 "\n", this->dice_roll_value1);
  fprintf(stream, "(dc)  dice_roll_value2                   = %" PRIu32 "\n", this->dice_roll_value2);
  fprintf(stream, "(ddm) attack_bonus                       = %" PRIu32 "\n", this->attack_bonus);
  fprintf(stream, "(df)  num_destroyed_ally_fcs             = %" PRIu32 "\n", this->num_destroyed_ally_fcs);
  fprintf(stream, "(dk)  num_dark_creatures                 = %" PRIu32 "\n", this->num_dark_creatures);
  fprintf(stream, "(dm)  effective_ap_if_not_tech           = %" PRIu32 "\n", this->effective_ap_if_not_tech);
  fprintf(stream, "(dn)  unknown_a1                         = %" PRIu32 "\n", this->unknown_a1);
  fprintf(stream, "(edm) target_attack_bonus                = %" PRIu32 "\n", this->target_attack_bonus);
  fprintf(stream, "(ef)  non_target_team_num_set_cards      = %" PRIu32 "\n", this->non_target_team_num_set_cards);
  fprintf(stream, "(ehp) target_current_hp                  = %" PRIu32 "\n", this->target_current_hp);
  fprintf(stream, "(f)   num_set_cards                      = %" PRIu32 "\n", this->num_set_cards);
  fprintf(stream, "(fdm) final_last_attack_damage           = %" PRIu32 "\n", this->final_last_attack_damage);
  fprintf(stream, "(ff)  target_team_num_set_cards          = %" PRIu32 "\n", this->target_team_num_set_cards);
  fprintf(stream, "(gn)  num_gun_type_items                 = %" PRIu32 "\n", this->num_gun_type_items);
  fprintf(stream, "(hf)  num_item_or_creature_cards_in_hand = %" PRIu32 "\n", this->num_item_or_creature_cards_in_hand);
  fprintf(stream, "(hp)  current_hp                         = %" PRIu32 "\n", this->current_hp);
  fprintf(stream, "(kap) action_cards_ap                    = %" PRIu32 "\n", this->action_cards_ap);
  fprintf(stream, "(ktp) action_cards_tp                    = %" PRIu32 "\n", this->action_cards_tp);
  fprintf(stream, "(ldm) last_attack_preliminary_damage     = %" PRIu32 "\n", this->last_attack_preliminary_damage);
  fprintf(stream, "(lv)  team_dice_bonus                    = %" PRIu32 "\n", this->team_dice_bonus);
  fprintf(stream, "(mc)  num_machine_creatures              = %" PRIu32 "\n", this->num_machine_creatures);
  fprintf(stream, "(mhp) max_hp                             = %" PRIu32 "\n", this->max_hp);
  fprintf(stream, "(ndm) last_attack_damage_count           = %" PRIu32 "\n", this->last_attack_damage_count);
  fprintf(stream, "(php) defined_max_hp                     = %" PRIu32 "\n", this->defined_max_hp);
  fprintf(stream, "(rdm) last_attack_damage                 = %" PRIu32 "\n", this->last_attack_damage);
  fprintf(stream, "(sa)  num_sword_type_items               = %" PRIu32 "\n", this->num_sword_type_items);
  fprintf(stream, "(sat) num_sword_type_items_on_team       = %" PRIu32 "\n", this->num_sword_type_items_on_team);
  fprintf(stream, "(tdm) effective_ap_if_not_physical       = %" PRIu32 "\n", this->effective_ap_if_not_physical);
  fprintf(stream, "(tf)  player_num_destroyed_fcs           = %" PRIu32 "\n", this->player_num_destroyed_fcs);
  fprintf(stream, "(tp)  effective_tp                       = %" PRIu32 "\n", this->effective_tp);
  fprintf(stream, "(tt)  effective_ap_if_not_tech2          = %" PRIu32 "\n", this->effective_ap_if_not_tech2);
  fprintf(stream, "(wd)  num_cane_type_items                = %" PRIu32 "\n", this->num_cane_type_items);
}

CardSpecial::CardSpecial(shared_ptr<Server> server) : w_server(server) {}

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
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  shared_ptr<const Card> attacker_card = s->card_for_set_card_ref(attacker_card_ref);
  auto attack_medium = attacker_card ? attacker_card->action_chain.chain.attack_medium : AttackMedium::UNKNOWN;

  for (size_t z = 0; z < 9; z++) {
    const auto& cond = target_card->action_chain.conditions[z];
    if (cond.type == ConditionType::NONE) {
      continue;
    }
    if (!is_nte && this->card_ref_has_ability_trap(cond)) {
      continue;
    }

    if (!s->ruler_server->check_usability_or_apply_condition_for_card_refs(
            cond.card_ref,
            target_card->get_card_ref(),
            attacker_card_ref,
            cond.card_definition_effect_index,
            attack_medium)) {
      continue;
    }

    switch (cond.type) {
      case ConditionType::WEAK_HIT_BLOCK:
        if (!is_nte && (*inout_damage <= cond.value)) {
          *inout_damage = 0;
        }
        break;

      case ConditionType::EXP_DECOY: {
        auto target_ps = target_card->player_state();
        if (target_ps) {
          uint8_t target_team_id = target_ps->get_team_id();
          int16_t exp_deduction = s->team_exp[target_team_id];
          if (exp_deduction < *inout_damage) {
            *inout_damage = *inout_damage - exp_deduction;
            s->team_exp[target_team_id] = 0;
          } else {
            s->team_exp[target_team_id] = exp_deduction - *inout_damage;
            exp_deduction = *inout_damage;
            *inout_damage = 0;
          }
          if (!is_nte) {
            this->send_6xB4x06_for_exp_change(target_card, attacker_card_ref, -exp_deduction, true);
          }
          this->compute_team_dice_bonus(target_team_id);
        }
        break;
      }

      case ConditionType::UNKNOWN_73:
        if (!is_nte && (cond.value <= *inout_damage)) {
          *inout_damage = 0;
        }
        break;

      case ConditionType::HALFGUARD:
        if (!is_nte && (cond.value <= *inout_damage)) {
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
    if ((this->server()->options.is_nte() || !this->card_ref_has_ability_trap(card->action_chain.conditions[z])) &&
        (card->action_chain.conditions[z].type == ConditionType::UNKNOWN_52)) {
      *inout_dice_boost = *inout_dice_boost * card->action_chain.conditions[z].value8;
    }
  }
}

void CardSpecial::apply_action_conditions(
    EffectWhen when,
    shared_ptr<const Card> attacker_card,
    shared_ptr<Card> defender_card,
    uint32_t flags,
    const ActionState* as) {
  auto s = this->server();
  auto log = s->log_stack("apply_action_conditions: ");

  ActionState temp_as;
  if (attacker_card == defender_card) {
    temp_as = this->create_attack_state_from_card_action_chain(attacker_card);
    if (as) {
      log.debug("using action state from override");
      temp_as = *as;
    } else {
      log.debug("using action state from attacker card");
    }
  } else {
    temp_as = this->create_defense_state_for_card_pair_action_chains(attacker_card, defender_card);
    log.debug("using action state from card pair");
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
    EffectWhen when,
    Condition* defender_cond,
    uint8_t cond_index,
    const ActionState& defense_state,
    shared_ptr<Card> defender_card,
    uint32_t flags,
    bool unknown_p8) {
  auto s = this->server();
  bool is_nte = s->options.is_nte();
  auto log = s->log_stack("apply_defense_condition: ");

  if (log.should_log(phosg::LogLevel::DEBUG)) {
    log.debug(
        "when=%s, cond_index=%hhu, defender_card=(@%04hX #%04hX), flags=%08" PRIX32 ", p8=%s",
        phosg::name_for_enum(when),
        cond_index,
        defender_card->get_card_ref(),
        defender_card->get_card_id(),
        flags,
        unknown_p8 ? "true" : "false");
    auto defender_cond_str = defender_cond->str(s);
    auto defense_state_str = defense_state.str(s);
    log.debug("defender_cond = %s", defender_cond_str.c_str());
    log.debug("defense_state = %s", defense_state_str.c_str());
  }

  if (defender_cond->type == ConditionType::NONE) {
    log.debug("no condition");
    return false;
  }

  auto orig_eff = this->original_definition_for_condition(*defender_cond);
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    auto orig_eff_str = orig_eff->str();
    log.debug("orig_eff = %s", orig_eff_str.c_str());
  }

  uint16_t attacker_card_ref = defense_state.attacker_card_ref;
  if (attacker_card_ref == 0xFFFF) {
    attacker_card_ref = defense_state.original_attacker_card_ref;
  }
  log.debug("attacker_card_ref = @%04hX", attacker_card_ref);

  bool defender_has_ability_trap = !is_nte && this->card_ref_has_ability_trap(*defender_cond);
  log.debug("defender_has_ability_trap = %s", defender_has_ability_trap ? "true" : "false");

  if ((is_nte || (flags & 4)) && !this->is_card_targeted_by_condition(*defender_cond, defense_state, defender_card)) {
    log.debug("not targeted by condition");
    if (defender_cond->type != ConditionType::NONE) {
      G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x04;
      cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x0D);
      cmd.effect.target_card_ref = defender_card->get_card_ref();
      cmd.effect.value = 0;
      cmd.effect.operation = -static_cast<int8_t>(defender_cond->type);
      s->send(cmd);
    }
    this->apply_stat_deltas_to_card_from_condition_and_clear_cond(*defender_cond, defender_card);
    defender_card->send_6xB4x4E_4C_4D_if_needed();
    return false;
  }

  if ((when == EffectWhen::AFTER_ANY_CARD_ATTACK) && (defender_cond->type == ConditionType::GUOM) && (flags & 4)) {
    log.debug("deleting guom condition");
    CardShortStatus stat = defender_card->get_short_status();
    if (stat.card_flags & 4) {
      G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x04;
      cmd.effect.attacker_card_ref = attacker_card_ref;
      cmd.effect.target_card_ref = defender_card->get_card_ref();
      cmd.effect.value = 0;
      cmd.effect.operation = -static_cast<int8_t>(defender_cond->type);
      cmd.effect.condition_index = cond_index;
      s->send(cmd);
      this->apply_stat_deltas_to_card_from_condition_and_clear_cond(*defender_cond, defender_card);
      defender_card->send_6xB4x4E_4C_4D_if_needed();
      return false;
    }
  }

  if (s->options.is_nte()) {
    auto defender_ps = defender_card->player_state();
    if ((when == EffectWhen::BEFORE_DRAW_PHASE) && (flags & 4) && (defender_cond->type == ConditionType::DROP) && defender_ps) {
      auto defender_sc_card = defender_ps->get_sc_card();
      uint8_t defender_team_id = defender_ps->get_team_id();
      if (defender_sc_card && s->team_exp[defender_team_id]) {
        G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
        cmd.effect.flags = 0x04;
        cmd.effect.attacker_card_ref = defender_cond->card_ref;
        cmd.effect.target_card_ref = defender_sc_card->get_card_ref();
        cmd.effect.value = 0;
        cmd.effect.operation = 0x2E;
        s->send(cmd);
      }
      s->team_exp[defender_team_id] = max<int16_t>(s->team_exp[defender_team_id] - 3, 0);
      this->compute_team_dice_bonus(defender_team_id);
    }
  }

  if ((when == EffectWhen::BEFORE_DICE_PHASE_THIS_TEAM_TURN) && (flags & 4) && !defender_has_ability_trap && (defender_cond->type == ConditionType::ACID)) {
    log.debug("applying acid");
    int16_t hp = defender_card->get_current_hp();
    if (hp > 0) {
      this->send_6xB4x06_for_stat_delta(defender_card, defender_cond->card_ref, 0x20, -1, 0, 1);
      defender_card->set_current_hp(hp - 1);
      this->destroy_card_if_hp_zero(defender_card, defender_cond->condition_giver_card_ref);
    }
  }

  if (!orig_eff || (orig_eff->when != when)) {
    log.debug("unsetting flag 4");
    flags &= ~4;
  }
  if ((flags == 0) || defender_has_ability_trap) {
    log.debug("no condition remains to apply");
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

  string expr = orig_eff->expr.decode();
  int16_t expr_value = this->evaluate_effect_expr(astats, expr.c_str(), dice_roll);
  log.debug("execute_effect ...");
  this->execute_effect(*defender_cond, defender_card, expr_value, defender_cond->value, orig_eff->type, flags, attacker_card_ref);
  if (flags & 4) {
    log.debug("recomputing action chaing results");
    if (is_nte || !(defender_card->card_flags & 2)) {
      defender_card->compute_action_chain_results(true, false);
    }
    defender_card->action_chain.chain.card_ap = defender_card->ap;
    defender_card->action_chain.chain.card_tp = defender_card->tp;
    defender_card->send_6xB4x4E_4C_4D_if_needed();
  }

  if (dice_roll.value_used_in_expr && !(original_cond_flags & 1) && !unknown_p8) {
    log.debug("dice roll was used; setting dice display flag");
    defender_cond->flags |= 1;
    G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
    cmd.effect.flags = 0x08;
    cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x10);
    cmd.effect.target_card_ref = defender_cond->card_ref;
    cmd.effect.dice_roll_value = dice_roll.value;
    s->send(cmd);
  }

  return true;
}

bool CardSpecial::apply_defense_conditions(
    const ActionState& as,
    EffectWhen when,
    shared_ptr<Card> defender_card,
    uint32_t flags) {
  for (size_t z = 0; z < 9; z++) {
    this->apply_defense_condition(when, &defender_card->action_chain.conditions[z], z, as, defender_card, flags, 0);
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

bool CardSpecial::apply_stat_deltas_to_card_from_condition_and_clear_cond(Condition& cond, shared_ptr<Card> card) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("apply_stat_deltas_to_card_from_condition_and_clear_cond(@%04hX #%04hX): ", card->get_card_ref(), card->get_card_id()));
  bool is_nte = s->options.is_nte();

  string cond_str = cond.str(s);
  log.debug("cond: %s", cond_str.c_str());

  ConditionType cond_type = cond.type;
  int16_t cond_value = is_nte ? cond.value.load() : clamp<int16_t>(cond.value, -99, 99);
  uint8_t cond_flags = cond.flags;
  uint16_t cond_card_ref = card->get_card_ref();
  cond.clear();

  switch (cond_type) {
    case ConditionType::A_T_SWAP_0C:
      if (cond_flags & 2) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t tp = clamp<int16_t>(card->tp, -99, 99);
        log.debug("A_T_SWAP_0C: swapping AP (%hd) and TP (%hd)", ap, tp);
        if (!is_nte) {
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, tp - ap, 0, 0);
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, ap - tp, 0, 0);
        }
        card->ap = tp;
        card->tp = ap;
      } else {
        log.debug("A_T_SWAP_0C: required flag is missing");
      }
      break;
    case ConditionType::A_H_SWAP:
      if (cond_flags & 2) {
        int16_t ap = clamp<int16_t>(card->ap, -99, 99);
        int16_t hp = clamp<int16_t>(card->get_current_hp(), -99, 99);
        if (hp != ap) {
          log.debug("A_H_SWAP: swapping AP (%hd) and HP (%hd)", ap, hp);
          if (!is_nte) {
            this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, hp - ap, 0, 0);
            this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x20, ap - hp, 0, 0);
          }
          card->set_current_hp(ap, true, true);
          card->ap = hp;
          this->destroy_card_if_hp_zero(card, cond_card_ref);
        } else {
          log.debug("A_H_SWAP: AP (%hd) == HP (%hd)", ap, hp);
        }
      } else {
        log.debug("A_H_SWAP: required flag is missing");
      }
      break;
    case ConditionType::AP_OVERRIDE:
      if (cond_flags & 2) {
        // Note: In NTE, this case behaves intuitively, but in non-NTE, it seems
        // that find_condition was changed to always return null. Perhaps this
        // was an accident, or perhaps not, but we implement both behaviors.
        Condition* other_cond = is_nte ? card->find_condition(ConditionType::AP_OVERRIDE) : nullptr;
        if (!other_cond) {
          if (!is_nte) {
            this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, -cond_value, 0, 0);
          }
          card->ap = max<int16_t>(card->ap - cond_value, 0);
          log.debug("AP_OVERRIDE: subtracting %hd from AP => %hd", cond_value, card->ap);
        } else {
          other_cond->value = clamp<int16_t>(other_cond->value + cond_value, -99, 99);
        }
      } else {
        log.debug("AP_OVERRIDE: required flag is missing");
      }
      break;
    case ConditionType::TP_OVERRIDE:
      if (cond_flags & 2) {
        // See note in the AP_OVERRIDE case about why non-NTE always uses null.
        Condition* other_cond = is_nte ? card->find_condition(ConditionType::TP_OVERRIDE) : nullptr;
        if (!other_cond) {
          if (!is_nte) {
            this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, -cond_value, 0, 0);
          }
          card->tp = max<int16_t>(card->tp - cond_value, 0);
          log.debug("TP_OVERRIDE: subtracting %hd from TP => %hd", cond_value, card->tp);
        } else {
          other_cond->value = clamp<int16_t>(other_cond->value + cond_value, -99, 99);
        }
      } else {
        log.debug("TP_OVERRIDE: required flag is missing");
      }
      break;
    case ConditionType::MISC_AP_BONUSES:
      if (cond_flags & 2) {
        if (!is_nte) {
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, -cond_value, 0, 0);
        }
        card->ap = max<int16_t>(card->ap - cond_value, 0);
        log.debug("MISC_AP_BONUSES: subtracting %hd from AP => %hd", cond_value, card->ap);
      } else {
        log.debug("MISC_AP_BONUSES: required flag is missing");
      }
      break;
    case ConditionType::MISC_TP_BONUSES:
      if (cond_flags & 2) {
        if (!is_nte) {
          this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, -cond_value, 0, 0);
        }
        card->tp = max<int16_t>(card->tp - cond_value, 0);
        log.debug("MISC_TP_BONUSES: subtracting %hd from TP => %hd", cond_value, card->tp);
      } else {
        log.debug("MISC_TP_BONUSES: required flag is missing");
      }
      break;
    case ConditionType::AP_SILENCE:
      if (is_nte) {
        goto trial_unimplemented;
      }
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0xA0, cond_value, 0, 0);
        card->ap = max<int16_t>(card->ap + cond_value, 0);
        log.debug("AP_SILENCE: adding %hd to AP => %hd", cond_value, card->ap);
      } else {
        log.debug("AP_SILENCE: required flag is missing");
      }
      break;
    case ConditionType::TP_SILENCE:
      if (is_nte) {
        goto trial_unimplemented;
      }
      if (cond_flags & 2) {
        this->send_6xB4x06_for_stat_delta(card, cond_card_ref, 0x80, cond_value, 0, 0);
        card->tp = max<int16_t>(card->tp + cond_value, 0);
        log.debug("TP_SILENCE: adding %hd to TP => %hd", cond_value, card->tp);
      } else {
        log.debug("TP_SILENCE: required flag is missing");
      }
      break;
    trial_unimplemented:
    default:
      log.debug("%s: no adjustments for condition type", phosg::name_for_enum(cond_type));
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
      ret |= this->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, card);
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
  auto s = this->server();
  auto is_nte = s->options.is_nte();

  auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
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
          (!is_nte && this->card_ref_has_ability_trap(cond)) ||
          !s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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
    auto ps = s->get_player_state(client_id);
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        check_card(ps->get_set_card(set_index));
      }
      check_card(ps->get_sc_card());
    }
  }

  if (!is_nte) {
    if (attacker_card && attacker_card->get_condition_value(ConditionType::UNKNOWN_7D)) {
      *out_value = *out_value * 1.5f;
    }
    if (target_card && target_card->get_condition_value(ConditionType::UNKNOWN_7D)) {
      *out_value = 0;
    }
  }
}

CardSpecial::AttackEnvStats CardSpecial::compute_attack_env_stats(
    const ActionState& pa,
    shared_ptr<const Card> card,
    const DiceRoll& dice_roll,
    uint16_t target_card_ref,
    uint16_t condition_giver_card_ref) {
  auto s = this->server();
  auto log = s->log_stack("compute_attack_env_stats: ");
  bool is_nte = s->options.is_nte();

  string pa_str = pa.str(s);
  log.debug("pa=%s, card=@%04hX #%04hX, dice_roll=%hhu, target=@%04hX, condition_giver=@%04hX", pa_str.c_str(), card->get_card_ref(), card->get_card_id(), dice_roll.value, target_card_ref, condition_giver_card_ref);

  auto attacker_card = s->card_for_set_card_ref(pa.attacker_card_ref);
  if (!attacker_card && (pa.original_attacker_card_ref != 0xFFFF)) {
    attacker_card = s->card_for_set_card_ref(pa.original_attacker_card_ref);
    log.debug("attacker=@%04hX #%04hX (from original)", attacker_card->get_card_ref(), attacker_card->get_card_id());
  } else if (attacker_card) {
    log.debug("attacker=@%04hX #%04hX (from set)", attacker_card->get_card_ref(), attacker_card->get_card_id());
  } else {
    log.debug("attacker=null (from set)");
  }

  AttackEnvStats ast;

  auto ps = card->player_state();
  log.debug("base ps = %hhu", ps->client_id);
  ast.num_set_cards = is_nte ? ps->count_set_cards_for_env_stats_nte() : ps->count_set_cards();
  auto condition_giver_card = s->card_for_set_card_ref(condition_giver_card_ref);
  auto target_card = s->card_for_set_card_ref(target_card_ref);
  if (!target_card) {
    target_card = condition_giver_card;
  }

  size_t ps_num_set_cards = 0;
  for (size_t z = 0; z < 4; z++) {
    auto other_ps = s->get_player_state(z);
    if (other_ps) {
      ps_num_set_cards += is_nte ? other_ps->count_set_cards_for_env_stats_nte() : other_ps->count_set_cards();
    }
  }
  ast.total_num_set_cards = ps_num_set_cards;

  uint8_t target_card_team_id = target_card
      ? target_card->player_state()->get_team_id()
      : 0xFF;

  size_t target_team_num_set_cards = 0;
  size_t non_target_team_num_set_cards = 0;
  for (size_t z = 0; z < 4; z++) {
    auto other_ps = s->get_player_state(z);
    if (other_ps) {
      if (target_card_team_id == other_ps->get_team_id()) {
        target_team_num_set_cards += is_nte ? other_ps->count_set_cards_for_env_stats_nte() : other_ps->count_set_cards();
      } else {
        non_target_team_num_set_cards += is_nte ? other_ps->count_set_cards_for_env_stats_nte() : other_ps->count_set_cards();
      }
    }
  }
  ast.target_team_num_set_cards = target_team_num_set_cards;
  ast.non_target_team_num_set_cards = non_target_team_num_set_cards;

  ast.num_native_creatures = this->get_all_set_cards_by_team_and_class(CardClass::NATIVE_CREATURE, 0xFF, true).size();
  ast.num_a_beast_creatures = this->get_all_set_cards_by_team_and_class(CardClass::A_BEAST_CREATURE, 0xFF, true).size();
  ast.num_machine_creatures = this->get_all_set_cards_by_team_and_class(CardClass::MACHINE_CREATURE, 0xFF, true).size();
  ast.num_dark_creatures = this->get_all_set_cards_by_team_and_class(CardClass::DARK_CREATURE, 0xFF, true).size();
  ast.num_sword_type_items = this->get_all_set_cards_by_team_and_class(CardClass::SWORD_ITEM, 0xFF, true).size();
  ast.num_gun_type_items = this->get_all_set_cards_by_team_and_class(CardClass::GUN_ITEM, 0xFF, true).size();
  ast.num_cane_type_items = this->get_all_set_cards_by_team_and_class(CardClass::CANE_ITEM, 0xFF, true).size();
  ast.num_sword_type_items_on_team = card ? this->get_all_set_cards_by_team_and_class(CardClass::SWORD_ITEM, card->get_team_id(), true).size() : 0;

  size_t num_item_or_creature_cards_in_hand = 0;
  for (size_t z = 0; z < 6; z++) {
    uint16_t card_ref = ps->card_ref_for_hand_index(z);
    if (card_ref == 0xFFFF) {
      continue;
    }
    auto ce = s->definition_for_card_id(card_ref);
    if (ce && ((ce->def.type == CardType::ITEM) || (ce->def.type == CardType::CREATURE))) {
      num_item_or_creature_cards_in_hand++;
    }
  }
  ast.num_item_or_creature_cards_in_hand = num_item_or_creature_cards_in_hand;

  if (is_nte) {
    ast.num_destroyed_ally_fcs = s->team_num_cards_destroyed[ps->get_team_id()] - card->num_ally_fcs_destroyed_at_set_time;
  } else {
    ast.num_destroyed_ally_fcs = card->num_destroyed_ally_fcs;
  }

  // Note: The original implementation has dice_roll as optional, but since it's
  // provided at all callsites, we require it (and hence don't check for nullptr
  // here)
  ast.dice_roll_value1 = dice_roll.value;
  ast.dice_roll_value2 = dice_roll.value;
  ast.effective_ap = card->action_chain.chain.effective_ap;
  ast.effective_tp = card->action_chain.chain.effective_tp;
  ast.current_hp = card->get_current_hp();
  ast.max_hp = card->get_max_hp();
  ast.team_dice_bonus = card ? s->team_dice_bonus[card->get_team_id()] : 0;

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

  int32_t total_last_attack_damage = 0;
  size_t last_attack_damage_count = 0;
  this->sum_last_attack_damage(nullptr, &total_last_attack_damage, &last_attack_damage_count);
  ast.final_last_attack_damage = total_last_attack_damage;
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

  size_t z = 0;

  uint16_t z_ref = pa.attacker_card_ref;
  // Note: The (z < 8) conditions in these two loops are not present in the
  // original code.
  for (z = 0;
       ((target_card_ref != z_ref) && (z < 8) && ((z_ref = pa.action_card_refs[z]) != 0xFFFF));
       z++) {
  }

  ast.action_cards_ap = 0;
  ast.action_cards_tp = 0;
  for (; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    auto ce = s->definition_for_card_ref(pa.action_card_refs[z]);
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
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
  auto target_card = s->card_for_set_card_ref(target_card_ref);
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
  } else if (is_nte) {
    target_card_loc = target_card->loc;
  } else {
    this->get_card1_loc_with_card2_opposite_direction(&target_card_loc, target_card, attacker_card);
  }

  auto attack_medium = attacker_card ? attacker_card->action_chain.chain.attack_medium : AttackMedium::INVALID_FF;

  if ((s->get_battle_phase() != BattlePhase::ACTION) ||
      (s->get_current_action_subphase() == ActionSubphase::ATTACK)) {
    return nullptr;
  }
  if (target_card_ref == attacker_card_ref) {
    return nullptr;
  }
  if (target_card_ref == set_card_ref) {
    return nullptr;
  }

  uint32_t pierce_flag = is_nte ? 0x00000080 : (0x00002000 << target_client_id);
  bool has_pierce = ((target_client_id != 0xFF) &&
      attacker_card &&
      (attacker_card->action_chain.check_flag(pierce_flag)));

  if (has_pierce && is_nte) {
    return nullptr;
  }

  // Handle Parry if present
  if (target_card && !(target_card->card_flags & 3)) {
    for (size_t x = 0; x < 9; x++) {
      auto& cond = target_card->action_chain.conditions[x];
      if (!is_nte && (unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
        continue;
      }
      if (cond.type == ConditionType::NONE) {
        continue;
      }
      if (!s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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
      if (has_pierce || (!is_nte && (unknown_p7 != 0)) || !target_ps) {
        continue;
      }

      // Parry forwards the attack to a random FC within one tile of the
      // original target. Note that Sega's implementation (used here) hardcodes
      // the Gifoie card's ID (00D9) for compute_effective_range.
      // TODO: We should fix this so it doesn't rely on a fixed card definition.
      parray<uint8_t, 9 * 9> range;
      compute_effective_range(range, s->options.card_index, 0x00D9, target_card_loc, s->map_and_rules);
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
        auto ce = s->definition_for_card_ref(card_ref);
        if (ce && ((ce->def.type == CardType::HUNTERS_SC) || (ce->def.type == CardType::ARKZ_SC))) {
          continue;
        }
        candidate_card_refs.emplace_back(card_ref);
      }

      size_t num_candidates = candidate_card_refs.size();
      if (num_candidates > 0) {
        uint8_t a = target_ps->roll_dice_with_effects(2);
        uint8_t b = target_ps->roll_dice_with_effects(1);
        return s->card_for_set_card_ref(
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
  // Curiously, this code does not exist in NTE, so it seems it was added after
  // NTE but never used.
  vector<shared_ptr<Card>> candidate_cards;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->get_player_state(client_id);
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
        if (!is_nte && (unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
          continue;
        }
        if (cond.type == ConditionType::NONE) {
          continue;
        }
        if (!s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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
                (is_nte || (unknown_p7 != 0)) &&
                ((unknown_p3 != 0) || (unknown_p4 != 0)) &&
                (target_client_id == client_id) &&
                target_card &&
                target_card->get_definition()->def.is_sc()) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::DEFENDER:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                (unknown_p4 != 0) &&
                (target_card_ref == other_set_card->action_chain.conditions[z].condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_set_card);
              if (!is_nte && unknown_p11 && (def_effect_index != 0xFF) && (set_card_ref != 0xFFFF) &&
                  !s->ruler_server->check_usability_or_apply_condition_for_card_refs(
                      set_card_ref, sc_card_ref, other_set_card->get_card_ref(), def_effect_index, attack_medium)) {
                *unknown_p11 = 1;
              }
            }
            break;
          case ConditionType::UNKNOWN_39:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                (unknown_p3 != 0) &&
                (target_card_ref == other_set_card->action_chain.conditions[z].condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::SURVIVAL_DECOYS:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                attacker_card &&
                (attacker_card->action_chain.chain.target_card_ref_count > 1) &&
                (unknown_p3 != 0) &&
                (other_set_card->get_team_id() == target_team_id)) {
              candidate_cards.emplace_back(other_set_card);
            }
            break;
          case ConditionType::REFLECT:
            if (!is_nte && (unknown_p7 == 0) && (unknown_p3 != 0)) {
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
    if (other_sc && !(other_sc->card_flags & (is_nte ? 1 : 3))) {
      for (size_t z = 0; (z < 9) && (candidate_cards.size() < 36); z++) {
        auto& cond = other_sc->action_chain.conditions[z];
        if (!is_nte && (unknown_p7 == 0) && this->card_ref_has_ability_trap(cond)) {
          continue;
        }
        if (cond.type == ConditionType::NONE) {
          continue;
        }
        if (!s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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
                (is_nte || (unknown_p7 != 0)) &&
                ((unknown_p3 != 0) || (unknown_p4 != 0)) &&
                (target_client_id == client_id) &&
                target_card &&
                target_card->get_definition()->def.is_sc()) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::DEFENDER:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                (unknown_p4 != 0) &&
                (target_card_ref == cond.condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_sc);
              if (!is_nte && unknown_p11 && (def_effect_index != 0xFF) && (set_card_ref != 0xFFFF) &&
                  !s->ruler_server->check_usability_or_apply_condition_for_card_refs(
                      set_card_ref, sc_card_ref, other_sc->get_card_ref(), def_effect_index, attack_medium)) {
                *unknown_p11 = 1;
              }
            }
            break;
          case ConditionType::UNKNOWN_39:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                (unknown_p3 != 0) &&
                (target_card_ref == cond.condition_giver_card_ref)) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::SURVIVAL_DECOYS:
            if (!has_pierce &&
                (is_nte || (unknown_p7 == 0)) &&
                attacker_card &&
                (attacker_card->action_chain.chain.target_card_ref_count > 1) &&
                (unknown_p3 != 0) &&
                (other_sc->get_team_id() == target_team_id)) {
              candidate_cards.emplace_back(other_sc);
            }
            break;
          case ConditionType::REFLECT:
            if (!is_nte && (unknown_p7 == 0) && (unknown_p3 != 0)) {
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
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("compute_stat_swap_type(@%04hX #%04hX): ", card->get_card_ref(), card->get_card_id()));
  if (!card) {
    log.debug("card is missing");
    return StatSwapType::NONE;
  }

  StatSwapType ret = StatSwapType::NONE;
  for (size_t cond_index = 0; cond_index < 9; cond_index++) {
    auto& cond = card->action_chain.conditions[cond_index];
    if (cond.type != ConditionType::NONE) {
      auto cond_log = log.sub(phosg::string_printf("(%zu) ", cond_index));
      string cond_str = cond.str(s);
      cond_log.debug("%s", cond_str.c_str());
      if (!this->card_ref_has_ability_trap(cond)) {
        if (cond.type == ConditionType::UNKNOWN_75) {
          if (ret == StatSwapType::A_H_SWAP) {
            log.debug("UNKNOWN_75: clearing");
            ret = StatSwapType::NONE;
          } else {
            log.debug("UNKNOWN_75: setting A_H_SWAP");
            ret = StatSwapType::A_H_SWAP;
          }
        } else if (cond.type == ConditionType::A_T_SWAP) {
          if (ret == StatSwapType::A_T_SWAP) {
            log.debug("A_T_SWAP: clearing");
            ret = StatSwapType::NONE;
          } else {
            log.debug("A_T_SWAP: setting A_T_SWAP");
            ret = StatSwapType::A_T_SWAP;
          }
        }
      } else {
        log.debug("skipping due to ability trap");
      }
    }
  }
  log.debug("ret = %zu", static_cast<size_t>(ret));
  return ret;
}

void CardSpecial::compute_team_dice_bonus(uint8_t team_id) {
  auto s = this->server();
  uint8_t value = s->team_exp[team_id] / (s->team_client_count[team_id] * 12);
  this->adjust_dice_boost_if_team_has_condition_52(team_id, &value, 0);
  s->team_dice_bonus[team_id] = min<uint8_t>(value, 8);
}

bool CardSpecial::condition_applies_on_sc_or_item_attack(const Condition& cond) const {
  auto ce = this->server()->definition_for_card_ref(cond.card_ref);
  if (!ce) {
    return false;
  }
  EffectWhen when = ce->def.effects[cond.card_definition_effect_index].when;
  return ((when == EffectWhen::AFTER_CREATURE_OR_HUNTER_SC_ATTACK) ||
      (when == EffectWhen::BEFORE_CREATURE_OR_HUNTER_SC_ATTACK));
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

size_t CardSpecial::count_cards_with_card_id_except_card_ref(
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
  auto s = this->server();
  if (s->options.is_nte()) {
    team_id = 0xFF;
    exclude_destroyed_cards = false;
  }
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
    auto ps = s->get_player_state(client_id);
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
    EffectWhen when) const {
  // Note: In the original code, as and dice_roll were optional pointers, but
  // they are non-null at all callsites, so we've replaced them with references
  // (and eliminated the null checks within this function).

  auto s = this->server();

  uint16_t attacker_card_ref = as.attacker_card_ref;
  if (attacker_card_ref == 0xFFFF) {
    attacker_card_ref = as.original_attacker_card_ref;
  }

  bool is_nte = s->options.is_nte();
  auto set_card = s->card_for_set_card_ref(set_card_ref);
  bool set_card_has_ability_trap =
      (!is_nte && set_card && this->card_has_condition_with_ref(set_card, ConditionType::ABILITY_TRAP, 0xFFFF, 0xFFFF));

  switch (arg2_text[0]) {
    case 'C':
      if (is_nte) {
        return false;
      }
      card = s->card_for_set_card_ref(set_card_ref);
      if (!card) {
        card = s->card_for_set_card_ref(sc_card_ref);
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
      auto ps = s->get_player_state(client_id_for_card_ref(card->get_card_ref()));
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
          uint8_t arg2_command = ce->def.effects[cond_index].arg2.at(0);
          if ((arg2_command == 'c') || (arg2_command == 'C')) {
            uint8_t other_ch1 = ce->def.effects[cond_index].arg2.at(1) - 0x30;
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
      auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
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
      auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
      return (attacker_card && (attacker_card->action_chain.chain.damage >= atoi(arg2_text + 1)));
    }

    case 'n':
      switch (atoi(arg2_text + 1)) {
        case 0x00: // n00
          return true;
        case 0x01: // n01
          return (!card || (card->get_definition()->def.type == CardType::HUNTERS_SC));
        case 0x02: // n02
          for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
            auto target_card = s->card_for_set_card_ref(as.target_card_refs[z]);
            if (target_card && target_card->check_card_flag(2)) {
              return true;
            }
          }
          return false;
        case 0x03: // n03
          for (size_t z = 0; z < 8; z++) {
            uint16_t action_card_ref = as.action_card_refs[z];
            if (action_card_ref != 0xFFFF) {
              auto ce = s->definition_for_card_ref(action_card_ref);
              if (card_class_is_tech_like(ce->def.card_class(), is_nte)) {
                return true;
              }
            }
          }
          return false;
        case 0x04: // n04
          return card->action_chain.check_flag(is_nte ? 0x00000080 : 0x0001E000);
        case 0x05: // n05
          return card->action_chain.check_flag(is_nte ? 0x00000002 : 0x00001E00);
        case 0x06: // n06
          return (card->get_definition()->def.card_class() == CardClass::NATIVE_CREATURE);
        case 0x07: // n07
          return (card->get_definition()->def.card_class() == CardClass::A_BEAST_CREATURE);
        case 0x08: // n08
          return (card->get_definition()->def.card_class() == CardClass::MACHINE_CREATURE);
        case 0x09: // n09
          return (card->get_definition()->def.card_class() == CardClass::DARK_CREATURE);
        case 0x0A: // n10
          return (card->get_definition()->def.card_class() == CardClass::SWORD_ITEM);
        case 0x0B: // n11
          return (card->get_definition()->def.card_class() == CardClass::GUN_ITEM);
        case 0x0C: // n12
          return (card->get_definition()->def.card_class() == CardClass::CANE_ITEM);
        case 0x0D: { // n13
          auto ce = card->get_definition();
          return ((ce->def.card_class() == CardClass::GUARD_ITEM) ||
              (!is_nte && (ce->def.card_class() == CardClass::MAG_ITEM)) ||
              s->ruler_server->find_condition_on_card_ref(card->get_card_ref(), ConditionType::GUARD_CREATURE, 0, 0, 0));
        }
        case 0x0E: // n14
          return card->get_definition()->def.is_sc();
        case 0x0F: // n15
          return ((card->action_chain.chain.attack_action_card_ref_count == 0) &&
              (card->action_metadata.defense_card_ref_count == 0));
        case 0x10: // n16
          return s->ruler_server->card_ref_is_aerial(card->get_card_ref());
        case 0x11: { // n17
          auto sc_card = s->card_for_set_card_ref(sc_card_ref);
          int16_t this_ap = card->ap;
          int16_t other_ap = -1;
          if (!sc_card) {
            auto ce = s->definition_for_card_ref(sc_card_ref);
            if (ce) {
              other_ap = ce->def.ap.stat;
            }
          } else {
            other_ap = sc_card->ap;
          }
          return (other_ap == this_ap);
        }
        case 0x12: // n18
          for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
            auto target_card = s->card_for_set_card_ref(as.target_card_refs[z]);
            if (target_card && target_card->get_definition()->def.is_sc()) {
              return true;
            }
          }
          return false;
        case 0x13: // n19
          return s->ruler_server->find_condition_on_card_ref(
              card->get_card_ref(), ConditionType::PARALYZE, 0, 0, 0);
        case 0x14: // n20
          return s->ruler_server->find_condition_on_card_ref(
              card->get_card_ref(), ConditionType::FREEZE, 0, 0, 0);
        case 0x15: { // n21
          if (!is_nte) {
            uint8_t client_id = client_id_for_card_ref(sc_card_ref);
            if (client_id != 0xFF) {
              return card->action_chain.check_flag(0x00002000 << client_id);
            }
          }
          return false;
        }
        case 0x16: { // n22
          if (!is_nte) {
            uint8_t client_id = client_id_for_card_ref(sc_card_ref);
            if (client_id != 0xFF) {
              return card->action_chain.check_flag(0x00000200 << client_id);
            }
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
        auto new_card = s->card_for_set_card_ref(set_card_ref);
        if (!new_card) {
          new_card = s->card_for_set_card_ref(sc_card_ref);
        }
        if (new_card) {
          card = new_card;
        }
      }
      return (this->find_condition_with_parameters(
                  card, ConditionType::ANY, set_card_ref, ((v % 10) == 0) ? 0xFF : (v % 10)) != nullptr);
    }
    case 'r':
      return (!set_card_has_ability_trap || is_nte) && (random_percent < atoi(arg2_text + 1));
    case 's': {
      auto ce = card->get_definition();
      return ((ce->def.self_cost >= arg2_text[1] - '0') &&
          (ce->def.self_cost <= arg2_text[2] - '0'));
    }
    case 't': {
      auto set_card = s->card_for_set_card_ref(set_card_ref);
      if (!set_card) {
        return false;
      }
      uint8_t v = atoi(arg2_text + 1);
      // TODO: Figure out what this logic actually does and rename the variables
      // or comment it appropriately.
      if (is_nte) {
        return (v < set_card->unknown_a9);
      } else if (when == EffectWhen::BEFORE_DICE_PHASE_THIS_TEAM_TURN) {
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
  auto log = this->server()->log_stack("evaluate_effect_expr: ");
  if (log.min_level == phosg::LogLevel::DEBUG) {
    log.debug("ast, expr=\"%s\", dice_roll=(client_id=%02hhX, a2=%02hhX, value=%02hhX, value_used_in_expr=%s, a5=%04hX)", expr, dice_roll.client_id, dice_roll.unknown_a2, dice_roll.value, dice_roll.value_used_in_expr ? "true" : "false", dice_roll.unknown_a5);
    ast.print(stderr);
  }

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
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("execute_effect(@%04hX #%04hX): ", card->get_card_ref(), card->get_card_id()));
  {
    string cond_str = cond.str(s);
    log.debug("cond=%s, card=@%04hX, expr_value=%hd, unknown_p5=%hd, cond_type=%s, unknown_p7=%" PRIu32 ", attacker_card_ref=@%04hX", cond_str.c_str(), ref_for_card(card), expr_value, unknown_p5, phosg::name_for_enum(cond_type), unknown_p7, attacker_card_ref);
  }
  bool is_nte = s->options.is_nte();

  int16_t clamped_expr_value = clamp<int16_t>(expr_value, -99, 99);

  cond.value8 = clamped_expr_value;
  if (!is_nte) {
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
      unknown_p7 &= ~4;
    }
    if (unknown_p7 == 0) {
      return false;
    }

  } else if (card->action_metadata.check_flag(0x10) &&
      (cond.card_ref != card->get_card_ref()) &&
      (cond.condition_giver_card_ref != card->get_card_ref())) {
    return false;
  }

  int16_t positive_expr_value = max<int16_t>(clamped_expr_value, 0);
  int16_t clamped_unknown_p5 = clamp<int16_t>(unknown_p5, 0, is_nte ? unknown_p5 : 99);
  auto attacker_sc = s->card_for_set_card_ref(attacker_card_ref);
  auto attack_medium = attacker_sc ? attacker_sc->action_chain.chain.attack_medium : AttackMedium::UNKNOWN;

  switch (cond_type) {
    case ConditionType::RAMPAGE:
    case ConditionType::IMMOBILE:
    case ConditionType::HOLD:
    case ConditionType::CANNOT_DEFEND:
    case ConditionType::GUOM:
    case ConditionType::PARALYZE:
    case ConditionType::PIERCE:
    case ConditionType::UNUSED_0F:
    case ConditionType::SET_MV_COST_TO_0:
    case ConditionType::UNUSED_13:
    case ConditionType::ACID:
    case ConditionType::ADD_1_TO_MV_COST:
    case ConditionType::FREEZE:
    case ConditionType::MAJOR_PIERCE:
    case ConditionType::HEAVY_PIERCE:
    case ConditionType::MAJOR_RAMPAGE:
    case ConditionType::HEAVY_RAMPAGE:
    case ConditionType::DEF_DISABLE_BY_COST:
    default:
      return false;

    case ConditionType::MV_BONUS:
      if (is_nte) {
        return false;
      }
      [[fallthrough]];
    case ConditionType::UNKNOWN_39:
    case ConditionType::DEFENDER:
    case ConditionType::SURVIVAL_DECOYS:
    case ConditionType::EXP_DECOY:
    case ConditionType::SET_MV:
      return true;

    case ConditionType::AP_BOOST:
      if (unknown_p7 & 1) {
        if (is_nte) {
          card->action_chain.chain.ap_effect_bonus += positive_expr_value;
        } else {
          card->action_chain.chain.ap_effect_bonus = clamp<int8_t>(
              card->action_chain.chain.ap_effect_bonus + positive_expr_value, -99, 99);
        }
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
        if (is_nte) {
          card->action_chain.chain.tp_effect_bonus += positive_expr_value;
        } else {
          card->action_chain.chain.tp_effect_bonus = clamp<int8_t>(
              card->action_chain.chain.tp_effect_bonus + positive_expr_value, -99, 99);
        }
      }
      return true;

    case ConditionType::GIVE_DAMAGE:
      if ((unknown_p7 & 4) != 0) {
        int16_t current_hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = is_nte ? (current_hp - positive_expr_value) : clamp<int16_t>(current_hp - positive_expr_value, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, -positive_expr_value, 0, 1);
        new_hp = max<int16_t>(new_hp, 0);
        if (new_hp != current_hp) {
          card->set_current_hp(new_hp);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::A_T_SWAP_0C:
    case ConditionType::A_T_SWAP_PERM:
      if (unknown_p7 & 4) {
        int16_t ap = is_nte ? card->ap : clamp<int16_t>(card->ap, -99, 99);
        int16_t tp = is_nte ? card->tp : clamp<int16_t>(card->tp, -99, 99);
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
        int16_t ap = is_nte ? card->ap : clamp<int16_t>(card->ap, -99, 99);
        int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
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
        int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = is_nte ? (hp + positive_expr_value) : clamp<int16_t>(hp + positive_expr_value, -99, 99);
        log.debug("HEAL: hp=%hd, positive_expr_value=%hd, new_hp=%hd", hp, positive_expr_value, new_hp);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, new_hp - hp, true, true);
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
        auto ps = s->player_states.at(client_id);
        if (!ps) {
          return false;
        }
        if ((card->card_flags & 2) || this->card_is_destroyed(card)) {
          return true;
        }
        this->send_6xB4x06_for_card_destroyed(card, attacker_card_ref);
        if (!is_nte) {
          card->unknown_802380C0();
        }
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
        size_t count = this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::UNIT_BLOW, 0xFFFF);
        int16_t clamped_count = is_nte ? count : clamp<int16_t>(count, -99, 99);
        int16_t result = card->action_chain.chain.ap_effect_bonus + clamped_count * positive_expr_value;
        card->action_chain.chain.ap_effect_bonus = is_nte ? result : clamp<int16_t>(result, -99, 99);
      }
      return false;

    case ConditionType::CURSE:
      if (unknown_p7 & 4) {
        for (size_t z = 0; z < card->action_chain.chain.target_card_ref_count; z++) {
          auto target_card = s->card_for_set_card_ref(card->action_chain.chain.target_card_refs[z]);
          if (target_card) {
            CardShortStatus stat = target_card->get_short_status();
            if (stat.card_flags & 2) {
              int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
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
        int16_t count = this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::COMBO_AP, 0xFFFF);
        int16_t clamped_count = is_nte ? count : clamp<int16_t>(count, -99, 99);
        int16_t result = card->action_chain.chain.ap_effect_bonus + clamped_count * clamped_count;
        card->action_chain.chain.ap_effect_bonus = is_nte ? result : clamp<int16_t>(result, -99, 99);
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

    case ConditionType::ABILITY_TRAP:
      if (is_nte && (unknown_p7 & 4)) {
        bool needs_update = false;
        for (ssize_t z = 8; z >= 0; z--) {
          auto& cond = card->action_chain.conditions[z];
          if (cond.type == ConditionType::NONE) {
            break;
          }

          G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
          cmd.effect.flags = 0x04;
          cmd.effect.attacker_card_ref = attacker_card_ref;
          cmd.effect.target_card_ref = card->get_card_ref();
          cmd.effect.value = 0;
          cmd.effect.operation = -static_cast<int8_t>(cond.type);
          cmd.effect.condition_index = z;
          s->send(cmd);

          this->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, card);
          needs_update = true;
        }

        if (needs_update) {
          card->send_6xB4x4E_4C_4D_if_needed();
        }
      }
      return false;

    case ConditionType::ANTI_ABNORMALITY_1:
      if (unknown_p7 & 4) {
        for (ssize_t z = 8; z >= 0; z--) {
          auto& cond = card->action_chain.conditions[z];
          if ((cond.type == ConditionType::IMMOBILE) ||
              (cond.type == ConditionType::HOLD) ||
              (cond.type == ConditionType::CANNOT_DEFEND) ||
              (cond.type == ConditionType::GUOM) ||
              (cond.type == ConditionType::PARALYZE) ||
              (cond.type == ConditionType::UNUSED_13) ||
              (cond.type == ConditionType::ACID) ||
              (cond.type == ConditionType::ADD_1_TO_MV_COST) ||
              (cond.type == ConditionType::CURSE) ||
              (cond.type == ConditionType::PIERCE_RAMPAGE_BLOCK) ||
              (!is_nte && (cond.type == ConditionType::FREEZE)) ||
              (cond.type == ConditionType::UNKNOWN_1E) ||
              (!is_nte && (cond.type == ConditionType::DROP))) {
            G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
            cmd.effect.flags = 0x04;
            cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 0x0C);
            cmd.effect.target_card_ref = card->get_card_ref();
            cmd.effect.value = 0;
            cmd.effect.operation = -static_cast<int8_t>(cond.type);
            cmd.effect.condition_index = z;
            s->send(cmd);
            this->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, card);
            card->send_6xB4x4E_4C_4D_if_needed();
          }
        }
      }
      return false;

    case ConditionType::UNKNOWN_1E:
      if (unknown_p7 & 4) {
        auto sc_card = s->card_for_set_card_ref(attacker_card_ref);
        if (!sc_card || (sc_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
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
      if (unknown_p7 & (is_nte ? 0x02 : 0x40)) {
        size_t count = this->count_action_cards_with_condition_for_all_current_attacks(ConditionType::EXPLOSION, 0xFFFF);
        int16_t clamped_count = is_nte ? count : clamp<int16_t>(count, -99, 99);
        card->action_metadata.attack_bonus = is_nte
            ? (clamped_count * clamped_count)
            : clamp<int16_t>(clamped_count * clamped_count, -99, 99);
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
      auto ps = s->player_states.at(client_id);
      if (!ps) {
        return false;
      }
      if (!is_nte) {
        card->unknown_802380C0();
      }
      return ps->discard_card_or_add_to_draw_pile(card->get_card_ref(), !is_nte);
    }

    case ConditionType::AP_LOSS:
      if (unknown_p7 & 1) {
        int16_t new_value = card->action_chain.chain.ap_effect_bonus - positive_expr_value;
        card->action_chain.chain.ap_effect_bonus = is_nte ? new_value : clamp<int16_t>(new_value, -99, 99);
      }
      return true;

    case ConditionType::BONUS_FROM_LEADER:
      if (unknown_p7 & 1) {
        size_t leader_count = this->count_cards_with_card_id_except_card_ref(expr_value, 0xFFFF);
        int16_t new_value = card->action_chain.chain.ap_effect_bonus + leader_count;
        card->action_chain.chain.ap_effect_bonus = is_nte ? new_value : clamp<int16_t>(new_value, -99, 99);
      }
      return true;

    case ConditionType::FILIAL: {
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, positive_expr_value, 0, 1);
        if (positive_expr_value != 0) {
          int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
          int16_t new_hp = is_nte ? (hp + positive_expr_value) : clamp<int16_t>(hp + positive_expr_value, -99, 99);
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
          auto attacker_ps = s->player_states.at(attacker_client_id);
          auto target_ps = s->player_states.at(target_client_id);
          if (attacker_ps && target_ps) {
            uint8_t attacker_team_id = attacker_ps->get_team_id();
            uint8_t target_team_id = target_ps->get_team_id();
            if (positive_expr_value < s->team_exp[target_team_id]) {
              s->team_exp[attacker_team_id] += positive_expr_value;
              s->team_exp[target_team_id] -= positive_expr_value;
            } else {
              positive_expr_value = s->team_exp[target_team_id];
              s->team_exp[attacker_team_id] += s->team_exp[target_team_id];
              s->team_exp[target_team_id] = 0;
            }
            this->compute_team_dice_bonus(attacker_team_id);
            this->compute_team_dice_bonus(target_team_id);
            if (!is_nte) {
              this->send_6xB4x06_for_exp_change(card, attacker_card_ref, -positive_expr_value, 1);
            }
            s->update_battle_state_flags_and_send_6xB4x03_if_needed();
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
      if (!is_nte && (unknown_p7 & 4)) {
        auto ps = card->player_state();
        if (ps) {
          uint8_t team_id = ps->get_team_id();
          int16_t delta = 0;
          if (s->team_exp[team_id] < 4) {
            s->team_exp[team_id] = 0;
          } else {
            delta = -3;
            s->team_exp[team_id] -= 3;
          }
          this->compute_team_dice_bonus(team_id);
          this->send_6xB4x06_for_exp_change(card, attacker_card_ref, delta, 1);
        }
      }
      return true;

    case ConditionType::ACTION_DISRUPTER:
      if (unknown_p7 & 4) {
        if (is_nte) {
          card->action_metadata.defense_card_ref_count = 0;
        } else {
          for (size_t z = 0; z < card->action_chain.chain.attack_action_card_ref_count; z++) {
            this->apply_stat_deltas_to_all_cards_from_all_conditions_with_card_ref(card->action_chain.chain.attack_action_card_refs[z]);
          }
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
        if ((client_id != 0xFF) && s->player_states.at(client_id)) {
          uint8_t team_id = s->player_states.at(client_id)->get_team_id();
          int32_t existing_exp = s->team_exp[team_id];
          if ((clamped_expr_value + existing_exp) < 0) {
            clamped_expr_value = -existing_exp;
            s->team_exp[team_id] = 0;
          } else {
            s->team_exp[team_id] = existing_exp + clamped_expr_value;
          }
          if (!is_nte) {
            this->send_6xB4x06_for_exp_change(card, attacker_card_ref, clamped_expr_value, 1);
          }
          this->compute_team_dice_bonus(team_id);
          s->update_battle_state_flags_and_send_6xB4x03_if_needed();
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
        if (is_nte) {
          for (size_t z = 0; z < attacker_sc->action_chain.chain.target_card_ref_count; z++) {
            card_refs.emplace_back(attacker_sc->action_chain.chain.target_card_refs[z]);
          }
        } else if (attacker_sc != card) {
          card_refs.emplace_back(card->get_card_ref());
        }

        for (uint16_t card_ref : card_refs) {
          auto sc_card = s->card_for_set_card_ref(card_ref);
          if (sc_card && (sc_card->get_current_hp() > 0)) {
            if (s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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
          size_t count = this->count_cards_with_card_id_except_card_ref(ce->def.card_id, card->get_card_ref());
          int16_t clamped_count = is_nte ? count : clamp<int16_t>(count, -99, 99);
          int16_t new_value = card->action_chain.chain.ap_effect_bonus + clamped_count * positive_expr_value;
          card->action_chain.chain.ap_effect_bonus = is_nte ? new_value : clamp<int16_t>(new_value, -99, 99);
        }
      }
      return true;

    case ConditionType::BERSERK:
      if (unknown_p7 & 4) {
        int16_t hp = is_nte ? card->get_current_hp() : clamp<int16_t>(card->get_current_hp(), -99, 99);
        int16_t new_hp = is_nte
            ? (hp - card->action_chain.chain.damage)
            : clamp<int16_t>(hp - this->max_all_attack_bonuses(nullptr), -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x20, new_hp - hp, false, true);
        new_hp = max<int16_t>(new_hp, 0);
        if (new_hp != hp) {
          card->set_current_hp(new_hp);
          this->destroy_card_if_hp_zero(card, attacker_card_ref);
        }
      }
      return true;

    case ConditionType::UNKNOWN_49:
      if (unknown_p7 & 4) {
        auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
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
        card->ap = is_nte
            ? (card->ap + positive_expr_value)
            : clamp<int16_t>(card->ap + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::TP_GROWTH:
      if (unknown_p7 & 4) {
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, positive_expr_value, 0, 1);
        card->tp = is_nte
            ? (card->tp + positive_expr_value)
            : clamp<int16_t>(card->tp + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::COPY:
      if (unknown_p7 & 4) {
        auto attacker_card = s->card_for_set_card_ref(attacker_card_ref);
        if (attacker_card && (attacker_card != card)) {
          int16_t new_ap = (positive_expr_value < 51) ? (card->ap / 2) : card->ap;
          int16_t new_tp = (positive_expr_value < 51) ? (card->tp / 2) : card->tp;
          if (!is_nte) {
            new_ap = clamp<int16_t>(new_ap, -99, 99);
            new_tp = clamp<int16_t>(new_tp, -99, 99);
          }
          this->send_6xB4x06_for_stat_delta(attacker_card, attacker_card_ref, 0xA0, new_ap - attacker_card->ap, false, false);
          this->send_6xB4x06_for_stat_delta(attacker_card, attacker_card_ref, 0x80, new_tp - attacker_card->tp, false, false);
          attacker_card->ap = new_ap;
          attacker_card->tp = new_tp;
        }
      }
      return true;

    case ConditionType::MISC_GUARDS:
      if (unknown_p7 & 8) {
        int16_t new_value = positive_expr_value + card->action_metadata.defense_bonus;
        card->action_metadata.defense_bonus = is_nte ? new_value : clamp<int16_t>(new_value, -99, 99);
      }
      return true;

    case ConditionType::AP_OVERRIDE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        cond.value = is_nte ? (positive_expr_value - card->ap) : clamp<int16_t>(positive_expr_value - card->ap, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, cond.value, false, false);
        card->ap = positive_expr_value;
        cond.flags |= 2;
      }
      return true;

    case ConditionType::TP_OVERRIDE:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        cond.value = is_nte ? (positive_expr_value - card->tp) : clamp<int16_t>(positive_expr_value - card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, cond.value, false, false);
        card->tp = positive_expr_value;
        cond.flags |= 2;
      }
      return true;

    case ConditionType::UNKNOWN_64:
    case ConditionType::FORWARD_DAMAGE:
      if (is_nte) {
        return false;
      }
      [[fallthrough]];
    case ConditionType::SLAYERS_ASSASSINS:
      if (is_nte) {
        auto set_card = s->card_for_set_card_ref(attacker_card_ref);
        bool card_found = false;
        if (!set_card) {
          card_found = false;
        } else {
          for (size_t z = 0; z < set_card->action_chain.chain.target_card_ref_count; z++) {
            if (set_card->action_chain.chain.target_card_refs[z] == card->get_card_ref()) {
              card_found = true;
              break;
            }
          }
        }
        if (card_found) {
          if (unknown_p7 & 8) {
            card->action_metadata.defense_bonus -= positive_expr_value;
          }
          return true;
        } else {
          return this->execute_effect(
              cond, card, positive_expr_value, clamped_unknown_p5, ConditionType::GIVE_DAMAGE, unknown_p7, attacker_card_ref);
        }

      } else if (unknown_p7 & 0x20) {
        card->action_metadata.attack_bonus = clamp<int16_t>(
            card->action_metadata.attack_bonus + positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::BLOCK_ATTACK:
      if (unknown_p7 & 4) {
        card->action_metadata.set_flags(0x10);
      }
      return true;

    case ConditionType::COMBO_TP:
      if (unknown_p7 & 1) {
        ssize_t count = this->count_cards_with_card_id_except_card_ref(expr_value, 0xFFFF);
        int16_t new_value = count + card->action_chain.chain.tp_effect_bonus;
        card->action_chain.chain.tp_effect_bonus = is_nte ? new_value : clamp<int16_t>(new_value, -99, 99);
      }
      return true;

    case ConditionType::MISC_AP_BONUSES:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        if (is_nte) {
          int16_t orig_ap = card->ap;
          card->ap = max<int16_t>(positive_expr_value + card->ap, 0);
          cond.value = card->ap - orig_ap;
        } else {
          int16_t orig_ap = clamp<int16_t>(card->ap, -99, 99);
          card->ap = clamp<int16_t>(positive_expr_value + card->ap, 0, 99);
          cond.value = clamp<int16_t>(card->ap - orig_ap, -99, 99);
        }
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::MISC_TP_BONUSES:
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        if (is_nte) {
          int16_t orig_tp = card->tp;
          card->tp = max<int16_t>(positive_expr_value + card->tp, 0);
          cond.value = card->tp - orig_tp;
        } else {
          int16_t orig_tp = clamp<int16_t>(card->tp, -99, 99);
          card->tp = clamp<int16_t>(positive_expr_value + card->tp, 0, 99);
          cond.value = clamp<int16_t>(card->tp - orig_tp, -99, 99);
        }
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::MISC_DEFENSE_BONUSES:
    case ConditionType::WEAK_SPOT_INFLUENCE:
      if (is_nte) {
        return false;
      }
      if (unknown_p7 & 0x20) {
        card->action_metadata.attack_bonus = clamp<int16_t>(
            card->action_metadata.attack_bonus - positive_expr_value, 0, 99);
      }
      return true;

    case ConditionType::MOSTLY_HALFGUARDS:
    case ConditionType::DAMAGE_MODIFIER_2:
      if (is_nte) {
        return false;
      }
      if (unknown_p7 & 0x40) {
        card->action_metadata.attack_bonus = positive_expr_value;
      }
      return true;

    case ConditionType::PERIODIC_FIELD:
      if (is_nte) {
        return false;
      }
      if ((unknown_p7 & 0x40) &&
          (static_cast<uint16_t>(attack_medium) == ((s->get_round_num() >> 1) & 1) + 1)) {
        card->action_metadata.attack_bonus = 0;
      }
      return true;

    case ConditionType::AP_SILENCE:
      if (is_nte) {
        return false;
      }
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t prev_ap = clamp<int16_t>(card->ap, -99, 99);
        card->ap = clamp<int16_t>(card->ap - positive_expr_value, 0, 99);
        cond.value = clamp<int16_t>(prev_ap - card->ap, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0xA0, -cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::TP_SILENCE:
      if (is_nte) {
        return false;
      }
      if ((unknown_p7 & 4) && !(cond.flags & 2)) {
        int16_t prev_ap = clamp<int16_t>(card->tp, -99, 99);
        card->tp = clamp<int16_t>(card->tp - positive_expr_value, 0, 99);
        cond.value = clamp<int16_t>(prev_ap - card->tp, -99, 99);
        this->send_6xB4x06_for_stat_delta(card, attacker_card_ref, 0x80, -cond.value, 0, 0);
        cond.flags |= 2;
      }
      return false;

    case ConditionType::RAMPAGE_AP_LOSS:
      if (is_nte) {
        return false;
      }
      if (unknown_p7 & 1) {
        card->action_chain.chain.tp_effect_bonus = clamp<int16_t>(
            card->action_chain.chain.tp_effect_bonus - positive_expr_value, -99, 99);
      }
      return true;

    case ConditionType::UNKNOWN_77:
      if (is_nte) {
        return false;
      }
      if (attacker_sc && (unknown_p7 & 4)) {
        vector<uint16_t> card_refs;
        card_refs.emplace_back(attacker_sc->get_card_ref());
        for (size_t z = 0; z < attacker_sc->action_chain.chain.target_card_ref_count; z++) {
          card_refs.emplace_back(attacker_sc->action_chain.chain.target_card_refs[z]);
        }

        for (uint16_t card_ref : card_refs) {
          auto set_card = s->card_for_set_card_ref(card_ref);
          if (set_card && (set_card->get_current_hp() > 0)) {
            if (s->ruler_server->check_usability_or_apply_condition_for_card_refs(
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

  if (this->server()->options.is_nte()) {
    // The NTE version of this function returns a boolean instead of a pointer;
    // we always return a pointer for simplicity reasons, even for NTE.
    for (size_t z = 0; z < 9; z++) {
      auto& cond = card->action_chain.conditions[z];
      auto orig_eff = this->original_definition_for_condition(cond);

      if (((cond_type == ConditionType::ANY) || (cond.type == cond_type)) &&
          ((set_card_ref == 0xFFFF) || (cond.card_ref == set_card_ref)) &&
          ((def_effect_index != 0xFF) || (orig_eff || (orig_eff->effect_num == def_effect_index)))) {
        return &cond;
      }
    }
    return nullptr;

  } else {
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
  auto s = this->server();
  if (s->options.is_nte()) {
    return default_card_id;
  } else if (card2 && !(static_cast<uint8_t>(card2->facing_direction) & 0x80)) {
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
    for (; (*expr >= 'a') && (*expr <= 'z'); expr++) {
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
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("get_targeted_cards_for_condition(@%04hX, %hhu, @%04hX): ", card_ref, def_effect_index, setter_card_ref));
  log.debug("card_ref=@%04hX, def_effect_index=%02hhX, setter_card_ref=@%04hX, as, p_target_type=%hd, apply_usability_filters=%s", card_ref, def_effect_index, setter_card_ref, p_target_type, apply_usability_filters ? "true" : "false");

  vector<shared_ptr<const Card>> ret;

  uint8_t client_id = client_id_for_card_ref(card_ref);
  auto card1 = s->card_for_set_card_ref(card_ref);
  if (!card1) {
    card1 = s->card_for_set_card_ref(setter_card_ref);
  }
  log.debug("card1=@%04hX", ref_for_card(card1));

  auto card2 = s->card_for_set_card_ref((as.attacker_card_ref == 0xFFFF)
          ? as.original_attacker_card_ref
          : as.attacker_card_ref);
  log.debug("card2=@%04hX", ref_for_card(card2));

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
  log.debug("attack_medium=%s", phosg::name_for_enum(attack_medium));

  auto add_card_refs = [&](const vector<uint16_t>& result_card_refs) -> void {
    for (uint16_t result_card_ref : result_card_refs) {
      auto result_card = s->card_for_set_card_ref(result_card_ref);
      if (result_card) {
        ret.emplace_back(result_card);
      }
    }
  };

  switch (p_target_type) {
    case 0x01: // p01
    case 0x05: { // p05
      auto result_card = s->card_for_set_card_ref(setter_card_ref);
      if (result_card) {
        log.debug("(p01/p05) result_card=@%04hX", ref_for_card(result_card));
        ret.emplace_back(result_card);
      } else {
        log.debug("(p01/p05) result_card=null");
      }
      break;
    }
    case 0x02: // p02
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          auto result_card = s->card_for_set_card_ref(as.target_card_refs[z]);
          if (result_card) {
            ret.emplace_back(result_card);
          }
        }
      } else if (card2) {
        ret.emplace_back(card2);
      }
      break;
    case 0x03: // p03
      if (card1) {
        auto ce = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, ce->def.card_id, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          add_card_refs(ps->get_card_refs_within_range_from_all_players(range, card1_loc, CardType::ITEM));
        }
      }
      if (card1) {
        auto ce = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, ce->def.card_id, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          add_card_refs(ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id()));
        }
      }
      break;
    case 0x04: // p04
      size_t z;
      for (z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF) && (as.action_card_refs[z] != card_ref); z++) {
      }
      for (; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
        auto result_card = s->card_for_set_card_ref(as.action_card_refs[z]);
        if (result_card) {
          ret.emplace_back(result_card);
        }
      }
      break;
    case 0x06: // p06
      ret = this->get_attacker_card_and_sc_if_item(as);
      break;
    case 0x07: { // p07
      auto card = this->get_attacker_card(as);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 0x08: { // p08
      auto card = this->sc_card_for_client_id(client_id);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 0x09: // p09
      if (card1) {
        auto ce = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, ce->def.card_id, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          add_card_refs(ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id()));
        }
      }
      break;
    case 0x0A: // p10
      ret = this->find_all_cards_on_same_or_other_team(client_id, true);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x0B: // p11
      ret = this->find_all_set_cards_on_client_team(client_id);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x0C: // p12
      if (s->options.is_nte()) {
        ret = this->find_cards_by_condition_inc_exc(
            ConditionType::NONE, ConditionType::AERIAL, AssistEffect::NONE, AssistEffect::FLY);
      } else {
        ret = this->find_all_cards_by_aerial_attribute(false);
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x0D: // p13
      ret = this->find_cards_by_condition_inc_exc(ConditionType::FREEZE);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x0E: // p14
      ret = this->find_cards_in_hp_range(-1000, 3);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x0F: // p15
      ret = this->get_all_set_cards();
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x10: { // p16
      ret = this->find_cards_in_hp_range(8, 1000);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    }
    case 0x11: { // p17
      auto result_card = s->card_for_set_card_ref(card_ref);
      if (result_card) {
        ret.emplace_back(result_card);
      }
      break;
    }
    case 0x12: { // p18
      auto card = this->sc_card_for_client_id(client_id);
      if (card) {
        ret.emplace_back(card);
      }
      break;
    }
    case 0x13: // p19
      ret = this->find_all_sc_cards_of_class(CardClass::HU_SC);
      break;
    case 0x14: // p20
      ret = this->find_all_sc_cards_of_class(CardClass::RA_SC);
      break;
    case 0x15: // p21
      ret = this->find_all_sc_cards_of_class(CardClass::FO_SC);
      break;
    case 0x16: // p22
      if (card1) {
        auto def = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again, Sega hardcodes the Gifoie card's ID here... we
          // should fix this eventually.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
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
    case 0x17: { // p23
      auto log23 = log.sub("(p23) ");
      if (card1) {
        auto def = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again with the Gifoie hardcoding...
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          log23.debug("effective range card ID is #%04hX", range_card_id);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules, &log23);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          log23.debug("%zu result card refs", result_card_refs.size());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_log = log23.subf("(result @%04hX) ", result_card_ref);
            auto result_card = s->card_for_set_card_ref(result_card_ref);
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
    case 0x18: // p24
      ret = this->find_cards_by_condition_inc_exc(ConditionType::PARALYZE);
      break;
    case 0x19: // p25
      if (s->options.is_nte()) {
        // This appears to be a copy/paste error in NTE that was fixed in the
        // final version. Presumably include_cond should be ConditionType::FLY
        // here, not PARALYZE.
        ret = this->find_cards_by_condition_inc_exc(
            ConditionType::PARALYZE, ConditionType::NONE, AssistEffect::FLY, AssistEffect::NONE);
      } else {
        ret = this->find_all_cards_by_aerial_attribute(true);
      }
      break;
    case 0x1A: // p26
      ret = this->find_cards_damaged_by_at_least(1);
      break;
    case 0x1B: // p27
      ret = this->get_all_set_cards_by_team_and_class(CardClass::NATIVE_CREATURE, 0xFF, false);
      break;
    case 0x1C: // p28
      ret = this->get_all_set_cards_by_team_and_class(CardClass::A_BEAST_CREATURE, 0xFF, false);
      break;
    case 0x1D: // p29
      ret = this->get_all_set_cards_by_team_and_class(CardClass::MACHINE_CREATURE, 0xFF, false);
      break;
    case 0x1E: // p30
      ret = this->get_all_set_cards_by_team_and_class(CardClass::DARK_CREATURE, 0xFF, false);
      break;
    case 0x1F: // p31
      ret = this->get_all_set_cards_by_team_and_class(CardClass::SWORD_ITEM, 0xFF, false);
      break;
    case 0x20: // p32
      ret = this->get_all_set_cards_by_team_and_class(CardClass::GUN_ITEM, 0xFF, false);
      break;
    case 0x21: // p33
      ret = this->get_all_set_cards_by_team_and_class(CardClass::CANE_ITEM, 0xFF, false);
      break;
    case 0x22: // p34
      if (as.original_attacker_card_ref == 0xFFFF) {
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          auto result_card = s->card_for_set_card_ref(as.target_card_refs[z]);
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
    case 0x23: // p35
      if (card1) {
        auto def = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Again with the Gifoie hardcoding...
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
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
    case 0x24: { // p36
      auto log36 = log.sub("(p36) ");
      // On NTE, this includes SCs and items; on other versions, it's SCs only
      static const auto should_include = +[](shared_ptr<const CardIndex::CardEntry> ce, bool is_nte) -> bool {
        return (ce && (ce->def.is_sc() || (is_nte ? (ce->def.type == CardType::ITEM) : false)));
      };
      bool is_nte = s->options.is_nte();
      if (as.original_attacker_card_ref == 0xFFFF) {
        log36.debug("original_attacker_card_ref missing");
        // debug_str_for_card_ref
        for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
          string debug_ref_str = s->debug_str_for_card_ref(as.target_card_refs[z]);
          log36.debug("examining %s", debug_ref_str.c_str());
          auto result_card = s->card_for_set_card_ref(as.target_card_refs[z]);
          if (result_card && should_include(result_card->get_definition(), is_nte)) {
            log36.debug("adding %s", debug_ref_str.c_str());
            ret.emplace_back(result_card);
          } else {
            log36.debug("skipping %s", debug_ref_str.c_str());
          }
        }
      } else if (card2 && should_include(card2->get_definition(), is_nte)) {
        string debug_ref_str = s->debug_str_for_card_ref(card2->get_card_ref());
        log36.debug("original_attacker_card_ref present; adding card2 = %s", debug_ref_str.c_str());
        ret.emplace_back(card2);
      } else if (card2) {
        string debug_ref_str = s->debug_str_for_card_ref(card2->get_card_ref());
        log36.debug("original_attacker_card_ref present and card2 (%s) not eligible", debug_ref_str.c_str());
      } else {
        log36.debug("original_attacker_card_ref present and card2 missing");
      }
      break;
    }
    case 0x25: // p37
      ret = this->find_all_cards_on_same_or_other_team(client_id, false);
      if (!s->options.is_nte()) {
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      }
      break;
    case 0x26: // p38
      if (card1) {
        auto def = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Yet another Gifoie hardcode location :(
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
            if (result_card &&
                (result_card->get_definition()->def.type != CardType::ITEM) &&
                (result_card->get_card_ref() != card_ref)) {
              ret.emplace_back(result_card);
            }
          }
        }
      }
      break;
    case 0x27: // p39
    case 0x28: { // p40
      auto log3940 = log.sub("(p39/p40) ");
      ret = this->find_all_set_cards_with_cost_in_range(
          (p_target_type == 0x27) ? 4 : 0,
          (p_target_type == 0x27) ? 99 : 3);
      if (log3940.should_log(phosg::LogLevel::DEBUG)) {
        for (const auto& card : ret) {
          log3940.debug("found target @%04hX #%04hX", card->get_card_ref(), card->get_card_id());
        }
      }
      if (!s->options.is_nte()) {
        log3940.debug("filtering targets");
        ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
        if (log3940.should_log(phosg::LogLevel::DEBUG)) {
          for (const auto& card : ret) {
            log3940.debug("retained target @%04hX #%04hX", card->get_card_ref(), card->get_card_id());
          }
        }
      }
      break;
    }
    case 0x29: { // p41
      auto ps = card1->player_state();
      if (card1 && ps) {
        // TODO: Sigh. Gifoie again.
        uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
        parray<uint8_t, 9 * 9> range;
        compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
        auto result_card_refs = ps->get_all_cards_within_range(
            range, card1_loc, s->options.is_nte() ? card1->get_team_id() : 0xFF);
        for (uint16_t result_card_ref : result_card_refs) {
          auto result_card = s->card_for_set_card_ref(result_card_ref);
          if (result_card &&
              (result_card != card1) &&
              (result_card->get_card_ref() != card_ref) &&
              (result_card->get_definition()->def.is_fc())) {
            ret.emplace_back(result_card);
          }
        }

        if (!s->options.is_nte()) {
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
      }
      break;
    }
    case 0x2A: { // p42
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
          check_card(s->card_for_set_card_ref(as.target_card_refs[z]));
        }
      } else if (card2) {
        check_card(card2);
      }
      break;
    }
    case 0x2B: // p43
      if (s->options.is_nte()) {
        break;
      }
      for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
        auto result_card = s->card_for_set_card_ref(as.target_card_refs[z]);
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
    case 0x2C: { // p44
      if (s->options.is_nte()) {
        break;
      }
      auto ps = s->get_player_state(client_id);
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
    case 0x2D: // p45
      if (s->options.is_nte()) {
        break;
      }
      this->sum_last_attack_damage(&ret, nullptr, nullptr);
      ret = this->filter_cards_by_range(ret, card1, card1_loc, card2);
      break;
    case 0x2E: // p46
      if (s->options.is_nte()) {
        break;
      }
      if (card1) {
        auto def = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (def && ps) {
          // TODO: Yet another hardcoded card ID... but this time it's Cross
          // Slay instead of Gifoie
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x009C, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
            if (result_card && (result_card->get_definition()->def.type != CardType::ITEM)) {
              ret.emplace_back(result_card);
            }
          }
        }
      }
      break;
    case 0x2F: { // p47
      if (s->options.is_nte()) {
        break;
      }
      uint8_t client_id = client_id_for_card_ref(as.original_attacker_card_ref);
      if (client_id != 0xFF) {
        auto card = this->sc_card_for_client_id(client_id);
        if (card) {
          ret.emplace_back(card);
        }
      }
      break;
    }
    case 0x30: // p48
      if (s->options.is_nte()) {
        break;
      }
      if (card1) {
        auto ce = s->definition_for_card_ref(card_ref);
        auto ps = card1->player_state();
        if (ce && ps) {
          // TODO: Sigh. Gifoie. Sigh.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, 0xFF);
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
            if (result_card) {
              auto result_ce = result_card->get_definition();
              if (result_ce->def.type == CardType::HUNTERS_SC) {
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
        auto setter_card = s->card_for_set_card_ref(setter_card_ref);
        if (setter_card) {
          ret.emplace_back(setter_card);
        }
      }
      break;
    case 0x31: // p49
      if (s->options.is_nte()) {
        break;
      }
      if (card1) {
        auto ps = card1->player_state();
        if (ps) {
          // TODO: One more Gifoie here.
          uint16_t range_card_id = this->get_card_id_with_effective_range(card1, 0x00D9, card2);
          parray<uint8_t, 9 * 9> range;
          compute_effective_range(range, s->options.card_index, range_card_id, card1_loc, s->map_and_rules);
          auto result_card_refs = ps->get_all_cards_within_range(range, card1_loc, card1->get_team_id());
          for (uint16_t result_card_ref : result_card_refs) {
            auto result_card = s->card_for_set_card_ref(result_card_ref);
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
      if (s->ruler_server->check_usability_or_apply_condition_for_card_refs(
              card_ref, setter_card_ref, c->get_card_ref(), def_effect_index, attack_medium)) {
        filtered_ret.emplace_back(c);
        log.debug("usability filter: kept card @%04hX", ref_for_card(c));
      } else {
        log.debug("usability filter: removed card @%04hX", ref_for_card(c));
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
  auto s = this->server();
  auto log = s->log_stack("is_card_targeted_by_condition: ");

  if (log.should_log(phosg::LogLevel::DEBUG)) {
    log.debug("card=(@%04hX #%04hX)", card->get_card_ref(), card->get_card_id());
    auto cond_str = cond.str(s);
    auto as_str = as.str(s);
    log.debug("cond = %s", cond_str.c_str());
    log.debug("as = %s", as_str.c_str());
  }

  if (cond.type == ConditionType::NONE) {
    log.debug("condition is NONE (=> true)");
    return true;
  }

  auto ce = s->definition_for_card_ref(cond.card_ref);
  auto sc_card = s->card_for_set_card_ref(cond.card_ref);
  if ((!sc_card || ((sc_card != card) && (sc_card->card_flags & 2))) &&
      ce &&
      ((ce->def.type == CardType::ITEM) || ce->def.is_sc()) &&
      (cond.remaining_turns != 100) &&
      (s->options.is_nte() || (client_id_for_card_ref(card->get_card_ref()) == client_id_for_card_ref(cond.card_ref)))) {
    log.debug("failed item or SC check (=> false)");
    return false;
  }

  if (cond.remaining_turns != 102) {
    log.debug("remaining_turns != 102 (=> true)");
    return true;
  }

  if (sc_card && ((sc_card == card) || !(sc_card->card_flags & 2))) {
    string arg3_s = ce->def.effects[cond.card_definition_effect_index].arg3.decode();
    if (arg3_s.size() < 1) {
      throw runtime_error("card definition arg3 is missing");
    }
    auto target_cards = this->get_targeted_cards_for_condition(
        cond.card_ref,
        cond.card_definition_effect_index,
        cond.condition_giver_card_ref,
        as,
        atoi(arg3_s.c_str() + 1),
        0);
    for (auto c : target_cards) {
      if (c == card) {
        log.debug("targeted by p condition (=> true)");
        return true;
      }
    }
    log.debug("not targeted by p condition (=> false)");
    return false;
  } else {

    log.debug("SC check does not apply");
    return false;
  }
}

void CardSpecial::on_card_set(shared_ptr<PlayerState> ps, uint16_t card_ref) {
  auto sc_card = ps->get_sc_card();
  uint16_t sc_card_ref = sc_card ? sc_card->get_card_ref() : 0xFFFF;

  ActionState as;
  this->evaluate_and_apply_effects(EffectWhen::CARD_SET, card_ref, as, sc_card_ref);
}

const CardDefinition::Effect* CardSpecial::original_definition_for_condition(const Condition& cond) const {
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
    return this->card_has_condition_with_ref(card, ConditionType::ABILITY_TRAP, 0xFFFF, 0xFFFF);
  }
}

void CardSpecial::send_6xB4x06_for_exp_change(
    shared_ptr<const Card> card,
    uint16_t attacker_card_ref,
    uint8_t dice_roll_value,
    bool unknown_p5) const {
  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
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
  auto s = this->server();
  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(
      attacker_card_ref, 0x13);
  cmd.effect.target_card_ref = destroyed_card->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = s->options.is_nte() ? 0x78 : 0x7E;
  this->server()->send(cmd);
}

uint16_t CardSpecial::send_6xB4x06_if_card_ref_invalid(uint16_t card_ref, int16_t value) const {
  auto s = this->server();
  if (!s->options.is_nte() && !s->card_ref_is_empty_or_has_valid_card_id(card_ref)) {
    if (value != 0) {
      G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
      cmd.effect.flags = 0x04;
      cmd.effect.attacker_card_ref = 0xFFFF;
      cmd.effect.target_card_ref = 0xFFFF;
      cmd.effect.value = value;
      cmd.effect.operation = 0x7E;
      s->send(cmd);
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

  bool is_nte = this->server()->options.is_nte();
  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
  cmd.effect.flags = flags | 2;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card_ref, 10);
  cmd.effect.target_card_ref = card->get_card_ref();
  cmd.effect.value = -hp_delta;
  cmd.effect.ap = is_nte ? card->ap : clamp<int16_t>(card->ap, 0, 99);
  cmd.effect.current_hp = clamp<int16_t>(card->get_current_hp(), 0, 99);
  cmd.effect.tp = is_nte ? card->tp : clamp<int16_t>(card->tp, 0, 99);
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

  auto s = this->server();
  if (s->options.is_nte()) {
    if (this->card_has_condition_with_ref(card, ConditionType::ABILITY_TRAP, 0xFFFF, 0xFFFF)) {
      return true;
    }
  } else {
    if (card->get_definition()->def.is_sc() && (eff.type == ConditionType::FREEZE)) {
      return true;
    }
  }

  switch (eff.type) {
    case ConditionType::GUOM:
    case ConditionType::CURSE:
      if (s->options.is_nte()) {
        return false;
      }
      [[fallthrough]];
    case ConditionType::IMMOBILE:
    case ConditionType::HOLD:
    case ConditionType::PARALYZE:
    case ConditionType::ACID:
    case ConditionType::FREEZE:
    case ConditionType::DROP: {
      if (s->options.is_nte()) {
        return (card->find_condition(ConditionType::ANTI_ABNORMALITY_2) != nullptr);
      } else {
        const auto* cond = this->find_condition_with_parameters(card, ConditionType::ANTI_ABNORMALITY_2, 0xFFFF, 0xFF);
        return (cond != nullptr) || s->ruler_server->card_ref_is_boss_sc(card->get_card_ref());
      }
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
  auto log = this->server()->log_stack("sum_last_attack_damage: ");

  size_t damage_count = 0;
  auto check_card = [&](shared_ptr<const Card> c) -> void {
    if (c && (c->last_attack_final_damage > 0)) {
      log.debug("check_card @%04hX #%04hX => %hd", c->get_card_ref(), c->get_card_id(), c->last_attack_final_damage);
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
    for (size_t index_offset = 0; (index_offset + 1) < cond_indexes.size(); index_offset++) {
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

void CardSpecial::apply_effects_after_card_move(shared_ptr<Card> card) {
  ActionState as = this->create_attack_state_from_card_action_chain(card);

  bool is_nte = this->server()->options.is_nte();
  if (!is_nte) {
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
    this->apply_defense_conditions(as, EffectWhen::BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL, card, 0x04);
    this->evaluate_and_apply_effects(EffectWhen::BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL, card->get_card_ref(), as, 0xFFFF);
  }

  this->apply_defense_conditions(as, EffectWhen::AFTER_CARD_MOVE, card, is_nte ? 0x1F : 0x04);
  this->evaluate_and_apply_effects(EffectWhen::AFTER_CARD_MOVE, card->get_card_ref(), as, 0xFFFF);
}

void CardSpecial::check_for_defense_interference(
    shared_ptr<const Card> attacker_card,
    shared_ptr<Card> target_card,
    int16_t* inout_unknown_p4) {
  auto s = this->server();

  // Note: This check is not part of the original implementation.
  if (s->options.behavior_flags & BehaviorFlag::DISABLE_INTERFERENCE) {
    return;
  }

  if (!inout_unknown_p4) {
    return;
  }
  if (target_card->get_current_hp() > *inout_unknown_p4) {
    return;
  }

  uint16_t ally_sc_card_ref = s->ruler_server->get_ally_sc_card_ref(
      target_card->get_card_ref());
  if (ally_sc_card_ref == 0xFFFF) {
    return;
  }

  auto ally_sc = s->card_for_set_card_ref(ally_sc_card_ref);
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

  auto ally_hes = s->ruler_server->get_hand_and_equip_state_for_client_id(target_ally_client_id);
  if (!ally_hes || (!(s->options.behavior_flags & BehaviorFlag::ALLOW_NON_COM_INTERFERENCE) && !ally_hes->is_cpu_player)) {
    return;
  }

  uint16_t target_card_id = s->card_id_for_card_ref(target_card->get_card_ref());
  if (target_card_id == 0xFFFF) {
    return;
  }

  uint16_t ally_sc_card_id = s->card_id_for_card_ref(ally_sc_card_ref);
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
  if (!entry || (s->get_random(99) >= entry->defense_probability)) {
    return;
  }

  target_ps->unknown_a17++;

  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(attacker_card->get_card_ref(), 0x12);
  cmd.effect.target_card_ref = target_card->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = 0x7D;
  s->send(cmd);
  if (inout_unknown_p4) {
    *inout_unknown_p4 = 0;
    target_card->action_metadata.set_flags(0x10);
  }
}

void CardSpecial::evaluate_and_apply_effects(
    EffectWhen when,
    uint16_t set_card_ref,
    const ActionState& as,
    uint16_t sc_card_ref,
    bool apply_defense_condition_to_all_cards,
    uint16_t apply_defense_condition_to_card_ref) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("evaluate_and_apply_effects(%s, @%04hX, @%04hX): ", phosg::name_for_enum(when), set_card_ref, sc_card_ref));
  bool is_nte = s->options.is_nte();

  {
    string as_str = as.str(s);
    log.debug("when=%s, set_card_ref=@%04hX, as=%s, sc_card_ref=@%04hX, apply_defense_condition_to_all_cards=%s, apply_defense_condition_to_card_ref=@%04hX",
        phosg::name_for_enum(when), set_card_ref, as_str.c_str(), sc_card_ref, apply_defense_condition_to_all_cards ? "true" : "false", apply_defense_condition_to_card_ref);
  }

  if (!is_nte) {
    set_card_ref = this->send_6xB4x06_if_card_ref_invalid(set_card_ref, 1);
  }

  auto ce = this->server()->definition_for_card_ref(set_card_ref);
  if (!ce) {
    log.debug("ce missing");
    return;
  }

  if (is_nte) {
    auto set_card = s->card_for_set_card_ref(set_card_ref);
    if ((set_card != nullptr) && set_card->get_condition_value(ConditionType::ABILITY_TRAP)) {
      return;
    }
  }

  uint16_t as_attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 2);
  if (as_attacker_card_ref == 0xFFFF) {
    as_attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as.original_attacker_card_ref, 3);
  }

  G_ApplyConditionEffect_Ep3_6xB4x06 dice_cmd;
  dice_cmd.effect.target_card_ref = set_card_ref;
  bool as_action_card_refs_contains_set_card_ref = false;
  bool as_action_card_refs_contains_duplicate_of_set_card = false;
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    if (as.action_card_refs[z] == dice_cmd.effect.target_card_ref) {
      as_action_card_refs_contains_set_card_ref = true;
      break;
    }
    auto action_ce = s->definition_for_card_ref(as.action_card_refs[z]);
    if (action_ce && (action_ce->def.card_id == ce->def.card_id)) {
      as_action_card_refs_contains_duplicate_of_set_card = true;
    }
  }

  bool unknown_v1 = as_action_card_refs_contains_duplicate_of_set_card && as_action_card_refs_contains_set_card_ref;

  uint8_t random_percent = s->get_random(99);
  bool any_expr_used_dice_roll = false;

  DiceRoll dice_roll;
  uint8_t client_id = client_id_for_card_ref(dice_cmd.effect.target_card_ref);
  auto set_card_ps = (client_id == 0xFF) ? nullptr : s->player_states.at(client_id);

  dice_roll.value = 1;
  if (set_card_ps) {
    dice_roll.value = set_card_ps->roll_dice_with_effects(1);
  }
  dice_roll.client_id = client_id;
  dice_roll.unknown_a2 = 3;
  dice_roll.value_used_in_expr = false;

  log.debug("inputs: dice_roll=%02hhX, random_percent=%hhu, unknown_v1=%s", dice_roll.value, random_percent, unknown_v1 ? "true" : "false");

  for (size_t def_effect_index = 0; (def_effect_index < 3) && !unknown_v1 && (ce->def.effects[def_effect_index].type != ConditionType::NONE); def_effect_index++) {
    auto effect_log = log.sub(phosg::string_printf("(effect:%zu) ", def_effect_index));
    const auto& card_effect = ce->def.effects[def_effect_index];
    string card_effect_str = card_effect.str();
    effect_log.debug("effect: %s", card_effect_str.c_str());
    if (card_effect.when != when) {
      effect_log.debug("does not apply (effect.when=%s, when=%s)", phosg::name_for_enum(card_effect.when), phosg::name_for_enum(when));
      continue;
    }

    string arg3_s = card_effect.arg3.decode();
    if (arg3_s.size() < 1) {
      throw runtime_error("card effect arg3 is missing");
    }
    int16_t arg3_value = atoi(arg3_s.c_str() + 1);
    effect_log.debug("arg3_value=%hd", arg3_value);
    auto targeted_cards = this->get_targeted_cards_for_condition(
        set_card_ref, def_effect_index, sc_card_ref, as, arg3_value, 1);
    string refs_str = refs_str_for_cards_vector(targeted_cards);
    effect_log.debug("targeted_cards=[%s]", refs_str.c_str());
    bool all_targets_matched = false;
    if (!is_nte &&
        !targeted_cards.empty() &&
        ((card_effect.type == ConditionType::UNKNOWN_64) ||
            (card_effect.type == ConditionType::MISC_DEFENSE_BONUSES) ||
            (card_effect.type == ConditionType::MOSTLY_HALFGUARDS))) {
      effect_log.debug("special targeting applies");
      size_t count = 0;
      for (size_t z = 0; z < targeted_cards.size(); z++) {
        dice_roll.value_used_in_expr = false;
        string arg2_text = card_effect.arg2.decode();
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
        auto set_card = s->card_for_set_card_ref(set_card_ref);
        if (!set_card) {
          set_card = s->card_for_set_card_ref(sc_card_ref);
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
      auto target_log = effect_log.sub(phosg::string_printf("(target:@%04hX) ", targeted_cards[z]->get_card_ref()));
      dice_roll.value_used_in_expr = false;
      string arg2_str = card_effect.arg2.decode();
      target_log.debug("arg2_str = %s", arg2_str.c_str());
      if (all_targets_matched ||
          this->evaluate_effect_arg2_condition(
              as, targeted_cards[z], arg2_str.c_str(), dice_roll, set_card_ref, sc_card_ref, random_percent, when)) {
        target_log.debug("arg2 condition passed");
        auto env_stats = this->compute_attack_env_stats(as, targeted_cards[z], dice_roll, set_card_ref, sc_card_ref);
        string expr_str = card_effect.expr.decode();
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
          target_log.debug("target card (not replaced) = @%04hX", target_card->get_card_ref());
        } else {
          target_log.debug("target card (replaced) = @%04hX", target_card->get_card_ref());
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
          G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
          cmd.effect.flags = 0x04;
          cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as_attacker_card_ref, 0x14);
          cmd.effect.target_card_ref = target_card->get_card_ref();
          cmd.effect.value = is_nte ? 0 : target_card->action_chain.conditions[applied_cond_index].remaining_turns;
          cmd.effect.operation = static_cast<int8_t>(card_effect.type);
          s->send(cmd);

          // Note: The original code has this check outside of the if
          // (applied_cond_index >= 0) block, but this is a bug since
          // applied_cond_index can be negative. In Sega's implementation, this
          // bug probably does nothing in any reasonable scenario, since the
          // target card refs array immediately precedes the conditions array,
          // and the target card refs array is excessively long, so OR'ing a
          // value that is almost certainly already 0xFFFF with 1 would do
          // nothing. In our implementation, however, we bounds-check
          // everything, so we've moved this check inside the relevant if block.
          if (dice_roll.value_used_in_expr) {
            target_card->action_chain.conditions[applied_cond_index].flags |= 1;
          }

          if (apply_defense_condition_to_all_cards || (apply_defense_condition_to_card_ref == targeted_cards[z]->get_card_ref())) {
            this->apply_defense_condition(
                when, &target_card->action_chain.conditions[applied_cond_index], applied_cond_index, as, target_card, 4, 1);
            target_log.debug("applied defense condition");
          }
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
    dice_cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(as_attacker_card_ref, 0x15);
    dice_cmd.effect.dice_roll_value = dice_roll.value;
    s->send(dice_cmd);
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
          G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
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
  this->evaluate_and_apply_effects(EffectWhen::CARD_DESTROYED, destroyed_card_ref, defense_as, 0xFFFF);
  for (size_t z = 0; (z < 8) && (defense_as.action_card_refs[z] != 0xFFFF); z++) {
    this->evaluate_and_apply_effects(
        EffectWhen::CARD_DESTROYED, defense_as.action_card_refs[z], defense_as, destroyed_card->get_card_ref());
  }

  if (attacker_card) {
    for (size_t cond_index = 0; cond_index < 9; cond_index++) {
      auto& cond = attacker_card->action_chain.conditions[cond_index];
      if (cond.type == ConditionType::CURSE) {
        bool is_nte = this->server()->options.is_nte();
        this->execute_effect(cond, attacker_card, 0, 0, ConditionType::CURSE, is_nte ? 0x1F : 0x04, 0xFFFF);
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
  auto log = this->server()->log_stack("filter_cards_by_range: ");

  if (log.should_log(phosg::LogLevel::DEBUG)) {
    auto card1_str = card1 ? phosg::string_printf("@%04hX #%04hX", card1->get_card_ref(), card1->get_card_id()) : "null";
    auto card2_str = card2 ? phosg::string_printf("@%04hX #%04hX", card2->get_card_ref(), card2->get_card_id()) : "null";
    auto loc_str = card1_loc.str();
    log.debug("card1=(%s), card2=(%s), loc=%s", card1_str.c_str(), card2_str.c_str(), loc_str.c_str());

    for (const auto& card : cards) {
      if (card) {
        log.debug("input card: @%04hX #%04hX", card->get_card_ref(), card->get_card_id());
      } else {
        log.debug("input card: null");
      }
    }
  }

  vector<shared_ptr<const Card>> ret;
  if (!card1 || cards.empty()) {
    log.debug("card1 missing or input list is blank");
    return ret;
  }

  auto ps = card1->player_state();
  if (!ps) {
    log.debug("ps is missing");
    return ret;
  }

  // TODO: Remove hardcoded card ID here (Earthquake)
  uint16_t card_id = this->get_card_id_with_effective_range(card1, 0x00ED, card2);
  log.debug("card_id = #%04hX", card_id);

  parray<uint8_t, 9 * 9> range;
  compute_effective_range(range, this->server()->options.card_index, card_id, card1_loc, this->server()->map_and_rules);
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    auto loc_str = card1_loc.str();
    log.debug("compute_effective_range(range, ci, #%04hX, %s, map) =>", card_id, loc_str.c_str());
    for (size_t y = 0; y < 9; y++) {
      const uint8_t* row = &range[y * 9];
      log.debug("  range[%zu] = %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX",
          y, row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
    }
  }

  auto card_refs_in_range = ps->get_card_refs_within_range_from_all_players(range, card1_loc, CardType::ITEM);
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    for (uint16_t card_ref : card_refs_in_range) {
      log.debug("ref in range: @%04hX", card_ref);
    }
  }

  for (auto card : cards) {
    if (!card || (card->get_card_ref() == 0xFFFF)) {
      if (card) {
        log.debug("(@%04hX #%04hX) out of range", card->get_card_ref(), card->get_card_id());
      } else {
        log.debug("(null) card missing");
      }
      continue;
    }
    for (uint16_t card_ref_in_range : card_refs_in_range) {
      if (card_ref_in_range == card->get_card_ref()) {
        log.debug("(@%04hX #%04hX) in range", card->get_card_ref(), card->get_card_id());
        ret.emplace_back(card);
        break;
      }
    }
    log.debug("(@%04hX #%04hX) out of range", card->get_card_ref(), card->get_card_id());
  }
  return ret;
}

void CardSpecial::apply_effects_after_attack_target_resolution(const ActionState& as) {
  auto s = this->server();
  auto log = s->log_stack("apply_effects_after_attack_target_resolution: ");
  string as_str = as.str(s);
  log.debug("as=%s", as_str.c_str());

  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(as.action_card_refs[z], 0x1E);
    if (card_ref == 0xFFFF) {
      break;
    }

    if (this->send_6xB4x06_if_card_ref_invalid(as.original_attacker_card_ref, 0x1F) == 0xFFFF) {
      this->evaluate_and_apply_effects(
          EffectWhen::CARD_SET,
          as.action_card_refs[z],
          as,
          this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x21));
      this->evaluate_and_apply_effects(
          EffectWhen::AFTER_ATTACK_TARGET_RESOLUTION,
          as.action_card_refs[z],
          as,
          this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x22));
    } else {
      uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(as.target_card_refs[0], 0x20);
      if (card_ref != 0xFFFF) {
        this->evaluate_and_apply_effects(EffectWhen::CARD_SET, as.action_card_refs[z], as, card_ref);
        this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_15, as.action_card_refs[z], as, card_ref);
      }
    }
  }

  if (as.original_attacker_card_ref == 0xFFFF) {
    uint16_t card_ref1 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x23);
    uint16_t card_ref2 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x25);
    this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_33, card_ref2, as, card_ref1);
    card_ref1 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x24);
    card_ref2 = this->send_6xB4x06_if_card_ref_invalid(as.attacker_card_ref, 0x26);
    this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_34, card_ref2, as, card_ref1);
    for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
      uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(as.action_card_refs[z], 0x27);
      if (card_ref == 0xFFFF) {
        break;
      }
      this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_35, as.target_card_refs[z], as, as.attacker_card_ref);
    }
  }
}

void CardSpecial::move_phase_before_for_card(shared_ptr<Card> card) {
  bool is_nte = this->server()->options.is_nte();
  ActionState as = this->create_attack_state_from_card_action_chain(card);
  this->apply_defense_conditions(as, EffectWhen::BEFORE_MOVE_PHASE, card, is_nte ? 0x1F : 0x04);
  this->evaluate_and_apply_effects(EffectWhen::BEFORE_MOVE_PHASE, card->get_card_ref(), as, 0xFFFF);
  if (!is_nte) {
    this->apply_defense_conditions(as, EffectWhen::BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL, card, 0x04);
    this->evaluate_and_apply_effects(EffectWhen::BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL, card->get_card_ref(), as, 0xFFFF);
  }
}

void CardSpecial::dice_phase_before_for_card(shared_ptr<Card> card) {
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  auto ps = card->player_state();
  if (is_nte && (!ps || !ps->is_team_turn())) {
    return;
  }

  ActionState as;
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

  if (!is_nte) {
    this->apply_defense_conditions(as, EffectWhen::BEFORE_DICE_PHASE_ALL_TURNS_FINAL, card, 0x04);
    this->evaluate_and_apply_effects(EffectWhen::BEFORE_DICE_PHASE_ALL_TURNS_FINAL, card->get_card_ref(), as, sc_card_ref);
  }
  if (ps->is_team_turn()) {
    this->apply_defense_conditions(as, EffectWhen::BEFORE_DICE_PHASE_THIS_TEAM_TURN, card, 0x04);
    this->evaluate_and_apply_effects(EffectWhen::BEFORE_DICE_PHASE_THIS_TEAM_TURN, card->get_card_ref(), as, sc_card_ref);
  }
}

template <EffectWhen When1, EffectWhen When2>
void CardSpecial::apply_effects_on_phase_change_t(shared_ptr<Card> unknown_p2, const ActionState* existing_as) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("apply_effects_on_phase_change_t<%s, %s>(@%04hX #%04hX): ", phosg::name_for_enum(When1), phosg::name_for_enum(When2), unknown_p2->get_card_ref(), unknown_p2->get_card_id()));
  bool is_nte = s->options.is_nte();

  ActionState as;
  if (!existing_as) {
    as = this->create_attack_state_from_card_action_chain(unknown_p2);
  } else {
    as = *existing_as;
  }

  this->apply_defense_conditions(as, When1, unknown_p2, is_nte ? 0x1F : 0x04);
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = s->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->apply_defense_conditions(target_as, When1, card, is_nte ? 0x1F : 0x04);
    }
  }
  auto card = this->sc_card_for_card(unknown_p2);
  this->evaluate_and_apply_effects(When1, unknown_p2->get_card_ref(), as, card ? card->get_card_ref() : 0xFFFF);
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->evaluate_and_apply_effects(When1, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = s->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(
          unknown_p2, card);
      this->evaluate_and_apply_effects(When2, as.target_card_refs[z], target_as, unknown_p2->get_card_ref());
      for (size_t w = 0; (w < 8) && (target_as.action_card_refs[w] != 0xFFFF); w++) {
        this->evaluate_and_apply_effects(When1, target_as.action_card_refs[w], target_as, card->get_card_ref());
      }
    }
  }
}

void CardSpecial::draw_phase_before_for_card(shared_ptr<Card> unknown_p2) {
  this->apply_effects_on_phase_change_t<EffectWhen::BEFORE_DRAW_PHASE, EffectWhen::UNKNOWN_0A>(unknown_p2);
}

void CardSpecial::action_phase_before_for_card(shared_ptr<Card> unknown_p2) {
  if (unknown_p2->player_state()->is_team_turn()) {
    this->apply_effects_on_phase_change_t<EffectWhen::BEFORE_ACT_PHASE, EffectWhen::UNKNOWN_0A>(unknown_p2);
  }
}

void CardSpecial::unknown_8024945C(shared_ptr<Card> unknown_p2, const ActionState* existing_as) {
  this->apply_effects_on_phase_change_t<EffectWhen::UNKNOWN_0A, EffectWhen::UNKNOWN_0A>(unknown_p2, this->server()->options.is_nte() ? nullptr : existing_as);
}

void CardSpecial::unknown_8024966C(shared_ptr<Card> unknown_p2, const ActionState* existing_as) {
  auto log = this->server()->log_stack(phosg::string_printf("unknown_8024966C(@%04hX #%04hX): ", unknown_p2->get_card_ref(), unknown_p2->get_card_id()));

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

  this->apply_defense_conditions(as, EffectWhen::ATTACK_STAT_OVERRIDES, unknown_p2, 4);
  this->apply_defense_conditions(as, EffectWhen::ATTACK_DAMAGE_ADJUSTMENT, unknown_p2, 4);
  if (defender_card) {
    this->apply_defense_conditions(as, EffectWhen::UNKNOWN_22, defender_card, 4);
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState defense_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->apply_defense_conditions(defense_as, EffectWhen::ATTACK_STAT_OVERRIDES, card, 4);
      this->apply_defense_conditions(defense_as, EffectWhen::DEFENSE_DAMAGE_ADJUSTMENT, card, 4);
    }
  }

  this->evaluate_and_apply_effects(EffectWhen::ATTACK_STAT_OVERRIDES, unknown_p2->get_card_ref(), as, card_ref);
  this->evaluate_and_apply_effects(EffectWhen::ATTACK_DAMAGE_ADJUSTMENT, unknown_p2->get_card_ref(), as, card_ref);
  if (defender_card) {
    this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_22, defender_card->get_card_ref(), as, card_ref);
  }

  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->evaluate_and_apply_effects(EffectWhen::ATTACK_STAT_OVERRIDES, as.action_card_refs[z], as, unknown_p2->get_card_ref());
    this->evaluate_and_apply_effects(EffectWhen::ATTACK_DAMAGE_ADJUSTMENT, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    card = this->server()->card_for_set_card_ref(as.target_card_refs[z]);
    if (card) {
      ActionState defense_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, card);
      this->evaluate_and_apply_effects(EffectWhen::ATTACK_STAT_OVERRIDES, card->get_card_ref(), defense_as, unknown_p2->get_card_ref());
      this->evaluate_and_apply_effects(EffectWhen::DEFENSE_DAMAGE_ADJUSTMENT, card->get_card_ref(), defense_as, unknown_p2->get_card_ref());
    }
  }
}

shared_ptr<Card> CardSpecial::sc_card_for_card(shared_ptr<Card> unknown_p2) {
  auto ps = unknown_p2->player_state();
  return ps ? ps->get_sc_card() : nullptr;
}

void CardSpecial::unknown_8024A9D8(const ActionState& pa, uint16_t action_card_ref) {
  for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    if (this->server()->options.is_nte() || (action_card_ref == 0xFFFF) || (action_card_ref == pa.action_card_refs[z])) {
      if (pa.original_attacker_card_ref == 0xFFFF) {
        this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_29, pa.action_card_refs[z], pa, pa.attacker_card_ref);
        this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_2A, pa.action_card_refs[z], pa, pa.attacker_card_ref);
      } else {
        this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_29, pa.action_card_refs[z], pa, pa.target_card_refs[0]);
        this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_2B, pa.action_card_refs[z], pa, pa.target_card_refs[0]);
      }
    }
  }
}

void CardSpecial::check_for_attack_interference(shared_ptr<Card> unknown_p2) {
  // Note: This check is not part of the original implementation.
  if (this->server()->options.behavior_flags & BehaviorFlag::DISABLE_INTERFERENCE) {
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
  if (!ally_hes || (!(this->server()->options.behavior_flags & BehaviorFlag::ALLOW_NON_COM_INTERFERENCE) && !ally_hes->is_cpu_player)) {
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

  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x04;
  cmd.effect.attacker_card_ref = this->send_6xB4x06_if_card_ref_invalid(unknown_p2->get_card_ref(), 0x11);
  cmd.effect.target_card_ref = unknown_p2->get_card_ref();
  cmd.effect.value = 0;
  cmd.effect.operation = 0x7D;
  this->server()->send(cmd);
}

template <
    EffectWhen WhenAllCards,
    EffectWhen WhenAttackerAndActionCards,
    EffectWhen WhenAttackerOrHunterSCCard,
    EffectWhen WhenTargetsAndActionCards>
void CardSpecial::apply_effects_before_or_after_attack(shared_ptr<Card> unknown_p2) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("apply_effects_before_or_after_attack<%s, %s, %s, %s>(@%04hX #%04hX): ",
      phosg::name_for_enum(WhenAllCards), phosg::name_for_enum(WhenAttackerAndActionCards), phosg::name_for_enum(WhenAttackerOrHunterSCCard), phosg::name_for_enum(WhenTargetsAndActionCards), unknown_p2->get_card_ref(), unknown_p2->get_card_id()));

  ActionState as = this->create_attack_state_from_card_action_chain(unknown_p2);

  auto sc_card = this->sc_card_for_card(unknown_p2);
  uint16_t sc_card_ref = 0xFFFF;
  if (sc_card) {
    sc_card_ref = sc_card->get_card_ref();
  }

  auto attacker_card = unknown_p2;
  if (unknown_p2->get_definition() && (unknown_p2->get_definition()->def.type == CardType::ITEM) && sc_card) {
    attacker_card = sc_card;
  }

  uint8_t apply_defense_conditions_flags = s->options.is_nte() ? 0x1F : 0x04;
  this->apply_defense_conditions(as, WhenAllCards, unknown_p2, apply_defense_conditions_flags);
  this->apply_defense_conditions(as, WhenAttackerAndActionCards, unknown_p2, apply_defense_conditions_flags);
  if (attacker_card) {
    this->apply_defense_conditions(as, WhenAttackerOrHunterSCCard, attacker_card, apply_defense_conditions_flags);
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto set_card = s->card_for_set_card_ref(as.target_card_refs[z]);
    if (set_card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, set_card);
      this->apply_defense_conditions(target_as, WhenAllCards, set_card, apply_defense_conditions_flags);
      this->apply_defense_conditions(target_as, WhenTargetsAndActionCards, set_card, apply_defense_conditions_flags);
    }
  }

  this->evaluate_and_apply_effects(WhenAllCards, unknown_p2->get_card_ref(), as, sc_card_ref);
  this->evaluate_and_apply_effects(WhenAttackerAndActionCards, unknown_p2->get_card_ref(), as, sc_card_ref);
  if (attacker_card) {
    this->evaluate_and_apply_effects(WhenAttackerOrHunterSCCard, attacker_card->get_card_ref(), as, sc_card_ref);
  }
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->evaluate_and_apply_effects(WhenAllCards, as.action_card_refs[z], as, unknown_p2->get_card_ref());
    this->evaluate_and_apply_effects(WhenAttackerAndActionCards, as.action_card_refs[z], as, unknown_p2->get_card_ref());
  }
  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    auto set_card = s->card_for_set_card_ref(as.target_card_refs[z]);
    if (set_card) {
      ActionState target_as = this->create_defense_state_for_card_pair_action_chains(unknown_p2, set_card);
      this->evaluate_and_apply_effects(WhenAllCards, set_card->get_card_ref(), target_as, unknown_p2->get_card_ref());
      this->evaluate_and_apply_effects(WhenTargetsAndActionCards, set_card->get_card_ref(), target_as, unknown_p2->get_card_ref());
      for (size_t z = 0; (z < 8) && (target_as.action_card_refs[z] != 0xFFFF); z++) {
        this->evaluate_and_apply_effects(WhenAllCards, target_as.action_card_refs[z], target_as, set_card->get_card_ref());
        this->evaluate_and_apply_effects(WhenTargetsAndActionCards, target_as.action_card_refs[z], target_as, set_card->get_card_ref());
      }
    }
  }
}

void CardSpecial::apply_effects_after_attack(shared_ptr<Card> card) {
  return this->apply_effects_before_or_after_attack<
      EffectWhen::AFTER_ANY_CARD_ATTACK,
      EffectWhen::AFTER_THIS_CARD_ATTACK,
      EffectWhen::AFTER_CREATURE_OR_HUNTER_SC_ATTACK,
      EffectWhen::AFTER_THIS_CARD_ATTACKED>(card);
}

void CardSpecial::apply_effects_before_attack(shared_ptr<Card> card) {
  return this->apply_effects_before_or_after_attack<
      EffectWhen::BEFORE_ANY_CARD_ATTACK,
      EffectWhen::BEFORE_THIS_CARD_ATTACK,
      EffectWhen::BEFORE_CREATURE_OR_HUNTER_SC_ATTACK,
      EffectWhen::BEFORE_THIS_CARD_ATTACKED>(card);
}

bool CardSpecial::client_has_atk_dice_boost_condition(uint8_t client_id) {
  auto s = this->server();
  bool is_nte = s->options.is_nte();
  auto ps = s->get_player_state(client_id);

  if (ps) {
    auto card = ps->get_sc_card();
    if (card) {
      for (size_t z = 0; z < 9; z++) {
        if ((is_nte || !this->card_ref_has_ability_trap(card->action_chain.conditions[z])) &&
            (card->action_chain.conditions[z].type == ConditionType::ATK_DICE_BOOST)) {
          return true;
        }
      }
    }
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = ps->get_set_card(set_index);
      if (card) {
        for (size_t z = 0; z < 9; z++) {
          if ((is_nte || !this->card_ref_has_ability_trap(card->action_chain.conditions[z])) &&
              (card->action_chain.conditions[z].type == ConditionType::ATK_DICE_BOOST)) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void CardSpecial::unknown_8024A6DC(shared_ptr<Card> unknown_p2, shared_ptr<Card> unknown_p3) {
  ActionState as = this->create_defense_state_for_card_pair_action_chains(
      unknown_p2, unknown_p3);
  for (size_t z = 0; (z < 8) && (as.action_card_refs[z] != 0xFFFF); z++) {
    this->evaluate_and_apply_effects(EffectWhen::CARD_SET, as.action_card_refs[z], as, unknown_p3->get_card_ref());
    this->evaluate_and_apply_effects(EffectWhen::UNKNOWN_15, as.action_card_refs[z], as, unknown_p3->get_card_ref());
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
