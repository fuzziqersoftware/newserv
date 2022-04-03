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
#include "ProxyCommands.hh"

using namespace std;



static const uint32_t SESSION_TIMEOUT_USECS = 30000000; // 30 seconds



static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}



ProxyServer::ProxyServer(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
  : base(base),
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
    struct evconnlistener*, evutil_socket_t fd, struct sockaddr*, int, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_accept(fd);
}

void ProxyServer::ListeningSocket::dispatch_on_listen_error(
    struct evconnlistener*, void* ctx) {
  reinterpret_cast<ListeningSocket*>(ctx)->on_listen_error();
}

void ProxyServer::ListeningSocket::on_listen_accept(int fd) {
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
    session->log(INFO, "Opened linked session");
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
    log(INFO, "[ProxyServer] Opened unlinked session");

    switch (version) {
      case GameVersion::PATCH:
        throw logic_error("cannot create unlinked patch session");
      case GameVersion::PC:
      case GameVersion::GC: {
        uint32_t server_key = random_object<uint32_t>();
        uint32_t client_key = random_object<uint32_t>();
        auto cmd = prepare_server_init_contents_dc_pc_gc(
            false, server_key, client_key);
        send_command(
            session->bev.get(),
            session->version,
            session->crypt_out.get(),
            0x02,
            0,
            &cmd,
            sizeof(cmd),
            "unlinked proxy client");
        bufferevent_flush(session->bev.get(), EV_READ | EV_WRITE, BEV_FLUSH);
        if (version == GameVersion::PC) {
          session->crypt_out.reset(new PSOPCEncryption(server_key));
          session->crypt_in.reset(new PSOPCEncryption(client_key));
        } else {
          session->crypt_out.reset(new PSOGCEncryption(server_key));
          session->crypt_in.reset(new PSOGCEncryption(client_key));
        }
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

  try {
    for_each_received_command(this->bev.get(), this->version, this->crypt_in.get(),
      [&](uint16_t command, uint32_t flag, const string& data) {
        print_received_command(command, flag, data.data(), data.size(),
            this->version, "unlinked proxy client");

        if (this->version == GameVersion::PC) {
          // We should only get a 9D while the session is unlinked; if we get
          // anything else, disconnect
          if (command != 0x9D) {
            throw runtime_error("command is not 9D");
          }
          const auto& cmd = check_size_t<C_Login_PC_9D>(data, sizeof(C_Login_PC_9D), sizeof(C_LoginWithUnusedSpace_PC_9D));
          uint32_t serial_number = strtoul(cmd.serial_number.c_str(), nullptr, 16);
          license = this->server->state->license_manager->verify_pc(
              serial_number, cmd.access_key.c_str(), nullptr);
          sub_version = cmd.sub_version;
          character_name = cmd.name;

        } else if (this->version == GameVersion::GC) {
          // We should only get a 9E while the session is unlinked; if we get
          // anything else, disconnect
          if (command != 0x9E) {
            throw runtime_error("command is not 9E");
          }
          const auto& cmd = check_size_t<C_Login_GC_9E>(data, sizeof(C_Login_GC_9E), sizeof(C_LoginWithUnusedSpace_GC_9E));
          uint32_t serial_number = strtoul(cmd.serial_number.c_str(), nullptr, 16);
          license = this->server->state->license_manager->verify_gc(
              serial_number, cmd.access_key.c_str(), nullptr);
          sub_version = cmd.sub_version;
          character_name = cmd.name;
          client_config = cmd.client_config.cfg;

        } else {
          throw logic_error("unsupported unlinked session version");
        }
      });

  } catch (const exception& e) {
    log(ERROR, "[ProxyServer] Failed to process command from unlinked client: %s", e.what());
    should_close_unlinked_session = true;
  }

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
      session->log(INFO, "Resuming linked session from unlinked session");

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
        session->log(INFO, "Opened licensed session for unlinked session");
      }
    }

    if (session.get() && (session->version != this->version)) {
      session->log(ERROR, "Linked session has different game version");
    } else {
      // Resume the linked session using the unlinked session
      try {
        session->resume(move(this->bev), this->crypt_in, this->crypt_out, sub_version, character_name);
        this->crypt_in.reset();
        this->crypt_out.reset();
      } catch (const exception& e) {
        session->log(ERROR, "Failed to resume linked session: %s", e.what());
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
    client_name(string_printf("LinkedSession:%08" PRIX64 ":client", this->id)),
    server_name(string_printf("LinkedSession:%08" PRIX64 ":server", this->id)),
    log(string_printf("[LinkedSession:%08" PRIX64 "] ", this->id)),
    timeout_event(event_new(this->server->base.get(), -1, EV_TIMEOUT,
        &LinkedSession::dispatch_on_timeout, this), event_free),
    license(nullptr),
    client_bev(nullptr, flush_and_free_bufferevent),
    server_bev(nullptr, flush_and_free_bufferevent),
    local_port(local_port),
    version(version),
    sub_version(0), // This is set during resume()
    remote_guild_card_number(0),
    suppress_newserv_commands(true),
    enable_chat_filter(true),
    enable_switch_assist(false),
    save_files(false),
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
  this->license = license;
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
  this->log(INFO, "Connecting to %s", netloc_str.c_str());
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
  this->log(INFO, "Session timed out");
  this->server->delete_session(this->id);
}



void ProxyServer::LinkedSession::on_stream_error(
    short events, bool is_server_stream) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    this->log(WARNING, "Error %d (%s) in %s stream",
        err, evutil_socket_error_to_string(err),
        is_server_stream ? "server" : "client");
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    this->log(INFO, "%s has disconnected",
        is_server_stream ? "Server" : "Client");
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



void ProxyServer::LinkedSession::on_client_input() {
  for_each_received_command(this->client_bev.get(), this->version, this->client_input_crypt.get(),
    [&](uint16_t command, uint32_t flag, string& data) {
      print_received_command(command, flag, data.data(), data.size(),
          this->version, this->client_name.c_str());
      process_proxy_command(
          this->server->state,
          *this,
          false, // from_server
          command,
          flag,
          data);
    });
}

void ProxyServer::LinkedSession::on_server_input() {
  for_each_received_command(this->server_bev.get(), this->version, this->server_input_crypt.get(),
    [&](uint16_t command, uint32_t flag, string& data) {
      print_received_command(command, flag, data.data(), data.size(),
          this->version, this->server_name.c_str());
      process_proxy_command(
          this->server->state,
          *this,
          true, // from_server
          command,
          flag,
          data);
    });
}

void ProxyServer::LinkedSession::send_to_end(
    bool to_server,
    uint16_t command,
    uint32_t flag,
    const void* data,
    size_t size) {
  if (size & 3) {
    throw runtime_error("command size is not a multiple of 4");
  }
  string name = string_printf("LinkedSession:%08" PRIX64 ":synthetic:%s",
      this->id, to_server ? "server" : "client");

  send_command(
      to_server ? this->server_bev.get() : this->client_bev.get(),
      this->version,
      to_server ? this->server_output_crypt.get() : this->client_output_crypt.get(),
      command,
      flag,
      data,
      size,
      name.c_str());
}

void ProxyServer::LinkedSession::send_to_end(
    bool to_server, uint16_t command, uint32_t flag, const string& data) {
  this->send_to_end(to_server, command, flag, data.data(), data.size());
}

void ProxyServer::LinkedSession::send_to_end_with_header(
    bool to_server, const void* data, size_t size) {
  size_t header_size = PSOCommandHeader::header_size(this->version);
  if (size < header_size) {
    throw runtime_error("command is too small for header");
  }
  const auto* header = reinterpret_cast<const PSOCommandHeader*>(data);
  this->send_to_end(
      to_server,
      header->command(this->version),
      header->flag(this->version),
      reinterpret_cast<const uint8_t*>(data) + header_size,
      size - header_size);
}

void ProxyServer::LinkedSession::send_to_end_with_header(
    bool to_server, const string& data) {
  this->send_to_end_with_header(to_server, data.data(), data.size());
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

std::shared_ptr<ProxyServer::LinkedSession> ProxyServer::create_licensed_session(
    std::shared_ptr<const License> l, uint16_t local_port, GameVersion version,
    const ClientConfig& newserv_client_config) {
  shared_ptr<LinkedSession> session(new LinkedSession(
      this, local_port, version, l, newserv_client_config));
  auto emplace_ret = this->id_to_session.emplace(session->id, session);
  if (!emplace_ret.second) {
    throw runtime_error("session already exists for this license");
  }
  session->log(INFO, "Opening licensed session");
  return emplace_ret.first->second;
}


void ProxyServer::delete_session(uint64_t id) {
  if (this->id_to_session.erase(id)) {
    log(INFO, "Closed LinkedSession:%08" PRIX64, id);
  }
}
