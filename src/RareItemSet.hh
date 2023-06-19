#pragma once

#include <stdint.h>

#include <memory>
#include <random>
#include <string>

#include "GSLArchive.hh"
#include "StaticGameData.hh"

class RareItemSet {
public:
  struct PackedDrop {
    uint8_t probability;
    uint8_t item_code[3];

    static uint32_t expand_rate(uint8_t pc);
  } __attribute__((packed));

  struct ExpandedDrop {
    uint32_t probability;
    uint8_t item_code[3];

    ExpandedDrop();
    explicit ExpandedDrop(const PackedDrop&);
  };

  struct Table {
    // 0x280 in size; describes one difficulty, section ID, and episode
    // TODO: It looks like this structure can actually vary. In PSOGC, these all
    // appear to be the same size/format, but that's probably not strictly
    // required to be the case.
    /* 0000 */ parray<PackedDrop, 0x65> monster_rares;
    /* 0194 */ parray<uint8_t, 0x1E> box_areas;
    /* 01B2 */ parray<PackedDrop, 0x1E> box_rares;
    /* 022A */ parray<uint8_t, 2> unknown_a1;
    /* 022C */ be_uint32_t monster_rares_offset; // == 0x0000
    /* 0230 */ be_uint32_t box_count; // == 0x1E
    /* 0234 */ be_uint32_t box_areas_offset; // == 0x0194
    /* 0238 */ be_uint32_t box_rares_offset; // == 0x01B2
    /* 023C */ be_uint32_t unused_offset1;
    /* 0240 */ parray<be_uint16_t, 0x10> unknown_a2;
    /* 0260 */ be_uint32_t unknown_a2_offset;
    /* 0264 */ be_uint32_t unknown_a2_count;
    /* 0268 */ be_uint32_t unknown_a3;
    /* 026C */ be_uint32_t unknown_a4;
    /* 0270 */ be_uint32_t offset_table_offset; // == 0x022C
    /* 0274 */ parray<be_uint32_t, 3> unknown_a5;
    /* 0280 */

    std::vector<ExpandedDrop> get_enemy_specs(uint8_t enemy_type) const;
    std::vector<ExpandedDrop> get_box_specs(uint8_t area) const;
  } __attribute__((packed));

  virtual ~RareItemSet() = default;

  virtual std::vector<ExpandedDrop> get_enemy_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const = 0;
  virtual std::vector<ExpandedDrop> get_box_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const = 0;

protected:
  RareItemSet() = default;

  static uint16_t key_for_params(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid);
};

class GSLRareItemSet : public RareItemSet {
public:
  GSLRareItemSet(std::shared_ptr<const std::string> data, bool is_big_endian);
  virtual ~GSLRareItemSet() = default;
  virtual std::vector<ExpandedDrop> get_enemy_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const;
  virtual std::vector<ExpandedDrop> get_box_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const;

private:
  std::unordered_map<uint16_t, const Table*> tables;

  GSLArchive gsl;
};

class RELRareItemSet : public RareItemSet {
public:
  RELRareItemSet(std::shared_ptr<const std::string> data);
  virtual ~RELRareItemSet() = default;
  virtual std::vector<ExpandedDrop> get_enemy_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const;
  virtual std::vector<ExpandedDrop> get_box_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const;

private:
  std::shared_ptr<const std::string> data;

  const Table& get_table(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid) const;
};

class JSONRareItemSet : public RareItemSet {
public:
  JSONRareItemSet(std::shared_ptr<const JSONObject> json);
  virtual ~JSONRareItemSet() = default;

  virtual std::vector<ExpandedDrop> get_enemy_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const;
  virtual std::vector<ExpandedDrop> get_box_specs(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const;

private:
  struct SpecCollection {
    std::vector<std::vector<ExpandedDrop>> enemy_type_to_specs;
    std::vector<std::vector<ExpandedDrop>> box_area_to_specs;
  };

  std::unordered_map<uint16_t, SpecCollection> collections;
};
