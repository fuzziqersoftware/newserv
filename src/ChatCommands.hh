#pragma once

#include <stdint.h>

#include <asio.hpp>
#include <memory>
#include <string>

#include "Client.hh"
#include "Lobby.hh"
#include "ProxySession.hh"
#include "ServerState.hh"

asio::awaitable<void> on_chat_command(std::shared_ptr<Client> c, const std::string& text, bool check_permissions);
