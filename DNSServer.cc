#include "DNSServer.hh"

#include <stdio.h>
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



DNSServer::DNSServer(uint32_t local_connect_address,
    uint32_t external_connect_address) :
    should_exit(false), local_connect_address(local_connect_address),
    external_connect_address(external_connect_address) { }

DNSServer::~DNSServer() {
  for (int fd : this->fds) {
    close(fd);
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
  this->fds.emplace(fd);
}

void DNSServer::start() {
  this->t = thread(&DNSServer::run_thread, this);
}

void DNSServer::schedule_stop() {
  this->should_exit = true;
}

void DNSServer::wait_for_stop() {
  this->t.join();
}

void DNSServer::run_thread() {
  vector<pollfd> poll_fds;
  for (int fd : this->fds) {
    poll_fds.emplace_back();
    auto& pfd = poll_fds.back();
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
  }

  while (!this->should_exit) {
    sockaddr_in remote;
    socklen_t remote_size = sizeof(sockaddr_in);
    memset(&remote, 0, remote_size);

    // 10 second timeout
    int num_fds = poll(poll_fds.data(), poll_fds.size(), 10000);
    if (num_fds < 0) {
      auto s = string_for_error(errno);
      log(ERROR, "DNS server terminating due to error: %s", s.c_str());
      break;
    }

    if (num_fds == 0) {
      continue;
    }

    for (const auto& pfd : poll_fds) {
      if (!(pfd.revents & POLLIN)) {
        continue;
      }

      string input(2048, 0);
      ssize_t bytes = recvfrom(pfd.fd, const_cast<char*>(input.data()),
          input.size(), 0, reinterpret_cast<sockaddr*>(&remote), &remote_size);
      if (bytes > 0) {
        input.resize(bytes);

        uint32_t remote_address = bswap32(remote.sin_addr.s_addr);
        uint32_t connect_address;
        if (is_local_address(remote_address)) {
          connect_address = this->local_connect_address;
        } else {
          connect_address = this->external_connect_address;
        }

        string output = this->build_response(input, connect_address);
        if (!output.empty()) {
          sendto(pfd.fd, output.data(), output.size(), 0,
              reinterpret_cast<sockaddr*>(&remote), remote_size);
        }
      }
    }
  }
}


string DNSServer::build_response(const std::string& input,
    uint32_t connect_address) {

  if (input.size() < 0x0C) {
    return "";
  }

  string ret;
  size_t name_len = strlen(input.data() + 0x0C) + 1;

  uint32_t connect_address_be = bswap32(connect_address);
  ret.append(input.substr(0, 2));
  ret.append("\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00", 10);
  ret.append(input.substr(12, name_len));
  ret.append("\x00\x01\x00\x01\xC0\x0C\x00\x01\x00\x01\x00\x00\x00\x3C\x00\x04", 16);
  ret.append(reinterpret_cast<const char*>(&connect_address_be), 4);

  return ret;
}
