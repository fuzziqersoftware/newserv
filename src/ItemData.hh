#pragma once

#include <phosg/Encoding.hh>
#include <string>

#include "Text.hh"

constexpr uint32_t MESETA_IDENTIFIER = 0x00040000;

struct ItemMagStats {
  uint16_t iq;
  uint16_t synchro;
  uint16_t def;
  uint16_t pow;
  uint16_t dex;
  uint16_t mind;
  uint8_t flags;
  uint8_t photon_blasts;
  uint8_t color;

  ItemMagStats()
      : iq(0),
        synchro(40),
        def(500),
        pow(0),
        dex(0),
        mind(0),
        flags(0),
        photon_blasts(0),
        color(14) {}

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

struct ItemData { // 0x14 bytes
  // QUICK ITEM FORMAT REFERENCE
  //           data1/0  data1/4  data1/8  data2
  //   Weapon: 00ZZZZGG SS00AABB AABBAABB 00000000
  //   Armor:  0101ZZ00 FFTTDDDD EEEE0000 00000000
  //   Shield: 0102ZZ00 FFTTDDDD EEEE0000 00000000
  //   Unit:   0103ZZ00 FF0000RR RR000000 00000000
  //   Mag:    02ZZLLWW HHHHIIII JJJJKKKK YYQQPPVV
  //   Tool:   03ZZZZFF 00CC0000 00000000 00000000
  //   Meseta: 04000000 00000000 00000000 MMMMMMMM
  // A = attribute type (for S-ranks, custom name)
  // B = attribute amount (for S-ranks, custom name)
  // C = stack size (for tools)
  // D = DEF bonus
  // E = EVP bonus
  // F = flags (40=present; for tools, unused if item is stackable)
  // G = weapon grind
  // H = mag DEF
  // I = mag POW
  // J = mag DEX
  // K = mag MIND
  // L = mag level
  // M = meseta amount
  // P = mag flags (40=present, 04=has left pb, 02=has right pb, 01=has center pb)
  // Q = mag IQ
  // R = unit modifier (little-endian)
  // S = weapon flags (80=unidentified, 40=present) and special (low 6 bits)
  // T = slot count
  // V = mag color
  // W = photon blasts
  // Y = mag synchro
  // Z = item ID
  // Note: PSO GC erroneously byteswaps data2 even when the item is a mag. This
  // makes it incompatible with little-endian versions of PSO (i.e. all other
  // versions). We manually byteswap data2 upon receipt and immediately before
  // sending where needed.

  union {
    parray<uint8_t, 12> data1;
    parray<le_uint16_t, 6> data1w;
    parray<le_uint32_t, 3> data1d;
  } __attribute__((packed));
  le_uint32_t id;
  union {
    parray<uint8_t, 4> data2;
    parray<le_uint16_t, 2> data2w;
    le_uint32_t data2d;
  } __attribute__((packed));

  ItemData();
  explicit ItemData(const std::string& orig_description, bool skip_special = false);
  ItemData(const ItemData& other);
  ItemData& operator=(const ItemData& other);

  bool operator==(const ItemData& other) const;
  bool operator!=(const ItemData& other) const;

  void clear();

  void bswap_data2_if_mag();

  std::string hex() const;
  std::string name(bool include_color_codes) const;
  uint32_t primary_identifier() const;

  bool is_wrapped() const;
  void unwrap();

  bool is_stackable() const;
  size_t stack_size() const;
  size_t max_stack_size() const;

  static bool is_common_consumable(uint32_t primary_identifier);
  bool is_common_consumable() const;

  void assign_mag_stats(const ItemMagStats& mag);
  void clear_mag_stats();
  uint16_t compute_mag_level() const;
  uint16_t compute_mag_strength_flags() const;
  uint8_t mag_photon_blast_for_slot(uint8_t slot) const;
  bool mag_has_photon_blast_in_any_slot(uint8_t pb_num) const;
  void add_mag_photon_blast(uint8_t pb_num);

  void set_sealed_item_kill_count(uint16_t v);
  uint8_t get_tool_item_amount() const;
  void set_tool_item_amount(uint8_t amount);
  int16_t get_armor_or_shield_defense_bonus() const;
  void set_armor_or_shield_defense_bonus(int16_t bonus);
  int16_t get_common_armor_evasion_bonus() const;
  void set_common_armor_evasion_bonus(int16_t bonus);
  int16_t get_unit_bonus() const;
  void set_unit_bonus(int16_t bonus);

  bool has_bonuses() const;
  bool is_s_rank_weapon() const;

  bool empty() const;

  static bool compare_for_sort(const ItemData& a, const ItemData& b);
} __attribute__((packed));

ItemData item_for_string(const std::string& desc);
