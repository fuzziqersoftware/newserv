#pragma once

#include "WindowsPlatform.hh"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Channel.hh"
#include "ItemCreator.hh"
#include "Map.hh"

struct ServerState;

struct ProxySession {
  static size_t num_proxy_sessions;

  std::shared_ptr<Channel> server_channel;

  parray<uint8_t, 6> prev_server_command_bytes;
  uint32_t remote_ip_crc = 0;
  bool received_reconnect = false;
  bool enable_remote_ip_crc_patch = false;
  bool bb_client_sent_E0 = false;

  struct LobbyPlayer {
    uint32_t guild_card_number = 0;
    uint64_t xb_user_id = 0;
    std::string name;
    uint8_t language = 0;
    uint8_t section_id = 0;
    uint8_t char_class = 0;
  };
  std::vector<LobbyPlayer> lobby_players;
  bool is_in_lobby = false;
  bool is_in_game = false;
  bool is_in_quest = false;
  uint8_t leader_client_id = 0;
  uint8_t lobby_event = 0;
  uint8_t lobby_difficulty = 0;
  uint8_t lobby_section_id = 0;
  GameMode lobby_mode = GameMode::NORMAL;
  Episode lobby_episode = Episode::EP1;
  uint32_t lobby_random_seed = 0;
  uint64_t server_ping_start_time = 0;
  bool suppress_next_ep3_media_update_confirmation = false;

  int64_t remote_guild_card_number = -1;
  parray<uint8_t, 0x28> remote_client_config_data;

  ProxyDropMode drop_mode = ProxyDropMode::PASSTHROUGH;
  std::shared_ptr<std::string> quest_dat_data;
  std::shared_ptr<ItemCreator> item_creator;
  std::shared_ptr<MapState> map_state;
  // TODO: Be less lazy and track item IDs correctly in proxy games. (Then
  // change this to use the actual client's next item ID, not this hardcoded
  // default.)
  uint32_t next_item_id = 0x44000000;

  struct PersistentConfig {
    uint32_t account_id;
    uint32_t remote_guild_card_number;
    bool enable_remote_ip_crc_patch;
    std::unique_ptr<asio::steady_timer> expire_timer;
  };

  explicit ProxySession(std::shared_ptr<Channel> server_channel, const PersistentConfig* pc);
  ~ProxySession();

  struct SavingFile {
    std::string basename;
    std::string output_filename;
    bool is_download;
    size_t total_size;
    std::string data;
  };
  std::unordered_map<std::string, SavingFile> saving_files;

  void set_drop_mode(std::shared_ptr<ServerState> s, Version version, int64_t override_random_seed, ProxyDropMode new_mode);

  void clear_lobby_players(size_t num_slots);
};
