#include "IPV4RangeSet.hh"

static std::string format_addr(uint32_t addr, uint8_t mask_bits) {
  return std::format("{}.{}.{}.{}/{}",
      (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF, mask_bits);
}

IPV4RangeSet::IPV4RangeSet(const phosg::JSON& json) {
  for (const auto& it : json.as_list()) {
    // String should be of the form a.b.c.d or a.b.c.d/e
    auto tokens = phosg::split(it->as_string(), '/');

    size_t mask_bits;
    if (tokens.size() == 1) {
      mask_bits = 32;
    } else if (tokens.size() == 2) {
      mask_bits = stoul(tokens[1], nullptr, 10);
      if (mask_bits > 32) {
        throw std::runtime_error("invalid IPv4 address range");
      }
    } else {
      throw std::runtime_error("invalid IPv4 address range");
    }

    auto addr_tokens = phosg::split(tokens[0], '.');
    if (addr_tokens.size() != 4) {
      throw std::runtime_error("invalid IPv4 address");
    }
    uint32_t addr = 0;
    if (mask_bits != 0) {
      for (size_t z = 0; z < 4; z++) {
        size_t end_pos = 0;
        size_t new_byte = stoul(addr_tokens[z], &end_pos, 10);
        if (end_pos != addr_tokens[z].size() || new_byte > 0xFF) {
          throw std::runtime_error("invalid IPv4 address");
        }
        addr = (addr << 8) | new_byte;
      }
      addr &= (0xFFFFFFFF << (32 - mask_bits));
    }

    this->ranges.emplace(addr, mask_bits);
  }

  // Ensure no ranges overlap (if they do, check() may misbehave)
  uint64_t addr = 0;
  for (auto& [start_addr, mask_bits] : this->ranges) {
    if (start_addr < addr) {
      throw std::runtime_error("IPv4 address ranges overlap");
    }
    addr = (mask_bits == 0) ? 0x100000000ULL : (start_addr + (1 << (32 - mask_bits)));
  }
}

phosg::JSON IPV4RangeSet::json() const {
  auto ret = phosg::JSON::list();
  for (const auto& [addr, mask_bits] : this->ranges) {
    ret.emplace_back(format_addr(addr, mask_bits));
  }
  return ret;
}

bool IPV4RangeSet::check(uint32_t addr) const {
  auto it = this->ranges.upper_bound(addr);
  if (it == this->ranges.begin()) {
    return false; // addr is before any range
  }
  const auto& range = *(--it);
  return (((range.first ^ addr) & (0xFFFFFFFF << (32 - range.second))) == 0);
}
