#pragma once

#include <asio.hpp>
#include <memory>
#include <string>

#include "AsyncUtils.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "Version.hh"

class Channel {
public:
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

    template <typename T>
    const T& check_size_t(size_t min_size, size_t max_size) const {
      return ::check_size_t<const T>(this->data.data(), this->data.size(), min_size, max_size);
    }
    template <typename T>
    T& check_size_t(size_t min_size, size_t max_size) {
      return ::check_size_t<T>(this->data.data(), this->data.size(), min_size, max_size);
    }

    template <typename T>
    const T& check_size_t(size_t max_size) const {
      return ::check_size_t<const T>(this->data.data(), this->data.size(), sizeof(T), max_size);
    }
    template <typename T>
    T& check_size_t(size_t max_size) {
      return ::check_size_t<T>(this->data.data(), this->data.size(), sizeof(T), max_size);
    }

    template <typename T>
    const T& check_size_t() const {
      return ::check_size_t<const T>(this->data.data(), this->data.size(), sizeof(T), sizeof(T));
    }
    template <typename T>
    T& check_size_t() {
      return ::check_size_t<T>(this->data.data(), this->data.size(), sizeof(T), sizeof(T));
    }
  };

  virtual ~Channel() = default;

  virtual std::string default_name() const = 0;

  // Returns whether the channel is connected or not.
  virtual bool connected() const = 0;

  // Disconnects the channel. Any pending data will still be sent before the
  // underlying transport (e.g. socket) is closed, but further send calls will
  // do nothing.
  virtual void disconnect() = 0;

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

  // Receives a message. Throws std::out_of_range if no messages are available.
  asio::awaitable<Message> recv();

protected:
  Channel(
      Version version,
      uint8_t language,
      const std::string& name,
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);
  Channel(const Channel& other) = delete;
  Channel(Channel&& other) = delete;
  Channel& operator=(const Channel& other) = delete;
  Channel& operator=(Channel&& other) = delete;

  // Sends raw data on the underlying transport. If the channel is already
  // disconnected, silently drops the data.
  virtual void send_raw(std::string&& data) = 0;
  // Receives raw data on the underlying transport. Raises when the channel is
  // disconnected.
  virtual asio::awaitable<void> recv_raw(void* data, size_t size) = 0;
};

// Standard channel type, used for most PSO clients. Represents an open TCP
// socket.
class SocketChannel : public Channel, public std::enable_shared_from_this<SocketChannel> {
public:
  std::unique_ptr<asio::ip::tcp::socket> sock;
  asio::ip::tcp::endpoint local_addr;
  asio::ip::tcp::endpoint remote_addr;

  // SocketChannel has a static constructor because it has an internal task,
  // which is necessary to support flushing before disconnection (for example)
  // and also to make send_raw not a coroutine, which keeps the rest of the
  // code cleaner. The task needs to hold a shared_ptr to the SocketChannel
  // whilc it's open
  static std::shared_ptr<SocketChannel> create(std::shared_ptr<asio::io_context> io_context,
      std::unique_ptr<asio::ip::tcp::socket>&& sock,
      Version version,
      uint8_t language,
      const std::string& name = "",
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);

  virtual std::string default_name() const;

  virtual bool connected() const;
  virtual void disconnect();

  virtual void send_raw(std::string&& data);
  virtual asio::awaitable<void> recv_raw(void* data, size_t size);

private:
  SocketChannel(
      std::shared_ptr<asio::io_context> io_context,
      std::unique_ptr<asio::ip::tcp::socket>&& sock,
      Version version,
      uint8_t language,
      const std::string& name,
      phosg::TerminalFormat terminal_send_color,
      phosg::TerminalFormat terminal_recv_color);

  std::deque<std::string> outbound_data;
  bool should_disconnect = false;
  AsyncEvent send_buffer_nonempty_signal;

  asio::awaitable<void> send_task();
};

// In-process peer channel, used for replay testing.
class PeerChannel : public Channel {
public:
  std::weak_ptr<PeerChannel> peer;

  PeerChannel(
      std::shared_ptr<asio::io_context> io_context,
      Version version,
      uint8_t language,
      const std::string& name = "",
      phosg::TerminalFormat terminal_send_color = phosg::TerminalFormat::END,
      phosg::TerminalFormat terminal_recv_color = phosg::TerminalFormat::END);

  static void link_peers(std::shared_ptr<PeerChannel> peer1, std::shared_ptr<PeerChannel> peer2);

  virtual std::string default_name() const;

  virtual bool connected() const;
  virtual void disconnect();

  virtual void send_raw(std::string&& data);
  virtual asio::awaitable<void> recv_raw(void* data, size_t size);

private:
  AsyncEvent send_buffer_nonempty_signal;
  std::deque<std::string> inbound_data;
};
