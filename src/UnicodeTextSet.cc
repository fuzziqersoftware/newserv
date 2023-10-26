#include "UnicodeTextSet.hh"

#include <phosg/Strings.hh>
#include <string>
#include <vector>

#include "Compression.hh"

using namespace std;

vector<vector<string>> parse_unicode_text_set(const string& prs_data) {
  string data = prs_decompress(prs_data);
  StringReader r(data);

  uint32_t num_collections = r.get_u32l();
  deque<uint32_t> collection_sizes;
  while (collection_sizes.size() < num_collections) {
    collection_sizes.emplace_back(r.get_u32l());
  }

  vector<vector<string>> ret;
  ret.reserve(collection_sizes.size());
  while (!collection_sizes.empty()) {
    uint32_t num_strings = collection_sizes.front();
    collection_sizes.pop_front();

    auto& strings = ret.emplace_back();
    strings.reserve(num_strings);
    while (strings.size() < num_strings) {
      StringReader sub_r = r.sub(r.get_u32l());
      StringWriter w;
      for (uint16_t ch = sub_r.get_u16l(); ch != 0; ch = sub_r.get_u16l()) {
        w.put_u16l(ch);
      }
      strings.emplace_back(tt_utf16_to_utf8(w.str()));
    }
  }
  return ret;
}

string serialize_unicode_text_set(const vector<vector<string>>& collections) {
  StringWriter header_w;
  StringWriter data_w;

  size_t total_num_strings = 0;
  header_w.put_u32l(collections.size());
  for (const auto& collection : collections) {
    header_w.put_u32l(collection.size());
    total_num_strings += collection.size();
  }

  unordered_map<string, uint32_t> encoded;

  size_t data_base_offset = (total_num_strings * 4) + header_w.size();
  for (const auto& collection : collections) {
    for (const auto& s : collection) {
      auto encoded_it = encoded.find(s);
      if (encoded_it == encoded.end()) {
        uint32_t offset = data_base_offset + data_w.size();
        encoded.emplace(s, offset);
        string s_utf16 = tt_utf8_to_utf16(s);
        data_w.write(s_utf16.data(), s_utf16.size());
        data_w.put_u16(0);
        while (data_w.size() & 3) {
          data_w.put_u8(0);
        }
      } else {
        header_w.put_u32l(encoded_it->second);
      }
    }
  }

  header_w.write(data_w.str());
  return std::move(header_w.str());
}
