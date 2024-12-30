#pragma once

#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>

#include <memory>
#include <phosg/Strings.hh>
#include <unordered_set>

#include "Client.hh"
#include "CommandFormats.hh"
#include "FunctionCompiler.hh"
#include "Lobby.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "Server.hh"
#include "Text.hh"

extern const std::unordered_set<uint32_t> v2_crypt_initial_client_commands;
extern const std::unordered_set<uint32_t> v3_crypt_initial_client_commands;
extern const std::unordered_set<std::string> bb_crypt_initial_client_commands;

constexpr size_t V3_V4_QUEST_LOAD_MAX_CHUNKS_IN_FLIGHT = 4;

// TODO: Many of these functions should take a Channel& instead of a
// shared_ptr<Client>. Refactor functions appropriately.

// Note: There are so many versions of this function for a few reasons:
// - There are a lot of different target types (sometimes we want to send a
//   command to one client, sometimes to everyone in a lobby, etc.)
// - For the const void* versions, the data and size arguments should not be
//   independently optional - this can lead to bugs where a non-null data
//   pointer is given but size is accidentally not given (e.g. if the type of
//   data in the calling function is changed from string to void*).

template <typename CmdT>
void send_or_enqueue_command(std::shared_ptr<Client> c, uint16_t command, uint32_t flag, const CmdT& cmd) {
  if (c->game_join_command_queue) {
    c->log.info("Client not ready to receive game commands; adding to queue");
    auto& q_cmd = c->game_join_command_queue->emplace_back();
    q_cmd.command = command;
    q_cmd.flag = flag;
    // TODO: It'd be nice to avoid this copy. Maybe take in a pointer to cmd
    // and move it into q_cmd somehow, so q_cmd can free it when needed?
    q_cmd.data.assign(reinterpret_cast<const char*>(&cmd), sizeof(cmd));
  } else {
    send_command(c, command, flag, &cmd, sizeof(cmd));
  }
}

void send_command(std::shared_ptr<Client> c, uint16_t command,
    uint32_t flag, const std::vector<std::pair<const void*, size_t>>& blocks);

void send_command(std::shared_ptr<Client> c, uint16_t command,
    uint32_t flag, const void* data, size_t size);

inline void send_command(std::shared_ptr<Client> c, uint16_t command,
    uint32_t flag) {
  send_command(c, command, flag, nullptr, 0);
}

void send_command_excluding_client(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, uint16_t command, uint32_t flag,
    const void* data, size_t size);

inline void send_command_excluding_client(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, uint16_t command, uint32_t flag) {
  send_command_excluding_client(l, c, command, flag, nullptr, 0);
}

void send_command_if_not_loading(std::shared_ptr<Lobby> l,
    uint16_t command, uint32_t flag, const void* data, size_t size);
inline void send_command_if_not_loading(std::shared_ptr<Lobby> l,
    uint16_t command, uint32_t flag, const std::string& data) {
  send_command_if_not_loading(l, command, flag, data.data(), data.size());
}
template <typename StructT>
inline void send_command_if_not_loading(std::shared_ptr<Lobby> l,
    uint16_t command, uint32_t flag, const StructT& data) {
  send_command_if_not_loading(l, command, flag, &data, sizeof(data));
}

void send_command(std::shared_ptr<Lobby> l, uint16_t command, uint32_t flag,
    const void* data, size_t size);

inline void send_command(std::shared_ptr<Lobby> l, uint16_t command, uint32_t flag) {
  send_command(l, command, flag, nullptr, 0);
}

void send_command(std::shared_ptr<ServerState> s, uint16_t command,
    uint32_t flag, const void* data, size_t size);

inline void send_command(std::shared_ptr<ServerState> s, uint16_t command,
    uint32_t flag) {
  send_command(s, command, flag, nullptr, 0);
}

template <typename TargetT, typename StructT>
static void send_command_t(std::shared_ptr<TargetT> c, uint16_t command,
    uint32_t flag, const StructT& data) {
  send_command(c, command, flag, &data, sizeof(data));
}

template <typename TargetT>
static void send_command(std::shared_ptr<TargetT> c, uint16_t command,
    uint32_t flag, const std::string& data) {
  send_command(c, command, flag, data.data(), data.size());
}

template <typename TargetT, typename StructT>
void send_command_vt(std::shared_ptr<TargetT> c, uint16_t command,
    uint32_t flag, const std::vector<StructT>& data) {
  send_command(c, command, flag, data.data(), data.size() * sizeof(StructT));
}

template <typename StructT>
void send_command_vt(Channel& ch, uint16_t command, uint32_t flag,
    const std::vector<StructT>& data) {
  ch.send(command, flag, data.data(), data.size() * sizeof(StructT));
}

template <typename TargetT, typename StructT, typename EntryT>
void send_command_t_vt(std::shared_ptr<TargetT> c, uint16_t command,
    uint32_t flag, const StructT& data, const std::vector<EntryT>& array_data) {
  std::string all_data(reinterpret_cast<const char*>(&data), sizeof(StructT));
  all_data.append(reinterpret_cast<const char*>(array_data.data()),
      array_data.size() * sizeof(EntryT));
  send_command(c, command, flag, all_data.data(), all_data.size());
}

void send_command_with_header(Channel& c, const void* data, size_t size);

enum SendServerInitFlag {
  IS_INITIAL_CONNECTION = 0x01,
  USE_SECONDARY_MESSAGE = 0x02,
};

S_ServerInitWithAfterMessageT_DC_PC_V3_02_17_91_9B<0xB4>
prepare_server_init_contents_console(
    uint32_t server_key, uint32_t client_key, uint8_t flags);
S_ServerInitWithAfterMessageT_BB_03_9B<0xB4>
prepare_server_init_contents_bb(
    const parray<uint8_t, 0x30>& server_key,
    const parray<uint8_t, 0x30>& client_key,
    uint8_t flags);
void send_server_init(std::shared_ptr<Client> c, uint8_t flags);
void send_update_client_config(std::shared_ptr<Client> c, bool always_send);

void empty_function_call_response_handler(uint32_t, uint32_t);

void send_quest_buffer_overflow(std::shared_ptr<Client> c);
void prepare_client_for_patches(std::shared_ptr<Client> c, std::function<void()> on_complete);
std::string prepare_send_function_call_data(
    std::shared_ptr<const CompiledFunctionCode> code,
    const std::unordered_map<std::string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset,
    bool use_encrypted_format);
void send_function_call(
    Channel& ch,
    const Client::Config& client_config,
    std::shared_ptr<const CompiledFunctionCode> code,
    const std::unordered_map<std::string, uint32_t>& label_writes = {},
    const void* suffix_data = nullptr,
    size_t suffix_size = 0,
    uint32_t checksum_addr = 0,
    uint32_t checksum_size = 0,
    uint32_t override_relocations_offset = 0);
void send_function_call(
    std::shared_ptr<Client> c,
    std::shared_ptr<const CompiledFunctionCode> code,
    const std::unordered_map<std::string, uint32_t>& label_writes = {},
    const void* suffix_data = nullptr,
    size_t suffix_size = 0,
    uint32_t checksum_addr = 0,
    uint32_t checksum_size = 0,
    uint32_t override_relocations_offset = 0);
bool send_protected_command(std::shared_ptr<Client> c, const void* data, size_t size, bool echo_to_lobby);

void send_reconnect(std::shared_ptr<Client> c, uint32_t address, uint16_t port);
void send_pc_console_split_reconnect(
    std::shared_ptr<Client> c,
    uint32_t address,
    uint16_t pc_port,
    uint16_t console_port);

void send_client_init_bb(std::shared_ptr<Client> c, uint32_t error);
void send_system_file_bb(std::shared_ptr<Client> c);
void send_player_preview_bb(std::shared_ptr<Client> c, int8_t character_index, const PlayerDispDataBBPreview* preview);
void send_accept_client_checksum_bb(std::shared_ptr<Client> c);
void send_guild_card_header_bb(std::shared_ptr<Client> c);
void send_guild_card_chunk_bb(std::shared_ptr<Client> c, size_t chunk_index);
void send_stream_file_index_bb(std::shared_ptr<Client> c);
void send_stream_file_chunk_bb(std::shared_ptr<Client> c, uint32_t chunk_index);
void send_approve_player_choice_bb(std::shared_ptr<Client> c);
void send_complete_player_bb(std::shared_ptr<Client> c);

void send_message_box(std::shared_ptr<Client> c, const std::string& text);
void send_ep3_timed_message_box(Channel& ch, uint32_t frames, const std::string& text);
void send_lobby_name(std::shared_ptr<Client> c, const std::string& text);
void send_quest_info(std::shared_ptr<Client> c, const std::string& text, uint8_t flag, bool is_download_quest);
void send_lobby_message_box(std::shared_ptr<Client> c, const std::string& text, bool left_side_on_bb = false);
void send_ship_info(std::shared_ptr<Client> c, const std::string& text);
void send_ship_info(Channel& ch, const std::string& text);
void send_text_message(Channel& ch, const std::string& text);
void send_text_message(std::shared_ptr<Client> c, const std::string& text);
void send_text_message(std::shared_ptr<Lobby> l, const std::string& text);
void send_text_message(std::shared_ptr<ServerState> s, const std::string& text);
void send_scrolling_message_bb(std::shared_ptr<Client> c, const std::string& text);
void send_text_or_scrolling_message(std::shared_ptr<Client> c, const std::string& text, const std::string& scrolling);
void send_text_or_scrolling_message(
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> exclude_c, const std::string& text, const std::string& scrolling);
void send_text_or_scrolling_message(std::shared_ptr<ServerState> s, const std::string& text, const std::string& scrolling);

std::string prepare_chat_data(
    Version version,
    uint8_t language,
    uint8_t from_client_id,
    const std::string& from_name,
    const std::string& text,
    char private_flags);
void send_chat_message_from_client(
    Channel& ch,
    const std::string& text,
    char private_flags);
void send_prepared_chat_message(
    std::shared_ptr<Client> c,
    uint32_t from_guild_card_number,
    const std::string& prepared_data);
void send_prepared_chat_message(
    std::shared_ptr<Lobby> l,
    uint32_t from_guild_card_number,
    const std::string& prepared_data);
void send_chat_message(
    std::shared_ptr<Client> c,
    uint32_t from_guild_card_number,
    const std::string& from_name,
    const std::string& text,
    char private_flags);
void send_simple_mail(
    std::shared_ptr<Client> c,
    uint32_t from_guild_card_number,
    const std::string& from_name,
    const std::string& text);
void send_simple_mail(
    std::shared_ptr<ServerState> s,
    uint32_t from_guild_card_number,
    const std::string& from_name,
    const std::string& text);

template <typename TargetT>
__attribute__((format(printf, 2, 3))) void send_text_message_printf(
    TargetT& t, const char* format, ...) {
  va_list va;
  va_start(va, format);
  std::string buf = phosg::string_vprintf(format, va);
  va_end(va);
  return send_text_message(t, buf.c_str());
}

__attribute__((format(printf, 2, 3))) void send_ep3_text_message_printf(
    std::shared_ptr<ServerState> s, const char* format, ...);

void send_info_board(std::shared_ptr<Client> c);

void send_choice_search_choices(std::shared_ptr<Client> c);

void send_card_search_result(
    std::shared_ptr<Client> c,
    std::shared_ptr<Client> result,
    std::shared_ptr<Lobby> result_lobby);

void send_guild_card(
    Channel& ch,
    uint32_t guild_card_number,
    uint64_t xb_user_id,
    const std::string& name,
    const std::string& team_name,
    const std::string& description,
    uint8_t language,
    uint8_t section_id,
    uint8_t char_class);
void send_guild_card(std::shared_ptr<Client> c, std::shared_ptr<Client> source);
void send_menu(std::shared_ptr<Client> c, std::shared_ptr<const Menu> menu, bool is_info_menu = false);
void send_game_menu(
    std::shared_ptr<Client> c,
    bool is_spectator_team_list,
    bool is_tournament_game_list);
void send_quest_menu(
    std::shared_ptr<Client> c,
    const std::vector<std::pair<QuestIndex::IncludeState, std::shared_ptr<const Quest>>>& quests,
    bool is_download_menu);
void send_quest_categories_menu(
    std::shared_ptr<Client> c,
    std::shared_ptr<const QuestIndex> quest_index,
    QuestMenuType menu_type,
    Episode episode);
void send_lobby_list(std::shared_ptr<Client> c);

void send_player_records(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l, std::shared_ptr<Client> joining_client = nullptr);
void send_join_lobby(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);
void send_update_lobby_data_bb(std::shared_ptr<Client> c);
void send_player_join_notification(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l, std::shared_ptr<Client> joining_client);
void send_player_leave_notification(std::shared_ptr<Lobby> l, uint8_t leaving_client_id);
void send_self_leave_notification(std::shared_ptr<Client> c);
void send_get_player_info(std::shared_ptr<Client> c, bool request_extended = false);

void send_execute_item_trade(std::shared_ptr<Client> c, const std::vector<ItemData>& items);
void send_execute_card_trade(std::shared_ptr<Client> c, const std::vector<std::pair<uint32_t, uint32_t>>& card_to_count);

void send_arrow_update(std::shared_ptr<Lobby> l);
void send_unblock_join(std::shared_ptr<Client> c);
void send_resume_game(std::shared_ptr<Lobby> l, std::shared_ptr<Client> ready_client);

enum PlayerStatsChange {
  SUBTRACT_HP = 0,
  SUBTRACT_TP = 1,
  SUBTRACT_MESETA = 2,
  ADD_HP = 3,
  ADD_TP = 4,
};

void send_player_stats_change(std::shared_ptr<Client> c, PlayerStatsChange stat, uint32_t amount);
void send_player_stats_change(Channel& ch, uint16_t client_id, PlayerStatsChange stat, uint32_t amount);
void send_remove_negative_conditions(std::shared_ptr<Client> c);
void send_remove_negative_conditions(Channel& ch, uint16_t client_id);
void send_warp(Channel& ch, uint8_t client_id, uint32_t floor, bool is_private);
void send_warp(std::shared_ptr<Client> c, uint32_t floor, bool is_private);
void send_warp(std::shared_ptr<Lobby> l, uint32_t floor, bool is_private);

void send_ep3_change_music(Channel& ch, uint32_t song);
void send_revive_player(std::shared_ptr<Client> c);

void send_game_join_sync_command(
    std::shared_ptr<Client> c, const void* data, size_t size, uint8_t dc_nte_sc, uint8_t dc_11_2000_sc, uint8_t sc);
void send_game_join_sync_command(
    std::shared_ptr<Client> c, const std::string& data, uint8_t dc_nte_sc, uint8_t dc_11_2000_sc, uint8_t sc);
void send_game_join_sync_command_compressed(
    std::shared_ptr<Client> c,
    const void* data,
    size_t size,
    size_t decompressed_size,
    uint8_t dc_nte_sc,
    uint8_t dc_11_2000_sc,
    uint8_t sc);
void send_game_item_state(std::shared_ptr<Client> c);
void send_game_enemy_state(std::shared_ptr<Client> c);
void send_game_object_state(std::shared_ptr<Client> c);
void send_game_set_state(std::shared_ptr<Client> c);
void send_game_flag_state(std::shared_ptr<Client> c);
void send_game_player_state(std::shared_ptr<Client> to_c, std::shared_ptr<Client> from_c, bool apply_overrides);
void send_drop_item_to_channel(std::shared_ptr<ServerState> s, Channel& ch, const ItemData& item,
    uint8_t source_type, uint8_t floor, const VectorXZF& pos, uint16_t entity_index);
void send_drop_stacked_item_to_channel(
    std::shared_ptr<ServerState> s, Channel& ch, const ItemData& item, uint8_t floor, const VectorXZF& pos);
void send_drop_stacked_item_to_lobby(
    std::shared_ptr<Lobby> l, const ItemData& item, uint8_t floor, const VectorXZF& pos);
void send_pick_up_item_to_client(std::shared_ptr<Client> c, uint8_t client_id, uint32_t id, uint8_t floor);
void send_create_inventory_item_to_client(std::shared_ptr<Client> c, uint8_t client_id, const ItemData& item);
void send_create_inventory_item_to_lobby(std::shared_ptr<Client> c, uint8_t client_id, const ItemData& item, bool exclude_c = false);
void send_destroy_item_to_lobby(std::shared_ptr<Client> c, uint32_t item_id, uint32_t amount, bool exclude_c = false);
void send_destroy_floor_item_to_client(std::shared_ptr<Client> c, uint32_t item_id, uint32_t floor);
void send_item_identify_result(std::shared_ptr<Client> c);
void send_bank(std::shared_ptr<Client> c);
void send_shop(std::shared_ptr<Client> c, uint8_t shop_type);
void send_level_up(std::shared_ptr<Client> c);
void send_give_experience(std::shared_ptr<Client> c, uint32_t amount);
void send_set_exp_multiplier(std::shared_ptr<Lobby> l);
void send_rare_enemy_index_list(std::shared_ptr<Client> c, const std::vector<size_t>& indexes);

void send_quest_function_call(Channel& ch, uint16_t label);
void send_quest_function_call(std::shared_ptr<Client> c, uint16_t label);

void send_ep3_card_list_update(std::shared_ptr<Client> c);
void send_ep3_media_update(
    std::shared_ptr<Client> c,
    uint32_t type,
    uint32_t which,
    const std::string& compressed_data);
void send_ep3_rank_update(std::shared_ptr<Client> c);
void send_ep3_card_battle_table_state(std::shared_ptr<Lobby> l, uint16_t table_number);
void send_ep3_set_context_token(std::shared_ptr<Client> c, uint32_t context_token);

void send_ep3_confirm_tournament_entry(
    std::shared_ptr<Client> c,
    std::shared_ptr<const Episode3::Tournament> t);
void send_ep3_tournament_list(
    std::shared_ptr<Client> c,
    bool is_for_spectator_team_create);
void send_ep3_tournament_entry_list(
    std::shared_ptr<Client> c,
    std::shared_ptr<const Episode3::Tournament> t,
    bool is_for_spectator_team_create);
void send_ep3_tournament_info(
    std::shared_ptr<Client> c,
    std::shared_ptr<const Episode3::Tournament> t);
void send_ep3_set_tournament_player_decks(std::shared_ptr<Client> c);
void send_ep3_tournament_match_result(std::shared_ptr<Lobby> l, uint32_t meseta_reward);

void send_ep3_tournament_details(
    std::shared_ptr<Client> c,
    std::shared_ptr<const Episode3::Tournament> t);
void send_ep3_game_details(
    std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);
void send_ep3_update_game_metadata(std::shared_ptr<Lobby> l);
void send_ep3_card_auction(std::shared_ptr<Lobby> l);
void send_ep3_disband_watcher_lobbies(std::shared_ptr<Lobby> primary_l);

// Pass mask_key = 0 to unmask the command
void set_mask_for_ep3_game_command(void* vdata, size_t size, uint8_t mask_key);

enum class QuestFileType {
  ONLINE = 0,
  DOWNLOAD_WITHOUT_PVR,
  DOWNLOAD_WITH_PVR,
  EPISODE_3,
  GBA_DEMO,
};

void send_open_quest_file(
    std::shared_ptr<Client> c,
    const std::string& quest_name,
    const std::string& filename,
    const std::string& xb_filename,
    uint32_t quest_number,
    QuestFileType type,
    std::shared_ptr<const std::string> contents);
void send_quest_file_chunk(
    std::shared_ptr<Client> c,
    const std::string& filename,
    size_t chunk_index,
    const void* data,
    size_t size,
    bool is_download_quest);
bool send_quest_barrier_if_all_clients_ready(std::shared_ptr<Lobby> l);
bool send_ep3_start_tournament_deck_select_if_all_clients_ready(std::shared_ptr<Lobby> l);

void send_server_time(std::shared_ptr<Client> c);

void send_change_event(std::shared_ptr<Client> c, uint8_t new_event);
void send_change_event(std::shared_ptr<Lobby> l, uint8_t new_event);
void send_change_event(std::shared_ptr<ServerState> s, uint8_t new_event);

void send_team_membership_info(std::shared_ptr<Client> c); // 12EA
void send_update_team_metadata_for_client(std::shared_ptr<Client> c); // 15EA (to all clients in lobby, with only c's data)
void send_all_nearby_team_metadatas_to_client(std::shared_ptr<Client> c, bool is_13EA); // 13EA/15EA (to only c, with all lobby clients' data)
void send_update_team_reward_flags(std::shared_ptr<Client> c); // 1DEA
void send_team_member_list(std::shared_ptr<Client> c); // 09EA
void send_intra_team_ranking(std::shared_ptr<Client> c); // 18EA
void send_team_reward_list(std::shared_ptr<Client> c, bool show_purchased); // 19EA, 1AEA
void send_cross_team_ranking(std::shared_ptr<Client> c); // 1CEA
