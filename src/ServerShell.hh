#pragma once

#include <memory>
#include <string>

#include "ServerState.hh"

class ServerShell : public std::enable_shared_from_this<ServerShell> {
public:
  explicit ServerShell(std::shared_ptr<ServerState> state);
  ServerShell(const ServerShell&) = delete;
  ServerShell(ServerShell&&) = delete;
  ServerShell& operator=(const ServerShell&) = delete;
  ServerShell& operator=(ServerShell&&) = delete;
  ~ServerShell();

protected:
  std::shared_ptr<ServerState> state;
  std::thread th;

  void thread_fn();
};
