#pragma once

#include <stdint.h>

#include "Client.hh"
#include "Lobby.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

void on_subcommand_multi(std::shared_ptr<Client> c, uint8_t command, uint8_t flag, std::string& data);
bool subcommand_is_implemented(uint8_t which);

void send_item_notification_if_needed(
    std::shared_ptr<ServerState> s,
    Channel& ch,
    const Client::Config& config,
    const ItemData& item,
    bool is_from_rare_table);
