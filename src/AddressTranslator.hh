#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

struct DiffEntry {
  uint32_t address;
  std::string a_data;
  std::string b_data;
};

void run_address_translator(const std::string& directory, const std::string& use_filename, const std::string& command);
std::vector<DiffEntry> diff_dol_files(const std::string& a_filename, const std::string& b_filename);
std::vector<DiffEntry> diff_xbe_files(const std::string& a_filename, const std::string& b_filename);
