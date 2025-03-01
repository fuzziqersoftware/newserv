#pragma once

#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::string> decode_ppk_file(const std::string& data, const std::string& password);
