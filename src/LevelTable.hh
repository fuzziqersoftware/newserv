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
  static CharacterStatsT<BE> from_json(const phosg::JSON& json) {
    return CharacterStatsT<BE>{
        json.at("ATP").as_int(),
        json.at("MST").as_int(),
        json.at("EVP").as_int(),
        json.at("HP").as_int(),
        json.at("DFP").as_int(),
        json.at("ATA").as_int(),
        json.at("LCK").as_int()};
  }
  phosg::JSON json() const {
    return phosg::JSON::dict({{"ATP", this->atp.load()},
        {"MST", this->mst.load()},
        {"EVP", this->evp.load()},
        {"HP", this->hp.load()},
        {"DFP", this->dfp.load()},
        {"ATA", this->ata.load()},
        {"LCK", this->lck.load()}});
  }
  operator CharacterStatsT<!BE>() const {
    return CharacterStatsT<!BE>{
        this->atp.load(),
        this->mst.load(),
        this->evp.load(),
        this->hp.load(),
        this->dfp.load(),
        this->ata.load(),
        this->lck.load()};
  }
} __packed_ws_be__(CharacterStatsT, 0x0E);
using CharacterStats = CharacterStatsT<false>;
using CharacterStatsBE = CharacterStatsT<true>;

template <bool BE>
struct PlayerStatsT {
  /* 00 */ CharacterStatsT<BE> char_stats;
  /* 0E */ U16T<BE> esp = 0;
  /* 10 */ F32T<BE> attack_range = 0.0;
  /* 14 */ F32T<BE> knockback_range = 0.0;
  /* 18 */ U32T<BE> level = 0; // Qedit specifies this as tech level when used for enemies
  /* 1C */ U32T<BE> exp = 0;
  /* 20 */ U32T<BE> meseta = 0; // Qedit specifies this as TP when used for enemies
  /* 24 */
  static PlayerStatsT<BE> from_json(const phosg::JSON& json) {
    return PlayerStatsT<BE>{
        CharacterStatsT<BE>::from_json(json),
        json.at("ESP").as_int(),
        json.at("AttackRange").as_float(),
        json.at("KnockbackRange").as_float(),
        json.at("Level").as_int(),
        json.at("EXP").as_int(),
        json.at("Meseta").as_int()};
  }
  phosg::JSON json() const {
    auto ret = this->char_stats.json();
    ret.emplace("ESP", this->esp.load());
    ret.emplace("AttackRange", this->attack_range.load());
    ret.emplace("KnockbackRange", this->knockback_range.load());
    ret.emplace("Level", this->level.load());
    ret.emplace("EXP", this->exp.load());
    ret.emplace("Meseta", this->meseta.load());
    return ret;
  }
  operator PlayerStatsT<!BE>() const {
    return PlayerStatsT<!BE>{
        this->char_stats,
        this->esp.load(),
        this->attack_range.load(),
        this->knockback_range.load(),
        this->level.load(),
        this->exp.load(),
        this->meseta.load()};
  }
} __packed_ws_be__(PlayerStatsT, 0x24);
using PlayerStats = PlayerStatsT<false>;
using PlayerStatsBE = PlayerStatsT<true>;

template <bool BE>
struct LevelStatsDeltaT {
  /* 00 */ uint8_t atp = 0;
  /* 01 */ uint8_t mst = 0;
  /* 02 */ uint8_t evp = 0;
  /* 03 */ uint8_t hp = 0;
  /* 04 */ uint8_t dfp = 0;
  /* 05 */ uint8_t ata = 0;
  /* 06 */ uint8_t lck = 0;
  /* 07 */ uint8_t tp = 0;
  /* 08 */ U32T<BE> exp = 0;
  /* 0C */
  static LevelStatsDeltaT<BE> from_json(const phosg::JSON& json) {
    return LevelStatsDeltaT<BE>{
        static_cast<uint8_t>(json.at("ATP").as_int()),
        static_cast<uint8_t>(json.at("MST").as_int()),
        static_cast<uint8_t>(json.at("EVP").as_int()),
        static_cast<uint8_t>(json.at("HP").as_int()),
        static_cast<uint8_t>(json.at("DFP").as_int()),
        static_cast<uint8_t>(json.at("ATA").as_int()),
        static_cast<uint8_t>(json.at("LCK").as_int()),
        static_cast<uint8_t>(json.at("TP").as_int()),
        static_cast<uint32_t>(json.at("EXP").as_int())};
  }
  phosg::JSON json() const {
    return phosg::JSON::dict({{"ATP", this->atp},
        {"MST", this->mst},
        {"EVP", this->evp},
        {"HP", this->hp},
        {"DFP", this->dfp},
        {"ATA", this->ata},
        {"LCK", this->lck},
        {"TP", this->tp},
        {"EXP", this->exp.load()}});
  }
  operator LevelStatsDeltaT<!BE>() const {
    return LevelStatsDeltaT<!BE>{
        this->atp, this->mst, this->evp, this->hp, this->dfp, this->ata, this->lck, this->tp, this->exp.load()};
  }

  void apply(CharacterStats& ps) const {
    ps.ata += this->ata;
    ps.atp += this->atp;
    ps.dfp += this->dfp;
    ps.evp += this->evp;
    ps.hp += this->hp;
    ps.mst += this->mst;
    ps.lck += this->lck;
  }
} __packed_ws_be__(LevelStatsDeltaT, 0x0C);
using LevelStatsDelta = LevelStatsDeltaT<false>;
using LevelStatsDeltaBE = LevelStatsDeltaT<true>;

class LevelTable {
  // This is the base class for all the LevelTable implementations. The public interface here only defines functions
  // that the server needs to handle requests, but some subclasses implement more functionality. See the comments and
  // Offsets structures inside the subclasses' constructor implementations for more details on the file formats.
public:
  virtual ~LevelTable() = default;

  virtual size_t num_char_classes() const = 0;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const = 0;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const = 0;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const = 0;

  void reset_to_base(PlayerStats& stats, uint8_t char_class) const;
  void advance_to_level(PlayerStats& stats, uint32_t level, uint8_t char_class) const;

  std::string serialize_binary_v4() const;
  phosg::JSON json() const;

protected:
  LevelTable() = default;
};

class JSONLevelTable : public LevelTable {
public:
  JSONLevelTable(const phosg::JSON& json);
  virtual ~JSONLevelTable() = default;

  virtual size_t num_char_classes() const;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

private:
  std::vector<CharacterStats> base_stats;
  std::vector<PlayerStats> max_stats;
  std::vector<std::array<LevelStatsDelta, 200>> level_deltas;
};

class LevelTableV2 : public LevelTable { // from PlayerTable.prs (PC)
public:
  struct HPTPFactors {
    le_float hp_factor;
    le_float tp_factor;
  } __packed_ws__(HPTPFactors, 8);

  struct UnknownA9 {
    le_float unknown_a1;
    le_float unknown_a2;
    le_float unknown_a3;
  } __packed_ws__(UnknownA9, 0x0C);

  struct AreaSoundConfig {
    le_uint16_t step_sound;
    le_uint16_t grass_step_sound;
    le_uint16_t water_step_sound;
    parray<uint8_t, 2> unused;
  } __packed_ws__(AreaSoundConfig, 8);

  struct WeaponReference {
    le_uint16_t data1_1;
    le_uint16_t data1_2;
  } __packed_ws__(WeaponReference, 4);

  struct UnknownA12 {
    le_float unknown_a1;
    le_float unknown_a2;
    le_float unknown_a3;
    le_float unknown_a4;
    le_uint32_t unknown_a5;
  } __packed_ws__(UnknownA12, 0x14);

  explicit LevelTableV2(const std::string& data);
  virtual ~LevelTableV2() = default;

  virtual size_t num_char_classes() const;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

protected:
  std::array<CharacterStats, 9> base_stats;
  std::array<PlayerStats, 9> max_stats;
  std::array<std::array<LevelStatsDelta, 200>, 9> level_deltas;
};

class LevelTableV3 : public LevelTable { // from PlyLevelTbl.cpt (GC/XB)
public:
  virtual ~LevelTableV3() = default;

  virtual size_t num_char_classes() const;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

protected:
  LevelTableV3() = default;
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};

class LevelTableGC : public LevelTableV3 {
public:
  explicit LevelTableGC(const std::string& data);
  virtual ~LevelTableGC() = default;
};

class LevelTableXB : public LevelTableV3 {
public:
  explicit LevelTableXB(const std::string& data);
  virtual ~LevelTableXB() = default;
};

class LevelTableV4 : public LevelTable { // from PlyLevelTbl.prs (BB)
public:
  explicit LevelTableV4(const std::string& data);
  virtual ~LevelTableV4() = default;

  virtual size_t num_char_classes() const;
  virtual const CharacterStats& base_stats_for_class(uint8_t char_class) const;
  virtual const PlayerStats& max_stats_for_class(uint8_t char_class) const;
  virtual const LevelStatsDelta& stats_delta_for_level(uint8_t char_class, uint8_t level) const;

protected:
  std::array<CharacterStats, 12> base_stats;
  std::array<std::array<LevelStatsDelta, 200>, 12> level_deltas;
};
