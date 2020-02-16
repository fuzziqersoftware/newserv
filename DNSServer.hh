#pragma once

#include <event2/event.h>

#include <memory>
#include <unordered_map>
#include <string>
#include <set>


class DNSServer {
public:
  DNSServer(std::shared_ptr<struct event_base> base,
      uint32_t local_connect_address, uint32_t external_connect_address);
  DNSServer(const DNSServer&) = delete;
  DNSServer(DNSServer&&) = delete;
  virtual ~DNSServer();

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

private:
  std::shared_ptr<struct event_base> base;
  std::unordered_map<int, std::unique_ptr<struct event, void(*)(struct event*)>> fd_to_receive_event;
  uint32_t local_connect_address;
  uint32_t external_connect_address;

  static void dispatch_on_receive_message(evutil_socket_t fd, short events,
      void* ctx);
  void on_receive_message(int fd, short event);
};
