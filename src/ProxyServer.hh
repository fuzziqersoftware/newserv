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

  void listen(uint16_t port, GameVersion version);

  void connect_client(struct bufferevent* bev, uint16_t server_port);

  struct LinkedSession {
    ProxyServer* server;

    std::shared_ptr<const License> license;

    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> client_bev;
    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> server_bev;
    uint16_t local_port;
    struct sockaddr_storage next_destination;
    GameVersion version;
    uint32_t sub_version;
    std::string character_name;

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

    LinkedSession(
        ProxyServer* server,
        uint16_t local_port,
        GameVersion version,
        std::shared_ptr<const License> license,
        uint32_t dest_addr,
        uint16_t dest_port);

    void resume(
        std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)>&& client_bev,
        std::shared_ptr<PSOEncryption> client_input_crypt,
        std::shared_ptr<PSOEncryption> client_output_crypt,
        uint32_t sub_version,
        const std::string& character_name);

    static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_client_error(struct bufferevent* bev, short events,
        void* ctx);
    static void dispatch_on_server_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_server_error(struct bufferevent* bev, short events,
        void* ctx);
    void on_client_input();
    void on_server_input();
    void on_stream_error(short events, bool is_server_stream);

    void send_to_end(const std::string& data, bool to_server);

    void disconnect();

    bool is_open() const;
  };

  std::shared_ptr<LinkedSession> get_session();
  void delete_session(uint32_t serial_number);

private:
  struct ListeningSocket {
    ProxyServer* server;

    int fd;
    uint16_t port;
    std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> listener;
    GameVersion version;

    static void dispatch_on_listen_accept(struct evconnlistener* listener,
        evutil_socket_t fd, struct sockaddr *address, int socklen, void* ctx);
    static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
    void on_listen_accept(struct sockaddr *address, int socklen);
    void on_listen_error();
  };

  struct UnlinkedSession {
    ProxyServer* server;

    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> bev;
    uint16_t local_port;
    GameVersion version;

    std::shared_ptr<PSOEncryption> crypt_out;
    std::shared_ptr<PSOEncryption> crypt_in;

    UnlinkedSession(ProxyServer* server, struct bufferevent* bev, uint16_t port, GameVersion version);

    void receive_and_process_commands();

    static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_client_error(struct bufferevent* bev, short events,
        void* ctx);
    void on_client_input();
    void on_client_error(short events);
  };

  std::shared_ptr<struct event_base> base;
  std::shared_ptr<ServerState> state;
  std::map<int, std::shared_ptr<ListeningSocket>> listeners;
  std::unordered_map<struct bufferevent*, std::shared_ptr<UnlinkedSession>> bev_to_unlinked_session;
  std::unordered_map<uint32_t, std::shared_ptr<LinkedSession>> serial_number_to_session;

  void on_client_connect(struct bufferevent* bev, uint16_t port, GameVersion version);
};
