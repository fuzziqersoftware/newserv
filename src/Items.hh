#pragma once

#include <stdint.h>

#include <memory>
#include <random>

#include "Client.hh"
#include "PSOEncryption.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

void player_use_item(std::shared_ptr<Client> c, size_t item_index, std::shared_ptr<PSOLFGEncryption> random_crypt);
void player_feed_mag(std::shared_ptr<Client> c, size_t mag_item_index, size_t fed_item_index);
