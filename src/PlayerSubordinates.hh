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
#include "CommonFileFormats.hh"
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
  // Some NPCs can crash the client if the character's class is incorrect. To
  // handle this, we save the affected fields in the unused bytes after
  // extra_model. This is a newserv-specific extension; it appears the
  // following 15 bytes were simply never used by Sega.
  /* 1D */ uint8_t npc_saved_data_type = 0;
  /* 1E */ uint8_t npc_saved_costume = 0;
  /* 1F */ uint8_t npc_saved_skin = 0;
  /* 20 */ uint8_t npc_saved_face = 0;
  /* 21 */ uint8_t npc_saved_head = 0;
  /* 22 */ uint8_t npc_saved_hair = 0;
  /* 23 */ uint8_t npc_saved_hair_r = 0;
  /* 24 */ uint8_t npc_saved_hair_g = 0;
  /* 25 */ uint8_t npc_saved_hair_b = 0;
  /* 26 */ parray<uint8_t, 2> unused;
  /* 28 */ F32T<BE> npc_saved_proportion_y = 0.0;
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

  void backup_npc_saved_fields() {
    if (this->npc_saved_data_type == 0x8E) {
      return;
    }

    // Restore old-format data if needed before backing up again
    this->restore_npc_saved_fields();

    this->npc_saved_data_type = 0x8E;
    this->npc_saved_costume = this->costume;
    this->npc_saved_skin = this->skin;
    this->npc_saved_face = this->face;
    this->npc_saved_head = this->head;
    this->npc_saved_hair = this->hair;
    this->npc_saved_hair_r = this->hair_r;
    this->npc_saved_hair_g = this->hair_g;
    this->npc_saved_hair_b = this->hair_b;
    this->npc_saved_proportion_y = this->proportion_y;
    this->costume = 0;
    this->skin = 0;
    this->face = 0;
    this->head = 0;
    this->hair = 0;
    this->hair_r = 0;
    this->hair_g = 0;
    this->hair_b = 0;
    this->proportion_y = 0;
  }

  void restore_npc_saved_fields() {
    switch (this->npc_saved_data_type) {
      case 0x00:
        break;
      case 0x8D: // Old format
        this->char_class = this->npc_saved_costume;
        this->head = this->npc_saved_skin;
        this->hair = this->npc_saved_face;
        break;
      case 0x8E: // New format
        this->costume = this->npc_saved_costume;
        this->skin = this->npc_saved_skin;
        this->face = this->npc_saved_face;
        this->head = this->npc_saved_head;
        this->hair = this->npc_saved_hair;
        this->hair_r = this->npc_saved_hair_r;
        this->hair_g = this->npc_saved_hair_g;
        this->hair_b = this->npc_saved_hair_b;
        this->proportion_y = this->npc_saved_proportion_y;
        break;
      default:
        throw std::runtime_error("unknown saved NPC data format");
    }

    this->npc_saved_data_type = 0;
    this->npc_saved_costume = 0;
    this->npc_saved_skin = 0;
    this->npc_saved_face = 0;
    this->npc_saved_head = 0;
    this->npc_saved_hair = 0;
    this->npc_saved_hair_r = 0;
    this->npc_saved_hair_g = 0;
    this->npc_saved_hair_b = 0;
    this->unused.clear(0);
    this->npc_saved_proportion_y = 0.0;
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
    ret.name_color = this->name_color;
    ret.extra_model = this->extra_model;
    ret.unused = this->unused;
    ret.name_color_checksum = this->name_color_checksum;
    ret.section_id = this->section_id;
    ret.char_class = this->char_class;
    ret.validation_flags = this->validation_flags;
    ret.version = this->version;
    ret.class_flags = this->class_flags;
    ret.costume = this->costume;
    ret.skin = this->skin;
    ret.face = this->face;
    ret.head = this->head;
    ret.hair = this->hair;
    ret.hair_r = this->hair_r;
    ret.hair_g = this->hair_g;
    ret.hair_b = this->hair_b;
    ret.proportion_x = this->proportion_x;
    ret.proportion_y = this->proportion_y;
    return ret;
  }
} __attribute__((packed));
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

  PlayerDispDataBB to_bb(Language to_language, Language from_language) const;
} __attribute__((packed));
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
  PlayerDispDataDCPCV3T<BE> to_dcpcv3(Language to_language, Language from_language) const {
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
PlayerDispDataBB PlayerDispDataDCPCV3T<BE>::to_bb(Language to_language, Language from_language) const {
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
  /* 78 */ Language language = Language::JAPANESE;
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
  /* 7A */ Language language = Language::JAPANESE;
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
  /* ED */ Language language = Language::JAPANESE;
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
  /* A0:8C */ uint8_t present = 0;
  /* A1:8D */ Language language = Language::JAPANESE;
  /* A2:8E */ uint8_t section_id = 0;
  /* A3:8F */ uint8_t char_class = 0;
  /* A4:90 */

  operator GuildCardBB() const;
} __attribute__((packed));
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
  /* 0229 */ Language language = Language::JAPANESE;
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
  /* 0105 */ Language language = Language::JAPANESE;
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
    ret.guild_card_number = this->guild_card_number;
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
  ret.guild_card_number = this->guild_card_number;
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
  // The remainder of this struct appears to be private/opaque in the XDK (and
  // newserv doesn't use it either)
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
    ret.rank_award_flags = this->rank_award_flags;
    ret.maximum_rank = this->maximum_rank;
    return ret;
  }
} __attribute__((packed));
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
} __attribute__((packed));
using PlayerRecordsChallengeDC = PlayerRecordsChallengeDCPCT<TextEncoding::ASCII, TextEncoding::CHALLENGE8>;
using PlayerRecordsChallengePC = PlayerRecordsChallengeDCPCT<TextEncoding::UTF16, TextEncoding::CHALLENGE16>;
check_struct_size(PlayerRecordsChallengeDC, 0xA0);
check_struct_size(PlayerRecordsChallengePC, 0xD8);

template <bool BE>
struct PlayerRecordsChallengeV3T {
  // Offsets are (1) relative to start of C5 entry, and (2) relative to start
  // of save file structure
  /* 0000:001C */ U16T<BE> title_color = 0x7FFF; // XRGB1555
  /* 0002:001E */ parray<uint8_t, 2> unknown_u0;
  /* 0004:0020 */ parray<ChallengeTimeT<BE>, 9> times_ep1_online;
  /* 0028:0044 */ parray<ChallengeTimeT<BE>, 5> times_ep2_online;
  /* 003C:0058 */ parray<ChallengeTimeT<BE>, 9> times_ep1_offline;
  /* 0060:007C */ uint8_t grave_is_ep2 = 0;
  /* 0061:007D */ uint8_t grave_stage_num = 0;
  /* 0062:007E */ uint8_t grave_floor = 0;
  /* 0063:007F */ uint8_t unknown_g0 = 0;
  /* 0064:0080 */ U16T<BE> grave_deaths = 0;
  /* 0066:0082 */ parray<uint8_t, 2> unknown_u4;
  /* 0068:0084 */ U32T<BE> grave_time = 0; // Encoded as in PlayerRecordsChallengeDCPC
  /* 006C:0088 */ U32T<BE> grave_defeated_by_enemy_rt_index = 0;
  /* 0070:008C */ F32T<BE> grave_x = 0.0f;
  /* 0074:0090 */ F32T<BE> grave_y = 0.0f;
  /* 0078:0094 */ F32T<BE> grave_z = 0.0f;
  /* 007C:0098 */ pstring<TextEncoding::ASCII, 0x14> grave_team;
  /* 0090:00AC */ pstring<TextEncoding::ASCII, 0x20> grave_message;
  /* 00B0:00CC */ parray<uint8_t, 4> unknown_m5;
  /* 00B4:00D0 */ parray<U32T<BE>, 3> unknown_t6;
  /* 00C0:00DC */ ChallengeAwardStateT<BE> ep1_online_award_state;
  /* 00C8:00E4 */ ChallengeAwardStateT<BE> ep2_online_award_state;
  /* 00D0:00EC */ ChallengeAwardStateT<BE> ep1_offline_award_state;
  /* 00D8:00F4 */
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
using PlayerRecordsChallengeV3 = PlayerRecordsChallengeV3T<false>;
using PlayerRecordsChallengeV3BE = PlayerRecordsChallengeV3T<true>;
check_struct_size(PlayerRecordsChallengeV3, 0x100);
check_struct_size(PlayerRecordsChallengeV3BE, 0x100);

struct PlayerRecordsChallengeEp3 {
  /* 00:1C */ be_uint16_t title_color = 0x7FFF; // XRGB1555
  /* 02:1E */ parray<uint8_t, 2> unknown_u0;
  /* 04:20 */ parray<ChallengeTimeT<true>, 9> times_ep1_online;
  /* 28:44 */ parray<ChallengeTimeT<true>, 5> times_ep2_online;
  /* 3C:58 */ parray<ChallengeTimeT<true>, 9> times_ep1_offline;
  /* 60:7C */ uint8_t grave_is_ep2 = 0;
  /* 61:7D */ uint8_t grave_stage_num = 0;
  /* 62:7E */ uint8_t grave_floor = 0;
  /* 63:7F */ uint8_t unknown_g0 = 0;
  /* 64:80 */ be_uint16_t grave_deaths = 0;
  /* 66:82 */ parray<uint8_t, 2> unknown_u4;
  /* 68:84 */ be_uint32_t grave_time = 0; // Encoded as in PlayerRecordsChallengeDCPC
  /* 6C:88 */ be_uint32_t grave_defeated_by_enemy_rt_index = 0;
  /* 70:8C */ be_float grave_x = 0.0f;
  /* 74:90 */ be_float grave_y = 0.0f;
  /* 78:94 */ be_float grave_z = 0.0f;
  /* 7C:98 */ pstring<TextEncoding::ASCII, 0x14> grave_team;
  /* 90:AC */ pstring<TextEncoding::ASCII, 0x20> grave_message;
  /* B0:CC */ parray<uint8_t, 4> unknown_m5;
  /* B4:D0 */ parray<be_uint32_t, 3> unknown_t6;
  /* C0:DC */ ChallengeAwardStateT<true> ep1_online_award_state;
  /* C8:E4 */ ChallengeAwardStateT<true> ep2_online_award_state;
  /* D0:EC */ ChallengeAwardStateT<true> ep1_offline_award_state;
  /* D8:F4 */
} __attribute__((packed));
check_struct_size(PlayerRecordsChallengeEp3, 0xD8);

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
      : title_color(rec.title_color),
        unknown_u0(rec.unknown_u0),
        times_ep1_online(rec.times_ep1_online),
        times_ep2_online(rec.times_ep2_online),
        times_ep1_offline(rec.times_ep1_offline),
        grave_is_ep2(rec.grave_is_ep2),
        grave_stage_num(rec.grave_stage_num),
        grave_floor(rec.grave_floor),
        unknown_g0(rec.unknown_g0),
        grave_deaths(rec.grave_deaths),
        unknown_u4(rec.unknown_u4),
        grave_time(rec.grave_time),
        grave_defeated_by_enemy_rt_index(rec.grave_defeated_by_enemy_rt_index),
        grave_x(rec.grave_x),
        grave_y(rec.grave_y),
        grave_z(rec.grave_z),
        grave_team(rec.grave_team.decode(), Language::ENGLISH),
        grave_message(rec.grave_message.decode(), Language::ENGLISH),
        unknown_m5(rec.unknown_m5),
        ep1_online_award_state(rec.ep1_online_award_state),
        ep2_online_award_state(rec.ep2_online_award_state),
        ep1_offline_award_state(rec.ep1_offline_award_state),
        rank_title(rec.rank_title.decode(), Language::ENGLISH),
        unknown_l7(rec.unknown_l7) {
    for (size_t z = 0; z < std::min<size_t>(this->unknown_t6.size(), rec.unknown_t6.size()); z++) {
      this->unknown_t6[z] = rec.unknown_t6[z];
    }
  }

  operator PlayerRecordsChallengeDC() const;
  operator PlayerRecordsChallengePC() const;
  template <bool BE>
  operator PlayerRecordsChallengeV3T<BE>() const {
    PlayerRecordsChallengeV3T<BE> ret;
    ret.title_color = this->title_color;
    ret.unknown_u0 = this->unknown_u0;
    ret.times_ep1_online = this->times_ep1_online;
    ret.times_ep2_online = this->times_ep2_online;
    ret.times_ep1_offline = this->times_ep1_offline;
    ret.grave_is_ep2 = this->grave_is_ep2;
    ret.grave_stage_num = this->grave_stage_num;
    ret.grave_floor = this->grave_floor;
    ret.unknown_g0 = this->unknown_g0;
    ret.grave_deaths = this->grave_deaths;
    ret.unknown_u4 = this->unknown_u4;
    ret.grave_time = this->grave_time;
    ret.grave_defeated_by_enemy_rt_index = this->grave_defeated_by_enemy_rt_index;
    ret.grave_x = this->grave_x;
    ret.grave_y = this->grave_y;
    ret.grave_z = this->grave_z;
    ret.grave_team.encode(this->grave_team.decode(), Language::ENGLISH);
    ret.grave_message.encode(this->grave_message.decode(), Language::ENGLISH);
    ret.unknown_m5 = this->unknown_m5;
    for (size_t z = 0; z < std::min<size_t>(ret.unknown_t6.size(), this->unknown_t6.size()); z++) {
      ret.unknown_t6[z] = this->unknown_t6[z];
    }
    ret.ep1_online_award_state = this->ep1_online_award_state;
    ret.ep2_online_award_state = this->ep2_online_award_state;
    ret.ep1_offline_award_state = this->ep1_offline_award_state;
    ret.rank_title.encode(this->rank_title.decode(), Language::ENGLISH);
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
      ret.place_counts[z] = this->place_counts[z];
    }
    ret.disconnect_count = this->disconnect_count;
    for (size_t z = 0; z < this->unknown_a1.size(); z++) {
      ret.unknown_a1[z] = this->unknown_a1[z];
    }
    for (size_t z = 0; z < this->unknown_a2.size(); z++) {
      ret.unknown_a2[z] = this->unknown_a2[z];
    }
    return ret;
  }
} __attribute__((packed));
using PlayerRecordsBattle = PlayerRecordsBattleT<false>;
using PlayerRecordsBattleBE = PlayerRecordsBattleT<true>;
check_struct_size(PlayerRecordsBattle, 0x18);
check_struct_size(PlayerRecordsBattleBE, 0x18);

template <typename DestT, typename SrcT = DestT>
DestT convert_player_disp_data(const SrcT&, Language, Language) {
  static_assert(phosg::always_false<DestT, SrcT>::v,
      "unspecialized convert_player_disp_data should never be called");
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3>(const PlayerDispDataDCPCV3& src, Language, Language) {
  return src;
}

template <>
inline PlayerDispDataDCPCV3 convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(
    const PlayerDispDataBB& src, Language to_language, Language from_language) {
  return src.to_dcpcv3<false>(to_language, from_language);
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB, PlayerDispDataDCPCV3>(
    const PlayerDispDataDCPCV3& src, Language to_language, Language from_language) {
  return src.to_bb(to_language, from_language);
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB>(
    const PlayerDispDataBB& src, Language, Language) {
  return src;
}

template <size_t NumFlags>
struct FlagsArray {
  parray<uint8_t, (NumFlags >> 3)> data = 0;

  FlagsArray() = default;
  FlagsArray(const FlagsArray& other) = default;
  FlagsArray(FlagsArray&& other) = default;
  FlagsArray& operator=(const FlagsArray& other) = default;
  FlagsArray& operator=(FlagsArray&& other) = default;

  FlagsArray(std::initializer_list<uint8_t> init_items) : data(init_items) {}

  template <size_t OtherNumFlags>
  explicit FlagsArray(const FlagsArray<OtherNumFlags>& other) : data(other.data) {}
  template <size_t OtherNumFlags>
  FlagsArray& operator=(const FlagsArray<OtherNumFlags>& other) {
    this->data = other.data;
    return *this;
  }

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

template <size_t NumFlagsPerTable, size_t NumTables, typename TableIndexT = size_t>
struct FlagsTable {
  parray<FlagsArray<NumFlagsPerTable>, NumTables> data;

  FlagsTable() = default;
  FlagsTable(const FlagsTable& other) = default;
  FlagsTable(FlagsTable&& other) = default;
  FlagsTable& operator=(const FlagsTable& other) = default;
  FlagsTable& operator=(FlagsTable&& other) = default;

  template <size_t OtherNumFlagsPerTable, size_t OtherNumTables>
  explicit FlagsTable(const FlagsTable<OtherNumFlagsPerTable, OtherNumTables, TableIndexT>& other) : data(other.data) {}
  template <size_t OtherNumFlagsPerTable, size_t OtherNumTables>
  FlagsTable& operator=(const FlagsTable<OtherNumFlagsPerTable, OtherNumTables, TableIndexT>& other) {
    this->data = other.data;
    return *this;
  }

  inline FlagsArray<NumFlagsPerTable>& array(TableIndexT which) {
    return this->data[static_cast<size_t>(which)];
  }
  inline const FlagsArray<NumFlagsPerTable>& array(TableIndexT which) const {
    return this->data[static_cast<size_t>(which)];
  }

  inline bool get(TableIndexT array_index, size_t flag_index) const {
    return this->array(array_index).get(flag_index);
  }
  inline void set(TableIndexT array_index, size_t flag_index) {
    this->array(array_index).set(flag_index);
  }
  inline void clear(TableIndexT array_index, size_t flag_index) {
    this->array(array_index).clear(flag_index);
  }
  inline void update_all(TableIndexT array_index, bool set) {
    this->array(array_index).update_all(set);
  }
  inline void update_all(bool set) {
    for (size_t z = 0; z < this->data.size(); z++) {
      this->update_all(z, set);
    }
  }
} __attribute__((packed));

using QuestFlagsForDifficulty = FlagsArray<0x400>;
using QuestFlagsV1 = FlagsTable<0x400, 3, Difficulty>;
using QuestFlags = FlagsTable<0x400, 4, Difficulty>;
using Ep3SeqVars = FlagsArray<0x2000>;
using SwitchFlagsV1 = FlagsTable<0x100, 0x10>;
using SwitchFlags = FlagsTable<0x100, 0x12>;
static_assert(sizeof(QuestFlagsForDifficulty) == 0x80);
static_assert(sizeof(QuestFlagsV1) == 0x180);
static_assert(sizeof(QuestFlags) == 0x200);
static_assert(sizeof(Ep3SeqVars) == 0x400);
static_assert(sizeof(SwitchFlagsV1) == 0x200);
static_assert(sizeof(SwitchFlags) == 0x240);

extern const QuestFlagsForDifficulty BB_QUEST_FLAG_APPLY_MASK;

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
    ret.spec = this->spec;
    for (size_t z = 0; z < this->corner_objects.size(); z++) {
      ret.corner_objects[z] = this->corner_objects[z];
    }
    ret.face_parts = this->face_parts;
    return ret;
  }
} __attribute__((packed));
using SymbolChat = SymbolChatT<false>;
using SymbolChatBE = SymbolChatT<true>;
check_struct_size(SymbolChat, 0x3C);
check_struct_size(SymbolChatBE, 0x3C);

struct TelepipeState {
  /* 00 */ le_uint16_t owner_client_id = 0xFFFF;
  /* 02 */ le_uint16_t floor = 0;
  /* 04 */ le_uint32_t room_id = 0;
  /* 08 */ VectorXYZF pos;
  /* 14 */ le_uint32_t angle_y = 0;
  /* 18 */
} __packed_ws__(TelepipeState, 0x18);

struct PlayerHoldState_DCProtos {
  // This is used in all versions of this command except DCNTE and 11/2000.
  /* 00 */ le_uint16_t unknown_a1 = 0;
  /* 02 */ le_uint16_t unknown_a2 = 0;
  // unknown_a3 is missing in this format, unlike the v1+ format below
  /* 04 */ le_float trigger_radius2 = 0.0f;
  /* 08 */ le_float x = 0.0f;
  /* 0C */ le_float z = 0.0f;
  /* 10 */
} __packed_ws__(PlayerHoldState_DCProtos, 0x10);

struct PlayerHoldState {
  // This is used in all versions of this command except DCNTE and 11/2000.
  /* 00 */ le_uint16_t unknown_a1 = 0;
  /* 02 */ le_uint16_t unknown_a2 = 0;
  /* 04 */ le_uint32_t unknown_a3 = 0;
  /* 08 */ le_float trigger_radius2 = 0.0f;
  /* 0C */ le_float x = 0.0f;
  /* 10 */ le_float z = 0.0f;
  /* 14 */

  PlayerHoldState() = default;
  PlayerHoldState(const PlayerHoldState_DCProtos& proto);
  operator PlayerHoldState_DCProtos() const;
} __packed_ws__(PlayerHoldState, 0x14);

struct StatusEffectState {
  /* 00 */ le_uint32_t effect_type = 0;
  /* 04 */ le_float multiplier = 0.0f;
  /* 08 */ le_uint32_t remaining_frames = 0;
  /* 0C */
} __packed_ws__(StatusEffectState, 0x0C);
