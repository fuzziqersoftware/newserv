#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>

// dc_serial_number_is_valid_slow is Sega's implementation;
// dc_serial_number_is_valid_fast produces identical results but is between 3000
// and 7500 times faster, depending on the compiler's optimization level.
bool dc_serial_number_is_valid_slow(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
bool dc_serial_number_is_valid_fast(
    const std::string& s, uint8_t domain, uint8_t subdomain = 0xFF);
bool dc_serial_number_is_valid_fast(
    uint32_t serial_number, uint8_t domain, uint8_t subdomain = 0xFF);
bool decoded_dc_serial_number_is_valid_fast(
    uint32_t serial_number, uint8_t domain, uint8_t subdomain = 0xFF);

std::string generate_dc_serial_number(uint8_t domain, uint8_t subdomain = 0xFF);
std::unordered_map<uint32_t, std::string> generate_all_dc_serial_numbers(uint8_t domain = 0xFF, uint8_t subdomain = 0xFF);

void dc_serial_number_speed_test(uint64_t seed = 0xFFFFFFFFFFFFFFFF);

struct EncryptedDCv2Executables {
  std::string executable;
  std::string indexes;
};

std::string decrypt_dp_address_jpn(
    const std::string& dp_address_jpn_data,
    const std::string& iwashi_sea_data,
    const std::string& katsuo_sea_data);
EncryptedDCv2Executables encrypt_dp_address_jpn(const std::string& executable, const std::string& indexes);

std::string crypt_dp_address_jpn_simple(const std::string& data, int64_t seed = -1);
