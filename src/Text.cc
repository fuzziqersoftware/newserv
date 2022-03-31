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

void char16ncpy(char16_t* dest, const char16_t* src, size_t count) {
  size_t x;
  for (x = 0; x < count && src[x] != 0; x++) {
    dest[x] = src[x];
  }
  if (x < count) {
    dest[x] = 0;
  }
}

size_t char16len(const char16_t* s) {
  size_t x;
  for (x = 0; s[x] != 0; x++);
  return x;
}



static vector<char16_t> unicode_to_sjis_table_data;
static vector<char16_t> sjis_to_unicode_table_data;

static void load_sjis_tables() {
  unicode_to_sjis_table_data.resize(0x10000);
  sjis_to_unicode_table_data.resize(0x10000);

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

// TODO: It looks like these functions are probably wrong. Specifically, we
// don't write the high byte when encoding non-ASCII chars, do we?

void encode_sjis(char* dest, const char16_t* source, size_t max) {
  const auto& table = unicode_to_sjis_table();
  while (*source && (--max)) {
    *(dest++) = table[*(source++)];
  };
  *dest = 0;
}

void decode_sjis(char16_t* dest, const char* source, size_t max) {
  const auto& table = sjis_to_unicode_table();
  while (*source && (--max)) {
    char16_t src_char = *(source++);
    if (src_char & 0x80) {
      src_char = (src_char << 8) | *(source++);
      if ((src_char & 0xFF) == 0) {
        return;
      }
    }
    *(dest++) = table[src_char];
  };
  *dest = 0;
}

std::string encode_sjis(const char16_t* src, size_t src_count) {
  const auto& table = unicode_to_sjis_table();
  string ret;
  for (; *src && (src_count > 0); src_count--) {
    ret.push_back(table[*(src++)]);
  };
  return ret;
}

std::u16string decode_sjis(const char* src, size_t src_count) {
  const auto& table = sjis_to_unicode_table();
  u16string ret;
  while (*src && (src_count > 0)) {
    char16_t src_char = *(src++);
    src_count--;
    if (src_char & 0x80) {
      if (src_count == 0) {
        return ret;
      }
      src_char = (src_char << 8) | *(src++);
      if ((src_char & 0xFF) == 0) {
        return ret;
      }
      src_count--;
    }
    ret.push_back(table[src_char]);
  };
  return ret;
}

std::string encode_sjis(const std::u16string& source) {
  const auto& table = unicode_to_sjis_table();
  string ret;
  for (char16_t ch : source) {
    ret.push_back(table[ch]);
  };
  return ret;
}

std::u16string decode_sjis(const std::string& source) {
  const auto& table = sjis_to_unicode_table();
  u16string ret;
  for (size_t x = 0; x < source.size();) {
    char16_t src_char = source[x++];
    if (src_char & 0x80) {
      if (x == source.size()) {
        return ret;
      }
      src_char = (src_char << 8) | source[x++];
      if ((src_char & 0xFF) == 0) {
        return ret;
      }
    }
    ret.push_back(table[src_char]);
  };
  return ret;
}



void add_language_marker_inplace(char* a, char e, size_t dest_count) {
  if ((a[0] == '\t') && (a[1] != 'C')) {
    return;
  }

  size_t existing_count = strlen(a);
  if (existing_count > dest_count - 3) {
    existing_count = dest_count - 3;
  }
  memmove(&a[2], a, (existing_count + 1) * sizeof(char));
  a[0] = '\t';
  a[1] = e;
  a[existing_count + 2] = 0;
}

void add_language_marker_inplace(char16_t* a, char16_t e, size_t dest_count) {
  if ((a[0] == '\t') && (a[1] != 'C')) {
    return;
  }

  size_t existing_count = char16len(a);
  if (existing_count > dest_count - 3) {
    existing_count = dest_count - 3;
  }
  memmove(&a[2], a, (existing_count + 1) * sizeof(char16_t));
  a[0] = '\t';
  a[1] = e;
  a[existing_count + 2] = 0;
}

void remove_language_marker_inplace(char* a) {
  if ((a[0] == '\t') && (a[1] != 'C')) {
    strcpy(a, &a[2]);
  }
}

void remove_language_marker_inplace(char16_t* a) {
  if ((a[0] == '\t') && (a[1] != 'C')) {
    char16ncpy(a, &a[2], char16len(a) - 2);
  }
}

std::string add_language_marker(const std::string& s, char marker) {
  if ((s.size() >= 2) && (s[0] == '\t') && (s[1] != 'C')) {
    return s;
  }

  string ret;
  ret.push_back('\t');
  ret.push_back(marker);
  return ret + s;
}

std::u16string add_language_marker(const std::u16string& s, char16_t marker) {
  if ((s.size() >= 2) && (s[0] == L'\t') && (s[1] != L'C')) {
    return s;
  }

  u16string ret;
  ret.push_back(L'\t');
  ret.push_back(marker);
  return ret + s;
}

std::string remove_language_marker(const std::string& s) {
  if ((s.size() < 2) || (s[0] != '\t') || (s[1] == 'C')) {
    return s;
  }
  return s.substr(2);
}

std::u16string remove_language_marker(const std::u16string& s) {
  if ((s.size() < 2) || (s[0] != L'\t') || (s[1] == L'C')) {
    return s;
  }
  return s.substr(2);
}
