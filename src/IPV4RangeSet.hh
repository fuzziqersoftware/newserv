#pragma once

#include <asio.hpp>
#include <phosg/JSON.hh>
#include <set>

class IPV4RangeSet {
public:
  IPV4RangeSet() = default;
  explicit IPV4RangeSet(const phosg::JSON& json);

  phosg::JSON json() const;

  bool check(uint32_t addr) const;

protected:
  std::map<uint32_t, uint8_t> ranges; // {addr: mask_bits}
};
