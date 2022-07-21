#pragma once

#include <netinet/in.h>

#include <memory>

#include "Channel.hh"
#include "CommandFormats.hh"
#include "FunctionCompiler.hh"
#include "License.hh"
#include "Player.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "Text.hh"



extern const uint64_t CLIENT_CONFIG_MAGIC;



struct Client {
  enum Flag {
    // For patch server clients, client is Blue Burst rather than PC
    BB_PATCH = 0x0001,
    // After joining a lobby, client will no longer send D6 commands when they
    // close message boxes
    NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN = 0x0002,
    // Client has the above flag and has already joined a lobby, or is not GC
    NO_MESSAGE_BOX_CLOSE_CONFIRMATION = 0x0004,
    // Client is Episode 3, should be able to see CARD lobbies, and should only
    // be able to see/join games with the IS_EPISODE_3 flag
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
    // Client disconnects if it receives B2 (send_function_call)
    DOES_NOT_SUPPORT_SEND_FUNCTION_CALL = 0x0200,
    // Client has already received a 97 (enable saves) command, so don't show
    // the programs menu anymore
    SAVE_ENABLED = 0x0400,
    // Client requires doubly-encrypted code section in send_function_call
    ENCRYPTED_SEND_FUNCTION_CALL = 0x0800,
    // Client supports send_function_call but does not actually run the code
    SEND_FUNCTION_CALL_CHECKSUM_ONLY = 0x1000,

    DEFAULT_V1_DC =
        DCV1 |
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION |
        DOES_NOT_SUPPORT_SEND_FUNCTION_CALL,

    DEFAULT_V2_DC =
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION,

    DEFAULT_V2_PC =
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION |
        SEND_FUNCTION_CALL_CHECKSUM_ONLY,

    DEFAULT_V3_GC = 0,

    DEFAULT_V3_GC_PLUS =
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN |
        ENCRYPTED_SEND_FUNCTION_CALL,

    DEFAULT_V3_GC_PLUS_NO_SFC =
        DEFAULT_V3_GC_PLUS |
        DOES_NOT_SUPPORT_SEND_FUNCTION_CALL,

    DEFAULT_V3_GC_EP3 =
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN |
        EPISODE_3 |
        ENCRYPTED_SEND_FUNCTION_CALL,

    DEFAULT_V3_GC_EP3_NO_SFC =
        DEFAULT_V3_GC_EP3 |
        DOES_NOT_SUPPORT_SEND_FUNCTION_CALL,

    DEFAULT_V4_BB =
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN |
        NO_MESSAGE_BOX_CLOSE_CONFIRMATION |
        SAVE_ENABLED,
  };

  uint64_t id;
  PrefixedLogger log;

  // License & account
  std::shared_ptr<const License> license;
  GameVersion version;

  // Note: these fields are included in the client config. On GC, the client
  // config can be up to 0x20 bytes; on BB it can be 0x28 bytes. We don't use
  // all of that space.
  uint8_t bb_game_state;
  uint16_t flags;

  // Network
  Channel channel;
  struct sockaddr_storage next_connection_addr;
  ServerBehavior server_behavior;
  bool should_disconnect;
  bool should_send_to_lobby_server;
  uint32_t proxy_destination_address;
  uint16_t proxy_destination_port;

  // Lobby/positioning
  float x;
  float z;
  uint32_t area; // which area is the client in?
  uint32_t lobby_id; // which lobby is this person in?
  uint8_t lobby_client_id; // which client number is this person?
  uint8_t lobby_arrow_color; // lobby arrow color ID
  bool prefer_high_lobby_client_id;
  ClientGameData game_data;

  // Miscellaneous (used by chat commands)
  uint32_t next_exp_value; // next EXP value to give
  int16_t override_section_id; // valid if >= 0
  int64_t override_random_seed; // valid if >= 0
  bool infinite_hp; // cheats enabled
  bool infinite_tp; // cheats enabled
  bool switch_assist; // cheats enabled
  G_SwitchStateChanged_6x05 last_switch_enabled_command;
  bool can_chat;
  std::string pending_bb_save_username;
  uint8_t pending_bb_save_player_index;

  // DOL file loading state
  uint32_t dol_base_addr;
  std::shared_ptr<DOLFileIndex::DOLFile> loading_dol_file;

  Client(struct bufferevent* bev, GameVersion version, ServerBehavior server_behavior);

  void set_license(std::shared_ptr<const License> l);

  ClientConfig export_config() const;
  ClientConfigBB export_config_bb() const;
  void import_config(const ClientConfig& cc);
  void import_config(const ClientConfigBB& cc);
};
