#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <array>
#include <phosg/Encoding.hh>
#include <string>
#include <utility>
#include <vector>

#include "Episode3/DataIndex.hh"
#include "ItemData.hh"
#include "LevelTable.hh"
#include "Text.hh"
#include "Version.hh"

struct PlayerBankItem;

struct PlayerInventoryItem { // 0x1C bytes
  le_uint32_t present;
  le_uint32_t flags; // 8 = equipped
  ItemData data;

  PlayerInventoryItem();
  PlayerInventoryItem(const PlayerBankItem&);
  void clear();
} __attribute__((packed));

struct PlayerBankItem { // 0x18 bytes
  ItemData data;
  le_uint16_t amount;
  le_uint16_t show_flags;

  PlayerBankItem();
  PlayerBankItem(const PlayerInventoryItem&);
  void clear();
} __attribute__((packed));

struct PlayerInventory { // 0x34C bytes
  uint8_t num_items;
  uint8_t hp_materials_used;
  uint8_t tp_materials_used;
  uint8_t language;
  PlayerInventoryItem items[30];

  PlayerInventory();

  size_t find_item(uint32_t item_id) const;

  size_t find_equipped_weapon() const;
  size_t find_equipped_armor() const;
  size_t find_equipped_mag() const;
} __attribute__((packed));

struct PlayerBank { // 0x12C8 bytes
  le_uint32_t num_items;
  le_uint32_t meseta;
  PlayerBankItem items[200];

  void load(const std::string& filename);
  void save(const std::string& filename, bool save_to_filesystem) const;

  bool switch_with_file(const std::string& save_filename,
      const std::string& load_filename);

  void add_item(const PlayerBankItem& item);
  PlayerBankItem remove_item(uint32_t item_id, uint32_t amount);
  size_t find_item(uint32_t item_id);
} __attribute__((packed));

struct PendingItemTrade {
  uint8_t other_client_id;
  bool confirmed; // true if client has sent a D2 command
  std::vector<ItemData> items;
};

struct PendingCardTrade {
  uint8_t other_client_id;
  bool confirmed; // true if client has sent an EE D2 command
  std::vector<std::pair<uint32_t, uint32_t>> card_to_count;
};

struct PlayerDispDataBB;

struct PlayerDispDataDCPCV3 { // 0xD0 bytes
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
  PlayerDispDataDCPCV3() noexcept;

  void enforce_v2_limits();
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
  le_uint32_t name_color; // ARGB8888
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
  ptext<char16_t, 0x0C> name;
  le_uint32_t play_time;
  uint32_t unknown_a3;
  parray<uint8_t, 0xE8> config;
  parray<uint8_t, 0x14> technique_levels;

  PlayerDispDataBB() noexcept;

  inline void enforce_v2_limits() {}
  PlayerDispDataDCPCV3 to_dcpcv3() const;
  PlayerDispDataBBPreview to_preview() const;
  void apply_preview(const PlayerDispDataBBPreview&);
  void apply_dressing_room(const PlayerDispDataBBPreview&);
} __attribute__((packed));

// TODO: Is this the same for XB as it is for GC? (This struct is based on the
// GC format)
struct GuildCardV3 {
  /* 00 */ le_uint32_t player_tag;
  /* 04 */ le_uint32_t guild_card_number;
  /* 08 */ ptext<char, 0x18> name;
  /* 20 */ ptext<char, 0x6C> description;
  /* 8C */ uint8_t present; // should be 1
  /* 8D */ uint8_t language;
  /* 8E */ uint8_t section_id;
  /* 8F */ uint8_t char_class;
  /* 90 */

  GuildCardV3() noexcept;
} __attribute__((packed));

// BB guild card format
struct GuildCardBB {
  le_uint32_t guild_card_number;
  ptext<char16_t, 0x18> name;
  ptext<char16_t, 0x10> team_name;
  ptext<char16_t, 0x58> description;
  uint8_t present; // should be 1 if guild card entry exists
  uint8_t language;
  uint8_t section_id;
  uint8_t char_class;

  GuildCardBB() noexcept;
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
  le_uint32_t guild_card_number; // 02B8
  le_uint32_t team_id; // 02BC
  le_uint64_t team_info; // 02C0
  le_uint16_t team_privilege_level; // 02C8
  le_uint16_t reserved; // 02CA
  ptext<char16_t, 0x0010> team_name; // 02CC
  parray<uint8_t, 0x0800> team_flag; // 02EC
  le_uint32_t team_rewards; // 0AEC
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

struct PlayerChallengeDataV3 {
  le_uint32_t client_id;
  struct {
    le_uint16_t unknown_a1;
    parray<uint8_t, 2> unknown_a2; // Possibly unused
    parray<le_uint32_t, 0x17> unknown_a3;
    struct {
      parray<uint8_t, 4> unknown_a1;
      le_uint16_t unknown_a2;
      parray<uint8_t, 2> unknown_a3;
      parray<le_uint32_t, 5> unknown_a4;
      parray<uint8_t, 0x34> unknown_a5;
    } __attribute__((packed)) unknown_a4; // 0x50 bytes
    struct {
      parray<uint8_t, 4> unknown_a1;
      parray<le_uint32_t, 3> unknown_a2;
    } __attribute__((packed)) unknown_a5; // 0x10 bytes
    struct UnknownPair {
      le_uint32_t unknown_a1;
      le_uint32_t unknown_a2;
    } __attribute__((packed));
    parray<UnknownPair, 3> unknown_a6; // 0x18 bytes
    parray<uint8_t, 0x28> unknown_a7;
  } __attribute__((packed)) unknown_a1; // 0x100 bytes
  // On Episode 3, unknown_a2[0] is win count, [1] is loss count, and [4] is
  // disconnect count
  parray<le_uint16_t, 8> unknown_a2;
  parray<le_uint32_t, 2> unknown_a3;
} __attribute__((packed)); // 0x11C bytes

struct PlayerChallengeDataBB {
  le_uint32_t client_id;
  parray<uint8_t, 0x158> unknown_a1;
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

struct PSOPlayerDataDCPC { // For command 61
  PlayerInventory inventory;
  PlayerDispDataDCPCV3 disp;
} __attribute__((packed));

struct PSOPlayerDataV3 { // For command 61
  PlayerInventory inventory;
  PlayerDispDataDCPCV3 disp;
  PlayerChallengeDataV3 challenge_data;
  ChoiceSearchConfig<le_uint16_t> choice_search_config;
  ptext<char, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  // The auto-reply message can be up to 0x200 bytes. If it's shorter than that,
  // the client truncates the command after the first zero byte (rounded up to
  // the next 4-byte boundary).
  char auto_reply[0];
} __attribute__((packed));

struct PSOPlayerDataGCEp3 { // For command 61
  PlayerInventory inventory;
  PlayerDispDataDCPCV3 disp;
  PlayerChallengeDataV3 challenge_data;
  ChoiceSearchConfig<le_uint16_t> choice_search_config;
  ptext<char, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  ptext<char, 0xAC> auto_reply;
  Episode3::PlayerConfig ep3_config;
} __attribute__((packed));

struct PSOPlayerDataBB { // For command 61
  PlayerInventory inventory;
  PlayerDispDataBB disp;
  PlayerChallengeDataBB challenge_data;
  ChoiceSearchConfig<le_uint16_t> choice_search_config;
  ptext<char16_t, 0xAC> info_board;
  parray<le_uint32_t, 0x1E> blocked_senders;
  le_uint32_t auto_reply_enabled;
  char16_t auto_reply[0];
} __attribute__((packed));

struct PlayerBB { // Used in 00E7 command
  PlayerInventory inventory; // 0000-034C; player
  PlayerDispDataBB disp; // 034C-04DC; player
  parray<uint8_t, 0x0010> unknown; // 04DC-04EC; not saved
  le_uint32_t option_flags; // 04EC-04F0; account
  parray<uint8_t, 0x0208> quest_data1; // 04F0-06F8; player
  PlayerBank bank; // 06F8-19C0; player
  le_uint32_t guild_card_number; // 19C0-19C4; player
  ptext<char16_t, 0x18> name; // 19C4-19F4; player
  ptext<char16_t, 0x10> team_name; // 19F4-1A14; player
  ptext<char16_t, 0x58> guild_card_description; // 1A14-1AC4; player
  uint8_t reserved1; // 1AC4-1AC5; player
  uint8_t reserved2; // 1AC5-1AC6; player
  uint8_t section_id; // 1AC6-1AC7; player
  uint8_t char_class; // 1AC7-1AC8; player
  le_uint32_t unknown3; // 1AC8-1ACC; not saved
  parray<uint8_t, 0x04E0> symbol_chats; // 1ACC-1FAC; account
  parray<uint8_t, 0x0A40> shortcuts; // 1FAC-29EC; account
  ptext<char16_t, 0x00AC> auto_reply; // 29EC-2B44; player
  ptext<char16_t, 0x00AC> info_board; // 2B44-2C9C; player
  parray<uint8_t, 0x001C> unknown5; // 2C9C-2CB8; not saved
  parray<uint8_t, 0x0140> challenge_data; // 2CB8-2DF8; player
  parray<uint8_t, 0x0028> tech_menu_config; // 2DF8-2E20; player
  parray<uint8_t, 0x002C> unknown6; // 2E20-2E4C; not saved
  parray<uint8_t, 0x0058> quest_data2; // 2E4C-2EA4; player
  KeyAndTeamConfigBB key_config; // 2EA4-3994; account
} __attribute__((packed));

struct SavedPlayerDataBB { // .nsc file format
  ptext<char, 0x40> signature;
  PlayerDispDataBBPreview preview;
  ptext<char16_t, 0x00AC> auto_reply;
  PlayerBank bank;
  parray<uint8_t, 0x0140> challenge_data;
  PlayerDispDataBB disp;
  ptext<char16_t, 0x0058> guild_card_description;
  ptext<char16_t, 0x00AC> info_board;
  PlayerInventory inventory;
  parray<uint8_t, 0x0208> quest_data1;
  parray<uint8_t, 0x0058> quest_data2;
  parray<uint8_t, 0x0028> tech_menu_config;

  void add_item(const PlayerInventoryItem& item);
  PlayerInventoryItem remove_item(
      uint32_t item_id, uint32_t amount, bool allow_meseta_overdraft);

  void print_inventory(FILE* stream) const;
} __attribute__((packed));

enum AccountFlag {
  IN_DRESSING_ROOM = 0x00000001,
};

struct SavedAccountDataBB { // .nsa file format
  ptext<char, 0x40> signature;
  parray<le_uint32_t, 0x001E> blocked_senders;
  GuildCardFileBB guild_cards;
  KeyAndTeamConfigBB key_config;
  le_uint32_t newserv_flags;
  le_uint32_t option_flags;
  parray<uint8_t, 0x0A40> shortcuts;
  parray<uint8_t, 0x04E0> symbol_chats;
  ptext<char16_t, 0x0010> team_name;
} __attribute__((packed));

class ClientGameData {
private:
  std::shared_ptr<SavedAccountDataBB> account_data;
  std::shared_ptr<SavedPlayerDataBB> player_data;
  uint64_t last_play_time_update;

public:
  uint32_t guild_card_number;
  bool should_update_play_time;

  // The following fields are not saved, and are only used in certain situations

  // Null unless the client is within the trade sequence (D0-D4 or EE commands)
  std::unique_ptr<PendingItemTrade> pending_item_trade;
  std::unique_ptr<PendingCardTrade> pending_card_trade;

  // Null unless the client is Episode 3 and has sent its config already
  std::shared_ptr<Episode3::PlayerConfig> ep3_config;

  // These are only used if the client is BB
  std::string bb_username;
  size_t bb_player_index;
  PlayerInventoryItem identify_result;
  std::array<std::vector<ItemData>, 3> shop_contents;
  bool should_save;

  ClientGameData();
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
  // Note: This function is not const because it updates the player's play time.
  void save_player_data();

  void import_player(const PSOPlayerDataDCPC& pd);
  void import_player(const PSOPlayerDataV3& pd);
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
