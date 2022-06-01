#include "ReceiveSubcommands.hh"

#include <string.h>

#include <memory>
#include <phosg/Strings.hh>

#include "Client.hh"
#include "Lobby.hh"
#include "Player.hh"
#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "Text.hh"
#include "Items.hh"

using namespace std;

// The functions in this file are called when a client sends a game command
// (60, 62, 6C, or 6D).



bool command_is_private(uint8_t command) {
  // TODO: are either of the Ep3 commands (C9/CB) private? Looks like not...
  return (command == 0x62) || (command == 0x6D);
}



template <typename CmdT = PSOSubcommand>
const CmdT* check_size_sc(
    const string& data,
    size_t min_size = sizeof(CmdT),
    size_t max_size = sizeof(CmdT),
    bool check_size_field = true) {
  if (max_size < min_size) {
    max_size = min_size;
  }
  const auto* cmd = &check_size_t<CmdT>(data, min_size, max_size);
  if (check_size_field && (cmd->size != data.size() / 4)) {
    throw runtime_error("invalid subcommand size field");
  }
  return cmd;
}

template <>
const PSOSubcommand* check_size_sc<PSOSubcommand>(
    const string& data, size_t min_size, size_t max_size, bool check_size_field) {
  if (max_size < min_size) {
    max_size = min_size;
  }
  const auto* ret = &check_size_t<PSOSubcommand>(data, min_size, max_size);
  if (check_size_field && (ret[0].byte[1] != data.size() / 4)) {
    throw runtime_error("invalid subcommand size field");
  }
  return ret;
}



static void forward_subcommand(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint8_t command, uint8_t flag, const void* data, size_t size) {

  // if the command is an Ep3-only command, make sure an Ep3 client sent it
  bool command_is_ep3 = (command & 0xF0) == 0xC0;
  if (command_is_ep3 && !(c->flags & Client::Flag::EPISODE_3)) {
    return;
  }

  if (command_is_private(command)) {
    if (flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[flag];
    if (!target) {
      return;
    }
    if (command_is_ep3 && !(target->flags & Client::Flag::EPISODE_3)) {
      return;
    }
    send_command(target, command, flag, data, size);

  } else {
    if (command_is_ep3) {
      for (auto& target : l->clients) {
        if (!target || (target == c) || !(target->flags & Client::Flag::EPISODE_3)) {
          continue;
        }
        send_command(target, command, flag, data, size);
      }

    } else {
      send_command_excluding_client(l, c, command, flag, data, size);
    }
  }
}

static void forward_subcommand(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint8_t command, uint8_t flag, const string& data) {
  forward_subcommand(l, c, command, flag, data.data(), data.size());
}



////////////////////////////////////////////////////////////////////////////////
// Chat commands and the like

static void process_subcommand_send_guild_card(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!command_is_private(command) || !l || (flag >= l->max_clients) ||
      (!l->clients[flag])) {
    return;
  }

  if (c->version == GameVersion::PC) {
    const auto* cmd = check_size_sc<G_SendGuildCard_PC_6x06>(data);
    c->game_data.player()->guild_card_desc = cmd->desc;
  } else if (c->version == GameVersion::GC) {
    const auto* cmd = check_size_sc<G_SendGuildCard_GC_6x06>(data);
    c->game_data.player()->guild_card_desc = cmd->desc;
  } else if (c->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_SendGuildCard_BB_6x06>(data);
    c->game_data.player()->guild_card_desc = cmd->desc;
  }

  send_guild_card(l->clients[flag], c);
}

// client sends a symbol chat
static void process_subcommand_symbol_chat(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, 0x08, 0xFFFF);

  if (!c->can_chat || (p[1].byte[0] != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

// client sends a word select chat
static void process_subcommand_word_select(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, 0x20, 0xFFFF);

  if (!c->can_chat || (p[0].byte[2] != c->lobby_client_id)) {
    return;
  }

  // TODO: bring this back if it turns out to be important; I suspect it's not
  //p->byte[2] = p->byte[3] = p->byte[(c->version == GameVersion::BB) ? 2 : 3];

  for (size_t x = 1; x < 8; x++) {
    if ((p[x].word[0] > 0x1863) && (p[x].word[0] != 0xFFFF)) {
      return;
    }
    if ((p[x].word[1] > 0x1863) && (p[x].word[1] != 0xFFFF)) {
      return;
    }
  }
  forward_subcommand(l, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Game commands used by cheat mechanisms

// need to process changing areas since we keep track of where players are
static void process_subcommand_change_area(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, 0x08, 0xFFFF);
  if (!l->is_game()) {
    return;
  }
  c->area = p[1].dword;
  forward_subcommand(l, c, command, flag, data);
}

// when a player is hit by an enemy, heal them if infinite HP is enabled
static void process_subcommand_hit_by_enemy(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, 0x04, 0xFFFF);
  if (!l->is_game() || (p->byte[2] != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->infinite_hp) {
    send_player_stats_change(l, c, PlayerStatsChange::ADD_HP, 1020);
  }
}

// when a player casts a tech, restore TP if infinite TP is enabled
static void process_subcommand_use_technique(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, 0x04, 0xFFFF);
  if (!l->is_game() || (p->byte[2] != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->infinite_tp) {
    send_player_stats_change(l, c, PlayerStatsChange::ADD_TP, 255);
  }
}

static void process_subcommand_switch_state_changed(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if (cmd.enabled && cmd.switch_id != 0xFFFF) {
    if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->switch_assist &&
        (c->last_switch_enabled_command.subcommand == 0x05)) {
      log(INFO, "[Switch assist] Replaying previous enable command");
      forward_subcommand(l, c, command, flag, &c->last_switch_enabled_command,
          sizeof(c->last_switch_enabled_command));
      send_command_t(c, command, flag, c->last_switch_enabled_command);
    }
    c->last_switch_enabled_command = cmd;
  }
}

////////////////////////////////////////////////////////////////////////////////

template <typename CmdT>
void process_subcommand_movement(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<CmdT>(data);

  if (cmd->client_id != c->lobby_client_id) {
    return;
  }

  c->x = cmd->x;
  c->z = cmd->z;

  forward_subcommand(l, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Item commands

static void process_subcommand_player_drop_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_PlayerDropItem_6x2A>(data);

  if ((cmd->client_id != c->lobby_client_id)) {
    return;
  }

  l->add_item(c->game_data.player()->remove_item(cmd->item_id, 0),
      cmd->area, cmd->x, cmd->z);

  log(INFO, "[Items/%08" PRIX32 "] Player %hhu dropped item %08" PRIX32 " at %hu:(%g, %g)",
      l->lobby_id, cmd->client_id, cmd->item_id.load(), cmd->area.load(), cmd->x.load(), cmd->z.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_create_inventory_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_PlayerCreateInventoryItem_6x2B>(data);

  if ((cmd->client_id != c->lobby_client_id)) {
    return;
  }
  if (c->version == GameVersion::BB) {
    // BB should never send this command - inventory items should only be
    // created by the server in response to shop buy / bank withdraw / etc. reqs
    return;
  }

  PlayerInventoryItem item;
  item.equip_flags = 0; // TODO: Use the right default flags here
  item.tech_flag = 0;
  item.game_flags = 0;
  item.data = cmd->item;
  c->game_data.player()->add_item(item);

  log(INFO, "[Items/%08" PRIX32 "] Player %hhu created inventory item %08" PRIX32,
      l->lobby_id, cmd->client_id, cmd->item.id.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_drop_partial_stack(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_DropStackedItem_6x5D>(data);

  // TODO: Should we check the client ID here too?
  if (!l->is_game()) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  // TODO: Should we delete anything from the inventory here? Does the client
  // send an appropriate 6x29 alongside this?
  PlayerInventoryItem item;
  item.equip_flags = 0; // TODO: Use the right default flags here
  item.tech_flag = 0;
  item.game_flags = 0;
  item.data = cmd->data;
  l->add_item(item, cmd->area, cmd->x, cmd->z);

  log(INFO, "[Items/%08" PRIX32 "] Player %hhu split stack to create ground item %08" PRIX32 " at %hu:(%g, %g)",
      l->lobby_id, cmd->client_id, item.data.id.load(), cmd->area.load(), cmd->x.load(), cmd->z.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_drop_partial_stack_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_SplitStackedItem_6xC3>(data);

    if (!l->is_game() || (cmd->client_id != c->lobby_client_id)) {
      return;
    }

    auto item = c->game_data.player()->remove_item(cmd->item_id, cmd->amount);

    // if a stack was split, the original item still exists, so the dropped item
    // needs a new ID. remove_item signals this by returning an item with id=-1
    if (item.data.id == 0xFFFFFFFF) {
      item.data.id = l->generate_item_id(c->lobby_client_id);
    }

    // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add
    // the item back to the player's inventory to correct for this (it will get
    // removed again by the 6x29 handler)
    c->game_data.player()->add_item(item);

    l->add_item(item, cmd->area, cmd->x, cmd->z);

    log(INFO, "[Items/%08" PRIX32 "/BB] Player %hhu split stack %08" PRIX32 " (%" PRIu32 " of them) at %hu:(%g, %g)",
        l->lobby_id, cmd->client_id, cmd->item_id.load(), cmd->amount.load(),
        cmd->area.load(), cmd->x.load(), cmd->z.load());
    c->game_data.player()->print_inventory(stderr);

    send_drop_stacked_item(l, item.data, cmd->area, cmd->x, cmd->z);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void process_subcommand_buy_shop_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_BuyShopItem_6x5E>(data);

  if (!l->is_game() || (cmd->client_id != c->lobby_client_id)) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  PlayerInventoryItem item;
  item.equip_flags = 0; // TODO: Use the right default flags here
  item.tech_flag = 0;
  item.game_flags = 0;
  item.data = cmd->item;
  c->game_data.player()->add_item(item);

  log(INFO, "[Items/%08" PRIX32 "] Player %hhu bought item %08" PRIX32 " from shop",
      l->lobby_id, cmd->client_id, item.data.id.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_box_or_enemy_item_drop(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_DropItem_6x5F>(data);

  if (!l->is_game() || (c->lobby_client_id != l->leader_id)) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  PlayerInventoryItem item;
  item.equip_flags = 0; // TODO: Use the right default flags here
  item.tech_flag = 0;
  item.game_flags = 0;
  item.data = cmd->data;
  l->add_item(item, cmd->area, cmd->x, cmd->z);

  log(INFO, "[Items/%08" PRIX32 "] Leader created ground item %08" PRIX32 " at %hhu:(%g, %g)",
      l->lobby_id, item.data.id.load(), cmd->area, cmd->x.load(), cmd->z.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}


static void process_subcommand_pick_up_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  auto* cmd = check_size_sc<G_PickUpItem_6x59>(data);

  if (!l->is_game()) {
    return;
  }
  if (l->version == GameVersion::BB) {
    // BB clients should never send this; only the server should send this
    return;
  }
  c->game_data.player()->add_item(l->remove_item(cmd->item_id));

  log(INFO, "[Items/%08" PRIX32 "] Player %hu picked up %08" PRIX32,
      l->lobby_id, cmd->client_id.load(), cmd->item_id.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_pick_up_item_request(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  // This is handled by the server on BB, and by the leader on other versions
  if (l->version == GameVersion::BB) {
    auto* cmd = check_size_sc<G_PickUpItemRequest_6x5A>(data);

    if (!l->is_game() || (cmd->client_id != c->lobby_client_id)) {
      return;
    }

    c->game_data.player()->add_item(l->remove_item(cmd->item_id));

    log(INFO, "[Items/%08" PRIX32 "/BB] Player %hhu picked up %08" PRIX32,
        l->lobby_id, cmd->client_id, cmd->item_id.load());
    c->game_data.player()->print_inventory(stderr);

    send_pick_up_item(l, c, cmd->item_id, cmd->area);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void process_subcommand_equip_unequip_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  // We don't track equip state on non-BB versions
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_ItemSubcommand>(data);

    if ((cmd->client_id != c->lobby_client_id)) {
      return;
    }

    size_t index = c->game_data.player()->inventory.find_item(cmd->item_id);
    if (cmd->command == 0x25) {
      c->game_data.player()->inventory.items[index].game_flags |= 0x00000008; // equip
    } else {
      c->game_data.player()->inventory.items[index].game_flags &= 0xFFFFFFF7; // unequip
    }

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void process_subcommand_use_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_UseItem_6x27>(data);

  if (cmd->client_id != c->lobby_client_id) {
    return;
  }

  size_t index = c->game_data.player()->inventory.find_item(cmd->item_id);
  player_use_item(c, index);

  log(INFO, "[Items/%08" PRIX32 "] Player used item %hhu:%08" PRIX32,
      l->lobby_id, cmd->client_id, cmd->item_id.load());
  c->game_data.player()->print_inventory(stderr);

  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_open_shop_bb_or_unknown_ep3(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->flags & Lobby::Flag::EPISODE_3_ONLY) {
    check_size_sc(data, 0x08, 0xFFFF);
    forward_subcommand(l, c, command, flag, data);

  } else {
    const auto* p = check_size_sc(data, 0x08);
    uint32_t shop_type = p[1].dword;

    if ((l->version == GameVersion::BB) && l->is_game()) {
      size_t num_items = 9 + (rand() % 4);
      c->game_data.shop_contents.clear();
      while (c->game_data.shop_contents.size() < num_items) {
        ItemData item_data;
        if (shop_type == 0) { // tool shop
          item_data = s->common_item_creator->create_shop_item(l->difficulty, 3);
        } else if (shop_type == 1) { // weapon shop
          item_data = s->common_item_creator->create_shop_item(l->difficulty, 0);
        } else if (shop_type == 2) { // guards shop
          item_data = s->common_item_creator->create_shop_item(l->difficulty, 1);
        } else { // unknown shop... just leave it blank I guess
          break;
        }

        item_data.id = l->generate_item_id(c->lobby_client_id);
        c->game_data.shop_contents.emplace_back(item_data);
      }

      send_shop(c, shop_type);
    }
  }
}

static void process_subcommand_open_bank_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t, const string&) {
  if ((l->version == GameVersion::BB) && l->is_game()) {
    send_bank(c);
  }
}

static void process_subcommand_bank_action_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t, const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_BankAction_BB_6xBD>(data);

    if (!l->is_game()) {
      return;
    }

    if (cmd->action == 0) { // deposit
      if (cmd->item_id == 0xFFFFFFFF) { // meseta
        if (cmd->meseta_amount > c->game_data.player()->disp.meseta) {
          return;
        }
        if ((c->game_data.player()->bank.meseta + cmd->meseta_amount) > 999999) {
          return;
        }
        c->game_data.player()->bank.meseta += cmd->meseta_amount;
        c->game_data.player()->disp.meseta -= cmd->meseta_amount;
      } else { // item
        auto item = c->game_data.player()->remove_item(cmd->item_id, cmd->item_amount);
        c->game_data.player()->bank.add_item(item);
        send_destroy_item(l, c, cmd->item_id, cmd->item_amount);
      }
    } else if (cmd->action == 1) { // take
      if (cmd->item_id == 0xFFFFFFFF) { // meseta
        if (cmd->meseta_amount > c->game_data.player()->bank.meseta) {
          return;
        }
        if ((c->game_data.player()->disp.meseta + cmd->meseta_amount) > 999999) {
          return;
        }
        c->game_data.player()->bank.meseta -= cmd->meseta_amount;
        c->game_data.player()->disp.meseta += cmd->meseta_amount;
      } else { // item
        auto bank_item = c->game_data.player()->bank.remove_item(cmd->item_id, cmd->item_amount);
        PlayerInventoryItem item = bank_item;
        item.data.id = l->generate_item_id(0xFF);
        c->game_data.player()->add_item(item);
        send_create_inventory_item(l, c, item.data);
      }
    }
  }
}

// player sorts the items in their inventory
static void process_subcommand_sort_inventory_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_SortInventory_6xC4>(data);

    PlayerInventory sorted;

    for (size_t x = 0; x < 30; x++) {
      if (cmd->item_ids[x] == 0xFFFFFFFF) {
        sorted.items[x].data.id = 0xFFFFFFFF;
      } else {
        size_t index = c->game_data.player()->inventory.find_item(cmd->item_ids[x]);
        sorted.items[x] = c->game_data.player()->inventory.items[index];
      }
    }

    sorted.num_items = c->game_data.player()->inventory.num_items;
    sorted.hp_materials_used = c->game_data.player()->inventory.hp_materials_used;
    sorted.tp_materials_used = c->game_data.player()->inventory.tp_materials_used;
    sorted.language = c->game_data.player()->inventory.language;
    c->game_data.player()->inventory = sorted;
  }
}

////////////////////////////////////////////////////////////////////////////////
// EXP/Drop Item commands

static void process_subcommand_enemy_drop_item_request(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_EnemyDropItemRequest_6x60>(data);

    if (!l->is_game()) {
      return;
    }

    PlayerInventoryItem item;

    // TODO: Deduplicate this code with the box drop item request handler
    bool is_rare = false;
    if (l->next_drop_item.data.data1d[0]) {
      item = l->next_drop_item;
      l->next_drop_item.data.data1d[0] = 0;
    } else {
      if (l->rare_item_set) {
        if (cmd->enemy_id <= 0x65) {
          is_rare = sample_rare_item(l->rare_item_set->rares[cmd->enemy_id].probability);
        }
      }

      if (is_rare) {
        const auto& code = l->rare_item_set->rares[cmd->enemy_id].item_code;
        item.data.data1[0] = code[0];
        item.data.data1[1] = code[1];
        item.data.data1[2] = code[2];
        //RandPercentages();
        if (item.data.data1d[0] == 0) {
          item.data.data1[4] |= 0x80; // make it unidentified if it's a weapon
        }
      } else {
        try {
          item.data = s->common_item_creator->create_drop_item(false, l->episode,
              l->difficulty, cmd->area, l->section_id);
        } catch (const out_of_range&) {
          // create_common_item throws this when it doesn't want to make an item
          return;
        }
      }
    }
    item.data.id = l->generate_item_id(0xFF);

    l->add_item(item, cmd->area, cmd->x, cmd->z);
    send_drop_item(l, item.data, false, cmd->area, cmd->x, cmd->z,
        cmd->request_id);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void process_subcommand_box_drop_item_request(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_BoxItemDropRequest_6xA2>(data);

    if (!l->is_game()) {
      return;
    }

    PlayerInventoryItem item;

    bool is_rare = false;
    if (l->next_drop_item.data.data1d[0]) {
      item = l->next_drop_item;
      l->next_drop_item.data.data1d[0] = 0;
    } else {
      size_t index;
      if (l->rare_item_set) {
        for (index = 0; index < 30; index++) {
          if (l->rare_item_set->box_areas[index] != cmd->area) {
            continue;
          }
          if (sample_rare_item(l->rare_item_set->box_rares[index].probability)) {
            is_rare = true;
            break;
          }
        }
      }

      if (is_rare) {
        const auto& code = l->rare_item_set->box_rares[index].item_code;
        item.data.data1[0] = code[0];
        item.data.data1[1] = code[1];
        item.data.data1[2] = code[2];
        //RandPercentages();
        if (item.data.data1d[0] == 0) {
          item.data.data1[4] |= 0x80; // make it unidentified if it's a weapon
        }
      } else {
        try {
          item.data = s->common_item_creator->create_drop_item(true, l->episode,
              l->difficulty, cmd->area, l->section_id);
        } catch (const out_of_range&) {
          // create_common_item throws this when it doesn't want to make an item
          return;
        }
      }
    }
    item.data.id = l->generate_item_id(0xFF);

    l->add_item(item, cmd->area, cmd->x, cmd->z);
    send_drop_item(l, item.data, false, cmd->area, cmd->x, cmd->z,
        cmd->request_id);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void process_subcommand_phase_setup(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);

  bool should_send_boss_drop_req = false;
  if (p[2].dword == l->difficulty) {
    if ((l->episode == 1) && (c->area == 0x0E)) {
      // On Normal, Dark Falz does not have a third phase, so send the drop
      // request after the end of the second phase. On all other difficulty
      // levels, send it after the third phase.
      if (((l->difficulty == 0) && (p[1].dword == 0x00000035)) ||
          ((l->difficulty != 0) && (p[1].dword == 0x00000037))) {
        should_send_boss_drop_req = true;
      }
    } else if ((l->episode == 2) && (p[1].dword == 0x00000057) && (c->area == 0x0D)) {
      should_send_boss_drop_req = true;
    }
  }

  if (should_send_boss_drop_req) {
    auto c = l->clients.at(l->leader_id);
    if (c) {
      G_EnemyDropItemRequest_6x60 req = {
        0x60,
        0x06,
        0x1090,
        static_cast<uint8_t>(c->area),
        static_cast<uint8_t>((l->episode == 2) ? 0x4E : 0x2F),
        0x0B4F,
        (l->episode == 2) ? -9999.0f : 10160.58984375f,
        0.0f,
        0xE0AEDC0100000002,
      };
      send_command_t(c, 0x62, l->leader_id, req);
    }
  }
}

// enemy hit by player
static void process_subcommand_enemy_hit(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_EnemyHitByPlayer_6x0A>(data);

    if (!l->is_game()) {
      return;
    }
    if (cmd->enemy_id >= l->enemies.size()) {
      return;
    }

    if (l->enemies[cmd->enemy_id].hit_flags & 0x80) {
      return;
    }
    l->enemies[cmd->enemy_id].hit_flags |= (1 << c->lobby_client_id);
    l->enemies[cmd->enemy_id].last_hit = c->lobby_client_id;
  }

  forward_subcommand(l, c, command, flag, data);
}

// enemy killed by player
static void process_subcommand_enemy_killed(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  forward_subcommand(l, c, command, flag, data);

  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_EnemyKilled_6xC8>(data);

    if (!l->is_game() || (cmd->enemy_id >= l->enemies.size() ||
        (l->enemies[cmd->enemy_id].hit_flags & 0x80))) {
      return;
    }
    if (l->enemies[cmd->enemy_id].experience == 0xFFFFFFFF) {
      send_text_message(c, u"$C6Unknown enemy type killed");
      return;
    }

    auto& enemy = l->enemies[cmd->enemy_id];
    enemy.hit_flags |= 0x80;
    for (size_t x = 0; x < l->max_clients; x++) {
      if (!((enemy.hit_flags >> x) & 1)) {
        continue; // player did not hit this enemy
      }

      auto other_c = l->clients[x];
      if (!other_c) {
        continue; // no player
      }
      if (other_c->game_data.player()->disp.level >= 199) {
        continue; // player is level 200 or higher
      }

      // killer gets full experience, others get 77%
      uint32_t exp;
      if (enemy.last_hit == other_c->lobby_client_id) {
        exp = enemy.experience;
      } else {
        exp = ((enemy.experience * 77) / 100);
      }

      other_c->game_data.player()->disp.experience += exp;
      send_give_experience(l, other_c, exp);

      bool leveled_up = false;
      do {
        const auto& level = s->level_table->stats_for_level(
            other_c->game_data.player()->disp.char_class, other_c->game_data.player()->disp.level + 1);
        if (other_c->game_data.player()->disp.experience >= level.experience) {
          leveled_up = true;
          level.apply(other_c->game_data.player()->disp.stats);
          other_c->game_data.player()->disp.level++;
        } else {
          break;
        }
      } while (other_c->game_data.player()->disp.level < 199);
      if (leveled_up) {
        send_level_up(l, other_c);
      }
    }
  }
}

static void process_subcommand_destroy_inventory_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_ItemSubcommand>(data);
  if (!l->is_game()) {
    return;
  }
  if (cmd->client_id != c->lobby_client_id) {
    return;
  }
  c->game_data.player()->remove_item(cmd->item_id, cmd->amount);
  log(INFO, "[Items/%08" PRIX32 "] Inventory item %hhu:%08" PRIX32 " destroyed (%" PRIX32 " of them)",
      l->lobby_id, cmd->client_id, cmd->item_id.load(), cmd->amount.load());
  c->game_data.player()->print_inventory(stderr);
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_destroy_ground_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* cmd = check_size_sc<G_ItemSubcommand>(data);
  if (!l->is_game()) {
    return;
  }
  l->remove_item(cmd->item_id);
  log(INFO, "[Items/%08" PRIX32 "] Ground item %08" PRIX32 " destroyed (%" PRIX32 " of them)",
      l->lobby_id, cmd->item_id.load(), cmd->amount.load());
  c->game_data.player()->print_inventory(stderr);
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_identify_item_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto* cmd = check_size_sc<G_ItemSubcommand>(data);
    if (!l->is_game() || (cmd->client_id != c->lobby_client_id)) {
      return;
    }

    size_t x = c->game_data.player()->inventory.find_item(cmd->item_id);
    if (c->game_data.player()->inventory.items[x].data.data1[0] != 0) {
      return; // only weapons can be identified
    }

    c->game_data.player()->disp.meseta -= 100;
    c->game_data.identify_result = c->game_data.player()->inventory.items[x];
    c->game_data.identify_result.data.data1[4] &= 0x7F;

    // TODO: move this into a SendCommands.cc function
    G_IdentifyResult_BB_6xB9 res;
    res.subcommand = 0xB9;
    res.size = sizeof(cmd) / 4;
    res.client_id = c->lobby_client_id;
    res.unused = 0;
    res.item = c->game_data.identify_result.data;
    send_command_t(l, 0x60, 0x00, res);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

// player accepts the tekk
// TODO: I don't know which subcommand id this is; the function should be
// correct though so we can just put it in the table when we figure out the id
// static void process_subcommand_accept_identified_item(shared_ptr<ServerState> s,
//     shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
//     const string& data) {
//
//   if (l->version == GameVersion::BB) {
//     const auto* cmd = check_size_sc<G_ItemSubcommand>(data);
//
//     if (cmd->client_id != c->lobby_client_id) {
//       return;
//     }
//
//     size_t x = c->game_data.player()->inventory.find_item(cmd->item_id);
//     c->game_data.player()->inventory.items[x] = c->game_data.player()->identify_result;
//     // TODO: what do we send to the other clients? anything?
//
//   } else {
//     forward_subcommand(l, c, command, flag, data);
//   }
// }

////////////////////////////////////////////////////////////////////////////////

static void process_subcommand_forward_check_size(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_game_loading(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_size_client(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (p->byte[2] != c->lobby_client_id) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_size_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_size_ep3_lobby(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (l->is_game() || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_forward_check_size_ep3_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (!l->is_game() || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void process_subcommand_invalid(shared_ptr<ServerState>,
    shared_ptr<Lobby>, shared_ptr<Client>, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (command_is_private(command)) {
    log(WARNING, "Invalid subcommand: %02hhX (private to %hhu)", p->byte[0], flag);
  } else {
    log(WARNING, "Invalid subcommand: %02hhX (public)", p->byte[0]);
  }
}

static void process_subcommand_unimplemented(shared_ptr<ServerState>,
    shared_ptr<Lobby>, shared_ptr<Client>, uint8_t command, uint8_t flag,
    const string& data) {
  const auto* p = check_size_sc(data, sizeof(PSOSubcommand), 0xFFFF);
  if (command_is_private(command)) {
    log(WARNING, "Unknown subcommand: %02hhX (private to %hhu)", p->byte[0], flag);
  } else {
    log(WARNING, "Unknown subcommand: %02hhX (public)", p->byte[0]);
  }
}

////////////////////////////////////////////////////////////////////////////////

// Subcommands are described by four fields: the minimum size and maximum size (in DWORDs),
// the handler function, and flags that tell when to allow the command. See command-input-subs.h
// for more information on flags. The maximum size is not enforced if it's zero.
typedef void (*subcommand_handler_t)(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data);

subcommand_handler_t subcommand_handlers[0x100] = {
  /* 00 */ process_subcommand_invalid,
  /* 01 */ process_subcommand_unimplemented,
  /* 02 */ process_subcommand_unimplemented,
  /* 03 */ process_subcommand_unimplemented,
  /* 04 */ process_subcommand_unimplemented,
  /* 05 */ process_subcommand_switch_state_changed,
  /* 06 */ process_subcommand_send_guild_card,
  /* 07 */ process_subcommand_symbol_chat,
  /* 08 */ process_subcommand_unimplemented,
  /* 09 */ process_subcommand_unimplemented,
  /* 0A */ process_subcommand_enemy_hit,
  /* 0B */ process_subcommand_forward_check_size_game,
  /* 0C */ process_subcommand_forward_check_size_game, // Add condition (poison/slow/etc.)
  /* 0D */ process_subcommand_forward_check_size_game, // Remove condition (poison/slow/etc.)
  /* 0E */ process_subcommand_unimplemented,
  /* 0F */ process_subcommand_unimplemented,
  /* 10 */ process_subcommand_unimplemented,
  /* 11 */ process_subcommand_unimplemented,
  /* 12 */ process_subcommand_forward_check_size_game, // Dragon actions
  /* 13 */ process_subcommand_forward_check_size_game, // De Rol Le actions
  /* 14 */ process_subcommand_forward_check_size_game,
  /* 15 */ process_subcommand_forward_check_size_game, // Vol Opt actions
  /* 16 */ process_subcommand_forward_check_size_game, // Vol Opt actions
  /* 17 */ process_subcommand_forward_check_size_game,
  /* 18 */ process_subcommand_forward_check_size_game,
  /* 19 */ process_subcommand_forward_check_size_game, // Dark Falz actions
  /* 1A */ process_subcommand_unimplemented,
  /* 1B */ process_subcommand_unimplemented,
  /* 1C */ process_subcommand_forward_check_size_game,
  /* 1D */ process_subcommand_unimplemented,
  /* 1E */ process_subcommand_unimplemented,
  /* 1F */ process_subcommand_forward_check_size,
  /* 20 */ process_subcommand_forward_check_size,
  /* 21 */ process_subcommand_change_area, // Inter-level warp
  /* 22 */ process_subcommand_forward_check_size_client, // Set player visibility
  /* 23 */ process_subcommand_forward_check_size_client, // Set player visibility
  /* 24 */ process_subcommand_forward_check_size_game,
  /* 25 */ process_subcommand_equip_unequip_item, // Equip item
  /* 26 */ process_subcommand_equip_unequip_item, // Unequip item
  /* 27 */ process_subcommand_use_item,
  /* 28 */ process_subcommand_forward_check_size_game, // Feed MAG
  /* 29 */ process_subcommand_destroy_inventory_item, // Delete item (via bank deposit / sale / feeding MAG)
  /* 2A */ process_subcommand_player_drop_item,
  /* 2B */ process_subcommand_create_inventory_item, // Create inventory item (e.g. from tekker or bank withdrawal)
  /* 2C */ process_subcommand_forward_check_size, // Talk to NPC
  /* 2D */ process_subcommand_forward_check_size, // Done talking to NPC
  /* 2E */ process_subcommand_unimplemented,
  /* 2F */ process_subcommand_hit_by_enemy,
  /* 30 */ process_subcommand_forward_check_size_game, // Level up
  /* 31 */ process_subcommand_forward_check_size_game, // Medical center
  /* 32 */ process_subcommand_forward_check_size_game, // Medical center
  /* 33 */ process_subcommand_forward_check_size_game, // Revive player (only confirmed with moon atomizer)
  /* 34 */ process_subcommand_unimplemented,
  /* 35 */ process_subcommand_unimplemented,
  /* 36 */ process_subcommand_forward_check_game,
  /* 37 */ process_subcommand_forward_check_size_game, // Photon blast
  /* 38 */ process_subcommand_unimplemented,
  /* 39 */ process_subcommand_forward_check_size_game, // Photon blast ready
  /* 3A */ process_subcommand_forward_check_size_game,
  /* 3B */ process_subcommand_forward_check_size,
  /* 3C */ process_subcommand_unimplemented,
  /* 3D */ process_subcommand_unimplemented,
  /* 3E */ process_subcommand_movement<G_StopAtPosition_6x3E>, // Stop moving
  /* 3F */ process_subcommand_movement<G_SetPosition_6x3F>, // Set position (e.g. when materializing after warp)
  /* 40 */ process_subcommand_movement<G_WalkToPosition_6x40>, // Walk
  /* 41 */ process_subcommand_unimplemented,
  /* 42 */ process_subcommand_movement<G_RunToPosition_6x42>, // Run
  /* 43 */ process_subcommand_forward_check_size_client,
  /* 44 */ process_subcommand_forward_check_size_client,
  /* 45 */ process_subcommand_forward_check_size_client,
  /* 46 */ process_subcommand_forward_check_size_client,
  /* 47 */ process_subcommand_forward_check_size_client,
  /* 48 */ process_subcommand_use_technique,
  /* 49 */ process_subcommand_forward_check_size_client,
  /* 4A */ process_subcommand_forward_check_size_client,
  /* 4B */ process_subcommand_hit_by_enemy,
  /* 4C */ process_subcommand_hit_by_enemy,
  /* 4D */ process_subcommand_forward_check_size_client,
  /* 4E */ process_subcommand_forward_check_size_client,
  /* 4F */ process_subcommand_forward_check_size_client,
  /* 50 */ process_subcommand_forward_check_size_client,
  /* 51 */ process_subcommand_unimplemented,
  /* 52 */ process_subcommand_forward_check_size, // Toggle shop/bank interaction
  /* 53 */ process_subcommand_forward_check_size_game,
  /* 54 */ process_subcommand_unimplemented,
  /* 55 */ process_subcommand_forward_check_size_client, // Intra-map warp
  /* 56 */ process_subcommand_forward_check_size_client,
  /* 57 */ process_subcommand_forward_check_size_client,
  /* 58 */ process_subcommand_forward_check_size_game,
  /* 59 */ process_subcommand_pick_up_item, // Item picked up
  /* 5A */ process_subcommand_pick_up_item_request, // Request to pick up item
  /* 5B */ process_subcommand_unimplemented,
  /* 5C */ process_subcommand_unimplemented,
  /* 5D */ process_subcommand_drop_partial_stack, // Drop meseta or stacked item
  /* 5E */ process_subcommand_buy_shop_item, // Buy item at shop
  /* 5F */ process_subcommand_box_or_enemy_item_drop, // Drop item from box/enemy
  /* 60 */ process_subcommand_enemy_drop_item_request, // Request for item drop (handled by the server on BB)
  /* 61 */ process_subcommand_forward_check_size_game, // Feed mag
  /* 62 */ process_subcommand_unimplemented,
  /* 63 */ process_subcommand_destroy_ground_item, // Destroy an item on the ground (used when too many items have been dropped)
  /* 64 */ process_subcommand_unimplemented,
  /* 65 */ process_subcommand_unimplemented,
  /* 66 */ process_subcommand_forward_check_size_game, // Use star atomizer
  /* 67 */ process_subcommand_forward_check_size_game, // Create enemy set
  /* 68 */ process_subcommand_forward_check_size_game, // Telepipe/Ryuker
  /* 69 */ process_subcommand_forward_check_size_game,
  /* 6A */ process_subcommand_forward_check_size_game,
  /* 6B */ process_subcommand_forward_check_game_loading,
  /* 6C */ process_subcommand_forward_check_game_loading,
  /* 6D */ process_subcommand_forward_check_game_loading,
  /* 6E */ process_subcommand_forward_check_game_loading,
  /* 6F */ process_subcommand_forward_check_game_loading,
  /* 70 */ process_subcommand_forward_check_game_loading,
  /* 71 */ process_subcommand_forward_check_game_loading,
  /* 72 */ process_subcommand_forward_check_game_loading,
  /* 73 */ process_subcommand_invalid,
  /* 74 */ process_subcommand_word_select,
  /* 75 */ process_subcommand_phase_setup,
  /* 76 */ process_subcommand_forward_check_size_game, // Enemy killed
  /* 77 */ process_subcommand_forward_check_size_game, // Sync quest data
  /* 78 */ process_subcommand_unimplemented,
  /* 79 */ process_subcommand_forward_check_size, // Lobby 14/15 soccer game
  /* 7A */ process_subcommand_unimplemented,
  /* 7B */ process_subcommand_unimplemented,
  /* 7C */ process_subcommand_forward_check_size_game,
  /* 7D */ process_subcommand_forward_check_size_game,
  /* 7E */ process_subcommand_unimplemented,
  /* 7F */ process_subcommand_unimplemented,
  /* 80 */ process_subcommand_forward_check_size_game, // Trigger trap
  /* 81 */ process_subcommand_unimplemented,
  /* 82 */ process_subcommand_unimplemented,
  /* 83 */ process_subcommand_forward_check_size_game, // Place trap
  /* 84 */ process_subcommand_forward_check_size_game,
  /* 85 */ process_subcommand_forward_check_size_game,
  /* 86 */ process_subcommand_forward_check_size_game, // Hit destructible wall
  /* 87 */ process_subcommand_unimplemented,
  /* 88 */ process_subcommand_forward_check_size_game,
  /* 89 */ process_subcommand_forward_check_size_game,
  /* 8A */ process_subcommand_unimplemented,
  /* 8B */ process_subcommand_unimplemented,
  /* 8C */ process_subcommand_unimplemented,
  /* 8D */ process_subcommand_forward_check_size_client,
  /* 8E */ process_subcommand_unimplemented,
  /* 8F */ process_subcommand_unimplemented,
  /* 90 */ process_subcommand_unimplemented,
  /* 91 */ process_subcommand_forward_check_size_game,
  /* 92 */ process_subcommand_unimplemented,
  /* 93 */ process_subcommand_forward_check_size_game, // Timed switch activated
  /* 94 */ process_subcommand_forward_check_size_game, // Warp (the $warp chat command is implemented using this)
  /* 95 */ process_subcommand_unimplemented,
  /* 96 */ process_subcommand_unimplemented,
  /* 97 */ process_subcommand_unimplemented,
  /* 98 */ process_subcommand_unimplemented,
  /* 99 */ process_subcommand_unimplemented,
  /* 9A */ process_subcommand_forward_check_size_game, // Update player stat ($infhp/$inftp are implemented using this command)
  /* 9B */ process_subcommand_unimplemented,
  /* 9C */ process_subcommand_forward_check_size_game,
  /* 9D */ process_subcommand_unimplemented,
  /* 9E */ process_subcommand_unimplemented,
  /* 9F */ process_subcommand_forward_check_size_game, // Gal Gryphon actions
  /* A0 */ process_subcommand_forward_check_size_game, // Gal Gryphon actions
  /* A1 */ process_subcommand_unimplemented,
  /* A2 */ process_subcommand_box_drop_item_request, // Request for item drop from box (handled by server on BB)
  /* A3 */ process_subcommand_forward_check_size_game, // Episode 2 boss actions
  /* A4 */ process_subcommand_forward_check_size_game, // Olga Flow phase 1 actions
  /* A5 */ process_subcommand_forward_check_size_game, // Olga Flow phase 2 actions
  /* A6 */ process_subcommand_forward_check_size, // Trade proposal
  /* A7 */ process_subcommand_unimplemented,
  /* A8 */ process_subcommand_forward_check_size_game, // Gol Dragon actions
  /* A9 */ process_subcommand_forward_check_size_game, // Barba Ray actions
  /* AA */ process_subcommand_forward_check_size_game, // Episode 2 boss actions
  /* AB */ process_subcommand_forward_check_size_client, // Create lobby chair
  /* AC */ process_subcommand_unimplemented,
  /* AD */ process_subcommand_forward_check_size_game, // Olga Flow phase 2 subordinate boss actions
  /* AE */ process_subcommand_forward_check_size_client,
  /* AF */ process_subcommand_forward_check_size_client, // Turn in lobby chair
  /* B0 */ process_subcommand_forward_check_size_client, // Move in lobby chair
  /* B1 */ process_subcommand_unimplemented,
  /* B2 */ process_subcommand_unimplemented,
  /* B3 */ process_subcommand_unimplemented,
  /* B4 */ process_subcommand_forward_check_size_ep3_game,
  /* B5 */ process_subcommand_open_shop_bb_or_unknown_ep3, // BB shop request
  /* B6 */ process_subcommand_unimplemented, // BB shop contents (server->client only)
  /* B7 */ process_subcommand_unimplemented, // TODO: BB buy shop item
  /* B8 */ process_subcommand_identify_item_bb, // Accept tekker result
  /* B9 */ process_subcommand_unimplemented,
  /* BA */ process_subcommand_unimplemented,
  /* BB */ process_subcommand_open_bank_bb, // BB Bank request
  /* BC */ process_subcommand_unimplemented, // BB bank contents (server->client only)
  /* BD */ process_subcommand_bank_action_bb,
  /* BE */ process_subcommand_unimplemented, // BB create inventory item (server->client only)
  /* BF */ process_subcommand_forward_check_size_ep3_lobby, // Ep3 change music, also BB give EXP (BB usage is server->client only)
  /* C0 */ process_subcommand_unimplemented,
  /* C1 */ process_subcommand_unimplemented,
  /* C2 */ process_subcommand_unimplemented,
  /* C3 */ process_subcommand_drop_partial_stack_bb, // Split stacked item - not sent if entire stack is dropped
  /* C4 */ process_subcommand_sort_inventory_bb,
  /* C5 */ process_subcommand_unimplemented,
  /* C6 */ process_subcommand_unimplemented,
  /* C7 */ process_subcommand_unimplemented,
  /* C8 */ process_subcommand_enemy_killed,
  /* C9 */ process_subcommand_unimplemented,
  /* CA */ process_subcommand_unimplemented,
  /* CB */ process_subcommand_unimplemented,
  /* CC */ process_subcommand_unimplemented,
  /* CD */ process_subcommand_unimplemented,
  /* CE */ process_subcommand_unimplemented,
  /* CF */ process_subcommand_forward_check_size_game,
  /* D0 */ process_subcommand_unimplemented,
  /* D1 */ process_subcommand_unimplemented,
  /* D2 */ process_subcommand_unimplemented,
  /* D3 */ process_subcommand_unimplemented,
  /* D4 */ process_subcommand_unimplemented,
  /* D5 */ process_subcommand_unimplemented,
  /* D6 */ process_subcommand_unimplemented,
  /* D7 */ process_subcommand_unimplemented,
  /* D8 */ process_subcommand_unimplemented,
  /* D9 */ process_subcommand_unimplemented,
  /* DA */ process_subcommand_unimplemented,
  /* DB */ process_subcommand_unimplemented,
  /* DC */ process_subcommand_unimplemented,
  /* DD */ process_subcommand_unimplemented,
  /* DE */ process_subcommand_unimplemented,
  /* DF */ process_subcommand_unimplemented,
  /* E0 */ process_subcommand_unimplemented,
  /* E1 */ process_subcommand_unimplemented,
  /* E2 */ process_subcommand_unimplemented,
  /* E3 */ process_subcommand_unimplemented,
  /* E4 */ process_subcommand_unimplemented,
  /* E5 */ process_subcommand_unimplemented,
  /* E6 */ process_subcommand_unimplemented,
  /* E7 */ process_subcommand_unimplemented,
  /* E8 */ process_subcommand_unimplemented,
  /* E9 */ process_subcommand_unimplemented,
  /* EA */ process_subcommand_unimplemented,
  /* EB */ process_subcommand_unimplemented,
  /* EC */ process_subcommand_unimplemented,
  /* ED */ process_subcommand_unimplemented,
  /* EE */ process_subcommand_unimplemented,
  /* EF */ process_subcommand_unimplemented,
  /* F0 */ process_subcommand_unimplemented,
  /* F1 */ process_subcommand_unimplemented,
  /* F2 */ process_subcommand_unimplemented,
  /* F3 */ process_subcommand_unimplemented,
  /* F4 */ process_subcommand_unimplemented,
  /* F5 */ process_subcommand_unimplemented,
  /* F6 */ process_subcommand_unimplemented,
  /* F7 */ process_subcommand_unimplemented,
  /* F8 */ process_subcommand_unimplemented,
  /* F9 */ process_subcommand_unimplemented,
  /* FA */ process_subcommand_unimplemented,
  /* FB */ process_subcommand_unimplemented,
  /* FC */ process_subcommand_unimplemented,
  /* FD */ process_subcommand_unimplemented,
  /* FE */ process_subcommand_unimplemented,
  /* FF */ process_subcommand_unimplemented,
};

void process_subcommand(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {
  if (data.empty()) {
    throw runtime_error("game command is empty");
  }
  uint8_t which = static_cast<uint8_t>(data[0]);
  subcommand_handlers[which](s, l, c, command, flag, data);
}

bool subcommand_is_implemented(uint8_t which) {
  return subcommand_handlers[which] != process_subcommand_unimplemented;
}
