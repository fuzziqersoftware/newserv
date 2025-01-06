#pragma once

#include <memory>
#include <string>

#include <event2/event.h>

#include "ProxyServer.hh"
#include "ServerState.hh"

class ServerShell : public std::enable_shared_from_this<ServerShell> {
public:
  class exit_shell : public std::runtime_error {
  public:
    exit_shell();
    ~exit_shell() = default;
  };

  explicit ServerShell(std::shared_ptr<ServerState> state);
  ServerShell(const ServerShell&) = delete;
  ServerShell(ServerShell&&) = delete;
  ServerShell& operator=(const ServerShell&) = delete;
  ServerShell& operator=(ServerShell&&) = delete;
  ~ServerShell();

  std::shared_ptr<ProxyServer::LinkedSession> get_proxy_session(const std::string& name);

  void execute_command(const std::string& command);

protected:
  std::shared_ptr<ServerState> state;
  std::thread th;

  void thread_fn();
};
