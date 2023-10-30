#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <array>
#include <phosg/Encoding.hh>
#include <string>
#include <utility>
#include <vector>

#include "Episode3/DataIndexes.hh"
#include "ItemCreator.hh"
#include "ItemNameIndex.hh"
#include "LevelTable.hh"
#include "PlayerSubordinates.hh"
#include "SaveFileFormats.hh"
#include "Text.hh"
#include "Version.hh"

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

constexpr uint64_t PLAYER_FILE_SIGNATURE_V0 = 0x6E65777365727620;
constexpr uint64_t PLAYER_FILE_SIGNATURE_V1 = 0xA904332D5CEF0296;

struct SavedPlayerDataBB { // .nsc file format
  /* 0000 */ be_uint64_t signature = PLAYER_FILE_SIGNATURE_V1;
  /* 0008 */ parray<uint8_t, 0x20> unused;
  /* 0028 */ PlayerRecords_Battle<false> battle_records;
  /* 0040 */ PlayerDispDataBBPreview preview;
  /* 00BC */ pstring<TextEncoding::UTF16, 0x00AC> auto_reply;
  /* 0214 */ PlayerBank bank;
  /* 14DC */ PlayerRecordsBB_Challenge challenge_records;
  /* 161C */ PlayerDispDataBB disp;
  /* 17AC */ pstring<TextEncoding::UTF16, 0x0058> guild_card_description;
  /* 185C */ pstring<TextEncoding::UTF16, 0x00AC> info_board;
  /* 19B4 */ PlayerInventory inventory;
  /* 1D00 */ parray<uint8_t, 0x0208> quest_data1;
  /* 1F08 */ parray<le_uint32_t, 0x0016> quest_data2;
  /* 1F60 */ parray<uint8_t, 0x0028> tech_menu_config;
  /* 1F88 */

  void update_to_latest_version();

  void add_item(const ItemData& item);
  ItemData remove_item(uint32_t item_id, uint32_t amount, bool allow_meseta_overdraft);
  void add_meseta(uint32_t amount);
  void remove_meseta(uint32_t amount, bool allow_overdraft);

  uint8_t get_technique_level(uint8_t which) const; // Returns FF or 00-1D
  void set_technique_level(uint8_t which, uint8_t level);

  enum class MaterialType : int8_t {
    HP = -2,
    TP = -1,
    POWER = 0,
    MIND = 1,
    EVADE = 2,
    DEF = 3,
    LUCK = 4,
  };

  uint8_t get_material_usage(MaterialType which) const;
  void set_material_usage(MaterialType which, uint8_t usage);
  void clear_all_material_usage();

  void print_inventory(FILE* stream, GameVersion version, std::shared_ptr<const ItemNameIndex> name_index) const;
} __attribute__((packed));

enum AccountFlag {
  IN_DRESSING_ROOM = 0x00000001,
};

struct SavedAccountDataBB { // .nsa file format
  pstring<TextEncoding::ASCII, 0x40> signature;
  parray<le_uint32_t, 0x001E> blocked_senders;
  PSOBBGuildCardFile guild_card_file;
  PSOBBSystemFile system_file;
  le_uint32_t unused;
  le_uint32_t option_flags;
  parray<uint8_t, 0x0A40> shortcuts;
  parray<uint8_t, 0x04E0> symbol_chats;
  pstring<TextEncoding::UTF16, 0x0010> team_name;
} __attribute__((packed));

class ClientGameData {
private:
  std::shared_ptr<SavedAccountDataBB> account_data;
  // The overlay player data is used in battle and challenge modes, when player
  // data is temporarily replaced in-game. In other play modes and in lobbies,
  // overlay_player_data is null.
  std::shared_ptr<SavedPlayerDataBB> overlay_player_data;
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
  ItemData identify_result;
  std::array<std::vector<ItemData>, 3> shop_contents;
  bool should_save;

  ClientGameData();
  ~ClientGameData();

  void create_battle_overlay(std::shared_ptr<const BattleRules> rules, std::shared_ptr<const LevelTable> level_table);
  void create_challenge_overlay(GameVersion version, size_t template_index, std::shared_ptr<const LevelTable> level_table);
  inline void delete_overlay() {
    this->overlay_player_data.reset();
  }
  inline bool has_overlay() const {
    return this->overlay_player_data.get() != nullptr;
  }

  std::shared_ptr<SavedAccountDataBB> account(bool allow_load = true);
  std::shared_ptr<SavedPlayerDataBB> player(bool allow_load = true, bool allow_overlay = true);
  std::shared_ptr<const SavedAccountDataBB> account(bool allow_load = true) const;
  std::shared_ptr<const SavedPlayerDataBB> player(bool allow_load = true, bool allow_overlay = true) const;

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
};
