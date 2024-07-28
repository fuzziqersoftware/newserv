#include "DNSServer.hh"

#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <string>
#include <vector>

#include "Loggers.hh"
#include "NetworkAddresses.hh"

using namespace std;

DNSServer::DNSServer(
    shared_ptr<struct event_base> base,
    uint32_t local_connect_address,
    uint32_t external_connect_address,
    shared_ptr<const IPV4RangeSet> banned_ipv4_ranges)
    : base(base),
      local_connect_address(local_connect_address),
      external_connect_address(external_connect_address),
      banned_ipv4_ranges(banned_ipv4_ranges) {}

DNSServer::~DNSServer() {
  for (const auto& it : this->fd_to_receive_event) {
    close(it.first);
  }
}

void DNSServer::listen(const std::string& socket_path) {
  this->add_socket(phosg::listen(socket_path, 0, 0));
}

void DNSServer::listen(const std::string& addr, int port) {
  this->add_socket(phosg::listen(addr, port, 0));
}

void DNSServer::listen(int port) {
  this->add_socket(phosg::listen("", port, 0));
}

void DNSServer::add_socket(int fd) {
  unique_ptr<struct event, void (*)(struct event*)> e(
      event_new(this->base.get(), fd, EV_READ | EV_PERSIST, &DNSServer::dispatch_on_receive_message, this),
      event_free);
  event_add(e.get(), nullptr);
  this->fd_to_receive_event.emplace(fd, std::move(e));
}

void DNSServer::dispatch_on_receive_message(evutil_socket_t fd,
    short events, void* ctx) {
  reinterpret_cast<DNSServer*>(ctx)->on_receive_message(fd, events);
}

string DNSServer::response_for_query(const void* vdata, size_t size, uint32_t resolved_address) {
  if (size < 0x0C) {
    throw invalid_argument("query too small");
  }

  const char* data = reinterpret_cast<const char*>(vdata);
  size_t name_len = strlen(&data[12]) + 1;

  phosg::be_uint32_t be_resolved_address = resolved_address;

  string response;
  response.append(data, 2);
  response.append("\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00", 10);
  response.append(&data[12], name_len);
  response.append("\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x3C\x00\x04", 16);
  response.append(reinterpret_cast<const char*>(&be_resolved_address), 4);
  return response;
}

string DNSServer::response_for_query(
    const string& query, uint32_t resolved_address) {
  return DNSServer::response_for_query(query.data(), query.size(), resolved_address);
}

void DNSServer::on_receive_message(int fd, short) {
  for (;;) {
    struct sockaddr_storage remote;
    socklen_t remote_size = sizeof(sockaddr_in);
    memset(&remote, 0, remote_size);

    string input(2048, 0);
    ssize_t bytes = recvfrom(fd, const_cast<char*>(input.data()), input.size(),
        0, reinterpret_cast<sockaddr*>(&remote), &remote_size);

    if (bytes < 0) {
      if (errno != EAGAIN) {
        dns_server_log.error("input error %d", errno);
        throw runtime_error("cannot read from udp socket");
      }
      break;

    } else if (bytes == 0) {
      break;

    } else if (bytes < 0x0C) {
      dns_server_log.warning("input query too small");
      phosg::print_data(stderr, input.data(), bytes);

    } else if (!this->banned_ipv4_ranges->check(remote)) {
      input.resize(bytes);
      const sockaddr_in* remote_sin = reinterpret_cast<const sockaddr_in*>(&remote);
      uint32_t remote_address = ntohl(remote_sin->sin_addr.s_addr);
      uint32_t connect_address = is_local_address(remote_address)
          ? this->local_connect_address
          : this->external_connect_address;
      string response = this->response_for_query(input, connect_address);
      sendto(fd, response.data(), response.size(), 0,
          reinterpret_cast<const sockaddr*>(&remote), remote_size);
    }
  }
}
