#pragma once

#include "WindowsPlatform.hh"

#include <stdlib.h>

#include <asio.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Time.hh>
#include <string>

#include "AsyncUtils.hh"
#include "Server.hh"

struct HTTPRequest {
  enum class Method {
    GET = 0,
    POST,
    DELETE,
    HEAD,
    PATCH,
    PUT,
    UPDATE,
    OPTIONS,
    CONNECT,
    TRACE,
  };
  std::string http_version;
  Method method;
  std::string path;
  std::string fragment;
  std::unordered_multimap<std::string, std::string> headers; // Header names converted to all lowercase
  std::unordered_multimap<std::string, std::string> query_params;
  std::string data;

  // Header name should be entirely lowercase for this function. Returns nullptr if the header doesn't exist; throws
  // http_error(400) if multiple instances of it exist.
  const std::string* get_header(const std::string& name) const;

  const std::string* get_query_param(const std::string& name) const;
};

struct HTTPResponse {
  std::string http_version;
  int response_code = 200;
  // Content-Length should NOT be specified in headers; it is automatically added in async_write() if data isn't blank.
  std::unordered_multimap<std::string, std::string> headers;
  std::string data;
};

struct WebSocketMessage {
  uint8_t header[2] = {0, 0};
  uint8_t opcode = 0x01;
  uint8_t mask_key[4] = {0, 0, 0, 0};
  std::string data;
};

class HTTPError : public std::runtime_error {
public:
  HTTPError(int code, const std::string& what);
  int code;
};

struct HTTPClient {
  AsyncSocketReader r;
  uint64_t last_communication_time = 0;
  bool is_websocket = false;

  HTTPClient(asio::ip::tcp::socket&& sock);

  asio::awaitable<HTTPRequest> recv_http_request(size_t max_line_size, size_t max_body_size);
  asio::awaitable<void> send_http_response(const HTTPResponse& resp);

  asio::awaitable<WebSocketMessage> recv_websocket_message(size_t max_data_size);
  asio::awaitable<void> send_websocket_message(const void* data, size_t size, uint8_t opcode = 0x01);
  asio::awaitable<void> send_websocket_message(const std::string& data, uint8_t opcode = 0x01);
};

template <typename RetT>
class HTTPRouter {
public:
  struct Args {
    std::shared_ptr<HTTPClient> client;
    const HTTPRequest& req;
    std::unordered_map<std::string, std::string> params;
    phosg::JSON post_data;

    template <typename T>
      requires(std::is_integral_v<T>)
    T get_param(const char* name, bool hex = false) const {
      const auto& value_str = this->params.at(name);
      size_t conversion_end;
      int64_t v = std::stoull(value_str, &conversion_end, hex ? 16 : 0);
      if (conversion_end != value_str.size()) {
        throw HTTPError(400, "Invalid integer value");
      }

      uint64_t uv = static_cast<uint64_t>(v);
      if constexpr (std::is_unsigned_v<T>) {
        if (uv & (~phosg::mask_for_type<T>)) {
          throw HTTPError(400, "Unsigned value out of range");
        }
        return uv;
      } else {
        if (((uv & (~(phosg::mask_for_type<T> >> 1))) != 0) && ((uv & (~(phosg::mask_for_type<T> >> 1))) != (~(phosg::mask_for_type<T> >> 1)))) {
          throw HTTPError(400, "Signed value out of range");
        }
        return v;
      }
    }
  };

  using Handler = std::function<asio::awaitable<RetT>(Args&&)>;

  static std::vector<std::string> split_and_normalize_path(const std::string& path) {
    auto path_tokens = phosg::split(path, '/');
    while (!path_tokens.empty() && path_tokens.back().empty()) {
      path_tokens.pop_back();
    }
    return path_tokens;
  }

  void add(HTTPRequest::Method method, const std::string& path_pattern, Handler handler) {
    this->routes.emplace_back(Route{
        .method = method, .path_tokens = this->split_and_normalize_path(path_pattern), .handler = handler});
  }

  asio::awaitable<RetT> call_handler(std::shared_ptr<HTTPClient> c, const HTTPRequest& req) {
    Args args = {.client = c, .req = req, .params = {}, .post_data = phosg::JSON()};

    auto tokens = this->split_and_normalize_path(req.path);
    for (const auto& route : this->routes) {
      if (route.path_tokens.size() != tokens.size()) {
        continue;
      }

      bool matched = true;
      args.params.clear();
      for (size_t z = 0; z < tokens.size(); z++) {
        if (route.path_tokens[z].starts_with(':')) {
          args.params.emplace(route.path_tokens[z].substr(1), tokens[z]);
        } else if (route.path_tokens[z] != tokens[z]) {
          matched = false;
          break;
        }
      }

      if (matched) {
        if (req.method != route.method) {
          throw HTTPError(405, "Incorrect HTTP method");
        }
        if (req.method == HTTPRequest::Method::POST) {
          auto* content_type = req.get_header("content-type");
          if (!content_type || (*content_type != "application/json")) {
            throw HTTPError(400, "POST requests must use the application/json content type");
          }
          try {
            args.post_data = phosg::JSON::parse(req.data);
          } catch (const std::exception& e) {
            throw HTTPError(400, std::format("Invalid JSON: {}", e.what()));
          }
        }
        co_return co_await route.handler(std::move(args));
      }
    }

    throw HTTPError(404, "Request path did not match any route");
  }

private:
  struct Route {
    HTTPRequest::Method method;
    std::vector<std::string> path_tokens;
    Handler handler;
  };
  std::vector<Route> routes;
};

struct HTTPServerLimits {
  size_t max_http_request_line_size = 0x1000; // 4KB
  size_t max_http_data_size = 0x200000; // 2MB
  size_t max_http_keepalive_idle_usecs = 300 * 1000 * 1000; // 5 minutes (0 = no limit)
  size_t max_websocket_message_size = 0x200000; // 2MB
  size_t max_websocket_idle_usecs = 0; // No limit by default
};

extern const HTTPServerLimits DEFAULT_HTTP_LIMITS;

template <typename ClientT = HTTPClient>
class AsyncHTTPServer : public Server<ClientT, ServerSocket> {
public:
  explicit AsyncHTTPServer(
      std::shared_ptr<asio::io_context> io_context,
      const std::string& log_prefix = "[AsyncHTTPServer] ",
      const HTTPServerLimits& limits = DEFAULT_HTTP_LIMITS)
      : Server<ClientT, ServerSocket>(io_context, log_prefix), limits(limits) {}
  AsyncHTTPServer(const AsyncHTTPServer&) = delete;
  AsyncHTTPServer(AsyncHTTPServer&&) = delete;
  AsyncHTTPServer& operator=(const AsyncHTTPServer&) = delete;
  AsyncHTTPServer& operator=(AsyncHTTPServer&&) = delete;
  virtual ~AsyncHTTPServer() = default;

  void listen(const std::string& addr, int port) {
    if (port == 0) {
      throw std::runtime_error("Listening port cannot be zero");
    }
    asio::ip::address asio_addr = addr.empty() ? asio::ip::address_v4::any() : asio::ip::make_address(addr);
    auto sock = std::make_shared<ServerSocket>();
    sock->name = std::format("http:{}:{}", addr, port);
    sock->endpoint = asio::ip::tcp::endpoint(asio_addr, port);
    this->add_socket(std::move(sock));
  }

protected:
  HTTPServerLimits limits;

  void require_GET(const HTTPRequest& req) {
    if (req.method != HTTPRequest::Method::GET) {
      throw HTTPError(405, "GET method required for this endpoint");
    }
  }

  phosg::JSON require_JSON_POST(const HTTPRequest& req) {
    if (req.method != HTTPRequest::Method::POST) {
      throw HTTPError(405, "POST method required for this endpoint");
    }

    auto* content_type = req.get_header("content-type");
    if (!content_type || (*content_type != "application/json")) {
      throw HTTPError(400, "POST requests must use the application/json content type");
    }

    try {
      return phosg::JSON::parse(req.data);
    } catch (const std::exception& e) {
      throw HTTPError(400, std::format("Invalid JSON: {}", e.what()));
    }
  }

  // Attempts to switch the client to WebSockets. Returns true if this is done successfully (and the caller should then
  // receive/send WebSocket messages), or false if this failed (and the caller should send an HTTP response).
  asio::awaitable<bool> enable_websockets(std::shared_ptr<ClientT> c, const HTTPRequest& req) {
    if (req.method != HTTPRequest::Method::GET) {
      co_return false;
    }

    auto connection_header = req.get_header("connection");
    if (!connection_header || phosg::tolower(*connection_header) != "upgrade") {
      co_return false;
    }
    auto upgrade_header = req.get_header("upgrade");
    if (!upgrade_header || phosg::tolower(*upgrade_header) != "websocket") {
      co_return false;
    }
    auto sec_websocket_key_header = req.get_header("sec-websocket-key");
    if (!sec_websocket_key_header) {
      co_return false;
    }

    std::string sec_websocket_accept_data = *sec_websocket_key_header + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string sec_websocket_accept = phosg::base64_encode(phosg::SHA1(sec_websocket_accept_data).bin());

    HTTPResponse resp;
    resp.http_version = req.http_version;
    resp.response_code = 101;
    resp.headers.emplace("Upgrade", "websocket");
    resp.headers.emplace("Connection", "upgrade");
    resp.headers.emplace("Sec-WebSocket-Accept", std::move(sec_websocket_accept));
    co_await c->send_http_response(resp);

    c->is_websocket = true;
    co_return true;
  }

  [[nodiscard]] virtual std::shared_ptr<ClientT> create_client(
      std::shared_ptr<ServerSocket>, asio::ip::tcp::socket&& client_sock) {
    return std::make_shared<HTTPClient>(std::move(client_sock));
  }

  // handle_request must do one of the following three things:
  // 1. Return an HTTP response.
  // 2. Call enable_websockets, and if it returns true, return nullptr. After this point, handle_request will not be
  //    called again for this client; handle_websocket_message will be called instead when any WebSocket messages are
  //    received. If enable_websockets returns false, handle_request must still return an HTTP response.
  // 3. Throw an exception. In this case, the client receives an HTTP 500 response.
  virtual asio::awaitable<std::unique_ptr<HTTPResponse>> handle_request(std::shared_ptr<ClientT> c, HTTPRequest&& req) = 0;
  virtual asio::awaitable<void> handle_websocket_message(std::shared_ptr<ClientT>, WebSocketMessage&&) {
    co_return;
  }

  virtual asio::awaitable<void> handle_client(std::shared_ptr<ClientT> c) {
    asio::steady_timer idle_timer(*this->io_context);
    while (c->r.get_socket().is_open()) {
      if (c->is_websocket) {
        WebSocketMessage msg = co_await c->recv_websocket_message(this->limits.max_websocket_message_size);
        idle_timer.cancel();
        try {
          co_await this->handle_websocket_message(c, std::move(msg));
        } catch (const std::exception& e) {
          c->r.close();
        }

      } else {
        HTTPRequest req = co_await c->recv_http_request(
            this->limits.max_http_request_line_size, this->limits.max_http_data_size);
        idle_timer.cancel();
        std::unique_ptr<HTTPResponse> resp;
        try {
          resp = co_await this->handle_request(c, std::move(req));
        } catch (const std::exception& e) {
          resp = std::make_unique<HTTPResponse>();
          resp->http_version = req.http_version;
          resp->response_code = 500;
          resp->headers.emplace("Content-Type", "text/plain");
          resp->data = "Internal server error:\n";
          resp->data += e.what();
        }
        if (resp) {
          co_await c->send_http_response(*resp);
        }
        if (!c->is_websocket) {
          auto* conn_header = req.get_header("connection");
          if (!conn_header || (*conn_header != "keep-alive")) {
            c->r.close();
          }
        }
      }

      size_t idle_usecs_limit = c->is_websocket
          ? this->limits.max_websocket_idle_usecs
          : this->limits.max_http_keepalive_idle_usecs;
      if (idle_usecs_limit && c->r.get_socket().is_open()) {
        idle_timer.expires_after(std::chrono::microseconds(idle_usecs_limit));
        idle_timer.async_wait([c](std::error_code ec) {
          if (!ec) {
            c->r.close();
          }
        });
      }
    }
    idle_timer.cancel();
  }
};
