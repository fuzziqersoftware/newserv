#pragma once

#include <stdint.h>

#include <string>



bool product_is_valid(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
bool product_is_valid_fast(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
std::string generate_product(uint8_t domain, uint8_t subdomain = 0xFF);

void product_speed_test(uint64_t seed = 0xFFFFFFFFFFFFFFFF);
