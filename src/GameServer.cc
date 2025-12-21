#include "GameServer.hh"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"

using namespace std;
using namespace std::placeholders;

GameServer::GameServer(shared_ptr<ServerState> state) : Server(state->io_context, "[GameServer] "), state(state) {}

void GameServer::listen(
    const std::string& name,
    const string& addr,
    uint16_t port,
    Version version,
    ServerBehavior behavior) {
  if (port == 0) {
    throw std::runtime_error("Listening port cannot be zero");
  }

  asio::ip::address asio_addr = addr.empty() ? asio::ip::address_v4::any() : asio::ip::make_address(addr);
  auto sock = make_shared<GameServerSocket>();
  sock->name = name;
  sock->endpoint = asio::ip::tcp::endpoint(asio_addr, port);
  sock->version = version;
  sock->behavior = behavior;
  this->add_socket(std::move(sock));
}

shared_ptr<Client> GameServer::connect_channel(shared_ptr<Channel> ch, uint16_t port, ServerBehavior initial_state) {
  auto c = make_shared<Client>(this->shared_from_this(), ch, initial_state);

  this->log.info_f("Client connected: C-{:X} via TSI-{}-{}-{}",
      c->id, port, phosg::name_for_enum(ch->version), phosg::name_for_enum(initial_state));

  asio::co_spawn(*this->io_context, this->handle_connected_client(c), asio::detached);
  return c;
}

shared_ptr<Client> GameServer::get_client() const {
  if (this->clients.empty()) {
    throw runtime_error("no clients on game server");
  }
  if (this->clients.size() > 1) {
    throw runtime_error("multiple clients on game server");
  }
  return *this->clients.begin();
}

vector<shared_ptr<Client>> GameServer::get_clients_by_identifier(const string& ident) const {
  int64_t account_id_hex = -1;
  int64_t account_id_dec = -1;
  try {
    account_id_dec = stoul(ident, nullptr, 10);
  } catch (const invalid_argument&) {
  }
  try {
    account_id_hex = stoul(ident, nullptr, 16);
  } catch (const invalid_argument&) {
  }

  // TODO: It's kind of not great that we do a linear search here, but this is only used in the shell, so it should be
  // pretty rare.
  vector<shared_ptr<Client>> results;
  for (const auto& c : this->clients) {
    if (c->login && c->login->account->account_id == account_id_hex) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->account->account_id == account_id_dec) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->xb_license && c->login->xb_license->gamertag == ident) {
      results.emplace_back(c);
      continue;
    }
    if (c->login && c->login->bb_license && c->login->bb_license->username == ident) {
      results.emplace_back(c);
      continue;
    }

    auto p = c->character_file(false, false);
    if (p && p->disp.name.eq(ident, p->inventory.language)) {
      results.emplace_back(c);
      continue;
    }

    if (c->channel->name == ident) {
      results.emplace_back(c);
      continue;
    }
    if (c->channel->name.starts_with(ident + " ")) {
      results.emplace_back(c);
      continue;
    }
  }

  return results;
}

shared_ptr<Client> GameServer::create_client(
    shared_ptr<GameServerSocket> listen_sock, asio::ip::tcp::socket&& client_sock) {
  uint32_t addr = ipv4_addr_for_asio_addr(client_sock.remote_endpoint().address());
  if (this->state->banned_ipv4_ranges->check(addr)) {
    if (client_sock.is_open()) {
      client_sock.close();
    }
    return nullptr;
  }

  auto channel = SocketChannel::create(
      this->io_context,
      make_unique<asio::ip::tcp::socket>(std::move(client_sock)),
      listen_sock->version,
      Language::ENGLISH,
      "",
      phosg::TerminalFormat::FG_YELLOW,
      phosg::TerminalFormat::FG_GREEN);
  auto c = make_shared<Client>(this->shared_from_this(), channel, listen_sock->behavior);
  this->log.info_f("Client connected: C-{:X} via {}", c->id, listen_sock->name);

  return c;
}

asio::awaitable<void> GameServer::handle_client_command(shared_ptr<Client> c, unique_ptr<Channel::Message> msg) {
  try {
    co_await on_command(c, std::move(msg));
  } catch (const exception& e) {
    this->log.warning_f("Error processing client command: {}", e.what());
    c->channel->disconnect();
  }
}

asio::awaitable<void> GameServer::handle_client(shared_ptr<Client> c) {
  auto g = phosg::on_close_scope(std::bind(&Client::cancel_pending_promises, c.get()));

  try {
    co_await on_connect(c);
  } catch (const exception& e) {
    this->log.warning_f("Error in client initialization: {}", e.what());
    c->channel->disconnect();
  }

  while (c->channel->connected()) {
    auto msg = std::make_unique<Channel::Message>(co_await c->channel->recv());
    asio::co_spawn(co_await asio::this_coro::executor, this->handle_client_command(c, std::move(msg)), asio::detached);
  }
}

asio::awaitable<void> GameServer::destroy_client(std::shared_ptr<Client> c) {
  this->log.info_f("Running cleanup tasks for {}", c->channel->name);

  // The client may not actually be disconnected yet if an uncaught exception occurred in a handler task
  c->channel->disconnect();

  // Close the proxy session, if any
  if (c->proxy_session) {
    if (c->proxy_session->server_channel) {
      c->proxy_session->server_channel->disconnect();
    }
    c->proxy_session.reset();
  }

  try {
    co_await on_disconnect(c);
  } catch (const exception& e) {
    this->log.warning_f("Error during client disconnect cleanup: {}", e.what());
  }

  // Note: It's important to move the disconnect hooks out of the client here because the hooks could modify
  // c->disconnect_hooks while it's being iterated here, which would invalidate these iterators.
  unordered_map<string, function<void()>> hooks = std::move(c->disconnect_hooks);
  for (auto h_it : hooks) {
    try {
      h_it.second();
    } catch (const exception& e) {
      c->log.warning_f("Disconnect hook {} failed: {}", h_it.first, e.what());
    }
  }
}
