#include <stdint.h>

#include "PSOProtocol.hh"
#include "Client.hh"
#include "Lobby.hh"
#include "Client.hh"
#include "ServerState.hh"



void on_subcommand(std::shared_ptr<ServerState> s,
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> c, uint8_t command,
    uint8_t flag, const std::string& data);

bool subcommand_is_implemented(uint8_t which);
