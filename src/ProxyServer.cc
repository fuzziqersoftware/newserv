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
#include <phosg/Hash.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"
#include "ProxyCommands.hh"

using namespace std;
using namespace std::placeholders;



static const uint32_t LICENSED_SESSION_TIMEOUT_USECS = 5 * 60 * 1000000; // 5 minutes
static const uint32_t UNLICENSED_SESSION_TIMEOUT_USECS = 10 * 1000000; // 10 seconds



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
}

ProxyServer::ListeningSocket::ListeningSocket(
    ProxyServer* server,
    uint16_t port,
    GameVersion version,
    const struct sockaddr_storage* default_destination)
  : server(server),
    log(string_printf("[ProxyServer:ListeningSocket:%hu] ", port), proxy_server_log.min_level),
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

  this->log.info("Listening on TCP port %hu (%s) on fd %d",
      this->port, name_for_version(this->version), static_cast<int>(this->fd));
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
  this->log.info("Client connected on fd %d (port %hu, version %s)",
      fd, this->port, name_for_version(this->version));
  auto* bev = bufferevent_socket_new(this->server->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  this->server->on_client_connect(bev, this->port, this->version,
      (this->default_destination.ss_family == AF_INET) ? &this->default_destination : nullptr);
}

void ProxyServer::ListeningSocket::on_listen_error() {
  int err = EVUTIL_SOCKET_ERROR();
  this->log.error("Failure on listening socket %d: %d (%s)",
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
    proxy_server_log.info("Virtual connection received on unregistered port %hu; closing it",
        server_port);
    bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
    bufferevent_free(bev);
    return;
  }

  proxy_server_log.info("Client connected on virtual connection %p (port %hu)", bev,
      server_port);
  this->on_client_connect(bev, server_port, version, nullptr);
}



void ProxyServer::on_client_connect(
    struct bufferevent* bev,
    uint16_t listen_port,
    GameVersion version,
    const struct sockaddr_storage* default_destination) {
  // If a default destination exists for this client and the client is a patch
  // client, create a linked session immediately and connect to the remote
  // server. This creates a direct session.
  if (default_destination && (version == GameVersion::PATCH)) {
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
    session->log.info("Opened linked session");

    Channel ch(bev, version, nullptr, nullptr, session.get(), "", TerminalFormat::FG_YELLOW, TerminalFormat::FG_GREEN);
    session->resume(move(ch));

  // If no default destination exists, or the client is not a patch client,
  // create an unlinked session - we'll have to get the destination from the
  // client's config, which we'll get via a 9E command soon.
  } else {
    auto emplace_ret = this->bev_to_unlinked_session.emplace(bev, new UnlinkedSession(
        this, bev, listen_port, version));
    if (!emplace_ret.second) {
      throw logic_error("stale unlinked session exists");
    }
    auto session = emplace_ret.first->second;
    proxy_server_log.info("Opened unlinked session");

    // Note that this should only be set when the linked session is created, not
    // when it is resumed!
    if (default_destination) {
      session->next_destination = *default_destination;
    }

    switch (version) {
      case GameVersion::PATCH:
        throw logic_error("cannot create unlinked patch session");
      case GameVersion::DC:
      case GameVersion::PC:
      case GameVersion::GC:
      case GameVersion::XB: {
        uint32_t server_key = random_object<uint32_t>();
        uint32_t client_key = random_object<uint32_t>();
        auto cmd = prepare_server_init_contents_console(
            server_key, client_key, 0);
        session->channel.send(0x02, 0x00, &cmd, sizeof(cmd));
        // TODO: Is this actually needed?
        // bufferevent_flush(session->channel.bev.get(), EV_READ | EV_WRITE, BEV_FLUSH);
        if ((version == GameVersion::DC) || (version == GameVersion::PC)) {
          session->channel.crypt_out.reset(new PSOV2Encryption(server_key));
          session->channel.crypt_in.reset(new PSOV2Encryption(client_key));
        } else {
          session->channel.crypt_out.reset(new PSOV3Encryption(server_key));
          session->channel.crypt_in.reset(new PSOV3Encryption(client_key));
        }
        break;
      }
      case GameVersion::BB: {
        parray<uint8_t, 0x30> server_key;
        parray<uint8_t, 0x30> client_key;
        random_data(server_key.data(), server_key.bytes());
        random_data(client_key.data(), client_key.bytes());
        auto cmd = prepare_server_init_contents_bb(server_key, client_key, 0);
        session->channel.send(0x03, 0x00, &cmd, sizeof(cmd));
        session->detector_crypt.reset(new PSOBBMultiKeyDetectorEncryption(
            this->state->bb_private_keys, bb_crypt_initial_client_commands, cmd.client_key.data(), sizeof(cmd.client_key)));
        session->channel.crypt_in = session->detector_crypt;
        session->channel.crypt_out.reset(new PSOBBMultiKeyImitatorEncryption(
            session->detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), true));
        break;
      }
      default:
        throw logic_error("unsupported game version on proxy server");
    }
  }
}



ProxyServer::UnlinkedSession::UnlinkedSession(
    ProxyServer* server,
    struct bufferevent* bev,
    uint16_t local_port,
    GameVersion version)
  : server(server),
    log(string_printf("[ProxyServer:UnlinkedSession:%p] ", bev), proxy_server_log.min_level),
    channel(
      bev,
      version,
      ProxyServer::UnlinkedSession::on_input,
      ProxyServer::UnlinkedSession::on_error,
      this,
      string_printf("UnlinkedSession:%p", bev),
      TerminalFormat::FG_YELLOW,
      TerminalFormat::FG_GREEN),
    local_port(local_port),
    version(version) {
  memset(&this->next_destination, 0, sizeof(this->next_destination));
}

void ProxyServer::UnlinkedSession::on_input(Channel& ch, uint16_t command, uint32_t, std::string& data) {
  auto* session = reinterpret_cast<UnlinkedSession*>(ch.context_obj);

  bool should_close_unlinked_session = false;
  shared_ptr<const License> license;
  uint32_t sub_version = 0;
  uint8_t language = 1; // Default = English
  string character_name;
  ClientConfigBB client_config;
  string login_command_bb;
  string hardware_id;

  try {
    if (session->version == GameVersion::DC) {
      // We should only get a 93 or 9D while the session is unlinked; if we get
      // anything else, disconnect
      if (command == 0x93) {
        const auto& cmd = check_size_t<C_LoginV1_DC_93>(data);
        license = session->server->state->license_manager->verify_pc(
            stoul(cmd.serial_number, nullptr, 16), cmd.access_key);
        sub_version = cmd.sub_version;
        language = cmd.language;
        character_name = cmd.name;
        hardware_id = cmd.hardware_id;
        client_config.cfg.flags |= Client::Flag::IS_DC_V1;
      } else if (command == 0x9D) {
        const auto& cmd = check_size_t<C_Login_DC_PC_GC_9D>(
            data, sizeof(C_Login_DC_PC_GC_9D), sizeof(C_LoginExtended_DC_GC_9D));
        license = session->server->state->license_manager->verify_pc(
            stoul(cmd.serial_number, nullptr, 16), cmd.access_key);
        sub_version = cmd.sub_version;
        language = cmd.language;
        character_name = cmd.name;
      } else {
        throw runtime_error("command is not 93 or 9D");
      }

    } else if (session->version == GameVersion::PC) {
      // We should only get a 9D while the session is unlinked; if we get
      // anything else, disconnect
      if (command != 0x9D) {
        throw runtime_error("command is not 9D");
      }
      const auto& cmd = check_size_t<C_Login_DC_PC_GC_9D>(
          data, sizeof(C_Login_DC_PC_GC_9D), sizeof(C_LoginExtended_PC_9D));
      license = session->server->state->license_manager->verify_pc(
          stoul(cmd.serial_number, nullptr, 16), cmd.access_key);
      sub_version = cmd.sub_version;
      language = cmd.language;
      character_name = cmd.name;

    } else if (session->version == GameVersion::GC) {
      // We should only get a 9E while the session is unlinked; if we get
      // anything else, disconnect
      // TODO: GCTE will send 9D; we should presumably handle that too, sigh
      if (command != 0x9E) {
        throw runtime_error("command is not 9E");
      }
      const auto& cmd = check_size_t<C_Login_GC_9E>(
          data, sizeof(C_Login_GC_9E), sizeof(C_LoginExtended_GC_9E));
      license = session->server->state->license_manager->verify_gc(
          stoul(cmd.serial_number, nullptr, 16), cmd.access_key);
      sub_version = cmd.sub_version;
      language = cmd.language;
      character_name = cmd.name;
      client_config.cfg = cmd.client_config.cfg;

    } else if (session->version == GameVersion::XB) {
      throw runtime_error("xbox licenses are not implemented");

    } else if (session->version == GameVersion::BB) {
      // We should only get a 93 while the session is unlinked; if we get
      // anything else, disconnect
      if (command != 0x93) {
        throw runtime_error("command is not 93");
      }
      const auto& cmd = check_size_t<C_Login_BB_93>(data);
      try {
        license = session->server->state->license_manager->verify_bb(
            cmd.username, cmd.password);
      } catch (const missing_license&) {
        if (!session->server->state->allow_unregistered_users) {
          throw;
        }
        shared_ptr<License> l = LicenseManager::create_license_bb(
            fnv1a32(cmd.username) & 0x7FFFFFFF, cmd.username, cmd.password, true);
        session->server->state->license_manager->add(l);
        license = l;
      }
      login_command_bb = move(data);

    } else {
      throw logic_error("unsupported unlinked session version");
    }

  } catch (const exception& e) {
    session->log.error("Failed to process command from unlinked client: %s", e.what());
    should_close_unlinked_session = true;
  }

  struct bufferevent* session_key = ch.bev.get();

  // If license is non-null, then the client has a password and can be connected
  // to the remote lobby server.
  if (license.get()) {
    // At this point, we will always close the unlinked session, even if it
    // doesn't get converted/merged to a linked session
    should_close_unlinked_session = true;

    // Look up the linked session for this license (if any)
    shared_ptr<LinkedSession> linked_session;
    try {
      linked_session = session->server->id_to_session.at(license->serial_number);
      linked_session->log.info("Resuming linked session from unlinked session");

    } catch (const out_of_range&) {
      // If there's no open session for this license, then there must be a valid
      // destination somewhere - either in the client config or in the unlinked
      // session
      if (client_config.cfg.magic == CLIENT_CONFIG_MAGIC) {
        linked_session.reset(new LinkedSession(
            session->server,
            session->local_port,
            session->version,
            license,
            client_config));
        linked_session->log.info("Opened licensed session for unlinked session based on client config");
      } else if (session->next_destination.ss_family == AF_INET) {
        linked_session.reset(new LinkedSession(
            session->server,
            session->local_port,
            session->version,
            license,
            session->next_destination));
        linked_session->log.info("Opened licensed session for unlinked session based on unlinked default destination");
      } else {
        session->log.error("Cannot open linked session: no valid destination in client config or unlinked session");
      }
    }

    if (linked_session.get()) {
      session->server->id_to_session.emplace(license->serial_number, linked_session);
      if (linked_session->version != session->version) {
        linked_session->log.error("Linked session has different game version");
      } else {
        // Resume the linked session using the unlinked session
        try {
          if (session->version == GameVersion::BB) {
            linked_session->resume(
                move(session->channel),
                session->detector_crypt,
                move(login_command_bb));
          } else {
            linked_session->resume(
                move(session->channel),
                session->detector_crypt,
                sub_version,
                language,
                character_name,
                hardware_id);
          }
        } catch (const exception& e) {
          linked_session->log.error("Failed to resume linked session: %s", e.what());
        }
      }
    }
  }

  if (should_close_unlinked_session) {
    session->log.info("Closing session");
    session->server->bev_to_unlinked_session.erase(session_key);
    // At this point, (*this) is destroyed! We must be careful not to touch it.
  }
}

void ProxyServer::UnlinkedSession::on_error(Channel& ch, short events) {
  auto* session = reinterpret_cast<UnlinkedSession*>(ch.context_obj);

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    session->log.warning("Error %d (%s) in unlinked client stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    session->log.info("Unlinked client has disconnected");
    session->server->bev_to_unlinked_session.erase(session->channel.bev.get());
  }
}



ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint64_t id,
    uint16_t local_port,
    GameVersion version)
  : server(server),
    id(id),
    log(string_printf("[ProxyServer:LinkedSession:%08" PRIX64 "] ", this->id), proxy_server_log.min_level),
    timeout_event(event_new(this->server->base.get(), -1, EV_TIMEOUT,
        &LinkedSession::dispatch_on_timeout, this), event_free),
    license(nullptr),
    client_channel(
      version,
      nullptr,
      nullptr,
      this,
      string_printf("LinkedSession:%08" PRIX64 ":client", this->id),
      TerminalFormat::FG_YELLOW,
      TerminalFormat::FG_GREEN),
    server_channel(
      version,
      nullptr,
      nullptr,
      this,
      string_printf("LinkedSession:%08" PRIX64 ":server", this->id),
      TerminalFormat::FG_YELLOW,
      TerminalFormat::FG_RED),
    local_port(local_port),
    remote_ip_crc(0),
    enable_remote_ip_crc_patch(false),
    version(version),
    sub_version(0), // This is set during resume()
    language(1), // Default = English. This is also set during resume()
    remote_guild_card_number(-1),
    enable_chat_filter(true),
    switch_assist(false),
    infinite_hp(false),
    infinite_tp(false),
    save_files(false),
    suppress_remote_login(false),
    function_call_return_value(-1),
    next_item_id(0x0F000000),
    override_section_id(-1),
    override_lobby_event(-1),
    override_lobby_number(-1),
    lobby_players(12),
    lobby_client_id(0),
    leader_client_id(0),
    is_in_game(false) {
  this->last_switch_enabled_command.subcommand = 0;
  memset(this->prev_server_command_bytes, 0, sizeof(this->prev_server_command_bytes));
}

ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint16_t local_port,
    GameVersion version,
    shared_ptr<const License> license,
    const ClientConfigBB& newserv_client_config)
  : LinkedSession(server, license->serial_number, local_port, version) {
  this->license = license;
  this->newserv_client_config = newserv_client_config;
  memset(&this->next_destination, 0, sizeof(this->next_destination));
  struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&this->next_destination);
  dest_sin->sin_family = AF_INET;
  dest_sin->sin_port = htons(this->newserv_client_config.cfg.proxy_destination_port);
  dest_sin->sin_addr.s_addr = htonl(this->newserv_client_config.cfg.proxy_destination_address);
}

ProxyServer::LinkedSession::LinkedSession(
    ProxyServer* server,
    uint16_t local_port,
    GameVersion version,
    std::shared_ptr<const License> license,
    const struct sockaddr_storage& next_destination)
  : LinkedSession(server, license->serial_number, local_port, version) {
  this->license = license;
  this->next_destination = next_destination;
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
    Channel&& client_channel,
    shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt,
    uint32_t sub_version,
    uint8_t language,
    const string& character_name,
    const string& hardware_id) {
  this->sub_version = sub_version;
  this->language = language;
  this->character_name = character_name;
  this->hardware_id = hardware_id;
  this->resume_inner(move(client_channel), detector_crypt);
}

void ProxyServer::LinkedSession::resume(
    Channel&& client_channel,
    shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt,
    string&& login_command_bb) {
  this->login_command_bb = move(login_command_bb);
  this->resume_inner(move(client_channel), detector_crypt);
}

void ProxyServer::LinkedSession::resume(Channel&& client_channel) {
  this->sub_version = 0;
  this->language = 1;
  this->character_name.clear();
  this->resume_inner(move(client_channel), nullptr);
}

void ProxyServer::LinkedSession::resume_inner(
    Channel&& client_channel,
    shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt) {
  if (this->client_channel.connected()) {
    throw runtime_error("client connection is already open for this session");
  }
  if (this->next_destination.ss_family != AF_INET) {
    throw logic_error("attempted to resume an unlicensed linked session without destination set");
  }

  this->client_channel.replace_with(
      move(client_channel),
      ProxyServer::LinkedSession::on_input,
      ProxyServer::LinkedSession::on_error,
      this,
      string_printf("LinkedSession:%08" PRIX64 ":client", this->id));

  this->detector_crypt = detector_crypt;
  this->server_channel.disconnect();
  this->saving_files.clear();

  this->connect();
}

void ProxyServer::LinkedSession::connect() {
  // Connect to the remote server. The command handlers will do the login steps
  // and set up forwarding
  const struct sockaddr_in* dest_sin = reinterpret_cast<const sockaddr_in*>(
      &this->next_destination);
  if (dest_sin->sin_family != AF_INET) {
    throw runtime_error("destination is not AF_INET");
  }

  string netloc_str = render_sockaddr_storage(this->next_destination);
  this->log.info("Connecting to %s", netloc_str.c_str());

  this->server_channel.set_bufferevent(bufferevent_socket_new(
      this->server->base.get(), -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));
  if (bufferevent_socket_connect(this->server_channel.bev.get(),
      reinterpret_cast<const sockaddr*>(dest_sin), sizeof(*dest_sin)) != 0) {
    throw runtime_error(string_printf("failed to connect (%d)", EVUTIL_SOCKET_ERROR()));
  }

  this->server_channel.on_command_received = ProxyServer::LinkedSession::on_input;
  this->server_channel.on_error = ProxyServer::LinkedSession::on_error;
  this->server_channel.context_obj = this;

  // Cancel the session delete timeout
  event_del(this->timeout_event.get());
}



ProxyServer::LinkedSession::SavingFile::SavingFile(
    const string& basename,
    const string& output_filename,
    uint32_t remaining_bytes)
  : basename(basename),
    output_filename(output_filename),
    remaining_bytes(remaining_bytes),
    f(fopen_unique(this->output_filename, "wb")) { }



void ProxyServer::LinkedSession::dispatch_on_timeout(
    evutil_socket_t, short, void* ctx) {
  reinterpret_cast<LinkedSession*>(ctx)->on_timeout();
}



void ProxyServer::LinkedSession::on_timeout() {
  this->log.info("Session timed out");
  this->server->delete_session(this->id);
}



void ProxyServer::LinkedSession::on_error(Channel& ch, short events) {
  auto* session = reinterpret_cast<LinkedSession*>(ch.context_obj);
  bool is_server_stream = (&ch == &session->server_channel);

  if (events & BEV_EVENT_CONNECTED) {
    session->log.info("%s channel connected", is_server_stream ? "Server" : "Client");
  }
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    session->log.warning("Error %d (%s) in %s stream",
        err, evutil_socket_error_to_string(err),
        is_server_stream ? "server" : "client");
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    session->log.info("%s has disconnected",
        is_server_stream ? "Server" : "Client");
    // If the server disconnected, send the client back to the game server so
    // they're not disconnected completely.
    if (is_server_stream) {
      session->send_to_game_server("The server has\ndisconnected.");
    }
    session->disconnect();
  }
}

void ProxyServer::LinkedSession::clear_lobby_players(size_t num_slots) {
  this->lobby_players.clear();
  this->lobby_players.resize(num_slots);
  this->log.info("Cleared lobby players");
}

void ProxyServer::LinkedSession::send_to_game_server(const char* error_message) {
  // Delete all the other players
  for (size_t x = 0; x < this->lobby_players.size(); x++) {
    if (this->lobby_players[x].guild_card_number == 0) {
      continue;
    }
    uint8_t leaving_id = x;
    uint8_t leader_id = this->lobby_client_id;
    S_LeaveLobby_66_69_Ep3_E9 cmd = {leaving_id, leader_id, 1, 0};
    this->client_channel.send(this->is_in_game ? 0x66 : 0x69, leaving_id, &cmd, sizeof(cmd));
  }

  string encoded_name = encode_sjis(this->server->state->name);
  if (this->is_in_game) {
    send_ship_info(this->client_channel, decode_sjis(string_printf(
        "You cannot return\nto $C6%s$C7\nwhile in a game.\n\n%s",
        encoded_name.c_str(), error_message ? error_message : "")));
    this->disconnect();

  } else {
    send_ship_info(this->client_channel, decode_sjis(string_printf(
        "You\'ve returned to\n\tC6%s$C7\n\n%s", encoded_name.c_str(),
        error_message ? error_message : "")));

    // Restore newserv_client_config, so the login server gets the client flags
    S_UpdateClientConfig_DC_PC_V3_04 update_client_config_cmd;
    update_client_config_cmd.player_tag = 0x00010000;
    update_client_config_cmd.guild_card_number = this->license->serial_number;
    update_client_config_cmd.cfg = this->newserv_client_config.cfg;
    this->client_channel.send(0x04, 0x00, &update_client_config_cmd, sizeof(update_client_config_cmd));

    static const vector<string> version_to_port_name({
        "bb-patch", "console-login", "pc-login", "console-login", "console-login", "bb-init"});
    const auto& port_name = version_to_port_name.at(static_cast<size_t>(
        this->version));

    S_Reconnect_19 reconnect_cmd = {{
        0, this->server->state->name_to_port_config.at(port_name)->port, 0}};

    // If the client is on a virtual connection, we can use any address
    // here and they should be able to connect back to the game server. If
    // the client is on a real connection, we'll use the sockname of the
    // existing connection (like we do in the server 19 command handler).
    if (this->client_channel.is_virtual_connection) {
      struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&this->next_destination);
      if (dest_sin->sin_family != AF_INET) {
        throw logic_error("ss not AF_INET");
      }
      reconnect_cmd.address.store_raw(dest_sin->sin_addr.s_addr);
    } else {
      const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(
          &this->client_channel.local_addr);
      if (sin->sin_family != AF_INET) {
        throw logic_error("existing connection is not ipv4");
      }
      reconnect_cmd.address.store_raw(sin->sin_addr.s_addr);
    }

    this->client_channel.send(0x19, 0x00, &reconnect_cmd, sizeof(reconnect_cmd));
  }
}

void ProxyServer::LinkedSession::disconnect() {
  // Forward the disconnection to the other end
  this->client_channel.disconnect();
  this->server_channel.disconnect();

  // Set a timeout to delete the session entirely (in case the client doesn't
  // reconnect)
  struct timeval tv = usecs_to_timeval(this->license.get()
      ? LICENSED_SESSION_TIMEOUT_USECS : UNLICENSED_SESSION_TIMEOUT_USECS);
  event_add(this->timeout_event.get(), &tv);
}

bool ProxyServer::LinkedSession::is_connected() const {
  return (this->server_channel.connected() && this->client_channel.connected());
}



void ProxyServer::LinkedSession::on_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data) {
  auto* session = reinterpret_cast<LinkedSession*>(ch.context_obj);
  bool is_server_stream = (&ch == &session->server_channel);

  try {
    if (is_server_stream) {
      size_t bytes_to_save = min<size_t>(data.size(), sizeof(session->prev_server_command_bytes));
      memcpy(session->prev_server_command_bytes, data.data(), bytes_to_save);
    }
    on_proxy_command(
        session->server->state,
        *session,
        is_server_stream,
        command,
        flag,
        data);
  } catch (const exception& e) {
    session->log.error("Failed to process command from %s: %s",
        is_server_stream ? "server" : "client", e.what());
    session->disconnect();
  }
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

shared_ptr<ProxyServer::LinkedSession> ProxyServer::create_licensed_session(
    shared_ptr<const License> l, uint16_t local_port, GameVersion version,
    const ClientConfigBB& newserv_client_config) {
  shared_ptr<LinkedSession> session(new LinkedSession(
      this, local_port, version, l, newserv_client_config));
  auto emplace_ret = this->id_to_session.emplace(session->id, session);
  if (!emplace_ret.second) {
    throw runtime_error("session already exists for this license");
  }
  session->log.info("Opening licensed session");
  return emplace_ret.first->second;
}

void ProxyServer::delete_session(uint64_t id) {
  if (this->id_to_session.erase(id)) {
    proxy_server_log.info("Closed LinkedSession:%08" PRIX64, id);
  }
}

size_t ProxyServer::delete_disconnected_sessions() {
  size_t count = 0;
  for (auto it = this->id_to_session.begin(); it != this->id_to_session.end();) {
    if (!it->second->is_connected()) {
      it = this->id_to_session.erase(it);
      count++;
    } else {
      it++;
    }
  }
  return count;
}
