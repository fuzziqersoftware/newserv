#pragma once

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
    Difficulty difficulty = Difficulty::NORMAL,
    bool allow_v1 = false,
    std::shared_ptr<Lobby> watched_lobby = nullptr,
    std::shared_ptr<Episode3::BattleRecordPlayer> battle_player = nullptr);
void set_lobby_quest(std::shared_ptr<Lobby> l, std::shared_ptr<const Quest> q, bool substitute_v3_for_ep3 = false);
asio::awaitable<void> start_login_server_procedure(std::shared_ptr<Client> c);

asio::awaitable<void> start_proxy_session(
    std::shared_ptr<Client> c, const std::string& host, uint16_t port, bool use_persistent_config);
asio::awaitable<void> end_proxy_session(std::shared_ptr<Client> c, const std::string& error_message = "");

asio::awaitable<void> on_connect(std::shared_ptr<Client> c);
asio::awaitable<void> on_disconnect(std::shared_ptr<Client> c);

asio::awaitable<void> on_command(std::shared_ptr<Client> c, std::unique_ptr<Channel::Message> msg);
asio::awaitable<void> on_command_with_header(std::shared_ptr<Client> c, const std::string& data);
