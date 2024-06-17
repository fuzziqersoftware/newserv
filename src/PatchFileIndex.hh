#pragma once

#include <inttypes.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct PatchFileIndex {
  explicit PatchFileIndex(const std::string& root_dir);

  struct File {
    PatchFileIndex* index;
    std::vector<std::string> path_directories;
    std::string name;
    std::shared_ptr<const std::string> loaded_data;
    std::vector<uint32_t> chunk_crcs;
    uint32_t crc32;
    uint32_t size;

    explicit File(PatchFileIndex* index);
    std::shared_ptr<const std::string> load_data();
  };

  const std::vector<std::shared_ptr<File>>& all_files() const;
  std::shared_ptr<File> get(const std::string& filename) const;

private:
  std::vector<std::shared_ptr<File>> files_by_patch_order;
  std::unordered_map<std::string, std::shared_ptr<File>> files_by_name;
  std::string root_dir;
};

struct PatchFileChecksumRequest {
  std::shared_ptr<PatchFileIndex::File> file;
  uint32_t crc32;
  uint32_t size;
  bool response_received;

  explicit PatchFileChecksumRequest(std::shared_ptr<PatchFileIndex::File> file)
      : file(file),
        crc32(0),
        size(0),
        response_received(false) {}
  inline bool needs_update() const {
    return !this->response_received ||
        (this->crc32 != this->file->crc32) ||
        (this->size != this->file->size);
  }
};
