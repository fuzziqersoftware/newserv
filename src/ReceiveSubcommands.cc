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
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

using SubcommandHandler = void (*)(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size);

struct SubcommandDefinition {
  enum Flag {
    ALWAYS_FORWARD_TO_WATCHERS = 0x01,
    ALLOW_FORWARD_TO_WATCHED_LOBBY = 0x02,
  };
  uint8_t nte_subcommand;
  uint8_t proto_subcommand;
  uint8_t final_subcommand;
  SubcommandHandler handler;
  uint8_t flags = 0;
};
using SDF = SubcommandDefinition::Flag;

extern const SubcommandDefinition subcommand_definitions[0x100];

static const SubcommandDefinition& def_for_nte_subcommand(uint8_t subcommand) {
  static std::array<uint8_t, 0x100> nte_to_final_map;
  static bool nte_to_final_map_populated = false;
  if (!nte_to_final_map_populated) {
    for (size_t z = 0; z < 0x100; z++) {
      nte_to_final_map[z] = 0x00;
    }
    for (size_t z = 0; z < 0x100; z++) {
      const auto& def = subcommand_definitions[z];
      if (def.nte_subcommand != 0x00) {
        if (nte_to_final_map[def.nte_subcommand]) {
          throw logic_error("multiple NTE subcommands map to the same final subcommand");
        }
        nte_to_final_map[def.nte_subcommand] = z;
      }
    }
    nte_to_final_map_populated = true;
  }
  return subcommand_definitions[nte_to_final_map[subcommand]];
}

static const SubcommandDefinition& def_for_proto_subcommand(uint8_t subcommand) {
  static std::array<uint8_t, 0x100> proto_to_final_map;
  static bool proto_to_final_map_populated = false;
  if (!proto_to_final_map_populated) {
    for (size_t z = 0; z < 0x100; z++) {
      proto_to_final_map[z] = 0x00;
    }
    for (size_t z = 0; z < 0x100; z++) {
      const auto& def = subcommand_definitions[z];
      if (def.proto_subcommand != 0x00) {
        if (proto_to_final_map[def.proto_subcommand]) {
          throw logic_error("multiple prototype subcommands map to the same final subcommand");
        }
        proto_to_final_map[def.proto_subcommand] = z;
      }
    }
    proto_to_final_map_populated = true;
  }
  return subcommand_definitions[proto_to_final_map[subcommand]];
}

// The functions in this file are called when a client sends a game command
// (60, 62, 6C, 6D, C9, or CB).

// There are three different sets of subcommand numbers: the DC NTE set, the
// November 2000 prototype set, and the set used by all other versions of the
// game (starting from the December 2000 prototype, all the way through BB).
// Currently we do not support the November 2000 prototype, but we do support
// DC NTE. In general, DC NTE clients can only interact with non-NTE players in
// very limited ways, since most subcommand-based actions take place in games,
// and non-NTE players cannot join NTE games. Commands sent by DC NTE clients
// are not handled by the functions defined in subcommand_handlers, but are
// instead handled by handle_subcommand_dc_nte. This means we only have to
// consider sending to DC NTE clients in a small subset of the command handlers
// (those that can occur in the lobby), and we can skip sending most
// subcommands to DC NTE by default.

bool command_is_private(uint8_t command) {
  return (command == 0x62) || (command == 0x6D);
}

static void forward_subcommand(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  // If the command is an Ep3-only command, make sure an Ep3 client sent it
  bool command_is_ep3 = (command & 0xF0) == 0xC0;
  if (command_is_ep3 && !is_ep3(c->version())) {
    throw runtime_error("Episode 3 command sent by non-Episode 3 client");
  }

  auto l = c->lobby.lock();
  if (!l) {
    c->log.warning("Not in any lobby; dropping command");
    return;
  }

  auto& header = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  const SubcommandDefinition* def;
  if (c->version() == Version::DC_NTE) {
    def = &def_for_nte_subcommand(header.subcommand);
  } else if (c->version() == Version::DC_V1_11_2000_PROTOTYPE) {
    def = &def_for_proto_subcommand(header.subcommand);
  } else {
    def = &subcommand_definitions[header.subcommand];
  }

  string nte_data;
  string proto_data;
  string final_data;
  Version c_version = c->version();
  auto send_to_client = [&](shared_ptr<Client> lc) -> void {
    Version lc_version = lc->version();
    if (l->is_game() || (!is_pre_v1(lc_version) && !is_pre_v1(c_version)) || (lc_version == c_version)) {
      send_command(lc, command, flag, data, size);
    } else if (lc->version() == Version::DC_NTE) {
      if (def->nte_subcommand) {
        if (nte_data.empty()) {
          nte_data.assign(reinterpret_cast<const char*>(data), size);
          nte_data[0] = def->nte_subcommand;
        }
        send_command(lc, command, flag, nte_data);
      }
    } else if (lc->version() == Version::DC_V1_11_2000_PROTOTYPE) {
      if (def->proto_subcommand) {
        if (proto_data.empty()) {
          proto_data.assign(reinterpret_cast<const char*>(data), size);
          proto_data[0] = def->proto_subcommand;
        }
        send_command(lc, command, flag, proto_data);
      }
    } else {
      if (def->final_subcommand) {
        if (final_data.empty()) {
          final_data.assign(reinterpret_cast<const char*>(data), size);
          final_data[0] = def->final_subcommand;
        }
        send_command(lc, command, flag, final_data);
      }
    }
  };

  if (command_is_private(command)) {
    if (flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[flag];
    if (!target) {
      return;
    }
    send_to_client(target);

  } else {
    if (command_is_ep3) {
      for (auto& lc : l->clients) {
        if (!lc || (lc == c) || !is_ep3(lc->version())) {
          continue;
        }
        send_to_client(lc);
      }
      if ((command == 0xCB) &&
          l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) &&
          (def->flags & SDF::ALLOW_FORWARD_TO_WATCHED_LOBBY)) {
        auto watched_lobby = l->watched_lobby.lock();
        if (watched_lobby) {
          for (auto& lc : watched_lobby->clients) {
            if (lc && is_ep3(lc->version())) {
              send_to_client(lc);
            }
          }
        }
      }

    } else {
      for (auto& lc : l->clients) {
        if (lc && (lc != c)) {
          send_to_client(lc);
        }
      }
    }

    // Before battle, forward only chat commands to watcher lobbies; during
    // battle, forward everything to watcher lobbies. (This is necessary because
    // if we forward everything before battle, the blocking menu subcommands
    // cause the battle setup menu to appear in the spectator room, which looks
    // weird and is generally undesirable.)
    if ((l->ep3_server && (l->ep3_server->setup_phase != Episode3::SetupPhase::REGISTRATION)) ||
        (def->flags & SDF::ALWAYS_FORWARD_TO_WATCHERS)) {
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        for (auto& target : watcher_lobby->clients) {
          if (target && is_ep3(target->version())) {
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
  if ((c->version() == Version::DC_NTE) || c->version() == Version::DC_V1_11_2000_PROTOTYPE) {
    c->log.error("Unrecognized DC NTE/prototype subcommand: %02hhX", cmd.subcommand);
    forward_subcommand(c, command, flag, data, size);
  } else if (command_is_private(command)) {
    c->log.error("Invalid subcommand: %02hhX (private to %hhu)", cmd.subcommand, flag);
  } else {
    c->log.error("Invalid subcommand: %02hhX (public)", cmd.subcommand);
  }
}

static void on_unimplemented(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  if ((c->version() == Version::DC_NTE) || c->version() == Version::DC_V1_11_2000_PROTOTYPE) {
    c->log.error("Unimplemented DC NTE/prototype subcommand: %02hhX", cmd.subcommand);
    forward_subcommand(c, command, flag, data, size);
  } else {
    if (command_is_private(command)) {
      c->log.warning("Unknown subcommand: %02hhX (private to %hhu)", cmd.subcommand, flag);
    } else {
      c->log.warning("Unknown subcommand: %02hhX (public)", cmd.subcommand);
    }
    if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      send_text_message_printf(c, "$C5Sub 6x%02hhX missing", cmd.subcommand);
    }
  }
}

template <typename CmdT>
static void send_or_enqueue_joining_player_command(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& data) {
  if (c->game_join_command_queue) {
    c->log.info("Client not ready to receive join commands; adding to queue");
    auto& cmd = c->game_join_command_queue->emplace_back();
    cmd.command = command;
    cmd.flag = flag;
    cmd.data.assign(reinterpret_cast<const char*>(&data), sizeof(data));
  } else {
    send_command(c, command, flag, &data, sizeof(data));
  }
}

static void send_or_enqueue_joining_player_command(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (c->game_join_command_queue) {
    c->log.info("Client not ready to receive join commands; adding to queue");
    auto& cmd = c->game_join_command_queue->emplace_back();
    cmd.command = command;
    cmd.flag = flag;
    cmd.data.assign(reinterpret_cast<const char*>(data), size);
  } else {
    send_command(c, command, flag, data, size);
  }
}

static void on_forward_check_game_loading(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }
  if (command_is_private(command)) {
    auto target = l->clients.at(flag);
    if (target) {
      send_or_enqueue_joining_player_command(target, command, flag, data, size);
    }
  } else {
    for (auto lc : l->clients) {
      if (lc) {
        send_or_enqueue_joining_player_command(lc, command, flag, data, size);
      }
    }
  }
}

static void on_forward_sync_joining_player_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->any_client_loading()) {
    return;
  }

  if (is_pre_v1(c->version())) {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    size_t compressed_size = size - sizeof(cmd);
    if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      string decompressed = bc0_decompress(reinterpret_cast<const char*>(data) + sizeof(cmd), compressed_size);
      c->log.info("Decompressed sync data (%zX -> %zX bytes; expected %" PRIX32 "):",
          compressed_size, decompressed.size(), cmd.decompressed_size.load());
      print_data(stderr, decompressed);
    }
  } else {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    if (cmd.compressed_size > size - sizeof(cmd)) {
      throw runtime_error("compressed end offset is beyond end of command");
    }
    if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      string decompressed = bc0_decompress(reinterpret_cast<const char*>(data) + sizeof(cmd), cmd.compressed_size);
      c->log.info("Decompressed sync data (%" PRIX32 " -> %zX bytes; expected %" PRIX32 "):",
          cmd.compressed_size.load(), decompressed.size(), cmd.decompressed_size.load());
      print_data(stderr, decompressed);
    }
  }

  on_forward_check_game_loading(c, command, flag, data, size);
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
  if (flag >= l->max_clients) {
    return;
  }
  auto target = l->clients[flag];
  if (!target) {
    return;
  }

  string decompressed;
  size_t compressed_size;
  size_t decompressed_size;
  if (is_pre_v1(c->version())) {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    compressed_size = size - sizeof(cmd);
    decompressed_size = cmd.decompressed_size;
    decompressed = bc0_decompress(reinterpret_cast<const char*>(data) + sizeof(cmd), compressed_size);
  } else {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    compressed_size = cmd.compressed_size;
    decompressed_size = cmd.decompressed_size;
    if (compressed_size > size - sizeof(cmd)) {
      throw runtime_error("compressed end offset is beyond end of command");
    }
    decompressed = bc0_decompress(reinterpret_cast<const char*>(data) + sizeof(cmd), cmd.compressed_size);
  }
  if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info("Decompressed item sync data (%zX -> %zX bytes; expected %zX):",
        compressed_size, decompressed.size(), decompressed_size);
    print_data(stderr, decompressed);
  }

  if (decompressed.size() < sizeof(G_SyncItemState_6x6D_Decompressed)) {
    throw runtime_error(string_printf(
        "decompressed 6x6D data (0x%zX bytes) is too short for header (0x%zX bytes)",
        decompressed.size(), sizeof(G_SyncItemState_6x6D_Decompressed)));
  }
  auto* decompressed_cmd = reinterpret_cast<G_SyncItemState_6x6D_Decompressed*>(decompressed.data());

  size_t num_floor_items = 0;
  for (size_t z = 0; z < decompressed_cmd->floor_item_count_per_floor.size(); z++) {
    num_floor_items += decompressed_cmd->floor_item_count_per_floor[z];
  }

  size_t required_size = sizeof(G_SyncItemState_6x6D_Decompressed) + num_floor_items * sizeof(FloorItem);
  if (decompressed.size() < required_size) {
    throw runtime_error(string_printf(
        "decompressed 6x6D data (0x%zX bytes) is too short for all floor items (0x%zX bytes)",
        decompressed.size(), required_size));
  }

  size_t target_num_items = target->character()->inventory.num_items;
  for (size_t z = 0; z < 12; z++) {
    uint32_t client_next_id = decompressed_cmd->next_item_id_per_player[z];
    uint32_t server_next_id = l->next_item_id_for_client[z];
    if (client_next_id == server_next_id) {
      l->log.info("Next item ID for player %zu (%08" PRIX32 ") matches expected value", z, l->next_item_id_for_client[z]);
    } else if ((z == target->lobby_client_id) && (client_next_id == server_next_id - target_num_items)) {
      l->log.info("Next item ID for player %zu (%08" PRIX32 ") matches expected value before inventory item ID assignment (%08" PRIX32 ")", z, l->next_item_id_for_client[z], static_cast<uint32_t>(server_next_id - target_num_items));
    } else {
      l->log.warning("Next item ID for player %zu (%08" PRIX32 ") does not match expected value (%08" PRIX32 ")",
          z, decompressed_cmd->next_item_id_per_player[z].load(), l->next_item_id_for_client[z]);
    }
  }

  send_game_item_state(target);
}

static void on_sync_joining_player_disp_and_inventory(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();

  // In V1/V2 games, this command sometimes is sent after the new client has
  // finished loading, so we don't check l->any_client_loading() here.
  auto l = c->require_lobby();
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
  bool sender_is_bb = (c->version() == Version::BB_V4);
  bool target_is_bb = (target->version() == Version::BB_V4);
  if (sender_is_bb != target_is_bb) {
    // TODO: Figure out the BB 6x70 format and implement this
    throw runtime_error("6x70 command cannot be translated across BB boundary");
  }

  // We need to byteswap mags' data2 fields if exactly one of the sender and
  // recipient are PSO GC
  bool sender_is_gc = is_gc(c->version());
  bool target_is_gc = is_gc(target->version());
  if (target_is_gc == sender_is_gc) {
    send_or_enqueue_joining_player_command(target, command, flag, data, size);

  } else if (sender_is_gc) {
    // Convert the GC command to the XB command format. There are some extra
    // fields on the end, so we also need to adjust the size field in the
    // extended header to account for that.
    G_SyncPlayerDispAndInventory_XB_6x70 out_cmd = {check_size_t<G_SyncPlayerDispAndInventory_DC_PC_GC_6x70>(data, size), 0, 0, 0};
    out_cmd.header.size = sizeof(out_cmd);
    if (c->license->xb_user_id) {
      out_cmd.xb_user_id_high = static_cast<uint32_t>((c->license->xb_user_id >> 32) & 0xFFFFFFFF);
      out_cmd.xb_user_id_low = static_cast<uint32_t>(c->license->xb_user_id & 0xFFFFFFFF);
    } else {
      out_cmd.xb_user_id_high = 0xAE000000;
      out_cmd.xb_user_id_low = c->license->serial_number;
    }
    for (size_t z = 0; z < out_cmd.inventory.num_items; z++) {
      out_cmd.inventory.items[z].data.decode_for_version(c->version());
      out_cmd.inventory.items[z].data.encode_for_version(target->version(), s->item_parameter_table_for_version(target->version()));
    }
    send_or_enqueue_joining_player_command(target, command, flag, out_cmd);

  } else {
    // The XB command has some extra fields on the end; PSO GC will just ignore
    // them, so we don't bother to remove them.
    static_assert(
        sizeof(G_SyncPlayerDispAndInventory_DC_PC_GC_6x70) < sizeof(G_SyncPlayerDispAndInventory_XB_6x70),
        "GC 6x70 command is larger than XB 6x70 command");
    auto out_cmd = check_size_t<G_SyncPlayerDispAndInventory_XB_6x70>(data, size);
    for (size_t z = 0; z < out_cmd.inventory.num_items; z++) {
      out_cmd.inventory.items[z].data.decode_for_version(c->version());
      out_cmd.inventory.items[z].data.encode_for_version(target->version(), s->item_parameter_table_for_version(target->version()));
    }
    send_or_enqueue_joining_player_command(target, command, flag, out_cmd);
  }
}

static void on_forward_check_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  if (cmd.client_id == c->lobby_client_id) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_lobby_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_game_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_ep3_lobby(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && l->is_ep3()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_ep3_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (l->is_game() && l->is_ep3()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Ep3 subcommands

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
    set_mask_for_ep3_game_command(data.data(), data.size(), (random_object<uint32_t>() % 0xFF) + 1);
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
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2: {
      const auto& cmd = check_size_t<G_SendGuildCard_DC_6x06>(data, size);
      c->character(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      const auto& cmd = check_size_t<G_SendGuildCard_PC_6x06>(data, size);
      c->character(true, false)->guild_card.description = cmd.guild_card.description;
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_TRIAL_EDITION:
    case Version::GC_EP3: {
      const auto& cmd = check_size_t<G_SendGuildCard_GC_6x06>(data, size);
      c->character(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::XB_V3: {
      const auto& cmd = check_size_t<G_SendGuildCard_XB_6x06>(data, size);
      c->character(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::BB_V4:
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

template <bool SenderIsBigEndian>
static void on_word_select_t(shared_ptr<Client> c, uint8_t command, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_WordSelect_6x74<SenderIsBigEndian>>(data, size);
  if (c->can_chat && (cmd.client_id == c->lobby_client_id)) {
    if (command_is_private(command)) {
      return;
    }

    auto s = c->require_server_state();
    auto l = c->require_lobby();
    if (l->battle_record && l->battle_record->battle_in_progress()) {
      l->battle_record->add_command(Episode3::BattleRecord::Event::Type::GAME_COMMAND, data, size);
    }

    unordered_set<shared_ptr<Client>> target_clients;
    for (const auto& lc : l->clients) {
      if (lc) {
        target_clients.emplace(lc);
      }
    }
    for (const auto& watcher_l : l->watcher_lobbies) {
      for (const auto& lc : watcher_l->clients) {
        if (lc) {
          target_clients.emplace(lc);
        }
      }
    }
    target_clients.erase(c);

    // In non-Ep3 lobbies, Ep3 uses the Ep1&2 word select table.
    bool is_non_ep3_lobby = (l->episode != Episode::EP3);

    Version from_version = c->version();
    if (is_non_ep3_lobby && is_ep3(from_version)) {
      from_version = Version::GC_V3;
    }
    for (const auto& lc : target_clients) {
      try {
        Version lc_version = lc->version();
        if (is_non_ep3_lobby && is_ep3(lc_version)) {
          lc_version = Version::GC_V3;
        }

        uint8_t subcommand;
        if (lc->version() == Version::DC_NTE) {
          subcommand = 0x62;
        } else if (lc->version() == Version::DC_V1_11_2000_PROTOTYPE) {
          subcommand = 0x69;
        } else {
          subcommand = 0x74;
        }

        if (is_big_endian(lc->version())) {
          G_WordSelect_6x74<true> out_cmd = {
              subcommand, cmd.size, cmd.client_id.load(),
              s->word_select_table->translate(cmd.message, from_version, lc_version)};
          send_command_t(lc, 0x60, 0x00, out_cmd);
        } else {
          G_WordSelect_6x74<false> out_cmd = {
              subcommand, cmd.size, cmd.client_id.load(),
              s->word_select_table->translate(cmd.message, from_version, lc_version)};
          send_command_t(lc, 0x60, 0x00, out_cmd);
        }

      } catch (const exception& e) {
        string name = c->character()->disp.name.decode(c->language());
        lc->log.warning("Untranslatable Word Select message: %s", e.what());
        send_text_message_printf(lc, "$C4Untranslatable Word\nSelect message from\n%s", name.c_str());
      }
    }
  }
}

static void on_word_select(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (is_pre_v1(c->version())) {
    // The Word Select command is a different size in final vs. NTE and
    // proto, so handle that here by appending FFFFFFFF0000000000000000
    string effective_data(reinterpret_cast<const char*>(data), size);
    effective_data.resize(0x20, 0x00);
    effective_data[0x01] = 0x08;
    effective_data[0x14] = 0xFF;
    effective_data[0x15] = 0xFF;
    effective_data[0x16] = 0xFF;
    effective_data[0x17] = 0xFF;
    on_word_select_t<false>(c, command, flag, effective_data.data(), effective_data.size());
  } else if (is_big_endian(c->version())) {
    on_word_select_t<true>(c, command, flag, data, size);
  } else {
    on_word_select_t<false>(c, command, flag, data, size);
  }
}

static void on_warp(shared_ptr<Client>, uint8_t, uint8_t, const void* data, size_t size) {
  check_size_t<G_InterLevelWarp_6x94>(data, size);
  // Unconditionally block these. Players should use $warp instead.
}

static void on_set_player_visible(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SetPlayerVisibility_6x22_6x23>(data, size);

  if (cmd.header.client_id == c->lobby_client_id) {
    forward_subcommand(c, command, flag, data, size);

    auto l = c->lobby.lock();
    if (l) {
      if (!l->is_game()) {
        if (!is_v1(c->version())) {
          send_arrow_update(l);
        }
        if (l->check_flag(Lobby::Flag::IS_OVERFLOW)) {
          send_message_box(c, "$C6All lobbies are full.\n\n$C7You are in a private lobby. You can use the\nteleporter to join other lobbies if there is space\navailable.");
          send_lobby_message_box(c, "");
        }
        if (c->version() == Version::BB_V4) {
          send_all_nearby_team_metadatas_to_client(c, false);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

static void on_change_floor_6x1F(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  if (is_pre_v1(c->version())) {
    check_size_t<G_SetPlayerFloor_DCNTE_6x1F>(data, size);
    // DC NTE and 11/2000 don't send 6F when they're done loading, so we do the
    // relevant things 6F would do here instead.
    if (c->config.check_flag(Client::Flag::LOADING)) {
      c->config.clear_flag(Client::Flag::LOADING);
      auto l = c->require_lobby();
      l->assign_inventory_and_bank_item_ids(c, true);
    }

  } else {
    const auto& cmd = check_size_t<G_SetPlayerFloor_6x1F>(data, size);
    if (cmd.floor >= 0) {
      c->floor = cmd.floor;
    }
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_change_floor_6x21(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_InterLevelWarp_6x21>(data, size);
  if (cmd.floor >= 0) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, command, flag, data, size);
}

// When a player dies, decrease their mag's synchro
static void on_player_died(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.client_id != c->lobby_client_id)) {
    return;
  }

  try {
    auto& inventory = c->character()->inventory;
    size_t mag_index = inventory.find_equipped_item(EquipSlot::MAG);
    auto& data = inventory.items[mag_index].data;
    data.data2[0] = max<int8_t>(static_cast<int8_t>(data.data2[0] - 5), 0);
  } catch (const out_of_range&) {
  }

  forward_subcommand(c, command, flag, data, size);
}

// When a player is hit by an enemy, heal them if infinite HP is enabled
static void on_hit_by_enemy(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->license->flags & License::Flag::CHEAT_ANYWHERE));
    if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
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
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->license->flags & License::Flag::CHEAT_ANYWHERE));
    if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
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
  on_forward_check_game_client(c, command, flag, data, size);
}

static void on_cast_technique(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_CastTechnique_6x47>(data, size,
      offsetof(G_CastTechnique_6x47, targets), sizeof(G_CastTechnique_6x47));
  size_t allowed_count = min<size_t>(cmd.header.size - 2, 10);
  if (cmd.target_count > allowed_count) {
    throw runtime_error("invalid cast technique command");
  }
  on_forward_check_game_client(c, command, flag, data, size);
}

static void on_subtract_pb_energy(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SubtractPBEnergy_6x49>(data, size,
      offsetof(G_SubtractPBEnergy_6x49, entries), sizeof(G_SubtractPBEnergy_6x49));
  size_t allowed_count = min<size_t>(cmd.header.size - 3, 14);
  if (cmd.entry_count > allowed_count) {
    throw runtime_error("invalid subtract PB energy command");
  }
  on_forward_check_game_client(c, command, flag, data, size);
}

static void on_npc_control(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_NPCControl_6x69>(data, size);
  // Don't allow NPC control commands if there is a player in the relevant slot
  const auto& l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("cannot create NPCs in the lobby");
  }
  if (cmd.npc_client_id >= 4) {
    throw runtime_error("NPC client ID is not valid");
  }
  if (l->clients[cmd.npc_client_id]) {
    throw runtime_error("cannot overwrite existing player with NPC");
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_switch_state_changed(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  forward_subcommand(c, command, flag, data, size);

  if (cmd.flags && cmd.header.object_id != 0xFFFF) {
    bool player_cheats_enabled = l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->license->flags & License::Flag::CHEAT_ANYWHERE);
    if (player_cheats_enabled &&
        c->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) &&
        (c->last_switch_enabled_command.header.subcommand == 0x05)) {
      c->log.info("[Switch assist] Replaying previous enable command");
      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message(c, "$C5Switch assist");
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
void on_movement_with_floor(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }
  c->x = cmd.x;
  c->z = cmd.z;
  if (cmd.floor >= 0) {
    c->floor = cmd.floor;
  }
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
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, 0, c->version() != Version::BB_V4);
  l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);

  if (l->log.should_log(LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu dropped item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.floor.load(), cmd.x.load(), cmd.z.load());
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
void forward_subcommand_with_item_transcode_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& cmd) {
  // I'm lazy and this should never happen for item commands (since all players
  // need to stay in sync)
  if (command_is_private(command)) {
    throw runtime_error("item subcommand sent via private command");
  }

  auto l = c->require_lobby();
  auto s = c->require_server_state();
  for (auto& other_c : l->clients) {
    if (!other_c || other_c == c) {
      continue;
    }
    if (c->version() != other_c->version()) {
      CmdT out_cmd = cmd;
      out_cmd.item_data.decode_for_version(c->version());
      out_cmd.item_data.encode_for_version(other_c->version(), s->item_parameter_table_for_version(other_c->version()));
      send_command_t(other_c, command, flag, out_cmd);
    } else {
      send_command_t(other_c, command, flag, cmd);
    }
  }
}

template <typename CmdT>
static void on_create_inventory_item_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }
  if (c->version() == Version::BB_V4) {
    // BB should never send this command - inventory items should only be
    // created by the server in response to shop buy / bank withdraw / etc. reqs
    return;
  }

  auto l = c->require_lobby();
  auto p = c->character();
  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  p->add_item(item);

  if (l->log.should_log(LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu created inventory item %08" PRIX32 " (%s)", c->lobby_client_id, item.id.load(), name.c_str());
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
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
  if (l->base_version == Version::BB_V4) {
    return;
  }

  // TODO: Should we delete anything from the inventory here? Does the client
  // send an appropriate 6x29 alongside this?
  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);

  if (l->log.should_log(LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu split stack to create floor item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), item.id.load(), name.c_str(), cmd.floor.load(), cmd.x.load(), cmd.z.load());
    c->character()->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
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
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_SplitStackedItem_BB_6xC3>(data, size);

    if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
      return;
    }

    auto p = c->character();
    auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != Version::BB_V4);

    // If a stack was split, the original item still exists, so the dropped item
    // needs a new ID. remove_item signals this by returning an item with an ID
    // of 0xFFFFFFFF.
    if (item.id == 0xFFFFFFFF) {
      item.id = l->generate_item_id(c->lobby_client_id);
    }

    // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add
    // the item back to the player's inventory to correct for this (it will get
    // removed again by the 6x29 handler)
    p->add_item(item);

    l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);
    send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.x, cmd.z);

    if (l->log.should_log(LogLevel::INFO)) {
      auto s = c->require_server_state();
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hu split stack %08" PRIX32 " (removed: %s) at %hu:(%g, %g)",
          cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.floor.load(), cmd.x.load(), cmd.z.load());
      p->print_inventory(stderr, c->version(), s->item_name_index);
    }

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
  if (l->base_version == Version::BB_V4) {
    return;
  }

  auto p = c->character();
  ItemData item = cmd.item_data;
  item.data2d = 0; // Clear the price field
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  p->add_item(item);

  size_t price = s->item_parameter_table_for_version(c->version())->price_for_item(item);
  p->remove_meseta(price, c->version() != Version::BB_V4);

  if (l->log.should_log(LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu bought item %08" PRIX32 " (%s) from shop (%zu Meseta)",
        cmd.header.client_id.load(), item.id.load(), name.c_str(), price);
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
}

template <typename CmdT>
static void on_box_or_enemy_item_drop_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  // I'm lazy and this should never happen for item commands (since all players
  // need to stay in sync)
  if (command_is_private(command)) {
    throw runtime_error("item subcommand sent via private command");
  }

  const auto& cmd = check_size_t<CmdT>(data, size);

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || (c->lobby_client_id != l->leader_id)) {
    return;
  }
  if (l->base_version == Version::BB_V4) {
    return;
  }

  ItemData item = cmd.item.item;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.item.floor, item, cmd.item.x, cmd.item.z, 0x00F);

  auto name = s->describe_item(c->version(), item, false);
  l->log.info("Player %hhu (leader) created floor item %08" PRIX32 " (%s) at %hhu:(%g, %g)",
      l->leader_id, item.id.load(), name.c_str(), cmd.item.floor, cmd.item.x.load(), cmd.item.z.load());

  for (auto& lc : l->clients) {
    if (!lc || lc == c) {
      continue;
    }
    CmdT out_cmd = cmd;
    if (c->version() != lc->version()) {
      out_cmd.item.item.decode_for_version(c->version());
      out_cmd.item.item.encode_for_version(lc->version(), s->item_parameter_table_for_version(lc->version()));
    }
    send_command_t(lc, command, flag, out_cmd);
  }
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

static void on_pick_up_item_generic(
    shared_ptr<Client> c, uint16_t client_id, uint16_t floor, uint32_t item_id, bool is_request) {
  auto l = c->require_lobby();
  if (!l->is_game() || (client_id != c->lobby_client_id)) {
    return;
  }

  if (!l->item_exists(floor, item_id)) {
    // This can happen if the network is slow, and the client tries to pick up
    // the same item multiple times. Or multiple clients could try to pick up
    // the same item at approximately the same time; only one should get it.
    l->log.warning("Player %hu requests to pick up %08" PRIX32 ", but the item does not exist; dropping command",
        client_id, item_id);

  } else {
    // This is handled by the server on BB, and by the leader on other versions.
    // However, the client's logic is to simply always send a 6x59 command when
    // it receives a 6x5A and the floor item exists, so we just implement that
    // logic here instead of forwarding the 6x5A to the leader.

    auto p = c->character();
    auto fi = l->remove_item(floor, item_id, c->lobby_client_id);
    if (!fi->visible_to_client(c->lobby_client_id)) {
      l->log.warning("Player %hu requests to pick up %08" PRIX32 ", but is it not visible to them; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      return;
    }

    try {
      p->add_item(fi->data);
    } catch (const out_of_range&) {
      // Inventory is full; put the item back where it was
      l->log.warning("Player %hu requests to pick up %08" PRIX32 ", but their inventory is full; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      return;
    }

    if (l->log.should_log(LogLevel::INFO)) {
      auto s = c->require_server_state();
      auto name = s->describe_item(c->version(), fi->data, false);
      l->log.info("Player %hu picked up %08" PRIX32 " (%s)", client_id, item_id, name.c_str());
      p->print_inventory(stderr, c->version(), s->item_name_index);
    }

    auto s = c->require_server_state();
    for (size_t z = 0; z < 12; z++) {
      auto lc = l->clients[z];
      if ((!lc) || (!is_request && (lc == c))) {
        continue;
      }
      if (fi->visible_to_client(z)) {
        send_pick_up_item_to_client(lc, client_id, item_id, floor);
      } else if (lc->version() == Version::BB_V4) {
        send_create_inventory_item_to_client(lc, client_id, fi->data);
      } else {
        send_drop_item_to_channel(s, lc->channel, fi->data, false, lc->floor, lc->x, lc->z, 0xFFFF);
        send_pick_up_item_to_client(lc, client_id, item_id, floor);
      }
    }
  }
}

static void on_pick_up_item(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto& cmd = check_size_t<G_PickUpItem_6x59>(data, size);
  on_pick_up_item_generic(c, cmd.client_id2, cmd.floor, cmd.item_id, false);
}

static void on_pick_up_item_request(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto& cmd = check_size_t<G_PickUpItemRequest_6x5A>(data, size);
  on_pick_up_item_generic(c, cmd.header.client_id, cmd.floor, cmd.item_id, true);
}

static void on_equip_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_EquipItem_6x25>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  EquipSlot slot = static_cast<EquipSlot>(cmd.equip_slot.load());
  auto p = c->character();
  p->inventory.equip_item_id(cmd.item_id, slot);
  c->log.info("Equipped item %08" PRIX32, cmd.item_id.load());

  forward_subcommand(c, command, flag, data, size);
}

static void on_unequip_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnequipItem_6x26>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  auto p = c->character();
  p->inventory.unequip_item_id(cmd.item_id);
  c->log.info("Unequipped item %08" PRIX32, cmd.item_id.load());

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
  auto s = c->require_server_state();
  auto p = c->character();
  size_t index = p->inventory.find_item(cmd.item_id);
  string name;
  {
    // Note: We do this weird scoping thing because player_use_item will
    // likely delete the item, which will break the reference here.
    const auto& item = p->inventory.items[index].data;
    name = s->describe_item(c->version(), item, false);
  }
  player_use_item(c, index);

  if (l->log.should_log(LogLevel::INFO)) {
    l->log.info("Player %hhu used item %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    p->print_inventory(stderr, c->version(), s->item_name_index);
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
  auto s = c->require_server_state();
  auto p = c->character();

  size_t mag_index = p->inventory.find_item(cmd.mag_item_id);
  size_t fed_index = p->inventory.find_item(cmd.fed_item_id);
  string mag_name, fed_name;
  {
    // Note: We do this weird scoping thing because player_feed_mag will
    // likely delete the items, which will break the references here.
    const auto& fed_item = p->inventory.items[fed_index].data;
    fed_name = s->describe_item(c->version(), fed_item, false);
    const auto& mag_item = p->inventory.items[mag_index].data;
    mag_name = s->describe_item(c->version(), mag_item, false);
  }
  player_feed_mag(c, mag_index, fed_index);

  // On BB, the player only sends a 6x28; on other versions, the player sends
  // a 6x29 immediately after to destroy the fed item. So on BB, we should
  // remove the fed item here, but on other versions, we allow the following
  // 6x29 command to do that.
  if (l->base_version == Version::BB_V4) {
    p->remove_item(cmd.fed_item_id, 1, false);
  }

  if (l->log.should_log(LogLevel::INFO)) {
    l->log.info("Player %hhu fed item %hu:%08" PRIX32 " (%s) to mag %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.fed_item_id.load(), fed_name.c_str(),
        cmd.header.client_id.load(), cmd.mag_item_id.load(), mag_name.c_str());
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_open_shop_bb_or_ep3_battle_subs(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("received 6xB5 command in lobby");
  } else if (l->is_ep3()) {
    on_ep3_battle_subs(c, command, flag, data, size);
  } else if (l->base_version != Version::BB_V4) {
    throw runtime_error("received BB shop subcommand in non-BB game");
  } else if (!l->item_creator) {
    throw runtime_error("received shop subcommand without item creator present");
  } else {
    const auto& cmd = check_size_t<G_ShopContentsRequest_BB_6xB5>(data, size);
    auto s = c->require_server_state();
    size_t level = c->character()->disp.stats.level + 1;
    switch (cmd.shop_type) {
      case 0:
        c->bb_shop_contents[0] = l->item_creator->generate_tool_shop_contents(level);
        break;
      case 1:
        c->bb_shop_contents[1] = l->item_creator->generate_weapon_shop_contents(level);
        break;
      case 2:
        c->bb_shop_contents[2] = l->item_creator->generate_armor_shop_contents(level);
        break;
      default:
        throw runtime_error("invalid shop type");
    }
    for (auto& item : c->bb_shop_contents[cmd.shop_type]) {
      item.id = 0xFFFFFFFF;
      item.data2d = s->item_parameter_table_for_version(c->version())->price_for_item(item);
    }

    send_shop(c, cmd.shop_type);
  }
}

static void on_open_bank_bb_or_card_trade_counter_ep3(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if ((l->base_version == Version::BB_V4) && l->is_game()) {
    c->config.set_flag(Client::Flag::AT_BANK_COUNTER);
    send_bank(c);
  } else if (l->is_ep3()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_ep3_private_word_select_bb_bank_action(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_BankAction_BB_6xBD>(data, size);

    if (!l->is_game()) {
      return;
    }

    auto p = c->character();
    auto& bank = c->current_bank();
    if (cmd.action == 0) { // Deposit
      if (cmd.item_id == 0xFFFFFFFF) { // Deposit Meseta
        if (cmd.meseta_amount > p->disp.stats.meseta) {
          l->log.info("Player %hu attempted to deposit %" PRIu32 " Meseta in the bank, but has only %" PRIu32 " Meseta on hand",
              c->lobby_client_id, cmd.meseta_amount.load(), p->disp.stats.meseta.load());
        } else if ((bank.meseta + cmd.meseta_amount) > 999999) {
          l->log.info("Player %hu attempted to deposit %" PRIu32 " Meseta in the bank, but already has %" PRIu32 " Meseta in the bank",
              c->lobby_client_id, cmd.meseta_amount.load(), p->disp.stats.meseta.load());
        } else {
          bank.meseta += cmd.meseta_amount;
          p->disp.stats.meseta -= cmd.meseta_amount;
          l->log.info("Player %hu deposited %" PRIu32 " Meseta in the bank (bank now has %" PRIu32 "; inventory now has %" PRIu32 ")",
              c->lobby_client_id, cmd.meseta_amount.load(), bank.meseta.load(), p->disp.stats.meseta.load());
        }

      } else { // Deposit item
        auto item = p->remove_item(cmd.item_id, cmd.item_amount, c->version() != Version::BB_V4);
        // If a stack was split, the bank item retains the same item ID as the
        // inventory item. This is annoying but doesn't cause any problems
        // because we always generate a new item ID when withdrawing from the
        // bank, so there's no chance of conflict later.
        if (item.id == 0xFFFFFFFF) {
          item.id = cmd.item_id;
        }
        bank.add_item(item);
        send_destroy_item_to_lobby(c, cmd.item_id, cmd.item_amount, true);

        if (l->log.should_log(LogLevel::INFO)) {
          string name = s->item_name_index->describe_item(Version::BB_V4, item);
          l->log.info("Player %hu deposited item %08" PRIX32 " (x%hhu) (%s) in the bank",
              c->lobby_client_id, cmd.item_id.load(), cmd.item_amount, name.c_str());
          c->character()->print_inventory(stderr, c->version(), s->item_name_index);
        }
      }

    } else if (cmd.action == 1) { // Take
      if (cmd.item_index == 0xFFFF) { // Take Meseta
        if (cmd.meseta_amount > bank.meseta) {
          l->log.info("Player %hu attempted to withdraw %" PRIu32 " Meseta from the bank, but has only %" PRIu32 " Meseta in the bank",
              c->lobby_client_id, cmd.meseta_amount.load(), bank.meseta.load());
        } else if ((p->disp.stats.meseta + cmd.meseta_amount) > 999999) {
          l->log.info("Player %hu attempted to withdraw %" PRIu32 " Meseta from the bank, but already has %" PRIu32 " Meseta on hand",
              c->lobby_client_id, cmd.meseta_amount.load(), p->disp.stats.meseta.load());
        } else {
          bank.meseta -= cmd.meseta_amount;
          p->disp.stats.meseta += cmd.meseta_amount;
          l->log.info("Player %hu withdrew %" PRIu32 " Meseta from the bank (bank now has %" PRIu32 "; inventory now has %" PRIu32 ")",
              c->lobby_client_id, cmd.meseta_amount.load(), bank.meseta.load(), p->disp.stats.meseta.load());
        }

      } else { // Take item
        auto item = bank.remove_item(cmd.item_id, cmd.item_amount);
        item.id = l->generate_item_id(c->lobby_client_id);
        p->add_item(item);
        send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

        if (l->log.should_log(LogLevel::INFO)) {
          string name = s->item_name_index->describe_item(Version::BB_V4, item);
          l->log.info("Player %hu withdrew item %08" PRIX32 " (x%hhu) (%s) from the bank",
              c->lobby_client_id, item.id.load(), cmd.item_amount, name.c_str());
          c->character()->print_inventory(stderr, c->version(), s->item_name_index);
        }
      }

    } else if (cmd.action == 3) { // Leave bank counter
      c->config.clear_flag(Client::Flag::AT_BANK_COUNTER);
    }

  } else if (is_ep3(c->version())) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_sort_inventory_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_SortInventory_BB_6xC4>(data, size);

    auto p = c->character();

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
    // It's annoying that extension data is stored in the inventory items array,
    // because we have to be careful to avoid sorting it here too.
    for (size_t x = 0; x < 30; x++) {
      sorted[x].extension_data1 = p->inventory.items[x].extension_data1;
      sorted[x].extension_data2 = p->inventory.items[x].extension_data2;
    }
    p->inventory.items = sorted;
  }
}

////////////////////////////////////////////////////////////////////////////////
// EXP/Drop Item commands

static void on_entity_drop_item_request(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  switch (l->drop_mode) {
    case Lobby::DropMode::CLIENT:
      forward_subcommand(c, command, flag, data, size);
      return;
    case Lobby::DropMode::DISABLED:
      return;
    case Lobby::DropMode::SERVER_SHARED:
    case Lobby::DropMode::SERVER_DUPLICATE:
    case Lobby::DropMode::SERVER_PRIVATE:
      break;
    default:
      throw logic_error("invalid drop mode");
  }

  G_SpecializableItemDropRequest_6xA2 cmd;
  if (size == sizeof(G_SpecializableItemDropRequest_6xA2)) {
    cmd = check_size_t<G_SpecializableItemDropRequest_6xA2>(data, size);
    if (cmd.header.subcommand != 0xA2) {
      throw runtime_error("item drop request has incorrect subcommand");
    }
  } else if (size == sizeof(G_StandardDropItemRequest_PC_V3_BB_6x60)) {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_PC_V3_BB_6x60>(data, size);
    if (in_cmd.header.subcommand != 0x60) {
      throw runtime_error("item drop request has incorrect subcommand");
    }
    cmd.entity_id = in_cmd.entity_id;
    cmd.floor = in_cmd.floor;
    cmd.rt_index = in_cmd.rt_index;
    cmd.x = in_cmd.x;
    cmd.z = in_cmd.z;
    cmd.ignore_def = 1;
    cmd.effective_area = in_cmd.effective_area;
  } else {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_DC_6x60>(data, size);
    if (in_cmd.header.subcommand != 0x60) {
      throw runtime_error("item drop request has incorrect subcommand");
    }
    cmd.entity_id = in_cmd.entity_id;
    cmd.floor = in_cmd.floor;
    cmd.rt_index = in_cmd.rt_index;
    cmd.x = in_cmd.x;
    cmd.z = in_cmd.z;
    cmd.ignore_def = 1;
    cmd.effective_area = in_cmd.floor;
  }

  auto generate_item = [&]() -> ItemData {
    if (cmd.rt_index == 0x30) {
      if (l->map) {
        auto& object = l->map->objects.at(cmd.entity_id);

        if (cmd.floor != object.floor) {
          l->log.warning("Floor %02hhX from command does not match object\'s expected floor %02hhX", cmd.floor, object.floor);
        }
        bool object_ignore_def = (object.param1 > 0.0);
        if (cmd.ignore_def != object_ignore_def) {
          l->log.warning("ignore_def value %s from command does not match object\'s expected ignore_def %s (from p1=%g)",
              cmd.ignore_def ? "true" : "false", object_ignore_def ? "true" : "false", object.param1);
        }

        if (cmd.ignore_def) {
          l->log.info("Creating item from box %04hX (area %02hX)", cmd.entity_id.load(), cmd.effective_area);
          return l->item_creator->on_box_item_drop(cmd.entity_id, cmd.effective_area);
        } else {
          l->log.info("Creating item from box %04hX (area %02hX; specialized with %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 ")",
              cmd.entity_id.load(), cmd.effective_area, object.param3, object.param4, object.param5, object.param6);
          return l->item_creator->on_specialized_box_item_drop(cmd.entity_id, cmd.effective_area, object.param3, object.param4, object.param5, object.param6);
        }

      } else if (cmd.ignore_def) {
        l->log.info("Creating item from box %04hX (area %02hX)", cmd.entity_id.load(), cmd.effective_area);
        return l->item_creator->on_box_item_drop(cmd.entity_id, cmd.effective_area);

      } else {
        l->log.info("Creating item from box %04hX (area %02hX; specialized with %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 ")",
            cmd.entity_id.load(), cmd.effective_area, cmd.param3.load(), cmd.param4.load(), cmd.param5.load(), cmd.param6.load());
        return l->item_creator->on_specialized_box_item_drop(
            cmd.entity_id, cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
      }

    } else {
      l->log.info("Creating item from enemy %04hX (area %02hX)", cmd.entity_id.load(), cmd.effective_area);

      uint8_t effective_rt_index = cmd.rt_index;
      if (l->map) {
        const auto& enemy = l->map->enemies.at(cmd.entity_id);
        effective_rt_index = rare_table_index_for_enemy_type(enemy.type);
        // rt_indexes in Episode 4 don't match those sent in the command; we just
        // ignore what the client sends.
        if ((l->episode != Episode::EP4) && (cmd.rt_index != effective_rt_index)) {
          l->log.warning("rt_index %02hhX from command does not match entity\'s expected index %02" PRIX32,
              cmd.rt_index, effective_rt_index);
        }
        if (cmd.floor != enemy.floor) {
          l->log.warning("Floor %02hhX from command does not match entity\'s expected floor %02hhX",
              cmd.floor, enemy.floor);
        }
      }
      return l->item_creator->on_monster_item_drop(cmd.entity_id, effective_rt_index, cmd.effective_area);
    }
  };

  switch (l->drop_mode) {
    case Lobby::DropMode::DISABLED:
    case Lobby::DropMode::CLIENT:
      throw logic_error("unhandled simple drop mode");
    case Lobby::DropMode::SERVER_SHARED:
    case Lobby::DropMode::SERVER_DUPLICATE: {
      // TODO: In SERVER_DUPLICATE mode, should we reduce the rates for rare
      // items? Maybe by a factor of l->count_clients()?
      auto item = generate_item();
      if (item.empty()) {
        l->log.info("No item was created");
      } else {
        string name = s->item_name_index->describe_item(l->base_version, item);
        l->log.info("Entity %04hX (area %02hX) created item %s", cmd.entity_id.load(), cmd.effective_area, name.c_str());
        if (l->drop_mode == Lobby::DropMode::SERVER_DUPLICATE) {
          for (const auto& lc : l->clients) {
            if (lc && ((cmd.rt_index == 0x30) || (lc->floor == cmd.floor))) {
              item.id = l->generate_item_id(0xFF);
              l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for %s",
                  item.id.load(), cmd.floor, cmd.x.load(), cmd.z.load(), lc->channel.name.c_str());
              l->add_item(cmd.floor, item, cmd.x, cmd.z, (1 << lc->lobby_client_id));
              send_drop_item_to_channel(s, lc->channel, item, cmd.rt_index != 0x30, cmd.floor, cmd.x, cmd.z, cmd.entity_id);
            }
          }

        } else {
          item.id = l->generate_item_id(0xFF);
          l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for all clients",
              item.id.load(), cmd.floor, cmd.x.load(), cmd.z.load());
          l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);
          send_drop_item_to_lobby(l, item, cmd.rt_index != 0x30, cmd.floor, cmd.x, cmd.z, cmd.entity_id);
        }
      }
      break;
    }
    case Lobby::DropMode::SERVER_PRIVATE: {
      for (const auto& lc : l->clients) {
        if (lc && ((cmd.rt_index == 0x30) || (lc->floor == cmd.floor))) {
          auto item = generate_item();
          if (item.empty()) {
            l->log.info("No item was created for %s", lc->channel.name.c_str());
          } else {
            string name = s->item_name_index->describe_item(l->base_version, item);
            l->log.info("Entity %04hX (area %02hX) created item %s", cmd.entity_id.load(), cmd.effective_area, name.c_str());
            item.id = l->generate_item_id(0xFF);
            l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for %s",
                item.id.load(), cmd.floor, cmd.x.load(), cmd.z.load(), lc->channel.name.c_str());
            l->add_item(cmd.floor, item, cmd.x, cmd.z, (1 << lc->lobby_client_id));
            send_drop_item_to_channel(s, lc->channel, item, cmd.rt_index != 0x30, cmd.floor, cmd.x, cmd.z, cmd.entity_id);
          }
        }
      }
      break;
    }
    default:
      throw logic_error("invalid drop mode");
  }

  if (cmd.rt_index == 0x30) {
    l->item_creator->set_box_destroyed(cmd.entity_id);
  } else {
    l->item_creator->set_monster_destroyed(cmd.entity_id);
  }
}

static void on_set_quest_flag(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  uint16_t flag_index, difficulty, action;
  if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
    const auto& cmd = check_size_t<G_UpdateQuestFlag_DC_PC_6x75>(data, size);
    flag_index = cmd.flag;
    action = cmd.action;
    difficulty = l->difficulty;
  } else {
    const auto& cmd = check_size_t<G_UpdateQuestFlag_V3_BB_6x75>(data, size);
    flag_index = cmd.flag;
    action = cmd.action;
    difficulty = cmd.difficulty;
  }

  if (flag_index >= 0x400) {
    return;
  }

  // TODO: Should we allow overlays here?
  auto p = c->character(true, false);

  // The client explicitly checks for both 0 and 1 - any other value means no
  // operation is performed.
  if (action == 0) {
    p->quest_flags.set(difficulty, flag_index);
  } else if (action == 1) {
    p->quest_flags.clear(difficulty, flag_index);
  }

  forward_subcommand(c, command, flag, data, size);

  if (is_v3(c->version())) {
    bool should_send_boss_drop_req = false;
    bool is_ep2 = (l->episode == Episode::EP2);
    if ((l->episode == Episode::EP1) && (c->floor == 0x0E)) {
      // On Normal, Dark Falz does not have a third phase, so send the drop
      // request after the end of the second phase. On all other difficulty
      // levels, send it after the third phase.
      if (((difficulty == 0) && (flag_index == 0x0035)) ||
          ((difficulty != 0) && (flag_index == 0x0037))) {
        should_send_boss_drop_req = true;
      }
    } else if (is_ep2 && (flag_index == 0x0057) && (c->floor == 0x0D)) {
      should_send_boss_drop_req = true;
    }

    if (should_send_boss_drop_req) {
      auto c = l->clients.at(l->leader_id);
      if (c) {
        G_StandardDropItemRequest_PC_V3_BB_6x60 req = {
            {
                {0x60, 0x06, 0x0000},
                static_cast<uint8_t>(c->floor),
                static_cast<uint8_t>(is_ep2 ? 0x4E : 0x2F),
                0x0B4F,
                is_ep2 ? -9999.0f : 10160.58984375f,
                0.0f,
                2,
                0,
            },
            0x01,
            {}};
        send_command_t(c, 0x62, l->leader_id, req);
      }
    }
  }
}

static void on_dragon_actions(shared_ptr<Client> c, uint8_t command, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_DragonBossActions_DC_PC_XB_BB_6x12>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  G_DragonBossActions_GC_6x12 sw_cmd = {{{cmd.header.subcommand, cmd.header.size, cmd.header.enemy_id},
      cmd.unknown_a2, cmd.unknown_a3, cmd.unknown_a4, cmd.x.load(), cmd.z.load()}};
  bool sender_is_gc = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_gc) {
        send_command_t(lc, 0x60, 0x00, cmd);
      } else {
        send_command_t(lc, 0x60, 0x00, sw_cmd);
      }
    }
  }
}

static void on_gol_dragon_actions(shared_ptr<Client> c, uint8_t command, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_GolDragonBossActions_XB_BB_6xA8>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  G_GolDragonBossActions_GC_6xA8 sw_cmd = {{{cmd.header.subcommand, cmd.header.size, cmd.header.enemy_id},
      cmd.unknown_a2,
      cmd.unknown_a3,
      cmd.unknown_a4,
      cmd.x.load(),
      cmd.z.load(),
      cmd.unknown_a5,
      0}};
  bool sender_is_gc = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_gc) {
        send_command_t(lc, 0x60, 0x00, cmd);
      } else {
        send_command_t(lc, 0x60, 0x00, sw_cmd);
      }
    }
  }
}

static void on_enemy_hit(shared_ptr<Client> c, uint8_t command, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_EnemyHitByPlayer_DC_PC_XB_BB_6x0A>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  if (l->base_version == Version::BB_V4) {
    if (c->lobby_client_id > 3) {
      throw logic_error("client ID is above 3");
    }
    if (!l->map) {
      throw runtime_error("game does not have a map loaded");
    }
    if (cmd.enemy_index >= l->map->enemies.size()) {
      return;
    }

    auto& enemy = l->map->enemies[cmd.enemy_index];
    enemy.last_hit_by_client_id = c->lobby_client_id;
  }

  G_EnemyHitByPlayer_GC_6x0A sw_cmd = {{{cmd.header.subcommand, cmd.header.size, cmd.header.enemy_id}, cmd.enemy_index, cmd.remaining_hp, cmd.flags.load()}};
  bool sender_is_gc = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_gc) {
        send_command_t(lc, 0x60, 0x00, cmd);
      } else {
        send_command_t(lc, 0x60, 0x00, sw_cmd);
      }
    }
  }
}

static void on_charge_attack_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version != Version::BB_V4) {
    throw runtime_error("BB-only command sent in non-BB game");
  }

  forward_subcommand(c, command, flag, data, size);

  const auto& cmd = check_size_t<G_ChargeAttack_BB_6xC7>(data, size);
  auto& disp = c->character()->disp;
  if (cmd.meseta_amount > disp.stats.meseta) {
    disp.stats.meseta = 0;
  } else {
    disp.stats.meseta -= cmd.meseta_amount;
  }
}

static void on_level_up(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_LevelUp_6x30>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto p = c->character();
  p->disp.stats.char_stats.atp = cmd.atp;
  p->disp.stats.char_stats.mst = cmd.mst;
  p->disp.stats.char_stats.evp = cmd.evp;
  p->disp.stats.char_stats.hp = cmd.hp;
  p->disp.stats.char_stats.dfp = cmd.dfp;
  p->disp.stats.char_stats.ata = cmd.ata;
  p->disp.stats.level = cmd.level.load();

  forward_subcommand(c, command, flag, data, size);
}

static void add_player_exp(shared_ptr<Client> c, uint32_t exp) {
  auto s = c->require_server_state();
  auto p = c->character();

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

  if (l->base_version != Version::BB_V4) {
    throw runtime_error("BB-only command sent in non-BB game");
  }
  if (!l->map) {
    throw runtime_error("map not loaded");
  }

  const auto& cmd = check_size_t<G_StealEXP_BB_6xC6>(data, size);

  auto p = c->character();
  const auto& enemy = l->map->enemies.at(cmd.enemy_index);
  const auto& inventory = p->inventory;
  const auto& weapon = inventory.items[inventory.find_equipped_item(EquipSlot::WEAPON)];

  auto item_parameter_table = s->item_parameter_table_for_version(c->version());

  uint8_t special_id = 0;
  if (((weapon.data.data1[1] < 0x0A) && (weapon.data.data1[2] < 0x05)) ||
      ((weapon.data.data1[1] < 0x0D) && (weapon.data.data1[2] < 0x04))) {
    special_id = weapon.data.data1[4] & 0x3F;
  } else {
    special_id = item_parameter_table->get_weapon(weapon.data.data1[1], weapon.data.data1[2]).special;
  }

  const auto& special = item_parameter_table->get_special(special_id);
  if (special.type != 3) { // Master's/Lord's/King's
    return;
  }

  const auto& bp_table = s->battle_params->get_table(l->mode == GameMode::SOLO, l->episode);
  uint32_t bp_index = battle_param_index_for_enemy_type(l->episode, enemy.type);
  uint32_t enemy_exp = bp_table.stats[l->difficulty][bp_index].experience;

  // Note: The original code checks if special.type is 9, 10, or 11, and skips
  // applying the android bonus if so. We don't do anything for those special
  // types, so we don't check for that here.
  uint32_t percent = special.amount + (char_class_is_android(p->disp.visual.char_class) ? 30 : 0);
  uint32_t stolen_exp = min<uint32_t>((enemy_exp * percent) / 100, (l->difficulty + 1) * 20);
  if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    send_text_message_printf(c, "$C5+%" PRIu32 " E-%hX %s",
        stolen_exp, cmd.enemy_index.load(), name_for_enum(enemy.type));
  }
  add_player_exp(c, stolen_exp);
}

static void on_enemy_exp_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  if (l->base_version != Version::BB_V4) {
    throw runtime_error("BB-only command sent in non-BB game");
  }

  const auto& cmd = check_size_t<G_EnemyEXPRequest_BB_6xC8>(data, size);

  if (!l->is_game()) {
    throw runtime_error("client should not kill enemies outside of games");
  }
  if (!l->map) {
    throw runtime_error("game does not have a map loaded");
  }
  if (cmd.enemy_index >= l->map->enemies.size()) {
    send_text_message(c, "$C6Missing enemy killed");
    return;
  }
  if (c->lobby_client_id > 3) {
    throw runtime_error("client ID is too large");
  }

  auto& e = l->map->enemies[cmd.enemy_index];
  string e_str = e.str();
  c->log.info("EXP requested for E-%hX => %s", cmd.enemy_index.load(), e_str.c_str());

  uint8_t state_flag = Map::Enemy::Flag::EXP_REQUESTED_BY_PLAYER0 << c->lobby_client_id;
  if (e.state_flags & state_flag) {
    if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      send_text_message_printf(c, "$C5E-%hX __CHECKED__", cmd.enemy_index.load());
    }

  } else {
    e.state_flags |= state_flag;

    double experience = 0.0;
    try {
      const auto& bp_table = s->battle_params->get_table(l->mode == GameMode::SOLO, l->episode);
      uint32_t bp_index = battle_param_index_for_enemy_type(l->episode, e.type);
      experience = bp_table.stats[l->difficulty][bp_index].experience;
    } catch (const exception& e) {
      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(c, "$C5E-%hX __MISSING__\n%s", cmd.enemy_index.load(), e.what());
      } else {
        send_text_message_printf(c, "$C4Unknown enemy type killed:\n%s", e.what());
      }
    }

    if (experience != 0.0) {
      if (c->floor != e.floor) {
        if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
          send_text_message_printf(c, "$C5E-%hX %s\n$C4FLOOR Y:%02" PRIX32 " E:%02hhX",
              cmd.enemy_index.load(), name_for_enum(e.type), c->floor, e.floor);
        }
      } else {
        // In PSOBB, Sega decided to add a 30% EXP boost for Episode 2. They could
        // have done something reasonable, like edit the BattleParamEntry files so
        // the monsters would all give more EXP, but they did something far lazier
        // instead: they just stuck an if statement in the client's EXP request
        // function. We, unfortunately, have to do the same here.
        bool is_killer = (e.last_hit_by_client_id == c->lobby_client_id);
        bool is_ep2 = (l->episode == Episode::EP2);
        uint32_t player_exp = experience *
            (is_killer ? 1.0 : 0.8) *
            l->base_exp_multiplier *
            l->challenge_exp_multiplier *
            (is_ep2 ? 1.3 : 1.0);
        if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
          send_text_message_printf(
              c, "$C5+%" PRIu32 " E-%hX %s%s",
              player_exp,
              cmd.enemy_index.load(),
              (!cmd.is_killer == !is_killer) ? "" : "$C6!K$C5 ",
              name_for_enum(e.type));
        }
        if (c->character()->disp.stats.level < 199) {
          add_player_exp(c, player_exp);
        }
      }
    }

    // Update kill counts on unsealable items
    auto& inventory = c->character()->inventory;
    for (size_t z = 0; z < inventory.num_items; z++) {
      auto& item = inventory.items[z];
      if ((item.flags & 0x08) &&
          s->item_parameter_table_for_version(c->version())->is_unsealable_item(item.data)) {
        item.data.set_sealed_item_kill_count(item.data.get_sealed_item_kill_count() + 1);
      }
    }
  }
}

void on_meseta_reward_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_MesetaRewardRequest_BB_6xC9>(data, size);

  auto p = c->character();
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
    item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(item);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
}

void on_item_reward_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ItemRewardRequest_BB_6xCA>(data, size);
  auto l = c->require_lobby();

  ItemData item;
  item = cmd.item_data;
  item.enforce_min_stack_size();
  item.id = l->generate_item_id(c->lobby_client_id);
  c->character()->add_item(item);
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
}

void on_transfer_item_via_mail_message_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_TransferItemViaMailMessage_BB_6xCB>(data, size);

  auto team = c->team();
  if (!team) {
    throw runtime_error("player is not in a team");
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->base_version != Version::BB_V4) {
    throw runtime_error("item tracking not enabled in BB game");
  }

  forward_subcommand(c, command, flag, data, size);

  auto s = c->require_server_state();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != Version::BB_V4);

  if (l->log.should_log(LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu sent inventory item %hu:%08" PRIX32 " (%s) x%" PRIu32 " to player %08" PRIX32,
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.amount.load(), cmd.target_guild_card_number.load());
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  // To receive an item, the player must be online, using BB, have a character
  // loaded (that is, be in a lobby or game), not be at the bank counter at the
  // moment, and there must be room in their bank to receive the item.
  bool item_sent = false;
  auto target_c = s->find_client(nullptr, cmd.target_guild_card_number);
  if (target_c &&
      (target_c->version() == Version::BB_V4) &&
      (target_c->character(false) != nullptr) &&
      !target_c->config.check_flag(Client::Flag::AT_BANK_COUNTER)) {
    try {
      target_c->current_bank().add_item(item);
      item_sent = true;
    } catch (const runtime_error&) {
    }
  }

  if (item_sent) {
    send_command(c, 0x16EA, 0x00000001);
  } else {
    send_command(c, 0x16EA, 0x00000000);
    // If the item failed to send, add it back to the sender's inventory
    item.id = l->generate_item_id(0xFF);
    p->add_item(item);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
}

void on_exchange_item_for_team_points_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_ExchangeItemForTeamPoints_BB_6xCC>(data, size);

  auto team = c->team();
  if (!team) {
    throw runtime_error("player is not in a team");
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  if (l->base_version != Version::BB_V4) {
    throw runtime_error("item tracking not enabled in BB game");
  }

  auto s = c->require_server_state();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != Version::BB_V4);

  size_t points = s->item_parameter_table_v4->get_item_team_points(item);
  s->team_index->add_member_points(c->license->serial_number, points);

  if (l->log.should_log(LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu exchanged inventory item %hu:%08" PRIX32 " (%s) for %zu team points",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), points);
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }

  forward_subcommand(c, command, flag, data, size);
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

  auto s = c->require_server_state();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != Version::BB_V4);

  if (l->log.should_log(LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu destroyed inventory item %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    p->print_inventory(stderr, c->version(), s->item_name_index);
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_destroy_floor_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_DestroyFloorItem_6x63>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto s = c->require_server_state();
  auto fi = l->remove_item(cmd.floor, cmd.item_id, 0xFF);
  auto name = s->describe_item(c->version(), fi->data, false);
  l->log.info("Player %hhu destroyed floor item %08" PRIX32 " (%s)", c->lobby_client_id, cmd.item_id.load(), name.c_str());

  // Only forward to players for whom the item was visible
  for (size_t z = 0; z < l->clients.size(); z++) {
    auto lc = l->clients[z];
    if (lc && fi->visible_to_client(z)) {
      send_command_t(lc, command, flag, cmd);
    }
  }
}

static void on_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_IdentifyItemRequest_6xB8>(data, size);
    if (!l->is_game()) {
      return;
    }
    if (!l->item_creator) {
      throw runtime_error("received item identify subcommand without item creator present");
    }

    auto p = c->character();
    size_t x = p->inventory.find_item(cmd.item_id);
    if (p->inventory.items[x].data.data1[0] != 0) {
      throw runtime_error("non-weapon items cannot be unidentified");
    }

    // It seems the client expects an item ID to be consumed here, even though
    // the returned item has the same ID as the original item. Perhaps this was
    // not the case on Sega's original server, and the returned item had a new
    // ID instead.
    l->generate_item_id(c->lobby_client_id);
    p->disp.stats.meseta -= 100;
    c->bb_identify_result = p->inventory.items[x].data;
    c->bb_identify_result.data1[4] &= 0x7F;
    l->item_creator->apply_tekker_deltas(c->bb_identify_result, p->disp.visual.section_id);
    send_item_identify_result(c);

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_accept_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_AcceptItemIdentification_BB_6xBA>(data, size);

    if (!c->bb_identify_result.id || (c->bb_identify_result.id == 0xFFFFFFFF)) {
      throw runtime_error("no identify result present");
    }
    if (c->bb_identify_result.id != cmd.item_id) {
      throw runtime_error("accepted item ID does not match previous identify request");
    }
    c->character()->add_item(c->bb_identify_result);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, c->bb_identify_result);
    c->bb_identify_result.clear();

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_sell_item_at_shop_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_SellItemAtShop_BB_6xC0>(data, size);

    auto s = c->require_server_state();
    auto p = c->character();
    auto item = p->remove_item(cmd.item_id, cmd.amount, c->version() != Version::BB_V4);
    size_t price = (s->item_parameter_table_for_version(c->version())->price_for_item(item) >> 3) * cmd.amount;
    p->add_meseta(price);

    if (l->log.should_log(LogLevel::INFO)) {
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hhu sold inventory item %08" PRIX32 " (%s) for %zu Meseta",
          c->lobby_client_id, cmd.item_id.load(), name.c_str(), price);
      p->print_inventory(stderr, c->version(), s->item_name_index);
    }

    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_buy_shop_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->base_version == Version::BB_V4) {
    const auto& cmd = check_size_t<G_BuyShopItem_BB_6xB7>(data, size);

    ItemData item;
    item = c->bb_shop_contents.at(cmd.shop_type).at(cmd.item_index);
    if (item.is_stackable()) {
      item.data1[5] = cmd.amount;
    } else if (cmd.amount != 1) {
      throw runtime_error("item is not stackable");
    }

    size_t price = item.data2d * cmd.amount;
    item.data2d = 0;
    auto p = c->character();
    p->remove_meseta(price, false);

    item.id = cmd.shop_item_id;
    l->on_item_id_generated_externally(item.id);
    p->add_item(item);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item, true);

    if (l->log.should_log(LogLevel::INFO)) {
      auto s = c->require_server_state();
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hhu purchased item %08" PRIX32 " (%s) for %zu meseta",
          c->lobby_client_id, item.id.load(), name.c_str(), price);
      p->print_inventory(stderr, c->version(), s->item_name_index);
    }
  }
}

static void on_medical_center_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void*, size_t) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4)) {
    c->character()->remove_meseta(10, false);
  }
}

static void on_battle_restart_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == Version::BB_V4) &&
      (l->mode == GameMode::BATTLE) &&
      l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) &&
      l->quest &&
      l->leader_id == c->lobby_client_id) {
    const auto& cmd = check_size_t<G_StartBattle_BB_6xCF>(data, size);

    auto vq = l->quest->version(Version::BB_V4, c->language());
    auto dat_contents = prs_decompress(*vq->dat_contents);

    auto new_rules = make_shared<BattleRules>(cmd.rules);
    if (l->item_creator) {
      l->item_creator->set_restrictions(new_rules);
    }

    for (auto& lc : l->clients) {
      if (lc) {
        lc->delete_overlay();
        lc->use_default_bank();
        lc->create_battle_overlay(new_rules, s->level_table);
      }
    }
    l->load_maps();
  }
}

static void on_battle_level_up_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == Version::BB_V4) &&
      (l->mode == GameMode::BATTLE) &&
      l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_BattleModeLevelUp_BB_6xD0>(data, size);
    auto lc = l->clients.at(cmd.header.client_id);
    if (lc) {
      auto s = c->require_server_state();
      auto lp = lc->character();
      uint32_t target_level = lp->disp.stats.level + cmd.num_levels;
      uint32_t before_exp = lp->disp.stats.experience;
      lp->disp.stats.advance_to_level(lp->disp.visual.char_class, target_level, s->level_table);
      send_give_experience(lc, lp->disp.stats.experience - before_exp);
      send_level_up(lc);
    }
  }
}

static void on_request_challenge_grave_recovery_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == Version::BB_V4) &&
      (l->mode == GameMode::CHALLENGE) &&
      l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_ChallengeModeGraveRecoveryItemRequest_BB_6xD1>(data, size);
    static const array<ItemData, 6> items = {
        ItemData(0x0300000000010000), // Monomate x1
        ItemData(0x0300010000010000), // Dimate x1
        ItemData(0x0300020000010000), // Trimate x1
        ItemData(0x0301000000010000), // Monofluid x1
        ItemData(0x0301010000010000), // Difluid x1
        ItemData(0x0301020000010000), // Trifluid x1
    };
    ItemData item = items.at(cmd.item_type);
    item.id = l->generate_item_id(0xFF);
    l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);
    send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.x, cmd.z);
  }
}

static void on_quest_exchange_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() &&
      (l->base_version == Version::BB_V4) &&
      l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_ExchangeItemInQuest_BB_6xD5>(data, size);

    try {
      auto p = c->character();

      size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, false);
      send_destroy_item_to_lobby(c, found_item.id, 1);

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.replace_item;
      new_item.enforce_min_stack_size();
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Quest item exchange failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_wrap_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4)) {
    const auto& cmd = check_size_t<G_WrapItem_BB_6xD6>(data, size);

    auto p = c->character();
    auto item = p->remove_item(cmd.item.id, 1, false);
    send_destroy_item_to_lobby(c, item.id, 1);
    item.wrap();
    p->add_item(item);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
}

static void on_photon_drop_exchange_for_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4)) {
    const auto& cmd = check_size_t<G_PaganiniPhotonDropExchange_BB_6xD7>(data, size);

    try {
      auto p = c->character();

      size_t found_index = p->inventory.find_item_by_primary_identifier(0x031000);
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 0, false);
      send_destroy_item_to_lobby(c, found_item.id, found_item.stack_size());

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.new_item;
      new_item.enforce_min_stack_size();
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Quest Photon Drop exchange for item failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_photon_drop_exchange_for_s_rank_special_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4)) {
    const auto& cmd = check_size_t<G_AddSRankWeaponSpecial_BB_6xD8>(data, size);

    try {
      auto p = c->character();

      static const array<uint8_t, 0x10> costs({60, 60, 20, 20, 30, 30, 30, 50, 40, 50, 40, 40, 50, 40, 40, 40});
      uint8_t cost = costs.at(cmd.special_type);

      size_t payment_item_index = p->inventory.find_item_by_primary_identifier(0x031000);
      // Ensure weapon exists before removing PDs, so inventory state will be
      // consistent in case of error
      p->inventory.find_item(cmd.item_id);

      auto payment_item = p->remove_item(p->inventory.items[payment_item_index].data.id, cost, false);
      send_destroy_item_to_lobby(c, payment_item.id, cost);

      auto item = p->remove_item(cmd.item_id, 1, false);
      send_destroy_item_to_lobby(c, item.id, cost);
      item.data1[2] = cmd.special_type;
      p->add_item(item);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Quest Photon Drop exchange for S-rank special failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_secret_lottery_ticket_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_ExchangeSecretLotteryTicket_BB_6xDE>(data, size);

    if (s->secret_lottery_results.empty()) {
      throw runtime_error("no secret lottery results are defined");
    }

    auto p = c->character();
    ssize_t slt_index = -1;
    try {
      slt_index = p->inventory.find_item_by_primary_identifier(0x031003); // Secret Lottery Ticket
    } catch (const out_of_range&) {
    }

    if (slt_index >= 0) {
      uint32_t slt_item_id = p->inventory.items[slt_index].data.id;

      G_ExchangeItemInQuest_BB_6xDB exchange_cmd;
      exchange_cmd.header.subcommand = 0xDB;
      exchange_cmd.header.size = 4;
      exchange_cmd.header.client_id = c->lobby_client_id;
      exchange_cmd.unknown_a1 = 1;
      exchange_cmd.item_id = slt_item_id;
      exchange_cmd.amount = 1;
      send_command_t(c, 0x60, 0x00, exchange_cmd);

      send_destroy_item_to_lobby(c, slt_item_id, 1);

      ItemData item = (s->secret_lottery_results.size() == 1)
          ? s->secret_lottery_results[0]
          : s->secret_lottery_results[random_object<uint32_t>() % s->secret_lottery_results.size()];
      item.enforce_min_stack_size();
      item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(item);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
    }

    S_ExchangeSecretLotteryTicketResult_BB_24 out_cmd;
    out_cmd.start_index = cmd.index;
    out_cmd.function_id = cmd.function_id1;
    if (s->secret_lottery_results.empty()) {
      out_cmd.unknown_a3.clear(0);
    } else if (s->secret_lottery_results.size() == 1) {
      out_cmd.unknown_a3.clear(1);
    } else {
      for (size_t z = 0; z < out_cmd.unknown_a3.size(); z++) {
        out_cmd.unknown_a3[z] = random_object<uint32_t>() % s->secret_lottery_results.size();
      }
    }
    send_command_t(c, 0x24, (slt_index >= 0) ? 0 : 1, out_cmd);
  }
}

static void on_photon_crystal_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    check_size_t<G_ExchangePhotonCrystals_BB_6xDF>(data, size);
    auto p = c->character();
    size_t index = p->inventory.find_item_by_primary_identifier(0x031002);
    auto item = p->remove_item(p->inventory.items[index].data.id, 1, false);
    send_destroy_item_to_lobby(c, item.id, 1);
  }
}

static void on_quest_F95E_result_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_RequestItemDropFromQuest_BB_6xE0>(data, size);
    auto s = c->require_server_state();

    size_t count = (cmd.type > 0x03) ? 1 : (l->difficulty + 1);
    for (size_t z = 0; z < count; z++) {
      const auto& results = s->quest_F95E_results.at(cmd.type).at(l->difficulty);
      if (results.empty()) {
        throw runtime_error("invalid result type");
      }
      ItemData item = (results.size() == 1) ? results[0] : results[random_object<uint32_t>() % results.size()];
      if (item.data1[0] == 0x04) { // Meseta
        // TODO: What is the right amount of Meseta to use here? Presumably it
        // should be random within a certain range, but it's not obvious what
        // that range should be.
        item.data2d = 100;
      } else if (item.data1[0] == 0x00) {
        item.data1[4] |= 0x80; // Unidentified
      } else {
        item.enforce_min_stack_size();
      }

      item.id = l->generate_item_id(0xFF);
      l->add_item(cmd.floor, item, cmd.x, cmd.z, 0x00F);

      send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.x, cmd.z);
    }
  }
}

static void on_quest_F95F_result_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_ExchangePhotonTickets_BB_6xE1>(data, size);
    auto s = c->require_server_state();
    auto p = c->character();

    const auto& result = s->quest_F95F_results.at(cmd.result_index);
    if (result.second.empty()) {
      throw runtime_error("invalid result index");
    }

    size_t index = p->inventory.find_item_by_primary_identifier(0x031004); // Photon Ticket
    auto ticket_item = p->remove_item(p->inventory.items[index].data.id, result.first, false);
    // TODO: Shouldn't we send a 6x29 here? Check if this causes desync in an
    // actual game

    G_ExchangeItemInQuest_BB_6xDB cmd_6xDB;
    cmd_6xDB.header = {0xDB, 0x04, c->lobby_client_id};
    cmd_6xDB.unknown_a1 = 1;
    cmd_6xDB.item_id = ticket_item.id;
    cmd_6xDB.amount = result.first;
    send_command_t(c, 0x60, 0x00, cmd_6xDB);

    ItemData new_item = result.second;
    new_item.enforce_min_stack_size();
    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    S_GallonPlanResult_BB_25 out_cmd;
    out_cmd.function_id = cmd.function_id1;
    out_cmd.offset1 = 0x3C;
    out_cmd.offset2 = 0x08;
    out_cmd.value1 = 0x00;
    out_cmd.value2 = cmd.result_index;
    send_command_t(c, 0x25, 0x00, out_cmd);
  }
}

static void on_momoka_item_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_MomokaItemExchange_BB_6xD9>(data, size);
    auto p = c->character();
    try {
      size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
      auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, false);

      G_ExchangeItemInQuest_BB_6xDB cmd_6xDB = {{0xDB, 0x04, c->lobby_client_id}, 1, found_item.id, 1};
      send_command_t(c, 0x60, 0x00, cmd_6xDB);

      send_destroy_item_to_lobby(c, found_item.id, 1);

      // TODO: We probably should use an allow-list here to prevent the client
      // from creating arbitrary items if cheat mode is disabled.
      ItemData new_item = cmd.replace_item;
      new_item.enforce_min_stack_size();
      new_item.id = l->generate_item_id(c->lobby_client_id);
      p->add_item(new_item);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

      send_command(c, 0x23, 0x00);
    } catch (const exception& e) {
      c->log.warning("Momoka item exchange failed: %s", e.what());
      send_command(c, 0x23, 0x01);
    }
  }
}

static void on_upgrade_weapon_attribute_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && (l->base_version == Version::BB_V4) && l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    const auto& cmd = check_size_t<G_UpgradeWeaponAttribute_BB_6xDA>(data, size);
    auto p = c->character();
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
      send_destroy_item_to_lobby(c, payment_item.id, cmd.payment_count);

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
        if ((item.data1[z] == 0) || (!(item.data1[z] & 0x80) && (item.data1[z] == cmd.attribute))) {
          attribute_index = z;
          break;
        }
      }
      if (attribute_index == 0) {
        throw runtime_error("no available attribute slots");
      }
      item.data1[attribute_index] = cmd.attribute;
      item.data1[attribute_index + 1] += attribute_amount;

      send_destroy_item_to_lobby(c, item.id, 1);
      send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
      send_quest_function_call(c, cmd.success_function_id);

    } catch (const exception& e) {
      c->log.warning("Weapon attribute upgrade failed: %s", e.what());
      send_quest_function_call(c, cmd.failure_function_id);
    }
  }
}

static void on_write_quest_global_flag_bb(shared_ptr<Client> c, uint8_t, uint8_t, const void* data, size_t size) {
  const auto& cmd = check_size_t<G_SetQuestGlobalFlag_BB_6xD2>(data, size);
  c->character()->quest_global_flags[cmd.index] = cmd.value;
}

////////////////////////////////////////////////////////////////////////////////

const SubcommandDefinition subcommand_definitions[0x100] = {
    /* 6x00 */ {0x00, 0x00, 0x00, on_invalid},
    /* 6x01 */ {0x01, 0x01, 0x01, on_invalid},
    /* 6x02 */ {0x02, 0x02, 0x02, nullptr},
    /* 6x03 */ {0x03, 0x03, 0x03, nullptr},
    /* 6x04 */ {0x04, 0x04, 0x04, nullptr},
    /* 6x05 */ {0x05, 0x05, 0x05, on_switch_state_changed},
    /* 6x06 */ {0x06, 0x06, 0x06, on_send_guild_card},
    /* 6x07 */ {0x07, 0x07, 0x07, on_symbol_chat, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6x08 */ {0x08, 0x08, 0x08, nullptr},
    /* 6x09 */ {0x09, 0x09, 0x09, nullptr},
    /* 6x0A */ {0x0A, 0x0A, 0x0A, on_enemy_hit},
    /* 6x0B */ {0x0B, 0x0B, 0x0B, on_forward_check_game},
    /* 6x0C */ {0x0C, 0x0C, 0x0C, on_forward_check_game},
    /* 6x0D */ {0x00, 0x00, 0x0D, on_forward_check_game},
    /* 6x0E */ {0x00, 0x00, 0x0E, nullptr},
    /* 6x0F */ {0x00, 0x00, 0x0F, on_invalid},
    /* 6x10 */ {0x0E, 0x0E, 0x10, nullptr},
    /* 6x11 */ {0x0F, 0x0F, 0x11, nullptr},
    /* 6x12 */ {0x10, 0x10, 0x12, on_dragon_actions},
    /* 6x13 */ {0x11, 0x11, 0x13, on_forward_check_game},
    /* 6x14 */ {0x12, 0x12, 0x14, on_forward_check_game},
    /* 6x15 */ {0x13, 0x13, 0x15, on_forward_check_game},
    /* 6x16 */ {0x14, 0x14, 0x16, on_forward_check_game},
    /* 6x17 */ {0x15, 0x15, 0x17, on_forward_check_game},
    /* 6x18 */ {0x16, 0x16, 0x18, on_forward_check_game},
    /* 6x19 */ {0x17, 0x17, 0x19, on_forward_check_game},
    /* 6x1A */ {0x00, 0x00, 0x1A, on_invalid},
    /* 6x1B */ {0x00, 0x19, 0x1B, on_forward_check_game},
    /* 6x1C */ {0x00, 0x1A, 0x1C, on_forward_check_game},
    /* 6x1D */ {0x19, 0x1B, 0x1D, on_invalid},
    /* 6x1E */ {0x1A, 0x1C, 0x1E, on_invalid},
    /* 6x1F */ {0x1B, 0x1D, 0x1F, on_change_floor_6x1F},
    /* 6x20 */ {0x1C, 0x1E, 0x20, on_movement_with_floor<G_SetPosition_6x20>},
    /* 6x21 */ {0x1D, 0x1F, 0x21, on_change_floor_6x21},
    /* 6x22 */ {0x1E, 0x20, 0x22, on_forward_check_client}, // Formerly on_set_player_invisible
    /* 6x23 */ {0x1F, 0x21, 0x23, on_set_player_visible},
    /* 6x24 */ {0x20, 0x22, 0x24, on_forward_check_game},
    /* 6x25 */ {0x21, 0x23, 0x25, on_equip_item},
    /* 6x26 */ {0x22, 0x24, 0x26, on_unequip_item},
    /* 6x27 */ {0x23, 0x25, 0x27, on_use_item},
    /* 6x28 */ {0x24, 0x26, 0x28, on_feed_mag},
    /* 6x29 */ {0x25, 0x27, 0x29, on_destroy_inventory_item},
    /* 6x2A */ {0x26, 0x28, 0x2A, on_player_drop_item},
    /* 6x2B */ {0x27, 0x29, 0x2B, on_create_inventory_item},
    /* 6x2C */ {0x28, 0x2A, 0x2C, on_forward_check_client},
    /* 6x2D */ {0x29, 0x2B, 0x2D, on_forward_check_client},
    /* 6x2E */ {0x2A, 0x2C, 0x2E, on_forward_check_client},
    /* 6x2F */ {0x2B, 0x2D, 0x2F, on_hit_by_enemy},
    /* 6x30 */ {0x2C, 0x2E, 0x30, on_level_up},
    /* 6x31 */ {0x2D, 0x2F, 0x31, on_forward_check_game},
    /* 6x32 */ {0x00, 0x00, 0x32, on_forward_check_game},
    /* 6x33 */ {0x2E, 0x30, 0x33, on_forward_check_game},
    /* 6x34 */ {0x2F, 0x31, 0x34, nullptr},
    /* 6x35 */ {0x30, 0x32, 0x35, nullptr},
    /* 6x36 */ {0x00, 0x00, 0x36, on_forward_check_game},
    /* 6x37 */ {0x32, 0x33, 0x37, on_forward_check_game},
    /* 6x38 */ {0x33, 0x34, 0x38, nullptr},
    /* 6x39 */ {0x00, 0x36, 0x39, on_forward_check_game},
    /* 6x3A */ {0x00, 0x37, 0x3A, on_forward_check_game},
    /* 6x3B */ {0x00, 0x38, 0x3B, forward_subcommand},
    /* 6x3C */ {0x34, 0x39, 0x3C, nullptr},
    /* 6x3D */ {0x00, 0x00, 0x3D, nullptr},
    /* 6x3E */ {0x00, 0x00, 0x3E, on_movement_with_floor<G_StopAtPosition_6x3E>},
    /* 6x3F */ {0x36, 0x3B, 0x3F, on_movement_with_floor<G_SetPosition_6x3F>},
    /* 6x40 */ {0x37, 0x3C, 0x40, on_movement<G_WalkToPosition_6x40>},
    /* 6x41 */ {0x38, 0x3D, 0x41, nullptr},
    /* 6x42 */ {0x39, 0x3E, 0x42, on_movement<G_RunToPosition_6x42>},
    /* 6x43 */ {0x3A, 0x3F, 0x43, on_forward_check_game_client},
    /* 6x44 */ {0x3B, 0x40, 0x44, on_forward_check_game_client},
    /* 6x45 */ {0x3C, 0x41, 0x45, on_forward_check_game_client},
    /* 6x46 */ {0x00, 0x42, 0x46, on_attack_finished},
    /* 6x47 */ {0x3D, 0x43, 0x47, on_cast_technique},
    /* 6x48 */ {0x00, 0x00, 0x48, on_cast_technique_finished},
    /* 6x49 */ {0x3E, 0x44, 0x49, on_subtract_pb_energy},
    /* 6x4A */ {0x3F, 0x45, 0x4A, on_forward_check_game_client},
    /* 6x4B */ {0x40, 0x46, 0x4B, on_hit_by_enemy},
    /* 6x4C */ {0x41, 0x47, 0x4C, on_hit_by_enemy},
    /* 6x4D */ {0x42, 0x48, 0x4D, on_player_died},
    /* 6x4E */ {0x00, 0x00, 0x4E, on_forward_check_game_client},
    /* 6x4F */ {0x43, 0x49, 0x4F, on_forward_check_game_client},
    /* 6x50 */ {0x44, 0x4A, 0x50, on_forward_check_game_client},
    /* 6x51 */ {0x00, 0x00, 0x51, nullptr},
    /* 6x52 */ {0x46, 0x4C, 0x52, forward_subcommand},
    /* 6x53 */ {0x47, 0x4D, 0x53, on_forward_check_game},
    /* 6x54 */ {0x48, 0x4E, 0x54, nullptr},
    /* 6x55 */ {0x49, 0x4F, 0x55, on_forward_check_game_client},
    /* 6x56 */ {0x4A, 0x50, 0x56, on_forward_check_client},
    /* 6x57 */ {0x00, 0x51, 0x57, on_forward_check_client},
    /* 6x58 */ {0x00, 0x00, 0x58, on_forward_check_client},
    /* 6x59 */ {0x4B, 0x52, 0x59, on_pick_up_item},
    /* 6x5A */ {0x4C, 0x53, 0x5A, on_pick_up_item_request},
    /* 6x5B */ {0x4D, 0x54, 0x5B, nullptr},
    /* 6x5C */ {0x4E, 0x55, 0x5C, nullptr},
    /* 6x5D */ {0x4F, 0x56, 0x5D, on_drop_partial_stack},
    /* 6x5E */ {0x50, 0x57, 0x5E, on_buy_shop_item},
    /* 6x5F */ {0x51, 0x58, 0x5F, on_box_or_enemy_item_drop},
    /* 6x60 */ {0x52, 0x59, 0x60, on_entity_drop_item_request},
    /* 6x61 */ {0x53, 0x5A, 0x61, on_forward_check_game},
    /* 6x62 */ {0x54, 0x5B, 0x62, nullptr},
    /* 6x63 */ {0x55, 0x5C, 0x63, on_destroy_floor_item},
    /* 6x64 */ {0x56, 0x5D, 0x64, nullptr},
    /* 6x65 */ {0x57, 0x5E, 0x65, nullptr},
    /* 6x66 */ {0x00, 0x00, 0x66, on_forward_check_game},
    /* 6x67 */ {0x58, 0x5F, 0x67, on_forward_check_game},
    /* 6x68 */ {0x59, 0x60, 0x68, on_forward_check_game},
    /* 6x69 */ {0x5A, 0x61, 0x69, on_npc_control},
    /* 6x6A */ {0x5B, 0x62, 0x6A, on_forward_check_game},
    /* 6x6B */ {0x5C, 0x63, 0x6B, on_forward_sync_joining_player_state},
    /* 6x6C */ {0x5D, 0x64, 0x6C, on_forward_sync_joining_player_state},
    /* 6x6D */ {0x5E, 0x65, 0x6D, on_sync_joining_player_item_state},
    /* 6x6E */ {0x5F, 0x66, 0x6E, on_forward_sync_joining_player_state},
    /* 6x6F */ {0x00, 0x00, 0x6F, on_forward_check_game_loading},
    /* 6x70 */ {0x60, 0x67, 0x70, on_sync_joining_player_disp_and_inventory},
    /* 6x71 */ {0x00, 0x00, 0x71, on_forward_check_game_loading},
    /* 6x72 */ {0x61, 0x68, 0x72, on_forward_check_game_loading},
    /* 6x73 */ {0x00, 0x00, 0x73, on_invalid},
    /* 6x74 */ {0x62, 0x69, 0x74, on_word_select, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6x75 */ {0x00, 0x00, 0x75, on_set_quest_flag},
    /* 6x76 */ {0x00, 0x00, 0x76, on_forward_check_game},
    /* 6x77 */ {0x00, 0x00, 0x77, on_forward_check_game},
    /* 6x78 */ {0x00, 0x00, 0x78, nullptr},
    /* 6x79 */ {0x00, 0x00, 0x79, on_forward_check_lobby_client},
    /* 6x7A */ {0x00, 0x00, 0x7A, nullptr},
    /* 6x7B */ {0x00, 0x00, 0x7B, nullptr},
    /* 6x7C */ {0x00, 0x00, 0x7C, on_forward_check_game},
    /* 6x7D */ {0x00, 0x00, 0x7D, on_forward_check_game},
    /* 6x7E */ {0x00, 0x00, 0x7E, nullptr},
    /* 6x7F */ {0x00, 0x00, 0x7F, nullptr},
    /* 6x80 */ {0x00, 0x00, 0x80, on_forward_check_game},
    /* 6x81 */ {0x00, 0x00, 0x81, nullptr},
    /* 6x82 */ {0x00, 0x00, 0x82, nullptr},
    /* 6x83 */ {0x00, 0x00, 0x83, on_forward_check_game},
    /* 6x84 */ {0x00, 0x00, 0x84, on_forward_check_game},
    /* 6x85 */ {0x00, 0x00, 0x85, on_forward_check_game},
    /* 6x86 */ {0x00, 0x00, 0x86, on_forward_check_game},
    /* 6x87 */ {0x00, 0x00, 0x87, on_forward_check_game},
    /* 6x88 */ {0x00, 0x00, 0x88, on_forward_check_game},
    /* 6x89 */ {0x00, 0x00, 0x89, on_forward_check_game},
    /* 6x8A */ {0x00, 0x00, 0x8A, on_forward_check_game},
    /* 6x8B */ {0x00, 0x00, 0x8B, nullptr},
    /* 6x8C */ {0x00, 0x00, 0x8C, nullptr},
    /* 6x8D */ {0x00, 0x00, 0x8D, on_forward_check_game_client},
    /* 6x8E */ {0x00, 0x00, 0x8E, nullptr},
    /* 6x8F */ {0x00, 0x00, 0x8F, nullptr},
    /* 6x90 */ {0x00, 0x00, 0x90, nullptr},
    /* 6x91 */ {0x00, 0x00, 0x91, on_forward_check_game},
    /* 6x92 */ {0x00, 0x00, 0x92, nullptr},
    /* 6x93 */ {0x00, 0x00, 0x93, on_forward_check_game},
    /* 6x94 */ {0x00, 0x00, 0x94, on_warp},
    /* 6x95 */ {0x00, 0x00, 0x95, nullptr},
    /* 6x96 */ {0x00, 0x00, 0x96, nullptr},
    /* 6x97 */ {0x00, 0x00, 0x97, on_forward_check_game},
    /* 6x98 */ {0x00, 0x00, 0x98, nullptr},
    /* 6x99 */ {0x00, 0x00, 0x99, nullptr},
    /* 6x9A */ {0x00, 0x00, 0x9A, on_forward_check_game_client},
    /* 6x9B */ {0x00, 0x00, 0x9B, on_forward_check_game},
    /* 6x9C */ {0x00, 0x00, 0x9C, on_forward_check_game},
    /* 6x9D */ {0x00, 0x00, 0x9D, on_forward_check_game},
    /* 6x9E */ {0x00, 0x00, 0x9E, nullptr},
    /* 6x9F */ {0x00, 0x00, 0x9F, on_forward_check_game},
    /* 6xA0 */ {0x00, 0x00, 0xA0, on_forward_check_game},
    /* 6xA1 */ {0x00, 0x00, 0xA1, on_forward_check_game},
    /* 6xA2 */ {0x00, 0x00, 0xA2, on_entity_drop_item_request},
    /* 6xA3 */ {0x00, 0x00, 0xA3, on_forward_check_game},
    /* 6xA4 */ {0x00, 0x00, 0xA4, on_forward_check_game},
    /* 6xA5 */ {0x00, 0x00, 0xA5, on_forward_check_game},
    /* 6xA6 */ {0x00, 0x00, 0xA6, on_forward_check_game},
    /* 6xA7 */ {0x00, 0x00, 0xA7, nullptr},
    /* 6xA8 */ {0x00, 0x00, 0xA8, on_gol_dragon_actions},
    /* 6xA9 */ {0x00, 0x00, 0xA9, on_forward_check_game},
    /* 6xAA */ {0x00, 0x00, 0xAA, on_forward_check_game},
    /* 6xAB */ {0x00, 0x00, 0xAB, on_forward_check_lobby_client},
    /* 6xAC */ {0x00, 0x00, 0xAC, nullptr},
    /* 6xAD */ {0x00, 0x00, 0xAD, on_forward_check_game},
    /* 6xAE */ {0x00, 0x00, 0xAE, on_forward_check_client},
    /* 6xAF */ {0x00, 0x00, 0xAF, on_forward_check_lobby_client},
    /* 6xB0 */ {0x00, 0x00, 0xB0, on_forward_check_lobby_client},
    /* 6xB1 */ {0x00, 0x00, 0xB1, nullptr},
    /* 6xB2 */ {0x00, 0x00, 0xB2, nullptr},
    /* 6xB3 */ {0x00, 0x00, 0xB3, nullptr}, // Should be sent via CA instead
    /* 6xB4 */ {0x00, 0x00, 0xB4, nullptr}, // Should be sent by the server only
    /* 6xB5 */ {0x00, 0x00, 0xB5, on_open_shop_bb_or_ep3_battle_subs},
    /* 6xB6 */ {0x00, 0x00, 0xB6, nullptr},
    /* 6xB7 */ {0x00, 0x00, 0xB7, on_buy_shop_item_bb},
    /* 6xB8 */ {0x00, 0x00, 0xB8, on_identify_item_bb},
    /* 6xB9 */ {0x00, 0x00, 0xB9, nullptr},
    /* 6xBA */ {0x00, 0x00, 0xBA, on_accept_identify_item_bb},
    /* 6xBB */ {0x00, 0x00, 0xBB, on_open_bank_bb_or_card_trade_counter_ep3},
    /* 6xBC */ {0x00, 0x00, 0xBC, on_forward_check_ep3_game},
    /* 6xBD */ {0x00, 0x00, 0xBD, on_ep3_private_word_select_bb_bank_action, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6xBE */ {0x00, 0x00, 0xBE, forward_subcommand, SDF::ALWAYS_FORWARD_TO_WATCHERS | SDF::ALLOW_FORWARD_TO_WATCHED_LOBBY},
    /* 6xBF */ {0x00, 0x00, 0xBF, on_forward_check_ep3_lobby},
    /* 6xC0 */ {0x00, 0x00, 0xC0, on_sell_item_at_shop_bb},
    /* 6xC1 */ {0x00, 0x00, 0xC1, forward_subcommand},
    /* 6xC2 */ {0x00, 0x00, 0xC2, forward_subcommand},
    /* 6xC3 */ {0x00, 0x00, 0xC3, on_drop_partial_stack_bb},
    /* 6xC4 */ {0x00, 0x00, 0xC4, on_sort_inventory_bb},
    /* 6xC5 */ {0x00, 0x00, 0xC5, on_medical_center_bb},
    /* 6xC6 */ {0x00, 0x00, 0xC6, on_steal_exp_bb},
    /* 6xC7 */ {0x00, 0x00, 0xC7, on_charge_attack_bb},
    /* 6xC8 */ {0x00, 0x00, 0xC8, on_enemy_exp_request_bb},
    /* 6xC9 */ {0x00, 0x00, 0xC9, on_meseta_reward_request_bb},
    /* 6xCA */ {0x00, 0x00, 0xCA, on_item_reward_request_bb},
    /* 6xCB */ {0x00, 0x00, 0xCB, on_transfer_item_via_mail_message_bb},
    /* 6xCC */ {0x00, 0x00, 0xCC, on_exchange_item_for_team_points_bb},
    /* 6xCD */ {0x00, 0x00, 0xCD, forward_subcommand},
    /* 6xCE */ {0x00, 0x00, 0xCE, forward_subcommand},
    /* 6xCF */ {0x00, 0x00, 0xCF, on_battle_restart_bb},
    /* 6xD0 */ {0x00, 0x00, 0xD0, on_battle_level_up_bb},
    /* 6xD1 */ {0x00, 0x00, 0xD1, on_request_challenge_grave_recovery_item_bb},
    /* 6xD2 */ {0x00, 0x00, 0xD2, on_write_quest_global_flag_bb},
    /* 6xD3 */ {0x00, 0x00, 0xD3, nullptr},
    /* 6xD4 */ {0x00, 0x00, 0xD4, nullptr},
    /* 6xD5 */ {0x00, 0x00, 0xD5, on_quest_exchange_item_bb},
    /* 6xD6 */ {0x00, 0x00, 0xD6, on_wrap_item_bb},
    /* 6xD7 */ {0x00, 0x00, 0xD7, on_photon_drop_exchange_for_item_bb},
    /* 6xD8 */ {0x00, 0x00, 0xD8, on_photon_drop_exchange_for_s_rank_special_bb},
    /* 6xD9 */ {0x00, 0x00, 0xD9, on_momoka_item_exchange_bb},
    /* 6xDA */ {0x00, 0x00, 0xDA, on_upgrade_weapon_attribute_bb},
    /* 6xDB */ {0x00, 0x00, 0xDB, nullptr},
    /* 6xDC */ {0x00, 0x00, 0xDC, on_forward_check_game},
    /* 6xDD */ {0x00, 0x00, 0xDD, nullptr},
    /* 6xDE */ {0x00, 0x00, 0xDE, on_secret_lottery_ticket_exchange_bb},
    /* 6xDF */ {0x00, 0x00, 0xDF, on_photon_crystal_exchange_bb},
    /* 6xE0 */ {0x00, 0x00, 0xE0, on_quest_F95E_result_bb},
    /* 6xE1 */ {0x00, 0x00, 0xE1, on_quest_F95F_result_bb},
    /* 6xE2 */ {0x00, 0x00, 0xE2, nullptr},
    /* 6xE3 */ {0x00, 0x00, 0xE3, nullptr},
    /* 6xE4 */ {0x00, 0x00, 0xE4, nullptr},
    /* 6xE5 */ {0x00, 0x00, 0xE5, nullptr},
    /* 6xE6 */ {0x00, 0x00, 0xE6, nullptr},
    /* 6xE7 */ {0x00, 0x00, 0xE7, nullptr},
    /* 6xE8 */ {0x00, 0x00, 0xE8, nullptr},
    /* 6xE9 */ {0x00, 0x00, 0xE9, nullptr},
    /* 6xEA */ {0x00, 0x00, 0xEA, nullptr},
    /* 6xEB */ {0x00, 0x00, 0xEB, nullptr},
    /* 6xEC */ {0x00, 0x00, 0xEC, nullptr},
    /* 6xED */ {0x00, 0x00, 0xED, nullptr},
    /* 6xEE */ {0x00, 0x00, 0xEE, nullptr},
    /* 6xEF */ {0x00, 0x00, 0xEF, nullptr},
    /* 6xF0 */ {0x00, 0x00, 0xF0, nullptr},
    /* 6xF1 */ {0x00, 0x00, 0xF1, nullptr},
    /* 6xF2 */ {0x00, 0x00, 0xF2, nullptr},
    /* 6xF3 */ {0x00, 0x00, 0xF3, nullptr},
    /* 6xF4 */ {0x00, 0x00, 0xF4, nullptr},
    /* 6xF5 */ {0x00, 0x00, 0xF5, nullptr},
    /* 6xF6 */ {0x00, 0x00, 0xF6, nullptr},
    /* 6xF7 */ {0x00, 0x00, 0xF7, nullptr},
    /* 6xF8 */ {0x00, 0x00, 0xF8, nullptr},
    /* 6xF9 */ {0x00, 0x00, 0xF9, nullptr},
    /* 6xFA */ {0x00, 0x00, 0xFA, nullptr},
    /* 6xFB */ {0x00, 0x00, 0xFB, nullptr},
    /* 6xFC */ {0x00, 0x00, 0xFC, nullptr},
    /* 6xFD */ {0x00, 0x00, 0xFD, nullptr},
    /* 6xFE */ {0x00, 0x00, 0xFE, nullptr},
    /* 6xFF */ {0x00, 0x00, 0xFF, nullptr},
};

void on_subcommand_multi(shared_ptr<Client> c, uint8_t command, uint8_t flag, const string& data) {
  if (data.empty()) {
    throw runtime_error("game command is empty");
  }

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

    const SubcommandDefinition* def;
    if (c->version() == Version::DC_NTE) {
      def = &def_for_nte_subcommand(header.subcommand);
    } else if (c->version() == Version::DC_V1_11_2000_PROTOTYPE) {
      def = &def_for_proto_subcommand(header.subcommand);
    } else {
      def = &subcommand_definitions[header.subcommand];
    }
    if (def->handler) {
      def->handler(c, command, flag, data, size);
    } else {
      on_unimplemented(c, command, flag, data, size);
    }
  }
}

bool subcommand_is_implemented(uint8_t which) {
  return subcommand_definitions[which].handler != nullptr;
}
