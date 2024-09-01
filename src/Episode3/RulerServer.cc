#include "RulerServer.hh"

#include <optional>

#include "DataIndexes.hh"
#include "Server.hh"

using namespace std;

namespace Episode3 {

void compute_effective_range(
    parray<uint8_t, 9 * 9>& ret,
    shared_ptr<const CardIndex> card_index,
    uint16_t card_id,
    const Location& loc,
    shared_ptr<const MapAndRulesState> map_and_rules,
    phosg::PrefixedLogger* log) {
  if (log && log->should_log(phosg::LogLevel::DEBUG)) {
    string loc_str = loc.str();
    log->debug("compute_effective_range: card_id=#%04hX, loc=%s", card_id, loc_str.c_str());
    log->debug("compute_effective_range: map_and_rules->map:");
    map_and_rules->map.print(stderr);
  }
  ret.clear(0);

  parray<uint32_t, 6> range_def;
  if (card_id == 0xFFFE) {
    // Heavy Fog: one tile directly in front
    range_def[3] = 0x00000100;
  } else {
    shared_ptr<const CardIndex::CardEntry> ce;
    try {
      ce = card_index->definition_for_id(card_id);
    } catch (const out_of_range&) {
      return;
    }
    for (size_t z = 0; z < 6; z++) {
      range_def[z] = ce->def.range[z];
    }
  }
  if (log) {
    log->debug("compute_effective_range: range_def: %05" PRIX32 " %05" PRIX32 " %05" PRIX32 " %05" PRIX32 " %05" PRIX32 " %05" PRIX32, range_def[0], range_def[1], range_def[2], range_def[3], range_def[4], range_def[5]);
  }

  if (range_def[0] == 0x000FFFFF) {
    // Entire field
    ret.clear(2);
    if (log) {
      log->debug("compute_effective_range: entire field (2)");
    }
    return;
  }

  parray<uint8_t, 9 * 9> decoded_range;
  for (size_t y = 0; y < 6; y++) {
    uint32_t row = range_def[y];
    for (size_t x = 0; x < 5; x++) {
      if (row & 0x0000000F) {
        decoded_range[x + (y * 9) + 2] = 1;
      }
      row >>= 4;
    }
  }
  if (log) {
    for (size_t y = 0; y < 9; y++) {
      log->debug("compute_effective_range: decoded_range: %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
          decoded_range[y * 9 + 0], decoded_range[y * 9 + 1], decoded_range[y * 9 + 2], decoded_range[y * 9 + 3], decoded_range[y * 9 + 4], decoded_range[y * 9 + 5], decoded_range[y * 9 + 6], decoded_range[y * 9 + 7], decoded_range[y * 9 + 8]);
    }
  }

  for (int16_t y = 0; y < 9; y++) {
    int16_t map_y = y + loc.y - 4;
    if (!map_and_rules || ((map_y >= 0) && (map_y < map_and_rules->map.height))) {
      for (int16_t x = 0; x < 9; x++) {
        int16_t map_x = x + loc.x - 4;
        if (!map_and_rules || ((map_x >= 0) && (map_x < map_and_rules->map.width))) {
          int16_t up_x, up_y;
          switch (loc.direction) {
            case Direction::LEFT:
              up_x = y;
              up_y = 9 - x - 1;
              break;
            case Direction::RIGHT:
              up_x = 9 - y - 1;
              up_y = x;
              break;
            case Direction::UP:
              up_x = x;
              up_y = y;
              break;
            case Direction::DOWN:
              up_x = 9 - x - 1;
              up_y = 9 - y - 1;
              break;
            default:
              throw logic_error("invalid direction");
          }
          ret[y * 9 + x] = decoded_range[up_y * 9 + up_x];
          if (log) {
            log->debug("compute_effective_range: x=%hd y=%hd up_x=%hd up_y=%hd v=%hhX", x, y, up_x, up_y, ret[y * 9 + x]);
          }
        }
      }
    }
  }

  if (log) {
    for (size_t y = 0; y < 9; y++) {
      log->debug("compute_effective_range: ret: %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
          ret[y * 9 + 0], ret[y * 9 + 1], ret[y * 9 + 2], ret[y * 9 + 3], ret[y * 9 + 4], ret[y * 9 + 5], ret[y * 9 + 6], ret[y * 9 + 7], ret[y * 9 + 8]);
    }
  }
}

bool card_linkage_is_valid(
    shared_ptr<const CardIndex::CardEntry> right_ce,
    shared_ptr<const CardIndex::CardEntry> left_ce,
    shared_ptr<const CardIndex::CardEntry> sc_ce,
    bool has_permission_effect) {
  if (!right_ce) {
    return false;
  }

  bool sc_is_named_android_without_permission_effect = false;
  bool sc_is_named_android = sc_ce->def.is_named_android_sc();
  if (sc_is_named_android &&
      !has_permission_effect &&
      (left_ce->def.type == CardType::ITEM)) {
    sc_is_named_android_without_permission_effect = true;
  }

  if (!left_ce) {
    return false;
  }

  for (size_t x = 0; x < 8; x++) {
    uint8_t right_color = left_ce->def.right_colors[x];
    if ((right_color != 0) &&
        (!sc_is_named_android_without_permission_effect || (right_color != 3))) {
      for (size_t y = 0; y < 8; y++) {
        if (right_color == right_ce->def.left_colors[y]) {
          return true;
        }
      }
    }
  }

  // If we get here, then the linkage does not make sense based only on the
  // cards' left/right colors. It may still be allowed if Permission is in
  // effect, though.

  // Ignore Permission effect if the left card is another action card (the Tech
  // color linkage must make sense in that case). (The way they do this is kind
  // of dumb - they should have checked that type == ACTION, but instead they
  // checked that type *isn't* most of the other types... but curiously, ASSIST
  // is not checked. This is probably just an oversight.)
  if (has_permission_effect &&
      (left_ce->def.type != CardType::HUNTERS_SC) &&
      (left_ce->def.type != CardType::ARKZ_SC) &&
      (left_ce->def.type != CardType::ITEM) &&
      (left_ce->def.type != CardType::CREATURE)) {
    has_permission_effect = false;
  }

  if (has_permission_effect) {
    // Permission allows a right card with left color 03 to link with anything
    for (size_t z = 0; z < 8; z++) {
      if (right_ce->def.left_colors[z] == 3) {
        return true;
      }
    }
  }

  return false;
}

RulerServer::RulerServer(shared_ptr<Server> server)
    : w_server(server),
      team_id_for_client_id(0xFF),
      error_code1(0),
      error_code2(0),
      error_code3(0) {}

shared_ptr<Server> RulerServer::server() {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const Server> RulerServer::server() const {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

ActionChainWithConds* RulerServer::action_chain_with_conds_for_card_ref(
    uint16_t card_ref) {
  return const_cast<ActionChainWithConds*>(as_const(*this).action_chain_with_conds_for_card_ref(card_ref));
}

const ActionChainWithConds* RulerServer::action_chain_with_conds_for_card_ref(
    uint16_t card_ref) const {
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (client_id != 0xFF) {
    // There appears to be a bug in Trial Edition: the bound on this loop is
    // 0x10, not 9.
    for (size_t z = 0; z < 9; z++) {
      const auto* chain = &this->set_card_action_chains[client_id]->at(z);
      if (card_ref == chain->chain.acting_card_ref) {
        return chain;
      }
    }
  }
  return nullptr;
}

bool RulerServer::any_attack_action_card_is_support_tech_or_support_pb(
    const ActionState& pa) const {
  if (pa.attacker_card_ref != 0xFFFF) {
    for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
      uint16_t card_id = this->card_id_for_card_ref(pa.action_card_refs[z]);
      if (this->card_id_is_support_tech_or_support_pb(card_id)) {
        return true;
      }
    }
  }
  return false;
}

bool RulerServer::card_has_pierce_or_rampage(
    uint8_t client_id,
    ConditionType cond_type,
    bool* out_has_rampage,
    uint16_t attacker_card_ref,
    uint16_t action_card_ref,
    uint8_t def_effect_index,
    AttackMedium attack_medium) const {
  auto short_statuses = (client_id < 4) ? this->short_statuses[client_id] : nullptr;
  *out_has_rampage = false;

  bool ret;
  bool is_nte = this->server()->options.is_nte();
  if (is_nte) {
    ret = true;
  } else {
    if (cond_type == ConditionType::NONE) {
      return false;
    }
    ret = this->check_usability_or_apply_condition_for_card_refs(
        action_card_ref,
        attacker_card_ref,
        // Original code omitted this null check and presumably could crash here
        short_statuses ? short_statuses->at(0).card_ref.load() : 0xFFFF,
        def_effect_index,
        attack_medium);
  }

  switch (cond_type) {
    case ConditionType::RAMPAGE:
    case ConditionType::UNKNOWN_20:
    case ConditionType::UNKNOWN_21:
    case ConditionType::MAJOR_RAMPAGE:
    case ConditionType::HEAVY_RAMPAGE:
      *out_has_rampage = true;
      return false;
    case ConditionType::PIERCE:
      return ret;
    case ConditionType::HEAVY_PIERCE:
      if (short_statuses) {
        const auto& sc_status = short_statuses->at(0);
        auto ce = this->definition_for_card_ref(sc_status.card_ref);
        if (ce && (ce->def.type == CardType::HUNTERS_SC)) {
          size_t count = 0;
          for (size_t z = 7; z < 15; z++) {
            if (this->card_exists_by_status(short_statuses->at(z))) {
              count++;
            }
          }
          if (count > 2) {
            return ret;
          }
        }
      }
      return false;
    case ConditionType::MAJOR_PIERCE:
      if (short_statuses) {
        const auto& sc_status = short_statuses->at(0);
        auto ce = this->definition_for_card_ref(sc_status.card_ref);
        // This appears to be an NTE bug: Major Pierce doesn't work on Arkz SCs.
        if (ce &&
            (!is_nte || (ce->def.type == CardType::HUNTERS_SC)) &&
            (this->get_card_ref_max_hp(sc_status.card_ref) <= sc_status.current_hp * 2)) {
          return ret;
        }
      }
      return false;
    default:
      return false;
  }
}

bool RulerServer::attack_action_has_rampage_and_not_pierce(const ActionState& pa, uint16_t card_ref) const {
  uint16_t orig_card_ref;
  uint16_t effective_range_card_id;
  TargetMode effective_target_mode;
  bool has_pierce = false;
  auto attack_medium = this->get_attack_medium(pa);

  if (!this->compute_effective_range_and_target_mode_for_attack(
          pa, &effective_range_card_id, &effective_target_mode, &orig_card_ref)) {
    return false;
  }

  if ((orig_card_ref != 0xFFFF) && (orig_card_ref != pa.attacker_card_ref) &&
      !this->check_usability_or_apply_condition_for_card_refs(
          orig_card_ref, pa.attacker_card_ref, card_ref, 0xFF, AttackMedium::INVALID_FF)) {
    return false;
  }

  ssize_t x = -1;
  for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    x = z;
  }
  for (; x >= 0; x--) {
    auto ce = this->definition_for_card_ref(pa.action_card_refs[x]);
    if (ce) {
      ssize_t cond_index;
      for (cond_index = 0; cond_index < 3; cond_index++) {
        if (ce->def.effects[cond_index].type == ConditionType::NONE) {
          break;
        }
      }
      for (cond_index--; cond_index >= 0; cond_index--) {
        bool has_rampage = this->check_pierce_and_rampage(
            card_ref,
            ce->def.effects[cond_index].type,
            &has_pierce,
            pa.attacker_card_ref,
            pa.action_card_refs[x],
            cond_index,
            attack_medium);
        if (has_rampage) {
          return true;
        }
        if (has_pierce) {
          return false;
        }
      }
    }
  }

  const auto* chain = this->action_chain_with_conds_for_card_ref(
      pa.attacker_card_ref);
  if (chain) {
    for (ssize_t z = 8; z >= 0; z--) {
      bool has_rampage = this->check_pierce_and_rampage(
          card_ref,
          chain->conditions[z].type,
          &has_pierce,
          pa.attacker_card_ref,
          chain->conditions[z].card_ref,
          chain->conditions[z].card_definition_effect_index,
          attack_medium);
      if (has_rampage) {
        return true;
      }
      if (has_pierce) {
        return false;
      }
    }
  }

  return false;
}

bool RulerServer::attack_action_has_pierce_and_not_rampage(const ActionState& pa, uint8_t client_id) const {
  if ((client_id_for_card_ref(pa.attacker_card_ref) == 0xFF) || (client_id >= 4)) {
    return false;
  }

  bool is_nte = this->server()->options.is_nte();
  auto attack_medium = this->get_attack_medium(pa);
  auto stat = this->short_statuses[client_id];
  if (!stat || (!is_nte && !this->card_exists_by_status(stat->at(0))) || (stat->at(0).card_ref == 0xFFFF)) {
    return false;
  }

  uint16_t card_ref1;
  if (!this->compute_effective_range_and_target_mode_for_attack(pa, nullptr, nullptr, &card_ref1)) {
    return false;
  }
  if ((card_ref1 != 0xFFFF) &&
      (card_ref1 != pa.attacker_card_ref) &&
      !this->check_usability_or_apply_condition_for_card_refs(card_ref1, pa.attacker_card_ref, stat->at(0).card_ref, 0xFF, AttackMedium::INVALID_FF)) {
    return false;
  }

  ssize_t last_action_card_index = -1;
  for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    last_action_card_index = z;
  }

  auto check_chain = [&]() -> optional<bool> {
    const auto* chain = this->action_chain_with_conds_for_card_ref(pa.attacker_card_ref);
    if (chain) {
      for (ssize_t cond_index = 8; cond_index >= 0; cond_index--) {
        bool has_rampage = false;
        if (this->card_has_pierce_or_rampage(
                client_id, chain->conditions[cond_index].type, &has_rampage,
                pa.attacker_card_ref, chain->conditions[cond_index].card_ref,
                chain->conditions[cond_index].card_definition_effect_index,
                attack_medium)) {
          return true;
        }
        if (has_rampage) {
          return false;
        }
      }
    }
    return nullopt;
  };

  if (is_nte) {
    auto res = check_chain();
    if (res.has_value()) {
      return res.value();
    }
  }

  for (; last_action_card_index >= 0; last_action_card_index--) {
    auto ce = this->definition_for_card_ref(
        pa.action_card_refs[last_action_card_index]);
    if (!ce) {
      continue;
    }

    ssize_t last_cond_index = -1;
    for (size_t z = 0; (z < 3) && (ce->def.effects[z].type != ConditionType::NONE); z++) {
      last_cond_index = z;
    }

    for (; last_cond_index >= 0; last_cond_index--) {
      bool has_rampage = false;
      if (this->card_has_pierce_or_rampage(
              client_id, ce->def.effects[last_cond_index].type, &has_rampage,
              pa.attacker_card_ref, pa.action_card_refs[last_action_card_index],
              last_cond_index, attack_medium)) {
        return true;
      }
      if (has_rampage) {
        return false;
      }
    }
  }

  if (!is_nte) {
    auto res = check_chain();
    if (res.has_value()) {
      return res.value();
    }
  }

  return false;
}

bool RulerServer::card_exists_by_status(const CardShortStatus& stat) const {
  if ((stat.card_flags & 3) || (stat.card_ref == 0xFFFF)) {
    return false;
  }
  uint8_t client_id = client_id_for_card_ref(stat.card_ref);
  if ((client_id < 4) && (this->team_id_for_client_id[client_id] != 0xFF)) {
    return true;
  }
  return false;
}

bool RulerServer::card_has_mighty_knuckle(uint32_t card_ref) const {
  auto ce = this->definition_for_card_ref(card_ref);
  if (ce) {
    for (size_t z = 0; z < 3; z++) {
      if (ce->def.effects[z].type == ConditionType::NONE) {
        return false;
      }
      if (ce->def.effects[z].type == ConditionType::MIGHTY_KNUCKLE) {
        return true;
      }
    }
  }
  return false;
}

uint16_t RulerServer::card_id_for_card_ref(uint16_t card_ref) const {
  return this->server()->card_id_for_card_ref(card_ref);
}

bool RulerServer::card_id_is_boss_sc(uint16_t card_id) {
  return (card_id >= 0x029B) && (card_id < 0x029F);
}

bool RulerServer::card_id_is_support_tech_or_support_pb(uint16_t card_id) {
  return (card_id == 0x00E1) ||
      (card_id == 0x00E2) ||
      (card_id == 0x00E6) ||
      (card_id == 0x00EB) ||
      (card_id == 0x00EC);
}

bool RulerServer::card_ref_can_attack(uint16_t card_ref) {
  if (card_ref == 0xFFFF) {
    return false;
  }

  if (!this->should_allow_attacks_on_current_turn()) {
    return false;
  }

  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce) {
    return false;
  }

  if (ce->def.type == CardType::ACTION) {
    return true;
  } else if (ce->def.type == CardType::ASSIST) {
    return false;
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  const auto* stat = this->short_status_for_card_ref(card_ref);
  if ((client_id == 0xFF) || !stat || !this->card_exists_by_status(*stat)) {
    return false;
  }

  if (this->find_condition_on_card_ref(card_ref, ConditionType::HOLD) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::GUOM) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::PARALYZE) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::FREEZE)) {
    return false;
  }

  // If the card is an item and its SC has any attack-preventing condition,
  // then the item also cannot attack
  if ((ce->def.type == CardType::ITEM) &&
      (!this->short_statuses[client_id] ||
          (this->short_statuses[client_id]->at(0).card_ref == 0xFFFF) ||
          this->find_condition_on_card_ref(this->short_statuses[client_id]->at(0).card_ref, ConditionType::HOLD) ||
          this->find_condition_on_card_ref(this->short_statuses[client_id]->at(0).card_ref, ConditionType::GUOM) ||
          this->find_condition_on_card_ref(this->short_statuses[client_id]->at(0).card_ref, ConditionType::PARALYZE) ||
          this->find_condition_on_card_ref(this->short_statuses[client_id]->at(0).card_ref, ConditionType::FREEZE))) {
    return false;
  }

  if ((ce->def.card_class() == CardClass::GUARD_ITEM) &&
      this->find_condition_on_card_ref(card_ref, ConditionType::SHIELD_WEAPON)) {
    return true;
  }

  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(
      client_id);
  for (size_t z = 0; z < num_assists; z++) {
    if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::PERMISSION) {
      return true;
    }
  }

  return !ce->def.cannot_attack;
}

bool RulerServer::card_ref_can_move(
    uint8_t client_id, uint16_t card_ref, bool ignore_atk_points) const {
  if (client_id == 0xFF) {
    return false;
  }

  if (client_id_for_card_ref(card_ref) != client_id) {
    return false;
  }

  if (!this->action_chain_with_conds_for_card_ref(card_ref)) {
    return false;
  }

  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce) {
    return false;
  }

  const CardShortStatus* stat = nullptr;
  auto short_statuses = this->short_statuses[client_id];
  if (short_statuses->at(0).card_ref == card_ref) { // SC moving
    stat = &short_statuses->at(0);
    if (ce->def.type == CardType::HUNTERS_SC) {
      for (size_t z = 7; z < 15; z++) {
        const auto& item_stat = short_statuses->at(z);
        if ((item_stat.card_ref != 0xFFFF) && this->card_exists_by_status(item_stat) &&
            (this->find_condition_on_card_ref(item_stat.card_ref, ConditionType::GUOM) ||
                this->find_condition_on_card_ref(item_stat.card_ref, ConditionType::IMMOBILE))) {
          return false;
        }
      }
    }
  } else if (ce->def.type == CardType::CREATURE) { // Creature moving
    for (size_t z = 7; z < 15; z++) {
      const auto* creature_stat = &short_statuses->at(z);
      if (creature_stat->card_ref == card_ref) {
        stat = creature_stat;
      }
    }
  }

  if (!stat || !this->card_exists_by_status(*stat) || (stat->card_flags & 0x80)) {
    return false;
  }

  if ((this->hand_and_equip_states[client_id]->assist_flags & AssistFlag::IS_SKIPPING_TURN)) {
    return false;
  }

  if (this->find_condition_on_card_ref(card_ref, ConditionType::HOLD) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::GUOM) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::FREEZE) ||
      this->find_condition_on_card_ref(card_ref, ConditionType::IMMOBILE)) {
    return false;
  }

  uint8_t current_atk = this->hand_and_equip_states[client_id]->atk_points;
  uint8_t max_move_dist = this->max_move_distance_for_card_ref(card_ref);
  if (max_move_dist == 0) {
    return false;
  }

  if (!ignore_atk_points) {
    if (max_move_dist < current_atk) {
      current_atk = max_move_dist;
    }
    return (current_atk != 0);
  } else {
    return true;
  }
}

bool RulerServer::card_ref_has_class_usability_condition(
    uint16_t card_ref) const {
  auto ce = this->definition_for_card_ref(card_ref);
  if (ce) {
    uint8_t criterion = static_cast<uint8_t>(ce->def.usable_criterion);
    if ((criterion >= 0x01) && (criterion < 0x04)) {
      return true;
    }
    if ((criterion >= 0x09) && (criterion < 0x1D)) {
      return true;
    }
  }
  return false;
}

bool RulerServer::card_ref_has_free_maneuver(uint16_t card_ref) const {
  return this->find_condition_on_card_ref(card_ref, ConditionType::FREE_MANEUVER);
}

bool RulerServer::card_ref_is_aerial(uint16_t card_ref) const {
  if (!this->server()->options.is_nte()) {
    const auto* stat = this->short_status_for_card_ref(card_ref);
    if (!stat || !this->card_exists_by_status(*stat)) {
      return false;
    }
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::FLY) {
      return true;
    }
  }

  // Note: The original code checks equipped items here for the Aerial condition
  // if card_ref is a Hunters SC, then ignores the result. We omit this check
  // for obvious reasons.
  return this->find_condition_on_card_ref(card_ref, ConditionType::AERIAL);
}

bool RulerServer::card_ref_is_aerial_or_has_free_maneuver(
    uint16_t card_ref) const {
  return (this->card_ref_has_free_maneuver(card_ref) || this->card_ref_is_aerial(card_ref));
}

bool RulerServer::card_ref_is_boss_sc(uint32_t card_ref) const {
  return this->card_id_is_boss_sc(this->card_id_for_card_ref(card_ref));
}

bool RulerServer::card_ref_or_any_set_card_has_condition_46(
    uint16_t card_ref) const {
  uint16_t card_id = this->card_id_for_card_ref(card_ref);
  if (card_id == 0xFFFF) {
    return false;
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (this->hand_and_equip_states[client_id]->assist_flags & AssistFlag::IMMORTAL) {
    auto ce = this->definition_for_card_id(card_id);
    if (!ce) {
      return false;
    }
    if ((ce->def.type != CardType::HUNTERS_SC) && (ce->def.type != CardType::ARKZ_SC)) {
      return true;
    }
  }

  for (size_t z = 0; z < 4; z++) {
    auto stat = this->short_statuses[z];
    if (stat) {
      const auto& sc_stat = stat->at(0);
      Condition cond;
      if (this->card_exists_by_status(sc_stat) &&
          this->find_condition_on_card_ref(sc_stat.card_ref, ConditionType::UNKNOWN_46, &cond) &&
          (cond.value == card_id)) {
        return true;
      }

      for (size_t w = 7; w < 15; w++) {
        const auto& item_stat = stat->at(w);
        if (this->card_exists_by_status(item_stat) &&
            this->find_condition_on_card_ref(item_stat.card_ref, ConditionType::UNKNOWN_46, &cond) &&
            (cond.value == card_id)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool RulerServer::card_ref_or_sc_has_fixed_range(uint16_t card_ref) const {
  if (this->find_condition_on_card_ref(card_ref, ConditionType::FIXED_RANGE)) {
    return true;
  }

  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce || (ce->def.type != CardType::ITEM)) {
    return false;
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  if ((client_id == 0xFF) || !this->short_statuses[client_id]) {
    return false;
  }

  return this->find_condition_on_card_ref(
      this->short_statuses[client_id]->at(0).card_ref, ConditionType::FIXED_RANGE);
}

bool RulerServer::check_move_path_and_get_cost(
    uint8_t client_id,
    uint16_t card_ref,
    parray<uint8_t, 0x100>* visited_map,
    MovePath* out_path,
    uint32_t* out_cost) const {
  if (client_id == 0xFF) {
    return false;
  }

  const auto* chain = this->action_chain_with_conds_for_card_ref(card_ref);
  if (!chain) {
    return false;
  }

  uint8_t atk = this->hand_and_equip_states[client_id]->atk_points;
  // Note: In the original code, it seems atk was signed, which doesn't make
  // much sense. We've fixed that here.
  // if (atk < 0) { // Uhhh what? This is supposed to be impossible
  //   return false;
  // }

  uint8_t max_dist = this->max_move_distance_for_card_ref(card_ref);
  if (max_dist < 1) {
    return false;
  }
  max_dist = min<uint8_t>(max_dist, 9);

  const auto* short_status = this->short_status_for_card_ref(card_ref);
  if (!short_status) {
    return false;
  }

  bool is_free_maneuver_or_aerial = this->card_ref_is_aerial_or_has_free_maneuver(card_ref);
  bool is_aerial = this->card_ref_is_aerial(card_ref);
  uint8_t x = short_status->loc.x;
  uint8_t y = short_status->loc.y;
  visited_map->clear(0);
  this->flood_fill_move_path(
      *chain, x + 1, y, Direction::RIGHT, atk, max_dist, is_free_maneuver_or_aerial, is_aerial, visited_map, out_path, 0, 0);
  this->flood_fill_move_path(
      *chain, x, y - 1, Direction::UP, atk, max_dist, is_free_maneuver_or_aerial, is_aerial, visited_map, out_path, 0, 0);
  this->flood_fill_move_path(
      *chain, x - 1, y, Direction::LEFT, atk, max_dist, is_free_maneuver_or_aerial, is_aerial, visited_map, out_path, 0, 0);
  this->flood_fill_move_path(
      *chain, x, y + 1, Direction::DOWN, atk, max_dist, is_free_maneuver_or_aerial, is_aerial, visited_map, out_path, 0, 0);
  if (out_path) {
    if (!out_path->is_valid() || (out_path->get_length_plus1() < 2)) {
      if (out_cost) {
        *out_cost = 99;
      }
    } else if (out_cost) {
      *out_cost = out_path->get_cost();
    }
  }

  return true;
}

bool RulerServer::check_pierce_and_rampage(
    uint16_t card_ref,
    ConditionType cond_type,
    bool* out_has_pierce,
    uint16_t attacker_card_ref,
    uint16_t action_card_ref,
    uint8_t def_effect_index,
    AttackMedium attack_medium) const {
  bool is_nte = this->server()->options.is_nte();

  // Note: NTE doesn't set this to zero; it apparently expects the caller to.
  *out_has_pierce = false;

  const auto* card_short_status = this->short_status_for_card_ref(card_ref);
  if (!is_nte && (cond_type == ConditionType::NONE)) {
    return false;
  }

  if ((card_ref != 0xFFFF) &&
      (!card_short_status || !this->card_exists_by_status(*card_short_status))) {
    return false;
  }

  auto ce = this->definition_for_card_ref(card_short_status->card_ref);
  if (!ce) {
    return false;
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  auto client_short_statuses = (client_id != 0xFF) ? this->short_statuses[client_id] : nullptr;

  if (card_ref == 0xFFFF) {
    card_short_status = nullptr;
    client_short_statuses = nullptr;
  }

  bool apply_check_result = (is_nte ||
      this->check_usability_or_apply_condition_for_card_refs(
          action_card_ref, attacker_card_ref, card_ref, def_effect_index, attack_medium));

  switch (cond_type) {
    case ConditionType::PIERCE:
      *out_has_pierce = 1;
      return false;
    case ConditionType::RAMPAGE:
      return apply_check_result;
    case ConditionType::UNKNOWN_20:
      if (card_short_status && ce && (ce->def.self_cost < 3)) {
        return apply_check_result;
      }
      return false;
    case ConditionType::UNKNOWN_21:
      if (card_short_status && ce && (ce->def.self_cost > 2)) {
        return apply_check_result;
      }
      return false;
    case ConditionType::MAJOR_RAMPAGE:
      if (!card_short_status) {
        return apply_check_result;
      }
      if (client_short_statuses) {
        const auto& sc_stat = client_short_statuses->at(0);
        auto ce = this->definition_for_card_ref(sc_stat.card_ref);
        if (ce && (ce->def.type == CardType::HUNTERS_SC) &&
            (this->get_card_ref_max_hp(sc_stat.card_ref) <= sc_stat.current_hp * 2)) {
          return apply_check_result;
        }
      }
      return false;
    case ConditionType::MAJOR_PIERCE:
    case ConditionType::HEAVY_PIERCE:
      *out_has_pierce = 1;
      return false;
    case ConditionType::HEAVY_RAMPAGE:
      if (!card_short_status) {
        return apply_check_result;
      }
      if (client_short_statuses) {
        auto ce = this->definition_for_card_ref(client_short_statuses->at(0).card_ref);
        if (ce && (ce->def.type == CardType::HUNTERS_SC)) {
          size_t count = 0;
          for (size_t z = 7; z < 15; z++) {
            if (this->card_exists_by_status(client_short_statuses->at(z))) {
              count++;
            }
          }
          if (count >= 3) {
            return apply_check_result;
          }
        }
      }
      return false;
    default:
      return false;
  }
}

bool RulerServer::check_usability_or_apply_condition_for_card_refs(
    uint16_t card_ref1,
    uint16_t card_ref2,
    uint16_t card_ref3,
    uint8_t def_effect_index,
    AttackMedium attack_medium) const {
  uint8_t client_id1 = client_id_for_card_ref(card_ref1);
  uint8_t client_id2 = client_id_for_card_ref(card_ref2);
  uint16_t card_id1 = this->card_id_for_card_ref(card_ref1);
  uint16_t card_id2 = this->card_id_for_card_ref(card_ref2);
  uint16_t card_id3 = this->card_id_for_card_ref(card_ref3);
  if (static_cast<uint8_t>(attack_medium) & 0x80) { // Presumably to detect 0xFF
    attack_medium = AttackMedium::UNKNOWN;
  }
  return this->check_usability_or_condition_apply(
      client_id1, card_id1, client_id2, card_id2, card_id3, def_effect_index, false, attack_medium);
}

bool RulerServer::check_usability_or_condition_apply(
    uint8_t client_id1,
    uint16_t card_id1,
    uint8_t client_id2,
    uint16_t card_id2,
    uint16_t card_id3,
    uint8_t def_effect_index,
    bool is_item_usability_check,
    AttackMedium attack_medium) const {
  auto s = this->server();
  bool is_nte = s->options.is_nte();
  auto log = s->log_stack(phosg::string_printf("check_usability_or_condition_apply(%02hhX, #%04hX, %02hhX, #%04hX, #%04hX, %02hhX, %s, %s): ", client_id1, card_id1, client_id2, card_id2, card_id3, def_effect_index, is_item_usability_check ? "true" : "false", phosg::name_for_enum(attack_medium)));

  if (static_cast<uint8_t>(attack_medium) & 0x80) {
    attack_medium = AttackMedium::UNKNOWN;
  }

  auto ce1 = this->definition_for_card_id(card_id1);
  auto ce2 = this->definition_for_card_id(card_id2);
  auto ce3 = this->definition_for_card_id(card_id3);
  if (!ce1) {
    log.debug("ce1 missing");
    return false;
  }
  if (!is_nte && (ce1->def.type == CardType::ITEM) && this->card_id_is_boss_sc(card_id2)) {
    log.debug("ce1 is item and card_id2 is boss sc");
    return false;
  }

  CriterionCode criterion_code;
  if (def_effect_index == 0xFF) {
    criterion_code = ce1->def.usable_criterion;
  } else {
    if (def_effect_index > 2) {
      log.debug("invalid def_effect_index");
      return false;
    }
    criterion_code = ce1->def.effects[def_effect_index].apply_criterion;
  }
  log.debug("criterion_code=%s", phosg::name_for_enum(criterion_code));

  // For item usability checks, prevent criteria that depend on player
  // positioning/team setup
  if (is_item_usability_check &&
      ((criterion_code == CriterionCode::SAME_TEAM) ||
          (criterion_code == CriterionCode::SAME_PLAYER) ||
          (criterion_code == CriterionCode::SAME_TEAM_NOT_SAME_PLAYER) ||
          (criterion_code == CriterionCode::FC) ||
          (criterion_code == CriterionCode::NOT_SC) ||
          (criterion_code == CriterionCode::SC))) {
    log.debug("criterion is forbidden");
    criterion_code = CriterionCode::NONE;
  }

  // Presumably this odd-looking expression here is used to handle two different
  // cases. When checking for a condition, def_effect_index should be non-0xFF,
  // so we'd return true if the criterion passes. When checking if an item or
  // creature card is usable, the two client IDs should be the same or the
  // second should not be given, so we'd return true if the criterion passes. If
  // neither of these cases apply, we should return false as a failsafe even if
  // the criterion passes. NTE did not have such a check.
  bool ret = is_nte || (!(def_effect_index & 0x80) || (client_id1 == client_id2)) || (client_id2 == 0xFF);
  switch (criterion_code) {
    case CriterionCode::NONE:
      return ret;
    case CriterionCode::HU_CLASS_SC:
      if (ce2 && (ce2->def.card_class() == CardClass::HU_SC)) {
        return ret;
      }
      break;
    case CriterionCode::RA_CLASS_SC:
      if (ce2 && (ce2->def.card_class() == CardClass::RA_SC)) {
        return ret;
      }
      break;
    case CriterionCode::FO_CLASS_SC:
      if (ce2 && (ce2->def.card_class() == CardClass::FO_SC)) {
        return ret;
      }
      break;
    case CriterionCode::SAME_TEAM:
      if ((client_id1 == client_id2) ||
          ((client_id1 != 0xFF) && (client_id2 != 0xFF) &&
              (this->team_id_for_client_id[client_id1] == this->team_id_for_client_id[client_id2]))) {
        return true;
      }
      break;
    case CriterionCode::SAME_PLAYER:
      if (client_id1 != client_id2) {
        return true;
      }
      break;
    case CriterionCode::SAME_TEAM_NOT_SAME_PLAYER:
      if ((client_id2 != client_id1) && (client_id1 != 0xFF) && (client_id2 != 0xFF) &&
          (this->team_id_for_client_id[client_id1] == this->team_id_for_client_id[client_id2])) {
        return true;
      }
      break;
    case CriterionCode::FC:
      if (!ce3 || ((ce3->def.type != CardType::HUNTERS_SC) && (ce3->def.type != CardType::ARKZ_SC))) {
        return ret;
      }
      break;
    case CriterionCode::NOT_SC:
      if (!ce2 || ((ce2->def.type != CardType::HUNTERS_SC) && (ce2->def.type != CardType::ARKZ_SC))) {
        return ret;
      }
      break;
    case CriterionCode::SC:
      if (ce2 && ((ce2->def.type == CardType::HUNTERS_SC) || (ce2->def.type == CardType::ARKZ_SC))) {
        return ret;
      }
      break;
    case CriterionCode::HU_OR_RA_CLASS_SC:
      if (ce2 && ((ce2->def.card_class() == CardClass::HU_SC) || (ce2->def.card_class() == CardClass::RA_SC))) {
        return ret;
      }
      break;
    case CriterionCode::HUNTER_NON_ANDROID_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0002, // Kranz
          0x0003, // Ino'lis
          0x0004, // Sil'fer
          0x0006, // Kylria
          0x0111, // Relmitos
          0x0112, // Viviana
          0x0115, // Glustar
          0x02AA, // H-HUmar
          0x02AB, // H-HUnewearl
          0x02AE, // H-RAmar
          0x02AF, // H-RAmarl
          0x02B2, // H-FOmar
          0x02B3, // H-FOmarl
          0x02B4, // H-FOnewm
          0x02B5, // H-FOnewearl
          0x02CC, // H-HUmar
          0x02CD, // H-RAmarl
          0x02CE, // H-FOmarl
          0x02CF, // H-HUnewearl
          0x02D1, // H-RAmarl
          0x02D5, // H-FOmar
          0x02D6, // H-FOnewearl
          0x02D9, // H-FOnewm
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_HU_CLASS_MALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0113, // Teifu
          0x02AA, // H-HUmar
          0x02AC, // H-HUcast
          0x02CC, // H-HUmar
          0x02D7, // H-HUcast
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_FEMALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0003, // Ino'lis
          0x0004, // Sil'fer
          0x0006, // Kylria
          0x0110, // Saligun
          0x0112, // Viviana
          0x0114, // Stella
          0x02AB, // H-HUnewearl
          0x02AD, // H-HUcaseal
          0x02AF, // H-RAmarl
          0x02B1, // H-RAcaseal
          0x02B3, // H-FOmarl
          0x02B5, // H-FOnewearl
          // Note: Seems like 0x02CD (H-RAmarl) should be here, but she isn't.
          0x02CE, // H-FOmarl
          0x02CF, // H-HUnewearl
          0x02D1, // H-RAmarl
          0x02D4, // H-HUcaseal
          0x02D6, // H-FOnewearl
          0x02D8, // H-RAcaseal
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_NON_RA_CLASS_HUMAN_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0003, // Ino'lis
          0x0004, // Sil'fer
          0x0111, // Relmitos
          0x0115, // Glustar
          0x0112, // Viviana
          0x02AA, // H-HUmar
          0x02AB, // H-HUnewearl
          0x02B2, // H-FOmar
          0x02B3, // H-FOmarl
          0x02B4, // H-FOnewm
          0x02B5, // H-FOnewearl
          0x02CC, // H-HUmar
          0x02CE, // H-FOmarl
          0x02CF, // H-HUnewearl
          0x02D5, // H-FOmar
          0x02D6, // H-FOnewearl
          0x02D9, // H-FOnewm
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_HU_CLASS_ANDROID_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0110, // Saligun
          0x0113, // Teifu
          0x02AC, // H-HUcast
          0x02AD, // H-HUcaseal
          0x02D4, // H-HUcaseal
          0x02D7, // H-HUcast
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_NON_RA_CLASS_NON_NEWMAN_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0003, // Ino'lis
          0x0110, // Saligun
          0x0111, // Relmitos
          0x0113, // Teifu
          0x02AA, // H-HUmar
          0x02AC, // H-HUcast
          0x02AD, // H-HUcaseal
          0x02B2, // H-FOmar
          0x02B3, // H-FOmarl
          0x02CC, // H-HUmar
          0x02CE, // H-FOmarl
          0x02D4, // H-HUcaseal
          0x02D5, // H-FOmar
          0x02D7, // H-HUcast
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_NON_NEWMAN_NON_FORCE_MALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0002, // Kranz
          0x0005, // Guykild
          0x0113, // Teifu
          0x02AA, // H-HUmar
          0x02AC, // H-HUcast
          0x02AE, // H-RAmar
          0x02B0, // H-RAcast
          0x02CC, // H-HUmar
          // Seems like H-RAmarl shouldn't be here, but she is.
          0x02CD, // H-RAmarl
          0x02D0, // H-RAcast
          0x02D7, // H-HUcast
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_HUNEWEARL_CLASS_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0004, // Sil'fer
          0x02AB, // H-HUnewearl
          0x02CF, // H-HUnewearl
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_RA_CLASS_MALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0002, // Kranz
          0x0005, // Guykild
          0x02AE, // H-RAmar
          0x02B0, // H-RAcast
          0x02CD, // H-RAmarl
          0x02D0, // H-RAcast
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_RA_CLASS_FEMALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0006, // Kylria
          0x0114, // Stella
          0x02AF, // H-RAmarl
          0x02B1, // H-RAcaseal
          0x02D1, // H-RAmarl
          0x02D2, // D-RAcaseal
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_RA_OR_FO_CLASS_FEMALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0003, // Ino'lis
          0x0006, // Kylria
          0x0112, // Viviana
          0x0114, // Stella
          0x02AF, // H-RAmarl
          0x02B1, // H-RAcaseal
          0x02B3, // H-FOmarl
          0x02B5, // H-FOnewearl
          0x02CE, // H-FOmarl
          0x02D1, // H-RAmarl
          0x02D6, // H-FOnewearl
          0x02D8, // H-RAcaseal
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_HU_OR_RA_CLASS_HUMAN_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0001, // Orland
          0x0002, // Kranz
          0x0004, // Sil'fer
          0x0006, // Kylria
          0x02AA, // H-HUmar
          0x02AB, // H-HUnewearl
          0x02AE, // H-RAmar
          0x02AF, // H-RAmarl
          0x02CC, // H-HUmar
          0x02CD, // H-RAmarl
          0x02CF, // H-HUnewearl
          0x02D1, // H-RAmarl
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_RA_CLASS_ANDROID_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0005, // Guykild
          0x0114, // Stella
          0x02B0, // H-RAcast
          0x02B1, // H-RAcaseal
          0x02D0, // H-RAcast
          0x02D8, // H-RAcaseal
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_FO_CLASS_FEMALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0003, // Ino'lis
          0x0112, // Viviana
          0x02B3, // H-FOmarl
          0x02B5, // H-FOnewearl
          0x02CE, // H-FOmarl
          0x02D6, // H-FOnewearl
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_HUMAN_FEMALE_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0003, // Ino'lis
          0x0004, // Sil'fer
          0x0006, // Kylria
          0x0112, // Viviana
          0x02AB, // H-HUnewearl
          0x02AF, // H-RAmarl
          0x02B3, // H-FOmarl
          0x02B5, // H-FOnewearl
          0x02CE, // H-FOmarl
          0x02CF, // H-HUnewearl
          0x02D1, // H-RAmarl
          0x02D6, // H-FOnewearl
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HUNTER_ANDROID_SC: {
      static const unordered_set<uint16_t> card_ids = {
          0x0005, // Guykild
          0x0110, // Saligun
          0x0113, // Teifu
          0x0114, // Stella
          0x02AC, // H-HUcast
          0x02AD, // H-HUcaseal
          0x02B0, // H-RAcast
          0x02B1, // H-RAcaseal
          0x02D0, // H-RAcast
          0x02D4, // H-HUcaseal
          0x02D7, // H-HUcast
          0x02D8, // H-RAcaseal
      };
      return ret && card_ids.count(card_id2);
    }
    case CriterionCode::HU_OR_FO_CLASS_SC:
      if (ce2 && ((ce2->def.card_class() == CardClass::HU_SC) || (ce2->def.card_class() == CardClass::FO_SC))) {
        return ret;
      }
      break;
    case CriterionCode::RA_OR_FO_CLASS_SC:
      if (ce2 && ((ce2->def.card_class() == CardClass::RA_SC) || (ce2->def.card_class() == CardClass::FO_SC))) {
        return ret;
      }
      break;
    case CriterionCode::PHYSICAL_OR_UNKNOWN_ATTACK_MEDIUM:
      if ((attack_medium == AttackMedium::UNKNOWN) || (attack_medium == AttackMedium::PHYSICAL)) {
        return ret;
      }
      break;
    case CriterionCode::TECH_OR_UNKNOWN_ATTACK_MEDIUM:
      if ((attack_medium == AttackMedium::UNKNOWN) || (attack_medium == AttackMedium::TECH)) {
        return ret;
      }
      break;
    case CriterionCode::PHYSICAL_OR_TECH_OR_UNKNOWN_ATTACK_MEDIUM:
      if ((attack_medium == AttackMedium::UNKNOWN) || (attack_medium == AttackMedium::PHYSICAL) || (attack_medium == AttackMedium::TECH)) {
        return ret;
      }
      break;
    case CriterionCode::NON_PHYSICAL_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC:
      if ((attack_medium != AttackMedium::PHYSICAL) && (attack_medium != AttackMedium::UNKNOWN)) {
        return false;
      }
      if (!ce3 || ((ce3->def.type != CardType::HUNTERS_SC) && (ce3->def.type != CardType::ARKZ_SC))) {
        return ret;
      }
      break;
    case CriterionCode::NON_PHYSICAL_NON_TECH_ATTACK_MEDIUM_NON_SC:
      if ((attack_medium != AttackMedium::PHYSICAL) && (attack_medium != AttackMedium::TECH)) {
        return false;
      }
      if (!ce3 || ((ce3->def.type != CardType::HUNTERS_SC) && (ce3->def.type != CardType::ARKZ_SC))) {
        return ret;
      }
      break;
    case CriterionCode::NON_PHYSICAL_NON_TECH_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC:
      if ((attack_medium != AttackMedium::UNKNOWN) && (attack_medium != AttackMedium::PHYSICAL) && (attack_medium != AttackMedium::TECH)) {
        return false;
      }
      if (!ce3 || ((ce3->def.type != CardType::HUNTERS_SC) && (ce3->def.type != CardType::ARKZ_SC))) {
        return ret;
      }
  }

  log.debug("default return (false)");
  return false;
}

uint16_t RulerServer::compute_attack_or_defense_costs(
    const ActionState& pa,
    bool allow_mighty_knuckle,
    uint8_t* out_ally_cost) const {
  int16_t final_cost = 1;
  bool has_mighty_knuckle = false;
  int16_t cost_bias = 0;
  int16_t tech_cost_bias = 0;
  int16_t assist_cost_bias = 0;
  int16_t total_cost = 0;
  int16_t total_ally_cost = 0;

  if (pa.client_id == 0xFFFF) {
    return 99;
  }

  if (out_ally_cost) {
    *out_ally_cost = 0;
  }

  auto action_type = this->get_pending_action_type(pa);
  auto ce = this->definition_for_card_ref(pa.attacker_card_ref);
  uint8_t client_id = client_id_for_card_ref(pa.attacker_card_ref);

  uint16_t sc_card_ref_if_item = 0xFFFF;
  if ((client_id != 0xFF) && ce && (ce->def.type == CardType::ITEM) &&
      this->short_statuses[client_id]) {
    sc_card_ref_if_item = this->short_statuses[client_id]->at(0).card_ref;
  }

  if (this->find_condition_on_card_ref(pa.attacker_card_ref, ConditionType::ADD_1_TO_MV_COST) ||
      this->find_condition_on_card_ref(sc_card_ref_if_item, ConditionType::ADD_1_TO_MV_COST)) {
    cost_bias = 1;
  }

  if (((action_type == ActionType::ATTACK) || (action_type == ActionType::INVALID_00)) &&
      (this->find_condition_on_card_ref(pa.attacker_card_ref, ConditionType::BIG_SWING) ||
          this->find_condition_on_card_ref(sc_card_ref_if_item, ConditionType::BIG_SWING))) {
    cost_bias++;
  }

  bool is_nte = this->server()->options.is_nte();
  if (pa.action_card_refs[0] == 0xFFFF) {
    total_cost = cost_bias + 1;
  } else {
    if (this->find_condition_on_card_ref(pa.attacker_card_ref, ConditionType::TECH) ||
        this->find_condition_on_card_ref(sc_card_ref_if_item, ConditionType::TECH)) {
      tech_cost_bias = -1;
    }

    auto s = this->server();
    for (size_t z = 0; pa.action_card_refs[z] != 0xFFFF; z++) {
      auto ce = this->definition_for_card_ref(pa.action_card_refs[z]);
      if (has_mighty_knuckle || !ce || (ce->def.type != CardType::ACTION)) {
        return 99;
      }
      total_cost += (ce->def.self_cost + cost_bias);
      if (card_class_is_tech_like(ce->def.card_class(), is_nte)) {
        total_cost += tech_cost_bias;
      }
      total_ally_cost += ce->def.ally_cost;
      if (this->card_has_mighty_knuckle(pa.action_card_refs[z])) {
        has_mighty_knuckle = true;
      }
      if (!is_nte) {
        size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(pa.client_id);
        for (size_t w = 0; w < num_assists; w++) {
          auto assist_effect = this->assist_server->get_active_assist_by_index(w);
          if (assist_effect == AssistEffect::INFLATION) {
            assist_cost_bias++;
          } else if (assist_effect == AssistEffect::DEFLATION) {
            assist_cost_bias--;
          }
        }
      }
    }
  }

  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(pa.client_id);
  for (size_t w = 0; w < num_assists; w++) {
    auto assist_effect = this->assist_server->get_active_assist_by_index(w);
    if (is_nte && (assist_effect == AssistEffect::INFLATION)) {
      assist_cost_bias++;
    } else if (is_nte && (assist_effect == AssistEffect::DEFLATION)) {
      assist_cost_bias--;
    } else if ((assist_effect == AssistEffect::BATTLE_ROYALE) && (pa.action_card_refs[0] == 0xFFFF)) {
      total_cost = 0;
      final_cost = 0;
    }
  }

  if (has_mighty_knuckle) {
    if (!allow_mighty_knuckle) {
      if (!is_nte) {
        final_cost = 0;
      }
    } else {
      final_cost = max<int16_t>(final_cost, this->hand_and_equip_states[pa.client_id]->atk_points);
    }
  }

  if (out_ally_cost) {
    *out_ally_cost = total_ally_cost;
  }
  return max<int16_t>(final_cost, total_cost + assist_cost_bias);
}

bool RulerServer::compute_effective_range_and_target_mode_for_attack(
    const ActionState& pa,
    uint16_t* out_effective_card_id,
    TargetMode* out_effective_target_mode,
    uint16_t* out_orig_card_ref) const {
  auto s = this->server();
  bool is_nte = s->options.is_nte();
  auto log = s->log_stack("compute_effective_range_and_target_mode_for_attack: ");

  size_t z;
  for (z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
  }
  if (z >= 8) {
    log.debug("too many action card refs");
    return false;
  }
  log.debug("%zu action card refs", z);
  uint16_t card_ref = (z == 0) ? pa.attacker_card_ref : pa.action_card_refs[z - 1];
  log.debug("base card ref = @%04hX", card_ref);

  uint16_t card_id = this->card_id_for_card_ref(card_ref);
  if (card_id == 0xFFFF) {
    log.debug("card ref is broken");
    return false;
  }

  auto ce = this->definition_for_card_id(card_id);
  uint8_t client_id = client_id_for_card_ref(pa.attacker_card_ref);
  if ((client_id == 0xFF) || !ce) {
    log.debug("card ref is broken or definition is missing");
    return false;
  }

  if (out_orig_card_ref) {
    log.debug("orig_card_ref = @%04hX", card_ref);
    *out_orig_card_ref = card_ref;
  }

  auto target_mode = ce->def.target_mode;
  if (this->card_ref_or_sc_has_fixed_range(pa.attacker_card_ref)) {
    const char* target_mode_name = name_for_target_mode(target_mode);
    log.debug("attacker card ref @%04hX has fixed range; target mode is %s (%hhu)",
        pa.attacker_card_ref.load(), target_mode_name, static_cast<uint8_t>(target_mode));
    card_id = this->card_id_for_card_ref(pa.attacker_card_ref);
    if (!is_nte) {
      auto sc_ce = this->definition_for_card_id(card_id);
      if (sc_ce && (static_cast<uint8_t>(target_mode) < 6)) {
        target_mode = sc_ce->def.target_mode;
        const char* target_mode_name = name_for_target_mode(target_mode);
        log.debug("sc_ce overrides target mode with %s (%hhu)",
            target_mode_name, static_cast<uint8_t>(target_mode));
      }
    }
  }

  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto assist_effect = this->assist_server->get_active_assist_by_index(z);
    if (assist_effect == AssistEffect::SIMPLE) {
      card_id = this->card_id_for_card_ref(pa.attacker_card_ref);
      log.debug("SIMPLE assist overrides card id with #%04hX", card_id);
    } else if (assist_effect == AssistEffect::HEAVY_FOG) {
      card_id = 0xFFFE;
      log.debug("HEAVY_FOG assist overrides card id with #%04hX", card_id);
    }
  }

  if (out_effective_target_mode) {
    *out_effective_target_mode = target_mode;
  }
  if (out_effective_card_id) {
    *out_effective_card_id = card_id;
  }
  return true;
}

size_t RulerServer::count_rampage_targets_for_attack(const ActionState& pa, uint8_t client_id) const {
  if (client_id == 0xFF) {
    return 0;
  }

  auto stat = this->short_statuses[client_id];
  if (!stat || !this->card_exists_by_status(stat->at(0))) {
    return 0;
  }

  auto ce = this->definition_for_card_ref(stat->at(0).card_ref);
  if (ce->def.type != CardType::HUNTERS_SC) {
    return 0;
  }

  size_t ret = 0;
  for (size_t z = 7; z < 15; z++) {
    const auto& stat_entry = stat->at(z);
    if (this->card_exists_by_status(stat_entry) &&
        this->attack_action_has_rampage_and_not_pierce(pa, stat_entry.card_ref)) {
      ret++;
    }
  }
  return ret;
}

bool RulerServer::defense_card_can_apply_to_attack(
    uint16_t defense_card_ref,
    uint16_t attacker_card_ref,
    uint16_t attacker_sc_card_ref) const {
  uint16_t defense_card_id = this->card_id_for_card_ref(defense_card_ref);
  uint16_t attacker_sc_card_id = this->card_id_for_card_ref(attacker_sc_card_ref);
  uint16_t attacker_card_id = this->card_id_for_card_ref(attacker_card_ref);
  auto defense_card_ce = this->definition_for_card_id(defense_card_id);
  auto attacker_sc_card_ce = this->definition_for_card_id(attacker_sc_card_id);
  auto attacker_card_ce = this->definition_for_card_id(attacker_card_id);
  if (!defense_card_ce) {
    return false;
  }

  const auto* chain = this->action_chain_with_conds_for_card_ref(attacker_card_ref);
  if (!chain) {
    return false;
  }

  for (size_t z = 0; z < 9; z++) {
    const auto& cond = chain->conditions[z];
    if (cond.type == ConditionType::DEF_DISABLE_BY_COST) {
      uint8_t min_cost = cond.value / 10;
      uint8_t max_cost = cond.value % 10;
      if (defense_card_ce->def.self_cost >= min_cost && defense_card_ce->def.self_cost <= max_cost) {
        return false;
      }
    }
  }

  for (size_t z = 0; z < 3; z++) {
    switch (defense_card_ce->def.effects[z].type) {
      case ConditionType::NATIVE_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::NATIVE_CREATURE)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::NATIVE_CREATURE))) {
          return false;
        }
        break;
      case ConditionType::A_BEAST_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::A_BEAST_CREATURE)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::A_BEAST_CREATURE))) {
          return false;
        }
        break;
      case ConditionType::MACHINE_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::MACHINE_CREATURE)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::MACHINE_CREATURE))) {
          return false;
        }
        break;
      case ConditionType::DARK_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::DARK_CREATURE)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::DARK_CREATURE))) {
          return false;
        }
        break;
      case ConditionType::SWORD_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::SWORD_ITEM)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::SWORD_ITEM))) {
          return false;
        }
        break;
      case ConditionType::GUN_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::GUN_ITEM)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::GUN_ITEM))) {
          return false;
        }
        break;
      case ConditionType::CANE_SHIELD:
        if ((!attacker_sc_card_ce || (attacker_sc_card_ce->def.card_class() != CardClass::CANE_ITEM)) &&
            (!attacker_card_ce || (attacker_card_ce->def.card_class() != CardClass::CANE_ITEM))) {
          return false;
        }
        break;
      default:
        break;
    }
  }

  return true;
}

bool RulerServer::defense_card_matches_any_attack_card_top_color(const ActionState& pa) const {
  auto ce = this->definition_for_card_ref(pa.action_card_refs[0]);
  if (!ce) {
    throw runtime_error("defense card definition is missing");
  }
  const auto* chain = this->action_chain_with_conds_for_card_ref(
      pa.original_attacker_card_ref);
  if (chain->chain.attack_action_card_ref_count < 1) {
    auto other_ce = this->definition_for_card_ref(pa.original_attacker_card_ref);
    if (other_ce && other_ce->def.any_top_color_matches(ce->def)) {
      return true;
    }
  }

  for (size_t z = 0; z < chain->chain.attack_action_card_ref_count; z++) {
    auto other_ce = this->definition_for_card_ref(chain->chain.attack_action_card_refs[z]);
    if (other_ce && other_ce->def.any_top_color_matches(ce->def)) {
      return true;
    }
  }
  return false;
}

shared_ptr<const CardIndex::CardEntry> RulerServer::definition_for_card_ref(uint16_t card_ref) const {
  uint16_t card_id = this->card_id_for_card_ref(card_ref);
  if (card_id == 0xFFFF) {
    return nullptr;
  }
  return this->definition_for_card_id(card_id);
}

int32_t RulerServer::error_code_for_client_setting_card(
    uint8_t client_id,
    uint16_t card_ref,
    const Location* loc,
    uint8_t assist_target_client_id) const {
  if (client_id > 3) {
    return -0x7D;
  }
  auto hes = this->hand_and_equip_states[client_id];
  if (!hes) {
    return -0x7D;
  }

  if (hes->assist_flags & AssistFlag::IS_SKIPPING_TURN) {
    return -0x76;
  }

  bool is_nte = this->server()->options.is_nte();
  if (!is_nte && !this->is_card_ref_in_hand(card_ref)) {
    return -0x5E;
  }

  uint16_t card_id = this->card_id_for_card_ref(card_ref);
  if ((hes->assist_flags & AssistFlag::SAME_CARD_BANNED) && (card_id != 0xFFFF)) {
    for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
      auto other_hes = this->hand_and_equip_states[other_client_id];
      if (!other_hes) {
        continue;
      }
      for (size_t z = 0; z < 8; z++) {
        if (card_id == this->card_id_for_card_ref(other_hes->set_card_refs2[z])) {
          return -0x76;
        }
      }
      if (card_id == this->card_id_for_card_ref(other_hes->assist_card_ref2)) {
        return -0x76;
      }
    }
  }

  auto ce = this->definition_for_card_id(card_id);
  if (!ce ||
      (static_cast<uint8_t>(ce->def.type) > 0x05) ||
      (ce->def.type == CardType::HUNTERS_SC) ||
      (ce->def.type == CardType::ARKZ_SC) ||
      (ce->def.type == CardType::ACTION)) {
    return -0x7D;
  }

  if (ce->def.type == CardType::ASSIST) {
    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    for (size_t z = 0; z < num_assists; z++) {
      if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::ASSISTLESS) {
        return -0x76;
      }
    }

    // Check for assists that can only be set on yourself
    auto eff = assist_effect_number_for_card_id(ce->def.card_id, is_nte);
    if (((eff == AssistEffect::LEGACY) || (!is_nte && (eff == AssistEffect::EXCHANGE))) &&
        (assist_target_client_id != 0xFF) &&
        (assist_target_client_id != client_id_for_card_ref(card_ref))) {
      return -0x75;
    }

  } else if (hes->assist_flags & AssistFlag::CANNOT_SET_FIELD_CHARACTERS) { // Item or creature
    return -0x76;
  }

  int16_t set_cost = this->set_cost_for_card(client_id, card_ref);
  if (set_cost < 0) {
    return set_cost;
  }
  if (hes->atk_points < set_cost) {
    return -0x80;
  }

  auto short_statuses = this->short_statuses[client_id];
  if ((short_statuses->at(0).card_ref == 0xFFFF) ||
      !this->card_exists_by_status(short_statuses->at(0)) ||
      !this->check_usability_or_apply_condition_for_card_refs(
          card_ref, short_statuses->at(0).card_ref, 0xFFFF, 0xFF, AttackMedium::INVALID_FF)) {
    return -0x75;
  }

  bool card_in_hand = false;
  for (size_t z = 1; z < 7; z++) {
    if (short_statuses->at(z).card_ref == card_ref) {
      card_in_hand = true;
      break;
    }
  }
  if (!card_in_hand) {
    return -0x7D;
  }

  if ((ce->def.type == CardType::ITEM) || (ce->def.type == CardType::CREATURE)) {
    int16_t existing_fcs_cost = 0;
    bool limit_summoning_by_count = !is_nte &&
        this->find_condition_on_card_ref(short_statuses->at(0).card_ref, ConditionType::FC_LIMIT_BY_COUNT);
    for (size_t z = 7; z < 15; z++) {
      const auto& this_status = short_statuses->at(z);
      if ((this_status.card_ref != 0xFFFF) && this->card_exists_by_status(this_status)) {
        auto this_ce = this->definition_for_card_ref(this_status.card_ref);
        if (!this_ce) {
          return -0x7D;
        }
        existing_fcs_cost += limit_summoning_by_count ? 2 : this_ce->def.self_cost;
      }
    }

    int16_t new_fcs_cost = existing_fcs_cost + (limit_summoning_by_count ? 2 : ce->def.self_cost);
    if (new_fcs_cost > 8) {
      return -0x77;
    }
  }

  if (ce->def.type == CardType::CREATURE) {
    int16_t summon_cost = ce->def.self_cost;
    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    for (size_t z = 0; z < num_assists; z++) {
      if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::FLATLAND) {
        summon_cost = 0;
      }
    }

    if (loc && !this->map_and_rules->tile_is_vacant(loc->x, loc->y)) {
      return -0x7E;
    }

    uint8_t team_id = this->team_id_for_client_id[client_id];
    if (team_id == 0xFF) {
      return -0x78;
    }
    if (!loc) {
      return 0;
    }

    if (is_nte) {
      // It seems NTE assumes that teams always start on the same ends of the
      // map; non-NTE removes this restriction.
      if (team_id == 1) {
        if (((loc->x < 1) ||
                (loc->x >= this->map_and_rules->map.width - 1) ||
                (loc->y < summon_cost + 1) ||
                (loc->y >= this->map_and_rules->map.height - 1)) &&
            (loc->y != this->map_and_rules->map.height - 2)) {
          return -0x7E;
        }
      } else if (((loc->x < 1) ||
                     (loc->x >= this->map_and_rules->map.width - 1) ||
                     (loc->y < 1) ||
                     (loc->y >= this->map_and_rules->map.height - summon_cost - 1)) &&
          (loc->y != 1)) {
        return -0x7E;
      }

    } else {
      Location summon_area_loc;
      uint8_t summon_area_size;
      if (!this->get_creature_summon_area(client_id, &summon_area_loc, &summon_area_size)) {
        if (team_id != 1) {
          if ((loc->x > 0) && (loc->x < this->map_and_rules->map.width - 1)) {
            if ((loc->y < this->map_and_rules->map.height - summon_cost - 1) &&
                (loc->y > 0)) {
              return 0;
            }
            if (loc->y == 1) {
              return 0;
            }
          }
        } else {
          if ((loc->x > 0) &&
              (loc->x < this->map_and_rules->map.width - 1)) {
            if ((summon_cost + 1 <= loc->y) && (loc->y < this->map_and_rules->map.height - 1)) {
              return 0;
            }
            if (loc->y == this->map_and_rules->map.height - 2) {
              return 0;
            }
          }
        }
        return -0x7E;
      }

      int32_t x_offset = 0, y_offset = 0;
      this->offsets_for_direction(summon_area_loc, &x_offset, &y_offset);
      if (x_offset == 0) {
        if ((loc->x < 1) && (loc->x >= this->map_and_rules->map.width - 1)) {
          return -0x7E;
        }
      } else {
        int16_t diff = max<int16_t>(summon_area_size - summon_cost, 0);
        if (x_offset > 0) {
          if (loc->x < summon_area_loc.x) {
            return -0x7E;
          }
          if (loc->x > summon_area_loc.x + diff) {
            return -0x7E;
          }
        } else if (x_offset < 0) {
          if ((loc->x > summon_area_loc.x) || (loc->x < summon_area_loc.x - diff)) {
            return -0x7E;
          }
        }
      }
      if (y_offset == 0) {
        if ((loc->y < 1) && (loc->y >= this->map_and_rules->map.height - 1)) {
          return -0x7E;
        }
      } else {
        int16_t diff = max<int16_t>(summon_area_size - summon_cost, 0);
        if (y_offset > 0) {
          if (loc->y < summon_area_loc.y) {
            return -0x7E;
          }
          if (loc->y > summon_area_loc.y + diff) {
            return -0x7E;
          }
        } else if (y_offset < 0) {
          if ((loc->y > summon_area_loc.y) || (loc->y < summon_area_loc.y - diff)) {
            return -0x7E;
          }
        }
      }
    }
  }
  return 0;
}

bool RulerServer::find_condition_on_card_ref(
    uint16_t card_ref,
    ConditionType cond_type,
    Condition* out_se,
    size_t* out_value_sum,
    bool find_first_instead_of_max) const {
  const auto* chain = this->action_chain_with_conds_for_card_ref(card_ref);
  if (!chain) {
    return false;
  }

  ssize_t found_value = 0;
  ssize_t found_index = -1;
  ssize_t found_order = 9;
  for (size_t z = 0; z < 9; z++) {
    if (chain->conditions[z].type == cond_type) {
      if (!find_first_instead_of_max) {
        if ((found_index == -1) || (found_order < chain->conditions[z].order)) {
          found_order = chain->conditions[z].order;
          found_index = z;
        }
      } else if ((found_index == -1) || (found_value < chain->conditions[z].value)) {
        found_value = chain->conditions[z].value;
        found_index = z;
      }
      if (out_value_sum) {
        *out_value_sum = *out_value_sum + chain->conditions[z].value;
      }
    }
  }

  if (found_index >= 0) {
    if (out_se) {
      *out_se = chain->conditions[found_index];
    }
    return true;
  } else {
    return false;
  }
}

bool RulerServer::flood_fill_move_path(
    const ActionChainWithConds& chain,
    int8_t x,
    int8_t y,
    Direction direction,
    uint8_t max_atk_points,
    int16_t max_distance,
    bool is_free_maneuver_or_aerial,
    bool is_aerial,
    parray<uint8_t, 0x100>* visited_map,
    MovePath* path,
    size_t num_occupied_tiles,
    size_t num_vacant_tiles) const {
  auto state = this->map_and_rules;
  if ((x < 1) || (x >= state->map.width - 1) ||
      (y < 1) || (y >= state->map.height - 1)) {
    return 0;
  }

  bool ret = false;
  bool tile_is_occupied = !state->tile_is_vacant(x, y);
  if (tile_is_occupied) {
    if (!is_free_maneuver_or_aerial) {
      return 0;
    }

  } else {
    uint32_t cost = this->get_path_cost(
        chain,
        num_vacant_tiles + num_occupied_tiles + 1,
        is_aerial ? num_occupied_tiles : 0);
    if (max_atk_points < cost) {
      return 0;
    }
    visited_map->at(x * 0x10 + y) = 1;
    if (path && (path->end_loc.x == x) && (path->end_loc.y == y) &&
        ((path->length == -1) || (cost < path->cost))) {
      ret = true;
      path->reset_totals();
      path->remaining_distance = max_distance;
      path->cost = cost;
      Location step_loc(x, y, direction);
      path->add_step(step_loc);
    }
  }

  if (tile_is_occupied) {
    num_occupied_tiles = num_occupied_tiles + 1;
  } else {
    num_vacant_tiles = num_vacant_tiles + 1;
  }

  int16_t new_max_distance = max_distance - 1;
  if (new_max_distance > 0) {
    static const int8_t offsets[4][2] = {
        {1, 0}, {0, -1}, {-1, 0}, {0, 1}};
    Direction dirs[3] = {direction, turn_left(direction), turn_right(direction)};
    for (size_t dir_index = 0; dir_index < 3; dir_index++) {
      if (static_cast<uint8_t>(dirs[dir_index]) > 3) {
        throw logic_error("invalid direction");
      }
      ret |= this->flood_fill_move_path(
          chain,
          x + offsets[static_cast<uint8_t>(dirs[dir_index])][0],
          y + offsets[static_cast<uint8_t>(dirs[dir_index])][1],
          dirs[dir_index],
          max_atk_points,
          new_max_distance,
          is_free_maneuver_or_aerial,
          is_aerial,
          visited_map,
          path,
          num_occupied_tiles,
          num_vacant_tiles);
    }
  }

  if (path && ret) {
    Location step_loc(x, y, direction);
    path->add_step(step_loc);
    if (tile_is_occupied) {
      path->num_occupied_tiles++;
    }
  }

  return ret;
}

uint16_t RulerServer::get_ally_sc_card_ref(uint16_t card_ref) const {
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if ((client_id != 0xFF) && this->short_statuses[client_id]) {
    for (size_t z = 0; z < 4; z++) {
      if ((z != client_id) &&
          (this->team_id_for_client_id[z] == this->team_id_for_client_id[client_id]) &&
          this->short_statuses[z]) {
        return this->short_statuses[z]->at(0).card_ref;
      }
    }
  }
  return 0xFFFF;
}

shared_ptr<const CardIndex::CardEntry> RulerServer::definition_for_card_id(uint32_t card_id) const {
  return this->server()->definition_for_card_id(card_id);
}

uint32_t RulerServer::get_card_id_with_effective_range(
    uint16_t card_ref, uint16_t card_id_override, TargetMode* out_target_mode) const {
  auto log = this->server()->log_stack(phosg::string_printf("get_card_id_with_effective_range(@%04hX, #%04hX): ", card_ref, card_id_override));

  uint16_t card_id = (card_id_override == 0xFFFF)
      ? this->card_id_for_card_ref(card_ref)
      : card_id_override;
  log.debug("card_id=#%04hX", card_id);

  if (card_id != 0xFFFF) {
    auto ce = this->definition_for_card_id(card_id);
    uint8_t client_id = client_id_for_card_ref(card_ref);
    if ((client_id != 0xFF) && ce) {
      TargetMode effective_target_mode = ce->def.target_mode;
      log.debug("ce valid for #%04hX with effective target mode %s", card_id, name_for_target_mode(effective_target_mode));

      if (this->card_ref_or_sc_has_fixed_range(card_ref)) {
        // Undo the override that may have been passed in
        log.debug("@%04hX has FIXED_RANGE", card_ref);
        card_id = this->card_id_for_card_ref(card_ref);
        auto orig_ce = this->definition_for_card_id(card_id);
        if (orig_ce && (static_cast<uint8_t>(effective_target_mode) < 6)) {
          log.debug("ce valid for #%04hX with effective target mode %s; overriding to %s", card_id, name_for_target_mode(effective_target_mode), name_for_target_mode(orig_ce->def.target_mode));
          effective_target_mode = orig_ce->def.target_mode;
        }
      }

      size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
      for (size_t z = 0; z < num_assists; z++) {
        auto eff = this->assist_server->get_active_assist_by_index(z);
        if (eff == AssistEffect::SIMPLE) {
          card_id = this->card_id_for_card_ref(card_ref);
          log.debug("SIMPLE assist effect is active; using #%04hX for range", card_id);
        } else if (eff == AssistEffect::HEAVY_FOG) {
          card_id = 0xFFFE;
          log.debug("HEAVY_FOG assist effect is active; limiting range to one tile in front");
        }
      }

      if (out_target_mode) {
        *out_target_mode = effective_target_mode;
      }
      log.debug("results: card_id=#%04hX, target_mode=%s", card_id, name_for_target_mode(effective_target_mode));
    }
  }

  return card_id;
}

uint8_t RulerServer::get_card_ref_max_hp(uint16_t card_ref) const {
  const auto* short_status = this->short_status_for_card_ref(card_ref);
  if (short_status && (short_status->max_hp > 0)) {
    return short_status->max_hp;
  }
  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce) {
    return 0;
  } else if (((ce->def.type == CardType::HUNTERS_SC) || (ce->def.type == CardType::ARKZ_SC)) &&
      (this->map_and_rules->rules.char_hp > 0) &&
      (this->server()->options.is_nte() || !this->card_ref_is_boss_sc(card_ref))) {
    return this->map_and_rules->rules.char_hp;
  } else {
    return ce->def.hp.stat;
  }
}

bool RulerServer::get_creature_summon_area(
    uint8_t client_id, Location* out_loc, uint8_t* out_region_size) const {
  if (!this->map_and_rules || (client_id > 3)) {
    return false;
  }

  Location loc;
  uint8_t region_size;
  loc.direction = static_cast<Direction>(
      (this->map_and_rules->start_facing_directions >> ((client_id & 0x0F) << 2)) & 0x000F);
  switch (loc.direction) {
    case Direction::RIGHT:
      loc.x = 1;
      loc.y = 0;
      region_size = this->map_and_rules->map.width - 3;
      break;
    case Direction::LEFT:
      loc.x = this->map_and_rules->map.width - 2;
      loc.y = 0;
      region_size = this->map_and_rules->map.width - 3;
      break;
    case Direction::UP:
      loc.x = 0;
      loc.y = 1;
      region_size = this->map_and_rules->map.height - 3;
      break;
    case Direction::DOWN:
      loc.x = 0;
      loc.y = this->map_and_rules->map.height - 2;
      region_size = this->map_and_rules->map.height - 3;
      break;
    default:
      // This case isn't in the original code; probably it fell through to one
      // of the above
      return false;
  }

  if (out_loc) {
    *out_loc = loc;
  }
  if (out_region_size) {
    *out_region_size = region_size;
  }
  return true;
}

shared_ptr<HandAndEquipState> RulerServer::get_hand_and_equip_state_for_client_id(
    uint8_t client_id) {
  return (client_id < 4) ? this->hand_and_equip_states[client_id] : nullptr;
}

shared_ptr<const HandAndEquipState> RulerServer::get_hand_and_equip_state_for_client_id(
    uint8_t client_id) const {
  return (client_id < 4) ? this->hand_and_equip_states[client_id] : nullptr;
}

bool RulerServer::get_move_path_length_and_cost(
    uint32_t client_id,
    uint32_t card_ref,
    const Location& loc,
    uint32_t* out_length,
    uint32_t* out_cost) const {
  MovePath path;
  parray<uint8_t, 0x100> visited_map;
  path.end_loc = loc;
  if (!this->check_move_path_and_get_cost(
          client_id, card_ref, &visited_map, &path, out_cost)) {
    return false;
  }

  bool path_is_valid = path.is_valid();
  if (out_length) {
    if (!path_is_valid || (path.get_length_plus1() < 2)) {
      *out_length = 99;
    } else {
      *out_length = path.get_length_plus1() - 1;
    }
  }

  return ((path_is_valid && (path.get_length_plus1() > 1)));
}

ssize_t RulerServer::get_path_cost(
    const ActionChainWithConds& chain,
    ssize_t path_length,
    ssize_t cost_penalty) const {
  for (size_t x = 0; x < 9; x++) {
    const auto& cond = chain.conditions[x];
    if (cond.type == ConditionType::SET_MV_COST_TO_0) {
      path_length = 0;
    } else if (cond.type == ConditionType::ADD_1_TO_MV_COST) {
      path_length++;
    } else if (cond.type == ConditionType::SCALE_MV_COST) {
      path_length *= cond.value;
    }
  }
  return clamp<ssize_t>(path_length + cost_penalty, 0, 99);
}

ActionType RulerServer::get_pending_action_type(const ActionState& pa) const {
  auto ce = this->definition_for_card_ref(pa.action_card_refs[0]);
  if (!ce || (ce->def.type != CardType::ACTION)) {
    if (pa.attacker_card_ref == 0xFFFF) {
      return ActionType::INVALID_00;
    } else {
      return ActionType::ATTACK;
    }
  } else {
    if (ce->def.card_class() == CardClass::DEFENSE_ACTION) {
      return ActionType::DEFENSE;
    } else {
      return ActionType::ATTACK;
    }
  }
}

bool RulerServer::is_attack_valid(const ActionState& pa) {
  uint8_t client_id = pa.client_id;
  uint16_t attacker_card_ref = pa.attacker_card_ref;
  if (client_id == 0xFF) {
    this->error_code3 = -0x72;
    return false;
  }

  if (this->hand_and_equip_states[client_id] &&
      (this->hand_and_equip_states[client_id]->assist_flags & AssistFlag::IS_SKIPPING_TURN)) {
    this->error_code3 = -0x70;
    return false;
  }

  // Note: The original code has a case here that results in error code -0x5E,
  // triggered by a function returning false. However, that function always
  // returns true and has no side effects, so we've omitted the case here.

  const auto* attacker_card_status = this->short_status_for_card_ref(attacker_card_ref);
  if (!attacker_card_status ||
      !this->card_ref_can_attack(attacker_card_ref) ||
      (attacker_card_status->card_flags & 0x500)) {
    this->error_code3 = -0x6F;
    return false;
  }

  if (!this->server()->options.is_nte() && (attacker_card_status->card_flags & 2)) {
    this->error_code3 = -0x60;
    return false;
  }

  auto attacker_ce = this->definition_for_card_ref(attacker_card_ref);
  auto attacker_chain = this->action_chain_with_conds_for_card_ref(attacker_card_ref);
  if (!attacker_chain ||
      (attacker_chain->chain.acting_card_ref != attacker_card_ref) ||
      !attacker_ce ||
      ((attacker_ce->def.type != CardType::HUNTERS_SC &&
          (attacker_ce->def.type != CardType::ARKZ_SC) &&
          (attacker_ce->def.type != CardType::CREATURE) &&
          (attacker_ce->def.type != CardType::ITEM)))) {
    this->error_code3 = -0x6F;
    return false;
  }

  uint16_t card_ref = attacker_chain->chain.unknown_card_ref_a3;
  if (card_ref == 0xFFFF) {
    card_ref = attacker_card_ref;
  }

  bool has_permission_effect = false;
  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = this->assist_server->get_active_assist_by_index(z);
    if (eff == AssistEffect::PERMISSION) {
      has_permission_effect = true;
    } else if (eff == AssistEffect::SKIP_ACT) {
      this->error_code3 = -0x6E;
      return false;
    }
  }

  size_t conditional_card_count = 0;
  size_t z;
  for (z = 0; z < 8; z++) {
    uint16_t right_card_ref = pa.action_card_refs[z];
    if (right_card_ref == 0xFFFF) {
      break;
    }

    if (client_id_for_card_ref(right_card_ref) != client_id) {
      this->error_code3 = -0x6D;
      return false;
    }

    auto left_card_ce = (z == 0) ? this->definition_for_card_ref(card_ref) : this->definition_for_card_ref(pa.action_card_refs[z - 1]);
    auto right_card_ce = this->definition_for_card_ref(right_card_ref);

    if (right_card_ce->def.type != CardType::ACTION) {
      this->error_code3 = -0x6C;
      return false;
    }
    if (!left_card_ce || !right_card_ce) {
      this->error_code3 = -0x6C;
      return false;
    }

    uint8_t attacker_client_id = client_id_for_card_ref(pa.attacker_card_ref);
    auto sc_ce = (attacker_client_id != 0xFF) ? this->definition_for_card_ref(this->set_card_action_chains[attacker_client_id]->at(0).chain.acting_card_ref) : nullptr;

    if (!card_linkage_is_valid(right_card_ce, left_card_ce, sc_ce, has_permission_effect)) {
      this->error_code3 = -0x6B;
      return false;
    }

    if (!this->check_usability_or_apply_condition_for_card_refs(
            right_card_ref, attacker_card_ref, 0xFFFF, 0xFF, AttackMedium::INVALID_FF)) {
      this->error_code3 = -0x6A;
      return false;
    }

    if (this->card_ref_has_class_usability_condition(right_card_ref)) {
      conditional_card_count = conditional_card_count + 1;
    }
  }

  if (z >= 9) {
    this->error_code3 = -0x69;
    return false;
  }

  if ((attacker_ce->def.type == CardType::HUNTERS_SC) && ((z == 0) || (z != conditional_card_count))) {
    auto short_statuses = this->short_statuses[client_id];
    for (z = 7; z < 15; z++) {
      if (this->card_ref_can_attack(short_statuses->at(z).card_ref)) {
        this->error_code3 = -0x68;
        return false;
      }
    };
  }

  return true;
}

bool RulerServer::is_attack_or_defense_valid(const ActionState& pa) {
  // This error code is present in the original code, but is no longer possible
  // since we require pa instead of using a pointer.
  // if (!pa) {
  //   this->error_code3 = -0x78;
  //   return false;
  // }

  auto hes = this->get_hand_and_equip_state_for_client_id(pa.client_id);
  if (!hes) {
    this->error_code3 = -0x72;
    return false;
  }

  if (hes->assist_flags & AssistFlag::IS_SKIPPING_TURN) {
    this->error_code3 = -0x70;
    return false;
  }

  // NTE apparently does not check the action's cost here
  bool is_nte = this->server()->options.is_nte();
  int16_t cost = is_nte ? 0 : this->compute_attack_or_defense_costs(pa, false, nullptr);

  switch (this->get_pending_action_type(pa)) {
    case ActionType::ATTACK:
      if (hes->atk_points < cost) {
        this->error_code3 = -0x80;
        return false;
      }
      return this->is_attack_valid(pa);

    case ActionType::DEFENSE:
      if (hes->def_points < cost) {
        this->error_code3 = -0x80;
        return false;
      }
      if (!this->is_defense_valid(pa)) {
        this->error_code3 = -0x80;
        return false;
      }
      return true;

    case ActionType::INVALID_00:
    default:
      this->error_code3 = -0x5F;
      return false;
  }
}

bool RulerServer::is_card_ref_in_hand(uint16_t card_ref) const {
  if (card_ref == 0xFFFF) {
    return true;
  }

  uint8_t client_id = client_id_for_card_ref(card_ref);
  auto hes = this->get_hand_and_equip_state_for_client_id(client_id);
  if (!hes) {
    return false;
  }

  for (size_t z = 0; z < 6; z++) {
    if (hes->hand_card_refs2[z] == card_ref) {
      return true;
    }
  }

  return false;
}

bool RulerServer::is_defense_valid(const ActionState& pa) {
  if ((pa.original_attacker_card_ref == 0xFFFF) ||
      (pa.target_card_refs[0] == 0xFFFF) ||
      (pa.action_card_refs[0] == 0xFFFF)) {
    this->error_code3 = -0x65;
    return false;
  }

  if (pa.client_id > 3) {
    this->error_code3 = -0x65;
    return false;
  }

  if (this->hand_and_equip_states[pa.client_id] &&
      (this->hand_and_equip_states[pa.client_id]->assist_flags & AssistFlag::IS_SKIPPING_TURN)) {
    this->error_code3 = -0x64;
    return false;
  }

  // Note: The original code has a case here that results in error code -0x5E,
  // triggered by a function returning false. However, that function always
  // returns true and has no side effects, so we've omitted the case here.

  const auto* stat = this->short_status_for_card_ref(pa.target_card_refs[0]);
  if ((!stat || !this->card_exists_by_status(*stat)) || (stat->card_flags & 0x800)) {
    this->error_code3 = -0x63;
    return false;
  }

  if (!this->defense_card_matches_any_attack_card_top_color(pa)) {
    this->error_code3 = -0x62;
    return false;
  }

  if (!this->defense_card_can_apply_to_attack(
          pa.action_card_refs[0], pa.target_card_refs[0], pa.original_attacker_card_ref)) {
    this->error_code3 = -0x61;
    return false;
  }

  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(pa.client_id);
  for (size_t z = 0; z < num_assists; z++) {
    if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::SKIP_ACT) {
      this->error_code3 = -0x64;
      return false;
    }
  }

  if (!this->server()->options.is_nte() &&
      (this->find_condition_on_card_ref(pa.target_card_refs[0], ConditionType::HOLD) ||
          this->find_condition_on_card_ref(pa.target_card_refs[0], ConditionType::CANNOT_DEFEND))) {
    this->error_code3 = -0x63;
    return false;
  }

  return true;
}

void RulerServer::link_objects(
    shared_ptr<MapAndRulesState> map_and_rules,
    shared_ptr<StateFlags> state_flags,
    shared_ptr<AssistServer> assist_server) {
  this->map_and_rules = map_and_rules;
  this->state_flags = state_flags;
  this->assist_server = assist_server;
}

size_t RulerServer::max_move_distance_for_card_ref(uint32_t card_ref) const {
  uint16_t card_id = this->card_id_for_card_ref(card_ref);
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (card_id == 0xFFFF) {
    return 0;
  }

  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce) {
    return 0;
  }

  if (this->server()->options.is_nte()) {
    if (ce->def.type == CardType::ITEM) {
      return ce->def.mv.stat;
    }

    Condition cond;
    if (this->find_condition_on_card_ref(card_ref, ConditionType::SET_MV, &cond)) {
      return cond.value;
    }

    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    bool has_stamina_effect = false;
    for (size_t z = 0; z < num_assists; z = z + 1) {
      auto assist = this->assist_server->get_active_assist_by_index(z);
      if (assist == AssistEffect::SNAIL_PACE) {
        return 1;
      } else if (assist == AssistEffect::STAMINA) {
        has_stamina_effect = true;
      }
    }
    return has_stamina_effect ? 99 : ce->def.mv.stat;

  } else {
    ssize_t ret = ce->def.mv.stat;
    Condition cond;
    if (this->find_condition_on_card_ref(card_ref, ConditionType::MV_BONUS, &cond, nullptr, true)) {
      ret += cond.value;
    }
    if (this->find_condition_on_card_ref(card_ref, ConditionType::SET_MV, &cond, nullptr, true)) {
      ret = cond.value;
    }
    ret = max<ssize_t>(0, ret);

    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    bool has_stamina_effect = false;
    for (size_t z = 0; z < num_assists; z++) {
      auto eff = this->assist_server->get_active_assist_by_index(z);
      if (eff == AssistEffect::SNAIL_PACE) {
        return 1;
      }
      if (eff == AssistEffect::STAMINA) {
        has_stamina_effect = true;
      }
    }

    return has_stamina_effect ? 9 : min<ssize_t>(9, ret);
  }
}

RulerServer::MovePath::MovePath()
    : length(-1),
      remaining_distance(0),
      num_occupied_tiles(0),
      cost(0) {}

void RulerServer::MovePath::add_step(const Location& loc) {
  this->step_locs[++this->length] = loc;
}

uint32_t RulerServer::MovePath::get_cost() const {
  return this->cost;
}

uint32_t RulerServer::MovePath::get_length_plus1() const {
  return this->length + 1;
}

void RulerServer::MovePath::reset_totals() {
  this->length = -1;
  this->remaining_distance = 0;
  this->num_occupied_tiles = 0;
  this->cost = 99;
}

bool RulerServer::MovePath::is_valid() const {
  return (this->length >= 0);
}

void RulerServer::offsets_for_direction(
    const Location& loc, int32_t* out_x_offset, int32_t* out_y_offset) {
  // Note: This function has opposite behavior for the UP and DOWN directions
  // as compared to the global array of the same name.
  // TODO: Figure out why this difference exists and document it.
  switch (loc.direction) {
    case Direction::LEFT:
      *out_x_offset = -1;
      *out_y_offset = 0;
      break;
    case Direction::RIGHT:
      *out_x_offset = 1;
      *out_y_offset = 0;
      break;
    case Direction::UP:
      *out_x_offset = 0;
      *out_y_offset = 1;
      break;
    case Direction::DOWN:
      *out_x_offset = 0;
      *out_y_offset = -1;
      break;
    default:
      break;
  }
}

void RulerServer::register_player(
    uint8_t client_id,
    shared_ptr<HandAndEquipState> hes,
    shared_ptr<parray<CardShortStatus, 0x10>> short_statuses,
    shared_ptr<DeckEntry> deck_entry,
    shared_ptr<parray<ActionChainWithConds, 9>> set_card_action_chains,
    shared_ptr<parray<ActionMetadata, 9>> set_card_action_metadatas) {
  this->hand_and_equip_states[client_id] = hes;
  this->short_statuses[client_id] = short_statuses;
  this->deck_entries[client_id] = deck_entry;
  this->set_card_action_chains[client_id] = set_card_action_chains;
  this->set_card_action_metadatas[client_id] = set_card_action_metadatas;
}

void RulerServer::replace_D1_D2_rank_cards_with_Attack(
    parray<le_uint16_t, 0x1F>& card_ids) const {
  for (size_t z = 0; z < card_ids.size(); z++) {
    auto ce = this->definition_for_card_id(card_ids[z]);
    if (ce && ((ce->def.rank == CardRank::D1) || (ce->def.rank == CardRank::D2))) {
      card_ids[z] = 0x008A; // Attack action card
    }
  }
}

AttackMedium RulerServer::get_attack_medium(const ActionState& pa) const {
  bool is_nte = this->server()->options.is_nte();
  for (size_t z = 0; z < 8; z++) {
    uint16_t card_ref = pa.action_card_refs[z];
    if (card_ref == 0xFFFF) {
      return AttackMedium::PHYSICAL;
    }
    auto ce = this->definition_for_card_ref(card_ref);
    if (ce && card_class_is_tech_like(ce->def.card_class(), is_nte)) {
      return AttackMedium::TECH;
    }
  }
  return AttackMedium::PHYSICAL;
}

void RulerServer::set_client_team_id(uint8_t client_id, uint8_t team_id) {
  this->team_id_for_client_id[client_id] = team_id;
}

int32_t RulerServer::set_cost_for_card(uint8_t client_id, uint16_t card_ref) const {
  auto ce = this->definition_for_card_ref(card_ref);
  if (!ce) {
    return -0x7D;
  }

  if ((client_id == 0xFF) || (client_id != client_id_for_card_ref(card_ref))) {
    return -0x7D;
  }

  bool is_nte = this->server()->options.is_nte();
  auto short_statuses = this->short_statuses[client_id];
  int32_t ret = ce->def.self_cost;
  if (!is_nte &&
      short_statuses &&
      this->card_exists_by_status(short_statuses->at(0)) &&
      this->find_condition_on_card_ref(short_statuses->at(0).card_ref, ConditionType::UNKNOWN_69)) {
    ret = 0;
  }

  for (size_t z = 0; z < 4; z++) {
    auto other_short_statuses = this->short_statuses[z];
    if (!other_short_statuses) {
      continue;
    }

    Condition cond;
    if (this->card_exists_by_status(other_short_statuses->at(0)) &&
        this->find_condition_on_card_ref(other_short_statuses->at(0).card_ref, ConditionType::CLONE, &cond) &&
        (static_cast<uint16_t>(cond.value) == ce->def.card_id)) {
      ret = 0;
    }

    for (size_t w = 7; w < 15; w++) {
      const auto& stat = other_short_statuses->at(w);
      if (this->card_exists_by_status(stat) &&
          this->find_condition_on_card_ref(stat.card_ref, ConditionType::CLONE, &cond) &&
          (static_cast<uint16_t>(cond.value) == ce->def.card_id)) {
        ret = 0;
      }
    }
  }

  size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
  for (size_t z = 0; z < num_assists; z++) {
    auto eff = this->assist_server->get_active_assist_by_index(z);
    if (eff == AssistEffect::LAND_PRICE) {
      // In NTE, Land Price is apparently 2x rather than 1.5x
      ret = is_nte ? (ret << 1) : (ret + (ret >> 1));
    } else if (eff == AssistEffect::DEFLATION) {
      ret = max<int32_t>(0, ret - 1);
    } else if (eff == AssistEffect::INFLATION) {
      ret++;
    }
  }

  return ret;
}

const CardShortStatus* RulerServer::short_status_for_card_ref(uint16_t card_ref) const {
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (client_id != 0xFF) {
    for (size_t z = 0; z < 16; z++) {
      const auto* stat = &this->short_statuses[client_id]->at(z);
      if (stat->card_ref == card_ref) {
        return stat;
      }
    }
  }
  return nullptr;
}

bool RulerServer::should_allow_attacks_on_current_turn() const {
  return (this->state_flags &&
      ((this->state_flags->turn_num > 1) ||
          (this->state_flags->current_team_turn1 != this->state_flags->first_team_turn)));
}

int32_t RulerServer::verify_deck(
    const parray<le_uint16_t, 0x1F>& card_ids,
    const parray<uint8_t, 0x2F0>* owned_card_counts) const {
  for (size_t z = 0; z < card_ids.size(); z++) {
    if (!this->definition_for_card_id(card_ids.at(z))) {
      return -0x7C;
    }
  }

  auto sc_card_ce = this->definition_for_card_id(card_ids.at(0));
  if (!sc_card_ce) {
    return -0x80;
  }

  bool is_arkz_sc;
  if (sc_card_ce->def.type == CardType::ARKZ_SC) {
    is_arkz_sc = true;
  } else if (sc_card_ce->def.type == CardType::HUNTERS_SC) {
    is_arkz_sc = false;
  } else {
    return -0x80;
  }

  for (size_t z = 1; z < card_ids.size(); z++) {
    ssize_t count = 0;
    for (size_t w = 1; w < card_ids.size(); w++) {
      if (card_ids.at(z) == card_ids.at(w)) {
        count++;
      }
    }
    if (count > 3) {
      return -0x7F;
    }

    if (owned_card_counts && (owned_card_counts->at(card_ids[z]) < count)) {
      return -0x7B;
    }

    auto ce = this->definition_for_card_id(card_ids[z]);
    if (!ce) {
      return -0x7A;
    }

    if ((ce->def.type == CardType::HUNTERS_SC) || (ce->def.type == CardType::ARKZ_SC)) {
      return -0x7A;
    } else if ((ce->def.type == CardType::ITEM) && is_arkz_sc) {
      return -0x7E;
    } else if ((ce->def.type == CardType::CREATURE) && !is_arkz_sc) {
      return -0x7D;
    }
  }

  return 0;
}

size_t RulerServer::count_targets_with_rampage_and_not_pierce_nte(const ActionState& as) const {
  size_t ret = 0;
  for (size_t z = 0; (z < as.target_card_refs.size()) && (as.target_card_refs[z] != 0xFFFF); z++) {
    if (this->attack_action_has_rampage_and_not_pierce(as, as.target_card_refs[z])) {
      ret++;
    }
  }
  return ret;
}

size_t RulerServer::count_targets_with_pierce_and_not_rampage_nte(const ActionState& as) const {
  size_t ret = 0;
  for (size_t z = 0; (z < as.target_card_refs.size()) && (as.target_card_refs[z] != 0xFFFF); z++) {
    if (this->attack_action_has_pierce_and_not_rampage(as, client_id_for_card_ref(as.target_card_refs[z]))) {
      ret++;
    }
  }
  return ret;
}

} // namespace Episode3
