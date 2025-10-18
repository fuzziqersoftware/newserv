#include "ImageEncoder.hh"

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/Image.hh>
#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;

struct GVMFileEntry {
  be_uint16_t file_num;
  pstring<TextEncoding::ASCII, 0x1C> name;
  uint8_t format_flags; // Same as in GVRHeader
  GVRDataFormat data_format; // Same as in GVRHeader
  be_uint16_t dimensions; // As powers of two in low nybbles (so e.g. 128x128 = 0x0055)
  be_uint32_t global_index;
} __packed_ws__(GVMFileEntry, 0x26);

struct GVMFileHeader {
  be_uint32_t signature; // 'GVMH'
  le_uint32_t header_size;
  be_uint16_t flags; // Specifies which fields are present in GVMFileEntries; we always use 0xF (all fields present)
  be_uint16_t num_files;
} __packed_ws__(GVMFileHeader, 0x0C);

struct GVRHeader {
  be_uint32_t magic; // 'GVRT'
  le_uint32_t data_size;
  be_uint16_t unknown;
  uint8_t format_flags; // High 4 bits are pixel format, low 4 are data flags
  GVRDataFormat data_format;
  be_uint16_t width;
  be_uint16_t height;
} __packed_ws__(GVRHeader, 0x10);

string encode_gvm(const phosg::ImageRGBA8888N& img, GVRDataFormat data_format, const string& internal_name, uint32_t global_index) {
  int8_t dimensions_field = -2;
  {
    size_t h = img.get_height();
    size_t w = img.get_width();
    if ((h != w) || (w & (w - 1)) || (h & (h - 1))) {
      throw runtime_error("image must be square and dimensions must be powers of 2");
    }
    for (w >>= 1; w; w >>= 1, dimensions_field++) {
    }
    if (dimensions_field < 1) {
      throw runtime_error("image is too small");
    }
    if (dimensions_field > 0xF) {
      throw runtime_error("image is too large");
    }
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

  phosg::StringWriter w;
  w.put<GVMFileHeader>({.signature = 0x47564D48, .header_size = 0x48, .flags = 0x000F, .num_files = 1});
  GVMFileEntry file_entry;
  file_entry.file_num = 0;
  file_entry.name.encode(internal_name, Language::ENGLISH);
  file_entry.data_format = data_format;
  file_entry.format_flags = 0;
  file_entry.dimensions = (dimensions_field << 4) | dimensions_field;
  file_entry.global_index = global_index;
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
          uint32_t c = img.read(x + xx, y + yy);
          switch (data_format) {
            case GVRDataFormat::RGB565:
              w.put_u16b(phosg::rgb565_for_rgba8888(c));
              break;
            case GVRDataFormat::RGB5A3:
              w.put_u16b(encode_rgb5a3(c));
              break;
            case GVRDataFormat::ARGB8888:
              w.put_u32b(phosg::argb8888_for_rgba8888(c));
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

static const array<uint32_t, 4> fon_colors = {0x000000FF, 0x555555FF, 0xAAAAAAFF, 0xFFFFFFFF};

phosg::ImageRGB888 decode_fon(const string& data, size_t width) {
  size_t num_pixels = data.size() * 4;
  size_t height = num_pixels / width;
  phosg::ImageRGB888 ret(width, height);

  phosg::BitReader r(data);
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      ret.write(x, y, fon_colors[r.read(2)]);
    }
  }
  return ret;
}

constexpr size_t uabs(size_t a, size_t b) {
  return (a > b) ? (a - b) : (b - a);
}

string encode_fon(const phosg::ImageRGB888& img) {
  phosg::BitWriter w;
  for (size_t y = 0; y < img.get_height(); y++) {
    for (size_t x = 0; x < img.get_width(); x++) {
      uint32_t color = img.read(x, y);

      size_t result_delta = 0x400;
      size_t result_index = 0;
      for (size_t z = 0; z < 4; z++) {
        size_t delta = uabs((fon_colors[z] >> 24) & 0xFF, (color >> 24) & 0xFF) +
            uabs((fon_colors[z] >> 16) & 0xFF, (color >> 16) & 0xFF) +
            uabs((fon_colors[z] >> 8) & 0xFF, (color >> 8) & 0xFF) +
            uabs(fon_colors[z] & 0xFF, color & 0xFF);
        if (delta < result_delta) {
          result_delta = delta;
          result_index = z;
        }
      }
      w.write(result_index & 2);
      w.write(result_index & 1);
    }
  }
  return w.str();
}
