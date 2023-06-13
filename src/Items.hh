#pragma once

#include <stdint.h>

#include <memory>
#include <random>

#include "Client.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

void player_use_item(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c, size_t item_index);
void player_feed_mag(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c, size_t mag_item_index, size_t fed_item_index);
