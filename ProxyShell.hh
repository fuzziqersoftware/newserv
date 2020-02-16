#pragma once

#include <memory>
#include <string>

#include <event2/event.h>

#include "Shell.hh"
#include "ProxyServer.hh"



class ProxyShell : public Shell {
public:
  ProxyShell(std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state,
      std::shared_ptr<ProxyServer> proxy_server);
  virtual ~ProxyShell() = default;
  ProxyShell(const ProxyShell&) = delete;
  ProxyShell(ProxyShell&&) = delete;
  ProxyShell& operator=(const ProxyShell&) = delete;
  ProxyShell& operator=(ProxyShell&&) = delete;

protected:
  std::shared_ptr<ProxyServer> proxy_server;

  virtual void execute_command(const std::string& command);
};
