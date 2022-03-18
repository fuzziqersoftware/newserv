#include "ProxyShell.hh"

#include <event2/event.h>
#include <stdio.h>

#include <phosg/Strings.hh>

using namespace std;



ProxyShell::ProxyShell(std::shared_ptr<struct event_base> base,
    std::shared_ptr<ServerState> state,
    std::shared_ptr<ProxyServer> proxy_server) : Shell(base, state),
    proxy_server(proxy_server) { }

void ProxyShell::execute_command(const string& command) {
  // find the entry in the command table and run the command
  size_t command_end = skip_non_whitespace(command, 0);
  size_t args_begin = skip_whitespace(command, command_end);
  string command_name = command.substr(0, command_end);
  string command_args = command.substr(args_begin);

  if (command_name == "exit") {
    throw exit_shell();

  } else if (command_name == "help") {
    fprintf(stderr, "\
Commands:\n\
  help\n\
    You\'re reading it now.\n\
  exit (or ctrl+d)\n\
    Shut down the proxy.\n\
  sc <data>\n\
    Send a command to the client.\n\
  ss <data>\n\
    Send a command to the server.\n\
  chat <text>\n\
    Send a chat message to the server.\n\
  dchat <data>\n\
    Send a chat message to the server with arbitrary data in it.\n\
  info-board <text>\n\
    Set your info board contents.\n\
  info-board-data <data>\n\
    Set your info board contents with arbitrary data.\n\
  marker <color-id>\n\
    Send a lobby marker message to the server.\n\
  event <event-id>\n\
    Send a lobby event update to yourself.\n\
  ship\n\
    Request the ship select menu from the server.\n\
");

  } else if ((command_name == "sc") || (command_name == "ss")) {
    bool to_client = (command_name[1] == 'c');
    string data = parse_data_string(command_args);
    if (data.size() & 3) {
      throw invalid_argument("data size is not a multiple of 4");
    }
    if (data.size() == 0) {
      throw invalid_argument("no data given");
    }
    uint16_t* size_field = reinterpret_cast<uint16_t*>(data.data() + 2);
    *size_field = data.size();

    log(INFO, "%s (from proxy):", to_client ? "server" : "client");
    print_data(stderr, data);

    if (to_client) {
      this->proxy_server->send_to_client(data);
    } else {
      this->proxy_server->send_to_server(data);
    }

  } else if ((command_name == "chat") || (command_name == "dchat")) {
    string data(12, '\0');
    data[0] = 0x06;
    data.push_back('\x09');
    data.push_back('E');
    if (command_name == "dchat") {
      data += parse_data_string(command_args);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));
    uint16_t* size_field = reinterpret_cast<uint16_t*>(data.data() + 2);
    *size_field = data.size();

    log(INFO, "Client (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_server(data);

  } else if (command_name == "marker") {
    string data("\x89\x00\x04\x00", 4);
    data[1] = stod(command_args);

    log(INFO, "Client (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_server(data);

  } else if (command_name == "event") {
    string data("\xDA\x00\x04\x00", 4);
    data[1] = stod(command_args);

    log(INFO, "Server (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_client(data);

  } else if (command_name == "ship") {
    static const string data("\xA0\x00\x04\x00", 4);

    log(INFO, "Server (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_server(data);

  } else if ((command_name == "info-board") || (command_name == "info-board-data")) {
    string data(4, '\0');
    data[0] = 0xD9;
    if (command_name == "info-board-data") {
      data += parse_data_string(command_args);
    } else {
      data += command_args;
    }
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));
    uint16_t* size_field = reinterpret_cast<uint16_t*>(data.data() + 2);
    *size_field = data.size();

    log(INFO, "Client (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_server(data);

  } else if (command_name == "set-save-quests") {
    if (command_args == "on") {
      this->proxy_server->set_save_quests(true);
    } else if (command_args == "off") {
      this->proxy_server->set_save_quests(false);
    } else {
      throw invalid_argument("argument must be \"on\" or \"off\"");
    }

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
