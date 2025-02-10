#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct DiffEntry {
  uint32_t address;
  std::string a_data;
  std::string b_data;
};

inline void run_address_translator(const std::string&, const std::string&, const std::string&) {
  throw std::runtime_error("resource_file is not available; install it and rebuild newserv");
}

inline std::vector<DiffEntry> diff_dol_files(const std::string&, const std::string&) {
  throw std::runtime_error("resource_file is not available; install it and rebuild newserv");
}
