#pragma once

#include <event2/event.h>

#include <unordered_set>
#include <vector>
#include <string>
#include <memory>

#include "Client.hh"
#include "ServerState.hh"



class Server {
public:
  Server() = delete;
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server(std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~Server() = default;

  void listen(const std::string& addr_str, const std::string& socket_path, GameVersion version, ServerBehavior initial_state);
  void listen(const std::string& addr_str, const std::string& addr, int port, GameVersion version, ServerBehavior initial_state);
  void listen(const std::string& addr_str, int port, GameVersion version, ServerBehavior initial_state);
  void add_socket(const std::string& addr_str, int fd, GameVersion version, ServerBehavior initial_state);

  void connect_client(struct bufferevent* bev, uint32_t address,
      uint16_t client_port, uint16_t server_port,
      GameVersion version, ServerBehavior initial_state);

  std::shared_ptr<Client> get_client() const;
  std::vector<std::shared_ptr<Client>> get_clients_by_identifier(
      const std::string& ident) const;
  std::shared_ptr<struct event_base> get_base() const;

private:
  std::shared_ptr<struct event_base> base;
  std::shared_ptr<struct event> destroy_clients_ev;

  struct ListeningSocket {
    std::string addr_str;
    int fd;
    GameVersion version;
    ServerBehavior behavior;
    std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> listener;

    ListeningSocket(
        Server* s,
        const std::string& name,
        int fd,
        GameVersion version,
        ServerBehavior behavior);
  };
  std::unordered_map<int, ListeningSocket> listening_sockets;
  std::unordered_map<Channel*, std::shared_ptr<Client>> channel_to_client;
  std::unordered_set<std::shared_ptr<Client>> clients_to_destroy;

  std::shared_ptr<ServerState> state;

  void enqueue_destroy_clients();
  static void dispatch_destroy_clients(evutil_socket_t, short, void* ctx);
  void destroy_clients();

  static void dispatch_on_listen_accept(struct evconnlistener* listener,
      evutil_socket_t fd, struct sockaddr *address, int socklen, void* ctx);
  static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);

  void disconnect_client(std::shared_ptr<Client> c);

  void on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd,
      struct sockaddr *address, int socklen);
  void on_listen_error(struct evconnlistener* listener);

  static void on_client_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data);
  static void on_client_error(Channel& ch, short events);
};
