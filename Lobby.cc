#include "Lobby.hh"

#include <string.h>

#include "SendCommands.hh"
#include "Text.hh"

using namespace std;



Lobby::Lobby() : lobby_id(0), min_level(0), max_level(0xFFFFFFFF),
    next_game_item_id(0), version(GameVersion::GC), section_id(0), episode(1),
    difficulty(0), mode(0), event(0), block(0), type(0), leader_id(0),
    max_clients(12), flags(0), loading_quest_id(0) {

  for (size_t x = 0; x < 12; x++) {
    this->next_item_id[x] = 0;
  }
  memset(&this->next_drop_item, 0, sizeof(this->next_drop_item));
  memset(this->variations, 0, 0x20 * sizeof(this->variations[0]));
  memset(this->password, 0, 36 * sizeof(this->password[0]));
  memset(this->name, 0, 36 * sizeof(this->name[0]));
}

bool Lobby::is_game() const {
  return this->flags & LobbyFlag::IsGame;
}

void Lobby::reassign_leader_on_client_departure_locked(size_t leaving_client_index) {
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
  rw_guard g(this->lock, false);
  for (size_t x = 0; x < this->max_clients; x++) {
    if (!this->clients[x].get()) {
      continue;
    }
    if (this->clients[x]->flags & ClientFlag::Loading) {
      return true;
    }
  }
  return false;
}

size_t Lobby::count_clients_locked() const {
  size_t ret = 0;
  for (size_t x = 0; x < this->max_clients; x++) {
    if (this->clients[x].get()) {
      ret++;
    }
  }
  return ret;
}

size_t Lobby::count_clients() const {
  rw_guard g(this->lock, false);
  return this->count_clients_locked();
}

void Lobby::add_client(shared_ptr<Client> c) {
  rw_guard g(this->lock, true);
  this->add_client_locked(c);
}

void Lobby::add_client_locked(shared_ptr<Client> c) {
  ssize_t index;
  for (index = this->max_clients - 1; index >= 0; index--) {
    if (!this->clients[index].get()) {
      this->clients[index] = c;
      break;
    }
  }
  if (index < 0) {
    throw out_of_range("no space left in lobby");
  }
  c->lobby_client_id = index;
  c->lobby_id = this->lobby_id;

  // if there's no one else in the lobby, set the leader id as well
  if (index == this->max_clients - 1) {
    for (index = this->max_clients - 2; index >= 0; index--) {
      if (this->clients[index].get()) {
        break;
      }
    }
    if (index < 0) {
      this->leader_id = c->lobby_client_id;
    }
  }
}

void Lobby::remove_client(shared_ptr<Client> c) {
  rw_guard g(this->lock, true);
  this->remove_client_locked(c);
}

void Lobby::remove_client_locked(shared_ptr<Client> c) {
  if (this->clients[c->lobby_client_id] != c) {
    auto other_c = this->clients[c->lobby_client_id].get();
    throw logic_error(string_printf(
        "client\'s lobby client id (%hhu) does not match client list (%hhu)",
        c->lobby_client_id, other_c ? other_c->lobby_client_id : 0xFF));
  }

  this->clients[c->lobby_client_id] = NULL;

  // unassign the client's lobby if it matches the current lobby's id (it may
  // not match if the client was already added to another lobby - this can
  // happen during the lobby change procedure)
  if (c->lobby_id == this->lobby_id) {
    c->lobby_id = 0;
  }

  this->reassign_leader_on_client_departure_locked(c->lobby_client_id);
}

void Lobby::move_client_to_lobby(shared_ptr<Lobby> dest_lobby,
    shared_ptr<Client> c) {
  if (dest_lobby.get() == this) {
    return;
  }

  // deadlock prevention: lock the lobbies in increasing order of memory address
  vector<rw_guard> guards;
  uint8_t* this_ptr = reinterpret_cast<uint8_t*>(this);
  uint8_t* dest_ptr = reinterpret_cast<uint8_t*>(dest_lobby.get());
  if (this_ptr < dest_ptr) {
    guards.emplace_back(this->lock, true);
    guards.emplace_back(dest_lobby->lock, true);
  } else {
    guards.emplace_back(dest_lobby->lock, true);
    guards.emplace_back(this->lock, true);
  }

  if (dest_lobby->count_clients_locked() >= dest_lobby->max_clients) {
    throw out_of_range("no space left in lobby");
  }

  this->remove_client_locked(c);
  dest_lobby->add_client_locked(c);
}



shared_ptr<Client> Lobby::find_client(const char16_t* identifier,
    uint64_t serial_number) {
  rw_guard g(this->lock, false);
  for (size_t x = 0; x < this->max_clients; x++) {
    if (!this->clients[x]) {
      continue;
    }
    if (serial_number && this->clients[x]->license &&
        (this->clients[x]->license->serial_number == serial_number)) {
      return this->clients[x];
    }
    if (identifier && !char16cmp(this->clients[x]->player.disp.name, identifier, 0x10)) {
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



void Lobby::add_item(const PlayerInventoryItem& item) {
  rw_guard g(this->lock, true);
  this->item_id_to_floor_item.emplace(item.data.item_id, item);
}

void Lobby::remove_item(uint32_t item_id, PlayerInventoryItem* item) {
  rw_guard g(this->lock, true);
  auto item_it = this->item_id_to_floor_item.find(item_id);
  if (item_it == this->item_id_to_floor_item.end()) {
    throw out_of_range("item not present");
  }
  *item = move(item_it->second);
  this->item_id_to_floor_item.erase(item_it);
}

uint32_t Lobby::generate_item_id(uint32_t client_id) {
  if (client_id < this->max_clients) {
    return this->next_item_id[client_id]++;
  }
  return this->next_game_item_id++;
}

void Lobby::assign_item_ids_for_player(uint32_t client_id, PlayerInventory& inv) {
  for (size_t x = 0; x < inv.num_items; x++) {
    inv.items[x].data.item_id = this->generate_item_id(client_id);
  }
}