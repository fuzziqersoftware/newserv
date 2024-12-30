#include "Items.hh"

#include <string.h>

#include "SendCommands.hh"

using namespace std;

void player_use_item(shared_ptr<Client> c, size_t item_index, shared_ptr<PSOLFGEncryption> opt_rand_crypt) {
  auto s = c->require_server_state();

  // On PC (and presumably DC), the client sends a 6x29 after this to delete the
  // used item. On GC and later versions, this does not happen, so we should
  // delete the item here.
  bool is_v4 = ::is_v4(c->version());
  bool is_v3_or_later = is_v3(c->version()) || is_v4;
  bool should_delete_item = is_v3_or_later;

  auto player = c->character();
  auto& item = player->inventory.items[item_index];
  uint32_t primary_identifier = item.data.primary_identifier();

  if (item.data.is_common_consumable()) { // Monomate, etc.
    // Nothing to do (it should be deleted)

  } else if ((primary_identifier & 0xFFFF0000) == 0x03020000) { // Technique disk
    auto item_parameter_table = s->item_parameter_table(c->version());
    uint8_t max_level = item_parameter_table->get_max_tech_level(player->disp.visual.char_class, item.data.data1[4]);
    if (item.data.data1[2] > max_level) {
      throw runtime_error("technique level too high");
    }
    player->set_technique_level(item.data.data1[4], item.data.data1[2]);

  } else if ((primary_identifier & 0xFFFF0000) == 0x030A0000) { // Grinder
    if (item.data.data1[2] > 2) {
      throw runtime_error("incorrect grinder value");
    }

    auto& weapon = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::WEAPON)];
    // Only enforce grind limits on BB, since the server doesn't have direct
    // control over players' inventories on other versions
    auto item_parameter_table = s->item_parameter_table(c->version());
    auto weapon_def = item_parameter_table->get_weapon(weapon.data.data1[1], weapon.data.data1[2]);
    if (is_v4 && (weapon.data.data1[3] >= weapon_def.max_grind)) {
      throw runtime_error("weapon already at maximum grind");
    }
    weapon.data.data1[3] = min<uint8_t>(weapon.data.data1[3] + item.data.data1[2] + 1, weapon_def.max_grind);

  } else if ((primary_identifier & 0xFFFF0000) == 0x030B0000) { // Material
    auto p = c->character();

    using Type = PSOBBCharacterFile::MaterialType;
    Type type;
    switch (item.data.data1[2]) {
      case 0: // Power Material
        type = Type::POWER;
        p->disp.stats.char_stats.atp += 2;
        break;
      case 1: // Mind Material
        type = Type::MIND;
        p->disp.stats.char_stats.mst += 2;
        break;
      case 2: // Evade Material
        type = Type::EVADE;
        p->disp.stats.char_stats.evp += 2;
        break;
      case 3: // HP Material
        type = Type::HP;
        break;
      case 4: // TP Material
        type = Type::TP;
        break;
      case 5: // Def Material
        type = Type::DEF;
        p->disp.stats.char_stats.dfp += 2;
        break;
      case 6: // Hit Material (v1/v2) or Luck Material (v3/v4)
        type = Type::LUCK;
        if (!is_v3_or_later && (c->version() != Version::GC_NTE)) {
          // Hit material doesn't exist on v3/v4, but we'll ignore type anyway
          // in this case because track_non_hp_tp_materials is false
          p->disp.stats.char_stats.ata += 2;
        } else {
          p->disp.stats.char_stats.lck += 2;
        }
        break;
      case 7: // Luck Material (v1/v2)
        type = Type::LUCK;
        if (!is_v3_or_later && (c->version() != Version::GC_NTE)) {
          p->disp.stats.char_stats.lck += 2;
        } else {
          throw runtime_error("unknown material used");
        }
        break;
      default:
        throw runtime_error("unknown material used");
    }
    if (is_v3_or_later || (type == Type::HP) || (type == Type::TP)) {
      p->set_material_usage(type, p->get_material_usage(type) + 1);
    }

  } else if ((primary_identifier & 0xFFFF0000) == 0x030F0000) { // AddSlot
    auto& armor = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::ARMOR)];
    if (armor.data.data1[5] >= 4) {
      throw runtime_error("armor already at maximum slot count");
    }
    armor.data.data1[5]++;

  } else if (item.data.is_wrapped(*s->item_stack_limits(c->version()))) {
    // Unwrap present
    item.data.unwrap(*s->item_stack_limits(c->version()));
    should_delete_item = false;

  } else if (primary_identifier == 0x00330000) {
    // Unseal Sealed J-Sword => Tsumikiri J-Sword
    item.data.data1[1] = 0x32;
    item.flags &= (~8); // Unequip it
    should_delete_item = false;

  } else if (primary_identifier == 0x00AB0000) {
    // Unseal Lame d'Argent => Excalibur
    item.data.data1[1] = 0xAC;
    item.flags &= (~8); // Unequip it
    should_delete_item = false;

  } else if (primary_identifier == 0x01034D00) {
    // Unseal Limiter => Adept
    item.data.data1[2] = 0x4E;
    item.flags &= (~8); // Unequip it
    should_delete_item = false;

  } else if (primary_identifier == 0x01034F00) {
    // Unseal Swordsman Lore => Proof of Sword-Saint
    item.data.data1[2] = 0x50;
    item.flags &= (~8); // Unequip it
    should_delete_item = false;

  } else if (primary_identifier == 0x030C0000) {
    // Cell of MAG 502
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = (player->disp.visual.section_id & 1) ? 0x1D : 0x21;

  } else if (primary_identifier == 0x030C0100) {
    // Cell of MAG 213
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = (player->disp.visual.section_id & 1) ? 0x27 : 0x22;

  } else if (primary_identifier == 0x030C0200) {
    // Parts of RoboChao
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = 0x28;

  } else if (primary_identifier == 0x030C0300) {
    // Heart of Opa Opa
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = 0x29;

  } else if (primary_identifier == 0x030C0400) {
    // Heart of Pian
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = 0x2A;

  } else if (primary_identifier == 0x030C0500) {
    // Heart of Chao
    auto& mag = player->inventory.items[player->inventory.find_equipped_item(EquipSlot::MAG)];
    mag.data.data1[1] = 0x2B;

  } else if ((primary_identifier & 0xFFFF0000) == 0x03150000) {
    // Christmas Present, etc. - use unwrap_table + probabilities therein
    auto item_parameter_table = s->item_parameter_table(c->version());
    auto table = item_parameter_table->get_event_items(item.data.data1[2]);
    size_t sum = 0;
    for (size_t z = 0; z < table.second; z++) {
      sum += table.first[z].probability;
    }
    if (sum == 0) {
      throw runtime_error("no unwrap results available for event");
    }
    size_t det = random_from_optional_crypt(opt_rand_crypt) % sum;
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

        // TODO: It seems that on non-BB, clients don't synchronize this at all
        // so they could end up thinking the unwrapped item is something
        // completely different. How does this actually work on console PSO?
        auto l = c->require_lobby();
        for (const auto& lc : l->clients) {
          if (lc && (lc->version() == Version::BB_V4)) {
            send_create_inventory_item_to_client(c, c->lobby_client_id, item.data);
          }
        }
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
        auto item_parameter_table = s->item_parameter_table(c->version());
        const auto& combo = item_parameter_table->get_item_combination(item.data, inv_item.data);
        if (combo.char_class != 0xFF && combo.char_class != player->disp.visual.char_class) {
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
        if (combo.level != 0xFF && player->disp.stats.level + 1 < combo.level) {
          throw runtime_error("item combination applies with level requirement, but player level is too low");
        }
        // If we get here, then the combo applies
        if (combo_applied) {
          throw runtime_error("multiple combinations apply");
        }
        combo_applied = true;

        inv_item.data.data1[0] = combo.result_item[0];
        inv_item.data.data1[1] = combo.result_item[1];
        // For mags, don't reset level + PBs + stats
        if (inv_item.data.data1[0] != 0x02) {
          inv_item.data.data1[2] = combo.result_item[2];
          inv_item.data.data1[3] = 0; // Grind
          inv_item.data.data1[4] = 0; // Flags + special
        }
        inv_item.flags &= (~8); // Unequip it
      } catch (const out_of_range&) {
      }
    }
  }

  if (should_delete_item) {
    // Allow overdrafting meseta if the client is not BB, since the server isn't
    // informed when meseta is added or removed from the bank.
    player->remove_item(item.data.id, 1, *s->item_stack_limits(c->version()));
  }
}

void apply_mag_feed_result(
    ItemData& mag_item,
    const ItemData& fed_item,
    shared_ptr<const ItemParameterTable> item_parameter_table,
    shared_ptr<const MagEvolutionTable> mag_evolution_table,
    uint8_t char_class,
    uint8_t section_id,
    bool version_has_rare_mags) {

  static const unordered_map<uint32_t, size_t> result_index_for_fed_item({
      {0x03000000, 0}, // Monomate
      {0x03000100, 1}, // Dimate
      {0x03000200, 2}, // Trimate
      {0x03010000, 3}, // Monofluid
      {0x03010100, 4}, // Difluid
      {0x03010200, 5}, // Trifluid
      {0x03060000, 6}, // Antidote
      {0x03060100, 7}, // Antiparalysis
      {0x03030000, 8}, // Sol Atomizer
      {0x03040000, 9}, // Moon Atomizer
      {0x03050000, 10}, // Star Atomizer
  });

  size_t result_index = result_index_for_fed_item.at(fed_item.primary_identifier());
  const auto& mag_def = item_parameter_table->get_mag(mag_item.data1[1]);
  const auto& feed_result = item_parameter_table->get_mag_feed_result(mag_def.feed_table, result_index);

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

  update_stat(mag_item, 2, feed_result.def);
  update_stat(mag_item, 3, feed_result.pow);
  update_stat(mag_item, 4, feed_result.dex);
  update_stat(mag_item, 5, feed_result.mind);
  mag_item.data2[0] = clamp<ssize_t>(static_cast<ssize_t>(mag_item.data2[0]) + feed_result.synchro, 0, 120);
  mag_item.data2[1] = clamp<ssize_t>(static_cast<ssize_t>(mag_item.data2[1]) + feed_result.iq, 0, 200);

  uint8_t mag_level = mag_item.compute_mag_level();
  mag_item.data1[2] = mag_level;
  uint8_t evolution_number = mag_evolution_table->get_evolution_number(mag_item.data1[1]);
  uint8_t mag_number = mag_item.data1[1];

  // Note: Sega really did just hardcode all these rules into the client. There
  // is no data file describing these evolutions, unfortunately.

  if (mag_level < 10) {
    // Nothing to do

  } else if (mag_level < 35) { // Level 10 evolution
    if (evolution_number < 1) {
      switch (char_class) {
        case 0: // HUmar
        case 1: // HUnewearl
        case 2: // HUcast
        case 9: // HUcaseal
          mag_item.data1[1] = 0x01; // Varuna
          break;
        case 3: // RAmar
        case 11: // RAmarl
        case 4: // RAcast
        case 5: // RAcaseal
          mag_item.data1[1] = 0x0D; // Kalki
          break;
        case 10: // FOmar
        case 6: // FOmarl
        case 7: // FOnewm
        case 8: // FOnewearl
          mag_item.data1[1] = 0x19; // Vritra
          break;
        default:
          throw runtime_error("invalid character class");
      }
    }

  } else if (mag_level < 50) { // Level 35 evolution
    if (evolution_number < 2) {
      uint16_t flags = mag_item.compute_mag_strength_flags();
      if (mag_number == 0x0D) {
        if (flags & 0x110) {
          mag_item.data1[1] = 0x02;
        } else if (flags & 8) {
          mag_item.data1[1] = 0x03;
        } else if (flags & 0x20) {
          mag_item.data1[1] = 0x0B;
        }
      } else if (mag_number == 1) {
        if (flags & 0x108) {
          mag_item.data1[1] = 0x0E;
        } else if (flags & 0x10) {
          mag_item.data1[1] = 0x0F;
        } else if (flags & 0x20) {
          mag_item.data1[1] = 0x04;
        }
      } else if (mag_number == 0x19) {
        if (flags & 0x120) {
          mag_item.data1[1] = 0x1A;
        } else if (flags & 8) {
          mag_item.data1[1] = 0x1B;
        } else if (flags & 0x10) {
          mag_item.data1[1] = 0x14;
        }
      }
    }

  } else if ((mag_level % 5) == 0) { // Level 50 (and beyond) evolutions
    if (evolution_number < 4) {

      if ((mag_level >= 100) && version_has_rare_mags) {
        uint8_t section_id_group = section_id % 3;
        uint16_t def = mag_item.data1w[2] / 100;
        uint16_t pow = mag_item.data1w[3] / 100;
        uint16_t dex = mag_item.data1w[4] / 100;
        uint16_t mind = mag_item.data1w[5] / 100;
        bool is_male = char_class_is_male(char_class);
        size_t table_index = (is_male ? 0 : 1) + section_id_group * 2;

        bool is_hunter = char_class_is_hunter(char_class);
        bool is_ranger = char_class_is_ranger(char_class);
        bool is_force = char_class_is_force(char_class);
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
          mag_item.data1[1] = result_table[table_index];
        }
      }

      // If a special evolution did not occur, do a normal level 50 evolution
      if (mag_number == mag_item.data1[1]) {
        uint16_t flags = mag_item.compute_mag_strength_flags();
        uint16_t def = mag_item.data1w[2] / 100;
        uint16_t pow = mag_item.data1w[3] / 100;
        uint16_t dex = mag_item.data1w[4] / 100;
        uint16_t mind = mag_item.data1w[5] / 100;

        bool is_hunter = char_class_is_hunter(char_class);
        bool is_ranger = char_class_is_ranger(char_class);
        bool is_force = char_class_is_force(char_class);
        if (is_hunter + is_ranger + is_force != 1) {
          throw logic_error("char class is not exactly one of the top-level classes");
        }

        if (is_hunter) {
          if (flags & 0x108) {
            mag_item.data1[1] = (section_id & 1)
                ? ((dex < mind) ? 0x08 : 0x06)
                : ((dex < mind) ? 0x0C : 0x05);
          } else if (flags & 0x010) {
            mag_item.data1[1] = (section_id & 1)
                ? ((mind < pow) ? 0x12 : 0x10)
                : ((mind < pow) ? 0x17 : 0x13);
          } else if (flags & 0x020) {
            mag_item.data1[1] = (section_id & 1)
                ? ((pow < dex) ? 0x16 : 0x24)
                : ((pow < dex) ? 0x07 : 0x1E);
          }
        } else if (is_ranger) {
          if (flags & 0x110) {
            mag_item.data1[1] = (section_id & 1)
                ? ((mind < pow) ? 0x0A : 0x05)
                : ((mind < pow) ? 0x0C : 0x06);
          } else if (flags & 0x008) {
            mag_item.data1[1] = (section_id & 1)
                ? ((dex < mind) ? 0x0A : 0x26)
                : ((dex < mind) ? 0x0C : 0x06);
          } else if (flags & 0x020) {
            mag_item.data1[1] = (section_id & 1)
                ? ((pow < dex) ? 0x18 : 0x1E)
                : ((pow < dex) ? 0x08 : 0x05);
          }
        } else if (is_force) {
          if (flags & 0x120) {
            if (def < 45) {
              mag_item.data1[1] = (section_id & 1)
                  ? ((pow < dex) ? 0x17 : 0x09)
                  : ((pow < dex) ? 0x1E : 0x1C);
            } else {
              mag_item.data1[1] = 0x24;
            }
          } else if (flags & 0x008) {
            if (def < 45) {
              mag_item.data1[1] = (section_id & 1)
                  ? ((dex < mind) ? 0x1C : 0x20)
                  : ((dex < mind) ? 0x1F : 0x25);
            } else {
              mag_item.data1[1] = 0x23;
            }
          } else if (flags & 0x010) {
            if (def < 45) {
              mag_item.data1[1] = (section_id & 1)
                  ? ((mind < pow) ? 0x12 : 0x0C)
                  : ((mind < pow) ? 0x15 : 0x11);
            } else {
              mag_item.data1[1] = 0x24;
            }
          }
        }
      }
    }
  }

  // If the mag has evolved, add its new photon blast
  if (mag_number != mag_item.data1[1]) {
    const auto& new_mag_def = item_parameter_table->get_mag(mag_item.data1[1]);
    mag_item.add_mag_photon_blast(new_mag_def.photon_blast);
  }
}

void player_feed_mag(std::shared_ptr<Client> c, size_t mag_item_index, size_t fed_item_index) {
  auto s = c->require_server_state();
  auto player = c->character();
  apply_mag_feed_result(
      player->inventory.items[mag_item_index].data,
      player->inventory.items[fed_item_index].data,
      s->item_parameter_table(c->version()),
      s->mag_evolution_table,
      player->disp.visual.char_class,
      player->disp.visual.section_id,
      !is_v1_or_v2(c->version()));
}
