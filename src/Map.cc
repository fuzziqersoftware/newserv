#include "Map.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "FileContentsCache.hh"

using namespace std;



BattleParamsIndex::BattleParamsIndex(
    shared_ptr<const string> data_on_ep1,
    shared_ptr<const string> data_on_ep2,
    shared_ptr<const string> data_on_ep4,
    shared_ptr<const string> data_off_ep1,
    shared_ptr<const string> data_off_ep2,
    shared_ptr<const string> data_off_ep4) {
  this->files[0][0].data = data_on_ep1;
  this->files[0][1].data = data_on_ep2;
  this->files[0][2].data = data_on_ep4;
  this->files[1][0].data = data_off_ep1;
  this->files[1][1].data = data_off_ep2;
  this->files[1][2].data = data_off_ep4;

  for (uint8_t is_solo = 0; is_solo < 2; is_solo++) {
    for (uint8_t episode = 0; episode < 3; episode++) {
      auto& file = this->files[is_solo][episode];
      if (file.data->size() < sizeof(Table)) {
        throw runtime_error(string_printf(
            "battle params table size is incorrect (expected %zX bytes, have %zX bytes; is_solo=%hhu, episode=%hhu)",
            sizeof(Table), file.data->size(), is_solo, episode));
      }
      file.table = reinterpret_cast<const Table*>(file.data->data());
    }
  }
}

const BattleParamsIndex::Entry& BattleParamsIndex::get(
    bool solo, uint8_t episode, uint8_t difficulty, uint8_t monster_type) const {
  if (episode > 3) {
    throw invalid_argument("incorrect episode");
  }
  if (difficulty > 4) {
    throw invalid_argument("incorrect difficulty");
  }
  if (monster_type > 0x60) {
    throw invalid_argument("incorrect monster type");
  }
  return this->files[!!solo][episode].table->difficulty[difficulty][monster_type];
}



PSOEnemy::PSOEnemy(uint64_t id) : PSOEnemy(id, 0, 0, 0, "__missing__") { }

PSOEnemy::PSOEnemy(
    uint64_t id,
    uint16_t source_type,
    uint32_t experience,
    uint32_t rt_index,
    const char* type_name)
  : id(id),
    source_type(source_type),
    hit_flags(0),
    last_hit(0),
    experience(experience),
    rt_index(rt_index),
    type_name(type_name) { }

string PSOEnemy::str() const {
  return string_printf("[Enemy E-%" PRIX64 " \"%s\" source_type=%hX hit=%02hhX/%hu exp=%" PRIu32 " rt_index=%" PRIX32 "]",
      this->id, this->type_name, this->source_type, this->hit_flags, this->last_hit, this->experience, this->rt_index);
}



struct EnemyEntry {
  uint32_t base;
  uint16_t reserved0;
  uint16_t num_clones;
  uint32_t reserved[11];
  float reserved12;
  uint32_t reserved13;
  uint32_t reserved14;
  uint32_t skin;
  uint32_t reserved15;
} __attribute__((packed));

static uint64_t next_enemy_id = 1;

vector<PSOEnemy> parse_map(
    shared_ptr<const BattleParamsIndex> battle_params,
    bool is_solo,
    uint8_t episode,
    uint8_t difficulty,
    shared_ptr<const string> data,
    bool alt_enemies) {

  const auto* map = reinterpret_cast<const EnemyEntry*>(data->data());
  size_t entry_count = data->size() / sizeof(EnemyEntry);
  if (data->size() != entry_count * sizeof(EnemyEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  vector<PSOEnemy> enemies;
  auto create_clones = [&](size_t count) {
    for (; count > 0; count--) {
      enemies.emplace_back(next_enemy_id++);
    }
  };

  auto get_battle_params = [&](uint8_t type) -> const BattleParamsIndex::Entry& {
    return battle_params->get(is_solo, episode, difficulty, type);
  };

  for (size_t y = 0; y < entry_count; y++) {
    const auto& e = map[y];
    size_t num_clones = e.num_clones;

    switch (e.base) {
      case 0x40: // Hildebear and Hildetorr
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x49 + (e.skin & 0x01)).experience,
            0x01 + (e.skin & 0x01), "Hilde(bear|torr)");
        break;
      case 0x41: // Rappies
        if (episode == 3) { // Del Rappy and Sand Rappy
          if (alt_enemies) {
            enemies.emplace_back(next_enemy_id++, e.base,
                get_battle_params(0x17 + (e.skin & 0x01)).experience,
                17 + (e.skin & 0x01), "(Del|Sand) Rappy");
          } else {
            enemies.emplace_back(next_enemy_id++, e.base,
                get_battle_params(0x05 + (e.skin & 0x01)).experience,
                17 + (e.skin & 0x01), "(Del|Sand) Rappy");
          }
        } else { // Rag Rappy and Al Rappy (Love for Episode II)
          if (e.skin & 0x01) {
            enemies.emplace_back(next_enemy_id++, e.base,
                get_battle_params(0x18 + (e.skin & 0x01)).experience,
                0xFF, "Rare Rappy"); // Don't know (yet) which rare Rappy it is
          } else {
            enemies.emplace_back(next_enemy_id++, e.base,
                get_battle_params(0x18 + (e.skin & 0x01)).experience,
                5, "Rag Rappy");
          }
        }
        break;
      case 0x42: // Monest + 30 Mothmants
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x01).experience, 4, "Monest");
        for (size_t x = 0; x < 30; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x00).experience, 3, "Mothmant");
        }
        break;
      case 0x43: // Savage Wolf and Barbarous Wolf
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x02 + ((e.reserved[10] & 0x800000) ? 1 : 0)).experience,
            7 + ((e.reserved[10] & 0x800000) ? 1 : 0), "(Savage|Barbarous) Wolf");
        break;
      case 0x44: // Booma family
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x4B + (e.skin % 3)).experience,
            9 + (e.skin % 3), "(|Go|Gigo)Booma");
        break;
      case 0x60: // Grass Assassin
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x4E).experience, 12, "Grass Assassin");
        break;
      case 0x61: // Del Lily, Poison Lily, Nar Lily
        if ((episode == 2) && (alt_enemies)) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x25).experience, 83, "Del Lily");
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x04 + ((e.reserved[10] & 0x800000) ? 1 : 0)).experience,
              13 + ((e.reserved[10] & 0x800000) ? 1 : 0), "(Poison|Nar) Lily");
        }
        break;
      case 0x62: // Nano Dragon
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1A).experience, 15, "Nano Dragon");
        break;
      case 0x63: // Shark family
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x4F + (e.skin % 3)).experience,
            16 + (e.skin % 3), "(Evil|Pal|Guil) Shark");
        break;
      case 0x64: // Slime + 4 clones
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x2F + ((e.reserved[10] & 0x800000) ? 0 : 1)).experience,
            19 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Pof?uilly Slime");
        for (size_t x = 0; x < 4; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x30).experience, 19, "Pof?uilly Slime clone");
        }
        break;
      case 0x65: // Pan Arms, Migium, Hidoom
        for (size_t x = 0; x < 3; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x31 + x).experience, 21 + x, "(Pan Arms|Hidoom|Migium)");
        }
        break;
      case 0x80: // Dubchic and Gillchic
        if (e.skin & 0x01) {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x1B + (e.skin & 0x01)).experience, 50, "(Dub|Gill)chic");
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x1B + (e.skin & 0x01)).experience, 24, "(Dub|Gill)chic");
        }
        break;
      case 0x81: // Garanz
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1D).experience, 25, "Garanz");
        break;
      case 0x82: // Sinow Beat and Gold
        if (e.reserved[10] & 0x800000) {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x13).experience,
              26 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Beat|Gold)");
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x06).experience,
              26 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Beat|Gold)");
        }
        if (e.num_clones == 0) {
          create_clones(4);
        }
        break;
      case 0x83: // Canadine
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x07).experience, 28, "Canadine");
        break;
      case 0x84: // Canadine Group
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x09).experience, 29, "Canune");
        for (size_t x = 0; x < 8; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x08).experience, 28, "Canadine");
        }
        break;
      case 0x85: // Dubwitch
        break;
      case 0xA0: // Delsaber
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x52).experience, 30, "Delsaber");
        break;
      case 0xA1: // Chaos Sorcerer + 2 Bits
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0A).experience, 31, "Chaos Sorcerer");
        create_clones(2);
        break;
      case 0xA2: // Dark Gunner
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1E).experience, 34, "Dark Gunner");
        break;
      case 0xA4: // Chaos Bringer
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0D).experience, 36, "Chaos Bringer");
        break;
      case 0xA5: // Dark Belra
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0E).experience, 37, "Dark Belra");
        break;
      case 0xA6: // Dimenian family
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x53 + (e.skin % 3)).experience, 41 + (e.skin % 3), "(|La|So) Dimenian");
        break;
      case 0xA7: // Bulclaw + 4 claws
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1F).experience, 40, "Bulclaw");
        for (size_t x = 0; x < 4; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x20).experience, 38, "Claw");
        }
        break;
      case 0xA8: // Claw
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x20).experience, 38, "Claw");
        break;
      case 0xC0: // Dragon or Gal Gryphon
        if (episode == 1) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x12).experience, 44, "Dragon");
        } else if (episode == 2) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1E).experience, 77, "Gal Gryphon");
        }
        break;
      case 0xC1: // De Rol Le
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0F).experience, 45, "De Rol Le");
        break;
      case 0xC2: // Vol Opt form 1
        break;
      case 0xC5: // Vol Opt form 2
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x25).experience, 46, "Vol Opt");
        break;
      case 0xC8: // Dark Falz + 510 Helpers
        if (difficulty) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x38).experience, 47, "Dark Falz 3"); // Final form
        } else {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x37).experience, 47, "Dark Falz 2"); // Second form
        }
        for (size_t x = 0; x < 510; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x35).experience, 0, "Darvant");
        }
        break;
      case 0xCA: // Olga Flow
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x2C).experience, 78, "Olga Flow");
        create_clones(0x200);
        break;
      case 0xCB: // Barba Ray
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0F).experience, 73, "Barba Ray");
        create_clones(0x2F);
        break;
      case 0xCC: // Gol Dragon
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x12).experience, 76, "Gol Dragon");
        create_clones(5);
        break;
      case 0xD4: // Sinows Berill & Spigell
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params((e.reserved[10] & 0x800000) ? 0x13 : 0x06).experience,
            62 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Berrill|Spigell)");
        create_clones(4);
        break;
      case 0xD5: // Merillia & Meriltas
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x4B + (e.skin & 0x01)).experience,
            52 + (e.skin & 0x01), "Meril(lia|tas)");
        break;
      case 0xD6: // Mericus, Merikle, & Mericarol
        if (e.skin) {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x44 + (e.skin % 3)).experience, 56 + (e.skin % 3), "Meri(cus|kle|carol)");
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x3A).experience, 56 + (e.skin % 3), "Meri(cus|kle|carol)");
        }
        break;
      case 0xD7: // Ul Gibbon and Zol Gibbon
        enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x3B + (e.skin & 0x01)).experience,
              59 + (e.skin & 0x01), "(Ul|Zol) Gibbon");
        break;
      case 0xD8: // Gibbles
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x3D).experience, 61, "Gibbles");
        break;
      case 0xD9: // Gee
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x07).experience, 54, "Gee");
        break;
      case 0xDA: // Gi Gue
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1A).experience, 55, "Gi Gue");
        break;
      case 0xDB: // Deldepth
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x30).experience, 71, "Deldepth");
        break;
      case 0xDC: // Delbiter
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x0D).experience, 72, "Delbiter");
        break;
      case 0xDD: // Dolmolm and Dolmdarl
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x4F + (e.skin & 0x01)).experience,
            64 + (e.skin & 0x01), "Dolm(olm|darl)");
        break;
      case 0xDE: // Morfos
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x40).experience, 66, "Morfos");
        break;
      case 0xDF: // Recobox & Recons
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x41).experience, 67, "Recobox");
        for (size_t x = 0; x < e.num_clones; x++) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x42).experience, 68, "Recon");
        }
        break;
      case 0xE0: // Epsilon, Sinow Zoa and Zele
        if ((episode == 2) && (alt_enemies)) {
          enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x23).experience, 84, "Epsilon");
          create_clones(4);
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x43 + (e.skin & 0x01)).experience,
              69 + (e.skin & 0x01), "Sinow Z(oa|ele)");
        }
        break;
      case 0xE1: // Ill Gill
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x26).experience, 82, "Ill Gill");
        break;
      case 0x0110: // Astark
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x09).experience, 1, "Astark");
        break;
      case 0x0111: // Satellite Lizard and Yowie
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x0D + ((e.reserved[10] & 0x800000) ? 1 : 0) + (alt_enemies ? 0x10 : 0)).experience,
            2 + ((e.reserved[10] & 0x800000) ? 0 : 1), "(Satellite Lizard|Yowie)");
        break;
      case 0x0112: // Merissa A/AA
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x19 + (e.skin & 0x01)).experience,
            4 + (e.skin & 0x01), "Merissa AA?");
        break;
      case 0x0113: // Girtablulu
        enemies.emplace_back(next_enemy_id++, e.base, get_battle_params(0x1F).experience, 6, "Girtablulu");
        break;
      case 0x0114: // Zu and Pazuzu
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x0B + (e.skin & 0x01) + (alt_enemies ? 0x14: 0x00)).experience,
            7 + (e.skin & 0x01), "(Pazu)?zu");
        break;
      case 0x0115: // Boota family
        if (e.skin & 2) {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x03).experience, 9 + (e.skin % 3), "(|Ba|Ze) Boota");
        } else {
          enemies.emplace_back(next_enemy_id++, e.base,
              get_battle_params(0x00 + (e.skin % 3)).experience,
              9 + (e.skin % 3), "(|Ba|Ze) Boota");
        }
        break;
      case 0x0116: // Dorphon and Eclair
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x0F + (e.skin & 0x01)).experience,
            12 + (e.skin & 0x01), "Dorphon (Eclair)?");
        break;
      case 0x0117: // Goran family
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x11 + (e.skin % 3)).experience,
            (e.skin & 2) ? 15 : ((e.skin & 1) ? 16 : 14), "Goran...");
        break;
      case 0x0119: // Saint Million, Shambertin, Kondrieu
        enemies.emplace_back(next_enemy_id++, e.base,
            get_battle_params(0x22).experience,
            (e.reserved[10] & 0x800000) ? 21 : (19 + (e.skin & 0x01)),
            "(Saint-Million|Shambertin|Kondrieu)");
        break;
      default:
        enemies.emplace_back(next_enemy_id++, e.base, 0xFFFFFFFF, 0, "__unknown__");
        static_game_data_log.warning(
            "(Entry %zu, offset %zX in file) Unknown enemy type %08" PRIX32 " %08" PRIX32,
            y, y * sizeof(EnemyEntry), e.base, e.skin);
        break;
    }
    create_clones(num_clones);
  }

  return enemies;
}



SetDataTable::SetDataTable(shared_ptr<const string> data, bool big_endian) {
  if (big_endian) {
    this->load_table_t<be_uint32_t>(data);
  } else {
    this->load_table_t<le_uint32_t>(data);
  }
}

template <typename U32T>
void SetDataTable::load_table_t(shared_ptr<const string> data) {
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
      variation2_values(variation2_values) { }
};

// These are indexed as [episode][is_solo][area]
// (Note that Lobby::episode is 1-3, so we actually use episode - 1)
static const vector<vector<vector<AreaMapFileIndex>>> map_file_info = {
  { // Episode 1
    { // Non-solo
      {"city00",    {},        {0}},
      {"forest01",  {},        {0, 1, 2, 3, 4}},
      {"forest02",  {},        {0, 1, 2, 3, 4}},
      {"cave01",    {0, 1, 2}, {0, 1}},
      {"cave02",    {0, 1, 2}, {0, 1}},
      {"cave03",    {0, 1, 2}, {0, 1}},
      {"machine01", {0, 1, 2}, {0, 1}},
      {"machine02", {0, 1, 2}, {0, 1}},
      {"ancient01", {0, 1, 2}, {0, 1}},
      {"ancient02", {0, 1, 2}, {0, 1}},
      {"ancient03", {0, 1, 2}, {0, 1}},
      {"boss01",    {},        {}},
      {"boss02",    {},        {}},
      {"boss03",    {},        {}},
      {"boss04",    {},        {}},
      {nullptr,     {},        {}},
    },
    { // Solo
      {"city00",    {},        {0}},
      {"forest01",  {},        {0, 2, 4}},
      {"forest02",  {},        {0, 3, 4}},
      {"cave01",    {0, 1, 2}, {0}},
      {"cave02",    {0, 1, 2}, {0}},
      {"cave03",    {0, 1, 2}, {0}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
    },
  },
  { // Episode 2
    { // Non-solo
      {"labo00",    {},        {0}},
      {"ruins01",   {0, 1},    {0}},
      {"ruins02",   {0, 1},    {0}},
      {"space01",   {0, 1},    {0}},
      {"space02",   {0, 1},    {0}},
      {"jungle01",  {},        {0, 1, 2}},
      {"jungle02",  {},        {0, 1, 2}},
      {"jungle03",  {},        {0, 1, 2}},
      {"jungle04",  {0, 1},    {0, 1}},
      {"jungle05",  {},        {0, 1, 2}},
      {"seabed01",  {0, 1},    {0, 1}},
      {"seabed02",  {0, 1},    {0, 1}},
      {"boss05",    {},        {}},
      {"boss06",    {},        {}},
      {"boss07",    {},        {}},
      {"boss08",    {},        {}},
    },
    { // Solo
      {"labo00",    {},        {0}},
      {"ruins01",   {0, 1},    {0}},
      {"ruins02",   {0, 1},    {0}},
      {"space01",   {0, 1},    {0}},
      {"space02",   {0, 1},    {0}},
      {"jungle01",  {},        {0, 1, 2}},
      {"jungle02",  {},        {0, 1, 2}},
      {"jungle03",  {},        {0, 1, 2}},
      {"jungle04",  {0, 1},    {0, 1}},
      {"jungle05",  {},        {0, 1, 2}},
      {"seabed01",  {0, 1},    {0}},
      {"seabed02",  {0, 1},    {0}},
      {"boss05",    {},        {}},
      {"boss06",    {},        {}},
      {"boss07",    {},        {}},
      {"boss08",    {},        {}},
    },
  },
  { // Episode 4
    { // Non-solo
      {"city02",    {0},       {0}},
      {"wilds01",   {0},       {0, 1, 2}},
      {"wilds01",   {1},       {0, 1, 2}},
      {"wilds01",   {2},       {0, 1, 2}},
      {"wilds01",   {3},       {0, 1, 2}},
      {"crater01",  {0},       {0, 1, 2}},
      {"desert01",  {0, 1, 2}, {0}},
      {"desert02",  {0},       {0, 1, 2}},
      {"desert03",  {0, 1, 2}, {0}},
      {"boss09",    {0},       {0}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
    },
    { // Solo
      {"city02",    {0},       {0}},
      {"wilds01",   {0},       {0, 1, 2}},
      {"wilds01",   {1},       {0, 1, 2}},
      {"wilds01",   {2},       {0, 1, 2}},
      {"wilds01",   {3},       {0, 1, 2}},
      {"crater01",  {0},       {0, 1, 2}},
      {"desert01",  {0, 1, 2}, {0}},
      {"desert02",  {0},       {0, 1, 2}},
      {"desert03",  {0, 1, 2}, {0}},
      {"boss09",    {0},       {0}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
      {nullptr,     {},        {}},
    },
  },
};

void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    shared_ptr<mt19937> random,
    uint8_t episode,
    bool is_solo) {
  const auto& ep_index = map_file_info.at(episode - 1);
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
      variations[z * 2 + 0] = (a->variation1_values.size() < 2) ? 0 :
          ((*random)() % a->variation1_values.size());
      variations[z * 2 + 1] = (a->variation2_values.size() < 2) ? 0 :
          ((*random)() % a->variation2_values.size());
    }
  }
}

vector<string> map_filenames_for_variation(
    uint8_t episode, bool is_solo, uint8_t area, uint32_t var1, uint32_t var2) {
  // Map filenames are like map_<name_token>[_VV][_VV][_off]<e|o>[_s].dat
  //   name_token comes from AreaMapFileIndex
  //   _VV are the values from the variation<1|2>_values vector (in contrast to
  //     the values sent in the 64 command, which are INDEXES INTO THAT VECTOR)
  //   _off or _s are used for solo mode (try both - city uses _s whereas levels
  //     use _off apparently)
  //   e|o specifies what kind of data: e = enemies, o = objects
  const auto& ep_index = map_file_info.at(episode - 1);
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
