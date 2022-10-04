#pragma once

#include <stddef.h>

#include <string>



std::string prs_compress(const void* vdata, size_t size);
std::string prs_compress(const std::string& data);

std::string prs_decompress(const std::string& data, size_t max_size = 0);
size_t prs_decompress_size(const std::string& data, size_t max_size = 0);

std::string bc0_decompress(const std::string& data);
