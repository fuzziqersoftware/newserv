#include "ReceiveSubcommands.hh"

#include <math.h>
#include <string.h>

#include <memory>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Vector.hh>

#include "Client.hh"
#include "Compression.hh"
#include "HTTPServer.hh"
#include "Items.hh"
#include "Lobby.hh"
#include "Loggers.hh"
#include "Map.hh"
#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

using SubcommandHandler = void (*)(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size);

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

static const SubcommandDefinition* def_for_nte_subcommand(uint8_t subcommand) {
  static std::array<uint8_t, 0x100> nte_to_final_map;
  static bool nte_to_final_map_populated = false;
  if (!nte_to_final_map_populated) {
    nte_to_final_map.fill(0);
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
  uint8_t final_subcommand = nte_to_final_map[subcommand];
  return final_subcommand ? &subcommand_definitions[final_subcommand] : nullptr;
}

static const SubcommandDefinition* def_for_proto_subcommand(uint8_t subcommand) {
  static std::array<uint8_t, 0x100> proto_to_final_map;
  static bool proto_to_final_map_populated = false;
  if (!proto_to_final_map_populated) {
    proto_to_final_map.fill(0);
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
  uint8_t final_subcommand = proto_to_final_map[subcommand];
  return final_subcommand ? &subcommand_definitions[final_subcommand] : nullptr;
}

const SubcommandDefinition* def_for_subcommand(Version version, uint8_t subcommand) {
  if (version == Version::DC_NTE) {
    return def_for_nte_subcommand(subcommand);
  } else if (version == Version::DC_11_2000) {
    return def_for_proto_subcommand(subcommand);
  } else {
    return &subcommand_definitions[subcommand];
  }
}

uint8_t translate_subcommand_number(Version to_version, Version from_version, uint8_t subcommand) {
  const auto* def = def_for_subcommand(from_version, subcommand);
  if (!def) {
    return 0x00;
  } else if (to_version == Version::DC_NTE) {
    return def->nte_subcommand;
  } else if (to_version == Version::DC_11_2000) {
    return def->proto_subcommand;
  } else {
    return def->final_subcommand;
  }
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
  const auto* def = def_for_subcommand(c->version(), header.subcommand);
  uint8_t def_flags = def ? def->flags : 0;

  string nte_data;
  string proto_data;
  string final_data;
  Version c_version = c->version();
  auto send_to_client = [&](shared_ptr<Client> lc) -> void {
    Version lc_version = lc->version();
    const void* data_to_send = nullptr;
    size_t size_to_send = 0;
    if ((!is_pre_v1(lc_version) && !is_pre_v1(c_version)) || (lc_version == c_version)) {
      data_to_send = data;
      size_to_send = size;
    } else if (lc->version() == Version::DC_NTE) {
      if (def && def->nte_subcommand) {
        if (nte_data.empty()) {
          nte_data.assign(reinterpret_cast<const char*>(data), size);
          nte_data[0] = def->nte_subcommand;
        }
        data_to_send = nte_data.data();
        size_to_send = nte_data.size();
      }
    } else if (lc->version() == Version::DC_11_2000) {
      if (def && def->proto_subcommand) {
        if (proto_data.empty()) {
          proto_data.assign(reinterpret_cast<const char*>(data), size);
          proto_data[0] = def->proto_subcommand;
        }
        data_to_send = proto_data.data();
        size_to_send = proto_data.size();
      }
    } else {
      if (def && def->final_subcommand) {
        if (final_data.empty()) {
          final_data.assign(reinterpret_cast<const char*>(data), size);
          final_data[0] = def->final_subcommand;
        }
        data_to_send = final_data.data();
        size_to_send = final_data.size();
      }
    }

    if (!data_to_send || !size_to_send) {
      lc->log.info("Command cannot be translated to client\'s version");
    } else {
      if ((command == 0xCB) && (lc->version() == Version::GC_EP3_NTE)) {
        command = 0xC9;
      }
      if (lc->game_join_command_queue) {
        lc->log.info("Client not ready to receive join commands; adding to queue");
        auto& cmd = lc->game_join_command_queue->emplace_back();
        cmd.command = command;
        cmd.flag = flag;
        cmd.data.assign(reinterpret_cast<const char*>(data_to_send), size_to_send);
      } else {
        send_command(lc, command, flag, data_to_send, size_to_send);
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
          (def_flags & SDF::ALLOW_FORWARD_TO_WATCHED_LOBBY)) {
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
        (def_flags & SDF::ALWAYS_FORWARD_TO_WATCHERS)) {
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        for (auto& target : watcher_lobby->clients) {
          if (target && is_ep3(target->version())) {
            send_to_client(target);
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

static void forward_subcommand_m(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
static void forward_subcommand_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& cmd) {
  forward_subcommand(c, command, flag, &cmd, sizeof(cmd));
}

static void on_invalid(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  if ((c->version() == Version::DC_NTE) || c->version() == Version::DC_11_2000) {
    c->log.error("Unrecognized DC NTE/prototype subcommand: %02hhX", cmd.subcommand);
    forward_subcommand(c, command, flag, data, size);
  } else if (command_is_private(command)) {
    c->log.error("Invalid subcommand: %02hhX (private to %hhu)", cmd.subcommand, flag);
  } else {
    c->log.error("Invalid subcommand: %02hhX (public)", cmd.subcommand);
  }
}

static void on_unimplemented(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  if ((c->version() == Version::DC_NTE) || c->version() == Version::DC_11_2000) {
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

static void on_forward_check_game_loading(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && l->any_client_loading()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_game_quest(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game() && l->quest) {
    forward_subcommand(c, command, flag, data, size);
  }
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
  for (auto& lc : l->clients) {
    if (!lc || lc == c) {
      continue;
    }
    if (c->version() != lc->version()) {
      CmdT out_cmd = cmd;
      out_cmd.header.subcommand = translate_subcommand_number(lc->version(), c->version(), out_cmd.header.subcommand);
      if (out_cmd.header.subcommand) {
        out_cmd.item_data.decode_for_version(c->version());
        out_cmd.item_data.encode_for_version(lc->version(), s->item_parameter_table_for_encode(lc->version()));
        send_command_t(lc, command, flag, out_cmd);
      } else {
        lc->log.info("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command_t(lc, command, flag, cmd);
    }
  }
}

template <typename CmdT, bool ForwardIfMissing = false, size_t EntityIDOffset = offsetof(G_EntityIDHeader, entity_id)>
void forward_subcommand_with_entity_id_transcode_t(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  // I'm lazy and this should never happen for item commands (since all players
  // need to stay in sync)
  if (command_is_private(command)) {
    throw runtime_error("entity subcommand sent via private command");
  }

  auto& cmd = check_size_t<CmdT>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("command cannot be used outside of a game");
  }

  le_uint16_t& cmd_entity_id = *reinterpret_cast<le_uint16_t*>(reinterpret_cast<uint8_t*>(&cmd) + EntityIDOffset);

  shared_ptr<const MapState::EnemyState> ene_st;
  shared_ptr<const MapState::ObjectState> obj_st;
  if ((cmd_entity_id >= 0x1000) && (cmd_entity_id < 0x4000)) {
    ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd_entity_id - 0x1000);
  } else if ((cmd_entity_id >= 0x4000) && (cmd_entity_id < 0xFFFF)) {
    obj_st = l->map_state->object_state_for_index(c->version(), c->floor, cmd_entity_id - 0x4000);
  }

  for (auto& lc : l->clients) {
    if (!lc || lc == c) {
      continue;
    }
    if (c->version() != lc->version()) {
      cmd.header.subcommand = translate_subcommand_number(lc->version(), c->version(), cmd.header.subcommand);
      if (cmd.header.subcommand) {
        bool should_forward = true;
        if (ene_st) {
          cmd_entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), ene_st);
          should_forward = ForwardIfMissing || (cmd_entity_id != 0xFFFF);
        } else if (obj_st) {
          cmd_entity_id = 0x4000 | l->map_state->index_for_object_state(lc->version(), obj_st);
          should_forward = ForwardIfMissing || (cmd_entity_id != 0xFFFF);
        }
        if (should_forward) {
          send_command_t(lc, command, flag, cmd);
        }
      } else {
        lc->log.info("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command_t(lc, command, flag, cmd);
    }
  }
}

template <typename CmdT>
void forward_subcommand_with_entity_targets_transcode_t(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  // I'm lazy and this should never happen for item commands (since all players
  // need to stay in sync)
  if (command_is_private(command)) {
    throw runtime_error("entity subcommand sent via private command");
  }

  const auto& cmd = check_size_t<CmdT>(data, size, offsetof(CmdT, targets), sizeof(CmdT));
  if (cmd.target_count > min<size_t>(cmd.header.size - offsetof(CmdT, targets) / 4, cmd.targets.size())) {
    throw runtime_error("invalid attack finished command");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("command cannot be used outside of a game");
  }

  struct TargetResolution {
    shared_ptr<const MapState::EnemyState> ene_st;
    shared_ptr<const MapState::ObjectState> obj_st;
    uint16_t entity_id;
  };
  vector<TargetResolution> resolutions;
  for (size_t z = 0; z < cmd.target_count; z++) {
    auto& res = resolutions.emplace_back(TargetResolution{nullptr, nullptr, cmd.targets[z].entity_id});
    if ((res.entity_id >= 0x1000) && (res.entity_id < 0x4000)) {
      res.ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, res.entity_id - 0x1000);
    } else if ((res.entity_id >= 0x4000) && (res.entity_id < 0xFFFF)) {
      res.obj_st = l->map_state->object_state_for_index(c->version(), c->floor, res.entity_id - 0x4000);
    }
  }

  for (auto& lc : l->clients) {
    if (!lc || lc == c) {
      continue;
    }
    if (c->version() != lc->version()) {
      CmdT out_cmd = cmd;
      out_cmd.header.subcommand = translate_subcommand_number(lc->version(), c->version(), cmd.header.subcommand);
      if (out_cmd.header.subcommand) {
        size_t valid_targets = 0;
        for (size_t z = 0; z < cmd.target_count; z++) {
          const auto& res = resolutions[z];
          auto& target = out_cmd.targets[valid_targets];
          if (res.ene_st) {
            target.entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), res.ene_st);
          } else if (res.obj_st) {
            target.entity_id = 0x4000 | l->map_state->index_for_object_state(lc->version(), res.obj_st);
          } else {
            target.entity_id = res.entity_id;
          }
          if (target.entity_id != 0xFFFF) {
            target.unknown_a2 = cmd.targets[z].unknown_a2;
            valid_targets++;
          }
        }
        size_t out_size = offsetof(CmdT, targets) + sizeof(TargetEntry) * valid_targets;
        out_cmd.header.size = out_size >> 2;
        out_cmd.target_count = valid_targets;
        send_command(lc, command, flag, &out_cmd, out_size);
      } else {
        lc->log.info("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command(lc, command, flag, data, size);
    }
  }
}

static shared_ptr<Client> get_sync_target(shared_ptr<Client> sender_c, uint8_t command, uint8_t flag, bool allow_if_not_loading) {
  if (!command_is_private(command)) {
    throw runtime_error("sync data sent via public command");
  }
  auto l = sender_c->require_lobby();
  if (l->is_game() && (allow_if_not_loading || l->any_client_loading()) && (flag < l->max_clients)) {
    return l->clients[flag];
  }
  return nullptr;
}

static void on_sync_joining_player_compressed_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto target = get_sync_target(c, command, flag, false); // Checks l->is_game
  if (!target) {
    return;
  }

  uint8_t orig_subcommand_number;
  size_t decompressed_size;
  size_t compressed_size;
  const void* compressed_data;
  if (is_pre_v1(c->version())) {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    orig_subcommand_number = cmd.header.basic_header.subcommand;
    decompressed_size = cmd.decompressed_size;
    compressed_size = size - sizeof(cmd);
    compressed_data = reinterpret_cast<const char*>(data) + sizeof(cmd);
  } else {
    const auto& cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(data, size, 0xFFFF);
    if (cmd.compressed_size > size - sizeof(cmd)) {
      throw runtime_error("compressed end offset is beyond end of command");
    }
    orig_subcommand_number = cmd.header.basic_header.subcommand;
    decompressed_size = cmd.decompressed_size;
    compressed_size = cmd.compressed_size;
    compressed_data = reinterpret_cast<const char*>(data) + sizeof(cmd);
  }

  const auto* subcommand_def = def_for_subcommand(c->version(), orig_subcommand_number);
  if (!subcommand_def) {
    throw runtime_error("unknown sync subcommand");
  }

  string decompressed = bc0_decompress(compressed_data, compressed_size);
  if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info("Decompressed sync data (%zX -> %zX bytes; expected %zX):",
        compressed_size, decompressed.size(), decompressed_size);
    phosg::print_data(stderr, decompressed);
  }

  // Assume all v1 and v2 versions are the same, and assume GC/XB are the same.
  // TODO: We should do this by checking if the supermaps are the same instead
  // of hardcoding this here.
  auto collapse_version = +[](Version v) -> Version {
    // Collapse DC v1/v2 and PC into PC_V2
    if (is_v1_or_v2(v) && !is_pre_v1(v) && (v != Version::GC_NTE)) {
      return Version::PC_V2;
    }
    // Collapse GC and XB into GC_V3
    if (is_v3(v)) {
      return Version::GC_V3;
    }
    // All other versions can't be collapsed
    return v;
  };
  bool skip_recompress = collapse_version(c->version()) == collapse_version(target->version());

  switch (subcommand_def->final_subcommand) {
    case 0x6B: {
      auto l = c->require_lobby();
      l->map_state->import_enemy_states_from_sync(
          c->version(),
          reinterpret_cast<const SyncEnemyStateEntry*>(decompressed.data()),
          decompressed.size() / sizeof(SyncEnemyStateEntry));
      if (skip_recompress) {
        send_game_join_sync_command_compressed(
            target,
            compressed_data,
            compressed_size,
            decompressed_size,
            subcommand_def->nte_subcommand,
            subcommand_def->proto_subcommand,
            subcommand_def->final_subcommand);
      } else {
        send_game_enemy_state(target);
      }
      break;
    }

    case 0x6C: {
      auto l = c->require_lobby();
      l->map_state->import_object_states_from_sync(
          c->version(),
          reinterpret_cast<const SyncObjectStateEntry*>(decompressed.data()),
          decompressed.size() / sizeof(SyncObjectStateEntry));
      if (skip_recompress) {
        send_game_join_sync_command_compressed(
            target,
            compressed_data,
            compressed_size,
            decompressed_size,
            subcommand_def->nte_subcommand,
            subcommand_def->proto_subcommand,
            subcommand_def->final_subcommand);
      } else {
        send_game_object_state(target);
      }
      break;
    }

    case 0x6D: {
      if (decompressed.size() < sizeof(G_SyncItemState_6x6D_Decompressed)) {
        throw runtime_error(phosg::string_printf(
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
        throw runtime_error(phosg::string_printf(
            "decompressed 6x6D data (0x%zX bytes) is too short for all floor items (0x%zX bytes)",
            decompressed.size(), required_size));
      }

      auto l = c->require_lobby();
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

      // The leader's item state is never forwarded since the leader may be able
      // to see items that the joining player should not see. We always generate
      // a new item state for the joining player instead.
      send_game_item_state(target);
      break;
    }
    case 0x6E: {
      phosg::StringReader r(decompressed);
      const auto& dec_header = r.get<G_SyncSetFlagState_6x6E_Decompressed>();
      if (dec_header.total_size != dec_header.entity_set_flags_size + dec_header.event_set_flags_size + dec_header.switch_flags_size) {
        throw runtime_error("incorrect size fields in 6x6E header");
      }

      auto l = c->require_lobby();
      phosg::StringReader set_flags_r = r.sub(r.where(), dec_header.entity_set_flags_size);
      const auto& set_flags_header = set_flags_r.get<G_SyncSetFlagState_6x6E_Decompressed::EntitySetFlags>();

      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        c->log.info("Set flags data:");
        phosg::print_data(stderr, r.getv(dec_header.entity_set_flags_size, false), dec_header.entity_set_flags_size);
      }

      const auto* object_set_flags = &set_flags_r.get<le_uint16_t>(
          true, set_flags_header.num_object_sets * sizeof(le_uint16_t));
      const auto* enemy_set_flags = &set_flags_r.get<le_uint16_t>(
          true, set_flags_header.num_enemy_sets * sizeof(le_uint16_t));
      size_t event_set_flags_count = dec_header.event_set_flags_size / sizeof(le_uint16_t);
      const auto* event_set_flags = &r.pget<le_uint16_t>(
          r.where() + dec_header.entity_set_flags_size,
          event_set_flags_count * sizeof(le_uint16_t));
      l->map_state->import_flag_states_from_sync(
          c->version(),
          object_set_flags,
          set_flags_header.num_object_sets,
          enemy_set_flags,
          set_flags_header.num_enemy_sets,
          event_set_flags,
          event_set_flags_count);

      size_t expected_switch_flag_num_floors = is_v1(c->version()) ? 0x10 : 0x12;
      size_t expected_switch_flags_size = expected_switch_flag_num_floors * 0x20;
      if (dec_header.switch_flags_size != expected_switch_flags_size) {
        l->log.warning("Switch flags size (0x%" PRIX32 ") does not match expected size (0x%zX)",
            dec_header.switch_flags_size.load(), expected_switch_flags_size);
      } else {
        l->log.info("Switch flags size matches expected size (0x%zX)", expected_switch_flags_size);
      }
      if (l->switch_flags) {
        phosg::StringReader switch_flags_r = r.sub(r.where() + dec_header.entity_set_flags_size + dec_header.event_set_flags_size);
        for (size_t floor = 0; floor < expected_switch_flag_num_floors; floor++) {
          // There is a bug in most (perhaps all) versions of the game, which
          // causes this array to be too small. It looks like Sega forgot to
          // account for the header (G_SyncSetFlagState_6x6E_Decompressed)
          // before compressing the buffer, so the game cuts off the last 8
          // bytes of the switch flags. Since this only affects the last floor,
          // which rarely has any switches on it (or is even accessible by the
          // player), it's not surprising that no one noticed this. But it does
          // mean we have to check switch_flags_r.eof() here.
          for (size_t z = 0; (z < 0x20) && !switch_flags_r.eof(); z++) {
            uint8_t& l_flags = l->switch_flags->data[floor][z];
            uint8_t r_flags = switch_flags_r.get_u8();
            if (l_flags != r_flags) {
              l->log.warning("Switch flags do not match at floor %02zX byte %02zX (expected %02hhX, received %02hhX)",
                  floor, z, l_flags, r_flags);
              l_flags = r_flags;
            }
          }
        }
      }

      if (skip_recompress) {
        send_game_join_sync_command_compressed(
            target,
            compressed_data,
            compressed_size,
            decompressed_size,
            subcommand_def->nte_subcommand,
            subcommand_def->proto_subcommand,
            subcommand_def->final_subcommand);
      } else {
        send_game_set_state(target);
      }
      break;
    }

    default:
      throw logic_error("invalid compressed sync state subcommand");
  }
}

template <typename CmdT>
static void on_sync_joining_player_quest_flags_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  if (!command_is_private(command)) {
    return;
  }

  auto l = c->require_lobby();
  if (l->is_game() && l->any_client_loading() && (l->leader_id == c->lobby_client_id)) {
    l->quest_flags_known = nullptr; // All quest flags are now known
    l->quest_flag_values = make_unique<QuestFlags>(cmd.quest_flags);
    auto target = l->clients.at(flag);
    if (target) {
      send_game_flag_state(target);
    }
  }
}

static void on_sync_joining_player_quest_flags(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  if (is_v1(c->version())) {
    on_sync_joining_player_quest_flags_t<G_SetQuestFlags_DCv1_6x6F>(c, command, flag, data, size);
  } else if (!is_v4(c->version())) {
    on_sync_joining_player_quest_flags_t<G_SetQuestFlags_V2_V3_6x6F>(c, command, flag, data, size);
  } else {
    on_sync_joining_player_quest_flags_t<G_SetQuestFlags_BB_6x6F>(c, command, flag, data, size);
  }
}

static void transcode_inventory_items(
    parray<PlayerInventoryItem, 30>& items,
    size_t num_items,
    Version from_version,
    Version to_version,
    shared_ptr<const ItemParameterTable> to_item_parameter_table) {
  if (num_items > 30) {
    throw runtime_error("invalid inventory item count");
  }
  if (from_version != to_version) {
    for (size_t z = 0; z < num_items; z++) {
      items[z].data.decode_for_version(from_version);
      items[z].data.encode_for_version(to_version, to_item_parameter_table);
    }
  }
  for (size_t z = num_items; z < 30; z++) {
    auto& item = items[z];
    item.present = 0;
    item.unknown_a1 = 0;
    item.flags = 0;
    item.data.clear();
  }
  if (is_v1(to_version)) {
    for (size_t z = 0; z < 30; z++) {
      auto& item = items[z];
      item.extension_data1 = 0x00;
      item.extension_data2 = 0x00;
    }
  } else {
    for (size_t z = 20; z < 30; z++) {
      items[z].extension_data1 = 0x00;
    }
    for (size_t z = 16; z < 30; z++) {
      items[z].extension_data2 = 0x00;
    }
  }
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_DCNTE_6x70& cmd,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : from_version(from_version),
      from_client_customization(from_client_customization),
      item_version(from_version),
      base(cmd.base),
      unknown_a5_nte(cmd.unknown_a5),
      unknown_a6_nte(cmd.unknown_a6),
      bonus_hp_from_materials(0),
      bonus_tp_from_materials(0),
      language(0),
      player_tag(0x00010000),
      guild_card_number(guild_card_number),
      unknown_a6(0),
      battle_team_number(0),
      telepipe(cmd.telepipe),
      unknown_a8(cmd.unknown_a8),
      unknown_a9_nte_112000(cmd.unknown_a9),
      area(cmd.area),
      flags2_value(cmd.flags2),
      flags2_is_v3(false),
      visual(cmd.visual),
      stats(cmd.stats),
      num_items(cmd.num_items),
      items(cmd.items),
      floor(cmd.area),
      xb_user_id(this->default_xb_user_id()),
      xb_unknown_a16(0) {
  this->name = this->visual.name.decode(this->language);
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_DC112000_6x70& cmd,
    uint32_t guild_card_number,
    uint8_t language,
    Version from_version,
    bool from_client_customization)
    : from_version(from_version),
      from_client_customization(from_client_customization),
      item_version(from_version),
      base(cmd.base),
      unknown_a5_nte(0),
      unknown_a6_nte(0),
      bonus_hp_from_materials(cmd.bonus_hp_from_materials),
      bonus_tp_from_materials(cmd.bonus_tp_from_materials),
      unknown_a5_112000(cmd.unknown_a5),
      language(language),
      player_tag(0x00010000),
      guild_card_number(guild_card_number),
      unknown_a6(0),
      battle_team_number(0),
      telepipe(cmd.telepipe),
      unknown_a8(cmd.unknown_a8),
      unknown_a9_nte_112000(cmd.unknown_a9),
      area(cmd.area),
      flags2_value(cmd.flags2),
      flags2_is_v3(false),
      visual(cmd.visual),
      stats(cmd.stats),
      num_items(cmd.num_items),
      items(cmd.items),
      floor(cmd.area),
      xb_user_id(this->default_xb_user_id()),
      xb_unknown_a16(0) {
  this->name = this->visual.name.decode(this->language);
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_DC_PC_6x70& cmd,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : Parsed6x70Data(cmd.base, guild_card_number, from_version, from_client_customization) {
  this->stats = cmd.stats;
  this->num_items = cmd.num_items;
  this->items = cmd.items;
  this->floor = cmd.base.area;
  this->xb_user_id = this->default_xb_user_id();
  this->xb_unknown_a16 = 0;
  this->name = this->visual.name.decode(this->language);
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_GC_6x70& cmd,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : Parsed6x70Data(cmd.base, guild_card_number, from_version, from_client_customization) {
  this->flags2_is_v3 = true;
  this->stats = cmd.stats;
  this->num_items = cmd.num_items;
  this->items = cmd.items;
  this->floor = cmd.floor;
  this->xb_user_id = this->default_xb_user_id();
  this->xb_unknown_a16 = 0;
  this->name = this->visual.name.decode(this->language);
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_XB_6x70& cmd,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : Parsed6x70Data(cmd.base, guild_card_number, from_version, from_client_customization) {
  this->flags2_is_v3 = true;
  this->stats = cmd.stats;
  this->num_items = cmd.num_items;
  this->items = cmd.items;
  this->floor = cmd.floor;
  this->xb_user_id = (static_cast<uint64_t>(cmd.xb_user_id_high) << 32) | cmd.xb_user_id_low;
  this->xb_unknown_a16 = cmd.unknown_a16;
  this->name = this->visual.name.decode(this->language);
}

Parsed6x70Data::Parsed6x70Data(
    const G_SyncPlayerDispAndInventory_BB_6x70& cmd,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : Parsed6x70Data(cmd.base, guild_card_number, from_version, from_client_customization) {
  this->flags2_is_v3 = true;
  this->stats = cmd.stats;
  this->num_items = cmd.num_items;
  this->items = cmd.items;
  this->floor = cmd.floor;
  this->xb_user_id = this->default_xb_user_id();
  this->xb_unknown_a16 = cmd.unknown_a16;
  this->name = cmd.name.decode(cmd.base.language);
  this->visual.name.encode(this->name, cmd.base.language);
}

G_SyncPlayerDispAndInventory_DCNTE_6x70 Parsed6x70Data::as_dc_nte(shared_ptr<ServerState> s) const {
  G_SyncPlayerDispAndInventory_DCNTE_6x70 ret;
  ret.base = this->base;
  ret.unknown_a5 = this->unknown_a5_nte;
  ret.unknown_a6 = this->unknown_a6;
  ret.telepipe = this->telepipe;
  ret.unknown_a8 = this->unknown_a8;
  ret.unknown_a9 = this->unknown_a9_nte_112000;
  ret.area = this->area;
  ret.flags2 = this->get_flags2(false);
  ret.visual = this->visual;
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, Version::DC_NTE, s->item_parameter_table_for_encode(Version::DC_NTE));
  ret.visual.enforce_lobby_join_limits_for_version(Version::DC_NTE);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.visual.name_color = name_color;
    ret.visual.compute_name_color_checksum();
  }
  return ret;
}

G_SyncPlayerDispAndInventory_DC112000_6x70 Parsed6x70Data::as_dc_112000(shared_ptr<ServerState> s) const {
  G_SyncPlayerDispAndInventory_DC112000_6x70 ret;
  ret.base = this->base;
  ret.bonus_hp_from_materials = this->bonus_hp_from_materials;
  ret.bonus_tp_from_materials = this->bonus_tp_from_materials;
  ret.unknown_a5 = this->unknown_a5_112000;
  ret.telepipe = this->telepipe;
  ret.unknown_a8 = this->unknown_a8;
  ret.unknown_a9 = this->unknown_a9_nte_112000;
  ret.area = this->area;
  ret.flags2 = this->get_flags2(false);
  ret.visual = this->visual;
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, Version::DC_11_2000, s->item_parameter_table_for_encode(Version::DC_11_2000));
  ret.visual.enforce_lobby_join_limits_for_version(Version::DC_11_2000);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.visual.name_color = name_color;
    ret.visual.compute_name_color_checksum();
  }

  return ret;
}

G_SyncPlayerDispAndInventory_DC_PC_6x70 Parsed6x70Data::as_dc_pc(shared_ptr<ServerState> s, Version to_version) const {
  G_SyncPlayerDispAndInventory_DC_PC_6x70 ret;
  ret.base = this->base_v1(false);
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, to_version, s->item_parameter_table_for_encode(to_version));
  ret.base.visual.enforce_lobby_join_limits_for_version(to_version);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.base.visual.name_color = name_color;
    ret.base.visual.compute_name_color_checksum();
  }

  return ret;
}

G_SyncPlayerDispAndInventory_GC_6x70 Parsed6x70Data::as_gc_gcnte(shared_ptr<ServerState> s, Version to_version) const {
  G_SyncPlayerDispAndInventory_GC_6x70 ret;
  ret.base = this->base_v1(!is_v1_or_v2(to_version));
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;
  ret.floor = this->floor;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, to_version, s->item_parameter_table_for_encode(to_version));
  ret.base.visual.enforce_lobby_join_limits_for_version(to_version);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.base.visual.name_color = name_color;
    if (is_v1_or_v2(to_version)) {
      ret.base.visual.compute_name_color_checksum();
    } else {
      ret.base.visual.name_color_checksum = 0;
    }
  }

  return ret;
}

G_SyncPlayerDispAndInventory_XB_6x70 Parsed6x70Data::as_xb(shared_ptr<ServerState> s) const {
  G_SyncPlayerDispAndInventory_XB_6x70 ret;
  ret.base = this->base_v1(true);
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;
  ret.floor = this->floor;
  ret.xb_user_id_high = this->xb_user_id >> 32;
  ret.xb_user_id_low = this->xb_user_id;
  ret.unknown_a16 = this->xb_unknown_a16;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, Version::XB_V3, s->item_parameter_table_for_encode(Version::XB_V3));
  ret.base.visual.enforce_lobby_join_limits_for_version(Version::XB_V3);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.base.visual.name_color = name_color;
    ret.base.visual.name_color_checksum = 0;
  }

  return ret;
}

G_SyncPlayerDispAndInventory_BB_6x70 Parsed6x70Data::as_bb(shared_ptr<ServerState> s, uint8_t language) const {
  G_SyncPlayerDispAndInventory_BB_6x70 ret;
  ret.base = this->base_v1(true);
  ret.name.encode(this->name, language);
  ret.base.visual.name.encode(phosg::string_printf("%10" PRId32, this->guild_card_number), language);
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;
  ret.floor = this->floor;
  ret.xb_user_id_high = this->xb_user_id >> 32;
  ret.xb_user_id_low = this->xb_user_id;
  ret.unknown_a16 = this->xb_unknown_a16;

  transcode_inventory_items(
      ret.items, ret.num_items, this->item_version, Version::BB_V4, s->item_parameter_table_for_encode(Version::BB_V4));
  ret.base.visual.enforce_lobby_join_limits_for_version(Version::BB_V4);

  uint32_t name_color = s->name_color_for_client(this->from_version, this->from_client_customization);
  if (name_color) {
    ret.base.visual.name_color = name_color;
    ret.base.visual.name_color_checksum = 0;
  }

  return ret;
}

uint64_t Parsed6x70Data::default_xb_user_id() const {
  return (0xAE00000000000000 | this->guild_card_number);
}

void Parsed6x70Data::clear_v1_unused_item_fields() {
  for (size_t z = 0; z < min<uint32_t>(this->num_items, 30); z++) {
    auto& item = this->items[z];
    item.unknown_a1 = 0;
    item.extension_data1 = 0;
    item.extension_data2 = 0;
  }
}

void Parsed6x70Data::clear_dc_protos_unused_item_fields() {
  for (size_t z = 0; z < min<uint32_t>(this->num_items, 30); z++) {
    auto& item = this->items[z];
    item.unknown_a1 = 0;
    item.extension_data1 = 0;
    item.extension_data2 = 0;
    item.data.data2d = 0;
  }
}

Parsed6x70Data::Parsed6x70Data(
    const G_6x70_Base_V1& base,
    uint32_t guild_card_number,
    Version from_version,
    bool from_client_customization)
    : from_version(from_version),
      from_client_customization(from_client_customization),
      item_version(this->from_version),
      base(base.base),
      bonus_hp_from_materials(base.bonus_hp_from_materials),
      bonus_tp_from_materials(base.bonus_tp_from_materials),
      permanent_status_effect(base.permanent_status_effect),
      temporary_status_effect(base.temporary_status_effect),
      attack_status_effect(base.attack_status_effect),
      defense_status_effect(base.defense_status_effect),
      unused_status_effect(base.unused_status_effect),
      language(base.language),
      player_tag(base.player_tag),
      guild_card_number(guild_card_number), // Ignore the client's GC#
      unknown_a6(base.unknown_a6),
      battle_team_number(base.battle_team_number),
      telepipe(base.telepipe),
      unknown_a8(base.unknown_a8),
      unknown_a9_final(base.unknown_a9),
      area(base.area),
      flags2_value(base.flags2),
      flags2_is_v3(!is_v1_or_v2(from_version)),
      technique_levels_v1(base.technique_levels_v1),
      visual(base.visual) {}

G_6x70_Base_V1 Parsed6x70Data::base_v1(bool is_v3) const {
  G_6x70_Base_V1 ret;
  ret.base = this->base;
  ret.bonus_hp_from_materials = this->bonus_hp_from_materials;
  ret.bonus_tp_from_materials = this->bonus_tp_from_materials;
  ret.permanent_status_effect = this->permanent_status_effect;
  ret.temporary_status_effect = this->temporary_status_effect;
  ret.attack_status_effect = this->attack_status_effect;
  ret.defense_status_effect = this->defense_status_effect;
  ret.unused_status_effect = this->unused_status_effect;
  ret.language = this->language;
  ret.player_tag = this->player_tag;
  ret.guild_card_number = this->guild_card_number;
  ret.unknown_a6 = this->unknown_a6;
  ret.battle_team_number = this->battle_team_number;
  ret.telepipe = this->telepipe;
  ret.unknown_a8 = this->unknown_a8;
  ret.unknown_a9 = this->unknown_a9_final;
  ret.area = this->area;
  ret.flags2 = this->get_flags2(is_v3);
  ret.technique_levels_v1 = this->technique_levels_v1;
  ret.visual = this->visual;
  return ret;
}

uint32_t Parsed6x70Data::get_flags2(bool is_v3) const {
  // The format of flags2 was changed significantly between v2 and v3, and not
  // accounting for this means that sometimes other characters won't appear
  // when joining a game. Unfortunately, some bits were deleted on v3 and other
  // bits were added, so it doesn't suffice to simply store the most complete
  // format of this field - we have to be able to convert between the two.

  // Bits on v2: ---CBAzy xwvutsrq ponmlkji hgfedcba
  // Bits on v3: ---GFEDC BAzyxwvu srqponkj hgfedcba
  // The bits ilmt were removed in v3 and the bits to their left were shifted
  // right. The bits DEFG were added in v3 and do not exist on v2.

  if (is_v3 == this->flags2_is_v3) {
    return this->flags2_value;
  } else if (!this->flags2_is_v3) { // Convert v2 -> v3
    return (
        (this->flags2_value & 0x000000FF) |
        ((this->flags2_value & 0x00000600) >> 1) |
        ((this->flags2_value & 0x0007E000) >> 3) |
        ((this->flags2_value & 0x1FF00000) >> 4));
  } else { // Convert v3 -> v2
    return (
        (this->flags2_value & 0x000000FF) |
        ((this->flags2_value << 1) & 0x00000600) |
        ((this->flags2_value << 3) & 0x0007E000) |
        ((this->flags2_value << 4) & 0x1FF00000));
  }
}

static void on_sync_joining_player_disp_and_inventory(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto s = c->require_server_state();

  // In V1/V2 games, this command sometimes is sent after the new client has
  // finished loading, so we don't check l->any_client_loading() here.
  auto target = get_sync_target(c, command, flag, true);
  if (!target) {
    return;
  }

  // If the sender is the leader and is pre-V1, and the target is V1 or later,
  // we need to synthesize a 6x71 command to tell the target all state has been
  // sent. (If both are pre-V1, the target won't expect this command; if both
  // are V1 or later, the leader will send this command itself.)
  Version target_v = target->version();
  Version c_v = c->version();
  if (is_pre_v1(c_v) && !is_pre_v1(target_v)) {
    static const be_uint32_t data = 0x71010000;
    send_command(target, 0x62, target->lobby_client_id, &data, sizeof(data));
  }

  bool is_client_customisation = c->config.check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION);
  switch (c_v) {
    case Version::DC_NTE:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_DCNTE_6x70>(data, size),
          c->login->account->account_id, c_v, is_client_customisation));
      c->last_reported_6x70->clear_dc_protos_unused_item_fields();
      break;
    case Version::DC_11_2000:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_DC112000_6x70>(data, size),
          c->login->account->account_id, c->language(), c_v, is_client_customisation));
      c->last_reported_6x70->clear_dc_protos_unused_item_fields();
      break;
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_DC_PC_6x70>(data, size),
          c->login->account->account_id, c_v, is_client_customisation));
      if (c_v == Version::DC_V1) {
        c->last_reported_6x70->clear_v1_unused_item_fields();
      }
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_GC_6x70>(data, size),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    case Version::XB_V3:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_XB_6x70>(data, size),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    case Version::BB_V4:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          check_size_t<G_SyncPlayerDispAndInventory_BB_6x70>(data, size),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    default:
      throw logic_error("6x70 command from unknown game version");
  }

  send_game_player_state(target, c, false);
}

static void on_forward_check_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  if (cmd.client_id == c->lobby_client_id) {
    forward_subcommand(c, command, flag, data, size);
  }
}

template <typename CmdT>
static void on_forward_check_client_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.client_id == c->lobby_client_id) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_game(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_lobby(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_lobby_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_game_client(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_forward_check_ep3_lobby(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  check_size_t<G_UnusedHeader>(data, size, 0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && l->is_ep3()) {
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
  if (c->version() != Version::GC_EP3_NTE) {
    set_mask_for_ep3_game_command(data.data(), data.size(), 0);
  } else {
    auto& new_header = check_size_t<G_CardBattleCommandHeader>(data, 0xFFFF);
    new_header.mask_key = 0;
  }

  if (header.subsubcommand == 0x1A) {
    return;
  } else if (header.subsubcommand == 0x20) {
    const auto& cmd = check_size_t<G_Unknown_Ep3_6xB5x20>(data, size);
    if (cmd.client_id >= 12) {
      return;
    }
  } else if (header.subsubcommand == 0x31) {
    const auto& cmd = check_size_t<G_ConfirmDeckSelection_Ep3_6xB5x31>(data, size);
    if (cmd.menu_type >= 0x15) {
      return;
    }
  } else if (header.subsubcommand == 0x32) {
    const auto& cmd = check_size_t<G_MoveSharedMenuCursor_Ep3_6xB5x32>(data, size);
    if (cmd.menu_type >= 0x15) {
      return;
    }
  } else if (header.subsubcommand == 0x36) {
    const auto& cmd = check_size_t<G_RecreatePlayer_Ep3_6xB5x36>(data, size);
    if (l->is_game() && (cmd.client_id >= 4)) {
      return;
    }
  } else if (header.subsubcommand == 0x38) {
    c->config.set_flag(Client::Flag::EP3_ALLOW_6xBC);
  } else if (header.subsubcommand == 0x3C) {
    c->config.clear_flag(Client::Flag::EP3_ALLOW_6xBC);
  }

  if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING) && (c->version() != Version::GC_EP3_NTE)) {
    set_mask_for_ep3_game_command(data.data(), data.size(), (phosg::random_object<uint32_t>() % 0xFF) + 1);
  }

  forward_subcommand(c, command, flag, data.data(), data.size());
}

static void on_ep3_trade_card_counts(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  if (c->version() == Version::GC_EP3_NTE) {
    check_size_t<G_CardCounts_Ep3NTE_6xBC>(data, size, 0xFFFF);
  } else {
    check_size_t<G_CardCounts_Ep3_6xBC>(data, size, 0xFFFF);
  }

  if (!command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    return;
  }
  auto target = l->clients.at(flag);
  if (!target || !target->config.check_flag(Client::Flag::EP3_ALLOW_6xBC)) {
    return;
  }

  forward_subcommand(c, command, flag, data, size);
}

////////////////////////////////////////////////////////////////////////////////
// Chat commands and the like

static void on_send_guild_card(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!command_is_private(command) || (flag >= l->max_clients) || (!l->clients[flag])) {
    return;
  }

  switch (c->version()) {
    case Version::DC_NTE: {
      const auto& cmd = check_size_t<G_SendGuildCard_DCNTE_6x06>(data, size);
      c->character(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::DC_11_2000:
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
    case Version::GC_EP3_NTE:
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

static void on_symbol_chat(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_SymbolChat_6x07>(data, size);
  if (c->can_chat && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
  }
}

template <bool SenderBE>
static void on_word_select_t(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_WordSelectT_6x74<SenderBE>>(data, size);
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
        } else if (lc->version() == Version::DC_11_2000) {
          subcommand = 0x69;
        } else {
          subcommand = 0x74;
        }

        if (is_big_endian(lc->version())) {
          G_WordSelectBE_6x74 out_cmd = {
              subcommand, cmd.size, cmd.client_id.load(),
              s->word_select_table->translate(cmd.message, from_version, lc_version)};
          send_command_t(lc, 0x60, 0x00, out_cmd);
        } else {
          G_WordSelect_6x74 out_cmd = {
              subcommand, cmd.size, cmd.client_id.load(),
              s->word_select_table->translate(cmd.message, from_version, lc_version)};
          send_command_t(lc, 0x60, 0x00, out_cmd);
        }

      } catch (const exception& e) {
        string name = escape_player_name(c->character()->disp.name.decode(c->language()));
        lc->log.warning("Untranslatable Word Select message: %s", e.what());
        send_text_message_printf(lc, "$C4Untranslatable Word\nSelect message from\n%s", name.c_str());
      }
    }
  }
}

static void on_word_select(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
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

static void on_warp(shared_ptr<Client>, uint8_t, uint8_t, void* data, size_t size) {
  check_size_t<G_InterLevelWarp_6x94>(data, size);
  // Unconditionally block these. Players should use $warp instead.
}

static void on_set_player_visible(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
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
          send_update_team_reward_flags(c);
          send_all_nearby_team_metadatas_to_client(c, false);
        }
      } else if (c->config.check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST) && (c->version() != Version::BB_V4)) {
        c->config.clear_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
        c->log.info("LOADING_RUNNING_JOINABLE_QUEST flag cleared");
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

static void on_change_floor_6x1F(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  if (is_pre_v1(c->version())) {
    check_size_t<G_SetPlayerFloor_DCNTE_6x1F>(data, size);
    // DC NTE and 11/2000 don't send 6F when they're done loading, so we clear
    // the loading flag here instead.
    if (c->config.check_flag(Client::Flag::LOADING)) {
      c->config.clear_flag(Client::Flag::LOADING);
      c->log.info("LOADING flag cleared");
      send_resume_game(c->require_lobby(), c);
      c->require_lobby()->assign_inventory_and_bank_item_ids(c, true);
    }

  } else {
    const auto& cmd = check_size_t<G_SetPlayerFloor_6x1F>(data, size);
    if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
      c->floor = cmd.floor;
    }
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_change_floor_6x21(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_InterLevelWarp_6x21>(data, size);
  if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_player_died(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_PlayerDied_6x4D>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  // Decrease MAG's synchro
  try {
    auto& inventory = c->character()->inventory;
    size_t mag_index = inventory.find_equipped_item(EquipSlot::MAG);
    auto& data = inventory.items[mag_index].data;
    data.data2[0] = max<int8_t>(static_cast<int8_t>(data.data2[0] - 5), 0);
  } catch (const out_of_range&) {
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_player_revivable(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_PlayerRevivable_6x4E>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  forward_subcommand(c, command, flag, data, size);

  // Revive if infinite HP is enabled
  bool player_cheats_enabled = l->check_flag(Lobby::Flag::CHEATS_ENABLED) ||
      (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));
  if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
    G_UseMedicalCenter_6x31 v2_cmd = {0x31, 0x01, c->lobby_client_id};
    G_RevivePlayer_V3_BB_6xA1 v3_cmd = {0xA1, 0x01, c->lobby_client_id};
    static_assert(sizeof(v2_cmd) == sizeof(v3_cmd), "Command sizes do not match");

    const void* c_data = (!is_v1_or_v2(c->version()) || (c->version() == Version::GC_NTE))
        ? static_cast<const void*>(&v3_cmd)
        : static_cast<const void*>(&v2_cmd);
    // TODO: We might need to send different versions of the command here to
    // different clients in certain crossplay scenarios, so just using
    // echo_to_lobby would not suffice. Figure out a way to handle this.
    send_protected_command(c, c_data, sizeof(v3_cmd), true);
  }
}

void on_player_revived(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  check_size_t<G_PlayerRevived_6x4F>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)));
    if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
      send_player_stats_change(c, PlayerStatsChange::ADD_HP, 2550);
    }
  }
}

static void on_received_condition(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ClientIDHeader>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
    if (cmd.client_id == c->lobby_client_id) {
      bool player_cheats_enabled = l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));
      if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        send_remove_negative_conditions(c);
      }
    }
  }
}

template <typename CmdT>
static void on_change_hp(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size, 0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)));
    if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
      send_player_stats_change(c, PlayerStatsChange::ADD_HP, 2550);
    }
  }
}

static void on_cast_technique_finished(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_CastTechniqueComplete_6x48>(data, size);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.header.client_id == c->lobby_client_id)) {
    forward_subcommand(c, command, flag, data, size);
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)));
    if (player_cheats_enabled && c->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
      send_player_stats_change(c, PlayerStatsChange::ADD_TP, 255);
    }
  }
}

static void on_attack_finished(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_AttackFinished_6x46>(data, size,
      offsetof(G_AttackFinished_6x46, targets), sizeof(G_AttackFinished_6x46));
  if (cmd.target_count > min<size_t>(cmd.header.size - 2, cmd.targets.size())) {
    throw runtime_error("invalid attack finished command");
  }
  forward_subcommand_with_entity_targets_transcode_t<G_AttackFinished_6x46>(c, command, flag, data, size);
}

static void on_cast_technique(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_CastTechnique_6x47>(data, size,
      offsetof(G_CastTechnique_6x47, targets), sizeof(G_CastTechnique_6x47));
  if (cmd.target_count > min<size_t>(cmd.header.size - 2, cmd.targets.size())) {
    throw runtime_error("invalid cast technique command");
  }
  forward_subcommand_with_entity_targets_transcode_t<G_CastTechnique_6x47>(c, command, flag, data, size);
}

static void on_execute_photon_blast(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ExecutePhotonBlast_6x49>(data, size,
      offsetof(G_ExecutePhotonBlast_6x49, targets), sizeof(G_ExecutePhotonBlast_6x49));
  if (cmd.target_count > min<size_t>(cmd.header.size - 3, cmd.targets.size())) {
    throw runtime_error("invalid subtract PB energy command");
  }
  forward_subcommand_with_entity_targets_transcode_t<G_ExecutePhotonBlast_6x49>(c, command, flag, data, size);
}

static void on_npc_control(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_NPCControl_6x69>(data, size);
  // Don't allow NPC control commands if there is a player in the relevant slot
  const auto& l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("cannot create or modify NPC in the lobby");
  }

  if ((cmd.command == 0 || cmd.command == 3) && ((cmd.param2 < 4) && l->clients[cmd.param2])) {
    throw runtime_error("cannot create NPC in existing player slot");
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_switch_state_changed(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  if (!l->quest &&
      (cmd.flags & 1) &&
      (cmd.header.entity_id != 0xFFFF) &&
      (cmd.switch_flag_num < 0x100) &&
      c->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
    auto sw_obj_st = l->map_state->object_state_for_index(c->version(), cmd.switch_flag_floor, cmd.header.entity_id - 0x4000);
    c->log.info("Switch assist triggered by K-%03zX setting SW-%02hhX-%02hX",
        sw_obj_st->k_id, cmd.switch_flag_floor, cmd.switch_flag_num.load());
    for (auto obj_st : l->map_state->door_states_for_switch_flag(c->version(), cmd.switch_flag_floor, cmd.switch_flag_num)) {
      if (obj_st->game_flags & 0x0001) {
        c->log.info("K-%03zX is already unlocked", obj_st->k_id);
        continue;
      }
      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(c, "$C5SWA K-%03zX %02hhX %02hX",
            obj_st->k_id, cmd.switch_flag_floor, cmd.switch_flag_num.load());
      }
      obj_st->game_flags |= 1;

      for (auto lc : l->clients) {
        if (!lc) {
          continue;
        }
        uint16_t object_index = l->map_state->index_for_object_state(lc->version(), obj_st);
        lc->log.info("Switch assist: door object K-%03zX has index %04hX on version %s",
            obj_st->k_id, object_index, phosg::name_for_enum(lc->version()));
        if (object_index != 0xFFFF) {
          G_UpdateObjectState_6x0B cmd0B;
          cmd0B.header.subcommand = 0x0B;
          cmd0B.header.size = sizeof(cmd0B) / 4;
          cmd0B.header.entity_id = object_index | 0x4000;
          cmd0B.flags = obj_st->game_flags;
          cmd0B.object_index = object_index;
          send_command_t(l, 0x60, 0x00, cmd0B);
        }
      }
    }
  }

  if (cmd.header.entity_id != 0xFFFF && c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    const auto& obj_st = l->map_state->object_state_for_index(
        c->version(), cmd.switch_flag_floor, cmd.header.entity_id - 0x4000);
    send_text_message_printf(c, "$C5K-%03zX A %s", obj_st->k_id, obj_st->type_name(c->version()));
  }

  if (l->switch_flags) {
    if (cmd.flags & 1) {
      l->switch_flags->set(cmd.switch_flag_floor, cmd.switch_flag_num);
      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(c, "$C5SW-%02hhX-%02hX ON", cmd.switch_flag_floor, cmd.switch_flag_num.load());
      }
    } else {
      l->switch_flags->clear(cmd.switch_flag_floor, cmd.switch_flag_num);
      if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(c, "$C5SW-%02hhX-%02hX OFF", cmd.switch_flag_floor, cmd.switch_flag_num.load());
      }
    }
  }

  forward_subcommand_with_entity_id_transcode_t<G_SwitchStateChanged_6x05, true>(c, command, flag, data, size);
}

static void on_play_sound_from_player(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_PlaySoundFromPlayer_6xB2>(data, size);
  // This command can be used to play arbitrary sounds, but the client only
  // ever sends it for the camera shutter sound, so we only allow that one.
  if (cmd.floor == c->floor && cmd.sound_id == 0x00051720) {
    forward_subcommand(c, command, flag, data, size);
  }
}

////////////////////////////////////////////////////////////////////////////////

template <typename CmdT>
void on_movement(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }
  c->pos = cmd.pos;
  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
void on_movement_with_floor(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }
  c->pos = cmd.pos;
  if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, command, flag, data, size);
}

void on_set_animation_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto& cmd = check_size_t<G_SetAnimationState_6x52>(data, size);
  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }
  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, command, flag, data, size);
    return;
  }

  // The animation numbers were changed on V3. This is the most common one to
  // see in the lobby (it occurs when a player talks to the counter), so we
  // take care to translate it specifically.
  bool c_is_v1_or_v2 = is_v1_or_v2(c->version());
  if (!((c_is_v1_or_v2 && (cmd.animation == 0x000A)) || (!c_is_v1_or_v2 && (cmd.animation == 0x0000)))) {
    forward_subcommand(c, command, flag, data, size);
    return;
  }

  G_SetAnimationState_6x52 other_cmd = cmd;
  other_cmd.animation = 0x000A - cmd.animation;
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      auto& out_cmd = (is_v1_or_v2(lc->version()) != c_is_v1_or_v2) ? other_cmd : cmd;
      out_cmd.header.subcommand = translate_subcommand_number(lc->version(), Version::BB_V4, 0x52);
      send_command_t(lc, command, flag, out_cmd);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Item commands

static void on_player_drop_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_DropItem_6x2A>(data, size);

  if ((cmd.header.client_id != c->lobby_client_id)) {
    return;
  }

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, 0, *s->item_stack_limits(c->version()));
  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu dropped item %08" PRIX32 " (%s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.floor.load(), cmd.pos.x.load(), cmd.pos.z.load());
    c->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

template <typename CmdT>
static void on_create_inventory_item_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);

  // Players cannot send this on behalf of another player, but they can send it
  // on behalf of an NPC; we don't track items for NPCs so in that case we just
  // mark the item ID as used and ignore it. This works for the most part,
  // because when NPCs use or equip items, we ignore the command since it has
  // the wrong client ID.
  // TODO: This won't work if NPCs ever drop items that players can interact
  // with. Presumably we would have to track all NPCs' inventory items to handle
  // this.
  auto s = c->require_server_state();
  if (cmd.header.client_id != c->lobby_client_id) {
    // Don't allow creating items in other players' inventories, only in NPCs'
    if (l->clients.at(cmd.header.client_id)) {
      return;
    }

    if (l->log.should_log(phosg::LogLevel::INFO)) {
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hu created inventory item %08" PRIX32 " (%s) in inventory of NPC %02hX; ignoring", c->lobby_client_id, item.id.load(), name.c_str(), cmd.header.client_id.load());
    }

  } else {
    c->character()->add_item(item, *s->item_stack_limits(c->version()));

    if (l->log.should_log(phosg::LogLevel::INFO)) {
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hu created inventory item %08" PRIX32 " (%s)", c->lobby_client_id, item.id.load(), name.c_str());
      c->print_inventory(stderr);
    }
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
}

static void on_create_inventory_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  if (size == sizeof(G_CreateInventoryItem_PC_V3_BB_6x2B)) {
    on_create_inventory_item_t<G_CreateInventoryItem_PC_V3_BB_6x2B>(c, command, flag, data, size);
  } else if (size == sizeof(G_CreateInventoryItem_DC_6x2B)) {
    on_create_inventory_item_t<G_CreateInventoryItem_DC_6x2B>(c, command, flag, data, size);
  } else {
    throw runtime_error("invalid size for 6x2B command");
  }
}

template <typename CmdT>
static void on_drop_partial_stack_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<CmdT>(data, size);

  auto l = c->require_lobby();
  if (c->version() == Version::BB_V4) {
    throw runtime_error("6x5D command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6x5D command sent in non-game lobby");
  }
  // TODO: Should we check the client ID here too?

  // We don't delete anything from the inventory here; the client will send a
  // 6x29 to do so immediately following this command.

  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu split stack to create floor item %08" PRIX32 " (%s) at %hu:(%g,%g)",
        cmd.header.client_id.load(), item.id.load(), name.c_str(), cmd.floor.load(), cmd.pos.x.load(), cmd.pos.z.load());
    c->print_inventory(stderr);
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
}

static void on_drop_partial_stack(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  if (size == sizeof(G_DropStackedItem_PC_V3_BB_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_PC_V3_BB_6x5D>(c, command, flag, data, size);
  } else if (size == sizeof(G_DropStackedItem_DC_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_DC_6x5D>(c, command, flag, data, size);
  } else {
    throw runtime_error("invalid size for 6x5D command");
  }
}

static void on_drop_partial_stack_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_SplitStackedItem_BB_6xC3>(data, size);
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC3 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC3 command sent in non-game lobby");
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    throw runtime_error("6xC3 command sent by incorrect client");
  }

  auto s = c->require_server_state();
  auto p = c->character();
  const auto& limits = *s->item_stack_limits(c->version());
  auto item = p->remove_item(cmd.item_id, cmd.amount, limits);

  // If a stack was split, the original item still exists, so the dropped item
  // needs a new ID. remove_item signals this by returning an item with an ID
  // of 0xFFFFFFFF.
  if (item.id == 0xFFFFFFFF) {
    item.id = l->generate_item_id(c->lobby_client_id);
  }

  // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add
  // the item back to the player's inventory to correct for this (it will get
  // removed again by the 6x29 handler)
  p->add_item(item, limits);

  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);
  send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.pos);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu split stack %08" PRIX32 " (removed: %s) at %hu:(%g, %g)",
        cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.floor.load(), cmd.pos.x.load(), cmd.pos.z.load());
    c->print_inventory(stderr);
  }
}

static void on_buy_shop_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_BuyShopItem_6x5E>(data, size);
  auto l = c->require_lobby();
  if (c->version() == Version::BB_V4) {
    throw runtime_error("6x5E command sent by BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6x5E command sent in non-game lobby");
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    throw runtime_error("6x5E command sent by incorrect client");
  }

  auto s = c->require_server_state();
  auto p = c->character();
  ItemData item = cmd.item_data;
  item.data2d = 0; // Clear the price field
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  p->add_item(item, *s->item_stack_limits(c->version()));

  size_t price = s->item_parameter_table(c->version())->price_for_item(item);
  p->remove_meseta(price, c->version() != Version::BB_V4);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hu bought item %08" PRIX32 " (%s) from shop (%zu Meseta)",
        cmd.header.client_id.load(), item.id.load(), name.c_str(), price);
    c->print_inventory(stderr);
  }

  forward_subcommand_with_item_transcode_t(c, command, flag, cmd);
}

void send_item_notification_if_needed(
    shared_ptr<ServerState> s,
    Channel& ch,
    const Client::Config& config,
    const ItemData& item,
    bool is_from_rare_table) {
  bool should_notify = false;
  bool should_include_rare_header = false;
  switch (config.get_drop_notification_mode()) {
    case Client::ItemDropNotificationMode::NOTHING:
      break;
    case Client::ItemDropNotificationMode::RARES_ONLY:
      should_notify = (is_from_rare_table || (item.data1[0] == 0x03)) && s->item_parameter_table(ch.version)->is_item_rare(item);
      should_include_rare_header = true;
      break;
    case Client::ItemDropNotificationMode::ALL_ITEMS:
      should_notify = (item.data1[0] != 0x04);
      break;
    case Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA:
      should_notify = true;
      break;
  }

  if (should_notify) {
    string name = s->describe_item(ch.version, item, true);
    const char* rare_header = (should_include_rare_header ? "$C6Rare item dropped:\n" : "");
    send_text_message_printf(ch, "%s%s", rare_header, name.c_str());
  }
}

template <typename CmdT>
static void on_box_or_enemy_item_drop_t(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
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
  if (c->version() == Version::BB_V4) {
    throw runtime_error("BB client sent 6x5F command");
  }

  bool should_notify = s->rare_notifs_enabled_for_client_drops && (l->drop_mode == Lobby::DropMode::CLIENT);

  shared_ptr<const MapState::EnemyState> ene_st;
  shared_ptr<const MapState::ObjectState> obj_st;
  string from_entity_str;
  if (cmd.item.source_type == 1) {
    ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.item.floor, cmd.item.entity_index);
    from_entity_str = phosg::string_printf(" from E-%03zX", ene_st->e_id);
  } else {
    obj_st = l->map_state->object_state_for_index(c->version(), cmd.item.floor, cmd.item.entity_index);
    from_entity_str = phosg::string_printf(" from K-%03zX", obj_st->k_id);
  }

  ItemData item = cmd.item.item;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.item.floor, item, cmd.item.pos, obj_st, ene_st, should_notify ? 0x100F : 0x000F);

  auto name = s->describe_item(c->version(), item, false);
  l->log.info("Player %hhu (leader) created floor item %08" PRIX32 " (%s)%s at %hhu:(%g, %g)",
      l->leader_id,
      item.id.load(),
      name.c_str(),
      from_entity_str.c_str(),
      cmd.item.floor,
      cmd.item.pos.x.load(),
      cmd.item.pos.z.load());

  for (auto& lc : l->clients) {
    if (!lc) {
      continue;
    }
    if (lc != c) {
      uint16_t entity_index = 0xFFFF;
      if (ene_st) {
        entity_index = l->map_state->index_for_enemy_state(lc->version(), ene_st);
      } else if (obj_st) {
        entity_index = l->map_state->index_for_object_state(lc->version(), obj_st);
      }
      send_drop_item_to_channel(s, lc->channel, item, cmd.item.source_type, cmd.item.floor, cmd.item.pos, entity_index);
    }
    send_item_notification_if_needed(s, lc->channel, lc->config, item, true);
  }
}

static void on_box_or_enemy_item_drop(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
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
    auto s = c->require_server_state();
    auto fi = l->remove_item(floor, item_id, c->lobby_client_id);
    if (!fi->visible_to_client(c->lobby_client_id)) {
      l->log.warning("Player %hu requests to pick up %08" PRIX32 ", but is it not visible to them; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      return;
    }

    try {
      p->add_item(fi->data, *s->item_stack_limits(c->version()));
    } catch (const out_of_range&) {
      // Inventory is full; put the item back where it was
      l->log.warning("Player %hu requests to pick up %08" PRIX32 ", but their inventory is full; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      return;
    }

    if (l->log.should_log(phosg::LogLevel::INFO)) {
      auto s = c->require_server_state();
      auto name = s->describe_item(c->version(), fi->data, false);
      l->log.info("Player %hu picked up %08" PRIX32 " (%s)", client_id, item_id, name.c_str());
      c->print_inventory(stderr);
    }

    for (size_t z = 0; z < 12; z++) {
      auto lc = l->clients[z];
      if ((!lc) || (!is_request && (lc == c))) {
        continue;
      }
      if (fi->visible_to_client(z)) {
        send_pick_up_item_to_client(lc, client_id, item_id, floor);
      } else {
        send_create_inventory_item_to_client(lc, client_id, fi->data);
      }
    }

    if (!c->login->account->check_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST) &&
        (fi->flags & 0x1000)) {
      uint32_t pi = fi->data.primary_identifier();
      bool should_send_game_notif, should_send_global_notif;
      if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
        should_send_game_notif = s->notify_game_for_item_primary_identifiers_v1_v2.count(pi);
        should_send_global_notif = s->notify_server_for_item_primary_identifiers_v1_v2.count(pi);
      } else if (!is_v4(c->version())) {
        should_send_game_notif = s->notify_game_for_item_primary_identifiers_v3.count(pi);
        should_send_global_notif = s->notify_server_for_item_primary_identifiers_v3.count(pi);
      } else {
        should_send_game_notif = s->notify_game_for_item_primary_identifiers_v4.count(pi);
        should_send_global_notif = s->notify_server_for_item_primary_identifiers_v4.count(pi);
      }

      if (should_send_game_notif || should_send_global_notif) {
        string p_name = p->disp.name.decode();
        string desc_ingame = s->describe_item(c->version(), fi->data, true);
        string desc_http = s->describe_item(c->version(), fi->data, false);

        if (s->http_server) {
          auto message = make_shared<phosg::JSON>(phosg::JSON::dict({
              {"PlayerAccountID", c->login->account->account_id},
              {"PlayerName", p_name},
              {"PlayerVersion", phosg::name_for_enum(c->version())},
              {"GameName", l->name},
              {"GameDropMode", phosg::name_for_enum(l->drop_mode)},
              {"ItemData", fi->data.hex()},
              {"ItemDescription", desc_http},
              {"NotifyGame", should_send_game_notif},
              {"NotifyServer", should_send_global_notif},
          }));
          s->http_server->send_rare_drop_notification(message);
        }

        string message = phosg::string_printf("$C6%s$C7 found\n%s", p_name.c_str(), desc_ingame.c_str());
        string bb_message = phosg::string_printf("$C6%s$C7 has found %s", p_name.c_str(), desc_ingame.c_str());
        if (should_send_global_notif) {
          for (auto& it : s->channel_to_client) {
            if (it.second->login &&
                !is_patch(it.second->version()) &&
                !is_ep3(it.second->version()) &&
                it.second->lobby.lock()) {
              send_text_or_scrolling_message(it.second, message, bb_message);
            }
          }
        } else {
          send_text_or_scrolling_message(l, nullptr, message, bb_message);
        }
      }
    }
  }
}

static void on_pick_up_item(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<G_PickUpItem_6x59>(data, size);
  on_pick_up_item_generic(c, cmd.client_id2, cmd.floor, cmd.item_id, false);
}

static void on_pick_up_item_request(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<G_PickUpItemRequest_6x5A>(data, size);
  on_pick_up_item_generic(c, cmd.header.client_id, cmd.floor, cmd.item_id, true);
}

static void on_equip_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_EquipItem_6x25>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto l = c->require_lobby();
  EquipSlot slot = static_cast<EquipSlot>(cmd.equip_slot.load());
  auto p = c->character();
  p->inventory.equip_item_id(cmd.item_id, slot, is_pre_v1(c->version()));
  c->log.info("Equipped item %08" PRIX32, cmd.item_id.load());

  forward_subcommand(c, command, flag, data, size);
}

static void on_unequip_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
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
    void* data, size_t size) {
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
  player_use_item(c, index, l->opt_rand_crypt);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    l->log.info("Player %hhu used item %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    c->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_feed_mag(
    shared_ptr<Client> c,
    uint8_t command,
    uint8_t flag,
    void* data, size_t size) {
  const auto& cmd = check_size_t<G_FeedMAG_6x28>(data, size);

  if (cmd.header.client_id != c->lobby_client_id) {
    return;
  }

  auto s = c->require_server_state();
  auto l = c->require_lobby();
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
  if (c->version() == Version::BB_V4) {
    p->remove_item(cmd.fed_item_id, 1, *s->item_stack_limits(c->version()));
  }

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    l->log.info("Player %hhu fed item %hu:%08" PRIX32 " (%s) to mag %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.fed_item_id.load(), fed_name.c_str(),
        cmd.header.client_id.load(), cmd.mag_item_id.load(), mag_name.c_str());
    c->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_xbox_voice_chat_control(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  // If sent by an XB client, should be forwarded to XB clients and no one else
  if (c->version() != Version::XB_V3) {
    return;
  }

  auto l = c->require_lobby();
  if (command_is_private(command)) {
    if (flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[flag];
    if (target && (target->version() == Version::XB_V3)) {
      send_command(target, command, flag, data, size);
    }
  } else {
    for (auto& lc : l->clients) {
      if (lc && (lc != c) && (lc->version() == Version::XB_V3)) {
        send_command(lc, command, flag, data, size);
      }
    }
  }
}

static void on_gc_nte_exclusive(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto can_participate = [&](Version vers) {
    return (!is_v1_or_v2(vers) || (vers == Version::GC_NTE));
  };
  if (!can_participate(c->version())) {
    return;
  }

  // Command should not be forwarded across the GC NTE boundary, but may be
  // forwarded to other clients within that boundary
  bool c_is_nte = (c->version() == Version::GC_NTE);

  auto l = c->require_lobby();
  if (command_is_private(command)) {
    if (flag >= l->max_clients) {
      return;
    }
    auto lc = l->clients[flag];
    if (lc && can_participate(lc->version()) && ((lc->version() == Version::GC_NTE) == c_is_nte)) {
      send_command(lc, command, flag, data, size);
    }
  } else {
    for (auto& lc : l->clients) {
      if (lc && (lc != c) && can_participate(lc->version()) && ((lc->version() == Version::GC_NTE) == c_is_nte)) {
        send_command(lc, command, flag, data, size);
      }
    }
  }
}

static void on_open_shop_bb_or_ep3_battle_subs(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xB5 command sent in non-game lobby");
  }

  if (is_ep3(c->version())) {
    on_ep3_battle_subs(c, command, flag, data, size);
  } else if (l->episode == Episode::EP3) { // There's no item_creator in an Ep3 game
    throw runtime_error("received BB shop subcommand in Ep3 game");
  } else if (c->version() != Version::BB_V4) {
    throw runtime_error("received BB shop subcommand from non-BB client");
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
      item.data2d = s->item_parameter_table(c->version())->price_for_item(item);
    }

    send_shop(c, cmd.shop_type);
  }
}

bool validate_6xBB(G_SyncCardTradeServerState_Ep3_6xBB& cmd) {
  if ((cmd.header.client_id >= 4) || (cmd.slot > 1)) {
    return false;
  }

  // TTradeCardServer uses 4 to indicate the slot is empty, so we allow 4 in
  // the client ID checks below
  switch (cmd.what) {
    case 1:
      if (cmd.args[0] >= 5) {
        return false;
      }
      cmd.args[1] = 0;
      cmd.args[2] = 0;
      cmd.args[3] = 0;
      break;
    case 0:
    case 2:
    case 4:
      cmd.args.clear(0);
      break;
    case 3:
      if (cmd.args[0] >= 5 || cmd.args[1] >= 5) {
        return false;
      }
      cmd.args[2] = 0;
      cmd.args[3] = 0;
      break;
    default:
      return false;
  }
  return true;
}

static void on_open_bank_bb_or_card_trade_counter_ep3(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xBB command sent in non-game lobby");
  }

  if (c->version() == Version::BB_V4) {
    c->config.set_flag(Client::Flag::AT_BANK_COUNTER);
    send_bank(c);
  } else if (l->is_ep3() && validate_6xBB(check_size_t<G_SyncCardTradeServerState_Ep3_6xBB>(data, size))) {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_ep3_private_word_select_bb_bank_action(
    shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xBD command sent in non-game lobby");
  }

  auto s = c->require_server_state();
  if (is_ep3(c->version())) {
    const auto& cmd = check_size_t<G_PrivateWordSelect_Ep3_6xBD>(data, size);
    s->word_select_table->validate(cmd.message, c->version());

    string from_name = c->character()->disp.name.decode(c->language());
    static const string whisper_text = "(whisper)";
    auto send_to_client = [&](shared_ptr<Client> lc) -> void {
      if (cmd.private_flags & (1 << lc->lobby_client_id)) {
        try {
          send_chat_message(lc, c->login->account->account_id, from_name, whisper_text, cmd.private_flags);
        } catch (const runtime_error& e) {
          lc->log.warning("Failed to encode chat message: %s", e.what());
        }
      } else {
        send_command_t(lc, command, flag, cmd);
      }
    };

    if (command_is_private(command)) {
      if (flag >= l->max_clients) {
        return;
      }
      auto target = l->clients[flag];
      if (target) {
        send_to_client(target);
      }
    } else {
      for (auto& lc : l->clients) {
        if (lc && (lc != c) && is_ep3(lc->version())) {
          send_to_client(lc);
        }
      }
    }

    for (const auto& watcher_lobby : l->watcher_lobbies) {
      for (auto& target : watcher_lobby->clients) {
        if (target && is_ep3(target->version())) {
          send_command(target, command, flag, data, size);
        }
      }
    }

    if (l->battle_record && l->battle_record->battle_in_progress()) {
      auto type = ((command & 0xF0) == 0xC0)
          ? Episode3::BattleRecord::Event::Type::EP3_GAME_COMMAND
          : Episode3::BattleRecord::Event::Type::GAME_COMMAND;
      l->battle_record->add_command(type, data, size);
    }

  } else if (c->version() == Version::BB_V4) {
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
        const auto& limits = *s->item_stack_limits(c->version());
        auto item = p->remove_item(cmd.item_id, cmd.item_amount, limits);
        // If a stack was split, the bank item retains the same item ID as the
        // inventory item. This is annoying but doesn't cause any problems
        // because we always generate a new item ID when withdrawing from the
        // bank, so there's no chance of conflict later.
        if (item.id == 0xFFFFFFFF) {
          item.id = cmd.item_id;
        }
        bank.add_item(item, limits);
        send_destroy_item_to_lobby(c, cmd.item_id, cmd.item_amount, true);

        if (l->log.should_log(phosg::LogLevel::INFO)) {
          string name = s->describe_item(Version::BB_V4, item, false);
          l->log.info("Player %hu deposited item %08" PRIX32 " (x%hhu) (%s) in the bank",
              c->lobby_client_id, cmd.item_id.load(), cmd.item_amount, name.c_str());
          c->print_inventory(stderr);
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
        const auto& limits = *s->item_stack_limits(c->version());
        auto item = bank.remove_item(cmd.item_id, cmd.item_amount, limits);
        item.id = l->generate_item_id(c->lobby_client_id);
        p->add_item(item, limits);
        send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

        if (l->log.should_log(phosg::LogLevel::INFO)) {
          string name = s->describe_item(Version::BB_V4, item, false);
          l->log.info("Player %hu withdrew item %08" PRIX32 " (x%hhu) (%s) from the bank",
              c->lobby_client_id, item.id.load(), cmd.item_amount, name.c_str());
          c->print_inventory(stderr);
        }
      }

    } else if (cmd.action == 3) { // Leave bank counter
      c->config.clear_flag(Client::Flag::AT_BANK_COUNTER);
    }
  }
}

static void on_sort_inventory_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC4 command sent by non-BB client");
  }

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

////////////////////////////////////////////////////////////////////////////////
// EXP/Drop Item commands

G_SpecializableItemDropRequest_6xA2 normalize_drop_request(const void* data, size_t size) {
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
    cmd.entity_index = in_cmd.entity_index;
    cmd.floor = in_cmd.floor;
    cmd.rt_index = in_cmd.rt_index;
    cmd.pos = in_cmd.pos;
    cmd.ignore_def = in_cmd.ignore_def;
    cmd.effective_area = in_cmd.effective_area;
  } else {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_DC_6x60>(data, size);
    if (in_cmd.header.subcommand != 0x60) {
      throw runtime_error("item drop request has incorrect subcommand");
    }
    cmd.entity_index = in_cmd.entity_index;
    cmd.floor = in_cmd.floor;
    cmd.rt_index = in_cmd.rt_index;
    cmd.pos = in_cmd.pos;
    cmd.ignore_def = in_cmd.ignore_def;
    cmd.effective_area = in_cmd.floor;
  }
  return cmd;
}

DropReconcileResult reconcile_drop_request_with_map(
    phosg::PrefixedLogger& log,
    Channel& client_channel,
    G_SpecializableItemDropRequest_6xA2& cmd,
    Episode episode,
    uint8_t event,
    const Client::Config& config,
    shared_ptr<MapState> map,
    bool mark_drop) {
  Version version = client_channel.version;

  bool is_box = (cmd.rt_index == 0x30);

  DropReconcileResult res;
  res.effective_rt_index = 0xFF;
  res.should_drop = true;
  res.ignore_def = (cmd.ignore_def != 0);

  if (is_box) {
    if (map) {
      res.obj_st = map->object_state_for_index(version, cmd.floor, cmd.entity_index);
      if (!res.obj_st->super_obj) {
        throw std::runtime_error("referenced object from drop request is a player trap");
      }
      const auto* set_entry = res.obj_st->super_obj->version(version).set_entry;
      if (!set_entry) {
        throw std::runtime_error("object set entry is missing");
      }
      log.info("Drop check for K-%03zX %c %s",
          res.obj_st->k_id,
          res.ignore_def ? 'G' : 'S',
          MapFile::name_for_object_type(set_entry->base_type));
      if (cmd.floor != res.obj_st->super_obj->floor) {
        log.warning("Floor %02hhX from command does not match object\'s expected floor %02hhX",
            cmd.floor, res.obj_st->super_obj->floor);
      }
      if (is_v1_or_v2(version) && (version != Version::GC_NTE)) {
        // V1 and V2 don't have 6xA2, so we can't get ignore_def or the object
        // parameters from the client on those versions
        cmd.param3 = set_entry->param3;
        cmd.param4 = set_entry->param4;
        cmd.param5 = set_entry->param5;
        cmd.param6 = set_entry->param6;
      }
      bool object_ignore_def = (set_entry->param1 > 0.0);
      if (res.ignore_def != object_ignore_def) {
        log.warning("ignore_def value %s from command does not match object\'s expected ignore_def %s (from p1=%g)",
            res.ignore_def ? "true" : "false", object_ignore_def ? "true" : "false", set_entry->param1.load());
      }
      if (config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(client_channel, "$C5K-%03zX %c %s",
            res.obj_st->k_id,
            res.ignore_def ? 'G' : 'S',
            MapFile::name_for_object_type(set_entry->base_type));
      }
    }

  } else {
    if (map) {
      res.ene_st = map->enemy_state_for_index(version, cmd.floor, cmd.entity_index);
      EnemyType type = res.ene_st->type(version, episode, event);
      log.info("Drop check for E-%03zX %s", res.ene_st->e_id, phosg::name_for_enum(type));
      res.effective_rt_index = rare_table_index_for_enemy_type(type);
      // rt_indexes in Episode 4 don't match those sent in the command; we just
      // ignore what the client sends.
      if ((episode != Episode::EP4) && (cmd.rt_index != res.effective_rt_index)) {
        // Special cases: BULCLAW => BULK and DARK_GUNNER => DEATH_GUNNER
        if (cmd.rt_index == 0x27 && type == EnemyType::BULCLAW) {
          log.info("E-%03zX killed as BULK instead of BULCLAW", res.ene_st->e_id);
          res.effective_rt_index = 0x27;
        } else if (cmd.rt_index == 0x23 && type == EnemyType::DARK_GUNNER) {
          log.info("E-%03zX killed as DEATH_GUNNER instead of DARK_GUNNER", res.ene_st->e_id);
          res.effective_rt_index = 0x23;
        } else {
          log.warning("rt_index %02hhX from command does not match entity\'s expected index %02" PRIX32,
              cmd.rt_index, res.effective_rt_index);
          if (!is_v4(version)) {
            res.effective_rt_index = cmd.rt_index;
          }
        }
      }
      if (cmd.floor != res.ene_st->super_ene->floor) {
        log.warning("Floor %02hhX from command does not match entity\'s expected floor %02hhX",
            cmd.floor, res.ene_st->super_ene->floor);
      }
      if (config.check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_printf(client_channel, "$C5E-%03zX %s", res.ene_st->e_id, phosg::name_for_enum(type));
      }
    }
  }

  if (mark_drop) {
    if (res.obj_st) {
      if (res.obj_st->item_drop_checked) {
        log.info("Drop check has already occurred for K-%03zX; skipping it", res.obj_st->k_id);
        res.should_drop = false;
      } else {
        res.obj_st->item_drop_checked = true;
      }
    }
    if (res.ene_st) {
      if (res.ene_st->server_flags & MapState::EnemyState::Flag::ITEM_DROPPED) {
        log.info("Drop check has already occurred for E-%03zX; skipping it", res.ene_st->e_id);
        res.should_drop = false;
      } else {
        res.ene_st->server_flags |= MapState::EnemyState::Flag::ITEM_DROPPED;
      }
    }
  }

  return res;
}

static void on_entity_drop_item_request(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || l->episode == Episode::EP3) {
    return;
  }

  // Note: We always call reconcile_drop_request_with_map, even in client drop
  // mode, so that we can correctly mark enemies and objects as having dropped
  // their items in persistent games.
  G_SpecializableItemDropRequest_6xA2 cmd = normalize_drop_request(data, size);
  auto rec = reconcile_drop_request_with_map(
      c->log, c->channel, cmd, l->episode, l->event, c->config, l->map_state, true);

  Lobby::DropMode drop_mode = l->drop_mode;
  switch (drop_mode) {
    case Lobby::DropMode::DISABLED:
      return;
    case Lobby::DropMode::CLIENT: {
      // If the leader is BB, use SERVER_SHARED instead
      // TODO: We should also use server drops if any clients have incompatible object lists, since they might generate incorrect IDs for items and we can't override them
      auto leader = l->clients[l->leader_id];
      if (leader && leader->version() == Version::BB_V4) {
        drop_mode = Lobby::DropMode::SERVER_SHARED;
        break;
      } else {
        forward_subcommand(c, command, flag, data, size);
        return;
      }
    }
    case Lobby::DropMode::SERVER_SHARED:
    case Lobby::DropMode::SERVER_DUPLICATE:
    case Lobby::DropMode::SERVER_PRIVATE:
      break;
    default:
      throw logic_error("invalid drop mode");
  }

  if (rec.should_drop) {
    auto generate_item = [&]() -> ItemCreator::DropResult {
      if (rec.obj_st) {
        if (rec.ignore_def) {
          l->log.info("Creating item from box %04hX => K-%03zX (area %02hX)",
              cmd.entity_index.load(), rec.obj_st->k_id, cmd.effective_area);
          return l->item_creator->on_box_item_drop(cmd.effective_area);
        } else {
          l->log.info("Creating item from box %04hX => K-%03zX (area %02hX; specialized with %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 ")",
              cmd.entity_index.load(), rec.obj_st->k_id, cmd.effective_area,
              cmd.param3.load(), cmd.param4.load(), cmd.param5.load(), cmd.param6.load());
          return l->item_creator->on_specialized_box_item_drop(
              cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
        }
      } else if (rec.ene_st) {
        l->log.info("Creating item from enemy %04hX => E-%03zX (area %02hX)",
            cmd.entity_index.load(), rec.ene_st->e_id, cmd.effective_area);
        return l->item_creator->on_monster_item_drop(rec.effective_rt_index, cmd.effective_area);
      } else {
        throw runtime_error("neither object nor enemy were present");
      }
    };

    auto get_entity_index = [&](Version v) -> uint16_t {
      if (rec.obj_st) {
        return l->map_state->index_for_object_state(v, rec.obj_st);
      } else if (rec.ene_st) {
        return l->map_state->index_for_enemy_state(v, rec.ene_st);
      } else {
        return 0xFFFF;
      }
    };

    switch (drop_mode) {
      case Lobby::DropMode::DISABLED:
      case Lobby::DropMode::CLIENT:
        throw logic_error("unhandled simple drop mode");
      case Lobby::DropMode::SERVER_SHARED:
      case Lobby::DropMode::SERVER_DUPLICATE: {
        // TODO: In SERVER_DUPLICATE mode, should we reduce the rates for rare
        // items? Maybe by a factor of l->count_clients()?
        auto res = generate_item();
        if (res.item.empty()) {
          l->log.info("No item was created");
        } else {
          string name = s->describe_item(c->version(), res.item, false);
          l->log.info("Entity %04hX (area %02hX) created item %s", cmd.entity_index.load(), cmd.effective_area, name.c_str());
          if (drop_mode == Lobby::DropMode::SERVER_DUPLICATE) {
            for (const auto& lc : l->clients) {
              if (lc && (rec.obj_st || (lc->floor == cmd.floor))) {
                res.item.id = l->generate_item_id(0xFF);
                l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for %s",
                    res.item.id.load(), cmd.floor, cmd.pos.x.load(), cmd.pos.z.load(), lc->channel.name.c_str());
                l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ene_st, 0x1000 | (1 << lc->lobby_client_id));
                send_drop_item_to_channel(
                    s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
                send_item_notification_if_needed(s, lc->channel, lc->config, res.item, res.is_from_rare_table);
              }
            }

          } else {
            res.item.id = l->generate_item_id(0xFF);
            l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for all clients",
                res.item.id.load(), cmd.floor, cmd.pos.x.load(), cmd.pos.z.load());
            l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ene_st, 0x100F);
            for (auto lc : l->clients) {
              if (lc) {
                send_drop_item_to_channel(
                    s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
                send_item_notification_if_needed(s, lc->channel, lc->config, res.item, res.is_from_rare_table);
              }
            }
          }
        }
        break;
      }
      case Lobby::DropMode::SERVER_PRIVATE: {
        for (const auto& lc : l->clients) {
          if (lc && (rec.obj_st || (lc->floor == cmd.floor))) {
            auto res = generate_item();
            if (res.item.empty()) {
              l->log.info("No item was created for %s", lc->channel.name.c_str());
            } else {
              string name = s->describe_item(lc->version(), res.item, false);
              l->log.info("Entity %04hX (area %02hX) created item %s", cmd.entity_index.load(), cmd.effective_area, name.c_str());
              res.item.id = l->generate_item_id(0xFF);
              l->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for %s",
                  res.item.id.load(), cmd.floor, cmd.pos.x.load(), cmd.pos.z.load(), lc->channel.name.c_str());
              l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ene_st, 0x1000 | (1 << lc->lobby_client_id));
              send_drop_item_to_channel(
                  s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
              send_item_notification_if_needed(s, lc->channel, lc->config, res.item, res.is_from_rare_table);
            }
          }
        }
        break;
      }
      default:
        throw logic_error("invalid drop mode");
    }
  }
}

static void on_set_quest_flag(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  uint16_t flag_num, difficulty, action;
  if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
    const auto& cmd = check_size_t<G_UpdateQuestFlag_DC_PC_6x75>(data, size);
    flag_num = cmd.flag;
    action = cmd.action;
    difficulty = l->difficulty;
  } else {
    const auto& cmd = check_size_t<G_UpdateQuestFlag_V3_BB_6x75>(data, size);
    flag_num = cmd.flag;
    action = cmd.action;
    difficulty = cmd.difficulty;
  }

  // The client explicitly checks action for both 0 and 1 - any other value
  // means no operation is performed.
  if ((flag_num >= 0x400) || (difficulty > 3) || (action > 1)) {
    return;
  }
  bool should_set = (action == 0);

  if (l->quest_flags_known) {
    l->quest_flags_known->set(difficulty, flag_num);
  }
  if (should_set) {
    l->quest_flag_values->set(difficulty, flag_num);
  } else {
    l->quest_flag_values->clear(difficulty, flag_num);
  }

  if (c->version() == Version::BB_V4) {
    auto s = c->require_server_state();
    // TODO: Should we allow overlays here?
    auto p = c->character(true, false);
    if (should_set) {
      c->log.info("Setting quest flag %s:%04hX", name_for_difficulty(difficulty), flag_num);
      p->quest_flags.set(difficulty, flag_num);
    } else {
      c->log.info("Clearing quest flag %s:%04hX", name_for_difficulty(difficulty), flag_num);
      p->quest_flags.clear(difficulty, flag_num);
    }
  }

  forward_subcommand(c, command, flag, data, size);

  if (l->drop_mode != Lobby::DropMode::DISABLED) {
    EnemyType boss_enemy_type = EnemyType::NONE;
    bool is_ep2 = (l->episode == Episode::EP2);
    if ((l->episode == Episode::EP1) && (c->floor == 0x0E)) {
      // On Normal, Dark Falz does not have a third phase, so send the drop
      // request after the end of the second phase. On all other difficulty
      // levels, send it after the third phase.
      if ((difficulty == 0) && (flag_num == 0x0035)) {
        boss_enemy_type = EnemyType::DARK_FALZ_2;
      } else if ((difficulty != 0) && (flag_num == 0x0037)) {
        boss_enemy_type = EnemyType::DARK_FALZ_3;
      }
    } else if (is_ep2 && (flag_num == 0x0057) && (c->floor == 0x0D)) {
      boss_enemy_type = EnemyType::OLGA_FLOW_2;
    }

    if (boss_enemy_type != EnemyType::NONE) {
      l->log.info("Creating item from final boss (%s)", phosg::name_for_enum(boss_enemy_type));
      uint16_t enemy_index = 0xFFFF;
      uint8_t enemy_floor = 0xFF;
      try {
        const auto& ene_st = l->map_state->enemy_state_for_floor_type(c->version(), c->floor, boss_enemy_type);
        enemy_index = l->map_state->index_for_enemy_state(c->version(), ene_st);
        enemy_floor = ene_st->super_ene->floor;
        if (c->floor != ene_st->super_ene->floor) {
          l->log.warning("Floor %02" PRIX32 " from client does not match entity\'s expected floor %02hhX",
              c->floor, ene_st->super_ene->floor);
        }
        l->log.info("Found enemy E-%03zX at index %04hX on floor %" PRIX32, ene_st->e_id, enemy_index, ene_st->super_ene->floor);
      } catch (const out_of_range&) {
        l->log.warning("Could not find enemy on floor %" PRIX32 "; unable to determine enemy type", c->floor);
        boss_enemy_type = EnemyType::NONE;
      }

      if (boss_enemy_type != EnemyType::NONE) {
        VectorXZF pos;
        switch (boss_enemy_type) {
          case EnemyType::DARK_FALZ_2:
            pos = {-58.0f, 31.0f};
            break;
          case EnemyType::DARK_FALZ_3:
            pos = {10160.0f, 0.0f};
            break;
          case EnemyType::OLGA_FLOW_2:
            pos = {-9999.0f, 0.0f};
            break;
          default:
            throw logic_error("invalid boss enemy type");
        }

        auto s = c->require_server_state();
        auto sdt = s->set_data_table(c->version(), l->episode, l->mode, l->difficulty);
        G_StandardDropItemRequest_PC_V3_BB_6x60 drop_req = {
            {
                {0x60, 0x06, 0x0000},
                static_cast<uint8_t>(c->floor),
                rare_table_index_for_enemy_type(boss_enemy_type),
                enemy_index == 0xFFFF ? 0x0B4F : enemy_index,
                pos,
                2,
                0,
            },
            sdt->default_area_for_floor(l->episode, enemy_floor),
            {}};
        on_entity_drop_item_request(c, 0x62, l->leader_id, &drop_req, sizeof(drop_req));
      }
    }
  }
}

static void on_sync_quest_register(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  const auto& cmd = check_size_t<G_SyncQuestRegister_6x77>(data, size);
  if (cmd.register_number >= 0x100) {
    throw runtime_error("invalid register number");
  }

  // If the lock status register is being written, change the game's flags to
  // allow or forbid joining
  if (l->quest &&
      l->quest->joinable &&
      (l->quest->lock_status_register >= 0) &&
      (cmd.register_number == l->quest->lock_status_register)) {
    // Lock if value is nonzero; unlock if value is zero
    if (cmd.value.as_int) {
      l->set_flag(Lobby::Flag::QUEST_IN_PROGRESS);
      l->clear_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
    } else {
      l->clear_flag(Lobby::Flag::QUEST_IN_PROGRESS);
      l->set_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
    }
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_set_entity_set_flag(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  const auto& cmd = check_size_t<G_SetEntitySetFlags_6x76>(data, size);
  if (cmd.header.entity_id >= 0x4000) {
    try {
      auto obj_st = l->map_state->object_state_for_index(c->version(), cmd.floor, cmd.header.entity_id - 0x4000);
      obj_st->set_flags |= cmd.flags;
      l->log.info("Client set set flags %04hX on K-%03zX (flags are now %04hX)",
          cmd.flags.load(), obj_st->k_id, cmd.flags.load());
    } catch (const out_of_range&) {
      l->log.warning("Flag update refers to missing object");
    }

  } else if (cmd.header.entity_id >= 0x1000) {
    int32_t section = -1;
    int32_t wave_number = -1;
    try {
      size_t enemy_index = cmd.header.entity_id - 0x1000;
      auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.floor, enemy_index);
      if (ene_st->super_ene->child_index > 0) {
        if (ene_st->super_ene->child_index > enemy_index) {
          throw logic_error("enemy\'s child index is greater than enemy\'s absolute index");
        }
        size_t parent_index = enemy_index - ene_st->super_ene->child_index;
        l->log.info("Client set set flags %04hX on E-%03zX but it is a child (%hu); redirecting to E-%zX",
            cmd.flags.load(), ene_st->e_id, ene_st->super_ene->child_index, parent_index);
        ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.floor, parent_index);
      }
      ene_st->set_flags |= cmd.flags;
      const auto* set_entry = ene_st->super_ene->version(c->version()).set_entry;
      if (!set_entry) {
        // We should not have been able to look up this enemy if it didn't exist on this version
        throw logic_error("enemy does not exist on this game version");
      }
      section = set_entry->section;
      wave_number = set_entry->wave_number;
      l->log.info("Client set set flags %04hX on E-%03zX (flags are now %04hX)",
          cmd.flags.load(), ene_st->e_id, cmd.flags.load());
    } catch (const out_of_range&) {
      l->log.warning("Flag update refers to missing enemy");
    }

    if ((section >= 0) && (wave_number >= 0)) {
      // When all enemies in a wave event have (set_flags & 8), which means
      // they are defeated, set event_flags = (event_flags | 0x18) & (~4),
      // which means it is done and should not trigger
      bool all_enemies_defeated = true;
      l->log.info("Checking for defeated enemies with section=%04" PRIX32 " wave_number=%04" PRIX32,
          section, wave_number);
      for (auto ene_st : l->map_state->enemy_states_for_floor_section_wave(c->version(), cmd.floor, section, wave_number)) {
        if (ene_st->super_ene->child_index) {
          l->log.info("E-%03zX is a child of another enemy", ene_st->e_id);
        } else if (!(ene_st->set_flags & 8)) {
          l->log.info("E-%03zX is not defeated; cannot advance event to finished state", ene_st->e_id);
          all_enemies_defeated = false;
          break;
        } else {
          l->log.info("E-%03zX is defeated", ene_st->e_id);
        }
      }
      if (all_enemies_defeated) {
        l->log.info("All enemies defeated; setting events with section=%04" PRIX32 " wave_number=%04" PRIX32 " to finished state",
            section, wave_number);
        for (auto ev_st : l->map_state->event_states_for_floor_section_wave(c->version(), cmd.floor, section, wave_number)) {
          ev_st->flags = (ev_st->flags | 0x18) & (~4);
          l->log.info("Set flags on W-%03zX to %04hX", ev_st->w_id, ev_st->flags);

          const auto& ev_ver = ev_st->super_ev->version(c->version());
          phosg::StringReader actions_r(ev_ver.action_stream, ev_ver.action_stream_size);
          while (!actions_r.eof()) {
            uint8_t opcode = actions_r.get_u8();
            switch (opcode) {
              case 0x00: // nop
                l->log.info("(W-%03zX script) nop", ev_st->w_id);
                break;
              case 0x01: // stop
                l->log.info("(W-%03zX script) stop", ev_st->w_id);
                actions_r.go(actions_r.size());
                break;
              case 0x08: { // construct_objects
                uint16_t section = actions_r.get_u16l();
                uint16_t group = actions_r.get_u16l();
                l->log.info("(W-%03zX script) construct_objects %04hX %04hX", ev_st->w_id, section, group);
                auto obj_sts = l->map_state->object_states_for_floor_section_group(
                    c->version(), ev_st->super_ev->floor, section, group);
                for (auto obj_st : obj_sts) {
                  if (!(obj_st->set_flags & 0x0A)) {
                    l->log.info("(W-%03zX script)   Setting flags 0010 on object K-%03zX", ev_st->w_id, obj_st->k_id);
                    obj_st->set_flags |= 0x10;
                  }
                }
                break;
              }
              case 0x09: // construct_enemies
              case 0x0D: { // construct_enemies_stop
                uint16_t section = actions_r.get_u16l();
                uint16_t wave_number = actions_r.get_u16l();
                l->log.info("(W-%03zX script) construct_enemies %04hX %04hX", ev_st->w_id, section, wave_number);
                auto ene_sts = l->map_state->enemy_states_for_floor_section_wave(
                    c->version(), ev_st->super_ev->floor, section, wave_number);
                for (auto ene_st : ene_sts) {
                  if (!ene_st->super_ene->child_index && !(ene_st->set_flags & 0x0A)) {
                    l->log.info("(W-%03zX script)   Setting flags 0002 on enemy set E-%zX", ev_st->w_id, ene_st->e_id);
                    ene_st->set_flags |= 0x0002;
                  }
                }
                if (opcode == 0x0D) {
                  actions_r.go(actions_r.size());
                }
                break;
              }
              case 0x0A: // enable_switch_flag
              case 0x0B: { // disable_switch_flag
                // These opcodes cause the client to send 6x05 commands, so
                // we don't have to do anything here.
                uint16_t switch_flag_num = actions_r.get_u16l();
                l->log.info("(W-%03zX script) %sable_switch_flag %04hX",
                    ev_st->w_id, (opcode & 1) ? "dis" : "en", switch_flag_num);
                break;
              }
              case 0x0C: { // trigger_event
                // This opcode causes the client to send a 6x67 command, so
                // we don't have to do anything here.
                uint32_t event_id = actions_r.get_u32l();
                l->log.info("(W-%03zX script) trigger_event %08" PRIX32, ev_st->w_id, event_id);
                break;
              }
              default:
                l->log.warning("(W-%03zX) Invalid opcode %02hhX at offset %zX in event action stream",
                    ev_st->w_id, opcode, actions_r.where() - 1);
                actions_r.go(actions_r.size());
            }
          }
        }
      }
    }
  }

  forward_subcommand_with_entity_id_transcode_t<G_SetEntitySetFlags_6x76>(c, command, flag, data, size);
}

static void on_trigger_set_event(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  const auto& cmd = check_size_t<G_TriggerSetEvent_6x67>(data, size);
  auto event_sts = l->map_state->event_states_for_id(c->version(), cmd.floor, cmd.event_id);
  l->log.info("Client triggered set events with floor %02" PRIX32 " and ID %" PRIX32 " (%zu events)",
      cmd.floor.load(), cmd.event_id.load(), event_sts.size());
  for (auto ev_st : event_sts) {
    ev_st->flags |= 0x04;
    if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
      send_text_message_printf(c, "$C5W-%03zX START", ev_st->w_id);
    }
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_update_telepipe_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  c->telepipe_state = check_size_t<G_SetTelepipeState_6x68>(data, size);
  c->telepipe_lobby_id = l->lobby_id;

  forward_subcommand(c, command, flag, data, size);
}

static void on_update_enemy_state(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<G_UpdateEnemyState_DC_PC_XB_BB_6x0A>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (c->lobby_client_id > 3) {
    throw logic_error("client ID is above 3");
  }

  if ((cmd.enemy_index & 0xF000) || (cmd.header.entity_id != (cmd.enemy_index | 0x1000))) {
    throw runtime_error("mismatched enemy id/index");
  }
  auto ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd.enemy_index);
  ene_st->game_flags = is_big_endian(c->version()) ? bswap32(cmd.flags) : cmd.flags.load();
  ene_st->total_damage = cmd.total_damage;
  ene_st->set_last_hit_by_client_id(c->lobby_client_id);
  l->log.info("E-%03zX updated to damage=%hu game_flags=%08" PRIX32,
      ene_st->e_id, ene_st->total_damage, ene_st->game_flags);

  G_UpdateEnemyState_GC_6x0A sw_cmd = {
      {cmd.header.subcommand, cmd.header.size, cmd.header.entity_id},
      cmd.enemy_index, cmd.total_damage, cmd.flags.load()};
  bool sender_is_be = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_be) {
        cmd.enemy_index = l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (cmd.enemy_index != 0xFFFF) {
          cmd.header.entity_id = 0x1000 | cmd.enemy_index;
          send_command_t(lc, 0x60, 0x00, cmd);
        }
      } else {
        sw_cmd.enemy_index = l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (sw_cmd.enemy_index != 0xFFFF) {
          sw_cmd.header.entity_id = 0x1000 | sw_cmd.enemy_index;
          send_command_t(lc, 0x60, 0x00, sw_cmd);
        }
      }
    }
  }
}

template <typename CmdT>
static void on_update_object_state_t(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<CmdT>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto obj_st = l->map_state->object_state_for_index(c->version(), c->floor, cmd.object_index);
  obj_st->game_flags = cmd.flags;
  l->log.info("K-%03zX updated with game_flags=%08" PRIX32, obj_st->k_id, obj_st->game_flags);

  if ((cmd.object_index & 0xF000) || (cmd.header.entity_id != (cmd.object_index | 0x4000))) {
    throw runtime_error("mismatched enemy id/index");
  }

  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      cmd.object_index = l->map_state->index_for_object_state(lc->version(), obj_st);
      if (cmd.object_index != 0xFFFF) {
        cmd.header.entity_id = 0x4000 | cmd.object_index;
        send_command_t(lc, 0x60, 0x00, cmd);
      }
    }
  }
}

static void on_update_attackable_col_state(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_UpdateAttackableColState_6x91>(data, size);
  if ((cmd.object_index & 0xF000) || ((cmd.object_index | 0x4000) != cmd.header.entity_id)) {
    throw runtime_error("incorrect object IDs in 6x91 command");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  if (l->switch_flags &&
      (cmd.should_set == 1) &&
      (cmd.switch_flag_num < 0x100) &&
      (cmd.floor < 0x12) &&
      (cmd.header.entity_id >= 0x4000) &&
      (cmd.header.entity_id != 0xFFFF)) {
    l->switch_flags->set(cmd.floor, cmd.switch_flag_num);
  }

  on_update_object_state_t<G_UpdateAttackableColState_6x91>(c, command, flag, data, size);
}

static void on_activate_timed_switch(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ActivateTimedSwitch_6x93>(data, size);
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }
  if (l->switch_flags) {
    if (cmd.should_set == 1) {
      l->switch_flags->set(cmd.switch_flag_floor, cmd.switch_flag_num);
    } else {
      l->switch_flags->clear(cmd.switch_flag_floor, cmd.switch_flag_num);
    }
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_battle_scores(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_BattleScores_6x7F>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  G_BattleScoresBE_6x7F sw_cmd;
  sw_cmd.header.subcommand = 0x7F;
  sw_cmd.header.size = cmd.header.size;
  sw_cmd.header.unused = 0;
  for (size_t z = 0; z < 4; z++) {
    auto& sw_entry = sw_cmd.entries[z];
    const auto& entry = cmd.entries[z];
    sw_entry.client_id = entry.client_id.load();
    sw_entry.place = entry.place.load();
    sw_entry.score = entry.score.load();
  }
  bool sender_is_be = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_be) {
        send_command_t(lc, 0x60, 0x00, cmd);
      } else {
        send_command_t(lc, 0x60, 0x00, sw_cmd);
      }
    }
  }
}

static void on_dragon_actions(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<G_DragonBossActions_DC_PC_XB_BB_6x12>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd.header.entity_id - 0x1000);
  if (ene_st->super_ene->type != EnemyType::DRAGON) {
    throw runtime_error("6x12 command sent for incorrect enemy type");
  }

  G_DragonBossActions_GC_6x12 sw_cmd = {{cmd.header.subcommand, cmd.header.size, cmd.header.entity_id},
      cmd.unknown_a2, cmd.unknown_a3, cmd.unknown_a4, cmd.x.load(), cmd.z.load()};
  bool sender_is_be = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_be) {
        cmd.header.entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (cmd.header.entity_id != 0xFFFF) {
          send_command_t(lc, 0x60, 0x00, cmd);
        }
      } else {
        sw_cmd.header.entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (sw_cmd.header.entity_id != 0xFFFF) {
          send_command_t(lc, 0x60, 0x00, sw_cmd);
        }
      }
    }
  }
}

static void on_gol_dragon_actions(shared_ptr<Client> c, uint8_t command, uint8_t, void* data, size_t size) {
  auto& cmd = check_size_t<G_GolDragonBossActions_XB_BB_6xA8>(data, size);

  if (command_is_private(command)) {
    return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd.header.entity_id - 0x1000);
  if (ene_st->super_ene->type != EnemyType::GOL_DRAGON) {
    throw runtime_error("6xA8 command sent for incorrect enemy type");
  }

  G_GolDragonBossActions_GC_6xA8 sw_cmd = {{cmd.header.subcommand, cmd.header.size, cmd.header.entity_id},
      cmd.unknown_a2,
      cmd.unknown_a3,
      cmd.unknown_a4,
      cmd.x.load(),
      cmd.z.load(),
      cmd.unknown_a5,
      0};
  bool sender_is_be = is_big_endian(c->version());
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version()) == sender_is_be) {
        cmd.header.entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (cmd.header.entity_id != 0xFFFF) {
          send_command_t(lc, 0x60, 0x00, cmd);
        }
      } else {
        sw_cmd.header.entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), ene_st);
        if (sw_cmd.header.entity_id != 0xFFFF) {
          send_command_t(lc, 0x60, 0x00, sw_cmd);
        }
      }
    }
  }
}

static void on_charge_attack_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC7 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_ChargeAttack_BB_6xC7>(data, size);
  auto& disp = c->character()->disp;
  if (cmd.meseta_amount > disp.stats.meseta) {
    disp.stats.meseta = 0;
  } else {
    disp.stats.meseta -= cmd.meseta_amount;
  }
}

static void send_max_level_notification_if_needed(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  if (!s->notify_server_for_max_level_achieved) {
    return;
  }

  uint32_t max_level;
  if (is_v1(c->version())) {
    max_level = 99;
  } else if (!is_ep3(c->version())) {
    max_level = 199;
  } else {
    max_level = 998;
  }

  auto p = c->character();
  if (p->disp.stats.level == max_level) {
    string name = p->disp.name.decode(c->language());
    size_t level_for_str = max_level + 1;
    string message = phosg::string_printf("$C6%s$C7\nGC: %" PRIu32 "\nhas reached Level $C6%zu",
        name.c_str(), c->login->account->account_id, level_for_str);
    string bb_message = phosg::string_printf("$C6%s$C7 (GC: %" PRIu32 ") has reached Level $C6%zu",
        name.c_str(), c->login->account->account_id, level_for_str);
    for (auto& it : s->channel_to_client) {
      if ((it.second != c) && it.second->login && !is_patch(it.second->version()) && it.second->lobby.lock()) {
        send_text_or_scrolling_message(it.second, message, bb_message);
      }
    }
  }
}

static void on_level_up(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  // On the DC prototypes, this command doesn't include any stats - it just
  // increments the player's level by 1.
  auto p = c->character();
  if (is_pre_v1(c->version())) {
    check_size_t<G_ChangePlayerLevel_DCNTE_6x30>(data, size);
    auto s = c->require_server_state();
    auto level_table = s->level_table(c->version());
    const auto& level_incrs = level_table->stats_delta_for_level(p->disp.visual.char_class, p->disp.stats.level + 1);
    p->disp.stats.char_stats.atp += level_incrs.atp;
    p->disp.stats.char_stats.mst += level_incrs.mst;
    p->disp.stats.char_stats.evp += level_incrs.evp;
    p->disp.stats.char_stats.hp += level_incrs.hp;
    p->disp.stats.char_stats.dfp += level_incrs.dfp;
    p->disp.stats.char_stats.ata += level_incrs.ata;
    p->disp.stats.char_stats.lck += level_incrs.lck;
    p->disp.stats.level++;
  } else {
    const auto& cmd = check_size_t<G_ChangePlayerLevel_6x30>(data, size);
    p->disp.stats.char_stats.atp = cmd.atp;
    p->disp.stats.char_stats.mst = cmd.mst;
    p->disp.stats.char_stats.evp = cmd.evp;
    p->disp.stats.char_stats.hp = cmd.hp;
    p->disp.stats.char_stats.dfp = cmd.dfp;
    p->disp.stats.char_stats.ata = cmd.ata;
    p->disp.stats.level = cmd.level.load();
  }

  send_max_level_notification_if_needed(c);
  forward_subcommand(c, command, flag, data, size);
}

static void add_player_exp(shared_ptr<Client> c, uint32_t exp) {
  auto s = c->require_server_state();
  auto p = c->character();

  p->disp.stats.experience += exp;
  if (c->version() == Version::BB_V4) {
    send_give_experience(c, exp);
  }

  bool leveled_up = false;
  do {
    const auto& level = s->level_table(c->version())->stats_delta_for_level(p->disp.visual.char_class, p->disp.stats.level + 1);
    if (p->disp.stats.experience >= level.experience) {
      leveled_up = true;
      level.apply(p->disp.stats.char_stats);
      p->disp.stats.level++;
    } else {
      break;
    }
  } while (p->disp.stats.level < 199);

  if (leveled_up) {
    send_max_level_notification_if_needed(c);
    if (c->version() == Version::BB_V4) {
      send_level_up(c);
    }
  }
}

static uint32_t base_exp_for_enemy_type(
    shared_ptr<const BattleParamsIndex> bp_index,
    EnemyType enemy_type,
    Episode current_episode,
    uint8_t difficulty,
    uint8_t area,
    bool is_solo) {
  // Always try the current episode first. If the current episode is Ep4, try
  // Ep1 next if in Crater and Ep2 next if in Desert (this mirrors the logic in
  // BB Patch Project's omnispawn patch).
  array<Episode, 3> episode_order;
  episode_order[0] = current_episode;
  if (current_episode == Episode::EP1) {
    episode_order[1] = Episode::EP2;
    episode_order[2] = Episode::EP4;
  } else if (current_episode == Episode::EP2) {
    episode_order[1] = Episode::EP1;
    episode_order[2] = Episode::EP4;
  } else if (current_episode == Episode::EP4) {
    if (area <= 0x05) {
      episode_order[1] = Episode::EP1;
      episode_order[2] = Episode::EP2;
    } else {
      episode_order[1] = Episode::EP2;
      episode_order[2] = Episode::EP1;
    }
  } else {
    throw runtime_error("invalid episode");
  }

  for (const auto& episode : episode_order) {
    try {
      const auto& bp_table = bp_index->get_table(is_solo, episode);
      uint32_t bp_index = battle_param_index_for_enemy_type(episode, enemy_type);
      return bp_table.stats[difficulty][bp_index].experience;
    } catch (const out_of_range&) {
    }
  }
  throw runtime_error(phosg::string_printf(
      "no base exp is available (type=%s, episode=%s, difficulty=%s, area=%02hhX, solo=%s)",
      phosg::name_for_enum(enemy_type),
      name_for_episode(current_episode),
      name_for_difficulty(difficulty),
      area,
      is_solo ? "true" : "false"));
}

static void on_steal_exp_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC6 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC6 command sent in non-game lobby");
  }

  auto s = c->require_server_state();
  const auto& cmd = check_size_t<G_StealEXP_BB_6xC6>(data, size);

  auto p = c->character();
  if (c->character()->disp.stats.level >= 199) {
    return;
  }

  const auto& ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd.enemy_index);
  if (ene_st->super_ene->floor != c->floor) {
    throw runtime_error("enemy is on a different floor");
  }

  const auto& inventory = p->inventory;
  const auto& weapon = inventory.items[inventory.find_equipped_item(EquipSlot::WEAPON)];

  auto item_parameter_table = s->item_parameter_table(c->version());

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

  auto type = ene_st->type(c->version(), l->episode, l->event);
  uint32_t enemy_exp = base_exp_for_enemy_type(
      s->battle_params, type, l->episode, l->difficulty, ene_st->super_ene->floor, l->mode == GameMode::SOLO);

  // Note: The original code checks if special.type is 9, 10, or 11, and skips
  // applying the android bonus if so. We don't do anything for those special
  // types, so we don't check for that here.
  float percent = special.amount + ((l->difficulty == 3) && char_class_is_android(p->disp.visual.char_class) ? 30 : 0);
  float ep2_factor = (l->episode == Episode::EP2) ? 1.3 : 1.0;
  uint32_t stolen_exp = max<uint32_t>(min<uint32_t>((enemy_exp * percent * ep2_factor) / 100.0f, (l->difficulty + 1) * 20), 1);
  if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info("Stolen EXP from E-%03zX with enemy_exp=%" PRIu32 " percent=%g stolen_exp=%" PRIu32,
        ene_st->e_id, enemy_exp, percent, stolen_exp);
    send_text_message_printf(c, "$C5+%" PRIu32 " E-%03zX %s",
        stolen_exp, ene_st->e_id, phosg::name_for_enum(type));
  }
  add_player_exp(c, stolen_exp);
}

static void on_enemy_exp_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  const auto& cmd = check_size_t<G_EnemyEXPRequest_BB_6xC8>(data, size);

  if (!l->is_game()) {
    throw runtime_error("client should not kill enemies outside of games");
  }
  if (c->lobby_client_id > 3) {
    throw runtime_error("client ID is too large");
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), c->floor, cmd.enemy_index);
  string ene_str = ene_st->super_ene->str();
  c->log.info("EXP requested for E-%03zX: %s", ene_st->e_id, ene_str.c_str());

  // If the requesting player never hit this enemy, they are probably cheating;
  // ignore the command. Also, each player sends a 6xC8 if they ever hit the
  // enemy; we only react to the first 6xC8 for each enemy (and give all
  // relevant players EXP then, if they deserve it).
  if (!ene_st->ever_hit_by_client_id(c->lobby_client_id) ||
      (ene_st->server_flags & MapState::EnemyState::Flag::EXP_GIVEN)) {
    return;
  }
  ene_st->server_flags |= MapState::EnemyState::Flag::EXP_GIVEN;

  auto type = ene_st->type(c->version(), l->episode, l->event);
  double base_exp = base_exp_for_enemy_type(
      s->battle_params, type, l->episode, l->difficulty, ene_st->super_ene->floor, l->mode == GameMode::SOLO);

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto lc = l->clients[client_id];
    if (!lc) {
      continue;
    }

    if (base_exp != 0.0) {
      // If this player killed the enemy, they get full EXP; if they tagged the
      // enemy, they get 80% EXP; if auto EXP share is enabled and they are
      // close enough to the monster, they get a smaller share; if none of these
      // situations apply, they get no EXP.
      double rate_factor;
      if (ene_st->last_hit_by_client_id(client_id)) {
        rate_factor = max<double>(1.0, l->exp_share_multiplier);
      } else if (ene_st->ever_hit_by_client_id(client_id)) {
        rate_factor = max<double>(0.8, l->exp_share_multiplier);
      } else if (lc->floor == ene_st->super_ene->floor) {
        rate_factor = l->exp_share_multiplier;
      } else {
        rate_factor = 0.0;
      }

      if (rate_factor > 0.0) {
        // In PSOBB, Sega decided to add a 30% EXP boost for Episode 2. They
        // could have done something reasonable, like edit the BattleParamEntry
        // files so the monsters would all give more EXP, but they did
        // something far lazier instead: they just stuck an if statement in the
        // client's EXP request function. We, unfortunately, have to do the
        // same thing here.
        bool is_ep2 = (l->episode == Episode::EP2);
        uint32_t player_exp = base_exp *
            rate_factor *
            l->base_exp_multiplier *
            l->challenge_exp_multiplier *
            (is_ep2 ? 1.3 : 1.0);
        if (lc->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
          send_text_message_printf(
              lc, "$C5+%" PRIu32 " E-%03zX %s",
              player_exp,
              ene_st->e_id,
              phosg::name_for_enum(type));
        }
        if (lc->character()->disp.stats.level < 199) {
          add_player_exp(lc, player_exp);
        }
      }
    }

    // Update kill counts on unsealable items, but only for the player who
    // actually killed the enemy
    if (ene_st->last_hit_by_client_id(client_id)) {
      auto& inventory = lc->character()->inventory;
      for (size_t z = 0; z < inventory.num_items; z++) {
        auto& item = inventory.items[z];
        if ((item.flags & 0x08) && s->item_parameter_table(lc->version())->is_unsealable_item(item.data)) {
          item.data.set_kill_count(item.data.get_kill_count() + 1);
        }
      }
    }
  }
}

void on_adjust_player_meseta_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_AdjustPlayerMeseta_BB_6xC9>(data, size);

  auto p = c->character();
  if (cmd.amount < 0) {
    if (-cmd.amount > static_cast<int32_t>(p->disp.stats.meseta.load())) {
      p->disp.stats.meseta = 0;
    } else {
      p->disp.stats.meseta += cmd.amount;
    }
  } else if (cmd.amount > 0) {
    auto s = c->require_server_state();
    auto l = c->require_lobby();

    ItemData item;
    item.data1[0] = 0x04;
    item.data2d = cmd.amount.load();
    item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(item, *s->item_stack_limits(c->version()));
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
}

void on_item_reward_request_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ItemRewardRequest_BB_6xCA>(data, size);
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  const auto& limits = *s->item_stack_limits(c->version());

  ItemData item;
  item = cmd.item_data;
  item.enforce_min_stack_size(limits);
  item.id = l->generate_item_id(c->lobby_client_id);

  // The logic for the item_create and item_create2 opcodes (B3 and B4)
  // includes a precondition check to see if the player can actually add the
  // item to their inventory or not, and the entire command is skipped if not.
  // However, on BB, the implementation performs this check and sends a 6xCA
  // command instead - the item is not immediately added to the inventory, and
  // is instead added when the server sends back a 6xBE command. So if a quest
  // creates multiple items in quick succession, there may be another 6xCA/6xBE
  // sequence in flight, and the client's check if an item can be created may
  // pass when a 6xBE command that would make it fail is already on the way
  // from the server. To handle this, we simply ignore any 6xCA command if the
  // item can't be created.
  try {
    c->character()->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
    if (l->log.should_log(phosg::LogLevel::INFO)) {
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hu created inventory item %08" PRIX32 " (%s) via quest command",
          c->lobby_client_id, item.id.load(), name.c_str());
      c->print_inventory(stderr);
    }

  } catch (const out_of_range&) {
    if (l->log.should_log(phosg::LogLevel::INFO)) {
      auto name = s->describe_item(c->version(), item, false);
      l->log.info("Player %hu attempted to create inventory item %08" PRIX32 " (%s) via quest command, but it cannot be placed in their inventory",
          c->lobby_client_id, item.id.load(), name.c_str());
    }
  }
}

void on_transfer_item_via_mail_message_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_TransferItemViaMailMessage_BB_6xCB>(data, size);

  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xCB command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xCB command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xCB command sent during free play");
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    throw runtime_error("6xCB command sent by incorrect client");
  }

  auto s = c->require_server_state();
  auto p = c->character();
  const auto& limits = *s->item_stack_limits(c->version());
  auto item = p->remove_item(cmd.item_id, cmd.amount, limits);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu sent inventory item %hu:%08" PRIX32 " (%s) x%" PRIu32 " to player %08" PRIX32,
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), cmd.amount.load(), cmd.target_guild_card_number.load());
    c->print_inventory(stderr);
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
      target_c->current_bank().add_item(item, limits);
      item_sent = true;
    } catch (const runtime_error&) {
    }
  }

  if (item_sent) {
    // See the comment in the 6xCC handler about why we do this. Similar to
    // that case, the 6xCB handler on the client side does exactly the same
    // thing as 6x29, but 6x29 is backward-compatible with other PSO versions
    // and 6xCB is not.
    G_DeleteInventoryItem_6x29 cmd29 = {{0x29, 0x03, cmd.header.client_id}, cmd.item_id, cmd.amount};
    forward_subcommand(c, command, flag, &cmd29, sizeof(cmd29));
    send_command(c, 0x16EA, 0x00000001);
  } else {
    send_command(c, 0x16EA, 0x00000000);
    // If the item failed to send, add it back to the sender's inventory
    item.id = l->generate_item_id(0xFF);
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
}

void on_exchange_item_for_team_points_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_ExchangeItemForTeamPoints_BB_6xCC>(data, size);

  auto team = c->team();
  if (!team) {
    throw runtime_error("player is not in a team");
  }
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xCC command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xCC command sent in non-game lobby");
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    throw runtime_error("6xCC command sent by incorrect client");
  }

  auto s = c->require_server_state();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, cmd.amount, *s->item_stack_limits(c->version()));

  size_t points = s->item_parameter_table(Version::BB_V4)->get_item_team_points(item);
  s->team_index->add_member_points(c->login->account->account_id, points);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu exchanged inventory item %hu:%08" PRIX32 " (%s) for %zu team points",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str(), points);
    c->print_inventory(stderr);
  }

  // The original implementation forwarded the 6xCC command to all other
  // clients. However, the handler does exactly the same thing as 6x29 if the
  // affected client isn't the local client. Since the sender has already
  // processed the 6xCC that they sent by the time we receive this, we pretend
  // that they sent 6x29 instead and send that to the others in the game.
  G_DeleteInventoryItem_6x29 cmd29 = {{0x29, 0x03, cmd.header.client_id}, cmd.item_id, cmd.amount};
  forward_subcommand(c, command, flag, &cmd29, sizeof(cmd29));
}

static void on_destroy_inventory_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
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
  auto item = p->remove_item(cmd.item_id, cmd.amount, *s->item_stack_limits(c->version()));

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu destroyed inventory item %hu:%08" PRIX32 " (%s)",
        c->lobby_client_id, cmd.header.client_id.load(), cmd.item_id.load(), name.c_str());
    c->print_inventory(stderr);
  }
  forward_subcommand(c, command, flag, data, size);
}

static void on_destroy_floor_item(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_DestroyFloorItem_6x5C_6x63>(data, size);

  bool is_6x5C;
  switch (c->version()) {
    case Version::DC_NTE:
      is_6x5C = (cmd.header.subcommand == 0x4E);
      break;
    case Version::DC_11_2000:
      is_6x5C = (cmd.header.subcommand == 0x55);
      break;
    default:
      is_6x5C = (cmd.header.subcommand == 0x5C);
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    return;
  }

  auto s = c->require_server_state();
  shared_ptr<Lobby::FloorItem> fi;
  try {
    fi = l->remove_item(cmd.floor, cmd.item_id, 0xFF);
  } catch (const out_of_range&) {
  }

  if (!fi) {
    // There are generally two data races that could occur here. Either the
    // player attempted to evict the item at the same time the server did (that
    // is, the client's and server's 6x63 commands crossed paths on the
    // network), or the player attempted to evict an item that was already
    // picked up. The former case is easy to handle; we can just ignore the
    // command. The latter case is more difficult - we have to know which
    // player picked up the item and send a 6x2B command to the sender, to sync
    // their item state with the server's again. We can't just look through the
    // players' inventories to find the item ID, since item IDs can be
    // destroyed when stackable items or Meseta are picked up.
    // TODO: We don't actually handle the evict/pickup conflict case. This case
    // is probably quite rare, but we should eventually handle it.
    l->log.info("Player %hhu attempted to destroy floor item %08" PRIX32 ", but it is missing",
        c->lobby_client_id, cmd.item_id.load());

  } else {
    auto name = s->describe_item(c->version(), fi->data, false);
    l->log.info("Player %hhu destroyed floor item %08" PRIX32 " (%s)", c->lobby_client_id, cmd.item_id.load(), name.c_str());

    // Only forward to players for whom the item was visible
    for (size_t z = 0; z < l->clients.size(); z++) {
      auto lc = l->clients[z];
      if (lc && fi->visible_to_client(z)) {
        if (lc->version() != c->version()) {
          G_DestroyFloorItem_6x5C_6x63 out_cmd = cmd;
          switch (lc->version()) {
            case Version::DC_NTE:
              out_cmd.header.subcommand = is_6x5C ? 0x4E : 0x55;
              break;
            case Version::DC_11_2000:
              out_cmd.header.subcommand = is_6x5C ? 0x55 : 0x5C;
              break;
            default:
              out_cmd.header.subcommand = is_6x5C ? 0x5C : 0x63;
          }
          send_command_t(lc, command, flag, out_cmd);
        } else {
          send_command_t(lc, command, flag, cmd);
        }
      }
    }
  }
}

static void on_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xDA command sent in non-game lobby");
  }

  if (c->version() == Version::BB_V4) {
    const auto& cmd = check_size_t<G_IdentifyItemRequest_6xB8>(data, size);
    if (!l->is_game() || l->episode == Episode::EP3) {
      return;
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
    l->item_creator->apply_tekker_deltas(c->bb_identify_result, l->effective_section_id());
    send_item_identify_result(c);

  } else {
    forward_subcommand(c, command, flag, data, size);
  }
}

static void on_accept_identify_item_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xB5 command sent in non-game lobby");
  }

  if (is_ep3(c->version())) {
    forward_subcommand(c, command, flag, data, size);

  } else if (c->version() == Version::BB_V4) {
    const auto& cmd = check_size_t<G_AcceptItemIdentification_BB_6xBA>(data, size);

    if (!c->bb_identify_result.id || (c->bb_identify_result.id == 0xFFFFFFFF)) {
      throw runtime_error("no identify result present");
    }
    if (c->bb_identify_result.id != cmd.item_id) {
      throw runtime_error("accepted item ID does not match previous identify request");
    }
    auto s = c->require_server_state();
    c->character()->add_item(c->bb_identify_result, *s->item_stack_limits(c->version()));
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, c->bb_identify_result);
    c->bb_identify_result.clear();
  }
}

static void on_sell_item_at_shop_bb(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC0 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC0 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_SellItemAtShop_BB_6xC0>(data, size);

  auto s = c->require_server_state();
  auto p = c->character();
  auto item = p->remove_item(cmd.item_id, cmd.amount, *s->item_stack_limits(c->version()));
  size_t price = (s->item_parameter_table(c->version())->price_for_item(item) >> 3) * cmd.amount;
  p->add_meseta(price);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu sold inventory item %08" PRIX32 " (%s) for %zu Meseta",
        c->lobby_client_id, cmd.item_id.load(), name.c_str(), price);
    c->print_inventory(stderr);
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_buy_shop_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xB7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xB7 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_BuyShopItem_BB_6xB7>(data, size);
  auto s = c->require_server_state();
  const auto& limits = *s->item_stack_limits(c->version());

  ItemData item;
  item = c->bb_shop_contents.at(cmd.shop_type).at(cmd.item_index);
  if (item.is_stackable(limits)) {
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
  p->add_item(item, limits);
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, item, true);

  if (l->log.should_log(phosg::LogLevel::INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item, false);
    l->log.info("Player %hhu purchased item %08" PRIX32 " (%s) for %zu meseta",
        c->lobby_client_id, item.id.load(), name.c_str(), price);
    c->print_inventory(stderr);
  }
}

static void on_medical_center_bb(shared_ptr<Client> c, uint8_t, uint8_t, void*, size_t) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC5 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC5 command sent in non-game lobby");
  }

  c->character()->remove_meseta(10, false);
}

static void on_battle_restart_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xCF command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xCF command sent in non-game lobby");
  }
  if (l->episode == Episode::EP3) {
    throw runtime_error("6xCF command sent in Episode 3 game");
  }
  if (l->mode != GameMode::BATTLE) {
    throw runtime_error("6xCF command sent in non-battle game");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    throw runtime_error("6xCF command sent during free play");
  }
  if (!l->quest) {
    throw runtime_error("6xCF command sent without quest loaded");
  }
  if (l->leader_id != c->lobby_client_id) {
    throw runtime_error("6xCF command sent by non-leader");
  }

  auto s = c->require_server_state();
  const auto& cmd = check_size_t<G_StartBattle_BB_6xCF>(data, size);

  auto new_rules = make_shared<BattleRules>(cmd.rules);
  l->item_creator->set_restrictions(new_rules);

  for (auto& lc : l->clients) {
    if (lc) {
      lc->delete_overlay();
      lc->use_default_bank();
      lc->create_battle_overlay(new_rules, s->level_table(c->version()));
    }
  }
  l->map_state->reset();
}

static void on_battle_level_up_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD0 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD0 command sent in non-game lobby");
  }
  if (l->mode != GameMode::BATTLE) {
    throw runtime_error("6xD0 command sent during free play");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    throw runtime_error("6xD0 command sent during free play");
  }

  const auto& cmd = check_size_t<G_BattleModeLevelUp_BB_6xD0>(data, size);
  auto lc = l->clients.at(cmd.header.client_id);
  if (lc) {
    auto s = c->require_server_state();
    auto lp = lc->character();
    uint32_t target_level = min<uint32_t>(lp->disp.stats.level + cmd.num_levels, 199);
    uint32_t before_exp = lp->disp.stats.experience;
    int32_t exp_delta = lp->disp.stats.experience - before_exp;
    if (exp_delta > 0) {
      s->level_table(lc->version())->advance_to_level(lp->disp.stats, target_level, lp->disp.visual.char_class);
      if (lc->version() == Version::BB_V4) {
        send_give_experience(lc, exp_delta);
        send_level_up(lc);
      }
    }
  }
}

static void on_request_challenge_grave_recovery_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD1 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD1 command sent in non-game lobby");
  }
  if (l->mode != GameMode::CHALLENGE) {
    throw runtime_error("6xD1 command sent in non-challenge game");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
    throw runtime_error("6xD1 command sent during free play");
  }

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
  item.id = l->generate_item_id(cmd.header.client_id);
  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x100F);
  send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.pos);
}

static void on_challenge_mode_retry_or_quit(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  const auto& cmd = check_size_t<G_SelectChallengeModeFailureOption_6x97>(data, size);

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.is_retry == 1) && l->quest && (l->quest->challenge_template_index >= 0)) {
    auto s = l->require_server_state();

    for (auto& m : l->floor_item_managers) {
      m.clear();
    }

    for (auto lc : l->clients) {
      if (lc) {
        lc->use_default_bank();
        lc->create_challenge_overlay(lc->version(), l->quest->challenge_template_index, s->level_table(c->version()));
        lc->log.info("Created challenge overlay");
        l->assign_inventory_and_bank_item_ids(lc, true);
      }
    }

    l->map_state->reset();
  }

  forward_subcommand(c, command, flag, data, size);
}

static void on_challenge_update_records(shared_ptr<Client> c, uint8_t command, uint8_t flag, void* data, size_t size) {
  auto l = c->lobby.lock();
  if (!l) {
    c->log.warning("Not in any lobby; dropping command");
    return;
  }

  const auto& cmd = check_size_t<G_SetChallengeRecordsBase_6x7C>(data, size, 0xFFFF);
  if (cmd.client_id != c->lobby_client_id) {
    return;
  }

  auto p = c->character(true, false);
  Version c_version = c->version();
  switch (c_version) {
    case Version::DC_V2:
    case Version::GC_NTE: {
      const auto& cmd = check_size_t<G_SetChallengeRecords_DC_6x7C>(data, size);
      p->challenge_records = cmd.records;
      break;
    }
    case Version::PC_V2: {
      const auto& cmd = check_size_t<G_SetChallengeRecords_PC_6x7C>(data, size);
      p->challenge_records = cmd.records;
      break;
    }
    case Version::GC_V3:
    case Version::XB_V3: {
      const auto& cmd = check_size_t<G_SetChallengeRecords_V3_6x7C>(data, size);
      p->challenge_records = cmd.records;
      break;
    }
    case Version::BB_V4: {
      const auto& cmd = check_size_t<G_SetChallengeRecords_BB_6x7C>(data, size);
      p->challenge_records = cmd.records;
      break;
    }
    default:
      throw runtime_error("game version cannot send 6x7C");
  }

  string dc_data;
  string pc_data;
  string v3_data;
  string bb_data;
  auto send_to_client = [&](shared_ptr<Client> lc) -> void {
    Version lc_version = lc->version();
    const void* data_to_send = nullptr;
    size_t size_to_send = 0;
    if ((lc_version == c_version) || (is_v3(lc_version) && is_v3(c_version))) {
      data_to_send = data;
      size_to_send = size;
    } else if ((lc->version() == Version::DC_V2) || (lc->version() == Version::GC_NTE)) {
      if (dc_data.empty()) {
        dc_data.resize(sizeof(G_SetChallengeRecords_DC_6x7C));
        auto& dc_cmd = check_size_t<G_SetChallengeRecords_DC_6x7C>(dc_data);
        dc_cmd.header = cmd.header;
        dc_cmd.header.size = sizeof(G_SetChallengeRecords_DC_6x7C) >> 2;
        dc_cmd.client_id = cmd.client_id;
        dc_cmd.records = p->challenge_records;
      }
      data_to_send = dc_data.data();
      size_to_send = dc_data.size();
    } else if (lc->version() == Version::PC_V2) {
      if (pc_data.empty()) {
        pc_data.resize(sizeof(G_SetChallengeRecords_PC_6x7C));
        auto& pc_cmd = check_size_t<G_SetChallengeRecords_PC_6x7C>(pc_data);
        pc_cmd.header = cmd.header;
        pc_cmd.header.size = sizeof(G_SetChallengeRecords_PC_6x7C) >> 2;
        pc_cmd.client_id = cmd.client_id;
        pc_cmd.records = p->challenge_records;
      }
      data_to_send = pc_data.data();
      size_to_send = pc_data.size();
    } else if (is_v3(lc->version())) {
      if (v3_data.empty()) {
        v3_data.resize(sizeof(G_SetChallengeRecords_V3_6x7C));
        auto& v3_cmd = check_size_t<G_SetChallengeRecords_V3_6x7C>(v3_data);
        v3_cmd.header = cmd.header;
        v3_cmd.header.size = sizeof(G_SetChallengeRecords_V3_6x7C) >> 2;
        v3_cmd.client_id = cmd.client_id;
        v3_cmd.records = p->challenge_records;
      }
      data_to_send = v3_data.data();
      size_to_send = v3_data.size();
    } else if (is_v4(lc->version())) {
      if (bb_data.empty()) {
        bb_data.resize(sizeof(G_SetChallengeRecords_BB_6x7C));
        auto& bb_cmd = check_size_t<G_SetChallengeRecords_BB_6x7C>(bb_data);
        bb_cmd.header = cmd.header;
        bb_cmd.header.size = sizeof(G_SetChallengeRecords_BB_6x7C) >> 2;
        bb_cmd.client_id = cmd.client_id;
        bb_cmd.records = p->challenge_records;
      }
      data_to_send = bb_data.data();
      size_to_send = bb_data.size();
    }

    if (!data_to_send || !size_to_send) {
      lc->log.info("Command cannot be translated to client\'s version");
    } else {
      send_command(lc, command, flag, data_to_send, size_to_send);
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
    for (auto& lc : l->clients) {
      if (lc && (lc != c)) {
        send_to_client(lc);
      }
    }
  }
}

static void on_quest_exchange_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD5 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD5 command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xD5 command sent during free play");
  }

  const auto& cmd = check_size_t<G_ExchangeItemInQuest_BB_6xD5>(data, size);
  auto s = c->require_server_state();

  try {
    auto p = c->character();
    const auto& limits = *s->item_stack_limits(c->version());

    size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, limits);
    send_destroy_item_to_lobby(c, found_item.id, 1);

    // TODO: We probably should use an allow-list here to prevent the client
    // from creating arbitrary items if cheat mode is disabled.
    ItemData new_item = cmd.replace_item;
    new_item.enforce_min_stack_size(limits);
    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning("Quest item exchange failed: %s", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
}

static void on_wrap_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD6 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD6 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_WrapItem_BB_6xD6>(data, size);
  auto s = c->require_server_state();

  auto p = c->character();
  auto item = p->remove_item(cmd.item.id, 1, *s->item_stack_limits(c->version()));
  send_destroy_item_to_lobby(c, item.id, 1);
  item.wrap(*s->item_stack_limits(c->version()), cmd.present_color);
  p->add_item(item, *s->item_stack_limits(c->version()));
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
}

static void on_photon_drop_exchange_for_item_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD7 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_PaganiniPhotonDropExchange_BB_6xD7>(data, size);
  auto s = c->require_server_state();

  try {
    auto p = c->character();
    const auto& limits = *s->item_stack_limits(c->version());

    size_t found_index = p->inventory.find_item_by_primary_identifier(0x03100000);
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 0, limits);
    send_destroy_item_to_lobby(c, found_item.id, found_item.stack_size(limits));

    // TODO: We probably should use an allow-list here to prevent the client
    // from creating arbitrary items if cheat mode is disabled.
    ItemData new_item = cmd.new_item;
    new_item.enforce_min_stack_size(limits);
    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning("Quest Photon Drop exchange for item failed: %s", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
}

static void on_photon_drop_exchange_for_s_rank_special_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD8 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD8 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_AddSRankWeaponSpecial_BB_6xD8>(data, size);
  auto s = c->require_server_state();
  const auto& limits = *s->item_stack_limits(c->version());

  try {
    auto p = c->character();

    static const array<uint8_t, 0x10> costs({60, 60, 20, 20, 30, 30, 30, 50, 40, 50, 40, 40, 50, 40, 40, 40});
    uint8_t cost = costs.at(cmd.special_type);

    size_t payment_item_index = p->inventory.find_item_by_primary_identifier(0x03100000);
    // Ensure weapon exists before removing PDs, so inventory state will be
    // consistent in case of error
    p->inventory.find_item(cmd.item_id);

    auto payment_item = p->remove_item(p->inventory.items[payment_item_index].data.id, cost, limits);
    send_destroy_item_to_lobby(c, payment_item.id, cost);

    auto item = p->remove_item(cmd.item_id, 1, limits);
    send_destroy_item_to_lobby(c, item.id, cost);
    item.data1[2] = cmd.special_type;
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning("Quest Photon Drop exchange for S-rank special failed: %s", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
}

static void on_secret_lottery_ticket_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xDE command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xDE command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xDE command sent during free play");
  }

  auto s = c->require_server_state();
  const auto& cmd = check_size_t<G_ExchangeSecretLotteryTicket_BB_6xDE>(data, size);

  if (s->secret_lottery_results.empty()) {
    throw runtime_error("no secret lottery results are defined");
  }

  auto p = c->character();
  ssize_t slt_index = -1;
  try {
    slt_index = p->inventory.find_item_by_primary_identifier(0x03100300); // Secret Lottery Ticket
  } catch (const out_of_range&) {
  }

  if (slt_index >= 0) {
    const auto& limits = *s->item_stack_limits(c->version());
    uint32_t slt_item_id = p->inventory.items[slt_index].data.id;

    G_ExchangeItemInQuest_BB_6xDB exchange_cmd;
    exchange_cmd.header.subcommand = 0xDB;
    exchange_cmd.header.size = 4;
    exchange_cmd.header.client_id = c->lobby_client_id;
    exchange_cmd.unknown_a1 = 1;
    exchange_cmd.item_id = slt_item_id;
    exchange_cmd.amount = 1;
    send_command_t(c, 0x60, 0x00, exchange_cmd);

    p->remove_item(slt_item_id, 1, limits);

    ItemData item = (s->secret_lottery_results.size() == 1)
        ? s->secret_lottery_results[0]
        : s->secret_lottery_results[random_from_optional_crypt(l->opt_rand_crypt) % s->secret_lottery_results.size()];
    item.enforce_min_stack_size(limits);
    item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }

  S_ExchangeSecretLotteryTicketResult_BB_24 out_cmd;
  out_cmd.start_index = cmd.index;
  out_cmd.label = cmd.success_label;
  if (s->secret_lottery_results.empty()) {
    out_cmd.unknown_a3.clear(0);
  } else if (s->secret_lottery_results.size() == 1) {
    out_cmd.unknown_a3.clear(1);
  } else {
    for (size_t z = 0; z < out_cmd.unknown_a3.size(); z++) {
      out_cmd.unknown_a3[z] = random_from_optional_crypt(l->opt_rand_crypt) % s->secret_lottery_results.size();
    }
  }
  send_command_t(c, 0x24, (slt_index >= 0) ? 0 : 1, out_cmd);
}

static void on_photon_crystal_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xDF command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xDF command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xDF command sent during free play");
  }

  check_size_t<G_ExchangePhotonCrystals_BB_6xDF>(data, size);
  auto s = c->require_server_state();
  auto p = c->character();
  size_t index = p->inventory.find_item_by_primary_identifier(0x03100200);
  auto item = p->remove_item(p->inventory.items[index].data.id, 1, *s->item_stack_limits(c->version()));
  send_destroy_item_to_lobby(c, item.id, 1);
  l->drop_mode = Lobby::DropMode::DISABLED;
  l->allowed_drop_modes = (1 << static_cast<uint8_t>(l->drop_mode)); // DISABLED only
}

static void on_quest_F95E_result_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xE0 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xE0 command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xE0 command sent during free play");
  }

  const auto& cmd = check_size_t<G_RequestItemDropFromQuest_BB_6xE0>(data, size);
  auto s = c->require_server_state();

  size_t count = (cmd.type > 0x03) ? 1 : (l->difficulty + 1);
  for (size_t z = 0; z < count; z++) {
    const auto& results = s->quest_F95E_results.at(cmd.type).at(l->difficulty);
    if (results.empty()) {
      throw runtime_error("invalid result type");
    }
    ItemData item = (results.size() == 1) ? results[0] : results[random_from_optional_crypt(l->opt_rand_crypt) % results.size()];
    if (item.data1[0] == 0x04) { // Meseta
      // TODO: What is the right amount of Meseta to use here? Presumably it
      // should be random within a certain range, but it's not obvious what
      // that range should be.
      item.data2d = 100;
    } else if (item.data1[0] == 0x00) {
      item.data1[4] |= 0x80; // Unidentified
    } else {
      item.enforce_min_stack_size(*s->item_stack_limits(c->version()));
    }

    item.id = l->generate_item_id(0xFF);
    l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x100F);

    send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.pos);
  }
}

static void on_quest_F95F_result_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xE1 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xE1 command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xE1 command sent during free play");
  }

  const auto& cmd = check_size_t<G_ExchangePhotonTickets_BB_6xE1>(data, size);
  auto s = c->require_server_state();
  auto p = c->character();

  const auto& result = s->quest_F95F_results.at(cmd.result_index);
  if (result.second.empty()) {
    throw runtime_error("invalid result index");
  }

  const auto& limits = *s->item_stack_limits(c->version());
  size_t index = p->inventory.find_item_by_primary_identifier(0x03100400); // Photon Ticket
  auto ticket_item = p->remove_item(p->inventory.items[index].data.id, result.first, limits);
  // TODO: Shouldn't we send a 6x29 here? Check if this causes desync in an
  // actual game

  G_ExchangeItemInQuest_BB_6xDB cmd_6xDB;
  cmd_6xDB.header = {0xDB, 0x04, c->lobby_client_id};
  cmd_6xDB.unknown_a1 = 1;
  cmd_6xDB.item_id = ticket_item.id;
  cmd_6xDB.amount = result.first;
  send_command_t(c, 0x60, 0x00, cmd_6xDB);

  ItemData new_item = result.second;
  new_item.enforce_min_stack_size(limits);
  new_item.id = l->generate_item_id(c->lobby_client_id);
  p->add_item(new_item, limits);
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

  S_GallonPlanResult_BB_25 out_cmd;
  out_cmd.label = cmd.success_label;
  out_cmd.offset1 = 0x3C;
  out_cmd.offset2 = 0x08;
  out_cmd.value1 = 0x00;
  out_cmd.value2 = cmd.result_index;
  send_command_t(c, 0x25, 0x00, out_cmd);
}

static void on_quest_F960_result_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xE2 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xE2 command sent in non-game lobby");
  }

  const auto& cmd = check_size_t<G_GetMesetaSlotPrize_BB_6xE2>(data, size);
  auto s = c->require_server_state();
  auto p = c->character();

  time_t t_secs = phosg::now() / 1000000;
  struct tm t_parsed;
  gmtime_r(&t_secs, &t_parsed);
  size_t weekday = t_parsed.tm_wday;

  ItemData item;
  for (size_t num_failures = 0; num_failures <= cmd.result_tier; num_failures++) {
    size_t tier = cmd.result_tier - num_failures;
    const auto& results = s->quest_F960_success_results.at(tier);
    uint64_t probability = results.base_probability + num_failures * results.probability_upgrade;
    if (random_from_optional_crypt(l->opt_rand_crypt) <= probability) {
      c->log.info("Tier %zu yielded a prize", tier);
      const auto& result_items = results.results.at(weekday);
      item = result_items[random_from_optional_crypt(l->opt_rand_crypt) % result_items.size()];
      break;
    } else {
      c->log.info("Tier %zu did not yield a prize", tier);
    }
  }
  if (item.empty()) {
    c->log.info("Choosing result from failure tier");
    const auto& result_items = s->quest_F960_failure_results.results.at(weekday);
    item = result_items[random_from_optional_crypt(l->opt_rand_crypt) % result_items.size()];
  }
  if (item.empty()) {
    throw runtime_error("no item produced, even from failure tier");
  }

  // The client sends a 6xC9 to remove Meseta before sending 6xE2, so we don't
  // have to deal with Meseta here.

  item.id = l->generate_item_id(c->lobby_client_id);
  // If it's a weapon, make it unidentified
  auto item_parameter_table = s->item_parameter_table(c->version());
  if ((item.data1[0] == 0x00) && (item_parameter_table->is_item_rare(item) || (item.data1[4] != 0))) {
    item.data1[4] |= 0x80;
  }

  // The 6xE3 handler on the client fails if the item already exists, so we
  // need to send 6xE3 before we call send_create_inventory_item_to_lobby.
  G_SetMesetaSlotPrizeResult_BB_6xE3 cmd_6xE3 = {
      {0xE3, sizeof(G_SetMesetaSlotPrizeResult_BB_6xE3) >> 2, c->lobby_client_id}, item};
  send_command_t(c, 0x60, 0x00, cmd_6xE3);

  try {
    p->add_item(item, *s->item_stack_limits(c->version()));
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
    if (c->log.should_log(phosg::LogLevel::INFO)) {
      string name = s->describe_item(c->version(), item, false);
      c->log.info("Awarded item %s", name.c_str());
    }
  } catch (const out_of_range&) {
    if (c->log.should_log(phosg::LogLevel::INFO)) {
      string name = s->describe_item(c->version(), item, false);
      c->log.info("Attempted to award item %s, but inventory was full", name.c_str());
    }
  }
}

static void on_momoka_item_exchange_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD9 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD9 command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xD9 command sent during free play");
  }

  const auto& cmd = check_size_t<G_MomokaItemExchange_BB_6xD9>(data, size);
  auto s = c->require_server_state();
  auto p = c->character();
  try {
    const auto& limits = *s->item_stack_limits(c->version());
    size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, limits);

    G_ExchangeItemInQuest_BB_6xDB cmd_6xDB = {{0xDB, 0x04, c->lobby_client_id}, 1, found_item.id, 1};
    send_command_t(c, 0x60, 0x00, cmd_6xDB);

    send_destroy_item_to_lobby(c, found_item.id, 1);

    // TODO: We probably should use an allow-list here to prevent the client
    // from creating arbitrary items if cheat mode is disabled.
    ItemData new_item = cmd.replace_item;
    new_item.enforce_min_stack_size(limits);
    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_command(c, 0x23, 0x00);
  } catch (const exception& e) {
    c->log.warning("Momoka item exchange failed: %s", e.what());
    send_command(c, 0x23, 0x01);
  }
}

static void on_upgrade_weapon_attribute_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xDA command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xDA command sent in non-game lobby");
  }
  if (!l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) && !l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
    throw runtime_error("6xDA command sent during free play");
  }

  const auto& cmd = check_size_t<G_UpgradeWeaponAttribute_BB_6xDA>(data, size);
  auto s = c->require_server_state();
  auto p = c->character();
  try {
    size_t item_index = p->inventory.find_item(cmd.item_id);
    auto& item = p->inventory.items[item_index].data;

    uint32_t payment_primary_identifier = cmd.payment_type ? 0x03100100 : 0x03100000;
    size_t payment_index = p->inventory.find_item_by_primary_identifier(payment_primary_identifier);
    auto& payment_item = p->inventory.items[payment_index].data;
    if (payment_item.stack_size(*s->item_stack_limits(c->version())) < cmd.payment_count) {
      throw runtime_error("not enough payment items present");
    }
    p->remove_item(payment_item.id, cmd.payment_count, *s->item_stack_limits(c->version()));
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
    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning("Weapon attribute upgrade failed: %s", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
}

static void on_write_quest_counter_bb(shared_ptr<Client> c, uint8_t, uint8_t, void* data, size_t size) {
  const auto& cmd = check_size_t<G_SetQuestCounter_BB_6xD2>(data, size);
  c->character()->quest_counters[cmd.index] = cmd.value;
}

////////////////////////////////////////////////////////////////////////////////

// This makes it easier to see which handlers exist on which prototypes via
// syntax highlighting
constexpr uint8_t NONE = 0x00;

const SubcommandDefinition subcommand_definitions[0x100] = {
    /* 6x00 */ {0x00, 0x00, 0x00, on_invalid},
    /* 6x01 */ {0x01, 0x01, 0x01, on_invalid},
    /* 6x02 */ {0x02, 0x02, 0x02, forward_subcommand_m},
    /* 6x03 */ {0x03, 0x03, 0x03, forward_subcommand_m},
    /* 6x04 */ {0x04, 0x04, 0x04, forward_subcommand_m},
    /* 6x05 */ {0x05, 0x05, 0x05, on_switch_state_changed},
    /* 6x06 */ {0x06, 0x06, 0x06, on_send_guild_card},
    /* 6x07 */ {0x07, 0x07, 0x07, on_symbol_chat, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6x08 */ {0x08, 0x08, 0x08, on_invalid},
    /* 6x09 */ {0x09, 0x09, 0x09, forward_subcommand_with_entity_id_transcode_t<G_Unknown_6x09>},
    /* 6x0A */ {0x0A, 0x0A, 0x0A, on_update_enemy_state},
    /* 6x0B */ {0x0B, 0x0B, 0x0B, on_update_object_state_t<G_UpdateObjectState_6x0B>},
    /* 6x0C */ {0x0C, 0x0C, 0x0C, on_received_condition},
    /* 6x0D */ {NONE, NONE, 0x0D, on_forward_check_game},
    /* 6x0E */ {NONE, NONE, 0x0E, on_forward_check_game},
    /* 6x0F */ {NONE, NONE, 0x0F, on_invalid},
    /* 6x10 */ {0x0E, 0x0E, 0x10, forward_subcommand_with_entity_id_transcode_t<G_Unknown_6x10_6x11_6x14>},
    /* 6x11 */ {0x0F, 0x0F, 0x11, forward_subcommand_with_entity_id_transcode_t<G_Unknown_6x10_6x11_6x14>},
    /* 6x12 */ {0x10, 0x10, 0x12, on_dragon_actions},
    /* 6x13 */ {0x11, 0x11, 0x13, forward_subcommand_with_entity_id_transcode_t<G_DeRolLeBossActions_6x13>},
    /* 6x14 */ {0x12, 0x12, 0x14, forward_subcommand_with_entity_id_transcode_t<G_Unknown_6x10_6x11_6x14>},
    /* 6x15 */ {0x13, 0x13, 0x15, forward_subcommand_with_entity_id_transcode_t<G_VolOptBossActions_6x15>},
    /* 6x16 */ {0x14, 0x14, 0x16, forward_subcommand_with_entity_id_transcode_t<G_VolOptBossActions_6x16>},
    /* 6x17 */ {0x15, 0x15, 0x17, forward_subcommand_with_entity_id_transcode_t<G_VolOpt2BossActions_6x17>},
    /* 6x18 */ {0x16, 0x16, 0x18, forward_subcommand_with_entity_id_transcode_t<G_VolOpt2BossActions_6x18>},
    /* 6x19 */ {0x17, 0x17, 0x19, forward_subcommand_with_entity_id_transcode_t<G_DarkFalzActions_6x19>},
    /* 6x1A */ {NONE, NONE, 0x1A, on_invalid},
    /* 6x1B */ {NONE, 0x19, 0x1B, on_forward_check_game},
    /* 6x1C */ {NONE, 0x1A, 0x1C, on_forward_check_game},
    /* 6x1D */ {0x19, 0x1B, 0x1D, on_invalid},
    /* 6x1E */ {0x1A, 0x1C, 0x1E, on_invalid},
    /* 6x1F */ {0x1B, 0x1D, 0x1F, on_change_floor_6x1F},
    /* 6x20 */ {0x1C, 0x1E, 0x20, on_movement_with_floor<G_SetPosition_6x20>},
    /* 6x21 */ {0x1D, 0x1F, 0x21, on_change_floor_6x21},
    /* 6x22 */ {0x1E, 0x20, 0x22, on_forward_check_client},
    /* 6x23 */ {0x1F, 0x21, 0x23, on_set_player_visible},
    /* 6x24 */ {0x20, 0x22, 0x24, on_forward_check_game},
    /* 6x25 */ {0x21, 0x23, 0x25, on_equip_item},
    /* 6x26 */ {0x22, 0x24, 0x26, on_unequip_item}, // TODO: Why does BB allow this in the lobby?
    /* 6x27 */ {0x23, 0x25, 0x27, on_use_item},
    /* 6x28 */ {0x24, 0x26, 0x28, on_feed_mag},
    /* 6x29 */ {0x25, 0x27, 0x29, on_destroy_inventory_item},
    /* 6x2A */ {0x26, 0x28, 0x2A, on_player_drop_item},
    /* 6x2B */ {0x27, 0x29, 0x2B, on_create_inventory_item},
    /* 6x2C */ {0x28, 0x2A, 0x2C, on_forward_check_client},
    /* 6x2D */ {0x29, 0x2B, 0x2D, on_forward_check_client},
    /* 6x2E */ {0x2A, 0x2C, 0x2E, on_forward_check_client},
    /* 6x2F */ {0x2B, 0x2D, 0x2F, on_change_hp<G_ChangePlayerHP_6x2F>},
    /* 6x30 */ {0x2C, 0x2E, 0x30, on_level_up},
    /* 6x31 */ {0x2D, 0x2F, 0x31, on_forward_check_game},
    /* 6x32 */ {NONE, NONE, 0x32, on_forward_check_game},
    /* 6x33 */ {0x2E, 0x30, 0x33, on_forward_check_game},
    /* 6x34 */ {0x2F, 0x31, 0x34, on_forward_check_game},
    /* 6x35 */ {0x30, 0x32, 0x35, on_invalid},
    /* 6x36 */ {NONE, NONE, 0x36, on_forward_check_game},
    /* 6x37 */ {0x32, 0x33, 0x37, on_forward_check_game},
    /* 6x38 */ {0x33, 0x34, 0x38, on_forward_check_game},
    /* 6x39 */ {NONE, 0x36, 0x39, on_forward_check_game},
    /* 6x3A */ {NONE, 0x37, 0x3A, on_forward_check_game},
    /* 6x3B */ {NONE, 0x38, 0x3B, forward_subcommand_m},
    /* 6x3C */ {0x34, 0x39, 0x3C, forward_subcommand_m},
    /* 6x3D */ {NONE, NONE, 0x3D, on_invalid},
    /* 6x3E */ {NONE, NONE, 0x3E, on_movement_with_floor<G_StopAtPosition_6x3E>},
    /* 6x3F */ {0x36, 0x3B, 0x3F, on_movement_with_floor<G_SetPosition_6x3F>},
    /* 6x40 */ {0x37, 0x3C, 0x40, on_movement<G_WalkToPosition_6x40>},
    /* 6x41 */ {0x38, 0x3D, 0x41, on_movement<G_MoveToPosition_6x41_6x42>},
    /* 6x42 */ {0x39, 0x3E, 0x42, on_movement<G_MoveToPosition_6x41_6x42>},
    /* 6x43 */ {0x3A, 0x3F, 0x43, on_forward_check_game_client},
    /* 6x44 */ {0x3B, 0x40, 0x44, on_forward_check_game_client},
    /* 6x45 */ {0x3C, 0x41, 0x45, on_forward_check_game_client},
    /* 6x46 */ {NONE, 0x42, 0x46, on_attack_finished},
    /* 6x47 */ {0x3D, 0x43, 0x47, on_cast_technique},
    /* 6x48 */ {NONE, NONE, 0x48, on_cast_technique_finished},
    /* 6x49 */ {0x3E, 0x44, 0x49, on_execute_photon_blast},
    /* 6x4A */ {0x3F, 0x45, 0x4A, on_forward_check_game_client},
    /* 6x4B */ {0x40, 0x46, 0x4B, on_change_hp<G_ClientIDHeader>},
    /* 6x4C */ {0x41, 0x47, 0x4C, on_change_hp<G_ClientIDHeader>},
    /* 6x4D */ {0x42, 0x48, 0x4D, on_player_died},
    /* 6x4E */ {NONE, NONE, 0x4E, on_player_revivable},
    /* 6x4F */ {0x43, 0x49, 0x4F, on_player_revived},
    /* 6x50 */ {0x44, 0x4A, 0x50, on_forward_check_game_client},
    /* 6x51 */ {NONE, NONE, 0x51, on_invalid},
    /* 6x52 */ {0x46, 0x4C, 0x52, on_set_animation_state},
    /* 6x53 */ {0x47, 0x4D, 0x53, on_forward_check_game},
    /* 6x54 */ {0x48, 0x4E, 0x54, forward_subcommand_m},
    /* 6x55 */ {0x49, 0x4F, 0x55, on_forward_check_game_client},
    /* 6x56 */ {0x4A, 0x50, 0x56, on_movement<G_SetPlayerPositionAndAngle_6x56>},
    /* 6x57 */ {NONE, 0x51, 0x57, on_forward_check_client},
    /* 6x58 */ {NONE, NONE, 0x58, on_forward_check_client},
    /* 6x59 */ {0x4B, 0x52, 0x59, on_pick_up_item},
    /* 6x5A */ {0x4C, 0x53, 0x5A, on_pick_up_item_request},
    /* 6x5B */ {0x4D, 0x54, 0x5B, forward_subcommand_m},
    /* 6x5C */ {0x4E, 0x55, 0x5C, on_destroy_floor_item},
    /* 6x5D */ {0x4F, 0x56, 0x5D, on_drop_partial_stack},
    /* 6x5E */ {0x50, 0x57, 0x5E, on_buy_shop_item},
    /* 6x5F */ {0x51, 0x58, 0x5F, on_box_or_enemy_item_drop},
    /* 6x60 */ {0x52, 0x59, 0x60, on_entity_drop_item_request},
    /* 6x61 */ {0x53, 0x5A, 0x61, on_forward_check_game},
    /* 6x62 */ {0x54, 0x5B, 0x62, on_forward_check_game},
    /* 6x63 */ {0x55, 0x5C, 0x63, on_destroy_floor_item},
    /* 6x64 */ {0x56, 0x5D, 0x64, on_forward_check_game},
    /* 6x65 */ {0x57, 0x5E, 0x65, on_forward_check_game},
    /* 6x66 */ {NONE, NONE, 0x66, on_forward_check_game},
    /* 6x67 */ {0x58, 0x5F, 0x67, on_trigger_set_event},
    /* 6x68 */ {0x59, 0x60, 0x68, on_update_telepipe_state},
    /* 6x69 */ {0x5A, 0x61, 0x69, on_npc_control},
    /* 6x6A */ {0x5B, 0x62, 0x6A, forward_subcommand_with_entity_id_transcode_t<G_SetBossWarpFlags_6x6A>},
    /* 6x6B */ {0x5C, 0x63, 0x6B, on_sync_joining_player_compressed_state},
    /* 6x6C */ {0x5D, 0x64, 0x6C, on_sync_joining_player_compressed_state},
    /* 6x6D */ {0x5E, 0x65, 0x6D, on_sync_joining_player_compressed_state},
    /* 6x6E */ {0x5F, 0x66, 0x6E, on_sync_joining_player_compressed_state},
    /* 6x6F */ {NONE, NONE, 0x6F, on_sync_joining_player_quest_flags},
    /* 6x70 */ {0x60, 0x67, 0x70, on_sync_joining_player_disp_and_inventory},
    /* 6x71 */ {NONE, NONE, 0x71, on_forward_check_game_loading},
    /* 6x72 */ {0x61, 0x68, 0x72, on_forward_check_game_loading},
    /* 6x73 */ {NONE, NONE, 0x73, on_forward_check_game_quest},
    /* 6x74 */ {0x62, 0x69, 0x74, on_word_select, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6x75 */ {NONE, NONE, 0x75, on_set_quest_flag},
    /* 6x76 */ {NONE, NONE, 0x76, on_set_entity_set_flag},
    /* 6x77 */ {NONE, NONE, 0x77, on_sync_quest_register},
    /* 6x78 */ {NONE, NONE, 0x78, forward_subcommand_m},
    /* 6x79 */ {NONE, NONE, 0x79, on_forward_check_lobby},
    /* 6x7A */ {NONE, NONE, 0x7A, on_forward_check_game_client},
    /* 6x7B */ {NONE, NONE, 0x7B, forward_subcommand_m},
    /* 6x7C */ {NONE, NONE, 0x7C, on_challenge_update_records},
    /* 6x7D */ {NONE, NONE, 0x7D, on_forward_check_game},
    /* 6x7E */ {NONE, NONE, 0x7E, forward_subcommand_m},
    /* 6x7F */ {NONE, NONE, 0x7F, on_battle_scores},
    /* 6x80 */ {NONE, NONE, 0x80, on_forward_check_game},
    /* 6x81 */ {NONE, NONE, 0x81, on_forward_check_game},
    /* 6x82 */ {NONE, NONE, 0x82, on_forward_check_game},
    /* 6x83 */ {NONE, NONE, 0x83, on_forward_check_game},
    /* 6x84 */ {NONE, NONE, 0x84, on_forward_check_game},
    /* 6x85 */ {NONE, NONE, 0x85, on_forward_check_game},
    /* 6x86 */ {NONE, NONE, 0x86, forward_subcommand_with_entity_id_transcode_t<G_HitDestructibleObject_6x86>},
    /* 6x87 */ {NONE, NONE, 0x87, on_forward_check_game},
    /* 6x88 */ {NONE, NONE, 0x88, on_forward_check_game},
    /* 6x89 */ {NONE, NONE, 0x89, forward_subcommand_with_entity_id_transcode_t<G_SetKillerEntityID_6x89, false, offsetof(G_SetKillerEntityID_6x89, killer_entity_id)>},
    /* 6x8A */ {NONE, NONE, 0x8A, on_forward_check_game},
    /* 6x8B */ {NONE, NONE, 0x8B, on_forward_check_game},
    /* 6x8C */ {NONE, NONE, 0x8C, on_forward_check_game},
    /* 6x8D */ {NONE, NONE, 0x8D, on_forward_check_game_client},
    /* 6x8E */ {NONE, NONE, 0x8E, on_forward_check_game},
    /* 6x8F */ {NONE, NONE, 0x8F, forward_subcommand_with_entity_id_transcode_t<G_AddBattleDamageScores_6x8F, false, offsetof(G_AddBattleDamageScores_6x8F, target_entity_id)>},
    /* 6x90 */ {NONE, NONE, 0x90, on_forward_check_game},
    /* 6x91 */ {NONE, NONE, 0x91, on_update_attackable_col_state},
    /* 6x92 */ {NONE, NONE, 0x92, on_forward_check_game},
    /* 6x93 */ {NONE, NONE, 0x93, on_activate_timed_switch},
    /* 6x94 */ {NONE, NONE, 0x94, on_warp},
    /* 6x95 */ {NONE, NONE, 0x95, on_forward_check_game},
    /* 6x96 */ {NONE, NONE, 0x96, on_forward_check_game},
    /* 6x97 */ {NONE, NONE, 0x97, on_challenge_mode_retry_or_quit},
    /* 6x98 */ {NONE, NONE, 0x98, on_forward_check_game},
    /* 6x99 */ {NONE, NONE, 0x99, on_forward_check_game},
    /* 6x9A */ {NONE, NONE, 0x9A, on_forward_check_game_client},
    /* 6x9B */ {NONE, NONE, 0x9B, on_forward_check_game},
    /* 6x9C */ {NONE, NONE, 0x9C, forward_subcommand_with_entity_id_transcode_t<G_Unknown_6x9C>},
    /* 6x9D */ {NONE, NONE, 0x9D, on_forward_check_game},
    /* 6x9E */ {NONE, NONE, 0x9E, forward_subcommand_m},
    /* 6x9F */ {NONE, NONE, 0x9F, forward_subcommand_with_entity_id_transcode_t<G_GalGryphonBossActions_6x9F>},
    /* 6xA0 */ {NONE, NONE, 0xA0, forward_subcommand_with_entity_id_transcode_t<G_GalGryphonBossActions_6xA0>},
    /* 6xA1 */ {NONE, NONE, 0xA1, on_forward_check_game},
    /* 6xA2 */ {NONE, NONE, 0xA2, on_entity_drop_item_request},
    /* 6xA3 */ {NONE, NONE, 0xA3, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowBossActions_6xA3>},
    /* 6xA4 */ {NONE, NONE, 0xA4, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowPhase1BossActions_6xA4>},
    /* 6xA5 */ {NONE, NONE, 0xA5, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowPhase2BossActions_6xA5>},
    /* 6xA6 */ {NONE, NONE, 0xA6, on_forward_check_game},
    /* 6xA7 */ {NONE, NONE, 0xA7, forward_subcommand_m},
    /* 6xA8 */ {NONE, NONE, 0xA8, on_gol_dragon_actions},
    /* 6xA9 */ {NONE, NONE, 0xA9, forward_subcommand_with_entity_id_transcode_t<G_BarbaRayBossActions_6xA9>},
    /* 6xAA */ {NONE, NONE, 0xAA, forward_subcommand_with_entity_id_transcode_t<G_BarbaRayBossActions_6xAA>},
    /* 6xAB */ {NONE, NONE, 0xAB, on_gc_nte_exclusive},
    /* 6xAC */ {NONE, NONE, 0xAC, on_gc_nte_exclusive},
    /* 6xAD */ {NONE, NONE, 0xAD, on_forward_check_game},
    /* 6xAE */ {NONE, NONE, 0xAE, on_forward_check_client},
    /* 6xAF */ {NONE, NONE, 0xAF, on_forward_check_lobby_client},
    /* 6xB0 */ {NONE, NONE, 0xB0, on_forward_check_lobby_client},
    /* 6xB1 */ {NONE, NONE, 0xB1, forward_subcommand_m},
    /* 6xB2 */ {NONE, NONE, 0xB2, on_play_sound_from_player},
    /* 6xB3 */ {NONE, NONE, 0xB3, on_xbox_voice_chat_control}, // Ep3 6xBx commands are handled via on_CA_Ep3 instead
    /* 6xB4 */ {NONE, NONE, 0xB4, on_xbox_voice_chat_control},
    /* 6xB5 */ {NONE, NONE, 0xB5, on_open_shop_bb_or_ep3_battle_subs},
    /* 6xB6 */ {NONE, NONE, 0xB6, on_invalid},
    /* 6xB7 */ {NONE, NONE, 0xB7, on_buy_shop_item_bb},
    /* 6xB8 */ {NONE, NONE, 0xB8, on_identify_item_bb},
    /* 6xB9 */ {NONE, NONE, 0xB9, on_invalid},
    /* 6xBA */ {NONE, NONE, 0xBA, on_accept_identify_item_bb},
    /* 6xBB */ {NONE, NONE, 0xBB, on_open_bank_bb_or_card_trade_counter_ep3},
    /* 6xBC */ {NONE, NONE, 0xBC, on_ep3_trade_card_counts},
    /* 6xBD */ {NONE, NONE, 0xBD, on_ep3_private_word_select_bb_bank_action, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6xBE */ {NONE, NONE, 0xBE, forward_subcommand_m, SDF::ALWAYS_FORWARD_TO_WATCHERS | SDF::ALLOW_FORWARD_TO_WATCHED_LOBBY},
    /* 6xBF */ {NONE, NONE, 0xBF, on_forward_check_ep3_lobby},
    /* 6xC0 */ {NONE, NONE, 0xC0, on_sell_item_at_shop_bb},
    /* 6xC1 */ {NONE, NONE, 0xC1, forward_subcommand_m},
    /* 6xC2 */ {NONE, NONE, 0xC2, forward_subcommand_m},
    /* 6xC3 */ {NONE, NONE, 0xC3, on_drop_partial_stack_bb},
    /* 6xC4 */ {NONE, NONE, 0xC4, on_sort_inventory_bb},
    /* 6xC5 */ {NONE, NONE, 0xC5, on_medical_center_bb},
    /* 6xC6 */ {NONE, NONE, 0xC6, on_steal_exp_bb},
    /* 6xC7 */ {NONE, NONE, 0xC7, on_charge_attack_bb},
    /* 6xC8 */ {NONE, NONE, 0xC8, on_enemy_exp_request_bb},
    /* 6xC9 */ {NONE, NONE, 0xC9, on_adjust_player_meseta_bb},
    /* 6xCA */ {NONE, NONE, 0xCA, on_item_reward_request_bb},
    /* 6xCB */ {NONE, NONE, 0xCB, on_transfer_item_via_mail_message_bb},
    /* 6xCC */ {NONE, NONE, 0xCC, on_exchange_item_for_team_points_bb},
    /* 6xCD */ {NONE, NONE, 0xCD, forward_subcommand_m},
    /* 6xCE */ {NONE, NONE, 0xCE, forward_subcommand_m},
    /* 6xCF */ {NONE, NONE, 0xCF, on_battle_restart_bb},
    /* 6xD0 */ {NONE, NONE, 0xD0, on_battle_level_up_bb},
    /* 6xD1 */ {NONE, NONE, 0xD1, on_request_challenge_grave_recovery_item_bb},
    /* 6xD2 */ {NONE, NONE, 0xD2, on_write_quest_counter_bb},
    /* 6xD3 */ {NONE, NONE, 0xD3, on_invalid},
    /* 6xD4 */ {NONE, NONE, 0xD4, on_forward_check_game},
    /* 6xD5 */ {NONE, NONE, 0xD5, on_quest_exchange_item_bb},
    /* 6xD6 */ {NONE, NONE, 0xD6, on_wrap_item_bb},
    /* 6xD7 */ {NONE, NONE, 0xD7, on_photon_drop_exchange_for_item_bb},
    /* 6xD8 */ {NONE, NONE, 0xD8, on_photon_drop_exchange_for_s_rank_special_bb},
    /* 6xD9 */ {NONE, NONE, 0xD9, on_momoka_item_exchange_bb},
    /* 6xDA */ {NONE, NONE, 0xDA, on_upgrade_weapon_attribute_bb},
    /* 6xDB */ {NONE, NONE, 0xDB, on_invalid},
    /* 6xDC */ {NONE, NONE, 0xDC, on_forward_check_game},
    /* 6xDD */ {NONE, NONE, 0xDD, on_invalid},
    /* 6xDE */ {NONE, NONE, 0xDE, on_secret_lottery_ticket_exchange_bb},
    /* 6xDF */ {NONE, NONE, 0xDF, on_photon_crystal_exchange_bb},
    /* 6xE0 */ {NONE, NONE, 0xE0, on_quest_F95E_result_bb},
    /* 6xE1 */ {NONE, NONE, 0xE1, on_quest_F95F_result_bb},
    /* 6xE2 */ {NONE, NONE, 0xE2, on_quest_F960_result_bb},
    /* 6xE3 */ {NONE, NONE, 0xE3, on_invalid},
    /* 6xE4 */ {NONE, NONE, 0xE4, on_invalid},
    /* 6xE5 */ {NONE, NONE, 0xE5, on_invalid},
    /* 6xE6 */ {NONE, NONE, 0xE6, on_invalid},
    /* 6xE7 */ {NONE, NONE, 0xE7, on_invalid},
    /* 6xE8 */ {NONE, NONE, 0xE8, on_invalid},
    /* 6xE9 */ {NONE, NONE, 0xE9, on_invalid},
    /* 6xEA */ {NONE, NONE, 0xEA, on_invalid},
    /* 6xEB */ {NONE, NONE, 0xEB, on_invalid},
    /* 6xEC */ {NONE, NONE, 0xEC, on_invalid},
    /* 6xED */ {NONE, NONE, 0xED, on_invalid},
    /* 6xEE */ {NONE, NONE, 0xEE, on_invalid},
    /* 6xEF */ {NONE, NONE, 0xEF, on_invalid},
    /* 6xF0 */ {NONE, NONE, 0xF0, on_invalid},
    /* 6xF1 */ {NONE, NONE, 0xF1, on_invalid},
    /* 6xF2 */ {NONE, NONE, 0xF2, on_invalid},
    /* 6xF3 */ {NONE, NONE, 0xF3, on_invalid},
    /* 6xF4 */ {NONE, NONE, 0xF4, on_invalid},
    /* 6xF5 */ {NONE, NONE, 0xF5, on_invalid},
    /* 6xF6 */ {NONE, NONE, 0xF6, on_invalid},
    /* 6xF7 */ {NONE, NONE, 0xF7, on_invalid},
    /* 6xF8 */ {NONE, NONE, 0xF8, on_invalid},
    /* 6xF9 */ {NONE, NONE, 0xF9, on_invalid},
    /* 6xFA */ {NONE, NONE, 0xFA, on_invalid},
    /* 6xFB */ {NONE, NONE, 0xFB, on_invalid},
    /* 6xFC */ {NONE, NONE, 0xFC, on_invalid},
    /* 6xFD */ {NONE, NONE, 0xFD, on_invalid},
    /* 6xFE */ {NONE, NONE, 0xFE, on_invalid},
    /* 6xFF */ {NONE, NONE, 0xFF, on_invalid},
};

void on_subcommand_multi(shared_ptr<Client> c, uint8_t command, uint8_t flag, string& data) {
  if (data.empty()) {
    throw runtime_error("game command is empty");
  }

  size_t offset = 0;
  while (offset < data.size()) {
    size_t cmd_size = 0;
    if (offset + sizeof(G_UnusedHeader) > data.size()) {
      throw runtime_error("insufficient data remaining for next subcommand header");
    }
    const auto* header = reinterpret_cast<const G_UnusedHeader*>(data.data() + offset);
    if (header->size != 0) {
      cmd_size = header->size << 2;
    } else {
      if (offset + sizeof(G_ExtendedHeaderT<G_UnusedHeader>) > data.size()) {
        throw runtime_error("insufficient data remaining for next extended subcommand header");
      }
      const auto* ext_header = reinterpret_cast<const G_ExtendedHeaderT<G_UnusedHeader>*>(data.data() + offset);
      cmd_size = ext_header->size;
      if (cmd_size < 8) {
        throw runtime_error("extended subcommand header has size < 8");
      }
      if (cmd_size & 3) {
        throw runtime_error("extended subcommand size is not a multiple of 4");
      }
    }
    if (cmd_size == 0) {
      throw runtime_error("invalid subcommand size");
    }
    void* cmd_data = data.data() + offset;

    const auto* def = def_for_subcommand(c->version(), header->subcommand);
    if (def && def->handler) {
      def->handler(c, command, flag, cmd_data, cmd_size);
    } else {
      on_unimplemented(c, command, flag, cmd_data, cmd_size);
    }
    offset += cmd_size;
  }
}
