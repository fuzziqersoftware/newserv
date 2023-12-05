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

PlayerFilesManager::PlayerFilesManager(std::shared_ptr<struct event_base> base)
    : base(base),
      clear_expired_files_event(
          event_new(this->base.get(), -1, EV_TIMEOUT | EV_PERSIST, &PlayerFilesManager::clear_expired_files, this),
          event_free) {
  auto tv = usecs_to_timeval(30 * 1000 * 1000);
  event_add(this->clear_expired_files_event.get(), &tv);
}

template <typename KeyT, typename ValueT>
size_t erase_unused(std::unordered_map<KeyT, std::shared_ptr<ValueT>>& m) {
  size_t ret = 0;
  for (auto it = m.begin(); it != m.end();) {
    if (it->second.use_count() <= 1) {
      it = m.erase(it);
      ret++;
    } else {
      it++;
    }
  }
  return ret;
}

std::shared_ptr<PSOBBBaseSystemFile> PlayerFilesManager::get_system(const std::string& filename) {
  try {
    return this->loaded_system_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PSOBBCharacterFile> PlayerFilesManager::get_character(const std::string& filename) {
  try {
    return this->loaded_character_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PSOBBGuildCardFile> PlayerFilesManager::get_guild_card(const std::string& filename) {
  try {
    return this->loaded_guild_card_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

std::shared_ptr<PlayerBank> PlayerFilesManager::get_bank(const std::string& filename) {
  try {
    return this->loaded_bank_files.at(filename);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

void PlayerFilesManager::set_system(const std::string& filename, std::shared_ptr<PSOBBBaseSystemFile> file) {
  if (!this->loaded_system_files.emplace(filename, file).second) {
    throw runtime_error("Guild Card file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_character(const std::string& filename, std::shared_ptr<PSOBBCharacterFile> file) {
  if (!this->loaded_character_files.emplace(filename, file).second) {
    throw runtime_error("character file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_guild_card(const std::string& filename, std::shared_ptr<PSOBBGuildCardFile> file) {
  if (!this->loaded_guild_card_files.emplace(filename, file).second) {
    throw runtime_error("Guild Card file already loaded: " + filename);
  }
}

void PlayerFilesManager::set_bank(const std::string& filename, std::shared_ptr<PlayerBank> file) {
  if (!this->loaded_bank_files.emplace(filename, file).second) {
    throw runtime_error("bank file already loaded: " + filename);
  }
}

void PlayerFilesManager::clear_expired_files(evutil_socket_t, short, void* ctx) {
  auto* self = reinterpret_cast<PlayerFilesManager*>(ctx);
  size_t num_deleted = erase_unused(self->loaded_system_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired system file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_character_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired character file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_guild_card_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired Guild Card file(s)", num_deleted);
  }
  num_deleted = erase_unused(self->loaded_bank_files);
  if (num_deleted) {
    player_data_log.info("Cleared %zu expired bank file(s)", num_deleted);
  }
}

ClientGameData::ClientGameData(std::shared_ptr<PlayerFilesManager> files_manager)
    : guild_card_number(0),
      should_update_play_time(false),
      bb_character_index(-1),
      files_manager(files_manager),
      last_play_time_update(0) {
  for (size_t z = 0; z < this->blocked_senders.size(); z++) {
    this->blocked_senders[z] = 0;
  }
}

ClientGameData::~ClientGameData() {
  if (!this->bb_username.empty() && this->character_data.get()) {
    this->save_all();
  }
}

const string& ClientGameData::get_bb_username() const {
  return this->bb_username;
}

void ClientGameData::set_bb_username(const string& bb_username) {
  // Make sure bb_username is filename-safe
  for (char ch : bb_username) {
    if (!isalnum(ch) && (ch != '-') && (ch != '_')) {
      throw runtime_error("invalid characters in username");
    }
  }
  this->bb_username = bb_username;
}

void ClientGameData::create_battle_overlay(shared_ptr<const BattleRules> rules, shared_ptr<const LevelTable> level_table) {
  this->overlay_character_data = make_shared<PSOBBCharacterFile>(*this->character(true, false));

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

void ClientGameData::create_challenge_overlay(Version version, size_t template_index, shared_ptr<const LevelTable> level_table) {
  auto p = this->character(true, false);
  const auto& tpl = get_challenge_template_definition(version, p->disp.visual.class_flags, template_index);

  this->overlay_character_data = make_shared<PSOBBCharacterFile>(*p);
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

shared_ptr<PSOBBBaseSystemFile> ClientGameData::system(bool allow_load) {
  if (!this->system_data && allow_load) {
    this->load_all_files();
  }
  return this->system_data;
}

shared_ptr<const PSOBBBaseSystemFile> ClientGameData::system(bool allow_load) const {
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
    if (!this->bb_username.empty() && (this->bb_character_index < 0)) {
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

string ClientGameData::character_filename(int8_t index) const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have character data");
  }
  if (index < 0) {
    index = this->bb_character_index;
  }
  if (index < 0) {
    throw logic_error("character index is not set");
  }
  return string_printf("system/players/player_%s_%hhd.psochar", this->bb_username.c_str(), index);
}

string ClientGameData::guild_card_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have Guild Card files");
  }
  return string_printf("system/players/guild_cards_%s.psocard", this->bb_username.c_str());
}

string ClientGameData::shared_bank_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have shared bank files");
  }
  return string_printf("system/players/shared_bank_%s.psobank", this->bb_username.c_str());
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
    this->system_data = make_shared<PSOBBBaseSystemFile>();
    this->character_data = make_shared<PSOBBCharacterFile>();
    this->guild_card_data = make_shared<PSOBBGuildCardFile>();
    return;
  }

  this->system_data.reset();
  this->character_data.reset();
  this->guild_card_data.reset();

  string sys_filename = this->system_filename();
  this->system_data = this->files_manager->get_system(sys_filename);
  if (this->system_data) {
    player_data_log.info("Using loaded system file %s", sys_filename.c_str());
  } else if (isfile(sys_filename)) {
    this->system_data = make_shared<PSOBBBaseSystemFile>(load_object_file<PSOBBBaseSystemFile>(sys_filename, true));
    this->files_manager->set_system(sys_filename, this->system_data);
    player_data_log.info("Loaded system data from %s", sys_filename.c_str());
  } else {
    player_data_log.info("System file is missing: %s", sys_filename.c_str());
  }

  if (this->bb_character_index >= 0) {
    string char_filename = this->character_filename();
    this->character_data = this->files_manager->get_character(char_filename);
    if (this->character_data) {
      player_data_log.info("Using loaded character file %s", char_filename.c_str());
    } else if (isfile(char_filename)) {
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
      this->character_data = make_shared<PSOBBCharacterFile>(freadx<PSOBBCharacterFile>(f.get()));
      this->files_manager->set_character(char_filename, this->character_data);
      player_data_log.info("Loaded character data from %s", char_filename.c_str());

      // If there was no .psosys file, load the system file from the .psochar
      // file instead
      if (!this->system_data) {
        this->system_data = make_shared<PSOBBBaseSystemFile>(freadx<PSOBBBaseSystemFile>(f.get()));
        this->files_manager->set_system(sys_filename, this->system_data);
        player_data_log.info("Loaded system data from %s", char_filename.c_str());
      }
    } else {
      player_data_log.info("Character file is missing: %s", char_filename.c_str());
    }
  }

  string card_filename = this->guild_card_filename();
  this->guild_card_data = this->files_manager->get_guild_card(card_filename);
  if (this->guild_card_data) {
    player_data_log.info("Using loaded Guild Card file %s", card_filename.c_str());
  } else if (isfile(card_filename)) {
    this->guild_card_data = make_shared<PSOBBGuildCardFile>(load_object_file<PSOBBGuildCardFile>(card_filename));
    this->files_manager->set_guild_card(card_filename, this->guild_card_data);
    player_data_log.info("Loaded Guild Card data from %s", card_filename.c_str());
  } else {
    player_data_log.info("Guild Card file is missing: %s", card_filename.c_str());
  }

  // If any of the above files were missing, try to load from .nsa/.nsc files instead
  if (!this->system_data || (!this->character_data && (this->bb_character_index >= 0)) || !this->guild_card_data) {
    string nsa_filename = this->legacy_account_filename();
    shared_ptr<LegacySavedAccountDataBB> nsa_data;
    if (isfile(nsa_filename)) {
      nsa_data = make_shared<LegacySavedAccountDataBB>(load_object_file<LegacySavedAccountDataBB>(nsa_filename));
      if (!nsa_data->signature.eq(LegacySavedAccountDataBB::SIGNATURE)) {
        throw runtime_error("account data header is incorrect");
      }
      if (!this->system_data) {
        this->system_data = make_shared<PSOBBBaseSystemFile>(nsa_data->system_file.base);
        this->files_manager->set_system(sys_filename, this->system_data);
        player_data_log.info("Loaded legacy system data from %s", nsa_filename.c_str());
      }
      if (!this->guild_card_data) {
        this->guild_card_data = make_shared<PSOBBGuildCardFile>(nsa_data->guild_card_file);
        this->files_manager->set_guild_card(card_filename, this->guild_card_data);
        player_data_log.info("Loaded legacy Guild Card data from %s", nsa_filename.c_str());
      }
    }

    if (!this->system_data) {
      this->system_data = make_shared<PSOBBBaseSystemFile>();
      this->files_manager->set_system(sys_filename, this->system_data);
      player_data_log.info("Created new system data");
    }
    if (!this->guild_card_data) {
      this->guild_card_data = make_shared<PSOBBGuildCardFile>();
      this->files_manager->set_guild_card(card_filename, this->guild_card_data);
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

      this->character_data = make_shared<PSOBBCharacterFile>();
      this->files_manager->set_character(this->character_filename(), this->character_data);
      this->character_data->inventory = nsc_data.inventory;
      this->character_data->disp = nsc_data.disp;
      this->character_data->play_time_seconds = nsc_data.disp.play_time;
      this->character_data->unknown_a2 = nsc_data.unknown_a2;
      this->character_data->quest_flags = nsc_data.quest_flags;
      this->character_data->death_count = nsc_data.death_count;
      this->character_data->bank = nsc_data.bank;
      this->character_data->guild_card.guild_card_number = this->guild_card_number;
      this->character_data->guild_card.name = nsc_data.disp.name;
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

void ClientGameData::save_all() {
  if (this->system_data) {
    this->save_system_file();
  }
  if (this->character_data) {
    this->save_character_file();
  }
  if (this->guild_card_data) {
    this->save_guild_card_file();
  }
  if (this->external_bank) {
    string filename = this->shared_bank_filename();
    save_object_file<PlayerBank>(filename, *this->external_bank);
    player_data_log.info("Saved shared bank file %s", filename.c_str());
  }
  if (this->external_bank_character) {
    this->save_character_file(
        this->character_filename(this->external_bank_character_index),
        this->system_data,
        this->external_bank_character);
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

void ClientGameData::save_character_file(
    const string& filename,
    shared_ptr<const PSOBBBaseSystemFile> system,
    shared_ptr<const PSOBBCharacterFile> character) {
  auto f = fopen_unique(filename, "wb");
  PSOCommandHeaderBB header = {sizeof(PSOCommandHeaderBB) + sizeof(PSOBBCharacterFile) + sizeof(PSOBBBaseSystemFile) + sizeof(PSOBBTeamMembership), 0x00E7, 0x00000000};
  fwritex(f.get(), header);
  fwritex(f.get(), *character);
  fwritex(f.get(), *system);
  // TODO: Technically, we should write the actual team membership struct to the
  // file here, but that would cause ClientGameData to depend on License, which
  // it currently does not. This data doesn't matter at all for correctness
  // within newserv, since it ignores this data entirely and instead generates
  // the membership struct from the team ID in the License and the team's state.
  // So, writing correct data here would mostly be for compatibility with other
  // PSO servers. But if the other server is newserv, then this data would be
  // used anyway, and if it's not, then it would presumably have a different set
  // of teams with a different set of team IDs anyway, so the membership struct
  // here would be useless either way.
  static const PSOBBTeamMembership empty_membership;
  fwritex(f.get(), empty_membership);
  player_data_log.info("Saved character file %s", filename.c_str());
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

  this->save_character_file(this->character_filename(), this->system_data, this->character_data);
}

void ClientGameData::save_guild_card_file() const {
  if (!this->guild_card_data.get()) {
    throw logic_error("no Guild Card file loaded");
  }
  string filename = this->guild_card_filename();
  save_object_file(filename, *this->guild_card_data);
  player_data_log.info("Saved Guild Card file %s", filename.c_str());
}

PlayerBank& ClientGameData::current_bank() {
  if (this->external_bank) {
    return *this->external_bank;
  } else if (this->external_bank_character) {
    return this->external_bank_character->bank;
  }
  return this->character()->bank;
}

std::shared_ptr<PSOBBCharacterFile> ClientGameData::current_bank_character() {
  return this->external_bank_character ? this->external_bank_character : this->character();
}

void ClientGameData::use_default_bank() {
  if (this->external_bank) {
    string filename = this->shared_bank_filename();
    save_object_file<PlayerBank>(filename, *this->external_bank);
    this->external_bank.reset();
    player_data_log.info("Detached shared bank %s", filename.c_str());
  }
  if (this->external_bank_character) {
    string filename = this->character_filename(this->external_bank_character_index);
    this->save_character_file(filename, this->system_data, this->external_bank_character);
    this->external_bank_character.reset();
    player_data_log.info("Detached character %s from bank", filename.c_str());
  }
}

bool ClientGameData::use_shared_bank() {
  this->use_default_bank();
  string filename = this->shared_bank_filename();
  if (isfile(filename)) {
    this->external_bank = make_shared<PlayerBank>(load_object_file<PlayerBank>(filename));
    player_data_log.info("Loaded shared bank %s", filename.c_str());
    return true;
  } else {
    this->external_bank = make_shared<PlayerBank>();
    player_data_log.info("Created shared bank for %s", filename.c_str());
    return false;
  }
}

void ClientGameData::use_character_bank(int8_t index) {
  this->use_default_bank();
  if (index != this->bb_character_index) {
    string filename = this->character_filename(index);
    this->external_bank_character = this->files_manager->get_character(filename);
    if (this->external_bank_character) {
      this->external_bank_character_index = index;
      player_data_log.info("Using loaded character file %s for external bank", filename.c_str());
    } else if (isfile(filename)) {
      auto f = fopen_unique(filename, "rb");
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
      this->external_bank_character = make_shared<PSOBBCharacterFile>(freadx<PSOBBCharacterFile>(f.get()));
      this->external_bank_character_index = index;
      this->files_manager->set_character(filename, this->external_bank_character);
      player_data_log.info("Loaded character data from %s for external bank", filename.c_str());
    } else {
      throw runtime_error("character does not exist");
    }
  }
}
