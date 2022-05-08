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



void Server::disconnect_client(struct bufferevent* bev) {
  this->disconnect_client(this->bev_to_client.at(bev));
}

void Server::disconnect_client(shared_ptr<Client> c) {
  this->bev_to_client.erase(c->bev);
  struct bufferevent* bev = c->bev;
  c->bev = nullptr;

  int fd = bufferevent_getfd(bev);
  if (fd < 0) {
    this->log(INFO, "Client on virtual connection %p disconnected", bev);
  } else {
    this->log(INFO, "Client on fd %d disconnected", fd);
  }

  // if the output buffer is not empty, move the client into the draining pool
  // instead of disconnecting it, to make sure all the data gets sent
  struct evbuffer* out_buffer = bufferevent_get_output(bev);
  if (evbuffer_get_length(out_buffer) == 0) {
    bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
    bufferevent_free(bev);
  } else {
    // the callbacks will free it when all the data is sent or the client
    // disconnects
    bufferevent_setcb(bev, nullptr,
        Server::dispatch_on_disconnecting_client_output,
        Server::dispatch_on_disconnecting_client_error, this);
    bufferevent_disable(bev, EV_READ);
  }

  process_disconnect(this->state, c);
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

void Server::dispatch_on_client_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_client_input(bev);
}

void Server::dispatch_on_client_error(struct bufferevent* bev, short events,
    void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_client_error(bev, events);
}

void Server::dispatch_on_disconnecting_client_output(struct bufferevent* bev,
    void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_disconnecting_client_output(bev);
}

void Server::dispatch_on_disconnecting_client_error(struct bufferevent* bev,
    short events, void* ctx) {
  reinterpret_cast<Server*>(ctx)->on_disconnecting_client_error(bev, events);
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
  shared_ptr<Client> c(new Client(bev, listening_socket->version,
      listening_socket->behavior));
  this->bev_to_client.emplace(make_pair(bev, c));

  bufferevent_setcb(bev, &Server::dispatch_on_client_input, nullptr,
      &Server::dispatch_on_client_error, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  process_connect(this->state, c);
}

void Server::connect_client(
    struct bufferevent* bev, uint32_t address, uint16_t port,
    GameVersion version, ServerBehavior initial_state) {
  this->log(INFO, "Client connected on virtual connection %p", bev);

  shared_ptr<Client> c(new Client(bev, version, initial_state));
  this->bev_to_client.emplace(make_pair(bev, c));

  // Manually set the remote address, since the bufferevent has no fd and the
  // Client constructor can't figure out the virtual remote address
  auto* sin = reinterpret_cast<sockaddr_in*>(&c->remote_addr);
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl(address);
  sin->sin_port = htons(port);

  bufferevent_setcb(bev, &Server::dispatch_on_client_input, nullptr,
      &Server::dispatch_on_client_error, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  process_connect(this->state, c);
}

void Server::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  this->log(ERROR, "Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void Server::on_client_input(struct bufferevent* bev) {
  shared_ptr<Client> c;
  try {
    c = this->bev_to_client.at(bev);
  } catch (const out_of_range& e) {
    this->log(WARNING, "Received message from client with no configuration");

    // ignore all the data
    // TODO: we probably should disconnect them or something
    struct evbuffer* in_buffer = bufferevent_get_input(bev);
    evbuffer_drain(in_buffer, evbuffer_get_length(in_buffer));
    return;
  }

  if (c->should_disconnect) {
    this->disconnect_client(bev);
    return;
  }

  c->last_recv_time = now();
  this->receive_and_process_commands(c);

  if (c->should_disconnect) {
    this->disconnect_client(bev);
    return;
  }
}

void Server::on_disconnecting_client_output(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}

void Server::on_client_error(struct bufferevent* bev, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    this->log(WARNING, "Client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    this->disconnect_client(bev);
  }
}

void Server::on_disconnecting_client_error(struct bufferevent* bev,
    short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    this->log(WARNING, "Disconnecting client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
    bufferevent_free(bev);
  }
}

void Server::receive_and_process_commands(shared_ptr<Client> c) {
  try {
    for_each_received_command(c->bev, c->version, c->crypt_in.get(),
        [this, c](uint16_t command, uint16_t flag, const std::string& data) {
          process_command(this->state, c, command, flag, data);
        });
  } catch (const exception& e) {
    this->log(INFO, "Error in client stream: %s", e.what());
    c->should_disconnect = true;
    return;
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
