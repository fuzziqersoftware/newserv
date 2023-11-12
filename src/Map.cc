#include "Map.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "Quest.hh"
#include "StaticGameData.hh"

using namespace std;

static constexpr float UINT32_MAX_AS_FLOAT = 4294967296.0f;

Map::Enemy::Enemy(EnemyType type)
    : type(type),
      flags(0),
      last_hit_by_client_id(0) {
}

string Map::Enemy::str() const {
  return string_printf("[Map::Enemy %s flags=%02hhX last_hit_by_client_id=%hu]",
      name_for_enum(this->type), this->flags, this->last_hit_by_client_id);
}

void Map::clear() {
  this->enemies.clear();
  this->rare_enemy_indexes.clear();
}

void Map::add_objects_from_map_data(const void* data, size_t size) {
  size_t entry_count = size / sizeof(ObjectEntry);
  if (size != entry_count * sizeof(ObjectEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  (void)data;
  // TODO: Actually track objects, so we can e.g. know what to drop from fixed
  // boxes
  // const auto* map = reinterpret_cast<const ObjectEntry*>(data);
}

bool Map::check_and_log_rare_enemy(bool default_is_rare, uint32_t rare_rate) {
  if (default_is_rare) {
    return true;
  }
  if ((this->rare_enemy_indexes.size() < 0x10) && (random_object<uint32_t>() < rare_rate)) {
    this->rare_enemy_indexes.emplace_back(this->enemies.size());
    return true;
  }
  return false;
}

void Map::add_enemy(EnemyType type) {
  static_game_data_log.info("Adding enemy E-%zX => %s", this->enemies.size(), name_for_enum(type));
  this->enemies.emplace_back(type);
}

void Map::add_enemy(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    size_t index,
    const EnemyEntry& e,
    const RareEnemyRates& rare_rates) {
  switch (e.base_type) {
    case 0x40: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.hildeblue);
      this->add_enemy(is_rare ? EnemyType::HILDEBLUE : EnemyType::HILDEBEAR);
      break;
    }
    case 0x41: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.rappy);
      switch (episode) {
        case Episode::EP1:
          this->add_enemy(is_rare ? EnemyType::AL_RAPPY : EnemyType::RAG_RAPPY);
          break;
        case Episode::EP2:
          if (is_rare) {
            switch (event) {
              case 0x01:
                this->add_enemy(EnemyType::SAINT_RAPPY);
                break;
              case 0x04:
                this->add_enemy(EnemyType::EGG_RAPPY);
                break;
              case 0x05:
                this->add_enemy(EnemyType::HALLO_RAPPY);
                break;
              default:
                this->add_enemy(EnemyType::LOVE_RAPPY);
            }
          } else {
            this->add_enemy(EnemyType::RAG_RAPPY);
          }
          break;
        case Episode::EP4:
          if (e.floor > 0x05) {
            this->add_enemy(is_rare ? EnemyType::DEL_RAPPY_ALT : EnemyType::SAND_RAPPY_ALT);
          } else {
            this->add_enemy(is_rare ? EnemyType::DEL_RAPPY : EnemyType::SAND_RAPPY);
          }
          break;
        default:
          throw logic_error("invalid episode");
      }
      break;
    }
    case 0x42: {
      this->add_enemy(EnemyType::MONEST);
      for (size_t x = 0; x < 30; x++) {
        this->add_enemy(EnemyType::MOTHMANT);
      }
      break;
    }
    case 0x43: {
      this->add_enemy(e.fparam2 ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
      break;
    }
    case 0x44:
      static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
      this->add_enemy(types[e.uparam1 % 3]);
      break;
    case 0x60:
      this->add_enemy(EnemyType::GRASS_ASSASSIN);
      break;
    case 0x61:
      if ((episode == Episode::EP2) && (e.floor > 0x0F)) {
        this->add_enemy(EnemyType::DEL_LILY);
      } else {
        bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.nar_lily);
        this->add_enemy(is_rare ? EnemyType::NAR_LILY : EnemyType::POISON_LILY);
      }
      break;
    case 0x62:
      this->add_enemy(EnemyType::NANO_DRAGON);
      break;
    case 0x63: {
      static const EnemyType types[3] = {EnemyType::EVIL_SHARK, EnemyType::PAL_SHARK, EnemyType::GUIL_SHARK};
      this->add_enemy(types[e.uparam1 % 3]);
      break;
    }
    case 0x64: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.pouilly_slime);
      for (size_t x = 0; x < 5; x++) { // Main slime + 4 clones
        this->add_enemy(is_rare ? EnemyType::POFUILLY_SLIME : EnemyType::POUILLY_SLIME);
      }
      break;
    }
    case 0x65:
      this->add_enemy(EnemyType::PAN_ARMS);
      this->add_enemy(EnemyType::HIDOOM);
      this->add_enemy(EnemyType::MIGIUM);
      break;
    case 0x80:
      this->add_enemy((e.uparam1 & 0x01) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
      break;
    case 0x81:
      this->add_enemy(EnemyType::GARANZ);
      break;
    case 0x82: {
      EnemyType type = e.fparam2 ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT;
      size_t count = (e.num_children == 0) ? 5 : (e.num_children + 1);
      for (size_t z = 0; z < count; z++) {
        this->add_enemy(type);
      }
      break;
    }
    case 0x83:
      this->add_enemy(EnemyType::CANADINE);
      break;
    case 0x84:
      this->add_enemy(EnemyType::CANANE);
      for (size_t x = 0; x < 8; x++) {
        this->add_enemy(EnemyType::CANADINE_GROUP);
      }
      break;
    case 0x85:
      this->add_enemy(EnemyType::DUBWITCH);
      break;
    case 0xA0:
      this->add_enemy(EnemyType::DELSABER);
      break;
    case 0xA1:
      this->add_enemy(EnemyType::CHAOS_SORCERER);
      this->add_enemy(EnemyType::BEE_R);
      this->add_enemy(EnemyType::BEE_L);
      break;
    case 0xA2:
      this->add_enemy(EnemyType::DARK_GUNNER);
      break;
    case 0xA3:
      this->add_enemy(EnemyType::DEATH_GUNNER);
      break;
    case 0xA4:
      this->add_enemy(EnemyType::CHAOS_BRINGER);
      break;
    case 0xA5:
      this->add_enemy(EnemyType::DARK_BELRA);
      break;
    case 0xA6: {
      static const EnemyType types[3] = {EnemyType::DIMENIAN, EnemyType::LA_DIMENIAN, EnemyType::SO_DIMENIAN};
      this->add_enemy(types[e.uparam1 % 3]);
      break;
    }
    case 0xA7:
      this->add_enemy(EnemyType::BULCLAW);
      for (size_t x = 0; x < 4; x++) {
        this->add_enemy(EnemyType::CLAW);
      }
      break;
    case 0xA8:
      this->add_enemy(EnemyType::CLAW);
      break;
    case 0xC0:
      if (episode == Episode::EP1) {
        this->add_enemy(EnemyType::DRAGON);
      } else if (episode == Episode::EP2) {
        this->add_enemy(EnemyType::GAL_GRYPHON);
      } else {
        throw runtime_error("DRAGON-type enemy placed outside of Episodes 1 or 2");
      }
      break;
    case 0xC1:
      this->add_enemy(EnemyType::DE_ROL_LE);
      for (size_t z = 0; z < 0x0A; z++) {
        this->add_enemy(EnemyType::DE_ROL_LE_BODY);
      }
      for (size_t z = 0; z < 0x09; z++) {
        this->add_enemy(EnemyType::DE_ROL_LE_MINE);
      }
      break;
    case 0xC2:
      this->add_enemy(EnemyType::VOL_OPT_1);
      for (size_t z = 0; z < 6; z++) {
        this->add_enemy(EnemyType::VOL_OPT_PILLAR);
      }
      for (size_t z = 0; z < 24; z++) {
        this->add_enemy(EnemyType::VOL_OPT_MONITOR);
      }
      for (size_t z = 0; z < 2; z++) {
        this->add_enemy(EnemyType::NONE);
      }
      this->add_enemy(EnemyType::VOL_OPT_AMP);
      this->add_enemy(EnemyType::VOL_OPT_CORE);
      this->add_enemy(EnemyType::NONE);
      break;
    case 0xC5:
      this->add_enemy(EnemyType::VOL_OPT_2);
      break;
    case 0xC8:
      if (difficulty) {
        this->add_enemy(EnemyType::DARK_FALZ_3);
      } else {
        this->add_enemy(EnemyType::DARK_FALZ_2);
      }
      for (size_t x = 0; x < 0x1FD; x++) {
        this->add_enemy(difficulty == 3 ? EnemyType::DARVANT_ULTIMATE : EnemyType::DARVANT);
      }
      this->add_enemy(EnemyType::DARK_FALZ_3);
      this->add_enemy(EnemyType::DARK_FALZ_2);
      this->add_enemy(EnemyType::DARK_FALZ_1);
      break;
    case 0xCA:
      for (size_t z = 0; z < 0x201; z++) {
        this->add_enemy(EnemyType::OLGA_FLOW_2);
      }
      break;
    case 0xCB:
      this->add_enemy(EnemyType::BARBA_RAY);
      for (size_t z = 0; z < 0x2F; z++) {
        this->add_enemy(EnemyType::PIG_RAY);
      }
      break;
    case 0xCC:
      for (size_t z = 0; z < 6; z++) {
        this->add_enemy(EnemyType::GOL_DRAGON);
      }
      break;
    case 0xD4: {
      EnemyType type = (e.uparam1 & 1) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL;
      for (size_t z = 0; z < 5; z++) {
        this->add_enemy(type);
      }
      break;
    }
    case 0xD5:
      this->add_enemy((e.uparam1 & 0x01) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      break;
    case 0xD6:
      if (e.uparam1 == 0) {
        this->add_enemy(EnemyType::MERICAROL);
      } else {
        this->add_enemy(((e.uparam1 % 3) == 2) ? EnemyType::MERICUS : EnemyType::MERIKLE);
      }
      break;
    case 0xD7:
      this->add_enemy((e.uparam1 & 0x01) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
      break;
    case 0xD8:
      this->add_enemy(EnemyType::GIBBLES);
      break;
    case 0xD9:
      this->add_enemy(EnemyType::GEE);
      break;
    case 0xDA:
      this->add_enemy(EnemyType::GI_GUE);
      break;
    case 0xDB:
      this->add_enemy(EnemyType::DELDEPTH);
      break;
    case 0xDC:
      this->add_enemy(EnemyType::DELBITER);
      break;
    case 0xDD:
      this->add_enemy((e.uparam1 & 0x01) ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
      break;
    case 0xDE:
      this->add_enemy(EnemyType::MORFOS);
      break;
    case 0xDF:
      this->add_enemy(EnemyType::RECOBOX);
      for (size_t x = 0; x < e.num_children; x++) {
        this->add_enemy(EnemyType::RECON);
      }
      break;
    case 0xE0:
      if ((episode == Episode::EP2) && (e.floor > 0x0F)) {
        this->add_enemy(EnemyType::EPSILON);
        for (size_t z = 0; z < 4; z++) {
          this->add_enemy(EnemyType::EPSIGUARD);
        }
      } else {
        this->add_enemy((e.uparam1 & 0x01) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
      }
      break;
    case 0xE1:
      this->add_enemy(EnemyType::ILL_GILL);
      break;
    case 0x0110:
      this->add_enemy(EnemyType::ASTARK);
      break;
    case 0x0111:
      if (e.floor > 0x05) {
        this->add_enemy(e.fparam2 ? EnemyType::YOWIE_ALT : EnemyType::SATELLITE_LIZARD_ALT);
      } else {
        this->add_enemy(e.fparam2 ? EnemyType::YOWIE : EnemyType::SATELLITE_LIZARD);
      }
      break;
    case 0x0112: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.merissa_aa);
      this->add_enemy(is_rare ? EnemyType::MERISSA_AA : EnemyType::MERISSA_A);
      break;
    }
    case 0x0113:
      this->add_enemy(EnemyType::GIRTABLULU);
      break;
    case 0x0114: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.pazuzu);
      if (e.floor > 0x05) {
        this->add_enemy(is_rare ? EnemyType::PAZUZU_ALT : EnemyType::ZU_ALT);
      } else {
        this->add_enemy(is_rare ? EnemyType::PAZUZU : EnemyType::ZU);
      }
      break;
    }
    case 0x0115:
      if (e.uparam1 & 2) {
        this->add_enemy(EnemyType::BA_BOOTA);
      } else {
        this->add_enemy((e.uparam1 & 1) ? EnemyType::ZE_BOOTA : EnemyType::BOOTA);
      }
      break;
    case 0x0116: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates.dorphon_eclair);
      this->add_enemy(is_rare ? EnemyType::DORPHON_ECLAIR : EnemyType::DORPHON);
      break;
    }
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      this->add_enemy(types[e.uparam1 % 3]);
      break;
    }
    case 0x0119: {
      bool is_rare = this->check_and_log_rare_enemy((e.fparam2 != 0.0f), rare_rates.kondrieu);
      if (is_rare) {
        this->add_enemy(EnemyType::KONDRIEU);
      } else {
        this->add_enemy((e.uparam1 & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILLION);
      }
      break;
    }
    default:
      for (size_t z = 0; z < static_cast<size_t>(e.num_children + 1); z++) {
        this->add_enemy(EnemyType::UNKNOWN);
      }
      static_game_data_log.warning(
          "(Entry %zu, offset %zX in file) Unknown enemy type %04hX",
          index, index * sizeof(EnemyEntry), e.base_type.load());
      break;
  }
}

void Map::add_enemies_from_map_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    const void* data,
    size_t size,
    const RareEnemyRates& rare_rates) {
  size_t entry_count = size / sizeof(EnemyEntry);
  if (size != entry_count * sizeof(EnemyEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  StringReader r(data, size);
  for (size_t y = 0; y < entry_count; y++) {
    this->add_enemy(episode, difficulty, event, y, r.get<EnemyEntry>(), rare_rates);
  }
}

struct DATParserRandomState {
  PSOV2Encryption global_random_crypt;
  PSOV2Encryption local_random_crypt;
  std::array<uint32_t, 0x20> location_index_table;
  uint32_t location_indexes_populated;
  uint32_t location_indexes_used;
  uint32_t location_entries_base_offset;

  DATParserRandomState(uint32_t rare_seed)
      : global_random_crypt(rare_seed),
        local_random_crypt(0),
        location_indexes_populated(0),
        location_indexes_used(0),
        location_entries_base_offset(0) {
    this->location_index_table.fill(0);
  }

  size_t rand_int_biased(size_t min_v, size_t max_v) {
    float max_f = static_cast<float>(max_v + 1);
    uint32_t crypt_v = this->global_random_crypt.next();
    fprintf(stderr, "(global) => %08" PRIX32 "\n", crypt_v);
    float det_f = static_cast<float>(crypt_v);
    return max<size_t>(floorf((max_f * det_f) / UINT32_MAX_AS_FLOAT), min_v);
  }

  uint32_t next_location_index() {
    if (this->location_indexes_used < this->location_indexes_populated) {
      return this->location_index_table.at(this->location_indexes_used++);
    }
    return 0;
  }

  void generate_shuffled_location_table(const Map::RandomEnemyLocationsHeader& header, StringReader r, uint16_t section) {
    if (header.num_sections == 0) {
      throw runtime_error("no locations defined");
    }

    StringReader sections_r = r.sub(header.section_table_offset, header.num_sections * sizeof(Map::RandomEnemyLocationSection));

    size_t bs_min = 0;
    size_t bs_max = header.num_sections - 1;
    do {
      size_t bs_mid = (bs_min + bs_max) / 2;
      if (sections_r.pget<Map::RandomEnemyLocationSection>(bs_mid * sizeof(Map::RandomEnemyLocationSection)).section < section) {
        bs_min = bs_mid + 1;
      } else {
        bs_max = bs_mid;
      }
    } while (bs_min < bs_max);

    const auto& sec = sections_r.pget<Map::RandomEnemyLocationSection>(bs_min * sizeof(Map::RandomEnemyLocationSection));
    if (section != sec.section) {
      return;
    }

    this->location_indexes_populated = sec.count;
    this->location_indexes_used = 0;
    this->location_entries_base_offset = sec.offset;
    for (size_t z = 0; z < sec.count; z++) {
      this->location_index_table.at(z) = z;
    }

    for (size_t z = 0; z < 4; z++) {
      for (size_t x = 0; x < sec.count; x++) {
        uint32_t crypt_v = this->local_random_crypt.next();
        fprintf(stderr, "(local?) => %08" PRIX32 "\n", crypt_v);
        size_t choice = floorf((static_cast<float>(sec.count) * static_cast<float>(crypt_v)) / UINT32_MAX_AS_FLOAT);
        uint32_t t = this->location_index_table[x];
        this->location_index_table[x] = this->location_index_table[choice];
        this->location_index_table[choice] = t;
      }
    }
  }
};

void Map::add_random_enemies_from_map_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    StringReader wave_events_segment_r,
    StringReader locations_segment_r,
    StringReader definitions_segment_r,
    uint32_t rare_seed,
    const RareEnemyRates& rare_rates) {

  static const array<uint32_t, 41> rand_enemy_base_types = {
      0x44, 0x43, 0x41, 0x42, 0x40, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x80,
      0x81, 0x82, 0x83, 0x84, 0x85, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
      0xA7, 0xA8, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD,
      0xDE, 0xDF, 0xE0, 0xE0, 0xE1};

  const auto& wave_events_header = wave_events_segment_r.get<EventsSectionHeader>();
  if (wave_events_header.format != 0x65767432) { // 'evt2'
    throw runtime_error("cannot generate random enemies from non-evt2 event stream");
  }
  wave_events_segment_r.go(wave_events_header.entries_offset);

  const auto& locations_header = locations_segment_r.get<RandomEnemyLocationsHeader>();
  const auto& definitions_header = definitions_segment_r.get<RandomEnemyDefinitionsHeader>();
  auto definitions_r = definitions_segment_r.sub(
      definitions_header.entries_offset,
      definitions_header.entry_count * sizeof(RandomEnemyDefinition));
  auto weights_r = definitions_segment_r.sub(
      definitions_header.weight_entries_offset,
      definitions_header.weight_entry_count * sizeof(RandomEnemyWeight));

  DATParserRandomState random(rare_seed);

  for (size_t wave_entry_index = 0; wave_entry_index < wave_events_header.entry_count; wave_entry_index++) {
    auto entry_log = static_game_data_log.sub(string_printf("(Entry %zu/%" PRIu32 ") ", wave_entry_index, wave_events_header.entry_count.load()));
    entry_log.info("Start");
    const auto& entry = wave_events_segment_r.get<Event2Entry>();

    size_t remaining_waves = random.rand_int_biased(1, entry.max_waves);
    entry_log.info("Chose %zu waves (max=%hu)", remaining_waves, entry.max_waves.load());
    // BP 0080E125 EAX is wave count

    uint32_t wave_number = entry.wave_number;
    while (remaining_waves) {
      remaining_waves--;
      auto wave_log = entry_log.sub(string_printf("(Wave %zu) ", remaining_waves));

      size_t remaining_enemies = random.rand_int_biased(entry.min_enemies, entry.max_enemies);
      wave_log.info("Chose %zu enemies (range=[%hhu, %hhu])", remaining_enemies, entry.min_enemies, entry.max_enemies);
      // BP 0080E208 EDI is enemy count

      random.generate_shuffled_location_table(locations_header, locations_segment_r, entry.section);
      wave_log.info("Generated shuffled location table");
      for (size_t z = 0; z < random.location_indexes_populated; z++) {
        wave_log.info("  table[%zX] = %" PRIX32, z, random.location_index_table[z]);
      }
      // BP 0080EBB0 *(EBP + 4) points to table (0x20 uint32_ts)

      while (remaining_enemies) {
        remaining_enemies--;
        auto enemy_log = wave_log.sub(string_printf("(Enemy %zu) ", remaining_enemies));

        // TODO: Factor this sum out of the loops
        weights_r.go(0);
        size_t weight_total = 0;
        while (!weights_r.eof()) {
          weight_total += weights_r.get<RandomEnemyWeight>().weight;
        }
        // BP 0080E2C2 EBX is weight_total

        size_t det = random.rand_int_biased(0, weight_total - 1);
        enemy_log.info("weight_total=%zX, det=%zX", weight_total, det);
        // BP 0080E300 EDX is det

        weights_r.go(0);
        while (!weights_r.eof()) {
          const auto& weight_entry = weights_r.get<RandomEnemyWeight>();
          if (det < weight_entry.weight) {
            if ((weight_entry.base_type_index != 0xFF) && (weight_entry.definition_entry_num != 0xFF)) {
              EnemyEntry e;
              e.base_type = rand_enemy_base_types.at(weight_entry.base_type_index);
              e.wave_number = wave_number;
              e.section = entry.section;

              size_t bs_min = 0;
              size_t bs_max = definitions_header.entry_count - 1;
              if (bs_max == 0) {
                throw runtime_error("no available random enemy definitions");
              }
              do {
                size_t bs_mid = (bs_min + bs_max) / 2;
                if (definitions_r.pget<RandomEnemyDefinition>(bs_mid * sizeof(RandomEnemyDefinition)).entry_num < weight_entry.definition_entry_num) {
                  bs_min = bs_mid + 1;
                } else {
                  bs_max = bs_mid;
                }
              } while (bs_min < bs_max);

              const auto& def = definitions_r.pget<RandomEnemyDefinition>(bs_min * sizeof(RandomEnemyDefinition));
              if (def.entry_num == weight_entry.definition_entry_num) {
                e.fparam1 = def.fparam1;
                e.fparam2 = def.fparam2;
                e.fparam3 = def.fparam3;
                e.fparam4 = def.fparam4;
                e.fparam5 = def.fparam5;
                e.uparam1 = def.uparam1;
                e.uparam2 = def.uparam2;
                e.num_children = random.rand_int_biased(def.min_children, def.max_children);
              } else {
                throw runtime_error("random enemy definition not found");
              }

              const auto& loc = locations_segment_r.pget<RandomEnemyLocationEntry>(
                  locations_header.entries_offset + sizeof(RandomEnemyLocationEntry) * random.next_location_index());
              e.x = loc.x;
              e.y = loc.y;
              e.z = loc.z;
              e.x_angle = loc.x_angle;
              e.y_angle = loc.y_angle;
              e.z_angle = loc.z_angle;

              enemy_log.info("Creating enemy with base_type %04hX fparam2 %g uparam1 %04hX", e.base_type.load(), e.fparam2.load(), e.uparam1.load());
              // BP 0080E6FE CX is base_type
              this->add_enemy(episode, difficulty, event, 0, e, rare_rates);
            } else {
              enemy_log.info("Cannot create enemy: parameters are missing");
            }
            break;
          } else {
            det -= weight_entry.weight;
          }
        }
      }
      if (remaining_waves) {
        // We don't generate the event stream here, but the client does, and in
        // doing so, it uses one value from global_random_crypt to determine the
        // delay parameter of the event. To keep our state in sync with what the
        // client would do, we skip a random value here.
        random.global_random_crypt.next();
        wave_number++;
      }
    }

    // For the same reason as above, we need to skip another random value here.
    random.global_random_crypt.next();
  }
}

void Map::add_enemies_and_objects_from_quest_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    const void* data,
    size_t size,
    uint32_t rare_seed,
    const RareEnemyRates& rare_rates) {

  struct DATSectionsForFloor {
    uint32_t objects = 0xFFFFFFFF;
    uint32_t enemies = 0xFFFFFFFF;
    uint32_t wave_events = 0xFFFFFFFF;
    uint32_t random_enemy_locations = 0xFFFFFFFF;
    uint32_t random_enemy_definitions = 0xFFFFFFFF;
  };

  vector<DATSectionsForFloor> floor_sections;
  StringReader r(data, size);
  while (!r.eof()) {
    size_t header_offset = r.where();
    const auto& header = r.get<SectionHeader>();
    static_game_data_log.info("(DAT:%08zX) type=%08" PRIX32 " floor=%08" PRIX32 " data_size=%08" PRIX32,
        header_offset, header.le_type.load(), header.floor.load(), header.data_size.load());

    if (header.type() == SectionHeader::Type::END && header.section_size == 0) {
      break;
    }
    if (header.section_size < sizeof(header)) {
      throw runtime_error(string_printf("quest layout has invalid section header at offset 0x%zX", r.where() - sizeof(header)));
    }

    if (header.floor > 0x100) {
      throw runtime_error("section floor number too large");
    }

    if (header.floor >= floor_sections.size()) {
      floor_sections.resize(header.floor + 1);
    }
    auto& sections = floor_sections[header.floor];
    switch (header.type()) {
      case SectionHeader::Type::OBJECTS:
        if (sections.objects != 0xFFFFFFFF) {
          throw runtime_error("multiple objects sections for same floor");
        }
        sections.objects = header_offset;
        break;
      case SectionHeader::Type::ENEMIES:
        if (sections.enemies != 0xFFFFFFFF) {
          throw runtime_error("multiple enemies sections for same floor");
        }
        sections.enemies = header_offset;
        break;
      case SectionHeader::Type::WAVE_EVENTS:
        if (sections.wave_events != 0xFFFFFFFF) {
          throw runtime_error("multiple wave events sections for same floor");
        }
        sections.wave_events = header_offset;
        break;
      case SectionHeader::Type::RANDOM_ENEMY_LOCATIONS:
        if (sections.random_enemy_locations != 0xFFFFFFFF) {
          throw runtime_error("multiple random enemy locations sections for same floor");
        }
        sections.random_enemy_locations = header_offset;
        break;
      case SectionHeader::Type::RANDOM_ENEMY_DEFINITIONS:
        if (sections.random_enemy_definitions != 0xFFFFFFFF) {
          throw runtime_error("multiple random enemy definitions sections for same floor");
        }
        sections.random_enemy_definitions = header_offset;
        break;
      default:
        throw runtime_error("invalid section type");
    }
    r.skip(header.data_size);
  }

  for (size_t floor = 0; floor < floor_sections.size(); floor++) {
    const auto& sections = floor_sections[floor];

    if (sections.objects != 0xFFFFFFFF) {
      const auto& header = r.pget<SectionHeader>(sections.objects);
      if (header.data_size % sizeof(ObjectEntry)) {
        throw runtime_error("quest layout object section size is not a multiple of object entry size");
      }
      static_game_data_log.info("(Floor %02zX) Adding objects", floor);
      this->add_objects_from_map_data(r.pgetv(sections.objects + sizeof(header), header.data_size), header.data_size);
    }

    if (sections.enemies != 0xFFFFFFFF) {
      const auto& header = r.pget<SectionHeader>(sections.enemies);
      if (header.data_size % sizeof(EnemyEntry)) {
        throw runtime_error("quest layout enemy section size is not a multiple of enemy entry size");
      }
      static_game_data_log.info("(Floor %02zX) Adding enemies", floor);
      this->add_enemies_from_map_data(
          episode,
          difficulty,
          event,
          r.pgetv(sections.enemies + sizeof(header), header.data_size),
          header.data_size,
          rare_rates);

    } else if ((sections.wave_events != 0xFFFFFFFF) &&
        (sections.random_enemy_locations != 0xFFFFFFFF) &&
        (sections.random_enemy_definitions != 0xFFFFFFFF)) {
      static_game_data_log.info("(Floor %02zX) Adding random enemies", floor);
      const auto& wave_events_header = r.pget<SectionHeader>(sections.wave_events);
      const auto& random_enemy_locations_header = r.pget<SectionHeader>(sections.random_enemy_locations);
      const auto& random_enemy_definitions_header = r.pget<SectionHeader>(sections.random_enemy_definitions);
      this->add_random_enemies_from_map_data(
          episode,
          difficulty,
          event,
          r.sub(sections.wave_events + sizeof(SectionHeader), wave_events_header.data_size),
          r.sub(sections.random_enemy_locations + sizeof(SectionHeader), random_enemy_locations_header.data_size),
          r.sub(sections.random_enemy_definitions + sizeof(SectionHeader), random_enemy_definitions_header.data_size),
          rare_seed,
          rare_rates);
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
        entry.object_list_basename = r.pget_cstr(var2_r.get<U32T>());
        entry.enemy_list_basename = r.pget_cstr(var2_r.get<U32T>());
        entry.event_list_basename = r.pget_cstr(var2_r.get<U32T>());
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
        fprintf(stream, "[%02zX/%02zX/%02zX] %s %s %s\n", a, v1, v2, e.object_list_basename.c_str(), e.enemy_list_basename.c_str(), e.event_list_basename.c_str());
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
    Episode episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2, bool is_enemies) {
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

  // Try both _off<e|o>.dat and <e|o>_s.dat suffixes first before falling back
  // to non-solo version
  vector<string> ret;
  if (is_enemies) {
    if (is_solo) {
      ret.emplace_back(filename + "_offe.dat");
      ret.emplace_back(filename + "e_s.dat");
    }
    ret.emplace_back(filename + "e.dat");
  } else {
    if (is_solo) {
      ret.emplace_back(filename + "_offo.dat");
      ret.emplace_back(filename + "o_s.dat");
    }
    ret.emplace_back(filename + "o.dat");
  }
  return ret;
}

const Map::RareEnemyRates Map::NO_RARE_ENEMIES = {
    .hildeblue = 0x00000000,
    .rappy = 0x00000000,
    .nar_lily = 0x00000000,
    .pouilly_slime = 0x00000000,
    .merissa_aa = 0x00000000,
    .pazuzu = 0x00000000,
    .dorphon_eclair = 0x00000000,
    .kondrieu = 0x00000000,
};

const Map::RareEnemyRates Map::DEFAULT_RARE_ENEMIES = {
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
