#pragma once

#include <asio.hpp>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Client.hh"
#include "Server.hh"
#include "ServerState.hh"

struct GameServerSocket : ServerSocket {
  Version version;
  ServerBehavior behavior;
};

class GameServer
    : public Server<Client, GameServerSocket>,
      public std::enable_shared_from_this<GameServer> {
public:
  GameServer() = delete;
  GameServer(const GameServer&) = delete;
  GameServer(GameServer&&) = delete;
  explicit GameServer(std::shared_ptr<ServerState> state);
  virtual ~GameServer() = default;

  void listen(const std::string& name, const std::string& addr, uint16_t port, Version version, ServerBehavior initial_state);

  std::shared_ptr<Client> connect_channel(std::shared_ptr<Channel> ch, uint16_t port, ServerBehavior initial_state);

  std::shared_ptr<Client> get_client() const;
  std::vector<std::shared_ptr<Client>> get_clients_by_identifier(const std::string& ident) const;

  inline std::shared_ptr<ServerState> get_state() const {
    return this->state;
  }

protected:
  std::shared_ptr<ServerState> state;

  asio::awaitable<void> handle_client_command(std::shared_ptr<Client> c, std::unique_ptr<Channel::Message> msg);

  [[nodiscard]] virtual std::shared_ptr<Client> create_client(
      std::shared_ptr<GameServerSocket> listen_sock, asio::ip::tcp::socket&& client_sock);
  virtual asio::awaitable<void> handle_client(std::shared_ptr<Client> c);
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<Client> c);
};
