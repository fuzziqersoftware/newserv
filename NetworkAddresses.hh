#pragma once

#include <stdint.h>

#include <set>



// PSO is IPv4-only, so we just treat addresses as uint32_t everywhere because
// it's easier

uint32_t resolve_address(const char* address);
std::set<uint32_t> get_local_address_list();
uint32_t get_connected_address(int fd);
bool is_local_address(uint32_t daddr);
