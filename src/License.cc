#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

#include "License.hh"
#include "Loggers.hh"

using namespace std;

License::License()
    : serial_number(0),
      privileges(0),
      ban_end_time(0) {}

string License::str() const {
  string ret = string_printf("License(serial_number=%" PRIu32, this->serial_number);
  if (!this->username.empty()) {
    ret += ", username=";
    ret += this->username;
  }
  if (!this->bb_password.empty()) {
    ret += ", bb-password=";
    ret += this->bb_password;
  }
  if (!this->access_key.empty()) {
    ret += ", access-key=";
    ret += this->access_key;
  }
  if (!this->gc_password.empty()) {
    ret += ", gc-password=";
    ret += this->gc_password;
  }
  ret += string_printf(", privileges=%" PRIu32, this->privileges);
  if (this->ban_end_time) {
    ret += string_printf(", banned-until=%" PRIu64, this->ban_end_time);
  }
  return ret + ")";
}

LicenseManager::LicenseManager()
    : filename(""),
      autosave(false) {}

LicenseManager::LicenseManager(const string& filename)
    : filename(filename),
      autosave(true) {
  try {
    auto licenses = load_vector_file<License>(this->filename);
    for (const auto& read_license : licenses) {
      shared_ptr<License> license(new License(read_license));

      // Before the temporary flag existed, licenses with root privileges would
      // have the temporary flag set. To migrate these, explicitly unset the
      // flag for all licenses loaded from the license file.
      license->privileges &= ~Privilege::TEMPORARY;

      uint32_t serial_number = license->serial_number;
      this->bb_username_to_license.emplace(license->username, license);
      this->serial_number_to_license.emplace(serial_number, license);
    }

  } catch (const cannot_open_file&) {
    license_log.warning("File %s does not exist; no licenses are registered",
        this->filename.c_str());
  }
}

void LicenseManager::save() const {
  if (this->filename.empty()) {
    throw logic_error("license manager has no filename; cannot save");
  }
  auto f = fopen_unique(this->filename, "wb");
  for (const auto& it : this->serial_number_to_license) {
    if (it.second->privileges & Privilege::TEMPORARY) {
      continue;
    }
    fwritex(f.get(), it.second.get(), sizeof(License));
  }
}

void LicenseManager::set_autosave(bool autosave) {
  this->autosave = autosave;
}

shared_ptr<const License> LicenseManager::verify_pc(uint32_t serial_number,
    const string& access_key) const {
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (!license->access_key.eq_n(access_key, 8)) {
      throw incorrect_access_key();
    }

    if (license->ban_end_time && (license->ban_end_time >= now())) {
      throw invalid_argument("user is banned");
    }
    return license;
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

shared_ptr<const License> LicenseManager::verify_gc(uint32_t serial_number,
    const string& access_key) const {
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (!license->access_key.eq_n(access_key, 12)) {
      throw incorrect_access_key();
    }
    if (license->ban_end_time && (license->ban_end_time >= now())) {
      throw invalid_argument("user is banned");
    }
    return license;
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

shared_ptr<const License> LicenseManager::verify_gc(uint32_t serial_number,
    const string& access_key, const string& password) const {
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (!license->access_key.eq_n(access_key, 12)) {
      throw incorrect_access_key();
    }
    if (license->gc_password != password) {
      throw incorrect_password();
    }
    if (license->ban_end_time && (license->ban_end_time >= now())) {
      throw invalid_argument("user is banned");
    }
    return license;
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

shared_ptr<const License> LicenseManager::verify_bb(const string& username,
    const string& password) const {
  try {
    auto& license = this->bb_username_to_license.at(username);
    if (license->bb_password != password) {
      throw incorrect_password();
    }

    if (license->ban_end_time && (license->ban_end_time >= now())) {
      throw invalid_argument("user is banned");
    }
    return license;
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

size_t LicenseManager::count() const {
  return this->serial_number_to_license.size();
}

void LicenseManager::ban_until(uint32_t serial_number, uint64_t end_time) {
  this->serial_number_to_license.at(serial_number)->ban_end_time = end_time;
  if (this->autosave) {
    this->save();
  }
}

shared_ptr<const License> LicenseManager::get(uint32_t serial_number) const {
  try {
    return this->serial_number_to_license.at(serial_number);
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

void LicenseManager::add(shared_ptr<License> l) {
  this->serial_number_to_license[l->serial_number] = l;
  if (!l->username.empty()) {
    this->bb_username_to_license[l->username] = l;
  }
  if (this->autosave) {
    this->save();
  }
}

void LicenseManager::remove(uint32_t serial_number) {
  auto l = this->serial_number_to_license.at(serial_number);
  this->serial_number_to_license.erase(l->serial_number);
  if (!l->username.empty()) {
    this->bb_username_to_license.erase(l->username);
  }
  if (this->autosave) {
    this->save();
  }
}

vector<License> LicenseManager::snapshot() const {
  vector<License> ret;
  for (auto it : this->serial_number_to_license) {
    ret.emplace_back(*it.second);
  }
  return ret;
}

shared_ptr<License> LicenseManager::create_license_pc(
    uint32_t serial_number, const string& access_key, bool temporary) {
  shared_ptr<License> l(new License());
  l->serial_number = serial_number;
  l->access_key = access_key;
  if (temporary) {
    l->privileges |= Privilege::TEMPORARY;
  }
  return l;
}

shared_ptr<License> LicenseManager::create_license_gc(
    uint32_t serial_number, const string& access_key, const string& password,
    bool temporary) {
  shared_ptr<License> l(new License());
  l->serial_number = serial_number;
  l->access_key = access_key;
  l->gc_password = password;
  if (temporary) {
    l->privileges |= Privilege::TEMPORARY;
  }
  return l;
}

shared_ptr<License> LicenseManager::create_license_bb(
    uint32_t serial_number, const string& username, const string& password,
    bool temporary) {
  shared_ptr<License> l(new License());
  l->serial_number = serial_number;
  l->username = username;
  l->bb_password = password;
  if (temporary) {
    l->privileges |= Privilege::TEMPORARY;
  }
  return l;
}
