#include "PatchFileIndex.hh"

#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <functional>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Strings.hh>
#include <stdexcept>

#include "Loggers.hh"

using namespace std;

int64_t file_mtime_int(const std::string& path) {
  auto mtime = std::filesystem::last_write_time(path);
  auto sctp = std::chrono::time_point_cast<std::chrono::nanoseconds>(
      mtime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  return sctp.time_since_epoch().count();
}

PatchFileIndex::File::File(PatchFileIndex* index)
    : index(index),
      crc32(0),
      size(0) {}

std::shared_ptr<const std::string> PatchFileIndex::File::load_data() {
  if (!this->loaded_data) {
    string relative_path = phosg::join(this->path_directories, "/") + "/" + this->name;
    string full_path = this->index->root_dir + "/" + relative_path;
    patch_index_log.info_f("Loading data for {}", relative_path);
    this->loaded_data = make_shared<string>(phosg::load_file(full_path));
    this->size = this->loaded_data->size();
  }
  return this->loaded_data;
}

PatchFileIndex::PatchFileIndex(const string& root_dir)
    : root_dir(root_dir) {

  string metadata_cache_filename = root_dir + "/.metadata-cache.json";
  phosg::JSON metadata_cache_json;
  try {
    string metadata_text = phosg::load_file(metadata_cache_filename);
    metadata_cache_json = phosg::JSON::parse(metadata_text);
    patch_index_log.info_f("Loaded patch metadata cache from {}", metadata_cache_filename);
  } catch (const exception& e) {
    metadata_cache_json = phosg::JSON::dict();
    patch_index_log.warning_f("Cannot load patch metadata cache from {}: {}", metadata_cache_filename, e.what());
  }

  // Assuming it's rare for patch files to change, we skip writing the metadata
  // cache if no files were changed at all (which should usually be the case)
  bool should_write_metadata_cache = false;
  phosg::JSON new_metadata_cache_json = phosg::JSON::dict();

  vector<string> path_directories;
  function<void(const string&)> collect_dir = [&](const string& dir) -> void {
    path_directories.emplace_back(dir);

    string relative_dirs = phosg::join(path_directories, "/");
    string full_dir_path = root_dir + '/' + relative_dirs;
    patch_index_log.info_f("Listing directory {}", full_dir_path);

    for (const auto& dir_item : std::filesystem::directory_iterator(full_dir_path)) {
      string item = dir_item.path().filename().string();

      // Skip invisible files (e.g. .DS_Store on macOS)
      if (item.starts_with(".")) {
        continue;
      }

      string relative_item_path = relative_dirs + '/' + item;
      string full_item_path = root_dir + '/' + relative_item_path;
      if (std::filesystem::is_directory(full_item_path)) {
        collect_dir(item);
      } else if (std::filesystem::is_regular_file(full_item_path)) {

        auto f = make_shared<File>(this);
        f->path_directories = path_directories;
        f->name = item;

        int64_t file_mtime = file_mtime_int(full_item_path);

        string compute_crc32s_message; // If not empty, should compute crc32s
        phosg::JSON cache_item_json;
        try {
          cache_item_json = metadata_cache_json.at(relative_item_path);
          uint64_t cached_size = cache_item_json.get_int(0);
          int64_t cached_mtime = cache_item_json.get_int(1);
          if (file_mtime != cached_mtime) {
            throw runtime_error("file has been modified");
          }
          if (std::filesystem::file_size(full_item_path) != cached_size) {
            throw runtime_error("file size has changed");
          }
          f->size = cached_size;
          f->crc32 = cache_item_json.get_int(2);
          for (const auto& chunk_crc32_json : cache_item_json.get_list(3)) {
            f->chunk_crcs.emplace_back(chunk_crc32_json->as_int());
          }

        } catch (const exception& e) {
          compute_crc32s_message = e.what();
        }

        if (!compute_crc32s_message.empty()) {
          auto data = f->load_data(); // Sets f->size
          f->crc32 = phosg::crc32(data->data(), f->size);
          for (size_t x = 0; x < data->size(); x += 0x4000) {
            size_t chunk_bytes = min<size_t>(f->size - x, 0x4000);
            f->chunk_crcs.emplace_back(phosg::crc32(data->data() + x, chunk_bytes));
          }

          // File was modified or cache item was missing; make a new cache item
          auto chunk_crcs_item = phosg::JSON::list();
          for (uint32_t chunk_crc : f->chunk_crcs) {
            chunk_crcs_item.emplace_back(chunk_crc);
          }
          new_metadata_cache_json.emplace(
              relative_item_path, phosg::JSON::list({f->size, file_mtime, f->crc32, std::move(chunk_crcs_item)}));
          should_write_metadata_cache = true;

        } else {
          // File was not modified and cache item was valid; just use the
          // existing cache item
          new_metadata_cache_json.emplace(
              relative_item_path, std::move(cache_item_json));
        }

        this->files_by_patch_order.emplace_back(f);
        this->files_by_name.emplace(relative_item_path, f);
        if (compute_crc32s_message.empty()) {
          patch_index_log.info_f("Added file {} ({} bytes; {} chunks; {:08X} from cache)",
              full_item_path, f->size, f->chunk_crcs.size(), f->crc32);
        } else {
          patch_index_log.info_f("Added file {} ({} bytes; {} chunks; {:08X} [{}])",
              full_item_path, f->size, f->chunk_crcs.size(), f->crc32, compute_crc32s_message);
        }
      }
    }

    path_directories.pop_back();
  };

  collect_dir(".");

  if (should_write_metadata_cache) {
    try {
      phosg::save_file(metadata_cache_filename, new_metadata_cache_json.serialize());
      patch_index_log.info_f("Saved patch metadata cache to {}", metadata_cache_filename);
    } catch (const exception& e) {
      patch_index_log.warning_f("Cannot save patch metadata cache to {}: {}", metadata_cache_filename, e.what());
    }
  } else {
    patch_index_log.info_f("No files were modified; skipping metadata cache update");
  }
}

const vector<shared_ptr<PatchFileIndex::File>>&
PatchFileIndex::all_files() const {
  return this->files_by_patch_order;
}

shared_ptr<PatchFileIndex::File> PatchFileIndex::get(
    const string& filename) const {
  return this->files_by_name.at(filename);
}
