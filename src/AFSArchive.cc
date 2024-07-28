#include "AFSArchive.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;

AFSArchive::AFSArchive(shared_ptr<const string> data)
    : data(data) {
  struct FileHeader {
    be_uint32_t magic;
    le_uint32_t num_files;
  } __packed_ws__(FileHeader, 8);

  struct FileEntry {
    le_uint32_t offset;
    le_uint32_t size;
  } __packed_ws__(FileEntry, 8);

  phosg::StringReader r(*this->data);
  const auto& header = r.get<FileHeader>();
  if (header.magic != 0x41465300) { // 'AFS\0'
    throw runtime_error("file is not an AFS archive");
  }

  while (this->entries.size() < header.num_files) {
    const auto& entry = r.get<FileEntry>();
    this->entries.emplace_back(Entry{.offset = entry.offset, .size = entry.size});
  }
}

pair<const void*, size_t> AFSArchive::get(size_t index) const {
  const auto& entry = this->entries.at(index);
  if (entry.offset > this->data->size()) {
    throw out_of_range("entry begins beyond end of archive");
  }
  if (entry.offset + entry.size > this->data->size()) {
    throw out_of_range("entry extends beyond end of archive");
  }

  return make_pair(this->data->data() + entry.offset, entry.size);
}

string AFSArchive::get_copy(size_t index) const {
  auto ret = this->get(index);
  return string(reinterpret_cast<const char*>(ret.first), ret.second);
}

phosg::StringReader AFSArchive::get_reader(size_t index) const {
  auto ret = this->get(index);
  return phosg::StringReader(ret.first, ret.second);
}

string AFSArchive::generate(const vector<string>& files, bool big_endian) {
  return big_endian ? AFSArchive::generate_t<true>(files) : AFSArchive::generate_t<false>(files);
}

template <bool BE>
string AFSArchive::generate_t(const vector<string>& files) {
  phosg::StringWriter w;
  w.put_u32b(0x41465300); // 'AFS\0'
  w.put<U32T<BE>>(files.size());

  // It seems entries are aligned to 0x800-byte boundaries, and the file's
  // header is always 0x80000 (!) bytes, most of which is unused
  uint32_t data_offset = 0x80000;
  for (const auto& file : files) {
    w.put<U32T<BE>>(data_offset);
    w.put<U32T<BE>>(file.size());
    data_offset = (data_offset + file.size() + 0x7FF) & (~0x7FF);
  }

  w.extend_to(0x80000);
  for (const auto& file : files) {
    w.write(file);
    w.extend_to((w.size() + 0x7FF) & (~0x7FF));
  }

  return std::move(w.str());
}
