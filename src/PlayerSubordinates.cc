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

uint32_t PlayerVisualConfig::compute_name_color_checksum(uint32_t name_color) {
  uint8_t x = (random_object<uint32_t>() % 0xFF) + 1;
  uint8_t y = (random_object<uint32_t>() % 0xFF) + 1;
  // name_color          = ABCDEFGHabcdefghIJKLMNOPijklmnop
  // name_color_checksum = ---------ijklmabcdeIJKLM-------- ^ (xxxxxxxxyyyyyyyyxxxxxxxxyyyyyyyy)
  uint32_t xbrgx95558 = ((name_color << 15) & 0x007C0000) | ((name_color >> 6) & 0x0003E000) | ((name_color >> 3) & 0x00001F00);
  uint32_t mask = (x << 24) | (y << 16) | (x << 8) | y;
  return xbrgx95558 ^ mask;
}

void PlayerVisualConfig::compute_name_color_checksum() {
  this->name_color_checksum = this->compute_name_color_checksum(this->name_color);
}

void PlayerDispDataDCPCV3::enforce_lobby_join_limits_for_client(shared_ptr<Client> c) {
  struct ClassMaxes {
    uint16_t costume;
    uint16_t skin;
    uint16_t face;
    uint16_t head;
    uint16_t hair;
  };
  static constexpr ClassMaxes v1_v2_class_maxes[14] = {
      {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
      {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
      {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
      {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
      {0x0000, 0x0009, 0x0000, 0x0005, 0x0000},
      {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0009, 0x0004, 0x0005, 0x0000, 0x0007},
      {0x0009, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
  };
  static constexpr ClassMaxes v3_v4_class_maxes[19] = {
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
      {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0019, 0x0000, 0x0005, 0x0000},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0012, 0x0004, 0x0005, 0x0000, 0x000A},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0001},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0001},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
      {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};

  const ClassMaxes* maxes;
  if ((c->version() == GameVersion::PC) || (c->version() == GameVersion::DC)) {
    // V1/V2 have fewer classes, so we'll substitute some here
    switch (this->visual.char_class) {
      case 0: // HUmar
      case 1: // HUnewearl
      case 2: // HUcast
      case 3: // RAmar
      case 4: // RAcast
      case 5: // RAcaseal
      case 6: // FOmarl
      case 7: // FOnewm
      case 8: // FOnewearl
      case 12: // V3 custom 1
      case 13: // V3 custom 2
        break;
      case 9: // HUcaseal
        this->visual.char_class = 5; // HUcaseal -> RAcaseal
        break;
      case 10: // FOmar
        this->visual.char_class = 0; // FOmar -> HUmar
        break;
      case 11: // RAmarl
        this->visual.char_class = 1; // RAmarl -> HUnewearl
        break;
      case 14: // V2 custom 1 / V3 custom 3
      case 15: // V2 custom 2 / V3 custom 4
      case 16: // V2 custom 3 / V3 custom 5
      case 17: // V2 custom 4 / V3 custom 6
      case 18: // V2 custom 5 / V3 custom 7
        this->visual.char_class -= 5;
        break;
      default:
        this->visual.char_class = 0; // Invalid classes -> HUmar
    }

    this->visual.version = min<uint8_t>(this->visual.version, c->config.check_flag(Client::Flag::IS_DC_V1) ? 0 : 2);
    maxes = &v1_v2_class_maxes[this->visual.char_class];

  } else {
    if (this->visual.char_class >= 19) {
      this->visual.char_class = 0; // Invalid classes -> HUmar
    }
    this->visual.version = min<uint8_t>(this->visual.version, 3);
    maxes = &v3_v4_class_maxes[this->visual.char_class];
  }

  // V1/V2 has fewer costumes and android skins, so substitute them here
  this->visual.costume = maxes->costume ? (this->visual.costume % maxes->costume) : 0;
  this->visual.skin = maxes->skin ? (this->visual.skin % maxes->skin) : 0;
  this->visual.face = maxes->face ? (this->visual.face % maxes->face) : 0;
  this->visual.head = maxes->head ? (this->visual.head % maxes->head) : 0;
  this->visual.hair = maxes->hair ? (this->visual.hair % maxes->hair) : 0;

  this->visual.compute_name_color_checksum();
  this->visual.class_flags = class_flags_for_class(this->visual.char_class);

  if (this->visual.name.at(0) == '\t' && (this->visual.name.at(1) == 'J' || this->visual.name.at(1) == 'E')) {
    this->visual.name.encode(this->visual.name.decode().substr(2));
  }
}

void PlayerDispDataBB::enforce_lobby_join_limits_for_client(shared_ptr<Client> c) {
  if (c->version() != GameVersion::BB) {
    throw logic_error("PlayerDispDataBB being sent to non-BB client");
  }
  this->play_time = 0;
  if (this->name.at(0) != '\t' || (this->name.at(1) != 'E' && this->name.at(1) != 'J')) {
    this->name.encode("\tJ" + this->name.decode());
  }
}

PlayerDispDataBB PlayerDispDataDCPCV3::to_bb(uint8_t to_language, uint8_t from_language) const {
  PlayerDispDataBB bb;
  bb.stats = this->stats;
  bb.visual = this->visual;
  bb.visual.name.encode("         0");
  string decoded_name = this->visual.name.decode(from_language);
  bb.name.encode(decoded_name, to_language);
  bb.config = this->config;
  bb.technique_levels_v1 = this->technique_levels_v1;
  return bb;
}

PlayerDispDataDCPCV3 PlayerDispDataBB::to_dcpcv3(uint8_t to_language, uint8_t from_language) const {
  PlayerDispDataDCPCV3 ret;
  ret.stats = this->stats;
  ret.visual = this->visual;
  string decoded_name = this->name.decode(from_language);
  ret.visual.name.encode(decoded_name, to_language);
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
  this->unknown_a1 = 0;
  this->unknown_a2 = 0;
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
  this->team_guild_card_number = 0;
  this->team_id = 0;
  this->unknown_a1.clear(0);
  this->client_id = 0;
  this->name.clear();
  this->hide_help_prompt = 0;
}

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsDC_Challenge& rec)
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

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsPC_Challenge& rec)
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
      rank_title(rec.rank_title),
      unknown_l7(0) {}

PlayerRecordsBB_Challenge::PlayerRecordsBB_Challenge(const PlayerRecordsV3_Challenge<false>& rec)
    : title_color(rec.stats.title_color),
      unknown_u0(rec.stats.unknown_u0),
      times_ep1_online(rec.stats.times_ep1_online),
      times_ep2_online(rec.stats.times_ep2_online),
      times_ep1_offline(rec.stats.times_ep1_offline),
      grave_is_ep2(rec.stats.grave_is_ep2),
      grave_stage_num(rec.stats.grave_stage_num),
      grave_floor(rec.stats.grave_floor),
      unknown_g0(rec.stats.unknown_g0),
      grave_deaths(rec.stats.grave_deaths),
      unknown_u4(rec.stats.unknown_u4),
      grave_time(rec.stats.grave_time),
      grave_defeated_by_enemy_rt_index(rec.stats.grave_defeated_by_enemy_rt_index),
      grave_x(rec.stats.grave_x),
      grave_y(rec.stats.grave_y),
      grave_z(rec.stats.grave_z),
      grave_team(rec.stats.grave_team.decode(), 1),
      grave_message(rec.stats.grave_message.decode(), 1),
      unknown_m5(rec.stats.unknown_m5),
      unknown_t6(rec.stats.unknown_t6),
      ep1_online_award_state(rec.stats.ep1_online_award_state),
      ep2_online_award_state(rec.stats.ep2_online_award_state),
      ep1_offline_award_state(rec.stats.ep1_offline_award_state),
      rank_title(rec.rank_title.decode(), 1),
      unknown_l7(rec.unknown_l7) {}

PlayerRecordsBB_Challenge::operator PlayerRecordsDC_Challenge() const {
  PlayerRecordsDC_Challenge ret;
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

PlayerRecordsBB_Challenge::operator PlayerRecordsPC_Challenge() const {
  PlayerRecordsPC_Challenge ret;
  ret.title_color = this->title_color;
  ret.unknown_u0 = this->unknown_u0;
  ret.rank_title = this->rank_title;
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

PlayerRecordsBB_Challenge::operator PlayerRecordsV3_Challenge<false>() const {
  PlayerRecordsV3_Challenge<false> ret;
  ret.stats.title_color = this->title_color;
  ret.stats.unknown_u0 = this->unknown_u0;
  ret.stats.times_ep1_online = this->times_ep1_online;
  ret.stats.times_ep2_online = this->times_ep2_online;
  ret.stats.times_ep1_offline = this->times_ep1_offline;
  ret.stats.grave_is_ep2 = this->grave_is_ep2;
  ret.stats.grave_stage_num = this->grave_stage_num;
  ret.stats.grave_floor = this->grave_floor;
  ret.stats.unknown_g0 = this->unknown_g0;
  ret.stats.grave_deaths = this->grave_deaths;
  ret.stats.unknown_u4 = this->unknown_u4;
  ret.stats.grave_time = this->grave_time;
  ret.stats.grave_defeated_by_enemy_rt_index = this->grave_defeated_by_enemy_rt_index;
  ret.stats.grave_x = this->grave_x;
  ret.stats.grave_y = this->grave_y;
  ret.stats.grave_z = this->grave_z;
  ret.stats.grave_team.encode(this->grave_team.decode(), 1);
  ret.stats.grave_message.encode(this->grave_message.decode(), 1);
  ret.stats.unknown_m5 = this->unknown_m5;
  ret.stats.unknown_t6 = this->unknown_t6;
  ret.stats.ep1_online_award_state = this->ep1_online_award_state;
  ret.stats.ep2_online_award_state = this->ep2_online_award_state;
  ret.stats.ep1_offline_award_state = this->ep1_offline_award_state;
  ret.rank_title.encode(this->rank_title.decode(), 1);
  ret.unknown_l7 = this->unknown_l7;
  return ret;
}

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

size_t PlayerInventory::find_item_by_primary_identifier(uint32_t primary_identifier) const {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.primary_identifier() == primary_identifier) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

size_t PlayerInventory::find_equipped_item(EquipSlot slot) const {
  ssize_t ret = -1;
  for (size_t y = 0; y < this->num_items; y++) {
    const auto& i = this->items[y];
    if (!(i.flags & 0x00000008)) {
      continue;
    }
    if (!i.data.can_be_equipped_in_slot(slot)) {
      continue;
    }

    // Units can be equipped in multiple slots, so the currently-equipped slot
    // is stored in the item data itself.
    if (((slot == EquipSlot::UNIT_1) && (i.data.data1[4] != 0x00)) ||
        ((slot == EquipSlot::UNIT_2) && (i.data.data1[4] != 0x01)) ||
        ((slot == EquipSlot::UNIT_3) && (i.data.data1[4] != 0x02)) ||
        ((slot == EquipSlot::UNIT_4) && (i.data.data1[4] != 0x03))) {
      continue;
    }

    if (ret < 0) {
      ret = y;
    } else {
      throw runtime_error("multiple items are equipped in the same slot");
    }
  }
  if (ret < 0) {
    throw out_of_range("no item is equipped in this slot");
  }
  return ret;
}

bool PlayerInventory::has_equipped_item(EquipSlot slot) const {
  try {
    this->find_equipped_item(slot);
    return true;
  } catch (const out_of_range&) {
    return false;
  }
}

void PlayerInventory::equip_item_id(uint32_t item_id, EquipSlot slot) {
  this->equip_item_index(this->find_item(item_id), slot);
}

void PlayerInventory::equip_item_index(size_t index, EquipSlot slot) {
  auto& item = this->items[index];

  if (slot == EquipSlot::UNKNOWN) {
    slot = item.data.default_equip_slot();
  }

  if (!item.data.can_be_equipped_in_slot(slot)) {
    throw runtime_error("incorrect item type for equip slot");
  }
  if (this->has_equipped_item(slot)) {
    throw runtime_error("equip slot is already in use");
  }

  item.flags |= 0x00000008;
  // Units store which slot they're equipped in within the item data itself
  if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x03)) {
    item.data.data1[4] = static_cast<uint8_t>(slot) - 9;
  }
}

void PlayerInventory::unequip_item_id(uint32_t item_id) {
  this->unequip_item_index(this->find_item(item_id));
}

void PlayerInventory::unequip_item_slot(EquipSlot slot) {
  this->unequip_item_index(this->find_equipped_item(slot));
}

void PlayerInventory::unequip_item_index(size_t index) {
  auto& item = this->items[index];

  item.flags &= (~0x00000008);
  // Units store which slot they're equipped in within the item data itself
  if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x03)) {
    item.data.data1[4] = 0x00;
  }
  // If the item is an armor, remove all units too
  if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x01)) {
    for (size_t z = 0; z < 30; z++) {
      auto& unit = this->items[z];
      if ((unit.data.data1[0] == 0x01) && (unit.data.data1[1] == 0x03)) {
        unit.flags &= (~0x00000008);
        unit.data.data1[4] = 0x00;
      }
    }
  }
}

size_t PlayerInventory::remove_all_items_of_type(uint8_t data1_0, int16_t data1_1) {
  size_t write_offset = 0;
  for (size_t read_offset = 0; read_offset < this->num_items; read_offset++) {
    bool should_delete = ((this->items[read_offset].data.data1[0] == data1_0) &&
        ((data1_1 < 0) || (this->items[read_offset].data.data1[1] == static_cast<uint8_t>(data1_1))));
    if (!should_delete) {
      if (read_offset != write_offset) {
        this->items[write_offset].present = this->items[read_offset].present;
        this->items[write_offset].unknown_a1 = this->items[read_offset].unknown_a1;
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

void PlayerInventory::decode_from_client(shared_ptr<Client> c) {
  for (size_t z = 0; z < this->items.size(); z++) {
    this->items[z].data.decode_for_version(c->version());
  }
}

void PlayerInventory::encode_for_client(shared_ptr<Client> c) {
  if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    // DC NTE has the item count as a 32-bit value here, whereas every other
    // version uses a single byte. To stop DC NTE from crashing by trying to
    // construct far more than 30 TItem objects, we clear the fields DC NTE
    // doesn't know about.
    this->hp_from_materials = 0;
    this->tp_from_materials = 0;
    this->language = 0;
  }

  auto item_parameter_table = c->require_server_state()->item_parameter_table_for_version(c->version());
  for (size_t z = 0; z < this->items.size(); z++) {
    this->items[z].data.encode_for_version(c->version(), item_parameter_table);
  }
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
  static const JSON empty_list = JSON::list();

  this->tech_disk_mode = json.get_enum("tech_disk_mode", this->tech_disk_mode);
  this->weapon_and_armor_mode = json.get_enum("weapon_and_armor_mode", this->weapon_and_armor_mode);
  this->mag_mode = json.get_enum("mag_mode", this->mag_mode);
  this->tool_mode = json.get_enum("tool_mode", this->tool_mode);
  this->trap_mode = json.get_enum("trap_mode", this->trap_mode);
  this->unused_F817 = json.get_int("unused_F817", this->unused_F817);
  this->respawn_mode = json.get_int("respawn_mode", this->respawn_mode);
  this->replace_char = json.get_int("replace_char", this->replace_char);
  this->drop_weapon = json.get_int("drop_weapon", this->drop_weapon);
  this->is_teams = json.get_int("is_teams", this->is_teams);
  this->hide_target_reticle = json.get_int("hide_target_reticle", this->hide_target_reticle);
  this->meseta_mode = json.get_enum("meseta_mode", this->meseta_mode);
  this->death_level_up = json.get_int("death_level_up", this->death_level_up);
  const JSON& trap_counts_json = json.get("trap_counts", empty_list);
  for (size_t z = 0; z < trap_counts_json.size(); z++) {
    this->trap_counts[z] = trap_counts_json.at(z).as_int();
  }
  this->enable_sonar = json.get_int("enable_sonar", this->enable_sonar);
  this->sonar_count = json.get_int("sonar_count", this->sonar_count);
  this->forbid_scape_dolls = json.get_int("forbid_scape_dolls", this->forbid_scape_dolls);
  this->lives = json.get_int("lives", this->lives);
  this->max_tech_level = json.get_int("max_tech_level", this->max_tech_level);
  this->char_level = json.get_int("char_level", this->char_level);
  this->time_limit = json.get_int("time_limit", this->time_limit);
  this->death_tech_level_up = json.get_int("death_tech_level_up", this->death_tech_level_up);
  this->box_drop_area = json.get_int("box_drop_area", this->box_drop_area);
}

JSON BattleRules::json() const {
  return JSON::dict({
      {"tech_disk_mode", this->tech_disk_mode},
      {"weapon_and_armor_mode", this->weapon_and_armor_mode},
      {"mag_mode", this->mag_mode},
      {"tool_mode", this->tool_mode},
      {"trap_mode", this->trap_mode},
      {"unused_F817", this->unused_F817},
      {"respawn_mode", this->respawn_mode},
      {"replace_char", this->replace_char},
      {"drop_weapon", this->drop_weapon},
      {"is_teams", this->is_teams},
      {"hide_target_reticle", this->hide_target_reticle},
      {"meseta_mode", this->meseta_mode},
      {"death_level_up", this->death_level_up},
      {"trap_counts", JSON::list({this->trap_counts[0], this->trap_counts[1], this->trap_counts[2], this->trap_counts[3]})},
      {"enable_sonar", this->enable_sonar},
      {"sonar_count", this->sonar_count},
      {"forbid_scape_dolls", this->forbid_scape_dolls},
      {"lives", this->lives.load()},
      {"max_tech_level", this->max_tech_level.load()},
      {"char_level", this->char_level.load()},
      {"time_limit", this->time_limit.load()},
      {"death_tech_level_up", this->death_tech_level_up.load()},
      {"box_drop_area", this->box_drop_area.load()},
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
const char* name_for_enum<BattleRules::MagMode>(BattleRules::MagMode v) {
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
BattleRules::MagMode enum_for_name<BattleRules::MagMode>(const char* name) {
  if (!strcmp(name, "ALLOW")) {
    return BattleRules::MagMode::ALLOW;
  } else if (!strcmp(name, "FORBID_ALL")) {
    return BattleRules::MagMode::FORBID_ALL;
  } else {
    throw invalid_argument("invalid BattleRules::MagMode name");
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
const char* name_for_enum<BattleRules::TrapMode>(BattleRules::TrapMode v) {
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
BattleRules::TrapMode enum_for_name<BattleRules::TrapMode>(const char* name) {
  if (!strcmp(name, "DEFAULT")) {
    return BattleRules::TrapMode::DEFAULT;
  } else if (!strcmp(name, "ALL_PLAYERS")) {
    return BattleRules::TrapMode::ALL_PLAYERS;
  } else {
    throw invalid_argument("invalid BattleRules::TrapMode name");
  }
}

template <>
const char* name_for_enum<BattleRules::MesetaMode>(BattleRules::MesetaMode v) {
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
BattleRules::MesetaMode enum_for_name<BattleRules::MesetaMode>(const char* name) {
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

static PlayerInventoryItem make_template_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  PlayerInventoryItem ret = {
      .present = 1,
      .unknown_a1 = 0,
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
}

static PlayerInventoryItem v2_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  auto ret = make_template_item(equipped, first_data, second_data);
  ret.data.decode_for_version(GameVersion::PC);
  return ret;
}

static PlayerInventoryItem v3_item(bool equipped, uint64_t first_data, uint64_t second_data) {
  return make_template_item(equipped, first_data, second_data);
}

const ChallengeTemplateDefinition& get_challenge_template_definition(GameVersion version, uint32_t class_flags, size_t index) {
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

  bool is_v2 = (version == GameVersion::DC) || (version == GameVersion::PC);

  if ((class_flags & 0xE0) == 0x20) {
    return is_v2 ? v2_hunter_templates.at(index) : v3_hunter_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x40) {
    return is_v2 ? v2_ranger_templates.at(index) : v3_ranger_templates.at(index);
  } else if ((class_flags & 0xE0) == 0x80) {
    return is_v2 ? v2_force_templates.at(index) : v3_force_templates.at(index);
  } else {
    throw runtime_error("invalid class flags on original player");
  }
}

SymbolChat::SymbolChat()
    : spec(0),
      corner_objects(0x00FF),
      face_parts() {}
