#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ProxyServer.hh"
#include "ServerState.hh"

class exit_shell : public std::runtime_error {
public:
  exit_shell();
  ~exit_shell() = default;
};

struct ShellCommand {
  struct Args {
    std::shared_ptr<ServerState> s;
    std::string command;
    std::string args;
    std::string session_name;
  };

  const char* name;
  const char* help_text;
  bool run_on_event_thread;
  std::deque<std::string> (*run)(Args&);

  static std::vector<const ShellCommand*> commands_by_order;
  static std::unordered_map<std::string, const ShellCommand*> commands_by_name;

  ShellCommand(const char* name, const char* help_text, bool run_on_event_thread, std::deque<std::string> (*run)(Args&));

  static std::deque<std::string> dispatch_str(std::shared_ptr<ServerState> s, const std::string& command);
  static std::deque<std::string> dispatch(Args& args);
};
