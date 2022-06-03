#pragma once

#include <stdint.h>

#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <phosg/Encoding.hh>

#include "Text.hh"



// Note: Much of the structures and enums here are based on the card list file,
// and comparing the card text with the data in the file. Some inferences may be
// incorrect here, since Episode 3's card text is wrong in various places.

struct Ep3CardStats {
  enum Rarity : uint8_t {
    N1    = 0x01,
    R1    = 0x02,
    S     = 0x03,
    E     = 0x04,
    N2    = 0x05,
    N3    = 0x06,
    N4    = 0x07,
    R2    = 0x08,
    R3    = 0x09,
    R4    = 0x0A,
    SS    = 0x0B,
    D1    = 0x0C,
    D2    = 0x0D,
    INVIS = 0x0E,
  };

  enum Type : uint8_t {
    SC_HUNTERS = 0x00, // No subtypes
    SC_ARKZ    = 0x01, // No subtypes
    ITEM       = 0x02, // Subtype 01 = sword, 02 = gun, 03 = cane. TODO: there are many more subtypes than those 3
    CREATURE   = 0x03, // No subtypes (TODO: Where are attributes stored then?)
    ACTION     = 0x04, // TODO: What do the subtypes mean? Are they actually flags instead?
    ASSIST     = 0x05, // No subtypes
  };

  struct Stat {
    enum Type : uint8_t {
      BLANK = 0,
      STAT = 1,
      PLUS_STAT = 2,
      MINUS_STAT = 3,
      EQUALS_STAT = 4,
      UNKNOWN = 5,
      PLUS_UNKNOWN = 6,
      MINUS_UNKNOWN = 7,
      EQUALS_UNKNOWN = 8,
    };
    be_uint16_t code;
    Type type;
    int8_t stat;

    void decode_code();
    std::string str() const;
  } __attribute__((packed));

  struct Effect {
    uint8_t command;
    ptext<char, 0x0F> expr; // May be blank if the command doesn't use it
    uint8_t when;
    ptext<char, 4> arg1;
    ptext<char, 4> arg2;
    ptext<char, 4> arg3;
    parray<uint8_t, 3> unknown_a3;

    bool is_empty() const;
    static std::string str_for_arg(const std::string& arg);
    std::string str() const;
  } __attribute__((packed));

  be_uint32_t card_id;
  parray<uint8_t, 0x40> jp_name;
  int8_t type; // Type enum. If <0, then this is the end of the card list
  uint8_t self_cost; // ATK dice points required
  uint8_t ally_cost; // ATK points from allies required; PBs use this
  uint8_t unused_a0; // Always 0
  Stat hp;
  Stat ap;
  Stat tp;
  Stat mv;
  parray<uint8_t, 8> left_colors;
  parray<uint8_t, 8> right_colors;
  parray<uint8_t, 8> top_colors;
  parray<be_uint32_t, 6> range;
  be_uint32_t unused_a1; // Always 0
  // Target modes:
  // 00 = no targeting (used for defense cards, mags, shields, etc.)
  // 01 = single enemy
  // 02 = multiple enemies (with range)
  // 03 = self (assist)
  // 04 = team (assist)
  // 05 = everyone (assist)
  // 06 = multiple allies (with range); only used by Shifta
  // 07 = all allies including yourself; see Anti, Resta, Leilla
  // 08 = all (attack); see e.g. Last Judgment, Earthquake
  // 09 = your own FCs but not SCs; see Traitor
  uint8_t target_mode;
  uint8_t assist_turns; // 90 = once, 99 = forever
  uint8_t cannot_move; // 0 for SC and creature cards; 1 for everything else
  uint8_t cannot_attack; // 1 for shields, mags, defense actions, and assist cards
  uint8_t unused_a2; // Always 0
  uint8_t hide_in_deck_edit; // 0 = player can use this card (appears in deck edit)
  uint8_t subtype; // e.g. gun, sword, etc. (used for checking if SCs can use it)
  uint8_t rarity; // Rarity enum
  be_uint32_t unknown_a2;
  // These two fields seem to always contain the same value, and are always 0
  // for non-assist cards and nonzero for assists. Each assist card has a unique
  // value here and no effects, which makes it look like this is how assist
  // effects are implemented. There seems to be some 1k-modulation going on here
  // too; most cards are in the range 101-174 but a few have e.g. 1150, 2141. A
  // few pairs of cards have the same effect, which makes it look like some
  // other fields are also involved in determining their effects (see e.g. Skip
  // Draw / Skip Move, Dice Fever / Dice Fever +, Reverse Card / Rich +).
  parray<be_uint16_t, 2> assist_effect;
  parray<be_uint16_t, 2> unknown_a3;
  ptext<char, 0x14> name;
  ptext<char, 0x0B> jp_short_name;
  ptext<char, 0x07> short_name;
  be_uint16_t has_effects; // 1 if any of the following structs are not blank
  Effect effects[3];

  void decode_range();
  std::string str() const;
} __attribute__((packed)); // 0x128 bytes in total

struct Ep3CardStatsFooter {
  be_uint32_t num_cards1;
  be_uint32_t unknown_a1;
  be_uint32_t num_cards2;
  be_uint32_t unknown_a2[11];
  be_uint32_t unknown_offset_a3;
  be_uint32_t unknown_a4[3];
  be_uint32_t footer_offset;
  be_uint32_t unknown_a5[3];
} __attribute__((packed));

struct Ep3Deck {
  ptext<char, 0x10> name;
  be_uint32_t client_id; // 0-3
  // List of card IDs. The card count is the number of nonzero entries here
  // before a zero entry (or 50 if no entries are nonzero). The first card ID is
  // the SC card, which the game implicitly subtracts from the limit - so a
  // valid deck should actually have 31 cards in it.
  parray<le_uint16_t, 50> card_ids;
  be_uint32_t unknown_a1;
  // Last modification time
  le_uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t unknown_a2;
} __attribute__((packed)); // 0x84 bytes in total

struct Ep3Config {
  // Offsets in comments in this struct are relative to start of 61/98 command
  /* 0728 */ parray<uint8_t, 0x1434> unknown_a1;
  /* 1B5C */ parray<Ep3Deck, 25> decks;
  /* 2840 */ uint64_t unknown_a2;
  /* 2848 */ be_uint32_t offline_clv_exp; // CLvOff = this / 100
  /* 284C */ be_uint32_t online_clv_exp; // CLvOn = this / 100
  /* 2850 */ parray<uint8_t, 0x14C> unknown_a3;
  /* 299C */ ptext<char, 0x10> name;
  // Other records are probably somewhere in here - e.g. win/loss, play time, etc.
  /* 29AC */ parray<uint8_t, 0xCC> unknown_a4;
} __attribute__((packed));

struct Ep3BattleRules {
  // When this structure is used in a map/quest definition, FF in any of these
  // fields means the user is allowed to override it. Any non-FF fields are
  // fixed for the map/quest and cannot be overridden.
  uint8_t overall_time_limit; // In increments of 5 minutes; 0 = unlimited
  uint8_t phase_time_limit; // In seconds; 0 = unlimited
  uint8_t allowed_cards; // 0 = any, 1 = N-rank only, 2 = N and R, 3 = N, R, and S 
  uint8_t min_dice; // 0 = default (1)
  // 4
  uint8_t max_dice; // 0 = default (6)
  uint8_t disable_deck_shuffle; // 0 = shuffle on, 1 = off
  uint8_t disable_deck_loop; // 0 = loop on, 1 = off
  uint8_t char_hp;
  // 8
  uint8_t hp_type; // 0 = defeat player, 1 = defeat team, 2 = common hp
  uint8_t no_assist_cards; // 1 = assist cards disallowed
  uint8_t disable_dialogue; // 0 = dialogue on, 1 = dialogue off
  uint8_t dice_exchange_mode; // 0 = high attack, 1 = high defense, 2 = none
  // C
  uint8_t disable_dice_boost; // 0 = dice boost on, 1 = off
  parray<uint8_t, 3> unused;
} __attribute__((packed));



struct Ep3MapList {
  be_uint32_t num_maps;
  be_uint32_t unknown_a1; // Always 0?
  be_uint32_t strings_offset; // From after total_size field (add 0x10 to this value)
  be_uint32_t total_size; // Including header, entries, and strings

  struct Entry { // Should be 0x220 bytes in total
    // These 3 fields probably include the location ID (scenery to load) and the
    // music ID
    be_uint16_t map_x;
    be_uint16_t map_y;
    be_uint16_t scene_data2;
    be_uint16_t map_number;
    // Text offsets are from the beginning of the strings block after all map
    // entries (that is, add strings_offset to them to get the string offset)
    be_uint32_t name_offset;
    be_uint32_t location_name_offset;
    be_uint32_t quest_name_offset;
    be_uint32_t description_offset;
    be_uint16_t width;
    be_uint16_t height;
    parray<uint8_t, 0x100> map_tiles;
    parray<uint8_t, 0x100> modification_tiles;
    be_uint32_t unknown_a2; // Seems to always be 0xFF000000
  } __attribute__((packed));

  // Variable-length fields:
  // Entry entries[num_maps];
  // char strings[...EOF]; // Null-terminated strings, pointed to by offsets in Entry structs
} __attribute__((packed));

struct Ep3CompressedMapHeader { // .mnm file format
  le_uint32_t map_number;
  le_uint32_t compressed_data_size;
  // Compressed data immediately follows (which decompresses to an Ep3Map)
} __attribute__((packed));

struct Ep3Map { // .mnm format (after decompressing and discarding the header)
  /* 0000 */ be_uint32_t unknown_a1;
  /* 0004 */ be_uint32_t map_number;
  /* 0008 */ uint8_t width;
  /* 0009 */ uint8_t height;
  /* 000A */ uint8_t scene_data2; // TODO: What is this?
  // All alt_maps fields (including the floats) past num_alt_maps are filled in
  // with FF. For example, if num_alt_maps == 8, the last two fields in each
  // alt_maps array are filled with FF.
  /* 000B */ uint8_t num_alt_maps; // TODO: What are the alt maps for?
  // In the map_tiles array, the values are:
  // 00 = not a valid tile
  // 01 = valid tile unless punched out (later)
  // 02 = team A start (1v1)
  // 03, 04 = team A start (2v2)
  // 05 = ???
  // 06, 07 = team B start (2v2)
  // 08 = team B start (1v1)
  // Note that the game displays the map reversed vertically in the preview
  // window. For example, player 1 is on team A, which usually starts at the top
  // of the map as defined in this struct, or at the bottom as shown in the
  // preview window.
  /* 000C */ parray<uint8_t, 0x100> map_tiles;
  /* 010C */ parray<uint8_t, 0x0C> unknown_a2;
  /* 0118 */ parray<uint8_t, 0x100> alt_maps1[0x0A];
  /* 0B18 */ parray<uint8_t, 0x100> alt_maps2[0x0A];
  /* 1518 */ parray<be_float, 0x12> alt_maps_unknown_a3[0x0A];
  /* 17E8 */ parray<be_float, 0x12> alt_maps_unknown_a4[0x0A];
  /* 1AB8 */ parray<be_float, 0x6C> unknown_a5;
  // In the modification_tiles array, the values are:
  // 10 = blocked (as if the corresponding map_tiles value was 00)
  // 20 = blocked (maybe one of 10 or 20 are passable by Aerial characters though)
  // 30, 31 = teleporters (green, red)
  // 40-44 = ???? (used in 244, 2E4, 2F9)
  // 50 = appears as improperly-z-buffered teal cube in preview
  // TODO: There may be more values that are valid here.
  /* 1C68 */ parray<uint8_t, 0x100> modification_tiles;
  // Note: The rules are near the end of this struct (starting 0x14 bytes before the end)
  /* 1D68 */ parray<uint8_t, 0x74> unknown_a6;
  /* 1DDC */ Ep3BattleRules default_rules;
  /* 1DEC */ parray<uint8_t, 4> unknown_a7;
  /* 1DF0 */ ptext<char, 0x14> name;
  /* 1E04 */ ptext<char, 0x14> location_name;
  /* 1E18 */ ptext<char, 0x3C> quest_name; // Same a location_name for non-quest maps
  /* 1E54 */ ptext<char, 0x190> description;
  /* 1FE4 */ be_uint16_t map_x;
  /* 1FE6 */ be_uint16_t map_y;
  struct NPCDeck {
    ptext<char, 0x18> name;
    parray<be_uint16_t, 0x20> card_ids; // Last one appears to always be FFFF
  } __attribute__((packed));
  /* 1FE8 */ NPCDeck npc_decks[3]; // Unused if name[0] == 0
  struct NPCCharacter {
    parray<be_uint16_t, 2> unknown_a1;
    parray<uint8_t, 4> unknown_a2;
    ptext<char, 0x10> name;
    parray<be_uint16_t, 0x7E> unknown_a3;
  } __attribute__((packed));
  /* 20F0 */ parray<NPCCharacter, 3> npc_chars; // Unused if name[0] == 0
  /* 242C */ parray<uint8_t, 0x14> unknown_a8; // Always FF?
  /* 2440 */ ptext<char, 0x190> before_message;
  /* 25D0 */ ptext<char, 0x190> after_message;
  /* 2760 */ ptext<char, 0x190> dispatch_message; // Usually "You can only dispatch <character>" or blank
  struct DialogueSet {
    be_uint16_t unknown_a1;
    be_uint16_t unknown_a2; // Always 0x0064 if valid, 0xFFFF if unused?
    ptext<char, 0x40> strings[4];
  } __attribute__((packed)); // Total size: 0x104 bytes
  /* 28F0 */ parray<DialogueSet, 0x10> dialogue_sets[3]; // Up to 0x10 per valid NPC
  /* 59B0 */ be_uint16_t reward_card_id; // TODO: This could be an array. The only examples I've seen have only one here
  /* 59B2 */ parray<be_uint16_t, 0x33> unknown_a9;
  /* 5A18 */
} __attribute__((packed));

class Ep3DataIndex {
public:
  explicit Ep3DataIndex(const std::string& directory);

  struct CardEntry {
    Ep3CardStats stats;
    std::vector<std::string> text;
  };

  struct MapEntry {
    Ep3Map map;
    std::string compressed_data;
  };

  const std::string& get_compressed_card_definitions() const;
  std::shared_ptr<const CardEntry> get_card_definition(uint32_t id) const;

  const std::string& get_compressed_map_list() const;
  std::shared_ptr<const MapEntry> get_map(uint32_t id) const;

private:
  std::string compressed_card_definitions;
  std::unordered_map<uint32_t, std::shared_ptr<CardEntry>> card_definitions;

  // The compressed map list is generated on demand from the maps map below.
  // It's marked mutable because the logical consistency of the Ep3DataIndex
  // object is not violated from the caller's perspective even if we don't
  // generate the compressed map list at load time.
  mutable std::string compressed_map_list;
  std::map<uint32_t, std::shared_ptr<MapEntry>> maps;
};
