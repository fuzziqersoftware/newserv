#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>

#include "Types.hh"

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
  phosg::StringReader get_reader(size_t index) const;

  static std::string generate(const std::vector<std::string>& files, bool big_endian);

private:
  template <bool BE>
  static std::string generate_t(const std::vector<std::string>& files);

  std::shared_ptr<const std::string> data;
  std::vector<Entry> entries;
};
