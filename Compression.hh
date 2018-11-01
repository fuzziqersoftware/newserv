#pragma once

#include <stddef.h>

#include <string>



std::string prs_compress(const std::string& data);
std::string prs_decompress(const std::string& data, size_t max_size = 0);
