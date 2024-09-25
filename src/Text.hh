#pragma once

#include <iconv.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <initializer_list>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <stdexcept>
#include <string>

#include "Types.hh"

#define __packed__ __attribute__((packed))
#define check_struct_size(StructT, Size)                                 \
  static_assert(sizeof(StructT) >= Size, "Structure size is too small"); \
  static_assert(sizeof(StructT) <= Size, "Structure size is too large")

#define __packed_ws__(StructT, Size) \
  __packed__;                        \
  check_struct_size(StructT, Size)

// Conversion functions

std::string encode_utf8_char(uint32_t ch);
uint32_t decode_utf8_char(const void** data, size_t* size);

class TextTranscoder {
public:
  TextTranscoder(const char* to, const char* from);
  TextTranscoder(const TextTranscoder&) = delete;
  TextTranscoder(TextTranscoder&&) = delete;
  TextTranscoder& operator=(const TextTranscoder&) = delete;
  TextTranscoder& operator=(TextTranscoder&&) = delete;
  virtual ~TextTranscoder();

  struct Result {
    size_t bytes_read;
    size_t bytes_written;
  };
  Result operator()(void* dest, size_t dest_bytes, const void* src, size_t src_bytes, bool truncate_oversize_result);

  std::string operator()(const void* src, size_t src_bytes);
  std::string operator()(const std::string& data);

protected:
  virtual std::string on_untranslatable(const void** src, size_t* size) const;

  static const iconv_t INVALID_IC;
  static const size_t FAILURE_RESULT;
  iconv_t ic;
};

class TextTranscoderCustomSJISToUTF8 : public TextTranscoder {
public:
  TextTranscoderCustomSJISToUTF8();
  virtual ~TextTranscoderCustomSJISToUTF8() = default;

protected:
  virtual std::string on_untranslatable(const void** src, size_t* size) const;
};

class TextTranscoderUTF8ToCustomSJIS : public TextTranscoder {
public:
  TextTranscoderUTF8ToCustomSJIS();
  virtual ~TextTranscoderUTF8ToCustomSJIS() = default;

protected:
  virtual std::string on_untranslatable(const void** src, size_t* size) const;
};

extern TextTranscoder tt_8859_to_utf8;
extern TextTranscoder tt_utf8_to_8859;
extern TextTranscoder tt_standard_sjis_to_utf8;
extern TextTranscoder tt_utf8_to_standard_sjis;
extern TextTranscoderCustomSJISToUTF8 tt_sega_sjis_to_utf8;
extern TextTranscoderUTF8ToCustomSJIS tt_utf8_to_sega_sjis;
extern TextTranscoder tt_utf16_to_utf8;
extern TextTranscoder tt_utf8_to_utf16;
extern TextTranscoder tt_ascii_to_utf8;
extern TextTranscoder tt_utf8_to_ascii;

std::string tt_encode_marked_optional(const std::string& utf8, uint8_t default_language, bool is_utf16);
std::string tt_encode_marked(const std::string& utf8, uint8_t default_language, bool is_utf16);
std::string tt_decode_marked(const std::string& data, uint8_t default_language, bool is_utf16);

char marker_for_language_code(uint8_t language_code);
bool is_language_marker_sjis_8859(char marker);
bool is_language_marker_utf16(char marker);

// Packed array object for use in protocol structs

template <typename ItemT, size_t Count>
struct parray {
  ItemT items[Count];

  parray(ItemT v) {
    this->clear(v);
  }
  parray(std::initializer_list<ItemT> init_items) {
    for (size_t z = 0; z < init_items.size(); z++) {
      this->items[z] = std::data(init_items)[z];
    }
    this->clear_after(init_items.size());
  }
  template <typename ArgT = ItemT>
    requires(std::is_arithmetic_v<ArgT> || phosg::is_converted_endian_sc_v<ArgT>)
  parray() {
    this->clear(0);
  }
  template <typename ArgT = ItemT>
    requires std::is_pointer_v<ArgT>
  parray() {
    this->clear(nullptr);
  }
  template <typename ArgT = ItemT>
    requires(!std::is_arithmetic_v<ArgT> && !std::is_pointer_v<ArgT> && !phosg::is_converted_endian_sc_v<ArgT>)
  parray() {}

  parray(const parray& other) {
    this->operator=(other);
  }
  parray(parray&& other) {
    this->operator=(std::move(other));
  }

  template <typename FromT>
    requires std::is_convertible_v<FromT, ItemT>
  parray(const parray<FromT, Count>& other) {
    this->operator=(other);
  }

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

  std::string as_string() const {
    return std::string(reinterpret_cast<const char*>(this->data()), sizeof(ItemT) * Count);
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
  parray& operator=(parray&& s) {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = s.items[x];
    }
    return *this;
  }

  template <typename FromT>
    requires std::is_convertible_v<FromT, ItemT>
  parray& operator=(const parray<FromT, Count>& s) {
    for (size_t x = 0; x < Count; x++) {
      const FromT& src_item = s.items[x];
      ItemT& dest_item = this->items[x];
      static_assert(!std::is_const_v<ItemT>, "ItemT is const");
      dest_item = src_item;
    }
    return *this;
  }

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
} __packed__;

template <typename ItemT, size_t Count>
struct bcarray {
  ItemT items[Count];

  bcarray(ItemT v) {
    this->clear(v);
  }
  bcarray(std::initializer_list<ItemT> init_items) {
    for (size_t z = 0; z < init_items.size(); z++) {
      this->items[z] = std::data(init_items)[z];
    }
    this->clear_after(init_items.size());
  }
  template <typename ArgT = ItemT>
    requires(std::is_arithmetic_v<ArgT> || phosg::is_converted_endian_sc_v<ArgT>)
  bcarray() {
    this->clear(0);
  }
  template <typename ArgT = ItemT>
    requires std::is_pointer_v<ArgT>
  bcarray() {
    this->clear(nullptr);
  }
  template <typename ArgT = ItemT>
    requires(!std::is_arithmetic_v<ArgT> && !std::is_pointer_v<ArgT> && !phosg::is_converted_endian_sc_v<ArgT>)
  bcarray() {}

  bcarray(const bcarray& other) {
    this->operator=(other);
  }
  bcarray(bcarray&& other) {
    this->operator=(std::move(other));
  }

  constexpr static size_t size() {
    return Count;
  }

  ItemT& operator[](size_t index) {
    if (index >= Count) {
      throw std::out_of_range("array index out of bounds");
    }
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

  bcarray& operator=(const bcarray& s) {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = s.items[x];
    }
    return *this;
  }
  bcarray& operator=(bcarray&& s) {
    for (size_t x = 0; x < Count; x++) {
      this->items[x] = std::move(s.items[x]);
    }
    return *this;
  }

  bool operator==(const bcarray& s) const {
    for (size_t x = 0; x < Count; x++) {
      if (this->items[x] != s.items[x]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const bcarray& s) const {
    return !this->operator==(s);
  }
};

// Packed text objects for use in protocol structs

enum class TextEncoding {
  UTF8,
  UTF16,
  SJIS,
  ISO8859,
  ASCII,
  MARKED,
  UTF16_ALWAYS_MARKED,
  CHALLENGE8, // MARKED but with challenge encryption on top
  CHALLENGE16, // UTF16 but with challenge encryption on top
};

template <typename CharT>
void encrypt_challenge_rank_text_t(void* vdata, size_t count) {
  CharT* data = reinterpret_cast<CharT*>(vdata);
  CharT prev = 0;
  for (CharT* p = data; p != data + count; p++) {
    CharT ch = *p;
    if (ch == 0) {
      break;
    }
    *p = ((ch - prev) ^ 0x7F) & 0xFF;
    prev = ch;
  }
}

template <typename CharT>
void decrypt_challenge_rank_text_t(void* vdata, size_t count) {
  CharT* data = reinterpret_cast<CharT*>(vdata);
  for (CharT* p = data; p != data + count; p++) {
    if (*p == 0) {
      break;
    }
    if (p == data) {
      *p ^= 0x7F;
    } else {
      *p = ((*p ^ 0x7F) + *(p - 1)) & 0xFF;
    }
  }
}

// This struct does not inherit from parray, even though it's semantically
// similar, because we want to enforce that the correct encoding is used.
template <
    TextEncoding Encoding,
    size_t Chars,
    size_t BytesPerChar = (((Encoding == TextEncoding::UTF16) || (Encoding == TextEncoding::UTF16_ALWAYS_MARKED) || (Encoding == TextEncoding::CHALLENGE16)) ? 2 : 1)>
struct pstring {
  static constexpr size_t Bytes = Chars * BytesPerChar;

  static constexpr size_t bytes() {
    return Bytes;
  }
  static constexpr size_t chars() {
    return Chars;
  }

  uint8_t data[Bytes];

  pstring() {
    memset(this->data, 0, Bytes);
  }
  pstring(const pstring<Encoding, Chars, BytesPerChar>& other) {
    memcpy(this->data, other.data, Bytes);
  }
  pstring(const std::string& s, uint8_t language) {
    this->encode(s, language);
  }
  pstring(pstring<Encoding, Chars, BytesPerChar>&& other) = delete;

  pstring<Encoding, Chars, BytesPerChar>& operator=(const pstring<Encoding, Chars, BytesPerChar>& other) {
    memcpy(this->data, other.data, Bytes);
    return *this;
  }
  template <size_t OtherChars>
  pstring<Encoding, Chars, BytesPerChar>& operator=(const pstring<Encoding, OtherChars, BytesPerChar>& other) {
    size_t end_offset = std::min<size_t>(Bytes, pstring<Encoding, OtherChars, BytesPerChar>::Bytes);
    memcpy(this->data, other.data, end_offset);
    this->clear_after_bytes(end_offset);
    return *this;
  }
  pstring<Encoding, Chars, BytesPerChar>& operator=(pstring<Encoding, Chars, BytesPerChar>&& s) = delete;

  void encode(const std::string& s, uint8_t client_language = 1) {
    try {
      switch (Encoding) {
        case TextEncoding::CHALLENGE8:
        case TextEncoding::ASCII: {
          auto ret = tt_utf8_to_ascii(this->data, Bytes, s.data(), s.size(), true);
          this->clear_after_bytes(ret.bytes_written);
          if (Encoding == TextEncoding::CHALLENGE8) {
            encrypt_challenge_rank_text_t<uint8_t>(this->data, Bytes);
          }
          break;
        }
        case TextEncoding::ISO8859: {
          auto ret = tt_utf8_to_8859(this->data, Bytes, s.data(), s.size(), true);
          this->clear_after_bytes(ret.bytes_written);
          break;
        }
        case TextEncoding::SJIS: {
          auto ret = tt_utf8_to_sega_sjis(this->data, Bytes, s.data(), s.size(), true);
          this->clear_after_bytes(ret.bytes_written);
          break;
        }
        case TextEncoding::UTF16_ALWAYS_MARKED:
          if (s.empty()) {
            this->clear();
            break;
          } else if (s.size() <= 2 || s[0] != '\t' || s[1] == 'C') {
            std::string to_encode = "\t";
            to_encode += marker_for_language_code(client_language);
            to_encode += s;
            auto ret = tt_utf8_to_utf16(this->data, Bytes, to_encode.data(), to_encode.size(), true);
            this->clear_after_bytes(ret.bytes_written);
            break;
          }
          [[fallthrough]];
        case TextEncoding::UTF16: {
          auto ret = tt_utf8_to_utf16(this->data, Bytes, s.data(), s.size(), true);
          this->clear_after_bytes(ret.bytes_written);
          break;
        }
        case TextEncoding::UTF8:
          memcpy(this->data, s.data(), std::min<size_t>(s.size(), Bytes));
          this->clear_after_bytes(s.size());
          break;
        case TextEncoding::CHALLENGE16: {
          auto ret = tt_utf8_to_utf16(this->data, Bytes, s.data(), s.size(), true);
          encrypt_challenge_rank_text_t<le_uint16_t>(this->data, ret.bytes_written / 2);
          this->clear_after_bytes(ret.bytes_written);
          break;
        }
        case TextEncoding::MARKED: {
          if (client_language == 0) {
            try {
              auto ret = tt_utf8_to_sega_sjis(this->data, Bytes, s.data(), s.size(), true);
              this->clear_after_bytes(ret.bytes_written);
            } catch (const std::runtime_error&) {
              this->data[0] = '\t';
              this->data[1] = 'E';
              auto ret = tt_utf8_to_8859(this->data + 2, Bytes - 2, s.data(), s.size(), true);
              this->clear_after_bytes(ret.bytes_written + 2);
            }
          } else {
            try {
              auto ret = tt_utf8_to_8859(this->data, Bytes, s.data(), s.size(), true);
              this->clear_after_bytes(ret.bytes_written);
            } catch (const std::runtime_error&) {
              this->data[0] = '\t';
              this->data[1] = 'J';
              auto ret = tt_utf8_to_sega_sjis(this->data + 2, Bytes - 2, s.data(), s.size(), true);
              this->clear_after_bytes(ret.bytes_written + 2);
            }
          }
          break;
        }
        default:
          throw std::logic_error("unknown text encoding");
      }
    } catch (const std::runtime_error& e) {
      phosg::log_warning("Unencodable text: %s", e.what());
      if (BytesPerChar == 2) {
        if (Bytes >= 6) {
          this->data[0] = '<';
          this->data[1] = 0x00;
          this->data[2] = '?';
          this->data[3] = 0x00;
          this->data[4] = '>';
          this->data[5] = 0x00;
          this->clear_after_bytes(6);
        } else if (Bytes >= 2) {
          this->data[0] = '?';
          this->data[1] = 0x00;
          this->clear_after_bytes(2);
        }
      } else {
        if (Bytes >= 3) {
          this->data[0] = '<';
          this->data[1] = '?';
          this->data[2] = '>';
          this->clear_after_bytes(3);
        } else if (Bytes >= 1) {
          this->data[0] = '?';
          this->clear_after_bytes(1);
        }
      }
    }
  }

  std::string decode(uint8_t client_language = 1) const {
    try {
      switch (Encoding) {
        case TextEncoding::CHALLENGE8: {
          std::string decrypted(reinterpret_cast<const char*>(this->data), this->used_chars_8());
          decrypt_challenge_rank_text_t<uint8_t>(decrypted.data(), decrypted.size());
          return tt_ascii_to_utf8(decrypted.data(), decrypted.size());
        }
        case TextEncoding::ASCII:
          return tt_ascii_to_utf8(this->data, this->used_chars_8());
        case TextEncoding::ISO8859:
          return tt_8859_to_utf8(this->data, this->used_chars_8());
        case TextEncoding::SJIS:
          return tt_sega_sjis_to_utf8(this->data, this->used_chars_8());
        case TextEncoding::UTF16:
          return tt_utf16_to_utf8(this->data, this->used_chars_16() * 2);
        case TextEncoding::UTF16_ALWAYS_MARKED: {
          std::string ret = tt_utf16_to_utf8(this->data, this->used_chars_16() * 2);
          return ((ret.size() >= 2) && (ret[0] == '\t') && (ret[1] != 'C')) ? ret.substr(2) : ret;
        }
        case TextEncoding::UTF8:
          return std::string(reinterpret_cast<const char*>(&this->data[0]), this->used_chars_8());
        case TextEncoding::CHALLENGE16: {
          std::string decrypted(reinterpret_cast<const char*>(&this->data[0]), this->used_chars_16() * 2);
          decrypt_challenge_rank_text_t<le_uint16_t>(decrypted.data(), decrypted.size() / 2);
          return tt_utf16_to_utf8(decrypted.data(), decrypted.size());
        }
        case TextEncoding::MARKED: {
          size_t offset = 0;
          if (this->data[0] == '\t') {
            if (this->data[1] == 'J') {
              client_language = 0;
              offset = 2;
            } else if (this->data[1] != 'C') {
              client_language = 1;
              offset = 2;
            }
          }
          return client_language
              ? tt_8859_to_utf8(&this->data[offset], this->used_chars_8() - offset)
              : tt_sega_sjis_to_utf8(&this->data[offset], this->used_chars_8() - offset);
        }
        default:
          throw std::logic_error("unknown text encoding");
      }
    } catch (const std::runtime_error& e) {
      phosg::log_warning("Undecodable text: %s", e.what());
      return "<?>";
    }
  }

  bool operator==(const pstring<Encoding, Chars, BytesPerChar>& other) const {
    return (memcmp(this->data, other.data, Bytes) == 0);
  }
  bool operator!=(const pstring<Encoding, Chars, BytesPerChar>& other) const {
    return (memcmp(this->data, other.data, Bytes) != 0);
  }

  bool eq(const std::string& other, uint8_t language = 1) const {
    return this->decode(language) == other;
  }

  size_t used_chars_8() const {
    size_t size = 0;
    for (size = 0; size < Bytes; size++) {
      if (!this->data[size]) {
        return size;
      }
    }
    return Bytes;
  }

  size_t used_chars_16() const {
    if (Bytes & 1) {
      throw std::logic_error("used_chars_16 must not be called on an odd-length pstring");
    }
    for (size_t z = 0; z < Bytes; z += 2) {
      if (!this->data[z] && !this->data[z + 1]) {
        return z >> 1;
      }
    }
    return Bytes >> 1;
  }

  bool empty() const {
    for (size_t z = 0; z < BytesPerChar; z++) {
      if (this->data[z] != 0) {
        return false;
      }
    }
    return true;
  }

  void clear(uint8_t v = 0) {
    memset(this->data, v, Bytes);
  }

  void clear_after_bytes(size_t pos, uint8_t v = 0) {
    for (; pos < Bytes; pos++) {
      this->data[pos] = v;
    }
  }

  void set_byte(size_t pos, uint8_t v) {
    if (pos >= Bytes) {
      throw std::out_of_range("pstring byte offset out of range");
    }
    this->data[pos] = v;
  }

  void assign_raw(const void* data, size_t size) {
    memcpy(this->data, data, std::min<size_t>(size, Bytes));
    this->clear_after_bytes(size);
  }
  void assign_raw(const std::string& data) {
    this->assign_raw(data.data(), data.size());
  }

  uint8_t at(size_t pos) const {
    if (pos >= Bytes) {
      throw std::out_of_range("pstring index out of range");
    }
    return this->data[pos];
  }

  // Note: The contents of a pstring do not have to be null-terminated, so there
  // is no .c_str() function.
} __packed__;

// Helper functions

void replace_char_inplace(char* a, char f, char r);

void add_color(phosg::StringWriter& w, const char* src, size_t max_input_chars);
std::string add_color(const std::string& s);

size_t add_color_inplace(char* a, size_t max_chars);
void add_color_inplace(std::string& s);

// remove_color does the opposite of add_color (it changes \t into $, for
// example). strip_color is irreversible; it deletes color escape sequences.
void remove_color(phosg::StringWriter& w, const char* src, size_t max_input_chars);
std::string remove_color(const std::string& s);

std::string strip_color(const std::string& s);

std::string escape_player_name(const std::string& name);
