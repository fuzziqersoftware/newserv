#include "LevelTable.hh"

#include <string.h>

#include <phosg/Filesystem.hh>

#include "CommonFileFormats.hh"
#include "StaticGameData.hh"

using namespace std;

void LevelTable::reset_to_base(PlayerStats& stats, uint8_t char_class) const {
  stats.level = 0;
  stats.exp = 0;
  stats.char_stats = this->base_stats_for_class(char_class);
}

void LevelTable::advance_to_level(PlayerStats& stats, uint32_t level, uint8_t char_class) const {
  for (; stats.level < level; stats.level++) {
    const auto& level_stats = this->stats_delta_for_level(char_class, stats.level + 1);
    // The original code clamps the resulting stat values to [0, max_stat]; we don't have max_stat handy so we just
    // allow them to be unbounded
    stats.char_stats.atp += level_stats.atp;
    stats.char_stats.mst += level_stats.mst;
    stats.char_stats.evp += level_stats.evp;
    stats.char_stats.hp += level_stats.hp;
    stats.char_stats.dfp += level_stats.dfp;
    stats.char_stats.ata += level_stats.ata;
    // Note: It is not a bug that lck is ignored here; the original code ignores it too.
    stats.exp = level_stats.exp;
  }
}

phosg::JSON LevelTable::json() const {
  auto base_stats_json = phosg::JSON::list();
  auto max_stats_json = phosg::JSON::list();
  auto level_deltas_json = phosg::JSON::list();
  for (size_t char_class = 0; char_class < this->num_char_classes(); char_class++) {
    base_stats_json.emplace_back(this->base_stats_for_class(char_class).json());
    max_stats_json.emplace_back(this->max_stats_for_class(char_class).json());
    auto this_class_level_deltas_json = phosg::JSON::list();
    for (size_t level = 0; level < 200; level++) {
      this_class_level_deltas_json.emplace_back(this->stats_delta_for_level(char_class, level).json());
    }
    level_deltas_json.emplace_back(std::move(this_class_level_deltas_json));
  }
  return phosg::JSON::dict({
      {"BaseStats", std::move(base_stats_json)},
      {"MaxStats", std::move(max_stats_json)},
      {"LevelDeltas", std::move(level_deltas_json)},
  });
}

JSONLevelTable::JSONLevelTable(const phosg::JSON& json) {
  const auto& base_stats_json = json.at("BaseStats").as_list();
  const auto& max_stats_json = json.at("MaxStats").as_list();
  const auto& level_deltas_json = json.at("LevelDeltas").as_list();
  for (size_t char_class = 0; char_class < base_stats_json.size(); char_class++) {
    this->base_stats.emplace_back(CharacterStats::from_json(*base_stats_json.at(char_class)));
    this->max_stats.emplace_back(PlayerStats::from_json(*max_stats_json.at(char_class)));
    const auto& this_class_level_deltas_json = level_deltas_json.at(char_class)->as_list();
    auto& parsed_deltas = this->level_deltas.emplace_back();
    for (size_t level = 0; level < 200; level++) {
      parsed_deltas[level] = LevelStatsDelta::from_json(*this_class_level_deltas_json.at(level));
    }
  }
}

size_t JSONLevelTable::num_char_classes() const {
  return this->base_stats.size();
}

const CharacterStats& JSONLevelTable::base_stats_for_class(uint8_t char_class) const {
  return this->base_stats.at(char_class);
}

const PlayerStats& JSONLevelTable::max_stats_for_class(uint8_t char_class) const {
  return this->max_stats.at(char_class);
}

const LevelStatsDelta& JSONLevelTable::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

LevelTableV2::LevelTableV2(const string& data) {
  struct Root {
    // The overall format of this file on V2 has much more data than we actually use. This table is sorted by the
    // offset in the PlayerTable.prs file; note that the offset fields in this structure do not match that order.
    // ## OFFS WHAT -> TARGET
    //    0008 level_deltas[0] -> LevelStatsDelta[200] (by level) (the rest follow immediately)
    // 00 5468 level_deltas -> u32[9] (by char_class)
    // 04 548C hp_tp_factors -> HPTPFactors[3] (by char_class_class; [0] = hunter, [1] = ranger, [2] = force)
    // 08 54A4 max_stats -> PlayerStats[9] (by char_class)
    // 0C 55E8 level_100_stats -> PlayerStats[9] (by char_class)
    //    572C base_stats[0] -> CharacterStats
    // 10 57AC base_stats -> u32[9] (by char_class)
    // 14 57D0 resist_data -> ResistData[9] (by char_class)
    // 18 58F0 attack_data -> AttackData[9] (by char_class)
    // 1C 5AA0 unknown_a4 -> float[15][3] (by [???][attack_number])
    // 20 5B54 unknown_a5 -> float[3][3] (by [strike_number][attack_number])
    // 24 5B78 unknown_a6 -> float[3][3] (by [strike_number][attack_number]) (may be [4][3] in original code; there are 0xC zero bytes after)
    // 28 5BA8 unknown_a7 -> uint8_t[15][3] (same indexes as unknown_a4)
    //    5BD8 unknown_a9[0] -> UnknownA9[15] (index unknown; appears animation-related)
    // 30 5DF4 unknown_a9 -> u32[3] (by char_class_class)
    // 2C 5E00 area_sound_configs -> AreaSoundConfig[0x12] (by area)
    //    5E90 unknown_a10[0] -> (0x10-byte struct)[0x0C] (the rest follow immediately)
    // 34 60D0 unknown_a10 -> u32[3] (by char_class_class)
    //    60DC unknown_a11[0] -> WeaponReference[12] (the rest follow immediately)
    // 38 616C unknown_a11 -> u32[3] (by char_class_class)
    //    6178 unknown_a12[0] -> UnknownA12[15] (the rest follow immediately)
    // 3C 64FC unknown_a12 -> u32[3] (by char_class_class)

    /* 00 / 5468 * */ le_uint32_t level_deltas;
    /* 04 / 548C * */ le_uint32_t hp_tp_factors;
    /* 08 / 54A4 * */ le_uint32_t max_stats;
    /* 0C / 55E8 * */ le_uint32_t level_100_stats;
    /* 10 / 57AC * */ le_uint32_t base_stats;
    /* 14 / 57D0 * */ le_uint32_t resist_data;
    /* 18 / 58F0 * */ le_uint32_t attack_data;
    /* 1C / 5AA0 * */ le_uint32_t unknown_a4;
    /* 20 / 5B54 * */ le_uint32_t unknown_a5;
    /* 24 / 5B78 * */ le_uint32_t unknown_a6;
    /* 28 / 5BA8 * */ le_uint32_t unknown_a7;
    /* 2C / 5E00 * */ le_uint32_t area_sound_configs;
    /* 30 / 5DF4 * */ le_uint32_t unknown_a9;
    /* 34 / 60D0 * */ le_uint32_t unknown_a10;
    /* 38 / 616C * */ le_uint32_t unknown_a11;
    /* 3C / 64FC * */ le_uint32_t unknown_a12;
  } __packed_ws__(Root, 0x40);

  phosg::StringReader r(data);

  const auto& footer = r.pget<RELFileFooter>(r.size() - sizeof(RELFileFooter));
  const auto& offsets = r.pget<Root>(footer.root_offset);
  const auto& level_deltas_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.level_deltas);
  const auto& base_stats_offsets = r.pget<parray<le_uint32_t, 9>>(offsets.base_stats);
  for (size_t char_class = 0; char_class < 9; char_class++) {
    const auto& src_level_deltas = r.pget<parray<LevelStatsDelta, 200>>(level_deltas_offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      this->level_deltas[char_class][level] = src_level_deltas[level];
    }
    this->max_stats[char_class] = r.pget<PlayerStats>(offsets.max_stats + char_class * sizeof(PlayerStats));
    this->base_stats[char_class] = r.pget<CharacterStats>(base_stats_offsets[char_class]);
  }
}

size_t LevelTableV2::num_char_classes() const {
  return 9;
}

const CharacterStats& LevelTableV2::base_stats_for_class(uint8_t char_class) const {
  return this->base_stats.at(char_class);
}

const PlayerStats& LevelTableV2::max_stats_for_class(uint8_t char_class) const {
  return this->max_stats.at(char_class);
}

const LevelStatsDelta& LevelTableV2::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

size_t LevelTableV3::num_char_classes() const {
  return 12;
}

const CharacterStats& LevelTableV3::base_stats_for_class(uint8_t char_class) const {
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

const PlayerStats& LevelTableV3::max_stats_for_class(uint8_t char_class) const {
  return max_stats_v3_v4.at(char_class);
}

const LevelStatsDelta& LevelTableV3::stats_delta_for_level(uint8_t char_class, uint8_t level) const {
  return this->level_deltas.at(char_class).at(level);
}

template <bool BE>
void parse_level_deltas_t(std::array<std::array<LevelStatsDelta, 200>, 12>& deltas, const string& data) {
  // The V3 format is very simple:
  //   root:
  //     u32 offset:
  //       u32[12] offsets:
  //         LevelStatsDeltaBE[200] level_deltas
  phosg::StringReader r(data);
  const auto& footer = r.pget<RELFileFooterT<BE>>(r.size() - sizeof(RELFileFooterT<BE>));
  const auto& offsets = r.pget<parray<U32T<BE>, 12>>(r.pget<U32T<BE>>(footer.root_offset));
  for (size_t char_class = 0; char_class < 12; char_class++) {
    const auto& src_deltas = r.pget<parray<LevelStatsDeltaT<BE>, 200>>(offsets[char_class]);
    for (size_t level = 0; level < 200; level++) {
      deltas[char_class][level] = src_deltas[level];
    }
  }
}

LevelTableGC::LevelTableGC(const string& data) {
  parse_level_deltas_t<true>(this->level_deltas, data);
}

LevelTableXB::LevelTableXB(const string& data) {
  parse_level_deltas_t<false>(this->level_deltas, data);
}

struct RootV4 {
  le_uint32_t base_stats; // -> u32[12] -> CharacterStats
  le_uint32_t level_deltas; // -> u32[12] -> LevelStatsDelta[200]
} __packed_ws__(RootV4, 8);

std::string LevelTable::serialize_binary_v4() const {
  RELFileWriter<false> rel;
  RootV4 root;

  {
    std::vector<uint32_t> offsets;
    for (size_t char_class = 0; char_class < this->num_char_classes(); char_class++) {
      offsets.emplace_back(rel.put<CharacterStats>(this->base_stats_for_class(char_class)));
    }
    root.base_stats = rel.w.size();
    for (uint32_t offset : offsets) {
      rel.write_offset(offset);
    }
  }

  {
    std::vector<uint32_t> offsets;
    for (size_t char_class = 0; char_class < this->num_char_classes(); char_class++) {
      offsets.emplace_back(rel.w.size());
      for (size_t level = 0; level < 200; level++) {
        rel.put<LevelStatsDelta>(this->stats_delta_for_level(char_class, level));
      }
    }
    root.level_deltas = rel.w.size();
    for (uint32_t offset : offsets) {
      rel.write_offset(offset);
    }
  }

  size_t root_offset = rel.put<RootV4>(root);
  rel.relocations.emplace(root_offset);
  rel.relocations.emplace(root_offset + 4);

  return rel.finalize(root_offset);
}

LevelTableV4::LevelTableV4(const string& data) {

  phosg::StringReader r(data);
  const auto& footer = r.pget<RELFileFooter>(r.size() - sizeof(RELFileFooter));
  const auto& offsets = r.pget<RootV4>(footer.root_offset);
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

size_t LevelTableV4::num_char_classes() const {
  return 12;
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
