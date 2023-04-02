#pragma once

#include <stdint.h>

#include <string>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Strings.hh>
#include <phosg/Random.hh>

#include "PSOEncryption.hh"
#include "Text.hh"
#include "Player.hh"



struct ShuffleTables {
  uint8_t forward_table[0x100];
  uint8_t reverse_table[0x100];

  ShuffleTables(PSOV2Encryption& crypt);

  static uint32_t pseudorand(PSOV2Encryption& crypt, uint32_t prev);

  void shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const;
};



// Every PSOGC save file begins with a PSOGCIFileHeader. The first 0x40 bytes of
// this are the .gci file header; the remaining bytes of the file are the actual
// data from the memory card. For save files (system / character / Guild Card),
// one of the structures below immediately follows the PSOGCIFileHeader. The
// system file is not encrypted, but the character and Guild Card files are
// encrypted using a seed stored in the system file.

struct PSOGCIFileHeader {
  /* 0000 */ parray<char, 4> game_id; // 'GPOE', 'GPSP', etc.
  /* 0004 */ parray<char, 2> developer_id; // '8P' for Sega
  // There is a structure for this part of the header, but we don't use it
  /* 0006 */ parray<uint8_t, 0x3A> remaining_gci_header;
  // game_name is e.g. "PSO EPISODE I & II" or "PSO EPISODE III"
  /* 0040 */ ptext<char, 0x1C> game_name;
  /* 005C */ be_uint32_t embedded_seed; // Used in some of Ralf's quest packs
  /* 0060 */ ptext<char, 0x20> file_name;
  /* 0080 */ parray<uint8_t, 0x1800> banner;
  /* 1880 */ parray<uint8_t, 0x800> icon;
  // data_size specifies the number of bytes remaining in the file. In all cases
  // except for the system file, this data is encrypted.
  /* 2080 */ be_uint32_t data_size;
  // To compute checksum, set checksum to zero, then compute the CRC32 of all
  // fields in this struct starting with gci_header.game_name. (Yes, including
  // the checksum field, which is temporarily zero.) See checksum_correct below.
  /* 2084 */ be_uint32_t checksum;
  /* 2088 */

  bool checksum_correct() const;
  void check() const;

  bool is_ep12() const;
  bool is_ep3() const;
} __attribute__((packed));

struct PSOGCSystemFile {
  /* 0000 */ be_uint32_t checksum;
  /* 0004 */ be_uint16_t unknown_a1;
  /* 0006 */ uint8_t unknown_a2;
  /* 0007 */ uint8_t language;
  /* 0008 */ be_uint32_t unknown_a3;
  /* 000C */ be_uint16_t unknown_a4;
  /* 000E */ be_uint16_t unknown_a5;
  /* 0010 */ parray<uint8_t, 0x100> unknown_a6;
  /* 0110 */ parray<uint8_t, 8> unknown_a7;
  /* 0118 */ be_uint32_t creation_internet_time; // Character file round1 seed
  /* 011C */
} __attribute__((packed));

struct PSOGCEp3SystemFile {
  /* 0000 */ PSOGCSystemFile base;
  /* 011C */ int8_t unknown_a1;
  /* 011D */ parray<uint8_t, 11> unknown_a2;
  /* 0128 */ be_uint32_t unknown_a3;
  /* 012C */
} __attribute__((packed));

struct PSOGCSaveFileSymbolChatEntry {
  /* 00 */ be_uint32_t present;
  /* 04 */ ptext<char, 0x18> name;
  /* 1C */ be_uint16_t unused;
  /* 1E */ uint8_t flags;
  /* 1F */ uint8_t face_spec;
  struct CornerObject {
    uint8_t type;
    uint8_t flags_color;
  } __attribute__((packed));
  /* 20 */ parray<CornerObject, 4> corner_objects;
  struct FacePart {
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
  } __attribute__((packed));
  /* 28 */ parray<FacePart, 12> face_parts;
  /* 58 */
} __attribute__((packed));

struct PSOGCSaveFileChatShortcutEntry {
  /* 00 */ be_uint32_t present_type;
  /* 04 */ parray<uint8_t, 0x50> definition;
  /* 54 */
} __attribute__((packed));

struct PSOGCCharacterFile {
  /* 00000 */ be_uint32_t checksum;
  struct Character {
    /* 0000 */ PlayerInventory inventory;
    /* 034C */ PlayerDispDataDCPCV3 disp;
    /* 041C */ be_uint32_t unknown_a1;
    /* 0420 */ be_uint32_t save_token; // Sent in 96 command
    /* 0424 */ parray<be_uint32_t, 3> unknown_a2;
    /* 0430 */ be_uint32_t save_count; // Sent in 96 command
    /* 0434 */ parray<uint8_t, 0x230> unknown_a3;
    /* 0664 */ PlayerBank bank;
    /* 192C */ GuildCardV3 guild_card;
    /* 19BC */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 1DDC */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 246C */ ptext<char, 0xAC> auto_reply;
    /* 2518 */ ptext<char, 0xAC> info_board;
    /* 25C4 */ parray<uint8_t, 0x11C> unknown_a4;
    /* 26E0 */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
    /* 2708 */ parray<uint8_t, 0x90> unknown_a5;
    /* 2798 */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 1152C */ ptext<char, 0x10> serial_number; // As %08X (not decimal)
  /* 1153C */ ptext<char, 0x10> access_key;
  /* 1154C */ ptext<char, 0x10> password;
  /* 1155C */ be_uint32_t unknown_a1;
  /* 11560 */ be_uint32_t unknown_a2;
  /* 11564 */ be_uint32_t unknown_a3;
  /* 11568 */ be_uint32_t round2_seed;
  /* 1156C */
} __attribute__((packed));

struct PSOGCEp3CharacterFile {
  /* 00000 */ be_uint32_t checksum; // crc32 of this field (as 0) through end of struct
  struct Character {
    /* 0000 */ PlayerInventory inventory;
    /* 034C */ PlayerDispDataDCPCV3 disp;
    /* 041C */ be_uint32_t unknown_a1;
    /* 0420 */ be_uint32_t save_token; // Sent in 96 command
    /* 0424 */ parray<be_uint32_t, 3> unknown_a2;
    /* 0430 */ be_uint32_t save_count; // Sent in 96 command
    /* 0434 */ parray<uint8_t, 0x498> unknown_a3;
    /* 08CC */ GuildCardV3 guild_card;
    /* 095C */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 0D7C */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 140C */ parray<uint8_t, 0xAC> unknown_a4;
    /* 14B8 */ ptext<char, 0xAC> info_board;
    /* 1564 */ parray<uint8_t, 0xF4> unknown_a5;
    /* 1658 */ Episode3::PlayerConfig ep3_config;
    /* 39A8 */ be_uint32_t unknown_a7;
    /* 39AC */ be_uint32_t unknown_a8;
    /* 39B0 */ be_uint32_t unknown_a9;
    /* 39B4 */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 193F0 */ ptext<char, 0x10> serial_number; // As %08X (not decimal)
  /* 19400 */ ptext<char, 0x10> access_key;
  /* 19410 */ ptext<char, 0x10> password;
  /* 19420 */ be_uint32_t unknown_a1;
  /* 19424 */ be_uint32_t unknown_a2;
  /* 19428 */ be_uint32_t unknown_a3;
  /* 1942C */ parray<be_uint32_t, 0x20> unknown_a4;
  /* 194AC */ be_uint32_t round2_seed;
  /* 194B0 */
} __attribute__((packed));

struct PSOGCGuildCardFile {
  /* 0000 */ be_uint32_t checksum;
  /* 0004 */ parray<uint8_t, 0xC0> unknown_a1;
  struct GuildCardBE {
    // Note: This struct (up through offset 0x90) is identical to GuildCardV3
    // except for 32-bit fields, which are big-endian here.
    /* 0000 */ be_uint32_t player_tag; // == 0x00000001 (not 0x00010000)
    /* 0004 */ be_uint32_t guild_card_number;
    /* 0008 */ ptext<char, 0x18> name;
    /* 0020 */ ptext<char, 0x6C> description;
    /* 008C */ uint8_t present;
    /* 008D */ uint8_t language;
    /* 008E */ uint8_t section_id;
    /* 008F */ uint8_t char_class;
    /* 0090 */
  } __attribute__((packed));
  struct GuildCardEntry {
    /* 0000 */ GuildCardBE base;
    /* 0090 */ uint8_t unknown_a1;
    /* 0091 */ uint8_t unknown_a2;
    /* 0092 */ uint8_t unknown_a3;
    /* 0093 */ uint8_t unknown_a4;
    /* 0094 */ ptext<char, 0x6C> comment;
    /* 0100 */
  } __attribute__((packed));
  /* 00C4 */ parray<GuildCardEntry, 0xD2> entries;
  /* D2C4 */ parray<GuildCardBE, 0x1C> blocked_senders;
  /* E284 */ be_uint32_t unknown_a3;
  /* E288 */ be_uint32_t round2_seed;
  /* E28C */
} __attribute__((packed));



template <bool IsBigEndian>
std::string decrypt_gci_or_vms_v2_data_section(
    const void* data_section, size_t size, uint32_t round1_seed) {

  std::string decrypted(size, '\0');
  PSOV2Encryption shuf_crypt(round1_seed);
  ShuffleTables shuf(shuf_crypt);
  shuf.shuffle(decrypted.data(), data_section, size, true);

  size_t orig_size = decrypted.size();
  decrypted.resize((decrypted.size() + 3) & (~3));

  PSOV2Encryption round1_crypt(round1_seed);
  round1_crypt.encrypt_minus_t<IsBigEndian>(decrypted.data(), decrypted.size());

  decrypted.resize(orig_size);
  return decrypted;
}

template <bool IsBigEndian>
std::string encrypt_gci_or_vms_v2_data_section(
    const void* data_section, size_t size, uint32_t round1_seed) {
  std::string encrypted(reinterpret_cast<const char*>(data_section), size);
  encrypted.resize((encrypted.size() + 3) & (~3));

  PSOV2Encryption crypt(round1_seed);
  crypt.encrypt_minus_t<IsBigEndian>(encrypted.data(), encrypted.size());

  std::string ret(size, '\0');
  PSOV2Encryption shuf_crypt(round1_seed);
  ShuffleTables shuf(shuf_crypt);
  shuf.shuffle(ret.data(), encrypted.data(), size, false);

  return ret;
}

template <typename StructT>
StructT decrypt_gci_fixed_size_file_data_section(
    const void* data_section, size_t size, uint32_t round1_seed) {
  std::string decrypted = decrypt_gci_or_vms_v2_data_section<true>(
      data_section, size, round1_seed);

  if (decrypted.size() < sizeof(StructT)) {
    throw std::runtime_error("file too small for structure");
  }
  StructT ret = *reinterpret_cast<const StructT*>(decrypted.data());

  PSOV2Encryption round2_crypt(ret.round2_seed);
  round2_crypt.encrypt_big_endian(&ret, offsetof(StructT, round2_seed));

  uint32_t expected_crc = ret.checksum;
  ret.checksum = 0;
  uint32_t actual_crc = crc32(&ret, sizeof(ret));
  ret.checksum = expected_crc;
  if (expected_crc != actual_crc) {
    throw std::runtime_error(string_printf(
        "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
        expected_crc, actual_crc));
  }

  return ret;
}

template <typename StructT>
std::string encrypt_gci_fixed_size_file_data_section(
    const StructT& s, uint32_t round1_seed) {
  StructT encrypted = s;
  encrypted.checksum = 0;
  encrypted.round2_seed = random_object<uint32_t>();
  encrypted.checksum = crc32(&encrypted, sizeof(encrypted));

  PSOV2Encryption round2_crypt(encrypted.round2_seed);
  round2_crypt.encrypt_big_endian(&encrypted, offsetof(StructT, round2_seed));

  return encrypt_gci_or_vms_v2_data_section<true>(
      &encrypted, sizeof(StructT), round1_seed);
}
