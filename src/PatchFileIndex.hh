#pragma once

#include <inttypes.h>

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>



struct PatchFileIndex {
  explicit PatchFileIndex(const std::string& root_dir);

  struct File {
    struct Chunk {
      std::string data;
      uint32_t crc32;
    };
    std::vector<std::string> path_directories;
    std::string name;
    std::vector<Chunk> chunks;
    size_t size;
    uint32_t crc32;

    File() : size(0), crc32(0) { }
    void load_data(const std::string& root_dir);
  };

  std::vector<std::shared_ptr<File>> files;
  std::string root_dir;
};

struct PatchFileChecksumRequest {
  std::shared_ptr<const PatchFileIndex::File> file;
  uint32_t crc32;
  uint32_t size;
  bool response_received;

  explicit PatchFileChecksumRequest(std::shared_ptr<const PatchFileIndex::File> file)
    : file(file), crc32(0), size(0), response_received(false) { }
  inline bool needs_update() const {
    return !this->response_received ||
           (this->crc32 != this->file->crc32) ||
           (this->size != this->file->size);
  }
};
