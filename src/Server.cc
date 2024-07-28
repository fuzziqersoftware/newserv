#include "Server.hh"

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

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;
using namespace std::placeholders;

void Server::disconnect_client(shared_ptr<Client> c) {
  if (c->channel.virtual_network_id) {
    server_log.info("Client disconnected: C-%" PRIX64 " on N-%" PRIu64, c->id, c->channel.virtual_network_id);
  } else if (c->channel.bev) {
    server_log.info("Client disconnected: C-%" PRIX64, c->id);
  } else {
    server_log.info("Client C-%" PRIX64 " removed from game server", c->id);
  }

  this->state->channel_to_client.erase(&c->channel);
  c->channel.disconnect();

  try {
    on_disconnect(c);
  } catch (const exception& e) {
    server_log.warning("Error during client disconnect cleanup: %s", e.what());
  }

  // We can't just let c be destroyed here, since disconnect_client can be
  // called from within the client's channel's receive handler. So, we instead
  // move it to another set, which we'll clear in an immediately-enqueued
  // callback after the current event. This will also call the client's
  // disconnect hooks (if any).
  this->clients_to_destroy.insert(std::move(c));
  this->enqueue_destroy_clients();
}

void Server::enqueue_destroy_clients() {
  auto tv = phosg::usecs_to_timeval(0);
  event_add(this->destroy_clients_ev.get(), &tv);
}

void Server::dispatch_destroy_clients(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<Server*>(ctx)->destroy_clients();
}

void Server::destroy_clients() {
  for (auto c_it = this->clients_to_destroy.begin();
       c_it != this->clients_to_destroy.end();
       c_it = this->clients_to_destroy.erase(c_it)) {
    auto c = *c_it;
    // Note: It's important to move the disconnect hooks out of the client here
    // because the hooks could modify c->disconnect_hooks while it's being
    // iterated here, which would invalidate these iterators.
    unordered_map<string, function<void()>> hooks = std::move(c->disconnect_hooks);
    for (auto h_it : hooks) {
      try {
        h_it.second();
      } catch (const exception& e) {
        c->log.warning("Disconnect hook %s failed: %s", h_it.first.c_str(), e.what());
      }
    }
  }
}

void Server::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_accept(listener, fd, address, socklen);
}

void Server::dispatch_on_listen_error(struct evconnlistener* listener, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_error(listener);
}

void Server::on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr*, int) {
  struct sockaddr_storage remote_addr;
  phosg::get_socket_addresses(fd, nullptr, &remote_addr);
  if (this->state->banned_ipv4_ranges->check(remote_addr)) {
    close(fd);
    return;
  }

  int listen_fd = evconnlistener_get_fd(listener);
  ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range& e) {
    server_log.warning("Can\'t determine version for socket %d; disconnecting client",
        listen_fd);
    close(fd);
    return;
  }

  struct bufferevent* bev = bufferevent_socket_new(this->base.get(), fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  auto c = make_shared<Client>(this->shared_from_this(), bev, 0, listening_socket->version, listening_socket->behavior);
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;
  this->state->channel_to_client.emplace(&c->channel, c);

  server_log.info("Client connected: C-%" PRIX64 " on fd %d via %d (%s)",
      c->id, fd, listen_fd, listening_socket->addr_str.c_str());

  try {
    on_connect(c);
  } catch (const exception& e) {
    server_log.warning("Error during client initialization: %s", e.what());
    this->disconnect_client(c);
  }
}

void Server::connect_virtual_client(
    struct bufferevent* bev,
    uint64_t virtual_network_id,
    uint32_t address,
    uint16_t client_port,
    uint16_t server_port,
    Version version,
    ServerBehavior initial_state) {
  auto c = make_shared<Client>(this->shared_from_this(), bev, virtual_network_id, version, initial_state);
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;
  this->state->channel_to_client.emplace(&c->channel, c);

  server_log.info(
      "Client connected: C-%" PRIX64 " on virtual network N-%" PRIu64 " via T-%hu-%s-%s-VI",
      c->id,
      virtual_network_id,
      server_port,
      phosg::name_for_enum(version),
      phosg::name_for_enum(initial_state));

  // Manually set the remote address, since the bufferevent has no fd and the
  // Channel constructor can't figure out the virtual remote address
  auto* remote_sin = reinterpret_cast<sockaddr_in*>(&c->channel.remote_addr);
  remote_sin->sin_family = AF_INET;
  remote_sin->sin_addr.s_addr = htonl(address);
  remote_sin->sin_port = htons(client_port);

  try {
    on_connect(c);
  } catch (const exception& e) {
    server_log.error("Error during client initialization: %s", e.what());
    this->disconnect_client(c);
  }
}

void Server::connect_virtual_client(shared_ptr<Client> c, Channel&& ch) {
  c->channel.replace_with(std::move(ch), Server::on_client_input, Server::on_client_error, this, phosg::string_printf("C-%" PRIX64, c->id));
  this->state->channel_to_client.emplace(&c->channel, c);
  server_log.info("Client C-%" PRIX64 " added to game server", c->id);
}

void Server::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  server_log.error("Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void Server::on_client_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->state->channel_to_client.at(&ch);

  if (c->should_disconnect) {
    server->disconnect_client(c);
  } else {
    if (server->state->catch_handler_exceptions) {
      try {
        on_command(c, command, flag, data);
      } catch (const exception& e) {
        server_log.warning("Error processing client command: %s", e.what());
        c->should_disconnect = true;
      }
    } else {
      on_command(c, command, flag, data);
    }
    if (c->should_disconnect) {
      server->disconnect_client(c);
    }
  }
}

void Server::on_client_error(Channel& ch, short events) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->state->channel_to_client.at(&ch);

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    server_log.warning("Client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    server->disconnect_client(c);
  }
}

Server::Server(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
    : base(base),
      destroy_clients_ev(event_new(this->base.get(), -1, EV_TIMEOUT, &Server::dispatch_destroy_clients, this), event_free),
      state(state) {}

void Server::listen(const std::string& addr_str, const string& socket_path, Version version, ServerBehavior behavior) {
  int fd = phosg::listen(socket_path, 0, SOMAXCONN);
  server_log.info("Listening on Unix socket %s on fd %d as %s", socket_path.c_str(), fd, addr_str.c_str());
  this->add_socket(addr_str, fd, version, behavior);
}

void Server::listen(
    const std::string& addr_str,
    const string& addr,
    int port,
    Version version,
    ServerBehavior behavior) {
  if (port == 0) {
    this->listen(addr_str, addr, version, behavior);
  } else {
    int fd = phosg::listen(addr, port, SOMAXCONN);
    string netloc_str = phosg::render_netloc(addr, port);
    server_log.info("Listening on TCP interface %s on fd %d as %s", netloc_str.c_str(), fd, addr_str.c_str());
    this->add_socket(addr_str, fd, version, behavior);
  }
}

void Server::listen(const std::string& addr_str, int port, Version version, ServerBehavior behavior) {
  this->listen(addr_str, "", port, version, behavior);
}

Server::ListeningSocket::ListeningSocket(
    Server* s, const std::string& addr_str,
    int fd, Version version, ServerBehavior behavior)
    : addr_str(addr_str),
      fd(fd),
      version(version),
      behavior(behavior),
      listener(evconnlistener_new(
                   s->base.get(), Server::dispatch_on_listen_accept, s,
                   LEV_OPT_REUSEABLE, 0, this->fd),
          evconnlistener_free) {
  evconnlistener_set_error_cb(
      this->listener.get(),
      Server::dispatch_on_listen_error);
}

void Server::add_socket(
    const std::string& addr_str,
    int fd,
    Version version,
    ServerBehavior behavior) {
  this->listening_sockets.emplace(
      piecewise_construct, forward_as_tuple(fd),
      forward_as_tuple(this, addr_str, fd, version, behavior));
}

shared_ptr<Client> Server::get_client() const {
  if (this->state->channel_to_client.empty()) {
    throw runtime_error("no clients on game server");
  }
  if (this->state->channel_to_client.size() > 1) {
    throw runtime_error("multiple clients on game server");
  }
  return this->state->channel_to_client.begin()->second;
}

vector<shared_ptr<Client>> Server::get_clients_by_identifier(const string& ident) const {
  int64_t account_id_hex = -1;
  int64_t account_id_dec = -1;
  try {
    account_id_dec = stoul(ident, nullptr, 10);
  } catch (const invalid_argument&) {
  }
  try {
    account_id_hex = stoul(ident, nullptr, 16);
  } catch (const invalid_argument&) {
  }

  // TODO: It's kind of not great that we do a linear search here, but this is
  // only used in the shell, so it should be pretty rare.
  vector<shared_ptr<Client>> results;
  for (const auto& it : this->state->channel_to_client) {
    auto c = it.second;
    if (c->login && c->login->account->account_id == account_id_hex) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->account->account_id == account_id_dec) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->xb_license && c->login->xb_license->gamertag == ident) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->bb_license && c->login->bb_license->username == ident) {
      results.emplace_back(c);
      continue;
    }

    auto p = c->character(false, false);
    if (p && p->disp.name.eq(ident, p->inventory.language)) {
      results.emplace_back(c);
      continue;
    }

    if (c->channel.name == ident) {
      results.emplace_back(c);
      continue;
    }
    if (phosg::starts_with(c->channel.name, ident + " ")) {
      results.emplace_back(c);
      continue;
    }
  }

  return results;
}

vector<shared_ptr<Client>> Server::all_clients() const {
  vector<shared_ptr<Client>> ret;
  for (const auto& it : this->state->channel_to_client) {
    ret.emplace_back(it.second);
  }
  return ret;
}

shared_ptr<struct event_base> Server::get_base() const {
  return this->base;
}
