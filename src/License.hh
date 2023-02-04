#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include "Text.hh"

enum Privilege {
  KICK_USER         = 0x00000001,
  BAN_USER          = 0x00000002,
  SILENCE_USER      = 0x00000004,
  CHANGE_LOBBY_INFO = 0x00000008,
  CHANGE_EVENT      = 0x00000010,
  ANNOUNCE          = 0x00000020,
  FREE_JOIN_GAMES   = 0x00000040,
  UNLOCK_GAMES      = 0x00000080,

  DEBUG             = 0x01000000,

  MODERATOR         = 0x00000007,
  ADMINISTRATOR     = 0x0000003F,
  ROOT              = 0x7FFFFFFF,

  TEMPORARY         = 0x80000000,
};

enum LicenseVerifyAction {
  BB = 0x00,
  GC = 0x01,
  PC = 0x02,
  SERIAL_NUMBER = 0x03,
};

struct License {
  ptext<char, 0x14> username; // BB username (max. 16 chars; should technically be Unicode)
  ptext<char, 0x14> bb_password; // BB password (max. 16 chars)
  uint32_t serial_number; // PC/GC serial number. MUST BE PRESENT FOR BB LICENSES TOO; this is also the player's guild card number.
  ptext<char, 0x10> access_key; // PC/GC access key. (to log in using PC on a GC license, just enter the first 8 characters of the GC access key)
  ptext<char, 0x0C> gc_password; // GC password
  uint32_t privileges; // privilege level
  uint64_t ban_end_time; // end time of ban (zero = not banned)

  License();
  std::string str() const;
} __attribute__((packed));

class incorrect_password : public std::invalid_argument {
public:
  incorrect_password() : invalid_argument("incorrect password") { }
};

class incorrect_access_key : public std::invalid_argument {
public:
  incorrect_access_key() : invalid_argument("incorrect access key") { }
};

class missing_license : public std::invalid_argument {
public:
  missing_license() : invalid_argument("missing license") { }
};

class LicenseManager {
public:
  LicenseManager();
  explicit LicenseManager(const std::string& filename);
  ~LicenseManager() = default;

  void save() const;
  void set_autosave(bool autosave);

  std::shared_ptr<const License> verify_pc(uint32_t serial_number,
      const std::string& access_key) const;
  std::shared_ptr<const License> verify_gc(uint32_t serial_number,
      const std::string& access_key) const;
  std::shared_ptr<const License> verify_gc(uint32_t serial_number,
      const std::string& access_key, const std::string& password) const;
  std::shared_ptr<const License> verify_bb(const std::string& username,
      const std::string& password) const;
  void ban_until(uint32_t serial_number, uint64_t seconds);

  size_t count() const;

  std::shared_ptr<const License> get(uint32_t serial_number) const;
  void add(std::shared_ptr<License> l);
  void remove(uint32_t serial_number);
  std::vector<License> snapshot() const;

  static std::shared_ptr<License> create_license_pc(
      uint32_t serial_number, const std::string& access_key, bool temporary);
  static std::shared_ptr<License> create_license_gc(
      uint32_t serial_number, const std::string& access_key,
      const std::string& password, bool temporary);
  static std::shared_ptr<License> create_license_bb(
      uint32_t serial_number, const std::string& username,
      const std::string& password, bool temporary);

protected:
  std::string filename;
  bool autosave;

  std::unordered_map<std::string, std::shared_ptr<License>> bb_username_to_license;
  std::unordered_map<uint32_t, std::shared_ptr<License>> serial_number_to_license;
};
