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
commands:\n\
  help\n\
    you\'re reading it now\n\
  exit (or ctrl+d)\n\
    shut down the proxy\n\
  sc <data>\n\
    send a command to the client\n\
  ss <data>\n\
    send a command to the server\n\
  chat <text>\n\
    send a chat message to the server\n\
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
    uint16_t* size_field = reinterpret_cast<uint16_t*>(const_cast<char*>(data.data() + 2));
    *size_field = data.size();

    log(INFO, "%s (from proxy):", to_client ? "server" : "client");
    print_data(stderr, data);

    if (to_client) {
      this->proxy_server->send_to_client(data);
    } else {
      this->proxy_server->send_to_server(data);
    }

  } else if (command_name == "chat") {
    string data(12, '\0');
    data[0] = 0x06;
    data.push_back('\x09');
    data.push_back('E');
    data += command_args;
    data.push_back('\0');
    data.resize((data.size() + 3) & (~3));
    uint16_t* size_field = reinterpret_cast<uint16_t*>(const_cast<char*>(data.data() + 2));
    *size_field = data.size();

    log(INFO, "client (from proxy):");
    print_data(stderr, data);
    this->proxy_server->send_to_server(data);

  } else {
    throw invalid_argument("unknown command; try \'help\'");
  }
}
