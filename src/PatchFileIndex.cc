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

PatchFileIndex::PatchFileIndex(const string& root_dir)
  : root_dir(root_dir) {

  string metadata_cache_filename = root_dir + "/.metadata-cache.json";
  shared_ptr<JSONObject> metadata_cache_json;
  try {
    string metadata_text = load_file(metadata_cache_filename);
    metadata_cache_json = JSONObject::parse(metadata_text);
    patch_index_log.info("Loaded patch metadata cache from %s", metadata_cache_filename.c_str());
  } catch (const exception& e) {
    metadata_cache_json = make_json_dict({});
    patch_index_log.warning("Cannot load patch metadata cache from %s: %s", metadata_cache_filename.c_str(), e.what());
  }

  // Assuming it's rare for patch files to change, we skip writing the metadata
  // cache if no files were changed at all (which should usually be the case)
  bool should_write_metadata_cache = false;
  shared_ptr<JSONObject> new_metadata_cache_json = make_json_dict({});

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

        auto st = stat(full_item_path);

        shared_ptr<File> f(new File());
        f->path_directories = path_directories;
        f->name = item;

        int64_t file_crc32 = -1;
        try {
          auto cache_item = metadata_cache_json->at(relative_item_path)->as_list();
          uint64_t cached_size = cache_item.at(0)->as_int();
          uint64_t cached_mtime = cache_item.at(1)->as_int();
          if (static_cast<uint64_t>(st.st_mtime) != cached_mtime) {
            throw runtime_error("file has been modified");
          }
          if (static_cast<uint64_t>(st.st_size) != cached_size) {
            throw runtime_error("file size has changed");
          }
          file_crc32 = cache_item.at(2)->as_int();
          patch_index_log.info("Using cached checksum for %s", relative_item_path.c_str());
        } catch (const exception& e) {
          should_write_metadata_cache = true;
          patch_index_log.info("Cannot use cached checksum for %s: %s", relative_item_path.c_str(), e.what());
        }

        string file_data = load_file(full_item_path);
        f->data.reset(new string(move(file_data)));
        f->crc32 = (file_crc32 < 0)
            ? ::crc32(f->data->data(), f->data->size()) : file_crc32;
        for (size_t x = 0; x < f->data->size(); x += 0x4000) {
          size_t chunk_bytes = min<size_t>(f->data->size() - x, 0x4000);
          f->chunk_crcs.emplace_back(::crc32(f->data->data() + x, chunk_bytes));
        }

        vector<shared_ptr<JSONObject>> new_cache_item({
          make_json_int(f->data->size()),
          make_json_int(st.st_mtime),
          make_json_int(f->crc32),
        });
        new_metadata_cache_json->as_dict().emplace(
            relative_item_path, make_json_list(move(new_cache_item)));

        this->files_by_patch_order.emplace_back(f);
        this->files_by_name.emplace(relative_item_path, f);
        patch_index_log.info("Added file %s (%zu bytes; %zu chunks; %08" PRIX32 ")",
            full_item_path.c_str(), f->data->size(), f->chunk_crcs.size(), f->crc32);
      }
    }

    path_directories.pop_back();
  };

  collect_dir(".");

  if (should_write_metadata_cache) {
    try {
      save_file(metadata_cache_filename, new_metadata_cache_json->serialize());
      patch_index_log.info("Saved patch metadata cache to %s", metadata_cache_filename.c_str());
    } catch (const exception& e) {
      patch_index_log.warning("Cannot save patch metadata cache to %s: %s", metadata_cache_filename.c_str(), e.what());
    }
  } else {
    patch_index_log.info("No files were modified; skipping metadata cache update");
  }
}

const vector<shared_ptr<const PatchFileIndex::File>>&
PatchFileIndex::all_files() const {
  return this->files_by_patch_order;
}

shared_ptr<const PatchFileIndex::File> PatchFileIndex::get(
    const string& filename) const {
  return this->files_by_name.at(filename);
}
