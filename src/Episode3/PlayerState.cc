#include "PlayerState.hh"

#include "Server.hh"

using namespace std;

namespace Episode3 {

PlayerState::PlayerState(uint8_t client_id, shared_ptr<Server> server)
    : w_server(server),
      client_id(client_id),
      num_mulligans_allowed(1),
      sc_card_type(CardType::HUNTERS_SC),
      team_id(0xFF),
      atk_points(0),
      def_points(0),
      atk_points2(0),
      atk_points2_max(6),
      atk_bonuses(0),
      def_bonuses(0),
      dice_results(0),
      unknown_a4(2),
      dice_max(6),
      total_set_cards_cost(0),
      sc_card_id(0xFFFF),
      sc_card_ref(0xFFFF),
      card_refs(0xFFFF),
      discard_log_card_refs(0xFFFF),
      discard_log_reasons(0),
      assist_remaining_turns(0),
      assist_card_set_number(0),
      set_assist_card_id(0xFFFF),
      god_whim_can_use_hidden_cards(false),
      unknown_a14(0),
      assist_flags(0),
      assist_delay_turns(0),
      start_facing_direction(Direction::RIGHT),
      num_destroyed_fcs(0),
      unknown_a16(0),
      unknown_a17(0) {}

void PlayerState::init() {
  auto s = this->server();
  auto log = s->log_stack("PlayerState::init: ");

  if (s->player_states.at(this->client_id).get() != this) {
    // Note: The original code handles this, but we don't. This appears not to
    // ever happen, so we didn't bother implementing it.
    throw logic_error("replacing a player state object is not permitted");
  }

  this->deck_state = make_shared<DeckState>(this->client_id, s->deck_entries[client_id]->card_ids, s);
  if (s->map_and_rules->rules.disable_deck_shuffle) {
    this->deck_state->disable_shuffle();
  }
  if (s->map_and_rules->rules.disable_deck_loop) {
    this->deck_state->disable_loop();
  }

  this->sc_card_ref = this->deck_state->sc_card_ref();
  this->sc_card_id = s->card_id_for_card_ref(this->sc_card_ref);
  this->team_id = s->deck_entries[this->client_id]->team_id;
  auto sc_ce = s->definition_for_card_ref(this->sc_card_ref);
  if (!sc_ce) {
    throw runtime_error("SC card definition is missing");
  }
  if (sc_ce->def.type == CardType::HUNTERS_SC) {
    this->sc_card_type = CardType::HUNTERS_SC;
  } else if (sc_ce->def.type == CardType::ARKZ_SC) {
    this->sc_card_type = CardType::ARKZ_SC;
  } else {
    // In the original code, sc_card_type gets left as 0xFFFFFFFF (yes, it's a
    // uint32_t). This probably breaks some things later on, so we instead
    // prevent it upfront.
    throw runtime_error("SC card is not a Hunters or Arkz SC");
  }

  this->hand_and_equip = make_shared<HandAndEquipState>();
  this->card_short_statuses = make_shared<parray<CardShortStatus, 0x10>>();
  this->set_card_action_chains = make_shared<parray<ActionChainWithConds, 9>>();
  this->set_card_action_metadatas = make_shared<parray<ActionMetadata, 9>>();

  this->hand_and_equip->clear_FF();
  for (size_t z = 0; z < 0x10; z++) {
    this->card_short_statuses->at(z).clear_FF();
  }
  for (size_t z = 0; z < 9; z++) {
    this->set_card_action_chains->at(z).clear_FF();
    this->set_card_action_metadatas->at(z).clear_FF();
  }

  this->sc_card = make_shared<Card>(this->deck_state->sc_card_id(), this->sc_card_ref, this->client_id, s);
  this->sc_card->init();
  this->draw_initial_hand();
  if (s->options.is_nte()) {
    this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    this->send_set_card_updates(true);
  }

  s->assist_server->hand_and_equip_states[this->client_id] = this->hand_and_equip;
  s->assist_server->card_short_statuses[this->client_id] = this->card_short_statuses;
  s->assist_server->deck_entries[this->client_id] = s->deck_entries[this->client_id];
  s->assist_server->set_card_action_chains[this->client_id] = this->set_card_action_chains;
  s->assist_server->set_card_action_metadatas[this->client_id] = this->set_card_action_metadatas;
  s->ruler_server->register_player(
      this->client_id,
      this->hand_and_equip,
      this->card_short_statuses,
      s->deck_entries[this->client_id],
      this->set_card_action_chains,
      this->set_card_action_metadatas);
  s->ruler_server->set_client_team_id(this->client_id, this->team_id);

  s->card_special->on_card_set(this->shared_from_this(), this->sc_card_ref);

  this->god_whim_can_use_hidden_cards = (s->deck_entries[this->client_id]->god_whim_flag != 3);
}

shared_ptr<Server> PlayerState::server() {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const Server> PlayerState::server() const {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

bool PlayerState::is_alive() const {
  auto sc_card = this->get_sc_card();
  return (sc_card && !(sc_card->card_flags & 2));
}

bool PlayerState::draw_cards_allowed() const {
  if (this->assist_flags & AssistFlag::IS_SKIPPING_TURN) {
    return false;
  }

  auto s = this->server();
  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = s->assist_server->get_active_assist_by_index(z);
    if (eff == AssistEffect::SKIP_DRAW) {
      return false;
    }
  }
  return true;
}

void PlayerState::apply_assist_card_effect_on_set(
    shared_ptr<PlayerState> setter_ps) {
  auto s = this->server();

  uint16_t assist_card_id = this->set_assist_card_id;
  if (assist_card_id == 0xFFFF) {
    assist_card_id = s->card_id_for_card_ref(this->card_refs[6]);
  }

  auto assist_effect = assist_effect_number_for_card_id(assist_card_id, s->options.is_nte());
  if ((assist_effect == AssistEffect::RESISTANCE) ||
      (assist_effect == AssistEffect::INDEPENDENT)) {
    this->assist_card_set_number = 0;
  }

  if (s->assist_server->should_block_assist_effects_for_client(this->client_id)) {
    return;
  }

  switch (assist_effect) {
    case AssistEffect::CARD_RETURN: {
      size_t hand_index;
      for (hand_index = 0; hand_index < 6; hand_index++) {
        if (this->card_refs[hand_index] == 0xFFFF) {
          break;
        }
      }

      if (hand_index < 6) {
        if (s->options.is_nte()) {
          if (this->deck_state->draw_card_by_ref(this->discard_log_card_refs[0])) {
            this->pop_from_discard_log(0);
          }
        } else {
          for (size_t z = 0; z < 0x10; z++) {
            if (this->deck_state->draw_card_by_ref(this->discard_log_card_refs[z])) {
              this->card_refs[hand_index] = this->discard_log_card_refs[z];
              this->discard_log_card_refs[z] = 0xFFFF;
              break;
            }
          }
        }
      }
      break;
    }

    case AssistEffect::ATK_DICE_2:
      this->assist_delay_turns = (!setter_ps || (setter_ps->team_id == this->team_id)) ? 2 : 1;
      break;

    case AssistEffect::EXCHANGE: {
      uint8_t t = this->atk_points;
      this->atk_points = this->def_points;
      this->def_points = t;
      this->atk_points2 = this->atk_points;
      this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      break;
    }

    case AssistEffect::SKIP_SET:
    case AssistEffect::SKIP_ACT:
      if (!s->options.is_nte()) {
        this->assist_delay_turns = 2;
      }
      break;

    case AssistEffect::NECROMANCER: {
      ssize_t hand_index;
      for (hand_index = 5; hand_index >= 0; hand_index--) {
        if (this->card_refs[hand_index] != 0xFFFF) {
          break;
        }
      }

      size_t log_index;
      for (log_index = 0; log_index < 0x10; log_index++) {
        auto ce = s->definition_for_card_ref(this->discard_log_card_refs[log_index]);
        if (ce && ((ce->def.type == CardType::ITEM || ce->def.type == CardType::CREATURE))) {
          break;
        }
      }

      if ((hand_index >= 0) && (log_index < 0x10) &&
          this->deck_state->draw_card_by_ref(this->discard_log_card_refs[log_index])) {
        uint16_t hand_card_ref = this->card_refs[hand_index];
        this->card_refs[hand_index] = this->discard_log_card_refs[log_index];
        this->discard_log_card_refs[log_index] = hand_card_ref;
        this->deck_state->set_card_discarded(hand_card_ref);
      }
      break;
    }

    case AssistEffect::LEGACY: {
      uint16_t total_cost = 0;
      for (ssize_t z = 7; z >= 0; z--) {
        shared_ptr<const Card> card = this->set_cards[z];
        if (card) {
          auto ce = card->get_definition();
          uint8_t card_cost = ce->def.self_cost;
          if (this->discard_card_or_add_to_draw_pile(this->card_refs[8 + z], false)) {
            total_cost += card_cost;
          }
        }
      }

      bool is_nte = s->options.is_nte();
      if (!is_nte) {
        this->on_cards_destroyed();
      }
      this->atk_points = min<uint8_t>(9, this->atk_points + (total_cost >> 1));
      this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      if (!is_nte) {
        s->send_6xB4x05();
      }
      break;
    }

    case AssistEffect::MUSCULAR:
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto other_ps = s->get_player_state(client_id);
        if (other_ps) {
          for (size_t set_index = 0; set_index < 8; set_index++) {
            auto card = other_ps->get_set_card(set_index);
            if (card) {
              card->ap++;
              card->send_6xB4x4E_4C_4D_if_needed();
            }
          }
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
        }
      }
      break;

    case AssistEffect::CHANGE_BODY:
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto other_ps = s->get_player_state(client_id);
        if (other_ps) {
          for (size_t set_index = 0; set_index < 8; set_index++) {
            auto card = other_ps->get_set_card(set_index);
            if (card) {
              uint8_t orig_ap = card->ap;
              card->ap = card->tp;
              card->tp = orig_ap;
              card->send_6xB4x4E_4C_4D_if_needed();
            }
          }
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
        }
      }
      break;

    case AssistEffect::GOD_WHIM:
      this->replace_all_set_assists_with_random_assists();
      break;

    case AssistEffect::ASSIST_RETURN:
      if (this->card_refs[7] != 0xFFFF) {
        uint8_t client_id = client_id_for_card_ref(this->card_refs[7]);
        auto other_ps = s->get_player_state(client_id);
        if (other_ps.get() != this) {
          other_ps->deck_state->draw_card_by_ref(this->card_refs[7]);
          other_ps->set_card_from_hand(
              this->card_refs[7], 0xF, nullptr, client_id, 1);
        }
      }
      break;

    case AssistEffect::REQUIEM:
      s->add_team_exp(this->team_id, this->num_destroyed_fcs << 1);
      s->update_battle_state_flags_and_send_6xB4x03_if_needed();

      this->num_destroyed_fcs = 0;
      if (!s->options.is_nte()) {
        s->team_num_cards_destroyed[this->team_id] = 0;
        for (size_t client_id = 0; client_id < 4; client_id++) {
          const auto other_ps = s->get_player_state(client_id);
          if (other_ps && (this->team_id == other_ps->get_team_id())) {
            auto card = other_ps->get_sc_card();
            if (card) {
              card->num_cards_destroyed_by_team_at_set_time = 0;
              card->num_destroyed_ally_fcs = 0;
            }
            for (size_t set_index = 0; set_index < 8; set_index++) {
              auto set_card = other_ps->get_set_card(set_index);
              if (set_card) {
                set_card->num_cards_destroyed_by_team_at_set_time = 0;
                set_card->num_destroyed_ally_fcs = 0;
              }
            }
          }
        }
      }
      break;

    case AssistEffect::SLOW_TIME: {
      bool is_nte = s->options.is_nte();
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto other_ps = s->get_player_state(client_id);
        if (!other_ps) {
          continue;
        }

        if (is_nte
                ? (other_ps->assist_remaining_turns != 90 && other_ps->assist_remaining_turns != 99)
                : (other_ps->assist_remaining_turns < 10)) {
          other_ps->assist_remaining_turns = min<uint8_t>(9, other_ps->assist_remaining_turns << 1);
        }

        for (ssize_t set_index = is_nte ? 0 : -1; set_index < 8; set_index++) {
          auto card = (set_index == -1)
              ? other_ps->get_sc_card()
              : other_ps->get_set_card(set_index);
          if (card) {
            for (size_t cond_index = 0; cond_index < 9; cond_index++) {
              auto& cond = card->action_chain.conditions[cond_index];
              if (cond.type == ConditionType::NONE) {
                continue;
              }
              if (is_nte) {
                if (cond.remaining_turns < 49) {
                  cond.remaining_turns <<= 1;
                }
              } else if (cond.remaining_turns < 10) {
                cond.remaining_turns = min<uint8_t>(9, cond.remaining_turns << 1);
              }
            }
          }
        }
        if (!is_nte) {
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
          other_ps->send_set_card_updates();
        }
      }
      break;
    }

    case AssistEffect::QUICK_TIME: {
      bool is_nte = s->options.is_nte();
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto other_ps = s->get_player_state(client_id);
        if (!other_ps) {
          continue;
        }

        if (is_nte
                ? (other_ps->assist_remaining_turns != 90 && other_ps->assist_remaining_turns != 99)
                : (other_ps->assist_remaining_turns < 10)) {
          other_ps->assist_remaining_turns = ((other_ps->assist_remaining_turns + 1) >> 1);
        }

        for (ssize_t set_index = is_nte ? 0 : -1; set_index < 8; set_index++) {
          auto card = (set_index == -1)
              ? other_ps->get_sc_card()
              : other_ps->get_set_card(set_index);
          if (card) {
            for (size_t cond_index = 0; cond_index < 9; cond_index++) {
              auto& cond = card->action_chain.conditions[cond_index];
              if ((cond.type != ConditionType::NONE) && (cond.remaining_turns < (is_nte ? 99 : 10))) {
                cond.remaining_turns = (cond.remaining_turns + 1) >> 1;
              }
            }
          }
        }
        if (!is_nte) {
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
          other_ps->send_set_card_updates();
        }
      }
      break;
    }

    case AssistEffect::SQUEEZE:
      this->set_random_assist_card_from_hand_for_free();
      break;

    case AssistEffect::BOMB:
      this->assist_delay_turns = 6;
      break;

    case AssistEffect::SKIP_TURN:
      if (!s->options.is_nte() && (!setter_ps || (setter_ps->team_id == this->team_id))) {
        this->assist_delay_turns = 6;
      } else {
        this->assist_delay_turns = 5;
      }
      break;

    default:
      break;
  }
}

void PlayerState::apply_dice_effects() {
  auto s = this->server();

  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = s->assist_server->get_active_assist_by_index(z);
    switch (eff) {
      case AssistEffect::DICE_FEVER:
        for (size_t die_index = 0; die_index < 2; die_index++) {
          if (this->dice_results[die_index] > 0) {
            this->dice_results[die_index] = s->options.is_nte() ? 6 : 5;
          }
        }
        break;
      case AssistEffect::DICE_HALF:
        for (size_t die_index = 0; die_index < 2; die_index++) {
          if (this->dice_results[die_index] > 0) {
            this->dice_results[die_index] = (this->dice_results[die_index] + 1) >> 1;
          }
        }
        break;
      case AssistEffect::DICE_PLUS_1:
        for (size_t die_index = 0; die_index < 2; die_index++) {
          if (this->dice_results[die_index] > 0) {
            this->dice_results[die_index] = this->dice_results[die_index] + 1;
          }
        }
        break;
      case AssistEffect::DICE_FEVER_PLUS:
        if (s->options.is_nte()) {
          break;
        }
        for (size_t die_index = 0; die_index < 2; die_index++) {
          if (this->dice_results[die_index] > 0) {
            this->dice_results[die_index] = 6;
          }
        }
        break;
      default:
        break;
    }
  }

  for (size_t die_index = 0; die_index < 2; die_index++) {
    this->dice_results[die_index] = min<uint8_t>(this->dice_results[die_index], 9);
  }
}

uint16_t PlayerState::card_ref_for_hand_index(size_t hand_index) const {
  return (hand_index < 6) ? this->card_refs[hand_index] : 0xFFFF;
}

int16_t PlayerState::compute_attack_or_defense_atk_costs(const ActionState& pa) const {
  auto s = this->server();
  return s->ruler_server->compute_attack_or_defense_costs(pa, 0, 0);
}

void PlayerState::compute_total_set_cards_cost() {
  this->total_set_cards_cost = 0;
  for (size_t set_index = 0; set_index < 8; set_index++) {
    auto card = this->set_cards[set_index];
    if (!card) {
      continue;
    }
    auto ce = card->get_definition();
    if (ce) {
      this->total_set_cards_cost += ce->def.self_cost;
    }
  }
}

size_t PlayerState::count_set_cards_for_env_stats_nte() const {
  size_t ret = 0;
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->card_refs[8 + set_index] != 0xFFFF) {
      ret++;
    }
  }
  return ret;
}

size_t PlayerState::count_set_cards() const {
  size_t ret = 0;
  for (size_t set_index = 0; set_index < 8; set_index++) {
    auto card = this->set_cards[set_index];
    if (card && !(card->card_flags & 2)) {
      ret++;
    }
  }
  return ret;
}

size_t PlayerState::count_set_refs() const {
  size_t ret = 0;
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->card_refs[set_index] != 0xFFFF) {
      ret++;
    }
  }
  return ret;
}

void PlayerState::discard_all_assist_cards_from_hand() {
  auto s = this->server();

  parray<uint16_t, 6> temp_card_refs;
  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    temp_card_refs[hand_index] = this->card_refs[hand_index];
  }

  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    uint16_t card_ref = temp_card_refs[hand_index];
    auto ce = s->definition_for_card_ref(card_ref);
    if (ce && (ce->def.type == CardType::ASSIST)) {
      this->discard_ref_from_hand(card_ref);
    }
  }

  this->move_null_hand_refs_to_end();
}

void PlayerState::discard_all_attack_action_cards_from_hand() {
  auto s = this->server();

  parray<uint16_t, 6> temp_card_refs;
  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    temp_card_refs[hand_index] = this->card_refs[hand_index];
  }

  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    uint16_t card_ref = temp_card_refs[hand_index];
    auto ce = s->definition_for_card_ref(card_ref);
    if (ce && (ce->def.type == CardType::ACTION) &&
        (ce->def.card_class() != CardClass::DEFENSE_ACTION)) {
      this->discard_ref_from_hand(card_ref);
    }
  }

  this->move_null_hand_refs_to_end();
}

void PlayerState::discard_all_item_and_creature_cards_from_hand() {
  auto s = this->server();

  parray<uint16_t, 6> temp_card_refs;
  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    temp_card_refs[hand_index] = this->card_refs[hand_index];
  }

  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    uint16_t card_ref = temp_card_refs[hand_index];
    auto ce = s->definition_for_card_ref(card_ref);
    if (ce && ((ce->def.type == CardType::ITEM) || (ce->def.type == CardType::CREATURE))) {
      this->discard_ref_from_hand(card_ref);
    }
  }

  this->move_null_hand_refs_to_end();
}

void PlayerState::discard_and_redraw_hand() {
  auto s = this->server();

  while (this->card_refs[0] != 0xFFFF) {
    this->discard_ref_from_hand(this->card_refs[0]);
  }

  if (!s->options.is_nte()) {
    G_EnqueueAnimation_Ep3_6xB4x2C cmd;
    cmd.change_type = 3;
    cmd.client_id = this->client_id;
    cmd.card_refs.clear(0xFFFF);
    cmd.unknown_a2.clear(0xFFFFFFFF);
    s->send(cmd);
  }

  this->deck_state->restart();
  this->draw_hand();
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
}

bool PlayerState::discard_card_or_add_to_draw_pile(uint16_t card_ref, bool add_to_draw_pile) {
  ssize_t set_index = this->set_index_for_card_ref(card_ref);
  if (set_index < 0) {
    return false;
  }

  this->deck_state->set_card_discarded(card_ref);
  this->card_refs[set_index + 8] = 0xFFFF;
  auto card = this->set_cards[set_index];
  if (card) {
    if (this->server()->options.is_nte()) {
      card->update_stats_on_destruction();
      this->set_cards[set_index].reset();
    } else {
      card->card_flags |= 2;
    }
  }
  if (add_to_draw_pile) {
    this->deck_state->set_card_ref_drawable_at_end(card_ref);
  }
  this->log_discard(card_ref, 0);
  return true;
}

void PlayerState::discard_random_hand_card() {
  size_t max = this->get_hand_size();
  if (max > 0) {
    auto s = this->server();
    this->discard_ref_from_hand(this->card_refs[s->get_random(max)]);
  }
  this->move_null_hand_refs_to_end();
}

bool PlayerState::discard_ref_from_hand(uint16_t card_ref) {
  ssize_t index = this->hand_index_for_card_ref(card_ref);
  if (index >= 0) {
    this->deck_state->set_card_discarded(card_ref);
    this->card_refs[index] = 0xFFFF;
    this->move_null_hand_refs_to_end();
    this->log_discard(card_ref, 0);
    this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    return true;
  } else {
    return false;
  }
}

void PlayerState::discard_set_assist_card() {
  auto s = this->server();

  this->set_assist_card_id = 0xFFFF;
  uint8_t client_id = client_id_for_card_ref(this->card_refs[6]);
  auto setter_ps = s->get_player_state(client_id);
  if (setter_ps) {
    setter_ps->get_deck()->set_card_discarded(this->card_refs[6]);
    this->card_refs[6] = 0xFFFF;
  }
  this->card_refs[7] = 0xFFFF;
  this->assist_remaining_turns = 0;
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();

  s->assist_server->populate_effects();

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->get_player_state(client_id);
    if (!other_ps) {
      continue;
    }
    uint32_t prev_assist_flags = other_ps->assist_flags;
    other_ps->set_assist_flags_from_assist_effects();
    if (other_ps->assist_flags != prev_assist_flags) {
      other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    }
  }

  s->destroy_cards_with_zero_hp();
}

bool PlayerState::do_mulligan() {
  if (!this->is_mulligan_allowed()) {
    return false;
  }

  auto s = this->server();

  this->num_mulligans_allowed--;
  while (this->card_refs[0] != 0xFFFF) {
    this->discard_ref_from_hand(this->card_refs[0]);
  }

  if (!s->options.is_nte()) {
    G_EnqueueAnimation_Ep3_6xB4x2C cmd;
    cmd.change_type = 3;
    cmd.client_id = this->client_id;
    cmd.card_refs.clear(0xFFFF);
    cmd.unknown_a2.clear(0xFFFFFFFF);
    s->send(cmd);
  }

  this->deck_state->do_mulligan(s->options.is_nte());
  this->draw_hand(5);

  if (!s->options.is_nte()) {
    this->discard_log_card_refs.clear(0xFFFF);
  }
  return true;
}

void PlayerState::draw_hand(ssize_t override_count) {
  auto s = this->server();

  ssize_t count = 5 - this->get_hand_size();
  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = s->assist_server->get_active_assist_by_index(z);
    if ((eff == AssistEffect::RICH_PLUS) && !s->options.is_nte()) {
      count = 4 - this->get_hand_size();
    } else if (eff == AssistEffect::RICH) {
      count = 6 - this->get_hand_size();
    }
  }

  if ((override_count != 0) && (override_count < count)) {
    count = override_count;
  }

  for (; count > 0; count--) {
    uint16_t card_ref = this->deck_state->draw_card();
    for (size_t z = 0; z < 6; z++) {
      if (this->card_refs[z] == 0xFFFF) {
        this->card_refs[z] = card_ref;
        break;
      }
    }
    if (!s->options.is_nte() && (s->get_setup_phase() == SetupPhase::MAIN_BATTLE)) {
      this->stats.num_cards_drawn++;
    }
  }

  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
}

void PlayerState::draw_initial_hand() {
  // Note: The original code called this->deck_state->init_card_states here, but
  // we don't because that logic is now in the DeckState constructor, and this
  // function should only be called during PlayerState construction (so, shortly
  // after DeckState construction as well).
  this->deck_state->restart();
  this->card_refs.clear(0xFFFF);
  this->draw_hand(5);
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
}

int32_t PlayerState::error_code_for_client_setting_card(
    uint16_t card_ref,
    uint8_t card_index,
    const Location* loc,
    uint8_t assist_target_client_id) const {
  auto s = this->server();

  int32_t code = s->ruler_server->error_code_for_client_setting_card(
      this->client_id, card_ref, loc, assist_target_client_id);
  if (code) {
    return code;
  }

  if (this->hand_index_for_card_ref(card_ref) < 0) {
    return -0x7F;
  }

  if (this->deck_state->state_for_card_ref(card_ref) != DeckState::CardState::IN_HAND) {
    return -0x7D;
  }

  auto ce = s->definition_for_card_ref(card_ref);
  if (!ce) {
    return -0x7D;
  }

  switch (ce->def.type) {
    case CardType::ITEM:
    case CardType::CREATURE:
      if ((card_index < 7) || (card_index >= 15)) {
        return -0x7E;
      }
      if (this->card_refs[card_index + 1] != 0xFFFF) {
        return -0x7E;
      }
      if ((ce->def.type == CardType::CREATURE) &&
          !s->map_and_rules->tile_is_vacant(loc->x, loc->y)) {
        return -0x7A;
      }
      return 0;

    case CardType::ASSIST:
      return (card_index == 15) ? 0 : -0x7E;

    case CardType::HUNTERS_SC:
    case CardType::ARKZ_SC:
    case CardType::ACTION:
    default:
      return -0x7D;
  }
}

vector<uint16_t> PlayerState::get_all_cards_within_range(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    uint8_t target_team_id) const {
  auto s = this->server();

  auto log = s->log_stack("get_all_cards_within_range: ");
  string loc_str = loc.str();
  log.debug("loc=%s, target_team_id=%02hhX", loc_str.c_str(), target_team_id);

  vector<uint16_t> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->player_states[client_id];
    if (other_ps &&
        ((target_team_id == 0xFF) || (target_team_id == other_ps->get_team_id()))) {
      auto card_refs = get_card_refs_within_range(range, loc, *other_ps->card_short_statuses, &log);
      ret.insert(ret.end(), card_refs.begin(), card_refs.end());
    }
  }
  return ret;
}

uint8_t PlayerState::get_atk_points() const {
  return this->atk_points;
}

void PlayerState::get_short_status_for_card_index_in_hand(size_t hand_index, CardShortStatus* stat) const {
  stat->card_ref = this->card_refs[hand_index - 1];
}

shared_ptr<DeckState> PlayerState::get_deck() {
  return this->deck_state;
}

uint8_t PlayerState::get_def_points() const {
  return this->def_points;
}

uint8_t PlayerState::get_dice_result(size_t which) const {
  return this->dice_results[which];
}

size_t PlayerState::get_hand_size() const {
  size_t ret = 0;
  for (size_t z = 0; z < 6; z++) {
    if (this->card_refs[z] != 0xFFFF) {
      ret++;
    }
  }
  return ret;
}

uint16_t PlayerState::get_sc_card_id() const {
  return this->sc_card_id;
}

shared_ptr<Card> PlayerState::get_sc_card() {
  return this->sc_card;
}

shared_ptr<const Card> PlayerState::get_sc_card() const {
  return this->sc_card;
}

uint16_t PlayerState::get_sc_card_ref() const {
  return this->sc_card_ref;
}

CardType PlayerState::get_sc_card_type() const {
  return this->sc_card_type;
}

shared_ptr<Card> PlayerState::get_set_card(size_t set_index) {
  return (set_index < 8) ? this->set_cards[set_index] : nullptr;
}

shared_ptr<const Card> PlayerState::get_set_card(size_t set_index) const {
  return (set_index < 8) ? this->set_cards[set_index] : nullptr;
}

uint16_t PlayerState::get_set_ref(size_t set_index) const {
  return this->card_refs[set_index + 8];
}

uint8_t PlayerState::get_team_id() const {
  return this->team_id;
}

ssize_t PlayerState::hand_index_for_card_ref(uint16_t card_ref) const {
  for (size_t z = 0; z < 6; z++) {
    if (this->card_refs[z] == card_ref) {
      return z;
    }
  }
  return -1;
}

size_t PlayerState::set_index_for_card_ref(uint16_t card_ref) const {
  for (size_t z = 0; z < 8; z++) {
    if (this->card_refs[z + 8] == card_ref) {
      return z;
    }
  }
  return -1;
}

bool PlayerState::is_mulligan_allowed() const {
  return (this->num_mulligans_allowed > 0);
}

bool PlayerState::is_team_turn() const {
  auto s = this->server();
  // Note: The original code checks if this->w_server is null before doing this.
  // We don't check because that should never happen, and server() will throw if
  // it does.
  return s->get_current_team_turn() == this->team_id;
}

void PlayerState::log_discard(uint16_t card_ref, uint16_t reason) {
  for (size_t z = this->discard_log_card_refs.size() - 1; z > 0; z--) {
    this->discard_log_card_refs[z] = this->discard_log_card_refs[z - 1];
    this->discard_log_reasons[z] = this->discard_log_reasons[z - 1];
  }
  this->discard_log_card_refs[0] = card_ref;
  this->discard_log_reasons[0] = reason;
}

uint16_t PlayerState::pop_from_discard_log(uint16_t) {
  // NTE appears to have a bug here (or some obviated code): it searches for an
  // entry with the given reason, then ignores the result of that search and
  // always returns the first entry instead.
  // size_t z;
  // for (size_t z = 0; z < this->discard_log_card_refs.size(); z++) {
  //   if ((this->discard_log_card_refs[z] != 0xFFFF) && (this->discard_log_reasons[z] == reason)) {
  //     break;
  //   }
  // }

  uint16_t ret = this->discard_log_card_refs[0];
  for (size_t z = 0; z < this->discard_log_card_refs.size() - 1; z++) {
    this->discard_log_card_refs[z] = this->discard_log_card_refs[z + 1];
    this->discard_log_reasons[z] = this->discard_log_reasons[z + 1];
  }
  this->discard_log_card_refs[this->discard_log_card_refs.size() - 1] = 0xFFFF;
  this->discard_log_reasons[this->discard_log_reasons.size() - 1] = 0;
  return ret;
}

bool PlayerState::move_card_to_location_by_card_index(size_t card_index, const Location& new_loc) {
  auto s = this->server();

  shared_ptr<Card> card;
  if (card_index == 0) {
    card = this->sc_card;
  } else {
    if ((card_index < 7) || (card_index >= 15)) {
      s->ruler_server->error_code2 = -0x78;
      return false;
    }
    card = this->set_cards[card_index - 7];
  }
  if (!card) {
    s->ruler_server->error_code2 = -0x78;
    return false;
  }

  int32_t code = card->error_code_for_move_to_location(new_loc);
  if (code) {
    s->ruler_server->error_code2 = code;
    return false;
  }

  card->move_to_location(new_loc);
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  this->send_6xB4x04_if_needed();
  s->send_6xB4x05();
  s->send_6xB4x39();
  s->card_special->apply_effects_after_card_move(card);
  return true;
}

void PlayerState::move_null_hand_refs_to_end() {
  size_t write_offset = 0;
  for (size_t read_offset = 0; read_offset < 6; read_offset++) {
    uint16_t card_ref = this->card_refs[read_offset];
    if (card_ref != 0xFFFF) {
      this->card_refs[write_offset++] = card_ref;
    }
  }
  for (; write_offset < 6; write_offset++) {
    this->card_refs[write_offset] = 0xFFFF;
  }
}

void PlayerState::on_cards_destroyed() {
  auto s = this->server();

  // {card_ref: should_return_to_hand}
  unordered_multimap<uint16_t, bool> card_refs_map;

  for (size_t z = 0; z < 8; z++) {
    auto card = this->set_cards[z];
    if (!card || !(card->card_flags & 2)) {
      continue;
    }

    uint16_t card_ref = this->card_refs[z + 8];
    card_refs_map.emplace(card_ref, s->card_special->should_return_card_ref_to_hand_on_destruction(this->card_refs[z + 8]));

    bool should_discard = true;
    for (size_t hand_index = 0; hand_index < 6; hand_index++) {
      if (this->card_refs[hand_index] == card_ref) {
        should_discard = false;
        break;
      }
    }

    if (should_discard) {
      this->log_discard(card_ref, 1);
      this->deck_state->set_card_discarded(this->card_refs[z + 8]);
    }

    this->card_refs[z + 8] = 0xFFFF;
    this->set_cards[z].reset();
  }

  size_t write_index = 0;
  bool indexes_diverged = false;
  for (size_t read_index = 0; read_index < 8; read_index++) {
    auto card = this->set_cards[read_index];
    if (card) {
      if (read_index != write_index) {
        this->set_cards[write_index] = card;
        this->card_refs[write_index + 8] = this->card_refs[read_index + 8];
        indexes_diverged = true;
      }
      write_index++;
    }
  }
  for (; write_index < 8; write_index++) {
    this->set_cards[write_index].reset();
    this->card_refs[write_index + 8] = 0xFFFF;
  }

  if (indexes_diverged) {
    this->send_set_card_updates();
  }

  for (const auto& it : card_refs_map) {
    uint16_t card_ref = it.first;
    bool should_return = it.second;

    if (should_return) {
      size_t hand_index;
      for (hand_index = 0; hand_index < 6; hand_index++) {
        if (this->card_refs[hand_index] == 0xFFFF) {
          break;
        }
      }
      if ((hand_index < 6) && this->deck_state->draw_card_by_ref(card_ref)) {
        this->card_refs[hand_index] = card_ref;
      }
    }
  }
}

void PlayerState::replace_all_set_assists_with_random_assists() {
  auto s = this->server();

  bool is_nte = s->options.is_nte();
  const auto& assist_card_ids = all_assist_card_ids(is_nte);
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->get_player_state(client_id);
    if (other_ps &&
        ((other_ps->card_refs[6] != 0xFFFF) || (!is_nte && (other_ps->set_assist_card_id != 0xFFFF)))) {
      uint16_t card_id = 0x0130;
      while (card_id == 0x0130) { // God Whim
        size_t index = s->get_random(assist_card_ids.size());
        card_id = assist_card_ids[index];
        // In NTE, God Whim can use ANY card, even unobtainable cards.
        if (!is_nte && !this->god_whim_can_use_hidden_cards) {
          auto ce = s->definition_for_card_id(card_id);
          if (!ce || ce->def.cannot_drop) {
            continue;
          }
        }
      }
      other_ps->replace_assist_card_by_id(card_id);
    }
  }
}

bool PlayerState::replace_assist_card_by_id(uint16_t card_id) {
  auto s = this->server();

  auto ce = s->definition_for_card_id(card_id);
  if (!ce || (ce->def.type != CardType::ASSIST)) {
    return false;
  }

  this->discard_set_assist_card();
  this->set_assist_card_id = card_id;
  this->assist_remaining_turns = ce->def.assist_turns;
  this->assist_card_set_number = s->next_assist_card_set_number++;
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  s->assist_server->populate_effects();

  if (!s->options.is_nte()) {
    for (size_t client_id = 0; client_id < 4; client_id++) {
      auto other_ps = s->get_player_state(client_id);
      if (other_ps) {
        uint32_t prev_assist_flags = other_ps->assist_flags;
        other_ps->set_assist_flags_from_assist_effects();
        if (prev_assist_flags != other_ps->assist_flags) {
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
        }
      }
    }
  }

  this->apply_assist_card_effect_on_set(this->shared_from_this());
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  return true;
}

bool PlayerState::return_set_card_to_hand2(uint16_t card_ref) {
  size_t set_index;
  for (set_index = 0; set_index < 8; set_index++) {
    if (this->card_refs[set_index + 8] == card_ref) {
      break;
    }
  }

  if (set_index < 8) {
    size_t hand_index;
    for (hand_index = 0; hand_index < 6; hand_index++) {
      if (this->card_refs[hand_index] == 0xFFFF) {
        break;
      }
    }
    if (hand_index < 6) {
      this->deck_state->set_card_discarded(card_ref);
      if (this->deck_state->draw_card_by_ref(card_ref)) {
        this->card_refs[hand_index] = card_ref;
        this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
        this->send_6xB4x04_if_needed();
        return true;
      }
    }
  }
  return false;
}

bool PlayerState::return_set_card_to_hand1(uint16_t card_ref) {
  size_t hand_index;
  for (hand_index = 0; hand_index < 6; hand_index++) {
    if (this->card_refs[hand_index] == 0xFFFF) {
      break;
    }
  }

  if ((hand_index < 6) && (card_ref != 0xFFFF)) {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = this->set_cards[set_index];
      if (card && (card->get_card_ref() == card_ref)) {
        uint16_t set_card_ref = this->card_refs[set_index + 8];
        this->card_refs[set_index + 8] = 0xFFFF;
        if (this->server()->options.is_nte()) {
          card->update_stats_on_destruction();
          this->set_cards[set_index].reset();
        } else {
          card->card_flags |= 2;
        }
        this->deck_state->set_card_discarded(set_card_ref);
        if (this->deck_state->draw_card_by_ref(set_card_ref)) {
          this->card_refs[hand_index] = set_card_ref;
          return true;
        }
      }
    }
  }

  return false;
}

uint8_t PlayerState::roll_dice(size_t num_dice) {
  auto s = this->server();

  uint8_t ret = 0;
  for (size_t z = 0; z < num_dice; z++) {
    this->dice_results[z] = s->get_random(this->dice_max) + 1;
    ret += this->dice_results[z];
  }

  if (num_dice < 1) {
    this->dice_results[0] = 0;
  }
  if (num_dice < 2) {
    this->dice_results[1] = 0;
  }

  return ret;
}

uint8_t PlayerState::roll_dice_with_effects(size_t num_dice) {
  this->roll_dice(num_dice);
  this->apply_dice_effects();
  return this->dice_results[0];
}

void PlayerState::send_set_card_updates(bool always_send) {
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  uint16_t mask = 0;
  if (this->sc_card) {
    this->sc_card->send_6xB4x4E_4C_4D_if_needed(always_send);
  } else if (is_nte) {
    this->send_6xB4x0A_for_set_card(0);
  } else {
    this->set_card_action_chains->at(0).clear();
    this->set_card_action_metadatas->at(0).clear();
    mask |= 1;
  }

  for (size_t set_index = 0; set_index < 8; set_index++) {
    auto card = this->set_cards[set_index];
    if (card) {
      card->send_6xB4x4E_4C_4D_if_needed(always_send);
    } else if (is_nte) {
      this->send_6xB4x0A_for_set_card(set_index + 1);
    } else {
      mask |= 1 << (set_index + 1);
      this->set_card_action_chains->at(set_index + 1).clear();
      this->set_card_action_metadatas->at(set_index + 1).clear();
    }
  }

  // mask will always be 0 here if is_nte is true
  if (mask && !s->get_should_copy_prev_states_to_current_states()) {
    G_ClearSetCardConditions_Ep3_6xB4x4F cmd;
    cmd.client_id = this->client_id;
    cmd.clear_mask = mask;
    s->send(cmd);
  }
}

void PlayerState::set_assist_flags_from_assist_effects() {
  auto s = this->server();

  this->assist_flags &= ~(
      AssistFlag::FIXED_RANGE |
      AssistFlag::SUMMONING_IS_FREE |
      AssistFlag::LIMIT_MOVE_TO_1 |
      AssistFlag::IMMORTAL |
      AssistFlag::SAME_CARD_BANNED |
      AssistFlag::CANNOT_SET_FIELD_CHARACTERS);
  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    switch (s->assist_server->get_active_assist_by_index(z)) {
      case AssistEffect::SIMPLE:
        this->assist_flags |= AssistFlag::FIXED_RANGE;
        break;
      case AssistEffect::TERRITORY:
        this->assist_flags |= AssistFlag::SAME_CARD_BANNED;
        break;
      case AssistEffect::OLD_TYPE:
        this->assist_flags |= AssistFlag::CANNOT_SET_FIELD_CHARACTERS;
        break;
      case AssistEffect::FLATLAND:
        this->assist_flags |= AssistFlag::SUMMONING_IS_FREE;
        break;
      case AssistEffect::IMMORTALITY:
        this->assist_flags |= AssistFlag::IMMORTAL;
        break;
      case AssistEffect::SNAIL_PACE:
        this->assist_flags |= AssistFlag::LIMIT_MOVE_TO_1;
        break;
      default:
        break;
    }
  }
  return;
}

bool PlayerState::set_card_from_hand(
    uint16_t card_ref,
    uint8_t card_index,
    const Location* loc,
    uint8_t assist_target_client_id,
    bool skip_error_checks_and_atk_sub) {
  auto s = this->server();

  if (!skip_error_checks_and_atk_sub) {
    int32_t code = this->error_code_for_client_setting_card(card_ref, card_index, loc, assist_target_client_id);
    if (code) {
      s->ruler_server->error_code1 = code;
      this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      return false;
    }
  }

  ssize_t hand_index = this->hand_index_for_card_ref(card_ref);
  if (hand_index >= 0) {
    this->card_refs[hand_index] = 0xFFFF;
    this->move_null_hand_refs_to_end();
  }

  if (!skip_error_checks_and_atk_sub) {
    int16_t cost = s->ruler_server->set_cost_for_card(this->client_id, card_ref);
    this->subtract_atk_points(cost);
  }

  this->deck_state->set_card_ref_in_play(card_ref);

  bool is_nte = s->options.is_nte();
  auto ce = s->definition_for_card_ref(card_ref);
  if (ce->def.type == CardType::ITEM || ce->def.type == CardType::CREATURE) {
    if ((card_index < 7) || (card_index >= 15)) {
      return 0;
    }
    this->card_refs[card_index + 1] = card_ref;
    // Note: NTE doesn't call the destructor on the existing card, if there is
    // one. Is that a bug?
    this->set_cards[card_index - 7] = make_shared<Card>(s->card_id_for_card_ref(card_ref), card_ref, this->client_id, s);
    auto new_card = this->set_cards[card_index - 7];
    new_card->init();

    if (ce->def.type == CardType::CREATURE) {
      new_card->loc.x = loc->x;
      new_card->loc.y = loc->y;
    }
    // Note: NTE doesn't track this, but NTE can't use it anyway, so we don't
    // check for NTE here.
    this->stats.num_item_or_creature_cards_set++;

  } else if (ce->def.type == CardType::ASSIST) {
    if (card_index != 15) {
      return false;
    }

    auto target_ps = s->player_states.at(assist_target_client_id);
    if (target_ps) {
      uint16_t prev_assist_card_ref = target_ps->card_refs[6];
      target_ps->discard_set_assist_card();
      target_ps->card_refs[6] = card_ref;
      target_ps->card_refs[7] = prev_assist_card_ref;

      target_ps->assist_remaining_turns = ce->def.assist_turns;
      target_ps->assist_delay_turns = 0;
      target_ps->assist_card_set_number = s->next_assist_card_set_number++;

      this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      if (!is_nte) {
        target_ps->apply_assist_card_effect_on_set(this->shared_from_this());
      }
      target_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      s->assist_server->populate_effects();

      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto other_ps = s->get_player_state(client_id);
        if (!other_ps) {
          continue;
        }
        uint32_t prev_assist_flags = other_ps->assist_flags;
        other_ps->set_assist_flags_from_assist_effects();
        if (other_ps->assist_flags != prev_assist_flags) {
          other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
        }
      }
      if (is_nte) {
        target_ps->apply_assist_card_effect_on_set(this->shared_from_this());
      }
    }
    // NTE doesn't track this, but NTE also doesn't have access to it.
    this->stats.num_assist_cards_set++;
  }
  // NTE doesn't track this, but NTE also doesn't have access to it.
  this->stats.num_cards_set++;

  this->compute_total_set_cards_cost();
  s->card_special->on_card_set(this->shared_from_this(), card_ref);
  if (!is_nte && (ce->def.type == CardType::ASSIST)) {
    s->check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
  }
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  s->send_6xB4x05();

  if (!is_nte) {
    G_AddToSetCardLog_Ep3_6xB4x4A cmd;
    cmd.card_refs.clear(0xFFFF);
    cmd.card_refs[0] = card_ref;
    cmd.client_id = this->client_id;
    cmd.entry_count = 1;
    cmd.round_num = s->get_round_num();
    s->send(cmd);
  }

  return true;
}

void PlayerState::set_initial_location() {
  auto s = this->server();

  auto mr = s->map_and_rules;

  uint8_t num_team_players;
  if (this->team_id == 0) {
    num_team_players = mr->num_team0_players;
  } else {
    num_team_players = mr->num_players - mr->num_team0_players;
  }

  uint8_t player_index_within_team = 0;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    if (client_id == this->client_id) {
      break;
    }
    auto other_ps = s->player_states[client_id];
    if (other_ps && (this->team_id == other_ps->get_team_id())) {
      player_index_within_team++;
    }
  }

  static const uint8_t start_tile_defs_offset_for_team_size[4] = {0, 0, 1, 3};
  if (num_team_players >= 4) {
    throw logic_error("too many players on team");
  }
  size_t start_tile_def_index = start_tile_defs_offset_for_team_size[num_team_players] + player_index_within_team;
  uint8_t player_start_tile = mr->map.start_tile_definitions[this->team_id][start_tile_def_index];

  Direction facing_direction = static_cast<Direction>((player_start_tile >> 6) & 3);
  this->start_facing_direction = facing_direction;
  mr->start_facing_directions |= (static_cast<uint16_t>(this->start_facing_direction) << (this->client_id << 2));

  bool start_tile_found = false;
  for (size_t y = 0; (y < 0x10) && !start_tile_found; y++) {
    for (size_t x = 0; (x < 0x10) && !start_tile_found; x++) {
      if (mr->map.tiles[y][x] == (player_start_tile & 0x3F)) {
        this->sc_card->loc.x = x;
        this->sc_card->loc.y = y;
        this->sc_card->loc.direction = facing_direction;
        start_tile_found = true;
        break;
      }
    }
  }
  if (!start_tile_found) {
    throw runtime_error("player start location not set");
  }
}

void PlayerState::set_map_occupied_bit_for_card_on_warp_tile(
    shared_ptr<const Card> card) {
  if (!card) {
    return;
  }

  auto s = this->server();

  for (size_t warp_type = 0; warp_type < 5; warp_type++) {
    for (size_t warp_end = 0; warp_end < 2; warp_end++) {
      if ((s->warp_positions[warp_type][warp_end][0] == card->loc.x) &&
          (s->warp_positions[warp_type][warp_end][1] == card->loc.y)) {
        s->map_and_rules->set_occupied_bit_for_tile(
            s->warp_positions[warp_type][warp_end ^ 1][0],
            s->warp_positions[warp_type][warp_end ^ 1][1]);
      }
    }
  }
}

void PlayerState::set_map_occupied_bits_for_sc_and_creatures() {
  auto s = this->server();

  if (this->sc_card && !(this->sc_card->card_flags & 2)) {
    s->map_and_rules->set_occupied_bit_for_tile(
        this->sc_card->loc.x, this->sc_card->loc.y);
    this->set_map_occupied_bit_for_card_on_warp_tile(this->sc_card);
  }

  if (this->sc_card_type == CardType::ARKZ_SC) {
    for (size_t set_index = 0; set_index < 8; set_index++) {
      auto card = this->set_cards[set_index];
      if (card) {
        s->map_and_rules->set_occupied_bit_for_tile(
            card->loc.x, card->loc.y);
        this->set_map_occupied_bit_for_card_on_warp_tile(card);
      }
    }
  }
}

void PlayerState::subtract_def_points(uint8_t cost) {
  this->def_points -= cost;
}

bool PlayerState::subtract_or_check_atk_or_def_points_for_action(
    const ActionState& pa, bool deduct_points) {
  auto s = this->server();

  int16_t cost = this->compute_attack_or_defense_atk_costs(pa);
  auto type = s->ruler_server->get_pending_action_type(pa);

  if ((type == ActionType::ATTACK) && (cost <= this->atk_points)) {
    if (deduct_points) {
      this->subtract_atk_points(cost);
    }
    return true;

  } else if ((type == ActionType::DEFENSE) && (cost <= (short)this->def_points)) {
    if (deduct_points) {
      this->subtract_def_points(cost);
    }
    return true;
  }

  return false;
}

void PlayerState::subtract_atk_points(uint8_t cost) {
  this->atk_points -= cost;
  this->atk_points2 = min<uint8_t>(this->atk_points, this->atk_points2_max);
}

G_UpdateHand_Ep3_6xB4x02 PlayerState::prepare_6xB4x02() const {
  G_UpdateHand_Ep3_6xB4x02 cmd;
  cmd.client_id = this->client_id;
  cmd.state.dice_results = this->dice_results;
  cmd.state.atk_points = this->atk_points;
  cmd.state.def_points = this->def_points;
  cmd.state.atk_points2 = this->atk_points2;
  cmd.state.unknown_a1 = this->unknown_a14;
  cmd.state.total_set_cards_cost = this->total_set_cards_cost;
  cmd.state.is_cpu_player = this->server()->presence_entries[this->client_id].is_cpu_player;
  cmd.state.assist_flags = this->assist_flags;
  for (size_t z = 0; z < 6; z++) {
    cmd.state.hand_card_refs[z] = this->card_refs[z];
    cmd.state.hand_card_refs2[z] = this->card_refs[z];
  }
  for (size_t z = 0; z < 8; z++) {
    cmd.state.set_card_refs[z] = this->card_refs[z + 8];
    cmd.state.set_card_refs2[z] = this->card_refs[z + 8];
  }
  cmd.state.assist_card_ref = this->card_refs[6];
  cmd.state.sc_card_ref = this->sc_card_ref;
  cmd.state.assist_card_ref2 = this->card_refs[6];
  cmd.state.assist_card_set_number = (this->card_refs[6] == 0xFFFF)
      ? 0
      : this->assist_card_set_number;
  cmd.state.assist_card_id = this->set_assist_card_id;
  cmd.state.assist_remaining_turns = this->assist_remaining_turns;
  cmd.state.assist_delay_turns = this->assist_delay_turns;
  cmd.state.atk_bonuses = this->atk_bonuses;
  cmd.state.def_bonuses = this->def_bonuses;
  return cmd;
}

void PlayerState::update_hand_and_equip_state_and_send_6xB4x02_if_needed(
    bool always_send) {
  auto cmd = this->prepare_6xB4x02();
  if (always_send || memcmp(&this->hand_and_equip, &cmd.state, sizeof(this->hand_and_equip))) {
    *this->hand_and_equip = cmd.state;
    this->server()->send(cmd);
  }
  this->send_6xB4x04_if_needed(always_send);
}

void PlayerState::set_random_assist_card_from_hand_for_free() {
  auto s = this->server();
  bool is_nte = s->options.is_nte();

  vector<uint16_t> candidate_card_refs;
  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    uint16_t card_ref = this->card_refs[hand_index];
    auto ce = s->definition_for_card_ref(card_ref);
    if (ce && (ce->def.type == CardType::ASSIST) &&
        (assist_effect_number_for_card_id(ce->def.card_id, is_nte) != AssistEffect::SQUEEZE)) {
      candidate_card_refs.emplace_back(card_ref);
    }
  }

  if (!candidate_card_refs.empty()) {
    this->discard_set_assist_card();
    size_t index = s->get_random(candidate_card_refs.size());
    this->set_card_from_hand(
        candidate_card_refs[index], 15, nullptr, this->client_id, 1);
  }
}

G_UpdateShortStatuses_Ep3_6xB4x04 PlayerState::prepare_6xB4x04() const {
  G_UpdateShortStatuses_Ep3_6xB4x04 cmd;
  cmd.client_id = this->client_id;
  // Note: The original code calls memset to clear all the short status structs
  // at once. We don't do this because the default constructor has already
  // cleared them at construction time; instead, we just clear the fields that
  // won't be overwritten and aren't initialized to zero already.
  for (size_t z = 0; z < 0x10; z++) {
    cmd.card_statuses[z].unused1 = 0;
  }

  if (!this->sc_card) {
    cmd.card_statuses[0].card_ref = 0xFFFF;
  } else {
    cmd.card_statuses[0] = this->sc_card->get_short_status();
  }

  for (size_t hand_index = 0; hand_index < 6; hand_index++) {
    this->get_short_status_for_card_index_in_hand(
        hand_index + 1, &cmd.card_statuses[hand_index + 1]);
    // This write is required to mimic memset()'s effect from the original code.
    // This field is probably ignored for hand refs anyway, but we might as well
    // be as consistent as possible.
    cmd.card_statuses[hand_index + 1].unused1 = 0;
  }

  for (size_t set_index = 0; set_index < 8; set_index++) {
    auto card = this->set_cards[set_index];
    if (!card) {
      cmd.card_statuses[set_index + 7].card_ref = 0xFFFF;
    } else {
      cmd.card_statuses[set_index + 7] = card->get_short_status();
    }
  }

  cmd.card_statuses[15].card_ref = this->card_refs[6];
  return cmd;
}

void PlayerState::send_6xB4x04_if_needed(bool always_send) {
  auto cmd = this->prepare_6xB4x04();
  if (always_send || (cmd.card_statuses != *this->card_short_statuses)) {
    auto s = this->server();
    *this->card_short_statuses = cmd.card_statuses;
    if (s->options.is_nte() || !s->get_should_copy_prev_states_to_current_states()) {
      s->send(cmd);
    }
  }
}

vector<uint16_t> PlayerState::get_card_refs_within_range_from_all_players(
    const parray<uint8_t, 9 * 9>& range,
    const Location& loc,
    CardType type) const {
  auto s = this->server();

  vector<uint16_t> ret;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto other_ps = s->player_states[client_id];
    if (other_ps && ((other_ps->get_sc_card_type() == type) || (type == CardType::ITEM))) {
      auto card_refs = get_card_refs_within_range(range, loc, *other_ps->card_short_statuses);
      ret.insert(ret.end(), card_refs.begin(), card_refs.end());
    }
  }
  return ret;
}

void PlayerState::draw_phase_before() {
  if (this->sc_card) {
    this->sc_card->draw_phase_before();
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index]) {
      this->set_cards[set_index]->draw_phase_before();
    }
  }
}

void PlayerState::action_phase_before() {
  if (this->sc_card) {
    this->sc_card->action_phase_before();
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index]) {
      this->set_cards[set_index]->action_phase_before();
    }
  }
}

void PlayerState::move_phase_before() {
  if (this->sc_card) {
    this->sc_card->move_phase_before();
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index]) {
      this->set_cards[set_index]->move_phase_before();
    }
  }
}

void PlayerState::handle_before_turn_assist_effects() {
  auto s = this->server();

  if ((this->assist_delay_turns > 0) &&
      (--this->assist_delay_turns == 0)) {
    this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
    for (size_t z = 0; z < num_assists; z++) {
      switch (s->assist_server->get_active_assist_by_index(z)) {
        case AssistEffect::BOMB:
          s->execute_bomb_assist_effect();
          break;
        case AssistEffect::ATK_DICE_2:
          // Note: This behavior doesn't match the card description. Is it
          // supposed to add 2 or multiply by 2?
          this->atk_points = min<int16_t>(this->atk_points + 2, 9);
          this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
          break;
        case AssistEffect::SKIP_TURN:
          this->assist_flags |= AssistFlag::IS_SKIPPING_TURN;
          this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
          break;
        default:
          break;
      }
    }
  }
}

int16_t PlayerState::get_assist_turns_remaining() {
  if ((this->card_refs[6] == 0xFFFF) && (this->set_assist_card_id == 0xFFFF)) {
    return -1;
  }
  return this->assist_remaining_turns;
}

bool PlayerState::set_action_cards_for_action_state(const ActionState& pa) {
  auto s = this->server();
  auto log = s->log_stack("set_action_cards_for_action_state: ");
  bool is_nte = s->options.is_nte();

  auto attacker_card = s->card_for_set_card_ref(pa.attacker_card_ref);
  if (attacker_card) {
    log.debug("attacker card present");
    attacker_card->card_flags |= 0x100;
  }

  auto action_type = s->ruler_server->get_pending_action_type(pa);
  if (action_type == ActionType::DEFENSE) {
    log.debug("action type is DEFENSE");
  } else if (action_type == ActionType::ATTACK) {
    log.debug("action type is ATTACK");
  } else {
    log.debug("action type is UNKNOWN");
  }
  if (!is_nte) {
    log.debug("(non-nte) subtracting action points");
    this->subtract_or_check_atk_or_def_points_for_action(pa, true);
  }

  if (action_type == ActionType::ATTACK) {
    auto card = s->card_for_set_card_ref(pa.attacker_card_ref);
    if (card) {
      card->loc.direction = pa.facing_direction;
      log.debug("set facing direction to %s", phosg::name_for_enum(card->loc.direction));

      G_AddToSetCardLog_Ep3_6xB4x4A cmd;
      cmd.card_refs.clear(0xFFFF);
      cmd.client_id = this->client_id;
      cmd.round_num = s->get_round_num();
      cmd.entry_count = 0;
      size_t z = 0;
      do {
        if (log.should_log(phosg::LogLevel::DEBUG)) {
          string ref_str = s->debug_str_for_card_ref(pa.action_card_refs[z]);
          log.debug("on action card ref %s", ref_str.c_str());
        }
        card->unknown_80237A90(pa, pa.action_card_refs[z]);
        card->unknown_802379BC(pa.action_card_refs[z]);
        if (!is_nte) {
          if (pa.action_card_refs[z] != 0xFFFF) {
            cmd.card_refs[z] = pa.action_card_refs[z];
            cmd.entry_count++;
          }
          auto ce = s->definition_for_card_ref(pa.action_card_refs[z]);
          if (ce) {
            auto card_class = ce->def.card_class();
            if (card_class_is_tech_like(card_class, is_nte)) {
              this->stats.num_tech_cards_set++;
            }
            if ((card_class == CardClass::ATTACK_ACTION) ||
                (card_class == CardClass::CONNECT_ONLY_ATTACK_ACTION) ||
                (card_class == CardClass::BOSS_ATTACK_ACTION)) {
              this->stats.num_attack_actions_set++;
            }
            this->stats.num_cards_set++;
          }
        }
        z++;
      } while ((z < 8) && (pa.action_card_refs[z] != 0xFFFF));
      // Note: This is never sent on NTE because entry_count will always be zero
      if (cmd.entry_count > 0) {
        s->send(cmd);
      }
    }

  } else if (action_type == ActionType::DEFENSE) {
    for (size_t z = 0; (z < 4 * 9) && (pa.target_card_refs[z] != 0xFFFF); z++) {
      auto target_card = s->card_for_set_card_ref(pa.target_card_refs[z]);
      if (target_card) {
        if (log.should_log(phosg::LogLevel::DEBUG)) {
          string ref_str = s->debug_str_for_card_ref(pa.target_card_refs[z]);
          log.debug("on target card ref %s", ref_str.c_str());
        }
        target_card->unknown_802379DC(pa);
        if (!is_nte) {
          if (this->client_id == target_card->get_client_id()) {
            this->stats.defense_actions_set_on_self++;
          } else {
            this->stats.defense_actions_set_on_ally++;
          }
          this->stats.num_cards_set++;
        }
      }
    }
    if (!is_nte) {
      G_AddToSetCardLog_Ep3_6xB4x4A cmd;
      cmd.card_refs.clear(0xFFFF);
      cmd.client_id = this->client_id;
      cmd.round_num = s->get_round_num();
      cmd.card_refs[0] = pa.defense_card_ref;
      cmd.entry_count = 1;
      s->send(cmd);
    }
  }
  if (is_nte) {
    log.debug("(nte) subtracting action points");
    this->subtract_or_check_atk_or_def_points_for_action(pa, 1);
  }
  for (size_t z = 0; (z < pa.action_card_refs.size()) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    if (log.should_log(phosg::LogLevel::DEBUG)) {
      string ref_str = s->debug_str_for_card_ref(pa.action_card_refs[z]);
      log.debug("discarding %s from hand", ref_str.c_str());
    }
    this->discard_ref_from_hand(pa.action_card_refs[z]);
  }
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  return true;
}

void PlayerState::dice_phase_before() {
  if (this->sc_card) {
    this->sc_card->dice_phase_before();
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index]) {
      this->set_cards[set_index]->dice_phase_before();
    }
  }

  this->compute_total_set_cards_cost();
  this->unknown_a14 = 0;
  if ((this->assist_remaining_turns > 0) &&
      (this->assist_remaining_turns < 90) &&
      (this->assist_delay_turns == 0)) {
    this->assist_remaining_turns--;
    if (this->assist_remaining_turns < 1) {
      this->discard_set_assist_card();
    }
  }
  if (this->is_team_turn()) {
    this->atk_points = 0;
    this->def_points = 0;
    this->atk_bonuses = 0;
    this->def_bonuses = 0;
    this->roll_dice(2);
  }
  this->assist_flags &= (AssistFlag::HAS_WON_BATTLE |
      AssistFlag::WINNER_DECIDED_BY_DEFEAT |
      AssistFlag::WINNER_DECIDED_BY_RANDOM |
      (this->server()->options.is_nte() ? AssistFlag::NONE : AssistFlag::ELIGIBLE_FOR_DICE_BOOST));
  this->set_assist_flags_from_assist_effects();
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed(0);
  this->send_set_card_updates();
}

void PlayerState::handle_homesick_assist_effect_from_bomb(shared_ptr<Card> card) {
  if (!card) {
    return;
  }

  auto s = this->server();

  size_t set_index;
  for (set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index] == card) {
      break;
    }
  }

  if (set_index < 8) {
    uint16_t card_ref = card->get_card_ref();
    size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
    for (size_t z = 0; z < num_assists; z++) {
      if (s->assist_server->get_active_assist_by_index(z) == AssistEffect::HOMESICK) {
        this->return_set_card_to_hand2(card_ref);
        this->log_discard(card_ref, 1);
        // On NTE, the card is destroyed immediately
        if (s->options.is_nte()) {
          this->set_cards[set_index]->update_stats_on_destruction();
          this->set_cards[set_index].reset();
        } else {
          this->set_cards[set_index]->card_flags |= 2;
        }
        return;
      }
    }

    if (this->deck_state->set_card_ref_drawable_next(card_ref)) {
      this->log_discard(card_ref, 1);
      // On NTE, the card is destroyed immediately
      if (s->options.is_nte()) {
        this->set_cards[set_index]->update_stats_on_destruction();
        this->set_cards[set_index].reset();
      } else {
        this->set_cards[set_index]->card_flags |= 2;
      }
    }
  }
}

void PlayerState::apply_main_die_assist_effects(uint8_t* die_value) const {
  auto s = this->server();

  size_t num_assists = s->assist_server->compute_num_assist_effects_for_client(this->client_id);
  for (size_t z = 0; z < num_assists; z++) {
    switch (s->assist_server->get_active_assist_by_index(z)) {
      case AssistEffect::DICE_FEVER:
        *die_value = s->options.is_nte() ? 6 : 5;
        break;
      case AssistEffect::DICE_HALF:
        *die_value = ((*die_value + 1) >> 1);
        break;
      case AssistEffect::DICE_PLUS_1:
        (*die_value)++;
        break;
      case AssistEffect::DICE_FEVER_PLUS:
        if (!s->options.is_nte()) {
          *die_value = 6;
        }
        break;
      default:
        break;
    }
  }
}

void PlayerState::roll_main_dice_or_apply_after_effects() {
  auto s = this->server();
  const auto& rules = s->map_and_rules->rules;

  // In NTE, the dice behave differently - there is no minimum, and instead the
  // player can specify a fixed value for each die or a random value (1-6). The
  // implementation of this function is therefore quite different on NTE, but
  // since we already support custom ranges for ATK and DEF dice, we just use
  // the non-NTE logic and assign the dice ranges at battle start time to yield
  // the NTE behavior. (See RulesTrial in DataIndexes.cc for how this is done.)

  bool is_1p_2v1 = (s->team_client_count.at(this->get_team_id()) < s->team_client_count[this->get_team_id() ^ 1]);

  auto atk_range = rules.atk_dice_range(is_1p_2v1);
  auto def_range = rules.def_dice_range(is_1p_2v1);

  uint8_t atk_dice_range_width = (atk_range.second - atk_range.first) + 1;
  if (atk_dice_range_width < 2) {
    this->dice_results[0] = atk_range.first;
  } else {
    this->dice_results[0] = atk_range.first + s->get_random(atk_dice_range_width);
  }

  uint8_t def_dice_range_width = (def_range.second - def_range.first) + 1;
  if (def_dice_range_width < 2) {
    this->dice_results[1] = def_range.first;
  } else {
    this->dice_results[1] = def_range.first + s->get_random(def_dice_range_width);
  }

  bool should_exchange = false;
  if (rules.dice_exchange_mode == DiceExchangeMode::HIGH_DEF) {
    should_exchange = (this->dice_results[0] > this->dice_results[1]);
  } else if (rules.dice_exchange_mode == DiceExchangeMode::HIGH_ATK) {
    should_exchange = (this->dice_results[0] < this->dice_results[1]);
  }

  if (!should_exchange) {
    this->atk_points = this->dice_results[0];
    this->def_points = this->dice_results[1];
    this->assist_flags &= (~AssistFlag::DICE_WERE_EXCHANGED);
  } else {
    this->atk_points = this->dice_results[1];
    this->def_points = this->dice_results[0];
    this->assist_flags |= AssistFlag::DICE_WERE_EXCHANGED;
  }

  this->atk_points = this->atk_points + s->card_special->client_has_atk_dice_boost_condition(this->client_id);

  uint8_t atk_before_bonuses = this->atk_points;
  uint8_t def_before_bonuses = this->def_points;

  this->apply_main_die_assist_effects(&this->atk_points);
  this->apply_main_die_assist_effects(&this->def_points);
  this->dice_results[0] = this->atk_points;
  this->dice_results[1] = this->def_points;

  if (s->options.is_nte()) {
    this->atk_bonuses = this->atk_points - atk_before_bonuses;
    this->def_bonuses = this->def_points - def_before_bonuses;
  }
  this->atk_points += s->team_dice_bonus[this->team_id];
  this->def_points += s->team_dice_bonus[this->team_id];
  this->atk_points = clamp<uint8_t>(this->atk_points, 1, 9);
  this->def_points = clamp<uint8_t>(this->def_points, 1, 9);
  if (!s->options.is_nte()) {
    this->atk_bonuses = this->atk_points - atk_before_bonuses;
    this->def_bonuses = this->def_points - def_before_bonuses;
  }
  this->atk_points2 = min<uint8_t>(this->atk_points2_max, this->atk_points);
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
}

void PlayerState::unknown_8023C110() {
  if (this->sc_card) {
    this->sc_card->unknown_802380C0();
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    auto card = this->set_cards[set_index];
    if (card) {
      card->unknown_802380C0();
    }
  }
}

void PlayerState::compute_team_dice_bonus_after_draw_phase() {
  auto s = this->server();

  if (this->sc_card) {
    this->sc_card->unknown_80237F88();
  }

  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (this->set_cards[set_index]) {
      this->set_cards[set_index]->unknown_80237F88();
    }
  }

  uint8_t current_team_turn = s->get_current_team_turn();
  uint8_t dice_boost = s->get_team_exp(current_team_turn) /
      (s->team_client_count[current_team_turn] * 12);
  s->card_special->adjust_dice_boost_if_team_has_condition_52(
      current_team_turn, &dice_boost, 0);
  s->team_dice_bonus[current_team_turn] = clamp<int16_t>(dice_boost, 0, 8);
  this->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
}

void PlayerState::send_6xB4x0A_for_set_card(size_t set_index) {
  if (set_index >= 9) {
    return;
  }

  auto s = this->server();

  // The original code (in NTE) calls memcmp here, but then ignores the results
  // and always copies the chain and metadata.
  // this->set_card_action_chains->at(set_index) == this->unknown_a12;
  // this->set_card_action_metadatas->at(set_index) == this->unknown_a13;
  this->set_card_action_chains->at(set_index) = this->unknown_a12;
  this->set_card_action_metadatas->at(set_index) = this->unknown_a13;

  if (s->options.is_nte()) {
    G_UpdateActionChainAndMetadata_Ep3NTE_6xB4x0A cmd;
    cmd.client_id = this->client_id;
    cmd.index = set_index;
    cmd.chain = this->unknown_a12;
    cmd.metadata = this->unknown_a13;
    s->send(cmd);

  } else {
    G_UpdateActionChainAndMetadata_Ep3_6xB4x0A cmd;
    cmd.client_id = this->client_id;
    cmd.index = set_index;
    cmd.chain = this->unknown_a12;
    cmd.metadata = this->unknown_a13;
    s->send(cmd);
  }
}

} // namespace Episode3
