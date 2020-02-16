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
  // after joining a lobby, client will no longer send D6 commands when they close message boxes
  NoMessageBoxCloseConfirmationAfterLobbyJoin = 0x0004,
  // client has the above flag and has already joined a lobby
  NoMessageBoxCloseConfirmation = 0x0008,
  // client can see Ep3 lobbies
  CanSeeExtraLobbies = 0x0010,
  // client is episode 3 and should use its game mechanic
  Episode3Games = 0x0020,
  // client is DC v1 (disables some features)
  IsDCv1 = 0x0040,
  // client is loading into a game
  Loading = 0x0080,

  // client is in the information menu (login server only)
  InInformationMenu = 0x0100,
  // client is at the welcome message (login server only)
  AtWelcomeMessage = 0x0200,

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
