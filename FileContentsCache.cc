#include "FileContentsCache.hh"

#include <unistd.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

using namespace std;


FileContentsCache::File::File(const string& name, shared_ptr<const string> contents,
    uint64_t load_time) : name(name), contents(contents), load_time(load_time) { }

shared_ptr<const string> FileContentsCache::get(const std::string& name) {

  uint64_t t = now();
  try {
    lock_guard<mutex> g(this->lock);
    auto& entry = this->name_to_file.at(name);
    if (t - entry.load_time < 300000000) { // not 5 minutes old? return it
      return entry.contents;
    }
  } catch (const out_of_range& e) { }

    shared_ptr<const string> contents(new string(load_file(name)));

    {
      lock_guard<mutex> g(this->lock);
      this->name_to_file.erase(name);
      this->name_to_file.emplace(piecewise_construct, forward_as_tuple(name),
          forward_as_tuple(name, contents, t));
    }

    return contents;

}

shared_ptr<const string> FileContentsCache::get(const char* name) {
  return this->get(string(name));
}
