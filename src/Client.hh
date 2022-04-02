#pragma once

#include <netinet/in.h>

#include <memory>

#include "License.hh"
#include "Player.hh"
#include "PSOEncryption.hh"

#include "Text.hh"
#include "PSOProtocol.hh"



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
} __attribute__((packed));

struct ClientConfigBB {
  ClientConfig cfg;
  parray<uint8_t, 0x08> unused;
} __attribute__((packed));

struct Client {
  enum Flag {
    // For patch server clients, client is Blue Burst rather than PC
    BB_PATCH = 0x0001,
    // After joining a lobby, client will no longer send D6 commands when they
    // close message boxes
    NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN = 0x0002,
    // Client has the above flag and has already joined a lobby, or is Blue Burst
    // (BB never sends D6 commands)
    NO_MESSAGE_BOX_CLOSE_CONFIRMATION = 0x0004,
    // Client is Episode 3, should be able to see CARD lobbies, and should only be
    // able to see/join games with the IS_EPISODE_3 flag
    EPISODE_3 = 0x0008,
    // Client is DC v1 (disables some features)
    DCV1 = 0x0010,
    // Client is loading into a game
    LOADING = 0x0020,
    // Client is loading a quest
    LOADING_QUEST = 0x0040,
    // Client is in the information menu (login server only)
    IN_INFORMATION_MENU = 0x0080,
    // Client is at the welcome message (login server only)
    AT_WELCOME_MESSAGE = 0x0100,

    // Note: There isn't a good way to detect Episode 3 until the player data is
    // sent (via a 61 command), so the IS_EPISODE_3 flag is set in that handler
    DEFAULT_V1 = DCV1,
    DEFAULT_V2_DC = 0x0000,
    DEFAULT_V2_PC = NO_MESSAGE_BOX_CLOSE_CONFIRMATION,
    DEFAULT_V3_GC = 0x0000,
    DEFAULT_V3_GC_PLUS = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN,
    DEFAULT_V3_GC_EP3 = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN | EPISODE_3,
    DEFAULT_V4_BB = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN | NO_MESSAGE_BOX_CLOSE_CONFIRMATION,
  };

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
  bool switch_assist; // cheats enabled
  PSOSubcommand last_switch_enabled_subcommand[3];
  bool can_chat;
  std::string pending_bb_save_username;
  uint8_t pending_bb_save_player_index;

  Client(struct bufferevent* bev, GameVersion version,
      ServerBehavior server_behavior);

  ClientConfig export_config() const;
  ClientConfigBB export_config_bb() const;
  void import_config(const ClientConfig& cc);
};
