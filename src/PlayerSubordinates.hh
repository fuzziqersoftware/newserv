#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <array>
#include <phosg/Encoding.hh>
#include <string>
#include <utility>
#include <vector>

#include "FileContentsCache.hh"
#include "ItemData.hh"
#include "LevelTable.hh"
#include "Text.hh"
#include "Version.hh"

extern FileContentsCache player_files_cache;

// PSO V2 stored some extra data in the character structs in a format that I'm
// sure Sega thought was very clever for backward compatibility, but for us is
// just plain annoying. Specifically, they used the third and fourth bytes of
// the InventoryItem struct to store some things not present in V1. The game
// stores arrays of bytes striped across these structures. In newserv, we call
// those fields extension_data. They contain:
//   items[0].extension_data1 through items[19].extension_data1:
//       Extended technique levels. The values in the technique_levels_v1 array
//       only go up to 14 (tech level 15); if the player has a technique above
//       level 15, the corresponding extension_data1 field holds the remaining
//       levels (so a level 20 tech would have 14 in technique_levels_v1 and 5
//       in the corresponding item's extension_data1 field).
//   items[0].extension_data2 through items[3].extension_data2:
//       The value known as unknown_a1 in the PSOGCCharacterFile::Character
//       struct. See SaveFileFormats.hh.
//   items[4].extension_data2 through items[7].extension_data2:
//       The timestamp when the character was last saved, in seconds since
//       January 1, 2000. Stored little-endian, so items[4] contains the LSB.
//   items[8].extension_data2 through items[12].extension_data2:
//       Number of power materials, mind materials, evade materials, def
//       materials, and luck materials (respectively) used by the player.
//   items[13].extension_data2 through items[15].extension_data2:
//       Unknown. These are not an array, but do appear to be related.

struct PlayerInventoryItem {
  /* 00 */ le_uint16_t present = 0;
  // See note above about these fields
  /* 02 */ uint8_t extension_data1 = 0;
  /* 03 */ uint8_t extension_data2 = 0;
  /* 04 */ le_uint32_t flags = 0; // 8 = equipped
  /* 08 */ ItemData data;
  /* 1C */
} __attribute__((packed));

struct PlayerBankItem {
  /* 00 */ ItemData data;
  /* 14 */ le_uint16_t amount = 0;
  /* 16 */ le_uint16_t present = 0;
  /* 18 */
} __attribute__((packed));

struct PlayerInventory {
  /* 0000 */ uint8_t num_items = 0;
  /* 0001 */ uint8_t hp_materials_used = 0;
  /* 0002 */ uint8_t tp_materials_used = 0;
  /* 0003 */ uint8_t language = 1; // English
  /* 0004 */ parray<PlayerInventoryItem, 30> items;
  /* 034C */

  PlayerInventory();

  size_t find_item(uint32_t item_id) const;

  size_t find_equipped_weapon() const;
  size_t find_equipped_armor() const;
  size_t find_equipped_mag() const;
} __attribute__((packed));

struct PlayerBank {
  /* 0000 */ le_uint32_t num_items = 0;
  /* 0004 */ le_uint32_t meseta = 0;
  /* 0008 */ parray<PlayerBankItem, 200> items;
  /* 12C8 */

  void load(const std::string& filename);
  void save(const std::string& filename, bool save_to_filesystem) const;

  bool switch_with_file(const std::string& save_filename,
      const std::string& load_filename);

  void add_item(const ItemData& item);
  ItemData remove_item(uint32_t item_id, uint32_t amount);
  size_t find_item(uint32_t item_id);
} __attribute__((packed));

struct PlayerDispDataBB;

struct PlayerStats {
  /* 00 */ CharacterStats char_stats;
  /* 0E */ le_uint16_t unknown_a1 = 0;
  /* 10 */ le_float unknown_a2 = 0.0;
  /* 14 */ le_float unknown_a3 = 0.0;
  /* 18 */ le_uint32_t level = 0;
  /* 1C */ le_uint32_t experience = 0;
  /* 20 */ le_uint32_t meseta = 0;
  /* 24 */
} __attribute__((packed));

struct PlayerVisualConfig {
  /* 00 */ ptext<char, 0x10> name;
  /* 10 */ le_uint64_t unknown_a2 = 0; // Note: This is probably not actually a 64-bit int.
  /* 18 */ le_uint32_t name_color = 0x00000000; // RGBA
  /* 1C */ uint8_t extra_model = 0;
  /* 1D */ parray<uint8_t, 0x0F> unused;
  /* 2C */ le_uint32_t unknown_a3 = 0;
  /* 30 */ uint8_t section_id = 0;
  /* 31 */ uint8_t char_class = 0;
  /* 32 */ uint8_t v2_flags = 0;
  /* 33 */ uint8_t version = 0;
  /* 34 */ le_uint32_t v1_flags = 0;
  /* 38 */ le_uint16_t costume = 0;
  /* 3A */ le_uint16_t skin = 0;
  /* 3C */ le_uint16_t face = 0;
  /* 3E */ le_uint16_t head = 0;
  /* 40 */ le_uint16_t hair = 0;
  /* 42 */ le_uint16_t hair_r = 0;
  /* 44 */ le_uint16_t hair_g = 0;
  /* 46 */ le_uint16_t hair_b = 0;
  /* 48 */ le_float proportion_x = 0.0;
  /* 4C */ le_float proportion_y = 0.0;
  /* 50 */
} __attribute__((packed));

struct PlayerDispDataDCPCV3 {
  /* 00 */ PlayerStats stats;
  /* 24 */ PlayerVisualConfig visual;
  /* 74 */ parray<uint8_t, 0x48> config;
  /* BC */ parray<uint8_t, 0x14> technique_levels_v1;
  /* D0 */
  void enforce_lobby_join_limits(GameVersion target_version);
  PlayerDispDataBB to_bb() const;
} __attribute__((packed));

struct PlayerDispDataBBPreview {
  /* 00 */ le_uint32_t experience = 0;
  /* 04 */ le_uint32_t level = 0;
  // The name field in this structure is used for the player's Guild Card
  // number, apparently (possibly because it's a char array and this is BB)
  /* 08 */ PlayerVisualConfig visual;
  /* 58 */ ptext<char16_t, 0x10> name;
  /* 78 */ uint32_t play_time = 0;
  /* 7C */
} __attribute__((packed));

// BB player appearance and stats data
struct PlayerDispDataBB {
  /* 0000 */ PlayerStats stats;
  /* 0024 */ PlayerVisualConfig visual;
  /* 0074 */ ptext<char16_t, 0x0C> name;
  /* 008C */ le_uint32_t play_time = 0;
  /* 0090 */ uint32_t unknown_a3 = 0;
  /* 0094 */ parray<uint8_t, 0xE8> config;
  /* 017C */ parray<uint8_t, 0x14> technique_levels_v1;
  /* 0190 */

  void enforce_lobby_join_limits(GameVersion target_version);
  PlayerDispDataDCPCV3 to_dcpcv3() const;
  PlayerDispDataBBPreview to_preview() const;
  void apply_preview(const PlayerDispDataBBPreview&);
  void apply_dressing_room(const PlayerDispDataBBPreview&);
} __attribute__((packed));

struct GuildCardPC {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  // TODO: Is the length of the name field correct here?
  /* 08 */ ptext<char16_t, 0x18> name;
  /* 38 */ ptext<char16_t, 0x5A> description;
  /* EC */ uint8_t present = 0;
  /* ED */ uint8_t language = 0;
  /* EE */ uint8_t section_id = 0;
  /* EF */ uint8_t char_class = 0;
  /* F0 */
} __attribute__((packed));

// TODO: Is this the same for XB as it is for GC? (This struct is based on the
// GC format)
struct GuildCardV3 {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ ptext<char, 0x18> name;
  /* 20 */ ptext<char, 0x6C> description;
  /* 8C */ uint8_t present = 0;
  /* 8D */ uint8_t language = 0;
  /* 8E */ uint8_t section_id = 0;
  /* 8F */ uint8_t char_class = 0;
  /* 90 */
} __attribute__((packed));

// BB guild card format
struct GuildCardBB {
  /* 0000 */ le_uint32_t guild_card_number = 0;
  /* 0004 */ ptext<char16_t, 0x18> name;
  /* 0034 */ ptext<char16_t, 0x10> team_name;
  /* 0054 */ ptext<char16_t, 0x58> description;
  /* 0104 */ uint8_t present = 0;
  /* 0105 */ uint8_t language = 0;
  /* 0106 */ uint8_t section_id = 0;
  /* 0107 */ uint8_t char_class = 0;
  /* 0108 */

  void clear();
} __attribute__((packed));

// an entry in the BB guild card file
struct GuildCardEntryBB {
  GuildCardBB data;
  ptext<char16_t, 0x58> comment;
  parray<uint8_t, 0x4> unknown_a1;

  void clear();
} __attribute__((packed));

// the format of the BB guild card file
struct GuildCardFileBB {
  parray<uint8_t, 0x114> unknown_a1;
  GuildCardBB blocked[0x1C];
  parray<uint8_t, 0x180> unknown_a2;
  GuildCardEntryBB entries[0x69];

  uint32_t checksum() const;
} __attribute__((packed));

struct KeyAndTeamConfigBB {
  parray<uint8_t, 0x0114> unknown_a1; // 0000
  parray<uint8_t, 0x016C> key_config; // 0114
  parray<uint8_t, 0x0038> joystick_config; // 0280
  le_uint32_t guild_card_number = 0; // 02B8
  le_uint32_t team_id = 0; // 02BC
  le_uint64_t team_info = 0; // 02C0
  le_uint16_t team_privilege_level = 0; // 02C8
  le_uint16_t reserved = 0; // 02CA
  ptext<char16_t, 0x0010> team_name; // 02CC
  parray<uint8_t, 0x0800> team_flag; // 02EC
  le_uint32_t team_rewards = 0; // 0AEC
} __attribute__((packed));

struct PlayerLobbyDataPC {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card = 0;
  // There's a strange behavior (bug? "feature"?) in Episode 3 where the start
  // button does nothing in the lobby (hence you can't "quit game") if the
  // client's IP address is zero. So, we fill it in with a fake nonzero value to
  // avoid this behavior, and to be consistent, we make IP addresses fake and
  // nonzero on all other versions too.
  be_uint32_t ip_address = 0x7F000001;
  le_uint32_t client_id = 0;
  ptext<char16_t, 0x10> name;

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataDCGC {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card = 0;
  be_uint32_t ip_address = 0x7F000001;
  le_uint32_t client_id = 0;
  ptext<char, 0x10> name;

  void clear();
} __attribute__((packed));

struct XBNetworkLocation {
  le_uint32_t internal_ipv4_address = 0x0A0A0A0A;
  le_uint32_t external_ipv4_address = 0x23232323;
  le_uint16_t port = 9100;
  parray<uint8_t, 6> mac_address = 0x77;
  parray<le_uint32_t, 2> unknown_a1;
  le_uint64_t account_id = 0xFFFFFFFFFFFFFFFF;
  parray<le_uint32_t, 4> unknown_a2;

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataXB {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card = 0;
  XBNetworkLocation netloc;
  le_uint32_t client_id = 0;
  ptext<char, 0x10> name;

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataBB {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card = 0;
  // This field is a guess; the official builds didn't use this, but all other
  // versions have it
  be_uint32_t ip_address = 0x7F000001;
  parray<uint8_t, 0x10> unknown_a1;
  le_uint32_t client_id = 0;
  ptext<char16_t, 0x10> name;
  le_uint32_t unknown_a2 = 0;

  void clear();
} __attribute__((packed));

template <bool IsWideChar>
struct PlayerRecordsDCPC_Challenge {
  using CharT = typename std::conditional<IsWideChar, char16_t, char>::type;

  /* 00 */ le_uint16_t title_color = 0x7FFF;
  /* 02 */ parray<uint8_t, 2> unknown_u0;
  /* 04 */ ptext<CharT, 0x0C> rank_title; // Encrypted; see decrypt_challenge_rank_text
  /* 10 */ parray<le_uint32_t, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time. TODO: This might be offline times
  /* 34 */ le_uint16_t unknown_g3 = 0;
  /* 36 */ le_uint16_t grave_deaths = 0;
  /* 38 */ parray<le_uint32_t, 5> grave_coords_time;
  /* 4C */ ptext<CharT, 0x14> grave_team;
  /* 60 */ ptext<CharT, 0x18> grave_message;
  /* 78 */ parray<le_uint32_t, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time. TODO: This might be online times
  /* 9C */ parray<uint8_t, 4> unknown_l4;
  /* A0 */
} __attribute__((packed));

struct PlayerRecordsDC_Challenge : PlayerRecordsDCPC_Challenge<false> {
} __attribute__((packed));

struct PlayerRecordsPC_Challenge : PlayerRecordsDCPC_Challenge<true> {
} __attribute__((packed));

template <bool IsBigEndian>
struct PlayerRecordsV3_Challenge {
  using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  // Offsets are (1) relative to start of C5 entry, and (2) relative to start
  // of save file structure
  struct Stats {
    /* 00:1C */ U16T title_color = 0x7FFF; // XRGB1555
    /* 02:1E */ parray<uint8_t, 2> unknown_u0;
    /* 04:20 */ parray<U32T, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time
    /* 28:44 */ parray<U32T, 5> times_ep2_online; // Encrypted; see decrypt_challenge_time
    /* 3C:58 */ parray<U32T, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time
    /* 60:7C */ parray<uint8_t, 4> unknown_g3;
    /* 64:80 */ U16T grave_deaths = 0;
    /* 66:82 */ parray<uint8_t, 2> unknown_u4;
    /* 68:84 */ parray<U32T, 5> grave_coords_time;
    /* 7C:98 */ ptext<char, 0x14> grave_team;
    /* 90:AC */ ptext<char, 0x20> grave_message;
    /* B0:CC */ parray<uint8_t, 4> unknown_m5;
    /* B4:D0 */ parray<U32T, 9> unknown_t6;
    /* D8:F4 */
  } __attribute__((packed));
  /* 0000:001C */ Stats stats;
  // On Episode 3, there are special cases that apply to this field - if the
  // text ends with certain strings (after decrypt_challenge_rank_text), the
  // player will have particle effects emanate from their character in the
  // lobby every 2 seconds. These effects are:
  //   Ends with ":GOD" => blue circle
  //   Ends with ":KING" => white particles
  //   Ends with ":LORD" => rising yellow sparkles
  //   Ends with ":CHAMP" => green circle
  /* 00D8:00F4 */ ptext<char, 0x0C> rank_title;
  /* 00E4:0100 */ parray<uint8_t, 0x1C> unknown_l7;
  /* 0100:011C */
} __attribute__((packed));

struct PlayerRecordsBB_Challenge {
  /* 0000 */ le_uint16_t title_color = 0x7FFF; // XRGB1555
  /* 0002 */ parray<uint8_t, 2> unknown_u0;
  /* 0004 */ parray<le_uint32_t, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time
  /* 0028 */ parray<le_uint32_t, 5> times_ep2_online; // Encrypted; see decrypt_challenge_time
  /* 003C */ parray<le_uint32_t, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time
  /* 0060 */ parray<uint8_t, 4> unknown_g3;
  /* 0064 */ le_uint16_t grave_deaths = 0;
  /* 0066 */ parray<uint8_t, 2> unknown_u4;
  /* 0068 */ parray<le_uint32_t, 5> grave_coords_time;
  /* 007C */ ptext<char16_t, 0x14> grave_team;
  /* 00A4 */ ptext<char16_t, 0x20> grave_message;
  /* 00E4 */ parray<uint8_t, 4> unknown_m5;
  /* 00E8 */ parray<le_uint32_t, 9> unknown_t6;
  /* 010C */ ptext<char16_t, 0x0C> rank_title; // Encrypted; see decrypt_challenge_rank_text
  /* 0124 */ parray<uint8_t, 0x1C> unknown_l7;
  /* 0140 */

  PlayerRecordsBB_Challenge() = default;
  PlayerRecordsBB_Challenge(const PlayerRecordsBB_Challenge& other) = default;
  PlayerRecordsBB_Challenge& operator=(const PlayerRecordsBB_Challenge& other) = default;

  PlayerRecordsBB_Challenge(const PlayerRecordsDC_Challenge& rec);
  PlayerRecordsBB_Challenge(const PlayerRecordsPC_Challenge& rec);
  PlayerRecordsBB_Challenge(const PlayerRecordsV3_Challenge<false>& rec);

  operator PlayerRecordsDC_Challenge() const;
  operator PlayerRecordsPC_Challenge() const;
  operator PlayerRecordsV3_Challenge<false>() const;
} __attribute__((packed));

template <bool IsBigEndian>
struct PlayerRecords_Battle {
  using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
  // On Episode 3, place_counts[0] is win count and [1] is loss count
  /* 00 */ parray<U16T, 4> place_counts;
  /* 08 */ U16T disconnect_count = 0;
  /* 0A */ parray<uint16_t, 3> unknown_a1;
  /* 10 */ parray<uint32_t, 2> unknown_a2;
  /* 18 */
} __attribute__((packed));

template <typename ItemIDT>
struct ChoiceSearchConfig {
  // 0 = enabled, 1 = disabled. Unused for command C3
  le_uint32_t choice_search_disabled = 0;
  struct Entry {
    ItemIDT parent_category_id = 0;
    ItemIDT category_id = 0;
  } __attribute__((packed));
  parray<Entry, 5> entries;
} __attribute__((packed));

template <typename DestT, typename SrcT = DestT>
DestT convert_player_disp_data(const SrcT&) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized strcpy_t should never be called");
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3>(
    const PlayerDispDataDCPCV3& src) {
  return src;
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(
    const PlayerDispDataBB& src) {
  return src.to_dcpcv3();
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB, PlayerDispDataDCPCV3>(
    const PlayerDispDataDCPCV3& src) {
  return src.to_bb();
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB>(
    const PlayerDispDataBB& src) {
  return src;
}
