#pragma once

#include <stdint.h>

#include <array>
#include <memory>
#include <phosg/Encoding.hh>
#include <string>

class LevelTable;

struct CharacterStats {
  /* 00 */ le_uint16_t atp = 0;
  /* 02 */ le_uint16_t mst = 0;
  /* 04 */ le_uint16_t evp = 0;
  /* 06 */ le_uint16_t hp = 0;
  /* 08 */ le_uint16_t dfp = 0;
  /* 0A */ le_uint16_t ata = 0;
  /* 0C */ le_uint16_t lck = 0;
  /* 0E */
} __attribute__((packed));

struct PlayerStats {
  /* 00 */ CharacterStats char_stats;
  /* 0E */ le_uint16_t esp = 0;
  /* 10 */ le_float height = 0.0;
  /* 14 */ le_float unknown_a3 = 0.0;
  /* 18 */ le_uint32_t level = 0;
  /* 1C */ le_uint32_t experience = 0;
  /* 20 */ le_uint32_t meseta = 0;
  /* 24 */

  void reset_to_base(uint8_t char_class, std::shared_ptr<const LevelTable> level_table);
  void advance_to_level(uint8_t char_class, uint32_t level, std::shared_ptr<const LevelTable> level_table);
} __attribute__((packed));

template <bool IsBigEndian>
struct LevelStatsDeltaBase {
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  /* 00 */ uint8_t atp;
  /* 01 */ uint8_t mst;
  /* 02 */ uint8_t evp;
  /* 03 */ uint8_t hp;
  /* 04 */ uint8_t dfp;
  /* 05 */ uint8_t ata;
  /* 06 */ uint8_t lck;
  /* 07 */ uint8_t tp;
  /* 08 */ U32T experience;
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
} __attribute__((packed));

struct LevelStatsDelta : LevelStatsDeltaBase<false> {
} __attribute__((packed));
struct LevelStatsDeltaBE : LevelStatsDeltaBase<true> {
} __attribute__((packed));

class LevelTable {
  // This is the base class for all the LevelTable implementations. The public
  // interface here only defines functions that the server needs to handle
  // requests, but some subclasses implement more functionality. See the
  // comments and Offsets structures inside the subclasses' constructor
  // implementations for more details on the file formats.
public:
  virtual ~LevelTable() = default;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const = 0;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const = 0;

protected:
  LevelTable() = default;
};

class LevelTableV2 : public LevelTable { // from PlayerTable.prs (PC)
public:
  struct Level100Entry {
    /* 00 */ CharacterStats char_stats;
    /* 0E */ le_uint16_t unknown_a1 = 0;
    /* 10 */ le_float height = 0.0;
    /* 14 */ le_float unknown_a3 = 0.0;
    /* 18 */ le_uint32_t level = 0;
    /* 1C */
  } __attribute__((packed));

  LevelTableV2(const std::string& data, bool compressed);
  virtual ~LevelTableV2() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  const Level100Entry& level_100_stats_for_class(uint8_t char_class) const;
  const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<CharacterStats, 9> base_stats;
  std::array<Level100Entry, 9> level_100_stats;
  std::array<PlayerStats, 9> max_stats;
  std::array<std::array<LevelStatsDelta, 200>, 9> level_deltas;
};

class LevelTableV3BE : public LevelTable { // from PlyLevelTbl.cpt (GC)
public:
  LevelTableV3BE(const std::string& data, bool encrypted);
  virtual ~LevelTableV3BE() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};

class LevelTableV4 : public LevelTable { // from PlyLevelTbl.prs (BB)
public:
  LevelTableV4(const std::string& data, bool compressed);
  virtual ~LevelTableV4() = default;

  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::array<CharacterStats, 12> base_stats;
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};
