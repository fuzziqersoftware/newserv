#include "ReplaySession.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "GameServer.hh"
#include "Loggers.hh"
#include "ReceiveSubcommands.hh"
#include "Server.hh"

static std::string encode_chat_message(Version version, const std::string& message) {
  std::string encoded_message;
  encoded_message.resize(8, 0);
  encoded_message += uses_utf16(version) ? tt_utf8_to_utf16("\tE" + message) : tt_utf8_to_ascii("\tE" + message);
  encoded_message.resize((encoded_message.size() + 3) & (~3));
  return prepend_command_header(version, true, 0x06, 0x00, encoded_message);
}

ReplaySession::Event::Event(Type type, size_t event_number, uint64_t client_id, size_t line_num)
    : type(type), event_number(event_number), client_id(client_id), complete(false), line_num(line_num) {}

std::string ReplaySession::Event::str() const {
  std::string ret;
  if (this->type == Type::CONNECT) {
    ret = std::format("Event[{}, CONNECT", this->client_id);
  } else if (this->type == Type::DISCONNECT) {
    ret = std::format("Event[{}, DISCONNECT", this->client_id);
  } else if (this->type == Type::SEND) {
    ret = std::format("Event[{}, SEND {:04X}", this->client_id, this->data.size());
  } else if (this->type == Type::RECEIVE) {
    ret = std::format("Event[{}, RECEIVE {:04X}", this->client_id, this->data.size());
  }
  if (this->complete) {
    ret += ", done";
  }
  ret += std::format(", ev-line {}]", this->line_num);
  return ret;
}

ReplaySession::Client::Client(std::shared_ptr<asio::io_context> io_context, uint64_t id, uint16_t port, Version version)
    : id(id),
      port(port),
      version(version),
      channel(std::make_shared<PeerChannel>(
          io_context,
          this->version,
          Language::ENGLISH,
          std::format("R-{:X}", this->id),
          phosg::TerminalFormat::END,
          phosg::TerminalFormat::END,
          false,
          false)) {}

std::string ReplaySession::Client::str() const {
  return std::format("Client[{}, T-{}, {}]", this->id, this->port, phosg::name_for_enum(this->version));
}

ReplaySession::Event& ReplaySession::create_event(Event::Type type, std::shared_ptr<Client> c, size_t line_num) {
  size_t event_number = this->events.size() + 1;
  auto& event = this->events.emplace(event_number, Event{type, event_number, c->id, line_num}).first->second;
  if (type == Event::Type::RECEIVE) {
    c->pending_receive_event_numbers.emplace_back(event_number);
  }
  return event;
}

template <typename CmdT>
std::string_view string_view_for_cmd(const CmdT& cmd) {
  return std::string_view{reinterpret_cast<const char*>(&cmd), sizeof(cmd)};
}

template <typename CmdT>
CmdT make_command_mask() {
  CmdT ret;
  memset(reinterpret_cast<void*>(&ret), 0xFF, sizeof(ret));
  return ret;
}

std::string_view generate_default_mask_02_patch() {
  static S_ServerInit_Patch_02 ret = make_command_mask<S_ServerInit_Patch_02>();
  ret.server_key = 0;
  ret.client_key = 0;
  return string_view_for_cmd(ret);
}

std::string_view default_mask_02_17_91_9B() {
  static S_ServerInitDefault_DC_PC_V3_02_17_91_9B ret = make_command_mask<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>();
  ret.server_key = 0;
  ret.client_key = 0;
  return string_view_for_cmd(ret);
}

std::string_view default_mask_03_bb() {
  static S_ServerInitDefault_BB_03_9B ret = make_command_mask<S_ServerInitDefault_BB_03_9B>();
  ret.server_key.clear(0);
  ret.client_key.clear(0);
  return string_view_for_cmd(ret);
}

std::string_view default_mask_E6_bb() {
  static S_ClientInit_BB_00E6 ret = make_command_mask<S_ClientInit_BB_00E6>();
  ret.security_token = 0;
  return string_view_for_cmd(ret);
}

std::string_view default_mask_19() {
  static S_ReconnectSplit_19 ret = make_command_mask<S_ReconnectSplit_19>();
  ret.pc_address = 0;
  ret.gc_address = 0;
  return string_view_for_cmd(ret);
}

template <typename CmdT>
std::string_view default_mask_41() {
  static CmdT ret = make_command_mask<CmdT>();
  ret.reconnect_command.address = 0;
  return string_view_for_cmd(ret);
}

template <typename CmdT>
std::string_view default_mask_64() {
  static CmdT ret = make_command_mask<CmdT>();
  ret.variations = Variations();
  if constexpr (requires { ret.random_seed; }) {
    ret.random_seed = 0;
  }
  if constexpr (requires { ret.players_ep3; }) {
    for (size_t z = 0; z < ret.players_ep3.size(); z++) {
      ret.players_ep3[z].disp.visual.sh.name_color_checksum = 0;
    }
  }
  return string_view_for_cmd(ret);
}

template <typename CmdT>
std::string_view default_mask_65_67_68() {
  static CmdT ret = make_command_mask<CmdT>();
  for (size_t z = 0; z < ret.entries.size(); z++) {
    ret.entries[z].disp.visual.sh.name_color_checksum = 0;
  }
  return string_view_for_cmd(ret);
}

std::string_view default_mask_E8_ep3() {
  static S_JoinSpectatorTeam_Ep3_E8 ret = make_command_mask<S_JoinSpectatorTeam_Ep3_E8>();
  ret.random_seed = 0;
  for (size_t z = 0; z < ret.players.size(); z++) {
    ret.players[z].disp.visual.sh.name_color_checksum = 0;
  }
  for (size_t z = 0; z < ret.spectator_players.size(); z++) {
    ret.spectator_players[z].disp.visual.sh.name_color_checksum = 0;
  }
  return string_view_for_cmd(ret);
}

template <typename CmdT>
std::string_view default_mask_C5() {
  static CmdT ret = make_command_mask<CmdT>();
  for (size_t z = 0; z < ret.size(); z++) {
    auto& entry = ret[z];
    for (size_t z = 0; z < entry.challenge.times_ep1_online.size(); z++) {
      entry.challenge.times_ep1_online[z].store_raw(0);
    }
    for (size_t z = 0; z < entry.challenge.times_ep1_offline.size(); z++) {
      entry.challenge.times_ep1_online[z].store_raw(0);
    }
    if constexpr (requires { entry.challenge.times_ep2_online; }) {
      for (size_t z = 0; z < entry.challenge.times_ep2_online.size(); z++) {
        entry.challenge.times_ep2_online[z].store_raw(0);
      }
    }
  }
  return string_view_for_cmd(ret);
}

std::string_view default_mask_B1() {
  static S_ServerTime_B1 ret;
  ret.time_str.clear(0);
  ret.time_flags_low = 0xFF;
  ret.time_flags_mid = 0xFF;
  ret.time_flags_high = 0xFF;
  return string_view_for_cmd(ret);
}

std::string_view default_mask_6xB4x46() {
  static G_ServerVersionStrings_Ep3_6xB4x46 ret = make_command_mask<G_ServerVersionStrings_Ep3_6xB4x46>();
  ret.version_signature.clear(0);
  ret.date_str1.clear(0);
  ret.date_str2.clear(0);
  return string_view_for_cmd(ret);
}

template <typename CmdT>
std::string_view default_mask_6x70() {
  static CmdT ret = make_command_mask<CmdT>();
  ret.visual.sh.name_color_checksum = 0;
  return string_view_for_cmd(ret);
}

void ReplaySession::check_receive_event(std::shared_ptr<Client> c, Event& ev, const std::string& received_data) const {
  constexpr uint64_t flags = phosg::FormatDataFlags::PRINT_ASCII | phosg::FormatDataFlags::OFFSET_16_BITS;

  if (c->pending_receive_event_numbers.empty()) {
    std::string data_str = phosg::format_data(received_data, 0, flags);
    throw std::runtime_error(std::format("Received unexpected command for client:\n{}", data_str));
  }
  if (c->pending_receive_event_numbers.front() != ev.event_number) {
    throw std::logic_error("Client receive events are out of order");
  }
  c->pending_receive_event_numbers.pop_front();

  size_t header_size = ((c->version == Version::BB_V4) ? 8 : 4);
  const std::string_view ev_data = std::string_view{ev.data}.substr(header_size);
  const std::string_view cmd_data = std::string_view{received_data}.substr(header_size);

  auto raise_failure = [&](const std::string& what, bool use_prev = true) -> void {
    std::string expected_data_str = phosg::format_data(ev.data, 0, flags);
    std::string received_data_str = use_prev
        ? phosg::format_data(received_data, 0, ev.data, phosg::FormatDataFlags::USE_COLOR | flags)
        : phosg::format_data(received_data, 0, flags);
    throw std::runtime_error(std::format("(ev-line {}) {}\nExpected command:\n{}\nReceived command:\n{}",
        ev.line_num, what, expected_data_str, received_data_str));
  };

  auto default_check = [&](std::string_view mask = {}) -> void {
    if (received_data.size() != ev.data.size()) {
      raise_failure("Received command sizes do not match", false);
    }
    for (size_t x = 0; x < std::min<size_t>(received_data.size(), ev.data.size()); x++) {
      // The mask applies only after the header
      uint8_t mask_value = ((x >= header_size) && (x < mask.size() + header_size)) ? mask[x - header_size] : 0xFF;
      if ((received_data[x] & mask_value) != (ev.data[x] & mask_value)) {
        raise_failure("Received command does not match expected");
      }
    }
  };

  switch (c->version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH: {
      const auto& header = check_size_t<PSOCommandHeaderPC>(ev.data, 0xFFFF);
      if (header.command == 0x02) {
        static const auto mask_cmd = default_mask_02_17_91_9B();
        return default_check(mask_cmd);
      } else {
        return default_check();
      }
      break;
    }
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      uint8_t command;
      if ((c->version == Version::PC_NTE) || (c->version == Version::PC_V2)) {
        command = check_size_t<PSOCommandHeaderPC>(ev.data, 0xFFFF).command;
      } else { // V3
        command = check_size_t<PSOCommandHeaderDCV3>(ev.data, 0xFFFF).command;
      }
      switch (command) {
        case 0x02:
        case 0x17:
        case 0x91:
        case 0x9B: {
          static const auto mask_cmd = default_mask_02_17_91_9B();
          return default_check(mask_cmd);
        }
        case 0x19: {
          static const auto mask_cmd = default_mask_19();
          return default_check(mask_cmd);
        }
        case 0x41:
          if ((c->version == Version::PC_NTE) || (c->version == Version::PC_V2)) {
            static const auto mask_cmd = default_mask_41<S_GuildCardSearchResult_PC_41>();
            return default_check(mask_cmd);
          } else {
            static const auto mask_cmd = default_mask_41<S_GuildCardSearchResult_DC_V3_41>();
            return default_check(mask_cmd);
          }
        case 0x64:
          if ((c->version == Version::PC_NTE) || (c->version == Version::PC_V2)) {
            static const auto mask_cmd = default_mask_64<S_JoinGame_PC_64>();
            return default_check(mask_cmd);
          } else if (c->version == Version::XB_V3) {
            static const auto mask_cmd = default_mask_64<S_JoinGame_XB_64>();
            return default_check(mask_cmd);
          } else if (c->version == Version::DC_NTE || c->version == Version::DC_11_2000) {
            static const auto mask_cmd = default_mask_64<S_JoinGame_DCNTE_64>();
            return default_check(mask_cmd);
          } else {
            static const auto mask_cmd = default_mask_64<S_JoinGame_Ep3_64>();
            return default_check(mask_cmd);
          }
        case 0xEB:
          if (!is_gc(c->version)) {
            return default_check();
          }
          [[fallthrough]];
        case 0x65:
        case 0x67:
        case 0x68: {
          if ((c->version == Version::PC_NTE) || (c->version == Version::PC_V2)) {
            static const auto mask_cmd = default_mask_65_67_68<S_JoinLobby_PC_65_67_68>();
            return default_check(mask_cmd);
          } else if (c->version == Version::XB_V3) {
            static const auto mask_cmd = default_mask_65_67_68<S_JoinLobby_XB_65_67_68>();
            return default_check(mask_cmd);
          } else if ((c->version == Version::DC_NTE) || (c->version == Version::DC_11_2000)) {
            static const auto mask_cmd = default_mask_65_67_68<S_JoinLobby_DCNTE_65_67_68>();
            return default_check(mask_cmd);
          } else {
            static const auto mask_cmd = default_mask_65_67_68<S_JoinLobby_DC_GC_65_67_68_Ep3_EB>();
            return default_check(mask_cmd);
          }
        }
        case 0xE8:
          if (is_gc(c->version)) {
            static const auto mask_cmd = default_mask_E8_ep3();
            return default_check(mask_cmd);
          } else {
            return default_check();
          }
        case 0xC5:
          if (c->version == Version::DC_V2) {
            static const auto mask_cmd = default_mask_C5<parray<PlayerRecordsEntry_DC, 12>>();
            return default_check(mask_cmd);
          } else if (is_v2(c->version)) {
            static const auto mask_cmd = default_mask_C5<parray<PlayerRecordsEntry_PC, 12>>();
            return default_check(mask_cmd);
          } else if (is_v3(c->version)) {
            static const auto mask_cmd = default_mask_C5<parray<PlayerRecordsEntry_V3, 12>>();
            return default_check(mask_cmd);
          } else if (is_v4(c->version)) {
            static const auto mask_cmd = default_mask_C5<parray<PlayerRecordsEntry_BB, 12>>();
            return default_check(mask_cmd);
          }
          break;
        case 0xB1: {
          static const auto mask_cmd = default_mask_B1();
          return default_check(mask_cmd);
        }
        case 0xC9: {
          static const auto mask_cmd = default_mask_6xB4x46();
          return default_check(mask_cmd);
        }
        case 0x6C:
          if (is_gc(c->version) && (ev_data.size() >= 0x14)) {
            const auto& cmd = check_size_t<G_MapList_Ep3_6xB6x40>(cmd_data, 0xFFFF);
            if ((cmd.header.header.basic_header.subcommand == 0xB6) && (cmd.header.subsubcommand == 0x40)) {
              // TODO: We can probably do something better here; for now we just check that the data is valid PRS
              phosg::StringReader r(cmd_data);
              r.skip(sizeof(cmd));
              uint32_t decompressed_size = r.get_u32b();
              if (prs_decompress_size(cmd_data.substr(r.where())) != decompressed_size) {
                raise_failure("Decompressed size for compressed map list is incorrect");
              }
              return;
            } else {
              return default_check();
            }
          } else {
            return default_check();
          }
        case 0x6D: {
          const auto& header = check_size_t<G_UnusedHeader>(ev_data, 0xFFFF);
          switch (translate_subcommand_number(Version::BB_V4, c->version, header.subcommand)) {
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E: {
              G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E ev_cmd;
              G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E received_cmd;
              std::string_view ev_bc0_data;
              std::string_view received_bc0_data;
              if (is_pre_v1(c->version)) {
                const auto& ev_header_nte = check_size_t<G_SyncGameStateHeader_DCProtos_6x6B_6x6C_6x6D_6x6E>(
                    ev_data, 0xFFFF);
                const auto& received_header_nte = check_size_t<G_SyncGameStateHeader_DCProtos_6x6B_6x6C_6x6D_6x6E>(
                    cmd_data, 0xFFFF);
                ev_cmd.header = ev_header_nte.header;
                ev_cmd.decompressed_size = ev_header_nte.decompressed_size;
                ev_cmd.compressed_size = ev_data.size() - sizeof(ev_header_nte);
                ev_bc0_data = ev_data.substr(sizeof(ev_header_nte));
                received_cmd.header = received_header_nte.header;
                received_cmd.decompressed_size = received_header_nte.decompressed_size;
                received_cmd.compressed_size = cmd_data.size() - sizeof(received_header_nte);
                received_bc0_data = cmd_data.substr(sizeof(received_header_nte));
              } else {
                ev_cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(ev_data, 0xFFFF);
                received_cmd = check_size_t<G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E>(cmd_data, 0xFFFF);
                ev_bc0_data = ev_data.substr(sizeof(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E));
                received_bc0_data = cmd_data.substr(sizeof(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E));
              }
              if (memcmp(&ev_cmd, &received_cmd, sizeof(ev_cmd))) {
                raise_failure("BC0-compressed headers do not match");
              }
              auto ev_decompressed_data = bc0_decompress(ev_bc0_data);
              auto received_decompressed_data = bc0_decompress(received_bc0_data);
              if (ev_decompressed_data != received_decompressed_data) {
                raise_failure("BC0-compressed sections do not decompress to the same data");
              }
              return;
            }
            case 0x70:
              if (c->version == Version::DC_NTE) {
                static const auto mask_cmd = default_mask_6x70<G_SyncPlayerDispAndInventory_DCNTE_6x70>();
                return default_check(mask_cmd);
              } else if (c->version == Version::DC_11_2000) {
                static const auto mask_cmd = default_mask_6x70<G_SyncPlayerDispAndInventory_DC112000_6x70>();
                return default_check(mask_cmd);
              } else {
                static const auto mask_cmd = default_mask_6x70<G_SyncPlayerDispAndInventory_DC_PC_6x70>();
                return default_check(mask_cmd);
              }
            default:
              return default_check();
          }
        }
        default:
          return default_check();
      }
      break;
    }
    case Version::BB_V4: {
      uint16_t command = check_size_t<PSOCommandHeaderBB>(ev.data, 0xFFFF).command;
      switch (command) {
        case 0x0003: {
          static const auto mask_cmd = default_mask_03_bb();
          return default_check(mask_cmd);
        }
        case 0x0019: {
          static const auto mask_cmd = default_mask_19();
          return default_check(mask_cmd);
        }
        case 0x0064: {
          static const auto mask_cmd = default_mask_64<S_JoinGame_BB_64>();
          return default_check(mask_cmd);
        }
        case 0x00B1: {
          static const auto mask_cmd = default_mask_B1();
          return default_check(mask_cmd);
        }
        case 0x00E6: {
          static const auto mask_cmd = default_mask_E6_bb();
          return default_check(mask_cmd);
        }
        default:
          return default_check();
      }
      break;
    }
    default:
      throw std::logic_error(std::format("(ev-line {}, {}) Invalid game version",
          ev.line_num, phosg::name_for_enum(c->version)));
  }

  throw std::logic_error(std::format("(ev-line {}, {}) Command was not checked",
      ev.line_num, phosg::name_for_enum(c->version)));
}

ReplaySession::ReplaySession(std::shared_ptr<ServerState> state, FILE* input_log)
    : state(state),
      commands_sent(0),
      bytes_sent(0),
      commands_received(0),
      bytes_received(0),
      idle_timeout_timer(*this->state->io_context) {
  Event* parsing_command = nullptr;

  size_t line_num = 0;
  while (!feof(input_log)) {
    line_num++;
    std::string line = phosg::fgets(input_log);
    if (line.ends_with("\n")) {
      line.resize(line.size() - 1);
    }
    if (line.empty()) {
      continue;
    }

    if (parsing_command) {
      std::string expected_start = std::format("{:04X} |", parsing_command->data.size());
      if (line.starts_with(expected_start)) {
        // Parse out the hex part of the hex/ASCII dump
        parsing_command->data += phosg::parse_data_string(line.substr(expected_start.size(), 16 * 3 + 1));
        continue;
      } else {
        parsing_command = nullptr;
      }
    }

    if (line == "### use psov2 crypt") {
      this->use_psov2_rand_crypt = true;
    }
    if (line == "### use legacy item random behavior") {
      this->use_legacy_item_random_behavior = true;
    }
    if (line.starts_with("### cc ")) {
      // ### cc $<chat command>
      if (this->clients.size() != 1) {
        throw std::runtime_error(std::format(
            "(ev-line {}) Bare `cc` shortcut cannot be used with multiple clients connected; use `on C-X cc` instead",
            line_num));
      }
      try {
        auto c = this->clients.begin()->second;
        auto& event = this->create_event(Event::Type::SEND, c, line_num);
        event.data = encode_chat_message(c->version, line.substr(7));
      } catch (const std::exception& e) {
        throw std::runtime_error(std::format("(ev-line {}) Failed to generate chat message ({})", line_num, e.what()));
      }
      continue;

    } else if (line.starts_with("### on C-")) {
      // ### on C-{} cc <chat command>
      try {
        size_t end_offset;
        auto c = this->clients.at(stoull(line.substr(9), &end_offset, 16));
        if (line.compare(end_offset + 9, 4, " cc ") != 0) {
          throw std::runtime_error("malformed `on C-X cc $...` shortcut command");
        }
        auto& event = this->create_event(Event::Type::SEND, c, line_num);
        event.data = encode_chat_message(c->version, line.substr(end_offset + 13));
      } catch (const std::exception& e) {
        throw std::runtime_error(std::format("(ev-line {}) Failed to generate chat message ({})", line_num, e.what()));
      }
      continue;

    } else if (line.starts_with("I ")) {
      // I <pid/ts> - [GameServer] Client connected: C-1 via TG-9000-GC_V3-gc-jp10-game_server
      // I <pid/ts> - [GameServer] Client connected: C-3 via TSI-9000-GC_V3-game_server
      size_t offset = line.find(" - [GameServer] Client connected: C-");
      if (offset != std::string::npos) {
        auto tokens = phosg::split(line, ' ');

        if (!tokens[8].starts_with("C-")) {
          throw std::runtime_error(std::format("(ev-line {}) Client connection message missing client ID token", line_num));
        }
        uint64_t client_id = stoull(tokens[8].substr(2), nullptr, 16);

        auto listen_tokens = phosg::split(tokens[10], '-');
        if (listen_tokens.size() < 4) {
          throw std::runtime_error(std::format(
              "(ev-line {}) Client connection message listening socket token format is incorrect", line_num));
        }
        uint16_t port = stoul(listen_tokens[1], nullptr, 10);
        Version version = phosg::enum_for_name<Version>(listen_tokens[2]);

        auto c = std::make_shared<Client>(state->io_context, client_id, port, version);
        if (!this->clients.emplace(c->id, c).second) {
          throw std::runtime_error(std::format("(ev-line {}) Duplicate client ID in input log", line_num));
        }
        this->create_event(Event::Type::CONNECT, c, line_num);
        continue;
      }

      // I <pid/ts> - [GameServer] Running cleanup tasks for C-{}
      offset = line.find(" - [GameServer] Running cleanup tasks for C-");
      if (offset != std::string::npos) {
        auto tokens = phosg::split(line, ' ');
        if (tokens.size() < 11) {
          throw std::runtime_error(std::format("(ev-line {}) Client disconnection message has incorrect token count", line_num));
        }
        if (!tokens[10].starts_with("C-")) {
          throw std::runtime_error(std::format("(ev-line {}) Client disconnection message missing client ID token", line_num));
        }
        uint64_t client_id = stoul(tokens[10].substr(2), nullptr, 16);
        try {
          auto& c = this->clients.at(client_id);
          if (c->disconnect_event_number > 0) {
            throw std::runtime_error(std::format("(ev-line {}) Client has multiple disconnect events", line_num));
          }
          c->disconnect_event_number = this->create_event(Event::Type::DISCONNECT, c, line_num).event_number;
        } catch (const std::out_of_range&) {
          throw std::runtime_error(std::format("(ev-line {}) Unknown disconnecting client ID in input log", line_num));
        }
        continue;
      }

      // I <pid/ts> - [Commands] Sending to C-{:X} (...)
      // I <pid/ts> - [Commands] Received from C-{:X} (...)
      offset = line.find(" - [Commands] Sending to C-");
      if (offset == std::string::npos) {
        offset = line.find(" - [Commands] Received from C-");
      }
      if (offset != std::string::npos) {
        auto tokens = phosg::split(line, ' ');
        if (tokens.size() < 10) {
          throw std::runtime_error(std::format("(ev-line {}) Command header line too short", line_num));
        }
        bool from_client = (tokens[6] == "Received");
        uint64_t client_id = stoull(tokens[8].substr(2), nullptr, 16);
        try {
          parsing_command = &this->create_event(
              from_client ? Event::Type::SEND : Event::Type::RECEIVE, this->clients.at(client_id), line_num);
        } catch (const std::out_of_range&) {
          throw std::runtime_error(std::format("(ev-line {}) Input log contains command for missing client", line_num));
        }
        continue;
      }
    }
  }

  if (replay_log.should_log(phosg::LogLevel::L_DEBUG)) {
    replay_log.debug_f("{} clients in log", this->clients.size());
    for (const auto& it : this->clients) {
      replay_log.debug_f("  {} => {}", it.first, it.second->str());
    }
    replay_log.debug_f("{} events in replay log", this->events.size());
    for (const auto& [_, ev] : this->events) {
      replay_log.debug_f("  {}", ev.str());
    }
  }
}

asio::awaitable<void> ReplaySession::run() {
  bool prev_use_psov2_rand_crypt = this->state->use_psov2_rand_crypt;
  bool prev_use_legacy_item_random_behavior = this->state->use_legacy_item_random_behavior;
  this->state->use_psov2_rand_crypt = this->use_psov2_rand_crypt;
  this->state->use_legacy_item_random_behavior = this->use_legacy_item_random_behavior;

  auto event_it = this->events.begin();
  try {
    for (; event_it != this->events.end(); event_it++) {
      auto& ev = event_it->second;
      if (ev.complete) {
        continue;
      }

      if (replay_log.should_log(phosg::LogLevel::L_DEBUG)) {
        replay_log.debug_f("Event: {}", ev.str());
      }

      auto& c = this->clients.at(ev.client_id);
      switch (ev.type) {
        case Event::Type::CONNECT: {
          if (c->channel->connected()) {
            throw std::runtime_error(std::format(
                "(ev-line {}) Connect event on already-connected client", ev.line_num));
          }

          const DataIndex::PortConfiguration* port_config;
          try {
            port_config = &this->state->data->number_to_port_config.at(c->port);
          } catch (const std::out_of_range&) {
            throw std::runtime_error(std::format(
                "(ev-line {}) Client connected to port missing from configuration", ev.line_num));
          }

          auto server_channel = std::make_shared<PeerChannel>(this->state->io_context, port_config->version, c->channel->language, "", phosg::TerminalFormat::END, phosg::TerminalFormat::END, false, false);
          PeerChannel::link_peers(c->channel, server_channel);

          if (this->state->game_server.get()) {
            this->state->game_server->connect_channel(server_channel, c->port, port_config->behavior);
          } else {
            throw std::runtime_error(std::format("(ev-line {}) No server available for connection", ev.line_num));
          }
          break;
        }

        case Event::Type::DISCONNECT:
          c->channel->disconnect();
          break;

        case Event::Type::SEND:
          if (!c->channel->connected()) {
            throw std::runtime_error(std::format(
                "(ev-line {}) Send event attempted on unconnected client C-{}", ev.line_num, ev.client_id));
          }
          c->channel->send(ev.data);
          this->commands_sent++;
          this->bytes_sent += ev.data.size();
          break;

        case Event::Type::RECEIVE: {
          if (!c->channel->connected()) {
            throw std::runtime_error(std::format("(ev-line {}) Receive event on non-connected client", ev.line_num));
          }

          this->reschedule_idle_timeout();
          auto msg = co_await c->channel->recv();

          // TODO: Use the iovec form of phosg::print_data here instead of prepend_command_header (which copies data)
          std::string full_command = prepend_command_header(
              c->version, (c->channel->crypt_in.get() != nullptr), msg.command, msg.flag, msg.data);
          this->commands_received++;
          this->bytes_received += full_command.size();

          this->check_receive_event(c, ev, full_command);

          // If the command is an encryption init, set up encryption on the channel
          switch (c->version) {
            case Version::PC_PATCH:
            case Version::BB_PATCH:
              if (msg.command == 0x02) {
                auto& cmd = msg.check_size_t<S_ServerInit_Patch_02>();
                c->channel->crypt_in = std::make_shared<PSOV2Encryption>(cmd.server_key);
                c->channel->crypt_out = std::make_shared<PSOV2Encryption>(cmd.client_key);
              }
              break;
            case Version::DC_NTE:
            case Version::DC_11_2000:
            case Version::DC_V1:
            case Version::DC_V2:
            case Version::PC_NTE:
            case Version::PC_V2:
            case Version::GC_NTE:
            case Version::GC_V3:
            case Version::GC_EP3_NTE:
            case Version::GC_EP3:
            case Version::XB_V3:
              if (msg.command == 0x02 || msg.command == 0x17 || msg.command == 0x91 || msg.command == 0x9B) {
                auto& cmd = msg.check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(0xFFFF);
                if (is_v1_or_v2(c->version)) {
                  c->channel->crypt_in = std::make_shared<PSOV2Encryption>(cmd.server_key);
                  c->channel->crypt_out = std::make_shared<PSOV2Encryption>(cmd.client_key);
                } else { // V3
                  c->channel->crypt_in = std::make_shared<PSOV3Encryption>(cmd.server_key);
                  c->channel->crypt_out = std::make_shared<PSOV3Encryption>(cmd.client_key);
                }
              }
              break;
            case Version::BB_V4:
              if (msg.command == 0x03 || msg.command == 0x9B) {
                auto& cmd = msg.check_size_t<S_ServerInitDefault_BB_03_9B>(0xFFFF);
                // TODO: At some point it may matter which BB private key file we use. Don't just blindly use the
                // first one here.
                c->channel->crypt_in = std::make_shared<PSOBBEncryption>(
                    *this->state->data->bb_private_keys[0], cmd.server_key.data(), cmd.server_key.size());
                c->channel->crypt_out = std::make_shared<PSOBBEncryption>(
                    *this->state->data->bb_private_keys[0], cmd.client_key.data(), cmd.client_key.size());
              }
              break;
            default:
              throw std::logic_error("Unsupported encryption version");
          }
          break;
        }
        default:
          throw std::logic_error("Unhandled event type");
      }
      ev.complete = true;
    }

  } catch (const std::exception& e) {
    this->failure = std::format("Replay failed: {}", e.what());
    if (event_it != this->events.end()) {
      this->failure += std::format("\nNext pending event: {}", event_it->second.str());
    } else {
      this->failure += std::format("\nNo events are pending at failure time");
    }
  }

  for (auto& [_, c] : this->clients) {
    if (c->channel) {
      c->channel->disconnect();
    }
  }

  this->state->use_psov2_rand_crypt = prev_use_psov2_rand_crypt;
  this->state->use_legacy_item_random_behavior = prev_use_legacy_item_random_behavior;

  if (this->failure.empty()) {
    // Wait a bit longer to ensure that any command sent at the end of the replay session don't crash the server
    co_await async_sleep(std::chrono::seconds(2));
    replay_log.info_f("Replay complete: {} commands sent ({} bytes), {} commands received ({} bytes)",
        this->commands_sent, this->bytes_sent, this->commands_received, this->bytes_received);
  }

  this->idle_timeout_timer.cancel();
}

void ReplaySession::reschedule_idle_timeout() {
  this->idle_timeout_timer.expires_after(std::chrono::seconds(10));
  this->idle_timeout_timer.async_wait([this](std::error_code ec) {
    if (!ec) {
      replay_log.error_f("Server did not send expected event within the idle timeout");
      for (const auto& it : this->clients) {
        it.second->channel->disconnect();
      }
    }
  });
}
