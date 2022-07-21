#pragma once

#include <event2/event.h>

#include <stdexcept>
#include <memory>
#include <string>
#include <phosg/Filesystem.hh>

#include "ServerState.hh"



class Shell {
public:
  Shell(std::shared_ptr<struct event_base> base);
  virtual ~Shell() = default;
  Shell(const Shell&) = delete;
  Shell(Shell&&) = delete;
  Shell& operator=(const Shell&) = delete;
  Shell& operator=(Shell&&) = delete;

  static const std::string PROMPT;

protected:
  std::shared_ptr<struct event_base> base;
  std::unique_ptr<struct event, void (*)(struct event*)> read_event;
  std::unique_ptr<struct event, void (*)(struct event*)> prompt_event;
  Poll poll;

  class exit_shell : public std::runtime_error {
  public:
    exit_shell();
    ~exit_shell() = default;
  };

  static void dispatch_print_prompt(evutil_socket_t fd, short events, void* ctx);
  static void dispatch_read_stdin(evutil_socket_t fd, short events, void* ctx);
  virtual void print_prompt();
  void read_stdin();
  virtual void execute_command(const std::string& command) = 0;
};
