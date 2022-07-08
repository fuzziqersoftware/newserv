#include "FileContentsCache.hh"

#include <unistd.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

using namespace std;



FileContentsCache::FileContentsCache(uint64_t ttl_usecs) : ttl_usecs(ttl_usecs) { }

FileContentsCache::File::File(const string& name, shared_ptr<const string> contents,
    uint64_t load_time) : name(name), contents(contents), load_time(load_time) { }

shared_ptr<const string> FileContentsCache::replace(
    const string& name, string&& data, uint64_t t) {
  if (t == 0) {
    t = now();
  }
  shared_ptr<const string> contents(new string(move(data)));
  auto emplace_ret = this->name_to_file.emplace(
      piecewise_construct,
      forward_as_tuple(name),
      forward_as_tuple(name, contents, t));
  if (!emplace_ret.second) {
    emplace_ret.first->second.contents = contents;
    emplace_ret.first->second.load_time = t;
  }
  return contents;
}

shared_ptr<const string> FileContentsCache::replace(
    const string& name, const void* data, size_t size, uint64_t t) {
  string s(reinterpret_cast<const char*>(data), size);
  return this->replace(name, move(s), t);
}

FileContentsCache::GetResult FileContentsCache::get_or_load(const std::string& name) {
  return this->get(name, load_file);
}

FileContentsCache::GetResult FileContentsCache::get_or_load(const char* name) {
  return this->get_or_load(string(name));
}

shared_ptr<const string> FileContentsCache::get_or_throw(const std::string& name) {
  auto throw_fn = +[](const std::string&) -> string {
    throw out_of_range("file missing from cache");
  };
  return this->get(name, throw_fn).data;
}

shared_ptr<const string> FileContentsCache::get_or_throw(const char* name) {
  return this->get_or_throw(string(name));
}

FileContentsCache::GetResult FileContentsCache::get(const std::string& name,
    std::function<std::string(const std::string&)> generate) {
  uint64_t t = now();
  try {
    auto& entry = this->name_to_file.at(name);
    if (this->ttl_usecs && (t - entry.load_time < this->ttl_usecs)) {
      return {entry.contents, false};
    }
  } catch (const out_of_range& e) { }
  return {this->replace(name, generate(name)), true};
}

FileContentsCache::GetResult FileContentsCache::get(const char* name,
    std::function<std::string(const std::string&)> generate) {
  return this->get(string(name), generate);
}
