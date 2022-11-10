#include "Card.hh"

#include "Server.hh"
#include "../CommandFormats.hh"

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
    current_defense_power(0) { }

void Card::init() {
  this->clear_action_chain_and_metadata_and_most_flags();
  this->team_id = this->player_state()->get_team_id();
  this->def_entry = this->server()->definition_for_card_id(this->card_id);
  if (!this->def_entry) {
    // The original implementation replaces the card ID and definition with 0009
    // (Saber) if the SC is Hunters-side, and 0056 (Booma) if the SC is
    // Arkz-side. This could break things later on in the battle, and even if it
    // doesn't, it certainly isn't behavior that the player would expect, so we
    // prevent it instead.
    throw runtime_error("card definition is missing");
  }
  this->sc_card_ref = this->player_state()->get_sc_card_ref();
  this->sc_def_entry = this->server()->definition_for_card_id(
      this->player_state()->get_sc_card_id());
  this->sc_card_type = this->player_state()->get_sc_card_type();
  this->max_hp = this->def_entry->def.hp.stat;
  this->current_hp = this->def_entry->def.hp.stat;
  if (this->sc_card_ref == this->card_ref) {
    int16_t rules_char_hp = this->server()->base()->map_and_rules1->rules.char_hp;
    int16_t base_char_hp = (rules_char_hp == 0) ? 15 : rules_char_hp;
    int16_t hp = clamp<int16_t>(base_char_hp + this->def_entry->def.hp.stat, 1, 99);
    this->max_hp = hp;
    this->current_hp = hp;
  }
  this->ap = this->def_entry->def.ap.stat;
  this->tp = this->def_entry->def.tp.stat;
  this->num_ally_fcs_destroyed_at_set_time = this->server()->team_num_ally_fcs_destroyed[this->team_id];
  this->num_cards_destroyed_by_team_at_set_time = this->server()->team_num_cards_destroyed[this->team_id];
  this->action_chain.chain.card_ap = this->ap;
  this->action_chain.chain.card_tp = this->tp;
  this->loc.direction = this->player_state()->start_facing_direction;
  if (this->sc_card_ref != this->card_ref) {
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

  ssize_t existing_cond_index;
  for (size_t z = 0; z < this->action_chain.conditions.size(); z++) {
    const auto& cond = this->action_chain.conditions[z];
    if (cond.type == eff.type) {
      existing_cond_index = z;
      if (eff.type == ConditionType::MV_BONUS ||
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
  }

  if (cond_index < 0) {
    return -1;
  }

  int16_t existing_cond_value = 0;
  auto& cond = this->action_chain.conditions[cond_index];
  if ((eff.type == ConditionType::MV_BONUS) && (cond.type == ConditionType::MV_BONUS)) {
    existing_cond_value = clamp<int16_t>(cond.value, -99, 99);
  }

  this->server()->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(
      cond, this->shared_from_this());
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

  switch (eff.arg1[0]) {
    case 'a':
      cond.a_arg_value = atoi(&eff.arg1[1]);
      break;
    case 'e':
      cond.remaining_turns = 99;
      break;
    case 'f':
      cond.remaining_turns = 100;
      break;
    case 'r':
      cond.remaining_turns = 102;
      break;
    case 't':
      cond.remaining_turns = atoi(&eff.arg1[1]);
  }

  this->server()->card_special->update_condition_orders(this->shared_from_this());
  return cond_index;
}

void Card::apply_ap_adjust_assists_to_attack(
    shared_ptr<const Card> attacker_card,
    int16_t* inout_attacker_ap,
    int16_t* in_defense_power) const {
  uint8_t client_id = attacker_card->get_client_id();
  size_t num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = this->server()->assist_server->get_active_assist_by_index(z);
    if ((eff == AssistEffect::FIX) &&
        attacker_card &&
        (attacker_card->def_entry->def.type != CardType::HUNTERS_SC) &&
        (attacker_card->def_entry->def.type != CardType::ARKZ_SC)) {
      *inout_attacker_ap = 2;
    } else if ((eff == AssistEffect::SILENT_COLOSSEUM) &&
        (*inout_attacker_ap - *in_defense_power >= 7)) {
      *inout_attacker_ap = 0;
    }
  }

  num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = this->server()->assist_server->get_active_assist_by_index(z);
    if ((eff == AssistEffect::AP_ABSORPTION) &&
        (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
      *inout_attacker_ap = 0;
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
    G_ApplyConditionEffect_GC_Ep3_6xB4x06* cmd,
    size_t strike_number,
    int16_t* out_effective_damage) {
  int16_t effective_damage = damage;
  this->server()->card_special->adjust_attack_damage_due_to_conditions(
      this->shared_from_this(), &effective_damage, attacker_card->get_card_ref());

  size_t num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = this->server()->assist_server->get_active_assist_by_index(z);
    if ((eff == AssistEffect::RANSOM) &&
        (attacker_card->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
      uint8_t team_id = this->player_state()->get_team_id();
      int16_t exp_amount = clamp<int16_t>(this->server()->team_exp[team_id], 0, effective_damage);
      this->server()->team_exp[team_id] -= exp_amount;
      effective_damage -= exp_amount;
      this->server()->compute_team_dice_boost(team_id);
      this->server()->update_battle_state_flags_and_send_6xB4x03_if_needed();
      if (cmd) {
        cmd->effect.ap += exp_amount;
      }
    }
  }

  if (this->action_metadata.check_flag(0x10)) {
    effective_damage = 0;
  }


  auto attacker_ps = attacker_card->player_state();
  attacker_ps->stats.damage_given += effective_damage;
  this->player_state()->stats.damage_taken += effective_damage;

  this->current_hp = clamp<int16_t>(
      this->current_hp - effective_damage, 0, this->max_hp);

  if ((effective_damage > 0) &&
      (attacker_ps->stats.max_attack_damage < effective_damage)) {
    attacker_ps->stats.max_attack_damage = effective_damage;
  }

  this->last_attack_final_damage = effective_damage;
  if (effective_damage > 0) {
    this->card_flags = this->card_flags | 4;
  }
  if (this->current_hp < 1) {
    this->destroy_set_card(attacker_card);
  }

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd_to_send;
  if (cmd) {
    cmd_to_send = *cmd;
  }
  cmd_to_send.effect.flags = (strike_number == 0) ? 0x11 : 0x01;
  cmd_to_send.effect.attacker_card_ref = attacker_card->card_ref;
  cmd_to_send.effect.target_card_ref = this->card_ref;
  cmd_to_send.effect.value = effective_damage;
  this->server()->send(cmd_to_send);

  this->propagate_shared_hp_if_needed();

  if ((this->def_entry->def.type == CardType::HUNTERS_SC) ||
      (this->def_entry->def.type == CardType::ARKZ_SC)) {
    this->player_state()->stats.sc_damage_taken += effective_damage;
  }

  if (out_effective_damage) {
    *out_effective_damage = effective_damage;
  }
}

int16_t Card::compute_defense_power_for_attacker_card(
    shared_ptr<const Card> attacker_card) {
  if (!attacker_card) {
    return 0;
  }

  this->action_metadata.defense_power = 0;
  this->action_metadata.defense_bonus = 0;

  for (size_t z = 0; z < this->action_metadata.defense_card_ref_count; z++) {
    if (attacker_card->card_ref != this->action_metadata.original_attacker_card_refs[z]) {
      continue;
    }
    auto ce = this->server()->definition_for_card_ref(
        this->action_metadata.defense_card_refs[z]);
    if (ce) {
      this->action_metadata.defense_power += ce->def.hp.stat;
    }
  }

  this->server()->card_special->apply_action_conditions(
      3, attacker_card, this->shared_from_this(), 0x08, nullptr);
  this->server()->card_special->apply_action_conditions(
      3, attacker_card, this->shared_from_this(), 0x10, nullptr);
  return this->action_metadata.defense_power + this->action_metadata.defense_bonus;
}

void Card::destroy_set_card(shared_ptr<Card> attacker_card) {
  this->current_hp = 0;
  if (!(this->card_flags & 2)) {
    if (!this->server()->ruler_server->card_ref_or_any_set_card_has_condition_46(this->card_ref)) {
      this->server()->card_special->on_card_destroyed(
          attacker_card, this->shared_from_this());

      this->card_flags = this->card_flags | 2;
      this->update_stats_on_destruction();
      this->player_state()->stats.num_owned_cards_destroyed++;

      if (attacker_card && (attacker_card->team_id != this->team_id)) {
        attacker_card->player_state()->stats.num_opponent_cards_destroyed++;
        this->server()->add_team_exp(this->team_id ^ 1, 3);
      }

      if ((this->sc_card_type == CardType::HUNTERS_SC) && (this->def_entry->def.type == CardType::ITEM)) {
        auto sc_card = this->player_state()->get_sc_card();
        if (!(sc_card->card_flags & 2) &&
            !sc_card->get_attack_condition_value(ConditionType::ELUDE, 0xFFFF, 0xFF, 0xFFFF, nullptr)) {
          int16_t hp = sc_card->get_current_hp();
          sc_card->set_current_hp(hp - 1);
          sc_card->player_state()->stats.sc_damage_taken++;
          if (attacker_card && (attacker_card->team_id != this->team_id)) {
            G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
            cmd.effect.flags = 0x41;
            cmd.effect.attacker_card_ref = attacker_card->card_ref;
            cmd.effect.target_card_ref = sc_card->card_ref;
            cmd.effect.value = 1;
            this->server()->send(cmd);
          }
          if (sc_card->get_current_hp() < 1) {
            sc_card->destroy_set_card(attacker_card);
          }
        }
      }

      if ((this->server()->base()->map_and_rules1->rules.hp_type == HPType::DEFEAT_TEAM) &&
          (this->player_state()->get_sc_card().get() == this)) {
        for (size_t set_index = 0; set_index < 8; set_index++) {
          auto card = this->player_state()->get_set_card(set_index);
          if (card) {
            card->card_flags |= 2;
          }
        }
      }

      for (size_t client_id = 0; client_id < 4; client_id++) {
        if (!this->server()->player_states[client_id]) {
          continue;
        }
        size_t num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(client_id);
        for (size_t z = 0; z < num_assists; z++) {
          auto eff = this->server()->assist_server->get_active_assist_by_index(z);
          if (eff == AssistEffect::HOMESICK) {
            if (client_id == this->client_id) {
              this->player_state()->return_set_card_to_hand2(this->card_ref);
            }
          } else if (eff == AssistEffect::INHERITANCE) {
            uint8_t other_team_id = this->server()->player_states[client_id]->get_team_id();
            uint8_t this_team_id = this->player_state()->get_team_id();
            if (this_team_id == other_team_id) {
              this->server()->add_team_exp(team_id, this->max_hp);
            }
          }
        }
      }

    } else if (this->w_destroyer_sc_card.expired() && attacker_card) {
      this->w_destroyer_sc_card = attacker_card->player_state()->get_sc_card();
    }
  }
}

int32_t Card::error_code_for_move_to_location(const Location& loc) const {
  if (this->player_state()->assist_flags & 0x80) {
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

  this->card_flags = this->card_flags & 0xFFFFFFF3;
  int16_t attack_ap = this->action_metadata.attack_bonus;
  int16_t attack_tp = 0;
  int16_t defense_power = this->compute_defense_power_for_attacker_card(attacker_card);
  if ((attack_ap == 0) && !this->action_metadata.check_flag(0x20)) {
    return;
  }

  G_ApplyConditionEffect_GC_Ep3_6xB4x06 cmd;
  cmd.effect.flags = 0x01;
  cmd.effect.attacker_card_ref = attacker_card->card_ref;
  cmd.effect.target_card_ref = this->card_ref;
  if (attacker_card->action_chain.chain.attack_medium == AttackMedium::UNKNOWN_03) {
    for (size_t strike_num = 0; strike_num < attacker_card->action_chain.chain.strike_count; strike_num++) {
      this->current_hp = min<int16_t>(
          this->current_hp + attacker_card->action_chain.chain.effective_tp,
          this->max_hp);
    }
    this->propagate_shared_hp_if_needed();
    cmd.effect.tp = attacker_card->action_chain.chain.effective_tp;
    cmd.effect.value = -cmd.effect.tp;
    this->server()->send(cmd);

  } else {
    uint16_t attacker_card_ref = attacker_card->get_card_ref();
    this->server()->card_special->compute_attack_ap(
        this->shared_from_this(), &attack_ap, attacker_card_ref);
    this->apply_ap_adjust_assists_to_attack(attacker_card, &attack_ap, &defense_power);
    int16_t raw_damage = attack_ap - defense_power;
    // Note: The original code uses attack_tp here, even though it is always
    // zero at this point
    int16_t preliminary_damage = max<int16_t>(raw_damage, 0) - attack_tp;
    this->last_attack_preliminary_damage = preliminary_damage;
    uint16_t targeted_card_ref = this->get_card_ref();

    uint32_t unknown_a9 = 0;
    auto target = this->server()->card_special->compute_replaced_target_based_on_conditions(
        targeted_card_ref, 1, 0, attacker_card_ref, 0xFFFF, 0, &unknown_a9, 0xFF, 0, 0xFFFF);
    if (!target) {
      target = this->shared_from_this();
    }
    if (unknown_a9 != 0) {
      preliminary_damage = 0;
    }

    if (!(this->card_flags & 2) &&
        (!attacker_card || !(attacker_card->card_flags & 2))) {
      this->server()->card_special->unknown_80244E20(
          attacker_card, this->shared_from_this(), &preliminary_damage);
    }

    cmd.effect.current_hp = min<int16_t>(attack_ap, 99);
    cmd.effect.ap = min<int16_t>(defense_power, 99);
    cmd.effect.tp = attack_tp;
    this->player_state()->stats.num_attacks_taken++;
    if (!(target->card_flags & 2)) {
      for (size_t strike_num = 0; strike_num < attacker_card->action_chain.chain.strike_count; strike_num++) {
        int16_t final_effective_damage = 0;
        target->commit_attack(
            preliminary_damage, attacker_card, &cmd, strike_num, &final_effective_damage);
        // TODO: Is this the right interpretation? The original code does some
        // fancy bitwise magic that I didn't bother to decipher, because this
        // interpretation makes sense based on how the game works.
        this->player_state()->stats.action_card_negated_damage += max<int16_t>(
            0, this->current_defense_power - final_effective_damage);
      }
    } else {
      target->commit_attack(0, attacker_card, &cmd, 0, nullptr);
    }
    if (this != target.get()) {
      this->commit_attack(0, attacker_card, &cmd, 0, nullptr);
    }

    this->server()->send_6xB4x39();
  }
}

bool Card::get_attack_condition_value(
    ConditionType cond_type,
    uint16_t card_ref,
    uint8_t def_effect_index,
    uint16_t value,
    uint16_t* out_value) const {
  return this->action_chain.get_condition_value(
      cond_type, card_ref, def_effect_index, value, out_value);
}

shared_ptr<const DataIndex::CardEntry> Card::get_definition() const {
  return this->def_entry;
}

uint16_t Card::get_card_ref() const {
  return this->card_ref;
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
  int32_t code = this->error_code_for_move_to_location(loc);
  if (code) {
    return code;
  }

  uint32_t path_cost;
  uint32_t path_length;
  if (!this->server()->ruler_server->get_move_path_length_and_cost(
        this->client_id, this->card_ref, loc, &path_length, &path_cost)) {
    return -0x79;
  }

  this->player_state()->stats.total_move_distance += path_length;
  this->player_state()->subtract_atk_points(path_cost);
  this->loc = loc;
  this->card_flags = this->card_flags | 0x80;

  for (size_t warp_type = 0; warp_type < 5; warp_type++) {
    for (size_t warp_end = 0; warp_end < 2; warp_end++) {
      if ((this->server()->warp_positions[warp_type][warp_end][0] == this->loc.x) &&
          (this->server()->warp_positions[warp_type][warp_end][1] == this->loc.y)) {
        G_Unknown_GC_Ep3_6xB4x2C cmd;
        cmd.loc.x = this->loc.x;
        cmd.loc.y = this->loc.y;
        this->loc.x = this->server()->warp_positions[warp_type][warp_end ^ 1][0];
        this->loc.y = this->server()->warp_positions[warp_type][warp_end ^ 1][1];
        cmd.change_type = 0;
        cmd.card_refs[0] = this->card_ref;
        this->server()->send(cmd);
        return 0;
      }
    }
  }

  return 0;
}

void Card::propagate_shared_hp_if_needed() {
  if ((this->server()->base()->map_and_rules1->rules.hp_type == HPType::COMMON_HP) &&
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
  ssize_t index = -1;
  if (this->card_ref == this->player_state()->get_sc_card_ref()) {
    index = 0;
  } else {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      if (this->card_ref == this->player_state()->get_set_ref(set_index)) {
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
  this->send_6xB4x4E_if_needed(always_send);

  auto& chain = this->player_state()->set_card_action_chains->at(index);
  if (always_send || (chain != this->action_chain)) {
    chain = this->action_chain;
    if (!this->server()->get_should_copy_prev_states_to_current_states()) {
      G_UpdateActionChain_GC_Ep3_6xB4x4C cmd;
      cmd.client_id = this->client_id;
      cmd.index = index;
      cmd.chain = this->action_chain.chain;
      this->server()->send(cmd);
    }
  }

  auto& metadata = this->player_state()->set_card_action_metadatas->at(index);
  if (always_send || (metadata != this->action_metadata)) {
    metadata = this->action_metadata;
    G_UpdateActionMetadata_GC_Ep3_6xB4x4D cmd;
    cmd.client_id = this->client_id;
    cmd.index = index;
    cmd.metadata = this->action_metadata;
    this->server()->send(cmd);
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
        G_UpdateCardConditions_GC_Ep3_6xB4x4E cmd;
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
  this->player_state()->num_destroyed_fcs++;
  this->server()->team_num_ally_fcs_destroyed[this->team_id]++;
  this->server()->team_num_cards_destroyed[this->team_id]++;

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = this->server()->player_states[client_id];
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

void Card::compute_action_chain_results(
    bool apply_action_conditions, bool ignore_this_card_ap_tp) {
  this->action_chain.compute_attack_medium(this->server());
  this->action_chain.chain.strike_count = 1;
  this->action_chain.chain.ap_effect_bonus = 0;
  this->action_chain.chain.tp_effect_bonus = 0;

  int16_t card_ap;
  int16_t card_tp;
  auto stat_swap_type = this->server()->card_special->compute_stat_swap_type(this->shared_from_this());
  this->server()->card_special->get_effective_ap_tp(
      stat_swap_type, &card_ap, &card_tp, this->get_current_hp(), this->ap, this->tp);

  int16_t effective_tp = card_tp;
  int16_t effective_ap = card_ap;
  for (size_t z = 0; (!ignore_this_card_ap_tp && (z < 8) && (z < this->action_chain.chain.attack_action_card_ref_count)); z++) {
    auto ce = this->server()->definition_for_card_ref(this->action_chain.chain.attack_action_card_refs[z]);
    if (ce) {
      effective_ap += ce->def.ap.stat;
      effective_tp += ce->def.tp.stat;
    }
  }

  // Add AP/TP from MAG items to SC's AP/TP
  if (this->def_entry->def.is_sc()) {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = this->player_state()->get_set_card(set_index);
      if ((card && (card->def_entry->def.card_class() == CardClass::MAG_ITEM)) && !(card->card_flags & 2)) {
        this->server()->card_special->get_effective_ap_tp(
            stat_swap_type, &card_ap, &card_tp, card->get_current_hp(), card->ap, card->tp);
        effective_ap += card_ap;
        effective_tp += card_tp;
      }
    }
  }

  if ((this->def_entry->def.type == CardType::ITEM) && this->sc_def_entry) {
    auto sc_card = this->player_state()->get_sc_card();
    sc_card->compute_action_chain_results(apply_action_conditions, true);
    effective_ap += sc_card->action_chain.chain.effective_ap + sc_card->action_chain.chain.ap_effect_bonus;
    effective_tp += sc_card->action_chain.chain.effective_tp + sc_card->action_chain.chain.tp_effect_bonus;
  }

  if (!this->action_chain.check_flag(0x10)) {
    this->action_chain.chain.effective_ap = min<int16_t>(effective_ap, 99);
  }
  if (!this->action_chain.check_flag(0x20)) {
    this->action_chain.chain.effective_tp = min<int16_t>(effective_tp, 99);
  }

  if (apply_action_conditions) {
    this->server()->card_special->apply_action_conditions(
        3, this->shared_from_this(), this->shared_from_this(), 1, nullptr);
  }

  size_t num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    switch (this->server()->assist_server->get_active_assist_by_index(z)) {
      case AssistEffect::POWERLESS_RAIN:
        if (this->card_type_is_sc_or_creature() &&
            (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          this->action_chain.chain.ap_effect_bonus -= 2;
        }
        break;
      case AssistEffect::BRAVE_WIND:
        if (this->card_type_is_sc_or_creature() &&
            (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL)) {
          this->action_chain.chain.ap_effect_bonus += 2;
        }
        break;
      case AssistEffect::INFLUENCE:
        if (this->card_type_is_sc_or_creature()) {
          int16_t count = this->player_state()->count_set_refs();
          this->action_chain.chain.ap_effect_bonus += (count >> 1);
        }
        break;
      case AssistEffect::AP_ABSORPTION:
        if (this->action_chain.chain.attack_medium == AttackMedium::TECH) {
          this->action_chain.chain.tp_effect_bonus += 2;
        }
        break;
      case AssistEffect::TECH_FIELD:
        if (this->card_type_is_sc_or_creature()) {
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
            auto other_ps = this->server()->get_player_state(client_id);
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
          this->action_chain.chain.ap_effect_bonus +=
              (this->server()->team_num_ally_fcs_destroyed[this->team_id] / 3);
        }
        break;
      default:
        break;
    }
  }

  int16_t damage = 0;
  if (this->action_chain.chain.attack_medium == AttackMedium::TECH) {
    damage = this->action_chain.chain.effective_tp + this->action_chain.chain.tp_effect_bonus;
  } else if (this->action_chain.chain.attack_medium == AttackMedium::PHYSICAL) {
    damage = this->action_chain.chain.effective_ap + this->action_chain.chain.ap_effect_bonus;
  }
  this->action_chain.chain.damage = min<int16_t>(
      damage * this->action_chain.chain.damage_multiplier, 99);

  if (apply_action_conditions) {
    this->server()->card_special->apply_action_conditions(
        3, this->shared_from_this(), this->shared_from_this(), 2, nullptr);
    if (this->action_chain.check_flag(0x100)) {
      this->action_chain.chain.damage = min<int16_t>(this->action_chain.chain.damage + 5, 99);
    }
  }

  num_assists = this->server()->assist_server->compute_num_assist_effects_for_client(this->get_client_id());
  for (size_t z = 0; z < num_assists; z++) {
    switch (this->server()->assist_server->get_active_assist_by_index(z)) {
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

void Card::unknown_802380C0() {
  this->card_flags &= 0xFFFFF7FB;
  this->action_metadata.clear_flags(0x30);
  this->action_chain.clear_flags(0x140);
  this->unknown_80237F98(0);
}

void Card::unknown_80237F98(bool require_condition_20_or_21) {
  bool should_send_updates = false;
  for (ssize_t z = 8; z >= 0; z--) {
    if (this->action_chain.conditions[z].type != ConditionType::NONE) {
      if (!require_condition_20_or_21 ||
          this->server()->card_special->condition_has_when_20_or_21(
            this->action_chain.conditions[z])) {
        ActionState as;
        auto& cond = this->action_chain.conditions[z];
        if (!this->server()->card_special->is_card_targeted_by_condition(
              cond, as, this->shared_from_this())) {
          this->server()->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(
              cond, this->shared_from_this());
          should_send_updates = true;
        } else if (this->action_chain.conditions[z].remaining_turns == 0) {
          if (--this->action_chain.conditions[z].a_arg_value < 1) {
            this->server()->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(
                cond, this->shared_from_this());
            should_send_updates = true;
          }
        }
      }
    }
  }

  this->compute_action_chain_results(1, false);
  this->unknown_80236554(nullptr, nullptr);
  if (should_send_updates) {
    this->send_6xB4x4E_4C_4D_if_needed();
  }
}

void Card::unknown_80237F88() {
  this->card_flags &= 0xFFFFF8FF;
}

void Card::unknown_80235AA0() {
  this->facing_direction = Direction::INVALID_FF;
  this->server()->card_special->unknown_80249060(this->shared_from_this());
}

void Card::unknown_80235AD4() {
  this->clear_action_chain_and_metadata_and_most_flags();
  this->server()->card_special->unknown_80249254(this->shared_from_this());
}

void Card::unknown_80235B10() {
  this->server()->card_special->unknown_80244BE4(this->shared_from_this());
}

void Card::unknown_80236374(shared_ptr<Card> other_card, const ActionState* as) {
  auto check_card = [&](shared_ptr<Card> card) -> void {
    if (card) {
      if (!card->unknown_80236554(other_card, as)) {
        card->action_metadata.clear_flags(0x20);
      } else {
        card->action_metadata.set_flags(0x20);
      }
    }
  };

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->player_states[client_id];
    if (ps) {
      if (this->server()->get_current_team_turn2() != ps->get_team_id()) {
        check_card(ps->get_sc_card());
        for (size_t set_index = 0; set_index < 8; set_index++) {
          check_card(ps->get_set_card(set_index));
        }
      }
    }
  }

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->player_states[client_id];
    if (ps) {
      if (this->server()->get_current_team_turn2() == ps->get_team_id()) {
        check_card(ps->get_sc_card());
        for (size_t set_index = 0; set_index < 8; set_index++) {
          check_card(ps->get_set_card(set_index));
        }
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

void Card::unknown_8023813C() {
  this->unknown_a9++;
  for (ssize_t z = 8; z >= 0; z--) {
    auto& cond = this->action_chain.conditions[z];
    if (cond.type != ConditionType::NONE) {
      ActionState as;
      if ((this->card_flags & 2) ||
          !this->server()->card_special->is_card_targeted_by_condition(cond, as, this->shared_from_this())) {
        cond.remaining_turns = 1;
      }
      if (cond.remaining_turns < 99) {
        cond.remaining_turns--;
        if (cond.remaining_turns < 1) {
          this->server()->card_special->apply_stat_deltas_to_card_from_condition_and_clear_cond(
              cond, this->shared_from_this());
        }
      }
    }
  }
  this->server()->card_special->unknown_80244CA8(this->shared_from_this());
}

bool Card::is_guard_item() const {
  return (this->def_entry->def.card_class() == CardClass::GUARD_ITEM);
}

bool Card::unknown_80236554(shared_ptr<Card> other_card, const ActionState* as) {
  bool ret = false;

  int16_t attack_bonus = 0;
  if (other_card) {
    if (!as) {
      for (size_t z = 0; z < other_card->action_chain.chain.target_card_ref_count; z++) {
        if (other_card->action_chain.chain.target_card_refs[z] == this->get_card_ref()) {
          attack_bonus = other_card->action_chain.chain.damage;
          ret = true;
          break;
        }
      }
    } else {
      for (size_t z = 0; (z < 4 * 9) && (as->target_card_refs[z] != 0xFFFF); z++) {
        if (as->target_card_refs[z] == this->get_card_ref()) {
          attack_bonus = other_card->action_chain.chain.damage;
          ret = true;
          break;
        }
      }
    }
  }

  this->action_metadata.attack_bonus = max<int16_t>(attack_bonus, 0);
  this->last_attack_preliminary_damage = 0;
  this->last_attack_final_damage = 0;

  if (other_card) {
    this->server()->card_special->apply_action_conditions(
        3, other_card, this->shared_from_this(), 0x20, as);
    this->server()->card_special->apply_action_conditions(
        0x17, other_card, this->shared_from_this(), 0x40, as);
    if (other_card->action_chain.check_flag(0x20000)) {
      this->action_metadata.attack_bonus = 0;
      return ret;
    }
  }
  if (!(this->card_flags & 2)) {
    return ret;
  }
  this->action_metadata.attack_bonus = 0;
  return ret;
}

void Card::unknown_802362D8(shared_ptr<Card> other_card) {
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->player_states[client_id];
    if (ps) {
      shared_ptr<Card> card = ps->get_sc_card();
      if (card) {
        card->execute_attack(other_card);
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        shared_ptr<Card> card = ps->get_set_card(set_index);
        if (card) {
          card->execute_attack(other_card);
        }
      }
    }
  }
}

void Card::unknown_80237734() {
  if (!this->action_chain.unknown_8024DEC4()) {
    return;
  }

  if (this->player_state()->stats.max_attack_combo_size < this->action_chain.chain.attack_action_card_ref_count) {
    this->player_state()->stats.max_attack_combo_size = this->action_chain.chain.attack_action_card_ref_count;
  }

  ActionState as;
  as.attacker_card_ref = this->get_card_ref();
  as.target_card_refs = this->action_chain.chain.target_card_refs;
  this->server()->replace_targets_due_to_destruction_or_conditions(&as);
  this->action_chain.chain.target_card_refs = as.target_card_refs;
  this->action_chain.chain.target_card_ref_count = 0;
  for (size_t z = 0; z < 4 * 9; z++) {
    if (this->action_chain.chain.target_card_refs[z] != 0xFFFF) {
      this->action_chain.chain.target_card_ref_count++;
    } else {
      break;
    }
  }

  for (size_t z = 0; z < this->action_chain.chain.target_card_ref_count; z++) {
    shared_ptr<Card> card = this->server()->card_for_set_card_ref(this->action_chain.chain.target_card_refs[z]);
    if (card) {
      card->current_defense_power = card->action_metadata.attack_bonus;
      if (!this->action_chain.check_flag(0x40)) {
        this->server()->card_special->unknown_8024A6DC(this->shared_from_this(), card);
      }
    }
  }

  this->compute_action_chain_results(1, 0);
  if (!this->action_chain.check_flag(0x40)) {
    this->server()->card_special->unknown_8024997C(this->shared_from_this());
  }
  if (!(this->card_flags & 2)) {
    this->compute_action_chain_results(1, 0);
    this->server()->card_special->unknown_8024504C(this->shared_from_this());
  }
  this->compute_action_chain_results(1, 0);
  this->unknown_80236374(this->shared_from_this(), nullptr);
  this->unknown_802362D8(this->shared_from_this());
  if (!this->action_chain.check_flag(0x40)) {
    this->server()->card_special->unknown_8024A394(this->shared_from_this());
  }
  this->player_state()->stats.num_attacks_given++;
  this->action_chain.clear_flags(8);
  this->action_chain.set_flags(4);
  this->card_flags |= 0x200;
  this->action_chain.clear_target_card_refs();
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->server()->player_states[client_id];
    if (ps) {
      ps->unknown_8023C110();
    }
  }
  this->send_6xB4x4E_4C_4D_if_needed();
}



} // namespace Episode3
