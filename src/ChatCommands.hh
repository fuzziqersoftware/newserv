#pragma once

#include <stdint.h>

#include <memory>
#include <string>

#include "ServerState.hh"
#include "Lobby.hh"
#include "Client.hh"
#include "ProxyServer.hh"

void on_chat_command(std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, const std::u16string& text);
void on_chat_command(std::shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, const std::u16string& text);
