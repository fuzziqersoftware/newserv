#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

#include "License.hh"

using namespace std;



string License::str() const {
  string ret = string_printf("License(serial_number=%" PRIu32, this->serial_number);
  if (this->username[0]) {
    ret += ", username=";
    ret += this->username;
  }
  if (this->bb_password[0]) {
    ret += ", bb-password=";
    ret += this->bb_password;
  }
  if (this->access_key[0]) {
    ret += ", access-key=";
    ret += this->access_key;
  }
  if (this->gc_password[0]) {
    ret += ", gc-password=";
    ret += this->gc_password;
  }
  ret += string_printf(", privileges=%" PRIu32, this->privileges);
  if (this->ban_end_time) {
    ret += string_printf(", banned-until=%" PRIu64, this->ban_end_time);
  }
  return ret + ")";
}



LicenseManager::LicenseManager(const string& filename) : filename(filename) {
  try {
    auto licenses = load_vector_file<License>(this->filename);
    for (const auto& read_license : licenses) {
      shared_ptr<License> license(new License(read_license));
      uint32_t serial_number = license->serial_number;
      this->bb_username_to_license.emplace(license->username, license);
      this->serial_number_to_license.emplace(serial_number, license);
    }

  } catch (const cannot_open_file&) {
    log(WARNING, "%s does not exist; no licenses are registered", this->filename.c_str());
  }
}

void LicenseManager::save() const {
  auto f = fopen_unique(this->filename, "wb");
  for (const auto& it : this->serial_number_to_license) {
    fwritex(f.get(), it.second.get(), sizeof(License));
  }
}

shared_ptr<const License> LicenseManager::verify_pc(uint32_t serial_number,
    const char* access_key, const char* password) const {
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
  auto& license = this->bb_username_to_license.at(username);
  if (password && strcmp(license->bb_password, password)) {
    throw invalid_argument("incorrect password");
  }

  if (license->ban_end_time && (license->ban_end_time >= now())) {
    throw invalid_argument("user is banned");
  }
  return license;
}

size_t LicenseManager::count() const {
  return this->serial_number_to_license.size();
}

void LicenseManager::ban_until(uint32_t serial_number, uint64_t end_time) {
  this->serial_number_to_license.at(serial_number)->ban_end_time = end_time;
  this->save();
}

void LicenseManager::add(shared_ptr<License> l) {
  uint32_t serial_number = l->serial_number;
  this->serial_number_to_license.emplace(serial_number, l);
  if (l->username[0]) {
    this->bb_username_to_license.emplace(l->username, l);
  }

  this->save();
}

void LicenseManager::remove(uint32_t serial_number) {
  auto l = this->serial_number_to_license.at(serial_number);
  this->serial_number_to_license.erase(l->serial_number);
  if (l->username[0]) {
    this->bb_username_to_license.erase(l->username);
  }

  this->save();
}

vector<License> LicenseManager::snapshot() const {
  vector<License> ret;
  for (auto it : this->serial_number_to_license) {
    ret.emplace_back(*it.second);
  }
  return ret;
}


shared_ptr<const License> LicenseManager::create_license_pc(
    uint32_t serial_number,const char* access_key, const char* password) {
  shared_ptr<License> l(new License());
  memset(l.get(), 0, sizeof(License));
  l->serial_number = serial_number;
  strncpy(l->access_key, access_key, 8);
  if (password) {
    strncpy(l->gc_password, password, 8);
  }
  return l;
}

shared_ptr<const License> LicenseManager::create_license_gc(
    uint32_t serial_number, const char* access_key, const char* password) {
  shared_ptr<License> l(new License());
  memset(l.get(), 0, sizeof(License));
  l->serial_number = serial_number;
  strncpy(l->access_key, access_key, 12);
  if (password) {
    strncpy(l->gc_password, password, 8);
  }
  return l;
}

shared_ptr<const License> LicenseManager::create_license_bb(
    uint32_t serial_number, const char* username, const char* password) {
  shared_ptr<License> l(new License());
  memset(l.get(), 0, sizeof(License));
  l->serial_number = serial_number;
  strncpy(l->username, username, 19);
  strncpy(l->bb_password, password, 19);
  return l;
}
