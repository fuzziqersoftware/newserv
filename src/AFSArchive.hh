#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>

class AFSArchive {
public:
  AFSArchive(std::shared_ptr<const std::string> data);
  ~AFSArchive() = default;

  struct Entry {
    uint64_t offset;
    uint32_t size;
  };
  inline const std::vector<Entry>& all_entries() const {
    return this->entries;
  }

  std::pair<const void*, size_t> get(size_t index) const;
  std::string get_copy(size_t index) const;
  StringReader get_reader(size_t index) const;

private:
  std::shared_ptr<const std::string> data;
  std::vector<Entry> entries;
};
