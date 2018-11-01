#pragma once

#include <atomic>
#include <thread>
#include <string>
#include <set>


class DNSServer {
public:
  DNSServer(uint32_t local_connect_address, uint32_t external_connect_address);
  DNSServer(const DNSServer&) = delete;
  DNSServer(DNSServer&&) = delete;
  virtual ~DNSServer();

  void listen(const std::string& socket_path);
  void listen(const std::string& addr, int port);
  void listen(int port);
  void add_socket(int fd);

  virtual void start();
  virtual void schedule_stop();
  virtual void wait_for_stop();

private:
  std::atomic<bool> should_exit;
  std::thread t;

  std::set<int> fds;

  uint32_t local_connect_address;
  uint32_t external_connect_address;

  void run_thread();
  static std::string build_response(const std::string& input,
      uint32_t connect_address);
};
