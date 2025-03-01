#include "AFSArchive.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Compression.hh"
#include "Text.hh"

using namespace std;

struct Entry {
  pstring<TextEncoding::ASCII, 0x80> filename;
  le_uint32_t unknown_a1;
  le_uint32_t decompressed_size;
  le_uint32_t compressed_size;
  le_uint32_t checksum;
  // Data follows immediately here
  // Trailer: le_uint32_t entry_size; //
};

static void decrypt_ppk_data(std::string& data, const std::string& filename, const std::string& password) {
  if (password.size() > 0xFF) {
    throw runtime_error("password is too long");
  }

  uint8_t key[0x100];
  for (size_t z = 0; z < 0x100; z++) {
    key[z] = z ^ filename[z % filename.size()];
  }
  for (size_t z = 0; z < password.size(); z++) {
    key[z + 1] ^= password[z];
  }
  for (size_t z = 0; z < 0xFC; z++) {
    key[z + 4] ^= key[z];
  }
  for (size_t z = 0; z < data.size(); z++) {
    data[z] ^= key[z & 0xFF];
  }
}

std::unordered_map<std::string, std::string> decode_ppk_file(const std::string& data, const std::string& password) {
  phosg::StringReader r(data);

  uint32_t signature = r.get_u32b();
  if (signature != 0x50503130 && signature != 0x4D5A5000) { // 'PP10' or 'MZP\0'
    throw runtime_error("file is not a ppk archive");
  }

  unordered_map<string, string> ret;
  for (size_t offset = r.size() - 4; offset >= 4;) {
    uint32_t size = r.pget_u32l(offset) ^ 0x12345678;
    uint32_t entry_offset = offset - size;
    const auto& entry = r.pget<Entry>(entry_offset);
    string data = r.pread(entry_offset + sizeof(Entry), entry.compressed_size);
    string filename = entry.filename.decode();
    decrypt_ppk_data(data, phosg::tolower(filename), password);
    uint32_t checksum = phosg::crc32(data.data(), data.size());
    if (checksum != entry.checksum) {
      throw runtime_error(phosg::string_printf(
          "incorrect checksum for file %s (expected %08" PRIX32 "; received %08" PRIX32 ")",
          filename.c_str(), entry.checksum.load(), checksum));
    }
    if (entry.compressed_size < entry.decompressed_size) {
      data = prs_decompress(data);
    }
    if (!ret.emplace(filename, data).second) {
      throw runtime_error(phosg::string_printf("archive contains multiple files with the same name (%s)", filename.c_str()));
    }
    offset = entry_offset - 4;
  }
  return ret;
}
