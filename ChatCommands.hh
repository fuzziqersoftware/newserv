#pragma once

#include <memory>

#include "ServerState.hh"
#include "Lobby.hh"
#include "Client.hh"

void process_chat_command(std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, const char16_t* text);
