#include "ReceiveSubcommands.hh"

#include <math.h>
#include <string.h>

#include <memory>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Vector.hh>

#include "Client.hh"
#include "Compression.hh"
#include "GameServer.hh"
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

// The functions in this file are called when a client sends a game command (60, 62, 6C, 6D, C9, or CB).

struct SubcommandMessage {
  uint16_t command;
  uint32_t flag;
  void* data;
  size_t size;

  template <typename T>
  const T& check_size_t(size_t min_size, size_t max_size) const {
    return ::check_size_t<const T>(this->data, this->size, min_size, max_size);
  }
  template <typename T>
  T& check_size_t(size_t min_size, size_t max_size) {
    return ::check_size_t<T>(this->data, this->size, min_size, max_size);
  }

  template <typename T>
  const T& check_size_t(size_t max_size) const {
    return ::check_size_t<const T>(this->data, this->size, sizeof(T), max_size);
  }
  template <typename T>
  T& check_size_t(size_t max_size) {
    return ::check_size_t<T>(this->data, this->size, sizeof(T), max_size);
  }

  template <typename T>
  const T& check_size_t() const {
    return ::check_size_t<const T>(this->data, this->size, sizeof(T), sizeof(T));
  }
  template <typename T>
  T& check_size_t() {
    return ::check_size_t<T>(this->data, this->size, sizeof(T), sizeof(T));
  }
};

using SubcommandHandler = asio::awaitable<void> (*)(shared_ptr<Client> c, SubcommandMessage& msg);

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

extern const vector<SubcommandDefinition> subcommand_definitions;

const SubcommandDefinition* def_for_subcommand(Version version, uint8_t subcommand) {
  static bool populated = false;
  static std::array<const SubcommandDefinition*, 0x100> nte_defs;
  static std::array<const SubcommandDefinition*, 0x100> proto_defs;
  static std::array<const SubcommandDefinition*, 0x100> final_defs;
  if (!populated) {
    nte_defs.fill(nullptr);
    proto_defs.fill(nullptr);
    final_defs.fill(nullptr);
    for (const auto& def : subcommand_definitions) {
      if (def.nte_subcommand != 0x00) {
        if (nte_defs[def.nte_subcommand]) {
          throw logic_error("multiple subcommand definitions map to the same NTE subcommand");
        }
        nte_defs[def.nte_subcommand] = &def;
      }
      if (def.proto_subcommand != 0x00) {
        if (proto_defs[def.proto_subcommand]) {
          throw logic_error("multiple subcommand definitions map to the same 11/2000 subcommand");
        }
        proto_defs[def.proto_subcommand] = &def;
      }
      if (def.final_subcommand != 0x00) {
        if (final_defs[def.final_subcommand]) {
          throw logic_error("multiple subcommand definitions map to the same final subcommand");
        }
        final_defs[def.final_subcommand] = &def;
      }
    }
    populated = true;
  }

  if (version == Version::DC_NTE) {
    return nte_defs[subcommand];
  } else if (version == Version::DC_11_2000) {
    return proto_defs[subcommand];
  } else {
    return final_defs[subcommand];
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

bool command_is_private(uint8_t command) {
  return (command == 0x62) || (command == 0x6D);
}

static void forward_subcommand(shared_ptr<Client> c, SubcommandMessage& msg) {
  // If the command is an Ep3-only command, make sure an Ep3 client sent it
  bool command_is_ep3 = (msg.command & 0xF0) == 0xC0;
  if (command_is_ep3 && !is_ep3(c->version())) {
    throw runtime_error("Episode 3 command sent by non-Episode 3 client");
  }

  auto l = c->lobby.lock();
  if (!l) {
    c->log.warning_f("Not in any lobby; dropping command");
    return;
  }

  auto& header = msg.check_size_t<G_UnusedHeader>(0xFFFF);
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
      data_to_send = msg.data;
      size_to_send = msg.size;
    } else if (lc->version() == Version::DC_NTE) {
      if (def && def->nte_subcommand) {
        if (nte_data.empty()) {
          nte_data.assign(reinterpret_cast<const char*>(msg.data), msg.size);
          nte_data[0] = def->nte_subcommand;
        }
        data_to_send = nte_data.data();
        size_to_send = nte_data.size();
      }
    } else if (lc->version() == Version::DC_11_2000) {
      if (def && def->proto_subcommand) {
        if (proto_data.empty()) {
          proto_data.assign(reinterpret_cast<const char*>(msg.data), msg.size);
          proto_data[0] = def->proto_subcommand;
        }
        data_to_send = proto_data.data();
        size_to_send = proto_data.size();
      }
    } else {
      if (def && def->final_subcommand) {
        if (final_data.empty()) {
          final_data.assign(reinterpret_cast<const char*>(msg.data), msg.size);
          final_data[0] = def->final_subcommand;
        }
        data_to_send = final_data.data();
        size_to_send = final_data.size();
      }
    }

    if (!data_to_send || !size_to_send) {
      lc->log.info_f("Command cannot be translated to client\'s version");
    } else {
      uint16_t command = msg.command;
      if ((command == 0xCB) && (lc->version() == Version::GC_EP3_NTE)) {
        command = 0xC9;
      }
      if (lc->game_join_command_queue) {
        lc->log.info_f("Client not ready to receive join commands; adding to queue");
        auto& cmd = lc->game_join_command_queue->emplace_back();
        cmd.command = command;
        cmd.flag = msg.flag;
        cmd.data.assign(reinterpret_cast<const char*>(data_to_send), size_to_send);
      } else {
        send_command(lc, command, msg.flag, data_to_send, size_to_send);
      }
    }
  };

  if (command_is_private(msg.command)) {
    if (msg.flag >= l->max_clients) {
      return;
    }
    auto target = l->clients[msg.flag];
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
      if ((msg.command == 0xCB) &&
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

    // Before battle, forward only chat commands to watcher lobbies; during battle, forward everything to watcher
    // lobbies. (This is necessary because if we forward everything before battle, the blocking menu subcommands cause
    // the battle setup menu to appear in the spectator room, which looks weird and is generally undesirable.)
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
      auto type = ((msg.command & 0xF0) == 0xC0)
          ? Episode3::BattleRecord::Event::Type::EP3_GAME_COMMAND
          : Episode3::BattleRecord::Event::Type::GAME_COMMAND;
      l->battle_record->add_command(type, msg.data, msg.size);
    }
  }
}

static asio::awaitable<void> forward_subcommand_m(shared_ptr<Client> c, SubcommandMessage& msg) {
  forward_subcommand(c, msg);
  co_return;
}

template <typename CmdT>
static void forward_subcommand_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& cmd) {
  forward_subcommand(c, command, flag, &cmd, sizeof(cmd));
}

static asio::awaitable<void> on_invalid(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_UnusedHeader>(0xFFFF);
  if ((c->version() == Version::DC_NTE) || c->version() == Version::DC_11_2000) {
    c->log.error_f("Unrecognized DC NTE/prototype subcommand: {:02X}", cmd.subcommand);
    forward_subcommand(c, msg);
  } else if (command_is_private(msg.command)) {
    c->log.error_f("Invalid subcommand: {:02X} (private to {})", cmd.subcommand, msg.flag);
  } else {
    c->log.error_f("Invalid subcommand: {:02X} (public)", cmd.subcommand);
  }
  co_return;
}

static asio::awaitable<void> on_debug_info(shared_ptr<Client>, SubcommandMessage&) {
  co_return;
}

static asio::awaitable<void> on_forward_check_game_loading(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (l->is_game() && l->any_client_loading()) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_game_quest(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (l->is_game() && l->quest) {
    forward_subcommand(c, msg);
  }
  co_return;
}

template <typename CmdT>
void forward_subcommand_with_item_transcode_t(shared_ptr<Client> c, uint8_t command, uint8_t flag, const CmdT& cmd) {
  // I'm lazy and this should never happen for item commands (since all players need to stay in sync)
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
        lc->log.info_f("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command_t(lc, command, flag, cmd);
    }
  }
}

template <typename CmdT, bool ForwardIfMissing = false, size_t EntityIDOffset = offsetof(G_EntityIDHeader, entity_id)>
asio::awaitable<void> forward_subcommand_with_entity_id_transcode_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  // I'm lazy and this should never happen for item commands (since all players need to stay in sync)
  if (command_is_private(msg.command)) {
    throw runtime_error("entity subcommand sent via private command");
  }

  auto& cmd = msg.check_size_t<CmdT>();

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("command cannot be used outside of a game");
  }

  le_uint16_t& cmd_entity_id = *reinterpret_cast<le_uint16_t*>(reinterpret_cast<uint8_t*>(&cmd) + EntityIDOffset);

  shared_ptr<const MapState::EnemyState> ene_st;
  shared_ptr<const MapState::ObjectState> obj_st;
  if ((cmd_entity_id >= 0x1000) && (cmd_entity_id < 0x4000)) {
    ene_st = l->map_state->enemy_state_for_index(c->version(), cmd_entity_id - 0x1000);
  } else if ((cmd_entity_id >= 0x4000) && (cmd_entity_id < 0xFFFF)) {
    obj_st = l->map_state->object_state_for_index(c->version(), cmd_entity_id - 0x4000);
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
          send_command_t(lc, msg.command, msg.flag, cmd);
        }
      } else {
        lc->log.info_f("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command_t(lc, msg.command, msg.flag, cmd);
    }
  }
  co_return;
}

template <typename HeaderT>
asio::awaitable<void> forward_subcommand_with_entity_targets_transcode_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  // I'm lazy and this should never happen for item commands (since all players need to stay in sync)
  if (command_is_private(msg.command)) {
    throw runtime_error("entity subcommand sent via private command");
  }

  phosg::StringReader r(msg.data, msg.size);
  const auto& header = r.get<HeaderT>();
  if (header.target_count > 10) {
    throw runtime_error("invalid target count");
  }
  if (header.target_count > std::min<size_t>(header.header.size - sizeof(HeaderT) / 4, 10)) {
    throw runtime_error("invalid target list command");
  }
  const auto* targets = r.get_array<TargetEntry>(header.target_count);

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
  for (size_t z = 0; z < header.target_count; z++) {
    auto& res = resolutions.emplace_back(TargetResolution{nullptr, nullptr, targets[z].entity_id});
    if ((res.entity_id >= 0x1000) && (res.entity_id < 0x4000)) {
      res.ene_st = l->map_state->enemy_state_for_index(c->version(), res.entity_id - 0x1000);
    } else if ((res.entity_id >= 0x4000) && (res.entity_id < 0xFFFF)) {
      res.obj_st = l->map_state->object_state_for_index(c->version(), res.entity_id - 0x4000);
    }
  }

  for (auto& lc : l->clients) {
    if (!lc || lc == c) {
      continue;
    }
    if (c->version() != lc->version()) {
      HeaderT out_header = header;
      vector<TargetEntry> out_targets;
      out_header.header.subcommand = translate_subcommand_number(lc->version(), c->version(), header.header.subcommand);
      out_header.target_count = 0;
      if (out_header.header.subcommand) {
        for (size_t z = 0; z < header.target_count; z++) {
          uint16_t entity_id;
          const auto& res = resolutions[z];
          if (res.ene_st) {
            entity_id = 0x1000 | l->map_state->index_for_enemy_state(lc->version(), res.ene_st);
          } else if (res.obj_st) {
            entity_id = 0x4000 | l->map_state->index_for_object_state(lc->version(), res.obj_st);
          } else {
            entity_id = res.entity_id;
          }
          if (entity_id != 0xFFFF) {
            out_targets.emplace_back(TargetEntry{entity_id, targets[z].unknown_a2});
          }
        }
        size_t out_size = sizeof(HeaderT) + sizeof(TargetEntry) * out_targets.size();
        out_header.header.size = out_size >> 2;
        out_header.target_count = out_targets.size();
        send_command_t_vt(lc, msg.command, msg.flag, out_header, out_targets);
      } else {
        lc->log.info_f("Subcommand cannot be translated to client\'s version");
      }
    } else {
      send_command(lc, msg.command, msg.flag, msg.data, msg.size);
    }
  }
  co_return;
}

static shared_ptr<Client> get_sync_target(
    shared_ptr<Client> sender_c, uint8_t command, uint8_t flag, bool allow_if_not_loading) {
  if (!command_is_private(command)) {
    throw runtime_error("sync data sent via public command");
  }
  auto l = sender_c->require_lobby();
  if (l->is_game() && (allow_if_not_loading || l->any_client_loading()) && (flag < l->max_clients)) {
    return l->clients[flag];
  }
  return nullptr;
}

static asio::awaitable<void> on_sync_joining_player_compressed_state(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto target = get_sync_target(c, msg.command, msg.flag, false); // Checks l->is_game
  if (!target) {
    co_return;
  }

  uint8_t orig_subcommand_number;
  size_t decompressed_size;
  size_t compressed_size;
  const void* compressed_data;
  if (is_pre_v1(c->version())) {
    const auto& cmd = msg.check_size_t<G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E>(0xFFFF);
    orig_subcommand_number = cmd.header.basic_header.subcommand;
    decompressed_size = cmd.decompressed_size;
    compressed_size = msg.size - sizeof(cmd);
    compressed_data = reinterpret_cast<const char*>(msg.data) + sizeof(cmd);
  } else {
    const auto& cmd = msg.check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(0xFFFF);
    if (cmd.compressed_size > msg.size - sizeof(cmd)) {
      throw runtime_error("compressed end offset is beyond end of command");
    }
    orig_subcommand_number = cmd.header.basic_header.subcommand;
    decompressed_size = cmd.decompressed_size;
    compressed_size = cmd.compressed_size;
    compressed_data = reinterpret_cast<const char*>(msg.data) + sizeof(cmd);
  }

  const auto* subcommand_def = def_for_subcommand(c->version(), orig_subcommand_number);
  if (!subcommand_def) {
    throw runtime_error("unknown sync subcommand");
  }

  string decompressed = bc0_decompress(compressed_data, compressed_size);
  if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info_f("Decompressed sync data ({:X} -> {:X} bytes; expected {:X}):",
        compressed_size, decompressed.size(), decompressed_size);
    phosg::print_data(stderr, decompressed);
  }

  // Assume all v1 and v2 versions are the same, and assume GC/XB are the same.
  // TODO: We should do this by checking if the supermaps are the same instead of hardcoding this here.
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
        throw runtime_error(std::format(
            "decompressed 6x6D data (0x{:X} bytes) is too short for header (0x{:X} bytes)",
            decompressed.size(), sizeof(G_SyncItemState_6x6D_Decompressed)));
      }
      auto* decompressed_cmd = reinterpret_cast<G_SyncItemState_6x6D_Decompressed*>(decompressed.data());

      size_t num_floor_items = 0;
      for (size_t z = 0; z < decompressed_cmd->floor_item_count_per_floor.size(); z++) {
        num_floor_items += decompressed_cmd->floor_item_count_per_floor[z];
      }

      size_t required_size = sizeof(G_SyncItemState_6x6D_Decompressed) + num_floor_items * sizeof(FloorItem);
      if (decompressed.size() < required_size) {
        throw runtime_error(std::format(
            "decompressed 6x6D data (0x{:X} bytes) is too short for all floor items (0x{:X} bytes)",
            decompressed.size(), required_size));
      }

      auto l = c->require_lobby();
      size_t target_num_items = target->character_file()->inventory.num_items;
      for (size_t z = 0; z < 12; z++) {
        uint32_t client_next_id = decompressed_cmd->next_item_id_per_player[z];
        uint32_t server_next_id = l->next_item_id_for_client[z];
        if (client_next_id == server_next_id) {
          l->log.info_f("Next item ID for player {} ({:08X}) matches expected value", z, l->next_item_id_for_client[z]);
        } else if ((z == target->lobby_client_id) && (client_next_id == server_next_id - target_num_items)) {
          l->log.info_f("Next item ID for player {} ({:08X}) matches expected value before inventory item ID assignment ({:08X})", z, l->next_item_id_for_client[z], static_cast<uint32_t>(server_next_id - target_num_items));
        } else {
          l->log.warning_f("Next item ID for player {} ({:08X}) does not match expected value ({:08X})",
              z, decompressed_cmd->next_item_id_per_player[z], l->next_item_id_for_client[z]);
        }
      }

      // The leader's item state is never forwarded since the leader may be able to see items that the joining player
      // should not see. We always generate a new item state for the joining player instead.
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

      if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
        c->log.info_f("Set flags data:");
        phosg::print_data(stderr, r.getv(dec_header.entity_set_flags_size, false), dec_header.entity_set_flags_size);
      }

      const auto* object_set_flags = &set_flags_r.get<le_uint16_t>(
          true, set_flags_header.num_object_sets * sizeof(le_uint16_t));
      const auto* enemy_set_flags = &set_flags_r.get<le_uint16_t>(
          true, set_flags_header.num_enemy_sets * sizeof(le_uint16_t));
      size_t event_set_flags_count = dec_header.event_set_flags_size / sizeof(le_uint16_t);
      const auto* event_set_flags = &r.pget<le_uint16_t>(
          r.where() + dec_header.entity_set_flags_size, event_set_flags_count * sizeof(le_uint16_t));
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
        l->log.warning_f("Switch flags size (0x{:X}) does not match expected size (0x{:X})",
            dec_header.switch_flags_size, expected_switch_flags_size);
      } else {
        l->log.info_f("Switch flags size matches expected size (0x{:X})", expected_switch_flags_size);
      }
      if (l->switch_flags) {
        phosg::StringReader switch_flags_r = r.sub(r.where() + dec_header.entity_set_flags_size + dec_header.event_set_flags_size);
        for (size_t floor = 0; floor < expected_switch_flag_num_floors; floor++) {
          // There is a bug in most (perhaps all) versions of the game, which causes this array to be too small. It
          // looks like Sega forgot to account for the header (G_SyncSetFlagState_6x6E_Decompressed) before compressing
          // the buffer, so the game cuts off the last 8 bytes of the switch flags. Since this only affects the last
          // floor, which rarely has any switches on it (or is even accessible by the player), it's not surprising that
          // no one noticed this. But it does mean we have to check switch_flags_r.eof() here.
          for (size_t z = 0; (z < 0x20) && !switch_flags_r.eof(); z++) {
            uint8_t& l_flags = l->switch_flags->array(floor).data[z];
            uint8_t r_flags = switch_flags_r.get_u8();
            if (l_flags != r_flags) {
              l->log.warning_f(
                  "Switch flags do not match at floor {:02X} byte {:02X} (expected {:02X}, received {:02X})",
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
static asio::awaitable<void> on_sync_joining_player_quest_flags_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();

  if (!command_is_private(msg.command)) {
    co_return;
  }

  auto l = c->require_lobby();
  if (l->is_game() && l->any_client_loading() && (l->leader_id == c->lobby_client_id)) {
    l->quest_flags_known = nullptr; // All quest flags are now known
    l->quest_flag_values = make_unique<QuestFlags>(cmd.quest_flags);
    auto target = l->clients.at(msg.flag);
    if (target) {
      send_game_flag_state(target);
    }
  }
}

static asio::awaitable<void> on_sync_joining_player_quest_flags(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (is_v1(c->version())) {
    co_await on_sync_joining_player_quest_flags_t<G_SetQuestFlags_DCv1_6x6F>(c, msg);
  } else if (!is_v4(c->version())) {
    co_await on_sync_joining_player_quest_flags_t<G_SetQuestFlags_V2_V3_6x6F>(c, msg);
  } else {
    co_await on_sync_joining_player_quest_flags_t<G_SetQuestFlags_BB_6x6F>(c, msg);
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
      language(Language::JAPANESE),
      player_tag(0x00010000),
      guild_card_number(guild_card_number),
      unknown_a6(0),
      battle_team_number(0),
      telepipe(cmd.telepipe),
      death_flags(cmd.death_flags),
      hold_state(cmd.hold_state),
      area(cmd.area),
      game_flags(cmd.game_flags),
      game_flags_is_v3(false),
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
    Language language,
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
      death_flags(cmd.death_flags),
      hold_state(cmd.hold_state),
      area(cmd.area),
      game_flags(cmd.game_flags),
      game_flags_is_v3(false),
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
  this->game_flags_is_v3 = true;
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
  this->game_flags_is_v3 = true;
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
  this->game_flags_is_v3 = true;
  this->stats = cmd.stats;
  this->num_items = cmd.num_items;
  this->items = cmd.items;
  this->floor = cmd.floor;
  this->xb_user_id = this->default_xb_user_id();
  this->xb_unknown_a16 = cmd.unknown_a16;
  this->name = cmd.name.decode(this->language);
  this->visual.name.encode(this->name, this->language);
}

G_SyncPlayerDispAndInventory_DCNTE_6x70 Parsed6x70Data::as_dc_nte(shared_ptr<ServerState> s) const {
  G_SyncPlayerDispAndInventory_DCNTE_6x70 ret;
  ret.base = this->base;
  ret.unknown_a5 = this->unknown_a5_nte;
  ret.unknown_a6 = this->unknown_a6;
  ret.telepipe = this->telepipe;
  ret.death_flags = this->death_flags;
  ret.hold_state = this->hold_state;
  ret.area = this->area;
  ret.game_flags = this->get_game_flags(false);
  ret.visual = this->visual;
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;

  transcode_inventory_items(
      ret.items,
      ret.num_items,
      this->item_version,
      Version::DC_NTE,
      s->item_parameter_table_for_encode(Version::DC_NTE));
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
  ret.death_flags = this->death_flags;
  ret.hold_state = this->hold_state;
  ret.area = this->area;
  ret.game_flags = this->get_game_flags(false);
  ret.visual = this->visual;
  ret.stats = this->stats;
  ret.num_items = this->num_items;
  ret.items = this->items;

  transcode_inventory_items(
      ret.items,
      ret.num_items,
      this->item_version,
      Version::DC_11_2000,
      s->item_parameter_table_for_encode(Version::DC_11_2000));
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

G_SyncPlayerDispAndInventory_BB_6x70 Parsed6x70Data::as_bb(shared_ptr<ServerState> s, Language language) const {
  G_SyncPlayerDispAndInventory_BB_6x70 ret;
  ret.base = this->base_v1(true);
  ret.name.encode(this->name, language);
  ret.base.visual.name.encode(std::format("{:10}", this->guild_card_number), language);
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
      language(static_cast<Language>(base.language32.load())),
      player_tag(base.player_tag),
      guild_card_number(guild_card_number), // Ignore the client's GC#
      unknown_a6(base.unknown_a6),
      battle_team_number(base.battle_team_number),
      telepipe(base.telepipe),
      death_flags(base.death_flags),
      hold_state(base.hold_state),
      area(base.area),
      game_flags(base.game_flags),
      game_flags_is_v3(!is_v1_or_v2(from_version)),
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
  ret.language32 = static_cast<size_t>(this->language);
  ret.player_tag = this->player_tag;
  ret.guild_card_number = this->guild_card_number;
  ret.unknown_a6 = this->unknown_a6;
  ret.battle_team_number = this->battle_team_number;
  ret.telepipe = this->telepipe;
  ret.death_flags = this->death_flags;
  ret.hold_state = this->hold_state;
  ret.area = this->area;
  ret.game_flags = this->get_game_flags(is_v3);
  ret.technique_levels_v1 = this->technique_levels_v1;
  ret.visual = this->visual;
  return ret;
}

uint32_t Parsed6x70Data::convert_game_flags(uint32_t game_flags, bool to_v3) {
  // The format of game_flags for players was changed significantly between v2 and v3, and not accounting for this
  // results in odd effects like other characters not appearing when joining a game. Unfortunately, some bits were
  // deleted on v3 and other bits were added, so it doesn't suffice to simply store the most complete format of this
  // field - we have to be able to convert between the two.

  // Bits on v2: JIHCBAzy xwvutsrq ponmlkji hgfedcba
  // Bits on v3: JIHGFEDC BAzyxwvu srqponkj hgfedcba
  // The bits ilmt were removed in v3 and the bits to their left were shifted right. The bits DEFG were added in v3 and
  // do not exist on v2. Known meanings for these bits so far:
  //   o = is dead
  //   n = should play hit animation
  //   y = is near enemy
  //   H = is enemy?
  //   I = is object? (some entities have both H and I set though)
  //   J = is item

  if (to_v3) {
    return (game_flags & 0xE00000FF) |
        ((game_flags & 0x00000600) >> 1) |
        ((game_flags & 0x0007E000) >> 3) |
        ((game_flags & 0x1FF00000) >> 4);
  } else {
    return (game_flags & 0xE00000FF) |
        ((game_flags << 1) & 0x00000600) |
        ((game_flags << 3) & 0x0007E000) |
        ((game_flags << 4) & 0x1FF00000);
  }
}

uint32_t Parsed6x70Data::get_game_flags(bool is_v3) const {
  return (this->game_flags_is_v3 == is_v3)
      ? this->game_flags
      : Parsed6x70Data::convert_game_flags(this->game_flags, is_v3);
}

static asio::awaitable<void> on_sync_joining_player_disp_and_inventory(
    shared_ptr<Client> c, SubcommandMessage& msg) {
  auto s = c->require_server_state();

  // In V1/V2 games, this command sometimes is sent after the new client has finished loading, so we don't check
  // l->any_client_loading() here.
  auto target = get_sync_target(c, msg.command, msg.flag, true);
  if (!target) {
    co_return;
  }

  // If the sender is the leader and is pre-V1, and the target is V1 or later, we need to synthesize a 6x71 command to
  // tell the target all state has been sent. (If both are pre-V1, the target won't expect this command; if both are V1
  // or later, the leader will send this command itself.)
  Version target_v = target->version();
  Version c_v = c->version();
  if (is_pre_v1(c_v) && !is_pre_v1(target_v)) {
    static const be_uint32_t data = 0x71010000;
    send_command(target, 0x62, target->lobby_client_id, &data, sizeof(data));
  }

  bool is_client_customisation = c->check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION);
  switch (c_v) {
    case Version::DC_NTE:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          msg.check_size_t<G_SyncPlayerDispAndInventory_DCNTE_6x70>(),
          c->login->account->account_id, c_v, is_client_customisation));
      c->last_reported_6x70->clear_dc_protos_unused_item_fields();
      break;
    case Version::DC_11_2000:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          msg.check_size_t<G_SyncPlayerDispAndInventory_DC112000_6x70>(),
          c->login->account->account_id, c->language(), c_v, is_client_customisation));
      c->last_reported_6x70->clear_dc_protos_unused_item_fields();
      break;
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          msg.check_size_t<G_SyncPlayerDispAndInventory_DC_PC_6x70>(),
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
          msg.check_size_t<G_SyncPlayerDispAndInventory_GC_6x70>(),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    case Version::XB_V3:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          msg.check_size_t<G_SyncPlayerDispAndInventory_XB_6x70>(),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    case Version::BB_V4:
      c->last_reported_6x70.reset(new Parsed6x70Data(
          msg.check_size_t<G_SyncPlayerDispAndInventory_BB_6x70>(),
          c->login->account->account_id, c_v, is_client_customisation));
      break;
    default:
      throw logic_error("6x70 command from unknown game version");
  }

  c->pos = c->last_reported_6x70->base.pos;
  send_game_player_state(target, c, false);
}

static asio::awaitable<void> on_forward_check_client(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_ClientIDHeader>(0xFFFF);
  if (cmd.client_id == c->lobby_client_id) {
    forward_subcommand(c, msg);
  }
  co_return;
}

template <typename CmdT>
static asio::awaitable<void> on_forward_check_client_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();
  if (cmd.client_id == c->lobby_client_id) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_game(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_lobby(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_lobby_client(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_ClientIDHeader>(0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_game_client(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_ClientIDHeader>(0xFFFF);
  auto l = c->require_lobby();
  if (l->is_game() && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_forward_check_ep3_lobby(shared_ptr<Client> c, SubcommandMessage& msg) {
  msg.check_size_t<G_UnusedHeader>(0xFFFF);
  auto l = c->require_lobby();
  if (!l->is_game() && l->is_ep3()) {
    forward_subcommand(c, msg);
  }
  co_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ep3 subcommands

static asio::awaitable<void> on_ep3_battle_subs(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& header = msg.check_size_t<G_CardBattleCommandHeader>(0xFFFF);

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    co_return;
  }

  if (c->version() != Version::GC_EP3_NTE) {
    set_mask_for_ep3_game_command(msg.data, msg.size, 0);
  } else {
    // Ep3 NTE sends uninitialized data in this field; clear it so we know the command isn't masked
    msg.check_size_t<G_CardBattleCommandHeader>(0xFFFF).mask_key = 0;
  }

  if (header.subsubcommand == 0x1A) {
    co_return;
  } else if (header.subsubcommand == 0x20) {
    const auto& cmd = msg.check_size_t<G_Unknown_Ep3_6xB5x20>();
    if (cmd.client_id >= 12) {
      co_return;
    }
  } else if (header.subsubcommand == 0x31) {
    const auto& cmd = msg.check_size_t<G_ConfirmDeckSelection_Ep3_6xB5x31>();
    if (cmd.menu_type >= 0x15) {
      co_return;
    }
  } else if (header.subsubcommand == 0x32) {
    const auto& cmd = msg.check_size_t<G_MoveSharedMenuCursor_Ep3_6xB5x32>();
    if (cmd.menu_type >= 0x15) {
      co_return;
    }
  } else if (header.subsubcommand == 0x36) {
    const auto& cmd = msg.check_size_t<G_RecreatePlayer_Ep3_6xB5x36>();
    if (l->is_game() && (cmd.client_id >= 4)) {
      co_return;
    }
  } else if (header.subsubcommand == 0x38) {
    c->set_flag(Client::Flag::EP3_ALLOW_6xBC);
  } else if (header.subsubcommand == 0x3C) {
    c->clear_flag(Client::Flag::EP3_ALLOW_6xBC);
  }

  for (const auto& lc : l->clients) {
    if (!lc || (lc == c)) {
      continue;
    }
    if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING) && (lc->version() != Version::GC_EP3_NTE)) {
      set_mask_for_ep3_game_command(msg.data, msg.size, (phosg::random_object<uint32_t>() % 0xFF) + 1);
    }
    send_command(lc, 0xC9, 0x00, msg.data, msg.size);
  }
}

static asio::awaitable<void> on_ep3_trade_card_counts(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (c->version() == Version::GC_EP3_NTE) {
    msg.check_size_t<G_CardCounts_Ep3NTE_6xBC>(0xFFFF);
  } else {
    msg.check_size_t<G_CardCounts_Ep3_6xBC>(0xFFFF);
  }

  if (!command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    co_return;
  }
  auto target = l->clients.at(msg.flag);
  if (!target || !target->check_flag(Client::Flag::EP3_ALLOW_6xBC)) {
    co_return;
  }

  forward_subcommand(c, msg);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Chat commands and the like

static asio::awaitable<void> on_send_guild_card(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!command_is_private(msg.command) || (msg.flag >= l->max_clients) || (!l->clients[msg.flag])) {
    co_return;
  }

  switch (c->version()) {
    case Version::DC_NTE: {
      const auto& cmd = msg.check_size_t<G_SendGuildCard_DCNTE_6x06>();
      c->character_file(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2: {
      const auto& cmd = msg.check_size_t<G_SendGuildCard_DC_6x06>();
      c->character_file(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      const auto& cmd = msg.check_size_t<G_SendGuildCard_PC_6x06>();
      c->character_file(true, false)->guild_card.description = cmd.guild_card.description;
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3: {
      const auto& cmd = msg.check_size_t<G_SendGuildCard_GC_6x06>();
      c->character_file(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::XB_V3: {
      const auto& cmd = msg.check_size_t<G_SendGuildCard_XB_6x06>();
      c->character_file(true, false)->guild_card.description.encode(cmd.guild_card.description.decode(c->language()), c->language());
      break;
    }
    case Version::BB_V4:
      // Nothing to do... the command is blank; the server generates the guild card to be sent
      break;
    default:
      throw logic_error("unsupported game version");
  }

  send_guild_card(l->clients[msg.flag], c);
}

static asio::awaitable<void> on_symbol_chat(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SymbolChat_6x07>();
  if (c->can_chat && (cmd.client_id == c->lobby_client_id)) {
    forward_subcommand(c, msg);
  }
  co_return;
}

template <bool SenderBE>
static asio::awaitable<void> on_word_select_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_WordSelectT_6x74<SenderBE>>();
  if (c->can_chat && (cmd.client_id == c->lobby_client_id)) {
    if (command_is_private(msg.command)) {
      co_return;
    }

    auto s = c->require_server_state();
    auto l = c->require_lobby();
    if (l->battle_record && l->battle_record->battle_in_progress()) {
      l->battle_record->add_command(Episode3::BattleRecord::Event::Type::GAME_COMMAND, msg.data, msg.size);
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
        string name = escape_player_name(c->character_file()->disp.name.decode(c->language()));
        lc->log.warning_f("Untranslatable Word Select message: {}", e.what());
        send_text_message_fmt(lc, "$C4Untranslatable Word\nSelect message from\n{}", name);
      }
    }
  }
}

static asio::awaitable<void> on_word_select(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (is_pre_v1(c->version())) {
    // The Word Select command is a different size in final vs. NTE and proto, so handle that here by appending
    // FFFFFFFF0000000000000000
    string effective_data(reinterpret_cast<const char*>(msg.data), msg.size);
    effective_data.resize(0x20, 0x00);
    effective_data[0x01] = 0x08;
    effective_data[0x14] = 0xFF;
    effective_data[0x15] = 0xFF;
    effective_data[0x16] = 0xFF;
    effective_data[0x17] = 0xFF;
    SubcommandMessage translated_msg{msg.command, msg.flag, effective_data.data(), effective_data.size()};
    co_await on_word_select_t<false>(c, translated_msg);
  } else if (is_big_endian(c->version())) {
    co_await on_word_select_t<true>(c, msg);
  } else {
    co_await on_word_select_t<false>(c, msg);
  }
}

static asio::awaitable<void> on_warp(shared_ptr<Client>, SubcommandMessage& msg) {
  // Unconditionally block these. Players should use $warp instead.
  msg.check_size_t<G_InterLevelWarp_6x94>();
  co_return;
}

static asio::awaitable<void> on_set_player_visible(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SetPlayerVisibility_6x22_6x23>();

  if (cmd.header.client_id == c->lobby_client_id) {
    forward_subcommand(c, msg);

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
      } else if (c->check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST) && (c->version() != Version::BB_V4)) {
        c->clear_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
        c->log.info_f("LOADING_RUNNING_JOINABLE_QUEST flag cleared");
      }
    }
  }
  co_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static asio::awaitable<void> on_change_floor_6x1F(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (is_pre_v1(c->version())) {
    msg.check_size_t<G_SetPlayerFloor_DCNTE_6x1F>();
    // DC NTE and 11/2000 don't send 6F when they're done loading, so we clear the loading flag here instead.
    if (c->check_flag(Client::Flag::LOADING)) {
      c->clear_flag(Client::Flag::LOADING);
      c->log.info_f("LOADING flag cleared");
      send_resume_game(c->require_lobby(), c);
      c->require_lobby()->assign_inventory_and_bank_item_ids(c, true);
    }

  } else {
    const auto& cmd = msg.check_size_t<G_SetPlayerFloor_6x1F>();
    if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
      c->floor = cmd.floor;
    }
  }
  forward_subcommand(c, msg);
  co_return;
}

static asio::awaitable<void> on_change_floor_6x21(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_InterLevelWarp_6x21>();
  if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, msg);
  co_return;
}

static asio::awaitable<void> on_player_died(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_PlayerDied_6x4D>(0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    co_return;
  }

  // Decrease MAG's synchro
  try {
    auto& inventory = c->character_file()->inventory;
    size_t mag_index = inventory.find_equipped_item(EquipSlot::MAG);
    auto& data = inventory.items[mag_index].data;
    data.data2[0] = max<int8_t>(static_cast<int8_t>(data.data2[0] - 5), 0);
  } catch (const out_of_range&) {
  }

  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_player_revivable(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_PlayerRevivable_6x4E>(0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.header.client_id != c->lobby_client_id)) {
    co_return;
  }

  forward_subcommand(c, msg);

  // Revive if infinite HP is enabled
  bool player_cheats_enabled = l->check_flag(Lobby::Flag::CHEATS_ENABLED) ||
      (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE));
  if (player_cheats_enabled && c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
    G_UseMedicalCenter_6x31 v2_cmd = {0x31, 0x01, c->lobby_client_id};
    G_RevivePlayer_V3_BB_6xA1 v3_cmd = {0xA1, 0x01, c->lobby_client_id};
    static_assert(sizeof(v2_cmd) == sizeof(v3_cmd), "Command sizes do not match");

    const void* c_data = (!is_v1_or_v2(c->version()) || (c->version() == Version::GC_NTE))
        ? static_cast<const void*>(&v3_cmd)
        : static_cast<const void*>(&v2_cmd);
    // TODO: We might need to send different versions of the command here to different clients in certain crossplay
    // scenarios, so just using echo_to_lobby would not suffice. Figure out a way to handle this.
    co_await send_protected_command(c, c_data, sizeof(v3_cmd), true);
  }
}

static asio::awaitable<void> on_player_revived(shared_ptr<Client> c, SubcommandMessage& msg) {
  msg.check_size_t<G_PlayerRevived_6x4F>(0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, msg);
    if ((l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE))) &&
        c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
      co_await send_change_player_hp(l, c->lobby_client_id, PlayerHPChange::MAXIMIZE_HP, 0);
    }
  }
  co_return;
}

static asio::awaitable<void> on_received_condition(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_ClientIDHeader>(0xFFFF);

  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, msg);
    if (cmd.client_id == c->lobby_client_id) {
      bool player_cheats_enabled = l->check_flag(Lobby::Flag::CHEATS_ENABLED) ||
          c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE);
      if (player_cheats_enabled && c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        co_await send_remove_negative_conditions(c);
      }
    }
  }
}

template <typename CmdT>
static asio::awaitable<void> on_change_hp(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>(0xFFFF);

  auto l = c->require_lobby();
  if (!l->is_game() || (cmd.client_id != c->lobby_client_id)) {
    co_return;
  }

  forward_subcommand(c, msg);
  if ((l->check_flag(Lobby::Flag::CHEATS_ENABLED) || c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)) &&
      c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
    co_await send_change_player_hp(l, c->lobby_client_id, PlayerHPChange::MAXIMIZE_HP, 0);
  }
}

static asio::awaitable<void> on_cast_technique_finished(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_CastTechniqueComplete_6x48>();

  auto l = c->require_lobby();
  if (l->is_game() && (cmd.header.client_id == c->lobby_client_id)) {
    forward_subcommand(c, msg);
    bool player_cheats_enabled = !is_v1(c->version()) &&
        (l->check_flag(Lobby::Flag::CHEATS_ENABLED) || (c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)));
    if (player_cheats_enabled && c->check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
      send_player_stats_change(c, PlayerStatsChange::ADD_TP, 255);
    }
  }
  co_return;
}

static asio::awaitable<void> on_npc_control(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_NPCControl_6x69>();
  // Don't allow NPC control commands if there is a player in the relevant slot
  const auto& l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("cannot create or modify NPC in the lobby");
  }

  if ((cmd.command == 0 || cmd.command == 3) && ((cmd.param2 < 4) && l->clients[cmd.param2])) {
    throw runtime_error("cannot create NPC in existing player slot");
  }

  forward_subcommand(c, msg);
  co_return;
}

static asio::awaitable<void> on_switch_state_changed(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_WriteSwitchFlag_6x05>();

  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  if (!l->quest &&
      (cmd.flags & 1) &&
      (cmd.header.entity_id != 0xFFFF) &&
      (cmd.switch_flag_num < 0x100) &&
      c->check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
    auto sw_obj_st = l->map_state->object_state_for_index(c->version(), cmd.switch_flag_floor, cmd.header.entity_id - 0x4000);
    c->log.info_f("Switch assist triggered by K-{:03X} setting SW-{:02X}-{:02X}",
        sw_obj_st->k_id, cmd.switch_flag_floor, cmd.switch_flag_num);
    for (auto obj_st : l->map_state->door_states_for_switch_flag(c->version(), cmd.switch_flag_floor, cmd.switch_flag_num)) {
      if (obj_st->game_flags & 0x0001) {
        c->log.info_f("K-{:03X} is already unlocked", obj_st->k_id);
        continue;
      }
      if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_fmt(c, "$C5SWA K-{:03X} {:02X} {:02X}",
            obj_st->k_id, cmd.switch_flag_floor, cmd.switch_flag_num);
      }
      obj_st->game_flags |= 1;

      for (auto lc : l->clients) {
        if (!lc) {
          continue;
        }
        uint16_t object_index = l->map_state->index_for_object_state(lc->version(), obj_st);
        lc->log.info_f("Switch assist: door object K-{:03X} has index {:04X} on version {}",
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

  if (cmd.header.entity_id != 0xFFFF && c->check_flag(Client::Flag::DEBUG_ENABLED)) {
    const auto& obj_st = l->map_state->object_state_for_index(
        c->version(), cmd.switch_flag_floor, cmd.header.entity_id - 0x4000);
    auto s = c->require_server_state();
    uint8_t area = l->area_for_floor(c->version(), c->floor);
    auto type_name = obj_st->type_name(c->version(), area);
    send_text_message_fmt(c, "$C5K-{:03X} A {}", obj_st->k_id, type_name);
  }

  // Apparently sometimes 6x05 is sent with an invalid switch flag number. The client seems to just ignore the command
  // in that case, so we go ahead and forward it (in case the client's object update function is meaningful somehow)
  // and just don't update our view of the switch flags.
  if (l->switch_flags && (cmd.switch_flag_num < 0x100)) {
    if (cmd.flags & 1) {
      l->switch_flags->set(cmd.switch_flag_floor, cmd.switch_flag_num);
      if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_fmt(c, "$C5SW-{:02X}-{:02X} ON", cmd.switch_flag_floor, cmd.switch_flag_num);
      }
    } else {
      l->switch_flags->clear(cmd.switch_flag_floor, cmd.switch_flag_num);
      if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
        send_text_message_fmt(c, "$C5SW-{:02X}-{:02X} OFF", cmd.switch_flag_floor, cmd.switch_flag_num);
      }
    }
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_WriteSwitchFlag_6x05, true>(c, msg);
  co_return;
}

static asio::awaitable<void> on_play_sound_from_player(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_PlaySoundFromPlayer_6xB2>();
  // This command can be used to play arbitrary sounds, but the client only ever sends it for the camera shutter sound,
  // so we only allow that one.
  if (cmd.sound_id == 0x00051720) {
    forward_subcommand(c, msg);
  }
  co_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename CmdT>
static asio::awaitable<void> on_movement_xz(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }
  c->pos.x = cmd.pos.x;
  c->pos.z = cmd.pos.z;
  forward_subcommand(c, msg);
}

template <typename CmdT>
static asio::awaitable<void> on_movement_xyz(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }
  c->pos = cmd.pos;
  forward_subcommand(c, msg);
}

template <typename CmdT>
static asio::awaitable<void> on_movement_xz_with_floor(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }
  c->pos.x = cmd.pos.x;
  c->pos.z = cmd.pos.z;
  if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, msg);
}

template <typename CmdT>
static asio::awaitable<void> on_movement_xyz_with_floor(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }
  c->pos = cmd.pos;
  if (cmd.floor >= 0 && c->floor != static_cast<uint32_t>(cmd.floor)) {
    c->floor = cmd.floor;
  }
  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_set_animation_state(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_SetAnimationState_6x52>();
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }
  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (l->is_game()) {
    forward_subcommand(c, msg);
    co_return;
  }

  // The animation numbers were changed on V3. This is the most common one to see in the lobby (it occurs when a player
  // talks to the counter), so we take care to translate it specifically.
  bool c_is_v1_or_v2 = is_v1_or_v2(c->version());
  if (!((c_is_v1_or_v2 && (cmd.animation == 0x000A)) || (!c_is_v1_or_v2 && (cmd.animation == 0x0000)))) {
    forward_subcommand(c, msg);
    co_return;
  }

  G_SetAnimationState_6x52 other_cmd = cmd;
  other_cmd.animation = 0x000A - cmd.animation;
  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      auto& out_cmd = (is_v1_or_v2(lc->version()) != c_is_v1_or_v2) ? other_cmd : cmd;
      out_cmd.header.subcommand = translate_subcommand_number(lc->version(), Version::BB_V4, 0x52);
      send_command_t(lc, msg.command, msg.flag, out_cmd);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Item commands

static asio::awaitable<void> on_player_drop_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_DropItem_6x2A>();

  if ((cmd.header.client_id != c->lobby_client_id)) {
    co_return;
  }

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  auto p = c->character_file();
  auto item = p->remove_item(cmd.item_id, 0, *s->item_stack_limits(c->version()));
  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} dropped item {:08X} ({}) at {}:({:g}, {:g})",
        cmd.header.client_id, cmd.item_id, name, cmd.floor, cmd.pos.x, cmd.pos.z);
    c->print_inventory();
  }

  forward_subcommand(c, msg);
}

template <typename CmdT>
static asio::awaitable<void> on_create_inventory_item_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();

  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);

  // Players cannot send this on behalf of another player, but they can send it on behalf of an NPC; we don't track
  // items for NPCs so in that case we just mark the item ID as used and ignore it. This works for the most part,
  // because when NPCs use or equip items, we ignore the command since it has the wrong client ID.
  // TODO: This won't work if NPCs ever drop items that players can interact with. Presumably we would have to track
  // all NPCs' inventory items to handle that.
  auto s = c->require_server_state();
  if (cmd.header.client_id != c->lobby_client_id) {
    // Don't allow creating items in other players' inventories, only in NPCs'
    if (l->clients.at(cmd.header.client_id)) {
      co_return;
    }

    if (l->log.should_log(phosg::LogLevel::L_INFO)) {
      auto name = s->describe_item(c->version(), item);
      l->log.info_f("Player {} created inventory item {:08X} ({}) in inventory of NPC {:02X}; ignoring", c->lobby_client_id, item.id, name, cmd.header.client_id);
    }

  } else {
    c->character_file()->add_item(item, *s->item_stack_limits(c->version()));

    if (l->log.should_log(phosg::LogLevel::L_INFO)) {
      auto name = s->describe_item(c->version(), item);
      l->log.info_f("Player {} created inventory item {:08X} ({})", c->lobby_client_id, item.id, name);
      c->print_inventory();
    }
  }

  forward_subcommand_with_item_transcode_t(c, msg.command, msg.flag, cmd);
}

static asio::awaitable<void> on_create_inventory_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (msg.size == sizeof(G_CreateInventoryItem_PC_V3_BB_6x2B)) {
    co_await on_create_inventory_item_t<G_CreateInventoryItem_PC_V3_BB_6x2B>(c, msg);
  } else if (msg.size == sizeof(G_CreateInventoryItem_DC_6x2B)) {
    co_await on_create_inventory_item_t<G_CreateInventoryItem_DC_6x2B>(c, msg);
  } else {
    throw runtime_error("invalid size for 6x2B command");
  }
  co_return;
}

template <typename CmdT>
static void on_drop_partial_stack_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<CmdT>();

  auto l = c->require_lobby();
  if (c->version() == Version::BB_V4) {
    throw runtime_error("6x5D command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6x5D command sent in non-game lobby");
  }
  // TODO: Should we check the client ID here too?

  // We don't delete anything from the inventory here; the client will send a 6x29 to do so following this command.

  ItemData item = cmd.item_data;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} split stack to create floor item {:08X} ({}) at {}:({:g},{:g})",
        cmd.header.client_id, item.id, name, cmd.floor, cmd.pos.x, cmd.pos.z);
    c->print_inventory();
  }

  forward_subcommand_with_item_transcode_t(c, msg.command, msg.flag, cmd);
}

static asio::awaitable<void> on_drop_partial_stack(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (msg.size == sizeof(G_DropStackedItem_PC_V3_BB_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_PC_V3_BB_6x5D>(c, msg);
  } else if (msg.size == sizeof(G_DropStackedItem_DC_6x5D)) {
    on_drop_partial_stack_t<G_DropStackedItem_DC_6x5D>(c, msg);
  } else {
    throw runtime_error("invalid size for 6x5D command");
  }
  co_return;
}

static asio::awaitable<void> on_drop_partial_stack_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SplitStackedItem_BB_6xC3>();
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
  auto p = c->character_file();
  const auto& limits = *s->item_stack_limits(c->version());
  auto item = p->remove_item(cmd.item_id, cmd.amount, limits);

  // If a stack was split, the original item still exists, so the dropped item needs a new ID. remove_item signals this
  // by returning an item with an ID of 0xFFFFFFFF.
  if (item.id == 0xFFFFFFFF) {
    item.id = l->generate_item_id(c->lobby_client_id);
  }

  // PSOBB sends a 6x29 command after it receives the 6x5D, so we need to add the item back to the player's inventory
  // to correct for this (it will get removed again by the 6x29 handler)
  p->add_item(item, limits);

  l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x00F);
  send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.pos);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} split stack {:08X} (removed: {}) at {}:({:g}, {:g})",
        cmd.header.client_id, cmd.item_id, name, cmd.floor, cmd.pos.x, cmd.pos.z);
    c->print_inventory();
  }
  co_return;
}

static asio::awaitable<void> on_buy_shop_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_BuyShopItem_6x5E>();
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
  auto p = c->character_file();
  ItemData item = cmd.item_data;
  item.data2d = 0; // Clear the price field
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  p->add_item(item, *s->item_stack_limits(c->version()));

  size_t price = s->item_parameter_table(c->version())->price_for_item(item);
  p->remove_meseta(price, c->version() != Version::BB_V4);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} bought item {:08X} ({}) from shop ({} Meseta)",
        cmd.header.client_id, item.id, name, price);
    c->print_inventory();
  }

  forward_subcommand_with_item_transcode_t(c, msg.command, msg.flag, cmd);
  co_return;
}

void send_item_notification_if_needed(shared_ptr<Client> c, const ItemData& item, bool is_from_rare_table) {
  auto s = c->require_server_state();

  bool should_notify = false;
  bool should_include_rare_header = false;
  switch (c->get_drop_notification_mode()) {
    case Client::ItemDropNotificationMode::NOTHING:
      break;
    case Client::ItemDropNotificationMode::RARES_ONLY:
      should_notify = (is_from_rare_table || (item.data1[0] == 0x03)) &&
          s->item_parameter_table(c->version())->is_item_rare(item);
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
    string name = s->describe_item(c->version(), item, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);
    const char* rare_header = (should_include_rare_header ? "$C6Rare item dropped:\n" : "");
    send_text_message_fmt(c, "{}{}", rare_header, name);
  }
}

template <typename CmdT>
static void on_box_or_enemy_item_drop_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  // I'm lazy and this should never happen for item commands (since all players need to stay in sync)
  if (command_is_private(msg.command)) {
    throw runtime_error("item subcommand sent via private command");
  }

  const auto& cmd = msg.check_size_t<CmdT>();

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || (c->lobby_client_id != l->leader_id)) {
    return;
  }
  if (c->version() == Version::BB_V4) {
    throw runtime_error("BB client sent 6x5F command");
  }

  bool should_notify = s->rare_notifs_enabled_for_client_drops && (l->drop_mode == ServerDropMode::CLIENT);

  shared_ptr<const MapState::EnemyState> ene_st;
  shared_ptr<const MapState::ObjectState> obj_st;
  string from_entity_str;
  if (cmd.item.source_type == 1) {
    ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.item.floor, cmd.item.entity_index);
    from_entity_str = std::format(" from E-{:03X}", ene_st->e_id);
  } else {
    obj_st = l->map_state->object_state_for_index(c->version(), cmd.item.floor, cmd.item.entity_index);
    from_entity_str = std::format(" from K-{:03X}", obj_st->k_id);
  }

  ItemData item = cmd.item.item;
  item.decode_for_version(c->version());
  l->on_item_id_generated_externally(item.id);
  l->add_item(cmd.item.floor, item, cmd.item.pos, obj_st, ene_st, should_notify ? 0x100F : 0x000F);

  auto name = s->describe_item(c->version(), item);
  l->log.info_f("Player {} (leader) created floor item {:08X} ({}){} at {}:({:g}, {:g})",
      l->leader_id,
      item.id,
      name,
      from_entity_str,
      cmd.item.floor,
      cmd.item.pos.x,
      cmd.item.pos.z);

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
    send_item_notification_if_needed(lc, item, true);
  }
}

static asio::awaitable<void> on_box_or_enemy_item_drop(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (msg.size == sizeof(G_DropItem_DC_6x5F)) {
    on_box_or_enemy_item_drop_t<G_DropItem_DC_6x5F>(c, msg);
  } else if (msg.size == sizeof(G_DropItem_PC_V3_BB_6x5F)) {
    on_box_or_enemy_item_drop_t<G_DropItem_PC_V3_BB_6x5F>(c, msg);
  } else {
    throw runtime_error("invalid size for 6x5F command");
  }
  co_return;
}

static asio::awaitable<void> on_pick_up_item_generic(
    shared_ptr<Client> c, uint16_t client_id, uint16_t floor, uint32_t item_id, bool is_request) {
  auto l = c->require_lobby();
  if (!l->is_game() || (client_id != c->lobby_client_id)) {
    co_return;
  }

  if (!l->item_exists(floor, item_id)) {
    // This can happen if the network is slow, and the client tries to pick up the same item multiple times. Or
    // multiple clients could try to pick up the same item at approximately the same time; only one should get it.
    l->log.warning_f("Player {} requests to pick up {:08X}, but the item does not exist; dropping command", client_id, item_id);

  } else {
    // This is handled by the server on BB, and by the leader on other versions. However, the client's logic is to
    // simply always send a 6x59 command when it receives a 6x5A and the floor item exists, so we just implement that
    // logic here instead of forwarding the 6x5A to the leader.

    auto p = c->character_file();
    auto s = c->require_server_state();
    auto fi = l->remove_item(floor, item_id, c->lobby_client_id);
    if (!fi->visible_to_client(c->lobby_client_id)) {
      l->log.warning_f("Player {} requests to pick up {:08X}, but is it not visible to them; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      co_return;
    }

    try {
      p->add_item(fi->data, *s->item_stack_limits(c->version()));
    } catch (const out_of_range&) {
      // Inventory is full; put the item back where it was
      l->log.warning_f("Player {} requests to pick up {:08X}, but their inventory is full; dropping command",
          client_id, item_id);
      l->add_item(floor, fi);
      co_return;
    }

    if (l->log.should_log(phosg::LogLevel::L_INFO)) {
      auto s = c->require_server_state();
      auto name = s->describe_item(c->version(), fi->data);
      l->log.info_f("Player {} picked up {:08X} ({})", client_id, item_id, name);
      c->print_inventory();
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

    if (!c->login->account->check_user_flag(Account::UserFlag::DISABLE_DROP_NOTIFICATION_BROADCAST) && (fi->flags & 0x1000)) {
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
        string desc_ingame = s->describe_item(c->version(), fi->data, ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES);
        string desc_http = s->describe_item(c->version(), fi->data);

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
          co_await s->http_server->send_rare_drop_notification(message);
        }

        string message = std::format("$C6{}$C7 found\n{}", p_name, desc_ingame);
        string bb_message = std::format("$C6{}$C7 has found {}", p_name, desc_ingame);
        if (should_send_global_notif) {
          for (auto& it : s->game_server->all_clients()) {
            if (it->login &&
                !is_patch(it->version()) &&
                !is_ep3(it->version()) &&
                it->lobby.lock()) {
              send_text_or_scrolling_message(it, message, bb_message);
            }
          }
        } else {
          send_text_or_scrolling_message(l, nullptr, message, bb_message);
        }
      }
    }
  }
}

static asio::awaitable<void> on_pick_up_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_PickUpItem_6x59>();
  co_await on_pick_up_item_generic(c, cmd.client_id2, cmd.floor, cmd.item_id, false);
}

static asio::awaitable<void> on_pick_up_item_request(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_PickUpItemRequest_6x5A>();
  co_await on_pick_up_item_generic(c, cmd.header.client_id, cmd.floor, cmd.item_id, true);
}

static asio::awaitable<void> on_equip_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_EquipItem_6x25>();

  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }

  auto l = c->require_lobby();
  EquipSlot slot = static_cast<EquipSlot>(cmd.equip_slot.load());
  auto p = c->character_file();
  p->inventory.equip_item_id(cmd.item_id, slot);
  c->log.info_f("Equipped item {:08X}", cmd.item_id);

  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_unequip_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_UnequipItem_6x26>();

  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }

  auto l = c->require_lobby();
  auto p = c->character_file();
  p->inventory.unequip_item_id(cmd.item_id);
  c->log.info_f("Unequipped item {:08X}", cmd.item_id);

  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_use_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_UseItem_6x27>();

  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }

  auto l = c->require_lobby();
  auto s = c->require_server_state();
  auto p = c->character_file();
  size_t index = p->inventory.find_item(cmd.item_id);
  string name;
  {
    // Note: We manually downscope item here because player_use_item will likely move or delete the item, which will
    // break the reference, so we don't want to accidentally use it again after that.
    const auto& item = p->inventory.items[index].data;
    name = s->describe_item(c->version(), item);
  }
  player_use_item(c, index, l->rand_crypt);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    l->log.info_f("Player {} used item {}:{:08X} ({})", c->lobby_client_id, cmd.header.client_id, cmd.item_id, name);
    c->print_inventory();
  }

  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_feed_mag(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_FeedMag_6x28>();

  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }

  auto s = c->require_server_state();
  auto l = c->require_lobby();
  auto p = c->character_file();

  size_t mag_index = p->inventory.find_item(cmd.mag_item_id);
  size_t fed_index = p->inventory.find_item(cmd.fed_item_id);
  string mag_name, fed_name;
  {
    // Note: We downscope these because player_feed_mag will likely delete the items, which will break these references
    const auto& fed_item = p->inventory.items[fed_index].data;
    fed_name = s->describe_item(c->version(), fed_item);
    const auto& mag_item = p->inventory.items[mag_index].data;
    mag_name = s->describe_item(c->version(), mag_item);
  }
  player_feed_mag(c, mag_index, fed_index);

  // On BB, the player only sends a 6x28; on other versions, the player sends a 6x29 immediately after to destroy the
  // fed item. So on BB, we should remove the fed item here, but on other versions, we allow the following 6x29 command
  // to do that.
  if (c->version() == Version::BB_V4) {
    p->remove_item(cmd.fed_item_id, 1, *s->item_stack_limits(c->version()));
  }

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    l->log.info_f("Player {} fed item {}:{:08X} ({}) to mag {}:{:08X} ({})",
        c->lobby_client_id, cmd.header.client_id, cmd.fed_item_id, fed_name,
        cmd.header.client_id, cmd.mag_item_id, mag_name);
    c->print_inventory();
  }

  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_xbox_voice_chat_control(shared_ptr<Client> c, SubcommandMessage& msg) {
  // If sent by an XB client, should be forwarded to XB clients and no one else
  if (c->version() != Version::XB_V3) {
    co_return;
  }

  auto l = c->require_lobby();
  if (command_is_private(msg.command)) {
    if (msg.flag >= l->max_clients) {
      co_return;
    }
    auto target = l->clients[msg.flag];
    if (target && (target->version() == Version::XB_V3)) {
      send_command(target, msg.command, msg.flag, msg.data, msg.size);
    }
  } else {
    for (auto& lc : l->clients) {
      if (lc && (lc != c) && (lc->version() == Version::XB_V3)) {
        send_command(lc, msg.command, msg.flag, msg.data, msg.size);
      }
    }
  }
}

static asio::awaitable<void> on_gc_nte_exclusive(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto can_participate = [&](Version vers) {
    return (!is_v1_or_v2(vers) || (vers == Version::GC_NTE));
  };
  if (!can_participate(c->version())) {
    co_return;
  }

  // Command should not be forwarded across the GC NTE boundary, but may be forwarded to other clients within that
  // boundary
  bool c_is_nte = (c->version() == Version::GC_NTE);

  auto l = c->require_lobby();
  if (command_is_private(msg.command)) {
    if (msg.flag >= l->max_clients) {
      co_return;
    }
    auto lc = l->clients[msg.flag];
    if (lc && can_participate(lc->version()) && ((lc->version() == Version::GC_NTE) == c_is_nte)) {
      send_command(lc, msg.command, msg.flag, msg.data, msg.size);
    }
  } else {
    for (auto& lc : l->clients) {
      if (lc && (lc != c) && can_participate(lc->version()) && ((lc->version() == Version::GC_NTE) == c_is_nte)) {
        send_command(lc, msg.command, msg.flag, msg.data, msg.size);
      }
    }
  }
}

static asio::awaitable<void> on_open_shop_bb_or_ep3_battle_subs(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xB5 command sent in non-game lobby");
  }

  if (is_ep3(c->version())) {
    co_await on_ep3_battle_subs(c, msg);
  } else if (l->episode == Episode::EP3) { // There's no item_creator in an Ep3 game
    throw runtime_error("received BB shop subcommand in Ep3 game");
  } else if (c->version() != Version::BB_V4) {
    throw runtime_error("received BB shop subcommand from non-BB client");
  } else {
    const auto& cmd = msg.check_size_t<G_ShopContentsRequest_BB_6xB5>();
    auto s = c->require_server_state();
    size_t level = c->character_file()->disp.stats.level + 1;
    switch (cmd.shop_type) {
      case 0:
        c->bb_shop_contents[0] = l->item_creator->generate_tool_shop_contents(level);
        break;
      case 1:
        c->bb_shop_contents[1] = l->item_creator->generate_weapon_shop_contents(level);
        break;
      case 2: {
        Episode episode = episode_for_area(l->area_for_floor(c->version(), 0));
        c->bb_shop_contents[2] = l->item_creator->generate_armor_shop_contents(episode, level);
        break;
      }
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

  // TTradeCardServer uses 4 to indicate the slot is empty, so we allow 4 in the client ID checks below
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

static asio::awaitable<void> on_open_bank_bb_or_card_trade_counter_ep3(
    shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xBB command sent in non-game lobby");
  }

  if (c->version() == Version::BB_V4) {
    c->set_flag(Client::Flag::AT_BANK_COUNTER);
    send_bank(c);
  } else if (l->is_ep3() && validate_6xBB(msg.check_size_t<G_SyncCardTradeServerState_Ep3_6xBB>())) {
    forward_subcommand(c, msg);
  }
  co_return;
}

static asio::awaitable<void> on_ep3_private_word_select_bb_bank_action(
    shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xBD command sent in non-game lobby");
  }

  auto s = c->require_server_state();
  if (is_ep3(c->version())) {
    const auto& cmd = msg.check_size_t<G_PrivateWordSelect_Ep3_6xBD>();
    s->word_select_table->validate(cmd.message, c->version());

    string from_name = c->character_file()->disp.name.decode(c->language());
    static const string whisper_text = "(whisper)";
    auto send_to_client = [&](shared_ptr<Client> lc) -> void {
      if (cmd.private_flags & (1 << lc->lobby_client_id)) {
        try {
          send_chat_message(lc, c->login->account->account_id, from_name, whisper_text, cmd.private_flags);
        } catch (const runtime_error& e) {
          lc->log.warning_f("Failed to encode chat message: {}", e.what());
        }
      } else {
        send_command_t(lc, msg.command, msg.flag, cmd);
      }
    };

    if (command_is_private(msg.command)) {
      if (msg.flag >= l->max_clients) {
        co_return;
      }
      auto target = l->clients[msg.flag];
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
          send_command(target, msg.command, msg.flag, msg.data, msg.size);
        }
      }
    }

    if (l->battle_record && l->battle_record->battle_in_progress()) {
      auto type = ((msg.command & 0xF0) == 0xC0)
          ? Episode3::BattleRecord::Event::Type::EP3_GAME_COMMAND
          : Episode3::BattleRecord::Event::Type::GAME_COMMAND;
      l->battle_record->add_command(type, msg.data, msg.size);
    }

  } else if (c->version() == Version::BB_V4) {
    const auto& cmd = msg.check_size_t<G_BankAction_BB_6xBD>();

    if (!l->is_game()) {
      co_return;
    }

    auto p = c->character_file();
    auto bank = c->bank_file();
    if (cmd.action == 0) { // Deposit
      if (cmd.item_id == 0xFFFFFFFF) { // Deposit Meseta
        if (cmd.meseta_amount > p->disp.stats.meseta) {
          l->log.info_f("Player {} attempted to deposit {} Meseta in the bank, but has only {} Meseta on hand",
              c->lobby_client_id, cmd.meseta_amount, p->disp.stats.meseta);
        } else if ((bank->meseta + cmd.meseta_amount) > bank->max_meseta) {
          l->log.info_f("Player {} attempted to deposit {} Meseta in the bank, but already has {} Meseta in the bank",
              c->lobby_client_id, cmd.meseta_amount, p->disp.stats.meseta);
        } else {
          bank->meseta += cmd.meseta_amount;
          p->disp.stats.meseta -= cmd.meseta_amount;
          l->log.info_f("Player {} deposited {} Meseta in the bank (bank now has {}; inventory now has {})",
              c->lobby_client_id, cmd.meseta_amount, bank->meseta, p->disp.stats.meseta);
        }

      } else { // Deposit item
        const auto& limits = *s->item_stack_limits(c->version());
        auto item = p->remove_item(cmd.item_id, cmd.item_amount, limits);
        // If a stack was split, the bank item retains the same item ID as the inventory item. This is annoying but
        // doesn't cause any problems because we always generate a new item ID when withdrawing from the bank, so
        // there's no chance of conflict later.
        if (item.id == 0xFFFFFFFF) {
          item.id = cmd.item_id;
        }
        bank->add_item(item, limits);
        send_destroy_item_to_lobby(c, cmd.item_id, cmd.item_amount, true);

        if (l->log.should_log(phosg::LogLevel::L_INFO)) {
          string name = s->describe_item(Version::BB_V4, item);
          l->log.info_f("Player {} deposited item {:08X} (x{}) ({}) in the bank",
              c->lobby_client_id, cmd.item_id, cmd.item_amount, name);
          c->print_inventory();
        }
      }

    } else if (cmd.action == 1) { // Take
      if (cmd.item_index == 0xFFFF) { // Take Meseta
        if (cmd.meseta_amount > bank->meseta) {
          l->log.info_f("Player {} attempted to withdraw {} Meseta from the bank, but has only {} Meseta in the bank",
              c->lobby_client_id, cmd.meseta_amount, bank->meseta);
        } else if ((p->disp.stats.meseta + cmd.meseta_amount) > 999999) {
          l->log.info_f("Player {} attempted to withdraw {} Meseta from the bank, but already has {} Meseta on hand",
              c->lobby_client_id, cmd.meseta_amount, p->disp.stats.meseta);
        } else {
          bank->meseta -= cmd.meseta_amount;
          p->disp.stats.meseta += cmd.meseta_amount;
          l->log.info_f("Player {} withdrew {} Meseta from the bank (bank now has {}; inventory now has {})",
              c->lobby_client_id, cmd.meseta_amount, bank->meseta, p->disp.stats.meseta);
        }

      } else { // Take item
        const auto& limits = *s->item_stack_limits(c->version());
        auto item = bank->remove_item(cmd.item_id, cmd.item_amount, limits);
        item.id = l->generate_item_id(c->lobby_client_id);
        p->add_item(item, limits);
        send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

        if (l->log.should_log(phosg::LogLevel::L_INFO)) {
          string name = s->describe_item(Version::BB_V4, item);
          l->log.info_f("Player {} withdrew item {:08X} (x{}) ({}) from the bank",
              c->lobby_client_id, item.id, cmd.item_amount, name);
          c->print_inventory();
        }
      }

    } else if (cmd.action == 3) { // Leave bank counter
      c->clear_flag(Client::Flag::AT_BANK_COUNTER);
    }
  }
}

static void on_sort_inventory_bb_inner(shared_ptr<Client> c, const SubcommandMessage& msg) {
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC4 command sent by non-BB client");
  }

  const auto& cmd = msg.check_size_t<G_SortInventory_BB_6xC4>();
  auto p = c->character_file();

  // Make sure the set of item IDs passed in by the client exactly matches the set of item IDs present in the inventory
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
  // It's annoying that extension data is stored in the inventory items array, because we have to be careful to avoid
  // sorting it here too.
  for (size_t x = 0; x < 30; x++) {
    sorted[x].extension_data1 = p->inventory.items[x].extension_data1;
    sorted[x].extension_data2 = p->inventory.items[x].extension_data2;
  }
  p->inventory.items = sorted;
}

static asio::awaitable<void> on_sort_inventory_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  // There is a GCC bug that causes this function to not compile properly unless the sorting implementation is in a
  // separate function. I think it's something to do with how it allocates the coroutine's locals, but it's enough to
  // avoid for now.
  on_sort_inventory_bb_inner(c, msg);
  co_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXP/Drop Item commands

G_SpecializableItemDropRequest_6xA2 normalize_drop_request(const void* data, size_t size) {
  G_SpecializableItemDropRequest_6xA2 cmd;
  if (size == sizeof(G_SpecializableItemDropRequest_6xA2)) {
    cmd = check_size_t<G_SpecializableItemDropRequest_6xA2>(data, size);
  } else if (size == sizeof(G_StandardDropItemRequest_PC_V3_BB_6x60)) {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_PC_V3_BB_6x60>(data, size);
    cmd.header = in_cmd.header;
    cmd.entity_index = in_cmd.entity_index;
    cmd.floor = in_cmd.floor;
    cmd.rt_index = in_cmd.rt_index;
    cmd.pos = in_cmd.pos;
    cmd.ignore_def = in_cmd.ignore_def;
    cmd.effective_area = in_cmd.effective_area;
  } else {
    const auto& in_cmd = check_size_t<G_StandardDropItemRequest_DC_6x60>(data, size);
    cmd.header = in_cmd.header;
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
    shared_ptr<Client> c,
    G_SpecializableItemDropRequest_6xA2& cmd,
    Difficulty difficulty,
    uint8_t event,
    shared_ptr<MapState> map,
    bool mark_drop) {
  Version version = c->version();
  bool is_box = (cmd.rt_index == 0x30);

  DropReconcileResult res;
  res.effective_rt_index = 0xFF;
  res.should_drop = true;
  res.ignore_def = (cmd.ignore_def != 0);
  if (!map) {
    return res;
  }

  if (is_box) {
    res.obj_st = map->object_state_for_index(version, cmd.floor, cmd.entity_index);
    if (!res.obj_st->super_obj) {
      throw std::runtime_error("referenced object from drop request is a player trap");
    }
    const auto* set_entry = res.obj_st->super_obj->version(version).set_entry;
    if (!set_entry) {
      throw std::runtime_error("object set entry is missing");
    }
    string type_name = MapFile::name_for_object_type(set_entry->base_type, version);
    c->log.info_f("Drop check for K-{:03X} {} {}",
        res.obj_st->k_id,
        res.ignore_def ? 'G' : 'S',
        type_name);
    if (cmd.floor != res.obj_st->super_obj->floor) {
      c->log.warning_f("Floor {:02X} from command does not match object\'s expected floor {:02X}",
          cmd.floor, res.obj_st->super_obj->floor);
    }
    if (is_v1_or_v2(version) && (version != Version::GC_NTE)) {
      // V1/V2 don't have 6xA2, so we can't get ignore_def or the object parameters from the client on those versions
      cmd.param3 = set_entry->param3;
      cmd.param4 = set_entry->param4;
      cmd.param5 = set_entry->param5;
      cmd.param6 = set_entry->param6;
    }
    bool object_ignore_def = (set_entry->param1 > 0.0);
    if (res.ignore_def != object_ignore_def) {
      c->log.warning_f("ignore_def value {} from command does not match object\'s expected ignore_def {} (from p1={:g})",
          res.ignore_def ? "true" : "false", object_ignore_def ? "true" : "false", set_entry->param1);
    }
    if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
      string type_name = MapFile::name_for_object_type(set_entry->base_type, version);
      send_text_message_fmt(c, "$C5K-{:03X} {} {}", res.obj_st->k_id, res.ignore_def ? 'G' : 'S', type_name);
    }

  } else {
    res.ref_ene_st = map->enemy_state_for_index(version, cmd.floor, cmd.entity_index);
    res.target_ene_st = res.ref_ene_st->alias_target_ene_st ? res.ref_ene_st->alias_target_ene_st : res.ref_ene_st;
    uint8_t area = map->floor_to_area.at(res.target_ene_st->super_ene->floor);
    EnemyType type = res.target_ene_st->type(version, area, difficulty, event);
    c->log.info_f("Drop check for E-{:03X} (target E-{:03X}, type {})",
        res.ref_ene_st->e_id, res.target_ene_st->e_id, phosg::name_for_enum(type));
    res.effective_rt_index = type_definition_for_enemy(type).rt_index;
    // rt_indexes in Episode 4 don't match those sent in the command; we just ignore what the client sends.
    if ((area < 0x24) && (cmd.rt_index != res.effective_rt_index)) {
      // Special cases: BULCLAW => BULK and DARK_GUNNER => DEATH_GUNNER
      if (cmd.rt_index == 0x27 && type == EnemyType::BULCLAW) {
        c->log.info_f("E-{:03X} killed as BULK instead of BULCLAW", res.target_ene_st->e_id);
        res.effective_rt_index = 0x27;
      } else if (cmd.rt_index == 0x23 && type == EnemyType::DARK_GUNNER) {
        c->log.info_f("E-{:03X} killed as DEATH_GUNNER instead of DARK_GUNNER", res.target_ene_st->e_id);
        res.effective_rt_index = 0x23;
      } else {
        c->log.warning_f("rt_index {:02X} from command does not match entity\'s expected index {:02X}",
            cmd.rt_index, res.effective_rt_index);
        if (!is_v4(version)) {
          res.effective_rt_index = cmd.rt_index;
        }
      }
    }
    if (cmd.floor != res.target_ene_st->super_ene->floor) {
      c->log.warning_f("Floor {:02X} from command does not match entity\'s expected floor {:02X}",
          cmd.floor, res.target_ene_st->super_ene->floor);
    }
    if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
      send_text_message_fmt(c, "$C5E-{:03X} {}", res.target_ene_st->e_id, phosg::name_for_enum(type));
    }
  }

  if (mark_drop) {
    if (res.obj_st) {
      if (res.obj_st->item_drop_checked) {
        c->log.info_f("Drop check has already occurred for K-{:03X}; skipping it", res.obj_st->k_id);
        res.should_drop = false;
      } else {
        res.obj_st->item_drop_checked = true;
      }
    }
    if (res.target_ene_st) {
      if (res.target_ene_st->server_flags & MapState::EnemyState::Flag::ITEM_DROPPED) {
        c->log.info_f("Drop check has already occurred for E-{:03X}; skipping it", res.target_ene_st->e_id);
        res.should_drop = false;
      } else {
        res.target_ene_st->server_flags |= MapState::EnemyState::Flag::ITEM_DROPPED;
      }
    }
  }

  return res;
}

static asio::awaitable<void> on_entity_drop_item_request(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game() || l->episode == Episode::EP3) {
    co_return;
  }

  // Note: We always call reconcile_drop_request_with_map, even in client drop mode, so that we can correctly mark
  // enemies and objects as having dropped their items in persistent games.
  G_SpecializableItemDropRequest_6xA2 cmd = normalize_drop_request(msg.data, msg.size);
  auto rec = reconcile_drop_request_with_map(c, cmd, l->difficulty, l->event, l->map_state, true);

  ServerDropMode drop_mode = l->drop_mode;
  switch (drop_mode) {
    case ServerDropMode::DISABLED:
      co_return;
    case ServerDropMode::CLIENT: {
      // If the leader is BB, use SERVER_SHARED instead
      // TODO: We should also use server drops if any clients have incompatible object lists, since they might generate
      // incorrect IDs for items and we can't override them
      auto leader = l->clients[l->leader_id];
      if (leader && leader->version() == Version::BB_V4) {
        drop_mode = ServerDropMode::SERVER_SHARED;
        break;
      } else {
        forward_subcommand(c, msg);
        co_return;
      }
    }
    case ServerDropMode::SERVER_SHARED:
    case ServerDropMode::SERVER_DUPLICATE:
    case ServerDropMode::SERVER_PRIVATE:
      break;
    default:
      throw logic_error("invalid drop mode");
  }

  if (rec.should_drop) {
    auto generate_item = [&]() -> ItemCreator::DropResult {
      if (rec.obj_st) {
        if (rec.ignore_def) {
          l->log.info_f("Creating item from box {:04X} => K-{:03X} (area {:02X})",
              cmd.entity_index, rec.obj_st->k_id, cmd.effective_area);
          return l->item_creator->on_box_item_drop(cmd.effective_area);
        } else {
          l->log.info_f(
              "Creating item from box {:04X} => K-{:03X} (area {:02X}; specialized with {:g} {:08X} {:08X} {:08X})",
              cmd.entity_index, rec.obj_st->k_id, cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
          return l->item_creator->on_specialized_box_item_drop(
              cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
        }
      } else if (rec.target_ene_st) {
        l->log.info_f("Creating item from enemy {:04X} => E-{:03X} (area {:02X})",
            cmd.entity_index, rec.target_ene_st->e_id, cmd.effective_area);
        return l->item_creator->on_monster_item_drop(rec.effective_rt_index, cmd.effective_area);
      } else {
        throw runtime_error("neither object nor enemy were present");
      }
    };

    auto get_entity_index = [&](Version v) -> uint16_t {
      if (rec.obj_st) {
        return l->map_state->index_for_object_state(v, rec.obj_st);
      } else if (rec.ref_ene_st) {
        return l->map_state->index_for_enemy_state(v, rec.ref_ene_st);
      } else {
        return 0xFFFF;
      }
    };

    switch (drop_mode) {
      case ServerDropMode::DISABLED:
      case ServerDropMode::CLIENT:
        throw logic_error("unhandled simple drop mode");
      case ServerDropMode::SERVER_SHARED:
      case ServerDropMode::SERVER_DUPLICATE: {
        auto res = generate_item();
        if (res.item.empty()) {
          l->log.info_f("No item was created");
        } else {
          string name = s->describe_item(c->version(), res.item);
          l->log.info_f("Entity {:04X} (area {:02X}) created item {}", cmd.entity_index, cmd.effective_area, name);
          if (drop_mode == ServerDropMode::SERVER_DUPLICATE) {
            for (const auto& lc : l->clients) {
              if (lc && (rec.obj_st || (lc->floor == cmd.floor))) {
                res.item.id = l->generate_item_id(0xFF);
                l->log.info_f("Creating item {:08X} at {:02X}:{:g},{:g} for {}",
                    res.item.id, cmd.floor, cmd.pos.x, cmd.pos.z, lc->channel->name);
                l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ref_ene_st, 0x1000 | (1 << lc->lobby_client_id));
                send_drop_item_to_channel(
                    s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
                send_item_notification_if_needed(lc, res.item, res.is_from_rare_table);
              }
            }

          } else {
            res.item.id = l->generate_item_id(0xFF);
            l->log.info_f("Creating item {:08X} at {:02X}:{:g},{:g} for all clients",
                res.item.id, cmd.floor, cmd.pos.x, cmd.pos.z);
            l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ref_ene_st, 0x100F);
            for (auto lc : l->clients) {
              if (lc) {
                send_drop_item_to_channel(
                    s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
                send_item_notification_if_needed(lc, res.item, res.is_from_rare_table);
              }
            }
          }
        }
        break;
      }
      case ServerDropMode::SERVER_PRIVATE: {
        for (const auto& lc : l->clients) {
          if (lc && (rec.obj_st || (lc->floor == cmd.floor))) {
            auto res = generate_item();
            if (res.item.empty()) {
              l->log.info_f("No item was created for {}", lc->channel->name);
            } else {
              string name = s->describe_item(lc->version(), res.item);
              l->log.info_f("Entity {:04X} (area {:02X}) created item {}", cmd.entity_index, cmd.effective_area, name);
              res.item.id = l->generate_item_id(0xFF);
              l->log.info_f("Creating item {:08X} at {:02X}:{:g},{:g} for {}",
                  res.item.id, cmd.floor, cmd.pos.x, cmd.pos.z, lc->channel->name);
              l->add_item(cmd.floor, res.item, cmd.pos, rec.obj_st, rec.ref_ene_st, 0x1000 | (1 << lc->lobby_client_id));
              send_drop_item_to_channel(
                  s, lc->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, get_entity_index(lc->version()));
              send_item_notification_if_needed(lc, res.item, res.is_from_rare_table);
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

static asio::awaitable<void> on_set_quest_flag(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  uint16_t flag_num, action;
  Difficulty difficulty;
  if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
    const auto& cmd = msg.check_size_t<G_UpdateQuestFlag_DC_PC_6x75>();
    flag_num = cmd.flag;
    action = cmd.action;
    difficulty = l->difficulty;
  } else {
    const auto& cmd = msg.check_size_t<G_UpdateQuestFlag_V3_BB_6x75>();
    flag_num = cmd.flag;
    action = cmd.action;
    difficulty = static_cast<Difficulty>(cmd.difficulty16.load());
  }

  // The client explicitly checks action for both 0 and 1 - any other value means no operation is performed.
  if ((flag_num >= 0x400) || (static_cast<size_t>(difficulty) > 3) || (action > 1)) {
    co_return;
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
    auto p = c->character_file(true, false);
    if (should_set) {
      c->log.info_f("Setting quest flag {}:{:04X}", name_for_difficulty(difficulty), flag_num);
      p->quest_flags.set(difficulty, flag_num);
    } else {
      c->log.info_f("Clearing quest flag {}:{:04X}", name_for_difficulty(difficulty), flag_num);
      p->quest_flags.clear(difficulty, flag_num);
    }
  }

  forward_subcommand(c, msg);

  if (l->drop_mode != ServerDropMode::DISABLED) {
    EnemyType boss_enemy_type = EnemyType::NONE;
    uint8_t area = l->area_for_floor(c->version(), c->floor);
    if (area == 0x0E) {
      // On Normal, Dark Falz does not have a third phase, so send the drop request after the end of the second phase.
      // On all other difficulty levels, send it after the third phase.
      if ((difficulty == Difficulty::NORMAL) && (flag_num == 0x0035)) {
        boss_enemy_type = EnemyType::DARK_FALZ_2;
      } else if ((difficulty != Difficulty::NORMAL) && (flag_num == 0x0037)) {
        boss_enemy_type = EnemyType::DARK_FALZ_3;
      }
    } else if ((flag_num == 0x0057) && (area == 0x1F)) {
      boss_enemy_type = EnemyType::OLGA_FLOW_2;
    }

    if (boss_enemy_type != EnemyType::NONE) {
      l->log.info_f("Creating item from final boss ({})", phosg::name_for_enum(boss_enemy_type));
      uint16_t enemy_index = 0xFFFF;
      try {
        auto ene_st = l->map_state->enemy_state_for_floor_type(c->version(), c->floor, boss_enemy_type);
        if (ene_st->alias_target_ene_st) {
          ene_st = ene_st->alias_target_ene_st;
        }
        enemy_index = l->map_state->index_for_enemy_state(c->version(), ene_st);
        if (c->floor != ene_st->super_ene->floor) {
          l->log.warning_f("Floor {:02X} from client does not match entity\'s expected floor {:02X}",
              c->floor, ene_st->super_ene->floor);
        }
        l->log.info_f("Found enemy E-{:03X} at index {:04X} on floor {:X}", ene_st->e_id, enemy_index, ene_st->super_ene->floor);
      } catch (const out_of_range&) {
        l->log.warning_f("Could not find enemy on floor {:X}; unable to determine enemy type", c->floor);
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
        G_StandardDropItemRequest_PC_V3_BB_6x60 drop_req = {
            {
                {0x60, 0x06, 0x0000},
                static_cast<uint8_t>(c->floor),
                type_definition_for_enemy(boss_enemy_type).rt_index,
                enemy_index == 0xFFFF ? 0x0B4F : enemy_index,
                pos,
                2,
                0,
            },
            area, {}};
        SubcommandMessage drop_msg{0x62, l->leader_id, &drop_req, sizeof(drop_req)};
        co_await on_entity_drop_item_request(c, drop_msg);
      }
    }
  }
}

static asio::awaitable<void> on_sync_quest_register(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  const auto& cmd = msg.check_size_t<G_SyncQuestRegister_6x77>();
  if (cmd.register_number >= 0x100) {
    throw runtime_error("invalid register number");
  }

  // If the lock status register is being written, change the game's flags to allow or forbid joining
  if (l->quest &&
      l->quest->meta.joinable &&
      (l->quest->meta.lock_status_register >= 0) &&
      (cmd.register_number == l->quest->meta.lock_status_register)) {
    // Lock if value is nonzero; unlock if value is zero
    if (cmd.value.as_int) {
      l->set_flag(Lobby::Flag::QUEST_IN_PROGRESS);
      l->clear_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
    } else {
      l->clear_flag(Lobby::Flag::QUEST_IN_PROGRESS);
      l->set_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
    }
  }

  bool should_forward = true;
  if (l->quest->meta.enable_schtserv_commands) {
    // We currently only implement one Schtserv server command here. There are likely many more which we don't support.

    if (cmd.register_number == 0xF0) {
      should_forward = false;
      c->schtserv_response_register = cmd.value.as_int;

    } else if ((cmd.register_number == 0xF1) && (cmd.value.as_int == 0x52455650)) {
      // PVER => respond with specific_version in schtserv's format
      should_forward = false;
      G_SyncQuestRegister_6x77 ret_cmd;
      ret_cmd.header.subcommand = 0x77;
      ret_cmd.header.size = sizeof(ret_cmd) / 4;
      ret_cmd.header.unused = 0;
      ret_cmd.register_number = c->schtserv_response_register;
      ret_cmd.value.as_int = is_v4(c->version()) ? 0x50 : c->sub_version;
      send_command_t(c, 0x60, 0x00, ret_cmd);
    }
  }

  if (should_forward) {
    forward_subcommand(c, msg);
  }
}

static asio::awaitable<void> on_set_entity_set_flag(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  const auto& cmd = msg.check_size_t<G_SetEntitySetFlags_6x76>();
  if (cmd.header.entity_id >= 0x4000) {
    try {
      auto obj_st = l->map_state->object_state_for_index(c->version(), cmd.floor, cmd.header.entity_id - 0x4000);
      obj_st->set_flags |= cmd.flags;
      l->log.info_f("Client set set flags {:04X} on K-{:03X} (flags are now {:04X})",
          cmd.flags, obj_st->k_id, cmd.flags);
    } catch (const out_of_range&) {
      l->log.warning_f("Flag update refers to missing object");
    }

  } else if (cmd.header.entity_id >= 0x1000) {
    int32_t room = -1;
    int32_t wave_number = -1;
    try {
      size_t enemy_index = cmd.header.entity_id - 0x1000;
      auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.floor, enemy_index);
      if (ene_st->super_ene->child_index > 0) {
        if (ene_st->super_ene->child_index > enemy_index) {
          throw logic_error("enemy\'s child index is greater than enemy\'s absolute index");
        }
        size_t parent_index = enemy_index - ene_st->super_ene->child_index;
        l->log.info_f("Client set set flags {:04X} on E-{:03X} but it is a child ({}); redirecting to E-{:X}",
            cmd.flags, ene_st->e_id, ene_st->super_ene->child_index, parent_index);
        ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.floor, parent_index);
      }
      ene_st->set_flags |= cmd.flags;
      const auto* set_entry = ene_st->super_ene->version(c->version()).set_entry;
      if (!set_entry) {
        // We should not have been able to look up this enemy if it didn't exist on this version
        throw logic_error("enemy does not exist on this game version");
      }
      room = set_entry->room;
      wave_number = set_entry->wave_number;
      l->log.info_f("Client set set flags {:04X} on E-{:03X} (flags are now {:04X})", cmd.flags, ene_st->e_id, cmd.flags);
    } catch (const out_of_range&) {
      l->log.warning_f("Flag update refers to missing enemy");
    }

    if ((room >= 0) && (wave_number >= 0)) {
      // When all enemies in a wave event have (set_flags & 8), which means they are defeated, set event_flags =
      // (event_flags | 0x18) & (~4), which means it is done and should not trigger again
      bool all_enemies_defeated = true;
      l->log.info_f("Checking for defeated enemies with room={:04X} wave_number={:04X}", room, wave_number);
      for (auto ene_st : l->map_state->enemy_states_for_floor_room_wave(c->version(), cmd.floor, room, wave_number)) {
        if (ene_st->super_ene->child_index) {
          l->log.info_f("E-{:03X} is a child of another enemy", ene_st->e_id);
        } else if (!(ene_st->set_flags & 8)) {
          l->log.info_f("E-{:03X} is not defeated; cannot advance event to finished state", ene_st->e_id);
          all_enemies_defeated = false;
          break;
        } else {
          l->log.info_f("E-{:03X} is defeated", ene_st->e_id);
        }
      }
      if (all_enemies_defeated) {
        l->log.info_f("All enemies defeated; setting events with room={:04X} wave_number={:04X} to finished state",
            room, wave_number);
        for (auto ev_st : l->map_state->event_states_for_floor_room_wave(c->version(), cmd.floor, room, wave_number)) {
          ev_st->flags = (ev_st->flags | 0x18) & (~4);
          l->log.info_f("Set flags on W-{:03X} to {:04X}", ev_st->w_id, ev_st->flags);

          const auto& ev_ver = ev_st->super_ev->version(c->version());
          phosg::StringReader actions_r(ev_ver.action_stream, ev_ver.action_stream_size);
          while (!actions_r.eof()) {
            uint8_t opcode = actions_r.get_u8();
            switch (opcode) {
              case 0x00: // nop
                l->log.info_f("(W-{:03X} script) nop", ev_st->w_id);
                break;
              case 0x01: // stop
                l->log.info_f("(W-{:03X} script) stop", ev_st->w_id);
                actions_r.go(actions_r.size());
                break;
              case 0x08: { // construct_objects
                uint16_t room = actions_r.get_u16l();
                uint16_t group = actions_r.get_u16l();
                l->log.info_f("(W-{:03X} script) construct_objects {:04X} {:04X}", ev_st->w_id, room, group);
                auto obj_sts = l->map_state->object_states_for_floor_room_group(
                    c->version(), ev_st->super_ev->floor, room, group);
                for (auto obj_st : obj_sts) {
                  if (!(obj_st->set_flags & 0x0A)) {
                    l->log.info_f("(W-{:03X} script)   Setting flags 0010 on object K-{:03X}", ev_st->w_id, obj_st->k_id);
                    obj_st->set_flags |= 0x10;
                  }
                }
                break;
              }
              case 0x09: // construct_enemies
              case 0x0D: { // construct_enemies_stop
                uint16_t room = actions_r.get_u16l();
                uint16_t wave_number = actions_r.get_u16l();
                l->log.info_f("(W-{:03X} script) construct_enemies {:04X} {:04X}", ev_st->w_id, room, wave_number);
                auto ene_sts = l->map_state->enemy_states_for_floor_room_wave(
                    c->version(), ev_st->super_ev->floor, room, wave_number);
                for (auto ene_st : ene_sts) {
                  if (!ene_st->super_ene->child_index && !(ene_st->set_flags & 0x0A)) {
                    l->log.info_f("(W-{:03X} script)   Setting flags 0002 on enemy set E-{:X}", ev_st->w_id, ene_st->e_id);
                    ene_st->set_flags |= 0x0002;
                  }
                }
                if (opcode == 0x0D) {
                  actions_r.go(actions_r.size());
                }
                break;
              }
              case 0x0A: // set_switch_flag
              case 0x0B: { // clear_switch_flag
                // These opcodes cause the client to send 6x05 commands, so we don't have to do anything here.
                uint16_t switch_flag_num = actions_r.get_u16l();
                l->log.info_f("(W-{:03X} script) {}able_switch_flag {:04X}",
                    ev_st->w_id, (opcode & 1) ? "dis" : "en", switch_flag_num);
                break;
              }
              case 0x0C: { // trigger_event
                // This opcode causes the client to send a 6x67 command, so we don't have to do anything here.
                uint32_t event_id = actions_r.get_u32l();
                l->log.info_f("(W-{:03X} script) trigger_event {:08X}", ev_st->w_id, event_id);
                break;
              }
              default:
                l->log.warning_f("(W-{:03X}) Invalid opcode {:02X} at offset {:X} in event action stream",
                    ev_st->w_id, opcode, actions_r.where() - 1);
                actions_r.go(actions_r.size());
            }
          }
        }
      }
    }
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_SetEntitySetFlags_6x76>(c, msg);
}

static asio::awaitable<void> on_trigger_set_event(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  const auto& cmd = msg.check_size_t<G_TriggerSetEvent_6x67>();
  auto event_sts = l->map_state->event_states_for_id(c->version(), cmd.floor, cmd.event_id);
  l->log.info_f("Client triggered set events with floor {:02X} and ID {:X} ({} events)",
      cmd.floor, cmd.event_id, event_sts.size());
  for (auto ev_st : event_sts) {
    ev_st->flags |= 0x04;
    if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
      send_text_message_fmt(c, "$C5W-{:03X} START", ev_st->w_id);
    }
  }

  forward_subcommand(c, msg);
}

static inline uint32_t bswap32_high16(uint32_t v) {
  return ((v >> 8) & 0x00FF0000) | ((v << 8) & 0xFF000000) | (v & 0x0000FFFF);
}

static asio::awaitable<void> on_update_telepipe_state(shared_ptr<Client> c, SubcommandMessage& msg) {
  if (c->lobby_client_id > 3) {
    throw logic_error("client ID is above 3");
  }
  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  auto& cmd = msg.check_size_t<G_SetTelepipeState_6x68>();
  c->telepipe_state = cmd.state;
  c->telepipe_lobby_id = l->lobby_id;

  // See the comments in G_SetTelepipeState_6x68 in CommandsFormats.hh for why we have to do this
  if (is_big_endian(c->version())) {
    c->telepipe_state.room_id = bswap32_high16(phosg::bswap32(c->telepipe_state.room_id));
  }

  for (auto lc : l->clients) {
    if (lc && (lc != c)) {
      if (is_big_endian(lc->version())) {
        cmd.state.room_id = phosg::bswap32(bswap32_high16(c->telepipe_state.room_id));
      } else {
        cmd.state.room_id = c->telepipe_state.room_id;
      }
      send_command_t(lc, 0x60, 0x00, cmd);
    }
  }
}

static asio::awaitable<void> on_update_enemy_state(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_UpdateEnemyState_DC_PC_XB_BB_6x0A>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }
  if (c->lobby_client_id > 3) {
    throw logic_error("client ID is above 3");
  }

  if ((cmd.enemy_index & 0xF000) || (cmd.header.entity_id != (cmd.enemy_index | 0x1000))) {
    throw runtime_error("mismatched enemy id/index");
  }
  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.enemy_index);
  uint32_t src_flags = is_big_endian(c->version()) ? bswap32(cmd.game_flags) : cmd.game_flags.load();
  if (l->difficulty == Difficulty::ULTIMATE) {
    src_flags = (src_flags & 0xFFFFFFC0) | (ene_st->game_flags & 0x0000003F);
  }
  ene_st->game_flags = src_flags;
  ene_st->total_damage = cmd.total_damage;
  ene_st->set_last_hit_by_client_id(c->lobby_client_id);
  l->log.info_f("E-{:03X} updated to damage={} game_flags={:08X}", ene_st->e_id, ene_st->total_damage, ene_st->game_flags);
  if (ene_st->alias_target_ene_st) {
    ene_st->alias_target_ene_st->game_flags = src_flags;
    ene_st->alias_target_ene_st->total_damage = cmd.total_damage;
    ene_st->alias_target_ene_st->set_last_hit_by_client_id(c->lobby_client_id);
    l->log.info_f("Alias target E-{:03X} updated to damage={} game_flags={:08X}",
        ene_st->alias_target_ene_st->e_id, ene_st->alias_target_ene_st->total_damage, ene_st->alias_target_ene_st->game_flags);
  }

  // TODO: It'd be nice if this worked on bosses too, but it seems we have to use each boss' specific state-syncing
  // command, or the cutscenes misbehave. Just setting flag 0x800 does work on Vol Opt (and the various parts), but
  // doesn't work on other Episode 1 bosses. Other episodes' bosses are not yet tested.
  bool is_fast_kill = c->check_flag(Client::Flag::FAST_KILLS_ENABLED) &&
      !type_definition_for_enemy(ene_st->super_ene->type).is_boss() &&
      !(ene_st->game_flags & 0x00000800);
  if (is_fast_kill) {
    ene_st->game_flags |= 0x00000800;
  }

  for (auto lc : l->clients) {
    if (lc && (is_fast_kill || (lc != c))) {
      cmd.enemy_index = l->map_state->index_for_enemy_state(lc->version(), ene_st);
      if (cmd.enemy_index != 0xFFFF) {
        cmd.header.entity_id = 0x1000 | cmd.enemy_index;
        cmd.game_flags = is_big_endian(lc->version()) ? phosg::bswap32(ene_st->game_flags) : ene_st->game_flags;
        send_command_t(lc, 0x60, 0x00, cmd);
      }
    }
  }
}

static asio::awaitable<void> on_incr_enemy_damage(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_IncrementEnemyDamage_Extension_6xE4>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }
  if (cmd.header.entity_id < 0x1000 || cmd.header.entity_id >= 0x4000) {
    throw runtime_error("6xE4 received for non-enemy entity");
  }
  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.header.entity_id & 0x0FFF);

  c->log.info_f("E-{:03X} damage incremented by {} with factor {}; before hit, damage was {} (cmd) or {} (ene_st) and HP was {}/{}",
      ene_st->e_id,
      cmd.hit_amount.load(),
      cmd.factor.load(),
      ene_st->total_damage,
      cmd.total_damage_before_hit.load(),
      cmd.current_hp_before_hit.load(),
      cmd.max_hp.load());
  ene_st->total_damage = std::min<uint32_t>(ene_st->total_damage + cmd.hit_amount, cmd.max_hp);
  if (ene_st->alias_target_ene_st) {
    ene_st->alias_target_ene_st->total_damage = std::min<uint32_t>(
        ene_st->alias_target_ene_st->total_damage + cmd.hit_amount, cmd.max_hp);
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_IncrementEnemyDamage_Extension_6xE4>(c, msg);
}

static asio::awaitable<void> on_set_enemy_low_game_flags_ultimate(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_SetEnemyLowGameFlagsUltimate_6x9C>();

  if (command_is_private(msg.command) ||
      (cmd.header.entity_id < 0x1000) ||
      (cmd.header.entity_id >= 0x4000) ||
      (cmd.low_game_flags & 0xFFFFFFC0) ||
      (c->lobby_client_id > 3)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game() || (l->difficulty != Difficulty::ULTIMATE)) {
    co_return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.header.entity_id - 0x1000);
  if (!(ene_st->game_flags & cmd.low_game_flags)) {
    ene_st->game_flags |= cmd.low_game_flags;
    l->log.info_f("E-{:03X} updated to game_flags={:08X}", ene_st->e_id, ene_st->game_flags);
  }
  if (ene_st->alias_target_ene_st && !(ene_st->alias_target_ene_st->game_flags & cmd.low_game_flags)) {
    ene_st->alias_target_ene_st->game_flags |= cmd.low_game_flags;
    l->log.info_f("Alias E-{:03X} updated to game_flags={:08X}",
        ene_st->alias_target_ene_st->e_id, ene_st->alias_target_ene_st->game_flags);
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_SetEnemyLowGameFlagsUltimate_6x9C>(c, msg);
}

template <typename CmdT>
static asio::awaitable<void> on_update_object_state_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<CmdT>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  auto obj_st = l->map_state->object_state_for_index(c->version(), cmd.object_index);
  obj_st->game_flags = cmd.flags;
  l->log.info_f("K-{:03X} updated with game_flags={:08X}", obj_st->k_id, obj_st->game_flags);

  if ((cmd.object_index & 0xF000) || (cmd.header.entity_id != (cmd.object_index | 0x4000))) {
    throw runtime_error("mismatched object id/index");
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

static asio::awaitable<void> on_update_attackable_col_state(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_UpdateAttackableColState_6x91>();
  if ((cmd.object_index & 0xF000) || ((cmd.object_index | 0x4000) != cmd.header.entity_id)) {
    throw runtime_error("incorrect object IDs in 6x91 command");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  if (l->switch_flags &&
      (cmd.should_set == 1) &&
      (cmd.switch_flag_num < 0x100) &&
      (cmd.floor < 0x12) &&
      (cmd.header.entity_id >= 0x4000) &&
      (cmd.header.entity_id != 0xFFFF)) {
    l->switch_flags->set(cmd.floor, cmd.switch_flag_num);
  }

  co_await on_update_object_state_t<G_UpdateAttackableColState_6x91>(c, msg);
}

static asio::awaitable<void> on_activate_timed_switch(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SetSwitchFlagFromTimer_6x93>();
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }
  if (l->switch_flags) {
    if (cmd.should_set == 1) {
      l->switch_flags->set(cmd.switch_flag_floor, cmd.switch_flag_num);
    } else {
      l->switch_flags->clear(cmd.switch_flag_floor, cmd.switch_flag_num);
    }
  }
  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_battle_scores(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_BattleScores_6x7F>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  G_BattleScoresBE_6x7F sw_cmd;
  sw_cmd.header.subcommand = 0x7F;
  sw_cmd.header.size = cmd.header.size;
  sw_cmd.header.unused = 0;
  for (size_t z = 0; z < 4; z++) {
    auto& sw_entry = sw_cmd.entries[z];
    const auto& entry = cmd.entries[z];
    sw_entry.client_id = entry.client_id;
    sw_entry.place = entry.place;
    sw_entry.score = entry.score;
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

static asio::awaitable<void> on_dragon_actions_6x12(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_DragonBossActions_DC_PC_XB_BB_6x12>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.header.entity_id - 0x1000);
  if (ene_st->super_ene->type != EnemyType::DRAGON) {
    throw runtime_error("6x12 command sent for incorrect enemy type");
  }
  if (ene_st->alias_target_ene_st) {
    throw runtime_error("DRAGON enemy is an alias");
  }

  G_DragonBossActions_GC_6x12 sw_cmd = {{cmd.header.subcommand, cmd.header.size, cmd.header.entity_id.load()},
      cmd.phase.load(), cmd.unknown_a3.load(), cmd.target_client_id.load(), cmd.x.load(), cmd.z.load()};
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

static asio::awaitable<void> on_gol_dragon_actions(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_GolDragonBossActions_XB_BB_6xA8>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.header.entity_id - 0x1000);
  if (ene_st->super_ene->type != EnemyType::GOL_DRAGON) {
    throw runtime_error("6xA8 command sent for incorrect enemy type");
  }
  if (ene_st->alias_target_ene_st) {
    throw runtime_error("GOL_DRAGON enemy is an alias");
  }

  G_GolDragonBossActions_GC_6xA8 sw_cmd = {{cmd.header.subcommand, cmd.header.size, cmd.header.entity_id},
      cmd.unknown_a2.load(),
      cmd.unknown_a3.load(),
      cmd.unknown_a4.load(),
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

template <typename CmdT>
static asio::awaitable<void> on_vol_opt_actions_t(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<CmdT>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  if (cmd.entity_index_count > 6) {
    throw runtime_error("invalid 6x16/6x84 command");
  }
  for (size_t z = 0; z < cmd.entity_index_table.size(); z++) {
    if (cmd.entity_index_table[z] >= 6) {
      throw runtime_error("invalid 6x16/6x84 command");
    }
  }

  co_await forward_subcommand_with_entity_id_transcode_t<CmdT>(c, msg);
}

static asio::awaitable<void> on_set_entity_pos_and_angle_6x17(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_SetEntityPositionAndAngle_6x17>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  // 6x17 is used to transport players to the other part of the Vol Opt boss arena, so phase 2 can begin. We only allow
  // 6x17 in the Monitor Room (Vol Opt arena).
  if (l->area_for_floor(c->version(), c->floor) != 0x0D) {
    throw runtime_error("client sent 6x17 command in area other than Vol Opt");
  }

  // If the target is on a different floor or does not exist, just drop the command instead of raising; this could have
  // been due to a data race
  if (cmd.header.entity_id < 0x1000) {
    auto target = l->clients.at(cmd.header.entity_id);
    if (!target || target->floor != c->floor) {
      co_return;
    }
    target->pos = cmd.pos;
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_SetEntityPositionAndAngle_6x17>(c, msg);
}

static asio::awaitable<void> on_set_boss_warp_flags_6x6A(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto& cmd = msg.check_size_t<G_SetBossWarpFlags_6x6A>();

  if (command_is_private(msg.command)) {
    co_return;
  }
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }
  if (cmd.header.entity_id < 0x4000) {
    throw runtime_error("6x6A sent for non-object entity");
  }

  auto obj_st = l->map_state->object_state_for_index(c->version(), cmd.header.entity_id - 0x4000);
  if (!obj_st->super_obj) {
    throw runtime_error("missing object for 6x6A command");
  }
  auto set_entry = obj_st->super_obj->version(c->version()).set_entry;
  if (!set_entry) {
    throw runtime_error("missing set entry for 6x6A command");
  }
  if (set_entry->base_type != 0x0019 && set_entry->base_type != 0x0055) {
    throw runtime_error("incorrect object type for 6x6A command");
  }

  co_await forward_subcommand_with_entity_id_transcode_t<G_SetBossWarpFlags_6x6A>(c, msg);
}

static asio::awaitable<void> on_charge_attack_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC7 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_ChargeAttack_BB_6xC7>();
  auto& disp = c->character_file()->disp;
  if (cmd.meseta_amount > disp.stats.meseta) {
    disp.stats.meseta = 0;
  } else {
    disp.stats.meseta -= cmd.meseta_amount;
  }
  co_return;
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

  auto p = c->character_file();
  if (p->disp.stats.level == max_level) {
    string name = p->disp.name.decode(c->language());
    size_t level_for_str = max_level + 1;
    string message = std::format("$C6{}$C7\nGC: {}\nhas reached Level $C6{}",
        name, c->login->account->account_id, level_for_str);
    string bb_message = std::format("$C6{}$C7 (GC: {}) has reached Level $C6{}",
        name, c->login->account->account_id, level_for_str);
    for (auto& it : s->game_server->all_clients()) {
      if ((it != c) && it->login && !is_patch(it->version()) && it->lobby.lock()) {
        send_text_or_scrolling_message(it, message, bb_message);
      }
    }
  }
}

static asio::awaitable<void> on_level_up(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }

  // On the DC prototypes, this command doesn't include any stats - it just increments the player's level by 1.
  auto p = c->character_file();
  if (is_pre_v1(c->version())) {
    msg.check_size_t<G_ChangePlayerLevel_DCNTE_6x30>();
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
    const auto& cmd = msg.check_size_t<G_ChangePlayerLevel_6x30>();
    p->disp.stats.char_stats.atp = cmd.atp;
    p->disp.stats.char_stats.mst = cmd.mst;
    p->disp.stats.char_stats.evp = cmd.evp;
    p->disp.stats.char_stats.hp = cmd.hp;
    p->disp.stats.char_stats.dfp = cmd.dfp;
    p->disp.stats.char_stats.ata = cmd.ata;
    p->disp.stats.level = cmd.level;
  }

  send_max_level_notification_if_needed(c);
  forward_subcommand(c, msg);
}

static void add_player_exp(shared_ptr<Client> c, uint32_t exp, uint16_t from_enemy_id) {
  auto s = c->require_server_state();
  auto p = c->character_file();

  p->disp.stats.experience += exp;
  if (c->version() == Version::BB_V4) {
    send_give_experience(c, exp, from_enemy_id);
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
    shared_ptr<const Quest> quest, // Null in free play
    EnemyType enemy_type,
    Episode current_episode,
    Difficulty difficulty,
    uint8_t floor,
    bool is_solo) {
  if (quest) {
    try {
      return quest->meta.enemy_exp_overrides.at(QuestMetadata::exp_override_key(difficulty, floor, enemy_type));
    } catch (const out_of_range&) {
    }
  }

  // Always try the current episode first. If the current episode is Ep4, try Ep1 next if in Crater and Ep2 next if in
  // Desert (this mirrors the logic in BB Patch Project's omnispawn patch).
  array<Episode, 3> episode_order;
  episode_order[0] = current_episode;
  if (current_episode == Episode::EP1) {
    episode_order[1] = Episode::EP2;
    episode_order[2] = Episode::EP4;
  } else if (current_episode == Episode::EP2) {
    episode_order[1] = Episode::EP1;
    episode_order[2] = Episode::EP4;
  } else if (current_episode == Episode::EP4) {
    uint8_t area = quest
        ? quest->meta.floor_assignments.at(floor).area
        : SetDataTableBase::default_floor_to_area(Version::BB_V4, Episode::EP4).at(floor);
    if (area <= 0x28) { // Crater
      episode_order[1] = Episode::EP1;
      episode_order[2] = Episode::EP2;
    } else { // Desert
      episode_order[1] = Episode::EP2;
      episode_order[2] = Episode::EP1;
    }
  } else {
    throw runtime_error("invalid episode");
  }

  for (const auto& episode : episode_order) {
    try {
      const auto& bp_table = bp_index->get_table(is_solo, episode);
      uint32_t bp_index = type_definition_for_enemy(enemy_type).bp_index;
      return bp_table.stats_for_index(difficulty, bp_index).experience;
    } catch (const out_of_range&) {
    }
  }
  throw runtime_error(std::format(
      "no base exp is available (type={}, episode={}, difficulty={}, floor={:02X}, solo={})",
      phosg::name_for_enum(enemy_type),
      name_for_episode(current_episode),
      name_for_difficulty(difficulty),
      floor,
      is_solo ? "true" : "false"));
}

static asio::awaitable<void> on_steal_exp_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC6 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC6 command sent in non-game lobby");
  }

  auto s = c->require_server_state();
  const auto& cmd = msg.check_size_t<G_StealEXP_BB_6xC6>();

  auto p = c->character_file();
  if (c->character_file()->disp.stats.level >= 199) {
    co_return;
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.enemy_index);
  if (ene_st->alias_target_ene_st) {
    ene_st = ene_st->alias_target_ene_st;
  }
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
    co_return;
  }

  uint8_t area = l->area_for_floor(c->version(), ene_st->super_ene->floor);
  Episode episode = episode_for_area(area);
  auto type = ene_st->type(c->version(), area, l->difficulty, l->event);
  uint32_t enemy_exp = base_exp_for_enemy_type(
      s->battle_params, l->quest, type, episode, l->difficulty, ene_st->super_ene->floor, l->mode == GameMode::SOLO);

  // Note: The original code checks if special.type is 9, 10, or 11, and skips applying the android bonus if so. We
  // don't do anything for those special types, so we don't check for that here.
  float percent = special.amount + ((l->difficulty == Difficulty::ULTIMATE) && char_class_is_android(p->disp.visual.char_class) ? 30 : 0);
  float ep2_factor = (episode == Episode::EP2) ? 1.3 : 1.0;
  uint32_t stolen_exp = max<uint32_t>(min<uint32_t>((enemy_exp * percent * ep2_factor) / 100.0f, (static_cast<size_t>(l->difficulty) + 1) * 20), 1);
  if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info_f("Stolen EXP from E-{:03X} with enemy_exp={} percent={:g} stolen_exp={}",
        ene_st->e_id, enemy_exp, percent, stolen_exp);
    send_text_message_fmt(c, "$C5+{} E-{:03X} {}", stolen_exp, ene_st->e_id, phosg::name_for_enum(type));
  }
  add_player_exp(c, stolen_exp, cmd.enemy_index | 0x1000);
}

static asio::awaitable<void> on_enemy_exp_request_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  const auto& cmd = msg.check_size_t<G_EnemyEXPRequest_BB_6xC8>();

  if (!l->is_game()) {
    throw runtime_error("client should not kill enemies outside of games");
  }
  if (c->lobby_client_id > 3) {
    throw runtime_error("client ID is too large");
  }

  auto ene_st = l->map_state->enemy_state_for_index(c->version(), cmd.enemy_index);
  string ene_str = ene_st->super_ene->str();
  c->log.info_f("EXP requested for E-{:03X}: {}", ene_st->e_id, ene_str);
  if (ene_st->alias_target_ene_st) {
    c->log.info_f("E-{:03X} is an alias for E-{:03X}", ene_st->e_id, ene_st->alias_target_ene_st->e_id);
    ene_st = ene_st->alias_target_ene_st;
  }

  // If the requesting player never hit this enemy, they are probably cheating; ignore the command. Also, each player
  // sends a 6xC8 if they ever hit the enemy; we only react to the first 6xC8 for each enemy (and give all relevant
  // players EXP then, if they deserve it).
  if (!ene_st->ever_hit_by_client_id(c->lobby_client_id) ||
      (ene_st->server_flags & MapState::EnemyState::Flag::EXP_GIVEN)) {
    l->log.info_f("EXP already given for this enemy; ignoring request");
    co_return;
  }
  ene_st->server_flags |= MapState::EnemyState::Flag::EXP_GIVEN;

  uint8_t area = l->area_for_floor(c->version(), ene_st->super_ene->floor);
  Episode episode = episode_for_area(area);
  auto type = ene_st->type(c->version(), area, l->difficulty, l->event);
  double base_exp = base_exp_for_enemy_type(
      s->battle_params, l->quest, type, episode, l->difficulty, ene_st->super_ene->floor, l->mode == GameMode::SOLO);
  l->log.info_f("Base EXP for this enemy ({}) is {:g}", phosg::name_for_enum(type), base_exp);

  for (size_t client_id = 0; client_id < 4; client_id++) {
    auto lc = l->clients[client_id];
    if (!lc) {
      l->log.info_f("No client in slot {}", client_id);
      continue;
    }
    if (lc->version() != Version::BB_V4) {
      // EXP is handled on the client side in all non-BB versions
      l->log.info_f("Client in slot {} is not BB", client_id);
      continue;
    }

    if (base_exp != 0.0) {
      // If this player killed the enemy, they get full EXP; if they tagged the enemy, they get 80% EXP; if auto EXP
      // share is enabled and they are close enough to the monster, they get a smaller share; if none of these
      // situations apply, they get no EXP. In Battle and Challenge modes, if a quest is loaded, EXP share is disabled.
      float exp_share_multiplier = (((l->mode == GameMode::BATTLE) || (l->mode == GameMode::CHALLENGE)) && l->quest)
          ? 0.0f
          : l->exp_share_multiplier;
      double rate_factor;
      if (lc->character_file()->disp.stats.level >= 199) {
        rate_factor = 0.0;
        l->log.info_f("Client in slot {} is level 200 and cannot receive EXP", client_id);
      } else if (ene_st->last_hit_by_client_id(client_id)) {
        rate_factor = max<double>(1.0, exp_share_multiplier);
        l->log.info_f("Client in slot {} killed this enemy; EXP rate is {:g}", client_id, rate_factor);
      } else if (ene_st->ever_hit_by_client_id(client_id)) {
        rate_factor = max<double>(0.8, exp_share_multiplier);
        l->log.info_f("Client in slot {} tagged this enemy; EXP rate is {:g}", client_id, rate_factor);
      } else if (lc->floor == ene_st->super_ene->floor) {
        rate_factor = max<double>(0.0, exp_share_multiplier);
        l->log.info_f("Client in slot {} shared this enemy; EXP rate is {:g}", client_id, rate_factor);
      } else {
        rate_factor = 0.0;
        l->log.info_f("Client in slot {} is not near this enemy; EXP rate is {:g}", client_id, rate_factor);
      }

      if (rate_factor > 0.0) {
        // In PSOBB, Sega decided to add a 30% EXP boost for Episode 2. They could have done something reasonable, like
        // edit the BattleParamEntry files so the monsters would all give more EXP, but they did something far lazier
        // instead: they just stuck an if statement in the client's EXP request function. We, unfortunately, have to do
        // the same thing here.
        uint32_t player_exp = base_exp *
            rate_factor *
            l->base_exp_multiplier *
            l->challenge_exp_multiplier *
            ((episode == Episode::EP2) ? 1.3 : 1.0);
        l->log.info_f("Client in slot {} receives {} EXP", client_id, player_exp);
        if (lc->check_flag(Client::Flag::DEBUG_ENABLED)) {
          send_text_message_fmt(lc, "$C5+{} E-{:03X} {}", player_exp, ene_st->e_id, phosg::name_for_enum(type));
        }
        add_player_exp(lc, player_exp, cmd.enemy_index | 0x1000);
      }
    }

    // Update kill counts on unsealable items, but only for the player who actually killed the enemy
    if (ene_st->last_hit_by_client_id(client_id)) {
      auto& inventory = lc->character_file()->inventory;
      for (size_t z = 0; z < inventory.num_items; z++) {
        auto& item = inventory.items[z];
        if ((item.flags & 0x08) && s->item_parameter_table(lc->version())->is_unsealable_item(item.data)) {
          item.data.set_kill_count(item.data.get_kill_count() + 1);
        }
      }
    }
  }
}

static asio::awaitable<void> on_adjust_player_meseta_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_AdjustPlayerMeseta_BB_6xC9>();

  auto p = c->character_file();
  if (cmd.amount < 0) {
    if (-cmd.amount > static_cast<int32_t>(p->disp.stats.meseta)) {
      p->disp.stats.meseta = 0;
    } else {
      p->disp.stats.meseta += cmd.amount;
    }
  } else if (cmd.amount > 0) {
    auto s = c->require_server_state();
    auto l = c->require_lobby();

    ItemData item;
    item.data1[0] = 0x04;
    item.data2d = cmd.amount;
    item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(item, *s->item_stack_limits(c->version()));
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
  co_return;
}

static void assert_quest_item_create_allowed(shared_ptr<const Lobby> l, const ItemData& item) {
  // We always enforce these restrictions if the quest has any restrictions defined, even if the client has cheat mode
  // enabled or has debug enabled. If the client can cheat, there are much easier ways to create items (e.g. the $item
  // chat command) than spoofing these quest item creation commands, so they should just do that instead.

  if (!l->quest) {
    throw std::runtime_error("cannot create quest reward item with no quest loaded");
  }
  if (l->quest->meta.create_item_mask_entries.empty()) {
    l->log.warning_f("Player created quest item {}, but the loaded quest ({}) has no item creation masks", item.hex(), l->quest->meta.name);
    return;
  }

  for (const auto& mask : l->quest->meta.create_item_mask_entries) {
    if (mask.match(item)) {
      l->log.info_f("Player created quest item {} which matches create item mask {}", item.hex(), mask.str());
      return;
    }
  }
  l->log.warning_f("Player attempted to create quest item {}, but it does not match any create item mask", item.hex());
  l->log.info_f("Quest has {} create item masks:", l->quest->meta.create_item_mask_entries.size());
  for (const auto& mask : l->quest->meta.create_item_mask_entries) {
    l->log.info_f("  {}", mask.str());
  }
  throw std::runtime_error("invalid item creation from quest");
}

static asio::awaitable<void> on_quest_create_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_QuestCreateItem_BB_6xCA>();
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  const auto& limits = *s->item_stack_limits(c->version());

  ItemData item;
  item = cmd.item_data;
  // enforce_stack_size_limits must come after this assert since quests may attempt to create stackable items with a
  // count of zero
  assert_quest_item_create_allowed(l, item);
  item.enforce_stack_size_limits(limits);
  item.id = l->generate_item_id(c->lobby_client_id);

  // The logic for the item_create and item_create2 quest opcodes (B3 and B4) includes a precondition check to see if
  // the player can actually add the item to their inventory or not, and the entire command is skipped if not. However,
  // on BB, the implementation performs this check and sends a 6xCA command instead - the item is not immediately added
  // to the inventory, and is instead added when the server sends back a 6xBE command. So if a quest creates multiple
  // items in quick succession, there may be another 6xCA/6xBE sequence in flight, and the client's check if an item
  // can be created may pass when a 6xBE command that would make it fail is already on the way from the server. To
  // handle this, we simply ignore any 6xCA command if the item can't be created.
  try {
    c->character_file()->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
    if (l->log.should_log(phosg::LogLevel::L_INFO)) {
      auto name = s->describe_item(c->version(), item);
      l->log.info_f("Player {} created inventory item {:08X} ({}) via quest command",
          c->lobby_client_id, item.id, name);
      c->print_inventory();
    }

  } catch (const out_of_range&) {
    if (l->log.should_log(phosg::LogLevel::L_INFO)) {
      auto name = s->describe_item(c->version(), item);
      l->log.info_f("Player {} attempted to create inventory item {:08X} ({}) via quest command, but it cannot be placed in their inventory",
          c->lobby_client_id, item.id, name);
    }
  }
  co_return;
}

asio::awaitable<void> on_transfer_item_via_mail_message_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_TransferItemViaMailMessage_BB_6xCB>();

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
  auto p = c->character_file();
  const auto& limits = *s->item_stack_limits(c->version());
  auto item = p->remove_item(cmd.item_id, cmd.amount, limits);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} sent inventory item {}:{:08X} ({}) x{} to player {:08X}",
        c->lobby_client_id, cmd.header.client_id, cmd.item_id, name, cmd.amount, cmd.target_guild_card_number);
    c->print_inventory();
  }

  // To receive an item, the player must be online, using BB, have a character loaded (that is, be in a lobby or game),
  // not be at the bank counter at the moment, and there must be room in their bank to receive the item.
  bool item_sent = false;
  auto target_c = s->find_client(nullptr, cmd.target_guild_card_number);
  if (target_c &&
      (target_c->version() == Version::BB_V4) &&
      (target_c->character_file(false) != nullptr) &&
      !target_c->check_flag(Client::Flag::AT_BANK_COUNTER)) {
    try {
      target_c->bank_file()->add_item(item, limits);
      item_sent = true;
    } catch (const runtime_error&) {
    }
  }

  if (item_sent) {
    // See the comment in the 6xCC handler about why we do this. Similar to that case, the 6xCB handler on the client
    // side does exactly the same thing as 6x29, but 6x29 is backward-compatible with other versions and 6xCB is not.
    G_DeleteInventoryItem_6x29 cmd29 = {{0x29, 0x03, cmd.header.client_id}, cmd.item_id, cmd.amount};
    SubcommandMessage delete_item_msg{msg.command, msg.flag, &cmd29, sizeof(cmd29)};
    forward_subcommand(c, delete_item_msg);
    send_command(c, 0x16EA, 0x00000001);
  } else {
    send_command(c, 0x16EA, 0x00000000);
    // If the item failed to send, add it back to the sender's inventory
    item.id = l->generate_item_id(0xFF);
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }
  co_return;
}

static asio::awaitable<void> on_exchange_item_for_team_points_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_ExchangeItemForTeamPoints_BB_6xCC>();

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
  auto p = c->character_file();
  const auto& limits = *s->item_stack_limits(c->version());
  auto item = p->remove_item(cmd.item_id, cmd.amount, limits);
  size_t amount = item.stack_size(limits);

  size_t points = s->item_parameter_table(Version::BB_V4)->get_item_team_points(item);
  s->team_index->add_member_points(c->login->account->account_id, points * amount);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} exchanged inventory item {}:{:08X} ({}) x{} for {} * {} = {} team points",
        c->lobby_client_id, cmd.header.client_id, cmd.item_id, name, amount, points, amount, points * amount);
    c->print_inventory();
  }

  // The original implementation forwarded the 6xCC command to all other clients. However, the handler does exactly the
  // same thing as 6x29 if the affected client isn't the local client. Since the sender has already processed the 6xCC
  // that they sent by the time we receive this, we pretend that they sent 6x29 instead and send that to the others in
  // the game.
  G_DeleteInventoryItem_6x29 cmd29 = {{0x29, 0x03, cmd.header.client_id}, cmd.item_id, cmd.amount};
  SubcommandMessage delete_item_msg{msg.command, msg.flag, &cmd29, sizeof(cmd29)};
  forward_subcommand(c, delete_item_msg);
  co_return;
}

static asio::awaitable<void> on_destroy_inventory_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_DeleteInventoryItem_6x29>();

  auto l = c->require_lobby();
  if (!l->is_game()) {
    co_return;
  }
  if (cmd.header.client_id != c->lobby_client_id) {
    co_return;
  }

  auto s = c->require_server_state();
  auto p = c->character_file();
  auto item = p->remove_item(cmd.item_id, cmd.amount, *s->item_stack_limits(c->version()));

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} destroyed inventory item {}:{:08X} ({})",
        c->lobby_client_id, cmd.header.client_id, cmd.item_id, name);
    c->print_inventory();
  }
  forward_subcommand(c, msg);
}

static asio::awaitable<void> on_destroy_floor_item(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_DestroyFloorItem_6x5C_6x63>();

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
    co_return;
  }

  auto s = c->require_server_state();
  shared_ptr<Lobby::FloorItem> fi;
  try {
    fi = l->remove_item(cmd.floor, cmd.item_id, 0xFF);
  } catch (const out_of_range&) {
  }

  if (!fi) {
    // There are generally two data races that could occur here. Either the player attempted to evict the item at the
    // same time the server did (that is, the client's and server's 6x63 commands crossed paths on the network), or the
    // player attempted to evict an item that was already picked up. The former case is easy to handle; we can just
    // ignore the command. The latter case is more difficult - we have to know which player picked up the item and send
    // a 6x2B command to the sender, to sync their item state with the server's again. We can't just look through the
    // players' inventories to find the item ID, since item IDs can be destroyed when stackable items or Meseta are
    // picked up.
    // TODO: We don't actually handle the evict/pickup conflict case. This case is probably quite rare, but we should
    // eventually handle it.
    l->log.info_f("Player {} attempted to destroy floor item {:08X}, but it is missing",
        c->lobby_client_id, cmd.item_id);

  } else {
    auto name = s->describe_item(c->version(), fi->data);
    l->log.info_f("Player {} destroyed floor item {:08X} ({})", c->lobby_client_id, cmd.item_id, name);

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
          send_command_t(lc, msg.command, msg.flag, out_cmd);
        } else {
          send_command_t(lc, msg.command, msg.flag, cmd);
        }
      }
    }
  }
}

static asio::awaitable<void> on_identify_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xDA command sent in non-game lobby");
  }

  if (c->version() == Version::BB_V4) {
    const auto& cmd = msg.check_size_t<G_IdentifyItemRequest_6xB8>();
    if (!l->is_game() || l->episode == Episode::EP3) {
      co_return;
    }

    auto p = c->character_file();
    size_t x = p->inventory.find_item(cmd.item_id);
    if (p->inventory.items[x].data.data1[0] != 0) {
      throw runtime_error("non-weapon items cannot be unidentified");
    }

    // It seems the client expects an item ID to be consumed here, even though the returned item has the same ID as the
    // original item. Perhaps this was not the case on Sega's original server, and the returned item had a new ID
    // instead.
    l->generate_item_id(c->lobby_client_id);
    p->disp.stats.meseta -= 100;
    c->bb_identify_result = p->inventory.items[x].data;
    c->bb_identify_result.data1[4] &= 0x7F;
    uint8_t effective_section_id = l->effective_section_id();
    if (effective_section_id >= 10) {
      throw std::runtime_error("effective section ID is not valid");
    }
    l->item_creator->apply_tekker_deltas(c->bb_identify_result, effective_section_id);
    send_item_identify_result(c);

  } else {
    forward_subcommand(c, msg);
  }
}

static asio::awaitable<void> on_accept_identify_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("6xB5 command sent in non-game lobby");
  }

  if (is_ep3(c->version())) {
    forward_subcommand(c, msg);

  } else if (c->version() == Version::BB_V4) {
    const auto& cmd = msg.check_size_t<G_AcceptItemIdentification_BB_6xBA>();

    if (!c->bb_identify_result.id || (c->bb_identify_result.id == 0xFFFFFFFF)) {
      throw runtime_error("no identify result present");
    }
    if (c->bb_identify_result.id != cmd.item_id) {
      throw runtime_error("accepted item ID does not match previous identify request");
    }
    auto s = c->require_server_state();
    c->character_file()->add_item(c->bb_identify_result, *s->item_stack_limits(c->version()));
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, c->bb_identify_result);
    c->bb_identify_result.clear();
  }
  co_return;
}

static asio::awaitable<void> on_sell_item_at_shop_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC0 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC0 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_SellItemAtShop_BB_6xC0>();

  auto s = c->require_server_state();
  auto p = c->character_file();
  auto item = p->remove_item(cmd.item_id, cmd.amount, *s->item_stack_limits(c->version()));
  size_t price = (s->item_parameter_table(c->version())->price_for_item(item) >> 3) * cmd.amount;
  p->add_meseta(price);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} sold inventory item {:08X} ({}) for {} Meseta",
        c->lobby_client_id, cmd.item_id, name, price);
    c->print_inventory();
  }

  forward_subcommand(c, msg);
  co_return;
}

static asio::awaitable<void> on_buy_shop_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xB7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xB7 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_BuyShopItem_BB_6xB7>();
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
  auto p = c->character_file();
  p->remove_meseta(price, false);

  item.id = cmd.shop_item_id;
  l->on_item_id_generated_externally(item.id);
  p->add_item(item, limits);
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, item, true);

  if (l->log.should_log(phosg::LogLevel::L_INFO)) {
    auto s = c->require_server_state();
    auto name = s->describe_item(c->version(), item);
    l->log.info_f("Player {} purchased item {:08X} ({}) for {} meseta", c->lobby_client_id, item.id, name, price);
    c->print_inventory();
  }
  co_return;
}

static asio::awaitable<void> on_medical_center_bb(shared_ptr<Client> c, SubcommandMessage&) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xC5 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xC5 command sent in non-game lobby");
  }

  c->character_file()->remove_meseta(10, false);
  co_return;
}

static asio::awaitable<void> on_battle_restart_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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
  const auto& cmd = msg.check_size_t<G_StartBattle_BB_6xCF>();

  auto new_rules = make_shared<BattleRules>(cmd.rules);
  l->item_creator->set_restrictions(new_rules);

  for (auto& lc : l->clients) {
    if (lc) {
      lc->delete_overlay();
      if (is_v4(lc->version())) {
        lc->change_bank(lc->bb_character_index);
      }
      lc->create_battle_overlay(new_rules, s->level_table(c->version()));
    }
  }
  l->map_state->reset();
  co_return;
}

static asio::awaitable<void> on_battle_level_up_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_BattleModeLevelUp_BB_6xD0>();
  auto lc = l->clients.at(cmd.header.client_id);
  if (lc) {
    auto s = c->require_server_state();
    auto lp = lc->character_file();
    uint32_t target_level = min<uint32_t>(lp->disp.stats.level + cmd.num_levels, 199);
    uint32_t before_exp = lp->disp.stats.experience;
    int32_t exp_delta = lp->disp.stats.experience - before_exp;
    if (exp_delta > 0) {
      s->level_table(lc->version())->advance_to_level(lp->disp.stats, target_level, lp->disp.visual.char_class);
      if (lc->version() == Version::BB_V4) {
        send_give_experience(lc, exp_delta, 0xFFFF);
        send_level_up(lc);
      }
    }
  }
  co_return;
}

static asio::awaitable<void> on_request_challenge_grave_recovery_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_ChallengeModeGraveRecoveryItemRequest_BB_6xD1>();
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
  co_return;
}

static asio::awaitable<void> on_challenge_mode_retry_or_quit(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SelectChallengeModeFailureOption_6x97>();

  auto l = c->require_lobby();
  auto leader_c = l->clients.at(l->leader_id);
  if (leader_c != c) {
    throw runtime_error("6x97 sent by non-leader");
  }

  if (l->is_game() && (cmd.is_retry == 1) && l->quest && (l->quest->meta.challenge_template_index >= 0)) {
    auto s = l->require_server_state();

    for (auto& m : l->floor_item_managers) {
      m.clear();
    }

    // If the leader (c) is BB, they are expected to send 02DF later, which will recreate the overlays.
    if (!is_v4(c->version())) {
      for (auto lc : l->clients) {
        if (lc) {
          if (is_v4(lc->version())) {
            lc->change_bank(lc->bb_character_index);
          }
          lc->create_challenge_overlay(lc->version(), l->quest->meta.challenge_template_index, s->level_table(c->version()));
          lc->log.info_f("Created challenge overlay");
          l->assign_inventory_and_bank_item_ids(lc, true);
        }
      }
    }

    l->map_state->reset();
  }

  forward_subcommand(c, msg);
  co_return;
}

static asio::awaitable<void> on_challenge_update_records(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->lobby.lock();
  if (!l) {
    c->log.warning_f("Not in any lobby; dropping command");
    co_return;
  }

  const auto& cmd = msg.check_size_t<G_SetChallengeRecordsBase_6x7C>(0xFFFF);
  if (cmd.client_id != c->lobby_client_id) {
    co_return;
  }

  auto p = c->character_file(true, false);
  Version c_version = c->version();
  switch (c_version) {
    case Version::DC_V2:
    case Version::GC_NTE: {
      const auto& cmd = msg.check_size_t<G_SetChallengeRecords_DC_6x7C>();
      p->challenge_records = cmd.records;
      break;
    }
    case Version::PC_V2: {
      const auto& cmd = msg.check_size_t<G_SetChallengeRecords_PC_6x7C>();
      p->challenge_records = cmd.records;
      break;
    }
    case Version::GC_V3:
    case Version::XB_V3: {
      const auto& cmd = msg.check_size_t<G_SetChallengeRecords_V3_6x7C>();
      p->challenge_records = cmd.records;
      break;
    }
    case Version::BB_V4: {
      const auto& cmd = msg.check_size_t<G_SetChallengeRecords_BB_6x7C>();
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
      data_to_send = msg.data;
      size_to_send = msg.size;
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
      lc->log.info_f("Command cannot be translated to client\'s version");
    } else {
      send_command(lc, msg.command, msg.flag, data_to_send, size_to_send);
    }
  };

  if (command_is_private(msg.command)) {
    if (msg.flag >= l->max_clients) {
      co_return;
    }
    auto target = l->clients[msg.flag];
    if (!target) {
      co_return;
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

static asio::awaitable<void> on_update_battle_data_6x7D(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->lobby.lock();
  if (!l) {
    c->log.warning_f("Not in any lobby; dropping command");
    co_return;
  }

  const auto& cmd = msg.check_size_t<G_SetBattleModeData_6x7D>(0xFFFF);
  if ((cmd.what == 3 || cmd.what == 4) && cmd.params[0] >= 4) {
    throw runtime_error("invalid client ID in 6x7D command");
  }

  co_await on_forward_check_game(c, msg);
}

static asio::awaitable<void> on_quest_exchange_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_QuestExchangeItem_BB_6xD5>();
  auto s = c->require_server_state();

  try {
    auto p = c->character_file();
    const auto& limits = *s->item_stack_limits(c->version());

    ItemData new_item = cmd.replace_item;
    assert_quest_item_create_allowed(l, new_item);
    new_item.enforce_stack_size_limits(limits);

    size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, limits);
    send_destroy_item_to_lobby(c, found_item.id, 1);

    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning_f("Quest item exchange failed: {}", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
  co_return;
}

static asio::awaitable<void> on_wrap_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD6 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD6 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_WrapItem_BB_6xD6>();
  auto s = c->require_server_state();

  auto p = c->character_file();
  auto item = p->remove_item(cmd.item.id, 1, *s->item_stack_limits(c->version()));
  send_destroy_item_to_lobby(c, item.id, 1);
  item.wrap(*s->item_stack_limits(c->version()), cmd.present_color);
  p->add_item(item, *s->item_stack_limits(c->version()));
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  co_return;
}

static asio::awaitable<void> on_photon_drop_exchange_for_item_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD7 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD7 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_PaganiniPhotonDropExchange_BB_6xD7>();
  auto s = c->require_server_state();

  try {
    auto p = c->character_file();
    const auto& limits = *s->item_stack_limits(c->version());

    ItemData new_item = cmd.new_item;
    assert_quest_item_create_allowed(l, new_item);
    new_item.enforce_stack_size_limits(limits);

    size_t found_index = p->inventory.find_item_by_primary_identifier(0x03100000);
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 0, limits);
    send_destroy_item_to_lobby(c, found_item.id, found_item.stack_size(limits));

    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning_f("Quest Photon Drop exchange for item failed: {}", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
  co_return;
}

static asio::awaitable<void> on_photon_drop_exchange_for_s_rank_special_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xD8 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xD8 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_AddSRankWeaponSpecial_BB_6xD8>();
  auto s = c->require_server_state();
  const auto& limits = *s->item_stack_limits(c->version());

  try {
    auto p = c->character_file();

    static const array<uint8_t, 0x10> costs({60, 60, 20, 20, 30, 30, 30, 50, 40, 50, 40, 40, 50, 40, 40, 40});
    uint8_t cost = costs.at(cmd.special_type);

    size_t payment_item_index = p->inventory.find_item_by_primary_identifier(0x03100000);
    {
      const auto& item = p->inventory.items[p->inventory.find_item(cmd.item_id)];
      if (!item.data.is_s_rank_weapon()) {
        throw std::runtime_error("6xD8 cannot be used for non-ES weapons");
      }
    }

    auto payment_item = p->remove_item(p->inventory.items[payment_item_index].data.id, cost, limits);
    send_destroy_item_to_lobby(c, payment_item.id, cost);

    auto item = p->remove_item(cmd.item_id, 1, limits);
    send_destroy_item_to_lobby(c, item.id, cost);
    item.data1[2] = cmd.special_type;
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);

    send_quest_function_call(c, cmd.success_label);

  } catch (const exception& e) {
    c->log.warning_f("Quest Photon Drop exchange for S-rank special failed: {}", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
  co_return;
}

static asio::awaitable<void> on_secret_lottery_ticket_exchange_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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
  if (!l->quest) {
    throw runtime_error("6xDE command sent with no quest loaded");
  }
  if (l->quest->meta.create_item_mask_entries.size() < 2) {
    throw runtime_error("quest does not have enough create item mask entries");
  }

  // See notes about 6xDE in CommandFormats.hh about this weirdness
  const auto& cmd = msg.check_size_t<G_ExchangeSecretLotteryTicket_Incomplete_BB_6xDE>(0x0C);
  uint16_t failure_label;
  if (msg.size >= 0x0C) {
    const auto& cmd = msg.check_size_t<G_ExchangeSecretLotteryTicket_BB_6xDE>();
    failure_label = cmd.failure_label;
  } else {
    failure_label = cmd.success_label;
  }

  // The last mask entry is the currency item (e.g. Secret Lottery Ticket)
  const auto& currency_mask = l->quest->meta.create_item_mask_entries.back();
  uint32_t currency_primary_identifier = currency_mask.primary_identifier();
  auto p = c->character_file();
  ssize_t currency_index = -1;
  try {
    currency_index = p->inventory.find_item_by_primary_identifier(currency_primary_identifier);
    c->log.info_f("Currency item {:08X} found at index {}", currency_primary_identifier, currency_index);
  } catch (const out_of_range&) {
    c->log.info_f("Currency item {:08X} not found in inventory", currency_primary_identifier);
  }

  S_ExchangeSecretLotteryTicketResult_BB_24 out_cmd;
  out_cmd.start_reg_num = cmd.start_reg_num;
  out_cmd.label = (currency_index >= 0) ? cmd.success_label.load() : failure_label;
  for (size_t z = 0; z < out_cmd.reg_values.size(); z++) {
    out_cmd.reg_values[z] = (l->rand_crypt->next() % (l->quest->meta.create_item_mask_entries.size() - 1)) + 1;
    c->log.info_f("Mask index {} is {} ({})", z, out_cmd.reg_values[z] - 1, l->quest->meta.create_item_mask_entries[out_cmd.reg_values[z] - 1].str());
  }

  if (currency_index >= 0) {
    size_t mask_index = out_cmd.reg_values[cmd.index - 1] - 1;
    const auto& mask = l->quest->meta.create_item_mask_entries[mask_index];
    c->log.info_f("Chose mask {} ({})", mask_index, mask.str());

    ItemData item;
    for (size_t z = 0; z < 12; z++) {
      const auto& r = mask.data1_ranges[z];
      if (r.min != r.max) {
        throw std::runtime_error("invalid range for bb_exchange_slt");
      }
      item.data1[z] = r.min;
    }
    auto s = c->require_server_state();
    const auto& limits = *s->item_stack_limits(c->version());
    item.enforce_stack_size_limits(limits);

    uint32_t slt_item_id = p->inventory.items[currency_index].data.id;

    G_ExchangeItemInQuest_BB_6xDB exchange_cmd;
    exchange_cmd.header.subcommand = 0xDB;
    exchange_cmd.header.size = 4;
    exchange_cmd.header.client_id = c->lobby_client_id;
    exchange_cmd.unknown_a1 = 1;
    exchange_cmd.item_id = slt_item_id;
    exchange_cmd.amount = 1;
    send_command_t(c, 0x60, 0x00, exchange_cmd);

    p->remove_item(slt_item_id, 1, limits);

    item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  }

  send_command_t(c, 0x24, (currency_index >= 0) ? 0 : 1, out_cmd);
  co_return;
}

static asio::awaitable<void> on_photon_crystal_exchange_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  msg.check_size_t<G_ExchangePhotonCrystals_BB_6xDF>();
  auto s = c->require_server_state();
  auto p = c->character_file();
  size_t index = p->inventory.find_item_by_primary_identifier(0x03100200);
  auto item = p->remove_item(p->inventory.items[index].data.id, 1, *s->item_stack_limits(c->version()));
  send_destroy_item_to_lobby(c, item.id, 1);
  l->drop_mode = ServerDropMode::DISABLED;
  l->allowed_drop_modes = (1 << static_cast<uint8_t>(l->drop_mode)); // DISABLED only
  co_return;
}

static asio::awaitable<void> on_quest_F95E_result_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_RequestItemDropFromQuest_BB_6xE0>();
  auto s = c->require_server_state();

  size_t count = (cmd.type > 0x03) ? 1 : (static_cast<size_t>(l->difficulty) + 1);
  for (size_t z = 0; z < count; z++) {
    const auto& results = s->quest_F95E_results.at(cmd.type).at(static_cast<size_t>(l->difficulty));
    if (results.empty()) {
      throw runtime_error("invalid result type");
    }
    ItemData item = (results.size() == 1) ? results[0] : results[l->rand_crypt->next() % results.size()];
    if (item.data1[0] == 0x04) { // Meseta
      // TODO: What is the right amount of Meseta to use here? Presumably it should be random within a certain range,
      // but it's not obvious what that range should be.
      item.data2d = 100;
    } else if (item.data1[0] == 0x00) {
      item.data1[4] |= 0x80; // Unidentified
    } else {
      item.enforce_stack_size_limits(*s->item_stack_limits(c->version()));
    }

    item.id = l->generate_item_id(0xFF);
    l->add_item(cmd.floor, item, cmd.pos, nullptr, nullptr, 0x100F);

    send_drop_stacked_item_to_lobby(l, item, cmd.floor, cmd.pos);
  }
  co_return;
}

static asio::awaitable<void> on_quest_F95F_result_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_ExchangePhotonTickets_BB_6xE1>();
  auto s = c->require_server_state();
  auto p = c->character_file();

  const auto& result = s->quest_F95F_results.at(cmd.result_index);
  if (result.second.empty()) {
    throw runtime_error("invalid result index");
  }

  const auto& limits = *s->item_stack_limits(c->version());
  size_t index = p->inventory.find_item_by_primary_identifier(0x03100400); // Photon Ticket
  auto ticket_item = p->remove_item(p->inventory.items[index].data.id, result.first, limits);
  // TODO: Shouldn't we send a 6x29 here? Check if this causes desync in an actual game

  G_ExchangeItemInQuest_BB_6xDB cmd_6xDB;
  cmd_6xDB.header = {0xDB, 0x04, c->lobby_client_id};
  cmd_6xDB.unknown_a1 = 1;
  cmd_6xDB.item_id = ticket_item.id;
  cmd_6xDB.amount = result.first;
  send_command_t(c, 0x60, 0x00, cmd_6xDB);

  ItemData new_item = result.second;
  new_item.enforce_stack_size_limits(limits);
  new_item.id = l->generate_item_id(c->lobby_client_id);
  p->add_item(new_item, limits);
  send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

  S_GallonPlanResult_BB_25 out_cmd;
  out_cmd.label = cmd.success_label;
  out_cmd.reg_num1 = 0x3C;
  out_cmd.reg_num2 = 0x08;
  out_cmd.value1 = 0x00;
  out_cmd.value2 = cmd.result_index;
  send_command_t(c, 0x25, 0x00, out_cmd);
  co_return;
}

static asio::awaitable<void> on_quest_F960_result_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw runtime_error("6xE2 command sent by non-BB client");
  }
  if (!l->is_game()) {
    throw runtime_error("6xE2 command sent in non-game lobby");
  }

  const auto& cmd = msg.check_size_t<G_GetMesetaSlotPrize_BB_6xE2>();
  auto s = c->require_server_state();
  auto p = c->character_file();

  time_t t_secs = phosg::now() / 1000000;
  struct tm t_parsed;
#ifndef PHOSG_WINDOWS
  gmtime_r(&t_secs, &t_parsed);
#else
  gmtime_s(&t_parsed, &t_secs);
#endif
  size_t weekday = t_parsed.tm_wday;

  ItemData item;
  for (size_t num_failures = 0; num_failures <= cmd.result_tier; num_failures++) {
    size_t tier = cmd.result_tier - num_failures;
    const auto& results = s->quest_F960_success_results.at(tier);
    uint64_t probability = results.base_probability + num_failures * results.probability_upgrade;
    if (l->rand_crypt->next() <= probability) {
      c->log.info_f("Tier {} yielded a prize", tier);
      const auto& result_items = results.results.at(weekday);
      item = result_items[l->rand_crypt->next() % result_items.size()];
      break;
    } else {
      c->log.info_f("Tier {} did not yield a prize", tier);
    }
  }
  if (item.empty()) {
    c->log.info_f("Choosing result from failure tier");
    const auto& result_items = s->quest_F960_failure_results.results.at(weekday);
    item = result_items[l->rand_crypt->next() % result_items.size()];
  }
  if (item.empty()) {
    throw runtime_error("no item produced, even from failure tier");
  }

  // The client sends a 6xC9 to remove Meseta before sending 6xE2, so we don't have to deal with Meseta here.

  item.id = l->generate_item_id(c->lobby_client_id);
  // If it's a weapon, make it unidentified
  auto item_parameter_table = s->item_parameter_table(c->version());
  if ((item.data1[0] == 0x00) && (item_parameter_table->is_item_rare(item) || (item.data1[4] != 0))) {
    item.data1[4] |= 0x80;
  }

  // The 6xE3 handler on the client fails if the item already exists, so we need to send 6xE3 before we call
  // send_create_inventory_item_to_lobby.
  G_SetMesetaSlotPrizeResult_BB_6xE3 cmd_6xE3 = {
      {0xE3, sizeof(G_SetMesetaSlotPrizeResult_BB_6xE3) >> 2, c->lobby_client_id}, item};
  send_command_t(c, 0x60, 0x00, cmd_6xE3);

  // Add the item to the player's inventory if possible; if not, drop it on the floor where the player is standing
  bool added_to_inventory;
  try {
    p->add_item(item, *s->item_stack_limits(c->version()));
    added_to_inventory = true;
  } catch (const out_of_range&) {
    // If the game's drop mode is private or duplicate, make the item visible only to this player; in other modes, make
    // it visible to everyone
    uint16_t flags = ((l->drop_mode == ServerDropMode::SERVER_PRIVATE) || (l->drop_mode == ServerDropMode::SERVER_DUPLICATE))
        ? (1 << c->lobby_client_id)
        : 0x000F;
    l->add_item(c->floor, item, cmd.pos, nullptr, nullptr, flags);
    added_to_inventory = false;
  }

  if (c->log.should_log(phosg::LogLevel::L_INFO)) {
    string name = s->describe_item(c->version(), item);
    c->log.info_f("Awarded item {} {}", name, added_to_inventory ? "in inventory" : "on ground (inventory is full)");
  }
  if (added_to_inventory) {
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, item);
  } else {
    send_drop_item_to_channel(s, c->channel, item, 0, cmd.floor, cmd.pos, 0xFFFF);
  }
  co_return;
}

static asio::awaitable<void> on_momoka_item_exchange_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  // See notes in CommandFormats.hh about why we allow larger commands here
  const auto& cmd = msg.check_size_t<G_MomokaItemExchange_BB_6xD9>(0xFFFF);
  auto s = c->require_server_state();
  auto p = c->character_file();
  try {
    const auto& limits = *s->item_stack_limits(c->version());

    ItemData new_item = cmd.replace_item;
    assert_quest_item_create_allowed(l, new_item);
    new_item.enforce_stack_size_limits(limits);

    size_t found_index = p->inventory.find_item_by_primary_identifier(cmd.find_item.primary_identifier());
    auto found_item = p->remove_item(p->inventory.items[found_index].data.id, 1, limits);

    G_ExchangeItemInQuest_BB_6xDB cmd_6xDB = {{0xDB, 0x04, c->lobby_client_id}, 1, found_item.id, 1};
    send_command_t(c, 0x60, 0x00, cmd_6xDB);

    send_destroy_item_to_lobby(c, found_item.id, 1);

    new_item.id = l->generate_item_id(c->lobby_client_id);
    p->add_item(new_item, limits);
    send_create_inventory_item_to_lobby(c, c->lobby_client_id, new_item);

    send_command(c, 0x23, 0x00);
  } catch (const exception& e) {
    c->log.warning_f("Momoka item exchange failed: {}", e.what());
    send_command(c, 0x23, 0x01);
  }
  co_return;
}

static asio::awaitable<void> on_upgrade_weapon_attribute_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
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

  const auto& cmd = msg.check_size_t<G_UpgradeWeaponAttribute_BB_6xDA>();
  auto s = c->require_server_state();
  auto p = c->character_file();
  try {
    size_t item_index = p->inventory.find_item(cmd.item_id);
    auto& item = p->inventory.items[item_index].data;
    if (item.is_s_rank_weapon()) {
      throw std::runtime_error("6xDA command sent for ES weapon");
    }

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
    } else if (cmd.payment_type == 0 && cmd.payment_count == 20) {
      attribute_amount = 5;
    } else {
      throw runtime_error("unknown PD/PS expenditure");
    }

    size_t attribute_index = 0;
    for (size_t z = 6; z <= (item.has_kill_count() ? 10 : 8); z += 2) {
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
    c->log.warning_f("Weapon attribute upgrade failed: {}", e.what());
    send_quest_function_call(c, cmd.failure_label);
  }
  co_return;
}

static asio::awaitable<void> on_write_quest_counter_bb(shared_ptr<Client> c, SubcommandMessage& msg) {
  const auto& cmd = msg.check_size_t<G_SetQuestCounter_BB_6xD2>();
  c->character_file()->quest_counters[cmd.index] = cmd.value;
  co_return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This makes it easier to see which handlers exist on which prototypes via syntax highlighting
constexpr uint8_t NONE = 0x00;

const vector<SubcommandDefinition> subcommand_definitions{
    // {DC NTE, 11/2000, all other versions, handler}
    /* 6x00 */ {0x00, 0x00, 0x00, on_invalid},
    /* 6x01 */ {0x01, 0x01, 0x01, on_invalid},
    /* 6x02 */ {0x02, 0x02, 0x02, forward_subcommand_m},
    /* 6x03 */ {0x03, 0x03, 0x03, forward_subcommand_m},
    /* 6x04 */ {0x04, 0x04, 0x04, forward_subcommand_m},
    /* 6x05 */ {0x05, 0x05, 0x05, on_switch_state_changed},
    /* 6x06 */ {0x06, 0x06, 0x06, on_send_guild_card},
    /* 6x07 */ {0x07, 0x07, 0x07, on_symbol_chat, SDF::ALWAYS_FORWARD_TO_WATCHERS},
    /* 6x08 */ {0x08, 0x08, 0x08, on_invalid},
    /* 6x09 */ {0x09, 0x09, 0x09, on_invalid}, // See notes in CommandFormats.hh
    /* 6x0A */ {0x0A, 0x0A, 0x0A, on_update_enemy_state},
    /* 6x0B */ {0x0B, 0x0B, 0x0B, on_update_object_state_t<G_UpdateObjectState_6x0B>},
    /* 6x0C */ {0x0C, 0x0C, 0x0C, on_received_condition},
    /* 6x0D */ {NONE, NONE, 0x0D, on_forward_check_game},
    /* 6x0E */ {NONE, NONE, 0x0E, forward_subcommand_with_entity_id_transcode_t<G_ClearNegativeStatusEffects_6x0E>},
    /* 6x0F */ {NONE, NONE, 0x0F, on_invalid},
    /* 6x10 */ {0x0E, 0x0E, 0x10, forward_subcommand_with_entity_id_transcode_t<G_DragonBossActions_6x10_6x11>},
    /* 6x11 */ {0x0F, 0x0F, 0x11, forward_subcommand_with_entity_id_transcode_t<G_DragonBossActions_6x10_6x11>},
    /* 6x12 */ {0x10, 0x10, 0x12, on_dragon_actions_6x12},
    /* 6x13 */ {0x11, 0x11, 0x13, forward_subcommand_with_entity_id_transcode_t<G_DeRolLeBossActions_6x13>},
    /* 6x14 */ {0x12, 0x12, 0x14, forward_subcommand_with_entity_id_transcode_t<G_DeRolLeBossActionsWithTarget_6x14>},
    /* 6x15 */ {0x13, 0x13, 0x15, forward_subcommand_with_entity_id_transcode_t<G_VolOptBossActions_6x15>},
    /* 6x16 */ {0x14, 0x14, 0x16, on_vol_opt_actions_t<G_VolOptBossActions_6x16>},
    /* 6x17 */ {0x15, 0x15, 0x17, on_set_entity_pos_and_angle_6x17},
    /* 6x18 */ {0x16, 0x16, 0x18, forward_subcommand_with_entity_id_transcode_t<G_VolOpt2BossActions_6x18>},
    /* 6x19 */ {0x17, 0x17, 0x19, forward_subcommand_with_entity_id_transcode_t<G_DarkFalzActions_6x19>},
    /* 6x1A */ {NONE, NONE, 0x1A, on_invalid},
    /* 6x1B */ {NONE, 0x19, 0x1B, on_forward_check_game},
    /* 6x1C */ {NONE, 0x1A, 0x1C, on_forward_check_game},
    /* 6x1D */ {0x19, 0x1B, 0x1D, on_invalid},
    /* 6x1E */ {0x1A, 0x1C, 0x1E, on_invalid},
    /* 6x1F */ {0x1B, 0x1D, 0x1F, on_change_floor_6x1F},
    /* 6x20 */ {0x1C, 0x1E, 0x20, on_movement_xyz_with_floor<G_SetPosition_6x20>},
    /* 6x21 */ {0x1D, 0x1F, 0x21, on_change_floor_6x21},
    /* 6x22 */ {0x1E, 0x20, 0x22, on_forward_check_client},
    /* 6x23 */ {0x1F, 0x21, 0x23, on_set_player_visible},
    /* 6x24 */ {0x20, 0x22, 0x24, on_movement_xyz<G_TeleportPlayer_6x24>},
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
    /* 6x35 */ {0x30, NONE, 0x35, on_invalid},
    /* 6x36 */ {0x31, 0x32, 0x36, on_forward_check_game},
    /* 6x37 */ {0x32, 0x33, 0x37, on_forward_check_game},
    /* 6x38 */ {NONE, 0x34, 0x38, on_forward_check_game},
    /* NONE */ {0x33, 0x35, NONE, on_forward_check_game},
    /* 6x39 */ {NONE, 0x36, 0x39, on_forward_check_game},
    /* 6x3A */ {NONE, 0x37, 0x3A, on_forward_check_game},
    /* 6x3B */ {NONE, 0x38, 0x3B, forward_subcommand_m},
    /* 6x3C */ {0x34, 0x39, 0x3C, forward_subcommand_m},
    /* 6x3D */ {0x35, 0x3A, 0x3D, on_invalid},
    /* 6x3E */ {NONE, NONE, 0x3E, on_movement_xyz_with_floor<G_StopAtPosition_6x3E>},
    /* 6x3F */ {0x36, 0x3B, 0x3F, on_movement_xyz_with_floor<G_SetPosition_6x3F>},
    /* 6x40 */ {0x37, 0x3C, 0x40, on_movement_xz<G_WalkToPosition_6x40>},
    /* 6x41 */ {0x38, 0x3D, 0x41, on_movement_xz<G_MoveToPosition_6x41_6x42>},
    /* 6x42 */ {0x39, 0x3E, 0x42, on_movement_xz<G_MoveToPosition_6x41_6x42>},
    /* 6x43 */ {0x3A, 0x3F, 0x43, on_forward_check_game_client},
    /* 6x44 */ {0x3B, 0x40, 0x44, on_forward_check_game_client},
    /* 6x45 */ {0x3C, 0x41, 0x45, on_forward_check_game_client},
    /* 6x46 */ {NONE, 0x42, 0x46, forward_subcommand_with_entity_targets_transcode_t<G_AttackFinished_Header_6x46>},
    /* 6x47 */ {0x3D, 0x43, 0x47, forward_subcommand_with_entity_targets_transcode_t<G_CastTechnique_Header_6x47>},
    /* 6x48 */ {NONE, NONE, 0x48, on_cast_technique_finished},
    /* 6x49 */ {0x3E, 0x44, 0x49, forward_subcommand_with_entity_targets_transcode_t<G_ExecutePhotonBlast_Header_6x49>},
    /* 6x4A */ {0x3F, 0x45, 0x4A, on_change_hp<G_ClientIDHeader>},
    /* 6x4B */ {0x40, 0x46, 0x4B, on_change_hp<G_ClientIDHeader>},
    /* 6x4C */ {0x41, 0x47, 0x4C, on_change_hp<G_ClientIDHeader>},
    /* 6x4D */ {0x42, 0x48, 0x4D, on_player_died},
    /* 6x4E */ {NONE, NONE, 0x4E, on_player_revivable},
    /* 6x4F */ {0x43, 0x49, 0x4F, on_player_revived},
    /* 6x50 */ {0x44, 0x4A, 0x50, on_forward_check_game_client},
    /* 6x51 */ {0x45, 0x4B, 0x51, on_invalid},
    /* 6x52 */ {0x46, 0x4C, 0x52, on_set_animation_state},
    /* 6x53 */ {0x47, 0x4D, 0x53, on_forward_check_game},
    /* 6x54 */ {0x48, 0x4E, 0x54, forward_subcommand_m},
    /* 6x55 */ {0x49, 0x4F, 0x55, on_movement_xyz<G_IntraMapWarp_6x55>},
    /* 6x56 */ {0x4A, 0x50, 0x56, on_movement_xyz<G_SetPlayerPositionAndAngle_6x56>},
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
    /* 6x6A */ {0x5B, 0x62, 0x6A, on_set_boss_warp_flags_6x6A},
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
    /* 6x7D */ {NONE, NONE, 0x7D, on_update_battle_data_6x7D},
    /* 6x7E */ {NONE, NONE, 0x7E, forward_subcommand_m},
    /* 6x7F */ {NONE, NONE, 0x7F, on_battle_scores},
    /* 6x80 */ {NONE, NONE, 0x80, on_forward_check_game},
    /* 6x81 */ {NONE, NONE, 0x81, on_forward_check_game},
    /* 6x82 */ {NONE, NONE, 0x82, on_forward_check_game},
    /* 6x83 */ {NONE, NONE, 0x83, on_forward_check_game},
    /* 6x84 */ {NONE, NONE, 0x84, on_vol_opt_actions_t<G_VolOptBossActions_6x84>},
    /* 6x85 */ {NONE, NONE, 0x85, on_forward_check_game},
    /* 6x86 */ {NONE, NONE, 0x86, on_update_object_state_t<G_HitDestructibleObject_6x86>},
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
    /* 6x9C */ {NONE, NONE, 0x9C, on_set_enemy_low_game_flags_ultimate},
    /* 6x9D */ {NONE, NONE, 0x9D, on_forward_check_game},
    /* 6x9E */ {NONE, NONE, 0x9E, forward_subcommand_m},
    /* 6x9F */ {NONE, NONE, 0x9F, forward_subcommand_with_entity_id_transcode_t<G_GalGryphonBossActions_6x9F>},
    /* 6xA0 */ {NONE, NONE, 0xA0, forward_subcommand_with_entity_id_transcode_t<G_GalGryphonBossActions_6xA0>},
    /* 6xA1 */ {NONE, NONE, 0xA1, on_forward_check_game},
    /* 6xA2 */ {NONE, NONE, 0xA2, on_entity_drop_item_request},
    /* 6xA3 */ {NONE, NONE, 0xA3, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowBossActions_6xA3>},
    /* 6xA4 */ {NONE, NONE, 0xA4, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowBossActions_6xA4_6xA5>},
    /* 6xA5 */ {NONE, NONE, 0xA5, forward_subcommand_with_entity_id_transcode_t<G_OlgaFlowBossActions_6xA4_6xA5>},
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
    /* 6xCA */ {NONE, NONE, 0xCA, on_quest_create_item_bb},
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
    /* 6xE4 */ {NONE, NONE, 0xE4, on_incr_enemy_damage}, // Extended subcommand; see CommandFormats.hh
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
    /* 6xFF */ {NONE, NONE, 0xFF, on_debug_info}, // Extended subcommand with no format; used for debugging patches
};

asio::awaitable<void> on_subcommand_multi(shared_ptr<Client> c, Channel::Message& msg) {
  if (msg.data.empty()) {
    throw runtime_error("subcommand is empty");
  }

  size_t offset = 0;
  while (offset < msg.data.size()) {
    size_t cmd_size = 0;
    if (offset + sizeof(G_UnusedHeader) > msg.data.size()) {
      throw runtime_error("insufficient data remaining for next subcommand header");
    }
    const auto* header = reinterpret_cast<const G_UnusedHeader*>(msg.data.data() + offset);
    if (header->size != 0) {
      cmd_size = header->size << 2;
    } else {
      if (offset + sizeof(G_ExtendedHeaderT<G_UnusedHeader>) > msg.data.size()) {
        throw runtime_error("insufficient data remaining for next extended subcommand header");
      }
      const auto* ext_header = reinterpret_cast<const G_ExtendedHeaderT<G_UnusedHeader>*>(msg.data.data() + offset);
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
    void* cmd_data = msg.data.data() + offset;

    const auto* def = def_for_subcommand(c->version(), header->subcommand);
    SubcommandMessage sub_msg{.command = msg.command, .flag = msg.flag, .data = cmd_data, .size = cmd_size};
    co_await def->handler(c, sub_msg);
    offset += cmd_size;
  }
}
