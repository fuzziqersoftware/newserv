#include "Text.hh"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;



int char16cmp(const char16_t* s1, const char16_t* s2, size_t count) {
  return char_traits<char16_t>::compare(s1, s2, count);
}

char16_t* char16cpy(char16_t* dest, const char16_t* src, size_t count) {
  return char_traits<char16_t>::copy(dest, src, count);
}

size_t char16len(const char16_t* s) {
  return char_traits<char16_t>::length(s);
}



// None of these functions truly convert between SJIS and Unicode. They will
// convert English properly (and some other languages as well), but Japanese
// text will screw up horribly
// TODO: fix this shit. this is definitely the worst part of this entire project

void encode_sjis(char* dest, const char16_t* source, size_t max) {
  while (*source && (--max)) {
    *(dest++) = *(source++);
  };
  *dest = 0;
}

void decode_sjis(char16_t* dest, const char* source, size_t max) {
  while (*source && (--max)) {
    *(dest++) = *(source++);
  };
  *dest = 0;
}

std::string encode_sjis(const char16_t* source) {
  string ret;
  while (*source) {
    ret.push_back(*(source++));
  };
  return ret;
}

std::u16string decode_sjis(const char* source) {
  u16string ret;
  while (*source) {
    ret.push_back(*(source++));
  };
  return ret;
}

std::string encode_sjis(const std::u16string& source) {
  string ret;
  for (char16_t ch : source) {
    ret.push_back(ch);
  };
  return ret;
}

std::u16string decode_sjis(const std::string& source) {
  u16string ret;
  for (char16_t ch : source) {
    ret.push_back(ch);
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
  memmove(&a[2], a, existing_count + 1);
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
  memmove(&a[2], a, existing_count + 1);
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
    char16cpy(a, &a[2], char16len(a) - 2);
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
