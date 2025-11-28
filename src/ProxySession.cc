#include "ProxySession.hh"

#include "ServerState.hh"

using namespace std;

size_t ProxySession::num_proxy_sessions = 0;

ProxySession::ProxySession(shared_ptr<Channel> server_channel, const PersistentConfig* pc)
    : server_channel(server_channel) {
  if (pc) {
    this->remote_guild_card_number = pc->remote_guild_card_number;
    this->enable_remote_ip_crc_patch = pc->enable_remote_ip_crc_patch;
  } else if (is_v4(this->server_channel->version)) {
    this->remote_guild_card_number = 0;
  }
  this->num_proxy_sessions++;
}

ProxySession::~ProxySession() {
  this->num_proxy_sessions--;
}

void ProxySession::set_drop_mode(
    shared_ptr<ServerState> s, Version version, int64_t override_random_seed, ProxyDropMode new_mode) {
  this->drop_mode = new_mode;
  if (this->drop_mode == ProxyDropMode::INTERCEPT) {
    auto rand_crypt = make_shared<MT19937Generator>((override_random_seed >= 0) ? override_random_seed : this->lobby_random_seed);
    this->item_creator = make_shared<ItemCreator>(
        s->common_item_set(version, nullptr),
        s->rare_item_set(version, nullptr),
        s->armor_random_set,
        s->tool_random_set,
        s->weapon_random_set(this->lobby_difficulty),
        s->tekker_adjustment_set,
        s->item_parameter_table(version),
        s->item_stack_limits(version),
        (this->lobby_mode == GameMode::SOLO) ? GameMode::NORMAL : this->lobby_mode,
        this->lobby_difficulty,
        this->lobby_section_id,
        std::move(rand_crypt),
        // TODO: Can we get battle rules here somehow?
        nullptr);
  } else {
    this->item_creator.reset();
  }
}

void ProxySession::clear_lobby_players(size_t num_slots) {
  this->lobby_players.clear();
  this->lobby_players.resize(num_slots);
}
