#pragma once

#include <stddef.h>

#include <string>
#include <functional>
#include <set>

#include "Text.hh"



// Use this class if you need to compress from multiple input buffers, or need
// to compress multiple chunks and don't want to copy their contents
// unnecessarily. (For most common use cases, use prs_compress (below) instead.)
class PRSCompressor {
public:
  // To use this class, instantiate it, then call .add() one or more times, then
  // call .close() and use the returned string as the compressed result.
  PRSCompressor(std::function<void(size_t, size_t)> progress_fn = nullptr);
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
  void add_byte(uint8_t v);
  void advance();
  void write_control(bool z);
  void flush_control();

  std::function<void(size_t, size_t)> progress_fn;
  bool closed;

  size_t control_byte_offset;
  uint16_t pending_control_bits;

  size_t input_bytes;
  parray<uint8_t, 0x100> forward_log;
  size_t compression_offset;
  parray<uint8_t, 0x2000> reverse_log;
  std::vector<std::set<size_t>> reverse_log_index;

  StringWriter output;
};

// Compresses data from a single input buffer using PRS and returns the
// compressed result. This is a shortcut for constructing a PRSCompressor,
// calling .add() once, and calling .close().
std::string prs_compress(const void* vdata, size_t size, std::function<void(size_t, size_t)> progress_fn = nullptr);
std::string prs_compress(const std::string& data, std::function<void(size_t, size_t)> progress_fn = nullptr);

// Decompresses PRS-compressed data.
std::string prs_decompress(const void* data, size_t size, size_t max_output_size = 0);
std::string prs_decompress(const std::string& data, size_t max_output_size = 0);

// Returns the decompressed size of PRS-compressed data, without actually
// decompressing it.
size_t prs_decompress_size(const void* data, size_t size, size_t max_output_size = 0);
size_t prs_decompress_size(const std::string& data, size_t max_output_size = 0);

// Compresses and decompresses data using the BC0 algorithm.
std::string bc0_compress(const std::string& data, std::function<void(size_t, size_t)> progress_fn = nullptr);
std::string bc0_decompress(const std::string& data);
