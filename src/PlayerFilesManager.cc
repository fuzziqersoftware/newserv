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

PlayerFilesManager::PlayerFilesManager(std::shared_ptr<struct event_base> base)
    : base(base),
      clear_expired_files_event(
          event_new(this->base.get(), -1, EV_TIMEOUT | EV_PERSIST, &PlayerFilesManager::clear_expired_files, this),
          event_free) {
  auto tv = phosg::usecs_to_timeval(30 * 1000 * 1000);
  event_add(this->clear_expired_files_event.get(), &tv);
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

void PlayerFilesManager::clear_expired_files(evutil_socket_t, short, void* ctx) {
  auto* self = reinterpret_cast<PlayerFilesManager*>(ctx);
  size_t num_deleted = erase_unused(self->loaded_system_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired system file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_character_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired character file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_guild_card_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired Guild Card file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_bank_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired bank file(s)", num_deleted);
  }
}
