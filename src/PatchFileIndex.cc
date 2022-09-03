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



PatchFileIndex::File::File() : crc32(0) { }

void PatchFileIndex::File::load_data(const string& root_dir) {
  string full_path = root_dir + '/' + join(this->path_directories, "/") + '/' + this->name;
  auto f = fopen_unique(full_path, "rb");

  string file_data = load_file(full_path);
  this->data.reset(new string(move(file_data)));
  this->crc32 = ::crc32(this->data->data(), this->data->size());
  for (size_t x = 0; x < this->data->size(); x += 0x4000) {
    size_t chunk_bytes = min<size_t>(this->data->size() - x, 0x4000);
    this->chunk_crcs.emplace_back(::crc32(this->data->data() + x, chunk_bytes));
  }
}

PatchFileIndex::PatchFileIndex(const string& root_dir)
  : root_dir(root_dir) {

  vector<string> path_directories;
  function<void(const string&)> collect_dir = [&](const string& dir) -> void {
    path_directories.emplace_back(dir);

    string relative_dirs = join(path_directories, "/");
    string full_dir_path = root_dir + '/' + relative_dirs;
    patch_index_log.info("Listing directory %s", full_dir_path.c_str());

    for (const auto& item : list_directory(full_dir_path)) {
      // Skip invisible files (e.g. .DS_Store on macOS)
      if (starts_with(item, ".")) {
        continue;
      }

      string relative_item_path = relative_dirs + '/' + item;
      string full_item_path = root_dir + '/' + relative_item_path;
      if (isdir(full_item_path)) {
        collect_dir(item);
      } else if (isfile(full_item_path)) {
        shared_ptr<File> f(new File());
        f->path_directories = path_directories;
        f->name = item;
        f->load_data(root_dir);
        this->files_by_patch_order.emplace_back(f);
        this->files_by_name.emplace(relative_item_path, f);
        patch_index_log.info("Added file %s (%zu bytes; %zu chunks; %08" PRIX32 ")",
            full_item_path.c_str(), f->data->size(), f->chunk_crcs.size(), f->crc32);
      }
    }

    path_directories.pop_back();
  };

  collect_dir(".");
}

const vector<shared_ptr<const PatchFileIndex::File>>&
PatchFileIndex::all_files() const {
  return this->files_by_patch_order;
}

shared_ptr<const PatchFileIndex::File> PatchFileIndex::get(
    const string& filename) const {
  return this->files_by_name.at(filename);
}
