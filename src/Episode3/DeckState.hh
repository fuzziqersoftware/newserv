#pragma once

#include <stdint.h>

#include <memory>

#include "../PSOEncryption.hh"
#include "../Text.hh"
#include "DataIndexes.hh"

namespace Episode3 {

struct NameEntry {
  parray<char, 0x10> name;
  uint8_t client_id;
  uint8_t present;
  uint8_t is_cpu_player;
  uint8_t unused;

  NameEntry();
  void clear();
} __attribute__((packed));

struct DeckEntry {
  pstring<TextEncoding::SJIS, 0x10> name;
  le_uint32_t team_id;
  parray<le_uint16_t, 31> card_ids;
  // If the following flag is not set to 3, then the God Whim assist effect can
  // use cards that are hidden from the player during deck building. The client
  // always sets this to 3, and it's not clear why this even exists.
  uint8_t god_whim_flag;
  uint8_t unused1;
  le_uint16_t player_level;
  parray<uint8_t, 2> unused2;

  DeckEntry();
  void clear();
} __attribute__((packed));

uint8_t index_for_card_ref(uint16_t card_ref);
uint8_t client_id_for_card_ref(uint16_t card_ref);

class DeckState {
public:
  enum class CardState {
    DRAWABLE = 0,
    STORY_CHARACTER = 1,
    IN_HAND = 2,
    IN_PLAY = 3,
    DISCARDED = 4,
    INVALID = 5,
  };

  template <typename CardIDT>
  DeckState(
      uint8_t client_id,
      const parray<CardIDT, 0x1F>& card_ids,
      std::shared_ptr<PSOLFGEncryption> random_crypt)
      : client_id(client_id),
        draw_index(1),
        card_ref_base(this->client_id << 8),
        shuffle_enabled(true),
        loop_enabled(true),
        random_crypt(random_crypt) {
    for (size_t z = 0; z < card_ids.size(); z++) {
      auto& e = this->entries[z];
      e.card_id = card_ids[z];
      e.deck_index = z;
      e.state = CardState::DRAWABLE;
      this->card_refs[z] = this->card_ref_for_index(z);
    }
    this->entries[0].state = CardState::STORY_CHARACTER;
  }

  void disable_loop();
  void disable_shuffle();

  uint8_t num_drawable_cards() const;
  bool contains_card_ref(uint16_t card_ref) const;
  uint16_t card_id_for_card_ref(uint16_t card_ref) const;
  uint16_t sc_card_id() const;
  uint16_t sc_card_ref() const;
  uint16_t card_ref_for_index(uint8_t index) const;
  CardState state_for_card_ref(uint16_t card_ref) const;

  uint16_t draw_card();
  bool draw_card_by_ref(uint16_t card_ref);
  bool set_card_ref_in_play(uint16_t card_ref);
  bool set_card_ref_drawable_next(uint16_t card_ref);
  bool set_card_ref_drawable_at_end(uint16_t card_ref);
  void set_card_discarded(uint16_t card_ref);

  void restart();
  void shuffle();
  void do_mulligan();

  void print(FILE* stream, std::shared_ptr<const CardIndex> card_index = nullptr) const;

private:
  struct CardEntry {
    uint16_t card_id;
    uint8_t deck_index;
    CardState state;
  };
  uint8_t client_id;
  uint8_t draw_index;
  uint16_t card_ref_base;
  bool shuffle_enabled;
  bool loop_enabled;
  parray<CardEntry, 31> entries;
  parray<uint16_t, 31> card_refs;

  std::shared_ptr<PSOLFGEncryption> random_crypt;
};

} // namespace Episode3
