#include "SaveFileFormats.hh"

#include <stdexcept>
#include <string>

#include "PSOProtocol.hh"

using namespace std;

// Originally there was going to be a language-based header, but then I decided
// against it. This string was already in use for that parser, so I didn't
// bother changing it.
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

bool PSOGCIFileHeader::checksum_correct() const {
  uint32_t cs = crc32(&this->game_name, this->game_name.bytes());
  cs = crc32(&this->embedded_seed, sizeof(this->embedded_seed), cs);
  cs = crc32(&this->file_name, this->file_name.bytes(), cs);
  cs = crc32(&this->banner, this->banner.bytes(), cs);
  cs = crc32(&this->icon, this->icon.bytes(), cs);
  cs = crc32(&this->data_size, sizeof(this->data_size), cs);
  cs = crc32("\0\0\0\0", 4, cs); // this->checksum (treated as zero)
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

bool PSOGCIFileHeader::is_trial() const {
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
  uint32_t crc = crc32("\0\0\0\0", 4);
  crc = crc32(&this->width, sizeof(*this) - sizeof(this->checksum), crc);
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

Image PSOGCSnapshotFile::decode_image() const {
  if (this->width != 256) {
    throw runtime_error("width is incorrect");
  }
  if (this->height != 192) {
    throw runtime_error("height is incorrect");
  }

  // 4x4 blocks of pixels
  Image ret(this->width, this->height, false);
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

void PSOBBGuildCardFile::Entry::clear() {
  this->data.clear();
  this->unknown_a1.clear(0);
}

uint32_t PSOBBGuildCardFile::checksum() const {
  return crc32(this, sizeof(*this));
}

PSOBBBaseSystemFile::PSOBBBaseSystemFile() {
  // This field is based on 1/1/2000, not 1/1/1970, so adjust appropriately
  this->base.creation_timestamp = (now() - 946684800000000ULL) / 1000000;
  for (size_t z = 0; z < PSOBBBaseSystemFile::DEFAULT_KEY_CONFIG.size(); z++) {
    this->key_config[z] = PSOBBBaseSystemFile::DEFAULT_KEY_CONFIG[z];
  }
  for (size_t z = 0; z < PSOBBBaseSystemFile::DEFAULT_JOYSTICK_CONFIG.size(); z++) {
    this->joystick_config[z] = PSOBBBaseSystemFile::DEFAULT_JOYSTICK_CONFIG[z];
  }
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
  ret->inventory.items[13].extension_data2 = 1;

  const auto& config = (ret->disp.visual.class_flags & 0x80) ? config_force : config_hunter_ranger;
  for (size_t z = 0; z < config.size(); z++) {
    ret->disp.config[z] = config[z];
  }

  ret->disp.stats.reset_to_base(ret->disp.visual.char_class, level_table);
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
  for (size_t z = 0; z < PSOBBCharacterFile::DEFAULT_SYMBOL_CHATS.size(); z++) {
    ret->symbol_chats[z] = PSOBBCharacterFile::DEFAULT_SYMBOL_CHATS[z].to_entry();
  }
  for (size_t z = 0; z < PSOBBCharacterFile::DEFAULT_TECH_MENU_CONFIG.size(); z++) {
    ret->tech_menu_config[z] = PSOBBCharacterFile::DEFAULT_TECH_MENU_CONFIG[z];
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

PSOBBCharacterFile::SymbolChatEntry PSOBBCharacterFile::DefaultSymbolChatEntry::to_entry() const {
  SymbolChatEntry ret;
  ret.present = 1;
  ret.name.encode(this->name, 1);
  ret.data.spec = this->spec;
  for (size_t z = 0; z < 4; z++) {
    ret.data.corner_objects[z] = this->corner_objects[z];
  }
  for (size_t z = 0; z < 12; z++) {
    ret.data.face_parts[z] = this->face_parts[z];
  }
  return ret;
}

// TODO: Eliminate duplication between this function and the parallel function
// in PlayerBank
void PSOBBCharacterFile::add_item(const ItemData& item) {
  uint32_t pid = item.primary_identifier();

  // Annoyingly, meseta is in the disp data, not in the inventory struct. If the
  // item is meseta, we have to modify disp instead.
  if (pid == MESETA_IDENTIFIER) {
    this->add_meseta(item.data2d);
    return;
  }

  // Handle combinable items
  size_t combine_max = item.max_stack_size();
  if (combine_max > 1) {
    // Get the item index if there's already a stack of the same item in the
    // player's inventory
    size_t y;
    for (y = 0; y < this->inventory.num_items; y++) {
      if (this->inventory.items[y].data.primary_identifier() == item.primary_identifier()) {
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
// function in PlayerBank
ItemData PSOBBCharacterFile::remove_item(uint32_t item_id, uint32_t amount, bool allow_meseta_overdraft) {
  ItemData ret;

  // If we're removing meseta (signaled by an invalid item ID), then create a
  // meseta item.
  if (item_id == 0xFFFFFFFF) {
    this->remove_meseta(amount, allow_meseta_overdraft);
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
  if (amount && (inventory_item.data.stack_size() > 1) &&
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
    this->inventory.items[x] = this->inventory.items[x + 1];
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

void PSOBBCharacterFile::print_inventory(FILE* stream, Version version, shared_ptr<const ItemNameIndex> name_index) const {
  fprintf(stream, "[PlayerInventory] Meseta: %" PRIu32 "\n", this->disp.stats.meseta.load());
  fprintf(stream, "[PlayerInventory] %hhu items\n", this->inventory.num_items);
  for (size_t x = 0; x < this->inventory.num_items; x++) {
    const auto& item = this->inventory.items[x];
    auto name = name_index->describe_item(version, item.data);
    auto hex = item.data.hex();
    fprintf(stream, "[PlayerInventory]   %2zu: [+%08" PRIX32 "] %s (%s)\n", x, item.flags.load(), hex.c_str(), name.c_str());
  }
}

void PSOBBCharacterFile::print_bank(FILE* stream, Version version, shared_ptr<const ItemNameIndex> name_index) const {
  fprintf(stream, "[PlayerBank] Meseta: %" PRIu32 "\n", this->bank.meseta.load());
  fprintf(stream, "[PlayerBank] %" PRIu32 " items\n", this->bank.num_items.load());
  for (size_t x = 0; x < this->bank.num_items; x++) {
    const auto& item = this->bank.items[x];
    const char* present_token = item.present ? "" : " (missing present flag)";
    auto name = name_index->describe_item(version, item.data);
    auto hex = item.data.hex();
    fprintf(stream, "[PlayerBank]   %3zu: %s (%s) (x%hu) %s\n", x, hex.c_str(), name.c_str(), item.amount.load(), present_token);
  }
}

const array<PSOBBCharacterFile::DefaultSymbolChatEntry, 6> PSOBBCharacterFile::DEFAULT_SYMBOL_CHATS = {
    DefaultSymbolChatEntry{"\tEHello", 0x28, {0xFFFF, 0x000D, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x05, 0x18, 0x1D, 0x00}, {0x05, 0x28, 0x1D, 0x01}, {0x36, 0x20, 0x2A, 0x00}, {0x3C, 0x00, 0x32, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{"\tEGood-bye", 0x74, {0x0476, 0x000C, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x06, 0x15, 0x14, 0x00}, {0x06, 0x2B, 0x14, 0x01}, {0x05, 0x18, 0x1F, 0x00}, {0x05, 0x28, 0x1F, 0x01}, {0x36, 0x20, 0x2A, 0x00}, {0x3C, 0x00, 0x32, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{"\tEHurrah!", 0x28, {0x0362, 0x0362, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x09, 0x16, 0x1B, 0x00}, {0x09, 0x2B, 0x1B, 0x01}, {0x37, 0x20, 0x2C, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{"\tECrying", 0x74, {0x074F, 0xFFFF, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x06, 0x15, 0x14, 0x00}, {0x06, 0x2B, 0x14, 0x01}, {0x05, 0x18, 0x1F, 0x00}, {0x05, 0x28, 0x1F, 0x01}, {0x21, 0x20, 0x2E, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{"\tEI\'m angry!", 0x5C, {0x0116, 0x0001, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x0B, 0x18, 0x1B, 0x01}, {0x0B, 0x28, 0x1B, 0x00}, {0x33, 0x20, 0x2A, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
    DefaultSymbolChatEntry{"\tEHelp me!", 0xEC, {0x065E, 0x0138, 0xFFFF, 0xFFFF}, {SymbolChat::FacePart{0x02, 0x17, 0x1B, 0x01}, {0x02, 0x2A, 0x1B, 0x00}, {0x31, 0x20, 0x2C, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x00}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}, {0xFF, 0x00, 0x00, 0x02}}},
};

const array<uint16_t, 20> PSOBBCharacterFile::DEFAULT_TECH_MENU_CONFIG = {
    0x0000, 0x0006, 0x0003, 0x0001, 0x0007, 0x0004, 0x0002, 0x0008, 0x0005, 0x0009,
    0x0012, 0x000F, 0x0010, 0x0011, 0x000D, 0x000A, 0x000B, 0x000C, 0x000E, 0x0000};

const array<uint8_t, 0x016C> PSOBBBaseSystemFile::DEFAULT_KEY_CONFIG = {
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

const array<uint8_t, 0x0038> PSOBBBaseSystemFile::DEFAULT_JOYSTICK_CONFIG = {
    0x00, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
