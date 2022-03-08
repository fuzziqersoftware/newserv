#include "NetworkAddresses.hh"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <stdexcept>

using namespace std;



uint32_t resolve_address(const char* address) {
  struct addrinfo *res0;
  if (getaddrinfo(address, nullptr, nullptr, &res0)) {
    auto e = string_for_error(errno);
    throw runtime_error(string_printf("can\'t resolve hostname %s: %s", address,
        e.c_str()));
  }

  std::unique_ptr<struct addrinfo, void(*)(struct addrinfo*)> res0_unique(
      res0, freeaddrinfo);
  struct addrinfo *res4 = nullptr;
  for (struct addrinfo* res = res0; res; res = res->ai_next) {
    if (res->ai_family == AF_INET) {
      res4 = res;
    }
  }
  if (!res4) {
    throw runtime_error(string_printf(
        "can\'t resolve hostname %s: no usable data", address));
  }

  struct sockaddr_in* res_sin = (struct sockaddr_in*)res4->ai_addr;
  return ntohl(res_sin->sin_addr.s_addr);
}

map<string, uint32_t> get_local_addresses() {
  struct ifaddrs* ifa_raw;
  if (getifaddrs(&ifa_raw)) {
    auto s = string_for_error(errno);
    throw runtime_error(string_printf("failed to get interface addresses: %s", s.c_str()));
  }

  unique_ptr<struct ifaddrs, void(*)(struct ifaddrs*)> ifa(ifa_raw, freeifaddrs);

  map<string, uint32_t> ret;
  for (struct ifaddrs* i = ifa.get(); i; i = i->ifa_next) {
    if (!i->ifa_addr) {
      continue;
    }

    auto* sin = reinterpret_cast<sockaddr_in*>(i->ifa_addr);
    if (sin->sin_family != AF_INET) {
      continue;
    }

    ret.emplace(i->ifa_name, ntohl(sin->sin_addr.s_addr));
  }

  return ret;
}

bool is_local_address(uint32_t addr) {
  uint8_t net = (addr >> 24) & 0xFF;
  if ((net != 127) && (net != 172) && (net != 10) && (net != 192)) {
    return false;
  }
  return true;
}

bool is_local_address(const sockaddr_storage& daddr) {
  if (daddr.ss_family != AF_INET) {
    return false;
  }
  const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(&daddr);
  return is_local_address(ntohl(sin->sin_addr.s_addr));
}

string string_for_address(uint32_t address) {
  return string_printf("%hhu.%hhu.%hhu.%hhu",
      static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
      static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address));
}

uint32_t address_for_string(const char* address) {
  return ntohl(inet_addr(address));
}
