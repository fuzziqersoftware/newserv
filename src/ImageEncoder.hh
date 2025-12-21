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

std::string encode_gvm(
    const phosg::ImageRGBA8888N& img,
    GVRDataFormat data_format,
    const std::string& internal_name,
    uint32_t global_index);
phosg::ImageRGB888 decode_fon(const std::string& data, size_t width);
std::string encode_fon(const phosg::ImageRGB888& img);

constexpr uint16_t encode_rgb5a3(uint32_t c) {
  if ((phosg::get_a(c) & 0xE0) == 0xE0) {
    return 0x8000 | ((phosg::get_r(c) << 7) & 0x7C00) | ((phosg::get_g(c) << 2) & 0x03E0) | ((phosg::get_b(c) >> 3) & 0x001F);
  } else {
    return ((phosg::get_a(c) << 7) & 0x7000) | ((phosg::get_r(c) << 4) & 0x0F00) | (phosg::get_g(c) & 0x00F0) | ((phosg::get_b(c) >> 4) & 0x000F);
  }
}

template <phosg::PixelFormat Format>
bool has_any_transparent_pixels(const phosg::Image<Format>& img) {
  if constexpr (phosg::Image<Format>::HAS_ALPHA) {
    for (size_t y = 0; y < img.get_height(); y++) {
      for (size_t x = 0; x < img.get_height(); x++) {
        if (phosg::get_a(img.read(x, y)) != 0xFF) {
          return true;
        }
      }
    }
  }
  return false;
}
