#include "Text.hh"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

using namespace std;



int char16ncmp(const char16_t* s1, const char16_t* s2, size_t count) {
  size_t x;
  for (x = 0; x < count && s1[x] != 0 && s2[x] != 0; x++) {
    if (s1[x] < s2[x]) {
      return -1;
    } else if (s1[x] > s2[x]) {
      return 1;
    }
  }
  if (s1[x] < s2[x]) {
    return -1;
  } else if (s1[x] > s2[x]) {
    return 1;
  }
  return 0;
}



static vector<char16_t> unicode_to_sjis_table_data;
static vector<char16_t> sjis_to_unicode_table_data;

static void load_sjis_tables() {
  unicode_to_sjis_table_data.resize(0x10000, 0);
  sjis_to_unicode_table_data.resize(0x10000, 0);

  // TODO: this is inefficient; it makes multiple copies of the string
  auto file_contents = load_file("system/sjis-table.ini");
  auto lines = split(file_contents, '\n');
  for (auto line : lines) {
    auto tokens = split(line, '\t');
    if (tokens.size() < 2) {
      continue;
    }
    char16_t sjis_char = stoul(tokens[0], nullptr, 0);
    char16_t unicode_char = stoul(tokens[1], nullptr, 0);

    unicode_to_sjis_table_data[unicode_char] = sjis_char;
    sjis_to_unicode_table_data[sjis_char] = unicode_char;
  }
}

static const vector<char16_t>& sjis_to_unicode_table() {
  if (sjis_to_unicode_table_data.empty()) {
    load_sjis_tables();
  }
  return sjis_to_unicode_table_data;
}

static const vector<char16_t>& unicode_to_sjis_table() {
  if (unicode_to_sjis_table_data.empty()) {
    load_sjis_tables();
  }
  return unicode_to_sjis_table_data;
}



std::string encode_sjis(const char16_t* src, size_t src_count) {
  const auto& table = unicode_to_sjis_table();

  const char16_t* src_end = src + src_count;
  string ret;
  while ((src != src_end) && *src) {
    uint16_t ch = *(src++);
    uint16_t translated_c = table[ch];
    if (translated_c == 0) {
      throw runtime_error("untranslatable unicode character");
    } else if (translated_c & 0xFF00) {
      ret.push_back((translated_c >> 8) & 0xFF);
      ret.push_back(translated_c & 0xFF);
    } else {
      ret.push_back(translated_c & 0xFF);
    }
  };
  return ret;
}

void encode_sjis(
    char* dest,
    size_t dest_count,
    const char16_t* src,
    size_t src_count) {
  const auto& table = unicode_to_sjis_table();

  if (dest_count == 0) {
    throw logic_error("cannot encode into zero-length buffer");
  }

  const char16_t* src_end = src + src_count;
  const char* dest_end = dest + (dest_count - 1);
  while ((dest != dest_end) && (src != src_end) && *src) {
    uint16_t ch = *(src++);
    uint16_t translated_c = table[ch];
    if (translated_c == 0) {
      throw runtime_error("untranslatable unicode character");
    } else if (translated_c & 0xFF00) {
      *(dest++) = (translated_c >> 8) & 0xFF;
      // If the second byte of this character would cause the null to overrun
      // the buffer, erase the first byte instead and return early
      if (dest == dest_end) {
        *(dest - 1) = 0;
      } else {
        *(dest++) = translated_c & 0xFF;
      }
    } else {
      *(dest++) = translated_c & 0xFF;
    }
  }
  *dest = 0;
}

std::u16string decode_sjis(const char* src, size_t src_count) {
  const auto& table = sjis_to_unicode_table();

  const char* src_end = src + src_count;
  u16string ret;
  while ((src != src_end) && *src) {
    uint16_t src_char = *(src++);
    if (src_char & 0x80) {
      if (src == src_end) {
        throw runtime_error("incomplete extended character");
      }
      src_char = (src_char << 8) | *(src++);
      if ((src_char & 0xFF) == 0) {
        throw runtime_error("incomplete extended character");
      }
    }
    ret.push_back(table[src_char]);
  };
  return ret;
}

void decode_sjis(
    char16_t* dest,
    size_t dest_count,
    const char* src,
    size_t src_count) {
  const auto& table = sjis_to_unicode_table();

  if (dest_count == 0) {
    throw logic_error("cannot decode into zero-length buffer");
  }

  const char* src_end = src + src_count;
  const char16_t* dest_end = dest + (dest_count - 1);
  while ((dest != dest_end) && (src != src_end) && *src) {
    uint16_t src_char = *(src++);
    if (src_char & 0x80) {
      if (src == src_end) {
        throw runtime_error("incomplete extended character");
      }
      src_char = (src_char << 8) | *(src++);
      if ((src_char & 0xFF) == 0) {
        throw runtime_error("incomplete extended character");
      }
    }
    *(dest++) = table[src_char];
  };
}
