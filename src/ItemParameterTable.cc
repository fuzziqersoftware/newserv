#include "ItemParameterTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

/* General notes on the ItemPMT formats:
 *
 * Sega apparently serialized the fields in this order, so we do the same in BinaryItemParameterTableT::serialize.
 * There's likely no reason for this order, though it appears they made an effort to place fields that don't
 * contain pointers before fields that do.
 * DCTE 112K   V1   V2 GCTE GCV3 XBV3    V4 R
 * 0000 0000 0000 0008 0040 0040 0040 00040   armor_table[0][1]
 * 030C 030C ---- ---- ---- ---- ---- ----- * shield_stat_boost_index_table // -> uint8_t[armor_table[1].count]
 * 0334 0334 03A8 0590 0874 0EE8 0EE8 01500   armor_table[0][0]
 * 0668 0668 ---- ---- ---- ---- ---- ----- * armor_stat_boost_index_table // -> uint8_t[armor_table[0].count]
 * 0694 0694 0780 0AA0 0E5C 14D0 14D0 02020   unit_table[0]
 * 08B4 08B4 0AB0 0DDC 12DC 1960 1960 02804   mag_table[0]
 * 0B74 0B74 0D70 1264 16B4 1FA8 1FA8 03118   tool_table[0][0] (the rest follow immediately)
 * 0E54 0F24 1120 1874 1B3C 2C8C 2C8C 04348   weapon_table[0][0] (the rest follow immediately)
 * 1908 199C ---- ---- ---- ---- ---- ----- * weapon_stat_boost_index_table // -> uint8_t[max(weapon.id : weapon_table) - (ItemStarsFirstID - 1)]
 * 1994 1A28 1DB0 2E4C 37A4 A7FC A7FC 0DE7C * photon_color_table // -> PhotonColorEntry[...]
 * 1C64 1CF8 2080 32CC 3A74 AACC AACC 0E194 * weapon_range_table // -> WeaponRange[...] (indexed by data1_1, but also by RangedSpecial::weapon_range_index + a version-dependent constant, and also by the result of a vfunc call on some TItemWeapons)
 * 1F98 202C 23C8 3DF8 47BC B88C B8A0 0F4B8 * weapon_kind_table // -> uint8_t[...]
 * 1FBF 2053 23F0 3E84 484C B938 B94C 0F5A8 * weapon_sale_divisor_table // -> uint8_t[...] on DC protos; float[...] on all other versions
 * 1FE6 207A 248C 40A8 4A80 BBCC BBE0 0F83C * non_weapon_sale_divisor_table // -> NonWeaponSaleDivisors
 * 1FE9 20D5 249C 40B8 4A90 BBDC BBF0 0F84C   mag_feed_table (data)
 * 22A9 233D 275C 4378 4D50 BE9C BEB0 0FB0C * star_value_table // -> uint8_t[...] (indexed by .id from weapon, armor, etc.)
 * 23EE 2484 28A2 ---- ---- ---- ---- ----- * unknown_a1 // TODO
 * 275E 27F4 2C12 4540 4F72 C100 C114 0FE3C * special_table // -> Special[...]
 * 2804 2898 2CB8 45E4 5018 C1A4 C1B8 0FEE0 * weapon_effect_table // -> WeaponEffect[...]
 * ---- ---- ---- 5704 61B8 D6E4 D6F8 11C80 * shield_effect_table // -> ShieldEffect[...] (indexed by data1[2])
 * ---- ---- ---- ---- 68B0 DE48 DE5C 12754 * sound_remap_table // -> {count, offset -> {sound_id, by_rt_index_offset -> uint32_t[SoundRemapRTTableSize], by_char_class_offset -> uint32_t[12]}} // Leaves first, then array refs going up the tree; forward order on the leaves (as if it matters); everything just before this offset
 * 2CE4 2D78 3198 58DC 68B8 DE50 DE64 1275C * stat_boost_table // -> StatBoost[...]
 * ---- ---- ---- ---- 69D8 DF88 DF9C 12894 * max_tech_level_table // -> MaxTechniqueLevels
 * ---- ---- ---- ---- 6ABC E06C E080 12978   combination_table[0]
 * ---- ---- ---- ---- 6B1C EB8C EBA0 14278 * tech_boost_table // -> TechBoostEntry[...]
 * ---- ---- ---- ---- ---- EEBC EED0 14698   unwrap_table[0][0] (the rest follow immediately)
 * ---- ---- ---- ---- ---- EF24 EF38 14700   unsealable_table[0]
 * ---- ---- ---- ---- ---- EF28 EF3C 14710   ranged_special_table[0]
 * 2D94 2E28 3258 5A5C 6E4C EF90 EF9C 1478C * armor_table // -> {count, offset -> ArmorOrShieldV*[count]}][2]
 * 2DA4 2E38 3268 5A6C 6E5C EFA0 EFAC 1479C * unit_table // -> {count, offset -> UnitV*[count]} (last if out of range)
 * 2DAC 2E40 3270 5A74 6E64 EFA8 EFB4 147A4 * mag_table // -> {count, offset -> MagV*[count]}
 * 2DB4 2E48 3278 5A7C 6E6C EFB0 EFBC 147AC * tool_table // -> {count, offset -> ToolV*[count]}[...] (last if out of range)
 * 2E1C 2EB8 32E8 5AFC 6F0C F078 F084 14884 * weapon_table // -> {count, offset -> WeaponV*[count]}[...]
 * ---- ---- ---- ---- 737C F5D0 F5DC 14FF4 * combination_table // -> {count, offset -> ItemCombination[count]}
 * ---- ---- ---- ---- ---- F5D8 F5E4 14FFC   unwrap_table[0] (the rest follow immediately)
 * ---- ---- ---- ---- ---- F5F0 F5FC 15014 * unwrap_table // -> {count, offset -> {count, offset -> EventItem[count]}[count]
 * ---- ---- ---- ---- ---- F5F8 F604 1501C * unsealable_table // -> {count, offset -> UnsealableItem[count]}
 * ---- ---- ---- ---- ---- F600 F60C 15024 * ranged_special_table // -> {count, offset -> RangedSpecial[count]}
 * 2F54 2FF0 3420 5F4C 7384 F608 F614 1502C * mag_feed_table (offsets) // -> MagFeedResultsTable
 *
 * Some hardcoded constants are different across versions. Our parser/serializer doesn't use some of them, but
 * these constants are (all values in hex):
 *                              DCTE / 112K /   V1 /   V2 / GCTE / GCV3 / XBV3 /   V4
 *   Weapon class count:          27 /   27 /   27 /   89 /   8D /   AA /   AA /   ED
 *   Tool class count:            0D /   0E /   0E /   10 /   13 /   18 /   18 /   1B
 *   Item stars first ID:         22 /   26 /   26 /   4E /   76 /   94 /   94 /   B1
 *   Item stars last ID:         168 /  16C /  16C /  215 /  298 /  2F7 /  2F7 /  437
 *   Special stars start index:   AA /   AE /   AE /  138 /  1A3 /  1CB /  1CB /  256
 *   Special count:               28 /   28 /   29 /   29 /   29 /   29 /   29 /   29
 *   Photon color count:          14 /   14 /   14 /   20 /   20 /   20 /   20 /   20
 *   Sound RT table size:         00 /   00 /   00 /   00 /   4F /   58 /   58 /   6A
 *
 * Oddities discovered in the various formats:
 * - DC NTE through DC V1:
 *   - The mag count is wrong; there are 0x2C mags defined in the file but the ArrayRef has a count of only 0x28.
 *   - A few of the tool table entries are encoded out of order. This isn't a bug, just a curiosity.
 *   - A few of the weapon table entries are encoded out of order as well.
 *   - One of the weapon classes has an incorrect count, so it also includes the first weapon of the next class.
 * - V2:
 *   - All of the quirks from V1, except the mag count is now correct.
 *   - The placeholder entries were added in most of the item tables (weapon, armor, shield, unit and mag, but not
 *     tool). The counts (major count, for weapons) are off by one for this reason, hence HasImplicitPlaceholders.
 * - V3 and later: All of the oddities from V2 and earlier are gone.
 */

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
  uint32_t unknown_a1 = json.get_int("UnknownA1");
  ret.unknown_a1[0] = unknown_a1 >> 16;
  ret.unknown_a1[1] = unknown_a1 >> 8;
  ret.unknown_a1[2] = unknown_a1;
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
  ret.emplace("UnknownA1", (this->unknown_a1[0] << 16) | (this->unknown_a1[1] << 8) | this->unknown_a1[2]);
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
  ret.amount1 = json.get_float("Amount1");
  ret.tech_num2 = json.get_int("TechNum2");
  ret.flags2 = json.get_int("Flags2");
  ret.amount2 = json.get_float("Amount2");
  ret.tech_num3 = json.get_int("TechNum3");
  ret.flags3 = json.get_int("Flags3");
  ret.amount3 = json.get_float("Amount3");
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
  ret.unknown_a2.x = unknown_a2.at(0)->as_float();
  ret.unknown_a2.y = unknown_a2.at(1)->as_float();
  ret.unknown_a2.z = unknown_a2.at(2)->as_float();
  ret.unknown_a2.t = unknown_a2.at(3)->as_float();
  ret.unknown_a3.x = unknown_a3.at(0)->as_float();
  ret.unknown_a3.y = unknown_a3.at(1)->as_float();
  ret.unknown_a3.z = unknown_a3.at(2)->as_float();
  ret.unknown_a3.t = unknown_a3.at(3)->as_float();
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
  ret.unknown_a1 = json.get_float("UnknownA1");
  ret.unknown_a2 = json.get_float("UnknownA2");
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
    std::optional<float> sale_divisor;
    if (data1_1 < this->num_weapon_sale_divisors()) {
      sale_divisor = this->get_sale_divisor(0, data1_1);
    }
    for (size_t data1_2 = 0; data1_2 < class_size; data1_2++) {
      auto weapon_dict = this->get_weapon(data1_1, data1_2).json();
      weapon_dict.emplace("WeaponKind", weapon_kind);
      if (sale_divisor.has_value()) {
        weapon_dict.emplace("SaleDivisor", *sale_divisor);
      } else {
        weapon_dict.emplace("SaleDivisor", phosg::JSON(nullptr));
      }
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
  for (size_t z = 0; z < this->num_item_combinations(); z++) {
    combination_table_json.emplace_back(this->get_item_combination(z).json());
  }

  auto sound_remaps_json = phosg::JSON::list();
  for (const auto& remap : get_all_sound_remaps()) {
    sound_remaps_json.emplace_back(remap.json());
  }

  auto tech_boosts_json = phosg::JSON::list();
  for (size_t z = 0; z < this->num_tech_boosts(); z++) {
    tech_boosts_json.emplace_back(this->get_tech_boost(z).json());
  }

  auto unwrap_tables_json = phosg::JSON::list();
  for (size_t event = 0; event < this->num_events(); event++) {
    auto [items, count] = this->get_event_items(event);
    auto this_unwrap_table_json = phosg::JSON::list();
    for (size_t z = 0; z < count; z++) {
      this_unwrap_table_json.emplace_back(items[z].json());
    }
    unwrap_tables_json.emplace_back(std::move(this_unwrap_table_json));
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
      if (item_code.size() < 2 || item_code.size() > 3) {
        throw std::runtime_error("invalid item code in Items dict");
      }
      uint8_t data1_0 = item_code[0];
      uint8_t data1_1 = item_code[1];
      uint8_t data1_2 = (data1_0 != 2) ? item_code.at(2) : 0;
      switch (data1_0) {
        case 0: {
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
          auto sale_divisor_json = item_json->at("SaleDivisor");
          if (!sale_divisor_json.is_null()) {
            if (this->weapon_sale_divisors.size() <= data1_1) {
              this->weapon_sale_divisors.resize(data1_1 + 1, 0);
            }
            this->weapon_sale_divisors[data1_1] = sale_divisor_json.as_float();
          }
          break;
        }
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

    this->armor_sale_divisor = json.get_float("ArmorSaleDivisor");
    this->shield_sale_divisor = json.get_float("ShieldSaleDivisor");
    this->unit_sale_divisor = json.get_float("UnitSaleDivisor");
    this->mag_sale_divisor = json.get_float("MagSaleDivisor");

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
      this->item_combinations.emplace_back(ItemCombination::from_json(*combo_json));
    }

    for (const auto& remaps_json : json.get_list("SoundRemaps")) {
      this->sound_remaps.emplace_back(SoundRemaps::from_json(*remaps_json));
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
    return (data1_1 > 1) ? this->shields.size() : this->armors.size();
  }
  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 < 1 || data1_1 > 2) {
      throw std::logic_error("invalid armor/shield class");
    }
    return (data1_1 > 1) ? this->shields.at(data1_2) : this->armors.at(data1_2);
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

  virtual size_t num_weapon_kinds() const {
    return this->weapon_kinds.size();
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

  virtual size_t num_weapon_sale_divisors() const {
    return this->weapon_sale_divisors.size();
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

  virtual size_t num_item_combinations() const {
    return this->item_combinations.size();
  }

  virtual const ItemCombination& get_item_combination(size_t index) const {
    return this->item_combinations.at(index);
  }

  virtual const std::vector<SoundRemaps>& get_all_sound_remaps() const {
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
  std::vector<ItemCombination> item_combinations;
  std::vector<SoundRemaps> sound_remaps;
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
  ItemBaseV2T() = default;
  ItemBaseV2T(const ItemParameterTable::ItemBase& b) : id(b.id) {}
  void parse_into(ItemParameterTable::ItemBase& ret) const {
    ret.id = this->id;
  }
} __packed_ws_be__(ItemBaseV2T, 4);

template <bool BE>
struct ItemBaseV3T : ItemBaseV2T<BE> {
  /* 04 */ U16T<BE> type = 0;
  /* 06 */ U16T<BE> skin = 0;
  /* 08 */
  ItemBaseV3T() = default;
  ItemBaseV3T(const ItemParameterTable::ItemBase& b) : ItemBaseV2T<BE>(b), type(b.type), skin(b.skin) {}
  void parse_into(ItemParameterTable::ItemBase& ret) const {
    this->ItemBaseV2T<BE>::parse_into(ret);
    ret.type = this->type;
    ret.skin = this->skin;
  }
} __packed_ws_be__(ItemBaseV3T, 8);

struct ItemBaseV4 : ItemBaseV3T<false> {
  /* 08 */ le_uint32_t team_points = 0;
  /* 0C */
  ItemBaseV4() = default;
  ItemBaseV4(const ItemParameterTable::ItemBase& b) : ItemBaseV3T<false>(b), team_points(b.team_points) {}
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
  WeaponDCProtos() = default;
  WeaponDCProtos(const ItemParameterTable::Weapon& w)
      : base(w),
        class_flags(w.class_flags),
        atp_min(w.atp_min),
        atp_max(w.atp_max),
        atp_required(w.atp_required),
        mst_required(w.mst_required),
        ata_required(w.ata_required),
        max_grind(w.max_grind),
        photon(w.photon),
        special(w.special),
        ata(w.ata) {}
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
  WeaponV1V2() = default;
  WeaponV1V2(const ItemParameterTable::Weapon& w)
      : WeaponDCProtos(w), stat_boost_entry_index(w.stat_boost_entry_index), unknown_a9(w.v2_unknown_a9) {}
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret = this->WeaponDCProtos::operator ItemParameterTable::Weapon();
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    ret.v2_unknown_a9 = this->unknown_a9;
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
  WeaponGCNTET() = default;
  WeaponGCNTET(const ItemParameterTable::Weapon& w)
      : base(w),
        class_flags(w.class_flags),
        atp_min(w.atp_min),
        atp_max(w.atp_max),
        atp_required(w.atp_required),
        mst_required(w.mst_required),
        ata_required(w.ata_required),
        mst(w.mst),
        max_grind(w.max_grind),
        photon(w.photon),
        special(w.special),
        ata(w.ata),
        stat_boost_entry_index(w.stat_boost_entry_index),
        projectile(w.projectile),
        trail1_x(w.trail1_x),
        trail1_y(w.trail1_y),
        trail2_x(w.trail2_x),
        trail2_y(w.trail2_y),
        color(w.color),
        unknown_a1(w.unknown_a1) {}
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
  WeaponV3T() = default;
  WeaponV3T(const ItemParameterTable::Weapon& w)
      : WeaponGCNTET<BE>(w),
        unknown_a4(w.unknown_a4),
        unknown_a5(w.unknown_a5),
        tech_boost_entry_index(w.tech_boost_entry_index),
        behavior_flags(w.behavior_flags) {}
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
  WeaponV4() = default;
  WeaponV4(const ItemParameterTable::Weapon& w)
      : base(w),
        class_flags(w.class_flags),
        atp_min(w.atp_min),
        atp_max(w.atp_max),
        atp_required(w.atp_required),
        mst_required(w.mst_required),
        ata_required(w.ata_required),
        mst(w.mst),
        max_grind(w.max_grind),
        photon(w.photon),
        special(w.special),
        ata(w.ata),
        stat_boost_entry_index(w.stat_boost_entry_index),
        projectile(w.projectile),
        trail1_x(w.trail1_x),
        trail1_y(w.trail1_y),
        trail2_x(w.trail2_x),
        trail2_y(w.trail2_y),
        color(w.color),
        unknown_a1(w.unknown_a1),
        unknown_a4(w.unknown_a4),
        unknown_a5(w.unknown_a5),
        tech_boost_entry_index(w.tech_boost_entry_index),
        behavior_flags(w.behavior_flags) {}
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
  ArmorOrShieldT() = default;
  ArmorOrShieldT(const ItemParameterTable::ArmorOrShield& as)
      : base(as),
        dfp(as.dfp),
        evp(as.evp),
        block_particle(as.block_particle),
        block_effect(as.block_effect),
        class_flags(as.class_flags),
        required_level(as.required_level),
        efr(as.efr),
        eth(as.eth),
        eic(as.eic),
        edk(as.edk),
        elt(as.elt),
        dfp_range(as.dfp_range),
        evp_range(as.evp_range) {}
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
  ArmorOrShieldFinalT() = default;
  ArmorOrShieldFinalT(const ItemParameterTable::ArmorOrShield& as)
      : ArmorOrShieldT<BaseT, BE>(as),
        stat_boost_entry_index(as.stat_boost_entry_index),
        tech_boost_entry_index(as.tech_boost_entry_index),
        flags_type(as.flags_type),
        unknown_a4(as.unknown_a4) {}
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
  UnitT() = default;
  UnitT(const ItemParameterTable::Unit& u) : base(u), stat(u.stat), stat_amount(u.stat_amount) {}
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
  /* 0A */ parray<uint8_t, 2> unused = 0;
  /* 0C */
  UnitFinalT() = default;
  UnitFinalT(const ItemParameterTable::Unit& u) : UnitT<BaseT, BE>(u), modifier_amount(u.modifier_amount) {}
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
  MagT() = default;
  MagT(const ItemParameterTable::Mag& m)
      : base(m),
        feed_table(m.feed_table),
        photon_blast(m.photon_blast),
        activation(m.activation),
        on_pb_full(m.on_pb_full),
        on_low_hp(m.on_low_hp),
        on_death(m.on_death),
        on_boss(m.on_boss),
        on_pb_full_flag(m.on_pb_full_flag),
        on_low_hp_flag(m.on_low_hp_flag),
        on_death_flag(m.on_death_flag),
        on_boss_flag(m.on_boss_flag) {}
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
  parray<uint8_t, 2> unused = 0;
  MagV2V3V4T() = default;
  MagV2V3V4T(const ItemParameterTable::Mag& m) : MagT<BaseT, BE>(m), class_flags(m.class_flags) {}
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
  ToolT() = default;
  ToolT(const ItemParameterTable::Tool& t)
      : base(t), amount(t.amount), tech(t.tech), cost(t.cost), item_flags(t.item_flags) {}
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
  SpecialT() = default;
  SpecialT(const ItemParameterTable::Special& t) : type(t.type), amount(t.amount) {}
  operator ItemParameterTable::Special() const {
    return {this->type, this->amount};
  }
} __packed_ws_be__(SpecialT, 4);

template <bool BE>
struct StatBoostT {
  uint8_t stat1 = 0;
  uint8_t stat2 = 0;
  U16T<BE> amount1 = 0;
  U16T<BE> amount2 = 0;
  StatBoostT() = default;
  StatBoostT(const ItemParameterTable::StatBoost& sb)
      : stat1(sb.stat1), stat2(sb.stat2), amount1(sb.amount1), amount2(sb.amount2) {}
  operator ItemParameterTable::StatBoost() const {
    return {this->stat1, this->amount1, this->stat2, this->amount2};
  }
} __packed_ws_be__(StatBoostT, 6);

template <bool BE>
struct TechBoostT {
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
  TechBoostT() = default;
  TechBoostT(const ItemParameterTable::TechBoost& tb)
      : tech_num1(tb.tech_num1),
        flags1(tb.flags1),
        amount1(tb.amount1),
        tech_num2(tb.tech_num2),
        flags2(tb.flags2),
        amount2(tb.amount2),
        tech_num3(tb.tech_num3),
        flags3(tb.flags3),
        amount3(tb.amount3) {}
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
} __packed_ws_be__(TechBoostT, 0x18);

struct NonWeaponSaleDivisorsDCProtos {
  uint8_t armor_divisor = 0;
  uint8_t shield_divisor = 0;
  uint8_t unit_divisor = 0;
  NonWeaponSaleDivisorsDCProtos() = default;
  NonWeaponSaleDivisorsDCProtos(const ItemParameterTable::NonWeaponSaleDivisors& sd)
      : armor_divisor(sd.armor_divisor), shield_divisor(sd.shield_divisor), unit_divisor(sd.unit_divisor) {}
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
  NonWeaponSaleDivisorsT() = default;
  NonWeaponSaleDivisorsT(const ItemParameterTable::NonWeaponSaleDivisors& sd)
      : armor_divisor(sd.armor_divisor),
        shield_divisor(sd.shield_divisor),
        unit_divisor(sd.unit_divisor),
        mag_divisor(sd.mag_divisor) {}
  operator ItemParameterTable::NonWeaponSaleDivisors() const {
    return {this->armor_divisor, this->shield_divisor, this->unit_divisor, this->mag_divisor};
  }
} __packed_ws_be__(NonWeaponSaleDivisorsT, 0x10);

template <bool BE>
struct ShieldEffectT {
  U32T<BE> sound_id;
  U32T<BE> unknown_a1;
  ShieldEffectT() = default;
  ShieldEffectT(const ItemParameterTable::ShieldEffect& se) : sound_id(se.sound_id), unknown_a1(se.unknown_a1) {}
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
  PhotonColorEntryT() = default;
  PhotonColorEntryT(const ItemParameterTable::PhotonColorEntry pc) {
    this->unknown_a1 = pc.unknown_a1;
    this->unknown_a2[0] = pc.unknown_a2.x;
    this->unknown_a2[1] = pc.unknown_a2.y;
    this->unknown_a2[2] = pc.unknown_a2.z;
    this->unknown_a2[3] = pc.unknown_a2.t;
    this->unknown_a3[0] = pc.unknown_a3.x;
    this->unknown_a3[1] = pc.unknown_a3.y;
    this->unknown_a3[2] = pc.unknown_a3.z;
    this->unknown_a3[3] = pc.unknown_a3.t;
  }
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
  UnknownA1T() = default;
  UnknownA1T(const ItemParameterTable::UnknownA1 a1) : unknown_a1(a1.unknown_a1), unknown_a2(a1.unknown_a2) {}
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
  WeaponRangeT() = default;
  WeaponRangeT(const ItemParameterTable::WeaponRange& wr)
      : unknown_a1(wr.unknown_a1),
        unknown_a2(wr.unknown_a2),
        unknown_a3(wr.unknown_a3),
        unknown_a4(wr.unknown_a4),
        unknown_a5(wr.unknown_a5) {}
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
  WeaponEffectT() = default;
  WeaponEffectT(const ItemParameterTable::WeaponEffect& we)
      : sound_id1(we.sound_id1),
        eff_value1(we.eff_value1),
        sound_id2(we.sound_id2),
        eff_value2(we.eff_value2),
        unknown_a5(we.unknown_a5) {}
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

struct RootDCProtos {
  /* ## / DCTE / 112K */
  /* 00 / 0013 / 0013 */ le_uint32_t entry_count = 0x13;
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
  /* 00 / 0013 */ le_uint32_t entry_count = 0x13;
  /* 04 / 32E8 */ le_uint32_t weapon_table = 0xFFFFFFFF;
  /* 08 / 3258 */ le_uint32_t armor_table = 0xFFFFFFFF;
  /* 0C / 3268 */ le_uint32_t unit_table = 0xFFFFFFFF;
  /* 10 / 3278 */ le_uint32_t tool_table = 0xFFFFFFFF;
  /* 14 / 3270 */ le_uint32_t mag_table = 0xFFFFFFFF;
  /* 18 / 23C8 */ le_uint32_t weapon_kind_table = 0xFFFFFFFF;
  /* 1C / 1DB0 */ le_uint32_t photon_color_table = 0xFFFFFFFF;
  /* 20 / 2080 */ le_uint32_t weapon_range_table = 0xFFFFFFFF;
  /* 24 / 23F0 */ le_uint32_t weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 28 / 248C */ le_uint32_t non_weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 2C / 3420 */ le_uint32_t mag_feed_table = 0xFFFFFFFF;
  /* 30 / 275C */ le_uint32_t star_value_table = 0xFFFFFFFF;
  /* 34 / 28A2 */ le_uint32_t unknown_a1 = 0xFFFFFFFF;
  /* 38 / 2C12 */ le_uint32_t special_table = 0xFFFFFFFF;
  /* 3C / 2CB8 */ le_uint32_t weapon_effect_table = 0xFFFFFFFF;
  /* 40 / 3198 */ le_uint32_t stat_boost_table = 0xFFFFFFFF;
} __packed_ws__(RootV1, 0x44);

struct RootV2 {
  /* ## / DCV2 */
  /* 00 / 0013 */ le_uint32_t entry_count = 0x13;
  /* 04 / 5AFC */ le_uint32_t weapon_table = 0xFFFFFFFF;
  /* 08 / 5A5C */ le_uint32_t armor_table = 0xFFFFFFFF;
  /* 0C / 5A6C */ le_uint32_t unit_table = 0xFFFFFFFF;
  /* 10 / 5A7C */ le_uint32_t tool_table = 0xFFFFFFFF;
  /* 14 / 5A74 */ le_uint32_t mag_table = 0xFFFFFFFF;
  /* 18 / 3DF8 */ le_uint32_t weapon_kind_table = 0xFFFFFFFF;
  /* 1C / 2E4C */ le_uint32_t photon_color_table = 0xFFFFFFFF;
  /* 20 / 32CC */ le_uint32_t weapon_range_table = 0xFFFFFFFF;
  /* 24 / 3E84 */ le_uint32_t weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 28 / 40A8 */ le_uint32_t non_weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 2C / 5F4C */ le_uint32_t mag_feed_table = 0xFFFFFFFF;
  /* 30 / 4378 */ le_uint32_t star_value_table = 0xFFFFFFFF;
  /* 34 / 4540 */ le_uint32_t special_table = 0xFFFFFFFF;
  /* 38 / 45E4 */ le_uint32_t weapon_effect_table = 0xFFFFFFFF;
  /* 3C / 58DC */ le_uint32_t stat_boost_table = 0xFFFFFFFF;
  /* 40 / 5704 */ le_uint32_t shield_effect_table = 0xFFFFFFFF;
} __packed_ws__(RootV2, 0x44);

struct RootGCNTE {
  /* ## / GCTE */
  /* 00 / 6F0C */ be_uint32_t weapon_table = 0xFFFFFFFF;
  /* 04 / 6E4C */ be_uint32_t armor_table = 0xFFFFFFFF;
  /* 08 / 6E5C */ be_uint32_t unit_table = 0xFFFFFFFF;
  /* 0C / 6E6C */ be_uint32_t tool_table = 0xFFFFFFFF;
  /* 10 / 6E64 */ be_uint32_t mag_table = 0xFFFFFFFF;
  /* 14 / 47BC */ be_uint32_t weapon_kind_table = 0xFFFFFFFF;
  /* 18 / 37A4 */ be_uint32_t photon_color_table = 0xFFFFFFFF;
  /* 1C / 3A74 */ be_uint32_t weapon_range_table = 0xFFFFFFFF;
  /* 20 / 484C */ be_uint32_t weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 24 / 4A80 */ be_uint32_t non_weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 28 / 7384 */ be_uint32_t mag_feed_table = 0xFFFFFFFF;
  /* 2C / 4D50 */ be_uint32_t star_value_table = 0xFFFFFFFF;
  /* 30 / 4F72 */ be_uint32_t special_table = 0xFFFFFFFF;
  /* 34 / 5018 */ be_uint32_t weapon_effect_table = 0xFFFFFFFF;
  /* 38 / 68B8 */ be_uint32_t stat_boost_table = 0xFFFFFFFF;
  /* 3C / 61B8 */ be_uint32_t shield_effect_table = 0xFFFFFFFF;
  /* 40 / 69D8 */ be_uint32_t max_tech_level_table = 0xFFFFFFFF;
  /* 44 / 737C */ be_uint32_t combination_table = 0xFFFFFFFF;
  /* 48 / 68B0 */ be_uint32_t sound_remap_table = 0xFFFFFFFF;
  /* 4C / 6B1C */ be_uint32_t tech_boost_table = 0xFFFFFFFF;
} __packed_ws__(RootGCNTE, 0x50);

template <bool BE>
struct RootV3V4T {
  /* ## / GCV3 / XBV3 /  BBV4 */
  /* 00 / F078 / F084 / 14884 */ U32T<BE> weapon_table = 0xFFFFFFFF;
  /* 04 / EF90 / EF9C / 1478C */ U32T<BE> armor_table = 0xFFFFFFFF;
  /* 08 / EFA0 / EFAC / 1479C */ U32T<BE> unit_table = 0xFFFFFFFF;
  /* 0C / EFB0 / EFBC / 147AC */ U32T<BE> tool_table = 0xFFFFFFFF;
  /* 10 / EFA8 / EFB4 / 147A4 */ U32T<BE> mag_table = 0xFFFFFFFF;
  /* 14 / B88C / B8A0 / 0F4B8 */ U32T<BE> weapon_kind_table = 0xFFFFFFFF;
  /* 18 / A7FC / A7FC / 0DE7C */ U32T<BE> photon_color_table = 0xFFFFFFFF;
  /* 1C / AACC / AACC / 0E194 */ U32T<BE> weapon_range_table = 0xFFFFFFFF;
  /* 20 / B938 / B94C / 0F5A8 */ U32T<BE> weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 24 / BBCC / BBE0 / 0F83C */ U32T<BE> non_weapon_sale_divisor_table = 0xFFFFFFFF;
  /* 28 / F608 / F614 / 1502C */ U32T<BE> mag_feed_table = 0xFFFFFFFF;
  /* 2C / BE9C / BEB0 / 0FB0C */ U32T<BE> star_value_table = 0xFFFFFFFF;
  /* 30 / C100 / C114 / 0FE3C */ U32T<BE> special_table = 0xFFFFFFFF;
  /* 34 / C1A4 / C1B8 / 0FEE0 */ U32T<BE> weapon_effect_table = 0xFFFFFFFF;
  /* 38 / DE50 / DE64 / 1275C */ U32T<BE> stat_boost_table = 0xFFFFFFFF;
  /* 3C / D6E4 / D6F8 / 11C80 */ U32T<BE> shield_effect_table = 0xFFFFFFFF;
  /* 40 / DF88 / DF9C / 12894 */ U32T<BE> max_tech_level_table = 0xFFFFFFFF;
  /* 44 / F5D0 / F5DC / 14FF4 */ U32T<BE> combination_table = 0xFFFFFFFF;
  /* 48 / DE48 / DE5C / 12754 */ U32T<BE> sound_remap_table = 0xFFFFFFFF;
  /* 4C / EB8C / EBA0 / 14278 */ U32T<BE> tech_boost_table = 0xFFFFFFFF;
  /* 50 / F5F0 / F5FC / 15014 */ U32T<BE> unwrap_table = 0xFFFFFFFF;
  /* 54 / F5F8 / F604 / 1501C */ U32T<BE> unsealable_table = 0xFFFFFFFF;
  /* 58 / F600 / F60C / 15024 */ U32T<BE> ranged_special_table = 0xFFFFFFFF;
} __packed_ws_be__(RootV3V4T, 0x5C);

// All versions after v1 have a header before the actual data, which appears to be entirely unused by the game.
// TODO: Figure out what these bytes mean.
// V2:     29274435 3A44807F
// GC_NTE: FFFFFFFF 00000001 00010000 40400000 40C00000 FFFF0008 29274467 6C76807F 090C0D0F F0000000 00000003 00C80078 C8000000 00FA00C0 0000010C 00D20000
// GC_V3:  FFFFFFFF 00000001 00010000 40400000 40C00000 FFFF0008 29274467 6C768040 3F090C0D 0FF00000 00000003 00C80078 C8000000 010A00C7 0000011F 00DC0000
// XB_V3:  FFFFFFFF 01000000 00000100 00004040 0000C040 FFFF0800 29274467 6C768040 3F090C0D 0FF00000 03000000 C8007800 C8000000 0A01C700 00001F01 DC000000
// BB_V4:  FFFFFFFF 01000000 00000100 00004040 0000C040 FFFF0800 29274467 6C768040 3F090C0D 0FF00000 03000000 C8007800 C8000000 62010F01 00007A01 27010000

struct EmptyHeader {};

struct HeaderV2 {
  le_uint32_t unknown_a1 = 0x35442729;
  le_uint32_t unknown_a2 = 0x7F80443A;
} __packed_ws__(HeaderV2, 0x08);

struct HeaderGCNTE {
  /* 00 */ be_uint32_t unknown_a1 = 0xFFFFFFFF;
  /* 04 */ be_uint32_t unknown_a2 = 0x00000001;
  /* 08 */ be_uint32_t unknown_a3 = 0x00010000;
  /* 0C */ be_float unknown_a4 = 3.0;
  /* 10 */ be_float unknown_a5 = 6.0;
  /* 14 */ be_uint16_t unknown_a6 = 0xFFFF;
  /* 16 */ be_uint16_t unknown_a7 = 0x0008;
  /* 18 */ parray<uint8_t, 0x10> unknown_a8 = {0x29, 0x27, 0x44, 0x67, 0x6C, 0x76, 0x80, 0x7F, 0x09, 0x0C, 0x0D, 0x0F, 0xF0, 0x00, 0x00, 0x00};
  /* 28 */ be_uint32_t unknown_a9 = 0x00000003;
  /* 2C */ be_uint16_t unknown_a10 = 0x00C8;
  /* 2E */ be_uint16_t unknown_a11 = 0x0078;
  /* 30 */ parray<uint8_t, 4> unknown_a12 = {0xC8, 0x00, 0x00, 0x00};
  /* 34 */ parray<be_uint16_t, 6> unknown_a13 = {0x00FA, 0x00C0, 0x0000, 0x010C, 0x00D2, 0x0000};
} __packed_ws__(HeaderGCNTE, 0x40);

template <bool BE>
struct HeaderV3V4Base {
  /* 00 */ U32T<BE> unknown_a1 = 0xFFFFFFFF;
  /* 04 */ U32T<BE> unknown_a2 = 0x00000001;
  /* 08 */ U32T<BE> unknown_a3 = 0x00010000;
  /* 0C */ F32T<BE> unknown_a4 = 3.0;
  /* 10 */ F32T<BE> unknown_a5 = 6.0;
  /* 14 */ U16T<BE> unknown_a6 = 0xFFFF;
  /* 16 */ U16T<BE> unknown_a7 = 0x0008;
  /* 18 */ parray<uint8_t, 0x10> unknown_a8 = {0x29, 0x27, 0x44, 0x67, 0x6C, 0x76, 0x80, 0x40, 0x3F, 0x09, 0x0C, 0x0D, 0x0F, 0xF0, 0x00, 0x00};
  /* 28 */ U32T<BE> unknown_a9 = 0x00000003;
  /* 2C */ U16T<BE> unknown_a10 = 0x00C8;
  /* 2E */ U16T<BE> unknown_a11 = 0x0078;
  /* 30 */ parray<uint8_t, 4> unknown_a12 = {0xC8, 0x00, 0x00, 0x00};
} __packed_ws_be__(HeaderV3V4Base, 0x34);

template <bool BE>
struct HeaderV3 : HeaderV3V4Base<BE> {
  /* 34 */ parray<U16T<BE>, 6> unknown_a13 = {0x010A, 0x00C7, 0x0000, 0x011F, 0x00DC, 0x0000};
} __packed_ws_be__(HeaderV3, 0x40);

struct HeaderV4 : HeaderV3V4Base<false> {
  /* 34 */ parray<le_uint16_t, 6> unknown_a13 = {0x0162, 0x010F, 0x0000, 0x017A, 0x0127, 0x0000};
} __packed_ws_be__(HeaderV3, 0x40);

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

const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>>& ItemParameterTable::item_combinations_index() const {
  if (!this->item_combination_index.has_value()) {
    auto& ret = this->item_combination_index.emplace();
    for (size_t z = 0; z < this->num_item_combinations(); z++) {
      const auto& combo = this->get_item_combination(z);
      ret[item_code_to_u32(combo.used_item)].emplace_back(combo);
    }
  }
  return *this->item_combination_index;
}

const std::vector<ItemParameterTable::ItemCombination>& ItemParameterTable::all_combinations_for_used_item(
    const ItemData& used_item) const {
  try {
    return this->item_combinations_index().at(item_code_to_u32(
        used_item.data1[0], used_item.data1[1], used_item.data1[2]));
  } catch (const out_of_range&) {
    static const vector<ItemCombination> ret;
    return ret;
  }
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
    typename HeaderT,
    typename RootT,
    typename WeaponT,
    typename ArmorOrShieldT,
    typename UnitT,
    typename ToolT,
    typename MagT,
    bool HasImplicitPlaceholders,
    size_t ItemStarsFirstID,
    size_t SpecialStarsBeginIndex,
    size_t SoundRemapRTTableSize,
    bool HasServerHeader,
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
    if (item_index >= (co.count + HasImplicitPlaceholders)) {
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
    return this->get_data_array_count<ArrayRefT<BE>>(this->root->weapon_table);
  }

  virtual size_t num_weapons_in_class(uint8_t data1_1) const {
    if (data1_1 >= this->num_weapon_classes()) {
      throw out_of_range("weapon ID out of range");
    }
    return this->indirect_lookup_2d_count(this->root->weapon_table, data1_1);
  }

  virtual const Weapon& get_weapon(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 >= this->num_weapon_classes()) {
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
    return this->indirect_lookup_2d_count(this->root->armor_table, data1_1 - 1) + HasImplicitPlaceholders;
  }

  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const {
    if ((data1_1 < 1) || (data1_1 > 2)) {
      throw out_of_range("armor/shield class ID out of range");
    }
    auto& vec = (data1_1 == 1) ? this->armors : this->shields;
    return this->add_to_vector_cache_2d_indirect<ArmorOrShieldT>(vec, this->root->armor_table, data1_1 - 1, data1_2);
  }

  virtual size_t num_units() const {
    return this->indirect_lookup_2d_count(this->root->unit_table, 0) + HasImplicitPlaceholders;
  }

  virtual const Unit& get_unit(uint8_t data1_2) const {
    return this->add_to_vector_cache_2d_indirect<UnitT>(this->units, this->root->unit_table, 0, data1_2);
  }

  virtual size_t num_tool_classes() const {
    return this->get_data_array_count<ArrayRefT<BE>>(this->root->tool_table);
  }

  virtual size_t num_tools_in_class(uint8_t data1_1) const {
    if (data1_1 >= this->num_tool_classes()) {
      throw out_of_range("tool class ID out of range");
    }
    return this->indirect_lookup_2d_count(this->root->tool_table, data1_1);
  }

  virtual const Tool& get_tool(uint8_t data1_1, uint8_t data1_2) const {
    if (data1_1 >= this->num_tool_classes()) {
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
    const auto* cos = &this->r.pget<ArrayRefT<BE>>(
        this->root->tool_table, this->num_tool_classes() * sizeof(ArrayRefT<BE>));
    for (size_t z = 0; z < this->num_tool_classes(); z++) {
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
    return this->indirect_lookup_2d_count(this->root->mag_table, 0) + HasImplicitPlaceholders;
  }

  virtual const Mag& get_mag(uint8_t data1_1) const {
    return this->add_to_vector_cache_2d_indirect<MagT>(this->mags, this->root->mag_table, 0, data1_1);
  }

  virtual size_t num_weapon_kinds() const {
    return this->get_data_array_count<uint8_t>(this->root->weapon_kind_table);
  }

  virtual uint8_t get_weapon_kind(uint8_t data1_1) const {
    return (data1_1 < this->num_weapon_kinds()) ? this->r.pget_u8(this->root->weapon_kind_table + data1_1) : 0x00;
  }

  virtual size_t num_photon_colors() const {
    return this->get_data_array_count<PhotonColorEntryT<BE>>(this->root->photon_color_table);
  }

  virtual const PhotonColorEntry& get_photon_color(size_t index) const {
    return this->add_to_vector_cache<PhotonColorEntryT<BE>>(this->photon_colors, this->root->photon_color_table, index);
  }

  virtual size_t num_weapon_ranges() const {
    return this->get_data_array_count<WeaponRangeT<BE>>(this->root->weapon_range_table);
  }

  virtual const WeaponRange& get_weapon_range(size_t index) const {
    return this->add_to_vector_cache<WeaponRangeT<BE>>(this->weapon_ranges, this->root->weapon_range_table, index);
  }

  virtual size_t num_weapon_sale_divisors() const {
    if constexpr (requires { this->root->weapon_integral_sale_divisor_table; }) {
      return this->get_data_array_count<uint8_t>(this->root->weapon_integral_sale_divisor_table);
    } else {
      return this->get_data_array_count<F32T<BE>>(this->root->weapon_sale_divisor_table);
    }
  }

  virtual float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const {
    if (data1_0 == 0) {
      if (data1_1 >= this->num_weapon_sale_divisors()) {
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
    return std::make_pair(
        ItemStarsFirstID, this->get_data_array_count<uint8_t>(this->root->star_value_table) + ItemStarsFirstID);
  }

  virtual uint32_t get_special_stars_base_index() const {
    return SpecialStarsBeginIndex;
  }

  virtual uint8_t get_item_stars(uint32_t id) const {
    auto range = this->get_star_value_index_range();
    return ((id >= range.first) && (id < range.second))
        ? this->r.pget_u8(this->root->star_value_table + id - range.first)
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
    return this->get_data_array_count<SpecialT<BE>>(this->root->special_table);
  }

  virtual const Special& get_special(uint8_t special) const {
    special &= 0x3F;
    if (special >= this->num_specials()) {
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

  virtual size_t num_item_combinations() const {
    if constexpr (requires { this->root->combination_table; }) {
      return this->r.pget<ArrayRefT<BE>>(this->root->combination_table).count;
    } else {
      return 0;
    }
  }

  virtual const ItemCombination& get_item_combination(size_t index) const {
    if constexpr (requires { this->root->combination_table; }) {
      const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->combination_table);
      if (index >= co.count) {
        throw std::logic_error("Item combination index out of range");
      }
      return this->r.pget<ItemCombination>(co.offset + index * sizeof(ItemCombination));
    } else {
      throw std::logic_error("Item combinations not available");
    }
  }

  virtual const std::vector<SoundRemaps>& get_all_sound_remaps() const {
    if constexpr (requires { this->root->sound_remap_table; }) {
      if (!this->sound_remaps.has_value()) {
        auto& ret = this->sound_remaps.emplace();
        const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->sound_remap_table);
        const auto* entries = this->r.pget_array<SoundRemapTableOffsetsT<BE>>(co.offset, co.count);
        for (size_t z = 0; z < co.count; z++) {
          auto& remaps = ret.emplace_back();
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
      return *this->sound_remaps;
    } else {
      static const std::vector<SoundRemaps> empty_vec{};
      return empty_vec;
    }
  }

  virtual size_t num_tech_boosts() const {
    if constexpr (requires { this->root->tech_boost_table; }) {
      return this->get_data_array_count<TechBoostT<BE>>(this->root->tech_boost_table);
    } else {
      return 0;
    }
  }

  virtual const TechBoost& get_tech_boost(size_t index) const {
    if constexpr (requires { this->root->tech_boost_table; }) {
      return this->add_to_vector_cache<TechBoostT<BE>>(this->tech_boosts, this->root->tech_boost_table, index);
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
    if (this->start_offsets.empty()) {
      this->start_offsets = all_relocation_offsets_for_rel_file<BE>(r.pgetv(0, r.size()), r.size());
    }
    return this->start_offsets;
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

  static std::string serialize(const ItemParameterTable& pmt) {
    set<uint32_t> relocations;
    RootT root;
    phosg::StringWriter w;

    if constexpr (!std::is_same_v<HeaderT, EmptyHeader>) {
      w.put<HeaderT>(HeaderT());
    }

    auto align = [&w](size_t alignment) -> void {
      while (w.size() & (alignment - 1)) {
        w.put_u8(0);
      }
    };
    auto write_ref = [&w, &relocations](const ArrayRefT<BE>& ref) -> void {
      w.put<ArrayRefT<BE>>(ref);
      relocations.emplace(w.size() - 4);
    };

    if constexpr (requires { root.entry_count; }) {
      root.entry_count = 0x13;
    }

    align(4);
    ArrayRefT<BE> shields_ref{pmt.num_armors_or_shields_in_class(2) - HasImplicitPlaceholders, w.size()};
    for (size_t data1_2 = 0; data1_2 < (shields_ref.count + HasImplicitPlaceholders); data1_2++) {
      w.put<ArmorOrShieldT>(pmt.get_armor_or_shield(2, data1_2));
    }
    if constexpr (requires { root.shield_stat_boost_index_table; }) {
      root.shield_stat_boost_index_table = w.size();
      w.write(pmt.get_shield_stat_boost_index_table());
    }

    align(4);
    ArrayRefT<BE> armors_ref{pmt.num_armors_or_shields_in_class(1) - HasImplicitPlaceholders, w.size()};
    for (size_t data1_2 = 0; data1_2 < (armors_ref.count + HasImplicitPlaceholders); data1_2++) {
      w.put<ArmorOrShieldT>(pmt.get_armor_or_shield(1, data1_2));
    }
    if constexpr (requires { root.armor_stat_boost_index_table; }) {
      root.armor_stat_boost_index_table = w.size();
      w.write(pmt.get_armor_stat_boost_index_table());
    }

    align(4);
    ArrayRefT<BE> units_ref{pmt.num_units() - HasImplicitPlaceholders, w.size()};
    for (size_t data1_2 = 0; data1_2 < (units_ref.count + HasImplicitPlaceholders); data1_2++) {
      w.put<UnitT>(pmt.get_unit(data1_2));
    }

    align(4);
    ArrayRefT<BE> mags_ref{pmt.num_mags() - HasImplicitPlaceholders, w.size()};
    for (size_t data1_2 = 0; data1_2 < (mags_ref.count + HasImplicitPlaceholders); data1_2++) {
      w.put<MagT>(pmt.get_mag(data1_2));
    }

    align(4);
    std::vector<ArrayRefT<BE>> tool_refs;
    for (size_t data1_1 = 0; data1_1 < pmt.num_tool_classes(); data1_1++) {
      auto& ref = tool_refs.emplace_back(ArrayRefT<BE>{pmt.num_tools_in_class(data1_1), w.size()});
      for (size_t data1_2 = 0; data1_2 < ref.count; data1_2++) {
        w.put<ToolT>(pmt.get_tool(data1_1, data1_2));
      }
    }

    align(4);
    std::vector<ArrayRefT<BE>> weapon_refs;
    for (size_t data1_1 = 0; data1_1 < pmt.num_weapon_classes(); data1_1++) {
      auto& ref = weapon_refs.emplace_back(ArrayRefT<BE>{pmt.num_weapons_in_class(data1_1), w.size()});
      for (size_t data1_2 = 0; data1_2 < ref.count; data1_2++) {
        w.put<WeaponT>(pmt.get_weapon(data1_1, data1_2));
      }
    }
    if constexpr (requires { root.weapon_stat_boost_index_table; }) {
      root.weapon_stat_boost_index_table = w.size();
      w.write(pmt.get_weapon_stat_boost_index_table());
    }

    align(4);
    root.photon_color_table = w.size();
    for (size_t z = 0; z < pmt.num_photon_colors(); z++) {
      w.put<PhotonColorEntryT<BE>>(pmt.get_photon_color(z));
    }

    align(4);
    root.weapon_range_table = w.size();
    for (size_t z = 0; z < pmt.num_weapon_ranges(); z++) {
      w.put<WeaponRangeT<BE>>(pmt.get_weapon_range(z));
    }

    root.weapon_kind_table = w.size();
    for (size_t z = 0; z < pmt.num_weapon_classes(); z++) {
      w.put_u8(pmt.get_weapon_kind(z));
    }

    if constexpr (requires { root.weapon_integral_sale_divisor_table; }) {
      root.weapon_integral_sale_divisor_table = w.size();
      for (size_t z = 0; z < pmt.num_weapon_classes(); z++) {
        w.put_u8(pmt.get_sale_divisor(0, z));
      }
    } else {
      align(4);
      root.weapon_sale_divisor_table = w.size();
      for (size_t z = 0; z < pmt.num_weapon_sale_divisors(); z++) {
        w.put<F32T<BE>>(pmt.get_sale_divisor(0, z));
      }
    }

    if constexpr (requires { root.non_weapon_integral_sale_divisor_table; }) {
      root.non_weapon_integral_sale_divisor_table = w.size();
      NonWeaponSaleDivisorsDCProtos sds;
      sds.armor_divisor = pmt.get_sale_divisor(1, 1);
      sds.shield_divisor = pmt.get_sale_divisor(1, 2);
      sds.unit_divisor = pmt.get_sale_divisor(1, 3);
      w.put<NonWeaponSaleDivisorsDCProtos>(sds);
    } else {
      align(4);
      root.non_weapon_sale_divisor_table = w.size();
      NonWeaponSaleDivisorsT<BE> sds;
      sds.armor_divisor = pmt.get_sale_divisor(1, 1);
      sds.shield_divisor = pmt.get_sale_divisor(1, 2);
      sds.unit_divisor = pmt.get_sale_divisor(1, 3);
      sds.mag_divisor = pmt.get_sale_divisor(2, 0);
      w.put<NonWeaponSaleDivisorsT<BE>>(sds);
    }

    MagFeedResultsListOffsetsT<BE> mag_feed_result_offsets;
    for (size_t table_index = 0; table_index < 8; table_index++) {
      mag_feed_result_offsets[table_index] = w.size();
      for (size_t item_id = 0; item_id < 11; item_id++) {
        w.put<MagFeedResult>(pmt.get_mag_feed_result(table_index, item_id));
      }
    }

    root.star_value_table = w.size();
    w.write(pmt.get_star_value_table());

    if constexpr (requires { root.unknown_a1; }) {
      align(2);
      root.unknown_a1 = w.size();
      w.write(pmt.get_unknown_a1());
    }

    align(2);
    root.special_table = w.size();
    for (size_t z = 0; z < pmt.num_specials(); z++) {
      w.put<SpecialT<BE>>(pmt.get_special(z));
    }

    align(4);
    root.weapon_effect_table = w.size();
    for (size_t z = 0; z < pmt.num_weapon_effects(); z++) {
      w.put<WeaponEffectT<BE>>(pmt.get_weapon_effect(z));
    }

    align(4);
    if constexpr (requires { root.shield_effect_table; }) {
      root.shield_effect_table = w.size();
      for (size_t z = 0; z < pmt.num_shield_effects(); z++) {
        w.put<ShieldEffectT<BE>>(pmt.get_shield_effect(z));
      }
    }

    align(4);
    if constexpr (requires { root.sound_remap_table; }) {
      std::vector<SoundRemapTableOffsetsT<BE>> remap_refs;
      const auto& remaps = pmt.get_all_sound_remaps();
      for (const auto& remap : remaps) {
        auto& remap_ref = remap_refs.emplace_back();
        remap_ref.sound_id = remap.sound_id;
        remap_ref.remaps_for_rt_index_table = w.size();
        for (uint32_t remap_sound_id : remap.by_rt_index) {
          w.put<U32T<BE>>(remap_sound_id);
        }
        remap_ref.remaps_for_char_class_table = w.size();
        for (uint32_t remap_sound_id : remap.by_char_class) {
          w.put<U32T<BE>>(remap_sound_id);
        }
      }
      ArrayRefT<BE> remap_vec{remaps.size(), w.size()};
      for (const auto& remap_ref : remap_refs) {
        w.put<SoundRemapTableOffsetsT<BE>>(remap_ref);
        relocations.emplace(w.size() - 8);
        relocations.emplace(w.size() - 4);
      }
      root.sound_remap_table = w.size();
      write_ref(remap_vec);
    }

    align(4);
    root.stat_boost_table = w.size();
    for (size_t z = 0; z < pmt.num_stat_boosts(); z++) {
      w.put<StatBoostT<BE>>(pmt.get_stat_boost(z));
    }

    if constexpr (requires { root.max_tech_level_table; }) {
      root.max_tech_level_table = w.size();
      MaxTechniqueLevels max_tech_levels;
      for (size_t tech_num = 0; tech_num < 0x13; tech_num++) {
        for (size_t char_class = 0; char_class < 0x0C; char_class++) {
          max_tech_levels[tech_num][char_class] = pmt.get_max_tech_level(char_class, tech_num);
        }
      }
      w.put<MaxTechniqueLevels>(max_tech_levels);
    }

    ArrayRefT<BE> combination_table_ref;
    if constexpr (requires { root.combination_table; }) {
      combination_table_ref.offset = w.size();
      combination_table_ref.count = pmt.num_item_combinations();
      for (size_t z = 0; z < combination_table_ref.count; z++) {
        w.put<ItemCombination>(pmt.get_item_combination(z));
      }
    }

    if constexpr (requires { root.tech_boost_table; }) {
      align(4);
      root.tech_boost_table = w.size();
      for (size_t z = 0; z < pmt.num_tech_boosts(); z++) {
        w.put<TechBoostT<BE>>(pmt.get_tech_boost(z));
      }
    }

    std::vector<ArrayRefT<BE>> unwrap_table_refs;
    if constexpr (requires { root.unwrap_table; }) {
      for (size_t event = 0; event < pmt.num_events(); event++) {
        auto [event_items, num_items] = pmt.get_event_items(event);
        unwrap_table_refs.emplace_back(ArrayRefT<BE>{num_items, w.size()});
        w.write(event_items, sizeof(EventItem) * num_items);
      }
    }

    ArrayRefT<BE> unsealable_table_ref;
    if constexpr (requires { root.unsealable_table; }) {
      const auto& items = pmt.all_unsealable_items();
      unsealable_table_ref.count = items.size();
      unsealable_table_ref.offset = w.size();
      for (const auto& item : items) {
        UnsealableItem encoded;
        u32_to_item_code(encoded.item, item);
        w.put<UnsealableItem>(encoded);
      }
    }

    ArrayRefT<BE> ranged_specials_ref;
    if constexpr (requires { root.ranged_special_table; }) {
      ranged_specials_ref.count = pmt.num_ranged_specials();
      ranged_specials_ref.offset = w.size();
      for (size_t z = 0; z < ranged_specials_ref.count; z++) {
        w.put<RangedSpecial>(pmt.get_ranged_special(z));
      }
    }

    align(4);
    root.armor_table = w.size();
    write_ref(armors_ref);
    write_ref(shields_ref);
    root.unit_table = w.size();
    write_ref(units_ref);
    root.mag_table = w.size();
    write_ref(mags_ref);
    root.tool_table = w.size();
    for (const auto& ref : tool_refs) {
      write_ref(ref);
    }
    root.weapon_table = w.size();
    for (const auto& ref : weapon_refs) {
      write_ref(ref);
    }
    if constexpr (requires { root.combination_table; }) {
      root.combination_table = w.size();
      write_ref(combination_table_ref);
    }
    if constexpr (requires { root.unwrap_table; }) {
      ArrayRefT<BE> event_ref{unwrap_table_refs.size(), w.size()};
      for (const auto& ref : unwrap_table_refs) {
        write_ref(ref);
      }
      root.unwrap_table = w.size();
      write_ref(event_ref);
    }
    if constexpr (requires { root.unsealable_table; }) {
      root.unsealable_table = w.size();
      write_ref(unsealable_table_ref);
    }
    if constexpr (requires { root.ranged_special_table; }) {
      root.ranged_special_table = w.size();
      write_ref(ranged_specials_ref);
    }

    root.mag_feed_table = w.size();
    w.put<MagFeedResultsListOffsetsT<BE>>(mag_feed_result_offsets);
    for (size_t z = 1; z <= 8; z++) {
      relocations.emplace(w.size() - (z * 4));
    }

    RELFileFooterT<BE> footer;
    footer.root_offset = w.size();
    w.put<RootT>(root);
    constexpr size_t root_field_count = (sizeof(RootT) / 4) - ((requires { root.entry_count; }) ? 1 : 0);
    for (size_t z = 1; z <= root_field_count; z++) {
      relocations.emplace(w.size() - (z * 4));
    }

    align(0x20);
    footer.relocations_offset = w.size();
    footer.num_relocations = relocations.size();
    footer.unused1[0] = 1;
    uint32_t last_offset = 0;
    for (uint32_t reloc_offset : relocations) {
      if (reloc_offset & 3) {
        throw logic_error("Relocation is not 4-byte aligned");
      }
      size_t reloc_value = (reloc_offset - last_offset) >> 2;
      if (reloc_value > 0xFFFF) {
        throw runtime_error("Relocation offset is too far away from previous");
      }
      w.put<U16T<BE>>(reloc_value);
      last_offset = reloc_offset;
    }

    align(0x20);
    w.put<RELFileFooterT<BE>>(footer);

    return std::move(w.str());
  }

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;
  const RootT* root;

  mutable std::set<uint32_t> start_offsets;

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
  mutable std::optional<std::vector<SoundRemaps>> sound_remaps;
  mutable std::vector<TechBoost> tech_boosts;

  // Key is used_item. We can't index on (used_item, equipped_item) because equipped_item may contain wildcards, and
  // the matching order matters.
  mutable std::optional<std::map<uint32_t, std::vector<ItemCombination>>> item_combination_index;

  mutable std::optional<std::unordered_set<uint32_t>> unsealable_table;
};

using ItemParameterTableDCNTE = BinaryItemParameterTableT<
    EmptyHeader, // typename HeaderT
    RootDCProtos, // typename RootT
    WeaponDCProtos, // typename WeaponT
    ArmorOrShieldDCProtos, // typename ArmorOrShieldT
    UnitDCProtos, // typename UnitT
    ToolV1V2, // typename ToolT
    MagV1, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x22, // size_t ItemStarsFirstID
    0xAA, // size_t SpecialStarsBeginIndex
    0x00, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    false>; // bool BE
using ItemParameterTableDC112000 = BinaryItemParameterTableT<
    EmptyHeader, // typename HeaderT
    RootDCProtos, // typename RootT
    WeaponDCProtos, // typename WeaponT
    ArmorOrShieldDCProtos, // typename ArmorOrShieldT
    UnitDCProtos, // typename UnitT
    ToolV1V2, // typename ToolT
    MagV1, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x26, // size_t ItemStarsFirstID
    0xAE, // size_t SpecialStarsBeginIndex
    0x00, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    false>; // bool BE
using ItemParameterTableV1 = BinaryItemParameterTableT<
    EmptyHeader, // typename HeaderT
    RootV1, // typename RootT
    WeaponV1V2, // typename WeaponT
    ArmorOrShieldV1V2, // typename ArmorOrShieldT
    UnitV1V2, // typename UnitT
    ToolV1V2, // typename ToolT
    MagV1, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x26, // size_t ItemStarsFirstID
    0xAE, // size_t SpecialStarsBeginIndex
    0x00, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    false>; // bool BE
using ItemParameterTableV2 = BinaryItemParameterTableT<
    HeaderV2, // typename HeaderT
    RootV2, // typename RootT
    WeaponV1V2, // typename WeaponT
    ArmorOrShieldV1V2, // typename ArmorOrShieldT
    UnitV1V2, // typename UnitT
    ToolV1V2, // typename ToolT
    MagV2, // typename MagT
    true, // bool HasImplicitPlaceholders
    0x4E, // size_t ItemStarsFirstID
    0x138, // size_t SpecialStarsBeginIndex
    0x00, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    false>; // bool BE
using ItemParameterTableGCNTE = BinaryItemParameterTableT<
    HeaderGCNTE, // typename HeaderT
    RootGCNTE, // typename RootT
    WeaponGCNTE, // typename WeaponT
    ArmorOrShieldGC, // typename ArmorOrShieldT
    UnitGC, // typename UnitT
    ToolGC, // typename ToolT
    MagGC, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x76, // size_t ItemStarsFirstID
    0x1A3, // size_t SpecialStarsBeginIndex
    0x4F, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    true>; // bool BE
using ItemParameterTableGC = BinaryItemParameterTableT<
    HeaderV3<true>, // typename HeaderT
    RootV3V4T<true>, // typename RootT
    WeaponGC, // typename WeaponT
    ArmorOrShieldGC, // typename ArmorOrShieldT
    UnitGC, // typename UnitT
    ToolGC, // typename ToolT
    MagGC, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x94, // size_t ItemStarsFirstID
    0x1CB, // size_t SpecialStarsBeginIndex
    0x58, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    true>; // bool BE
using ItemParameterTableXB = BinaryItemParameterTableT<
    HeaderV3<false>, // typename HeaderT
    RootV3V4T<false>, // typename RootT
    WeaponXB, // typename WeaponT
    ArmorOrShieldXB, // typename ArmorOrShieldT
    UnitXB, // typename UnitT
    ToolXB, // typename ToolT
    MagXB, // typename MagT
    false, // bool HasImplicitPlaceholders
    0x94, // size_t ItemStarsFirstID
    0x1CB, // size_t SpecialStarsBeginIndex
    0x58, // size_t SoundRemapRTTableSize
    false, // bool HasServerHeader
    false>; // bool BE
using ItemParameterTableV4 = BinaryItemParameterTableT<
    HeaderV4, // typename HeaderT
    RootV3V4T<false>, // typename RootT
    WeaponV4, // typename WeaponT
    ArmorOrShieldV4, // typename ArmorOrShieldT
    UnitV4, // typename UnitT
    ToolV4, // typename ToolT
    MagV4, // typename MagT
    false, // bool HasImplicitPlaceholders
    0xB1, // size_t ItemStarsFirstID
    0x256, // size_t SpecialStarsBeginIndex
    0x6A, // size_t SoundRemapRTTableSize
    true, // bool HasServerHeader
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

std::string ItemParameterTable::serialize_binary(Version version) const {
  switch (version) {
    case Version::DC_NTE:
      return ItemParameterTableDCNTE::serialize(*this);
    case Version::DC_11_2000:
      return ItemParameterTableDC112000::serialize(*this);
    case Version::DC_V1:
      return ItemParameterTableV1::serialize(*this);
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      return ItemParameterTableV2::serialize(*this);
    case Version::GC_NTE:
      return ItemParameterTableGCNTE::serialize(*this);
    case Version::GC_V3:
    case Version::GC_EP3:
    case Version::GC_EP3_NTE:
      return ItemParameterTableGC::serialize(*this);
    case Version::XB_V3:
      return ItemParameterTableXB::serialize(*this);
    case Version::BB_V4:
      return ItemParameterTableV4::serialize(*this);
    default:
      throw std::logic_error("Cannot create item parameter table for this version");
  }
}
