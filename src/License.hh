#pragma once

#include <memory>
#include <phosg/JSON.hh>
#include <string>
#include <unordered_map>
#include <vector>

#include "Text.hh"

class LicenseIndex;

class License {
public:
  enum class Flag : uint32_t {
    // clang-format off
    KICK_USER                  = 0x00000001,
    BAN_USER                   = 0x00000002,
    SILENCE_USER               = 0x00000004,
    CHANGE_EVENT               = 0x00000010,
    ANNOUNCE                   = 0x00000020,
    FREE_JOIN_GAMES            = 0x00000040,
    DEBUG                      = 0x01000000,
    CHEAT_ANYWHERE             = 0x02000000,
    DISABLE_QUEST_REQUIREMENTS = 0x04000000,
    MODERATOR                  = 0x00000007,
    ADMINISTRATOR              = 0x000000FF,
    ROOT                       = 0x7FFFFFFF,
    IS_SHARED_SERIAL           = 0x80000000,
    // NOTE: When adding or changing license flags, don't forget to change the
    // documentation in the shell's help text.

    UNUSED_BITS                = 0x78FFFF00,
    // clang-format on
  };

  uint32_t serial_number = 0;
  std::string access_key;
  std::string gc_password;
  std::string xb_gamertag;
  uint64_t xb_user_id = 0;
  uint64_t xb_account_id = 0;
  std::string bb_username;
  std::string bb_password;

  uint32_t flags = 0;
  uint64_t ban_end_time = 0; // 0 = not banned
  std::string last_player_name;
  std::string auto_reply_message;

  uint32_t ep3_current_meseta = 0;
  uint32_t ep3_total_meseta_earned = 0;

  uint32_t bb_team_id = 0;

  License() = default;
  explicit License(const JSON& json);
  virtual ~License() = default;

  JSON json() const;
  virtual void save() const;
  virtual void delete_file() const;

  [[nodiscard]] inline bool check_flag(Flag flag) const {
    return !!(this->flags & static_cast<uint32_t>(flag));
  }
  inline void set_flag(Flag flag) {
    this->flags |= static_cast<uint32_t>(flag);
  }
  inline void clear_flag(Flag flag) {
    this->flags &= (~static_cast<uint32_t>(flag));
  }
  inline void toggle_flag(Flag flag) {
    this->flags ^= static_cast<uint32_t>(flag);
  }
  inline void replace_all_flags(Flag mask) {
    this->flags = static_cast<uint32_t>(mask);
  }

  std::string str() const;
};

class DiskLicense : public License {
public:
  DiskLicense() = default;
  explicit DiskLicense(const JSON& json);
  virtual ~DiskLicense() = default;

  virtual void save() const;
  virtual void delete_file() const;
};

class LicenseIndex {
public:
  class no_username : public std::invalid_argument {
  public:
    no_username() : invalid_argument("serial number is zero or username is missing") {}
  };
  class incorrect_password : public std::invalid_argument {
  public:
    incorrect_password() : invalid_argument("incorrect password") {}
  };
  class incorrect_access_key : public std::invalid_argument {
  public:
    incorrect_access_key() : invalid_argument("incorrect access key") {}
  };
  class missing_license : public std::invalid_argument {
  public:
    missing_license() : invalid_argument("missing license") {}
  };

  LicenseIndex() = default;
  virtual ~LicenseIndex() = default;

  virtual std::shared_ptr<License> create_license() const;
  virtual std::shared_ptr<License> create_temporary_license() const;

  size_t count() const;
  std::shared_ptr<License> get(uint32_t serial_number) const;
  std::shared_ptr<License> get_by_bb_username(const std::string& bb_username) const;
  std::vector<std::shared_ptr<License>> all() const;

  void add(std::shared_ptr<License> l);
  void remove(uint32_t serial_number);

  std::shared_ptr<License> verify_v1_v2(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name) const;
  std::shared_ptr<License> verify_gc_no_password(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name) const;
  std::shared_ptr<License> verify_gc_with_password(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& password,
      const std::string& character_name) const;
  std::shared_ptr<License> verify_xb(const std::string& gamertag, uint64_t user_id, uint64_t account_id) const;
  std::shared_ptr<License> verify_bb(const std::string& username, const std::string& password) const;

protected:
  std::unordered_map<std::string, std::shared_ptr<License>> bb_username_to_license;
  std::unordered_map<std::string, std::shared_ptr<License>> xb_gamertag_to_license;
  std::unordered_map<uint32_t, std::shared_ptr<License>> serial_number_to_license;

  std::shared_ptr<License> create_temporary_license_for_shared_license(
      uint32_t base_flags,
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& password,
      const std::string& character_name) const;
};

class DiskLicenseIndex : public LicenseIndex {
public:
  DiskLicenseIndex();
  virtual ~DiskLicenseIndex() = default;

  virtual std::shared_ptr<License> create_license() const;
};
