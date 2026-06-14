#include "ProxySession.hh"

#include "ServerState.hh"

size_t ProxySession::num_proxy_sessions = 0;

ProxySession::ProxySession(std::shared_ptr<Channel> server_channel, const PersistentConfig* pc)
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
    std::shared_ptr<ServerState> s, Version version, int64_t override_random_seed, ProxyDropMode new_mode) {
  this->drop_mode = new_mode;
  if (this->drop_mode == ProxyDropMode::INTERCEPT) {
    auto rand_crypt = std::make_shared<MT19937Generator>((override_random_seed >= 0) ? override_random_seed : this->lobby_random_seed);
    this->item_creator = std::make_shared<ItemCreator>(
        s->data->common_item_set(version, nullptr),
        s->data->rare_item_set(version, nullptr),
        s->data->armor_random_set,
        s->data->tool_random_set,
        s->data->weapon_random_set(this->lobby_difficulty),
        s->data->tekker_adjustment_set,
        s->data->item_parameter_table(version),
        s->data->item_stack_limits(version),
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
