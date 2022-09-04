#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>

#include <memory>
#include <phosg/Strings.hh>
#include <unordered_set>

#include "Client.hh"
#include "Lobby.hh"
#include "Server.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "Text.hh"
#include "CommandFormats.hh"
#include "FunctionCompiler.hh"



extern const std::unordered_set<uint32_t> v2_crypt_initial_client_commands;
extern const std::unordered_set<uint32_t> v3_crypt_initial_client_commands;
extern const std::unordered_set<std::string> bb_crypt_initial_client_commands;

// TODO: Many of these functions should take a Channel& instead of a
// shared_ptr<Client>. Refactor functions appropriately.

// Note: There are so many versions of this function for a few reasons:
// - There are a lot of different target types (sometimes we want to send a
//   command to one client, sometimes to everyone in a lobby, etc.)
// - For the const void* versions, the data and size arguments should not be
//   independently optional - this can lead to bugs where a non-null data
//   pointer is given but size is accidentally not given (e.g. if the type of
//   data in the calling function is changed from string to void*).

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

S_ServerInit_DC_PC_V3_02_17_91_9B prepare_server_init_contents_console(
    uint32_t server_key, uint32_t client_key, uint8_t flags);
S_ServerInit_BB_03_9B prepare_server_init_contents_bb(
    const parray<uint8_t, 0x30>& server_key,
    const parray<uint8_t, 0x30>& client_key,
    uint8_t flags);
void send_server_init(
    std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c,
    uint8_t flags);
void send_update_client_config(std::shared_ptr<Client> c);

void send_function_call(
    std::shared_ptr<Client> c,
    std::shared_ptr<CompiledFunctionCode> code,
    const std::unordered_map<std::string, uint32_t>& label_writes = {},
    const std::string& suffix = "",
    uint32_t checksum_addr = 0,
    uint32_t checksum_size = 0);

void send_reconnect(std::shared_ptr<Client> c, uint32_t address, uint16_t port);
void send_pc_console_split_reconnect(
    std::shared_ptr<Client> c,
    uint32_t address,
    uint16_t pc_port,
    uint16_t console_port);

void send_client_init_bb(std::shared_ptr<Client> c, uint32_t error);
void send_team_and_key_config_bb(std::shared_ptr<Client> c);
void send_player_preview_bb(std::shared_ptr<Client> c, uint8_t player_index,
    const PlayerDispDataBBPreview* preview);
void send_accept_client_checksum_bb(std::shared_ptr<Client> c);
void send_guild_card_header_bb(std::shared_ptr<Client> c);
void send_guild_card_chunk_bb(std::shared_ptr<Client> c, size_t chunk_index);
void send_stream_file_index_bb(std::shared_ptr<Client> c);
void send_stream_file_chunk_bb(std::shared_ptr<Client> c, uint32_t chunk_index);
void send_approve_player_choice_bb(std::shared_ptr<Client> c);
void send_complete_player_bb(std::shared_ptr<Client> c);

void send_enter_directory_patch(std::shared_ptr<Client> c, const std::string& dir);
void send_patch_file(std::shared_ptr<Client> c, std::shared_ptr<PatchFileIndex::File> f);

void send_message_box(std::shared_ptr<Client> c, const std::u16string& text);
void send_lobby_name(std::shared_ptr<Client> c, const std::u16string& text);
void send_quest_info(std::shared_ptr<Client> c, const std::u16string& text,
    bool is_download_quest);
void send_lobby_message_box(std::shared_ptr<Client> c, const std::u16string& text);
void send_ship_info(std::shared_ptr<Client> c, const std::u16string& text);
void send_ship_info(Channel& ch, const std::u16string& text);
void send_text_message(Channel& ch, const std::u16string& text);
void send_text_message(std::shared_ptr<Client> c, const std::u16string& text);
void send_text_message(std::shared_ptr<Lobby> l, const std::u16string& text);
void send_text_message(std::shared_ptr<ServerState> l, const std::u16string& text);
void send_chat_message(Channel& ch, const std::u16string& text);
void send_chat_message(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const std::u16string& from_name, const std::u16string& text);
void send_simple_mail(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const std::u16string& from_name, const std::u16string& text);

template <typename TargetT>
__attribute__((format(printf, 2, 3))) void send_text_message_printf(
    TargetT& t, const char* format, ...) {
  va_list va;
  va_start(va, format);
  std::string buf = string_vprintf(format, va);
  va_end(va);
  std::u16string decoded = decode_sjis(buf);
  return send_text_message(t, decoded.c_str());
}

void send_info_board(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);

void send_card_search_result(
    std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c,
    std::shared_ptr<Client> result,
    std::shared_ptr<Lobby> result_lobby);

void send_guild_card(std::shared_ptr<Client> c, std::shared_ptr<Client> source);
void send_menu(std::shared_ptr<Client> c, const std::u16string& menu_name,
    uint32_t menu_id, const std::vector<MenuItem>& items, bool is_info_menu = false);
void send_game_menu(std::shared_ptr<Client> c, std::shared_ptr<ServerState> s);
void send_quest_menu(std::shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<std::shared_ptr<const Quest>>& quests, bool is_download_menu);
void send_quest_menu(std::shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu);
void send_lobby_list(std::shared_ptr<Client> c, std::shared_ptr<ServerState> s);

void send_join_lobby(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);
void send_player_join_notification(std::shared_ptr<Client> c,
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> joining_client);
void send_player_leave_notification(std::shared_ptr<Lobby> l,
    uint8_t leaving_client_id);
void send_self_leave_notification(std::shared_ptr<Client> c);
void send_get_player_info(std::shared_ptr<Client> c);

void send_execute_item_trade(std::shared_ptr<Client> c,
    const std::vector<ItemData>& items);

void send_arrow_update(std::shared_ptr<Lobby> l);
void send_resume_game(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> ready_client);

enum PlayerStatsChange {
  SUBTRACT_HP = 0,
  SUBTRACT_TP = 1,
  SUBTRACT_MESETA = 2,
  ADD_HP = 3,
  ADD_TP = 4,
};

void send_player_stats_change(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c,
    PlayerStatsChange which, uint32_t amount);
void send_warp(Channel& ch, uint8_t client_id, uint32_t area);
void send_warp(std::shared_ptr<Client> c, uint32_t area);

void send_ep3_change_music(std::shared_ptr<Client> c, uint32_t song);
void send_set_player_visibility(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, bool visible);
void send_revive_player(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c);

void send_drop_item(Channel& ch, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float z, uint16_t request_id);
void send_drop_item(std::shared_ptr<Lobby> l, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float z, uint16_t request_id);
void send_drop_stacked_item(std::shared_ptr<Lobby> l, const ItemData& item,
    uint8_t area, float x, float z);
void send_pick_up_item(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c, uint32_t id,
    uint8_t area);
void send_create_inventory_item(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c,
    const ItemData& item);
void send_destroy_item(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c,
    uint32_t item_id, uint32_t amount);
void send_bank(std::shared_ptr<Client> c);
void send_shop(std::shared_ptr<Client> c, uint8_t shop_type);
void send_level_up(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c);
void send_give_experience(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c,
    uint32_t amount);
void send_ep3_card_list_update(
    std::shared_ptr<ServerState> s, std::shared_ptr<Client> c);
void send_ep3_rank_update(std::shared_ptr<Client> c);
void send_ep3_map_list(
    std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l);
void send_ep3_map_data(
    std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l, uint32_t map_id);

enum class QuestFileType {
  ONLINE = 0,
  DOWNLOAD,
  EPISODE_3,
  GBA_DEMO,
};

void send_quest_file(std::shared_ptr<Client> c, const std::string& quest_name,
    const std::string& basename, const std::string& contents,
    QuestFileType type);

void send_server_time(std::shared_ptr<Client> c);

void send_change_event(std::shared_ptr<Client> c, uint8_t new_event);
void send_change_event(std::shared_ptr<Lobby> l, uint8_t new_event);
void send_change_event(std::shared_ptr<ServerState> s, uint8_t new_event);
