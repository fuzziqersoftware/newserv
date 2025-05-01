#pragma once

#include <asio.hpp>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "IPV4RangeSet.hh"

struct ServerState;

class DNSServer {
public:
  explicit DNSServer(std::shared_ptr<ServerState> state);
  DNSServer(const DNSServer&) = delete;
  DNSServer(DNSServer&&) = delete;
  DNSServer& operator=(const DNSServer&) = delete;
  DNSServer& operator=(DNSServer&&) = delete;
  virtual ~DNSServer() = default;

  void listen(const std::string& addr, int port);

  static std::string response_for_query(const void* vdata, size_t size, uint32_t resolved_address);
  static std::string response_for_query(const std::string& query, uint32_t resolved_address);

private:
  std::shared_ptr<ServerState> state;
  std::unordered_set<std::shared_ptr<asio::ip::udp::socket>> sockets;

  asio::awaitable<void> dns_server_task(std::shared_ptr<asio::ip::udp::socket> sock);
};
