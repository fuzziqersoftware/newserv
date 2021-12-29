#include <memory>
#include <string>

#include "Client.hh"
#include "ServerState.hh"


void process_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c);
void process_disconnect(std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c);
void process_command(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    uint16_t command, uint32_t flag, uint16_t size, const void* data);
