#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <random>
#include <string>
#include <vector>

#include "EnemyType.hh"
#include "StaticGameData.hh"
#include "Text.hh"

class BattleParamsIndex {
public:
  struct Entry {
    le_uint16_t atp; // attack power
    le_uint16_t psv; // perseverance (intelligence?)
    le_uint16_t evp; // evasion
    le_uint16_t hp; // hit points
    le_uint16_t dfp; // defense
    le_uint16_t ata; // accuracy
    le_uint16_t lck; // luck
    le_uint16_t esp; // ???
    parray<uint8_t, 0x0C> unknown_a1;
    le_uint32_t experience;
    le_uint32_t difficulty;

    std::string str() const;
  } __attribute__((packed));

  struct Table {
    parray<parray<Entry, 0x60>, 4> difficulty;

    void print(FILE* stream) const;
  } __attribute__((packed));

  BattleParamsIndex(
      std::shared_ptr<const std::string> data_on_ep1, // BattleParamEntry_on.dat
      std::shared_ptr<const std::string> data_on_ep2, // BattleParamEntry_lab_on.dat
      std::shared_ptr<const std::string> data_on_ep4, // BattleParamEntry_ep4_on.dat
      std::shared_ptr<const std::string> data_off_ep1, // BattleParamEntry.dat
      std::shared_ptr<const std::string> data_off_ep2, // BattleParamEntry_lab.dat
      std::shared_ptr<const std::string> data_off_ep4); // BattleParamEntry_ep4.dat

  const Entry& get(
      bool solo, Episode episode, uint8_t difficulty, EnemyType type) const;
  const Entry& get(
      bool solo, Episode episode, uint8_t difficulty, size_t entry_index) const;

private:
  struct LoadedFile {
    std::shared_ptr<const std::string> data;
    const Table* table;
  };

  // online/offline, episode
  LoadedFile files[2][3];
};
