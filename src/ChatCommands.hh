#pragma once

#include <stdint.h>

#include <memory>
#include <string>

#include "Client.hh"
#include "Lobby.hh"
#include "ProxyServer.hh"
#include "ServerState.hh"

void on_chat_command(std::shared_ptr<Client> c, const std::u16string& text);
void on_chat_command(std::shared_ptr<ProxyServer::LinkedSession> ses, const std::u16string& text);
