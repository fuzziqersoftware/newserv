#include "GSLArchive.hh"

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <stdexcept>

#include "Text.hh"
#include "Types.hh"

using namespace std;

template <bool BE>
struct GSLHeaderEntryT {
  pstring<TextEncoding::ASCII, 0x20> filename;
  U32T<BE> offset; // In pages, so actual offset is this * 0x800
  U32T<BE> size;
  uint64_t unused;
} __packed__;

using GSLHeaderEntry = GSLHeaderEntryT<false>;
using GSLHeaderEntryBE = GSLHeaderEntryT<true>;
check_struct_size(GSLHeaderEntry, 0x30);
check_struct_size(GSLHeaderEntryBE, 0x30);

template <bool BE>
void GSLArchive::load_t() {
  phosg::StringReader r(*this->data);
  uint64_t min_data_offset = 0xFFFFFFFFFFFFFFFF;
  while (r.where() < min_data_offset) {
    const auto& entry = r.get<GSLHeaderEntryT<BE>>();
    if (entry.filename.empty()) {
      break;
    }
    uint64_t offset = static_cast<uint64_t>(entry.offset) * 0x800;
    if (offset + entry.size > this->data->size()) {
      throw runtime_error("GSL entry extends beyond end of data");
    }
    this->entries.emplace(entry.filename.decode(), Entry{offset, entry.size});
  }
}

GSLArchive::GSLArchive(shared_ptr<const string> data, bool big_endian)
    : data(data) {
  if (big_endian) {
    this->load_t<true>();
  } else {
    this->load_t<false>();
  }
}

const unordered_map<string, GSLArchive::Entry> GSLArchive::all_entries() const {
  return this->entries;
}

pair<const void*, size_t> GSLArchive::get(const std::string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return make_pair(this->data->data() + entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}

string GSLArchive::get_copy(const string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return this->data->substr(entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}

phosg::StringReader GSLArchive::get_reader(const string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return phosg::StringReader(this->data->data() + entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}

string GSLArchive::generate(const unordered_map<string, string>& files, bool big_endian) {
  return big_endian ? GSLArchive::generate_t<true>(files) : GSLArchive::generate_t<false>(files);
}

template <bool BE>
string GSLArchive::generate_t(const unordered_map<string, string>& files) {
  phosg::StringWriter w;

  // Make sure there's enough space for a blank header entry before any file's
  // data pages begin
  uint32_t data_start_offset = ((sizeof(GSLHeaderEntryT<BE>) * (files.size() + 1)) + 0x7FF) & (~0x7FF);
  uint32_t data_offset = data_start_offset;
  for (const auto& file : files) {
    GSLHeaderEntryT<BE> entry;
    entry.filename.encode(file.first);
    entry.offset = data_offset >> 11;
    entry.size = file.second.size();
    entry.unused = 0;
    w.put(entry);
    data_offset = (data_offset + file.second.size() + 0x7FF) & (~0x7FF);
  }
  w.extend_to(data_start_offset);

  for (const auto& file : files) {
    w.write(file.second);
    w.extend_to((w.size() + 0x7FF) & (~0x7FF));
  }

  return std::move(w.str());
}
