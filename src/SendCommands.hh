#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>

#include <memory>
#include <phosg/Strings.hh>

#include "Client.hh"
#include "Lobby.hh"
#include "Server.hh"
#include "Menu.hh"
#include "Quest.hh"
#include "Text.hh"



void send_command(struct bufferevent* bev, GameVersion version,
    PSOEncryption* crypt, uint16_t command, uint32_t flag = 0,
    const void* data = nullptr, size_t size = 0, const char* name_str = nullptr);

void send_command(std::shared_ptr<Client> c, uint16_t command,
    uint32_t flag = 0, const void* data = nullptr, size_t size = 0);

void send_command_excluding_client(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, uint16_t command, uint32_t flag = 0,
    const void* data = nullptr, size_t size = 0);

void send_command(std::shared_ptr<Lobby> l, uint16_t command, uint32_t flag = 0,
    const void* data = nullptr, size_t size = 0);

void send_command(std::shared_ptr<ServerState> s, uint16_t command,
    uint32_t flag = 0, const void* data = nullptr, size_t size = 0);

template <typename TARGET, typename STRUCT>
void send_command(std::shared_ptr<TARGET> c, uint16_t command, uint32_t flag,
    const STRUCT& data) {
  send_command(c, command, flag, &data, sizeof(data));
}

template <typename TARGET>
void send_command(std::shared_ptr<TARGET> c, uint16_t command, uint32_t flag,
    const std::string& data) {
  send_command(c, command, flag, data.data(), data.size());
}

template <typename TARGET, typename STRUCT>
void send_command(std::shared_ptr<TARGET> c, uint16_t command, uint32_t flag,
    const std::vector<STRUCT>& data) {
  send_command(c, command, flag, data.data(), data.size() * sizeof(STRUCT));
}

template <typename TARGET, typename STRUCT, typename ENTRY>
void send_command(std::shared_ptr<TARGET> c, uint16_t command, uint32_t flag,
    const STRUCT& data, const std::vector<ENTRY>& array_data) {
  std::string all_data(reinterpret_cast<const char*>(&data), sizeof(STRUCT));
  all_data.append(reinterpret_cast<const char*>(array_data.data()),
      array_data.size() * sizeof(ENTRY));
  send_command(c, command, flag, all_data.data(), all_data.size());
}



struct ServerInitCommand_GC_02_17 {
  char copyright[0x40];
  uint32_t server_key;
  uint32_t client_key;
  char after_message[200];
} __attribute__((packed));

std::string prepare_server_init_contents_dc_pc_gc(
    bool initial_connection, uint32_t server_key, uint32_t client_key);
void send_server_init(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    bool initial_connection);
void send_update_client_config(std::shared_ptr<Client> c);

void send_reconnect(std::shared_ptr<Client> c, uint32_t address, uint16_t port);
void send_pc_gc_split_reconnect(std::shared_ptr<Client> c, uint32_t address,
    uint16_t pc_port, uint16_t gc_port);

void send_client_init_bb(std::shared_ptr<Client> c, uint32_t error);
void send_team_and_key_config_bb(std::shared_ptr<Client> c);
void send_player_preview_bb(std::shared_ptr<Client> c, uint8_t player_index,
    const PlayerDispDataBBPreview* preview);
void send_accept_client_checksum_bb(std::shared_ptr<Client> c);
void send_guild_card_header_bb(std::shared_ptr<Client> c);
void send_guild_card_chunk_bb(std::shared_ptr<Client> c, size_t chunk_index);
void send_stream_file_bb(std::shared_ptr<Client> c);
void send_approve_player_choice_bb(std::shared_ptr<Client> c);
void send_complete_player_bb(std::shared_ptr<Client> c);

void send_check_directory_patch(std::shared_ptr<Client> c, const char* dir);

void send_message_box(std::shared_ptr<Client> c, const char16_t* text);
void send_lobby_name(std::shared_ptr<Client> c, const char16_t* text);
void send_quest_info(std::shared_ptr<Client> c, const char16_t* text);
void send_lobby_message_box(std::shared_ptr<Client> c, const char16_t* text);
void send_ship_info(std::shared_ptr<Client> c, const char16_t* text);
void send_text_message(std::shared_ptr<Client> c, const char16_t* text);
void send_text_message(std::shared_ptr<Lobby> l, const char16_t* text);
void send_text_message(std::shared_ptr<ServerState> l, const char16_t* text);
void send_chat_message(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const char16_t* from_name, const char16_t* text);
void send_simple_mail(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const char16_t* from_name, const char16_t* text);

template <typename TARGET>
__attribute__((format(printf, 2, 3))) void send_text_message_printf(
    std::shared_ptr<TARGET> t, const char* format, ...) {
  va_list va;
  va_start(va, format);
  std::string buf = string_vprintf(format, va);
  va_end(va);
  std::u16string decoded = decode_sjis(buf);
  return send_text_message(t, decoded.c_str());
}

void send_info_board(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);

void send_card_search_result(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    std::shared_ptr<Client> result, std::shared_ptr<Lobby> result_lobby);

void send_guild_card(std::shared_ptr<Client> c, std::shared_ptr<Client> source);
void send_menu(std::shared_ptr<Client> c, const char16_t* menu_name,
    uint32_t menu_id, const std::vector<MenuItem>& items, bool is_info_menu);
void send_game_menu(std::shared_ptr<Client> c, std::shared_ptr<ServerState> s);
void send_quest_menu(std::shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<std::shared_ptr<const Quest>>& quests, bool is_download_menu);
void send_quest_menu(std::shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu);
void send_lobby_list(std::shared_ptr<Client> c, std::shared_ptr<ServerState> s);

struct JoinGameCommand_GC_64 {
  uint32_t variations[0x20];
  PlayerLobbyDataGC lobby_data[4];
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp; // guess; putting 0 here causes no movement messages to be sent
  uint8_t difficulty;
  uint8_t battle_mode;
  uint8_t event;
  uint8_t section_id;
  uint8_t challenge_mode;
  uint32_t rare_seed;
  uint32_t episode; // for PSOPC, this must be 0x00000100
  struct {
    PlayerInventory inventory;
    PlayerDispDataPCGC disp;
  } player[4]; // only used on ep3
} __attribute__((packed));

void send_join_lobby(std::shared_ptr<Client> c, std::shared_ptr<Lobby> l);
void send_player_join_notification(std::shared_ptr<Client> c,
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> joining_client);
void send_player_leave_notification(std::shared_ptr<Lobby> l,
    uint8_t leaving_client_id);
void send_get_player_info(std::shared_ptr<Client> c);

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
void send_warp(std::shared_ptr<Client> c, uint32_t area);

void send_ep3_change_music(std::shared_ptr<Client> c, uint32_t song);
void send_set_player_visibility(std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, bool visible);
void send_revive_player(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c);

void send_drop_item(std::shared_ptr<Lobby> l, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float y, uint16_t request_id);
void send_drop_stacked_item(std::shared_ptr<Lobby> l, const ItemData& item,
    uint8_t area, float x, float y);
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
void send_ep3_card_list_update(std::shared_ptr<Client> c);
void send_ep3_rank_update(std::shared_ptr<Client> c);
void send_ep3_map_list(std::shared_ptr<Lobby> l);
void send_ep3_map_data(std::shared_ptr<Lobby> l, uint32_t map_id);

void send_quest_file(std::shared_ptr<Client> c, const std::string& basename,
    const std::string& contents, bool is_download_quest, bool is_ep3_quest);

void send_server_time(std::shared_ptr<Client> c);

void send_change_event(std::shared_ptr<Client> c, uint8_t new_event);
void send_change_event(std::shared_ptr<Lobby> l, uint8_t new_event);
void send_change_event(std::shared_ptr<ServerState> s, uint8_t new_event);
