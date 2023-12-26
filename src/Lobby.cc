#include "Lobby.hh"

#include <string.h>

#include <phosg/Random.hh>

#include "Compression.hh"
#include "Loggers.hh"
#include "SendCommands.hh"
#include "Text.hh"

using namespace std;

bool Lobby::FloorItem::visible_to_client(uint8_t client_id) const {
  return this->visibility_flags & (1 << client_id);
}

Lobby::FloorItemManager::FloorItemManager(uint32_t lobby_id, uint8_t floor)
    : log(string_printf("[Lobby:%08" PRIX32 ":FloorItems:%02hhX] ", lobby_id, floor), lobby_log.min_level),
      next_drop_number(0) {}

bool Lobby::FloorItemManager::exists(uint32_t item_id) const {
  return this->items.count(item_id);
}

shared_ptr<Lobby::FloorItem> Lobby::FloorItemManager::find(uint32_t item_id) const {
  return this->items.at(item_id);
}

void Lobby::FloorItemManager::add(const ItemData& item, float x, float z, uint16_t visibility_flags) {
  auto fi = make_shared<FloorItem>();
  fi->data = item;
  fi->x = x;
  fi->z = z;
  fi->drop_number = this->next_drop_number++;
  fi->visibility_flags = visibility_flags & 0x0FFF;
  this->add(fi);
}

void Lobby::FloorItemManager::add(shared_ptr<Lobby::FloorItem> fi) {
  if (fi->visibility_flags == 0) {
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
  this->log.info("Added floor item %08" PRIX32 " at %g, %g with drop number %" PRIu64 " visible to %03hX",
      fi->data.id.load(), fi->x, fi->z, fi->drop_number, fi->visibility_flags);
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
  this->log.info("Removed floor item %08" PRIX32 " at %g, %g with drop number %" PRIu64 " visible to %03hX",
      fi->data.id.load(), fi->x, fi->z, fi->drop_number, fi->visibility_flags);
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
    if ((it.second->visibility_flags & remaining_clients_mask) == 0) {
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
    if ((it.second->visibility_flags & 0x00F) != 0x00F) {
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
  unordered_map<uint32_t, shared_ptr<FloorItem>> old_items;
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

Lobby::Lobby(shared_ptr<ServerState> s, uint32_t id)
    : server_state(s),
      log(string_printf("[Lobby:%" PRIX32 "] ", id), lobby_log.min_level),
      lobby_id(id),
      min_level(0),
      max_level(0xFFFFFFFF),
      next_game_item_id(0xCC000000),
      base_version(Version::GC_V3),
      allowed_versions(0x0000),
      section_id(0),
      episode(Episode::NONE),
      mode(GameMode::NORMAL),
      difficulty(0),
      base_exp_multiplier(1),
      challenge_exp_multiplier(1.0f),
      random_seed(random_object<uint32_t>()),
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
  for (size_t x = 0; x < 12; x++) {
    this->next_item_id_for_client[x] = 0x00010000 + 0x00200000 * x;
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

void Lobby::set_drop_mode(DropMode new_mode) {
  this->drop_mode = new_mode;

  bool should_have_item_creator = (this->base_version == Version::BB_V4) ||
      ((new_mode != DropMode::DISABLED) && (new_mode != DropMode::CLIENT));
  if (should_have_item_creator && !this->item_creator) {
    this->create_item_creator();
  } else if (!should_have_item_creator && this->item_creator) {
    this->item_creator.reset();
  }
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
  this->map = make_shared<Map>(this->lobby_id, this->random_crypt);

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

  // If the lobby is a game and there was no one in it, reassign all the floor
  // item IDs and reset the next item IDs
  if (this->is_game()) {
    if (leader_index >= this->max_clients) {
      for (size_t x = 0; x < 12; x++) {
        this->next_item_id_for_client[x] = 0x00010000 + 0x00200000 * x;
      }
      this->next_game_item_id = 0xCC000000;

      // Reassign all floor item IDs so they won't conflict with any players'
      // item IDs
      for (auto& m : this->floor_item_managers) {
        this->next_game_item_id = m.reassign_all_item_ids(this->next_game_item_id);
      }
    }
    this->assign_inventory_and_bank_item_ids(c);
    // On BB, we send artificial flag state to fix an Episode 2 bug where the
    // CCA door lock state is overwritten by quests.
    if (c->version() == Version::BB_V4) {
      c->config.set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
    }
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

bool Lobby::item_exists(uint8_t floor, uint32_t item_id) const {
  if (floor >= this->floor_item_managers.size()) {
    return false;
  }
  return this->floor_item_managers.at(floor).exists(item_id);
}

shared_ptr<Lobby::FloorItem> Lobby::find_item(uint8_t floor, uint32_t item_id) const {
  return this->floor_item_managers.at(floor).find(item_id);
}

void Lobby::add_item(uint8_t floor, const ItemData& data, float x, float z, uint16_t visibility_flags) {
  auto& m = this->floor_item_managers.at(floor);
  m.add(data, x, z, visibility_flags);
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
  size_t num_players = this->count_clients();
  return [this, num_players](shared_ptr<const Quest> q) -> QuestIndex::IncludeState {
    bool is_enabled = true;
    for (const auto& lc : this->clients) {
      if (lc && !lc->can_see_quest(q, this->difficulty, num_players)) {
        return QuestIndex::IncludeState::HIDDEN;
      }
      if (lc && !lc->can_play_quest(q, this->difficulty, num_players)) {
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
    if (l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS) || l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS)) {
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
Lobby::DropMode enum_for_name<Lobby::DropMode>(const char* name) {
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
const char* name_for_enum<Lobby::DropMode>(Lobby::DropMode value) {
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
