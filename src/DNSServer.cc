#include "DNSServer.hh"

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
#include "ServerState.hh"

using namespace std;

DNSServer::DNSServer(shared_ptr<ServerState> state)
    : state(state) {}

void DNSServer::listen(const std::string& addr, int port) {
  if (port == 0) {
    throw std::runtime_error("Listening port cannot be zero");
  }
  asio::ip::address asio_addr = addr.empty() ? asio::ip::address_v4::any() : asio::ip::make_address(addr);
  asio::ip::udp::endpoint endpoint(asio_addr, port);
  auto sock = make_shared<asio::ip::udp::socket>(*this->state->io_context, endpoint);
  this->sockets.emplace(sock);

  asio::co_spawn(*this->state->io_context, this->dns_server_task(sock), asio::detached);
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

string DNSServer::response_for_query(const string& query, uint32_t resolved_address) {
  return DNSServer::response_for_query(query.data(), query.size(), resolved_address);
}

asio::awaitable<void> DNSServer::dns_server_task(std::shared_ptr<asio::ip::udp::socket> sock) {
  for (;;) {
    string input(2048, 0);
    asio::ip::udp::endpoint sender_ep;
    size_t bytes = co_await sock->async_receive_from(asio::buffer(input), sender_ep, asio::use_awaitable);
    uint32_t sender_addr = ipv4_addr_for_asio_addr(sender_ep.address());

    if (bytes < 0x0C) {
      dns_server_log.warning_f("input query too small");
      phosg::print_data(stderr, input.data(), bytes);
    } else if (!this->state->banned_ipv4_ranges->check(sender_addr)) {
      input.resize(bytes);
      uint32_t connect_address = is_local_address(sender_addr)
          ? this->state->local_address
          : this->state->external_address;
      string response = this->response_for_query(input, connect_address);
      co_await sock->async_send_to(asio::buffer(response.data(), response.size()), sender_ep, asio::use_awaitable);
    }
  }
}
