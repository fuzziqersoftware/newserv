#pragma once

#include <stddef.h>

#include <array>
#include <deque>
#include <functional>
#include <string>

#include "Text.hh"

// Use this class if you need to compress from multiple input buffers, or need
// to compress multiple chunks and don't want to copy their contents
// unnecessarily. (For most common use cases, use prs_compress (below) instead.)
class PRSCompressor {
public:
  // To use this class, instantiate it, then call .add() one or more times, then
  // call .close() and use the returned string as the compressed result.
  explicit PRSCompressor(size_t compression_level = 1, std::function<void(size_t, size_t)> progress_fn = nullptr);
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

  size_t compression_level;
  std::function<void(size_t, size_t)> progress_fn;
  bool closed;

  size_t control_byte_offset;
  uint16_t pending_control_bits;

  size_t input_bytes;
  WrappedLog<0x101> forward_log;
  IndexedLog<0x2000> reverse_log;

  StringWriter output;
};

// Compresses data from a single input buffer using PRS and returns the
// compressed result. This is a shortcut for constructing a PRSCompressor,
// calling .add() once, and calling .close().
std::string prs_compress(
    const void* vdata,
    size_t size,
    size_t compression_level = 1,
    std::function<void(size_t, size_t)> progress_fn = nullptr);
std::string prs_compress(
    const std::string& data,
    size_t compression_level = 1,
    std::function<void(size_t, size_t)> progress_fn = nullptr);

// Decompresses PRS-compressed data.
std::string prs_decompress(const void* data, size_t size, size_t max_output_size = 0);
std::string prs_decompress(const std::string& data, size_t max_output_size = 0);

// Returns the decompressed size of PRS-compressed data, without actually
// decompressing it.
size_t prs_decompress_size(const void* data, size_t size, size_t max_output_size = 0);
size_t prs_decompress_size(const std::string& data, size_t max_output_size = 0);

// Prints the command stream from a PRS-compressed buffer.
void prs_disassemble(FILE* stream, const void* data, size_t size);
void prs_disassemble(FILE* stream, const std::string& data);

// Compresses and decompresses data using the BC0 algorithm.
std::string bc0_compress(const std::string& data, std::function<void(size_t, size_t)> progress_fn = nullptr);
std::string bc0_decompress(const std::string& data);
