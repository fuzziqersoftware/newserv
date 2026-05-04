#include "ItemParameterTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities

template <>
ServerDropMode phosg::enum_for_name<ServerDropMode>(const char* name) {
  if (!strcmp(name, "DISABLED")) {
    return ServerDropMode::DISABLED;
  } else if (!strcmp(name, "CLIENT")) {
    return ServerDropMode::CLIENT;
  } else if (!strcmp(name, "SERVER_SHARED")) {
    return ServerDropMode::SERVER_SHARED;
  } else if (!strcmp(name, "SERVER_PRIVATE")) {
    return ServerDropMode::SERVER_PRIVATE;
  } else if (!strcmp(name, "SERVER_DUPLICATE")) {
    return ServerDropMode::SERVER_DUPLICATE;
  } else {
    throw runtime_error("invalid drop mode");
  }
}

template <>
const char* phosg::name_for_enum<ServerDropMode>(ServerDropMode value) {
  switch (value) {
    case ServerDropMode::DISABLED:
      return "DISABLED";
    case ServerDropMode::CLIENT:
      return "CLIENT";
    case ServerDropMode::SERVER_SHARED:
      return "SERVER_SHARED";
    case ServerDropMode::SERVER_PRIVATE:
      return "SERVER_PRIVATE";
    case ServerDropMode::SERVER_DUPLICATE:
      return "SERVER_DUPLICATE";
    default:
      throw runtime_error("invalid drop mode");
  }
}

static void u32_to_item_code(parray<uint8_t, 3>& decoded, uint32_t item) {
  decoded[0] = item >> 16;
  decoded[1] = item >> 8;
  decoded[2] = item;
}

static uint32_t item_code_to_u32(const parray<uint8_t, 3>& item) {
  return (item[0] << 16) | (item[1] << 8) | item[2];
}
static uint32_t item_code_to_u32(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) {
  return (data1_0 << 16) | (data1_1 << 8) | data1_2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parsed structures

void ItemParameterTable::ItemBase::parse_base_from_json(const phosg::JSON& json) {
  this->id = json.get_int("ID");
  this->type = json.get_int("Type");
  this->skin = json.get_int("Skin");
  this->team_points = json.get_int("TeamPoints");
}
phosg::JSON ItemParameterTable::ItemBase::json() const {
  return phosg::JSON::dict(
      {{"ID", this->id}, {"Type", this->type}, {"Skin", this->skin}, {"TeamPoints", this->team_points}});
}

ItemParameterTable::Weapon ItemParameterTable::Weapon::from_json(const phosg::JSON& json) {
  ItemParameterTable::Weapon ret;
  ret.parse_base_from_json(json);
  ret.class_flags = json.get_int("ClassFlags");
  ret.atp_min = json.get_int("ATPMin");
  ret.atp_max = json.get_int("ATPMax");
  ret.atp_required = json.get_int("ATPRequired");
  ret.mst_required = json.get_int("MSTRequired");
  ret.ata_required = json.get_int("ATARequired");
  ret.mst = json.get_int("MST");
  ret.max_grind = json.get_int("MaxGrind");
  ret.photon = json.get_int("Photon");
  ret.special = json.get_int("Special");
  ret.ata = json.get_int("ATA");
  ret.stat_boost_entry_index = json.get_int("StatBoostEntryIndex");
  ret.projectile = json.get_int("Projectile");
  ret.trail1_x = json.get_int("Trail1X");
  ret.trail1_y = json.get_int("Trail1Y");
  ret.trail2_x = json.get_int("Trail2X");
  ret.trail2_y = json.get_int("Trail2Y");
  ret.color = json.get_int("Color");
  const auto& unknown_a1 = json.get_list("UnknownA1");
  ret.unknown_a1[0] = unknown_a1[0]->as_int();
  ret.unknown_a1[1] = unknown_a1[1]->as_int();
  ret.unknown_a1[2] = unknown_a1[2]->as_int();
  ret.unknown_a4 = json.get_int("UnknownA4");
  ret.unknown_a5 = json.get_int("UnknownA5");
  ret.tech_boost_entry_index = json.get_int("TechBoostEntryIndex");
  ret.behavior_flags = json.get_int("BehaviorFlags");
  return ret;
}
phosg::JSON ItemParameterTable::Weapon::json() const {
  phosg::JSON ret = this->ItemBase::json();
  ret.emplace("ClassFlags", this->class_flags);
  ret.emplace("ATPMin", this->atp_min);
  ret.emplace("ATPMax", this->atp_max);
  ret.emplace("ATPRequired", this->atp_required);
  ret.emplace("MSTRequired", this->mst_required);
  ret.emplace("ATARequired", this->ata_required);
  ret.emplace("MST", this->mst);
  ret.emplace("MaxGrind", this->max_grind);
  ret.emplace("Photon", this->photon);
  ret.emplace("Special", this->special);
  ret.emplace("ATA", this->ata);
  ret.emplace("StatBoostEntryIndex", this->stat_boost_entry_index);
  ret.emplace("Projectile", this->projectile);
  ret.emplace("Trail1X", this->trail1_x);
  ret.emplace("Trail1Y", this->trail1_y);
  ret.emplace("Trail2X", this->trail2_x);
  ret.emplace("Trail2Y", this->trail2_y);
  ret.emplace("Color", this->color);
  ret.emplace("UnknownA1", phosg::JSON::list({this->unknown_a1[0], this->unknown_a1[1], this->unknown_a1[2]}));
  ret.emplace("UnknownA4", this->unknown_a4);
  ret.emplace("UnknownA5", this->unknown_a5);
  ret.emplace("TechBoostEntryIndex", this->tech_boost_entry_index);
  ret.emplace("BehaviorFlags", this->behavior_flags);
  return ret;
}

ItemParameterTable::ArmorOrShield ItemParameterTable::ArmorOrShield::from_json(const phosg::JSON& json) {
  ItemParameterTable::ArmorOrShield ret;
  ret.parse_base_from_json(json);
  ret.dfp = json.get_int("DFP");
  ret.evp = json.get_int("EVP");
  ret.block_particle = json.get_int("BlockParticle");
  ret.block_effect = json.get_int("BlockEffect");
  ret.class_flags = json.get_int("ClassFlags");
  ret.required_level = json.get_int("RequiredLevel");
  ret.efr = json.get_int("EFR");
  ret.eth = json.get_int("ETH");
  ret.eic = json.get_int("EIC");
  ret.edk = json.get_int("EDK");
  ret.elt = json.get_int("ELT");
  ret.dfp_range = json.get_int("DFPRange");
  ret.evp_range = json.get_int("EVPRange");
  ret.stat_boost_entry_index = json.get_int("StatBoostEntryIndex");
  ret.tech_boost_entry_index = json.get_int("TechBoostEntryIndex");
  ret.flags_type = json.get_int("FlagsType");
  ret.unknown_a4 = json.get_int("UnknownA4");
  return ret;
}
phosg::JSON ItemParameterTable::ArmorOrShield::json() const {
  phosg::JSON ret = this->ItemBase::json();
  ret.emplace("DFP", this->dfp);
  ret.emplace("EVP", this->evp);
  ret.emplace("BlockParticle", this->block_particle);
  ret.emplace("BlockEffect", this->block_effect);
  ret.emplace("ClassFlags", this->class_flags);
  ret.emplace("RequiredLevel", this->required_level);
  ret.emplace("EFR", this->efr);
  ret.emplace("ETH", this->eth);
  ret.emplace("EIC", this->eic);
  ret.emplace("EDK", this->edk);
  ret.emplace("ELT", this->elt);
  ret.emplace("DFPRange", this->dfp_range);
  ret.emplace("EVPRange", this->evp_range);
  ret.emplace("StatBoostEntryIndex", this->stat_boost_entry_index);
  ret.emplace("TechBoostEntryIndex", this->tech_boost_entry_index);
  ret.emplace("FlagsType", this->flags_type);
  ret.emplace("UnknownA4", this->unknown_a4);
  return ret;
}

ItemParameterTable::Unit ItemParameterTable::Unit::from_json(const phosg::JSON& json) {
  ItemParameterTable::Unit ret;
  ret.parse_base_from_json(json);
  ret.stat = json.get_int("Stat");
  ret.stat_amount = json.get_int("StatAmount");
  ret.modifier_amount = json.get_int("ModifierAmount");
  return ret;
}
phosg::JSON ItemParameterTable::Unit::json() const {
  phosg::JSON ret = this->ItemBase::json();
  ret.emplace("Stat", this->stat);
  ret.emplace("StatAmount", this->stat_amount);
  ret.emplace("ModifierAmount", this->modifier_amount);
  return ret;
}

ItemParameterTable::Mag ItemParameterTable::Mag::from_json(const phosg::JSON& json) {
  ItemParameterTable::Mag ret;
  ret.parse_base_from_json(json);
  ret.feed_table = json.get_int("FeedTable");
  ret.photon_blast = json.get_int("PhotonBlast");
  ret.activation = json.get_int("Activation");
  ret.on_pb_full = json.get_int("OnPBFull");
  ret.on_low_hp = json.get_int("OnLowHP");
  ret.on_death = json.get_int("OnDeath");
  ret.on_boss = json.get_int("OnBoss");
  ret.on_pb_full_flag = json.get_int("OnPBFullFlag");
  ret.on_low_hp_flag = json.get_int("OnLowHPFlag");
  ret.on_death_flag = json.get_int("OnDeathFlag");
  ret.on_boss_flag = json.get_int("OnBossFlag");
  ret.class_flags = json.get_int("ClassFlags");
  return ret;
}
phosg::JSON ItemParameterTable::Mag::json() const {
  phosg::JSON ret = this->ItemBase::json();
  ret.emplace("FeedTable", this->feed_table);
  ret.emplace("PhotonBlast", this->photon_blast);
  ret.emplace("Activation", this->activation);
  ret.emplace("OnPBFull", this->on_pb_full);
  ret.emplace("OnLowHP", this->on_low_hp);
  ret.emplace("OnDeath", this->on_death);
  ret.emplace("OnBoss", this->on_boss);
  ret.emplace("OnPBFullFlag", this->on_pb_full_flag);
  ret.emplace("OnLowHPFlag", this->on_low_hp_flag);
  ret.emplace("OnDeathFlag", this->on_death_flag);
  ret.emplace("OnBossFlag", this->on_boss_flag);
  ret.emplace("ClassFlags", this->class_flags);
  return ret;
}

ItemParameterTable::Tool ItemParameterTable::Tool::from_json(const phosg::JSON& json) {
  ItemParameterTable::Tool ret;
  ret.parse_base_from_json(json);
  ret.amount = json.get_int("Amount");
  ret.tech = json.get_int("Tech");
  ret.cost = json.get_int("Cost");
  ret.item_flags = json.get_int("ItemFlags");
  return ret;
}
phosg::JSON ItemParameterTable::Tool::json() const {
  phosg::JSON ret = this->ItemBase::json();
  ret.emplace("Amount", this->amount);
  ret.emplace("Tech", this->tech);
  ret.emplace("Cost", this->cost);
  ret.emplace("ItemFlags", this->item_flags);
  return ret;
}

ItemParameterTable::MagFeedResult ItemParameterTable::MagFeedResult::from_json(const phosg::JSON& json) {
  ItemParameterTable::MagFeedResult ret;
  ret.def = json.get_int("DEF");
  ret.pow = json.get_int("POW");
  ret.dex = json.get_int("DEX");
  ret.mind = json.get_int("MIND");
  ret.iq = json.get_int("IQ");
  ret.synchro = json.get_int("Synchro");
  return ret;
}
phosg::JSON ItemParameterTable::MagFeedResult::json() const {
  return phosg::JSON::dict({
      {"DEF", this->def},
      {"POW", this->pow},
      {"DEX", this->dex},
      {"MIND", this->mind},
      {"IQ", this->iq},
      {"Synchro", this->synchro},
  });
}

ItemParameterTable::Special ItemParameterTable::Special::from_json(const phosg::JSON& json) {
  ItemParameterTable::Special ret;
  ret.type = json.get_int("Type");
  ret.amount = json.get_int("Amount");
  return ret;
}
phosg::JSON ItemParameterTable::Special::json() const {
  return phosg::JSON::dict({{"Type", this->type}, {"Amount", this->amount}});
}

ItemParameterTable::StatBoost ItemParameterTable::StatBoost::from_json(const phosg::JSON& json) {
  ItemParameterTable::StatBoost ret;
  ret.stat1 = json.get_int("Stat1");
  ret.amount1 = json.get_int("Amount1");
  ret.stat2 = json.get_int("Stat2");
  ret.amount2 = json.get_int("Amount2");
  return ret;
}
phosg::JSON ItemParameterTable::StatBoost::json() const {
  return phosg::JSON::dict({
      {"Stat1", this->stat1},
      {"Amount1", this->amount1},
      {"Stat2", this->stat2},
      {"Amount2", this->amount2},
  });
}

ItemParameterTable::ItemCombination ItemParameterTable::ItemCombination::from_json(const phosg::JSON& json) {
  ItemParameterTable::ItemCombination ret;
  u32_to_item_code(ret.used_item, json.get_int("UsedItem"));
  u32_to_item_code(ret.equipped_item, json.get_int("EquippedItem"));
  u32_to_item_code(ret.result_item, json.get_int("ResultItem"));
  ret.mag_level = json.get_int("MagLevel");
  ret.grind = json.get_int("Grind");
  ret.level = json.get_int("Level");
  ret.char_class = json.get_int("CharClass");
  return ret;
}
phosg::JSON ItemParameterTable::ItemCombination::json() const {
  return phosg::JSON::dict({
      {"UsedItem", item_code_to_u32(this->used_item)},
      {"EquippedItem", item_code_to_u32(this->equipped_item)},
      {"ResultItem", item_code_to_u32(this->result_item)},
      {"MagLevel", this->mag_level},
      {"Grind", this->grind},
      {"Level", this->level},
      {"CharClass", this->char_class},
  });
}

ItemParameterTable::TechBoost ItemParameterTable::TechBoost::from_json(const phosg::JSON& json) {
  ItemParameterTable::TechBoost ret;
  ret.tech_num1 = json.get_int("TechNum1");
  ret.flags1 = json.get_int("Flags1");
  ret.amount1 = json.get_int("Amount1");
  ret.tech_num2 = json.get_int("TechNum2");
  ret.flags2 = json.get_int("Flags2");
  ret.amount2 = json.get_int("Amount2");
  ret.tech_num3 = json.get_int("TechNum3");
  ret.flags3 = json.get_int("Flags3");
  ret.amount3 = json.get_int("Amount3");
  return ret;
}
phosg::JSON ItemParameterTable::TechBoost::json() const {
  return phosg::JSON::dict({
      {"TechNum1", this->tech_num1},
      {"Flags1", this->flags1},
      {"Amount1", this->amount1},
      {"TechNum2", this->tech_num2},
      {"Flags2", this->flags2},
      {"Amount2", this->amount2},
      {"TechNum3", this->tech_num3},
      {"Flags3", this->flags3},
      {"Amount3", this->amount3},
  });
}

ItemParameterTable::EventItem ItemParameterTable::EventItem::from_json(const phosg::JSON& json) {
  ItemParameterTable::EventItem ret;
  u32_to_item_code(ret.item, json.get_int("Item"));
  ret.probability = json.get_int("Probability");
  return ret;
}
phosg::JSON ItemParameterTable::EventItem::json() const {
  return phosg::JSON::dict({
      {"Item", item_code_to_u32(this->item)},
      {"Probability", this->probability},
  });
}

ItemParameterTable::UnsealableItem ItemParameterTable::UnsealableItem::from_json(const phosg::JSON& json) {
  ItemParameterTable::UnsealableItem ret;
  u32_to_item_code(ret.item, json.get_int("Item"));
  return ret;
}
phosg::JSON ItemParameterTable::UnsealableItem::json() const {
  return phosg::JSON::dict({{"Item", item_code_to_u32(this->item)}});
}

ItemParameterTable::NonWeaponSaleDivisors ItemParameterTable::NonWeaponSaleDivisors::from_json(const phosg::JSON& json) {
  ItemParameterTable::NonWeaponSaleDivisors ret;
  ret.armor_divisor = json.get_float("ArmorDivisor");
  ret.shield_divisor = json.get_float("ShieldDivisor");
  ret.unit_divisor = json.get_float("UnitDivisor");
  ret.mag_divisor = json.get_float("MagDivisor");
  return ret;
}
phosg::JSON ItemParameterTable::NonWeaponSaleDivisors::json() const {
  return phosg::JSON::dict({
      {"ArmorDivisor", this->armor_divisor},
      {"ShieldDivisor", this->shield_divisor},
      {"UnitDivisor", this->unit_divisor},
      {"MagDivisor", this->mag_divisor},
  });
}

ItemParameterTable::ShieldEffect ItemParameterTable::ShieldEffect::from_json(const phosg::JSON& json) {
  ItemParameterTable::ShieldEffect ret;
  ret.sound_id = json.get_int("SoundID");
  ret.unknown_a1 = json.get_int("UnknownA1");
  return ret;
}
phosg::JSON ItemParameterTable::ShieldEffect::json() const {
  return phosg::JSON::dict({{"SoundID", this->sound_id}, {"UnknownA1", this->unknown_a1}});
}

ItemParameterTable::PhotonColorEntry ItemParameterTable::PhotonColorEntry::from_json(const phosg::JSON& json) {
  ItemParameterTable::PhotonColorEntry ret;
  ret.unknown_a1 = json.get_int("UnknownA1");
  const auto& unknown_a2 = json.get_list("UnknownA2");
  const auto& unknown_a3 = json.get_list("UnknownA3");
  ret.unknown_a2.x = unknown_a2.at(0)->as_int();
  ret.unknown_a2.y = unknown_a2.at(1)->as_int();
  ret.unknown_a2.z = unknown_a2.at(2)->as_int();
  ret.unknown_a2.t = unknown_a2.at(3)->as_int();
  ret.unknown_a3.x = unknown_a3.at(0)->as_int();
  ret.unknown_a3.y = unknown_a3.at(1)->as_int();
  ret.unknown_a3.z = unknown_a3.at(2)->as_int();
  ret.unknown_a3.t = unknown_a3.at(3)->as_int();
  return ret;
}
phosg::JSON ItemParameterTable::PhotonColorEntry::json() const {
  return phosg::JSON::dict({
      {"UnknownA1", this->unknown_a1},
      {"UnknownA2", phosg::JSON::list({this->unknown_a2.x.load(), this->unknown_a2.y.load(), this->unknown_a2.z.load(), this->unknown_a2.t.load()})},
      {"UnknownA3", phosg::JSON::list({this->unknown_a3.x.load(), this->unknown_a3.y.load(), this->unknown_a3.z.load(), this->unknown_a3.t.load()})},
  });
}

ItemParameterTable::UnknownA1 ItemParameterTable::UnknownA1::from_json(const phosg::JSON& json) {
  ItemParameterTable::UnknownA1 ret;
  ret.unknown_a1 = json.get_int("UnknownA1");
  ret.unknown_a2 = json.get_int("UnknownA2");
  return ret;
}
phosg::JSON ItemParameterTable::UnknownA1::json() const {
  return phosg::JSON::dict({{"UnknownA1", this->unknown_a1}, {"UnknownA2", this->unknown_a2}});
}

ItemParameterTable::WeaponEffect ItemParameterTable::WeaponEffect::from_json(const phosg::JSON& json) {
  ItemParameterTable::WeaponEffect ret;
  ret.sound_id1 = json.get_int("SoundID1");
  ret.eff_value1 = json.get_int("EffectValue1");
  ret.sound_id2 = json.get_int("SoundID2");
  ret.eff_value2 = json.get_int("EffectValue2");
  std::string unknown_a5 = phosg::parse_data_string(json.get_string("UnknownA5"));
  for (size_t z = 0; z < std::min<size_t>(ret.unknown_a5.size(), unknown_a5.size()); z++) {
    ret.unknown_a5[z] = unknown_a5[z];
  }
  return ret;
}
phosg::JSON ItemParameterTable::WeaponEffect::json() const {
  return phosg::JSON::dict({
      {"SoundID1", this->sound_id1},
      {"EffectValue1", this->eff_value1},
      {"SoundID2", this->sound_id2},
      {"EffectValue2", this->eff_value2},
      {"UnknownA5", phosg::format_data_string(this->unknown_a5.data(), this->unknown_a5.size())},
  });
}

ItemParameterTable::WeaponRange ItemParameterTable::WeaponRange::from_json(const phosg::JSON& json) {
  ItemParameterTable::WeaponRange ret;
  ret.unknown_a1 = json.get_int("UnknownA1");
  ret.unknown_a2 = json.get_int("UnknownA2");
  ret.unknown_a3 = json.get_int("UnknownA3");
  ret.unknown_a4 = json.get_int("UnknownA4");
  ret.unknown_a5 = json.get_int("UnknownA5");
  return ret;
}
phosg::JSON ItemParameterTable::WeaponRange::json() const {
  return phosg::JSON::dict({
      {"UnknownA1", this->unknown_a1},
      {"UnknownA2", this->unknown_a2},
      {"UnknownA3", this->unknown_a3},
      {"UnknownA4", this->unknown_a4},
      {"UnknownA5", this->unknown_a5},
  });
}

ItemParameterTable::RangedSpecial ItemParameterTable::RangedSpecial::from_json(const phosg::JSON& json) {
  ItemParameterTable::RangedSpecial ret;
  ret.data1_1 = json.get_int("Data1_1");
  ret.data1_2 = json.get_int("Data1_2");
  ret.weapon_range_index = json.get_int("WeaponRangeIndex");
  ret.unknown_a1 = json.get_int("UnknownA1");
  return ret;
}
phosg::JSON ItemParameterTable::RangedSpecial::json() const {
  return phosg::JSON::dict({
      {"Data1_1", this->data1_1},
      {"Data1_2", this->data1_2},
      {"WeaponRangeIndex", this->weapon_range_index},
      {"UnknownA1", this->unknown_a1},
  });
}

ItemParameterTable::SoundRemaps ItemParameterTable::SoundRemaps::from_json(const phosg::JSON& json) {
  ItemParameterTable::SoundRemaps ret;
  ret.sound_id = json.get_int("SoundID");
  for (const auto& sound_id_json : json.get_list("ByRTIndex")) {
    ret.by_rt_index.emplace_back(sound_id_json->as_int());
  }
  for (const auto& sound_id_json : json.get_list("ByCharClass")) {
    ret.by_char_class.emplace_back(sound_id_json->as_int());
  }
  return ret;
}

phosg::JSON ItemParameterTable::SoundRemaps::json() const {
  auto by_rt_index_json = phosg::JSON::list();
  auto by_char_class_json = phosg::JSON::list();
  for (uint32_t sound_id : this->by_rt_index) {
    by_rt_index_json.emplace_back(sound_id);
  }
  for (uint32_t sound_id : this->by_char_class) {
    by_char_class_json.emplace_back(sound_id);
  }
  return phosg::JSON::dict({
      {"SoundID", this->sound_id},
      {"ByRTIndex", std::move(by_rt_index_json)},
      {"ByCharClass", std::move(by_char_class_json)},
  });
}

phosg::JSON ItemParameterTable::json() const {
  auto items_json = phosg::JSON::dict();

  for (size_t data1_1 = 0; data1_1 < this->num_weapon_classes(); data1_1++) {
    size_t class_size = this->num_weapons_in_class(data1_1);
    uint8_t weapon_kind = this->get_weapon_kind(data1_1);
    float sale_divisor = this->get_sale_divisor(0, data1_1);
    for (size_t data1_2 = 0; data1_2 < class_size; data1_2++) {
      auto weapon_dict = this->get_weapon(data1_1, data1_2).json();
      weapon_dict.emplace("WeaponKind", weapon_kind);
      weapon_dict.emplace("SaleDivisor", sale_divisor);
      items_json.emplace(std::format("00{:02X}{:02X}", data1_1, data1_2), std::move(weapon_dict));
    }
  }
  for (size_t data1_1 = 1; data1_1 < 3; data1_1++) {
    size_t class_size = this->num_armors_or_shields_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < class_size; data1_2++) {
      items_json.emplace(std::format("01{:02X}{:02X}", data1_1, data1_2), this->get_armor_or_shield(data1_1, data1_2).json());
    }
  }
  size_t class_size = this->num_units();
  for (size_t data1_2 = 0; data1_2 < class_size; data1_2++) {
    items_json.emplace(std::format("0103{:02X}", data1_2), this->get_unit(data1_2).json());
  }
  class_size = this->num_mags();
  for (size_t data1_1 = 0; data1_1 < class_size; data1_1++) {
    items_json.emplace(std::format("02{:02X}", data1_1), this->get_mag(data1_1).json());
  }
  for (size_t data1_1 = 0; data1_1 < this->num_tool_classes(); data1_1++) {
    size_t class_size = this->num_tools_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < class_size; data1_2++) {
      items_json.emplace(std::format("03{:02X}{:02X}", data1_1, data1_2), this->get_tool(data1_1, data1_2).json());
    }
  }

  auto photon_colors_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_photon_colors(); z++) {
    photon_colors_json.emplace_back(this->get_photon_color(z).json());
  }

  auto weapon_ranges_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_weapon_ranges(); z++) {
    weapon_ranges_json.emplace_back(this->get_weapon_range(z).json());
  }

  float armor_sale_divisor = this->get_sale_divisor(1, 1);
  float shield_sale_divisor = this->get_sale_divisor(1, 2);
  float unit_sale_divisor = this->get_sale_divisor(1, 3);
  float mag_sale_divisor = this->get_sale_divisor(2, 0);

  auto mag_feed_results_json = phosg::JSON::list();
  for (size_t table_index = 0; table_index < 8; table_index++) {
    auto this_mag_feed_result_json = phosg::JSON::list();
    for (size_t item_index = 0; item_index < 11; item_index++) {
      this_mag_feed_result_json.emplace_back(this->get_mag_feed_result(table_index, item_index).json());
    }
    mag_feed_results_json.emplace_back(std::move(this_mag_feed_result_json));
  }

  auto [start_star_value_index, end_star_value_index] = this->get_star_value_index_range();
  auto star_values_json = phosg::JSON::list();
  for (size_t z = start_star_value_index; z < end_star_value_index; z++) {
    star_values_json.emplace_back(this->get_item_stars(z));
  }

  phosg::JSON unknown_a1_json = this->get_unknown_a1();

  auto specials_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_specials(); z++) {
    specials_json.emplace_back(this->get_special(z).json());
  }

  auto weapon_effects_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_weapon_effects(); z++) {
    weapon_effects_json.emplace_back(this->get_weapon_effect(z).json());
  }

  auto weapon_stat_boost_indexes_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_weapon_stat_boost_indexes(); z++) {
    weapon_stat_boost_indexes_json.emplace_back(this->get_weapon_stat_boost_index(z));
  }

  auto armor_stat_boost_indexes_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_armor_stat_boost_indexes(); z++) {
    armor_stat_boost_indexes_json.emplace_back(this->get_armor_stat_boost_index(z));
  }

  auto shield_stat_boost_indexes_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_shield_stat_boost_indexes(); z++) {
    shield_stat_boost_indexes_json.emplace_back(this->get_shield_stat_boost_index(z));
  }

  auto stat_boosts_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_stat_boosts(); z++) {
    stat_boosts_json.emplace_back(this->get_stat_boost(z).json());
  }

  auto shield_effects_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_shield_effects(); z++) {
    shield_effects_json.emplace_back(this->get_shield_effect(z).json());
  }

  auto max_tech_levels_json = phosg::JSON::dict();
  for (size_t tech_num = 0; tech_num < 0x13; tech_num++) {
    auto this_tech_ret = phosg::JSON::dict();
    for (size_t char_class = 0; char_class < 0x0C; char_class++) {
      this_tech_ret.emplace(name_for_char_class(char_class), this->get_max_tech_level(char_class, tech_num));
    }
    max_tech_levels_json.emplace(name_for_technique(tech_num), std::move(this_tech_ret));
  }

  auto combination_table_json = phosg::JSON::list();
  for (const auto& [used_item, combos] : this->all_item_combinations()) {
    for (const auto& combo : combos) {
      combination_table_json.emplace_back(combo.json());
    }
  }

  auto sound_remaps_json = phosg::JSON::list();
  for (const auto& [_, remaps] : get_all_sound_remaps()) {
    sound_remaps_json.emplace_back(remaps.json());
  }

  auto tech_boosts_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_tech_boosts(); z++) {
    tech_boosts_json.emplace_back(this->get_tech_boost(z).json());
  }

  auto unwrap_tables_json = phosg::JSON::list();
  for (size_t event = 0; event < this->num_events(); event++) {
    auto [items, count] = this->get_event_items(event);
    for (size_t z = 0; z < count; z++) {
      unwrap_tables_json.emplace_back(items[z].json());
    }
  }

  auto unsealable_items_json = phosg::JSON::list();
  const auto& unsealable_items = this->all_unsealable_items();
  for (uint32_t item : unsealable_items) {
    unsealable_items_json.emplace_back(item);
  }

  auto ranged_specials_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_ranged_specials(); z++) {
    ranged_specials_json.emplace_back(this->get_ranged_special(z).json());
  }

  return phosg::JSON::dict({
      {"ArmorSaleDivisor", armor_sale_divisor},
      {"ArmorStatBoostIndexes", std::move(armor_stat_boost_indexes_json)},
      {"ItemCombinations", std::move(combination_table_json)},
      {"Items", std::move(items_json)},
      {"MagFeedResults", std::move(mag_feed_results_json)},
      {"MagSaleDivisor", mag_sale_divisor},
      {"MaxTechLevels", std::move(max_tech_levels_json)},
      {"PhotonColors", std::move(photon_colors_json)},
      {"RangedSpecials", std::move(ranged_specials_json)},
      {"ShieldEffects", std::move(shield_effects_json)},
      {"ShieldSaleDivisor", shield_sale_divisor},
      {"ShieldStatBoostIndexes", std::move(shield_stat_boost_indexes_json)},
      {"SoundRemaps", std::move(sound_remaps_json)},
      {"Specials", std::move(specials_json)},
      {"StarValues", std::move(star_values_json)},
      {"StarValueBaseIndex", start_star_value_index},
      {"SpecialStarsBaseIndex", this->get_special_stars_base_index()},
      {"StatBoosts", std::move(stat_boosts_json)},
      {"TechBoosts", std::move(tech_boosts_json)},
      {"UnitSaleDivisor", unit_sale_divisor},
      {"UnknownA1", std::move(unknown_a1_json)},
      {"UnsealableItems", std::move(unsealable_items_json)},
      {"UnwrapTables", std::move(unwrap_tables_json)},
      {"WeaponEffects", std::move(weapon_effects_json)},
      {"WeaponRanges", std::move(weapon_ranges_json)},
      {"WeaponStatBoostIndexes", std::move(weapon_stat_boost_indexes_json)},
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JSON implementation

class JSONItemParameterTable : public ItemParameterTable {
public:
  JSONItemParameterTable() = delete;

  explicit JSONItemParameterTable(const phosg::JSON& json) {
    for (const auto& [key, item_json] : json.get_dict("Items")) {
      auto item_code = phosg::parse_data_string(key);
      if (item_code.size() != 3) {
        throw std::runtime_error("invalid item code in Items dict");
      }
      uint8_t data1_0 = item_code[0];
      uint8_t data1_1 = item_code[1];
      uint8_t data1_2 = item_code[2];
      switch (data1_0) {
        case 0:
          if (this->weapons.size() <= data1_1) {
            this->weapons.resize(data1_1 + 1);
          }
          if (this->weapons[data1_1].size() <= data1_2) {
            this->weapons[data1_1].resize(data1_2 + 1);
          }
          this->weapons[data1_1][data1_2] = Weapon::from_json(*item_json);
          if (this->weapon_kinds.size() <= data1_1) {
            this->weapon_kinds.resize(data1_1 + 1, 0);
          }
          this->weapon_kinds[data1_1] = item_json->get_int("WeaponKind");
          if (this->weapon_sale_divisors.size() <= data1_1) {
            this->weapon_sale_divisors.resize(data1_1 + 1, 0);
          }
          this->weapon_sale_divisors[data1_1] = item_json->get_int("WeaponKind");
          break;
        case 1:
          switch (item_code[1]) {
            case 1:
              if (this->armors.size() <= data1_2) {
                this->armors.resize(data1_2 + 1);
              }
              this->armors[data1_2] = ArmorOrShield::from_json(*item_json);
              break;
            case 2:
              if (this->shields.size() <= data1_2) {
                this->shields.resize(data1_2 + 1);
              }
              this->shields[data1_2] = ArmorOrShield::from_json(*item_json);
              break;
            case 3:
              if (this->units.size() <= data1_2) {
                this->units.resize(data1_2 + 1);
              }
              this->units[data1_2] = Unit::from_json(*item_json);
              break;
            default:
              throw std::runtime_error("invalid defensive item code in Items dict");
          }
          break;
        case 2:
          if (this->mags.size() <= data1_1) {
            this->mags.resize(data1_1 + 1);
          }
          this->mags[data1_1] = Mag::from_json(*item_json);
          break;
        case 3:
          if (this->tools.size() <= data1_1) {
            this->tools.resize(data1_1 + 1);
          }
          if (this->tools[data1_1].size() <= data1_2) {
            this->tools[data1_1].resize(data1_2 + 1);
          }
          this->tools[data1_1][data1_2] = Tool::from_json(*item_json);
          this->tools_by_id.emplace(this->tools[data1_1][data1_2].id, std::make_pair(data1_1, data1_2));
          break;
        default:
          throw std::runtime_error("invalid base item code in Items dict");
      }
    }

    for (const auto& photon_color_json : json.get_list("PhotonColors")) {
      this->photon_colors.emplace_back(PhotonColorEntry::from_json(*photon_color_json));
    }

    for (const auto& weapon_range_json : json.get_list("WeaponRanges")) {
      this->weapon_ranges.emplace_back(WeaponRange::from_json(*weapon_range_json));
    }

    this->armor_sale_divisor = json.get_int("ArmorSaleDivisor");
    this->shield_sale_divisor = json.get_int("ShieldSaleDivisor");
    this->unit_sale_divisor = json.get_int("UnitSaleDivisor");
    this->mag_sale_divisor = json.get_int("MagSaleDivisor");

    const auto& mag_feed_results_json = json.get_list("MagFeedResults");
    for (size_t table_index = 0; table_index < 8; table_index++) {
      const auto& this_mag_feed_result_json = mag_feed_results_json.at(table_index);
      for (size_t item_index = 0; item_index < 11; item_index++) {
        this->mag_feed_results[table_index][item_index] = MagFeedResult::from_json(
            this_mag_feed_result_json->at(item_index));
      }
    }

    this->star_value_start_index = json.get_int("StarValueBaseIndex");
    this->special_stars_base_index = json.get_int("SpecialStarsBaseIndex");
    for (const auto& it : json.get_list("StarValues")) {
      this->star_value_table.emplace_back(it->as_int());
    }

    this->unknown_a1 = json.get_string("UnknownA1");

    for (const auto& it : json.get_list("Specials")) {
      this->specials.emplace_back(Special::from_json(*it));
    }

    for (const auto& it : json.get_list("WeaponEffects")) {
      this->weapon_effects.emplace_back(WeaponEffect::from_json(*it));
    }

    for (const auto& it : json.get_list("WeaponStatBoostIndexes")) {
      this->weapon_stat_boost_indexes.emplace_back(it->as_int());
    }
    for (const auto& it : json.get_list("ArmorStatBoostIndexes")) {
      this->armor_stat_boost_indexes.emplace_back(it->as_int());
    }
    for (const auto& it : json.get_list("ShieldStatBoostIndexes")) {
      this->shield_stat_boost_indexes.emplace_back(it->as_int());
    }

    for (const auto& it : json.get_list("StatBoosts")) {
      this->stat_boosts.emplace_back(StatBoost::from_json(*it));
    }

    for (const auto& it : json.get_list("ShieldEffects")) {
      this->shield_effects.emplace_back(ShieldEffect::from_json(*it));
    }

    const auto& max_tech_levels_json = json.get_dict("MaxTechLevels");
    for (size_t tech_num = 0; tech_num < 0x13; tech_num++) {
      const auto& this_tech_json = max_tech_levels_json.at(name_for_technique(tech_num))->as_dict();
      auto& this_tech_ret = this->max_tech_levels[tech_num];
      for (size_t char_class = 0; char_class < 12; char_class++) {
        this_tech_ret[char_class] = this_tech_json.at(name_for_char_class(char_class))->as_int();
      }
    }

    for (const auto& combo_json : json.get_list("ItemCombinations")) {
      auto combo = ItemCombination::from_json(*combo_json);
      this->item_combinations[item_code_to_u32(combo.used_item)].emplace_back(std::move(combo));
    }

    for (const auto& remaps_json : json.get_list("SoundRemaps")) {
      auto remaps = SoundRemaps::from_json(*remaps_json);
      this->sound_remaps.emplace(remaps.sound_id, std::move(remaps));
    }

    for (const auto& it : json.get_list("TechBoosts")) {
      this->tech_boosts.emplace_back(TechBoost::from_json(*it));
    }

    for (const auto& table_it : json.get_list("UnwrapTables")) {
      auto& this_table = this->unwrap_table.emplace_back();
      for (const auto& item_it : table_it->as_list()) {
        this_table.emplace_back(EventItem::from_json(*item_it));
      }
    }

    for (const auto& it : json.get_list("UnsealableItems")) {
      this->unsealable_items.emplace(it->as_int());
    }

    for (const auto& it : json.get_list("RangedSpecials")) {
      this->ranged_specials.emplace_back(RangedSpecial::from_json(*it));
    }
  }

  virtual ~JSONItemParameterTable() = default;

  virtual size_t num_weapon_classes() const {
    return this->weapons.size();
  }
  virtual size_t num_weapons_in_class(uint8_t data1_1) const {
    return this->weapons.at(data1_1).size();
  }
  virtual const Weapon& get_weapon(uint8_t data1_1, uint8_t data1_2) const {
    return this->weapons.at(data1_1).at(data1_2);
  }

  virtual size_t num_armors_or_shields_in_class(uint8_t data1_1) const {
    if (data1_1 < 1 || data1_1 > 2) {
      throw std::logic_error("invalid armor/shield class");
    }
    return (data1_1 > 1) ? this->armors.size() : this->shields.size();
  }
  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 < 1 || data1_1 > 2) {
      throw std::logic_error("invalid armor/shield class");
    }
    return (data1_1 > 1) ? this->armors.at(data1_2) : this->shields.at(data1_2);
  }

  virtual size_t num_units() const {
    return this->units.size();
  }
  virtual const Unit& get_unit(uint8_t data1_2) const {
    return this->units.at(data1_2);
  }

  virtual size_t num_tool_classes() const {
    return this->tools.size();
  }
  virtual size_t num_tools_in_class(uint8_t data1_1) const {
    return this->tools.at(data1_1).size();
  }
  virtual const Tool& get_tool(uint8_t data1_1, uint8_t data1_2) const {
    return this->tools.at(data1_1).at(data1_2);
  }
  virtual std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const {
    return this->tools_by_id.at(id);
  }

  virtual size_t num_mags() const {
    return this->mags.size();
  }
  virtual const Mag& get_mag(uint8_t data1_1) const {
    return this->mags.at(data1_1);
  }

  virtual uint8_t get_weapon_kind(uint8_t data1_1) const {
    return this->weapon_kinds.at(data1_1);
  }

  virtual size_t num_photon_colors() const {
    return this->photon_colors.size();
  }
  virtual const PhotonColorEntry& get_photon_color(size_t index) const {
    return this->photon_colors.at(index);
  }

  virtual size_t num_weapon_ranges() const {
    return this->weapon_ranges.size();
  }
  virtual const WeaponRange& get_weapon_range(size_t index) const {
    return this->weapon_ranges.at(index);
  }

  virtual float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
    switch (data1_0) {
      case 0:
        return this->weapon_sale_divisors.at(data1_1);
      case 1:
        switch (data1_1) {
          case 1:
            return this->armor_sale_divisor;
          case 2:
            return this->shield_sale_divisor;
          case 3:
            return this->unit_sale_divisor;
        }
        throw runtime_error("invalid defensive item type");
      case 2:
        return this->mag_sale_divisor;
      default:
        throw runtime_error("item type does not have a sale divisor");
    }
  }

  virtual const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t item_index) const {
    return this->mag_feed_results.at(table_index).at(item_index);
  }

  virtual std::pair<uint32_t, uint32_t> get_star_value_index_range() const {
    return std::make_pair(this->star_value_start_index, this->star_value_start_index + this->star_value_table.size());
  }
  virtual uint32_t get_special_stars_base_index() const {
    return this->special_stars_base_index;
  }
  virtual uint8_t get_item_stars(uint32_t id) const {
    return this->star_value_table.at(id - this->star_value_start_index);
  }
  virtual uint8_t get_special_stars(uint8_t special) const {
    return ((special & 0x3F) && !(special & 0x80)) ? this->get_item_stars(special + this->special_stars_base_index) : 0;
  }

  virtual std::string get_unknown_a1() const {
    return this->unknown_a1;
  }

  virtual size_t num_specials() const {
    return this->specials.size();
  }
  virtual const Special& get_special(uint8_t special) const {
    return this->specials.at(special);
  }

  virtual size_t num_weapon_effects() const {
    return this->weapon_effects.size();
  }
  virtual const WeaponEffect& get_weapon_effect(size_t index) const {
    return this->weapon_effects.at(index);
  }

  virtual size_t num_weapon_stat_boost_indexes() const {
    return this->weapon_stat_boost_indexes.size();
  }
  virtual uint8_t get_weapon_stat_boost_index(size_t index) const {
    return this->weapon_stat_boost_indexes.at(index);
  }
  virtual size_t num_armor_stat_boost_indexes() const {
    return this->armor_stat_boost_indexes.size();
  }
  virtual uint8_t get_armor_stat_boost_index(size_t index) const {
    return this->armor_stat_boost_indexes.at(index);
  }
  virtual size_t num_shield_stat_boost_indexes() const {
    return this->shield_stat_boost_indexes.size();
  }
  virtual uint8_t get_shield_stat_boost_index(size_t index) const {
    return this->shield_stat_boost_indexes.at(index);
  }

  virtual size_t num_stat_boosts() const {
    return this->stat_boosts.size();
  }
  virtual const StatBoost& get_stat_boost(size_t index) const {
    return this->stat_boosts.at(index);
  }

  virtual size_t num_shield_effects() const {
    return this->shield_effects.size();
  }
  virtual const ShieldEffect& get_shield_effect(size_t index) const {
    return this->shield_effects.at(index);
  }

  virtual uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
    return this->max_tech_levels.at(tech_num).at(char_class);
  }

  virtual const std::map<uint32_t, std::vector<ItemCombination>>& all_item_combinations() const {
    return this->item_combinations;
  }

  virtual const std::unordered_map<uint32_t, SoundRemaps>& get_all_sound_remaps() const {
    return this->sound_remaps;
  }

  virtual size_t num_tech_boosts() const {
    return this->tech_boosts.size();
  }
  virtual const TechBoost& get_tech_boost(size_t index) const {
    return this->tech_boosts.at(index);
  }

  virtual size_t num_events() const {
    return this->unwrap_table.size();
  }
  virtual std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const {
    const auto& event_table = this->unwrap_table.at(event_number);
    return std::make_pair(event_table.data(), event_table.size());
  }

  virtual const std::unordered_set<uint32_t>& all_unsealable_items() const {
    return this->unsealable_items;
  }

  virtual size_t num_ranged_specials() const {
    return this->ranged_specials.size();
  }
  virtual const RangedSpecial& get_ranged_special(size_t index) const {
    return this->ranged_specials.at(index);
  }

protected:
  std::vector<std::vector<Weapon>> weapons;
  std::vector<ArmorOrShield> armors;
  std::vector<ArmorOrShield> shields;
  std::vector<Unit> units;
  std::vector<std::vector<Tool>> tools;
  std::unordered_map<uint32_t, std::pair<uint8_t, uint8_t>> tools_by_id;
  std::vector<Mag> mags;
  std::vector<uint8_t> weapon_kinds;
  std::vector<PhotonColorEntry> photon_colors;
  std::vector<WeaponRange> weapon_ranges;
  std::vector<float> weapon_sale_divisors;
  float armor_sale_divisor;
  float shield_sale_divisor;
  float unit_sale_divisor;
  float mag_sale_divisor;
  std::array<std::array<MagFeedResult, 11>, 8> mag_feed_results;
  std::vector<uint8_t> star_value_table;
  size_t star_value_start_index;
  size_t special_stars_base_index;
  std::string unknown_a1;
  std::vector<Special> specials;
  std::vector<WeaponEffect> weapon_effects;
  std::vector<uint8_t> weapon_stat_boost_indexes;
  std::vector<uint8_t> armor_stat_boost_indexes;
  std::vector<uint8_t> shield_stat_boost_indexes;
  std::vector<StatBoost> stat_boosts;
  std::vector<ShieldEffect> shield_effects;
  MaxTechniqueLevels max_tech_levels;
  std::map<uint32_t, std::vector<ItemCombination>> item_combinations;
  std::unordered_map<uint32_t, SoundRemaps> sound_remaps;
  std::vector<TechBoost> tech_boosts;
  std::vector<std::vector<EventItem>> unwrap_table;
  std::unordered_set<uint32_t> unsealable_items;
  std::vector<RangedSpecial> ranged_specials;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialized structures

template <bool BE>
struct ItemBaseV2T {
  /* 00 */ U32T<BE> id = 0xFFFFFFFF;
  /* 04 */
  void parse_into(ItemParameterTable::ItemBase& ret) const {
    ret.id = this->id;
  }
} __packed_ws_be__(ItemBaseV2T, 4);

template <bool BE>
struct ItemBaseV3T : ItemBaseV2T<BE> {
  /* 04 */ U16T<BE> type = 0;
  /* 06 */ U16T<BE> skin = 0;
  /* 08 */
  void parse_into(ItemParameterTable::ItemBase& ret) const {
    this->ItemBaseV2T<BE>::parse_into(ret);
    ret.type = this->type;
    ret.skin = this->skin;
  }
} __packed_ws_be__(ItemBaseV3T, 8);

struct ItemBaseV4 : ItemBaseV3T<false> {
  /* 08 */ le_uint32_t team_points = 0;
  /* 0C */
  void parse_into(ItemParameterTable::ItemBase& ret) const {
    this->ItemBaseV3T<false>::parse_into(ret);
    ret.team_points = this->team_points;
  }
} __packed_ws__(ItemBaseV4, 0x0C);

struct WeaponDCProtos {
  /* 00 */ ItemBaseV2T<false> base;
  /* 04 */ le_uint16_t class_flags = 0;
  /* 06 */ le_uint16_t atp_min = 0;
  /* 08 */ le_uint16_t atp_max = 0;
  /* 0A */ le_uint16_t atp_required = 0;
  /* 0C */ le_uint16_t mst_required = 0;
  /* 0E */ le_uint16_t ata_required = 0;
  /* 10 */ uint8_t max_grind = 0;
  /* 11 */ uint8_t photon = 0;
  /* 12 */ uint8_t special = 0;
  /* 13 */ uint8_t ata = 0;
  /* 14 */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret;
    this->base.parse_into(ret);
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
} __packed_ws__(WeaponDCProtos, 0x14);

struct WeaponV1V2 : WeaponDCProtos {
  /* 14 */ uint8_t stat_boost_entry_index = 0; // TODO: This could be larger (16 or 32 bits)
  /* 15 */ parray<uint8_t, 3> unknown_a9;
  /* 18 */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret = this->WeaponDCProtos::operator ItemParameterTable::Weapon();
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    return ret;
  }
} __packed_ws__(WeaponV1V2, 0x18);

template <bool BE>
struct WeaponGCNTET {
  /* 00 */ ItemBaseV3T<BE> base;
  /* 08 */ U16T<BE> class_flags = 0;
  /* 0A */ U16T<BE> atp_min = 0;
  /* 0C */ U16T<BE> atp_max = 0;
  /* 0E */ U16T<BE> atp_required = 0;
  /* 10 */ U16T<BE> mst_required = 0;
  /* 12 */ U16T<BE> ata_required = 0;
  /* 14 */ U16T<BE> mst = 0;
  /* 16 */ uint8_t max_grind = 0;
  /* 17 */ uint8_t photon = 0;
  /* 18 */ uint8_t special = 0;
  /* 19 */ uint8_t ata = 0;
  /* 1A */ uint8_t stat_boost_entry_index = 0;
  /* 1B */ uint8_t projectile = 0;
  /* 1C */ int8_t trail1_x = 0;
  /* 1D */ int8_t trail1_y = 0;
  /* 1E */ int8_t trail2_x = 0;
  /* 1F */ int8_t trail2_y = 0;
  /* 20 */ int8_t color = 0;
  /* 21 */ parray<uint8_t, 3> unknown_a1 = 0;
  /* 24 */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret;
    this->base.parse_into(ret);
    ret.class_flags = this->class_flags;
    ret.atp_min = this->atp_min;
    ret.atp_max = this->atp_max;
    ret.atp_required = this->atp_required;
    ret.mst_required = this->mst_required;
    ret.ata_required = this->ata_required;
    ret.mst = this->mst;
    ret.max_grind = this->max_grind;
    ret.photon = this->photon;
    ret.special = this->special;
    ret.ata = this->ata;
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    ret.projectile = this->projectile;
    ret.trail1_x = this->trail1_x;
    ret.trail1_y = this->trail1_y;
    ret.trail2_x = this->trail2_x;
    ret.trail2_y = this->trail2_y;
    ret.color = this->color;
    ret.unknown_a1 = this->unknown_a1;
    return ret;
  }
} __packed_ws_be__(WeaponGCNTET, 0x24);
using WeaponGCNTE = WeaponGCNTET<true>;

template <bool BE>
struct WeaponV3T : WeaponGCNTET<BE> {
  /* 24 */ uint8_t unknown_a4 = 0;
  /* 25 */ uint8_t unknown_a5 = 0;
  /* 26 */ uint8_t tech_boost_entry_index = 0;
  /* 27 */ uint8_t behavior_flags = 0;
  /* 28 */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret = this->WeaponGCNTET<BE>::operator ItemParameterTable::Weapon();
    ret.unknown_a4 = this->unknown_a4;
    ret.unknown_a5 = this->unknown_a5;
    ret.tech_boost_entry_index = this->tech_boost_entry_index;
    ret.behavior_flags = this->behavior_flags;
    return ret;
  }
} __packed_ws_be__(WeaponV3T, 0x28);
using WeaponGC = WeaponV3T<true>;
using WeaponXB = WeaponV3T<false>;

struct WeaponV4 {
  /* 00 */ ItemBaseV4 base;
  /* 0C */ le_uint16_t class_flags = 0x00FF;
  /* 0E */ le_uint16_t atp_min = 0;
  /* 10 */ le_uint16_t atp_max = 0;
  /* 12 */ le_uint16_t atp_required = 0;
  /* 14 */ le_uint16_t mst_required = 0;
  /* 16 */ le_uint16_t ata_required = 0;
  /* 18 */ le_uint16_t mst = 0;
  /* 1A */ uint8_t max_grind = 0;
  /* 1B */ uint8_t photon = 0;
  /* 1C */ uint8_t special = 0;
  /* 1D */ uint8_t ata = 0;
  /* 1E */ uint8_t stat_boost_entry_index = 0;
  /* 1F */ uint8_t projectile = 0;
  /* 20 */ int8_t trail1_x = 0;
  /* 21 */ int8_t trail1_y = 0;
  /* 22 */ int8_t trail2_x = 0;
  /* 23 */ int8_t trail2_y = 0;
  /* 24 */ int8_t color = 0;
  /* 25 */ parray<uint8_t, 3> unknown_a1 = 0;
  /* 28 */ uint8_t unknown_a4 = 0;
  /* 29 */ uint8_t unknown_a5 = 0;
  /* 2A */ uint8_t tech_boost_entry_index = 0;
  /* 2B */ uint8_t behavior_flags = 0;
  /* 2C */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret;
    this->base.parse_into(ret);
    ret.class_flags = this->class_flags;
    ret.atp_min = this->atp_min;
    ret.atp_max = this->atp_max;
    ret.atp_required = this->atp_required;
    ret.mst_required = this->mst_required;
    ret.ata_required = this->ata_required;
    ret.mst = this->mst;
    ret.max_grind = this->max_grind;
    ret.photon = this->photon;
    ret.special = this->special;
    ret.ata = this->ata;
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    ret.projectile = this->projectile;
    ret.trail1_x = this->trail1_x;
    ret.trail1_y = this->trail1_y;
    ret.trail2_x = this->trail2_x;
    ret.trail2_y = this->trail2_y;
    ret.color = this->color;
    ret.unknown_a1 = this->unknown_a1;
    ret.unknown_a4 = this->unknown_a4;
    ret.unknown_a5 = this->unknown_a5;
    ret.tech_boost_entry_index = this->tech_boost_entry_index;
    ret.behavior_flags = this->behavior_flags;
    return ret;
  }
} __packed_ws__(WeaponV4, 0x2C);

template <typename BaseT, bool BE>
struct ArmorOrShieldT {
  /* V1/V2 offsets */
  /* 00 */ BaseT base;
  /* 04 */ U16T<BE> dfp = 0;
  /* 06 */ U16T<BE> evp = 0;
  /* 08 */ uint8_t block_particle = 0;
  /* 09 */ uint8_t block_effect = 0;
  /* 0A */ U16T<BE> class_flags = 0x00FF;
  /* 0C */ uint8_t required_level = 0;
  /* 0D */ uint8_t efr = 0;
  /* 0E */ uint8_t eth = 0;
  /* 0F */ uint8_t eic = 0;
  /* 10 */ uint8_t edk = 0;
  /* 11 */ uint8_t elt = 0;
  /* 12 */ uint8_t dfp_range = 0;
  /* 13 */ uint8_t evp_range = 0;
  /* 14 */
  operator ItemParameterTable::ArmorOrShield() const {
    ItemParameterTable::ArmorOrShield ret;
    this->base.parse_into(ret);
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
} __attribute__((packed));
static_assert(sizeof(ArmorOrShieldT<ItemBaseV2T<false>, false>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldT<ItemBaseV2T<true>, true>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldT<ItemBaseV3T<false>, false>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldT<ItemBaseV3T<true>, true>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldT<ItemBaseV4, false>) == 0x1C, "Structure size is incorrect");
using ArmorOrShieldDCProtos = ArmorOrShieldT<ItemBaseV2T<false>, false>;

template <typename BaseT, bool BE>
struct ArmorOrShieldFinalT : ArmorOrShieldT<BaseT, BE> {
  /* 14 */ uint8_t stat_boost_entry_index = 0;
  /* 15 */ uint8_t tech_boost_entry_index = 0;
  /* 16 */ uint8_t flags_type = 0;
  /* 17 */ uint8_t unknown_a4 = 0;
  /* 18 */
  operator ItemParameterTable::ArmorOrShield() const {
    ItemParameterTable::ArmorOrShield ret = this->ArmorOrShieldT<BaseT, BE>::operator ItemParameterTable::ArmorOrShield();
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    ret.tech_boost_entry_index = this->tech_boost_entry_index;
    ret.flags_type = this->flags_type;
    ret.unknown_a4 = this->unknown_a4;
    return ret;
  }
} __attribute__((packed));
static_assert(sizeof(ArmorOrShieldFinalT<ItemBaseV2T<false>, false>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldFinalT<ItemBaseV2T<true>, true>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldFinalT<ItemBaseV3T<false>, false>) == 0x1C, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldFinalT<ItemBaseV3T<true>, true>) == 0x1C, "Structure size is incorrect");
static_assert(sizeof(ArmorOrShieldFinalT<ItemBaseV4, false>) == 0x20, "Structure size is incorrect");
using ArmorOrShieldV1V2 = ArmorOrShieldFinalT<ItemBaseV2T<false>, false>;
using ArmorOrShieldGC = ArmorOrShieldFinalT<ItemBaseV3T<true>, true>;
using ArmorOrShieldXB = ArmorOrShieldFinalT<ItemBaseV3T<false>, false>;
using ArmorOrShieldV4 = ArmorOrShieldFinalT<ItemBaseV4, false>;

template <typename BaseT, bool BE>
struct UnitT {
  /* V1/V2 offsets */
  /* 00 */ BaseT base;
  /* 04 */ U16T<BE> stat = 0;
  /* 06 */ U16T<BE> stat_amount = 0;
  /* 08 */
  operator ItemParameterTable::Unit() const {
    ItemParameterTable::Unit ret;
    this->base.parse_into(ret);
    ret.stat = this->stat;
    ret.stat_amount = this->stat_amount;
    return ret;
  }
} __attribute__((packed));
static_assert(sizeof(UnitT<ItemBaseV2T<false>, false>) == 8, "Structure size is incorrect");
static_assert(sizeof(UnitT<ItemBaseV2T<true>, true>) == 8, "Structure size is incorrect");
static_assert(sizeof(UnitT<ItemBaseV3T<false>, false>) == 0x0C, "Structure size is incorrect");
static_assert(sizeof(UnitT<ItemBaseV3T<true>, true>) == 0x0C, "Structure size is incorrect");
static_assert(sizeof(UnitT<ItemBaseV4, false>) == 0x10, "Structure size is incorrect");
using UnitDCProtos = UnitT<ItemBaseV2T<false>, false>;

template <typename BaseT, bool BE>
struct UnitFinalT : UnitT<BaseT, BE> {
  /* 08 */ S16T<BE> modifier_amount = 0;
  /* 0A */ parray<uint8_t, 2> unused;
  /* 0C */
  operator ItemParameterTable::Unit() const {
    ItemParameterTable::Unit ret = this->UnitT<BaseT, BE>::operator ItemParameterTable::Unit();
    ret.modifier_amount = this->modifier_amount;
    return ret;
  }
} __attribute__((packed));
static_assert(sizeof(UnitFinalT<ItemBaseV2T<false>, false>) == 0x0C, "Structure size is incorrect");
static_assert(sizeof(UnitFinalT<ItemBaseV2T<true>, true>) == 0x0C, "Structure size is incorrect");
static_assert(sizeof(UnitFinalT<ItemBaseV3T<false>, false>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(UnitFinalT<ItemBaseV3T<true>, true>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(UnitFinalT<ItemBaseV4, false>) == 0x14, "Structure size is incorrect");
using UnitV1V2 = UnitFinalT<ItemBaseV2T<false>, false>;
using UnitGC = UnitFinalT<ItemBaseV3T<true>, true>;
using UnitXB = UnitFinalT<ItemBaseV3T<false>, false>;
using UnitV4 = UnitFinalT<ItemBaseV4, false>;

template <typename BaseT, bool BE>
struct MagT {
  /* V1/V2 offsets */
  /* 00 */ BaseT base;
  /* 04 */ U16T<BE> feed_table = 0;
  /* 06 */ uint8_t photon_blast = 0;
  /* 07 */ uint8_t activation = 0;
  /* 08 */ uint8_t on_pb_full = 0;
  /* 09 */ uint8_t on_low_hp = 0;
  /* 0A */ uint8_t on_death = 0;
  /* 0B */ uint8_t on_boss = 0;
  /* 0C */ uint8_t on_pb_full_flag = 0;
  /* 0D */ uint8_t on_low_hp_flag = 0;
  /* 0E */ uint8_t on_death_flag = 0;
  /* 0F */ uint8_t on_boss_flag = 0;
  /* 10 */
  operator ItemParameterTable::Mag() const {
    ItemParameterTable::Mag ret;
    this->base.parse_into(ret);
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
} __attribute__((packed));
static_assert(sizeof(MagT<ItemBaseV2T<false>, false>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(MagT<ItemBaseV2T<true>, true>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(MagT<ItemBaseV3T<false>, false>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(MagT<ItemBaseV3T<true>, true>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(MagT<ItemBaseV4, true>) == 0x18, "Structure size is incorrect");
using MagV1 = MagT<ItemBaseV2T<false>, false>;

template <typename BaseT, bool BE>
struct MagV2V3V4T : MagT<BaseT, BE> {
  U16T<BE> class_flags = 0x00FF;
  parray<uint8_t, 2> unused;
  operator ItemParameterTable::Mag() const {
    ItemParameterTable::Mag ret = this->MagT<BaseT, BE>::operator ItemParameterTable::Mag();
    ret.class_flags = this->class_flags;
    return ret;
  }
} __attribute__((packed));
static_assert(sizeof(MagV2V3V4T<ItemBaseV2T<false>, false>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(MagV2V3V4T<ItemBaseV2T<true>, true>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(MagV2V3V4T<ItemBaseV3T<false>, false>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(MagV2V3V4T<ItemBaseV3T<true>, true>) == 0x18, "Structure size is incorrect");
static_assert(sizeof(MagV2V3V4T<ItemBaseV4, false>) == 0x1C, "Structure size is incorrect");
using MagV2 = MagV2V3V4T<ItemBaseV2T<false>, false>;
using MagGC = MagV2V3V4T<ItemBaseV3T<true>, true>;
using MagXB = MagV2V3V4T<ItemBaseV3T<false>, false>;
using MagV4 = MagV2V3V4T<ItemBaseV4, false>;

template <typename BaseT, bool BE>
struct ToolT {
  /* V1/V2 offsets */
  /* 00 */ BaseT base;
  /* 04 */ U16T<BE> amount = 0;
  /* 06 */ U16T<BE> tech = 0;
  /* 08 */ S32T<BE> cost = 0;
  /* 0C */ U32T<BE> item_flags = 0;
  /* 10 */
  operator ItemParameterTable::Tool() const {
    ItemParameterTable::Tool ret;
    this->base.parse_into(ret);
    ret.amount = this->amount;
    ret.tech = this->tech;
    ret.cost = this->cost;
    ret.item_flags = this->item_flags;
    return ret;
  }
} __attribute__((packed));
static_assert(sizeof(ToolT<ItemBaseV2T<false>, false>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(ToolT<ItemBaseV2T<true>, true>) == 0x10, "Structure size is incorrect");
static_assert(sizeof(ToolT<ItemBaseV3T<false>, false>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(ToolT<ItemBaseV3T<true>, true>) == 0x14, "Structure size is incorrect");
static_assert(sizeof(ToolT<ItemBaseV4, false>) == 0x18, "Structure size is incorrect");
using ToolV1V2 = ToolT<ItemBaseV2T<false>, false>;
using ToolGC = ToolT<ItemBaseV3T<true>, true>;
using ToolXB = ToolT<ItemBaseV3T<false>, false>;
using ToolV4 = ToolT<ItemBaseV4, false>;

using MagFeedResultsList = parray<ItemParameterTable::MagFeedResult, 11>;

template <bool BE>
using MagFeedResultsListOffsetsT = parray<U32T<BE>, 8>;

template <bool BE>
struct SpecialT {
  U16T<BE> type = 0xFFFF;
  U16T<BE> amount = 0;
  operator ItemParameterTable::Special() const {
    return {this->type, this->amount};
  }
} __packed_ws_be__(SpecialT, 4);

template <bool BE>
struct StatBoostT {
  parray<uint8_t, 2> stats = 0;
  parray<U16T<BE>, 2> amounts = 0;
  operator ItemParameterTable::StatBoost() const {
    return {this->stats[0], this->amounts[0], this->stats[1], this->amounts[1]};
  }
} __packed_ws_be__(StatBoostT, 6);

template <bool BE>
struct TechBoostEntryT {
  uint8_t tech_num1 = 0;
  uint8_t flags1 = 0;
  parray<uint8_t, 2> unused1;
  F32T<BE> amount1 = 0.0f;
  uint8_t tech_num2 = 0;
  uint8_t flags2 = 0;
  parray<uint8_t, 2> unused2;
  F32T<BE> amount2 = 0.0f;
  uint8_t tech_num3 = 0;
  uint8_t flags3 = 0;
  parray<uint8_t, 2> unused3;
  F32T<BE> amount3 = 0.0f;
  operator ItemParameterTable::TechBoost() const {
    return {
        this->tech_num1,
        this->flags1,
        this->amount1,
        this->tech_num2,
        this->flags2,
        this->amount2,
        this->tech_num3,
        this->flags3,
        this->amount3,
    };
  }
} __packed_ws_be__(TechBoostEntryT, 0x18);

struct NonWeaponSaleDivisorsDCProtos {
  uint8_t armor_divisor = 0;
  uint8_t shield_divisor = 0;
  uint8_t unit_divisor = 0;
  operator ItemParameterTable::NonWeaponSaleDivisors() const {
    return {
        static_cast<float>(this->armor_divisor),
        static_cast<float>(this->shield_divisor),
        static_cast<float>(this->unit_divisor),
        0.0f};
  }
} __packed_ws__(NonWeaponSaleDivisorsDCProtos, 3);

template <bool BE>
struct NonWeaponSaleDivisorsT {
  F32T<BE> armor_divisor = 0.0f;
  F32T<BE> shield_divisor = 0.0f;
  F32T<BE> unit_divisor = 0.0f;
  F32T<BE> mag_divisor = 0.0f;
  operator ItemParameterTable::NonWeaponSaleDivisors() const {
    return {this->armor_divisor, this->shield_divisor, this->unit_divisor, this->mag_divisor};
  }
} __packed_ws_be__(NonWeaponSaleDivisorsT, 0x10);

template <bool BE>
struct ShieldEffectT {
  U32T<BE> sound_id;
  U32T<BE> unknown_a1;
  operator ItemParameterTable::ShieldEffect() const {
    return {this->sound_id, this->unknown_a1};
  }
} __packed_ws_be__(ShieldEffectT, 8);

template <bool BE>
struct PhotonColorEntryT {
  /* 00 */ U32T<BE> unknown_a1;
  /* 04 */ parray<F32T<BE>, 4> unknown_a2;
  /* 14 */ parray<F32T<BE>, 4> unknown_a3;
  /* 24 */
  operator ItemParameterTable::PhotonColorEntry() const {
    ItemParameterTable::PhotonColorEntry ret;
    ret.unknown_a1 = this->unknown_a1;
    ret.unknown_a2.x = this->unknown_a2[0];
    ret.unknown_a2.y = this->unknown_a2[1];
    ret.unknown_a2.z = this->unknown_a2[2];
    ret.unknown_a2.t = this->unknown_a2[3];
    ret.unknown_a3.x = this->unknown_a3[0];
    ret.unknown_a3.y = this->unknown_a3[1];
    ret.unknown_a3.z = this->unknown_a3[2];
    ret.unknown_a3.t = this->unknown_a3[3];
    return ret;
  }
} __packed_ws_be__(PhotonColorEntryT, 0x24);

template <bool BE>
struct UnknownA1T {
  U16T<BE> unknown_a1;
  U16T<BE> unknown_a2;
  operator ItemParameterTable::UnknownA1() const {
    return {this->unknown_a1, this->unknown_a2};
  }
} __packed_ws_be__(UnknownA1T, 4);

template <bool BE>
struct WeaponRangeT {
  F32T<BE> unknown_a1;
  F32T<BE> unknown_a2;
  U32T<BE> unknown_a3;
  U32T<BE> unknown_a4;
  U32T<BE> unknown_a5;
  operator ItemParameterTable::WeaponRange() const {
    return {this->unknown_a1, this->unknown_a2, this->unknown_a3, this->unknown_a4, this->unknown_a5};
  }
} __packed_ws_be__(WeaponRangeT, 0x14);

template <bool BE>
struct WeaponEffectT {
  U32T<BE> sound_id1;
  U32T<BE> eff_value1;
  U32T<BE> sound_id2;
  U32T<BE> eff_value2;
  parray<uint8_t, 0x10> unknown_a5;
  operator ItemParameterTable::WeaponEffect() const {
    return {this->sound_id1, this->eff_value1, this->sound_id2, this->eff_value2, this->unknown_a5};
  }
} __packed_ws_be__(WeaponEffectT, 0x20);

template <bool BE>
struct SoundRemapTableOffsetsT {
  U32T<BE> sound_id;
  U32T<BE> remaps_for_rt_index_table;
  U32T<BE> remaps_for_char_class_table;
} __packed_ws_be__(SoundRemapTableOffsetsT, 0x0C);

/* The fields in the root structures are as follows. See the specializations of BinaryItemParameterTableT for the
 * values of the hardcoded constants in each version (NumWeaponClasses, etc.).
 * DCTE / 112K / V1   / V2   / GCTE / GCV3 / XBV3 / V4
 * 0013 / 0013 / 0013 / 0013 / ---- / ---- / ---- / ----- entry_count // Count of pointers in root struct; unused
 * 2E1C / 2EB8 / 32E8 / 5AFC / 6F0C / F078 / F084 / 14884 weapon_table // -> {count, offset -> WeaponV*[count]}[NumWeaponClasses]
 * 2D94 / 2E28 / 3258 / 5A5C / 6E4C / EF90 / EF9C / 1478C armor_table // -> {count, offset -> ArmorOrShieldV*[count]}][2]
 * 2DA4 / 2E38 / 3268 / 5A6C / 6E5C / EFA0 / EFAC / 1479C unit_table // -> {count, offset -> UnitV*[count]} (last if out of range)
 * 2DB4 / 2E48 / 3278 / 5A7C / 6E6C / EFB0 / EFBC / 147AC tool_table // -> {count, offset -> ToolV*[count]}[NumToolClasses] (last if out of range)
 * 2DAC / 2E40 / 3270 / 5A74 / 6E64 / EFA8 / EFB4 / 147A4 mag_table // -> {count, offset -> MagV*[count]}
 * 1F98 / 202C / 23C8 / 3DF8 / 47BC / B88C / B8A0 / 0F4B8 weapon_kind_table // -> uint8_t[NumWeaponClasses]
 * 1994 / 1A28 / 1DB0 / 2E4C / 37A4 / A7FC / A7FC / 0DE7C photon_color_table // -> PhotonColorEntry[NumPhotonColors]
 * 1C64 / 1CF8 / 2080 / 32CC / 3A74 / AACC / AACC / 0E194 weapon_range_table // -> WeaponRange[...] (indexed by data1_1, but also by RangedSpecial::weapon_range_index + a version-dependent constant, and also by the result of a vfunc call on some TItemWeapons)
 * 1FBF / 2053 / 23F0 / 3E84 / 484C / B938 / B94C / 0F5A8 weapon_sale_divisor_table // -> uint8_t[NumWeaponClasses] on DC protos; float[NumWeaponClasses] on all other versions
 * 1FE6 / 207A / 248C / 40A8 / 4A80 / BBCC / BBE0 / 0F83C non_weapon_sale_divisor_table // -> NonWeaponSaleDivisors
 * 2F54 / 2FF0 / 3420 / 5F4C / 7384 / F608 / F614 / 1502C mag_feed_table // -> MagFeedResultsTable
 * 22A9 / 233D / 275C / 4378 / 4D50 / BE9C / BEB0 / 0FB0C star_value_table // -> uint8_t[...] (indexed by .id from weapon, armor, etc.)
 * 23EE / 2484 / 28A2 / ---- / ---- / ---- / ---- / ----- unknown_a1 // TODO
 * 275E / 27F4 / 2C12 / 4540 / 4F72 / C100 / C114 / 0FE3C special_table // -> Special[NumSpecials]
 * 2804 / 2898 / 2CB8 / 45E4 / 5018 / C1A4 / C1B8 / 0FEE0 weapon_effect_table // -> WeaponEffect[...]
 * 1908 / 199C / ---- / ---- / ---- / ---- / ---- / ----- weapon_stat_boost_index_table // -> uint8_t[max(weapon.id : weapon_table) - (ItemStarsFirstID - 1)]
 * 0668 / 0668 / ---- / ---- / ---- / ---- / ---- / ----- armor_stat_boost_index_table // -> uint8_t[armor_table[0].count]
 * 030C / 030C / ---- / ---- / ---- / ---- / ---- / ----- shield_stat_boost_index_table // -> uint8_t[armor_table[1].count]
 * 2CE4 / 2D78 / 3198 / 58DC / 68B8 / DE50 / DE64 / 1275C stat_boost_table // -> StatBoost[...]
 * ---- / ---- / ---- / 5704 / 61B8 / D6E4 / D6F8 / 11C80 shield_effect_table // -> ShieldEffect[...] (indexed by data1[2])
 * ---- / ---- / ---- / ---- / 69D8 / DF88 / DF9C / 12894 max_tech_level_table // -> MaxTechniqueLevels
 * ---- / ---- / ---- / ---- / 737C / F5D0 / F5DC / 14FF4 combination_table // -> {count, offset -> ItemCombination[count]}
 * ---- / ---- / ---- / ---- / 68B0 / DE48 / DE5C / 12754 sound_remap_table // -> {count, offset -> {sound_id, by_rt_index_offset -> uint32_t[SoundRemapRTTableSize], by_char_class_offset -> uint32_t[12]}}
 * ---- / ---- / ---- / ---- / 6B1C / EB8C / EBA0 / 14278 tech_boost_table // -> TechBoostEntry[...][3]
 * ---- / ---- / ---- / ---- / ---- / F5F0 / F5FC / 15014 unwrap_table // -> {count, offset -> {count, offset -> EventItem[count]}[count]
 * ---- / ---- / ---- / ---- / ---- / F5F8 / F604 / 1501C unsealable_table // -> {count, offset -> UnsealableItem[count]}
 * ---- / ---- / ---- / ---- / ---- / F600 / F60C / 15024 ranged_special_table // -> {count, offset -> RangedSpecial[count]}
 */

struct RootDCProtos {
  /* ## / DCTE / 112K */
  /* 00 / 0013 / 0013 */ le_uint32_t entry_count;
  /* 04 / 2E1C / 2EB8 */ le_uint32_t weapon_table;
  /* 08 / 2D94 / 2E28 */ le_uint32_t armor_table;
  /* 0C / 2DA4 / 2E38 */ le_uint32_t unit_table;
  /* 10 / 2DB4 / 2E48 */ le_uint32_t tool_table;
  /* 14 / 2DAC / 2E40 */ le_uint32_t mag_table;
  /* 18 / 1F98 / 202C */ le_uint32_t weapon_kind_table;
  /* 1C / 1994 / 1A28 */ le_uint32_t photon_color_table;
  /* 20 / 1C64 / 1CF8 */ le_uint32_t weapon_range_table;
  /* 24 / 1FBF / 2053 */ le_uint32_t weapon_integral_sale_divisor_table;
  /* 28 / 1FE6 / 207A */ le_uint32_t non_weapon_integral_sale_divisor_table;
  /* 2C / 2F54 / 2FF0 */ le_uint32_t mag_feed_table;
  /* 30 / 22A9 / 233D */ le_uint32_t star_value_table;
  /* 34 / 23EE / 2484 */ le_uint32_t unknown_a1;
  /* 38 / 275E / 27F4 */ le_uint32_t special_table;
  /* 3C / 2804 / 2898 */ le_uint32_t weapon_effect_table;
  /* 40 / 1908 / 199C */ le_uint32_t weapon_stat_boost_index_table;
  /* 44 / 0668 / 0668 */ le_uint32_t armor_stat_boost_index_table;
  /* 48 / 030C / 030C */ le_uint32_t shield_stat_boost_index_table;
  /* 4C / 2CE4 / 2D78 */ le_uint32_t stat_boost_table;
} __packed_ws__(RootDCProtos, 0x50);

struct RootV1 {
  /* ## / DCV1 */
  /* 00 / 0013 */ le_uint32_t entry_count;
  /* 04 / 32E8 */ le_uint32_t weapon_table;
  /* 08 / 3258 */ le_uint32_t armor_table;
  /* 0C / 3268 */ le_uint32_t unit_table;
  /* 10 / 3278 */ le_uint32_t tool_table;
  /* 14 / 3270 */ le_uint32_t mag_table;
  /* 18 / 23C8 */ le_uint32_t weapon_kind_table;
  /* 1C / 1DB0 */ le_uint32_t photon_color_table;
  /* 20 / 2080 */ le_uint32_t weapon_range_table;
  /* 24 / 23F0 */ le_uint32_t weapon_sale_divisor_table;
  /* 28 / 248C */ le_uint32_t non_weapon_sale_divisor_table;
  /* 2C / 3420 */ le_uint32_t mag_feed_table;
  /* 30 / 275C */ le_uint32_t star_value_table;
  /* 34 / 28A2 */ le_uint32_t unknown_a1;
  /* 38 / 2C12 */ le_uint32_t special_table;
  /* 3C / 2CB8 */ le_uint32_t weapon_effect_table;
  /* 40 / 3198 */ le_uint32_t stat_boost_table;
} __packed_ws__(RootV1, 0x44);

struct RootV2 {
  /* ## / DCV2 */
  /* 00 / 0013 */ le_uint32_t entry_count;
  /* 04 / 5AFC */ le_uint32_t weapon_table;
  /* 08 / 5A5C */ le_uint32_t armor_table;
  /* 0C / 5A6C */ le_uint32_t unit_table;
  /* 10 / 5A7C */ le_uint32_t tool_table;
  /* 14 / 5A74 */ le_uint32_t mag_table;
  /* 18 / 3DF8 */ le_uint32_t weapon_kind_table;
  /* 1C / 2E4C */ le_uint32_t photon_color_table;
  /* 20 / 32CC */ le_uint32_t weapon_range_table;
  /* 24 / 3E84 */ le_uint32_t weapon_sale_divisor_table;
  /* 28 / 40A8 */ le_uint32_t non_weapon_sale_divisor_table;
  /* 2C / 5F4C */ le_uint32_t mag_feed_table;
  /* 30 / 4378 */ le_uint32_t star_value_table;
  /* 34 / 4540 */ le_uint32_t special_table;
  /* 38 / 45E4 */ le_uint32_t weapon_effect_table;
  /* 3C / 58DC */ le_uint32_t stat_boost_table;
  /* 40 / 5704 */ le_uint32_t shield_effect_table;
} __packed_ws__(RootV2, 0x44);

struct RootGCNTE {
  /* ## / GCTE */
  /* 00 / 6F0C */ be_uint32_t weapon_table;
  /* 04 / 6E4C */ be_uint32_t armor_table;
  /* 08 / 6E5C */ be_uint32_t unit_table;
  /* 0C / 6E6C */ be_uint32_t tool_table;
  /* 10 / 6E64 */ be_uint32_t mag_table;
  /* 14 / 47BC */ be_uint32_t weapon_kind_table;
  /* 18 / 37A4 */ be_uint32_t photon_color_table;
  /* 1C / 3A74 */ be_uint32_t weapon_range_table;
  /* 20 / 484C */ be_uint32_t weapon_sale_divisor_table;
  /* 24 / 4A80 */ be_uint32_t non_weapon_sale_divisor_table;
  /* 28 / 7384 */ be_uint32_t mag_feed_table;
  /* 2C / 4D50 */ be_uint32_t star_value_table;
  /* 30 / 4F72 */ be_uint32_t special_table;
  /* 34 / 5018 */ be_uint32_t weapon_effect_table;
  /* 38 / 68B8 */ be_uint32_t stat_boost_table;
  /* 3C / 61B8 */ be_uint32_t shield_effect_table;
  /* 40 / 69D8 */ be_uint32_t max_tech_level_table;
  /* 44 / 737C */ be_uint32_t combination_table;
  /* 48 / 68B0 */ be_uint32_t sound_remap_table;
  /* 4C / 6B1C */ be_uint32_t tech_boost_table;
} __packed_ws__(RootGCNTE, 0x50);

template <bool BE>
struct RootV3V4T {
  /* ## / GCV3 / XBV3 /  BBV4 */
  /* 00 / F078 / F084 / 14884 */ U32T<BE> weapon_table;
  /* 04 / EF90 / EF9C / 1478C */ U32T<BE> armor_table;
  /* 08 / EFA0 / EFAC / 1479C */ U32T<BE> unit_table;
  /* 0C / EFB0 / EFBC / 147AC */ U32T<BE> tool_table;
  /* 10 / EFA8 / EFB4 / 147A4 */ U32T<BE> mag_table;
  /* 14 / B88C / B8A0 / 0F4B8 */ U32T<BE> weapon_kind_table;
  /* 18 / A7FC / A7FC / 0DE7C */ U32T<BE> photon_color_table;
  /* 1C / AACC / AACC / 0E194 */ U32T<BE> weapon_range_table;
  /* 20 / B938 / B94C / 0F5A8 */ U32T<BE> weapon_sale_divisor_table;
  /* 24 / BBCC / BBE0 / 0F83C */ U32T<BE> non_weapon_sale_divisor_table;
  /* 28 / F608 / F614 / 1502C */ U32T<BE> mag_feed_table;
  /* 2C / BE9C / BEB0 / 0FB0C */ U32T<BE> star_value_table;
  /* 30 / C100 / C114 / 0FE3C */ U32T<BE> special_table;
  /* 34 / C1A4 / C1B8 / 0FEE0 */ U32T<BE> weapon_effect_table;
  /* 38 / DE50 / DE64 / 1275C */ U32T<BE> stat_boost_table;
  /* 3C / D6E4 / D6F8 / 11C80 */ U32T<BE> shield_effect_table;
  /* 40 / DF88 / DF9C / 12894 */ U32T<BE> max_tech_level_table;
  /* 44 / F5D0 / F5DC / 14FF4 */ U32T<BE> combination_table;
  /* 48 / DE48 / DE5C / 12754 */ U32T<BE> sound_remap_table;
  /* 4C / EB8C / EBA0 / 14278 */ U32T<BE> tech_boost_table;
  /* 50 / F5F0 / F5FC / 15014 */ U32T<BE> unwrap_table;
  /* 54 / F5F8 / F604 / 1501C */ U32T<BE> unsealable_table;
  /* 58 / F600 / F60C / 15024 */ U32T<BE> ranged_special_table;
} __packed_ws_be__(RootV3V4T, 0x5C);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reader implementation

std::set<uint32_t> ItemParameterTable::compute_all_valid_primary_identifiers() const {
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
      uint64_t effective_data1 = data1 | (static_cast<uint64_t>(x) << 48);
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

std::variant<
    const ItemParameterTable::Weapon*,
    const ItemParameterTable::ArmorOrShield*,
    const ItemParameterTable::Unit*,
    const ItemParameterTable::Mag*,
    const ItemParameterTable::Tool*>
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
      // NOTE: Unlike in ItemData, the tech number comes first in primary identifiers, so we don't need to special-case
      // 0302XXYY here
      return &this->get_tool(data1_1, data1_2);
    default:
      throw runtime_error("invalid primary identifier");
  }
}

uint32_t ItemParameterTable::get_item_id(const ItemData& item) const {
  switch (item.data1[0]) {
    case 0:
      return this->get_weapon(item.data1[1], item.data1[2]).id;
    case 1:
      if (item.data1[1] == 3) {
        return this->get_unit(item.data1[2]).id;
      } else if ((item.data1[1] == 1) || (item.data1[1] == 2)) {
        return this->get_armor_or_shield(item.data1[1], item.data1[2]).id;
      }
      throw runtime_error("invalid item");
    case 2:
      return this->get_mag(item.data1[1]).id;
    case 3:
      if (item.data1[1] == 2) {
        return this->get_tool(2, item.data1[4]).id;
      } else {
        return this->get_tool(item.data1[1], item.data1[2]).id;
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
      return this->get_weapon(item.data1[1], item.data1[2]).team_points;
    case 1:
      if (item.data1[1] == 3) {
        return this->get_unit(item.data1[2]).team_points;
      } else if ((item.data1[1] == 1) || (item.data1[1] == 2)) {
        return this->get_armor_or_shield(item.data1[1], item.data1[2]).team_points;
      }
      throw runtime_error("invalid item");
    case 2:
      return this->get_mag(item.data1[1]).team_points;
    case 3:
      if (item.data1[1] == 2) {
        return this->get_tool(2, item.data1[4]).team_points;
      } else {
        return this->get_tool(item.data1[1], item.data1[2]).team_points;
      }
      throw logic_error("this should be impossible");
    case 4:
      throw runtime_error("item is meseta and therefore has no definition");
    default:
      throw runtime_error("invalid item");
  }
}

uint8_t ItemParameterTable::get_item_base_stars(const ItemData& item) const {
  switch (item.data1[0]) {
    case 0:
    case 1:
      return this->get_item_stars(this->get_item_id(item));
    case 2:
      return (item.data1[1] >= 0x28) ? 12 : 0;
    case 3: {
      const auto& def = (item.data1[1] == 2)
          ? this->get_tool(2, item.data1[4])
          : this->get_tool(item.data1[1], item.data1[2]);
      return (def.item_flags & 0x80) ? 12 : 0;
    }
    default:
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

std::string ItemParameterTable::get_star_value_table() const {
  auto [start_z, end_z] = this->get_star_value_index_range();
  std::string ret;
  for (size_t z = start_z; z < end_z; z++) {
    ret.push_back(this->get_item_stars(z));
  }
  return ret;
}

std::string ItemParameterTable::get_weapon_stat_boost_index_table() const {
  std::string ret;
  for (size_t z = 0; z < this->num_weapon_stat_boost_indexes(); z++) {
    ret.push_back(this->get_weapon_stat_boost_index(z));
  }
  return ret;
}

std::string ItemParameterTable::get_armor_stat_boost_index_table() const {
  std::string ret;
  for (size_t z = 0; z < this->num_armor_stat_boost_indexes(); z++) {
    ret.push_back(this->get_armor_stat_boost_index(z));
  }
  return ret;
}

std::string ItemParameterTable::get_shield_stat_boost_index_table() const {
  std::string ret;
  for (size_t z = 0; z < this->num_shield_stat_boost_indexes(); z++) {
    ret.push_back(this->get_shield_stat_boost_index(z));
  }
  return ret;
}

bool ItemParameterTable::is_item_rare(const ItemData& item) const {
  try {
    return (this->get_item_base_stars(item) >= 9);
  } catch (const out_of_range&) {
    return false;
  }
}

bool ItemParameterTable::is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const {
  return this->all_unsealable_items().count(item_code_to_u32(data1_0, data1_1, data1_2));
}

bool ItemParameterTable::is_unsealable_item(const ItemData& item) const {
  return this->is_unsealable_item(item.data1[0], item.data1[1], item.data1[2]);
}

const ItemParameterTable::ItemCombination& ItemParameterTable::get_item_combination(
    const ItemData& used_item, const ItemData& equipped_item) const {
  for (const auto& def : this->all_combinations_for_used_item(used_item)) {
    if ((def.equipped_item[0] == 0xFF || def.equipped_item[0] == equipped_item.data1[0]) &&
        (def.equipped_item[1] == 0xFF || def.equipped_item[1] == equipped_item.data1[1]) &&
        (def.equipped_item[2] == 0xFF || def.equipped_item[2] == equipped_item.data1[2])) {
      return def;
    }
  }
  throw out_of_range("no item combination applies");
}

const std::vector<ItemParameterTable::ItemCombination>& ItemParameterTable::all_combinations_for_used_item(
    const ItemData& used_item) const {
  try {
    return this->all_item_combinations().at(item_code_to_u32(
        used_item.data1[0], used_item.data1[1], used_item.data1[2]));
  } catch (const out_of_range&) {
    static const vector<ItemCombination> ret;
    return ret;
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

      float sale_divisor = this->get_sale_divisor(0, item.data1[1]);
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
        return this->get_item_adjusted_stars(item) * this->get_sale_divisor(1, 3);
      }

      double sale_divisor = (double)this->get_sale_divisor(1, item.data1[1]);
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

template <
    typename RootT,
    typename WeaponT,
    size_t NumWeaponClasses,
    typename ArmorOrShieldT,
    typename UnitT,
    typename ToolT,
    size_t NumToolClasses,
    typename MagT,
    size_t ItemStarsFirstID,
    size_t ItemStarsLastID,
    size_t SpecialStarsBeginIndex,
    size_t NumSpecials,
    size_t NumPhotonColors,
    size_t SoundRemapRTTableSize,
    bool BE>
class BinaryItemParameterTableT : public ItemParameterTable {
public:
  explicit BinaryItemParameterTableT(std::shared_ptr<const std::string> data)
      : ItemParameterTable(),
        data(data),
        r(*this->data),
        root(&this->r.pget<RootT>(BE ? r.pget_u32b(r.size() - 0x10) : r.pget_u32l(r.size() - 0x10))) {}
  ~BinaryItemParameterTableT() = default;

  inline size_t indirect_lookup_2d_count(size_t base_offset, size_t co_index) const {
    return this->r.pget<ArrayRefT<BE>>(base_offset + sizeof(ArrayRefT<BE>) * co_index).count;
  }

  template <typename T>
  const T& indirect_lookup_2d(size_t base_offset, size_t co_index, size_t item_index) const {
    const auto& co = this->r.pget<ArrayRefT<BE>>(base_offset + sizeof(ArrayRefT<BE>) * co_index);
    if (item_index >= co.count) {
      throw out_of_range("2-D array index out of range");
    }
    return this->r.pget<T>(co.offset + sizeof(T) * item_index);
  }

  template <typename RawT, typename ParsedT>
  const ParsedT& add_to_vector_cache(std::vector<ParsedT>& cache, size_t base_offset, size_t index) const {
    while (cache.size() <= index) {
      cache.emplace_back(this->r.pget<RawT>(base_offset + sizeof(RawT) * cache.size()));
    }
    return cache[index];
  }

  template <typename RawT, typename ParsedT>
  const ParsedT& add_to_vector_cache_2d_indirect(
      std::vector<ParsedT>& cache, size_t base_offset, size_t major_index, size_t minor_index) const {
    while (cache.size() <= minor_index) {
      cache.emplace_back(indirect_lookup_2d<RawT>(base_offset, major_index, cache.size()));
    }
    return cache[minor_index];
  }

  virtual size_t num_weapon_classes() const {
    return NumWeaponClasses;
  }

  virtual size_t num_weapons_in_class(uint8_t data1_1) const {
    if (data1_1 >= NumWeaponClasses) {
      throw out_of_range("weapon ID out of range");
    }
    return this->indirect_lookup_2d_count(this->root->weapon_table, data1_1);
  }

  virtual const Weapon& get_weapon(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 >= NumWeaponClasses) {
      throw out_of_range("weapon ID out of range");
    }
    uint16_t key = (data1_1 << 8) | data1_2;
    auto it = this->weapons.find(key);
    if (it == this->weapons.end()) {
      const auto& weapon = this->indirect_lookup_2d<WeaponT>(this->root->weapon_table, data1_1, data1_2);
      it = this->weapons.emplace(key, weapon).first;
    }
    return it->second;
  }

  virtual size_t num_armors_or_shields_in_class(uint8_t data1_1) const {
    if ((data1_1 < 1) || (data1_1 > 2)) {
      throw out_of_range("armor/shield class ID out of range");
    }
    return this->indirect_lookup_2d_count(this->root->armor_table, data1_1 - 1);
  }

  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
    if ((data1_1 < 1) || (data1_1 > 2)) {
      throw out_of_range("armor/shield class ID out of range");
    }
    auto& vec = (data1_1 == 1) ? this->armors : this->shields;
    return this->add_to_vector_cache_2d_indirect<ArmorOrShieldT>(vec, this->root->armor_table, data1_1 - 1, data1_2);
  }

  virtual size_t num_units() const {
    return this->indirect_lookup_2d_count(this->root->unit_table, 0);
  }

  virtual const Unit& get_unit(uint8_t data1_2) const {
    return this->add_to_vector_cache_2d_indirect<UnitT>(this->units, this->root->unit_table, 0, data1_2);
  }

  virtual size_t num_tool_classes() const {
    return NumToolClasses;
  }

  virtual size_t num_tools_in_class(uint8_t data1_1) const {
    if (data1_1 >= NumToolClasses) {
      throw out_of_range("tool class ID out of range");
    }
    return this->indirect_lookup_2d_count(this->root->tool_table, data1_1);
  }

  virtual const Tool& get_tool(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 >= NumToolClasses) {
      throw out_of_range("tool class ID out of range");
    }
    uint16_t key = (data1_1 << 8) | data1_2;
    auto it = this->tools.find(key);
    if (it == this->tools.end()) {
      const auto& tool = this->indirect_lookup_2d<ToolT>(this->root->tool_table, data1_1, data1_2);
      it = this->tools.emplace(key, tool).first;
    }
    return it->second;
  }

  virtual std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const {
    const auto* cos = &this->r.pget<ArrayRefT<BE>>(this->root->tool_table, NumToolClasses * sizeof(ArrayRefT<BE>));
    for (size_t z = 0; z < NumToolClasses; z++) {
      const auto& co = cos[z];
      const auto* defs = &this->r.pget<ToolT>(co.offset, sizeof(ToolT) * co.count);
      for (size_t y = 0; y < co.count; y++) {
        if (defs[y].base.id == id) {
          return make_pair(z, y);
        }
      }
    }
    throw out_of_range(std::format("invalid tool class {:08X}", id));
  }

  virtual size_t num_mags() const {
    return this->indirect_lookup_2d_count(this->root->mag_table, 0);
  }

  virtual const Mag& get_mag(uint8_t data1_1) const {
    return this->add_to_vector_cache_2d_indirect<MagT>(this->mags, this->root->mag_table, 0, data1_1);
  }

  virtual uint8_t get_weapon_kind(uint8_t data1_1) const {
    return (data1_1 < NumWeaponClasses) ? this->r.pget_u8(this->root->weapon_kind_table + data1_1) : 0x00;
  }

  virtual size_t num_photon_colors() const {
    return NumPhotonColors;
  }

  virtual const PhotonColorEntry& get_photon_color(size_t index) const {
    if (index >= NumPhotonColors) {
      throw std::out_of_range("invalid photon color index");
    }
    return this->add_to_vector_cache<PhotonColorEntryT<BE>>(this->photon_colors, this->root->photon_color_table, index);
  }

  virtual size_t num_weapon_ranges() const {
    return this->get_data_array_count<WeaponRangeT<BE>>(this->root->weapon_range_table);
  }

  virtual const WeaponRange& get_weapon_range(size_t index) const {
    return this->add_to_vector_cache<WeaponRangeT<BE>>(this->weapon_ranges, this->root->weapon_range_table, index);
  }

  virtual float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
    if (data1_0 == 0) {
      if (data1_1 >= NumWeaponClasses) {
        return 0.0f;
      }
      if constexpr (requires { this->root->weapon_sale_divisor_table; }) {
        return this->r.pget<F32T<BE>>(this->root->weapon_sale_divisor_table + data1_1 * sizeof(F32T<BE>));
      } else {
        return this->r.pget<uint8_t>(this->root->weapon_integral_sale_divisor_table + data1_1 * sizeof(uint8_t));
      }
    }

    if constexpr (requires { this->root->non_weapon_sale_divisor_table; }) {
      const auto& divisors = this->r.pget<NonWeaponSaleDivisorsT<BE>>(this->root->non_weapon_sale_divisor_table);
      if (data1_0 == 1) {
        switch (data1_1) {
          case 1:
            return divisors.armor_divisor;
          case 2:
            return divisors.shield_divisor;
          case 3:
            return divisors.unit_divisor;
        }
      } else if (data1_0 == 2) {
        return divisors.mag_divisor;
      }
    } else {
      if (data1_0 == 1) {
        const auto& divisors = this->r.pget<NonWeaponSaleDivisorsDCProtos>(
            this->root->non_weapon_integral_sale_divisor_table);
        switch (data1_1) {
          case 1:
            return divisors.armor_divisor;
          case 2:
            return divisors.shield_divisor;
          case 3:
            return divisors.unit_divisor;
        }
      }
    }

    return 0.0f;
  }

  virtual const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t item_index) const {
    if (table_index >= 8) {
      throw out_of_range("invalid mag feed table index");
    }
    if (item_index >= 11) {
      throw out_of_range("invalid mag feed item index");
    }
    const auto& table_offsets = this->r.pget<MagFeedResultsListOffsetsT<BE>>(this->root->mag_feed_table);
    return this->r.pget<MagFeedResultsList>(table_offsets[table_index])[item_index];
  }

  virtual std::pair<uint32_t, uint32_t> get_star_value_index_range() const {
    return std::make_pair(ItemStarsFirstID, ItemStarsLastID);
  }

  virtual uint32_t get_special_stars_base_index() const {
    return SpecialStarsBeginIndex;
  }

  virtual uint8_t get_item_stars(uint32_t id) const {
    return ((id >= ItemStarsFirstID) && (id < ItemStarsLastID))
        ? this->r.pget_u8(this->root->star_value_table + id - ItemStarsFirstID)
        : 0;
  }

  virtual uint8_t get_special_stars(uint8_t special) const {
    return ((special & 0x3F) && !(special & 0x80)) ? this->get_item_stars(special + SpecialStarsBeginIndex) : 0;
  }

  virtual std::string get_unknown_a1() const {
    if constexpr (requires { this->root->unknown_a1; }) {
      return this->r.pread(this->root->unknown_a1, this->get_data_range_size(this->root->unknown_a1));
    } else {
      return "";
    }
  }

  virtual size_t num_specials() const {
    return NumSpecials;
  }

  virtual const Special& get_special(uint8_t special) const {
    special &= 0x3F;
    if (special >= NumSpecials) {
      throw out_of_range("invalid special index");
    }
    while (this->specials.size() <= special) {
      this->specials.emplace_back(this->r.pget<SpecialT<BE>>(
          this->root->special_table + sizeof(SpecialT<BE>) * this->specials.size()));
    }
    return this->specials[special];
  }

  virtual size_t num_weapon_effects() const {
    return this->get_data_array_count<WeaponEffectT<BE>>(this->root->weapon_effect_table);
  }

  virtual const WeaponEffect& get_weapon_effect(size_t index) const {
    return this->add_to_vector_cache<WeaponEffectT<BE>>(this->weapon_effects, this->root->weapon_effect_table, index);
  }

  virtual size_t num_weapon_stat_boost_indexes() const {
    if constexpr (requires { this->root->weapon_stat_boost_index_table; }) {
      return this->get_data_range_size(this->root->weapon_stat_boost_index_table);
    } else {
      return 0;
    }
  }

  virtual uint8_t get_weapon_stat_boost_index(size_t index) const {
    if constexpr (requires { this->root->weapon_stat_boost_index_table; }) {
      return this->r.pget_u8(this->root->weapon_stat_boost_index_table + index);
    } else {
      throw std::logic_error("weapon stat boost index table not available");
    }
  }

  virtual size_t num_armor_stat_boost_indexes() const {
    if constexpr (requires { this->root->armor_stat_boost_index_table; }) {
      return this->get_data_range_size(this->root->armor_stat_boost_index_table);
    } else {
      return 0;
    }
  }

  virtual uint8_t get_armor_stat_boost_index(size_t index) const {
    if constexpr (requires { this->root->armor_stat_boost_index_table; }) {
      return this->r.pget_u8(this->root->armor_stat_boost_index_table + index);
    } else {
      throw std::logic_error("armor stat boost index table not available");
    }
  }

  virtual size_t num_shield_stat_boost_indexes() const {
    if constexpr (requires { this->root->shield_stat_boost_index_table; }) {
      return this->get_data_range_size(this->root->shield_stat_boost_index_table);
    } else {
      return 0;
    }
  }

  virtual uint8_t get_shield_stat_boost_index(size_t index) const {
    if constexpr (requires { this->root->shield_stat_boost_index_table; }) {
      return this->r.pget_u8(this->root->shield_stat_boost_index_table + index);
    } else {
      throw std::logic_error("shield stat boost index table not available");
    }
  }

  virtual size_t num_stat_boosts() const {
    return this->get_data_array_count<StatBoostT<BE>>(this->root->stat_boost_table);
  }

  virtual const StatBoost& get_stat_boost(size_t index) const {
    return this->add_to_vector_cache<StatBoostT<BE>>(this->stat_boosts, this->root->stat_boost_table, index);
  }

  virtual size_t num_shield_effects() const {
    if constexpr (requires { this->root->shield_effect_table; }) {
      return this->get_data_array_count<ShieldEffectT<BE>>(this->root->shield_effect_table);
    } else {
      return 0;
    }
  }

  virtual const ShieldEffect& get_shield_effect(size_t index) const {
    if constexpr (requires { this->root->shield_effect_table; }) {
      return this->add_to_vector_cache<ShieldEffectT<BE>>(this->shield_effects, this->root->shield_effect_table, index);
    } else {
      throw std::logic_error("shield effect table not available");
    }
  }

  virtual uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const {
    if (char_class >= 12) {
      throw out_of_range("invalid character class");
    }
    if (tech_num >= 19) {
      throw out_of_range("invalid technique number");
    }
    if constexpr (requires { this->root->max_tech_level_table; }) {
      return r.pget_u8(this->root->max_tech_level_table + tech_num * 12 + char_class);
    } else {
      if ((tech_num == 14) || (tech_num == 17)) { // Ryuker or Reverser
        return 0;
      } else {
        return ((char_class == 6) || (char_class == 7) || (char_class == 8) || (char_class == 10)) ? 29 : 14;
      }
    }
  }

  virtual const std::map<uint32_t, std::vector<ItemCombination>>& all_item_combinations() const {
    if constexpr (requires { this->root->combination_table; }) {
      if (!this->item_combination_index.has_value()) {
        auto& ret = this->item_combination_index.emplace();
        const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->combination_table);
        const auto* defs = &this->r.pget<ItemCombination>(co.offset, co.count * sizeof(ItemCombination));
        for (size_t z = 0; z < co.count; z++) {
          ret[item_code_to_u32(defs[z].used_item)].emplace_back(defs[z]);
        }
      }
      return *this->item_combination_index;
    } else {
      static const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>> empty_map{};
      return empty_map;
    }
  }

  virtual const std::unordered_map<uint32_t, SoundRemaps>& get_all_sound_remaps() const {
    if constexpr (requires { this->root->sound_remap_table; }) {
      if (!this->sound_remaps.has_value()) {
        auto& ret = this->sound_remaps.emplace();
        const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->sound_remap_table);
        const auto* entries = this->r.pget_array<SoundRemapTableOffsetsT<BE>>(co.offset, co.count);
        for (size_t z = 0; z < co.count; z++) {
          auto& remaps = ret.emplace(entries[z].sound_id, SoundRemaps{}).first->second;
          remaps.sound_id = entries[z].sound_id;
          auto sub_r = r.sub(entries[z].remaps_for_rt_index_table, SoundRemapRTTableSize * sizeof(U32T<BE>));
          for (size_t z = 0; z < SoundRemapRTTableSize; z++) {
            remaps.by_rt_index.emplace_back(sub_r.template get<U32T<BE>>());
          }
          sub_r = r.sub(entries[z].remaps_for_char_class_table, 12 * sizeof(U32T<BE>));
          for (size_t z = 0; z < 12; z++) {
            remaps.by_char_class.emplace_back(sub_r.template get<U32T<BE>>());
          }
        }
      }
    }
    return *this->sound_remaps;
  }

  virtual size_t num_tech_boosts() const {
    if constexpr (requires { this->root->tech_boost_table; }) {
      return this->get_data_array_count<TechBoostEntryT<BE>>(this->root->tech_boost_table);
    } else {
      return 0;
    }
  }

  virtual const TechBoost& get_tech_boost(size_t index) const {
    if constexpr (requires { this->root->tech_boost_table; }) {
      return this->add_to_vector_cache<TechBoostEntryT<BE>>(this->tech_boosts, this->root->tech_boost_table, index);
    } else {
      throw std::logic_error("tech boost table not available");
    }
  }

  virtual size_t num_events() const {
    if constexpr (requires { this->root->unwrap_table; }) {
      return this->r.pget<ArrayRefT<BE>>(this->root->unwrap_table).count;
    } else {
      return 0;
    }
  }

  virtual std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const {
    if constexpr (requires { this->root->unwrap_table; }) {
      const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->unwrap_table);
      if (event_number >= co.count) {
        throw out_of_range("invalid event number");
      }
      const auto& event_co = this->r.pget<ArrayRefT<BE>>(co.offset + sizeof(ArrayRefT<BE>) * event_number);
      const auto* defs = &this->r.pget<EventItem>(event_co.offset, event_co.count * sizeof(EventItem));
      return make_pair(defs, event_co.count);
    } else {
      return make_pair(nullptr, 0);
    }
  }

  virtual const std::unordered_set<uint32_t>& all_unsealable_items() const {
    if constexpr (requires { this->root->unsealable_table; }) {
      if (!this->unsealable_table.has_value()) {
        auto& ret = this->unsealable_table.emplace();
        const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->unsealable_table);
        const auto* defs = &this->r.pget<UnsealableItem>(co.offset, co.count * sizeof(UnsealableItem));
        for (size_t z = 0; z < co.count; z++) {
          ret.emplace(item_code_to_u32(defs[z].item));
        }
      }
      return *this->unsealable_table;
    } else {
      static const std::unordered_set<uint32_t> empty_set{};
      return empty_set;
    }
  }

  virtual size_t num_ranged_specials() const {
    if constexpr (requires { this->root->ranged_special_table; }) {
      return this->indirect_lookup_2d_count(this->root->ranged_special_table, 0);
    } else {
      return 0;
    }
  }

  virtual const RangedSpecial& get_ranged_special(size_t index) const {
    if constexpr (requires { this->root->ranged_special_table; }) {
      return this->indirect_lookup_2d<RangedSpecial>(this->root->ranged_special_table, 0, index);
    } else {
      throw std::logic_error("ranged special table not available");
    }
  }

  const std::set<uint32_t>& all_start_offsets() const {
    if (!this->start_offsets.has_value()) {
      auto& ret = this->start_offsets.emplace();
      ret.emplace(r.size() - 0x20); // REL footer
      ret.emplace(BE ? r.pget_u32b(r.size() - 0x10) : r.pget_u32l(r.size() - 0x10)); // root
      ret.emplace(BE ? r.pget_u32b(r.size() - 0x20) : r.pget_u32l(r.size() - 0x20)); // relocations

      const auto& footer = r.pget<RELFileFooterT<BE>>(r.size() - sizeof(RELFileFooterT<BE>));
      auto sub_r = r.sub(footer.relocations_offset, footer.num_relocations * sizeof(U16T<BE>));
      uint32_t offset = 0;
      while (!sub_r.eof()) {
        offset += sub_r.template get<U16T<BE>>() * 4;
        ret.emplace(r.pget<U32T<BE>>(offset));
      }
    }
    return *this->start_offsets;
  }

  size_t get_data_range_size(size_t start_offset) const {
    const auto& offsets = this->all_start_offsets();
    auto it = offsets.lower_bound(start_offset);
    if (it == offsets.end()) {
      throw std::out_of_range("start offset out of range");
    }
    if (*it == start_offset) {
      it++;
    }
    if (it == offsets.end()) {
      throw std::out_of_range("no further offset beyond start offset");
    }
    return *it - start_offset;
  }

  template <typename T>
  size_t get_data_array_count(size_t start_offset) const {
    return this->get_data_range_size(start_offset) / sizeof(T);
  }

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const RootT* root;

  mutable std::optional<std::set<uint32_t>> start_offsets;

  mutable std::unordered_map<uint16_t, Weapon> weapons;
  mutable std::vector<ArmorOrShield> armors;
  mutable std::vector<ArmorOrShield> shields;
  mutable std::vector<Unit> units;
  mutable std::vector<Mag> mags;
  mutable std::unordered_map<uint16_t, Tool> tools;

  mutable std::vector<Special> specials;
  mutable std::vector<StatBoost> stat_boosts;
  mutable std::vector<PhotonColorEntry> photon_colors;
  mutable std::vector<WeaponRange> weapon_ranges;
  mutable std::vector<WeaponEffect> weapon_effects;
  mutable std::vector<ShieldEffect> shield_effects;
  mutable std::optional<std::unordered_map<uint32_t, SoundRemaps>> sound_remaps;
  mutable std::vector<TechBoost> tech_boosts;

  // Key is used_item. We can't index on (used_item, equipped_item) because equipped_item may contain wildcards, and
  // the matching order matters.
  mutable std::optional<std::map<uint32_t, std::vector<ItemCombination>>> item_combination_index;

  mutable std::optional<std::unordered_set<uint32_t>> unsealable_table;
};

using ItemParameterTableDCNTE = BinaryItemParameterTableT<
    RootDCProtos, // typename RootT
    WeaponDCProtos, // typename WeaponT
    0x27, // size_t NumWeaponClasses
    ArmorOrShieldDCProtos, // typename ArmorOrShieldT
    UnitDCProtos, // typename UnitT
    ToolV1V2, // typename ToolT
    0x0D, // size_t NumToolClasses
    MagV1, // typename MagT
    0x22, // size_t ItemStarsFirstID
    0x168, // size_t ItemStarsLastID
    0xAA, // size_t SpecialStarsBeginIndex
    0x28, // size_t NumSpecials
    0x14, // size_t NumPhotonColors
    0x00, // size_t SoundRemapRTTableSize
    false>; // bool BE
using ItemParameterTableDC112000 = BinaryItemParameterTableT<
    RootDCProtos, // typename RootT
    WeaponDCProtos, // typename WeaponT
    0x27, // size_t NumWeaponClasses
    ArmorOrShieldDCProtos, // typename ArmorOrShieldT
    UnitDCProtos, // typename UnitT
    ToolV1V2, // typename ToolT
    0x0E, // size_t NumToolClasses
    MagV1, // typename MagT
    0x26, // size_t ItemStarsFirstID
    0x16C, // size_t ItemStarsLastID
    0xAE, // size_t SpecialStarsBeginIndex
    0x28, // size_t NumSpecials
    0x14, // size_t NumPhotonColors
    0x00, // size_t SoundRemapRTTableSize
    false>; // bool BE
using ItemParameterTableV1 = BinaryItemParameterTableT<
    RootV1, // typename RootT
    WeaponV1V2, // typename WeaponT
    0x27, // size_t NumWeaponClasses
    ArmorOrShieldV1V2, // typename ArmorOrShieldT
    UnitV1V2, // typename UnitT
    ToolV1V2, // typename ToolT
    0x0E, // size_t NumToolClasses
    MagV1, // typename MagT
    0x26, // size_t ItemStarsFirstID
    0x16C, // size_t ItemStarsLastID
    0xAE, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x14, // size_t NumPhotonColors
    0x00, // size_t SoundRemapRTTableSize
    false>; // bool BE
using ItemParameterTableV2 = BinaryItemParameterTableT<
    RootV2, // typename RootT
    WeaponV1V2, // typename WeaponT
    0x89, // size_t NumWeaponClasses
    ArmorOrShieldV1V2, // typename ArmorOrShieldT
    UnitV1V2, // typename UnitT
    ToolV1V2, // typename ToolT
    0x10, // size_t NumToolClasses
    MagV2, // typename MagT
    0x4E, // size_t ItemStarsFirstID
    0x215, // size_t ItemStarsLastID
    0x138, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x20, // size_t NumPhotonColors
    0x00, // size_t SoundRemapRTTableSize
    false>; // bool BE
using ItemParameterTableGCNTE = BinaryItemParameterTableT<
    RootGCNTE, // typename RootT
    WeaponGCNTE, // typename WeaponT
    0x8D, // size_t NumWeaponClasses
    ArmorOrShieldGC, // typename ArmorOrShieldT
    UnitGC, // typename UnitT
    ToolGC, // typename ToolT
    0x13, // size_t NumToolClasses
    MagGC, // typename MagT
    0x76, // size_t ItemStarsFirstID
    0x298, // size_t ItemStarsLastID
    0x1A3, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x20, // size_t NumPhotonColors
    0x4F, // size_t SoundRemapRTTableSize
    true>; // bool BE
using ItemParameterTableGC = BinaryItemParameterTableT<
    RootV3V4T<true>, // typename RootT
    WeaponGC, // typename WeaponT
    0xAA, // size_t NumWeaponClasses
    ArmorOrShieldGC, // typename ArmorOrShieldT
    UnitGC, // typename UnitT
    ToolGC, // typename ToolT
    0x18, // size_t NumToolClasses
    MagGC, // typename MagT
    0x94, // size_t ItemStarsFirstID
    0x2F7, // size_t ItemStarsLastID
    0x1CB, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x20, // size_t NumPhotonColors
    0x58, // size_t SoundRemapRTTableSize
    true>; // bool BE
using ItemParameterTableXB = BinaryItemParameterTableT<
    RootV3V4T<false>, // typename RootT
    WeaponXB, // typename WeaponT
    0xAA, // size_t NumWeaponClasses
    ArmorOrShieldXB, // typename ArmorOrShieldT
    UnitXB, // typename UnitT
    ToolXB, // typename ToolT
    0x18, // size_t NumToolClasses
    MagXB, // typename MagT
    0x94, // size_t ItemStarsFirstID
    0x2F7, // size_t ItemStarsLastID
    0x1CB, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x20, // size_t NumPhotonColors
    0x58, // size_t SoundRemapRTTableSize
    false>; // bool BE
using ItemParameterTableV4 = BinaryItemParameterTableT<
    RootV3V4T<false>, // typename RootT
    WeaponV4, // typename WeaponT
    0xED, // size_t NumWeaponClasses
    ArmorOrShieldV4, // typename ArmorOrShieldT
    UnitV4, // typename UnitT
    ToolV4, // typename ToolT
    0x1B, // size_t NumToolClasses
    MagV4, // typename MagT
    0xB1, // size_t ItemStarsFirstID
    0x437, // size_t ItemStarsLastID
    0x256, // size_t SpecialStarsBeginIndex
    0x29, // size_t NumSpecials
    0x20, // size_t NumPhotonColors
    0x6A, // size_t SoundRemapRTTableSize
    false>; // bool BE

std::shared_ptr<ItemParameterTable> ItemParameterTable::from_binary(
    std::shared_ptr<const std::string> data, Version version) {
  switch (version) {
    case Version::DC_NTE:
      return std::make_shared<ItemParameterTableDCNTE>(data);
    case Version::DC_11_2000:
      return std::make_shared<ItemParameterTableDC112000>(data);
    case Version::DC_V1:
      return std::make_shared<ItemParameterTableV1>(data);
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return std::make_shared<ItemParameterTableV2>(data);
    case Version::GC_NTE:
      return std::make_shared<ItemParameterTableGCNTE>(data);
    case Version::GC_V3:
    case Version::GC_EP3:
    case Version::GC_EP3_NTE:
      return std::make_shared<ItemParameterTableGC>(data);
    case Version::XB_V3:
      return std::make_shared<ItemParameterTableXB>(data);
    case Version::BB_V4:
      return std::make_shared<ItemParameterTableV4>(data);
    default:
      throw std::logic_error("Cannot create item parameter table for this version");
  }
}

std::shared_ptr<ItemParameterTable> ItemParameterTable::from_json(const phosg::JSON& json) {
  return std::make_shared<JSONItemParameterTable>(json);
}
