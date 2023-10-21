#include "UnicodeTextSet.hh"

#include <phosg/Strings.hh>
#include <string>
#include <vector>

#include "Compression.hh"

using namespace std;

vector<u16string> parse_unicode_text_set(const string& prs_data) {
  string data = prs_decompress(prs_data);
  StringReader r(data);
  r.skip(4);
  uint32_t count = r.get_u32l();

  vector<u16string> ret;
  while (ret.size() < count) {
    ret.emplace_back(&r.pget<char16_t>(r.get_u32l()));
  }
  return ret;
}

string serialize_unicode_text_set(const vector<u16string>& strings) {
  StringWriter w;
  w.put_u32l(strings.size());
  size_t string_offset = (strings.size() * 4) + 4; // Header size
  for (const auto& s : strings) {
    w.put_u32l(string_offset);
    string_offset = (((s.size() + 1) << 1) + 3) & (~3);
  }
  for (const auto& s : strings) {
    u16string uni_s = decode_sjis(s);
    w.write(uni_s.c_str(), (uni_s.size() + 1) * 2);
    while (w.size() & 3) {
      w.put_u8(0);
    }
  }
  return std::move(w.str());
}
