#include "Map.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "StaticGameData.hh"

using namespace std;



string BattleParamsIndex::Entry::str() const {
  string a1str = format_data_string(this->unknown_a1.data(), this->unknown_a1.bytes());
  return string_printf(
      "BattleParamsEntry[ATP=%hu PSV=%hu EVP=%hu HP=%hu DFP=%hu ATA=%hu LCK=%hu ESP=%hu a1=%s EXP=%" PRIu32 " diff=%" PRIu32 "]",
      this->atp.load(),
      this->psv.load(),
      this->evp.load(),
      this->hp.load(),
      this->dfp.load(),
      this->ata.load(),
      this->lck.load(),
      this->esp.load(),
      a1str.c_str(),
      this->experience.load(),
      this->difficulty.load());
}

void BattleParamsIndex::Table::print(FILE* stream) const {
  auto print_entry = +[](FILE* stream, const Entry& e) {
    string a1str = format_data_string(e.unknown_a1.data(), e.unknown_a1.bytes());
    fprintf(stream,
      "%5hu %5hu %5hu %5hu %5hu %5hu %5hu %5hu %s %5" PRIu32 " %5" PRIu32,
      e.atp.load(),
      e.psv.load(),
      e.evp.load(),
      e.hp.load(),
      e.dfp.load(),
      e.ata.load(),
      e.lck.load(),
      e.esp.load(),
      a1str.c_str(),
      e.experience.load(),
      e.difficulty.load());
  };

  for (size_t diff = 0; diff < 4; diff++) {
    fprintf(stream, "%c ZZ   ATP   PSV   EVP    HP   DFP   ATA   LCK   ESP                       A1   EXP  DIFF\n",
        abbreviation_for_difficulty(diff));
    for (size_t z = 0; z < 0x60; z++) {
      fprintf(stream, "  %02zX ", z);
      print_entry(stream, this->difficulty[diff][z]);
      fputc('\n', stream);
    }
  }
}



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
    bool solo, Episode episode, uint8_t difficulty, uint8_t monster_type) const {
  if (difficulty > 4) {
    throw invalid_argument("incorrect difficulty");
  }
  if (monster_type > 0x60) {
    throw invalid_argument("incorrect monster type");
  }

  uint8_t ep_index;
  switch (episode) {
    case Episode::EP1:
      ep_index = 0;
      break;
    case Episode::EP2:
      ep_index = 1;
      break;
    case Episode::EP4:
      ep_index = 2;
      break;
    default:
      throw invalid_argument("invalid episode");
  }

  return this->files[!!solo][ep_index].table->difficulty[difficulty][monster_type];
}



PSOEnemy::PSOEnemy(uint64_t id) : PSOEnemy(id, 0, 0, 0, 0, "__missing__") { }

PSOEnemy::PSOEnemy(
    uint64_t id,
    uint16_t source_type,
    uint32_t experience,
    uint32_t rt_index,
    size_t num_clones,
    const char* type_name)
  : id(id),
    source_type(source_type),
    hit_flags(0),
    last_hit(0),
    experience(experience),
    rt_index(rt_index),
    num_clones(num_clones),
    type_name(type_name) { }

string PSOEnemy::str() const {
  return string_printf("[Enemy E-%" PRIX64 " \"%s\" source_type=%hX hit=%02hhX/%hu exp=%" PRIu32 " rt_index=%" PRIX32 " clones=%zu]",
      this->id, this->type_name, this->source_type, this->hit_flags, this->last_hit, this->experience, this->rt_index, this->num_clones);
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
    Episode episode,
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

  auto create_enemy = [&](
      const EnemyEntry& e,
      ssize_t bp_index,
      uint32_t rt_index,
      const char* type_name) {
    const BattleParamsIndex::Entry& bp_entry = battle_params->get(
        is_solo, episode, difficulty, bp_index);
    enemies.emplace_back(
        next_enemy_id++,
        e.base,
        bp_entry.experience,
        rt_index,
        e.num_clones,
        type_name);
  };

  for (size_t y = 0; y < entry_count; y++) {
    const auto& e = map[y];

    switch (e.base) {
      case 0x40: // Hildebear and Hildetorr
        create_enemy(e, 0x49 + (e.skin & 0x01), 0x01 + (e.skin & 0x01), "Hilde(bear|torr)");
        break;
      case 0x41: // Rappies
        if (episode == Episode::EP4) { // Del Rappy and Sand Rappy
          if (alt_enemies) {
            create_enemy(e, 0x17 + (e.skin & 0x01), 17 + (e.skin & 0x01), "(Del|Sand) Rappy");
          } else {
            create_enemy(e, 0x05 + (e.skin & 0x01), 17 + (e.skin & 0x01), "(Del|Sand) Rappy");
          }
        } else { // Rag Rappy and Al Rappy (Love for Episode II)
          if (e.skin & 0x01) {
            // TODO: Don't know (yet) which rare Rappy it is
            create_enemy(e, 0x18 + (e.skin & 0x01), 0xFF, "Rare Rappy");
          } else {
            create_enemy(e, 0x18 + (e.skin & 0x01), 5, "Rag Rappy");
          }
        }
        break;
      case 0x42: // Monest + 30 Mothmants
        create_enemy(e, 0x01, 4, "Monest");
        for (size_t x = 0; x < 30; x++) {
          create_enemy(e, 0x00, 3, "Mothmant");
        }
        break;
      case 0x43: // Savage Wolf and Barbarous Wolf
        create_enemy(e, 0x02 + ((e.reserved[10] & 0x800000) ? 1 : 0),
            7 + ((e.reserved[10] & 0x800000) ? 1 : 0), "(Savage|Barbarous) Wolf");
        break;
      case 0x44: // Booma family
        create_enemy(e, 0x4B + (e.skin % 3), 9 + (e.skin % 3), "(|Go|Gigo)Booma");
        break;
      case 0x60: // Grass Assassin
        create_enemy(e, 0x4E, 12, "Grass Assassin");
        break;
      case 0x61: // Del Lily, Poison Lily, Nar Lily
        if ((episode == Episode::EP2) && (alt_enemies)) {
          create_enemy(e, 0x25, 83, "Del Lily");
        } else {
          create_enemy(e, 0x04 + ((e.reserved[10] & 0x800000) ? 1 : 0),
              13 + ((e.reserved[10] & 0x800000) ? 1 : 0), "(Poison|Nar) Lily");
        }
        break;
      case 0x62: // Nano Dragon
        create_enemy(e, 0x1A, 15, "Nano Dragon");
        break;
      case 0x63: // Shark family
        create_enemy(e, 0x4F + (e.skin % 3), 16 + (e.skin % 3), "(Evil|Pal|Guil) Shark");
        break;
      case 0x64: // Slime + 4 clones
        create_enemy(e, 0x2F + ((e.reserved[10] & 0x800000) ? 0 : 1),
            19 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Pof?uilly Slime");
        for (size_t x = 0; x < 4; x++) {
          create_enemy(e, 0x30, 19, "Pof?uilly Slime clone");
        }
        break;
      case 0x65: // Pan Arms, Migium, Hidoom
        for (size_t x = 0; x < 3; x++) {
          create_enemy(e, 0x31 + x, 21 + x, "(Pan Arms|Hidoom|Migium)");
        }
        break;
      case 0x80: // Dubchic and Gillchic
        if (e.skin & 0x01) {
          create_enemy(e, 0x1B + (e.skin & 0x01), 50, "(Dub|Gill)chic");
        } else {
          create_enemy(e, 0x1B + (e.skin & 0x01), 24, "(Dub|Gill)chic");
        }
        break;
      case 0x81: // Garanz
        create_enemy(e, 0x1D, 25, "Garanz");
        break;
      case 0x82: // Sinow Beat and Gold
        if (e.reserved[10] & 0x800000) {
          create_enemy(e, 0x13, 26 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Beat|Gold)");
        } else {
          create_enemy(e, 0x06, 26 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Beat|Gold)");
        }
        if (e.num_clones == 0) {
          create_clones(4);
        }
        break;
      case 0x83: // Canadine
        create_enemy(e, 0x07, 28, "Canadine");
        break;
      case 0x84: // Canadine Group
        create_enemy(e, 0x09, 29, "Canune");
        for (size_t x = 0; x < 8; x++) {
          create_enemy(e, 0x08, 28, "Canadine");
        }
        break;
      case 0x85: // Dubwitch
        enemies.emplace_back(next_enemy_id++, e.base, 0xFFFFFFFF, 0, 0, "__dubwitch__");
        break;
      case 0xA0: // Delsaber
        create_enemy(e, 0x52, 30, "Delsaber");
        break;
      case 0xA1: // Chaos Sorcerer + 2 Bits
        create_enemy(e, 0x0A, 31, "Chaos Sorcerer");
        create_clones(2);
        break;
      case 0xA2: // Dark Gunner
        create_enemy(e, 0x1E, 34, "Dark Gunner");
        break;
      case 0xA4: // Chaos Bringer
        create_enemy(e, 0x0D, 36, "Chaos Bringer");
        break;
      case 0xA5: // Dark Belra
        create_enemy(e, 0x0E, 37, "Dark Belra");
        break;
      case 0xA6: // Dimenian family
        create_enemy(e, 0x53 + (e.skin % 3), 41 + (e.skin % 3), "(|La|So) Dimenian");
        break;
      case 0xA7: // Bulclaw + 4 claws
        create_enemy(e, 0x1F, 40, "Bulclaw");
        for (size_t x = 0; x < 4; x++) {
          create_enemy(e, 0x20, 38, "Claw");
        }
        break;
      case 0xA8: // Claw
        create_enemy(e, 0x20, 38, "Claw");
        break;
      case 0xC0: // Dragon or Gal Gryphon
        if (episode == Episode::EP1) {
          create_enemy(e, 0x12, 44, "Dragon");
        } else if (episode == Episode::EP2) {
          create_enemy(e, 0x1E, 77, "Gal Gryphon");
        }
        break;
      case 0xC1: // De Rol Le
        create_enemy(e, 0x0F, 45, "De Rol Le");
        break;
      case 0xC2: // Vol Opt form 1
        enemies.emplace_back(next_enemy_id++, e.base, 0xFFFFFFFF, 0, 0, "__vol_opt_1__");
        break;
      case 0xC5: // Vol Opt form 2
        create_enemy(e, 0x25, 46, "Vol Opt");
        break;
      case 0xC8: // Dark Falz + 510 Darvants
        if (difficulty) {
          create_enemy(e, 0x38, 47, "Dark Falz 3"); // Final form
        } else {
          create_enemy(e, 0x37, 47, "Dark Falz 2"); // Second form
        }
        for (size_t x = 0; x < 510; x++) {
          create_enemy(e, 0x35, 0, "Darvant");
        }
        break;
      case 0xCA: // Olga Flow
        create_enemy(e, 0x2C, 78, "Olga Flow");
        create_clones(0x200);
        break;
      case 0xCB: // Barba Ray
        create_enemy(e, 0x0F, 73, "Barba Ray");
        create_clones(0x2F);
        break;
      case 0xCC: // Gol Dragon
        create_enemy(e, 0x12, 76, "Gol Dragon");
        create_clones(5);
        break;
      case 0xD4: // Sinows Berill & Spigell
        create_enemy(e, (e.reserved[10] & 0x800000) ? 0x13 : 0x06,
            62 + ((e.reserved[10] & 0x800000) ? 1 : 0), "Sinow (Berrill|Spigell)");
        create_clones(4);
        break;
      case 0xD5: // Merillia & Meriltas
        create_enemy(e, 0x4B + (e.skin & 0x01), 52 + (e.skin & 0x01), "Meril(lia|tas)");
        break;
      case 0xD6: // Mericus, Merikle, & Mericarol
        if (e.skin) {
          create_enemy(e, 0x44 + (e.skin % 3), 56 + (e.skin % 3), "Meri(cus|kle|carol)");
        } else {
          create_enemy(e, 0x3A, 56 + (e.skin % 3), "Meri(cus|kle|carol)");
        }
        break;
      case 0xD7: // Ul Gibbon and Zol Gibbon
        create_enemy(e, 0x3B + (e.skin & 0x01), 59 + (e.skin & 0x01), "(Ul|Zol) Gibbon");
        break;
      case 0xD8: // Gibbles
        create_enemy(e, 0x3D, 61, "Gibbles");
        break;
      case 0xD9: // Gee
        create_enemy(e, 0x07, 54, "Gee");
        break;
      case 0xDA: // Gi Gue
        create_enemy(e, 0x1A, 55, "Gi Gue");
        break;
      case 0xDB: // Deldepth
        create_enemy(e, 0x30, 71, "Deldepth");
        break;
      case 0xDC: // Delbiter
        create_enemy(e, 0x0D, 72, "Delbiter");
        break;
      case 0xDD: // Dolmolm and Dolmdarl
        create_enemy(e, 0x4F + (e.skin & 0x01), 64 + (e.skin & 0x01), "Dolm(olm|darl)");
        break;
      case 0xDE: // Morfos
        create_enemy(e, 0x40, 66, "Morfos");
        break;
      case 0xDF: // Recobox & Recons
        create_enemy(e, 0x41, 67, "Recobox");
        for (size_t x = 0; x < e.num_clones; x++) {
          create_enemy(e, 0x42, 68, "Recon");
        }
        break;
      case 0xE0: // Epsilon, Sinow Zoa and Zele
        if ((episode == Episode::EP2) && (alt_enemies)) {
          create_enemy(e, 0x23, 84, "Epsilon");
          create_clones(4);
        } else {
          create_enemy(e, 0x43 + (e.skin & 0x01), 69 + (e.skin & 0x01), "Sinow Z(oa|ele)");
        }
        break;
      case 0xE1: // Ill Gill
        create_enemy(e, 0x26, 82, "Ill Gill");
        break;
      case 0x0110: // Astark
        create_enemy(e, 0x09, 1, "Astark");
        break;
      case 0x0111: // Satellite Lizard and Yowie
        create_enemy(e, 0x0D + ((e.reserved[10] & 0x800000) ? 1 : 0) + (alt_enemies ? 0x10 : 0),
            2 + ((e.reserved[10] & 0x800000) ? 0 : 1), "(Satellite Lizard|Yowie)");
        break;
      case 0x0112: // Merissa A/AA
        create_enemy(e, 0x19 + (e.skin & 0x01), 4 + (e.skin & 0x01), "Merissa AA?");
        break;
      case 0x0113: // Girtablulu
        create_enemy(e, 0x1F, 6, "Girtablulu");
        break;
      case 0x0114: // Zu and Pazuzu
        create_enemy(e, 0x0B + (e.skin & 0x01) + (alt_enemies ? 0x14: 0x00),
            7 + (e.skin & 0x01), "(Pazu)?zu");
        break;
      case 0x0115: // Boota family
        if (e.skin & 2) {
          create_enemy(e, 0x03, 9 + (e.skin % 3), "(|Ba|Ze) Boota");
        } else {
          create_enemy(e, 0x00 + (e.skin % 3), 9 + (e.skin % 3), "(|Ba|Ze) Boota");
        }
        break;
      case 0x0116: // Dorphon and Eclair
        create_enemy(e, 0x0F + (e.skin & 0x01), 12 + (e.skin & 0x01), "Dorphon( Eclair)?");
        break;
      case 0x0117: // Goran family
        create_enemy(e, 0x11 + (e.skin % 3), (e.skin & 2) ? 15 : ((e.skin & 1) ? 16 : 14), "(Pyro )?Goran( Detonator)?");
        break;
      case 0x0119: // Saint-Million, Shambertin, Kondrieu
        create_enemy(e, 0x22,
            (e.reserved[10] & 0x800000) ? 21 : (19 + (e.skin & 0x01)),
            "(Saint-Million|Shambertin|Kondrieu)");
        break;
      default:
        enemies.emplace_back(next_enemy_id++, e.base, 0xFFFFFFFF, 0, 0, "__unknown__");
        static_game_data_log.warning(
            "(Entry %zu, offset %zX in file) Unknown enemy type %08" PRIX32 " %08" PRIX32,
            y, y * sizeof(EnemyEntry), e.base, e.skin);
        break;
    }
    create_clones(e.num_clones);
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

// These are indexed as [episode][is_solo][area], where episode is 0-2
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
    shared_ptr<mt19937> random,
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
      variations[z * 2 + 0] = (a->variation1_values.size() < 2) ? 0 :
          ((*random)() % a->variation1_values.size());
      variations[z * 2 + 1] = (a->variation2_values.size() < 2) ? 0 :
          ((*random)() % a->variation2_values.size());
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
