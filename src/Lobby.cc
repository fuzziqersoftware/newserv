#include "Lobby.hh"

#include <string.h>

#include <phosg/Random.hh>

#include "Compression.hh"
#include "Loggers.hh"
#include "SendCommands.hh"
#include "Text.hh"

using namespace std;

bool Lobby::FloorItem::visible_to_client(uint8_t client_id) const {
  return this->flags & (1 << client_id);
}

Lobby::FloorItemManager::FloorItemManager(uint32_t lobby_id, uint8_t floor)
    : log(phosg::string_printf("[Lobby:%08" PRIX32 ":FloorItems:%02hhX] ", lobby_id, floor), lobby_log.min_level),
      next_drop_number(0) {}

bool Lobby::FloorItemManager::exists(uint32_t item_id) const {
  return this->items.count(item_id);
}

shared_ptr<Lobby::FloorItem> Lobby::FloorItemManager::find(uint32_t item_id) const {
  return this->items.at(item_id);
}

void Lobby::FloorItemManager::add(
    const ItemData& item,
    const VectorXZF& pos,
    shared_ptr<const MapState::ObjectState> from_obj,
    shared_ptr<const MapState::EnemyState> from_ene,
    uint16_t flags) {
  auto fi = make_shared<FloorItem>();
  fi->data = item;
  fi->pos = pos;
  fi->drop_number = this->next_drop_number++;
  fi->from_obj = from_obj;
  fi->from_ene = from_ene;
  fi->flags = flags;
  this->add(fi);
}

void Lobby::FloorItemManager::add(shared_ptr<Lobby::FloorItem> fi) {
  if (fi->flags == 0) {
    throw logic_error("floor item is not visible to any player");
  }

  auto emplace_ret = this->items.emplace(fi->data.id, fi);
  if (!emplace_ret.second) {
    throw runtime_error("floor item already exists with the same ID");
  }
  for (size_t z = 0; z < 12; z++) {
    if (fi->visible_to_client(z)) {
      this->queue_for_client[z].emplace(fi->drop_number, fi);
    }
  }
  this->log.info("Added floor item %08" PRIX32 " at %g, %g with drop number %" PRIu64 " with flags %03hX",
      fi->data.id.load(), fi->pos.x.load(), fi->pos.z.load(), fi->drop_number, fi->flags);
}

std::shared_ptr<Lobby::FloorItem> Lobby::FloorItemManager::remove(uint32_t item_id, uint8_t client_id) {
  auto item_it = this->items.find(item_id);
  if (item_it == this->items.end()) {
    throw out_of_range("item not present");
  }
  auto fi = item_it->second;
  if ((client_id != 0xFF) && !fi->visible_to_client(client_id)) {
    throw runtime_error("client does not have access to item");
  }
  for (size_t z = 0; z < 12; z++) {
    if (fi->visible_to_client(z) && !this->queue_for_client[z].erase(fi->drop_number)) {
      throw logic_error("item queue for client is inconsistent");
    }
  }
  this->items.erase(item_it);
  this->log.info("Removed floor item %08" PRIX32 " at %g, %g with drop number %" PRIu64 " with flags %03hX",
      fi->data.id.load(), fi->pos.x.load(), fi->pos.z.load(), fi->drop_number, fi->flags);
  return fi;
}

std::unordered_set<std::shared_ptr<Lobby::FloorItem>> Lobby::FloorItemManager::evict() {
  unordered_set<shared_ptr<FloorItem>> ret;
  for (size_t z = 0; z < 12; z++) {
    while (this->queue_for_client[z].size() > 48) {
      ret.emplace(this->remove(this->queue_for_client[z].begin()->second->data.id, 0xFF));
    }
  }
  this->log.info("Evicted %zu items", ret.size());
  return ret;
}

void Lobby::FloorItemManager::clear_inaccessible(uint16_t remaining_clients_mask) {
  unordered_set<uint32_t> item_ids_to_delete;
  for (const auto& it : this->items) {
    if ((it.second->flags & remaining_clients_mask) == 0) {
      item_ids_to_delete.emplace(it.first);
    }
  }
  for (uint32_t item_id : item_ids_to_delete) {
    this->remove(item_id, 0xFF);
  }
  this->log.info("Deleted %zu inaccessible items", item_ids_to_delete.size());
}

void Lobby::FloorItemManager::clear_private() {
  unordered_set<uint32_t> item_ids_to_delete;
  for (const auto& it : this->items) {
    if ((it.second->flags & 0x00F) != 0x00F) {
      item_ids_to_delete.emplace(it.first);
    }
  }
  for (uint32_t item_id : item_ids_to_delete) {
    this->remove(item_id, 0xFF);
  }
  this->log.info("Deleted %zu private items", item_ids_to_delete.size());
}

void Lobby::FloorItemManager::clear() {
  size_t num_items = this->items.size();
  this->items.clear();
  for (auto& queue : this->queue_for_client) {
    queue.clear();
  }
  this->next_drop_number = 0;
  this->log.info("Deleted %zu items", num_items);
}

uint32_t Lobby::FloorItemManager::reassign_all_item_ids(uint32_t next_item_id) {
  ::map<uint32_t, shared_ptr<FloorItem>> old_items;
  old_items.swap(this->items);
  for (auto& queue : this->queue_for_client) {
    queue.clear();
  }
  for (auto& it : old_items) {
    it.second->data.id = next_item_id++;
    this->add(it.second);
  }
  return next_item_id;
}

Lobby::Lobby(shared_ptr<ServerState> s, uint32_t id, bool is_game)
    : server_state(s),
      log(phosg::string_printf("[%s:%" PRIX32 "] ", is_game ? "Game" : "Lobby", id), lobby_log.min_level),
      lobby_id(id),
      min_level(0),
      max_level(0xFFFFFFFF),
      next_game_item_id(0xCC000000),
      allowed_versions(0x0000),
      override_section_id(0xFF),
      episode(Episode::NONE),
      mode(GameMode::NORMAL),
      difficulty(0),
      base_exp_multiplier(1),
      exp_share_multiplier(0.5),
      challenge_exp_multiplier(1.0f),
      random_seed(phosg::random_object<uint32_t>()),
      drop_mode(DropMode::CLIENT),
      event(0),
      block(0),
      leader_id(0),
      max_clients(12),
      enabled_flags(0),
      idle_timeout_usecs(0),
      idle_timeout_event(
          event_new(s->base.get(), -1, EV_TIMEOUT | EV_PERSIST, &Lobby::dispatch_on_idle_timeout, this),
          event_free) {
  this->log.info("Created");
  if (is_game) {
    this->set_flag(Flag::GAME);
  }
  this->reset_next_item_ids();
}

Lobby::~Lobby() {
  this->log.info("Deleted");
}

void Lobby::reset_next_item_ids() {
  uint32_t base_item_id = this->is_game() ? 0x00010000 : 0x10010000;
  for (size_t x = 0; x < 12; x++) {
    this->next_item_id_for_client[x] = base_item_id + 0x00200000 * x;
  }
  this->next_game_item_id = 0xCC000000;
}

shared_ptr<ServerState> Lobby::require_server_state() const {
  auto s = this->server_state.lock();
  if (!s) {
    throw logic_error("server is deleted");
  }
  return s;
}

shared_ptr<Lobby::ChallengeParameters> Lobby::require_challenge_params() const {
  if (!this->challenge_params) {
    throw runtime_error("challenge params are missing");
  }
  return this->challenge_params;
}

void Lobby::create_item_creator(Version logic_version) {
  if (!this->is_game() || this->episode == Episode::EP3) {
    this->item_creator.reset();
    return;
  }

  auto s = this->require_server_state();

  if (logic_version == Version::UNKNOWN) {
    auto leader_c = this->clients[this->leader_id];
    logic_version = leader_c ? leader_c->version() : Version::BB_V4;
  }

  shared_ptr<const RareItemSet> rare_item_set;
  shared_ptr<const CommonItemSet> common_item_set;
  switch (logic_version) {
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

  this->item_creator = make_shared<ItemCreator>(
      common_item_set,
      rare_item_set,
      s->armor_random_set,
      s->tool_random_set,
      s->weapon_random_sets.at(this->difficulty),
      s->tekker_adjustment_set,
      s->item_parameter_table(logic_version),
      s->item_stack_limits(logic_version),
      this->episode,
      (this->mode == GameMode::SOLO) ? GameMode::NORMAL : this->mode,
      this->difficulty,
      this->effective_section_id(),
      this->opt_rand_crypt,
      this->quest ? this->quest->battle_rules : nullptr);
}

uint8_t Lobby::effective_section_id() const {
  if (this->override_section_id != 0xFF) {
    return this->override_section_id;
  }
  if (this->check_flag(Lobby::Flag::USE_CREATOR_SECTION_ID)) {
    return this->creator_section_id;
  }
  auto leader = this->clients.at(this->leader_id);
  if (leader) {
    return leader->character()->disp.visual.section_id;
  }
  return 0;
}

uint16_t Lobby::quest_version_flags() const {
  uint16_t ret = 0;
  for (auto lc : this->clients) {
    if (lc) {
      ret |= (1 << static_cast<size_t>(lc->version()));
    }
  }
  return ret;
}

void Lobby::load_maps() {
  auto rare_rates = this->rare_enemy_rates ? this->rare_enemy_rates : MapState::DEFAULT_RARE_ENEMIES;

  if (this->episode == Episode::EP3) {
    this->map_state = make_shared<MapState>();
  } else if (this->quest) {
    this->map_state = make_shared<MapState>(
        this->lobby_id,
        this->difficulty,
        this->event,
        this->random_seed,
        this->rare_enemy_rates,
        this->opt_rand_crypt,
        this->quest->get_supermap(this->random_seed));
  } else {
    auto s = this->require_server_state();
    this->map_state = make_shared<MapState>(
        this->lobby_id,
        this->difficulty,
        this->event,
        this->random_seed,
        this->rare_enemy_rates,
        this->opt_rand_crypt,
        s->supermaps_for_variations(this->episode, this->mode, this->difficulty, this->variations));
  }

  if (this->check_flag(Lobby::Flag::DEBUG)) {
    this->log.info("Generated map state:");
    this->map_state->print(stderr);
  }
}

[[nodiscard]] bool Lobby::is_ep3_nte() const {
  for (const auto& lc : this->clients) {
    if (lc && (lc->version() != Version::GC_EP3_NTE)) {
      return false;
    }
  }
  return true;
}

void Lobby::create_ep3_server() {
  auto s = this->require_server_state();
  if (!this->ep3_server) {
    this->log.info("Creating Episode 3 server state");
  } else {
    this->log.info("Recreating Episode 3 server state");
  }
  auto tourn = this->tournament_match ? this->tournament_match->tournament.lock() : nullptr;

  bool is_nte = this->is_ep3_nte();
  Episode3::Server::Options options = {
      .card_index = is_nte ? s->ep3_card_index_trial : s->ep3_card_index,
      .map_index = s->ep3_map_index,
      .behavior_flags = s->ep3_behavior_flags,
      .opt_rand_stream = nullptr,
      .opt_rand_crypt = this->opt_rand_crypt,
      .tournament = tourn,
      .trap_card_ids = s->ep3_trap_card_ids,
  };
  if (is_nte) {
    options.behavior_flags |= Episode3::BehaviorFlag::IS_TRIAL_EDITION;
  } else {
    options.behavior_flags &= (~Episode3::BehaviorFlag::IS_TRIAL_EDITION);
  }
  this->ep3_server = make_shared<Episode3::Server>(this->shared_from_this(), std::move(options));
  this->ep3_server->init();
}

void Lobby::reassign_leader_on_client_departure(size_t leaving_client_index) {
  for (size_t x = 0; x < this->max_clients; x++) {
    if (x == leaving_client_index) {
      continue;
    }
    if (this->clients[x]) {
      this->leader_id = x;
      this->create_item_creator();
      return;
    }
  }
  this->leader_id = 0;
}

bool Lobby::any_client_loading() const {
  for (size_t x = 0; x < this->max_clients; x++) {
    auto lc = this->clients[x];
    if (!lc.get()) {
      continue;
    }
    if (lc->config.check_flag(Client::Flag::LOADING) ||
        lc->config.check_flag(Client::Flag::LOADING_QUEST) ||
        lc->config.check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST)) {
      return true;
    }
  }
  return false;
}

size_t Lobby::count_clients() const {
  size_t ret = 0;
  for (size_t x = 0; x < this->max_clients; x++) {
    if (this->clients[x]) {
      ret++;
    }
  }
  return ret;
}

bool Lobby::any_v1_clients_present() const {
  for (size_t x = 0; x < this->max_clients; x++) {
    if (this->clients[x] && is_v1(this->clients[x]->version())) {
      return true;
    }
  }
  return false;
}

void Lobby::add_client(shared_ptr<Client> c, ssize_t required_client_id) {
  if (!c->login) {
    throw runtime_error("client is not logged in");
  }

  ssize_t index;
  ssize_t min_client_id = this->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 4 : 0;

  if (required_client_id >= 0) {
    if (this->clients.at(required_client_id).get()) {
      throw out_of_range("required slot is in use");
    }
    this->clients[required_client_id] = c;
    index = required_client_id;

  } else if (c->config.check_flag(Client::Flag::DEBUG_ENABLED) && (this->mode != GameMode::SOLO)) {
    for (index = this->max_clients - 1; index >= min_client_id; index--) {
      if (!this->clients[index].get()) {
        this->clients[index] = c;
        break;
      }
    }
    if (index < min_client_id) {
      throw out_of_range("no space left in lobby");
    }
  } else {
    for (index = min_client_id; index < this->max_clients; index++) {
      if (!this->clients[index].get()) {
        this->clients[index] = c;
        break;
      }
    }
    if (index >= this->max_clients) {
      throw out_of_range("no space left in lobby");
    }
  }

  c->lobby_client_id = index;
  c->lobby = this->weak_from_this();
  c->lobby_arrow_color = 0;

  // If there's no one else in the lobby, set the leader id as well
  size_t leader_index;
  for (leader_index = 0; leader_index < this->max_clients; leader_index++) {
    if (this->clients[leader_index] && (this->clients[leader_index] != c)) {
      break;
    }
  }
  if (leader_index >= this->max_clients) {
    this->leader_id = c->lobby_client_id;
    this->create_item_creator();
  }

  // If this is a lobby or no one was here before this, reassign all the floor
  // item IDs and reset the next item IDs
  if (!this->is_game() || (leader_index >= this->max_clients)) {
    this->reset_next_item_ids();
    for (auto& m : this->floor_item_managers) {
      this->next_game_item_id = m.reassign_all_item_ids(this->next_game_item_id);
    }
  }

  // If this is not a game or the joining client is the leader, they will assign
  // their item IDs BEFORE they process any inbound commands (therefore a 6x6D
  // command, which we will send during loading, should reflect the item state
  // AFTER their IDs are assigned). If the joining client is not the leader,
  // they will not assign their item IDs until they receive a 6x71 command,
  // which is sent AFTER the 6x6D command, so the 6x6D should reflect the item
  // state BEFORE their IDs are assigned. (In the latter case, we'll assign the
  // IDs for real when they send a 6F command, or 6x1F equivalent in the case of
  // DC NTE and 11/2000.)
  this->assign_inventory_and_bank_item_ids(c, (!this->is_game() || (c->lobby_client_id == this->leader_id)));

  // On BB, we send artificial flag state to fix an Episode 2 bug where the
  // CCA door lock state is overwritten by quests.
  if (this->is_game() && (c->version() == Version::BB_V4)) {
    c->config.set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
  }

  // If the lobby is recording a battle record, add the player join event
  if (this->battle_record) {
    auto p = c->character();
    PlayerLobbyDataDCGC lobby_data;
    lobby_data.player_tag = 0x00010000;
    lobby_data.guild_card_number = c->login->account->account_id;
    lobby_data.name.encode(p->disp.name.decode(c->language()), c->language());
    this->battle_record->add_player(
        lobby_data,
        p->inventory,
        p->disp.to_dcpcv3<false>(c->language(), c->language()),
        c->ep3_config ? (c->ep3_config->online_clv_exp / 100) : 0);
  }

  // Send spectator count notifications if needed
  if (this->is_game() && this->is_ep3()) {
    if (this->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
      auto watched_l = this->watched_lobby.lock();
      if (watched_l) {
        send_ep3_update_game_metadata(watched_l);
      }
    } else {
      send_ep3_update_game_metadata(this->shared_from_this());
    }
  }

  // There is a player in the lobby, so it is no longer idle
  if (event_pending(this->idle_timeout_event.get(), EV_TIMEOUT, nullptr)) {
    event_del(this->idle_timeout_event.get());
    this->log.info("Idle timeout cancelled");
  }
}

void Lobby::remove_client(shared_ptr<Client> c) {
  if (this->clients.at(c->lobby_client_id) != c) {
    auto other_c = this->clients[c->lobby_client_id].get();
    throw logic_error(phosg::string_printf(
        "client\'s lobby client id (%hhu) does not match client list (%u)",
        c->lobby_client_id,
        static_cast<uint8_t>(other_c ? other_c->lobby_client_id : 0xFF)));
  }
  this->clients[c->lobby_client_id] = nullptr;

  // Unassign the client's lobby if it matches the current lobby (it may not
  // match if the client was already added to another lobby - this can happen
  // during the lobby change procedure)
  {
    auto c_lobby = c->lobby.lock();
    if (c_lobby.get() == this) {
      c->lobby.reset();
    }
  }

  this->reassign_leader_on_client_departure(c->lobby_client_id);

  // If the lobby is recording a battle record, add the player leave event
  if (this->battle_record) {
    this->battle_record->delete_player(c->lobby_client_id);
  }

  // If the lobby is Episode 3, update the appropriate spectator counts
  if (this->is_game() && this->is_ep3()) {
    if (this->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
      auto watched_l = this->watched_lobby.lock();
      if (watched_l) {
        send_ep3_update_game_metadata(watched_l);
      }
    } else {
      send_ep3_update_game_metadata(this->shared_from_this());
    }
  }

  // If there are still players left in the lobby, delete all items that only
  // the leaving player could see. Don't do this if no one is left in the lobby,
  // since that would mean items could not persist in empty lobbies.
  uint16_t remaining_clients_mask = 0;
  for (size_t z = 0; z < 12; z++) {
    if (this->clients[z]) {
      remaining_clients_mask |= (1 << z);
    }
  }
  if (remaining_clients_mask) {
    for (auto& m : this->floor_item_managers) {
      m.clear_inaccessible(remaining_clients_mask);
    }
  } else {
    for (auto& m : this->floor_item_managers) {
      m.clear_private();
    }
  }

  if (!remaining_clients_mask &&
      this->check_flag(Flag::PERSISTENT) &&
      !this->check_flag(Flag::DEFAULT) &&
      (this->idle_timeout_usecs > 0)) {
    // If the lobby is persistent but has an idle timeout, make it expire after
    // the specified time
    auto tv = phosg::usecs_to_timeval(this->idle_timeout_usecs);
    event_add(this->idle_timeout_event.get(), &tv);
    this->log.info("Idle timeout scheduled");
  }
}

void Lobby::move_client_to_lobby(
    shared_ptr<Lobby> dest_lobby,
    shared_ptr<Client> c,
    ssize_t required_client_id) {
  if (dest_lobby.get() == this) {
    return;
  }

  if (required_client_id >= 0) {
    if (dest_lobby->clients.at(required_client_id)) {
      throw out_of_range("required slot is in use");
    }
  } else {
    ssize_t min_client_id = this->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 4 : 0;
    size_t available_slots = dest_lobby->max_clients - min_client_id;
    if (dest_lobby->count_clients() >= available_slots) {
      throw out_of_range("no space left in lobby");
    }
  }

  this->remove_client(c);
  dest_lobby->add_client(c, required_client_id);
}

shared_ptr<Client> Lobby::find_client(const string* identifier, uint64_t account_id) {
  for (size_t x = 0; x < this->max_clients; x++) {
    auto lc = this->clients[x];
    if (!lc) {
      continue;
    }
    if (account_id && lc->login && (lc->login->account->account_id == account_id)) {
      return lc;
    }
    if (identifier && (lc->character()->disp.name.eq(*identifier, lc->language()))) {
      return lc;
    }
  }

  throw out_of_range("client not found");
}

Lobby::JoinError Lobby::join_error_for_client(std::shared_ptr<Client> c, const std::string* password) const {
  if (this->count_clients() >= this->max_clients) {
    return JoinError::FULL;
  }
  bool debug_enabled = c->config.check_flag(Client::Flag::DEBUG_ENABLED);
  if (!this->version_is_allowed(c->version()) && !debug_enabled) {
    return JoinError::VERSION_CONFLICT;
  }
  if (this->is_game()) {
    if (this->check_flag(Flag::QUEST_SELECTION_IN_PROGRESS)) {
      return JoinError::QUEST_SELECTION_IN_PROGRESS;
    }
    if (this->check_flag(Flag::QUEST_IN_PROGRESS)) {
      return JoinError::QUEST_IN_PROGRESS;
    }
    if (this->check_flag(Flag::BATTLE_IN_PROGRESS)) {
      return JoinError::BATTLE_IN_PROGRESS;
    }
    if (this->mode == GameMode::SOLO) {
      return JoinError::SOLO;
    }
    if (!debug_enabled &&
        (this->check_flag(Flag::IS_CLIENT_CUSTOMIZATION) != c->config.check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION))) {
      return JoinError::VERSION_CONFLICT;
    }
    if (!c->login->account->check_flag(Account::Flag::FREE_JOIN_GAMES)) {
      if (password && !this->password.empty() && (*password != this->password)) {
        return JoinError::INCORRECT_PASSWORD;
      }
      auto p = c->character();
      if (p->disp.stats.level < this->min_level) {
        return JoinError::LEVEL_TOO_LOW;
      }
      if (p->disp.stats.level > this->max_level) {
        return JoinError::LEVEL_TOO_HIGH;
      }
      if (this->quest) {
        size_t num_clients = this->count_clients() + 1;
        bool v1_present = is_v1(c->version()) || this->any_v1_clients_present();
        auto this_sh = this->shared_from_this();
        if (!c->can_see_quest(this->quest, this_sh, this->event, this->difficulty, num_clients, v1_present) ||
            !c->can_play_quest(this->quest, this_sh, this->event, this->difficulty, num_clients, v1_present)) {
          return JoinError::NO_ACCESS_TO_QUEST;
        }
      }
    }
    // Only prevent joining during loading if the client is actually trying to
    // join (not just loading the game list)
    if (password && this->any_client_loading()) {
      return JoinError::LOADING;
    }
  }
  return JoinError::ALLOWED;
}

bool Lobby::item_exists(uint8_t floor, uint32_t item_id) const {
  if (floor >= this->floor_item_managers.size()) {
    return false;
  }
  return this->floor_item_managers.at(floor).exists(item_id);
}

shared_ptr<Lobby::FloorItem> Lobby::find_item(uint8_t floor, uint32_t item_id) const {
  return this->floor_item_managers.at(floor).find(item_id);
}

void Lobby::add_item(
    uint8_t floor,
    const ItemData& data,
    const VectorXZF& pos,
    std::shared_ptr<const MapState::ObjectState> from_obj,
    std::shared_ptr<const MapState::EnemyState> from_ene,
    uint16_t flags) {
  auto& m = this->floor_item_managers.at(floor);
  m.add(data, pos, from_obj, from_ene, flags);
  this->evict_items_from_floor(floor);
}

void Lobby::add_item(uint8_t floor, shared_ptr<FloorItem> fi) {
  auto& m = this->floor_item_managers.at(floor);
  m.add(fi);
  this->evict_items_from_floor(floor);
}

void Lobby::evict_items_from_floor(uint8_t floor) {
  auto& m = this->floor_item_managers.at(floor);
  auto evicted = m.evict();
  if (!evicted.empty()) {
    auto l = this->shared_from_this();
    for (const auto& fi : evicted) {
      for (size_t z = 0; z < 12; z++) {
        auto lc = this->clients[z];
        if (lc && fi->visible_to_client(z)) {
          send_destroy_floor_item_to_client(lc, fi->data.id, floor);
        }
      }
    }
  }
}

shared_ptr<Lobby::FloorItem> Lobby::remove_item(uint8_t floor, uint32_t item_id, uint8_t requesting_client_id) {
  return this->floor_item_managers.at(floor).remove(item_id, requesting_client_id);
}

uint32_t Lobby::generate_item_id(uint8_t client_id) {
  if (client_id < this->max_clients) {
    return this->next_item_id_for_client[client_id]++;
  }
  return this->next_game_item_id++;
}

void Lobby::on_item_id_generated_externally(uint32_t item_id) {
  // Note: The client checks for the range (0x00010000, 0x02010000) here, but
  // server-side item drop logic uses 0x00810000 as its base ID, so we restrict
  // the range further here.
  if ((item_id > 0x00010000) && (item_id < 0x00810000)) {
    uint16_t item_client_id = (item_id >> 21) & 0x7FF;
    uint32_t& next_item_id = this->next_item_id_for_client.at(item_client_id);
    next_item_id = std::max<uint32_t>(next_item_id, item_id + 1);
  }
}

void Lobby::assign_inventory_and_bank_item_ids(shared_ptr<Client> c, bool consume_ids) {
  auto p = c->character();
  uint32_t orig_next_item_id = this->next_item_id_for_client.at(c->lobby_client_id);
  for (size_t z = 0; z < p->inventory.num_items; z++) {
    p->inventory.items[z].data.id = this->generate_item_id(c->lobby_client_id);
  }
  if (!consume_ids) {
    this->next_item_id_for_client[c->lobby_client_id] = orig_next_item_id;
  }

  if (c->log.info("Assigned inventory item IDs%s", consume_ids ? "" : " but did not mark IDs as used")) {
    c->print_inventory(stderr);
    auto& bank = c->current_bank();
    if (p->bank.num_items) {
      bank.assign_ids(0x99000000 + (c->lobby_client_id << 20));
      c->log.info("Assigned bank item IDs");
      c->print_bank(stderr);
    } else {
      c->log.info("Bank is empty");
    }
  }
}

unordered_map<uint32_t, shared_ptr<Client>> Lobby::clients_by_account_id() const {
  unordered_map<uint32_t, shared_ptr<Client>> ret;
  for (auto c : this->clients) {
    if (c && c->login) {
      ret.emplace(c->login->account->account_id, c);
    }
  }
  return ret;
}

QuestIndex::IncludeCondition Lobby::quest_include_condition() const {
  size_t num_players = this->count_clients();
  bool v1_present = this->any_v1_clients_present();
  return [this, num_players, v1_present](shared_ptr<const Quest> q) -> QuestIndex::IncludeState {
    bool is_enabled = true;
    for (const auto& lc : this->clients) {
      auto this_sh = this->shared_from_this();
      if (lc && !lc->can_see_quest(q, this_sh, this->event, this->difficulty, num_players, v1_present)) {
        return QuestIndex::IncludeState::HIDDEN;
      }
      if (lc && !lc->can_play_quest(q, this_sh, this->event, this->difficulty, num_players, v1_present)) {
        is_enabled = false;
      }
    }
    return is_enabled ? QuestIndex::IncludeState::AVAILABLE : QuestIndex::IncludeState::DISABLED;
  };
}

void Lobby::dispatch_on_idle_timeout(evutil_socket_t, short, void* ctx) {
  auto l = reinterpret_cast<Lobby*>(ctx)->shared_from_this();
  if (l->count_clients() == 0) {
    l->log.info("Idle timeout expired");
    auto s = l->require_server_state();
    s->remove_lobby(l);
  } else {
    l->log.error("Idle timeout occurred, but clients are present in lobby");
    event_del(l->idle_timeout_event.get());
  }
}

bool Lobby::compare_shared(const shared_ptr<const Lobby>& a, const shared_ptr<const Lobby>& b) {
  // Sort keys:
  // 1. Priority class: has free space < empty (persistent) < full < non-joinable (in quest/battle)
  // 2. Password: public < locked
  // 3. Game mode: Normal < Battle < Challenge < Solo
  // 4. Episode: 1 < 2 < 4
  // 5. Difficulty: Normal < Hard < Very Hard < Ultimate
  // 6. Game name
  static auto get_priority = +[](const shared_ptr<const Lobby>& l) -> size_t {
    if (l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS) ||
        l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) ||
        l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS)) {
      return 4;
    }
    size_t num_clients = l->count_clients();
    if (num_clients == l->max_clients) {
      return 3;
    }
    if (num_clients == 0) {
      return 2;
    }
    return 1;
  };
  size_t a_priority = get_priority(a);
  size_t b_priority = get_priority(b);
  if (a_priority < b_priority) {
    return true;
  } else if (a_priority > b_priority) {
    return false;
  }

  if (a->password.empty() && !b->password.empty()) {
    return true;
  } else if (!a->password.empty() && b->password.empty()) {
    return false;
  }

  size_t a_mode = static_cast<size_t>(a->mode);
  size_t b_mode = static_cast<size_t>(b->mode);
  if (a_mode < b_mode) {
    return true;
  } else if (a_mode > b_mode) {
    return false;
  }

  size_t a_episode = static_cast<size_t>(a->episode);
  size_t b_episode = static_cast<size_t>(b->episode);
  if (a_episode < b_episode) {
    return true;
  } else if (a_episode > b_episode) {
    return false;
  }

  if (a->difficulty < b->difficulty) {
    return true;
  } else if (a->difficulty > b->difficulty) {
    return false;
  }

  return a->name < b->name;
}

template <>
Lobby::DropMode phosg::enum_for_name<Lobby::DropMode>(const char* name) {
  if (!strcmp(name, "DISABLED")) {
    return Lobby::DropMode::DISABLED;
  } else if (!strcmp(name, "CLIENT")) {
    return Lobby::DropMode::CLIENT;
  } else if (!strcmp(name, "SERVER_SHARED")) {
    return Lobby::DropMode::SERVER_SHARED;
  } else if (!strcmp(name, "SERVER_PRIVATE")) {
    return Lobby::DropMode::SERVER_PRIVATE;
  } else if (!strcmp(name, "SERVER_DUPLICATE")) {
    return Lobby::DropMode::SERVER_DUPLICATE;
  } else {
    throw runtime_error("invalid drop mode");
  }
}

template <>
const char* phosg::name_for_enum<Lobby::DropMode>(Lobby::DropMode value) {
  switch (value) {
    case Lobby::DropMode::DISABLED:
      return "DISABLED";
    case Lobby::DropMode::CLIENT:
      return "CLIENT";
    case Lobby::DropMode::SERVER_SHARED:
      return "SERVER_SHARED";
    case Lobby::DropMode::SERVER_PRIVATE:
      return "SERVER_PRIVATE";
    case Lobby::DropMode::SERVER_DUPLICATE:
      return "SERVER_DUPLICATE";
    default:
      throw runtime_error("invalid drop mode");
  }
}
