#include "ReceiveSubcommands.hh"

#include <string.h>

#include <memory>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "Client.hh"
#include "Compression.hh"
#include "Items.hh"
#include "Lobby.hh"
#include "Loggers.hh"
#include "Map.hh"
#include "PSOProtocol.hh"
#include "Player.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

// The functions in this file are called when a client sends a game command
// (60, 62, 6C, 6D, C9, or CB).

bool command_is_private(uint8_t command) {
  return (command == 0x62) || (command == 0x6D);
}

template <typename CmdT>
const CmdT& check_size_sc(
    const string& data,
    size_t min_size = sizeof(CmdT),
    size_t max_size = sizeof(CmdT),
    bool check_size_field = true) {
  if (max_size < min_size) {
    max_size = min_size;
  }
  const auto& cmd = check_size_t<CmdT>(data, min_size, max_size);
  if (check_size_field) {
    if (data.size() < 4) {
      throw runtime_error("subcommand is too short for header");
    }
    const auto* header = reinterpret_cast<const G_UnusedHeader*>(data.data());
    if (header->size == 0) {
      if (data.size() < 8) {
        throw runtime_error("subcommand has extended size but is shorter than 8 bytes");
      }
      const auto* ext_header = reinterpret_cast<const G_ExtendedHeader<G_UnusedHeader>*>(data.data());
      if (ext_header->size != data.size()) {
        throw runtime_error("invalid subcommand extended size field");
      }
    } else {
      if ((header->size * 4) != data.size()) {
        throw runtime_error("invalid subcommand size field");
      }
    }
  }
  return cmd;
}

static const unordered_set<uint8_t> watcher_subcommands({
    0x07, // Symbol chat
    0x74, // Word select
    0xBD, // Word select during battle (with private_flags)
});

static void forward_subcommand(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint8_t command, uint8_t flag, const void* data, size_t size) {

  // if the command is an Ep3-only command, make sure an Ep3 client sent it
  bool command_is_ep3 = (command & 0xF0) == 0xC0;
  if (command_is_ep3 && !(c->flags & Client::Flag::IS_EPISODE_3)) {
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
    if (command_is_ep3 && !(target->flags & Client::Flag::IS_EPISODE_3)) {
      return;
    }
    send_command(target, command, flag, data, size);

  } else {
    if (command_is_ep3) {
      for (auto& target : l->clients) {
        if (!target || (target == c) || !(target->flags & Client::Flag::IS_EPISODE_3)) {
          continue;
        }
        send_command(target, command, flag, data, size);
      }

    } else {
      send_command_excluding_client(l, c, command, flag, data, size);
    }

    // Before battle, forward only chat commands to watcher lobbies; during
    // battle, forward everything to watcher lobbies.
    if (size &&
        (watcher_subcommands.count(*reinterpret_cast<const uint8_t*>(data) ||
            (l->ep3_server_base &&
                l->ep3_server_base->server->setup_phase != Episode3::SetupPhase::REGISTRATION)))) {
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        forward_subcommand(watcher_lobby, c, command, flag, data, size);
      }
    }

    if (l->battle_record && l->battle_record->battle_in_progress()) {
      auto type = ((command & 0xF0) == 0xC0)
          ? Episode3::BattleRecord::Event::Type::EP3_GAME_COMMAND
          : Episode3::BattleRecord::Event::Type::GAME_COMMAND;
      l->battle_record->add_command(type, data, size);
    }
  }
}

static void forward_subcommand(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint8_t command, uint8_t flag, const string& data) {
  forward_subcommand(l, c, command, flag, data.data(), data.size());
}

static void on_invalid(shared_ptr<ServerState>,
    shared_ptr<Lobby>, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_UnusedHeader>(
      data, sizeof(G_UnusedHeader), 0xFFFF);
  if (command_is_private(command)) {
    c->log.error("Invalid subcommand: %02hhX (private to %hhu)",
        cmd.subcommand, flag);
  } else {
    c->log.error("Invalid subcommand: %02hhX (public)", cmd.subcommand);
  }
}

static void on_unimplemented(shared_ptr<ServerState>,
    shared_ptr<Lobby>, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_UnusedHeader>(
      data, sizeof(G_UnusedHeader), 0xFFFF);
  if (command_is_private(command)) {
    c->log.warning("Unknown subcommand: %02hhX (private to %hhu)",
        cmd.subcommand, flag);
  } else {
    c->log.warning("Unknown subcommand: %02hhX (public)", cmd.subcommand);
  }
  if (c->options.debug) {
    send_text_message_printf(c, "$C5Sub 6x%02hhX missing", cmd.subcommand);
  }
}

static void on_forward_check_size(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc<G_UnusedHeader>(data, sizeof(G_UnusedHeader), 0xFFFF);
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_sync_game_state(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }

  const auto& cmd = check_size_sc<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(
      data, sizeof(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E), 0xFFFF);
  if (cmd.compressed_size > data.size() - sizeof(cmd)) {
    throw runtime_error("compressed end offset is beyond end of command");
  }

  if (c->options.debug) {
    string decompressed = bc0_decompress(cmd.data, cmd.compressed_size);
    c->log.info("Decompressed sync data (%" PRIX32 " -> %zX bytes; expected %" PRIX32 "):",
        cmd.compressed_size.load(), decompressed.size(), cmd.decompressed_size.load());
    print_data(stderr, decompressed);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_game_loading(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_size_client(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_ClientIDHeader>(
      data, sizeof(G_ClientIDHeader), 0xFFFF);
  if (cmd.client_id != c->lobby_client_id) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_size_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc<G_UnusedHeader>(data, sizeof(G_UnusedHeader), 0xFFFF);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_size_ep3_lobby(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc<G_UnusedHeader>(data, sizeof(G_UnusedHeader), 0xFFFF);
  if (l->is_game() || !l->is_ep3()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

static void on_forward_check_size_ep3_game(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  check_size_sc<G_UnusedHeader>(data, sizeof(G_UnusedHeader), 0xFFFF);
  if (!l->is_game() || !l->is_ep3()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Ep3 subcommands

static void on_ep3_battle_subs(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& orig_data) {
  const auto& header = check_size_sc<G_CardBattleCommandHeader>(
      orig_data, sizeof(G_CardBattleCommandHeader), 0xFFFF);
  if (!l->is_game() || !l->is_ep3()) {
    return;
  }

  string data = orig_data;
  set_mask_for_ep3_game_command(data.data(), data.size(), 0);

  if (header.subcommand == 0xB5) {
    if (header.subsubcommand == 0x1A) {
      return;
    } else if (header.subsubcommand == 0x36) {
      const auto& cmd = check_size_t<G_Unknown_GC_Ep3_6xB5x36>(data);
      if (l->is_game() && (cmd.unknown_a1 >= 4)) {
        return;
      }
    }
  }

  if (!(s->ep3_data_index->behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
    uint8_t mask_key = 0;
    while (!mask_key) {
      mask_key = random_object<uint8_t>();
    }
    set_mask_for_ep3_game_command(data.data(), data.size(), mask_key);
  }

  forward_subcommand(l, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Chat commands and the like

static void on_send_guild_card(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!command_is_private(command) || !l || (flag >= l->max_clients) ||
      (!l->clients[flag])) {
    return;
  }

  switch (c->version()) {
    case GameVersion::DC: {
      const auto& cmd = check_size_sc<G_SendGuildCard_DC_6x06>(data);
      c->game_data.player()->guild_card_description = cmd.description;
      break;
    }
    case GameVersion::PC: {
      const auto& cmd = check_size_sc<G_SendGuildCard_PC_6x06>(data);
      c->game_data.player()->guild_card_description = cmd.description;
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      const auto& cmd = check_size_sc<G_SendGuildCard_V3_6x06>(data);
      c->game_data.player()->guild_card_description = cmd.description;
      break;
    }
    case GameVersion::BB:
      // Nothing to do... the command is blank; the server generates the guild
      // card to be sent
      break;
    default:
      throw logic_error("unsupported game version");
  }

  send_guild_card(l->clients[flag], c);
}

// client sends a symbol chat
static void on_symbol_chat(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_SymbolChat_6x07>(data);

  if (!c->can_chat || (cmd.client_id != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
}

// client sends a word select chat
static void on_word_select(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_WordSelect_6x74>(data);

  if (!c->can_chat || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  forward_subcommand(l, c, command, flag, data);
}

// client is done loading into a lobby (we use this to trigger arrow updates)
static void on_set_player_visibility(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_SetPlayerVisibility_6x22_6x23>(data);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  forward_subcommand(l, c, command, flag, data);

  if (!l->is_game() && !(c->flags & Client::Flag::IS_DC_V1)) {
    send_arrow_update(l);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Game commands used by cheat mechanisms

static void on_change_area(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_InterLevelWarp_6x21>(data);
  if (!l->is_game()) {
    return;
  }
  c->area = cmd.area;
  forward_subcommand(l, c, command, flag, data);
}

// when a player is hit by an enemy, heal them if infinite HP is enabled
static void on_hit_by_enemy(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_ClientIDHeader>(data, sizeof(G_ClientIDHeader), 0xFFFF);
  if (!l->is_game() || (cmd.client_id != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.infinite_hp) {
    send_player_stats_change(l, c, PlayerStatsChange::ADD_HP, 2550);
  }
}

// when a player casts a tech, restore TP if infinite TP is enabled
static void on_cast_technique_finished(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_CastTechniqueComplete_6x48>(data);
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.infinite_tp) {
    send_player_stats_change(l, c, PlayerStatsChange::ADD_TP, 255);
  }
}

static void on_attack_finished(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_AttackFinished_6x46>(data,
      offsetof(G_AttackFinished_6x46, entries), sizeof(G_AttackFinished_6x46));
  size_t allowed_count = min<size_t>(cmd.header.size - 2, 11);
  if (cmd.count > allowed_count) {
    throw runtime_error("invalid attack finished command");
  }
  on_forward_check_size_client(s, l, c, command, flag, data);
}

static void on_cast_technique(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_CastTechnique_6x47>(data,
      offsetof(G_CastTechnique_6x47, targets), sizeof(G_CastTechnique_6x47));
  size_t allowed_count = min<size_t>(cmd.header.size - 2, 10);
  if (cmd.target_count > allowed_count) {
    throw runtime_error("invalid cast technique command");
  }
  on_forward_check_size_client(s, l, c, command, flag, data);
}

static void on_subtract_pb_energy(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_SubtractPBEnergy_6x49>(data,
      offsetof(G_SubtractPBEnergy_6x49, entries), sizeof(G_SubtractPBEnergy_6x49));
  size_t allowed_count = min<size_t>(cmd.header.size - 3, 14);
  if (cmd.entry_count > allowed_count) {
    throw runtime_error("invalid subtract PB energy command");
  }
  on_forward_check_size_client(s, l, c, command, flag, data);
}

static void on_switch_state_changed(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);
  if (cmd.flags && cmd.header.object_id != 0xFFFF) {
    if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.switch_assist &&
        (c->last_switch_enabled_command.header.subcommand == 0x05)) {
      c->log.info("[Switch assist] Replaying previous enable command");
      if (c->options.debug) {
        send_text_message(c, u"$C5Switch assist");
      }
      forward_subcommand(l, c, command, flag, &c->last_switch_enabled_command,
          sizeof(c->last_switch_enabled_command));
      send_command_t(c, command, flag, c->last_switch_enabled_command);
    }
    c->last_switch_enabled_command = cmd;
  }
}

////////////////////////////////////////////////////////////////////////////////

template <typename CmdT>
void on_movement(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<CmdT>(data);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  c->x = cmd.x;
  c->z = cmd.z;

  forward_subcommand(l, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Item commands

static void on_player_drop_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_DropItem_6x2A>(data);

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto item = c->game_data.player()->remove_item(cmd.item_id, 0, c->version() != GameVersion::BB);
    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.data.name(false);
    l->log.info("Player %hu dropped item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5DROP %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_create_inventory_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_CreateInventoryItem_DC_6x2B>(data,
      sizeof(G_CreateInventoryItem_DC_6x2B), sizeof(G_CreateInventoryItem_PC_V3_BB_6x2B));

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  if (c->version() == GameVersion::BB) {
    // BB should never send this command - inventory items should only be
    // created by the server in response to shop buy / bank withdraw / etc. reqs
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    PlayerInventoryItem item;
    item.present = 1;
    item.flags = 0;
    item.data = cmd.item;
    c->game_data.player()->add_item(item);

    auto name = item.data.name(false);
    l->log.info("Player %hu created inventory item %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item.id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5CREATE %08" PRIX32 "\n%s",
          cmd.item.id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_drop_partial_stack(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_DropStackedItem_DC_6x5D>(data,
      sizeof(G_DropStackedItem_DC_6x5D), sizeof(G_DropStackedItem_PC_V3_BB_6x5D));

  // TODO: Should we check the client ID here too?
  if (!l->is_game()) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    // TODO: Should we delete anything from the inventory here? Does the client
    // send an appropriate 6x29 alongside this?
    PlayerInventoryItem item;
    item.present = 1;
    item.flags = 0;
    item.data = cmd.data;
    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.data.name(false);
    l->log.info("Player %hu split stack to create ground item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), item.data.id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5SPLIT %08" PRIX32 "\n%s",
          item.data.id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_drop_partial_stack_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_SplitStackedItem_6xC3>(data);

    if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto item = c->game_data.player()->remove_item(
        cmd.item_id, cmd.amount, c->version() != GameVersion::BB);

    // if a stack was split, the original item still exists, so the dropped item
    // needs a new ID. remove_item signals this by returning an item with id=-1
    if (item.data.id == 0xFFFFFFFF) {
      item.data.id = l->generate_item_id(c->lobby_client_id);
    }

    // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add
    // the item back to the player's inventory to correct for this (it will get
    // removed again by the 6x29 handler)
    c->game_data.player()->add_item(item);

    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.data.name(false);
    l->log.info("Player %hu split stack %08" PRIX32 " (removed: %s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5SPLIT/BB %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);

    send_drop_stacked_item(l, item.data, cmd.area, cmd.x, cmd.z);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_buy_shop_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_BuyShopItem_6x5E>(data);

  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    PlayerInventoryItem item;
    item.present = 1;
    item.flags = 0;
    item.data = cmd.item;
    c->game_data.player()->add_item(item);

    auto name = item.data.name(false);
    l->log.info("Player %hu bought item %08" PRIX32 " (%s) from shop",
        cmd.header.client_id.load(), item.data.id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5BUY %08" PRIX32 "\n%s",
          item.data.id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_box_or_enemy_item_drop(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_DropItem_DC_6x5F>(data,
      sizeof(G_DropItem_DC_6x5F), sizeof(G_DropItem_PC_V3_BB_6x5F));

  if (!l->is_game() || (c->lobby_client_id != l->leader_id)) {
    return;
  }
  if (l->version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    PlayerInventoryItem item;
    item.present = 1;
    item.flags = 0;
    item.data = cmd.data;
    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.data.name(false);
    l->log.info("Leader created ground item %08" PRIX32 " (%s) at %hhu:(%g, %g)",
        item.data.id.load(), name.c_str(), cmd.area, cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5DROP %08" PRIX32 "\n%s",
          item.data.id.load(), name.c_str());
    }
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_pick_up_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  auto& cmd = check_size_sc<G_PickUpItem_6x59>(data);

  if (!l->is_game()) {
    return;
  }
  if (l->version == GameVersion::BB) {
    // BB clients should never send this; only the server should send this
    return;
  }

  auto effective_c = l->clients.at(cmd.header.client_id);
  if (!effective_c.get()) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto item = l->remove_item(cmd.item_id);
    effective_c->game_data.player()->add_item(item);

    auto name = item.data.name(false);
    l->log.info("Player %hu picked up %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5PICK %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    effective_c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_pick_up_item_request(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  // This is handled by the server on BB, and by the leader on other versions
  if (l->version == GameVersion::BB) {
    auto& cmd = check_size_sc<G_PickUpItemRequest_6x5A>(data);

    if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto item = l->remove_item(cmd.item_id);
    c->game_data.player()->add_item(item);

    auto name = item.data.name(false);
    l->log.info("Player %hu picked up %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5PICK/BB %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);

    send_pick_up_item(l, c, cmd.item_id, cmd.area);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_equip_unequip_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_EquipOrUnequipItem_6x25_6x26>(data);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    size_t index = c->game_data.player()->inventory.find_item(cmd.item_id);
    if (cmd.header.subcommand == 0x25) { // Equip
      c->game_data.player()->inventory.items[index].flags |= 0x00000008;
    } else { // Unequip
      c->game_data.player()->inventory.items[index].flags &= 0xFFFFFFF7;
    }
  } else if (l->version == GameVersion::BB) {
    throw logic_error("item tracking not enabled in BB game");
  }

  // TODO: Should we forward this command on BB? The old version of newserv
  // didn't, but that seems wrong.
  forward_subcommand(l, c, command, flag, data);
}

static void on_use_item(
    shared_ptr<ServerState> s,
    shared_ptr<Lobby> l,
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_UseItem_6x27>(data);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    size_t index = c->game_data.player()->inventory.find_item(cmd.item_id);
    string name, colored_name;
    {
      // Note: We do this weird scoping thing because player_use_item will
      // likely delete the item, which will break the reference here.
      const auto& item = c->game_data.player()->inventory.items[index].data;
      name = item.name(false);
      colored_name = item.name(true);
    }
    player_use_item(s, c, index);

    l->log.info("Player used item %hu:%08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      send_text_message_printf(c, "$C5USE %08" PRIX32 "\n%s",
          cmd.item_id.load(), colored_name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_feed_mag(
    shared_ptr<ServerState> s,
    shared_ptr<Lobby> l,
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_FeedMAG_6x28>(data);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    size_t mag_index = c->game_data.player()->inventory.find_item(cmd.mag_item_id);
    size_t fed_index = c->game_data.player()->inventory.find_item(cmd.fed_item_id);
    string mag_name, mag_colored_name, fed_name, fed_colored_name;
    {
      // Note: We do this weird scoping thing because player_use_item will
      // likely delete the item, which will break the reference here.
      const auto& fed_item = c->game_data.player()->inventory.items[fed_index].data;
      fed_name = fed_item.name(false);
      fed_colored_name = fed_item.name(true);
      const auto& mag_item = c->game_data.player()->inventory.items[mag_index].data;
      mag_name = mag_item.name(false);
      mag_colored_name = mag_item.name(true);
    }
    player_feed_mag(s, c, mag_index, fed_index);

    // On BB, the player only sends a 6x28; on other versions, the player sends
    // a 6x29 immediately after to destroy the fed item. So on BB, we should
    // remove the fed item here, but on other versions, we allow the following
    // 6x29 command to do that.
    if (l->version == GameVersion::BB) {
      c->game_data.player()->remove_item(cmd.fed_item_id, 1, false);
    }

    l->log.info("Player fed item %hu:%08" PRIX32 " (%s) to mag %hu:%08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.fed_item_id.load(), fed_name.c_str(),
        cmd.header.client_id.load(), cmd.mag_item_id.load(), mag_name.c_str());
    if (c->options.debug) {
      send_text_message_printf(c, "$C5FEED %08" PRIX32 "\n%s\n...TO %08" PRIX32 "\n%s",
          cmd.fed_item_id.load(), fed_colored_name.c_str(),
          cmd.mag_item_id.load(), mag_colored_name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_open_shop_bb_or_ep3_battle_subs(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->is_ep3()) {
    on_ep3_battle_subs(s, l, c, command, flag, data);

  } else if (!l->item_creator.get()) {
    throw runtime_error("received shop subcommand without item creator present");

  } else {
    const auto& cmd = check_size_sc<G_ShopContentsRequest_BB_6xB5>(data, 0x08);
    if ((l->version == GameVersion::BB) && l->is_game()) {
      if (!l->item_creator) {
        throw logic_error("item creator missing from BB game");
      }

      size_t level = c->game_data.player()->disp.level + 1;
      switch (cmd.shop_type) {
        case 0:
          c->game_data.shop_contents[0] = l->item_creator->generate_tool_shop_contents(level);
          break;
        case 1:
          c->game_data.shop_contents[1] = l->item_creator->generate_weapon_shop_contents(level);
          break;
        case 2:
          c->game_data.shop_contents[2] = l->item_creator->generate_armor_shop_contents(level);
          break;
        default:
          throw runtime_error("invalid shop type");
      }
      for (auto& item : c->game_data.shop_contents[cmd.shop_type]) {
        item.id = l->generate_item_id(c->lobby_client_id);
        item.data2d = s->item_parameter_table->price_for_item(item);
      }

      send_shop(c, cmd.shop_type);
    }
  }
}

static void on_open_bank_bb_or_card_trade_counter_ep3(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {
  if ((l->version == GameVersion::BB) && l->is_game()) {
    send_bank(c);
  } else if ((l->version == GameVersion::GC) && l->is_ep3()) {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_bank_action_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t, const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_BankAction_BB_6xBD>(data);

    if (!l->is_game()) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    if (cmd.action == 0) { // deposit
      if (cmd.item_id == 0xFFFFFFFF) { // meseta
        if (cmd.meseta_amount > c->game_data.player()->disp.meseta) {
          return;
        }
        if ((c->game_data.player()->bank.meseta + cmd.meseta_amount) > 999999) {
          return;
        }
        c->game_data.player()->bank.meseta += cmd.meseta_amount;
        c->game_data.player()->disp.meseta -= cmd.meseta_amount;
      } else { // item
        auto item = c->game_data.player()->remove_item(
            cmd.item_id, cmd.item_amount, c->version() != GameVersion::BB);
        c->game_data.player()->bank.add_item(item);
        send_destroy_item(l, c, cmd.item_id, cmd.item_amount);
      }
    } else if (cmd.action == 1) { // take
      if (cmd.item_id == 0xFFFFFFFF) { // meseta
        if (cmd.meseta_amount > c->game_data.player()->bank.meseta) {
          return;
        }
        if ((c->game_data.player()->disp.meseta + cmd.meseta_amount) > 999999) {
          return;
        }
        c->game_data.player()->bank.meseta -= cmd.meseta_amount;
        c->game_data.player()->disp.meseta += cmd.meseta_amount;
      } else { // item
        auto bank_item = c->game_data.player()->bank.remove_item(cmd.item_id, cmd.item_amount);
        PlayerInventoryItem item = bank_item;
        item.data.id = l->generate_item_id(0xFF);
        c->game_data.player()->add_item(item);
        send_create_inventory_item(l, c, item.data);
      }
    }
  }
}

static void on_sort_inventory_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_SortInventory_6xC4>(data);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    PlayerInventory sorted;

    for (size_t x = 0; x < 30; x++) {
      if (cmd.item_ids[x] == 0xFFFFFFFF) {
        sorted.items[x].data.id = 0xFFFFFFFF;
      } else {
        size_t index = c->game_data.player()->inventory.find_item(cmd.item_ids[x]);
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

static bool drop_item(
    std::shared_ptr<Lobby> l,
    int64_t enemy_id,
    uint8_t area,
    float x,
    float z,
    uint16_t request_id) {

  // If the game is not BB, forward the request to the leader instead of
  // generating the item drop command
  if (l->version != GameVersion::BB) {
    if (!(l->flags & Lobby::Flag::DROPS_ENABLED)) {
      return true; // don't forward request to leader if drops are disabled
    } else {
      return false; // do the normal thing where we ask the leader for a drop
    }
  }

  // If the game is BB, run the rare + common drop logic
  PlayerInventoryItem item;
  if (!l->item_creator.get()) {
    throw runtime_error("received box drop subcommand without item creator present");
  }

  if (enemy_id >= 0) {
    item.data = l->item_creator->on_monster_item_drop(
        l->enemies.at(enemy_id).rt_index, area);
  } else {
    item.data = l->item_creator->on_box_item_drop(area);
  }
  item.data.id = l->generate_item_id(0xFF);

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    l->add_item(item, area, x, z);
  }
  send_drop_item(l, item.data, (enemy_id >= 0), area, x, z, request_id);
  return true;
}

static void on_enemy_drop_item_request(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game()) {
    return;
  }

  const auto& cmd = check_size_sc<G_EnemyDropItemRequest_DC_6x60>(data,
      sizeof(G_EnemyDropItemRequest_DC_6x60),
      sizeof(G_EnemyDropItemRequest_PC_V3_BB_6x60));
  if (!drop_item(l, cmd.enemy_id, cmd.area, cmd.x, cmd.z, cmd.enemy_id)) {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_box_drop_item_request(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (!l->is_game()) {
    return;
  }

  const auto& cmd = check_size_sc<G_BoxItemDropRequest_6xA2>(data);
  if (!drop_item(l, -1, cmd.area, cmd.x, cmd.z, cmd.request_id)) {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_phase_setup(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (c->version() == GameVersion::DC || c->version() == GameVersion::PC) {
    forward_subcommand(l, c, command, flag, data);
    return;
  }

  const auto& cmd = check_size_sc<G_PhaseSetup_V3_BB_6x75>(data);
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(l, c, command, flag, data);

  bool should_send_boss_drop_req = false;
  bool is_ep2 = (l->episode == Episode::EP2);
  if (cmd.difficulty == l->difficulty) {
    if ((l->episode == Episode::EP1) && (c->area == 0x0E)) {
      // On Normal, Dark Falz does not have a third phase, so send the drop
      // request after the end of the second phase. On all other difficulty
      // levels, send it after the third phase.
      if (((l->difficulty == 0) && (cmd.basic_cmd.phase == 0x00000035)) ||
          ((l->difficulty != 0) && (cmd.basic_cmd.phase == 0x00000037))) {
        should_send_boss_drop_req = true;
      }
    } else if (is_ep2 && (cmd.basic_cmd.phase == 0x00000057) && (c->area == 0x0D)) {
      should_send_boss_drop_req = true;
    }
  }

  if (should_send_boss_drop_req) {
    auto c = l->clients.at(l->leader_id);
    if (c) {
      G_EnemyDropItemRequest_PC_V3_BB_6x60 req = {
          {
              {0x60, 0x06, 0x0000},
              static_cast<uint8_t>(c->area),
              static_cast<uint8_t>(is_ep2 ? 0x4E : 0x2F),
              0x0B4F,
              is_ep2 ? -9999.0f : 10160.58984375f,
              0.0f,
              2,
              0,
          },
          0xE0AEDC01,
      };
      send_command_t(c, 0x62, l->leader_id, req);
    }
  }
}

// enemy hit by player
static void on_enemy_hit(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_EnemyHitByPlayer_6x0A>(data);

    if (!l->is_game()) {
      return;
    }
    if (cmd.enemy_id >= l->enemies.size()) {
      return;
    }

    if (l->enemies[cmd.enemy_id].hit_flags & 0x80) {
      return;
    }
    l->enemies[cmd.enemy_id].hit_flags |= (1 << c->lobby_client_id);
    l->enemies[cmd.enemy_id].last_hit = c->lobby_client_id;
  }

  forward_subcommand(l, c, command, flag, data);
}

static void on_enemy_killed(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  forward_subcommand(l, c, command, flag, data);

  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_EnemyKilled_6xC8>(data);

    if (!l->is_game()) {
      throw runtime_error("client should not kill enemies outside of games");
    }
    if (cmd.enemy_id >= l->enemies.size()) {
      send_text_message(c, u"$C6Missing enemy killed");
      return;
    }

    auto& e = l->enemies[cmd.enemy_id];
    string e_str = e.str();
    c->log.info("Enemy killed: entry %hu => %s", cmd.enemy_id.load(), e_str.c_str());
    if (e.hit_flags & 0x80) {
      if (c->options.debug) {
        send_text_message_printf(c, "$C5E-%hX (already dead)", cmd.enemy_id.load());
      }
      return; // Enemy is already dead
    }
    if (e.experience == 0xFFFFFFFF) {
      if (c->options.debug) {
        send_text_message_printf(c, "$C5E-%hX (missing definition)", cmd.enemy_id.load());
      } else {
        send_text_message(c, u"$C6Unknown enemy type killed");
      }
      return;
    }

    e.hit_flags |= 0x80;
    for (size_t x = 0; x < l->max_clients; x++) {
      if (!((e.hit_flags >> x) & 1)) {
        continue; // Player did not hit this enemy
      }

      auto other_c = l->clients[x];
      if (!other_c) {
        continue; // No player
      }
      if (other_c->game_data.player()->disp.level >= 199) {
        continue; // Player is level 200 or higher
      }

      // Killer gets full experience, others get 77%
      uint32_t exp;
      if (e.last_hit == other_c->lobby_client_id) {
        exp = e.experience;
      } else {
        exp = ((e.experience * 77) / 100);
      }

      other_c->game_data.player()->disp.experience += exp;
      send_give_experience(l, other_c, exp);
      if (other_c->options.debug) {
        send_text_message_printf(other_c, "$C5+%" PRIu32 " E-%hX (%s)",
            exp, cmd.enemy_id.load(), e.type_name);
      }

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

static void on_destroy_inventory_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_DeleteInventoryItem_6x29>(data);
  if (!l->is_game()) {
    return;
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto item = c->game_data.player()->remove_item(
        cmd.item_id, cmd.amount, c->version() != GameVersion::BB);
    auto name = item.data.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " destroyed (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5DESTROY %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_destroy_ground_item(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  const auto& cmd = check_size_sc<G_DestroyGroundItem_6x63>(data);
  if (!l->is_game()) {
    return;
  }
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto item = l->remove_item(cmd.item_id);
    auto name = item.data.name(false);
    l->log.info("Ground item %08" PRIX32 " destroyed (%s)", cmd.item_id.load(),
        name.c_str());
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5DESTROY/GND %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_identify_item_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_AcceptItemIdentification_BB_6xB8>(data);
    if (!l->is_game()) {
      return;
    }
    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    size_t x = c->game_data.player()->inventory.find_item(cmd.item_id);
    if (c->game_data.player()->inventory.items[x].data.data1[0] != 0) {
      return; // only weapons can be identified
    }

    c->game_data.player()->disp.meseta -= 100;
    c->game_data.identify_result = c->game_data.player()->inventory.items[x];
    c->game_data.identify_result.data.data1[4] &= 0x7F;

    // TODO: move this into a SendCommands.cc function
    G_IdentifyResult_BB_6xB9 res;
    res.header.subcommand = 0xB9;
    res.header.size = sizeof(res) / 4;
    res.header.client_id = c->lobby_client_id;
    res.item = c->game_data.identify_result.data;
    send_command_t(l, 0x60, 0x00, res);

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_accept_identify_item_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag,
    const string& data) {

  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_AcceptItemIdentification_BB_6xBA>(data);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    if (!c->game_data.identify_result.data.id) {
      throw runtime_error("no identify result present");
    }
    if (c->game_data.identify_result.data.id != cmd.item_id) {
      throw runtime_error("accepted item ID does not match previous identify request");
    }
    c->game_data.player()->add_item(c->game_data.identify_result);
    send_create_inventory_item(l, c, c->game_data.identify_result.data);
    c->game_data.identify_result.clear();

  } else {
    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_sell_item_at_shop_bb(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {

  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_SellItemAtShop_BB_6xC0>(data);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto item = c->game_data.player()->remove_item(
        cmd.item_id, cmd.amount, c->version() != GameVersion::BB);
    size_t price = (s->item_parameter_table->price_for_item(item.data) >> 3) * cmd.amount;
    c->game_data.player()->disp.meseta = min<uint32_t>(
        c->game_data.player()->disp.meseta + price, 999999);

    auto name = item.data.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " destroyed via sale (%s)",
        c->lobby_client_id, cmd.item_id.load(), name.c_str());
    c->game_data.player()->print_inventory(stderr);
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5DESTROY/SELL %08" PRIX32 "\n+%zu Meseta\n%s",
          cmd.item_id.load(), price, name.c_str());
    }

    forward_subcommand(l, c, command, flag, data);
  }
}

static void on_buy_shop_item_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t, const string& data) {
  if (l->version == GameVersion::BB) {
    const auto& cmd = check_size_sc<G_BuyShopItem_BB_6xB7>(data);
    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    PlayerInventoryItem item;
    item.data = c->game_data.shop_contents.at(cmd.shop_type).at(cmd.item_index);
    if (item.data.is_stackable()) {
      item.data.data1[5] = cmd.amount;
    } else if (cmd.amount != 1) {
      throw runtime_error("item is not stackable");
    }

    size_t price = item.data.data2d * cmd.amount;
    item.data.data2d = 0;
    if (c->game_data.player()->disp.meseta < price) {
      throw runtime_error("player does not have enough money");
    }
    c->game_data.player()->disp.meseta -= price;

    item.data.id = cmd.inventory_item_id;
    c->game_data.player()->add_item(item);
    send_create_inventory_item(l, c, item.data);

    auto name = item.data.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " created via purchase (%s) for %zu meseta",
        c->lobby_client_id, cmd.inventory_item_id.load(), name.c_str(), price);
    c->game_data.player()->print_inventory(stderr);
    if (c->options.debug) {
      string name = item.data.name(true);
      send_text_message_printf(c, "$C5CREATE/BUY %08" PRIX32 "\n-%zu Meseta\n%s",
          cmd.inventory_item_id.load(), price, name.c_str());
    }
  }
}

static void on_medical_center_bb(shared_ptr<ServerState>,
    shared_ptr<Lobby> l, shared_ptr<Client> c, uint8_t, uint8_t, const string&) {

  if (l->version == GameVersion::BB) {
    if (c->game_data.player()->disp.meseta < 10) {
      throw runtime_error("insufficient funds");
    }
    c->game_data.player()->disp.meseta -= 10;
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
    /* 00 */ on_invalid,
    /* 01 */ nullptr,
    /* 02 */ nullptr,
    /* 03 */ nullptr,
    /* 04 */ nullptr,
    /* 05 */ on_switch_state_changed,
    /* 06 */ on_send_guild_card,
    /* 07 */ on_symbol_chat,
    /* 08 */ nullptr,
    /* 09 */ nullptr,
    /* 0A */ on_enemy_hit,
    /* 0B */ on_forward_check_size_game,
    /* 0C */ on_forward_check_size_game, // Add condition (poison/slow/etc.)
    /* 0D */ on_forward_check_size_game, // Remove condition (poison/slow/etc.)
    /* 0E */ nullptr,
    /* 0F */ nullptr,
    /* 10 */ nullptr,
    /* 11 */ nullptr,
    /* 12 */ on_forward_check_size_game, // Dragon actions
    /* 13 */ on_forward_check_size_game, // De Rol Le actions
    /* 14 */ on_forward_check_size_game,
    /* 15 */ on_forward_check_size_game, // Vol Opt actions
    /* 16 */ on_forward_check_size_game, // Vol Opt actions
    /* 17 */ on_forward_check_size_game,
    /* 18 */ on_forward_check_size_game,
    /* 19 */ on_forward_check_size_game, // Dark Falz actions
    /* 1A */ nullptr,
    /* 1B */ nullptr,
    /* 1C */ on_forward_check_size_game,
    /* 1D */ nullptr,
    /* 1E */ nullptr,
    /* 1F */ on_forward_check_size,
    /* 20 */ on_forward_check_size,
    /* 21 */ on_change_area, // Inter-level warp
    /* 22 */ on_forward_check_size_client, // Set player visibility
    /* 23 */ on_set_player_visibility, // Set player visibility
    /* 24 */ on_forward_check_size_game,
    /* 25 */ on_equip_unequip_item, // Equip item
    /* 26 */ on_equip_unequip_item, // Unequip item
    /* 27 */ on_use_item,
    /* 28 */ on_feed_mag, // Feed MAG
    /* 29 */ on_destroy_inventory_item, // Delete item (via bank deposit / sale / feeding MAG)
    /* 2A */ on_player_drop_item,
    /* 2B */ on_create_inventory_item, // Create inventory item (e.g. from tekker or bank withdrawal)
    /* 2C */ on_forward_check_size, // Talk to NPC
    /* 2D */ on_forward_check_size, // Done talking to NPC
    /* 2E */ nullptr,
    /* 2F */ on_hit_by_enemy,
    /* 30 */ on_forward_check_size_game, // Level up
    /* 31 */ on_forward_check_size_game, // Medical center
    /* 32 */ on_forward_check_size_game, // Medical center
    /* 33 */ on_forward_check_size_game, // Moon atomizer/Reverser
    /* 34 */ nullptr,
    /* 35 */ nullptr,
    /* 36 */ on_forward_check_game,
    /* 37 */ on_forward_check_size_game, // Photon blast
    /* 38 */ nullptr,
    /* 39 */ on_forward_check_size_game, // Photon blast ready
    /* 3A */ on_forward_check_size_game,
    /* 3B */ on_forward_check_size,
    /* 3C */ nullptr,
    /* 3D */ nullptr,
    /* 3E */ on_movement<G_StopAtPosition_6x3E>, // Stop moving
    /* 3F */ on_movement<G_SetPosition_6x3F>, // Set position (e.g. when materializing after warp)
    /* 40 */ on_movement<G_WalkToPosition_6x40>, // Walk
    /* 41 */ nullptr,
    /* 42 */ on_movement<G_RunToPosition_6x42>, // Run
    /* 43 */ on_forward_check_size_client,
    /* 44 */ on_forward_check_size_client,
    /* 45 */ on_forward_check_size_client,
    /* 46 */ on_attack_finished,
    /* 47 */ on_cast_technique,
    /* 48 */ on_cast_technique_finished,
    /* 49 */ on_subtract_pb_energy,
    /* 4A */ on_forward_check_size_client,
    /* 4B */ on_hit_by_enemy,
    /* 4C */ on_hit_by_enemy,
    /* 4D */ on_forward_check_size_client,
    /* 4E */ on_forward_check_size_client,
    /* 4F */ on_forward_check_size_client,
    /* 50 */ on_forward_check_size_client,
    /* 51 */ nullptr,
    /* 52 */ on_forward_check_size, // Toggle shop/bank interaction
    /* 53 */ on_forward_check_size_game,
    /* 54 */ nullptr,
    /* 55 */ on_forward_check_size_client, // Intra-map warp
    /* 56 */ on_forward_check_size_client,
    /* 57 */ on_forward_check_size_client,
    /* 58 */ on_forward_check_size_client, // Begin playing emote
    /* 59 */ on_pick_up_item, // Item picked up
    /* 5A */ on_pick_up_item_request, // Request to pick up item
    /* 5B */ nullptr,
    /* 5C */ nullptr,
    /* 5D */ on_drop_partial_stack, // Drop meseta or stacked item
    /* 5E */ on_buy_shop_item, // Buy item at shop
    /* 5F */ on_box_or_enemy_item_drop, // Drop item from box/enemy
    /* 60 */ on_enemy_drop_item_request, // Request for item drop (handled by the server on BB)
    /* 61 */ on_forward_check_size_game, // Feed mag
    /* 62 */ nullptr,
    /* 63 */ on_destroy_ground_item, // Destroy an item on the ground (used when too many items have been dropped)
    /* 64 */ nullptr,
    /* 65 */ nullptr,
    /* 66 */ on_forward_check_size_game, // Use star atomizer
    /* 67 */ on_forward_check_size_game, // Create enemy set
    /* 68 */ on_forward_check_size_game, // Telepipe/Ryuker
    /* 69 */ on_forward_check_size_game,
    /* 6A */ on_forward_check_size_game,
    /* 6B */ on_forward_sync_game_state,
    /* 6C */ on_forward_sync_game_state,
    /* 6D */ on_forward_sync_game_state,
    /* 6E */ on_forward_sync_game_state,
    /* 6F */ on_forward_check_game_loading,
    /* 70 */ on_forward_check_game_loading,
    /* 71 */ on_forward_check_game_loading,
    /* 72 */ on_forward_check_game_loading,
    /* 73 */ on_invalid,
    /* 74 */ on_word_select,
    /* 75 */ on_phase_setup,
    /* 76 */ on_forward_check_size_game, // Enemy killed
    /* 77 */ on_forward_check_size_game, // Sync quest data
    /* 78 */ nullptr,
    /* 79 */ on_forward_check_size, // Lobby 14/15 soccer game
    /* 7A */ nullptr,
    /* 7B */ nullptr,
    /* 7C */ on_forward_check_size_game,
    /* 7D */ on_forward_check_size_game,
    /* 7E */ nullptr,
    /* 7F */ nullptr,
    /* 80 */ on_forward_check_size_game, // Trigger trap
    /* 81 */ nullptr,
    /* 82 */ nullptr,
    /* 83 */ on_forward_check_size_game, // Place trap
    /* 84 */ on_forward_check_size_game,
    /* 85 */ on_forward_check_size_game,
    /* 86 */ on_forward_check_size_game, // Hit destructible wall
    /* 87 */ on_forward_check_size_game, // Shrink character
    /* 88 */ on_forward_check_size_game,
    /* 89 */ on_forward_check_size_game,
    /* 8A */ on_forward_check_size_game,
    /* 8B */ nullptr,
    /* 8C */ nullptr,
    /* 8D */ on_forward_check_size_client,
    /* 8E */ nullptr,
    /* 8F */ nullptr,
    /* 90 */ nullptr,
    /* 91 */ on_forward_check_size_game,
    /* 92 */ nullptr,
    /* 93 */ on_forward_check_size_game, // Timed switch activated
    /* 94 */ on_forward_check_size_game, // Warp (the $warp chat command is implemented using this)
    /* 95 */ nullptr,
    /* 96 */ nullptr,
    /* 97 */ nullptr,
    /* 98 */ nullptr,
    /* 99 */ nullptr,
    /* 9A */ on_forward_check_size_game, // Update player stat ($infhp/$inftp are implemented using this command)
    /* 9B */ nullptr,
    /* 9C */ on_forward_check_size_game,
    /* 9D */ nullptr,
    /* 9E */ nullptr,
    /* 9F */ on_forward_check_size_game, // Gal Gryphon actions
    /* A0 */ on_forward_check_size_game, // Gal Gryphon actions
    /* A1 */ on_forward_check_size_game, // Part of revive process. Occurs right after revive command, function unclear.
    /* A2 */ on_box_drop_item_request, // Request for item drop from box (handled by server on BB)
    /* A3 */ on_forward_check_size_game, // Episode 2 boss actions
    /* A4 */ on_forward_check_size_game, // Olga Flow phase 1 actions
    /* A5 */ on_forward_check_size_game, // Olga Flow phase 2 actions
    /* A6 */ on_forward_check_size, // Trade proposal
    /* A7 */ nullptr,
    /* A8 */ on_forward_check_size_game, // Gol Dragon actions
    /* A9 */ on_forward_check_size_game, // Barba Ray actions
    /* AA */ on_forward_check_size_game, // Episode 2 boss actions
    /* AB */ on_forward_check_size_client, // Create lobby chair
    /* AC */ nullptr,
    /* AD */ on_forward_check_size_game, // Olga Flow phase 2 subordinate boss actions
    /* AE */ on_forward_check_size_client,
    /* AF */ on_forward_check_size_client, // Turn in lobby chair
    /* B0 */ on_forward_check_size_client, // Move in lobby chair
    /* B1 */ nullptr,
    /* B2 */ nullptr,
    /* B3 */ on_ep3_battle_subs,
    /* B4 */ on_ep3_battle_subs,
    /* B5 */ on_open_shop_bb_or_ep3_battle_subs, // BB shop request
    /* B6 */ nullptr, // BB shop contents (server->client only)
    /* B7 */ on_buy_shop_item_bb,
    /* B8 */ on_identify_item_bb,
    /* B9 */ nullptr,
    /* BA */ on_accept_identify_item_bb,
    /* BB */ on_open_bank_bb_or_card_trade_counter_ep3,
    /* BC */ on_forward_check_size_ep3_game, // BB bank contents (server->client only), Ep3 card trade sequence
    /* BD */ on_bank_action_bb,
    /* BE */ on_forward_check_size, // BB create inventory item (server->client only), Ep3 sound chat
    /* BF */ on_forward_check_size_ep3_lobby, // Ep3 change music, also BB give EXP (BB usage is server->client only)
    /* C0 */ on_sell_item_at_shop_bb,
    /* C1 */ nullptr,
    /* C2 */ nullptr,
    /* C3 */ on_drop_partial_stack_bb, // Split stacked item - not sent if entire stack is dropped
    /* C4 */ on_sort_inventory_bb,
    /* C5 */ on_medical_center_bb,
    /* C6 */ nullptr,
    /* C7 */ nullptr,
    /* C8 */ on_enemy_killed,
    /* C9 */ nullptr,
    /* CA */ nullptr,
    /* CB */ nullptr,
    /* CC */ nullptr,
    /* CD */ nullptr,
    /* CE */ nullptr,
    /* CF */ on_forward_check_size_game,
    /* D0 */ nullptr,
    /* D1 */ nullptr,
    /* D2 */ nullptr,
    /* D3 */ nullptr,
    /* D4 */ nullptr,
    /* D5 */ nullptr,
    /* D6 */ nullptr,
    /* D7 */ nullptr,
    /* D8 */ nullptr,
    /* D9 */ nullptr,
    /* DA */ nullptr,
    /* DB */ nullptr,
    /* DC */ nullptr,
    /* DD */ nullptr,
    /* DE */ nullptr,
    /* DF */ nullptr,
    /* E0 */ nullptr,
    /* E1 */ nullptr,
    /* E2 */ nullptr,
    /* E3 */ nullptr,
    /* E4 */ nullptr,
    /* E5 */ nullptr,
    /* E6 */ nullptr,
    /* E7 */ nullptr,
    /* E8 */ nullptr,
    /* E9 */ nullptr,
    /* EA */ nullptr,
    /* EB */ nullptr,
    /* EC */ nullptr,
    /* ED */ nullptr,
    /* EE */ nullptr,
    /* EF */ nullptr,
    /* F0 */ nullptr,
    /* F1 */ nullptr,
    /* F2 */ nullptr,
    /* F3 */ nullptr,
    /* F4 */ nullptr,
    /* F5 */ nullptr,
    /* F6 */ nullptr,
    /* F7 */ nullptr,
    /* F8 */ nullptr,
    /* F9 */ nullptr,
    /* FA */ nullptr,
    /* FB */ nullptr,
    /* FC */ nullptr,
    /* FD */ nullptr,
    /* FE */ nullptr,
    /* FF */ nullptr,
};

void on_subcommand(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {
  if (data.empty()) {
    throw runtime_error("game command is empty");
  }
  if (c->version() == GameVersion::DC && (c->flags & (Client::Flag::IS_TRIAL_EDITION | Client::Flag::IS_DC_V1_PROTOTYPE))) {
    // TODO: We should convert these to non-trial formats and vice versa
    forward_subcommand(l, c, command, flag, std::move(data));
  } else {
    uint8_t which = static_cast<uint8_t>(data[0]);
    auto fn = subcommand_handlers[which];
    if (fn) {
      fn(s, l, c, command, flag, data);
    } else {
      on_unimplemented(s, l, c, command, flag, data);
    }
  }
}

bool subcommand_is_implemented(uint8_t which) {
  return subcommand_handlers[which] != nullptr;
}
