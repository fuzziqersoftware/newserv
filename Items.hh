#pragma once

#include <stdint.h>

#include <memory>

#include "Lobby.hh"
#include "Client.hh"

void player_use_item(std::shared_ptr<Lobby> l, std::shared_ptr<Client> c,
    size_t item_index);

struct CommonItemCreator {
  std::vector<uint32_t> enemy_item_categories;
  std::vector<uint32_t> box_item_categories;
  std::vector<std::vector<uint8_t>> unit_types;

  CommonItemCreator(const std::vector<uint32_t>& enemy_item_categories,
      const std::vector<uint32_t>& box_item_categories,
      const std::vector<std::vector<uint8_t>>& unit_types);

  int32_t decide_item_type(bool is_box) const;
  ItemData create_drop_item(bool is_box, uint8_t episode, uint8_t difficulty,
      uint8_t area, uint8_t section_id) const;
  ItemData create_shop_item(uint8_t difficulty, uint8_t shop_type) const;
};
