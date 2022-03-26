#pragma once

#include <inttypes.h>



enum class GameVersion {
  DC = 0,
  PC,
  PATCH,
  GC,
  BB,
};

enum ClientFlag {
  // After joining a lobby, client will no longer send D6 commands when they close message boxes
  NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN = 0x0004,
  // Client has the above flag and has already joined a lobby
  NO_MESSAGE_BOX_CLOSE_CONFIRMATION = 0x0008,
  // Client can see Ep3 lobbies
  CAN_SEE_EPISODE_3_LOBBIES = 0x0010,
  // Client is episode 3 and should use its game mechanic
  EPISODE_3_GAMES = 0x0020,
  // Client is DC v1 (disables some features)
  IS_DCV1 = 0x0040,
  // Client is loading into a game
  LOADING = 0x0080,

  // Client is in the information menu (login server only)
  IN_INFORMATION_MENU = 0x0100,
  // Client is at the welcome message (login server only)
  AT_WELCOME_MESSAGE = 0x0200,

  // Note: There isn't a good way to detect Episode 3 until the player data is
  // sent (via a 61 command), so the Episode3Games flag is set in that handler
  DEFAULT_V1 = IS_DCV1,
  DEFAULT_V2_DC = 0x0000,
  DEFAULT_V2_PC = 0x0000,
  DEFAULT_V3_GC = 0x0000,
  DEFAULT_V3_GC_PLUS = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN,
  DEFAULT_V3_BB = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN | NO_MESSAGE_BOX_CLOSE_CONFIRMATION,
  DEFAULT_V4 = NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN | EPISODE_3_GAMES | CAN_SEE_EPISODE_3_LOBBIES,
};

uint16_t flags_for_version(GameVersion version, uint8_t sub_version);
const char* name_for_version(GameVersion version);
GameVersion version_for_name(const char* name);
