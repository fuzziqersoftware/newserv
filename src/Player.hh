#pragma once

#include <event2/event.h>
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

class PlayerFilesManager {
public:
  explicit PlayerFilesManager(std::shared_ptr<struct event_base> base);
  ~PlayerFilesManager() = default;

  std::shared_ptr<PSOBBBaseSystemFile> get_system(const std::string& filename);
  std::shared_ptr<PSOBBCharacterFile> get_character(const std::string& filename);
  std::shared_ptr<PSOBBGuildCardFile> get_guild_card(const std::string& filename);
  std::shared_ptr<PlayerBank> get_bank(const std::string& filename);

  void set_system(const std::string& filename, std::shared_ptr<PSOBBBaseSystemFile> file);
  void set_character(const std::string& filename, std::shared_ptr<PSOBBCharacterFile> file);
  void set_guild_card(const std::string& filename, std::shared_ptr<PSOBBGuildCardFile> file);
  void set_bank(const std::string& filename, std::shared_ptr<PlayerBank> file);

private:
  std::shared_ptr<struct event_base> base;
  std::unique_ptr<struct event, void (*)(struct event*)> clear_expired_files_event;

  std::unordered_map<std::string, std::shared_ptr<PSOBBBaseSystemFile>> loaded_system_files;
  std::unordered_map<std::string, std::shared_ptr<PSOBBCharacterFile>> loaded_character_files;
  std::unordered_map<std::string, std::shared_ptr<PSOBBGuildCardFile>> loaded_guild_card_files;
  std::unordered_map<std::string, std::shared_ptr<PlayerBank>> loaded_bank_files;

  static void clear_expired_files(evutil_socket_t fd, short events, void* ctx);
};

class ClientGameData {
public:
  uint32_t guild_card_number;
  bool should_update_play_time;

  // The following fields are not saved, and are only used in certain situations

  std::array<uint32_t, 30> blocked_senders;

  // This is only used if the client is v1 or v2
  std::unique_ptr<PlayerDispDataDCPCV3> last_reported_disp_v1_v2;

  // Null unless the client is within the trade sequence (D0-D4 or EE commands)
  std::unique_ptr<PendingItemTrade> pending_item_trade;
  std::unique_ptr<PendingCardTrade> pending_card_trade;

  // Null unless the client is Episode 3 and has sent its config already
  std::shared_ptr<Episode3::PlayerConfig> ep3_config;

  // These are only used if the client is BB
  int8_t bb_character_index;
  ItemData identify_result;
  std::array<std::vector<ItemData>, 3> shop_contents;

  explicit ClientGameData(std::shared_ptr<PlayerFilesManager> files_manager);
  ~ClientGameData();

  const std::string& get_bb_username() const;
  void set_bb_username(const std::string& bb_username);

  void create_battle_overlay(std::shared_ptr<const BattleRules> rules, std::shared_ptr<const LevelTable> level_table);
  void create_challenge_overlay(Version version, size_t template_index, std::shared_ptr<const LevelTable> level_table);
  inline void delete_overlay() {
    this->overlay_character_data.reset();
  }
  inline bool has_overlay() const {
    return this->overlay_character_data.get() != nullptr;
  }

  std::shared_ptr<PSOBBBaseSystemFile> system(bool allow_load = true);
  std::shared_ptr<const PSOBBBaseSystemFile> system(bool allow_load = true) const;

  std::shared_ptr<PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true);
  std::shared_ptr<const PSOBBCharacterFile> character(bool allow_load = true, bool allow_overlay = true) const;

  std::shared_ptr<PSOBBGuildCardFile> guild_cards(bool allow_load = true);
  std::shared_ptr<const PSOBBGuildCardFile> guild_cards(bool allow_load = true) const;

  void create_character_file(
      uint32_t guild_card_number,
      uint8_t language,
      const PlayerDispDataBBPreview& preview,
      std::shared_ptr<const LevelTable> level_table);

  void save_all();
  void save_system_file() const;
  static void save_character_file(
      const std::string& filename,
      std::shared_ptr<const PSOBBBaseSystemFile> sys,
      std::shared_ptr<const PSOBBCharacterFile> character);
  // Note: This function is not const because it updates the player's play time.
  void save_character_file();
  void save_guild_card_file() const;

  PlayerBank& current_bank();
  std::shared_ptr<PSOBBCharacterFile> current_bank_character();
  bool use_shared_bank(); // Returns true if the bank exists; false if it was created
  void use_character_bank(int8_t bb_character_index);
  void use_default_bank();

private:
  std::string bb_username;
  std::shared_ptr<PlayerFilesManager> files_manager;

  // The overlay character data is used in battle and challenge modes, when
  // character data is temporarily replaced in-game. In other play modes and in
  // lobbies, overlay_character_data is null.
  std::shared_ptr<PSOBBBaseSystemFile> system_data;
  std::shared_ptr<PSOBBCharacterFile> overlay_character_data;
  std::shared_ptr<PSOBBCharacterFile> character_data;
  std::shared_ptr<PSOBBGuildCardFile> guild_card_data;
  std::shared_ptr<PlayerBank> external_bank;
  std::shared_ptr<PSOBBCharacterFile> external_bank_character;
  int8_t external_bank_character_index;
  uint64_t last_play_time_update;

  void save_and_clear_external_bank();

  void load_all_files();

  std::string system_filename() const;
  std::string character_filename(int8_t index = -1) const;
  std::string guild_card_filename() const;
  std::string shared_bank_filename() const;

  std::string legacy_player_filename() const;
  std::string legacy_account_filename() const;
};
