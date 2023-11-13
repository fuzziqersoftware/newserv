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

class ClientGameData {
public:
  uint32_t guild_card_number;
  bool should_update_play_time;

  // The following fields are not saved, and are only used in certain situations

  std::array<uint32_t, 30> blocked_senders;

  // Null unless the client is within the trade sequence (D0-D4 or EE commands)
  std::unique_ptr<PendingItemTrade> pending_item_trade;
  std::unique_ptr<PendingCardTrade> pending_card_trade;

  // Null unless the client is Episode 3 and has sent its config already
  std::shared_ptr<Episode3::PlayerConfig> ep3_config;

  // These are only used if the client is BB
  std::string bb_username;
  int8_t bb_character_index;
  ItemData identify_result;
  std::array<std::vector<ItemData>, 3> shop_contents;

  ClientGameData();
  ~ClientGameData();

  void create_battle_overlay(std::shared_ptr<const BattleRules> rules, std::shared_ptr<const LevelTable> level_table);
  void create_challenge_overlay(GameVersion version, size_t template_index, std::shared_ptr<const LevelTable> level_table);
  inline void delete_overlay() {
    this->overlay_character_data.reset();
  }
  inline bool has_overlay() const {
    return this->overlay_character_data.get() != nullptr;
  }

  std::shared_ptr<PSOBBSystemFile> system(bool allow_load = true);
  std::shared_ptr<const PSOBBSystemFile> system(bool allow_load = true) const;

  std::shared_ptr<PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true);
  std::shared_ptr<const PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true) const;

  std::shared_ptr<PSOBBGuildCardFile> guild_cards(bool allow_load = true);
  std::shared_ptr<const PSOBBGuildCardFile> guild_cards(bool allow_load = true) const;

  void create_character_file(
      uint32_t guild_card_number,
      uint8_t language,
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);

  void save_system_file() const;
  // Note: This function is not const because it updates the player's play time.
  void save_character_file();
  void save_guild_card_file() const;

private:
  // The overlay character data is used in battle and challenge modes, when
  // character data is temporarily replaced in-game. In other play modes and in
  // lobbies, overlay_character_data is null.
  std::shared_ptr<PSOBBSystemFile> system_data;
  std::shared_ptr<PSOBBCharacterFile> overlay_character_data;
  std::shared_ptr<PSOBBCharacterFile> character_data;
  std::shared_ptr<PSOBBGuildCardFile> guild_card_data;
  uint64_t last_play_time_update;

  void load_all_files();

  std::string system_filename() const;
  std::string character_filename() const;
  std::string guild_card_filename() const;

  std::string legacy_player_filename() const;
  std::string legacy_account_filename() const;
};
