#include "Items.hh"

#include <string.h>

#include <phosg/Random.hh>

#include "SendCommands.hh"

using namespace std;

void player_use_item(shared_ptr<ServerState> s, shared_ptr<Client> c, size_t item_index) {
  // On PC (and presumably DC), the client sends a 6x29 after this to delete the
  // used item. On GC and later versions, this does not happen, so we should
  // delete the item here.
  bool should_delete_item = (c->version() != GameVersion::DC) && (c->version() != GameVersion::PC);

  auto player = c->game_data.player();
  auto& item = player->inventory.items[item_index];
  uint32_t item_identifier = item.data.primary_identifier();

  if (item.data.is_common_consumable()) { // Monomate, etc.
    // Nothing to do (it should be deleted)

  } else if (item_identifier == 0x030200) { // Technique disk
    uint8_t max_level = s->item_parameter_table->get_max_tech_level(player->disp.char_class, item.data.data1[4]);
    if (item.data.data1[2] > max_level) {
      throw runtime_error("technique level too high");
    }
    player->disp.technique_levels.data()[item.data.data1[4]] = item.data.data1[2];

  } else if ((item_identifier & 0xFFFF00) == 0x030A00) { // Grinder
    if (item.data.data1[2] > 2) {
      throw invalid_argument("incorrect grinder value");
    }
    auto& weapon = player->inventory.items[player->inventory.find_equipped_weapon()];
    auto weapon_def = s->item_parameter_table->get_weapon(
        weapon.data.data1[1], weapon.data.data1[2]);
    if (weapon.data.data1[3] >= weapon_def.max_grind) {
      throw runtime_error("weapon already at maximum grind");
    }
    weapon.data.data1[3] += (item.data.data1[2] + 1);

  } else if ((item_identifier & 0xFFFF00) == 0x030B00) { // Material
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

  } else if ((item_identifier & 0xFFFF00) == 0x030F00) { // AddSlot
    auto& armor = player->inventory.items[player->inventory.find_equipped_armor()];
    if (armor.data.data1[5] >= 4) {
      throw runtime_error("armor already at maximum slot count");
    }
    armor.data.data1[5]++;

  } else if ((item.data.data1[0] == 0x02) && (item.data.data2[2] & 0x40)) {
    // Unwrap mag present
    item.data.data2[2] &= 0xBF;
    should_delete_item = false;

  } else if ((item.data.data1[0] != 0x02) && (item.data.data1[4] & 0x40)) {
    // Unwrap non-mag present
    item.data.data1[4] &= 0xBF;
    should_delete_item = false;

  } else if (item_identifier == 0x003300) {
    // Unseal Sealed J-Sword => Tsumikiri J-Sword
    item.data.data1[1] = 0x32;
    should_delete_item = false;

  } else if (item_identifier == 0x00AB00) {
    // Unseal Lame d'Argent => Excalibur
    item.data.data1[1] = 0xAC;
    should_delete_item = false;

  } else if (item_identifier == 0x01034D) {
    // Unseal Limiter => Adept
    item.data.data1[2] = 0x4E;
    should_delete_item = false;

  } else if (item_identifier == 0x01034F) {
    // Unseal Swordsman Lore => Proof of Sword-Saint
    item.data.data1[2] = 0x50;
    should_delete_item = false;

  } else if (item_identifier == 0x030C00) {
    // Cell of MAG 502
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = (player->disp.section_id & 1) ? 0x1D : 0x21;

  } else if (item_identifier == 0x030C01) {
    // Cell of MAG 213
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = (player->disp.section_id & 1) ? 0x27 : 0x22;

  } else if (item_identifier == 0x030C02) {
    // Parts of RoboChao
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = 0x28;

  } else if (item_identifier == 0x030C03) {
    // Heart of Opa Opa
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = 0x29;

  } else if (item_identifier == 0x030C04) {
    // Heart of Pian
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = 0x2A;

  } else if (item_identifier == 0x030C05) {
    // Heart of Chao
    auto& mag = player->inventory.items[player->inventory.find_equipped_mag()];
    mag.data.data1[1] = 0x2B;

  } else if ((item_identifier & 0xFFFF00) == 0x031500) {
    // Christmas Present, etc. - use unwrap_table + probabilities therein
    auto table = s->item_parameter_table->get_event_items(item.data.data1[2]);
    size_t sum = 0;
    for (size_t z = 0; z < table.second; z++) {
      sum += table.first[z].probability;
    }
    if (sum == 0) {
      throw runtime_error("no unwrap results available for event");
    }
    size_t det = random_object<size_t>() % sum;
    for (size_t z = 0; z < table.second; z++) {
      const auto& entry = table.first[z];
      if (det > entry.probability) {
        det -= entry.probability;
      } else {
        item.data.data2d = 0;
        item.data.data1[0] = entry.item[0];
        item.data.data1[1] = entry.item[1];
        item.data.data1[2] = entry.item[2];
        item.data.data1.clear_after(3);
        should_delete_item = false;

        auto l = s->find_lobby(c->lobby_id);
        send_create_inventory_item(l, c, item.data);
        break;
      }
    }

  } else {
    // Use item combinations table from ItemPMT
    bool combo_applied = false;
    for (size_t z = 0; z < player->inventory.num_items; z++) {
      auto& inv_item = player->inventory.items[z];
      if (!(inv_item.flags & 0x00000008)) {
        continue;
      }
      try {
        const auto& combo = s->item_parameter_table->get_item_combination(
            item.data, inv_item.data);
        if (combo.char_class != 0xFF && combo.char_class != player->disp.char_class) {
          throw runtime_error("item combination requires specific char_class");
        }
        if (combo.mag_level != 0xFF) {
          if (inv_item.data.data1[0] != 2) {
            throw runtime_error("item combination applies with mag level requirement, but equipped item is not a mag");
          }
          if (inv_item.data.compute_mag_level() < combo.mag_level) {
            throw runtime_error("item combination applies with mag level requirement, but equipped mag level is too low");
          }
        }
        if (combo.grind != 0xFF) {
          if (inv_item.data.data1[0] != 0) {
            throw runtime_error("item combination applies with grind requirement, but equipped item is not a weapon");
          }
          if (inv_item.data.data1[3] < combo.grind) {
            throw runtime_error("item combination applies with grind requirement, but equipped weapon grind is too low");
          }
        }
        if (combo.level != 0xFF && player->disp.level + 1 < combo.level) {
          throw runtime_error("item combination applies with level requirement, but player level is too low");
        }
        // If we get here, then the combo applies
        if (combo_applied) {
          throw runtime_error("multiple combinations apply");
        }
        combo_applied = true;

        inv_item.data.data1[0] = combo.result_item[0];
        inv_item.data.data1[1] = combo.result_item[1];
        inv_item.data.data1[2] = combo.result_item[2];
        inv_item.data.data1[3] = 0; // Grind
        inv_item.data.data1[4] = 0; // Flags + special
      } catch (const out_of_range&) {
      }
    }

    if (!combo_applied) {
      throw runtime_error("no combinations apply");
    }
  }

  if (should_delete_item) {
    // Allow overdrafting meseta if the client is not BB, since the server isn't
    // informed when meseta is added or removed from the bank.
    player->remove_item(item.data.id, 1, c->version() != GameVersion::BB);
  }
}

void player_feed_mag(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c, size_t mag_item_index, size_t fed_item_index) {
  static const unordered_map<uint32_t, size_t> result_index_for_fed_item({
      {0x030000, 0}, // Monomate
      {0x030001, 1}, // Dimate
      {0x030002, 2}, // Trimate
      {0x030100, 3}, // Monofluid
      {0x030101, 4}, // Difluid
      {0x030102, 5}, // Trifluid
      {0x030600, 6}, // Antidote
      {0x030601, 7}, // Antiparalysis
      {0x030300, 8}, // Sol Atomizer
      {0x030400, 9}, // Moon Atomizer
      {0x030500, 10}, // Star Atomizer
  });

  auto player = c->game_data.player();
  auto& fed_item = player->inventory.items[fed_item_index];
  auto& mag_item = player->inventory.items[mag_item_index];

  size_t result_index = result_index_for_fed_item.at(fed_item.data.primary_identifier());
  const auto& mag_def = s->item_parameter_table->get_mag(mag_item.data.data1[1]);
  const auto& feed_result = s->item_parameter_table->get_mag_feed_result(mag_def.feed_table, result_index);

  fprintf(stderr, "[feed-mag] table = %hu, index = %zu\n", mag_def.feed_table.load(), result_index);
  print_data(stderr, &feed_result, sizeof(feed_result));

  auto update_stat = +[](ItemData& data, size_t which, int8_t delta) -> void {
    uint16_t existing_stat = data.data1w[which] % 100;
    if ((delta > 0) || ((delta < 0) && (-delta < existing_stat))) {
      uint16_t level = data.compute_mag_level();
      if (level > 200) {
        throw runtime_error("mag level is too high");
      }
      if ((level == 200) && ((99 - existing_stat) < delta)) {
        delta = 99 - existing_stat;
      }
      data.data1w[which] += delta;
    }
  };

  auto print_mag_item = [&](const char* step) -> void {
    auto hex = mag_item.data.hex();
    fprintf(stderr, "[feed-mag] step: %s\n[feed-mag] %s\n", step, hex.c_str());
  };

  print_mag_item("pre");
  update_stat(mag_item.data, 2, feed_result.def);
  print_mag_item("update-def");
  update_stat(mag_item.data, 3, feed_result.pow);
  print_mag_item("update-pow");
  update_stat(mag_item.data, 4, feed_result.dex);
  print_mag_item("update-dex");
  update_stat(mag_item.data, 5, feed_result.mind);
  print_mag_item("update-mind");
  mag_item.data.data2[0] = clamp<ssize_t>(static_cast<ssize_t>(mag_item.data.data2[0]) + feed_result.synchro, 0, 120);
  print_mag_item("update-synchro");
  mag_item.data.data2[1] = clamp<ssize_t>(static_cast<ssize_t>(mag_item.data.data2[1]) + feed_result.iq, 0, 200);
  print_mag_item("update-iq");

  uint8_t mag_level = mag_item.data.compute_mag_level();
  mag_item.data.data1[2] = mag_level;
  print_mag_item("compute-level");
  uint8_t evolution_number = s->mag_evolution_table->get_evolution_number(mag_item.data.data1[1]);
  uint8_t mag_number = mag_item.data.data1[1];
  fprintf(stderr, "[feed-mag] evo_num = %02hhX, mag_num = %02hhX\n", evolution_number, mag_number);

  // Note: Sega really did just hardcode all these rules into the client. There
  // is no data file describing these evolutions, unfortunately.

  if (mag_level < 10) {
    // Nothing to do

  } else if (mag_level < 35) { // Level 10 evolution
    if (evolution_number < 1) {
      switch (player->disp.char_class) {
        case 0: // HUmar
        case 1: // HUnewearl
        case 2: // HUcast
        case 9: // HUcaseal
          mag_item.data.data1[1] = 0x01; // Varuna
          break;
        case 3: // RAmar
        case 11: // RAmarl
        case 4: // RAcast
        case 5: // RAcaseal
          mag_item.data.data1[1] = 0x0D; // Kalki
          break;
        case 10: // FOmar
        case 6: // FOmarl
        case 7: // FOnewm
        case 8: // FOnewearl
          mag_item.data.data1[1] = 0x19; // Vritra
          break;
        default:
          throw runtime_error("invalid character class");
      }
    }

  } else if (mag_level < 50) { // Level 35 evolution
    if (evolution_number < 2) {
      uint16_t flags = mag_item.data.compute_mag_strength_flags();
      if (mag_number == 0x0D) {
        if ((flags & 0x110) == 0) {
          mag_item.data.data1[1] = 0x02;
        } else if (flags & 8) {
          mag_item.data.data1[1] = 0x03;
        } else if (flags & 0x20) {
          mag_item.data.data1[1] = 0x0B;
        }
      } else if (mag_number == 1) {
        if (flags & 0x108) {
          mag_item.data.data1[1] = 0x0E;
        } else if (flags & 0x10) {
          mag_item.data.data1[1] = 0x0F;
        } else if (flags & 0x20) {
          mag_item.data.data1[1] = 0x04;
        }
      } else if (mag_number == 0x19) {
        if (flags & 0x120) {
          mag_item.data.data1[1] = 0x1A;
        } else if (flags & 8) {
          mag_item.data.data1[1] = 0x1B;
        } else if (flags & 0x10) {
          mag_item.data.data1[1] = 0x14;
        }
      }
    }

  } else if ((mag_level % 5) == 0) { // Level 50 (and beyond) evolutions
    if (evolution_number < 4) {

      if (mag_level >= 100) {
        uint8_t section_id_group = player->disp.section_id % 3;
        uint16_t def = mag_item.data.data1w[2] / 100;
        uint16_t pow = mag_item.data.data1w[3] / 100;
        uint16_t dex = mag_item.data.data1w[4] / 100;
        uint16_t mind = mag_item.data.data1w[5] / 100;
        bool is_male = char_class_is_male(player->disp.char_class);
        size_t table_index = (is_male ? 0 : 1) + section_id_group * 2;

        bool is_hunter = char_class_is_hunter(player->disp.char_class);
        bool is_ranger = char_class_is_ranger(player->disp.char_class);
        bool is_force = char_class_is_force(player->disp.char_class);
        if (is_force) {
          table_index += 12;
        } else if (is_ranger) {
          table_index += 6;
        } else if (!is_hunter) {
          throw logic_error("char class is not any of the top-level classes");
        }

        // Note: The original code checks the class (hunter/ranger/force) again
        // here, and goes into 3 branches that each do these same checks.
        // However, the result of all 3 branches is exactly the same!
        if (((section_id_group == 0) && (pow + mind == def + dex)) ||
            ((section_id_group == 1) && (pow + dex == mind + def)) ||
            ((section_id_group == 2) && (pow + def == mind + dex))) {
          // clang-format off
          static const uint8_t result_table[] = {
          //   M0    F0    M1    F1    M2    F2
              0x39, 0x3B, 0x3A, 0x3B, 0x3A, 0x3B, // Hunter
              0x3D, 0x3C, 0x3D, 0x3C, 0x3D, 0x3E, // Ranger
              0x41, 0x3F, 0x41, 0x40, 0x41, 0x40, // Force
          };
          // clang-format on
          mag_item.data.data1[1] = result_table[table_index];
        }
      }

      // If a special evolution did not occur, do a normal level 50 evolution
      if (mag_number == mag_item.data.data1[1]) {
        uint16_t flags = mag_item.data.compute_mag_strength_flags();
        uint16_t def = mag_item.data.data1w[2] / 100;
        uint16_t pow = mag_item.data.data1w[3] / 100;
        uint16_t dex = mag_item.data.data1w[4] / 100;
        uint16_t mind = mag_item.data.data1w[5] / 100;

        bool is_hunter = char_class_is_hunter(player->disp.char_class);
        bool is_ranger = char_class_is_ranger(player->disp.char_class);
        bool is_force = char_class_is_force(player->disp.char_class);
        if (is_hunter + is_ranger + is_force != 1) {
          throw logic_error("char class is not exactly one of the top-level classes");
        }

        if (is_hunter) {
          if (flags & 0x108) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((dex < mind) ? 0x08 : 0x06)
                : ((dex < mind) ? 0x0C : 0x05);
          } else if (flags & 0x010) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((mind < pow) ? 0x12 : 0x10)
                : ((mind < pow) ? 0x17 : 0x13);
          } else if (flags & 0x020) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((pow < dex) ? 0x16 : 0x24)
                : ((pow < dex) ? 0x07 : 0x1E);
          }
        } else if (is_ranger) {
          if (flags & 0x110) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((mind < pow) ? 0x0A : 0x05)
                : ((mind < pow) ? 0x0C : 0x06);
          } else if (flags & 0x008) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((dex < mind) ? 0x0A : 0x26)
                : ((dex < mind) ? 0x0C : 0x06);
          } else if (flags & 0x020) {
            mag_item.data.data1[1] = (player->disp.section_id & 1)
                ? ((pow < dex) ? 0x18 : 0x1E)
                : ((pow < dex) ? 0x08 : 0x05);
          }
        } else if (is_force) {
          if (flags & 0x120) {
            if (def < 45) {
              mag_item.data.data1[1] = (player->disp.section_id & 1)
                  ? ((pow < dex) ? 0x17 : 0x09)
                  : ((pow < dex) ? 0x1E : 0x1C);
            } else {
              mag_item.data.data1[1] = 0x24;
            }
          } else if (flags & 0x008) {
            if (def < 45) {
              mag_item.data.data1[1] = (player->disp.section_id & 1)
                  ? ((dex < mind) ? 0x1C : 0x20)
                  : ((dex < mind) ? 0x1F : 0x25);
            } else {
              mag_item.data.data1[1] = 0x23;
            }
          } else if (flags & 0x010) {
            if (def < 45) {
              mag_item.data.data1[1] = (player->disp.section_id & 1)
                  ? ((mind < pow) ? 0x12 : 0x0C)
                  : ((mind < pow) ? 0x15 : 0x11);
            } else {
              mag_item.data.data1[1] = 0x24;
            }
          }
        }
      }
    }
  }

  print_mag_item("evolution-check");

  // If the mag has evolved, add its new photon blast
  if (mag_number != mag_item.data.data1[1]) {
    const auto& new_mag_def = s->item_parameter_table->get_mag(mag_item.data.data1[1]);
    mag_item.data.add_mag_photon_blast(new_mag_def.photon_blast);
  }

  print_mag_item("add-pb");
}
