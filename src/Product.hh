#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>

bool product_is_valid(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
bool product_is_valid_fast(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
bool product_is_valid_fast(
    uint32_t product, uint8_t domain, uint8_t subdomain);
bool decoded_product_is_valid_fast(
    uint32_t product, uint8_t domain, uint8_t subdomain);

std::string generate_product(uint8_t domain, uint8_t subdomain = 0xFF);
std::unordered_map<uint32_t, std::string> generate_all_products(uint8_t domain = 0xFF, uint8_t subdomain = 0xFF);

void product_speed_test(uint64_t seed = 0xFFFFFFFFFFFFFFFF);
