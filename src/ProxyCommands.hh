#pragma once

#include "Client.hh"

asio::awaitable<void> on_proxy_command(std::shared_ptr<Client> c, bool from_server, std::unique_ptr<Channel::Message> msg);
asio::awaitable<void> handle_proxy_server_commands(std::shared_ptr<Client> c, std::shared_ptr<ProxySession> ses, std::shared_ptr<Channel> channel);
