#include "ItemCreator.hh"

#include <algorithm>
#include <array>

using namespace std;

static const array<uint8_t, 10> favored_weapon_by_section_id = {
    0x09, 0x07, 0x02, 0x04, 0x08, 0x0A, 0xFF, 0x03, 0xFF, 0x05};

ItemCreator::ItemCreator(
    shared_ptr<const CommonItemSet> common_item_set,
    shared_ptr<const RareItemSet> rare_item_set,
    shared_ptr<const ArmorRandomSet> armor_random_set,
    shared_ptr<const ToolRandomSet> tool_random_set,
    shared_ptr<const WeaponRandomSet> weapon_random_set,
    shared_ptr<const TekkerAdjustmentSet> tekker_adjustment_set,
    shared_ptr<const ItemParameterTable> item_parameter_table,
    GameVersion version,
    Episode episode,
    GameMode mode,
    uint8_t difficulty,
    uint8_t section_id,
    uint32_t random_seed,
    shared_ptr<const BattleRules> restrictions)
    : log(string_printf("[ItemCreator:%s/%s/%s/%c/%hhu] ", name_for_version(version), abbreviation_for_episode(episode), abbreviation_for_mode(mode), abbreviation_for_difficulty(difficulty), section_id)),
      version(version),
      episode(episode),
      mode(mode),
      difficulty(difficulty),
      section_id(section_id),
      rare_item_set(rare_item_set),
      armor_random_set(armor_random_set),
      tool_random_set(tool_random_set),
      weapon_random_set(weapon_random_set),
      tekker_adjustment_set(tekker_adjustment_set),
      item_parameter_table(item_parameter_table),
      pt(common_item_set->get_table(this->episode, this->mode, this->difficulty, this->section_id)),
      restrictions(restrictions),
      random_crypt(random_seed) {
  this->generate_unit_weights_tables();
}

void ItemCreator::clear_destroyed_entities() {
  this->destroyed_monsters.clear();
  this->destroyed_boxes.clear();
}

bool ItemCreator::are_rare_drops_allowed() const {
  // Note: The client has an additional check here, which appears to be a subtle
  // anti-cheating measure. There is a flag on the client, initially zero, which
  // is set to 1 when certain unexpected item-related things happen (for
  // example, a player possessing a mag with a level above 200). When the flag
  // is set, this function returns false, which prevents all rare item drops.
  // newserv intentionally does not implement this flag.
  return (this->mode != GameMode::CHALLENGE);
}

uint8_t ItemCreator::normalize_area_number(uint8_t area) const {
  if (!this->restrictions || (this->restrictions->box_drop_area == 0) || (area < 0x10) || (area > 0x11)) {
    switch (this->episode) {
      case Episode::EP1:
        if (area >= 0x0F) {
          throw runtime_error("invalid Episode 1 area number");
        }
        switch (area) {
          case 11:
            return 3; // Dragon -> Cave 1
          case 12:
            return 6; // De Rol Le -> Mine 1
          case 13:
            return 8; // Vol Opt -> Ruins 1
          case 14:
            return 10; // Dark Falz -> Ruins 3
          default:
            return area;
        }
        throw logic_error("this should be impossible");
      case Episode::EP2: {
        static const vector<uint8_t> area_subs = {
            0x01, // 13 (VR Temple Alpha)
            0x02, // 14 (VR Temple Beta)
            0x03, // 15 (VR Spaceship Alpha)
            0x04, // 16 (VR Spaceship Beta)
            0x08, // 17 (Central Control Area)
            0x05, // 18 (Jungle North)
            0x06, // 19 (Jungle South)
            0x07, // 1A (Mountain)
            0x08, // 1B (Seaside)
            0x09, // 1C (Seabed Upper)
            0x0A, // 1D (Seabed Lower)
            0x09, // 1E (Gal Gryphon)
            0x0A, // 1F (Olga Flow)
            0x03, // 20 (Barba Ray)
            0x05, // 21 (Gol Dragon)
            0x08, // 22 (Seaside Night)
            0x0A, // 23 (Tower)
        };
        if ((area >= 0x13) && (area < 0x24)) {
          return area_subs.at(area - 0x13);
        }
        return area;
      }
      case Episode::EP4:
        if (area >= 0x24 && area < 0x2D) {
          return area - 0x22;
        }
        return area;
      default:
        throw logic_error("invalid episode number");
    }

  } else {
    return this->restrictions->box_drop_area;
  }
}

ItemData ItemCreator::on_box_item_drop(uint16_t entity_id, uint8_t area) {
  return this->destroyed_boxes.emplace(entity_id).second
      ? this->on_box_item_drop_with_norm_area(this->normalize_area_number(area) - 1)
      : ItemData();
}

ItemData ItemCreator::on_monster_item_drop(uint16_t entity_id, uint32_t enemy_type, uint8_t area) {
  return this->destroyed_monsters.emplace(entity_id).second
      ? this->on_monster_item_drop_with_norm_area(enemy_type, this->normalize_area_number(area) - 1)
      : ItemData();
}

ItemData ItemCreator::on_box_item_drop_with_norm_area(uint8_t area_norm) {
  this->log.info("Box drop checks for area_norm %02hhX; random state: %08" PRIX32 " %08" PRIX32,
      area_norm, this->random_crypt.seed(), this->random_crypt.absolute_offset());
  ItemData item = this->check_rare_specs_and_create_rare_box_item(area_norm);
  if (item.empty()) {
    uint8_t item_class = this->get_rand_from_weighted_tables_2d_vertical(this->pt->box_item_class_prob_table(), area_norm);
    this->log.info("Item class is %02hhX", item_class);
    switch (item_class) {
      case 0: // Weapon
        item.data1[0] = 0;
        break;
      case 1: // Armor
        item.data1[0] = 1;
        item.data1[1] = 1;
        break;
      case 2: // Shield
        item.data1[0] = 1;
        item.data1[1] = 2;
        break;
      case 3: // Unit
        item.data1[0] = 1;
        item.data1[1] = 3;
        break;
      case 4: // Tool
        item.data1[0] = 3;
        break;
      case 5: // Meseta
        item.data1[0] = 4;
        break;
      case 6: // Nothing
        break;
      default:
        throw logic_error("this should be impossible");
    }
    this->generate_common_item_variances(area_norm, item);
  }
  return item;
}

ItemData ItemCreator::on_monster_item_drop_with_norm_area(uint32_t enemy_type, uint8_t area_norm) {
  if (enemy_type > 0x58) {
    this->log.warning("Invalid enemy type: %" PRIX32, enemy_type);
    return ItemData();
  }
  this->log.info("Enemy type: %" PRIX32 "; random state: %08" PRIX32 " %08" PRIX32, enemy_type, this->random_crypt.seed(), this->random_crypt.absolute_offset());

  uint8_t type_drop_prob = this->pt->enemy_type_drop_probs().at(enemy_type);
  uint8_t drop_sample = this->rand_int(100);
  if (drop_sample >= type_drop_prob) {
    this->log.info("Drop not chosen (%hhu >= %hhu)", drop_sample, type_drop_prob);
    return ItemData();
  }

  ItemData item = this->check_rare_spec_and_create_rare_enemy_item(enemy_type, area_norm);
  if (item.empty()) {
    uint32_t item_class_determinant =
        this->should_allow_meseta_drops()
        ? this->rand_int(3)
        : (this->rand_int(2) + 1);

    uint32_t item_class;
    switch (item_class_determinant) {
      case 0:
        item_class = 5;
        break;
      case 1:
        item_class = 4;
        break;
      case 2:
        item_class = this->pt->enemy_item_classes().at(enemy_type);
        break;
      default:
        throw logic_error("invalid item class determinant");
    }

    this->log.info("Rare drop not chosen; item class determinant is %" PRIu32 "; item class is %" PRIu32, item_class_determinant, item_class);

    switch (item_class) {
      case 0: // Weapon
        item.data1[0] = 0x00;
        break;
      case 1: // Armor
        item.data1w[0] = 0x0101;
        break;
      case 2: // Shield
        item.data1w[0] = 0x0201;
        break;
      case 3: // Unit
        item.data1w[0] = 0x0301;
        break;
      case 4: // Tool
        item.data1[0] = 0x03;
        break;
      case 5: // Meseta
        item.data1[0] = 0x04;
        item.data2d = this->choose_meseta_amount(this->pt->enemy_meseta_ranges(), enemy_type) & 0xFFFF;
        break;
      default:
        return item;
    }

    if (item.data1[0] != 0x04) {
      this->generate_common_item_variances(area_norm, item);
    }
  }

  return item;
}

ItemData ItemCreator::check_rare_specs_and_create_rare_box_item(uint8_t area_norm) {
  ItemData item;
  if (!this->are_rare_drops_allowed()) {
    return item;
  }

  auto rare_specs = this->rare_item_set->get_box_specs(
      this->mode, this->episode, this->difficulty, this->section_id, area_norm + 1);
  for (const auto& spec : rare_specs) {
    item = this->check_rate_and_create_rare_item(spec, area_norm);
    if (!item.empty()) {
      this->log.info("Box spec %08" PRIX32 " produced item %02hhX%02hhX%02hhX",
          spec.probability, spec.item_code[0], spec.item_code[1], spec.item_code[2]);
      break;
    }
    this->log.info("Box spec %08" PRIX32 " did not produce item %02hhX%02hhX%02hhX",
        spec.probability, spec.item_code[0], spec.item_code[1], spec.item_code[2]);
  }
  return item;
}

uint32_t ItemCreator::rand_int(uint64_t max) {
  return this->random_crypt.next() % max;
}

float ItemCreator::rand_float_0_1_from_crypt() {
  // This lacks some precision, but matches the original implementation.
  return (static_cast<double>(this->random_crypt.next() >> 16) / 65536.0);
}

template <size_t NumRanges>
uint32_t ItemCreator::choose_meseta_amount(
    const parray<CommonItemSet::Table::Range<uint16_t>, NumRanges> ranges,
    size_t table_index) {
  uint16_t min = ranges[table_index].min;
  uint16_t max = ranges[table_index].max;

  // Note: The original code seems like it has a bug here: it compares to 0xFF
  // instead of 0xFFFF (and returns 0xFF if either limit matches 0xFF).
  if (((min == 0xFFFF) || (max == 0xFFFF)) || (max < min)) {
    return 0xFFFF;
  } else if (min != max) {
    return this->rand_int((max - min) + 1) + min;
  }
  return min;
}

bool ItemCreator::should_allow_meseta_drops() const {
  return (this->mode != GameMode::CHALLENGE);
}

ItemData ItemCreator::check_rare_spec_and_create_rare_enemy_item(uint32_t enemy_type, uint8_t area_norm) {
  ItemData item;
  if (this->are_rare_drops_allowed() && (enemy_type > 0) && (enemy_type < 0x58)) {
    // Note: In the original implementation, enemies can only have one possible
    // rare drop. In our implementation, they can have multiple rare drops if
    // JSONRareItemSet is used (the other RareItemSet implementations never
    // return multiple drops for an enemy type).
    auto rare_specs = this->rare_item_set->get_enemy_specs(
        this->mode, this->episode, this->difficulty, this->section_id, enemy_type);
    for (const auto& spec : rare_specs) {
      item = this->check_rate_and_create_rare_item(spec, area_norm);
      if (!item.empty()) {
        this->log.info("Enemy spec %08" PRIX32 " produced item %02hhX%02hhX%02hhX",
            spec.probability, spec.item_code[0], spec.item_code[1], spec.item_code[2]);
        break;
      }
      this->log.info("Enemy spec %08" PRIX32 " did not produce item %02hhX%02hhX%02hhX",
          spec.probability, spec.item_code[0], spec.item_code[1], spec.item_code[2]);
    }
  }
  return item;
}

ItemData ItemCreator::check_rate_and_create_rare_item(const RareItemSet::ExpandedDrop& drop, uint8_t area_norm) {
  if (drop.probability == 0) {
    return ItemData();
  }

  // Note: The original code uses 0xFFFFFFFF as the maximum here. We use
  // 0x100000000 instead, which makes all rare items SLIGHTLY more rare.
  if (this->rand_int(0x100000000) >= drop.probability) {
    return ItemData();
  }

  ItemData item;
  item.data1[0] = drop.item_code[0];
  item.data1[1] = drop.item_code[1];
  item.data1[2] = drop.item_code[2];
  switch (item.data1[0]) {
    case 0:
      if (this->pt->has_rare_bonus_value_prob_table()) {
        this->generate_rare_weapon_bonuses(item, this->rand_int(10));
      } else {
        this->generate_common_weapon_bonuses(item, area_norm);
      }
      this->set_item_unidentified_flag_if_not_challenge(item);
      break;
    case 1:
      this->generate_common_armor_slots_and_bonuses(item);
      break;
    case 2:
      this->generate_common_mag_variances(item);
      break;
    case 3:
      this->clear_tool_item_if_invalid(item);
      this->set_tool_item_amount_to_1(item);
      break;
    case 4:
      break;
    default:
      throw logic_error("invalid item class");
  }

  this->clear_item_if_restricted(item);
  this->set_item_kill_count_if_unsealable(item);
  return item;
}

void ItemCreator::generate_rare_weapon_bonuses(ItemData& item, uint32_t random_sample) {
  if (item.data1[0] != 0) {
    return;
  }

  if (!this->pt->has_rare_bonus_value_prob_table()) {
    throw logic_error("generate_rare_weapon_bonuses called for common item table without rare bonus value probability table");
  }

  for (size_t z = 0; z < 6; z += 2) {
    uint8_t bonus_type = this->get_rand_from_weighted_tables_2d_vertical(this->pt->bonus_type_prob_table(), random_sample);
    int16_t bonus_value = this->get_rand_from_weighted_tables_2d_vertical(this->pt->bonus_value_prob_table(), 5);
    item.data1[z + 6] = bonus_type;
    item.data1[z + 7] = bonus_value * 5 - 10;
    // Note: The original code has a special case here, which divides
    // item.data1[z + 7] by 5 and multiplies it by 5 again if bonus_type is 5
    // (Hit). Why this is done is unclear, because item.data1[z + 7] must
    // already be a multiple of 5.
  }

  this->deduplicate_weapon_bonuses(item);
}

void ItemCreator::generate_common_weapon_bonuses(ItemData& item, uint8_t area_norm) {
  if (item.data1[0] != 0) {
    return;
  }

  for (size_t row = 0; row < 3; row++) {
    uint8_t spec = this->pt->nonrare_bonus_prob_spec().at(row).at(area_norm);
    if (spec == 0xFF) {
      this->log.info("Bonus %zu is forbidden", row);
    } else {
      item.data1[(row * 2) + 6] = this->get_rand_from_weighted_tables_2d_vertical(this->pt->bonus_type_prob_table(), area_norm);
      int16_t amount = this->get_rand_from_weighted_tables_2d_vertical(this->pt->bonus_value_prob_table(), spec);
      item.data1[(row * 2) + 7] = amount * 5 - 10;
      this->log.info("Bonus %zu generated as %02hhX %02hhX from area_norm %02hhX and spec %02hhX", row, item.data1[(row * 2) + 6], item.data1[(row * 2) + 7], area_norm, spec);
    }
    // Note: The original code has a special case here, which divides
    // item.data1[z + 7] by 5 and multiplies it by 5 again if bonus_type is 5
    // (Hit). Why this is done is unclear, because item.data1[z + 7] must
    // already be a multiple of 5.
  }

  this->deduplicate_weapon_bonuses(item);
}

void ItemCreator::deduplicate_weapon_bonuses(ItemData& item) const {
  for (size_t x = 0; x < 6; x += 2) {
    for (size_t y = 0; y < x; y += 2) {
      if (item.data1[y + 6] == 0x00) {
        item.data1[x + 6] = 0x00;
      } else if (item.data1[x + 6] == item.data1[y + 6]) {
        item.data1[x + 6] = 0x00;
      }
    }
    if (item.data1[x + 6] == 0x00) {
      item.data1[x + 7] = 0x00;
    }
  }
}

void ItemCreator::set_item_kill_count_if_unsealable(ItemData& item) const {
  if (this->item_parameter_table->is_unsealable_item(item)) {
    this->log.info("Item is unsealable; setting kill count to zero");
    item.set_sealed_item_kill_count(0);
  }
}

void ItemCreator::set_item_unidentified_flag_if_not_challenge(ItemData& item) const {
  if (this->mode == GameMode::CHALLENGE) {
    return;
  }
  if (item.data1[0] != 0x00) {
    return;
  }
  // On V3, all rare weapons and weapons with specials are untekked when
  // created; on V2, only rares that are not in the standard item classes are
  // untekked when created.
  if (this->is_v3()) {
    if (this->item_parameter_table->is_item_rare(item) || (item.data1[4] != 0)) {
      item.data1[4] |= 0x80;
    }
  } else {
    if (this->item_parameter_table->is_item_rare(item) ? (item.data1[1] > 0x0C) : (item.data1[4] != 0)) {
      item.data1[4] |= 0x80;
    }
  }
}

void ItemCreator::set_tool_item_amount_to_1(ItemData& item) const {
  if (item.data1[0] == 0x03) {
    item.set_tool_item_amount(1);
  }
}

void ItemCreator::clear_tool_item_if_invalid(ItemData& item) {
  if ((item.data1[1] == 0x02) &&
      ((item.data1[2] > 0x1D) || (item.data1[4] > 0x12))) {
    item.clear();
  }
}

void ItemCreator::clear_item_if_restricted(ItemData& item) const {
  if (this->item_parameter_table->is_item_rare(item) && !this->are_rare_drops_allowed()) {
    this->log.info("Restricted: item is rare, but rares not allowed");
    item.clear();
    return;
  }

  if (this->mode == GameMode::CHALLENGE) {
    // Forbid HP/TP-restoring units and meseta in challenge mode
    // Note: PSO GC doesn't check for 0x61 or 0x62 here since those items
    // (HP/Resurrection and TP/Resurrection) only exist on BB.
    if (item.data1[0] == 1) {
      if ((item.data1[1] == 3) && (((item.data1[2] >= 0x33) && (item.data1[2] <= 0x38)) || (item.data1[2] == 0x61) || (item.data1[2] == 0x62))) {
        this->log.info("Restricted: restore units not allowed in Challenge mode");
        item.clear();
        return;
      }
    } else if (item.data1[0] == 4) {
      this->log.info("Restricted: meseta not allowed in Challenge mode");
      item.clear();
      return;
    }
  }

  if (this->restrictions) {
    switch (item.data1[0]) {
      case 0:
      case 1:
        switch (this->restrictions->weapon_and_armor_mode) {
          case BattleRules::WeaponAndArmorMode::ALLOW:
          case BattleRules::WeaponAndArmorMode::CLEAR_AND_ALLOW:
            break;
          case BattleRules::WeaponAndArmorMode::FORBID_RARES:
            if (this->item_parameter_table->is_item_rare(item)) {
              this->log.info("Restricted: rare items not allowed");
              item.clear();
            }
            break;
          case BattleRules::WeaponAndArmorMode::FORBID_ALL:
            this->log.info("Restricted: weapons and armors not allowed");
            item.clear();
            break;
          default:
            throw logic_error("invalid weapon and armor mode");
        }
        break;
      case 2:
        if (this->restrictions->mag_mode == BattleRules::MagMode::FORBID_ALL) {
          this->log.info("Restricted: mags not allowed");
          item.clear();
        }
        break;
      case 3:
        if (this->restrictions->tool_mode == BattleRules::ToolMode::FORBID_ALL) {
          this->log.info("Restricted: tools not allowed");
          item.clear();
        } else if (item.data1[1] == 2) {
          switch (this->restrictions->tech_disk_mode) {
            case BattleRules::TechDiskMode::ALLOW:
              break;
            case BattleRules::TechDiskMode::FORBID_ALL:
              this->log.info("Restricted: tech disks not allowed");
              item.clear();
              break;
            case BattleRules::TechDiskMode::LIMIT_LEVEL:
              this->log.info("Restricted: tech disk level limited to %hhu",
                  static_cast<uint8_t>(this->restrictions->max_tech_level + 1));
              if (this->restrictions->max_tech_level == 0) {
                item.data1[2] = 0;
              } else {
                item.data1[2] %= this->restrictions->max_tech_level;
              }
              break;
            default:
              throw logic_error("invalid tech disk mode");
          }
        } else if ((item.data1[1] == 9) && this->restrictions->forbid_scape_dolls) {
          this->log.info("Restricted: scape dolls not allowed");
          item.clear();
        }
        break;
      case 4:
        if (this->restrictions->meseta_mode == BattleRules::MesetaMode::FORBID_ALL) {
          this->log.info("Restricted: meseta not allowed");
          item.clear();
        }
        break;
      default:
        throw logic_error("invalid item");
    }
  }
}

void ItemCreator::generate_common_item_variances(uint32_t norm_area, ItemData& item) {
  switch (item.data1[0]) {
    case 0:
      this->generate_common_weapon_variances(norm_area, item);
      break;
    case 1:
      if (item.data1[1] == 3) {
        float f1 = 1.0 + this->pt->unit_maxes_table().at(norm_area);
        float f2 = this->rand_float_0_1_from_crypt();
        uint8_t det = static_cast<uint32_t>(f1 * f2) & 0xFF;
        this->log.info("Unit variances determinant: %g * %g = %08" PRIX32, f1, f2, det);
        this->generate_common_unit_variances(det, item);
        if (item.data1[2] == 0xFF) {
          this->log.info("Unit subtype not valid; clearing item");
          item.clear();
        }
      } else {
        this->generate_common_armor_or_shield_type_and_variances(norm_area, item);
      }
      break;
    case 2:
      this->generate_common_mag_variances(item);
      break;
    case 3:
      this->generate_common_tool_variances(norm_area, item);
      break;
    case 4:
      item.data2d = this->choose_meseta_amount(this->pt->box_meseta_ranges(), norm_area) & 0xFFFF;
      break;
    default:
      // Note: The original code does the following here:
      // item.clear();
      // item.data1[0] = 0x05;
      throw logic_error("invalid item class");
  }

  this->clear_item_if_restricted(item);
  this->set_item_kill_count_if_unsealable(item);
}

void ItemCreator::generate_common_armor_or_shield_type_and_variances(char area_norm, ItemData& item) {
  this->generate_common_armor_slots_and_bonuses(item);

  uint8_t type = this->get_rand_from_weighted_tables_1d(this->pt->armor_shield_type_index_prob_table());
  item.data1[2] = area_norm + type + this->pt->armor_or_shield_type_bias();
  if (item.data1[2] < 3) {
    item.data1[2] = 0;
  } else {
    item.data1[2] -= 3;
  }
  this->log.info("Armor/shield type: max(%02hhX + %02hhX + %02hhX - 3, 0) = %02hhX",
      area_norm, type, this->pt->armor_or_shield_type_bias(), item.data1[2]);
}

void ItemCreator::generate_common_armor_slots_and_bonuses(ItemData& item) {
  if ((item.data1[0] != 0x01) || (item.data1[1] < 1) || (item.data1[1] > 2)) {
    return;
  }

  if (item.data1[1] == 1) {
    this->generate_common_armor_slot_count(item);
  }

  const auto& def = this->item_parameter_table->get_armor_or_shield(item.data1[1], item.data1[2]);
  item.set_armor_or_shield_defense_bonus(def.dfp_range * this->rand_float_0_1_from_crypt());
  item.set_common_armor_evasion_bonus(def.evp_range * this->rand_float_0_1_from_crypt());
}

void ItemCreator::generate_common_armor_slot_count(ItemData& item) {
  item.data1[5] = this->get_rand_from_weighted_tables_1d(this->pt->armor_slot_count_prob_table());
}

void ItemCreator::generate_common_tool_variances(uint32_t area_norm, ItemData& item) {
  item.clear();

  uint8_t tool_class = this->get_rand_from_weighted_tables_2d_vertical(this->pt->tool_class_prob_table(), area_norm);
  if (this->is_v3() && (tool_class == 0x1A)) {
    tool_class = 0x73;
  }

  this->generate_common_tool_type(tool_class, item);
  if (item.data1[1] == 0x02) { // Tech disk
    item.data1[4] = this->get_rand_from_weighted_tables_2d_vertical(this->pt->technique_index_prob_table(), area_norm);
    item.data1[2] = this->generate_tech_disk_level(item.data1[4], area_norm);
    this->clear_tool_item_if_invalid(item);
  }
  this->set_tool_item_amount_to_1(item);
}

uint8_t ItemCreator::generate_tech_disk_level(uint32_t tech_num, uint32_t area_norm) {
  const auto& range = this->pt->technique_level_ranges().at(tech_num).at(area_norm);
  if (((range.min == 0xFF) || (range.max == 0xFF)) || (range.max < range.min)) {
    return 0xFF;
  } else if (range.min != range.max) {
    return this->rand_int((range.max - range.min) + 1) + range.min;
  }
  return range.min;
}

void ItemCreator::generate_common_tool_type(uint8_t id, ItemData& item) const {
  auto data = this->item_parameter_table->find_tool_by_id(id);
  item.data1[0] = 0x03;
  item.data1[1] = data.first;
  item.data1[2] = data.second;
}

void ItemCreator::generate_common_mag_variances(ItemData& item) const {
  if (item.data1[0] == 0x02) {
    item.data1[1] = 0x00;
    item.assign_mag_stats(ItemMagStats());
  }
}

void ItemCreator::generate_common_weapon_variances(uint8_t area_norm, ItemData& item) {
  item.clear();
  item.data1[0] = 0x00;

  parray<uint8_t, 0x0D> weapon_type_prob_table;
  weapon_type_prob_table[0] = 0;
  memmove(
      weapon_type_prob_table.data() + 1,
      this->pt->base_weapon_type_prob_table().data(),
      0x0C);

  for (size_t z = 1; z < 13; z++) {
    // Technically this should be `if (... < 0)`, but whatever
    if ((area_norm + this->pt->subtype_base_table().at(z - 1)) & 0x80) {
      weapon_type_prob_table[z] = 0;
    }
  }

  this->log.info("Subtype table: %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX",
      weapon_type_prob_table[0], weapon_type_prob_table[1], weapon_type_prob_table[2], weapon_type_prob_table[3],
      weapon_type_prob_table[4], weapon_type_prob_table[5], weapon_type_prob_table[6], weapon_type_prob_table[7],
      weapon_type_prob_table[8], weapon_type_prob_table[9], weapon_type_prob_table[10], weapon_type_prob_table[11],
      weapon_type_prob_table[12]);

  item.data1[1] = this->get_rand_from_weighted_tables_1d(weapon_type_prob_table);
  if (item.data1[1] == 0) {
    this->log.info("00 chosen from subtype table; skipping item");
    item.clear();
  } else {
    int8_t subtype_base = this->pt->subtype_base_table().at(item.data1[1] - 1);
    uint8_t area_length = this->pt->subtype_area_length_table().at(item.data1[1] - 1);
    this->log.info("Subtype table yielded %02hhX; subtype base is %hhd with area length %hhu", item.data1[1], subtype_base, area_length);
    if (subtype_base < 0) {
      item.data1[2] = (area_norm + subtype_base) / area_length;
      this->log.info("Resulting subtype: (%02hhX + %02hhX) / %02hhX = %02hhX", area_norm, subtype_base, area_length, item.data1[2]);
      this->generate_common_weapon_grind(item, (area_norm + subtype_base) - (item.data1[2] * area_length));
    } else {
      item.data1[2] = subtype_base + (area_norm / area_length);
      this->log.info("Resulting subtype: %02hhX + (%02hhX / %02hhX) = %02hhX", subtype_base, area_norm, area_length, item.data1[2]);
      this->generate_common_weapon_grind(item, area_norm - (area_norm / area_length) * area_length);
    }
    this->generate_common_weapon_bonuses(item, area_norm);
    this->generate_common_weapon_special(item, area_norm);
    this->set_item_unidentified_flag_if_not_challenge(item);
  }
}

void ItemCreator::generate_common_weapon_grind(ItemData& item, uint8_t offset_within_subtype_range) {
  if (item.data1[0] == 0) {
    uint8_t offset = clamp<uint8_t>(offset_within_subtype_range, 0, 3);
    item.data1[3] = this->get_rand_from_weighted_tables_2d_vertical(this->pt->grind_prob_table(), offset);
    this->log.info("Generated grind %02hhX from offset within subtype range %02hhX", item.data1[3], offset_within_subtype_range);
  }
}

void ItemCreator::generate_common_weapon_special(ItemData& item, uint8_t area_norm) {
  if (item.data1[0] != 0) {
    return;
  }
  if (this->item_parameter_table->is_item_rare(item)) {
    this->log.info("Item is rare; skipping special generation");
    return;
  }
  uint8_t special_mult = this->pt->special_mult().at(area_norm);
  if (special_mult == 0) {
    this->log.info("Special multiplier is zero for area_norm %02hhX; skipping special generation", area_norm);
    return;
  }
  uint8_t det = this->rand_int(100);
  uint8_t prob = this->pt->special_percent().at(area_norm);
  if (det >= prob) {
    this->log.info("Special not chosen (%02hhX > %02hhX)", det, prob);
    return;
  }
  item.data1[4] = this->choose_weapon_special(special_mult * this->rand_float_0_1_from_crypt());
}

uint8_t ItemCreator::choose_weapon_special(uint8_t det) {
  if (det >= 4) {
    this->log.info("Special not chosen (det %02hhX >= 4)", det);
    return 0;
  }

  static const uint8_t maxes[4] = {8, 10, 11, 11};
  uint8_t det2 = this->rand_int(maxes[det]);
  this->log.info("Choosing special with det %02hhX and det2 %02hhX", det, det2);
  size_t index = 0;
  for (size_t z = 1; z < this->item_parameter_table->num_specials; z++) {
    if (det + 1 == this->item_parameter_table->get_special_stars(z)) {
      if (index == det2) {
        this->log.info("Chose special %02zX", z);
        return z;
      } else {
        index++;
      }
    }
  }
  this->log.info("No special was eligible");
  return 0;
}

void ItemCreator::generate_unit_weights_tables() {
  // Note: This part of the function was originally in a different function,
  // since it had another callsite. Unlike the original code, we generate these
  // tables only once at construction time, so we've inlined the function here.

  size_t star_base_index;
  switch (this->version) {
    case GameVersion::DC:
    case GameVersion::PC:
      star_base_index = 0x1D1;
      this->unit_weights_table1.resize(0x84);
      break;
    case GameVersion::GC:
    case GameVersion::XB:
      star_base_index = 0x2AF;
      this->unit_weights_table1.resize(0x88);
      break;
    case GameVersion::BB:
      star_base_index = 0x37D;
      this->unit_weights_table1.resize(0x88);
      break;
    default:
      throw logic_error("invalid game version");
  }

  size_t z;
  for (z = 0; z < 0x10; z++) {
    uint8_t v = this->item_parameter_table->get_item_stars(z + star_base_index);
    this->unit_weights_table1.at((z * 5) + 0) = v - 1;
    this->unit_weights_table1.at((z * 5) + 1) = v - 1;
    this->unit_weights_table1.at((z * 5) + 2) = v;
    this->unit_weights_table1.at((z * 5) + 3) = v + 1;
    this->unit_weights_table1.at((z * 5) + 4) = v + 1;
  }
  for (; z < (this->unit_weights_table1.size() - 0x40); z++) {
    this->unit_weights_table1.at(z + 0x40) = this->item_parameter_table->get_item_stars(z + star_base_index);
  }
  // Note: Inlining ends here

  this->unit_weights_table2.clear(0);
  for (size_t z = 0; z < 0x88; z++) {
    uint8_t index = this->unit_weights_table1[z];
    if (index < this->unit_weights_table2.size()) {
      this->unit_weights_table2[index]++;
    }
    z = z + 1;
  }
}

void ItemCreator::generate_common_unit_variances(uint8_t det, ItemData& item) {
  if (det >= 0x0D) {
    return;
  }
  item.clear();
  item.data1[0] = 0x01;
  item.data1[1] = 0x03;

  // Note: The original code calls generate_unit_weights_table1 here (which we
  // have inlined into generate_unit_weights_tables above). This call seems
  // unnecessary because the contents of the tables don't depend on anything
  // except what appears in ItemPMT, which is essentially constant, so we
  // don't bother regenerating the table here.

  if (this->unit_weights_table2[det] == 0) {
    this->log.info("Unit weights table 2 entry is zero; skipping variances");
    return;
  }

  size_t which = this->rand_int(this->unit_weights_table2[det]);
  this->log.info("Unit values: which=%02zX max=%02hhX", which, this->unit_weights_table2[det]);
  size_t current_index = 0;
  for (size_t z = 0; z < this->unit_weights_table1.size(); z++) {
    if (det != this->unit_weights_table1[z]) {
      continue;
    }
    if (current_index != which) {
      current_index++;
    } else {
      if (z >= 0x50) {
        if (det <= 0x87) {
          item.data1[2] = z + 0xC0;
        }
      } else {
        item.data1[2] = z / 5;
        const auto& def = this->item_parameter_table->get_unit(item.data1[2]);
        switch (z % 5) {
          case 0:
            item.set_unit_bonus(-(def.modifier_amount * 2));
            break;
          case 1:
            item.set_unit_bonus(-def.modifier_amount);
            break;
          case 2:
            break;
          case 3:
            item.set_unit_bonus(def.modifier_amount);
            break;
          case 4:
            item.set_unit_bonus(def.modifier_amount * 2);
            break;
        }
      }
      break;
    }
  }
}

// Returns a weighted random result, indicating the chosen position in the
// weighted table.
//
// For example, an input table of 40 40 40 40 would be equally likely to return
// 0, 1, 2, or 3. An input table of 40 40 80 would return 2 50% of the time, and
// 0 or 1 each 25% of the time.
template <typename IntT>
IntT ItemCreator::get_rand_from_weighted_tables(const IntT* tables, size_t offset, size_t num_values, size_t stride) {
  uint64_t rand_max = 0;
  for (size_t x = 0; x != num_values; x++) {
    rand_max += tables[x * stride + offset];
  }
  if (rand_max == 0) {
    throw runtime_error("weighted table is empty");
  }

  uint32_t x = this->rand_int(rand_max);
  for (size_t z = 0; z < num_values; z++) {
    IntT table_value = tables[z * stride + offset];
    if (x < table_value) {
      return z;
    }
    x -= table_value;
  }
  throw logic_error("selector was not less than rand_max");
}

template <typename IntT, size_t X>
IntT ItemCreator::get_rand_from_weighted_tables_1d(const parray<IntT, X>& tables) {
  return ItemCreator::get_rand_from_weighted_tables<IntT>(tables.data(), 0, X, 1);
}

template <typename IntT, size_t X, size_t Y>
IntT ItemCreator::get_rand_from_weighted_tables_2d_vertical(const parray<parray<IntT, X>, Y>& tables, size_t offset) {
  return ItemCreator::get_rand_from_weighted_tables<IntT>(tables[0].data(), offset, Y, X);
}

vector<ItemData> ItemCreator::generate_armor_shop_contents(size_t player_level) {
  vector<ItemData> shop;
  this->generate_armor_shop_armors(shop, player_level);
  this->generate_armor_shop_shields(shop, player_level);
  this->generate_armor_shop_units(shop, player_level);
  return shop;
}

size_t ItemCreator::get_table_index_for_armor_shop(
    size_t player_level) {
  if (player_level < 11) {
    return 0;
  } else if (player_level < 26) {
    return 1;
  } else if (player_level < 43) {
    return 2;
  } else if (player_level < 61) {
    return 3;
  } else {
    return 4;
  }
}

bool ItemCreator::shop_does_not_contain_duplicate_armor(
    const vector<ItemData>& shop, const ItemData& item) {
  for (const auto& shop_item : shop) {
    if ((shop_item.data1[0] == item.data1[0]) &&
        (shop_item.data1[1] == item.data1[1]) &&
        (shop_item.data1[2] == item.data1[2]) &&
        (shop_item.data1[5] == item.data1[5])) {
      return false;
    }
  }
  return true;
}

bool ItemCreator::shop_does_not_contain_duplicate_tech_disk(
    const vector<ItemData>& shop, const ItemData& item) {
  for (const auto& shop_item : shop) {
    if ((shop_item.data1[0] == item.data1[0]) &&
        (shop_item.data1[1] == item.data1[1]) &&
        (shop_item.data1[2] == item.data1[2]) &&
        (shop_item.data1[4] == item.data1[4])) {
      return false;
    }
  }
  return true;
}

bool ItemCreator::shop_does_not_contain_duplicate_or_too_many_similar_weapons(
    const vector<ItemData>& shop, const ItemData& item) {
  size_t similar_items = 0;
  for (const auto& shop_item : shop) {
    // Disallow exact matches
    if (shop_item == item) {
      return false;
    }

    if ((shop_item.data1[0] == item.data1[0]) &&
        (shop_item.data1[1] == item.data1[1])) {
      similar_items++;
      if (similar_items >= 2) {
        return false;
      }
    }
  }
  return true;
}

bool ItemCreator::shop_does_not_contain_duplicate_item_by_primary_identifier(
    const vector<ItemData>& shop, const ItemData& item) {
  for (const auto& shop_item : shop) {
    if ((shop_item.data1[0] == item.data1[0]) &&
        (shop_item.data1[1] == item.data1[1]) &&
        (shop_item.data1[2] == item.data1[2])) {
      return false;
    }
  }
  return true;
}

void ItemCreator::generate_armor_shop_armors(
    vector<ItemData>& shop, size_t player_level) {
  size_t num_items;
  if (player_level < 11) {
    num_items = 4;
  } else if (player_level < 26) {
    num_items = 6;
  } else {
    // Note: The original code has another case here that can result in 8 items,
    // but that overflows BB's shop item list command, so we omit it here.
    num_items = 7;
  }
  size_t table_index = this->get_table_index_for_armor_shop(player_level);

  ProbabilityTable<uint8_t, 100> pt;
  auto src_table = this->armor_random_set->get_armor_table(table_index);
  for (size_t z = 0; z < src_table.second; z++) {
    for (size_t y = 0; y < src_table.first[z].weight; y++) {
      pt.push(src_table.first[z].value);
    }
  }
  pt.shuffle(this->random_crypt);

  for (size_t items_generated = 0; items_generated < num_items;) {
    ItemData item;
    item.data1[0] = 1;
    item.data1[1] = 1;
    item.data1[2] = pt.pop();

    if ((this->difficulty == 3) && (player_level > 99)) {
      if (player_level > 150) {
        item.data1[2] += 3;
      } else if (player_level >= 100) {
        item.data1[2] += 2;
      }
    }

    this->generate_common_armor_slot_count(item);
    if (this->shop_does_not_contain_duplicate_armor(shop, item)) {
      shop.emplace_back(std::move(item));
      items_generated++;
    }
  }
}

void ItemCreator::generate_armor_shop_shields(vector<ItemData>& shop, size_t player_level) {
  size_t num_items;
  if (player_level < 11) {
    num_items = 4;
  } else if (player_level < 26) {
    num_items = 5;
  } else if (player_level < 42) {
    num_items = 6;
  } else {
    num_items = 7;
  }
  size_t table_index = this->get_table_index_for_armor_shop(player_level);

  ProbabilityTable<uint8_t, 100> pt;
  auto src_table = this->armor_random_set->get_shield_table(table_index);
  for (size_t z = 0; z < src_table.second; z++) {
    for (size_t y = 0; y < src_table.first[z].weight; y++) {
      pt.push(src_table.first[z].value);
    }
  }
  pt.shuffle(this->random_crypt);

  for (size_t items_generated = 0; items_generated < num_items;) {
    ItemData item;
    item.data1[0] = 1;
    item.data1[1] = 2;
    item.data1[2] = pt.pop();

    if ((this->difficulty == 3) && (player_level > 99)) {
      if (player_level > 150) {
        item.data1[2] += 3;
      } else if (player_level >= 100) {
        item.data1[2] += 2;
      }
    }

    if (this->shop_does_not_contain_duplicate_item_by_primary_identifier(shop, item)) {
      shop.emplace_back(std::move(item));
      items_generated++;
    }
  }
}

void ItemCreator::generate_armor_shop_units(vector<ItemData>& shop, size_t player_level) {
  size_t num_items;
  if (player_level < 11) {
    return; // num_items = 0
  } else if (player_level < 26) {
    num_items = 3;
  } else if (player_level < 43) {
    num_items = 5;
  } else {
    num_items = 6;
  }
  size_t table_index = this->get_table_index_for_armor_shop(player_level);

  ProbabilityTable<uint8_t, 100> pt;
  auto src_table = this->armor_random_set->get_unit_table(table_index);
  for (size_t z = 0; z < src_table.second; z++) {
    for (size_t y = 0; y < src_table.first[z].weight; y++) {
      pt.push(src_table.first[z].value);
    }
  }
  pt.shuffle(this->random_crypt);

  for (size_t items_generated = 0; items_generated < num_items;) {
    ItemData item;
    item.data1[0] = 1;
    item.data1[1] = 3;
    item.data1[2] = pt.pop();
    if (this->shop_does_not_contain_duplicate_item_by_primary_identifier(shop, item)) {
      shop.emplace_back(std::move(item));
      items_generated++;
    }
  }
}

vector<ItemData> ItemCreator::generate_tool_shop_contents(size_t player_level) {
  vector<ItemData> shop;
  this->generate_common_tool_shop_recovery_items(shop, player_level);
  this->generate_rare_tool_shop_recovery_items(shop, player_level);
  this->generate_tool_shop_tech_disks(shop, player_level);
  sort(shop.begin(), shop.end(), ItemData::compare_for_sort);
  return shop;
}

size_t ItemCreator::get_table_index_for_tool_shop(size_t player_level) {
  if (player_level < 11) {
    return 0;
  } else if (player_level < 26) {
    return 1;
  } else if (player_level < 43) {
    return 2;
  } else if (player_level < 61) {
    return 3;
  } else {
    return 4;
  }
}

static const vector<pair<uint8_t, uint8_t>> tool_item_defs({
    {0x00, 0x00},
    {0x00, 0x01},
    {0x00, 0x02},
    {0x01, 0x00},
    {0x01, 0x01},
    {0x01, 0x02},
    {0x06, 0x00},
    {0x06, 0x01},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x00},
    {0x07, 0x00},
    {0x08, 0x00},
    {0x09, 0x00},
    {0x0A, 0x00},
    {0xFF, 0xFF},
});

void ItemCreator::generate_common_tool_shop_recovery_items(
    vector<ItemData>& shop, size_t player_level) {
  size_t table_index;
  if (player_level < 11) {
    table_index = 0;
  } else if (player_level < 26) {
    table_index = 1;
  } else if (player_level < 45) {
    table_index = 2;
  } else if (player_level < 61) {
    table_index = 3;
  } else if (player_level < 100) {
    table_index = 4;
  } else {
    table_index = 5;
  }

  auto table = this->tool_random_set->get_common_recovery_table(table_index);
  for (size_t z = 0; z < table.second; z++) {
    uint8_t type = table.first[z];
    if (type == 0x0F) {
      continue;
    }

    auto& item = shop.emplace_back();
    item.data1[0] = 3;
    item.data1[1] = tool_item_defs[type].first;
    item.data1[2] = tool_item_defs[type].second;
  }
}

void ItemCreator::generate_rare_tool_shop_recovery_items(
    vector<ItemData>& shop, size_t player_level) {
  if (player_level < 11) {
    return;
  }
  static constexpr size_t num_items = 2;

  ProbabilityTable<uint8_t, 100> pt;
  size_t table_index = this->get_table_index_for_tool_shop(player_level);
  auto table = this->tool_random_set->get_rare_recovery_table(table_index);
  for (size_t z = 0; z < table.second; z++) {
    const auto& e = table.first[z];
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }
  pt.shuffle(this->random_crypt);

  size_t effective_num_items = num_items;
  size_t items_generated = 0;
  while (items_generated < effective_num_items) {
    uint8_t type = pt.pop();
    if (type == 0x0F) {
      if (effective_num_items == num_items) {
        effective_num_items--;
      }
    } else {
      ItemData item;
      item.data1[0] = 3;
      item.data1[1] = tool_item_defs[type].first;
      item.data1[2] = tool_item_defs[type].second;
      if (this->shop_does_not_contain_duplicate_item_by_primary_identifier(shop, item)) {
        shop.emplace_back(std::move(item));
        items_generated++;
      }
    }
  }
}

void ItemCreator::generate_tool_shop_tech_disks(vector<ItemData>& shop, size_t player_level) {
  size_t num_items;
  if (player_level < 11) {
    num_items = 4;
  } else if (player_level < 43) {
    num_items = 5;
  } else {
    num_items = 7;
  }

  size_t table_index = this->get_table_index_for_tool_shop(player_level);
  auto table = this->tool_random_set->get_tech_disk_table(table_index);

  ProbabilityTable<uint8_t, 100> pt;
  for (size_t z = 0; z < table.second; z++) {
    const auto& e = table.first[z];
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }
  pt.shuffle(this->random_crypt);

  static const array<uint8_t, 0x13> tech_num_map = {
      0x00, 0x03, 0x06, 0x0F, 0x10, 0x0D, 0x0A, 0x0B, 0x0C, 0x01, 0x04, 0x07,
      0x0E, 0x11, 0x02, 0x05, 0x08, 0x09, 0x12};

  size_t items_generated = 0;
  while (items_generated < num_items) {
    uint8_t tech_num_index = pt.pop();
    ItemData item;
    item.data1[0] = 3;
    item.data1[1] = 2;
    item.data1[4] = tech_num_map.at(tech_num_index);
    this->choose_tech_disk_level_for_tool_shop(item, player_level, tech_num_index);
    if (this->shop_does_not_contain_duplicate_tech_disk(shop, item)) {
      shop.emplace_back(std::move(item));
      items_generated++;
    }
  }
}

void ItemCreator::choose_tech_disk_level_for_tool_shop(
    ItemData& item, size_t player_level, uint8_t tech_num_index) {
  size_t table_index = this->get_table_index_for_tool_shop(player_level);
  auto table = this->tool_random_set->get_tech_disk_level_table(table_index);
  if (tech_num_index >= table.second) {
    throw runtime_error("technique number out of range");
  }
  const auto& e = table.first[tech_num_index];

  switch (e.mode) {
    case ToolRandomSet::TechDiskLevelEntry::Mode::LEVEL_1:
      item.data1[2] = 0;
      break;
    case ToolRandomSet::TechDiskLevelEntry::Mode::PLAYER_LEVEL_DIVISOR:
      item.data1[2] = clamp<ssize_t>(
          (min<size_t>(player_level, 99) / e.player_level_divisor_or_min_level) - 1, 0, 14);
      break;
    case ToolRandomSet::TechDiskLevelEntry::Mode::RANDOM_IN_RANGE: {
      // Note: This logic does not give a uniform distribution - if the minimum
      // level is not zero (level 1), then the minimum level is more likely than
      // all the other levels. This behavior matches the client's logic, though
      // it's unclear if this nonuniformity was intentional.
      int16_t min_level = max<int16_t>(e.player_level_divisor_or_min_level - 1, 0);
      item.data1[2] = clamp<int16_t>(this->rand_int(e.max_level), min_level, 14);
      break;
    }
    default:
      throw logic_error("invalid tech disk level mode");
  }
}

vector<ItemData> ItemCreator::generate_weapon_shop_contents(size_t player_level) {
  size_t num_items;
  if (player_level < 11) {
    num_items = 10;
  } else if (player_level < 43) {
    num_items = 12;
  } else {
    num_items = 16;
  }

  size_t table_index;
  if (this->difficulty == 3) {
    if (player_level < 11) {
      table_index = 0;
    } else if (player_level < 26) {
      table_index = 1;
    } else if (player_level < 43) {
      table_index = 2;
    } else if (player_level < 61) {
      table_index = 3;
    } else if (player_level < 100) {
      table_index = 4;
    } else if (player_level < 151) {
      table_index = 5;
    } else {
      table_index = 6;
    }
  } else {
    if (player_level < 11) {
      table_index = 0;
    } else if (player_level < 26) {
      table_index = 1;
    } else if (player_level < 43) {
      table_index = 2;
    } else if (player_level < 61) {
      table_index = 3;
    } else {
      table_index = 4;
    }
  }

  ProbabilityTable<uint8_t, 100> pt;
  auto table = this->weapon_random_set->get_weapon_type_table(table_index);
  for (size_t z = 0; z < table.second; z++) {
    const auto& e = table.first[z];
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }
  pt.shuffle(this->random_crypt);

  vector<ItemData> shop;
  while (shop.size() < num_items) {
    ItemData item;

    uint8_t which = pt.pop();
    if (which == 0x39) {
      static const vector<pair<uint8_t, uint8_t>> defs({
          {0x28, 0x00},
          {0x2A, 0x00},
          {0x2B, 0x00},
          {0x35, 0x00},
          {0x52, 0x00},
          {0x48, 0x00},
          {0x64, 0x00},
          {0x59, 0x00},
          {0x8A, 0x00},
          {0x99, 0x00},
      });
      const auto& def = defs.at(this->section_id);
      item.data1[0] = 0;
      item.data1[1] = def.first;
      item.data1[2] = def.second;

    } else if (which == 0x3A) {
      static const vector<pair<uint8_t, uint8_t>> defs({
          {0x99, 0x00},
          {0x64, 0x00},
          {0x8A, 0x00},
          {0x28, 0x00},
          {0x59, 0x00},
          {0x2B, 0x00},
          {0x52, 0x00},
          {0x2A, 0x00},
          {0x48, 0x00},
          {0x35, 0x00},
      });
      const auto& def = defs.at(this->section_id);
      item.data1[0] = 0;
      item.data1[1] = def.first;
      item.data1[2] = def.second;

    } else {
      static const vector<pair<uint8_t, uint8_t>> defs({
          {0x01, 0x00},
          {0x01, 0x01},
          {0x01, 0x02},
          {0x01, 0x03},
          {0x01, 0x04},
          {0x03, 0x00},
          {0x03, 0x01},
          {0x03, 0x02},
          {0x03, 0x03},
          {0x03, 0x04},
          {0x02, 0x00},
          {0x02, 0x01},
          {0x02, 0x02},
          {0x02, 0x03},
          {0x02, 0x04},
          {0x05, 0x00},
          {0x05, 0x01},
          {0x05, 0x02},
          {0x05, 0x03},
          {0x05, 0x04},
          {0x04, 0x00},
          {0x04, 0x01},
          {0x04, 0x02},
          {0x04, 0x03},
          {0x04, 0x04},
          {0x06, 0x00},
          {0x06, 0x01},
          {0x06, 0x02},
          {0x06, 0x03},
          {0x06, 0x04},
          {0x07, 0x00},
          {0x07, 0x01},
          {0x07, 0x02},
          {0x07, 0x03},
          {0x07, 0x04},
          {0x08, 0x00},
          {0x08, 0x01},
          {0x08, 0x02},
          {0x08, 0x03},
          {0x08, 0x04},
          {0x09, 0x00},
          {0x09, 0x01},
          {0x09, 0x02},
          {0x09, 0x03},
          {0x09, 0x04},
          {0x0A, 0x00},
          {0x0A, 0x01},
          {0x0A, 0x02},
          {0x0A, 0x03},
          {0x0B, 0x00},
          {0x0B, 0x01},
          {0x0B, 0x02},
          {0x0B, 0x03},
          {0x0C, 0x00},
          {0x0C, 0x01},
          {0x0C, 0x02},
          {0x0C, 0x03},
          {0xFF, 0xFF},
          {0xFF, 0xFF},
          {0x01, 0x05},
          {0x02, 0x05},
          {0x06, 0x05},
          {0x08, 0x05},
          {0x0A, 0x04},
          {0x0C, 0x04},
          {0x0B, 0x04},
          {0x01, 0x06},
          {0x03, 0x05},
          {0x07, 0x05},
          {0x0A, 0x05},
          {0x0C, 0x05},
          {0x0B, 0x05},
      });
      const auto& def = defs.at(which);
      item.data1[0] = 0;
      item.data1[1] = def.first;
      item.data1[2] = def.second;
    }

    this->generate_weapon_shop_item_grind(item, player_level);
    this->generate_weapon_shop_item_special(item, player_level);
    this->generate_weapon_shop_item_bonus1(item, player_level);
    this->generate_weapon_shop_item_bonus2(item, player_level);
    item.data1[10] = 0;
    item.data1[11] = 0;

    if (this->shop_does_not_contain_duplicate_or_too_many_similar_weapons(shop, item)) {
      shop.emplace_back(std::move(item));
    }
  }

  sort(shop.begin(), shop.end(), ItemData::compare_for_sort);
  return shop;
}

void ItemCreator::generate_weapon_shop_item_grind(ItemData& item, size_t player_level) {
  size_t table_index;
  if (player_level < 4) {
    table_index = 0;
  } else if (player_level < 11) {
    table_index = 1;
  } else if (player_level < 26) {
    table_index = 2;
  } else if (player_level < 41) {
    table_index = 3;
  } else if (player_level < 56) {
    table_index = 4;
  } else {
    table_index = 5;
  }

  uint8_t favored_weapon = favored_weapon_by_section_id.at(this->section_id);
  bool is_favored = (favored_weapon != 0xFF) && (item.data1[1] == favored_weapon);
  const auto* range = is_favored
      ? this->weapon_random_set->get_favored_grind_range(table_index)
      : this->weapon_random_set->get_standard_grind_range(table_index);

  const auto& weapon_def = this->item_parameter_table->get_weapon(
      item.data1[1], item.data1[2]);
  item.data1[3] = clamp<uint8_t>(
      this->rand_int(range->max + 1), range->min, weapon_def.max_grind);
}

void ItemCreator::generate_weapon_shop_item_special(ItemData& item, size_t player_level) {
  ProbabilityTable<uint8_t, 100> pt;

  size_t table_index;
  if (player_level < 11) {
    table_index = 0;
  } else if (player_level < 18) {
    table_index = 1;
  } else if (player_level < 26) {
    table_index = 2;
  } else if (player_level < 36) {
    table_index = 3;
  } else if (player_level < 46) {
    table_index = 4;
  } else if (player_level < 61) {
    table_index = 5;
  } else if (player_level < 76) {
    table_index = 6;
  } else {
    table_index = 7;
  }

  const auto* table = this->weapon_random_set->get_special_mode_table(table_index);
  for (size_t z = 0; z < table->size(); z++) {
    const auto& e = table->at(z);
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }

  // Note: The original code shuffles pt and then pops a single value from it.
  // For simplicity, we just sample a single value (and don't pop it) instead.
  switch (pt.sample(this->random_crypt)) {
    case 0:
      item.data1[4] = 0;
      break;
    case 1:
      item.data1[4] = this->choose_weapon_special(0);
      break;
    case 2:
      item.data1[4] = this->choose_weapon_special(1);
      break;
    default:
      throw runtime_error("invalid special mode");
  }
}

static const array<int8_t, 20> bonus_values = {
    -50, -45, -40, -35, -30, -25, -20, -15, -10, -5, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};

void ItemCreator::generate_weapon_shop_item_bonus1(
    ItemData& item, size_t player_level) {
  size_t table_index;
  if (player_level < 4) {
    table_index = 0;
  } else if (player_level < 11) {
    table_index = 1;
  } else if (player_level < 18) {
    table_index = 2;
  } else if (player_level < 26) {
    table_index = 3;
  } else if (player_level < 36) {
    table_index = 4;
  } else if (player_level < 46) {
    table_index = 5;
  } else if (player_level < 61) {
    table_index = 6;
  } else if (player_level < 76) {
    table_index = 7;
  } else {
    table_index = 8;
  }

  const auto* type_table = this->weapon_random_set->get_bonus_type_table(0, table_index);
  ProbabilityTable<uint8_t, 100> pt;
  for (size_t z = 0; z < type_table->size(); z++) {
    const auto& e = type_table->at(z);
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }

  // Note: The original code shuffles pt and then pops a single value from it.
  // For simplicity, we just sample a single value (and don't pop it) instead.
  item.data1[6] = pt.sample(this->random_crypt);
  if (item.data1[6] == 0) {
    item.data1[7] = 0;

  } else {
    const auto* range = this->weapon_random_set->get_bonus_range(0, table_index);
    item.data1[7] = bonus_values.at(max<size_t>(
        this->rand_int(range->max + 1), range->min));
  }
}

void ItemCreator::generate_weapon_shop_item_bonus2(ItemData& item, size_t player_level) {
  size_t table_index;
  if (player_level < 6) {
    table_index = 0;
  } else if (player_level < 11) {
    table_index = 1;
  } else if (player_level < 18) {
    table_index = 2;
  } else if (player_level < 26) {
    table_index = 3;
  } else if (player_level < 36) {
    table_index = 4;
  } else if (player_level < 46) {
    table_index = 5;
  } else if (player_level < 61) {
    table_index = 6;
  } else if (player_level < 76) {
    table_index = 7;
  } else {
    table_index = 8;
  }

  const auto* type_table = this->weapon_random_set->get_bonus_type_table(1, table_index);
  ProbabilityTable<uint8_t, 100> pt;
  for (size_t z = 0; z < type_table->size(); z++) {
    const auto& e = type_table->at(z);
    for (size_t y = 0; y < e.weight; y++) {
      pt.push(e.value);
    }
  }
  pt.shuffle(this->random_crypt);

  do {
    item.data1[8] = pt.pop();
  } while ((item.data1[8] != 0) && (item.data1[8] == item.data1[6]));

  if (item.data1[8] == 0) {
    item.data1[9] = 0;

  } else {
    const auto* range = this->weapon_random_set->get_bonus_range(1, table_index);
    item.data1[9] = bonus_values.at(max<size_t>(
        this->rand_int(range->max + 1), range->min));
  }
}

ItemData ItemCreator::on_specialized_box_item_drop(uint16_t entity_id, uint32_t def0, uint32_t def1, uint32_t def2) {
  if (!this->destroyed_boxes.emplace(entity_id).second) {
    return ItemData();
  }

  ItemData item;
  item.data1[0] = (def0 >> 0x18) & 0x0F;
  item.data1[1] = (def0 >> 0x10) + ((item.data1[0] == 0x00) || (item.data1[0] == 0x01));
  item.data1[2] = def0 >> 8;

  switch (item.data1[0]) {
    case 0x00:
      item.data1[3] = (def1 >> 0x18) & 0xFF;
      item.data1[4] = def0 & 0xFF;
      item.data1[6] = (def1 >> 8) & 0xFF;
      item.data1[7] = def1 & 0xFF;
      item.data1[8] = (def2 >> 0x18) & 0xFF;
      item.data1[9] = (def2 >> 0x10) & 0xFF;
      item.data1[10] = (def2 >> 8) & 0xFF;
      item.data1[11] = def2 & 0xFF;
      break;
    case 0x01:
      item.data1[3] = (def1 >> 0x18) & 0xFF;
      item.data1[4] = (def1 >> 0x10) & 0xFF;
      item.data1[5] = def0 & 0xFF;
      break;
    case 0x02:
      item.assign_mag_stats(ItemMagStats());
      break;
    case 0x03:
      if (item.data1[1] == 0x02) {
        item.data1[4] = def0 & 0xFF;
      }
      item.set_tool_item_amount(1);
      break;
    case 0x04:
      item.data2d = ((def1 >> 0x10) & 0xFFFF) * 10;
      break;

    default:
      throw runtime_error("invalid item class");
  }

  return item;
}

ssize_t ItemCreator::apply_tekker_deltas(ItemData& item, uint8_t section_id) {
  if (item.data1[0] != 0) {
    throw runtime_error("tekker deltas can only be applied to weapons");
  }

  static const array<int8_t, 11> delta_table = {-10, -5, -3, -2, -1, 0, 1, 2, 3, 5, 10};

  bool favored = item.data1[1] == favored_weapon_by_section_id[section_id];
  ssize_t luck = 0;

  // Adjust the weapon's special
  {
    const auto& prob_table = this->tekker_adjustment_set->get_special_upgrade_prob_table(section_id, favored);
    uint8_t delta_index = prob_table.sample(this->random_crypt);
    int8_t delta = delta_table.at(delta_index);
    // Note: The original code checks specifically for -1 and +1 here, but the
    // data files only include delta_indexes 4, 5, and 6 (which correspond to -1,
    // 0, and 1) anyway, so we just check for positive and negative numbers
    // instead. When using the original JudgeItem.rel file, the behavior should
    // be the same, but this feels more correct.
    try {
      uint8_t new_special;
      if (delta < 0) {
        new_special = item.data1[4] - 1;
      } else if (delta > 0) {
        new_special = item.data1[4] + 1;
      } else {
        new_special = item.data1[4];
      }
      if ((new_special != item.data1[4]) &&
          (this->item_parameter_table->get_special(item.data1[4]).type ==
              this->item_parameter_table->get_special(new_special).type)) {
        item.data1[4] = new_special;
      }
    } catch (const runtime_error&) {
      // Invalid special number passed to get_special; just ignore it
    }
    luck += this->tekker_adjustment_set->get_luck_for_special_upgrade(delta_index);
  }

  // Adjust the weapon's grind if it's not rare
  if (!this->item_parameter_table->is_item_rare(item)) {
    const auto& weapon_def = this->item_parameter_table->get_weapon(item.data1[1], item.data1[2]);
    const auto& prob_table = this->tekker_adjustment_set->get_grind_delta_prob_table(section_id, favored);
    uint8_t delta_index = prob_table.sample(this->random_crypt);
    int8_t delta = delta_table.at(delta_index);
    int16_t new_grind = static_cast<int16_t>(item.data1[3]) + static_cast<int16_t>(delta);
    item.data1[3] = clamp<int16_t>(new_grind, 0, weapon_def.max_grind);
    luck += this->tekker_adjustment_set->get_luck_for_grind_delta(delta_index);
  }

  // Adjust the weapon's bonuses
  {
    const auto& prob_table = this->tekker_adjustment_set->get_bonus_delta_prob_table(section_id, favored);
    // Note: The original code really does use the same delta for all three
    // bonuses.
    uint8_t delta_index = prob_table.sample(this->random_crypt);
    int8_t delta = delta_table.at(delta_index);
    // Note: The original code doesn't check if there's actually a bonus in each
    // slot before incrementing the values. Presumably there's a check later
    // that will clear any invalid bonuses, but we don't have such a check, so
    // we need to check here if each bonus is actually present.
    for (size_t z = 6; z <= 10; z += 2) {
      if (item.data1[z] >= 1 && item.data1[z] <= 5) {
        item.data1[z + 1] = min<int8_t>(item.data1[z + 1] + delta, 100);
      }
    }
    luck += this->tekker_adjustment_set->get_luck_for_bonus_delta(delta_index);
  }

  return luck;
}
