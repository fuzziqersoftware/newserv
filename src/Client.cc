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



Client::Client(struct bufferevent* bev, GameVersion version,
    ServerBehavior server_behavior) : version(version),
    flags(flags_for_version(version, 0)), bev(bev),
    server_behavior(server_behavior), should_disconnect(false),
    play_time_begin(now()), last_recv_time(this->play_time_begin),
    last_send_time(0), area(0), lobby_id(0), lobby_client_id(0),
    lobby_arrow_color(0), next_exp_value(0), infinite_hp(false),
    infinite_tp(false), can_chat(true) {

  int fd = bufferevent_getfd(this->bev);
  get_socket_addresses(fd, &this->local_addr, &this->remote_addr);
  memset(this->name, 0, sizeof(this->name));
  memset(&this->next_connection_addr, 0, sizeof(this->next_connection_addr));
}

bool Client::send(string&& data) {
  if (!this->bev) {
    return false;
  }

  if (this->crypt_out.get()) {
    this->crypt_out->encrypt(data.data(), data.size());
  }

  struct evbuffer* buf = bufferevent_get_output(this->bev);
  evbuffer_add(buf, data.data(), data.size());
  return true;
}
