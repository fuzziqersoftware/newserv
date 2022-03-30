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
    state(state) { }

void ProxyServer::listen(uint16_t port, GameVersion version) {
  int fd = ::listen("", port, SOMAXCONN);
  if (fd < 0) {
    throw runtime_error("cannot listen on port");
  }
  auto* listener = evconnlistener_new(this->base.get(),
      &ProxyServer::ListeningSocket::dispatch_on_listen_accept, this,
      LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 0, fd);
  if (!listener) {
    close(fd);
    throw runtime_error("cannot create listener");
  }
  evconnlistener_set_error_cb(
      listener, &ProxyServer::ListeningSocket::dispatch_on_listen_error);

  unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> evlistener(
      listener, evconnlistener_free);
  shared_ptr<ListeningSocket> socket_obj(new ListeningSocket({
    .server = this,
    .fd = fd,
    .port = port,
    .listener = move(evlistener),
    .version = version,
  }));
  this->listeners.emplace(port, socket_obj);

  log(INFO, "[ProxyServer] Listening on TCP port %hu (%s) on fd %d",
      port, name_for_version(version), fd);
}

void ProxyServer::ListeningSocket::dispatch_on_listen_accept(
    struct evconnlistener*, evutil_socket_t, struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_accept(address, socklen);
}

void ProxyServer::ListeningSocket::dispatch_on_listen_error(
    struct evconnlistener*, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_error();
}

void ProxyServer::ListeningSocket::on_listen_accept(struct sockaddr*, int) {
  log(INFO, "[ProxyServer] Client connected on fd %d", fd);
  auto* bev = bufferevent_socket_new(this->server->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  this->server->on_client_connect(bev, this->port, this->version);
}

void ProxyServer::ListeningSocket::on_listen_error() {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[ProxyServer] Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(this->listener.get()),
      err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->server->base.get(), nullptr);
}



void ProxyServer::connect_client(struct bufferevent* bev, uint16_t server_port) {
  // Look up the listening socket for the given port, and use that game version
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
  this->on_client_connect(bev, server_port, version);
}



void ProxyServer::on_client_connect(
    struct bufferevent* bev, uint16_t listen_port, GameVersion version) {
  auto emplace_ret = this->bev_to_unlinked_session.emplace(bev, new UnlinkedSession(
      this, bev, listen_port, version));
  if (!emplace_ret.second) {
    throw logic_error("stale unlinked session exists");
  }
  auto session = emplace_ret.first->second;

  switch (version) {
    case GameVersion::GC: {
      uint32_t server_key = random_object<uint32_t>();
      uint32_t client_key = random_object<uint32_t>();
      string data = prepare_server_init_contents_dc_pc_gc(false, server_key, client_key);
      send_command(session->bev.get(), session->version, session->crypt_out.get(), 0x02,
          0, data.data(), data.size(), "unlinked proxy client");
      session->crypt_out.reset(new PSOGCEncryption(server_key));
      session->crypt_in.reset(new PSOGCEncryption(client_key));
      break;
    }
    default:
      throw logic_error("unsupported game version on proxy server");
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
        } else if (data.size() < sizeof(LoginCommand_GC_9E) - 0x64) {
          log(ERROR, "[ProxyServer] Login command is too small");
          should_close_unlinked_session = true;
        } else {
          const auto* cmd = reinterpret_cast<const LoginCommand_GC_9E*>(data.data());
          uint32_t serial_number = strtoul(cmd->serial_number, nullptr, 16);
          try {
            license = this->server->state->license_manager->verify_gc(
                serial_number, cmd->access_key, nullptr);
            sub_version = cmd->sub_version;
            character_name = cmd->name;
            memcpy(&client_config, &cmd->cfg, offsetof(ClientConfig, unused_bb_only));
            memset(client_config.unused_bb_only, 0xFF, sizeof(client_config.unused_bb_only));
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
      session = this->server->serial_number_to_session.at(license->serial_number);

    } catch (const out_of_range&) {
      // If there's no open session for this license, then there must be a valid
      // destination in the client config. If there is, open a new linked
      // session and set its initial destination
      if (client_config.magic != CLIENT_CONFIG_MAGIC) {
        log(ERROR, "[ProxyServer/%08" PRIX32 "] Client configuration is invalid; cannot open session",
            license->serial_number);
      } else {
        session.reset(new LinkedSession(
            this->server,
            this->local_port,
            this->version,
            license,
            client_config));
        this->server->serial_number_to_session.emplace(
            license->serial_number, session);
        log(INFO, "[ProxyServer/%08" PRIX32 "] Opened session",
            license->serial_number);
      }
    }

    if (session.get() && (session->version != this->version)) {
      log(ERROR, "[ProxyServer/%08" PRIX32 "] Linked session has different game version",
          session->license->serial_number);
    } else {
      // Resume the linked session using the unlinked session
      try {
        session->resume(move(this->bev), this->crypt_in, this->crypt_out, sub_version, character_name);
        this->crypt_in.reset();
        this->crypt_out.reset();
      } catch (const exception& e) {
        log(ERROR, "[ProxyServer/%08" PRIX32 "] Failed to resume linked session: %s",
            session->license->serial_number, e.what());
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
    uint16_t local_port,
    GameVersion version,
    std::shared_ptr<const License> license,
    const ClientConfig& newserv_client_config)
  : server(server),
    timeout_event(event_new(this->server->base.get(), -1, EV_TIMEOUT,
        &LinkedSession::dispatch_on_timeout, this), event_free),
    license(license),
    client_bev(nullptr, flush_and_free_bufferevent),
    server_bev(nullptr, flush_and_free_bufferevent),
    local_port(local_port),
    version(version),
    sub_version(0), // This is set during resume()
    guild_card_number(0),
    newserv_client_config(newserv_client_config),
    lobby_players(12),
    lobby_client_id(0) {
  memset(this->remote_client_config_data, 0, 0x20);
  memset(&this->next_destination, 0, sizeof(this->next_destination));
  struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&this->next_destination);
  dest_sin->sin_family = AF_INET;
  dest_sin->sin_port = htons(this->newserv_client_config.proxy_destination_port);
  dest_sin->sin_addr.s_addr = htonl(this->newserv_client_config.proxy_destination_address);
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
  log(INFO, "[ProxyServer/%08" PRIX32 "] Connecting to %s", this->license->serial_number, netloc_str.c_str());
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
  log(INFO, "[ProxyServer/%08" PRIX32 "] Session timed out",
      this->license->serial_number);
  this->server->delete_session(this->license->serial_number);
}



void ProxyServer::LinkedSession::on_stream_error(
    short events, bool is_server_stream) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[ProxyServer/%08" PRIX32 "] Error %d (%s) in %s stream",
        this->license->serial_number, err, evutil_socket_error_to_string(err),
        is_server_stream ? "server" : "client");
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "[ProxyServer/%08" PRIX32 "] %s has disconnected",
        this->license->serial_number, is_server_stream ? "Server" : "Client");
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
    uint32_t serial_number, uint16_t command, const string& data) {
  if (command == 0x60 || command == 0x6C || command == 0xC9 ||
      command == 0x62 || command == 0x6D || command == 0xCB) {
    if (data.size() < 4) {
      log(WARNING, "[ProxyServer/%08" PRIX32 "] Received broadcast/target command with no contents", serial_number);
    } else {
      if (!subcommand_is_implemented(data[0])) {
        log(WARNING, "[ProxyServer/%08" PRIX32 "] Received subcommand %02hhX which is not implemented on the server",
            serial_number, data[0]);
      }
    }
  }
}

void ProxyServer::LinkedSession::on_client_input() {
  string name = string_printf("ProxySession:%08" PRIX32 ":client", this->license->serial_number);
  for_each_received_command(this->client_bev.get(), this->version, this->client_input_crypt.get(),
    [&](uint16_t command, uint32_t flag, string& data) {
      print_received_command(command, flag, data.data(), data.size(),
          this->version, name.c_str());
      check_implemented_subcommand(this->license->serial_number, command, data);

      bool should_forward = true;
      switch (command) {
        case 0x06:
          if (data.size() < 12) {
            break;
          }
          // If this chat message looks like a chat command, suppress it
          if (data[8] == '$' || (data[8] == '\t' && data[9] != 'C' && data[10] == '$')) {
            log(WARNING, "[ProxyServer/%08" PRIX32 "] Chat message appears to be a server command; dropping it",
                this->license->serial_number);
            should_forward = false;
          } else {
            // Turn all $ into \t and all # into \n
            add_color_inplace(data.data() + 8, data.size() - 8);
          }
          break;

        case 0xA0: // Change ship
        case 0xA1: { // Change block
          // These will take you back to the newserv main menu instead of the
          // proxied service's menu

          // Restore the newserv client config, so the client gets its newserv
          // guild card number back and the login server knows e.g. not to show
          // the welcome message (if the appropriate flag is set)
          struct {
            uint32_t player_tag;
            uint32_t serial_number;
            ClientConfig config;
          } __attribute__((packed)) update_client_config_cmd = {
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

          ReconnectCommand_19 reconnect_cmd = {
              0, this->server->state->named_port_configuration.at(port_name).port, 0};

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
          log(WARNING, "[ProxyServer/%08" PRIX32 "] No server is present; dropping command",
              this->license->serial_number);
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
  string name = string_printf("ProxySession:%08" PRIX32 ":server", this->license->serial_number);

  try {
    for_each_received_command(this->server_bev.get(), this->version, this->server_input_crypt.get(),
      [&](uint16_t command, uint32_t flag, string& data) {
        print_received_command(command, flag, data.data(), data.size(),
            this->version, name.c_str());
        check_implemented_subcommand(this->license->serial_number, command, data);

        bool should_forward = true;

        switch (command) {
          case 0x02:
          case 0x17: {
            if (this->version == GameVersion::BB) {
              throw invalid_argument("console server init received on BB");
            }
            // Most servers don't include after_message or have a shorter
            // after_message than newserv does, so don't require it
            if (data.size() < offsetof(ServerInitCommand_GC_02_17, after_message)) {
              throw std::runtime_error("init encryption command is too small");
            }

            const auto* cmd = reinterpret_cast<const ServerInitCommand_GC_02_17*>(
                data.data());

            // This doesn't get forwarded to the client, so don't recreate the
            // client's crypts
            if (this->version == GameVersion::PC) {
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
              VerifyLicenseCommand_GC_DB cmd;
              memset(&cmd, 0, sizeof(cmd));
              snprintf(cmd.serial_number, sizeof(cmd.serial_number), "%08" PRIX32 "",
                  this->license->serial_number);
              memcpy(cmd.access_key, this->license->access_key, 0x10);
              cmd.sub_version = this->sub_version;
              snprintf(cmd.serial_number2, sizeof(cmd.serial_number2), "%08" PRIX32 "",
                  this->license->serial_number);
              memcpy(cmd.access_key2, this->license->access_key, 0x10);
              memcpy(cmd.password, this->license->gc_password, 0x0C);
              send_command(this->server_bev.get(), this->version,
                  this->server_output_crypt.get(), 0xDB, 0, &cmd, sizeof(cmd),
                  name.c_str());
              break;
            }
            // Command 02 should be handled like 9A at this point (we should
            // send a 9E in response)
            [[fallthrough]];
          }

          case 0x9A: {
            should_forward = false;
            LoginCommand_GC_9E cmd;
            memset(&cmd, 0, sizeof(cmd));

            if (this->guild_card_number == 0) {
              cmd.player_tag = 0xFFFF0000;
              cmd.guild_card_number = 0xFFFFFFFF;
            } else {
              cmd.player_tag = 0x00010000;
              cmd.guild_card_number = this->guild_card_number;
            }
            cmd.sub_version = this->sub_version;
            cmd.unused2[1] = 1;
            snprintf(cmd.serial_number, sizeof(cmd.serial_number), "%08" PRIX32 "",
                this->license->serial_number);
            memcpy(cmd.access_key, this->license->access_key, 0x10);
            snprintf(cmd.serial_number2, sizeof(cmd.serial_number2), "%08" PRIX32 "",
                this->license->serial_number);
            memcpy(cmd.access_key2, this->license->access_key, 0x10);
            strncpy(cmd.name, this->character_name.c_str(), sizeof(cmd.name) - 1);
            memcpy(&cmd.cfg, this->remote_client_config_data, 0x20);

            // If there's a guild card number, a shorter 9E is sent that ends
            // right after the client config data
            send_command(
                this->server_bev.get(),
                this->version,
                this->server_output_crypt.get(),
                0x9E,
                0x01,
                &cmd,
                this->guild_card_number ? (offsetof(LoginCommand_GC_9E, cfg) + 0x20) : sizeof(cmd),
                name.c_str());
            break;
          }

          case 0x04: {
            struct Contents {
              uint32_t player_tag;
              uint32_t guild_card_number;
              uint8_t client_config[0];
            } __attribute__((packed));

            if (data.size() < sizeof(Contents)) {
              throw std::runtime_error("set security data command is too small");
            }

            bool had_guild_card_number = (this->guild_card_number != 0);

            const auto* cmd = reinterpret_cast<const Contents*>(data.data());
            this->guild_card_number = cmd->guild_card_number;
            log(INFO, "[ProxyServer/%08" PRIX32 "] Guild card number set to %" PRIX32,
                this->license->serial_number, this->guild_card_number);

            // It seems the client ignores the length of the 04 command, and
            // always copies 0x20 bytes to its config data. So if the server
            // sends a short 04 command, part of the previous command ends up in
            // the security data (usually part of the copyright string from the
            // server init command). We simulate that bug here.
            // If there was previously a guild card number, assume we got the
            // lobby server init text instead of the port map init text.
            memcpy(
                this->remote_client_config_data,
                had_guild_card_number
                  ? "t Lobby Server. Copyright SEGA E"
                  : "t Port Map. Copyright SEGA Enter",
                0x20);
            memcpy(this->remote_client_config_data, cmd->client_config, min<size_t>(data.size() - sizeof(Contents), 0x20));

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
            if (data.size() < sizeof(ReconnectCommand_19)) {
              throw std::runtime_error("reconnect command is too small");
            }

            auto* args = reinterpret_cast<ReconnectCommand_19*>(data.data());
            memset(&this->next_destination, 0, sizeof(this->next_destination));
            struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
                &this->next_destination);
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = args->address.load_raw();
            sin->sin_port = htons(args->port);

            if (!this->client_bev.get()) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Received reconnect command with no destination present",
                  this->license->serial_number);

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
            // If the client has the no-close-confirmation flag set in its
            // newserv client config, send a fake confirmation to the remote
            // server immediately.
            if (this->newserv_client_config.flags & ClientFlag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION) {
              send_command(this->server_bev.get(), this->version,
                  this->server_output_crypt.get(), 0xD6, 0x00, "", 0,
                  name.c_str());
            }
            break;
          }

          case 0x44:
          case 0xA6: {
            if (!this->server->save_files) {
              break;
            }

            bool is_download_quest = (command == 0xA6);

            struct OpenFileCommand {
              char name[0x20];
              uint16_t unused;
              uint16_t flags;
              char filename[0x10];
              uint32_t file_size;
            };
            if (data.size() < sizeof(OpenFileCommand)) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Open file command is too small; skipping file",
                  this->license->serial_number);
              break;
            }
            const auto* cmd = reinterpret_cast<const OpenFileCommand*>(data.data());

            string output_filename = string_printf("%s.%s.%" PRIu64,
                cmd->filename, is_download_quest ? "download" : "online", now());
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
            log(INFO, "[ProxyServer/%08" PRIX32 "] Opened file %s",
                this->license->serial_number, output_filename.c_str());
            break;
          }

          case 0x13:
          case 0xA7: {
            if (!this->server->save_files) {
              break;
            }

            struct WriteFileCommand {
              char filename[0x10];
              uint8_t data[0x400];
              uint32_t data_size;
            };
            if (data.size() < sizeof(WriteFileCommand)) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Write file command is too small",
                  this->license->serial_number);
              break;
            }
            const auto* cmd = reinterpret_cast<const WriteFileCommand*>(data.data());

            SavingFile* sf = nullptr;
            try {
              sf = &this->saving_files.at(cmd->filename);
            } catch (const out_of_range&) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Received data for non-open file %s",
                  this->license->serial_number, cmd->filename);
              break;
            }

            size_t bytes_to_write = cmd->data_size;
            if (bytes_to_write > 0x400) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Chunk data size is invalid; truncating to 0x400",
                  this->license->serial_number);
              bytes_to_write = 0x400;
            }

            log(INFO, "[ProxyServer/%08" PRIX32 "] Writing %zu bytes to %s",
                this->license->serial_number, bytes_to_write,
                sf->output_filename.c_str());
            fwritex(sf->f.get(), cmd->data, bytes_to_write);
            if (bytes_to_write > sf->remaining_bytes) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Chunk size extends beyond original file size; file may be truncated",
                  this->license->serial_number);
              sf->remaining_bytes = 0;
            } else {
              sf->remaining_bytes -= bytes_to_write;
            }

            if (sf->remaining_bytes == 0) {
              log(INFO, "[ProxyServer/%08" PRIX32 "] File %s is complete",
                  this->license->serial_number, sf->output_filename.c_str());
              this->saving_files.erase(cmd->filename);
            }
            break;
          }

          case 0xB8: {
            if (!this->server->save_files) {
              break;
            }
            if (data.size() < 4) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Card list data size is too small; skipping file",
                  this->license->serial_number);
              break;
            }

            StringReader r(data);
            size_t size = r.get_u32l();
            if (r.remaining() < size) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Card list data size extends beyond end of command; skipping file",
                  this->license->serial_number);
              break;
            }

            string output_filename = string_printf("cardupdate.mnr.%" PRIu64, now());
            save_file(output_filename, r.read(size));

            log(INFO, "[ProxyServer/%08" PRIX32 "] Wrote %zu bytes to %s",
                this->license->serial_number, size, output_filename.c_str());
            break;
          }

          case 0x67: // join lobby
            this->lobby_players.clear();
            this->lobby_players.resize(12);
            log(WARNING, "[ProxyServer/%08" PRIX32 "] Cleared lobby players",
                this->license->serial_number);

            // This command can cause the client to no longer send D6 responses
            // when 1A/D5 large message boxes are closed. newserv keeps track of
            // this behavior in the client config, so if it happens during a
            // proxy session, update the client config that we'll restore if the
            // client uses the change ship or change block command.
            if (this->newserv_client_config.flags & ClientFlag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN) {
              this->newserv_client_config.flags |= ClientFlag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION;
            }

            [[fallthrough]];

          case 0x65: // other player joined game
          case 0x68: { // other player joined lobby
            struct Command {
              uint8_t client_id;
              uint8_t leader_id;
              uint8_t disable_udp;
              uint8_t lobby_number;
              uint16_t block_number;
              uint16_t event;
              uint32_t unused;
              struct Entry {
                PlayerLobbyDataGC lobby_data;
                PlayerLobbyJoinDataPCGC data;
              } __attribute__((packed));
              Entry entries[0];
            } __attribute__((packed));

            size_t expected_size = sizeof(Command) + sizeof(Command::Entry) * flag;
            if (data.size() < expected_size) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Lobby join command is incorrect size (expected 0x%zX bytes, received 0x%zX bytes)",
                  this->license->serial_number, expected_size, data.size());
            } else {
              const auto* cmd = reinterpret_cast<const Command*>(data.data());

              this->lobby_client_id = cmd->client_id;

              for (size_t x = 0; x < flag; x++) {
                size_t index = cmd->entries[x].lobby_data.client_id;
                if (index >= this->lobby_players.size()) {
                  log(WARNING, "[ProxyServer/%08" PRIX32 "] Ignoring invalid player index %zu at position %zu",
                      this->license->serial_number, index, x);
                } else {
                  this->lobby_players[index].guild_card_number = cmd->entries[x].lobby_data.guild_card;
                  this->lobby_players[index].name = cmd->entries[x].data.disp.name;
                  log(INFO, "[ProxyServer/%08" PRIX32 "] Added lobby player: (%zu) %" PRIu32 " %s",
                      this->license->serial_number, index,
                      this->lobby_players[index].guild_card_number,
                      this->lobby_players[index].name.c_str());
                }
              }
            }
            break;
          }

          case 0x64: { // join game
            // We don't need to clear lobby_players here because we always
            // overwrite all 4 entries in this case
            this->lobby_players.resize(4);
            log(WARNING, "[ProxyServer/%08" PRIX32 "] Cleared lobby players",
                this->license->serial_number);

            const size_t expected_size = offsetof(JoinGameCommand_GC_64, player);
            const size_t ep3_expected_size = sizeof(JoinGameCommand_GC_64);
            if (data.size() < expected_size) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Game join command is incorrect size (expected 0x%zX bytes, received 0x%zX bytes)",
                  this->license->serial_number, expected_size, data.size());
            } else {
              const auto* cmd = reinterpret_cast<const JoinGameCommand_GC_64*>(data.data());

              this->lobby_client_id = cmd->client_id;

              for (size_t x = 0; x < flag; x++) {
                this->lobby_players[x].guild_card_number = cmd->lobby_data[x].guild_card;
                if (data.size() >= ep3_expected_size) {
                  this->lobby_players[x].name = cmd->player[x].disp.name;
                } else {
                  this->lobby_players[x].name.clear();
                }
                log(INFO, "[ProxyServer/%08" PRIX32 "] Added lobby player: (%zu) %" PRIu32 " %s",
                    this->license->serial_number, x,
                    this->lobby_players[x].guild_card_number,
                    this->lobby_players[x].name.c_str());
              }
            }
            break;
          }

          case 0x66:
          case 0x69: {
            struct Command {
              uint8_t client_id;
              uint8_t leader_id;
              uint16_t unused;
            } __attribute__((packed));
            if (data.size() < sizeof(Command)) {
              log(WARNING, "[ProxyServer/%08" PRIX32 "] Lobby leave command is incorrect size",
                  this->license->serial_number);
            } else {
              const auto* cmd = reinterpret_cast<const Command*>(data.data());
              size_t index = cmd->client_id;
              if (index >= this->lobby_players.size()) {
                log(WARNING, "[ProxyServer/%08" PRIX32 "] Lobby leave command references missing position",
                  this->license->serial_number);
              } else {
                this->lobby_players[index].guild_card_number = 0;
                this->lobby_players[index].name.clear();
                log(INFO, "[ProxyServer/%08" PRIX32 "] Removed lobby player (%zu)",
                    this->license->serial_number, index);
              }
            }
            break;
          }
        }

        if (should_forward) {
          if (!this->client_bev.get()) {
            log(WARNING, "[ProxyServer/%08" PRIX32 "] No client is present; dropping command",
                this->license->serial_number);
          } else {
            // Note: we intentionally don't pass name_str here because we already
            // printed the command above
            send_command(this->client_bev.get(), this->version,
                this->client_output_crypt.get(), command, flag,
                data.data(), data.size());
          }
        }
      });

  } catch (const exception& e) {
    log(ERROR, "[ProxyServer/%08" PRIX32 "] Failed to process server command: %s",
        this->license->serial_number, e.what());
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

  string name = string_printf("ProxySession:%08" PRIX32 ":shell:%s",
      this->license->serial_number, to_server ? "server" : "client");

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
  if (this->serial_number_to_session.empty()) {
    throw runtime_error("no sessions exist");
  }
  if (this->serial_number_to_session.size() > 1) {
    throw runtime_error("multiple sessions exist");
  }
  return this->serial_number_to_session.begin()->second;
}

void ProxyServer::delete_session(uint32_t serial_number) {
  if (this->serial_number_to_session.erase(serial_number)) {
    log(WARNING, "[ProxyServer/%08" PRIX32 "] Closed session", serial_number);
  }
}
