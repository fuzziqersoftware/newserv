#include "SaveFileFormats.hh"

#include <phosg/Hash.hh>
#include <stdexcept>
#include <string>

#include "LevelTable.hh"
#include "PSOProtocol.hh"

using namespace std;

struct DefaultSymbolChatEntry {
  array<const char*, 8> language_to_name;
  uint32_t spec;
  array<uint16_t, 4> corner_objects;
  array<SymbolChatFacePart, 12> face_parts;

  SaveFileSymbolChatEntryBB to_entry(uint8_t language) const {
    SaveFileSymbolChatEntryBB ret;
    ret.present = 1;
    ret.name.encode(this->language_to_name.at(language), language);
    ret.spec.spec = this->spec;
    for (size_t z = 0; z < 4; z++) {
      ret.spec.corner_objects[z] = this->corner_objects[z];
    }
    for (size_t z = 0; z < 12; z++) {
      ret.spec.face_parts[z] = this->face_parts[z];
    }
    return ret;
  }
};

static const array<DefaultSymbolChatEntry, 6> DEFAULT_SYMBOL_CHATS = {
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF", "\tEHello", "\tEHallo", "\tESalut", "\tEHola", "\tB\xE4\xBD\xA0\xE5\xA5\xBD", "\tT\xE4\xBD\xA0\xE5\xA5\xBD", "\tK\xEC\x95\x88\xEB\x85\x95"}, 0x28, {0xFFFF, 0x000D, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x05, 0x18, 0x1D, 0x00}, {0x05, 0x28, 0x1D, 0x01}, {0x36, 0x20, 0x2A, 0x00}, {0x3C, 0x00, 0x32, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\x95\xE3\x82\x88\xE3\x81\x86\xE3\x81\xAA\xE3\x82\x89", "\tEGood-bye", "\tETschus", "\tEAu revoir", "\tEAdios", "\tB\xE5\x86\x8D\xE8\xA7\x81", "\tT\xE5\x86\x8D\xE8\xA6\x8B", "\tK\xEC\x9E\x98\xEA\xB0\x80"}, 0x74, {0x0476, 0x000C, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x06, 0x15, 0x14, 0x00}, {0x06, 0x2B, 0x14, 0x01}, {0x05, 0x18, 0x1F, 0x00}, {0x05, 0x28, 0x1F, 0x01}, {0x36, 0x20, 0x2A, 0x00}, {0x3C, 0x00, 0x32, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\xB0\xE3\x82\x93\xE3\x81\x96\xE3\x83\xBC\xE3\x81\x84", "\tEHurrah!", "\tEHurra!", "\tEHourra !", "\tEHurra", "\tB\xE4\xB8\x87\xE5\xB2\x81", "\tT\xE8\x90\xAC\xE6\xAD\xB2", "\tK\xEB\xA7\x8C\xEC\x84\xB8"}, 0x28, {0x0362, 0x0362, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x09, 0x16, 0x1B, 0x00}, {0x09, 0x2B, 0x1B, 0x01}, {0x37, 0x20, 0x2C, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\x86\xE3\x81\x87\xEF\xBD\x9E\xE3\x82\x93", "\tECrying", "\tEIch bin sauer!", "\tEJe suis triste", "\tELlanto", "\tB\xE5\x96\x82\xEF\xBD\x9E", "\tT\xE5\x96\x82\xEF\xBD\x9E", "\tK\xEC\x9D\x91~"}, 0x74, {0x074F, 0xFFFF, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x06, 0x15, 0x14, 0x00}, {0x06, 0x2B, 0x14, 0x01}, {0x05, 0x18, 0x1F, 0x00}, {0x05, 0x28, 0x1F, 0x01}, {0x21, 0x20, 0x2E, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\x8A\xE3\x81\x93\xE3\x81\xA3\xE3\x81\x9F\xEF\xBC\x81", "\tEI'm angry!", "\tEWeinen", "\tEJe suis en colere !", "\tEEnfado", "\tB\xE7\x94\x9F\xE6\xB0\x94\xE4\xBA\x86\xEF\xBC\x81", "\tT\xE7\x94\x9F\xE6\xB0\xA3\xE4\xBA\x86\xEF\xBC\x81", "\tK\xEB\x82\x98\xEC\x99\x94\xEB\x8B\xA4\xEF\xBC\x81"}, 0x5C, {0x0116, 0x0001, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x0B, 0x18, 0x1B, 0x01}, {0x0B, 0x28, 0x1B, 0x00}, {0x33, 0x20, 0x2A, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{{"\tJ\xE3\x81\x9F\xE3\x81\x99\xE3\x81\x91\xE3\x81\xA6\xEF\xBC\x81", "\tEHelp me!", "\tEHilf mir!", "\tEAide-moi !", "\tEAyuda", "\tB\xE6\x95\x91\xE5\x91\xBD\xE5\x95\x8A\xEF\xBC\x81", "\tT\xE6\x95\x91\xE5\x91\xBD\xE5\x95\x8A\xEF\xBC\x81", "\tK\xEB\x8F\x84\xEC\x99\x80\xEC\xA4\x98\xEF\xBC\x81"}, 0xEC, {0x065E, 0x0138, 0xFFFF, 0xFFFF}, {SymbolChatFacePart{0x02, 0x17, 0x1B, 0x01}, {0x02, 0x2A, 0x1B, 0x00}, {0x31, 0x20, 0x2C, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
};

static const array<uint16_t, 20> DEFAULT_TECH_MENU_CONFIG = {
    0x0000, 0x0006, 0x0003, 0x0001, 0x0007, 0x0004, 0x0002, 0x0008, 0x0005, 0x0009,
    0x0012, 0x000F, 0x0010, 0x0011, 0x000D, 0x000A, 0x000B, 0x000C, 0x000E, 0x0000};

static const array<uint8_t, 0x016C> DEFAULT_KEY_CONFIG = {
    0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

static const array<uint8_t, 0x0038> DEFAULT_JOYSTICK_CONFIG = {
    0x00, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

// Originally there was going to be a language-based header for .nsc files, but
// then I decided against it. This string was already in use for that parser,
// so I didn't bother changing it.
const char* LegacySavedAccountDataBB::SIGNATURE = "newserv account file format; 7 sections present; sequential;";

ShuffleTables::ShuffleTables(PSOV2Encryption& crypt) {
  for (size_t x = 0; x < 0x100; x++) {
    this->forward_table[x] = x;
  }

  int32_t r28 = 0xFF;
  uint8_t* r31 = &this->forward_table[0xFF];
  while (r28 >= 0) {
    uint32_t r3 = this->pseudorand(crypt, r28 + 1);
    if (r3 >= 0x100) {
      throw logic_error("bad r3");
    }
    uint8_t t = this->forward_table[r3];
    this->forward_table[r3] = *r31;
    *r31 = t;

    this->reverse_table[t] = r28;
    r31--;
    r28--;
  }
}

uint32_t ShuffleTables::pseudorand(PSOV2Encryption& crypt, uint32_t prev) {
  return (((prev & 0xFFFF) * ((crypt.next() >> 16) & 0xFFFF)) >> 16) & 0xFFFF;
}

void ShuffleTables::shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const {
  uint8_t* dest = reinterpret_cast<uint8_t*>(vdest);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(vsrc);
  const uint8_t* table = reverse ? this->reverse_table : this->forward_table;

  for (size_t block_offset = 0; block_offset < (size & 0xFFFFFF00); block_offset += 0x100) {
    for (size_t z = 0; z < 0x100; z++) {
      dest[block_offset + table[z]] = src[block_offset + z];
    }
  }

  // Any remaining bytes that don't fill an entire block are not shuffled
  memcpy(&dest[size & 0xFFFFFF00], &src[size & 0xFFFFFF00], size & 0xFF);
}

bool PSOVMSFileHeader::checksum_correct() const {
  auto add_data = +[](const void* data, size_t size, uint16_t crc) -> uint16_t {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    for (size_t z = 0; z < size; z++) {
      crc ^= (static_cast<uint16_t>(bytes[z]) << 8);
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (crc & 0x8000) {
          crc = (crc << 1) ^ 0x1021;
        } else {
          crc = (crc << 1);
        }
      }
    }
    return crc;
  };

  uint16_t crc = add_data(this, offsetof(PSOVMSFileHeader, crc), 0);
  crc = add_data("\0\0", 2, crc);
  crc = add_data(&this->data_size,
      sizeof(PSOVMSFileHeader) - offsetof(PSOVMSFileHeader, data_size) + this->num_icons * 0x200 + this->data_size, crc);
  return (crc == this->crc);
}

void PSOVMSFileHeader::check() const {
  if (!this->checksum_correct()) {
    throw runtime_error("VMS file unencrypted header checksum is incorrect");
  }
}

bool PSOVMSFileHeader::is_v2() const {
  return !memcmp(this->short_desc.data, "PSOV2", 5);
}

bool PSOGCIFileHeader::checksum_correct() const {
  uint32_t cs = phosg::crc32(&this->game_name, this->game_name.bytes());
  cs = phosg::crc32(&this->embedded_seed, sizeof(this->embedded_seed), cs);
  cs = phosg::crc32(&this->file_name, this->file_name.bytes(), cs);
  cs = phosg::crc32(&this->banner, this->banner.bytes(), cs);
  cs = phosg::crc32(&this->icon, this->icon.bytes(), cs);
  cs = phosg::crc32(&this->data_size, sizeof(this->data_size), cs);
  cs = phosg::crc32("\0\0\0\0", 4, cs); // this->checksum (treated as zero)
  return (cs == this->checksum);
}

void PSOGCIFileHeader::check() const {
  if (!this->checksum_correct()) {
    throw runtime_error("GCI file unencrypted header checksum is incorrect");
  }
  if (this->developer_id[0] != '8' || this->developer_id[1] != 'P') {
    throw runtime_error("GCI file is not for a Sega game");
  }
  if ((this->game_id[0] != 'G') && (this->game_id[0] != 'D')) {
    throw runtime_error("GCI file is not for a GameCube game");
  }
  if (this->game_id[1] != 'P') {
    throw runtime_error("GCI file is not for Phantasy Star Online");
  }
  if ((this->game_id[2] != 'S') && (this->game_id[2] != 'O')) {
    throw runtime_error("GCI file is not for Phantasy Star Online");
  }
}

bool PSOGCIFileHeader::is_ep12() const {
  return (this->game_id[2] == 'O');
}

bool PSOGCIFileHeader::is_ep3() const {
  return (this->game_id[2] == 'S');
}

bool PSOGCIFileHeader::is_nte() const {
  return (this->game_id[0] == 'D');
}

uint32_t compute_psogc_timestamp(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second) {
  static uint16_t month_start_day[12] = {
      0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  uint32_t year_start_day = ((year - 1998) >> 2) + (year - 2000) * 365;
  if ((((year - 1998) & 3) == 0) && (month < 3)) {
    year_start_day--;
  }
  uint32_t res_day = (day - 1) + year_start_day + month_start_day[month - 1];
  return second + (minute + (hour + (res_day * 24)) * 60) * 60;
}

string decrypt_gci_fixed_size_data_section_for_salvage(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    uint64_t round2_seed,
    size_t max_decrypt_bytes) {
  string decrypted = decrypt_data_section<true>(data_section, size, round1_seed, max_decrypt_bytes);

  PSOV2Encryption round2_crypt(round2_seed);
  round2_crypt.encrypt_big_endian(decrypted.data(), decrypted.size());

  return decrypted;
}

bool PSOGCSnapshotFile::checksum_correct() const {
  uint32_t crc = phosg::crc32("\0\0\0\0", 4);
  crc = phosg::crc32(&this->width, sizeof(*this) - sizeof(this->checksum), crc);
  return (crc == this->checksum);
}

static uint32_t decode_rgb565(uint16_t c) {
  // Input bits:                    rrrrrggg gggbbbbb
  // Output bits: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
  return ((c << 16) & 0xF8000000) | ((c << 11) & 0x07000000) | // R
      ((c << 13) & 0x00FC0000) | ((c << 7) & 0x00030000) | // G
      ((c << 11) & 0x0000F800) | ((c << 6) & 0x00000700) | // B
      0x000000FF; // A
}

phosg::Image PSOGCSnapshotFile::decode_image() const {
  size_t width = this->width ? this->width.load() : 256;
  size_t height = this->height ? this->height.load() : 192;
  if (width != 256) {
    throw runtime_error("width is incorrect");
  }
  if (height != 192) {
    throw runtime_error("height is incorrect");
  }

  // 4x4 blocks of pixels
  phosg::Image ret(width, height, false);
  size_t offset = 0;
  for (size_t y = 0; y < this->height; y += 4) {
    for (size_t x = 0; x < this->width; x += 4) {
      for (size_t yy = 0; yy < 4; yy++) {
        for (size_t xx = 0; xx < 4; xx++) {
          uint32_t color = decode_rgb565(this->pixels[offset++]);
          ret.write_pixel(x + xx, y + yy, color);
        }
      }
    }
  }
  return ret;
}

PSOGCEp3CharacterFile::Character::Character(const PSOGCEp3NTECharacter& nte)
    : inventory(nte.inventory),
      disp(nte.disp),
      validation_flags(nte.validation_flags),
      creation_timestamp(nte.creation_timestamp),
      signature(nte.signature),
      play_time_seconds(nte.play_time_seconds),
      option_flags(nte.option_flags),
      save_count(nte.save_count),
      ppp_username(nte.ppp_username),
      ppp_password(nte.ppp_password),
      seq_vars(nte.seq_vars),
      death_count(nte.death_count),
      bank(nte.bank),
      guild_card(nte.guild_card),
      symbol_chats(nte.symbol_chats),
      chat_shortcuts(nte.chat_shortcuts),
      auto_reply(nte.auto_reply),
      info_board(nte.info_board),
      battle_records(nte.battle_records),
      unknown_a10(nte.unknown_a10),
      challenge_record_stats(nte.challenge_record_stats),
      ep3_config(nte.ep3_config),
      unknown_a11(nte.unknown_a11),
      unknown_a12(nte.unknown_a12),
      unknown_a13(nte.unknown_a13) {
  this->ep3_config.backup_visual = this->disp.visual;
}

PSOGCEp3CharacterFile::Character::operator PSOGCEp3NTECharacter() const {
  PSOGCEp3NTECharacter ret;
  ret.inventory = this->inventory;
  ret.disp = this->disp;
  ret.validation_flags = this->validation_flags;
  ret.creation_timestamp = this->creation_timestamp;
  ret.signature = this->signature;
  ret.play_time_seconds = this->play_time_seconds;
  ret.option_flags = this->option_flags;
  ret.save_count = this->save_count;
  ret.ppp_username = this->ppp_username;
  ret.ppp_password = this->ppp_password;
  ret.seq_vars = this->seq_vars;
  ret.death_count = this->death_count;
  ret.bank = this->bank;
  ret.guild_card = this->guild_card;
  ret.symbol_chats = this->symbol_chats;
  ret.chat_shortcuts = this->chat_shortcuts;
  ret.auto_reply = this->auto_reply;
  ret.info_board = this->info_board;
  ret.battle_records = this->battle_records;
  ret.unknown_a10 = this->unknown_a10;
  ret.challenge_record_stats = this->challenge_record_stats;
  ret.ep3_config = Episode3::PlayerConfigNTE(this->ep3_config);
  ret.unknown_a11 = this->unknown_a11;
  ret.unknown_a12 = this->unknown_a12;
  ret.unknown_a13 = this->unknown_a13;
  return ret;
}

void PSOBBGuildCardFile::Entry::clear() {
  this->data.clear();
  this->unknown_a1.clear(0);
}

uint32_t PSOBBGuildCardFile::checksum() const {
  return phosg::crc32(this, sizeof(*this));
}

PSOBBBaseSystemFile::PSOBBBaseSystemFile() {
  // This field is based on 1/1/2000, not 1/1/1970, so adjust appropriately
  this->creation_timestamp = (phosg::now() - 946684800000000ULL) / 1000000;
  for (size_t z = 0; z < DEFAULT_KEY_CONFIG.size(); z++) {
    this->key_config[z] = DEFAULT_KEY_CONFIG[z];
  }
  for (size_t z = 0; z < DEFAULT_JOYSTICK_CONFIG.size(); z++) {
    this->joystick_config[z] = DEFAULT_JOYSTICK_CONFIG[z];
  }
}

PlayerDispDataBBPreview PSOBBCharacterFile::to_preview() const {
  PlayerDispDataBBPreview pre;
  pre.level = this->disp.stats.level;
  pre.experience = this->disp.stats.experience;
  pre.visual = this->disp.visual;
  pre.name = this->disp.name;
  pre.play_time_seconds = this->play_time_seconds;
  return pre;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_config(
    uint32_t guild_card_number,
    uint8_t language,
    const PlayerVisualConfig& visual,
    const std::string& name,
    shared_ptr<const LevelTable> level_table) {
  static const array<array<PlayerInventoryItem, 5>, 12> initial_inventory{{
      {
          PlayerInventoryItem(ItemData(0x0001000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0001000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0001000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0006000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0006000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0006000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x000A000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x000A000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x000A000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0001000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x000A000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
      {
          PlayerInventoryItem(ItemData(0x0006000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x0101000000000000, 0x0000000000000000), true),
          PlayerInventoryItem(ItemData(0x02000500F4010000, 0x0000000028000012), true),
          PlayerInventoryItem(ItemData(0x0300000000040000, 0x0000000000000000), false),
          PlayerInventoryItem(ItemData(0x0301000000040000, 0x0000000000000000), false),
      },
  }};

  static const array<uint8_t, 0xE8> config_hunter_ranger{
      {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00,
          0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
  static const array<uint8_t, 0xE8> config_force{
      {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00,
          0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

  auto ret = make_shared<PSOBBCharacterFile>();
  ret->disp.visual = visual;
  ret->disp.name.encode(name, language);

  const auto& initial_items = initial_inventory.at(visual.char_class);
  ret->inventory.num_items = initial_items.size();
  for (size_t z = 0; z < initial_items.size(); z++) {
    ret->inventory.items[z] = initial_items[z];
  }

  // Set mag color based on initial costume
  static const array<array<uint8_t, 25>, 12> mag_colors = {{
      {0x09, 0x01, 0x02, 0x11, 0x0A, 0x05, 0x06, 0x0B, 0x05, 0x00, 0x07, 0x0B, 0x0C, 0x04, 0x05, 0x06, 0x0E, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x01, 0x02, 0x11, 0x04, 0x05, 0x06, 0x08, 0x11, 0x0D, 0x01, 0x02, 0x0C, 0x04, 0x05, 0x06, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x01, 0x02, 0x11, 0x04, 0x0E, 0x06, 0x01, 0x0E, 0x09, 0x07, 0x02, 0x11, 0x04, 0x05, 0x06, 0x04, 0x11, 0x0D, 0x01, 0x0B, 0x11, 0x0D, 0x05, 0x06},
      {0x00, 0x01, 0x0B, 0x11, 0x04, 0x05, 0x06, 0x0F, 0x05, 0x09, 0x07, 0x02, 0x11, 0x04, 0x05, 0x0F, 0x06, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x01, 0x0B, 0x11, 0x0A, 0x05, 0x06, 0x06, 0x09, 0x09, 0x01, 0x02, 0x11, 0x0A, 0x0E, 0x06, 0x01, 0x04, 0x0D, 0x07, 0x01, 0x0C, 0x0A, 0x05, 0x06},
      {0x10, 0x07, 0x02, 0x11, 0x0A, 0x05, 0x0A, 0x00, 0x07, 0x00, 0x01, 0x08, 0x11, 0x04, 0x09, 0x0F, 0x0D, 0x02, 0x0A, 0x07, 0x02, 0x0C, 0x04, 0x0E, 0x0E},
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x10, 0x01, 0x00, 0x07, 0x02, 0x0C, 0x04, 0x05, 0x06, 0x10, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x0D, 0x01, 0x02, 0x11, 0x04, 0x05, 0x06, 0x00, 0x11, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x04, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x10, 0x05, 0x09, 0x01, 0x0B, 0x0C, 0x04, 0x05, 0x06, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x01, 0x02, 0x0C, 0x04, 0x05, 0x0F, 0x0A, 0x04, 0x0D, 0x01, 0x08, 0x11, 0x04, 0x05, 0x0F, 0x05, 0x10, 0x10, 0x07, 0x02, 0x0B, 0x0A, 0x0A, 0x0F},
      {0x00, 0x01, 0x0B, 0x0C, 0x04, 0x05, 0x06, 0x08, 0x0A, 0x0D, 0x07, 0x02, 0x11, 0x0A, 0x05, 0x06, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x07, 0x02, 0x11, 0x04, 0x05, 0x06, 0x09, 0x0C, 0x00, 0x01, 0x02, 0x11, 0x0D, 0x05, 0x10, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  }};
  uint8_t char_class = (visual.char_class > 0x0B) ? 0 : visual.char_class;
  uint8_t mag_color_index;
  if (char_class == 2 || char_class == 4 || char_class == 5 || char_class == 9) {
    mag_color_index = (visual.skin >= 25) ? 0 : visual.skin.load();
  } else {
    mag_color_index = (visual.costume >= 18) ? 0 : visual.costume.load();
  }
  ret->inventory.items[2].data.data2[3] = mag_colors.at(char_class).at(mag_color_index);

  ret->inventory.items[13].extension_data2 = 1;

  const auto& config = (ret->disp.visual.class_flags & 0x80) ? config_force : config_hunter_ranger;
  for (size_t z = 0; z < config.size(); z++) {
    ret->disp.config[z] = config[z];
  }

  if (level_table) {
    level_table->reset_to_base(ret->disp.stats, ret->disp.visual.char_class);
  }
  ret->disp.technique_levels_v1.clear(0xFF);
  if (ret->disp.visual.class_flags & 0x80) {
    ret->disp.technique_levels_v1[0] = 0x00; // Forces start with Foie Lv.1
  }
  ret->inventory.language = language;
  ret->guild_card.guild_card_number = guild_card_number;
  ret->guild_card.name = ret->disp.name;
  ret->guild_card.present = 1;
  ret->guild_card.language = ret->inventory.language;
  ret->guild_card.section_id = ret->disp.visual.section_id;
  ret->guild_card.char_class = ret->disp.visual.char_class;
  for (size_t z = 0; z < DEFAULT_SYMBOL_CHATS.size(); z++) {
    ret->symbol_chats[z] = DEFAULT_SYMBOL_CHATS[z].to_entry(language);
  }
  for (size_t z = 0; z < DEFAULT_TECH_MENU_CONFIG.size(); z++) {
    ret->tech_menu_shortcut_entries[z] = DEFAULT_TECH_MENU_CONFIG[z];
  }
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_preview(
    uint32_t guild_card_number,
    uint8_t language,
    const PlayerDispDataBBPreview& preview,
    shared_ptr<const LevelTable> level_table) {
  return PSOBBCharacterFile::create_from_config(
      guild_card_number, language, preview.visual, preview.name.decode(language), level_table);
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSODCNTECharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      0,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  ret->inventory.decode_from_client(Version::DC_V1);
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags;
  ret->creation_timestamp = src.creation_timestamp;
  ret->play_time_seconds = src.play_time_seconds;
  ret->option_flags = src.option_flags;
  ret->save_count = 1;
  for (size_t difficulty = 0; difficulty < 4; difficulty++) {
    // Copy the first half of the quest flags to all difficulties
    ret->quest_flags.data[difficulty].data = src.quest_flags;
  }
  ret->bank.meseta = src.bank_meseta.load();
  ret->bank.num_items = src.num_bank_items.load();
  for (size_t z = 0; z < std::min<size_t>(ret->bank.items.size(), src.bank_items.size()); z++) {
    auto& dest_item = ret->bank.items[z];
    dest_item.data = src.bank_items[z];
    dest_item.data.decode_for_version(Version::DC_NTE);
    dest_item.amount = dest_item.data.get_tool_item_amount(ItemData::StackLimits::DEFAULT_STACK_LIMITS_DC_NTE);
  }
  ret->bank.decode_from_client(Version::DC_V1);
  ret->guild_card = src.guild_card;
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSODC112000CharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  ret->inventory.decode_from_client(Version::DC_V1);
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags;
  ret->creation_timestamp = src.creation_timestamp;
  ret->play_time_seconds = src.play_time_seconds;
  ret->option_flags = src.option_flags;
  ret->save_count = 1;
  for (size_t difficulty = 0; difficulty < 4; difficulty++) {
    // Copy the first half of the quest flags to all difficulties
    ret->quest_flags.data[difficulty].data = src.quest_flags;
  }
  ret->bank.meseta = src.bank_meseta.load();
  ret->bank.num_items = src.num_bank_items.load();
  for (size_t z = 0; z < std::min<size_t>(ret->bank.items.size(), src.bank_items.size()); z++) {
    auto& dest_item = ret->bank.items[z];
    dest_item.data = src.bank_items[z];
    dest_item.data.decode_for_version(Version::DC_NTE);
    dest_item.amount = dest_item.data.get_tool_item_amount(ItemData::StackLimits::DEFAULT_STACK_LIMITS_DC_NTE);
  }
  ret->bank.decode_from_client(Version::DC_V1);
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& src_sc = src.symbol_chats[z];
    ret_sc.present = src_sc.present.load();
    ret_sc.name.encode(src_sc.name.decode(language), language);
    ret_sc.spec = src_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSODCV1CharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  ret->inventory.decode_from_client(Version::DC_V1);
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags;
  ret->creation_timestamp = src.creation_timestamp;
  ret->play_time_seconds = src.play_time_seconds;
  ret->option_flags = src.option_flags;
  ret->save_count = src.save_count;
  ret->quest_flags = src.quest_flags;
  ret->bank = src.bank;
  ret->bank.decode_from_client(Version::DC_V1);
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& src_sc = src.symbol_chats[z];
    ret_sc.present = src_sc.present.load();
    ret_sc.name.encode(src_sc.name.decode(language), language);
    ret_sc.spec = src_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSODCV2CharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  ret->inventory.decode_from_client(Version::DC_V2);
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags;
  ret->creation_timestamp = src.creation_timestamp;
  ret->play_time_seconds = src.play_time_seconds;
  ret->option_flags = src.option_flags;
  ret->save_count = src.save_count;
  ret->quest_flags = src.quest_flags;
  ret->bank = src.bank;
  ret->bank.decode_from_client(Version::DC_V2);
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& src_sc = src.symbol_chats[z];
    ret_sc.present = src_sc.present.load();
    ret_sc.name.encode(src_sc.name.decode(language), language);
    ret_sc.spec = src_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  ret->battle_records = src.battle_records;
  ret->challenge_records = src.challenge_records;
  ret->tech_menu_shortcut_entries = src.tech_menu_shortcut_entries;
  for (size_t z = 0; z < 5; z++) {
    ret->choice_search_config.entries[z].parent_choice_id = src.choice_search_config[z * 2].load();
    ret->choice_search_config.entries[z].choice_id = src.choice_search_config[z * 2 + 1].load();
  }
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSOGCNTECharacterFileCharacter& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  // Note: We intentionally do not call ret->inventory.decode_from_client here.
  // This is because the GC client byteswaps data2 in each item before sending
  // it to the server in the 61 and 98 commands, but GetExtendedPlayerInfo does
  // not do this, so the data2 fields are already in the correct order here.
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags.load();
  ret->creation_timestamp = src.creation_timestamp.load();
  ret->play_time_seconds = src.play_time_seconds.load();
  ret->option_flags = src.option_flags.load();
  ret->save_count = src.save_count.load();
  ret->quest_flags = src.quest_flags;
  ret->bank = src.bank;
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& gc_sc = src.symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  ret->battle_records = src.battle_records;
  ret->unknown_a4 = src.unknown_a4;
  ret->challenge_records = src.challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret->tech_menu_shortcut_entries.size(), src.tech_menu_shortcut_entries.size()); z++) {
    ret->tech_menu_shortcut_entries[z] = src.tech_menu_shortcut_entries[z].load();
  }
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSOGCCharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  // Note: We intentionally do not call ret->inventory.decode_from_client here.
  // This is because the GC client byteswaps data2 in each item before sending
  // it to the server in the 61 and 98 commands, but GetExtendedPlayerInfo does
  // not do this, so the data2 fields are already in the correct order here.
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags.load();
  ret->creation_timestamp = src.creation_timestamp.load();
  ret->play_time_seconds = src.play_time_seconds.load();
  ret->option_flags = src.option_flags.load();
  ret->save_count = src.save_count.load();
  ret->quest_flags = src.quest_flags;
  ret->death_count = src.death_count.load();
  ret->bank = src.bank;
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& src_sc = src.symbol_chats[z];
    ret_sc.present = src_sc.present.load();
    ret_sc.name.encode(src_sc.name.decode(language), language);
    ret_sc.spec = src_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  ret->auto_reply.encode(src.auto_reply.decode(language), language);
  ret->info_board.encode(src.info_board.decode(language), language);
  ret->battle_records = src.battle_records;
  ret->unknown_a4 = src.unknown_a4;
  ret->challenge_records = src.challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret->tech_menu_shortcut_entries.size(), src.tech_menu_shortcut_entries.size()); z++) {
    ret->tech_menu_shortcut_entries[z] = src.tech_menu_shortcut_entries[z].load();
  }
  ret->choice_search_config = src.choice_search_config;
  ret->unknown_a6 = src.unknown_a6;
  for (size_t z = 0; z < std::min<size_t>(ret->quest_counters.size(), src.quest_counters.size()); z++) {
    ret->quest_counters[z] = src.quest_counters[z].load();
  }
  ret->offline_battle_records = src.offline_battle_records;
  ret->unknown_a7 = src.unknown_a7;
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSOGCEp3CharacterFile::Character& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags.load();
  ret->creation_timestamp = src.creation_timestamp.load();
  ret->play_time_seconds = src.play_time_seconds.load();
  ret->option_flags = src.option_flags.load();
  ret->save_count = src.save_count.load();
  ret->death_count = src.death_count.load();
  ret->bank = src.bank;
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& gc_sc = src.symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.chat_shortcuts.size()); z++) {
    ret->shortcuts[z] = src.chat_shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  ret->auto_reply.encode(src.auto_reply.decode(language), language);
  ret->info_board.encode(src.info_board.decode(language), language);
  ret->battle_records = src.battle_records;
  ret->unknown_a4 = src.ep3_config.unknown_a4;
  ret->challenge_records.rank_title.encode(src.ep3_config.rank_text.decode(language), language);
  for (size_t z = 0; z < std::min<size_t>(ret->tech_menu_shortcut_entries.size(), src.ep3_config.tech_menu_shortcut_entries.size()); z++) {
    ret->tech_menu_shortcut_entries[z] = src.ep3_config.tech_menu_shortcut_entries[z].load();
  }
  ret->choice_search_config.disabled = !!(ret->option_flags & 0x00040000);
  for (size_t z = 0; z < 5; z++) {
    ret->choice_search_config.entries[z].parent_choice_id = src.ep3_config.choice_search_config[z * 2].load();
    ret->choice_search_config.entries[z].choice_id = src.ep3_config.choice_search_config[z * 2 + 1].load();
  }
  for (size_t z = 0; z < std::min<size_t>(ret->quest_counters.size(), src.ep3_config.scenario_progress.size()); z++) {
    ret->quest_counters[z] = src.ep3_config.scenario_progress[z].load();
  }
  ret->offline_battle_records = src.ep3_config.unused_offline_records;
  return ret;
}

shared_ptr<PSOBBCharacterFile> PSOBBCharacterFile::create_from_file(const PSOXBCharacterFileCharacter& src) {
  auto ret = PSOBBCharacterFile::create_from_config(
      src.guild_card.guild_card_number,
      src.inventory.language,
      src.disp.visual,
      src.disp.visual.name.decode(0),
      nullptr);
  ret->inventory = src.inventory;
  ret->inventory.decode_from_client(Version::XB_V3);
  uint8_t language = ret->inventory.language;
  ret->disp = src.disp.to_bb(language, language);
  ret->validation_flags = src.validation_flags;
  ret->creation_timestamp = src.creation_timestamp.load();
  ret->play_time_seconds = src.play_time_seconds.load();
  ret->option_flags = src.option_flags.load();
  ret->save_count = src.save_count.load();
  ret->quest_flags = src.quest_flags;
  ret->death_count = src.death_count.load();
  ret->bank = src.bank;
  ret->bank.decode_from_client(Version::XB_V3);
  ret->guild_card = src.guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret->symbol_chats.size(), src.symbol_chats.size()); z++) {
    auto& ret_sc = ret->symbol_chats[z];
    const auto& src_sc = src.symbol_chats[z];
    ret_sc.present = src_sc.present.load();
    ret_sc.name.encode(src_sc.name.decode(language), language);
    ret_sc.spec = src_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret->shortcuts.size(), src.shortcuts.size()); z++) {
    ret->shortcuts[z] = src.shortcuts[z].convert<false, TextEncoding::UTF16, 0x50>(language);
  }
  ret->auto_reply.encode(src.auto_reply.decode(language), language);
  ret->info_board.encode(src.info_board.decode(language), language);
  ret->battle_records = src.battle_records;
  ret->unknown_a4 = src.unknown_a4;
  ret->challenge_records = src.challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret->tech_menu_shortcut_entries.size(), src.tech_menu_shortcut_entries.size()); z++) {
    ret->tech_menu_shortcut_entries[z] = src.tech_menu_shortcut_entries[z].load();
  }
  ret->choice_search_config = src.choice_search_config;
  ret->unknown_a6 = src.unknown_a6;
  for (size_t z = 0; z < std::min<size_t>(ret->quest_counters.size(), src.quest_counters.size()); z++) {
    ret->quest_counters[z] = src.quest_counters[z].load();
  }
  ret->offline_battle_records = src.offline_battle_records;
  ret->unknown_a7 = src.unknown_a7;
  return ret;
}

PSOBBCharacterFile::operator PSODCNTECharacterFile::Character() const {
  uint8_t language = this->inventory.language;

  PSODCNTECharacterFile::Character ret;
  ret.inventory = this->inventory;
  // We don't need to do the v1-compatible encoding (hence it is OK to pass
  // nullptr here) but we do need to encode mag stats in the v2 format
  ret.inventory.encode_for_client(Version::DC_NTE, nullptr);
  ret.disp = this->disp.to_dcpcv3<false>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::DC_V2);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.quest_flags.clear(0);
  ret.quest_flags = this->quest_flags.data[0].data; // Just use Normal difficulty flags
  ret.bank_meseta = this->bank.meseta.load();
  ret.num_bank_items = this->bank.num_items.load();
  for (size_t z = 0; z < std::min<size_t>(ret.bank_items.size(), this->bank.items.size()); z++) {
    auto& dest_item = ret.bank_items[z];
    dest_item = this->bank.items[z].data;
    dest_item.encode_for_version(Version::DC_NTE, nullptr);
  }
  return ret;
}

PSOBBCharacterFile::operator PSODC112000CharacterFile::Character() const {
  uint8_t language = this->inventory.language;

  PSODC112000CharacterFile::Character ret;
  ret.inventory = this->inventory;
  // We don't need to do the v1-compatible encoding (hence it is OK to pass
  // nullptr here) but we do need to encode mag stats in the v2 format
  ret.inventory.encode_for_client(Version::DC_11_2000, nullptr);
  ret.disp = this->disp.to_dcpcv3<false>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::DC_V2);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count_since_last_inventory_erasure = this->save_count.load();
  ret.quest_flags = this->quest_flags.data[0].data; // Just use Normal difficulty flags
  ret.bank_meseta = this->bank.meseta.load();
  ret.num_bank_items = this->bank.num_items.load();
  for (size_t z = 0; z < std::min<size_t>(ret.bank_items.size(), this->bank.items.size()); z++) {
    auto& dest_item = ret.bank_items[z];
    dest_item = this->bank.items[z].data;
    dest_item.encode_for_version(Version::DC_NTE, nullptr);
  }
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<false, TextEncoding::MARKED, 0x3C>(language);
  }
  return ret;
}

PSOBBCharacterFile::operator PSODCV1CharacterFile::Character() const {
  uint8_t language = this->inventory.language;

  PSODCV1CharacterFile::Character ret;
  ret.inventory = this->inventory;
  // We don't need to do the v1-compatible encoding (hence it is OK to pass
  // nullptr here) but we do need to encode mag stats in the v2 format
  ret.inventory.encode_for_client(Version::DC_V1, nullptr);
  ret.disp = this->disp.to_dcpcv3<false>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::DC_V2);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count = this->save_count.load();
  ret.quest_flags = this->quest_flags;
  ret.bank = this->bank;
  ret.bank.encode_for_client(Version::DC_V1);
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<false, TextEncoding::MARKED, 0x3C>(language);
  }
  return ret;
}

PSOBBCharacterFile::operator PSODCV2CharacterFile::Character() const {
  uint8_t language = this->inventory.language;

  PSODCV2CharacterFile::Character ret;
  ret.inventory = this->inventory;
  // We don't need to do the v1-compatible encoding (hence it is OK to pass
  // nullptr here) but we do need to encode mag stats in the v2 format
  ret.inventory.encode_for_client(Version::DC_V2, nullptr);
  ret.disp = this->disp.to_dcpcv3<false>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::DC_V2);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count = this->save_count.load();
  ret.quest_flags = this->quest_flags;
  ret.bank = this->bank;
  ret.bank.encode_for_client(Version::DC_V2);
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<false, TextEncoding::MARKED, 0x3C>(language);
  }
  ret.battle_records = this->battle_records;
  ret.challenge_records = this->challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret.tech_menu_shortcut_entries.size(), this->tech_menu_shortcut_entries.size()); z++) {
    ret.tech_menu_shortcut_entries[z] = this->tech_menu_shortcut_entries[z].load();
  }
  for (size_t z = 0; z < 5; z++) {
    ret.choice_search_config[z * 2] = this->choice_search_config.entries[z].parent_choice_id.load();
    ret.choice_search_config[z * 2 + 1] = this->choice_search_config.entries[z].choice_id.load();
  }
  return ret;
}

PSOBBCharacterFile::operator PSOGCNTECharacterFileCharacter() const {
  uint8_t language = this->inventory.language;

  PSOGCNTECharacterFileCharacter ret;
  ret.inventory = this->inventory;
  // Note: We intentionally do not call ret.inventory.encode_for_client here.
  // This is because the GC client byteswaps data2 in each item before sending
  // it to the server in the 61 and 98 commands, but GetExtendedPlayerInfo does
  // not do this, so the data2 fields are already in the correct order here.
  ret.disp = this->disp.to_dcpcv3<true>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::GC_V3);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count = this->save_count.load();
  ret.quest_flags = this->quest_flags;
  ret.bank = this->bank;
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<true, TextEncoding::MARKED, 0x50>(language);
  }
  ret.battle_records = this->battle_records;
  ret.unknown_a4 = this->unknown_a4;
  ret.challenge_records = this->challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret.tech_menu_shortcut_entries.size(), this->tech_menu_shortcut_entries.size()); z++) {
    ret.tech_menu_shortcut_entries[z] = this->tech_menu_shortcut_entries[z].load();
  }
  return ret;
}

PSOBBCharacterFile::operator PSOGCCharacterFile::Character() const {
  uint8_t language = this->inventory.language;

  PSOGCCharacterFile::Character ret;
  ret.inventory = this->inventory;
  // Note: We intentionally do not call ret.inventory.encode_for_client here.
  // This is because the GC client byteswaps data2 in each item before sending
  // it to the server in the 61 and 98 commands, but GetExtendedPlayerInfo does
  // not do this, so the data2 fields are already in the correct order here.
  ret.disp = this->disp.to_dcpcv3<true>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::GC_V3);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count = this->save_count.load();
  ret.quest_flags = this->quest_flags;
  ret.death_count = this->death_count.load();
  ret.bank = this->bank;
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<true, TextEncoding::MARKED, 0x50>(language);
  }
  ret.auto_reply.encode(this->auto_reply.decode(language), language);
  ret.info_board.encode(this->info_board.decode(language), language);
  ret.battle_records = this->battle_records;
  ret.unknown_a4 = this->unknown_a4;
  ret.challenge_records = this->challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret.tech_menu_shortcut_entries.size(), this->tech_menu_shortcut_entries.size()); z++) {
    ret.tech_menu_shortcut_entries[z] = this->tech_menu_shortcut_entries[z].load();
  }
  ret.choice_search_config = this->choice_search_config;
  ret.unknown_a6 = this->unknown_a6;
  for (size_t z = 0; z < std::min<size_t>(ret.quest_counters.size(), this->quest_counters.size()); z++) {
    ret.quest_counters[z] = this->quest_counters[z].load();
  }
  ret.offline_battle_records = this->offline_battle_records;
  ret.unknown_a7 = this->unknown_a7;
  return ret;
}

PSOBBCharacterFile::operator PSOXBCharacterFileCharacter() const {
  uint8_t language = this->inventory.language;

  PSOXBCharacterFileCharacter ret;
  ret.inventory = this->inventory;
  ret.inventory.encode_for_client(Version::XB_V3, nullptr);
  ret.disp = this->disp.to_dcpcv3<false>(language, language);
  ret.disp.visual.enforce_lobby_join_limits_for_version(Version::XB_V3);
  ret.validation_flags = this->validation_flags.load();
  ret.creation_timestamp = this->creation_timestamp.load();
  ret.play_time_seconds = this->play_time_seconds.load();
  ret.option_flags = this->option_flags.load();
  ret.save_count = this->save_count.load();
  ret.quest_flags = this->quest_flags;
  ret.death_count = this->death_count.load();
  ret.bank = this->bank;
  ret.bank.encode_for_client(Version::XB_V3);
  ret.guild_card = this->guild_card;
  for (size_t z = 0; z < std::min<size_t>(ret.symbol_chats.size(), this->symbol_chats.size()); z++) {
    auto& ret_sc = ret.symbol_chats[z];
    const auto& gc_sc = this->symbol_chats[z];
    ret_sc.present = gc_sc.present.load();
    ret_sc.name.encode(gc_sc.name.decode(language), language);
    ret_sc.spec = gc_sc.spec;
  }
  for (size_t z = 0; z < std::min<size_t>(ret.shortcuts.size(), this->shortcuts.size()); z++) {
    ret.shortcuts[z] = this->shortcuts[z].convert<false, TextEncoding::MARKED, 0x50>(language);
  }
  ret.auto_reply.encode(this->auto_reply.decode(language), language);
  ret.info_board.encode(this->info_board.decode(language), language);
  ret.battle_records = this->battle_records;
  ret.unknown_a4 = this->unknown_a4;
  ret.challenge_records = this->challenge_records;
  for (size_t z = 0; z < std::min<size_t>(ret.tech_menu_shortcut_entries.size(), this->tech_menu_shortcut_entries.size()); z++) {
    ret.tech_menu_shortcut_entries[z] = this->tech_menu_shortcut_entries[z].load();
  }
  ret.choice_search_config = this->choice_search_config;
  ret.unknown_a6 = this->unknown_a6;
  for (size_t z = 0; z < std::min<size_t>(ret.quest_counters.size(), this->quest_counters.size()); z++) {
    ret.quest_counters[z] = this->quest_counters[z].load();
  }
  ret.offline_battle_records = this->offline_battle_records;
  ret.unknown_a7 = this->unknown_a7;
  return ret;
}

LoadedPSOCHARFile load_psochar(const string& filename, bool load_system) {
  auto f = phosg::fopen_unique(filename, "rb");
  auto header = phosg::freadx<PSOCommandHeaderBB>(f.get());
  if (header.size != 0x399C) {
    throw runtime_error("incorrect size in character file header");
  }
  if (header.command != 0x00E7) {
    throw runtime_error("incorrect command in character file header");
  }
  if (header.flag != 0x00000000) {
    throw runtime_error("incorrect flag in character file header");
  }
  static_assert(sizeof(PSOBBCharacterFile) + sizeof(PSOBBBaseSystemFile) + sizeof(PSOBBTeamMembership) == 0x3994, ".psochar size is incorrect");

  LoadedPSOCHARFile ret;
  ret.character_file = make_shared<PSOBBCharacterFile>(phosg::freadx<PSOBBCharacterFile>(f.get()));
  if (load_system) {
    ret.system_file = make_shared<PSOBBBaseSystemFile>(phosg::freadx<PSOBBBaseSystemFile>(f.get()));
  }
  return ret;
}

void save_psochar(
    const std::string& filename,
    std::shared_ptr<const PSOBBBaseSystemFile> system,
    std::shared_ptr<const PSOBBCharacterFile> character) {
  auto f = phosg::fopen_unique(filename, "wb");
  PSOCommandHeaderBB header = {sizeof(PSOCommandHeaderBB) + sizeof(PSOBBCharacterFile) + sizeof(PSOBBBaseSystemFile) + sizeof(PSOBBTeamMembership), 0x00E7, 0x00000000};
  phosg::fwritex(f.get(), header);
  phosg::fwritex(f.get(), *character);
  phosg::fwritex(f.get(), *system);
  // TODO: Technically, we should write the actual team membership struct to
  // the file here, but that would cause Client to depend on Account, which it
  // currently does not. This data doesn't matter at all for correctness within
  // newserv, since it ignores this data entirely and instead generates the
  // membership struct from the team ID in the Account and the team's state.
  // So, writing correct data here would mostly be for compatibility with other
  // PSO servers. But if the other server is newserv, then this data wouldn't
  // be used anyway, and if it's not, then it would presumably have a different
  // set of teams with a different set of team IDs anyway, so the membership
  // struct here would be useless either way.
  static const PSOBBTeamMembership empty_membership;
  phosg::fwritex(f.get(), empty_membership);
}

// TODO: Eliminate duplication between this function and the parallel function
// in PlayerBankT
void PSOBBCharacterFile::add_item(const ItemData& item, const ItemData::StackLimits& limits) {
  uint32_t primary_identifier = item.primary_identifier();

  // Annoyingly, meseta is in the disp data, not in the inventory struct. If the
  // item is meseta, we have to modify disp instead.
  if (primary_identifier == 0x04000000) {
    this->add_meseta(item.data2d);
    return;
  }

  // Handle combinable items
  size_t combine_max = item.max_stack_size(limits);
  if (combine_max > 1) {
    // Get the item index if there's already a stack of the same item in the
    // player's inventory
    size_t y;
    for (y = 0; y < this->inventory.num_items; y++) {
      if (this->inventory.items[y].data.primary_identifier() == primary_identifier) {
        break;
      }
    }

    // If we found an existing stack, add it to the total and return
    if (y < this->inventory.num_items) {
      size_t new_stack_size = this->inventory.items[y].data.data1[5] + item.data1[5];
      if (new_stack_size > combine_max) {
        throw out_of_range("stack is too large");
      }
      this->inventory.items[y].data.data1[5] = new_stack_size;
      return;
    }
  }

  // If we get here, then it's not meseta and not a combine item, so it needs to
  // go into an empty inventory slot
  if (this->inventory.num_items >= 30) {
    throw out_of_range("inventory is full");
  }
  auto& inv_item = this->inventory.items[this->inventory.num_items];
  inv_item.present = 1;
  inv_item.unknown_a1 = 0;
  inv_item.flags = 0;
  inv_item.data = item;
  this->inventory.num_items++;
}

// TODO: Eliminate code duplication between this function and the parallel
// function in PlayerBankT
ItemData PSOBBCharacterFile::remove_item(uint32_t item_id, uint32_t amount, const ItemData::StackLimits& limits) {
  ItemData ret;

  // If we're removing meseta (signaled by an invalid item ID), then create a
  // meseta item.
  if (item_id == 0xFFFFFFFF) {
    this->remove_meseta(amount, !is_v4(limits.version));
    ret.data1[0] = 0x04;
    ret.data2d = amount;
    return ret;
  }

  size_t index = this->inventory.find_item(item_id);
  auto& inventory_item = this->inventory.items[index];
  bool is_equipped = (inventory_item.flags & 0x00000008);

  // If the item is a combine item and are we removing less than we have of it,
  // then create a new item and reduce the amount of the existing stack. Note
  // that passing amount == 0 means to remove the entire stack, so this only
  // applies if amount is nonzero.
  if (amount && (inventory_item.data.stack_size(limits) > 1) &&
      (amount < inventory_item.data.data1[5])) {
    if (is_equipped) {
      throw runtime_error("character has a combine item equipped");
    }
    ret = inventory_item.data;
    ret.data1[5] = amount;
    ret.id = 0xFFFFFFFF;
    inventory_item.data.data1[5] -= amount;
    return ret;
  }

  // If we get here, then it's not meseta, and either it's not a combine item or
  // we're removing the entire stack. Delete the item from the inventory slot
  // and return the deleted item.
  if (is_equipped) {
    this->inventory.unequip_item_index(index);
  }
  ret = inventory_item.data;
  this->inventory.num_items--;
  for (size_t x = index; x < this->inventory.num_items; x++) {
    auto& to_item = this->inventory.items[x];
    const auto& from_item = this->inventory.items[x + 1];
    to_item.present = from_item.present;
    to_item.unknown_a1 = from_item.unknown_a1;
    to_item.flags = from_item.flags;
    to_item.data = from_item.data;
  }
  auto& last_item = this->inventory.items[this->inventory.num_items];
  last_item.present = 0;
  last_item.unknown_a1 = 0;
  last_item.flags = 0;
  last_item.data.clear();
  return ret;
}

void PSOBBCharacterFile::add_meseta(uint32_t amount) {
  this->disp.stats.meseta = min<size_t>(static_cast<size_t>(this->disp.stats.meseta) + amount, 999999);
}

void PSOBBCharacterFile::remove_meseta(uint32_t amount, bool allow_overdraft) {
  if (amount <= this->disp.stats.meseta) {
    this->disp.stats.meseta -= amount;
  } else if (allow_overdraft) {
    this->disp.stats.meseta = 0;
  } else {
    throw out_of_range("player does not have enough meseta");
  }
}

uint8_t PSOBBCharacterFile::get_technique_level(uint8_t which) const {
  return (this->disp.technique_levels_v1[which] == 0xFF)
      ? 0xFF
      : (this->disp.technique_levels_v1[which] + this->inventory.items[which].extension_data1);
}

void PSOBBCharacterFile::set_technique_level(uint8_t which, uint8_t level) {
  if (level == 0xFF) {
    this->disp.technique_levels_v1[which] = 0xFF;
    this->inventory.items[which].extension_data1 = 0x00;
  } else if (level <= 0x0E) {
    this->disp.technique_levels_v1[which] = level;
    this->inventory.items[which].extension_data1 = 0x00;
  } else {
    this->disp.technique_levels_v1[which] = 0x0E;
    this->inventory.items[which].extension_data1 = level - 0x0E;
  }
}

uint8_t PSOBBCharacterFile::get_material_usage(MaterialType which) const {
  switch (which) {
    case MaterialType::HP:
      return this->inventory.hp_from_materials >> 1;
    case MaterialType::TP:
      return this->inventory.tp_from_materials >> 1;
    case MaterialType::POWER:
    case MaterialType::MIND:
    case MaterialType::EVADE:
    case MaterialType::DEF:
    case MaterialType::LUCK:
      return this->inventory.items[8 + static_cast<uint8_t>(which)].extension_data2;
    default:
      throw logic_error("invalid material type");
  }
}

void PSOBBCharacterFile::set_material_usage(MaterialType which, uint8_t usage) {
  switch (which) {
    case MaterialType::HP:
      this->inventory.hp_from_materials = usage << 1;
      break;
    case MaterialType::TP:
      this->inventory.tp_from_materials = usage << 1;
      break;
    case MaterialType::POWER:
    case MaterialType::MIND:
    case MaterialType::EVADE:
    case MaterialType::DEF:
    case MaterialType::LUCK:
      this->inventory.items[8 + static_cast<uint8_t>(which)].extension_data2 = usage;
      break;
    default:
      throw logic_error("invalid material type");
  }
}

void PSOBBCharacterFile::clear_all_material_usage() {
  this->inventory.hp_from_materials = 0;
  this->inventory.tp_from_materials = 0;
  for (size_t z = 0; z < 5; z++) {
    this->inventory.items[z + 8].extension_data2 = 0;
  }
}

void PSOBBCharacterFile::import_tethealla_material_usage(std::shared_ptr<const LevelTable> level_table) {
  // Tethealla (Ephinea) doesn't store material counts anywhere in the file,
  // so if the material counts in the inventory extension data are all zero,
  // check the current stats against the expected stats for the character's
  // current level and set the material counts if they make sense.
  if (this->get_material_usage(PSOBBCharacterFile::MaterialType::POWER) |
      this->get_material_usage(PSOBBCharacterFile::MaterialType::MIND) |
      this->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE) |
      this->get_material_usage(PSOBBCharacterFile::MaterialType::DEF) |
      this->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK)) {
    return;
  }

  PlayerStats level_base_stats = this->disp.stats;
  level_table->reset_to_base(level_base_stats, this->disp.visual.char_class);
  level_table->advance_to_level(level_base_stats, this->disp.stats.level, this->disp.visual.char_class);

  uint64_t pow = (this->disp.stats.char_stats.atp - level_base_stats.char_stats.atp) / 2;
  uint64_t mind = (this->disp.stats.char_stats.mst - level_base_stats.char_stats.mst) / 2;
  uint64_t evade = (this->disp.stats.char_stats.evp - level_base_stats.char_stats.evp) / 2;
  uint64_t def = (this->disp.stats.char_stats.dfp - level_base_stats.char_stats.dfp) / 2;
  uint64_t luck = (this->disp.stats.char_stats.lck - level_base_stats.char_stats.lck) / 2;

  // We intentionally do not check any limits here. This is because on pre-v3,
  // there are no limits, and we don't want to reject legitimate characters
  // that have used more than 250 materials.

  this->set_material_usage(MaterialType::POWER, pow);
  this->set_material_usage(MaterialType::MIND, mind);
  this->set_material_usage(MaterialType::EVADE, evade);
  this->set_material_usage(MaterialType::DEF, def);
  this->set_material_usage(MaterialType::LUCK, luck);
}

void PSOBBCharacterFile::recompute_stats(std::shared_ptr<const LevelTable> level_table) {
  uint32_t level = this->disp.stats.level;
  uint32_t exp = this->disp.stats.experience;
  level_table->reset_to_base(this->disp.stats, this->disp.visual.char_class);
  level_table->advance_to_level(this->disp.stats, level, this->disp.visual.char_class);
  this->disp.stats.experience = exp;

  this->disp.stats.char_stats.atp += (this->get_material_usage(MaterialType::POWER) * 2);
  this->disp.stats.char_stats.mst += (this->get_material_usage(MaterialType::MIND) * 2);
  this->disp.stats.char_stats.evp += (this->get_material_usage(MaterialType::EVADE) * 2);
  this->disp.stats.char_stats.dfp += (this->get_material_usage(MaterialType::DEF) * 2);
  this->disp.stats.char_stats.lck += (this->get_material_usage(MaterialType::LUCK) * 2);
  // Note: HP in this structure is unaffected by material usage
}

static uint16_t crc16(const void* data, size_t size) {
  static const uint16_t table[0x100] = {
      // clang-format off
      /* 00 */ 0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
      /* 08 */ 0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
      /* 10 */ 0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
      /* 18 */ 0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
      /* 20 */ 0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
      /* 28 */ 0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
      /* 30 */ 0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
      /* 38 */ 0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
      /* 40 */ 0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
      /* 48 */ 0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
      /* 50 */ 0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
      /* 58 */ 0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
      /* 60 */ 0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
      /* 68 */ 0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
      /* 70 */ 0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
      /* 78 */ 0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
      /* 80 */ 0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
      /* 88 */ 0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
      /* 90 */ 0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
      /* 98 */ 0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
      /* A0 */ 0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
      /* A8 */ 0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
      /* B0 */ 0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
      /* B8 */ 0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
      /* C0 */ 0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
      /* C8 */ 0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
      /* D0 */ 0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
      /* D8 */ 0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
      /* E0 */ 0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
      /* E8 */ 0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
      /* F0 */ 0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
      /* F8 */ 0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78,
      // clang-format on
  };

  uint16_t ret = 0xFFFF;
  phosg::StringReader r(data, size);
  while (!r.eof()) {
    ret = (ret >> 8) ^ table[r.get_u8() ^ (ret & 0xFF)];
  }
  return ret ^ 0xFFFF;
}

string encode_psobb_hangame_credentials(const string& user_id, const string& token, const string& unused) {
  if (user_id.size() < 4) {
    throw runtime_error("user_id must be at least 4 characters");
  }
  if (user_id.size() > 12) {
    throw runtime_error("user_id must be at most 12 characters");
  }
  if (!phosg::ends_with(user_id, "@HG")) {
    throw runtime_error("user_id must end with \"@HG\"");
  }
  if (token.empty()) {
    throw runtime_error("token must not be empty");
  }
  if (token.size() > 8) {
    throw runtime_error("token must be at most 8 characters");
  }
  for (char ch : token) {
    if (!isdigit(ch)) {
      throw runtime_error("token must contain only decimal digits");
    }
  }
  if (unused.size() > 0xFF) {
    throw runtime_error("unused must be at most 255 characters");
  }

  // The encoded format is:
  //   parray<uint8_t, 4> mask_key; // xor this with all bytes starting with checksum
  //   le_uint16_t checksum; // crc16(&unused, EOF - &unused)
  //   uint8_t unused;
  //   uint8_t user_id_size;
  //   char user_id[user_id_size]; // Length must be in [4, 12] and must end with "@HG"
  //   uint8_t token_size;
  //   char token[token_size]; // Length must be in [1, 8] and must be all decimal digits
  //   uint8_t unused_size;
  //   char unused[unused_size]; // Ignored (possibly email address?)
  // We'll fill in mask_key and checksum after all the other fields.
  string data(7, '\0'); // mask_key, checksum, unused
  data.push_back(user_id.size());
  data += user_id;
  data.push_back(token.size());
  data += token;
  data.push_back(unused.size());
  data += unused;

  uint16_t checksum = crc16(data.data() + 6, data.size() - 6);
  uint32_t timestamp = time(nullptr);
  data[0] = (timestamp & 0xFF);
  data[1] = ((timestamp >> 8) & 0xFF);
  data[2] = ((timestamp >> 16) & 0xFF);
  data[3] = ((timestamp >> 24) & 0xFF);
  data[4] = checksum & 0xFF;
  data[5] = (checksum >> 8) & 0xFF;

  for (size_t z = 0; z < data.size() - 4; z++) {
    data[z + 4] ^= data[z % 3];
  }

  return data;
}
