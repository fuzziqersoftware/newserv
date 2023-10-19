#include "PlayerSubordinates.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <stdexcept>

#include "ItemData.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

using namespace std;

FileContentsCache player_files_cache(300 * 1000 * 1000);

void PlayerDispDataDCPCV3::enforce_lobby_join_limits(GameVersion target_version) {
  if ((target_version == GameVersion::PC) || (target_version == GameVersion::DC)) {
    // V1/V2 have fewer classes, so we'll substitute some here
    if (this->visual.char_class == 9) {
      this->visual.char_class = 5; // HUcaseal -> RAcaseal
    } else if (this->visual.char_class == 10) {
      this->visual.char_class = 0; // FOmar -> HUmar
    } else if (this->visual.char_class == 11) {
      this->visual.char_class = 1; // RAmarl -> HUnewearl
    }

    // V1/V2 has fewer costumes, so substitute them here too
    this->visual.costume %= 9;

    // If the player is somehow still not a valid class, make them appear as the
    // "ninja" NPC
    if (this->visual.char_class > 8) {
      this->visual.extra_model = 0;
      this->visual.v2_flags |= 2;
    }
    this->visual.version = 2;
  }
}

void PlayerDispDataBB::enforce_lobby_join_limits(GameVersion) {
  this->play_time = 0;
}

PlayerDispDataBB PlayerDispDataDCPCV3::to_bb() const {
  PlayerDispDataBB bb;
  bb.stats = this->stats;
  bb.visual = this->visual;
  bb.visual.name = "         0";
  bb.name = this->visual.name;
  bb.config = this->config;
  bb.technique_levels_v1 = this->technique_levels_v1;
  return bb;
}

PlayerDispDataDCPCV3 PlayerDispDataBB::to_dcpcv3() const {
  PlayerDispDataDCPCV3 ret;
  ret.stats = this->stats;
  ret.visual = this->visual;
  ret.visual.name = this->name;
  remove_language_marker_inplace(ret.visual.name);
  ret.config = this->config;
  ret.technique_levels_v1 = this->technique_levels_v1;
  return ret;
}

PlayerDispDataBBPreview PlayerDispDataBB::to_preview() const {
  PlayerDispDataBBPreview pre;
  pre.level = this->stats.level;
  pre.experience = this->stats.experience;
  pre.visual = this->visual;
  pre.name = this->name;
  pre.play_time = this->play_time;
  return pre;
}

void PlayerDispDataBB::apply_preview(const PlayerDispDataBBPreview& pre) {
  this->stats.level = pre.level;
  this->stats.experience = pre.experience;
  this->visual = pre.visual;
  this->name = pre.name;
}

void PlayerDispDataBB::apply_dressing_room(const PlayerDispDataBBPreview& pre) {
  this->visual.name_color = pre.visual.name_color;
  this->visual.extra_model = pre.visual.extra_model;
  this->visual.unknown_a3 = pre.visual.unknown_a3;
  this->visual.section_id = pre.visual.section_id;
  this->visual.char_class = pre.visual.char_class;
  this->visual.v2_flags = pre.visual.v2_flags;
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
  this->name.clear(0);
  this->team_name.clear(0);
  this->description.clear(0);
  this->present = 0;
  this->language = 0;
  this->section_id = 0;
  this->char_class = 0;
}

void GuildCardEntryBB::clear() {
  this->data.clear();
  this->unknown_a1.clear(0);
}

uint32_t GuildCardFileBB::checksum() const {
  return crc32(this, sizeof(*this));
}

void PlayerBank::load(const string& filename) {
  *this = player_files_cache.get_obj_or_load<PlayerBank>(filename).obj;
  for (uint32_t x = 0; x < this->num_items; x++) {
    this->items[x].data.id = 0x0F010000 + x;
  }
}

void PlayerBank::save(const string& filename, bool save_to_filesystem) const {
  player_files_cache.replace(filename, this, sizeof(*this));
  if (save_to_filesystem) {
    save_file(filename, this, sizeof(*this));
  }
}

void PlayerLobbyDataPC::clear() {
  this->player_tag = 0;
  this->guild_card = 0;
  this->ip_address = 0;
  this->client_id = 0;
  ptext<char16_t, 0x10> name;
}

void PlayerLobbyDataDCGC::clear() {
  this->player_tag = 0;
  this->guild_card = 0;
  this->ip_address = 0;
  this->client_id = 0;
  ptext<char, 0x10> name;
}

void XBNetworkLocation::clear() {
  this->internal_ipv4_address = 0;
  this->external_ipv4_address = 0;
  this->port = 0;
  this->mac_address.clear(0);
  this->unknown_a1.clear(0);
  this->account_id = 0;
  this->unknown_a2.clear(0);
}

void PlayerLobbyDataXB::clear() {
  this->player_tag = 0;
  this->guild_card = 0;
  this->netloc.clear();
  this->client_id = 0;
  this->name.clear(0);
}

void PlayerLobbyDataBB::clear() {
  this->player_tag = 0;
  this->guild_card = 0;
  this->ip_address = 0;
  this->unknown_a1.clear(0);
  this->client_id = 0;
  this->name.clear(0);
  this->unknown_a2 = 0;
}

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsDC_Challenge& rec)
    : title_color(rec.title_color),
      unknown_u0(rec.unknown_u0),
      times_ep1_online(rec.times_ep1_online),
      times_ep2_online(0),
      times_ep1_offline(0),
      unknown_g3(rec.unknown_g3),
      grave_deaths(rec.grave_deaths),
      unknown_u4(0),
      grave_coords_time(rec.grave_coords_time),
      grave_team(rec.grave_team),
      grave_message(rec.grave_message),
      unknown_m5(0),
      unknown_t6(0),
      rank_title(encrypt_challenge_rank_text(decode_sjis(decrypt_challenge_rank_text(rec.rank_title)))),
      unknown_l7(0) {}

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsPC_Challenge& rec)
    : title_color(rec.title_color),
      unknown_u0(rec.unknown_u0),
      times_ep1_online(rec.times_ep1_online),
      times_ep2_online(0),
      times_ep1_offline(0),
      unknown_g3(rec.unknown_g3),
      grave_deaths(rec.grave_deaths),
      unknown_u4(0),
      grave_coords_time(rec.grave_coords_time),
      grave_team(rec.grave_team),
      grave_message(rec.grave_message),
      unknown_m5(0),
      unknown_t6(0),
      rank_title(rec.rank_title),
      unknown_l7(0) {}

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsV3_Challenge<false>& rec)
    : title_color(rec.stats.title_color),
      unknown_u0(rec.stats.unknown_u0),
      times_ep1_online(rec.stats.times_ep1_online),
      times_ep2_online(rec.stats.times_ep2_online),
      times_ep1_offline(rec.stats.times_ep1_offline),
      unknown_g3(rec.stats.unknown_g3),
      grave_deaths(rec.stats.grave_deaths),
      unknown_u4(rec.stats.unknown_u4),
      grave_coords_time(rec.stats.grave_coords_time),
      grave_team(rec.stats.grave_team),
      grave_message(rec.stats.grave_message),
      unknown_m5(rec.stats.unknown_m5),
      unknown_t6(rec.stats.unknown_t6),
      rank_title(encrypt_challenge_rank_text(decode_sjis(decrypt_challenge_rank_text(rec.rank_title)))),
      unknown_l7(rec.unknown_l7) {}

PlayerRecordsBB_Challenge::operator PlayerRecordsDC_Challenge() const {
  PlayerRecordsDC_Challenge ret;
  ret.title_color = this->title_color;
  ret.unknown_u0 = this->unknown_u0;
  ret.rank_title = encrypt_challenge_rank_text(encode_sjis(decrypt_challenge_rank_text(this->rank_title)));
  ret.times_ep1_online = this->times_ep1_online;
  ret.unknown_g3 = 0;
  ret.grave_deaths = this->grave_deaths;
  ret.grave_coords_time = this->grave_coords_time;
  ret.grave_team = this->grave_team;
  ret.grave_message = this->grave_message;
  ret.times_ep1_offline = this->times_ep1_offline;
  ret.unknown_l4.clear(0);
  return ret;
}

PlayerRecordsBB_Challenge::operator PlayerRecordsPC_Challenge() const {
  PlayerRecordsPC_Challenge ret;
  ret.title_color = this->title_color;
  ret.unknown_u0 = this->unknown_u0;
  ret.rank_title = this->rank_title;
  ret.times_ep1_online = this->times_ep1_online;
  ret.unknown_g3 = 0;
  ret.grave_deaths = this->grave_deaths;
  ret.grave_coords_time = this->grave_coords_time;
  ret.grave_team = this->grave_team;
  ret.grave_message = this->grave_message;
  ret.times_ep1_offline = this->times_ep1_offline;
  ret.unknown_l4.clear(0);
  return ret;
}

PlayerRecordsBB_Challenge::operator PlayerRecordsV3_Challenge<false>() const {
  PlayerRecordsV3_Challenge<false> ret;
  ret.stats.title_color = this->title_color;
  ret.stats.unknown_u0 = this->unknown_u0;
  ret.stats.times_ep1_online = this->times_ep1_online;
  ret.stats.times_ep2_online = this->times_ep2_online;
  ret.stats.times_ep1_offline = this->times_ep1_offline;
  ret.stats.unknown_g3 = this->unknown_g3;
  ret.stats.grave_deaths = this->grave_deaths;
  ret.stats.unknown_u4 = this->unknown_u4;
  ret.stats.grave_coords_time = this->grave_coords_time;
  ret.stats.grave_team = this->grave_team;
  ret.stats.grave_message = this->grave_message;
  ret.stats.unknown_m5 = this->unknown_m5;
  ret.stats.unknown_t6 = this->unknown_t6;
  ret.rank_title = encrypt_challenge_rank_text(encode_sjis(decrypt_challenge_rank_text(this->rank_title)));
  ret.unknown_l7 = this->unknown_l7;
  return ret;
}

PlayerInventory::PlayerInventory()
    : num_items(0),
      hp_materials_used(0),
      tp_materials_used(0),
      language(0) {}

void PlayerBank::add_item(const ItemData& item) {
  uint32_t pid = item.primary_identifier();

  if (pid == MESETA_IDENTIFIER) {
    this->meseta += item.data2d;
    if (this->meseta > 999999) {
      this->meseta = 999999;
    }
    return;
  }

  size_t combine_max = item.max_stack_size();
  if (combine_max > 1) {
    size_t y;
    for (y = 0; y < this->num_items; y++) {
      if (this->items[y].data.primary_identifier() == item.primary_identifier()) {
        break;
      }
    }

    if (y < this->num_items) {
      this->items[y].data.data1[5] += item.data1[5];
      if (this->items[y].data.data1[5] > combine_max) {
        this->items[y].data.data1[5] = combine_max;
      }
      this->items[y].amount = this->items[y].data.data1[5];
      return;
    }
  }

  if (this->num_items >= 200) {
    throw runtime_error("bank is full");
  }
  auto& last_item = this->items[this->num_items];
  last_item.data = item;
  last_item.amount = (item.max_stack_size() > 1) ? item.data1[5] : 1;
  last_item.present = 1;
  this->num_items++;
}

ItemData PlayerBank::remove_item(uint32_t item_id, uint32_t amount) {
  ItemData ret;

  if (item_id == 0xFFFFFFFF) {
    if (amount > this->meseta) {
      throw out_of_range("player does not have enough meseta");
    }
    ret.data1[0] = 0x04;
    ret.data2d = amount;
    this->meseta -= amount;
    return ret;
  }

  size_t index = this->find_item(item_id);
  auto& bank_item = this->items[index];

  if (amount && (bank_item.data.stack_size() > 1) && (amount < bank_item.data.data1[5])) {
    ret = bank_item.data;
    ret.data1[5] = amount;
    bank_item.data.data1[5] -= amount;
    bank_item.amount -= amount;
    return ret;
  }

  ret = bank_item.data;
  this->num_items--;
  for (size_t x = index; x < this->num_items; x++) {
    this->items[x] = this->items[x + 1];
  }
  auto& last_item = this->items[this->num_items];
  last_item.amount = 0;
  last_item.present = 0;
  last_item.data.clear();
  return ret;
}

size_t PlayerInventory::find_item(uint32_t item_id) const {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

size_t PlayerInventory::find_equipped_weapon() const {
  ssize_t ret = -1;
  for (size_t y = 0; y < this->num_items; y++) {
    if (!(this->items[y].flags & 0x00000008)) {
      continue;
    }
    if (this->items[y].data.data1[0] != 0) {
      continue;
    }
    if (ret < 0) {
      ret = y;
    } else {
      throw runtime_error("multiple weapons are equipped");
    }
  }
  if (ret < 0) {
    throw out_of_range("no weapon is equipped");
  }
  return ret;
}

size_t PlayerInventory::find_equipped_armor() const {
  ssize_t ret = -1;
  for (size_t y = 0; y < this->num_items; y++) {
    if (!(this->items[y].flags & 0x00000008)) {
      continue;
    }
    if (this->items[y].data.data1[0] != 1 || this->items[y].data.data1[1] != 1) {
      continue;
    }
    if (ret < 0) {
      ret = y;
    } else {
      throw runtime_error("multiple armors are equipped");
    }
  }
  if (ret < 0) {
    throw out_of_range("no armor is equipped");
  }
  return ret;
}

size_t PlayerInventory::find_equipped_mag() const {
  ssize_t ret = -1;
  for (size_t y = 0; y < this->num_items; y++) {
    if (!(this->items[y].flags & 0x00000008)) {
      continue;
    }
    if (this->items[y].data.data1[0] != 2) {
      continue;
    }
    if (ret < 0) {
      ret = y;
    } else {
      throw runtime_error("multiple mags are equipped");
    }
  }
  if (ret < 0) {
    throw out_of_range("no mag is equipped");
  }
  return ret;
}

size_t PlayerInventory::remove_all_items_of_type(uint8_t data1_0, int16_t data1_1) {
  size_t write_offset = 0;
  for (size_t read_offset = 0; read_offset < this->num_items; read_offset++) {
    bool should_delete = ((this->items[read_offset].data.data1[0] == data1_0) &&
        ((data1_1 < 0) || (this->items[read_offset].data.data1[1] == static_cast<uint8_t>(data1_1))));
    if (!should_delete) {
      if (read_offset != write_offset) {
        this->items[write_offset].present = this->items[read_offset].present;
        this->items[write_offset].flags = this->items[read_offset].flags;
        this->items[write_offset].data = this->items[read_offset].data;
      }
      write_offset++;
    }
  }
  size_t ret = this->num_items - write_offset;
  this->num_items = write_offset;
  return ret;
}

size_t PlayerBank::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

BattleRules::BattleRules(const JSON& json) {
  this->tech_disk_mode = json.get_enum("tech_disk_mode", this->tech_disk_mode);
  this->weapon_and_armor_mode = json.get_enum("weapon_and_armor_mode", this->weapon_and_armor_mode);
  this->forbid_mags = json.get_bool("forbid_mags", this->forbid_mags);
  this->tool_mode = json.get_enum("tool_mode", this->tool_mode);
  this->meseta_drop_mode = json.get_enum("meseta_drop_mode", this->meseta_drop_mode);
  this->forbid_scape_dolls = json.get_bool("forbid_scape_dolls", this->forbid_scape_dolls);
  this->max_tech_disk_level = json.get_int("max_tech_disk_level", this->max_tech_disk_level);
  this->replace_char = json.get_bool("replace_char", this->replace_char);
  this->char_level = json.get_int("char_level", this->char_level);
  this->box_drop_area = json.get_int("box_drop_area", this->box_drop_area);
}

JSON BattleRules::json() const {
  return JSON::dict({
      {"tech_disk_mode", this->tech_disk_mode},
      {"weapon_and_armor_mode", this->weapon_and_armor_mode},
      {"forbid_mags", this->forbid_mags},
      {"tool_mode", this->tool_mode},
      {"meseta_drop_mode", this->meseta_drop_mode},
      {"forbid_scape_dolls", this->forbid_scape_dolls},
      {"max_tech_disk_level", this->max_tech_disk_level},
      {"replace_char", this->replace_char},
      {"char_level", this->char_level},
      {"box_drop_area", this->box_drop_area},
  });
}

template <>
const char* name_for_enum<BattleRules::TechDiskMode>(BattleRules::TechDiskMode v) {
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
BattleRules::TechDiskMode enum_for_name<BattleRules::TechDiskMode>(const char* name) {
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
const char* name_for_enum<BattleRules::WeaponAndArmorMode>(BattleRules::WeaponAndArmorMode v) {
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
BattleRules::WeaponAndArmorMode enum_for_name<BattleRules::WeaponAndArmorMode>(const char* name) {
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
const char* name_for_enum<BattleRules::ToolMode>(BattleRules::ToolMode v) {
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
BattleRules::ToolMode enum_for_name<BattleRules::ToolMode>(const char* name) {
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
const char* name_for_enum<BattleRules::MesetaDropMode>(BattleRules::MesetaDropMode v) {
  switch (v) {
    case BattleRules::MesetaDropMode::ALLOW:
      return "ALLOW";
    case BattleRules::MesetaDropMode::FORBID_ALL:
      return "FORBID_ALL";
    case BattleRules::MesetaDropMode::CLEAR_AND_ALLOW:
      return "CLEAR_AND_ALLOW";
    default:
      throw invalid_argument("invalid BattleRules::MesetaDropMode value");
  }
}
template <>
BattleRules::MesetaDropMode enum_for_name<BattleRules::MesetaDropMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::MesetaDropMode::ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::MesetaDropMode::FORBID_ALL;
  } else if (!strcmp(name, "CLEAR_AND_ALLOW")) {
    return BattleRules::MesetaDropMode::CLEAR_AND_ALLOW;
  } else {
    throw invalid_argument("invalid BattleRules::MesetaDropMode name");
  }
}

const ChallengeTemplateDefinition& get_challenge_template_definition(uint32_t class_flags, size_t index) {
  static auto make_template_item = +[](bool equipped, uint64_t first_data, uint64_t second_data = 0) -> PlayerInventoryItem {
    PlayerInventoryItem ret = {
        .present = 1,
        .extension_data1 = 0,
        .extension_data2 = 0,
        .flags = (equipped ? 8 : 0),
        .data = ItemData()};
    ret.data.data1[0] = first_data >> 56;
    ret.data.data1[1] = first_data >> 48;
    ret.data.data1[2] = first_data >> 40;
    ret.data.data1[3] = first_data >> 32;
    ret.data.data1[4] = first_data >> 24;
    ret.data.data1[5] = first_data >> 16;
    ret.data.data1[6] = first_data >> 8;
    ret.data.data1[7] = first_data >> 0;
    ret.data.data1[8] = second_data >> 56;
    ret.data.data1[9] = second_data >> 48;
    ret.data.data1[10] = second_data >> 40;
    ret.data.data1[11] = second_data >> 32;
    ret.data.data2[0] = second_data >> 24;
    ret.data.data2[1] = second_data >> 16;
    ret.data.data2[2] = second_data >> 8;
    ret.data.data2[3] = second_data >> 0;
    return ret;
  };

  // clang-format off
  static const vector<ChallengeTemplateDefinition> hunter_templates({
      {0, {make_template_item(true, 0x0001000000000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0300000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4, {make_template_item(true, 0x0001000500000000, 0x0000000000000000), make_template_item(true, 0x0101010000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x02010D002003F401, 0x0000000028000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {6, {make_template_item(true, 0x0002000000000000, 0x0000000000000000), make_template_item(true, 0x0101020000000000, 0x0000000000000000), make_template_item(true, 0x0102010000000000, 0x0000000000000000), make_template_item(true, 0x0201100020032003, 0x0000000028000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {9, {make_template_item(true, 0x0002000500000000, 0x0000000000000000), make_template_item(true, 0x0101030000000000, 0x0000000000000000), make_template_item(true, 0x0102020000000000, 0x0000000000000000), make_template_item(true, 0x02011300E8032003, 0x0000640028000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0301000000020000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {make_template_item(true, 0x0001010000000000, 0x0000000000000000), make_template_item(true, 0x0101030000000000, 0x0000000000000000), make_template_item(true, 0x0102030000000000, 0x0000000000000000), make_template_item(true, 0x020116004C04E803, 0x0000640028000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x0301000000020000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {make_template_item(true, 0x0001010500000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102030000000000, 0x0000000000000000), make_template_item(true, 0x020118004C04E803, 0x6400C80028000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x0301000000020000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {17, {make_template_item(true, 0x0002010000000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x02012700DC056C07, 0xC8002C0128000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301000000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {19, {make_template_item(true, 0x0002010500000000, 0x0000000000000000), make_template_item(true, 0x0101050000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x02012200DC057805, 0xC8002C0128000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301000000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {22, {make_template_item(true, 0x0001020000000000, 0x0000000000000000), make_template_item(true, 0x0101050000000000, 0x0000000000000000), make_template_item(true, 0x0102050000000000, 0x0000000000000000), make_template_item(true, 0x020E260008071405, 0x2C01900128000012), make_template_item(false, 0x03000000000A0000, 0x0000000000000000), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301000000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {make_template_item(true, 0x0001030000000000, 0x0000000000000000), make_template_item(true, 0x0101070000000000, 0x0000000000000000), make_template_item(true, 0x0102070000000000, 0x0000000000000000), make_template_item(true, 0x02054600D007B80B, 0xE803E80328000012), make_template_item(false, 0x03000100000A0000, 0x0000000000000000), make_template_item(false, 0x0301010000050000, 0x0000000000000000), make_template_item(false, 0x0306010000050000, 0x0000000000000000), make_template_item(false, 0x0306000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {make_template_item(true, 0x0001040000000000, 0x0000000000000000), make_template_item(true, 0x01010E0000000000, 0x0000000000000000), make_template_item(true, 0x01020E0000000000, 0x0000000000000000), make_template_item(true, 0x02058C00A00F7017, 0xD007D00728000012), make_template_item(false, 0x03000200000A0000, 0x0000000000000000), make_template_item(false, 0x0301020000050000, 0x0000000000000000), make_template_item(false, 0x0306010000050000, 0x0000000000000000), make_template_item(false, 0x0306000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {make_template_item(true, 0x0001050000000000, 0x0000000000000000), make_template_item(true, 0x0101160000000000, 0x0000000000000000), make_template_item(true, 0x0102120000000000, 0x0000000000000000), make_template_item(true, 0x0205B40070177017, 0xB80BB80B28000012), make_template_item(false, 0x03000200000A0000, 0x0000000000000000), make_template_item(false, 0x0301020000050000, 0x0000000000000000), make_template_item(false, 0x0306010000050000, 0x0000000000000000), make_template_item(false, 0x0306000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0, {make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {make_template_item(true, 0x02054600D007B80B, 0xE803E80328000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {make_template_item(true, 0x02058200A00F8813, 0xD007D00728000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {make_template_item(true, 0x0205BE007017581B, 0xB80BB80B28000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> ranger_templates({
      {0, {make_template_item(true, 0x0006000000000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0300000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4, {make_template_item(true, 0x0006000500000000, 0x0000000000000000), make_template_item(true, 0x0101010000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x020D0C00F401C800, 0xF401000028000012), make_template_item(false, 0x0300000000050000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {5, {make_template_item(true, 0x0006000500000000, 0x0000000000000000), make_template_item(true, 0x0101010000000000, 0x0000000000000000), make_template_item(true, 0x0102010000000000, 0x0000000000000000), make_template_item(true, 0x020D0E00F401C800, 0xBC02000028000012), make_template_item(false, 0x0300000000050000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {8, {make_template_item(true, 0x0006000500000000, 0x0000000000000000), make_template_item(true, 0x0101020000000000, 0x0000000000000000), make_template_item(true, 0x0102020000000000, 0x0000000000000000), make_template_item(true, 0x020D1000F4012C01, 0x2003000028000012), make_template_item(false, 0x0300000000050000, 0x0000000000000000), make_template_item(false, 0x0301000000010000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {10, {make_template_item(true, 0x0006000500000000, 0x0000000000000000), make_template_item(true, 0x0101020000000000, 0x0000000000000000), make_template_item(true, 0x0102030000000000, 0x0000000000000000), make_template_item(true, 0x020D120058029001, 0x2003000028000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0300010000020000, 0x0000000000000000), make_template_item(false, 0x0301000000010000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {12, {make_template_item(true, 0x0006010000000000, 0x0000000000000000), make_template_item(true, 0x0101030000000000, 0x0000000000000000), make_template_item(true, 0x0102030000000000, 0x0000000000000000), make_template_item(true, 0x020D140058029001, 0x2003C80028000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0300010000020000, 0x0000000000000000), make_template_item(false, 0x0301000000010000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {14, {make_template_item(true, 0x0006010500000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x020D1700BC02F401, 0x8403C80028000012), make_template_item(false, 0x0300000000070000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x0301000000020000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {15, {make_template_item(true, 0x0006010500000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x020D190020035802, 0x8403C80028000012), make_template_item(false, 0x0300000000070000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x0301000000020000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {18, {make_template_item(true, 0x0006020000000000, 0x0000000000000000), make_template_item(true, 0x0101050000000000, 0x0000000000000000), make_template_item(true, 0x0102050000000000, 0x0000000000000000), make_template_item(true, 0x020D1E002003BC02, 0xB0042C0128000012), make_template_item(false, 0x0300000000070000, 0x0000000000000000), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301000000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {make_template_item(true, 0x0006030000000000, 0x0000000000000000), make_template_item(true, 0x0101070000000000, 0x0000000000000000), make_template_item(true, 0x0102070000000000, 0x0000000000000000), make_template_item(true, 0x020C4600D007E803, 0xB80BE80328000012), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301010000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {make_template_item(true, 0x0006040000000000, 0x0000000000000000), make_template_item(true, 0x01010E0000000000, 0x0000000000000000), make_template_item(true, 0x01020E0000000000, 0x0000000000000000), make_template_item(true, 0x020C8C00B80BC409, 0x7017C40928000012), make_template_item(false, 0x0300020000050000, 0x0000000000000000), make_template_item(false, 0x0301020000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {make_template_item(true, 0x0006050000000000, 0x0000000000000000), make_template_item(true, 0x0101160000000000, 0x0000000000000000), make_template_item(true, 0x0102120000000000, 0x0000000000000000), make_template_item(true, 0x0206B400B80BB80B, 0x2823B80B28000012), make_template_item(false, 0x0300020000080000, 0x0000000000000000), make_template_item(false, 0x0301020000050000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0308000000050000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {0, {make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {make_template_item(true, 0x020C4600D007E803, 0xB80BE80328000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {make_template_item(true, 0x020C8C00B80BC409, 0x7017C40928000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {make_template_item(true, 0x0206B400B80BB80B, 0x2823B80B28000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  static const vector<ChallengeTemplateDefinition> force_templates({
      {0, {make_template_item(true, 0x000A000000000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0300000000040000, 0x0000000000000000), make_template_item(false, 0x0301000000040000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {4, {make_template_item(true, 0x000A000500000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x02190D0020036400, 0x0000900128000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0301000000060000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {6, {make_template_item(true, 0x000B000000000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x02190F002003C800, 0x0000F40128000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0301000000060000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {9, {make_template_item(true, 0x000B000500000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x0219120084032C01, 0x0000580228000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0301000000060000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 2}, {0x03, 2}, {0x0D, 2}, {0x0A, 2}}},
      {11, {make_template_item(true, 0x000B000500000000, 0x0000000000000000), make_template_item(true, 0x0101000000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x02191400E8032C01, 0x0000BC0228000012), make_template_item(false, 0x0300000000060000, 0x0000000000000000), make_template_item(false, 0x0300010000020000, 0x0000000000000000), make_template_item(false, 0x0301000000080000, 0x0000000000000000), make_template_item(false, 0x0301010000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {12, {make_template_item(true, 0x000B000500000000, 0x0000000000000000), make_template_item(true, 0x0101030000000000, 0x0000000000000000), make_template_item(true, 0x0102000000000000, 0x0000000000000000), make_template_item(true, 0x02191600E8039001, 0x6400BC0228000012), make_template_item(false, 0x0300000000070000, 0x0000000000000000), make_template_item(false, 0x0300010000020000, 0x0000000000000000), make_template_item(false, 0x0301000000070000, 0x0000000000000000), make_template_item(false, 0x0301010000030000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 4}, {0x06, 4}, {0x03, 4}, {0x0D, 2}, {0x0A, 2}, {0x0B, 2}, {0x0C, 2}}},
      {15, {make_template_item(true, 0x000B000A00000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x02191B00B004F401, 0xC800200328000012), make_template_item(false, 0x0300000000070000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x0301000000080000, 0x0000000000000000), make_template_item(false, 0x0301010000040000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {16, {make_template_item(true, 0x000B000A00000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x02191D00B0045802, 0xC800840328000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000030000, 0x0000000000000000), make_template_item(false, 0x03010000000A0000, 0x0000000000000000), make_template_item(false, 0x0301010000040000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {19, {make_template_item(true, 0x000A010000000000, 0x0000000000000000), make_template_item(true, 0x0101040000000000, 0x0000000000000000), make_template_item(true, 0x0102040000000000, 0x0000000000000000), make_template_item(true, 0x02192200DC05BC02, 0xC800E80328000012), make_template_item(false, 0x0300000000080000, 0x0000000000000000), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301010000050000, 0x0000000000000000), make_template_item(false, 0x03010000000A0000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 6}, {0x06, 6}, {0x03, 6}, {0x01, 0}, {0x04, 0}, {0x0D, 3}, {0x0A, 3}, {0x0B, 3}, {0x0C, 3}, {0x0F, 2}}},
      {24, {make_template_item(true, 0x000A010A00000000, 0x0000000000000000), make_template_item(true, 0x0101060000000000, 0x0000000000000000), make_template_item(true, 0x0102060000000000, 0x0000000000000000), make_template_item(true, 0x021C4600D007E803, 0xE803B80B28000012), make_template_item(false, 0x0300010000050000, 0x0000000000000000), make_template_item(false, 0x0301010000080000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 7}, {0x06, 7}, {0x03, 7}, {0x01, 4}, {0x04, 4}, {0x0D, 7}, {0x0A, 7}, {0x0B, 7}, {0x0C, 7}, {0x0F, 6}}},
      {50, {make_template_item(true, 0x000A020000000000, 0x0000000000000000), make_template_item(true, 0x01010E0000000000, 0x0000000000000000), make_template_item(true, 0x01020D0000000000, 0x0000000000000000), make_template_item(true, 0x021C8C00B80BD007, 0xD007581B28000012), make_template_item(false, 0x0300020000050000, 0x0000000000000000), make_template_item(false, 0x0301020000080000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 9}, {0x06, 9}, {0x03, 9}, {0x01, 9}, {0x04, 9}, {0x0D, 9}, {0x0A, 9}, {0x0B, 9}, {0x0C, 9}, {0x0F, 9}}},
      {99, {make_template_item(true, 0x000A040000000000, 0x0000000000000000), make_template_item(true, 0x0101160000000000, 0x0000000000000000), make_template_item(true, 0x0102110000000000, 0x0000000000000000), make_template_item(true, 0x021CB400AC0DD007, 0xC409102728000012), make_template_item(false, 0x0300020000050000, 0x0000000000000000), make_template_item(false, 0x03010200000A0000, 0x0000000000000000), make_template_item(false, 0x0306010000030000, 0x0000000000000000), make_template_item(false, 0x0306000000030000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {{0x00, 14}, {0x06, 14}, {0x03, 14}, {0x01, 14}, {0x04, 14}, {0x0D, 14}, {0x0A, 14}, {0x0B, 14}, {0x0C, 14}, {0x0F, 14}}},
      {0, {make_template_item(true, 0x02000500F4010000, 0x0000000028000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {24, {make_template_item(true, 0x021C4600D007E803, 0xE803B80B28000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {50, {make_template_item(true, 0x021C8C00B80BD007, 0xD007581B28000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
      {99, {make_template_item(true, 0x021CB400AC0DD007, 0xC409102728000012), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000), make_template_item(false, 0x0309000000000000, 0x0000000000000000)}, {}},
  });
  // clang-format on

  if ((class_flags & 0xE0) == 0x20) {
    return hunter_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x40) {
    return ranger_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x80) {
    return force_templates.at(index);
  } else {
    throw runtime_error("invalid class flags on original player");
  }
}
