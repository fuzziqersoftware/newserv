#include "PSOProtocol.hh"

#include <event2/buffer.h>

#include <stdexcept>
#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;



extern bool use_terminal_colors;



PSOCommandHeader::PSOCommandHeader() {
  this->bb.size = 0;
  this->bb.command = 0;
  this->bb.flag = 0;
}

uint16_t PSOCommandHeader::command(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
      return this->dc.command;
    case GameVersion::GC:
      return this->gc.command;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return this->pc.command;
    case GameVersion::BB:
      return this->bb.command;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_command(GameVersion version, uint16_t command) {
  switch (version) {
    case GameVersion::DC:
      this->dc.command = command;
      break;
    case GameVersion::GC:
      this->gc.command = command;
      break;
    case GameVersion::PC:
    case GameVersion::PATCH:
      this->pc.command = command;
      break;
    case GameVersion::BB:
      this->bb.command = command;
      break;
    default:
      throw logic_error("unknown game version");
  }
}

uint16_t PSOCommandHeader::size(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
      return this->dc.size;
    case GameVersion::GC:
      return this->gc.size;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return this->pc.size;
    case GameVersion::BB:
      return this->bb.size;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_size(GameVersion version, uint32_t size) {
  switch (version) {
    case GameVersion::DC:
      this->dc.size = size;
      break;
    case GameVersion::GC:
      this->gc.size = size;
      break;
    case GameVersion::PC:
    case GameVersion::PATCH:
      this->pc.size = size;
      break;
    case GameVersion::BB:
      this->bb.size = size;
      break;
    default:
      throw logic_error("unknown game version");
  }
}

uint32_t PSOCommandHeader::flag(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
      return this->dc.flag;
    case GameVersion::GC:
      return this->gc.flag;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return this->pc.flag;
    case GameVersion::BB:
      return this->bb.flag;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_flag(GameVersion version, uint32_t flag) {
  switch (version) {
    case GameVersion::DC:
      this->dc.flag = flag;
      break;
    case GameVersion::GC:
      this->gc.flag = flag;
      break;
    case GameVersion::PC:
    case GameVersion::PATCH:
      this->pc.flag = flag;
      break;
    case GameVersion::BB:
      this->bb.flag = flag;
      break;
    default:
      throw logic_error("unknown game version");
  }
}



void for_each_received_command(
    struct bufferevent* bev,
    GameVersion version,
    PSOEncryption* crypt,
    function<void(uint16_t, uint16_t, string&)> fn) {
  struct evbuffer* buf = bufferevent_get_input(bev);

  size_t header_size = (version == GameVersion::BB) ? 8 : 4;
  for (;;) {
    PSOCommandHeader header;
    if (evbuffer_copyout(buf, &header, header_size)
        < static_cast<ssize_t>(header_size)) {
      break;
    }

    if (crypt) {
      crypt->decrypt(&header, header_size, false);
    }

    size_t command_logical_size = header.size(version);

    // If encryption is enabled, BB pads commands to 8-byte boundaries, and this
    // is not reflected in the size field. This logic does not occur if
    // encryption is not yet enabled.
    size_t command_physical_size = (crypt && (version == GameVersion::BB))
        ? ((command_logical_size + header_size - 1) & ~(header_size - 1))
        : command_logical_size;
    if (evbuffer_get_length(buf) < command_physical_size) {
      break;
    }

    // If we get here, then there is a full command in the buffer. Some
    // encryption algorithms' advancement depends on the decrypted data, so we
    // have to actually decrypt the header again (with advance=true) to keep
    // them in a consistent state.

    string header_data(header_size, '\0');
    if (evbuffer_remove(buf, header_data.data(), header_data.size())
        < static_cast<ssize_t>(header_data.size())) {
      throw logic_error("enough bytes available, but could not remove them");
    }

    string command_data(command_physical_size - header_size, '\0');
    if (evbuffer_remove(buf, command_data.data(), command_data.size())
        < static_cast<ssize_t>(command_data.size())) {
      throw logic_error("enough bytes available, but could not remove them");
    }

    if (crypt) {
      crypt->decrypt(header_data.data(), header_data.size());
      crypt->decrypt(command_data.data(), command_data.size());
    }
    command_data.resize(command_logical_size - header_size);

    fn(header.command(version), header.flag(version), command_data);
  }
}

void print_received_command(
    uint16_t command,
    uint32_t flag,
    const void* data,
    size_t size,
    GameVersion version,
    const char* name,
    TerminalFormat color) {
  if (use_terminal_colors) {
    print_color_escape(stderr, color, TerminalFormat::BOLD, TerminalFormat::END);
  }

  string name_token;
  if (name && name[0]) {
    name_token = string(" from ") + name;
  }
  log(INFO, "Received%s (version=%s command=%04hX flag=%08X)",
      name_token.c_str(), name_for_version(version), command, flag);

  PSOCommandHeader header;
  size_t header_size = header.header_size(version);
  header.set_command(version, command);
  header.set_flag(version, flag);
  header.set_size(version, size + header_size);

  vector<struct iovec> iovs;
  iovs.emplace_back(iovec{.iov_base = &header, .iov_len = header_size});
  iovs.emplace_back(iovec{.iov_base = const_cast<void*>(data), .iov_len = size});
  print_data(stderr, iovs, 0, nullptr, PrintDataFlags::PRINT_ASCII | PrintDataFlags::DISABLE_COLOR);

  if (use_terminal_colors) {
    print_color_escape(stderr, TerminalFormat::NORMAL, TerminalFormat::END);
  }
}

void check_size_v(size_t size, size_t min_size, size_t max_size) {
  if (size < min_size) {
    throw std::runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, size));
  }
  if (max_size < min_size) {
    max_size = min_size;
  }
  if (size > max_size) {
    throw std::runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, size));
  }
}
