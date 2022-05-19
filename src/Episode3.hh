#pragma once

#include <stdint.h>

#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <phosg/Encoding.hh>

#include "Text.hh"



struct Ep3CardDefinition {
  struct Stat {
    be_uint16_t code;
    uint8_t type;
    uint8_t stat;
  } __attribute__((packed));
  be_uint32_t card_id;
  parray<uint8_t, 0x40> jp_name;
  uint8_t type;
  uint8_t cost;
  be_uint16_t unused;
  Stat hp;
  Stat ap;
  Stat tp;
  Stat mv;
  parray<uint8_t, 8> left_colors;
  parray<uint8_t, 8> right_colors;
  parray<uint8_t, 8> top_colors;
  parray<be_uint32_t, 8> range;
  parray<uint8_t, 0x10> unknown_a2;
  ptext<char, 0x14> name;
  ptext<char, 0x0B> jp_short_name;
  ptext<char, 0x07> short_name;
  be_uint16_t unknown_a3; // Could be has_abilities?
  parray<uint8_t, 0x60> unknown_a4;
} __attribute__((packed));

struct Ep3CardDefinitionsFooter {
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

  const std::string& get_compressed_card_definitions() const;
  std::shared_ptr<const Ep3CardDefinition> get_card_definition(uint32_t id) const;

  struct MapEntry {
    Ep3Map map;
    std::string compressed_data;
  };

  const std::string& get_compressed_map_list() const;
  std::shared_ptr<const MapEntry> get_map(uint32_t id) const;

private:
  std::string compressed_card_definitions;
  std::unordered_map<uint32_t, std::shared_ptr<Ep3CardDefinition>> card_definitions;

  std::string compressed_map_list;
  std::map<uint32_t, std::shared_ptr<MapEntry>> maps;
};
