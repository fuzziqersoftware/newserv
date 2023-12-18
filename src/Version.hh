#pragma once

#include <inttypes.h>

#include <phosg/Types.hh>
#include <string>
#include <vector>

enum class Version {
  PC_PATCH = 0,
  BB_PATCH = 1,
  DC_NTE = 2,
  DC_V1_11_2000_PROTOTYPE = 3,
  DC_V1 = 4,
  DC_V2 = 5,
  PC_NTE = 6,
  PC_V2 = 7,
  GC_NTE = 8,
  GC_V3 = 9,
  GC_EP3_TRIAL_EDITION = 10,
  GC_EP3 = 11,
  XB_V3 = 12,
  BB_V4 = 13,
  UNKNOWN = 15,
};

template <>
const char* name_for_enum<Version>(Version v);
template <>
Version enum_for_name<Version>(const char* name);

inline bool is_patch(Version version) {
  return (version == Version::PC_PATCH) || (version == Version::BB_PATCH);
}
inline bool is_pre_v1(Version version) {
  return (version == Version::DC_NTE) || (version == Version::DC_V1_11_2000_PROTOTYPE);
}
inline bool is_v1(Version version) {
  return (version == Version::DC_NTE) || (version == Version::DC_V1_11_2000_PROTOTYPE) || (version == Version::DC_V1);
}
inline bool is_v2(Version version) {
  return (version == Version::DC_V2) || (version == Version::PC_NTE) || (version == Version::PC_V2) || (version == Version::GC_NTE);
}
inline bool is_v1_or_v2(Version version) {
  return is_v1(version) || is_v2(version);
}
inline bool is_v3(Version version) {
  return (version == Version::GC_V3) ||
      (version == Version::GC_EP3_TRIAL_EDITION) ||
      (version == Version::GC_EP3) ||
      (version == Version::XB_V3);
}
inline bool is_v4(Version version) {
  return (version == Version::BB_V4);
}

inline bool is_ep3(Version version) {
  return (version == Version::GC_EP3_TRIAL_EDITION) || (version == Version::GC_EP3);
}

inline bool is_dc(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_V1_11_2000_PROTOTYPE) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2);
}
inline bool is_gc(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_TRIAL_EDITION) ||
      (version == Version::GC_EP3);
}

inline bool is_big_endian(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_TRIAL_EDITION) ||
      (version == Version::GC_EP3);
}
inline bool uses_v2_encryption(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::DC_NTE) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::GC_NTE);
}
inline bool uses_v3_encryption(Version version) {
  return (version == Version::GC_V3) ||
      (version == Version::GC_EP3_TRIAL_EDITION) ||
      (version == Version::GC_EP3) ||
      (version == Version::XB_V3);
}
inline bool uses_v4_encryption(Version version) {
  return (version == Version::BB_V4);
}

inline bool uses_utf16(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::BB_V4);
}

uint32_t default_specific_version_for_version(Version version, int64_t sub_version);

enum class ServerBehavior {
  PC_CONSOLE_DETECT = 0,
  LOGIN_SERVER,
  LOBBY_SERVER,
  PATCH_SERVER_PC,
  PATCH_SERVER_BB,
  PROXY_SERVER,
};

const char* login_port_name_for_version(Version v);
const char* lobby_port_name_for_version(Version v);
const char* proxy_port_name_for_version(Version v);

template <>
const char* name_for_enum<ServerBehavior>(ServerBehavior behavior);
template <>
ServerBehavior enum_for_name<ServerBehavior>(const char* name);
