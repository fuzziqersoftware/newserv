#pragma once

#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>

#include "StaticGameData.hh"
#include "Text.hh"
#include "Types.hh"

struct ShopRandomSetBase {
  template <typename T>
  struct IntPairT {
    union {
      T first;
      T value;
      T min;
    } __attribute__((packed));
    union {
      T second;
      T weight;
      T max;
    } __attribute__((packed));

    IntPairT() = default;

    template <typename OtherT>
    IntPairT(const IntPairT<OtherT>& v) : first(v.first), second(v.second) {}

    IntPairT(const phosg::JSON& v) : first(v.get_int(0)), second(v.get_int(1)) {}

    phosg::JSON json() const {
      return phosg::JSON::list({this->first, this->second});
    }
  } __attribute__((packed));
};

struct ArmorShopRandomSet : ShopRandomSetBase {
  // This struct parses and accesses data from ArmorRandom.rel
  ArmorShopRandomSet(const void* data, size_t size, bool big_endian);
  ArmorShopRandomSet(const std::string& data, bool big_endian);
  explicit ArmorShopRandomSet(const phosg::JSON& json);

  template <bool BE>
  void parse_t(const void* data, size_t size);
  template <bool BE>
  std::string serialize_binary_t() const;

  std::string serialize_binary(bool big_endian) const;
  phosg::JSON json() const;

  void print(FILE* stream) const;

  std::vector<std::vector<IntPairT<uint8_t>>> armor_table;
  std::vector<std::vector<IntPairT<uint8_t>>> shield_table;
  std::vector<std::vector<IntPairT<uint8_t>>> unit_table;
};

struct ToolShopRandomSet : ShopRandomSetBase {
  // This struct parses and accesses data from ToolRandom.rel
  struct TechDiskLevelEntry {
    enum class Mode : uint8_t {
      LEVEL_1 = 0,
      PLAYER_LEVEL_DIVISOR = 1,
      RANDOM_IN_RANGE = 2,
    };
    Mode mode;
    uint8_t player_level_divisor_or_min_level;
    uint8_t max_level;

    TechDiskLevelEntry() = default;
    explicit TechDiskLevelEntry(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(TechDiskLevelEntry, 3);

  ToolShopRandomSet(const void* data, size_t size, bool big_endian);
  ToolShopRandomSet(const std::string& data, bool big_endian);
  explicit ToolShopRandomSet(const phosg::JSON& json);

  template <bool BE>
  void parse_t(const void* data, size_t size);
  template <bool BE>
  std::string serialize_binary_t() const;

  std::string serialize_binary(bool big_endian) const;
  phosg::JSON json() const;

  void print(FILE* stream) const;

  static const std::vector<std::pair<uint8_t, uint8_t>> item_defs;
  static const std::array<uint8_t, 0x13> tech_num_map;

  std::vector<std::vector<uint8_t>> common_recovery_table;
  std::vector<std::vector<IntPairT<uint8_t>>> rare_recovery_table;
  std::vector<std::vector<IntPairT<uint8_t>>> tech_disk_table;
  std::vector<std::vector<TechDiskLevelEntry>> tech_disk_level_table;
};

struct WeaponShopRandomSet : ShopRandomSetBase {
  // This struct parses and accesses data from WeaponRandom*.rel
  WeaponShopRandomSet(const void* data, size_t size, bool big_endian);
  WeaponShopRandomSet(const std::string& data, bool big_endian);
  explicit WeaponShopRandomSet(const phosg::JSON& json);

  template <bool BE>
  void parse_t(const void* data, size_t size);
  template <bool BE>
  std::string serialize_binary_t() const;

  std::string serialize_binary(bool big_endian) const;
  phosg::JSON json() const;

  void print(FILE* stream) const;

  static const std::array<std::pair<uint8_t, uint8_t>, 0x48> type_defs;
  static const std::array<std::pair<uint8_t, uint8_t>, 10> type_defs_39;
  static const std::array<std::pair<uint8_t, uint8_t>, 10> type_defs_3A;
  static const std::array<int8_t, 20> bonus_values;

  std::vector<std::vector<std::vector<IntPairT<uint8_t>>>> weapon_type_weight_tables; // [table_index][section_id][entry_index]
  std::array<std::array<IntPairT<uint32_t>, 6>, 9> bonus_type_table1; // [table_index][entry_index]
  std::array<std::array<IntPairT<uint32_t>, 6>, 9> bonus_type_table2; // [table_index][entry_index]
  std::array<IntPairT<uint32_t>, 9> bonus_range_table1; // [table_index]
  std::array<IntPairT<uint32_t>, 9> bonus_range_table2; // [table_index]
  std::array<std::array<IntPairT<uint32_t>, 3>, 8> special_mode_table; // [table_index][entry_index]
  std::array<IntPairT<uint32_t>, 6> default_grind_range_table; // [table_index]
  std::array<IntPairT<uint32_t>, 6> favored_grind_range_table; // [table_index]
};
