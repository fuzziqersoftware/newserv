#pragma once

#include <event2/event.h>

#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <thread>

#include "Client.hh"
#include "ServerState.hh"



class Server {
private:
  struct WorkerThread {
    Server* server;
    int worker_num;
    std::unique_ptr<struct event_base, void(*)(struct event_base*)> base;
    std::unordered_set<std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)>> listeners;
    std::unordered_map<struct bufferevent*, std::shared_ptr<Client>> bev_to_client;
    std::thread t;
    std::string thread_name;

    WorkerThread(Server* server, int worker_num);

    void disconnect_client(struct bufferevent* bev);

    static void dispatch_on_listen_accept(struct evconnlistener* listener,
        evutil_socket_t fd, struct sockaddr *address, int socklen, void* ctx);
    static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
    static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_client_error(struct bufferevent* bev, short events,
        void* ctx);
    static void dispatch_check_for_thread_exit(evutil_socket_t fd, short what, void* ctx);
  };

  std::atomic<bool> should_exit;
  std::vector<WorkerThread> threads;

  std::atomic<size_t> client_count;
  std::unordered_map<int, std::pair<GameVersion, ServerBehavior>> listen_fd_to_version_and_state;
  std::shared_ptr<ServerState> state;

  void on_listen_accept(WorkerThread& wt, struct evconnlistener *listener,
      evutil_socket_t fd, struct sockaddr *address, int socklen);
  void on_listen_error(WorkerThread& wt, struct evconnlistener *listener);
  void on_client_input(WorkerThread& wt, struct bufferevent *bev);
  void on_client_error(WorkerThread& wt, struct bufferevent *bev, short events);
  void check_for_thread_exit(WorkerThread& wt, evutil_socket_t fd, short what);

  void receive_and_process_commands(std::shared_ptr<Client> c, struct bufferevent* buf);

  void process_client_connect(std::shared_ptr<Client> c);
  void process_client_disconnect(std::shared_ptr<Client> c);
  void process_client_command(std::shared_ptr<Client> c, const std::string& command);

  void run_thread(int thread_id);

public:
  Server() = delete;
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server(std::shared_ptr<ServerState> state);
  virtual ~Server() = default;

  void listen(const std::string& socket_path, GameVersion version, ServerBehavior initial_state);
  void listen(const std::string& addr, int port, GameVersion version, ServerBehavior initial_state);
  void listen(int port, GameVersion version, ServerBehavior initial_state);
  void add_socket(int fd, GameVersion version, ServerBehavior initial_state);

  virtual void start();
  virtual void schedule_stop();
  virtual void wait_for_stop();
};
