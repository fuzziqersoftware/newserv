#include "ProxyServer.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"

using namespace std;



static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}



ProxyServer::ProxyServer(
    shared_ptr<struct event_base> base,
    const struct sockaddr_storage& initial_destination,
    GameVersion version)
  : base(base),
    client_bev(nullptr, flush_and_free_bufferevent),
    server_bev(nullptr, flush_and_free_bufferevent),
    next_destination(initial_destination),
    version(version),
    header_size((version == GameVersion::BB) ? 8 : 4),
    save_quests(false) {
  memset(&this->client_input_header, 0, sizeof(this->client_input_header));
  memset(&this->server_input_header, 0, sizeof(this->server_input_header));
}

void ProxyServer::listen(int port) {
  unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> listener(
      evconnlistener_new(this->base.get(),
        &ProxyServer::dispatch_on_listen_accept, this,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 0,
        ::listen("", port, SOMAXCONN)), evconnlistener_free);
  this->listeners.emplace(port, move(listener));
}

void ProxyServer::set_save_quests(bool save_quests) {
  this->save_quests = save_quests;
}



ProxyServer::SavingQuestFile::SavingQuestFile(
    const std::string& basename,
    const std::string& output_filename,
    uint32_t remaining_bytes)
  : basename(basename),
    output_filename(output_filename),
    remaining_bytes(remaining_bytes),
    f(fopen_unique(this->output_filename, "wb")) { }



void ProxyServer::send_to_client(const std::string& data) {
  this->send_to_end(data, false);
}

void ProxyServer::send_to_server(const std::string& data) {
  this->send_to_end(data, true);
}

void ProxyServer::send_to_end(const std::string& data, bool to_server) {
  struct bufferevent* bev = to_server ? this->server_bev.get() : this->client_bev.get();
  if (!bev) {
    throw runtime_error("connection not open");
  }

  struct evbuffer* buf = bufferevent_get_output(bev);

  PSOEncryption* crypt = to_server ? this->server_output_crypt.get() : this->client_output_crypt.get();
  if (crypt) {
    string crypted_data = data;
    crypt->encrypt(crypted_data.data(), crypted_data.size());
    evbuffer_add(buf, crypted_data.data(), crypted_data.size());
  } else {
    evbuffer_add(buf, data.data(), data.size());
  }
}



void ProxyServer::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_listen_accept(listener, fd, address,
      socklen);
}

void ProxyServer::dispatch_on_listen_error(struct evconnlistener* listener,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_listen_error(listener);
}

void ProxyServer::dispatch_on_client_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_client_input(bev);
}

void ProxyServer::dispatch_on_client_error(struct bufferevent* bev, short events,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_client_error(bev, events);
}

void ProxyServer::dispatch_on_server_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_server_input(bev);
}

void ProxyServer::dispatch_on_server_error(struct bufferevent* bev, short events,
    void* ctx) {
  reinterpret_cast<ProxyServer*>(ctx)->on_server_error(bev, events);
}



void ProxyServer::on_listen_accept(struct evconnlistener*, evutil_socket_t fd,
    struct sockaddr*, int) {
  if (this->client_bev.get()) {
    log(WARNING, "[ProxyServer] Ignoring client connection because client already exists");
    close(fd);
    return;
  }

  log(INFO, "[ProxyServer] Client connected on fd %d", fd);
  this->on_client_connect(bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));
}

void ProxyServer::connect_client(struct bufferevent* bev) {
  if (this->client_bev.get()) {
    log(WARNING, "[ProxyServer] Ignoring client virtual connection because client already exists");
    bufferevent_flush(bev, EV_WRITE, BEV_FINISHED);
    return;
  }

  log(INFO, "[ProxyServer] Client connected on virtual connection %p", bev);
  this->on_client_connect(bev);
}

void ProxyServer::on_client_connect(struct bufferevent* bev) {
  this->client_bev.reset(bev);

  bufferevent_setcb(this->client_bev.get(),
      &ProxyServer::dispatch_on_client_input, nullptr,
      &ProxyServer::dispatch_on_client_error, this);
  bufferevent_enable(this->client_bev.get(), EV_READ | EV_WRITE);

  // Connect to the server, disconnecting first if needed
  this->server_bev.reset(bufferevent_socket_new(this->base.get(), -1,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS));

  // TODO: figure out why this copy is necessary... shouldn't we just be able to
  // use the sockaddr_storage directly?
  const struct sockaddr_in* sin_ss = reinterpret_cast<const sockaddr_in*>(&this->next_destination);
  if (sin_ss->sin_family != AF_INET) {
    throw logic_error("ss not AF_INET");
  }
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = sin_ss->sin_port;
  sin.sin_addr.s_addr = sin_ss->sin_addr.s_addr;

  string netloc_str = render_sockaddr_storage(this->next_destination);
  log(INFO, "[ProxyServer] Connecting to %s", netloc_str.c_str());
  if (bufferevent_socket_connect(this->server_bev.get(),
      reinterpret_cast<const sockaddr*>(&sin), sizeof(sin)) != 0) {
    throw runtime_error(string_printf("failed to connect (%d)", EVUTIL_SOCKET_ERROR()));
  }
  bufferevent_setcb(this->server_bev.get(),
      &ProxyServer::dispatch_on_server_input, nullptr,
      &ProxyServer::dispatch_on_server_error, this);
  bufferevent_enable(this->server_bev.get(), EV_READ | EV_WRITE);
}

void ProxyServer::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[ProxyServer] Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void ProxyServer::on_client_input(struct bufferevent*) {
  this->receive_and_process_commands(false);
}

void ProxyServer::on_server_input(struct bufferevent*) {
  this->receive_and_process_commands(true);
}

void ProxyServer::on_client_error(struct bufferevent*, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[ProxyServer] Error %d (%s) in client stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "[ProxyServer] Client has disconnected");
    this->client_bev.reset();
    // "forward" the disconnection to the server
    this->server_bev.reset();

    // disable encryption
    this->server_input_crypt.reset();
    this->server_output_crypt.reset();
    this->client_input_crypt.reset();
    this->client_output_crypt.reset();
  }
}

void ProxyServer::on_server_error(struct bufferevent*, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[ProxyServer] Error %d (%s) in server stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "[ProxyServer] Server has disconnected");
    this->server_bev.reset();
    // "forward" the disconnection to the client
    this->client_bev.reset();

    // disable encryption
    this->server_input_crypt.reset();
    this->server_output_crypt.reset();
    this->client_input_crypt.reset();
    this->client_output_crypt.reset();
  }
}

size_t ProxyServer::get_size_field(const PSOCommandHeader* header) {
  if (this->version == GameVersion::DC) {
    return header->dc.size;
  } else if (this->version == GameVersion::PC) {
    return header->pc.size;
  } else if (this->version == GameVersion::GC) {
    return header->gc.size;
  } else if (this->version == GameVersion::BB) {
    return header->bb.size;
  } else {
    throw logic_error("version not supported in proxy mode");
  }
}

size_t ProxyServer::get_command_field(const PSOCommandHeader* header) {
  if (this->version == GameVersion::DC) {
    return header->dc.command;
  } else if (this->version == GameVersion::PC) {
    return header->pc.command;
  } else if (this->version == GameVersion::GC) {
    return header->gc.command;
  } else if (this->version == GameVersion::BB) {
    return header->bb.command;
  } else {
    throw logic_error("version not supported in proxy mode");
  }
}

void ProxyServer::receive_and_process_commands(bool from_server) {
  struct bufferevent* source_bev = from_server ? this->server_bev.get() : this->client_bev.get();
  struct bufferevent* dest_bev = from_server ? this->client_bev.get() : this->server_bev.get();

  struct evbuffer* source_buf = bufferevent_get_input(source_bev);
  struct evbuffer* dest_buf = dest_bev ? bufferevent_get_output(dest_bev) : nullptr;

  PSOEncryption* source_crypt = from_server ? this->server_input_crypt.get() : this->client_input_crypt.get();
  PSOEncryption* dest_crypt = from_server ? this->client_output_crypt.get() : this->server_output_crypt.get();

  PSOCommandHeader* input_header = from_server ? &this->server_input_header : &this->client_input_header;

  for (;;) {
    if (this->get_size_field(input_header) == 0) {
      ssize_t bytes = evbuffer_copyout(source_buf, input_header,
          this->header_size);
      if (bytes < static_cast<ssize_t>(this->header_size)) {
        break;
      }

      if (source_crypt) {
        source_crypt->decrypt(input_header, this->header_size);
      }
    }

    size_t command_size = this->get_size_field(input_header);
    if (evbuffer_get_length(source_buf) < command_size) {
      break;
    }

    string command(command_size, '\0');
    ssize_t bytes = evbuffer_remove(source_buf, command.data(), command_size);
    if (bytes < static_cast<ssize_t>(command_size)) {
      throw logic_error("enough bytes available, but could not remove them");
    }
    memcpy(command.data(), input_header, this->header_size);

    if (source_crypt) {
      source_crypt->decrypt(command.data() + this->header_size,
          command_size - this->header_size);
    }

    log(INFO, "[ProxyServer] %s:", from_server ? "server" : "client");
    print_data(stderr, command);

    // Preprocess the command if needed

    // Preprocessing for bidirectional commands...
    switch (this->get_command_field(input_header)) {
      case 0x60:
      case 0x62:
      case 0x6C:
      case 0x6D:
      case 0xC9:
      case 0xCB: { // broadcast/target commands
        if (command.size() <= this->header_size) {
          log(WARNING, "[ProxyServer] Received broadcast/target command with no contents");
        } else {
          uint8_t which = *reinterpret_cast<uint8_t*>(command.data() + this->header_size);
          if (!subcommand_is_implemented(which)) {
            log(WARNING, "[ProxyServer] Received broadcast/target subcommand %02hhX which is not implemented on the server",
                which);
          }
        }
        break;
      }
    }

    // Preprocessing for server->client commands...
    if (from_server) {
      switch (this->get_command_field(input_header)) {
        case 0x02: // init encryption
        case 0x17: { // init encryption
          if (this->version == GameVersion::BB) {
            throw invalid_argument("console server init received on BB");
          }

          struct InitEncryptionCommand {
            PSOCommandHeaderDCGC header;
            char copyright[0x40];
            uint32_t server_key;
            uint32_t client_key;
          };
          if (command.size() < sizeof(InitEncryptionCommand)) {
            throw std::runtime_error("init encryption command is too small");
          }

          const InitEncryptionCommand* cmd = reinterpret_cast<const InitEncryptionCommand*>(
              command.data());
          if (this->version == GameVersion::PC) {
            this->server_input_crypt.reset(new PSOPCEncryption(cmd->server_key));
            this->server_output_crypt.reset(new PSOPCEncryption(cmd->client_key));
            this->client_input_crypt.reset(new PSOPCEncryption(cmd->client_key));
            this->client_output_crypt.reset(new PSOPCEncryption(cmd->server_key));

          } else if (this->version == GameVersion::GC) {
            this->server_input_crypt.reset(new PSOGCEncryption(cmd->server_key));
            this->server_output_crypt.reset(new PSOGCEncryption(cmd->client_key));
            this->client_input_crypt.reset(new PSOGCEncryption(cmd->client_key));
            this->client_output_crypt.reset(new PSOGCEncryption(cmd->server_key));

          } else {
            throw invalid_argument("unsupported version");
          }
          break;
        }

        case 0x19: { // reconnect
          struct ReconnectCommandArgs {
            uint32_t address;
            uint16_t port;
            uint16_t unused;
          };

          if (command.size() < sizeof(ReconnectCommandArgs) + this->header_size) {
            throw std::runtime_error("reconnect command is too small");
          }

          ReconnectCommandArgs* args = reinterpret_cast<ReconnectCommandArgs*>(
              command.data() + this->header_size);
          memset(&this->next_destination, 0, sizeof(this->next_destination));
          struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
              &this->next_destination);
          sin->sin_family = AF_INET;
          sin->sin_port = htons(args->port);
          sin->sin_addr.s_addr = args->address; // already network byte order

          if (!dest_bev) {
            log(WARNING, "[ProxyServer] Received reconnect command with no destination present");
          } else {
            struct sockaddr_storage sockname_ss;
            socklen_t len = sizeof(sockname_ss);
            int fd = bufferevent_getfd(dest_bev);
            if (fd < 0) { // virtual connection
              args->address = 0x23232323; // TODO: apply the different-network logic here too
              args->port = 9000;
            } else {
              getsockname(fd, reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
              if (sockname_ss.ss_family != AF_INET) {
                throw logic_error("existing connection is not ipv4");
              }

              struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
                  &sockname_ss);
              args->address = sockname_sin->sin_addr.s_addr; // Already network byte order
              args->port = ntohs(sockname_sin->sin_port); // Client expects this little-endian for some reason
            }
          }
          break;
        }

        case 0x44:
        case 0xA6: { // open quest file
          if (!this->save_quests) {
            break;
          }

          bool is_download_quest = this->get_command_field(input_header) == 0xA6;

          struct OpenFileCommand {
            char name[0x20];
            uint16_t unused;
            uint16_t flags;
            char filename[0x10];
            uint32_t file_size;
          };
          if (command.size() < sizeof(OpenFileCommand)) {
            log(WARNING, "[ProxyServer] Open file command is too small");
            break;
          }
          const auto* cmd = reinterpret_cast<const OpenFileCommand*>(command.data() + this->header_size);

          string output_filename = string_printf("%s.%s.%" PRIu64,
              cmd->filename, is_download_quest ? "download" : "online", now());
          for (size_t x = 0; x < output_filename.size(); x++) {
            if (output_filename[x] < 0x20 || output_filename[x] > 0x7E || output_filename[x] == '/') {
              output_filename[x] = '_';
            }
          }
          if (output_filename[0] == '.') {
            output_filename[0] = '_';
          }

          SavingQuestFile sqf(cmd->filename, output_filename, cmd->file_size);
          this->saving_quest_files.emplace(cmd->filename, move(sqf));
          log(INFO, "[ProxyServer] Opened quest file %s", output_filename.c_str());
          break;
        }
        case 0x13:
        case 0xA7: { // quest data segment
          if (!this->save_quests) {
            break;
          }

          struct WriteFileCommand {
            char filename[0x10];
            uint8_t data[0x400];
            uint32_t data_size;
          };
          if (command.size() < sizeof(WriteFileCommand)) {
            log(WARNING, "[ProxyServer] Write file command is too small");
            break;
          }
          const auto* cmd = reinterpret_cast<const WriteFileCommand*>(command.data() + this->header_size);

          SavingQuestFile* sqf = nullptr;
          try {
            sqf = &this->saving_quest_files.at(cmd->filename);
          } catch (const out_of_range&) {
            log(WARNING, "[ProxyServer] Can\'t find saving quest file %s",
                cmd->filename);
            break;
          }

          size_t bytes_to_write = cmd->data_size;
          if (bytes_to_write > 0x400) {
            log(WARNING, "[ProxyServer] Chunk data size is invalid; truncating to 0x400");
            bytes_to_write = 0x400;
          }

          log(INFO, "[ProxyServer] Writing %zu bytes to %s", bytes_to_write,
              sqf->output_filename.c_str());
          fwritex(sqf->f.get(), cmd->data, bytes_to_write);
          if (bytes_to_write > sqf->remaining_bytes) {
            log(WARNING, "[ProxyServer] Chunk size extends beyond original file size; file may be truncated");
            sqf->remaining_bytes = 0;
          } else {
            sqf->remaining_bytes -= bytes_to_write;
          }

          if (sqf->remaining_bytes == 0) {
            log(INFO, "[ProxyServer] File %s is complete", sqf->output_filename.c_str());
            this->saving_quest_files.erase(cmd->filename);
          }

          break;
        }
      }
    }

    // reencrypt and forward the command
    if (dest_buf) {
      if (dest_crypt) {
        dest_crypt->encrypt(command.data(), command.size());
      }
      //log(INFO, "[ProxyServer-debug] Sending encrypted command");
      //print_data(stderr, command);

      evbuffer_add(dest_buf, command.data(), command.size());
    } else {
      log(WARNING, "[ProxyServer] No destination present; dropping command");
    }

    // clear the input header so we can read the next command
    memset(input_header, 0, this->header_size);
  }
}
