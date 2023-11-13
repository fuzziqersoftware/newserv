#include "Player.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <stdexcept>

#include "FileContentsCache.hh"
#include "ItemData.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

using namespace std;

ClientGameData::ClientGameData()
    : guild_card_number(0),
      should_update_play_time(false),
      bb_character_index(-1),
      last_play_time_update(0) {
  for (size_t z = 0; z < this->blocked_senders.size(); z++) {
    this->blocked_senders[z] = 0;
  }
}

ClientGameData::~ClientGameData() {
  if (!this->bb_username.empty() && this->character_data.get()) {
    this->save_character_file();
  }
}

void ClientGameData::create_battle_overlay(shared_ptr<const BattleRules> rules, shared_ptr<const LevelTable> level_table) {
  this->overlay_character_data.reset(new PSOBBCharacterFile(*this->character(true, false)));

  if (rules->weapon_and_armor_mode != BattleRules::WeaponAndArmorMode::ALLOW) {
    this->overlay_character_data->inventory.remove_all_items_of_type(0);
    this->overlay_character_data->inventory.remove_all_items_of_type(1);
  }
  if (rules->mag_mode == BattleRules::MagMode::FORBID_ALL) {
    this->overlay_character_data->inventory.remove_all_items_of_type(2);
  }
  if (rules->tool_mode != BattleRules::ToolMode::ALLOW) {
    this->overlay_character_data->inventory.remove_all_items_of_type(3);
  }
  if (rules->replace_char) {
    // TODO: Shouldn't we clear other material usage here? It looks like the
    // original code doesn't, but that seems wrong.
    this->overlay_character_data->inventory.hp_from_materials = 0;
    this->overlay_character_data->inventory.tp_from_materials = 0;

    uint32_t target_level = clamp<uint32_t>(rules->char_level, 0, 199);
    uint8_t char_class = this->overlay_character_data->disp.visual.char_class;
    auto& stats = this->overlay_character_data->disp.stats;

    stats.reset_to_base(char_class, level_table);
    stats.advance_to_level(char_class, target_level, level_table);

    stats.unknown_a1 = 40;
    stats.meseta = 300;
  }
  if (rules->tech_disk_mode == BattleRules::TechDiskMode::LIMIT_LEVEL) {
    // TODO: Verify this is what the game actually does.
    for (uint8_t tech_num = 0; tech_num < 0x13; tech_num++) {
      uint8_t existing_level = this->overlay_character_data->get_technique_level(tech_num);
      if ((existing_level != 0xFF) && (existing_level > rules->max_tech_level)) {
        this->overlay_character_data->set_technique_level(tech_num, rules->max_tech_level);
      }
    }
  } else if (rules->tech_disk_mode == BattleRules::TechDiskMode::FORBID_ALL) {
    for (uint8_t tech_num = 0; tech_num < 0x13; tech_num++) {
      this->overlay_character_data->set_technique_level(tech_num, 0xFF);
    }
  }
  if (rules->meseta_mode != BattleRules::MesetaMode::ALLOW) {
    this->overlay_character_data->disp.stats.meseta = 0;
  }
  if (rules->forbid_scape_dolls) {
    this->overlay_character_data->inventory.remove_all_items_of_type(3, 9);
  }
}

void ClientGameData::create_challenge_overlay(GameVersion version, size_t template_index, shared_ptr<const LevelTable> level_table) {
  auto p = this->character(true, false);
  const auto& tpl = get_challenge_template_definition(version, p->disp.visual.class_flags, template_index);

  this->overlay_character_data.reset(new PSOBBCharacterFile(*p));
  auto overlay = this->overlay_character_data;

  for (size_t z = 0; z < overlay->inventory.items.size(); z++) {
    auto& i = overlay->inventory.items[z];
    i.present = 0;
    i.unknown_a1 = 0;
    i.extension_data1 = 0;
    i.extension_data2 = 0;
    i.flags = 0;
    i.data = ItemData();
  }

  overlay->inventory.items[13].extension_data2 = 1;

  overlay->disp.stats.reset_to_base(overlay->disp.visual.char_class, level_table);
  overlay->disp.stats.advance_to_level(overlay->disp.visual.char_class, tpl.level, level_table);

  overlay->disp.stats.unknown_a1 = 40;
  overlay->disp.stats.unknown_a3 = 10.0;
  overlay->disp.stats.experience = level_table->stats_delta_for_level(overlay->disp.visual.char_class, overlay->disp.stats.level).experience;
  overlay->disp.stats.meseta = 0;
  overlay->clear_all_material_usage();
  for (size_t z = 0; z < 0x13; z++) {
    overlay->set_technique_level(z, 0xFF);
  }

  for (size_t z = 0; z < tpl.items.size(); z++) {
    auto& inv_item = overlay->inventory.items[z];
    inv_item.present = tpl.items[z].present;
    inv_item.unknown_a1 = tpl.items[z].unknown_a1;
    inv_item.flags = tpl.items[z].flags;
    inv_item.data = tpl.items[z].data;
  }
  overlay->inventory.num_items = tpl.items.size();

  for (const auto& tech_level : tpl.tech_levels) {
    overlay->set_technique_level(tech_level.tech_num, tech_level.level);
  }
}

shared_ptr<PSOBBSystemFile> ClientGameData::system(bool allow_load) {
  if (!this->system_data && allow_load) {
    this->load_all_files();
  }
  return this->system_data;
}

shared_ptr<const PSOBBSystemFile> ClientGameData::system(bool allow_load) const {
  if (!this->system_data.get() && allow_load) {
    throw runtime_error("system data is not loaded");
  }
  return this->system_data;
}

shared_ptr<PSOBBCharacterFile> ClientGameData::character(bool allow_load, bool allow_overlay) {
  if (this->overlay_character_data && allow_overlay) {
    return this->overlay_character_data;
  }
  if (!this->character_data && allow_load) {
    if (this->bb_character_index < 0) {
      throw runtime_error("character index not specified");
    }
    this->load_all_files();
  }
  return this->character_data;
}

shared_ptr<const PSOBBCharacterFile> ClientGameData::character(bool allow_load, bool allow_overlay) const {
  if (allow_overlay && this->overlay_character_data) {
    return this->overlay_character_data;
  }
  if (!this->character_data && allow_load) {
    throw runtime_error("character data is not loaded");
  }
  return this->character_data;
}

shared_ptr<PSOBBGuildCardFile> ClientGameData::guild_cards(bool allow_load) {
  if (!this->guild_card_data && allow_load) {
    this->load_all_files();
  }
  return this->guild_card_data;
}

shared_ptr<const PSOBBGuildCardFile> ClientGameData::guild_cards(bool allow_load) const {
  if (!this->guild_card_data && allow_load) {
    throw runtime_error("account data is not loaded");
  }
  return this->guild_card_data;
}

string ClientGameData::system_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have system data");
  }
  return string_printf("system/players/system_%s.psosys", this->bb_username.c_str());
}

string ClientGameData::character_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have character data");
  }
  if (this->bb_character_index < 0) {
    throw logic_error("character index is not set");
  }
  return string_printf("system/players/player_%s_%hhd.psochar", this->bb_username.c_str(), this->bb_character_index);
}

string ClientGameData::guild_card_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have Guild Card files");
  }
  return string_printf("system/players/guild_cards_%s.psocard", this->bb_username.c_str());
}

string ClientGameData::legacy_account_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have legacy account data");
  }
  return string_printf("system/players/account_%s.nsa", this->bb_username.c_str());
}

string ClientGameData::legacy_player_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have legacy player data");
  }
  if (this->bb_character_index < 0) {
    throw logic_error("character index is not set");
  }
  return string_printf(
      "system/players/player_%s_%hhd.nsc",
      this->bb_username.c_str(),
      static_cast<int8_t>(this->bb_character_index + 1));
}

void ClientGameData::create_character_file(
    uint32_t guild_card_number,
    uint8_t language,
    const PlayerDispDataBBPreview& preview,
    shared_ptr<const LevelTable> level_table) {
  this->character_data = PSOBBCharacterFile::create_from_preview(guild_card_number, language, preview, level_table);
  this->save_character_file();
}

void ClientGameData::load_all_files() {
  if (this->bb_username.empty()) {
    this->system_data.reset(new PSOBBSystemFile());
    this->character_data.reset(new PSOBBCharacterFile());
    this->guild_card_data.reset(new PSOBBGuildCardFile());
    return;
  }

  this->system_data.reset();
  this->character_data.reset();
  this->guild_card_data.reset();

  string sys_filename = this->system_filename();
  if (isfile(sys_filename)) {
    this->system_data.reset(new PSOBBSystemFile(load_object_file<PSOBBSystemFile>(sys_filename)));
    player_data_log.info("Loaded system data from %s", sys_filename.c_str());
  }

  if (this->bb_character_index >= 0) {
    string char_filename = this->character_filename();
    if (isfile(char_filename)) {
      auto f = fopen_unique(char_filename, "rb");
      auto header = freadx<PSOCommandHeaderBB>(f.get());
      if (header.size != 0x399C) {
        throw runtime_error("incorrect size in character file header");
      }
      if (header.command != 0x00E7) {
        throw runtime_error("incorrect command in character file header");
      }
      if (header.flag != 0x00000000) {
        throw runtime_error("incorrect flag in character file header");
      }
      this->character_data.reset(new PSOBBCharacterFile(freadx<PSOBBCharacterFile>(f.get())));
      player_data_log.info("Loaded character data from %s", char_filename.c_str());

      // If there was no .psosys file, load the system file from the .psochar
      // file instead
      if (!this->system_data) {
        this->system_data.reset(new PSOBBSystemFile(freadx<PSOBBSystemFile>(f.get())));
        player_data_log.info("Loaded system data from %s", char_filename.c_str());
      }
    }
  }

  string card_filename = this->guild_card_filename();
  if (isfile(card_filename)) {
    this->guild_card_data.reset(new PSOBBGuildCardFile(load_object_file<PSOBBGuildCardFile>(card_filename)));
    player_data_log.info("Loaded Guild Card data from %s", card_filename.c_str());
  }

  // If any of the above files were missing, try to load from .nsa/.nsc files instead
  if (!this->system_data || (!this->character_data && (this->bb_character_index >= 0)) || !this->guild_card_data) {
    string nsa_filename = this->legacy_account_filename();
    shared_ptr<LegacySavedAccountDataBB> nsa_data;
    if (isfile(nsa_filename)) {
      nsa_data.reset(new LegacySavedAccountDataBB(load_object_file<LegacySavedAccountDataBB>(nsa_filename)));
      if (!nsa_data->signature.eq(LegacySavedAccountDataBB::SIGNATURE)) {
        throw runtime_error("account data header is incorrect");
      }
      if (!this->system_data) {
        this->system_data.reset(new PSOBBSystemFile(nsa_data->system_file));
        player_data_log.info("Loaded legacy system data from %s", nsa_filename.c_str());
      }
      if (!this->guild_card_data) {
        this->guild_card_data.reset(new PSOBBGuildCardFile(nsa_data->guild_card_file));
        player_data_log.info("Loaded legacy Guild Card data from %s", nsa_filename.c_str());
      }
    }

    if (!this->system_data) {
      this->system_data.reset(new PSOBBSystemFile());
      player_data_log.info("Created new system data");
    }
    if (!this->guild_card_data) {
      this->guild_card_data.reset(new PSOBBGuildCardFile());
      player_data_log.info("Created new Guild Card data");
    }

    if (!this->character_data && (this->bb_character_index >= 0)) {
      string nsc_filename = this->legacy_player_filename();
      auto nsc_data = load_object_file<LegacySavedPlayerDataBB>(nsc_filename);
      if (nsc_data.signature == LegacySavedPlayerDataBB::SIGNATURE_V0) {
        nsc_data.signature = LegacySavedPlayerDataBB::SIGNATURE_V0;
        nsc_data.unused.clear();
        nsc_data.battle_records.place_counts.clear(0);
        nsc_data.battle_records.disconnect_count = 0;
        nsc_data.battle_records.unknown_a1.clear(0);
      } else if (nsc_data.signature != LegacySavedPlayerDataBB::SIGNATURE_V1) {
        throw runtime_error("legacy player data has incorrect signature");
      }

      this->character_data.reset(new PSOBBCharacterFile());
      this->character_data->inventory = nsc_data.inventory;
      this->character_data->disp = nsc_data.disp;
      this->character_data->play_time_seconds = nsc_data.disp.play_time;
      this->character_data->unknown_a2 = nsc_data.unknown_a2;
      this->character_data->quest_flags = nsc_data.quest_flags;
      this->character_data->death_count = nsc_data.death_count;
      this->character_data->bank = nsc_data.bank;
      this->character_data->guild_card.guild_card_number = this->guild_card_number;
      this->character_data->guild_card.name = nsc_data.disp.name;
      this->character_data->guild_card.team_name = this->system_data->team_name;
      this->character_data->guild_card.description = nsc_data.guild_card_description;
      this->character_data->guild_card.present = 1;
      this->character_data->guild_card.language = nsc_data.inventory.language;
      this->character_data->guild_card.section_id = nsc_data.disp.visual.section_id;
      this->character_data->guild_card.char_class = nsc_data.disp.visual.char_class;
      this->character_data->auto_reply = nsc_data.auto_reply;
      this->character_data->info_board = nsc_data.info_board;
      this->character_data->battle_records = nsc_data.battle_records;
      this->character_data->challenge_records = nsc_data.challenge_records;
      this->character_data->tech_menu_config = nsc_data.tech_menu_config;
      this->character_data->quest_global_flags = nsc_data.quest_global_flags;
      if (nsa_data) {
        this->character_data->option_flags = nsa_data->option_flags;
        this->character_data->symbol_chats = nsa_data->symbol_chats;
        this->character_data->shortcuts = nsa_data->shortcuts;
        player_data_log.info("Loaded legacy player data from %s and %s", nsa_filename.c_str(), nsc_filename.c_str());
      } else {
        player_data_log.info("Loaded legacy player data from %s", nsc_filename.c_str());
      }
    }
  }

  this->blocked_senders.fill(0);
  for (size_t z = 0; z < this->guild_card_data->blocked.size(); z++) {
    if (this->guild_card_data->blocked[z].present) {
      this->blocked_senders[z] = this->guild_card_data->blocked[z].guild_card_number;
    }
  }

  if (this->character_data) {
    this->last_play_time_update = now();
  }
}

void ClientGameData::save_system_file() const {
  if (!this->system_data) {
    throw logic_error("no system file loaded");
  }
  string filename = this->system_filename();
  save_object_file(filename, *this->system_data);
  player_data_log.info("Saved system file %s", filename.c_str());
}

void ClientGameData::save_character_file() {
  if (!this->system_data.get()) {
    throw logic_error("no system file loaded");
  }
  if (!this->character_data.get()) {
    throw logic_error("no character file loaded");
  }
  if (this->should_update_play_time) {
    // This is slightly inaccurate, since fractions of a second are truncated
    // off each time we save. I'm lazy, so insert shrug emoji here.
    uint64_t t = now();
    uint64_t seconds = (t - this->last_play_time_update) / 1000000;
    this->character_data->disp.play_time += seconds;
    this->character_data->play_time_seconds = this->character_data->disp.play_time;
    player_data_log.info("Added %" PRIu64 " seconds to play time", seconds);
    this->last_play_time_update = t;
  }

  string filename = this->character_filename();
  auto f = fopen_unique(filename, "wb");
  PSOCommandHeaderBB header = {sizeof(PSOCommandHeaderBB) + sizeof(PSOBBCharacterFile) + sizeof(PSOBBSystemFile), 0x00E7, 0x00000000};
  fwritex(f.get(), header);
  fwritex(f.get(), *this->character_data);
  fwritex(f.get(), *this->system_data);
  player_data_log.info("Saved character file %s", filename.c_str());
}

void ClientGameData::save_guild_card_file() const {
  if (!this->guild_card_data.get()) {
    throw logic_error("no Guild Card file loaded");
  }
  string filename = this->guild_card_filename();
  save_object_file(filename, *this->guild_card_data);
  player_data_log.info("Saved Guild Card file %s", filename.c_str());
}
