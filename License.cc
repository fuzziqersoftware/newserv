#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

#include "License.hh"

using namespace std;



LicenseManager::LicenseManager(const std::string& filename) : filename(filename) {
  auto licenses = load_vector_file<License>(this->filename);

  for (const auto& read_license : licenses) {
    shared_ptr<License> license(new License(read_license));
    this->bb_username_to_license.emplace(license->username, license);
    this->serial_number_to_license.emplace(license->serial_number, license);
  }
}

void LicenseManager::save_locked() const {
  auto f = fopen_unique(this->filename, "wb");
  for (const auto& it : this->serial_number_to_license) {
    fwritex(f.get(), it.second.get(), sizeof(License));
  }
}

shared_ptr<const License> LicenseManager::verify_pc(uint32_t serial_number,
    const char* access_key, const char* password) const {
  rw_guard g(this->lock, false);

  auto& license = this->serial_number_to_license.at(serial_number);
  if (strncmp(license->access_key, access_key, 8)) {
    throw invalid_argument("incorrect access key");
  }
  if (password && (strcmp(license->gc_password, password))) {
    throw invalid_argument("incorrect password");
  }

  if (license->ban_end_time && (license->ban_end_time >= now())) {
    throw invalid_argument("user is banned");
  }
  return license;
}

shared_ptr<const License> LicenseManager::verify_gc(uint32_t serial_number,
    const char* access_key, const char* password) const {
  rw_guard g(this->lock, false);

  auto& license = this->serial_number_to_license.at(serial_number);
  if (strncmp(license->access_key, access_key, 12)) {
    throw invalid_argument("incorrect access key");
  }
  if (password && (strcmp(license->gc_password, password))) {
    throw invalid_argument("incorrect password");
  }

  if (license->ban_end_time && (license->ban_end_time >= now())) {
    throw invalid_argument("user is banned");
  }
  return license;
}

shared_ptr<const License> LicenseManager::verify_bb(const char* username,
    const char* password) const {
  rw_guard g(this->lock, false);

  auto& license = this->bb_username_to_license.at(username);
  if (password && strcmp(license->bb_password, password)) {
    throw invalid_argument("incorrect password");
  }

  if (license->ban_end_time && (license->ban_end_time >= now())) {
    throw invalid_argument("user is banned");
  }
  return license;
}

void LicenseManager::ban_until(uint32_t serial_number, uint64_t end_time) {
  rw_guard g(this->lock, false);
  this->serial_number_to_license.at(serial_number)->ban_end_time = end_time;
  this->save_locked();
}

void LicenseManager::add(shared_ptr<License> l) {
  {
    rw_guard g(this->lock, true);
    this->serial_number_to_license.emplace(l->serial_number, l);
    if (l->username[0]) {
      this->bb_username_to_license.emplace(l->username, l);
    }
  }

  rw_guard g(this->lock, false);
  this->save_locked();
}

void LicenseManager::remove(uint32_t serial_number) {
  {
    rw_guard g(this->lock, true);
    auto l = this->serial_number_to_license.at(serial_number);
    this->serial_number_to_license.erase(l->serial_number);
    if (l->username[0]) {
      this->bb_username_to_license.erase(l->username);
    }
  }

  rw_guard g(this->lock, false);
  this->save_locked();
}
