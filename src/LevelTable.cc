#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "Compression.hh"
#include "PSOEncryption.hh"

using namespace std;

void LevelTable::reset_to_base(PlayerStats& stats, uint8_t char_class) const {
  stats.level = 0;
  stats.experience = 0;
  stats.char_stats = this->base_stats_for_class(char_class);
}

void LevelTable::advance_to_level(PlayerStats& stats, uint32_t level, uint8_t char_class) const {
  for (; stats.level < level; stats.level++) {
    const auto& level_stats = this->stats_delta_for_level(char_class, stats.level + 1);
    // The original code clamps the resulting stat values to [0, max_stat]; we
    // don't have max_stat handy so we just allow them to be unbounded
    stats.char_stats.atp += level_stats.atp;
    stats.char_stats.mst += level_stats.mst;
    stats.char_stats.evp += level_stats.evp;
    stats.char_stats.hp += level_stats.hp;
    stats.char_stats.dfp += level_stats.dfp;
    stats.char_stats.ata += level_stats.ata;
    // Note: It is not a bug that lck is ignored here; the original code
    // ignores it too.
    stats.experience = level_stats.experience;
  }
}

LevelTableV2::LevelTableV2(const string& data, bool compressed) {
  struct Offsets {
    // TODO: The overall format of this file on V2 has much more data than we
    // actually use. What's known of the structure so far:
    le_uint32_t level_deltas; // -> u32[9] -> LevelStatsDelta[200]
    le_uint32_t unknown_a1; // -> float[6]
    le_uint32_t max_stats; // -> PlayerStats[9]
    le_uint32_t level_100_stats; // -> Level100Entry[9]
    le_uint32_t base_stats; // -> u32[9] -> CharacterStats
    le_uint32_t unknown_a2; // -> (0x120 zero bytes)
    le_uint32_t attack_data; // -> AttackData[9]
    le_uint32_t unknown_a4; // -> (0x14-byte struct)[9]
    le_uint32_t unknown_a5; // -> float[9]
    le_uint32_t unknown_a6; // -> (0x30 bytes)
    le_uint32_t unknown_a7; // -> (0x2D bytes)
    le_uint32_t unknown_a8; // -> u32[3] -> float[0x2D]
    le_uint32_t unknown_a9; // -> (0x90 bytes)
    le_uint32_t unknown_a10; // -> u32[3] -> (0x10-byte struct)[0x0C]
    le_uint32_t unknown_a11; // -> u32[3] -> (0x30-bytes)
    le_uint32_t unknown_a12; // -> u32[3] -> (0x14-byte struct)[0x0F]
  } __packed_ws__(Offsets, 0x40);

  StringReader r;
  string decompressed_data;
  if (compressed) {
    decompressed_data = prs_decompress(data);
    r = StringReader(decompressed_data);
  } else {
    r = StringReader(data);
  }

  const auto& offsets = r.pget<Offsets>(r.pget_u32l(r.size() - 0x10));
  const auto& level_deltas_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.level_deltas);
  const auto& base_stats_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.base_stats);
  for (size_t char_class = 0; char_class < 9; char_class++) {
    const auto& src_level_deltas = r.pget<parray<LevelStatsDelta, 200>>(level_deltas_offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      this->level_deltas[char_class][level] = src_level_deltas[level];
    }
    this->max_stats[char_class] = r.pget<PlayerStats>(offsets.max_stats + char_class * sizeof(PlayerStats));
    this->level_100_stats[char_class] = r.pget<Level100Entry>(offsets.level_100_stats + char_class * sizeof(Level100Entry));
    this->base_stats[char_class] = r.pget<CharacterStats>(base_stats_offsets[char_class]);
  }
}

const CharacterStats& LevelTableV2::base_stats_for_class(uint8_t char_class) const {
  return this->base_stats.at(char_class);
}

const LevelTableV2::Level100Entry& LevelTableV2::level_100_stats_for_class(uint8_t char_class) const {
  return this->level_100_stats.at(char_class);
}

const PlayerStats& LevelTableV2::max_stats_for_class(uint8_t char_class) const {
  return this->max_stats.at(char_class);
}

const LevelStatsDelta& LevelTableV2::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

LevelTableV3BE::LevelTableV3BE(const string& data, bool encrypted) {
  StringReader r;
  string decompressed_data;
  if (encrypted) {
    auto decrypted = decrypt_pr2_data<true>(data);
    decompressed_data = prs_decompress(decrypted.compressed_data);
    if (decompressed_data.size() != decrypted.decompressed_size) {
      throw runtime_error("decompressed data size does not match expected size");
    }
    r = StringReader(decompressed_data);
  } else {
    r = StringReader(data);
  }

  // The GC format is very simple (but everything is big-endian):
  //   root:
  //     u32 offset:
  //       u32[12] offsets:
  //         LevelStatsDeltaBE[200] level_deltas
  const auto& offsets = r.pget<parray<be_uint32_t, 12>>(r.pget_u32b(r.pget_u32b(r.size() - 0x10)));
  for (size_t char_class = 0; char_class < 12; char_class++) {
    const auto& src_deltas = r.pget<parray<LevelStatsDeltaBE, 200>>(offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      const auto& src_delta = src_deltas[level];
      auto& dest_delta = this->level_deltas[char_class][level];
      dest_delta.atp = src_delta.atp;
      dest_delta.mst = src_delta.mst;
      dest_delta.evp = src_delta.evp;
      dest_delta.hp = src_delta.hp;
      dest_delta.dfp = src_delta.dfp;
      dest_delta.ata = src_delta.ata;
      dest_delta.lck = src_delta.lck;
      dest_delta.tp = src_delta.tp;
      dest_delta.experience = src_delta.experience.load();
    }
  }
}

const CharacterStats& LevelTableV3BE::base_stats_for_class(uint8_t char_class) const {
  static const array<CharacterStats, 12> data = {
      //                ATP     MST     EVP      HP     DFP     ATA     LCK
      CharacterStats{0x0023, 0x001D, 0x002D, 0x0014, 0x0011, 0x001E, 0x000A},
      CharacterStats{0x001E, 0x0028, 0x003C, 0x0013, 0x0016, 0x0019, 0x000A},
      CharacterStats{0x0023, 0x0000, 0x0023, 0x0016, 0x0012, 0x0023, 0x000A},
      CharacterStats{0x0012, 0x0014, 0x0024, 0x0010, 0x000D, 0x0028, 0x000A},
      CharacterStats{0x0019, 0x0000, 0x001F, 0x0012, 0x0012, 0x002D, 0x000A},
      CharacterStats{0x0014, 0x0000, 0x001F, 0x0011, 0x0017, 0x002D, 0x000A},
      CharacterStats{0x000D, 0x0035, 0x0023, 0x0014, 0x000A, 0x000F, 0x000A},
      CharacterStats{0x000D, 0x003C, 0x0032, 0x0013, 0x0007, 0x000C, 0x000A},
      CharacterStats{0x000A, 0x003A, 0x0035, 0x0013, 0x000D, 0x000A, 0x000A},
      CharacterStats{0x0023, 0x0000, 0x0023, 0x0016, 0x0012, 0x0023, 0x000A},
      CharacterStats{0x000D, 0x0035, 0x0023, 0x0014, 0x000A, 0x000F, 0x000A},
      CharacterStats{0x0012, 0x0014, 0x0024, 0x0010, 0x000D, 0x0028, 0x000A},
  };
  return data.at(char_class);
}

const LevelStatsDelta& LevelTableV3BE::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

LevelTableV4::LevelTableV4(const string& data, bool compressed) {
  struct Offsets {
    le_uint32_t base_stats; // -> u32[12] -> CharacterStats
    le_uint32_t level_deltas; // -> u32[12] -> LevelStatsDelta[200]
  } __packed_ws__(Offsets, 8);

  StringReader r;
  string decompressed_data;
  if (compressed) {
    decompressed_data = prs_decompress(data);
    r = StringReader(decompressed_data);
  } else {
    r = StringReader(data);
  }

  const auto& offsets = r.pget<Offsets>(r.pget_u32l(r.size() - 0x10));
  const auto& level_deltas_offsets = r.pget<parray<le_uint32_t, 12>>(offsets.level_deltas);
  const auto& base_stats_offsets = r.pget<parray<le_uint32_t, 12>>(offsets.base_stats);
  for (size_t char_class = 0; char_class < 12; char_class++) {
    const auto& src_level_deltas = r.pget<parray<LevelStatsDelta, 200>>(level_deltas_offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      this->level_deltas[char_class][level] = src_level_deltas[level];
    }
    this->base_stats[char_class] = r.pget<CharacterStats>(base_stats_offsets[char_class]);
  }
}

const CharacterStats& LevelTableV4::base_stats_for_class(uint8_t char_class) const {
  return this->base_stats.at(char_class);
}

const LevelStatsDelta& LevelTableV4::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}
