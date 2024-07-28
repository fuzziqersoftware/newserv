#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>

class GSLArchive {
public:
  GSLArchive(std::shared_ptr<const std::string> data, bool big_endian);
  ~GSLArchive() = default;

  struct Entry {
    uint64_t offset;
    uint32_t size;
  };
  const std::unordered_map<std::string, Entry> all_entries() const;

  std::pair<const void*, size_t> get(const std::string& name) const;
  std::string get_copy(const std::string& name) const;
  phosg::StringReader get_reader(const std::string& name) const;

  static std::string generate(const std::unordered_map<std::string, std::string>& files, bool big_endian);

private:
  template <bool BE>
  void load_t();
  template <bool BE>
  static std::string generate_t(const std::unordered_map<std::string, std::string>& files);

  std::shared_ptr<const std::string> data;

  std::unordered_map<std::string, Entry> entries;
};
