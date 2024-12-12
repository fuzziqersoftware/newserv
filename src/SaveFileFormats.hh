#pragma once

#include <stdint.h>

#include <algorithm>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Image.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <string>

#include "ChoiceSearch.hh"
#include "Episode3/DataIndexes.hh"
#include "ItemNameIndex.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "PlayerSubordinates.hh"
#include "Text.hh"

////////////////////////////////////////////////////////////////////////////////
// Memory card / VMU structures

struct PSOVMSFileHeader {
  /* 0000 */ pstring<TextEncoding::MARKED, 0x10> short_desc;
  /* 0010 */ pstring<TextEncoding::MARKED, 0x20> long_desc;
  /* 0030 */ pstring<TextEncoding::MARKED, 0x10> creator_id;
  /* 0040 */ le_uint16_t num_icons = 1;
  /* 0042 */ le_uint16_t animation_speed = 1;
  /* 0044 */ le_uint16_t eyecatch_type = 0;
  /* 0046 */ le_uint16_t crc = 0;
  /* 0048 */ le_uint32_t data_size = 0; // Not including header and icons
  /* 004C */ parray<uint8_t, 0x14> unused;
  /* 0060 */ parray<le_uint16_t, 0x10> icon_palette;
  // Variable-length field:
  /* 0080 */ // parray<parray<uint8_t, 0x200>, num_icons> icon;

  bool checksum_correct() const;
  void check() const;
  inline size_t icon_data_size() const {
    return this->num_icons * 0x200;
  }

  bool is_v2() const;
} __packed_ws__(PSOVMSFileHeader, 0x80);

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
  /* 0006 */ uint8_t unused = 0;
  /* 0007 */ uint8_t image_flags = 0;
  /* 0008 */ pstring<TextEncoding::MARKED, 0x20> internal_file_name;
  /* 0028 */ be_uint32_t modification_time = 0;
  /* 002C */ be_uint32_t image_data_offset = 0;
  /* 0030 */ be_uint16_t icon_formats = 0;
  /* 0032 */ be_uint16_t icon_animation_speeds = 0;
  /* 0034 */ uint8_t permission_flags = 0;
  /* 0035 */ uint8_t copy_count = 0;
  /* 0036 */ be_uint16_t first_block_index = 0;
  /* 0038 */ be_uint16_t num_blocks = 0;
  /* 003A */ parray<uint8_t, 2> unused2;
  /* 003C */ be_uint32_t comment_offset = 0;
  // GCI header ends here (and memcard file data begins here)
  // game_name is e.g. "PSO EPISODE I & II" or "PSO EPISODE III"
  /* 0040 */ pstring<TextEncoding::MARKED, 0x1C> game_name;
  /* 005C */ be_uint32_t embedded_seed = 0; // Used in some of Ralf's quest packs
  /* 0060 */ pstring<TextEncoding::MARKED, 0x20> file_name;
  /* 0080 */ parray<uint8_t, 0x1800> banner;
  /* 1880 */ parray<uint8_t, 0x800> icon;
  // data_size specifies the number of bytes remaining in the file. In all cases
  // except for the system file, this data is encrypted.
  /* 2080 */ be_uint32_t data_size = 0;
  // To compute checksum, set checksum to zero, then compute the CRC32 of all
  // fields in this struct starting with gci_header.game_name. (Yes, including
  // the checksum field, which is temporarily zero.) See checksum_correct below.
  /* 2084 */ be_uint32_t checksum = 0;
  /* 2088 */

  bool checksum_correct() const;
  void check() const;

  bool is_ep12() const;
  bool is_ep3() const;
  bool is_nte() const;
} __packed_ws__(PSOGCIFileHeader, 0x2088);

////////////////////////////////////////////////////////////////////////////////
// Subordinate structures

struct ShuffleTables {
  uint8_t forward_table[0x100];
  uint8_t reverse_table[0x100];

  ShuffleTables(PSOV2Encryption& crypt);

  static uint32_t pseudorand(PSOV2Encryption& crypt, uint32_t prev);

  void shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const;
};

template <bool BE, TextEncoding Encoding, size_t NameLength>
struct SaveFileSymbolChatEntryT {
  /* DC:PC:GC:XB:BB */
  /* 00:00:00:00:00 */ U32T<BE> present;
  /* 04:04:04:04:04 */ pstring<Encoding, NameLength> name;
  /* 1C:34:1C:1C:2C */ SymbolChatT<BE> spec;
  /* 58:70:58:58:68 */
} __packed__;
using SaveFileSymbolChatEntryPC = SaveFileSymbolChatEntryT<false, TextEncoding::UTF16, 0x18>;
using SaveFileSymbolChatEntryGC = SaveFileSymbolChatEntryT<true, TextEncoding::MARKED, 0x18>;
using SaveFileSymbolChatEntryDCXB = SaveFileSymbolChatEntryT<false, TextEncoding::MARKED, 0x18>;
using SaveFileSymbolChatEntryBB = SaveFileSymbolChatEntryT<false, TextEncoding::UTF16, 0x14>;
check_struct_size(SaveFileSymbolChatEntryPC, 0x70);
check_struct_size(SaveFileSymbolChatEntryGC, 0x58);
check_struct_size(SaveFileSymbolChatEntryDCXB, 0x58);
check_struct_size(SaveFileSymbolChatEntryBB, 0x68);

template <bool BE>
struct WordSelectMessageT {
  U16T<BE> num_tokens = 0;
  U16T<BE> target_type = 0;
  parray<U16T<BE>, 8> tokens;
  U32T<BE> numeric_parameter = 0;
  U32T<BE> unknown_a4 = 0;

  operator WordSelectMessageT<!BE>() const {
    WordSelectMessageT<!BE> ret;
    ret.num_tokens = this->num_tokens.load();
    ret.target_type = this->target_type.load();
    for (size_t z = 0; z < this->tokens.size(); z++) {
      ret.tokens[z] = this->tokens[z].load();
    }
    ret.numeric_parameter = this->numeric_parameter.load();
    ret.unknown_a4 = this->unknown_a4.load();
    return ret;
  }
} __packed__;
using WordSelectMessage = WordSelectMessageT<false>;
using WordSelectMessageBE = WordSelectMessageT<true>;
check_struct_size(WordSelectMessage, 0x1C);
check_struct_size(WordSelectMessageBE, 0x1C);

template <bool BE, TextEncoding Encoding, size_t MaxChars>
struct SaveFileChatShortcutEntryT {
  union Definition {
    pstring<Encoding, MaxChars> text;
    WordSelectMessageT<BE> word_select;
    SymbolChatT<BE> symbol_chat;

    Definition() : text() {}
    Definition(const Definition& other) : text(other.text) {}
    Definition& operator=(const Definition& other) {
      this->text = other.text;
      return *this;
    }
  } __packed__;

  /* DC:GC:BB */
  /* 00:00:00 */ U32T<BE> type; // 1 = text, 2 = word select, 3 = symbol chat
  /* 04:04:04 */ Definition definition;
  /* 40:54:A4 */

  template <bool RetBE, TextEncoding RetEncoding, size_t RetMaxSize>
  SaveFileChatShortcutEntryT<RetBE, RetEncoding, RetMaxSize> convert(uint8_t language) const {
    SaveFileChatShortcutEntryT<RetBE, RetEncoding, RetMaxSize> ret;
    ret.type = this->type.load();
    switch (ret.type) {
      case 1:
        ret.definition.text.encode(this->definition.text.decode(language), language);
        break;
      case 2:
        // TODO: We should translate the message across PSO versions if
        // possible, but this is a lossy process :|
        ret.definition.word_select = this->definition.word_select;
        break;
      case 3:
        ret.definition.symbol_chat = this->definition.symbol_chat;
        break;
    }
    return ret;
  }
} __packed__;
using SaveFileShortcutEntryDC = SaveFileChatShortcutEntryT<false, TextEncoding::MARKED, 0x3C>;
using SaveFileShortcutEntryPC = SaveFileChatShortcutEntryT<false, TextEncoding::UTF16, 0x3C>;
using SaveFileShortcutEntryGC = SaveFileChatShortcutEntryT<true, TextEncoding::MARKED, 0x50>;
using SaveFileShortcutEntryXB = SaveFileChatShortcutEntryT<false, TextEncoding::MARKED, 0x50>;
using SaveFileShortcutEntryBB = SaveFileChatShortcutEntryT<false, TextEncoding::UTF16, 0x50>;
check_struct_size(SaveFileShortcutEntryDC, 0x40);
check_struct_size(SaveFileShortcutEntryPC, 0x7C);
check_struct_size(SaveFileShortcutEntryGC, 0x54);
check_struct_size(SaveFileShortcutEntryXB, 0x54);
check_struct_size(SaveFileShortcutEntryBB, 0xA4);

struct PSOBBTeamMembership {
  /* 0000 */ le_uint32_t team_master_guild_card_number = 0;
  /* 0004 */ le_uint32_t team_id = 0;
  /* 0008 */ le_uint32_t unknown_a5 = 0;
  /* 000C */ le_uint32_t unknown_a6 = 0;
  /* 0010 */ uint8_t privilege_level = 0;
  /* 0011 */ uint8_t unknown_a7 = 0;
  /* 0012 */ uint8_t unknown_a8 = 0;
  /* 0013 */ uint8_t unknown_a9 = 0;
  /* 0014 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
  /* 0034 */ parray<le_uint16_t, 0x20 * 0x20> flag_data;
  /* 0834 */ le_uint32_t reward_flags = 0;
  /* 0838 */

  PSOBBTeamMembership() = default;
} __packed_ws__(PSOBBTeamMembership, 0x838);

////////////////////////////////////////////////////////////////////////////////
// System files

struct PSOPCCreationTimeFile { // PSO______FLS
  // The game creates this file if necessary and fills it with random data.
  // Most of the random data appears to be a decoy; only one field is used.
  // As in other PSO versions, creation_timestamp is used as an encryption key
  // for the other save files, but only if the serial number isn't set in the
  // Windows registry.
  /* 0000 */ parray<uint8_t, 0x624> unused1;
  /* 0624 */ le_uint32_t creation_timestamp = 0;
  /* 0628 */ parray<uint8_t, 0xDD8> unused2;
  /* 1400 */
} __packed_ws__(PSOPCCreationTimeFile, 0x1400);

struct PSOPCSystemFile { // PSO______COM
  /* 0000 */ le_uint32_t checksum = 0;
  // Most of these fields are guesses based on the format used in GC and the
  // assumption that Sega didn't change much between versions.
  /* 0004 */ le_int16_t music_volume = 0;
  /* 0006 */ int8_t sound_volume = 0;
  /* 0007 */ uint8_t language = 1;
  /* 0008 */ le_int32_t server_time_delta_frames = 1728000;
  /* 000C */ parray<le_uint16_t, 0x10> unknown_a4; // Last one is always 0x1234?
  /* 002C */ parray<uint8_t, 0x100> event_flags;
  /* 012C */ le_uint32_t round1_seed = 0;
  /* 0130 */ parray<uint8_t, 0xD0> end_padding;
  /* 0200 */
} __packed_ws__(PSOPCSystemFile, 0x200);

struct PSOGCSystemFile {
  /* 0000 */ be_uint32_t checksum = 0;
  /* 0004 */ be_int16_t music_volume = 0; // 0 = full volume; -250 = min volume
  /* 0006 */ int8_t sound_volume = 0; // 0 = full volume; -100 = min volume
  /* 0007 */ uint8_t language = 1;
  // This field stores the effective time zone offset between the server and
  // client, in frames. The default value is 1728000, which corresponds to 16
  // hours. This is recomputed when the client receives a B1 command.
  /* 0008 */ be_int32_t server_time_delta_frames = 1728000;
  /* 000C */ be_uint16_t udp_behavior = 0; // 0 = auto, 1 = on, 2 = off
  /* 000E */ be_uint16_t surround_sound_enabled = 0;
  /* 0010 */ parray<uint8_t, 0x100> event_flags; // Can be set by quest opcode D8 or E8
  /* 0110 */ parray<uint8_t, 8> unknown_a7;
  // This timestamp is the number of seconds since 12:00AM on 1 January 2000.
  // This field is also used as the round1 seed for encrypting the character and
  // Guild Card files.
  /* 0118 */ be_uint32_t creation_timestamp = 0;
  /* 011C */
} __packed_ws__(PSOGCSystemFile, 0x11C);

struct PSOGCEp3SystemFile {
  /* 0000 */ PSOGCSystemFile base;
  /* 011C */ int8_t unknown_a1 = 0;
  /* 011D */ parray<uint8_t, 11> unknown_a2;
  /* 0128 */ be_uint32_t unknown_a3 = 0;
  /* 012C */
} __packed_ws__(PSOGCEp3SystemFile, 0x12C);

struct PSOBBMinimalSystemFile {
  /* 0000 */ be_uint32_t checksum = 0;
  /* 0004 */ be_int16_t music_volume = 0;
  /* 0006 */ int8_t sound_volume = 0;
  /* 0007 */ uint8_t language = 0;
  /* 0008 */ be_int32_t server_time_delta_frames = 1728000;
  /* 000C */ be_uint16_t udp_behavior = 0; // 0 = auto, 1 = on, 2 = off
  /* 000E */ be_uint16_t surround_sound_enabled = 0;
  /* 0010 */ parray<uint8_t, 0x0100> event_flags;
  /* 0110 */ le_uint32_t creation_timestamp = 0;
  /* 0114 */
} __packed_ws__(PSOBBMinimalSystemFile, 0x114);

struct PSOBBBaseSystemFile : PSOBBMinimalSystemFile {
  /* 0114 */ parray<uint8_t, 0x016C> key_config;
  /* 0280 */ parray<uint8_t, 0x0038> joystick_config;
  /* 02B8 */

  PSOBBBaseSystemFile();
} __packed_ws__(PSOBBBaseSystemFile, 0x2B8);

////////////////////////////////////////////////////////////////////////////////
// Character files

struct PSODCNTECharacterFile {
  /* 0000 */ le_uint32_t checksum = 0;
  struct Character {
    // See PSOGCCharacterFile::Character for descriptions of fields' meanings.
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ le_uint32_t validation_flags = 0;
    /* 0420:0004 */ le_uint32_t creation_timestamp = 0;
    /* 0424:0008 */ le_uint32_t signature = 0xBB40711D;
    /* 0428:000C */ le_uint32_t play_time_seconds = 0;
    /* 042C:0010 */ le_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ le_uint16_t save_count_since_last_inventory_erasure = 1;
    /* 0432:0016 */ le_uint16_t inventory_erasure_count = 0;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    // TODO: Figure out how quest flags work; it's obviously different from 0x80
    // bytes per difficulty like in v1. Is it just 2048 flags shared across all
    // difficulties, instead of 1024 in each difficulty?
    /* 0460:0044 */ parray<uint8_t, 0x100> quest_flags;
    /* 0560:0144 */ le_uint16_t bank_meseta;
    /* 0562:0146 */ le_uint16_t num_bank_items;
    /* 0564:0148 */ parray<ItemData, 60> bank_items;
    /* 0A14:05F8 */ GuildCardDCNTE guild_card;
    /* 0A8F:0673 */ uint8_t unknown_s1; // Probably actually unused
    /* 0A90:0674 */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
    /* 0AA0:0684 */ pstring<TextEncoding::ASCII, 0x10> v1_access_key;
    /* 0AB0:0694 */
  } __packed_ws__(Character, 0xAB0);
  /* 0004 */ Character character;
  /* 0AB4 */ le_uint32_t round2_seed = 0;
  /* 0AB8 */
} __packed_ws__(PSODCNTECharacterFile, 0xAB8);

struct PSODC112000CharacterFile {
  /* 0000 */ le_uint32_t checksum = 0;
  struct Character {
    // See PSOGCCharacterFile::Character for descriptions of fields' meanings.
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ le_uint32_t validation_flags = 0;
    /* 0420:0004 */ le_uint32_t creation_timestamp = 0;
    /* 0424:0008 */ le_uint32_t signature = 0xBB40711D;
    /* 0428:000C */ le_uint32_t play_time_seconds = 0;
    /* 042C:0010 */ le_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ le_uint16_t save_count_since_last_inventory_erasure = 1;
    /* 0432:0016 */ le_uint16_t inventory_erasure_count = 0;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    // 11/2000 just has 0x800 quest flags, and they're not split by difficulty
    /* 0460:0044 */ parray<uint8_t, 0x100> quest_flags;
    /* 0560:0144 */ le_uint16_t bank_meseta;
    /* 0562:0146 */ le_uint16_t num_bank_items;
    /* 0564:0148 */ parray<ItemData, 60> bank_items;
    /* 0A14:05F8 */ GuildCardDC guild_card;
    /* 0A91:0675 */ parray<uint8_t, 3> unknown_s1; // Probably actually unused
    /* 0A94:0678 */ parray<SaveFileSymbolChatEntryDCXB, 12> symbol_chats;
    /* 0EB4:0A98 */ parray<SaveFileShortcutEntryDC, 20> shortcuts;
    /* 13B4:0F98 */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
    /* 13C4:0FA8 */ pstring<TextEncoding::ASCII, 0x10> v1_access_key;
    /* 13D4:0FB8 */
  } __packed_ws__(Character, 0x13D4);
  /* 0004 */ Character character;
  /* 13D8 */ le_uint32_t round2_seed = 0;
} __packed_ws__(PSODC112000CharacterFile, 0x13DC);

struct PSODCV1CharacterFile {
  /* 0000 */ le_uint32_t checksum = 0;
  struct Character {
    // See PSOGCCharacterFile::Character for descriptions of fields' meanings.
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ le_uint32_t validation_flags = 0;
    /* 0420:0004 */ le_uint32_t creation_timestamp = 0;
    /* 0424:0008 */ le_uint32_t signature = 0xA205B064;
    /* 0428:000C */ le_uint32_t play_time_seconds = 0;
    /* 042C:0010 */ le_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ le_uint32_t save_count = 1;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    /* 0460:0044 */ QuestFlagsV1 quest_flags;
    /* 05E0:01C4 */ PlayerBank60 bank;
    /* 0B88:076C */ GuildCardDC guild_card;
    /* 0C05:07E9 */ parray<uint8_t, 3> unknown_s1; // Probably actually unused
    /* 0C08:07EC */ parray<SaveFileSymbolChatEntryDCXB, 12> symbol_chats;
    /* 1028:0C0C */ parray<SaveFileShortcutEntryDC, 20> shortcuts;
    /* 1528:110C */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
    /* 1538:111C */ pstring<TextEncoding::ASCII, 0x10> v1_access_key;
    /* 1548:112C */
  } __packed_ws__(Character, 0x1548);
  /* 0004 */ Character character;
  /* 154C */ le_uint32_t round2_seed = 0;
} __packed_ws__(PSODCV1CharacterFile, 0x1550);

struct PSODCV2CharacterFile {
  /* 0000 */ le_uint32_t checksum = 0;
  struct Character {
    // See PSOGCCharacterFile::Character for descriptions of fields' meanings.
    /* 0000:---- */ PlayerInventory inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3 disp;
    /* 041C:0000 */ le_uint32_t validation_flags = 0;
    /* 0420:0004 */ le_uint32_t creation_timestamp = 0;
    /* 0424:0008 */ le_uint32_t signature = 0xA205B064;
    /* 0428:000C */ le_uint32_t play_time_seconds = 0;
    /* 042C:0010 */ le_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ le_uint32_t save_count = 1;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    /* 0460:0044 */ QuestFlags quest_flags;
    /* 0660:0244 */ PlayerBank60 bank;
    /* 0C08:07EC */ GuildCardDC guild_card;
    /* 0C85:0869 */ parray<uint8_t, 3> unknown_s1; // Probably actually unused
    /* 0C88:086C */ parray<SaveFileSymbolChatEntryDCXB, 12> symbol_chats;
    /* 10A8:0C8C */ parray<SaveFileShortcutEntryDC, 20> shortcuts;
    /* 15A8:118C */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
    /* 15B8:119C */ pstring<TextEncoding::ASCII, 0x10> v1_access_key;
    /* 15C8:11AC */ PlayerRecordsBattle battle_records;
    /* 15E0:11C4 */ PlayerRecordsChallengeDC challenge_records;
    /* 1680:1264 */ parray<le_uint16_t, 20> tech_menu_shortcut_entries;
    // The Choice Search config is stored here as 32-bit integers, even though
    // it's represented with 16-bit integers in the various commands that send it
    // to and from the server. The order of the entries here is the same (that
    // is, the first two of these ints are entries[0], the second two are
    // entries[1], etc.).
    /* 16A8:128C */ parray<le_uint32_t, 10> choice_search_config;
    /* 16D0:12B4 */ parray<uint8_t, 4> unknown_a2;
    /* 16D4:12B8 */ pstring<TextEncoding::ASCII, 0x10> v2_serial_number;
    /* 16E4:12C8 */ pstring<TextEncoding::ASCII, 0x10> v2_access_key;
    /* 16F4:12D8 */
  } __packed_ws__(Character, 0x16F4);
  /* 0004 */ Character character;
  /* 16F0 */ le_uint32_t round2_seed = 0;
} __packed_ws__(PSODCV2CharacterFile, 0x16FC);

struct PSOPCCharacterFile { // PSO______SYS and PSO______SYD
  // See PSOGCCharacterFile::Character for descriptions of fields' meanings.
  /* 00000 */ le_uint32_t signature = 0x4341454E; // 'CAEN' (stored as 4E 45 41 43)
  /* 00004 */ le_uint32_t extra_headers = 1;
  /* 00008 */ le_uint32_t num_entries = 0x80;
  /* 0000C */ le_uint32_t entry_size = 0x1D54; // Actual entry size is +0x40
  /* 00010 */ parray<uint8_t, 0x430> unknown_a1;
  struct CharacterEntry {
    /* 0000 */ le_uint32_t present = 1; // 1 if character present, 0 if empty
    struct EncryptedSection {
      /* 0000 */ le_uint32_t checksum = 0;
      struct Character {
        /* 0000 */ PlayerInventory inventory;
        /* 034C */ PlayerDispDataDCPCV3 disp;
        /* 041C */ be_uint32_t validation_flags = 0;
        /* 0420 */ be_uint32_t creation_timestamp = 0;
        /* 0424 */ be_uint32_t signature = 0x6C5D889E;
        /* 0428 */ be_uint32_t play_time_seconds = 0;
        /* 042C */ be_uint32_t option_flags = 0x00040058;
        /* 0430 */ be_uint32_t save_count = 1;
        /* 0434 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
        /* 0450 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
        /* 0460 */ QuestFlags quest_flags;
        /* 0660 */ PlayerBank60 bank;
        /* 0C08 */ GuildCardPC guild_card;
        /* 0CF8 */ parray<SaveFileSymbolChatEntryPC, 12> symbol_chats;
        /* 1238 */ parray<SaveFileShortcutEntryPC, 20> shortcuts;
        /* 1BE8 */ PlayerRecordsBattle battle_records;
        /* 1C00 */ PlayerRecordsChallengePC challenge_records;
        /* 1CD8 */ parray<le_uint16_t, 20> tech_menu_shortcut_entries;
        /* 1D00 */ parray<le_uint32_t, 10> choice_search_config;
        /* 1D28 */ parray<uint8_t, 4> unknown_a2;
        /* 1D2C */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
        /* 1D3C */ pstring<TextEncoding::ASCII, 0x10> access_key; // As decimal
        /* 1D4C */
      } __packed_ws__(Character, 0x1D4C);
      /* 0004 */ Character character;
      /* 1D50 */ le_uint32_t round2_seed = 0;
      /* 1D54 */
    } __packed_ws__(EncryptedSection, 0x1D54);
    /* 0004 */ EncryptedSection encrypted;
    /* 1D58 */ parray<uint8_t, 0x3C> unused;
    /* 1D94 */
  } __packed_ws__(CharacterEntry, 0x1D94);
  /* 00440 */ parray<CharacterEntry, 0x80> entries;
  /* ECE40 */
} __packed_ws__(PSOPCCharacterFile, 0xECE40);

struct PSOGCNTECharacterFileCharacter {
  /* 0000:---- */ PlayerInventoryBE inventory;
  /* 034C:---- */ PlayerDispDataDCPCV3BE disp;
  /* 041C:0000 */ be_uint32_t validation_flags = 0;
  /* 0420:0004 */ be_uint32_t creation_timestamp = 0;
  /* 0424:0008 */ be_uint32_t signature = 0xA205B064;
  /* 0428:000C */ be_uint32_t play_time_seconds = 0;
  /* 042C:0010 */ be_uint32_t option_flags = 0x00040058;
  /* 0430:0014 */ be_uint32_t save_count = 1;
  /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
  /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
  /* 0460:0044 */ QuestFlags quest_flags;
  /* 0660:0244 */ PlayerBank200BE bank;
  /* 1928:150C */ GuildCardGCNTEBE guild_card;
  /* 19CC:15B0 */ parray<SaveFileSymbolChatEntryGC, 12> symbol_chats;
  /* 1DEC:19D0 */ parray<SaveFileShortcutEntryGC, 20> shortcuts;
  /* 247C:2060 */ PlayerRecordsBattleBE battle_records;
  /* 2494:2078 */ parray<uint8_t, 4> unknown_a4;
  /* 2498:207C */ PlayerRecordsChallengeDC challenge_records;
  /* 2538:211C */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
  // TODO: choice_search_config and offline_battle_records may be in here
  // somewhere. When they are found, don't forget to update the conversion
  // functions in PSOBBCharacterFile.
  /* 2560:2144 */ parray<uint8_t, 0x130> unknown_n2;
  /* 2690:2274 */
} __packed_ws__(PSOGCNTECharacterFileCharacter, 0x2690);

struct PSOGCCharacterFile {
  /* 00000 */ be_uint32_t checksum = 0;
  struct Character {
    // This structure is internally split into two by the game. The offsets here
    // are relative to the start of this structure (first column), and relative
    // to the start of the second internal structure (second column).
    /* 0000:---- */ PlayerInventoryBE inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3BE disp;
    // Known bits in the validation_flags field:
    //   00000001: Character was not saved after disconnecting (and the message
    //     about items being deleted is shown in the select menu)
    //   00000002: Character has level out of range (< 0 or > max)
    //   00000004: Character has EXP out of range for their current level
    //   00000008: Character has one or more stats out of range (< 0 or > max)
    //   00000010: Character has ever possessed a hacked item, according to the
    //     check_for_hacked_item function in DCv2 (TODO: Does this exist in V3+
    //     also? If so, is the logic the same?)
    //   00000020: Character has meseta out of range (< 0 or > 999999)
    //   00000040: Character was loaded on a client that has "important" files
    //     modified (on GC, these files are ending_normal.sfd, psogc_j.sfd,
    //     psogc_j2.sfd, ult01.sfd, ult02.sfd, ult03.sfd, ult04.sfd,
    //     ItemPMT.prs, itemrt.gsl, itempt.gsl, and PlyLevelTbl.cpt). For files
    //     larger than 1000000 bytes (decimal), the game only checks the file's
    //     size and skips checksumming its contents.
    // It seems that v3 and BB only use flag 00000001 and ignore the rest.
    /* 041C:0000 */ be_uint32_t validation_flags = 0;
    /* 0420:0004 */ be_uint32_t creation_timestamp = 0;
    // The signature field holds the value 0xA205B064, which is 2718281828 in
    // decimal - approximately e * 10^9. It's unknown why Sega chose this
    // value. On some other versions, this field has a different value; see the
    // defaults in the other versions' structures.
    /* 0424:0008 */ be_uint32_t signature = 0xA205B064;
    /* 0428:000C */ be_uint32_t play_time_seconds = 0;
    // This field is a collection of several flags and small values. The known
    // fields are:
    //   ------AB -----CDD EEEFFFGG HIJKLMNO
    //   A = Function key setting (BB; 0 = menu shortcuts; 1 = chat shortcuts).
    //       This bit is unused by PSO GC.
    //   B = Keyboard controls (BB; 0 = on; 1 = off). This field is also used
    //       by PSO GC, but its function is currently unknown.
    //   C = Choice search setting (0 = enabled; 1 = disabled)
    //   D = Which pane of the shortcut menu was last used
    //   E = Player lobby labels (0 = name; 1 = name, language, and level;
    //       2 = W/D counts; 3 = challenge rank; 4 = nothing)
    //   F = Idle disconnect time (0 = 15 mins; 1 = 30 mins; 2 = 45 mins;
    //       3 = 60 mins; 4: never; 5-7: undefined behavior due to a missing
    //       bounds check).
    //   G = Message speed (0 = slow; 1 = normal; 2 = fast; 3 = very fast)
    //   H, I, J, K = unknown; these appear to be used only during Japanese
    //       text input. See TWindowKeyBoardBase_read_option_flags
    //   L = Rumble enabled
    //   M = Cursor position (0 = saved; 1 = non-saved)
    //   N = Button config (0 = normal; 1 = L/R reversed)
    //   O = Map direction (0 = non-fixed; 1 = fixed)
    /* 042C:0010 */ be_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ be_uint32_t save_count = 1;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    /* 0460:0044 */ QuestFlags quest_flags;
    /* 0660:0244 */ be_uint32_t death_count = 0;
    /* 0664:0248 */ PlayerBank200BE bank;
    /* 192C:1510 */ GuildCardGCBE guild_card;
    /* 19BC:15A0 */ parray<SaveFileSymbolChatEntryGC, 12> symbol_chats;
    /* 1DDC:19C0 */ parray<SaveFileShortcutEntryGC, 20> shortcuts;
    /* 246C:2050 */ pstring<TextEncoding::MARKED, 0xAC> auto_reply;
    /* 2518:20FC */ pstring<TextEncoding::MARKED, 0xAC> info_board;
    /* 25C4:21A8 */ PlayerRecordsBattleBE battle_records;
    /* 25DC:21C0 */ parray<uint8_t, 4> unknown_a4;
    /* 25E0:21C4 */ PlayerRecordsChallengeV3BE challenge_records;
    /* 26E0:22C4 */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
    /* 2708:22EC */ ChoiceSearchConfigBE choice_search_config;
    /* 2720:2304 */ parray<uint8_t, 0x10> unknown_a6;
    /* 2730:2314 */ parray<be_uint32_t, 0x10> quest_counters;
    /* 2770:2354 */ PlayerRecordsBattleBE offline_battle_records;
    /* 2788:236C */ parray<uint8_t, 4> unknown_a7;
    /* 278C:2370 */ be_uint32_t unknown_f6 = 0;
    /* 2790:2374 */ be_uint32_t unknown_f7 = 0;
    /* 2794:2378 */ be_uint32_t unknown_f8 = 0;
    /* 2798:237C */
  } __packed_ws__(Character, 0x2798);
  /* 00004 */ parray<Character, 7> characters; // 0-3: main chars, 4-6: temps
  /* 1152C */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
  /* 1153C */ pstring<TextEncoding::ASCII, 0x10> access_key;
  /* 1154C */ pstring<TextEncoding::ASCII, 0x10> password;
  /* 1155C */ be_uint64_t bgm_test_songs_unlocked = 0;
  /* 11564 */ be_uint32_t save_count = 1;
  /* 11568 */ be_uint32_t round2_seed = 0;
  /* 1156C */
} __packed_ws__(PSOGCCharacterFile, 0x1156C);

struct PSOGCEp3NTECharacter {
  // This structure is internally split into two by the game. The offsets here
  // are relative to the start of this structure (first column), and relative
  // to the start of the second internal structure (second column).
  /* 0000:---- */ PlayerInventoryBE inventory;
  /* 034C:---- */ PlayerDispDataDCPCV3BE disp;
  /* 041C:0000 */ be_uint32_t validation_flags = 0;
  /* 0420:0004 */ be_uint32_t creation_timestamp = 0;
  /* 0424:0008 */ be_uint32_t signature = 0xA205B064;
  /* 0428:000C */ be_uint32_t play_time_seconds = 0;
  /* 042C:0010 */ be_uint32_t option_flags = 0x00040058;
  /* 0430:0014 */ be_uint32_t save_count = 1;
  /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
  /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
  /* 0460:0044 */ parray<uint8_t, 0x400> seq_vars;
  /* 0860:0444 */ be_uint32_t death_count = 0;
  /* 0864:0448 */ PlayerBank200BE bank;
  /* 1B2C:1710 */ GuildCardGCBE guild_card;
  /* 1BBC:17A0 */ parray<SaveFileSymbolChatEntryGC, 12> symbol_chats;
  /* 1FDC:1BC0 */ parray<SaveFileShortcutEntryGC, 20> chat_shortcuts;
  /* 266C:2250 */ pstring<TextEncoding::MARKED, 0xAC> auto_reply;
  /* 2718:22FC */ pstring<TextEncoding::MARKED, 0xAC> info_board;
  /* 27C4:23A8 */ PlayerRecordsBattleBE battle_records;
  /* 27DC:23C0 */ parray<uint8_t, 4> unknown_a10;
  /* 27E0:23C4 */ PlayerRecordsChallengeV3BE::Stats challenge_record_stats;
  /* 28B8:249C */ Episode3::PlayerConfigNTE ep3_config;
  /* 4610:41F4 */ be_uint32_t unknown_a11 = 0;
  /* 4614:41F8 */ be_uint32_t unknown_a12 = 0;
  /* 4618:41FC */ be_uint32_t unknown_a13 = 0;
  /* 461C:4200 */

  PSOGCEp3NTECharacter() = default;
} __packed_ws__(PSOGCEp3NTECharacter, 0x461C);

struct PSOGCEp3CharacterFile {
  /* 00000 */ be_uint32_t checksum = 0; // crc32 of this field (as 0) through end of struct
  struct Character {
    // This structure is internally split into two by the game. The offsets here
    // are relative to the start of this structure (first column), and relative
    // to the start of the second internal structure (second column).
    /* 0000:---- */ PlayerInventoryBE inventory;
    /* 034C:---- */ PlayerDispDataDCPCV3BE disp;
    /* 041C:0000 */ be_uint32_t validation_flags = 0;
    /* 0420:0004 */ be_uint32_t creation_timestamp = 0;
    /* 0424:0008 */ be_uint32_t signature = 0xA205B064;
    /* 0428:000C */ be_uint32_t play_time_seconds = 0;
    /* 042C:0010 */ be_uint32_t option_flags = 0x00040058;
    /* 0430:0014 */ be_uint32_t save_count = 1;
    /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
    /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
    // seq_vars is an array of 8192 bits, which contain all the Episode 3 quest
    // progress flags. This includes things like which maps are unlocked, which
    // NPC decks are unlocked, and whether the player has a VIP card or not.
    // Logically, this structure maps to quest_flags in other versions, but is
    // a different size.
    /* 0460:0044 */ parray<uint8_t, 0x400> seq_vars;
    /* 0860:0444 */ be_uint32_t death_count = 0;
    // Curiously, Episode 3 characters do have item banks, but there are only 4
    // item slots. Presumably Sega didn't completely remove the bank in Ep3
    // because they would have had to change too much code.
    /* 0864:0448 */ PlayerBankT<4, true> bank;
    /* 08CC:04B0 */ GuildCardGCBE guild_card;
    /* 095C:0540 */ parray<SaveFileSymbolChatEntryGC, 12> symbol_chats;
    /* 0D7C:0960 */ parray<SaveFileShortcutEntryGC, 20> chat_shortcuts;
    /* 140C:0FF0 */ pstring<TextEncoding::MARKED, 0xAC> auto_reply;
    /* 14B8:109C */ pstring<TextEncoding::MARKED, 0xAC> info_board;
    // In this struct, place_counts[0] is win_count and [1] is loss_count
    /* 1564:1148 */ PlayerRecordsBattleBE battle_records;
    /* 157C:1160 */ parray<uint8_t, 4> unknown_a10;
    /* 1580:1164 */ PlayerRecordsChallengeV3BE::Stats challenge_record_stats;
    /* 1658:123C */ Episode3::PlayerConfig ep3_config;
    /* 39A8:358C */ be_uint32_t unknown_a11 = 0;
    /* 39AC:3590 */ be_uint32_t unknown_a12 = 0;
    /* 39B0:3594 */ be_uint32_t unknown_a13 = 0;
    /* 39B4:3598 */

    Character() = default;
    explicit Character(const PSOGCEp3NTECharacter& nte);
    operator PSOGCEp3NTECharacter() const;
  } __packed_ws__(Character, 0x39B4);
  /* 00004 */ parray<Character, 7> characters;
  /* 193F0 */ pstring<TextEncoding::ASCII, 0x10> serial_number; // As %08X (not decimal)
  /* 19400 */ pstring<TextEncoding::ASCII, 0x10> access_key; // As 12 ASCII characters (decimal)
  /* 19410 */ pstring<TextEncoding::ASCII, 0x10> password;
  // In Episode 3, this field still exists, but is unused since BGM test was
  // removed from the options menu in favor of the jukebox. The jukebox is
  // accessible online only, and which songs are available there is controlled
  // by the B7 command sent by the server instead.
  /* 19420 */ be_uint64_t bgm_test_songs_unlocked = 0;
  /* 19428 */ be_uint32_t save_count = 1;
  // This is an array of 999 bits, represented here as 128 bytes (the last 25
  // bits are not used). Each bit corresponds to a card ID with the bit's index;
  // if the bit is set, then during offline play, the card's rank is replaced
  // with D2 if its original rank is S, SS, E, or D2, or with D1 if the original
  // rank is any other value. Upon receiving a B8 command (server card
  // definitions), the game clears this array, and sets all bits whose
  // corresponding cards from the server have the D1 or D2 ranks. This could
  // have been used by Sega to prevent broken cards from being used offline, but
  // there's no indication that they ever used this functionality.
  /* 1942C */ parray<uint8_t, 0x80> card_rank_override_flags;
  /* 194AC */ be_uint32_t round2_seed = 0;
  /* 194B0 */
} __packed_ws__(PSOGCEp3CharacterFile, 0x194B0);

struct PSOXBCharacterFileCharacter {
  // This structure is internally split into two by the game. The offsets here
  // are relative to the start of this structure (first column), and relative
  // to the start of the second internal structure (second column).
  // Most fields have the same meanings as in PSOGCCharacterFile::Character.
  /* 0000:---- */ PlayerInventory inventory;
  /* 034C:---- */ PlayerDispDataDCPCV3 disp;
  /* 041C:0000 */ le_uint32_t validation_flags = 0;
  /* 0420:0004 */ le_uint32_t creation_timestamp = 0;
  /* 0424:0008 */ le_uint32_t signature = 0xC87ED5B1;
  /* 0428:000C */ le_uint32_t play_time_seconds = 0;
  /* 042C:0010 */ le_uint32_t option_flags = 0x00040058;
  /* 0430:0014 */ le_uint32_t save_count = 1;
  /* 0434:0018 */ pstring<TextEncoding::ASCII, 0x1C> ppp_username;
  /* 0450:0034 */ pstring<TextEncoding::ASCII, 0x10> ppp_password;
  /* 0460:0044 */ QuestFlags quest_flags;
  /* 0660:0244 */ le_uint32_t death_count = 0;
  /* 0664:0248 */ PlayerBank200 bank;
  /* 192C:1510 */ GuildCardXB guild_card;
  /* 1B58:173C */ parray<SaveFileSymbolChatEntryDCXB, 12> symbol_chats;
  /* 1F78:1B5C */ parray<SaveFileShortcutEntryXB, 16> shortcuts;
  /* 24B8:209C */ pstring<TextEncoding::MARKED, 0xAC> auto_reply;
  /* 2518:20FC */ pstring<TextEncoding::MARKED, 0xAC> info_board;
  // TODO: The following fields are guesses and have not been verified.
  /* 2610:21F4 */ PlayerRecordsBattle battle_records;
  /* 2628:220C */ parray<uint8_t, 4> unknown_a4;
  /* 262C:2210 */ PlayerRecordsChallengeV3 challenge_records;
  /* 272C:2310 */ parray<le_uint16_t, 20> tech_menu_shortcut_entries;
  /* 2754:2338 */ ChoiceSearchConfig choice_search_config;
  /* 276C:2350 */ parray<uint8_t, 0x10> unknown_a6;
  /* 277C:2360 */ parray<le_uint32_t, 0x10> quest_counters;
  /* 27BC:23A0 */ PlayerRecordsBattle offline_battle_records;
  /* 27D4:23B8 */ parray<uint8_t, 4> unknown_a7;
  struct UnknownA8Entry {
    /* 00 */ le_uint32_t unknown_a1 = 0;
    /* 04 */ parray<uint8_t, 0x1C> unknown_a2;
    /* 20 */ parray<le_float, 4> unknown_a3;
    /* 30 */
  } __packed_ws__(UnknownA8Entry, 0x30);
  /* 27D8:23BC */ parray<UnknownA8Entry, 5> unknown_a8;
  /* 28C8:24AC */
} __packed_ws__(PSOXBCharacterFileCharacter, 0x28C8);

struct PSOBBCharacterFile {
  // Most fields have the same meanings as in PSOGCCharacterFile::Character.
  // This is the character data used by the server for all game versions, and
  // is also the format used in .psochar files.

  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataBB disp;
  /* 04DC */ le_uint32_t validation_flags = 0;
  /* 04E0 */ le_uint32_t creation_timestamp = 0;
  /* 04E4 */ le_uint32_t signature = 0xC87ED5B1;
  /* 04E8 */ le_uint32_t play_time_seconds = 0;
  /* 04EC */ le_uint32_t option_flags = 0x00040058;
  /* 04F0 */ le_uint32_t save_count = 1;
  /* 04F4 */ QuestFlags quest_flags;
  /* 06F4 */ le_uint32_t death_count = 0;
  /* 06F8 */ PlayerBank200 bank;
  /* 19C0 */ GuildCardBB guild_card;
  /* 1AC8 */ le_uint32_t unknown_a3 = 0;
  /* 1ACC */ parray<SaveFileSymbolChatEntryBB, 0x0C> symbol_chats;
  /* 1FAC */ parray<SaveFileShortcutEntryBB, 0x10> shortcuts;
  /* 29EC */ pstring<TextEncoding::UTF16, 0x00AC> auto_reply;
  /* 2B44 */ pstring<TextEncoding::UTF16, 0x00AC> info_board;
  /* 2C9C */ PlayerRecordsBattle battle_records;
  /* 2CB4 */ parray<uint8_t, 4> unknown_a4;
  /* 2CB8 */ PlayerRecordsChallengeBB challenge_records;
  /* 2DF8 */ parray<le_uint16_t, 0x0014> tech_menu_shortcut_entries;
  /* 2E20 */ ChoiceSearchConfig choice_search_config;
  /* 2E38 */ parray<uint8_t, 0x0010> unknown_a6;
  /* 2E48 */ parray<le_uint32_t, 0x0010> quest_counters;
  /* 2E88 */ PlayerRecordsBattle offline_battle_records;
  /* 2EA0 */ parray<uint8_t, 4> unknown_a7;
  /* 2EA4 */

  PSOBBCharacterFile() = default;

  PlayerDispDataBBPreview to_preview() const;

  static std::shared_ptr<PSOBBCharacterFile> create_from_config(
      uint32_t guild_card_number,
      uint8_t language,
      const PlayerVisualConfig& visual,
      const std::string& name,
      std::shared_ptr<const LevelTable> level_table);
  static std::shared_ptr<PSOBBCharacterFile> create_from_preview(
      uint32_t guild_card_number,
      uint8_t language,
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSODCNTECharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSODC112000CharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSODCV1CharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSODCV2CharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSOGCNTECharacterFileCharacter& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSOGCCharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSOGCEp3CharacterFile::Character& src);
  static std::shared_ptr<PSOBBCharacterFile> create_from_file(const PSOXBCharacterFileCharacter& src);

  operator PSODCNTECharacterFile::Character() const;
  operator PSODC112000CharacterFile::Character() const;
  operator PSODCV1CharacterFile::Character() const;
  operator PSODCV2CharacterFile::Character() const;
  operator PSOGCNTECharacterFileCharacter() const;
  operator PSOGCCharacterFile::Character() const;
  operator PSOGCEp3CharacterFile::Character() const;
  operator PSOXBCharacterFileCharacter() const;

  void add_item(const ItemData& item, const ItemData::StackLimits& limits);
  ItemData remove_item(uint32_t item_id, uint32_t amount, const ItemData::StackLimits& limits);
  void add_meseta(uint32_t amount);
  void remove_meseta(uint32_t amount, bool allow_overdraft);

  uint8_t get_technique_level(uint8_t which) const; // Returns FF or 00-1D
  void set_technique_level(uint8_t which, uint8_t level);

  enum class MaterialType : int8_t {
    HP = -2,
    TP = -1,
    POWER = 0,
    MIND = 1,
    EVADE = 2,
    DEF = 3,
    LUCK = 4,
  };

  uint8_t get_material_usage(MaterialType which) const;
  void set_material_usage(MaterialType which, uint8_t usage);
  void clear_all_material_usage();
  void import_tethealla_material_usage(std::shared_ptr<const LevelTable> level_table);
  void recompute_stats(std::shared_ptr<const LevelTable> level_table);
} __packed_ws__(PSOBBCharacterFile, 0x2EA4);

struct LoadedPSOCHARFile {
  std::shared_ptr<PSOBBBaseSystemFile> system_file; // Null if load_system is false
  std::shared_ptr<PSOBBCharacterFile> character_file; // Never null
  // Team membership is present in the file, but ignored by newserv
};

LoadedPSOCHARFile load_psochar(const std::string& filename, bool load_system);
void save_psochar(
    const std::string& filename,
    std::shared_ptr<const PSOBBBaseSystemFile> system,
    std::shared_ptr<const PSOBBCharacterFile> character);

////////////////////////////////////////////////////////////////////////////////
// Guild Card files

struct PSODCNTEGuildCardFile {
  // Note: DC NTE does not encrypt the Guild Card file
  struct Entry {
    /* 0000 */ GuildCardDCNTE guild_card;
    /* 007B */ uint8_t unknown_a1 = 0; // Probably actually unused
    /* 007C */
  } __packed_ws__(Entry, 0x7C);
  /* 0000 */ parray<Entry, 100> entries;
  /* 3070 */
} __packed_ws__(PSODCNTEGuildCardFile, 0x3070);

struct PSODCGuildCardFileEntry {
  /* 0000 */ GuildCardDC guild_card;
  /* 007D */ parray<uint8_t, 3> unknown_a1 = 0; // Probably actually unused
  /* 0080 */
} __packed_ws__(PSODCGuildCardFileEntry, 0x80);

struct PSODC112000GuildCardFile {
  // Note: 11/2000 does not encrypt the Guild Card file
  /* 0000 */ parray<PSODCGuildCardFileEntry, 100> entries;
  /* 3200 */
} __packed_ws__(PSODC112000GuildCardFile, 0x3200);

struct PSODCV1V2GuildCardFile {
  struct EncryptedSection {
    /* 0000 */ le_uint32_t checksum = 0;
    /* 0004 */ parray<PSODCGuildCardFileEntry, 100> entries;
    /* 3204 */ le_int16_t music_volume = 0;
    /* 3206 */ int8_t sound_volume = 0;
    /* 3207 */ uint8_t language = 1;
    /* 3208 */ le_int32_t server_time_delta_frames = 540000; // 648000 on DCv1
    /* 320C */ le_uint32_t creation_timestamp = 0;
    /* 3210 */ le_uint32_t round2_seed = 0;
    /* 3214 */
  } __packed_ws__(EncryptedSection, 0x3214);
  /* 0000 */ EncryptedSection encrypted_section;
  /* 3214 */ parray<uint8_t, 0x100> event_flags;
  /* 3314 */
} __packed_ws__(PSODCV1V2GuildCardFile, 0x3314);

struct PSOPCGuildCardFile { // PSO______GUD
  /* 0000 */ le_uint32_t checksum = 0;
  // TODO: Figure out the PC guild card format.
  /* 0004 */ parray<uint8_t, 0x7980> unknown_a1;
  /* 7984 */ le_uint32_t creation_timestamp = 0;
  /* 7988 */ le_uint32_t round2_seed = 0;
  /* 798C */ parray<uint8_t, 0x74> end_padding;
  /* 7A00 */
} __packed_ws__(PSOPCGuildCardFile, 0x7A00);

struct PSOGCGuildCardFile {
  /* 0000 */ be_uint32_t checksum = 0;
  /* 0004 */ parray<uint8_t, 0xC0> unknown_a1;
  struct GuildCardEntry {
    /* 0000 */ GuildCardGCBE base;
    /* 0090 */ uint8_t unknown_a1 = 0;
    /* 0091 */ uint8_t unknown_a2 = 0;
    /* 0092 */ uint8_t unknown_a3 = 0;
    /* 0093 */ uint8_t unknown_a4 = 0;
    /* 0094 */ pstring<TextEncoding::MARKED, 0x6C> comment;
    /* 0100 */
  } __packed_ws__(GuildCardEntry, 0x100);
  /* 00C4 */ parray<GuildCardEntry, 0xD2> entries;
  /* D2C4 */ parray<GuildCardGCBE, 0x1C> blocked_senders;
  /* E284 */ be_uint32_t creation_timestamp = 0;
  /* E288 */ be_uint32_t round2_seed = 0;
  /* E28C */
} __packed_ws__(PSOGCGuildCardFile, 0xE28C);

struct PSOBBGuildCardFile {
  struct Entry {
    /* 0000 */ GuildCardBB data;
    /* 0108 */ pstring<TextEncoding::UTF16, 0x58> comment;
    /* 01B8 */ parray<uint8_t, 0x4> unknown_a1;
    /* 01BC */

    void clear();
  } __packed_ws__(Entry, 0x1BC);

  /* 0000 */ PSOBBMinimalSystemFile system_file;
  /* 0114 */ parray<GuildCardBB, 0x1C> blocked;
  /* 1DF4 */ parray<uint8_t, 0x180> unknown_a2;
  /* 1F74 */ parray<Entry, 0x69> entries;
  /* D590 */

  PSOBBGuildCardFile() = default;

  uint32_t checksum() const;
} __packed_ws__(PSOBBGuildCardFile, 0xD590);

////////////////////////////////////////////////////////////////////////////////
// Snapshot files

struct PSOGCSnapshotFile {
  /* 00000 */ be_uint32_t checksum = 0;
  /* 00004 */ be_uint16_t width = 0x100;
  /* 00006 */ be_uint16_t height = 0xC0;
  // Pixels are stored as 4x4 blocks of RGB565 values. See the implementation
  // of decode_image for details.
  /* 00008 */ parray<be_uint16_t, 0xC000> pixels;
  /* 18008 */ uint8_t unknown_a1 = 0x18; // Always 0x18?
  /* 18009 */ uint8_t unknown_a2 = 0;
  /* 1800A */ be_int16_t max_players = 0;
  /* 1800C */ parray<be_uint32_t, 12> players_present;
  /* 1803C */ parray<be_uint32_t, 12> player_levels;
  /* 1806C */ parray<pstring<TextEncoding::ASCII, 0x18>, 12> player_names;
  /* 1818C */

  bool checksum_correct() const;
  phosg::Image decode_image() const;
} __packed_ws__(PSOGCSnapshotFile, 0x1818C);

////////////////////////////////////////////////////////////////////////////////
// Obsolete newserv-specific formats (for backward compatibility only)

struct LegacySavedPlayerDataBB { // .nsc file format
  static constexpr uint64_t SIGNATURE_V0 = 0x6E65777365727620;
  static constexpr uint64_t SIGNATURE_V1 = 0xA904332D5CEF0296;

  /* 0000 */ be_uint64_t signature = SIGNATURE_V1;
  /* 0008 */ parray<uint8_t, 0x20> unused;
  /* 0028 */ PlayerRecordsBattle battle_records;
  /* 0040 */ PlayerDispDataBBPreview preview;
  /* 00BC */ pstring<TextEncoding::UTF16, 0x00AC> auto_reply;
  /* 0214 */ PlayerBank200 bank;
  /* 14DC */ PlayerRecordsChallengeBB challenge_records;
  /* 161C */ PlayerDispDataBB disp;
  /* 17AC */ pstring<TextEncoding::UTF16, 0x0058> guild_card_description;
  /* 185C */ pstring<TextEncoding::UTF16, 0x00AC> info_board;
  /* 19B4 */ PlayerInventory inventory;
  /* 1D00 */ parray<uint8_t, 4> unknown_a2;
  /* 1D04 */ QuestFlags quest_flags;
  /* 1F04 */ le_uint32_t death_count = 0;
  /* 1F08 */ parray<le_uint32_t, 0x0016> quest_counters;
  /* 1F60 */ parray<le_uint16_t, 0x0014> tech_menu_shortcut_entries;
  /* 1F88 */
} __packed_ws__(LegacySavedPlayerDataBB, 0x1F88);

struct LegacySavedAccountDataBB { // .nsa file format
  static const char* SIGNATURE;

  /* 0000 */ pstring<TextEncoding::ASCII, 0x40> signature;
  /* 0040 */ parray<le_uint32_t, 0x001E> blocked_senders;
  /* 00B8 */ PSOBBGuildCardFile guild_card_file;
  /* D648 */ PSOBBBaseSystemFile system_file;
  /* D880 */ PSOBBTeamMembership team_membership;
  /* E138 */ le_uint32_t unused = 0;
  /* E13C */ le_uint32_t option_flags = 0x00040058;
  /* E140 */ parray<SaveFileShortcutEntryBB, 0x10> shortcuts;
  /* EB80 */ parray<SaveFileSymbolChatEntryBB, 0x0C> symbol_chats;
  /* F060 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
  /* F080 */
} __packed_ws__(LegacySavedAccountDataBB, 0xF080);

////////////////////////////////////////////////////////////////////////////////
// Encoding/decoding functions

template <bool BE>
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
  round1_crypt.encrypt_minus_t<BE>(decrypted.data(), decrypted.size());

  decrypted.resize(orig_size);
  return decrypted;
}

template <bool BE>
std::string encrypt_data_section(const void* data_section, size_t size, uint32_t round1_seed) {
  std::string encrypted(reinterpret_cast<const char*>(data_section), size);
  encrypted.resize((encrypted.size() + 3) & (~3));

  PSOV2Encryption crypt(round1_seed);
  crypt.encrypt_minus_t<BE>(encrypted.data(), encrypted.size());

  std::string ret(size, '\0');
  PSOV2Encryption shuf_crypt(round1_seed);
  ShuffleTables shuf(shuf_crypt);
  shuf.shuffle(ret.data(), encrypted.data(), size, false);

  return ret;
}

template <bool BE>
std::string decrypt_fixed_size_data_section_s(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    bool skip_checksum = false,
    int64_t override_round2_seed = -1) {
  if (size < 2 * sizeof(U32T<BE>)) {
    throw std::runtime_error("data size is too small");
  }
  std::string decrypted = decrypt_data_section<BE>(data_section, size, round1_seed);

  uint32_t round2_seed = (override_round2_seed >= 0)
      ? override_round2_seed
      : reinterpret_cast<const U32T<BE>*>(decrypted.data() + decrypted.size() - sizeof(U32T<BE>))->load();
  PSOV2Encryption round2_crypt(round2_seed);
  if (BE) {
    round2_crypt.encrypt_big_endian(decrypted.data(), decrypted.size() - sizeof(U32T<BE>));
  } else {
    round2_crypt.encrypt(decrypted.data(), decrypted.size() - sizeof(U32T<BE>));
  }

  if (!skip_checksum) {
    U32T<BE>& checksum = *reinterpret_cast<U32T<BE>*>(decrypted.data());
    uint32_t expected_crc = checksum;
    checksum = 0;
    uint32_t actual_crc = phosg::crc32(decrypted.data(), decrypted.size());
    checksum = expected_crc;
    if (expected_crc != actual_crc) {
      throw std::runtime_error(phosg::string_printf(
          "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
          expected_crc, actual_crc));
    }
  }

  return decrypted;
}

template <typename StructT, bool BE, size_t ChecksumLength = sizeof(StructT)>
StructT decrypt_fixed_size_data_section_t(
    const void* data_section,
    size_t size,
    uint32_t round1_seed,
    bool skip_checksum = false,
    int64_t override_round2_seed = -1) {

  std::string decrypted = decrypt_data_section<BE>(data_section, size, round1_seed);
  if (decrypted.size() < sizeof(StructT)) {
    throw std::runtime_error("file too small for structure");
  }
  StructT ret = *reinterpret_cast<const StructT*>(decrypted.data());

  PSOV2Encryption round2_crypt((override_round2_seed >= 0) ? override_round2_seed : ret.round2_seed.load());
  if (BE) {
    round2_crypt.encrypt_big_endian(&ret, offsetof(StructT, round2_seed));
  } else {
    round2_crypt.encrypt(&ret, offsetof(StructT, round2_seed));
  }

  if (!skip_checksum) {
    uint32_t expected_crc = ret.checksum;
    ret.checksum = 0;
    uint32_t actual_crc = phosg::crc32(&ret, ChecksumLength);
    ret.checksum = expected_crc;
    if (expected_crc != actual_crc) {
      throw std::runtime_error(phosg::string_printf(
          "incorrect decrypted data section checksum: expected %08" PRIX32 "; received %08" PRIX32,
          expected_crc, actual_crc));
    }
  }

  return ret;
}

template <bool BE>
std::string encrypt_fixed_size_data_section_s(const void* data, size_t size, uint32_t round1_seed) {
  if (size < 2 * sizeof(U32T<BE>)) {
    throw std::runtime_error("data size is too small");
  }

  uint32_t round2_seed = phosg::random_object<uint32_t>();

  std::string encrypted(reinterpret_cast<const char*>(data), size);
  *reinterpret_cast<U32T<BE>*>(encrypted.data()) = 0;
  *reinterpret_cast<U32T<BE>*>(encrypted.data() + encrypted.size() - sizeof(U32T<BE>)) = round2_seed;
  *reinterpret_cast<U32T<BE>*>(encrypted.data()) = phosg::crc32(encrypted.data(), encrypted.size());

  PSOV2Encryption round2_crypt(round2_seed);
  if (BE) {
    round2_crypt.encrypt_big_endian(encrypted.data(), encrypted.size());
  } else {
    round2_crypt.encrypt(encrypted.data(), encrypted.size());
  }

  return encrypt_data_section<BE>(encrypted.data(), encrypted.size(), round1_seed);
}

template <typename StructT, bool BE, size_t ChecksumLength = sizeof(StructT)>
std::string encrypt_fixed_size_data_section_t(const StructT& s, uint32_t round1_seed) {
  StructT encrypted = s;
  encrypted.checksum = 0;
  encrypted.round2_seed = phosg::random_object<uint32_t>();
  encrypted.checksum = phosg::crc32(&encrypted, ChecksumLength);

  PSOV2Encryption round2_crypt(encrypted.round2_seed);
  if (BE) {
    round2_crypt.encrypt_big_endian(&encrypted, offsetof(StructT, round2_seed));
  } else {
    round2_crypt.encrypt(&encrypted, offsetof(StructT, round2_seed));
  }

  return encrypt_data_section<BE>(&encrypted, sizeof(StructT), round1_seed);
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

std::string encode_psobb_hangame_credentials(
    const std::string& user_id, const std::string& token, const std::string& unused = "");
