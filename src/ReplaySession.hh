#pragma once

#include <stdint.h>
#include <stdio.h>

#include <deque>
#include <memory>
#include <string>

#include "Channel.hh"
#include "ServerState.hh"
#include "Version.hh"

class ReplaySession {
public:
  ReplaySession(std::shared_ptr<ServerState> state, FILE* input_log, bool is_interactive);
  ReplaySession(const ReplaySession&) = delete;
  ReplaySession(ReplaySession&&) = delete;
  ReplaySession& operator=(const ReplaySession&) = delete;
  ReplaySession& operator=(ReplaySession&&) = delete;
  ~ReplaySession() = default;

  asio::awaitable<void> run();
  inline bool failed() const {
    return this->run_failed;
  }

private:
  struct Event {
    enum class Type {
      CONNECT = 0,
      DISCONNECT,
      SEND,
      RECEIVE,
    };
    Type type;
    uint64_t client_id;
    std::string data; // Only used for SEND and RECEIVE
    std::string mask; // Only used for RECEIVE
    bool allow_size_disparity;
    bool complete;
    size_t line_num;

    std::shared_ptr<Event> next_event;

    Event(Type type, uint64_t client_id, size_t line_num);

    std::string str() const;
  };

  struct Client {
    uint64_t id;
    uint16_t port;
    Version version;
    std::shared_ptr<PeerChannel> channel;
    std::deque<std::shared_ptr<Event>> receive_events;
    std::shared_ptr<Event> disconnect_event;

    Client(std::shared_ptr<asio::io_context> io_context, uint64_t id, uint16_t port, Version version);

    std::string str() const;
  };

  std::shared_ptr<ServerState> state;
  bool is_interactive;
  bool prev_psov2_crypt_enabled;

  std::unordered_map<uint64_t, std::shared_ptr<Client>> clients;

  std::shared_ptr<Event> first_event;
  std::shared_ptr<Event> last_event;

  size_t commands_sent;
  size_t bytes_sent;
  size_t commands_received;
  size_t bytes_received;

  asio::steady_timer idle_timeout_timer;
  bool run_failed;

  std::shared_ptr<ReplaySession::Event> create_event(Event::Type type, std::shared_ptr<Client> c, size_t line_num);

  void apply_default_mask(std::shared_ptr<Event> ev);

  void reschedule_idle_timeout();
};
