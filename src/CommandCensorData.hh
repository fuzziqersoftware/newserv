#pragma once

#include <stdint.h>

#include <utility>

#include "Version.hh"

std::pair<const void*, size_t> censor_data_for_client_command(Version version, uint16_t command);
