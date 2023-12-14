#include "Lobby.hh"

#include <string.h>

#include <phosg/Random.hh>

#include "Compression.hh"
#include "Loggers.hh"
#include "SendCommands.hh"
#include "Text.hh"

using namespace std;

Lobby::Lobby(shared_ptr<ServerState> s, uint32_t id)
    : server_state(s),
      log(string_printf("[Lobby:%" PRIX32 "] ", id), lobby_log.min_level),
      lobby_id(id),
      min_level(0),
      max_level(0xFFFFFFFF),
      next_game_item_id(0x00810000),
      base_version(Version::GC_V3),
      allowed_versions(0x0000),
      section_id(0),
      episode(Episode::NONE),
      mode(GameMode::NORMAL),
      difficulty(0),
      base_exp_multiplier(1),
      challenge_exp_multiplier(1.0f),
      random_seed(random_object<uint32_t>()),
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
  for (size_t x = 0; x < 12; x++) {
    this->next_item_id[x] = 0x00010000 + 0x00200000 * x;
  }
}

Lobby::~Lobby() {
  this->log.info("Deleted");
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

void Lobby::create_item_creator() {
  auto s = this->require_server_state();

  shared_ptr<const RareItemSet> rare_item_set;
  shared_ptr<const CommonItemSet> common_item_set;
  switch (this->base_version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::GC_EP3_TRIAL_EDITION:
    case Version::GC_EP3:
      throw runtime_error("cannot create item creator for this base version");
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
      // TODO: We should probably have a v1 common item set at some point too
      common_item_set = s->common_item_set_v2;
      rare_item_set = s->rare_item_sets.at("rare-table-v1");
      break;
    case Version::DC_V2:
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
      s->item_parameter_table_for_version(this->base_version),
      this->base_version,
      this->episode,
      (this->mode == GameMode::SOLO) ? GameMode::NORMAL : this->mode,
      this->difficulty,
      this->section_id,
      this->random_seed,
      this->quest ? this->quest->battle_rules : nullptr);
}

void Lobby::load_maps() {
  auto s = this->require_server_state();
  this->map = make_shared<Map>(this->lobby_id);

  if (this->quest) {
    auto leader_c = this->clients.at(this->leader_id);
    if (!leader_c) {
      throw logic_error("lobby leader is missing");
    }

    auto vq = this->quest->version(Version::BB_V4, leader_c->language());
    auto dat_contents = prs_decompress(*vq->dat_contents);
    this->map->clear();
    this->map->add_enemies_and_objects_from_quest_data(
        this->episode,
        this->difficulty,
        this->event,
        dat_contents.data(),
        dat_contents.size(),
        this->random_seed,
        this->rare_enemy_rates ? this->rare_enemy_rates : Map::NO_RARE_ENEMIES);
    if (this->item_creator) {
      this->item_creator->clear_destroyed_entities();
    }

  } else { // No quest loaded
    for (size_t floor = 0; floor < 0x10; floor++) {
      this->log.info("[Map/%zu] Using variations %" PRIX32 ", %" PRIX32,
          floor, this->variations[floor * 2].load(), this->variations[floor * 2 + 1].load());

      auto enemy_filenames = map_filenames_for_variation(
          this->episode,
          (this->mode == GameMode::SOLO),
          floor,
          this->variations[floor * 2],
          this->variations[floor * 2 + 1],
          true);
      if (enemy_filenames.empty()) {
        this->log.info("[Map/%zu:e] No file to load", floor);
      } else {
        bool any_map_loaded = false;
        for (const string& filename : enemy_filenames) {
          try {
            auto map_data = s->load_bb_file(filename, "", "map/" + filename);
            this->map->add_enemies_from_map_data(
                this->episode,
                this->difficulty,
                this->event,
                floor,
                map_data->data(),
                map_data->size(),
                this->rare_enemy_rates);
            any_map_loaded = true;
            break;
          } catch (const exception& e) {
            this->log.info("[Map/%zu:e] Failed to load %s: %s", floor, filename.c_str(), e.what());
          }
        }
        if (!any_map_loaded) {
          throw runtime_error(string_printf("no enemy maps loaded for floor %zu", floor));
        }
      }

      auto object_filenames = map_filenames_for_variation(
          this->episode,
          (this->mode == GameMode::SOLO),
          floor,
          this->variations[floor * 2],
          this->variations[floor * 2 + 1],
          false);
      if (object_filenames.empty()) {
        this->log.info("[Map/%zu:o] No file to load", floor);
      } else {
        bool any_map_loaded = false;
        for (const string& filename : object_filenames) {
          try {
            auto map_data = s->load_bb_file(filename, "", "map/" + filename);
            this->map->add_objects_from_map_data(floor, map_data->data(), map_data->size());
            any_map_loaded = true;
            break;
          } catch (const exception& e) {
            this->log.info("[Map/%zu:o] Failed to load %s: %s", floor, filename.c_str(), e.what());
          }
        }
        if (!any_map_loaded) {
          throw runtime_error(string_printf("no object maps loaded for floor %zu", floor));
        }
      }
    }
  }

  this->log.info("Generated objects list (%zu entries):", this->map->objects.size());
  for (size_t z = 0; z < this->map->objects.size(); z++) {
    string o_str = this->map->objects[z].str(s->item_name_index);
    this->log.info("(K-%zX) %s", z, o_str.c_str());
  }
  this->log.info("Generated enemies list (%zu entries):", this->map->enemies.size());
  for (size_t z = 0; z < this->map->enemies.size(); z++) {
    string e_str = this->map->enemies[z].str();
    this->log.info("(E-%zX) %s", z, e_str.c_str());
  }
  this->log.info("Loaded maps contain %zu object entries and %zu enemy entries overall (%zu as rares)",
      this->map->objects.size(), this->map->enemies.size(), this->map->rare_enemy_indexes.size());

  if (this->item_creator) {
    this->item_creator->clear_destroyed_entities();
  }
}

void Lobby::create_ep3_server() {
  auto s = this->require_server_state();
  if (!this->ep3_server) {
    this->log.info("Creating Episode 3 server state");
  } else {
    this->log.info("Recreating Episode 3 server state");
  }
  auto tourn = this->tournament_match ? this->tournament_match->tournament.lock() : nullptr;
  bool is_trial = this->base_version == Version::GC_EP3_TRIAL_EDITION;
  Episode3::Server::Options options = {
      .card_index = is_trial ? s->ep3_card_index_trial : s->ep3_card_index,
      .map_index = s->ep3_map_index,
      .behavior_flags = s->ep3_behavior_flags,
      .random_crypt = this->random_crypt,
      .tournament = tourn,
      .trap_card_ids = s->ep3_trap_card_ids,
  };
  this->ep3_server = make_shared<Episode3::Server>(this->shared_from_this(), std::move(options));
  this->ep3_server->init();
}

void Lobby::reassign_leader_on_client_departure(size_t leaving_client_index) {
  for (size_t x = 0; x < this->max_clients; x++) {
    if (x == leaving_client_index) {
      continue;
    }
    if (this->clients[x].get()) {
      this->leader_id = x;
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
    if (this->clients[x].get()) {
      ret++;
    }
  }
  return ret;
}

void Lobby::add_client(shared_ptr<Client> c, ssize_t required_client_id) {
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
  }

  // If the lobby is a game and item tracking is enabled, assign the inventory's
  // item IDs. If there was no one else in the lobby, reset all the next item
  // IDs also
  if (this->is_game() && this->check_flag(Lobby::Flag::ITEM_TRACKING_ENABLED)) {
    if (leader_index >= this->max_clients) {
      for (size_t x = 0; x < 12; x++) {
        this->next_item_id[x] = 0x00010000 + 0x00200000 * x;
      }
      this->next_game_item_id = 0x00810000;

      // Reassign all floor item IDs so they won't conflict with any players'
      // item IDs
      unordered_map<uint32_t, FloorItem> new_item_id_to_floor_item;
      for (const auto& it : this->item_id_to_floor_item) {
        uint32_t new_item_id = this->generate_item_id(0xFF);
        auto& new_fi = new_item_id_to_floor_item.emplace(new_item_id, it.second).first->second;
        new_fi.data.id = new_item_id;
      }
      this->item_id_to_floor_item = std::move(new_item_id_to_floor_item);
    }
    this->assign_inventory_and_bank_item_ids(c);
  }

  // If the lobby is recording a battle record, add the player join event
  if (this->battle_record) {
    auto p = c->character();
    PlayerLobbyDataDCGC lobby_data;
    lobby_data.player_tag = 0x00010000;
    lobby_data.guild_card_number = c->license->serial_number;
    lobby_data.name.encode(p->disp.name.decode(c->language()), c->language());
    this->battle_record->add_player(
        lobby_data,
        p->inventory,
        p->disp.to_dcpcv3(c->language(), c->language()),
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
    throw logic_error(string_printf(
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

  // If the lobby is persistent but has an idle timeout, make it expire after
  // the specified time
  if ((this->count_clients() == 0) && this->check_flag(Flag::PERSISTENT) && (this->idle_timeout_usecs > 0)) {
    auto tv = usecs_to_timeval(this->idle_timeout_usecs);
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

shared_ptr<Client> Lobby::find_client(const string* identifier, uint64_t serial_number) {
  for (size_t x = 0; x < this->max_clients; x++) {
    auto lc = this->clients[x];
    if (!lc) {
      continue;
    }
    if (serial_number && lc->license &&
        (lc->license->serial_number == serial_number)) {
      return lc;
    }
    if (identifier && (lc->character()->disp.name.eq(*identifier, lc->language()))) {
      return lc;
    }
  }

  throw out_of_range("client not found");
}

uint8_t Lobby::game_event_for_lobby_event(uint8_t lobby_event) {
  if (lobby_event > 7) {
    return 0;
  }
  if (lobby_event == 7) {
    return 2;
  }
  if (lobby_event == 2) {
    return 0;
  }
  return lobby_event;
}

bool Lobby::item_exists(uint32_t item_id) const {
  return this->item_id_to_floor_item.count(item_id);
}

const Lobby::FloorItem& Lobby::find_item(uint32_t item_id) const {
  return this->item_id_to_floor_item.at(item_id);
}

void Lobby::add_item(const ItemData& data, uint8_t floor, float x, float z) {
  auto& fi = this->item_id_to_floor_item[data.id];
  fi.data = data;
  fi.floor = floor;
  fi.x = x;
  fi.z = z;
}

ItemData Lobby::remove_item(uint32_t item_id) {
  auto item_it = this->item_id_to_floor_item.find(item_id);
  if (item_it == this->item_id_to_floor_item.end()) {
    throw out_of_range("item not present");
  }
  ItemData ret = item_it->second.data;
  this->item_id_to_floor_item.erase(item_it);
  return ret;
}

uint32_t Lobby::generate_item_id(uint8_t client_id) {
  if (client_id < this->max_clients) {
    return this->next_item_id[client_id]++;
  }
  return this->next_game_item_id++;
}

void Lobby::on_item_id_generated_externally(uint32_t item_id) {
  // Note: The client checks for the range (0x00010000, 0x02010000) here, but
  // server-side item drop logic uses 0x00810000 as its base ID, so we restrict
  // the range further here.
  if ((item_id > 0x00010000) && (item_id < 0x00810000)) {
    uint16_t item_client_id = (item_id >> 21) & 0x7FF;
    uint32_t& next_item_id = this->next_item_id.at(item_client_id);
    next_item_id = std::max<uint32_t>(next_item_id, item_id + 1);
  }
}

void Lobby::assign_inventory_and_bank_item_ids(shared_ptr<Client> c) {
  auto p = c->character();
  for (size_t z = 0; z < p->inventory.num_items; z++) {
    p->inventory.items[z].data.id = this->generate_item_id(c->lobby_client_id);
  }
  if (c->log.info("Assigned inventory item IDs")) {
    p->print_inventory(stderr, c->version(), c->require_server_state()->item_name_index);
    if (p->bank.num_items) {
      p->bank.assign_ids(0x99000000 + (c->lobby_client_id << 20));
      c->log.info("Assigned bank item IDs");
      p->print_bank(stderr, c->version(), c->require_server_state()->item_name_index);
    } else {
      c->log.info("Bank is empty");
    }
  }
}

unordered_map<uint32_t, shared_ptr<Client>> Lobby::clients_by_serial_number() const {
  unordered_map<uint32_t, shared_ptr<Client>> ret;
  for (auto c : this->clients) {
    if (c) {
      ret.emplace(c->license->serial_number, c);
    }
  }
  return ret;
}

QuestIndex::IncludeCondition Lobby::quest_include_condition() const {
  return [this](shared_ptr<const Quest> q) -> QuestIndex::IncludeState {
    bool is_enabled = true;
    for (const auto& lc : this->clients) {
      if (lc && !lc->can_see_quest(q, this->difficulty)) {
        return QuestIndex::IncludeState::HIDDEN;
      }
      if (lc && !lc->can_play_quest(q, this->difficulty)) {
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
