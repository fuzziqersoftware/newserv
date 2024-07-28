#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <phosg/Time.hh>

class FileContentsCache {
public:
  struct File {
    std::string name;
    std::shared_ptr<const std::string> data;
    uint64_t load_time;

    File() = delete;
    File(const std::string& name, std::string&& contents, uint64_t load_time);
    File(const File&) = delete;
    File(File&&) = delete;
    File& operator=(const File&) = delete;
    File& operator=(File&&) = delete;
    ~File() = default;
  };

  explicit FileContentsCache(uint64_t ttl_usecs);
  FileContentsCache(const FileContentsCache&) = delete;
  FileContentsCache(FileContentsCache&&) = delete;
  FileContentsCache& operator=(const FileContentsCache&) = delete;
  FileContentsCache& operator=(FileContentsCache&&) = delete;
  ~FileContentsCache() = default;

  template <typename NameT>
  bool delete_key(NameT key) {
    return this->name_to_file.erase(key);
  }

  std::shared_ptr<const File> replace(const std::string& name, std::string&& data, uint64_t t = 0);
  std::shared_ptr<const File> replace(const std::string& name, const void* data, size_t size, uint64_t t = 0);

  struct GetResult {
    std::shared_ptr<const File> file;
    bool generate_called;
  };

  GetResult get_or_load(const std::string& name);
  GetResult get_or_load(const char* name);
  std::shared_ptr<const File> get_or_throw(const std::string& name);
  std::shared_ptr<const File> get_or_throw(const char* name);

  GetResult get(const std::string& name, std::function<std::string(const std::string&)> generate);
  GetResult get(const char* name, std::function<std::string(const std::string&)> generate);

  template <typename T>
  struct GetObjResult {
    const T& obj;
    std::shared_ptr<const File> data;
    bool generate_called;
  };

  template <typename T, typename NameT>
  GetObjResult<T> get_obj_or_load(NameT name) {
    auto res = this->get_or_load(name);
    if (res.file->data->size() != sizeof(T)) {
      throw std::runtime_error("cached string size is incorrect");
    }
    return {*reinterpret_cast<const T*>(res.file->data->data()), res.file, res.generate_called};
  }
  template <typename T, typename NameT>
  GetObjResult<T> get_obj_or_throw(NameT name) {
    auto res = this->get_or_throw(name);
    if (res.file->data->size() != sizeof(T)) {
      throw std::runtime_error("cached string size is incorrect");
    }
    return {*reinterpret_cast<const T*>(res.file->data->data()), res.file, res.generate_called};
  }
  template <typename T, typename NameT>
  GetObjResult<T> get_obj(NameT name, std::function<T(const std::string&)> generate) {
    uint64_t t = phosg::now();
    try {
      auto& f = this->name_to_file.at(name);
      if (f->data->size() != sizeof(T)) {
        throw std::runtime_error("cached string size is incorrect");
      }
      if (this->ttl_usecs && (t - f->load_time < this->ttl_usecs)) {
        return {*reinterpret_cast<const T*>(f->data->data()), f, false};
      }
    } catch (const std::out_of_range& e) {
    }
    T value = generate(name);
    auto ret = this->replace_obj(name, value);
    ret.generate_called = true;
    return ret;
  }
  template <typename T, typename NameT>
  GetObjResult<T> replace_obj(NameT name, const T& value) {
    auto cached_value = this->replace(name, &value, sizeof(value));
    return {*reinterpret_cast<const T*>(cached_value->data->data()), cached_value, false};
  }

private:
  std::unordered_map<std::string, std::shared_ptr<File>> name_to_file;
  uint64_t ttl_usecs;
};

class ThreadSafeFileCache {
public:
  explicit ThreadSafeFileCache() = default;
  ThreadSafeFileCache(const ThreadSafeFileCache&) = delete;
  ThreadSafeFileCache(ThreadSafeFileCache&&) = delete;
  ThreadSafeFileCache& operator=(const ThreadSafeFileCache&) = delete;
  ThreadSafeFileCache& operator=(ThreadSafeFileCache&&) = delete;
  ~ThreadSafeFileCache() = default;

  // Warning: generate() is called while the lock is held for writing, so it
  // will block other threads.
  std::shared_ptr<const std::string> get(const std::string& name, std::function<std::shared_ptr<const std::string>(const std::string&)> generate);

private:
  std::shared_mutex lock;
  std::unordered_map<std::string, std::shared_ptr<const std::string>> name_to_file;
};
