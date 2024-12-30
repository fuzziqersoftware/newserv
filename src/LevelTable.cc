#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "CommonFileFormats.hh"
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
    le_uint32_t level_deltas; // (5468) -> u32[9] -> LevelStatsDelta[200]
    le_uint32_t unknown_a1; // (548C) -> float[6]
    le_uint32_t max_stats; // (54A4) -> PlayerStats[9]
    le_uint32_t level_100_stats; // (55E8) -> PlayerStats[9]
    le_uint32_t base_stats; // (57AC) -> u32[9] -> CharacterStats
    le_uint32_t unknown_a2; // (57D0) -> (0x120 zero bytes)
    le_uint32_t attack_data; // (58F0) -> AttackData[9]
    le_uint32_t unknown_a4; // (5AA0) -> parray<parray<float, 5>, 9>
    le_uint32_t unknown_a5; // (5B54) -> float[9]
    le_uint32_t unknown_a6; // (5B78) -> (0x30 bytes)
    le_uint32_t unknown_a7; // (5BA8) -> (0x2D bytes)
    le_uint32_t unknown_a8; // (5E00) -> u32[3] -> float[0x2D]
    le_uint32_t unknown_a9; // (5DF4) -> (0x90 bytes)
    le_uint32_t unknown_a10; // (60D0) -> u32[3] -> (0x10-byte struct)[0x0C]
    le_uint32_t unknown_a11; // (616C) -> u32[3] -> (0x30-bytes)
    le_uint32_t unknown_a12; // (64FC) -> u32[3] -> (0x14-byte struct)[0x0F]
  } __packed_ws__(Offsets, 0x40);

  phosg::StringReader r;
  string decompressed_data;
  if (compressed) {
    decompressed_data = prs_decompress(data);
    r = phosg::StringReader(decompressed_data);
  } else {
    r = phosg::StringReader(data);
  }

  const auto& footer = r.pget<RELFileFooter>(r.size() - sizeof(RELFileFooter));
  const auto& offsets = r.pget<Offsets>(footer.root_offset);
  const auto& level_deltas_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.level_deltas);
  const auto& base_stats_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.base_stats);
  for (size_t char_class = 0; char_class < 9; char_class++) {
    const auto& src_level_deltas = r.pget<parray<LevelStatsDelta, 200>>(level_deltas_offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      this->level_deltas[char_class][level] = src_level_deltas[level];
    }
    this->max_stats[char_class] = r.pget<PlayerStats>(offsets.max_stats + char_class * sizeof(PlayerStats));
    this->level_100_stats[char_class] = r.pget<PlayerStats>(offsets.level_100_stats + char_class * sizeof(PlayerStats));
    this->base_stats[char_class] = r.pget<CharacterStats>(base_stats_offsets[char_class]);
  }
}

const CharacterStats& LevelTableV2::base_stats_for_class(uint8_t char_class) const {
  return this->base_stats.at(char_class);
}

const PlayerStats& LevelTableV2::level_100_stats_for_class(uint8_t char_class) const {
  return this->level_100_stats.at(char_class);
}

const PlayerStats& LevelTableV2::max_stats_for_class(uint8_t char_class) const {
  return this->max_stats.at(char_class);
}

const LevelStatsDelta& LevelTableV2::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

LevelTableV3BE::LevelTableV3BE(const string& data, bool encrypted) {
  phosg::StringReader r;
  string decompressed_data;
  if (encrypted) {
    auto decrypted = decrypt_pr2_data<true>(data);
    decompressed_data = prs_decompress(decrypted.compressed_data);
    if (decompressed_data.size() != decrypted.decompressed_size) {
      throw runtime_error("decompressed data size does not match expected size");
    }
    r = phosg::StringReader(decompressed_data);
  } else {
    r = phosg::StringReader(data);
  }

  // The GC format is very simple (but everything is big-endian):
  //   root:
  //     u32 offset:
  //       u32[12] offsets:
  //         LevelStatsDeltaBE[200] level_deltas
  const auto& footer = r.pget<RELFileFooterBE>(r.size() - sizeof(RELFileFooterBE));
  const auto& offsets = r.pget<parray<be_uint32_t, 12>>(r.pget_u32b(footer.root_offset));
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

static const array<PlayerStats, 12> max_stats_v3_v4 = {
    //              ATP     MST     EVP      HP     DFP     ATA     LCK      ESP   PRX   PRY  L  E  M
    PlayerStats{{0x056B, 0x02DC, 0x02F4, 0x0265, 0x0243, 0x054B, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x04CB, 0x0499, 0x032B, 0x0254, 0x024D, 0x056C, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x065D, 0x0000, 0x0294, 0x0379, 0x0259, 0x0514, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x04E7, 0x0299, 0x02CB, 0x02D5, 0x0203, 0x06CB, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x0541, 0x0000, 0x02BB, 0x0430, 0x025E, 0x05FA, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x0492, 0x0000, 0x0313, 0x0439, 0x02B0, 0x062C, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x0365, 0x0504, 0x024C, 0x0302, 0x01F2, 0x0440, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x032B, 0x05DC, 0x02A7, 0x02E3, 0x01CF, 0x04B0, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x0244, 0x06D6, 0x0373, 0x02BE, 0x0186, 0x04EC, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x050B, 0x0000, 0x036D, 0x02EE, 0x020D, 0x05DC, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x03E7, 0x053C, 0x028B, 0x02CC, 0x01D6, 0x03F2, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
    PlayerStats{{0x0474, 0x0407, 0x0384, 0x02CF, 0x0241, 0x06C2, 0x0064}, 0x0064, 0.0f, 0.0f, 0, 0, 0},
};

const PlayerStats& LevelTableV3BE::max_stats_for_class(uint8_t char_class) const {
  return max_stats_v3_v4.at(char_class);
}

const LevelStatsDelta& LevelTableV3BE::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

LevelTableV4::LevelTableV4(const string& data, bool compressed) {
  struct Offsets {
    le_uint32_t base_stats; // -> u32[12] -> CharacterStats
    le_uint32_t level_deltas; // -> u32[12] -> LevelStatsDelta[200]
  } __packed_ws__(Offsets, 8);

  phosg::StringReader r;
  string decompressed_data;
  if (compressed) {
    decompressed_data = prs_decompress(data);
    r = phosg::StringReader(decompressed_data);
  } else {
    r = phosg::StringReader(data);
  }

  const auto& footer = r.pget<RELFileFooter>(r.size() - sizeof(RELFileFooter));
  const auto& offsets = r.pget<Offsets>(footer.root_offset);
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

const PlayerStats& LevelTableV4::max_stats_for_class(uint8_t char_class) const {
  return max_stats_v3_v4.at(char_class);
}

const LevelStatsDelta& LevelTableV4::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}
