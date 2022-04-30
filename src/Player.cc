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



// originally there was going to be a language-based header, but then I decided against it.
// these strings were already in use for that parser, so I didn't bother changing them.
#define PLAYER_FILE_SIGNATURE   "newserv player file format; 10 sections present; sequential;"
#define ACCOUNT_FILE_SIGNATURE  "newserv account file format; 7 sections present; sequential;"



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
    this->char_class = 0; // fomar -> humar
  } else if (this->char_class == 10) {
    this->char_class = 1; // ramarl -> hunewearl
  } else if (this->char_class == 9) {
    this->char_class = 5; // hucaseal -> racaseal
  }

  // if the player is still not a valid class, make them appear as the "ninja" NPC
  if (this->char_class > 8) {
    this->extra_model = 0;
    this->v2_flags |= 2;
  }
  this->version = 2;
}

// converts PC/GC player data to BB format
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

// converts BB player data to PC/GC format
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

// creates a player preview, which can then be sent to a BB client for character select
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
    this->items[x].data.item_id = 0x0F010000 + x;
  }
}

void PlayerBank::save(const string& filename) const {
  save_file(filename, this, sizeof(*this));
}



void Player::import(const PSOPlayerDataPC& pc) {
  this->inventory = pc.inventory;
  this->disp = pc.disp.to_bb();
  // TODO: Add these fields to the existing structure so we can parse them
  // this->info_board = pc.info_board;
  // this->blocked_senders = pc.blocked_senders;
  // this->auto_reply = pc.auto_reply;
}

void Player::import(const PSOPlayerDataGC& gc) {
  this->inventory = gc.inventory;
  this->disp = gc.disp.to_bb();
  this->info_board = gc.info_board;
  this->blocked_senders = gc.blocked_senders;
  if (gc.auto_reply_enabled) {
    this->auto_reply = gc.auto_reply;
  } else {
    this->auto_reply.clear();
  }
}

void Player::import(const PSOPlayerDataBB& bb) {
  // Note: we don't copy the inventory and disp here because we already have
  // it (we sent the player data to the client in the first place)
  this->info_board = bb.info_board;
  this->blocked_senders = bb.blocked_senders;
  if (bb.auto_reply_enabled) {
    this->auto_reply = bb.auto_reply;
  } else {
    this->auto_reply.clear();
  }
}

PlayerBB Player::export_bb_player_data() const {
  PlayerBB bb;
  bb.inventory = this->inventory;
  bb.disp = this->disp;
  bb.unknown.clear();
  bb.option_flags = this->option_flags;
  bb.quest_data1 = this->quest_data1;
  bb.bank = this->bank;
  bb.serial_number = this->serial_number;
  bb.name = this->disp.name;
  bb.team_name = this->team_name;
  bb.guild_card_desc = this->guild_card_desc;
  bb.reserved1 = 0;
  bb.reserved2 = 0;
  bb.section_id = this->disp.section_id;
  bb.char_class = this->disp.char_class;
  bb.unknown3 = 0;
  bb.symbol_chats = this->symbol_chats;
  bb.shortcuts = this->shortcuts;
  bb.auto_reply = this->auto_reply;
  bb.info_board = this->info_board;
  bb.unknown5.clear();
  bb.challenge_data = this->challenge_data;
  bb.tech_menu_config = this->tech_menu_config;
  bb.unknown6.clear();
  bb.quest_data2 = this->quest_data2;
  bb.key_config = this->key_config;
  return bb;
}

////////////////////////////////////////////////////////////////////////////////

// checksums the guild card file for BB player account data
uint32_t compute_guild_card_checksum(const void* vdata, size_t size) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(vdata);

  uint32_t cs = 0xFFFFFFFF;
  for (size_t offset = 0; offset < size; offset++) {
    cs ^= data[offset];
    for (size_t y = 0; y < 8; y++) {
      if (!(cs & 1)) {
        cs = (cs >> 1) & 0x7FFFFFFF;
      } else {
        cs = ((cs >> 1) & 0x7FFFFFFF) ^ 0xEDB88320;
      }
    }
  }
  return (cs ^ 0xFFFFFFFF);
}

void Player::load_account_data(const string& filename) {
  SavedAccountBB account = load_object_file<SavedAccountBB>(filename);
  if (account.signature != ACCOUNT_FILE_SIGNATURE) {
    throw runtime_error("account data header is incorrect");
  }
  this->blocked_senders = account.blocked_senders;
  this->guild_cards = account.guild_cards;
  this->key_config = account.key_config;
  this->option_flags = account.option_flags;
  this->shortcuts = account.shortcuts;
  this->symbol_chats = account.symbol_chats;
  this->team_name = account.team_name;
}

void Player::save_account_data(const string& filename) const {
  SavedAccountBB account;
  account.signature = ACCOUNT_FILE_SIGNATURE;
  account.blocked_senders = this->blocked_senders;
  account.guild_cards = this->guild_cards;
  account.key_config = this->key_config;
  account.option_flags = this->option_flags;
  account.shortcuts = this->shortcuts;
  account.symbol_chats = this->symbol_chats;
  account.team_name = this->team_name;
  save_file(filename, &account, sizeof(account));
}

void Player::load_player_data(const string& filename) {
  SavedPlayerBB player = load_object_file<SavedPlayerBB>(filename);
  if (player.signature != PLAYER_FILE_SIGNATURE) {
    throw runtime_error("account data header is incorrect");
  }
  this->auto_reply = player.auto_reply;
  this->bank = player.bank;
  this->challenge_data = player.challenge_data;
  this->disp = player.disp;
  this->guild_card_desc = player.guild_card_desc;
  this->info_board = player.info_board;
  this->inventory = player.inventory;
  this->quest_data1 = player.quest_data1;
  this->quest_data2 = player.quest_data2;
  this->tech_menu_config = player.tech_menu_config;
}

void Player::save_player_data(const string& filename) const {
  SavedPlayerBB player;
  player.signature = PLAYER_FILE_SIGNATURE;
  player.preview = this->disp.to_preview();
  player.auto_reply = this->auto_reply;
  player.bank = this->bank;
  player.challenge_data = this->challenge_data;
  player.disp = this->disp;
  player.guild_card_desc = this->guild_card_desc;
  player.info_board = this->info_board;
  player.inventory = this->inventory;
  player.quest_data1 = this->quest_data1;
  player.quest_data2 = this->quest_data2;
  player.tech_menu_config = this->tech_menu_config;
  save_file(filename, &player, sizeof(player));
}



PlayerLobbyDataPC::PlayerLobbyDataPC() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0) { }

PlayerLobbyDataGC::PlayerLobbyDataGC() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0) { }

PlayerLobbyDataBB::PlayerLobbyDataBB() noexcept
  : player_tag(0), guild_card(0), ip_address(0), client_id(0), unknown2(0) { }



////////////////////////////////////////////////////////////////////////////////

const uint32_t meseta_identifier = 0x00040000;

uint32_t ItemData::primary_identifier() const {
  if (this->item_data1[0] == 0x03 && this->item_data1[1] == 0x02) {
    return 0x00030200; // Tech disk (data1[2] is level, so omit it)
  } else if (this->item_data1[0] == 0x02) {
    return 0x00020000 | (this->item_data1[1] << 8); // Mag
  } else {
    return (this->item_data1[0] << 16) | (this->item_data1[1] << 8) | this->item_data1[2];
  }
}

PlayerBankItem PlayerInventoryItem::to_bank_item() const {
  PlayerBankItem bank_item;
  bank_item.data = this->data;

  if (combine_item_to_max.count(this->data.primary_identifier())) {
    bank_item.amount = this->data.item_data1[5];
  } else {
    bank_item.amount = 1;
  }
  bank_item.show_flags = 1;

  return bank_item;
}

PlayerInventoryItem PlayerBankItem::to_inventory_item() const {
  PlayerInventoryItem item;
  item.data = this->data;
  if (item.data.item_data1[0] > 2) {
    item.equip_flags = 0x0044;
  } else {
    item.equip_flags = 0x0050;
  }
  item.equip_flags = 0x0001; // TODO: is this a bug?
  item.tech_flag = 0x0001;
  return item;
}

void Player::add_item(const PlayerInventoryItem& item) {
  uint32_t pid = item.data.primary_identifier();

  // is it meseta? then just add to the meseta total
  if (pid == meseta_identifier) {
    this->disp.meseta += item.data.item_data2d;
    if (this->disp.meseta > 999999) {
      this->disp.meseta = 999999;
    }
    return;
  }

  // is it a combine item?
  try {
    uint32_t combine_max = combine_item_to_max.at(pid);

    // is there already a stack of it in the player's inventory?
    size_t y;
    for (y = 0; y < this->inventory.num_items; y++) {
      if (this->inventory.items[y].data.primary_identifier() == item.data.primary_identifier()) {
        break;
      }
    }

    // if there's already a stack, add to the stack and return
    if (y < this->inventory.num_items) {
      this->inventory.items[y].data.item_data1[5] += item.data.item_data1[5];
      if (this->inventory.items[y].data.item_data1[5] > combine_max) {
        this->inventory.items[y].data.item_data1[5] = combine_max;
      }
      return;
    }
  } catch (const out_of_range&) { }

  // else, just add the item if there's room
  if (this->inventory.num_items >= 30) {
    throw runtime_error("inventory is full");
  }
  this->inventory.items[this->inventory.num_items] = item;
  this->inventory.num_items++;
}

// adds an item to a bank
void PlayerBank::add_item(const PlayerBankItem& item) {
  uint32_t pid = item.data.primary_identifier();

  // is it meseta? then just add to the meseta total
  if (pid == meseta_identifier) {
    this->meseta += item.data.item_data2d;
    if (this->meseta > 999999) {
      this->meseta = 999999;
    }
    return;
  }

  // is it a combine item?
  try {
    uint32_t combine_max = combine_item_to_max.at(pid);

    // is there already a stack of it in the player's inventory?
    size_t y;
    for (y = 0; y < this->num_items; y++) {
      if (this->items[y].data.primary_identifier() == item.data.primary_identifier()) {
        break;
      }
    }

    // if there's already a stack, add to the stack and return
    if (y < this->num_items) {
      this->items[y].data.item_data1[5] += item.data.item_data1[5];
      if (this->items[y].data.item_data1[5] > combine_max) {
        this->items[y].data.item_data1[5] = combine_max;
      }
      this->items[y].amount = this->items[y].data.item_data1[5];
      return;
    }
  } catch (const out_of_range&) { }

  // else, just add the item if there's room
  if (this->num_items >= 200) {
    throw runtime_error("bank is full");
  }
  this->items[this->num_items] = item;
  this->num_items++;
}

PlayerInventoryItem Player::remove_item(uint32_t item_id, uint32_t amount) {
  PlayerInventoryItem ret;

  // are we removing meseta? then create a meseta item
  if (item_id == 0xFFFFFFFF) {
    if (amount > this->disp.meseta) {
      throw out_of_range("player does not have enough meseta");
    }

    memset(&ret, 0, sizeof(ret));
    ret.data.item_data1[0] = 0x04;
    ret.data.item_data2d = amount;
    this->disp.meseta -= amount;
    return ret;
  }

  // find this item
  size_t index = this->inventory.find_item(item_id);
  auto& inventory_item = this->inventory.items[index];

  // is it a combine item, and are we removing less than we have of it?
  // (amount == 0 means remove all of it)
  if (amount && (amount < inventory_item.data.item_data1[5]) &&
      combine_item_to_max.count(inventory_item.data.primary_identifier())) {
    ret = inventory_item;
    ret.data.item_data1[5] = amount;
    ret.data.item_id = 0xFFFFFFFF;
    inventory_item.data.item_data1[5] -= amount;
    return ret;
  }

  // not a combine item, or we're removing the whole stack? then just remove the item
  ret = inventory_item;
  this->inventory.num_items--;
  memcpy(&this->inventory.items[index], &this->inventory.items[index + 1],
      sizeof(PlayerInventoryItem) * (this->inventory.num_items - index));
  return ret;
}

// removes an item from a bank. works just like RemoveItem for inventories; I won't comment it
PlayerBankItem PlayerBank::remove_item(uint32_t item_id, uint32_t amount) {
  PlayerBankItem ret;

  // are we removing meseta? then create a meseta item
  if (item_id == 0xFFFFFFFF) {
    if (amount > this->meseta) {
      throw out_of_range("player does not have enough meseta");
    }
    memset(&ret, 0, sizeof(ret));
    ret.data.item_data1[0] = 0x04;
    ret.data.item_data2d = amount;
    this->meseta -= amount;
    return ret;
  }

  // find this item
  size_t index = this->find_item(item_id);
  auto& bank_item = this->items[index];

  // is it a combine item, and are we removing less than we have of it?
  // (amount == 0 means remove all of it)
  if (amount && (amount < bank_item.data.item_data1[5]) &&
      combine_item_to_max.count(bank_item.data.primary_identifier())) {
    ret = bank_item;
    ret.data.item_data1[5] = amount;
    ret.amount = amount;
    bank_item.data.item_data1[5] -= amount;
    bank_item.amount -= amount;
    return ret;
  }

  // not a combine item, or we're removing the whole stack? then just remove the item
  ret = bank_item;
  this->num_items--;
  memcpy(&this->items[index], &this->items[index + 1],
      sizeof(PlayerBankItem) * (this->num_items - index));
  return ret;
}

size_t PlayerInventory::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.item_id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

size_t PlayerBank::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->num_items; x++) {
    if (this->items[x].data.item_id == item_id) {
      return x;
    }
  }
  throw out_of_range("item not present");
}

void Player::print_inventory(FILE* stream) const {
  fprintf(stream, "[PlayerInventory] Meseta: %" PRIu32 "\n", this->disp.meseta.load());
  fprintf(stream, "[PlayerInventory] %hhu items\n", this->inventory.num_items);
  for (size_t x = 0; x < this->inventory.num_items; x++) {
    const auto& item = this->inventory.items[x];
    string name = name_for_item(item.data);
    fprintf(stream, "[PlayerInventory]   %zu (%08" PRIX32 "): %08" PRIX32 " (%s)\n",
        x, item.data.item_id.load(), item.data.primary_identifier(), name.c_str());
  }
}

string filename_for_player_bb(const string& username, uint8_t player_index) {
  return string_printf("system/players/player_%s_%hhu.nsc", username.c_str(),
      static_cast<uint8_t>(player_index + 1));
}

string filename_for_bank_bb(const string& username, const std::string& bank_name) {
  return string_printf("system/players/bank_%s_%s.nsb", username.c_str(),
      bank_name.c_str());
}

string filename_for_class_template_bb(uint8_t char_class) {
  return string_printf("system/blueburst/player_class_%hhu.nsc", char_class);
}

string filename_for_account_bb(const string& username) {
  return string_printf("system/players/account_%s.nsa", username.c_str());
}
