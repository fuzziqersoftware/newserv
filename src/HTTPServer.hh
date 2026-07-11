#pragma once

#include <stdlib.h>

#include <memory>
#include <string>
#include <variant>

#include "AsyncHTTPServer.hh"
#include "ServerState.hh"

enum class HTTPEventType {
  PLAYER_LOGIN = 0,
  PLAYER_LOGOUT,
  CREATE_LOBBY,
  DESTROY_LOBBY,
  PLAYER_JOIN_LOBBY,
  PLAYER_LEAVE_LOBBY,
  SET_LOBBY_QUEST,
  RARE_DROP,
  // TODO: Add more event types as needed
  NUM_EVENT_TYPES,
};

HTTPEventType http_event_type_for_name(const std::string& name);
const char* name_for_http_event_type(HTTPEventType type);

class NewservHTTPClient : public HTTPClient {
public:
  using HTTPClient::HTTPClient;
  virtual ~NewservHTTPClient() = default;

  std::unordered_set<HTTPEventType> subscribed_event_types;
};

class HTTPServer : public AsyncHTTPServer<NewservHTTPClient> {
public:
  static constexpr size_t NUM_EVENT_TYPES = static_cast<size_t>(HTTPEventType::NUM_EVENT_TYPES);
  static const std::vector<std::string> ALL_EVENT_NAMES;

  explicit HTTPServer(std::shared_ptr<ServerState> state);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer() = default;

  asio::awaitable<void> send_event_notification(HTTPEventType type, std::shared_ptr<const phosg::JSON> message);

  inline bool event_enabled(HTTPEventType type) const {
    return !this->subscribers_for_event_type(type).empty();
  }

protected:
  struct RawResponse {
    std::string content_type;
    std::string filename;
    std::string data;
  };
  struct SharedRawResponse {
    std::string content_type;
    std::string filename;
    std::shared_ptr<const std::string> data;
  };
  std::shared_ptr<ServerState> state;
  std::array<std::unordered_set<std::shared_ptr<NewservHTTPClient>>, NUM_EVENT_TYPES> event_subscribers;
  HTTPRouter<std::variant<RawResponse, SharedRawResponse, std::shared_ptr<const phosg::JSON>>, NewservHTTPClient> router;

  inline std::unordered_set<std::shared_ptr<NewservHTTPClient>>& subscribers_for_event_type(HTTPEventType type) {
    return this->event_subscribers.at(static_cast<size_t>(type));
  }
  inline const std::unordered_set<std::shared_ptr<NewservHTTPClient>>& subscribers_for_event_type(
      HTTPEventType type) const {
    return this->event_subscribers.at(static_cast<size_t>(type));
  }

  virtual asio::awaitable<std::unique_ptr<HTTPResponse>> handle_request(
      std::shared_ptr<NewservHTTPClient> c, HTTPRequest&& req);
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<NewservHTTPClient> c);
};

template <typename ServerStateT, typename FnT>
  requires(std::is_invocable_r_v<std::shared_ptr<phosg::JSON>, FnT>)
void send_http_event_notif(ServerStateT&& s, HTTPEventType type, FnT&& fn) {
  if (s->http_server && s->http_server->event_enabled(type)) {
    auto json = fn();
    uint64_t timestamp = phosg::now();
    json->emplace("EventType", name_for_http_event_type(type));
    json->emplace("ServerTimestamp", phosg::format_time(timestamp));
    json->emplace("ServerTimestampUsecs", timestamp);
    asio::co_spawn(*s->io_context, s->http_server->send_event_notification(type, std::move(json)), asio::detached);
  }
}
