#include "GSLArchive.hh"

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <stdexcept>

#include "Text.hh"

using namespace std;



// TODO: Support big-endian GSLs also (e.g. from PSO GC)

struct GSLHeaderEntry {
  ptext<char, 0x20> filename;
  le_uint32_t offset; // In pages, so actual offset is this * 0x800
  le_uint32_t size;
  uint64_t unused;
};

GSLArchive::GSLArchive(shared_ptr<const string> data) : data(data) {
  StringReader r(*this->data);
  uint64_t min_data_offset = 0xFFFFFFFFFFFFFFFF;
  while (r.where() < min_data_offset) {
    const auto& entry = r.get<GSLHeaderEntry>();
    if (entry.filename.len() == 0) {
      break;
    }
    uint64_t offset = static_cast<uint64_t>(entry.offset) * 0x800;
    if (offset + entry.size > this->data->size()) {
      throw runtime_error("GSL entry extends beyond end of data");
    }
    this->entries.emplace(entry.filename, Entry{offset, entry.size});
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

StringReader GSLArchive::get_reader(const string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return StringReader(this->data->data() + entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}
