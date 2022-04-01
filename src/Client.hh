#pragma once

#include <netinet/in.h>

#include <memory>

#include "License.hh"
#include "Player.hh"
#include "PSOEncryption.hh"

#include "Text.hh"



extern const uint64_t CLIENT_CONFIG_MAGIC;



enum class ServerBehavior {
  SPLIT_RECONNECT = 0,
  LOGIN_SERVER,
  LOBBY_SERVER,
  DATA_SERVER_BB,
  PATCH_SERVER,
  PROXY_SERVER,
};

struct ClientConfig {
  uint64_t magic;
  uint8_t bb_game_state;
  uint8_t bb_player_index;
  uint16_t flags;
  uint32_t proxy_destination_address;
  uint16_t proxy_destination_port;
  parray<uint8_t, 0x0E> unused;
  parray<uint8_t, 0x08> unused_bb_only;
} __attribute__((packed));

struct Client {
  // License & account
  std::shared_ptr<const License> license;
  GameVersion version;

  // Note: these fields are included in the client config. On GC, the client
  // config can be up to 0x20 bytes; on BB it can be 0x28 bytes. We don't use
  // all of that space.
  uint8_t bb_game_state;
  uint8_t bb_player_index;
  uint16_t flags;

  // Encryption
  std::unique_ptr<PSOEncryption> crypt_in;
  std::unique_ptr<PSOEncryption> crypt_out;

  // Network
  struct sockaddr_storage local_addr;
  struct sockaddr_storage remote_addr;
  struct bufferevent* bev;
  struct sockaddr_storage next_connection_addr;
  ServerBehavior server_behavior;
  bool is_virtual_connection;
  bool should_disconnect;
  uint32_t proxy_destination_address;
  uint16_t proxy_destination_port;

  // Timing & menus
  uint64_t play_time_begin; // time of connection (used for incrementing play time on BB)
  uint64_t last_recv_time; // time of last data received
  uint64_t last_send_time; // time of last data sent

  // Lobby/positioning
  uint32_t area; // which area is the client in?
  uint32_t lobby_id; // which lobby is this person in?
  uint8_t lobby_client_id; // which client number is this person?
  uint8_t lobby_arrow_color; // lobby arrow color ID
  Player player;

  // Miscellaneous (used by chat commands)
  uint32_t next_exp_value; // next EXP value to give
  int16_t override_section_id; // valid if >= 0
  bool infinite_hp; // cheats enabled
  bool infinite_tp; // cheats enabled
  bool can_chat;
  std::string pending_bb_save_username;
  uint8_t pending_bb_save_player_index;

  Client(struct bufferevent* bev, GameVersion version,
      ServerBehavior server_behavior);

  ClientConfig export_config() const;
  void import_config(const ClientConfig& cc);
};
