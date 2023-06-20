#include "Compression.hh"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <phosg/Strings.hh>

#include "Text.hh"

using namespace std;

PRSCompressor::PRSCompressor(
    size_t compression_level, function<void(size_t, size_t)> progress_fn)
    : compression_level(compression_level),
      progress_fn(progress_fn),
      closed(false),
      control_byte_offset(0),
      pending_control_bits(0),
      input_bytes(0) {
  this->output.put_u8(0);
}

void PRSCompressor::add(const void* data, size_t size) {
  if (this->closed) {
    throw logic_error("compressor is closed");
  }

  StringReader r(data, size);
  while (!r.eof()) {
    this->add_byte(r.get_u8());
  }
}

void PRSCompressor::add(const string& data) {
  this->add(data.data(), data.size());
}

void PRSCompressor::add_byte(uint8_t v) {
  if (this->reverse_log.end_offset() + this->forward_log.data.size() <= this->input_bytes) {
    this->advance();
  }
  this->forward_log.at(this->input_bytes) = v;
  this->input_bytes++;
}

void PRSCompressor::advance() {
  // Search for a match in the decompressed data history
  size_t best_match_size = 0;
  size_t best_match_offset = 0;
  size_t best_match_literals = 0;
  for (size_t num_literals = 0; num_literals < this->compression_level; num_literals++) {
    for (size_t z = 0; z < num_literals; z++) {
      this->reverse_log.push_back(this->forward_log.at(this->reverse_log.end_offset()));
    }

    size_t compression_offset = reverse_log.end_offset();
    uint8_t first_v = this->forward_log.at(compression_offset);
    const auto& start_offsets = this->reverse_log.find(first_v);

    for (auto it = start_offsets.begin(); (it != start_offsets.end()) && (best_match_size < 0x100); it++) {
      size_t match_offset = *it;
      if (match_offset + 0x2000 <= compression_offset) {
        continue;
      }

      size_t match_size = 0;
      size_t match_loop_bytes = compression_offset - match_offset;
      while ((match_size < 0x100) &&
          (compression_offset + match_size < this->input_bytes) &&
          (this->reverse_log.at(match_offset + (match_size % match_loop_bytes)) == this->forward_log.at(compression_offset + match_size))) {
        match_size++;
      }

      // If there are multiple matches of the longest length, use the latest one,
      // since it's more likely that it can be expressed as a short copy instead
      // of a long copy.
      if (match_size >= (best_match_size + best_match_literals)) {
        best_match_offset = match_offset;
        best_match_size = match_size;
        best_match_literals = num_literals;
      }
    }
    for (size_t z = 0; z < num_literals; z++) {
      this->reverse_log.pop_back();
    }
  }

  // If the best match has literals preceding it, write those literals
  for (size_t z = 0; z < best_match_literals; z++) {
    this->advance_literal();
  }

  // If there is a suitable match, write a backreference; otherwise, write a
  // literal. The backreference should be encoded:
  // - As a short copy if offset in [-0x100, -1] and size in [2, 5]
  // - As a long copy if offset in [-0x1FFF, -1] and size in [3, 9]
  // - As an extended copy if offset in [-0x1FFF, -1] and size in [10, 0x100]
  // Technically an extended copy can be used for sizes 1-9 as well, but if
  // size is 1 or 2, writing literals is better (since it uses fewer data
  // bytes and control bits), and a long copy can cover sizes 3-9 (and also
  // uses fewer data bytes and control bits).
  ssize_t backreference_offset = best_match_offset - this->reverse_log.end_offset();
  if (best_match_size < 2) {
    // The match is too small; a literal would use fewer bits
    this->advance_literal();

  } else if ((backreference_offset >= -0x100) && (best_match_size <= 5)) {
    this->advance_short_copy(backreference_offset, best_match_size);

  } else if (best_match_size < 3) {
    // We can't use a long copy for size 2, and it's not worth it to use an
    // extended copy for this either (as noted above), so write a literal
    this->advance_literal();

  } else if ((backreference_offset >= -0x1FFF) && (best_match_size <= 9)) {
    this->advance_long_copy(backreference_offset, best_match_size);

  } else if ((backreference_offset >= -0x1FFF) && (best_match_size <= 0x100)) {
    this->advance_extended_copy(backreference_offset, best_match_size);

  } else {
    throw logic_error("invalid best match");
  }
}

void PRSCompressor::move_forward_data_to_reverse_log(size_t size) {
  for (; size > 0; size--) {
    this->reverse_log.push_back(this->forward_log.at(this->reverse_log.end_offset()));
    if (this->progress_fn && ((this->reverse_log.end_offset() & 0xFFF) == 0)) {
      this->progress_fn(this->reverse_log.end_offset(), this->output.size());
    }
  }
}

void PRSCompressor::advance_literal() {
  this->write_control(true);
  this->output.put_u8(this->forward_log.at(this->reverse_log.end_offset()));
  this->move_forward_data_to_reverse_log(1);
}

void PRSCompressor::advance_short_copy(ssize_t offset, size_t size) {
  uint8_t encoded_size = size - 2;
  this->write_control(false);
  this->write_control(false);
  this->write_control(encoded_size & 2);
  this->write_control(encoded_size & 1);
  this->output.put_u8(offset & 0xFF);
  this->move_forward_data_to_reverse_log(size);
}

void PRSCompressor::advance_long_copy(ssize_t offset, size_t size) {
  this->write_control(false);
  this->write_control(true);
  uint16_t a = (offset << 3) | (size - 2);
  this->output.put_u8(a & 0xFF);
  this->output.put_u8(a >> 8);
  this->move_forward_data_to_reverse_log(size);
}

void PRSCompressor::advance_extended_copy(ssize_t offset, size_t size) {
  this->write_control(false);
  this->write_control(true);
  uint16_t a = (offset << 3);
  this->output.put_u8(a & 0xFF);
  this->output.put_u8(a >> 8);
  this->output.put_u8(size - 1);
  this->move_forward_data_to_reverse_log(size);
}

string& PRSCompressor::close() {
  if (!this->closed) {
    // Advance until all input is consumed
    while (this->reverse_log.end_offset() < this->input_bytes) {
      this->advance();
    }
    // Write stop command
    this->write_control(false);
    this->write_control(true);
    this->output.put_u8(0);
    this->output.put_u8(0);
    // Write remaining control bits
    this->flush_control();
    this->closed = true;
  }
  return this->output.str();
}

void PRSCompressor::write_control(bool z) {
  if (this->pending_control_bits & 0x0100) {
    this->output.pput_u8(
        this->control_byte_offset, this->pending_control_bits & 0xFF);
    this->control_byte_offset = this->output.size();
    this->output.put_u8(0);
    this->pending_control_bits = z ? 0x8080 : 0x8000;
  } else {
    this->pending_control_bits =
        (this->pending_control_bits >> 1) | (z ? 0x8080 : 0x8000);
  }
}

void PRSCompressor::flush_control() {
  if (this->pending_control_bits & 0xFF00) {
    while (!(this->pending_control_bits & 0x0100)) {
      this->pending_control_bits >>= 1;
    }
    this->output.pput_u8(
        this->control_byte_offset, this->pending_control_bits & 0xFF);
  } else {
    if (this->control_byte_offset != this->output.size() - 1) {
      throw logic_error("data written without control bits");
    }
    this->output.str().resize(this->output.str().size() - 1);
  }
}

string prs_compress(
    const void* vdata,
    size_t size,
    size_t compression_level,
    function<void(size_t, size_t)> progress_fn) {
  PRSCompressor prs(compression_level, progress_fn);
  prs.add(vdata, size);
  return std::move(prs.close());
}

string prs_compress(
    const string& data,
    size_t compression_level,
    function<void(size_t, size_t)> progress_fn) {
  return prs_compress(data.data(), data.size(), compression_level, progress_fn);
}

class ControlStreamReader {
public:
  ControlStreamReader(StringReader& r)
      : r(r),
        bits(0x0000) {}

  bool read() {
    if (!(this->bits & 0x0100)) {
      this->bits = 0xFF00 | this->r.get_u8();
    }
    bool ret = this->bits & 1;
    this->bits >>= 1;
    return ret;
  }

  uint8_t buffered_bits() const {
    uint16_t z = this->bits;
    uint8_t ret = 0;
    for (; z & 0x0100; z >>= 1, ret++) {
    }
    return ret;
  }

private:
  StringReader& r;
  uint16_t bits;
};

string prs_decompress(const void* data, size_t size, size_t max_output_size) {
  // PRS is an LZ77-based compression algorithm. Compressed data is split into
  // two streams: a control stream and a data stream. The control stream is read
  // one bit at a time, and the data stream is read one byte at a time. The
  // streams are interleaved such that the decompressor never has to move
  // backward in the input stream - when the decompressor needs a control bit
  // and there are no unused bits from the previous byte of the control stream,
  // it reads a byte from the input and treats it as the next 8 control bits.

  // There are 3 distinct commands in PRS, labeled here with their control bits:
  // 1 - Literal byte. The decompressor copies one byte from the input data
  //     stream to the output.
  // 00 - Short backreference. The decompressor reads two control bits and adds
  //      2 to this value to determine the number of bytes to copy, then reads
  //      one byte from the data stream to determine how far back in the output
  //      to copy from. This byte is treated as an 8-bit negative number - so
  //      0xF7, for example, means to start copying data from 9 bytes before the
  //      end of the output. The range must start before the end of the output,
  //      but the end of the range may be beyond the end of the output. In this
  //      case, the bytes between the beginning of the range and original end of
  //      the output are simply repeated.
  // 01 - Long backreference. The decompressor reads two bytes from the data and
  //      byteswaps the resulting 16-bit value (that is, the low byte is read
  //      first). The start offset (again, as a negative number) is the top 13
  //      bits of this value; the size is the low 3 bits of this value, plus 2.
  //      If the size bits are all zero, an additional byte is read from the
  //      data stream and 1 is added to it to determine the backreference size
  //      (we call this an extended backreference). Therefore, the maximum
  //      backreference size is 256 bytes.
  // Decompression ends when either there are no more input bytes to read, or
  // when a long backreference is read with all zeroes in its offset field. The
  // original implementation stops decompression successfully when any attempt
  // to read from the input encounters the end of the stream, but newserv's
  // implementation only allows this at the end of an opcode - if end-of-stream
  // is encountered partway through an opcode, we throw instead, because it's
  // likely the input has been truncated or is malformed in some way.

  StringWriter w;
  StringReader r(data, size);
  ControlStreamReader cr(r);

  while (!r.eof()) {
    // Control 1 = literal byte
    if (cr.read()) {
      if (max_output_size && w.size() == max_output_size) {
        throw runtime_error("maximum output size exceeded");
      }
      w.put_u8(r.get_u8());

    } else {
      ssize_t offset;
      size_t count;

      // Control 01 = long backreference
      if (cr.read()) {
        // The bits stored in the data stream are AAAAABBBCCCCCCCC, which we
        // rearrange into offset = CCCCCCCCAAAAA and size = BBB.
        uint16_t a = r.get_u8();
        a |= (r.get_u8() << 8);
        offset = (a >> 3) | (~0x1FFF);
        // If offset is zero, it's a stop opcode
        if (offset == ~0x1FFF) {
          break;
        }
        // If the size field is zero, it's an extended backreference (size comes
        // from another byte in the data stream)
        count = (a & 7) ? ((a & 7) + 2) : (r.get_u8() + 1);

        // Control 00 = short backreference
      } else {
        // Count comes from 2 bits in the control stream instead of from the
        // data stream (and 2 is added). Importantly, the control stream bits
        // are read first - this may involve reading another control stream
        // byte, which happens before the offset is read from the data stream.
        count = cr.read() << 1;
        count = (count | cr.read()) + 2;
        offset = r.get_u8() | (~0xFF);
      }

      // Copy bytes from the referenced location in the output. Importantly,
      // copy only one byte at a time, in order to support ranges that cover the
      // current end of the output.
      size_t read_offset = w.size() + offset;
      if (read_offset >= w.size()) {
        throw runtime_error("backreference offset beyond beginning of output");
      }
      for (size_t z = 0; z < count; z++) {
        if (max_output_size && w.size() == max_output_size) {
          throw runtime_error("maximum output size exceeded");
        }
        w.put_u8(w.str()[read_offset + z]);
      }
    }
  }

  return std::move(w.str());
}

string prs_decompress(const string& data, size_t max_output_size) {
  return prs_decompress(data.data(), data.size(), max_output_size);
}

size_t prs_decompress_size(const void* data, size_t size, size_t max_output_size) {
  size_t ret = 0;
  StringReader r(data, size);
  ControlStreamReader cr(r);

  while (!r.eof()) {
    if (cr.read()) {
      ret++;
      r.get_u8();

    } else {
      ssize_t offset;
      size_t count;

      if (cr.read()) {
        uint16_t a = r.get_u8();
        a |= (r.get_u8() << 8);
        offset = (a >> 3) | (~0x1FFF);
        if (offset == ~0x1FFF) {
          break;
        }
        count = (a & 7) ? ((a & 7) + 2) : (r.get_u8() + 1);

      } else {
        count = cr.read() << 1;
        count = (count | cr.read()) + 2;
        offset = r.get_u8() | (~0xFF);
      }

      size_t read_offset = ret + offset;
      if (read_offset >= ret) {
        throw runtime_error("backreference offset beyond beginning of output");
      }
      ret += count;
    }

    if (max_output_size && ret > max_output_size) {
      throw runtime_error("maximum output size exceeded");
    }
  }

  return ret;
}

size_t prs_decompress_size(const string& data, size_t max_output_size) {
  return prs_decompress_size(data.data(), data.size(), max_output_size);
}

void prs_disassemble(FILE* stream, const void* data, size_t size) {
  size_t output_bytes = 0;
  StringReader r(data, size);
  ControlStreamReader cr(r);

  while (!r.eof()) {
    size_t r_offset = r.where();
    uint8_t buffered_bits = cr.buffered_bits();
    size_t input_bits = 8 * r_offset + (buffered_bits ? (8 - buffered_bits) : 0);
    if (cr.read()) {
      fprintf(stream, "[%zX / %zX => %zX] literal %02hhX\n", r_offset, input_bits, output_bytes, r.get_u8());
      output_bytes++;

    } else {
      ssize_t offset;
      size_t count;

      bool is_long_copy = cr.read();
      if (is_long_copy) {
        uint16_t a = r.get_u8();
        a |= (r.get_u8() << 8);
        offset = (a >> 3) | (~0x1FFF);
        if (offset == ~0x1FFF) {
          fprintf(stream, "[%zX / %zX => %zX] end\n", r_offset, input_bits, output_bytes);
          break;
        }
        count = (a & 7) ? ((a & 7) + 2) : (r.get_u8() + 1);

      } else {
        count = cr.read() << 1;
        count = (count | cr.read()) + 2;
        offset = r.get_u8() | (~0xFF);
      }

      size_t read_offset = output_bytes + offset;
      fprintf(stream, "[%zX / %zX => %zX] %s copy -%zX (from %zX) %zX\n",
          r_offset, input_bits, output_bytes, is_long_copy ? "long" : "short",
          -offset, read_offset, count);

      if (read_offset >= output_bytes) {
        throw runtime_error("backreference offset beyond beginning of output");
      }
      output_bytes += count;
    }
  }
}

void prs_disassemble(FILE* stream, const std::string& data) {
  return prs_disassemble(stream, data.data(), data.size());
}

// BC0 is a compression algorithm fairly similar to PRS, but with a simpler set
// of commands. Like PRS, there is a control stream, indicating when to copy a
// literal byte from the input and when to copy from a backreference; unlike
// PRS, there is only one type of backreference. Also, there is no stop opcode;
// the decompressor simply stops when there are no more input bytes to read.

// TODO: bc0_compress produces slightly larger output than Sega's compressor.
// Reverse-engineer their implementation and fix this.

string bc0_compress(
    const string& data, function<void(size_t, size_t)> progress_fn) {
  StringReader r(data);
  StringWriter w;

  parray<uint8_t, 0x1000> memo;
  uint16_t memo_offset = 0x0FEE;
  size_t memo_bytes_written = 0;
  vector<deque<size_t>> memo_index(0x100);
  auto write_memo = [&](uint8_t new_v) -> void {
    uint8_t existing_v = memo[memo_offset];
    if ((memo_bytes_written >= 0x1000) && !memo_index[existing_v].empty()) {
      memo_index[existing_v].pop_front();
    }
    memo[memo_offset] = new_v;
    memo_index[new_v].emplace_back(memo_offset);
    memo_offset = (memo_offset + 1) & 0xFFF;
    memo_bytes_written++;
  };

  size_t next_control_byte_offset = w.size();
  w.put_u8(0);
  uint16_t pending_control_bits = 0x0000;

  parray<uint8_t, 18> match_buf;
  while (!r.eof()) {
    if ((r.where() & 0x1000) && progress_fn) {
      progress_fn(r.where(), w.size());
    }

    // Search in the memo for the longest string matching the upcoming data, of
    // size 3-18 bytes
    size_t best_match_offset = 0;
    size_t best_match_size = 0;
    size_t max_match_size = min<size_t>(r.remaining(), 18);
    const uint8_t* match_buf = &r.get<uint8_t>(false, max_match_size);

    for (size_t match_size = 3; match_size <= max_match_size; match_size++) {
      for (size_t offset : memo_index[match_buf[0]]) {
        // Forbid matches that span the memo boundary - during decompression,
        // the client will be overwriting its memo while reading from it and
        // will likely generate incorrect data
        // TODO: We can actually support this (and it will improve compression),
        // but we have to set a loop boundary like we have in prs_compress and
        // I'm lazy.
        size_t start_memo_offset = offset;
        size_t end_memo_offset = (offset + match_size) & 0xFFF;
        if (end_memo_offset < start_memo_offset) {
          if ((memo_offset < end_memo_offset) || (memo_offset > start_memo_offset)) {
            continue;
          }
        } else {
          if ((memo_offset > start_memo_offset) && (memo_offset < end_memo_offset)) {
            continue;
          }
        }

        // Note: We don't have to explicitly forbid matches that span the
        // uninitialized part of the memo (during the first 0x12 bytes) because
        // the preceding check will catch those too (and there can't be any
        // start offsets in the memo index within that region anyway).

        bool match_found = true;
        for (size_t z = 0; z < match_size; z++) {
          if (match_buf[z] != memo[(offset + z) & 0xFFF]) {
            match_found = false;
            break;
          }
        }
        // If a match was found at this size, don't bother looking for other
        // matches of the same size
        if (match_found) {
          best_match_size = match_size;
          best_match_offset = offset;
          break;
        }
      }
      // If no matches were found at the current size, don't bother looking for
      // longer matches
      if (best_match_size < match_size) {
        break;
      }
    }

    // Write a backreference if a match was found; otherwise, write a literal
    if (best_match_size >= 3) {
      pending_control_bits = (pending_control_bits >> 1) | 0x8000;
      w.put_u8(best_match_offset & 0xFF); // a1
      w.put_u8(((best_match_offset >> 4) & 0xF0) | (best_match_size - 3)); // a2
      for (size_t z = 0; z < best_match_size; z++) {
        write_memo(r.get_u8());
      }
    } else {
      pending_control_bits = (pending_control_bits >> 1) | 0x8080;
      uint8_t v = r.get_u8();
      w.put_u8(v);
      write_memo(v);
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

  return std::move(w.str());
}

// The BC0 decompression implementation in PSO GC is vulnerable to overflow
// attacks - there is no bounds checking on the output buffer. It is unlikely
// that this can be usefully exploited (e.g. for RCE) because the output pointer
// is loaded from memory before every byte is written, so we cannot change the
// output pointer to any arbitrary address.

string bc0_decompress(const string& data) {
  return bc0_decompress(data.data(), data.size());
}

string bc0_decompress(const void* data, size_t size) {
  StringReader r(data, size);
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
  // high byte, we need to read a new control stream byte to get the next set of
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
    // size are stored in two bytes in the input stream, laid out as follows:
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

  return std::move(w.str());
}
