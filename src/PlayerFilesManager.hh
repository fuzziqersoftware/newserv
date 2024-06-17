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

class PlayerFilesManager {
public:
  explicit PlayerFilesManager(std::shared_ptr<struct event_base> base);
  ~PlayerFilesManager() = default;

  std::shared_ptr<PSOBBBaseSystemFile> get_system(const std::string& filename);
  std::shared_ptr<PSOBBCharacterFile> get_character(const std::string& filename);
  std::shared_ptr<PSOBBGuildCardFile> get_guild_card(const std::string& filename);
  std::shared_ptr<PlayerBank200> get_bank(const std::string& filename);

  void set_system(const std::string& filename, std::shared_ptr<PSOBBBaseSystemFile> file);
  void set_character(const std::string& filename, std::shared_ptr<PSOBBCharacterFile> file);
  void set_guild_card(const std::string& filename, std::shared_ptr<PSOBBGuildCardFile> file);
  void set_bank(const std::string& filename, std::shared_ptr<PlayerBank200> file);

private:
  std::shared_ptr<struct event_base> base;
  std::unique_ptr<struct event, void (*)(struct event*)> clear_expired_files_event;

  std::unordered_map<std::string, std::shared_ptr<PSOBBBaseSystemFile>> loaded_system_files;
  std::unordered_map<std::string, std::shared_ptr<PSOBBCharacterFile>> loaded_character_files;
  std::unordered_map<std::string, std::shared_ptr<PSOBBGuildCardFile>> loaded_guild_card_files;
  std::unordered_map<std::string, std::shared_ptr<PlayerBank200>> loaded_bank_files;

  static void clear_expired_files(evutil_socket_t fd, short events, void* ctx);
};
