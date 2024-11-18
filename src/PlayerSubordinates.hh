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
#include "PSOEncryption.hh"
#include "PlayerInventory.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

class Client;
class ItemParameterTable;

struct PlayerDispDataBB;

template <bool BE>
struct PlayerVisualConfigT {
  /* 00 */ pstring<TextEncoding::ASCII, 0x10> name;
  /* 10 */ parray<uint8_t, 8> unknown_a2;
  /* 18 */ U32T<BE> name_color = 0xFFFFFFFF; // ARGB
  /* 1C */ uint8_t extra_model = 0;
  /* 1D */ parray<uint8_t, 0x0F> unused;
  // See compute_name_color_checksum for details on how this is computed. If the
  // value is incorrect, V1 and V2 will ignore the name_color field and use the
  // default color instead. This field is ignored on GC; on BB (and presumably
  // Xbox), if this has a nonzero value, the "Change Name" option appears in the
  // character selection menu.
  /* 2C */ U32T<BE> name_color_checksum = 0;
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
  /* 34 */ U32T<BE> class_flags = 0;
  /* 38 */ U16T<BE> costume = 0;
  /* 3A */ U16T<BE> skin = 0;
  /* 3C */ U16T<BE> face = 0;
  /* 3E */ U16T<BE> head = 0;
  /* 40 */ U16T<BE> hair = 0;
  /* 42 */ U16T<BE> hair_r = 0;
  /* 44 */ U16T<BE> hair_g = 0;
  /* 46 */ U16T<BE> hair_b = 0;
  /* 48 */ F32T<BE> proportion_x = 0.0;
  /* 4C */ F32T<BE> proportion_y = 0.0;
  /* 50 */

  static uint32_t compute_name_color_checksum(uint32_t name_color) {
    uint8_t x = (phosg::random_object<uint32_t>() % 0xFF) + 1;
    uint8_t y = (phosg::random_object<uint32_t>() % 0xFF) + 1;
    // name_color (ARGB)   = ABCDEFGHabcdefghIJKLMNOPijklmnop
    // name_color_checksum = 000000000ijklmabcdeIJKLM00000000 ^ xxxxxxxxyyyyyyyyxxxxxxxxyyyyyyyy
    uint32_t xbrgx95558 = ((name_color << 15) & 0x007C0000) | ((name_color >> 6) & 0x0003E000) | ((name_color >> 3) & 0x00001F00);
    uint32_t mask = (x << 24) | (y << 16) | (x << 8) | y;
    return xbrgx95558 ^ mask;
  }

  void compute_name_color_checksum() {
    this->name_color_checksum = this->compute_name_color_checksum(this->name_color);
  }

  void enforce_lobby_join_limits_for_version(Version v) {
    struct ClassMaxes {
      uint16_t costume;
      uint16_t skin;
      uint16_t face;
      uint16_t head;
      uint16_t hair;
    };
    static constexpr ClassMaxes v1_v2_class_maxes[14] = {
        {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
        {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
        {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
        {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
        {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
        {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
        {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    };
    static constexpr ClassMaxes v3_v4_class_maxes[19] = {
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
        {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0001},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0001},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
        {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};

    const ClassMaxes* maxes;
    if (v == Version::GC_NTE) {
      // GC NTE has HUcaseal, FOmar, and RAmarl, but missing others
      if (this->char_class >= 12) {
        this->char_class = 0; // Invalid classes -> HUmar
      }

      // GC NTE is basically v2, but uses v3 maxes
      this->version = std::min<uint8_t>(this->version, 2);
      maxes = &v3_v4_class_maxes[this->char_class];

      // Prevent GC NTE from crashing from extra models
      this->extra_model = 0;
      this->validation_flags &= 0xFD;
    } else if (is_v1_or_v2(v)) {
      // V1/V2 have fewer classes, so we'll substitute some here
      switch (this->char_class) {
        case 0: // HUmar
        case 1: // HUnewearl
        case 2: // HUcast
        case 3: // RAmar
        case 4: // RAcast
        case 5: // RAcaseal
        case 6: // FOmarl
        case 7: // FOnewm
        case 8: // FOnewearl
        case 12: // V3 custom 1
        case 13: // V3 custom 2
          break;
        case 9: // HUcaseal
          this->char_class = 5; // HUcaseal -> RAcaseal
          break;
        case 10: // FOmar
          this->char_class = 0; // FOmar -> HUmar
          break;
        case 11: // RAmarl
          this->char_class = 1; // RAmarl -> HUnewearl
          break;
        case 14: // V2 custom 1 / V3 custom 3
        case 15: // V2 custom 2 / V3 custom 4
        case 16: // V2 custom 3 / V3 custom 5
        case 17: // V2 custom 4 / V3 custom 6
        case 18: // V2 custom 5 / V3 custom 7
          this->char_class -= 5;
          break;
        default:
          this->char_class = 0; // Invalid classes -> HUmar
      }

      this->version = std::min<uint8_t>(this->version, is_v1(v) ? 0 : 2);
      maxes = &v1_v2_class_maxes[this->char_class];

    } else {
      if (this->char_class >= 19) {
        this->char_class = 0; // Invalid classes -> HUmar
      }
      this->version = std::min<uint8_t>(this->version, 3);
      maxes = &v3_v4_class_maxes[this->char_class];
    }

    // V1/V2 has fewer costumes and android skins, so substitute them here
    this->costume = maxes->costume ? (this->costume % maxes->costume) : 0;
    this->skin = maxes->skin ? (this->skin % maxes->skin) : 0;
    this->face = maxes->face ? (this->face % maxes->face) : 0;
    this->head = maxes->head ? (this->head % maxes->head) : 0;
    this->hair = maxes->hair ? (this->hair % maxes->hair) : 0;

    if (this->name_color == 0) {
      this->name_color = 0xFFFFFFFF;
    }
    if (is_v1_or_v2(v)) {
      this->compute_name_color_checksum();
    } else {
      this->name_color_checksum = 0;
    }
    this->class_flags = class_flags_for_class(this->char_class);
    this->name.clear_after_bytes(0x0C);
  }

  operator PlayerVisualConfigT<!BE>() const {
    PlayerVisualConfigT<!BE> ret;
    ret.name = this->name;
    ret.unknown_a2 = this->unknown_a2;
    ret.name_color = this->name_color.load();
    ret.extra_model = this->extra_model;
    ret.unused = this->unused;
    ret.name_color_checksum = this->name_color_checksum.load();
    ret.section_id = this->section_id;
    ret.char_class = this->char_class;
    ret.validation_flags = this->validation_flags;
    ret.version = this->version;
    ret.class_flags = this->class_flags.load();
    ret.costume = this->costume.load();
    ret.skin = this->skin.load();
    ret.face = this->face.load();
    ret.head = this->head.load();
    ret.hair = this->hair.load();
    ret.hair_r = this->hair_r.load();
    ret.hair_g = this->hair_g.load();
    ret.hair_b = this->hair_b.load();
    ret.proportion_x = this->proportion_x.load();
    ret.proportion_y = this->proportion_y.load();
    return ret;
  }
} __packed__;
using PlayerVisualConfig = PlayerVisualConfigT<false>;
using PlayerVisualConfigBE = PlayerVisualConfigT<true>;
check_struct_size(PlayerVisualConfig, 0x50);
check_struct_size(PlayerVisualConfigBE, 0x50);

template <bool BE>
struct PlayerDispDataDCPCV3T {
  /* 00 */ PlayerStatsT<BE> stats;
  /* 24 */ PlayerVisualConfigT<BE> visual;
  /* 74 */ parray<uint8_t, 0x48> config;
  /* BC */ parray<uint8_t, 0x14> technique_levels_v1;
  /* D0 */

  void enforce_lobby_join_limits_for_version(Version v) {
    this->visual.enforce_lobby_join_limits_for_version(v);
  }

  PlayerDispDataBB to_bb(uint8_t to_language, uint8_t from_language) const;
} __packed__;
using PlayerDispDataDCPCV3 = PlayerDispDataDCPCV3T<false>;
using PlayerDispDataDCPCV3BE = PlayerDispDataDCPCV3T<true>;
check_struct_size(PlayerDispDataDCPCV3, 0xD0);
check_struct_size(PlayerDispDataDCPCV3BE, 0xD0);

struct PlayerDispDataBBPreview {
  /* 00 */ le_uint32_t experience = 0;
  /* 04 */ le_uint32_t level = 0;
  // The name field in this structure is used for the player's Guild Card
  // number, apparently (possibly because it's a char array and this is BB)
  /* 08 */ PlayerVisualConfig visual;
  /* 58 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> name;
  /* 78 */ uint32_t play_time_seconds = 0;
  /* 7C */
} __packed_ws__(PlayerDispDataBBPreview, 0x7C);

// BB player appearance and stats data
struct PlayerDispDataBB {
  /* 0000 */ PlayerStats stats;
  /* 0024 */ PlayerVisualConfig visual;
  /* 0074 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> name;
  /* 0094 */ parray<uint8_t, 0xE8> config;
  /* 017C */ parray<uint8_t, 0x14> technique_levels_v1;
  /* 0190 */

  void enforce_lobby_join_limits_for_version(Version v) {
    this->visual.enforce_lobby_join_limits_for_version(v);
    this->name.clear_after_bytes(0x18); // 12 characters
  }

  template <bool BE>
  PlayerDispDataDCPCV3T<BE> to_dcpcv3(uint8_t to_language, uint8_t from_language) const {
    PlayerDispDataDCPCV3T<BE> ret;
    ret.stats = this->stats;
    ret.visual = this->visual;
    std::string decoded_name = this->name.decode(from_language);
    ret.visual.name.encode(decoded_name, to_language);
    ret.config = this->config;
    ret.technique_levels_v1 = this->technique_levels_v1;
    return ret;
  }

  void apply_preview(const PlayerDispDataBBPreview&);
  void apply_dressing_room(const PlayerDispDataBBPreview&);
} __packed_ws__(PlayerDispDataBB, 0x190);

template <bool BE>
PlayerDispDataBB PlayerDispDataDCPCV3T<BE>::to_bb(uint8_t to_language, uint8_t from_language) const {
  PlayerDispDataBB bb;
  bb.stats = this->stats;
  bb.visual = this->visual;
  bb.visual.name.encode("         0");
  std::string decoded_name = this->visual.name.decode(from_language);
  bb.name.encode(decoded_name, to_language);
  bb.config = this->config;
  bb.technique_levels_v1 = this->technique_levels_v1;
  return bb;
}

struct GuildCardBB;

struct GuildCardDCNTE {
  /* 00 */ le_uint32_t player_tag = 0x00010000;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 20 */ pstring<TextEncoding::MARKED, 0x48> description;
  /* 68 */ parray<uint8_t, 0x0F> unused2;
  /* 77 */ uint8_t present = 0;
  /* 78 */ uint8_t language = 0;
  /* 79 */ uint8_t section_id = 0;
  /* 7A */ uint8_t char_class = 0;
  /* 7B */

  operator GuildCardBB() const;
} __packed_ws__(GuildCardDCNTE, 0x7B);

struct GuildCardDC {
  /* 00 */ le_uint32_t player_tag = 0x00010000;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 20 */ pstring<TextEncoding::MARKED, 0x48> description;
  /* 68 */ parray<uint8_t, 0x11> unused2;
  /* 79 */ uint8_t present = 0;
  /* 7A */ uint8_t language = 0;
  /* 7B */ uint8_t section_id = 0;
  /* 7C */ uint8_t char_class = 0;
  /* 7D */

  operator GuildCardBB() const;
} __packed_ws__(GuildCardDC, 0x7D);

struct GuildCardPC {
  /* 00 */ le_uint32_t player_tag = 0x00010000;
  /* 04 */ le_uint32_t guild_card_number = 0;
  // TODO: Is the length of the name field correct here?
  /* 08 */ pstring<TextEncoding::UTF16, 0x18> name;
  /* 38 */ pstring<TextEncoding::UTF16, 0x5A> description;
  /* EC */ uint8_t present = 0;
  /* ED */ uint8_t language = 0;
  /* EE */ uint8_t section_id = 0;
  /* EF */ uint8_t char_class = 0;
  /* F0 */

  operator GuildCardBB() const;
} __packed_ws__(GuildCardPC, 0xF0);

// 0000 | 62 00 AC 00 06 2A 00 00 00 00 01 00 90 96 66 8C | b    *        f
// 0010 | 31 31 31 31 00 00 00 00 00 00 00 00 00 00 00 00 | 1111
// 0020 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0030 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0040 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0050 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0060 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0070 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0080 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 0090 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |
// 00A0 | 00 00 00 00 00 00 00 00 01 00 06 00             |

template <bool BE, size_t DescriptionLength>
struct GuildCardGCT {
  /* NTE:Final */
  /* 00:00 */ U32T<BE> player_tag = 0x00010000;
  /* 04:04 */ U32T<BE> guild_card_number = 0;
  /* 08:08 */ pstring<TextEncoding::ASCII, 0x18> name;
  /* 20:20 */ pstring<TextEncoding::MARKED, DescriptionLength> description;
  /* 8C:8C */ uint8_t present = 0;
  /* 8D:8D */ uint8_t language = 0;
  /* 8E:8E */ uint8_t section_id = 0;
  /* 8F:8F */ uint8_t char_class = 0;
  /* 90:90 */

  operator GuildCardBB() const;
} __packed__;
using GuildCardGCNTE = GuildCardGCT<false, 0x80>;
using GuildCardGCNTEBE = GuildCardGCT<true, 0x80>;
using GuildCardGC = GuildCardGCT<false, 0x6C>;
using GuildCardGCBE = GuildCardGCT<true, 0x6C>;
check_struct_size(GuildCardGCNTE, 0xA4);
check_struct_size(GuildCardGCNTEBE, 0xA4);
check_struct_size(GuildCardGC, 0x90);
check_struct_size(GuildCardGCBE, 0x90);

struct GuildCardXB {
  /* 0000 */ le_uint32_t player_tag = 0x00010000;
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

  operator GuildCardBB() const;
} __packed_ws__(GuildCardXB, 0x22C);

struct GuildCardBB {
  /* 0000 */ le_uint32_t guild_card_number = 0;
  /* 0004 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x18> name;
  /* 0034 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
  /* 0054 */ pstring<TextEncoding::UTF16, 0x58> description;
  /* 0104 */ uint8_t present = 0;
  /* 0105 */ uint8_t language = 0;
  /* 0106 */ uint8_t section_id = 0;
  /* 0107 */ uint8_t char_class = 0;
  /* 0108 */

  void clear();

  operator GuildCardDCNTE() const;
  operator GuildCardDC() const;
  operator GuildCardPC() const;
  template <bool BE, size_t DescriptionLength>
  operator GuildCardGCT<BE, DescriptionLength>() const {
    GuildCardGCT<BE, DescriptionLength> ret;
    ret.player_tag = 0x00010000;
    ret.guild_card_number = this->guild_card_number.load();
    ret.name.encode(this->name.decode(this->language), this->language);
    ret.description.encode(this->description.decode(this->language), this->language);
    ret.present = this->present;
    ret.language = this->language;
    ret.section_id = this->section_id;
    ret.char_class = this->char_class;
    return ret;
  }
  operator GuildCardXB() const;
} __packed_ws__(GuildCardBB, 0x108);

template <bool BE, size_t DescriptionLength>
GuildCardGCT<BE, DescriptionLength>::operator GuildCardBB() const {
  GuildCardBB ret;
  ret.guild_card_number = this->guild_card_number.load();
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

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
} __packed_ws__(PlayerLobbyDataPC, 0x30);

struct PlayerLobbyDataDCGC {
  le_uint32_t player_tag = 0;
  le_uint32_t guild_card_number = 0;
  be_uint32_t ip_address = 0x7F000001;
  le_uint32_t client_id = 0;
  pstring<TextEncoding::ASCII, 0x10> name;

  void clear();
} __packed_ws__(PlayerLobbyDataDCGC, 0x20);

struct XBNetworkLocation {
  /* 00 */ le_uint32_t internal_ipv4_address = 0x0A0A0A0A;
  /* 04 */ le_uint32_t external_ipv4_address = 0x23232323;
  /* 08 */ le_uint16_t port = 9500;
  /* 0A */ parray<uint8_t, 6> mac_address = 0x77;
  /* 10 */ le_uint32_t sg_ip_address = 0x0B0B0B0B;
  /* 14 */ le_uint32_t spi = 0xCCCCCCCC;
  /* 18 */ le_uint64_t account_id = 0xFFFFFFFFFFFFFFFF;
  /* 20 */ parray<le_uint32_t, 4> unknown_a3;
  /* 30 */

  void clear();
} __packed_ws__(XBNetworkLocation, 0x30);

struct PlayerLobbyDataXB {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ XBNetworkLocation netloc;
  /* 38 */ le_uint32_t client_id = 0;
  /* 3C */ pstring<TextEncoding::ASCII, 0x10> name;
  /* 4C */

  void clear();
} __packed_ws__(PlayerLobbyDataXB, 0x4C);

struct PlayerLobbyDataBB {
  /* 00 */ le_uint32_t player_tag = 0;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ le_uint32_t team_master_guild_card_number = 0;
  /* 0C */ le_uint32_t team_id = 0;
  /* 10 */ parray<uint8_t, 0x0C> unknown_a1;
  /* 1C */ le_uint32_t client_id = 0;
  /* 20 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> name;
  // If this field is zero, the "Press F1 for help" prompt appears in the corner
  // of the screen in the lobby and on Pioneer 2.
  /* 40 */ le_uint32_t hide_help_prompt = 1;
  /* 44 */

  void clear();
} __packed_ws__(PlayerLobbyDataBB, 0x44);

template <bool BE>
struct ChallengeAwardStateT {
  U32T<BE> rank_award_flags = 0;
  ChallengeTimeT<BE> maximum_rank;

  operator ChallengeAwardStateT<!BE>() const {
    ChallengeAwardStateT<!BE> ret;
    ret.rank_award_flags = this->rank_award_flags.load();
    ret.maximum_rank = this->maximum_rank;
    return ret;
  }
} __packed__;
using ChallengeAwardState = ChallengeAwardStateT<false>;
using ChallengeAwardStateBE = ChallengeAwardStateT<true>;
check_struct_size(ChallengeAwardState, 8);
check_struct_size(ChallengeAwardStateBE, 8);

template <TextEncoding UnencryptedEncoding, TextEncoding EncryptedEncoding>
struct PlayerRecordsChallengeDCPCT {
  /* DC:PC */
  /* 00:00 */ le_uint16_t title_color = 0x7FFF;
  /* 02:02 */ parray<uint8_t, 2> unknown_u0;
  /* 04:04 */ pstring<EncryptedEncoding, 0x0C> rank_title;
  /* 10:1C */ parray<ChallengeTimeT<false>, 9> times_ep1_online; // TODO: This might be offline times
  /* 34:40 */ uint8_t grave_stage_num = 0;
  /* 35:41 */ uint8_t grave_floor = 0;
  /* 36:42 */ le_uint16_t grave_deaths = 0;
  // grave_time is encoded with the following bit fields:
  //   YYYYMMMM DDDDDDDD HHHHHHHH mmmmmmmm
  //   Y = year after 2000 (clamped to [0, 15])
  //   M = month
  //   D = day
  //   H = hour
  //   m = minute
  /* 38:44 */ le_uint32_t grave_time = 0;
  /* 3C:48 */ le_uint32_t grave_defeated_by_enemy_rt_index = 0;
  /* 40:4C */ le_float grave_x = 0.0f;
  /* 44:50 */ le_float grave_y = 0.0f;
  /* 48:54 */ le_float grave_z = 0.0f;
  /* 4C:58 */ pstring<UnencryptedEncoding, 0x14> grave_team;
  /* 60:80 */ pstring<UnencryptedEncoding, 0x18> grave_message;
  /* 78:B0 */ parray<ChallengeTimeT<false>, 9> times_ep1_offline; // TODO: This might be online times
  /* 9C:D4 */ parray<uint8_t, 4> unknown_l4;
  /* A0:D8 */
} __packed__;
using PlayerRecordsChallengeDC = PlayerRecordsChallengeDCPCT<TextEncoding::ASCII, TextEncoding::CHALLENGE8>;
using PlayerRecordsChallengePC = PlayerRecordsChallengeDCPCT<TextEncoding::UTF16, TextEncoding::CHALLENGE16>;
check_struct_size(PlayerRecordsChallengeDC, 0xA0);
check_struct_size(PlayerRecordsChallengePC, 0xD8);

template <bool BE>
struct PlayerRecordsChallengeV3T {
  // Offsets are (1) relative to start of C5 entry, and (2) relative to start
  // of save file structure
  struct Stats {
    /* 00:1C */ U16T<BE> title_color = 0x7FFF; // XRGB1555
    /* 02:1E */ parray<uint8_t, 2> unknown_u0;
    /* 04:20 */ parray<ChallengeTimeT<BE>, 9> times_ep1_online;
    /* 28:44 */ parray<ChallengeTimeT<BE>, 5> times_ep2_online;
    /* 3C:58 */ parray<ChallengeTimeT<BE>, 9> times_ep1_offline;
    /* 60:7C */ uint8_t grave_is_ep2 = 0;
    /* 61:7D */ uint8_t grave_stage_num = 0;
    /* 62:7E */ uint8_t grave_floor = 0;
    /* 63:7F */ uint8_t unknown_g0 = 0;
    /* 64:80 */ U16T<BE> grave_deaths = 0;
    /* 66:82 */ parray<uint8_t, 2> unknown_u4;
    /* 68:84 */ U32T<BE> grave_time = 0; // Encoded as in PlayerRecordsChallengeDCPC
    /* 6C:88 */ U32T<BE> grave_defeated_by_enemy_rt_index = 0;
    /* 70:8C */ F32T<BE> grave_x = 0.0f;
    /* 74:90 */ F32T<BE> grave_y = 0.0f;
    /* 78:94 */ F32T<BE> grave_z = 0.0f;
    /* 7C:98 */ pstring<TextEncoding::ASCII, 0x14> grave_team;
    /* 90:AC */ pstring<TextEncoding::ASCII, 0x20> grave_message;
    /* B0:CC */ parray<uint8_t, 4> unknown_m5;
    /* B4:D0 */ parray<U32T<BE>, 3> unknown_t6;
    /* C0:DC */ ChallengeAwardStateT<BE> ep1_online_award_state;
    /* C8:E4 */ ChallengeAwardStateT<BE> ep2_online_award_state;
    /* D0:EC */ ChallengeAwardStateT<BE> ep1_offline_award_state;
    /* D8:F4 */
  } __packed__;
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
} __packed__;
using PlayerRecordsChallengeV3 = PlayerRecordsChallengeV3T<false>;
using PlayerRecordsChallengeV3BE = PlayerRecordsChallengeV3T<true>;
check_struct_size(PlayerRecordsChallengeV3, 0x100);
check_struct_size(PlayerRecordsChallengeV3BE, 0x100);

struct PlayerRecordsChallengeBB {
  /* 0000 */ le_uint16_t title_color = 0x7FFF; // XRGB1555
  /* 0002 */ parray<uint8_t, 2> unknown_u0;
  /* 0004 */ parray<ChallengeTimeT<false>, 9> times_ep1_online;
  /* 0028 */ parray<ChallengeTimeT<false>, 5> times_ep2_online;
  /* 003C */ parray<ChallengeTimeT<false>, 9> times_ep1_offline;
  /* 0060 */ uint8_t grave_is_ep2 = 0;
  /* 0061 */ uint8_t grave_stage_num = 0;
  /* 0062 */ uint8_t grave_floor = 0;
  /* 0063 */ uint8_t unknown_g0 = 0;
  /* 0064 */ le_uint16_t grave_deaths = 0;
  /* 0066 */ parray<uint8_t, 2> unknown_u4;
  /* 0068 */ le_uint32_t grave_time = 0; // Encoded as in PlayerRecordsChallengeDCPC
  /* 006C */ le_uint32_t grave_defeated_by_enemy_rt_index = 0;
  /* 0070 */ le_float grave_x = 0.0f;
  /* 0074 */ le_float grave_y = 0.0f;
  /* 0078 */ le_float grave_z = 0.0f;
  /* 007C */ pstring<TextEncoding::UTF16, 0x14> grave_team;
  /* 00A4 */ pstring<TextEncoding::UTF16, 0x20> grave_message;
  /* 00E4 */ parray<uint8_t, 4> unknown_m5;
  /* 00E8 */ parray<le_uint32_t, 3> unknown_t6;
  /* 00F4 */ ChallengeAwardStateT<false> ep1_online_award_state;
  /* 00FC */ ChallengeAwardStateT<false> ep2_online_award_state;
  /* 0104 */ ChallengeAwardStateT<false> ep1_offline_award_state;
  /* 010C */ pstring<TextEncoding::CHALLENGE16, 0x0C> rank_title;
  /* 0124 */ parray<uint8_t, 0x1C> unknown_l7;
  /* 0140 */

  PlayerRecordsChallengeBB() = default;
  PlayerRecordsChallengeBB(const PlayerRecordsChallengeBB& other) = default;
  PlayerRecordsChallengeBB& operator=(const PlayerRecordsChallengeBB& other) = default;

  PlayerRecordsChallengeBB(const PlayerRecordsChallengeDC& rec);
  PlayerRecordsChallengeBB(const PlayerRecordsChallengePC& rec);

  template <bool BE>
  PlayerRecordsChallengeBB(const PlayerRecordsChallengeV3T<BE>& rec)
      : title_color(rec.stats.title_color.load()),
        unknown_u0(rec.stats.unknown_u0),
        times_ep1_online(rec.stats.times_ep1_online),
        times_ep2_online(rec.stats.times_ep2_online),
        times_ep1_offline(rec.stats.times_ep1_offline),
        grave_is_ep2(rec.stats.grave_is_ep2),
        grave_stage_num(rec.stats.grave_stage_num),
        grave_floor(rec.stats.grave_floor),
        unknown_g0(rec.stats.unknown_g0),
        grave_deaths(rec.stats.grave_deaths.load()),
        unknown_u4(rec.stats.unknown_u4),
        grave_time(rec.stats.grave_time.load()),
        grave_defeated_by_enemy_rt_index(rec.stats.grave_defeated_by_enemy_rt_index.load()),
        grave_x(rec.stats.grave_x.load()),
        grave_y(rec.stats.grave_y.load()),
        grave_z(rec.stats.grave_z.load()),
        grave_team(rec.stats.grave_team.decode(), 1),
        grave_message(rec.stats.grave_message.decode(), 1),
        unknown_m5(rec.stats.unknown_m5),
        ep1_online_award_state(rec.stats.ep1_online_award_state),
        ep2_online_award_state(rec.stats.ep2_online_award_state),
        ep1_offline_award_state(rec.stats.ep1_offline_award_state),
        rank_title(rec.rank_title.decode(), 1),
        unknown_l7(rec.unknown_l7) {
    for (size_t z = 0; z < std::min<size_t>(this->unknown_t6.size(), rec.stats.unknown_t6.size()); z++) {
      this->unknown_t6[z] = rec.stats.unknown_t6[z].load();
    }
  }

  operator PlayerRecordsChallengeDC() const;
  operator PlayerRecordsChallengePC() const;
  template <bool BE>
  operator PlayerRecordsChallengeV3T<BE>() const {
    PlayerRecordsChallengeV3T<BE> ret;
    ret.stats.title_color = this->title_color.load();
    ret.stats.unknown_u0 = this->unknown_u0;
    ret.stats.times_ep1_online = this->times_ep1_online;
    ret.stats.times_ep2_online = this->times_ep2_online;
    ret.stats.times_ep1_offline = this->times_ep1_offline;
    ret.stats.grave_is_ep2 = this->grave_is_ep2;
    ret.stats.grave_stage_num = this->grave_stage_num;
    ret.stats.grave_floor = this->grave_floor;
    ret.stats.unknown_g0 = this->unknown_g0;
    ret.stats.grave_deaths = this->grave_deaths.load();
    ret.stats.unknown_u4 = this->unknown_u4;
    ret.stats.grave_time = this->grave_time.load();
    ret.stats.grave_defeated_by_enemy_rt_index = this->grave_defeated_by_enemy_rt_index.load();
    ret.stats.grave_x = this->grave_x.load();
    ret.stats.grave_y = this->grave_y.load();
    ret.stats.grave_z = this->grave_z.load();
    ret.stats.grave_team.encode(this->grave_team.decode(), 1);
    ret.stats.grave_message.encode(this->grave_message.decode(), 1);
    ret.stats.unknown_m5 = this->unknown_m5;
    for (size_t z = 0; z < std::min<size_t>(ret.stats.unknown_t6.size(), this->unknown_t6.size()); z++) {
      ret.stats.unknown_t6[z] = this->unknown_t6[z].load();
    }
    ret.stats.ep1_online_award_state = this->ep1_online_award_state;
    ret.stats.ep2_online_award_state = this->ep2_online_award_state;
    ret.stats.ep1_offline_award_state = this->ep1_offline_award_state;
    ret.rank_title.encode(this->rank_title.decode(), 1);
    ret.unknown_l7 = this->unknown_l7;
    return ret;
  }
} __packed_ws__(PlayerRecordsChallengeBB, 0x140);

template <bool BE>
struct PlayerRecordsBattleT {
  // On Episode 3, place_counts[0] is win count and [1] is loss count
  /* 00 */ parray<U16T<BE>, 4> place_counts;
  /* 08 */ U16T<BE> disconnect_count = 0;
  /* 0A */ parray<U16T<BE>, 2> unknown_a1;
  /* 0E */ parray<uint8_t, 2> unused;
  /* 10 */ parray<U32T<BE>, 2> unknown_a2;
  /* 18 */

  operator PlayerRecordsBattleT<!BE>() const {
    PlayerRecordsBattleT<!BE> ret;
    for (size_t z = 0; z < this->place_counts.size(); z++) {
      ret.place_counts[z] = this->place_counts[z].load();
    }
    ret.disconnect_count = this->disconnect_count.load();
    for (size_t z = 0; z < this->unknown_a1.size(); z++) {
      ret.unknown_a1[z] = this->unknown_a1[z].load();
    }
    for (size_t z = 0; z < this->unknown_a2.size(); z++) {
      ret.unknown_a2[z] = this->unknown_a2[z].load();
    }
    return ret;
  }
} __packed__;
using PlayerRecordsBattle = PlayerRecordsBattleT<false>;
using PlayerRecordsBattleBE = PlayerRecordsBattleT<true>;
check_struct_size(PlayerRecordsBattle, 0x18);
check_struct_size(PlayerRecordsBattleBE, 0x18);

template <typename DestT, typename SrcT = DestT>
DestT convert_player_disp_data(const SrcT&, uint8_t, uint8_t) {
  static_assert(phosg::always_false<DestT, SrcT>::v,
      "unspecialized convert_player_disp_data should never be called");
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3>(const PlayerDispDataDCPCV3& src, uint8_t, uint8_t) {
  return src;
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(
    const PlayerDispDataBB& src, uint8_t to_language, uint8_t from_language) {
  return src.to_dcpcv3<false>(to_language, from_language);
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
} __packed_ws__(QuestFlagsForDifficulty, 0x80);

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
} __packed_ws__(QuestFlags, 0x200);

struct QuestFlagsV1 {
  parray<QuestFlagsForDifficulty, 3> data;

  QuestFlagsV1& operator=(const QuestFlags& other);
  operator QuestFlags() const;
} __packed_ws__(QuestFlagsV1, 0x180);

struct SwitchFlags {
  parray<parray<uint8_t, 0x20>, 0x12> data;

  inline bool get(uint8_t floor, uint16_t flag_num) const {
    return this->data[floor][flag_num >> 3] & (0x80 >> (flag_num & 7));
  }
  inline void set(uint8_t floor, uint16_t flag_num) {
    this->data[floor][flag_num >> 3] |= (0x80 >> (flag_num & 7));
  }
  inline void clear(uint8_t floor, uint16_t flag_num) {
    this->data[floor][flag_num >> 3] &= ~(0x80 >> (flag_num & 7));
  }
} __packed_ws__(SwitchFlags, 0x240);

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
  enum class RespawnMode : uint8_t {
    ALLOW = 0,
    FORBID = 1,
    LIMIT_LIVES = 2,
  };

  // Set by quest opcode F812, but values are remapped
  /* 00 */ TechDiskMode tech_disk_mode = TechDiskMode::ALLOW;
  // Set by quest opcode F813, but values are remapped
  /* 01 */ WeaponAndArmorMode weapon_and_armor_mode = WeaponAndArmorMode::ALLOW;
  // Set by quest opcode F814, but values are remapped
  /* 02 */ MagMode mag_mode = MagMode::ALLOW;
  // Set by quest opcode F815, but values are remapped
  /* 03 */ ToolMode tool_mode = ToolMode::ALLOW;
  // Set by quest opcode F816. Values are not remapped
  /* 04 */ TrapMode trap_mode = TrapMode::DEFAULT;
  // Set by quest opcode F817. Value appears to be unused in all PSO versions.
  /* 05 */ uint8_t unused_F817 = 0;
  // Set by quest opcode F818, but values are remapped
  /* 06 */ RespawnMode respawn_mode = RespawnMode::ALLOW;
  // Set by quest opcode F819
  /* 07 */ uint8_t replace_char = 0;
  // Set by quest opcode F81A, but value is inverted
  /* 08 */ uint8_t drop_weapon = 0;
  // Set by quest opcode F81B
  /* 09 */ uint8_t is_teams = 0;
  // Set by quest opcode F852
  /* 0A */ uint8_t hide_target_reticle = 0;
  // Set by quest opcode F81E. Values are not remapped
  /* 0B */ MesetaMode meseta_mode = MesetaMode::ALLOW;
  // Set by quest opcode F81D
  /* 0C */ uint8_t death_level_up = 0;
  // Set by quest opcode F851. The trap type is remapped
  /* 0D */ parray<uint8_t, 4> trap_counts;
  // Set by quest opcode F85E
  /* 11 */ uint8_t enable_sonar = 0;
  // Set by quest opcode F85F
  /* 12 */ uint8_t sonar_count = 0;
  // Set by quest opcode F89E
  /* 13 */ uint8_t forbid_scape_dolls = 0;
  // This value does not appear to be set by any quest opcode
  /* 14 */ le_uint32_t unknown_a1 = 0;
  // Set by quest opcode F86F
  /* 18 */ le_uint32_t lives = 0;
  // Set by quest opcode F870
  /* 1C */ le_uint32_t max_tech_level = 0;
  // Set by quest opcode F871
  /* 20 */ le_uint32_t char_level = 0;
  // Set by quest opcode F872
  /* 24 */ le_uint32_t time_limit = 0;
  // Set by quest opcode F8A8
  /* 28 */ le_uint16_t death_tech_level_up = 0;
  /* 2A */ parray<uint8_t, 2> unused;
  // Set by quest opcode F86B
  /* 2C */ le_uint32_t box_drop_area = 0;
  /* 30 */

  BattleRules() = default;
  explicit BattleRules(const phosg::JSON& json);
  phosg::JSON json() const;

  bool operator==(const BattleRules& other) const = default;
  bool operator!=(const BattleRules& other) const = default;
} __packed_ws__(BattleRules, 0x30);

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

struct SymbolChatFacePart {
  uint8_t type = 0xFF; // FF = no part in this slot
  uint8_t x = 0;
  uint8_t y = 0;
  // Bits: ------VH (V = reverse vertical, H = reverse horizontal)
  uint8_t flags = 0;
} __packed_ws__(SymbolChatFacePart, 4);

template <bool BE>
struct SymbolChatT {
  // Bits: ----------------------DMSSSCCCFF
  //   S = sound, C = face color, F = face shape, D = capture, M = mute sound
  /* 00 */ U32T<BE> spec = 0;

  // Corner objects are specified in reading order ([0] is the top-left one).
  // Bits (each entry): ---VHCCCZZZZZZZZ
  //   V = reverse vertical, H = reverse horizontal, C = color, Z = object
  // If Z is all 1 bits (0xFF), no corner object is rendered.
  /* 04 */ parray<U16T<BE>, 4> corner_objects;
  /* 0C */ parray<SymbolChatFacePart, 12> face_parts;
  /* 3C */

  SymbolChatT()
      : spec(0),
        corner_objects(0x00FF),
        face_parts() {}

  operator SymbolChatT<!BE>() const {
    SymbolChatT<!BE> ret;
    ret.spec = this->spec.load();
    for (size_t z = 0; z < this->corner_objects.size(); z++) {
      ret.corner_objects[z] = this->corner_objects[z].load();
    }
    ret.face_parts = this->face_parts;
    return ret;
  }
} __packed__;
using SymbolChat = SymbolChatT<false>;
using SymbolChatBE = SymbolChatT<true>;
check_struct_size(SymbolChat, 0x3C);
check_struct_size(SymbolChatBE, 0x3C);

extern const QuestFlagsForDifficulty bb_quest_flag_apply_mask;
