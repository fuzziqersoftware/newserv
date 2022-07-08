#include "ReplaySession.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "Shell.hh"
#include "Server.hh"

using namespace std;



ReplaySession::Event::Event(Type type, uint64_t client_id)
  : type(type), client_id(client_id), complete(false) { }

ReplaySession::Client::Client(
    ReplaySession* session, uint64_t id, uint16_t port, GameVersion version)
  : id(id),
    port(port),
    version(version),
    channel(
      this->version,
      &ReplaySession::dispatch_on_command_received,
      &ReplaySession::dispatch_on_error,
      session,
      string_printf("R-%" PRIX64, this->id)) { }



shared_ptr<ReplaySession::Event> ReplaySession::create_event(
    Event::Type type, shared_ptr<Client> c) {
  shared_ptr<Event> event(new Event(type, c->id));
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

  void* cmd_data = ev->mask.data() + ((version == GameVersion::BB) ? 8 : 4);
  size_t cmd_size = ev->mask.size() - ((version == GameVersion::BB) ? 8 : 4);

  switch (version) {
    case GameVersion::PATCH: {
      const auto& header = check_size_t<PSOCommandHeaderPC>(
          ev->data, sizeof(PSOCommandHeaderPC), 0xFFFF);
      if (header.command == 0x02) {
        auto& cmd_mask = check_size_t<S_ServerInit_Patch_02>(cmd_data, cmd_size);
        cmd_mask.server_key = 0;
        cmd_mask.client_key = 0;
      }
      break;
    }
    case GameVersion::PC:
    case GameVersion::GC: {
      uint8_t command;
      if (version == GameVersion::PC) {
        command = check_size_t<PSOCommandHeaderPC>(
            ev->data, sizeof(PSOCommandHeaderPC), 0xFFFF).command;
      } else {
        command = check_size_t<PSOCommandHeaderDCGC>(
            ev->data, sizeof(PSOCommandHeaderDCGC), 0xFFFF).command;
      }
      switch (command) {
        case 0x02:
        case 0x17:
        case 0x91:
        case 0x9B: {
          auto& cmd_mask = check_size_t<S_ServerInit_DC_PC_GC_02_17_91_9B>(
              cmd_data, cmd_size, sizeof(S_ServerInit_DC_PC_GC_02_17_91_9B), 0xFFFF);
          cmd_mask.server_key = 0;
          cmd_mask.client_key = 0;
          break;
        }
        case 0x0019: {
          auto& cmd_mask = check_size_t<S_Reconnect_19>(cmd_data, cmd_size);
          cmd_mask.address = 0;
          break;
        }
        case 0x64: {
          if (version == GameVersion::PC) {
            auto& cmd_mask = check_size_t<S_JoinGame_PC_64>(cmd_data, cmd_size,
                offsetof(S_JoinGame_GC_64, players_ep3));
            cmd_mask.variations.clear(0);
            cmd_mask.rare_seed = 0;
          } else { // GC
            auto& cmd_mask = check_size_t<S_JoinGame_GC_64>(cmd_data, cmd_size,
                offsetof(S_JoinGame_GC_64, players_ep3));
            cmd_mask.variations.clear(0);
            cmd_mask.rare_seed = 0;
          }
          break;
        }
        case 0xB1: {
          for (size_t x = 8; x < ev->mask.size(); x++) {
            ev->mask[x] = 0;
          }
          break;
        }
      }
      break;
    }
    case GameVersion::BB: {
      uint16_t command = check_size_t<PSOCommandHeaderBB>(
            ev->data, sizeof(PSOCommandHeaderBB), 0xFFFF).command;
      switch (command) {
        case 0x0003: {
          auto& cmd_mask = check_size_t<S_ServerInit_BB_03_9B>(
              cmd_data, cmd_size, sizeof(S_ServerInit_BB_03_9B), 0xFFFF);
          cmd_mask.server_key.clear(0);
          cmd_mask.client_key.clear(0);
          break;
        }
        case 0x0019: {
          auto& cmd_mask = check_size_t<S_Reconnect_19>(cmd_data, cmd_size);
          cmd_mask.address = 0;
          break;
        }
        case 0x0064: {
          auto& cmd_mask = check_size_t<S_JoinGame_BB_64>(cmd_data, cmd_size,
              offsetof(S_JoinGame_BB_64, players_ep3),
              offsetof(S_JoinGame_BB_64, players_ep3));
          cmd_mask.variations.clear(0);
          cmd_mask.rare_seed = 0;
          break;
        }
        case 0x00B1: {
          for (size_t x = 8; x < ev->mask.size(); x++) {
            ev->mask[x] = 0;
          }
          break;
        }
        case 0x00E6: {
          auto& cmd_mask = check_size_t<S_ClientInit_BB_00E6>(cmd_data, cmd_size);
          cmd_mask.team_id = 0;
          break;
        }
      }
      break;
    }
    case GameVersion::DC:
      throw logic_error("DC auto-masking is not implemented");
    default:
      throw logic_error("invalid game version");
  }
}

ReplaySession::ReplaySession(
    shared_ptr<struct event_base> base,
    FILE* input_log,
    shared_ptr<ServerState> state)
  : state(state),
    base(base),
    commands_sent(0),
    bytes_sent(0),
    commands_received(0),
    bytes_received(0) {
  shared_ptr<Event> parsing_command = nullptr;

  while (!feof(input_log)) {
    string line = fgets(input_log);
    if (starts_with(line, Shell::PROMPT)) {
      line = line.substr(Shell::PROMPT.size());
    }
    if (ends_with(line, "\n")) {
      line.resize(line.size() - 1);
    }
    if (line.empty()) {
      continue;
    }

    if (parsing_command.get()) {
      string expected_start = string_printf("%016zX |", parsing_command->data.size());
      if (starts_with(line, expected_start)) {
        // Parse out the hex part of the hex/ASCII dump
        string mask_bytes;
        string data_bytes = parse_data_string(
            line.substr(expected_start.size(), 16 * 3 + 1), &mask_bytes);
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

    if (starts_with(line, "I ")) {
      // I <pid/ts> - [Server] Client connected: C-%X on fd %d via %d (T-%hu-%s-%s-%s)
      // I <pid/ts> - [Server] Client connected: C-%X on virtual connection %p via T-%hu-VI
      size_t offset = line.find(" - [Server] Client connected: C-");
      if (offset != string::npos) {
        auto tokens = split(line, ' ');
        if (tokens.size() != 15) {
          throw runtime_error("client connection message has incorrect token count");
        }
        if (!starts_with(tokens[8], "C-")) {
          throw runtime_error("client connection message missing client ID token");
        }
        auto listen_tokens = split(tokens[14], '-');
        if (listen_tokens.size() < 4) {
          throw runtime_error("client connection message listening socket token format is incorrect");
        }

        shared_ptr<Client> c(new Client(
            this,
            stoull(tokens[8].substr(2), nullptr, 16),
            stoul(listen_tokens[1], nullptr, 10),
            version_for_name(listen_tokens[2].c_str())));
        if (!this->clients.emplace(c->id, c).second) {
          throw runtime_error("duplicate client ID in input log");
        }
        this->create_event(Event::Type::CONNECT, c);
        continue;
      }

      // I <pid/ts> - [Server] Disconnecting C-%X on fd %d
      offset = line.find(" - [Server] Client disconnected: C-");
      if (offset != string::npos) {
        auto tokens = split(line, ' ');
        if (tokens.size() < 9) {
          throw runtime_error("client disconnection message has incorrect token count");
        }
        if (!starts_with(tokens[8], "C-")) {
          throw runtime_error("client disconnection message missing client ID token");
        }
        uint64_t client_id = stoul(tokens[8].substr(2), nullptr, 16);
        try {
          auto& c = this->clients.at(client_id);
          if (c->disconnect_event.get()) {
            throw runtime_error("client has multiple disconnect events");
          }
          c->disconnect_event = this->create_event(Event::Type::DISCONNECT, c);
        } catch (const out_of_range&) {
          throw runtime_error("unknown disconnecting client ID in input log");
        }
        continue;
      }

      // I <pid/ts> - [Commands] Sending to C-%X (...)
      // I <pid/ts> - [Commands] Received from C-%X (...)
      offset = line.find(" - [Commands] Sending to C-");
      if (offset == string::npos) {
        offset = line.find(" - [Commands] Received from C-");
      }
      if (offset != string::npos) {
        auto tokens = split(line, ' ');
        if (tokens.size() < 10) {
          throw runtime_error("command header line too short");
        }
        bool from_client = (tokens[6] == "Received");
        uint64_t client_id = stoull(tokens[8].substr(2), nullptr, 16);
        try {
          parsing_command = this->create_event(
              from_client ? Event::Type::SEND : Event::Type::RECEIVE,
              this->clients.at(client_id));
        } catch (const out_of_range&) {
          throw runtime_error("input log contains command for missing client");
        }
        continue;
      }
    }
  }
}

void ReplaySession::start() {
  this->update_timeout_event();
  this->execute_pending_events();
}

void ReplaySession::update_timeout_event() {
  if (!this->timeout_ev.get()) {
    this->timeout_ev.reset(
        event_new(this->base.get(), -1, EV_TIMEOUT, this->dispatch_on_timeout, this),
        event_free);
  }
  struct timeval tv = usecs_to_timeval(3000000);
  event_add(this->timeout_ev.get(), &tv);
}

void ReplaySession::dispatch_on_timeout(evutil_socket_t, short, void*) {
  throw runtime_error("timeout waiting for next event");
}

void ReplaySession::execute_pending_events() {
  while (this->first_event) {
    if (!this->first_event->complete) {
      auto& c = this->clients.at(this->first_event->client_id);
      switch (this->first_event->type) {
        case Event::Type::CONNECT: {
          if (c->channel.connected()) {
            throw runtime_error("connect event on already-connected client");
          }

          struct bufferevent* bevs[2];
          bufferevent_pair_new(this->base.get(), 0, bevs);

          c->channel.set_bufferevent(bevs[0]);
          this->channel_to_client.emplace(&c->channel, c);

          shared_ptr<const PortConfiguration> port_config;
          try {
            port_config = this->state->number_to_port_config.at(c->port);
          } catch (const out_of_range&) {
            bufferevent_free(bevs[1]);
            throw runtime_error("client connected to port missing from configuration");
          }

          if (port_config->behavior == ServerBehavior::PROXY_SERVER) {
            // TODO: We should support this at some point in the future
            throw runtime_error("client connected to proxy server");
          } else if (this->state->game_server.get()) {
            this->state->game_server->connect_client(bevs[1], 0x20202020,
                1025, c->port, port_config->version, port_config->behavior);
          } else {
            throw runtime_error("no server available for connection");
            bufferevent_free(bevs[1]);
          }
          break;
        }
        case Event::Type::DISCONNECT:
          this->channel_to_client.erase(&c->channel);
          c->channel.disconnect();
          break;
        case Event::Type::SEND:
          if (!c->channel.connected()) {
            throw runtime_error("send event attempted on unconnected client");
          }
          c->channel.send(this->first_event->data);
          this->commands_sent++;
          this->bytes_sent += this->first_event->data.size();
          break;
        case Event::Type::RECEIVE:
          // Receive events cannot be executed here, since we have to wait for
          // an incoming command. The existing handlers will take care of it:
          // on_command_received will be called sometime (hopefully) soon.
          return;
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

  // If we get here, then there are no more events to run: we're done.
  // TODO: We should flush any pending sends on the remaining client here, even
  // though there are no pending receives (to make sure the last sent commands
  // don't crash newserv)
  replay_log.info("Replay complete: %zu commands sent (%zu bytes), %zu commands received (%zu bytes)",
      this->commands_sent, this->bytes_sent, this->commands_received, this->bytes_received);
  event_base_loopexit(this->base.get(), nullptr);
}

void ReplaySession::dispatch_on_command_received(
    Channel& ch, uint16_t command, uint32_t flag, string& data) {
  ReplaySession* session = reinterpret_cast<ReplaySession*>(ch.context_obj);
  session->on_command_received(
      session->channel_to_client.at(&ch), command, flag, data);
}

void ReplaySession::dispatch_on_error(Channel& ch, short events) {
  ReplaySession* session = reinterpret_cast<ReplaySession*>(ch.context_obj);
  session->on_error(session->channel_to_client.at(&ch), events);
}

void ReplaySession::on_command_received(
    shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {

  string full_command = prepend_command_header(
      c->version, c->channel.crypt_in.get(), command, flag, data);
  this->commands_received++;
  this->bytes_received += full_command.size();

  if (c->receive_events.empty()) {
    print_data(stderr, full_command);
    throw runtime_error("received unexpected command for client");
  }

  auto& ev = c->receive_events.front();
  if (full_command.size() != ev->data.size()) {
    replay_log.error("Expected command:");
    print_data(stderr, ev->data);
    replay_log.error("Received command:");
    print_data(stderr, full_command);
    throw runtime_error("received command sizes do not match");
  }
  for (size_t x = 0; x < full_command.size(); x++) {
    if ((full_command[x] & ev->mask[x]) != (ev->data[x] & ev->mask[x])) {
      replay_log.error("Expected command:");
      print_data(stderr, ev->data);
      replay_log.error("Received command:");
      print_data(stderr, full_command, 0, ev->data.data());
      throw runtime_error("received command data does not match expected data");
    }
  }

  ev->complete = true;
  c->receive_events.pop_front();

  // If the command is an encryption init, set up encryption on the channel
  switch (c->version) {
    case GameVersion::DC:
      throw runtime_error("DC encryption is not supported during replays");
    case GameVersion::PATCH:
      if (command == 0x02) {
        auto& cmd = check_size_t<S_ServerInit_Patch_02>(data);
        c->channel.crypt_in.reset(new PSOPCEncryption(cmd.server_key));
        c->channel.crypt_out.reset(new PSOPCEncryption(cmd.client_key));
      }
      break;
    case GameVersion::PC:
    case GameVersion::GC:
      if (command == 0x02 || command == 0x17 || command == 0x91 || command == 0x9B) {
        auto& cmd = check_size_t<S_ServerInit_DC_PC_GC_02_17_91_9B>(data,
            offsetof(S_ServerInit_DC_PC_GC_02_17_91_9B, after_message), 0xFFFF);
        if (c->version == GameVersion::GC) {
          c->channel.crypt_in.reset(new PSOGCEncryption(cmd.server_key));
          c->channel.crypt_out.reset(new PSOGCEncryption(cmd.client_key));
        } else {
          c->channel.crypt_in.reset(new PSOPCEncryption(cmd.server_key));
          c->channel.crypt_out.reset(new PSOPCEncryption(cmd.client_key));
        }
      }
      break;
    case GameVersion::BB:
      if (command == 0x03 || command == 0x9B) {
        auto& cmd = check_size_t<S_ServerInit_BB_03_9B>(data,
            sizeof(S_ServerInit_BB_03_9B), 0xFFFF);
        // TODO: At some point it may matter which BB private key file we use.
        // Don't just blindly use the first one here.
        c->channel.crypt_in.reset(new PSOBBEncryption(
            *this->state->bb_private_keys[0], cmd.server_key.data(), cmd.server_key.size()));
        c->channel.crypt_out.reset(new PSOBBEncryption(
            *this->state->bb_private_keys[0], cmd.client_key.data(), cmd.client_key.size()));
      }
      break;
    default:
      throw logic_error("unsupported encryption version");
  }

  this->update_timeout_event();
  this->execute_pending_events();
}

void ReplaySession::on_error(shared_ptr<Client> c, short events) {
  if (events & BEV_EVENT_ERROR) {
    throw runtime_error(string_printf("C-%" PRIX64 " caused stream error", c->id));
  }
  if (events & BEV_EVENT_EOF) {
    if (!c->disconnect_event.get()) {
      throw runtime_error(string_printf(
          "C-%" PRIX64 " disconnected, but has no disconnect event", c->id));
    }
    if (!c->receive_events.empty()) {
      throw runtime_error(string_printf(
          "C-%" PRIX64 " disconnected, but has pending receive events", c->id));
    }
    c->disconnect_event->complete = true;
    this->channel_to_client.erase(&c->channel);
    c->channel.disconnect();
  }
}
