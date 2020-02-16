#pragma once

#include <memory>
#include <string>

#include <event2/event.h>

#include "Shell.hh"



class ServerShell : public Shell {
public:
  ServerShell(std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~ServerShell() = default;
  ServerShell(const ServerShell&) = delete;
  ServerShell(ServerShell&&) = delete;
  ServerShell& operator=(const ServerShell&) = delete;
  ServerShell& operator=(ServerShell&&) = delete;

protected:
  virtual void print_prompt();
  virtual void execute_command(const std::string& command);
};
