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
#include "Shell.hh"

class CatSession : public Shell {
public:
  CatSession(
      std::shared_ptr<struct event_base> base,
      const struct sockaddr_storage& remote,
      GameVersion version,
      std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file);
  virtual ~CatSession() = default;

protected:
  PrefixedLogger log;
  Channel channel;
  std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file;

  virtual void print_prompt();
  virtual void execute_command(const std::string& command);

  static void dispatch_on_channel_input(
      Channel& ch, uint16_t command, uint32_t flag, std::string& msg);
  static void dispatch_on_channel_error(Channel& ch, short events);
  void on_channel_input(uint16_t command, uint32_t flag, std::string& msg);
  void on_channel_error(short events);
};
