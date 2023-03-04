#pragma once

#include <stdint.h>

#include <memory>
#include <random>

#include "Client.hh"
#include "StaticGameData.hh"



void player_use_item(std::shared_ptr<Client> c, size_t item_index);

struct CommonItemData {
  std::vector<uint32_t> enemy_item_categories;
  std::vector<uint32_t> box_item_categories;
  std::vector<std::vector<uint8_t>> unit_types;

  CommonItemData(
      std::vector<uint32_t>&& enemy_item_categories,
      std::vector<uint32_t>&& box_item_categories,
      std::vector<std::vector<uint8_t>>&& unit_types);
};

struct CommonItemCreator {
  std::shared_ptr<const CommonItemData> data;
  std::shared_ptr<std::mt19937> random;

  CommonItemCreator(
      std::shared_ptr<const CommonItemData> data,
      std::shared_ptr<std::mt19937> random);

  int32_t decide_item_type(bool is_box) const;
  ItemData create_drop_item(bool is_box, Episode episode, uint8_t difficulty,
      uint8_t area, uint8_t section_id) const;
  ItemData create_shop_item(uint8_t difficulty, uint8_t shop_type) const;
};
