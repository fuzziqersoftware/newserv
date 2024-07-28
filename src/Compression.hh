#pragma once

#include <stddef.h>

#include <array>
#include <deque>
#include <functional>
#include <phosg/Tools.hh>
#include <string>

#include "Text.hh"

enum class CompressPhase {
  INDEX = 0,
  CONSTRUCT_PATHS,
  BACKTRACE_OPTIMAL_PATH,
  GENERATE_RESULT,
};

template <>
const char* phosg::name_for_enum<CompressPhase>(CompressPhase v);

typedef std::function<void(CompressPhase phase, size_t input_progress, size_t input_size, size_t output_size)> ProgressCallback;

////////////////////////////////////////////////////////////////////////////////
// PRS compression
////////////////////////////////////////////////////////////////////////////////

// Use this class if you need to compress from multiple input buffers, or need
// to compress multiple chunks and don't want to copy their contents
// unnecessarily. (For most common use cases, use prs_compress, below, instead.)
// To use this class, instantiate it, then call .add() one or more times, then
// call .close() and use the returned string as the compressed result.
class PRSCompressor {
public:
  // compression_level specifies how aggressively to search for alternate paths:
  //   -1: Don't perform any compression at all, but produce output that can be
  //       understood by prs_decompress. The output will be about 9/8 the size
  //       of the input.
  //   0:  Greedily search for the longest backreference at every point. Don't
  //       consider any alternate paths. Generally offers a good balance between
  //       speed and output size.
  //   1:  Consider two paths at each point when a backreference is found: using
  //       the backreference or ignoring it.
  //   2+: Consider further chains of paths at each point. Using values 2 or
  //       greater for compression_level generally yields diminishing returns.
  explicit PRSCompressor(ssize_t compression_level = 0, ProgressCallback progress_fn = nullptr);
  ~PRSCompressor() = default;

  // Adds more input data to be compressed, which logically comes after all
  // previous data provided via add() calls. Cannot be called after close() is
  // called.
  void add(const void* data, size_t size);
  void add(const std::string& data);

  // Ends compression and returns the complete compressed result. It's OK to
  // std::move() from the returned string reference.
  std::string& close();

  // Returns the total number of bytes passed to add() calls so far.
  inline size_t input_size() const {
    return this->input_bytes;
  }

private:
  template <size_t Size>
  struct WrappedLog {
    parray<uint8_t, Size> data;

    WrappedLog() : data(0) {}
    ~WrappedLog() = default;

    inline uint8_t at(size_t offset) const {
      return this->data[offset % this->data.size()];
    }
    inline uint8_t& at(size_t offset) {
      return this->data[offset % this->data.size()];
    }
  };

  template <size_t Size>
  struct IndexedLog : WrappedLog<Size> {
    size_t offset;
    size_t size;
    std::array<std::deque<size_t>, 0x100> index;

    IndexedLog()
        : WrappedLog<Size>(),
          offset(0),
          size(0) {}
    ~IndexedLog() = default;

    inline size_t end_offset() const {
      return this->offset + this->size;
    }

    void push_back(uint8_t v) {
      if (this->size == Size) {
        this->pop_front();
      }
      size_t write_offset = this->offset + this->size;
      this->at(write_offset) = v;
      this->index[v].push_back(write_offset);
      this->size++;
    }
    uint8_t pop_back() {
      if (!this->size) {
        throw std::logic_error("pop_back called on empty IndexedLog");
      }
      this->size--;
      size_t offset = this->offset + this->size;
      uint8_t v = this->at(offset);
      this->index[v].pop_back();
      return v;
    }
    uint8_t pop_front() {
      uint8_t v = this->at(this->offset);
      this->index[v].pop_front();
      this->offset++;
      this->size--;
      return v;
    }
    const std::deque<size_t>& find(uint8_t v) {
      return this->index[v];
    }
  };

  void add_byte(uint8_t v);
  void advance();
  void move_forward_data_to_reverse_log(size_t size);
  void advance_literal();
  void advance_short_copy(ssize_t offset, size_t size);
  void advance_long_copy(ssize_t offset, size_t size);
  void advance_extended_copy(ssize_t offset, size_t size);
  void write_control(bool z);
  void flush_control();

  ssize_t compression_level;
  ProgressCallback progress_fn;
  bool closed;

  size_t control_byte_offset;
  uint16_t pending_control_bits;

  size_t input_bytes;
  WrappedLog<0x101> forward_log;
  IndexedLog<0x2000> reverse_log;

  phosg::StringWriter output;
};

// These functions use PRSCompressor to compress a buffer of data. This is
// essentially a shortcut for constructing a PRSCompressor, calling .add() on
// it once, then calling .close().
std::string prs_compress(
    const void* vdata,
    size_t size,
    ssize_t compression_level = 0,
    ProgressCallback progress_fn = nullptr);
std::string prs_compress(
    const std::string& data,
    ssize_t compression_level = 0,
    ProgressCallback progress_fn = nullptr);

// A faster form of prs_compress that doesn't have a tunable compression level.
std::string prs_compress_indexed(
    const void* vdata,
    size_t size,
    ProgressCallback progress_fn = nullptr);
std::string prs_compress_indexed(
    const std::string& data,
    ProgressCallback progress_fn = nullptr);

// Compresses data using PRS to the smallest possible output size. This function
// is slow, but produces results significantly smaller than even Sega's original
// compressor.
std::string prs_compress_optimal(const void* vdata, size_t size, ProgressCallback progress_fn = nullptr);
std::string prs_compress_optimal(const std::string& data, ProgressCallback progress_fn = nullptr);

// Compresses data using PRS to the LARGEST possible output size. There is no
// practical use for this function except for amusement.
std::string prs_compress_pessimal(const void* vdata, size_t size);

// Decompresses PRS-compressed data.
struct PRSDecompressResult {
  std::string data;
  size_t input_bytes_used;
};
PRSDecompressResult prs_decompress_with_meta(const void* data, size_t size, size_t max_output_size = 0, bool allow_unterminated = false);
PRSDecompressResult prs_decompress_with_meta(const std::string& data, size_t max_output_size = 0, bool allow_unterminated = false);
std::string prs_decompress(const void* data, size_t size, size_t max_output_size = 0, bool allow_unterminated = false);
std::string prs_decompress(const std::string& data, size_t max_output_size = 0, bool allow_unterminated = false);

// Returns the decompressed size of PRS-compressed data, without actually
// decompressing it.
size_t prs_decompress_size(const void* data, size_t size, size_t max_output_size = 0, bool allow_unterminated = false);
size_t prs_decompress_size(const std::string& data, size_t max_output_size = 0, bool allow_unterminated = false);

// Prints the command stream from a PRS-compressed buffer.
void prs_disassemble(FILE* stream, const void* data, size_t size);
void prs_disassemble(FILE* stream, const std::string& data);

////////////////////////////////////////////////////////////////////////////////
// BC0 compression
////////////////////////////////////////////////////////////////////////////////

// Compresses data using the BC0 algorithm. Like with PRS, the optimal variant
// is slow, but produces the smallest possible output.
std::string bc0_compress_optimal(
    const void* in_data_v,
    size_t in_size,
    ProgressCallback progress_fn = nullptr);
std::string bc0_compress(const std::string& data, ProgressCallback progress_fn = nullptr);
std::string bc0_compress(const void* in_data_v, size_t in_size, ProgressCallback progress_fn = nullptr);

// Encodes data in a BC0-compatible format without compression (similar to using
// compression_level=-1 with prs_compress).
std::string bc0_encode(const void* in_data_v, size_t in_size);

// Decompresses BC0-compressed data.
std::string bc0_decompress(const std::string& data);
std::string bc0_decompress(const void* data, size_t size);

// Prints the command stream from a BC0-compressed buffer.
void bc0_disassemble(FILE* stream, const std::string& data);
void bc0_disassemble(FILE* stream, const void* data, size_t size);
