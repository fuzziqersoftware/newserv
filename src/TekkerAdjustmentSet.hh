#pragma once

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>

#include "EnemyType.hh"
#include "GSLArchive.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Types.hh"

struct TekkerAdjustmentSet {
  // This struct parses and accesses data from JudgeItem.rel

  TekkerAdjustmentSet(const void* data, size_t size, bool big_endian);
  TekkerAdjustmentSet(const std::string& data, bool big_endian);
  explicit TekkerAdjustmentSet(const phosg::JSON& json);

  template <bool BE>
  void parse_t(const void* data, size_t size);
  template <bool BE>
  std::string serialize_binary_t() const;

  std::string serialize_binary(bool big_endian) const;
  phosg::JSON json() const;

  static uint8_t favored_weapon_type_for_section_id(uint8_t section_id);

  struct Table {
    std::unordered_map<int8_t, size_t> probs;
    size_t total;
  };

  std::array<Table, 10> favored_special_delta_table;
  std::array<Table, 10> default_special_delta_table;
  std::array<Table, 10> favored_grind_delta_table;
  std::array<Table, 10> default_grind_delta_table;
  std::array<Table, 10> favored_bonus_delta_table;
  std::array<Table, 10> default_bonus_delta_table;
  std::unordered_map<int8_t, int8_t> special_luck_table;
  std::unordered_map<int8_t, int8_t> grind_luck_table;
  std::unordered_map<int8_t, int8_t> bonus_luck_table;
};
