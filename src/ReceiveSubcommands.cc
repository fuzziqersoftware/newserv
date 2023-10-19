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

static const unordered_set<uint8_t> watcher_subcommands({
    0x07, // Symbol chat
    0x74, // Word select
    0xBD, // Word select during battle (with private_flags)
    0xBE, // Sound chat
});

static void forward_subcommand(
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    const void* data,
    size_t size) {

  // If the command is an Ep3-only command, make sure an Ep3 client sent it
  bool command_is_ep3 = (command & 0xF0) == 0xC0;
  if (command_is_ep3 && !(c->flags & Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("Episode 3 command sent by non-Episode 3 client");
  }

  auto l = c->require_lobby();
  if (command_is_private(command)) {
    if (flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[flag];
    if (!target) {
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
    // battle, forward everything to watcher lobbies. (This is necessary because
    // if we forward everything before battle, the blocking menu subcommands
    // cause the battle setup menu to appear in the spectator room, which looks
    // weird and is generally undesirable.)
    uint8_t subcommand = size ? *reinterpret_cast<const uint8_t*>(data) : 0x00;
    if ((l->ep3_server && (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION)) ||
        watcher_subcommands.count(subcommand)) {
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        for (auto& target : watcher_lobby->clients) {
          if (target && (target->flags & Client::Flag::IS_EPISODE_3)) {
            send_command(target, command, flag, data, size);
          }
        }
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

static void on_invalid(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  if (command_is_private(command)) {
    c->log.error("Invalid subcommand: %02hhX (private to %hhu)",
        cmd.subcommand, flag);
  } else {
    c->log.error("Invalid subcommand: %02hhX (public)", cmd.subcommand);
  }
}

static void on_unimplemented(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
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

static void on_forward_check_size(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_check_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_sync_joining_player_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }

  const auto& cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
  if (cmd.compressed_size > size - sizeof(cmd)) {
    throw runtime_error("compressed end offset is beyond end of command");
  }

  if (c->options.debug) {
    string decompressed = bc0_decompress(cmd.data, cmd.compressed_size);
    c->log.info("Decompressed sync data (%" PRIX32 " -> %zX bytes; expected %" PRIX32 "):",
        cmd.compressed_size.load(), decompressed.size(), cmd.decompressed_size.load());
    print_data(stderr, decompressed);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_sync_joining_player_item_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }

  // I'm lazy and this should never happen (this command should always be
  // private to the joining player)
  if (!command_is_private(command)) {
    throw runtime_error("6x6D sent via public command");
  }

  // For non-V3 versions, just forward the data verbatim. For V3, we need to
  // byteswap mags' data2 fields if exactly one of the sender and recipient is
  // PSO GC
  bool sender_is_gc = (c->version() == GameVersion::GC);
  if (!sender_is_gc && (c->version() != GameVersion::XB)) {
    forward_subcommand(c, command, flag, data, size);

  } else {
    if (flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[flag];
    if (!target) {
      return;
    }
    bool target_is_gc = (target->version() == GameVersion::GC);

    if (target_is_gc == sender_is_gc) {
      send_command(target, command, flag, data, size);

    } else {
      const auto& cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
      if (cmd.compressed_size > size - sizeof(cmd)) {
        throw runtime_error("compressed end offset is beyond end of command");
      }

      string decompressed = bc0_decompress(cmd.data, cmd.compressed_size);
      if (c->options.debug) {
        c->log.info("Decompressed item sync data (%" PRIX32 " -> %zX bytes; expected %" PRIX32 "):",
            cmd.compressed_size.load(), decompressed.size(), cmd.decompressed_size.load());
        print_data(stderr, decompressed);
      }

      if (decompressed.size() < sizeof(G_SyncItemState_6x6D_Decompressed)) {
        throw runtime_error(string_printf(
            "decompressed 6x6D data (0x%zX bytes) is too short for header (0x%zX bytes)",
            decompressed.size(), sizeof(G_SyncItemState_6x6D_Decompressed)));
      }
      auto* decompressed_cmd = reinterpret_cast<G_SyncItemState_6x6D_Decompressed*>(decompressed.data());

      size_t num_floor_items = 0;
      for (size_t z = 0; z < decompressed_cmd->floor_item_count_per_area.size(); z++) {
        num_floor_items += decompressed_cmd->floor_item_count_per_area[z];
      }

      size_t required_size = sizeof(G_SyncItemState_6x6D_Decompressed) + num_floor_items * sizeof(G_SyncItemState_6x6D_Decompressed::FloorItem);
      if (decompressed.size() < required_size) {
        throw runtime_error(string_printf(
            "decompressed 6x6D data (0x%zX bytes) is too short for all items (0x%zX bytes)",
            decompressed.size(), required_size));
      }

      for (size_t z = 0; z < num_floor_items; z++) {
        decompressed_cmd->items[z].item_data.bswap_data2_if_mag();
      }

      string out_compressed_data = bc0_compress(decompressed);

      G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E out_cmd;
      out_cmd.header.basic_header.subcommand = 0x6D;
      out_cmd.header.basic_header.size = 0x00;
      out_cmd.header.basic_header.unused = 0x0000;
      out_cmd.header.size = ((out_compressed_data.size() + sizeof(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E)) + 3) & (~3);
      out_cmd.decompressed_size = decompressed.size();
      out_cmd.compressed_size = out_compressed_data.size();

      if (c->options.debug) {
        c->log.info("Byteswapped and recompressed item sync data (%zX bytes)", out_compressed_data.size());
      }

      vector<pair<const void*, size_t>> blocks;
      blocks.emplace_back(make_pair(&out_cmd, sizeof(out_cmd)));
      blocks.emplace_back(make_pair(out_compressed_data.data(), out_compressed_data.size()));
      send_command(target, command, flag, blocks);
    }
  }
}

static void on_sync_joining_player_disp_and_inventory(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  // In V1/V2 games, this command sometimes is sent after the new client has
  // finished loading, so we don't check l->any_client_loading() here.
  if (!l->is_game()) {
    return;
  }

  // I'm lazy and this should never happen (this command should always be
  // private to the joining player)
  if (!command_is_private(command)) {
    throw runtime_error("6x70 sent via public command");
  }
  if (flag >= l->max_clients) {
    return;
  }
  auto target = l->clients[flag];
  if (!target) {
    return;
  }

  // This command's format is different on BB and non-BB
  bool sender_is_bb = (c->version() == GameVersion::BB);
  bool target_is_bb = (target->version() == GameVersion::BB);
  if (sender_is_bb != target_is_bb) {
    // TODO: Figure out the BB 6x70 format and implement this
    throw runtime_error("6x70 command cannot be translated across BB boundary");
  }

  // We need to byteswap mags' data2 fields if exactly one of the sender and
  // recipient are PSO GC
  bool sender_is_gc = (c->version() == GameVersion::GC);
  bool target_is_gc = (target->version() == GameVersion::GC);
  if (target_is_gc == sender_is_gc) {
    send_command(target, command, flag, data, size);
  } else {
    auto out_cmd = check_size_t<G_SyncPlayerDispAndInventory_DC_PC_V3_6x70>(data, size);
    for (size_t z = 0; z < 30; z++) {
      out_cmd.inventory.items[z].data.bswap_data2_if_mag();
    }
    send_command_t(target, command, flag, out_cmd);
  }
}

static void on_forward_check_game_loading(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_check_size_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  if (cmd.client_id != c->lobby_client_id) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_check_size_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_check_size_ep3_lobby(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (l->is_game() || !l->is_ep3()) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_forward_check_size_ep3_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    return;
  }
  forward_subcommand(c, command, flag, data, size);
}

////////////////////////////////////////////////////////////////////////////////
// Ep3 subcommands

static void on_ep3_sound_chat(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  // Unlike the 6x and C9 commands, subcommands sent with the CB command are
  // forwarded from spectator teams to the primary team. The client only uses
  // this behavior for the 6xBE command (sound chat), and newserv enforces that
  // only that command is sent via CB.
  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("non-Episode 3 client sent sound chat command");
  }

  auto l = c->require_lobby();
  if ((command == 0xCB) && (l->flags & Lobby::Flag::IS_SPECTATOR_TEAM)) {
    auto watched_lobby = l->watched_lobby.lock();
    if (watched_lobby) {
      for (auto& target : watched_lobby->clients) {
        if (target && (target->flags & Client::Flag::IS_EPISODE_3)) {
          send_command(target, command, flag, data, size);
        }
      }
    }
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_ep3_battle_subs(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* orig_data, size_t size) {
  const auto& header = check_size_t<G_CardBattleCommandHeader>(orig_data, size, 0xFFFF);

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    return;
  }

  string data(reinterpret_cast<const char*>(orig_data), size);
  set_mask_for_ep3_game_command(data.data(), data.size(), 0);

  if (header.subsubcommand == 0x1A) {
    return;
  } else if (header.subsubcommand == 0x36) {
    const auto& cmd = check_size_t<G_RecreatePlayer_GC_Ep3_6xB5x36>(data, size);
    if (l->is_game() && (cmd.client_id >= 4)) {
      return;
    }
  }

  if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
    uint8_t mask_key = 0;
    while (!mask_key) {
      mask_key = random_object<uint8_t>();
    }
    set_mask_for_ep3_game_command(data.data(), data.size(), mask_key);
  }

  forward_subcommand(c, command, flag, data.data(), data.size());
}

////////////////////////////////////////////////////////////////////////////////
// Chat commands and the like

static void on_send_guild_card(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!command_is_private(command) || (flag >= l->max_clients) || (!l->clients[flag])) {
    return;
  }

  switch (c->version()) {
    case GameVersion::DC: {
      const auto& cmd = check_size_t<G_SendGuildCard_DC_6x06>(data, size);
      c->game_data.player(true, false)->guild_card_description = cmd.description;
      break;
    }
    case GameVersion::PC: {
      const auto& cmd = check_size_t<G_SendGuildCard_PC_6x06>(data, size);
      c->game_data.player(true, false)->guild_card_description = cmd.description;
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      const auto& cmd = check_size_t<G_SendGuildCard_V3_6x06>(data, size);
      c->game_data.player(true, false)->guild_card_description = cmd.description;
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

static void on_symbol_chat(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SymbolChat_6x07>(data, size);
  if (c->can_chat && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_word_select(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_WordSelect_6x74>(data, size);
  if (c->can_chat && (cmd.header.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_set_player_visibility(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SetPlayerVisibility_6x22_6x23>(data, size);

  if (cmd.header.client_id == c->lobby_client_id) {
    auto l = c->require_lobby();

    forward_subcommand(c, command, flag, data, size);
    if (!l->is_game() && !(c->flags & Client::Flag::IS_DC_V1)) {
      send_arrow_update(l);
    }
    if (!l->is_game() && (l->flags & Lobby::Flag::IS_OVERFLOW)) {
      send_message_box(c, u"$C6All lobbies are full.\n\n$C7You are in a private lobby. You can use the\nteleporter to join other lobbies if there is space\navailable.");
      send_lobby_message_box(c, u"");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Game commands used by cheat mechanisms

template <typename CmdT>
static void on_change_area(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  c->area = cmd.area;
  forward_subcommand(c, command, flag, data, size);
}

// When a player dies, decrease their mag's synchro
static void on_player_died(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.client_id != c->lobby_client_id)) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    try {
      auto& inventory = c->game_data.player()->inventory;
      size_t mag_index = inventory.find_equipped_mag();
      auto& data = inventory.items[mag_index].data;
      data.data2[0] = max<int8_t>(static_cast<int8_t>(data.data2[0] - 5), 0);
    } catch (const out_of_range&) {
    }
  }

  forward_subcommand(c, command, flag, data, size);
}

// When a player is hit by an enemy, heal them if infinite HP is enabled
static void on_hit_by_enemy(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
    if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.infinite_hp) {
      send_player_stats_change(c, PlayerStatsChange::ADD_HP, 2550);
    }
  }
}

// When a player casts a tech, restore TP if infinite TP is enabled
static void on_cast_technique_finished(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_CastTechniqueComplete_6x48>(data, size);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.header.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
    if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.infinite_tp) {
      send_player_stats_change(c, PlayerStatsChange::ADD_TP, 255);
    }
  }
}

static void on_attack_finished(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_AttackFinished_6x46>(data, size,
      offsetof(G_AttackFinished_6x46, targets), sizeof(G_AttackFinished_6x46));
  size_t allowed_count = min<size_t>(cmd.header.size - 2, 11);
  if (cmd.count > allowed_count) {
    throw runtime_error("invalid attack finished command");
  }
  on_forward_check_size_client(c, command, flag, data, size);
}

static void on_cast_technique(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_CastTechnique_6x47>(data, size,
      offsetof(G_CastTechnique_6x47, targets), sizeof(G_CastTechnique_6x47));
  size_t allowed_count = min<size_t>(cmd.header.size - 2, 10);
  if (cmd.target_count > allowed_count) {
    throw runtime_error("invalid cast technique command");
  }
  on_forward_check_size_client(c, command, flag, data, size);
}

static void on_subtract_pb_energy(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SubtractPBEnergy_6x49>(data, size,
      offsetof(G_SubtractPBEnergy_6x49, entries), sizeof(G_SubtractPBEnergy_6x49));
  size_t allowed_count = min<size_t>(cmd.header.size - 3, 14);
  if (cmd.entry_count > allowed_count) {
    throw runtime_error("invalid subtract PB energy command");
  }
  on_forward_check_size_client(c, command, flag, data, size);
}

static void on_switch_state_changed(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  forward_subcommand(c, command, flag, data, size);

  if (cmd.flags && cmd.header.object_id != 0xFFFF) {
    if ((l->flags & Lobby::Flag::CHEATS_ENABLED) && c->options.switch_assist &&
        (c->last_switch_enabled_command.header.subcommand == 0x05)) {
      c->log.info("[Switch assist] Replaying previous enable command");
      if (c->options.debug) {
        send_text_message(c, u"$C5Switch assist");
      }
      forward_subcommand(c, command, flag, &c->last_switch_enabled_command, sizeof(c->last_switch_enabled_command));
      send_command_t(c, command, flag, c->last_switch_enabled_command);
    }
    c->last_switch_enabled_command = cmd;
  }
}

////////////////////////////////////////////////////////////////////////////////

template <typename CmdT>
void on_movement(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  c->x = cmd.x;
  c->z = cmd.z;

  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
void on_movement_with_area(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  c->x = cmd.x;
  c->z = cmd.z;
  c->area = cmd.area;

  forward_subcommand(c, command, flag, data, size);
}

////////////////////////////////////////////////////////////////////////////////
// Item commands

static void on_player_drop_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_DropItem_6x2A>(data, size);

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    auto item = p->remove_item(cmd.item_id, 0, c->version() != GameVersion::BB);
    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.name(false);
    l->log.info("Player %hu dropped item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5DROP %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    p->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
void forward_subcommand_with_mag_bswap_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& cmd) {
  // I'm lazy and this should never happen for item commands (since all players
  // need to stay in sync)
  if (command_is_private(command)) {
    throw runtime_error("6x2B sent via private command");
  }

  auto l = c->require_lobby();
  for (auto& other_c : l->clients) {
    if (!other_c || other_c == c) {
      continue;
    }
    CmdT out_cmd = cmd;
    if ((c->version() == GameVersion::GC) != (other_c->version() == GameVersion::GC)) {
      out_cmd.item_data.bswap_data2_if_mag();
    }
    send_command_t(other_c, command, flag, out_cmd);
  }
}

template <typename CmdT>
static void on_create_inventory_item_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  if (c->version() == GameVersion::BB) {
    // BB should never send this command - inventory items should only be
    // created by the server in response to shop buy / bank withdraw / etc. reqs
    return;
  }

  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    {
      ItemData item = cmd.item_data;
      if (c->version() == GameVersion::GC) {
        item.bswap_data2_if_mag();
      }
      p->add_item(item);
    }

    auto name = cmd.item_data.name(false);
    l->log.info("Player %hu created inventory item %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_data.id.load(), name.c_str());
    if (c->options.debug) {
      string name = cmd.item_data.name(true);
      send_text_message_printf(c, "$C5CREATE %08" PRIX32 "\n%s", cmd.item_data.id.load(), name.c_str());
    }
    p->print_inventory(stderr);
  }

  forward_subcommand_with_mag_bswap_t(c, command, flag, cmd);
}

static void on_create_inventory_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (size == sizeof(G_CreateInventoryItem_PC_V3_BB_6x2B)) {
    on_create_inventory_item_t<G_CreateInventoryItem_PC_V3_BB_6x2B>(c, command, flag, data, size);
  } else if (size == sizeof(G_CreateInventoryItem_DC_6x2B)) {
    on_create_inventory_item_t<G_CreateInventoryItem_DC_6x2B>(c, command, flag, data, size);
  } else {
    throw runtime_error("invalid size for 6x2B command");
  }
}

template <typename CmdT>
static void on_drop_partial_stack_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  // TODO: Should we check the client ID here too?
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (l->base_version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    // TODO: Should we delete anything from the inventory here? Does the client
    // send an appropriate 6x29 alongside this?
    {
      ItemData item = cmd.item_data;
      if (c->version() == GameVersion::GC) {
        item.bswap_data2_if_mag();
      }
      l->add_item(item, cmd.area, cmd.x, cmd.z);
    }

    auto name = cmd.item_data.name(false);
    l->log.info("Player %hu split stack to create ground item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_data.id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = cmd.item_data.name(true);
      send_text_message_printf(c, "$C5SPLIT %08" PRIX32 "\n%s", cmd.item_data.id.load(), name.c_str());
    }
    c->game_data.player()->print_inventory(stderr);
  }

  forward_subcommand_with_mag_bswap_t(c, command, flag, cmd);
}

static void on_drop_partial_stack(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (size == sizeof(G_DropStackedItem_PC_V3_BB_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_PC_V3_BB_6x5D>(c, command, flag, data, size);
  } else if (size == sizeof(G_DropStackedItem_DC_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_DC_6x5D>(c, command, flag, data, size);
  } else {
    throw runtime_error("invalid size for 6x5D command");
  }
}

static void on_drop_partial_stack_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_SplitStackedItem_BB_6xC3>(data, size);

    if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto p = c->game_data.player();
    auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != GameVersion::BB);

    // if a stack was split, the original item still exists, so the dropped item
    // needs a new ID. remove_item signals this by returning an item with id=-1
    if (item.id == 0xFFFFFFFF) {
      item.id = l->generate_item_id(c->lobby_client_id);
    }

    // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add
    // the item back to the player's inventory to correct for this (it will get
    // removed again by the 6x29 handler)
    p->add_item(item);

    l->add_item(item, cmd.area, cmd.x, cmd.z);

    auto name = item.name(false);
    l->log.info("Player %hu split stack %08" PRIX32 " (removed: %s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(),
        cmd.area.load(), cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5SPLIT/BB %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    p->print_inventory(stderr);

    send_drop_stacked_item(l, item, cmd.area, cmd.x, cmd.z);

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_buy_shop_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_BuyShopItem_6x5E>(data, size);

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  if (l->base_version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    {
      ItemData item = cmd.item_data;
      if (c->version() == GameVersion::GC) {
        item.bswap_data2_if_mag();
      }
      p->add_item(item);
    }

    size_t price = s->item_parameter_table->price_for_item(cmd.item_data);
    auto name = cmd.item_data.name(false);
    l->log.info("Player %hu bought item %08" PRIX32 " (%s) from shop (%zu Meseta)",
        cmd.header.client_id.load(), cmd.item_data.id.load(), name.c_str(), price);
    if (c->options.debug) {
      string name = cmd.item_data.name(true);
      send_text_message_printf(c, "$C5BUY %08" PRIX32 "\n%s", cmd.item_data.id.load(), name.c_str());
    }
    p->remove_meseta(price, c->version() != GameVersion::BB);
    p->print_inventory(stderr);
  }

  forward_subcommand_with_mag_bswap_t(c, command, flag, cmd);
}

template <typename CmdT>
static void on_box_or_enemy_item_drop_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game() || (c->lobby_client_id != l->leader_id)) {
    return;
  }
  if (l->base_version == GameVersion::BB) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    {
      ItemData item = cmd.item_data;
      if (c->version() == GameVersion::GC) {
        item.bswap_data2_if_mag();
      }
      l->add_item(item, cmd.area, cmd.x, cmd.z);
    }

    auto name = cmd.item_data.name(false);
    l->log.info("Leader created ground item %08" PRIX32 " (%s) at %hhu:(%g, %g)",
        cmd.item_data.id.load(), name.c_str(), cmd.area, cmd.x.load(), cmd.z.load());
    if (c->options.debug) {
      string name = cmd.item_data.name(true);
      send_text_message_printf(c, "$C5DROP %08" PRIX32 "\n%s", cmd.item_data.id.load(), name.c_str());
    }
  }

  forward_subcommand_with_mag_bswap_t(c, command, flag, cmd);
}

static void on_box_or_enemy_item_drop(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (size == sizeof(G_DropItem_DC_6x5F)) {
    on_box_or_enemy_item_drop_t<G_DropItem_DC_6x5F>(c, command, flag, data, size);
  } else if (size == sizeof(G_DropItem_PC_V3_BB_6x5F)) {
    on_box_or_enemy_item_drop_t<G_DropItem_PC_V3_BB_6x5F>(c, command, flag, data, size);
  } else {
    throw runtime_error("invalid size for 6x5F command");
  }
}

static void on_pick_up_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto& cmd = check_size_t<G_PickUpItem_6x59>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (l->base_version == GameVersion::BB) {
    // BB clients should never send this; only the server should send this
    return;
  }

  auto effective_c = l->clients.at(cmd.header.client_id);
  if (!effective_c.get()) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto effective_p = effective_c->game_data.player();
    auto item = l->remove_item(cmd.item_id);
    effective_p->add_item(item);

    auto name = item.name(false);
    l->log.info("Player %hu picked up %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5PICK %08" PRIX32 "\n%s", cmd.item_id.load(), name.c_str());
    }
    effective_p->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_pick_up_item_request(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  // This is handled by the server on BB, and by the leader on other versions
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    auto& cmd = check_size_t<G_PickUpItemRequest_6x5A>(data, size);

    if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto p = c->game_data.player();
    auto item = l->remove_item(cmd.item_id);
    p->add_item(item);

    auto name = item.name(false);
    l->log.info("Player %hu picked up %08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5PICK/BB %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    p->print_inventory(stderr);

    send_pick_up_item(c, cmd.item_id, cmd.area);

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_equip_unequip_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_EquipOrUnequipItem_6x25_6x26>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    size_t index = p->inventory.find_item(cmd.item_id);
    if (cmd.header.subcommand == 0x25) { // Equip
      p->inventory.items[index].flags |= 0x00000008;
    } else { // Unequip
      p->inventory.items[index].flags &= 0xFFFFFFF7;
    }
  } else if (l->base_version == GameVersion::BB) {
    throw logic_error("item tracking not enabled in BB game");
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_use_item(
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    const void* data, size_t size) {
  const auto& cmd = check_size_t<G_UseItem_6x27>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    size_t index = p->inventory.find_item(cmd.item_id);
    string name, colored_name;
    {
      // Note: We do this weird scoping thing because player_use_item will
      // likely delete the item, which will break the reference here.
      const auto& item = p->inventory.items[index].data;
      name = item.name(false);
      colored_name = item.name(true);
    }
    player_use_item(c, index);

    l->log.info("Player used item %hu:%08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      send_text_message_printf(c, "$C5USE %08" PRIX32 "\n%s",
          cmd.item_id.load(), colored_name.c_str());
    }
    p->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_feed_mag(
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    const void* data, size_t size) {
  const auto& cmd = check_size_t<G_FeedMAG_6x28>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();

    size_t mag_index = p->inventory.find_item(cmd.mag_item_id);
    size_t fed_index = p->inventory.find_item(cmd.fed_item_id);
    string mag_name, mag_colored_name, fed_name, fed_colored_name;
    {
      // Note: We do this weird scoping thing because player_use_item will
      // likely delete the item, which will break the reference here.
      const auto& fed_item = p->inventory.items[fed_index].data;
      fed_name = fed_item.name(false);
      fed_colored_name = fed_item.name(true);
      const auto& mag_item = p->inventory.items[mag_index].data;
      mag_name = mag_item.name(false);
      mag_colored_name = mag_item.name(true);
    }
    player_feed_mag(c, mag_index, fed_index);

    // On BB, the player only sends a 6x28; on other versions, the player sends
    // a 6x29 immediately after to destroy the fed item. So on BB, we should
    // remove the fed item here, but on other versions, we allow the following
    // 6x29 command to do that.
    if (l->base_version == GameVersion::BB) {
      p->remove_item(cmd.fed_item_id, 1, false);
    }

    l->log.info("Player fed item %hu:%08" PRIX32 " (%s) to mag %hu:%08" PRIX32 " (%s)",
        cmd.header.client_id.load(), cmd.fed_item_id.load(), fed_name.c_str(),
        cmd.header.client_id.load(), cmd.mag_item_id.load(), mag_name.c_str());
    if (c->options.debug) {
      send_text_message_printf(c, "$C5FEED %08" PRIX32 "\n%s\n...TO %08" PRIX32 "\n%s",
          cmd.fed_item_id.load(), fed_colored_name.c_str(),
          cmd.mag_item_id.load(), mag_colored_name.c_str());
    }
    p->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_open_shop_bb_or_ep3_battle_subs(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_ep3()) {
    on_ep3_battle_subs(c, command, flag, data, size);

  } else if (!l->item_creator.get()) {
    throw runtime_error("received shop subcommand without item creator present");

  } else {
    const auto& cmd = check_size_t<G_ShopContentsRequest_BB_6xB5>(data, size);
    if ((l->base_version == GameVersion::BB) && l->is_game()) {
      if (!l->item_creator) {
        throw logic_error("item creator missing from BB game");
      }

      auto s = c->require_server_state();
      size_t level = c->game_data.player()->disp.stats.level + 1;
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

static void on_open_bank_bb_or_card_trade_counter_ep3(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if ((l->base_version == GameVersion::BB) && l->is_game()) {
    send_bank(c);
  } else if ((l->base_version == GameVersion::GC) && l->is_ep3()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_ep3_private_word_select_bb_bank_action(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_BankAction_BB_6xBD>(data, size);

    if (!l->is_game()) {
      return;
    }

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto p = c->game_data.player();
    if (cmd.action == 0) { // deposit
      if (cmd.item_id == 0xFFFFFFFF) { // meseta
        if (cmd.meseta_amount > p->disp.stats.meseta) {
          return;
        }
        if ((p->bank.meseta + cmd.meseta_amount) > 999999) {
          return;
        }
        p->bank.meseta += cmd.meseta_amount;
        p->disp.stats.meseta -= cmd.meseta_amount;
      } else { // item
        auto item = p->remove_item(cmd.item_id, cmd.item_amount, c->version() != GameVersion::BB);
        p->bank.add_item(item);
        send_destroy_item(c, cmd.item_id, cmd.item_amount);
      }
    } else if (cmd.action == 1) { // take
      if (cmd.item_id == 0xFFFFFFFF) { // meseta
        if (cmd.meseta_amount > p->bank.meseta) {
          return;
        }
        if ((p->disp.stats.meseta + cmd.meseta_amount) > 999999) {
          return;
        }
        p->bank.meseta -= cmd.meseta_amount;
        p->disp.stats.meseta += cmd.meseta_amount;
      } else { // item
        auto item = p->bank.remove_item(cmd.item_id, cmd.item_amount);
        item.id = l->generate_item_id(0xFF);
        p->add_item(item);
        send_create_inventory_item(c, item);
      }
    }

  } else if ((c->version() == GameVersion::GC) && (c->flags & Client::Flag::IS_EPISODE_3)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_sort_inventory_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_SortInventory_BB_6xC4>(data, size);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto p = c->game_data.player();

    // Make sure the set of item IDs passed in by the client exactly matches the
    // set of item IDs present in the inventory
    unordered_set<uint32_t> sorted_item_ids;
    size_t expected_count = 0;
    for (size_t x = 0; x < 30; x++) {
      if (cmd.item_ids[x] != 0xFFFFFFFF) {
        sorted_item_ids.emplace(cmd.item_ids[x]);
        expected_count++;
      }
    }
    if (sorted_item_ids.size() != expected_count) {
      throw runtime_error("sorted array contains duplicate item IDs");
    }
    if (sorted_item_ids.size() != p->inventory.num_items) {
      throw runtime_error("sorted array contains a different number of items than the inventory contains");
    }
    for (size_t x = 0; x < p->inventory.num_items; x++) {
      if (!sorted_item_ids.erase(cmd.item_ids[x])) {
        throw runtime_error("inventory contains item ID not present in sorted array");
      }
    }
    if (!sorted_item_ids.empty()) {
      throw runtime_error("sorted array contains item ID not present in inventory");
    }

    parray<PlayerInventoryItem, 30> sorted;
    for (size_t x = 0; x < 30; x++) {
      if (cmd.item_ids[x] == 0xFFFFFFFF) {
        sorted[x].data.id = 0xFFFFFFFF;
      } else {
        size_t index = p->inventory.find_item(cmd.item_ids[x]);
        sorted[x] = p->inventory.items[index];
      }
    }
    p->inventory.items = sorted;
  }
}

////////////////////////////////////////////////////////////////////////////////
// EXP/Drop Item commands

static void on_entity_drop_item_request(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  // If there is no item creator (that is, the game is BB or has server rare
  // tables disabled), then forward the request to the leader
  if (!l->item_creator) {
    if (l->flags & Lobby::Flag::DROPS_ENABLED) {
      forward_subcommand(c, command, flag, data, size);
    }
    return;
  }

  G_SpecializableItemDropRequest_6xA2 cmd;
  if (size == sizeof(G_SpecializableItemDropRequest_6xA2)) {
    cmd = check_size_t<G_SpecializableItemDropRequest_6xA2>(data, size);
  } else {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_DC_6x60>(
        data, size, 0xFFFF);
    cmd.entity_id = in_cmd.entity_id;
    cmd.area = in_cmd.area;
    cmd.rt_index = in_cmd.rt_index;
    cmd.x = in_cmd.x;
    cmd.z = in_cmd.z;
    cmd.ignore_def = true;
  }

  ItemData item;
  if (cmd.rt_index == 0x30) {
    if (cmd.ignore_def) {
      item = l->item_creator->on_box_item_drop(cmd.area);
    } else {
      item = l->item_creator->on_specialized_box_item_drop(cmd.def[0], cmd.def[1], cmd.def[2]);
    }
  } else {
    if (l->map) {
      const auto& enemy = l->map->enemies.at(cmd.entity_id);
      uint32_t expected_rt_index = rare_table_index_for_enemy_type(enemy.type);
      if (cmd.rt_index != expected_rt_index) {
        c->log.warning("rt_index %02hhX from command does not match entity\'s expected index %02" PRIX32,
            cmd.rt_index, expected_rt_index);
      }
    }
    item = l->item_creator->on_monster_item_drop(cmd.rt_index, cmd.area);
  }
  item.id = l->generate_item_id(0xFF);

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    l->add_item(item, cmd.area, cmd.x, cmd.z);
  }
  send_drop_item(l, item, cmd.rt_index != 0x30, cmd.area, cmd.x, cmd.z, cmd.entity_id);
}

static void on_set_quest_flag(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  uint16_t flag_index, difficulty, action;
  if (c->version() == GameVersion::DC || c->version() == GameVersion::PC) {
    const auto& cmd = check_size_t<G_SetQuestFlag_DC_PC_6x75>(data, size);
    flag_index = cmd.flag;
    action = cmd.action;
    difficulty = l->difficulty;
  } else {
    const auto& cmd = check_size_t<G_SetQuestFlag_V3_BB_6x75>(data, size);
    flag_index = cmd.basic_cmd.flag;
    action = cmd.basic_cmd.action;
    difficulty = cmd.difficulty;
  }

  if (flag_index >= 0x400) {
    return;
  }

  // TODO: Should we allow overlays here?
  auto p = c->game_data.player(true, false);

  // The client explicitly checks for both 0 and 1 - any other value means no
  // operation is performed.
  size_t bit_index = (difficulty << 10) + flag_index;
  size_t byte_index = bit_index >> 3;
  uint8_t mask = 0x80 >> (bit_index & 7);
  if (action == 0) {
    p->quest_data1[byte_index] |= mask;
  } else if (action == 1) {
    p->quest_data1[byte_index] &= (~mask);
  }

  forward_subcommand(c, command, flag, data, size);

  if (c->version() == GameVersion::GC) {
    bool should_send_boss_drop_req = false;
    bool is_ep2 = (l->episode == Episode::EP2);
    if ((l->episode == Episode::EP1) && (c->area == 0x0E)) {
      // On Normal, Dark Falz does not have a third phase, so send the drop
      // request after the end of the second phase. On all other difficulty
      // levels, send it after the third phase.
      if (((difficulty == 0) && (flag_index == 0x0035)) ||
          ((difficulty != 0) && (flag_index == 0x0037))) {
        should_send_boss_drop_req = true;
      }
    } else if (is_ep2 && (flag_index == 0x0057) && (c->area == 0x0D)) {
      should_send_boss_drop_req = true;
    }

    if (should_send_boss_drop_req) {
      auto c = l->clients.at(l->leader_id);
      if (c) {
        G_StandardDropItemRequest_PC_V3_BB_6x60 req = {
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
}

static void on_enemy_hit(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_EnemyHitByPlayer_6x0A>(data, size);

    if (!l->is_game()) {
      return;
    }
    if (c->lobby_client_id > 3) {
      throw logic_error("client ID is above 3");
    }
    if (!l->map) {
      throw runtime_error("game does not have a map loaded");
    }
    if (cmd.enemy_id >= l->map->enemies.size()) {
      return;
    }

    auto& enemy = l->map->enemies[cmd.enemy_id];
    if (enemy.flags & Map::Enemy::Flag::DEFEATED) {
      return;
    }
    enemy.flags |= (Map::Enemy::Flag::HIT_BY_PLAYER0 << c->lobby_client_id);
    enemy.last_hit_by_client_id = c->lobby_client_id;
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_charge_attack_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version != GameVersion::BB) {
    throw runtime_error("BB-only command sent in non-BB game");
  }

  forward_subcommand(c, command, flag, data, size);

  const auto& cmd = check_size_t<G_ChargeAttack_BB_6xC7>(data, size);
  auto& disp = c->game_data.player()->disp;
  if (cmd.meseta_amount > disp.stats.meseta) {
    disp.stats.meseta = 0;
  } else {
    disp.stats.meseta -= cmd.meseta_amount;
  }
}

static void add_player_exp(shared_ptr<Client> c, uint32_t exp) {
  auto s = c->require_server_state();
  auto p = c->game_data.player();

  p->disp.stats.experience += exp;
  send_give_experience(c, exp);

  bool leveled_up = false;
  do {
    const auto& level = s->level_table->stats_delta_for_level(
        p->disp.visual.char_class, p->disp.stats.level + 1);
    if (p->disp.stats.experience >= level.experience) {
      leveled_up = true;
      level.apply(p->disp.stats.char_stats);
      p->disp.stats.level++;
    } else {
      break;
    }
  } while (p->disp.stats.level < 199);
  if (leveled_up) {
    send_level_up(c);
  }
}

static void on_steal_exp_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  if (l->base_version != GameVersion::BB) {
    throw runtime_error("BB-only command sent in non-BB game");
  }
  if (!l->map) {
    throw runtime_error("map not loaded");
  }

  const auto& cmd = check_size_t<G_StealEXP_BB_6xC6>(data, size);

  auto p = c->game_data.player();
  const auto& enemy = l->map->enemies.at(cmd.enemy_id);
  const auto& inventory = p->inventory;
  const auto& weapon = inventory.items[inventory.find_equipped_weapon()];

  uint8_t special = 0;
  if (((weapon.data.data1[1] < 0x0A) && (weapon.data.data1[2] < 0x05)) ||
      ((weapon.data.data1[1] < 0x0D) && (weapon.data.data1[2] < 0x04))) {
    special = weapon.data.data1[4] & 0x3F;
  } else {
    special = s->item_parameter_table->get_weapon(weapon.data.data1[1], weapon.data.data1[2]).special;
  }

  if (special >= 0x09 && special <= 0x0B) {
    // Master's = 8, Lord's = 10, King's = 12
    uint32_t percent = 8 + ((special - 9) << 1) + (char_class_is_android(p->disp.visual.char_class) ? 30 : 0);
    uint32_t enemy_exp = s->battle_params->get(l->mode == GameMode::SOLO, l->episode, l->difficulty, enemy.type).experience;
    uint32_t stolen_exp = min<uint32_t>((enemy_exp * percent) / 100, 80);
    if (c->options.debug) {
      send_text_message_printf(c, "$C5+%" PRIu32 " E-%hX %s",
          stolen_exp, cmd.enemy_id.load(), name_for_enum(enemy.type));
    }
    add_player_exp(c, stolen_exp);
  }
}

static void on_enemy_killed_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  if (l->base_version != GameVersion::BB) {
    throw runtime_error("BB-only command sent in non-BB game");
  }

  forward_subcommand(c, command, flag, data, size);

  const auto& cmd = check_size_t<G_EnemyKilled_BB_6xC8>(data, size);

  if (!l->is_game()) {
    throw runtime_error("client should not kill enemies outside of games");
  }
  if (!l->map) {
    throw runtime_error("game does not have a map loaded");
  }
  if (cmd.enemy_id >= l->map->enemies.size()) {
    send_text_message(c, u"$C6Missing enemy killed");
    return;
  }

  auto& e = l->map->enemies[cmd.enemy_id];
  string e_str = e.str();
  c->log.info("Enemy killed: E-%hX => %s", cmd.enemy_id.load(), e_str.c_str());
  if (e.flags & Map::Enemy::Flag::DEFEATED) {
    if (c->options.debug) {
      send_text_message_printf(c, "$C5E-%hX __DEFEATED__", cmd.enemy_id.load());
    }
    return;
  }

  uint32_t experience = 0xFFFFFFFF;
  try {
    experience = s->battle_params->get(l->mode == GameMode::SOLO, l->episode, l->difficulty, e.type).experience * l->exp_multiplier;
  } catch (const exception& e) {
    if (c->options.debug) {
      send_text_message_printf(c, "$C5E-%hX __MISSING__\n%s", cmd.enemy_id.load(), e.what());
    } else {
      send_text_message_printf(c, "$C4Unknown enemy type killed:\n%s", e.what());
    }
  }

  e.flags |= Map::Enemy::Flag::DEFEATED;
  for (size_t x = 0; x < l->max_clients; x++) {
    if (!((e.flags >> x) & 1)) {
      continue; // Player did not hit this enemy
    }

    auto other_c = l->clients[x];
    if (!other_c) {
      continue; // No player
    }
    if (other_c->game_data.player()->disp.stats.level >= 199) {
      continue; // Player is level 200 or higher
    }

    if (experience != 0xFFFFFFFF) {
      // Killer gets full experience, others get 77%
      bool is_killer = (e.last_hit_by_client_id == other_c->lobby_client_id);
      uint32_t player_exp = is_killer ? experience : ((experience * 77) / 100);
      if (c->options.debug) {
        send_text_message_printf(c, "$C5+%" PRIu32 " E-%hX %s",
            player_exp, cmd.enemy_id.load(), name_for_enum(e.type));
      }
      add_player_exp(c, player_exp);
    }
  }

  // Update kill counts on unsealable items
  auto& inventory = c->game_data.player()->inventory;
  for (size_t z = 0; z < inventory.num_items; z++) {
    auto& item = inventory.items[z];
    if ((item.flags & 0x08) &&
        s->item_parameter_table->is_unsealable_item(item.data)) {
      item.data.set_sealed_item_kill_count(item.data.get_sealed_item_kill_count() + 1);
    }
  }
}

void on_meseta_reward_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_MesetaRewardRequest_BB_6xC9>(data, size);

  auto p = c->game_data.player();
  if (cmd.amount < 0) {
    if (-cmd.amount > static_cast<int32_t>(p->disp.stats.meseta.load())) {
      p->disp.stats.meseta = 0;
    } else {
      p->disp.stats.meseta += cmd.amount;
    }
  } else if (cmd.amount > 0) {
    auto l = c->require_lobby();

    ItemData item;
    item.data1[0] = 0x04;
    item.data2d = cmd.amount.load();
    item.id = l->generate_item_id(0xFF);
    p->add_item(item);
    send_create_inventory_item(c, item);
  }
}

void on_item_reward_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ItemRewardRequest_BB_6xCA>(data, size);
  auto l = c->require_lobby();

  ItemData item;
  item = cmd.item_data;
  item.id = l->generate_item_id(0xFF);
  c->game_data.player()->add_item(item);
  send_create_inventory_item(c, item);
}

static void on_destroy_inventory_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_DeleteInventoryItem_6x29>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto p = c->game_data.player();
    auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != GameVersion::BB);
    auto name = item.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " destroyed (%s)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5DESTROY %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    p->print_inventory(stderr);
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_destroy_ground_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_DestroyGroundItem_6x63>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  if (l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED) {
    auto item = l->remove_item(cmd.item_id);
    auto name = item.name(false);
    l->log.info("Ground item %08" PRIX32 " destroyed (%s)", cmd.item_id.load(),
        name.c_str());
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5DESTROY/GND %08" PRIX32 "\n%s",
          cmd.item_id.load(), name.c_str());
    }
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_IdentifyItemRequest_6xB8>(data, size);
    if (!l->is_game()) {
      return;
    }
    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }
    if (!l->item_creator.get()) {
      throw logic_error("received item identify subcommand without item creator present");
    }

    auto p = c->game_data.player();
    size_t x = p->inventory.find_item(cmd.item_id);
    if (p->inventory.items[x].data.data1[0] != 0) {
      return; // Only weapons can be identified
    }

    p->disp.stats.meseta -= 100;
    c->game_data.identify_result = p->inventory.items[x].data;
    c->game_data.identify_result.data1[4] &= 0x7F;
    l->item_creator->apply_tekker_deltas(c->game_data.identify_result, p->disp.visual.section_id);
    send_item_identify_result(c);

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_accept_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_AcceptItemIdentification_BB_6xBA>(data, size);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    if (!c->game_data.identify_result.id || (c->game_data.identify_result.id == 0xFFFFFFFF)) {
      throw runtime_error("no identify result present");
    }
    if (c->game_data.identify_result.id != cmd.item_id) {
      throw runtime_error("accepted item ID does not match previous identify request");
    }
    c->game_data.player()->add_item(c->game_data.identify_result);
    send_create_inventory_item(c, c->game_data.identify_result);
    c->game_data.identify_result.clear();

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_sell_item_at_shop_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_SellItemAtShop_BB_6xC0>(data, size);

    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    auto p = c->game_data.player();
    auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != GameVersion::BB);
    size_t price = (s->item_parameter_table->price_for_item(item) >> 3) * cmd.amount;
    p->add_meseta(price);

    auto name = item.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " (%s) destroyed via sale (%zu Meseta)",
        c->lobby_client_id, cmd.item_id.load(), name.c_str(), price);
    p->print_inventory(stderr);
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5DESTROY/SELL %08" PRIX32 "\n+%zu Meseta\n%s",
          cmd.item_id.load(), price, name.c_str());
    }

    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_buy_shop_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == GameVersion::BB) {
    const auto& cmd = check_size_t<G_BuyShopItem_BB_6xB7>(data, size);
    if (!(l->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
      throw logic_error("item tracking not enabled in BB game");
    }

    ItemData item;
    item = c->game_data.shop_contents.at(cmd.shop_type).at(cmd.item_index);
    if (item.is_stackable()) {
      item.data1[5] = cmd.amount;
    } else if (cmd.amount != 1) {
      throw runtime_error("item is not stackable");
    }

    size_t price = item.data2d * cmd.amount;
    item.data2d = 0;
    auto p = c->game_data.player();
    p->remove_meseta(price, false);

    item.id = cmd.inventory_item_id;
    p->add_item(item);
    send_create_inventory_item(c, item);

    auto name = item.name(false);
    l->log.info("Inventory item %hu:%08" PRIX32 " created via purchase (%s) for %zu meseta",
        c->lobby_client_id, cmd.inventory_item_id.load(), name.c_str(), price);
    p->print_inventory(stderr);
    if (c->options.debug) {
      string name = item.name(true);
      send_text_message_printf(c, "$C5CREATE/BUY %08" PRIX32 "\n-%zu Meseta\n%s",
          cmd.inventory_item_id.load(), price, name.c_str());
    }
  }
}

static void on_medical_center_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void*, size_t) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB)) {
    c->game_data.player()->remove_meseta(10, false);
  }
}

static void on_battle_restart_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == GameVersion::BB) &&
      (l->mode == GameMode::BATTLE) &&
      (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_RestartBattle_BB_6xCF>(data, size);

    shared_ptr<BattleRules> new_rules(new BattleRules(cmd.rules));
    if (l->item_creator) {
      l->item_creator->set_restrictions(new_rules);
    }

    for (auto& lc : l->clients) {
      if (lc) {
        lc->game_data.delete_overlay();
        lc->game_data.create_battle_overlay(new_rules, s->level_table);
      }
    }
    l->map->clear();
  }
}

static void on_battle_level_up_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == GameVersion::BB) &&
      (l->mode == GameMode::BATTLE) &&
      (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_BattleModeLevelUp_BB_6xD0>(data, size);
    auto lc = l->clients[cmd.header.client_id];
    if (lc) {
      auto s = c->require_server_state();
      auto lp = lc->game_data.player();
      uint32_t target_level = lp->disp.stats.level + cmd.num_levels;
      uint32_t before_exp = lp->disp.stats.experience;
      lp->disp.stats.advance_to_level(lp->disp.visual.char_class, target_level, s->level_table);
      send_give_experience(lc, lp->disp.stats.experience - before_exp);
      send_level_up(lc);
    }
  }
}

static void on_quest_exchange_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == GameVersion::BB) &&
      (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_ExchangeItemInQuest_BB_6xD5>(data, size);

    try {
      auto p = c->game_data.player();

      size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, false);
      send_destroy_item(c, found_item.id, 1);

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.replace_item;
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item(c, new_item);

      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Quest item exchange failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_wrap_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB)) {
    const auto& cmd = check_size_t<G_WrapItem_BB_6xD6>(data, size);

    auto p = c->game_data.player();
    auto item = p->remove_item(cmd.item.id, 1, false);
    send_destroy_item(c, item.id, 1);
    item.wrap();
    p->add_item(cmd.item);
    send_create_inventory_item(c, item);
  }
}

static void on_photon_drop_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB)) {
    const auto& cmd = check_size_t<G_PaganiniPhotonDropExchange_BB_6xD7>(data, size);

    try {
      auto p = c->game_data.player();

      size_t found_index = p->inventory.find_item_by_primary_identifier(0x031000);
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 0, false);
      send_destroy_item(c, found_item.id, found_item.stack_size());

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.new_item;
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item(c, new_item);

      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Quest Photon Drop exchange failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_photon_crystal_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB) && (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    check_size_t<G_BlackPaperDealPhotonCrystalExchange_BB_6xDF>(data, size);
    auto p = c->game_data.player();
    size_t index = p->inventory.find_item_by_primary_identifier(0x031002);
    auto item = p->remove_item(p->inventory.items[index].data.id, 1, false);
    send_destroy_item(c, item.id, 1);
    // TODO: Should we disable drops here?
  }
}

static void on_momoka_item_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB) && (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_MomokaItemExchange_BB_6xD9>(data, size);
    auto p = c->game_data.player();
    try {
      size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, false);

      G_ExchangeItemInQuest_BB_6xDB cmd_6xDB = {{0xDB, 0x04, c->lobby_client_id}, 1, found_item.id, 1};
      send_command_t(c, 0x60, 0x00, cmd_6xDB);

      send_destroy_item(c, found_item.id, 1);

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.replace_item;
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item(c, new_item);

      send_command(c, 0x23, 0x00);
    } catch (const exception& e) {
      c->log.warning("Momoka item exchange failed: %s", e.what());
      send_command(c, 0x23, 0x01);
    }
  }
}

static void on_upgrade_weapon_attribute_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == GameVersion::BB) && (l->flags & Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_UpgradeWeaponAttribute_BB_6xDA>(data, size);
    auto p = c->game_data.player();
    try {
      size_t item_index = p->inventory.find_item(cmd.item_id);
      auto& item = p->inventory.items[item_index].data;

      uint32_t payment_primary_identifier = cmd.payment_type ? 0x031001 : 0x031000;
      size_t payment_index = p->inventory.find_item_by_primary_identifier(payment_primary_identifier);
      auto& payment_item = p->inventory.items[payment_index].data;
      if (payment_item.stack_size() < cmd.payment_count) {
        throw runtime_error("not enough payment items present");
      }
      p->remove_item(payment_item.id, cmd.payment_count, false);
      send_destroy_item(c, payment_item.id, cmd.payment_count);

      uint8_t attribute_amount = 0;
      if (cmd.payment_type == 1 && cmd.payment_count == 1) {
        attribute_amount = 30;
      } else if (cmd.payment_type == 0 && cmd.payment_count == 4) {
        attribute_amount = 1;
      } else if (cmd.payment_type == 1 && cmd.payment_count == 20) {
        attribute_amount = 5;
      } else {
        throw runtime_error("unknown PD/PS expenditure");
      }

      size_t attribute_index = 0;
      for (size_t z = 6; z <= 10; z += 2) {
        if (!(item.data1[z] & 0x80) && (item.data1[z] == cmd.attribute)) {
          attribute_index = z;
        } else if (item.data1[z] == 0) {
          attribute_index = z;
        }
      }
      if (attribute_index == 0) {
        throw runtime_error("no available attribute slots");
      }
      item.data1[attribute_index] = cmd.attribute;
      item.data1[attribute_index] += attribute_amount;

      send_destroy_item(c, item.id, 1);
      send_create_inventory_item(c, item);
      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Weapon attribute upgrade failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

typedef void (*subcommand_handler_t)(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size);

subcommand_handler_t subcommand_handlers[0x100] = {
    /* 6x00 */ on_invalid,
    /* 6x01 */ nullptr,
    /* 6x02 */ nullptr,
    /* 6x03 */ nullptr,
    /* 6x04 */ nullptr,
    /* 6x05 */ on_switch_state_changed,
    /* 6x06 */ on_send_guild_card,
    /* 6x07 */ on_symbol_chat,
    /* 6x08 */ nullptr,
    /* 6x09 */ nullptr,
    /* 6x0A */ on_enemy_hit,
    /* 6x0B */ on_forward_check_size_game,
    /* 6x0C */ on_forward_check_size_game,
    /* 6x0D */ on_forward_check_size_game,
    /* 6x0E */ nullptr,
    /* 6x0F */ nullptr,
    /* 6x10 */ nullptr,
    /* 6x11 */ nullptr,
    /* 6x12 */ on_forward_check_size_game,
    /* 6x13 */ on_forward_check_size_game,
    /* 6x14 */ on_forward_check_size_game,
    /* 6x15 */ on_forward_check_size_game,
    /* 6x16 */ on_forward_check_size_game,
    /* 6x17 */ on_forward_check_size_game,
    /* 6x18 */ on_forward_check_size_game,
    /* 6x19 */ on_forward_check_size_game,
    /* 6x1A */ nullptr,
    /* 6x1B */ nullptr,
    /* 6x1C */ on_forward_check_size_game,
    /* 6x1D */ nullptr,
    /* 6x1E */ nullptr,
    /* 6x1F */ on_change_area<G_SetPlayerArea_6x1F>,
    /* 6x20 */ on_movement_with_area<G_SetPosition_6x20>,
    /* 6x21 */ on_change_area<G_InterLevelWarp_6x21>,
    /* 6x22 */ on_forward_check_size_client,
    /* 6x23 */ on_set_player_visibility,
    /* 6x24 */ on_forward_check_size_game,
    /* 6x25 */ on_equip_unequip_item,
    /* 6x26 */ on_equip_unequip_item,
    /* 6x27 */ on_use_item,
    /* 6x28 */ on_feed_mag,
    /* 6x29 */ on_destroy_inventory_item,
    /* 6x2A */ on_player_drop_item,
    /* 6x2B */ on_create_inventory_item,
    /* 6x2C */ on_forward_check_size,
    /* 6x2D */ on_forward_check_size,
    /* 6x2E */ nullptr,
    /* 6x2F */ on_hit_by_enemy,
    /* 6x30 */ on_forward_check_size_game,
    /* 6x31 */ on_forward_check_size_game,
    /* 6x32 */ on_forward_check_size_game,
    /* 6x33 */ on_forward_check_size_game,
    /* 6x34 */ nullptr,
    /* 6x35 */ nullptr,
    /* 6x36 */ on_forward_check_game,
    /* 6x37 */ on_forward_check_size_game,
    /* 6x38 */ nullptr,
    /* 6x39 */ on_forward_check_size_game,
    /* 6x3A */ on_forward_check_size_game,
    /* 6x3B */ on_forward_check_size,
    /* 6x3C */ nullptr,
    /* 6x3D */ nullptr,
    /* 6x3E */ on_movement_with_area<G_StopAtPosition_6x3E>,
    /* 6x3F */ on_movement_with_area<G_SetPosition_6x3F>,
    /* 6x40 */ on_movement<G_WalkToPosition_6x40>,
    /* 6x41 */ nullptr,
    /* 6x42 */ on_movement<G_RunToPosition_6x42>,
    /* 6x43 */ on_forward_check_size_client,
    /* 6x44 */ on_forward_check_size_client,
    /* 6x45 */ on_forward_check_size_client,
    /* 6x46 */ on_attack_finished,
    /* 6x47 */ on_cast_technique,
    /* 6x48 */ on_cast_technique_finished,
    /* 6x49 */ on_subtract_pb_energy,
    /* 6x4A */ on_forward_check_size_client,
    /* 6x4B */ on_hit_by_enemy,
    /* 6x4C */ on_hit_by_enemy,
    /* 6x4D */ on_player_died,
    /* 6x4E */ on_forward_check_size_client,
    /* 6x4F */ on_forward_check_size_client,
    /* 6x50 */ on_forward_check_size_client,
    /* 6x51 */ nullptr,
    /* 6x52 */ on_forward_check_size,
    /* 6x53 */ on_forward_check_size_game,
    /* 6x54 */ nullptr,
    /* 6x55 */ on_forward_check_size_client,
    /* 6x56 */ on_forward_check_size_client,
    /* 6x57 */ on_forward_check_size_client,
    /* 6x58 */ on_forward_check_size_client,
    /* 6x59 */ on_pick_up_item,
    /* 6x5A */ on_pick_up_item_request,
    /* 6x5B */ nullptr,
    /* 6x5C */ nullptr,
    /* 6x5D */ on_drop_partial_stack,
    /* 6x5E */ on_buy_shop_item,
    /* 6x5F */ on_box_or_enemy_item_drop,
    /* 6x60 */ on_entity_drop_item_request,
    /* 6x61 */ on_forward_check_size_game,
    /* 6x62 */ nullptr,
    /* 6x63 */ on_destroy_ground_item,
    /* 6x64 */ nullptr,
    /* 6x65 */ nullptr,
    /* 6x66 */ on_forward_check_size_game,
    /* 6x67 */ on_forward_check_size_game,
    /* 6x68 */ on_forward_check_size_game,
    /* 6x69 */ on_forward_check_size_game,
    /* 6x6A */ on_forward_check_size_game,
    /* 6x6B */ on_forward_sync_joining_player_state,
    /* 6x6C */ on_forward_sync_joining_player_state,
    /* 6x6D */ on_sync_joining_player_item_state,
    /* 6x6E */ on_forward_sync_joining_player_state,
    /* 6x6F */ on_forward_check_game_loading,
    /* 6x70 */ on_sync_joining_player_disp_and_inventory,
    /* 6x71 */ on_forward_check_game_loading,
    /* 6x72 */ on_forward_check_game_loading,
    /* 6x73 */ on_invalid,
    /* 6x74 */ on_word_select,
    /* 6x75 */ on_set_quest_flag,
    /* 6x76 */ on_forward_check_size_game,
    /* 6x77 */ on_forward_check_size_game,
    /* 6x78 */ nullptr,
    /* 6x79 */ on_forward_check_size,
    /* 6x7A */ nullptr,
    /* 6x7B */ nullptr,
    /* 6x7C */ on_forward_check_size_game,
    /* 6x7D */ on_forward_check_size_game,
    /* 6x7E */ nullptr,
    /* 6x7F */ nullptr,
    /* 6x80 */ on_forward_check_size_game,
    /* 6x81 */ nullptr,
    /* 6x82 */ nullptr,
    /* 6x83 */ on_forward_check_size_game,
    /* 6x84 */ on_forward_check_size_game,
    /* 6x85 */ on_forward_check_size_game,
    /* 6x86 */ on_forward_check_size_game,
    /* 6x87 */ on_forward_check_size_game,
    /* 6x88 */ on_forward_check_size_game,
    /* 6x89 */ on_forward_check_size_game,
    /* 6x8A */ on_forward_check_size_game,
    /* 6x8B */ nullptr,
    /* 6x8C */ nullptr,
    /* 6x8D */ on_forward_check_size_client,
    /* 6x8E */ nullptr,
    /* 6x8F */ nullptr,
    /* 6x90 */ nullptr,
    /* 6x91 */ on_forward_check_size_game,
    /* 6x92 */ nullptr,
    /* 6x93 */ on_forward_check_size_game,
    /* 6x94 */ on_forward_check_size_game,
    /* 6x95 */ nullptr,
    /* 6x96 */ nullptr,
    /* 6x97 */ nullptr,
    /* 6x98 */ nullptr,
    /* 6x99 */ nullptr,
    /* 6x9A */ on_forward_check_size_game,
    /* 6x9B */ nullptr,
    /* 6x9C */ on_forward_check_size_game,
    /* 6x9D */ nullptr,
    /* 6x9E */ nullptr,
    /* 6x9F */ on_forward_check_size_game,
    /* 6xA0 */ on_forward_check_size_game,
    /* 6xA1 */ on_forward_check_size_game,
    /* 6xA2 */ on_entity_drop_item_request,
    /* 6xA3 */ on_forward_check_size_game,
    /* 6xA4 */ on_forward_check_size_game,
    /* 6xA5 */ on_forward_check_size_game,
    /* 6xA6 */ on_forward_check_size,
    /* 6xA7 */ nullptr,
    /* 6xA8 */ on_forward_check_size_game,
    /* 6xA9 */ on_forward_check_size_game,
    /* 6xAA */ on_forward_check_size_game,
    /* 6xAB */ on_forward_check_size_client,
    /* 6xAC */ nullptr,
    /* 6xAD */ on_forward_check_size_game,
    /* 6xAE */ on_forward_check_size_client,
    /* 6xAF */ on_forward_check_size_client,
    /* 6xB0 */ on_forward_check_size_client,
    /* 6xB1 */ nullptr,
    /* 6xB2 */ nullptr,
    /* 6xB3 */ nullptr, // Should be sent via CA instead
    /* 6xB4 */ nullptr, // Should be sent by the server only
    /* 6xB5 */ on_open_shop_bb_or_ep3_battle_subs,
    /* 6xB6 */ nullptr,
    /* 6xB7 */ on_buy_shop_item_bb,
    /* 6xB8 */ on_identify_item_bb,
    /* 6xB9 */ nullptr,
    /* 6xBA */ on_accept_identify_item_bb,
    /* 6xBB */ on_open_bank_bb_or_card_trade_counter_ep3,
    /* 6xBC */ on_forward_check_size_ep3_game,
    /* 6xBD */ on_ep3_private_word_select_bb_bank_action,
    /* 6xBE */ on_ep3_sound_chat,
    /* 6xBF */ on_forward_check_size_ep3_lobby,
    /* 6xC0 */ on_sell_item_at_shop_bb,
    /* 6xC1 */ nullptr,
    /* 6xC2 */ nullptr,
    /* 6xC3 */ on_drop_partial_stack_bb,
    /* 6xC4 */ on_sort_inventory_bb,
    /* 6xC5 */ on_medical_center_bb,
    /* 6xC6 */ on_steal_exp_bb,
    /* 6xC7 */ on_charge_attack_bb,
    /* 6xC8 */ on_enemy_killed_bb,
    /* 6xC9 */ on_meseta_reward_request_bb,
    /* 6xCA */ on_item_reward_request_bb,
    /* 6xCB */ nullptr,
    /* 6xCC */ nullptr,
    /* 6xCD */ nullptr,
    /* 6xCE */ nullptr,
    /* 6xCF */ on_battle_restart_bb,
    /* 6xD0 */ on_battle_level_up_bb,
    /* 6xD1 */ nullptr,
    /* 6xD2 */ nullptr,
    /* 6xD3 */ nullptr,
    /* 6xD4 */ nullptr,
    /* 6xD5 */ on_quest_exchange_item_bb,
    /* 6xD6 */ on_wrap_item_bb,
    /* 6xD7 */ on_photon_drop_exchange_bb,
    /* 6xD8 */ nullptr,
    /* 6xD9 */ on_momoka_item_exchange_bb,
    /* 6xDA */ on_upgrade_weapon_attribute_bb,
    /* 6xDB */ nullptr,
    /* 6xDC */ nullptr,
    /* 6xDD */ nullptr,
    /* 6xDE */ nullptr,
    /* 6xDF */ on_photon_crystal_exchange_bb,
    /* 6xE0 */ nullptr,
    /* 6xE1 */ nullptr,
    /* 6xE2 */ nullptr,
    /* 6xE3 */ nullptr,
    /* 6xE4 */ nullptr,
    /* 6xE5 */ nullptr,
    /* 6xE6 */ nullptr,
    /* 6xE7 */ nullptr,
    /* 6xE8 */ nullptr,
    /* 6xE9 */ nullptr,
    /* 6xEA */ nullptr,
    /* 6xEB */ nullptr,
    /* 6xEC */ nullptr,
    /* 6xED */ nullptr,
    /* 6xEE */ nullptr,
    /* 6xEF */ nullptr,
    /* 6xF0 */ nullptr,
    /* 6xF1 */ nullptr,
    /* 6xF2 */ nullptr,
    /* 6xF3 */ nullptr,
    /* 6xF4 */ nullptr,
    /* 6xF5 */ nullptr,
    /* 6xF6 */ nullptr,
    /* 6xF7 */ nullptr,
    /* 6xF8 */ nullptr,
    /* 6xF9 */ nullptr,
    /* 6xFA */ nullptr,
    /* 6xFB */ nullptr,
    /* 6xFC */ nullptr,
    /* 6xFD */ nullptr,
    /* 6xFE */ nullptr,
    /* 6xFF */ nullptr,
};

void on_subcommand_multi(shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {
  if (data.empty()) {
    throw runtime_error("game command is empty");
  }
  if (c->version() == GameVersion::DC && (c->flags & (Client::Flag::IS_DC_TRIAL_EDITION | Client::Flag::IS_DC_V1_PROTOTYPE))) {
    // TODO: We should convert these to non-trial formats and vice versa
    forward_subcommand(c, command, flag, data.data(), data.size());
  } else {
    StringReader r(data);
    while (!r.eof()) {
      size_t size;
      const auto& header = r.get<G_UnusedHeader>(false);
      if (header.size != 0) {
        size = header.size << 2;
      } else {
        const auto& ext_header = r.get<G_ExtendedHeader<G_UnusedHeader>>(false);
        size = ext_header.size;
        if (size < 8) {
          throw runtime_error("extended subcommand header has size < 8");
        }
        if (size & 3) {
          throw runtime_error("extended subcommand size is not a multiple of 4");
        }
      }
      if (size == 0) {
        throw runtime_error("invalid subcommand size");
      }
      const void* data = r.getv(size);

      auto fn = subcommand_handlers[header.subcommand];
      if (fn) {
        fn(c, command, flag, data, size);
      } else {
        on_unimplemented(c, command, flag, data, size);
      }
    }
  }
}

bool subcommand_is_implemented(uint8_t which) {
  return subcommand_handlers[which] != nullptr;
}
