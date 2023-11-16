#include "ItemNameIndex.hh"

#include "StaticGameData.hh"

using namespace std;

ItemNameIndex::ItemNameIndex(JSON&& v2_names, JSON&& v3_names, JSON&& v4_names) {
  auto get_or_create_meta = [&](uint32_t primary_identifier) {
    shared_ptr<ItemMetadata> meta;
    try {
      return this->primary_identifier_index.at(primary_identifier);
    } catch (const out_of_range&) {
      auto meta = make_shared<ItemMetadata>();
      meta->primary_identifier = primary_identifier;
      this->primary_identifier_index.emplace(primary_identifier, meta);
      return meta;
    }
  };

  for (const auto& it : v2_names.as_dict()) {
    uint32_t primary_identifier = stoul(it.first, nullptr, 16);
    auto meta = get_or_create_meta(primary_identifier);
    meta->v2_name = std::move(it.second->as_string());
    this->v2_name_index.emplace(tolower(meta->v2_name), meta);
  }
  for (const auto& it : v3_names.as_dict()) {
    uint32_t primary_identifier = stoul(it.first, nullptr, 16);
    auto meta = get_or_create_meta(primary_identifier);
    meta->v3_name = std::move(it.second->as_string());
    this->v3_name_index.emplace(tolower(meta->v3_name), meta);
  }
  for (const auto& it : v4_names.as_dict()) {
    uint32_t primary_identifier = stoul(it.first, nullptr, 16);
    auto meta = get_or_create_meta(primary_identifier);
    meta->v4_name = std::move(it.second->as_string());
    this->v4_name_index.emplace(tolower(meta->v4_name), meta);
  }
}

static const char* s_rank_name_characters = "\0ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

// clang-format off
static const array<const char*, 0x29> name_for_weapon_special = {
    nullptr,
    "Draw",      // Type: 0001, amount: 0005
    "Drain",     // Type: 0001, amount: 0009
    "Fill",      // Type: 0001, amount: 000D
    "Gush",      // Type: 0001, amount: 0011
    "Heart",     // Type: 0002, amount: 0003
    "Mind",      // Type: 0002, amount: 0004
    "Soul",      // Type: 0002, amount: 0005
    "Geist",     // Type: 0002, amount: 0006
    "Master\'s", // Type: 0003, amount: 0008
    "Lord\'s",   // Type: 0003, amount: 000A
    "King\'s",   // Type: 0003, amount: 000C
    "Charge",    // Type: 0004, amount: 0064
    "Spirit",    // Type: 0005, amount: 0064
    "Berserk",   // Type: 0006, amount: 0064
    "Ice",       // Type: 0007, amount: 0020
    "Frost",     // Type: 0007, amount: 0030
    "Freeze",    // Type: 0007, amount: 0040
    "Blizzard",  // Type: 0007, amount: 0050
    "Bind",      // Type: 0008, amount: 0020
    "Hold",      // Type: 0008, amount: 0030
    "Seize",     // Type: 0008, amount: 0040
    "Arrest",    // Type: 0008, amount: 0050
    "Heat",      // Type: 0009, amount: 0001
    "Fire",      // Type: 0009, amount: 0002
    "Flame",     // Type: 0009, amount: 0003
    "Burning",   // Type: 0009, amount: 0004
    "Shock",     // Type: 000A, amount: 0001
    "Thunder",   // Type: 000A, amount: 0002
    "Storm",     // Type: 000A, amount: 0003
    "Tempest",   // Type: 000A, amount: 0004
    "Dim",       // Type: 000B, amount: 0030
    "Shadow",    // Type: 000B, amount: 0042
    "Dark",      // Type: 000B, amount: 004E
    "Hell",      // Type: 000B, amount: 005D
    "Panic",     // Type: 000C, amount: 001D
    "Riot",      // Type: 000C, amount: 002C
    "Havoc",     // Type: 000C, amount: 003C
    "Chaos",     // Type: 000C, amount: 004C
    "Devil\'s",  // Type: 000D, amount: 0032
    "Demon\'s",  // Type: 000D, amount: 0019
};
// clang-format on

const array<const char*, 0x11> name_for_s_rank_special = {
    nullptr,
    "Jellen",
    "Zalure",
    "HP-Revival",
    "TP-Revival",
    "Burning",
    "Tempest",
    "Blizzard",
    "Arrest",
    "Chaos",
    "Hell",
    "Spirit",
    "Berserk",
    "Demon\'s",
    "Gush",
    "Geist",
    "King\'s",
};

std::string ItemNameIndex::describe_item(
    GameVersion version,
    const ItemData& item,
    std::shared_ptr<const ItemParameterTable> item_parameter_table) const {
  if (item.data1[0] == 0x04) {
    return string_printf("%s%" PRIu32 " Meseta", item_parameter_table ? "$C7" : "", item.data2d.load());
  }

  vector<string> ret_tokens;

  // For weapons, specials appear before the weapon name
  if ((item.data1[0] == 0x00) && (item.data1[4] != 0x00) && !item.is_s_rank_weapon()) {
    // 0x80 is the unidentified flag, but we always return the identified name
    // of the item here, so we ignore it
    bool is_present = item.data1[4] & 0x40;
    uint8_t special_id = item.data1[4] & 0x3F;
    if (is_present) {
      ret_tokens.emplace_back("Wrapped");
    }
    if (special_id) {
      try {
        ret_tokens.emplace_back(name_for_weapon_special.at(special_id));
      } catch (const out_of_range&) {
        ret_tokens.emplace_back(string_printf("!SP:%02hhX", special_id));
      }
    }
  }
  if ((item.data1[0] == 0x00) && (item.data1[2] != 0x00) && item.is_s_rank_weapon()) {
    try {
      ret_tokens.emplace_back(name_for_s_rank_special.at(item.data1[2]));
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(string_printf("!SSP:%02hhX", item.data1[2]));
    }
  }

  // Armors, shields, and units (0x01) can be wrapped, as can mags (0x02) and
  // non-stackable tools (0x03). However, each of these item classes has its
  // flags in a different location.
  if (((item.data1[1] == 0x01) && (item.data1[4] & 0x40)) ||
      ((item.data1[0] == 0x02) && (item.data2[2] & 0x40)) ||
      ((item.data1[0] == 0x03) && !item.is_stackable() && (item.data1[3] & 0x40))) {
    ret_tokens.emplace_back("Wrapped");
  }

  // Add the item name. Technique disks are special because the level is part of
  // the primary identifier, so we manually generate the name instead of looking
  // it up.
  uint32_t primary_identifier = item.primary_identifier();
  if ((primary_identifier & 0xFFFFFF00) == 0x00030200) {
    string technique_name;
    try {
      technique_name = tech_id_to_name.at(item.data1[4]);
      technique_name[0] = toupper(technique_name[0]);
    } catch (const out_of_range&) {
      technique_name = string_printf("!TECH:%02hhX", item.data1[4]);
    }
    // Hide the level for Reverser and Ryuker, unless the level isn't 1
    if ((item.data1[2] == 0) && ((item.data1[4] == 0x0E) || (item.data1[4] == 0x11))) {
      ret_tokens.emplace_back(string_printf("Disk:%s", technique_name.c_str()));
    } else {
      ret_tokens.emplace_back(string_printf("Disk:%s Lv.%d", technique_name.c_str(), item.data1[2] + 1));
    }
  } else {
    try {
      auto meta = this->primary_identifier_index.at(primary_identifier);
      const string* name;
      switch (version) {
        case GameVersion::DC:
        case GameVersion::PC:
          name = &meta->v2_name;
          break;
        case GameVersion::GC:
        case GameVersion::XB:
          name = &meta->v3_name;
          break;
        case GameVersion::BB:
          name = &meta->v4_name;
          break;
        default:
          throw logic_error("invalid game version");
      }
      if (name->empty()) {
        throw out_of_range("item does not exist");
      }
      ret_tokens.emplace_back(*name);

    } catch (const out_of_range&) {
      ret_tokens.emplace_back(string_printf("!ID:%06" PRIX32, primary_identifier));
    }
  }

  // For weapons, add the grind and percentages, or S-rank name if applicable
  if (item.data1[0] == 0x00) {
    if (item.data1[3] > 0) {
      ret_tokens.emplace_back(string_printf("+%hhu", item.data1[3]));
    }

    if (item.is_s_rank_weapon()) {
      // S-rank (has name instead of percent bonuses)
      uint16_t be_data1w3 = bswap16(item.data1w[3]);
      uint16_t be_data1w4 = bswap16(item.data1w[4]);
      uint16_t be_data1w5 = bswap16(item.data1w[5]);
      uint8_t char_indexes[8] = {
          static_cast<uint8_t>((be_data1w3 >> 5) & 0x1F),
          static_cast<uint8_t>(be_data1w3 & 0x1F),
          static_cast<uint8_t>((be_data1w4 >> 10) & 0x1F),
          static_cast<uint8_t>((be_data1w4 >> 5) & 0x1F),
          static_cast<uint8_t>(be_data1w4 & 0x1F),
          static_cast<uint8_t>((be_data1w5 >> 10) & 0x1F),
          static_cast<uint8_t>((be_data1w5 >> 5) & 0x1F),
          static_cast<uint8_t>(be_data1w5 & 0x1F),
      };

      string name;
      for (size_t x = 0; x < 8; x++) {
        char ch = s_rank_name_characters[char_indexes[x]];
        if (ch == 0) {
          break;
        }
        name += ch;
      }
      if (!name.empty()) {
        ret_tokens.emplace_back("(" + name + ")");
      }

    } else { // Not S-rank (extended name bits not set)
      parray<int8_t, 5> percentages(0);
      for (size_t x = 0; x < 3; x++) {
        uint8_t which = item.data1[6 + 2 * x];
        uint8_t value = item.data1[7 + 2 * x];
        if (which == 0) {
          continue;
        }
        if (which > 5) {
          ret_tokens.emplace_back(string_printf("!PC:%02hhX%02hhX", which, value));
        } else {
          percentages[which - 1] = value;
        }
      }
      if (!percentages.is_filled_with(0)) {
        ret_tokens.emplace_back(string_printf("%hhd/%hhd/%hhd/%hhd/%hhd",
            percentages[0], percentages[1], percentages[2], percentages[3], percentages[4]));
      }
    }

    // For armors, add the slots, unit modifiers, and/or DEF/EVP bonuses
  } else if (item.data1[0] == 0x01) {
    if (item.data1[1] == 0x03) { // Units
      uint16_t modifier = item.data1w[3];
      if (modifier == 0x0001 || modifier == 0x0002) {
        ret_tokens.back().append("+");
      } else if (modifier == 0x0003 || modifier == 0x0004) {
        ret_tokens.back().append("++");
      } else if (modifier == 0xFFFF || modifier == 0xFFFE) {
        ret_tokens.back().append("-");
      } else if (modifier == 0xFFFD || modifier == 0xFFFC) {
        ret_tokens.back().append("--");
      } else if (modifier != 0x0000) {
        ret_tokens.emplace_back(string_printf("!MD:%04hX", modifier));
      }

    } else { // Armor/shields
      if (item.data1[5] > 0) {
        if (item.data1[5] == 1) {
          ret_tokens.emplace_back("(1 slot)");
        } else {
          ret_tokens.emplace_back(string_printf("(%hhu slots)", item.data1[5]));
        }
      }
      if (item.data1w[3] != 0) {
        ret_tokens.emplace_back(string_printf("+%hdDEF",
            static_cast<int16_t>(item.data1w[3].load())));
      }
      if (item.data1w[4] != 0) {
        ret_tokens.emplace_back(string_printf("+%hdEVP",
            static_cast<int16_t>(item.data1w[4].load())));
      }
    }

    // For mags, add tons of info
  } else if (item.data1[0] == 0x02) {
    ret_tokens.emplace_back(string_printf("LV%hhu", item.data1[2]));

    uint16_t def = item.data1w[2];
    uint16_t pow = item.data1w[3];
    uint16_t dex = item.data1w[4];
    uint16_t mind = item.data1w[5];
    auto format_stat = +[](uint16_t stat) -> string {
      uint16_t level = stat / 100;
      uint8_t partial = stat % 100;
      if (partial == 0) {
        return string_printf("%hu", level);
      } else if (partial % 10 == 0) {
        return string_printf("%hu.%hhu", level, static_cast<uint8_t>(partial / 10));
      } else {
        return string_printf("%hu.%02hhu", level, partial);
      }
    };
    ret_tokens.emplace_back(format_stat(def) + "/" + format_stat(pow) + "/" + format_stat(dex) + "/" + format_stat(mind));
    ret_tokens.emplace_back(string_printf("%hhu%%", item.data2[0]));
    ret_tokens.emplace_back(string_printf("%hhuIQ", item.data2[1]));

    uint8_t flags = item.data2[2];
    if (flags & 7) {
      static const vector<const char*> pb_shortnames = {
          "F", "E", "G", "P", "L", "M&Y", "MG", "GR"};

      const char* pb_names[3] = {nullptr, nullptr, nullptr};
      uint8_t left_pb = item.mag_photon_blast_for_slot(2);
      uint8_t center_pb = item.mag_photon_blast_for_slot(0);
      uint8_t right_pb = item.mag_photon_blast_for_slot(1);
      if (left_pb != 0xFF) {
        pb_names[0] = pb_shortnames[left_pb];
      }
      if (center_pb != 0xFF) {
        pb_names[1] = pb_shortnames[center_pb];
      }
      if (right_pb != 0xFF) {
        pb_names[2] = pb_shortnames[right_pb];
      }

      string token = "PB:";
      for (size_t x = 0; x < 3; x++) {
        if (pb_names[x] == nullptr) {
          continue;
        }
        if (token.size() > 3) {
          token += ',';
        }
        token += pb_names[x];
      }
      ret_tokens.emplace_back(std::move(token));
    }

    try {
      ret_tokens.emplace_back(string_printf("(%s)", name_for_mag_color.at(item.data2[3])));
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(string_printf("(!CL:%02hhX)", item.data2[3]));
    }

    // For tools, add the amount (if applicable)
  } else if (item.data1[0] == 0x03) {
    if (item.max_stack_size() > 1) {
      ret_tokens.emplace_back(string_printf("x%hhu", item.data1[5]));
    }
  }

  string ret = join(ret_tokens, " ");
  if (item_parameter_table) {
    if (item.is_s_rank_weapon()) {
      return "$C4" + ret;
    } else if (item_parameter_table->is_item_rare(item)) {
      return "$C6" + ret;
    } else if (item.has_bonuses()) {
      return "$C2" + ret;
    } else {
      return "$C7" + ret;
    }
  } else {
    return ret;
  }
}

ItemData ItemNameIndex::parse_item_description(GameVersion version, const std::string& desc) const {
  try {
    return this->parse_item_description_phase(version, desc, false);
  } catch (const exception& e1) {
    try {
      return this->parse_item_description_phase(version, desc, true);
    } catch (const exception& e2) {
      try {
        string data = parse_data_string(desc);
        if (data.size() < 2) {
          throw runtime_error("item code too short");
        }
        if (data[0] > 4) {
          throw runtime_error("invalid item class");
        }
        if (data.size() > 16) {
          throw runtime_error("item code too long");
        }

        ItemData ret;
        if (data.size() <= 12) {
          memcpy(ret.data1.data(), data.data(), data.size());
        } else {
          memcpy(ret.data1.data(), data.data(), 12);
          memcpy(ret.data2.data(), data.data() + 12, data.size() - 12);
        }
        return ret;
      } catch (const exception& ed) {
        if (strcmp(e1.what(), e2.what())) {
          throw runtime_error(string_printf("cannot parse item description (as text 1: %s) (as text 2: %s) (as data: %s)", e1.what(), e2.what(), ed.what()));
        } else {
          throw runtime_error(string_printf("cannot parse item description (as text: %s) (as data: %s)", e1.what(), ed.what()));
        }
      }
    }
  }
}

ItemData ItemNameIndex::parse_item_description_phase(GameVersion version, const std::string& description, bool skip_special) const {
  ItemData ret;
  ret.data1d.clear(0);
  ret.id = 0xFFFFFFFF;
  ret.data2d = 0;

  string desc = tolower(description);
  if (ends_with(desc, " meseta")) {
    ret.data1[0] = 0x04;
    ret.data2d = stol(desc, nullptr, 10);
    return ret;
  }

  if (starts_with(desc, "disk:")) {
    auto tokens = split(desc, ' ');
    tokens[0] = tokens[0].substr(5); // Trim off "disk:"
    if ((tokens[0] == "reverser") || (tokens[0] == "ryuker")) {
      uint8_t tech = technique_for_name(tokens[0]);
      ret.data1[0] = 0x03;
      ret.data1[1] = 0x02;
      ret.data1[2] = 0x00;
      ret.data1[4] = tech;
    } else {
      if (tokens.size() != 2) {
        throw runtime_error("invalid tech disk format");
      }
      if (!starts_with(tokens[1], "lv.")) {
        throw runtime_error("invalid tech disk level");
      }
      uint8_t tech = technique_for_name(tokens[0]);
      uint8_t level = stoul(tokens[1].substr(3), nullptr, 10) - 1;
      ret.data1[0] = 0x03;
      ret.data1[1] = 0x02;
      ret.data1[2] = level;
      ret.data1[4] = tech;
    }
    return ret;
  }

  bool is_wrapped = starts_with(desc, "wrapped ");
  if (is_wrapped) {
    desc = desc.substr(8);
  }

  // TODO: It'd be nice to be able to parse S-rank weapon specials here too.
  uint8_t weapon_special = 0;
  if (!skip_special) {
    for (size_t z = 0; z < name_for_weapon_special.size(); z++) {
      if (!name_for_weapon_special[z]) {
        continue;
      }
      string prefix = tolower(name_for_weapon_special[z]);
      prefix += ' ';
      if (starts_with(desc, prefix)) {
        weapon_special = z;
        desc = desc.substr(prefix.size());
        break;
      }
    }
  }

  const map<string, shared_ptr<ItemMetadata>>* name_index;
  switch (version) {
    case GameVersion::DC:
    case GameVersion::PC:
      name_index = &this->v2_name_index;
      break;
    case GameVersion::GC:
    case GameVersion::XB:
      name_index = &this->v3_name_index;
      break;
    case GameVersion::BB:
      name_index = &this->v4_name_index;
      break;
    default:
      throw logic_error("invalid game version");
  }

  auto name_it = name_index->lower_bound(desc);
  // Look up to 3 places before the lower bound. We have to do this to catch
  // cases like Sange vs. Sange & Yasha - if the input is like "Sange 0/...",
  // then we'll see Sange & Yasha first, which we should skip.
  size_t lookback = 0;
  while (lookback < 4) {
    if (name_it != name_index->end() && desc.starts_with(name_it->first)) {
      break;
    } else if (name_it == name_index->begin()) {
      throw runtime_error("no such item");
    } else {
      name_it--;
      lookback++;
    }
  }
  if (lookback >= 4) {
    throw runtime_error("item not found: " + desc);
  }

  desc = desc.substr(name_it->first.size());
  if (starts_with(desc, " ")) {
    desc = desc.substr(1);
  }

  uint32_t primary_identifier = name_it->second->primary_identifier;
  ret.data1[0] = (primary_identifier >> 16) & 0xFF;
  ret.data1[1] = (primary_identifier >> 8) & 0xFF;
  ret.data1[2] = primary_identifier & 0xFF;

  if (ret.data1[0] == 0x00) {
    // Weapons: add special, grind and percentages (or name, if S-rank)
    ret.data1[4] = weapon_special | (is_wrapped ? 0x40 : 0x00);

    auto tokens = split(desc, ' ');
    for (auto& token : tokens) {
      if (token.empty()) {
        continue;
      }
      if (starts_with(token, "+")) {
        token = token.substr(1);
        ret.data1[3] = stoul(token, nullptr, 10);

      } else if (ret.is_s_rank_weapon()) {
        if (token.size() > 8) {
          throw runtime_error("s-rank name too long");
        }

        uint8_t char_indexes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (size_t z = 0; z < token.size(); z++) {
          char ch = toupper(token[z]);
          const char* pos = strchr(s_rank_name_characters, ch);
          if (!pos) {
            throw runtime_error(string_printf("s-rank name contains invalid character %02hhX (%c)", ch, ch));
          }
          char_indexes[z] = (pos - s_rank_name_characters);
        }

        ret.data1w[3] = bswap16(0x8000 | (char_indexes[1] & 0x1F) | ((char_indexes[0] & 0x1F) << 5));
        ret.data1w[4] = bswap16(0x8000 | (char_indexes[4] & 0x1F) | ((char_indexes[3] & 0x1F) << 5) | ((char_indexes[2] & 0x1F) << 10));
        ret.data1w[5] = bswap16(0x8000 | (char_indexes[7] & 0x1F) | ((char_indexes[6] & 0x1F) << 5) | ((char_indexes[5] & 0x1F) << 10));

      } else {
        auto p_tokens = split(token, '/');
        if (p_tokens.size() > 5) {
          throw runtime_error("invalid bonuses token");
        }
        uint8_t bonus_index = 0;
        for (size_t z = 0; z < p_tokens.size(); z++) {
          int8_t bonus_value = stol(p_tokens[z], nullptr, 10);
          if (bonus_value == 0) {
            continue;
          }
          if (bonus_index >= 3) {
            throw runtime_error("weapon has too many bonuses");
          }
          ret.data1[6 + (2 * bonus_index)] = z + 1;
          ret.data1[7 + (2 * bonus_index)] = static_cast<uint8_t>(bonus_value);
          bonus_index++;
        }
      }
    }

  } else if (ret.data1[0] == 0x01) {
    if (ret.data1[1] == 0x03) { // Unit
      static const unordered_map<string, uint16_t> modifiers({
          {"--", 0xFFFC},
          {"-", 0xFFFE},
          {"", 0x0000},
          {"+", 0x0002},
          {"++", 0x0004},
      });
      ret.data1w[3] = modifiers.at(desc);

    } else { // Armor/shield
      for (const auto& token : split(desc, ' ')) {
        if (token.empty()) {
          continue;
        } else if (!starts_with(token, "+")) {
          throw runtime_error("invalid armor/shield modifier");
        }
        if (ends_with(token, "def")) {
          ret.data1w[3] = static_cast<uint16_t>(stol(token.substr(1, token.size() - 4), nullptr, 10));
        } else if (ends_with(token, "evp")) {
          ret.data1w[4] = static_cast<uint16_t>(stol(token.substr(1, token.size() - 4), nullptr, 10));
        } else {
          ret.data1[5] = stoul(token.substr(1), nullptr, 10);
        }
      }
    }

    if (is_wrapped) {
      ret.data1[4] |= 0x40;
    }

  } else if (ret.data1[0] == 0x02) {
    for (const auto& token : split(desc, ' ')) {
      if (token.empty()) {
        continue;
      } else if (starts_with(token, "pb:")) { // Photon blasts
        auto pb_tokens = split(token.substr(3), ',');
        if (pb_tokens.size() > 3) {
          throw runtime_error("too many photon blasts specified");
        }
        static const unordered_map<string, uint8_t> name_to_pb_num({
            {"f", 0},
            {"e", 1},
            {"g", 2},
            {"p", 3},
            {"l", 4},
            {"m", 5},
            {"my", 5},
            {"m&y", 5},
        });
        for (const auto& pb_token : pb_tokens) {
          ret.add_mag_photon_blast(name_to_pb_num.at(pb_token));
        }
      } else if (ends_with(token, "%")) { // Synchro
        ret.data2[0] = stoul(token.substr(0, token.size() - 1), nullptr, 10);
      } else if (ends_with(token, "iq")) { // IQ
        ret.data2[1] = stoul(token.substr(0, token.size() - 2), nullptr, 10);
      } else if (!token.empty() && isdigit(token[0])) { // Stats
        auto s_tokens = split(token, '/');
        if (s_tokens.size() != 4) {
          throw runtime_error("incorrect stat count");
        }
        for (size_t z = 0; z < 4; z++) {
          auto n_tokens = split(s_tokens[z], '.');
          if (n_tokens.size() == 0 || n_tokens.size() > 2) {
            throw logic_error("incorrect stats argument format");
          } else if ((n_tokens.size() == 1) || (n_tokens[1].size() == 0)) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100;
          } else if (n_tokens[1].size() == 1) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100 + stoul(n_tokens[1], nullptr, 10) * 10;
          } else if (n_tokens[1].size() == 2) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100 + stoul(n_tokens[1], nullptr, 10);
          } else {
            throw runtime_error("incorrect stat format");
          }
        }
        ret.data1[2] = ret.compute_mag_level();
      } else { // Color
        ret.data2[3] = mag_color_for_name.at(token);
      }
    }

    if (is_wrapped) {
      ret.data2[2] |= 0x40;
    }
  } else if (ret.data1[0] == 0x03) {
    if (ret.max_stack_size() > 1) {
      if (starts_with(desc, "x")) {
        ret.data1[5] = stoul(desc.substr(1), nullptr, 10);
      } else {
        ret.data1[5] = 1;
      }
    } else if (!desc.empty()) {
      throw runtime_error("item cannot be stacked");
    }

    if (is_wrapped) {
      if (ret.is_stackable()) {
        throw runtime_error("stackable items cannot be wrapped");
      } else {
        ret.data1[3] |= 0x40;
      }
    }
  } else {
    throw logic_error("invalid item class");
  }

  return ret;
}
