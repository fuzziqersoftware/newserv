#include "Items.hh"

#include <string.h>

#include <phosg/Random.hh>

using namespace std;

/* These items all need some kind of special handling that hasn't been implemented yet.

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
    if (c->game_data.player()->inventory.items[y].flags & 0x00000008) {
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

  // On PC (and presumably DC), the client sends a 6x29 after this to delete the
  // used item. On GC and later versions, this does not happen, so we should
  // delete the item here.
  bool should_delete_item = (c->version() != GameVersion::DC) &&
                            (c->version() != GameVersion::PC);

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
    // Allow overdrafting meseta if the client is not BB, since the server isn't
    // informed when meseta is added or removed from the bank.
    c->game_data.player()->remove_item(item.data.id, 1, c->version() != GameVersion::BB);
  }
}
