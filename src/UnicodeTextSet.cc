#include "UnicodeTextSet.hh"

#include <phosg/Strings.hh>
#include <string>
#include <vector>

#include "Compression.hh"

using namespace std;

vector<string> parse_unicode_text_set(const string& prs_data) {
  string data = prs_decompress(prs_data);
  StringReader r(data);
  r.skip(4);
  uint32_t count = r.get_u32l();

  vector<string> ret;
  while (ret.size() < count) {
    u16string s(&r.pget<char16_t>(r.get_u32l()));
    ret.emplace_back(tt_utf16_to_utf8(s.data(), s.size() * 2));
  }
  return ret;
}

string serialize_unicode_text_set(const vector<string>& strings) {
  StringWriter w;
  w.put_u32l(strings.size());
  size_t string_offset = (strings.size() * 4) + 4; // Header size
  for (const auto& s : strings) {
    w.put_u32l(string_offset);
    string_offset = (((s.size() + 1) << 1) + 3) & (~3);
  }
  for (const auto& s : strings) {
    string s_utf16 = tt_utf8_to_utf16(s);
    w.write(s_utf16.data(), s_utf16.size());
    w.put_u16(0);
    while (w.size() & 3) {
      w.put_u8(0);
    }
  }
  return std::move(w.str());
}
