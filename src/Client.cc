#include "Client.hh"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Version.hh"

using namespace std;



const uint64_t CLIENT_CONFIG_MAGIC = 0x492A890E82AC9839;



Client::Client(
    struct bufferevent* bev,
    GameVersion version,
    ServerBehavior server_behavior)
  : version(version),
    flags(flags_for_version(this->version, 0)),
    bev(bev),
    server_behavior(server_behavior),
    should_disconnect(false),
    proxy_destination_address(0),
    proxy_destination_port(0),
    play_time_begin(now()),
    last_recv_time(this->play_time_begin),
    last_send_time(0),
    area(0),
    lobby_id(0),
    lobby_client_id(0),
    lobby_arrow_color(0),
    next_exp_value(0),
    override_section_id(-1),
    infinite_hp(false),
    infinite_tp(false),
    switch_assist(false),
    can_chat(true) {
  memset(&this->last_switch_enabled_subcommand, 0, sizeof(this->last_switch_enabled_subcommand));
  int fd = bufferevent_getfd(this->bev);
  if (fd < 0) {
    this->is_virtual_connection = true;
    memset(&this->local_addr, 0, sizeof(this->local_addr));
    memset(&this->remote_addr, 0, sizeof(this->remote_addr));
  } else {
    this->is_virtual_connection = false;
    get_socket_addresses(fd, &this->local_addr, &this->remote_addr);
  }
  memset(&this->next_connection_addr, 0, sizeof(this->next_connection_addr));
}

ClientConfig Client::export_config() const {
  ClientConfig cc;
  cc.magic = CLIENT_CONFIG_MAGIC;
  cc.bb_game_state = this->bb_game_state;
  cc.bb_player_index = this->bb_player_index;
  cc.flags = this->flags;
  cc.proxy_destination_address = this->proxy_destination_address;
  cc.proxy_destination_port = this->proxy_destination_port;
  cc.unused.clear(0xFF);
  cc.unused_bb_only.clear(0xFF);
  return cc;
}

void Client::import_config(const ClientConfig& cc) {
  if (cc.magic != CLIENT_CONFIG_MAGIC) {
    throw invalid_argument("invalid client config");
  }
  this->bb_game_state = cc.bb_game_state;
  this->bb_player_index = cc.bb_player_index;
  this->flags = cc.flags;
  this->proxy_destination_address = cc.proxy_destination_address;
  this->proxy_destination_port = cc.proxy_destination_port;
}
