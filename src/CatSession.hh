#pragma once

#include <event2/event.h>

#include <functional>
#include <map>
#include <memory>
#include <phosg/Filesystem.hh>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

class CatSession {
public:
  CatSession(
      std::shared_ptr<struct event_base> base,
      const struct sockaddr_storage& remote,
      Version version,
      std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file);
  CatSession(const CatSession&) = delete;
  CatSession(CatSession&&) = delete;
  CatSession& operator=(const CatSession&) = delete;
  CatSession& operator=(CatSession&&) = delete;
  virtual ~CatSession() = default;

protected:
  phosg::PrefixedLogger log;
  std::shared_ptr<struct event_base> base;
  std::unique_ptr<struct event, void (*)(struct event*)> read_event;
  phosg::Poll poll;

  Channel channel;
  std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file;

  class exit_shell : public std::runtime_error {
  public:
    exit_shell();
    ~exit_shell() = default;
  };

  virtual void execute_command(const std::string& command);

  static void dispatch_read_stdin(evutil_socket_t fd, short events, void* ctx);
  static void dispatch_on_channel_input(Channel& ch, uint16_t command, uint32_t flag, std::string& msg);
  static void dispatch_on_channel_error(Channel& ch, short events);
  void on_channel_input(uint16_t command, uint32_t flag, std::string& msg);
  void on_channel_error(short events);
  void read_stdin();
};
