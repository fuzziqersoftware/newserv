#pragma once

#include <memory>
#include <mutex>
#include <phosg/Hash.hh>
#include <phosg/JSON.hh>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Text.hh"

class LicenseIndex;

struct DCNTELicense {
  std::string serial_number;
  std::string access_key;

  inline uint64_t proxy_session_id_part() const {
    return phosg::fnv1a32(this->serial_number);
  }

  static std::shared_ptr<DCNTELicense> from_json(const phosg::JSON& json);
  phosg::JSON json() const;
};

struct V1V2License {
  uint32_t serial_number = 0;
  std::string access_key;

  inline uint64_t proxy_session_id_part() const {
    return this->serial_number;
  }

  static std::shared_ptr<V1V2License> from_json(const phosg::JSON& json);
  phosg::JSON json() const;
};

struct GCLicense {
  uint32_t serial_number = 0;
  std::string access_key;
  std::string password;

  inline uint64_t proxy_session_id_part() const {
    return this->serial_number;
  }

  static std::shared_ptr<GCLicense> from_json(const phosg::JSON& json);
  phosg::JSON json() const;
};

struct XBLicense {
  std::string gamertag;
  uint64_t user_id = 0;
  uint64_t account_id = 0;

  inline uint64_t proxy_session_id_part() const {
    return phosg::fnv1a32(this->gamertag);
  }

  static std::shared_ptr<XBLicense> from_json(const phosg::JSON& json);
  phosg::JSON json() const;
};

struct BBLicense {
  std::string username;
  std::string password;

  inline uint64_t proxy_session_id_part() const {
    return phosg::fnv1a32(this->username);
  }

  static std::shared_ptr<BBLicense> from_json(const phosg::JSON& json);
  phosg::JSON json() const;
};

struct Account {
  enum class Flag : uint32_t {
    // clang-format off
    KICK_USER                   = 0x00000001,
    BAN_USER                    = 0x00000002,
    SILENCE_USER                = 0x00000004,
    CHANGE_EVENT                = 0x00000010,
    ANNOUNCE                    = 0x00000020,
    FREE_JOIN_GAMES             = 0x00000040,
    DEBUG                       = 0x01000000,
    CHEAT_ANYWHERE              = 0x02000000,
    DISABLE_QUEST_REQUIREMENTS  = 0x04000000,
    ALWAYS_ENABLE_CHAT_COMMANDS = 0x08000000,
    MODERATOR                   = 0x00000007,
    ADMINISTRATOR               = 0x000000FF,
    ROOT                        = 0x7FFFFFFF,
    IS_SHARED_ACCOUNT           = 0x80000000,
    // NOTE: When adding or changing license flags, don't forget to change the
    // documentation in the shell's help text.
    UNUSED_BITS                 = 0x70FFFF00,
    // clang-format on
  };
  enum class UserFlag : uint32_t {
    DISABLE_DROP_NOTIFICATION_BROADCAST = 0x00000001,
  };

  // account_id is also the account's guild card number
  uint32_t account_id = 0;

  uint32_t flags = 0;
  uint32_t user_flags = 0;
  uint64_t ban_end_time = 0; // 0 = not banned
  std::string last_player_name;
  std::string auto_reply_message;

  uint32_t ep3_current_meseta = 0;
  uint32_t ep3_total_meseta_earned = 0;

  uint32_t bb_team_id = 0;
  bool is_temporary = false; // If true, isn't saved to disk

  std::unordered_set<std::string> auto_patches_enabled;

  std::unordered_map<std::string, std::shared_ptr<DCNTELicense>> dc_nte_licenses;
  std::unordered_map<uint32_t, std::shared_ptr<V1V2License>> dc_licenses;
  std::unordered_map<uint32_t, std::shared_ptr<V1V2License>> pc_licenses;
  std::unordered_map<uint32_t, std::shared_ptr<GCLicense>> gc_licenses;
  std::unordered_map<uint64_t, std::shared_ptr<XBLicense>> xb_licenses;
  std::unordered_map<std::string, std::shared_ptr<BBLicense>> bb_licenses;

  Account() = default;
  explicit Account(const phosg::JSON& json);
  virtual ~Account() = default;

  phosg::JSON json() const;
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

  [[nodiscard]] inline bool check_user_flag(UserFlag flag) const {
    return !!(this->user_flags & static_cast<uint32_t>(flag));
  }
  inline void set_user_flag(UserFlag flag) {
    this->user_flags |= static_cast<uint32_t>(flag);
  }
  inline void clear_user_flag(UserFlag flag) {
    this->user_flags &= (~static_cast<uint32_t>(flag));
  }
  inline void toggle_user_flag(UserFlag flag) {
    this->user_flags ^= static_cast<uint32_t>(flag);
  }

  std::string str() const;
};

struct Login {
  bool account_was_created = false;
  // This field will never be null
  std::shared_ptr<Account> account;
  // Exactly one of the following will be non-null, representing the license
  // that the client logged in with
  std::shared_ptr<DCNTELicense> dc_nte_license;
  std::shared_ptr<V1V2License> dc_license;
  std::shared_ptr<V1V2License> pc_license;
  std::shared_ptr<GCLicense> gc_license;
  std::shared_ptr<XBLicense> xb_license;
  std::shared_ptr<BBLicense> bb_license;

  uint64_t proxy_session_id() const;

  std::string str() const;
};

class AccountIndex {
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
  class missing_account : public std::invalid_argument {
  public:
    missing_account() : invalid_argument("missing account") {}
  };
  class account_banned : public std::invalid_argument {
  public:
    account_banned() : invalid_argument("account is banned") {}
  };

  explicit AccountIndex(bool force_all_temporary);
  virtual ~AccountIndex() = default;

  std::shared_ptr<Account> create_account(bool is_temporary) const;

  size_t count() const;
  std::vector<std::shared_ptr<Account>> all() const;

  void add(std::shared_ptr<Account> a);
  void remove(uint32_t serial_number);

  void add_dc_nte_license(std::shared_ptr<Account> account, std::shared_ptr<DCNTELicense> license);
  void add_dc_license(std::shared_ptr<Account> account, std::shared_ptr<V1V2License> license);
  void add_pc_license(std::shared_ptr<Account> account, std::shared_ptr<V1V2License> license);
  void add_gc_license(std::shared_ptr<Account> account, std::shared_ptr<GCLicense> license);
  void add_xb_license(std::shared_ptr<Account> account, std::shared_ptr<XBLicense> license);
  void add_bb_license(std::shared_ptr<Account> account, std::shared_ptr<BBLicense> license);
  void remove_dc_nte_license(std::shared_ptr<Account> account, const std::string& serial_number);
  void remove_dc_license(std::shared_ptr<Account> account, uint32_t serial_number);
  void remove_pc_license(std::shared_ptr<Account> account, uint32_t serial_number);
  void remove_gc_license(std::shared_ptr<Account> account, uint32_t serial_number);
  void remove_xb_license(std::shared_ptr<Account> account, uint64_t user_id);
  void remove_bb_license(std::shared_ptr<Account> account, const std::string& username);

  std::shared_ptr<Account> from_account_id(uint32_t account_id) const;
  std::shared_ptr<Login> from_dc_nte_credentials(
      const std::string& serial_number,
      const std::string& access_key,
      bool allow_create);
  std::shared_ptr<Login> from_dc_credentials(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name,
      bool allow_create);
  std::shared_ptr<Login> from_pc_nte_credentials(
      uint32_t guild_card_number,
      bool allow_create);
  std::shared_ptr<Login> from_pc_credentials(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name,
      bool allow_create);
  std::shared_ptr<Login> from_gc_credentials(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string* password,
      const std::string& character_name,
      bool allow_create);
  std::shared_ptr<Login> from_xb_credentials(
      const std::string& gamertag,
      uint64_t user_id,
      uint64_t account_id,
      bool allow_create);
  std::shared_ptr<Login> from_bb_credentials(
      const std::string& username,
      const std::string* password,
      bool allow_create);

  std::shared_ptr<Account> create_temporary_account_for_shared_account(
      std::shared_ptr<const Account> src_a, const std::string& variation_data) const;

protected:
  bool force_all_temporary;

  // This class must be thread-safe because it's used by both the patch server
  // and game server threads
  mutable std::shared_mutex lock;
  std::unordered_map<uint32_t, std::shared_ptr<Account>> by_account_id;
  std::unordered_map<std::string, std::shared_ptr<Account>> by_dc_nte_serial_number;
  std::unordered_map<uint32_t, std::shared_ptr<Account>> by_dc_serial_number;
  std::unordered_map<uint32_t, std::shared_ptr<Account>> by_pc_serial_number;
  std::unordered_map<uint32_t, std::shared_ptr<Account>> by_gc_serial_number;
  std::unordered_map<uint64_t, std::shared_ptr<Account>> by_xb_user_id;
  std::unordered_map<std::string, std::shared_ptr<Account>> by_bb_username;

  void add_locked(std::shared_ptr<Account> a);

  std::shared_ptr<Login> from_dc_nte_credentials_locked(
      const std::string& serial_number,
      const std::string& access_key);
  std::shared_ptr<Login> from_dc_credentials_locked(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name);
  std::shared_ptr<Login> from_pc_credentials_locked(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string& character_name);
  std::shared_ptr<Login> from_gc_credentials_locked(
      uint32_t serial_number,
      const std::string& access_key,
      const std::string* password,
      const std::string& character_name);
  std::shared_ptr<Login> from_xb_credentials_locked(uint64_t user_id);
  std::shared_ptr<Login> from_bb_credentials_locked(
      const std::string& username,
      const std::string* password);
};
