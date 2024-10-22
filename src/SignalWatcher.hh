#pragma once

#include <memory>
#include <string>
#include <thread>

#include <event2/event.h>

#include "ServerState.hh"

class SignalWatcher : public std::enable_shared_from_this<SignalWatcher> {
public:
  explicit SignalWatcher(std::shared_ptr<ServerState> state);
  SignalWatcher(const SignalWatcher&) = delete;
  SignalWatcher(SignalWatcher&&) = delete;
  SignalWatcher& operator=(const SignalWatcher&) = delete;
  SignalWatcher& operator=(SignalWatcher&&) = delete;
  ~SignalWatcher() = default;

protected:
  phosg::PrefixedLogger log;
  std::shared_ptr<ServerState> state;
  std::unique_ptr<struct event, void (*)(struct event*)> sigusr1_event;
  std::unique_ptr<struct event, void (*)(struct event*)> sigusr2_event;

  static void dispatch_on_signal(evutil_socket_t, short, void* ctx);
  void on_signal(int signum);
};
