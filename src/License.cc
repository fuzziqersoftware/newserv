#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>

#include "License.hh"
#include "Loggers.hh"

using namespace std;

License::License(const JSON& json)
    : serial_number(0),
      flags(0),
      ban_end_time(0),
      ep3_current_meseta(0),
      ep3_total_meseta_earned(0) {
  this->serial_number = json.get_int("SerialNumber");
  this->access_key = json.get_string("AccessKey", "");
  this->gc_password = json.get_string("GCPassword", "");
  this->bb_username = json.get_string("BBUsername", "");
  this->bb_password = json.get_string("BBPassword", "");
  this->flags = json.get_int("Flags", 0);
  this->ban_end_time = json.get_int("BanEndTime", 0);
  this->ep3_current_meseta = json.get_int("Ep3CurrentMeseta", 0);
  this->ep3_total_meseta_earned = json.get_int("Ep3TotalMesetaEarned", 0);
}

JSON License::json() const {
  return JSON::dict({
      {"SerialNumber", this->serial_number},
      {"AccessKey", this->access_key},
      {"GCPassword", this->gc_password},
      {"BBUsername", this->bb_username},
      {"BBPassword", this->bb_password},
      {"Flags", this->flags},
      {"BanEndTime", this->ban_end_time},
      {"Ep3CurrentMeseta", this->ep3_current_meseta},
      {"Ep3TotalMesetaEarned", this->ep3_total_meseta_earned},
  });
}

void License::save() const {
  auto json = this->json();
  string json_data = json.serialize(JSON::SerializeOption::FORMAT | JSON::SerializeOption::HEX_INTEGERS);
  string filename = string_printf("system/licenses/%010" PRIu32 ".json", this->serial_number);
  save_file(filename, json_data);
}

void License::delete_file() const {
  string filename = string_printf("system/licenses/%010" PRIu32 ".json", this->serial_number);
  remove(filename.c_str());
}

string License::str() const {
  vector<string> tokens;
  tokens.emplace_back(string_printf("serial_number=%010" PRIu32 "/%08" PRIX32, this->serial_number, this->serial_number));
  if (!this->access_key.empty()) {
    tokens.emplace_back("access_key=" + this->access_key);
  }
  if (!this->gc_password.empty()) {
    tokens.emplace_back("gc_password=" + this->gc_password);
  }
  if (!this->bb_username.empty()) {
    tokens.emplace_back("bb_username=" + this->bb_username);
  }
  if (!this->bb_password.empty()) {
    tokens.emplace_back("bb_password=" + this->bb_password);
  }
  tokens.emplace_back(string_printf("flags=%08" PRIX32, this->flags));
  if (this->ban_end_time) {
    tokens.emplace_back(string_printf("ban_end_time=%016" PRIX64, this->ban_end_time));
  }
  if (this->ep3_current_meseta) {
    tokens.emplace_back(string_printf("ep3_current_meseta=%" PRIu32, this->ep3_current_meseta));
  }
  if (this->ep3_total_meseta_earned) {
    tokens.emplace_back(string_printf("ep3_total_meseta_earned=%" PRIu32, this->ep3_total_meseta_earned));
  }
  return "[License: " + join(tokens, ", ") + "]";
}

struct BinaryLicense {
  pstring<TextEncoding::ASCII, 0x14> username; // BB username (max. 16 chars; should technically be Unicode)
  pstring<TextEncoding::ASCII, 0x14> bb_password; // BB password (max. 16 chars)
  uint32_t serial_number; // PC/GC serial number. MUST BE PRESENT FOR BB LICENSES TOO; this is also the player's guild card number.
  pstring<TextEncoding::ASCII, 0x10> access_key; // PC/GC access key. (to log in using PC on a GC license, just enter the first 8 characters of the GC access key)
  pstring<TextEncoding::ASCII, 0x0C> gc_password; // GC password
  uint32_t privileges; // privilege level
  uint64_t ban_end_time; // end time of ban (zero = not banned)
} __attribute__((packed));

LicenseIndex::LicenseIndex() {
  if (!isdir("system/licenses")) {
    mkdir("system/licenses", 0755);
  }

  // Convert binary licenses to JSON licenses and save them
  if (isfile("system/licenses.nsi")) {
    auto bin_licenses = load_vector_file<BinaryLicense>("system/licenses.nsi");
    for (const auto& bin_license : bin_licenses) {
      // Only add licenses from the binary file if there isn't a JSON version of
      // the same license
      try {
        this->get(bin_license.serial_number);
      } catch (const missing_license&) {
        License license;
        license.serial_number = bin_license.serial_number;
        license.access_key = bin_license.access_key.decode();
        license.gc_password = bin_license.gc_password.decode();
        license.bb_username = bin_license.username.decode();
        license.bb_password = bin_license.bb_password.decode();
        license.flags = bin_license.privileges;
        license.ban_end_time = bin_license.ban_end_time;
        license.ep3_current_meseta = 0;
        license.ep3_total_meseta_earned = 0;
        license.save();
      }
    }
    ::remove("system/licenses.nsi");
  }

  for (const auto& item : list_directory("system/licenses")) {
    if (ends_with(item, ".json")) {
      JSON json = JSON::parse(load_file("system/licenses/" + item));
      shared_ptr<License> license(new License(json));
      this->add(license);
    }
  }
}

size_t LicenseIndex::count() const {
  return this->serial_number_to_license.size();
}

shared_ptr<License> LicenseIndex::get(uint32_t serial_number) const {
  try {
    return this->serial_number_to_license.at(serial_number);
  } catch (const out_of_range&) {
    throw missing_license();
  }
}

vector<shared_ptr<License>> LicenseIndex::all() const {
  vector<shared_ptr<License>> ret;
  ret.reserve(this->serial_number_to_license.size());
  for (const auto& it : this->serial_number_to_license) {
    ret.emplace_back(it.second);
  }
  return ret;
}

void LicenseIndex::add(shared_ptr<License> l) {
  this->serial_number_to_license[l->serial_number] = l;
  if (!l->bb_username.empty()) {
    this->bb_username_to_license[l->bb_username] = l;
  }
}

void LicenseIndex::remove(uint32_t serial_number) {
  auto l = this->serial_number_to_license.at(serial_number);
  this->serial_number_to_license.erase(l->serial_number);
  if (!l->bb_username.empty()) {
    this->bb_username_to_license.erase(l->bb_username);
  }
}

shared_ptr<License> LicenseIndex::verify_v1_v2(uint32_t serial_number, const string& access_key) const {
  if (serial_number == 0) {
    throw no_username();
  }
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (license->access_key.compare(0, 8, access_key) != 0) {
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

shared_ptr<License> LicenseIndex::verify_gc(uint32_t serial_number, const string& access_key) const {
  if (serial_number == 0) {
    throw no_username();
  }
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (license->access_key != access_key) {
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

shared_ptr<License> LicenseIndex::verify_gc(uint32_t serial_number, const string& access_key, const string& password) const {
  if (serial_number == 0) {
    throw no_username();
  }
  try {
    auto& license = this->serial_number_to_license.at(serial_number);
    if (license->access_key != access_key) {
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

shared_ptr<License> LicenseIndex::verify_bb(const string& username, const string& password) const {
  if (username.empty() || password.empty()) {
    throw no_username();
  }
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
