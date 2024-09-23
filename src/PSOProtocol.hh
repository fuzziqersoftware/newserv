#pragma once

#include <event2/bufferevent.h>
#include <inttypes.h>

#include <functional>
#include <phosg/Strings.hh>

#include "PSOEncryption.hh"
#include "Version.hh"

struct PSOCommandHeaderPC {
  le_uint16_t size;
  uint8_t command;
  uint8_t flag;
} __packed_ws__(PSOCommandHeaderPC, 4);

struct PSOCommandHeaderDCV3 {
  uint8_t command;
  uint8_t flag;
  le_uint16_t size;
} __packed_ws__(PSOCommandHeaderDCV3, 4);

struct PSOCommandHeaderBB {
  le_uint16_t size;
  le_uint16_t command;
  le_uint32_t flag;
} __packed_ws__(PSOCommandHeaderBB, 8);

union PSOCommandHeader {
  PSOCommandHeaderDCV3 dc;
  PSOCommandHeaderPC pc;
  PSOCommandHeaderDCV3 gc;
  PSOCommandHeaderDCV3 xb;
  PSOCommandHeaderBB bb;

  uint16_t command(Version version) const;
  void set_command(Version version, uint16_t command);
  uint16_t size(Version version) const;
  void set_size(Version version, uint32_t size);
  uint32_t flag(Version version) const;
  void set_flag(Version version, uint32_t flag);
  static inline size_t header_size(Version version) {
    return (version == Version::BB_V4) ? 8 : 4;
  }

  PSOCommandHeader();
} __packed_ws__(PSOCommandHeader, 8);

// This function is used in a lot of places to check received command sizes and
// cast them to the appropriate type
template <typename RetT, typename PtrT>
RetT& check_size_generic(
    PtrT data,
    size_t size,
    size_t min_size,
    size_t max_size) {
  if (size < min_size) {
    throw std::runtime_error(phosg::string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, size));
  }
  if (size > max_size) {
    throw std::runtime_error(phosg::string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, size));
  }
  return *reinterpret_cast<RetT*>(data);
}

template <typename T>
const T& check_size_t(const std::string& data, size_t min_size, size_t max_size) {
  return check_size_generic<const T, const void*>(data.data(), data.size(), min_size, max_size);
}
template <typename T>
const T& check_size_t(const std::string& data, size_t max_size) {
  return check_size_generic<const T, const void*>(data.data(), data.size(), sizeof(T), max_size);
}
template <typename T>
const T& check_size_t(const std::string& data) {
  return check_size_generic<const T, const void*>(data.data(), data.size(), sizeof(T), sizeof(T));
}

template <typename T>
T& check_size_t(std::string& data, size_t min_size, size_t max_size) {
  return check_size_generic<T, void*>(data.data(), data.size(), min_size, max_size);
}
template <typename T>
T& check_size_t(std::string& data, size_t max_size) {
  return check_size_generic<T, void*>(data.data(), data.size(), sizeof(T), max_size);
}
template <typename T>
T& check_size_t(std::string& data) {
  return check_size_generic<T, void*>(data.data(), data.size(), sizeof(T), sizeof(T));
}

template <typename T>
const T& check_size_t(const void* data, size_t size, size_t min_size, size_t max_size) {
  return check_size_generic<const T, const void*>(data, size, min_size, max_size);
}
template <typename T>
const T& check_size_t(const void* data, size_t size, size_t max_size) {
  return check_size_generic<const T, const void*>(data, size, sizeof(T), max_size);
}
template <typename T>
const T& check_size_t(const void* data, size_t size) {
  return check_size_generic<const T, const void*>(data, size, sizeof(T), sizeof(T));
}

template <typename T>
T& check_size_t(void* data, size_t size, size_t min_size, size_t max_size) {
  return check_size_generic<T, void*>(data, size, min_size, max_size);
}
template <typename T>
T& check_size_t(void* data, size_t size, size_t max_size) {
  return check_size_generic<T, void*>(data, size, sizeof(T), max_size);
}
template <typename T>
T& check_size_t(void* data, size_t size) {
  return check_size_generic<T, void*>(data, size, sizeof(T), sizeof(T));
}

template <typename T>
T* check_size_vec_t(std::string& data, size_t count, bool allow_extra = false) {
  size_t expected_size = count * sizeof(T);
  return &check_size_generic<T, void*>(data.data(), data.size(), expected_size, allow_extra ? 0xFFFF : expected_size);
}

void check_size_v(size_t size, size_t min_size, size_t max_size = 0);

std::string prepend_command_header(
    Version version,
    bool encryption_enabled,
    uint16_t cmd,
    uint32_t flag,
    const std::string& data);
