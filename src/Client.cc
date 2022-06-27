#include "Client.hh"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "Version.hh"

using namespace std;



const uint64_t CLIENT_CONFIG_MAGIC = 0x492A890E82AC9839;



Client::Client(
    struct bufferevent* bev,
    GameVersion version,
    ServerBehavior server_behavior)
  : log("", client_log.min_level),
    version(version),
    bb_game_state(0),
    flags(flags_for_version(this->version, 0)),
    channel(bev, this->version, nullptr, nullptr, this, "", TerminalFormat::FG_YELLOW, TerminalFormat::FG_GREEN),
    server_behavior(server_behavior),
    should_disconnect(false),
    should_send_to_lobby_server(false),
    proxy_destination_address(0),
    proxy_destination_port(0),
    x(0.0f),
    z(0.0f),
    area(0),
    lobby_id(0),
    lobby_client_id(0),
    lobby_arrow_color(0),
    prefer_high_lobby_client_id(false),
    next_exp_value(0),
    override_section_id(-1),
    infinite_hp(false),
    infinite_tp(false),
    switch_assist(false),
    can_chat(true),
    pending_bb_save_player_index(0),
    dol_base_addr(0) {
  this->last_switch_enabled_command.subcommand = 0;
  memset(&this->next_connection_addr, 0, sizeof(this->next_connection_addr));
}

void Client::set_license(shared_ptr<const License> l) {
  this->license = l;
  this->game_data.serial_number = this->license->serial_number;
  if (this->version == GameVersion::BB) {
    this->game_data.bb_username = this->license->username;
  }
}

ClientConfig Client::export_config() const {
  ClientConfig cc;
  cc.magic = CLIENT_CONFIG_MAGIC;
  cc.flags = this->flags;
  cc.proxy_destination_address = this->proxy_destination_address;
  cc.proxy_destination_port = this->proxy_destination_port;
  cc.unused.clear(0xFF);
  return cc;
}

ClientConfigBB Client::export_config_bb() const {
  ClientConfigBB cc;
  cc.cfg = this->export_config();
  cc.bb_game_state = this->bb_game_state;
  cc.bb_player_index = this->game_data.bb_player_index;
  cc.unused.clear(0xFF);
  return cc;
}

void Client::import_config(const ClientConfig& cc) {
  if (cc.magic != CLIENT_CONFIG_MAGIC) {
    throw invalid_argument("invalid client config");
  }
  this->flags = cc.flags;
  this->proxy_destination_address = cc.proxy_destination_address;
  this->proxy_destination_port = cc.proxy_destination_port;
}

void Client::import_config(const ClientConfigBB& cc) {
  this->import_config(cc.cfg);
  this->bb_game_state = cc.bb_game_state;
  this->game_data.bb_player_index = cc.bb_player_index;
}
