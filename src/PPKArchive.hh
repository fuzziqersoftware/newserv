#pragma once

#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::string> decode_ppk_file(std::string_view data, std::string_view password);
