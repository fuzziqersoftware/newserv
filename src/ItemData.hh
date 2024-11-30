#pragma once

#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <string>

#include "Text.hh"
#include "Version.hh"

class ItemParameterTable;

enum class EquipSlot {
  // When equipping items through the Item Pack pause menu, the client sends
  // UNKNOWN for the slot. The receiving client (and server, in our case) have
  // to analyze the item being equipped and put it in the appropriate slot in
  // this case. See ItemData::default_equip_slot() for this computation.
  UNKNOWN = 0x00,
  // When equipping items through the quick menu or Equip pause menu, the client
  // sends one of the slots below.
  MAG = 0x01,
  ARMOR = 0x02,
  SHIELD = 0x03,
  WEAPON = 0x06,
  UNIT_1 = 0x09,
  UNIT_2 = 0x0A,
  UNIT_3 = 0x0B,
  UNIT_4 = 0x0C,
};

struct ItemMagStats {
  uint16_t iq = 0;
  uint16_t synchro = 40;
  uint16_t def = 500;
  uint16_t pow = 0;
  uint16_t dex = 0;
  uint16_t mind = 0;
  uint8_t flags = 0;
  uint8_t photon_blasts = 0;
  uint8_t color = 14;

  inline uint16_t def_level() const {
    return this->def / 100;
  }
  inline uint16_t pow_level() const {
    return this->pow / 100;
  }
  inline uint16_t dex_level() const {
    return this->dex / 100;
  }
  inline uint16_t mind_level() const {
    return this->mind / 100;
  }
  inline uint16_t level() const {
    return this->def_level() + this->pow_level() + this->dex_level() + this->mind_level();
  }
};

struct ItemData {
  struct StackLimits {
    Version version;
    std::vector<uint8_t> max_tool_stack_sizes_by_data1_1;
    uint32_t max_meseta_stack_size;

    StackLimits(Version version, const std::vector<uint8_t>& max_tool_stack_sizes_by_data1_1, uint32_t max_meseta_stack_size);
    StackLimits(Version version, const phosg::JSON& json);
    StackLimits(const StackLimits& other) = default;
    StackLimits(StackLimits&& other) = default;
    StackLimits& operator=(const StackLimits& other) = default;
    StackLimits& operator=(StackLimits&& other) = default;

    uint8_t get(uint8_t data1_0, uint8_t data1_1) const;

    static const std::vector<uint8_t> DEFAULT_TOOL_LIMITS_DC_NTE; // Also for 11/2000
    static const std::vector<uint8_t> DEFAULT_TOOL_LIMITS_V1_V2;
    static const std::vector<uint8_t> DEFAULT_TOOL_LIMITS_V3_V4;

    static const StackLimits DEFAULT_STACK_LIMITS_DC_NTE;
    static const StackLimits DEFAULT_STACK_LIMITS_V1_V2;
    static const StackLimits DEFAULT_STACK_LIMITS_V3_V4;
  };

  // QUICK ITEM FORMAT REFERENCE
  //           data1/0  data1/4  data1/8  data2
  //   Weapon: 00ZZZZGG SSNNAABB AABBAABB 00000000
  //   Armor:  0101ZZ00 FFTTDDDD EEEE0000 00000000
  //   Shield: 0102ZZ00 FFTTDDDD EEEE0000 00000000
  //   Unit:   0103ZZ00 FF00RRRR 00000000 00000000
  //   Mag:    02ZZLLWW HHHHIIII JJJJKKKK YYQQPPVV
  //   Tool:   03ZZZZUU 00CC0000 00000000 00000000
  //   Meseta: 04000000 00000000 00000000 MMMMMMMM
  // A = attribute type (for S-ranks, custom name)
  // B = attribute amount (for S-ranks, custom name)
  // C = stack size (for tools)
  // D = DEF bonus
  // E = EVP bonus
  // F = armor/shield/unit flags (40=present; low 4 bits are present color)
  // G = weapon grind
  // H = mag DEF
  // I = mag POW
  // J = mag DEX
  // K = mag MIND
  // L = mag level
  // M = meseta amount
  // N = present color (weapon only; for other types this is in the flags field)
  // P = mag flags (40=present, 04=has left pb, 02=has right pb, 01=has center pb)
  // Q = mag IQ
  // R = unit modifier (little-endian)
  // S = weapon flags (80=unidentified, 40=present) and special (low 6 bits)
  // T = slot count
  // U = tool flags (40=present; unused if item is stackable)
  // V = mag color
  // W = photon blasts
  // Y = mag synchro
  // Z = item ID
  // Note: PSO GC erroneously byteswaps data2 even when the item is a mag. This
  // makes it incompatible with little-endian versions of PSO (i.e. all other
  // versions). We manually byteswap data2 upon receipt and immediately before
  // sending where needed.
  // Related note: PSO V2 has an annoyingly complicated format for mags that
  // doesn't match the above table. We decode this upon receipt and encode it
  // imemdiately before sending when interacting with V2 clients; see the
  // implementation of decode_for_version() for details.

  union {
    parray<uint8_t, 12> data1;
    parray<le_uint16_t, 6> data1w;
    parray<be_uint16_t, 6> data1wb;
    parray<le_uint32_t, 3> data1d;
    parray<be_uint32_t, 3> data1db;
  } __packed__;
  le_uint32_t id;
  union {
    parray<uint8_t, 4> data2;
    parray<le_uint16_t, 2> data2w;
    parray<be_uint16_t, 2> data2wb;
    le_uint32_t data2d;
    be_uint32_t data2db;
  } __packed__;

  ItemData();
  ItemData(const ItemData& other);
  ItemData(uint64_t first, uint64_t second = 0);
  ItemData& operator=(const ItemData& other);

  bool operator==(const ItemData& other) const;
  bool operator!=(const ItemData& other) const;

  bool operator<(const ItemData& other) const;

  void clear();

  static ItemData from_data(const std::string& data);
  static ItemData from_primary_identifier(const StackLimits& limits, uint32_t primary_identifier);
  std::string hex() const;
  std::string short_hex() const;
  uint32_t primary_identifier() const;

  bool is_wrapped(const StackLimits& limits) const;
  void wrap(const StackLimits& limits, uint8_t present_color);
  void unwrap(const StackLimits& limits);

  bool is_stackable(const StackLimits& limits) const;
  size_t stack_size(const StackLimits& limits) const;
  size_t max_stack_size(const StackLimits& limits) const;
  void enforce_min_stack_size(const StackLimits& limits);

  static bool is_common_consumable(uint32_t primary_identifier);
  bool is_common_consumable() const;

  void assign_mag_stats(const ItemMagStats& mag);
  void clear_mag_stats();
  uint16_t compute_mag_level() const;
  uint16_t compute_mag_strength_flags() const;
  uint8_t mag_photon_blast_for_slot(uint8_t slot) const;
  bool mag_has_photon_blast_in_any_slot(uint8_t pb_num) const;
  void add_mag_photon_blast(uint8_t pb_num);
  void decode_for_version(Version version);
  void encode_for_version(Version version, std::shared_ptr<const ItemParameterTable> item_parameter_table);
  uint8_t get_encoded_v2_data() const;
  bool has_encoded_v2_data() const;

  bool has_kill_count() const;
  uint16_t get_kill_count() const;
  void set_kill_count(uint16_t v);
  uint8_t get_tool_item_amount(const StackLimits& limits) const;
  void set_tool_item_amount(const StackLimits& limits, uint8_t amount);
  int16_t get_armor_or_shield_defense_bonus() const;
  void set_armor_or_shield_defense_bonus(int16_t bonus);
  int16_t get_common_armor_evasion_bonus() const;
  void set_common_armor_evasion_bonus(int16_t bonus);
  int16_t get_unit_bonus() const;
  void set_unit_bonus(int16_t bonus);

  bool has_bonuses() const;
  bool is_s_rank_weapon() const;

  EquipSlot default_equip_slot() const;
  bool can_be_equipped_in_slot(EquipSlot slot) const;

  bool can_be_encoded_in_rel_rare_table() const;

  bool empty() const;

  static bool compare_for_sort(const ItemData& a, const ItemData& b);
} __packed_ws__(ItemData, 0x14);
