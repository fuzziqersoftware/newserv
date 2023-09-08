#include "GVMEncoder.hh"

#include <phosg/Encoding.hh>
#include <phosg/Image.hh>
#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;

static uint16_t encode_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F);
}

static uint16_t encode_rgb5a3(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if ((a & 0xE0) == 0xE0) {
    return 0x8000 | ((r << 7) & 0x7C00) | ((g << 2) & 0x03E0) | ((b >> 3) & 0x001F);
  } else {
    return ((a << 7) & 0x7000) | ((r << 4) & 0x0F00) | (g & 0x00F0) | ((b >> 4) & 0x000F);
  }
}

static uint32_t encode_argb8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

struct GVMFileEntry {
  be_uint16_t file_num;
  parray<char, 28> name;
  parray<be_uint32_t, 2> unknown_a1;
} __attribute__((packed));

struct GVMFileHeader {
  be_uint32_t magic; // 'GVMH'
  le_uint32_t header_size;
  be_uint16_t flags;
  be_uint16_t num_files;
} __attribute__((packed));

struct GVRHeader {
  be_uint32_t magic; // 'GVRT'
  le_uint32_t data_size;
  be_uint16_t unknown;
  uint8_t format_flags; // High 4 bits are pixel format, low 4 are data flags
  GVRDataFormat data_format;
  be_uint16_t width;
  be_uint16_t height;
} __attribute__((packed));

string encode_gvm(const Image& img, GVRDataFormat data_format) {
  if (img.get_width() > 0xFFFF) {
    throw runtime_error("image is too wide to be encoded as a GVR texture");
  }
  if (img.get_height() > 0xFFFF) {
    throw runtime_error("image is too tall to be encoded as a GVR texture");
  }
  if (img.get_width() & 3) {
    throw runtime_error("image width is not a multiple of 4");
  }
  if (img.get_height() & 3) {
    throw runtime_error("image height is not a multiple of 4");
  }
  size_t pixel_count = img.get_width() * img.get_height();
  size_t pixel_bytes = 0;
  switch (data_format) {
    case GVRDataFormat::RGB565:
    case GVRDataFormat::RGB5A3:
      pixel_bytes = pixel_count * 2;
      break;
    case GVRDataFormat::ARGB8888:
      pixel_bytes = pixel_count * 2;
      break;
    default:
      throw invalid_argument("cannot encode pixel format");
  }

  StringWriter w;
  w.put<GVMFileHeader>({.magic = 0x47564D48, .header_size = 0x48, .flags = 0x010F, .num_files = 1});
  GVMFileEntry file_entry;
  file_entry.file_num = 0;
  file_entry.name = "img";
  file_entry.unknown_a1.clear(0);
  w.put(file_entry);
  w.extend_to(0x50, 0x00);
  w.put<GVRHeader>({.magic = 0x47565254,
      .data_size = pixel_bytes + 8,
      .unknown = 0,
      .format_flags = 0,
      .data_format = data_format,
      .width = img.get_width(),
      .height = img.get_height()});

  for (size_t y = 0; y < img.get_height(); y += 4) {
    for (size_t x = 0; x < img.get_width(); x += 4) {
      for (size_t yy = 0; yy < 4; yy++) {
        for (size_t xx = 0; xx < 4; xx++) {
          uint64_t a, r, g, b;
          img.read_pixel(x + xx, y + yy, &r, &g, &b, &a);
          switch (data_format) {
            case GVRDataFormat::RGB565:
              w.put_u16b(encode_rgb565(r, g, b));
              break;
            case GVRDataFormat::RGB5A3:
              w.put_u16b(encode_rgb5a3(r, g, b, a));
              break;
            case GVRDataFormat::ARGB8888:
              w.put_u32b(encode_argb8888(r, g, b, a));
              break;
            default:
              throw logic_error("cannot encode pixel format");
          }
        }
      }
    }
  }

  return std::move(w.str());
}
