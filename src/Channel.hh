#pragma once

#include <netinet/in.h>

#include <memory>
#include <string>

#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "Version.hh"

struct Channel {
  std::unique_ptr<struct bufferevent, void (*)(struct bufferevent*)> bev;
  struct sockaddr_storage local_addr;
  struct sockaddr_storage remote_addr;
  uint64_t virtual_network_id; // 0 = normal TCP connection

  Version version;
  uint8_t language;
  std::shared_ptr<PSOEncryption> crypt_in;
  std::shared_ptr<PSOEncryption> crypt_out;

  std::string name;
  phosg::TerminalFormat terminal_send_color;
  phosg::TerminalFormat terminal_recv_color;

  struct Message {
    uint16_t command;
    uint32_t flag;
    std::string data;
  };

  typedef void (*on_command_received_t)(Channel&, uint16_t, uint32_t, std::string&);
  typedef void (*on_error_t)(Channel&, short);

  on_command_received_t on_command_received;
  on_error_t on_error;
  void* context_obj;

  // Creates an unconnected channel
  Channel(
      Version version,
      uint8_t language,
      on_command_received_t on_command_received,
      on_error_t on_error,
      void* context_obj,
      const std::string& name,
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);
  // Creates a connected channel
  Channel(
      struct bufferevent* bev,
      uint64_t virtual_network_id,
      Version version,
      uint8_t language,
      on_command_received_t on_command_received,
      on_error_t on_error,
      void* context_obj,
      const std::string& name = "",
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);
  Channel(const Channel& other) = delete;
  Channel(Channel&& other) = delete;
  Channel& operator=(const Channel& other) = delete;
  Channel& operator=(Channel&& other) = delete;

  void replace_with(
      Channel&& other,
      on_command_received_t on_command_received,
      on_error_t on_error,
      void* context_obj,
      const std::string& name = "");

  void set_bufferevent(struct bufferevent* bev, uint64_t virtual_network_id);

  inline bool connected() const {
    return this->bev.get() != nullptr;
  }
  void disconnect();

  // Receives a message. Throws std::out_of_range if no messages are available.
  Message recv();

  // Sends a message with an automatically-constructed header.
  void send(uint16_t cmd, uint32_t flag = 0, bool silent = false);
  void send(uint16_t cmd, uint32_t flag, const void* data, size_t size, bool silent = false);
  void send(uint16_t cmd, uint32_t flag, const std::vector<std::pair<const void*, size_t>> blocks, bool silent = false);
  void send(uint16_t cmd, uint32_t flag, const std::string& data, bool silent = false);
  template <typename CmdT>
    requires(!std::is_pointer_v<CmdT>)
  void send(uint16_t cmd, uint32_t flag, const CmdT& data, bool silent = false) {
    this->send(cmd, flag, &data, sizeof(data), silent);
  }

  // Sends a message with a pre-existing header (as the first few bytes in the
  // data)
  void send(const void* data, size_t size, bool silent = false);
  void send(const std::string& data, bool silent = false);

private:
  static void dispatch_on_input(struct bufferevent*, void* ctx);
  static void dispatch_on_error(struct bufferevent*, short events, void* ctx);
};
