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
    std::vector<std::string> path_directories;
    std::string name;
    std::shared_ptr<const std::string> data;
    std::vector<uint32_t> chunk_crcs;
    uint32_t crc32;

    File();
  };

  const std::vector<std::shared_ptr<const File>>& all_files() const;
  std::shared_ptr<const File> get(const std::string& filename) const;

private:
  std::vector<std::shared_ptr<const File>> files_by_patch_order;
  std::unordered_map<std::string, std::shared_ptr<const File>> files_by_name;
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
           (this->size != this->file->data->size());
  }
};
