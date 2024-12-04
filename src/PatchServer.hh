#pragma once

#include <event2/event.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Account.hh"
#include "Channel.hh"
#include "IPV4RangeSet.hh"
#include "PatchFileIndex.hh"
#include "Version.hh"

class PatchServer : public std::enable_shared_from_this<PatchServer> {
public:
  struct Config {
    bool allow_unregistered_users;
    bool hide_data_from_logs;
    uint64_t idle_timeout_usecs;
    std::string message;
    std::shared_ptr<AccountIndex> account_index;
    std::shared_ptr<const PatchFileIndex> patch_file_index;
    std::shared_ptr<const IPV4RangeSet> banned_ipv4_ranges;
    std::shared_ptr<struct event_base> shared_base;
  };

  PatchServer() = delete;
  explicit PatchServer(std::shared_ptr<const Config> config);
  PatchServer(const PatchServer&) = delete;
  PatchServer(PatchServer&&) = delete;
  PatchServer& operator=(const PatchServer&) = delete;
  PatchServer& operator=(PatchServer&&) = delete;
  virtual ~PatchServer() = default;

  void schedule_stop();
  void wait_for_stop();

  void listen(const std::string& addr_str, const std::string& socket_path, Version version);
  void listen(const std::string& addr_str, const std::string& addr, int port, Version version);
  void listen(const std::string& addr_str, int port, Version version);
  void add_socket(const std::string& addr_str, int fd, Version version);

  void set_config(std::shared_ptr<const Config> config);

private:
  class Client : public std::enable_shared_from_this<Client> {
  public:
    std::weak_ptr<PatchServer> server;
    uint64_t id;
    phosg::PrefixedLogger log;

    Channel channel;
    std::vector<PatchFileChecksumRequest> patch_file_checksum_requests;
    uint64_t idle_timeout_usecs;

    std::unique_ptr<struct event, void (*)(struct event*)> idle_timeout_event;

    Client(
        std::shared_ptr<PatchServer> server,
        struct bufferevent* bev,
        Version version,
        uint64_t idle_timeout_usecs,
        bool hide_data_from_logs);
    ~Client() = default;

    void reschedule_timeout_event();

    inline Version version() const {
      return this->channel.version;
    }

    static void dispatch_idle_timeout(evutil_socket_t, short, void* ctx);
    void idle_timeout();
  };

  struct ListeningSocket {
    std::string addr_str;
    int fd;
    Version version;
    std::unique_ptr<struct evconnlistener, void (*)(struct evconnlistener*)> listener;

    ListeningSocket(PatchServer* s, const std::string& name, int fd, Version version);
  };

  std::shared_ptr<struct event_base> base;
  bool base_is_shared;
  std::shared_ptr<const Config> config;

  std::unordered_set<std::shared_ptr<Client>> clients_to_destroy;
  std::shared_ptr<struct event> destroy_clients_ev;

  std::unordered_map<int, ListeningSocket> listening_sockets;
  std::unordered_map<Channel*, std::shared_ptr<Client>> channel_to_client;

  std::thread th;

  void send_server_init(std::shared_ptr<Client> c) const;
  void send_message_box(std::shared_ptr<Client> c, const std::string& text) const;
  void send_enter_directory(std::shared_ptr<Client> c, const std::string& dir) const;
  void change_to_directory(
      std::shared_ptr<Client> c,
      std::vector<std::string>& client_path_directories,
      const std::vector<std::string>& file_path_directories) const;
  void on_02(std::shared_ptr<Client> c, std::string& data);
  void on_04(std::shared_ptr<Client> c, std::string& data);
  void on_0F(std::shared_ptr<Client> c, std::string& data);
  void on_10(std::shared_ptr<Client> c, std::string& data);

  void disconnect_client(std::shared_ptr<Client> c);

  void enqueue_destroy_clients();
  static void dispatch_destroy_clients(evutil_socket_t, short, void* ctx);

  static void dispatch_on_listen_accept(struct evconnlistener* listener,
      evutil_socket_t fd, struct sockaddr* address, int socklen, void* ctx);
  static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);

  void on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* address, int socklen);
  void on_listen_error(struct evconnlistener* listener);

  static void on_client_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data);
  static void on_client_error(Channel& ch, short events);

  void thread_fn();
};
