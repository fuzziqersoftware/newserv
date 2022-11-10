#include "DeckState.hh"

using namespace std;

namespace Episode3 {



NameEntry::NameEntry() {
  this->clear();
}

void NameEntry::clear() {
  this->client_id = 0xFF;
  this->present = 0;
  this->unused_by_server = 0;
  this->unused = 0;
}



DeckEntry::DeckEntry() {
  this->clear();
}

void DeckEntry::clear() {
  this->team_id = 0xFFFFFFFF;
  this->god_whim_flag = 3;
  this->unused1 = 0;
  this->unused2 = 0;
  this->unused3 = 0;
  this->card_ids.clear(0xFFFF);
}



uint8_t index_for_card_ref(uint16_t card_ref) {
  return card_ref & 0xFF;
}

uint8_t client_id_for_card_ref(uint16_t card_ref) {
  return (card_ref >> 8) & 0xFF;
}



uint8_t DeckState::num_drawable_cards() const {
  return this->card_refs.size() - this->draw_index;
}

bool DeckState::set_card_ref_in_play(uint16_t card_ref) {
  if (!this->contains_card_ref(card_ref)) {
    return false;
  }
  uint8_t index = index_for_card_ref(card_ref);
  if (this->entries[index].state == CardState::IN_HAND) {
    this->entries[index].state = CardState::IN_PLAY;
    return true;
  } else {
    return false;
  }
}

bool DeckState::contains_card_ref(uint16_t card_ref) const {
  return index_for_card_ref(card_ref) < this->entries.size();
}

void DeckState::disable_loop() {
  this->loop_enabled = false;
}

void DeckState::disable_shuffle() {
  this->shuffle_enabled = false;
}

uint16_t DeckState::draw_card() {
  if (this->num_drawable_cards() == 0) {
    this->restart();
  }
  if (this->num_drawable_cards() == 0) {
    return 0xFFFF;
  }

  uint16_t ref = this->card_refs[this->draw_index++];
  this->entries[index_for_card_ref(ref)].state = CardState::IN_HAND;
  return ref;
}

bool DeckState::draw_card_by_ref(uint16_t card_ref) {
  if (card_ref == 0xFFFF) {
    return false;
  }

  uint8_t index = index_for_card_ref(card_ref);
  if (index > this->entries.size()) {
    return false;
  }

  // If the card is discarded, then it should be before the draw index, and we
  // can just change its state.
  if (this->entries[index].state == CardState::DISCARDED) {
    this->entries[index].state = CardState::IN_HAND;
    return true;

  // If the card is still drawable, we need to move it so it's just in front of
  // the draw index, then immediately draw it
  } else if (this->entries[index].state == CardState::DRAWABLE) {
    ssize_t ref_index;
    for (ref_index = this->card_refs.size(); ref_index >= 0; ref_index--) {
      if (this->card_refs[ref_index] == card_ref) {
        break;
      }
    }
    if (ref_index < 0) {
      return false;
    }

    size_t ref_uindex = ref_index;
    for (; ref_uindex > this->draw_index; ref_uindex--) {
      // Note: draw_index is also unsigned, so ref_uindex cannot be zero here
      this->card_refs[ref_uindex] = this->card_refs[ref_uindex - 1];
    }
    this->card_refs[this->draw_index] = card_ref;
    this->entries[index].state = CardState::IN_HAND;
    this->draw_index++;
    return true;

  } else {
    return false;
  }
}

uint16_t DeckState::card_id_for_card_ref(uint16_t card_ref) const {
  if (card_ref == 0xFFFF) {
    return 0xFFFF;
  }

  uint8_t index = index_for_card_ref(card_ref);
  if (index < this->entries.size()) {
    return this->entries[index].card_id;
  } else {
    return 0xFFFF;
  }
}

uint16_t DeckState::sc_card_id() const {
  return this->entries[0].card_id;
}

uint16_t DeckState::sc_card_ref() const {
  return this->card_refs[0];
}

uint16_t DeckState::card_ref_for_index(uint8_t index) const {
  return this->card_ref_base | index;
}

DeckState::CardState DeckState::state_for_card_ref(uint16_t card_ref) const {
  uint8_t index = index_for_card_ref(card_ref);
  return (index < this->entries.size()) ? this->entries[index].state : CardState::INVALID;
}

void DeckState::restart() {
  // First, if deck loop is on, return all discarded cards to the drawable state
  if (this->loop_enabled) {
    for (size_t z = 0; z < this->entries.size(); z++) {
      if (this->entries[z].state == CardState::DISCARDED) {
        this->entries[z].state = CardState::DRAWABLE;
      }
    }
  }

  // For any cards that are still in hand or still in play, move their refs to
  // the already-drawn part of the deck
  this->draw_index = 0;
  for (size_t z = 0; z < this->entries.size(); z++) {
    if (this->entries[z].state != CardState::DRAWABLE) {
      this->card_refs[this->draw_index++] = this->card_ref_for_index(z);
    }
  }

  // For now-drawable cards, put their refs after the draw index
  size_t index = this->draw_index;
  for (size_t z = 0; z < this->entries.size(); z++) {
    if (this->entries[z].state == CardState::DRAWABLE) {
      this->card_refs[index++] = this->card_ref_for_index(z);
    }
  }

  this->shuffle();
}

void DeckState::do_mulligan() {
  for (size_t z = 0; z < this->entries.size(); z++) {
    if (this->entries[z].state == CardState::DISCARDED) {
      this->entries[z].state = CardState::DRAWABLE;
    }
  }
  this->draw_index = 1;

  if (this->shuffle_enabled) {
    // Get the next 5 cards from the deck, and put the previous 5 cards after
    // them (so they will be shuffled back in).
    for (uint8_t z = 0; z < 5; z++) {
      uint8_t index = z + this->draw_index;
      uint16_t temp_ref = this->card_refs[index];
      this->card_refs[index] = this->card_refs[index + 5];
      this->card_refs[index + 5] = temp_ref;
    }

    // Shuffle the deck, except the first 5 cards (which are about to be drawn).
    size_t max = this->num_drawable_cards() - 5;
    uint8_t base_index = this->draw_index + 5;
    for (size_t z = 0; z < this->card_refs.size(); z++) {
      uint8_t index1 = this->random_crypt->next() % max;
      uint8_t index2 = this->random_crypt->next() % max;
      uint16_t temp_ref = this->card_refs[base_index + index1];
      this->card_refs[base_index + index1] = this->card_refs[base_index + index2];
      this->card_refs[base_index + index2] = temp_ref;
    }
  }
}

bool DeckState::set_card_ref_drawable_next(uint16_t card_ref) {
  if (card_ref == 0xFFFF) {
    return false;
  }
  if (client_id_for_card_ref(card_ref) != this->client_id) {
    return false;
  }

  uint8_t index = index_for_card_ref(card_ref);
  if (this->entries[index].state == CardState::DRAWABLE) {
    return false;
  } else if (this->draw_index < 1) {
    return false;
  } else {
    this->entries[index].state = CardState::DRAWABLE;
    this->card_refs[--this->draw_index] = card_ref;
    return true;
  }
}

bool DeckState::set_card_ref_drawable_at_end(uint16_t card_ref) {
  if (this->set_card_ref_drawable_next(card_ref)) {
    uint16_t head_card_ref = this->card_refs[this->draw_index];
    if (this->draw_index < this->card_refs.size() - 1) {
      for (size_t z = this->draw_index; z < this->card_refs.size() - 1; z++) {
        this->card_refs[z] = this->card_refs[z + 1];
      }
    }
    this->card_refs[this->card_refs.size() - 1] = head_card_ref;
    return true;
  } else {
    return false;
  }
}

void DeckState::set_card_discarded(uint16_t card_ref) {
  uint8_t index = index_for_card_ref(card_ref);
  if (index < this->entries.size()) {
    this->entries[index].state = CardState::DISCARDED;
  }
}

void DeckState::shuffle() {
  if (this->shuffle_enabled) {
    size_t max = this->num_drawable_cards();
    for (size_t z = 0; z < this->card_refs.size(); z++) {
      // Note: This is the way Sega originally implemented shuffling - they just
      // do N swaps on the entire array. A more uniform way to do it would be to
      // instead swap each item with another random item (possibly itself) that
      // doesn't appear earlier than it in the array, but this is not what Sega
      // did.
      uint8_t index1 = this->draw_index + this->random_crypt->next() % max;
      uint8_t index2 = this->draw_index + this->random_crypt->next() % max;
      uint16_t temp_ref = this->card_refs[index1];
      this->card_refs[index1] = this->card_refs[index2];
      this->card_refs[index2] = temp_ref;
    }
  }
}



} // namespace Episode3
