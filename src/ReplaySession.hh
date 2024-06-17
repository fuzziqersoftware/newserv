#pragma once

#include <event2/event.h>
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
  ReplaySession(
      std::shared_ptr<struct event_base> base,
      FILE* input_log,
      std::shared_ptr<ServerState> state,
      bool require_basic_credentials);
  ReplaySession(const ReplaySession&) = delete;
  ReplaySession(ReplaySession&&) = delete;
  ReplaySession& operator=(const ReplaySession&) = delete;
  ReplaySession& operator=(ReplaySession&&) = delete;
  ~ReplaySession() = default;

  void start();

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
    Channel channel;
    std::deque<std::shared_ptr<Event>> receive_events;
    std::shared_ptr<Event> disconnect_event;

    Client(ReplaySession* session, uint64_t id, uint16_t port, Version version);

    std::string str() const;
  };

  std::shared_ptr<ServerState> state;
  bool require_basic_credentials;

  std::unordered_map<uint64_t, std::shared_ptr<Client>> clients;
  std::unordered_map<Channel*, std::shared_ptr<Client>> channel_to_client;

  std::shared_ptr<Event> first_event;
  std::shared_ptr<Event> last_event;

  std::shared_ptr<struct event_base> base;
  std::shared_ptr<struct event> timeout_ev;

  size_t commands_sent;
  size_t bytes_sent;
  size_t commands_received;
  size_t bytes_received;

  std::shared_ptr<ReplaySession::Event> create_event(
      Event::Type type, std::shared_ptr<Client> c, size_t line_num);
  void update_timeout_event();

  void apply_default_mask(std::shared_ptr<Event> ev);
  void check_for_password(std::shared_ptr<const Event> ev) const;

  static void dispatch_on_timeout(evutil_socket_t fd, short events, void* ctx);
  static void dispatch_on_command_received(
      Channel& ch, uint16_t command, uint32_t flag, std::string& data);
  static void dispatch_on_error(Channel& ch, short events);
  void on_command_received(
      std::shared_ptr<Client> c, uint16_t command, uint32_t flag, std::string& data);
  void on_error(std::shared_ptr<Client> c, short events);

  void execute_pending_events();
};
