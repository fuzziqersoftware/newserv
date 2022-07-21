#pragma once

#include <memory>
#include <string>

#include <event2/event.h>

#include "Shell.hh"
#include "ProxyServer.hh"

#define SHELL_PROMPT "newserv> "



class ServerShell : public Shell {
public:
  ServerShell(
      std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~ServerShell() = default;
  ServerShell(const ServerShell&) = delete;
  ServerShell(ServerShell&&) = delete;
  ServerShell& operator=(const ServerShell&) = delete;
  ServerShell& operator=(ServerShell&&) = delete;

protected:
  std::shared_ptr<ServerState> state;

  std::shared_ptr<ProxyServer::LinkedSession> get_proxy_session();

  virtual void print_prompt();
  virtual void execute_command(const std::string& command);
};
