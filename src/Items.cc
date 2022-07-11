#include "Items.hh"

#include <string.h>

#include <phosg/Random.hh>

using namespace std;



uint32_t random_int(shared_ptr<mt19937> rand, uint32_t min, uint32_t max) {
  uint32_t range = max - min + 1;
  return min + ((*rand)() % range);
}



////////////////////////////////////////////////////////////////////////////////

/* these items all need some kind of special handling that hasn't been implemented yet.

030B04 = TP Material (?)
030C00 = Cell Of MAG 502
030C01 = Cell Of MAG 213
030C02 = Parts Of RoboChao
030C03 = Heart Of Opa Opa
030C04 = Heart Of Pian
030C05 = Heart Of Chao

030D00 = Sorcerer's Right Arm
030D01 = S-beat's Arms
030D02 = P-arm's Arms
030D03 = Delsaber's Right Arm
030D04 = C-bringer's Right Arm
030D05 = Delsaber's Left Arm
030D06 = S-red's Arms
030D07 = Dragon's Claw
030D08 = Hildebear's Head
030D09 = Hildeblue's Head
030D0A = Parts of Baranz
030D0B = Belra's Right Arms
030D0C = GIGUE'S ARMS
030D0D = S-BERILL'S ARMS
030D0E = G-ASSASIN'S ARMS
030D0F = BOOMA'S RIGHT ARMS
030D10 = GOBOOMA'S RIGHT ARMS
030D11 = GIGOBOOMA'S RIGHT ARMS
030D12 = GAL WIND
030D13 = RAPPY'S WING

030E00 = BERILL PHOTON
030E01 = PARASITIC GENE FLOW
030E02 = MAGICSTONE IRITISTA
030E03 = BLUE BLACK STONE
030E04 = SYNCESTA
030E05 = MAGIC WATER
030E06 = PARASITIC CELL TYPE D
030E07 = MAGIC ROCK HEART KEY
030E08 = MAGIC ROCK MOOLA
030E09 = STAR AMPLIFIER
030E0A = BOOK OF HITOGATA
030E0B = HEART OF CHU CHU
030E0C = PART OF EGG BLASTER
030E0D = HEART OF ANGLE
030E0E = HEART OF DEVIL
030E0F = KIT OF HAMBERGER
030E10 = PANTHER'S SPIRIT
030E11 = KIT OF MARK3
030E12 = KIT OF MASTER SYSTEM
030E13 = KIT OF GENESIS
030E14 = KIT OF SEGA SATURN
030E15 = KIT OF DREAMCAST
030E16 = AMP. RESTA
030E17 = AMP. ANTI
030E18 = AMP. SHIFTA
030E19 = AMP. DEBAND
030E1A = AMP.
030E1B = AMP.
030E1C = AMP.
030E1D = AMP.
030E1E = AMP.
030E1F = AMP.
030E20 = AMP.
030E21 = AMP.
030E22 = AMP.
030E23 = AMP.
030E24 = AMP.
030E25 = AMP.
030E26 = HEART OF KAPUKAPU
030E27 = PROTON BOOSTER
030F00 = ADD SLOT
031000 = PHOTON DROP
031001 = PHOTON SPHERE
031002 = PHOTON CRYSTAL
031100 = BOOK OF KATANA 1
031101 = BOOK OF KATANA 2
031102 = BOOK OF KATANA 3
031200 = WEAPONS BRONZE BADGE
031201 = WEAPONS SILVER BADGE
031202 = WEAPONS GOLD BADGE
031203 = WEAPONS CRYSTAL BADGE
031204 = WEAPONS STEEL BADGE
031205 = WEAPONS ALUMINUM BADGE
031206 = WEAPONS LEATHER BADGE
031207 = WEAPONS BONE BADGE
031208 = LETTER OF APPRECATION
031209 = AUTOGRAPH ALBUM
03120A = VALENTINE'S CHOCOLATE
03120B = NEWYEAR'S CARD
03120C = CRISMAS CARD
03120D = BIRTHDAY CARD
03120E = PROOF OF SONIC TEAM
03120F = SPECIAL EVENT TICKET
031300 = PRESENT
031400 = CHOCOLATE
031401 = CANDY
031402 = CAKE
031403 = SILVER BADGE
031404 = GOLD BADGE
031405 = CRYSTAL BADGE
031406 = IRON BADGE
031407 = ALUMINUM BADGE
031408 = LEATHER BADGE
031409 = BONE BADGE
03140A = BONQUET
03140B = DECOCTION
031500 = CRISMAS PRESENT
031501 = EASTER EGG
031502 = JACK-O'S-LANTERN
031700 = HUNTERS REPORT
031701 = HUNTERS REPORT RANK A
031702 = HUNTERS REPORT RANK B
031703 = HUNTERS REPORT RANK C
031704 = HUNTERS REPORT RANK F
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031705 = HUNTERS REPORT
031802 = Dragon Scale
031803 = Heaven Striker Coat
031807 = Rappys Beak
031802 = Dragon Scale */

////////////////////////////////////////////////////////////////////////////////

void player_use_item(shared_ptr<Client> c, size_t item_index) {
  auto player = c->game_data.player();

  ssize_t equipped_weapon = -1;
  // ssize_t equipped_armor = -1;
  // ssize_t equipped_shield = -1;
  // ssize_t equipped_mag = -1;
  for (size_t y = 0; y < c->game_data.player()->inventory.num_items; y++) {
    if (c->game_data.player()->inventory.items[y].equip_flags & 0x0008) {
      if (c->game_data.player()->inventory.items[y].data.data1[0] == 0) {
        equipped_weapon = y;
      }
      // else if ((c->game_data.player()->inventory.items[y].data.data1[0] == 1) &&
      //            (c->game_data.player()->inventory.items[y].data.data1[1] == 1)) {
      //   equipped_armor = y;
      // } else if ((c->game_data.player()->inventory.items[y].data.data1[0] == 1) &&
      //            (c->game_data.player()->inventory.items[y].data.data1[1] == 2)) {
      //   equipped_shield = y;
      // } else if (c->game_data.player()->inventory.items[y].data.data1[0] == 2) {
      //   equipped_mag = y;
      // }
    }
  }

  bool should_delete_item = true;

  auto& item = c->game_data.player()->inventory.items[item_index];
  if (item.data.data1w[0] == 0x0203) { // technique disk
    c->game_data.player()->disp.technique_levels.data()[item.data.data1[4]] = item.data.data1[2];

  } else if (item.data.data1w[0] == 0x0A03) { // grinder
    if (equipped_weapon < 0) {
      throw invalid_argument("grinder used with no weapon equipped");
    }
    if (item.data.data1[2] > 2) {
      throw invalid_argument("incorrect grinder value");
    }
    c->game_data.player()->inventory.items[equipped_weapon].data.data1[3] += (item.data.data1[2] + 1);
    // TODO: we should check for max grind here

  } else if (item.data.data1w[0] == 0x0B03) { // material
    switch (item.data.data1[2]) {
      case 0: // Power Material
        c->game_data.player()->disp.stats.atp += 2;
        break;
      case 1: // Mind Material
        c->game_data.player()->disp.stats.mst += 2;
        break;
      case 2: // Evade Material
        c->game_data.player()->disp.stats.evp += 2;
        break;
      case 3: // HP Material
        c->game_data.player()->inventory.hp_materials_used += 2;
        break;
      case 4: // TP Material
        c->game_data.player()->inventory.tp_materials_used += 2;
        break;
      case 5: // Def Material
        c->game_data.player()->disp.stats.dfp += 2;
        break;
      case 6: // Luck Material
        c->game_data.player()->disp.stats.lck += 2;
        break;
      default:
        throw invalid_argument("unknown material used");
    }

  } else {
    // default item action is to unwrap the item if it's a present
    if ((item.data.data1[0] == 2) && (item.data.data2[2] & 0x40)) {
      item.data.data2[2] &= 0xBF;
      should_delete_item = false;
    } else if ((item.data.data1[0] != 2) && (item.data.data1[4] & 0x40)) {
      item.data.data1[4] &= 0xBF;
      should_delete_item = false;
    }
  }

  if (should_delete_item) {
    c->game_data.player()->remove_item(item.data.id, 1);
  }
}

////////////////////////////////////////////////////////////////////////////////

CommonItemData::CommonItemData(
    vector<uint32_t>&& enemy_item_categories,
    vector<uint32_t>&& box_item_categories,
    vector<vector<uint8_t>>&& unit_types) :
    enemy_item_categories(move(enemy_item_categories)),
    box_item_categories(move(box_item_categories)),
    unit_types(move(unit_types)) {

  // sanity check the values
  if (this->enemy_item_categories.size() != 8) {
    throw invalid_argument("enemy item categories is incorrect length");
  }
  if (this->box_item_categories.size() != 8) {
    throw invalid_argument("box item categories is incorrect length");
  }
  if (this->unit_types.size() != 4) {
    throw invalid_argument("unit types is incorrect length");
  }

  {
    uint64_t sum = 0;
    for (uint32_t v : this->enemy_item_categories) {
      sum += v;
    }
    if (sum > 0xFFFFFFFF) {
      throw invalid_argument("enemy item category sum is too large");
    }
  }

  {
    uint64_t sum = 0;
    for (uint32_t v : this->box_item_categories) {
      sum += v;
    }
    if (sum > 0xFFFFFFFF) {
      throw invalid_argument("box item category sum is too large");
    }
  }
}

CommonItemCreator::CommonItemCreator(
    std::shared_ptr<const CommonItemData> data,
    std::shared_ptr<std::mt19937> random)
  : data(data), random(random) { }

int32_t CommonItemCreator::decide_item_type(bool is_box) const {
  uint32_t det = (*this->random)();

  const auto& v = is_box ? this->data->box_item_categories : this->data->enemy_item_categories;
  for (size_t x = 0; x < v.size(); x++) {
    uint32_t probability = v.at(x);
    if (probability > det) {
      return x;
    }
    det -= probability;
  }
  return -1;
}

ItemData CommonItemCreator::create_drop_item(bool is_box, uint8_t episode,
    uint8_t difficulty, uint8_t area, uint8_t) const {
  // TODO: use the section ID (last argument) to vary drop frequencies appropriately
  // change the area if it's invalid (data for the bosses are actually in other areas)
  if (area > 10) {
    if (episode == 1) {
      if (area == 11) {
        area = 3; // dragon
      } else if (area == 12) {
        area = 6; // de rol le
      } else if (area == 13) {
        area = 8; // vol opt
      } else if (area == 14) {
        area = 10; // dark falz
      } else {
        area = 1; // unknown area -> forest 1
      }
    } else if (episode == 2) {
      if (area == 12) {
        area = 9; // gal gryphon
      } else if (area == 13) {
        area = 10; // olga flow
      } else if (area == 14) {
        area = 3; // barba ray
      } else if (area == 15) {
        area = 6; // gol dragon
      } else {
        area = 10; // tower
      }
    } else if (episode == 3) {
      area = 1;
    }
  }

  ItemData item;

  // picks a random non-rare item type, then gives it appropriate random stats
  // modify some of the constants in this section to change the system's
  // parameters
  int32_t type = this->decide_item_type(is_box);
  switch (type) {
    case 0x00: // material
      item.data1[0] = 0x03;
      item.data1[1] = 0x0B;
      item.data1[2] = random_int(this->random, 0, 6);
      break;

    case 0x01: // equipment
      switch (random_int(this->random, 0, 3)) {
        case 0x00: // weapon
          item.data1[1] = random_int(this->random, 1, 12); // random normal class
          item.data1[2] = difficulty + random_int(this->random, 0, 2); // special type
          if ((item.data1[1] > 0x09) && (item.data1[2] > 0x04)) {
            item.data1[2] = 0x04; // no special classes above 4
          }
          item.data1[4] = 0x80; // untekked
          if (item.data1[2] < 0x04) {
            item.data1[4] |= random_int(this->random, 0, 40); // give a special
          }
          for (size_t x = 0, y = 0; (x < 5) && (y < 3); x++) { // percentages
            if (random_int(this->random, 0, 10) == 1) { // 1/11 chance of getting each type of percentage
              item.data1[6 + (y * 2)] = x + 1;
              item.data1[7 + (y * 2)] = random_int(this->random, 0, 10) * 5;
              y++;
            }
          }
          break;

        case 0x01: // armor
          item.data1[0] = 0x01;
          item.data1[1] = 0x01;
          item.data1[2] = (6 * difficulty) + random_int(this->random, 0, ((area / 2) + 2) - 1); // standard type based on difficulty and area
          if (item.data1[2] > 0x17) {
            item.data1[2] = 0x17; // no standard types above 0x17
          }
          if (random_int(this->random, 0, 10) == 0) { // +/-
            item.data1[4] = random_int(this->random, 0, 5);
            item.data1[6] = random_int(this->random, 0, 2);
          }
          item.data1[5] = random_int(this->random, 0, 4); // slots
          break;

        case 0x02: // shield
          item.data1[0] = 0x01;
          item.data1[1] = 0x02;
          item.data1[2] = (5 * difficulty) + random_int(this->random, 0, ((area / 2) + 2) - 1); // standard type based on difficulty and area
          if (item.data1[2] > 0x14) {
            item.data1[2] = 0x14; // no standard types above 0x14
          }
          if (random_int(this->random, 0, 10) == 0) { // +/-
            item.data1[4] = random_int(this->random, 0, 5);
            item.data1[6] = random_int(this->random, 0, 5);
          }
          break;

        case 0x03: { // unit
          const auto& type_table = this->data->unit_types.at(difficulty);
          uint8_t type = type_table[random_int(this->random, 0, type_table.size() - 1)];
          if (type == 0xFF) {
            throw out_of_range("no item dropped"); // 0xFF -> no item drops
          }
          item.data1[0] = 0x01;
          item.data1[1] = 0x03;
          item.data1[2] = type;
          break;
        }
      }
      break;

    case 0x02: // technique
      item.data1[0] = 0x03;
      item.data1[1] = 0x02;
      item.data1[4] = random_int(this->random, 0, 18); // tech type
      if ((item.data1[4] != 14) && (item.data1[4] != 17)) { // if not ryuker or reverser, give it a level
        if (item.data1[4] == 16) { // if not anti, give it a level between 1 and 30
          if (area > 3) {
            item.data1[2] = difficulty + random_int(this->random, 0, ((area - 1) / 2) - 1);
          } else {
            item.data1[2] = difficulty;
          }
          if (item.data1[2] > 6) {
            item.data1[2] = 6;
          }
        } else {
          item.data1[2] = (5 * difficulty) + random_int(this->random, 0, ((area * 3) / 2) - 1); // else between 1 and 7
        }
      }
      break;

    case 0x03: // scape doll
      item.data1[0] = 0x03;
      item.data1[1] = 0x09;
      item.data1[2] = 0x00;
      break;

    case 0x04: // grinder
      item.data1[0] = 0x03;
      item.data1[1] = 0x0A;
      item.data1[2] = random_int(this->random, 0, 2); // mono, di, tri
      break;

    case 0x05: // consumable
      item.data1[0] = 0x03;
      item.data1[5] = 0x01;
      switch (random_int(this->random, 0, 2)) {
        case 0: // antidote / antiparalysis
          item.data1[1] = 6;
          item.data1[2] = random_int(this->random, 0, 1);
          break;

        case 1: // telepipe / trap vision
          item.data1[1] = 7 + random_int(this->random, 0, 1);
          break;

        case 2: // sol / moon / star atomizer
          item.data1[1] = 3 + random_int(this->random, 0, 2);
          break;
      }
      break;

    case 0x06: // consumable
      item.data1[0] = 0x03;
      item.data1[5] = 0x01;
      item.data1[1] = random_int(this->random, 0, 1); // mate or fluid
      if (difficulty == 0) {
        item.data1[2] = random_int(this->random, 0, 1); // only mono and di on normal
      } else if (difficulty == 3) {
        item.data1[2] = random_int(this->random, 1, 2); // only di and tri on ultimate
      } else {
        item.data1[2] = random_int(this->random, 0, 2); // else, any of the three
      }
      break;

    case 0x07: // meseta
      item.data1[0] = 0x04;
      item.data2d = (90 * difficulty) + (random_int(this->random, 1, 20) * (area * 2)); // meseta amount
      break;

    default:
      throw out_of_range("no item created");
  }

  return item;
}


ItemData CommonItemCreator::create_shop_item(uint8_t difficulty,
    uint8_t item_type) const {
  static const uint8_t max_percentages[4] = {20, 35, 45, 50};
  static const uint8_t max_quantity[4] =    { 1,  1,  2,  2};
  static const uint8_t max_tech_level[4] =  { 8, 15, 23, 30};
  static const uint8_t max_anti_level[4] =  { 2,  4,  6,  7};

  ItemData item;

  item.data1[0] = item_type;
  while (item.data1[0] == 2) {
    item.data1[0] = rand() % 3;
  }
  switch (item.data1[0]) {
    case 0: { // weapon
      item.data1[1] = (rand() % 12) + 1;
      if (item.data1[1] > 9) {
        item.data1[2] = difficulty;
      } else {
        item.data1[2] = (rand() & 1) + difficulty;
      }

      item.data1[3] = rand() % 11;
      item.data1[4] = rand() % 11;

      size_t num_percentages = 0;
      for (size_t x = 0; (x < 5) && (num_percentages < 3); x++) {
        if ((rand() % 4) == 1) {
          item.data1[(num_percentages * 2) + 6] = x;
          item.data1[(num_percentages * 2) + 7] = rand() % (max_percentages[difficulty] + 1);
          num_percentages++;
        }
      }
      break;
    }

    case 1: // armor
      item.data1[1] = 0;
      while (item.data1[1] == 0) {
        item.data1[1] = rand() & 3;
      }
      switch (item.data1[1]) {
        case 1:
          item.data1[2] = (rand() % 6) + (difficulty * 6);
          item.data1[5] = rand() % 5;
          break;
        case 2:
          item.data2[2] = (rand() % 6) + (difficulty * 5);
          *reinterpret_cast<short*>(&item.data1[6]) = (rand() % 9) - 4;
          *reinterpret_cast<short*>(&item.data1[9]) = (rand() % 9) - 4;
          break;
        case 3:
          item.data2[2] = rand() % 0x3B;
          *reinterpret_cast<short*>(&item.data1[7]) = (rand() % 5) - 4;
          break;
      }
      break;

    case 3: // tool
      item.data1[1] = rand() % 12;
      switch (item.data1[1]) {
        case 0:
        case 1:
          if (difficulty == 0) {
            item.data1[2] = 0;
          } else if (difficulty == 1) {
            item.data1[2] = rand() % 2;
          } else if (difficulty == 2) {
            item.data1[2] = (rand() % 2) + 1;
          } else if (difficulty == 3) {
            item.data1[2] = 2;
          }
          break;

        case 6:
          item.data1[2] = rand() % 2;
          break;

        case 10:
          item.data1[2] = rand() % 3;
          break;

        case 11:
          item.data1[2] = rand() % 7;
          break;
      }

      switch (item.data1[1]) {
        case 2:
          item.data1[4] = rand() % 19;
          switch (item.data1[4]) {
            case 14:
            case 17:
              item.data1[2] = 0; // reverser & ryuker always level 1 
              break;
            case 16:
              item.data1[2] = rand() % max_anti_level[difficulty];
              break;
            default:
              item.data1[2] = rand() % max_tech_level[difficulty];
          }
          break;
        case 0:
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 16:
          item.data1[5] = rand() % (max_quantity[difficulty] + 1);
          break;
      }
  }

  return item;
}
