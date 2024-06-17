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
  GC_EP3_NTE = 10,
  GC_EP3 = 11,
  XB_V3 = 12,
  BB_V4 = 13,
  UNKNOWN = 15,
};

constexpr size_t NUM_VERSIONS = static_cast<size_t>(Version::BB_V4) + 1;
constexpr size_t NUM_PATCH_VERSIONS = static_cast<size_t>(Version::BB_PATCH) + 1;
constexpr size_t NUM_NON_PATCH_VERSIONS = NUM_VERSIONS - NUM_PATCH_VERSIONS;

static_assert(NUM_NON_PATCH_VERSIONS == 12, "Don't forget to update VersionNameColors in config.json");

template <>
const char* name_for_enum<Version>(Version v);
template <>
Version enum_for_name<Version>(const char* name);

inline bool is_any_nte(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_V1_11_2000_PROTOTYPE) ||
      (version == Version::PC_NTE) ||
      (version == Version::GC_NTE) ||
      (version == Version::GC_EP3_NTE);
}

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
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3) ||
      (version == Version::XB_V3);
}
inline bool is_v4(Version version) {
  return (version == Version::BB_V4);
}

inline bool is_ep3(Version version) {
  return (version == Version::GC_EP3_NTE) || (version == Version::GC_EP3);
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
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3);
}

inline bool is_sh4(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_V1_11_2000_PROTOTYPE) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2);
}
inline bool is_x86(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::XB_V3) ||
      (version == Version::BB_V4);
}
inline bool is_ppc(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3);
}

inline bool is_big_endian(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
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
      (version == Version::GC_EP3_NTE) ||
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

constexpr uint32_t SPECIFIC_VERSION_DC_NTE = 0x314F4A31; // 1OJ1
constexpr uint32_t SPECIFIC_VERSION_DC_11_2000_PROTOTYPE = 0x314F4A32; // 1OJ2
constexpr uint32_t SPECIFIC_VERSION_DC_V1_INDETERMINATE = 0x31000000; // 1___
constexpr uint32_t SPECIFIC_VERSION_DC_V2_INDETERMINATE = 0x32000000; // 2___
constexpr uint32_t SPECIFIC_VERSION_PC_V2 = 0x324F4A57; // 2OJW
constexpr uint32_t SPECIFIC_VERSION_GC_NTE = 0x334F4A54; // 3OJT
constexpr uint32_t SPECIFIC_VERSION_GC_V3_EU = 0x334F5030; // 3OP0
constexpr uint32_t SPECIFIC_VERSION_GC_V3_US_12 = 0x334F4532; // 3OE2
constexpr uint32_t SPECIFIC_VERSION_GC_V3_JP_13 = 0x334F4A33; // 3OJ3
constexpr uint32_t SPECIFIC_VERSION_GC_V3_JP_14 = 0x334F4A34; // 3OJ4
constexpr uint32_t SPECIFIC_VERSION_GC_V3_JP_15 = 0x334F4A35; // 3OJ5
constexpr uint32_t SPECIFIC_VERSION_GC_V3_INDETERMINATE = 0x334F0000; // 3O__
constexpr uint32_t SPECIFIC_VERSION_GC_EP3_INDETERMINATE = 0x33534A00; // 3SJ_
constexpr uint32_t SPECIFIC_VERSION_GC_EP3_NTE = 0x33534A54; // 3SJT
constexpr uint32_t SPECIFIC_VERSION_GC_EP3_JP = 0x33534A30; // 3SJ0
constexpr uint32_t SPECIFIC_VERSION_GC_EP3_US = 0x33534530; // 3SE0
constexpr uint32_t SPECIFIC_VERSION_GC_EP3_EU = 0x33535030; // 3SP0
constexpr uint32_t SPECIFIC_VERSION_XB_V3_INDETERMINATE = 0x344F0000; // 4O__
constexpr uint32_t SPECIFIC_VERSION_BB_V4_INDETERMINATE = 0x35000000; // 5___
constexpr uint32_t SPECIFIC_VERSION_INDETERMINATE = 0x00000000; // ____

uint32_t default_specific_version_for_version(Version version, int64_t sub_version);
bool specific_version_is_indeterminate(uint32_t specific_version);
bool specific_version_is_dc(uint32_t specific_version);
bool specific_version_is_gc(uint32_t specific_version);
bool specific_version_is_xb(uint32_t specific_version);
bool specific_version_is_bb(uint32_t specific_version);

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

const char* file_path_token_for_version(Version version);
