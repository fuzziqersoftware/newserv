#include "ItemParameterTable.hh"

#include "CommonFileFormats.hh"

using namespace std;

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
  /* 26 */ uint8_t tech_boost = 0;
  /* 27 */ uint8_t behavior_flags = 0;
  /* 28 */
  operator ItemParameterTable::Weapon() const {
    ItemParameterTable::Weapon ret = this->WeaponGCNTET<BE>::operator ItemParameterTable::Weapon();
    ret.unknown_a4 = this->unknown_a4;
    ret.unknown_a5 = this->unknown_a5;
    ret.tech_boost = this->tech_boost;
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
  /* 2A */ uint8_t tech_boost = 0;
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
    ret.tech_boost = this->tech_boost;
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
  /* 15 */ uint8_t tech_boost = 0;
  /* 16 */ uint8_t flags_type = 0;
  /* 17 */ uint8_t unknown_a4 = 0;
  /* 18 */
  operator ItemParameterTable::ArmorOrShield() const {
    ItemParameterTable::ArmorOrShield ret = this->ArmorOrShieldT<BaseT, BE>::operator ItemParameterTable::ArmorOrShield();
    ret.stat_boost_entry_index = this->stat_boost_entry_index;
    ret.tech_boost = this->tech_boost;
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
  parray<U16T<BE>, 2> amounts;
  operator ItemParameterTable::StatBoost() const {
    return {this->stats[0], this->amounts[0], this->stats[1], this->amounts[1]};
  }
} __packed_ws_be__(StatBoostT, 6);

template <bool BE>
struct TechniqueBoostEntryT {
  uint8_t tech_num = 0;
  uint8_t flags = 0;
  parray<uint8_t, 2> unused;
  F32T<BE> amount = 0.0f;
  operator ItemParameterTable::TechniqueBoost() const {
    return {this->tech_num, this->flags, this->amount};
  }
} __packed_ws_be__(TechniqueBoostEntryT, 8);

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
struct UnknownA5T {
  U32T<BE> target_param;
  U32T<BE> unknown_a2;
  U32T<BE> unknown_a3;
  operator ItemParameterTable::UnknownA5() const {
    return {this->target_param, this->unknown_a2, this->unknown_a3};
  }
} __packed_ws_be__(UnknownA5T, 0x0C);

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
struct WeaponEffect {
  U32T<BE> sound_id1;
  U32T<BE> eff_value1;
  U32T<BE> sound_id2;
  U32T<BE> eff_value2;
  parray<uint8_t, 0x10> unknown_a5;
  operator ItemParameterTable::WeaponEffect() const {
    return {this->sound_id1, this->eff_value1, this->sound_id2, this->eff_value2, this->unknown_a5};
  }
} __packed_ws_be__(WeaponEffect, 0x20);

/* The fields in the root structure are:
 * DCTE / 112K / V1   / V2   / GCTE / V3   / V4
 * 0013 / 0013 / 0013 / 0013 /      /      /       entry_count // Count of pointers in root struct; unused
 * 0668 / 0668 /      /      /      /      /       armor_stat_boost_index_table // -> [uint8_t]
 * 2D94 / 2E28 / 3258 / 5A5C / 6E4C / EF90 / 1478C armor_table // -> [{count, offset -> [ArmorOrShieldV*]}](2; armors and shields)
 *      /      /      /      / 737C / F5D0 / 14FF4 combination_table // -> {count, offset -> [ItemCombination]}
 * 2F54 / 2FF0 / 3420 / 5F4C / 7384 / F608 / 1502C mag_feed_table // -> MagFeedResultsTable
 * 2DAC / 2E40 / 3270 / 5A74 / 6E64 / EFA8 / 147A4 mag_table // -> {count, offset -> [MagV*]}
 *      /      /      /      / 69D8 / DF88 / 12894 max_tech_level_table // -> MaxTechniqueLevels
 * 1FE6 / 207A / 248C / 40A8 / 4A80 / BBCC / 0F83C non_weapon_sale_divisor_table // -> NonWeaponSaleDivisors
 * 1994 / 1A28 / 1DB0 / 2E4C / 37A4 / A7FC / 0DE7C photon_color_table // -> PhotonColorEntry[...]
 *      /      /      /      /      / F600 / 15024 ranged_special_table // -> {count, offset -> [RangedSpecial]}
 *      /      / 3198 / 5704 / 61B8 / D6E4 / 11C80 shield_effect_table // -> ShieldEffect[...] (indexed by data1[2])
 * 030C / 030C /      /      /      /      /       shield_stat_boost_index_table // -> [uint8_t]
 * 275E / 27F4 / 2C12 / 4540 / 4F72 / C100 / 0FE3C special_table // -> [Special]
 * 22A9 / 233D / 275C / 4378 / 4D50 / BE9C / 0FB0C star_value_table // -> [uint8_t] (indexed by .id from weapon, armor, etc.)
 * 2CE4 / 2D78 / 2CB8 / 58DC / 68B8 / DE50 / 1275C stat_boost_table // -> [StatBoost]
 *      /      /      /      / 6B1C / EB8C / 14278 tech_boost_table // -> [TechniqueBoostEntry[3]]
 * 2DB4 / 2E48 / 3278 / 5A7C / 6E6C / EFB0 / 147AC tool_table // -> [{count, offset -> [ToolV*]}] (last if out of range)
 * 2DA4 / 2E38 / 3268 / 5A6C / 6E5C / EFA0 / 1479C unit_table // -> {count, offset -> [UnitV*]} (last if out of range)
 * 23EE / 2484 / 28A2 / 45E4 /      /      /       unknown_a1 // TODO
 *      /      /      /      / 68B0 / DE48 / 12754 unknown_a5 // -> {count, offset -> [UnknownA5]}
 *      /      /      /      /      / F5F8 / 1501C unsealable_table // -> {count, offset -> [UnsealableItem]}
 *      /      /      /      /      / F5F0 / 15014 unwrap_table // -> {count, offset -> [{count, offset -> [EventItem]}]}
 * 1F98 / 202C / 23C8 / 3DF8 / 47BC / B88C / 0F4B8 weapon_class_table // -> [uint8_t](0x89)
 * 2804 / 2898 /      /      / 5018 / C1A4 / 0FEE0 weapon_effect_table // -> [WeaponEffect]
 * 1C64 / 1CF8 / 2080 / 32CC / 3A74 / AACC / 0E194 weapon_range_table // -> WeaponRange[...]
 * 1FBF / 2053 / 23F0 / 3E84 / 484C / B938 / 0F5A8 weapon_sale_divisor_table // -> [uint8_t] on DC protos; [float] on all other versions
 * 1908 / 199C /      /      /      /      /       weapon_stat_boost_index_table // -> [StatBoost]
 * 2E1C / 2EB8 / 32E8 / 5AFC / 6F0C / F078 / 14884 weapon_table // -> [{count, offset -> [WeaponV*]}]
 */

struct RootDCProtos {
  /* 00 */ le_uint32_t entry_count;
  /* 04 */ le_uint32_t weapon_table;
  /* 08 */ le_uint32_t armor_table;
  /* 0C */ le_uint32_t unit_table;
  /* 10 */ le_uint32_t tool_table;
  /* 14 */ le_uint32_t mag_table;
  /* 18 */ le_uint32_t weapon_class_table;
  /* 1C */ le_uint32_t photon_color_table;
  /* 20 */ le_uint32_t weapon_range_table;
  /* 24 */ le_uint32_t weapon_integral_sale_divisor_table;
  /* 28 */ le_uint32_t non_weapon_integral_sale_divisor_table;
  /* 2C */ le_uint32_t mag_feed_table;
  /* 30 */ le_uint32_t star_value_table;
  /* 34 */ le_uint32_t unknown_a1;
  /* 38 */ le_uint32_t special_table;
  /* 3C */ le_uint32_t weapon_effect_table;
  /* 40 */ le_uint32_t weapon_stat_boost_index_table;
  /* 44 */ le_uint32_t armor_stat_boost_index_table;
  /* 48 */ le_uint32_t shield_stat_boost_index_table;
  /* 4C */ le_uint32_t stat_boost_table;
} __packed_ws__(RootDCProtos, 0x50);

struct RootV1V2 {
  /* 00 */ le_uint32_t entry_count;
  /* 04 */ le_uint32_t weapon_table;
  /* 08 */ le_uint32_t armor_table;
  /* 0C */ le_uint32_t unit_table;
  /* 10 */ le_uint32_t tool_table;
  /* 14 */ le_uint32_t mag_table;
  /* 18 */ le_uint32_t weapon_class_table;
  /* 1C */ le_uint32_t photon_color_table;
  /* 20 */ le_uint32_t weapon_range_table;
  /* 24 */ le_uint32_t weapon_sale_divisor_table;
  /* 28 */ le_uint32_t non_weapon_sale_divisor_table;
  /* 2C */ le_uint32_t mag_feed_table;
  /* 30 */ le_uint32_t star_value_table;
  /* 34 */ le_uint32_t unknown_a1;
  /* 38 */ le_uint32_t special_table;
  /* 3C */ le_uint32_t stat_boost_table;
  /* 40 */ le_uint32_t shield_effect_table;
} __packed_ws__(RootV1V2, 0x44);

struct RootGCNTE {
  /* 00 */ be_uint32_t weapon_table;
  /* 04 */ be_uint32_t armor_table;
  /* 08 */ be_uint32_t unit_table;
  /* 0C */ be_uint32_t tool_table;
  /* 10 */ be_uint32_t mag_table;
  /* 14 */ be_uint32_t weapon_class_table;
  /* 18 */ be_uint32_t photon_color_table;
  /* 1C */ be_uint32_t weapon_range_table;
  /* 20 */ be_uint32_t weapon_sale_divisor_table;
  /* 24 */ be_uint32_t non_weapon_sale_divisor_table;
  /* 28 */ be_uint32_t mag_feed_table;
  /* 2C */ be_uint32_t star_value_table;
  /* 30 */ be_uint32_t special_table;
  /* 34 */ be_uint32_t weapon_effect_table;
  /* 38 */ be_uint32_t stat_boost_table;
  /* 3C */ be_uint32_t shield_effect_table;
  /* 40 */ be_uint32_t max_tech_level_table;
  /* 44 */ be_uint32_t combination_table;
  /* 48 */ be_uint32_t unknown_a5;
  /* 4C */ be_uint32_t tech_boost_table;
} __packed_ws__(RootGCNTE, 0x50);

template <bool BE>
struct RootV3V4T {
  /* 00 */ U32T<BE> weapon_table;
  /* 04 */ U32T<BE> armor_table;
  /* 08 */ U32T<BE> unit_table;
  /* 0C */ U32T<BE> tool_table;
  /* 10 */ U32T<BE> mag_table;
  /* 14 */ U32T<BE> weapon_class_table;
  /* 18 */ U32T<BE> photon_color_table;
  /* 1C */ U32T<BE> weapon_range_table;
  /* 20 */ U32T<BE> weapon_sale_divisor_table;
  /* 24 */ U32T<BE> non_weapon_sale_divisor_table;
  /* 28 */ U32T<BE> mag_feed_table;
  /* 2C */ U32T<BE> star_value_table;
  /* 30 */ U32T<BE> special_table;
  /* 34 */ U32T<BE> weapon_effect_table;
  /* 38 */ U32T<BE> stat_boost_table;
  /* 3C */ U32T<BE> shield_effect_table;
  /* 40 */ U32T<BE> max_tech_level_table;
  /* 44 */ U32T<BE> combination_table;
  /* 48 */ U32T<BE> unknown_a5;
  /* 4C */ U32T<BE> tech_boost_table;
  /* 50 */ U32T<BE> unwrap_table;
  /* 54 */ U32T<BE> unsealable_table;
  /* 58 */ U32T<BE> ranged_special_table;
} __packed_ws_be__(RootV3V4T, 0x5C);

ItemParameterTable::ItemParameterTable(std::shared_ptr<const std::string> data)
    : data(data), r(*this->data) {}

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

bool ItemParameterTable::is_item_rare(const ItemData& item) const {
  try {
    return (this->get_item_base_stars(item) >= 9);
  } catch (const out_of_range&) {
    return false;
  }
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
    bool BE>
class ItemParameterTableT : public ItemParameterTable {
public:
  explicit ItemParameterTableT(std::shared_ptr<const std::string> data)
      : ItemParameterTable(data),
        root(&this->r.pget<RootT>(BE ? r.pget_u32b(r.size() - 0x10) : r.pget_u32l(r.size() - 0x10))) {}
  ~ItemParameterTableT() = default;

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
    while (vec.size() <= data1_2) {
      vec.emplace_back(this->indirect_lookup_2d<ArmorOrShieldT>(this->root->armor_table, data1_1 - 1, vec.size()));
    }
    return vec[data1_2];
  }

  virtual size_t num_units() const {
    return this->indirect_lookup_2d_count(this->root->unit_table, 0);
  }

  virtual const Unit& get_unit(uint8_t data1_2) const {
    while (this->units.size() <= data1_2) {
      this->units.emplace_back(this->indirect_lookup_2d<UnitT>(this->root->unit_table, 0, this->units.size()));
    }
    return this->units[data1_2];
  }

  virtual size_t num_mags() const {
    return this->indirect_lookup_2d_count(this->root->mag_table, 0);
  }

  virtual const Mag& get_mag(uint8_t data1_1) const {
    while (this->mags.size() <= data1_1) {
      this->mags.emplace_back(this->indirect_lookup_2d<MagT>(this->root->mag_table, 0, data1_1));
    }
    return this->mags[data1_1];
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
    const auto* cos = &this->r.pget<ArrayRefT<BE>>(
        this->root->tool_table, NumToolClasses * sizeof(ArrayRefT<BE>));
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

  virtual uint8_t get_item_stars(uint32_t id) const {
    return ((id >= ItemStarsFirstID) && (id < ItemStarsLastID))
        ? this->r.pget_u8(this->root->star_value_table + id - ItemStarsFirstID)
        : 0;
  }

  virtual uint8_t get_special_stars(uint8_t special) const {
    return ((special & 0x3F) && !(special & 0x80)) ? this->get_item_stars(special + SpecialStarsBeginIndex) : 0;
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

  virtual const StatBoost& get_stat_boost(uint8_t entry_index) const {
    while (this->stat_boosts.size() <= entry_index) {
      this->stat_boosts.emplace_back(this->r.pget<StatBoostT<BE>>(
          this->root->stat_boost_table + sizeof(StatBoostT<BE>) * this->stat_boosts.size()));
    }
    return this->stat_boosts[entry_index];
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

  virtual uint8_t get_weapon_class(uint8_t data1_1) const {
    return (data1_1 < NumWeaponClasses) ? this->r.pget_u8(this->root->weapon_class_table + data1_1) : 0x00;
  }

  virtual bool is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const {
    if constexpr (requires { this->root->unsealable_table; }) {
      const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->unsealable_table);
      const auto* defs = &this->r.pget<UnsealableItem>(co.offset, co.count * sizeof(UnsealableItem));
      for (size_t z = 0; z < co.count; z++) {
        if ((defs[z].item[0] == data1_0) && (defs[z].item[1] == data1_1) && (defs[z].item[2] == data1_2)) {
          return true;
        }
      }
    }
    return false;
  }

  virtual const std::map<uint32_t, std::vector<ItemCombination>>& get_all_item_combinations() const {
    if constexpr (requires { this->root->combination_table; }) {
      if (this->item_combination_index.empty()) {
        const auto& co = this->r.pget<ArrayRefT<BE>>(this->root->combination_table);
        const auto* defs = &this->r.pget<ItemCombination>(co.offset, co.count * sizeof(ItemCombination));
        for (size_t z = 0; z < co.count; z++) {
          const auto& def = defs[z];
          uint32_t key = (def.used_item[0] << 16) | (def.used_item[1] << 8) | def.used_item[2];
          this->item_combination_index[key].emplace_back(def);
        }
      }
      return this->item_combination_index;
    }
    static const std::map<uint32_t, std::vector<ItemParameterTable::ItemCombination>> empty_map;
    return empty_map;
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

protected:
  const RootT* root;
};

using ItemParameterTableDCNTE = ItemParameterTableT<
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
    false>; // bool BE
using ItemParameterTableDC112000 = ItemParameterTableT<
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
    false>; // bool BE
using ItemParameterTableV1 = ItemParameterTableT<
    RootV1V2, // typename RootT
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
    false>; // bool BE
using ItemParameterTableV2 = ItemParameterTableT<
    RootV1V2, // typename RootT
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
    false>; // bool BE
using ItemParameterTableGCNTE = ItemParameterTableT<
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
    true>; // bool BE
using ItemParameterTableGC = ItemParameterTableT<
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
    true>; // bool BE
using ItemParameterTableXB = ItemParameterTableT<
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
    false>; // bool BE
using ItemParameterTableV4 = ItemParameterTableT<
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
    false>; // bool BE

std::shared_ptr<ItemParameterTable> ItemParameterTable::create(
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

MagEvolutionTable::MagEvolutionTable(shared_ptr<const string> data, size_t num_mags)
    : data(data),
      num_mags(num_mags),
      r(*data) {
  size_t offset_table_offset = this->r.pget_u32l(this->data->size() - 0x10);
  this->offsets = &r.pget<TableOffsets>(offset_table_offset);
}

uint8_t MagEvolutionTable::get_evolution_number(uint8_t data1_1) const {
  if (data1_1 >= this->num_mags) {
    throw runtime_error("invalid mag number");
  }
  return this->r.pget_u8(this->offsets->evolution_number + data1_1);
}
