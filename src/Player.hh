#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>
#include <vector>
#include <phosg/Encoding.hh>

#include "Version.hh"
#include "Text.hh"



// raw item data
// TODO: use parray for the fields here
struct ItemData {
  union {
    uint8_t item_data1[12];
    le_uint16_t item_data1w[6];
    le_uint32_t item_data1d[3];
  } __attribute__((packed));
  uint32_t item_id;
  union {
    uint8_t item_data2[4];
    le_uint16_t item_data2w[2];
    le_uint32_t item_data2d;
  } __attribute__((packed));

  uint32_t primary_identifier() const;
} __attribute__((packed));

struct PlayerBankItem;

// an item in a player's inventory
struct PlayerInventoryItem {
  le_uint16_t equip_flags;
  le_uint16_t tech_flag;
  le_uint32_t game_flags;
  ItemData data;

  PlayerBankItem to_bank_item() const;
} __attribute__((packed));

// an item in a player's bank
struct PlayerBankItem {
  ItemData data;
  le_uint16_t amount;
  le_uint16_t show_flags;

  PlayerInventoryItem to_inventory_item() const;
} __attribute__((packed));

// a player's inventory (remarkably, the format is the same in all versions of PSO)
struct PlayerInventory {
  uint8_t num_items;
  uint8_t hp_materials_used;
  uint8_t tp_materials_used;
  uint8_t language;
  PlayerInventoryItem items[30];

  size_t find_item(uint32_t item_id);
} __attribute__((packed));

// a player's bank
struct PlayerBank {
  le_uint32_t num_items;
  le_uint32_t meseta;
  PlayerBankItem items[200];

  void load(const std::string& filename);
  void save(const std::string& filename) const;

  bool switch_with_file(const std::string& save_filename,
      const std::string& load_filename);

  void add_item(const PlayerBankItem& item);
  void remove_item(uint32_t item_id, uint32_t amount, PlayerBankItem* item);
  size_t find_item(uint32_t item_id);
} __attribute__((packed));



// simple player stats
struct PlayerStats {
  le_uint16_t atp;
  le_uint16_t mst;
  le_uint16_t evp;
  le_uint16_t hp;
  le_uint16_t dfp;
  le_uint16_t ata;
  le_uint16_t lck;

  PlayerStats() noexcept;
} __attribute__((packed));

struct PlayerDispDataBB;

// PC/GC player appearance and stats data
struct PlayerDispDataPCGC { // 0xD0 in size
  PlayerStats stats;
  parray<uint8_t, 0x0A> unknown_a1;
  le_uint32_t level;
  le_uint32_t experience;
  le_uint32_t meseta;
  ptext<char, 0x10> name;
  uint64_t unknown_a2;
  le_uint32_t name_color;
  uint8_t extra_model;
  parray<uint8_t, 0x0F> unused;
  le_uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  le_uint32_t v1_flags;
  le_uint16_t costume;
  le_uint16_t skin;
  le_uint16_t face;
  le_uint16_t head;
  le_uint16_t hair;
  le_uint16_t hair_r;
  le_uint16_t hair_g;
  le_uint16_t hair_b;
  le_float proportion_x;
  le_float proportion_y;
  parray<uint8_t, 0x48> config;
  parray<uint8_t, 0x14> technique_levels;

  // Note: This struct has a default constructor because it's used in a command
  // that has a fixed-size array. If we didn't define this constructor, the
  // trivial fields in that array's members would be uninitialized, and we could
  // send uninitialized memory to the client.
  PlayerDispDataPCGC() noexcept;

  void enforce_pc_limits();
  PlayerDispDataBB to_bb() const;
} __attribute__((packed));

// BB player preview format
struct PlayerDispDataBBPreview {
  le_uint32_t experience;
  le_uint32_t level;
  ptext<char, 0x10> guild_card;
  uint64_t unknown_a2;
  le_uint32_t name_color;
  uint8_t extra_model;
  parray<uint8_t, 0x0F> unused;
  le_uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  le_uint32_t v1_flags;
  le_uint16_t costume;
  le_uint16_t skin;
  le_uint16_t face;
  le_uint16_t head;
  le_uint16_t hair;
  le_uint16_t hair_r;
  le_uint16_t hair_g;
  le_uint16_t hair_b;
  le_float proportion_x;
  le_float proportion_y;
  ptext<char16_t, 0x10> name;
  uint32_t play_time;

  PlayerDispDataBBPreview() noexcept;
} __attribute__((packed));

// BB player appearance and stats data
struct PlayerDispDataBB {
  PlayerStats stats;
  parray<uint8_t, 0x0A> unknown_a1;
  le_uint32_t level;
  le_uint32_t experience;
  le_uint32_t meseta;
  ptext<char, 0x10> guild_card;
  uint64_t unknown_a2;
  le_uint32_t name_color;
  uint8_t extra_model;
  parray<uint8_t, 0x0F> unused;
  le_uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  le_uint32_t v1_flags;
  le_uint16_t costume;
  le_uint16_t skin;
  le_uint16_t face;
  le_uint16_t head;
  le_uint16_t hair;
  le_uint16_t hair_r;
  le_uint16_t hair_g;
  le_uint16_t hair_b;
  le_float proportion_x;
  le_float proportion_y;
  ptext<char16_t, 0x10> name;
  parray<uint8_t, 0xE8> config;
  parray<uint8_t, 0x14> technique_levels;

  PlayerDispDataBB() noexcept;

  inline void enforce_pc_limits() { }
  PlayerDispDataPCGC to_pcgc() const;
  PlayerDispDataBBPreview to_preview() const;
  void apply_preview(const PlayerDispDataBBPreview&);
} __attribute__((packed));



struct GuildCardGC {
  le_uint32_t player_tag;
  le_uint32_t serial_number;
  ptext<char, 0x18> name;
  ptext<char, 0x6C> desc;
  uint8_t reserved1; // should be 1
  uint8_t reserved2; // should be 1
  uint8_t section_id;
  uint8_t char_class;

  GuildCardGC() noexcept;
} __attribute__((packed));

// BB guild card format
struct GuildCardBB {
  le_uint32_t serial_number;
  ptext<char16_t, 0x18> name;
  ptext<char16_t, 0x10> teamname;
  ptext<char16_t, 0x58> desc;
  uint8_t reserved1; // should be 1
  uint8_t reserved2; // should be 1
  uint8_t section_id;
  uint8_t char_class;

  GuildCardBB() noexcept;
} __attribute__((packed));

// an entry in the BB guild card file
struct GuildCardEntryBB {
  GuildCardBB data;
  parray<uint8_t, 0xB4> unknown;
} __attribute__((packed));

// the format of the BB guild card file
struct GuildCardFileBB {
  parray<uint8_t, 0x1F84> unknown_a1;
  GuildCardEntryBB entry[0x0068]; // that's 104 of them in decimal
  parray<uint8_t, 0x01AC> unknown_a2;
} __attribute__((packed));

// PSOBB key config and team info
struct KeyAndTeamConfigBB {
  parray<uint8_t, 0x0114> unknown_a1;      // 0000
  parray<uint8_t, 0x016C> key_config;      // 0114
  parray<uint8_t, 0x0038> joystick_config; // 0280
  le_uint32_t serial_number;               // 02B8
  le_uint32_t team_id;                     // 02BC
  le_uint64_t team_info;                   // 02C0
  le_uint16_t team_privilege_level;        // 02C8
  le_uint16_t reserved;                    // 02CA
  ptext<char16_t, 0x0010> team_name;       // 02CC
  parray<uint8_t, 0x0800> team_flag;       // 02EC
  le_uint64_t team_rewards;                // 0AEC
} __attribute__((packed));

// BB account data
struct PlayerAccountDataBB {
  parray<uint8_t, 0x04E0> symbol_chats;
  KeyAndTeamConfigBB key_config;
  GuildCardFileBB guild_cards;
  le_uint32_t options;
  parray<uint8_t, 0x0A40> shortcuts; // chat shortcuts (@1FB4 in E7 command)
} __attribute__((packed));



struct PlayerLobbyDataPC {
  le_uint32_t player_tag;
  le_uint32_t guild_card;
  be_uint32_t ip_address;
  le_uint32_t client_id;
  ptext<char16_t, 0x10> name;

  PlayerLobbyDataPC() noexcept;
} __attribute__((packed));

struct PlayerLobbyDataGC {
  le_uint32_t player_tag;
  le_uint32_t guild_card;
  be_uint32_t ip_address;
  le_uint32_t client_id;
  ptext<char, 0x10> name;

  PlayerLobbyDataGC() noexcept;
} __attribute__((packed));

struct PlayerLobbyDataBB {
  le_uint32_t player_tag;
  le_uint32_t guild_card;
  be_uint32_t ip_address; // Guess - the official builds didn't use this, but all other versions have it
  parray<uint8_t, 0x10> unknown_a1;
  le_uint32_t client_id;
  ptext<char16_t, 0x10> name;
  le_uint32_t unknown2;

  PlayerLobbyDataBB() noexcept;
} __attribute__((packed));



struct PSOPlayerDataPC { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
} __attribute__((packed));

struct PSOPlayerDataGC { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
  parray<uint8_t, 0x134> unknown;
  ptext<char, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char auto_reply[0];
} __attribute__((packed));

struct PSOPlayerDataBB { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataBB disp;
  ptext<char, 0x174> unused;
  ptext<char16_t, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char16_t auto_reply[0];
} __attribute__((packed));

// complete BB player data format (used in E7 command)
struct PlayerBB {
  PlayerInventory inventory;               // 0000 // player
  PlayerDispDataBB disp;                   // 034C // player
  parray<uint8_t, 0x0010> unknown;          // 04DC //
  le_uint32_t option_flags;                   // 04EC // account
  parray<uint8_t, 0x0208> quest_data1;      // 04F0 // player
  PlayerBank bank;                         // 06F8 // player
  le_uint32_t serial_number;                  // 19C0 // player
  ptext<char16_t, 0x18> name;              // 19C4 // player
  ptext<char16_t, 0x10> team_name;         // 19C4 // player
  ptext<char16_t, 0x58> guild_card_desc;   // 1A14 // player
  uint8_t reserved1;                       // 1AC4 // player
  uint8_t reserved2;                       // 1AC5 // player
  uint8_t section_id;                      // 1AC6 // player
  uint8_t char_class;                      // 1AC7 // player
  le_uint32_t unknown3;                       // 1AC8 //
  parray<uint8_t, 0x04E0> symbol_chats;     // 1ACC // account
  parray<uint8_t, 0x0A40> shortcuts;        // 1FAC // account
  ptext<char16_t, 0x00AC> auto_reply;      // 29EC // player
  ptext<char16_t, 0x00AC> info_board;      // 2B44 // player
  parray<uint8_t, 0x001C> unknown5;         // 2C9C //
  parray<uint8_t, 0x0140> challenge_data;   // 2CB8 // player
  parray<uint8_t, 0x0028> tech_menu_config; // 2DF8 // player
  parray<uint8_t, 0x002C> unknown6;         // 2E20 //
  parray<uint8_t, 0x0058> quest_data2;      // 2E4C // player
  KeyAndTeamConfigBB key_config;           // 2EA4 // account
} __attribute__((packed));                 // total size: 39A0



struct SavedPlayerBB { // .nsc file format
  ptext<char, 0x40>       signature;
  PlayerDispDataBBPreview preview;
  ptext<char16_t, 0x00AC> auto_reply;
  PlayerBank              bank;
  parray<uint8_t, 0x0140> challenge_data;
  PlayerDispDataBB        disp;
  ptext<char16_t, 0x58>   guild_card_desc;
  ptext<char16_t, 0x00AC> info_board;
  PlayerInventory         inventory;
  parray<uint8_t, 0x0208> quest_data1;
  parray<uint8_t, 0x0058> quest_data2;
  parray<uint8_t, 0x0028> tech_menu_config;
} __attribute__((packed));

struct SavedAccountBB { // .nsa file format
  ptext<char, 0x40>           signature;
  parray<le_uint32_t, 0x001E> blocked_senders;
  GuildCardFileBB             guild_cards;
  KeyAndTeamConfigBB          key_config;
  le_uint32_t                 option_flags;
  parray<uint8_t, 0x0A40>     shortcuts;
  parray<uint8_t, 0x04E0>     symbol_chats;
  ptext<char16_t, 0x0010>     team_name;
} __attribute__((packed));

// complete player info stored by the server
struct Player {
  le_uint32_t                 loaded_from_shipgate_time;
  ptext<char16_t, 0x00AC>     auto_reply;            // player
  PlayerBank                  bank;                  // player
  ptext<char, 0x0020>         bank_name;             // not saved
  parray<le_uint32_t, 0x001E> blocked_senders;       // account
  parray<uint8_t, 0x0140>     challenge_data;        // player
  PlayerDispDataBB            disp;                  // player
  parray<uint8_t, 0x2408>     ep3_config;            // not saved
  ptext<char16_t, 0x0058>     guild_card_desc;       // player
  GuildCardFileBB             guild_cards;           // account
  PlayerInventoryItem         identify_result;       // not saved
  ptext<char16_t, 0x00AC>     info_board;            // player
  PlayerInventory             inventory;             // player
  KeyAndTeamConfigBB          key_config;            // account
  le_uint32_t                 option_flags;          // account
  parray<uint8_t, 0x0208>     quest_data1;           // player
  parray<uint8_t, 0x0058>     quest_data2;           // player
  le_uint32_t                 serial_number;         // account identifier
  std::vector<ItemData>       current_shop_contents; // not saved
  parray<uint8_t, 0x0A40>     shortcuts;             // account
  parray<uint8_t, 0x04E0>     symbol_chats;          // account
  ptext<char16_t, 0x0010>     team_name;             // account
  parray<uint8_t, 0x0028>     tech_menu_config;      // player

  void load_player_data(const std::string& filename);
  void save_player_data(const std::string& filename) const;

  void load_account_data(const std::string& filename);
  void save_account_data(const std::string& filename) const;

  void import(const PSOPlayerDataPC& pd);
  void import(const PSOPlayerDataGC& pd);
  void import(const PSOPlayerDataBB& pd);
  PlayerBB export_bb_player_data() const;

  void add_item(const PlayerInventoryItem& item);
  void remove_item(uint32_t item_id, uint32_t amount, PlayerInventoryItem* item);
  size_t find_item(uint32_t item_id);
};



uint32_t compute_guild_card_checksum(const void* data, size_t size);

std::string filename_for_player_bb(const std::string& username, uint8_t player_index);
std::string filename_for_bank_bb(const std::string& username, const std::string& bank_name);
std::string filename_for_class_template_bb(uint8_t char_class);
std::string filename_for_account_bb(const std::string& username);



template <typename DestT, typename SrcT = DestT>
DestT convert_player_disp_data(const SrcT&) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized strcpy_t should never be called");
}

template <>
inline PlayerDispDataPCGC convert_player_disp_data<PlayerDispDataPCGC>(
    const PlayerDispDataPCGC& src) {
  return src;
}

template <>
inline PlayerDispDataPCGC convert_player_disp_data<PlayerDispDataPCGC, PlayerDispDataBB>(
    const PlayerDispDataBB& src) {
  return src.to_pcgc();
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB, PlayerDispDataPCGC>(
    const PlayerDispDataPCGC& src) {
  return src.to_bb();
}

template <>
inline PlayerDispDataBB convert_player_disp_data<PlayerDispDataBB>(
    const PlayerDispDataBB& src) {
  return src;
}
