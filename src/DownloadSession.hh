#pragma once

#include <event2/event.h>

#include <functional>
#include <map>
#include <memory>
#include <phosg/Filesystem.hh>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

class DownloadSession {
public:
  DownloadSession(
      std::shared_ptr<struct event_base> base,
      const struct sockaddr_storage& remote,
      const std::string& output_dir,
      Version version,
      uint8_t language,
      std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file,
      uint32_t serial_number2,
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& username,
      const std::string& password,
      const std::string& xb_gamertag,
      uint64_t xb_user_id,
      uint64_t xb_account_id,
      std::shared_ptr<PSOBBCharacterFile> character,
      const std::unordered_set<std::string>& ship_menu_selections,
      const std::vector<std::string>& on_request_complete_commands,
      bool interactive,
      bool show_command_data);
  DownloadSession(const DownloadSession&) = delete;
  DownloadSession(DownloadSession&&) = delete;
  DownloadSession& operator=(const DownloadSession&) = delete;
  DownloadSession& operator=(DownloadSession&&) = delete;
  virtual ~DownloadSession() = default;

protected:
  // Config (must be set by caller)
  std::string output_dir;
  std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file;
  uint32_t serial_number2;
  uint32_t serial_number;
  std::string access_key;
  std::string username;
  std::string password;
  std::string xb_gamertag;
  uint64_t xb_user_id;
  uint64_t xb_account_id;
  std::shared_ptr<PSOBBCharacterFile> character;
  std::unordered_set<std::string> ship_menu_selections;
  std::vector<std::string> on_request_complete_commands;
  bool interactive;

  // State (set during session)
  phosg::PrefixedLogger log;
  std::shared_ptr<struct event_base> base;
  Channel channel;
  uint64_t hardware_id;
  uint32_t guild_card_number;
  parray<uint8_t, 0x28> prev_cmd_data;
  parray<uint8_t, 0x20> client_config;
  bool sent_96;
  std::vector<S_LobbyListEntry_83> lobby_menu_items;

  bool should_request_category_list;
  uint64_t current_request;
  std::map<uint64_t, std::string> pending_requests;
  std::unordered_set<uint64_t> done_requests;

  struct OpenFile {
    uint64_t request;
    std::string filename;
    size_t total_size;
    std::string data;
  };
  std::unordered_map<std::string, OpenFile> open_files;

  struct GameConfig {
    GameMode mode;
    Episode episode;
    bool v1;
    bool v2;
    bool v3;
  };
  static const std::vector<GameConfig> game_configs;
  size_t current_game_config_index;
  bool in_game;
  bool bin_complete;
  bool dat_complete;

  static void dispatch_on_channel_input(Channel& ch, uint16_t command, uint32_t flag, std::string& msg);
  static void dispatch_on_channel_error(Channel& ch, short events);
  void on_channel_input(uint16_t command, uint32_t flag, std::string& msg);
  void on_channel_error(short events);

  void send_next_request();
  void on_request_complete();

  void assign_item_ids(uint32_t base_item_id);
  void send_93_9D_9E(bool extended);
  void send_61_98(bool is_98);
};
