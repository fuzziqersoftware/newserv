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
  c->bev = NULL;

  // if the output buffer is not empty, move the client into the draining pool
  // instead of disconnecting it, to make sure all the data gets sent
  struct evbuffer* out_buffer = bufferevent_get_output(bev);
  if (evbuffer_get_length(out_buffer) == 0) {
    bufferevent_free(bev);
  } else {
    // the callbacks will free it when all the data is sent or the client
    // disconnects
    bufferevent_setcb(bev, NULL,
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
    evutil_socket_t fd, struct sockaddr* address, int socklen) {

  int listen_fd = evconnlistener_get_fd(listener);
  ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range& e) {
    log(WARNING, "[Server] can\'t determine version for socket %d; disconnecting client",
        listen_fd);
    close(fd);
    return;
  }

  log(INFO, "[Server] client connected via fd %d", listen_fd);

  struct bufferevent *bev = bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  shared_ptr<Client> c(new Client(bev, listening_socket->version,
      listening_socket->behavior));
  this->bev_to_client.emplace(make_pair(bev, c));

  bufferevent_setcb(bev, &Server::dispatch_on_client_input, NULL,
      &Server::dispatch_on_client_error, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  process_connect(this->state, c);
}

void Server::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[Server] failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), NULL);
}

void Server::on_client_input(struct bufferevent* bev) {
  shared_ptr<Client> c;
  try {
    c = this->bev_to_client.at(bev);
  } catch (const out_of_range& e) {
    log(WARNING, "[Server] received message from client with no configuration");

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
  bufferevent_free(bev);
}

void Server::on_client_error(struct bufferevent* bev, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[Server] client caused %d (%s)", err,
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
    log(WARNING, "[Server] disconnecting client caused %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    bufferevent_free(bev);
  }
}

void Server::receive_and_process_commands(shared_ptr<Client> c) {
  struct evbuffer* buf = bufferevent_get_input(c->bev);
  size_t header_size = (c->version == GameVersion::BB) ? 8 : 4;

  // read as much data into recv_buffer as we can and decrypt it
  size_t existing_bytes = c->recv_buffer.size();
  size_t new_bytes = evbuffer_get_length(buf);
  new_bytes &= ~(header_size - 1); // only read in multiples of header_size
  c->recv_buffer.resize(existing_bytes + new_bytes);
  void* recv_ptr = const_cast<char*>(c->recv_buffer.data() + existing_bytes);
  if (evbuffer_remove(buf, recv_ptr, new_bytes) != static_cast<ssize_t>(new_bytes)) {
    throw runtime_error("some bytes could not be read from the receive buffer");
  }

  // decrypt the received data if encryption is enabled
  if (c->crypt_in.get()) {
    c->crypt_in->decrypt(recv_ptr, new_bytes);
  }

  // process as many commands as possible
  size_t offset = 0;
  while (offset < c->recv_buffer.size()) {
    const PSOCommandHeader* header = reinterpret_cast<const PSOCommandHeader*>(
        c->recv_buffer.data() + offset);
    size_t size = header->size(c->version);
    if (offset + size > c->recv_buffer.size()) {
      break; // don't have a complete command; we're done for now
    }

    // if we get here, then we have a complete, decrypted command waiting to be
    // processed. we copy it out and append zeroes on the end so that it's safe
    // to call string functions on the buffer in command handlers
    string data = c->recv_buffer.substr(offset + header_size, size - header_size);
    data.append(4, '\0');
    try {
      process_command(this->state, c, header->command(c->version),
          header->flag(c->version), size - header_size, data.data());
    } catch (const exception& e) {
      log(INFO, "[Server] error in client stream: %s", e.what());
      c->should_disconnect = true;
      return;
    }

    // BB pads commands to 8-byte boundaries, so if we see a shorter command,
    // skip over the padding
    offset += (size + header_size - 1) & ~(header_size - 1);
  }

  // remove the processed commands from the receive buffer
  c->recv_buffer = c->recv_buffer.substr(offset);
}

Server::Server(shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state) : base(base), state(state) { }

void Server::listen(const string& socket_path, GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(socket_path, 0, SOMAXCONN);
  log(INFO, "[Server] listening on unix socket %s (version %s) on fd %d",
      socket_path.c_str(), name_for_version(version), fd);
  this->add_socket(fd, version, behavior);
}

void Server::listen(const string& addr, int port, GameVersion version,
    ServerBehavior behavior) {
  int fd = ::listen(addr, port, SOMAXCONN);
  string netloc_str = render_netloc(addr, port);
  log(INFO, "[Server] listening on tcp interface %s (version %s) on fd %d",
      netloc_str.c_str(), name_for_version(version), fd);
  this->add_socket(fd, version, behavior);
}

void Server::listen(int port, GameVersion version, ServerBehavior behavior) {
  this->listen("", port, version, behavior);
}

Server::ListeningSocket::ListeningSocket(Server* s, int fd,
    GameVersion version, ServerBehavior behavior) :
    fd(fd), version(version), behavior(behavior), listener(
        evconnlistener_new(s->base.get(), Server::dispatch_on_listen_accept, s,
          LEV_OPT_REUSEABLE, 0, this->fd), evconnlistener_free) {
  evconnlistener_set_error_cb(this->listener.get(),
      Server::dispatch_on_listen_error);
}

void Server::add_socket(int fd, GameVersion version, ServerBehavior behavior) {
  this->listening_sockets.emplace(piecewise_construct, forward_as_tuple(fd),
      forward_as_tuple(this, fd, version, behavior));
}
