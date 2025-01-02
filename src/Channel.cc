#include "Channel.hh"

#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "Version.hh"

using namespace std;

extern bool use_terminal_colors;

static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}

Channel::Channel(
    Version version,
    uint8_t language,
    on_command_received_t on_command_received,
    on_error_t on_error,
    void* context_obj,
    const string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : bev(nullptr, flush_and_free_bufferevent),
      virtual_network_id(0),
      version(version),
      language(language),
      name(name),
      terminal_send_color(terminal_send_color),
      terminal_recv_color(terminal_recv_color),
      on_command_received(on_command_received),
      on_error(on_error),
      context_obj(context_obj) {
}

Channel::Channel(
    struct bufferevent* bev,
    uint64_t virtual_network_id,
    Version version,
    uint8_t language,
    on_command_received_t on_command_received,
    on_error_t on_error,
    void* context_obj,
    const string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : bev(nullptr, flush_and_free_bufferevent),
      version(version),
      language(language),
      name(name),
      terminal_send_color(terminal_send_color),
      terminal_recv_color(terminal_recv_color),
      on_command_received(on_command_received),
      on_error(on_error),
      context_obj(context_obj) {
  this->set_bufferevent(bev, virtual_network_id);
}

void Channel::replace_with(
    Channel&& other,
    on_command_received_t on_command_received,
    on_error_t on_error,
    void* context_obj,
    const std::string& name) {
  this->set_bufferevent(other.bev.release(), other.virtual_network_id);
  this->local_addr = other.local_addr;
  this->remote_addr = other.remote_addr;
  this->version = other.version;
  this->language = other.language;
  this->crypt_in = other.crypt_in;
  this->crypt_out = other.crypt_out;
  this->name = name;
  this->terminal_send_color = other.terminal_send_color;
  this->terminal_recv_color = other.terminal_recv_color;
  this->on_command_received = on_command_received;
  this->on_error = on_error;
  this->context_obj = context_obj;
  other.disconnect(); // Clears crypts, addrs, etc.
}

void Channel::set_bufferevent(struct bufferevent* bev, uint64_t virtual_network_id) {
  this->bev.reset(bev);
  this->virtual_network_id = virtual_network_id;

  if (this->bev.get()) {
    int fd = bufferevent_getfd(this->bev.get());
    if (fd < 0) {
      memset(&this->local_addr, 0, sizeof(this->local_addr));
      memset(&this->remote_addr, 0, sizeof(this->remote_addr));
    } else {
      phosg::get_socket_addresses(fd, &this->local_addr, &this->remote_addr);
    }

    bufferevent_setcb(this->bev.get(), &Channel::dispatch_on_input, nullptr, &Channel::dispatch_on_error, this);
    bufferevent_enable(this->bev.get(), EV_READ | EV_WRITE);

  } else {
    memset(&this->local_addr, 0, sizeof(this->local_addr));
    memset(&this->remote_addr, 0, sizeof(this->remote_addr));
  }
}

void Channel::disconnect() {
  if (this->bev.get()) {
    // If the output buffer is not empty, move the bufferevent into the draining
    // pool instead of disconnecting it, to make sure all the data gets sent.
    struct evbuffer* out_buffer = bufferevent_get_output(this->bev.get());
    if (evbuffer_get_length(out_buffer) == 0) {
      this->bev.reset(); // Destructor flushes and frees the bufferevent
    } else {
      // The callbacks will free it when all the data is sent or the client
      // disconnects

      auto on_output = +[](struct bufferevent* bev, void*) -> void {
        flush_and_free_bufferevent(bev);
      };

      auto on_error = +[](struct bufferevent* bev, short events, void*) -> void {
        if (events & BEV_EVENT_ERROR) {
          int err = EVUTIL_SOCKET_ERROR();
          channel_exceptions_log.warning(
              "Disconnecting channel caused error %d (%s)", err,
              evutil_socket_error_to_string(err));
        }
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
          bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
          bufferevent_free(bev);
        }
      };

      struct bufferevent* bev = this->bev.release();
      bufferevent_setcb(bev, nullptr, on_output, on_error, bev);
      bufferevent_disable(bev, EV_READ);
    }
  }

  memset(&this->local_addr, 0, sizeof(this->local_addr));
  memset(&this->remote_addr, 0, sizeof(this->remote_addr));
  this->virtual_network_id = false;
  this->crypt_in.reset();
  this->crypt_out.reset();
}

Channel::Message Channel::recv() {
  struct evbuffer* buf = bufferevent_get_input(this->bev.get());

  size_t header_size = (this->version == Version::BB_V4) ? 8 : 4;
  PSOCommandHeader header;
  if (evbuffer_copyout(buf, &header, header_size) < static_cast<ssize_t>(header_size)) {
    throw out_of_range("no command available");
  }

  if (this->crypt_in.get()) {
    this->crypt_in->decrypt(&header, header_size, false);
  }

  size_t command_logical_size = header.size(version);

  // If encryption is enabled, BB pads commands to 8-byte boundaries, and this
  // is not reflected in the size field. This logic does not occur if encryption
  // is not yet enabled.
  size_t command_physical_size = (this->crypt_in.get() && (version == Version::BB_V4))
      ? ((command_logical_size + 7) & ~7)
      : command_logical_size;
  if (evbuffer_get_length(buf) < command_physical_size) {
    throw out_of_range("no command available");
  }

  // If we get here, then there is a full command in the buffer. Some encryption
  // algorithms' advancement depends on the decrypted data, so we have to
  // actually decrypt the header again (with advance=true) to keep them in a
  // consistent state.

  string header_data(header_size, '\0');
  if (evbuffer_remove(buf, header_data.data(), header_data.size()) < static_cast<ssize_t>(header_data.size())) {
    throw logic_error("enough bytes available, but could not remove them");
  }
  if (this->crypt_in.get()) {
    this->crypt_in->decrypt(header_data.data(), header_data.size());
  }

  string command_data(command_physical_size - header_size, '\0');
  if (evbuffer_remove(buf, command_data.data(), command_data.size()) < static_cast<ssize_t>(command_data.size())) {
    throw logic_error("enough bytes available, but could not remove them");
  }

  if (this->crypt_in.get()) {
    // Some versions of PSO DC can send commands whose sizes are not a multiple
    // of 4, but the server is expected to always use a multiple of 4 bytes when
    // decrypting (the extra cipher bytes are lost). To emulate this behavior,
    // we have to round up the size for DC commands here.
    size_t orig_size = command_data.size();
    command_data.resize((orig_size + 3) & (~3), 0);
    this->crypt_in->decrypt(command_data.data(), command_data.size());
    command_data.resize(orig_size);
  }
  command_data.resize(command_logical_size - header_size);

  if (command_data_log.should_log(phosg::LogLevel::INFO) && (this->terminal_recv_color != phosg::TerminalFormat::END)) {
    if (use_terminal_colors && this->terminal_recv_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, this->terminal_recv_color, phosg::TerminalFormat::BOLD, phosg::TerminalFormat::END);
    }

    if (version == Version::BB_V4) {
      command_data_log.info(
          "Received from %s (version=BB command=%04hX flag=%08" PRIX32 ")",
          this->name.c_str(),
          header.command(this->version),
          header.flag(this->version));
    } else {
      command_data_log.info(
          "Received from %s (version=%s command=%02hX flag=%02" PRIX32 ")",
          this->name.c_str(),
          phosg::name_for_enum(this->version),
          header.command(this->version),
          header.flag(this->version));
    }

    vector<struct iovec> iovs;
    iovs.emplace_back(iovec{.iov_base = header_data.data(), .iov_len = header_data.size()});
    iovs.emplace_back(iovec{.iov_base = command_data.data(), .iov_len = command_data.size()});
    phosg::print_data(stderr, iovs, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);

    if (use_terminal_colors && this->terminal_recv_color != phosg::TerminalFormat::NORMAL) {
      phosg::print_color_escape(stderr, phosg::TerminalFormat::NORMAL, phosg::TerminalFormat::END);
    }
  }

  return {
      .command = header.command(this->version),
      .flag = header.flag(this->version),
      .data = std::move(command_data),
  };
}

void Channel::send(uint16_t cmd, uint32_t flag, bool silent) {
  this->send(cmd, flag, nullptr, 0, silent);
}

void Channel::send(uint16_t cmd, uint32_t flag, const std::vector<std::pair<const void*, size_t>> blocks, bool silent) {
  if (!this->connected()) {
    channel_exceptions_log.warning("Attempted to send command on closed channel; dropping data");
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
      if (this->crypt_out.get() &&
          (this->version != Version::DC_NTE) &&
          (this->version != Version::DC_11_2000) &&
          (this->version != Version::DC_V1)) {
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
      // BB has an annoying behavior here: command lengths must be multiples of
      // 4, but the actual data length must be a multiple of 8. If the size
      // field is not divisible by 8, 4 extra bytes are sent anyway. This
      // behavior only applies when encryption is enabled - any commands sent
      // before encryption is enabled have no size restrictions (except they
      // must include a full header and must fit in the client's receive
      // buffer), and no implicit extra bytes are sent.
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

  // All versions of PSO I've seen (so far) have a receive buffer 0x7C00
  // bytes in size
  if (send_data_size > 0x7C00) {
    throw runtime_error("outbound command too large");
  }

  send_data.reserve(send_data_size);
  for (const auto& b : blocks) {
    send_data.append(reinterpret_cast<const char*>(b.first), b.second);
  }
  send_data.resize(send_data_size, '\0');

  if (!silent && (command_data_log.should_log(phosg::LogLevel::INFO)) && (this->terminal_send_color != phosg::TerminalFormat::END)) {
    if (use_terminal_colors && this->terminal_send_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, phosg::TerminalFormat::FG_YELLOW, phosg::TerminalFormat::BOLD, phosg::TerminalFormat::END);
    }
    if (version == Version::BB_V4) {
      command_data_log.info("Sending to %s (version=BB command=%04hX flag=%08" PRIX32 ")",
          this->name.c_str(), cmd, flag);
    } else {
      command_data_log.info("Sending to %s (version=%s command=%02hX flag=%02" PRIX32 ")",
          this->name.c_str(), phosg::name_for_enum(version), cmd, flag);
    }
    phosg::print_data(stderr, send_data.data(), logical_size, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::DISABLE_COLOR | phosg::PrintDataFlags::OFFSET_16_BITS);
    if (use_terminal_colors && this->terminal_send_color != phosg::TerminalFormat::NORMAL) {
      print_color_escape(stderr, phosg::TerminalFormat::NORMAL, phosg::TerminalFormat::END);
    }
  }

  if (this->crypt_out.get()) {
    this->crypt_out->encrypt(send_data.data(), send_data.size());
  }

  struct evbuffer* buf = bufferevent_get_output(this->bev.get());
  evbuffer_add(buf, send_data.data(), send_data.size());
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
  return this->send(data.data(), data.size(), silent);
}

void Channel::dispatch_on_input(struct bufferevent*, void* ctx) {
  Channel* ch = reinterpret_cast<Channel*>(ctx);
  // The client can be disconnected during on_command_received, so we have to
  // make sure ch->bev is valid every time before calling recv()
  while (ch->bev.get()) {
    Message msg;
    try {
      msg = ch->recv();
    } catch (const out_of_range&) {
      break;
    } catch (const exception& e) {
      channel_exceptions_log.warning("Error receiving on channel: %s", e.what());
      ch->on_error(*ch, BEV_EVENT_ERROR);
      break;
    }
    if (ch->on_command_received) {
      ch->on_command_received(*ch, msg.command, msg.flag, msg.data);
    }
  }
}

void Channel::dispatch_on_error(struct bufferevent*, short events, void* ctx) {
  Channel* ch = reinterpret_cast<Channel*>(ctx);
  if (ch->on_error) {
    ch->on_error(*ch, events);
  } else {
    ch->disconnect();
  }
}
