#pragma once

#include <inttypes.h>



enum class GameVersion {
  DC = 0,
  PC,
  Patch,
  GC,
  BB,
};

enum ClientFlag {
  // After joining a lobby, client will no longer send D6 commands when they close message boxes
  NoMessageBoxCloseConfirmationAfterLobbyJoin = 0x0004,
  // Client has the above flag and has already joined a lobby
  NoMessageBoxCloseConfirmation = 0x0008,
  // Client can see Ep3 lobbies
  CanSeeExtraLobbies = 0x0010,
  // Client is episode 3 and should use its game mechanic
  Episode3Games = 0x0020,
  // Client is DC v1 (disables some features)
  IsDCv1 = 0x0040,
  // Client is loading into a game
  Loading = 0x0080,

  // Client is in the information menu (login server only)
  InInformationMenu = 0x0100,
  // Client is at the welcome message (login server only)
  AtWelcomeMessage = 0x0200,

  // Note: There isn't a good way to detect Episode 3 until the player data is
  // sent (via a 61 command), so the Episode3Games flag is set in that handler
  DefaultV1 = IsDCv1,
  DefaultV2DC = 0x0000,
  DefaultV2PC = 0x0000,
  DefaultV3GC = 0x0000,
  DefaultV3GCPlus = NoMessageBoxCloseConfirmationAfterLobbyJoin,
  DefaultV3BB = NoMessageBoxCloseConfirmationAfterLobbyJoin | NoMessageBoxCloseConfirmation,
  DefaultV4 = NoMessageBoxCloseConfirmationAfterLobbyJoin | CanSeeExtraLobbies | Episode3Games,
};

uint16_t flags_for_version(GameVersion version, uint8_t sub_version);
const char* name_for_version(GameVersion version);
GameVersion version_for_name(const char* name);
