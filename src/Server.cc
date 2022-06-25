#include "Server.hh"

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
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;
using namespace std::placeholders;



void Server::disconnect_client(shared_ptr<Client> c) {
  if (c->channel.is_virtual_connection) {
    this->log(INFO, "Disconnecting client on virtual connection %p",
        c->channel.bev.get());
  } else {
    this->log(INFO, "Disconnecting client on fd %d",
        bufferevent_getfd(c->channel.bev.get()));
  }

  this->channel_to_client.erase(&c->channel);
  c->channel.disconnect();

  process_disconnect(this->state, c);
  // c is destroyed here (process_disconnect should remove any other references
  // to it, e.g. from Lobby objects)
}

void Server::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_accept(listener, fd, address,
      socklen);
}

void Server::dispatch_on_listen_error(struct evconnlistener* listener,
    void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_listen_error(listener);
}

void Server::on_listen_accept(struct evconnlistener* listener,
    evutil_socket_t fd, struct sockaddr*, int) {

  int listen_fd = evconnlistener_get_fd(listener);
  ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range& e) {
    this->log(WARNING, "Can\'t determine version for socket %d; disconnecting client",
        listen_fd);
    close(fd);
    return;
  }

  this->log(INFO, "Client fd %d connected via fd %d (%s)",
      fd, listen_fd, listening_socket->name.c_str());

  struct bufferevent *bev = bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  shared_ptr<Client> c(new Client(
      bev, listening_socket->version, listening_socket->behavior));
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;
  this->channel_to_client.emplace(&c->channel, c);

  process_connect(this->state, c);
}

void Server::connect_client(
    struct bufferevent* bev, uint32_t address, uint16_t port,
    GameVersion version, ServerBehavior initial_state) {
  this->log(INFO, "Client connected on virtual connection %p", bev);

  shared_ptr<Client> c(new Client(bev, version, initial_state));
  c->channel.on_command_received = Server::on_client_input;
  c->channel.on_error = Server::on_client_error;
  c->channel.context_obj = this;

  this->channel_to_client.emplace(&c->channel, c);

  // Manually set the remote address, since the bufferevent has no fd and the
  // Channel constructor can't figure out the virtual remote address
  auto* sin = reinterpret_cast<sockaddr_in*>(&c->channel.remote_addr);
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl(address);
  sin->sin_port = htons(port);

  process_connect(this->state, c);
}

void Server::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  this->log(ERROR, "Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void Server::on_client_input(Channel& ch, uint16_t command, uint32_t flag, std::string& data) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  if (c->should_disconnect) {
    server->disconnect_client(c);
  } else {
    process_command(server->state, c, command, flag, data);
    if (c->should_disconnect) {
      server->disconnect_client(c);
    }
  }
}

void Server::on_client_error(Channel& ch, short events) {
  Server* server = reinterpret_cast<Server*>(ch.context_obj);
  shared_ptr<Client> c = server->channel_to_client.at(&ch);

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    server->log(WARNING, "Client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    server->disconnect_client(c);
  }
}

Server::Server(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
  : log("[Server] "),
    base(base),
    state(state) { }

void Server::listen(
    const std::string& name,
    const string& socket_path,
    GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(socket_path, 0, SOMAXCONN);
  this->log(INFO, "Listening on Unix socket %s (%s) on fd %d (name: %s)",
      socket_path.c_str(), name_for_version(version), fd, name.c_str());
  this->add_socket(name, fd, version, behavior);
}

void Server::listen(
    const std::string& name,
    const string& addr,
    int port,
    GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(addr, port, SOMAXCONN);
  string netloc_str = render_netloc(addr, port);
  this->log(INFO, "Listening on TCP interface %s (%s) on fd %d (name: %s)",
      netloc_str.c_str(), name_for_version(version), fd, name.c_str());
  this->add_socket(name, fd, version, behavior);
}

void Server::listen(const std::string& name, int port, GameVersion version, ServerBehavior behavior) {
  this->listen(name, "", port, version, behavior);
}

Server::ListeningSocket::ListeningSocket(Server* s, const std::string& name,
    int fd, GameVersion version, ServerBehavior behavior) :
    name(name), fd(fd), version(version), behavior(behavior), listener(
        evconnlistener_new(s->base.get(), Server::dispatch_on_listen_accept, s,
          LEV_OPT_REUSEABLE, 0, this->fd), evconnlistener_free) {
  evconnlistener_set_error_cb(this->listener.get(),
      Server::dispatch_on_listen_error);
}

void Server::add_socket(
    const std::string& name,
    int fd,
    GameVersion version,
    ServerBehavior behavior) {
  this->listening_sockets.emplace(piecewise_construct, forward_as_tuple(fd),
      forward_as_tuple(this, name, fd, version, behavior));
}

shared_ptr<Client> Server::get_client() const {
  if (this->channel_to_client.empty()) {
    throw runtime_error("no clients on game server");
  }
  if (this->channel_to_client.size() > 1) {
    throw runtime_error("multiple clients on game server");
  }
  return this->channel_to_client.begin()->second;
}
