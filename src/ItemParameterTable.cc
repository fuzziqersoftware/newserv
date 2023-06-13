#include "ItemParameterTable.hh"

using namespace std;

ItemParameterTable::ItemParameterTable(shared_ptr<const string> data)
    : data(data),
      r(*data) {
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
  throw runtime_error("invalid tool class");
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

const ItemParameterTable::MagFeedResult& ItemParameterTable::get_mag_feed_result(
    uint8_t table_index, uint8_t item_index) const {
  if (table_index >= 8) {
    throw runtime_error("invalid mag feed table index");
  }
  if (item_index >= 11) {
    throw runtime_error("invalid mag feed item index");
  }
  const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets>(this->offsets->mag_feed_table);
  const auto& results = this->r.pget<MagFeedResultsList>(table_offsets.offsets[table_index]);
  return results.results[item_index];
}

uint8_t ItemParameterTable::get_item_stars(uint16_t slot) const {
  if ((slot >= 0xB1) && (slot < 0x437)) {
    return this->r.pget_u8(this->offsets->star_value_table + slot - 0xB1);
  }
  return 0;
}

uint8_t ItemParameterTable::get_special_stars(uint8_t det) const {
  if (!(det & 0x3F) || (det & 0x80)) {
    return 0;
  }
  // Note: PSO GC uses 0x1CB here. 0x256 was chosen to point to the same data in
  // PSO BB's ItemPMT file.
  return this->get_item_stars(det + 0x0256);
}

uint8_t ItemParameterTable::get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
  if (char_class >= 12) {
    throw runtime_error("invalid character class");
  }
  if (tech_num >= 19) {
    throw runtime_error("invalid technique number");
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
      throw runtime_error("invalid item");
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
      throw runtime_error("item is meseta and therefore has no definition");
    default:
      throw runtime_error("invalid item");
  }
}

uint8_t ItemParameterTable::get_item_base_stars(const ItemData& item) const {
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

uint8_t ItemParameterTable::get_item_adjusted_stars(const ItemData& item) const {
  uint8_t ret = this->get_item_base_stars(item);
  if (item.data1[0] == 0) {
    if (ret < 9) {
      if (!(item.data1[4] & 0x80)) {
        ret += this->get_special_stars(item.data1[4]);
      }
    } else if (item.data1[4] & 0x80) {
      ret = 0;
    }
  } else if (item.data1[0] == 1) {
    if (item.data1[1] == 3) {
      int16_t unit_bonus = item.get_unit_bonus();
      if (unit_bonus < 0) {
        ret--;
      } else if (unit_bonus > 0) {
        ret++;
      }
    }
  }
  return min<uint8_t>(ret, 12);
}

bool ItemParameterTable::is_item_rare(const ItemData& item) const {
  return (this->get_item_base_stars(item) >= 9);
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

void ItemParameterTable::populate_item_combination_index() const {
  if (!this->item_combination_index.empty()) {
    return;
  }

  const auto& co = this->r.pget<CountAndOffset>(this->offsets->combination_table);
  const auto* defs = &this->r.pget<ItemCombination>(
      co.offset, co.count * sizeof(ItemCombination));
  for (size_t z = 0; z < co.count; z++) {
    const auto& def = defs[z];
    uint32_t key = (def.used_item[0] << 16) | (def.used_item[1] << 8) | def.used_item[2];
    this->item_combination_index[key].emplace_back(def);
  }
}

const ItemParameterTable::ItemCombination& ItemParameterTable::get_item_combination(
    const ItemData& used_item, const ItemData& equipped_item) const {
  for (const auto& def : this->get_all_combinations_for_used_item(used_item)) {
    if ((def.equipped_item[0] == 0xFF || def.equipped_item[0] == equipped_item.data1[0]) &&
        (def.equipped_item[1] == 0xFF || def.equipped_item[1] == equipped_item.data1[1]) &&
        (def.equipped_item[2] == 0xFF || def.equipped_item[2] == equipped_item.data1[2])) {
      return def;
    }
  }
  throw out_of_range("no item combination applies");
}

const std::vector<ItemParameterTable::ItemCombination>& ItemParameterTable::get_all_combinations_for_used_item(
    const ItemData& used_item) const {
  try {
    uint32_t key = (used_item.data1[0] << 16) | (used_item.data1[1] << 8) | used_item.data1[2];
    return this->get_all_item_combinations().at(key);
  } catch (const out_of_range&) {
    static const vector<ItemCombination> ret;
    return ret;
  }
}

const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>>& ItemParameterTable::get_all_item_combinations() const {
  this->populate_item_combination_index();
  return this->item_combination_index;
}

std::pair<const ItemParameterTable::EventItem*, size_t> ItemParameterTable::get_event_items(uint8_t event_number) const {
  const auto& co = this->r.pget<CountAndOffset>(this->offsets->unwrap_table);
  if (event_number >= co.count) {
    throw runtime_error("invalid event number");
  }
  const auto& event_co = this->r.pget<CountAndOffset>(co.offset + sizeof(CountAndOffset) * event_number);
  const auto* defs = &this->r.pget<EventItem>(event_co.offset, event_co.count * sizeof(ItemCombination));
  return make_pair(defs, event_co.count);
}

size_t ItemParameterTable::price_for_item(const ItemData& item) const {
  switch (item.data1[0]) {
    case 0: {
      if (item.data1[4] & 0x80) {
        return 8;
      }
      if (this->is_item_rare(item)) {
        return 80;
      }

      float sale_divisor = this->get_sale_divisor(item.data1[0], item.data1[1]);
      if (sale_divisor == 0.0) {
        throw runtime_error("item sale divisor is zero");
      }

      const auto& def = this->get_weapon(item.data1[1], item.data1[2]);
      double atp_max = def.atp_max + item.data1[3];
      double atp_factor = ((atp_max * atp_max) / sale_divisor);

      double bonus_factor = 0.0;
      for (size_t bonus_index = 0; bonus_index < 3; bonus_index++) {
        uint8_t bonus_type = item.data1[(2 * bonus_index) + 6];
        if ((bonus_type > 0) && (bonus_type < 6)) {
          bonus_factor += item.data1[(2 * bonus_index) + 7];
        }
        bonus_factor += 100.0;
      }

      size_t special_stars = this->get_special_stars(item.data1[4]);
      double special_stars_factor = 1000.0 * special_stars * special_stars;

      return special_stars_factor + (atp_factor * (bonus_factor / 100.0));
    }

    case 1: {
      if (this->is_item_rare(item)) {
        return 80;
      }

      if (item.data1[1] == 3) { // Unit
        return this->get_item_adjusted_stars(item) * this->get_sale_divisor(item.data1[0], 3);
      }

      double sale_divisor = (double)this->get_sale_divisor(item.data1[0], item.data1[1]);
      if (sale_divisor == 0.0) {
        throw runtime_error("item sale divisor is zero");
      }

      int16_t def_bonus = item.get_armor_or_shield_defense_bonus();
      int16_t evp_bonus = item.get_common_armor_evasion_bonus();

      const auto& def = this->get_armor_or_shield(item.data1[1], item.data1[2]);
      double power_factor = def.dfp + def.evp + def_bonus + evp_bonus;
      double power_factor_floor = static_cast<int32_t>((power_factor * power_factor) / sale_divisor);
      return power_factor_floor + (70.0 * static_cast<double>(item.data1[5] + 1) * static_cast<double>(def.required_level + 1));
    }

    case 2:
      return (item.data1[2] + 1) * this->get_sale_divisor(2, item.data1[1]);

    case 3: {
      const auto& def = this->get_tool(item.data1[1], item.data1[2]);
      return def.cost * ((item.data1[1] == 2) ? (item.data1[2] + 1) : 1);
    }

    case 4:
      return item.data2d;

    default:
      throw runtime_error("invalid item");
  }
  throw logic_error("this should be impossible");
}

MagEvolutionTable::MagEvolutionTable(shared_ptr<const string> data)
    : data(data),
      r(*data) {
  size_t offset_table_offset = this->r.pget_u32l(this->data->size() - 0x10);
  this->offsets = &r.pget<TableOffsets>(offset_table_offset);
}

uint8_t MagEvolutionTable::get_evolution_number(uint8_t data1_1) const {
  const auto& table = this->r.pget<EvolutionNumberTable>(this->offsets->evolution_number);
  return table.values[data1_1];
}
