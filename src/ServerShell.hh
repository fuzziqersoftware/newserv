#pragma once

#include <memory>
#include <string>

#include <event2/event.h>

#include "ProxyServer.hh"
#include "ServerState.hh"

class ServerShell : public std::enable_shared_from_this<ServerShell> {
public:
  explicit ServerShell(std::shared_ptr<ServerState> state);
  ServerShell(const ServerShell&) = delete;
  ServerShell(ServerShell&&) = delete;
  ServerShell& operator=(const ServerShell&) = delete;
  ServerShell& operator=(ServerShell&&) = delete;
  ~ServerShell();

  std::shared_ptr<ProxyServer::LinkedSession> get_proxy_session(const std::string& name);

protected:
  std::shared_ptr<ServerState> state;
  std::thread th;

  void thread_fn();
};
