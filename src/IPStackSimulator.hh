#include <netinet/in.h>
#include <stdint.h>

#include <deque>
#include <phosg/Filesystem.hh>
#include <phosg/Process.hh>
#include <string>

#include "IPFrameInfo.hh"
#include "ProxyServer.hh"
#include "Server.hh"
#include "ServerState.hh"

class IPStackSimulator {
public:
  IPStackSimulator(
      std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  ~IPStackSimulator();

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

  static uint32_t connect_address_for_remote_address(uint32_t remote_addr);

private:
  std::shared_ptr<struct event_base> base;
  std::shared_ptr<ServerState> state;

  using unique_listener = std::unique_ptr<struct evconnlistener, void (*)(struct evconnlistener*)>;
  using unique_bufferevent = std::unique_ptr<struct bufferevent, void (*)(struct bufferevent*)>;
  using unique_evbuffer = std::unique_ptr<struct evbuffer, void (*)(struct evbuffer*)>;
  using unique_event = std::unique_ptr<struct event, void (*)(struct event*)>;

  struct IPClient {
    IPStackSimulator* sim;

    unique_bufferevent bev;
    uint8_t mac_addr[6];
    uint32_t ipv4_addr;

    struct TCPConnection {
      std::weak_ptr<IPClient> client;

      // The PSO protocol begins with the server sending a command, but we
      // shouldn't send a PSH immediately after the SYN+ACK, so the connection
      // isn't handed to the Server object until after the 3-way handshake
      // (receive SYN, send SYN+ACK, receive ACK). This means server_bev is null
      // during the first part of the connection phase.
      unique_bufferevent server_bev;
      // TODO: Get rid of pending_data and just use server_bev's input buffer in
      // its place
      unique_evbuffer pending_data;
      unique_event resend_push_event;

      bool awaiting_first_ack;

      uint32_t server_addr;
      uint16_t server_port;
      uint16_t client_port;
      uint32_t next_client_seq;
      uint32_t acked_server_seq;
      size_t resend_push_usecs;
      size_t next_push_max_frame_size;
      size_t max_frame_size;
      size_t bytes_received;
      size_t bytes_sent;

      TCPConnection();
    };
    std::unordered_map<uint64_t, TCPConnection> tcp_connections;

    IPClient(struct bufferevent* bev);
  };

  std::unordered_set<unique_listener> listeners;
  std::unordered_map<struct bufferevent*, std::shared_ptr<IPClient>> bev_to_client;

  uint8_t host_mac_address_bytes[6];
  uint8_t broadcast_mac_address_bytes[6];

  FILE* pcap_text_log_file;

  static uint64_t tcp_conn_key_for_connection(
      const IPClient::TCPConnection& conn);
  static uint64_t tcp_conn_key_for_client_frame(
      const IPv4Header& ipv4, const TCPHeader& tcp);
  static uint64_t tcp_conn_key_for_client_frame(const FrameInfo& fi);

  static std::string str_for_ipv4_netloc(uint32_t addr, uint16_t port);
  static std::string str_for_tcp_connection(std::shared_ptr<const IPClient> c,
                                            const IPClient::TCPConnection& conn);

  static void dispatch_on_listen_accept(struct evconnlistener* listener,
                                        evutil_socket_t fd, struct sockaddr* address, int socklen, void* ctx);
  void on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd,
                        struct sockaddr* address, int socklen);
  static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
  void on_listen_error(struct evconnlistener* listener);

  static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
  void on_client_input(struct bufferevent* bev);
  static void dispatch_on_client_error(struct bufferevent* bev, short events,
                                       void* ctx);
  void on_client_error(struct bufferevent* bev, short events);

  void on_client_frame(std::shared_ptr<IPClient> c, const std::string& frame);
  void on_client_arp_frame(std::shared_ptr<IPClient> c, const FrameInfo& fi);
  void on_client_udp_frame(std::shared_ptr<IPClient> c, const FrameInfo& fi);
  void on_client_tcp_frame(std::shared_ptr<IPClient> c, const FrameInfo& fi);

  static void dispatch_on_resend_push(evutil_socket_t fd, short events,
                                      void* ctx);
  void on_resend_push(std::shared_ptr<IPClient> c, IPClient::TCPConnection& conn);

  static void dispatch_on_server_input(struct bufferevent* bev, void* ctx);
  void on_server_input(std::shared_ptr<IPClient> c, IPClient::TCPConnection& conn);
  static void dispatch_on_server_error(struct bufferevent* bev, short events,
                                       void* ctx);
  void on_server_error(std::shared_ptr<IPClient> c, IPClient::TCPConnection& conn, short events);

  void send_pending_push_frame(
      std::shared_ptr<IPClient> c, IPClient::TCPConnection& conn);
  void send_tcp_frame(
      std::shared_ptr<IPClient> c,
      IPClient::TCPConnection& conn,
      uint16_t flags = 0,
      struct evbuffer* src_buf = nullptr,
      size_t src_bytes = 0);

  void open_server_connection(
      std::shared_ptr<IPClient> c, IPClient::TCPConnection& conn);

  void log_frame(const std::string& data) const;
};
