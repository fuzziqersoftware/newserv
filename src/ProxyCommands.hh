#pragma once

#include <stdint.h>

#include <string>

#include "ServerState.hh"
#include "ProxyServer.hh"



void process_proxy_command(
    std::shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    bool from_server,
    uint16_t command,
    uint32_t flag,
    std::string& data);
