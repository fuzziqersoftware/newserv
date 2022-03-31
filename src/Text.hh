#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <string>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>



#define countof(F) (sizeof(F) / sizeof(F[0]))



int char16ncmp(const char16_t* s1, const char16_t* s2, size_t count);
size_t char16len(const char16_t* s);

void encode_sjis(char* dest, const char16_t* source, size_t dest_count);
void decode_sjis(char16_t* dest, const char* source, size_t dest_count);
std::string encode_sjis(const char16_t* source, size_t src_count);
std::u16string decode_sjis(const char* source, size_t src_count);
std::string encode_sjis(const std::u16string& source);
std::u16string decode_sjis(const std::string& source);



// Like strncpy, but *always* null-terminates the string, even if it has to
// truncate it.
template <typename T>
void strcpy_z(T* dest, const T* src, size_t count) {
  size_t x;
  for (x = 0; x < count - 1 && src[x] != 0; x++) {
    dest[x] = src[x];
  }
  dest[x] = 0;
}



template <typename DestT, typename SrcT = DestT>
void strncpy_t(DestT*, const SrcT*, size_t) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized strcpy_t should never be called");
}

template <>
inline void strncpy_t<char>(char* dest, const char* src, size_t count) {
  strcpy_z(dest, src, count);
}

template <>
inline void strncpy_t<char, char16_t>(char* dest, const char16_t* src, size_t count) {
  encode_sjis(dest, src, count);
}

template <>
inline void strncpy_t<char16_t, char>(char16_t* dest, const char* src, size_t count) {
  decode_sjis(dest, src, count);
}

template <>
inline void strncpy_t<char16_t>(char16_t* dest, const char16_t* src, size_t count) {
  strcpy_z(dest, src, count);
}



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
      } else if (*a == '\0') {
        break;
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

template <typename T>
void add_color(StringWriter& w, const T* src, size_t max_input_chars) {
  for (size_t x = 0; (x < max_input_chars) && *src; x++) {
    if (*src == '$') {
      w.put<T>('\t');
    } else if (*src == '#') {
      w.put<T>('\n');
    } else if (*src == '%') {
      src++;
      x++;
      if (*src == 's') {
        w.put<T>('$');
      } else if (*src == '%') {
        w.put<T>('%');
      } else if (*src == 'n') {
        w.put<T>('#');
      } else if (*src == '\0') {
        break;
      } else {
        w.put<T>(*src);
      }
    } else {
      w.put<T>(*src);
    }
    src++;
  }
  w.put<T>(0);
}
