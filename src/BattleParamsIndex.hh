#pragma once

#include <inttypes.h>

#include <array>
#include <memory>
#include <phosg/Encoding.hh>
#include <random>
#include <string>
#include <vector>

#include "EnemyType.hh"
#include "LevelTable.hh"
#include "StaticGameData.hh"
#include "Text.hh"

class BattleParamsIndex {
public:
  // These files are little-endian, even on PSO GC.

  struct AttackData {
    /* 00 */ le_int16_t unknown_a1;
    /* 02 */ le_int16_t atp;
    /* 04 */ le_int16_t ata_bonus;
    /* 06 */ le_uint16_t unknown_a4;
    /* 08 */ le_float distance_x;
    /* 0C */ le_uint32_t angle_x; // Out of 0x10000 (high 16 bits are unused)
    /* 10 */ le_float distance_y;
    /* 14 */ le_uint16_t unknown_a8;
    /* 16 */ le_uint16_t unknown_a9;
    /* 18 */ le_uint16_t unknown_a10;
    /* 1A */ le_uint16_t unknown_a11;
    /* 1C */ le_uint32_t unknown_a12;
    /* 20 */ le_uint32_t unknown_a13;
    /* 24 */ le_uint32_t unknown_a14;
    /* 28 */ le_uint32_t unknown_a15;
    /* 2C */ le_uint32_t unknown_a16;
    /* 30 */
  } __packed_ws__(AttackData, 0x30);

  struct ResistData {
    /* 00 */ le_int16_t evp_bonus;
    /* 02 */ le_uint16_t efr;
    /* 04 */ le_uint16_t eic;
    /* 06 */ le_uint16_t eth;
    /* 08 */ le_uint16_t elt;
    /* 0A */ le_uint16_t edk;
    /* 0C */ le_uint32_t unknown_a6;
    /* 10 */ le_uint32_t unknown_a7;
    /* 14 */ le_uint32_t unknown_a8;
    /* 18 */ le_uint32_t unknown_a9;
    /* 1C */ le_int32_t dfp_bonus;
    /* 20 */
  } __packed_ws__(ResistData, 0x20);

  struct MovementData {
    /* 00 */ le_float idle_move_speed;
    /* 04 */ le_float idle_animation_speed;
    /* 08 */ le_float move_speed;
    /* 0C */ le_float animation_speed;
    /* 10 */ le_float unknown_a1;
    /* 14 */ le_float unknown_a2;
    /* 18 */ le_uint32_t unknown_a3;
    /* 1C */ le_uint32_t unknown_a4;
    /* 20 */ le_uint32_t unknown_a5;
    /* 24 */ le_uint32_t unknown_a6;
    /* 28 */ le_uint32_t unknown_a7;
    /* 2C */ le_uint32_t unknown_a8;
    /* 30 */
  } __packed_ws__(MovementData, 0x30);

  struct Table {
    /* 0000 */ parray<parray<PlayerStats, 0x60>, 4> stats;
    /* 3600 */ parray<parray<AttackData, 0x60>, 4> attack_data;
    /* 7E00 */ parray<parray<ResistData, 0x60>, 4> resist_data;
    /* AE00 */ parray<parray<MovementData, 0x60>, 4> movement_data;
    /* F600 */

    void print(FILE* stream) const;
  } __packed_ws__(Table, 0xF600);

  BattleParamsIndex(
      std::shared_ptr<const std::string> data_on_ep1, // BattleParamEntry_on.dat
      std::shared_ptr<const std::string> data_on_ep2, // BattleParamEntry_lab_on.dat
      std::shared_ptr<const std::string> data_on_ep4, // BattleParamEntry_ep4_on.dat
      std::shared_ptr<const std::string> data_off_ep1, // BattleParamEntry.dat
      std::shared_ptr<const std::string> data_off_ep2, // BattleParamEntry_lab.dat
      std::shared_ptr<const std::string> data_off_ep4); // BattleParamEntry_ep4.dat

  const Table& get_table(bool solo, Episode episode) const;

private:
  struct File {
    std::shared_ptr<const std::string> data;
    const Table* table = nullptr;
  };

  // Indexed as [online/offline][episode]
  std::array<std::array<File, 3>, 2> files;
};
