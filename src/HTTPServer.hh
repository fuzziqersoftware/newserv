#pragma once

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "ProxyServer.hh"
#include "ServerState.hh"

class HTTPServer {
public:
  // shared_base should be null unless the HTTP server should run on the main
  // thread (on Windows).
  HTTPServer(std::shared_ptr<ServerState> state, std::shared_ptr<struct event_base> shared_base);

  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer() = default;

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

  void schedule_stop();
  void wait_for_stop();

  void send_rare_drop_notification(std::shared_ptr<const phosg::JSON> message);

protected:
  class http_error : public std::runtime_error {
  public:
    http_error(int code, const std::string& what);
    int code;
  };

  struct WebsocketClient {
    struct evhttp_connection* conn;
    struct bufferevent* bev;

    uint8_t pending_opcode;
    std::string pending_data;

    uint64_t last_communication_time;

    void* context;

    WebsocketClient(struct evhttp_connection* conn);
    ~WebsocketClient();

    void reset_pending_frame();
  };

  std::shared_ptr<ServerState> state;
  std::shared_ptr<struct event_base> base;
  std::shared_ptr<struct evhttp> http;
  std::thread th; // Not used on Windows

  std::unordered_set<std::shared_ptr<WebsocketClient>> rare_drop_subscribers;

  std::unordered_map<struct bufferevent*, std::shared_ptr<WebsocketClient>> bev_to_websocket_client;

  static void require_GET(struct evhttp_request* req);
  static phosg::JSON require_POST(struct evhttp_request* req);

  std::shared_ptr<WebsocketClient> enable_websockets(struct evhttp_request* req);

  static void dispatch_on_websocket_read(struct bufferevent* bev, void* ctx);
  static void dispatch_on_websocket_error(struct bufferevent* bev, short events, void* ctx);

  void on_websocket_read(struct bufferevent* bev);
  void on_websocket_error(struct bufferevent* bev, short events);

  void disconnect_websocket_client(struct bufferevent* bev);
  void send_websocket_message(struct bufferevent* bev, const std::string& message, uint8_t opcode = 0x01);
  void send_websocket_message(std::shared_ptr<WebsocketClient> c, const std::string& message, uint8_t opcode = 0x01);

  virtual void handle_websocket_message(std::shared_ptr<WebsocketClient> c, uint8_t opcode, const std::string& message);
  virtual void handle_websocket_disconnect(std::shared_ptr<WebsocketClient> c);

  void thread_fn();

  static void dispatch_handle_request(struct evhttp_request* req, void* ctx);
  void handle_request(struct evhttp_request* req);

  static const std::unordered_map<int, const char*> explanation_for_response_code;
  static void send_response(struct evhttp_request* req, int code, const char* content_type, struct evbuffer* b);
  static void send_response(struct evhttp_request* req, int code, const char* content_type, const char* fmt, ...);

  static std::unordered_multimap<std::string, std::string> parse_url_params(const std::string& query);
  static std::unordered_map<std::string, std::string> parse_url_params_unique(const std::string& query);
  static const std::string& get_url_param(
      const std::unordered_multimap<std::string, std::string>& params,
      const std::string& key,
      const std::string* _default = nullptr);

  static phosg::JSON generate_server_version_st();
  static phosg::JSON generate_client_config_json_st(const Client::Config& config);
  static phosg::JSON generate_account_json_st(std::shared_ptr<const Account> a);
  static phosg::JSON generate_game_client_json_st(std::shared_ptr<const Client> c, std::shared_ptr<const ItemNameIndex> item_name_index);
  static phosg::JSON generate_proxy_client_json_st(std::shared_ptr<const ProxyServer::LinkedSession> ses);
  static phosg::JSON generate_lobby_json_st(std::shared_ptr<const Lobby> l, std::shared_ptr<const ItemNameIndex> item_name_index);
  phosg::JSON generate_accounts_json() const;
  phosg::JSON generate_game_server_clients_json() const;
  phosg::JSON generate_proxy_server_clients_json() const;
  phosg::JSON generate_server_info_json() const;
  phosg::JSON generate_lobbies_json() const;
  phosg::JSON generate_summary_json() const;
  phosg::JSON generate_all_json() const;

  phosg::JSON generate_ep3_cards_json(bool trial) const;
  phosg::JSON generate_common_tables_json() const;
  phosg::JSON generate_rare_tables_json() const;
  phosg::JSON generate_rare_table_json(const std::string& table_name) const;
  phosg::JSON generate_quest_list_json(std::shared_ptr<const QuestIndex> q);
};
