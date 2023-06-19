#include "CatSession.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ProxyCommands.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"

using namespace std;

CatSession::CatSession(
    shared_ptr<struct event_base> base,
    const struct sockaddr_storage& remote,
    GameVersion version,
    shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file)
    : Shell(base),
      log("[CatSession] ", proxy_server_log.min_level),
      channel(
          version,
          CatSession::dispatch_on_channel_input,
          CatSession::dispatch_on_channel_error,
          this,
          "CatSession"),
      bb_key_file(bb_key_file) {
  if (remote.ss_family != AF_INET) {
    throw runtime_error("remote is not AF_INET");
  }

  string netloc_str = render_sockaddr_storage(remote);
  this->log.info("Connecting to %s", netloc_str.c_str());

  struct bufferevent* bev = bufferevent_socket_new(
      this->base.get(), -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  if (!bev) {
    throw runtime_error(string_printf("failed to open socket (%d)", EVUTIL_SOCKET_ERROR()));
  }
  this->channel.set_bufferevent(bev);

  if (bufferevent_socket_connect(this->channel.bev.get(),
          reinterpret_cast<const sockaddr*>(&remote), sizeof(struct sockaddr_in)) != 0) {
    throw runtime_error(string_printf("failed to connect (%d)", EVUTIL_SOCKET_ERROR()));
  }
}

void CatSession::dispatch_on_channel_input(
    Channel& ch, uint16_t command, uint32_t flag, std::string& data) {
  auto* session = reinterpret_cast<CatSession*>(ch.context_obj);
  session->on_channel_input(command, flag, data);
}

void CatSession::on_channel_input(
    uint16_t command, uint32_t flag, std::string& data) {
  if (this->channel.version != GameVersion::BB) {
    if (command == 0x02 || command == 0x17 || command == 0x91 || command == 0x9B) {
      const auto& cmd = check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(data, 0xFFFF);
      if ((this->channel.version == GameVersion::GC) ||
          (this->channel.version == GameVersion::XB)) {
        this->channel.crypt_in.reset(new PSOV3Encryption(cmd.server_key));
        this->channel.crypt_out.reset(new PSOV3Encryption(cmd.client_key));
        this->log.info("Enabled V3 encryption (server key %08" PRIX32 ", client key %08" PRIX32 ")",
            cmd.server_key.load(), cmd.client_key.load());
      } else { // PC, DC, or patch server
        this->channel.crypt_in.reset(new PSOV2Encryption(cmd.server_key));
        this->channel.crypt_out.reset(new PSOV2Encryption(cmd.client_key));
        this->log.info("Enabled V2 encryption (server key %08" PRIX32 ", client key %08" PRIX32 ")",
            cmd.server_key.load(), cmd.client_key.load());
      }
    }
  } else { // BB
    if (command == 0x03 || command == 0x9B) {
      if (!this->bb_key_file) {
        throw runtime_error("BB encryption requires a key file");
      }
      const auto& cmd = check_size_t<S_ServerInitDefault_BB_03_9B>(data, 0xFFFF);
      this->channel.crypt_in.reset(new PSOBBEncryption(*this->bb_key_file, &cmd.server_key[0], sizeof(cmd.server_key)));
      this->channel.crypt_out.reset(new PSOBBEncryption(*this->bb_key_file, &cmd.client_key[0], sizeof(cmd.client_key)));
      this->log.info("Enabled BB encryption");
    }
  }

  // TODO: Use the iovec form of print_data here instead of
  // prepend_command_header (which copies the string)
  string full_cmd = prepend_command_header(
      this->channel.version, this->channel.crypt_in.get(), command, flag, data);
  print_data(stdout, full_cmd, 0, nullptr, PrintDataFlags::PRINT_ASCII | PrintDataFlags::OFFSET_16_BITS);
}

void CatSession::dispatch_on_channel_error(Channel& ch, short events) {
  auto* session = reinterpret_cast<CatSession*>(ch.context_obj);
  session->on_channel_error(events);
}

void CatSession::on_channel_error(short events) {
  if (events & BEV_EVENT_CONNECTED) {
    this->log.info("Channel connected");
  }
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    this->log.warning("Error %d (%s) in unlinked client stream", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    this->log.info("Session endpoint has disconnected");
    this->channel.disconnect();
    event_base_loopexit(this->base.get(), nullptr);
  }
}

void CatSession::print_prompt() {}

void CatSession::execute_command(const std::string& command) {
  string full_cmd = parse_data_string(command, nullptr, ParseDataFlags::ALLOW_FILES);
  send_command_with_header(this->channel, full_cmd.data(), full_cmd.size());
}
