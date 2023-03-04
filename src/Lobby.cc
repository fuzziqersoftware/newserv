#include "Lobby.hh"

#include <string.h>

#include <phosg/Random.hh>

#include "Loggers.hh"
#include "SendCommands.hh"
#include "Text.hh"

using namespace std;



Lobby::Lobby(uint32_t id)
  : log(string_printf("[Lobby/%" PRIX32 "] ", id), lobby_log.min_level),
    lobby_id(id),
    min_level(0),
    max_level(0xFFFFFFFF),
    next_game_item_id(0x00810000),
    version(GameVersion::GC),
    section_id(0),
    episode(Episode::NONE),
    mode(GameMode::NORMAL),
    difficulty(0),
    random_seed(random_object<uint32_t>()),
    random(new mt19937(this->random_seed)),
    event(0),
    block(0),
    type(0),
    leader_id(0),
    max_clients(12),
    flags(0) {
  for (size_t x = 0; x < 12; x++) {
    this->next_item_id[x] = 0x00010000 + 0x00200000 * x;
  }
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
    if (!this->clients[x].get()) {
      continue;
    }
    if (this->clients[x]->flags & (Client::Flag::LOADING | Client::Flag::LOADING_QUEST)) {
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
  ssize_t min_client_id = (this->flags & Lobby::Flag::IS_SPECTATOR_TEAM) ? 4 : 0;

  if (required_client_id >= 0) {
    if (this->clients[required_client_id].get()) {
      throw out_of_range("required slot is in use");
    }
    this->clients[required_client_id] = c;
    index = required_client_id;

  } else if (c->options.debug) {
    for (index = max_clients - 1; index >= min_client_id; index--) {
      if (!this->clients[index].get()) {
        this->clients[index] = c;
        break;
      }
    }
    if (index < min_client_id) {
      throw out_of_range("no space left in lobby");
    }
  } else {
    for (index = min_client_id; index < max_clients; index++) {
      if (!this->clients[index].get()) {
        this->clients[index] = c;
        break;
      }
    }
    if (index >= max_clients) {
      throw out_of_range("no space left in lobby");
    }
  }

  c->lobby_client_id = index;
  c->lobby_id = this->lobby_id;

  // If there's no one else in the lobby, set the leader id as well
  size_t leader_index;
  for (leader_index = 0; leader_index < max_clients; leader_index++) {
    if (this->clients[leader_index] && (this->clients[leader_index] != c)) {
      break;
    }
  }
  if (leader_index >= max_clients) {
    this->leader_id = c->lobby_client_id;
  }

  // If the lobby is a game and item tracking is enabled, assign the inventory's
  // item IDs
  if (this->is_game() && (this->flags & Lobby::Flag::ITEM_TRACKING_ENABLED)) {
    auto& inv = c->game_data.player()->inventory;
    size_t count = min<uint8_t>(inv.num_items, 30);
    for (size_t x = 0; x < count; x++) {
      inv.items[x].data.id = this->generate_item_id(c->lobby_client_id);
    }
    c->game_data.player()->print_inventory(stderr);
  }

  // If the lobby is recording a battle record, add the player join event
  if (this->battle_record) {
    PlayerLobbyDataDCGC lobby_data;
    lobby_data.player_tag = 0x00010000;
    lobby_data.guild_card = c->license->serial_number;
    lobby_data.name = encode_sjis(c->game_data.player()->disp.name);
    this->battle_record->add_player(lobby_data,
        c->game_data.player()->inventory,
        c->game_data.player()->disp.to_dcpcv3());
  }

  // Send spectator count notifications if needed
  if (this->is_game() && this->is_ep3()) {
    if (this->flags & Lobby::Flag::IS_SPECTATOR_TEAM) {
      auto watched_l = this->watched_lobby.lock();
      if (watched_l) {
        send_ep3_update_spectator_count(watched_l);
      }
    } else {
      send_ep3_update_spectator_count(this->shared_from_this());
    }
  }
}

void Lobby::remove_client(shared_ptr<Client> c) {
  if (this->clients[c->lobby_client_id] != c) {
    auto other_c = this->clients[c->lobby_client_id].get();
    throw logic_error(string_printf(
        "client\'s lobby client id (%hhu) does not match client list (%u)",
        c->lobby_client_id,
        static_cast<uint8_t>(other_c ? other_c->lobby_client_id : 0xFF)));
  }

  this->clients[c->lobby_client_id] = nullptr;

  // Unassign the client's lobby if it matches the current lobby's id (it may
  // not match if the client was already added to another lobby - this can
  // happen during the lobby change procedure)
  if (c->lobby_id == this->lobby_id) {
    c->lobby_id = 0;
  }

  this->reassign_leader_on_client_departure(c->lobby_client_id);

  // If the lobby is recording a battle record, add the player leave event
  if (this->battle_record) {
    this->battle_record->delete_player(c->lobby_client_id);
  }

  // If the lobby is Episode 3, update the appropriate spectator counts
  if (this->is_game() && this->is_ep3()) {
    if (this->flags & Lobby::Flag::IS_SPECTATOR_TEAM) {
      auto watched_l = this->watched_lobby.lock();
      if (watched_l) {
        send_ep3_update_spectator_count(watched_l);
      }
    } else {
      send_ep3_update_spectator_count(this->shared_from_this());
    }
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
    if (dest_lobby->clients[required_client_id]) {
      throw out_of_range("required slot is in use");
    }
  } else {
    ssize_t min_client_id = (this->flags & Lobby::Flag::IS_SPECTATOR_TEAM) ? 4 : 0;
    size_t available_slots = dest_lobby->max_clients - min_client_id;
    if (dest_lobby->count_clients() >= available_slots) {
      throw out_of_range("no space left in lobby");
    }
  }

  this->remove_client(c);
  dest_lobby->add_client(c, required_client_id);
}



shared_ptr<Client> Lobby::find_client(const u16string* identifier,
    uint64_t serial_number) {
  for (size_t x = 0; x < this->max_clients; x++) {
    if (!this->clients[x]) {
      continue;
    }
    if (serial_number && this->clients[x]->license &&
        (this->clients[x]->license->serial_number == serial_number)) {
      return this->clients[x];
    }
    if (identifier && (this->clients[x]->game_data.player()->disp.name == *identifier)) {
      return this->clients[x];
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



void Lobby::add_item(const PlayerInventoryItem& item, uint8_t area, float x, float z) {
  auto& fi = this->item_id_to_floor_item[item.data.id];
  fi.inv_item = item;
  fi.area = area;
  fi.x = x;
  fi.z = z;
}

PlayerInventoryItem Lobby::remove_item(uint32_t item_id) {
  auto item_it = this->item_id_to_floor_item.find(item_id);
  if (item_it == this->item_id_to_floor_item.end()) {
    throw out_of_range("item not present");
  }
  PlayerInventoryItem ret = move(item_it->second.inv_item);
  this->item_id_to_floor_item.erase(item_it);
  return ret;
}

uint32_t Lobby::generate_item_id(uint8_t client_id) {
  if (client_id < this->max_clients) {
    return this->next_item_id[client_id]++;
  }
  return this->next_game_item_id++;
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
