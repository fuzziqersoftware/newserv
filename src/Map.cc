#include "Map.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "Quest.hh"
#include "StaticGameData.hh"

using namespace std;

Map::Enemy::Enemy(EnemyType type)
    : type(type),
      flags(0),
      last_hit_by_client_id(0) {}

string Map::Enemy::str() const {
  return string_printf("[Map::Enemy %s flags=%02hhX last_hit_by_client_id=%hu]",
      name_for_enum(this->type), this->flags, this->last_hit_by_client_id);
}

struct EnemyEntry {
  /* 00 */ le_uint16_t base_type;
  /* 02 */ le_uint16_t unknown_a0; // Overwritten by client at load time
  /* 04 */ le_uint16_t enemy_index; // Overwritten by client at load time
  /* 06 */ le_uint16_t num_children;
  /* 08 */ le_uint16_t area;
  /* 0A */ le_uint16_t entity_id; // == enemy_index + 0x1000
  /* 0C */ le_uint16_t section;
  /* 0E */ le_uint16_t wave_number;
  /* 10 */ le_uint32_t wave_number2;
  /* 14 */ le_float x;
  /* 18 */ le_float y;
  /* 1C */ le_float z;
  /* 20 */ le_uint32_t x_angle;
  /* 24 */ le_uint32_t y_angle;
  /* 28 */ le_uint32_t z_angle;
  /* 2C */ le_uint32_t unknown_a3;
  /* 30 */ le_uint32_t unknown_a4;
  /* 34 */ le_uint32_t unknown_a5;
  /* 38 */ le_uint32_t unknown_a6;
  /* 3C */ le_uint32_t unknown_a7;
  /* 40 */ le_uint32_t skin;
  /* 44 */ le_uint32_t unknown_a8;
  /* 48 */

  string str() const {
    return string_printf("EnemyEntry(base_type=%hX, a0=%hX, enemy_index=%hX, num_children=%hX, area=%hX, entity_id=%hX, section=%hX, wave_number=%hX, wave_number2=%" PRIX32 ", x=%g, y=%g, z=%g, x_angle=%" PRIX32 ", y_angle=%" PRIX32 ", z_angle=%" PRIX32 ", a3=%" PRIX32 ", a4=%" PRIX32 ", a5=%" PRIX32 ", a6=%" PRIX32 ", a7=%" PRIX32 ", skin=%" PRIX32 ", a8=%" PRIX32 ")",
        this->base_type.load(), this->unknown_a0.load(), this->enemy_index.load(), this->num_children.load(), this->area.load(),
        this->entity_id.load(), this->section.load(), this->wave_number.load(),
        this->wave_number2.load(), this->x.load(), this->y.load(), this->z.load(), this->x_angle.load(),
        this->y_angle.load(), this->z_angle.load(), this->unknown_a3.load(), this->unknown_a4.load(),
        this->unknown_a5.load(), this->unknown_a6.load(), this->unknown_a7.load(), this->skin.load(),
        this->unknown_a8.load());
  }
} __attribute__((packed));

struct ObjectEntry {
  /* 00 */ le_uint16_t base_type;
  /* 02 */ le_uint16_t unknown_a1;
  /* 04 */ le_uint32_t unknown_a2;
  /* 08 */ le_uint16_t id;
  /* 0A */ le_uint16_t group;
  /* 0C */ le_uint16_t section;
  /* 0E */ le_uint16_t unknown_a3;
  /* 10 */ le_float x;
  /* 14 */ le_float y;
  /* 18 */ le_float z;
  /* 1C */ le_uint32_t x_angle;
  /* 20 */ le_uint32_t y_angle;
  /* 24 */ le_uint32_t z_angle;
  /* 28 */ le_uint32_t unknown_a4;
  /* 2C */ le_uint32_t unknown_a5;
  /* 30 */ le_uint32_t unknown_a6;
  /* 34 */ le_uint32_t unknown_a7;
  /* 38 */ le_uint32_t unknown_a8;
  /* 3C */ le_uint32_t unknown_a9;
  /* 40 */ le_uint32_t unknown_a10;
  /* 44 */

  string str() const {
    return string_printf("ObjectEntry(base_type=%hX, a1=%hX, a2=%" PRIX32 ", id=%hX, group=%hX, section=%hX, a3=%hX, x=%g, y=%g, z=%g, x_angle=%" PRIX32 ", y_angle=%" PRIX32 ", z_angle=%" PRIX32 ", a3=%" PRIX32 ", a4=%" PRIX32 ", a5=%" PRIX32 ", a6=%" PRIX32 ", a7=%" PRIX32 ", a8=%" PRIX32 ", a9=%" PRIX32 ")",
        this->base_type.load(), this->unknown_a1.load(), this->unknown_a2.load(), this->id.load(), this->group.load(),
        this->section.load(), this->unknown_a3.load(), this->x.load(), this->y.load(), this->z.load(), this->x_angle.load(),
        this->y_angle.load(), this->z_angle.load(), this->unknown_a3.load(), this->unknown_a4.load(),
        this->unknown_a5.load(), this->unknown_a6.load(), this->unknown_a7.load(), this->unknown_a8.load(),
        this->unknown_a9.load());
  }
} __attribute__((packed));

void Map::clear() {
  this->enemies.clear();
  this->rare_enemy_indexes.clear();
}

void Map::add_enemies_from_map_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    const void* data,
    size_t size,
    const RareEnemyRates* rare_rates) {
  static const RareEnemyRates default_rare_rates = {
      // All 1/512 except Kondrieu, which is 1/10
      .hildeblue = 0x00800000,
      .rappy = 0x00800000,
      .nar_lily = 0x00800000,
      .pouilly_slime = 0x00800000,
      .merissa_aa = 0x00800000,
      .pazuzu = 0x00800000,
      .dorphon_eclair = 0x00800000,
      .kondrieu = 0x1999999A,
  };
  if (!rare_rates) {
    rare_rates = &default_rare_rates;
  }

  auto check_rare = [&](bool default_is_rare, uint32_t rare_rate) -> bool {
    if (default_is_rare) {
      return true;
    }
    if ((this->rare_enemy_indexes.size() < 0x10) &&
        (random_object<uint32_t>() < rare_rate)) {
      this->rare_enemy_indexes.emplace_back(this->enemies.size());
      return true;
    }
    return false;
  };

  const auto* map = reinterpret_cast<const EnemyEntry*>(data);
  size_t entry_count = size / sizeof(EnemyEntry);
  if (size != entry_count * sizeof(EnemyEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  for (size_t y = 0; y < entry_count; y++) {
    const auto& e = map[y];

    string hex = format_data_string(&e, sizeof(e));
    fprintf(stderr, "[%04zX] %s\n", y, hex.c_str());

    switch (e.base_type) {
      case 0x40: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->hildeblue);
        this->enemies.emplace_back(is_rare ? EnemyType::HILDEBLUE : EnemyType::HILDEBEAR);
        break;
      }
      case 0x41: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->rappy);
        switch (episode) {
          case Episode::EP1:
            this->enemies.emplace_back(is_rare ? EnemyType::AL_RAPPY : EnemyType::RAG_RAPPY);
            break;
          case Episode::EP2:
            if (is_rare) {
              switch (event) {
                case 0x01:
                  this->enemies.emplace_back(EnemyType::SAINT_RAPPY);
                  break;
                case 0x04:
                  this->enemies.emplace_back(EnemyType::EGG_RAPPY);
                  break;
                case 0x05:
                  this->enemies.emplace_back(EnemyType::HALLO_RAPPY);
                  break;
                default:
                  this->enemies.emplace_back(EnemyType::LOVE_RAPPY);
              }
            } else {
              this->enemies.emplace_back(EnemyType::RAG_RAPPY);
            }
            break;
          case Episode::EP4:
            if (e.area > 0x05) {
              this->enemies.emplace_back(is_rare ? EnemyType::DEL_RAPPY_ALT : EnemyType::SAND_RAPPY_ALT);
            } else {
              this->enemies.emplace_back(is_rare ? EnemyType::DEL_RAPPY : EnemyType::SAND_RAPPY);
            }
            break;
          default:
            throw logic_error("invalid episode");
        }
        break;
      }
      case 0x42: {
        this->enemies.emplace_back(EnemyType::MONEST);
        for (size_t x = 0; x < 30; x++) {
          this->enemies.emplace_back(EnemyType::MOTHMANT);
        }
        break;
      }
      case 0x43: {
        this->enemies.emplace_back(e.unknown_a4 ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
        break;
      }
      case 0x44:
        static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
        this->enemies.emplace_back(types[e.skin % 3]);
        break;
      case 0x60:
        this->enemies.emplace_back(EnemyType::GRASS_ASSASSIN);
        break;
      case 0x61:
        if ((episode == Episode::EP2) && (e.area > 0x0F)) {
          this->enemies.emplace_back(EnemyType::DEL_LILY);
        } else {
          bool is_rare = check_rare(e.skin & 0x01, rare_rates->nar_lily);
          this->enemies.emplace_back(is_rare ? EnemyType::NAR_LILY : EnemyType::POISON_LILY);
        }
        break;
      case 0x62:
        this->enemies.emplace_back(EnemyType::NANO_DRAGON);
        break;
      case 0x63: {
        static const EnemyType types[3] = {EnemyType::EVIL_SHARK, EnemyType::PAL_SHARK, EnemyType::GUIL_SHARK};
        this->enemies.emplace_back(types[e.skin % 3]);
        break;
      }
      case 0x64: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->pouilly_slime);
        for (size_t x = 0; x < 5; x++) { // Main slime + 4 clones
          this->enemies.emplace_back(is_rare ? EnemyType::POFUILLY_SLIME : EnemyType::POUILLY_SLIME);
        }
        break;
      }
      case 0x65:
        this->enemies.emplace_back(EnemyType::PAN_ARMS);
        this->enemies.emplace_back(EnemyType::HIDOOM);
        this->enemies.emplace_back(EnemyType::MIGIUM);
        break;
      case 0x80:
        this->enemies.emplace_back((e.skin & 0x01) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
        break;
      case 0x81:
        this->enemies.emplace_back(EnemyType::GARANZ);
        break;
      case 0x82: {
        EnemyType type = e.unknown_a4 ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT;
        size_t count = (e.num_children == 0) ? 5 : (e.num_children + 1);
        for (size_t z = 0; z < count; z++) {
          this->enemies.emplace_back(type);
        }
        break;
      }
      case 0x83:
        this->enemies.emplace_back(EnemyType::CANADINE);
        break;
      case 0x84:
        this->enemies.emplace_back(EnemyType::CANANE);
        for (size_t x = 0; x < 8; x++) {
          this->enemies.emplace_back(EnemyType::CANADINE_GROUP);
        }
        break;
      case 0x85:
        this->enemies.emplace_back(EnemyType::DUBWITCH);
        break;
      case 0xA0:
        this->enemies.emplace_back(EnemyType::DELSABER);
        break;
      case 0xA1:
        this->enemies.emplace_back(EnemyType::CHAOS_SORCERER);
        this->enemies.emplace_back(EnemyType::BEE_R);
        this->enemies.emplace_back(EnemyType::BEE_L);
        break;
      case 0xA2:
        this->enemies.emplace_back(EnemyType::DARK_GUNNER);
        break;
      case 0xA3:
        this->enemies.emplace_back(EnemyType::DEATH_GUNNER);
        break;
      case 0xA4:
        this->enemies.emplace_back(EnemyType::CHAOS_BRINGER);
        break;
      case 0xA5:
        this->enemies.emplace_back(EnemyType::DARK_BELRA);
        break;
      case 0xA6: {
        static const EnemyType types[3] = {EnemyType::DIMENIAN, EnemyType::LA_DIMENIAN, EnemyType::SO_DIMENIAN};
        this->enemies.emplace_back(types[e.skin % 3]);
        break;
      }
      case 0xA7:
        this->enemies.emplace_back(EnemyType::BULCLAW);
        for (size_t x = 0; x < 4; x++) {
          this->enemies.emplace_back(EnemyType::CLAW);
        }
        break;
      case 0xA8:
        this->enemies.emplace_back(EnemyType::CLAW);
        break;
      case 0xC0:
        if (episode == Episode::EP1) {
          this->enemies.emplace_back(EnemyType::DRAGON);
        } else if (episode == Episode::EP2) {
          this->enemies.emplace_back(EnemyType::GAL_GRYPHON);
        } else {
          throw runtime_error("DRAGON-type enemy placed outside of Episodes 1 or 2");
        }
        break;
      case 0xC1:
        this->enemies.emplace_back(EnemyType::DE_ROL_LE);
        for (size_t z = 0; z < 0x0A; z++) {
          this->enemies.emplace_back(EnemyType::DE_ROL_LE_BODY);
        }
        for (size_t z = 0; z < 0x09; z++) {
          this->enemies.emplace_back(EnemyType::DE_ROL_LE_MINE);
        }
        break;
      case 0xC2:
        this->enemies.emplace_back(EnemyType::VOL_OPT_1);
        for (size_t z = 0; z < 6; z++) {
          this->enemies.emplace_back(EnemyType::VOL_OPT_PILLAR);
        }
        for (size_t z = 0; z < 24; z++) {
          this->enemies.emplace_back(EnemyType::VOL_OPT_MONITOR);
        }
        for (size_t z = 0; z < 2; z++) {
          this->enemies.emplace_back(EnemyType::NONE);
        }
        this->enemies.emplace_back(EnemyType::VOL_OPT_AMP);
        this->enemies.emplace_back(EnemyType::VOL_OPT_CORE);
        this->enemies.emplace_back(EnemyType::NONE);
        break;
      case 0xC5:
        this->enemies.emplace_back(EnemyType::VOL_OPT_2);
        break;
      case 0xC8:
        if (difficulty) {
          this->enemies.emplace_back(EnemyType::DARK_FALZ_3);
        } else {
          this->enemies.emplace_back(EnemyType::DARK_FALZ_2);
        }
        for (size_t x = 0; x < 0x1FD; x++) {
          this->enemies.emplace_back(difficulty == 3 ? EnemyType::DARVANT_ULTIMATE : EnemyType::DARVANT);
        }
        this->enemies.emplace_back(EnemyType::DARK_FALZ_3);
        this->enemies.emplace_back(EnemyType::DARK_FALZ_2);
        this->enemies.emplace_back(EnemyType::DARK_FALZ_1);
        break;
      case 0xCA:
        for (size_t z = 0; z < 0x201; z++) {
          this->enemies.emplace_back(EnemyType::OLGA_FLOW_2);
        }
        break;
      case 0xCB:
        this->enemies.emplace_back(EnemyType::BARBA_RAY);
        for (size_t z = 0; z < 0x2F; z++) {
          this->enemies.emplace_back(EnemyType::PIG_RAY);
        }
        break;
      case 0xCC:
        for (size_t z = 0; z < 6; z++) {
          this->enemies.emplace_back(EnemyType::GOL_DRAGON);
        }
        break;
      case 0xD4: {
        EnemyType type = (e.skin & 1) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL;
        for (size_t z = 0; z < 5; z++) {
          this->enemies.emplace_back(type);
        }
        break;
      }
      case 0xD5:
        this->enemies.emplace_back((e.skin & 0x01) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
        break;
      case 0xD6:
        if (e.skin == 0) {
          this->enemies.emplace_back(EnemyType::MERICAROL);
        } else {
          this->enemies.emplace_back(((e.skin % 3) == 2) ? EnemyType::MERICUS : EnemyType::MERIKLE);
        }
        break;
      case 0xD7:
        this->enemies.emplace_back((e.skin & 0x01) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
        break;
      case 0xD8:
        this->enemies.emplace_back(EnemyType::GIBBLES);
        break;
      case 0xD9:
        this->enemies.emplace_back(EnemyType::GEE);
        break;
      case 0xDA:
        this->enemies.emplace_back(EnemyType::GI_GUE);
        break;
      case 0xDB:
        this->enemies.emplace_back(EnemyType::DELDEPTH);
        break;
      case 0xDC:
        this->enemies.emplace_back(EnemyType::DELBITER);
        break;
      case 0xDD:
        this->enemies.emplace_back((e.skin & 0x01) ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
        break;
      case 0xDE:
        this->enemies.emplace_back(EnemyType::MORFOS);
        break;
      case 0xDF:
        this->enemies.emplace_back(EnemyType::RECOBOX);
        for (size_t x = 0; x < e.num_children; x++) {
          this->enemies.emplace_back(EnemyType::RECON);
        }
        break;
      case 0xE0:
        if ((episode == Episode::EP2) && (e.area > 0x0F)) {
          this->enemies.emplace_back(EnemyType::EPSILON);
          for (size_t z = 0; z < 4; z++) {
            this->enemies.emplace_back(EnemyType::EPSIGUARD);
          }
        } else {
          this->enemies.emplace_back((e.skin & 0x01) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
        }
        break;
      case 0xE1:
        this->enemies.emplace_back(EnemyType::ILL_GILL);
        break;
      case 0x0110:
        this->enemies.emplace_back(EnemyType::ASTARK);
        break;
      case 0x0111:
        if (e.area > 0x05) {
          this->enemies.emplace_back(e.unknown_a4 ? EnemyType::YOWIE_ALT : EnemyType::SATELLITE_LIZARD_ALT);
        } else {
          this->enemies.emplace_back(e.unknown_a4 ? EnemyType::YOWIE : EnemyType::SATELLITE_LIZARD);
        }
        break;
      case 0x0112: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->merissa_aa);
        this->enemies.emplace_back(is_rare ? EnemyType::MERISSA_AA : EnemyType::MERISSA_A);
        break;
      }
      case 0x0113:
        this->enemies.emplace_back(EnemyType::GIRTABLULU);
        break;
      case 0x0114: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->pazuzu);
        if (e.area > 0x05) {
          this->enemies.emplace_back(is_rare ? EnemyType::PAZUZU_ALT : EnemyType::ZU_ALT);
        } else {
          this->enemies.emplace_back(is_rare ? EnemyType::PAZUZU : EnemyType::ZU);
        }
        break;
      }
      case 0x0115:
        if (e.skin & 2) {
          this->enemies.emplace_back(EnemyType::BA_BOOTA);
        } else {
          this->enemies.emplace_back((e.skin & 1) ? EnemyType::ZE_BOOTA : EnemyType::BOOTA);
        }
        break;
      case 0x0116: {
        bool is_rare = check_rare(e.skin & 0x01, rare_rates->dorphon_eclair);
        this->enemies.emplace_back(is_rare ? EnemyType::DORPHON_ECLAIR : EnemyType::DORPHON);
        break;
      }
      case 0x0117: {
        static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
        this->enemies.emplace_back(types[e.skin % 3]);
        break;
      }
      case 0x0119: {
        bool is_rare = check_rare((e.unknown_a4 != 0), rare_rates->kondrieu);
        if (is_rare) {
          this->enemies.emplace_back(EnemyType::KONDRIEU);
        } else {
          this->enemies.emplace_back((e.skin & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILLION);
        }
        break;
      }
      default:
        for (size_t z = 0; z < static_cast<size_t>(e.num_children + 1); z++) {
          this->enemies.emplace_back(EnemyType::UNKNOWN);
        }
        static_game_data_log.warning(
            "(Entry %zu, offset %zX in file) Unknown enemy type %04hX",
            y, y * sizeof(EnemyEntry), e.base_type.load());
        break;
    }
  }
}

void Map::add_enemies_from_quest_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    const void* data,
    size_t size) {
  StringReader r(data, size);
  while (!r.eof()) {
    const auto& header = r.get<Quest::DATSectionHeader>();
    if (header.type == 0 && header.section_size == 0) {
      break;
    }
    if (header.section_size < sizeof(header)) {
      throw runtime_error(string_printf("quest layout has invalid section header at offset 0x%zX", r.where() - sizeof(header)));
    }
    if (header.type == 2) {
      if (header.data_size % sizeof(EnemyEntry)) {
        throw runtime_error("quest layout enemy section size is not a multiple of enemy entry size");
      }
      this->add_enemies_from_map_data(episode, difficulty, event, r.getv(header.data_size), header.data_size);
    } else {
      r.skip(header.section_size - sizeof(header));
    }
  }
}

SetDataTable::SetDataTable(shared_ptr<const string> data, bool big_endian) {
  if (big_endian) {
    this->load_table_t<true>(data);
  } else {
    this->load_table_t<false>(data);
  }
}

template <bool IsBigEndian>
void SetDataTable::load_table_t(shared_ptr<const string> data) {
  using U32T = typename conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  StringReader r(*data);

  struct Footer {
    U32T table3_offset;
    U32T table3_count; // In le_uint16_ts (so *2 for size in bytes)
    U32T unknown_a3; // == 1
    U32T unknown_a4; // == 0
    U32T root_table_offset_offset;
    U32T unknown_a6; // == 0
    U32T unknown_a7; // == 0
    U32T unknown_a8; // == 0
  } __attribute__((packed));
  if (r.size() < sizeof(Footer)) {
    throw runtime_error("set data table is too small");
  }
  auto& footer = r.pget<Footer>(r.size() - sizeof(Footer));

  uint32_t root_table_offset = r.pget<U32T>(footer.root_table_offset_offset);
  auto root_r = r.sub(root_table_offset, footer.root_table_offset_offset - root_table_offset);
  while (!root_r.eof()) {
    auto& var1_v = this->entries.emplace_back();
    uint32_t var1_table_offset = root_r.template get<U32T>();
    uint32_t var1_table_count = root_r.template get<U32T>();
    auto var1_r = r.sub(var1_table_offset, var1_table_count * 0x08);
    while (!var1_r.eof()) {
      auto& var2_v = var1_v.emplace_back();
      uint32_t var2_table_offset = var1_r.get<U32T>();
      uint32_t var2_table_count = var1_r.get<U32T>();
      auto var2_r = r.sub(var2_table_offset, var2_table_count * 0x0C);
      while (!var2_r.eof()) {
        auto& entry = var2_v.emplace_back();
        entry.name1 = r.pget_cstr(var2_r.get<U32T>());
        entry.enemy_list_basename = r.pget_cstr(var2_r.get<U32T>());
        entry.name3 = r.pget_cstr(var2_r.get<U32T>());
      }
    }
  }
}

void SetDataTable::print(FILE* stream) const {
  for (size_t a = 0; a < this->entries.size(); a++) {
    const auto& v1_v = this->entries[a];
    for (size_t v1 = 0; v1 < v1_v.size(); v1++) {
      const auto& v2_v = v1_v[v1];
      for (size_t v2 = 0; v2 < v2_v.size(); v2++) {
        const auto& e = v2_v[v2];
        fprintf(stream, "[%02zX/%02zX/%02zX] %s %s %s\n", a, v1, v2, e.name1.c_str(), e.enemy_list_basename.c_str(), e.name3.c_str());
      }
    }
  }
}

struct AreaMapFileIndex {
  const char* name_token;
  vector<uint32_t> variation1_values;
  vector<uint32_t> variation2_values;

  AreaMapFileIndex(
      const char* name_token,
      vector<uint32_t> variation1_values,
      vector<uint32_t> variation2_values)
      : name_token(name_token),
        variation1_values(variation1_values),
        variation2_values(variation2_values) {}
};

// These are indexed as [episode][is_solo][area], where episode is 0-2
static const vector<vector<vector<AreaMapFileIndex>>> map_file_info = {
    {
        // Episode 1
        {
            // Non-solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 1, 2, 3, 4}},
            {"forest02", {}, {0, 1, 2, 3, 4}},
            {"cave01", {0, 1, 2}, {0, 1}},
            {"cave02", {0, 1, 2}, {0, 1}},
            {"cave03", {0, 1, 2}, {0, 1}},
            {"machine01", {0, 1, 2}, {0, 1}},
            {"machine02", {0, 1, 2}, {0, 1}},
            {"ancient01", {0, 1, 2}, {0, 1}},
            {"ancient02", {0, 1, 2}, {0, 1}},
            {"ancient03", {0, 1, 2}, {0, 1}},
            {"boss01", {}, {}},
            {"boss02", {}, {}},
            {"boss03", {}, {}},
            {"boss04", {}, {}},
            {nullptr, {}, {}},
        },
        {
            // Solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 2, 4}},
            {"forest02", {}, {0, 3, 4}},
            {"cave01", {0, 1, 2}, {0}},
            {"cave02", {0, 1, 2}, {0}},
            {"cave03", {0, 1, 2}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
    {
        // Episode 2
        {
            // Non-solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0, 1}},
            {"seabed02", {0, 1}, {0, 1}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
        {
            // Solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0}},
            {"seabed02", {0, 1}, {0}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
    },
    {
        // Episode 4
        {
            // Non-solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
        {
            // Solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
};

const vector<vector<AreaMapFileIndex>>& map_file_info_for_episode(Episode ep) {
  switch (ep) {
    case Episode::EP1:
      return map_file_info.at(0);
    case Episode::EP2:
      return map_file_info.at(1);
    case Episode::EP4:
      return map_file_info.at(2);
    default:
      throw invalid_argument("episode has no maps");
  }
}

void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    shared_ptr<PSOLFGEncryption> random_crypt,
    Episode episode,
    bool is_solo) {
  const auto& ep_index = map_file_info_for_episode(episode);
  for (size_t z = 0; z < 0x10; z++) {
    const AreaMapFileIndex* a = nullptr;
    if (is_solo) {
      a = &ep_index.at(true).at(z);
    }
    if (!a || !a->name_token) {
      a = &ep_index.at(false).at(z);
    }
    if (!a->name_token) {
      variations[z * 2 + 0] = 0;
      variations[z * 2 + 1] = 0;
    } else {
      variations[z * 2 + 0] = (a->variation1_values.size() < 2) ? 0 : (random_crypt->next() % a->variation1_values.size());
      variations[z * 2 + 1] = (a->variation2_values.size() < 2) ? 0 : (random_crypt->next() % a->variation2_values.size());
    }
  }
}

vector<string> map_filenames_for_variation(
    Episode episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2) {
  // Map filenames are like map_<name_token>[_VV][_VV][_off]<e|o>[_s].dat
  //   name_token comes from AreaMapFileIndex
  //   _VV are the values from the variation<1|2>_values vector (in contrast to
  //     the values sent in the 64 command, which are INDEXES INTO THAT VECTOR)
  //   _off or _s are used for solo mode (try both - city uses _s whereas levels
  //     use _off apparently)
  //   e|o specifies what kind of data: e = enemies, o = objects
  const auto& ep_index = map_file_info_for_episode(episode);
  const AreaMapFileIndex* a = nullptr;
  if (is_solo) {
    a = &ep_index.at(true).at(area);
  }
  if (!a || !a->name_token) {
    a = &ep_index.at(false).at(area);
  }
  if (!a->name_token) {
    return vector<string>();
  }

  string filename = "map_";
  filename += a->name_token;
  if (!a->variation1_values.empty()) {
    filename += string_printf("_%02" PRIX32, a->variation1_values.at(var1));
  }
  if (!a->variation2_values.empty()) {
    filename += string_printf("_%02" PRIX32, a->variation2_values.at(var2));
  }

  vector<string> ret;
  if (is_solo) {
    // Try both _offe.dat and e_s.dat suffixes
    ret.emplace_back(filename + "_offe.dat");
    ret.emplace_back(filename + "e_s.dat");
  } else {
    ret.emplace_back(filename + "e.dat");
  }
  return ret;
}
