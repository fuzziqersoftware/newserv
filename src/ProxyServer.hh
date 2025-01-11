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

class ProxyServer : public std::enable_shared_from_this<ProxyServer> {
public:
  ProxyServer() = delete;
  ProxyServer(const ProxyServer&) = delete;
  ProxyServer(ProxyServer&&) = delete;
  ProxyServer(
      std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~ProxyServer() = default;

  void listen(const std::string& addr, uint16_t port, Version version, const struct sockaddr_storage* default_destination = nullptr);

  void connect_virtual_client(struct bufferevent* bev, uint64_t virtual_network_id, uint16_t server_port);

  struct LinkedSession : std::enable_shared_from_this<LinkedSession> {
    std::weak_ptr<ProxyServer> server;
    uint64_t id;
    phosg::PrefixedLogger log;

    std::unique_ptr<struct event, void (*)(struct event*)> timeout_event;

    std::shared_ptr<Login> login;

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

    uint32_t sub_version;
    std::string character_name;
    std::string serial_number2; // Only used for DC sessions
    uint64_t hardware_id;
    std::string login_command_bb;
    XBNetworkLocation xb_netloc;
    parray<le_uint32_t, 3> xb_9E_unknown_a1a;

    uint32_t challenge_rank_color_override;
    std::string challenge_rank_title_override;
    int64_t remote_guild_card_number;
    parray<uint8_t, 0x20> remote_client_config_data;
    Client::Config config;
    // A null handler in here means to forward the response to the remote server
    std::deque<std::function<void(uint32_t return_value, uint32_t checksum)>> function_call_return_handler_queue;
    ItemData next_drop_item;
    uint32_t next_item_id;

    enum class DropMode {
      DISABLED = 0,
      PASSTHROUGH,
      INTERCEPT,
    };
    DropMode drop_mode;
    std::shared_ptr<std::string> quest_dat_data;
    std::shared_ptr<ItemCreator> item_creator;
    std::shared_ptr<MapState> map_state;

    struct LobbyPlayer {
      uint32_t guild_card_number = 0;
      uint64_t xb_user_id = 0;
      std::string name;
      uint8_t language = 0;
      uint8_t section_id = 0;
      uint8_t char_class = 0;
    };
    std::vector<LobbyPlayer> lobby_players;
    size_t lobby_client_id;
    size_t leader_client_id;
    uint16_t floor;
    VectorXZF pos;
    bool is_in_game;
    bool is_in_quest;
    uint8_t lobby_event;
    uint8_t lobby_difficulty;
    uint8_t lobby_section_id;
    GameMode lobby_mode;
    Episode lobby_episode;
    uint32_t lobby_random_seed;
    uint64_t client_ping_start_time = 0;
    uint64_t server_ping_start_time = 0;

    std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt;

    struct SavingFile {
      std::string basename;
      std::string output_filename;
      bool is_download;
      size_t total_size;
      std::string data;

      SavingFile(
          const std::string& basename,
          const std::string& output_filename,
          size_t total_size,
          bool is_download);
    };
    std::unordered_map<std::string, SavingFile> saving_files;

    // TODO: This first constructor should be private
    LinkedSession(
        std::shared_ptr<ProxyServer> server,
        uint64_t id,
        uint16_t local_port,
        Version version);
    LinkedSession(
        std::shared_ptr<ProxyServer> server,
        uint16_t local_port,
        Version version,
        std::shared_ptr<Login> login,
        const Client::Config& config);
    LinkedSession(
        std::shared_ptr<ProxyServer> server,
        uint16_t local_port,
        Version version,
        std::shared_ptr<Login> login,
        const struct sockaddr_storage& next_destination);
    LinkedSession(
        std::shared_ptr<ProxyServer> server,
        uint64_t id,
        uint16_t local_port,
        Version version,
        const struct sockaddr_storage& next_destination);

    std::shared_ptr<ProxyServer> require_server() const;
    std::shared_ptr<ServerState> require_server_state() const;

    inline Version version() const {
      return this->client_channel.version;
    }
    inline uint8_t language() const {
      return this->client_channel.language;
    }
    inline uint32_t effective_sub_version() const {
      return this->config.check_flag(Client::Flag::PROXY_VIRTUAL_CLIENT)
          ? default_sub_version_for_version(this->version())
          : this->sub_version;
    }
    void set_version(Version v);

    void resume(
        Channel&& client_channel,
        std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt,
        uint32_t sub_version,
        const std::string& character_name,
        const std::string& serial_number2,
        uint64_t hardware_id,
        const XBNetworkLocation& xb_netloc,
        const parray<le_uint32_t, 3>& xb_9E_unknown_a1a);
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

    void update_channel_names();

    void clear_lobby_players(size_t num_slots);

    void set_drop_mode(DropMode new_mode);

    void send_to_game_server(const char* error_message = nullptr);
    void disconnect();
    bool is_connected() const;
  };

  std::shared_ptr<LinkedSession> get_session() const;
  std::shared_ptr<LinkedSession> get_session_by_name(const std::string& name) const;
  const std::unordered_map<uint64_t, std::shared_ptr<LinkedSession>>& all_sessions() const;

  std::shared_ptr<LinkedSession> create_logged_in_session(
      std::shared_ptr<Login> login,
      uint16_t local_port,
      Version version,
      const Client::Config& config);
  void delete_session(uint64_t id);

  size_t num_sessions() const;

  size_t delete_disconnected_sessions();

private:
  struct ListeningSocket {
    ProxyServer* server;

    phosg::PrefixedLogger log;
    uint16_t port;
    phosg::scoped_fd fd;
    std::unique_ptr<struct evconnlistener, void (*)(struct evconnlistener*)> listener;
    Version version;
    struct sockaddr_storage default_destination;

    ListeningSocket(
        ProxyServer* server,
        const std::string& addr,
        uint16_t port,
        Version version,
        const struct sockaddr_storage* default_destination);

    static void dispatch_on_listen_accept(struct evconnlistener* listener,
        evutil_socket_t fd, struct sockaddr* address, int socklen, void* ctx);
    static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
    void on_listen_accept(int fd);
    void on_listen_error();
  };

  struct UnlinkedSession {
    std::weak_ptr<ProxyServer> server;
    uint64_t id;

    phosg::PrefixedLogger log;
    Channel channel;
    uint16_t local_port;
    struct sockaddr_storage next_destination;

    std::shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt;

    // Temporary state used just before resuming a LinkedSession. These aren't
    // just local variables inside on_input because XB requires two commands to
    // get started (9E and 9F), so we need to store this state somewhere between
    // those commands.
    std::shared_ptr<Login> login;
    uint32_t sub_version = 0;
    std::string character_name;
    Client::Config config;
    std::string login_command_bb;
    std::string serial_number2;
    uint64_t hardware_id;
    XBNetworkLocation xb_netloc;
    parray<le_uint32_t, 3> xb_9E_unknown_a1a;

    UnlinkedSession(
        std::shared_ptr<ProxyServer> server,
        uint64_t id,
        struct bufferevent* bev,
        uint64_t virtual_network_id,
        uint16_t port,
        Version version);

    std::shared_ptr<ProxyServer> require_server() const;
    std::shared_ptr<ServerState> require_server_state() const;

    inline Version version() const {
      return this->channel.version;
    }

    void set_login(std::shared_ptr<Login> login);

    void receive_and_process_commands();

    static void on_input(Channel& ch, uint16_t command, uint32_t flag, std::string& msg);
    static void on_error(Channel& ch, short events);
  };

  std::shared_ptr<struct event_base> base;
  std::shared_ptr<struct event> destroy_sessions_ev;
  std::shared_ptr<ServerState> state;
  std::map<int, std::shared_ptr<ListeningSocket>> listeners;
  std::unordered_map<uint64_t, std::shared_ptr<UnlinkedSession>> id_to_unlinked_session;
  std::unordered_set<std::shared_ptr<UnlinkedSession>> unlinked_sessions_to_destroy;
  std::unordered_map<uint64_t, std::shared_ptr<LinkedSession>> id_to_linked_session;
  uint64_t next_unlinked_session_id;
  uint64_t next_logged_out_session_id;

  static void dispatch_destroy_sessions(evutil_socket_t, short, void* ctx);
  void destroy_sessions();

  void on_client_connect(
      struct bufferevent* bev,
      uint64_t virtual_network_id,
      uint16_t listen_port,
      Version version,
      const struct sockaddr_storage* default_destination);

  static constexpr uint64_t FIRST_UNLINKED_SESSION_ID = 0x0000000000000001;
  static constexpr uint64_t FIRST_LINKED_LOGGED_OUT_SESSION_ID = 0x0000000080000000;
  static constexpr uint64_t FIRST_LINKED_LOGGED_IN_SESSION_ID = 0x00000000FFFFFFFF;
};
