#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>

#include "Account.hh"

using namespace std;

shared_ptr<DCNTELicense> DCNTELicense::from_json(const phosg::JSON& json) {
  auto ret = make_shared<DCNTELicense>();
  ret->serial_number = json.get_string("SerialNumber");
  ret->access_key = json.get_string("AccessKey");
  if (ret->serial_number.size() > 16) {
    throw runtime_error("serial number is too long");
  }
  if (ret->serial_number.empty()) {
    throw runtime_error("serial number is too short");
  }
  if (ret->access_key.size() > 16) {
    throw runtime_error("access key is too long");
  }
  if (ret->access_key.empty()) {
    throw runtime_error("access key is too short");
  }
  return ret;
}

phosg::JSON DCNTELicense::json() const {
  return phosg::JSON::dict({
      {"SerialNumber", this->serial_number},
      {"AccessKey", this->access_key},
  });
}

shared_ptr<V1V2License> V1V2License::from_json(const phosg::JSON& json) {
  auto ret = make_shared<V1V2License>();
  ret->serial_number = json.get_int("SerialNumber");
  ret->access_key = json.get_string("AccessKey");
  if (ret->serial_number == 0) {
    throw runtime_error("serial number is zero");
  }
  if (ret->access_key.size() != 8) {
    throw runtime_error("access key length is incorrect");
  }
  return ret;
}

phosg::JSON V1V2License::json() const {
  return phosg::JSON::dict({
      {"SerialNumber", this->serial_number},
      {"AccessKey", this->access_key},
  });
}

shared_ptr<GCLicense> GCLicense::from_json(const phosg::JSON& json) {
  auto ret = make_shared<GCLicense>();
  ret->serial_number = json.get_int("SerialNumber");
  ret->access_key = json.get_string("AccessKey");
  ret->password = json.get_string("Password");
  if (ret->serial_number == 0) {
    throw runtime_error("serial number is zero");
  }
  if (ret->access_key.size() != 12) {
    throw runtime_error("access key length is incorrect");
  }
  if (ret->password.empty()) {
    throw runtime_error("password is too short");
  }
  return ret;
}

phosg::JSON GCLicense::json() const {
  return phosg::JSON::dict({
      {"SerialNumber", this->serial_number},
      {"AccessKey", this->access_key},
      {"Password", this->password},
  });
}

shared_ptr<XBLicense> XBLicense::from_json(const phosg::JSON& json) {
  auto ret = make_shared<XBLicense>();
  ret->gamertag = json.get_string("GamerTag");
  ret->user_id = json.get_int("UserID");
  ret->account_id = json.get_int("AccountID");
  if (ret->gamertag.empty()) {
    throw runtime_error("gamertag is too short");
  }
  if (ret->user_id == 0) {
    throw runtime_error("user ID is zero");
  }
  if (ret->account_id == 0) {
    throw runtime_error("account ID is zero");
  }
  return ret;
}

phosg::JSON XBLicense::json() const {
  return phosg::JSON::dict({
      {"GamerTag", this->gamertag},
      {"UserID", this->user_id},
      {"AccountID", this->account_id},
  });
}

shared_ptr<BBLicense> BBLicense::from_json(const phosg::JSON& json) {
  auto ret = make_shared<BBLicense>();
  ret->username = json.get_string("UserName");
  ret->password = json.get_string("Password");
  if (ret->username.size() > 16) {
    throw runtime_error("username is too long");
  }
  if (ret->username.empty()) {
    throw runtime_error("username is too short");
  }
  if (ret->password.size() > 16) {
    throw runtime_error("password is too long");
  }
  if (ret->password.empty()) {
    throw runtime_error("password is too short");
  }
  return ret;
}

phosg::JSON BBLicense::json() const {
  return phosg::JSON::dict({
      {"UserName", this->username},
      {"Password", this->password},
  });
}

Account::Account(const phosg::JSON& json)
    : account_id(0),
      flags(0),
      user_flags(0),
      ban_end_time(0),
      ep3_current_meseta(0),
      ep3_total_meseta_earned(0),
      bb_team_id(0) {
  uint64_t format_version = 0;
  try {
    format_version = json.get_int("FormatVersion");
  } catch (const out_of_range&) {
  }

  if (format_version == 0) {
    // Original format - no account ID
    this->account_id = json.get_int("SerialNumber");
    string access_key = json.get_string("AccessKey", "");
    string dc_nte_serial_number = json.get_string("DCNTESerialNumber", "");
    string dc_nte_access_key = json.get_string("DCNTEAccessKey", "");
    string gc_password = json.get_string("GCPassword", "");
    string xb_gamertag = json.get_string("XBGamerTag", "");
    uint64_t xb_user_id = json.get_int("XBUserID", 0);
    uint64_t xb_account_id = json.get_int("XBAccountID", 0);
    string bb_username = json.get_string("BBUsername", "");
    string bb_password = json.get_string("BBPassword", "");
    if (access_key.size() == 12) {
      if (!gc_password.empty()) {
        auto lic = make_shared<GCLicense>();
        lic->serial_number = this->account_id;
        lic->access_key = access_key;
        lic->password = gc_password;
        this->gc_licenses.emplace(lic->serial_number, lic);
      }
    } else if (access_key.size() >= 8) {
      auto lic = make_shared<V1V2License>();
      lic->serial_number = this->account_id;
      lic->access_key = access_key.substr(0, 8);
      this->dc_licenses.emplace(lic->serial_number, lic);
      this->pc_licenses.emplace(lic->serial_number, lic);
    }
    if (!dc_nte_serial_number.empty() && !dc_nte_access_key.empty()) {
      auto lic = make_shared<DCNTELicense>();
      lic->serial_number = dc_nte_serial_number;
      lic->access_key = dc_nte_access_key;
      this->dc_nte_licenses.emplace(lic->serial_number, lic);
    }
    if (!xb_gamertag.empty() && xb_user_id && xb_account_id) {
      auto lic = make_shared<XBLicense>();
      lic->gamertag = xb_gamertag;
      lic->user_id = xb_user_id;
      lic->account_id = xb_account_id;
      this->xb_licenses.emplace(lic->user_id, lic);
    }
    if (!bb_username.empty() && !bb_password.empty()) {
      auto lic = make_shared<BBLicense>();
      lic->username = bb_username;
      lic->password = bb_password;
      this->bb_licenses.emplace(lic->username, lic);
    }
  } else {
    // Second-gen format - with account ID; multiple credentials per version
    this->account_id = json.get_int("AccountID");
    for (const auto& it : json.get_list("DCNTELicenses")) {
      auto lic = DCNTELicense::from_json(*it);
      this->dc_nte_licenses.emplace(lic->serial_number, lic);
    }
    for (const auto& it : json.get_list("DCLicenses")) {
      auto lic = V1V2License::from_json(*it);
      this->dc_licenses.emplace(lic->serial_number, lic);
    }
    for (const auto& it : json.get_list("PCLicenses")) {
      auto lic = V1V2License::from_json(*it);
      this->pc_licenses.emplace(lic->serial_number, lic);
    }
    for (const auto& it : json.get_list("GCLicenses")) {
      auto lic = GCLicense::from_json(*it);
      this->gc_licenses.emplace(lic->serial_number, lic);
    }
    for (const auto& it : json.get_list("XBLicenses")) {
      auto lic = XBLicense::from_json(*it);
      this->xb_licenses.emplace(lic->user_id, lic);
    }
    for (const auto& it : json.get_list("BBLicenses")) {
      auto lic = BBLicense::from_json(*it);
      this->bb_licenses.emplace(lic->username, lic);
    }
  }

  this->flags = json.get_int("Flags", 0);
  this->user_flags = json.get_int("UserFlags", 0);
  this->ban_end_time = json.get_int("BanEndTime", 0);
  this->last_player_name = json.get_string("LastPlayerName", "");
  this->auto_reply_message = json.get_string("AutoReplyMessage", "");
  this->ep3_current_meseta = json.get_int("Ep3CurrentMeseta", 0);
  this->ep3_total_meseta_earned = json.get_int("Ep3TotalMesetaEarned", 0);
  this->bb_team_id = json.get_int("BBTeamID", 0);

  try {
    for (const auto& it : json.get_list("AutoPatchesEnabled")) {
      this->auto_patches_enabled.emplace(it->as_string());
    }
  } catch (const out_of_range&) {
  }
}

phosg::JSON Account::json() const {
  phosg::JSON dc_nte_json = phosg::JSON::list();
  for (const auto& it : this->dc_nte_licenses) {
    dc_nte_json.emplace_back(it.second->json());
  }
  phosg::JSON dc_json = phosg::JSON::list();
  for (const auto& it : this->dc_licenses) {
    dc_json.emplace_back(it.second->json());
  }
  phosg::JSON pc_json = phosg::JSON::list();
  for (const auto& it : this->pc_licenses) {
    pc_json.emplace_back(it.second->json());
  }
  phosg::JSON gc_json = phosg::JSON::list();
  for (const auto& it : this->gc_licenses) {
    gc_json.emplace_back(it.second->json());
  }
  phosg::JSON xb_json = phosg::JSON::list();
  for (const auto& it : this->xb_licenses) {
    xb_json.emplace_back(it.second->json());
  }
  phosg::JSON bb_json = phosg::JSON::list();
  for (const auto& it : this->bb_licenses) {
    bb_json.emplace_back(it.second->json());
  }

  phosg::JSON auto_patches_json = phosg::JSON::list();
  for (const auto& it : this->auto_patches_enabled) {
    auto_patches_json.emplace_back(it);
  }

  return phosg::JSON::dict({
      {"FormatVersion", 1},
      {"AccountID", this->account_id},
      {"DCNTELicenses", std::move(dc_nte_json)},
      {"DCLicenses", std::move(dc_json)},
      {"PCLicenses", std::move(pc_json)},
      {"GCLicenses", std::move(gc_json)},
      {"XBLicenses", std::move(xb_json)},
      {"BBLicenses", std::move(bb_json)},
      {"Flags", this->flags},
      {"UserFlags", this->user_flags},
      {"BanEndTime", this->ban_end_time},
      {"LastPlayerName", this->last_player_name},
      {"AutoReplyMessage", this->auto_reply_message},
      {"Ep3CurrentMeseta", this->ep3_current_meseta},
      {"Ep3TotalMesetaEarned", this->ep3_total_meseta_earned},
      {"BBTeamID", this->bb_team_id},
      {"AutoPatchesEnabled", std::move(auto_patches_json)},
  });
}

string Account::str() const {
  std::string ret = phosg::string_printf("Account: %010" PRIu32 "/%08" PRIX32 "\n", this->account_id, this->account_id);

  if (this->flags) {
    string flags_str = "";
    if (this->flags == static_cast<uint32_t>(Flag::ROOT)) {
      flags_str = "ROOT";
    } else if (this->flags == static_cast<uint32_t>(Flag::ADMINISTRATOR)) {
      flags_str = "ADMINISTRATOR";
    } else if (this->flags == static_cast<uint32_t>(Flag::MODERATOR)) {
      flags_str = "MODERATOR";
    } else {
      if (this->check_flag(Flag::KICK_USER)) {
        flags_str += "KICK_USER,";
      }
      if (this->check_flag(Flag::BAN_USER)) {
        flags_str += "BAN_USER,";
      }
      if (this->check_flag(Flag::SILENCE_USER)) {
        flags_str += "SILENCE_USER,";
      }
      if (this->check_flag(Flag::CHANGE_EVENT)) {
        flags_str += "CHANGE_EVENT,";
      }
      if (this->check_flag(Flag::ANNOUNCE)) {
        flags_str += "ANNOUNCE,";
      }
      if (this->check_flag(Flag::FREE_JOIN_GAMES)) {
        flags_str += "FREE_JOIN_GAMES,";
      }
      if (this->check_flag(Flag::DEBUG)) {
        flags_str += "DEBUG,";
      }
      if (this->check_flag(Flag::CHEAT_ANYWHERE)) {
        flags_str += "CHEAT_ANYWHERE,";
      }
      if (this->check_flag(Flag::DISABLE_QUEST_REQUIREMENTS)) {
        flags_str += "ALWAYS_ENABLE_CHAT_COMMANDS,";
      }
      if (this->check_flag(Flag::IS_SHARED_ACCOUNT)) {
        flags_str += "IS_SHARED_ACCOUNT,";
      }
    }
    if (flags_str.empty()) {
      flags_str = "none";
    } else if (phosg::ends_with(flags_str, ",")) {
      flags_str.pop_back();
    }
    ret += phosg::string_printf("  Flags: %08" PRIX32 " (%s)\n", this->flags, flags_str.c_str());
  }

  if (this->user_flags) {
    string user_flags_str = "";
    if (this->check_user_flag(UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST)) {
      user_flags_str += "DISABLE_DROP_NOTIFICATION_BROADCAST,";
    }
    if (user_flags_str.empty()) {
      user_flags_str = "none";
    } else if (phosg::ends_with(user_flags_str, ",")) {
      user_flags_str.pop_back();
    }
    ret += phosg::string_printf("  User flags: %08" PRIX32 " (%s)\n", this->user_flags, user_flags_str.c_str());
  }

  if (this->ban_end_time) {
    string time_str = phosg::format_time(this->ban_end_time);
    ret += phosg::string_printf("  Banned until: %" PRIu64 " (%s)\n", this->ban_end_time, time_str.c_str());
  }
  if (this->ep3_current_meseta || this->ep3_total_meseta_earned) {
    ret += phosg::string_printf("  Episode 3 meseta: %" PRIu32 " (total earned: %" PRIu32 ")\n",
        this->ep3_current_meseta, this->ep3_total_meseta_earned);
  }
  if (!this->last_player_name.empty()) {
    ret += phosg::string_printf("  Last player name: \"%s\"\n", this->last_player_name.c_str());
  }
  if (!this->auto_reply_message.empty()) {
    ret += phosg::string_printf("  Auto reply message: \"%s\"\n", this->auto_reply_message.c_str());
  }
  if (this->bb_team_id) {
    ret += phosg::string_printf("  BB team ID: %08" PRIX32 "\n", this->bb_team_id);
  }
  if (this->is_temporary) {
    ret += phosg::string_printf("  Is temporary license: true\n");
  }

  for (const auto& it : this->dc_nte_licenses) {
    ret += phosg::string_printf("  DC NTE license: serial_number=%s access_key=%s\n",
        it.second->serial_number.c_str(), it.second->access_key.c_str());
  }
  for (const auto& it : this->dc_licenses) {
    ret += phosg::string_printf("  DC license: serial_number=%" PRIX32 " access_key=%s\n",
        it.second->serial_number, it.second->access_key.c_str());
  }
  for (const auto& it : this->pc_licenses) {
    ret += phosg::string_printf("  PC license: serial_number=%" PRIX32 " access_key=%s\n",
        it.second->serial_number, it.second->access_key.c_str());
  }
  for (const auto& it : this->gc_licenses) {
    ret += phosg::string_printf("  GC license: serial_number=%010" PRIu32 " access_key=%s password=%s\n",
        it.second->serial_number, it.second->access_key.c_str(), it.second->password.c_str());
  }
  for (const auto& it : this->xb_licenses) {
    ret += phosg::string_printf("  XB license: gamertag=%s user_id=%016" PRIX64 " account_id=%016" PRIX64 "\n",
        it.second->gamertag.c_str(), it.second->user_id, it.second->account_id);
  }
  for (const auto& it : this->bb_licenses) {
    ret += phosg::string_printf("  BB license: username=%s password=%s\n",
        it.second->username.c_str(), it.second->password.c_str());
  }

  phosg::strip_trailing_whitespace(ret);
  return ret;
}

void Account::save() const {
  if (!this->is_temporary) {
    auto json = this->json();
    string json_data = json.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::HEX_INTEGERS);
    string filename = phosg::string_printf("system/licenses/%010" PRIu32 ".json", this->account_id);
    phosg::save_file(filename, json_data);
  }
}

void Account::delete_file() const {
  string filename = phosg::string_printf("system/licenses/%010" PRIu32 ".json", this->account_id);
  remove(filename.c_str());
}

uint64_t Login::proxy_session_id() const {
  uint64_t low_part = 0;
  if (this->dc_nte_license) {
    low_part = this->dc_nte_license->proxy_session_id_part();
  } else if (this->dc_license) {
    low_part = this->dc_license->proxy_session_id_part();
  } else if (this->pc_license) {
    low_part = this->pc_license->proxy_session_id_part();
  } else if (this->gc_license) {
    low_part = this->gc_license->proxy_session_id_part();
  } else if (this->xb_license) {
    low_part = this->xb_license->proxy_session_id_part();
  } else if (this->bb_license) {
    low_part = this->bb_license->proxy_session_id_part();
  } else {
    throw logic_error("none of the licenses in a Login were present");
  }
  return (static_cast<uint64_t>(this->account->account_id) << 32) | low_part;
}

string Login::str() const {
  string ret = phosg::string_printf("Account:%08" PRIX32, this->account->account_id);
  if (this->account_was_created) {
    ret += " (new)";
  }
  if (this->dc_nte_license) {
    ret += phosg::string_printf(" via DC NTE serial number %s", this->dc_nte_license->serial_number.c_str());
  } else if (this->dc_license) {
    ret += phosg::string_printf(" via DC serial number %08" PRIX32, this->dc_license->serial_number);
  } else if (this->pc_license) {
    ret += phosg::string_printf(" via PC serial number %08" PRIX32, this->pc_license->serial_number);
  } else if (this->gc_license) {
    ret += phosg::string_printf(" via GC serial number %010" PRIu32, this->gc_license->serial_number);
  } else if (this->xb_license) {
    ret += phosg::string_printf(" via XB user ID %016" PRIX64, this->xb_license->user_id);
  } else if (this->bb_license) {
    ret += phosg::string_printf(" via BB username %s", this->bb_license->username.c_str());
  } else {
    ret += phosg::string_printf(" artificially");
  }
  return ret;
}

size_t AccountIndex::count() const {
  shared_lock g(this->lock);
  return this->by_account_id.size();
}

shared_ptr<Account> AccountIndex::from_account_id(uint32_t account_id) const {
  try {
    shared_lock g(this->lock);
    return this->by_account_id.at(account_id);
  } catch (const out_of_range&) {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_dc_nte_credentials_locked(const string& serial_number, const string& access_key) {
  auto login = make_shared<Login>();
  login->account = this->by_dc_nte_serial_number.at(serial_number);
  login->dc_nte_license = login->account->dc_nte_licenses.at(serial_number);
  if (login->dc_nte_license->access_key != access_key) {
    throw incorrect_access_key();
  }
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_dc_nte_credentials(
    const string& serial_number, const string& access_key, bool allow_create) {
  if (serial_number.empty()) {
    throw no_username();
  }

  try {
    shared_lock g(this->lock);
    return this->from_dc_nte_credentials_locked(serial_number, access_key);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_dc_nte_credentials_locked(serial_number, access_key);
  } catch (const out_of_range&) {
  }

  if (allow_create) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = phosg::fnv1a32(serial_number) & 0x7FFFFFFF;
    auto lic = make_shared<DCNTELicense>();
    lic->serial_number = serial_number;
    lic->access_key = access_key;
    login->account->dc_nte_licenses.emplace(lic->serial_number, lic);
    login->dc_nte_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_dc_credentials_locked(
    uint32_t serial_number, const string& access_key, const string& character_name) {
  auto login = make_shared<Login>();
  login->account = this->by_dc_serial_number.at(serial_number);
  login->dc_license = login->account->dc_licenses.at(serial_number);
  bool is_shared = login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT);
  if (!is_shared && (login->dc_license->access_key != access_key)) {
    throw incorrect_access_key();
  }
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  if (is_shared) {
    login->account = this->create_temporary_account_for_shared_account(login->account, access_key + ":" + character_name);
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_dc_credentials(
    uint32_t serial_number, const string& access_key, const string& character_name, bool allow_create) {
  if (serial_number == 0) {
    throw no_username();
  }

  try {
    shared_lock g(this->lock);
    return this->from_dc_credentials_locked(serial_number, access_key, character_name);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_dc_credentials_locked(serial_number, access_key, character_name);
  } catch (const out_of_range&) {
  }

  if (allow_create) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = serial_number;
    auto lic = make_shared<V1V2License>();
    lic->serial_number = serial_number;
    lic->access_key = access_key;
    login->account->dc_licenses.emplace(lic->serial_number, lic);
    login->dc_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_pc_nte_credentials(uint32_t guild_card_number, bool allow_create) {
  if (!allow_create) {
    throw missing_account();
  }
  if (guild_card_number == 0xFFFFFFFF) {
    guild_card_number = phosg::random_object<uint32_t>() & 0x7FFFFFFF;
  }
  auto login = make_shared<Login>();
  login->account_was_created = true;
  login->account = make_shared<Account>();
  login->account->account_id = guild_card_number;
  login->account->is_temporary = true;
  auto lic = make_shared<V1V2License>();
  lic->serial_number = guild_card_number;
  login->account->pc_licenses.emplace(lic->serial_number, lic);
  login->pc_license = lic;
  this->add(login->account);
  return login;
}

shared_ptr<Login> AccountIndex::from_pc_credentials_locked(
    uint32_t serial_number, const string& access_key, const string& character_name) {
  auto login = make_shared<Login>();
  login->account = this->by_pc_serial_number.at(serial_number);
  login->pc_license = login->account->pc_licenses.at(serial_number);
  bool is_shared = login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT);
  if (!is_shared && (login->pc_license->access_key != access_key)) {
    throw incorrect_access_key();
  }
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  if (is_shared) {
    login->account = this->create_temporary_account_for_shared_account(login->account, access_key + ":" + character_name);
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_pc_credentials(
    uint32_t serial_number, const string& access_key, const string& character_name, bool allow_create) {
  if (serial_number == 0) {
    throw no_username();
  }

  try {
    shared_lock g(this->lock);
    return this->from_pc_credentials_locked(serial_number, access_key, character_name);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_pc_credentials_locked(serial_number, access_key, character_name);
  } catch (const out_of_range&) {
  }

  if (allow_create) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = serial_number;
    auto lic = make_shared<V1V2License>();
    lic->serial_number = serial_number;
    lic->access_key = access_key;
    login->account->pc_licenses.emplace(lic->serial_number, lic);
    login->pc_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_gc_credentials_locked(
    uint32_t serial_number, const string& access_key, const string* password, const string& character_name) {
  auto login = make_shared<Login>();
  login->account = this->by_gc_serial_number.at(serial_number);
  login->gc_license = login->account->gc_licenses.at(serial_number);
  bool is_shared = login->account->check_flag(Account::Flag::IS_SHARED_ACCOUNT);
  if (!is_shared && (login->gc_license->access_key != access_key)) {
    throw incorrect_access_key();
  }
  if (password && (login->gc_license->password != *password)) {
    throw incorrect_password();
  }
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  if (is_shared) {
    login->account = this->create_temporary_account_for_shared_account(login->account, access_key + ":" + character_name);
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_gc_credentials(
    uint32_t serial_number, const string& access_key, const string* password, const string& character_name, bool allow_create) {
  if (serial_number == 0) {
    throw no_username();
  }

  try {
    shared_lock g(this->lock);
    return this->from_gc_credentials_locked(serial_number, access_key, password, character_name);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_gc_credentials_locked(serial_number, access_key, password, character_name);
  } catch (const out_of_range&) {
  }

  if (allow_create && password) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = serial_number;
    auto lic = make_shared<GCLicense>();
    lic->serial_number = serial_number;
    lic->access_key = access_key;
    lic->password = *password;
    login->account->gc_licenses.emplace(lic->serial_number, lic);
    login->gc_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_xb_credentials_locked(uint64_t user_id) {
  auto login = make_shared<Login>();
  login->account = this->by_xb_user_id.at(user_id);
  login->xb_license = login->account->xb_licenses.at(user_id);
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_xb_credentials(
    const string& gamertag, uint64_t user_id, uint64_t account_id, bool allow_create) {
  if (user_id == 0 || account_id == 0) {
    throw incorrect_access_key();
  }

  try {
    shared_lock g(this->lock);
    return this->from_xb_credentials_locked(user_id);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_xb_credentials_locked(user_id);
  } catch (const out_of_range&) {
  }

  if (allow_create) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = phosg::fnv1a32(gamertag) & 0x7FFFFFFF;
    auto lic = make_shared<XBLicense>();
    lic->gamertag = gamertag;
    lic->user_id = user_id;
    lic->account_id = account_id;
    login->account->xb_licenses.emplace(lic->user_id, lic);
    login->xb_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

shared_ptr<Login> AccountIndex::from_bb_credentials_locked(const string& username, const string* password) {
  auto login = make_shared<Login>();
  login->account = this->by_bb_username.at(username);
  login->bb_license = login->account->bb_licenses.at(username);
  if (password && (login->bb_license->password != *password)) {
    throw incorrect_password();
  }
  if (login->account->ban_end_time && (login->account->ban_end_time >= phosg::now())) {
    throw account_banned();
  }
  return login;
}

shared_ptr<Login> AccountIndex::from_bb_credentials(const string& username, const string* password, bool allow_create) {
  if (username.empty() || (password && password->empty())) {
    throw no_username();
  }

  try {
    shared_lock g(this->lock);
    return this->from_bb_credentials_locked(username, password);
  } catch (const out_of_range&) {
  }

  unique_lock g(this->lock);
  try {
    return this->from_bb_credentials_locked(username, password);
  } catch (const out_of_range&) {
  }

  if (allow_create && password) {
    auto login = make_shared<Login>();
    login->account_was_created = true;
    login->account = make_shared<Account>();
    login->account->account_id = phosg::fnv1a32(username) & 0x7FFFFFFF;
    auto lic = make_shared<BBLicense>();
    lic->username = username;
    lic->password = *password;
    login->account->bb_licenses.emplace(lic->username, lic);
    login->bb_license = lic;
    this->add_locked(login->account);
    return login;
  } else {
    throw missing_account();
  }
}

vector<shared_ptr<Account>> AccountIndex::all() const {
  shared_lock g(this->lock);
  vector<shared_ptr<Account>> ret;
  ret.reserve(this->by_account_id.size());
  for (const auto& it : this->by_account_id) {
    ret.emplace_back(it.second);
  }
  return ret;
}

void AccountIndex::add(shared_ptr<Account> a) {
  unique_lock g(this->lock);
  this->add_locked(a);
}

void AccountIndex::add_locked(shared_ptr<Account> a) {
  if (this->force_all_temporary) {
    a->is_temporary = true;
  }

  for (const auto& it : a->dc_nte_licenses) {
    if (this->by_dc_nte_serial_number.count(it.second->serial_number)) {
      throw runtime_error("account already exists with this DC NTE serial number");
    }
  }
  for (const auto& it : a->dc_licenses) {
    if (this->by_dc_serial_number.count(it.second->serial_number)) {
      throw runtime_error("account already exists with this DC serial number");
    }
  }
  for (const auto& it : a->pc_licenses) {
    if (this->by_pc_serial_number.count(it.second->serial_number)) {
      throw runtime_error("account already exists with this PC NTE serial number");
    }
  }
  for (const auto& it : a->gc_licenses) {
    if (this->by_gc_serial_number.count(it.second->serial_number)) {
      throw runtime_error("account already exists with this GC serial number");
    }
  }
  for (const auto& it : a->xb_licenses) {
    if (this->by_xb_user_id.count(it.second->user_id)) {
      throw runtime_error("account already exists with this XB user ID");
    }
  }
  for (const auto& it : a->bb_licenses) {
    if (this->by_bb_username.count(it.second->username)) {
      throw runtime_error("account already exists with this BB username");
    }
  }

  while (this->by_account_id.count(a->account_id) || !a->account_id || (a->account_id == 0xFFFFFFFF)) {
    a->account_id = (a->account_id + 1) & 0x7FFFFFFF;
  }

  this->by_account_id[a->account_id] = a;
  for (const auto& it : a->dc_nte_licenses) {
    this->by_dc_nte_serial_number[it.second->serial_number] = a;
  }
  for (const auto& it : a->dc_licenses) {
    this->by_dc_serial_number[it.second->serial_number] = a;
  }
  for (const auto& it : a->pc_licenses) {
    this->by_pc_serial_number[it.second->serial_number] = a;
  }
  for (const auto& it : a->gc_licenses) {
    this->by_gc_serial_number[it.second->serial_number] = a;
  }
  for (const auto& it : a->xb_licenses) {
    this->by_xb_user_id[it.second->user_id] = a;
  }
  for (const auto& it : a->bb_licenses) {
    this->by_bb_username[it.second->username] = a;
  }
}

void AccountIndex::remove(uint32_t account_id) {
  unique_lock g(this->lock);
  auto acc_it = this->by_account_id.find(account_id);
  if (acc_it == this->by_account_id.end()) {
    throw out_of_range("account does not exist");
  }
  auto a = std::move(acc_it->second);
  this->by_account_id.erase(acc_it);

  for (const auto& it : a->dc_nte_licenses) {
    this->by_dc_nte_serial_number.erase(it.second->serial_number);
  }
  for (const auto& it : a->dc_licenses) {
    this->by_dc_serial_number.erase(it.second->serial_number);
  }
  for (const auto& it : a->pc_licenses) {
    this->by_pc_serial_number.erase(it.second->serial_number);
  }
  for (const auto& it : a->gc_licenses) {
    this->by_gc_serial_number.erase(it.second->serial_number);
  }
  for (const auto& it : a->xb_licenses) {
    this->by_xb_user_id.erase(it.second->user_id);
  }
  for (const auto& it : a->bb_licenses) {
    this->by_bb_username.erase(it.second->username);
  }
}

void AccountIndex::add_dc_nte_license(shared_ptr<Account> account, shared_ptr<DCNTELicense> license) {
  if (!this->by_dc_nte_serial_number.emplace(license->serial_number, account).second) {
    throw runtime_error("serial number already registered");
  }
  if (!account->dc_nte_licenses.emplace(license->serial_number, license).second) {
    this->by_dc_nte_serial_number.erase(license->serial_number);
    throw logic_error("serial number registered in account but not in account index");
  }
}

void AccountIndex::add_dc_license(shared_ptr<Account> account, shared_ptr<V1V2License> license) {
  if (!this->by_dc_serial_number.emplace(license->serial_number, account).second) {
    throw runtime_error("serial number already registered");
  }
  if (!account->dc_licenses.emplace(license->serial_number, license).second) {
    this->by_dc_serial_number.erase(license->serial_number);
    throw logic_error("serial number registered in account but not in account index");
  }
}

void AccountIndex::add_pc_license(shared_ptr<Account> account, shared_ptr<V1V2License> license) {
  if (!this->by_pc_serial_number.emplace(license->serial_number, account).second) {
    throw runtime_error("serial number already registered");
  }
  if (!account->pc_licenses.emplace(license->serial_number, license).second) {
    this->by_pc_serial_number.erase(license->serial_number);
    throw logic_error("serial number registered in account but not in account index");
  }
}

void AccountIndex::add_gc_license(shared_ptr<Account> account, shared_ptr<GCLicense> license) {
  if (!this->by_gc_serial_number.emplace(license->serial_number, account).second) {
    throw runtime_error("serial number already registered");
  }
  if (!account->gc_licenses.emplace(license->serial_number, license).second) {
    this->by_gc_serial_number.erase(license->serial_number);
    throw logic_error("serial number registered in account but not in account index");
  }
}

void AccountIndex::add_xb_license(shared_ptr<Account> account, shared_ptr<XBLicense> license) {
  if (!this->by_xb_user_id.emplace(license->user_id, account).second) {
    throw runtime_error("user ID already registered");
  }
  if (!account->xb_licenses.emplace(license->user_id, license).second) {
    this->by_xb_user_id.erase(license->user_id);
    throw logic_error("user ID registered in account but not in account index");
  }
}

void AccountIndex::add_bb_license(shared_ptr<Account> account, shared_ptr<BBLicense> license) {
  if (!this->by_bb_username.emplace(license->username, account).second) {
    throw runtime_error("username already registered");
  }
  if (!account->bb_licenses.emplace(license->username, license).second) {
    this->by_bb_username.erase(license->username);
    throw logic_error("username registered in account but not in account index");
  }
}

void AccountIndex::remove_dc_nte_license(shared_ptr<Account> account, const string& serial_number) {
  auto it = account->dc_nte_licenses.find(serial_number);
  if (it == account->dc_nte_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_dc_nte_serial_number.erase(it->second->serial_number)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->dc_nte_licenses.erase(it);
}

void AccountIndex::remove_dc_license(shared_ptr<Account> account, uint32_t serial_number) {
  auto it = account->dc_licenses.find(serial_number);
  if (it == account->dc_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_dc_serial_number.erase(it->second->serial_number)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->dc_licenses.erase(it);
}

void AccountIndex::remove_pc_license(shared_ptr<Account> account, uint32_t serial_number) {
  auto it = account->pc_licenses.find(serial_number);
  if (it == account->pc_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_pc_serial_number.erase(it->second->serial_number)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->pc_licenses.erase(it);
}

void AccountIndex::remove_gc_license(shared_ptr<Account> account, uint32_t serial_number) {
  auto it = account->gc_licenses.find(serial_number);
  if (it == account->gc_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_gc_serial_number.erase(it->second->serial_number)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->gc_licenses.erase(it);
}

void AccountIndex::remove_xb_license(shared_ptr<Account> account, uint64_t user_id) {
  auto it = account->xb_licenses.find(user_id);
  if (it == account->xb_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_xb_user_id.erase(it->second->user_id)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->xb_licenses.erase(it);
}

void AccountIndex::remove_bb_license(shared_ptr<Account> account, const string& username) {
  auto it = account->bb_licenses.find(username);
  if (it == account->bb_licenses.end()) {
    throw runtime_error("license not registered to account");
  }
  if (!this->by_bb_username.erase(it->second->username)) {
    throw runtime_error("license registered in account but not in account index");
  }
  account->bb_licenses.erase(it);
}

shared_ptr<Account> AccountIndex::create_temporary_account_for_shared_account(
    shared_ptr<const Account> src_a, const string& variation_data) const {
  auto ret = make_shared<Account>(*src_a);
  ret->is_temporary = true;
  ret->account_id = phosg::fnv1a32(&src_a->account_id, sizeof(src_a->account_id));
  ret->account_id = phosg::fnv1a32(variation_data, ret->account_id);
  return ret;
}

AccountIndex::AccountIndex(bool force_all_temporary)
    : force_all_temporary(force_all_temporary) {
  if (!this->force_all_temporary) {
    if (!phosg::isdir("system/licenses")) {
      mkdir("system/licenses", 0755);
    } else {
      for (const auto& item : phosg::list_directory("system/licenses")) {
        if (phosg::ends_with(item, ".json")) {
          try {
            phosg::JSON json = phosg::JSON::parse(phosg::load_file("system/licenses/" + item));
            this->add(make_shared<Account>(json));
          } catch (const exception& e) {
            phosg::log_error("Failed to index account %s", item.c_str());
            throw;
          }
        }
      }
    }
  }
}
