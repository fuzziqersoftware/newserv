#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

    std::shared_ptr<Client> get_client() const;
    std::shared_ptr<Client> get_proxy_client() const;
  };

  const char* name;
  const char* help_text;
  asio::awaitable<std::deque<std::string>> (*run)(Args&);

  static std::vector<const ShellCommand*> commands_by_order;
  static std::unordered_map<std::string, const ShellCommand*> commands_by_name;

  ShellCommand(const char* name, const char* help_text, asio::awaitable<std::deque<std::string>> (*run)(Args&));

  static asio::awaitable<std::deque<std::string>> dispatch_str(std::shared_ptr<ServerState> s, const std::string& command);
  static asio::awaitable<std::deque<std::string>> dispatch(Args& args);
};
