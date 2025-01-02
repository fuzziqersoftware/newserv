#include "PSOProtocol.hh"

#include <event2/buffer.h>

#include <phosg/Strings.hh>
#include <stdexcept>

#include "Text.hh"

using namespace std;

extern bool use_terminal_colors;

PSOCommandHeader::PSOCommandHeader() {
  this->bb.size = 0;
  this->bb.command = 0;
  this->bb.flag = 0;
}

uint16_t PSOCommandHeader::command(Version version) const {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->pc.command;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      return this->dc.command;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->gc.command;
    case Version::XB_V3:
      return this->xb.command;
    case Version::BB_V4:
      return this->bb.command;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_command(Version version, uint16_t command) {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      this->pc.command = command;
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      this->dc.command = command;
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      this->gc.command = command;
      break;
    case Version::XB_V3:
      this->xb.command = command;
      break;
    case Version::BB_V4:
      this->bb.command = command;
      break;
    default:
      throw logic_error("unknown game version");
  }
}

uint16_t PSOCommandHeader::size(Version version) const {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->pc.size;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      return this->dc.size;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->gc.size;
    case Version::XB_V3:
      return this->xb.size;
    case Version::BB_V4:
      return this->bb.size;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_size(Version version, uint32_t size) {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      this->pc.size = size;
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      this->dc.size = size;
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      this->gc.size = size;
      break;
    case Version::XB_V3:
      this->xb.size = size;
      break;
    case Version::BB_V4:
      this->bb.size = size;
      break;
    default:
      throw logic_error("unknown game version");
  }
}

uint32_t PSOCommandHeader::flag(Version version) const {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->pc.flag;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      return this->dc.flag;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->gc.flag;
    case Version::XB_V3:
      return this->xb.flag;
    case Version::BB_V4:
      return this->bb.flag;
    default:
      throw logic_error("unknown game version");
  }
}

void PSOCommandHeader::set_flag(Version version, uint32_t flag) {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      this->pc.flag = flag;
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      this->dc.flag = flag;
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      this->gc.flag = flag;
      break;
    case Version::XB_V3:
      this->xb.flag = flag;
      break;
    case Version::BB_V4:
      this->bb.flag = flag;
      break;
    default:
      throw logic_error("unknown game version");
  }
}

void check_size_v(size_t size, size_t min_size, size_t max_size) {
  if (size < min_size) {
    throw std::runtime_error(phosg::string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, size));
  }
  if (max_size < min_size) {
    max_size = min_size;
  }
  if (size > max_size) {
    throw std::runtime_error(phosg::string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, size));
  }
}

std::string prepend_command_header(
    Version version,
    bool encryption_enabled,
    uint16_t cmd,
    uint32_t flag,
    const std::string& data) {
  phosg::StringWriter ret;
  switch (version) {
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

    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2: {
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

    case Version::BB_V4: {
      PSOCommandHeaderBB header;
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

    default:
      throw logic_error("unimplemented game version in prepend_command_header");
  }
  ret.write(data);
  return std::move(ret.str());
}
