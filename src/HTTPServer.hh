#pragma once

#include <stdlib.h>

#include <memory>
#include <string>
#include <variant>

#include "AsyncHTTPServer.hh"
#include "ServerState.hh"

class HTTPServer : public AsyncHTTPServer<> {
public:
  explicit HTTPServer(std::shared_ptr<ServerState> state);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer() = default;

  asio::awaitable<void> send_rare_drop_notification(std::shared_ptr<const phosg::JSON> message);

protected:
  struct RawResponse {
    std::string content_type;
    std::string data;
  };
  std::shared_ptr<ServerState> state;
  std::unordered_set<std::shared_ptr<HTTPClient>> rare_drop_subscribers;
  HTTPRouter<std::variant<RawResponse, std::shared_ptr<const phosg::JSON>>> router;

  void require_GET(const HTTPRequest& req);
  phosg::JSON require_POST(const HTTPRequest& req);

  virtual asio::awaitable<std::unique_ptr<HTTPResponse>> handle_request(std::shared_ptr<HTTPClient> c, HTTPRequest&& req);
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<HTTPClient> c);
};
