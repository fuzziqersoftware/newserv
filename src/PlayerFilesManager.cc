#include "PlayerFilesManager.hh"

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
#include "PSOProtocol.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

using namespace std;

PlayerFilesManager::PlayerFilesManager(std::shared_ptr<asio::io_context> io_context)
    : io_context(io_context),
      clear_expired_files_timer(*this->io_context) {
  this->schedule_callback();
}

std::shared_ptr<PSOBBBaseSystemFile> PlayerFilesManager::get_system(const std::string& filename) {
  try {
    return this->loaded_system_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PSOBBCharacterFile> PlayerFilesManager::get_character(const std::string& filename) {
  try {
    return this->loaded_character_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PSOBBGuildCardFile> PlayerFilesManager::get_guild_card(const std::string& filename) {
  try {
    return this->loaded_guild_card_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PlayerBank200> PlayerFilesManager::get_bank(const std::string& filename) {
  try {
    return this->loaded_bank_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

void PlayerFilesManager::set_system(const std::string& filename, std::shared_ptr<PSOBBBaseSystemFile> file) {
  if (!this->loaded_system_files.emplace(filename, file).second) {
    throw runtime_error("Guild Card file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_character(const std::string& filename, std::shared_ptr<PSOBBCharacterFile> file) {
  if (!this->loaded_character_files.emplace(filename, file).second) {
    throw runtime_error("character file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_guild_card(const std::string& filename, std::shared_ptr<PSOBBGuildCardFile> file) {
  if (!this->loaded_guild_card_files.emplace(filename, file).second) {
    throw runtime_error("Guild Card file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_bank(const std::string& filename, std::shared_ptr<PlayerBank200> file) {
  if (!this->loaded_bank_files.emplace(filename, file).second) {
    throw runtime_error("bank file already loaded: " + filename);
  }
}

void PlayerFilesManager::schedule_callback() {
  this->clear_expired_files_timer.expires_after(std::chrono::seconds(30));
  this->clear_expired_files_timer.async_wait(bind(&PlayerFilesManager::clear_expired_files, this));
}

template <typename KeyT, typename ValueT>
size_t erase_unused(std::unordered_map<KeyT, std::shared_ptr<ValueT>>& m) {
  size_t ret = 0;
  for (auto it = m.begin(); it != m.end();) {
    if (it->second.use_count() <= 1) {
      it = m.erase(it);
      ret++;
    } else {
      it++;
    }
  }
  return ret;
}

void PlayerFilesManager::clear_expired_files() {
  size_t num_deleted = erase_unused(this->loaded_system_files);
  if (num_deleted) {
    player_data_log.info_f("Cleared {} expired system file(s)", num_deleted);
  }
  num_deleted = erase_unused(this->loaded_character_files);
  if (num_deleted) {
    player_data_log.info_f("Cleared {} expired character file(s)", num_deleted);
  }
  num_deleted = erase_unused(this->loaded_guild_card_files);
  if (num_deleted) {
    player_data_log.info_f("Cleared {} expired Guild Card file(s)", num_deleted);
  }
  num_deleted = erase_unused(this->loaded_bank_files);
  if (num_deleted) {
    player_data_log.info_f("Cleared {} expired bank file(s)", num_deleted);
  }

  this->schedule_callback();
}
