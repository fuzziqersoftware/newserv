#pragma once

#include "WindowsPlatform.hh"

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <phosg/Encoding.hh>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "CommonFileFormats.hh"
#include "ItemData.hh"
#include "Text.hh"
#include "Types.hh"
#include "Version.hh"

// TODO: These don't really belong here, but putting them anywhere else creates annoying dependency cycles. Find or
// make a better place for these.
enum class ServerDropMode {
  DISABLED = 0,
  CLIENT = 1, // Not allowed for BB games
  SERVER_SHARED = 2,
  SERVER_PRIVATE = 3,
  SERVER_DUPLICATE = 4,
};
enum class ProxyDropMode {
  DISABLED = 0,
  PASSTHROUGH,
  INTERCEPT,
};

template <>
ServerDropMode phosg::enum_for_name<ServerDropMode>(const char* name);
template <>
const char* phosg::name_for_enum<ServerDropMode>(ServerDropMode value);

class ItemParameterTable {
public:
  // These structures are all parsed representations of the file's data. The file's actual structures are defined in
  // ItemParamterTable.cc, with analogous names to these structures.

  struct ItemBase {
    // id specifies several things; notably, it doubles as the index of the item's name in the text archive (e.g.
    // TextEnglish) collection 0.
    uint32_t id = 0xFFFFFFFF;
    uint16_t type = 0; // "Model" in Soly's ItemPMT editor
    uint16_t skin = 0; // "Texture" in Soly's ItemPMT editor
    uint32_t team_points = 0;

    void parse_base_from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };
  struct Weapon : ItemBase {
    uint16_t class_flags = 0;
    uint16_t atp_min = 0;
    uint16_t atp_max = 0;
    uint16_t atp_required = 0;
    uint16_t mst_required = 0;
    uint16_t ata_required = 0;
    uint16_t mst = 0;
    uint8_t max_grind = 0;
    uint8_t photon = 0;
    uint8_t special = 0;
    uint8_t ata = 0;
    uint8_t stat_boost_entry_index = 0;
    uint8_t projectile = 0;
    int8_t trail1_x = 0;
    int8_t trail1_y = 0;
    int8_t trail2_x = 0;
    int8_t trail2_y = 0;
    int8_t color = 0;
    parray<uint8_t, 3> unknown_a1 = 0;
    uint8_t unknown_a4 = 0;
    uint8_t unknown_a5 = 0;
    uint8_t tech_boost_entry_index = 0;
    // Bits in behavior_flags:
    //   01 = disable combos (weapon can only be used once in a row)
    //   02 = TODO (sets TItemWeapon flag 40000; used in TItemWeapon_v1E)
    //   04 = TODO (sets TItemWeapon flag 80000; used in TItemWeapon_v1E)
    //   08 = weapon cannot have attributes (they are ignored if present)
    uint8_t behavior_flags = 0;

    static Weapon from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct ArmorOrShield : ItemBase {
    uint16_t dfp = 0;
    uint16_t evp = 0;
    uint8_t block_particle = 0;
    uint8_t block_effect = 0;
    uint16_t class_flags = 0x00FF;
    uint8_t required_level = 0;
    uint8_t efr = 0;
    uint8_t eth = 0;
    uint8_t eic = 0;
    uint8_t edk = 0;
    uint8_t elt = 0;
    uint8_t dfp_range = 0;
    uint8_t evp_range = 0;
    uint8_t stat_boost_entry_index = 0;
    uint8_t tech_boost_entry_index = 0;
    // TODO: Figure out what this does. Only a few values appear to do anything:
    // Armors:
    //   01 sets item->flags |= 1 (used in TItemProArmor_v10)
    //   02 constructs TItemProArmorParticle instead of TItemProArmor
    // Shields:
    //   01 sets item->flags |= 4 (used in TItemProShield_v10)
    //   03 sets item->flags |= 8 (used in TItemProShield_v1A)
    uint8_t flags_type = 0;
    uint8_t unknown_a4 = 0;

    static ArmorOrShield from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct Unit : ItemBase {
    uint16_t stat = 0;
    uint16_t stat_amount = 0;
    int16_t modifier_amount = 0;

    static Unit from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct Mag : ItemBase {
    uint16_t feed_table = 0;
    uint8_t photon_blast = 0;
    uint8_t activation = 0;
    uint8_t on_pb_full = 0;
    uint8_t on_low_hp = 0;
    uint8_t on_death = 0;
    uint8_t on_boss = 0;
    // These flags control how likely each effect is to activate. First, the game computes step_synchro as follows:
    //   If synchro is in [0, 30], set step_synchro = 0
    //   If synchro is in [31, 60], set step_synchro = 15
    //   If synchro is in [61, 80], set step_synchro = 25
    //   If synchro is in [81, 100], set step_synchro = 30
    //   If synchro is in [101, 120], set step_synchro = 35
    // Then, the percent chance of the effect occurring upon its trigger (e.g. entering a boss arena) is:
    //   flag == 0 => chance is (activation)
    //   flag == 1 => chance is (activation + step_synchro)
    //   flag == 2 => chance is (step_synchro)
    //   flag == 3 => chance is (activation - 10)
    //   flag == 4 => chance is (step_synchro - 10)
    //   anything else => chance is 0 (effect never occurs)
    uint8_t on_pb_full_flag = 0;
    uint8_t on_low_hp_flag = 0;
    uint8_t on_death_flag = 0;
    uint8_t on_boss_flag = 0;
    uint16_t class_flags = 0x00FF;

    static Mag from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct Tool : ItemBase {
    uint16_t amount = 0;
    uint16_t tech = 0;
    int32_t cost = 0;
    // Bits in item_flags:
    //   00000001 - ever usable by player ("Use" appears in inventory menu)
    //   00000002 - unknown
    //   00000004 - unknown
    //   00000008 - usable by android characters
    //   00000010 - usable in Pioneer 2 / Lab
    //   00000020 - usable in boss arenas
    //   00000040 - usable in Challenge mode
    //   00000080 - is rare (renders as red box; V3+ only)
    uint32_t item_flags = 0;

    static Tool from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct MagFeedResult {
    int8_t def = 0;
    int8_t pow = 0;
    int8_t dex = 0;
    int8_t mind = 0;
    int8_t iq = 0;
    int8_t synchro = 0;
    parray<uint8_t, 2> unused;

    static MagFeedResult from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(MagFeedResult, 8);

  struct Special {
    uint16_t type = 0xFFFF;
    uint16_t amount = 0;

    static Special from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct StatBoost {
    // Only the first of these stat/amount pairs is used in most versions of the game. In DC 11/2000 Sega apparently
    // changed the loop from `for (z = 0; z != 2; z++)` to `for (z = 0; z != 1; z++)`, so only the first stat/amount
    // pair is used on all versions after DC NTE.
    // Values for stats:
    //   01 = ATP bonus
    //   02 = ATA bonus
    //   03 = EVP bonus
    //   04 = DFP bonus
    //   05 = MST bonus
    //   06 = HP bonus
    //   07 = LCK bonus
    //   08 = all of the above bonuses except HP
    //   09 = ATP penalty
    //   0A = ATA penalty
    //   0B = EVP penalty
    //   0C = DFP penalty
    //   0D = MST penalty
    //   0E = HP penalty
    //   0F = LCK penalty
    //   10 = all of the above penalties except HP
    //   Anything else (including 00) = no bonus or penalty
    uint8_t stat1 = 0;
    uint16_t amount1 = 0;
    uint8_t stat2 = 0;
    uint16_t amount2 = 0;

    static StatBoost from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __attribute__((packed));

  // Indexed as [tech_num][char_class]
  using MaxTechniqueLevels = parray<parray<uint8_t, 12>, 19>;

  struct ItemCombination {
    parray<uint8_t, 3> used_item;
    parray<uint8_t, 3> equipped_item;
    parray<uint8_t, 3> result_item;
    uint8_t mag_level = 0;
    uint8_t grind = 0;
    uint8_t level = 0;
    uint8_t char_class = 0;
    parray<uint8_t, 3> unused;

    static ItemCombination from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(ItemCombination, 0x10);

  struct TechBoost {
    // It appears that only one bit in the flags fields is used: 01 = enable piercing (for Megid)
    uint8_t tech_num1 = 0;
    uint8_t flags1 = 0;
    float amount1 = 0.0f;
    uint8_t tech_num2 = 0;
    uint8_t flags2 = 0;
    float amount2 = 0.0f;
    uint8_t tech_num3 = 0;
    uint8_t flags3 = 0;
    float amount3 = 0.0f;

    static TechBoost from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct EventItem {
    parray<uint8_t, 3> item;
    uint8_t probability = 0;

    static EventItem from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(EventItem, 4);

  struct UnsealableItem {
    parray<uint8_t, 3> item;
    uint8_t unused = 0;

    static UnsealableItem from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(UnsealableItem, 4);

  struct NonWeaponSaleDivisors {
    float armor_divisor = 0.0f;
    float shield_divisor = 0.0f;
    float unit_divisor = 0.0f;
    float mag_divisor = 0.0f;

    static NonWeaponSaleDivisors from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct ShieldEffect {
    uint32_t sound_id = 0;
    uint32_t unknown_a1 = 0;

    static ShieldEffect from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct PhotonColorEntry {
    uint32_t unknown_a1 = 0;
    VectorXYZTF unknown_a2;
    VectorXYZTF unknown_a3;

    static PhotonColorEntry from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct UnknownA1 {
    uint16_t unknown_a1 = 0;
    uint16_t unknown_a2 = 0;

    static UnknownA1 from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct WeaponEffect {
    uint32_t sound_id1 = 0;
    uint32_t eff_value1 = 0;
    uint32_t sound_id2 = 0;
    uint32_t eff_value2 = 0;
    parray<uint8_t, 0x10> unknown_a5;

    static WeaponEffect from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct WeaponRange {
    float unknown_a1 = 0;
    float unknown_a2 = 0;
    uint32_t unknown_a3 = 0; // Angle
    uint32_t unknown_a4 = 0; // Angle
    uint32_t unknown_a5 = 0;

    static WeaponRange from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  struct RangedSpecial {
    uint8_t data1_1 = 0;
    uint8_t data1_2 = 0;
    uint8_t weapon_range_index = 0;
    uint8_t unknown_a1 = 0;

    static RangedSpecial from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(RangedSpecial, 4);

  struct SoundRemaps {
    uint32_t sound_id = 0;
    std::vector<uint32_t> by_rt_index;
    std::vector<uint32_t> by_char_class;

    static SoundRemaps from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  virtual ~ItemParameterTable() = default;

  static std::shared_ptr<ItemParameterTable> from_binary(std::shared_ptr<const std::string> data, Version version);
  static std::shared_ptr<ItemParameterTable> from_json(const phosg::JSON& json);

  phosg::JSON json() const;
  // std::string serialize_binary() const; // TODO

  std::set<uint32_t> compute_all_valid_primary_identifiers() const;

  // weapon_table accessors
  virtual size_t num_weapon_classes() const = 0;
  virtual size_t num_weapons_in_class(uint8_t data1_1) const = 0;
  virtual const Weapon& get_weapon(uint8_t data1_1, uint8_t data1_2) const = 0;

  // armor_table accessors
  virtual size_t num_armors_or_shields_in_class(uint8_t data1_1) const = 0;
  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const = 0;

  // unit_table accessors
  virtual size_t num_units() const = 0;
  virtual const Unit& get_unit(uint8_t data1_2) const = 0;

  // tool_table accessors
  virtual size_t num_tool_classes() const = 0;
  virtual size_t num_tools_in_class(uint8_t data1_1) const = 0;
  virtual const Tool& get_tool(uint8_t data1_1, uint8_t data1_2) const = 0;
  virtual std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const = 0;

  // mag_table accessors
  virtual size_t num_mags() const = 0;
  virtual const Mag& get_mag(uint8_t data1_1) const = 0;

  // weapon_kind_table accessors (data1_1 in [0, num_weapon_classes()])
  virtual uint8_t get_weapon_kind(uint8_t data1_1) const = 0;

  // photon_color_table accessors
  virtual size_t num_photon_colors() const = 0;
  virtual const PhotonColorEntry& get_photon_color(size_t index) const = 0;

  // weapon_range_table accessors
  virtual size_t num_weapon_ranges() const = 0;
  virtual const WeaponRange& get_weapon_range(size_t index) const = 0;

  // weapon_sale_divisor_table and non_weapon_sale_divisor_table accessors (data1_0 in [0, 1, 2]; data1_1 in [0,
  // num_weapon_classes()] for weapons or ignored otherwise)
  virtual float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const = 0;

  // mag_feed_table accessors (table_index in [0, 7], item_index in [0, 10])
  virtual const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t item_index) const = 0;

  // star_value_table accessors
  virtual std::pair<uint32_t, uint32_t> get_star_value_index_range() const = 0;
  virtual uint32_t get_special_stars_base_index() const = 0;
  virtual uint8_t get_item_stars(uint32_t id) const = 0;
  virtual uint8_t get_special_stars(uint8_t special) const = 0;
  std::string get_star_value_table() const;

  // unknown_a1 accessors
  virtual std::string get_unknown_a1() const = 0;

  // special_table accessors
  virtual size_t num_specials() const = 0;
  virtual const Special& get_special(uint8_t special) const = 0;

  // weapon_effect_table accessors
  virtual size_t num_weapon_effects() const = 0;
  virtual const WeaponEffect& get_weapon_effect(size_t index) const = 0;

  // weapon_stat_boost_index_table accessors
  virtual size_t num_weapon_stat_boost_indexes() const = 0;
  virtual uint8_t get_weapon_stat_boost_index(size_t index) const = 0;
  std::string get_weapon_stat_boost_index_table() const;

  // armor_stat_boost_index_table accessors
  virtual size_t num_armor_stat_boost_indexes() const = 0;
  virtual uint8_t get_armor_stat_boost_index(size_t index) const = 0;
  std::string get_armor_stat_boost_index_table() const;

  // shield_stat_boost_index_table accessors
  virtual size_t num_shield_stat_boost_indexes() const = 0;
  virtual uint8_t get_shield_stat_boost_index(size_t index) const = 0;
  std::string get_shield_stat_boost_index_table() const;

  // stat_boost_table accessors
  virtual size_t num_stat_boosts() const = 0;
  virtual const StatBoost& get_stat_boost(size_t index) const = 0;

  // shield_effect_table accessors
  virtual size_t num_shield_effects() const = 0;
  virtual const ShieldEffect& get_shield_effect(size_t index) const = 0;

  // max_tech_level_table accessors
  virtual uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const = 0;

  // combination_table accessors
  virtual const std::map<uint32_t, std::vector<ItemCombination>>& all_item_combinations() const = 0;
  const std::vector<ItemCombination>& all_combinations_for_used_item(const ItemData& used_item) const;
  const ItemCombination& get_item_combination(const ItemData& used_item, const ItemData& equipped_item) const;

  // sound_remap_table accessors
  virtual const std::unordered_map<uint32_t, SoundRemaps>& get_all_sound_remaps() const = 0;

  // tech_boost_table accessors
  virtual size_t num_tech_boosts() const = 0;
  virtual const TechBoost& get_tech_boost(size_t index) const = 0;

  // unwrap_table accessors
  virtual size_t num_events() const = 0;
  virtual std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const = 0;

  // unsealable_table accessors
  virtual const std::unordered_set<uint32_t>& all_unsealable_items() const = 0;
  bool is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const;
  bool is_unsealable_item(const ItemData& item) const;

  // ranged_special_table accessors
  virtual size_t num_ranged_specials() const = 0;
  virtual const RangedSpecial& get_ranged_special(size_t index) const = 0;

  // Composite accessors
  std::variant<const Weapon*, const ArmorOrShield*, const Unit*, const Mag*, const Tool*>
  definition_for_primary_identifier(uint32_t primary_identifier) const;
  uint32_t get_item_id(const ItemData& item) const;
  uint32_t get_item_team_points(const ItemData& item) const;
  uint8_t get_item_base_stars(const ItemData& item) const;
  uint8_t get_item_adjusted_stars(const ItemData& item, bool ignore_unidentified = false) const;
  bool is_item_rare(const ItemData& item) const;
  size_t price_for_item(const ItemData& item) const;

protected:
  ItemParameterTable() = default;
};
