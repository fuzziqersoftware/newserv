#pragma once

#include <inttypes.h>

#include <array>
#include <phosg/Types.hh>
#include <string>
#include <vector>

enum class Version {
  PC_PATCH = 0,
  BB_PATCH = 1,
  DC_NTE = 2,
  DC_11_2000 = 3,
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

constexpr std::array<Version, 14> ALL_VERSIONS = {
    Version::PC_PATCH,
    Version::BB_PATCH,
    Version::DC_NTE,
    Version::DC_11_2000,
    Version::DC_V1,
    Version::DC_V2,
    Version::PC_NTE,
    Version::PC_V2,
    Version::GC_NTE,
    Version::GC_V3,
    Version::GC_EP3_NTE,
    Version::GC_EP3,
    Version::XB_V3,
    Version::BB_V4,
};
constexpr std::array<Version, 12> ALL_NON_PATCH_VERSIONS = {
    Version::DC_NTE,
    Version::DC_11_2000,
    Version::DC_V1,
    Version::DC_V2,
    Version::PC_NTE,
    Version::PC_V2,
    Version::GC_NTE,
    Version::GC_V3,
    Version::GC_EP3_NTE,
    Version::GC_EP3,
    Version::XB_V3,
    Version::BB_V4,
};
constexpr std::array<Version, 10> ALL_ARPG_SEMANTIC_VERSIONS = {
    Version::DC_NTE,
    Version::DC_11_2000,
    Version::DC_V1,
    Version::DC_V2,
    Version::PC_NTE,
    Version::PC_V2,
    Version::GC_NTE,
    Version::GC_V3,
    Version::XB_V3,
    Version::BB_V4,
};

constexpr size_t NUM_VERSIONS = ALL_VERSIONS.size();
constexpr size_t NUM_PATCH_VERSIONS = static_cast<size_t>(Version::BB_PATCH) + 1;
constexpr size_t NUM_NON_PATCH_VERSIONS = NUM_VERSIONS - NUM_PATCH_VERSIONS;

static_assert(NUM_NON_PATCH_VERSIONS == 12, "Don't forget to update VersionNameColors in config.json");

template <>
const char* phosg::name_for_enum<Version>(Version v);
template <>
Version phosg::enum_for_name<Version>(const char* name);

constexpr bool is_any_nte(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_11_2000) ||
      (version == Version::PC_NTE) ||
      (version == Version::GC_NTE) ||
      (version == Version::GC_EP3_NTE);
}

constexpr bool is_patch(Version version) {
  return (version == Version::PC_PATCH) || (version == Version::BB_PATCH);
}
constexpr bool is_pre_v1(Version version) {
  return (version == Version::DC_NTE) || (version == Version::DC_11_2000);
}
constexpr bool is_v1(Version version) {
  return (version == Version::DC_NTE) || (version == Version::DC_11_2000) || (version == Version::DC_V1);
}
constexpr bool is_v2(Version version) {
  return (version == Version::DC_V2) || (version == Version::PC_NTE) || (version == Version::PC_V2) || (version == Version::GC_NTE);
}
constexpr bool is_v1_or_v2(Version version) {
  return is_v1(version) || is_v2(version);
}
constexpr bool is_v3(Version version) {
  return (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3) ||
      (version == Version::XB_V3);
}
constexpr bool is_v4(Version version) {
  return (version == Version::BB_V4);
}

constexpr bool is_ep3(Version version) {
  return (version == Version::GC_EP3_NTE) || (version == Version::GC_EP3);
}

constexpr bool is_dc(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_11_2000) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2);
}
constexpr bool is_gc(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3);
}

constexpr bool is_sh4(Version version) {
  return (version == Version::DC_NTE) ||
      (version == Version::DC_11_2000) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2);
}
constexpr bool is_x86(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::XB_V3) ||
      (version == Version::BB_V4);
}
constexpr bool is_ppc(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3);
}

constexpr bool is_big_endian(Version version) {
  return (version == Version::GC_NTE) ||
      (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3);
}
constexpr bool uses_v2_encryption(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::DC_NTE) ||
      (version == Version::DC_V1) ||
      (version == Version::DC_V2) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::GC_NTE);
}
constexpr bool uses_v3_encryption(Version version) {
  return (version == Version::GC_V3) ||
      (version == Version::GC_EP3_NTE) ||
      (version == Version::GC_EP3) ||
      (version == Version::XB_V3);
}
constexpr bool uses_v4_encryption(Version version) {
  return (version == Version::BB_V4);
}

constexpr bool uses_utf16(Version version) {
  return (version == Version::PC_PATCH) ||
      (version == Version::BB_PATCH) ||
      (version == Version::PC_NTE) ||
      (version == Version::PC_V2) ||
      (version == Version::BB_V4);
}

constexpr uint32_t SPECIFIC_VERSION_DC_NTE = 0x314F4A31; // 1OJ1
constexpr uint32_t SPECIFIC_VERSION_DC_11_2000_PROTOTYPE = 0x314F4A32; // 1OJ2
constexpr uint32_t SPECIFIC_VERSION_DC_V1_JP = 0x314F4A46; // 1OJF
constexpr uint32_t SPECIFIC_VERSION_DC_V1_US = 0x314F4546; // 1OEF
constexpr uint32_t SPECIFIC_VERSION_DC_V1_EU_INDETERMINATE = 0x314F5000; // 1OP_
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

uint32_t default_sub_version_for_version(Version version);
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
const char* phosg::name_for_enum<ServerBehavior>(ServerBehavior behavior);
template <>
ServerBehavior phosg::enum_for_name<ServerBehavior>(const char* name);

const char* file_path_token_for_version(Version version);

uint64_t generate_random_hardware_id(Version version);
