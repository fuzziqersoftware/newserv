#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <string>
#include <utility>
#include <vector>

#include "ChoiceSearch.hh"
#include "FileContentsCache.hh"
#include "ItemData.hh"
#include "LevelTable.hh"
#include "Text.hh"
#include "Version.hh"

class Client;
class ItemParameterTable;

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
//       The flags field from the PSOGCCharacterFile::Character struct; see
//       SaveFileFormats.hh for details.
//   items[4].extension_data2 through items[7].extension_data2:
//       The timestamp when the character was last saved, in seconds since
//       January 1, 2000. Stored little-endian, so items[4] contains the LSB.
//   items[8].extension_data2 through items[12].extension_data2:
//       Number of power materials, mind materials, evade materials, def
//       materials, and luck materials (respectively) used by the player.
//   items[13].extension_data2 through items[15].extension_data2:
//       Unknown. These are not an array, but do appear to be related.

struct PlayerInventoryItem {
  /* 00 */ uint8_t present = 0;
  /* 01 */ uint8_t unknown_a1 = 0;
  // See note above about these fields
  /* 02 */ uint8_t extension_data1 = 0;
  /* 03 */ uint8_t extension_data2 = 0;
  /* 04 */ le_uint32_t flags = 0; // 8 = equipped
  /* 08 */ ItemData data;
  /* 1C */

  PlayerInventoryItem() = default;
  explicit PlayerInventoryItem(const ItemData& item, bool equipped = false);
} __attribute__((packed));

struct PlayerBankItem {
  /* 00 */ ItemData data;
  /* 14 */ le_uint16_t amount = 0;
  /* 16 */ le_uint16_t present = 0;
  /* 18 */

  inline bool operator<(const PlayerBankItem& other) const {
    return this->data < other.data;
  }
} __attribute__((packed));

struct PlayerInventory {
  /* 0000 */ uint8_t num_items = 0;
  /* 0001 */ uint8_t hp_from_materials = 0;
  /* 0002 */ uint8_t tp_from_materials = 0;
  /* 0003 */ uint8_t language = 0;
  /* 0004 */ parray<PlayerInventoryItem, 30> items;
  /* 034C */

  size_t find_item(uint32_t item_id) const;
  size_t find_item_by_primary_identifier(uint32_t primary_identifier) const;

  size_t find_equipped_item(EquipSlot slot) const;
  bool has_equipped_item(EquipSlot slot) const;
  void equip_item_id(uint32_t item_id, EquipSlot slot);
  void equip_item_index(size_t index, EquipSlot slot);
  void unequip_item_id(uint32_t item_id);
  void unequip_item_slot(EquipSlot slot);
  void unequip_item_index(size_t index);

  size_t remove_all_items_of_type(uint8_t data0, int16_t data1 = -1);

  void decode_from_client(std::shared_ptr<Client> c);
  void encode_for_client(std::shared_ptr<Client> c);
} __attribute__((packed));

struct PlayerBank {
  /* 0000 */ le_uint32_t num_items = 0;
  /* 0004 */ le_uint32_t meseta = 0;
  /* 0008 */ parray<PlayerBankItem, 200> items;
  /* 12C8 */

  void add_item(const ItemData& item);
  ItemData remove_item(uint32_t item_id, uint32_t amount);
  size_t find_item(uint32_t item_id);

  void sort();
  void assign_ids(uint32_t base_id);
} __attribute__((packed));

struct PlayerDispDataBB;

struct PlayerVisualConfig {
  /* 00 */ pstring<TextEncoding::ASCII, 0x10> name;
  /* 10 */ parray<uint8_t, 8> unknown_a2;
  /* 18 */ le_uint32_t name_color = 0xFFFFFFFF; // ARGB
  /* 1C */ uint8_t extra_model = 0;
  /* 1D */ parray<uint8_t, 0x0F> unused;
  // See compute_name_color_checksum for details on how this is computed. This
  // field is ignored on V3.
  /* 2C */ le_uint32_t name_color_checksum = 0;
  /* 30 */ uint8_t section_id = 0;
  /* 31 */ uint8_t char_class = 0;
  // validation_flags specifies that some parts of this structure are not valid
  // and should be ignored. The bits are:
  //   -----FCS
  //   F = class_flags is incorrect for the character's char_class value
  //   C = char_class is out of range
  //   S = section_id is out of range
  /* 32 */ uint8_t validation_flags = 0;
  /* 33 */ uint8_t version = 0;
  // class_flags specifies features of the character's class. The bits are:
  //   -------- -------- -------- FRHANMfm
  //   F = force, R = ranger, H = hunter
  //   A = android, N = newman, M = human
  //   f = female, m = male
  /* 34 */ le_uint32_t class_flags = 0;
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

  static uint32_t compute_name_color_checksum(uint32_t name_color);
  void compute_name_color_checksum();
} __attribute__((packed));

struct PlayerDispDataDCPCV3 {
  /* 00 */ PlayerStats stats;
  /* 24 */ PlayerVisualConfig visual;
  /* 74 */ parray<uint8_t, 0x48> config;
  /* BC */ parray<uint8_t, 0x14> technique_levels_v1;
  /* D0 */

  void enforce_lobby_join_limits_for_client(std::shared_ptr<Client> c);
  PlayerDispDataBB to_bb(uint8_t to_language, uint8_t from_language) const;
} __attribute__((packed));

struct PlayerDispDataBBPreview {
  /* 00 */ le_uint32_t experience = 0;
  /* 04 */ le_uint32_t level = 0;
  // The name field in this structure is used for the player's Guild Card
  // number, apparently (possibly because it's a char array and this is BB)
  /* 08 */ PlayerVisualConfig visual;
  /* 58 */ pstring<TextEncoding::UTF16, 0x10> name;
  /* 78 */ uint32_t play_time = 0;
  /* 7C */
} __attribute__((packed));

// BB player appearance and stats data
struct PlayerDispDataBB {
  /* 0000 */ PlayerStats stats;
  /* 0024 */ PlayerVisualConfig visual;
  /* 0074 */ pstring<TextEncoding::UTF16, 0x0C> name;
  /* 008C */ le_uint32_t play_time = 0;
  /* 0090 */ uint32_t unknown_a3 = 0;
  /* 0094 */ parray<uint8_t, 0xE8> config;
  /* 017C */ parray<uint8_t, 0x14> technique_levels_v1;
  /* 0190 */

  void enforce_lobby_join_limits_for_client(std::shared_ptr<Client> c);
  PlayerDispDataDCPCV3 to_dcpcv3(uint8_t to_language, uint8_t from_language) const;
  PlayerDispDataBBPreview to_preview() const;
  void apply_preview(const PlayerDispDataBBPreview&);
  void apply_dressing_room(const PlayerDispDataBBPreview&);
} __attribute__((packed));

struct GuildCardDC {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 20 */ pstring<TextEncoding::MARKED, 0x48> description;
  /* 68 */ parray<uint8_t, 0x11> unused2;
  /* 79 */ uint8_t present = 0;
  /* 7A */ uint8_t language = 0;
  /* 7B */ uint8_t section_id = 0;
  /* 7C */ uint8_t char_class = 0;
  /* 7D */
} __attribute__((packed));

struct GuildCardPC {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  // TODO: Is the length of the name field correct here?
  /* 08 */ pstring<TextEncoding::UTF16, 0x18> name;
  /* 38 */ pstring<TextEncoding::UTF16, 0x5A> description;
  /* EC */ uint8_t present = 0;
  /* ED */ uint8_t language = 0;
  /* EE */ uint8_t section_id = 0;
  /* EF */ uint8_t char_class = 0;
  /* F0 */
} __attribute__((packed));

struct GuildCardGC {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 20 */ pstring<TextEncoding::MARKED, 0x6C> description;
  /* 8C */ uint8_t present = 0;
  /* 8D */ uint8_t language = 0;
  /* 8E */ uint8_t section_id = 0;
  /* 8F */ uint8_t char_class = 0;
  /* 90 */
} __attribute__((packed));

struct GuildCardXB {
  /* 0000 */ le_uint32_t player_tag = 0;
  /* 0004 */ le_uint32_t guild_card_number = 0;
  /* 0008 */ le_uint32_t xb_user_id_high = 0;
  /* 000C */ le_uint32_t xb_user_id_low = 0;
  /* 0010 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 0028 */ pstring<TextEncoding::MARKED, 0x200> description;
  /* 0228 */ uint8_t present = 0;
  /* 0229 */ uint8_t language = 0;
  /* 022A */ uint8_t section_id = 0;
  /* 022B */ uint8_t char_class = 0;
  /* 022C */
} __attribute__((packed));

struct GuildCardBB {
  /* 0000 */ le_uint32_t guild_card_number = 0;
  /* 0004 */ pstring<TextEncoding::UTF16, 0x18> name;
  /* 0034 */ pstring<TextEncoding::UTF16, 0x10> team_name;
  /* 0054 */ pstring<TextEncoding::UTF16, 0x58> description;
  /* 0104 */ uint8_t present = 0;
  /* 0105 */ uint8_t language = 0;
  /* 0106 */ uint8_t section_id = 0;
  /* 0107 */ uint8_t char_class = 0;
  /* 0108 */

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataPC {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card_number = 0;
  // There's a strange behavior (bug? "feature"?) in Episode 3 where the start
  // button does nothing in the lobby (hence you can't "quit game") if the
  // client's IP address is zero. So, we fill it in with a fake nonzero value to
  // avoid this behavior, and to be consistent, we make IP addresses fake and
  // nonzero on all other versions too.
  be_uint32_t ip_address = 0x7F000001;
  le_uint32_t client_id = 0;
  pstring<TextEncoding::UTF16, 0x10> name;

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataDCGC {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card_number = 0;
  be_uint32_t ip_address = 0x7F000001;
  le_uint32_t client_id = 0;
  pstring<TextEncoding::ASCII, 0x10> name;

  void clear();
} __attribute__((packed));

struct XBNetworkLocation {
  /* 00 */ le_uint32_t internal_ipv4_address = 0x0A0A0A0A;
  /* 04 */ le_uint32_t external_ipv4_address = 0x23232323;
  /* 08 */ le_uint16_t port = 9500;
  /* 0A */ parray<uint8_t, 6> mac_address = 0x77;
  /* 10 */ le_uint32_t unknown_a1;
  /* 14 */ le_uint32_t unknown_a2;
  /* 18 */ le_uint64_t account_id = 0xFFFFFFFFFFFFFFFF;
  /* 20 */ parray<le_uint32_t, 4> unknown_a3;
  /* 24 */

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataXB {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ XBNetworkLocation netloc;
  /* 2C */ le_uint32_t client_id = 0;
  /* 30 */ pstring<TextEncoding::ASCII, 0x10> name;
  /* 40 */

  void clear();
} __attribute__((packed));

struct PlayerLobbyDataBB {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ le_uint32_t team_guild_card_number = 0;
  /* 0C */ le_uint32_t team_id = 0;
  /* 10 */ parray<uint8_t, 0x0C> unknown_a1;
  /* 1C */ le_uint32_t client_id = 0;
  /* 20 */ pstring<TextEncoding::UTF16, 0x10> name;
  // If this field is zero, the "Press F1 for help" prompt appears in the corner
  // of the screen in the lobby and on Pioneer 2.
  /* 40 */ le_uint32_t hide_help_prompt = 1;
  /* 44  */

  void clear();
} __attribute__((packed));

template <bool IsBigEndian>
struct ChallengeAwardState {
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
  U32T rank_award_flags = 0;
  U32T maximum_rank = 0; // Encrypted; see decrypt_challenge_time
} __attribute__((packed));

template <TextEncoding UnencryptedEncoding, TextEncoding EncryptedEncoding>
struct PlayerRecordsDCPC_Challenge {
  /* 00 */ le_uint16_t title_color = 0x7FFF;
  /* 02 */ parray<uint8_t, 2> unknown_u0;
  /* 04 */ pstring<EncryptedEncoding, 0x0C> rank_title;
  /* 10 */ parray<le_uint32_t, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time. TODO: This might be offline times
  /* 34 */ uint8_t grave_stage_num = 0;
  /* 35 */ uint8_t grave_floor = 0;
  /* 36 */ le_uint16_t grave_deaths = 0;
  // grave_time is encoded with the following bit fields:
  //   YYYYMMMM DDDDDDDD HHHHHHHH mmmmmmmm
  //   Y = year after 2000 (clamped to [0, 15])
  //   M = month
  //   D = day
  //   H = hour
  //   m = minute
  /* 38 */ le_uint32_t grave_time = 0;
  /* 3C */ le_uint32_t grave_defeated_by_enemy_rt_index = 0;
  /* 40 */ le_float grave_x = 0.0f;
  /* 44 */ le_float grave_y = 0.0f;
  /* 48 */ le_float grave_z = 0.0f;
  /* 4C */ pstring<UnencryptedEncoding, 0x14> grave_team;
  /* 60 */ pstring<UnencryptedEncoding, 0x18> grave_message;
  /* 78 */ parray<le_uint32_t, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time. TODO: This might be online times
  /* 9C */ parray<uint8_t, 4> unknown_l4;
  /* A0 */
} __attribute__((packed));

struct PlayerRecordsDC_Challenge : PlayerRecordsDCPC_Challenge<TextEncoding::CHALLENGE8, TextEncoding::ASCII> {
} __attribute__((packed));

struct PlayerRecordsPC_Challenge : PlayerRecordsDCPC_Challenge<TextEncoding::CHALLENGE16, TextEncoding::UTF16> {
} __attribute__((packed));

template <bool IsBigEndian>
struct PlayerRecordsV3_Challenge {
  using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;
  using FloatT = typename std::conditional<IsBigEndian, be_float, le_float>::type;

  // Offsets are (1) relative to start of C5 entry, and (2) relative to start
  // of save file structure
  struct Stats {
    /* 00:1C */ U16T title_color = 0x7FFF; // XRGB1555
    /* 02:1E */ parray<uint8_t, 2> unknown_u0;
    /* 04:20 */ parray<U32T, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time
    /* 28:44 */ parray<U32T, 5> times_ep2_online; // Encrypted; see decrypt_challenge_time
    /* 3C:58 */ parray<U32T, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time
    /* 60:7C */ uint8_t grave_is_ep2 = 0;
    /* 61:7D */ uint8_t grave_stage_num = 0;
    /* 62:7E */ uint8_t grave_floor = 0;
    /* 63:7F */ uint8_t unknown_g0 = 0;
    /* 64:80 */ U16T grave_deaths = 0;
    /* 66:82 */ parray<uint8_t, 2> unknown_u4;
    /* 68:84 */ U32T grave_time = 0; // Encoded as in PlayerRecordsDCPC_Challenge
    /* 6C:88 */ U32T grave_defeated_by_enemy_rt_index = 0;
    /* 70:8C */ FloatT grave_x = 0.0f;
    /* 74:90 */ FloatT grave_y = 0.0f;
    /* 78:94 */ FloatT grave_z = 0.0f;
    /* 7C:98 */ pstring<TextEncoding::ASCII, 0x14> grave_team;
    /* 90:AC */ pstring<TextEncoding::ASCII, 0x20> grave_message;
    /* B0:CC */ parray<uint8_t, 4> unknown_m5;
    /* B4:D0 */ parray<U32T, 3> unknown_t6;
    /* C0:DC */ ChallengeAwardState<IsBigEndian> ep1_online_award_state;
    /* C8:E4 */ ChallengeAwardState<IsBigEndian> ep2_online_award_state;
    /* D0:EC */ ChallengeAwardState<IsBigEndian> ep1_offline_award_state;
    /* D8:F4 */
  } __attribute__((packed));
  /* 0000:001C */ Stats stats;
  // On Episode 3, there are special cases that apply to this field - if the
  // text ends with certain strings, the player will have particle effects
  // emanate from their character in the lobby every 2 seconds. The effects are:
  //   Ends with ":GOD" => blue circle
  //   Ends with ":KING" => white particles
  //   Ends with ":LORD" => rising yellow sparkles
  //   Ends with ":CHAMP" => green circle
  /* 00D8:00F4 */ pstring<TextEncoding::CHALLENGE8, 0x0C> rank_title;
  /* 00E4:0100 */ parray<uint8_t, 0x1C> unknown_l7;
  /* 0100:011C */
} __attribute__((packed));

struct PlayerRecordsBB_Challenge {
  /* 0000 */ le_uint16_t title_color = 0x7FFF; // XRGB1555
  /* 0002 */ parray<uint8_t, 2> unknown_u0;
  /* 0004 */ parray<le_uint32_t, 9> times_ep1_online; // Encrypted; see decrypt_challenge_time
  /* 0028 */ parray<le_uint32_t, 5> times_ep2_online; // Encrypted; see decrypt_challenge_time
  /* 003C */ parray<le_uint32_t, 9> times_ep1_offline; // Encrypted; see decrypt_challenge_time
  /* 0060 */ uint8_t grave_is_ep2 = 0;
  /* 0061 */ uint8_t grave_stage_num = 0;
  /* 0062 */ uint8_t grave_floor = 0;
  /* 0063 */ uint8_t unknown_g0 = 0;
  /* 0064 */ le_uint16_t grave_deaths = 0;
  /* 0066 */ parray<uint8_t, 2> unknown_u4;
  /* 0068 */ le_uint32_t grave_time = 0; // Encoded as in PlayerRecordsDCPC_Challenge
  /* 006C */ le_uint32_t grave_defeated_by_enemy_rt_index = 0;
  /* 0070 */ le_float grave_x = 0.0f;
  /* 0074 */ le_float grave_y = 0.0f;
  /* 0078 */ le_float grave_z = 0.0f;
  /* 007C */ pstring<TextEncoding::UTF16, 0x14> grave_team;
  /* 00A4 */ pstring<TextEncoding::UTF16, 0x20> grave_message;
  /* 00E4 */ parray<uint8_t, 4> unknown_m5;
  /* 00E8 */ parray<le_uint32_t, 3> unknown_t6;
  /* 00F4 */ ChallengeAwardState<false> ep1_online_award_state;
  /* 00FC */ ChallengeAwardState<false> ep2_online_award_state;
  /* 0104 */ ChallengeAwardState<false> ep1_offline_award_state;
  /* 010C */ pstring<TextEncoding::UTF16, 0x0C> rank_title; // Encrypted; see decrypt_challenge_rank_text
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

template <typename DestT, typename SrcT = DestT>
DestT convert_player_disp_data(const SrcT&, uint8_t, uint8_t) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized convert_player_disp_data should never be called");
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3>(const PlayerDispDataDCPCV3& src, uint8_t, uint8_t) {
  return src;
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(
    const PlayerDispDataBB& src, uint8_t to_language, uint8_t from_language) {
  return src.to_dcpcv3(to_language, from_language);
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB, PlayerDispDataDCPCV3>(
    const PlayerDispDataDCPCV3& src, uint8_t to_language, uint8_t from_language) {
  return src.to_bb(to_language, from_language);
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB>(
    const PlayerDispDataBB& src, uint8_t, uint8_t) {
  return src;
}

struct QuestFlagsForDifficulty {
  parray<uint8_t, 0x80> data;

  inline bool get(uint16_t flag_index) const {
    size_t byte_index = flag_index >> 3;
    uint8_t mask = 0x80 >> (flag_index & 7);
    return !!(this->data[byte_index] & mask);
  }
  inline void set(uint16_t flag_index) {
    size_t byte_index = flag_index >> 3;
    uint8_t mask = 0x80 >> (flag_index & 7);
    this->data[byte_index] |= mask;
  }
  inline void clear(uint16_t flag_index) {
    size_t byte_index = flag_index >> 3;
    uint8_t mask = 0x80 >> (flag_index & 7);
    this->data[byte_index] &= (~mask);
  }
  inline void update_all(bool set) {
    if (set) {
      this->data.clear(0xFF);
    } else {
      this->data.clear(0x00);
    }
  }
} __attribute__((packed));

struct QuestFlags {
  parray<QuestFlagsForDifficulty, 4> data;

  inline bool get(uint8_t difficulty, uint16_t flag_index) const {
    return this->data[difficulty].get(flag_index);
  }
  inline void set(uint8_t difficulty, uint16_t flag_index) {
    this->data[difficulty].set(flag_index);
  }
  inline void clear(uint8_t difficulty, uint16_t flag_index) {
    this->data[difficulty].clear(flag_index);
  }
  inline void update_all(uint8_t difficulty, bool set) {
    this->data[difficulty].update_all(set);
  }
  inline void update_all(bool set) {
    for (size_t z = 0; z < 4; z++) {
      this->update_all(z, set);
    }
  }
} __attribute__((packed));

struct BattleRules {
  enum class TechDiskMode : uint8_t {
    ALLOW = 0,
    FORBID_ALL = 1,
    LIMIT_LEVEL = 2,
  };
  enum class WeaponAndArmorMode : uint8_t {
    ALLOW = 0,
    CLEAR_AND_ALLOW = 1,
    FORBID_ALL = 2,
    FORBID_RARES = 3,
  };
  enum class MagMode : uint8_t {
    ALLOW = 0,
    FORBID_ALL = 1,
  };
  enum class ToolMode : uint8_t {
    ALLOW = 0,
    CLEAR_AND_ALLOW = 1,
    FORBID_ALL = 2,
  };
  enum class TrapMode : uint8_t {
    DEFAULT = 0,
    ALL_PLAYERS = 1,
  };
  enum class MesetaMode : uint8_t {
    ALLOW = 0,
    FORBID_ALL = 1,
    CLEAR_AND_ALLOW = 2,
  };

  // Set by quest opcode F812, but values are remapped.
  //   F812 00 => FORBID_ALL
  //   F812 01 => ALLOW
  //   F812 02 => LIMIT_LEVEL
  /* 00 */ TechDiskMode tech_disk_mode = TechDiskMode::ALLOW;
  // Set by quest opcode F813, but values are remapped.
  //   F813 00 => FORBID_ALL
  //   F813 01 => ALLOW
  //   F813 02 => CLEAR_AND_ALLOW
  //   F813 03 => FORBID_RARES
  /* 01 */ WeaponAndArmorMode weapon_and_armor_mode = WeaponAndArmorMode::ALLOW;
  // Set by quest opcode F814, but values are remapped.
  //   F814 00 => FORBID_ALL
  //   F814 01 => ALLOW
  /* 02 */ MagMode mag_mode = MagMode::ALLOW;
  // Set by quest opcode F815, but values are remapped.
  //   F815 00 => FORBID_ALL
  //   F815 01 => ALLOW
  //   F815 02 => CLEAR_AND_ALLOW
  /* 03 */ ToolMode tool_mode = ToolMode::ALLOW;
  // Set by quest opcode F816. Values are not remapped.
  //   F816 00 => DEFAULT
  //   F816 01 => ALL_PLAYERS
  /* 04 */ TrapMode trap_mode = TrapMode::DEFAULT;
  // Set by quest opcode F817. Value appears to be unused in all PSO versions.
  /* 05 */ uint8_t unused_F817 = 0;
  // Set by quest opcode F818, but values are remapped.
  //   F818 00 => 01
  //   F818 01 => 00
  //   F818 02 => 02
  // TODO: Define an enum class for this field.
  /* 06 */ uint8_t respawn_mode = 0;
  // Set by quest opcode F819.
  /* 07 */ uint8_t replace_char = 0;
  // Set by quest opcode F81A, but value is inverted.
  /* 08 */ uint8_t drop_weapon = 0;
  // Set by quest opcode F81B.
  /* 09 */ uint8_t is_teams = 0;
  // Set by quest opcode F852.
  /* 0A */ uint8_t hide_target_reticle = 0;
  // Set by quest opcode F81E. Values are not remapped.
  //   F81E 00 => ALLOW
  //   F81E 01 => FORBID_ALL
  //   F81E 02 => CLEAR_AND_ALLOW
  /* 0B */ MesetaMode meseta_mode = MesetaMode::ALLOW;
  // Set by quest opcode F81D.
  /* 0C */ uint8_t death_level_up = 0;
  // Set by quest opcode F851. The trap type is remapped:
  //   F851 00 XX => set count to XX for trap type 00
  //   F851 01 XX => set count to XX for trap type 02
  //   F851 02 XX => set count to XX for trap type 03
  //   F851 03 XX => set count to XX for trap type 01
  /* 0D */ parray<uint8_t, 4> trap_counts;
  // Set by quest opcode F85E.
  /* 11 */ uint8_t enable_sonar = 0;
  // Set by quest opcode F85F.
  /* 12 */ uint8_t sonar_count = 0;
  // Set by quest opcode F89E.
  /* 13 */ uint8_t forbid_scape_dolls = 0;
  // This value does not appear to be set by any quest opcode.
  /* 14 */ le_uint32_t unknown_a1 = 0;
  // Set by quest opcode F86F.
  /* 18 */ le_uint32_t lives = 0;
  // Set by quest opcode F870.
  /* 1C */ le_uint32_t max_tech_level = 0;
  // Set by quest opcode F871.
  /* 20 */ le_uint32_t char_level = 0;
  // Set by quest opcode F872.
  /* 24 */ le_uint32_t time_limit = 0;
  // Set by quest opcode F8A8.
  /* 28 */ le_uint16_t death_tech_level_up = 0;
  /* 2A */ parray<uint8_t, 2> unused;
  // Set by quest opcode F86B.
  /* 2C */ le_uint32_t box_drop_area = 0;
  /* 30 */

  BattleRules() = default;
  explicit BattleRules(const JSON& json);
  JSON json() const;

  bool operator==(const BattleRules& other) const = default;
  bool operator!=(const BattleRules& other) const = default;
} __attribute__((packed));

struct ChallengeTemplateDefinition {
  uint32_t level;
  std::vector<PlayerInventoryItem> items;
  struct TechLevel {
    uint8_t tech_num;
    uint8_t level;
  };
  std::vector<TechLevel> tech_levels;
};

const ChallengeTemplateDefinition& get_challenge_template_definition(Version version, uint32_t class_flags, size_t index);

struct SymbolChat {
  // Bits: ----------------------DMSSSCCCFF
  //   S = sound, C = face color, F = face shape, D = capture, M = mute sound
  /* 00 */ le_uint32_t spec = 0;

  // Corner objects are specified in reading order ([0] is the top-left one).
  // Bits (each entry): ---VHCCCZZZZZZZZ
  //   V = reverse vertical, H = reverse horizontal, C = color, Z = object
  // If Z is all 1 bits (0xFF), no corner object is rendered.
  /* 04 */ parray<le_uint16_t, 4> corner_objects;

  struct FacePart {
    uint8_t type = 0xFF; // FF = no part in this slot
    uint8_t x = 0;
    uint8_t y = 0;
    // Bits: ------VH (V = reverse vertical, H = reverse horizontal)
    uint8_t flags = 0;
  } __attribute__((packed));
  /* 0C */ parray<FacePart, 12> face_parts;
  /* 3C */

  SymbolChat();
} __attribute__((packed));
