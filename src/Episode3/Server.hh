#pragma once

#include <stdint.h>

#include <memory>

#include "../Text.hh"
#include "../CommandFormats.hh"
#include "../Channel.hh"
#include "AssistServer.hh"
#include "CardSpecial.hh"
#include "MapState.hh"
#include "PlayerState.hh"
#include "RulerServer.hh"

struct Lobby;

namespace Episode3 {



/**
 * This implementation of Episode 3 battles (contained in all files in the
 * src/Episode3 directory, except for DataIndex.hh/cc) is derived from Sega's
 * original server implementation, reverse-engineered from the Episode 3 client
 * executable. The control flow, function breakdown, and structure definitions
 * in these files map very closely to how their server implementation was
 * written; notable differences (due to necessary environment differences or bug
 * fixes) are described in the comments therein.
 *
 * There are likely undiscovered bugs in this code, some originally written by
 * Sega, but more written by me as I manually transcribed and updated this code.
 */

// Class ownership levels (classes may only contain weak_ptrs, not shared_ptrs,
// to classes at the same or higher level):
// - ServerBase
// - - Server
// - - - RulerServer
// - - - - AssistServer
// - - - - CardSpecial
// - - - - - StateFlags
// - - - - - DeckEntry
// - - - - - PlayerState
// - - - - - - Card
// - - - - - - - CardShortStatus
// - - - - - - - DeckState
// - - - - - - - HandAndEquipState
// - - - - - - - MapAndRulesState / OverlayState
// - - - - - - - - Everything within DataIndex

class Server;



class ServerBase : public std::enable_shared_from_this<ServerBase> {
public:
  ServerBase(
      std::shared_ptr<Lobby> lobby,
      std::shared_ptr<const DataIndex> data_index,
      uint32_t random_seed,
      bool is_tournament);
  void init();
  void reset();
  void recreate_server();

  struct PresenceEntry {
    uint8_t player_present;
    uint8_t deck_valid;
    uint8_t is_cpu_player;
    PresenceEntry();
    void clear();
  } __attribute__((packed));

  std::weak_ptr<Lobby> lobby;
  std::shared_ptr<const DataIndex> data_index;
  uint32_t random_seed;
  bool is_tournament;

  std::shared_ptr<MapAndRulesState> map_and_rules1;
  std::shared_ptr<MapAndRulesState> map_and_rules2;
  std::shared_ptr<DeckEntry> deck_entries[4];
  std::shared_ptr<Server> server;
  parray<PresenceEntry, 4> presence_entries;
  uint8_t num_clients_present;
  parray<NameEntry, 4> name_entries;
  parray<uint8_t, 4> name_entries_valid;
  OverlayState overlay_state;
  parray<parray<uint8_t, 0x2F0>, 4> client_card_counts;
};

class Server : public std::enable_shared_from_this<Server> {
public:
  explicit Server(std::shared_ptr<ServerBase> base);
  void init();
  std::shared_ptr<ServerBase> base();
  std::shared_ptr<const ServerBase> base() const;

  int8_t get_winner_team_id() const;

  template <typename T>
  void send(const T& cmd) const {
    if (cmd.header.size != sizeof(cmd) / 4) {
      throw std::logic_error("outbound command size field is incorrect");
    }
    if (cmd.header.subsubcommand == 0x06) {
      this->num_6xB4x06_commands_sent++;
      this->prev_num_6xB4x06_commands_sent = this->num_6xB4x06_commands_sent;
      if (this->num_6xB4x06_commands_sent > 0x100) {
        return;
      }
    }
    this->send(&cmd, cmd.header.size * 4);
  }
  void send(const void* data, size_t size) const;

  void send_commands_for_joining_spectator(Channel& ch) const;

  __attribute__((format(printf, 2, 3)))
  void send_debug_message_printf(const char* fmt, ...) const;
  __attribute__((format(printf, 2, 3)))
  void send_info_message_printf(const char* fmt, ...) const; 
  void send_debug_command_received_message(
      uint8_t client_id, uint8_t subsubcommand, const char* description) const;
  void send_debug_command_received_message(
      uint8_t subsubcommand, const char* description) const;
  void send_debug_message_if_error_code_nonzero(
      uint8_t client_id, int32_t error_code) const;

  void add_team_exp(uint8_t team_id, int32_t exp);
  bool advance_battle_phase();
  void action_phase_after();
  void draw_phase_before();
  std::shared_ptr<const DataIndex::CardEntry> definition_for_card_ref(uint16_t card_ref) const;
  std::shared_ptr<Card> card_for_set_card_ref(uint16_t card_ref);
  std::shared_ptr<const Card> card_for_set_card_ref(uint16_t card_ref) const;
  uint16_t card_id_for_card_ref(uint16_t card_ref) const;
  bool card_ref_is_empty_or_has_valid_card_id(uint16_t card_ref) const;
  bool check_for_battle_end();
  void check_for_destroyed_cards_and_send_6xB4x05_6xB4x02();
  bool check_presence_entry(uint8_t client_id) const;
  void clear_player_flags_after_dice_phase();
  void compute_all_map_occupied_bits();
  void compute_team_dice_boost(uint8_t team_id);
  void copy_player_states_to_prev_states();
  std::shared_ptr<const DataIndex::CardEntry> definition_for_card_id(uint16_t card_id) const;
  void destroy_cards_with_zero_hp();
  void determine_first_team_turn();
  void dice_phase_after();
  void set_phase_before();
  void draw_phase_after();
  void dice_phase_before();
  void end_attack_list_for_client(uint8_t client_id);
  void end_action_phase();
  bool enqueue_attack_or_defense(uint8_t client_id, ActionState* pa);
  BattlePhase get_battle_phase() const;
  ActionSubphase get_current_action_subphase() const;
  uint8_t get_current_team_turn() const;
  std::shared_ptr<PlayerState> get_player_state(uint8_t client_id);
  std::shared_ptr<const PlayerState> get_player_state(uint8_t client_id) const;
  uint32_t get_random(uint32_t max);
  float get_random_float_0_1();
  uint32_t get_round_num() const;
  SetupPhase get_setup_phase() const;
  uint32_t get_should_copy_prev_states_to_current_states() const;
  bool is_registration_complete() const;
  void move_phase_after();
  void action_phase_before();
  void send_6xB4x1C_names_update();
  int8_t send_6xB4x33_remove_ally_atk_if_needed(const ActionState& pa);
  void send_all_state_updates();
  void send_set_card_updates_and_6xB4x04_if_needed();
  void set_battle_ended();
  void set_battle_started();
  void set_client_id_ready_to_advance_phase(uint8_t client_id);
  void set_phase_after();
  void move_phase_before();
  void set_player_deck_valid(uint8_t client_id);
  void setup_and_start_battle();
  void update_battle_state_flags_and_send_6xB4x03_if_needed(
      bool always_send = false);
  bool update_registration_phase();
  void on_server_data_input(const std::string& data);
  void handle_6xB3x0B_mulligan_hand(const std::string& data);
  void handle_6xB3x0C_end_mulligan_phase(const std::string& data);
  void handle_6xB3x0D_end_non_action_phase(const std::string& data);
  void handle_6xB3x0E_discard_card_from_hand(const std::string& data);
  void handle_6xB3x0F_set_card_from_hand(const std::string& data);
  void handle_6xB3x10_move_fc_to_location(const std::string& data);
  void handle_6xB3x11_enqueue_attack_or_defense(const std::string& data);
  void handle_6xB3x12_end_attack_list(const std::string& data);
  void handle_6xB3x13_update_map_during_setup(const std::string& data);
  void handle_6xB3x14_update_deck_during_setup(const std::string& data);
  void handle_6xB3x15_unused_hard_reset_server_state(const std::string& data);
  void handle_6xB3x1B_update_player_name(const std::string& data);
  void handle_6xB3x1D_start_battle(const std::string& data);
  void handle_6xB3x21_end_battle(const std::string& data);
  void handle_6xB3x28_end_defense_list(const std::string& data);
  void handle_6xB3x2B_ignored(const std::string&);
  void handle_6xB3x34_subtract_ally_atk_points(const std::string& data);
  void handle_6xB3x37_client_ready_to_advance_from_starter_roll_phase(const std::string& data);
  void handle_6xB3x3A_ignored(const std::string& data);
  void handle_6xB3x40_map_list_request(const std::string& data);
  void handle_6xB3x41_map_request(const std::string& data);
  void handle_6xB3x48_end_turn(const std::string& data);
  void handle_6xB3x49_card_counts(const std::string& data);
  void compute_losing_team_id_and_add_winner_flags(uint32_t flags);
  uint32_t get_team_exp(uint8_t team_id) const;
  uint32_t send_6xB4x06_if_card_ref_invalid(
      uint16_t card_ref, int16_t negative_value);
  void unknown_8023EEF4();
  void execute_bomb_assist_effect();
  void replace_targets_due_to_destruction_or_conditions(
      ActionState* as);
  bool any_target_exists_for_attack(const ActionState& as);
  uint8_t get_current_team_turn2() const;
  void unknown_8023EE48();
  void unknown_8023EE80();
  void unknown_802402F4();
  void send_6xB4x39() const;
  void send_6xB4x05(); // Recomputes the map occupied bits, so can't be const
  void send_6xB4x02_for_all_players_if_needed(bool always_send = false);
  void send_6xB4x50_trap_tile_locations() const;

  G_UpdateDecks_GC_Ep3_6xB4x07 prepare_6xB4x07_decks_update() const;
  G_SetPlayerNames_GC_Ep3_6xB4x1C prepare_6xB4x1C_names_update() const;
  static std::string prepare_6xB6x41_map_definition(
      std::shared_ptr<const DataIndex::MapEntry> map);
  G_SetTrapTileLocations_GC_Ep3_6xB4x50 prepare_6xB4x50_trap_tile_locations() const;

  std::vector<std::shared_ptr<Card>> const_cast_set_cards_v(
      const std::vector<std::shared_ptr<const Card>>& cards);
private:
  typedef void (Server::*handler_t)(const std::string&);
  static const std::unordered_map<uint8_t, handler_t> subcommand_handlers;

  std::weak_ptr<ServerBase> w_base;
  std::shared_ptr<const DataIndex::MapEntry> last_chosen_map;

public:
  uint32_t battle_finished;
  uint32_t battle_in_progress;
  uint32_t round_num;
  BattlePhase battle_phase;
  uint8_t first_team_turn;
  uint8_t current_team_turn1;
  SetupPhase setup_phase;
  RegistrationPhase registration_phase;
  ActionSubphase action_subphase;
  uint8_t current_team_turn2;
  ActionState pending_attacks[0x20];
  uint32_t num_pending_attacks;
  parray<uint8_t, 4> client_done_enqueuing_attacks;
  parray<uint8_t, 4> player_ready_to_end_phase;
  std::shared_ptr<PSOV2Encryption> random_crypt;
  uint32_t unknown_a10;
  uint32_t overall_time_expired;
  // Note: In the original implementation, this is a uint32_t and is measured in
  // seconds. In our environment, the simplest implementation uses now(), which
  // returns microseconds, so we use a uint64_t instead.
  uint64_t battle_start_usecs;
  uint32_t should_copy_prev_states_to_current_states;
  std::shared_ptr<CardSpecial> card_special;
  std::shared_ptr<StateFlags> state_flags;
  std::shared_ptr<PlayerState> player_states[4];
  parray<uint32_t, 4> clients_done_in_mulligan_phase;
  uint32_t num_pending_attacks_with_cards;
  std::shared_ptr<Card> attack_cards[0x20];
  ActionState pending_attacks_with_cards[0x20];
  uint32_t unknown_a14;
  uint32_t unknown_a15;
  parray<uint32_t, 4> defense_list_ended_for_client;
  std::shared_ptr<AssistServer> assist_server;
  uint16_t next_assist_card_set_number;
  std::shared_ptr<RulerServer> ruler_server;
  parray<parray<parray<uint8_t, 2>, 2>, 5> warp_positions; // Array indexes are (type, end, x/y)
  parray<int16_t, 2> team_exp;
  parray<int16_t, 2> team_dice_boost;
  parray<uint32_t, 2> team_client_count;
  parray<uint32_t, 2> team_num_ally_fcs_destroyed;
  parray<uint32_t, 2> team_num_cards_destroyed;
  uint32_t hard_reset_flag;
  uint8_t tournament_flag;
  parray<uint8_t, 5> num_trap_tiles_of_type;
  parray<uint8_t, 5> chosen_trap_tile_index_of_type;
  parray<parray<parray<uint8_t, 2>, 8>, 5> trap_tile_locs;
  ActionState pb_action_states[4];
  parray<uint8_t, 4> has_done_pb;
  parray<parray<uint8_t, 4>, 4> has_done_pb_with_client;
  mutable uint32_t num_6xB4x06_commands_sent;
  mutable uint32_t prev_num_6xB4x06_commands_sent;
};



} // namespace Episode3
