#pragma once

#include <event2/event.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "IPV4RangeSet.hh"

class DNSServer {
public:
  DNSServer(
      std::shared_ptr<struct event_base> base,
      uint32_t local_connect_address,
      uint32_t external_connect_address,
      std::shared_ptr<const IPV4RangeSet> banned_ipv4_ranges);
  DNSServer(const DNSServer&) = delete;
  DNSServer(DNSServer&&) = delete;
  virtual ~DNSServer();

  inline void set_banned_ipv4_ranges(std::shared_ptr<const IPV4RangeSet> banned_ipv4_ranges) {
    this->banned_ipv4_ranges = banned_ipv4_ranges;
  }

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

  static std::string response_for_query(const void* vdata, size_t size, uint32_t resolved_address);
  static std::string response_for_query(const std::string& query, uint32_t resolved_address);

private:
  std::shared_ptr<struct event_base> base;
  std::unordered_map<int, std::unique_ptr<struct event, void (*)(struct event*)>> fd_to_receive_event;
  uint32_t local_connect_address;
  uint32_t external_connect_address;
  std::shared_ptr<const IPV4RangeSet> banned_ipv4_ranges;

  static void dispatch_on_receive_message(evutil_socket_t fd, short events, void* ctx);
  void on_receive_message(int fd, short event);
};
