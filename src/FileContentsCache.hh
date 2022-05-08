#pragma once

#include <memory>
#include <string>
#include <unordered_map>

using namespace std;


class FileContentsCache {
private:
  struct File {
    std::string name;
    std::shared_ptr<const std::string> contents;
    uint64_t load_time;

    File() = delete;
    File(const std::string& name, std::shared_ptr<const std::string> contents,
        uint64_t load_time);
    File(const File&) = delete;
    File(File&&) = delete;
    File& operator=(const File&) = delete;
    File& operator=(File&&) = delete;
    ~File() = default;
  };

public:
  FileContentsCache() = default;
  FileContentsCache(const FileContentsCache&) = delete;
  FileContentsCache(FileContentsCache&&) = delete;
  FileContentsCache& operator=(const FileContentsCache&) = delete;
  FileContentsCache& operator=(FileContentsCache&&) = delete;
  ~FileContentsCache() = default;

  std::shared_ptr<const std::string> get(const std::string& name);
  std::shared_ptr<const std::string> get(const char* name);

  std::shared_ptr<const std::string> get(
      const std::string& name, std::function<std::string()> generate);
  std::shared_ptr<const std::string> get(
      const char* name, std::function<std::string()> generate);

private:
  std::unordered_map<std::string, File> name_to_file;
};
