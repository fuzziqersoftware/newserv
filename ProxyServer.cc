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
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;



ProxyServer::ProxyServer(shared_ptr<struct event_base> base,
    const struct sockaddr_storage& initial_destination, int listen_port) :
    base(base), listener(evconnlistener_new(this->base.get(),
      ProxyServer::dispatch_on_listen_accept, this, LEV_OPT_REUSEABLE, 0,
      ::listen("", listen_port, SOMAXCONN)), evconnlistener_free),
    client_bev(nullptr, bufferevent_free),
    server_bev(nullptr, bufferevent_free),
    next_destination(initial_destination), listen_port(listen_port) {
  memset(&this->client_input_header, 0, sizeof(this->client_input_header));
  memset(&this->server_input_header, 0, sizeof(this->server_input_header));
}



void ProxyServer::send_to_client(const std::string& data) {
  this->send_to_end(data, false);
}

void ProxyServer::send_to_server(const std::string& data) {
  this->send_to_end(data, true);
}

void ProxyServer::send_to_end(const std::string& data, bool to_server) {
  struct bufferevent* bev = to_server ? this->server_bev.get() : this->client_bev.get();
  if (!bev) {
    throw runtime_error("connection not open");
  }

  struct evbuffer* buf = bufferevent_get_output(bev);

  PSOEncryption* crypt = to_server ? this->server_output_crypt.get() : this->client_output_crypt.get();
  if (crypt) {
    string crypted_data = data;
    crypt->encrypt(const_cast<char*>(crypted_data.data()), crypted_data.size());
    evbuffer_add(buf, crypted_data.data(), crypted_data.size());
  } else {
    evbuffer_add(buf, data.data(), data.size());
  }
}



void ProxyServer::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_listen_accept(listener, fd, address,
      socklen);
}

void ProxyServer::dispatch_on_listen_error(struct evconnlistener* listener,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_listen_error(listener);
}

void ProxyServer::dispatch_on_client_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_client_input(bev);
}

void ProxyServer::dispatch_on_client_error(struct bufferevent* bev, short events,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_client_error(bev, events);
}

void ProxyServer::dispatch_on_server_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_server_input(bev);
}

void ProxyServer::dispatch_on_server_error(struct bufferevent* bev, short events,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_server_error(bev, events);
}



void ProxyServer::on_listen_accept(struct evconnlistener* listener,
    evutil_socket_t fd, struct sockaddr* address, int socklen) {

  if (this->client_bev.get()) {
    log(WARNING, "ignoring client connection because client already exists");
    close(fd);
    return;
  } else {
    log(INFO, "client connected");
  }

  this->client_bev.reset(bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));

  bufferevent_setcb(this->client_bev.get(),
      &ProxyServer::dispatch_on_client_input, NULL,
      &ProxyServer::dispatch_on_client_error, this);
  bufferevent_enable(this->client_bev.get(), EV_READ | EV_WRITE);

  // connect to the server, disconnecting first if needed
  this->server_bev.reset(bufferevent_socket_new(this->base.get(), -1,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));

  // TODO: figure out why this copy is necessary... shouldn't we just be able to
  // use the sockaddr_storage directly?
  const struct sockaddr_in* sin_ss = reinterpret_cast<const sockaddr_in*>(&this->next_destination);
  if (sin_ss->sin_family != AF_INET) {
    throw logic_error("ss not AF_INET");
  }
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = sin_ss->sin_port;
  sin.sin_addr.s_addr = sin_ss->sin_addr.s_addr;

  string netloc_str = render_sockaddr_storage(this->next_destination);
  log(INFO, "connecting to %s", netloc_str.c_str());
  if (bufferevent_socket_connect(this->server_bev.get(),
      reinterpret_cast<const sockaddr*>(&sin), sizeof(sin)) != 0) {
    throw runtime_error(string_printf("failed to connect (%d)", EVUTIL_SOCKET_ERROR()));
  }
  bufferevent_setcb(this->server_bev.get(),
      &ProxyServer::dispatch_on_server_input, NULL,
      &ProxyServer::dispatch_on_server_error, this);
  bufferevent_enable(this->server_bev.get(), EV_READ | EV_WRITE);
}

void ProxyServer::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), NULL);
}

void ProxyServer::on_client_input(struct bufferevent* bev) {
  this->receive_and_process_commands(false);
}

void ProxyServer::on_server_input(struct bufferevent* bev) {
  this->receive_and_process_commands(true);
}

void ProxyServer::on_client_error(struct bufferevent* bev, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "error %d (%s) in client stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "client has disconnected");
    this->client_bev.reset();
    // "forward" the disconnection to the server
    this->server_bev.reset();

    // disable encryption
    this->server_input_crypt.reset();
    this->server_output_crypt.reset();
    this->client_input_crypt.reset();
    this->client_output_crypt.reset();
  }
}

void ProxyServer::on_server_error(struct bufferevent* bev, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "error %d (%s) in server stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "server has disconnected");
    this->server_bev.reset();
    // "forward" the disconnection to the client
    this->client_bev.reset();

    // disable encryption
    this->server_input_crypt.reset();
    this->server_output_crypt.reset();
    this->client_input_crypt.reset();
    this->client_output_crypt.reset();
  }
}

void ProxyServer::receive_and_process_commands(bool from_server) {
  struct bufferevent* source_bev = from_server ? this->server_bev.get() : this->client_bev.get();
  struct bufferevent* dest_bev = from_server ? this->client_bev.get() : this->server_bev.get();

  struct evbuffer* source_buf = bufferevent_get_input(source_bev);
  struct evbuffer* dest_buf = dest_bev ? bufferevent_get_output(dest_bev) : NULL;

  PSOEncryption* source_crypt = from_server ? this->server_input_crypt.get() : this->client_input_crypt.get();
  PSOEncryption* dest_crypt = from_server ? this->client_output_crypt.get() : this->server_output_crypt.get();

  PSOCommandHeaderDCGC* input_header = from_server ? &this->server_input_header : &this->client_input_header;

  for (;;) {
    if (input_header->size == 0) {
      ssize_t bytes = evbuffer_copyout(source_buf, input_header,
          sizeof(*input_header));
      //log(INFO, "[ProxyServer-debug] %zd bytes copied for header", bytes);
      if (bytes < sizeof(*input_header)) {
        break;
      }

      //log(INFO, "[ProxyServer-debug] received encrypted header");
      //print_data(stderr, input_header, sizeof(*input_header));

      if (source_crypt) {
        source_crypt->decrypt(input_header, sizeof(*input_header));
      }
    }

    if (evbuffer_get_length(source_buf) < input_header->size) {
      //log(INFO, "[ProxyServer-debug] insufficient data for command (%zX/%hX bytes)", evbuffer_get_length(source_buf), input_header->size);
      break;
    }

    string command(input_header->size, '\0');
    ssize_t bytes = evbuffer_remove(source_buf,
        const_cast<char*>(command.data()), input_header->size);
    if (bytes < input_header->size) {
      throw logic_error("enough bytes available, but could not remove them");
    }
    //log(INFO, "[ProxyServer-debug] read command (%zX bytes)", bytes);
    // overwrite the header with the already-decrypted header
    memcpy(const_cast<char*>(command.data()), input_header,
        sizeof(*input_header));

    //log(INFO, "[ProxyServer-debug] received encrypted command with pre-decrypted header");
    //print_data(stderr, command);

    if (source_crypt) {
      source_crypt->decrypt(
          const_cast<char*>(command.data() + sizeof(*input_header)),
          input_header->size - sizeof(*input_header));
    }

    log(INFO, "%s:", from_server ? "server" : "client");
    print_data(stderr, command);

    // preprocess the command if needed
    if (from_server) {
      switch (input_header->command) {
        case 0x02: // init encryption
        case 0x17: { // init encryption
          struct InitEncryptionCommand {
            PSOCommandHeaderDCGC header;
            char copyright[0x40];
            uint32_t server_key;
            uint32_t client_key;
          };
          if (command.size() < sizeof(InitEncryptionCommand)) {
            throw std::runtime_error("init encryption command is too small");
          }

          const InitEncryptionCommand* cmd = reinterpret_cast<const InitEncryptionCommand*>(
              command.data());
          this->server_input_crypt.reset(new PSOGCEncryption(cmd->server_key));
          this->server_output_crypt.reset(new PSOGCEncryption(cmd->client_key));
          this->client_input_crypt.reset(new PSOGCEncryption(cmd->client_key));
          this->client_output_crypt.reset(new PSOGCEncryption(cmd->server_key));
          break;
        }

        case 0x19: { // reconnect
          struct ReconnectCommand {
            PSOCommandHeaderDCGC header;
            uint32_t address;
            uint16_t port;
            uint16_t unused;
          };
          if (command.size() < sizeof(ReconnectCommand)) {
            throw std::runtime_error("init encryption command is too small");
          }

          ReconnectCommand* cmd = reinterpret_cast<ReconnectCommand*>(
              const_cast<char*>(command.data()));
          memset(&this->next_destination, 0, sizeof(this->next_destination));
          struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
              &this->next_destination);
          sin->sin_family = AF_INET;
          sin->sin_port = htons(cmd->port);
          sin->sin_addr.s_addr = cmd->address; // already network byte order

          if (!dest_bev) {
            log(WARNING, "received reconnect command with no destination present");
          } else {
            struct sockaddr_storage sockname_ss;
            socklen_t len = sizeof(sockname_ss);
            getsockname(bufferevent_getfd(dest_bev),
                reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
            if (sockname_ss.ss_family != AF_INET) {
              throw logic_error("existing connection is not ipv4");
            }

            struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
                &sockname_ss);
            cmd->address = sockname_sin->sin_addr.s_addr; // already network byte order
            cmd->port = this->listen_port;
          }
          break;
        }
      }
    }

    // reencrypt and forward the command
    if (dest_buf) {
      if (dest_crypt) {
        dest_crypt->encrypt(const_cast<char*>(command.data()), command.size());
      }
      //log(INFO, "[ProxyServer-debug] sending encrypted command");
      //print_data(stderr, command);

      evbuffer_add(dest_buf, command.data(), command.size());
    } else {
      log(WARNING, "no destination present; dropping command");
    }

    // clear the input header so we can read the next command
    memset(input_header, 0, sizeof(*input_header));
  }
}
