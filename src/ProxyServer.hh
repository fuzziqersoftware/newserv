#pragma once

#include <event2/event.h>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
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
  ProxyServer(
      std::shared_ptr<struct event_base> base,
      const struct sockaddr_storage& initial_destination,
      GameVersion version);
  virtual ~ProxyServer() = default;

  void listen(int port);

  void connect_client(struct bufferevent* bev, uint32_t server_ipv4_addr,
      uint16_t server_port);

  void send_to_client(const std::string& data);
  void send_to_server(const std::string& data);

  void set_save_quests(bool save_quests);

private:
  std::shared_ptr<struct event_base> base;
  std::map<int, std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)>> listeners;
  std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> client_bev;
  std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> server_bev;
  struct sockaddr_storage next_destination;
  struct sockaddr_storage default_destination;
  int listen_port;
  GameVersion version;

  size_t header_size;
  PSOCommandHeader client_input_header;
  PSOCommandHeader server_input_header;
  std::shared_ptr<PSOEncryption> client_input_crypt;
  std::shared_ptr<PSOEncryption> client_output_crypt;
  std::shared_ptr<PSOEncryption> server_input_crypt;
  std::shared_ptr<PSOEncryption> server_output_crypt;

  struct SavingQuestFile {
    std::string basename;
    std::string output_filename;
    uint32_t remaining_bytes;
    std::unique_ptr<FILE, std::function<void(FILE*)>> f;

    SavingQuestFile(
        const std::string& basename,
        const std::string& output_filename,
        uint32_t remaining_bytes);
  };
  bool save_quests;
  std::unordered_map<std::string, SavingQuestFile> saving_quest_files;

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

  void on_client_connect(struct bufferevent* bev,
      uint32_t server_ipv4_addr = 0, uint16_t server_port = 0);

  size_t get_size_field(const PSOCommandHeader* header);
  size_t get_command_field(const PSOCommandHeader* header);

  void receive_and_process_commands(bool from_server);
};
