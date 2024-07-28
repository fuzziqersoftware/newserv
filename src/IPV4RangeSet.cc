#include "IPV4RangeSet.hh"

#include <arpa/inet.h>

using namespace std;

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
        throw runtime_error("invalid IPv4 address range");
      }
    } else {
      throw runtime_error("invalid IPv4 address range");
    }

    auto addr_tokens = phosg::split(tokens[0], '.');
    if (addr_tokens.size() != 4) {
      throw runtime_error("invalid IPv4 address");
    }
    uint32_t addr = 0;
    for (size_t z = 0; z < 4; z++) {
      size_t end_pos = 0;
      size_t new_byte = stoul(addr_tokens[z], &end_pos, 10);
      if (end_pos != addr_tokens[z].size() || new_byte > 0xFF) {
        throw runtime_error("invalid IPv4 address");
      }
      addr = (addr << 8) | new_byte;
    }
    addr &= (0xFFFFFFFF << (32 - mask_bits));

    this->ranges.emplace(addr, mask_bits);
  }
}

phosg::JSON IPV4RangeSet::json() const {
  auto ret = phosg::JSON::list();
  for (const auto& it : this->ranges) {
    uint32_t addr = it.first;
    uint8_t mask_bits = it.second;
    ret.emplace_back(phosg::string_printf("%hhu.%hhu.%hhu.%hhu/%hhu",
        static_cast<uint8_t>((addr >> 24) & 0xFF),
        static_cast<uint8_t>((addr >> 16) & 0xFF),
        static_cast<uint8_t>((addr >> 8) & 0xFF),
        static_cast<uint8_t>(addr & 0xFF),
        mask_bits));
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

bool IPV4RangeSet::check(const struct sockaddr_storage& ss) const {
  if (ss.ss_family != AF_INET) {
    return false;
  }
  const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(&ss);
  return this->check(ntohl(sin->sin_addr.s_addr));
}
