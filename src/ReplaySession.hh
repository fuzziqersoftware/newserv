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
  ReplaySession(std::shared_ptr<ServerState> state, FILE* input_log);
  ReplaySession(const ReplaySession&) = delete;
  ReplaySession(ReplaySession&&) = delete;
  ReplaySession& operator=(const ReplaySession&) = delete;
  ReplaySession& operator=(ReplaySession&&) = delete;
  ~ReplaySession() = default;

  asio::awaitable<void> run();

  inline std::string failure_str() const {
    return this->failure;
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
    size_t event_number = 0;
    size_t next_event_number = 0;
    uint64_t client_id = 0;
    std::string data; // Only used for SEND and RECEIVE
    std::string mask; // Only used for RECEIVE
    bool allow_size_disparity = false;
    bool complete = false;
    size_t line_num = 0;

    Event(Type type, size_t event_number, uint64_t client_id, size_t line_num);

    std::string str() const;
  };

  struct Client {
    uint64_t id = 0;
    uint16_t port = 0;
    Version version = Version::UNKNOWN;
    std::shared_ptr<PeerChannel> channel;
    std::deque<size_t> pending_receive_event_numbers;
    size_t disconnect_event_number = 0;

    Client(std::shared_ptr<asio::io_context> io_context, uint64_t id, uint16_t port, Version version);

    std::string str() const;
  };

  std::shared_ptr<ServerState> state;
  bool use_psov2_rand_crypt = false;
  bool use_legacy_item_random_behavior = false;

  std::unordered_map<uint64_t, std::shared_ptr<Client>> clients;

  std::map<size_t, Event> events;

  size_t commands_sent = 0;
  size_t bytes_sent = 0;
  size_t commands_received = 0;
  size_t bytes_received = 0;

  asio::steady_timer idle_timeout_timer;
  std::string failure;

  ReplaySession::Event& create_event(Event::Type type, std::shared_ptr<Client> c, size_t line_num);

  void apply_default_mask(Event& ev) const;

  void reschedule_idle_timeout();
};
