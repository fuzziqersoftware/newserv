#pragma once

#include <event2/event.h>

#include <deque>
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

class ProxyServer {
public:
  ProxyServer() = delete;
  ProxyServer(const ProxyServer&) = delete;
  ProxyServer(ProxyServer&&) = delete;
  ProxyServer(
      std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~ProxyServer() = default;

  void listen(uint16_t port, GameVersion version,
      const struct sockaddr_storage* default_destination = nullptr);

  void connect_client(struct bufferevent* bev, uint16_t server_port);

  struct LinkedSession {
    ProxyServer* server;
    uint64_t id;
    PrefixedLogger log;

    std::unique_ptr<struct event, void (*)(struct event*)> timeout_event;

    std::shared_ptr<const License> license;

    Channel client_channel;
    Channel server_channel;
    uint16_t local_port;
    struct sockaddr_storage next_destination;

    enum class DisconnectAction {
      LONG_TIMEOUT = 0,
      MEDIUM_TIMEOUT,
      SHORT_TIMEOUT,
      CLOSE_IMMEDIATELY,
    };
    DisconnectAction disconnect_action;

    uint8_t prev_server_command_bytes[6];
    uint32_t remote_ip_crc;
    bool enable_remote_ip_crc_patch;

    GameVersion version;
    uint32_t sub_version;
    uint8_t language;
    std::string character_name;
    std::string hardware_id; // Only used for DC sessions
    std::string login_command_bb;

    ClientOptions options;
    int64_t remote_guild_card_number;
    parray<uint8_t, 0x20> remote_client_config_data;
    ClientConfigBB newserv_client_config;
    // A null handler in here means to forward the response to the remote server
    std::deque<std::function<void(uint32_t return_value, uint32_t checksum)>> function_call_return_handler_queue;
    G_SwitchStateChanged_6x05 last_switch_enabled_command;
    PlayerInventoryItem next_drop_item;
    uint32_t next_item_id;

    struct LobbyPlayer {
      uint32_t guild_card_number;
      std::string name;
      uint8_t section_id;
      uint8_t char_class;
      LobbyPlayer() : guild_card_number(0),
                      section_id(0),
                      char_class(0) {}
    };
    std::vector<LobbyPlayer> lobby_players;
    size_t lobby_client_id;
    size_t leader_client_id;
    uint16_t area;
    float x;
    float z;
    bool is_in_game;
    bool is_in_quest;

    std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt;

    struct SavingFile {
      std::string basename;
      std::string output_filename;
      bool is_download;
      size_t remaining_bytes;
      std::deque<std::string> blocks;

      SavingFile(
          const std::string& basename,
          const std::string& output_filename,
          size_t remaining_bytes,
          bool is_download);

      void write() const;
    };
    std::unordered_map<std::string, SavingFile> saving_files;

    // TODO: This first constructor should be private
    LinkedSession(
        ProxyServer* server,
        uint64_t id,
        uint16_t local_port,
        GameVersion version);
    LinkedSession(
        ProxyServer* server,
        uint16_t local_port,
        GameVersion version,
        std::shared_ptr<const License> license,
        const ClientConfigBB& newserv_client_config);
    LinkedSession(
        ProxyServer* server,
        uint16_t local_port,
        GameVersion version,
        std::shared_ptr<const License> license,
        const struct sockaddr_storage& next_destination);
    LinkedSession(
        ProxyServer* server,
        uint64_t id,
        uint16_t local_port,
        GameVersion version,
        const struct sockaddr_storage& next_destination);

    void resume(
        Channel&& client_channel,
        std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt,
        uint32_t sub_version,
        uint8_t language,
        const std::string& character_name,
        const std::string& hardware_id);
    void resume(
        Channel&& client_channel,
        std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt,
        std::string&& login_command_bb);
    void resume(Channel&& client_channel);
    void resume_inner(
        Channel&& client_channel,
        std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt);
    void connect();

    static uint64_t timeout_for_disconnect_action(DisconnectAction action);
    static void dispatch_on_timeout(evutil_socket_t fd, short what, void* ctx);
    static void on_input(Channel& ch, uint16_t, uint32_t, std::string& msg);
    static void on_error(Channel& ch, short events);
    void on_timeout();

    void clear_lobby_players(size_t num_slots);

    void send_to_game_server(const char* error_message = nullptr);
    void disconnect();
    bool is_connected() const;
  };

  std::shared_ptr<LinkedSession> get_session();
  std::shared_ptr<LinkedSession> get_session_by_name(const std::string& name);
  std::shared_ptr<LinkedSession> create_licensed_session(
      std::shared_ptr<const License> l,
      uint16_t local_port,
      GameVersion version,
      const ClientConfigBB& newserv_client_config);
  void delete_session(uint64_t id);
  void delete_session(struct bufferevent* bev);

  size_t delete_disconnected_sessions();

private:
  struct ListeningSocket {
    ProxyServer* server;

    PrefixedLogger log;
    uint16_t port;
    scoped_fd fd;
    std::unique_ptr<struct evconnlistener, void (*)(struct evconnlistener*)> listener;
    GameVersion version;
    struct sockaddr_storage default_destination;

    ListeningSocket(
        ProxyServer* server,
        uint16_t port,
        GameVersion version,
        const struct sockaddr_storage* default_destination);

    static void dispatch_on_listen_accept(struct evconnlistener* listener,
        evutil_socket_t fd, struct sockaddr* address, int socklen, void* ctx);
    static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
    void on_listen_accept(int fd);
    void on_listen_error();
  };

  struct UnlinkedSession {
    ProxyServer* server;

    PrefixedLogger log;
    Channel channel;
    uint16_t local_port;
    GameVersion version;
    struct sockaddr_storage next_destination;

    std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt;

    UnlinkedSession(ProxyServer* server, struct bufferevent* bev, uint16_t port, GameVersion version);

    void receive_and_process_commands();

    static void on_input(Channel& ch, uint16_t command, uint32_t flag, std::string& msg);
    static void on_error(Channel& ch, short events);
  };

  std::shared_ptr<struct event_base> base;
  std::shared_ptr<struct event> destroy_sessions_ev;
  std::shared_ptr<ServerState> state;
  std::map<int, std::shared_ptr<ListeningSocket>> listeners;
  std::unordered_map<struct bufferevent*, std::shared_ptr<UnlinkedSession>> bev_to_unlinked_session;
  std::unordered_set<std::shared_ptr<UnlinkedSession>> unlinked_sessions_to_destroy;
  std::unordered_map<uint64_t, std::shared_ptr<LinkedSession>> id_to_session;
  uint64_t next_unlicensed_session_id;

  static void dispatch_destroy_sessions(evutil_socket_t, short, void* ctx);
  void destroy_sessions();

  void on_client_connect(
      struct bufferevent* bev,
      uint16_t listen_port,
      GameVersion version,
      const struct sockaddr_storage* default_destination);
};
