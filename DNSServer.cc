#include "DNSServer.hh"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>

#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <vector>
#include <string>

#include "NetworkAddresses.hh"

using namespace std;



DNSServer::DNSServer(shared_ptr<struct event_base> base,
    uint32_t local_connect_address, uint32_t external_connect_address) :
    base(base), local_connect_address(local_connect_address),
    external_connect_address(external_connect_address) { }

DNSServer::~DNSServer() {
  for (const auto& it : this->fd_to_receive_event) {
    close(it.first);
  }
}

void DNSServer::listen(const std::string& socket_path) {
  this->add_socket(::listen(socket_path, 0, 0));
}

void DNSServer::listen(const std::string& addr, int port) {
  this->add_socket(::listen(addr, port, 0));
}

void DNSServer::listen(int port) {
  this->add_socket(::listen("", port, 0));
}

void DNSServer::add_socket(int fd) {
  unique_ptr<struct event, void(*)(struct event*)> e(event_new(this->base.get(),
        fd, EV_READ | EV_PERSIST, &DNSServer::dispatch_on_receive_message,
        this), event_free);
  event_add(e.get(), NULL);
  this->fd_to_receive_event.emplace(fd, move(e));
}

void DNSServer::dispatch_on_receive_message(evutil_socket_t fd,
    short events, void* ctx) {
  reinterpret_cast<DNSServer*>(ctx)->on_receive_message(fd, events);
}

void DNSServer::on_receive_message(int fd, short event) {
  for (;;) {
    sockaddr_in remote;
    socklen_t remote_size = sizeof(sockaddr_in);
    memset(&remote, 0, remote_size);

    string input(2048, 0);
    ssize_t bytes = recvfrom(fd, const_cast<char*>(input.data()), input.size(),
        0, reinterpret_cast<sockaddr*>(&remote), &remote_size);

    if (bytes < 0) {
      if (errno != EAGAIN) {
        log(INFO, "[DNSServer] input error %d", errno);
        throw runtime_error("cannot read from udp socket");
      }
      break;

    } else if (bytes == 0) {
      break;

    } else { // bytes > 0
      input.resize(bytes);

      uint32_t remote_address = bswap32(remote.sin_addr.s_addr);
      uint32_t connect_address;
      if (is_local_address(remote_address)) {
        connect_address = this->local_connect_address;
      } else {
        connect_address = this->external_connect_address;
      }

      if (input.size() >= 0x0C) {
        string response;
        size_t name_len = strlen(input.data() + 0x0C) + 1;

        uint32_t connect_address_be = bswap32(connect_address);
        response.append(input.substr(0, 2));
        response.append("\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00", 10);
        response.append(input.substr(12, name_len));
        response.append("\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x3C\x00\x04", 16);
        response.append(reinterpret_cast<const char*>(&connect_address_be), 4);

        sendto(fd, response.data(), response.size(), 0,
            reinterpret_cast<const sockaddr*>(&remote), remote_size);
      } else {
        log(WARNING, "[DNSServer] input query too small");
        print_data(stderr, input);
      }
    }
  }
}
