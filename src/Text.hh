#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>


int char16cmp(const char16_t* s1, const char16_t* s2, size_t count);
void char16cpy(char16_t* dest, const char16_t* src, size_t count);
size_t char16len(const char16_t* s);


void encode_sjis(char* dest, const char16_t* source, size_t dest_count);
void decode_sjis(char16_t* dest, const char* source, size_t dest_count);
std::string encode_sjis(const char16_t* source);
std::u16string decode_sjis(const char* source);
std::string encode_sjis(const std::u16string& source);
std::u16string decode_sjis(const std::string& source);


void add_language_marker_inplace(char* s, char marker, size_t dest_count);
void add_language_marker_inplace(char16_t* s, char16_t marker, size_t dest_count);
void remove_language_marker_inplace(char* s);
void remove_language_marker_inplace(char16_t* s);
std::string add_language_marker(const std::string& s, char marker);
std::u16string add_language_marker(const std::u16string& s, char16_t marker);
std::string remove_language_marker(const std::string& s);
std::u16string remove_language_marker(const std::u16string& s);


template <typename T>
void replace_char_inplace(T* a, T f, T r) {
  while (*a) {
    if (*a == f) {
      *a = r;
    }
    a++;
  }
}

template <typename T>
size_t add_color_inplace(T* a, size_t max_chars) {
  T* d = a;
  T* orig_d = d;

  for (size_t x = 0; (x < max_chars) && *a; x++) {
    if (*a == '$') {
      *(d++) = '\t';
    } else if (*a == '#') {
      *(d++) = '\n';
    } else if (*a == '%') {
      a++;
      x++;
      if (*a == 's') {
        *(d++) = '$';
      } else if (*a == '%') {
        *(d++) = '%';
      } else if (*a == 'n') {
        *(d++) = '#';
      } else {
        *(d++) = *a;
      }
    } else {
      *(d++) = *a;
    }
    a++;
  }
  *d = 0;

  return d - orig_d;
}
