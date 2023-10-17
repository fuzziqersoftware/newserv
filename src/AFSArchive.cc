#include "AFSArchive.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

using namespace std;

AFSArchive::AFSArchive(std::shared_ptr<const std::string> data)
    : data(data) {
  struct FileHeader {
    be_uint32_t magic;
    le_uint32_t num_files;
  } __attribute__((packed));

  struct FileEntry {
    le_uint32_t offset;
    le_uint32_t size;
  } __attribute__((packed));

  StringReader r(*this->data);
  const auto& header = r.get<FileHeader>();
  if (header.magic != 0x41465300) {
    throw runtime_error("file is not an AFS archive");
  }

  while (this->entries.size() < header.num_files) {
    const auto& entry = r.get<FileEntry>();
    this->entries.emplace_back(Entry{.offset = entry.offset, .size = entry.size});
  }
}

std::pair<const void*, size_t> AFSArchive::get(size_t index) const {
  const auto& entry = this->entries.at(index);
  if (entry.offset > this->data->size()) {
    throw out_of_range("entry begins beyond end of archive");
  }
  if (entry.offset + entry.size > this->data->size()) {
    throw out_of_range("entry extends beyond end of archive");
  }

  return make_pair(this->data->data() + entry.offset, entry.size);
}

std::string AFSArchive::get_copy(size_t index) const {
  auto ret = this->get(index);
  return string(reinterpret_cast<const char*>(ret.first), ret.second);
}

StringReader AFSArchive::get_reader(size_t index) const {
  auto ret = this->get(index);
  return StringReader(ret.first, ret.second);
}
