#include "Player.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <stdexcept>

#include "FileContentsCache.hh"
#include "ItemData.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

using namespace std;

// Originally there was going to be a language-based header, but then I decided
// against it. This string was already in use for that parser, so I didn't
// bother changing it.
static const string ACCOUNT_FILE_SIGNATURE =
    "newserv account file format; 7 sections present; sequential;";

ClientGameData::ClientGameData()
    : last_play_time_update(0),
      guild_card_number(0),
      should_update_play_time(false),
      bb_player_index(0),
      should_save(true) {}

ClientGameData::~ClientGameData() {
  if (!this->bb_username.empty()) {
    if (this->account_data.get()) {
      this->save_account_data();
    }
    if (this->player_data.get()) {
      this->save_player_data();
    }
  }
}

shared_ptr<SavedAccountDataBB> ClientGameData::account(bool should_load) {
  if (!this->account_data.get() && should_load) {
    if (this->bb_username.empty()) {
      this->account_data.reset(new SavedAccountDataBB());
      this->account_data->signature = ACCOUNT_FILE_SIGNATURE;
    } else {
      this->load_account_data();
    }
  }
  return this->account_data;
}

shared_ptr<SavedPlayerDataBB> ClientGameData::player(bool should_load) {
  if (!this->player_data.get() && should_load) {
    if (this->bb_username.empty()) {
      this->player_data.reset(new SavedPlayerDataBB());
    } else {
      this->load_player_data();
    }
  }
  return this->player_data;
}

shared_ptr<const SavedAccountDataBB> ClientGameData::account() const {
  if (!this->account_data.get()) {
    throw runtime_error("account data is not loaded");
  }
  return this->account_data;
}

shared_ptr<const SavedPlayerDataBB> ClientGameData::player() const {
  if (!this->player_data.get()) {
    throw runtime_error("player data is not loaded");
  }
  return this->player_data;
}

string ClientGameData::account_data_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have account data");
  }
  return string_printf("system/players/account_%s.nsa",
      this->bb_username.c_str());
}

string ClientGameData::player_data_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have account data");
  }
  return string_printf("system/players/player_%s_%zu.nsc",
      this->bb_username.c_str(), this->bb_player_index + 1);
}

string ClientGameData::player_template_filename(uint8_t char_class) {
  return string_printf("system/players/default_player_%hhu.nsc", char_class);
}

void ClientGameData::create_player(
    const PlayerDispDataBBPreview& preview,
    shared_ptr<const LevelTable> level_table) {
  shared_ptr<SavedPlayerDataBB> data(new SavedPlayerDataBB(
      load_object_file<SavedPlayerDataBB>(player_template_filename(preview.visual.char_class))));
  data->update_to_latest_version();

  try {
    data->disp.apply_preview(preview);
    data->disp.stats.char_stats = level_table->base_stats_for_class(data->disp.visual.char_class);
  } catch (const exception& e) {
    throw runtime_error(string_printf("template application failed: %s", e.what()));
  }

  this->player_data = data;

  this->save_player_data();
}

void ClientGameData::load_account_data() {
  string filename = this->account_data_filename();

  shared_ptr<SavedAccountDataBB> data;
  try {
    data.reset(new SavedAccountDataBB(
        player_files_cache.get_obj_or_load<SavedAccountDataBB>(filename).obj));
    if (data->signature != ACCOUNT_FILE_SIGNATURE) {
      throw runtime_error("account data header is incorrect");
    }
    player_data_log.info("Loaded account data file %s", filename.c_str());

  } catch (const exception& e) {
    player_data_log.info("Cannot load account data for %s (%s); using default",
        this->bb_username.c_str(), e.what());
    player_files_cache.delete_key(filename);
    data.reset(new SavedAccountDataBB(
        player_files_cache.get_obj_or_load<SavedAccountDataBB>(
                              "system/players/default.nsa")
            .obj));
    if (data->signature != ACCOUNT_FILE_SIGNATURE) {
      throw runtime_error("default account data header is incorrect");
    }
    player_data_log.info("Loaded default account data file");
  }

  this->account_data = data;
}

void ClientGameData::save_account_data() const {
  if (!this->account_data.get()) {
    throw logic_error("save_account_data called when no account data loaded");
  }
  string filename = this->account_data_filename();
  player_files_cache.replace(filename, this->account_data.get(), sizeof(SavedAccountDataBB));
  if (this->should_save) {
    save_file(filename, this->account_data.get(), sizeof(SavedAccountDataBB));
    player_data_log.info("Saved account data file %s to filesystem", filename.c_str());
  } else {
    player_data_log.info("Saved account data file %s to cache only", filename.c_str());
  }
}

void ClientGameData::load_player_data() {
  this->last_play_time_update = now();
  string filename = this->player_data_filename();
  this->player_data.reset(new SavedPlayerDataBB(
      player_files_cache.get_obj_or_load<SavedPlayerDataBB>(filename).obj));
  try {
    this->player_data->update_to_latest_version();
  } catch (const exception&) {
    this->player_data.reset();
    player_files_cache.delete_key(filename);
    throw;
  }
  player_data_log.info("Loaded player data file %s", filename.c_str());
}

void ClientGameData::save_player_data() {
  if (!this->player_data.get()) {
    throw logic_error("save_player_data called when no player data loaded");
  }
  if (this->should_update_play_time) {
    // This is slightly inaccurate, since fractions of a second are truncated
    // off each time we save. I'm lazy, so insert shrug emoji here.
    uint64_t t = now();
    uint64_t seconds = (t - this->last_play_time_update) / 1000000;
    this->player_data->disp.play_time += seconds;
    player_data_log.info("Added %" PRIu64 " seconds to play time", seconds);
    this->last_play_time_update = t;
  }
  string filename = this->player_data_filename();
  player_files_cache.replace(filename, this->player_data.get(), sizeof(SavedPlayerDataBB));
  if (this->should_save) {
    save_file(filename, this->player_data.get(), sizeof(SavedPlayerDataBB));
    player_data_log.info("Saved player data file %s to filesystem", filename.c_str());
  } else {
    player_data_log.info("Saved player data file %s to cache only", filename.c_str());
  }
}

void SavedPlayerDataBB::update_to_latest_version() {
  if (this->signature == PLAYER_FILE_SIGNATURE_V0) {
    this->signature = PLAYER_FILE_SIGNATURE_V1;
    this->unused.clear();
    this->battle_records.place_counts.clear(0);
    this->battle_records.disconnect_count = 0;
    this->battle_records.unknown_a1.clear(0);
  } else if (this->signature != PLAYER_FILE_SIGNATURE_V1) {
    throw runtime_error("player data has incorrect signature");
  }
}

// TODO: Eliminate duplication between this function and the parallel function
// in PlayerBank
void SavedPlayerDataBB::add_item(const PlayerInventoryItem& item) {
  uint32_t pid = item.data.primary_identifier();

  // Annoyingly, meseta is in the disp data, not in the inventory struct. If the
  // item is meseta, we have to modify disp instead.
  if (pid == MESETA_IDENTIFIER) {
    this->add_meseta(item.data.data2d);
    return;
  }

  // Handle combinable items
  size_t combine_max = item.data.max_stack_size();
  if (combine_max > 1) {
    // Get the item index if there's already a stack of the same item in the
    // player's inventory
    size_t y;
    for (y = 0; y < this->inventory.num_items; y++) {
      if (this->inventory.items[y].data.primary_identifier() == item.data.primary_identifier()) {
        break;
      }
    }

    // If we found an existing stack, add it to the total and return
    if (y < this->inventory.num_items) {
      this->inventory.items[y].data.data1[5] += item.data.data1[5];
      if (this->inventory.items[y].data.data1[5] > combine_max) {
        this->inventory.items[y].data.data1[5] = combine_max;
      }
      return;
    }
  }

  // If we get here, then it's not meseta and not a combine item, so it needs to
  // go into an empty inventory slot
  if (this->inventory.num_items >= 30) {
    throw runtime_error("inventory is full");
  }
  this->inventory.items[this->inventory.num_items] = item;
  this->inventory.num_items++;
}

// TODO: Eliminate code duplication between this function and the parallel
// function in PlayerBank
PlayerInventoryItem SavedPlayerDataBB::remove_item(
    uint32_t item_id, uint32_t amount, bool allow_meseta_overdraft) {
  PlayerInventoryItem ret;

  // If we're removing meseta (signaled by an invalid item ID), then create a
  // meseta item.
  if (item_id == 0xFFFFFFFF) {
    this->remove_meseta(amount, allow_meseta_overdraft);
    ret.data.data1[0] = 0x04;
    ret.data.data2d = amount;
    return ret;
  }

  size_t index = this->inventory.find_item(item_id);
  auto& inventory_item = this->inventory.items[index];

  // If the item is a combine item and are we removing less than we have of it,
  // then create a new item and reduce the amount of the existing stack. Note
  // that passing amount == 0 means to remove the entire stack, so this only
  // applies if amount is nonzero.
  if (amount && (inventory_item.data.stack_size() > 1) &&
      (amount < inventory_item.data.data1[5])) {
    ret = inventory_item;
    ret.data.data1[5] = amount;
    ret.data.id = 0xFFFFFFFF;
    inventory_item.data.data1[5] -= amount;
    return ret;
  }

  // If we get here, then it's not meseta, and either it's not a combine item or
  // we're removing the entire stack. Delete the item from the inventory slot
  // and return the deleted item.
  ret = inventory_item;
  this->inventory.num_items--;
  for (size_t x = index; x < this->inventory.num_items; x++) {
    this->inventory.items[x] = this->inventory.items[x + 1];
  }
  this->inventory.items[this->inventory.num_items] = PlayerInventoryItem();
  return ret;
}

void SavedPlayerDataBB::add_meseta(uint32_t amount) {
  this->disp.stats.meseta = min<size_t>(static_cast<size_t>(this->disp.stats.meseta) + amount, 999999);
}

void SavedPlayerDataBB::remove_meseta(uint32_t amount, bool allow_overdraft) {
  if (amount <= this->disp.stats.meseta) {
    this->disp.stats.meseta -= amount;
  } else if (allow_overdraft) {
    this->disp.stats.meseta = 0;
  } else {
    throw out_of_range("player does not have enough meseta");
  }
}
