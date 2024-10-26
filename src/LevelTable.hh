#pragma once

#include <stdint.h>

#include <array>
#include <memory>
#include <phosg/Encoding.hh>
#include <string>

#include "Text.hh"

class LevelTable;

template <bool BE>
struct CharacterStatsT {
  /* 00 */ U16T<BE> atp = 0;
  /* 02 */ U16T<BE> mst = 0;
  /* 04 */ U16T<BE> evp = 0;
  /* 06 */ U16T<BE> hp = 0;
  /* 08 */ U16T<BE> dfp = 0;
  /* 0A */ U16T<BE> ata = 0;
  /* 0C */ U16T<BE> lck = 0;
  /* 0E */

  operator CharacterStatsT<!BE>() const {
    CharacterStatsT<!BE> ret;
    ret.atp = this->atp.load();
    ret.mst = this->mst.load();
    ret.evp = this->evp.load();
    ret.hp = this->hp.load();
    ret.dfp = this->dfp.load();
    ret.ata = this->ata.load();
    ret.lck = this->lck.load();
    return ret;
  }
} __packed__;
using CharacterStats = CharacterStatsT<false>;
using CharacterStatsBE = CharacterStatsT<true>;
check_struct_size(CharacterStats, 0x0E);
check_struct_size(CharacterStatsBE, 0x0E);

template <bool BE>
struct PlayerStatsT {
  /* 00 */ CharacterStatsT<BE> char_stats;
  /* 0E */ U16T<BE> esp = 0;
  /* 10 */ F32T<BE> height = 0.0;
  /* 14 */ F32T<BE> unknown_a3 = 0.0;
  /* 18 */ U32T<BE> level = 0;
  /* 1C */ U32T<BE> experience = 0;
  /* 20 */ U32T<BE> meseta = 0;
  /* 24 */

  operator PlayerStatsT<!BE>() const {
    PlayerStatsT<!BE> ret;
    ret.char_stats = this->char_stats;
    ret.esp = this->esp.load();
    ret.height = this->height.load();
    ret.unknown_a3 = this->unknown_a3.load();
    ret.level = this->level.load();
    ret.experience = this->experience.load();
    ret.meseta = this->meseta.load();
    return ret;
  }
} __packed__;
using PlayerStats = PlayerStatsT<false>;
using PlayerStatsBE = PlayerStatsT<true>;
check_struct_size(PlayerStats, 0x24);
check_struct_size(PlayerStatsBE, 0x24);

template <bool BE>
struct LevelStatsDeltaT {
  /* 00 */ uint8_t atp;
  /* 01 */ uint8_t mst;
  /* 02 */ uint8_t evp;
  /* 03 */ uint8_t hp;
  /* 04 */ uint8_t dfp;
  /* 05 */ uint8_t ata;
  /* 06 */ uint8_t lck;
  /* 07 */ uint8_t tp;
  /* 08 */ U32T<BE> experience;
  /* 0C */

  void apply(CharacterStats& ps) const {
    ps.ata += this->ata;
    ps.atp += this->atp;
    ps.dfp += this->dfp;
    ps.evp += this->evp;
    ps.hp += this->hp;
    ps.mst += this->mst;
    ps.lck += this->lck;
  }
} __packed__;
using LevelStatsDelta = LevelStatsDeltaT<false>;
using LevelStatsDeltaBE = LevelStatsDeltaT<true>;
check_struct_size(LevelStatsDelta, 0x0C);
check_struct_size(LevelStatsDeltaBE, 0x0C);

class LevelTable {
  // This is the base class for all the LevelTable implementations. The public
  // interface here only defines functions that the server needs to handle
  // requests, but some subclasses implement more functionality. See the
  // comments and Offsets structures inside the subclasses' constructor
  // implementations for more details on the file formats.
public:
  virtual ~LevelTable() = default;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const = 0;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const = 0;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const = 0;

  void reset_to_base(PlayerStats& stats, uint8_t char_class) const;
  void advance_to_level(PlayerStats& stats, uint32_t level, uint8_t char_class) const;

protected:
  LevelTable() = default;
};

class LevelTableV2 : public LevelTable { // from PlayerTable.prs (PC)
public:
  LevelTableV2(const std::string& data, bool compressed);
  virtual ~LevelTableV2() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  const PlayerStats& level_100_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<CharacterStats, 9> base_stats;
  std::array<PlayerStats, 9> level_100_stats;
  std::array<PlayerStats, 9> max_stats;
  std::array<std::array<LevelStatsDelta, 200>, 9> level_deltas;
};

class LevelTableV3BE : public LevelTable { // from PlyLevelTbl.cpt (GC)
public:
  LevelTableV3BE(const std::string& data, bool encrypted);
  virtual ~LevelTableV3BE() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};

class LevelTableV4 : public LevelTable { // from PlyLevelTbl.prs (BB)
public:
  LevelTableV4(const std::string& data, bool compressed);
  virtual ~LevelTableV4() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<CharacterStats, 12> base_stats;
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};
