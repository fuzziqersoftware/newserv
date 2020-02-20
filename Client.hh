#pragma once

#include <netinet/in.h>

#include <memory>
#include <phosg/Concurrency.hh>

#include "License.hh"
#include "Player.hh"
#include "PSOEncryption.hh"



enum class ServerBehavior {
  SplitReconnect = 0,
  LoginServer,
  LobbyServer,
  DataServerBB,
  PatchServer,
};

struct ClientConfig {
  uint32_t magic; // must be set to 0x48615467
  uint8_t bb_game_state; // status of client connecting on BB
  uint8_t bb_player_index; // selected char
  uint16_t flags; // just in case we lose them somehow between connections
  uint16_t ports[4]; // used by shipgate clients
  uint32_t unused[4];
};

struct ClientConfigBB {
  ClientConfig cfg;
  uint32_t unused[2];
};

struct Client {
  // license & account
  std::shared_ptr<const License> license;
  char16_t name[0x20];
  ClientConfigBB config;
  GameVersion version;
  uint16_t flags;

  // encryption
  std::unique_ptr<PSOEncryption> crypt_in;
  std::unique_ptr<PSOEncryption> crypt_out;

  // network
  struct sockaddr_storage local_addr;
  struct sockaddr_storage remote_addr;
  struct bufferevent* bev;
  struct sockaddr_storage next_connection_addr;
  ServerBehavior server_behavior;
  bool should_disconnect;
  std::string recv_buffer;

  // timing & menus
  uint64_t play_time_begin; // time of connection (used for incrementing play time on BB)
  uint64_t last_recv_time; // time of last data received
  uint64_t last_send_time; // time of last data sent

  // lobby/positioning
  uint32_t area; // which area is the client in?
  uint32_t lobby_id; // which lobby is this person in?
  uint8_t lobby_client_id; // which client number is this person?
  uint8_t lobby_arrow_color; // lobby arrow color ID
  Player player;

  // miscellaneous (used by chat commands)
  uint32_t next_exp_value; // next EXP value to give
  bool infinite_hp; // cheats enabled
  bool infinite_tp; // cheats enabled
  bool can_chat;
  std::string pending_bb_save_username;
  uint8_t pending_bb_save_player_index;

  Client(struct bufferevent* bev, GameVersion version,
      ServerBehavior server_behavior);

  // adds data to the client's output buffer, encrypting it first
  bool send(std::string&& data);
};
