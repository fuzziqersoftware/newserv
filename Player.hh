#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "Version.hh"



// raw item data
struct ItemData {
  union {
    uint8_t item_data1[12];
    uint16_t item_data1w[6];
    uint32_t item_data1d[3];
  };
  uint32_t item_id;
  union {
    uint8_t item_data2[4];
    uint16_t item_data2w[2];
    uint32_t item_data2d;
  };

  uint32_t primary_identifier() const;
};

struct PlayerBankItem;

// an item in a player's inventory
struct PlayerInventoryItem {
  uint16_t equip_flags;
  uint16_t tech_flag;
  uint32_t game_flags;
  ItemData data;

  PlayerBankItem to_bank_item() const;
};

// an item in a player's bank
struct PlayerBankItem {
  ItemData data;
  uint16_t amount;
  uint16_t show_flags;

  PlayerInventoryItem to_inventory_item() const;
};

// a player's inventory (remarkably, the format is the same in all versions of PSO)
struct PlayerInventory {
  uint8_t num_items;
  uint8_t hp_materials_used;
  uint8_t tp_materials_used;
  uint8_t language;
  PlayerInventoryItem items[30];

  size_t find_item(uint32_t item_id);
};

// a player's bank
struct PlayerBank {
  uint32_t num_items;
  uint32_t meseta;
  PlayerBankItem items[200];

  void load(const std::string& filename);
  void save(const std::string& filename) const;

  bool switch_with_file(const char* save_filename, const char* load_filename);

  void add_item(const PlayerBankItem& item);
  void remove_item(uint32_t item_id, uint32_t amount, PlayerBankItem* item);
  size_t find_item(uint32_t item_id);
};



// simple player stats
struct PlayerStats {
  uint16_t atp;
  uint16_t mst;
  uint16_t evp;
  uint16_t hp;
  uint16_t dfp;
  uint16_t ata;
  uint16_t lck;
};

struct PlayerDispDataBB;

// PC/GC player appearance and stats data
struct PlayerDispDataPCGC { // 0xD0 in size
  PlayerStats stats;
  uint16_t unknown1;
  uint32_t unknown2[2];
  uint32_t level;
  uint32_t experience;
  uint32_t meseta;
  char name[16];
  uint32_t unknown3[2];
  uint32_t name_color;
  uint8_t extra_model;
  uint8_t unused[15];
  uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  uint32_t v1_flags;
  uint16_t costume;
  uint16_t skin;
  uint16_t face;
  uint16_t head;
  uint16_t hair;
  uint16_t hair_r;
  uint16_t hair_g;
  uint16_t hair_b;
  float proportion_x;
  float proportion_y;
  uint8_t config[0x48];
  uint8_t technique_levels[0x14];

  PlayerDispDataBB to_bb() const;
};

// BB player preview format
struct PlayerDispDataBBPreview {
  uint32_t experience;
  uint32_t level;
  char guild_card[16];
  uint32_t unknown3[2];
  uint32_t name_color;
  uint8_t extra_model;
  uint8_t unused[15];
  uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  uint32_t v1_flags;
  uint16_t costume;
  uint16_t skin;
  uint16_t face;
  uint16_t head;
  uint16_t hair;
  uint16_t hair_r;
  uint16_t hair_g;
  uint16_t hair_b;
  float proportion_x;
  float proportion_y;
  char16_t name[16];
  uint32_t play_time;
};

// BB player appearance and stats data
struct PlayerDispDataBB {
  PlayerStats stats;
  uint16_t unknown1;
  uint32_t unknown2[2];
  uint32_t level;
  uint32_t experience;
  uint32_t meseta;
  char guild_card[16];
  uint32_t unknown3[2];
  uint32_t name_color;
  uint8_t extra_model;
  uint8_t unused[11];
  uint32_t play_time; // not actually a game field; used only by my server
  uint32_t name_color_checksum;
  uint8_t section_id;
  uint8_t char_class;
  uint8_t v2_flags;
  uint8_t version;
  uint32_t v1_flags;
  uint16_t costume;
  uint16_t skin;
  uint16_t face;
  uint16_t head;
  uint16_t hair;
  uint16_t hair_r;
  uint16_t hair_g;
  uint16_t hair_b;
  float proportion_x;
  float proportion_y;
  char16_t name[0x10];
  uint8_t config[0xE8];
  uint8_t technique_levels[0x14];

  PlayerDispDataPCGC to_pcgc() const;
  PlayerDispDataBBPreview to_preview() const;
  void apply_preview(const PlayerDispDataBBPreview&);
};



struct GuildCardGC {
  uint32_t player_tag;
  uint32_t serial_number;
  char name[0x18];
  char desc[0x6C];
  uint8_t reserved1; // should be 1
  uint8_t reserved2; // should be 1
  uint8_t section_id;
  uint8_t char_class;
};

// BB guild card format
struct GuildCardBB {
  uint32_t serial_number;
  char16_t name[0x18];
  char16_t teamname[0x10];
  char16_t desc[0x58];
  uint8_t reserved1; // should be 1
  uint8_t reserved2; // should be 1
  uint8_t section_id;
  uint8_t char_class;
};

// an entry in the BB guild card file
struct GuildCardEntryBB {
  GuildCardBB data;
  uint8_t unknown[0xB4];
};

// the format of the BB guild card file
struct GuildCardFileBB {
  uint8_t unknown[0x1F84];
  GuildCardEntryBB entry[0x0068]; // that's 104 of them in decimal
  uint8_t unknown2[0x01AC];
};

// PSOBB key config and team info
struct KeyAndTeamConfigBB {
  uint8_t unknown[0x0114];         // 0000
  uint8_t key_config[0x016C];      // 0114
  uint8_t joystick_config[0x0038]; // 0280
  uint32_t serial_number;          // 02B8
  uint32_t team_id;                // 02BC
  uint32_t team_info[2];           // 02C0
  uint16_t team_privilege_level;   // 02C8
  uint16_t reserved;               // 02CA
  char16_t team_name[0x0010];       // 02CC
  uint8_t team_flag[0x0800];       // 02EC
  uint32_t team_rewards[2];        // 0AEC
};

// BB account data
struct PlayerAccountDataBB {
  uint8_t symbol_chats[0x04E0];
  KeyAndTeamConfigBB key_config;
  GuildCardFileBB guild_cards;
  uint32_t options;
  uint8_t shortcuts[0x0A40]; // chat shortcuts (@1FB4 in E7 command)
};



struct PlayerLobbyDataPC {
  uint32_t player_tag;
  uint32_t guild_card;
  uint32_t ip_address;
  uint32_t client_id;
  char16_t name[16];
};

struct PlayerLobbyDataGC {
  uint32_t player_tag;
  uint32_t guild_card;
  uint32_t ip_address;
  uint32_t client_id;
  char name[16];
};

struct PlayerLobbyDataBB {
  uint32_t player_tag;
  uint32_t guild_card;
  uint32_t unknown1[5];
  uint32_t client_id;
  char16_t name[16];
  uint32_t unknown2;
};



struct PSOPlayerDataPC { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
};

struct PSOPlayerDataGC { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
  char unknown[0x134];
  char info_board[0xAC];
  uint32_t blocked[0x1E];
  uint32_t auto_reply_enabled;
  char auto_reply[0];
};

struct PSOPlayerDataBB { // for command 0x61
  PlayerInventory inventory;
  PlayerDispDataBB disp;
  char unused[0x174];
  char16_t info_board[0xAC];
  uint32_t blocked[0x1E];
  uint32_t auto_reply_enabled;
  char16_t auto_reply[0];
};

// PC/GC lobby player data (used in lobby/game join commands)
struct PlayerLobbyJoinDataPCGC {
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
};

// BB lobby player data (used in lobby/game join commands)
struct PlayerLobbyJoinDataBB {
  PlayerInventory inventory;
  PlayerDispDataBB disp;
};

// complete BB player data format (used in E7 command)
struct PlayerBB {
  PlayerInventory inventory;        // 0000 // player
  PlayerDispDataBB disp;            // 034C // player
  uint8_t unknown[0x0010];          // 04DC //
  uint32_t option_flags;            // 04EC // account
  uint8_t quest_data1[0x0208];      // 04F0 // player
  PlayerBank bank;                  // 06F8 // player
  uint32_t serial_number;           // 19C0 // player
  char16_t name[0x18];              // 19C4 // player
  char16_t team_name[0x10];         // 19C4 // player
  char16_t guild_card_desc[0x58];   // 1A14 // player
  uint8_t reserved1;                // 1AC4 // player
  uint8_t reserved2;                // 1AC5 // player
  uint8_t section_id;               // 1AC6 // player
  uint8_t char_class;               // 1AC7 // player
  uint32_t unknown3;                // 1AC8 //
  uint8_t symbol_chats[0x04E0];     // 1ACC // account
  uint8_t shortcuts[0x0A40];        // 1FAC // account
  char16_t auto_reply[0x00AC];      // 29EC // player
  char16_t info_board[0x00AC];      // 2B44 // player
  uint8_t unknown5[0x001C];         // 2C9C //
  uint8_t challenge_data[0x0140];   // 2CB8 // player
  uint8_t tech_menu_config[0x0028]; // 2DF8 // player
  uint8_t unknown6[0x002C];         // 2E20 //
  uint8_t quest_data2[0x0058];      // 2E4C // player
  KeyAndTeamConfigBB key_config;    // 2EA4 // account
};                                  // total size: 39A0



struct SavedPlayerBB { // .nsc file format
  char signature[0x40];
  PlayerDispDataBBPreview preview;

  char16_t          auto_reply[0x00AC];
  PlayerBank        bank;
  uint8_t           challenge_data[0x0140];
  PlayerDispDataBB  disp;
  char16_t          guild_card_desc[0x58];
  char16_t          info_board[0x00AC];
  PlayerInventory   inventory;
  uint8_t           quest_data1[0x0208];
  uint8_t           quest_data2[0x0058];
  uint8_t           tech_menu_config[0x0028];
};

struct SavedAccountBB { // .nsa file format
  char signature[0x40];
  uint32_t            blocked[0x001E];
  GuildCardFileBB     guild_cards;
  KeyAndTeamConfigBB  key_config;
  uint32_t            option_flags;
  uint8_t             shortcuts[0x0A40];
  uint8_t             symbol_chats[0x04E0];
  char16_t            team_name[0x0010];
};

// complete player info stored by the server
struct Player {
  uint32_t            loaded_from_shipgate_time;

  char16_t            auto_reply[0x00AC];       // player
  PlayerBank          bank;                     // player
  char                bank_name[0x20];
  uint32_t            blocked[0x001E];          // account
  uint8_t             challenge_data[0x0140];   // player
  PlayerDispDataBB    disp;                     // player
  uint8_t             ep3_config[0x2408];
  char16_t            guild_card_desc[0x58];    // player
  GuildCardFileBB     guild_cards;              // account
  PlayerInventoryItem identify_result;
  char16_t            info_board[0x00AC];       // player
  PlayerInventory     inventory;                // player
  KeyAndTeamConfigBB  key_config;               // account
  uint32_t            option_flags;             // account
  uint8_t             quest_data1[0x0208];      // player
  uint8_t             quest_data2[0x0058];      // player
  uint32_t            serial_number;
  std::vector<ItemData> current_shop_contents;
  uint8_t             shortcuts[0x0A40];        // account
  uint8_t             symbol_chats[0x04E0];     // account
  char16_t            team_name[0x0010];        // account
  uint8_t             tech_menu_config[0x0028]; // player

  void load_player_data(const std::string& filename);
  void save_player_data(const std::string& filename) const;

  void load_account_data(const std::string& filename);
  void save_account_data(const std::string& filename) const;

  void import(const PSOPlayerDataPC& pd);
  void import(const PSOPlayerDataGC& pd);
  void import(const PSOPlayerDataBB& pd);
  PlayerLobbyJoinDataPCGC export_lobby_data_pc() const;
  PlayerLobbyJoinDataPCGC export_lobby_data_gc() const;
  PlayerLobbyJoinDataBB export_lobby_data_bb() const;
  PlayerBB export_bb_player_data() const;

  void add_item(const PlayerInventoryItem& item);
  void remove_item(uint32_t item_id, uint32_t amount, PlayerInventoryItem* item);
  size_t find_item(uint32_t item_id);
};



uint32_t compute_guild_card_checksum(const void* data, size_t size);

std::string filename_for_player_bb(const std::string& username, uint8_t player_index);
std::string filename_for_bank_bb(const std::string& username, const char* bank_name);
std::string filename_for_class_template_bb(uint8_t char_class);
std::string filename_for_account_bb(const std::string& username);
