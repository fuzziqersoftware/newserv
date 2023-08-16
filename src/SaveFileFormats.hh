#pragma once

#include <stdint.h>

#include <algorithm>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Image.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <string>

#include "PSOEncryption.hh"
#include "Player.hh"
#include "Text.hh"

struct ShuffleTables {
  uint8_t forward_table[0x100];
  uint8_t reverse_table[0x100];

  ShuffleTables(PSOV2Encryption& crypt);

  static uint32_t pseudorand(PSOV2Encryption& crypt, uint32_t prev);

  void shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const;
};

struct PSOGCIFileHeader {
  // Every PSOGC save file begins with a PSOGCIFileHeader. The first 0x40 bytes
  // of this structure are the .gci file header; the remaining bytes after that
  // are the actual data from the memory card. For save files (system /
  // character / Guild Card), one of the structures below immediately follows
  // the PSOGCIFileHeader. The system file is not encrypted, but the character
  // and Guild Card files are encrypted using a seed stored in the system file.
  /* 0000 */ parray<char, 4> game_id; // 'GPOE', 'GPSP', etc.
  /* 0004 */ parray<char, 2> developer_id; // '8P' for Sega
  // There is a structure for this part of the header, but we don't use it.
  /* 0006 */ uint8_t unused;
  /* 0007 */ uint8_t image_flags;
  /* 0008 */ ptext<char, 0x20> internal_file_name;
  /* 0028 */ be_uint32_t modification_time;
  /* 002C */ be_uint32_t image_data_offset;
  /* 0030 */ be_uint16_t icon_formats;
  /* 0032 */ be_uint16_t icon_animation_speeds;
  /* 0034 */ uint8_t permission_flags;
  /* 0035 */ uint8_t copy_count;
  /* 0036 */ be_uint16_t first_block_index;
  /* 0038 */ be_uint16_t num_blocks;
  /* 003A */ parray<uint8_t, 2> unused2;
  /* 003C */ be_uint32_t comment_offset;
  // GCI header ends here (and memcard file data begins here)
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
  /* 0004 */ be_int16_t music_volume; // 0 = full volume; -250 = min volume
  /* 0006 */ int8_t sound_volume; // 0 = full volume; -100 = min volume
  /* 0007 */ uint8_t language;
  /* 0008 */ be_uint32_t unknown_a3; // Default 1728000 (== 60 * 60 * 24 * 20)
  /* 000C */ be_uint16_t udp_behavior; // 0 = auto, 1 = on, 2 = off
  /* 000E */ be_uint16_t surround_sound_enabled;
  /* 0010 */ parray<uint8_t, 0x100> event_flags; // Can be set by quest opcode D8 or E8
  /* 0110 */ parray<uint8_t, 8> unknown_a7;
  // This timestamp is the number of seconds since 12:00AM on 1 January 2000.
  // This field is also used as the round1 seed for encrypting the character and
  // Guild Card files.
  /* 0118 */ be_uint32_t creation_timestamp;
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
    // This structure is internally split into two by the game. The offsets here
    // are relative to the start of this structure (first column), and relative
    // to the start of the second internal structure (second column).
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ be_uint32_t unknown_a1;
    /* 0420:0004 */ be_uint32_t creation_timestamp;
    // The signature field holds the value 0xA205B064, which is 2718281828 in
    // decimal - approximately e * 10^9. It's unknown why Sega chose this value.
    /* 0424:0008 */ be_uint32_t signature;
    /* 0428:000C */ be_uint32_t play_time_seconds;
    // This field is a collection of several flags and small values. The known
    // fields are:
    //   ------zA BCDEFG-- HHHIIIJJ KLMNOPQR
    //   z = Function key setting (BB; 0 = menu shortcuts; 1 = chat shortcuts).
    //       This bit is unused by PSO GC.
    //   A = Keyboard controls (BB; 0 = on; 1 = off). Note that A is also used
    //       by PSO GC, but its function is currently unknown.
    //   G = Choice search setting (0 = enabled; 1 = disabled)
    //   H = Player lobby labels (0 = name; 1 = name, language, and level;
    //       2 = W/D counts; 3 = challenge rank; 4 = nothing)
    //   I = Idle disconnect time (0 = 15 mins; 1 = 30 mins; 2 = 45 mins;
    //       3 = 60 mins; 4 or 5: immediately; 6: 16 seconds; 7: 32 seconds).
    //       Obviously the behaviors for 4-7 are unintended; this is the result
    //       of a missing bounds check.
    //   J = Message speed (0 = slow; 1 = normal; 2 = fast; 3 = very fast)
    //   P = Cursor position (0 = saved; 1 = non-saved)
    //   Q = Button config (0 = normal; 1 = L/R reversed)
    //   R = Map direction (0 = non-fixed; 1 = fixed)
    /* 042C:0010 */ be_uint32_t option_flags;
    /* 0430:0014 */ be_uint32_t save_count;
    /* 0434:0018 */ parray<uint8_t, 0x230> unknown_a4;
    /* 0664:0248 */ PlayerBank bank;
    /* 192C:1510 */ GuildCardV3 guild_card;
    /* 19BC:15A0 */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 1DDC:19C0 */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 246C:2050 */ ptext<char, 0xAC> auto_reply;
    /* 2518:20FC */ ptext<char, 0xAC> info_board;
    /* 25C4:21A8 */ PlayerRecords_Battle<true> battle_records;
    /* 25DC:21C0 */ parray<uint8_t, 4> unknown_a2;
    /* 25E0:21C4 */ PlayerRecordsV3_Challenge<true> challenge_records;
    /* 26E0:22C4 */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
    /* 2708:22EC */ parray<uint8_t, 0x90> unknown_a6;
    /* 2798:237C */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 1152C */ ptext<char, 0x10> serial_number; // As %08X (not decimal)
  /* 1153C */ ptext<char, 0x10> access_key;
  /* 1154C */ ptext<char, 0x10> password;
  /* 1155C */ be_uint64_t bgm_test_songs_unlocked;
  /* 11564 */ be_uint32_t save_count;
  /* 11568 */ be_uint32_t round2_seed;
  /* 1156C */
} __attribute__((packed));

struct PSOGCEp3CharacterFile {
  /* 00000 */ be_uint32_t checksum; // crc32 of this field (as 0) through end of struct
  struct Character {
    // This structure is internally split into two by the game. The offsets here
    // are relative to the start of this structure (first column), and relative
    // to the start of the second internal structure (second column).
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ be_uint32_t unknown_a1;
    /* 0420:0004 */ be_uint32_t creation_timestamp;
    /* 0424:0008 */ be_uint32_t signature; // Same value as for Episodes 1&2 (above)
    /* 0428:000C */ be_uint32_t play_time_seconds;
    // See the comment in PSOGCCharacterFile::Character about what the bits in
    // this field mean.
    /* 042C:0010 */ be_uint32_t option_flags;
    /* 0430:0014 */ be_uint32_t save_count;
    /* 0434:0018 */ parray<uint8_t, 0x1C> unknown_a2;
    /* 0450:0034 */ parray<uint8_t, 0x10> unknown_a3;
    // seq_vars is an array of 1024 bits, which contain all the Episode 3 quest
    // progress flags. This includes things like which maps are unlocked, which
    // NPC decks are unlocked, and whether the player has a VIP card or not.
    /* 0460:0044 */ parray<uint8_t, 0x400> seq_vars;
    /* 0860:0444 */ be_uint32_t unknown_a4;
    /* 0864:0448 */ be_uint32_t unknown_a5;
    /* 0868:044C */ be_uint32_t unknown_a6;
    /* 086C:0450 */ parray<uint8_t, 0x60> unknown_a7;
    /* 08CC:04B0 */ GuildCardV3 guild_card;
    /* 095C:0540 */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 0D7C:0960 */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 140C:0FF0 */ ptext<char, 0xAC> auto_reply;
    /* 14B8:109C */ ptext<char, 0xAC> info_board;
    /* 1564:1148 */ be_uint16_t win_count;
    /* 1566:114A */ be_uint16_t lose_count;
    /* 1568:114C */ parray<be_uint16_t, 5> unknown_a8;
    /* 1572:1156 */ parray<uint8_t, 2> unused;
    /* 1574:1158 */ parray<be_uint32_t, 2> unknown_a9;
    /* 157C:1160 */ parray<uint8_t, 0xDC> unknown_a10;
    /* 1658:123C */ Episode3::PlayerConfig ep3_config;
    /* 39A8:358C */ be_uint32_t unknown_a11;
    /* 39AC:3590 */ be_uint32_t unknown_a12;
    /* 39B0:3594 */ be_uint32_t unknown_a13;
    /* 39B4:3598 */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 193F0 */ ptext<char, 0x10> serial_number; // As %08X (not decimal)
  /* 19400 */ ptext<char, 0x10> access_key;
  /* 19410 */ ptext<char, 0x10> password;
  // In Episode 3, this field still exists, but is unused since BGM test was
  // removed from the options menu in favor of the jukebox. The jukebox is
  // accessible online only, and which songs are available there is controlled
  // by the B7 command sent by the server instead.
  /* 19420 */ be_uint64_t bgm_test_songs_unlocked;
  /* 19428 */ be_uint32_t save_count;
  // This is an array of 1000 bits, represented here as 128 bytes, the last few
  // of which are unused. Each bit corresponds to a card ID with the bit's
  // index; if the bit is set, then the card's rarity is replaced with D2 if its
  // original rarity is S, SS, E, or D2, or with D1 if the original rarity is
  // any other value. Upon receiving a B8 command (new card definitions), the
  // game updates this array of bits based on which cards in the received update
  // have D1 or D2 rarities. This could have been used by Sega to persist part
  // of the online updates into offline play, but there's no indication that
  // they ever used this functionality.
  /* 1942C */ parray<uint8_t, 0x80> card_rarity_override_flags;
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
  /* E284 */ be_uint32_t creation_timestamp;
  /* E288 */ be_uint32_t round2_seed;
  /* E28C */
} __attribute__((packed));

struct PSOGCSnapshotFile {
  /* 00000 */ be_uint32_t checksum;
  /* 00004 */ be_uint16_t width;
  /* 00006 */ be_uint16_t height;
  /* 00008 */ parray<be_uint16_t, 0xC000> pixels;
  /* 18008 */ uint8_t unknown_a1; // Always 0x18?
  /* 18009 */ uint8_t unknown_a2;
  /* 1800A */ be_int16_t max_players;
  /* 1800C */ parray<be_uint32_t, 12> players_present;
  /* 1803C */ parray<be_uint32_t, 12> player_levels;
  /* 1806C */ parray<ptext<char, 0x18>, 12> player_names;
  /* 1818C */

  bool checksum_correct() const;
  Image decode_image() const;
} __attribute__((packed));

template <bool IsBigEndian>
std::string decrypt_gci_or_vms_v2_data_section(
    const void* data_section, size_t size, uint32_t round1_seed, size_t max_decrypt_bytes = 0) {
  if (max_decrypt_bytes == 0) {
    max_decrypt_bytes = size;
  } else {
    max_decrypt_bytes = std::min<size_t>(max_decrypt_bytes, size);
  }

  std::string decrypted(max_decrypt_bytes, '\0');
  PSOV2Encryption shuf_crypt(round1_seed);
  ShuffleTables shuf(shuf_crypt);
  shuf.shuffle(decrypted.data(), data_section, max_decrypt_bytes, true);

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
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    bool skip_checksum = false,
    uint64_t override_round2_seed = 0xFFFFFFFFFFFFFFFF) {
  std::string decrypted = decrypt_gci_or_vms_v2_data_section<true>(
      data_section, size, round1_seed);

  if (decrypted.size() < sizeof(StructT)) {
    throw std::runtime_error("file too small for structure");
  }
  StructT ret = *reinterpret_cast<const StructT*>(decrypted.data());

  PSOV2Encryption round2_crypt(override_round2_seed < 0x100000000 ? override_round2_seed : ret.round2_seed.load());
  round2_crypt.encrypt_big_endian(&ret, offsetof(StructT, round2_seed));

  if (!skip_checksum) {
    uint32_t expected_crc = ret.checksum;
    ret.checksum = 0;
    uint32_t actual_crc = crc32(&ret, sizeof(ret));
    ret.checksum = expected_crc;
    if (expected_crc != actual_crc) {
      throw std::runtime_error(string_printf(
          "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
          expected_crc, actual_crc));
    }
  }

  return ret;
}

std::string decrypt_gci_fixed_size_file_data_section_for_salvage(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    uint64_t round2_seed,
    size_t max_decrypt_bytes);

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

uint32_t compute_psogc_timestamp(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second);
