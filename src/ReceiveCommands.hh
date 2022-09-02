#include <memory>
#include <string>

#include "Client.hh"
#include "ServerState.hh"



void on_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c);
void on_disconnect(std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c);
void on_command(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const std::string& data);
