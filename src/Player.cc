#include "Player.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <stdexcept>
#include <phosg/Filesystem.hh>

#include "Text.hh"
#include "Version.hh"
#include "StaticGameData.hh"

using namespace std;



// Originally there was going to be a language-based header, but then I decided
// against it. These strings were already in use for that parser, so I didn't
// bother changing them.
static const string PLAYER_FILE_SIGNATURE =
    "newserv player file format; 10 sections present; sequential;";
static const string ACCOUNT_FILE_SIGNATURE =
    "newserv account file format; 7 sections present; sequential;";



PlayerStats::PlayerStats() noexcept
  : atp(0), mst(0), evp(0), hp(0), dfp(0), ata(0), lck(0) { }

PlayerDispDataPCGC::PlayerDispDataPCGC() noexcept
  : level(0),
    experience(0),
    meseta(0),
    unknown_a2(0),
    name_color(0),
    extra_model(0),
    name_color_checksum(0),
    section_id(0),
    char_class(0),
    v2_flags(0),
    version(0),
    v1_flags(0),
    costume(0),
    skin(0),
    face(0),
    head(0),
    hair(0),
    hair_r(0),
    hair_g(0),
    hair_b(0),
    proportion_x(0),
    proportion_y(0) { }

void PlayerDispDataPCGC::enforce_pc_limits() {
  // PC has fewer classes, so we'll substitute some here
  if (this->char_class == 11) {
    this->char_class = 0; // FOmar -> HUmar
  } else if (this->char_class == 10) {
    this->char_class = 1; // RAmarl -> HUnewearl
  } else if (this->char_class == 9) {
    this->char_class = 5; // HUcaseal -> RAcaseal
  }

  // If the player is somehow still not a valid class, make them appear as the
  // "ninja" NPC
  if (this->char_class > 8) {
    this->extra_model = 0;
    this->v2_flags |= 2;
  }
  this->version = 2;
}

PlayerDispDataBB PlayerDispDataPCGC::to_bb() const {
  PlayerDispDataBB bb;
  bb.stats.atp = this->stats.atp;
  bb.stats.mst = this->stats.mst;
  bb.stats.evp = this->stats.evp;
  bb.stats.hp = this->stats.hp;
  bb.stats.dfp = this->stats.dfp;
  bb.stats.ata = this->stats.ata;
  bb.stats.lck = this->stats.lck;
  bb.unknown_a1 = this->unknown_a1;
  bb.level = this->level;
  bb.experience = this->experience;
  bb.meseta = this->meseta;
  bb.guild_card = "         0";
  bb.unknown_a2 = this->unknown_a2;
  bb.name_color = this->name_color;
  bb.extra_model = this->extra_model;
  bb.unused = this->unused;
  bb.name_color_checksum = this->name_color_checksum;
  bb.section_id = this->section_id;
  bb.char_class = this->char_class;
  bb.v2_flags = this->v2_flags;
  bb.version = this->version;
  bb.v1_flags = this->v1_flags;
  bb.costume = this->costume;
  bb.skin = this->skin;
  bb.face = this->face;
  bb.head = this->head;
  bb.hair = this->hair;
  bb.hair_r = this->hair_r;
  bb.hair_g = this->hair_g;
  bb.hair_b = this->hair_b;
  bb.proportion_x = this->proportion_x;
  bb.proportion_y = this->proportion_y;
  bb.name = add_language_marker(this->name, 'J');
  bb.config = this->config;
  bb.technique_levels = this->technique_levels;
  return bb;
}



PlayerDispDataBB::PlayerDispDataBB() noexcept
  : level(0),
    experience(0),
    meseta(0),
    unknown_a2(0),
    name_color(0),
    extra_model(0),
    name_color_checksum(0),
    section_id(0),
    char_class(0),
    v2_flags(0),
    version(0),
    v1_flags(0),
    costume(0),
    skin(0),
    face(0),
    head(0),
    hair(0),
    hair_r(0),
    hair_g(0),
    hair_b(0),
    proportion_x(0),
    proportion_y(0) { }

PlayerDispDataPCGC PlayerDispDataBB::to_pcgc() const {
  PlayerDispDataPCGC pcgc;
  pcgc.stats.atp = this->stats.atp;
  pcgc.stats.mst = this->stats.mst;
  pcgc.stats.evp = this->stats.evp;
  pcgc.stats.hp = this->stats.hp;
  pcgc.stats.dfp = this->stats.dfp;
  pcgc.stats.ata = this->stats.ata;
  pcgc.stats.lck = this->stats.lck;
  pcgc.unknown_a1 = this->unknown_a1;
  pcgc.level = this->level;
  pcgc.experience = this->experience;
  pcgc.meseta = this->meseta;
  pcgc.unknown_a2 = this->unknown_a2;
  pcgc.name_color = this->name_color;
  pcgc.extra_model = this->extra_model;
  pcgc.unused = this->unused;
  pcgc.name_color_checksum = this->name_color_checksum;
  pcgc.section_id = this->section_id;
  pcgc.char_class = this->char_class;
  pcgc.v2_flags = this->v2_flags;
  pcgc.version = this->version;
  pcgc.v1_flags = this->v1_flags;
  pcgc.costume = this->costume;
  pcgc.skin = this->skin;
  pcgc.face = this->face;
  pcgc.head = this->head;
  pcgc.hair = this->hair;
  pcgc.hair_r = this->hair_r;
  pcgc.hair_g = this->hair_g;
  pcgc.hair_b = this->hair_b;
  pcgc.proportion_x = this->proportion_x;
  pcgc.proportion_y = this->proportion_y;
  pcgc.name = remove_language_marker(this->name);
  pcgc.config = this->config;
  pcgc.technique_levels = this->technique_levels;
  return pcgc;
}

PlayerDispDataBBPreview PlayerDispDataBB::to_preview() const {
  PlayerDispDataBBPreview pre;
  pre.level = this->level;
  pre.experience = this->experience;
  pre.guild_card = this->guild_card;
  pre.unknown_a2 = this->unknown_a2;
  pre.name_color = this->name_color;
  pre.extra_model = this->extra_model;
  pre.unused = this->unused;
  pre.name_color_checksum = this->name_color_checksum;
  pre.section_id = this->section_id;
  pre.char_class = this->char_class;
  pre.v2_flags = this->v2_flags;
  pre.version = this->version;
  pre.v1_flags = this->v1_flags;
  pre.costume = this->costume;
  pre.skin = this->skin;
  pre.face = this->face;
  pre.head = this->head;
  pre.hair = this->hair;
  pre.hair_r = this->hair_r;
  pre.hair_g = this->hair_g;
  pre.hair_b = this->hair_b;
  pre.proportion_x = this->proportion_x;
  pre.proportion_y = this->proportion_y;
  pre.name = this->name;
  pre.play_time = 0; // TODO: Store this somewhere and return it here
  return pre;
}

void PlayerDispDataBB::apply_preview(const PlayerDispDataBBPreview& pre) {
  this->level = pre.level;
  this->experience = pre.experience;
  this->guild_card = pre.guild_card;
  this->unknown_a2 = pre.unknown_a2;
  this->name_color = pre.name_color;
  this->extra_model = pre.extra_model;
  this->unused = pre.unused;
  this->name_color_checksum = pre.name_color_checksum;
  this->section_id = pre.section_id;
  this->char_class = pre.char_class;
  this->v2_flags = pre.v2_flags;
  this->version = pre.version;
  this->v1_flags = pre.v1_flags;
  this->costume = pre.costume;
  this->skin = pre.skin;
  this->face = pre.face;
  this->head = pre.head;
  this->hair = pre.hair;
  this->hair_r = pre.hair_r;
  this->hair_g = pre.hair_g;
  this->hair_b = pre.hair_b;
  this->proportion_x = pre.proportion_x;
  this->proportion_y = pre.proportion_y;
  this->name = pre.name;
}



PlayerDispDataBBPreview::PlayerDispDataBBPreview() noexcept
  : experience(0),
    level(0),
    unknown_a2(0),
    name_color(0),
    extra_model(0),
    name_color_checksum(0),
    section_id(0),
    char_class(0),
    v2_flags(0),
    version(0),
    v1_flags(0),
    costume(0),
    skin(0),
    face(0),
    head(0),
    hair(0),
    hair_r(0),
    hair_g(0),
    hair_b(0),
    proportion_x(0),
    proportion_y(0),
    play_time(0) { }



GuildCardGC::GuildCardGC() noexcept
  : player_tag(0), serial_number(0), reserved1(1), reserved2(1), section_id(0), char_class(0) { }

GuildCardBB::GuildCardBB() noexcept
  : serial_number(0), reserved1(1), reserved2(1), section_id(0), char_class(0) { }



void PlayerBank::load(const string& filename) {
  *this = load_object_file<PlayerBank>(filename);
  for (uint32_t x = 0; x < this->num_items; x++) {
    this->items[x].data.id = 0x0F010000 + x;
  }
}

void PlayerBank::save(const string& filename) const {
  save_file(filename, this, sizeof(*this));
}



////////////////////////////////////////////////////////////////////////////////

ClientGameData::~ClientGameData() {
  if (!this->bb_username.empty()) {
    if (this->account_data.get()) {
      this->save_account_data();
    }
    if (this->player_data.get()) {
      this->save_player_data();
    }
  }
}

shared_ptr<SavedAccountDataBB> ClientGameData::account(bool should_load) {
  if (!this->account_data.get() && should_load) {
    if (this->bb_username.empty()) {
      this->account_data.reset(new SavedAccountDataBB());
    } else {
      this->load_account_data();
    }
  }
  return this->account_data;
}

shared_ptr<SavedPlayerDataBB> ClientGameData::player(bool should_load) {
  if (!this->player_data.get() && should_load) {
    if (this->bb_username.empty()) {
      this->player_data.reset(new SavedPlayerDataBB());
    } else {
      this->load_player_data();
    }
  }
  return this->player_data;
}

shared_ptr<const SavedAccountDataBB> ClientGameData::account() const {
  if (!this->account_data.get()) {
    throw runtime_error("account data is not loaded");
  }
  return this->account_data;
}

shared_ptr<const SavedPlayerDataBB> ClientGameData::player() const {
  if (!this->player_data.get()) {
    throw runtime_error("player data is not loaded");
  }
  return this->player_data;
}

string ClientGameData::account_data_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have account data");
  }
  return string_printf("system/players/account_%s.nsa",
      this->bb_username.c_str());
}

string ClientGameData::player_data_filename() const {
  if (this->bb_username.empty()) {
    throw logic_error("non-BB players do not have account data");
  }
  return string_printf("system/players/player_%s_%zu.nsc",
      this->bb_username.c_str(), this->bb_player_index + 1);
}

string ClientGameData::player_template_filename(uint8_t char_class) {
  return string_printf("system/blueburst/player_class_%hhu.nsc", char_class);
}

void ClientGameData::create_player(
    const PlayerDispDataBBPreview& preview,
    shared_ptr<const LevelTable> level_table) {
  shared_ptr<SavedPlayerDataBB> data(new SavedPlayerDataBB(
      load_object_file<SavedPlayerDataBB>(player_template_filename(preview.char_class))));
  if (data->signature != PLAYER_FILE_SIGNATURE) {
    throw runtime_error("player data header is incorrect");
  }

  try {
    data->disp.apply_preview(preview);
    data->disp.stats = level_table->base_stats_for_class(data->disp.char_class);
  } catch (const exception& e) {
    throw runtime_error(string_printf("template application failed: %s", e.what()));
  }

  this->player_data = data;

  this->save_player_data();
}

void ClientGameData::load_account_data() {
  string filename = this->account_data_filename();

  shared_ptr<SavedAccountDataBB> data;
  try {
    data.reset(new SavedAccountDataBB(
        load_object_file<SavedAccountDataBB>(filename)));
    if (data->signature != ACCOUNT_FILE_SIGNATURE) {
      throw runtime_error("account data header is incorrect");
    }
  } catch (const exception& e) {
    log(INFO, "[BB/Account] No account data for %s; using default",
        this->bb_username.c_str());
    data.reset(new SavedAccountDataBB(
        load_object_file<SavedAccountDataBB>("system/blueburst/default.nsa")));
    if (data->signature != ACCOUNT_FILE_SIGNATURE) {
      throw runtime_error("default account data header is incorrect");
    }
  }

  this->account_data = data;
  log(INFO, "Loaded account data file %s", filename.c_str());
}

void ClientGameData::save_account_data() const {
  string filename = this->account_data_filename();
  save_file(filename, this->account_data.get(), sizeof(SavedAccountDataBB));
  log(INFO, "Saved account data file %s", filename.c_str());
}

void ClientGameData::load_player_data() {
  string filename = this->player_data_filename();
  shared_ptr<SavedPlayerDataBB> data(new SavedPlayerDataBB(
      load_object_file<SavedPlayerDataBB>(filename)));
  if (data->signature != PLAYER_FILE_SIGNATURE) {
    throw runtime_error("player data header is incorrect");
  }
  this->player_data = data;
  log(INFO, "Loaded player data file %s", filename.c_str());
}

void ClientGameData::save_player_data() const {
  string filename = this->player_data_filename();
  save_file(filename, this->player_data.get(), sizeof(SavedPlayerDataBB));
  log(INFO, "Saved player data file %s", filename.c_str());
}

void ClientGameData::import_player(const PSOPlayerDataPC& pc) {
  auto player = this->player();
  player->inventory = pc.inventory;
  player->disp = pc.disp.to_bb();
  // TODO: Add these fields to the command structure so we can parse them
  // info_board = pc.info_board;
  // blocked_senders = pc.blocked_senders;
  // auto_reply = pc.auto_reply;
}

void ClientGameData::import_player(const PSOPlayerDataGC& gc) {
  auto account = this->account();
  auto player = this->player();
  player->inventory = gc.inventory;
  player->disp = gc.disp.to_bb();
  player->info_board = gc.info_board;
  account->blocked_senders = gc.blocked_senders;
  if (gc.auto_reply_enabled) {
    player->auto_reply = gc.auto_reply;
  } else {
    player->auto_reply.clear();
  }
}

void ClientGameData::import_player(const PSOPlayerDataBB& bb) {
  auto account = this->account();
  auto player = this->player();
  // Note: we don't copy the inventory and disp here because we already have
  // them (we sent the player data to the client in the first place)
  player->info_board = bb.info_board;
  account->blocked_senders = bb.blocked_senders;
  if (bb.auto_reply_enabled) {
    player->auto_reply = bb.auto_reply;
  } else {
    player->auto_reply.clear();
  }
}

PlayerBB ClientGameData::export_player_bb() {
  auto account = this->account();
  auto player = this->player();

  PlayerBB ret;
  ret.inventory = player->inventory;
  ret.disp = player->disp;
  ret.unknown.clear();
  ret.option_flags = account->option_flags;
  ret.quest_data1 = player->quest_data1;
  ret.bank = player->bank;
  ret.serial_number = this->serial_number;
  ret.name = player->disp.name;
  ret.team_name = account->team_name;
  ret.guild_card_desc = player->guild_card_desc;
  ret.reserved1 = 0;
  ret.reserved2 = 0;
  ret.section_id = player->disp.section_id;
  ret.char_class = player->disp.char_class;
  ret.unknown3 = 0;
  ret.symbol_chats = account->symbol_chats;
  ret.shortcuts = account->shortcuts;
  ret.auto_reply = player->auto_reply;
  ret.info_board = player->info_board;
  ret.unknown5.clear();
  ret.challenge_data = player->challenge_data;
  ret.tech_menu_config = player->tech_menu_config;
  ret.unknown6.clear();
  ret.quest_data2 = player->quest_data2;
  ret.key_config = account->key_config;
  return ret;
}



PlayerLobbyDataPC::PlayerLobbyDataPC() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0) { }

PlayerLobbyDataGC::PlayerLobbyDataGC() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0) { }

PlayerLobbyDataBB::PlayerLobbyDataBB() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0), unknown2(0) { }



////////////////////////////////////////////////////////////////////////////////

constexpr uint32_t MESETA_IDENTIFIER = 0x00040000;

ItemData::ItemData() {
  this->data1d[0] = 0;
  this->data1d[1] = 0;
  this->data1d[2] = 0;
  this->id = 0xFFFFFFFF;
  this->data2d = 0;
}

uint32_t ItemData::primary_identifier() const {
  if (this->data1[0] == 0x03 && this->data1[1] == 0x02) {
    return 0x00030200; // Tech disk (data1[2] is level, so omit it)
  } else if (this->data1[0] == 0x02) {
    return 0x00020000 | (this->data1[1] << 8); // Mag
  } else {
    return (this->data1[0] << 16) | (this->data1[1] << 8) | this->data1[2];
  }
}

PlayerInventoryItem::PlayerInventoryItem()
  : equip_flags(0x0000), tech_flag(0x0000), data() { }

PlayerInventoryItem::PlayerInventoryItem(const PlayerBankItem& src)
  : equip_flags((this->data.data1[0] > 2) ? 0x0044 : 0x0050),
    tech_flag(0x0001),
    data(src.data) { }

PlayerBankItem::PlayerBankItem()
  : data(), amount(0), show_flags(0) { }

PlayerBankItem::PlayerBankItem(const PlayerInventoryItem& src)
  : data(src.data),
    amount(combine_item_to_max.count(this->data.primary_identifier()) ? this->data.data1[5] : 1),
    show_flags(1) { }

// TODO: Eliminate duplication between this function and the parallel function
// in PlayerBank
void SavedPlayerDataBB::add_item(const PlayerInventoryItem& item) {
  uint32_t pid = item.data.primary_identifier();

  // Annoyingly, meseta is in the disp data, not in the inventory struct. If the
  // item is meseta, we have to modify disp instead.
  if (pid == MESETA_IDENTIFIER) {
    this->disp.meseta += item.data.data2d;
    if (this->disp.meseta > 999999) {
      this->disp.meseta = 999999;
    }
    return;
  }

  // Handle combinable items
  try {
    uint32_t combine_max = combine_item_to_max.at(pid);

    // Get the item index if there's already a stack of the same item in the
    // player's inventory
    size_t y;
    for (y = 0; y < this->inventory.num_items; y++) {
      if (this->inventory.items[y].data.primary_identifier() == item.data.primary_identifier()) {
        break;
      }
    }

    // If we found an existing stack, add it to the total and return
    if (y < this->inventory.num_items) {
      this->inventory.items[y].data.data1[5] += item.data.data1[5];
      if (this->inventory.items[y].data.data1[5] > combine_max) {
        this->inventory.items[y].data.data1[5] = combine_max;
      }
      return;
    }
  } catch (const out_of_range&) { }

  // If we get here, then it's not meseta and not a combine item, so it needs to
  // go into an empty inventory slot
  if (this->inventory.num_items >= 30) {
    throw runtime_error("inventory is full");
  }
  this->inventory.items[this->inventory.num_items] = item;
  this->inventory.num_items++;
}

void PlayerBank::add_item(const PlayerBankItem& item) {
  uint32_t pid = item.data.primary_identifier();

  if (pid == MESETA_IDENTIFIER) {
    this->meseta += item.data.data2d;
    if (this->meseta > 999999) {
      this->meseta = 999999;
    }
    return;
  }

  try {
    uint32_t combine_max = combine_item_to_max.at(pid);

    size_t y;
    for (y = 0; y < this->num_items; y++) {
      if (this->items[y].data.primary_identifier() == item.data.primary_identifier()) {
        break;
      }
    }

    if (y < this->num_items) {
      this->items[y].data.data1[5] += item.data.data1[5];
      if (this->items[y].data.data1[5] > combine_max) {
        this->items[y].data.data1[5] = combine_max;
      }
      this->items[y].amount = this->items[y].data.data1[5];
      return;
    }
  } catch (const out_of_range&) { }

  if (this->num_items >= 200) {
    throw runtime_error("bank is full");
  }
  this->items[this->num_items] = item;
  this->num_items++;
}

// TODO: Eliminate code duplication between this function and the parallel
// function in PlayerBank
PlayerInventoryItem SavedPlayerDataBB::remove_item(
    uint32_t item_id, uint32_t amount) {
  PlayerInventoryItem ret;

  // If we're removing meseta (signaled by an invalid item ID), then create a
  // meseta item.
  if (item_id == 0xFFFFFFFF) {
    if (amount > this->disp.meseta) {
      throw out_of_range("player does not have enough meseta");
    }

    memset(&ret, 0, sizeof(ret));
    ret.data.data1[0] = 0x04;
    ret.data.data2d = amount;
    this->disp.meseta -= amount;
    return ret;
  }

  size_t index = this->inventory.find_item(item_id);
  auto& inventory_item = this->inventory.items[index];

  // If the item is a combine item and are we removing less than we have of it,
  // then create a new item and reduce the amount of the existing stack. Note
  // that passing amount == 0 means to remove the entire stack, so this only
  // applies if amount is nonzero.
  if (amount && (amount < inventory_item.data.data1[5]) &&
      combine_item_to_max.count(inventory_item.data.primary_identifier())) {
    ret = inventory_item;
    ret.data.data1[5] = amount;
    ret.data.id = 0xFFFFFFFF;
    inventory_item.data.data1[5] -= amount;
    return ret;
  }

  // If we get here, then it's not meseta, and either it's not a combine item or
  // we're removing the entire stack. Delete the item from the inventory slot
  // and return the deleted item.
  ret = inventory_item;
  this->inventory.num_items--;
  memcpy(&this->inventory.items[index], &this->inventory.items[index + 1],
      sizeof(PlayerInventoryItem) * (this->inventory.num_items - index));
  return ret;
}

PlayerBankItem PlayerBank::remove_item(uint32_t item_id, uint32_t amount) {
  PlayerBankItem ret;

  if (item_id == 0xFFFFFFFF) {
    if (amount > this->meseta) {
      throw out_of_range("player does not have enough meseta");
    }
    memset(&ret, 0, sizeof(ret));
    ret.data.data1[0] = 0x04;
    ret.data.data2d = amount;
    this->meseta -= amount;
    return ret;
  }

  size_t index = this->find_item(item_id);
  auto& bank_item = this->items[index];

  if (amount && (amount < bank_item.data.data1[5]) &&
      combine_item_to_max.count(bank_item.data.primary_identifier())) {
    ret = bank_item;
    ret.data.data1[5] = amount;
    ret.amount = amount;
    bank_item.data.data1[5] -= amount;
    bank_item.amount -= amount;
    return ret;
  }

  ret = bank_item;
  this->num_items--;
  memcpy(&this->items[index], &this->items[index + 1],
      sizeof(PlayerBankItem) * (this->num_items - index));
  return ret;
}

size_t PlayerInventory::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
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
  fprintf(stream, "[PlayerInventory] Meseta: %" PRIu32 "\n", this->disp.meseta.load());
  fprintf(stream, "[PlayerInventory] %hhu items\n", this->inventory.num_items);
  for (size_t x = 0; x < this->inventory.num_items; x++) {
    const auto& item = this->inventory.items[x];
    auto name = name_for_item(item.data, false);
    fprintf(stream, "[PlayerInventory]   %zu (%08" PRIX32 "): %06" PRIX32 " (%s)\n",
        x, item.data.id.load(), item.data.primary_identifier(), name.c_str());
  }
}
