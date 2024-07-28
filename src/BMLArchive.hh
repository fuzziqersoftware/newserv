#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>

class BMLArchive {
public:
  BMLArchive(std::shared_ptr<const std::string> data, bool big_endian);
  ~BMLArchive() = default;

  struct Entry {
    uint64_t offset;
    uint32_t size;
    uint64_t gvm_offset;
    uint32_t gvm_size;
  };
  const std::unordered_map<std::string, Entry> all_entries() const;

  std::pair<const void*, size_t> get(const std::string& name) const;
  std::pair<const void*, size_t> get_gvm(const std::string& name) const;
  std::string get_copy(const std::string& name) const;
  phosg::StringReader get_reader(const std::string& name) const;

private:
  template <bool BE>
  void load_t();

  std::shared_ptr<const std::string> data;

  std::unordered_map<std::string, Entry> entries;
};
