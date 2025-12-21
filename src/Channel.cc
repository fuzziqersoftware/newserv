#include "Channel.hh"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "StaticGameData.hh"
#include "Version.hh"

using namespace std;

extern bool use_terminal_colors;

Channel::Channel(
    Version version,
    Language language,
    const string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : version(version),
      language(language),
      name(name),
      terminal_send_color(terminal_send_color),
      terminal_recv_color(terminal_recv_color) {
}

void Channel::send(uint16_t cmd, uint32_t flag, bool silent) {
  this->send(cmd, flag, nullptr, 0, silent);
}

void Channel::send(
    uint16_t cmd, uint32_t flag, const std::vector<std::pair<const void*, size_t>> blocks, bool silent) {
  if (!this->connected()) {
    channel_exceptions_log.warning_f("Attempted to send command on closed channel; dropping data");
    return;
  }

  size_t size = 0;
  for (const auto& b : blocks) {
    size += b.second;
  }

  string send_data;
  size_t logical_size;
  size_t send_data_size = 0;
  switch (this->version) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      PSOCommandHeaderDCV3 header;
      if (this->crypt_out.get() && !is_v1(this->version)) {
        send_data_size = (sizeof(header) + size + 3) & ~3;
      } else {
        send_data_size = (sizeof(header) + size);
      }
      logical_size = send_data_size;
      header.command = cmd;
      header.flag = flag;
      header.size = send_data_size;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      break;
    }
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2: {
      PSOCommandHeaderPC header;
      if (this->crypt_out.get()) {
        send_data_size = (sizeof(header) + size + 3) & ~3;
      } else {
        send_data_size = (sizeof(header) + size);
      }
      logical_size = send_data_size;
      header.size = send_data_size;
      header.command = cmd;
      header.flag = flag;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      break;
    }
    case Version::BB_V4: {
      // BB has an annoying behavior here: command lengths must be multiples of 4, but the actual data length must be a
      // multiple of 8. If the size field is not divisible by 8, 4 extra bytes are sent anyway. This behavior only
      // applies when encryption is enabled - any commands sent before encryption is enabled have no size restrictions
      // (except they must include a full header and must fit in the client's receive buffer), and no implicit extra
      // bytes are sent.
      PSOCommandHeaderBB header;
      if (this->crypt_out.get()) {
        send_data_size = (sizeof(header) + size + 7) & ~7;
      } else {
        send_data_size = (sizeof(header) + size);
      }
      logical_size = (sizeof(header) + size + 3) & ~3;
      header.size = logical_size;
      header.command = cmd;
      header.flag = flag;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      break;
    }

    default:
      throw logic_error("unimplemented game version in send_command");
  }

  // All versions of PSO I've seen (so far) have a receive buffer 0x7C00 bytes in size
  if (send_data_size > 0x7C00) {
    throw runtime_error("outbound command too large");
  }

  send_data.reserve(send_data_size);
  for (const auto& b : blocks) {
    send_data.append(reinterpret_cast<const char*>(b.first), b.second);
  }
  send_data.resize(send_data_size, '\0');

  if (!silent && (command_data_log.should_log(phosg::LogLevel::L_INFO)) && (this->terminal_send_color != phosg::TerminalFormat::END)) {
    if (use_terminal_colors && this->terminal_send_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, phosg::TerminalFormat::FG_YELLOW, phosg::TerminalFormat::BOLD, phosg::TerminalFormat::END);
    }
    if (version == Version::BB_V4) {
      command_data_log.info_f("Sending to {} (version=BB command={:04X} flag={:08X})", this->name, cmd, flag);
    } else {
      command_data_log.info_f("Sending to {} (version={} command={:02X} flag={:02X})",
          this->name, phosg::name_for_enum(version), cmd, flag);
    }
    phosg::print_data(stderr, send_data.data(), logical_size, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);
    if (use_terminal_colors && this->terminal_send_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, phosg::TerminalFormat::NORMAL, phosg::TerminalFormat::END);
    }
  }

  if (this->crypt_out.get()) {
    this->crypt_out->encrypt(send_data.data(), send_data.size());
  }

  this->send_raw(std::move(send_data));
}

void Channel::send(uint16_t cmd, uint32_t flag, const void* data, size_t size, bool silent) {
  this->send(cmd, flag, {make_pair(data, size)}, silent);
}

void Channel::send(uint16_t cmd, uint32_t flag, const string& data, bool silent) {
  this->send(cmd, flag, data.data(), data.size(), silent);
}

void Channel::send(const void* data, size_t size, bool silent) {
  size_t header_size = (this->version == Version::BB_V4) ? 8 : 4;
  const auto* header = reinterpret_cast<const PSOCommandHeader*>(data);
  this->send(
      header->command(this->version),
      header->flag(this->version),
      reinterpret_cast<const uint8_t*>(data) + header_size,
      size - header_size,
      silent);
}

void Channel::send(const string& data, bool silent) {
  this->send(data.data(), data.size(), silent);
}

asio::awaitable<Channel::Message> Channel::recv() {
  size_t header_size = (this->version == Version::BB_V4) ? 8 : 4;
  PSOCommandHeader header;
  co_await this->recv_raw(&header, header_size);
  if (this->crypt_in.get()) {
    this->crypt_in->decrypt(&header, header_size);
  }

  size_t command_logical_size = header.size(version);
  if (command_logical_size < header_size) {
    throw runtime_error("header size field is smaller than header");
  }

  // If encryption is enabled, BB pads commands to 8-byte boundaries, and this is not reflected in the size field. This
  // logic does not occur if encryption is not yet enabled.
  size_t command_physical_size = (this->crypt_in.get() && (version == Version::BB_V4))
      ? ((command_logical_size + 7) & ~7)
      : command_logical_size;

  string command_data(command_physical_size - header_size, '\0');
  co_await this->recv_raw(command_data.data(), command_data.size());

  if (this->crypt_in.get()) {
    // Some versions of PSO DC can send commands whose sizes are not a multiple of 4, but the server is expected to
    // always use a multiple of 4 bytes when decrypting (the extra cipher bytes are lost). To emulate this behavior, we
    // have to round up the size for DC commands here.
    size_t orig_size = command_data.size();
    command_data.resize((orig_size + 3) & (~3), 0);
    this->crypt_in->decrypt(command_data.data(), command_data.size());
    command_data.resize(orig_size);
  }
  command_data.resize(command_logical_size - header_size);

  if (command_data_log.should_log(phosg::LogLevel::L_INFO) && (this->terminal_recv_color != phosg::TerminalFormat::END)) {
    if (use_terminal_colors && this->terminal_recv_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, this->terminal_recv_color, phosg::TerminalFormat::BOLD, phosg::TerminalFormat::END);
    }

    if (version == Version::BB_V4) {
      command_data_log.info_f(
          "Received from {} (version=BB command={:04X} flag={:08X})",
          this->name,
          header.command(this->version),
          header.flag(this->version));
    } else {
      command_data_log.info_f(
          "Received from {} (version={} command={:02X} flag={:02X})",
          this->name,
          phosg::name_for_enum(this->version),
          header.command(this->version),
          header.flag(this->version));
    }

    vector<struct iovec> iovs;
    iovs.emplace_back(iovec{.iov_base = &header, .iov_len = header_size});
    iovs.emplace_back(iovec{.iov_base = command_data.data(), .iov_len = command_data.size()});
    phosg::print_data(stderr, iovs, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);

    if (use_terminal_colors && this->terminal_recv_color != phosg::TerminalFormat::NORMAL) {
      phosg::print_color_escape(stderr, phosg::TerminalFormat::NORMAL, phosg::TerminalFormat::END);
    }
  }

  co_return Message{
      .command = header.command(this->version),
      .flag = header.flag(this->version),
      .data = std::move(command_data),
  };
}

shared_ptr<SocketChannel> SocketChannel::create(
    std::shared_ptr<asio::io_context> io_context,
    std::unique_ptr<asio::ip::tcp::socket>&& sock,
    Version version,
    Language language,
    const string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color) {
  shared_ptr<SocketChannel> ret(new SocketChannel(
      io_context, std::move(sock), version, language, name, terminal_send_color, terminal_recv_color));
  asio::co_spawn(*io_context, ret->send_task(), asio::detached);
  return ret;
}

SocketChannel::SocketChannel(
    std::shared_ptr<asio::io_context> io_context,
    std::unique_ptr<asio::ip::tcp::socket>&& sock,
    Version version,
    Language language,
    const string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : Channel(version, language, name, terminal_send_color, terminal_recv_color),
      sock(std::move(sock)),
      local_addr(this->sock->local_endpoint()),
      remote_addr(this->sock->remote_endpoint()),
      send_buffer_nonempty_signal(io_context->get_executor()) {}

std::string SocketChannel::default_name() const {
  return "ip:" + str_for_endpoint(this->remote_addr);
}

bool SocketChannel::connected() const {
  return !this->should_disconnect && this->sock && this->sock->is_open();
}

void SocketChannel::disconnect() {
  this->should_disconnect = true;
  this->send_buffer_nonempty_signal.set();
}

void SocketChannel::send_raw(string&& data) {
  if (this->sock && !this->should_disconnect) {
    this->outbound_data.emplace_back(std::move(data));
    this->send_buffer_nonempty_signal.set();
  }
}

asio::awaitable<void> SocketChannel::recv_raw(void* data, size_t size) {
  if (!this->sock || this->should_disconnect) {
    throw runtime_error("Cannot receive on closed channel");
  }
  co_await asio::async_read(*this->sock, asio::buffer(data, size), asio::use_awaitable);
}

asio::awaitable<void> SocketChannel::send_task() {
  // Ensure *this doesn't get deleted while the socket is open
  auto this_sh = this->shared_from_this();

  while (this->sock->is_open()) {
    deque<string> to_send;
    to_send.swap(this->outbound_data);

    if (!to_send.empty()) {
      vector<asio::const_buffer> bufs;
      bufs.reserve(to_send.size());
      for (const auto& it : to_send) {
        bufs.emplace_back(asio::buffer(it.data(), it.size()));
      }
      co_await asio::async_write(*this->sock, bufs, asio::use_awaitable);
    }

    if (this->outbound_data.empty()) {
      if (this->should_disconnect) {
        this->sock->close();
      } else {
        this->send_buffer_nonempty_signal.clear();
        co_await this->send_buffer_nonempty_signal.wait();
      }
    }
  }
}

PeerChannel::PeerChannel(
    std::shared_ptr<asio::io_context> io_context,
    Version version,
    Language language,
    const std::string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : Channel(version, language, name, terminal_send_color, terminal_recv_color),
      send_buffer_nonempty_signal(io_context->get_executor()) {}

void PeerChannel::link_peers(std::shared_ptr<PeerChannel> peer1, std::shared_ptr<PeerChannel> peer2) {
  if (peer1->connected() || peer2->connected()) {
    throw logic_error("Cannot link already-connected peer channels");
  }
  peer1->peer = peer2;
  peer2->peer = peer1;
}

std::string PeerChannel::default_name() const {
  return std::format("peer:{}->{}", reinterpret_cast<const void*>(this), reinterpret_cast<const void*>(this->peer.lock().get()));
}

bool PeerChannel::connected() const {
  return (!this->inbound_data.empty()) || (this->peer.lock() != nullptr);
}

void PeerChannel::disconnect() {
  auto peer = this->peer.lock();
  if (peer) {
    peer->peer.reset();
    peer->send_buffer_nonempty_signal.set();
  }
  this->peer.reset();
  this->send_buffer_nonempty_signal.set();
}

void PeerChannel::send_raw(string&& data) {
  auto peer = this->peer.lock();
  if (peer) {
    peer->inbound_data.emplace_back(std::move(data));
    peer->send_buffer_nonempty_signal.set();
  }
}

asio::awaitable<void> PeerChannel::recv_raw(void* data, size_t size) {
  while (size > 0) {
    while (this->inbound_data.empty() && this->peer.lock()) {
      this->send_buffer_nonempty_signal.clear();
      co_await this->send_buffer_nonempty_signal.wait();
    }

    if (!this->inbound_data.empty()) {
      auto& front_block = this->inbound_data.front();
      if (size < front_block.size()) {
        memcpy(data, front_block.data(), size);
        front_block = front_block.substr(size);
        size = 0;
      } else {
        memcpy(data, front_block.data(), front_block.size());
        size -= front_block.size();
        data = reinterpret_cast<uint8_t*>(data) + front_block.size();
        this->inbound_data.pop_front();
      }
    } else if (!this->peer.lock()) {
      throw runtime_error("Channel peer has disconnected");
    }
  }
}
