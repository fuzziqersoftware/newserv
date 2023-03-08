#include "ItemParameterTable.hh"

using namespace std;



ItemParameterTable::ItemParameterTable(shared_ptr<const string> data)
  : data(data), r(*data) {
  size_t offset_table_offset = this->r.pget_u32l(this->data->size() - 0x10);
  this->offsets = &r.pget<TableOffsets>(offset_table_offset);
}

const ItemParameterTable::Weapon& ItemParameterTable::get_weapon(
    uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 >= 0xED) {
    throw runtime_error("weapon ID out of range");
  }
  const auto& co = this->r.pget<CountAndOffset>(
      this->offsets->weapon_table + sizeof(CountAndOffset) * data1_1);
  if (data1_2 >= co.count) {
    throw runtime_error("weapon ID out of range");
  }
  return this->r.pget<Weapon>(co.offset + sizeof(Weapon) * data1_2);
}

const ItemParameterTable::ArmorOrShield& ItemParameterTable::get_armor_or_shield(
    uint8_t data1_1, uint8_t data1_2) const {
  if ((data1_1 < 1) || (data1_1 > 2)) {
    throw runtime_error("armor/shield ID out of range");
  }
  const auto& co = this->r.pget<CountAndOffset>(
      this->offsets->armor_table + sizeof(CountAndOffset) * (data1_1 - 1));
  if (data1_2 >= co.count) {
    throw runtime_error("armor/shield ID out of range");
  }
  return this->r.pget<ArmorOrShield>(co.offset + sizeof(ArmorOrShield) * data1_2);
}

const ItemParameterTable::Unit& ItemParameterTable::get_unit(
    uint8_t data1_2) const {
  const auto& co = this->r.pget<CountAndOffset>(this->offsets->unit_table);
  if (data1_2 >= co.count) {
    throw runtime_error("unit ID out of range");
  }
  return this->r.pget<Unit>(co.offset + sizeof(Unit) * data1_2);
}

const ItemParameterTable::Tool& ItemParameterTable::get_tool(
    uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 > 0x1A) {
    throw runtime_error("tool ID out of range");
  }
  const auto& co = this->r.pget<CountAndOffset>(
      this->offsets->tool_table + sizeof(CountAndOffset) * data1_1);
  if (data1_2 >= co.count) {
    throw runtime_error("tool ID out of range");
  }
  return this->r.pget<Tool>(co.offset + sizeof(Tool) * data1_2);
}

pair<uint8_t, uint8_t> ItemParameterTable::find_tool_by_class(
    uint8_t tool_class) const {
  const auto& cos = this->r.pget<parray<CountAndOffset, 0x18>>(
      this->offsets->tool_table);
  for (size_t z = 0; z < cos.size(); z++) {
    const auto& co = cos[z];
    const auto* defs = &this->r.pget<Tool>(co.offset, sizeof(Tool) * co.count);
    for (size_t y = 0; y < co.count; y++) {
      if (defs[y].base.id == tool_class) {
        return make_pair(z, y);
      }
    }
  }
  throw out_of_range("invalid tool class");
}

const ItemParameterTable::Mag& ItemParameterTable::get_mag(
    uint8_t data1_1) const {
  const auto& co = this->r.pget<CountAndOffset>(this->offsets->mag_table);
  if (data1_1 >= co.count) {
    throw runtime_error("unit ID out of range");
  }
  return this->r.pget<Mag>(co.offset + sizeof(Mag) * data1_1);
}

float ItemParameterTable::get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
  if (data1_0 == 0) { // Weapon
    if (data1_1 < 0xED) {
      return this->r.pget_f32l(
          this->offsets->weapon_sale_divisor_table + data1_1 * sizeof(float));
    }
    return 0.0f;
  }

  const auto& divisors = this->r.pget<NonWeaponSaleDivisors>(
      this->offsets->sale_divisor_table);
  if (data1_0 == 1) {
    switch (data1_1) {
      case 1:
        return divisors.armor_divisor;
      case 2:
        return divisors.shield_divisor;
      case 3:
        return divisors.unit_divisor;
    }
    return 0.0f;
  }

  if (data1_0 == 2) {
    return divisors.mag_divisor;
  }

  return 0.0f;
}

uint8_t ItemParameterTable::get_item_stars(uint16_t slot) const {
  if ((slot >= 0xB1) && (slot < 0x437)) {
    return this->r.pget_u8(this->offsets->star_value_table + slot - 0xB1);
  }
  return 0;
}

uint8_t ItemParameterTable::get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
  if (char_class >= 12) {
    throw logic_error("invalid character class");
  }
  if (tech_num >= 19) {
    throw logic_error("invalid technique number");
  }
  return r.pget_u8(this->offsets->max_tech_level_table + tech_num * 12 + char_class);
}



const ItemParameterTable::ItemBase& ItemParameterTable::get_item_definition(
    const ItemData& item) const {
  switch (item.data1[0]) {
    case 0:
      return this->get_weapon(item.data1[1], item.data1[2]).base;
    case 1:
      if (item.data1[1] == 3) {
        return this->get_unit(item.data1[2]).base;
      } else if ((item.data1[1] == 1) || (item.data1[1] == 2)) {
        return this->get_armor_or_shield(item.data1[1], item.data1[2]).base;
      }
      throw logic_error("invalid item");
    case 2:
      return this->get_mag(item.data1[1]).base;
    case 3:
      if (item.data1[1] == 2) {
        return this->get_tool(2, item.data1[4]).base;
      } else {
        return this->get_tool(item.data1[1], item.data1[2]).base;
      }
      throw logic_error("this should be impossible");
    case 4:
      throw logic_error("item is meseta and therefore has no definition");
    default:
      throw logic_error("invalid item");
  }
}

uint8_t ItemParameterTable::get_item_stars(const ItemData& item) const {
  if (item.data1[0] == 2) {
    return (item.data1[1] > 0x27) ? 12 : 0;
  } else if (item.data1[0] < 2) {
    return this->get_item_stars(this->get_item_definition(item).id);
  } else if (item.data1[0] == 3) {
    const auto& def = (item.data1[1] == 2)
        ? this->get_tool(2, item.data1[4])
        : this->get_tool(item.data1[1], item.data1[2]);
    return (def.item_flag & 0x80) ? 12 : 0;
  } else {
    return 0;
  }
}

bool ItemParameterTable::is_item_rare(const ItemData& item) const {
  return (this->get_item_stars(item) >= 9);
}

bool ItemParameterTable::is_unsealable_item(const ItemData& item) const {
  const auto& co = this->r.pget<CountAndOffset>(this->offsets->unsealable_table);
  const auto* defs = &this->r.pget<UnsealableItem>(
      co.offset, co.count * sizeof(UnsealableItem));
  for (size_t z = 0; z < co.count; z++) {
    if ((defs[z].item[0] == item.data1[0]) &&
        (defs[z].item[1] == item.data1[1]) &&
        (defs[z].item[2] == item.data1[2])) {
      return true;
    }
  }
  return false;
}

