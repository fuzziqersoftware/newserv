#pragma once

#include <inttypes.h>
#include <event2/bufferevent.h>

#include <functional>
#include <phosg/Strings.hh>

#include "Version.hh"
#include "PSOEncryption.hh"

struct PSOCommandHeaderPC {
  uint16_t size;
  uint8_t command;
  uint8_t flag;
} __attribute__((packed));

struct PSOCommandHeaderDCGC {
  uint8_t command;
  uint8_t flag;
  uint16_t size;
} __attribute__((packed));

struct PSOCommandHeaderBB {
  uint16_t size;
  uint16_t command;
  uint32_t flag;
} __attribute__((packed));

union PSOCommandHeader {
  PSOCommandHeaderDCGC dc;
  PSOCommandHeaderPC pc;
  PSOCommandHeaderDCGC gc;
  PSOCommandHeaderBB bb;

  uint16_t command(GameVersion version) const;
  void set_command(GameVersion version, uint16_t command);
  uint16_t size(GameVersion version) const;
  void set_size(GameVersion version, uint32_t size);
  uint32_t flag(GameVersion version) const;
  void set_flag(GameVersion version, uint32_t flag);
  static inline size_t header_size(GameVersion version) {
    return (version == GameVersion::BB) ? 8 : 4;
  }

  PSOCommandHeader();
} __attribute__((packed));

union PSOSubcommand {
  uint8_t byte[4];
  uint16_t word[2];
  uint32_t dword;
} __attribute__((packed));

void for_each_received_command(
    struct bufferevent* bev,
    GameVersion version,
    PSOEncryption* crypt,
    std::function<void(uint16_t, uint16_t, std::string&)> fn);

void print_received_command(
    uint16_t command,
    uint32_t flag,
    const void* data,
    size_t size,
    GameVersion version,
    const char* name = nullptr);

// This function is used in a lot of places to check received command sizes and
// cast them to the appropriate type
template <typename T>
const T& check_size_t(
    const std::string& data,
    size_t min_size = sizeof(T),
    size_t max_size = sizeof(T)) {
  if (data.size() < min_size) {
    throw std::runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, data.size()));
  }
  if (data.size() > max_size) {
    throw std::runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, data.size()));
  }
  return *reinterpret_cast<const T*>(data.data());
}
template <typename T>
T& check_size_t(
    std::string& data,
    size_t min_size = sizeof(T),
    size_t max_size = sizeof(T)) {
  if (data.size() < min_size) {
    throw std::runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, data.size()));
  }
  if (data.size() > max_size) {
    throw std::runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, data.size()));
  }
  return *reinterpret_cast<T*>(data.data());
}

void check_size_v(size_t size, size_t min_size, size_t max_size = 0);
