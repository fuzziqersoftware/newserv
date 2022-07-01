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



std::string prepend_command_header(
    GameVersion version,
    bool encryption_enabled,
    uint16_t cmd,
    uint32_t flag,
    const std::string& data) {
  StringWriter ret;
  switch (version) {
    case GameVersion::GC:
    case GameVersion::DC: {
      PSOCommandHeaderDCGC header;
      if (encryption_enabled) {
        header.size = (sizeof(header) + data.size() + 3) & ~3;
      } else {
        header.size = (sizeof(header) + data.size());
      }
      header.command = cmd;
      header.flag = flag;
      ret.put(header);
      break;
    }
    case GameVersion::PC:
    case GameVersion::PATCH: {
      PSOCommandHeaderPC header;
      if (encryption_enabled) {
        header.size = (sizeof(header) + data.size() + 3) & ~3;
      } else {
        header.size = (sizeof(header) + data.size());
      }
      header.command = cmd;
      header.flag = flag;
      ret.put(header);
      break;
    }
    case GameVersion::BB: {
      PSOCommandHeaderBB header;
      if (encryption_enabled) {
        header.size = (sizeof(header) + data.size() + 7) & ~7;
      } else {
        header.size = (sizeof(header) + data.size());
      }
      header.command = cmd;
      header.flag = flag;
      ret.put(header);
      break;
    }
    default:
      throw logic_error("unimplemented game version in prepend_command_header");
  }
  ret.write(data);
  return move(ret.str());
}
