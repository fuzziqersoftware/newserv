#include "WindowsPlatform.hh"

#include "NetworkAddresses.hh"

#include <errno.h>
#include <sys/types.h>
#ifndef PHOSG_WINDOWS
#include <ifaddrs.h>
#else
#include <iphlpapi.h>
#include <ws2tcpip.h>
#endif

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <stdexcept>

using namespace std;

map<string, uint32_t> get_local_addresses() {
  map<string, uint32_t> ret;

#ifndef PHOSG_WINDOWS
  struct ifaddrs* ifa_raw;
  if (getifaddrs(&ifa_raw)) {
    auto s = phosg::string_for_error(errno);
    throw runtime_error(std::format("failed to get interface addresses: {}", s));
  }
  unique_ptr<struct ifaddrs, void (*)(struct ifaddrs*)> ifa(ifa_raw, freeifaddrs);

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

#else
  ULONG buffer_size = 0x1000;
  std::vector<char> buffer(buffer_size);

  auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
  DWORD result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &buffer_size);
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(buffer_size);
    adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &buffer_size);
  }

  if (result != NO_ERROR) {
    throw runtime_error(std::format("GetAdaptersAddresses failed: {}", result));
  }

  for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
    for (IP_ADAPTER_UNICAST_ADDRESS* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
      if (ua->Address.lpSockaddr->sa_family == AF_INET) {
        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
        ret.emplace(adapter->AdapterName, ntohl(sa_in->sin_addr.S_un.S_addr));
      }
    }
  }
#endif

  return ret;
}

bool is_loopback_address(uint32_t addr) {
  return ((addr & 0xFF000000) == 0x7F000000); // 127.0.0.0/8
}

bool is_local_address(uint32_t addr) {
  return is_loopback_address(addr) || // 127.0.0.0/8
      ((addr & 0xFF000000) == 0x0A000000) || // 10.0.0.0/8
      ((addr & 0xFFF00000) == 0xAC100000) || // 172.16.0.0/12
      ((addr & 0xFFFF0000) == 0xC0A80000) || // 192.168.0.0/16
      ((addr & 0xFFFF0000) == 0xA9FE0000); // 169.254.0.0/16
}

bool is_local_address(const sockaddr_storage& daddr) {
  if (daddr.ss_family != AF_INET) {
    return false;
  }
  const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(&daddr);
  return is_local_address(ntohl(sin->sin_addr.s_addr));
}

string string_for_address(uint32_t address) {
  return std::format("{}.{}.{}.{}",
      static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
      static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address));
}

uint32_t address_for_string(const char* address) {
  return ntohl(inet_addr(address));
}

uint64_t devolution_phone_number_for_netloc(uint32_t addr, uint16_t port) {
  // It seems the address part of the number is fixed-width, but the port is
  // not. Why did they do it this way?
  if (port & 0xF000) {
    return (static_cast<uint64_t>(addr) << 16) | port;
  } else if (port & 0x0F00) {
    return (static_cast<uint64_t>(addr) << 12) | port;
  } else if (port & 0x00F0) {
    return (static_cast<uint64_t>(addr) << 8) | port;
  } else {
    return (static_cast<uint64_t>(addr) << 4) | port;
  }
}
