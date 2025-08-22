#pragma once

#include <stdlib.h>

#include <memory>
#include <string>

#include "AsyncHTTPServer.hh"
#include "ServerState.hh"

class HTTPServer : public AsyncHTTPServer<> {
public:
  explicit HTTPServer(std::shared_ptr<ServerState> state);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer() = default;

  asio::awaitable<void> send_rare_drop_notification(std::shared_ptr<const phosg::JSON> message);

protected:
  std::shared_ptr<ServerState> state;
  std::unordered_set<std::shared_ptr<HTTPClient>> rare_drop_subscribers;

  std::shared_ptr<phosg::JSON> generate_server_version() const;
  std::shared_ptr<phosg::JSON> generate_account_json(std::shared_ptr<const Account> a) const;
  std::shared_ptr<phosg::JSON> generate_client_json(
      std::shared_ptr<const Client> c, std::shared_ptr<const ItemNameIndex> item_name_index) const;
  std::shared_ptr<phosg::JSON> generate_lobby_json(
      std::shared_ptr<const Lobby> l, std::shared_ptr<const ItemNameIndex> item_name_index) const;
  std::shared_ptr<phosg::JSON> generate_accounts_json() const;
  std::shared_ptr<phosg::JSON> generate_clients_json() const;
  std::shared_ptr<phosg::JSON> generate_server_info_json() const;
  std::shared_ptr<phosg::JSON> generate_lobbies_json() const;
  std::shared_ptr<phosg::JSON> generate_summary_json() const;
  std::shared_ptr<phosg::JSON> generate_all_json() const;

  asio::awaitable<std::shared_ptr<phosg::JSON>> generate_ep3_cards_json(bool trial) const;
  std::shared_ptr<phosg::JSON> generate_common_table_list_json() const;
  std::shared_ptr<phosg::JSON> generate_rare_table_list_json() const;
  asio::awaitable<std::shared_ptr<phosg::JSON>> generate_common_table_json(const std::string& table_name) const;
  asio::awaitable<std::shared_ptr<phosg::JSON>> generate_rare_table_json(const std::string& table_name) const;
  asio::awaitable<std::shared_ptr<phosg::JSON>> generate_quest_list_json(std::shared_ptr<const QuestIndex> q);

  void require_GET(const HTTPRequest& req);
  phosg::JSON require_POST(const HTTPRequest& req);

  virtual asio::awaitable<std::unique_ptr<HTTPResponse>> handle_request(std::shared_ptr<HTTPClient> c, HTTPRequest&& req);
  virtual asio::awaitable<void> destroy_client(std::shared_ptr<HTTPClient> c);
};
