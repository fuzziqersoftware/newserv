#include "PatchServer.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "EventUtils.hh"
#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;

static atomic<uint64_t> next_id(1);

PatchServer::Client::Client(
    shared_ptr<PatchServer> server,
    struct bufferevent* bev,
    Version version,
    uint64_t idle_timeout_usecs,
    bool hide_data_from_logs)
    : server(server),
      id(next_id++),
      log(phosg::string_printf("[C-%" PRIX64 "] ", this->id), client_log.min_level),
      channel(bev, 0, version, 1, nullptr, nullptr, this, phosg::string_printf("C-%" PRIX64, this->id), phosg::TerminalFormat::FG_YELLOW, phosg::TerminalFormat::FG_GREEN),
      idle_timeout_usecs(idle_timeout_usecs),
      idle_timeout_event(
          event_new(bufferevent_get_base(bev), -1, EV_TIMEOUT, &PatchServer::Client::dispatch_idle_timeout, this),
          event_free) {
  this->reschedule_timeout_event();

  // Don't print data sent to patch clients to the logs. The patch server
  // protocol is fully understood and data logs for patch clients are generally
  // more annoying than helpful at this point.
  if (hide_data_from_logs) {
    this->channel.terminal_recv_color = phosg::TerminalFormat::END;
    this->channel.terminal_send_color = phosg::TerminalFormat::END;
  }

  this->log.info("Created");
}

void PatchServer::Client::reschedule_timeout_event() {
  struct timeval idle_tv = phosg::usecs_to_timeval(this->idle_timeout_usecs);
  event_add(this->idle_timeout_event.get(), &idle_tv);
}

void PatchServer::Client::dispatch_idle_timeout(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Client*>(ctx)->idle_timeout();
}

void PatchServer::Client::idle_timeout() {
  this->log.info("Idle timeout expired");
  auto s = this->server.lock();
  if (s) {
    auto c = this->shared_from_this();
    s->disconnect_client(c);
  } else {
    this->channel.disconnect();
    this->log.info("Server is deleted; cannot disconnect client");
  }
}

void PatchServer::send_server_init(shared_ptr<Client> c) const {
  uint32_t server_key = phosg::random_object<uint32_t>();
  uint32_t client_key = phosg::random_object<uint32_t>();

  S_ServerInit_Patch_02 cmd;
  cmd.copyright.encode("Patch Server. Copyright SonicTeam, LTD. 2001");
  cmd.server_key = server_key;
  cmd.client_key = client_key;
  c->channel.send(0x02, 0x00, cmd);

  c->channel.crypt_out = make_shared<PSOV2Encryption>(server_key);
  c->channel.crypt_in = make_shared<PSOV2Encryption>(client_key);
}

void PatchServer::send_message_box(shared_ptr<Client> c, const string& text) const {
  phosg::StringWriter w;
  try {
    if (c->version() == Version::PC_PATCH) {
      w.write(tt_encode_marked_optional(text, c->channel.language, true));
    } else if (c->version() == Version::BB_PATCH) {
      w.write(tt_encode_marked_optional(add_color(text), c->channel.language, true));
    } else {
      throw logic_error("non-patch client on patch server");
    }
  } catch (const runtime_error& e) {
    phosg::log_warning("Failed to encode message for patch message box command: %s", e.what());
    return;
  }
  w.put_u16(0);
  while (w.str().size() & 3) {
    w.put_u8(0);
  }
  c->channel.send(0x13, 0x00, w.str());
}

void PatchServer::send_enter_directory(shared_ptr<Client> c, const string& dir) const {
  S_EnterDirectory_Patch_09 cmd = {{dir, 1}};
  c->channel.send(0x09, 0x00, cmd);
}

void PatchServer::on_02(shared_ptr<Client> c, string& data) {
  check_size_v(data.size(), 0);
  c->channel.send(0x04, 0x00); // This requests the user's login information
}

void PatchServer::change_to_directory(
    shared_ptr<Client> c,
    vector<string>& client_path_directories,
    const vector<string>& file_path_directories) const {
  // First, exit all leaf directories that don't match the desired path
  while (!client_path_directories.empty() &&
      ((client_path_directories.size() > file_path_directories.size()) ||
          (client_path_directories.back() != file_path_directories[client_path_directories.size() - 1]))) {
    c->channel.send(0x0A, 0x00);
    client_path_directories.pop_back();
  }

  // At this point, client_path_directories should be a prefix of
  // file_path_directories (or should match exactly)
  if (client_path_directories.size() > file_path_directories.size()) {
    throw logic_error("did not exit all necessary directories");
  }
  for (size_t x = 0; x < client_path_directories.size(); x++) {
    if (client_path_directories[x] != file_path_directories[x]) {
      throw logic_error("intermediate path is not a prefix of final path");
    }
  }

  // Second, enter all necessary leaf directories
  while (client_path_directories.size() < file_path_directories.size()) {
    const string& dir = file_path_directories[client_path_directories.size()];
    this->send_enter_directory(c, dir);
    client_path_directories.emplace_back(dir);
  }
}

void PatchServer::on_04(shared_ptr<Client> c, string& data) {
  const auto& cmd = check_size_t<C_Login_Patch_04>(data);

  string username = cmd.username.decode();
  string password = cmd.password.decode();

  // There are 3 cases here:
  // - No login information at all: just proceed without checking credentials
  // - Username: check that account exists if allow_unregistered_users is off
  // - Username and password: call verify_bb
  if (!username.empty() && !password.empty()) {
    try {
      this->config->account_index->from_bb_credentials(username, &password, false);

    } catch (const AccountIndex::incorrect_password& e) {
      c->channel.send(0x15, 0x03);
      this->disconnect_client(c);
      return;

    } catch (const AccountIndex::missing_account& e) {
      if (!this->config->allow_unregistered_users) {
        c->channel.send(0x15, 0x08);
        this->disconnect_client(c);
        return;
      }
    }

  } else if (!username.empty() && !this->config->allow_unregistered_users) {
    try {
      this->config->account_index->from_bb_credentials(username, nullptr, false);
    } catch (const AccountIndex::missing_account& e) {
      c->channel.send(0x15, 0x08);
      this->disconnect_client(c);
      return;
    }
  }

  if (!this->config->message.empty()) {
    this->send_message_box(c, this->config->message.c_str());
  }

  const auto& index = this->config->patch_file_index;
  if (index.get()) {
    c->channel.send(0x0B, 0x00); // Start patch session; go to root directory

    vector<string> path_directories;
    for (const auto& file : index->all_files()) {
      this->change_to_directory(c, path_directories, file->path_directories);

      S_FileChecksumRequest_Patch_0C req = {c->patch_file_checksum_requests.size(), {file->name, 1}};
      c->channel.send(0x0C, 0x00, req);
      c->patch_file_checksum_requests.emplace_back(file);
    }
    this->change_to_directory(c, path_directories, {});

    c->channel.send(0x0D, 0x00); // End of checksum requests

  } else {
    // No patch index present: just do something that will satisfy the client
    // without actually checking or downloading any files
    this->send_enter_directory(c, ".");
    this->send_enter_directory(c, "data");
    this->send_enter_directory(c, "scene");
    c->channel.send(0x0A, 0x00);
    c->channel.send(0x0A, 0x00);
    c->channel.send(0x0A, 0x00);
    c->channel.send(0x12, 0x00);
  }
}

void PatchServer::on_0F(shared_ptr<Client> c, string& data) {
  auto& cmd = check_size_t<C_FileInformation_Patch_0F>(data);
  auto& req = c->patch_file_checksum_requests.at(cmd.request_id);
  req.crc32 = cmd.checksum;
  req.size = cmd.size;
  req.response_received = true;
}

void PatchServer::on_10(shared_ptr<Client> c, string&) {
  S_StartFileDownloads_Patch_11 start_cmd = {0, 0};
  for (const auto& req : c->patch_file_checksum_requests) {
    if (!req.response_received) {
      throw runtime_error("client did not respond to checksum request");
    }
    if (req.needs_update()) {
      c->log.info("File %s needs update (CRC: %08" PRIX32 "/%08" PRIX32 ", size: %" PRIu32 "/%" PRIu32 ")",
          req.file->name.c_str(), req.file->crc32, req.crc32, req.file->size, req.size);
      start_cmd.total_bytes += req.file->size;
      start_cmd.num_files++;
    } else {
      c->log.info("File %s is up to date", req.file->name.c_str());
    }
  }

  if (start_cmd.num_files) {
    c->channel.send(0x11, 0x00, start_cmd);
    vector<string> path_directories;
    for (const auto& req : c->patch_file_checksum_requests) {
      if (req.needs_update()) {
        this->change_to_directory(c, path_directories, req.file->path_directories);

        S_OpenFile_Patch_06 open_cmd = {0, req.file->size, {req.file->name, 1}};
        c->channel.send(0x06, 0x00, open_cmd);

        for (size_t x = 0; x < req.file->chunk_crcs.size(); x++) {
          auto data = req.file->load_data();
          size_t chunk_size = min<uint32_t>(req.file->size - (x * 0x4000), 0x4000);

          vector<pair<const void*, size_t>> blocks;
          S_WriteFileHeader_Patch_07 cmd_header = {x, req.file->chunk_crcs[x], chunk_size};
          blocks.emplace_back(&cmd_header, sizeof(cmd_header));
          blocks.emplace_back(data->data() + (x * 0x4000), chunk_size);
          c->channel.send(0x07, 0x00, blocks);
        }

        S_CloseCurrentFile_Patch_08 close_cmd = {0};
        c->channel.send(0x08, 0x00, close_cmd);
      }
    }
    this->change_to_directory(c, path_directories, {});
  }

  c->channel.send(0x12, 0x00);
}

void PatchServer::disconnect_client(shared_ptr<Client> c) {
  if (c->channel.virtual_network_id) {
    server_log.info("Client disconnected: C-%" PRIX64 " on N-%" PRIu64, c->id, c->channel.virtual_network_id);
  } else if (c->channel.bev) {
    server_log.info("Client disconnected: C-%" PRIX64, c->id);
  } else {
    server_log.info("Client C-%" PRIX64 " removed from patch server", c->id);
  }

  this->channel_to_client.erase(&c->channel);
  c->channel.disconnect();

  // We can't just let c be destroyed here, since disconnect_client can be
  // called from within the client's channel's receive handler. So, we instead
  // move it to another set, which we'll clear in an immediately-enqueued
  // callback after the current event. This will also call the client's
  // disconnect hooks (if any).
  this->clients_to_destroy.insert(std::move(c));
  this->enqueue_destroy_clients();
}

void PatchServer::enqueue_destroy_clients() {
  auto tv = phosg::usecs_to_timeval(0);
  event_add(this->destroy_clients_ev.get(), &tv);
}

void PatchServer::dispatch_destroy_clients(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<PatchServer*>(ctx)->clients_to_destroy.clear();
}

void PatchServer::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<PatchServer*>(ctx)->on_listen_accept(listener, fd, address, socklen);
}

void PatchServer::dispatch_on_listen_error(
    struct evconnlistener* listener, void* ctx) {
  reinterpret_cast<PatchServer*>(ctx)->on_listen_error(listener);
}

void PatchServer::on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr*, int) {
  struct sockaddr_storage remote_addr;
  phosg::get_socket_addresses(fd, nullptr, &remote_addr);
  if (this->config->banned_ipv4_ranges->check(remote_addr)) {
    close(fd);
    return;
  }

  int listen_fd = evconnlistener_get_fd(listener);
  ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range& e) {
    server_log.warning("Can\'t determine version for socket %d; disconnecting client", listen_fd);
    close(fd);
    return;
  }

  struct bufferevent* bev = bufferevent_socket_new(this->base.get(), fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  auto c = make_shared<Client>(
      this->shared_from_this(),
      bev,
      listening_socket->version,
      this->config->idle_timeout_usecs,
      this->config->hide_data_from_logs);
  c->channel.on_command_received = PatchServer::on_client_input;
  c->channel.on_error = PatchServer::on_client_error;
  c->channel.context_obj = this;
  this->channel_to_client.emplace(&c->channel, c);

  server_log.info("Patch client connected: C-%" PRIX64 " on fd %d via %d (%s)",
      c->id, fd, listen_fd, listening_socket->addr_str.c_str());

  this->send_server_init(c);
}

void PatchServer::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  server_log.error("Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void PatchServer::on_client_input(Channel& ch, uint16_t command, uint32_t, std::string& data) {
  PatchServer* server = reinterpret_cast<PatchServer*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  try {
    switch (command) {
      case 0x02:
        server->on_02(c, data);
        break;
      case 0x04:
        server->on_04(c, data);
        break;
      case 0x0F:
        server->on_0F(c, data);
        break;
      case 0x10:
        server->on_10(c, data);
        break;
      default:
        throw runtime_error("invalid command");
    }
  } catch (const exception& e) {
    server_log.warning("Error processing client command: %s", e.what());
  }
}

void PatchServer::on_client_error(Channel& ch, short events) {
  PatchServer* server = reinterpret_cast<PatchServer*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    server_log.warning("Client caused error %d (%s)", err, evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    server->disconnect_client(c);
  }
}

PatchServer::PatchServer(shared_ptr<const Config> config)
    : config(config) {
  if (config->shared_base) {
    this->base = config->shared_base;
    this->base_is_shared = true;
  } else {
    this->base.reset(event_base_new(), event_base_free);
    this->base_is_shared = false;
  }
  this->destroy_clients_ev.reset(
      event_new(this->base.get(), -1, EV_TIMEOUT, &PatchServer::dispatch_destroy_clients, this), event_free);
  if (!this->base_is_shared) {
    this->th = thread(&PatchServer::thread_fn, this);
  }
}

void PatchServer::schedule_stop() {
  if (!this->base_is_shared) {
    event_base_loopexit(this->base.get(), nullptr);
  }
}

void PatchServer::wait_for_stop() {
  if (!this->base_is_shared) {
    this->th.join();
  }
}

void PatchServer::listen(const std::string& addr_str, const string& socket_path, Version version) {
  int fd = phosg::listen(socket_path, 0, SOMAXCONN);
  server_log.info("Listening on Unix socket %s on fd %d as %s", socket_path.c_str(), fd, addr_str.c_str());
  this->add_socket(addr_str, fd, version);
}

void PatchServer::listen(const std::string& addr_str, const string& addr, int port, Version version) {
  if (port == 0) {
    this->listen(addr_str, addr, version);
  } else {
    int fd = phosg::listen(addr, port, SOMAXCONN);
    string netloc_str = phosg::render_netloc(addr, port);
    server_log.info("Listening on TCP interface %s on fd %d as %s", netloc_str.c_str(), fd, addr_str.c_str());
    this->add_socket(addr_str, fd, version);
  }
}

void PatchServer::listen(const std::string& addr_str, int port, Version version) {
  this->listen(addr_str, "", port, version);
}

PatchServer::ListeningSocket::ListeningSocket(PatchServer* s, const std::string& addr_str, int fd, Version version)
    : addr_str(addr_str),
      fd(fd),
      version(version),
      listener(evconnlistener_new(s->base.get(), PatchServer::dispatch_on_listen_accept, s, LEV_OPT_REUSEABLE, 0, this->fd),
          evconnlistener_free) {
  evconnlistener_set_error_cb(this->listener.get(), PatchServer::dispatch_on_listen_error);
}

void PatchServer::add_socket(const std::string& addr_str, int fd, Version version) {
  this->listening_sockets.emplace(piecewise_construct, forward_as_tuple(fd), forward_as_tuple(this, addr_str, fd, version));
}

void PatchServer::thread_fn() {
  event_base_loop(this->base.get(), EVLOOP_NO_EXIT_ON_EMPTY);
}

void PatchServer::set_config(std::shared_ptr<const Config> config) {
  if (this->base_is_shared) {
    this->config = config;
  } else {
    forward_to_event_thread(this->base, [s = this->shared_from_this(), config = std::move(config)]() {
      s->config = config;
    });
  }
}
