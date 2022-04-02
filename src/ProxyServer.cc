#include "ProxyServer.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"

using namespace std;



static const uint32_t SESSION_TIMEOUT_USECS = 30000000; // 30 seconds



static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}



ProxyServer::ProxyServer(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
  : save_files(false),
    base(base),
    state(state),
    next_unlicensed_session_id(0xFF00000000000001) { }

void ProxyServer::listen(uint16_t port, GameVersion version,
    const struct sockaddr_storage* default_destination) {
  shared_ptr<ListeningSocket> socket_obj(new ListeningSocket(
      this, port, version, default_destination));
  auto l = this->listeners.emplace(port, socket_obj).first->second;
  log(INFO, "[ProxyServer] Listening on TCP port %hu (%s) on fd %d",
      port, name_for_version(version), static_cast<int>(l->fd));
}

ProxyServer::ListeningSocket::ListeningSocket(
    ProxyServer* server,
    uint16_t port,
    GameVersion version,
    const struct sockaddr_storage* default_destination)
  : server(server),
    port(port),
    fd(::listen("", port, SOMAXCONN)),
    listener(nullptr, evconnlistener_free),
    version(version) {
  if (!this->fd.is_open()) {
    throw runtime_error("cannot listen on port");
  }
  this->listener.reset(evconnlistener_new(
      this->server->base.get(),
      &ProxyServer::ListeningSocket::dispatch_on_listen_accept,
      this,
      LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
      0,
      this->fd));
  if (!listener) {
    throw runtime_error("cannot create listener");
  }
  evconnlistener_set_error_cb(
      this->listener.get(), &ProxyServer::ListeningSocket::dispatch_on_listen_error);

  if (default_destination) {
    this->default_destination = *default_destination;
  } else {
    this->default_destination.ss_family = 0;
  }
}

void ProxyServer::ListeningSocket::dispatch_on_listen_accept(
    struct evconnlistener*, evutil_socket_t, struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_accept(address, socklen);
}

void ProxyServer::ListeningSocket::dispatch_on_listen_error(
    struct evconnlistener*, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_error();
}

void ProxyServer::ListeningSocket::on_listen_accept(struct sockaddr*, int fd) {
  log(INFO, "[ProxyServer] Client connected on fd %d", fd);
  auto* bev = bufferevent_socket_new(this->server->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  this->server->on_client_connect(bev, this->port, this->version,
      (this->default_destination.ss_family == AF_INET) ? &this->default_destination : nullptr);
}

void ProxyServer::ListeningSocket::on_listen_error() {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[ProxyServer] Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(this->listener.get()),
      err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->server->base.get(), nullptr);
}



void ProxyServer::connect_client(struct bufferevent* bev, uint16_t server_port) {
  // Look up the listening socket for the given port, and use that game version.
  // We don't support default-destination proxying for virtual connections (yet)
  GameVersion version;
  try {
    version = this->listeners.at(server_port)->version;
  } catch (const out_of_range&) {
    log(INFO, "[ProxyServer] Virtual connection received on unregistered port %hu; closing it",
        server_port);
    flush_and_free_bufferevent(bev);
    return;
  }

  log(INFO, "[ProxyServer] Client connected on virtual connection %p", bev);
  this->on_client_connect(bev, server_port, version, nullptr);
}



void ProxyServer::on_client_connect(
    struct bufferevent* bev,
    uint16_t listen_port,
    GameVersion version,
    const struct sockaddr_storage* default_destination) {
  // If a default destination exists for this client, create a linked session
  // immediately and connect to the remote server. This creates a direct session
  if (default_destination) {
    uint64_t session_id = this->next_unlicensed_session_id++;
    if (this->next_unlicensed_session_id == 0) {
      this->next_unlicensed_session_id = 0xFF00000000000001;
    }

    auto emplace_ret = this->id_to_session.emplace(session_id, new LinkedSession(
        this, session_id, listen_port, version, *default_destination));
    if (!emplace_ret.second) {
      throw logic_error("linked session already exists for unlicensed client");
    }
    auto session = emplace_ret.first->second;
    log(INFO, "[ProxyServer/%08" PRIX64 "] Opened session", session->id);
    session->resume(bev);

  // If no default destination exists, create an unlinked session - we'll have
  // to get the destination from the client's config, which we'll get via a 9E
  // command soon
  } else {
    auto emplace_ret = this->bev_to_unlinked_session.emplace(bev, new UnlinkedSession(
        this, bev, listen_port, version));
    if (!emplace_ret.second) {
      throw logic_error("stale unlinked session exists");
    }
    auto session = emplace_ret.first->second;

    switch (version) {
      case GameVersion::PATCH:
        throw logic_error("cannot create unlinked patch session");
      case GameVersion::PC:
      case GameVersion::GC: {
        uint32_t server_key = random_object<uint32_t>();
        uint32_t client_key = random_object<uint32_t>();
        auto cmd = prepare_server_init_contents_dc_pc_gc(
            false, server_key, client_key);
        send_command(session->bev.get(), session->version,
            session->crypt_out.get(), 0x02, 0, &cmd, sizeof(cmd),
            "unlinked proxy client");
        session->crypt_out.reset(new PSOGCEncryption(server_key));
        session->crypt_in.reset(new PSOGCEncryption(client_key));
        break;
      }
      default:
        throw logic_error("unsupported game version on proxy server");
    }
  }
}



ProxyServer::UnlinkedSession::UnlinkedSession(
    ProxyServer* server, struct bufferevent* bev, uint16_t local_port, GameVersion version)
  : server(server),
    bev(bev, flush_and_free_bufferevent),
    local_port(local_port),
    version(version) {
  bufferevent_setcb(this->bev.get(),
      &UnlinkedSession::dispatch_on_client_input, nullptr,
      &UnlinkedSession::dispatch_on_client_error, this);
  bufferevent_enable(this->bev.get(), EV_READ | EV_WRITE);
}

void ProxyServer::UnlinkedSession::dispatch_on_client_input(
    struct bufferevent*, void* ctx) {
  reinterpret_cast<UnlinkedSession*>(ctx)->on_client_input();
}

void ProxyServer::UnlinkedSession::dispatch_on_client_error(
    struct bufferevent*, short events, void* ctx) {
  reinterpret_cast<UnlinkedSession*>(ctx)->on_client_error(events);
}

void ProxyServer::UnlinkedSession::on_client_input() {
  bool should_close_unlinked_session = false;
  shared_ptr<const License> license;
  uint32_t sub_version = 0;
  string character_name;
  ClientConfig client_config;

  for_each_received_command(this->bev.get(), this->version, this->crypt_in.get(),
    [&](uint16_t command, uint32_t flag, const string& data) {
      print_received_command(command, flag, data.data(), data.size(),
          this->version, "unlinked proxy client");

      if (this->version == GameVersion::GC) {
        // We should really only get a 9E while the session is unlinked; if we
        // get anything else, disconnect
        if (command != 0x9E) {
          log(ERROR, "[ProxyServer] Received unexpected command %02hX", command);
          should_close_unlinked_session = true;
        } else if (data.size() < sizeof(C_Login_PC_GC_9D_9E) - 0x64) {
          log(ERROR, "[ProxyServer] Login command is too small");
          should_close_unlinked_session = true;
        } else {
          const auto* cmd = reinterpret_cast<const C_Login_PC_GC_9D_9E*>(data.data());
          uint32_t serial_number = strtoul(cmd->serial_number.c_str(), nullptr, 16);
          try {
            license = this->server->state->license_manager->verify_gc(
                serial_number, cmd->access_key.c_str(), nullptr);
            sub_version = cmd->sub_version;
            character_name = cmd->name;
            client_config = cmd->client_config.cfg;
          } catch (const exception& e) {
            log(ERROR, "[ProxyServer] Unlinked client has no valid license");
            should_close_unlinked_session = true;
          }
        }
      }
    });

  struct bufferevent* session_key = this->bev.get();

  // If license is non-null, then the client has a password and can be connected
  // to the remote lobby server.
  if (license.get()) {
    // At this point, we will always close the unlinked session, even if it
    // doesn't get converted/merged to a linked session
    should_close_unlinked_session = true;

    // Look up the linked session for this license (if any)
    shared_ptr<LinkedSession> session;
    try {
      session = this->server->id_to_session.at(license->serial_number);

    } catch (const out_of_range&) {
      // If there's no open session for this license, then there must be a valid
      // destination in the client config. If there is, open a new linked
      // session and set its initial destination
      if (client_config.magic != CLIENT_CONFIG_MAGIC) {
        log(ERROR, "[ProxyServer] Client configuration is invalid; cannot open session");
      } else {
        session.reset(new LinkedSession(
            this->server,
            this->local_port,
            this->version,
            license,
            client_config));
        this->server->id_to_session.emplace(license->serial_number, session);
        log(INFO, "[ProxyServer/%08" PRIX64 "] Opened session", session->id);
      }
    }

    if (session.get() && (session->version != this->version)) {
      log(ERROR, "[ProxyServer/%08" PRIX64 "] Linked session has different game version",
          session->id);
    } else {
      // Resume the linked session using the unlinked session
      try {
        session->resume(move(this->bev), this->crypt_in, this->crypt_out, sub_version, character_name);
        this->crypt_in.reset();
        this->crypt_out.reset();
      } catch (const exception& e) {
        log(ERROR, "[ProxyServer/%08" PRIX64 "] Failed to resume linked session: %s",
            session->id, e.what());
      }
    }
  }

  if (should_close_unlinked_session) {
    this->server->bev_to_unlinked_session.erase(session_key);
    // At this point, (*this) is destroyed! We must be careful not to touch it.
  }
}

void ProxyServer::UnlinkedSession::on_client_error(short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[ProxyServer] Error %d (%s) in unlinked client stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    log(WARNING, "[ProxyServer] Unlinked client has disconnected");
    this->server->bev_to_unlinked_session.erase(this->bev.get());
  }
}



ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint64_t id,
    uint16_t local_port,
    GameVersion version)
  : server(server),
    id(id),
    timeout_event(event_new(this->server->base.get(), -1, EV_TIMEOUT,
        &LinkedSession::dispatch_on_timeout, this), event_free),
    license(nullptr),
    client_bev(nullptr, flush_and_free_bufferevent),
    server_bev(nullptr, flush_and_free_bufferevent),
    local_port(local_port),
    version(version),
    sub_version(0), // This is set during resume()
    guild_card_number(0),
    suppress_newserv_commands(true),
    enable_chat_filter(true),
    override_section_id(-1),
    override_lobby_event(-1),
    override_lobby_number(-1),
    lobby_players(12),
    lobby_client_id(0) { }

ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint16_t local_port,
    GameVersion version,
    std::shared_ptr<const License> license,
    const ClientConfig& newserv_client_config)
  : LinkedSession(server, license->serial_number, local_port, version) {
  this->newserv_client_config = newserv_client_config;
  memset(&this->next_destination, 0, sizeof(this->next_destination));
  struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&this->next_destination);
  dest_sin->sin_family = AF_INET;
  dest_sin->sin_port = htons(this->newserv_client_config.proxy_destination_port);
  dest_sin->sin_addr.s_addr = htonl(this->newserv_client_config.proxy_destination_address);
}

ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint64_t id,
    uint16_t local_port,
    GameVersion version,
    const struct sockaddr_storage& destination)
  : LinkedSession(server, id, local_port, version) {
  this->next_destination = destination;
}

void ProxyServer::LinkedSession::resume(
    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)>&& client_bev,
    std::shared_ptr<PSOEncryption> client_input_crypt,
    std::shared_ptr<PSOEncryption> client_output_crypt,
    uint32_t sub_version,
    const string& character_name) {
  if (this->client_bev.get()) {
    throw runtime_error("client connection is already open for this session");
  }
  if (this->next_destination.ss_family != AF_INET) {
    throw logic_error("attempted to resume an unlicensed linked session wihout destination set");
  }

  this->client_bev = move(client_bev);
  bufferevent_setcb(this->client_bev.get(),
      &ProxyServer::LinkedSession::dispatch_on_client_input, nullptr,
      &ProxyServer::LinkedSession::dispatch_on_client_error, this);
  bufferevent_enable(this->client_bev.get(), EV_READ | EV_WRITE);

  this->client_input_crypt = client_input_crypt;
  this->client_output_crypt = client_output_crypt;
  this->sub_version = sub_version;
  this->character_name = character_name;
  this->server_input_crypt.reset();
  this->server_output_crypt.reset();
  this->saving_files.clear();

  this->connect();
}

void ProxyServer::LinkedSession::resume(struct bufferevent* client_bev) {
  if (this->client_bev.get()) {
    throw runtime_error("client connection is already open for this session");
  }

  this->client_bev.reset(client_bev);
  bufferevent_setcb(this->client_bev.get(),
      &ProxyServer::LinkedSession::dispatch_on_client_input, nullptr,
      &ProxyServer::LinkedSession::dispatch_on_client_error, this);
  bufferevent_enable(this->client_bev.get(), EV_READ | EV_WRITE);

  this->client_input_crypt.reset();
  this->client_output_crypt.reset();
  this->server_input_crypt.reset();
  this->server_output_crypt.reset();
  this->sub_version = 0;
  this->character_name.clear();
  this->saving_files.clear();

  this->connect();
}

void ProxyServer::LinkedSession::connect() {
  // Connect to the remote server. The command handlers will do the login steps
  // and set up forwarding
  this->server_bev.reset(bufferevent_socket_new(this->server->base.get(), -1,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));

  struct sockaddr_storage local_ss;
  struct sockaddr_in* local_sin = reinterpret_cast<struct sockaddr_in*>(&local_ss);
  memset(local_sin, 0, sizeof(*local_sin));
  local_sin->sin_family = AF_INET;
  const struct sockaddr_in* dest_sin = reinterpret_cast<const sockaddr_in*>(&this->next_destination);
  if (dest_sin->sin_family != AF_INET) {
    throw logic_error("ss not AF_INET");
  }
  local_sin->sin_port = dest_sin->sin_port;
  local_sin->sin_addr.s_addr = dest_sin->sin_addr.s_addr;

  string netloc_str = render_sockaddr_storage(local_ss);
  log(INFO, "[ProxyServer/%08" PRIX64 "] Connecting to %s", this->id, netloc_str.c_str());
  if (bufferevent_socket_connect(this->server_bev.get(),
      reinterpret_cast<const sockaddr*>(local_sin), sizeof(*local_sin)) != 0) {
    throw runtime_error(string_printf("failed to connect (%d)", EVUTIL_SOCKET_ERROR()));
  }
  bufferevent_setcb(this->server_bev.get(),
      &ProxyServer::LinkedSession::dispatch_on_server_input, nullptr,
      &ProxyServer::LinkedSession::dispatch_on_server_error, this);
  bufferevent_enable(this->server_bev.get(), EV_READ | EV_WRITE);

  // Cancel the session delete timeout
  event_del(this->timeout_event.get());
}



ProxyServer::LinkedSession::SavingFile::SavingFile(
    const std::string& basename,
    const std::string& output_filename,
    uint32_t remaining_bytes)
  : basename(basename),
    output_filename(output_filename),
    remaining_bytes(remaining_bytes),
    f(fopen_unique(this->output_filename, "wb")) { }



void ProxyServer::LinkedSession::dispatch_on_client_input(
    struct bufferevent*, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_client_input();
}

void ProxyServer::LinkedSession::dispatch_on_client_error(
    struct bufferevent*, short events, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_stream_error(events, false);
}

void ProxyServer::LinkedSession::dispatch_on_server_input(
    struct bufferevent*, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_server_input();
}

void ProxyServer::LinkedSession::dispatch_on_server_error(
    struct bufferevent*, short events, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_stream_error(events, true);
}

void ProxyServer::LinkedSession::dispatch_on_timeout(
    evutil_socket_t, short, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_timeout();
}



void ProxyServer::LinkedSession::on_timeout() {
  log(INFO, "[ProxyServer/%08" PRIX64 "] Session timed out", this->id);
  this->server->delete_session(this->id);
}



void ProxyServer::LinkedSession::on_stream_error(
    short events, bool is_server_stream) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[ProxyServer/%08" PRIX64 "] Error %d (%s) in %s stream",
        this->id, err, evutil_socket_error_to_string(err),
        is_server_stream ? "server" : "client");
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "[ProxyServer/%08" PRIX64 "] %s has disconnected",
        this->id, is_server_stream ? "Server" : "Client");
    this->disconnect();
  }
}

void ProxyServer::LinkedSession::disconnect() {
  // Forward the disconnection to the other end
  this->server_bev.reset();
  this->client_bev.reset();

  // Disable encryption for the next connection
  this->server_input_crypt.reset();
  this->server_output_crypt.reset();
  this->client_input_crypt.reset();
  this->client_output_crypt.reset();

  // Set a timeout to delete the session entirely (in case the client doesn't
  // reconnect)
  struct timeval tv = usecs_to_timeval(SESSION_TIMEOUT_USECS);
  event_add(this->timeout_event.get(), &tv);
}



static void check_implemented_subcommand(
    uint64_t id, uint16_t command, const string& data) {
  if (command == 0x60 || command == 0x6C || command == 0xC9 ||
      command == 0x62 || command == 0x6D || command == 0xCB) {
    if (data.size() < 4) {
      log(WARNING, "[ProxyServer/%08" PRIX64 "] Received broadcast/target command with no contents", id);
    } else {
      if (!subcommand_is_implemented(data[0])) {
        log(WARNING, "[ProxyServer/%08" PRIX64 "] Received subcommand %02hhX which is not implemented on the server",
            id, data[0]);
      }
    }
  }
}

void ProxyServer::LinkedSession::on_client_input() {
  string name = string_printf("ProxySession:%08" PRIX64 ":client", this->id);
  for_each_received_command(this->client_bev.get(), this->version, this->client_input_crypt.get(),
    [&](uint16_t command, uint32_t flag, string& data) {
      print_received_command(command, flag, data.data(), data.size(),
          this->version, name.c_str());
      check_implemented_subcommand(this->id, command, data);

      bool should_forward = true;
      switch (command) {
        case 0x06:
          if (this->version != GameVersion::GC) {
            break;
          }
          if (data.size() < 12) {
            break;
          }
          // If this chat message looks like a newserv chat command, suppress it
          if (this->suppress_newserv_commands &&
              (data[8] == '$' || (data[8] == '\t' && data[9] != 'C' && data[10] == '$'))) {
            log(WARNING, "[ProxyServer/%08" PRIX64 "] Chat message appears to be a server command; dropping it",
                this->id);
            should_forward = false;
          } else if (this->enable_chat_filter) {
            // Turn all $ into \t and all # into \n
            add_color_inplace(data.data() + 8, data.size() - 8);
          }
          break;

        case 0xA0: // Change ship
        case 0xA1: { // Change block
          if ((this->version == GameVersion::PATCH) || (this->version == GameVersion::BB)) {
            break;
          }
          if (!this->license) {
            break;
          }

          // These will take you back to the newserv main menu instead of the
          // proxied service's menu

          // Delete all the other players
          for (size_t x = 0; x < this->lobby_players.size(); x++) {
            if (this->lobby_players[x].guild_card_number == 0) {
              continue;
            }
            uint8_t leaving_id = x;
            uint8_t leader_id = this->lobby_client_id;
            S_LeaveLobby_66_69 cmd = {leaving_id, leader_id, 0};
            send_command(this->client_bev.get(), this->version,
                this->client_output_crypt.get(), 0x69, leaving_id, &cmd,
                sizeof(cmd), name.c_str());
          }

          // Restore the newserv client config, so the client gets its newserv
          // guild card number back and the login server knows e.g. not to show
          // the welcome message (if the appropriate flag is set)
          S_UpdateClientConfig_DC_PC_GC_04 update_client_config_cmd = {
            0x00010000,
            this->license->serial_number,
            this->newserv_client_config,
          };
          send_command(this->client_bev.get(), this->version,
              this->client_output_crypt.get(), 0x04, 0x00,
              &update_client_config_cmd, sizeof(update_client_config_cmd),
              name.c_str());

          static const vector<string> version_to_port_name({
              "dc-login", "pc-login", "bb-patch", "gc-us3", "bb-login"});
          const auto& port_name = version_to_port_name.at(static_cast<size_t>(
              this->version));

          S_Reconnect_19 reconnect_cmd = {
              0, this->server->state->name_to_port_config.at(port_name)->port, 0};

          // If the client is on a virtual connection, we can use any address
          // here and they should be able to connect back to the game server. If
          // the client is on a real connection, we'll use the sockname of the
          // existing connection (like we do in the server 19 command handler).
          int fd = bufferevent_getfd(this->client_bev.get());
          if (fd < 0) {
            struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&this->next_destination);
            if (dest_sin->sin_family != AF_INET) {
              throw logic_error("ss not AF_INET");
            }
            reconnect_cmd.address.store_raw(dest_sin->sin_addr.s_addr);
          } else {
            struct sockaddr_storage sockname_ss;
            socklen_t len = sizeof(sockname_ss);
            getsockname(fd, reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
            if (sockname_ss.ss_family != AF_INET) {
              throw logic_error("existing connection is not ipv4");
            }

            struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
                &sockname_ss);
            reconnect_cmd.address.store_raw(sockname_sin->sin_addr.s_addr);
          }

          send_command(this->client_bev.get(), this->version,
              this->client_output_crypt.get(), 0x19, 0x00, &reconnect_cmd,
              sizeof(reconnect_cmd), name.c_str());
          break;
        }
      }

      if (should_forward) {
        if (!this->client_bev.get()) {
          log(WARNING, "[ProxyServer/%08" PRIX64 "] No server is present; dropping command",
              this->id);
        } else {
          // Note: we intentionally don't pass a name string here because we already
          // printed the command above
          send_command(this->server_bev.get(), this->version,
              this->server_output_crypt.get(), command, flag,
              data.data(), data.size());
        }
      }
    });
}

void ProxyServer::LinkedSession::on_server_input() {
  string name = string_printf("ProxySession:%08" PRIX64 ":server", this->id);

  try {
    for_each_received_command(this->server_bev.get(), this->version, this->server_input_crypt.get(),
      [&](uint16_t command, uint32_t flag, string& data) {
        print_received_command(command, flag, data.data(), data.size(),
            this->version, name.c_str());
        check_implemented_subcommand(this->id, command, data);

        // In the case of server init commands, the client output crypt cannot
        // be set until after we forwarwd the command to the client, hence this
        // variable.
        shared_ptr<PSOEncryption> new_client_output_crypt;
        bool should_forward = true;
        switch (command) {
          case 0x02:
          case 0x17: {
            if (this->version == GameVersion::PATCH && command == 0x17) {
              throw invalid_argument("patch server sent 17 server init");
            }
            if (this->version == GameVersion::BB) {
              throw invalid_argument("console server init received on BB");
            }

            // Most servers don't include after_message or have a shorter
            // after_message than newserv does, so don't require it
            if (data.size() < offsetof(S_ServerInit_DC_GC_02_17, after_message)) {
              throw std::runtime_error("init encryption command is too small");
            }
            const auto* cmd = reinterpret_cast<const S_ServerInit_DC_GC_02_17*>(
                data.data());

            if (!this->license) {
              if ((this->version == GameVersion::PC) || (this->version == GameVersion::PATCH)) {
                this->server_input_crypt.reset(new PSOPCEncryption(cmd->server_key));
                this->server_output_crypt.reset(new PSOPCEncryption(cmd->client_key));
                this->client_input_crypt.reset(new PSOPCEncryption(cmd->client_key));
                new_client_output_crypt.reset(new PSOPCEncryption(cmd->server_key));
              } else if (this->version == GameVersion::GC) {
                this->server_input_crypt.reset(new PSOGCEncryption(cmd->server_key));
                this->server_output_crypt.reset(new PSOGCEncryption(cmd->client_key));
                this->client_input_crypt.reset(new PSOGCEncryption(cmd->client_key));
                new_client_output_crypt.reset(new PSOGCEncryption(cmd->server_key));
              } else {
                throw invalid_argument("unsupported version");
              }
              break;

            } else {
              // This doesn't get forwarded to the client, so don't recreate the
              // client's crypts
              if (this->version == GameVersion::PATCH) {
                throw logic_error("patch session is indirect");
              } else if (this->version == GameVersion::PC) {
                this->server_input_crypt.reset(new PSOPCEncryption(cmd->server_key));
                this->server_output_crypt.reset(new PSOPCEncryption(cmd->client_key));
              } else if (this->version == GameVersion::GC) {
                this->server_input_crypt.reset(new PSOGCEncryption(cmd->server_key));
                this->server_output_crypt.reset(new PSOGCEncryption(cmd->client_key));
              } else {
                throw invalid_argument("unsupported version");
              }

              should_forward = false;

              // If this is a 17, respond with a DB; otherwise respond with a 9E.
              // We don't let the client do this because it believes it already
              // did (when it was in an unlinked session).
              if (command == 0x17) {
                C_VerifyLicense_GC_DB cmd;
                cmd.serial_number = string_printf("%08" PRIX32 "",
                    this->license->serial_number);
                cmd.access_key = this->license->access_key;
                cmd.sub_version = this->sub_version;
                cmd.serial_number2 = cmd.serial_number;
                cmd.access_key2 = cmd.access_key;
                cmd.password = this->license->gc_password;
                send_command(this->server_bev.get(), this->version,
                    this->server_output_crypt.get(), 0xDB, 0, &cmd, sizeof(cmd),
                    name.c_str());
                break;
              }
              // Command 02 should be handled like 9A at this point (we should
              // send a 9E in response)
              [[fallthrough]];
            }
          }

          case 0x9A: {
            if (!this->license) {
              break;
            }
            should_forward = false;
            C_Login_PC_GC_9D_9E cmd;

            if (this->guild_card_number == 0) {
              cmd.player_tag = 0xFFFF0000;
              cmd.guild_card_number = 0xFFFFFFFF;
            } else {
              cmd.player_tag = 0x00010000;
              cmd.guild_card_number = this->guild_card_number;
            }
            cmd.sub_version = this->sub_version;
            cmd.unused2.data()[1] = 1;
            cmd.serial_number = string_printf("%08" PRIX32 "",
                this->license->serial_number);
            cmd.access_key = this->license->access_key;
            cmd.serial_number2 = cmd.serial_number;
            cmd.access_key2 = cmd.access_key;
            cmd.name = this->character_name;
            cmd.client_config.data = this->remote_client_config_data;

            // If there's a guild card number, a shorter 9E is sent that ends
            // right after the client config data
            send_command(
                this->server_bev.get(),
                this->version,
                this->server_output_crypt.get(),
                0x9E,
                0x01,
                &cmd,
                this->guild_card_number ? offsetof(C_Login_PC_GC_9D_9E, unused4) : sizeof(cmd),
                name.c_str());
            break;
          }

          case 0x04: {
            if (this->version != GameVersion::GC) {
              break;
            }

            // Some servers send a short 04 command if they don't use all of the
            // 0x20 bytes available. We should be prepared to handle that.
            if (data.size() < offsetof(S_UpdateClientConfig_DC_PC_GC_04, cfg)) {
              throw std::runtime_error("set security data command is too small");
            }

            bool had_guild_card_number = (this->guild_card_number != 0);

            const auto* cmd = reinterpret_cast<const S_UpdateClientConfig_DC_PC_GC_04*>(data.data());
            this->guild_card_number = cmd->guild_card_number;
            log(INFO, "[ProxyServer/%08" PRIX64 "] Guild card number set to %" PRIX32,
                this->id, this->guild_card_number);

            // It seems the client ignores the length of the 04 command, and
            // always copies 0x20 bytes to its config data. So if the server
            // sends a short 04 command, part of the previous command ends up in
            // the security data (usually part of the copyright string from the
            // server init command). We simulate that bug here.
            // If there was previously a guild card number, assume we got the
            // lobby server init text instead of the port map init text.
            memcpy(this->remote_client_config_data.data(),
                had_guild_card_number
                  ? "t Lobby Server. Copyright SEGA E"
                  : "t Port Map. Copyright SEGA Enter",
                this->remote_client_config_data.bytes());
            memcpy(this->remote_client_config_data.data(), &cmd->cfg,
                min<size_t>(data.size() - sizeof(S_UpdateClientConfig_DC_PC_GC_04),
                  this->remote_client_config_data.bytes()));

            // If the guild card number was not set, pretend (to the server)
            // that this is the first 04 command the client has received. The
            // client responds with a 96 (checksum) in that case.
            if (!had_guild_card_number) {
              // We don't actually have a client checksum, of course...
              // hopefully just random data will do (probably no private servers
              // check this at all)
              le_uint64_t checksum = random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
              send_command(this->server_bev.get(), this->version,
                  this->server_output_crypt.get(), 0x96, 0x00, &checksum,
                  sizeof(checksum), name.c_str());
            }

            break;
          }

          case 0x19: {
            if (this->version == GameVersion::PATCH) {
              break;
            }

            if (data.size() < sizeof(S_Reconnect_19)) {
              throw std::runtime_error("reconnect command is too small");
            }

            auto* args = reinterpret_cast<S_Reconnect_19*>(data.data());
            memset(&this->next_destination, 0, sizeof(this->next_destination));
            struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
                &this->next_destination);
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = args->address.load_raw();
            sin->sin_port = htons(args->port);

            if (!this->client_bev.get()) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Received reconnect command with no destination present",
                  this->id);

            } else {
              // If the client is on a virtual connection (fd < 0), only change
              // the port (so we'll know which version to treat the next
              // connection as). It's better to leave the address as-is so we
              // can circumvent the Plus/Ep3 same-network-server check.
              int fd = bufferevent_getfd(this->client_bev.get());
              if (fd >= 0) {
                struct sockaddr_storage sockname_ss;
                socklen_t len = sizeof(sockname_ss);
                getsockname(fd, reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
                if (sockname_ss.ss_family != AF_INET) {
                  throw logic_error("existing connection is not ipv4");
                }

                struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
                    &sockname_ss);
                args->address.store_raw(sockname_sin->sin_addr.s_addr);
                args->port = ntohs(sockname_sin->sin_port);

              } else {
                args->port = this->local_port;
              }
            }
            break;
          }

          case 0x1A:
          case 0xD5: {
            if (this->version != GameVersion::PATCH) {
              break;
            }

            // If the client has the no-close-confirmation flag set in its
            // newserv client config, send a fake confirmation to the remote
            // server immediately.
            if (this->newserv_client_config.flags & Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION) {
              send_command(this->server_bev.get(), this->version,
                  this->server_output_crypt.get(), 0xD6, 0x00, "", 0,
                  name.c_str());
            }
            break;
          }

          case 0x44:
          case 0xA6: {
            if (this->version != GameVersion::GC) {
              break;
            }
            if (!this->server->save_files) {
              break;
            }

            bool is_download_quest = (command == 0xA6);

            if (data.size() < sizeof(S_OpenFile_PC_GC_44_A6)) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Open file command is too small; skipping file",
                  this->id);
              break;
            }
            const auto* cmd = reinterpret_cast<const S_OpenFile_PC_GC_44_A6*>(data.data());

            string output_filename = string_printf("%s.%s.%" PRIu64,
                cmd->filename.c_str(),
                is_download_quest ? "download" : "online", now());
            for (size_t x = 0; x < output_filename.size(); x++) {
              if (output_filename[x] < 0x20 || output_filename[x] > 0x7E || output_filename[x] == '/') {
                output_filename[x] = '_';
              }
            }
            if (output_filename[0] == '.') {
              output_filename[0] = '_';
            }

            SavingFile sf(cmd->filename, output_filename, cmd->file_size);
            this->saving_files.emplace(cmd->filename, move(sf));
            log(INFO, "[ProxyServer/%08" PRIX64 "] Opened file %s",
                this->id, output_filename.c_str());
            break;
          }

          case 0x13:
          case 0xA7: {
            if (this->version != GameVersion::GC) {
              break;
            }
            if (!this->server->save_files) {
              break;
            }

            if (data.size() < sizeof(S_WriteFile_13_A7)) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Write file command is too small",
                  this->id);
              break;
            }
            const auto* cmd = reinterpret_cast<const S_WriteFile_13_A7*>(data.data());

            SavingFile* sf = nullptr;
            try {
              sf = &this->saving_files.at(cmd->filename);
            } catch (const out_of_range&) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Received data for non-open file %s",
                  this->id, cmd->filename.c_str());
              break;
            }

            size_t bytes_to_write = cmd->data_size;
            if (bytes_to_write > 0x400) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Chunk data size is invalid; truncating to 0x400",
                  this->id);
              bytes_to_write = 0x400;
            }

            log(INFO, "[ProxyServer/%08" PRIX64 "] Writing %zu bytes to %s",
                this->id, bytes_to_write,
                sf->output_filename.c_str());
            fwritex(sf->f.get(), cmd->data, bytes_to_write);
            if (bytes_to_write > sf->remaining_bytes) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Chunk size extends beyond original file size; file may be truncated",
                  this->id);
              sf->remaining_bytes = 0;
            } else {
              sf->remaining_bytes -= bytes_to_write;
            }

            if (sf->remaining_bytes == 0) {
              log(INFO, "[ProxyServer/%08" PRIX64 "] File %s is complete",
                  this->id, sf->output_filename.c_str());
              this->saving_files.erase(cmd->filename);
            }
            break;
          }

          case 0xB8: {
            if (this->version != GameVersion::GC) {
              break;
            }
            if (!this->server->save_files) {
              break;
            }
            if (data.size() < 4) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Card list data size is too small; skipping file",
                  this->id);
              break;
            }

            StringReader r(data);
            size_t size = r.get_u32l();
            if (r.remaining() < size) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Card list data size extends beyond end of command; skipping file",
                  this->id);
              break;
            }

            string output_filename = string_printf("cardupdate.mnr.%" PRIu64, now());
            save_file(output_filename, r.read(size));

            log(INFO, "[ProxyServer/%08" PRIX64 "] Wrote %zu bytes to %s",
                this->id, size, output_filename.c_str());
            break;
          }

          case 0x67: // join lobby
            if (this->version != GameVersion::GC) {
              break;
            }

            this->lobby_players.clear();
            this->lobby_players.resize(12);
            log(WARNING, "[ProxyServer/%08" PRIX64 "] Cleared lobby players",
                this->id);

            // This command can cause the client to no longer send D6 responses
            // when 1A/D5 large message boxes are closed. newserv keeps track of
            // this behavior in the client config, so if it happens during a
            // proxy session, update the client config that we'll restore if the
            // client uses the change ship or change block command.
            if (this->newserv_client_config.flags & Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN) {
              this->newserv_client_config.flags |= Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION;
            }

            [[fallthrough]];

          case 0x65: // other player joined game
          case 0x68: { // other player joined lobby
            if (this->version != GameVersion::GC) {
              break;
            }

            size_t expected_size = offsetof(S_JoinLobby_GC_65_67_68, entries) + sizeof(S_JoinLobby_GC_65_67_68::Entry) * flag;
            if (data.size() < expected_size) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Lobby join command is incorrect size (expected 0x%zX bytes, received 0x%zX bytes)",
                  this->id, expected_size, data.size());
            } else {
              auto* cmd = reinterpret_cast<S_JoinLobby_GC_65_67_68*>(data.data());

              this->lobby_client_id = cmd->client_id;

              for (size_t x = 0; x < flag; x++) {
                size_t index = cmd->entries[x].lobby_data.client_id;
                if (index >= this->lobby_players.size()) {
                  log(WARNING, "[ProxyServer/%08" PRIX64 "] Ignoring invalid player index %zu at position %zu",
                      this->id, index, x);
                } else {
                  this->lobby_players[index].guild_card_number = cmd->entries[x].lobby_data.guild_card;
                  this->lobby_players[index].name = cmd->entries[x].disp.name;
                  log(INFO, "[ProxyServer/%08" PRIX64 "] Added lobby player: (%zu) %" PRIu32 " %s",
                      this->id, index,
                      this->lobby_players[index].guild_card_number,
                      this->lobby_players[index].name.c_str());
                }
              }

              if (this->override_lobby_event >= 0) {
                cmd->event = this->override_lobby_event;
              }
              if (this->override_lobby_event >= 0) {
                cmd->lobby_number = this->override_lobby_number;
              }
            }
            break;
          }

          case 0x64: { // join game
            if (this->version != GameVersion::GC) {
              break;
            }

            // We don't need to clear lobby_players here because we always
            // overwrite all 4 entries in this case
            this->lobby_players.resize(4);
            log(WARNING, "[ProxyServer/%08" PRIX64 "] Cleared lobby players",
                this->id);

            const size_t expected_size = offsetof(S_JoinGame_GC_64, players_ep3);
            const size_t ep3_expected_size = sizeof(S_JoinGame_GC_64);
            if (data.size() < expected_size) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Game join command is incorrect size (expected 0x%zX bytes, received 0x%zX bytes)",
                  this->id, expected_size, data.size());
            } else {
              auto* cmd = reinterpret_cast<S_JoinGame_GC_64*>(data.data());

              this->lobby_client_id = cmd->client_id;

              for (size_t x = 0; x < flag; x++) {
                this->lobby_players[x].guild_card_number = cmd->lobby_data[x].guild_card;
                if (data.size() >= ep3_expected_size) {
                  this->lobby_players[x].name = cmd->players_ep3[x].disp.name;
                } else {
                  this->lobby_players[x].name.clear();
                }
                log(INFO, "[ProxyServer/%08" PRIX64 "] Added lobby player: (%zu) %" PRIu32 " %s",
                    this->id, x,
                    this->lobby_players[x].guild_card_number,
                    this->lobby_players[x].name.c_str());
              }

              if (this->override_section_id >= 0) {
                cmd->section_id = this->override_section_id;
              }
              if (this->override_lobby_event >= 0) {
                cmd->event = this->override_lobby_event;
              }
            }
            break;
          }

          case 0x66:
          case 0x69: {
            if (this->version != GameVersion::GC) {
              break;
            }

            if (data.size() < sizeof(S_LeaveLobby_66_69)) {
              log(WARNING, "[ProxyServer/%08" PRIX64 "] Lobby leave command is incorrect size",
                  this->id);
            } else {
              const auto* cmd = reinterpret_cast<const S_LeaveLobby_66_69*>(data.data());
              size_t index = cmd->client_id;
              if (index >= this->lobby_players.size()) {
                log(WARNING, "[ProxyServer/%08" PRIX64 "] Lobby leave command references missing position",
                  this->id);
              } else {
                this->lobby_players[index].guild_card_number = 0;
                this->lobby_players[index].name.clear();
                log(INFO, "[ProxyServer/%08" PRIX64 "] Removed lobby player (%zu)",
                    this->id, index);
              }
            }
            break;
          }
        }

        if (should_forward) {
          if (!this->client_bev.get()) {
            log(WARNING, "[ProxyServer/%08" PRIX64 "] No client is present; dropping command",
                this->id);
          } else {
            // Note: we intentionally don't pass name_str here because we already
            // printed the command above
            send_command(this->client_bev.get(), this->version,
                this->client_output_crypt.get(), command, flag,
                data.data(), data.size());
          }
        }

        if (new_client_output_crypt.get()) {
          this->client_output_crypt = new_client_output_crypt;
        }
      });

  } catch (const exception& e) {
    log(ERROR, "[ProxyServer/%08" PRIX64 "] Failed to process server command: %s",
        this->id, e.what());
    this->disconnect();
  }
}

void ProxyServer::LinkedSession::send_to_end(
    const void* data, size_t size, bool to_server) {
  size_t header_size = PSOCommandHeader::header_size(this->version);
  if (size < header_size) {
    throw runtime_error("command is too small for header");
  }
  if (size & 3) {
    throw runtime_error("command size is not a multiple of 4");
  }
  const auto* header = reinterpret_cast<const PSOCommandHeader*>(data);

  string name = string_printf("ProxySession:%08" PRIX64 ":shell:%s",
      this->id, to_server ? "server" : "client");

  send_command(
      to_server ? this->server_bev.get() : this->client_bev.get(),
      this->version,
      to_server ? this->server_output_crypt.get() : this->client_output_crypt.get(),
      header->command(this->version),
      header->flag(this->version),
      reinterpret_cast<const uint8_t*>(data) + header_size,
      size - header_size,
      name.c_str());
}

void ProxyServer::LinkedSession::send_to_end(const string& data, bool to_server) {
  this->send_to_end(data.data(), data.size(), to_server);
}

shared_ptr<ProxyServer::LinkedSession> ProxyServer::get_session() {
  if (this->id_to_session.empty()) {
    throw runtime_error("no sessions exist");
  }
  if (this->id_to_session.size() > 1) {
    throw runtime_error("multiple sessions exist");
  }
  return this->id_to_session.begin()->second;
}

void ProxyServer::delete_session(uint64_t id) {
  if (this->id_to_session.erase(id)) {
    log(WARNING, "[ProxyServer/%08" PRIX64 "] Closed session", id);
  }
}
