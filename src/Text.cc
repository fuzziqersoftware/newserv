#include "Text.hh"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <vector>

using namespace std;

const iconv_t TextTranscoder::INVALID_IC = (iconv_t)(-1);
const size_t TextTranscoder::FAILURE_RESULT = static_cast<size_t>(-1);

TextTranscoder::TextTranscoder(const char* to, const char* from)
    : ic(iconv_open(to, from)) {
  if (ic == this->INVALID_IC) {
    string error_str = phosg::string_for_error(errno);
    throw runtime_error(phosg::string_printf("failed to initialize %s -> %s text converter: %s", from, to, error_str.c_str()));
  }
}

TextTranscoder::~TextTranscoder() {
  iconv_close(this->ic);
}

TextTranscoder::Result TextTranscoder::operator()(
    void* dest, size_t dest_bytes, const void* src, size_t src_bytes, bool truncate_oversize_result) {
  // Clear any conversion state left over from the previous call
  iconv(this->ic, nullptr, nullptr, nullptr, nullptr);

  void* orig_dest = dest;
  const void* orig_src = src;
  while (src_bytes > 0) {
    size_t src_bytes_before = src_bytes;
    size_t ret = iconv(
        this->ic,
        reinterpret_cast<char**>(const_cast<void**>(&src)),
        &src_bytes,
        reinterpret_cast<char**>(&dest),
        &dest_bytes);

    size_t bytes_read = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(orig_src);
    if (ret == this->FAILURE_RESULT) {
      switch (errno) {
        case EILSEQ: {
          string custom_result = this->on_untranslatable(&src, &src_bytes);
          if (custom_result.empty()) {
            throw runtime_error(phosg::string_printf("untranslatable character at position 0x%zX", bytes_read));
          } else if (custom_result.size() <= dest_bytes) {
            memcpy(dest, custom_result.data(), custom_result.size());
            dest = reinterpret_cast<char*>(dest) + custom_result.size();
            dest_bytes -= custom_result.size();
          } else if (!truncate_oversize_result) {
            throw runtime_error("string does not fit in buffer");
          }
          break;
        }
        case EINVAL:
          throw runtime_error(phosg::string_printf("incomplete multibyte sequence at position 0x%zX", bytes_read));
        case E2BIG:
          if (!truncate_oversize_result) {
            throw runtime_error("string does not fit in buffer");
          } else {
            src_bytes = 0;
            break;
          }
        default:
          throw runtime_error("transcoding failed: " + phosg::string_for_error(errno));
      }
    } else if (src_bytes_before == src_bytes) {
      throw runtime_error("could not transcode any characters");
    }
  }

  size_t bytes_read = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(orig_src);
  size_t bytes_written = reinterpret_cast<char*>(dest) - reinterpret_cast<char*>(orig_dest);
  return Result{
      .bytes_read = bytes_read,
      .bytes_written = bytes_written,
  };
}

string TextTranscoder::operator()(const void* src, size_t src_bytes) {
  // Clear any conversion state left over from the previous call
  iconv(this->ic, nullptr, nullptr, nullptr, nullptr);

  const void* orig_src = src;
  deque<string> blocks;
  while (src_bytes > 0) {
    // Assume 2x input size on average, but always allocate at least 8 bytes
    string& block = blocks.emplace_back(max<size_t>((src_bytes << 1), 8), '\0');
    char* dest = block.data();
    size_t dest_size = block.size();
    size_t src_bytes_before = src_bytes;
    size_t ret = iconv(
        this->ic,
        reinterpret_cast<char**>(const_cast<void**>(&src)),
        &src_bytes,
        reinterpret_cast<char**>(&dest),
        &dest_size);
    block.resize(block.size() - dest_size);

    size_t bytes_read = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(orig_src);
    if (ret == this->FAILURE_RESULT) {
      switch (errno) {
        case EILSEQ: {
          string custom_result = this->on_untranslatable(&src, &src_bytes);
          if (custom_result.empty()) {
            throw runtime_error(phosg::string_printf("untranslatable character at position %zu", bytes_read));
          }
          blocks.emplace_back(std::move(custom_result));
          break;
        }
        case EINVAL:
          throw runtime_error(phosg::string_printf("incomplete multibyte sequence at position %zu", bytes_read));
        case E2BIG:
          break;
        default:
          throw runtime_error("transcoding failed: " + phosg::string_for_error(errno));
      }
    } else if (src_bytes_before == src_bytes) {
      throw runtime_error("could not transcode any characters");
    }
  }

  return phosg::join(blocks, "");
}

string TextTranscoder::operator()(const string& data) {
  return this->operator()(data.data(), data.size());
}

std::string TextTranscoder::on_untranslatable(const void**, size_t*) const {
  return "";
}

TextTranscoderCustomSJISToUTF8::TextTranscoderCustomSJISToUTF8() : TextTranscoder("UTF-8", "SHIFT_JIS") {}

std::string encode_utf8_char(uint32_t ch) {
  string ret;
  if (ch < 0x80) {
    ret.push_back(ch);
  } else if (ch < 0x800) {
    ret.push_back(0xC0 | (ch >> 6));
    ret.push_back(0x80 | (ch & 0x3F));
  } else if (ch < 0x10000) {
    ret.push_back(0xE0 | (ch >> 12));
    ret.push_back(0x80 | ((ch >> 6) & 0x3F));
    ret.push_back(0x80 | (ch & 0x3F));
  } else if (ch < 0x110000) {
    ret.push_back(0xF0 | (ch >> 18));
    ret.push_back(0x80 | ((ch >> 12) & 0x3F));
    ret.push_back(0x80 | ((ch >> 6) & 0x3F));
    ret.push_back(0x80 | (ch & 0x3F));
  } else {
    throw runtime_error("unencodable Unicode code point");
  }
  return ret;
}

uint32_t decode_utf8_char(const void** vdata, size_t* size) {
  if (*size == 0) {
    throw runtime_error("incomplete UTF-8 character");
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(*vdata);
  if (!(data[0] & 0x80)) {
    (*size)--;
    *vdata = data + 1;
    return *data;
  } else if ((data[0] & 0xE0) == 0xC0) {
    if ((*size < 2) || ((data[1] & 0xC0) != 0x80)) {
      throw runtime_error("incomplete UTF-8 character");
    }
    (*size) -= 2;
    *vdata = data + 2;
    return ((data[0] & 0x1F) << 6) | (data[1] & 0x3F);
  } else if ((data[0] & 0xF0) == 0xE0) {
    if ((*size < 3) || ((data[1] & 0xC0) != 0x80) || ((data[2] & 0xC0) != 0x80)) {
      throw runtime_error("incomplete UTF-8 character");
    }
    (*size) -= 3;
    *vdata = data + 3;
    return ((data[0] & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
  } else if ((data[0] & 0xF8) == 0xF0) {
    if ((*size < 4) || ((data[1] & 0xC0) != 0x80) || ((data[2] & 0xC0) != 0x80) || ((data[3] & 0xC0) != 0x80)) {
      throw runtime_error("incomplete UTF-8 character");
    }
    (*size) -= 4;
    *vdata = data + 4;
    return ((data[0] & 0x07) << 18) | ((data[1] & 0x3F) << 12) | ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
  } else {
    throw runtime_error("invalid UTF-8 character");
  }
}

std::string TextTranscoderCustomSJISToUTF8::on_untranslatable(const void** vsrc, size_t* size) const {
  // Sega implemented some nonstandard Shift-JIS characters on PSO GC (and
  // probably XB as well): the heart symbol, encoded as F040, and the PSO font,
  // encoded as F041-F064. Understandably, libiconv doesn't know what to do
  // with these because they're not actually part of Shift-JIS, so we have to
  // handle them manually here. We convert them to actual UTF-8 symbols:
  // F040 (heart symbol) -> U+2665 (heart suit symbol)
  // F041 (PSO font number 0) -> 24EA (circled digit zero)
  // F042-F04A (PSO font numbers 1-9) -> 2460-2468 (circled digits 1-9)
  // F04B-F064 (PSO font letters) -> 1D4D0-1D4E9 (script letters A-Z)

  const uint8_t* src = reinterpret_cast<const uint8_t*>(*vsrc);
  if ((*size < 2) || (src[0] != 0xF0)) {
    return "";
  }

  string ret;
  if (src[1] < 0x40) {
    return "";
  } else if (src[1] == 0x40) { // F040 -> U+2665
    ret = encode_utf8_char(0x2665);
  } else if (src[1] == 0x41) { // F041 -> U+24EA
    ret = encode_utf8_char(0x24EA);
  } else if (src[1] <= 0x4A) { // F042-F04A -> U+2460-U+2468
    ret = encode_utf8_char(0x2460 + (src[1] - 0x42));
  } else if (src[1] <= 0x64) { // F04B-F064 -> U+1D4D0-U+1D4E9
    ret = encode_utf8_char(0x1D4D0 + (src[1] - 0x4B));
  } else {
    return "";
  }

  *vsrc = src + 2;
  (*size) -= 2;
  return ret;
}

TextTranscoderUTF8ToCustomSJIS::TextTranscoderUTF8ToCustomSJIS() : TextTranscoder("SHIFT_JIS", "UTF-8") {}

std::string TextTranscoderUTF8ToCustomSJIS::on_untranslatable(const void** src, size_t* size) const {
  const void* orig_src = *src;
  size_t orig_size = *size;
  uint32_t ch;
  try {
    ch = decode_utf8_char(src, size);
  } catch (const runtime_error&) {
    return "";
  }

  if (ch == 0x2665) { // U+2665 -> F040
    return "\xF0\x40";
  } else if (ch == 0x24EA) { // U+24EA -> F041
    return "\xF0\x41";
  } else if (ch >= 0x2460 && ch <= 0x2468) { // U+2460-U+2468 -> F042-F04A
    string ret("\xF0");
    ret.push_back(0x42 + (ch - 0x2460));
    return ret;
  } else if (ch >= 0x1D4D0 && ch <= 0x1D4E9) { // U+1D4D0-U+1D4E9 -> F04B-F064
    string ret("\xF0");
    ret.push_back(0x4B + (ch - 0x1D4D0));
    return ret;
  } else {
    *src = orig_src;
    *size = orig_size;
    return "";
  }
}

TextTranscoder tt_8859_to_utf8("UTF-8", "ISO-8859-1");
TextTranscoder tt_utf8_to_8859("ISO-8859-1", "UTF-8");
TextTranscoder tt_standard_sjis_to_utf8("UTF-8", "SHIFT_JIS");
TextTranscoder tt_utf8_to_standard_sjis("SHIFT_JIS", "UTF-8");
TextTranscoderCustomSJISToUTF8 tt_sega_sjis_to_utf8;
TextTranscoderUTF8ToCustomSJIS tt_utf8_to_sega_sjis;
TextTranscoder tt_utf16_to_utf8("UTF-8", "UTF-16LE");
TextTranscoder tt_utf8_to_utf16("UTF-16LE", "UTF-8");
TextTranscoder tt_ascii_to_utf8("UTF-8", "ASCII");
TextTranscoder tt_utf8_to_ascii("ASCII", "UTF-8");

string tt_encode_marked_optional(const string& utf8, uint8_t default_language, bool is_utf16) {
  if (is_utf16) {
    return tt_utf8_to_utf16(utf8);
  } else {
    if (default_language) {
      try {
        return tt_utf8_to_8859(utf8);
      } catch (const exception& e) {
        return "\tJ" + tt_utf8_to_sega_sjis(utf8);
      }
    } else {
      try {
        return tt_utf8_to_sega_sjis(utf8);
      } catch (const exception& e) {
        return "\tE" + tt_utf8_to_8859(utf8);
      }
    }
  }
}

string tt_encode_marked(const string& utf8, uint8_t default_language, bool is_utf16) {
  if (is_utf16) {
    string to_encode = "\t";
    to_encode += marker_for_language_code(default_language);
    to_encode += utf8;
    return tt_utf8_to_utf16(to_encode);
  } else {
    if (default_language) {
      try {
        return "\tE" + tt_utf8_to_8859(utf8);
      } catch (const exception& e) {
        return "\tJ" + tt_utf8_to_sega_sjis(utf8);
      }
    } else {
      try {
        return "\tJ" + tt_utf8_to_sega_sjis(utf8);
      } catch (const exception& e) {
        return "\tE" + tt_utf8_to_8859(utf8);
      }
    }
  }
}

string tt_decode_marked(const string& data, uint8_t default_language, bool is_utf16) {
  if (is_utf16) {
    string ret = tt_utf16_to_utf8(data);
    if (ret.size() >= 2 && ret[0] == '\t' && is_language_marker_utf16(ret[1])) {
      ret = ret.substr(2);
    }
    return ret;
  } else {
    if (data.size() >= 2 && data[0] == '\t') {
      if (data[1] == 'J') {
        return tt_sega_sjis_to_utf8(data.substr(2));
      } else if (data[1] == 'E') {
        return tt_8859_to_utf8(data.substr(2));
      }
    }
    return default_language ? tt_8859_to_utf8(data) : tt_sega_sjis_to_utf8(data);
  }
}

string add_language_marker(const string& s, char marker) {
  if ((s.size() >= 2) && (s[0] == '\t') && (s[1] != 'C')) {
    return s;
  }

  string ret;
  ret.push_back('\t');
  ret.push_back(marker);
  ret += s;
  return ret;
}

string remove_language_marker(const string& s) {
  if ((s.size() < 2) || (s[0] != '\t') || (s[1] == 'C')) {
    return s;
  }
  return s.substr(2);
}

void replace_char_inplace(char* a, char f, char r) {
  while (*a) {
    if (*a == f) {
      *a = r;
    }
    a++;
  }
}

size_t add_color_inplace(char* a, size_t max_chars) {
  char* d = a;
  char* orig_d = d;

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

void add_color_inplace(string& s) {
  s.resize(add_color_inplace(s.data(), s.size()));
}

void add_color(phosg::StringWriter& w, const char* src, size_t max_input_chars) {
  for (size_t x = 0; (x < max_input_chars) && *src; x++) {
    if (*src == '$') {
      w.put<char>('\t');
    } else if (*src == '#') {
      w.put<char>('\n');
    } else if (*src == '%') {
      src++;
      x++;
      if (*src == 's') {
        w.put<char>('$');
      } else if (*src == '%') {
        w.put<char>('%');
      } else if (*src == 'n') {
        w.put<char>('#');
      } else if (*src == '\0') {
        break;
      } else {
        w.put<char>(*src);
      }
    } else {
      w.put<char>(*src);
    }
    src++;
  }
}

string add_color(const string& s) {
  phosg::StringWriter w;
  add_color(w, s.data(), s.size());
  return std::move(w.str());
}

void remove_color(phosg::StringWriter& w, const char* src, size_t max_input_chars) {
  for (size_t x = 0; (x < max_input_chars) && *src; x++) {
    if (*src == '$') {
      w.put<char>('%');
      w.put<char>('s');
    } else if (*src == '%') {
      w.put<char>('%');
      w.put<char>('%');
    } else if (*src == '#') {
      w.put<char>('%');
      w.put<char>('n');
    } else if (*src == '\t') {
      w.put<char>('$');
    } else if (*src == '\n') {
      w.put<char>('#');
    } else {
      w.put<char>(*src);
    }
    src++;
  }
  w.put<char>(0);
}

string remove_color(const string& s) {
  phosg::StringWriter w;
  remove_color(w, s.data(), s.size());
  return std::move(w.str());
}

string strip_color(const string& s) {
  string ret;
  for (size_t r = 0; r < s.size(); r++) {
    if ((s[r] == '$' || s[r] == '\t') &&
        (s[r + 1] == 'C') && (((s[r + 2] >= '0') && (s[r + 2] <= '9')) || (s[r + 2] == 'G') || (s[r + 2] == 'a'))) {
      r += 2;
    } else {
      ret.push_back(s[r]);
    }
  }
  return ret;
}

string escape_player_name(const string& name) {
  if (name.size() > 2 && name[0] == '\t' && name[1] != 'C') {
    return remove_color(name.substr(2));
  } else {
    return remove_color(name);
  }
}

char marker_for_language_code(uint8_t language_code) {
  switch (language_code) {
    case 0:
      return 'J';
    case 1:
    case 2:
    case 3:
    case 4:
      return 'E';
    case 5:
      return 'B';
    case 6:
      return 'T';
    case 7:
      return 'K';
    default:
      return 'E';
  }
}

bool is_language_marker_sjis_8859(char marker) {
  return (marker == 'J' || marker == 'E');
}

bool is_language_marker_utf16(char marker) {
  return (marker == 'J' || marker == 'E' || marker == 'B' || marker == 'T' || marker == 'K');
}
