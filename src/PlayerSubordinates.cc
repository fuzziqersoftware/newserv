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

FileContentsCache player_files_cache(300 * 1000 * 1000);

void PlayerDispDataDCPCV3::enforce_lobby_join_limits(GameVersion target_version) {
  if ((target_version == GameVersion::PC) || (target_version == GameVersion::DC)) {
    // V1/V2 have fewer classes, so we'll substitute some here
    if (this->visual.char_class == 11) {
      this->visual.char_class = 0; // FOmar -> HUmar
    } else if (this->visual.char_class == 10) {
      this->visual.char_class = 1; // RAmarl -> HUnewearl
    } else if (this->visual.char_class == 9) {
      this->visual.char_class = 5; // HUcaseal -> RAcaseal
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
  this->visual.v1_flags = pre.visual.v1_flags;
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

size_t PlayerBank::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

void SavedPlayerDataBB::print_inventory(FILE* stream) const {
  fprintf(stream, "[PlayerInventory] Meseta: %" PRIu32 "\n", this->disp.stats.meseta.load());
  fprintf(stream, "[PlayerInventory] %hhu items\n", this->inventory.num_items);
  for (size_t x = 0; x < this->inventory.num_items; x++) {
    const auto& item = this->inventory.items[x];
    auto name = item.data.name(false);
    auto hex = item.data.hex();
    fprintf(stream, "[PlayerInventory]   %zu: %s (%s)\n", x, hex.c_str(), name.c_str());
  }
}
