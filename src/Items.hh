#pragma once

#include <stdint.h>

#include <memory>
#include <random>

#include "Client.hh"
#include "ItemData.hh"
#include "ItemParameterTable.hh"
#include "PSOEncryption.hh"
#include "ServerState.hh"
#include "StaticGameData.hh"

void player_use_item(std::shared_ptr<Client> c, size_t item_index, std::shared_ptr<PSOLFGEncryption> opt_rand_crypt);
void player_feed_mag(std::shared_ptr<Client> c, size_t mag_item_index, size_t fed_item_index);

void apply_mag_feed_result(
    ItemData& mag_item,
    const ItemData& fed_item,
    std::shared_ptr<const ItemParameterTable> item_parameter_table,
    std::shared_ptr<const MagEvolutionTable> mag_evolution_table,
    uint8_t char_class,
    uint8_t section_id,
    bool version_has_rare_mags);
