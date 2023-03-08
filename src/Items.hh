#pragma once

#include <stdint.h>

#include <memory>
#include <random>

#include "Client.hh"
#include "StaticGameData.hh"



void player_use_item(std::shared_ptr<Client> c, size_t item_index);
