#pragma once

#include <stdint.h>

#include <asio.hpp>
#include <list>
#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Process.hh>
#include <string>

#include "AsyncUtils.hh"
#include "Channel.hh"
#include "IPFrameInfo.hh"
#include "Server.hh"
#include "ServerState.hh"
#include "Text.hh"

class IPStackSimulator;
class IPSSChannel;

constexpr size_t DEFAULT_RESEND_PUSH_USECS = 200000; // 200ms

enum class VirtualNetworkProtocol {
  ETHERNET_TAPSERVER = 0,
  HDLC_TAPSERVER,
  HDLC_RAW,
};

struct IPSSSocket : ServerSocket {
  VirtualNetworkProtocol protocol;
};

struct IPSSClient : std::enable_shared_from_this<IPSSClient> {
  std::shared_ptr<asio::io_context> io_context;
  std::weak_ptr<IPStackSimulator> sim;
  uint64_t network_id;
  asio::ip::tcp::socket sock;
  VirtualNetworkProtocol protocol;
  uint32_t hdlc_escape_control_character_flags = 0xFFFFFFFF;
  uint32_t hdlc_remote_magic_number = 0;
  parray<uint8_t, 6> mac_addr; // Only used for LinkType::ETHERNET
  uint32_t ipv4_addr;
  asio::steady_timer idle_timeout_timer;

  struct TCPConnection {
    std::weak_ptr<IPSSClient> client;
    std::shared_ptr<IPSSChannel> server_channel;
    bool awaiting_first_ack = true;
    bool awaiting_ack = false;
    uint32_t server_addr = 0;
    uint16_t server_port = 0;
    uint16_t client_port = 0;
    uint32_t next_client_seq = 0;
    uint32_t acked_server_seq = 0;
    size_t resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
    size_t next_push_max_frame_size = 1024;
    size_t max_frame_size = 1024;
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
    size_t outbound_data_bytes = 0;
    std::list<std::string> outbound_data;
    asio::steady_timer resend_push_timer;

    TCPConnection(std::shared_ptr<IPSSClient> client);

    inline uint64_t key() const {
      return (static_cast<uint64_t>(this->server_addr) << 32) |
          (static_cast<uint64_t>(this->server_port) << 16) |
          static_cast<uint64_t>(this->client_port);
    }
    static inline uint64_t key(const IPv4Header& ipv4, const TCPHeader& tcp) {
      return (static_cast<uint64_t>(ipv4.dest_addr) << 32) |
          (static_cast<uint64_t>(tcp.dest_port) << 16) |
          static_cast<uint64_t>(tcp.src_port);
    }
    static inline uint64_t key(const FrameInfo& fi) {
      if (!fi.ipv4 || !fi.tcp) {
        throw std::logic_error("tcp_conn_key_for_frame called on non-TCP frame");
      }
      return key(*fi.ipv4, *fi.tcp);
    }

    void drain_outbound_data(size_t bytes);
    void linearize_outbound_data(size_t bytes);
  };
  std::unordered_map<uint64_t, std::shared_ptr<TCPConnection>> tcp_connections;

  IPSSClient(
      std::shared_ptr<IPStackSimulator> sim,
      uint64_t network_id,
      VirtualNetworkProtocol protocol,
      asio::ip::tcp::socket&& sock);
  void reschedule_idle_timeout();
};

// IPSSChannel provides an "unwrapped" connection to the rest of the server. It
// implements the Channel interface and can be used in place of an
// SocketChannel, so the rest of the server doesn't have to know about
// IPStackSimulator.
class IPSSChannel : public Channel {
public:
  std::shared_ptr<IPStackSimulator> sim;
  std::weak_ptr<IPSSClient> ipss_client;
  std::weak_ptr<IPSSClient::TCPConnection> tcp_conn;

  IPSSChannel(
      std::shared_ptr<IPStackSimulator> sim,
      std::weak_ptr<IPSSClient> ipss_client,
      std::weak_ptr<IPSSClient::TCPConnection> tcp_conn,
      Version version,
      Language language,
      const std::string& name = "",
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);

  virtual std::string default_name() const;

  virtual bool connected() const;
  virtual void disconnect();

  // Adds inbound data, which will then be available via recv_raw(). This
  // function is called by IPStackSimulator to forward "unwrapped" data to
  // the game/proxy servers.
  void add_inbound_data(const void* data, size_t size);

  virtual void send_raw(std::string&& data);
  virtual asio::awaitable<void> recv_raw(void* data, size_t size);

private:
  AsyncEvent data_available_signal;
  std::deque<std::string> inbound_data;
  void* recv_buf = nullptr;
  size_t recv_buf_size = 0;
};

class IPStackSimulator
    : public Server<IPSSClient, IPSSSocket>,
      public std::enable_shared_from_this<IPStackSimulator> {
public:
  IPStackSimulator(std::shared_ptr<ServerState> state);
  ~IPStackSimulator() = default;

  void listen(const std::string& name, const std::string& addr, int port, VirtualNetworkProtocol protocol);

  static uint32_t connect_address_for_remote_address(uint32_t remote_addr);

  inline std::shared_ptr<ServerState> get_state() {
    return this->state;
  }

private:
  std::shared_ptr<ServerState> state;
  uint64_t next_network_id = 1;

  parray<uint8_t, 6> host_mac_address_bytes;
  parray<uint8_t, 6> broadcast_mac_address_bytes;

  static uint64_t tcp_conn_key_for_connection(std::shared_ptr<const IPSSClient::TCPConnection> conn);
  static uint64_t tcp_conn_key_for_client_frame(const IPv4Header& ipv4, const TCPHeader& tcp);
  static uint64_t tcp_conn_key_for_client_frame(const FrameInfo& fi);

  static std::string str_for_ipv4_netloc(uint32_t addr, uint16_t port);
  static std::string str_for_tcp_connection(
      std::shared_ptr<const IPSSClient> c, std::shared_ptr<const IPSSClient::TCPConnection> conn);

  asio::awaitable<void> send_ethernet_tapserver_frame(
      std::shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size) const;
  asio::awaitable<void> send_hdlc_frame(
      std::shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size, bool is_raw) const;
  asio::awaitable<void> send_layer3_frame(
      std::shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size) const;
  [[nodiscard]] inline asio::awaitable<void> send_layer3_frame(
      std::shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const std::string& data) const {
    return this->send_layer3_frame(c, proto, data.data(), data.size());
  }

  asio::awaitable<void> on_client_frame(std::shared_ptr<IPSSClient> c, const void* data, size_t size);
  asio::awaitable<void> on_client_lcp_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);
  asio::awaitable<void> on_client_pap_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);
  asio::awaitable<void> on_client_ipcp_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);
  asio::awaitable<void> on_client_arp_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);
  asio::awaitable<void> on_client_udp_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);
  asio::awaitable<void> on_client_tcp_frame(std::shared_ptr<IPSSClient> c, const FrameInfo& fi);

  void schedule_send_pending_push_frame(std::shared_ptr<IPSSClient::TCPConnection> conn, uint64_t delay_usecs);
  asio::awaitable<void> send_pending_push_frame(
      std::shared_ptr<IPSSClient> c, std::shared_ptr<IPSSClient::TCPConnection> conn);
  asio::awaitable<void> send_tcp_frame(
      std::shared_ptr<IPSSClient> c,
      std::shared_ptr<IPSSClient::TCPConnection> conn,
      uint16_t flags = 0,
      const void* payload_data = nullptr,
      size_t payload_bytes = 0);

  asio::awaitable<void> open_server_connection(
      std::shared_ptr<IPSSClient> c, std::shared_ptr<IPSSClient::TCPConnection> conn);
  asio::awaitable<void> close_tcp_connection(
      std::shared_ptr<IPSSClient> c, std::shared_ptr<IPSSClient::TCPConnection> conn);

  [[nodiscard]] virtual std::shared_ptr<IPSSClient> create_client(
      std::shared_ptr<IPSSSocket> listen_sock, asio::ip::tcp::socket&& client_sock);
  asio::awaitable<void> handle_tapserver_client(std::shared_ptr<IPSSClient> c);
  asio::awaitable<void> handle_hdlc_raw_client(std::shared_ptr<IPSSClient> c);
  virtual asio::awaitable<void> handle_client(std::shared_ptr<IPSSClient> c);
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<IPSSClient> c);

  friend class IPSSChannel;
};
