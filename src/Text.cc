#include "Text.hh"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <vector>

using namespace std;

// A third case is when inbuf is NULL or *inbuf is NULL, and outbuf is NULL or *outbuf is NULL. In this case, the iconv function sets cd’s conversion state to the initial state.

const iconv_t TextTranscoder::INVALID_IC = (iconv_t)(-1);
const size_t TextTranscoder::FAILURE_RESULT = static_cast<size_t>(-1);

TextTranscoder::TextTranscoder(const char* to, const char* from)
    : ic(iconv_open(to, from)) {
  if (ic == this->INVALID_IC) {
    string error_str = string_for_error(errno);
    throw runtime_error(string_printf("failed to initialize %s -> %s text converter: %s", from, to, error_str.c_str()));
  }
}

TextTranscoder::TextTranscoder(TextTranscoder&& other) : ic(other.ic) {
  other.ic = this->INVALID_IC;
}

TextTranscoder& TextTranscoder::operator=(TextTranscoder&& other) {
  this->ic = other.ic;
  other.ic = this->INVALID_IC;
  return *this;
}

TextTranscoder::~TextTranscoder() {
  iconv_close(this->ic);
}

TextTranscoder::Result TextTranscoder::operator()(
    void* dest, size_t dest_size, const void* src, size_t src_bytes, bool truncate_oversize_result) {
  // Clear any conversion state left over from the previous call
  iconv(this->ic, nullptr, nullptr, nullptr, nullptr);

  void* orig_dest = dest;
  const void* orig_src = src;
  size_t ret = iconv(
      this->ic,
      reinterpret_cast<char**>(const_cast<void**>(&src)),
      &src_bytes,
      reinterpret_cast<char**>(&dest),
      &dest_size);

  size_t bytes_read = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(orig_src);
  if (ret == this->FAILURE_RESULT) {
    switch (errno) {
      case EILSEQ:
        throw runtime_error(string_printf("untranslatable character at position 0x%zX", bytes_read));
      case EINVAL:
        throw runtime_error(string_printf("incomplete multibyte sequence at position 0x%zX", bytes_read));
      case E2BIG:
        if (!truncate_oversize_result) {
          throw runtime_error("string does not fit in buffer");
        } else {
          break;
        }
      default:
        throw runtime_error("transcoding failed: " + string_for_error(errno));
    }
  }

  size_t bytes_written = reinterpret_cast<char*>(dest) - reinterpret_cast<char*>(orig_dest);
  return Result{
      .bytes_read = bytes_read,
      .bytes_written = bytes_written,
  };
}

string TextTranscoder::operator()(const void* src, size_t src_size) {
  // Clear any conversion state left over from the previous call
  iconv(this->ic, nullptr, nullptr, nullptr, nullptr);

  const void* orig_src = src;
  deque<string> blocks;
  while (src_size > 0) {
    // Assume 2x input size on average, but always allocate at least 8 bytes
    string& block = blocks.emplace_back(max<size_t>((src_size << 1), 8), '\0');
    char* dest = block.data();
    size_t dest_size = block.size();
    size_t ret = iconv(
        this->ic,
        reinterpret_cast<char**>(const_cast<void**>(&src)),
        &src_size,
        reinterpret_cast<char**>(&dest),
        &dest_size);
    block.resize(block.size() - dest_size);

    size_t bytes_read = reinterpret_cast<const char*>(src) - reinterpret_cast<const char*>(orig_src);
    if (ret == this->FAILURE_RESULT) {
      switch (errno) {
        case EILSEQ:
          throw runtime_error(string_printf("untranslatable character at position %zu", bytes_read));
        case EINVAL:
          throw runtime_error(string_printf("incomplete multibyte sequence at position %zu", bytes_read));
        case E2BIG:
          break;
        default:
          throw runtime_error("transcoding failed: " + string_for_error(errno));
      }
    }
    if ((bytes_read == 0) && (src_size != 0)) {
      throw runtime_error("failed to transcode input data");
    }
  }

  return join(blocks, "");
}

string TextTranscoder::operator()(const string& data) {
  return this->operator()(data.data(), data.size());
}

TextTranscoder tt_8859_to_utf8("UTF-8", "ISO-8859-1");
TextTranscoder tt_utf8_to_8859("ISO-8859-1", "UTF-8");
TextTranscoder tt_sjis_to_utf8("UTF-8", "SHIFT_JIS");
TextTranscoder tt_utf8_to_sjis("SHIFT_JIS", "UTF-8");
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
        return "\tJ" + tt_utf8_to_sjis(utf8);
      }
    } else {
      try {
        return tt_utf8_to_sjis(utf8);
      } catch (const exception& e) {
        return "\tE" + tt_utf8_to_8859(utf8);
      }
    }
  }
}

string tt_encode_marked(const string& utf8, uint8_t default_language, bool is_utf16) {
  if (is_utf16) {
    return tt_utf8_to_utf16((default_language ? "\tE" : "\tJ") + utf8);
  } else {
    if (default_language) {
      try {
        return "\tE" + tt_utf8_to_8859(utf8);
      } catch (const exception& e) {
        return "\tJ" + tt_utf8_to_sjis(utf8);
      }
    } else {
      try {
        return "\tJ" + tt_utf8_to_sjis(utf8);
      } catch (const exception& e) {
        return "\tE" + tt_utf8_to_8859(utf8);
      }
    }
  }
}

string tt_decode_marked(const string& data, uint8_t default_language, bool is_utf16) {
  if (is_utf16) {
    string ret = tt_utf16_to_utf8(data);
    if (ret.size() >= 2 && ret[0] == '\t' && (ret[1] == 'E' || ret[1] == 'J')) {
      ret = ret.substr(2);
    }
    return ret;
  } else {
    if (data.size() >= 2 && data[0] == '\t') {
      if (data[1] == 'J') {
        return tt_sjis_to_utf8(data.substr(2));
      } else if (data[1] == 'E') {
        return tt_8859_to_utf8(data.substr(2));
      }
    }
    return default_language ? tt_8859_to_utf8(data) : tt_sjis_to_utf8(data);
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

void add_color(StringWriter& w, const char* src, size_t max_input_chars) {
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
  w.put<char>(0);
}

string add_color(const string& s) {
  StringWriter w;
  add_color(w, s.data(), s.size());
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
