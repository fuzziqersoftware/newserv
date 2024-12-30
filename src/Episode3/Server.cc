#include "Server.hh"

#include <phosg/Random.hh>
#include <phosg/Time.hh>

#include "../Loggers.hh"
#include "../Revision.hh"
#include "../SendCommands.hh"

using namespace std;

namespace Episode3 {

// This is (obviously) not the original string. The original string is:
// "03/05/29 18:00 by K.Toya" (NTE)
// "[V1][FINAL2.0] 03/09/13 15:30 by K.Toya" (Final)
static const char* VERSION_SIGNATURE =
    "newserv Ep3 based on [V1][FINAL2.0] 03/09/13 15:30 by K.Toya";
static const char* VERSION_SIGNATURE_NTE =
    "newserv Ep3 NTE based on 03/05/29 18:00 by K.Toya";

Server::PresenceEntry::PresenceEntry() {
  this->clear();
}

void Server::PresenceEntry::clear() {
  this->player_present = 0;
  this->deck_valid = 0;
  this->is_cpu_player = 0;
}

Server::Server(shared_ptr<Lobby> lobby, Options&& options)
    : lobby(lobby),
      battle_record(lobby ? lobby->battle_record : nullptr),
      has_lobby(lobby != nullptr),
      options(std::move(options)),
      last_chosen_map(this->options.tournament ? this->options.tournament->get_map() : nullptr),
      tournament_match_result_sent(false),
      override_environment_number(0xFF),
      def_dice_value_range_override(0xFF),
      atk_dice_value_range_2v1_override(0xFF),
      def_dice_value_range_2v1_override(0xFF),
      battle_finished(false),
      battle_in_progress(false),
      round_num(1),
      battle_phase(BattlePhase::INVALID_00),
      first_team_turn(0xFF),
      current_team_turn1(0xFF),
      setup_phase(SetupPhase::REGISTRATION),
      registration_phase(RegistrationPhase::AWAITING_NUM_PLAYERS),
      action_subphase(ActionSubphase::ATTACK),
      current_team_turn2(0xFF),
      num_pending_attacks(0),
      client_done_enqueuing_attacks(false),
      player_ready_to_end_phase(false),
      unknown_a10(0),
      overall_time_expired(false),
      battle_start_usecs(0),
      should_copy_prev_states_to_current_states(0),
      card_special(nullptr),
      clients_done_in_mulligan_phase(false),
      num_pending_attacks_with_cards(0),
      unknown_a14(0),
      unknown_a15(0),
      defense_list_ended_for_client(false),
      next_assist_card_set_number(1),
      team_exp(0),
      team_dice_bonus(0),
      team_client_count(0),
      team_num_ally_fcs_destroyed(0),
      team_num_cards_destroyed(0),
      num_trap_tiles_of_type(0),
      chosen_trap_tile_index_of_type(0),
      has_done_pb(0),
      num_6xB4x06_commands_sent(0),
      prev_num_6xB4x06_commands_sent(0) {
  if (this->has_lobby) {
    new StackLogger(this, lobby->log.prefix + "[Ep3::Server] ", lobby->log.min_level);
  } else {
    new StackLogger(this, "[Ep3::Server] ", lobby_log.min_level);
  }
}

Server::~Server() noexcept(false) {
  if (this->logger_stack.size() != 1) {
    throw logic_error(phosg::string_printf("incorrect logger stack size: expected 1, received %zu", this->logger_stack.size()));
  }
  delete this->logger_stack.back();
}

void Server::init() {
  this->map_and_rules = make_shared<MapAndRulesState>();
  this->num_clients_present = 0;
  this->overlay_state.clear();
  for (size_t z = 0; z < 4; z++) {
    this->presence_entries[z].clear();
    this->deck_entries[z] = make_shared<DeckEntry>();
    this->name_entries[z].clear();
    this->name_entries_valid[z] = false;
  }

  this->card_special = make_shared<CardSpecial>(this->shared_from_this());

  // Note: The original implementation calls the default PSOV2Encryption
  // constructor for random_crypt, which just uses 0 as the seed. It then
  // re-seeds the generator later. We instead expect the caller to provide a
  // seeded generator, and we don't re-seed it at all.
  // this->random_crypt = make_shared<PSOV2Encryption>(0);

  this->state_flags = make_shared<StateFlags>();

  this->clear_player_flags_after_dice_phase();

  this->update_battle_state_flags_and_send_6xB4x03_if_needed();

  this->assist_server = make_shared<AssistServer>(this->shared_from_this());
  this->ruler_server = make_shared<RulerServer>(this->shared_from_this());
  this->ruler_server->link_objects(this->map_and_rules, this->state_flags, this->assist_server);

  this->send_6xB4x46();
}

Server::StackLogger::StackLogger(const Server* s, const std::string& prefix)
    : PrefixedLogger(s->logger_stack.back()->prefix + prefix, s->logger_stack.back()->min_level),
      server(s) {
  s->logger_stack.push_back(this);
}

Server::StackLogger::StackLogger(const Server* s, const std::string& prefix, phosg::LogLevel min_level)
    : PrefixedLogger(prefix, min_level),
      server(s) {
  s->logger_stack.push_back(this);
}

Server::StackLogger::StackLogger(StackLogger&& other)
    : PrefixedLogger(std::move(other)),
      server(other.server) {
  if (this->server->logger_stack.back() != &other) {
    throw logic_error("cannot move StackLogger unless it is the last one");
  }
  this->server->logger_stack.back() = this;
}

Server::StackLogger& Server::StackLogger::operator=(StackLogger&& other) {
  this->PrefixedLogger::operator=(std::move(other));
  this->server = other.server;
  if (this->server->logger_stack.back() != &other) {
    throw logic_error("cannot move StackLogger unless it is the last one");
  }
  this->server->logger_stack.back() = this;
  return *this;
}

Server::StackLogger::~StackLogger() noexcept(false) {
  if (this->server->logger_stack.back() != this) {
    throw logic_error("incorrect logger stack unwind order");
  }
  this->server->logger_stack.pop_back();
}

Server::StackLogger Server::log_stack(const std::string& prefix) const {
  return StackLogger(this, prefix);
}

const Server::StackLogger& Server::log() const {
  return *this->logger_stack.back();
}

std::string Server::debug_str_for_card_ref(uint16_t card_ref) const {
  if (card_ref == 0xFFFF) {
    return "@FFFF";
  }
  auto ce = this->definition_for_card_ref(card_ref);
  if (ce) {
    string name = ce->def.en_name.decode();
    return phosg::string_printf("@%04hX (#%04" PRIX32 " %s)", card_ref, ce->def.card_id.load(), name.c_str());
  } else {
    return phosg::string_printf("@%04hX (missing)", card_ref);
  }
}

std::string Server::debug_str_for_card_id(uint16_t card_id) const {
  if (card_id == 0xFFFF) {
    return "#FFFF";
  }
  auto ce = this->definition_for_card_id(card_id);
  if (ce) {
    string name = ce->def.en_name.decode();
    return phosg::string_printf("#%04hX (%s)", card_id, name.c_str());
  } else {
    return phosg::string_printf("#%04hX (missing)", card_id);
  }
}

int8_t Server::get_winner_team_id() const {
  // Note: This function is not part of the original implementation.

  parray<size_t, 2> team_player_counts(0);
  parray<size_t, 2> team_win_flag_counts(0);
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (!ps) {
      continue;
    }
    uint8_t team_id = ps->get_team_id();
    team_player_counts[team_id]++;
    if (ps->assist_flags & AssistFlag::HAS_WON_BATTLE) {
      team_win_flag_counts[team_id]++;
    }
  }

  if (!team_player_counts[0] || !team_player_counts[1]) {
    throw logic_error("at least one team has no players");
  }
  if (team_win_flag_counts[0] && team_win_flag_counts[1]) {
    throw logic_error("both teams have winning players");
  }
  for (int8_t z = 0; z < 2; z++) {
    if (!team_win_flag_counts[z]) {
      continue;
    }
    if (team_win_flag_counts[z] != team_player_counts[z]) {
      throw logic_error("only some players on team have won");
    }
    return z;
  }
  return -1; // No team has won (yet)
}

void Server::send(const void* data, size_t size, uint8_t command, bool enable_masking) const {
  // Note: This function is (obviously) not part of the original implementation.
  if (this->has_lobby) {
    auto l = this->lobby.lock();
    if (!l) {
      throw runtime_error("lobby is deleted");
    }

    string masked_data;
    if (enable_masking &&
        !this->options.is_nte() &&
        !(this->options.behavior_flags & BehaviorFlag::DISABLE_MASKING) &&
        (size >= 8)) {
      masked_data.assign(reinterpret_cast<const char*>(data), size);
      uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
      set_mask_for_ep3_game_command(masked_data.data(), masked_data.size(), mask_key);
      data = masked_data.data();
      size = masked_data.size();
    }

    // Note: Sega's servers sent battle commands with the 60 command. The handlers
    // for 60, 62, and C9 on the client are identical, so we choose to use C9
    // instead because it's unique to Episode 3, and therefore seems more
    // appropriate to convey battle commands.
    send_command(l, command, 0x00, data, size);
    for (auto watcher_l : l->watcher_lobbies) {
      send_command_if_not_loading(watcher_l, command, 0x00, data, size);
    }
    if (this->battle_record && this->battle_record->writable()) {
      this->battle_record->add_command(BattleRecord::Event::Type::BATTLE_COMMAND, data, size);
    }

  } else if ((this->options.behavior_flags & BehaviorFlag::LOG_COMMANDS_IF_LOBBY_MISSING) &&
      this->log().info("Generated command")) {
    phosg::print_data(stderr, data, size, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);
  }
}

void Server::send_6xB4x46() const {
  // Note: This function is not part of the original implementation; it was
  // factored out from its callsites in this file and the strings were changed.

  // NTE doesn't have the date_str2 field, but we send it anyway to make
  // debugging easier.
  G_ServerVersionStrings_Ep3_6xB4x46 cmd;
  cmd.version_signature.encode(this->options.is_nte() ? VERSION_SIGNATURE_NTE : VERSION_SIGNATURE, 1);
  cmd.date_str1.encode(phosg::format_time(this->options.card_index->definitions_mtime() * 1000000), 1);
  string build_date = phosg::format_time(BUILD_TIMESTAMP);
  cmd.date_str2.encode(phosg::string_printf("newserv %s compiled at %s", GIT_REVISION_HASH, build_date.c_str()), 1);
  this->send(cmd);
}

string Server::prepare_6xB6x41_map_definition(shared_ptr<const MapIndex::Map> map, uint8_t language, bool is_nte) {
  auto vm = map->version(language);

  const auto& compressed = vm->compressed(is_nte);

  phosg::StringWriter w;
  uint32_t subcommand_size = (compressed.size() + sizeof(G_MapData_Ep3_6xB6x41) + 3) & (~3);
  w.put<G_MapData_Ep3_6xB6x41>({{{{0xB6, 0, 0}, subcommand_size}, 0x41, {}}, vm->map->map_number.load(), compressed.size(), 0});
  w.write(compressed);
  return std::move(w.str());
}

void Server::send_commands_for_joining_spectator(Channel& ch) const {
  bool should_send_state = true;
  if (this->setup_phase == SetupPhase::REGISTRATION) {
    // If registration is still in progress, we only need to send the map data
    // (if a map is even chosen yet)
    if ((this->registration_phase != RegistrationPhase::REGISTERED) &&
        (this->registration_phase != RegistrationPhase::BATTLE_STARTED)) {
      should_send_state = false;
    }
  }

  if (this->last_chosen_map) {
    string data = this->prepare_6xB6x41_map_definition(this->last_chosen_map, ch.language, this->options.is_nte());
    this->log().info("Sending %c version of map %08" PRIX32, char_for_language_code(ch.language), this->last_chosen_map->map_number);
    ch.send(0x6C, 0x00, data);
  }

  if (should_send_state) {
    ch.send(0xC9, 0x00, this->prepare_6xB4x03());
    for (uint8_t client_id = 0; client_id < 4; client_id++) {
      auto ps = this->player_states[client_id];
      if (ps) {
        ch.send(0xC9, 0x00, ps->prepare_6xB4x02());
        ch.send(0xC9, 0x00, ps->prepare_6xB4x04());
      }
    }
    if (ch.version == Version::GC_EP3_NTE) {
      G_UpdateMap_Ep3NTE_6xB4x05 cmd;
      cmd.state = *this->map_and_rules;
      ch.send(0xC9, 0x00, cmd);
    } else {
      G_UpdateMap_Ep3_6xB4x05 cmd;
      cmd.state = *this->map_and_rules;
      ch.send(0xC9, 0x00, cmd);
    }
    // TODO: Sega does something like this; do we have to do this too?
    // for (uint8_t client_id = 0; client_id < 4; client_id++) {
    //   (send 6xB4x4E, 6xB4x4C, 6xB4x4D for each set card)
    //   (send 6xB4x4F for client_id)
    // }
    ch.send(0xC9, 0x00, this->prepare_6xB4x07_decks_update());
    // TODO: Sega sends 6xB4x05 here again; why? Is that necessary? They also
    // send 6xB4x02 again for each player after that (but not 6xB4x04)
    ch.send(0xC9, 0x00, this->prepare_6xB4x1C_names_update());
    ch.send(0xC9, 0x00, this->prepare_6xB4x50_trap_tile_locations());
    {
      G_LoadCurrentEnvironment_Ep3_6xB4x3B cmd_3B;
      ch.send(0xC9, 0x00, &cmd_3B, sizeof(cmd_3B));
    }
  }
}

__attribute__((format(printf, 2, 3))) void Server::send_debug_message_printf(const char* fmt, ...) const {
  auto l = this->lobby.lock();
  if (l && (this->options.behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES)) {
    va_list va;
    va_start(va, fmt);
    std::string buf = phosg::string_vprintf(fmt, va);
    va_end(va);
    send_text_message(l, buf);
  }
}

__attribute__((format(printf, 2, 3))) void Server::send_info_message_printf(const char* fmt, ...) const {
  auto l = this->lobby.lock();
  if (l) {
    va_list va;
    va_start(va, fmt);
    std::string buf = phosg::string_vprintf(fmt, va);
    va_end(va);
    send_text_message(l, buf);
  }
}

void Server::send_debug_command_received_message(
    uint8_t client_id, uint8_t subsubcommand, const char* description) const {
  this->log().debug("%hhu/CAx%02hhX %s", client_id, subsubcommand, description);
  this->send_debug_message_printf("$C5%hhu/CAx%02hhX %s", client_id, subsubcommand, description);
}

void Server::send_debug_command_received_message(uint8_t subsubcommand, const char* description) const {
  this->log().debug("*/CAx%02hhX %s", subsubcommand, description);
  this->send_debug_message_printf("$C5*/CAx%02hhX %s", subsubcommand, description);
}

void Server::send_debug_message_if_error_code_nonzero(uint8_t client_id, int32_t error_code) const {
  if (error_code < 0) {
    this->send_debug_message_printf("$C4%hhu/ERROR -0x%zX", client_id, static_cast<ssize_t>(-error_code));
  } else if (error_code > 0) {
    this->send_debug_message_printf("$C4%hhu/ERROR 0x%zX", client_id, static_cast<ssize_t>(error_code));
  }
}

void Server::add_team_exp(uint8_t team_id, int32_t exp) {
  size_t num_assists = this->assist_server->compute_num_assist_effects_for_team(team_id);
  for (size_t z = 0; z < num_assists; z++) {
    if (this->assist_server->get_active_assist_by_index(z) == AssistEffect::GOLD_RUSH) {
      exp += (exp / 2);
    }
  }

  this->team_exp[team_id] = clamp<int16_t>(this->team_exp[team_id] + exp, 0, this->team_client_count[team_id] * 96);

  uint8_t dice_boost = this->team_exp[team_id] / (this->team_client_count[team_id] * 12);
  this->card_special->adjust_dice_boost_if_team_has_condition_52(team_id, &dice_boost, 0);
  this->team_dice_bonus[team_id] = min<uint8_t>(dice_boost, 8);
}

bool Server::advance_battle_phase() {
  switch (this->battle_phase) {
    case BattlePhase::DICE:
      this->dice_phase_after();
      this->set_phase_before();
      break;
    case BattlePhase::SET:
      this->set_phase_after();
      this->move_phase_before();
      break;
    case BattlePhase::MOVE:
      this->move_phase_after();
      this->action_phase_before();
      break;
    case BattlePhase::ACTION:
      this->action_phase_after();
      this->draw_phase_before();
      break;
    case BattlePhase::DRAW:
      this->draw_phase_after();
      this->dice_phase_before();
      break;
    default:
      throw logic_error("invalid battle phase");
  }
  return this->check_for_battle_end();
}

void Server::action_phase_after() {
  this->send_6xB4x02_for_all_players_if_needed();
  this->battle_phase = BattlePhase::DRAW;
}

void Server::draw_phase_before() {
  for (size_t z = 0; z < 4; z++) {
    if (this->player_states[z]) {
      this->player_states[z]->draw_phase_before();
    }
  }
}

shared_ptr<const CardIndex::CardEntry> Server::definition_for_card_ref(uint16_t card_ref) const {
  try {
    return this->options.card_index->definition_for_id(this->card_id_for_card_ref(card_ref));
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<Card> Server::card_for_set_card_ref(uint16_t card_ref) {
  if (card_ref == 0xFFFF) {
    return nullptr;
  }
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (client_id == 0xFF) {
    return nullptr;
  }
  auto ps = this->player_states.at(client_id);
  if (!ps) {
    return nullptr;
  }
  auto card = ps->get_sc_card();
  if (card && (card->get_card_ref() == card_ref)) {
    return card;
  }
  for (size_t set_index = 0; set_index < 8; set_index++) {
    card = ps->get_set_card(set_index);
    if (card && (card->get_card_ref() == card_ref)) {
      return card;
    }
  }
  return nullptr;
}

shared_ptr<const Card> Server::card_for_set_card_ref(uint16_t card_ref) const {
  return const_cast<Server*>(this)->card_for_set_card_ref(card_ref);
}

uint16_t Server::card_id_for_card_ref(uint16_t card_ref) const {
  uint8_t client_id = client_id_for_card_ref(card_ref);
  if (client_id != 0xFF) {
    auto ps = this->player_states.at(client_id);
    if (!ps) {
      return 0xFFFF;
    }
    auto deck = ps->get_deck();
    if (deck) {
      return deck->card_id_for_card_ref(card_ref);
    }
  }
  return 0xFFFF;
}

bool Server::card_ref_is_empty_or_has_valid_card_id(uint16_t card_ref) const {
  if (card_ref == 0xFFFF) {
    return true;
  } else {
    return this->card_id_for_card_ref(card_ref) != 0xFFFF;
  }
}

bool Server::check_for_battle_end() {
  bool ret = false;
  if (this->map_and_rules->rules.hp_type == HPType::DEFEAT_TEAM) {
    bool teams_defeated[2] = {true, true};
    for (size_t client_id = 0; client_id < 4; client_id++) {
      auto ps = this->player_states[client_id];
      if (!ps) {
        continue;
      }
      auto sc_card = ps->get_sc_card();
      if (sc_card && !sc_card->check_card_flag(2)) {
        teams_defeated[ps->get_team_id()] = false;
      }
    }

    if (this->options.is_nte()) {
      if (teams_defeated[0] || teams_defeated[1]) {
        ret = true;
        for (size_t client_id = 0; client_id < 4; client_id++) {
          auto ps = this->player_states[client_id];
          if (ps && teams_defeated[ps->get_team_id()] == 0) {
            ps->assist_flags |= AssistFlag::HAS_WON_BATTLE;
          }
        }
      }
    } else if (!teams_defeated[0] || !teams_defeated[1]) {
      if (teams_defeated[0] || teams_defeated[1]) {
        ret = true;
        for (size_t client_id = 0; client_id < 4; client_id++) {
          auto ps = this->player_states[client_id];
          if (ps) {
            ps->assist_flags &= ~(AssistFlag::HAS_WON_BATTLE |
                AssistFlag::WINNER_DECIDED_BY_DEFEAT |
                AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
            if (teams_defeated[ps->get_team_id()] == 0) {
              ps->assist_flags |= AssistFlag::HAS_WON_BATTLE;
            }
          }
        }
      }
    } else { // Both teams defeated?? I guess this is technically possible
      ret = true;
      this->compute_losing_team_id_and_add_winner_flags(AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
    }

  } else { // Not DEFEAT_TEAM

    if (this->options.is_nte()) {
      uint8_t loser_team_id = 0;
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps && ps->get_sc_card() && ps->get_sc_card()->check_card_flag(2)) {
          ret = true;
          loser_team_id = ps->get_team_id();
          break;
        }
      }
      if (ret) {
        for (size_t z = 0; z < 4; z++) {
          auto ps = this->player_states[z];
          if (ps && (ps->get_team_id() != loser_team_id)) {
            ps->assist_flags |= AssistFlag::HAS_WON_BATTLE;
          }
        }
      }

    } else {
      bool teams_alive[2] = {false, false};
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto ps = this->player_states[client_id];
        if (!ps) {
          continue;
        }
        auto sc_card = ps->get_sc_card();
        if (sc_card && sc_card->check_card_flag(2)) {
          teams_alive[ps->get_team_id()] = true;
        }
      }

      if (!teams_alive[0] || !teams_alive[1]) {
        if (teams_alive[0] || teams_alive[1]) {
          ret = true;
          for (size_t client_id = 0; client_id < 4; client_id++) {
            auto ps = this->player_states[client_id];
            if (ps) {
              ps->assist_flags &= ~(AssistFlag::HAS_WON_BATTLE |
                  AssistFlag::WINNER_DECIDED_BY_DEFEAT |
                  AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
              if (!teams_alive[ps->get_team_id()]) {
                ps->assist_flags |= AssistFlag::HAS_WON_BATTLE;
              }
            }
          }
        }
      } else {
        ret = true;
        this->compute_losing_team_id_and_add_winner_flags(AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
      }
    }
  }

  if (ret) {
    this->set_battle_ended();
  }
  return ret;
}

void Server::force_replace_assist_card(uint8_t client_id, uint16_t card_id) {
  auto ps = this->player_states.at(client_id);
  if (!ps) {
    throw runtime_error("player does not exist");
  }
  ps->replace_assist_card_by_id(card_id);
}

void Server::force_destroy_field_character(uint8_t client_id, size_t visible_index) {
  auto ps = this->player_states.at(client_id);
  if (!ps) {
    throw runtime_error("player does not exist");
  }

  // TODO: Is it possible for there to be gaps in the set cards array? If not,
  // we could just do a direct array lookup here instead of this loop
  shared_ptr<Card> set_card = nullptr;
  for (size_t set_index = 0; set_index < 8; set_index++) {
    if (!ps->set_cards[set_index]) {
      continue;
    }
    if (visible_index == 0) {
      set_card = ps->set_cards[set_index];
      break;
    } else {
      visible_index--;
    }
  }

  if (set_card) {
    set_card->card_flags |= 2;
    this->check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
    this->check_for_battle_end();
  }
}

void Server::force_battle_result(uint8_t specified_client_id, bool set_winner) {
  auto specified_ps = this->player_states.at(specified_client_id);
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->assist_flags &= ~(AssistFlag::HAS_WON_BATTLE |
          AssistFlag::WINNER_DECIDED_BY_DEFEAT |
          AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
      if ((ps->get_team_id() == specified_ps->get_team_id()) == set_winner) {
        ps->assist_flags |= AssistFlag::HAS_WON_BATTLE;
      }
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed(true);
    }
  }
  this->set_battle_ended();
}

void Server::check_for_destroyed_cards_and_send_6xB4x05_6xB4x02() {
  for (size_t z = 0; z < 4; z++) {
    if (this->player_states[z]) {
      this->player_states[z]->on_cards_destroyed();
    }
  }
  this->send_6xB4x05();
  this->send_6xB4x02_for_all_players_if_needed();
}

bool Server::check_presence_entry(uint8_t client_id) const {
  return (client_id < 4)
      ? this->presence_entries[client_id].player_present
      : false;
}

void Server::clear_player_flags_after_dice_phase() {
  this->player_ready_to_end_phase.clear(0);
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->assist_flags &= ~(AssistFlag::READY_TO_END_PHASE | AssistFlag::READY_TO_END_ACTION_PHASE);
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    }
  }
}

void Server::compute_all_map_occupied_bits() {
  for (size_t y = 0; y < 0x10; y++) {
    for (size_t x = 0; x < 0x10; x++) {
      this->map_and_rules->clear_occupied_bit_for_tile(x, y);
    }
  }
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->set_map_occupied_bits_for_sc_and_creatures();
    }
  }
}

void Server::compute_team_dice_bonus(uint8_t team_id) {
  this->team_dice_bonus[team_id] = clamp<int16_t>(
      this->team_exp[team_id] / (this->team_client_count[team_id] * 12), 0, 8);
}

void Server::copy_player_states_to_prev_states() {
  if (this->should_copy_prev_states_to_current_states != 1) {
    this->should_copy_prev_states_to_current_states = 1;
    this->num_6xB4x06_commands_sent = 0;
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        ps->prev_set_card_action_chains = *ps->set_card_action_chains;
        ps->prev_set_card_action_metadatas = *ps->set_card_action_metadatas;
        ps->prev_card_short_statuses = *ps->card_short_statuses;
      }
    }
  }
}

shared_ptr<const CardIndex::CardEntry> Server::definition_for_card_id(uint16_t card_id) const {
  try {
    return this->options.card_index->definition_for_id(card_id);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

void Server::destroy_cards_with_zero_hp() {
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps) {
      bool any_card_destroyed = false;
      for (ssize_t set_index = -1; set_index < 8; set_index++) {
        auto card = (set_index < 0) ? ps->get_sc_card() : ps->get_set_card(set_index);
        if (card && !(card->card_flags & 2) && (card->get_current_hp() < 1)) {
          card->destroy_set_card(this->options.is_nte() ? nullptr : card->w_destroyer_sc_card.lock());
          any_card_destroyed = true;
        }
      }
      if (any_card_destroyed) {
        ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
    }
  }
}

void Server::determine_first_team_turn() {
  this->team_client_count[0] = this->map_and_rules->num_team0_players;
  this->team_client_count[1] = this->map_and_rules->num_players - this->team_client_count[0];
  if (this->team_client_count[0] == 0 || this->team_client_count[1] == 0) {
    throw runtime_error("one or both teams have no players");
  }
  this->first_team_turn = 0xFF;
  while (this->first_team_turn == 0xFF) {
    uint8_t results[2] = {0, 0};
    for (size_t client_id = 0; client_id < 4; client_id++) {
      auto ps = this->player_states[client_id];
      if (ps) {
        results[ps->get_team_id()] += ps->roll_dice(1);
      }
    }
    // Handle unbalanced team sizes by weighting the results by the other team's
    // player count
    results[0] *= this->team_client_count[1];
    results[1] *= this->team_client_count[0];
    if (results[1] < results[0]) {
      this->first_team_turn = 0;
    } else if (results[0] < results[1]) {
      this->first_team_turn = 1;
    }
  }
  this->current_team_turn1 = this->first_team_turn;
  this->current_team_turn2 = this->current_team_turn1;
}

void Server::dice_phase_after() {
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (!ps) {
      continue;
    }
    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    for (size_t z = 0; z < num_assists; z++) {
      auto eff = this->assist_server->get_active_assist_by_index(z);
      if ((eff == AssistEffect::CHARITY) || (!this->options.is_nte() && (eff == AssistEffect::CHARITY_PLUS))) {
        int16_t exp_delta = (eff == AssistEffect::CHARITY_PLUS) ? -1 : 1;
        for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
          auto other_ps = this->player_states[other_client_id];
          if (other_ps && this->current_team_turn2 == other_ps->get_team_id()) {
            for (size_t die_index = 0; die_index < 2; die_index++) {
              if (other_ps->get_dice_result(die_index) >= 5) {
                this->add_team_exp(ps->get_team_id(), exp_delta);
              }
            }
          }
        }
        this->update_battle_state_flags_and_send_6xB4x03_if_needed();
      }
    }
  }
  this->battle_phase = BattlePhase::SET;
}

void Server::set_phase_before() {
  for (size_t z = 0; z < 4; z++) {
    if (this->player_states[z]) {
      this->player_states[z]->handle_before_turn_assist_effects();
    }
  }
  this->check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
}

void Server::draw_phase_after() {
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps) {
      if (ps->draw_cards_allowed()) {
        ps->draw_hand(0);
      }
      if (ps->is_team_turn()) {
        ps->compute_team_dice_bonus_after_draw_phase();
      }
    }
  }

  this->check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
  this->battle_phase = BattlePhase::DICE;
  this->current_team_turn1 ^= 1;
  this->round_num++;

  if (this->current_team_turn1 == this->first_team_turn) {
    if (this->map_and_rules->rules.overall_time_limit > 0) {
      // Battle time limits are specified in increments of 5 minutes.
      // Note: This part is not based on the original code because the timing
      // facilities used are different.
      uint64_t limit_5mins = this->map_and_rules->rules.overall_time_limit;
      uint64_t end_usecs = this->battle_start_usecs + (limit_5mins * 300 * 1000 * 1000);
      if (phosg::now() >= end_usecs) {
        this->overall_time_expired = true;
      }
    }

    // Apparently the hard limit of 1000 was added after NTE was released
    if (this->overall_time_expired || (!this->options.is_nte() && (this->round_num >= 1000))) {
      bool no_winner_specified = true;
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps && (ps->assist_flags & AssistFlag::HAS_WON_BATTLE)) {
          no_winner_specified = false;
          break;
        }
      }
      if (no_winner_specified) {
        this->compute_losing_team_id_and_add_winner_flags(0);
      }
      this->round_num--;
      this->set_battle_ended();
    }
  }
}

void Server::dice_phase_before() {
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->dice_phase_before();
    }
    this->client_done_enqueuing_attacks[z] = 0;
  }
  this->destroy_cards_with_zero_hp();
  this->check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
  this->check_for_battle_end();
  this->send_6xB4x02_for_all_players_if_needed();
  this->action_subphase = ActionSubphase::ATTACK;
  this->current_team_turn2 = this->current_team_turn1;
  this->num_pending_attacks = 0;
  this->num_pending_attacks_with_cards = 0;
  this->unknown_a14 = 0;
  this->update_battle_state_flags_and_send_6xB4x03_if_needed();
}

void Server::end_attack_list_for_client(uint8_t client_id) {
  if (client_id >= 4) {
    return;
  }

  auto ps = this->player_states.at(client_id);
  if (!ps) {
    return;
  }

  if (this->current_team_turn2 == ps->get_team_id()) {
    this->client_done_enqueuing_attacks[client_id] = true;
  }

  bool all_clients_done_enqueuing_attacks = true;
  for (size_t other_client_id = 0; other_client_id < 4; other_client_id++) {
    auto other_ps = this->player_states[other_client_id];
    if (!other_ps) {
      continue;
    }
    auto card = other_ps->get_sc_card();
    if (card &&
        !card->check_card_flag(2) &&
        (other_ps->get_team_id() == this->current_team_turn2) &&
        (this->client_done_enqueuing_attacks[other_client_id] == 0)) {
      all_clients_done_enqueuing_attacks = false;
    }
  }

  if (all_clients_done_enqueuing_attacks) {
    for (size_t z = 0; z < 4; z++) {
      auto other_ps = this->player_states[z];
      if (other_ps) {
        other_ps->assist_flags &= (~AssistFlag::READY_TO_END_ACTION_PHASE);
        other_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
    }
    this->end_action_phase();
    this->client_done_enqueuing_attacks.clear(false);

  } else {
    ps->assist_flags |= AssistFlag::READY_TO_END_ACTION_PHASE;
    ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  }
}

void Server::end_action_phase() {
  this->num_pending_attacks = 0;
  this->unknown_a15 = 1;
  // Annoyingly, this is the original logic. We use an enum because it appears
  // that this can only ever be 0 or 2, but we may have to delete the enum if
  // that turns out to be false.
  this->action_subphase = static_cast<ActionSubphase>(static_cast<uint8_t>(this->action_subphase) + 2);
  if (this->options.is_nte()) {
    this->unknown_8023EEF4();
    this->update_battle_state_flags_and_send_6xB4x03_if_needed(0);
    this->send_6xB4x02_for_all_players_if_needed();
  } else {
    this->copy_player_states_to_prev_states();
    this->unknown_8023EEF4();
    this->send_set_card_updates_and_6xB4x04_if_needed();
  }
}

bool Server::enqueue_attack_or_defense(uint8_t client_id, ActionState* pa) {
  auto log = this->log_stack("enqueue_attack_or_defense: ");
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    string s = pa->str(this->shared_from_this());
    log.debug("input: %s", s.c_str());
  }

  if (client_id >= 4) {
    this->ruler_server->error_code3 = -0x78;
    log.debug("failed: invalid client ID");
    return false;
  }

  auto ps = this->player_states[client_id];
  if (!ps) {
    this->ruler_server->error_code3 = -0x72;
    log.debug("failed: player not present");
    return false;
  }

  if (pa->action_card_refs[0] == 0xFFFF) {
    if (pa->defense_card_ref != 0xFFFF) {
      pa->action_card_refs[0] = pa->defense_card_ref;
      log.debug("moved defense card ref to action card ref 0");
    }
  } else {
    pa->defense_card_ref = pa->action_card_refs[0];
    log.debug("moved action card ref 0 to defense card ref");
  }

  if (!this->ruler_server->is_attack_or_defense_valid(*pa)) {
    log.debug("failed: attack or defense not valid");
    return false;
  }

  int16_t ally_atk_result = this->send_6xB4x33_remove_ally_atk_if_needed(*pa);
  if (ally_atk_result == 1) {
    log.debug("pending: need ally approval");
    return true;
  } else if (ally_atk_result == -1) {
    log.debug("failed: ally declined");
    return false;
  }

  if (this->num_pending_attacks >= 0x20) {
    this->ruler_server->error_code3 = -0x71;
    log.debug("failed: too many pending attacks");
    return false;
  }

  size_t attack_index = this->num_pending_attacks++;
  this->pending_attacks[attack_index] = *pa;
  if (log.should_log(phosg::LogLevel::DEBUG)) {
    string pa_str = this->pending_attacks[attack_index].str(this->shared_from_this());
    log.debug("set pending attack %zu: %s", attack_index, pa_str.c_str());
  }
  ps->set_action_cards_for_action_state(*pa);
  log.debug("set action cards");
  auto card = this->card_for_set_card_ref(this->send_6xB4x06_if_card_ref_invalid(pa->attacker_card_ref, 1));
  if (card) {
    card->card_flags |= 0x400;
    auto card_ps = card->player_state();
    if (card_ps) {
      card_ps->send_6xB4x04_if_needed();
    }
  }
  card = this->card_for_set_card_ref(this->send_6xB4x06_if_card_ref_invalid(
      pa->original_attacker_card_ref, 2));
  if (card) {
    card = this->card_for_set_card_ref(pa->target_card_refs[0]);
    if (card) {
      card->card_flags |= 0x800;
      card->player_state()->send_6xB4x04_if_needed();
    }
  }
  return true;
}

BattlePhase Server::get_battle_phase() const {
  return this->battle_phase;
}

ActionSubphase Server::get_current_action_subphase() const {
  return this->action_subphase;
}

uint8_t Server::get_current_team_turn() const {
  return this->current_team_turn1;
}

shared_ptr<PlayerState> Server::get_player_state(uint8_t client_id) {
  if (client_id >= 4) {
    return nullptr;
  }
  return this->player_states[client_id];
}

shared_ptr<const PlayerState> Server::get_player_state(uint8_t client_id) const {
  if (client_id >= 4) {
    return nullptr;
  }
  return this->player_states[client_id];
}

uint32_t Server::get_random_raw() {
  le_uint32_t ret;
  if (this->options.opt_rand_stream) {
    this->options.opt_rand_stream->readx(&ret, sizeof(ret));
  } else {
    ret = random_from_optional_crypt(this->options.opt_rand_crypt);
  }

  if (this->battle_record && this->battle_record->writable()) {
    this->battle_record->add_random_data(&ret, sizeof(ret));
  }
  return ret;
}

uint32_t Server::get_random(uint32_t max) {
  // The original implementation was essentially:
  // return (static_cast<double>(this->random_source->next() >> 16) / 65536.0) * max
  // This is unnecessarily complicated and imprecise, so we instead just do:
  return this->get_random_raw() % max;
}

float Server::get_random_float_0_1() {
  // This lacks some precision, but matches the original implementation.
  return (static_cast<double>(this->get_random_raw() >> 16) / 65536.0);
}

uint32_t Server::get_round_num() const {
  return this->round_num;
}

SetupPhase Server::get_setup_phase() const {
  return this->setup_phase;
}

uint32_t Server::get_should_copy_prev_states_to_current_states() const {
  return this->should_copy_prev_states_to_current_states;
}

bool Server::is_registration_complete() const {
  return this->setup_phase != SetupPhase::REGISTRATION;
}

void Server::move_phase_after() {
  if (!this->options.is_nte()) {
    for (size_t trap_type = 0; trap_type < 5; trap_type++) {
      uint8_t trap_tile_index = this->chosen_trap_tile_index_of_type[trap_type];
      if (trap_tile_index == 0xFF) {
        continue;
      }

      bool should_trigger = false;
      int16_t trap_x = this->trap_tile_locs[trap_type][trap_tile_index][0];
      int16_t trap_y = this->trap_tile_locs[trap_type][trap_tile_index][1];
      for (size_t client_id = 0; client_id < 4; client_id++) {
        auto ps = this->player_states[client_id];
        if (ps) {
          auto sc_card = ps->get_sc_card();
          if (sc_card && (sc_card->card_flags & 0x80) &&
              (sc_card->loc.x == trap_x) && (sc_card->loc.y == trap_y)) {
            should_trigger = true;
            break;
          }
        }
      }
      if (!should_trigger) {
        continue;
      }

      static const array<vector<uint16_t>, 5> default_trap_card_ids = {
          // Red: Dice Fever, Heavy Fog, Muscular, Immortality, Snail Pace
          vector<uint16_t>{0x00F7, 0x010F, 0x012E, 0x013B, 0x013C},
          // Blue: Gold Rush, Charity, Requiem
          vector<uint16_t>{0x0131, 0x012B, 0x0133},
          // Purple: Powerless Rain, Trash 1, Empty Hand, Skip Draw
          vector<uint16_t>{0x00FA, 0x0125, 0x0126, 0x0137},
          // Green: Brave Wind, Homesick, Fly
          vector<uint16_t>{0x00FB, 0x014E, 0x0107},
          // Yellow: Dice+1, Battle Royale, Reverse Card, Giant Garden, Fix
          vector<uint16_t>{0x00F6, 0x0242, 0x014B, 0x0145, 0x012D}};

      const vector<uint16_t>* trap_card_ids = &this->options.trap_card_ids.at(trap_type);
      if (trap_card_ids->empty()) {
        trap_card_ids = &default_trap_card_ids.at(trap_type);
      }

      // This is the original implementation. We do something smarter instead.
      // uint16_t trap_card_id = 0;
      // while (trap_card_id == 0) {
      //   trap_card_id = TRAP_CARD_IDS[trap_type][this->get_random(5)];
      // }
      uint16_t trap_card_id = 0xFFFF;
      if (trap_card_ids->size() == 1) {
        trap_card_id = trap_card_ids->at(0);
      } else if (trap_card_ids->size() > 1) {
        trap_card_id = trap_card_ids->at(this->get_random(trap_card_ids->size()));
      }

      if (trap_card_id != 0xFFFF) {
        for (size_t client_id = 0; client_id < 4; client_id++) {
          auto ps = this->player_states[client_id];
          if (ps) {
            auto sc_card = ps->get_sc_card();
            if (sc_card &&
                (abs(sc_card->loc.x - trap_x) < 2) &&
                (abs(sc_card->loc.y - trap_y) < 2) &&
                ps->replace_assist_card_by_id(trap_card_id)) {
              G_EnqueueAnimation_Ep3_6xB4x2C cmd;
              cmd.change_type = 0x01;
              cmd.client_id = client_id;
              cmd.card_refs.clear(0xFFFF);
              cmd.loc.x = trap_x;
              cmd.loc.y = trap_y;
              cmd.loc.direction = static_cast<Direction>(trap_type);
              cmd.unknown_a2[0] = trap_card_id;
              cmd.unknown_a2[1] = 0xFFFFFFFF;
              this->send(cmd);
            }
          }
        }
      }

      // Note: This is the original implementation:
      // if (this->num_trap_tiles_of_type[trap_type] > 1) {
      //   uint8_t new_index = this->chosen_trap_tile_index_of_type[trap_type];
      //   while (new_index == this->chosen_trap_tile_index_of_type[trap_type]) {
      //     new_index = this->get_random(this->num_trap_tiles_of_type[trap_type]);
      //   }
      //   this->chosen_trap_tile_index_of_type[trap_type] = new_index;
      //   this->send_6xB4x50();
      // }
      // We instead use an implementation that consumes a constant amount of
      // randomness per pass.
      if (this->num_trap_tiles_of_type[trap_type] == 2) {
        this->chosen_trap_tile_index_of_type[trap_type] ^= 1;
        this->send_6xB4x50_trap_tile_locations();
      } else if (this->num_trap_tiles_of_type[trap_type] > 2) {
        // Generate a new random index, but forbid it from matching the existing
        // index
        uint8_t new_index = this->get_random(this->num_trap_tiles_of_type[trap_type] - 1);
        if (new_index >= this->chosen_trap_tile_index_of_type[trap_type]) {
          new_index++;
        }
        this->chosen_trap_tile_index_of_type[trap_type] = new_index;
        this->send_6xB4x50_trap_tile_locations();
      }
    }
  }

  this->battle_phase = BattlePhase::ACTION;
}

void Server::action_phase_before() {
  this->unknown_a10 = 0;
  this->current_team_turn2 = this->current_team_turn1;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps) {
      ps->action_phase_before();
    }
    this->has_done_pb[client_id] = false;
  }
}

G_SetPlayerNames_Ep3_6xB4x1C Server::prepare_6xB4x1C_names_update() const {
  G_SetPlayerNames_Ep3_6xB4x1C cmd;
  cmd.entries = this->name_entries;
  return cmd;
}

void Server::send_6xB4x1C_names_update() {
  this->send(this->prepare_6xB4x1C_names_update());
}

int8_t Server::send_6xB4x33_remove_ally_atk_if_needed(const ActionState& pa) {
  G_SubtractAllyATKPoints_Ep3_6xB4x33 cmd;

  bool has_ally_cost = false;
  uint8_t ally_cost = 0;
  uint8_t setter_client_id = 0xFF;
  shared_ptr<PlayerState> setter_ps = nullptr;
  cmd.card_ref = 0xFFFF;
  for (size_t z = 0; (z < 8) && (pa.action_card_refs[z] != 0xFFFF); z++) {
    auto ce = this->definition_for_card_ref(pa.action_card_refs[z]);
    if (ce && (ce->def.ally_cost > 0)) {
      ally_cost = ce->def.ally_cost;
      has_ally_cost = true;
      setter_client_id = client_id_for_card_ref(pa.action_card_refs[z]);
      setter_ps = this->get_player_state(setter_client_id);
      cmd.card_ref = pa.action_card_refs[z];
      break;
    }
  }

  if (!has_ally_cost) {
    return 0;
  }

  if (!setter_ps) {
    this->ruler_server->error_code3 = -0x67;
    return 0;
  }

  bool ally_has_sufficient_atk = false;
  for (size_t z = 0; z < 4; z++) {
    auto ally_ps = this->get_player_state(z);
    if ((z != setter_client_id) && ally_ps) {
      if ((ally_ps->get_team_id() == setter_ps->get_team_id()) &&
          (ally_ps->get_atk_points() >= ally_cost)) {
        ally_has_sufficient_atk = true;
      }
    }
  }

  if (!ally_has_sufficient_atk) {
    this->ruler_server->error_code3 = -0x66;
    return -1;
  }

  this->pb_action_states[setter_client_id] = pa;
  this->has_done_pb[setter_client_id] = true;
  for (size_t z = 0; z < 4; z++) {
    this->has_done_pb_with_client[setter_client_id][z] = false;
  }

  cmd.client_id = setter_client_id;
  cmd.ally_cost = ally_cost;
  this->send(cmd);
  return 1;
}

G_UpdateDecks_Ep3_6xB4x07 Server::prepare_6xB4x07_decks_update() const {
  G_UpdateDecks_Ep3_6xB4x07 cmd07;
  for (size_t z = 0; z < 4; z++) {
    if (!this->check_presence_entry(z)) {
      cmd07.entries_present[z] = 0;
      cmd07.entries[z].clear();
      cmd07.entries[z].team_id = 0xFFFFFFFF;
    } else {
      cmd07.entries_present[z] = 1;
      cmd07.entries[z] = *this->deck_entries[z];
    }
  }
  return cmd07;
}

void Server::send_all_state_updates() {
  this->send(this->prepare_6xB4x07_decks_update());

  if (this->options.is_nte()) {
    G_UpdateMap_Ep3NTE_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    this->send(cmd);
  } else {
    G_UpdateMap_Ep3_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    this->send(cmd);
  }

  this->send_6xB4x02_for_all_players_if_needed();
}

void Server::send_set_card_updates_and_6xB4x04_if_needed() {
  if (this->should_copy_prev_states_to_current_states == 0) {
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        ps->send_set_card_updates();
        ps->send_6xB4x04_if_needed();
      }
    }
  } else {
    this->should_copy_prev_states_to_current_states = 0;
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        *ps->set_card_action_chains = ps->prev_set_card_action_chains;
        *ps->set_card_action_metadatas = ps->prev_set_card_action_metadatas;
        ps->send_set_card_updates();
        *ps->card_short_statuses = ps->prev_card_short_statuses;
        ps->send_6xB4x04_if_needed();
      }
    }
    this->num_6xB4x06_commands_sent = 0;
  }
}

void Server::set_battle_ended() {
  this->setup_phase = SetupPhase::BATTLE_ENDED;
  this->send_6xB4x39();
  this->update_battle_state_flags_and_send_6xB4x03_if_needed();
}

void Server::set_battle_started() {
  this->setup_phase = SetupPhase::MAIN_BATTLE;
  this->round_num = 1;
  this->battle_phase = BattlePhase::DICE;
  this->dice_phase_before();
  this->update_battle_state_flags_and_send_6xB4x03_if_needed();
  this->send_6xB4x02_for_all_players_if_needed();
  this->send_6xB4x05();
}

bool Server::player_can_receive_dice_boost(uint8_t client_id) const {
  auto ps = this->player_states[client_id];
  bool is_1p_2v1 = (this->team_client_count.at(ps->get_team_id()) < this->team_client_count[ps->get_team_id() ^ 1]);
  const auto& rules = this->map_and_rules->rules;
  return (rules.atk_dice_range(is_1p_2v1).second >= 3) || (rules.def_dice_range(is_1p_2v1).second >= 3);
}

void Server::set_client_id_ready_to_advance_phase(uint8_t client_id, BattlePhase battle_phase) {
  if (client_id >= 4) {
    return;
  }

  bool is_nte = this->options.is_nte();
  auto ps = this->player_states[client_id];
  if (ps &&
      (this->current_team_turn1 == ps->get_team_id()) &&
      (!is_nte || (this->battle_phase == battle_phase)) &&
      (this->setup_phase == SetupPhase::MAIN_BATTLE)) {
    ps->assist_flags |= AssistFlag::READY_TO_END_PHASE;
    ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    if (this->battle_phase == BattlePhase::DICE) {
      if (is_nte ||
          !(ps->assist_flags & AssistFlag::ELIGIBLE_FOR_DICE_BOOST) ||
          this->map_and_rules->rules.disable_dice_boost ||
          !this->player_can_receive_dice_boost(client_id)) {
        ps->assist_flags &= (~AssistFlag::ELIGIBLE_FOR_DICE_BOOST);
        ps->roll_main_dice_or_apply_after_effects();
        if (!is_nte && (ps->get_atk_points() < 3) && (ps->get_def_points() < 3)) {
          ps->assist_flags |= AssistFlag::ELIGIBLE_FOR_DICE_BOOST;
        }
      } else {
        // TODO: It'd be nice to do this in a constant-randomness way, but I'm
        // lazy, and this matches Sega's original implementation. The less-lazy
        // way to do it would be to roll three dice: one in the range [1, N],
        // one in the range [3, N], and one in the range [1, 2] to decide
        // whether to swap the first two results.
        for (size_t z = 0; z < 200; z++) {
          ps->roll_main_dice_or_apply_after_effects();
          if ((ps->get_atk_points() >= 3) || (ps->get_def_points() >= 3)) {
            break;
          }
        }
        ps->assist_flags &= (~AssistFlag::ELIGIBLE_FOR_DICE_BOOST);
      }
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    }
    this->player_ready_to_end_phase[client_id] = true;

    bool should_advance_phase = true;
    for (size_t z = 0; z < 4; z++) {
      auto other_ps = this->player_states[z];
      if (!other_ps) {
        continue;
      }
      auto sc_card = other_ps->get_sc_card();
      if (sc_card && !sc_card->check_card_flag(2) &&
          (this->current_team_turn1 == other_ps->get_team_id()) &&
          !this->player_ready_to_end_phase[z]) {
        should_advance_phase = false;
        break;
      }
    }

    if (should_advance_phase) {
      if (!this->options.is_nte()) {
        this->copy_player_states_to_prev_states();
      }
      this->advance_battle_phase();
      if (!this->options.is_nte()) {
        this->send_set_card_updates_and_6xB4x04_if_needed();
      }
      this->clear_player_flags_after_dice_phase();
      this->update_battle_state_flags_and_send_6xB4x03_if_needed();
      this->send_6xB4x39();
    }
  }
}

void Server::set_phase_after() {
  bool is_nte = this->options.is_nte();

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps) {
      auto card = ps->get_sc_card();
      if (card) {
        this->card_special->apply_action_conditions(EffectWhen::AFTER_SET_PHASE, nullptr, card, is_nte ? 0x1F : 0x04, nullptr);
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card) {
          this->card_special->apply_action_conditions(EffectWhen::AFTER_SET_PHASE, nullptr, card, is_nte ? 0x1F : 0x04, nullptr);
        }
      }
    }
  }

  this->send_6xB4x02_for_all_players_if_needed();

  bool clients_with_assist_vanish[4] = {false, false, false, false};
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (!ps) {
      continue;
    }
    size_t num_assists = this->assist_server->compute_num_assist_effects_for_client(client_id);
    for (size_t z = 0; z < num_assists; z++) {
      switch (this->assist_server->get_active_assist_by_index(z)) {
        case AssistEffect::SHUFFLE_ALL:
        case AssistEffect::SHUFFLE_GROUP:
          if (is_nte ||
              (!this->map_and_rules->rules.disable_deck_shuffle && !this->map_and_rules->rules.disable_deck_loop)) {
            ps->discard_and_redraw_hand();
          }
          break;
        case AssistEffect::TRASH_1:
          ps->discard_random_hand_card();
          break;
        case AssistEffect::EMPTY_HAND:
          ps->discard_all_attack_action_cards_from_hand();
          break;
        case AssistEffect::HITMAN:
          ps->discard_all_item_and_creature_cards_from_hand();
          break;
        case AssistEffect::ASSIST_TRASH:
          ps->discard_all_assist_cards_from_hand();
          break;
        case AssistEffect::ASSIST_VANISH:
          if (!is_nte) {
            clients_with_assist_vanish[client_id] = true;
          } else if (ps->get_assist_turns_remaining() != 90) {
            ps->discard_set_assist_card();
          }
          break;
        default:
          break;
      }
    }
  }

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps && clients_with_assist_vanish[client_id]) {
      ps->discard_set_assist_card();
    }
  }

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps &&
        (ps->get_assist_turns_remaining() == 90) &&
        (ps->assist_delay_turns < 1)) {
      ps->discard_set_assist_card();
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    }
  }

  this->battle_phase = BattlePhase::MOVE;
}

void Server::move_phase_before() {
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps) {
      ps->move_phase_before();
    }
  }
}

void Server::set_player_deck_valid(uint8_t client_id) {
  this->presence_entries[client_id].deck_valid = true;
}

void Server::setup_and_start_battle() {
  bool is_nte = this->options.is_nte();

  this->setup_phase = SetupPhase::STARTER_ROLLS;

  // Note: This is where original implementation re-seeds its random generator
  // (it uses time() as the seed value).

  for (size_t z = 0; z < 4; z++) {
    if (!this->check_presence_entry(z)) {
      if (!is_nte) {
        this->name_entries[z].clear();
      }
    } else {
      this->player_states[z] = make_shared<PlayerState>(z, this->shared_from_this());
      this->player_states[z]->init();
    }
  }

  if (!is_nte && (this->map_and_rules->rules.hp_type == HPType::COMMON_HP)) {
    int16_t team_hp[2] = {99, 99};
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (!ps) {
        continue;
      }
      auto card = ps->get_sc_card();
      if (card) {
        team_hp[ps->get_team_id()] = min<int16_t>(team_hp[ps->get_team_id()], card->get_current_hp());
      }
    }

    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (!ps) {
        continue;
      }
      auto card = ps->get_sc_card();
      if (card) {
        int16_t this_team_hp = team_hp[ps->get_team_id()];
        if (this_team_hp < 99) {
          card->set_current_and_max_hp(this_team_hp);
        }
      }
    }
  }

  this->map_and_rules->start_facing_directions = 0;
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->set_initial_location();
    }
  }

  this->determine_first_team_turn();
  this->compute_all_map_occupied_bits();

  for (size_t y = 0; y < 0x10; y++) {
    for (size_t x = 0; x < 0x10; x++) {
      if (this->map_and_rules->map.tiles[y][x] > 1) {
        this->map_and_rules->map.tiles[y][x] = 1;
      }
    }
  }

  // Non-NTE:
  //   this->__unused6__ = 0;
  // NTE:
  //   this->unknown_a1 = 0;
  //   this->unknown_a2 = 0;

  for (size_t warp_type = 0; warp_type < 5; warp_type++) {
    this->warp_positions[warp_type][0].clear(0xFF);
    this->warp_positions[warp_type][1].clear(0xFF);
  }

  this->num_trap_tiles_nte = 0;
  for (size_t z = 0; z < 0x10; z++) {
    this->trap_tile_locs_nte[z].clear(0xFF);
  }

  for (size_t y = 0; y < 0x10; y++) {
    for (size_t x = 0; x < 0x10; x++) {
      uint8_t tile_spec = this->overlay_state.tiles[y][x];
      uint8_t tile_type = tile_spec & 0xF0;
      uint8_t tile_subtype = tile_spec & 0x0F;
      if (tile_type == 0x30) {
        if (this->warp_positions[tile_subtype][0][0] == 0xFF) {
          this->warp_positions[tile_subtype][0][0] = x;
          this->warp_positions[tile_subtype][0][1] = y;
        } else if (this->warp_positions[tile_subtype][1][0] == 0xFF) {
          this->warp_positions[tile_subtype][1][0] = x;
          this->warp_positions[tile_subtype][1][1] = y;
        }
      } else if ((tile_type == 0x10) || (tile_type == 0x20) || (tile_type == 0x50)) {
        this->map_and_rules->map.tiles[y][x] = 0;
      } else if (is_nte && (tile_type == 0x40) && (this->num_trap_tiles_nte < 0x10)) {
        this->trap_tile_locs_nte[this->num_trap_tiles_nte][0] = x;
        this->trap_tile_locs_nte[this->num_trap_tiles_nte][1] = y;
        this->num_trap_tiles_nte++;
      }
    }
  }

  if (!is_nte) {
    for (size_t trap_type = 0; trap_type < 5; trap_type++) {
      this->chosen_trap_tile_index_of_type[trap_type] = 0xFF;

      size_t num_trap_tiles = 0;
      for (size_t y = 0; y < 0x10; y++) {
        for (size_t x = 0; x < 0x10; x++) {
          if ((this->overlay_state.tiles[y][x] == (trap_type | 0x40)) &&
              (num_trap_tiles < 8)) {
            this->trap_tile_locs[trap_type][num_trap_tiles][0] = x;
            this->trap_tile_locs[trap_type][num_trap_tiles][1] = y;
            num_trap_tiles++;
          }
        }
      }
      this->num_trap_tiles_of_type[trap_type] = num_trap_tiles;

      if (num_trap_tiles > 0) {
        this->chosen_trap_tile_index_of_type[trap_type] = this->get_random(num_trap_tiles);
      }
    }
  }

  if (is_nte) {
    this->send_all_state_updates();
    this->send_6xB4x02_for_all_players_if_needed();

    G_UpdateMap_Ep3NTE_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    cmd.start_battle = 1;
    this->send(cmd);

  } else {
    this->send_6xB4x02_for_all_players_if_needed(true);
    this->send_6xB4x05();

    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        ps->send_set_card_updates(true);
      }
    }

    this->send_all_state_updates();
    this->send_6xB4x1C_names_update();
  }

  this->registration_phase = RegistrationPhase::BATTLE_STARTED;
  this->update_battle_state_flags_and_send_6xB4x03_if_needed(true);

  if (!is_nte) {
    this->send_6xB4x50_trap_tile_locations();
    G_UpdateMap_Ep3_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    cmd.start_battle = 1;
    this->send(cmd);
  }
  this->battle_start_usecs = phosg::now();

  this->send_6xB4x46();

  // Re-send game metadata to spectator teams, since loading the battle scene
  // seems to delete it
  auto l = this->lobby.lock();
  if (l) {
    send_ep3_update_game_metadata(l);
  }
}

G_SetStateFlags_Ep3_6xB4x03 Server::prepare_6xB4x03() const {
  G_SetStateFlags_Ep3_6xB4x03 cmd;
  cmd.state.turn_num = this->round_num;
  cmd.state.battle_phase = this->battle_phase;
  cmd.state.current_team_turn1 = this->current_team_turn1;
  cmd.state.current_team_turn2 = this->current_team_turn2;
  cmd.state.action_subphase = this->action_subphase;
  cmd.state.setup_phase = this->setup_phase;
  cmd.state.registration_phase = this->registration_phase;
  cmd.state.team_exp[0] = this->team_exp[0];
  cmd.state.team_exp[1] = this->team_exp[1];
  cmd.state.team_dice_bonus[0] = this->team_dice_bonus[0];
  cmd.state.team_dice_bonus[1] = this->team_dice_bonus[1];
  cmd.state.first_team_turn = this->first_team_turn;
  cmd.state.tournament_flag = this->options.tournament ? 1 : 0;
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (!ps) {
      cmd.state.client_sc_card_types[z] = CardType::INVALID_FF;
    } else {
      cmd.state.client_sc_card_types[z] = ps->get_sc_card_type();
    }
  }
  return cmd;
}

void Server::update_battle_state_flags_and_send_6xB4x03_if_needed(bool always_send) {
  G_SetStateFlags_Ep3_6xB4x03 cmd = this->prepare_6xB4x03();
  if (always_send || (*this->state_flags != cmd.state)) {
    *this->state_flags = cmd.state;
    this->send(cmd);
  }
}

bool Server::update_registration_phase() {
  // Returns true if the battle can begin

  auto log = this->log_stack("update_registration_phase: ");

  if (this->setup_phase != SetupPhase::REGISTRATION) {
    log.debug("setup_phase is not REGISTRATION");
    return false;
  }

  if (this->map_and_rules->num_players == 0) {
    this->registration_phase = RegistrationPhase::AWAITING_NUM_PLAYERS;
    log.debug("registration_phase set to AWAITING_NUM_PLAYERS");
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    return false;
  }

  if (this->map_and_rules->num_players != this->num_clients_present) {
    this->registration_phase = RegistrationPhase::AWAITING_PLAYERS;
    log.debug("registration_phase set to AWAITING_PLAYERS");
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    return false;
  }

  size_t num_team0_registered_players = 0;
  for (size_t z = 0; z < 4; z++) {
    if (this->deck_entries[z]->team_id == 0) {
      num_team0_registered_players++;
    }
  }

  if (num_team0_registered_players != this->map_and_rules->num_team0_players) {
    this->registration_phase = RegistrationPhase::AWAITING_DECKS;
    log.debug("registration_phase set to AWAITING_DECKS");
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    return false;
  }

  this->registration_phase = RegistrationPhase::REGISTERED;
  this->update_battle_state_flags_and_send_6xB4x03_if_needed();
  log.debug("battle can begin");
  return true;
}

const unordered_map<uint8_t, Server::handler_t> Server::subcommand_handlers({
    {0x0B, &Server::handle_CAx0B_mulligan_hand},
    {0x0C, &Server::handle_CAx0C_end_mulligan_phase},
    {0x0D, &Server::handle_CAx0D_end_non_action_phase},
    {0x0E, &Server::handle_CAx0E_discard_card_from_hand},
    {0x0F, &Server::handle_CAx0F_set_card_from_hand},
    {0x10, &Server::handle_CAx10_move_fc_to_location},
    {0x11, &Server::handle_CAx11_enqueue_attack_or_defense},
    {0x12, &Server::handle_CAx12_end_attack_list},
    {0x13, &Server::handle_CAx13_update_map_during_setup},
    {0x14, &Server::handle_CAx14_update_deck_during_setup},
    {0x15, &Server::handle_CAx15_unused_hard_reset_server_state},
    {0x1B, &Server::handle_CAx1B_update_player_name},
    {0x1D, &Server::handle_CAx1D_start_battle},
    {0x21, &Server::handle_CAx21_end_battle},
    {0x28, &Server::handle_CAx28_end_defense_list},
    {0x2B, &Server::handle_CAx2B_legacy_set_card},
    {0x34, &Server::handle_CAx34_subtract_ally_atk_points},
    {0x37, &Server::handle_CAx37_client_ready_to_advance_from_starter_roll_phase},
    {0x3A, &Server::handle_CAx3A_time_limit_expired},
    {0x40, &Server::handle_CAx40_map_list_request},
    {0x41, &Server::handle_CAx41_map_request},
    {0x48, &Server::handle_CAx48_end_turn},
    {0x49, &Server::handle_CAx49_card_counts},
});

void Server::on_server_data_input(shared_ptr<Client> sender_c, const string& data) {
  auto header = check_size_t<G_CardBattleCommandHeader>(data, 0xFFFF);
  size_t expected_size = header.size * 4;
  if (expected_size < data.size()) {
    phosg::print_data(stderr, data);
    throw runtime_error(phosg::string_printf("command is incomplete: expected %zX bytes, received %zX bytes", expected_size, data.size()));
  }
  if (header.subcommand != 0xB3) {
    throw runtime_error("server data command is not 6xB3");
  }

  handler_t handler = nullptr;
  try {
    handler = this->subcommand_handlers.at(header.subsubcommand);
  } catch (const out_of_range&) {
    throw runtime_error("unknown CAx subsubcommand");
  }

  if (this->battle_record && this->battle_record->writable()) {
    this->battle_record->add_command(BattleRecord::Event::Type::SERVER_DATA_COMMAND, data.data(), data.size());
  }

  if ((sender_c && (sender_c->version() == Version::GC_EP3_NTE)) || !header.mask_key) {
    (this->*handler)(sender_c, data);
  } else {
    string unmasked_data = data;
    set_mask_for_ep3_game_command(unmasked_data.data(), unmasked_data.size(), 0);
    (this->*handler)(sender_c, unmasked_data);
  }
}

void Server::handle_CAx0B_mulligan_hand(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_RedrawInitialHand_Ep3_CAx0B>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "REDRAW");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::HAND_REDRAW_OPTION) {
    error_code = -0x5D;
  }
  if (in_cmd.client_id >= 4) {
    error_code = -0x78;
  }
  if (error_code == 0) {
    auto ps = this->player_states.at(in_cmd.client_id);
    if (!ps) {
      error_code = -0x72;
    } else {
      ps->do_mulligan();
    }
  }

  if (!this->options.is_nte() || (error_code == 0)) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num.load();
    out_cmd.error_code = error_code;
    this->send(out_cmd);
  }
  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, error_code);
}

void Server::handle_CAx0C_end_mulligan_phase(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndInitialRedrawPhase_Ep3_CAx0C>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "HAND READY");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if ((this->setup_phase != SetupPhase::HAND_REDRAW_OPTION) &&
      (this->setup_phase != SetupPhase::STARTER_ROLLS)) {
    error_code = -0x5D;
  }

  if (in_cmd.client_id > 4) {
    error_code = -0x78;
  }

  if (this->options.is_nte() && error_code) {
    return;
  }

  G_ActionResult_Ep3_6xB4x1E out_cmd_ack;
  out_cmd_ack.sequence_num = in_cmd.header.sequence_num;
  out_cmd_ack.response_phase = 1;
  this->send(out_cmd_ack);

  if (error_code == 0) {
    auto ps = this->player_states.at(in_cmd.client_id);
    if (!ps) {
      error_code = -0x72;
    } else {
      this->clients_done_in_mulligan_phase[in_cmd.client_id] = true;
      ps->assist_flags |= AssistFlag::READY_TO_END_PHASE;
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();

      bool all_clients_ready = true;
      for (size_t z = 0; z < 4; z++) {
        if (this->player_states[z] && !this->clients_done_in_mulligan_phase[z]) {
          all_clients_ready = false;
          break;
        }
      }
      if (all_clients_ready) {
        this->set_battle_started();
      }
    }
  }

  if (!this->options.is_nte() || !error_code) {
    G_ActionResult_Ep3_6xB4x1E out_cmd_fin;
    out_cmd_fin.sequence_num = in_cmd.header.sequence_num;
    out_cmd_fin.response_phase = 2;
    out_cmd_fin.error_code = error_code;
    this->send(out_cmd_fin);
  }

  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, error_code);
}

void Server::handle_CAx0D_end_non_action_phase(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndNonAttackPhase_Ep3_CAx0D>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "END PHASE");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  if (!this->options.is_nte()) {
    G_ActionResult_Ep3_6xB4x1E out_cmd_ack;
    out_cmd_ack.sequence_num = in_cmd.header.sequence_num;
    out_cmd_ack.response_phase = 1;
    this->send(out_cmd_ack);
  }

  this->set_client_id_ready_to_advance_phase(in_cmd.client_id, static_cast<BattlePhase>(in_cmd.battle_phase.load()));

  G_ActionResult_Ep3_6xB4x1E out_cmd_fin;
  out_cmd_fin.sequence_num = in_cmd.header.sequence_num;
  out_cmd_fin.response_phase = this->options.is_nte() ? 0 : 2;
  this->send(out_cmd_fin);
}

void Server::handle_CAx0E_discard_card_from_hand(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_DiscardCardFromHand_Ep3_CAx0E>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "DISCARD");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::MAIN_BATTLE) {
    error_code = -0x5D;
  }
  if (this->battle_phase != BattlePhase::DRAW) {
    error_code = -0x5D;
  }
  if (in_cmd.client_id >= 4) {
    error_code = -0x78;
  }

  if (error_code == 0) {
    auto ps = this->player_states.at(in_cmd.client_id);
    if (!ps) {
      error_code = -0x72;
    } else if (!(ps->assist_flags & AssistFlag::IS_SKIPPING_TURN)) {
      error_code = ps->discard_ref_from_hand(in_cmd.card_ref) ? 0 : 1;
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
    } else {
      error_code = -0x70;
    }
  }

  if (!this->options.is_nte() || (error_code == 0)) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num;
    out_cmd.error_code = error_code;
    this->send(out_cmd);
  }
  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, error_code);
}

void Server::handle_CAx0F_set_card_from_hand(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_SetCardFromHand_Ep3_CAx0F>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "SET FC");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::MAIN_BATTLE) {
    error_code = -0x5D;
  }
  if (this->battle_phase != BattlePhase::SET) {
    error_code = -0x5D;
  }
  if (in_cmd.card_ref == 0xFFFF) {
    error_code = -0x78;
  }

  bool was_set = false;
  if (in_cmd.client_id >= 4) {
    error_code = -0x78;
  }

  bool always_send_response = (error_code == 0);
  if (always_send_response) {
    this->ruler_server->error_code1 = 0;
    auto ps = this->player_states.at(in_cmd.client_id);
    if (!ps) {
      this->ruler_server->error_code1 = -0x72;
    } else {
      was_set = ps->set_card_from_hand(in_cmd.card_ref, in_cmd.set_index, &in_cmd.loc, in_cmd.assist_target_player, 0);
    }
  } else {
    this->ruler_server->error_code1 = error_code;
  }

  if (!this->options.is_nte() || always_send_response) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num;
    out_cmd.error_code = this->options.is_nte() ? (was_set ? 0 : 1) : this->ruler_server->error_code1;
    this->send(out_cmd);
  }

  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, this->ruler_server->error_code1);
}

void Server::handle_CAx10_move_fc_to_location(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_MoveFieldCharacter_Ep3_CAx10>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "MOVE");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::MAIN_BATTLE) {
    error_code = -0x5D;
  }
  if (this->battle_phase != BattlePhase::MOVE) {
    error_code = -0x5D;
  }
  if (in_cmd.client_id >= 4) {
    error_code = -0x78;
  }

  if (error_code == 0) {
    auto ps = this->player_states.at(in_cmd.client_id);
    if (!ps) {
      this->ruler_server->error_code2 = -0x72;
    } else {
      this->ruler_server->error_code2 = 0;
      ps->move_card_to_location_by_card_index(in_cmd.set_index, in_cmd.loc);
    }
  } else {
    this->ruler_server->error_code2 = error_code;
  }

  if (!this->options.is_nte() || (this->ruler_server->error_code2 == 0)) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num;
    out_cmd.error_code = this->options.is_nte() ? 0 : this->ruler_server->error_code2;
    this->send(out_cmd);
  }

  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, this->ruler_server->error_code2);
}

void Server::handle_CAx11_enqueue_attack_or_defense(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EnqueueAttackOrDefense_Ep3_CAx11>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "ENQUEUE ACT");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::MAIN_BATTLE) {
    error_code = -0x5D;
  }
  if (this->battle_phase != BattlePhase::ACTION) {
    error_code = -0x5D;
  }
  if (error_code == 0) {
    this->ruler_server->error_code3 = 0;
    ActionState pa = in_cmd.entry;
    if (this->enqueue_attack_or_defense(in_cmd.client_id, &pa)) {
      G_SetActionState_Ep3_6xB4x09 out_cmd;
      out_cmd.client_id = in_cmd.client_id;
      out_cmd.state = in_cmd.entry;
      this->send(out_cmd);
    }
  } else {
    this->ruler_server->error_code3 = error_code;
  }

  bool is_nte = this->options.is_nte();
  if (!is_nte || (error_code == 0)) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num;
    out_cmd.error_code = is_nte ? !!this->ruler_server->error_code3 : this->ruler_server->error_code3;
    this->send(out_cmd);
  }

  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, this->ruler_server->error_code3);
}

void Server::handle_CAx12_end_attack_list(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndAttackList_Ep3_CAx12>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "END ATK LIST");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  int32_t error_code = 0;
  if (this->setup_phase != SetupPhase::MAIN_BATTLE) {
    error_code = -0x5D;
  }
  if (error_code == 0) {
    this->end_attack_list_for_client(in_cmd.client_id);
  }

  if (!this->options.is_nte() || (error_code == 0)) {
    G_ActionResult_Ep3_6xB4x1E out_cmd;
    out_cmd.sequence_num = in_cmd.header.sequence_num;
    this->send(out_cmd);
  }

  this->send_debug_message_if_error_code_nonzero(in_cmd.client_id, error_code);
}

template <typename CmdT>
void Server::handle_CAx13_update_map_during_setup_t(shared_ptr<Client> c, const string& data) {
  const auto& in_cmd = check_size_t<CmdT>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "UPDATE MAP");

  if (!this->battle_in_progress &&
      (this->setup_phase == SetupPhase::REGISTRATION) &&
      (this->map_and_rules->num_players == 0) &&
      (this->registration_phase != RegistrationPhase::REGISTERED) &&
      (this->registration_phase != RegistrationPhase::BATTLE_STARTED)) {
    *this->map_and_rules = in_cmd.map_and_rules_state;
    // The client will likely send incorrect values for the extended rules (or
    // in the case of NTE, no values at all, since the Rules structure is
    // smaller). So, use the values from the last chosen map if applicable, or
    // the values from the $dicerange command if available.
    uint8_t language = c ? c->language() : 1;
    const Rules* map_rules = this->last_chosen_map ? &this->last_chosen_map->version(language)->map->default_rules : nullptr;
    auto& server_rules = this->map_and_rules->rules;
    // NTE can specify the DEF dice value range in its Rules struct, so we use
    // that unless the map or $dicerange overrides it.
    server_rules.def_dice_value_range = (map_rules && (map_rules->def_dice_value_range != 0xFF))
        ? map_rules->def_dice_value_range
        : (this->def_dice_value_range_override != 0xFF)
        ? this->def_dice_value_range_override
        : this->options.is_nte()
        ? server_rules.def_dice_value_range
        : 0;
    server_rules.atk_dice_value_range_2v1 = (map_rules && (map_rules->atk_dice_value_range_2v1 != 0xFF))
        ? map_rules->atk_dice_value_range_2v1
        : (this->atk_dice_value_range_2v1_override != 0xFF)
        ? this->atk_dice_value_range_2v1_override
        : 0;
    server_rules.def_dice_value_range_2v1 = (map_rules && (map_rules->def_dice_value_range_2v1 != 0xFF))
        ? map_rules->def_dice_value_range_2v1
        : (this->def_dice_value_range_2v1_override != 0xFF)
        ? this->def_dice_value_range_2v1_override
        : 0;

    // If this match is part of a tournament, ignore the rules sent by the
    // client and use the tournament rules instead.
    if (this->options.tournament) {
      this->map_and_rules->rules = this->options.tournament->get_rules();
    }

    if (this->override_environment_number != 0xFF) {
      this->map_and_rules->environment_number = this->override_environment_number;
      this->override_environment_number = 0xFF;
    }
    this->overlay_state = in_cmd.overlay_state;
    if (this->options.behavior_flags & BehaviorFlag::DISABLE_TIME_LIMITS) {
      this->map_and_rules->rules.overall_time_limit = 0;
      this->map_and_rules->rules.phase_time_limit = 0;
    }
    if (this->map_and_rules->rules.check_invalid_fields()) {
      this->map_and_rules->rules.check_and_reset_invalid_fields();
    }
    if (this->map_and_rules->num_players_per_team == 0) {
      this->map_and_rules->num_players_per_team = this->map_and_rules->num_players >> 1;
    }
    this->update_registration_phase();
  }
}

void Server::handle_CAx13_update_map_during_setup(shared_ptr<Client> c, const string& data) {
  if (this->options.is_nte()) {
    this->handle_CAx13_update_map_during_setup_t<G_SetMapState_Ep3NTE_CAx13>(c, data);
  } else {
    this->handle_CAx13_update_map_during_setup_t<G_SetMapState_Ep3_CAx13>(c, data);
  }
}

void Server::handle_CAx14_update_deck_during_setup(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_SetPlayerDeck_Ep3_CAx14>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "UPDATE DECK");

  if (!this->battle_in_progress) {
    if ((this->setup_phase == SetupPhase::REGISTRATION) &&
        (this->registration_phase != RegistrationPhase::REGISTERED) &&
        (this->registration_phase != RegistrationPhase::BATTLE_STARTED)) {
      if (in_cmd.client_id >= 4) {
        return;
      }
      DeckEntry entry = in_cmd.entry;
      int32_t verify_error = 0;
      if (!(this->options.behavior_flags & BehaviorFlag::SKIP_DECK_VERIFY)) {
        // Note: Sega's original implementation doesn't use the card counts here
        if (this->options.behavior_flags & BehaviorFlag::IGNORE_CARD_COUNTS) {
          verify_error = this->ruler_server->verify_deck(entry.card_ids);
        } else {
          verify_error = this->ruler_server->verify_deck(entry.card_ids,
              &this->client_card_counts[in_cmd.client_id]);
        }
      }
      if (verify_error) {
        throw runtime_error(phosg::string_printf("invalid deck: -0x%" PRIX32, verify_error));
      }
      if (!this->options.is_nte() && !(this->options.behavior_flags & BehaviorFlag::SKIP_D1_D2_REPLACE)) {
        this->ruler_server->replace_D1_D2_rank_cards_with_Attack(entry.card_ids);
      }
      *this->deck_entries[in_cmd.client_id] = in_cmd.entry;
      this->presence_entries[in_cmd.client_id].player_present = true;
      this->presence_entries[in_cmd.client_id].is_cpu_player = in_cmd.is_cpu_player;
      this->set_player_deck_valid(in_cmd.client_id);
    }

    this->num_clients_present = 0;
    for (size_t z = 0; z < 4; z++) {
      if (this->check_presence_entry(z)) {
        this->num_clients_present++;
      }
    }

    this->send_all_state_updates();
    this->update_registration_phase();
  }
}

void Server::handle_CAx15_unused_hard_reset_server_state(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_HardResetServerState_Ep3_CAx15>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "HARD RESET");

  // In the original implementation, this command recreates the server object.
  // This is possible because the dispatch function is not part of the server
  // object in the original implementation; however, in our implementation, it
  // is, so we don't support this. The original implementation did this:
  //   this->base()->recreate_server(); // Destroys *this, which we can't do
  //   root_card_server = this->server;
  //   *this->map_and_rules = *this->initial_map_and_rules;
  //   this->send_all_state_updates();
  //   this->update_registration_phase();
  //   this->setup_and_start_battle();
  throw runtime_error("hard reset command received");
}

void Server::handle_CAx1B_update_player_name(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_SetPlayerName_Ep3_CAx1B>(data);
  this->send_debug_command_received_message(in_cmd.entry.client_id, in_cmd.header.subsubcommand, "UPDATE NAME");

  if (in_cmd.entry.client_id < 4) {
    if (!this->is_registration_complete()) {
      this->name_entries[in_cmd.entry.client_id] = in_cmd.entry;
      this->name_entries_valid[in_cmd.entry.client_id] = false;
    }

    // Note: This check is not part of the original code. This replaces a
    // disconnecting player with a CPU if the battle is in progress.
    auto l = this->lobby.lock();
    if (l && !l->clients[in_cmd.entry.client_id]) {
      this->name_entries[in_cmd.entry.client_id].is_cpu_player = 1;
      this->presence_entries[in_cmd.entry.client_id].is_cpu_player = 1;
      auto ps = this->player_states[in_cmd.entry.client_id];
      if (ps && ps->hand_and_equip && !ps->hand_and_equip->is_cpu_player) {
        ps->hand_and_equip->is_cpu_player = 1;
        this->send_6xB4x02_for_all_players_if_needed();
      }
    }
  }

  G_SetPlayerNames_Ep3_6xB4x1C out_cmd;
  for (size_t z = 0; z < 4; z++) {
    out_cmd.entries[z] = this->name_entries[z];
  }
  this->send(out_cmd);
}

void Server::handle_CAx1D_start_battle(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_StartBattle_Ep3_CAx1D>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "START BATTLE");

  if (!this->battle_in_progress) {
    bool is_nte = this->options.is_nte();

    bool should_start = false;
    if (is_nte) {
      should_start = (this->registration_phase == RegistrationPhase::REGISTERED);
    } else if (this->update_registration_phase()) {
      should_start = true;
    } else {
      G_RejectBattleStartRequest_Ep3_6xB4x53 out_cmd;
      out_cmd.setup_phase = this->setup_phase;
      out_cmd.registration_phase = this->registration_phase;
      out_cmd.state = *this->map_and_rules;
      this->send(out_cmd);

      for (size_t z = 0; z < 4; z++) {
        this->deck_entries[z]->clear();
        this->presence_entries[z].clear();
      }
      this->battle_in_progress = false;
    }

    if (should_start) {
      if (this->battle_record && this->battle_record->writable()) {
        this->battle_record->set_battle_start_timestamp();
      }

      auto l = this->lobby.lock();
      if (l) {
        // Note: Sega's implementation doesn't set EX results values here; they
        // did it at game join time instead. We do it here for code simplicity.
        if (!this->options.is_nte() && l->ep3_ex_result_values) {
          this->send(*l->ep3_ex_result_values);
        }
      }

      this->setup_and_start_battle();
      this->battle_in_progress = true;
    }
  }
}

void Server::handle_CAx21_end_battle(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndBattle_Ep3_CAx21>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "END BATTLE");
  if (this->setup_phase == SetupPhase::BATTLE_ENDED) {
    this->battle_finished = true;

    // This logic isn't part of the original implementation.
    auto l = this->lobby.lock();
    if (l) {
      send_ep3_disband_watcher_lobbies(l);
    }
  }
}

void Server::handle_CAx28_end_defense_list(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndDefenseList_Ep3_CAx28>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "END DEF LIST");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  G_ActionResult_Ep3_6xB4x1E out_cmd_ack;
  out_cmd_ack.sequence_num = in_cmd.header.sequence_num;
  out_cmd_ack.response_phase = 1;
  this->send(out_cmd_ack);

  this->defense_list_ended_for_client[in_cmd.client_id] = 1;

  bool all_defense_lists_ended = true;
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps && (this->current_team_turn1 != ps->get_team_id())) {
      if (!ps->get_sc_card()->check_card_flag(2) &&
          (this->defense_list_ended_for_client[z] == 0)) {
        all_defense_lists_ended = false;
        break;
      }
    }
  }

  if (all_defense_lists_ended && (this->unknown_a10 == 0)) {
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        ps->assist_flags &= (~AssistFlag::READY_TO_END_ACTION_PHASE);
        ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
    }
    this->unknown_8023EE48();
    this->unknown_a10 = 1;
  } else {
    auto ps = this->player_states[in_cmd.client_id];
    ps->assist_flags |= AssistFlag::READY_TO_END_ACTION_PHASE;
    ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  }
  if (this->unknown_a10 != 0) {
    this->unknown_8023EE80();
    this->unknown_a10 = 0;
  }

  G_ActionResult_Ep3_6xB4x1E out_cmd_fin;
  out_cmd_fin.sequence_num = in_cmd.header.sequence_num;
  out_cmd_fin.response_phase = 2;
  this->send(out_cmd_fin);
}

void Server::handle_CAx2B_legacy_set_card(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_ExecLegacyCard_Ep3_CAx2B>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "EXEC LEGACY");
  // Sega's original implementation does nothing here, so we do nothing as well.
}

void Server::handle_CAx34_subtract_ally_atk_points(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_PhotonBlastRequest_Ep3_CAx34>(data);

  uint8_t card_ref_client_id = client_id_for_card_ref(in_cmd.card_ref);
  this->send_debug_command_received_message(card_ref_client_id, in_cmd.header.subsubcommand, "SUB ALLY ATK");

  if (card_ref_client_id >= 4) {
    return;
  }

  auto this_ps = this->player_states[card_ref_client_id];
  if (this_ps && (in_cmd.ally_client_id < 4)) {
    auto ally_ps = this->player_states[in_cmd.ally_client_id];
    if (ally_ps && (this->has_done_pb[card_ref_client_id])) {

      if (in_cmd.reason == 0) {
        this->has_done_pb_with_client[card_ref_client_id][in_cmd.ally_client_id] = true;
        bool accepted = true;
        for (size_t z = 0; z < 4; z++) {
          auto ally_ps = this->get_player_state(z);
          if ((z != card_ref_client_id) && ally_ps) {
            if (this_ps->get_team_id() == ally_ps->get_team_id()) {
              if (this->has_done_pb_with_client[card_ref_client_id][z] == 0) {
                accepted = false;
              }
              break;
            }
          }
        }
        if (accepted) {
          G_PhotonBlastStatus_Ep3_6xB4x35 out_cmd;
          out_cmd.accepted = 0;
          out_cmd.card_ref = in_cmd.card_ref;
          out_cmd.client_id = card_ref_client_id;
          this->send(out_cmd);
        }

      } else {
        auto ce = this->definition_for_card_ref(in_cmd.card_ref);
        if (ce->def.ally_cost <= ally_ps->get_atk_points()) {
          auto& pa = this->pb_action_states[card_ref_client_id];
          if (this->num_pending_attacks < 0x20) {
            this->num_pending_attacks++;
            this->pending_attacks[this->num_pending_attacks] = pa;
            this_ps->set_action_cards_for_action_state(pa);
            ally_ps->subtract_atk_points(ce->def.ally_cost);
            if (ce->def.ally_cost > 0) {
              ally_ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
            }
            auto attacker_card = this->card_for_set_card_ref(pa.attacker_card_ref);
            if (attacker_card) {
              attacker_card->card_flags |= 0x400;
              attacker_card->player_state()->send_6xB4x04_if_needed();
            }
            uint16_t card_ref = this->send_6xB4x06_if_card_ref_invalid(pa.original_attacker_card_ref, 9);
            auto orig_attacker_card = this->card_for_set_card_ref(card_ref);
            auto target_card = this->card_for_set_card_ref(pa.target_card_refs[0]);
            if (orig_attacker_card && target_card) {
              target_card->card_flags |= 0x800;
              target_card->player_state()->send_6xB4x04_if_needed();
            }
            this->has_done_pb[card_ref_client_id] = false;

            G_PhotonBlastStatus_Ep3_6xB4x35 out_cmd;
            out_cmd.client_id = card_ref_client_id;
            out_cmd.accepted = 1;
            out_cmd.card_ref = in_cmd.card_ref;
            this->send(out_cmd);
          }
        }
      }
    }
  }
}

void Server::handle_CAx37_client_ready_to_advance_from_starter_roll_phase(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_AdvanceFromStartingRollsPhase_Ep3_CAx37>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "CHOOSE ORDER");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  auto ps = this->player_states[in_cmd.client_id];
  if (ps) {
    ps->assist_flags |= AssistFlag::READY_TO_END_STARTER_ROLL_PHASE;
    ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
  }
  if (this->setup_phase == SetupPhase::STARTER_ROLLS) {
    bool all_clients_ready = true;
    for (size_t z = 0; z < 4; z++) {
      auto other_ps = this->player_states[z];
      if (other_ps && !(other_ps->assist_flags & AssistFlag::READY_TO_END_STARTER_ROLL_PHASE)) {
        all_clients_ready = false;
        break;
      }
    }

    if (all_clients_ready) {
      this->setup_phase = SetupPhase::HAND_REDRAW_OPTION;
      this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    }
  }
}

void Server::handle_CAx3A_time_limit_expired(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_OverallTimeLimitExpired_Ep3_CAx3A>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "TIME EXPIRED");
  // We don't need to do anything here because the overall time limit is tracked
  // server-side instead.
}

void Server::handle_CAx40_map_list_request(shared_ptr<Client> sender_c, const string& data) {
  const auto& in_cmd = check_size_t<G_MapListRequest_Ep3_CAx40>(data);
  this->send_debug_command_received_message(in_cmd.header.subsubcommand, "MAP LIST");

  auto l = this->lobby.lock();
  if (!l) {
    throw runtime_error("lobby is deleted");
  }

  size_t num_players = l ? l->count_clients() : 1;
  uint8_t language = sender_c ? sender_c->language() : 1;
  const auto& list_data = this->options.map_index->get_compressed_list(num_players, language);

  phosg::StringWriter w;
  uint32_t subcommand_size = (list_data.size() + sizeof(G_MapList_Ep3_6xB6x40) + 3) & (~3);
  w.put<G_MapList_Ep3_6xB6x40>(G_MapList_Ep3_6xB6x40{{{{0xB6, 0, 0}, subcommand_size}, 0x40, {}}, list_data.size(), 0});
  w.write(list_data);
  while (w.size() & 3) {
    w.put_u8(0);
  }

  const auto& out_data = w.str();
  this->send(out_data.data(), out_data.size(), 0x6C, false);
}

void Server::send_6xB6x41_to_all_clients() const {
  if (!this->last_chosen_map) {
    throw logic_error("cannot send 6xB4x41 without a map chosen");
  }

  auto l = this->lobby.lock();
  if (l) {
    vector<string> map_commands_by_language;
    auto send_to_client = [&](shared_ptr<Client> c) -> void {
      if (!c) {
        return;
      }
      if (map_commands_by_language.size() <= c->language()) {
        map_commands_by_language.resize(c->language() + 1);
      }
      if (map_commands_by_language[c->language()].empty()) {
        map_commands_by_language[c->language()] = this->prepare_6xB6x41_map_definition(
            this->last_chosen_map, c->language(), this->options.is_nte());
      }
      this->log().info("Sending %c version of map %08" PRIX32, char_for_language_code(c->language()), this->last_chosen_map->map_number);
      send_command(c, 0x6C, 0x00, map_commands_by_language[c->language()]);
    };
    for (const auto& c : l->clients) {
      send_to_client(c);
    }
    for (auto watcher_l : l->watcher_lobbies) {
      for (const auto& c : watcher_l->clients) {
        send_to_client(c);
      }
    }

    if (this->battle_record && this->battle_record->writable()) {
      // TODO: It's not great that we just pick the first one; ideally we'd put
      // all of them in the recording and send the appropriate one to the client
      // in the playback lobby
      for (string& data : map_commands_by_language) {
        if (!data.empty()) {
          this->battle_record->add_command(
              BattleRecord::Event::Type::BATTLE_COMMAND, std::move(data));
          break;
        }
      }
    }

  } else {
    auto out_data = this->prepare_6xB6x41_map_definition(this->last_chosen_map, 1, false);
    this->send(out_data.data(), out_data.size(), 0x6C, false);
  }
}

void Server::handle_CAx41_map_request(shared_ptr<Client>, const string& data) {
  const auto& cmd = check_size_t<G_MapDataRequest_Ep3_CAx41>(data);
  this->send_debug_command_received_message(cmd.header.subsubcommand, "MAP DATA");
  if (!this->options.tournament || (this->options.tournament->get_map()->map_number == cmd.map_number)) {
    this->last_chosen_map = this->options.map_index->for_number(cmd.map_number);
    this->send_6xB6x41_to_all_clients();
  }
}

void Server::handle_CAx48_end_turn(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_EndTurn_Ep3_CAx48>(data);
  this->send_debug_command_received_message(in_cmd.client_id, in_cmd.header.subsubcommand, "END TURN");
  if (in_cmd.client_id >= 4) {
    throw runtime_error("invalid client ID");
  }

  auto ps = this->get_player_state(in_cmd.client_id);
  if (ps && ps->draw_cards_allowed()) {
    ps->draw_hand(0);
  }

  G_ActionResult_Ep3_6xB4x1E out_cmd;
  out_cmd.sequence_num = in_cmd.header.sequence_num;
  this->send(out_cmd);
}

void Server::handle_CAx49_card_counts(shared_ptr<Client>, const string& data) {
  const auto& in_cmd = check_size_t<G_CardCounts_Ep3_CAx49>(data);
  this->send_debug_command_received_message(in_cmd.header.sender_client_id, in_cmd.header.subsubcommand, "CARD COUNTS");

  // Note: Sega's implmentation completely ignores this command. This
  // implementation is not based on the original code.
  auto& dest_counts = this->client_card_counts[in_cmd.header.sender_client_id];
  dest_counts = in_cmd.card_id_to_count;
  decrypt_trivial_gci_data(dest_counts.data(), dest_counts.bytes(), in_cmd.basis);
}

void Server::compute_losing_team_id_and_add_winner_flags(uint32_t flags) {
  bool is_nte = this->options.is_nte();

  if (!is_nte) {
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        ps->assist_flags &= ~(AssistFlag::HAS_WON_BATTLE |
            AssistFlag::WINNER_DECIDED_BY_DEFEAT |
            AssistFlag::BATTLE_DID_NOT_END_DUE_TO_TIME_LIMIT);
      }
    }
  }

  uint32_t winner_flags = flags | AssistFlag::HAS_WON_BATTLE | AssistFlag::WINNER_DECIDED_BY_DEFEAT;

  int8_t losing_team_id = -1;
  array<uint32_t, 2> team_counts = {0, 0};

  if (!is_nte) {
    // First, check which team has more dead SCs
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        auto sc_card = ps->get_sc_card();
        if (sc_card && (sc_card->card_flags & 2)) {
          team_counts.at(ps->get_team_id())++;
        }
      }
    }
    if (team_counts[1] < team_counts[0]) {
      losing_team_id = 0;
    } else if (team_counts[0] < team_counts[1]) {
      losing_team_id = 1;
    }

    // If the SC counts match, break ties by remaining SC HP
    if (losing_team_id == -1) {
      team_counts[0] = 0;
      team_counts[1] = 0;
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps) {
          auto sc_card = ps->get_sc_card();
          if (sc_card) {
            team_counts.at(ps->get_team_id()) += sc_card->get_current_hp();
          }
        }
      }
      if (team_counts[0] < team_counts[1]) {
        losing_team_id = 0;
      } else if (team_counts[1] < team_counts[0]) {
        losing_team_id = 1;
      }
    }
  }

  // If still tied, break ties by number of opponent cards destroyed
  if (losing_team_id == -1) {
    team_counts[0] = 0;
    team_counts[1] = 0;
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        team_counts.at(ps->get_team_id()) += ps->stats.num_opponent_cards_destroyed;
      }
    }
    if (team_counts[0] < team_counts[1]) {
      losing_team_id = 0;
    } else if (team_counts[1] < team_counts[0]) {
      losing_team_id = 1;
    }
  }

  // If still tied, break ties by amount of damage given
  if (losing_team_id == -1) {
    team_counts[0] = 0;
    team_counts[1] = 0;
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps) {
        team_counts.at(ps->get_team_id()) += ps->stats.damage_given;
      }
    }
    if (team_counts[0] < team_counts[1]) {
      losing_team_id = 0;
    } else if (team_counts[1] < team_counts[0]) {
      losing_team_id = 1;
    }
  }

  // If STILL tied, roll dice and arbitrarily make one team the winner
  if (losing_team_id == -1) {
    while (losing_team_id == -1) {
      team_counts[1] = 0;
      team_counts[0] = 0;
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps) {
          team_counts.at(ps->get_team_id()) += ps->roll_dice(1);
        }
      }
      team_counts[0] *= this->team_client_count[1];
      team_counts[1] *= this->team_client_count[0];
      if (team_counts[0] < team_counts[1]) {
        losing_team_id = 0;
      } else if (team_counts[1] < team_counts[0]) {
        losing_team_id = 1;
      }
    }
    winner_flags = flags | AssistFlag::HAS_WON_BATTLE | AssistFlag::WINNER_DECIDED_BY_RANDOM;
  }

  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      if (losing_team_id != ps->get_team_id()) {
        ps->assist_flags |= winner_flags;
      }
      if (!is_nte || (losing_team_id != ps->get_team_id())) {
        ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed();
      }
    }
  }
}

uint32_t Server::get_team_exp(uint8_t team_id) const {
  return this->team_exp[team_id];
}

uint32_t Server::send_6xB4x06_if_card_ref_invalid(uint16_t card_ref, int16_t negative_value) {
  if (this->card_special) {
    return this->card_special->send_6xB4x06_if_card_ref_invalid(card_ref, -negative_value);
  }
  return card_ref;
}

void Server::unknown_8023EEF4() {
  auto log = this->log_stack("unknown_8023EEF4: ");

  if (this->unknown_a14 >= 0x20) {
    log.debug("unknown_a14 too large (0x%" PRIX32 ")", this->unknown_a14);
    return;
  }

  bool is_nte = this->options.is_nte();
  while (this->unknown_a14 < this->num_pending_attacks_with_cards) {
    auto card = this->attack_cards[this->unknown_a14];
    if (this->get_current_team_turn() == card->get_team_id()) {
      ActionState as = this->pending_attacks_with_cards[this->unknown_a14];
      if (log.should_log(phosg::LogLevel::DEBUG)) {
        log.debug("card @%04hX #%04hX can attack", card->get_card_ref(), card->get_card_id());
        string as_str = as.str(this->shared_from_this());
        log.debug("as: %s", as_str.c_str());
      }
      if (is_nte) {
        this->replace_targets_due_to_destruction_nte(&as);
      } else {
        this->replace_targets_due_to_destruction_or_conditions(&as);
      }
      if (log.should_log(phosg::LogLevel::DEBUG)) {
        string as_str = as.str(this->shared_from_this());
        log.debug("as after target replacement: %s", as_str.c_str());
      }
      if (this->any_target_exists_for_attack(as)) {
        log.debug("as is valid");
        break;
      } else {
        log.debug("as is not valid");
      }
    } else {
      log.debug("card @%04hX #%04hX cannot attack (wrong turn)", card->get_card_ref(), card->get_card_id());
    }
    this->unknown_a14++;
  }

  if (this->unknown_a14 < this->num_pending_attacks_with_cards) {
    log.debug("a14 (%" PRIu32 ") < num_pending_attacks_with_cards (%" PRIu32 ")", this->unknown_a14, this->num_pending_attacks_with_cards);
    this->defense_list_ended_for_client.clear(false);

    G_UpdateAttackTargets_Ep3_6xB4x29 cmd;
    cmd.attack_number = this->unknown_a14;
    cmd.state = this->pending_attacks_with_cards[this->unknown_a14];
    if (is_nte) {
      this->replace_targets_due_to_destruction_nte(&cmd.state);
    } else {
      this->replace_targets_due_to_destruction_or_conditions(&cmd.state);
    }
    ActionState as = cmd.state;
    this->send(cmd);

    this->card_special->apply_effects_after_attack_target_resolution(as);

    if (!is_nte) {
      this->attack_cards[this->unknown_a14]->compute_action_chain_results(1, 0);
      this->attack_cards[this->unknown_a14]->unknown_80236374(this->attack_cards[this->unknown_a14], &as);
      if (!this->attack_cards[this->unknown_a14]->action_chain.check_flag(0x40)) {
        this->card_special->unknown_8024945C(this->attack_cards[this->unknown_a14], &as);
      }
      this->attack_cards[this->unknown_a14]->compute_action_chain_results(1, 0);
      this->attack_cards[this->unknown_a14]->unknown_80236374(this->attack_cards[this->unknown_a14], &as);
      if (!this->attack_cards[this->unknown_a14]->action_chain.check_flag(0x40)) {
        this->card_special->unknown_8024966C(this->attack_cards[this->unknown_a14], &as);
      }
      this->attack_cards[this->unknown_a14]->compute_action_chain_results(1, 0);
      this->attack_cards[this->unknown_a14]->unknown_80236374(this->attack_cards[this->unknown_a14], &as);
      this->attack_cards[this->unknown_a14]->send_6xB4x4E_4C_4D_if_needed();
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps) {
          ps->send_set_card_updates();
        }
      }
    }

  } else {
    this->unknown_a15 = 0;
    for (size_t z = 0; z < 4; z++) {
      auto ps = this->player_states[z];
      if (ps && (this->current_team_turn1 == ps->get_team_id())) {
        this->set_client_id_ready_to_advance_phase(z, this->battle_phase);
      }
    }
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    this->send_6xB4x02_for_all_players_if_needed();
  }

  if (!is_nte) {
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    this->send_6xB4x02_for_all_players_if_needed();
  }
}

void Server::execute_bomb_assist_effect() {
  int16_t max_hp = -999;
  int16_t min_hp = 999;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps && !this->assist_server->should_block_assist_effects_for_client(client_id)) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card && !(card->card_flags & 2)) {
          max_hp = max<int16_t>(max_hp, card->get_current_hp());
          min_hp = min<int16_t>(min_hp, card->get_current_hp());
        }
      }
    }
  }

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    // Possible bug: shouldn't we check should_block_assist_effects_for_client
    // here too? If the player has a card with the same HP as another one that
    // would be destroyed, it looks like the card can be destroyed even if the
    // client should be immune to assist effects here.
    if (ps) {
      for (size_t set_index = 0; set_index < 8; set_index++) {
        auto card = ps->get_set_card(set_index);
        if (card && !(card->card_flags & 2) &&
            ((card->get_current_hp() == max_hp) || (card->get_current_hp() == min_hp))) {
          card->player_state()->handle_homesick_assist_effect_from_bomb(card);
        }
      }
    }
  }
}

void Server::replace_targets_due_to_destruction_nte(ActionState* as) {
  auto attacker_card = this->card_for_set_card_ref(this->send_6xB4x06_if_card_ref_invalid(as->attacker_card_ref, 3));

  for (size_t z = 0; z < 0x24; z++) {
    auto target_card = this->card_for_set_card_ref(as->target_card_refs[z]);
    if (!target_card) {
      break;
    }
    if (!(target_card->card_flags & 2) ||
        (target_card->get_definition()->def.type != CardType::ITEM) ||
        attacker_card->action_chain.check_flag(0x02)) {
      continue;
    }
    auto ps = target_card->player_state();
    shared_ptr<Card> found_guard_item;
    for (size_t z = 0; z < 8; z++) {
      auto set_card = ps->get_set_card(z);
      if (set_card &&
          (set_card != target_card) &&
          !(set_card->card_flags & 2) &&
          set_card->is_guard_item()) {
        found_guard_item = set_card;
        break;
      }
    }
    auto replaced_target = found_guard_item;
    if (!found_guard_item) {
      for (size_t z = 0; z < 8; z++) {
        auto set_card = ps->get_set_card(z);
        if (set_card && (set_card != target_card) && !(set_card->card_flags & 2)) {
          replaced_target = set_card;
          break;
        }
      }
    }
    as->target_card_refs[z] = replaced_target ? replaced_target->get_card_ref() : ps->get_sc_card()->get_card_ref();
  }

  size_t write_offset = 0;
  for (size_t z = 0; z < as->target_card_refs.size(); z++) {
    if (as->target_card_refs[z] != 0xFFFF) {
      if (z != write_offset) {
        as->target_card_refs[write_offset] = as->target_card_refs[z];
      }
      write_offset++;
    }
  }
  as->target_card_refs.clear_after(write_offset, 0xFFFF);
}

void Server::replace_targets_due_to_destruction_or_conditions(ActionState* as) {
  auto attacker_card = this->card_for_set_card_ref(this->send_6xB4x06_if_card_ref_invalid(as->attacker_card_ref, 3));
  if (!attacker_card) {
    as->target_card_refs[0] = 0xFFFF;
    return;
  }

  vector<uint16_t> phase1_replaced_card_refs;
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->get_player_state(client_id);
    if (!attacker_card->action_chain.check_flag(0x200 << client_id)) {
      for (size_t target_index = 0; target_index < 4 * 9; target_index++) {
        uint32_t target_card_ref = as->target_card_refs[target_index];
        if (target_card_ref == 0xFFFF) {
          break;
        }
        if (client_id == client_id_for_card_ref(target_card_ref)) {
          auto target_card = this->card_for_set_card_ref(this->send_6xB4x06_if_card_ref_invalid(target_card_ref, 5));
          auto ce = this->definition_for_card_ref(target_card_ref);
          if (ce && ps) {
            if (!target_card || (target_card->card_flags & 2)) {
              if (ce->def.type == CardType::ITEM) {
                for (size_t set_index = 0; set_index < 8; set_index++) {
                  target_card = ps->get_set_card(set_index);
                  if (target_card &&
                      (target_card->get_card_ref() != target_card_ref) &&
                      !(target_card->card_flags & 2) &&
                      target_card->is_guard_item()) {
                    break;
                  }
                  target_card = nullptr;
                }
                auto replaced_target_card = target_card;
                if (!target_card) {
                  for (size_t set_index = 0; set_index < 8; set_index++) {
                    replaced_target_card = ps->get_set_card(set_index);
                    if (replaced_target_card &&
                        (replaced_target_card->get_card_ref() != target_card_ref) &&
                        !(replaced_target_card->card_flags & 2)) {
                      break;
                    }
                    replaced_target_card = target_card;
                  }
                }
                if (!replaced_target_card) {
                  target_card = ps->get_sc_card();
                  if (target_card) {
                    phase1_replaced_card_refs.emplace_back(target_card->get_card_ref());
                  }
                } else {
                  phase1_replaced_card_refs.emplace_back(replaced_target_card->get_card_ref());
                }
              }
            } else {
              phase1_replaced_card_refs.emplace_back(target_card_ref);
            }
          }
        }
      }

    } else {
      if (ps) {
        size_t present_target_count = 0;
        size_t missing_target_count = 0;
        size_t set_card_count = ps->count_set_cards();
        for (size_t target_index = 0; (target_index < 4 * 9) && (as->target_card_refs[target_index] != 0xFFFF); target_index++) {
          if (client_id == client_id_for_card_ref(as->target_card_refs[target_index])) {
            auto target_card = this->card_for_set_card_ref(as->target_card_refs[target_index]);
            if (!target_card || (target_card->card_flags & 2)) {
              missing_target_count++;
            } else {
              present_target_count++;
              phase1_replaced_card_refs.emplace_back(target_card->get_card_ref());
            }
          }
        }
        auto sc_card = ps->get_sc_card();
        if ((present_target_count == 0) &&
            (missing_target_count > 0) &&
            (set_card_count == 0) &&
            sc_card &&
            sc_card->get_definition() &&
            (sc_card->get_definition()->def.type == CardType::HUNTERS_SC)) {
          phase1_replaced_card_refs.emplace_back(sc_card->get_card_ref());
        }
      }
    }
  }

  // Note: The original code only writes a single FFFF after the last card ref
  // in this array; we instead clear the entire array.
  as->target_card_refs.clear(0xFFFF);
  for (size_t z = 0; z < phase1_replaced_card_refs.size(); z++) {
    as->target_card_refs[z] = this->send_6xB4x06_if_card_ref_invalid(phase1_replaced_card_refs[z], 4);
  }
  // as->target_card_refs[phase1_replaced_card_refs.size()] = 0xFFFF;

  vector<uint16_t> phase2_replaced_card_refs;
  for (size_t z = 0; (z < 4 * 9) && (as->target_card_refs[z] != 0xFFFF); z++) {
    uint16_t target_card_ref = this->send_6xB4x06_if_card_ref_invalid(as->target_card_refs[z], 7);
    auto target_card = this->card_for_set_card_ref(target_card_ref);
    if (target_card) {
      auto replaced_target = this->card_special->compute_replaced_target_based_on_conditions(
          target_card->get_card_ref(), 1, 0, as->attacker_card_ref, 0xFFFF, 1,
          0, 0xFF, 0, 0xFFFF);
      if (!replaced_target) {
        replaced_target = target_card;
      }
      phase2_replaced_card_refs.emplace_back(this->send_6xB4x06_if_card_ref_invalid(replaced_target->get_card_ref(), 8));
    }
  }

  // Note: This is different from the original code in the same way as above: we
  // clear the entire array first.
  as->target_card_refs.clear(0xFFFF);
  for (size_t z = 0; z < phase2_replaced_card_refs.size(); z++) {
    as->target_card_refs[z] = this->send_6xB4x06_if_card_ref_invalid(phase2_replaced_card_refs[z], 4);
  }
  // as->target_card_refs[phase2_replaced_card_refs.size()] = 0xFFFF;
}

bool Server::any_target_exists_for_attack(const ActionState& as) {
  auto card = this->card_for_set_card_ref(as.attacker_card_ref);
  if (!card || (card->card_flags & 2)) {
    return false;
  }

  for (size_t z = 0; (z < 4 * 9) && (as.target_card_refs[z] != 0xFFFF); z++) {
    card = this->card_for_set_card_ref(as.target_card_refs[z]);
    if (!card) {
      break;
    }
    if (!(card->card_flags & 2)) {
      return true;
    }
  }
  return false;
}

uint8_t Server::get_current_team_turn2() const {
  return this->current_team_turn2;
}

void Server::unknown_8023EE48() {
  this->unknown_802402F4();
  this->send_6xB4x02_for_all_players_if_needed();
}

void Server::unknown_8023EE80() {
  bool is_nte = this->options.is_nte();

  if (this->unknown_a14 < this->num_pending_attacks_with_cards) {
    this->attack_cards[this->unknown_a14]->apply_attack_result();
    if (is_nte) {
      for (size_t z = 0; z < 4; z++) {
        auto ps = this->player_states[z];
        if (ps) {
          ps->unknown_8023C110();
        }
      }
    }
    this->unknown_a14++;
  }

  this->check_for_battle_end();

  if (is_nte) {
    this->unknown_8023EEF4();
    this->update_battle_state_flags_and_send_6xB4x03_if_needed();
    this->send_6xB4x02_for_all_players_if_needed();
  } else {
    this->copy_player_states_to_prev_states();
    this->unknown_8023EEF4();
    this->send_set_card_updates_and_6xB4x04_if_needed();
  }
}

void Server::unknown_802402F4() {
  auto log = this->log_stack("unknown_802402F4: ");
  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto ps = this->player_states[client_id];
    if (ps && (this->current_team_turn2 == ps->get_team_id())) {
      auto card = ps->get_sc_card();
      if (card) {
        log.debug("SC card has action chain");
        card->compute_action_chain_results(true, false);
      }
      for (size_t set_index = 0; set_index < 8; set_index++) {
        card = ps->get_set_card(set_index);
        if (card) {
          log.debug("set card %zu has action chain", set_index);
          card->compute_action_chain_results(true, false);
        }
      }
    }
  }
}

vector<shared_ptr<Card>> Server::const_cast_set_cards_v(
    const vector<shared_ptr<const Card>>& cards) {
  // TODO: This is dumb. Figure out a not-dumb way to do this.
  vector<shared_ptr<Card>> ret;
  for (auto const_card : cards) {
    auto mutable_card = this->card_for_set_card_ref(const_card->get_card_ref());
    if (mutable_card.get() != const_card.get()) {
      throw logic_error("inconsistent set cards index");
    }
    ret.emplace_back(mutable_card);
  }
  return ret;
}

void Server::send_6xB4x39() const {
  if (this->options.is_nte()) {
    G_UpdateAllPlayerStatistics_Ep3NTE_6xB4x39 cmd;
    for (size_t z = 0; z < 4; z++) {
      if (this->player_states[z]) {
        cmd.stats[z] = this->player_states[z]->stats;
      }
    }
    this->send(cmd);
  } else {
    G_UpdateAllPlayerStatistics_Ep3_6xB4x39 cmd;
    for (size_t z = 0; z < 4; z++) {
      if (this->player_states[z]) {
        cmd.stats[z] = this->player_states[z]->stats;
      }
    }
    this->send(cmd);
  }
}

void Server::send_6xB4x05() {
  this->compute_all_map_occupied_bits();
  if (this->options.is_nte()) {
    G_UpdateMap_Ep3NTE_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    this->send(cmd);
  } else {
    G_UpdateMap_Ep3_6xB4x05 cmd;
    cmd.state = *this->map_and_rules;
    this->send(cmd);
  }
}

void Server::send_6xB4x02_for_all_players_if_needed(bool always_send) {
  for (size_t z = 0; z < 4; z++) {
    auto ps = this->player_states[z];
    if (ps) {
      ps->update_hand_and_equip_state_and_send_6xB4x02_if_needed(always_send);
    }
  }
}

G_SetTrapTileLocations_Ep3_6xB4x50 Server::prepare_6xB4x50_trap_tile_locations() const {
  G_SetTrapTileLocations_Ep3_6xB4x50 cmd;
  for (size_t trap_type = 0; trap_type < 5; trap_type++) {
    uint8_t trap_index = this->chosen_trap_tile_index_of_type[trap_type];
    if (trap_index != 0xFF) {
      cmd.locations[trap_type] = this->trap_tile_locs[trap_type][trap_index];
    } else {
      cmd.locations[trap_type].clear(0xFF);
    }
  }
  return cmd;
}

void Server::send_6xB4x50_trap_tile_locations() const {
  this->send(this->prepare_6xB4x50_trap_tile_locations());
}

} // namespace Episode3
