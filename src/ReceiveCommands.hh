#include <memory>
#include <string>

#include "Client.hh"
#include "ServerState.hh"

std::shared_ptr<Lobby> create_game_generic(
    std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c,
    const std::u16string& name,
    const std::u16string& password,
    Episode episode,
    GameMode mode,
    uint8_t difficulty,
    uint32_t flags,
    std::shared_ptr<Lobby> watched_lobby = nullptr,
    std::shared_ptr<Episode3::BattleRecordPlayer> battle_player = nullptr);

void on_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c);
void on_disconnect(std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c);
void on_command(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const std::string& data);
void on_command_with_header(std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c, std::string& data);
