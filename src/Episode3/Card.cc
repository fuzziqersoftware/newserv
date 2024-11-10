#include "Card.hh"

#include "../CommandFormats.hh"
#include "Server.hh"

using namespace std;

namespace Episode3 {

Card::Card(
    uint16_t card_id,
    uint16_t card_ref,
    uint16_t client_id,
    shared_ptr<Server> server)
    : w_server(server),
      w_player_state(server->get_player_state(client_id)),
      client_id(client_id),
      card_id(card_id),
      card_ref(card_ref),
      card_flags(0),
      loc(0, 0, Direction::RIGHT),
      facing_direction(Direction::INVALID_FF),
      action_chain(),
      action_metadata(),
      num_ally_fcs_destroyed_at_set_time(0),
      num_cards_destroyed_by_team_at_set_time(0),
      unknown_a9(1),
      last_attack_preliminary_damage(0),
      last_attack_final_damage(0),
      num_destroyed_ally_fcs(0),
      current_defense_power(0) {}

void Card::init() {
  auto s = this->server();
  auto ps = this->player_state();

  this->clear_action_chain_and_metadata_and_most_flags();
  this->team_id = ps->get_team_id();
  this->def_entry = s->definition_for_card_id(this->card_id);
  if (!this->def_entry) {
    // The original implementation replaces the card ID and definition with 0009
    // (Saber) if the SC is Hunters-side, and 0056 (Booma) if the SC is
    // Arkz-side. This could break things later on in the battle, and even if it
    // doesn't, it certainly isn't behavior that the player would expect, so we
    // prevent it instead.
    throw runtime_error("card definition is missing");
  }
  this->sc_card_ref = ps->get_sc_card_ref();
  this->sc_def_entry = s->definition_for_card_id(ps->get_sc_card_id());
  this->sc_card_type = ps->get_sc_card_type();
  if (this->sc_card_ref == this->card_ref) {
    if (s->options.is_nte()) {
      if (s->map_and_rules->rules.char_hp) {
        this->max_hp = s->map_and_rules->rules.char_hp;
        this->current_hp = s->map_and_rules->rules.char_hp;
      } else {
        this->max_hp = this->def_entry->def.hp.stat;
        this->current_hp = this->def_entry->def.hp.stat;
      }
    } else {
      int16_t rules_char_hp = s->map_and_rules->rules.char_hp;
      int16_t base_char_hp = (rules_char_hp == 0) ? 15 : rules_char_hp;
      int16_t hp = clamp<int16_t>(base_char_hp + this->def_entry->def.hp.stat, 1, 99);
      this->max_hp = hp;
      this->current_hp = hp;
    }
  } else {
    this->max_hp = this->def_entry->def.hp.stat;
    this->current_hp = this->def_entry->def.hp.stat;
  }
  this->ap = this->def_entry->def.ap.stat;
  this->tp = this->def_entry->def.tp.stat;
  this->num_ally_fcs_destroyed_at_set_time = s->team_num_ally_fcs_destroyed[this->team_id];
  this->num_cards_destroyed_by_team_at_set_time = s->team_num_cards_destroyed[this->team_id];
  this->action_chain.chain.card_ap = this->ap;
  this->action_chain.chain.card_tp = this->tp;
  this->loc.direction = ps->start_facing_direction;
  // Ep3 NTE always sends 6xB4x0A at construction time; final only does for
  // non-SC cards
  if (s->options.is_nte() || (this->sc_card_ref != this->card_ref)) {
    this->send_6xB4x4E_4C_4D_if_needed();
  }
}

shared_ptr<Server> Card::server() {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const Server> Card::server() const {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<PlayerState> Card::player_state() {
  auto s = this->w_player_state.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const PlayerState> Card::player_state() const {
  auto s = this->w_player_state.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

ssize_t Card::apply_abnormal_condition(
    const CardDefinition::Effect& eff,
    uint8_t def_effect_index,
    uint16_t target_card_ref,
    uint16_t sc_card_ref,
    int16_t value,
    int8_t dice_roll_value,
    int8_t random_percent) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("apply_abnormal_condition(%02hhX, @%04X, @%04X, %hd, %hhd, %hhd): ", def_effect_index, target_card_ref, sc_card_ref, value, dice_roll_value, random_percent));
  bool is_nte = s->options.is_nte();

  ssize_t existing_cond_index;
  for (size_t z = 0; z < this->action_chain.conditions.size(); z++) {
    const auto& cond = this->action_chain.conditions[z];
    if (cond.type == eff.type) {
      existing_cond_index = z;
      if ((!is_nte && eff.type == ConditionType::MV_BONUS) ||
          ((cond.card_definition_effect_index == def_effect_index) &&
              (cond.card_ref == target_card_ref))) {
        break;
      }
    } else {
      existing_cond_index = -1;
    }
  }

  ssize_t cond_index = existing_cond_index;
  if (existing_cond_index < 0) {
    cond_index = existing_cond_index;
    for (size_t z = 0; z < this->action_chain.conditions.size(); z++) {
      if (this->action_chain.conditions[z].type == ConditionType::NONE) {
        cond_index = z;
        break;
      }
    }
    log.debug("existing_cond_index < 0 (new condition) => cond_index = %zd", cond_index);
  } else {
    log.debug("existing_cond_index = %zd (existing condition)", existing_cond_index);
  }

  if (cond_index < 0) {
    log.debug("no space for condition");
    return -1;
  }

  int16_t existing_cond_value = 0;
  auto& cond = this->action_chain.conditions[cond_index];
  if ((eff.type == ConditionType::MV_BONUS) && (cond.type == ConditionType::MV_BONUS)) {
    existing_cond_value = clamp<int16_t>(cond.value, -99, 99);
    log.debug("MV_BONUS combines => existing_cond_value = %hd", existing_cond_value);
  }

  s->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, this->shared_from_this());
  cond.type = eff.type;
  cond.card_ref = target_card_ref;
  cond.condition_giver_card_ref = sc_card_ref;
  cond.card_definition_effect_index = def_effect_index;
  cond.order = 10;
  if (dice_roll_value < 0) {
    cond.dice_roll_value = this->player_state()->roll_dice_with_effects(1);
  } else {
    cond.dice_roll_value = dice_roll_value;
  }
  cond.flags = 0;
  cond.value = value + existing_cond_value;
  cond.value8 = value + existing_cond_value;
  cond.random_percent = random_percent;

  switch (eff.arg1.at(0)) {
    case 'a': {
      string s = eff.arg1.decode();
      cond.a_arg_value = atoi(s.c_str() + 1);
      break;
    }
    case 'e':
      cond.remaining_turns = 99;
      break;
    case 'f':
      cond.remaining_turns = 100;
      break;
    case 'r':
      cond.remaining_turns = 102;
      break;
    case 't': {
      string s = eff.arg1.decode();
      cond.remaining_turns = atoi(s.c_str() + 1);
    }
  }

  string cond_str = cond.str(s);
  log.debug("wrote condition %zd => %s", cond_index, cond_str.c_str());

  if (!is_nte) {
    s->card_special->update_condition_orders(this->shared_from_this());
    for (size_t z = 0; z < this->action_chain.conditions.size(); z++) {
      if (this->action_chain.conditions[z].type == ConditionType::NONE) {
        continue;
      }
      string cond_str = cond.str(s);
      log.debug("sorted conditions: [%zu] => %s", z, cond_str.c_str());
    }
  }

  return cond_index;
}

void Card::apply_ap_and_tp_adjust_assists_to_attack(
    shared_ptr<const Card> attacker_card,
    int16_t* inout_attacker_ap,
    int16_t* in_defense_power,
    int16_t* inout_attacker_tp) const {
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  uint8_t client_id = attacker_card->get_client_id();
  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    switch (s->assist_server->get_active_assist_by_index(z)) {
      case AssistEffect::POWERLESS_RAIN:
        if (is_nte) {
          *inout_attacker_ap = max<int16_t>(*inout_attacker_ap - 2, 0);
        }
        break;
      case AssistEffect::BRAVE_WIND:
        if (is_nte) {
          *inout_attacker_ap = max<int16_t>(*inout_attacker_ap + 2, 0);
        }
        break;
      case AssistEffect::SILENT_COLOSSEUM:
        if (*inout_attacker_ap - *in_defense_power >= 7) {
          *inout_attacker_ap = 0;
        }
        break;
      case AssistEffect::INFLUENCE:
        if (is_nte) {
          *inout_attacker_ap += attacker_card->player_state()->count_set_cards_for_env_stats_nte();
        }
        break;
      case AssistEffect::FIX:
        if (!is_nte && attacker_card && !attacker_card->def_entry->def.is_sc()) {
          *inout_attacker_ap = 2;
        }
        break;
      default:
        break;
    }
  }

  num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = s->assist_server->get_active_assist_by_index(z);
    if (eff == AssistEffect::AP_ABSORPTION) {
      if (is_nte) {
        if (attacker_card->action_chain.chain.attack_medium == AttackMedium::TECH) {
          *inout_attacker_ap = *inout_attacker_ap + 2;
        } else if (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
          *inout_attacker_tp = *inout_attacker_ap;
          *inout_attacker_ap = 0;
        }
      } else if (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
        *inout_attacker_ap = 0;
      }
    }
  }
}

bool Card::card_type_is_sc_or_creature() const {
  return (this->def_entry->def.type == CardType::HUNTERS_SC) ||
      (this->def_entry->def.type == CardType::ARKZ_SC) ||
      (this->def_entry->def.type == CardType::CREATURE);
}

bool Card::check_card_flag(uint32_t flags) const {
  return this->card_flags & flags;
}

void Card::commit_attack(
    int16_t damage,
    shared_ptr<Card> attacker_card,
    G_ApplyConditionEffect_Ep3_6xB4x06* cmd,
    size_t strike_number,
    int16_t* out_effective_damage) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("commit_attack(@%04hX #%04hX, @%04hX #%04hX => %hd (str%zu)): ", this->get_card_ref(), this->get_card_id(), attacker_card->get_card_ref(), attacker_card->get_card_id(), damage, strike_number));
  bool is_nte = s->options.is_nte();

  int16_t effective_damage = damage;
  s->card_special->adjust_attack_damage_due_to_conditions(
      this->shared_from_this(), &effective_damage, attacker_card->get_card_ref());
  log.debug("adjusted damage = %hd", effective_damage);

  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = s->assist_server->get_active_assist_by_index(z);
    if ((eff == AssistEffect::RANSOM) &&
        (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
      uint8_t team_id = this->player_state()->get_team_id();
      int16_t exp_amount = clamp<int16_t>(s->team_exp[team_id], 0, effective_damage);
      s->team_exp[team_id] -= exp_amount;
      effective_damage -= exp_amount;
      if (!is_nte) {
        s->compute_team_dice_bonus(team_id);
        s->update_battle_state_flags_and_send_6xB4x03_if_needed();
        if (cmd) {
          cmd->effect.ap += exp_amount;
        }
      }
    }
  }
  log.debug("after assists = %hd", effective_damage);

  if (this->action_metadata.check_flag(0x10)) {
    effective_damage = 0;
    log.debug("flag 0x10 => effective damage = %hd", effective_damage);
  }

  auto attacker_ps = attacker_card->player_state();
  attacker_ps->stats.damage_given += effective_damage;
  this->player_state()->stats.damage_taken += effective_damage;
  log.debug("updated stats");

  this->current_hp = clamp<int16_t>(this->current_hp - effective_damage, 0, this->max_hp);
  log.debug("hp set to %hd", this->current_hp);

  if ((effective_damage > 0) &&
      (attacker_ps->stats.max_attack_damage < effective_damage)) {
    attacker_ps->stats.max_attack_damage = effective_damage;
    log.debug("attacker new max damage %hd", effective_damage);
  }

  this->last_attack_final_damage = effective_damage;
  log.debug("last attack final damage = %hd", effective_damage);
  if (effective_damage > 0) {
    this->card_flags = this->card_flags | 4;
    log.debug("set flag 4");
  }
  if (this->current_hp < 1) {
    this->destroy_set_card(attacker_card);
    log.debug("card destroyed");
  }

  G_ApplyConditionEffect_Ep3_6xB4x06 cmd_to_send;
  if (cmd) {
    cmd_to_send = *cmd;
  }
  cmd_to_send.effect.flags = (strike_number == 0) ? 0x11 : 0x01;
  cmd_to_send.effect.attacker_card_ref = attacker_card->card_ref;
  cmd_to_send.effect.target_card_ref = this->card_ref;
  cmd_to_send.effect.value = effective_damage;
  s->send(cmd_to_send);

  this->propagate_shared_hp_if_needed();

  if (!is_nte && this->def_entry->def.is_sc()) {
    this->player_state()->stats.sc_damage_taken += effective_damage;
  }

  if (out_effective_damage) {
    *out_effective_damage = effective_damage;
  }
}

int16_t Card::compute_defense_power_for_attacker_card(shared_ptr<const Card> attacker_card) {
  if (!attacker_card) {
    return 0;
  }

  this->action_metadata.defense_power = 0;
  this->action_metadata.defense_bonus = 0;

  auto s = this->server();
  for (size_t z = 0; z < this->action_metadata.defense_card_ref_count; z++) {
    if (attacker_card->card_ref != this->action_metadata.original_attacker_card_refs[z]) {
      continue;
    }
    auto ce = s->definition_for_card_ref(this->action_metadata.defense_card_refs[z]);
    if (ce) {
      this->action_metadata.defense_power += ce->def.hp.stat;
    }
  }

  s->card_special->apply_action_conditions(EffectWhen::BEFORE_ANY_CARD_ATTACK, attacker_card, this->shared_from_this(), 0x08, nullptr);
  s->card_special->apply_action_conditions(EffectWhen::BEFORE_ANY_CARD_ATTACK, attacker_card, this->shared_from_this(), 0x10, nullptr);
  return this->action_metadata.defense_power + this->action_metadata.defense_bonus;
}

void Card::destroy_set_card(shared_ptr<Card> attacker_card) {
  auto s = this->server();
  auto ps = this->player_state();

  this->current_hp = 0;
  if (!(this->card_flags & 2)) {
    if (!s->ruler_server->card_ref_or_any_set_card_has_condition_46(this->card_ref)) {
      s->card_special->on_card_destroyed(attacker_card, this->shared_from_this());

      this->card_flags = this->card_flags | 2;
      this->update_stats_on_destruction();
      ps->stats.num_owned_cards_destroyed++;

      if (attacker_card && (attacker_card->team_id != this->team_id)) {
        attacker_card->player_state()->stats.num_opponent_cards_destroyed++;
        s->add_team_exp(this->team_id ^ 1, 3);
      }

      if ((this->sc_card_type == CardType::HUNTERS_SC) && (this->def_entry->def.type == CardType::ITEM)) {
        auto sc_card = ps->get_sc_card();
        if (!(sc_card->card_flags & 2) && !sc_card->get_condition_value(ConditionType::ELUDE)) {
          int16_t hp = sc_card->get_current_hp();
          sc_card->set_current_hp(hp - 1);
          sc_card->player_state()->stats.sc_damage_taken++;
          if (attacker_card && (attacker_card->team_id != this->team_id)) {
            G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
            cmd.effect.flags = 0x41;
            cmd.effect.attacker_card_ref = attacker_card->card_ref;
            cmd.effect.target_card_ref = sc_card->card_ref;
            cmd.effect.value = 1;
            s->send(cmd);
          }
          if (sc_card->get_current_hp() < 1) {
            sc_card->destroy_set_card(attacker_card);
          }
        }
      }

      if ((s->map_and_rules->rules.hp_type == HPType::DEFEAT_TEAM) &&
          (ps->get_sc_card().get() == this)) {
        for (size_t set_index = 0; set_index < 8; set_index++) {
          auto card = ps->get_set_card(set_index);
          if (card) {
            card->card_flags |= 2;
          }
        }
      }

      for (size_t client_id = 0; client_id < 4; client_id++) {
        if (!s->player_states[client_id]) {
          continue;
        }
        size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(client_id);
        for (size_t z = 0; z < num_assists; z++) {
          auto eff = s->assist_server->get_active_assist_by_index(z);
          if (eff == AssistEffect::HOMESICK) {
            if (client_id == this->client_id) {
              ps->return_set_card_to_hand2(this->card_ref);
            }
          } else if (eff == AssistEffect::INHERITANCE) {
            uint8_t other_team_id = s->player_states[client_id]->get_team_id();
            uint8_t this_team_id = ps->get_team_id();
            if (this_team_id == other_team_id) {
              s->add_team_exp(team_id, this->max_hp);
            }
          }
        }
      }

    } else if (!this->w_destroyer_sc_card.lock() && attacker_card) {
      this->w_destroyer_sc_card = attacker_card->player_state()->get_sc_card();
    }
  }
}

int32_t Card::error_code_for_move_to_location(const Location& loc) const {
  // TODO: NTE has different logic here, which appears to be similar enough to
  // the final logic that I didn't bother to reverse-engineer it completely.
  // Eventually, we should revisit this, but I suspect doing so would be a lot
  // of tedium for little to no benefit.

  if (this->player_state()->assist_flags & AssistFlag::IS_SKIPPING_TURN) {
    return -0x76;
  }
  if (this->card_flags & 2) {
    return -0x60;
  }
  if (!this->server()->ruler_server->card_ref_can_move(
          this->client_id, this->card_ref, 1)) {
    return -0x7B;
  }
  // Note: The original code passes non-null pointers here but ignores the
  // values written to them; we use nulls since the behavior should be the same.
  if (!this->server()->ruler_server->get_move_path_length_and_cost(
          this->client_id, this->card_ref, loc, nullptr, nullptr)) {
    return -0x79;
  }
  return 0;
}

void Card::execute_attack(shared_ptr<Card> attacker_card) {
  if (!attacker_card) {
    return;
  }

  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("execute_attack(@%04X #%04X, @%04X #%04X): ", this->get_card_ref(), this->get_card_id(), attacker_card->get_card_ref(), attacker_card->get_card_id()));
  bool is_nte = s->options.is_nte();

  this->card_flags &= 0xFFFFFFF3;
  int16_t attack_ap = this->action_metadata.attack_bonus;
  int16_t attack_tp = 0;
  int16_t defense_power = is_nte ? 0 : this->compute_defense_power_for_attacker_card(attacker_card);
  log.debug("ap=%hd, tp=%hd", attack_ap, attack_tp);
  if (!is_nte && (attack_ap == 0) && !this->action_metadata.check_flag(0x20)) {
    log.debug("ap == 0 and flag 0x20 not set");
    return;
  } else {
    log.debug("ap != 0 or flag 0x20 set; continuing...");
  }

  G_ApplyConditionEffect_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x01;
  cmd.effect.attacker_card_ref = attacker_card->card_ref;
  cmd.effect.target_card_ref = this->card_ref;
  if (attacker_card->action_chain.chain.attack_medium == AttackMedium::UNKNOWN_03) {
    // Probably Resta
    for (size_t strike_num = 0; strike_num < attacker_card->action_chain.chain.strike_count; strike_num++) {
      this->current_hp = min<int16_t>(
          this->current_hp + attacker_card->action_chain.chain.effective_tp,
          this->max_hp);
    }
    this->propagate_shared_hp_if_needed();
    cmd.effect.tp = attacker_card->action_chain.chain.effective_tp;
    cmd.effect.value = -cmd.effect.tp;
    s->send(cmd);

  } else {

    if (is_nte) {
      defense_power = this->compute_defense_power_for_attacker_card(attacker_card);
      log.debug("ap=%hd, tp=%hd, defense=%hd", attack_ap, attack_tp, defense_power);
      attacker_card->compute_action_chain_results(true, false);
      attack_ap = attacker_card->action_chain.chain.damage;
      if (this->action_chain.chain.attack_medium == AttackMedium::TECH) {
        attack_ap += this->action_chain.chain.tech_attack_bonus_nte;
      } else if (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
        attack_ap += this->action_chain.chain.physical_attack_bonus_nte;
      }
    }

    s->card_special->compute_attack_ap(this->shared_from_this(), &attack_ap, attacker_card->get_card_ref());
    log.debug("computed ap %hd", attack_ap);
    this->apply_ap_and_tp_adjust_assists_to_attack(attacker_card, &attack_ap, &defense_power, &attack_tp);
    log.debug("assist adjusts ap=%hd, defense=%hd", attack_ap, defense_power);

    int16_t raw_damage = attack_ap - defense_power;
    int16_t preliminary_damage = max<int16_t>(raw_damage, 0) - attack_tp;
    this->last_attack_preliminary_damage = preliminary_damage;
    log.debug("raw_damage=%hd, preliminary_damange=%hd", raw_damage, preliminary_damage);

    uint32_t unknown_a9 = 0;
    auto target = s->card_special->compute_replaced_target_based_on_conditions(
        this->get_card_ref(), 1, 0, attacker_card->get_card_ref(), 0xFFFF, 0, &unknown_a9, 0xFF, nullptr, 0xFFFF);

    if (!target) {
      target = this->shared_from_this();
      log.debug("target is not replaced");
    } else {
      log.debug("target replaced with @%04hX #%04hX", target->get_card_ref(), target->get_card_id());
    }

    if (!is_nte) {
      if (unknown_a9 != 0) {
        preliminary_damage = 0;
        log.debug("a9 nonzero; preliminary_damage = 0");
      }
      if (!(this->card_flags & 2) && (!attacker_card || !(attacker_card->card_flags & 2))) {
        s->card_special->check_for_defense_interference(attacker_card, this->shared_from_this(), &preliminary_damage);
        log.debug("checked for defense interference");
      }
    }

    cmd.effect.current_hp = is_nte ? attack_ap : min<int16_t>(attack_ap, 99);
    cmd.effect.ap = is_nte ? defense_power : min<int16_t>(defense_power, 99);
    cmd.effect.tp = attack_tp;

    auto ps = this->player_state();
    ps->stats.num_attacks_taken++;

    if (!(target->card_flags & 2)) {
      log.debug("flag 2 not set");
      for (size_t strike_num = 0; strike_num < attacker_card->action_chain.chain.strike_count; strike_num++) {
        int16_t final_effective_damage = 0;
        target->commit_attack(preliminary_damage, attacker_card, &cmd, strike_num, &final_effective_damage);
        ps->stats.action_card_negated_damage += max<int16_t>(0, this->current_defense_power - final_effective_damage);
      }
    } else {
      log.debug("flag 2 set; committing zero-damage attack");
      target->commit_attack(0, attacker_card, &cmd, 0, nullptr);
    }

    if (!is_nte && (this != target.get())) {
      log.debug("target was replaced; committing zero-damage attack on original card");
      this->commit_attack(0, attacker_card, &cmd, 0, nullptr);
    }

    s->send_6xB4x39();
  }
}

bool Card::get_condition_value(
    ConditionType cond_type,
    uint16_t card_ref,
    uint8_t def_effect_index,
    uint16_t value,
    uint16_t* out_value) const {
  return this->action_chain.get_condition_value(cond_type, card_ref, def_effect_index, value, out_value);
}

Condition* Card::find_condition(ConditionType cond_type) {
  for (size_t z = 0; z < this->action_chain.conditions.size(); z++) {
    auto& cond = this->action_chain.conditions[z];
    if (cond.type == cond_type) {
      return &cond;
    }
  }
  return nullptr;
}

const Condition* Card::find_condition(ConditionType cond_type) const {
  return const_cast<Card*>(this)->find_condition(cond_type);
}

shared_ptr<const CardIndex::CardEntry> Card::get_definition() const {
  return this->def_entry;
}

uint16_t Card::get_card_ref() const {
  return this->card_ref;
}

uint16_t Card::get_card_id() const {
  return this->get_definition()->def.card_id;
}

uint8_t Card::get_client_id() const {
  return this->client_id;
}

uint8_t Card::get_current_hp() const {
  return this->current_hp;
}

uint8_t Card::get_max_hp() const {
  return this->max_hp;
}

CardShortStatus Card::get_short_status() {
  CardShortStatus ret;
  if (this->def_entry->def.type == CardType::ITEM) {
    this->loc = this->player_state()->get_sc_card()->loc;
  }
  ret.card_ref = this->card_ref;
  ret.current_hp = this->current_hp;
  ret.max_hp = this->max_hp;
  ret.card_flags = this->card_flags;
  ret.loc = this->loc;
  return ret;
}

uint8_t Card::get_team_id() const {
  return this->team_id;
}

int32_t Card::move_to_location(const Location& loc) {
  auto s = this->server();

  int32_t code = this->error_code_for_move_to_location(loc);
  if (code) {
    return code;
  }

  uint32_t path_cost;
  uint32_t path_length;
  if (!s->ruler_server->get_move_path_length_and_cost(
          this->client_id, this->card_ref, loc, &path_length, &path_cost)) {
    return -0x79;
  }

  this->player_state()->stats.total_move_distance += path_length;
  this->player_state()->subtract_atk_points(path_cost);
  this->loc = loc;
  this->card_flags = this->card_flags | 0x80;

  // On NTE, traps happen now, not after the Move phase
  if (s->options.is_nte() &&
      this->def_entry->def.is_sc() &&
      ((s->overlay_state.tiles[loc.y][loc.x] & 0xF0) == 0x40)) {
    for (size_t z = 0; z < 4; z++) {
      auto other_ps = s->player_states[z];
      if (!other_ps) {
        continue;
      }
      auto other_sc = other_ps->get_sc_card();
      if (!other_sc) {
        continue;
      }

      if ((abs(other_sc->loc.x - loc.x) < 2) && (abs(other_sc->loc.y - loc.y) < 2)) {
        uint8_t trap_type = s->overlay_state.tiles[loc.y][loc.x] & 0x0F;
        uint16_t trap_card_id = s->overlay_state.trap_card_ids_nte[trap_type];
        if (other_ps->replace_assist_card_by_id(trap_card_id)) {
          G_EnqueueAnimation_Ep3_6xB4x2C cmd;
          cmd.change_type = 1;
          cmd.client_id = other_ps->client_id;
          cmd.unknown_a2[0] = trap_card_id;
          s->send(cmd);
        }
      }
    }
  }

  for (size_t warp_type = 0; warp_type < 5; warp_type++) {
    for (size_t warp_end = 0; warp_end < 2; warp_end++) {
      if ((s->warp_positions[warp_type][warp_end][0] == this->loc.x) &&
          (s->warp_positions[warp_type][warp_end][1] == this->loc.y)) {
        G_EnqueueAnimation_Ep3_6xB4x2C cmd;
        cmd.loc.x = this->loc.x;
        cmd.loc.y = this->loc.y;
        this->loc.x = s->warp_positions[warp_type][warp_end ^ 1][0];
        this->loc.y = s->warp_positions[warp_type][warp_end ^ 1][1];
        cmd.change_type = 0;
        cmd.card_refs.clear(0xFFFF);
        cmd.card_refs[0] = this->card_ref;
        cmd.unknown_a2.clear(0xFFFFFFFF);
        s->send(cmd);
        return 0;
      }
    }
  }

  return 0;
}

void Card::propagate_shared_hp_if_needed() {
  if ((this->server()->map_and_rules->rules.hp_type == HPType::COMMON_HP) &&
      ((this->def_entry->def.type == CardType::HUNTERS_SC) || (this->def_entry->def.type == CardType::ARKZ_SC))) {
    for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
      auto other_ps = this->server()->player_states[other_client_id];
      if ((other_client_id != this->client_id) && other_ps &&
          (other_ps->get_team_id() == this->team_id)) {
        other_ps->get_sc_card()->set_current_hp(this->current_hp, false);
      }
    }
  }
}

void Card::send_6xB4x4E_4C_4D_if_needed(bool always_send) {
  auto ps = this->player_state();

  ssize_t index = -1;
  if (this->card_ref == ps->get_sc_card_ref()) {
    index = 0;
  } else {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      if (this->card_ref == ps->get_set_ref(set_index)) {
        index = set_index + 1;
        break;
      }
    }
  }

  if (index < 0) {
    return;
  }

  this->action_chain.chain.card_ap = this->ap;
  this->action_chain.chain.card_tp = this->tp;

  auto& chain = ps->set_card_action_chains->at(index);
  auto& metadata = ps->set_card_action_metadatas->at(index);

  auto s = this->server();
  if (s->options.is_nte()) {
    chain = this->action_chain;
    metadata = this->action_metadata;

    G_UpdateActionChainAndMetadata_Ep3NTE_6xB4x0A cmd;
    cmd.client_id = this->client_id;
    cmd.index = index;
    cmd.chain = this->action_chain;
    cmd.metadata = this->action_metadata;
    s->send(cmd);

  } else {
    this->send_6xB4x4E_if_needed(always_send);

    if (always_send || (chain != this->action_chain)) {
      chain = this->action_chain;
      if (!s->get_should_copy_prev_states_to_current_states()) {
        G_UpdateActionChain_Ep3_6xB4x4C cmd;
        cmd.client_id = this->client_id;
        cmd.index = index;
        cmd.chain = this->action_chain.chain;
        s->send(cmd);
      }
    }

    if (always_send || (metadata != this->action_metadata)) {
      metadata = this->action_metadata;
      G_UpdateActionMetadata_Ep3_6xB4x4D cmd;
      cmd.client_id = this->client_id;
      cmd.index = index;
      cmd.metadata = this->action_metadata;
      s->send(cmd);
    }
  }
}

void Card::send_6xB4x4E_if_needed(bool always_send) {
  ssize_t chain_index = -1;
  if (this->card_ref == this->player_state()->get_sc_card_ref()) {
    chain_index = 0;
  } else {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      if (this->card_ref == this->player_state()->get_set_ref(set_index)) {
        chain_index = set_index + 1;
        break;
      }
    }
  }

  if (chain_index >= 0) {
    auto& prev_conds = this->player_state()->set_card_action_chains->at(chain_index).conditions;
    const auto& curr_conds = this->action_chain.conditions;
    if ((prev_conds != curr_conds) || (always_send != 0)) {
      prev_conds = curr_conds;
      if (!this->server()->get_should_copy_prev_states_to_current_states()) {
        G_UpdateCardConditions_Ep3_6xB4x4E cmd;
        cmd.client_id = this->client_id;
        cmd.index = chain_index;
        cmd.conditions = this->action_chain.conditions;
        this->server()->send(cmd);
      }
    }
  }
}

void Card::set_current_and_max_hp(int16_t hp) {
  this->current_hp = hp;
  this->max_hp = hp;
}

void Card::set_current_hp(
    uint32_t new_hp, bool propagate_shared_hp, bool enforce_max_hp) {
  if (!enforce_max_hp) {
    new_hp = max<int16_t>(new_hp, 0);
    this->max_hp = max<int16_t>(this->max_hp, new_hp);
  } else {
    new_hp = clamp<int16_t>(new_hp, 0, this->max_hp);
  }

  this->current_hp = new_hp;
  this->player_state()->update_hand_and_equip_state_and_send_6xB4x02_if_needed();

  if (propagate_shared_hp) {
    this->propagate_shared_hp_if_needed();
  }
}

void Card::update_stats_on_destruction() {
  auto s = this->server();
  this->player_state()->num_destroyed_fcs++;
  s->team_num_ally_fcs_destroyed[this->team_id]++;
  s->team_num_cards_destroyed[this->team_id]++;

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->player_states[client_id];
    if (other_ps && (other_ps->get_team_id() == this->team_id)) {
      auto card = other_ps->get_sc_card();
      if (card) {
        card->num_destroyed_ally_fcs++;
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        card = other_ps->get_set_card(set_index);
        if (card) {
          card->num_destroyed_ally_fcs++;
        }
      }
    }
  }
}

void Card::clear_action_chain_and_metadata_and_most_flags() {
  this->card_flags &= 0x8000FA7F;
  this->action_chain.clear_inner();
  this->action_chain.chain.acting_card_ref = this->card_ref;
  this->action_metadata.clear();
  this->action_metadata.card_ref = this->card_ref;
}

void Card::compute_action_chain_results(bool apply_action_conditions, bool ignore_this_card_ap_tp) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("compute_action_chain_results(@%04hX #%04hX): ", this->get_card_ref(), this->get_card_id()));
  bool is_nte = s->options.is_nte();

  this->action_chain.compute_attack_medium(s);
  this->action_chain.chain.strike_count = 1;
  this->action_chain.chain.ap_effect_bonus = 0;
  this->action_chain.chain.tp_effect_bonus = 0;

  log.debug("(initial) medium=%s, strike_count=%hhu, ap_effect_bonus=%hhd, tp_effect_bonus=%hhd",
      phosg::name_for_enum(this->action_chain.chain.attack_medium),
      this->action_chain.chain.strike_count,
      this->action_chain.chain.ap_effect_bonus,
      this->action_chain.chain.tp_effect_bonus);

  int16_t effective_ap;
  int16_t effective_tp;
  StatSwapType stat_swap_type;
  if (is_nte) {
    effective_ap = this->ap;
    effective_tp = this->tp;
    stat_swap_type = StatSwapType::NONE;
  } else {
    stat_swap_type = s->card_special->compute_stat_swap_type(this->shared_from_this());
    log.debug("stat_swap_type = %zu (0=none, 1=a/t, 2=a/h)", static_cast<size_t>(stat_swap_type));
    s->card_special->get_effective_ap_tp(stat_swap_type, &effective_ap, &effective_tp, this->get_current_hp(), this->ap, this->tp);
    log.debug("effective_ap = %hd, effective_tp = %hd", effective_ap, effective_tp);
  }

  // This option doesn't exist in NTE
  ignore_this_card_ap_tp &= !is_nte;

  for (size_t z = 0; (!ignore_this_card_ap_tp && (z < 8) && (z < this->action_chain.chain.attack_action_card_ref_count)); z++) {
    auto ce = s->definition_for_card_ref(this->action_chain.chain.attack_action_card_refs[z]);
    if (ce) {
      effective_ap += ce->def.ap.stat;
      effective_tp += ce->def.tp.stat;
      log.debug("(action card @%04hX) updated effective_ap = %hd, effective_tp = %hd", this->action_chain.chain.attack_action_card_refs[z].load(), effective_ap, effective_tp);
    }
  }

  // Add AP/TP from MAG items to SC's AP/TP
  auto ps = this->player_state();
  if (this->def_entry->def.is_sc()) {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = ps->get_set_card(set_index);
      if ((card && (card->def_entry->def.card_class() == CardClass::MAG_ITEM)) && !(card->card_flags & 2)) {
        int16_t card_ap, card_tp;
        s->card_special->get_effective_ap_tp(
            stat_swap_type, &card_ap, &card_tp, card->get_current_hp(), card->ap, card->tp);
        effective_ap += card_ap;
        effective_tp += card_tp;
        log.debug("(mag card set_index %zu @%04hX) updated effective_ap = %hd, effective_tp = %hd",
            set_index, card->get_card_ref(), effective_ap, effective_tp);
      }
    }
  }

  if ((this->def_entry->def.type == CardType::ITEM) && this->sc_def_entry) {
    auto sc_card = ps->get_sc_card();
    sc_card->compute_action_chain_results(apply_action_conditions, true);
    effective_ap += sc_card->action_chain.chain.effective_ap + sc_card->action_chain.chain.ap_effect_bonus;
    effective_tp += sc_card->action_chain.chain.effective_tp + sc_card->action_chain.chain.tp_effect_bonus;
    log.debug("(item is attacking; adding SC stats) updated effective_ap = %hd, effective_tp = %hd",
        effective_ap, effective_tp);
  }

  if (!this->action_chain.check_flag(0x10)) {
    this->action_chain.chain.effective_ap = is_nte ? effective_ap : min<int16_t>(effective_ap, 99);
    log.debug("set chain effective_ap = %hd", this->action_chain.chain.effective_ap);
  }
  if (!this->action_chain.check_flag(0x20)) {
    this->action_chain.chain.effective_tp = is_nte ? effective_tp : min<int16_t>(effective_tp, 99);
    log.debug("set chain effective_tp = %hd", this->action_chain.chain.effective_tp);
  }

  if (apply_action_conditions) {
    auto this_sh = this->shared_from_this();
    s->card_special->apply_action_conditions(EffectWhen::BEFORE_ANY_CARD_ATTACK, this_sh, this_sh, 1, nullptr);
    log.debug("applied action conditions (1)");
  } else {
    log.debug("skipped applying action conditions (1)");
  }

  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    switch (s->assist_server->get_active_assist_by_index(z)) {
      case AssistEffect::POWERLESS_RAIN:
        if (!is_nte &&
            this->card_type_is_sc_or_creature() &&
            (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          this->action_chain.chain.ap_effect_bonus -= 2;
        }
        break;
      case AssistEffect::BRAVE_WIND:
        if (!is_nte &&
            this->card_type_is_sc_or_creature() &&
            (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::INFLUENCE:
        if (!is_nte && this->card_type_is_sc_or_creature()) {
          int16_t count = ps->count_set_refs();
          this->action_chain.chain.ap_effect_bonus += (count >> 1);
        }
        break;
      case AssistEffect::AP_ABSORPTION:
        if (!is_nte && (this->action_chain.chain.attack_medium == AttackMedium::TECH)) {
          this->action_chain.chain.tp_effect_bonus += 2;
        }
        break;
      case AssistEffect::FIX:
        if (is_nte && !this->def_entry->def.is_sc()) {
          this->action_chain.chain.ap_effect_bonus = 2 - this->action_chain.chain.card_ap;
        }
        break;
      case AssistEffect::TECH_FIELD:
        if (is_nte ? this->def_entry->def.is_sc() : this->card_type_is_sc_or_creature()) {
          this->action_chain.chain.tp_effect_bonus += 2;
        }
        break;
      case AssistEffect::FOREST_RAIN:
        if (this->def_entry->def.card_class() == CardClass::NATIVE_CREATURE) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::CAVE_WIND:
        if (this->def_entry->def.card_class() == CardClass::A_BEAST_CREATURE) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::MINE_BRIGHTNESS:
        if (this->def_entry->def.card_class() == CardClass::MACHINE_CREATURE) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::RUIN_DARKNESS:
        if (this->def_entry->def.card_class() == CardClass::DARK_CREATURE) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::SABER_DANCE:
        if (this->def_entry->def.card_class() == CardClass::SWORD_ITEM) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::BULLET_STORM:
        if (this->def_entry->def.card_class() == CardClass::GUN_ITEM) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::CANE_PALACE:
        if (this->def_entry->def.card_class() == CardClass::CANE_ITEM) {
          this->action_chain.chain.tp_effect_bonus += 2;
        }
        break;
      case AssistEffect::GIANT_GARDEN:
        if (!this->def_entry->def.is_sc() && (this->def_entry->def.self_cost > 3)) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::MARCH_OF_THE_MEEK:
        if (!this->def_entry->def.is_sc() && (this->def_entry->def.self_cost <= 3)) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::SUPPORT:
        if (this->def_entry->def.is_sc()) {
          size_t num_scs_in_range = 0;
          for (size_t client_id = 0; client_id < 4; client_id++) {
            auto other_ps = s->get_player_state(client_id);
            if (!other_ps || (client_id == this->client_id) || (other_ps->get_team_id() != this->team_id)) {
              continue;
            }
            auto other_sc_card = other_ps->get_sc_card();
            if (other_sc_card &&
                (abs(this->loc.x - other_sc_card->loc.x) < 2) &&
                (abs(this->loc.y - other_sc_card->loc.y) < 2)) {
              num_scs_in_range++;
            }
          }
          if (num_scs_in_range > 0) {
            this->action_chain.chain.ap_effect_bonus += 3;
          }
        }
        break;
      case AssistEffect::VENGEANCE:
        if (!this->def_entry->def.is_sc()) {
          size_t denom = is_nte ? 2 : 3;
          this->action_chain.chain.ap_effect_bonus += (s->team_num_ally_fcs_destroyed[this->team_id] / denom);
        }
        break;
      default:
        break;
    }
  }

  int16_t damage = 0;
  if (this->action_chain.chain.attack_medium == AttackMedium::TECH) {
    damage = this->action_chain.chain.effective_tp + this->action_chain.chain.tp_effect_bonus;
    log.debug("(tech) damage = %hhd (eff) + %hhd (bonus) = %hd", this->action_chain.chain.effective_tp, this->action_chain.chain.tp_effect_bonus, damage);
  } else if (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
    damage = this->action_chain.chain.effective_ap + this->action_chain.chain.ap_effect_bonus;
    log.debug("(physical) damage = %hhd (eff) + %hhd (bonus) = %hd", this->action_chain.chain.effective_ap, this->action_chain.chain.ap_effect_bonus, damage);
  } else {
    log.debug("(unknown attack medium) damage = 0");
  }

  this->action_chain.chain.damage = is_nte
      ? (damage * this->action_chain.chain.damage_multiplier)
      : min<int16_t>(damage * this->action_chain.chain.damage_multiplier, 99);
  log.debug("overall chain damage = %hd (base) * %hhd (mult) = %hhd", damage, this->action_chain.chain.damage_multiplier, this->action_chain.chain.damage);

  if (apply_action_conditions) {
    auto this_sh = this->shared_from_this();
    s->card_special->apply_action_conditions(EffectWhen::BEFORE_ANY_CARD_ATTACK, this_sh, this_sh, 2, nullptr);
    log.debug("applied action conditions (2)");
    if (!is_nte && this->action_chain.check_flag(0x100)) {
      this->action_chain.chain.damage = min<int16_t>(this->action_chain.chain.damage + 5, 99);
      log.debug("(has flag 0x100) chain damage = %hhd", this->action_chain.chain.damage);
    }
  } else {
    log.debug("skipped applying action conditions (2)");
  }

  if (!is_nte) {
    num_assists = s->assist_server->compute_num_assist_effects_for_client(this->get_client_id());
    for (size_t z = 0; z < num_assists; z++) {
      switch (s->assist_server->get_active_assist_by_index(z)) {
        case AssistEffect::AP_ABSORPTION:
          if (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
            this->action_chain.chain.damage = 0;
          }
          break;
        case AssistEffect::SILENT_COLOSSEUM:
          if (this->action_chain.chain.damage >= 7) {
            this->action_chain.chain.damage = 0;
          }
          break;
        case AssistEffect::FIX:
          if (!this->def_entry->def.is_sc()) {
            this->action_chain.chain.damage = 2;
          }
          break;
        default:
          break;
      }
    }
  }

  if (log.should_log(phosg::LogLevel::DEBUG)) {
    string chain_str = this->action_chain.str(s);
    log.debug("result computed as %s", chain_str.c_str());
  }
}

void Card::unknown_802380C0() {
  bool is_nte = this->server()->options.is_nte();
  this->card_flags &= 0xFFFFF7FB;
  this->action_metadata.clear_flags(is_nte ? 0x10 : 0x30);
  this->action_chain.clear_flags(is_nte ? 0x40 : 0x140);
  this->unknown_80237F98(0);
}

void Card::unknown_80237F98(bool require_condition_20_or_21) {
  auto s = this->server();

  bool should_send_updates = false;
  for (ssize_t z = 8; z >= 0; z--) {
    if (this->action_chain.conditions[z].type != ConditionType::NONE) {
      if (!require_condition_20_or_21 ||
          s->card_special->condition_applies_on_sc_or_item_attack(this->action_chain.conditions[z])) {
        ActionState as;
        auto& cond = this->action_chain.conditions[z];
        if (!s->card_special->is_card_targeted_by_condition(cond, as, this->shared_from_this())) {
          s->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, this->shared_from_this());
          should_send_updates = true;
        } else if (this->action_chain.conditions[z].remaining_turns == 0) {
          if (--this->action_chain.conditions[z].a_arg_value < 1) {
            s->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(cond, this->shared_from_this());
            should_send_updates = true;
          }
        }
      }
    }
  }

  this->compute_action_chain_results(1, false);
  if (!s->options.is_nte()) {
    this->unknown_80236554(nullptr, nullptr);
  }
  if (should_send_updates) {
    this->send_6xB4x4E_4C_4D_if_needed();
  }
}

void Card::unknown_80237F88() {
  this->card_flags &= 0xFFFFF8FF;
}

void Card::draw_phase_before() {
  if (!this->server()->options.is_nte()) {
    this->facing_direction = Direction::INVALID_FF;
  }
  this->server()->card_special->draw_phase_before_for_card(this->shared_from_this());
}

void Card::action_phase_before() {
  if (!this->server()->options.is_nte()) {
    this->clear_action_chain_and_metadata_and_most_flags();
  }
  this->server()->card_special->action_phase_before_for_card(this->shared_from_this());
}

void Card::move_phase_before() {
  this->server()->card_special->move_phase_before_for_card(this->shared_from_this());
}

void Card::unknown_80236374(shared_ptr<Card> other_card, const ActionState* as) {
  auto s = this->server();
  auto log = s->log_stack(phosg::string_printf("unknown_80236374(@%04hX #%04hX, @%04hX #%04hX): ", this->get_card_ref(), this->get_card_id(), other_card->get_card_ref(), other_card->get_card_id()));

  if (log.should_log(phosg::LogLevel::DEBUG)) {
    if (as) {
      string as_str = as->str(s);
      log.debug("as = %s", as_str.c_str());
    } else {
      log.debug("as = null");
    }
  }

  auto check_card = [&](shared_ptr<Card> card) -> void {
    if (card) {
      if (!card->unknown_80236554(other_card, as)) {
        log.debug("check_card @%04hX #%04hX => false", card->get_card_ref(), card->get_card_id());
        card->action_metadata.clear_flags(0x20);
      } else {
        log.debug("check_card @%04hX #%04hX => true", card->get_card_ref(), card->get_card_id());
        card->action_metadata.set_flags(0x20);
      }
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = s->player_states[client_id];
    if (ps && (s->get_current_team_turn2() != ps->get_team_id())) {
      check_card(ps->get_sc_card());
      for (size_t set_index = 0; set_index < 8; set_index++) {
        check_card(ps->get_set_card(set_index));
      }
    }
  }

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = s->player_states[client_id];
    if (ps && (s->get_current_team_turn2() == ps->get_team_id())) {
      check_card(ps->get_sc_card());
      for (size_t set_index = 0; set_index < 8; set_index++) {
        check_card(ps->get_set_card(set_index));
      }
    }
  }
}

void Card::unknown_802379BC(uint16_t card_ref) {
  this->action_chain.chain.unknown_card_ref_a3 =
      (card_ref == 0xFFFF) ? this->card_ref : card_ref;
}

void Card::unknown_802379DC(const ActionState& pa) {
  this->action_metadata.add_defense_card_ref(
      pa.defense_card_ref, this->shared_from_this(), pa.original_attacker_card_ref);
  this->server()->card_special->unknown_8024A9D8(pa, 0xFFFF);
  for (size_t z = 0; z < this->action_metadata.target_card_ref_count; z++) {
    shared_ptr<Card> card = this->server()->card_for_set_card_ref(this->action_metadata.target_card_refs[z]);
    if (card) {
      card->action_chain.set_action_subphase_from_card(this->shared_from_this());
      card->send_6xB4x4E_4C_4D_if_needed();
    }
  }
  this->send_6xB4x4E_4C_4D_if_needed();
}

void Card::unknown_80237A90(const ActionState& pa, uint16_t action_card_ref) {
  auto s = this->server();

  this->facing_direction = pa.facing_direction;
  this->action_chain.add_attack_action_card_ref(action_card_ref, s);

  if (s->options.is_nte()) {
    if (s->ruler_server->count_targets_with_rampage_and_not_pierce_nte(pa)) {
      this->action_chain.set_flags(0x02);
    }
    if (s->ruler_server->count_targets_with_pierce_and_not_rampage_nte(pa)) {
      this->action_chain.set_flags(0x80);
    }
  } else {
    for (size_t z = 0; z < 4; z++) {
      if (s->ruler_server->count_rampage_targets_for_attack(pa, z) != 0) {
        this->action_chain.set_flags(0x200 << z);
      }
      if (s->ruler_server->attack_action_has_pierce_and_not_rampage(pa, z)) {
        this->action_chain.set_flags(0x2000 << z);
      }
    }
    if (s->ruler_server->any_attack_action_card_is_support_tech_or_support_pb(pa)) {
      this->action_chain.set_flags(0x20000);
    }
  }

  if (this->action_chain.chain.target_card_ref_count == 0) {
    for (size_t z = 0; (z < 4 * 9) && (pa.target_card_refs[z] != 0xFFFF); z++) {
      this->action_chain.add_target_card_ref(pa.target_card_refs[z]);
      shared_ptr<Card> sc_card = s->card_for_set_card_ref(pa.target_card_refs[z]);
      if (sc_card) {
        sc_card->action_metadata.add_target_card_ref(this->card_ref);
        sc_card->card_flags |= 8;
        sc_card->send_6xB4x4E_4C_4D_if_needed();
      }
    }
  }

  if (this->action_chain.chain.attack_number & 0x80) {
    this->action_chain.chain.attack_number = s->num_pending_attacks_with_cards;
    s->attack_cards[s->num_pending_attacks_with_cards] = this->shared_from_this();
    s->pending_attacks_with_cards[s->num_pending_attacks_with_cards] = pa;
    s->num_pending_attacks_with_cards++;
  }
  s->card_special->unknown_8024A9D8(pa, action_card_ref);
  this->send_6xB4x4E_4C_4D_if_needed();
}

void Card::dice_phase_before() {
  auto s = this->server();
  this->unknown_a9++;
  for (ssize_t z = 8; z >= 0; z--) {
    auto& cond = this->action_chain.conditions[z];
    if (cond.type != ConditionType::NONE) {
      ActionState as;
      if ((this->card_flags & 2) ||
          !s->card_special->is_card_targeted_by_condition(cond, as, this->shared_from_this())) {
        cond.remaining_turns = 1;
      }
      if (cond.remaining_turns < 99) {
        // Note: There is at least one case in the original implementation where
        // remaining_turns can go negative: Creinu's HP Assist. The condition is
        // applied with remaining_turns=0 to all affected cards (so it should be
        // immediately removed here). But since remaining_turns is unsigned in
        // our implementation, we have to check for underflow here.
        if (cond.remaining_turns > 0) {
          cond.remaining_turns--;
        }
        if (cond.remaining_turns < 1) {
          s->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(
              cond, this->shared_from_this());
        }
      }
    }
  }
  if (s->options.is_nte()) {
    this->clear_action_chain_and_metadata_and_most_flags();
  }
  s->card_special->dice_phase_before_for_card(this->shared_from_this());
}

bool Card::is_guard_item() const {
  return (this->def_entry->def.card_class() == CardClass::GUARD_ITEM);
}

bool Card::unknown_80236554(shared_ptr<Card> other_card, const ActionState* as) {
  auto s = this->server();
  auto log = s->log_stack(other_card
          ? phosg::string_printf("unknown_80236554(@%04hX #%04hX, @%04hX #%04hX): ", this->get_card_ref(), this->get_card_id(), other_card->get_card_ref(), other_card->get_card_id())
          : phosg::string_printf("unknown_80236554(@%04hX #%04hX, null): ", this->get_card_ref(), this->get_card_id()));
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    if (as) {
      string as_str = as->str(s);
      log.debug("as = %s", as_str.c_str());
    } else {
      log.debug("as = null");
    }
  }

  bool ret = false;

  int16_t attack_bonus = 0;
  if (other_card) {
    if (!as) {
      for (size_t z = 0; z < other_card->action_chain.chain.target_card_ref_count; z++) {
        if (other_card->action_chain.chain.target_card_refs[z] == this->get_card_ref()) {
          attack_bonus = other_card->action_chain.chain.damage;
          ret = true;
          log.debug("attack_bonus = %hd (matched other_card->action_chain.chain.target_card_refs)", attack_bonus);
          break;
        }
      }
    } else {
      for (size_t z = 0; (z < 4 * 9) && (as->target_card_refs[z] != 0xFFFF); z++) {
        if (as->target_card_refs[z] == this->get_card_ref()) {
          attack_bonus = other_card->action_chain.chain.damage;
          log.debug("attack_bonus = %hd (matched as->target_card_refs)", attack_bonus);
          ret = true;
          break;
        }
      }
    }
  }

  this->action_metadata.attack_bonus = max<int16_t>(attack_bonus, 0);
  log.debug("attack_bonus = %hhd", this->action_metadata.attack_bonus);
  this->last_attack_preliminary_damage = 0;
  this->last_attack_final_damage = 0;
  log.debug("last attack damage stats cleared");

  if (other_card) {
    log.debug("applying BEFORE_ANY_CARD_ATTACK conditions");
    s->card_special->apply_action_conditions(
        EffectWhen::BEFORE_ANY_CARD_ATTACK, other_card, this->shared_from_this(), 0x20, as);
    log.debug("applying BEFORE_THIS_CARD_ATTACKED conditions");
    s->card_special->apply_action_conditions(
        EffectWhen::BEFORE_THIS_CARD_ATTACKED, other_card, this->shared_from_this(), 0x40, as);
    if (other_card->action_chain.check_flag(0x20000)) {
      log.debug("attack_bonus cleared due to cancellation");
      this->action_metadata.attack_bonus = 0;
      return ret;
    }
  }
  if (this->card_flags & 2) {
    log.debug("attack_bonus cleared due to destruction");
    this->action_metadata.attack_bonus = 0;
  }
  return ret;
}

void Card::execute_attack_on_all_valid_targets(shared_ptr<Card> attacker_card) {
  auto s = this->server();
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = s->player_states[client_id];
    if (ps) {
      shared_ptr<Card> card = ps->get_sc_card();
      if (card) {
        card->execute_attack(attacker_card);
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        shared_ptr<Card> card = ps->get_set_card(set_index);
        if (card) {
          card->execute_attack(attacker_card);
        }
      }
    }
  }
}

void Card::apply_attack_result() {
  auto s = this->server();
  auto ps = this->player_state();
  bool is_nte = s->options.is_nte();

  auto log = s->log_stack(phosg::string_printf("apply_attack_result(@%04hX #%04hX): ", this->get_card_ref(), this->get_card_id()));
  if (!this->action_chain.can_apply_attack()) {
    return;
  }

  if (is_nte) {
    if (this->action_chain.check_flag(0x02)) {
      auto first_target_card = s->card_for_set_card_ref(this->action_chain.chain.target_card_refs[0]);
      if (first_target_card) {
        auto first_target_ps = first_target_card->player_state();
        if (first_target_ps->count_set_cards() == 0) {
          this->action_chain.clear_target_card_refs();
          this->action_chain.chain.target_card_refs[0] = first_target_ps->get_sc_card_ref();
          this->action_chain.chain.target_card_ref_count = 1;
        }
      }
    }

    ActionChainWithConds temp_chain = this->action_chain;
    temp_chain.chain.target_card_ref_count = 0;

    for (size_t z = 0; z < this->action_chain.chain.target_card_ref_count; z++) {
      auto target_card = s->card_for_set_card_ref(this->action_chain.chain.target_card_refs[z]);
      if (!target_card) {
        continue;
      }
      if (!(target_card->card_flags & 2)) {
        temp_chain.chain.target_card_refs[temp_chain.chain.target_card_ref_count] = target_card->get_card_ref();
        temp_chain.chain.target_card_ref_count++;
      } else if ((target_card->get_definition()->def.type == CardType::ITEM) && !this->action_chain.check_flag(0x02)) {
        auto target_ps = target_card->player_state();
        shared_ptr<Card> candidate_card;
        for (size_t set_index = 0; set_index < 8; set_index++) {
          auto set_card = target_ps->get_set_card(set_index);
          if (set_card && (set_card != target_card) && !(set_card->card_flags & 2) && set_card->is_guard_item()) {
            candidate_card = set_card;
            break;
          }
        }
        if (!candidate_card) {
          for (size_t set_index = 0; set_index < 8; set_index++) {
            auto set_card = target_ps->get_set_card(set_index);
            if (set_card && (set_card != target_card) && !(set_card->card_flags & 2)) {
              candidate_card = set_card;
              break;
            }
          }
        }
        if (candidate_card) {
          temp_chain.chain.target_card_refs[temp_chain.chain.target_card_ref_count] = candidate_card->get_card_ref();
          temp_chain.chain.target_card_ref_count++;
        } else {
          auto target_sc = target_ps->get_sc_card();
          if (!(target_sc->card_flags & 2)) {
            temp_chain.chain.target_card_refs[temp_chain.chain.target_card_ref_count] = target_sc->get_card_ref();
            temp_chain.chain.target_card_ref_count++;
          }
        }
      }
    }

    this->action_chain.chain.target_card_ref_count = 0;
    for (size_t z = 0; z < temp_chain.chain.target_card_ref_count; z++) {
      this->action_chain.add_target_card_ref(temp_chain.chain.target_card_refs[z]);
    }

    if (!this->action_chain.check_flag(0x40)) {
      s->card_special->unknown_8024945C(this->shared_from_this(), nullptr);
    }

    for (size_t z = 0; z < this->action_chain.chain.target_card_ref_count; z++) {
      auto target_card = s->card_for_set_card_ref(this->action_chain.chain.target_card_refs[z]);
      if (target_card && !this->action_chain.check_flag(0x40)) {
        s->card_special->unknown_8024A6DC(this->shared_from_this(), target_card);
      }
    }

    this->compute_action_chain_results(true, false);
    if (!this->action_chain.check_flag(0x40)) {
      s->card_special->apply_effects_before_attack(this->shared_from_this());
    }

    this->compute_action_chain_results(true, false);
    for (size_t z = 0; z < this->action_chain.chain.target_card_ref_count; z++) {
      auto target_card = s->card_for_set_card_ref(this->action_chain.chain.target_card_refs[z]);
      if (target_card) {
        target_card->execute_attack(this->shared_from_this());
      }
    }

  } else {
    if (ps->stats.max_attack_combo_size < this->action_chain.chain.attack_action_card_ref_count) {
      ps->stats.max_attack_combo_size = this->action_chain.chain.attack_action_card_ref_count;
    }

    ActionState as;
    as.attacker_card_ref = this->get_card_ref();
    as.target_card_refs = this->action_chain.chain.target_card_refs;
    s->replace_targets_due_to_destruction_or_conditions(&as);
    this->action_chain.chain.target_card_refs = as.target_card_refs;
    this->action_chain.chain.target_card_ref_count = 0;
    for (size_t z = 0; z < 4 * 9; z++) {
      if (this->action_chain.chain.target_card_refs[z] != 0xFFFF) {
        this->action_chain.chain.target_card_ref_count++;
      } else {
        break;
      }
    }

    if (log.should_log(phosg::LogLevel::DEBUG)) {
      string as_str = as.str(s);
      log.debug("as constructed as %s", as_str.c_str());
    }

    for (size_t z = 0; z < this->action_chain.chain.target_card_ref_count; z++) {
      shared_ptr<Card> card = s->card_for_set_card_ref(this->action_chain.chain.target_card_refs[z]);
      if (card) {
        card->current_defense_power = card->action_metadata.attack_bonus;
        if (!this->action_chain.check_flag(0x40)) {
          log.debug("unknown_8024A6DC(@%04hX #%04hX) ...", card->get_card_ref(), card->get_card_id());
          s->card_special->unknown_8024A6DC(this->shared_from_this(), card);
        }
      }
    }

    log.debug("compute_action_chain_results 1 ...");
    this->compute_action_chain_results(true, false);

    if (!this->action_chain.check_flag(0x40)) {
      log.debug("apply_effects_before_attack ...");
      s->card_special->apply_effects_before_attack(this->shared_from_this());
    }
    if (!(this->card_flags & 2)) {
      log.debug("compute_action_chain_results 2 ...");
      this->compute_action_chain_results(true, false);
      log.debug("check_for_attack_interference ...");
      s->card_special->check_for_attack_interference(this->shared_from_this());
    }
    log.debug("compute_action_chain_results 3 ...");
    this->compute_action_chain_results(true, false);

    log.debug("unknown_80236374 ...");
    this->unknown_80236374(this->shared_from_this(), nullptr);
    log.debug("execute_attack_on_all_valid_targets ...");
    this->execute_attack_on_all_valid_targets(this->shared_from_this());
  }

  if (!this->action_chain.check_flag(0x40)) {
    log.debug("apply_effects_after_attack ...");
    s->card_special->apply_effects_after_attack(this->shared_from_this());
  }
  ps->stats.num_attacks_given++;

  this->action_chain.clear_flags(8);
  this->action_chain.set_flags(4);
  this->card_flags |= 0x200;
  this->action_chain.clear_target_card_refs();
  if (!is_nte) {
    for (size_t client_id = 0; client_id < 4; client_id++) {
      auto ps = s->player_states[client_id];
      if (ps) {
        log.debug("unknown_8023C110(%zu) ...", client_id);
        ps->unknown_8023C110();
      }
    }
  }

  this->send_6xB4x4E_4C_4D_if_needed();
}

} // namespace Episode3
