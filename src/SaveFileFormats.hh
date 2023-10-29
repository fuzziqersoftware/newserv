#pragma once

#include <stdint.h>

#include <algorithm>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Image.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <string>

#include "Episode3/DataIndexes.hh"
#include "PSOEncryption.hh"
#include "PlayerSubordinates.hh"
#include "Text.hh"

struct ShuffleTables {
  uint8_t forward_table[0x100];
  uint8_t reverse_table[0x100];

  ShuffleTables(PSOV2Encryption& crypt);

  static uint32_t pseudorand(PSOV2Encryption& crypt, uint32_t prev);

  void shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const;
};

struct PSOVMSFileHeader {
  /* 0000 */ pstring<TextEncoding::SJIS, 0x10> short_desc;
  /* 0010 */ pstring<TextEncoding::SJIS, 0x20> long_desc;
  /* 0030 */ pstring<TextEncoding::SJIS, 0x10> creator_id;
  /* 0040 */ le_uint16_t num_icons;
  /* 0042 */ le_uint16_t animation_speed;
  /* 0044 */ le_uint16_t eyecatch_type;
  /* 0046 */ le_uint16_t crc;
  /* 0048 */ le_uint32_t data_size; // Not including header and icons
  /* 004C */ parray<uint8_t, 0x14> unused;
  /* 0060 */ parray<le_uint16_t, 0x10> icon_palette;
  // Variable-length field:
  /* 0080 */ // parray<uint8_t, num_icons> icon;

  bool checksum_correct() const;
} __attribute__((packed));

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
  /* 0008 */ pstring<TextEncoding::SJIS, 0x20> internal_file_name;
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
  /* 0040 */ pstring<TextEncoding::SJIS, 0x1C> game_name;
  /* 005C */ be_uint32_t embedded_seed; // Used in some of Ralf's quest packs
  /* 0060 */ pstring<TextEncoding::SJIS, 0x20> file_name;
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
  bool is_trial() const;
} __attribute__((packed));

struct PSOGCSystemFile {
  /* 0000 */ be_uint32_t checksum;
  /* 0004 */ be_int16_t music_volume; // 0 = full volume; -250 = min volume
  /* 0006 */ int8_t sound_volume; // 0 = full volume; -100 = min volume
  /* 0007 */ uint8_t language;
  // This field stores the effective time zone offset between the server and
  // client, in frames. The default value is 1728000, which corresponds to 16
  // hours. This is recomputed when the client receives a B1 command.
  /* 0008 */ be_uint32_t server_time_delta_frames;
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

struct PSOBBSystemFileBase {
  /* 0000 */ be_uint32_t checksum = 0;
  /* 0004 */ be_int16_t music_volume = -50;
  /* 0006 */ int8_t sound_volume = 0;
  /* 0007 */ uint8_t language = 0;
  /* 0008 */ be_uint32_t server_time_delta_frames = 1728000;
  /* 000C */ be_uint16_t udp_behavior = 0; // 0 = auto, 1 = on, 2 = off
  /* 000E */ be_uint16_t surround_sound_enabled = 0;
  /* 0010 */ parray<uint8_t, 0x0100> event_flags;
  /* 0110 */ le_uint32_t creation_timestamp = 0;
  /* 0114 */
} __attribute__((packed));

struct PSOBBSystemFile {
  /* 0000 */ PSOBBSystemFileBase base;
  /* 0114 */ parray<uint8_t, 0x016C> key_config;
  /* 0280 */ parray<uint8_t, 0x0038> joystick_config;
  /* 02B8 */ le_uint32_t guild_card_number = 0;
  /* 02BC */ le_uint32_t team_id = 0;
  /* 02C0 */ le_uint64_t team_info = 0;
  /* 02C8 */ le_uint16_t team_privilege_level = 0;
  /* 02CA */ le_uint16_t reserved = 0;
  /* 02CC */ pstring<TextEncoding::UTF16, 0x0010> team_name;
  /* 02EC */ parray<uint8_t, 0x0800> team_flag;
  /* 0AEC */ le_uint32_t team_rewards = 0;
  /* 0AF0 */
} __attribute__((packed));

struct PSOBBGuildCardFile {
  struct Entry {
    /* 0000 */ GuildCardBB data;
    /* 0108 */ pstring<TextEncoding::UTF16, 0x58> comment;
    /* 01B8 */ parray<uint8_t, 0x4> unknown_a1;
    /* 01BC */

    void clear();
  } __attribute__((packed));

  PSOBBSystemFileBase system_file;
  parray<GuildCardBB, 0x1C> blocked;
  parray<uint8_t, 0x180> unknown_a2;
  parray<Entry, 0x69> entries;

  uint32_t checksum() const;
} __attribute__((packed));

struct PSOGCSaveFileSymbolChatEntry {
  /* 00 */ be_uint32_t present;
  /* 04 */ pstring<TextEncoding::SJIS, 0x18> name;
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

struct PSOPCSaveFileSymbolChatEntry {
  /* 00 */ le_uint32_t present;
  /* 04 */ pstring<TextEncoding::UTF16, 0x18> name;
  /* 34 */ uint8_t face_spec;
  /* 35 */ uint8_t flags;
  /* 36 */ be_uint16_t unused;
  struct CornerObject {
    uint8_t type;
    uint8_t flags_color;
  } __attribute__((packed));
  /* 38 */ parray<CornerObject, 4> corner_objects;
  struct FacePart {
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
  } __attribute__((packed));
  /* 40 */ parray<FacePart, 12> face_parts;
  /* 70 */
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
    // Known bits in the flags field:
    //   00000001: Character was not saved after disconnecting (and the message
    //     about items being deleted is shown in the select menu)
    //   00000002: Used for something, but it's not known what it does
    /* 041C:0000 */ be_uint32_t flags;
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
    /* 0434:0018 */ parray<uint8_t, 0x1C> unknown_a4;
    /* 0450:0034 */ parray<uint8_t, 0x10> unknown_a5;
    // 1024 bits (flags) per difficulty
    /* 0460:0044 */ parray<parray<uint8_t, 0x80>, 4> quest_flags;
    /* 0660:0244 */ be_uint32_t death_count;
    /* 0664:0248 */ PlayerBank bank;
    /* 192C:1510 */ GuildCardV3 guild_card;
    /* 19BC:15A0 */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 1DDC:19C0 */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 246C:2050 */ pstring<TextEncoding::SJIS, 0xAC> auto_reply;
    /* 2518:20FC */ pstring<TextEncoding::SJIS, 0xAC> info_board;
    /* 25C4:21A8 */ PlayerRecords_Battle<true> battle_records;
    /* 25DC:21C0 */ parray<uint8_t, 4> unknown_a2;
    /* 25E0:21C4 */ PlayerRecordsV3_Challenge<true> challenge_records;
    /* 26E0:22C4 */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
    /* 2708:22EC */ parray<uint8_t, 0x28> unknown_a6;
    /* 2730:2314 */ parray<be_uint32_t, 0x10> quest_global_flags;
    /* 2770:2354 */ PlayerRecords_Battle<true> offline_battle_records;
    /* 2788:236C */ parray<uint8_t, 4> unknown_f5;
    /* 278C:2370 */ be_uint32_t unknown_f6;
    /* 2790:2374 */ be_uint32_t unknown_f7;
    /* 2794:2378 */ be_uint32_t unknown_f8;
    /* 2798:237C */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 1152C */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
  /* 1153C */ pstring<TextEncoding::ASCII, 0x10> access_key;
  /* 1154C */ pstring<TextEncoding::ASCII, 0x10> password;
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
    /* 041C:0000 */ be_uint32_t flags;
    /* 0420:0004 */ be_uint32_t creation_timestamp;
    /* 0424:0008 */ be_uint32_t signature; // Same value as for Episodes 1&2 (above)
    /* 0428:000C */ be_uint32_t play_time_seconds;
    // See the comment in PSOGCCharacterFile::Character about what the bits in
    // this field mean.
    /* 042C:0010 */ be_uint32_t option_flags;
    /* 0430:0014 */ be_uint32_t save_count;
    /* 0434:0018 */ parray<uint8_t, 0x1C> unknown_a2;
    /* 0450:0034 */ parray<uint8_t, 0x10> unknown_a3;
    // seq_vars is an array of 8192 bits, which contain all the Episode 3 quest
    // progress flags. This includes things like which maps are unlocked, which
    // NPC decks are unlocked, and whether the player has a VIP card or not.
    /* 0460:0044 */ parray<uint8_t, 0x400> seq_vars;
    /* 0860:0444 */ be_uint32_t death_count;
    // The following three fields appear to actually be a PlayerBank structure
    // with only 4 item slots instead of 200. They presumably didn't completely
    // remove the bank in Ep3 because they would have to change too much code.
    /* 0864:0448 */ be_uint32_t num_bank_items;
    /* 0868:044C */ be_uint32_t bank_meseta;
    /* 086C:0450 */ parray<PlayerBankItem, 4> bank_items;
    /* 08CC:04B0 */ GuildCardV3 guild_card;
    /* 095C:0540 */ parray<PSOGCSaveFileSymbolChatEntry, 12> symbol_chats;
    /* 0D7C:0960 */ parray<PSOGCSaveFileChatShortcutEntry, 20> chat_shortcuts;
    /* 140C:0FF0 */ pstring<TextEncoding::SJIS, 0xAC> auto_reply;
    /* 14B8:109C */ pstring<TextEncoding::SJIS, 0xAC> info_board;
    // In this struct, place_counts[0] is win_count and [1] is loss_count
    /* 1564:1148 */ PlayerRecords_Battle<true> battle_records;
    /* 157C:1160 */ parray<uint8_t, 4> unknown_a10;
    /* 1580:1164 */ PlayerRecordsV3_Challenge<true>::Stats challenge_record_stats;
    /* 1658:123C */ Episode3::PlayerConfig ep3_config;
    /* 39A8:358C */ be_uint32_t unknown_a11;
    /* 39AC:3590 */ be_uint32_t unknown_a12;
    /* 39B0:3594 */ be_uint32_t unknown_a13;
    /* 39B4:3598 */
  } __attribute__((packed));
  /* 00004 */ parray<Character, 7> characters;
  /* 193F0 */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
  /* 19400 */ pstring<TextEncoding::ASCII, 0x10> access_key; // As 12 ASCII characters (decimal)
  /* 19410 */ pstring<TextEncoding::ASCII, 0x10> password;
  // In Episode 3, this field still exists, but is unused since BGM test was
  // removed from the options menu in favor of the jukebox. The jukebox is
  // accessible online only, and which songs are available there is controlled
  // by the B7 command sent by the server instead.
  /* 19420 */ be_uint64_t bgm_test_songs_unlocked;
  /* 19428 */ be_uint32_t save_count;
  // This is an array of 999 bits, represented here as 128 bytes (the last bit
  // is never used). Each bit corresponds to a card ID with the bit's index; if
  // the bit is set, then during offline play, the card's rank is replaced with
  // D2 if its original rank is S, SS, E, or D2, or with D1 if the original rank
  // is any other value. Upon receiving a B8 command (server card definitions),
  // the game clears this array, and sets all bits whose corresponding cards
  // from the server have the D1 or D2 ranks. This could have been used by Sega
  // to prevent broken cards from being used offline, but there's no indication
  // that they ever used this functionality.
  /* 1942C */ parray<uint8_t, 0x80> card_rank_override_flags;
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
    /* 0008 */ pstring<TextEncoding::SJIS, 0x18> name;
    /* 0020 */ pstring<TextEncoding::SJIS, 0x6C> description;
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
    /* 0094 */ pstring<TextEncoding::SJIS, 0x6C> comment;
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
  // Pixels are stored as 4x4 blocks of RGB565 values. See the implementation
  // of decode_image for details.
  /* 00008 */ parray<be_uint16_t, 0xC000> pixels;
  /* 18008 */ uint8_t unknown_a1; // Always 0x18?
  /* 18009 */ uint8_t unknown_a2;
  /* 1800A */ be_int16_t max_players;
  /* 1800C */ parray<be_uint32_t, 12> players_present;
  /* 1803C */ parray<be_uint32_t, 12> player_levels;
  /* 1806C */ parray<pstring<TextEncoding::SJIS, 0x18>, 12> player_names;
  /* 1818C */

  bool checksum_correct() const;
  Image decode_image() const;
} __attribute__((packed));

template <bool IsBigEndian>
std::string decrypt_data_section(const void* data_section, size_t size, uint32_t round1_seed, size_t max_decrypt_bytes = 0) {
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
std::string encrypt_data_section(const void* data_section, size_t size, uint32_t round1_seed) {
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

template <bool IsBigEndian>
std::string decrypt_fixed_size_data_section_s(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    bool skip_checksum = false,
    uint64_t override_round2_seed = 0xFFFFFFFFFFFFFFFF) {
  using U32T = std::conditional_t<IsBigEndian, be_uint32_t, le_uint32_t>;

  if (size < 2 * sizeof(U32T)) {
    throw std::runtime_error("data size is too small");
  }
  std::string decrypted = decrypt_data_section<IsBigEndian>(data_section, size, round1_seed);

  uint32_t round2_seed = override_round2_seed < 0x100000000
      ? static_cast<uint32_t>(override_round2_seed)
      : reinterpret_cast<const U32T*>(decrypted.data() + decrypted.size() - sizeof(U32T))->load();
  PSOV2Encryption round2_crypt(round2_seed);
  if (IsBigEndian) {
    round2_crypt.encrypt_big_endian(decrypted.data(), decrypted.size() - sizeof(U32T));
  } else {
    round2_crypt.encrypt(decrypted.data(), decrypted.size() - sizeof(U32T));
  }

  if (!skip_checksum) {
    U32T& checksum = *reinterpret_cast<U32T*>(decrypted.data());
    uint32_t expected_crc = checksum;
    checksum = 0;
    uint32_t actual_crc = crc32(decrypted.data(), decrypted.size());
    checksum = expected_crc;
    if (expected_crc != actual_crc) {
      throw std::runtime_error(string_printf(
          "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
          expected_crc, actual_crc));
    }
  }

  return decrypted;
}

template <typename StructT, bool IsBigEndian>
StructT decrypt_fixed_size_data_section_t(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    bool skip_checksum = false,
    uint64_t override_round2_seed = 0xFFFFFFFFFFFFFFFF) {

  std::string decrypted = decrypt_data_section<IsBigEndian>(data_section, size, round1_seed);
  if (decrypted.size() < sizeof(StructT)) {
    throw std::runtime_error("file too small for structure");
  }
  StructT ret = *reinterpret_cast<const StructT*>(decrypted.data());

  PSOV2Encryption round2_crypt(override_round2_seed < 0x100000000 ? override_round2_seed : ret.round2_seed.load());
  if (IsBigEndian) {
    round2_crypt.encrypt_big_endian(&ret, offsetof(StructT, round2_seed));
  } else {
    round2_crypt.encrypt(&ret, offsetof(StructT, round2_seed));
  }

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

template <bool IsBigEndian>
std::string encrypt_fixed_size_data_section_s(const void* data, size_t size, uint32_t round1_seed) {
  using U32T = std::conditional_t<IsBigEndian, be_uint32_t, le_uint32_t>;

  if (size < 2 * sizeof(U32T)) {
    throw std::runtime_error("data size is too small");
  }

  uint32_t round2_seed = random_object<uint32_t>();

  std::string encrypted(reinterpret_cast<const char*>(data), size);
  *reinterpret_cast<U32T*>(encrypted.data()) = 0;
  *reinterpret_cast<U32T*>(encrypted.data() + encrypted.size() - sizeof(U32T)) = round2_seed;
  *reinterpret_cast<U32T*>(encrypted.data()) = crc32(encrypted.data(), encrypted.size());

  PSOV2Encryption round2_crypt(round2_seed);
  if (IsBigEndian) {
    round2_crypt.encrypt_big_endian(encrypted.data(), encrypted.size());
  } else {
    round2_crypt.encrypt(encrypted.data(), encrypted.size());
  }

  return encrypt_data_section<IsBigEndian>(encrypted.data(), encrypted.size(), round1_seed);
}

template <typename StructT, bool IsBigEndian>
std::string encrypt_fixed_size_data_section_t(const StructT& s, uint32_t round1_seed) {
  StructT encrypted = s;
  encrypted.checksum = 0;
  encrypted.round2_seed = random_object<uint32_t>();
  encrypted.checksum = crc32(&encrypted, sizeof(encrypted));

  PSOV2Encryption round2_crypt(encrypted.round2_seed);
  if (IsBigEndian) {
    round2_crypt.encrypt_big_endian(&encrypted, offsetof(StructT, round2_seed));
  } else {
    round2_crypt.encrypt(&encrypted, offsetof(StructT, round2_seed));
  }

  return encrypt_data_section<IsBigEndian>(&encrypted, sizeof(StructT), round1_seed);
}

std::string decrypt_gci_fixed_size_data_section_for_salvage(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    uint64_t round2_seed,
    size_t max_decrypt_bytes);

uint32_t compute_psogc_timestamp(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second);

struct PSOPCCreationTimeFile { // PSO______FLS
  // The game creates this file if necessary and fills it with random data.
  // Most of the random data appears to be a decoy; only one field is used.
  // As in other PSO versions, creation_timestamp is used as an encryption key
  // for the other save files, but only if the serial number isn't set in the
  // Windows registry.
  /* 0000 */ parray<uint8_t, 0x624> unused1;
  /* 0624 */ le_uint32_t creation_timestamp;
  /* 0628 */ parray<uint8_t, 0xDD8> unused2;
  /* 1400 */
} __attribute__((packed));

struct PSOPCSystemFile { // PSO______COM
  /* 0000 */ le_uint32_t checksum;
  // Most of these fields are guesses based on the format used in GC and the
  // assumption that Sega didn't change much between versions.
  /* 0004 */ le_int16_t music_volume;
  /* 0006 */ int8_t sound_volume;
  /* 0007 */ uint8_t language;
  /* 0008 */ le_uint32_t server_time_delta_frames;
  /* 000C */ parray<le_uint16_t, 0x10> unknown_a4; // Last one is always 0x1234?
  /* 002C */ parray<uint8_t, 0x100> event_flags;
  /* 012C */ le_uint32_t round1_seed;
  /* 0130 */ parray<uint8_t, 0xD0> end_padding;
  /* 0200 */
} __attribute__((packed));

struct PSOPCGuildCardFile { // PSO______GUD
  /* 0000 */ le_uint32_t checksum;
  // TODO: Figure out the PC guild card format.
  /* 0004 */ parray<uint8_t, 0x7980> unknown_a1;
  /* 7984 */ le_uint32_t creation_timestamp;
  /* 7988 */ le_uint32_t round2_seed;
  /* 798C */ parray<uint8_t, 0x74> end_padding;
  /* 7A00 */
} __attribute__((packed));

struct PSOPCCharacterFile { // PSO______SYS and PSO______SYD
  /* 00000 */ le_uint32_t signature; // 'CAEN' (stored as 4E 45 41 43)
  /* 00004 */ le_uint32_t extra_headers; // 1
  /* 00008 */ le_uint32_t num_entries; // 0x80
  /* 0000C */ le_uint32_t entry_size; // 0x1D54 (actual entry size is +0x40)
  /* 00010 */ parray<uint8_t, 0x430> unknown_a1;
  struct CharacterEntry {
    /* 0000 */ le_uint32_t present; // 1 if character present, 0 if empty
    struct Character {
      /* 0000 */ le_uint32_t checksum;
      /* 0004 */ PlayerInventory inventory;
      /* 0350 */ PlayerDispDataDCPCV3 disp;
      /* 0420 */ be_uint32_t flags;
      /* 0424 */ be_uint32_t creation_timestamp;
      /* 0428 */ be_uint32_t signature; // == 0x6C5D889E?
      /* 042C */ be_uint32_t play_time_seconds;
      /* 0430 */ be_uint32_t option_flags; // TODO: document bits in this field
      /* 0434 */ be_uint32_t save_count;
      // TODO: Figure out what this is. On GC, this is where the bank data goes.
      /* 0438 */ parray<uint8_t, 0x7D4> unknown_a2;
      /* 0C0C */ GuildCardPC guild_card;
      /* 0CFC */ parray<PSOPCSaveFileSymbolChatEntry, 12> symbol_chats;
      // TODO: Figure out what this is. On GC, this is where chat shortcuts and
      // challenge/battle records go.
      /* 123C */ parray<uint8_t, 0xAA0> unknown_a3;
      /* 1CDC */ parray<le_uint16_t, 20> tech_menu_shortcut_entries;
      /* 1D04 */ parray<uint8_t, 0x2C> unknown_a4;
      /* 1D30 */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
      /* 1D40 */ pstring<TextEncoding::ASCII, 0x10> access_key; // As decimal
      /* 1D50 */ le_uint32_t round2_seed;
      /* 1D54 */
    } __attribute__((packed));
    /* 0004 */ Character character;
    /* 1D58 */ parray<uint8_t, 0x3C> unused;
    /* 1D94 */
  } __attribute__((packed));
  /* 00440 */ parray<CharacterEntry, 0x80> entries;
  /* ECE40 */
} __attribute__((packed));
