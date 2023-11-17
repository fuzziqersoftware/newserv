#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Image.hh>
#include <phosg/Strings.hh>

#include "Text.hh"

enum class GVRDataFormat : uint8_t {
  INTENSITY_4 = 0x00,
  INTENSITY_8 = 0x01,
  INTENSITY_A4 = 0x02,
  INTENSITY_A8 = 0x03,
  RGB565 = 0x04,
  RGB5A3 = 0x05,
  ARGB8888 = 0x06,
  INDEXED_4 = 0x08,
  INDEXED_8 = 0x09,
  DXT1 = 0x0E,
};

std::string encode_gvm(const Image& img, GVRDataFormat data_format);

constexpr uint16_t encode_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F);
}

constexpr uint16_t encode_rgb5a3(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if ((a & 0xE0) == 0xE0) {
    return 0x8000 | ((r << 7) & 0x7C00) | ((g << 2) & 0x03E0) | ((b >> 3) & 0x001F);
  } else {
    return ((a << 7) & 0x7000) | ((r << 4) & 0x0F00) | (g & 0x00F0) | ((b >> 4) & 0x000F);
  }
}

constexpr uint32_t encode_argb8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

constexpr uint16_t encode_xrgb8888_to_xrgb1555(uint32_t xrgb8888) {
  // In:  rrrrrrrrggggggggbbbbbbbbaaaaaaaa
  // Out:                 -rrrrrgggggbbbbb
  return ((xrgb8888 >> 9) & 0x7C00) | ((xrgb8888 >> 6) & 0x03E0) | ((xrgb8888 >> 3) & 0x001F);
}

constexpr uint32_t encode_rgbx8888_to_xrgb1555(uint16_t rgbx8888) {
  // In:  rrrrrrrrggggggggbbbbbbbbxxxxxxxx
  // Out:                 -rrrrrgggggbbbbb
  return ((rgbx8888 >> 17) & 0x7C00) | ((rgbx8888 >> 14) & 0x03E0) | ((rgbx8888 >> 11) & 0x001F);
}

constexpr uint32_t decode_xrgb1555_to_rgba8888(uint16_t xrgb1555) {
  // In:                  -rrrrrgggggbbbbb
  // Out: rrrrrrrrggggggggbbbbbbbbaaaaaaaa (a is always FF)
  return ((xrgb1555 << 17) & 0xF8000000) | ((xrgb1555 << 12) & 0x07000000) |
      ((xrgb1555 << 14) & 0x00F80000) | ((xrgb1555 << 9) & 0x00070000) |
      ((xrgb1555 << 11) & 0x0000F800) | ((xrgb1555 << 6) & 0x00000700) |
      0x000000FF;
}
