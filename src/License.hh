#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

enum Privilege {
  KICK_USER         = 0x00000001,
  BAN_USER          = 0x00000002,
  SILENCE_USER      = 0x00000004,
  CHANGE_LOBBY_INFO = 0x00000008,
  CHANGE_EVENT      = 0x00000010,
  ANNOUNCE          = 0x00000020,
  FREE_JOIN_GAMES   = 0x00000040,
  UNLOCK_GAMES      = 0x00000080,

  MODERATOR         = 0x00000007,
  ADMINISTRATOR     = 0x0000003F,
  ROOT              = 0xFFFFFFFF,
};

enum LicenseVerifyAction {
  BB = 0x00,
  GC = 0x01,
  PC = 0x02,
  SERIAL_NUMBER = 0x03,
};

struct License {
  char username[20]; // BB username (max. 16 chars; should technically be Unicode)
  char bb_password[20]; // BB password (max. 16 chars)
  uint32_t serial_number; // PC/GC serial number. MUST BE PRESENT FOR BB LICENSES TOO; this is also the player's guild card number.
  char access_key[16]; // PC/GC access key. (to log in using PC on a GC license, just enter the first 8 characters of the GC access key)
  char gc_password[12]; // GC password
  uint32_t privileges; // privilege level
  uint64_t ban_end_time; // end time of ban (zero = not banned)

  License();
  std::string str() const;
} __attribute__((packed));

class LicenseManager {
public:
  LicenseManager(const std::string& filename);
  ~LicenseManager() = default;

  std::shared_ptr<const License> verify_pc(uint32_t serial_number,
      const char* access_key, const char* password) const;
  std::shared_ptr<const License> verify_gc(uint32_t serial_number,
      const char* access_key, const char* password) const;
  std::shared_ptr<const License> verify_bb(const char* username,
      const char* password) const;
  void ban_until(uint32_t serial_number, uint64_t seconds);

  size_t count() const;

  void add(std::shared_ptr<License> l);
  void remove(uint32_t serial_number);
  std::vector<License> snapshot() const;

  static std::shared_ptr<const License> create_license_pc(
      uint32_t serial_number, const char* access_key, const char* password);
  static std::shared_ptr<const License> create_license_gc(
      uint32_t serial_number, const char* access_key, const char* password);
  static std::shared_ptr<const License> create_license_bb(
      uint32_t serial_number, const char* username, const char* password);

protected:
  void save() const;

  std::string filename;
  std::unordered_map<std::string, std::shared_ptr<License>> bb_username_to_license;
  std::unordered_map<uint32_t, std::shared_ptr<License>> serial_number_to_license;
};
