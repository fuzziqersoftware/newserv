#include "Compression.hh"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;



struct prs_compress_ctx {
  uint8_t bitpos;
  std::string forward_log;
  std::string output;

  prs_compress_ctx() : bitpos(0), forward_log("\0", 1) { }

  string finish() {
    this->put_control_bit(0);
    this->put_control_bit(1);
    this->put_static_data(0);
    this->put_static_data(0);
    this->output += this->forward_log;
    this->forward_log.clear();
    return this->output;
  }

  void put_control_bit_nosave(bool bit) {
    if (bit) {
      this->forward_log[0] |= 1 << this->bitpos;
    }
    this->bitpos++;
  }

  void put_control_save() {
    if (this->bitpos >= 8) {
      this->bitpos = 0;
      this->output += this->forward_log;
      this->forward_log.resize(1);
      this->forward_log[0] = 0;
    }
  }

  void put_control_bit(bool bit) {
    this->put_control_bit_nosave(bit);
    this->put_control_save();
  }

  void put_static_data(uint8_t data) {
    this->forward_log.push_back(static_cast<char>(data));
  }

  void raw_byte(uint8_t value) {
    this->put_control_bit_nosave(1);
    this->put_static_data(value);
    this->put_control_save();
  }

  void short_copy(ssize_t offset, uint8_t size) {
    size -= 2;
    this->put_control_bit(0);
    this->put_control_bit(0);
    this->put_control_bit((size >> 1) & 1);
    this->put_control_bit_nosave(size & 1);
    this->put_static_data(offset & 0xFF);
    this->put_control_save();
  }

  void long_copy(ssize_t offset, uint8_t size) {
    if (size <= 9) {
      this->put_control_bit(0);
      this->put_control_bit_nosave(1);
      this->put_static_data(((offset << 3) & 0xF8) | ((size - 2) & 0x07));
      this->put_static_data((offset >> 5) & 0xFF);
      this->put_control_save();
    } else {
      this->put_control_bit(0);
      this->put_control_bit_nosave(1);
      this->put_static_data((offset << 3) & 0xF8);
      this->put_static_data((offset >> 5) & 0xFF);
      this->put_static_data(size - 1);
      this->put_control_save();
    }
  }

  void copy(ssize_t offset, uint8_t size) {
    if ((offset > -0x100) && (size <= 5)) {
      this->short_copy(offset, size);
    } else {
      this->long_copy(offset, size);
    }
  }
};

string prs_compress(const void* vdata, size_t size) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(vdata);
  prs_compress_ctx pc;

  ssize_t data_ssize = static_cast<ssize_t>(size);
  ssize_t read_offset = 0;
  while (read_offset < data_ssize) {

    // look for a chunk of data in history matching what's at the current offset
    ssize_t best_offset = 0;
    ssize_t best_size = 0;
    for (ssize_t this_offset = -3; // min copy size is 3 bytes
         (this_offset + read_offset >= 0) && // don't go before the beginning
         (this_offset > -0x1FF0) && // max offset is -0x1FF0
         (best_size < 255); // max size is 0xFF bytes
         this_offset--) {

      // for this offset, expand the match as much as possible
      ssize_t this_size = 1;
      while ((this_size < 0x100) && // max copy size is 255 bytes
             ((this_offset + this_size) < 0) && // don't copy past the read offset
             (this_size <= data_ssize - read_offset) && // don't copy past the end
             !memcmp(data + read_offset + this_offset, data + read_offset,
                     this_size)) {
        this_size++;
      }
      this_size--;

      if (this_size > best_size) {
        best_offset = this_offset;
        best_size = this_size;
      }
    }

    // if there are no good matches, write the byte directly
    if (best_size < 3) {
      pc.raw_byte(data[read_offset]);
      read_offset++;

    } else {
      pc.copy(best_offset, best_size);
      read_offset += best_size;
    }
  }

  return pc.finish();
}

string prs_compress(const string& data) {
  return prs_compress(data.data(), data.size());
}



static int16_t get_u8_or_eof(StringReader& r) {
  return r.eof() ? -1 : r.get_u8();
}

string prs_decompress(const void* data, size_t size, size_t max_output_size) {
  string output;
  StringReader r(data, size);

  int32_t r3, r5;
  int bitpos = 9;
  int16_t currentbyte; // int16_t because it can be -1 when EOF occurs
  int flag;
  int offset;
  unsigned long x, t;

  currentbyte = get_u8_or_eof(r);
  if (currentbyte == EOF) {
    return output;
  }

  for (;;) {
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      int ch = get_u8_or_eof(r);
      if (ch == EOF) {
        return output;
      }
      output += static_cast<char>(ch);
      if (max_output_size && (output.size() > max_output_size)) {
        throw runtime_error("maximum output size exceeded");
      }
      continue;
    }
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      r3 = get_u8_or_eof(r);
      if (r3 == EOF) {
        return output;
      }
      int high_byte = get_u8_or_eof(r);
      if (high_byte == EOF) {
        return output;
      }
      offset = ((high_byte & 0xFF) << 8) | (r3 & 0xFF);
      if (offset == 0) {
        return output;
      }
      r3 = r3 & 0x00000007;
      r5 = (offset >> 3) | 0xFFFFE000;
      if (r3 == 0) {
        flag = 0;
        r3 = get_u8_or_eof(r);
        if (r3 == EOF) {
          return output;
        }
        r3 = (r3 & 0xFF) + 1;
      } else {
        r3 += 2;
      }
    } else {
      r3 = 0;
      for (x = 0; x < 2; x++) {
        bitpos--;
        if (bitpos == 0) {
          currentbyte = get_u8_or_eof(r);
          if (currentbyte == EOF) {
            return output;
          }
          bitpos = 8;
        }
        flag = currentbyte & 1;
        currentbyte = currentbyte >> 1;
        offset = r3 << 1;
        r3 = offset | flag;
      }
      offset = get_u8_or_eof(r);
      if (offset == EOF) {
        return output;
      }
      r3 += 2;
      r5 = offset | 0xFFFFFF00;
    }
    if (r3 == 0) {
      continue;
    }
    t = r3;
    for (x = 0; x < t; x++) {
      output += output.at(output.size() + r5);
      if (max_output_size && (output.size() > max_output_size)) {
        throw runtime_error("maximum output size exceeded");
      }
    }
  }
}

string prs_decompress(const string& data, size_t max_output_size) {
  return prs_decompress(data.data(), data.size(), max_output_size);
}

size_t prs_decompress_size(const string& data, size_t max_output_size) {
  size_t output_size = 0;
  StringReader r(data.data(), data.size());

  int32_t r3;
  int bitpos = 9;
  int16_t currentbyte; // int16_t because it can be -1 when EOF occurs
  int flag;
  int offset;
  unsigned long x;

  currentbyte = get_u8_or_eof(r);
  if (currentbyte == EOF) {
    return output_size;
  }

  for (;;) {
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output_size;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      int ch = get_u8_or_eof(r);
      if (ch == EOF) {
        return output_size;
      }
      output_size++;
      if (max_output_size && (output_size > max_output_size)) {
        throw runtime_error("maximum output size exceeded");
      }
      continue;
    }
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output_size;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      r3 = get_u8_or_eof(r);
      if (r3 == EOF) {
        return output_size;
      }
      int high_byte = get_u8_or_eof(r);
      if (high_byte == EOF) {
        return output_size;
      }
      offset = ((high_byte & 0xFF) << 8) | (r3 & 0xFF);
      if (offset == 0) {
        return output_size;
      }
      r3 = r3 & 0x00000007;
      if (r3 == 0) {
        flag = 0;
        r3 = get_u8_or_eof(r);
        if (r3 == EOF) {
          return output_size;
        }
        r3 = (r3 & 0xFF) + 1;
      } else {
        r3 += 2;
      }
    } else {
      r3 = 0;
      for (x = 0; x < 2; x++) {
        bitpos--;
        if (bitpos == 0) {
          currentbyte = get_u8_or_eof(r);
          if (currentbyte == EOF) {
            return output_size;
          }
          bitpos = 8;
        }
        flag = currentbyte & 1;
        currentbyte = currentbyte >> 1;
        offset = r3 << 1;
        r3 = offset | flag;
      }
      offset = get_u8_or_eof(r);
      if (offset == EOF) {
        return output_size;
      }
      r3 += 2;
    }
    if (r3 == 0) {
      continue;
    }
    output_size += r3;
    if (max_output_size && (output_size > max_output_size)) {
      throw runtime_error("maximum output size exceeded");
    }
  }
}



// BC0 is a compression algorithm fairly similar to PRS, but with a simpler set
// of commands. Like PRS, there is a control stream, indicating when to copy a
// literal byte from the input and when to copy from a backreference; unlike
// PRS, there is only one type of backreference. Also, there is no stop opcode;
// the decompressor simply stops when there are no more input bytes to read.

// The BC0 decompression implementation in PSO GC is vulnerable to overflow
// attacks - there is no bounds checking on the output buffer. It is unlikely
// that this can be usefully exploited (e.g. for RCE) because the output pointer
// is checked before every byte is written, so we cannot change the output
// pointer to any arbitrary address.

string bc0_decompress(const string& data) {
  StringReader r(data);
  StringWriter w;

  // Unlike PRS, BC0 uses a memo which "rolls over" every 0x1000 bytes. The
  // boundaries of these "memo pages" are offset by -0x12 bytes for some reason,
  // so the first output byte corresponds to position 0xFEE on the first memo
  // page. Backreferences refer to offsets based on the start of memo pages; for
  // example, if the current output offset is 0x1234, a backreference with
  // offset 0x123 refers to the byte that was written at offset 0x1112 (because
  // that byte is at offset 0x112 in the memo, because the memo rolls over every
  // 0x1000 bytes and the first memo byte was 0x12 bytes before the beginning of
  // the next page). The memo is initially zeroed from 0 to 0xFEE; it seems PSO
  // GC doesn't initialize the last 0x12 bytes of the first memo page. For this
  // reason, we avoid generating backreferences that refer to those bytes.
  parray<uint8_t, 0x1000> memo;
  uint16_t memo_offset = 0x0FEE;

  // The low byte of this value contains the control stream data; the high bits
  // specify which low bits are valid. When the last 1 is shifted out of the
  // high bit, we need to read a new control stream byte to get the next set of
  // control bits.
  uint16_t control_stream_bits = 0x0000;

  while (!r.eof()) {
    // Read control stream bits if needed
    control_stream_bits >>= 1;
    if ((control_stream_bits & 0x100) == 0) {
      control_stream_bits = 0xFF00 | r.get_u8();
      if (r.eof()) {
        break;
      }
    }

    // Control bit 0 means to perform a backreference copy. The offset and
    // length are stored in two bytes in the input stream, laid out as follows:
    // a1 = 0bBBBBBBBB
    // a2 = 0bAAAACCCC
    // The offset is the concatenation of bits AAAABBBBBBBB, which refers to a
    // position in the memo; the number of bytes to copy is (CCCC + 3). The
    // decompressor copies that many bytes from that offset in the memo, and
    // writes them to the output and to the current position in the memo.
    if ((control_stream_bits & 1) == 0) {
      uint8_t a1 = r.get_u8();
      if (r.eof()) {
        break;
      }
      uint8_t a2 = r.get_u8();
      size_t count = (a2 & 0x0F) + 3;
      size_t backreference_offset = a1 | ((a2 << 4) & 0xF00);
      for (size_t z = 0; z < count; z++) {
        uint8_t v = memo[(backreference_offset + z) & 0x0FFF];
        w.put_u8(v);
        memo[memo_offset] = v;
        memo_offset = (memo_offset + 1) & 0x0FFF;
      }

    // Control bit 1 means to write a byte directly from the input to the
    // output. As above, the byte is also written to the memo.
    } else {
      uint8_t v = r.get_u8();
      w.put_u8(v);
      memo[memo_offset] = v;
      memo_offset = (memo_offset + 1) & 0x0FFF;
    }
  }

  return move(w.str());
}



string bc0_compress(const string& data) {
  StringReader r(data);
  StringWriter w;

  parray<uint8_t, 0x1000> memo;
  uint16_t memo_offset = 0x0FEE;

  size_t next_control_byte_offset = w.size();
  w.put_u8(0);
  uint16_t pending_control_bits = 0x0000;

  parray<uint8_t, 17> match_buf;
  while (!r.eof()) {
    // Search in the memo for the longest string matching the upcoming data, of
    // length 3-17 bytes
    size_t best_match_offset = 0;
    size_t best_match_length = 0;
    size_t max_match_length = min<size_t>(r.remaining(), 17);
    r.readx(match_buf.data(), max_match_length, false);
    for (size_t match_length = 3; match_length <= max_match_length; match_length++) {

      // Forbid matches that span the current memo position, or that cover the
      // uninitialized part of the memo when the client decompresses (see
      // comment in bc0_decompress about this)
      size_t start_offset = (r.where() < 0x12) ? 0 : memo_offset;
      size_t end_offset = (memo_offset - match_length + 1) & 0xFFF;

      for (size_t offset = start_offset; offset != end_offset; offset = (offset + 1) & 0xFFF) {
        bool match_found = true;
        for (size_t z = 0; z < match_length; z++) {
          if (match_buf[z] != memo[(offset + z) & 0xFFF]) {
            match_found = false;
            break;
          }
        }
        // If a match was found at this length, don't bother looking for other
        // matches of the same length
        if (match_found) {
          best_match_length = match_length;
          best_match_offset = offset;
          break;
        }
      }
      // If no matches were found at the current length, don't bother looking
      // for longer matches
      if (best_match_length < match_length) {
        break;
      }
    }

    // Write a backreference if a match was found; otherwise, write a literal
    if (best_match_length >= 3) {
      pending_control_bits = (pending_control_bits >> 1) | 0x8000;
      w.put_u8(best_match_offset & 0xFF); // a1
      w.put_u8(((best_match_offset >> 4) & 0xF0) | (best_match_length - 3)); // a2
      for (size_t z = 0; z < best_match_length; z++) {
        memo[memo_offset] = r.get_u8();
        memo_offset = (memo_offset + 1) & 0xFFF;
      }
    } else {
      pending_control_bits = (pending_control_bits >> 1) | 0x8080;
      uint8_t v = r.get_u8();
      w.put_u8(v);
      memo[memo_offset] = v;
      memo_offset = (memo_offset + 1) & 0xFFF;
    }

    // Write the control byte to the output if needed, and reserve space for the
    // next one
    if (pending_control_bits & 0x0100) {
      w.pput_u8(next_control_byte_offset, pending_control_bits & 0xFF);
      next_control_byte_offset = w.size();
      w.put_u8(0);
      pending_control_bits = 0x0000;
    }
  }

  // Write the final control byte to the output if needed. If not needed, then
  // there should be an empty reserved space at the end; delete it since none of
  // its bits will be used.
  if (pending_control_bits & 0xFF00) {
    while (!(pending_control_bits & 0x0100)) {
      pending_control_bits >>= 1;
    }
    w.pput_u8(next_control_byte_offset, pending_control_bits & 0xFF);
  } else {
    if (next_control_byte_offset != w.size() - 1) {
      throw logic_error("data written without control bits");
    }
    w.str().resize(w.str().size() - 1);
  }

  return move(w.str());
}
