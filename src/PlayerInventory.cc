#include "PlayerInventory.hh"

void PlayerBank::load(FILE* f) {
  le_uint32_t num_items;
  le_uint32_t meseta;
  phosg::freadx(f, &num_items, sizeof(num_items));
  phosg::freadx(f, &meseta, sizeof(meseta));
  this->meseta = meseta;
  this->items.reserve(num_items);
  while (this->items.size() < num_items) {
    auto& item = this->items.emplace_back();
    phosg::freadx(f, &item, sizeof(item));
  }
}

void PlayerBank::save(FILE* f) const {
  le_uint32_t num_items = this->items.size();
  le_uint32_t meseta = this->meseta;
  phosg::fwritex(f, &num_items, sizeof(num_items));
  phosg::fwritex(f, &meseta, sizeof(meseta));
  for (const auto& item : this->items) {
    phosg::fwritex(f, &item, sizeof(item));
  }
}

uint32_t PlayerBank::bb_checksum() const {
  le_uint32_t num_items = this->items.size();
  le_uint32_t meseta = this->meseta;
  uint32_t ret = phosg::crc32(&num_items, sizeof(num_items));
  ret = phosg::crc32(&meseta, sizeof(meseta), ret);
  for (const auto& item : this->items) {
    ret = phosg::crc32(&item, sizeof(item), ret);
  }
  return ret;
}

void PlayerBank::add_item(const ItemData& item, const ItemData::StackLimits& limits) {
  uint32_t primary_identifier = item.primary_identifier();

  if (primary_identifier == 0x04000000) {
    this->meseta += item.data2d;
    if (this->meseta > this->max_meseta) {
      this->meseta = this->max_meseta;
    }
    return;
  }

  size_t combine_max = item.max_stack_size(limits);
  if (combine_max > 1) {
    size_t y;
    for (y = 0; y < this->items.size(); y++) {
      if (this->items[y].data.primary_identifier() == primary_identifier) {
        break;
      }
    }

    if (y < this->items.size()) {
      uint8_t new_count = this->items[y].data.data1[5] + item.data1[5];
      if (new_count > combine_max) {
        throw std::runtime_error("stack size would exceed limit");
      }
      this->items[y].data.data1[5] = new_count;
      this->items[y].amount = new_count;
      return;
    }
  }

  if (this->items.size() >= this->max_items) {
    throw std::runtime_error("no free space in bank");
  }

  auto& new_item = this->items.emplace_back();
  new_item.data = item;
  new_item.amount = (item.max_stack_size(limits) > 1) ? item.data1[5] : 1;
  new_item.present = 1;
}

ItemData PlayerBank::remove_item(uint32_t item_id, uint32_t amount, const ItemData::StackLimits& limits) {
  size_t index = this->find_item(item_id);
  auto& bank_item = this->items[index];

  ItemData ret = bank_item.data;
  if (amount && (bank_item.data.stack_size(limits) > 1) && (amount < bank_item.data.data1[5])) {
    ret.data1[5] = amount;
    bank_item.data.data1[5] -= amount;
    bank_item.amount -= amount;
  } else {
    this->items.erase(this->items.begin() + index);
  }
  return ret;
}

size_t PlayerBank::find_item(uint32_t item_id) {
  for (size_t x = 0; x < this->items.size(); x++) {
    if (this->items[x].data.id == item_id) {
      return x;
    }
  }
  throw std::out_of_range("item not present");
}

void PlayerBank::sort() {
  std::sort(this->items.begin(), this->items.end());
}

void PlayerBank::assign_ids(uint32_t base_id) {
  for (size_t z = 0; z < this->items.size(); z++) {
    this->items[z].data.id = base_id + z;
  }
}

void PlayerBank::enforce_stack_limits(std::shared_ptr<const ItemData::StackLimits> stack_limits) {
  for (auto& item : this->items) {
    item.data.enforce_stack_size_limits(*stack_limits);
  }
}
