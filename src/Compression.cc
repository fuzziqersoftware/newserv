#include "Compression.hh"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <phosg/Strings.hh>
#include <set>

#include "Text.hh"

using namespace std;

template <>
const char* phosg::name_for_enum<CompressPhase>(CompressPhase v) {
  switch (v) {
    case CompressPhase::INDEX:
      return "INDEX";
    case CompressPhase::CONSTRUCT_PATHS:
      return "CONSTRUCT_PATHS";
    case CompressPhase::BACKTRACE_OPTIMAL_PATH:
      return "BACKTRACE_OPTIMAL_PATH";
    case CompressPhase::GENERATE_RESULT:
      return "GENERATE_RESULT";
    default:
      return "__UNKNOWN__";
  }
}

template <size_t WindowLength, size_t MaxMatchLength>
struct WindowIndex {
  const uint8_t* data;
  size_t size;
  size_t offset;
  set<size_t, function<bool(size_t, size_t)>> index;

  WindowIndex(const void* data, size_t size)
      : data(reinterpret_cast<const uint8_t*>(data)),
        size(size),
        offset(0),
        index(bind(&WindowIndex::set_comparator, this, placeholders::_1, placeholders::_2)) {}

  void advance() {
    if (this->offset >= WindowLength) {
      this->index.erase(this->offset - WindowLength);
    }
    this->index.emplace(this->offset);
    this->offset++;
  }

  size_t get_match_length(size_t match_offset) const {
    size_t match_iter = match_offset;
    size_t offset_iter = this->offset;
    while ((match_iter < match_offset + MaxMatchLength) &&
        (match_iter < this->size) &&
        (offset_iter < this->size) &&
        (this->data[match_iter] == this->data[offset_iter])) {
      match_iter++;
      offset_iter++;
    }
    return match_iter - match_offset;
  };

  // The data structure we want is a binary-searchable set of all strings
  // starting at all possible offsets within the sliding window, and we need
  // to be able to search lexicographically but insert and delete by offset.
  // A std::map<std::string, size_t> would accomplish this, but would be
  // horrendously inefficient: we'd have to copy strings far too much. We can
  // solve this by instead storing the offset of each string as keys in a set
  // and using a custom comparator to treat them as references to binary
  // strings within the data.
  bool set_comparator(size_t a, size_t b) const {
    size_t max_length = min<size_t>(MaxMatchLength, this->size - max<size_t>(a, b));
    size_t end_a = a + max_length;
    for (; a < end_a; a++, b++) {
      uint8_t data_a = static_cast<uint8_t>(this->data[a]);
      uint8_t data_b = static_cast<uint8_t>(this->data[b]);
      if (data_a < data_b) {
        return true; // a comes before b lexicographically
      } else if (data_a > data_b) {
        return false; // a comes after b lexicographically
      }
    }
    return a < b; // Maximum-length match; order them by offset
  };

  pair<size_t, size_t> get_best_match() const {
    // Find the best match from the index. It's unlikely that we'll get an
    // exact match, so check the entry before the upper_bound result too.
    // Note: We use upper_bound rather than lower_bound because in PRS, a
    // backreference can be encoded with fewer bits if it's close to the
    // decompression offset, and this makes us pick the latest match by
    // default.
    size_t match_offset = 0;
    size_t match_size = 0;
    auto it = this->index.upper_bound(this->offset);
    if (it != this->index.end()) {
      size_t new_match_offset = *it;
      size_t new_match_size = this->get_match_length(new_match_offset);
      if ((new_match_size > match_size) || (new_match_size == match_size && new_match_offset > match_offset)) {
        match_offset = new_match_offset;
        match_size = new_match_size;
      }
    }
    if (it != this->index.begin()) {
      it--;
      size_t new_match_offset = *it;
      size_t new_match_size = this->get_match_length(new_match_offset);
      if ((new_match_size > match_size) || (new_match_size == match_size && new_match_offset > match_offset)) {
        match_offset = new_match_offset;
        match_size = new_match_size;
      }
    }
    return make_pair(match_offset, match_size);
  }
};

struct LZSSInterleavedWriter {
  phosg::StringWriter w;
  size_t buf_offset;
  uint8_t next_control_bit;
  uint8_t buf[0x19];

  LZSSInterleavedWriter()
      : buf_offset(1),
        next_control_bit(1) {
    this->buf[0] = 0;
  }

  void flush_if_ready() {
    if (this->next_control_bit == 0) {
      this->w.write(this->buf, this->buf_offset);
      this->buf[0] = 0;
      this->buf_offset = 1;
      this->next_control_bit = 1;
    }
  }

  std::string&& close() {
    if (this->buf_offset > 1 || this->next_control_bit != 1) {
      this->w.write(this->buf, this->buf_offset);
    }
    return std::move(this->w.str());
  }

  void write_control(bool v) {
    if (this->next_control_bit == 0) {
      throw logic_error("write_control called with no space to write");
    }
    if (v) {
      this->buf[0] |= this->next_control_bit;
    }
    this->next_control_bit <<= 1;
  }

  void write_data(uint8_t v) {
    this->buf[this->buf_offset++] = v;
  }

  size_t size() const {
    return this->w.size() + this->buf_offset;
  }
};

class ControlStreamReader {
public:
  ControlStreamReader(phosg::StringReader& r)
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
  phosg::StringReader& r;
  uint16_t bits;
};

struct PRSPathNode {
  enum class CommandType {
    NONE = 0,
    LITERAL,
    SHORT_COPY,
    LONG_COPY,
    EXTENDED_COPY,
  };

  int16_t short_copy_offset = 0;
  uint8_t max_short_copy_size = 0;
  int16_t long_copy_offset = 0;
  uint8_t max_long_copy_size = 0;
  int16_t extended_copy_offset = 0;
  uint16_t max_extended_copy_size = 0;

  // Pathfinding state
  size_t from_offset = 0;
  CommandType from_command_type = CommandType::NONE;
  size_t bits_used = static_cast<size_t>(-1);

  // Stream generation state
  size_t to_offset = 0;
};

string prs_compress_optimal(const void* in_data_v, size_t in_size, ProgressCallback progress_fn) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_v);

  vector<PRSPathNode> nodes;
  nodes.resize(in_size + 1);
  nodes[0].bits_used = 18; // Stop command: 2 control bits and 2 data bytes

  size_t copy_progress_max = 3 * in_size;
  atomic<size_t> copy_progress = 0;

  // Populate all possible short copies
  std::thread short_window_thread([&]() -> void {
    WindowIndex<0x100, 5> window(in_data_v, in_size);
    while (window.offset < in_size) {
      if (window.offset && (window.offset & 0xFFF) == 0 && progress_fn) {
        size_t progress = copy_progress.fetch_add(0x1000) + 0x1000;
        progress_fn(CompressPhase::INDEX, progress, copy_progress_max, 0);
      }
      auto& node = nodes[window.offset];
      auto match = window.get_best_match();
      if (match.second >= 2) {
        node.short_copy_offset = match.first - window.offset;
        node.max_short_copy_size = match.second;
      }
      window.advance();
    }
  });

  // Populate all possible long copies
  std::thread long_window_thread([&]() -> void {
    WindowIndex<0x1FFF, 9> window(in_data_v, in_size);
    while (window.offset < in_size) {
      if (window.offset && (window.offset & 0xFFF) == 0 && progress_fn) {
        size_t progress = copy_progress.fetch_add(0x1000) + 0x1000;
        progress_fn(CompressPhase::INDEX, progress, copy_progress_max, 0);
      }
      auto& node = nodes[window.offset];
      auto match = window.get_best_match();
      if (match.second >= 3) {
        node.long_copy_offset = match.first - window.offset;
        node.max_long_copy_size = match.second;
      }
      window.advance();
    }
  });

  // Populate all possible extended copies
  std::thread extended_window_thread([&]() -> void {
    WindowIndex<0x1FFF, 0x100> window(in_data_v, in_size);
    while (window.offset < in_size) {
      if (window.offset && (window.offset & 0xFFF) == 0 && progress_fn) {
        size_t progress = copy_progress.fetch_add(0x1000) + 0x1000;
        progress_fn(CompressPhase::INDEX, progress, copy_progress_max, 0);
      }
      auto& node = nodes[window.offset];
      auto match = window.get_best_match();
      if (match.second >= 1) {
        node.extended_copy_offset = match.first - window.offset;
        node.max_extended_copy_size = match.second;
      }
      window.advance();
    }
  });

  short_window_thread.join();
  long_window_thread.join();
  extended_window_thread.join();

  // For each node, populate the literal value, and the best ways to get to the
  // following nodes
  for (size_t z = 0; z < in_size; z++) {
    if ((z & 0xFFF) == 0 && progress_fn) {
      progress_fn(CompressPhase::CONSTRUCT_PATHS, z, in_size, 0);
    }

    auto& node = nodes[z];

    // Literal: 1 control bit + 1 data byte
    size_t bits_used = node.bits_used + 9;
    {
      auto& next_node = nodes[z + 1];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.from_command_type = PRSPathNode::CommandType::LITERAL;
        next_node.bits_used = bits_used;
      }
    }

    // Short copy: 4 control bits + 1 data byte
    bits_used = node.bits_used + 12;
    for (size_t x = 2; x <= node.max_short_copy_size; x++) {
      auto& next_node = nodes[z + x];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.from_command_type = PRSPathNode::CommandType::SHORT_COPY;
        next_node.bits_used = bits_used;
      }
    }

    // Long copy: 2 control bits + 2 data bytes
    bits_used = node.bits_used + 18;
    for (size_t x = 3; x <= node.max_long_copy_size; x++) {
      auto& next_node = nodes[z + x];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.from_command_type = PRSPathNode::CommandType::LONG_COPY;
        next_node.bits_used = bits_used;
      }
    }

    // Extended copy: 2 control bits + 3 data bytes
    bits_used = node.bits_used + 26;
    for (size_t x = 1; x <= node.max_extended_copy_size; x++) {
      auto& next_node = nodes[z + x];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.from_command_type = PRSPathNode::CommandType::EXTENDED_COPY;
        next_node.bits_used = bits_used;
      }
    }
  }

  // Find the shortest path from the last node to the first node
  size_t last_progress_fn_call = static_cast<size_t>(-1);
  for (size_t z = in_size; z > 0;) {
    if ((z & ~0xFFF) != (last_progress_fn_call & ~0xFFF)) {
      last_progress_fn_call = z;
      if (progress_fn) {
        progress_fn(CompressPhase::BACKTRACE_OPTIMAL_PATH, z, in_size, 0);
      }
    }
    size_t from_offset = nodes[z].from_offset;
    nodes[from_offset].to_offset = z;
    z = from_offset;
  }

  // Produce the PRS command stream from the shortest path
  LZSSInterleavedWriter w;
  last_progress_fn_call = static_cast<size_t>(-1);
  for (size_t offset = 0; offset < in_size;) {
    if ((offset & ~0xFFF) != (last_progress_fn_call & ~0xFFF)) {
      last_progress_fn_call = offset;
      if (progress_fn) {
        progress_fn(CompressPhase::GENERATE_RESULT, offset, in_size, w.size());
      }
    }

    const auto& node = nodes[offset];
    const auto& next_node = nodes[node.to_offset];

    size_t copy_size = node.to_offset - offset;
    switch (next_node.from_command_type) {
      case PRSPathNode::CommandType::LITERAL:
        if (copy_size != 1) {
          throw logic_error("incorrect size for LITERAL copy type");
        }
        w.write_control(true);
        w.write_data(in_data[offset]);
        break;
      case PRSPathNode::CommandType::SHORT_COPY: {
        if (copy_size < 2 || copy_size > 5) {
          throw logic_error("incorrect size for SHORT_COPY copy type");
        }
        uint8_t encoded_size = copy_size - 2;
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(encoded_size & 2);
        w.flush_if_ready();
        w.write_control(encoded_size & 1);
        w.write_data(node.short_copy_offset & 0xFF);
        break;
      }
      case PRSPathNode::CommandType::LONG_COPY: {
        if (copy_size < 2 || copy_size > 9) {
          throw logic_error("incorrect size for LONG_COPY copy type");
        }
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(true);
        uint16_t a = (node.long_copy_offset << 3) | (copy_size - 2);
        w.write_data(a & 0xFF);
        w.write_data(a >> 8);
        break;
      }
      case PRSPathNode::CommandType::EXTENDED_COPY: {
        if (copy_size < 1 || copy_size > 0x100) {
          throw logic_error("incorrect size for EXTENDED_COPY copy type");
        }
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(true);
        uint16_t a = (node.extended_copy_offset << 3);
        w.write_data(a & 0xFF);
        w.write_data(a >> 8);
        w.write_data(copy_size - 1);
        break;
      }
      default:
        throw logic_error("invalid copy type in shortest path");
    }
    w.flush_if_ready();

    offset = node.to_offset;
  }

  // Write stop command
  w.write_control(false);
  w.flush_if_ready();
  w.write_control(true);
  w.write_data(0);
  w.write_data(0);

  return std::move(w.close());
}

string prs_compress_optimal(const string& data, ProgressCallback progress_fn) {
  return prs_compress_optimal(data.data(), data.size(), progress_fn);
}

string prs_compress_pessimal(const void* vdata, size_t size) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(vdata);

  // The worst possible encoding we can do is a literal byte when no byte with
  // the same value is within the window, or an extended copy if there is a byte
  // with the same value in the window.
  WindowIndex<0x1FFF, 1> window(in_data, size);
  LZSSInterleavedWriter w;
  for (size_t z = 0; z < size; z++) {
    auto match = window.get_best_match();
    if (match.second >= 1) {
      // Write extended copy
      int16_t offset = match.first - window.offset;
      w.write_control(false);
      w.flush_if_ready();
      w.write_control(true);
      uint16_t a = (offset << 3);
      w.write_data(a & 0xFF);
      w.write_data(a >> 8);
      w.write_data(0);
    } else {
      // Write literal
      w.write_control(true);
      w.write_data(in_data[z]);
    }
    w.flush_if_ready();
    window.advance();
  }

  // Write stop command
  w.write_control(false);
  w.flush_if_ready();
  w.write_control(true);
  w.write_data(0);
  w.write_data(0);

  return std::move(w.close());
}

PRSCompressor::PRSCompressor(
    ssize_t compression_level, ProgressCallback progress_fn)
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

  phosg::StringReader r(data, size);
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
  for (ssize_t num_literals = 0; num_literals <= this->compression_level; num_literals++) {
    for (size_t z = 0; z < static_cast<size_t>(num_literals); z++) {
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
    for (size_t z = 0; z < static_cast<size_t>(num_literals); z++) {
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
      this->progress_fn(CompressPhase::GENERATE_RESULT, this->reverse_log.end_offset(), this->input_bytes, this->output.size());
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
    ssize_t compression_level,
    ProgressCallback progress_fn) {
  PRSCompressor prs(compression_level, progress_fn);
  prs.add(vdata, size);
  return std::move(prs.close());
}

string prs_compress(
    const string& data,
    ssize_t compression_level,
    ProgressCallback progress_fn) {
  return prs_compress(data.data(), data.size(), compression_level, progress_fn);
}

string prs_compress_indexed(
    const void* in_data_v, size_t in_size, ProgressCallback progress_fn) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_v);

  LZSSInterleavedWriter w;
  WindowIndex<0x100, 5> w_short(in_data_v, in_size);
  WindowIndex<0x1FFF, 9> w_long(in_data_v, in_size);
  WindowIndex<0x1FFF, 0x100> w_extended(in_data_v, in_size);

  size_t last_progress_fn_call_offset = 0;
  while (w_short.offset < in_size) {
    if (progress_fn && ((last_progress_fn_call_offset & ~0xFFF) != (w_short.offset & ~0xFFF))) {
      last_progress_fn_call_offset = w_short.offset;
      progress_fn(CompressPhase::GENERATE_RESULT, w_short.offset, in_size, w.size());
    }

    auto m_short = w_short.get_best_match();
    auto m_long = w_long.get_best_match();
    auto m_extended = w_extended.get_best_match();

    // Write the match that achieves the best ratio of output bytes to
    // compressed bits used. To do this without floating-point math, we multiply
    // the output byte count for each type of command by 468 / (command_bits),
    // since 468 is the least common multiple of the number of bits for each
    // command type. The command type with the highest score is the one we'll
    // use, breaking ties by choosing the shorter command type. Note that the
    // size of any copy type can be zero if no match was found; if no matches
    // were found at all, then we can always write a literal.
    size_t score_literal = 52;
    size_t score_short = m_short.second * 39;
    size_t score_long = m_long.second * 26;
    size_t score_extended = m_extended.second * 18;
    PRSPathNode::CommandType command_type = PRSPathNode::CommandType::NONE;
    if (score_literal < score_short) {
      if (score_short < score_long) {
        if (score_long < score_extended) {
          command_type = PRSPathNode::CommandType::EXTENDED_COPY;
        } else {
          command_type = PRSPathNode::CommandType::LONG_COPY;
        }
      } else {
        if (score_short < score_extended) {
          command_type = PRSPathNode::CommandType::EXTENDED_COPY;
        } else {
          command_type = PRSPathNode::CommandType::SHORT_COPY;
        }
      }
    } else {
      if (score_literal < score_long) {
        if (score_long < score_extended) {
          command_type = PRSPathNode::CommandType::EXTENDED_COPY;
        } else {
          command_type = PRSPathNode::CommandType::LONG_COPY;
        }
      } else {
        if (score_literal < score_extended) {
          command_type = PRSPathNode::CommandType::EXTENDED_COPY;
        } else {
          command_type = PRSPathNode::CommandType::LITERAL;
        }
      }
    }

    size_t bytes_consumed = 0;
    switch (command_type) {
      case PRSPathNode::CommandType::LITERAL:
        w.write_control(true);
        w.write_data(in_data[w_short.offset]);
        bytes_consumed = 1;
        break;
      case PRSPathNode::CommandType::SHORT_COPY: {
        ssize_t backreference_offset = m_short.first - w_short.offset;
        uint8_t encoded_size = m_short.second - 2;
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(encoded_size & 2);
        w.flush_if_ready();
        w.write_control(encoded_size & 1);
        w.write_data(backreference_offset & 0xFF);
        bytes_consumed = m_short.second;
        break;
      }
      case PRSPathNode::CommandType::LONG_COPY: {
        ssize_t backreference_offset = m_long.first - w_long.offset;
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(true);
        uint16_t a = (backreference_offset << 3) | (m_long.second - 2);
        w.write_data(a & 0xFF);
        w.write_data(a >> 8);
        bytes_consumed = m_long.second;
        break;
      }
      case PRSPathNode::CommandType::EXTENDED_COPY: {
        ssize_t backreference_offset = m_extended.first - w_extended.offset;
        w.write_control(false);
        w.flush_if_ready();
        w.write_control(true);
        uint16_t a = (backreference_offset << 3);
        w.write_data(a & 0xFF);
        w.write_data(a >> 8);
        w.write_data(m_extended.second - 1);
        bytes_consumed = m_extended.second;
        break;
      }
      case PRSPathNode::CommandType::NONE:
      default:
        throw logic_error("invalid command type");
    }
    w.flush_if_ready();

    if (bytes_consumed == 0) {
      throw logic_error("no input data was consumed");
    }

    for (size_t z = 0; z < bytes_consumed; z++) {
      w_short.advance();
      w_long.advance();
      w_extended.advance();
    }
  }

  // Write stop command
  w.write_control(false);
  w.flush_if_ready();
  w.write_control(true);
  w.write_data(0);
  w.write_data(0);

  return std::move(w.close());
}

string prs_compress_indexed(const string& data, ProgressCallback progress_fn) {
  return prs_compress_indexed(data.data(), data.size(), progress_fn);
}

PRSDecompressResult prs_decompress_with_meta(
    const void* data, size_t size, size_t max_output_size, bool allow_unterminated) {
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

  phosg::StringWriter w;
  phosg::StringReader r(data, size);
  ControlStreamReader cr(r);

  while (!r.eof()) {
    // Control 1 = literal byte
    if (cr.read()) {
      if (max_output_size && w.size() == max_output_size) {
        if (allow_unterminated) {
          return {std::move(w.str()), r.where()};
        } else {
          throw runtime_error("maximum output size exceeded");
        }
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
          if (allow_unterminated) {
            return {std::move(w.str()), r.where()};
          } else {
            throw out_of_range("maximum output size exceeded");
          }
        }
        w.put_u8(w.str()[read_offset + z]);
      }
    }
  }

  return {std::move(w.str()), r.where()};
}

PRSDecompressResult prs_decompress_with_meta(const string& data, size_t max_output_size, bool allow_unterminated) {
  return prs_decompress_with_meta(data.data(), data.size(), max_output_size, allow_unterminated);
}

string prs_decompress(const void* data, size_t size, size_t max_output_size, bool allow_unterminated) {
  auto ret = prs_decompress_with_meta(data, size, max_output_size, allow_unterminated);
  return std::move(ret.data);
}

string prs_decompress(const string& data, size_t max_output_size, bool allow_unterminated) {
  auto ret = prs_decompress_with_meta(data.data(), data.size(), max_output_size, allow_unterminated);
  return std::move(ret.data);
}

size_t prs_decompress_size(const void* data, size_t size, size_t max_output_size, bool allow_unterminated) {
  size_t ret = 0;
  phosg::StringReader r(data, size);
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
      if (allow_unterminated) {
        return max_output_size;
      } else {
        throw out_of_range("maximum output size exceeded");
      }
    }
  }

  return ret;
}

size_t prs_decompress_size(const string& data, size_t max_output_size, bool allow_unterminated) {
  return prs_decompress_size(data.data(), data.size(), max_output_size, allow_unterminated);
}

void prs_disassemble(FILE* stream, const void* data, size_t size) {
  size_t output_bytes = 0;
  phosg::StringReader r(data, size);
  ControlStreamReader cr(r);

  while (!r.eof()) {
    uint8_t buffered_bits = cr.buffered_bits();
    if (cr.read()) {
      uint8_t literal_value = r.get_u8();
      fprintf(stream, "[%zX] %hhu> 1    %02hhX        literal %02hhX\n",
          output_bytes, buffered_bits, literal_value, literal_value);
      output_bytes++;

    } else {
      size_t count, read_offset;
      if (cr.read()) {
        uint8_t a_low = r.get_u8();
        uint8_t a_high = r.get_u8();
        uint16_t a = (a_high << 8) | a_low;
        ssize_t offset = (a >> 3) | (~0x1FFF);
        if (offset == ~0x1FFF) {
          fprintf(stream, "[%zX] end\n", output_bytes);
          break;
        }
        if (a & 7) {
          count = (a & 7) + 2;
          read_offset = output_bytes + offset;
          fprintf(stream, "[%zX] %hhu> 01   %02hhX %02hhX     long copy from %zd (offset=%zX) size=%zX\n",
              output_bytes, buffered_bits, a_low, a_high, offset, read_offset, count);
        } else {
          uint8_t count_u8 = r.get_u8();
          count = count_u8 + 1;
          read_offset = output_bytes + offset;
          fprintf(stream, "[%zX] %hhu> 01   %02hhX %02hhX %02hhX  extended copy from %zd (offset=%zX) size=%zX\n",
              output_bytes, buffered_bits, a_low, a_high, count_u8, offset, read_offset, count);
        }

      } else {
        bool first_bit = cr.read();
        bool second_bit = cr.read();
        uint8_t offset_u8 = r.get_u8();
        count = ((first_bit ? 2 : 0) | (second_bit ? 1 : 0)) + 2;
        ssize_t offset = offset_u8 | (~0xFF);
        read_offset = output_bytes + offset;
        fprintf(stream, "[%zX] %hhu> 00%c%c %02hhX        short copy from %zd (offset=%zX) size=%zX\n",
            output_bytes, buffered_bits, first_bit ? '1' : '0', second_bit ? '1' : '0', offset_u8, offset, read_offset, count);
      }

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

struct BC0PathNode {
  uint16_t memo_offset = 0;
  uint8_t max_copy_size = 0;

  // Pathfinding state
  size_t from_offset = 0;
  size_t bits_used = static_cast<size_t>(-1);

  // Stream generation state
  size_t to_offset = 0;
};

string bc0_compress_optimal(
    const void* in_data_v, size_t in_size, ProgressCallback progress_fn) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_v);

  vector<BC0PathNode> nodes;
  nodes.resize(in_size + 1);
  nodes[0].bits_used = 0;

  // Populate all possible backreferences
  {
    WindowIndex<0x1000, 0x12> window(in_data_v, in_size);
    while (window.offset < in_size) {
      if ((window.offset & 0xFFF) == 0 && progress_fn) {
        progress_fn(CompressPhase::INDEX, window.offset, in_size, 0);
      }
      auto& node = nodes[window.offset];
      auto match = window.get_best_match();
      if (match.second >= 3) {
        node.memo_offset = (match.first - 0x12) & 0xFFF;
        node.max_copy_size = match.second;
      }
      window.advance();
    }
  }

  // For each node, populate the literal value, and the best ways to get to the
  // following nodes
  for (size_t z = 0; z < in_size; z++) {
    if ((z & 0xFFF) == 0 && progress_fn) {
      progress_fn(CompressPhase::CONSTRUCT_PATHS, z, in_size, 0);
    }

    auto& node = nodes[z];

    // Literal: 1 control bit + 1 data byte
    size_t bits_used = node.bits_used + 9;
    {
      auto& next_node = nodes[z + 1];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.bits_used = bits_used;
      }
    }

    // Backreference: 1 control bit + 2 data bytes
    bits_used = node.bits_used + 17;
    for (size_t x = 3; x <= node.max_copy_size; x++) {
      auto& next_node = nodes[z + x];
      if (next_node.bits_used > bits_used) {
        next_node.from_offset = z;
        next_node.bits_used = bits_used;
      }
    }
  }

  // Find the shortest path from the last node to the first node
  size_t last_progress_fn_call = static_cast<size_t>(-1);
  for (size_t z = in_size; z > 0;) {
    if ((z & ~0xFFF) != (last_progress_fn_call & ~0xFFF)) {
      last_progress_fn_call = z;
      if (progress_fn) {
        progress_fn(CompressPhase::BACKTRACE_OPTIMAL_PATH, z, in_size, 0);
      }
    }
    size_t from_offset = nodes[z].from_offset;
    nodes[from_offset].to_offset = z;
    z = from_offset;
  }

  // Produce the BC0 command stream from the shortest path
  LZSSInterleavedWriter w;
  last_progress_fn_call = static_cast<size_t>(-1);
  for (size_t offset = 0; offset < in_size;) {
    if ((offset & ~0xFFF) != (last_progress_fn_call & ~0xFFF)) {
      last_progress_fn_call = offset;
      if (progress_fn) {
        progress_fn(CompressPhase::GENERATE_RESULT, offset, in_size, w.size());
      }
    }

    const auto& node = nodes[offset];
    size_t copy_size = node.to_offset - offset;
    if (copy_size >= 3 && copy_size <= 0x12) {
      w.write_control(false);
      w.write_data(node.memo_offset & 0xFF);
      w.write_data(((node.memo_offset >> 4) & 0xF0) | (copy_size - 3));
    } else if (copy_size == 1) {
      w.write_control(true);
      w.write_data(in_data[offset]);
    }
    w.flush_if_ready();

    offset = node.to_offset;
  }

  return std::move(w.close());
}

string bc0_compress(const string& data, ProgressCallback progress_fn) {
  return bc0_compress(data.data(), data.size(), progress_fn);
}

string bc0_compress(const void* in_data_v, size_t in_size, ProgressCallback progress_fn) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_v);

  LZSSInterleavedWriter w;
  WindowIndex<0x1000, 0x12> window(in_data_v, in_size);

  size_t last_progress_fn_call_offset = 0;
  while (window.offset < in_size) {
    if (progress_fn && ((last_progress_fn_call_offset & ~0xFFF) != (window.offset & ~0xFFF))) {
      last_progress_fn_call_offset = window.offset;
      progress_fn(CompressPhase::GENERATE_RESULT, window.offset, in_size, w.size());
    }

    auto match = window.get_best_match();

    // Write a backreference if a match was found; otherwise, write a literal
    if (match.second >= 3) {
      w.write_control(false);
      size_t memo_offset = match.first - 0x12;
      w.write_data(memo_offset & 0xFF);
      w.write_data(((memo_offset >> 4) & 0xF0) | (match.second - 3));
    } else {
      w.write_control(true);
      w.write_data(in_data[window.offset]);
      match.second = 1;
    }
    w.flush_if_ready();

    for (size_t z = 0; z < match.second; z++) {
      window.advance();
    }
  }

  return std::move(w.close());
}

string bc0_encode(const void* in_data_v, size_t in_size) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_v);

  LZSSInterleavedWriter w;
  for (size_t z = 0; z < in_size; z++) {
    w.write_control(true);
    w.write_data(in_data[z]);
    w.flush_if_ready();
  }

  return std::move(w.close());
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
  phosg::StringReader r(data, size);
  phosg::StringWriter w;

  // Unlike PRS, BC0 uses a memo which "rolls over" every 0x1000 bytes. The
  // boundaries of these "memo pages" are offset by -0x12 bytes for some reason,
  // so the first output byte corresponds to position 0xFEE on the first memo
  // page. Backreferences refer to offsets based on the start of memo pages; for
  // example, if the current output offset is 0x1234, a backreference with
  // offset 0x123 refers to the byte that was written at offset 0x1111 (because
  // that byte is at offset 0x111 in the memo, because the memo rolls over every
  // 0x1000 bytes and the first memo byte was 0x12 bytes before the beginning of
  // the next page). The memo is initially zeroed from 0 to 0xFEE; it seems PSO
  // GC doesn't initialize the last 0x12 bytes of the first memo page.
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

    if ((control_stream_bits & 1) == 0) {
      // Control bit 0 means to perform a backreference copy. The offset and
      // size are stored in two bytes in the input stream, laid out as follows:
      // a1 = 0bBBBBBBBB
      // a2 = 0bAAAACCCC
      // The offset is the concatenation of bits AAAABBBBBBBB, which refers to
      // a position in the memo; the number of bytes to copy is (CCCC + 3). The
      // decompressor copies that many bytes from that offset in the memo, and
      // writes them to the output and to the current position in the memo.
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

    } else {
      // Control bit 1 means to write a byte directly from the input to the
      // output. As above, the byte is also written to the memo.
      uint8_t v = r.get_u8();
      w.put_u8(v);
      memo[memo_offset] = v;
      memo_offset = (memo_offset + 1) & 0x0FFF;
    }
  }

  return std::move(w.str());
}

void bc0_disassemble(FILE* stream, const string& data) {
  bc0_disassemble(stream, data.data(), data.size());
}

void bc0_disassemble(FILE* stream, const void* data, size_t size) {
  phosg::StringReader r(data, size);
  uint16_t control_stream_bits = 0x0000;

  size_t output_bytes = 0;
  while (!r.eof()) {
    // size_t opcode_offset = r.where();

    control_stream_bits >>= 1;
    if ((control_stream_bits & 0x100) == 0) {
      control_stream_bits = 0xFF00 | r.get_u8();
      if (r.eof()) {
        break;
      }
    }

    if ((control_stream_bits & 1) == 0) {
      uint8_t a1 = r.get_u8();
      if (r.eof()) {
        break;
      }
      (void)a1;
      uint8_t a2 = r.get_u8();
      size_t count = (a2 & 0x0F) + 3;
      // size_t backreference_offset = a1 | ((a2 << 4) & 0xF00);
      fprintf(stream, "[%zX] backreference %02zX\n", output_bytes, count);
      output_bytes += count;

    } else {
      fprintf(stream, "[%zX] literal %02hhX\n", output_bytes, r.get_u8());
      output_bytes++;
    }
  }
}
