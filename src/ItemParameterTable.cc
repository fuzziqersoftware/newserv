#include "ItemParameterTable.hh"

using namespace std;

ItemParameterTable::ItemParameterTable(shared_ptr<const string> data, Version version)
    : data(data),
      r(*data),
      offsets_v2(nullptr),
      offsets_v3(nullptr),
      offsets_v4(nullptr) {
  switch (version) {
    case Version::V2: {
      size_t offset_table_offset = this->r.pget_u32l(this->data->size() - 0x10);
      this->offsets_v2 = &this->r.pget<TableOffsetsV2>(offset_table_offset);
      this->num_weapon_classes = 0x89;
      this->num_tool_classes = 0x10;
      this->item_stars_first_id = 0x4E;
      this->item_stars_last_id = 0x215;
      this->special_stars_begin_index = 0x138;
      this->star_value_table_size = 0x1C7;
      break;
    }
    case Version::V3: {
      size_t offset_table_offset = this->r.pget_u32b(this->data->size() - 0x10);
      this->offsets_v3 = &this->r.pget<TableOffsetsV3V4<true>>(offset_table_offset);
      this->num_weapon_classes = 0xAA;
      this->num_tool_classes = 0x18;
      this->item_stars_first_id = 0x94;
      this->item_stars_last_id = 0x2F7;
      this->special_stars_begin_index = 0x1CB;
      this->star_value_table_size = 0x263;
      break;
    }
    case Version::V4: {
      size_t offset_table_offset = this->r.pget_u32l(this->data->size() - 0x10);
      this->offsets_v4 = &this->r.pget<TableOffsetsV3V4<false>>(offset_table_offset);
      this->num_weapon_classes = 0xED;
      this->num_tool_classes = 0x1B;
      this->item_stars_first_id = 0xB1;
      this->item_stars_last_id = 0x437;
      this->special_stars_begin_index = 0x256;
      this->star_value_table_size = 0x330;
      break;
    }
    default:
      throw logic_error("invalid item parameter table version");
  }

  this->num_specials = 0x29;
  this->first_rare_mag_index = 0x28;
}

const ItemParameterTable::WeaponV4& ItemParameterTable::get_weapon(uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 >= this->num_weapon_classes) {
    throw runtime_error("weapon ID out of range");
  }

  if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->weapon_table + sizeof(ArrayRefLE) * data1_1);
    if (data1_2 >= co.count) {
      throw runtime_error("weapon ID out of range");
    }
    return this->r.pget<WeaponV4>(co.offset + sizeof(WeaponV4) * data1_2);
  }

  uint16_t key = (data1_1 << 8) | data1_2;
  try {
    return this->parsed_weapons.at(key);
  } catch (const std::out_of_range&) {
    auto& def_v4 = this->parsed_weapons.emplace(key, WeaponV4{}).first->second;

    if (this->offsets_v2) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v2->weapon_table + sizeof(ArrayRefLE) * data1_1);
      if (data1_2 >= co.count) {
        throw runtime_error("weapon ID out of range");
      }
      const auto& def_v2 = this->r.pget<WeaponV2>(co.offset + sizeof(WeaponV2) * data1_2);
      def_v4.base.id = def_v2.base.id;
      def_v4.class_flags = def_v2.class_flags;
      def_v4.atp_min = def_v2.atp_min;
      def_v4.atp_max = def_v2.atp_max;
      def_v4.atp_required = def_v2.atp_required;
      def_v4.mst_required = def_v2.mst_required;
      def_v4.ata_required = def_v2.ata_required;
      def_v4.max_grind = def_v2.max_grind;
      def_v4.photon = def_v2.photon;
      def_v4.special = def_v2.special;
      def_v4.ata = def_v2.ata;

    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->weapon_table + sizeof(ArrayRefBE) * data1_1);
      if (data1_2 >= co.count) {
        throw runtime_error("weapon ID out of range");
      }
      const auto& def_v3 = this->r.pget<WeaponV3<true>>(co.offset + sizeof(WeaponV3<true>) * data1_2);
      def_v4.base.id = def_v3.base.id.load();
      def_v4.base.type = def_v3.base.type.load();
      def_v4.base.skin = def_v3.base.skin.load();
      def_v4.class_flags = def_v3.class_flags.load();
      def_v4.atp_min = def_v3.atp_min.load();
      def_v4.atp_max = def_v3.atp_max.load();
      def_v4.atp_required = def_v3.atp_required.load();
      def_v4.mst_required = def_v3.mst_required.load();
      def_v4.ata_required = def_v3.ata_required.load();
      def_v4.mst = def_v3.mst.load();
      def_v4.max_grind = def_v3.max_grind;
      def_v4.photon = def_v3.photon;
      def_v4.special = def_v3.special;
      def_v4.ata = def_v3.ata;
      def_v4.stat_boost = def_v3.stat_boost;
      def_v4.projectile = def_v3.projectile;
      def_v4.trail1_x = def_v3.trail1_x;
      def_v4.trail1_y = def_v3.trail1_y;
      def_v4.trail2_x = def_v3.trail2_x;
      def_v4.trail2_y = def_v3.trail2_y;
      def_v4.color = def_v3.color;
      def_v4.unknown_a1 = def_v3.unknown_a1;
      def_v4.unknown_a2 = def_v3.unknown_a2;
      def_v4.unknown_a3 = def_v3.unknown_a3;
      def_v4.unknown_a4 = def_v3.unknown_a4;
      def_v4.unknown_a5 = def_v3.unknown_a5;
      def_v4.tech_boost = def_v3.tech_boost;
      def_v4.combo_type = def_v3.combo_type;

    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return def_v4;
  }
}

const ItemParameterTable::ArmorOrShieldV4& ItemParameterTable::get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
  if ((data1_1 < 1) || (data1_1 > 2)) {
    throw runtime_error("armor/shield class ID out of range");
  }

  if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->armor_table + sizeof(ArrayRefLE) * (data1_1 - 1));
    if (data1_2 >= co.count) {
      throw runtime_error("armor/shield ID out of range");
    }
    return this->r.pget<ArmorOrShieldV4>(co.offset + sizeof(ArmorOrShieldV4) * data1_2);
  }

  auto& parsed_vec = (data1_1 == 2) ? this->parsed_shields : this->parsed_armors;
  try {
    const auto& ret = parsed_vec.at(data1_2);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    if (data1_2 >= parsed_vec.size()) {
      parsed_vec.resize(data1_2 + 1);
    }
    auto& def_v4 = parsed_vec[data1_2];

    if (this->offsets_v2) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v2->armor_table + sizeof(ArrayRefLE) * (data1_1 - 1));
      if (data1_2 >= co.count) {
        throw runtime_error("armor/shield ID out of range");
      }
      const auto& def_v2 = this->r.pget<ArmorOrShieldV2>(co.offset + sizeof(ArmorOrShieldV2) * data1_2);
      def_v4.base.id = def_v2.base.id;
      def_v4.dfp = def_v2.dfp;
      def_v4.evp = def_v2.evp;
      def_v4.block_particle = def_v2.block_particle;
      def_v4.block_effect = def_v2.block_effect;
      def_v4.class_flags = def_v2.class_flags;
      def_v4.required_level = def_v2.required_level;
      def_v4.efr = def_v2.efr;
      def_v4.eth = def_v2.eth;
      def_v4.eic = def_v2.eic;
      def_v4.edk = def_v2.edk;
      def_v4.elt = def_v2.elt;
      def_v4.dfp_range = def_v2.dfp_range;
      def_v4.evp_range = def_v2.evp_range;
      def_v4.stat_boost = def_v2.stat_boost;
      def_v4.tech_boost = def_v2.tech_boost;
      def_v4.unknown_a2 = def_v2.unknown_a2;

    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->armor_table + sizeof(ArrayRefBE) * (data1_1 - 1));
      if (data1_2 >= co.count) {
        throw runtime_error("armor/shield ID out of range");
      }
      const auto& def_v3 = this->r.pget<ArmorOrShieldV3>(co.offset + sizeof(ArmorOrShieldV3) * data1_2);
      def_v4.base.id = def_v3.base.id.load();
      def_v4.base.type = def_v3.base.type.load();
      def_v4.base.skin = def_v3.base.skin.load();
      def_v4.dfp = def_v3.dfp.load();
      def_v4.evp = def_v3.evp.load();
      def_v4.block_particle = def_v3.block_particle;
      def_v4.block_effect = def_v3.block_effect;
      def_v4.class_flags = def_v3.class_flags.load();
      def_v4.required_level = def_v3.required_level;
      def_v4.efr = def_v3.efr;
      def_v4.eth = def_v3.eth;
      def_v4.eic = def_v3.eic;
      def_v4.edk = def_v3.edk;
      def_v4.elt = def_v3.elt;
      def_v4.dfp_range = def_v3.dfp_range;
      def_v4.evp_range = def_v3.evp_range;
      def_v4.stat_boost = def_v3.stat_boost;
      def_v4.tech_boost = def_v3.tech_boost;
      def_v4.unknown_a2 = def_v3.unknown_a2.load();

    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return def_v4;
  }
}

const ItemParameterTable::UnitV4& ItemParameterTable::get_unit(uint8_t data1_2) const {
  if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->unit_table);
    if (data1_2 >= co.count) {
      throw runtime_error("unit ID out of range");
    }
    return this->r.pget<UnitV4>(co.offset + sizeof(UnitV4) * data1_2);
  }

  try {
    const auto& ret = this->parsed_units.at(data1_2);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    if (data1_2 >= this->parsed_units.size()) {
      this->parsed_units.resize(data1_2 + 1);
    }
    auto& def_v4 = this->parsed_units[data1_2];

    if (this->offsets_v2) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v2->unit_table);
      if (data1_2 >= co.count) {
        throw runtime_error("unit ID out of range");
      }
      const auto& def_v2 = this->r.pget<UnitV2>(co.offset + sizeof(UnitV2) * data1_2);
      def_v4.base.id = def_v2.base.id;
      def_v4.stat = def_v2.stat;
      def_v4.stat_amount = def_v2.stat_amount;
      def_v4.modifier_amount = def_v2.modifier_amount;

    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->unit_table);
      if (data1_2 >= co.count) {
        throw runtime_error("unit ID out of range");
      }
      const auto& def_v3 = this->r.pget<UnitV3>(co.offset + sizeof(UnitV3) * data1_2);
      def_v4.base.id = def_v3.base.id.load();
      def_v4.base.type = def_v3.base.type.load();
      def_v4.base.skin = def_v3.base.skin.load();
      def_v4.stat = def_v3.stat.load();
      def_v4.stat_amount = def_v3.stat_amount.load();
      def_v4.modifier_amount = def_v3.modifier_amount.load();

    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return def_v4;
  }
}

const ItemParameterTable::MagV4& ItemParameterTable::get_mag(uint8_t data1_1) const {
  if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->mag_table);
    if (data1_1 >= co.count) {
      throw runtime_error("mag ID out of range");
    }
    return this->r.pget<MagV4>(co.offset + sizeof(MagV4) * data1_1);
  }

  try {
    const auto& ret = this->parsed_mags.at(data1_1);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    if (data1_1 >= this->parsed_mags.size()) {
      this->parsed_mags.resize(data1_1 + 1);
    }
    auto& def_v4 = this->parsed_mags[data1_1];

    if (this->offsets_v2) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v2->mag_table);
      if (data1_1 >= co.count) {
        throw runtime_error("mag ID out of range");
      }
      const auto& def_v2 = this->r.pget<MagV2>(co.offset + sizeof(MagV2) * data1_1);
      def_v4.base.id = def_v2.base.id;
      def_v4.feed_table = def_v2.feed_table;
      def_v4.photon_blast = def_v2.photon_blast;
      def_v4.activation = def_v2.activation;
      def_v4.on_pb_full = def_v2.on_pb_full;
      def_v4.on_low_hp = def_v2.on_low_hp;
      def_v4.on_death = def_v2.on_death;
      def_v4.on_boss = def_v2.on_boss;
      def_v4.on_pb_full_flag = def_v2.on_pb_full_flag;
      def_v4.on_low_hp_flag = def_v2.on_low_hp_flag;
      def_v4.on_death_flag = def_v2.on_death_flag;
      def_v4.on_boss_flag = def_v2.on_boss_flag;
      def_v4.class_flags = def_v2.class_flags;

    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->mag_table);
      if (data1_1 >= co.count) {
        throw runtime_error("mag ID out of range");
      }
      const auto& def_v3 = this->r.pget<MagV3>(co.offset + sizeof(MagV3) * data1_1);
      def_v4.base.id = def_v3.base.id.load();
      def_v4.base.type = def_v3.base.type.load();
      def_v4.base.skin = def_v3.base.skin.load();
      def_v4.feed_table = def_v3.feed_table.load();
      def_v4.photon_blast = def_v3.photon_blast;
      def_v4.activation = def_v3.activation;
      def_v4.on_pb_full = def_v3.on_pb_full;
      def_v4.on_low_hp = def_v3.on_low_hp;
      def_v4.on_death = def_v3.on_death;
      def_v4.on_boss = def_v3.on_boss;
      def_v4.on_pb_full_flag = def_v3.on_pb_full_flag;
      def_v4.on_low_hp_flag = def_v3.on_low_hp_flag;
      def_v4.on_death_flag = def_v3.on_death_flag;
      def_v4.on_boss_flag = def_v3.on_boss_flag;
      def_v4.class_flags = def_v3.class_flags.load();

    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return def_v4;
  }
}

const ItemParameterTable::ToolV4& ItemParameterTable::get_tool(uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 >= this->num_tool_classes) {
    throw runtime_error("tool class ID out of range");
  }

  if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->tool_table + sizeof(ArrayRefLE) * data1_1);
    if (data1_2 >= co.count) {
      throw runtime_error("tool ID out of range");
    }
    return this->r.pget<ToolV4>(co.offset + sizeof(ToolV4) * data1_2);
  }

  uint16_t key = (data1_1 << 8) | data1_2;
  try {
    return this->parsed_tools.at(key);
  } catch (const std::out_of_range&) {
    auto& def_v4 = this->parsed_tools.emplace(key, ToolV4{}).first->second;

    if (this->offsets_v2) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v2->tool_table + sizeof(ArrayRefLE) * data1_1);
      if (data1_2 >= co.count) {
        throw runtime_error("tool ID out of range");
      }
      const auto& def_v2 = this->r.pget<ToolV2>(co.offset + sizeof(ToolV2) * data1_2);
      def_v4.base.id = def_v2.base.id;
      def_v4.amount = def_v2.amount;
      def_v4.tech = def_v2.tech;
      def_v4.cost = def_v2.cost;
      def_v4.item_flag = def_v2.item_flag;

    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->tool_table + sizeof(ArrayRefBE) * data1_1);
      if (data1_2 >= co.count) {
        throw runtime_error("tool ID out of range");
      }
      const auto& def_v3 = this->r.pget<ToolV3>(co.offset + sizeof(ToolV3) * data1_2);
      def_v4.base.id = def_v3.base.id.load();
      def_v4.base.type = def_v3.base.type.load();
      def_v4.base.skin = def_v3.base.skin.load();
      def_v4.amount = def_v3.amount.load();
      def_v4.tech = def_v3.tech.load();
      def_v4.cost = def_v3.cost.load();
      def_v4.item_flag = def_v3.item_flag;

    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return def_v4;
  }
}

template <typename ToolT, bool IsBigEndian>
pair<uint8_t, uint8_t> ItemParameterTable::find_tool_by_id_t(uint32_t tool_table_offset, uint32_t item_id) const {
  const auto* cos = &this->r.pget<ArrayRef<IsBigEndian>>(
      tool_table_offset, this->num_tool_classes * sizeof(ArrayRef<IsBigEndian>));
  for (size_t z = 0; z < this->num_tool_classes; z++) {
    const auto& co = cos[z];
    const auto* defs = &this->r.pget<ToolT>(co.offset, sizeof(ToolT) * co.count);
    for (size_t y = 0; y < co.count; y++) {
      if (defs[y].base.id == item_id) {
        return make_pair(z, y);
      }
    }
  }
  throw runtime_error("invalid tool class");
}

pair<uint8_t, uint8_t> ItemParameterTable::find_tool_by_id(uint32_t item_id) const {
  if (this->offsets_v2) {
    return this->find_tool_by_id_t<ToolV2, false>(this->offsets_v2->tool_table, item_id);
  } else if (this->offsets_v3) {
    return this->find_tool_by_id_t<ToolV3, true>(this->offsets_v3->tool_table, item_id);
  } else if (this->offsets_v4) {
    return this->find_tool_by_id_t<ToolV4, false>(this->offsets_v4->tool_table, item_id);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

template <bool IsBigEndian>
float ItemParameterTable::get_sale_divisor_t(
    uint32_t weapon_table_offset, uint32_t non_weapon_table_offset, uint8_t data1_0, uint8_t data1_1) const {
  using FloatT = typename std::conditional<IsBigEndian, be_float, le_float>::type;

  switch (data1_0) {
    case 0:
      if (data1_1 >= this->num_weapon_classes) {
        return 0.0f;
      }
      return this->r.pget<FloatT>(weapon_table_offset + data1_1 * sizeof(FloatT));

    case 1: {
      const auto& divisors = this->r.pget<NonWeaponSaleDivisors<IsBigEndian>>(non_weapon_table_offset);
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

    case 2: {
      const auto& divisors = this->r.pget<NonWeaponSaleDivisors<IsBigEndian>>(non_weapon_table_offset);
      return divisors.mag_divisor;
    }

    default:
      return 0.0f;
  }
}

float ItemParameterTable::get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
  if (this->offsets_v2) {
    return this->get_sale_divisor_t<false>(
        this->offsets_v2->weapon_sale_divisor_table, this->offsets_v2->sale_divisor_table, data1_0, data1_1);
  } else if (this->offsets_v3) {
    return this->get_sale_divisor_t<true>(
        this->offsets_v3->weapon_sale_divisor_table, this->offsets_v3->sale_divisor_table, data1_0, data1_1);
  } else if (this->offsets_v4) {
    return this->get_sale_divisor_t<false>(
        this->offsets_v4->weapon_sale_divisor_table, this->offsets_v4->sale_divisor_table, data1_0, data1_1);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::MagFeedResult& ItemParameterTable::get_mag_feed_result(
    uint8_t table_index, uint8_t item_index) const {
  if (table_index >= 8) {
    throw runtime_error("invalid mag feed table index");
  }
  if (item_index >= 11) {
    throw runtime_error("invalid mag feed item index");
  }

  uint32_t offset;
  if (this->offsets_v2) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets<false>>(this->offsets_v2->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v3) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets<true>>(this->offsets_v3->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v4) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets<false>>(this->offsets_v4->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  return this->r.pget<MagFeedResultsList>(offset)[item_index];
}

uint8_t ItemParameterTable::get_item_stars(uint32_t item_id) const {
  uint32_t base_offset;
  if (this->offsets_v2) {
    base_offset = this->offsets_v2->star_value_table;
  } else if (this->offsets_v3) {
    base_offset = this->offsets_v3->star_value_table;
  } else if (this->offsets_v4) {
    base_offset = this->offsets_v4->star_value_table;
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  return ((item_id >= this->item_stars_first_id) && (item_id < this->item_stars_last_id))
      ? this->r.pget_u8(base_offset + item_id - this->item_stars_first_id)
      : 0;
}

uint8_t ItemParameterTable::get_special_stars(uint8_t det) const {
  return ((det & 0x3F) && !(det & 0x80))
      ? this->get_item_stars(det + this->special_stars_begin_index)
      : 0;
}

const ItemParameterTable::Special<false>& ItemParameterTable::get_special(uint8_t special) const {
  special &= 0x3F;
  if (special >= this->num_specials) {
    throw runtime_error("invalid special index");
  }

  if (this->offsets_v2) {
    return this->r.pget<Special<false>>(this->offsets_v2->special_data_table + sizeof(Special<false>) * special);
  } else if (this->offsets_v3) {
    if ((special >= this->parsed_specials.size()) || (this->parsed_specials[special].type != 0xFFFF)) {
      if (special >= this->parsed_specials.size()) {
        this->parsed_specials.resize(special + 1);
      }
      const auto& sp_be = this->r.pget<Special<true>>(this->offsets_v3->special_data_table + sizeof(Special<true>) * special);
      this->parsed_specials[special].type = sp_be.type.load();
      this->parsed_specials[special].amount = sp_be.amount.load();
    }
    return this->parsed_specials[special];
  } else if (this->offsets_v4) {
    return this->r.pget<Special<false>>(this->offsets_v4->special_data_table + sizeof(Special<false>) * special);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

uint8_t ItemParameterTable::get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
  if (char_class >= 12) {
    throw runtime_error("invalid character class");
  }
  if (tech_num >= 19) {
    throw runtime_error("invalid technique number");
  }

  if (this->offsets_v2) {
    if ((tech_num == 14) || (tech_num == 17)) { // Ryuker or Reverser
      return 0;
    } else if (tech_num == 16) { // Anti
      return 7;
    } else {
      return ((char_class == 6) || (char_class == 7) || (char_class == 8) || (char_class == 10)) ? 29 : 14;
    }
  } else if (this->offsets_v3) {
    return r.pget_u8(this->offsets_v3->max_tech_level_table + tech_num * 12 + char_class);
  } else if (this->offsets_v4) {
    return r.pget_u8(this->offsets_v4->max_tech_level_table + tech_num * 12 + char_class);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

uint32_t ItemParameterTable::get_item_id(const ItemData& item) const {
  switch (item.data1[0]) {
    case 0:
      return this->get_weapon(item.data1[1], item.data1[2]).base.id;
    case 1:
      if (item.data1[1] == 3) {
        return this->get_unit(item.data1[2]).base.id;
      } else if ((item.data1[1] == 1) || (item.data1[1] == 2)) {
        return this->get_armor_or_shield(item.data1[1], item.data1[2]).base.id;
      }
      throw runtime_error("invalid item");
    case 2:
      return this->get_mag(item.data1[1]).base.id;
    case 3:
      if (item.data1[1] == 2) {
        return this->get_tool(2, item.data1[4]).base.id;
      } else {
        return this->get_tool(item.data1[1], item.data1[2]).base.id;
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
    return (item.data1[1] >= this->first_rare_mag_index) ? 12 : 0;
  } else if (item.data1[0] < 2) {
    return this->get_item_stars(this->get_item_id(item));
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
  uint32_t offset, count;
  if (this->offsets_v2) {
    return false;
  } else if (this->offsets_v3) {
    const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->unsealable_table);
    offset = co.offset;
    count = co.count;
  } else if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->unsealable_table);
    offset = co.offset;
    count = co.count;
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  const auto* defs = &this->r.pget<UnsealableItem>(offset, count * sizeof(UnsealableItem));
  for (size_t z = 0; z < count; z++) {
    if ((defs[z].item[0] == item.data1[0]) &&
        (defs[z].item[1] == item.data1[1]) &&
        (defs[z].item[2] == item.data1[2])) {
      return true;
    }
  }
  return false;
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
  if (this->item_combination_index.empty()) {
    uint32_t offset, count;
    if (this->offsets_v2) {
      static const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>> empty_map;
      return empty_map;
    } else if (this->offsets_v3) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3->combination_table);
      offset = co.offset;
      count = co.count;
    } else if (this->offsets_v4) {
      const auto& co = this->r.pget<ArrayRefLE>(this->offsets_v4->combination_table);
      offset = co.offset;
      count = co.count;
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    const auto* defs = &this->r.pget<ItemCombination>(offset, count * sizeof(ItemCombination));
    for (size_t z = 0; z < count; z++) {
      const auto& def = defs[z];
      uint32_t key = (def.used_item[0] << 16) | (def.used_item[1] << 8) | def.used_item[2];
      this->item_combination_index[key].emplace_back(def);
    }
  }
  return this->item_combination_index;
}

template <bool IsBigEndian>
std::pair<const ItemParameterTable::EventItem*, size_t> ItemParameterTable::get_event_items_t(
    uint32_t base_offset, uint8_t event_number) const {
  const auto& co = this->r.pget<ArrayRef<IsBigEndian>>(base_offset);
  if (event_number >= co.count) {
    throw runtime_error("invalid event number");
  }
  const auto& event_co = this->r.pget<ArrayRef<IsBigEndian>>(co.offset + sizeof(ArrayRef<IsBigEndian>) * event_number);
  const auto* defs = &this->r.pget<EventItem>(event_co.offset, event_co.count * sizeof(EventItem));
  return make_pair(defs, event_co.count);
}

std::pair<const ItemParameterTable::EventItem*, size_t> ItemParameterTable::get_event_items(uint8_t event_number) const {
  if (this->offsets_v2) {
    return make_pair(nullptr, 0);
  } else if (this->offsets_v3) {
    return this->get_event_items_t<true>(this->offsets_v3->unwrap_table, event_number);
  } else if (this->offsets_v4) {
    return this->get_event_items_t<false>(this->offsets_v4->unwrap_table, event_number);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
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

      return special_stars_factor + ((atp_factor * bonus_factor) / 100.0);
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
