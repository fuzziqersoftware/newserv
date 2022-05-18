#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>
#include <vector>
#include <phosg/Encoding.hh>

#include "LevelTable.hh"
#include "Version.hh"
#include "Text.hh"



struct ItemData {
  union {
    uint8_t data1[12];
    le_uint16_t data1w[6];
    le_uint32_t data1d[3];
  } __attribute__((packed));
  le_uint32_t id;
  union {
    uint8_t data2[4];
    le_uint16_t data2w[2];
    le_uint32_t data2d;
  } __attribute__((packed));

  ItemData();

  uint32_t primary_identifier() const;
} __attribute__((packed));

struct PlayerBankItem;

struct PlayerInventoryItem {
  le_uint16_t equip_flags;
  le_uint16_t tech_flag;
  le_uint32_t game_flags;
  ItemData data;

  PlayerInventoryItem();
  PlayerInventoryItem(const PlayerBankItem&);
} __attribute__((packed));

struct PlayerBankItem {
  ItemData data;
  le_uint16_t amount;
  le_uint16_t show_flags;

  PlayerBankItem();
  PlayerBankItem(const PlayerInventoryItem&);
} __attribute__((packed));

struct PlayerInventory {
  uint8_t num_items;
  uint8_t hp_materials_used;
  uint8_t tp_materials_used;
  uint8_t language;
  PlayerInventoryItem items[30];

  PlayerInventory();

  size_t find_item(uint32_t item_id);
} __attribute__((packed));

struct PlayerBank {
  le_uint32_t num_items;
  le_uint32_t meseta;
  PlayerBankItem items[200];

  void load(const std::string& filename);
  void save(const std::string& filename) const;

  bool switch_with_file(const std::string& save_filename,
      const std::string& load_filename);

  void add_item(const PlayerBankItem& item);
  PlayerBankItem remove_item(uint32_t item_id, uint32_t amount);
  size_t find_item(uint32_t item_id);
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
  le_uint32_t team_rewards;                // 0AEC
} __attribute__((packed));



struct Ep3Deck {
  // TODO: are the last 4 bytes actually part of this? They don't seem to be
  // used for anything else, but the game limits the name to 14 chars + a
  // language marker, which equals exactly 0x10 characters.
  ptext<char, 0x14> name;
  // List of card IDs. The card count is the number of nonzero entries here
  // before a zero entry (or 50 if no entries are nonzero). The first card ID is
  // the SC card, which the game implicitly subtracts from the limit - so a
  // valid deck should actually have 31 cards in it.
  le_uint16_t card_ids[50];
  uint32_t unknown_a1;
  // Last modification time
  le_uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t unknown_a2;
} __attribute__((packed));

struct Ep3Config {
  parray<uint8_t, 0x1434> unknown_a1; // at 728 in 61/98 command
  Ep3Deck decks[25]; // at 1B5C in 61/98 command
  uint64_t unknown_a2; // at 2840 in 61/98 command
  be_uint32_t offline_clv_exp; // CLvOff = this / 100
  be_uint32_t online_clv_exp; // CLvOn = this / 100
  parray<uint8_t, 0x14C> unknown_a3; // at 2850 in 61/98 command
  ptext<char, 0x10> name; // at 299C in 61/98 command
  // Other records are probably somewhere in here - e.g. win/loss, play time, etc.
  parray<uint8_t, 0xCC> unknown_a4; // at 29AC in 61/98 command
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



struct PSOPlayerDataPC { // For command 61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
} __attribute__((packed));

struct PSOPlayerDataGC { // For command 61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
  parray<uint8_t, 0x134> unknown;
  ptext<char, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char auto_reply[0];
} __attribute__((packed));

struct PSOPlayerDataGCEp3 { // For command 61
  PlayerInventory inventory;
  PlayerDispDataPCGC disp;
  parray<uint8_t, 0x134> unknown;
  ptext<char, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char auto_reply[0xAC];
  Ep3Config ep3_config;
} __attribute__((packed));

struct PSOPlayerDataBB { // For command 61
  PlayerInventory inventory;
  PlayerDispDataBB disp;
  ptext<char, 0x174> unused;
  ptext<char16_t, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char16_t auto_reply[0];
} __attribute__((packed));

struct PlayerBB { // Used in 00E7 command
  PlayerInventory inventory;                // player
  PlayerDispDataBB disp;                    // player
  parray<uint8_t, 0x0010> unknown;          // not saved
  le_uint32_t option_flags;                 // account
  parray<uint8_t, 0x0208> quest_data1;      // player
  PlayerBank bank;                          // player
  le_uint32_t serial_number;                // player
  ptext<char16_t, 0x18> name;               // player
  ptext<char16_t, 0x10> team_name;          // player
  ptext<char16_t, 0x58> guild_card_desc;    // player
  uint8_t reserved1;                        // player
  uint8_t reserved2;                        // player
  uint8_t section_id;                       // player
  uint8_t char_class;                       // player
  le_uint32_t unknown3;                     // not saved
  parray<uint8_t, 0x04E0> symbol_chats;     // account
  parray<uint8_t, 0x0A40> shortcuts;        // account
  ptext<char16_t, 0x00AC> auto_reply;       // player
  ptext<char16_t, 0x00AC> info_board;       // player
  parray<uint8_t, 0x001C> unknown5;         // not saved
  parray<uint8_t, 0x0140> challenge_data;   // player
  parray<uint8_t, 0x0028> tech_menu_config; // player
  parray<uint8_t, 0x002C> unknown6;         // not saved
  parray<uint8_t, 0x0058> quest_data2;      // player
  KeyAndTeamConfigBB key_config;            // account
} __attribute__((packed));



struct SavedPlayerDataBB { // .nsc file format
  ptext<char, 0x40>       signature;
  PlayerDispDataBBPreview preview;
  ptext<char16_t, 0x00AC> auto_reply;
  PlayerBank              bank;
  parray<uint8_t, 0x0140> challenge_data;
  PlayerDispDataBB        disp;
  ptext<char16_t, 0x0058> guild_card_desc;
  ptext<char16_t, 0x00AC> info_board;
  PlayerInventory         inventory;
  parray<uint8_t, 0x0208> quest_data1;
  parray<uint8_t, 0x0058> quest_data2;
  parray<uint8_t, 0x0028> tech_menu_config;

  void add_item(const PlayerInventoryItem& item);
  PlayerInventoryItem remove_item(uint32_t item_id, uint32_t amount);

  void print_inventory(FILE* stream) const;
} __attribute__((packed));

struct SavedAccountDataBB { // .nsa file format
  ptext<char, 0x40>           signature;
  parray<le_uint32_t, 0x001E> blocked_senders;
  GuildCardFileBB             guild_cards;
  KeyAndTeamConfigBB          key_config;
  le_uint32_t                 unused;
  le_uint32_t                 option_flags;
  parray<uint8_t, 0x0A40>     shortcuts;
  parray<uint8_t, 0x04E0>     symbol_chats;
  ptext<char16_t, 0x0010>     team_name;
} __attribute__((packed));



class ClientGameData {
private:
  std::shared_ptr<SavedAccountDataBB> account_data;
  std::shared_ptr<SavedPlayerDataBB> player_data;

public:
  uint32_t serial_number;

  // The following fields are not saved, and are only used in certain situations

  // Null unless the client is Episode 3 and has sent its config already
  std::shared_ptr<Ep3Config> ep3_config;

  // These are only used if the client is BB
  std::string bb_username;
  size_t bb_player_index;
  PlayerInventoryItem identify_result;
  std::vector<ItemData> shop_contents;

  ClientGameData() : serial_number(0), bb_player_index(0) { }
  ~ClientGameData();

  std::shared_ptr<SavedAccountDataBB> account(bool should_load = true);
  std::shared_ptr<SavedPlayerDataBB> player(bool should_load = true);
  std::shared_ptr<const SavedAccountDataBB> account() const;
  std::shared_ptr<const SavedPlayerDataBB> player() const;

  std::string account_data_filename() const;
  std::string player_data_filename() const;
  static std::string player_template_filename(uint8_t char_class);

  void create_player(
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);

  void load_account_data();
  void save_account_data() const;
  void load_player_data();
  void save_player_data() const;

  void import_player(const PSOPlayerDataPC& pd);
  void import_player(const PSOPlayerDataGC& pd);
  void import_player(const PSOPlayerDataBB& pd);
  // Note: this function is not const because it can cause player and account
  // data to be loaded
  PlayerBB export_player_bb();
};



uint32_t compute_guild_card_checksum(const void* data, size_t size);



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
