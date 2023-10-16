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
