#include <stdint.h>

#include "PSOProtocol.hh"
#include "Client.hh"
#include "Lobby.hh"
#include "Client.hh"
#include "ServerState.hh"


void check_size(uint16_t size, uint16_t min_size, uint16_t max_size = 0);

void process_subcommand(std::shared_ptr<ServerState> s,
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> c, uint8_t command,
    uint8_t flag, const PSOSubcommand* sub, size_t count);

bool subcommand_is_implemented(uint8_t which);
