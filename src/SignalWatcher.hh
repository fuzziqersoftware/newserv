#pragma once

#include <asio.hpp>
#include <memory>
#include <string>
#include <thread>

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
  asio::signal_set signals;

  asio::awaitable<void> signal_handler_task();
};
