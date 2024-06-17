#pragma once

#include <stdint.h>

#include <string>

#include "ProxyServer.hh"
#include "ServerState.hh"

void on_proxy_command(
    std::shared_ptr<ProxyServer::LinkedSession> ses,
    bool from_server,
    uint16_t command,
    uint32_t flag,
    std::string& data);
