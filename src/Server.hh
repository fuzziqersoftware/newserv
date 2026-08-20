#pragma once

#include <asio.hpp>
#include <memory>
#include <phosg/Network.hh>
#include <string>
#include <unordered_set>
#include <vector>

#include "Loggers.hh"

struct ServerSocket {
  std::string name;
  asio::ip::tcp::endpoint endpoint;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
};

template <typename ClientT, typename SocketT = ServerSocket>
class Server {
public:
  phosg::PrefixedLogger log;

  Server() = delete;
  Server(std::shared_ptr<asio::io_context> io_context, const std::string& log_prefix)
      : log(log_prefix, server_log.min_level), io_context(io_context) {}
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) = delete;
  virtual ~Server() = default;

  // Generally subclasses will implement listen(), which should create a SocketT object (of their desired type) with a
  // valid endpoint and call add_socket to actually listen on that endpoint
  void add_socket(std::shared_ptr<SocketT> sock) {
    sock->acceptor = std::make_unique<asio::ip::tcp::acceptor>(*this->io_context, sock->endpoint);
    asio::co_spawn(*this->io_context, this->accept_connections(sock), asio::detached);

    asio::ip::tcp::endpoint sock_addr = sock->acceptor->local_endpoint();
    std::string addr_str = sock_addr.address().to_string();
    this->log.info_f("Listening on {}:{} as {}", addr_str, sock_addr.port(), sock->name);

    this->sockets.emplace(std::move(sock));
  }

  inline std::shared_ptr<asio::io_context> get_io_context() const {
    return this->io_context;
  }

  inline const std::unordered_set<std::shared_ptr<ClientT>> all_clients() const {
    return this->clients;
  }

protected:
  std::shared_ptr<asio::io_context> io_context;
  std::unordered_set<std::shared_ptr<SocketT>> sockets;
  std::unordered_set<std::shared_ptr<ClientT>> clients;

  // create_client is called when a new socket is opened. It should create (and return) the ClientT object, or may
  // close client_sock and return nullptr if it decides to reject the connection. create_client should NOT send or
  // receive any data, hence it is not a coroutine.
  [[nodiscard]] virtual std::shared_ptr<ClientT> create_client(
      std::shared_ptr<SocketT> sock, asio::ip::tcp::socket&& client_sock) = 0;
  // handle_client is called immediately after create_client if create_client did not return nullptr. It should handle
  // all sending and receiving of data on the client's connection.
  virtual asio::awaitable<void> handle_client(std::shared_ptr<ClientT> c) = 0;
  // destroy_client is called when the client is about to be destroyed, often after it has disconnected (hence, it
  // cannot assume that it can send or receive any data). Additionally, the client has already been removed from
  // this->clients at the time this is called.
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<ClientT> c) {
    (void)c;
    co_return;
  }

  asio::awaitable<void> handle_socket_client(std::shared_ptr<SocketT> listen_sock, asio::ip::tcp::socket&& client_sock) {
    std::shared_ptr<ClientT> c;
    try {
      c = this->create_client(listen_sock, std::move(client_sock));
    } catch (const std::exception& e) {
      this->log.warning_f("Error creating client: {}", e.what());
    }
    if (!c) {
      co_return;
    }
    co_await this->handle_connected_client(c);
  }

  asio::awaitable<void> handle_connected_client(std::shared_ptr<ClientT> c) {
    try {
      this->clients.emplace(c);
      co_await this->handle_client(c);
    } catch (const std::system_error& e) {
      const auto& ec = e.code();
      if (ec == asio::error::eof || ec == asio::error::connection_reset) {
        this->log.info_f("Client has disconnected");
      } else if (ec == asio::error::operation_aborted) {
        this->log.info_f("Client task was cancelled");
      } else {
        this->log.warning_f("Error in client task: {}", e.what());
      }
    } catch (const std::exception& e) {
      this->log.warning_f("Error in client task: {}", e.what());
    }
    if (c) {
      try {
        this->clients.erase(c);
        co_await this->destroy_client(c);
      } catch (const std::exception& e) {
        this->log.warning_f("Error in client cleanup task: {}", e.what());
      }
    }
  }

  asio::awaitable<void> accept_connections(std::shared_ptr<SocketT> sock) {
    for (;;) {
      auto client_sock = co_await sock->acceptor->async_accept(asio::use_awaitable);
      asio::co_spawn(*this->io_context, this->handle_socket_client(sock, std::move(client_sock)), asio::detached);
    }
  }
};
