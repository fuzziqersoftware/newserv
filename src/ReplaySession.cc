#include "ReplaySession.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "GameServer.hh"
#include "Loggers.hh"
#include "Server.hh"

using namespace std;

static string encode_chat_message(Version version, const string& message) {
  string encoded_message;
  encoded_message.resize(8, 0);
  encoded_message += uses_utf16(version) ? tt_utf8_to_utf16("\tE" + message) : tt_utf8_to_ascii("\tE" + message);
  encoded_message.resize((encoded_message.size() + 3) & (~3));
  return prepend_command_header(version, true, 0x06, 0x00, encoded_message);
}

ReplaySession::Event::Event(Type type, uint64_t client_id, size_t line_num)
    : type(type),
      client_id(client_id),
      allow_size_disparity(false),
      complete(false),
      line_num(line_num) {}

string ReplaySession::Event::str() const {
  string ret;
  if (this->type == Type::CONNECT) {
    ret = std::format("Event[{}, CONNECT", this->client_id);
  } else if (this->type == Type::DISCONNECT) {
    ret = std::format("Event[{}, DISCONNECT", this->client_id);
  } else if (this->type == Type::SEND) {
    ret = std::format("Event[{}, SEND {:04X}", this->client_id, this->data.size());
  } else if (this->type == Type::RECEIVE) {
    ret = std::format("Event[{}, RECEIVE {:04X}", this->client_id, this->data.size());
  }
  if (this->allow_size_disparity) {
    ret += ", size disparity allowed";
  }
  if (this->complete) {
    ret += ", done";
  }
  ret += std::format(", ev-line {}]", this->line_num);
  return ret;
}

ReplaySession::Client::Client(shared_ptr<asio::io_context> io_context, uint64_t id, uint16_t port, Version version)
    : id(id),
      port(port),
      version(version),
      channel(make_shared<PeerChannel>(io_context, this->version, 1, std::format("R-{:X}", this->id))) {}

string ReplaySession::Client::str() const {
  return std::format("Client[{}, T-{}, {}]", this->id, this->port, phosg::name_for_enum(this->version));
}

shared_ptr<ReplaySession::Event> ReplaySession::create_event(Event::Type type, shared_ptr<Client> c, size_t line_num) {
  auto event = make_shared<Event>(type, c->id, line_num);
  if (!this->last_event.get()) {
    this->first_event = event;
  } else {
    this->last_event->next_event = event;
  }
  this->last_event = event;
  if (type == Event::Type::RECEIVE) {
    c->receive_events.emplace_back(event);
  }
  return event;
}

void ReplaySession::apply_default_mask(shared_ptr<Event> ev) {
  auto version = this->clients.at(ev->client_id)->version;

  void* cmd_data = ev->data.data() + ((version == Version::BB_V4) ? 8 : 4);
  size_t cmd_size = ev->data.size() - ((version == Version::BB_V4) ? 8 : 4);
  void* mask_data = ev->mask.data() + ((version == Version::BB_V4) ? 8 : 4);
  size_t mask_size = ev->mask.size() - ((version == Version::BB_V4) ? 8 : 4);

  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH: {
      const auto& header = check_size_t<PSOCommandHeaderPC>(ev->data, 0xFFFF);
      if (header.command == 0x02) {
        auto& cmd_mask = check_size_t<S_ServerInit_Patch_02>(mask_data, mask_size);
        cmd_mask.server_key = 0;
        cmd_mask.client_key = 0;
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
      if ((version == Version::PC_NTE) || (version == Version::PC_V2)) {
        command = check_size_t<PSOCommandHeaderPC>(ev->data, 0xFFFF).command;
      } else { // V3
        command = check_size_t<PSOCommandHeaderDCV3>(ev->data, 0xFFFF).command;
      }
      switch (command) {
        case 0x02:
        case 0x17:
        case 0x91:
        case 0x9B: {
          auto& mask = check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(
              mask_data, mask_size, 0xFFFF);
          mask.server_key = 0;
          mask.client_key = 0;
          break;
        }
        case 0x19:
          if (mask_size == sizeof(S_ReconnectSplit_19)) {
            auto& mask = check_size_t<S_ReconnectSplit_19>(mask_data, mask_size);
            mask.pc_address = 0;
            mask.gc_address = 0;
          } else {
            auto& mask = check_size_t<S_Reconnect_19>(mask_data, mask_size);
            mask.address = 0;
          }
          break;
        case 0x41:
          if ((version == Version::PC_NTE) || (version == Version::PC_V2)) {
            auto& mask = check_size_t<S_GuildCardSearchResult_PC_41>(mask_data, mask_size);
            mask.reconnect_command.address = 0;
          } else { // V3
            auto& mask = check_size_t<S_GuildCardSearchResult_DC_V3_41>(mask_data, mask_size);
            mask.reconnect_command.address = 0;
          }
          break;
        case 0x64:
          if ((version == Version::PC_NTE) || (version == Version::PC_V2)) {
            auto& mask = check_size_t<S_JoinGame_PC_64>(mask_data, mask_size);
            mask.variations = Variations();
            mask.random_seed = 0;
          } else if (version == Version::XB_V3) {
            auto& mask = check_size_t<S_JoinGame_XB_64>(mask_data, mask_size);
            mask.variations = Variations();
            mask.random_seed = 0;
          } else if (version == Version::DC_NTE || version == Version::DC_11_2000) {
            auto& mask = check_size_t<S_JoinGame_DCNTE_64>(mask_data, mask_size);
            mask.variations = Variations();
          } else {
            auto& mask = check_size_t<S_JoinGame_DC_64>(mask_data, mask_size, sizeof(S_JoinGame_Ep3_64));
            mask.variations = Variations();
            mask.random_seed = 0;
            for (size_t offset = sizeof(S_JoinGame_GC_64) +
                    offsetof(S_JoinGame_Ep3_64::Ep3PlayerEntry, disp.visual.name_color_checksum);
                offset + 4 <= mask_size;
                offset += sizeof(S_JoinGame_Ep3_64::Ep3PlayerEntry)) {
              *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(mask_data) + offset) = 0;
            }
          }
          break;
        case 0xEB:
          if (!is_gc(version)) {
            break;
          }
          [[fallthrough]];
        case 0x65:
        case 0x67:
        case 0x68: {
          auto update_mask = [&]<typename CmdT>() -> void {
            for (size_t offset = offsetof(CmdT, entries) + offsetof(typename CmdT::Entry, disp.visual.name_color_checksum);
                offset + 4 <= mask_size;
                offset += sizeof(typename CmdT::Entry)) {
              *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(mask_data) + offset) = 0;
            }
          };
          if ((version == Version::PC_NTE) || (version == Version::PC_V2)) {
            update_mask.template operator()<S_JoinLobby_PC_65_67_68>();
          } else if (version == Version::XB_V3) {
            update_mask.template operator()<S_JoinLobby_XB_65_67_68>();
          } else if (version == Version::DC_NTE) {
            update_mask.template operator()<S_JoinLobby_DCNTE_65_67_68>();
          } else {
            update_mask.template operator()<S_JoinLobby_DC_GC_65_67_68_Ep3_EB>();
          }
          break;
        }
        case 0xE8:
          if (is_gc(version)) {
            auto& mask = check_size_t<S_JoinSpectatorTeam_Ep3_E8>(mask_data, mask_size);
            mask.random_seed = 0;
            for (size_t z = 0; z < 4; z++) {
              mask.players[z].disp.visual.name_color_checksum = 0;
            }
            for (size_t z = 0; z < 8; z++) {
              mask.spectator_players[z].disp.visual.name_color_checksum = 0;
            }
          }
          break;
        case 0xC5: {
          auto update_mask = [&]<typename CmdT, bool HasEp2>() -> void {
            for (size_t offset = 0; offset + sizeof(CmdT) <= mask_size; offset += sizeof(CmdT)) {
              auto* entry = reinterpret_cast<CmdT*>(reinterpret_cast<uint8_t*>(mask_data) + offset);
              for (size_t z = 0; z < entry->challenge.times_ep1_online.size(); z++) {
                entry->challenge.times_ep1_online[z].store_raw(0);
              }
              for (size_t z = 0; z < entry->challenge.times_ep1_offline.size(); z++) {
                entry->challenge.times_ep1_online[z].store_raw(0);
              }
              if constexpr (HasEp2) {
                for (size_t z = 0; z < entry->challenge.times_ep2_online.size(); z++) {
                  entry->challenge.times_ep2_online[z].store_raw(0);
                }
              }
            }
          };
          if (version == Version::DC_V2) {
            update_mask.template operator()<PlayerRecordsEntry_DC, false>();
          } else if (is_v2(version)) {
            update_mask.template operator()<PlayerRecordsEntry_PC, false>();
          } else if (is_v3(version)) {
            update_mask.template operator()<PlayerRecordsEntry_V3, true>();
          } else if (is_v4(version)) {
            update_mask.template operator()<PlayerRecordsEntry_BB, true>();
          }
          break;
        }
        case 0xB1:
          for (size_t x = 4; x < ev->mask.size(); x++) {
            ev->mask[x] = 0;
          }
          break;
        case 0xC9:
          if (mask_size == sizeof(G_ServerVersionStrings_Ep3NTE_6xB4x46)) {
            auto& mask = check_size_t<G_ServerVersionStrings_Ep3NTE_6xB4x46>(mask_data, mask_size);
            mask.version_signature.clear(0);
            mask.date_str1.clear(0);
          } else if (mask_size == sizeof(G_ServerVersionStrings_Ep3_6xB4x46)) {
            auto& mask = check_size_t<G_ServerVersionStrings_Ep3_6xB4x46>(mask_data, mask_size);
            mask.version_signature.clear(0);
            mask.date_str1.clear(0);
            mask.date_str2.clear(0);
          }
          break;
        case 0x6C:
          if (is_gc(version) && mask_size >= 0x14) {
            const auto& cmd = check_size_t<G_MapList_Ep3_6xB6x40>(cmd_data, cmd_size, 0xFFFF);
            if ((cmd.header.header.basic_header.subcommand == 0xB6) &&
                (cmd.header.subsubcommand == 0x40)) {
              check_size_t<PSOCommandHeaderDCV3>(ev->mask, 0xFFFF).size = 0;
              auto& mask = check_size_t<G_MapList_Ep3_6xB6x40>(mask_data, mask_size, 0xFFFF);
              mask.header.header.size = 0;
              mask.compressed_data_size = 0;
              ev->allow_size_disparity = true;
              for (size_t z = sizeof(PSOCommandHeaderDCV3) + sizeof(G_MapList_Ep3_6xB6x40); z < ev->mask.size(); z++) {
                ev->mask[z] = 0;
              }
            }
          }
          break;
        case 0x6D:
          if (version == Version::DC_NTE) {
            const auto& header = check_size_t<G_UnusedHeader>(cmd_data, cmd_size, 0xFFFF);
            if (header.subcommand == 0x60) {
              auto& mask = check_size_t<G_SyncPlayerDispAndInventory_DCNTE_6x70>(mask_data, mask_size, 0xFFFF);
              mask.visual.name_color_checksum = 0;
            }
          } else if (version == Version::DC_11_2000) {
            const auto& header = check_size_t<G_UnusedHeader>(cmd_data, cmd_size, 0xFFFF);
            if (header.subcommand == 0x67) {
              auto& mask = check_size_t<G_SyncPlayerDispAndInventory_DC112000_6x70>(mask_data, mask_size, 0xFFFF);
              mask.visual.name_color_checksum = 0;
            }
          } else if (!is_pre_v1(version)) {
            const auto& header = check_size_t<G_UnusedHeader>(cmd_data, cmd_size, 0xFFFF);
            if (header.subcommand == 0x70) {
              auto& mask = check_size_t<G_SyncPlayerDispAndInventory_DC_PC_6x70>(mask_data, mask_size, 0xFFFF);
              mask.base.visual.name_color_checksum = 0;
            }
          }
          break;
      }
      break;
    }
    case Version::BB_V4: {
      uint16_t command = check_size_t<PSOCommandHeaderBB>(ev->data, 0xFFFF).command;
      switch (command) {
        case 0x0003: {
          auto& mask = check_size_t<S_ServerInitDefault_BB_03_9B>(mask_data, mask_size, 0xFFFF);
          mask.server_key.clear(0);
          mask.client_key.clear(0);
          break;
        }
        case 0x0019: {
          auto& mask = check_size_t<S_Reconnect_19>(mask_data, mask_size);
          mask.address = 0;
          break;
        }
        case 0x0064: {
          auto& mask = check_size_t<S_JoinGame_BB_64>(mask_data, mask_size);
          mask.variations = Variations();
          mask.random_seed = 0;
          break;
        }
        case 0x00B1: {
          for (size_t x = 8; x < ev->mask.size(); x++) {
            ev->mask[x] = 0;
          }
          break;
        }
        case 0x00E6: {
          auto& mask = check_size_t<S_ClientInit_BB_00E6>(mask_data, mask_size);
          mask.security_token = 0;
          break;
        }
      }
      break;
    }
    default:
      throw logic_error("invalid game version");
  }
}

ReplaySession::ReplaySession(shared_ptr<ServerState> state, FILE* input_log, bool is_interactive)
    : state(state),
      is_interactive(is_interactive),
      prev_psov2_crypt_enabled(this->state->use_psov2_rand_crypt),
      commands_sent(0),
      bytes_sent(0),
      commands_received(0),
      bytes_received(0),
      idle_timeout_timer(*this->state->io_context),
      run_failed(false) {
  shared_ptr<Event> parsing_command = nullptr;

  size_t line_num = 0;
  size_t num_events = 0;
  while (!feof(input_log)) {
    line_num++;
    string line = phosg::fgets(input_log);
    if (line.ends_with("\n")) {
      line.resize(line.size() - 1);
    }
    if (line.empty()) {
      continue;
    }

    if (parsing_command.get()) {
      string expected_start = std::format("{:04X} |", parsing_command->data.size());
      if (line.starts_with(expected_start)) {
        // Parse out the hex part of the hex/ASCII dump
        string mask_bytes;
        string data_bytes = phosg::parse_data_string(line.substr(expected_start.size(), 16 * 3 + 1), &mask_bytes);
        parsing_command->data += data_bytes;
        parsing_command->mask += mask_bytes;
        continue;
      } else {
        if (parsing_command->type == Event::Type::RECEIVE) {
          this->apply_default_mask(parsing_command);
        }
        parsing_command = nullptr;
      }
    }

    if (line == "### use psov2 crypt") {
      this->state->use_psov2_rand_crypt = true;
    }
    if (line.starts_with("### cc ")) {
      // ### cc $<chat command>
      if (this->clients.size() != 1) {
        throw runtime_error(std::format(
            "(ev-line {}) cc shortcut cannot be used with multiple clients connected; use on C-X cc instead", line_num));
      }
      shared_ptr<Event> event;
      try {
        auto c = this->clients.begin()->second;
        event = this->create_event(Event::Type::SEND, c, line_num);
        event->data = encode_chat_message(c->version, line.substr(7));
        num_events++;
      } catch (const exception& e) {
        throw runtime_error(std::format("(ev-line {}) failed to generate chat message ({})", line_num, e.what()));
      }
      continue;

    } else if (line.starts_with("### on C-")) {
      // ### on C-{} cc <chat command>
      shared_ptr<Event> event;
      try {
        size_t end_offset;
        auto c = this->clients.at(stoull(line.substr(9), &end_offset, 16));
        if (line.compare(end_offset + 9, 4, " cc ") != 0) {
          throw runtime_error("malformed `on C-X cc $...` shortcut command");
        }
        event = this->create_event(Event::Type::SEND, c, line_num);
        event->data = encode_chat_message(c->version, line.substr(end_offset + 13));
        num_events++;
      } catch (const exception& e) {
        throw runtime_error(std::format("(ev-line {}) failed to generate chat message ({})", line_num, e.what()));
      }
      continue;

    } else if (line.starts_with("I ")) {
      // I <pid/ts> - [GameServer] Client connected: C-1 via TG-9000-GC_V3-gc-jp10-game_server
      // I <pid/ts> - [GameServer] Client connected: C-3 via TSI-9000-GC_V3-game_server
      size_t offset = line.find(" - [GameServer] Client connected: C-");
      if (offset != string::npos) {
        auto tokens = phosg::split(line, ' ');

        if (!tokens[8].starts_with("C-")) {
          throw runtime_error(std::format("(ev-line {}) client connection message missing client ID token", line_num));
        }
        uint64_t client_id = stoull(tokens[8].substr(2), nullptr, 16);

        auto listen_tokens = phosg::split(tokens[10], '-');
        if (listen_tokens.size() < 4) {
          throw runtime_error(std::format(
              "(ev-line {}) client connection message listening socket token format is incorrect", line_num));
        }
        uint16_t port = stoul(listen_tokens[1], nullptr, 10);
        Version version = phosg::enum_for_name<Version>(listen_tokens[2]);

        auto c = make_shared<Client>(state->io_context, client_id, port, version);
        if (!this->clients.emplace(c->id, c).second) {
          throw runtime_error(std::format("(ev-line {}) duplicate client ID in input log", line_num));
        }
        this->create_event(Event::Type::CONNECT, c, line_num);
        num_events++;
        continue;
      }

      // I <pid/ts> - [GameServer] Running cleanup tasks for C-{}
      offset = line.find(" - [GameServer] Running cleanup tasks for C-");
      if (offset != string::npos) {
        auto tokens = phosg::split(line, ' ');
        if (tokens.size() < 11) {
          throw runtime_error(std::format("(ev-line {}) client disconnection message has incorrect token count", line_num));
        }
        if (!tokens[10].starts_with("C-")) {
          throw runtime_error(std::format("(ev-line {}) client disconnection message missing client ID token", line_num));
        }
        uint64_t client_id = stoul(tokens[10].substr(2), nullptr, 16);
        try {
          auto& c = this->clients.at(client_id);
          if (c->disconnect_event.get()) {
            throw runtime_error(std::format("(ev-line {}) client has multiple disconnect events", line_num));
          }
          c->disconnect_event = this->create_event(Event::Type::DISCONNECT, c, line_num);
          num_events++;
        } catch (const out_of_range&) {
          throw runtime_error(std::format("(ev-line {}) unknown disconnecting client ID in input log", line_num));
        }
        continue;
      }

      // I <pid/ts> - [Commands] Sending to C-{:X} (...)
      // I <pid/ts> - [Commands] Received from C-{:X} (...)
      offset = line.find(" - [Commands] Sending to C-");
      if (offset == string::npos) {
        offset = line.find(" - [Commands] Received from C-");
      }
      if (offset != string::npos) {
        auto tokens = phosg::split(line, ' ');
        if (tokens.size() < 10) {
          throw runtime_error(std::format("(ev-line {}) command header line too short", line_num));
        }
        bool from_client = (tokens[6] == "Received");
        uint64_t client_id = stoull(tokens[8].substr(2), nullptr, 16);
        try {
          parsing_command = this->create_event(
              from_client ? Event::Type::SEND : Event::Type::RECEIVE,
              this->clients.at(client_id),
              line_num);
          num_events++;
        } catch (const out_of_range&) {
          throw runtime_error(std::format("(ev-line {}) input log contains command for missing client", line_num));
        }
        continue;
      }
    }
  }

  replay_log.debug_f("{} clients in log", this->clients.size());
  for (const auto& it : this->clients) {
    string client_str = it.second->str();
    replay_log.debug_f("  {} => {}", it.first, client_str);
  }

  replay_log.debug_f("{} events in replay log", num_events);
  for (auto ev = this->first_event; ev != nullptr; ev = ev->next_event) {
    string ev_str = ev->str();
    replay_log.debug_f("  {}", ev_str);
  }
}

asio::awaitable<void> ReplaySession::run() {
  try {
    replay_log.info_f("Starting replay");
    while (this->first_event) {
      if (!this->first_event->complete) {
        auto& c = this->clients.at(this->first_event->client_id);

        replay_log.debug_f("Event: {}", this->first_event->str());
        switch (this->first_event->type) {
          case Event::Type::CONNECT: {
            if (c->channel->connected()) {
              throw runtime_error(std::format(
                  "(ev-line {}) connect event on already-connected client", this->first_event->line_num));
            }

            shared_ptr<const PortConfiguration> port_config;
            try {
              port_config = this->state->number_to_port_config.at(c->port);
            } catch (const out_of_range&) {
              throw runtime_error(std::format(
                  "(ev-line {}) client connected to port missing from configuration", this->first_event->line_num));
            }

            auto server_channel = make_shared<PeerChannel>(this->state->io_context, port_config->version, c->channel->language);
            PeerChannel::link_peers(c->channel, server_channel);

            if (this->state->game_server.get()) {
              this->state->game_server->connect_channel(server_channel, c->port, port_config->behavior);
            } else {
              throw runtime_error(std::format(
                  "(ev-line {}) no server available for connection", this->first_event->line_num));
            }
            break;
          }

          case Event::Type::DISCONNECT:
            c->channel->disconnect();
            break;

          case Event::Type::SEND:
            if (!c->channel->connected()) {
              throw runtime_error(std::format(
                  "(ev-line {}) send event attempted on unconnected client", this->first_event->line_num));
            }
            c->channel->send(this->first_event->data);
            this->commands_sent++;
            this->bytes_sent += this->first_event->data.size();
            break;

          case Event::Type::RECEIVE: {
            if (!c->channel->connected()) {
              throw runtime_error(std::format("(ev-line {}) receive event on non-connected client",
                  this->first_event->line_num));
            }
            if (c->receive_events.front() != this->first_event) {
              throw logic_error("Client receive events are out of order");
            }

            this->reschedule_idle_timeout();
            auto msg = co_await c->channel->recv();

            // TODO: Use the iovec form of phosg::print_data here instead of
            // prepend_command_header (which copies the string)
            string full_command = prepend_command_header(
                c->version, (c->channel->crypt_in.get() != nullptr), msg.command, msg.flag, msg.data);
            this->commands_received++;
            this->bytes_received += full_command.size();

            if (c->receive_events.empty()) {
              phosg::print_data(stderr, full_command, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::OFFSET_16_BITS);
              throw runtime_error("received unexpected command for client");
            }

            auto& ev = c->receive_events.front();
            if ((full_command.size() != ev->data.size()) && !ev->allow_size_disparity) {
              replay_log.error_f("Expected command:");
              phosg::print_data(stderr, ev->data, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::OFFSET_16_BITS);
              replay_log.error_f("Received command:");
              phosg::print_data(stderr, full_command, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::OFFSET_16_BITS);
              throw runtime_error(std::format("(ev-line {}) received command sizes do not match", ev->line_num));
            }
            for (size_t x = 0; x < min<size_t>(full_command.size(), ev->data.size()); x++) {
              if ((full_command[x] & ev->mask[x]) != (ev->data[x] & ev->mask[x])) {
                replay_log.error_f("Expected command:");
                phosg::print_data(stderr, ev->data, 0, nullptr, phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::OFFSET_16_BITS);
                replay_log.error_f("Received command:");
                phosg::print_data(stderr, full_command, 0, ev->data.data(), phosg::PrintDataFlags::PRINT_ASCII | phosg::PrintDataFlags::OFFSET_16_BITS);
                throw runtime_error(std::format("(ev-line {}) received command data does not match expected data", ev->line_num));
              }
            }

            ev->complete = true;
            c->receive_events.pop_front();

            // If the command is an encryption init, set up encryption on the channel
            switch (c->version) {
              case Version::PC_PATCH:
              case Version::BB_PATCH:
                if (msg.command == 0x02) {
                  auto& cmd = msg.check_size_t<S_ServerInit_Patch_02>();
                  c->channel->crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
                  c->channel->crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
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
                    c->channel->crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
                    c->channel->crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
                  } else { // V3
                    c->channel->crypt_in = make_shared<PSOV3Encryption>(cmd.server_key);
                    c->channel->crypt_out = make_shared<PSOV3Encryption>(cmd.client_key);
                  }
                }
                break;
              case Version::BB_V4:
                if (msg.command == 0x03 || msg.command == 0x9B) {
                  auto& cmd = msg.check_size_t<S_ServerInitDefault_BB_03_9B>(0xFFFF);
                  // TODO: At some point it may matter which BB private key file we use.
                  // Don't just blindly use the first one here.
                  c->channel->crypt_in = make_shared<PSOBBEncryption>(
                      *this->state->bb_private_keys[0], cmd.server_key.data(), cmd.server_key.size());
                  c->channel->crypt_out = make_shared<PSOBBEncryption>(
                      *this->state->bb_private_keys[0], cmd.client_key.data(), cmd.client_key.size());
                }
                break;
              default:
                throw logic_error("unsupported encryption version");
            }
            break;
          }
          default:
            throw logic_error("unhandled event type");
        }
        this->first_event->complete = true;
      }

      this->first_event = this->first_event->next_event;
      if (!this->first_event.get()) {
        this->last_event = nullptr;
      }
    }
  } catch (const exception& e) {
    replay_log.error_f("Replay failed: {}", e.what());
    if (this->first_event) {
      replay_log.error_f("Next pending event: {}", this->first_event->str());
    } else {
      replay_log.error_f("No events are pending at failure time");
    }
    this->run_failed = true;
  }

  for (auto& [_, c] : this->clients) {
    if (c->channel) {
      c->channel->disconnect();
    }
  }
  this->state->use_psov2_rand_crypt = this->prev_psov2_crypt_enabled;

  if (!this->run_failed) {
    // Wait a bit longer to ensure that any command sent at the end of the replay
    // session don't crash the server
    co_await async_sleep(std::chrono::seconds(2));
    replay_log.info_f("Replay complete: {} commands sent ({} bytes), {} commands received ({} bytes)",
        this->commands_sent, this->bytes_sent, this->commands_received, this->bytes_received);
  }
  if (!this->is_interactive) {
    this->state->io_context->stop();
  }
}

void ReplaySession::reschedule_idle_timeout() {
  this->idle_timeout_timer.expires_after(std::chrono::seconds(3));
  this->idle_timeout_timer.async_wait([this](std::error_code ec) {
    if (!ec) {
      replay_log.error_f("Server did not send expected event within the idle timeout");
      for (const auto& it : this->clients) {
        it.second->channel->disconnect();
      }
    }
  });
}
