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
    uint8_t stat_boost_entry_index = 0; // TODO: This could be larger (16 or 32 bits)
    uint8_t projectile = 0;
    int8_t trail1_x = 0;
    int8_t trail1_y = 0;
    int8_t trail2_x = 0;
    int8_t trail2_y = 0;
    int8_t color = 0;
    parray<uint8_t, 3> unknown_a1 = 0;
    uint8_t unknown_a4 = 0;
    uint8_t unknown_a5 = 0;
    uint8_t tech_boost = 0;
    // Bits in behavior_flags:
    //   01 = disable combos (weapon can only be used once in a row)
    //   02 = TODO (sets TItemWeapon flag 40000; used in TItemWeapon_v1E)
    //   04 = TODO (sets TItemWeapon flag 80000; used in TItemWeapon_v1E)
    //   08 = weapon cannot have attributes (they are ignored if present)
    uint8_t behavior_flags = 0;
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
    uint8_t tech_boost = 0;
    // TODO: Figure out what this does. Only a few values appear to do anything:
    // Armors:
    //   01 sets item->flags |= 1 (used in TItemProArmor_v10)
    //   02 constructs TItemProArmorParticle instead of TItemProArmor
    // Shields:
    //   01 sets item->flags |= 4 (used in TItemProShield_v10)
    //   03 sets item->flags |= 8 (used in TItemProShield_v1A)
    uint8_t flags_type = 0;
    uint8_t unknown_a4 = 0;
  };

  struct Unit : ItemBase {
    uint16_t stat = 0;
    uint16_t stat_amount = 0;
    int16_t modifier_amount = 0;
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
    /* 0C */ uint32_t item_flags = 0;
  };

  struct MagFeedResult {
    int8_t def = 0;
    int8_t pow = 0;
    int8_t dex = 0;
    int8_t mind = 0;
    int8_t iq = 0;
    int8_t synchro = 0;
    parray<uint8_t, 2> unused;
  } __packed_ws__(MagFeedResult, 8);

  struct Special {
    uint16_t type = 0xFFFF;
    uint16_t amount = 0;
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
  } __packed_ws__(ItemCombination, 0x10);

  struct TechniqueBoost {
    uint8_t tech_num = 0;
    // It appears that only one bit in the flags field is used: 01 = enable piercing (for Megid)
    uint8_t flags = 0;
    float amount = 0.0f;
  };

  struct EventItem {
    parray<uint8_t, 3> item;
    uint8_t probability = 0;
  } __packed_ws__(EventItem, 4);

  struct UnsealableItem {
    parray<uint8_t, 3> item;
    uint8_t unused = 0;
  } __packed_ws__(UnsealableItem, 4);

  struct NonWeaponSaleDivisors {
    float armor_divisor = 0.0f;
    float shield_divisor = 0.0f;
    float unit_divisor = 0.0f;
    float mag_divisor = 0.0f;
  };

  struct ShieldEffect {
    uint32_t sound_id;
    uint32_t unknown_a1;
  };

  struct PhotonColorEntry {
    uint32_t unknown_a1;
    VectorXYZTF unknown_a2;
    VectorXYZTF unknown_a3;
  };

  struct UnknownA1 {
    uint16_t unknown_a1;
    uint16_t unknown_a2;
  };

  struct UnknownA5 {
    uint32_t target_param; // For players, char_class; for enemies, rt_index; for objects, 0x30
    uint32_t unknown_a2;
    uint32_t unknown_a3;
  };

  struct WeaponEffect {
    uint32_t sound_id1;
    uint32_t eff_value1;
    uint32_t sound_id2;
    uint32_t eff_value2;
    parray<uint8_t, 0x10> unknown_a5;
  };

  struct WeaponRange {
    float unknown_a1;
    float unknown_a2;
    uint32_t unknown_a3; // Angle
    uint32_t unknown_a4; // Angle
    uint32_t unknown_a5;
  };

  struct RangedSpecial {
    uint8_t data1_1;
    uint8_t data1_2;
    uint8_t weapon_range_index;
    uint8_t unknown_a1;
  } __packed_ws__(RangedSpecial, 4);

  ItemParameterTable() = delete;
  virtual ~ItemParameterTable() = default;

  static std::shared_ptr<ItemParameterTable> create(std::shared_ptr<const std::string> data, Version version);

  std::set<uint32_t> compute_all_valid_primary_identifiers() const;

  virtual size_t num_weapon_classes() const = 0;
  virtual size_t num_weapons_in_class(uint8_t data1_1) const = 0;
  virtual const Weapon& get_weapon(uint8_t data1_1, uint8_t data1_2) const = 0;
  virtual size_t num_armors_or_shields_in_class(uint8_t data1_1) const = 0;
  virtual const ArmorOrShield& get_armor_or_shield(uint8_t data1_1, uint8_t data1_2) const = 0;
  virtual size_t num_units() const = 0;
  virtual const Unit& get_unit(uint8_t data1_2) const = 0;
  virtual size_t num_mags() const = 0;
  virtual const Mag& get_mag(uint8_t data1_1) const = 0;
  virtual size_t num_tool_classes() const = 0;
  virtual size_t num_tools_in_class(uint8_t data1_1) const = 0;
  virtual const Tool& get_tool(uint8_t data1_1, uint8_t data1_2) const = 0;
  virtual std::pair<uint8_t, uint8_t> find_tool_by_id(uint32_t id) const = 0;

  std::variant<const Weapon*, const ArmorOrShield*, const Unit*, const Mag*, const Tool*>
  definition_for_primary_identifier(uint32_t primary_identifier) const;

  virtual float get_sale_divisor(uint8_t data1_0, uint8_t data1_1) const = 0;
  virtual const MagFeedResult& get_mag_feed_result(uint8_t table_index, uint8_t which) const = 0;
  virtual uint8_t get_item_stars(uint32_t id) const = 0;
  virtual uint8_t get_special_stars(uint8_t special) const = 0;
  virtual size_t num_specials() const = 0;
  virtual const Special& get_special(uint8_t special) const = 0;
  virtual const StatBoost& get_stat_boost(uint8_t entry_index) const = 0;
  virtual uint8_t get_max_tech_level(uint8_t char_class, uint8_t tech_num) const = 0;
  virtual uint8_t get_weapon_class(uint8_t data1_1) const = 0;

  uint32_t get_item_id(const ItemData& item) const;
  uint32_t get_item_team_points(const ItemData& item) const;
  uint8_t get_item_base_stars(const ItemData& item) const;
  uint8_t get_item_adjusted_stars(const ItemData& item, bool ignore_unidentified = false) const;
  bool is_item_rare(const ItemData& item) const;
  virtual bool is_unsealable_item(uint8_t data1_0, uint8_t data1_1, uint8_t data1_2) const = 0;
  bool is_unsealable_item(const ItemData& item) const;
  const ItemCombination& get_item_combination(const ItemData& used_item, const ItemData& equipped_item) const;
  const std::vector<ItemCombination>& get_all_combinations_for_used_item(const ItemData& used_item) const;
  virtual const std::map<uint32_t, std::vector<ItemCombination>>& get_all_item_combinations() const = 0;
  virtual size_t num_events() const = 0;
  virtual std::pair<const EventItem*, size_t> get_event_items(uint8_t event_number) const = 0;

  size_t price_for_item(const ItemData& item) const;

protected:
  std::shared_ptr<const std::string> data;
  phosg::StringReader r;

  mutable std::unordered_map<uint16_t, Weapon> weapons;
  mutable std::vector<ArmorOrShield> armors;
  mutable std::vector<ArmorOrShield> shields;
  mutable std::vector<Unit> units;
  mutable std::vector<Mag> mags;
  mutable std::unordered_map<uint16_t, Tool> tools;

  mutable std::vector<Special> specials;
  mutable std::vector<StatBoost> stat_boosts;

  // Key is used_item. We can't index on (used_item, equipped_item) because equipped_item may contain wildcards, and
  // the matching order matters.
  mutable std::map<uint32_t, std::vector<ItemCombination>> item_combination_index;

  explicit ItemParameterTable(std::shared_ptr<const std::string> data);
};

class MagEvolutionTable {
public:
  // TODO: V1 format is different! Offsets are 0438 0440 0498 0520 054C
  struct MotionReference {
    struct Side {
      // This specifies which entry in ItemMagMotion.dat is used. The file is just a list of 0x64-byte structures.
      // 0xFF = no TItemMagSub is created
      uint8_t motion_table_entry = 0xFF;
      parray<uint8_t, 5> unknown_a1 = 0;
    } __packed_ws__(Side, 0x06);
    parray<Side, 2> sides; // [0] = right side, [1] = left side
  } __packed_ws__(MotionReference, 0x0C);

  struct MotionReferenceTables {
    // It seems that there are two definition tables, but only the first is used on any version of PSO. On v3 and
    // later, the two offsets point to the same table, but on v2 they don't and the second table contains different
    // data. TODO: Figure out what the deal is with the different v2 tables.
    le_uint32_t ref_table; // -> MotionReference[num_mags]
    le_uint32_t unused_ref_table; // -> MotionReference[num_mags]
  } __packed_ws__(MotionReferenceTables, 0x08);

  struct ColorEntry {
    // Colors are specified as 4 floats, each in the range [0, 1], for each color channel. The default colors are:
    //   alpha   red     green   blue    color (see StaticGameData.cc)
    //   1.0     1.0     0.2     0.1     red
    //   1.0     0.2     0.2     1.0     blue
    //   1.0     1.0     0.9     0.1     yellow
    //   1.0     0.1     1.0     0.1     green
    //   1.0     0.8     0.1     1.0     purple
    //   1.0     0.1     0.1     0.2     black
    //   1.0     0.9     1.0     1.0     white
    //   1.0     0.1     0.9     1.0     cyan
    //   1.0     0.5     0.3     0.2     brown
    //   1.0     1.0     0.4     0.0     orange (v3+)
    //   1.0     0.502   0.545   0.977   light-blue (v3+)
    //   1.0     0.502   0.502   0.0     olive (v3+)
    //   1.0     0.0     0.941   0.714   turquoise (v3+)
    //   1.0     0.8     0.098   0.392   fuchsia (v3+)
    //   1.0     0.498   0.498   0.498   grey (v3+)
    //   1.0     0.996   0.996   0.832   cream (v3+)
    //   1.0     0.996   0.498   0.784   pink (v3+)
    //   1.0     0.0     0.498   0.322   dark-green (v3+)
    le_float alpha;
    le_float red;
    le_float green;
    le_float blue;
  } __packed_ws__(ColorEntry, 0x10);

  struct UnknownA3Entry {
    uint8_t flags;
    uint8_t unknown_a2;
    le_uint16_t unknown_a3;
    le_uint16_t unknown_a4;
    le_uint16_t unknown_a5;
  } __packed_ws__(UnknownA3Entry, 0x08);

  struct TableOffsets {
    // num_mags = 0x3A in v2 and GC NTE, 0x43 in V3, 0x53 in BB
    // num_colors = 0x09 in v2 and GC NTE, 0x12 in V3/BB
    // TODO: GC NTE uses the v2 format but is big-endian
    /* -- / V2   / V3   / BB */
    /* 00 / 05BC / 0340 / 0400 */ le_uint32_t motion_tables; // -> MotionReferenceTables
    /* 04 / 0594 / 0348 / 0408 */ le_uint32_t unknown_a2; // -> (uint8_t[2])[num_mags] (references into unknown_a3)
    /* 08 / 0608 / 03CE / 04AE */ le_uint32_t unknown_a3; // -> UnknownA3Entry[max(unknown_a2) + 1]
    /* 0C / 06B0 / 0476 / 0556 */ le_uint32_t unknown_a4; // -> (uint8_t)[num_mags]
    /* 10 / 06EC / 04BC / 05AC */ le_uint32_t color_table; // -> ColorEntry[num_colors]
    /* 14 / 077C / 05DC / 06CC */ le_uint32_t evolution_number; // -> uint8_t[num_mags]
  } __packed_ws__(TableOffsets, 0x18);

  MagEvolutionTable(std::shared_ptr<const std::string> data, size_t num_mags);
  ~MagEvolutionTable() = default;

  uint8_t get_evolution_number(uint8_t data1_1) const;

protected:
  std::shared_ptr<const std::string> data;
  size_t num_mags;
  phosg::StringReader r;
  const TableOffsets* offsets;
};
