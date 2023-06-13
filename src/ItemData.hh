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
  ItemData(const ItemData& other);
  ItemData& operator=(const ItemData& other);

  bool operator==(const ItemData& other) const;
  bool operator!=(const ItemData& other) const;

  void clear();

  std::string hex() const;
  std::string name(bool include_color_codes) const;
  uint32_t primary_identifier() const;

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
