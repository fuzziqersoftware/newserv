#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <stdexcept>
#include <string>

// (1a) Conversion functions

// These return the number of characters written, including the terminating null
// character. In the case of encode_sjis, two-byte characters count as two
// characters, so the returned number is the number of bytes written.
// allow_skip_terminator means no null byte will be written if dest_count
// characters are written to the output. If this argument is false, a null
// terminator is always written, even if the string is truncated.
size_t encode_sjis(
    char* dest, size_t dest_count,
    const char16_t* src, size_t src_count,
    bool allow_skip_terminator = false);
size_t decode_sjis(
    char16_t* dest, size_t dest_count,
    const char* src, size_t src_count,
    bool allow_skip_terminator = false);

std::string encode_sjis(const char16_t* source, size_t src_count);
std::u16string decode_sjis(const char* source, size_t src_count);

inline std::string encode_sjis(const std::u16string& s) {
  return encode_sjis(s.data(), s.size());
}

inline std::u16string decode_sjis(const std::string& s) {
  return decode_sjis(s.data(), s.size());
}

// These functions exist so that decode_sjis and encode_sjis can be
// indiscriminately used within templates that use different char types.
inline const std::string& encode_sjis(const std::string& s) { return s; }
inline const std::u16string& decode_sjis(const std::u16string& s) { return s; }

// (1b) Type-independent utility functions

template <typename T>
size_t text_strlen_t(const T* s) {
  size_t ret = 0;
  for (; s[ret] != 0; ret++) {
  }
  return ret;
}

template <typename T>
size_t text_strnlen_t(const T* s, size_t count) {
  size_t ret = 0;
  for (; (ret < count) && (s[ret] != 0); ret++) {
  }
  return ret;
}

template <typename T>
size_t text_streq_t(const T* a, const T* b) {
  for (;;) {
    if (*a != *b) {
      return false;
    }
    if (*a == 0) {
      return true;
    }
    a++;
    b++;
  }
}

template <typename T>
size_t text_strneq_t(const T* a, const T* b, size_t count) {
  for (; count; count--) {
    if (*a != *b) {
      return false;
    }
    if (*a == 0) {
      return true;
    }
    a++;
    b++;
  }
  return true;
}

template <typename T>
size_t text_strncpy_t(T* dest, const T* src, size_t count) {
  size_t x;
  for (x = 0; x < count && src[x] != 0; x++) {
    dest[x] = src[x];
  }
  if (x < count) {
    dest[x++] = 0;
  }
  return x;
}

// Like strncpy, but *always* null-terminates the string, even if it has to
// truncate it.
template <typename T>
size_t text_strnzcpy_t(T* dest, const T* src, size_t count) {
  size_t x;
  for (x = 0; x < count - 1 && src[x] != 0; x++) {
    dest[x] = src[x];
  }
  dest[x++] = 0;
  return x;
}

// (2) Type conversion functions

template <typename DestT, typename SrcT = DestT>
size_t text_strncpy_t(DestT*, size_t, const SrcT*, size_t) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized text_strncpy_t should never be called");
  return 0;
}

template <>
inline size_t text_strncpy_t<char>(
    char* dest, size_t dest_count, const char* src, size_t src_count) {
  size_t count = std::min<size_t>(dest_count, src_count);
  return text_strncpy_t(dest, src, count);
}

template <>
inline size_t text_strncpy_t<char, char16_t>(
    char* dest, size_t dest_count, const char16_t* src, size_t src_count) {
  return encode_sjis(dest, dest_count, src, src_count, true);
}

template <>
inline size_t text_strncpy_t<char16_t, char>(
    char16_t* dest, size_t dest_count, const char* src, size_t src_count) {
  return decode_sjis(dest, dest_count, src, src_count, true);
}

template <>
inline size_t text_strncpy_t<char16_t>(
    char16_t* dest, size_t dest_count, const char16_t* src, size_t src_count) {
  size_t count = std::min<size_t>(dest_count, src_count);
  return text_strncpy_t(dest, src, count);
}

template <typename DestT, typename SrcT = DestT>
size_t text_strnzcpy_t(DestT*, size_t, const SrcT*, size_t) {
  static_assert(always_false<DestT, SrcT>::v,
      "unspecialized text_strnzcpy_t should never be called");
  return 0;
}

template <>
inline size_t text_strnzcpy_t<char>(
    char* dest, size_t dest_count, const char* src, size_t src_count) {
  size_t count = std::min<size_t>(dest_count, src_count);
  return text_strnzcpy_t(dest, src, count);
}

template <>
inline size_t text_strnzcpy_t<char, char16_t>(
    char* dest, size_t dest_count, const char16_t* src, size_t src_count) {
  return encode_sjis(dest, dest_count, src, src_count);
}

template <>
inline size_t text_strnzcpy_t<char16_t, char>(
    char16_t* dest, size_t dest_count, const char* src, size_t src_count) {
  return decode_sjis(dest, dest_count, src, src_count);
}

template <>
inline size_t text_strnzcpy_t<char16_t>(
    char16_t* dest, size_t dest_count, const char16_t* src, size_t src_count) {
  size_t count = std::min<size_t>(dest_count, src_count);
  return text_strnzcpy_t(dest, src, count);
}

// (3) Packed text objects for use in protocol structs

template <typename ItemT, size_t Count>
struct parray {
  ItemT items[Count];

  parray(ItemT v) {
    this->clear(v);
  }
  template <typename ArgT = ItemT, std::enable_if_t<std::is_arithmetic<ArgT>::value, bool> = true>
  parray() {
    this->clear(0);
  }
  template <typename ArgT = ItemT, std::enable_if_t<std::is_pointer<ArgT>::value, bool> = true>
  parray() {
    this->clear(nullptr);
  }
  template <typename ArgT = ItemT, std::enable_if_t<!std::is_arithmetic<ArgT>::value && !std::is_pointer<ArgT>::value, bool> = true>
  parray() {}

  parray(const parray& other) {
    this->operator=(other);
  }
  parray(parray&& s) = delete;

  template <size_t OtherCount>
  parray(const parray<ItemT, OtherCount>& s) {
    this->operator=(s);
  }

  constexpr static size_t size() {
    return Count;
  }
  constexpr static size_t bytes() {
    return Count * sizeof(ItemT);
  }
  ItemT* data() {
    return this->items;
  }
  const ItemT* data() const {
    return this->items;
  }

  ItemT& operator[](size_t index) {
    if (index >= Count) {
      throw std::out_of_range("array index out of bounds");
    }
    // Note: This looks really dumb, but apparently works around an issue in GCC
    // that causes a "returning address of temporary" error here.
    return *&this->items[index];
  }
  const ItemT& operator[](size_t index) const {
    if (index >= Count) {
      throw std::out_of_range("array index out of bounds");
    }
    return *&this->items[index];
  }

  ItemT& at(size_t index) {
    return this->operator[](index);
  }
  const ItemT& at(size_t index) const {
    return this->operator[](index);
  }

  ItemT* sub_ptr(size_t offset = 0, size_t count = Count) {
    if (offset + count > Count) {
      throw std::out_of_range("sub-array out of range");
    }
    return &this->items[offset];
  }
  const ItemT* sub_ptr(size_t offset = 0, size_t count = Count) const {
    if (offset + count > Count) {
      throw std::out_of_range("sub-array out of range");
    }
    return &this->items[offset];
  }

  template <size_t SubCount>
  parray<ItemT, SubCount>& sub(size_t offset = 0) {
    if (offset + SubCount > Count) {
      throw std::out_of_range("sub-array out of range");
    }
    return *reinterpret_cast<parray<ItemT, SubCount>*>(&this->items[offset]);
  }
  template <size_t SubCount>
  const parray<ItemT, SubCount>& sub(size_t offset = 0) const {
    if (offset + SubCount > Count) {
      throw std::out_of_range("sub-array out of range");
    }
    return *reinterpret_cast<const parray<ItemT, SubCount>*>(&this->items[offset]);
  }

  void assign_range(const ItemT* new_items, size_t count = Count, size_t start_offset = 0) {
    for (size_t x = start_offset; (x < Count) && (x < start_offset + count); x++) {
      this->items[x] = new_items[x];
    }
  }

  parray& operator=(const parray& s) {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = s.items[x];
    }
    return *this;
  }
  parray& operator=(parray&& s) = delete;

  template <size_t OtherCount>
  parray& operator=(const parray<ItemT, OtherCount>& s) {
    if (OtherCount <= Count) {
      size_t x;
      for (x = 0; x < OtherCount; x++) {
        this->items[x] = s.items[x];
      }
      for (; x < Count; x++) {
        this->items[x] = 0;
      }
    } else {
      for (size_t x = 0; x < Count; x++) {
        this->items[x] = s.items[x];
      }
    }
    return *this;
  }

  parray& operator=(const ItemT* s) {
    if (!s) {
      throw std::logic_error("attempted to assign nullptr to parray");
    }
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = s[x];
    }
    return *this;
  }

  bool operator==(const parray& s) const {
    for (size_t x = 0; x < Count; x++) {
      if (this->items[x] != s.items[x]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const parray& s) const {
    return !this->operator==(s);
  }

  void clear(ItemT v) {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = v;
    }
  }
  void clear() {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = ItemT();
    }
  }
  void clear_after(size_t position, ItemT v = 0) {
    for (size_t x = position; x < Count; x++) {
      this->items[x] = v;
    }
  }

  bool is_filled_with(ItemT v) const {
    for (size_t x = 0; x < Count; x++) {
      if (this->items[x] != v) {
        return false;
      }
    }
    return true;
  }
} __attribute__((packed));

// TODO: It appears that these actually do not have to be null-terminated in PSO
// commands some of the time. As an example, creating a game with a name with
// the maximum length results in a C1 command with no null byte between the game
// name and the password. We should be able to handle this by making ptexts not
// required to be null-terminated in storage - this will still be safe if we
// limit all operations by Count.
template <typename CharT, size_t Count>
struct ptext : parray<CharT, Count> {
  ptext() {
    this->clear(0);
  }
  ptext(const ptext& other) : parray<CharT, Count>(other) {}
  ptext(ptext&& s) = delete;

  template <typename OtherCharT>
  ptext(const OtherCharT* s) {
    if (!s) {
      throw std::logic_error("attempted to assign nullptr to ptext");
    }
    this->operator=(s);
  }
  template <typename OtherCharT>
  ptext(const OtherCharT* s, size_t count) {
    if (!s) {
      throw std::logic_error("attempted to assign nullptr to ptext");
    }
    this->assign(s, count);
  }
  template <typename OtherCharT>
  ptext(const std::basic_string<OtherCharT>& s) {
    this->operator=(s);
  }
  template <typename OtherCharT, size_t OtherCount>
  ptext(const ptext<OtherCharT, OtherCount>& s) {
    this->operator=(s);
  }

  size_t len() const {
    return text_strnlen_t(this->items, Count);
  }

  // Q: Why is there no c_str() here?
  // A: Because the contents of a ptext don't have to be null-terminated.

  ptext& operator=(const ptext& s) {
    memcpy(this->items, s.items, sizeof(CharT) * Count);
    return *this;
  }
  ptext& operator=(ptext&& s) = delete;

  template <typename OtherCharT>
  ptext& operator=(const OtherCharT* s) {
    if (!s) {
      throw std::logic_error("attempted to assign nullptr to ptext");
    }
    size_t chars_written = text_strncpy_t(this->items, Count, s, Count);
    this->clear_after(chars_written);
    return *this;
  }
  template <typename OtherCharT>
  ptext& assign(const OtherCharT* s, size_t s_count) {
    if (!s) {
      throw std::logic_error("attempted to assign nullptr to ptext");
    }
    size_t chars_written = text_strncpy_t(this->items, Count, s, s_count);
    this->clear_after(chars_written);
    return *this;
  }
  template <typename OtherCharT>
  ptext& operator=(const std::basic_string<OtherCharT>& s) {
    size_t chars_written = text_strncpy_t(this->items, Count, s.c_str(), s.size());
    this->clear_after(chars_written);
    return *this;
  }
  template <typename OtherCharT, size_t OtherCount>
  ptext& operator=(const ptext<OtherCharT, OtherCount>& s) {
    size_t chars_written = text_strncpy_t(this->items, Count, s.items, OtherCount);
    this->clear_after(chars_written);
    return *this;
  }

  template <typename OtherCharT>
  bool operator==(const OtherCharT* s) const {
    if (!s) {
      throw std::logic_error("attempted to compare ptext to nullptr");
    }
    return text_strneq_t(this->items, s, Count);
  }
  template <typename OtherCharT>
  bool operator==(const std::basic_string<OtherCharT>& s) const {
    return text_strneq_t(this->items, s.c_str(), Count);
  }
  template <typename OtherCharT, size_t OtherCount>
  bool operator==(const ptext<OtherCharT, OtherCount>& s) const {
    return text_strneq_t(this->items, s.items, std::min<size_t>(Count, OtherCount));
  }
  template <typename OtherCharT>
  bool operator!=(const OtherCharT* s) const {
    if (!s) {
      throw std::logic_error("attempted to compare ptext to nullptr");
    }
    return !this->operator==(s);
  }
  template <typename OtherCharT>
  bool operator!=(const std::basic_string<OtherCharT>& s) const {
    return !this->operator==(s);
  }
  template <typename OtherCharT, size_t OtherCount>
  bool operator!=(const ptext<OtherCharT, OtherCount>& s) const {
    return !this->operator==(s);
  }

  template <typename OtherCharT>
  bool eq_n(const OtherCharT* s, size_t count) const {
    if (!s) {
      throw std::logic_error("attempted to compare ptext to nullptr");
    }
    return text_strneq_t(this->items, s, count);
  }
  template <typename OtherCharT>
  bool eq_n(const std::basic_string<OtherCharT>& s, size_t count) const {
    return text_strneq_t(this->items, s.c_str(), count);
  }
  template <typename OtherCharT, size_t OtherCount>
  bool eq_n(const ptext<OtherCharT, OtherCount>& s, size_t count) const {
    return text_strneq_t(this->items, s.items, count);
  }

  operator std::basic_string<CharT>() const {
    return std::basic_string<CharT>(this->items, this->len());
  }

  bool empty() const {
    return (this->items[0] == 0);
  }
} __attribute__((packed));

// (4) Markers and character replacement

template <typename CharT>
std::basic_string<CharT> add_language_marker(
    const std::basic_string<CharT>& s, CharT marker) {
  if ((s.size() >= 2) && (s[0] == '\t') && (s[1] != 'C')) {
    return s;
  }

  std::basic_string<CharT> ret;
  ret.push_back('\t');
  ret.push_back(marker);
  ret += s;
  return ret;
}

template <typename CharT, size_t Count>
std::basic_string<CharT> add_language_marker(
    const ptext<CharT, Count>& s, CharT marker) {
  if ((s.items[0] == '\t') && (s.items[1] != 'C')) {
    return s;
  }

  std::basic_string<CharT> ret;
  ret.push_back('\t');
  ret.push_back(marker);
  ret += s;
  return ret;
}

template <typename CharT>
const CharT* remove_language_marker(const CharT* s) {
  if ((s[0] != '\t') || (s[1] == 'C')) {
    return s;
  }
  return s + 2;
}

template <typename CharT, size_t Count>
std::basic_string<CharT> remove_language_marker(const ptext<CharT, Count>& s) {
  if ((s.items[0] != '\t') || (s.items[1] == L'C')) {
    return s;
  }
  return &s.items[2];
}

template <typename CharT>
std::basic_string<CharT> remove_language_marker(
    const std::basic_string<CharT>& s) {
  if ((s.size() < 2) || (s[0] != L'\t') || (s[1] == L'C')) {
    return s;
  }
  return s.substr(2);
}

template <typename CharT, size_t Count>
void remove_language_marker_inplace(ptext<CharT, Count>& a) {
  if ((a.items[0] == '\t') && (a.items[1] != 'C')) {
    text_strnzcpy_t(a.items, Count, &a.items[2], Count);
    a.items[text_strlen_t(a.items) + 1] = 0;
  }
}

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
  // TODO: we should clear the chars after the null if the new string is shorter
  // than the original

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

template <typename CharT, size_t Count>
void add_color_inplace(ptext<CharT, Count>& t) {
  size_t sx = 0;
  size_t dx = 0;
  for (; (sx < Count - 1) && t.items[sx]; sx++) {
    if (t.items[sx] == '$') {
      t.items[dx] = '\t';
    } else if (t.items[sx] == '#') {
      t.items[dx] = '\n';
    } else if (t.items[sx] == '%') {
      sx++;
      if ((sx == Count - 1) || (t.items[sx] == '\0')) {
        break;
      } else if (t.items[sx] == 's') {
        t.items[dx] = '$';
      } else if (t.items[sx] == '%') {
        t.items[dx] = '%';
      } else if (t.items[sx] == 'n') {
        t.items[dx] = '#';
      } else {
        t.items[dx] = t.items[sx];
      }
    } else {
      t.items[dx] = t.items[sx];
    }
    dx++;
  }
  for (; dx < Count; dx++) {
    t.items[dx] = 0;
  }
}
