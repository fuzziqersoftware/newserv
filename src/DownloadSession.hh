#pragma once

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
      std::shared_ptr<asio::io_context> io_context,
      const std::string& remote_host,
      uint16_t remote_port,
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

  asio::awaitable<void> run();

protected:
  // Config (must be set by caller)
  std::string remote_host;
  uint16_t remote_port;
  std::string output_dir;
  Version version;
  uint8_t language;
  bool show_command_data;
  std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file;
  uint32_t serial_number;
  uint32_t serial_number2;
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
  std::shared_ptr<asio::io_context> io_context;
  std::shared_ptr<Channel> channel;
  uint64_t hardware_id;
  uint32_t guild_card_number = 0;
  parray<uint8_t, 0x28> prev_cmd_data;
  parray<uint8_t, 0x20> client_config;
  bool sent_96 = false;
  std::vector<S_LobbyListEntry_83> lobby_menu_items;

  bool should_request_category_list = true;
  uint64_t current_request = 0;
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
  size_t current_game_config_index = 0;
  bool in_game = false;
  bool bin_complete = false;
  bool dat_complete = false;

  void send_93_9D_9E(bool extended);
  void send_61_98(bool is_98);
  asio::awaitable<void> on_message(Channel::Message& msg);
  void send_next_request();
  void on_request_complete();
};
