#pragma once

#include <map>
#include <phosg/JSON.hh>

class IPV4RangeSet {
public:
  IPV4RangeSet() = default;
  explicit IPV4RangeSet(const phosg::JSON& json);

  phosg::JSON json() const;

  bool check(uint32_t addr) const;

protected:
  std::map<uint32_t, uint32_t> ranges; // {start_addr: end_addr (inclusive)}
};
