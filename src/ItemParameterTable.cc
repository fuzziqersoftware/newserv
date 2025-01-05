#include "ItemParameterTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

ItemParameterTable::ItemParameterTable(shared_ptr<const string> data, Version version)
    : version(version),
      data(data),
      r(*data),
      offsets_dc_protos(nullptr),
      offsets_v1_v2(nullptr),
      offsets_gc_nte(nullptr),
      offsets_v3_le(nullptr),
      offsets_v3_be(nullptr),
      offsets_v4(nullptr) {
  size_t offset_table_offset = is_big_endian(version)
      ? this->r.pget_u32b(this->data->size() - 0x10)
      : this->r.pget_u32l(this->data->size() - 0x10);

  switch (this->version) {
    case Version::DC_NTE: {
      this->offsets_dc_protos = &this->r.pget<TableOffsetsDCProtos>(offset_table_offset);
      this->num_weapon_classes = 0x27;
      this->num_tool_classes = 0x0D;
      this->item_stars_first_id = 0x22;
      this->item_stars_last_id = 0x168;
      this->special_stars_begin_index = 0xAA;
      this->num_specials = 0x28;
      // TODO: Check if first_rare_mag_index is the same on this version
      break;
    }
    case Version::DC_11_2000: {
      this->offsets_dc_protos = &this->r.pget<TableOffsetsDCProtos>(offset_table_offset);
      this->num_weapon_classes = 0x27;
      this->num_tool_classes = 0x0E;
      this->item_stars_first_id = 0x26;
      this->item_stars_last_id = 0x16C;
      this->special_stars_begin_index = 0xAE;
      this->num_specials = 0x28;
      // TODO: Check if first_rare_mag_index is the same on this version
      break;
    }
    case Version::DC_V1: {
      this->offsets_v1_v2 = &this->r.pget<TableOffsetsV1V2>(offset_table_offset);
      this->num_weapon_classes = 0x27;
      this->num_tool_classes = 0x0E;
      this->item_stars_first_id = 0x26;
      this->item_stars_last_id = 0x16C;
      this->special_stars_begin_index = 0xAE;
      this->num_specials = 0x29;
      // TODO: Check if first_rare_mag_index is the same on this version
      break;
    }
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2: {
      this->offsets_v1_v2 = &this->r.pget<TableOffsetsV1V2>(offset_table_offset);
      this->num_weapon_classes = 0x89;
      this->num_tool_classes = 0x10;
      this->item_stars_first_id = 0x4E;
      this->item_stars_last_id = 0x215;
      this->special_stars_begin_index = 0x138;
      this->num_specials = 0x29;
      break;
    }

    case Version::GC_NTE: {
      this->offsets_gc_nte = &this->r.pget<TableOffsetsGCNTE>(offset_table_offset);
      this->num_weapon_classes = 0x8D;
      this->num_tool_classes = 0x13;
      this->item_stars_first_id = 0x76;
      this->item_stars_last_id = 0x298;
      this->special_stars_begin_index = 0x1A3;
      this->num_specials = 0x29;
      break;
    }

    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::GC_V3:
    case Version::XB_V3: {
      if (is_big_endian(this->version)) {
        this->offsets_v3_be = &this->r.pget<TableOffsetsV3V4BE>(offset_table_offset);
      } else {
        this->offsets_v3_le = &this->r.pget<TableOffsetsV3V4>(offset_table_offset);
      }
      this->num_weapon_classes = 0xAA;
      this->num_tool_classes = 0x18;
      this->item_stars_first_id = 0x94;
      this->item_stars_last_id = 0x2F7;
      this->special_stars_begin_index = 0x1CB;
      this->num_specials = 0x29;
      break;
    }

    case Version::BB_V4: {
      this->offsets_v4 = &this->r.pget<TableOffsetsV3V4>(offset_table_offset);
      this->num_weapon_classes = 0xED;
      this->num_tool_classes = 0x1B;
      this->item_stars_first_id = 0xB1;
      this->item_stars_last_id = 0x437;
      this->special_stars_begin_index = 0x256;
      this->num_specials = 0x29;
      break;
    }
    default:
      throw logic_error("invalid item parameter table version");
  }

  this->first_rare_mag_index = 0x28;
}

set<uint32_t> ItemParameterTable::compute_all_valid_primary_identifiers() const {
  set<uint32_t> ret;

  auto find_items_1d = [&](uint64_t data1, size_t position) -> size_t {
    ItemData item(data1, 0);
    for (size_t x = 0; x < 0x100; x++) {
      item.data1[position] = x;
      try {
        this->get_item_id(item);
      } catch (const out_of_range&) {
        return x;
      }
      ret.emplace(item.primary_identifier());
    }
    return 0x100;
  };
  auto find_items_2d = [&](uint64_t data1) {
    for (size_t x = 0; x < 0x100; x++) {
      size_t effective_data1 = data1 | (static_cast<uint64_t>(x) << 48);
      size_t data2_position = (effective_data1 == 0x0302000000000000) ? 4 : 2;
      if (find_items_1d(effective_data1, data2_position) == 0) {
        break;
      }
    }
  };

  find_items_2d(0x0000000000000000);
  find_items_1d(0x0101000000000000, 2);
  find_items_1d(0x0102000000000000, 2);
  find_items_1d(0x0103000000000000, 2);
  find_items_1d(0x0200000000000000, 1);
  find_items_2d(0x0300000000000000);
  return ret;
}

ItemParameterTable::WeaponV4 ItemParameterTable::WeaponDCProtos::to_v4() const {
  WeaponV4 ret;
  ret.base.id = this->base.id;
  ret.class_flags = this->class_flags;
  ret.atp_min = this->atp_min;
  ret.atp_max = this->atp_max;
  ret.atp_required = this->atp_required;
  ret.mst_required = this->mst_required;
  ret.ata_required = this->ata_required;
  ret.max_grind = this->max_grind;
  ret.photon = this->photon;
  ret.special = this->special;
  ret.ata = this->ata;
  return ret;
}

ItemParameterTable::WeaponV4 ItemParameterTable::WeaponV1V2::to_v4() const {
  WeaponV4 ret;
  ret.base.id = this->base.id;
  ret.class_flags = this->class_flags;
  ret.atp_min = this->atp_min;
  ret.atp_max = this->atp_max;
  ret.atp_required = this->atp_required;
  ret.mst_required = this->mst_required;
  ret.ata_required = this->ata_required;
  ret.max_grind = this->max_grind;
  ret.photon = this->photon;
  ret.special = this->special;
  ret.ata = this->ata;
  ret.stat_boost = this->stat_boost;
  return ret;
}

ItemParameterTable::WeaponV4 ItemParameterTable::WeaponGCNTE::to_v4() const {
  WeaponV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.class_flags = this->class_flags.load();
  ret.atp_min = this->atp_min.load();
  ret.atp_max = this->atp_max.load();
  ret.atp_required = this->atp_required.load();
  ret.mst_required = this->mst_required.load();
  ret.ata_required = this->ata_required.load();
  ret.mst = this->mst.load();
  ret.max_grind = this->max_grind;
  ret.photon = this->photon;
  ret.special = this->special;
  ret.ata = this->ata;
  ret.stat_boost = this->stat_boost;
  ret.projectile = this->projectile;
  ret.trail1_x = this->trail1_x;
  ret.trail1_y = this->trail1_y;
  ret.trail2_x = this->trail2_x;
  ret.trail2_y = this->trail2_y;
  ret.color = this->color;
  ret.unknown_a1 = this->unknown_a1;
  ret.unknown_a2 = this->unknown_a2;
  ret.unknown_a3 = this->unknown_a3;
  return ret;
}

template <bool BE>
ItemParameterTable::WeaponV4 ItemParameterTable::WeaponV3T<BE>::to_v4() const {
  WeaponV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.class_flags = this->class_flags.load();
  ret.atp_min = this->atp_min.load();
  ret.atp_max = this->atp_max.load();
  ret.atp_required = this->atp_required.load();
  ret.mst_required = this->mst_required.load();
  ret.ata_required = this->ata_required.load();
  ret.mst = this->mst.load();
  ret.max_grind = this->max_grind;
  ret.photon = this->photon;
  ret.special = this->special;
  ret.ata = this->ata;
  ret.stat_boost = this->stat_boost;
  ret.projectile = this->projectile;
  ret.trail1_x = this->trail1_x;
  ret.trail1_y = this->trail1_y;
  ret.trail2_x = this->trail2_x;
  ret.trail2_y = this->trail2_y;
  ret.color = this->color;
  ret.unknown_a1 = this->unknown_a1;
  ret.unknown_a2 = this->unknown_a2;
  ret.unknown_a3 = this->unknown_a3;
  ret.unknown_a4 = this->unknown_a4;
  ret.unknown_a5 = this->unknown_a5;
  ret.tech_boost = this->tech_boost;
  ret.combo_type = this->combo_type;
  return ret;
}

ItemParameterTable::ArmorOrShieldV4 ItemParameterTable::ArmorOrShieldDCProtos::to_v4() const {
  ArmorOrShieldV4 ret;
  ret.base.id = this->base.id;
  ret.dfp = this->dfp;
  ret.evp = this->evp;
  ret.block_particle = this->block_particle;
  ret.block_effect = this->block_effect;
  ret.class_flags = this->class_flags;
  ret.required_level = this->required_level;
  ret.efr = this->efr;
  ret.eth = this->eth;
  ret.eic = this->eic;
  ret.edk = this->edk;
  ret.elt = this->elt;
  ret.dfp_range = this->dfp_range;
  ret.evp_range = this->evp_range;
  return ret;
}

ItemParameterTable::ArmorOrShieldV4 ItemParameterTable::ArmorOrShieldV1V2::to_v4() const {
  ArmorOrShieldV4 ret;
  ret.base.id = this->base.id;
  ret.dfp = this->dfp;
  ret.evp = this->evp;
  ret.block_particle = this->block_particle;
  ret.block_effect = this->block_effect;
  ret.class_flags = this->class_flags;
  ret.required_level = this->required_level;
  ret.efr = this->efr;
  ret.eth = this->eth;
  ret.eic = this->eic;
  ret.edk = this->edk;
  ret.elt = this->elt;
  ret.dfp_range = this->dfp_range;
  ret.evp_range = this->evp_range;
  ret.stat_boost = this->stat_boost;
  ret.tech_boost = this->tech_boost;
  ret.unknown_a2 = this->unknown_a2;
  return ret;
}

template <bool BE>
ItemParameterTable::ArmorOrShieldV4 ItemParameterTable::ArmorOrShieldV3T<BE>::to_v4() const {
  ArmorOrShieldV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.dfp = this->dfp.load();
  ret.evp = this->evp.load();
  ret.block_particle = this->block_particle;
  ret.block_effect = this->block_effect;
  ret.class_flags = this->class_flags.load();
  ret.required_level = this->required_level;
  ret.efr = this->efr;
  ret.eth = this->eth;
  ret.eic = this->eic;
  ret.edk = this->edk;
  ret.elt = this->elt;
  ret.dfp_range = this->dfp_range;
  ret.evp_range = this->evp_range;
  ret.stat_boost = this->stat_boost;
  ret.tech_boost = this->tech_boost;
  ret.unknown_a2 = this->unknown_a2.load();
  return ret;
}

ItemParameterTable::UnitV4 ItemParameterTable::UnitDCProtos::to_v4() const {
  UnitV4 ret;
  ret.base.id = this->base.id;
  ret.stat = this->stat;
  ret.stat_amount = this->stat_amount;
  return ret;
}

ItemParameterTable::UnitV4 ItemParameterTable::UnitV1V2::to_v4() const {
  UnitV4 ret;
  ret.base.id = this->base.id;
  ret.stat = this->stat;
  ret.stat_amount = this->stat_amount;
  ret.modifier_amount = this->modifier_amount;
  return ret;
}

template <bool BE>
ItemParameterTable::UnitV4 ItemParameterTable::UnitV3T<BE>::to_v4() const {
  UnitV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.stat = this->stat.load();
  ret.stat_amount = this->stat_amount.load();
  ret.modifier_amount = this->modifier_amount.load();
  return ret;
}

ItemParameterTable::MagV4 ItemParameterTable::MagV1::to_v4() const {
  MagV4 ret;
  ret.base.id = this->base.id;
  ret.feed_table = this->feed_table;
  ret.photon_blast = this->photon_blast;
  ret.activation = this->activation;
  ret.on_pb_full = this->on_pb_full;
  ret.on_low_hp = this->on_low_hp;
  ret.on_death = this->on_death;
  ret.on_boss = this->on_boss;
  ret.on_pb_full_flag = this->on_pb_full_flag;
  ret.on_low_hp_flag = this->on_low_hp_flag;
  ret.on_death_flag = this->on_death_flag;
  ret.on_boss_flag = this->on_boss_flag;
  return ret;
}

ItemParameterTable::MagV4 ItemParameterTable::MagV2::to_v4() const {
  MagV4 ret;
  ret.base.id = this->base.id;
  ret.feed_table = this->feed_table;
  ret.photon_blast = this->photon_blast;
  ret.activation = this->activation;
  ret.on_pb_full = this->on_pb_full;
  ret.on_low_hp = this->on_low_hp;
  ret.on_death = this->on_death;
  ret.on_boss = this->on_boss;
  ret.on_pb_full_flag = this->on_pb_full_flag;
  ret.on_low_hp_flag = this->on_low_hp_flag;
  ret.on_death_flag = this->on_death_flag;
  ret.on_boss_flag = this->on_boss_flag;
  ret.class_flags = this->class_flags;
  return ret;
}

template <bool BE>
ItemParameterTable::MagV4 ItemParameterTable::MagV3T<BE>::to_v4() const {
  MagV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.feed_table = this->feed_table.load();
  ret.photon_blast = this->photon_blast;
  ret.activation = this->activation;
  ret.on_pb_full = this->on_pb_full;
  ret.on_low_hp = this->on_low_hp;
  ret.on_death = this->on_death;
  ret.on_boss = this->on_boss;
  ret.on_pb_full_flag = this->on_pb_full_flag;
  ret.on_low_hp_flag = this->on_low_hp_flag;
  ret.on_death_flag = this->on_death_flag;
  ret.on_boss_flag = this->on_boss_flag;
  ret.class_flags = this->class_flags.load();
  return ret;
}

ItemParameterTable::ToolV4 ItemParameterTable::ToolV1V2::to_v4() const {
  ToolV4 ret;
  ret.base.id = this->base.id;
  ret.amount = this->amount;
  ret.tech = this->tech;
  ret.cost = this->cost;
  ret.item_flags = this->item_flags;
  return ret;
}

template <bool BE>
ItemParameterTable::ToolV4 ItemParameterTable::ToolV3T<BE>::to_v4() const {
  ToolV4 ret;
  ret.base.id = this->base.id.load();
  ret.base.type = this->base.type.load();
  ret.base.skin = this->base.skin.load();
  ret.amount = this->amount.load();
  ret.tech = this->tech.load();
  ret.cost = this->cost.load();
  ret.item_flags = this->item_flags.load();
  return ret;
}

template <bool BE>
size_t indirect_lookup_2d_count(const phosg::StringReader& r, size_t root_offset, size_t co_index) {
  return r.pget<ArrayRefT<BE>>(root_offset + sizeof(ArrayRefT<BE>) * co_index).count;
}

template <typename T, bool BE>
const T& indirect_lookup_2d(const phosg::StringReader& r, size_t root_offset, size_t co_index, size_t item_index) {
  const auto& co = r.pget<ArrayRefT<BE>>(root_offset + sizeof(ArrayRefT<BE>) * co_index);
  if (item_index >= co.count) {
    throw out_of_range("item ID out of range");
  }
  return r.pget<T>(co.offset + sizeof(T) * item_index);
}

size_t ItemParameterTable::num_weapons_in_class(uint8_t data1_1) const {
  if (data1_1 >= this->num_weapon_classes) {
    throw out_of_range("weapon ID out of range");
  }
  if (this->offsets_dc_protos) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_dc_protos->weapon_table, data1_1);
  } else if (this->offsets_v1_v2) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v1_v2->weapon_table, data1_1);
  } else if (this->offsets_gc_nte) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_gc_nte->weapon_table, data1_1);
  } else if (this->offsets_v3_le) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v3_le->weapon_table, data1_1);
  } else if (this->offsets_v3_be) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_v3_be->weapon_table, data1_1);
  } else if (this->offsets_v4) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v4->weapon_table, data1_1);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::WeaponV4& ItemParameterTable::get_weapon(uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 >= this->num_weapon_classes) {
    throw out_of_range("weapon ID out of range");
  }

  if (this->offsets_v4) {
    return indirect_lookup_2d<WeaponV4, false>(this->r, this->offsets_v4->weapon_table, data1_1, data1_2);
  }

  uint16_t key = (data1_1 << 8) | data1_2;
  try {
    return this->parsed_weapons.at(key);
  } catch (const std::out_of_range&) {
    WeaponV4 def_v4;
    if (this->offsets_dc_protos) {
      def_v4 = indirect_lookup_2d<WeaponDCProtos, false>(this->r, this->offsets_dc_protos->weapon_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v1_v2) {
      def_v4 = indirect_lookup_2d<WeaponV1V2, false>(this->r, this->offsets_v1_v2->weapon_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_gc_nte) {
      def_v4 = indirect_lookup_2d<WeaponGCNTE, true>(this->r, this->offsets_gc_nte->weapon_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v3_le) {
      def_v4 = indirect_lookup_2d<WeaponV3, false>(this->r, this->offsets_v3_le->weapon_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v3_be) {
      def_v4 = indirect_lookup_2d<WeaponV3BE, true>(this->r, this->offsets_v3_be->weapon_table, data1_1, data1_2).to_v4();
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }
    return this->parsed_weapons.emplace(key, def_v4).first->second;
  }
}

size_t ItemParameterTable::num_armors_or_shields_in_class(uint8_t data1_1) const {
  if ((data1_1 < 1) || (data1_1 > 2)) {
    throw out_of_range("armor/shield class ID out of range");
  }
  if (this->offsets_dc_protos) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_dc_protos->armor_table, data1_1 - 1);
  } else if (this->offsets_v1_v2) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v1_v2->armor_table, data1_1 - 1);
  } else if (this->offsets_gc_nte) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_gc_nte->armor_table, data1_1 - 1);
  } else if (this->offsets_v3_le) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v3_le->armor_table, data1_1 - 1);
  } else if (this->offsets_v3_be) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_v3_be->armor_table, data1_1 - 1);
  } else if (this->offsets_v4) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v4->armor_table, data1_1 - 1);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::ArmorOrShieldV4& ItemParameterTable::get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
  if ((data1_1 < 1) || (data1_1 > 2)) {
    throw out_of_range("armor/shield class ID out of range");
  }

  if (this->offsets_v4) {
    return indirect_lookup_2d<ArmorOrShieldV4, false>(this->r, this->offsets_v4->armor_table, data1_1 - 1, data1_2);
  }

  auto& parsed_vec = (data1_1 == 2) ? this->parsed_shields : this->parsed_armors;
  try {
    const auto& ret = parsed_vec.at(data1_2);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    ArmorOrShieldV4 def_v4;

    if (this->offsets_dc_protos) {
      def_v4 = indirect_lookup_2d<ArmorOrShieldDCProtos, false>(this->r, this->offsets_dc_protos->armor_table, data1_1 - 1, data1_2).to_v4();
    } else if (this->offsets_v1_v2) {
      def_v4 = indirect_lookup_2d<ArmorOrShieldV1V2, false>(this->r, this->offsets_v1_v2->armor_table, data1_1 - 1, data1_2).to_v4();
    } else if (this->offsets_gc_nte) {
      def_v4 = indirect_lookup_2d<ArmorOrShieldV3BE, true>(this->r, this->offsets_gc_nte->armor_table, data1_1 - 1, data1_2).to_v4();
    } else if (this->offsets_v3_le) {
      def_v4 = indirect_lookup_2d<ArmorOrShieldV3, false>(this->r, this->offsets_v3_le->armor_table, data1_1 - 1, data1_2).to_v4();
    } else if (this->offsets_v3_be) {
      def_v4 = indirect_lookup_2d<ArmorOrShieldV3BE, true>(this->r, this->offsets_v3_be->armor_table, data1_1 - 1, data1_2).to_v4();
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }
    if (data1_2 >= parsed_vec.size()) {
      parsed_vec.resize(data1_2 + 1);
    }
    parsed_vec[data1_2] = def_v4;
    return parsed_vec[data1_2];
  }
}

size_t ItemParameterTable::num_units() const {
  if (this->offsets_dc_protos) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_dc_protos->unit_table, 0);
  } else if (this->offsets_v1_v2) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v1_v2->unit_table, 0);
  } else if (this->offsets_gc_nte) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_gc_nte->unit_table, 0);
  } else if (this->offsets_v3_le) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v3_le->unit_table, 0);
  } else if (this->offsets_v3_be) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_v3_be->unit_table, 0);
  } else if (this->offsets_v4) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v4->unit_table, 0);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::UnitV4& ItemParameterTable::get_unit(uint8_t data1_2) const {
  if (this->offsets_v4) {
    return indirect_lookup_2d<UnitV4, false>(this->r, this->offsets_v4->unit_table, 0, data1_2);
  }

  try {
    const auto& ret = this->parsed_units.at(data1_2);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    UnitV4 def_v4;
    if (this->offsets_dc_protos) {
      def_v4 = indirect_lookup_2d<UnitDCProtos, false>(this->r, this->offsets_dc_protos->unit_table, 0, data1_2).to_v4();
    } else if (this->offsets_v1_v2) {
      def_v4 = indirect_lookup_2d<UnitV1V2, false>(this->r, this->offsets_v1_v2->unit_table, 0, data1_2).to_v4();
    } else if (this->offsets_gc_nte) {
      def_v4 = indirect_lookup_2d<UnitV3BE, true>(this->r, this->offsets_gc_nte->unit_table, 0, data1_2).to_v4();
    } else if (this->offsets_v3_le) {
      def_v4 = indirect_lookup_2d<UnitV3, false>(this->r, this->offsets_v3_le->unit_table, 0, data1_2).to_v4();
    } else if (this->offsets_v3_be) {
      def_v4 = indirect_lookup_2d<UnitV3BE, true>(this->r, this->offsets_v3_be->unit_table, 0, data1_2).to_v4();
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }
    if (data1_2 >= this->parsed_units.size()) {
      this->parsed_units.resize(data1_2 + 1);
    }
    this->parsed_units[data1_2] = def_v4;
    return this->parsed_units[data1_2];
  }
}

size_t ItemParameterTable::num_mags() const {
  if (this->offsets_dc_protos) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_dc_protos->mag_table, 0);
  } else if (this->offsets_v1_v2) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v1_v2->mag_table, 0);
  } else if (this->offsets_gc_nte) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_gc_nte->mag_table, 0);
  } else if (this->offsets_v3_le) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v3_le->mag_table, 0);
  } else if (this->offsets_v3_be) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_v3_be->mag_table, 0);
  } else if (this->offsets_v4) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v4->mag_table, 0);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::MagV4& ItemParameterTable::get_mag(uint8_t data1_1) const {
  if (this->offsets_v4) {
    return indirect_lookup_2d<MagV4, false>(this->r, this->offsets_v4->mag_table, 0, data1_1);
  }

  try {
    const auto& ret = this->parsed_mags.at(data1_1);
    if (ret.base.id == 0xFFFFFFFF) {
      throw out_of_range("cache entry not populated");
    }
    return ret;
  } catch (const std::out_of_range&) {
    MagV4 def_v4;
    if (this->offsets_dc_protos) {
      def_v4 = indirect_lookup_2d<MagV1, false>(this->r, this->offsets_dc_protos->mag_table, 0, data1_1).to_v4();
    } else if (this->offsets_v1_v2) {
      if (is_v1(this->version)) {
        def_v4 = indirect_lookup_2d<MagV1, false>(this->r, this->offsets_v1_v2->mag_table, 0, data1_1).to_v4();
      } else {
        def_v4 = indirect_lookup_2d<MagV2, false>(this->r, this->offsets_v1_v2->mag_table, 0, data1_1).to_v4();
      }
    } else if (this->offsets_gc_nte) {
      def_v4 = indirect_lookup_2d<MagV3BE, true>(this->r, this->offsets_gc_nte->mag_table, 0, data1_1).to_v4();
    } else if (this->offsets_v3_le) {
      def_v4 = indirect_lookup_2d<MagV3, false>(this->r, this->offsets_v3_le->mag_table, 0, data1_1).to_v4();
    } else if (this->offsets_v3_be) {
      def_v4 = indirect_lookup_2d<MagV3BE, true>(this->r, this->offsets_v3_be->mag_table, 0, data1_1).to_v4();
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }
    if (data1_1 >= this->parsed_mags.size()) {
      this->parsed_mags.resize(data1_1 + 1);
    }
    this->parsed_mags[data1_1] = def_v4;
    return this->parsed_mags[data1_1];
  }
}

size_t ItemParameterTable::num_tools_in_class(uint8_t data1_1) const {
  if (data1_1 >= this->num_tool_classes) {
    throw out_of_range("tool class ID out of range");
  }
  if (this->offsets_dc_protos) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_dc_protos->tool_table, data1_1);
  } else if (this->offsets_v1_v2) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v1_v2->tool_table, data1_1);
  } else if (this->offsets_gc_nte) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_gc_nte->tool_table, data1_1);
  } else if (this->offsets_v3_le) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v3_le->tool_table, data1_1);
  } else if (this->offsets_v3_be) {
    return indirect_lookup_2d_count<true>(this->r, this->offsets_v3_be->tool_table, data1_1);
  } else if (this->offsets_v4) {
    return indirect_lookup_2d_count<false>(this->r, this->offsets_v4->tool_table, data1_1);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::ToolV4& ItemParameterTable::get_tool(uint8_t data1_1, uint8_t data1_2) const {
  if (data1_1 >= this->num_tool_classes) {
    throw out_of_range("tool class ID out of range");
  }

  if (this->offsets_v4) {
    return indirect_lookup_2d<ToolV4, false>(this->r, this->offsets_v4->tool_table, data1_1, data1_2);
  }

  uint16_t key = (data1_1 << 8) | data1_2;
  try {
    return this->parsed_tools.at(key);
  } catch (const std::out_of_range&) {
    ToolV4 def_v4;

    if (this->offsets_dc_protos) {
      def_v4 = indirect_lookup_2d<ToolV1V2, false>(this->r, this->offsets_dc_protos->tool_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v1_v2) {
      def_v4 = indirect_lookup_2d<ToolV1V2, false>(this->r, this->offsets_v1_v2->tool_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_gc_nte) {
      def_v4 = indirect_lookup_2d<ToolV3BE, true>(this->r, this->offsets_gc_nte->tool_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v3_le) {
      def_v4 = indirect_lookup_2d<ToolV3, false>(this->r, this->offsets_v3_le->tool_table, data1_1, data1_2).to_v4();
    } else if (this->offsets_v3_be) {
      def_v4 = indirect_lookup_2d<ToolV3BE, true>(this->r, this->offsets_v3_be->tool_table, data1_1, data1_2).to_v4();
    } else {
      throw logic_error("table is not v2, v3, or v4");
    }

    return this->parsed_tools.emplace(key, def_v4).first->second;
  }
}

template <typename ToolDefT, bool BE>
pair<uint8_t, uint8_t> ItemParameterTable::find_tool_by_id_t(uint32_t tool_table_offset, uint32_t item_id) const {
  const auto* cos = &this->r.pget<ArrayRefT<BE>>(
      tool_table_offset, this->num_tool_classes * sizeof(ArrayRefT<BE>));
  for (size_t z = 0; z < this->num_tool_classes; z++) {
    const auto& co = cos[z];
    const auto* defs = &this->r.pget<ToolDefT>(co.offset, sizeof(ToolDefT) * co.count);
    for (size_t y = 0; y < co.count; y++) {
      if (defs[y].base.id == item_id) {
        return make_pair(z, y);
      }
    }
  }
  throw out_of_range(phosg::string_printf("invalid tool class %08" PRIX32, item_id));
}

pair<uint8_t, uint8_t> ItemParameterTable::find_tool_by_id(uint32_t item_id) const {
  if (this->offsets_dc_protos) {
    return this->find_tool_by_id_t<ToolV1V2, false>(this->offsets_dc_protos->tool_table, item_id);
  } else if (this->offsets_v1_v2) {
    return this->find_tool_by_id_t<ToolV1V2, false>(this->offsets_v1_v2->tool_table, item_id);
  } else if (this->offsets_gc_nte) {
    return this->find_tool_by_id_t<ToolV3BE, true>(this->offsets_gc_nte->tool_table, item_id);
  } else if (this->offsets_v3_le) {
    return this->find_tool_by_id_t<ToolV3, false>(this->offsets_v3_le->tool_table, item_id);
  } else if (this->offsets_v3_be) {
    return this->find_tool_by_id_t<ToolV3BE, true>(this->offsets_v3_be->tool_table, item_id);
  } else if (this->offsets_v4) {
    return this->find_tool_by_id_t<ToolV4, false>(this->offsets_v4->tool_table, item_id);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

variant<
    const ItemParameterTable::WeaponV4*,
    const ItemParameterTable::ArmorOrShieldV4*,
    const ItemParameterTable::UnitV4*,
    const ItemParameterTable::MagV4*,
    const ItemParameterTable::ToolV4*>
ItemParameterTable::definition_for_primary_identifier(uint32_t primary_identifier) const {
  uint8_t data1_0 = (primary_identifier >> 24) & 0xFF;
  uint8_t data1_1 = (primary_identifier >> 16) & 0xFF;
  uint8_t data1_2 = (primary_identifier >> 8) & 0xFF;
  switch (data1_0) {
    case 0:
      return &this->get_weapon(data1_1, data1_2);
    case 1:
      switch (data1_1) {
        case 1:
        case 2:
          return &this->get_armor_or_shield(data1_1, data1_2);
        case 3:
          return &this->get_unit(data1_2);
        default:
          throw runtime_error("invalid primary identifier");
      }
    case 2:
      return &this->get_mag(data1_1);
    case 3:
      return &this->get_tool(data1_1, data1_2);
    default:
      throw runtime_error("invalid primary identifier");
  }
}

template <bool BE, typename OffsetsT>
float ItemParameterTable::get_sale_divisor_t(const OffsetsT* offsets, uint8_t data1_0, uint8_t data1_1) const {
  switch (data1_0) {
    case 0:
      if (data1_1 >= this->num_weapon_classes) {
        return 0.0f;
      }
      return this->r.pget<F32T<BE>>(offsets->weapon_sale_divisor_table + data1_1 * sizeof(F32T<BE>));

    case 1: {
      const auto& divisors = this->r.pget<NonWeaponSaleDivisorsT<BE>>(offsets->sale_divisor_table);
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
      const auto& divisors = this->r.pget<NonWeaponSaleDivisorsT<BE>>(offsets->sale_divisor_table);
      return divisors.mag_divisor;
    }

    default:
      return 0.0f;
  }
}

float ItemParameterTable::get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
  if (this->offsets_dc_protos) {
    return this->get_sale_divisor_t<false>(this->offsets_dc_protos, data1_0, data1_1);
  } else if (this->offsets_v1_v2) {
    return this->get_sale_divisor_t<false>(this->offsets_v1_v2, data1_0, data1_1);
  } else if (this->offsets_gc_nte) {
    return this->get_sale_divisor_t<true>(this->offsets_gc_nte, data1_0, data1_1);
  } else if (this->offsets_v3_le) {
    return this->get_sale_divisor_t<false>(this->offsets_v3_le, data1_0, data1_1);
  } else if (this->offsets_v3_be) {
    return this->get_sale_divisor_t<true>(this->offsets_v3_be, data1_0, data1_1);
  } else if (this->offsets_v4) {
    return this->get_sale_divisor_t<false>(this->offsets_v4, data1_0, data1_1);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

const ItemParameterTable::MagFeedResult& ItemParameterTable::get_mag_feed_result(
    uint8_t table_index, uint8_t item_index) const {
  if (table_index >= 8) {
    throw out_of_range("invalid mag feed table index");
  }
  if (item_index >= 11) {
    throw out_of_range("invalid mag feed item index");
  }

  uint32_t offset;
  if (this->offsets_dc_protos) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets>(this->offsets_dc_protos->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v1_v2) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets>(this->offsets_v1_v2->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_gc_nte) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsetsBE>(this->offsets_gc_nte->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v3_le) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets>(this->offsets_v3_le->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v3_be) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsetsBE>(this->offsets_v3_be->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else if (this->offsets_v4) {
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsets>(this->offsets_v4->mag_feed_table);
    offset = table_offsets.offsets[table_index];
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  return this->r.pget<MagFeedResultsList>(offset)[item_index];
}

uint8_t ItemParameterTable::get_item_stars(uint32_t item_id) const {
  uint32_t base_offset;
  if (this->offsets_dc_protos) {
    base_offset = this->offsets_dc_protos->star_value_table;
  } else if (this->offsets_v1_v2) {
    base_offset = this->offsets_v1_v2->star_value_table;
  } else if (this->offsets_gc_nte) {
    base_offset = this->offsets_gc_nte->star_value_table;
  } else if (this->offsets_v3_le) {
    base_offset = this->offsets_v3_le->star_value_table;
  } else if (this->offsets_v3_be) {
    base_offset = this->offsets_v3_be->star_value_table;
  } else if (this->offsets_v4) {
    base_offset = this->offsets_v4->star_value_table;
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  return ((item_id >= this->item_stars_first_id) && (item_id < this->item_stars_last_id))
      ? this->r.pget_u8(base_offset + item_id - this->item_stars_first_id)
      : 0;
}

uint8_t ItemParameterTable::get_special_stars(uint8_t special) const {
  return ((special & 0x3F) && !(special & 0x80))
      ? this->get_item_stars(special + this->special_stars_begin_index)
      : 0;
}

const ItemParameterTable::Special& ItemParameterTable::get_special(uint8_t special) const {
  special &= 0x3F;
  if (special >= this->num_specials) {
    throw out_of_range("invalid special index");
  }

  if (this->offsets_dc_protos) {
    return this->r.pget<Special>(this->offsets_dc_protos->special_data_table + sizeof(Special) * special);
  } else if (this->offsets_v1_v2) {
    return this->r.pget<Special>(this->offsets_v1_v2->special_data_table + sizeof(Special) * special);
  } else if (this->offsets_v3_le) {
    return this->r.pget<Special>(this->offsets_v3_le->special_data_table + sizeof(Special) * special);
  } else if (this->offsets_gc_nte) {
    if ((special >= this->parsed_specials.size()) || (this->parsed_specials[special].type != 0xFFFF)) {
      if (special >= this->parsed_specials.size()) {
        this->parsed_specials.resize(special + 1);
      }
      const auto& sp_be = this->r.pget<SpecialBE>(this->offsets_gc_nte->special_data_table + sizeof(SpecialBE) * special);
      this->parsed_specials[special].type = sp_be.type.load();
      this->parsed_specials[special].amount = sp_be.amount.load();
    }
    return this->parsed_specials[special];
  } else if (this->offsets_v3_be) {
    if ((special >= this->parsed_specials.size()) || (this->parsed_specials[special].type != 0xFFFF)) {
      if (special >= this->parsed_specials.size()) {
        this->parsed_specials.resize(special + 1);
      }
      const auto& sp_be = this->r.pget<SpecialBE>(this->offsets_v3_be->special_data_table + sizeof(SpecialBE) * special);
      this->parsed_specials[special].type = sp_be.type.load();
      this->parsed_specials[special].amount = sp_be.amount.load();
    }
    return this->parsed_specials[special];
  } else if (this->offsets_v4) {
    return this->r.pget<Special>(this->offsets_v4->special_data_table + sizeof(Special) * special);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

uint8_t ItemParameterTable::get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
  if (char_class >= 12) {
    throw out_of_range("invalid character class");
  }
  if (tech_num >= 19) {
    throw out_of_range("invalid technique number");
  }

  if (this->offsets_dc_protos || this->offsets_v1_v2) {
    if ((tech_num == 14) || (tech_num == 17)) { // Ryuker or Reverser
      return 0;
    } else {
      return ((char_class == 6) || (char_class == 7) || (char_class == 8) || (char_class == 10)) ? 29 : 14;
    }
  } else if (this->offsets_gc_nte) {
    return r.pget_u8(this->offsets_gc_nte->max_tech_level_table + tech_num * 12 + char_class);
  } else if (this->offsets_v3_le) {
    return r.pget_u8(this->offsets_v3_le->max_tech_level_table + tech_num * 12 + char_class);
  } else if (this->offsets_v3_be) {
    return r.pget_u8(this->offsets_v3_be->max_tech_level_table + tech_num * 12 + char_class);
  } else if (this->offsets_v4) {
    return r.pget_u8(this->offsets_v4->max_tech_level_table + tech_num * 12 + char_class);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

uint8_t ItemParameterTable::get_weapon_v1_replacement(uint8_t data1_1) const {
  uint32_t offset;
  if (this->offsets_dc_protos) {
    offset = this->offsets_dc_protos->v1_replacement_table;
  } else if (this->offsets_v1_v2) {
    offset = this->offsets_v1_v2->v1_replacement_table;
  } else if (this->offsets_gc_nte) {
    offset = this->offsets_gc_nte->v1_replacement_table;
  } else if (this->offsets_v3_le) {
    offset = this->offsets_v3_le->v1_replacement_table;
  } else if (this->offsets_v3_be) {
    offset = this->offsets_v3_be->v1_replacement_table;
  } else if (this->offsets_v4) {
    offset = this->offsets_v4->v1_replacement_table;
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  return (data1_1 < this->num_weapon_classes)
      ? this->r.pget_u8(offset + data1_1)
      : 0x00;
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

uint32_t ItemParameterTable::get_item_team_points(const ItemData& item) const {
  switch (item.data1[0]) {
    case 0:
      return this->get_weapon(item.data1[1], item.data1[2]).base.team_points;
    case 1:
      if (item.data1[1] == 3) {
        return this->get_unit(item.data1[2]).base.team_points;
      } else if ((item.data1[1] == 1) || (item.data1[1] == 2)) {
        return this->get_armor_or_shield(item.data1[1], item.data1[2]).base.team_points;
      }
      throw runtime_error("invalid item");
    case 2:
      return this->get_mag(item.data1[1]).base.team_points;
    case 3:
      if (item.data1[1] == 2) {
        return this->get_tool(2, item.data1[4]).base.team_points;
      } else {
        return this->get_tool(item.data1[1], item.data1[2]).base.team_points;
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
    return (def.item_flags & 0x80) ? 12 : 0;
  } else {
    return 0;
  }
}

uint8_t ItemParameterTable::get_item_adjusted_stars(const ItemData& item, bool ignore_unidentified) const {
  uint8_t ret = this->get_item_base_stars(item);
  if (item.data1[0] == 0) {
    bool is_unidentified = (!ignore_unidentified) && (item.data1[4] & 0x80);
    if (ret < 9) {
      if (!is_unidentified) {
        ret += this->get_special_stars(item.data1[4]);
      }
    } else if (is_unidentified) {
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
  try {
    return (this->get_item_base_stars(item) >= 9);
  } catch (const out_of_range&) {
    return false;
  }
}

bool ItemParameterTable::is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const {
  uint32_t offset, count;
  if (this->offsets_dc_protos || this->offsets_v1_v2 || this->offsets_gc_nte) {
    return false;
  } else if (this->offsets_v3_le) {
    const auto& co = this->r.pget<ArrayRef>(this->offsets_v3_le->unsealable_table);
    offset = co.offset;
    count = co.count;
  } else if (this->offsets_v3_be) {
    const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3_be->unsealable_table);
    offset = co.offset;
    count = co.count;
  } else if (this->offsets_v4) {
    const auto& co = this->r.pget<ArrayRef>(this->offsets_v4->unsealable_table);
    offset = co.offset;
    count = co.count;
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }

  const auto* defs = &this->r.pget<UnsealableItem>(offset, count * sizeof(UnsealableItem));
  for (size_t z = 0; z < count; z++) {
    if ((defs[z].item[0] == data1_0) &&
        (defs[z].item[1] == data1_1) &&
        (defs[z].item[2] == data1_2)) {
      return true;
    }
  }
  return false;
}

bool ItemParameterTable::is_unsealable_item(const ItemData& item) const {
  return this->is_unsealable_item(item.data1[0], item.data1[1], item.data1[2]);
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
    if (this->offsets_dc_protos || this->offsets_v1_v2 || this->offsets_gc_nte) {
      static const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>> empty_map;
      return empty_map;
    } else if (this->offsets_v3_le) {
      const auto& co = this->r.pget<ArrayRef>(this->offsets_v3_le->combination_table);
      offset = co.offset;
      count = co.count;
    } else if (this->offsets_v3_be) {
      const auto& co = this->r.pget<ArrayRefBE>(this->offsets_v3_be->combination_table);
      offset = co.offset;
      count = co.count;
    } else if (this->offsets_v4) {
      const auto& co = this->r.pget<ArrayRef>(this->offsets_v4->combination_table);
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

template <bool BE>
size_t ItemParameterTable::num_events_t(uint32_t base_offset) const {
  return this->r.pget<ArrayRefT<BE>>(base_offset).count;
}

size_t ItemParameterTable::num_events() const {
  if (this->offsets_dc_protos || this->offsets_v1_v2 || this->offsets_gc_nte) {
    return 0;
  } else if (this->offsets_v3_le) {
    return this->num_events_t<false>(this->offsets_v3_le->unwrap_table);
  } else if (this->offsets_v3_be) {
    return this->num_events_t<true>(this->offsets_v3_be->unwrap_table);
  } else if (this->offsets_v4) {
    return this->num_events_t<false>(this->offsets_v4->unwrap_table);
  } else {
    throw logic_error("table is not v2, v3, or v4");
  }
}

template <bool BE>
std::pair<const ItemParameterTable::EventItem*, size_t> ItemParameterTable::get_event_items_t(
    uint32_t base_offset, uint8_t event_number) const {
  const auto& co = this->r.pget<ArrayRefT<BE>>(base_offset);
  if (event_number >= co.count) {
    throw out_of_range("invalid event number");
  }
  const auto& event_co = this->r.pget<ArrayRefT<BE>>(co.offset + sizeof(ArrayRefT<BE>) * event_number);
  const auto* defs = &this->r.pget<EventItem>(event_co.offset, event_co.count * sizeof(EventItem));
  return make_pair(defs, event_co.count);
}

std::pair<const ItemParameterTable::EventItem*, size_t> ItemParameterTable::get_event_items(uint8_t event_number) const {
  if (this->offsets_dc_protos || this->offsets_v1_v2 || this->offsets_gc_nte) {
    return make_pair(nullptr, 0);
  } else if (this->offsets_v3_le) {
    return this->get_event_items_t<false>(this->offsets_v3_le->unwrap_table, event_number);
  } else if (this->offsets_v3_be) {
    return this->get_event_items_t<true>(this->offsets_v3_be->unwrap_table, event_number);
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
      const auto& def = (item.data1[1] == 2)
          ? this->get_tool(2, item.data1[4])
          : this->get_tool(item.data1[1], item.data1[2]);
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
