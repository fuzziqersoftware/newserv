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
  HTTPServer(std::shared_ptr<ServerState> state);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer() = default;

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

protected:
  class http_error : public std::runtime_error {
  public:
    http_error(int code, const std::string& what);
    int code;
  };

  std::shared_ptr<ServerState> state;
  std::shared_ptr<struct evhttp> http;

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

  JSON generate_client_config_json(const Client::Config& config) const;
  JSON generate_license_json(std::shared_ptr<const License> l) const;
  JSON generate_game_client_json(std::shared_ptr<const Client> c) const;
  JSON generate_proxy_client_json(std::shared_ptr<const ProxyServer::LinkedSession> ses) const;
  JSON generate_lobby_json(std::shared_ptr<const Lobby> l) const;
  JSON generate_game_server_clients_json() const;
  JSON generate_proxy_server_clients_json() const;
  JSON generate_lobbies_json() const;
  JSON generate_summary_json() const;
  JSON generate_all_json() const;
};
