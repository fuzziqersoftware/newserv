#include "PSOProtocol.hh"

#include <event2/buffer.h>

#include <stdexcept>

using namespace std;



PSOCommandHeader::PSOCommandHeader() {
  this->bb.size = 0;
  this->bb.command = 0;
  this->bb.flag = 0;
}

uint16_t PSOCommandHeader::command(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->command;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->command;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->command;
  }
  throw logic_error("unknown game version");
}

uint16_t PSOCommandHeader::size(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->size;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->size;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->size;
  }
  throw logic_error("unknown game version");
}

uint32_t PSOCommandHeader::flag(GameVersion version) const {
  switch (version) {
    case GameVersion::DC:
    case GameVersion::GC:
      return reinterpret_cast<const PSOCommandHeaderDCGC*>(this)->flag;
    case GameVersion::PC:
    case GameVersion::PATCH:
      return reinterpret_cast<const PSOCommandHeaderPC*>(this)->flag;
    case GameVersion::BB:
      return reinterpret_cast<const PSOCommandHeaderBB*>(this)->flag;
  }
  throw logic_error("unknown game version");
}



void for_each_received_command(
    struct bufferevent* bev,
    GameVersion version,
    PSOEncryption* crypt,
    function<void(uint16_t, uint16_t, const string&)> fn) {
  struct evbuffer* buf = bufferevent_get_input(bev);

  size_t header_size = version == GameVersion::BB ? 8 : 4;
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

    // BB pads commands to 8-byte boundaries, and this is not reflected in the
    // size field
    size_t command_physical_size = (version == GameVersion::BB)
        ? (command_logical_size + header_size - 1) & ~(header_size - 1)
        : command_logical_size;
    if (evbuffer_get_length(buf) < command_physical_size) {
      break;
    }

    // If we get here, then there is a full command in the buffer

    evbuffer_drain(buf, header_size);

    string command_data(command_logical_size - header_size, '\0');
    if (evbuffer_remove(buf, command_data.data(), command_data.size())
        < static_cast<ssize_t>(command_data.size())) {
      throw logic_error("enough bytes available, but could not remove them");
    }
    if (command_logical_size != command_physical_size) {
      evbuffer_drain(buf, command_physical_size - command_logical_size);
    }

    if (crypt) {
      crypt->skip(header_size);
      crypt->decrypt(command_data.data(), command_data.size());
    }

    fn(header.command(version), header.flag(version), command_data);
  }
}