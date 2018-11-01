#include "Player.hh"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <stdexcept>
#include <phosg/Filesystem.hh>

#include "Text.hh"
#include "Version.hh"

using namespace std;



// originally there was going to be a language-based header, but then I decided against it.
// these strings were already in use for that parser, so I didn't bother changing them.
#define PLAYER_FILE_SIGNATURE  "newserv player file format; 10 sections present; sequential;"
#define ACCOUNT_FILE_SIGNATURE   "newserv account file format; 7 sections present; sequential;"



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
  bb.unknown1 = this->unknown1;
  bb.unknown2[0] = this->unknown2[0];
  bb.unknown2[1] = this->unknown2[1];
  bb.level = this->level;
  bb.experience = this->experience;
  bb.meseta = this->meseta;
  strcpy(bb.guild_card, "         0");
  bb.unknown3[0] = this->unknown3[0];
  bb.unknown3[1] = this->unknown3[1];
  bb.name_color = this->name_color;
  bb.extra_model = this->extra_model;
  memcpy(&bb.unused, &this->unused, 15);
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
  decode_sjis(bb.name, this->name, 0x10);
  add_language_marker_inplace(bb.name, 'J', 0x10);
  memcpy(&bb.config, &this->config, 0x48);
  memcpy(&bb.technique_levels, &this->technique_levels, 0x14);
  return bb;
}

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
  pcgc.unknown1 = this->unknown1;
  pcgc.unknown2[0] = this->unknown2[0];
  pcgc.unknown2[1] = this->unknown2[1];
  pcgc.level = this->level;
  pcgc.experience = this->experience;
  pcgc.meseta = this->meseta;
  pcgc.unknown3[0] = this->unknown3[0];
  pcgc.unknown3[1] = this->unknown3[1];
  pcgc.name_color = this->name_color;
  pcgc.extra_model = this->extra_model;
  memcpy(&pcgc.unused, &this->unused, 15);
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
  encode_sjis(pcgc.name, this->name, 0x10);
  remove_language_marker_inplace(pcgc.name);
  memcpy(&pcgc.config, &this->config, 0x48);
  memcpy(&pcgc.technique_levels, &this->technique_levels, 0x14);
  return pcgc;
}

// creates a player preview, which can then be sent to a BB client for character select
PlayerDispDataBBPreview PlayerDispDataBB::to_preview() const {
  PlayerDispDataBBPreview pre;
  pre.level = this->level;
  pre.experience = this->experience;
  strcpy(pre.guild_card, this->guild_card);
  pre.unknown3[0] = this->unknown3[0];
  pre.unknown3[1] = this->unknown3[1];
  pre.name_color = this->name_color;
  pre.extra_model = this->extra_model;
  memcpy(&pre.unused, &this->unused, 11);
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
  char16cpy(pre.name, this->name, 16);
  pre.play_time = this->play_time;
  return pre;
}

void PlayerDispDataBB::apply_preview(const PlayerDispDataBBPreview& pre) {
  this->level = pre.level;
  this->experience = pre.experience;
  strcpy(this->guild_card, pre.guild_card);
  this->unknown3[0] = pre.unknown3[0];
  this->unknown3[1] = pre.unknown3[1];
  this->name_color = pre.name_color;
  this->extra_model = pre.extra_model;
  memcpy(&this->unused, &pre.unused, 11);
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
  char16cpy(this->name, pre.name, 16);
  this->play_time = 0;
}



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
  /* TODO: fix and re-enable this functionality
  decode_sjis(this->info_board, pc->info_board);
  memcpy(&this->blocked, pc->blocked, sizeof(uint32_t) * 30);
  if (pc->auto_reply_enabled) {
    decode_sjis(this->auto_reply, pc->auto_reply);
  } else {*/
  this->auto_reply[0] = 0;
  //}
}

void Player::import(const PSOPlayerDataGC& gc) {
  this->inventory = gc.inventory;
  this->disp = gc.disp.to_bb();
  decode_sjis(this->info_board, gc.info_board, 0xAC);
  memcpy(&this->blocked, gc.blocked, sizeof(uint32_t) * 30);
  if (gc.auto_reply_enabled) {
    decode_sjis(this->auto_reply, gc.auto_reply, 0xAC);
  } else {
    this->auto_reply[0] = 0;
  }
}

void Player::import(const PSOPlayerDataBB& bb) {
  // note: we don't copy the inventory and disp here because we already have
  // it (we sent the player data to the client in the first place)
  char16cpy(this->info_board, bb.info_board, 0xAC);
  memcpy(&this->blocked, bb.blocked, sizeof(uint32_t) * 30);
  if (bb.auto_reply_enabled) {
    char16cpy(this->auto_reply, bb.auto_reply, 0xAC);
  } else {
    this->auto_reply[0] = 0;
  }
}

// generates data for 65/67/68 commands (joining games/lobbies)
PlayerLobbyJoinDataPCGC Player::export_lobby_data_pc() const {
  PlayerLobbyJoinDataPCGC pc;
  pc.inventory = this->inventory;
  pc.disp = this->disp.to_pcgc();

  // PC has fewer classes, so we'll substitute some here
  if (pc.disp.char_class == 11) {
    pc.disp.char_class = 0; // fomar -> humar
  } else if (pc.disp.char_class == 10) {
    pc.disp.char_class = 1; // ramarl -> hunewearl
  } else if (pc.disp.char_class == 9) {
    pc.disp.char_class = 5; // hucaseal -> racaseal
  }

  // if the player is still not a valid class, make them appear as the "ninja" NPC
  if (pc.disp.char_class > 8) {
    pc.disp.extra_model = 0;
    pc.disp.v2_flags |= 2;
  }
  pc.disp.version = 2;

  return pc;
}

PlayerLobbyJoinDataPCGC Player::export_lobby_data_gc() const {
  PlayerLobbyJoinDataPCGC gc;
  gc.inventory = this->inventory;
  gc.disp = this->disp.to_pcgc();
  return gc;
}

PlayerLobbyJoinDataBB Player::export_lobby_data_bb() const {
  PlayerLobbyJoinDataBB bb;
  bb.inventory = this->inventory;
  bb.disp = this->disp;
  return bb;
}

PlayerBB Player::export_bb_player_data() const {
  PlayerBB bb;
  bb.inventory = this->inventory;
  bb.disp = this->disp;
  memset(bb.unknown, 0, 0x10);
  bb.option_flags = this->option_flags;
  memcpy(bb.quest_data1, &this->quest_data1, 0x0208);
  bb.bank = this->bank;
  bb.serial_number = this->serial_number;
  char16cpy(bb.name, this->disp.name, 24);
  char16cpy(bb.team_name, this->team_name, 16);
  char16cpy(bb.guild_card_desc, this->guild_card_desc, 0x58);
  bb.reserved1 = 0;
  bb.reserved2 = 0;
  bb.section_id = this->disp.section_id;
  bb.char_class = this->disp.char_class;
  bb.unknown3 = 0;
  memcpy(bb.symbol_chats, this->symbol_chats, 0x04E0);
  memcpy(bb.shortcuts, this->shortcuts, 0x0A40);
  char16cpy(bb.auto_reply, this->auto_reply, 0xAC);
  char16cpy(bb.info_board, this->info_board, 0xAC);
  memset(bb.unknown5, 0, 0x1C);
  memcpy(bb.challenge_data, this->challenge_data, 0x0140);
  memcpy(bb.tech_menu_config, this->tech_menu_config, 0x0028);
  memset(bb.unknown6, 0, 0x2C);
  memcpy(bb.quest_data2, &this->quest_data2, 0x0058);
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

  if (strcmp(account.signature, ACCOUNT_FILE_SIGNATURE)) {
    throw runtime_error("account data header is incorrect");
  }

  memcpy(&this->blocked, &account.blocked, sizeof(uint32_t) * 30);
  this->guild_cards = account.guild_cards;
  this->key_config = account.key_config;
  this->option_flags = account.option_flags;
  memcpy(&this->shortcuts, &account.shortcuts, 0x0A40);
  memcpy(&this->symbol_chats, &account.symbol_chats, 0x04E0);
  char16cpy(this->team_name, account.team_name, 16);
}

void Player::save_account_data(const string& filename) const {
  SavedAccountBB account;

  strcpy(account.signature, ACCOUNT_FILE_SIGNATURE);
  memcpy(&account.blocked, &this->blocked, sizeof(uint32_t) * 30);
  account.guild_cards = this->guild_cards;
  account.key_config = this->key_config;
  account.option_flags = this->option_flags;
  memcpy(&account.shortcuts, &this->shortcuts, 0x0A40);
  memcpy(&account.symbol_chats, &this->symbol_chats, 0x04E0);
  char16cpy(account.team_name, this->team_name, 16);

  save_file(filename, &account, sizeof(account));
}

void Player::load_player_data(const string& filename) {
  SavedPlayerBB player = load_object_file<SavedPlayerBB>(filename);

  if (strcmp(player.signature, PLAYER_FILE_SIGNATURE)) {
    throw runtime_error("account data header is incorrect");
  }

  char16cpy(this->auto_reply, player.auto_reply, 0xAC);
  this->bank = player.bank;
  memcpy(&this->challenge_data, &player.challenge_data, 0x0140);
  this->disp = player.disp;
  char16cpy(this->guild_card_desc,player.guild_card_desc, 0x58);
  char16cpy(this->info_board,player.info_board, 0xAC);
  this->inventory = player.inventory;
  memcpy(&this->quest_data1,&player.quest_data1,0x0208);
  memcpy(&this->quest_data2,&player.quest_data2,0x0058);
  memcpy(&this->tech_menu_config,&player.tech_menu_config,0x0028);
}

void Player::save_player_data(const string& filename) const {
  SavedPlayerBB player;

  strcpy(player.signature, PLAYER_FILE_SIGNATURE);
  player.preview = this->disp.to_preview();
  char16cpy(player.auto_reply, this->auto_reply, 0xAC);
  player.bank = this->bank;
  memcpy(&player.challenge_data, &this->challenge_data, 0x0140);
  player.disp = this->disp;
  char16cpy(player.guild_card_desc,this->guild_card_desc, 0x58);
  char16cpy(player.info_board,this->info_board, 0xAC);
  player.inventory = this->inventory;
  memcpy(&player.quest_data1,&this->quest_data1,0x0208);
  memcpy(&player.quest_data2,&this->quest_data2,0x0058);
  memcpy(&player.tech_menu_config,&this->tech_menu_config,0x0028);

  save_file(filename, &player, sizeof(player));
}



////////////////////////////////////////////////////////////////////////////////

static const unordered_map<uint32_t, uint32_t> combine_item_to_max({
  {0x030000, 10},
  {0x030001, 10},
  {0x030002, 10},
  {0x030100, 10},
  {0x030101, 10},
  {0x030102, 10},
  {0x030300, 10},
  {0x030400, 10},
  {0x030500, 10},
  {0x030600, 10},
  {0x030601, 10},
  {0x030700, 10},
  {0x030800, 10},
  {0x031000, 99},
  {0x031001, 99},
  {0x031002, 99},
});

const uint32_t meseta_identifier = 0x00000004;

uint32_t ItemData::primary_identifier() const {
  return (this->item_data1[0] << 16) || (this->item_data1[1] << 8) | this->item_data1[2];
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

void Player::remove_item(uint32_t item_id, uint32_t amount,
    PlayerInventoryItem* item) {

  // are we removing meseta? then create a meseta item
  if (item_id == 0xFFFFFFFF) {
    if (amount > this->disp.meseta) {
      throw out_of_range("player does not have enough meseta");
    }
    if (item) {
      memset(item, 0, sizeof(*item));
      item->data.item_data1[0] = 0x04;
      item->data.item_data2d = amount;
    }
    this->disp.meseta -= amount;
    return;
  }

  // find this item
  size_t index = this->inventory.find_item(item_id);
  auto& inventory_item = this->inventory.items[index];

  // is it a combine item, and are we removing less than we have of it?
  // (amount == 0 means remove all of it)
  if (amount && (amount < inventory_item.data.item_data1[5]) &&
      combine_item_to_max.count(inventory_item.data.primary_identifier())) {
    if (item) {
      *item = inventory_item;
      item->data.item_data1[5] = amount;
      item->data.item_id = 0xFFFFFFFF;
    }
    inventory_item.data.item_data1[5] -= amount;
    return;
  }

  // not a combine item, or we're removing the whole stack? then just remove the item
  if (item) {
    *item = inventory_item;
  }
  this->inventory.num_items--;
  memcpy(&this->inventory.items[index], &this->inventory.items[index + 1],
      sizeof(PlayerInventoryItem) * (this->inventory.num_items - index));
}

// removes an item from a bank. works just like RemoveItem for inventories; I won't comment it
void PlayerBank::remove_item(uint32_t item_id, uint32_t amount,
    PlayerBankItem* item) {

  // are we removing meseta? then create a meseta item
  if (item_id == 0xFFFFFFFF) {
    if (amount > this->meseta) {
      throw out_of_range("player does not have enough meseta");
    }
    if (item) {
      memset(item, 0, sizeof(*item));
      item->data.item_data1[0] = 0x04;
      item->data.item_data2d = amount;
    }
    this->meseta -= amount;
    return;
  }

  // find this item
  size_t index = this->find_item(item_id);
  auto& bank_item = this->items[index];

  // is it a combine item, and are we removing less than we have of it?
  // (amount == 0 means remove all of it)
  if (amount && (amount < bank_item.data.item_data1[5]) &&
      combine_item_to_max.count(bank_item.data.primary_identifier())) {
    if (item) {
      *item = bank_item;
      item->data.item_data1[5] = amount;
      item->amount = amount;
    }
    bank_item.data.item_data1[5] -= amount;
    bank_item.amount -= amount;
    return;
  }

  // not a combine item, or we're removing the whole stack? then just remove the item
  if (item) {
    *item = bank_item;
  }
  this->num_items--;
  memcpy(&this->items[index], &this->items[index + 1],
      sizeof(PlayerBankItem) * (this->num_items - index));
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

string filename_for_player_bb(const string& username, uint8_t player_index) {
  return string_printf("system/players/player_%s_%ld.nsc", username.c_str(),
      player_index + 1);
}

string filename_for_bank_bb(const string& username, const char* bank_name) {
  return string_printf("system/players/bank_%s_%s.nsb", username.c_str(),
      bank_name);
}

string filename_for_class_template_bb(uint8_t char_class) {
  return string_printf("system/blueburst/player_class_%hhu.nsc", char_class);
}

string filename_for_account_bb(const string& username) {
  return string_printf("system/players/account_%s.nsa", username.c_str());
}
