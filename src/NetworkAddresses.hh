#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include <map>
#include <string>

// PSO is IPv4-only, so we just treat addresses as uint32_t everywhere because
// it's easier

uint32_t resolve_address(const char* address);
std::map<std::string, uint32_t> get_local_addresses();
uint32_t get_connected_address(int fd);
bool is_loopback_address(uint32_t addr);
bool is_local_address(uint32_t daddr);
bool is_local_address(const sockaddr_storage& daddr);

std::string string_for_address(uint32_t address);
uint32_t address_for_string(const char* address);

uint64_t devolution_phone_number_for_netloc(uint32_t addr, uint16_t port);
