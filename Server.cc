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
#include <thread>

#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;



Server::WorkerThread::WorkerThread(Server* server, int worker_num) :
    server(server), worker_num(worker_num),
    base(event_base_new(), event_base_free), t() {
  this->thread_name = string_printf("Server::run_thread (worker_num=%d)",
      worker_num);
}

void Server::WorkerThread::disconnect_client(struct bufferevent* bev) {
  {
    auto client = this->bev_to_client.at(bev);
    this->bev_to_client.erase(bev);
    this->server->client_count--;

    rw_guard g(client->lock, true);
    client->bev = NULL;
  }

  // if the output buffer is not empty, move the client into the draining pool
  // instead of disconnecting it, to make sure all the data gets sent
  struct evbuffer* out_buffer = bufferevent_get_output(bev);
  if (evbuffer_get_length(out_buffer) == 0) {
    bufferevent_free(bev);
  } else {
    // the callbacks will free it when all the data is sent or the client
    // disconnects
    bufferevent_setcb(bev, NULL,
        Server::WorkerThread::dispatch_on_disconnecting_client_output,
        Server::WorkerThread::dispatch_on_disconnecting_client_error, this);
    bufferevent_disable(bev, EV_READ);
  }
}

void Server::WorkerThread::dispatch_on_listen_accept(
    struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_listen_accept(*wt, listener, fd, address, socklen);
}

void Server::WorkerThread::dispatch_on_listen_error(
    struct evconnlistener *listener, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_listen_error(*wt, listener);
}

void Server::WorkerThread::dispatch_on_client_input(
    struct bufferevent *bev, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_client_input(*wt, bev);
}

void Server::WorkerThread::dispatch_on_client_error(
    struct bufferevent *bev, short events, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_client_error(*wt, bev, events);
}

void Server::WorkerThread::dispatch_on_disconnecting_client_output(
    struct bufferevent *bev, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_disconnecting_client_output(*wt, bev);
}

void Server::WorkerThread::dispatch_on_disconnecting_client_error(
    struct bufferevent *bev, short events, void *ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->on_disconnecting_client_error(*wt, bev, events);
}

void Server::WorkerThread::dispatch_check_for_thread_exit(
    evutil_socket_t fd, short what, void* ctx) {
  WorkerThread* wt = (WorkerThread*)ctx;
  wt->server->check_for_thread_exit(*wt, fd, what);
}

void Server::on_listen_accept(Server::WorkerThread& wt,
    struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen) {

  int fd_flags = fcntl(fd, F_GETFD, 0);
  if (fd_flags >= 0) {
    fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC);
  }

  int listen_fd = evconnlistener_get_fd(listener);
  GameVersion version;
  ServerBehavior initial_state;
  try {
    auto p = this->listen_fd_to_version_and_state.at(listen_fd);
    version = p.first;
    initial_state = p.second;
  } catch (const out_of_range& e) {
    log(WARNING, "[Server] can\'t determine version for socket %d; disconnecting client",
        listen_fd);
    close(fd);
    return;
  }

  struct bufferevent *bev = bufferevent_socket_new(wt.base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_UNLOCK_CALLBACKS);
  auto emplace_ret = wt.bev_to_client.emplace(bev, new Client(bev, version, initial_state));
  this->client_count++;

  bufferevent_setcb(bev, &WorkerThread::dispatch_on_client_input, NULL,
      &WorkerThread::dispatch_on_client_error, &wt);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  this->process_client_connect(emplace_ret.first->second);
}

void Server::on_listen_error(Server::WorkerThread& wt,
    struct evconnlistener *listener) {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[Server] failure on listening socket %d: %d (%s)\n",
      evconnlistener_get_fd(listener), err,
      evutil_socket_error_to_string(err));
  event_base_loopexit(wt.base.get(), NULL);
}

void Server::on_client_input(Server::WorkerThread& wt,
    struct bufferevent *bev) {
  shared_ptr<Client> c;
  try {
    c = wt.bev_to_client.at(bev);
  } catch (const out_of_range& e) {
    log(WARNING, "[Server] received message from client with no configuration");

    // ignore all the data
    struct evbuffer* in_buffer = bufferevent_get_input(bev);
    evbuffer_drain(in_buffer, evbuffer_get_length(in_buffer));
    return;
  }

  if (c->should_disconnect) {
    wt.disconnect_client(bev);
    this->process_client_disconnect(c);
    return;
  }

  c->last_recv_time = now();
  this->receive_and_process_commands(c, bev);

  if (c->should_disconnect) {
    wt.disconnect_client(bev);
    this->process_client_disconnect(c);
    return;
  }
}

void Server::on_disconnecting_client_output(Server::WorkerThread& wt,
    struct bufferevent *bev) {
  bufferevent_free(bev);
}

void Server::on_client_error(Server::WorkerThread& wt,
    struct bufferevent *bev, short events) {
  shared_ptr<Client> c;
  try {
    c = wt.bev_to_client.at(bev);
  } catch (const out_of_range& e) { }

  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[Server] client caused %d (%s)\n", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    wt.disconnect_client(bev);
    if (c) {
      this->process_client_disconnect(c);
    }
  }
}

void Server::on_disconnecting_client_error(Server::WorkerThread& wt,
    struct bufferevent *bev, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[Server] disconnecting client caused %d (%s)\n", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    bufferevent_free(bev);
  }
}

void Server::check_for_thread_exit(Server::WorkerThread& wt,
    evutil_socket_t fd, short what) {
  if (this->should_exit) {
    event_base_loopexit(wt.base.get(), NULL);
  }
}

void Server::receive_and_process_commands(shared_ptr<Client> c, struct bufferevent* bev) {
  struct evbuffer* buf = bufferevent_get_input(bev);
  size_t header_size = (c->version == GameVersion::BB) ? 8 : 4;

  // read as much data into recv_buffer as we can and decrypt it
  size_t existing_bytes = c->recv_buffer.size();
  size_t new_bytes = evbuffer_get_length(buf);
  new_bytes &= ~(header_size - 1); // only read in multiples of header_size
  c->recv_buffer.resize(existing_bytes + new_bytes);
  void* recv_ptr = const_cast<char*>(c->recv_buffer.data() + existing_bytes);
  if (evbuffer_remove(buf, recv_ptr, new_bytes) != new_bytes) {
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

void Server::process_client_connect(std::shared_ptr<Client> c) {
  process_connect(this->state, c);
}

void Server::process_client_disconnect(std::shared_ptr<Client> c) {
  process_disconnect(this->state, c);
}

void Server::run_thread(int worker_num) {
  WorkerThread& wt = this->threads[worker_num];

  struct timeval tv = usecs_to_timeval(2000000);

  struct event* ev = event_new(wt.base.get(), -1, EV_PERSIST,
      &WorkerThread::dispatch_check_for_thread_exit, &wt);
  event_add(ev, &tv);

  event_base_dispatch(wt.base.get());

  event_del(ev);
}

Server::Server(shared_ptr<ServerState> state) :
    should_exit(false), client_count(0), state(state) {
  for (size_t x = 0; x < this->state->num_threads; x++) {
    this->threads.emplace_back(this, x);
  }
}

void Server::listen(const string& socket_path, GameVersion version, ServerBehavior initial_state) {
  int fd = ::listen(socket_path, 0, SOMAXCONN);
  log(INFO, "[Server] listening on unix socket %s (version %s) on fd %d",
      socket_path.c_str(), name_for_version(version), fd);
  this->add_socket(fd, version, initial_state);
}

void Server::listen(const string& addr, int port, GameVersion version, ServerBehavior initial_state) {
  int fd = ::listen(addr, port, SOMAXCONN);
  string netloc_str = render_netloc(addr, port);
  log(INFO, "[Server] listening on tcp interface %s (version %s) on fd %d",
      netloc_str.c_str(), name_for_version(version), fd);
  this->add_socket(fd, version, initial_state);
}

void Server::listen(int port, GameVersion version, ServerBehavior initial_state) {
  this->listen("", port, version, initial_state);
}

void Server::add_socket(int fd, GameVersion version, ServerBehavior initial_state) {
  this->listen_fd_to_version_and_state.emplace(piecewise_construct,
      forward_as_tuple(fd), forward_as_tuple(version, initial_state));
}

void Server::start() {
  for (auto& wt : this->threads) {
    for (const auto& it : this->listen_fd_to_version_and_state) {
      struct evconnlistener* listener = evconnlistener_new(wt.base.get(),
          WorkerThread::dispatch_on_listen_accept, &wt, LEV_OPT_REUSEABLE, 0,
          it.first);
      if (!listener) {
        throw runtime_error("can\'t create evconnlistener");
      }
      evconnlistener_set_error_cb(listener, WorkerThread::dispatch_on_listen_error);
      wt.listeners.emplace(listener, evconnlistener_free);
    }
    wt.t = thread(&Server::run_thread, this, wt.worker_num);
  }
}

void Server::schedule_stop() {
  log(INFO, "[Server] scheduling exit for all threads");
  this->should_exit = true;

  for (const auto& it : listen_fd_to_version_and_state) {
    log(INFO, "[Server] closing listening fd %d", it.first);
    close(it.first);
  }
}

void Server::wait_for_stop() {
  for (auto& wt : this->threads) {
    if (!wt.t.joinable()) {
      continue;
    }
    log(INFO, "[Server] waiting for worker %d to terminate", wt.worker_num);
    wt.t.join();
  }
  log(INFO, "[Server] shutdown complete");
}
