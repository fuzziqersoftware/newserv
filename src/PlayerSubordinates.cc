#include "PlayerSubordinates.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <algorithm>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <stdexcept>

#include "Client.hh"
#include "ItemData.hh"
#include "ItemParameterTable.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

using namespace std;

void PlayerDispDataBB::apply_dressing_room(const PlayerDispDataBBPreview& pre) {
  this->visual.name_color = pre.visual.name_color;
  this->visual.extra_model = pre.visual.extra_model;
  this->visual.name_color_checksum = pre.visual.name_color_checksum;
  this->visual.section_id = pre.visual.section_id;
  this->visual.char_class = pre.visual.char_class;
  this->visual.validation_flags = pre.visual.validation_flags;
  this->visual.version = pre.visual.version;
  this->visual.class_flags = pre.visual.class_flags;
  this->visual.costume = pre.visual.costume;
  this->visual.skin = pre.visual.skin;
  this->visual.face = pre.visual.face;
  this->visual.head = pre.visual.head;
  this->visual.hair = pre.visual.hair;
  this->visual.hair_r = pre.visual.hair_r;
  this->visual.hair_g = pre.visual.hair_g;
  this->visual.hair_b = pre.visual.hair_b;
  this->visual.proportion_x = pre.visual.proportion_x;
  this->visual.proportion_y = pre.visual.proportion_y;
  this->name = pre.name;
}

void GuildCardBB::clear() {
  this->guild_card_number = 0;
  this->name.clear();
  this->team_name.clear();
  this->description.clear();
  this->present = 0;
  this->language = 0;
  this->section_id = 0;
  this->char_class = 0;
}

GuildCardDCNTE::operator GuildCardBB() const {
  GuildCardBB ret;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardDC::operator GuildCardBB() const {
  GuildCardBB ret;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardPC::operator GuildCardBB() const {
  GuildCardBB ret;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardXB::operator GuildCardBB() const {
  GuildCardBB ret;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardBB::operator GuildCardDCNTE() const {
  GuildCardDCNTE ret;
  ret.player_tag = 0x00010000;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardBB::operator GuildCardDC() const {
  GuildCardDC ret;
  ret.player_tag = 0x00010000;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardBB::operator GuildCardPC() const {
  GuildCardPC ret;
  ret.player_tag = 0x00010000;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

GuildCardBB::operator GuildCardXB() const {
  GuildCardXB ret;
  ret.player_tag = 0x00010000;
  ret.guild_card_number = this->guild_card_number;
  ret.name.encode(this->name.decode(this->language), this->language);
  ret.description.encode(this->description.decode(this->language), this->language);
  ret.present = this->present;
  ret.language = this->language;
  ret.section_id = this->section_id;
  ret.char_class = this->char_class;
  return ret;
}

void PlayerLobbyDataPC::clear() {
  this->player_tag = 0;
  this->guild_card_number = 0;
  this->ip_address = 0;
  this->client_id = 0;
  this->name.clear();
}

void PlayerLobbyDataDCGC::clear() {
  this->player_tag = 0;
  this->guild_card_number = 0;
  this->ip_address = 0;
  this->client_id = 0;
  this->name.clear();
}

void XBNetworkLocation::clear() {
  this->internal_ipv4_address = 0;
  this->external_ipv4_address = 0;
  this->port = 0;
  this->mac_address.clear(0);
  this->sg_ip_address = 0;
  this->spi = 0;
  this->account_id = 0;
  this->unknown_a3.clear(0);
}

void PlayerLobbyDataXB::clear() {
  this->player_tag = 0;
  this->guild_card_number = 0;
  this->netloc.clear();
  this->client_id = 0;
  this->name.clear();
}

void PlayerLobbyDataBB::clear() {
  this->player_tag = 0;
  this->guild_card_number = 0;
  this->team_master_guild_card_number = 0;
  this->team_id = 0;
  this->unknown_a1.clear(0);
  this->client_id = 0;
  this->name.clear();
  this->hide_help_prompt = 0;
}

PlayerRecordsChallengeBB::PlayerRecordsChallengeBB(const PlayerRecordsChallengeDC& rec)
    : title_color(rec.title_color),
      unknown_u0(rec.unknown_u0),
      times_ep1_online(rec.times_ep1_online),
      times_ep2_online(0),
      times_ep1_offline(0),
      grave_is_ep2(0),
      grave_stage_num(rec.grave_stage_num),
      grave_floor(rec.grave_floor),
      unknown_g0(0),
      grave_deaths(rec.grave_deaths),
      unknown_u4(0),
      grave_time(rec.grave_time),
      grave_defeated_by_enemy_rt_index(rec.grave_defeated_by_enemy_rt_index),
      grave_x(rec.grave_x),
      grave_y(rec.grave_y),
      grave_z(rec.grave_z),
      grave_team(rec.grave_team.decode(), 1),
      grave_message(rec.grave_message.decode(), 1),
      unknown_m5(0),
      unknown_t6(0),
      rank_title(rec.rank_title.decode(), 1),
      unknown_l7(0) {}

PlayerRecordsChallengeBB::PlayerRecordsChallengeBB(const PlayerRecordsChallengePC& rec)
    : title_color(rec.title_color),
      unknown_u0(rec.unknown_u0),
      times_ep1_online(rec.times_ep1_online),
      times_ep2_online(0),
      times_ep1_offline(0),
      grave_is_ep2(0),
      grave_stage_num(rec.grave_stage_num),
      grave_floor(rec.grave_floor),
      unknown_g0(0),
      grave_deaths(rec.grave_deaths),
      unknown_u4(0),
      grave_time(rec.grave_time),
      grave_defeated_by_enemy_rt_index(rec.grave_defeated_by_enemy_rt_index),
      grave_x(rec.grave_x),
      grave_y(rec.grave_y),
      grave_z(rec.grave_z),
      grave_team(rec.grave_team.decode(), 1),
      grave_message(rec.grave_message.decode(), 1),
      unknown_m5(0),
      unknown_t6(0),
      rank_title(rec.rank_title.decode(), 1),
      unknown_l7(0) {}

PlayerRecordsChallengeBB::operator PlayerRecordsChallengeDC() const {
  PlayerRecordsChallengeDC ret;
  ret.title_color = this->title_color;
  ret.unknown_u0 = this->unknown_u0;
  ret.rank_title.encode(this->rank_title.decode());
  ret.times_ep1_online = this->times_ep1_online;
  if (this->grave_is_ep2) {
    ret.grave_stage_num = 0;
    ret.grave_floor = 0;
    ret.grave_defeated_by_enemy_rt_index = 0;
    ret.grave_x = 0;
    ret.grave_y = 0;
    ret.grave_z = 0;
  } else {
    ret.grave_stage_num = this->grave_stage_num;
    ret.grave_floor = this->grave_floor;
    ret.grave_defeated_by_enemy_rt_index = this->grave_defeated_by_enemy_rt_index;
    ret.grave_x = this->grave_x;
    ret.grave_y = this->grave_y;
    ret.grave_z = this->grave_z;
  }
  ret.grave_time = this->grave_time;
  ret.grave_deaths = this->grave_deaths;
  ret.grave_team.encode(this->grave_team.decode());
  ret.grave_message.encode(this->grave_message.decode());
  ret.times_ep1_offline = this->times_ep1_offline;
  ret.unknown_l4.clear(0);
  return ret;
}

PlayerRecordsChallengeBB::operator PlayerRecordsChallengePC() const {
  PlayerRecordsChallengePC ret;
  ret.title_color = this->title_color;
  ret.unknown_u0 = this->unknown_u0;
  ret.rank_title.encode(this->rank_title.decode());
  ret.times_ep1_online = this->times_ep1_online;
  if (this->grave_is_ep2) {
    ret.grave_stage_num = 0;
    ret.grave_floor = 0;
    ret.grave_defeated_by_enemy_rt_index = 0;
    ret.grave_x = 0;
    ret.grave_y = 0;
    ret.grave_z = 0;
  } else {
    ret.grave_stage_num = this->grave_stage_num;
    ret.grave_floor = this->grave_floor;
    ret.grave_defeated_by_enemy_rt_index = this->grave_defeated_by_enemy_rt_index;
    ret.grave_x = this->grave_x;
    ret.grave_y = this->grave_y;
    ret.grave_z = this->grave_z;
  }
  ret.grave_time = this->grave_time;
  ret.grave_deaths = this->grave_deaths;
  ret.grave_team.encode(this->grave_team.decode());
  ret.grave_message.encode(this->grave_message.decode());
  ret.times_ep1_offline = this->times_ep1_offline;
  ret.unknown_l4.clear(0);
  return ret;
}

QuestFlagsV1& QuestFlagsV1::operator=(const QuestFlags& other) {
  this->data[0] = other.data[0];
  this->data[1] = other.data[1];
  this->data[2] = other.data[2];
  return *this;
}

QuestFlagsV1::operator QuestFlags() const {
  QuestFlags ret;
  ret.data[0] = this->data[0];
  ret.data[1] = this->data[1];
  ret.data[2] = this->data[2];
  return ret;
}

BattleRules::BattleRules(const phosg::JSON& json) {
  static const phosg::JSON empty_list = phosg::JSON::list();

  this->tech_disk_mode = json.get_enum("TechDiskMode", this->tech_disk_mode);
  this->weapon_and_armor_mode = json.get_enum("WeaponAndArmorMode", this->weapon_and_armor_mode);
  this->mag_mode = json.get_enum("MagMode", this->mag_mode);
  this->tool_mode = json.get_enum("ToolMode", this->tool_mode);
  this->trap_mode = json.get_enum("TrapMode", this->trap_mode);
  this->unused_F817 = json.get_int("UnusedF817", this->unused_F817);
  this->respawn_mode = json.get_enum("RespawnMode", this->respawn_mode);
  this->replace_char = json.get_int("ReplaceChar", this->replace_char);
  this->drop_weapon = json.get_int("DropWeapon", this->drop_weapon);
  this->is_teams = json.get_int("IsTeams", this->is_teams);
  this->hide_target_reticle = json.get_int("HideTargetReticle", this->hide_target_reticle);
  this->meseta_mode = json.get_enum("MesetaMode", this->meseta_mode);
  this->death_level_up = json.get_int("DeathLevelUp", this->death_level_up);
  const phosg::JSON& trap_counts_json = json.get("TrapCounts", empty_list);
  for (size_t z = 0; z < trap_counts_json.size(); z++) {
    this->trap_counts[z] = trap_counts_json.at(z).as_int();
  }
  this->enable_sonar = json.get_int("EnableSonar", this->enable_sonar);
  this->sonar_count = json.get_int("SonarCount", this->sonar_count);
  this->forbid_scape_dolls = json.get_int("ForbidScapeDolls", this->forbid_scape_dolls);
  this->lives = json.get_int("Lives", this->lives);
  this->max_tech_level = json.get_int("MaxTechLevel", this->max_tech_level);
  this->char_level = json.get_int("CharLevel", this->char_level);
  this->time_limit = json.get_int("TimeLimit", this->time_limit);
  this->death_tech_level_up = json.get_int("DeathTechLevelUp", this->death_tech_level_up);
  this->box_drop_area = json.get_int("BoxDropArea", this->box_drop_area);
}

phosg::JSON BattleRules::json() const {
  return phosg::JSON::dict({
      {"TechDiskMode", this->tech_disk_mode},
      {"WeaponAndArmorMode", this->weapon_and_armor_mode},
      {"MagMode", this->mag_mode},
      {"ToolMode", this->tool_mode},
      {"TrapMode", this->trap_mode},
      {"UnusedF817", this->unused_F817},
      {"RespawnMode", this->respawn_mode},
      {"ReplaceChar", this->replace_char},
      {"DropWeapon", this->drop_weapon},
      {"IsTeams", this->is_teams},
      {"HideTargetReticle", this->hide_target_reticle},
      {"MesetaMode", this->meseta_mode},
      {"DeathLevelUp", this->death_level_up},
      {"TrapCounts", phosg::JSON::list({this->trap_counts[0], this->trap_counts[1], this->trap_counts[2], this->trap_counts[3]})},
      {"EnableSonar", this->enable_sonar},
      {"SonarCount", this->sonar_count},
      {"ForbidScapeDolls", this->forbid_scape_dolls},
      {"Lives", this->lives.load()},
      {"MaxTechLevel", this->max_tech_level.load()},
      {"CharLevel", this->char_level.load()},
      {"TimeLimit", this->time_limit.load()},
      {"DeathTechLevelUp", this->death_tech_level_up.load()},
      {"BoxDropArea", this->box_drop_area.load()},
  });
}

template <>
const char* phosg::name_for_enum<BattleRules::TechDiskMode>(BattleRules::TechDiskMode v) {
  switch (v) {
    case BattleRules::TechDiskMode::ALLOW:
      return "ALLOW";
    case BattleRules::TechDiskMode::FORBID_ALL:
      return "FORBID_ALL";
    case BattleRules::TechDiskMode::LIMIT_LEVEL:
      return "LIMIT_LEVEL";
    default:
      throw invalid_argument("invalid BattleRules::TechDiskMode value");
  }
}
template <>
BattleRules::TechDiskMode phosg::enum_for_name<BattleRules::TechDiskMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::TechDiskMode::ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::TechDiskMode::FORBID_ALL;
  } else if (!strcmp(name, "LIMIT_LEVEL")) {
    return BattleRules::TechDiskMode::LIMIT_LEVEL;
  } else {
    throw invalid_argument("invalid BattleRules::TechDiskMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::WeaponAndArmorMode>(BattleRules::WeaponAndArmorMode v) {
  switch (v) {
    case BattleRules::WeaponAndArmorMode::ALLOW:
      return "ALLOW";
    case BattleRules::WeaponAndArmorMode::CLEAR_AND_ALLOW:
      return "CLEAR_AND_ALLOW";
    case BattleRules::WeaponAndArmorMode::FORBID_ALL:
      return "FORBID_ALL";
    case BattleRules::WeaponAndArmorMode::FORBID_RARES:
      return "FORBID_RARES";
    default:
      throw invalid_argument("invalid BattleRules::WeaponAndArmorMode value");
  }
}
template <>
BattleRules::WeaponAndArmorMode phosg::enum_for_name<BattleRules::WeaponAndArmorMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::WeaponAndArmorMode::ALLOW;
  } else if (!strcmp(name, "CLEAR_AND_ALLOW")) {
    return BattleRules::WeaponAndArmorMode::CLEAR_AND_ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::WeaponAndArmorMode::FORBID_ALL;
  } else if (!strcmp(name, "FORBID_RARES")) {
    return BattleRules::WeaponAndArmorMode::FORBID_RARES;
  } else {
    throw invalid_argument("invalid BattleRules::WeaponAndArmorMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::MagMode>(BattleRules::MagMode v) {
  switch (v) {
    case BattleRules::MagMode::ALLOW:
      return "ALLOW";
    case BattleRules::MagMode::FORBID_ALL:
      return "FORBID_ALL";
    default:
      throw invalid_argument("invalid BattleRules::MagMode value");
  }
}
template <>
BattleRules::MagMode phosg::enum_for_name<BattleRules::MagMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::MagMode::ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::MagMode::FORBID_ALL;
  } else {
    throw invalid_argument("invalid BattleRules::MagMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::ToolMode>(BattleRules::ToolMode v) {
  switch (v) {
    case BattleRules::ToolMode::ALLOW:
      return "ALLOW";
    case BattleRules::ToolMode::CLEAR_AND_ALLOW:
      return "CLEAR_AND_ALLOW";
    case BattleRules::ToolMode::FORBID_ALL:
      return "FORBID_ALL";
    default:
      throw invalid_argument("invalid BattleRules::ToolMode value");
  }
}
template <>
BattleRules::ToolMode phosg::enum_for_name<BattleRules::ToolMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::ToolMode::ALLOW;
  } else if (!strcmp(name, "CLEAR_AND_ALLOW")) {
    return BattleRules::ToolMode::CLEAR_AND_ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::ToolMode::FORBID_ALL;
  } else {
    throw invalid_argument("invalid BattleRules::ToolMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::TrapMode>(BattleRules::TrapMode v) {
  switch (v) {
    case BattleRules::TrapMode::DEFAULT:
      return "DEFAULT";
    case BattleRules::TrapMode::ALL_PLAYERS:
      return "ALL_PLAYERS";
    default:
      throw invalid_argument("invalid BattleRules::TrapMode value");
  }
}
template <>
BattleRules::TrapMode phosg::enum_for_name<BattleRules::TrapMode>(const char* name) {
  if (!strcmp(name, "DEFAULT")) {
    return BattleRules::TrapMode::DEFAULT;
  } else if (!strcmp(name, "ALL_PLAYERS")) {
    return BattleRules::TrapMode::ALL_PLAYERS;
  } else {
    throw invalid_argument("invalid BattleRules::TrapMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::MesetaMode>(BattleRules::MesetaMode v) {
  switch (v) {
    case BattleRules::MesetaMode::ALLOW:
      return "ALLOW";
    case BattleRules::MesetaMode::FORBID_ALL:
      return "FORBID_ALL";
    case BattleRules::MesetaMode::CLEAR_AND_ALLOW:
      return "CLEAR_AND_ALLOW";
    default:
      throw invalid_argument("invalid BattleRules::MesetaDropMode value");
  }
}
template <>
BattleRules::MesetaMode phosg::enum_for_name<BattleRules::MesetaMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::MesetaMode::ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::MesetaMode::FORBID_ALL;
  } else if (!strcmp(name, "CLEAR_AND_ALLOW")) {
    return BattleRules::MesetaMode::CLEAR_AND_ALLOW;
  } else {
    throw invalid_argument("invalid BattleRules::MesetaDropMode name");
  }
}

template <>
const char* phosg::name_for_enum<BattleRules::RespawnMode>(BattleRules::RespawnMode v) {
  switch (v) {
    case BattleRules::RespawnMode::ALLOW:
      return "ALLOW";
    case BattleRules::RespawnMode::FORBID:
      return "FORBID";
    case BattleRules::RespawnMode::LIMIT_LIVES:
      return "LIMIT_LIVES";
    default:
      throw invalid_argument("invalid BattleRules::MesetaDropMode value");
  }
}
template <>
BattleRules::RespawnMode phosg::enum_for_name<BattleRules::RespawnMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::RespawnMode::ALLOW;
  } else if (!strcmp(name, "FORBID")) {
    return BattleRules::RespawnMode::FORBID;
  } else if (!strcmp(name, "LIMIT_LIVES")) {
    return BattleRules::RespawnMode::LIMIT_LIVES;
  } else {
    throw invalid_argument("invalid BattleRules::MesetaDropMode name");
  }
}

static PlayerInventoryItem make_template_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  return PlayerInventoryItem(ItemData(first_data, second_data), equipped);
}

static PlayerInventoryItem v2_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  auto ret = make_template_item(equipped, first_data, second_data);
  ret.data.decode_for_version(Version::PC_V2);
  return ret;
}

static PlayerInventoryItem v3_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  return make_template_item(equipped, first_data, second_data);
}

const ChallengeTemplateDefinition& get_challenge_template_definition(Version version, uint32_t class_flags, size_t index) {
  // clang-format off
  static const vector<ChallengeTemplateDefinition> v2_hunter_templates({
      {0,  {v2_item(true, 0x0001000000000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0300000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4,  {v2_item(true, 0x0001000500000000, 0x0000000000000000), v2_item(true,  0x0101010000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x02010D002003F501, 0x0100010000002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {6,  {v2_item(true, 0x0002000000000000, 0x0000000000000000), v2_item(true,  0x0101020000000000, 0x0000000000000000), v2_item(true,  0x0102010000000000, 0x0000000000000000), v2_item(true,  0x0201100020032103, 0x0100010000002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {9,  {v2_item(true, 0x0002000500000000, 0x0000000000000000), v2_item(true,  0x0101030000000000, 0x0000000000000000), v2_item(true,  0x0102020000000000, 0x0000000000000000), v2_item(true,  0x02011300E8032103, 0x0100650000002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0301000000020000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {v2_item(true, 0x0001010000000000, 0x0000000000000000), v2_item(true,  0x0101030000000000, 0x0000000000000000), v2_item(true,  0x0102030000000000, 0x0000000000000000), v2_item(true,  0x020116004C04E903, 0x0100650000002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x0301000000020000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {v2_item(true, 0x0001010500000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102030000000000, 0x0000000000000000), v2_item(true,  0x020118004C04E903, 0x6500C90000002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x0301000000020000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {17, {v2_item(true, 0x0002010000000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x02012000DC05B104, 0xC9002D0100002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301000000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {19, {v2_item(true, 0x0002010500000000, 0x0000000000000000), v2_item(true,  0x0101050000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x02012100DC051505, 0xC9002D0100002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301000000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {22, {v2_item(true, 0x0001020000000000, 0x0000000000000000), v2_item(true,  0x0101050000000000, 0x0000000000000000), v2_item(true,  0x0102050000000000, 0x0000000000000000), v2_item(true,  0x020E260008071505, 0x2D01910100002800), v2_item(false, 0x03000000000A0000, 0x0000000000000000), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301000000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v2_item(true, 0x0001030000000000, 0x0000000000000000), v2_item(true,  0x0101070000000000, 0x0000000000000000), v2_item(true,  0x0102070000000000, 0x0000000000000000), v2_item(true,  0x02054600D007B90B, 0xE903E90300002800), v2_item(false, 0x03000100000A0000, 0x0000000000000000), v2_item(false, 0x0301010000050000, 0x0000000000000000), v2_item(false, 0x0306010000050000, 0x0000000000000000), v2_item(false, 0x0306000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v2_item(true, 0x0001040000000000, 0x0000000000000000), v2_item(true,  0x01010E0000000000, 0x0000000000000000), v2_item(true,  0x01020E0000000000, 0x0000000000000000), v2_item(true,  0x02058C00A00F7117, 0xD107D10700002800), v2_item(false, 0x03000200000A0000, 0x0000000000000000), v2_item(false, 0x0301020000050000, 0x0000000000000000), v2_item(false, 0x0306010000050000, 0x0000000000000000), v2_item(false, 0x0306000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v2_item(true, 0x0001050000000000, 0x0000000000000000), v2_item(true,  0x0101160000000000, 0x0000000000000000), v2_item(true,  0x0102120000000000, 0x0000000000000000), v2_item(true,  0x0205B40070177117, 0xB90BB90B00002800), v2_item(false, 0x03000200000A0000, 0x0000000000000000), v2_item(false, 0x0301020000050000, 0x0000000000000000), v2_item(false, 0x0306010000050000, 0x0000000000000000), v2_item(false, 0x0306000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0,  {v2_item(true, 0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v2_item(true, 0x02054600D007B90B, 0xE903E90300002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v2_item(true, 0x02058200A00F8913, 0xD107D10700002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v2_item(true, 0x0205BE007017591B, 0xB90BB90B00002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> v2_ranger_templates({
      {0,  {v2_item(true, 0x0006000000000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0300000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4,  {v2_item(true, 0x0006000500000000, 0x0000000000000000), v2_item(true,  0x0101010000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x020D0C00F401C900, 0xF501010000002800), v2_item(false, 0x0300000000050000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {5,  {v2_item(true, 0x0006000500000000, 0x0000000000000000), v2_item(true,  0x0101010000000000, 0x0000000000000000), v2_item(true,  0x0102010000000000, 0x0000000000000000), v2_item(true,  0x020D0E00F401C900, 0xBD02010000002800), v2_item(false, 0x0300000000050000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {8,  {v2_item(true, 0x0006000500000000, 0x0000000000000000), v2_item(true,  0x0101020000000000, 0x0000000000000000), v2_item(true,  0x0102020000000000, 0x0000000000000000), v2_item(true,  0x020D1000F4012D01, 0x2103010000002800), v2_item(false, 0x0300000000050000, 0x0000000000000000), v2_item(false, 0x0301000000010000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {10, {v2_item(true, 0x0006000500000000, 0x0000000000000000), v2_item(true,  0x0101020000000000, 0x0000000000000000), v2_item(true,  0x0102030000000000, 0x0000000000000000), v2_item(true,  0x020D120058029101, 0x2103010000002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0300010000020000, 0x0000000000000000), v2_item(false, 0x0301000000010000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {v2_item(true, 0x0006010000000000, 0x0000000000000000), v2_item(true,  0x0101030000000000, 0x0000000000000000), v2_item(true,  0x0102030000000000, 0x0000000000000000), v2_item(true,  0x020D140058029101, 0x2103C90000002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0300010000020000, 0x0000000000000000), v2_item(false, 0x0301000000010000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {v2_item(true, 0x0006010500000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x020D1700BC02F501, 0x8503C90000002800), v2_item(false, 0x0300000000070000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x0301000000020000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {15, {v2_item(true, 0x0006010500000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x020D190020035902, 0x8503C90000002800), v2_item(false, 0x0300000000070000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x0301000000020000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {18, {v2_item(true, 0x0006020000000000, 0x0000000000000000), v2_item(true,  0x0101050000000000, 0x0000000000000000), v2_item(true,  0x0102050000000000, 0x0000000000000000), v2_item(true,  0x020D1E002003BD02, 0xB1042D0100002800), v2_item(false, 0x0300000000070000, 0x0000000000000000), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301000000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v2_item(true, 0x0006030000000000, 0x0000000000000000), v2_item(true,  0x0101070000000000, 0x0000000000000000), v2_item(true,  0x0102070000000000, 0x0000000000000000), v2_item(true,  0x020C4600D007E903, 0xB90BE90300002800), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301010000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v2_item(true, 0x0006040000000000, 0x0000000000000000), v2_item(true,  0x01010E0000000000, 0x0000000000000000), v2_item(true,  0x01020E0000000000, 0x0000000000000000), v2_item(true,  0x020C8C00B80BC509, 0x7117C50900002800), v2_item(false, 0x0300020000050000, 0x0000000000000000), v2_item(false, 0x0301020000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v2_item(true, 0x0006050000000000, 0x0000000000000000), v2_item(true,  0x0101160000000000, 0x0000000000000000), v2_item(true,  0x0102120000000000, 0x0000000000000000), v2_item(true,  0x0206B400B80BB90B, 0x2923B90B00002800), v2_item(false, 0x0300020000080000, 0x0000000000000000), v2_item(false, 0x0301020000050000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0308000000050000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0,  {v2_item(true, 0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v2_item(true, 0x020C4600D007E903, 0xB90BE90300002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v2_item(true, 0x020C8C00B80BC509, 0x7117C50900002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v2_item(true, 0x0206B400B80BB90B, 0x2923B90B00002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> v2_force_templates({
      {0,  {v2_item(true, 0x000A000000000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0300000000040000, 0x0000000000000000), v2_item(false, 0x0301000000040000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {4,  {v2_item(true, 0x000A000500000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x02190D0020036500, 0x0100910100002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0301000000060000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {6,  {v2_item(true, 0x000B000000000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x02190F002003C900, 0x0100F50100002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0301000000060000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {9,  {v2_item(true, 0x000B000500000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x0219120084032D01, 0x0100590200002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0301000000060000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {11, {v2_item(true, 0x000B000500000000, 0x0000000000000000), v2_item(true,  0x0101000000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x02191400E8032D01, 0x0100BD0200002800), v2_item(false, 0x0300000000060000, 0x0000000000000000), v2_item(false, 0x0300010000020000, 0x0000000000000000), v2_item(false, 0x0301000000080000, 0x0000000000000000), v2_item(false, 0x0301010000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {12, {v2_item(true, 0x000B000500000000, 0x0000000000000000), v2_item(true,  0x0101030000000000, 0x0000000000000000), v2_item(true,  0x0102000000000000, 0x0000000000000000), v2_item(true,  0x02191600E8039101, 0x6500BD0200002800), v2_item(false, 0x0300000000070000, 0x0000000000000000), v2_item(false, 0x0300010000020000, 0x0000000000000000), v2_item(false, 0x0301000000070000, 0x0000000000000000), v2_item(false, 0x0301010000030000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {15, {v2_item(true, 0x000B000A00000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x02191B00B004F501, 0xC900210300002800), v2_item(false, 0x0300000000070000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x0301000000080000, 0x0000000000000000), v2_item(false, 0x0301010000040000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {16, {v2_item(true, 0x000B000A00000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x02191D00B0045902, 0xC900850300002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000030000, 0x0000000000000000), v2_item(false, 0x03010000000A0000, 0x0000000000000000), v2_item(false, 0x0301010000040000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {19, {v2_item(true, 0x000A010000000000, 0x0000000000000000), v2_item(true,  0x0101040000000000, 0x0000000000000000), v2_item(true,  0x0102040000000000, 0x0000000000000000), v2_item(true,  0x02192200DC05BD02, 0xC900E90300002800), v2_item(false, 0x0300000000080000, 0x0000000000000000), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301010000050000, 0x0000000000000000), v2_item(false, 0x03010000000A0000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {24, {v2_item(true, 0x000A010A00000000, 0x0000000000000000), v2_item(true,  0x0101060000000000, 0x0000000000000000), v2_item(true,  0x0102060000000000, 0x0000000000000000), v2_item(true,  0x021C4600D007E903, 0xE903B90B00002800), v2_item(false, 0x0300010000050000, 0x0000000000000000), v2_item(false, 0x0301010000080000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 7}, {0x06, 7}, {0x03, 7}, {0x01, 4}, {0x04, 4}, {0x0D, 7}, {0x0A, 7}, {0x0B, 7}, {0x0C, 7}, {0x0F, 6}}},
      {50, {v2_item(true, 0x000A020000000000, 0x0000000000000000), v2_item(true,  0x01010E0000000000, 0x0000000000000000), v2_item(true,  0x01020D0000000000, 0x0000000000000000), v2_item(true,  0x021C8C00B80BD107, 0xD107591B00002800), v2_item(false, 0x0300020000050000, 0x0000000000000000), v2_item(false, 0x0301020000080000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 9}, {0x06, 9}, {0x03, 9}, {0x01, 9}, {0x04, 9}, {0x0D, 9}, {0x0A, 9}, {0x0B, 9}, {0x0C, 9}, {0x0F, 9}}},
      {99, {v2_item(true, 0x000A040000000000, 0x0000000000000000), v2_item(true,  0x0101160000000000, 0x0000000000000000), v2_item(true,  0x0102110000000000, 0x0000000000000000), v2_item(true,  0x021CB400AC0DD107, 0xC509112700002800), v2_item(false, 0x0300020000050000, 0x0000000000000000), v2_item(false, 0x03010200000A0000, 0x0000000000000000), v2_item(false, 0x0306010000030000, 0x0000000000000000), v2_item(false, 0x0306000000030000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 14}, {0x06, 14}, {0x03, 14}, {0x01, 14}, {0x04, 14}, {0x0D, 14}, {0x0A, 14}, {0x0B, 14}, {0x0C, 14}, {0x0F, 14}}},
      {0,  {v2_item(true, 0x02000500F4010100, 0x0100010000002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {24, {v2_item(true, 0x021C4600D007E903, 0xE903B90B00002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {50, {v2_item(true, 0x021C8C00B80BD107, 0xD107591B00002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {99, {v2_item(true, 0x021CB400AC0DD107, 0xC509112700002800), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000), v2_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}} },
  });

  static const vector<ChallengeTemplateDefinition> v3_hunter_templates({
      {0,  {v3_item(true, 0x0001000000000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0300000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4,  {v3_item(true, 0x0001000500000000, 0x0000000000000000), v3_item(true,  0x0101010000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x02010D002003F401, 0x0000000028000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {6,  {v3_item(true, 0x0002000000000000, 0x0000000000000000), v3_item(true,  0x0101020000000000, 0x0000000000000000), v3_item(true,  0x0102010000000000, 0x0000000000000000), v3_item(true,  0x0201100020032003, 0x0000000028000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {9,  {v3_item(true, 0x0002000500000000, 0x0000000000000000), v3_item(true,  0x0101030000000000, 0x0000000000000000), v3_item(true,  0x0102020000000000, 0x0000000000000000), v3_item(true,  0x02011300E8032003, 0x0000640028000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0301000000020000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {v3_item(true, 0x0001010000000000, 0x0000000000000000), v3_item(true,  0x0101030000000000, 0x0000000000000000), v3_item(true,  0x0102030000000000, 0x0000000000000000), v3_item(true,  0x020116004C04E803, 0x0000640028000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x0301000000020000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {v3_item(true, 0x0001010500000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102030000000000, 0x0000000000000000), v3_item(true,  0x020118004C04E803, 0x6400C80028000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x0301000000020000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {17, {v3_item(true, 0x0002010000000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x02012700DC056C07, 0xC8002C0128000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301000000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {19, {v3_item(true, 0x0002010500000000, 0x0000000000000000), v3_item(true,  0x0101050000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x02012200DC057805, 0xC8002C0128000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301000000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {22, {v3_item(true, 0x0001020000000000, 0x0000000000000000), v3_item(true,  0x0101050000000000, 0x0000000000000000), v3_item(true,  0x0102050000000000, 0x0000000000000000), v3_item(true,  0x020E260008071405, 0x2C01900128000012), v3_item(false, 0x03000000000A0000, 0x0000000000000000), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301000000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v3_item(true, 0x0001030000000000, 0x0000000000000000), v3_item(true,  0x0101070000000000, 0x0000000000000000), v3_item(true,  0x0102070000000000, 0x0000000000000000), v3_item(true,  0x02054600D007B80B, 0xE803E80328000012), v3_item(false, 0x03000100000A0000, 0x0000000000000000), v3_item(false, 0x0301010000050000, 0x0000000000000000), v3_item(false, 0x0306010000050000, 0x0000000000000000), v3_item(false, 0x0306000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v3_item(true, 0x0001040000000000, 0x0000000000000000), v3_item(true,  0x01010E0000000000, 0x0000000000000000), v3_item(true,  0x01020E0000000000, 0x0000000000000000), v3_item(true,  0x02058C00A00F7017, 0xD007D00728000012), v3_item(false, 0x03000200000A0000, 0x0000000000000000), v3_item(false, 0x0301020000050000, 0x0000000000000000), v3_item(false, 0x0306010000050000, 0x0000000000000000), v3_item(false, 0x0306000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v3_item(true, 0x0001050000000000, 0x0000000000000000), v3_item(true,  0x0101160000000000, 0x0000000000000000), v3_item(true,  0x0102120000000000, 0x0000000000000000), v3_item(true,  0x0205B40070177017, 0xB80BB80B28000012), v3_item(false, 0x03000200000A0000, 0x0000000000000000), v3_item(false, 0x0301020000050000, 0x0000000000000000), v3_item(false, 0x0306010000050000, 0x0000000000000000), v3_item(false, 0x0306000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0,  {v3_item(true, 0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v3_item(true, 0x02054600D007B80B, 0xE803E80328000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v3_item(true, 0x02058200A00F8813, 0xD007D00728000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v3_item(true, 0x0205BE007017581B, 0xB80BB80B28000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> v3_ranger_templates({
      {0,  {v3_item(true, 0x0006000000000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0300000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4,  {v3_item(true, 0x0006000500000000, 0x0000000000000000), v3_item(true,  0x0101010000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x020D0C00F401C800, 0xF401000028000012), v3_item(false, 0x0300000000050000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {5,  {v3_item(true, 0x0006000500000000, 0x0000000000000000), v3_item(true,  0x0101010000000000, 0x0000000000000000), v3_item(true,  0x0102010000000000, 0x0000000000000000), v3_item(true,  0x020D0E00F401C800, 0xBC02000028000012), v3_item(false, 0x0300000000050000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {8,  {v3_item(true, 0x0006000500000000, 0x0000000000000000), v3_item(true,  0x0101020000000000, 0x0000000000000000), v3_item(true,  0x0102020000000000, 0x0000000000000000), v3_item(true,  0x020D1000F4012C01, 0x2003000028000012), v3_item(false, 0x0300000000050000, 0x0000000000000000), v3_item(false, 0x0301000000010000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {10, {v3_item(true, 0x0006000500000000, 0x0000000000000000), v3_item(true,  0x0101020000000000, 0x0000000000000000), v3_item(true,  0x0102030000000000, 0x0000000000000000), v3_item(true,  0x020D120058029001, 0x2003000028000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0300010000020000, 0x0000000000000000), v3_item(false, 0x0301000000010000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {v3_item(true, 0x0006010000000000, 0x0000000000000000), v3_item(true,  0x0101030000000000, 0x0000000000000000), v3_item(true,  0x0102030000000000, 0x0000000000000000), v3_item(true,  0x020D140058029001, 0x2003C80028000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0300010000020000, 0x0000000000000000), v3_item(false, 0x0301000000010000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {v3_item(true, 0x0006010500000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x020D1700BC02F401, 0x8403C80028000012), v3_item(false, 0x0300000000070000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x0301000000020000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {15, {v3_item(true, 0x0006010500000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x020D190020035802, 0x8403C80028000012), v3_item(false, 0x0300000000070000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x0301000000020000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {18, {v3_item(true, 0x0006020000000000, 0x0000000000000000), v3_item(true,  0x0101050000000000, 0x0000000000000000), v3_item(true,  0x0102050000000000, 0x0000000000000000), v3_item(true,  0x020D1E002003BC02, 0xB0042C0128000012), v3_item(false, 0x0300000000070000, 0x0000000000000000), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301000000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v3_item(true, 0x0006030000000000, 0x0000000000000000), v3_item(true,  0x0101070000000000, 0x0000000000000000), v3_item(true,  0x0102070000000000, 0x0000000000000000), v3_item(true,  0x020C4600D007E803, 0xB80BE80328000012), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301010000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v3_item(true, 0x0006040000000000, 0x0000000000000000), v3_item(true,  0x01010E0000000000, 0x0000000000000000), v3_item(true,  0x01020E0000000000, 0x0000000000000000), v3_item(true,  0x020C8C00B80BC409, 0x7017C40928000012), v3_item(false, 0x0300020000050000, 0x0000000000000000), v3_item(false, 0x0301020000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v3_item(true, 0x0006050000000000, 0x0000000000000000), v3_item(true,  0x0101160000000000, 0x0000000000000000), v3_item(true,  0x0102120000000000, 0x0000000000000000), v3_item(true,  0x0206B400B80BB80B, 0x2823B80B28000012), v3_item(false, 0x0300020000080000, 0x0000000000000000), v3_item(false, 0x0301020000050000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0308000000050000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0,  {v3_item(true, 0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {v3_item(true, 0x020C4600D007E803, 0xB80BE80328000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {v3_item(true, 0x020C8C00B80BC409, 0x7017C40928000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {v3_item(true, 0x0206B400B80BB80B, 0x2823B80B28000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> v3_force_templates({
      {0,  {v3_item(true, 0x000A000000000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0300000000040000, 0x0000000000000000), v3_item(false, 0x0301000000040000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {4,  {v3_item(true, 0x000A000500000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x02190D0020036400, 0x0000900128000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0301000000060000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {6,  {v3_item(true, 0x000B000000000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x02190F002003C800, 0x0000F40128000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0301000000060000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {9,  {v3_item(true, 0x000B000500000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x0219120084032C01, 0x0000580228000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0301000000060000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {11, {v3_item(true, 0x000B000500000000, 0x0000000000000000), v3_item(true,  0x0101000000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x02191400E8032C01, 0x0000BC0228000012), v3_item(false, 0x0300000000060000, 0x0000000000000000), v3_item(false, 0x0300010000020000, 0x0000000000000000), v3_item(false, 0x0301000000080000, 0x0000000000000000), v3_item(false, 0x0301010000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {12, {v3_item(true, 0x000B000500000000, 0x0000000000000000), v3_item(true,  0x0101030000000000, 0x0000000000000000), v3_item(true,  0x0102000000000000, 0x0000000000000000), v3_item(true,  0x02191600E8039001, 0x6400BC0228000012), v3_item(false, 0x0300000000070000, 0x0000000000000000), v3_item(false, 0x0300010000020000, 0x0000000000000000), v3_item(false, 0x0301000000070000, 0x0000000000000000), v3_item(false, 0x0301010000030000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {15, {v3_item(true, 0x000B000A00000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x02191B00B004F401, 0xC800200328000012), v3_item(false, 0x0300000000070000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x0301000000080000, 0x0000000000000000), v3_item(false, 0x0301010000040000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {16, {v3_item(true, 0x000B000A00000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x02191D00B0045802, 0xC800840328000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000030000, 0x0000000000000000), v3_item(false, 0x03010000000A0000, 0x0000000000000000), v3_item(false, 0x0301010000040000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {19, {v3_item(true, 0x000A010000000000, 0x0000000000000000), v3_item(true,  0x0101040000000000, 0x0000000000000000), v3_item(true,  0x0102040000000000, 0x0000000000000000), v3_item(true,  0x02192200DC05BC02, 0xC800E80328000012), v3_item(false, 0x0300000000080000, 0x0000000000000000), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301010000050000, 0x0000000000000000), v3_item(false, 0x03010000000A0000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {24, {v3_item(true, 0x000A010A00000000, 0x0000000000000000), v3_item(true,  0x0101060000000000, 0x0000000000000000), v3_item(true,  0x0102060000000000, 0x0000000000000000), v3_item(true,  0x021C4600D007E803, 0xE803B80B28000012), v3_item(false, 0x0300010000050000, 0x0000000000000000), v3_item(false, 0x0301010000080000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 7}, {0x06, 7}, {0x03, 7}, {0x01, 4}, {0x04, 4}, {0x0D, 7}, {0x0A, 7}, {0x0B, 7}, {0x0C, 7}, {0x0F, 6}}},
      {50, {v3_item(true, 0x000A020000000000, 0x0000000000000000), v3_item(true,  0x01010E0000000000, 0x0000000000000000), v3_item(true,  0x01020D0000000000, 0x0000000000000000), v3_item(true,  0x021C8C00B80BD007, 0xD007581B28000012), v3_item(false, 0x0300020000050000, 0x0000000000000000), v3_item(false, 0x0301020000080000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 9}, {0x06, 9}, {0x03, 9}, {0x01, 9}, {0x04, 9}, {0x0D, 9}, {0x0A, 9}, {0x0B, 9}, {0x0C, 9}, {0x0F, 9}}},
      {99, {v3_item(true, 0x000A040000000000, 0x0000000000000000), v3_item(true,  0x0101160000000000, 0x0000000000000000), v3_item(true,  0x0102110000000000, 0x0000000000000000), v3_item(true,  0x021CB400AC0DD007, 0xC409102728000012), v3_item(false, 0x0300020000050000, 0x0000000000000000), v3_item(false, 0x03010200000A0000, 0x0000000000000000), v3_item(false, 0x0306010000030000, 0x0000000000000000), v3_item(false, 0x0306000000030000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 14}, {0x06, 14}, {0x03, 14}, {0x01, 14}, {0x04, 14}, {0x0D, 14}, {0x0A, 14}, {0x0B, 14}, {0x0C, 14}, {0x0F, 14}}},
      {0,  {v3_item(true, 0x02000500F4010000, 0x0000000028000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {24, {v3_item(true, 0x021C4600D007E803, 0xE803B80B28000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {50, {v3_item(true, 0x021C8C00B80BD007, 0xD007581B28000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
      {99, {v3_item(true, 0x021CB400AC0DD007, 0xC409102728000012), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000), v3_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 0}}},
  });
  // clang-format on

  if ((class_flags & 0xE0) == 0x20) {
    return is_v1_or_v2(version) ? v2_hunter_templates.at(index) : v3_hunter_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x40) {
    return is_v1_or_v2(version) ? v2_ranger_templates.at(index) : v3_ranger_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x80) {
    return is_v1_or_v2(version) ? v2_force_templates.at(index) : v3_force_templates.at(index);
  } else {
    throw runtime_error("invalid class flags on original player");
  }
}

const QuestFlagsForDifficulty bb_quest_flag_apply_mask{{
    // clang-format off
    /* 0000 */ 0x00, 0x3F, 0xFF, 0xE3, 0xE0, 0xFF, 0xFF, 0x00,
    /* 0040 */ 0x03, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    /* 0080 */ 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x00, 0x00,
    /* 00C0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0100 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xFC, 0x00,
    /* 0140 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0180 */ 0xFE, 0x00, 0x7F, 0xFE, 0x0F, 0xFF, 0xFF, 0x80,
    /* 01C0 */ 0x3F, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x0F, 0xFF,
    /* 0200 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
    /* 0240 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0280 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
    /* 02C0 */ 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0300 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0340 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0380 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 03C0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // clang-format on

    // The flags in the above mask are:
    // 000A 000B 000C 000D 000E 000F 0010 0011 0012 0013 0014 0015 0016 0017
    // 0018 0019 001A 001E 001F 0020 0021 0022 0028 0029 002A 002B 002C 002D
    // 002E 002F 0030 0031 0032 0033 0034 0035 0036 0037 0046 0047 0048 0049
    // 004A 004B 004C 004D 004E 004F 0050 0051 0052 0053 0054 0055 0056 0057
    // 0058 0059 005A 005B 005C 005D 005E 005F 0060 0061 0062 0063 0097 0098
    // 0099 009A 012D 012E 012F 0130 0131 0132 0133 0134 0135 0140 0141 0142
    // 0143 0144 0145 0146 0147 0148 0149 014A 014B 014C 014D 014E 014F 0150
    // 0151 0152 0153 0154 0155 0156 0157 0158 0159 015A 015B 015C 015D 015E
    // 015F 0160 0161 0162 0163 0164 0165 0166 0167 0168 0169 016A 016B 016C
    // 016D 016E 016F 0170 0171 0172 0173 0174 0175 0176 0177 0178 0179 017A
    // 017B 017C 017D 017E 017F 0180 0181 0182 0183 0184 0185 0186 0191 0192
    // 0193 0194 0195 0196 0197 0198 0199 019A 019B 019C 019D 019E 01A4 01A5
    // 01A6 01A7 01A8 01A9 01AA 01AB 01AC 01AD 01AE 01AF 01B0 01B1 01B2 01B3
    // 01B4 01B5 01B6 01B7 01B8 01C2 01C3 01C4 01C5 01C6 01C7 01C8 01C9 01CA
    // 01CB 01CC 01CD 01CE 01CF 01D0 01D1 01D2 01D3 01D4 01D5 01D6 01F4 01F5
    // 01F6 01F7 01F8 01F9 01FA 01FB 01FC 01FD 01FE 01FF 0200 0201 0202 0203
    // 0204 0205 0206 0207 0208 0209 020A 020B 020C 020D 020E 020F 0210 0211
    // 0212 0213 0214 0215 0216 0217 0218 0219 021A 021B 021C 021D 021E 021F
    // 0220 0221 0222 0223 0224 0225 0226 0227 0228 0229 022A 022B 022C 022D
    // 022E 022F 0230 0231 0232 0233 0234 0235 02BD 02BE 02BF 02C0 02C1 02C2
    // 02C3 02C4
}};
