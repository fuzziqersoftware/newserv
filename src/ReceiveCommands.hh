#include <memory>
#include <string>

#include "Client.hh"
#include "ServerState.hh"

std::shared_ptr<Lobby> create_game_generic(
    std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c,
    const std::string& name,
    const std::string& password = "",
    Episode episode = Episode::EP1,
    GameMode mode = GameMode::NORMAL,
    uint8_t difficulty = 0,
    uint32_t flags = 0,
    bool allow_v1 = false,
    std::shared_ptr<Lobby> watched_lobby = nullptr,
    std::shared_ptr<Episode3::BattleRecordPlayer> battle_player = nullptr);

void on_connect(std::shared_ptr<Client> c);
void on_disconnect(std::shared_ptr<Client> c);
void on_command(std::shared_ptr<Client> c, uint16_t command, uint32_t flag, std::string& data);
void on_command_with_header(std::shared_ptr<Client> c, const std::string& data);
