#include "ProxySession.hh"

#include "ServerState.hh"

using namespace std;

size_t ProxySession::num_proxy_sessions = 0;

ProxySession::ProxySession(shared_ptr<Channel> server_channel, const PersistentConfig* pc)
    : server_channel(server_channel) {
  if (pc) {
    this->remote_guild_card_number = pc->remote_guild_card_number;
    this->remote_bb_security_token = pc->remote_bb_security_token;
    this->remote_client_config_data = pc->remote_client_config_data;
    this->enable_remote_ip_crc_patch = pc->enable_remote_ip_crc_patch;
  } else if (is_v4(this->server_channel->version)) {
    this->remote_guild_card_number = 0;
  }
  this->num_proxy_sessions++;
}

ProxySession::~ProxySession() {
  this->num_proxy_sessions--;
}

void ProxySession::set_drop_mode(shared_ptr<ServerState> s, Version version, int64_t override_random_seed, DropMode new_mode) {
  this->drop_mode = new_mode;
  if (this->drop_mode == DropMode::INTERCEPT) {
    shared_ptr<const RareItemSet> rare_item_set;
    shared_ptr<const CommonItemSet> common_item_set;
    switch (version) {
      case Version::PC_PATCH:
      case Version::BB_PATCH:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
        throw runtime_error("cannot create item creator for this base version");
      case Version::DC_NTE:
      case Version::DC_11_2000:
      case Version::DC_V1:
        // TODO: We should probably have a v1 common item set at some point too
        common_item_set = s->common_item_set_v2;
        rare_item_set = s->rare_item_sets.at("rare-table-v1");
        break;
      case Version::DC_V2:
      case Version::PC_NTE:
      case Version::PC_V2:
        common_item_set = s->common_item_set_v2;
        rare_item_set = s->rare_item_sets.at("rare-table-v2");
        break;
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::XB_V3:
        common_item_set = s->common_item_set_v3_v4;
        rare_item_set = s->rare_item_sets.at("rare-table-v3");
        break;
      case Version::BB_V4:
        common_item_set = s->common_item_set_v3_v4;
        rare_item_set = s->rare_item_sets.at("rare-table-v4");
        break;
      default:
        throw logic_error("invalid lobby base version");
    }
    auto rand_crypt = make_shared<MT19937Generator>((override_random_seed >= 0) ? override_random_seed : this->lobby_random_seed);
    this->item_creator = make_shared<ItemCreator>(
        common_item_set,
        rare_item_set,
        s->armor_random_set,
        s->tool_random_set,
        s->weapon_random_sets.at(this->lobby_difficulty),
        s->tekker_adjustment_set,
        s->item_parameter_table(version),
        s->item_stack_limits(version),
        this->lobby_episode,
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
