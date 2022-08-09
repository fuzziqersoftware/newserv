#include "PatchFileIndex.hh"

#include <stdio.h>
#include <string.h>

#include <functional>
#include <stdexcept>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"

using namespace std;



void PatchFileIndex::File::load_data(const string& root_dir) {
  string full_path = root_dir + '/' + join(this->path_directories, "/") + '/' + this->name;
  auto f = fopen_unique(full_path, "rb");

  this->size = 0;
  this->crc32 = 0;
  while (!feof(f.get())) {
    auto& chunk = this->chunks.emplace_back();
    chunk.data = fread(f.get(), 0x4000);
    this->size += chunk.data.size();
    this->crc32 = ::crc32(chunk.data.data(), chunk.data.size(), this->crc32);
  }
}

PatchFileIndex::PatchFileIndex(const string& root_dir)
  : root_dir(root_dir) {

  vector<string> path_directories;
  function<void(const string&)> collect_dir = [&](const string& dir) -> void {
    path_directories.emplace_back(dir);

    string full_dir_path = root_dir + '/' + join(path_directories, "/");
    patch_index_log.info("Listing directory %s", full_dir_path.c_str());

    for (const auto& item : list_directory(full_dir_path)) {
      // Skip invisible files (e.g. .DS_Store on macOS)
      if (starts_with(item, ".")) {
        continue;
      }

      string full_item_path = full_dir_path + '/' + item;
      if (isdir(full_item_path)) {
        collect_dir(item);
      } else if (isfile(full_item_path)) {
        shared_ptr<File> f(new File());
        f->path_directories = path_directories;
        f->name = item;
        f->load_data(root_dir);
        this->files.emplace_back(f);
        patch_index_log.info("Added file %s (%zu bytes; %zu chunks; %08" PRIX32 ")",
            full_item_path.c_str(), f->size, f->chunks.size(), f->crc32);
      }
    }

    path_directories.pop_back();
  };

  collect_dir(".");
}
