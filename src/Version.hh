#pragma once

#include <inttypes.h>



enum class GameVersion {
  DC = 0,
  PC,
  PATCH,
  GC,
  XB,
  BB,
};

enum class ServerBehavior {
  SPLIT_RECONNECT = 0,
  LOGIN_SERVER,
  LOBBY_SERVER,
  DATA_SERVER_BB,
  PATCH_SERVER,
  PROXY_SERVER,
};

uint16_t flags_for_version(GameVersion version, int64_t sub_version);
const char* name_for_version(GameVersion version);
GameVersion version_for_name(const char* name);

const char* name_for_server_behavior(ServerBehavior behavior);
ServerBehavior server_behavior_for_name(const char* name);
