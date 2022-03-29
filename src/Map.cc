#include "Map.hh"

#include <phosg/Filesystem.hh>

#include "FileContentsCache.hh"

using namespace std;

extern FileContentsCache file_cache;



static void load_battle_param_file(const string& filename, BattleParams* entries) {
  scoped_fd fd(filename, O_RDONLY);
  readx(fd, entries, 0x60 * sizeof(BattleParams));
}

BattleParamTable::BattleParamTable(const char* prefix) {
  load_battle_param_file(string_printf("%s_on.dat", prefix),
      &this->entries[0][0][0][0]);
  load_battle_param_file(string_printf("%s_lab_on.dat", prefix),
      &this->entries[0][1][0][0]);
  load_battle_param_file(string_printf("%s_ep4_on.dat", prefix),
      &this->entries[0][2][0][0]);
  load_battle_param_file(string_printf("%s.dat", prefix),
      &this->entries[1][0][0][0]);
  load_battle_param_file(string_printf("%s_lab.dat", prefix),
      &this->entries[1][1][0][0]);
  load_battle_param_file(string_printf("%s_ep4.dat", prefix),
      &this->entries[1][2][0][0]);
}

const BattleParams& BattleParamTable::get(bool solo, uint8_t episode,
    uint8_t difficulty, uint8_t monster_type) const {
  if (episode > 3) {
    throw invalid_argument("incorrect episode");
  }
  if (difficulty > 4) {
    throw invalid_argument("incorrect difficulty");
  }
  if (monster_type > 0x60) {
    throw invalid_argument("incorrect monster type");
  }
  return this->entries[!!solo][episode][difficulty][monster_type];
}

const BattleParams* BattleParamTable::get_subtable(bool solo, uint8_t episode,
    uint8_t difficulty) const {
  if (episode > 3) {
    throw invalid_argument("incorrect episode");
  }
  if (difficulty > 4) {
    throw invalid_argument("incorrect difficulty");
  }
  return &this->entries[!!solo][episode][difficulty][0];
}



PSOEnemy::PSOEnemy() : PSOEnemy(0, 0) { }

PSOEnemy::PSOEnemy(uint32_t experience, uint32_t rt_index) : unused(0),
    hit_flags(0), last_hit(0), experience(experience), rt_index(rt_index) { }



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

static vector<PSOEnemy> parse_map(uint8_t episode, uint8_t difficulty,
    const BattleParams* battle_params, const EnemyEntry* map,
    size_t entry_count, bool alt_enemies) {

  vector<PSOEnemy> enemies;
  enemies.resize(0xB50);
  size_t num_enemies = 0;

  // TODO: this is some of the nastiest code ever. de-nastify it at your leisure
  for (size_t y = 0; y < entry_count; y++) {
    if (enemies.size() >= 0xB50) {
      break;
    }

    size_t num_clones = map[y].num_clones;

    switch (map[y].base) {
      case 0x40: // Hildebear and Hildetorr
        enemies[num_enemies].rt_index = 0x01 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x49 + (map[y].skin & 0x01)].experience;
        break;
      case 0x41: // Rappies
        if (episode == 3) { // Del Rappy and Sand Rappy
          enemies[num_enemies].rt_index = 17 + (map[y].skin & 0x01);
          if (alt_enemies) {
            enemies[num_enemies].experience = battle_params[0x17 + (map[y].skin & 0x01)].experience;
          } else {
            enemies[num_enemies].experience = battle_params[0x05 + (map[y].skin & 0x01)].experience;
          }
        } else { // Rag Rappy and Al Rappy (Love for Episode II)
          if (map[y].skin & 0x01) {
            enemies[num_enemies].rt_index = 0xFF; // No clue what rappy it could be... yet.
          } else {
            enemies[num_enemies].rt_index = 5;
          }
          enemies[num_enemies].experience = battle_params[0x18 + (map[y].skin & 0x01)].experience;
        }
        break;
      case 0x42: // Monest + 30 Mothmants
        enemies[num_enemies].experience = battle_params[0x01].experience;
        enemies[num_enemies].rt_index = 4;
        for (size_t x = 0; x < 30; x++) {
          if (num_enemies >= 0xB50) {
            break;
          }
          num_enemies++;
          enemies[num_enemies].rt_index = 3;
          enemies[num_enemies].experience = battle_params[0x00].experience;
        }
        break;
      case 0x43: // Savage Wolf and Barbarous Wolf
        enemies[num_enemies].rt_index = 7 + ((map[y].reserved[10] & 0x800000) ? 1 : 0);
        enemies[num_enemies].experience = battle_params[0x02 + ((map[y].reserved[10] & 0x800000) ? 1 : 0)].experience;
        break;
      case 0x44: // Booma family
        enemies[num_enemies].rt_index = 9 + (map[y].skin % 3);
        enemies[num_enemies].experience = battle_params[0x4B + (map[y].skin % 3)].experience;
        break;
      case 0x60: // Grass Assassin
        enemies[num_enemies].rt_index = 12;
        enemies[num_enemies].experience = battle_params[0x4E].experience;
        break;
      case 0x61: // Del Lily, Poison Lily, Nar Lily
        if ((episode == 2) && (alt_enemies)) {
          enemies[num_enemies].rt_index = 83;
          enemies[num_enemies].experience = battle_params[0x25].experience;
        } else {
          enemies[num_enemies].rt_index = 13 + ((map[y].reserved[10] & 0x800000) ? 1 : 0);
          enemies[num_enemies].experience = battle_params[0x04 + ((map[y].reserved[10] & 0x800000) ? 1 : 0)].experience;
        }
        break;
      case 0x62: // Nano Dragon
        enemies[num_enemies].rt_index = 15;
        enemies[num_enemies].experience = battle_params[0x1A].experience;
        break;
      case 0x63: // Shark family
        enemies[num_enemies].rt_index = 16 + (map[y].skin % 3);
        enemies[num_enemies].experience = battle_params[0x4F + (map[y].skin % 3)].experience;
        break;
      case 0x64: // Slime + 4 clones
        enemies[num_enemies].rt_index = 19 + ((map[y].reserved[10] & 0x800000) ? 1 : 0);
        enemies[num_enemies].experience = battle_params[0x2F + ((map[y].reserved[10] & 0x800000) ? 0 : 1)].experience;
        for (size_t x = 0; x < 4; x++) {
          if (num_enemies >= 0xB50) {
            break;
          }
          num_enemies++;
          enemies[num_enemies].rt_index = 19;
          enemies[num_enemies].experience = battle_params[0x30].experience;
        }
        break;
      case 0x65: // Pan Arms, Migium, Hidoom
        for (size_t x = 0; x < 3; x++) {
          enemies[num_enemies + x].rt_index = 21 + x;
          enemies[num_enemies + x].experience = battle_params[0x31 + x].experience;
        }
        num_enemies += 2;
        break;
      case 0x80: // Dubchic and Gilchic
        enemies[num_enemies].experience = battle_params[0x1B + (map[y].skin & 0x01)].experience;
        if (map[y].skin & 0x01) {
          enemies[num_enemies].rt_index = 50;
        } else {
          enemies[num_enemies].rt_index = 24;
        }
        break;
      case 0x81: // Garanz
        enemies[num_enemies].rt_index = 25;
        enemies[num_enemies].experience = battle_params[0x1D].experience;
        break;
      case 0x82: // Sinow Beat and Gold
        enemies[num_enemies].rt_index = 26 + ((map[y].reserved[10] & 0x800000) ? 1 : 0);
        if (map[y].reserved[10] & 0x800000) {
          enemies[num_enemies].experience = battle_params[0x13].experience;
        } else {
          enemies[num_enemies].experience = battle_params[0x06].experience;
        }
        if (map[y].num_clones == 0) {
          num_clones = 4; // only if no clone # present
        }
        break;
      case 0x83: // Canadine
        enemies[num_enemies].rt_index = 28;
        enemies[num_enemies].experience = battle_params[0x07].experience;
        break;
      case 0x84: // Canadine Group
        enemies[num_enemies].rt_index = 29;
        enemies[num_enemies].experience = battle_params[0x09].experience;
        for (size_t x = 1; x < 9; x++) {
          enemies[num_enemies + x].rt_index = 28;
          enemies[num_enemies + x].experience = battle_params[0x08].experience;
        }
        num_enemies += 8;
        break;
      case 0x85: // Dubwitch
        break;
      case 0xA0: // Delsaber
        enemies[num_enemies].rt_index = 30;
        enemies[num_enemies].experience = battle_params[0x52].experience;
        break;
      case 0xA1: // Chaos Sorcerer + 2 Bits
        enemies[num_enemies].rt_index = 31;
        enemies[num_enemies].experience = battle_params[0x0A].experience;
        num_enemies += 2;
        break;
      case 0xA2: // Dark Gunner
        enemies[num_enemies].rt_index = 34;
        enemies[num_enemies].experience = battle_params[0x1E].experience;
        break;
      case 0xA4: // Chaos Bringer
        enemies[num_enemies].rt_index = 36;
        enemies[num_enemies].experience = battle_params[0x0D].experience;
        break;
      case 0xA5: // Dark Belra
        enemies[num_enemies].rt_index = 37;
        enemies[num_enemies].experience = battle_params[0x0E].experience;
        break;
      case 0xA6: // Dimenian family
        enemies[num_enemies].rt_index = 41 + (map[y].skin % 3);
        enemies[num_enemies].experience = battle_params[0x53 + (map[y].skin % 3)].experience;
        break;
      case 0xA7: // Bulclaw + 4 claws
        enemies[num_enemies].rt_index = 40;
        enemies[num_enemies].experience = battle_params[0x1F].experience;
        for (size_t x = 1; x < 5; x++) {
          enemies[num_enemies + x].rt_index = 38;
          enemies[num_enemies + x].experience = battle_params[0x20].experience;
        }
        num_enemies += 4;
        break;
      case 0xA8: // Claw
        enemies[num_enemies].rt_index = 38;
        enemies[num_enemies].experience = battle_params[0x20].experience;
        break;
      case 0xC0: // Dragon or Gal Gryphon
        if (episode == 1) {
          enemies[num_enemies].rt_index = 44;
          enemies[num_enemies].experience = battle_params[0x12].experience;
        } else if (episode == 0x02) {
          enemies[num_enemies].rt_index = 77;
          enemies[num_enemies].experience = battle_params[0x1E].experience;
        }
        break;
      case 0xC1: // De Rol Le
        enemies[num_enemies].rt_index = 45;
        enemies[num_enemies].experience = battle_params[0x0F].experience;
        break;
      case 0xC2: // Vol Opt form 1
        break;
      case 0xC5: // Vol Opt form 2
        enemies[num_enemies].rt_index = 46;
        enemies[num_enemies].experience = battle_params[0x25].experience;
        break;
      case 0xC8: // Dark Falz + 510 Helpers
        enemies[num_enemies].rt_index = 47;
        if (difficulty) {
          enemies[num_enemies].experience = battle_params[0x38].experience; // Form 2
        } else {
          enemies[num_enemies].experience = battle_params[0x37].experience;
        }
        for (size_t x = 1; x < 511; x++) {
          //enemies[num_enemies + x].base = 200;
          enemies[num_enemies + x].experience = battle_params[0x35].experience;
        }
        num_enemies += 510;
        break;
      case 0xCA: // Olga Flow
        enemies[num_enemies].rt_index = 78;
        enemies[num_enemies].experience = battle_params[0x2C].experience;
        num_enemies += 512;
        break;
      case 0xCB: // Barba Ray
        enemies[num_enemies].rt_index = 73;
        enemies[num_enemies].experience = battle_params[0x0F].experience;
        num_enemies += 47;
        break;
      case 0xCC: // Gol Dragon
        enemies[num_enemies].rt_index = 76;
        enemies[num_enemies].experience = battle_params[0x12].experience;
        num_enemies += 5;
        break;
      case 0xD4: // Sinow Berill & Spigell
        enemies[num_enemies].rt_index = 62 + ((map[y].reserved[10] & 0x800000) ? 1 : 0);
        enemies[num_enemies].experience = battle_params[(map[y].reserved[10] & 0x800000) ? 0x13 : 0x06].experience;
        num_enemies += 4; // Add 4 clones which are never used...
        break;
      case 0xD5: // Merillia & Meriltas
        enemies[num_enemies].rt_index = 52 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x4B + (map[y].skin & 0x01)].experience;
        break;
      case 0xD6: // Mericus, Merikle, & Mericarol
        enemies[num_enemies].rt_index = 56 + (map[y].skin % 3);
        if (map[y].skin) {
          enemies[num_enemies].experience = battle_params[0x44 + (map[y].skin % 3)].experience;
        } else {
          enemies[num_enemies].experience = battle_params[0x3A].experience;
        }
        break;
      case 0xD7: // Ul Gibbon and Zol Gibbon
        enemies[num_enemies].rt_index = 59 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x3B + (map[y].skin & 0x01)].experience;
        break;
      case 0xD8: // Gibbles
        enemies[num_enemies].rt_index = 61;
        enemies[num_enemies].experience = battle_params[0x3D].experience;
        break;
      case 0xD9: // Gee
        enemies[num_enemies].rt_index = 54;
        enemies[num_enemies].experience = battle_params[0x07].experience;
        break;
      case 0xDA: // Gi Gue
        enemies[num_enemies].rt_index = 55;
        enemies[num_enemies].experience = battle_params[0x1A].experience;
        break;
      case 0xDB: // Deldepth
        enemies[num_enemies].rt_index = 71;
        enemies[num_enemies].experience = battle_params[0x30].experience;
        break;
      case 0xDC: // Delbiter
        enemies[num_enemies].rt_index = 72;
        enemies[num_enemies].experience = battle_params[0x0D].experience;
        break;
      case 0xDD: // Dolmolm and Dolmdarl
        enemies[num_enemies].rt_index = 64 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x4F + (map[y].skin & 0x01)].experience;
        break;
      case 0xDE: // Morfos
        enemies[num_enemies].rt_index = 66;
        enemies[num_enemies].experience = battle_params[0x40].experience;
        break;
      case 0xDF: // Recobox & Recons
        enemies[num_enemies].rt_index = 67;
        enemies[num_enemies].experience = battle_params[0x41].experience;
        for (size_t x = 1; x <= map[y].num_clones; x++) {
          enemies[num_enemies + x].rt_index = 68;
          enemies[num_enemies + x].experience = battle_params[0x42].experience;
        }
        break;
      case 0xE0: // Epsilon, Sinow Zoa and Zele
        if ((episode == 0x02) && (alt_enemies)) {
          enemies[num_enemies].rt_index = 84;
          enemies[num_enemies].experience = battle_params[0x23].experience;
          num_enemies += 4;
        } else {
          enemies[num_enemies].rt_index = 69 + (map[y].skin & 0x01);
          enemies[num_enemies].experience = battle_params[0x43 + (map[y].skin & 0x01)].experience;
        }
        break;
      case 0xE1: // Ill Gill
        enemies[num_enemies].rt_index = 82;
        enemies[num_enemies].experience = battle_params[0x26].experience;
        break;
      case 0x0110: // Astark
        enemies[num_enemies].rt_index = 1;
        enemies[num_enemies].experience = battle_params[0x09].experience;
        break;
      case 0x0111: // Satellite Lizard and Yowie
        enemies[num_enemies].rt_index = 2 + ((map[y].reserved[10] & 0x800000) ? 0 : 1);
        enemies[num_enemies].experience = battle_params[0x0D + ((map[y].reserved[10] & 0x800000) ? 1 : 0) + (alt_enemies ? 0x10 : 0)].experience;
        break;
      case 0x0112: // Merissa A/AA
        enemies[num_enemies].rt_index = 4 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x19 + (map[y].skin & 0x01)].experience;
        break;
      case 0x0113: // Girtablulu
        enemies[num_enemies].rt_index = 6;
        enemies[num_enemies].experience = battle_params[0x1F].experience;
        break;
      case 0x0114: // Zu and Pazuzu
        enemies[num_enemies].rt_index = 7 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x0B + (map[y].skin & 0x01) + (alt_enemies ? 0x14: 0x00)].experience;
        break;
      case 0x0115: // Boota family
        enemies[num_enemies].rt_index = 9 + (map[y].skin % 3);
        if (map[y].skin & 2) {
          enemies[num_enemies].experience = battle_params[0x03].experience;
        } else {
          enemies[num_enemies].experience = battle_params[0x00 + (map[y].skin % 3)].experience;
        }
        break;
      case 0x0116: // Dorphon and Eclair
        enemies[num_enemies].rt_index = 12 + (map[y].skin & 0x01);
        enemies[num_enemies].experience = battle_params[0x0F + (map[y].skin & 0x01)].experience;
        break;
      case 0x0117: // Goran family
        if (map[y].skin & 0x02) {
          enemies[num_enemies].rt_index = 15;
        } else if (map[y].skin & 0x01) {
          enemies[num_enemies].rt_index = 16;
        } else {
          enemies[num_enemies].rt_index = 14;
        }
        enemies[num_enemies].experience = battle_params[0x11 + (map[y].skin % 3)].experience;
        break;
      case 0x0119: // Saint Million, Shambertin, and Kondrieu
        if (map[y].reserved[10] & 0x800000) {
          enemies[num_enemies].rt_index = 21;
        } else {
          enemies[num_enemies].rt_index = 19 + (map[y].skin & 0x01);
        }
        enemies[num_enemies].experience = battle_params[0x22].experience;
        break;
      default:
        enemies[num_enemies].experience = 0xFFFFFFFF;
        log(WARNING, "Unknown enemy type %08" PRIX32 " %08" PRIX32, map[y].base,
            map[y].skin);
        break;
      }
      if (num_clones) {
        num_enemies += num_clones;
      }
      num_enemies++;
  }

  return enemies;
}

vector<PSOEnemy> load_map(const char* filename, uint8_t episode,
    uint8_t difficulty, const BattleParams* battle_params, bool alt_enemies) {
  shared_ptr<const string> data = file_cache.get(filename);
  const EnemyEntry* entries = reinterpret_cast<const EnemyEntry*>(data->data());
  size_t entry_count = data->size() / sizeof(EnemyEntry);
  return parse_map(episode, difficulty, battle_params, entries, entry_count,
      alt_enemies);
}
