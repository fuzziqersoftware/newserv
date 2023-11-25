#pragma once

#include <stdexcept>
#include <string>

inline void run_ar_code_translator(const std::string&, const std::string&, const std::string&) {
  throw std::runtime_error("resource_file is not available; install it and rebuild newserv");
}
