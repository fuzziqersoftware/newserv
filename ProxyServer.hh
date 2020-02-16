#pragma once

#include <event2/event.h>

#include <unordered_set>
#include <vector>
#include <string>
#include <memory>

#include "PSOEncryption.hh"
#include "PSOProtocol.hh"



class ProxyServer {
public:
  ProxyServer() = delete;
  ProxyServer(const ProxyServer&) = delete;
  ProxyServer(ProxyServer&&) = delete;
  ProxyServer(std::shared_ptr<struct event_base> base,
      const struct sockaddr_storage& initial_destination, int listen_port);
  virtual ~ProxyServer() = default;

  void send_to_client(const std::string& data);
  void send_to_server(const std::string& data);

private:
  std::shared_ptr<struct event_base> base;
  std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> listener;
  std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> client_bev;
  std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> server_bev;
  struct sockaddr_storage next_destination;
  int listen_port;

  PSOCommandHeaderDCGC client_input_header;
  PSOCommandHeaderDCGC server_input_header;
  std::shared_ptr<PSOEncryption> client_input_crypt;
  std::shared_ptr<PSOEncryption> client_output_crypt;
  std::shared_ptr<PSOEncryption> server_input_crypt;
  std::shared_ptr<PSOEncryption> server_output_crypt;

  void send_to_end(const std::string& data, bool to_server);

  static void dispatch_on_listen_accept(struct evconnlistener* listener,
      evutil_socket_t fd, struct sockaddr *address, int socklen, void* ctx);
  static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
  static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
  static void dispatch_on_client_error(struct bufferevent* bev, short events,
      void* ctx);
  static void dispatch_on_server_input(struct bufferevent* bev, void* ctx);
  static void dispatch_on_server_error(struct bufferevent* bev, short events,
      void* ctx);

  void on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd,
      struct sockaddr *address, int socklen);
  void on_listen_error(struct evconnlistener* listener);
  void on_client_input(struct bufferevent* bev);
  void on_client_error(struct bufferevent* bev, short events);
  void on_server_input(struct bufferevent* bev);
  void on_server_error(struct bufferevent* bev, short events);

  void receive_and_process_commands(bool from_server);
};
