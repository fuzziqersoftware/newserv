#include "ItemNameIndex.hh"

#include <algorithm>

#include "StaticGameData.hh"

ItemNameIndex::ItemNameIndex(
    std::shared_ptr<const ItemParameterTable> item_parameter_table,
    std::shared_ptr<const ItemData::StackLimits> limits,
    const std::vector<std::string>& name_coll)
    : item_parameter_table(item_parameter_table), limits(limits) {

  for (uint32_t primary_identifier : item_parameter_table->compute_all_valid_primary_identifiers()) {
    // 00000000 is a valid primary identifier but not a valid weapon; skip it
    if (primary_identifier == 0x00000000) {
      continue;
    }

    const std::string* name = nullptr;
    bool is_es_weapon = false;
    try {
      ItemData item = ItemData::from_primary_identifier(*this->limits, primary_identifier);
      is_es_weapon = item.is_s_rank_weapon();
      name = &name_coll.at(item_parameter_table->get_item_id(item));
    } catch (const std::out_of_range&) {
    }

    if (name) {
      auto meta = std::make_shared<ItemMetadata>();
      meta->primary_identifier = primary_identifier;
      meta->name = *name;
      this->primary_identifier_index.emplace(meta->primary_identifier, meta);
      this->name_index.emplace(phosg::tolower(meta->name), meta);
      if (is_es_weapon && ((primary_identifier & 0x0000FFFF) == 0x00000000)) {
        this->es_name_index.emplace(phosg::tolower(meta->name), meta);
      }
    }
  }
}

static std::string s_rank_name_characters("\0ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", 0x20);

// clang-format off
static const std::array<const char*, 0x29> name_for_weapon_special = {
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

const std::array<const char*, 0x11> name_for_s_rank_special = {
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

std::string ItemNameIndex::describe_item(const ItemData& item, uint8_t flags) const {
  bool include_color_escapes = flags & ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES;
  bool name_only = flags & ItemNameIndex::Flag::NAME_ONLY;

  if (item.data1[0] == 0x04) {
    return std::format("{}{} Meseta", include_color_escapes ? "$C7" : "", item.data2d);
  }

  std::vector<std::string> ret_tokens;

  // For weapons, specials appear before the weapon name
  bool is_unidentified = false;
  if ((item.data1[0] == 0x00) && (item.data1[4] != 0x00) && !item.is_s_rank_weapon()) {
    is_unidentified = item.data1[4] & 0x80;
    uint8_t special_id = item.data1[4] & 0x3F;
    if (!name_only) {
      if (item.data1[4] & 0x40) {
        ret_tokens.emplace_back("Wrapped");
      }
      if (is_unidentified) {
        ret_tokens.emplace_back("????");
      }
    }
    if (special_id) {
      try {
        ret_tokens.emplace_back(name_for_weapon_special.at(special_id));
      } catch (const std::out_of_range&) {
        ret_tokens.emplace_back(std::format("!SP:{:02X}", special_id));
      }
    }
  }
  if (!name_only && (item.data1[0] == 0x00) && (item.data1[2] != 0x00) && item.is_s_rank_weapon()) {
    try {
      ret_tokens.emplace_back(name_for_s_rank_special.at(item.data1[2]));
    } catch (const std::out_of_range&) {
      ret_tokens.emplace_back(std::format("!SSP:{:02X}", item.data1[2]));
    }
  }

  // Armors, shields, and units (0x01) can be wrapped, as can mags (0x02) and non-stackable tools (0x03). However, each
  // of these item classes has its flags in a different location.
  if (!name_only &&
      (((item.data1[0] == 0x01) && (item.data1[4] & 0x40)) ||
          ((item.data1[0] == 0x02) && (item.data2[2] & 0x40)) ||
          ((item.data1[0] == 0x03) && !item.is_stackable(*this->limits) && (item.data1[3] & 0x40)))) {
    ret_tokens.emplace_back("Wrapped");
  }

  // Add the item name
  uint32_t primary_identifier = item.primary_identifier();
  if ((primary_identifier & 0xFFFF0000) == 0x03020000) {
    std::string technique_name;
    try {
      technique_name = tech_id_to_name.at(item.data1[4]);
      technique_name[0] = toupper(technique_name[0]);
    } catch (const std::out_of_range&) {
      technique_name = std::format("!TD:{:02X}", item.data1[4]);
    }
    // Hide the level for Reverser and Ryuker, unless the level isn't 1
    if (name_only || ((item.data1[2] == 0) && ((item.data1[4] == 0x0E) || (item.data1[4] == 0x11)))) {
      ret_tokens.emplace_back(std::format("Disk:{}", technique_name));
    } else {
      ret_tokens.emplace_back(std::format("Disk:{} Lv.{}", technique_name, item.data1[2] + 1));
    }
  } else {
    try {
      auto meta = this->primary_identifier_index.at(primary_identifier);
      ret_tokens.emplace_back(meta->name);
    } catch (const std::out_of_range&) {
      ret_tokens.emplace_back(std::format("!ID:{:08X}", primary_identifier));
    }
  }

  if (item.data1[0] == 0x00) {
    // For weapons, add the grind and bonuses, or S-rank name if applicable
    if (!name_only && item.data1[3] > 0) {
      ret_tokens.emplace_back(std::format("+{}", item.data1[3]));
    }

    if (item.is_s_rank_weapon()) {
      // S-rank weapons have names instead of percent bonuses. The name is encoded as 9 5-bit characters in the bytes
      // where the bonuses usually go. The first character does not appear when the name is rendered; instead, it's
      // used to determine if the weapon type name should appear or not. Unlike the client, we check the first
      // character directly and don't bother decoding it.
      uint16_t be_data1w3 = phosg::bswap16(item.data1w[3]);
      uint16_t be_data1w4 = phosg::bswap16(item.data1w[4]);
      uint16_t be_data1w5 = phosg::bswap16(item.data1w[5]);
      bool hide_type_name = ((be_data1w3 & 0x7C00) == 0x0800);
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

      std::string name;
      for (size_t x = 0; x < 8; x++) {
        char ch = s_rank_name_characters.at(char_indexes[x]);
        if (ch == 0) {
          break;
        }
        name += ch;
      }
      if (!name.empty()) {
        if (hide_type_name) {
          ret_tokens.emplace_back("\"" + name + "\"");
        } else {
          ret_tokens.emplace_back("(" + name + ")");
        }
      }

    } else if (!name_only) { // Not S-rank (extended name bits not set)
      size_t num_bonuses = 3;

      if (item.data1[10] & 0x80) {
        ret_tokens.emplace_back(std::format("K:{}", item.get_kill_count()));
        num_bonuses = 2;
      }

      parray<int8_t, 5> bonuses(0);
      for (size_t x = 0; x < num_bonuses; x++) {
        uint8_t which = item.data1[6 + 2 * x];
        uint8_t value = item.data1[7 + 2 * x];
        if (which == 0) {
          continue;
        }
        if (which > 5) {
          ret_tokens.emplace_back(std::format("!PC:{:02X}{:02X}", which, value));
        } else {
          bonuses[which - 1] = value;
        }
      }
      if (!bonuses.is_filled_with(0)) {
        bool should_include_hit = (bonuses[4] != 0);
        bool should_highlight_hit = include_color_escapes && (bonuses[4] > 0);
        const char* color_prefix = include_color_escapes ? "$C7" : "";
        if (should_include_hit) {
          ret_tokens.emplace_back(std::format("{}{}/{}/{}/{}/{}{}",
              color_prefix, bonuses[0], bonuses[1], bonuses[2], bonuses[3],
              (should_highlight_hit ? "$C6" : ""), bonuses[4]));
        } else {
          ret_tokens.emplace_back(std::format("{}{}/{}/{}/{}",
              color_prefix, bonuses[0], bonuses[1], bonuses[2], bonuses[3]));
        }
      }
    }

  } else if (item.data1[0] == 0x01) {
    // For armors, add the slots, unit modifiers, and/or DEF/EVP bonuses
    if (item.data1[1] == 0x03) { // Units
      int16_t modifier = item.data1w[3];
      if (modifier == 1 || modifier == 2) {
        ret_tokens.back().append("+");
      } else if (modifier >= 3) {
        ret_tokens.back().append("++");
      } else if (modifier == -1 || modifier == -2) {
        ret_tokens.back().append("-");
      } else if (modifier <= -3) {
        ret_tokens.back().append("--");
      } else if (modifier != 0) {
        ret_tokens.emplace_back(std::format("!MD:{:04X}", modifier));
      }

      if (!name_only && (item.data1[10] & 0x80)) {
        ret_tokens.emplace_back(std::format("K:{}", item.get_kill_count()));
      }

    } else if (!name_only) { // Armor/shields
      if (item.data1[5] > 0) {
        if (item.data1[5] == 1) {
          ret_tokens.emplace_back("(1 slot)");
        } else {
          ret_tokens.emplace_back(std::format("({} slots)", item.data1[5]));
        }
      }
      if (item.data1w[3] != 0) {
        ret_tokens.emplace_back(std::format("+{}DEF",
            static_cast<int16_t>(item.data1w[3])));
      }
      if (item.data1w[4] != 0) {
        ret_tokens.emplace_back(std::format("+{}EVP",
            static_cast<int16_t>(item.data1w[4])));
      }
    }

  } else if (!name_only && (item.data1[0] == 0x02)) {
    // For mags, add tons of info
    ret_tokens.emplace_back(std::format("LV{}", item.data1[2]));

    uint16_t def = item.data1w[2];
    uint16_t pow = item.data1w[3];
    uint16_t dex = item.data1w[4];
    uint16_t mind = item.data1w[5];
    auto format_stat = +[](uint16_t stat) -> std::string {
      uint16_t level = stat / 100;
      uint8_t partial = stat % 100;
      if (partial == 0) {
        return std::format("{}", level);
      } else if (partial % 10 == 0) {
        return std::format("{}.{}", level, static_cast<uint8_t>(partial / 10));
      } else {
        return std::format("{}.{:02}", level, partial);
      }
    };
    ret_tokens.emplace_back(format_stat(def) + "/" + format_stat(pow) + "/" + format_stat(dex) + "/" + format_stat(mind));
    ret_tokens.emplace_back(std::format("{}{}", item.data2[0], include_color_escapes ? "%%" : "%"));
    ret_tokens.emplace_back(std::format("{}IQ", item.data2[1]));

    uint8_t flags = item.data2[2];
    if (flags & 7) {
      static const std::vector<const char*> pb_shortnames{"F", "E", "G", "P", "L", "M&Y", "MG", "GR"};

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

      std::string token = "PB:";
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
      ret_tokens.emplace_back(std::format("({})", name_for_mag_color.at(item.data2[3])));
    } catch (const std::out_of_range&) {
      ret_tokens.emplace_back(std::format("(!CL:{:02X})", item.data2[3]));
    }

  } else if (item.data1[0] == 0x03) {
    // For tools, add the amount (if applicable)
    if (!name_only && (item.max_stack_size(*this->limits) > 1)) {
      ret_tokens.emplace_back(std::format("x{}", item.data1[5]));
    }
  }

  std::string ret = phosg::join(ret_tokens, " ");
  if (include_color_escapes) {
    if (is_unidentified) {
      return "$C3" + ret;
    } else if (item.is_s_rank_weapon()) {
      return "$C4" + ret;
    } else if (this->item_parameter_table->is_item_rare(item)) {
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

ItemData ItemNameIndex::parse_item_description(const std::string& desc) const {
  ItemData ret;
  try {
    ret = this->parse_item_description_phase(desc, false);
  } catch (const std::exception& e1) {
    try {
      ret = this->parse_item_description_phase(desc, true);
    } catch (const std::exception& e2) {
      try {
        ret = ItemData::from_data(phosg::parse_data_string(desc));
      } catch (const std::exception& ed) {
        if (strcmp(e1.what(), e2.what())) {
          throw std::runtime_error(std::format("cannot parse item description \"{}\" (as text 1: {}) (as text 2: {}) (as data: {})",
              desc, e1.what(), e2.what(), ed.what()));
        } else {
          throw std::runtime_error(std::format("cannot parse item description \"{}\" (as text: {}) (as data: {})",
              desc, e1.what(), ed.what()));
        }
      }
    }
  }
  ret.enforce_stack_size_limits(*this->limits);
  return ret;
}

ItemData ItemNameIndex::parse_item_description_phase(const std::string& description, bool skip_special) const {
  ItemData ret;
  ret.data1d.clear(0);
  ret.id = 0xFFFFFFFF;
  ret.data2d = 0;

  std::string desc = phosg::tolower(description);
  if (desc.ends_with(" meseta")) {
    ret.data1[0] = 0x04;
    ret.data2d = std::stol(desc, nullptr, 10);
    return ret;
  }

  if (desc.starts_with("es ")) {
    auto parse_name = [&](const std::string& token) -> void {
      if (token.size() > 8) {
        throw std::runtime_error("s-rank name too long");
      }

      uint8_t char_indexes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
      for (size_t z = 0; z < token.size(); z++) {
        char ch = toupper(token[z]);
        size_t pos = s_rank_name_characters.find(ch);
        if (pos == std::string::npos) {
          throw std::runtime_error(std::format("s-rank name contains invalid character {:02X} ({})", ch, ch));
        }
        char_indexes[z] = pos;
      }

      ret.data1w[3] = phosg::bswap16(0x8000 | (char_indexes[1] & 0x1F) | ((char_indexes[0] & 0x1F) << 5));
      ret.data1w[4] = phosg::bswap16(0x8000 | (char_indexes[4] & 0x1F) | ((char_indexes[3] & 0x1F) << 5) | ((char_indexes[2] & 0x1F) << 10));
      ret.data1w[5] = phosg::bswap16(0x8000 | (char_indexes[7] & 0x1F) | ((char_indexes[6] & 0x1F) << 5) | ((char_indexes[5] & 0x1F) << 10));
    };
    auto parse_special = [&](const std::string& token) -> bool {
      for (size_t z = 0; z < name_for_s_rank_special.size(); z++) {
        if (name_for_s_rank_special[z] && (token == phosg::tolower(name_for_s_rank_special[z]))) {
          ret.data1[2] = z;
          return true;
        }
      }
      return false;
    };
    auto parse_grind = [&](const std::string& token) -> bool {
      if (token.starts_with('+')) {
        ret.data1[3] = stoul(token.substr(1), nullptr, 0);
        return true;
      }
      return false;
    };
    auto parse_type = [&](const std::string& token) -> void {
      const auto& meta = this->es_name_index.at(token);
      if (meta->primary_identifier & 0xFF00FFFF) {
        throw std::runtime_error("ES weapon has invalid bits in primary identifier");
      }
      ret.data1[1] = meta->primary_identifier >> 16;
    };

    // Syntax: ES [NAME] [SPECIAL] TYPE [+GRIND]
    auto tokens = phosg::split(desc, ' ');
    switch (tokens.size()) {
      case 0:
      case 1:
        throw std::runtime_error("ES weapon type is missing");

      case 2:
        // Must be ES TYPE
        parse_name(tokens[1]);
        break;

      case 3:
        // Any of the following:
        // ES TYPE +N
        // ES SPECIAL TYPE
        // ES NAME TYPE
        if (parse_grind(tokens[2])) {
          parse_type(tokens[1]);
        } else if (parse_special(tokens[1])) {
          parse_type(tokens[2]);
        } else {
          parse_name(tokens[1]);
          parse_type(tokens[2]);
        }
        break;
      case 4:
        // Any of the following:
        // ES SPECIAL TYPE +N
        // ES NAME TYPE +N
        // ES NAME SPECIAL TYPE
        if (parse_grind(tokens[3])) {
          if (!parse_special(tokens[1])) {
            parse_name(tokens[1]);
          }
          parse_type(tokens[2]);
        } else {
          parse_name(tokens[1]);
          if (!parse_special(tokens[2])) {
            throw std::runtime_error("invalid ES special");
          }
          parse_type(tokens[3]);
        }
        break;
      case 5:
        // Must be ES NAME SPECIAL TYPE +N
        parse_name(tokens[1]);
        if (!parse_special(tokens[2])) {
          throw std::runtime_error("invalid ES special");
        }
        parse_type(tokens[3]);
        if (!parse_grind(tokens[4])) {
          throw std::runtime_error("invalid grind");
        }
        break;
      default:
        throw std::runtime_error("too many ES weapon tokens");
    }

    return ret;
  }

  if (desc.starts_with("disk:")) {
    auto tokens = phosg::split(desc, ' ');
    tokens[0] = tokens[0].substr(5); // Trim off "disk:"
    if ((tokens[0] == "reverser") || (tokens[0] == "ryuker")) {
      uint8_t tech = technique_for_name(tokens[0]);
      ret.data1[0] = 0x03;
      ret.data1[1] = 0x02;
      ret.data1[2] = 0x00;
      ret.data1[4] = tech;
    } else {
      if (tokens.size() != 2) {
        throw std::runtime_error("invalid tech disk format");
      }
      if (!tokens[1].starts_with("lv.")) {
        throw std::runtime_error("invalid tech disk level");
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

  bool is_wrapped = desc.starts_with("wrapped ");
  if (is_wrapped) {
    desc = desc.substr(8);
  }
  bool is_unidentified = desc.starts_with("?");
  if (is_unidentified) {
    size_t z;
    for (z = 1; z < desc.size(); z++) {
      if (desc[z] != ' ' && desc[z] != '?') {
        break;
      }
    }
    desc = desc.substr(z);
  }

  uint8_t weapon_special = 0;
  if (!skip_special) {
    for (size_t z = 0; z < name_for_weapon_special.size(); z++) {
      if (!name_for_weapon_special[z]) {
        continue;
      }
      std::string prefix = phosg::tolower(name_for_weapon_special[z]);
      prefix += ' ';
      if (desc.starts_with(prefix)) {
        weapon_special = z;
        desc = desc.substr(prefix.size());
        break;
      }
    }
  }

  auto name_it = this->name_index.lower_bound(desc);
  // Look up to 3 places before the lower bound. We have to do this to catch cases like Sange vs. Sange & Yasha - if
  // the input is like "Sange 0/...", then we'll see Sange & Yasha first, which we should skip.
  size_t lookback = 0;
  while (lookback < 4) {
    if (name_it != this->name_index.end() && desc.starts_with(name_it->first)) {
      break;
    } else if (name_it == this->name_index.begin()) {
      throw std::runtime_error("no such item");
    } else {
      name_it--;
      lookback++;
    }
  }
  if (lookback >= 4) {
    throw std::runtime_error("item not found: " + desc);
  }

  desc = desc.substr(name_it->first.size());
  if (desc.starts_with(" ")) {
    desc = desc.substr(1);
  }

  // Tech disks should have already been handled above, so we don't need to special-case 0302xxxx identifiers here.
  uint32_t primary_identifier = name_it->second->primary_identifier;
  ret.data1[0] = (primary_identifier >> 24) & 0xFF;
  ret.data1[1] = (primary_identifier >> 16) & 0xFF;
  ret.data1[2] = (primary_identifier >> 8) & 0xFF;

  if (ret.data1[0] == 0x00) {
    // Weapons: add special, grind and percentages (or name, if S-rank) and kill count if unsealable
    ret.data1[4] = weapon_special | (is_wrapped ? 0x40 : 0x00) | (is_unidentified ? 0x80 : 0x00);

    bool kill_count_set = false;
    auto tokens = phosg::split(desc, ' ');
    for (auto& token : tokens) {
      if (token.empty()) {
        continue;
      }
      if (token.starts_with("+")) {
        token = token.substr(1);
        ret.data1[3] = stoul(token, nullptr, 10);

      } else if (ret.is_s_rank_weapon()) {
        throw std::runtime_error("ES weapon must be prefixed with \"ES\"");

      } else if (token.starts_with("k:")) {
        ret.set_kill_count(stoul(token.substr(2), nullptr, 0));
        kill_count_set = true;

      } else {
        auto p_tokens = phosg::split(token, '/');
        if (p_tokens.size() > 5) {
          throw std::runtime_error("invalid bonuses token");
        }
        uint8_t max_bonuses = this->item_parameter_table->is_unsealable_item(ret) ? 2 : 3;
        uint8_t bonus_index = 0;
        for (size_t z = 0; z < p_tokens.size(); z++) {
          int8_t bonus_value = stol(p_tokens[z], nullptr, 10);
          if (bonus_value == 0) {
            continue;
          }
          if (bonus_index >= max_bonuses) {
            throw std::runtime_error("weapon has too many bonuses");
          }
          ret.data1[6 + (2 * bonus_index)] = z + 1;
          ret.data1[7 + (2 * bonus_index)] = static_cast<uint8_t>(bonus_value);
          bonus_index++;
        }
      }
    }

    if (this->item_parameter_table->is_unsealable_item(ret) && !kill_count_set) {
      ret.set_kill_count(0);
    }

  } else if (ret.data1[0] == 0x01) {
    if (ret.data1[1] == 0x03) { // Unit
      static const std::unordered_map<std::string, uint16_t> modifiers{
          {"--", 0xFFFC}, {"-", 0xFFFE}, {"", 0x0000}, {"+", 0x0002}, {"++", 0x0004}};
      bool kill_count_set = false;
      for (const auto& token : phosg::split(desc, ' ')) {
        if (token == "--") {
          ret.data1w[3] = 0xFFFC;
        } else if (token == "-") {
          ret.data1w[3] = 0xFFFE;
        } else if (token == "+") {
          ret.data1w[3] = 0x0002;
        } else if (token == "++") {
          ret.data1w[3] = 0x0004;
        } else if (token.starts_with('+')) {
          ret.data1w[3] = stoul(token.substr(1), nullptr, 0);
        } else if (token.starts_with('-')) {
          ret.data1w[3] = static_cast<uint16_t>(stol(token, nullptr, 0));
        } else if (token.starts_with("k:")) {
          ret.set_kill_count(stoul(token.substr(2), nullptr, 0));
          kill_count_set = true;
        }
      }

      if (this->item_parameter_table->is_unsealable_item(ret) && !kill_count_set) {
        ret.set_kill_count(0);
      }

    } else { // Armor/shield
      for (const auto& token : phosg::split(desc, ' ')) {
        if (token.empty()) {
          continue;
        } else if (!token.starts_with("+")) {
          throw std::runtime_error("invalid armor/shield modifier");
        }
        if (token.ends_with("def")) {
          ret.data1w[3] = static_cast<uint16_t>(stol(token.substr(1, token.size() - 4), nullptr, 10));
        } else if (token.ends_with("evp")) {
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
    for (const auto& token : phosg::split(desc, ' ')) {
      if (token.empty()) {
        continue;
      } else if (token.starts_with("pb:")) { // Photon blasts
        auto pb_tokens = phosg::split(token.substr(3), ',');
        if (pb_tokens.size() > 3) {
          throw std::runtime_error("too many photon blasts specified");
        }
        static const std::unordered_map<std::string, uint8_t> name_to_pb_num{
            {"f", 0}, {"e", 1}, {"g", 2}, {"p", 3}, {"l", 4}, {"m", 5}, {"my", 5}, {"m&y", 5}};
        for (const auto& pb_token : pb_tokens) {
          ret.add_mag_photon_blast(name_to_pb_num.at(pb_token));
        }
      } else if (token.ends_with("%")) { // Synchro
        ret.data2[0] = stoul(token.substr(0, token.size() - 1), nullptr, 10);
      } else if (token.ends_with("iq")) { // IQ
        ret.data2[1] = stoul(token.substr(0, token.size() - 2), nullptr, 10);
      } else if (!token.empty() && isdigit(token[0])) { // Stats
        auto s_tokens = phosg::split(token, '/');
        if (s_tokens.size() != 4) {
          throw std::runtime_error("incorrect stat count");
        }
        for (size_t z = 0; z < 4; z++) {
          auto n_tokens = phosg::split(s_tokens[z], '.');
          if (n_tokens.size() == 0 || n_tokens.size() > 2) {
            throw std::logic_error("incorrect stats argument format");
          } else if ((n_tokens.size() == 1) || (n_tokens[1].size() == 0)) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100;
          } else if (n_tokens[1].size() == 1) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100 + stoul(n_tokens[1], nullptr, 10) * 10;
          } else if (n_tokens[1].size() == 2) {
            ret.data1w[z + 2] = stoul(n_tokens[0], nullptr, 10) * 100 + stoul(n_tokens[1], nullptr, 10);
          } else {
            throw std::runtime_error("incorrect stat format");
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
    if (ret.max_stack_size(*this->limits) > 1) {
      if (desc.starts_with("x")) {
        ret.data1[5] = stoul(desc.substr(1), nullptr, 10);
      } else {
        ret.data1[5] = 1;
      }
    } else if (!desc.empty()) {
      throw std::runtime_error("item cannot be stacked");
    }

    if (is_wrapped) {
      if (ret.is_stackable(*this->limits)) {
        throw std::runtime_error("stackable items cannot be wrapped");
      } else {
        ret.data1[3] |= 0x40;
      }
    }
  } else {
    throw std::logic_error("invalid item class");
  }

  return ret;
}

void ItemNameIndex::print_table(FILE* stream) const {
  auto pmt = this->item_parameter_table;

  phosg::fwrite_fmt(stream, "WEAPONS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS FLAG ATPLO ATPHI ATPRQ MSTRQ ATARQ -MST- GND PH SP ATA SB(S1:AMT1,S2:AMT2) PJ 1X 1Y 2X 2Y CR --A1-- A4 A5 TB(TN:FL:AMOUNT, ...                     ) BF CL ST* USL ---DIVISOR--- NAME\n");
  for (size_t data1_1 = 0; data1_1 < pmt->num_weapon_classes(); data1_1++) {
    uint8_t weapon_class = pmt->get_weapon_kind(data1_1);
    float sale_divisor = pmt->get_sale_divisor(0x00, data1_1);
    std::string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_weapons_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& w = pmt->get_weapon(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(w.id);
      bool is_unsealable = pmt->is_unsealable_item(0x00, data1_1, data1_2);

      ItemData item;
      item.data1[0] = 0x00;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;
      std::string name = this->describe_item(item);

      const auto& stat_boost = pmt->get_stat_boost(w.stat_boost_entry_index);

      std::string tech_boosts_str;
      if (w.tech_boost_entry_index < pmt->num_tech_boosts()) {
        const auto& tech_boost = pmt->get_tech_boost(w.tech_boost_entry_index);
        tech_boosts_str = std::format("({:02X}:{:02X}:{:g},{:02X}:{:02X}:{:g},{:02X}:{:02X}:{:g})",
            tech_boost.tech_num1, tech_boost.flags1, tech_boost.amount1, tech_boost.tech_num2, tech_boost.flags2,
            tech_boost.amount2, tech_boost.tech_num3, tech_boost.flags3, tech_boost.amount3);
      }
      tech_boosts_str.resize(40, ' ');

      phosg::fwrite_fmt(stream,
          // CODE           =>     ID   TYPE   SKIN  PTS   FLAG ATP- ATP+ ATPR MSTR ATAR  MST  GND     PH     SP  ATA
          "  00{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:04X} {:5} {:5} {:5} {:5} {:5} {:5} {:3} {:02X} {:02X} {:3} "
          //   SB(    S1:AMT1  ,    S2:AMT2  )     PJ     1X     1Y     2X     2Y     CR --------A1--------     A4
          "{:02X}({:02X}:{:04X},{:02X}:{:04X}) {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}{:02X}{:02X} {:02X} "
          //   A5     TB(    TN:    FL: AMT,    TN:    FL: AMT,    TN:    FL: AMT)     BF     CL   ST* US DV NAME\n"
          "{:02X} {:02X}{} {:02X} {:02X} {:2}* {} {} {}\n",
          data1_1,
          data1_2,
          w.id,
          w.type,
          w.skin,
          w.team_points,
          w.usability_flags,
          w.atp_min,
          w.atp_max,
          w.atp_required,
          w.mst_required,
          w.ata_required,
          w.mst,
          w.max_grind,
          w.photon,
          w.special,
          w.ata,
          w.stat_boost_entry_index,
          stat_boost.stat1,
          stat_boost.amount1,
          stat_boost.stat2,
          stat_boost.amount2,
          w.projectile,
          w.trail1_x,
          w.trail1_y,
          w.trail2_x,
          w.trail2_y,
          w.color,
          w.unknown_a1[0],
          w.unknown_a1[1],
          w.unknown_a1[2],
          w.unknown_a4,
          w.unknown_a5,
          w.tech_boost_entry_index,
          tech_boosts_str,
          w.behavior_flags,
          weapon_class,
          stars,
          is_unsealable ? "YES" : " no",
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "ARMORS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS -DFP- -EVP- BP BE FLAG LVL EFR ETH EIC EDK ELT DFR EVR SB(S1:AMT1,S2:AMT2) TB(TN:FL:AMOUNT, ...                     ) FT A4 ST* ---DIVISOR--- NAME\n");
  for (size_t data1_1 = 1; data1_1 < 3; data1_1++) {
    float sale_divisor = pmt->get_sale_divisor(0x01, data1_1);
    std::string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_armors_or_shields_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& a = pmt->get_armor_or_shield(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(a.id);

      ItemData item;
      item.data1[0] = 0x01;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;
      std::string name = this->describe_item(item);

      auto& stat_boost = pmt->get_stat_boost(a.stat_boost_entry_index);

      std::string tech_boosts_str;
      if (a.tech_boost_entry_index < pmt->num_tech_boosts()) {
        const auto& tech_boost = pmt->get_tech_boost(a.tech_boost_entry_index);
        tech_boosts_str = std::format("({:02X}:{:02X}:{:g},{:02X}:{:02X}:{:g},{:02X}:{:02X}:{:g})",
            tech_boost.tech_num1, tech_boost.flags1, tech_boost.amount1, tech_boost.tech_num2, tech_boost.flags2,
            tech_boost.amount2, tech_boost.tech_num3, tech_boost.flags3, tech_boost.amount3);
      }
      tech_boosts_str.resize(40, ' ');

      phosg::fwrite_fmt(stream, "  01{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:5} {:5} {:02X} {:02X} {:04X} {:3} {:3} {:3} {:3} {:3} {:3} {:3} {:3} {:02X}({:02X}:{:04X},{:02X}:{:04X}) {:02X}{} {:02X} {:02X} {:2}* {} {}\n",
          data1_1,
          data1_2,
          a.id,
          a.type,
          a.skin,
          a.team_points,
          a.dfp,
          a.evp,
          a.block_particle,
          a.block_effect,
          a.usability_flags,
          static_cast<uint8_t>(a.required_level + 1),
          a.efr,
          a.eth,
          a.eic,
          a.edk,
          a.elt,
          a.dfp_range,
          a.evp_range,
          a.stat_boost_entry_index,
          stat_boost.stat1,
          stat_boost.amount1,
          stat_boost.stat2,
          stat_boost.amount2,
          a.tech_boost_entry_index,
          tech_boosts_str,
          a.flags_type,
          a.unknown_a4,
          stars,
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "UNITS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS STAT COUNT ST-MOD ST* ---DIVISOR--- NAME\n");
  {
    float sale_divisor = pmt->get_sale_divisor(0x01, 0x03);
    std::string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_units();
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& u = pmt->get_unit(data1_2);
      uint8_t stars = pmt->get_item_stars(u.id);

      ItemData item;
      item.data1[0] = 0x01;
      item.data1[1] = 0x03;
      item.data1[2] = data1_2;
      std::string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  0103{:02X} => {:08X} {:04X} {:04X} {:6} {:04X} {:5} {:6} {:2}* {} {}\n",
          data1_2, u.id, u.type, u.skin, u.team_points, u.stat, u.amount, u.modifier_amount, stars, divisor_str, name);
    }
  }

  phosg::fwrite_fmt(stream, "MAGS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS FTBL PB AC E1 E2 E3 E4 C1 C2 C3 C4 FLAG ---DIVISOR--- NAME\n");
  {
    size_t data1_1_limit = pmt->num_mags();
    for (size_t data1_1 = 0; data1_1 < data1_1_limit; data1_1++) {
      const auto& m = pmt->get_mag(data1_1);

      float sale_divisor = pmt->get_sale_divisor(0x02, data1_1);
      std::string divisor_str = std::format("{:g}", sale_divisor);
      divisor_str.resize(13, ' ');

      ItemData item;
      item.data1[0] = 0x02;
      item.data1[1] = data1_1;
      item.data1[2] = 0x00;
      std::string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  02{:02X}00 => {:08X} {:04X} {:04X} {:6} {:04X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:04X} {} {}\n",
          data1_1,
          m.id,
          m.type,
          m.skin,
          m.team_points,
          m.feed_table,
          m.photon_blast,
          m.activation,
          m.on_pb_full,
          m.on_low_hp,
          m.on_death,
          m.on_boss,
          m.on_pb_full_flag,
          m.on_low_hp_flag,
          m.on_death_flag,
          m.on_boss_flag,
          m.usability_flags,
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "TOOLS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS COUNT TECH -COST- ITEMFLAG NAME\n");
  for (size_t data1_1 = 0; data1_1 < pmt->num_tool_classes(); data1_1++) {
    size_t data1_2_limit = pmt->num_tools_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& t = pmt->get_tool(data1_1, data1_2);

      ItemData item;
      item.data1[0] = 0x03;
      item.data1[1] = data1_1;
      item.data1[(data1_1 == 0x02) ? 4 : 2] = data1_2;
      item.set_tool_item_amount(*this->limits, 1);
      std::string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  03{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:5} {:04X} {:6} {:08X} {}\n",
          data1_1, data1_2, t.id, t.type, t.skin, t.team_points, t.amount, t.tech, t.cost, t.item_flags, name);
    }
  }

  phosg::fwrite_fmt(stream, "MAX TECH LEVELS\n");
  phosg::fwrite_fmt(stream, "  CLASS     =>  F GF RF  B GB RB  Z GZ RZ GR DB JL ZL SH RY RS AT RV MG\n");
  for (size_t char_class = 0; char_class < 12; char_class++) {
    phosg::fwrite_fmt(stream, "  {:9} =>", name_for_char_class(char_class));
    for (size_t tech_num = 0; tech_num < 0x13; tech_num++) {
      uint8_t max_level = pmt->get_max_tech_level(char_class, tech_num) + 1;
      if (max_level == 0x00) {
        phosg::fwrite_fmt(stream, "   ");
      } else {
        phosg::fwrite_fmt(stream, " {:2}", max_level);
      }
    }
    phosg::fwrite_fmt(stream, "\n");
  }

  phosg::fwrite_fmt(stream, "MAG FEED TABLES\n");
  for (size_t table_index = 0; table_index < 8; table_index++) {
    static const char* names[11] = {
        "Monomate", "Dimate", "Trimate", "Monofluid", "Difluid", "Trifluid", "Antidote", "Antiparalysis",
        "Sol Atomizer", "Moon Atomizer", "Star Atomizer"};
    phosg::fwrite_fmt(stream, "  TABLE {:02X}         => -DEF -POW -DEX MIND -IQ- SYNC\n", table_index);
    for (size_t which = 0; which < 11; which++) {
      const auto& res = pmt->get_mag_feed_result(table_index, which);
      phosg::fwrite_fmt(stream, "    {:14} => {:4} {:4} {:4} {:4} {:4} {:4}\n",
          names[which], res.def, res.pow, res.dex, res.mind, res.iq, res.synchro);
    }
  }

  phosg::fwrite_fmt(stream, "SPECIAL DEFINITIONS\n");
  phosg::fwrite_fmt(stream, "  SP => TYPE COUNT ST* NAME\n");
  for (size_t index = 0; index < pmt->num_specials(); index++) {
    const auto& sp = pmt->get_special(index);
    uint8_t stars = pmt->get_special_stars(index);
    const char* name = "";
    if (index) {
      try {
        name = name_for_weapon_special.at(index);
      } catch (const std::out_of_range&) {
      }
    }
    phosg::fwrite_fmt(stream, "  {:02X} => {:04X} {:5} {:2}* {}\n", index, sp.type, sp.amount, stars, name);
  }

  phosg::fwrite_fmt(stream, "ITEM COMBINATIONS\n");
  phosg::fwrite_fmt(stream, "  ---USE + -EQUIP => RESULT MLV GND LVL CLS\n");
  for (const auto& [_, used_item_index] : pmt->item_combinations_index()) {
    for (const auto& [_, combo] : used_item_index) {
      phosg::fwrite_fmt(stream, "  {:02X}{:02X}{:02X} + {:02X}{:02X}{:02X} => {:02X}{:02X}{:02X}",
          combo->used_item[0], combo->used_item[1], combo->used_item[2],
          combo->equipped_item[0], combo->equipped_item[1], combo->equipped_item[2],
          combo->result_item[0], combo->result_item[1], combo->result_item[2]);
      if (combo->mag_level != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo->mag_level);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo->grind != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo->grind);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo->level != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo->level);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo->char_class != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}\n", combo->char_class);
      } else {
        phosg::fwrite_fmt(stream, "    \n");
      }
    }
  }

  phosg::fwrite_fmt(stream, "EVENT ITEMS\n");
  size_t num_events = pmt->num_events();
  for (size_t event_number = 0; event_number < num_events; event_number++) {
    phosg::fwrite_fmt(stream, "  EV {:3} => PRB\n", event_number);
    auto events_list = pmt->get_event_items(event_number);
    for (size_t z = 0; z < events_list.second; z++) {
      const auto& event_item = events_list.first[z];
      phosg::fwrite_fmt(stream, "  {:02X}{:02X}{:02X} => {:3}\n",
          event_item.item[0], event_item.item[1], event_item.item[2], event_item.probability);
    }
  }

  phosg::fwrite_fmt(stream, "PHOTON COLORS\n");
  phosg::fwrite_fmt(stream, "  ## => -BLEND?- (LIGHT) (DARK)\n");
  for (size_t z = 0; z < pmt->num_photon_colors(); z++) {
    const auto& pc = pmt->get_photon_color(z);
    phosg::fwrite_fmt(stream, "  {:02X} => {:08X} ({:g}, {:g}, {:g}, {:g}) ({:g}, {:g}, {:g}, {:g})\n",
        z, pc.blendable, pc.light_color.x, pc.light_color.y, pc.light_color.z, pc.light_color.t,
        pc.dark_color.x, pc.dark_color.y, pc.dark_color.z, pc.dark_color.t);
  }

  phosg::fwrite_fmt(stream, "WEAPON RANGES\n");
  phosg::fwrite_fmt(stream, "  ## => ---A3--- ---A4--- ---A5--- (A1) (A2)\n");
  for (size_t z = 0; z < pmt->num_weapon_ranges(); z++) {
    const auto& wr = pmt->get_weapon_range(z);
    phosg::fwrite_fmt(stream, "  {:02X} => {:08X} {:08X} {:08X} ({:g}) ({:g})\n",
        z, wr.horizontal_half_angle, wr.vertical_half_angle, wr.max_targets, wr.horizontal, wr.vertical);
  }

  phosg::fwrite_fmt(stream, "SALE DIVISORS\n");
  phosg::fwrite_fmt(stream, "  ARMOR = {:g}\n", pmt->get_sale_divisor(1, 1));
  phosg::fwrite_fmt(stream, "  SHIELD = {:g}\n", pmt->get_sale_divisor(1, 2));
  phosg::fwrite_fmt(stream, "  UNIT = {:g}\n", pmt->get_sale_divisor(1, 3));
  phosg::fwrite_fmt(stream, "  MAG = {:g}\n", pmt->get_sale_divisor(2, 0));

  auto write_data_string = [&](std::string_view data, size_t addr = 0) -> void {
    if (data.empty()) {
      phosg::fwrite_fmt(stream, "  (no data)\n");
    } else {
      auto data_str = phosg::format_data(data, addr);
      phosg::strip_trailing_whitespace(data_str);
      phosg::fwrite_fmt(stream, "  {}\n", phosg::str_replace_all(data_str, "\n", "\n  "));
    }
  };

  phosg::fwrite_fmt(stream, "STAR VALUES\n");
  write_data_string(pmt->get_star_value_table(), pmt->get_star_value_index_range().first);

  phosg::fwrite_fmt(stream, "UNKNOWN_A1\n");
  write_data_string(pmt->get_unknown_a1());

  phosg::fwrite_fmt(stream, "WEAPON EFFECTS\n");
  phosg::fwrite_fmt(stream, "  ## => -SOUND1- -VALUE1- -SOUND2- -VALUE2- -SOUND3- -VALUE3- -SOUND4- -VALUE4-\n");
  for (size_t z = 0; z < pmt->num_weapon_effects(); z++) {
    const auto& we = pmt->get_weapon_effect(z);
    phosg::fwrite_fmt(stream, "  {:02X} => {:08X} {:08X} {:08X} {:08X} \n",
        z, we.sound_id1, we.eff_value1, we.sound_id2, we.eff_value2,
        we.sound_id3, we.eff_value3, we.sound_id4, we.eff_value4);
  }

  phosg::fwrite_fmt(stream, "WEAPON STAT BOOST INDEX TABLE\n");
  write_data_string(pmt->get_weapon_stat_boost_index_table());

  phosg::fwrite_fmt(stream, "ARMOR STAT BOOST INDEX TABLE\n");
  write_data_string(pmt->get_armor_stat_boost_index_table());

  phosg::fwrite_fmt(stream, "SHIELD STAT BOOST INDEX TABLE\n");
  write_data_string(pmt->get_shield_stat_boost_index_table());

  phosg::fwrite_fmt(stream, "STAT BOOSTS\n");
  phosg::fwrite_fmt(stream, "  ## => BOOSTS\n");
  for (size_t z = 0; z < pmt->num_stat_boosts(); z++) {
    const auto& sb = pmt->get_stat_boost(z);
    static constexpr std::array<const char*, 0x10> stat_names{
        "ATP+", "ATA+", "EVP+", "DFP+", "MST+", "HP+", "LCK+", "ALL+",
        "ATP-", "ATA-", "EVP-", "DFP-", "MST-", "HP-", "LCK-", "ALL-"};
    std::string s;
    if (sb.stat1 > 0x10) {
      s = std::format("[{:02X}:{:04X}]", sb.stat1, sb.amount1);
    } else if (sb.stat1 > 0) {
      s = std::format("{}{}", stat_names[sb.stat1 - 1], sb.amount1);
    }
    if (sb.stat2) {
      if (!s.empty()) {
        s += ", ";
      }
      if (sb.stat2 > 0x10) {
        s += std::format("[{:02X}:{:04X}]", sb.stat2, sb.amount2);
      } else if (sb.stat2 > 0) {
        s += std::format("{}{}", stat_names[sb.stat2 - 1], sb.amount2);
      }
    }
    if (s.empty()) {
      s = "(none)";
    }
    phosg::fwrite_fmt(stream, "  {:02X} => {}\n", z, s);
  }

  phosg::fwrite_fmt(stream, "SHIELD EFFECTS\n");
  phosg::fwrite_fmt(stream, "  ## => -SOUND1- -PITCH--\n");
  for (size_t z = 0; z < pmt->num_shield_effects(); z++) {
    const auto& se = pmt->get_shield_effect(z);
    phosg::fwrite_fmt(stream, "  {:02X} => {:08X} {:08X}\n", z, se.sound_id, se.sound_pitch);
  }

  phosg::fwrite_fmt(stream, "SOUND REMAPS\n");
  phosg::fwrite_fmt(stream, "  -SOUND1- => RT:[...] CC:[...]\n");
  for (const auto& remap : pmt->get_all_sound_remaps()) {
    std::string rt_str;
    for (uint32_t rt_sound_id : remap.by_rt_index) {
      if (!rt_str.empty()) {
        rt_str += ",";
      }
      rt_str += std::format("{:08X}", rt_sound_id);
    }
    std::string cc_str;
    for (uint32_t cc_sound_id : remap.by_char_class) {
      if (!cc_str.empty()) {
        cc_str += ",";
      }
      cc_str += std::format("{:08X}", cc_sound_id);
    }
    phosg::fwrite_fmt(stream, "  {:08X} => RT:[{}] CC:[{}]\n", remap.sound_id, rt_str, cc_str);
  }

  phosg::fwrite_fmt(stream, "TECH BOOSTS\n");
  phosg::fwrite_fmt(stream, "  ## => BOOSTS\n");
  for (size_t z = 0; z < pmt->num_tech_boosts(); z++) {
    const auto& tb = pmt->get_tech_boost(z);
    std::string s;
    if (tb.amount1) {
      s += std::format("{:02X}:{:02X}:{:g}", tb.tech_num1, tb.flags1, tb.amount1);
    }
    if (tb.amount2) {
      s += std::format("{:02X}:{:02X}:{:g}", tb.tech_num2, tb.flags2, tb.amount2);
    }
    if (tb.amount3) {
      s += std::format("{:02X}:{:02X}:{:g}", tb.tech_num3, tb.flags3, tb.amount3);
    }
    phosg::fwrite_fmt(stream, "  {:02X} => {}\n", z, s);
  }

  phosg::fwrite_fmt(stream, "UNSEALABLE ITEMS\n");
  phosg::fwrite_fmt(stream, "  -ITEM- NAME\n");
  std::vector<uint32_t> unsealable_items;
  for (uint32_t item_code : pmt->all_unsealable_items()) {
    unsealable_items.emplace_back(item_code);
  }
  std::sort(unsealable_items.begin(), unsealable_items.end());
  for (uint32_t item_code : unsealable_items) {
    ItemData item;
    item.data1[0] = item_code >> 16;
    item.data1[1] = item_code >> 8;
    item.data1[2] = item_code;
    phosg::fwrite_fmt(stream, "  {:06X} {}\n", item_code, this->describe_item(item));
  }

  phosg::fwrite_fmt(stream, "RANGED SPECIALS\n");
  phosg::fwrite_fmt(stream, "  ## => 11 12 WR A1\n");
  for (size_t z = 0; z < pmt->num_ranged_specials(); z++) {
    const auto& rs = pmt->get_ranged_special(z);
    phosg::fwrite_fmt(stream, "  {:02X} => {:02X} {:02X} {:02X} {:02X}\n",
        z, rs.data1_1, rs.data1_2, rs.weapon_range_index, rs.unknown_a1);
  }
}

std::string ItemNameIndex::generate_item_parameter_table_html() const {
  std::deque<std::string> blocks;
  auto pmt = this->item_parameter_table;

  blocks.emplace_back("\
<html><head><title>PSO item parameter table</title><style>\n\
body {\n\
  background-color: #222222;\n\
  color: #EEEEEE;\n\
  font-family: monospace;\n\
}\n\
td {\n\
  background-color: #333333;\n\
  color: #EEEEEE;\n\
  padding: 4px;\n\
}\n\
th {\n\
  background-color: #444444;\n\
  color: #EEEEEE;\n\
  font-weight: bold;\n\
  padding: 4px;\n\
}\n\
.right {\n\
  text-align: right;\n\
}\n\
</style></head><body>");

  auto format_usability_flags = [](uint8_t flags) -> std::string {
    switch (flags) {
      case 0x00:
        return "None";
      case 0x49:
        return "HUmar only";
      case 0xA1:
        return "HUnewearl only";
      case 0x51:
        return "HUcast only";
      case 0x91:
        return "HUcaseal only";
      case 0x4A:
        return "RAmar only";
      case 0x8A:
        return "RAmarl only";
      case 0x52:
        return "RAcast only";
      case 0x92:
        return "RAcaseal only";
      case 0x4C:
        return "FOmar only";
      case 0x8C:
        return "FOmarl only";
      case 0x64:
        return "FOnewm only";
      case 0xA4:
        return "FOnewearl only";
      case 0xFF:
        return "All";
      default:
        std::vector<std::string> tokens;
        switch (flags & 0xC0) {
          case 0x00:
            tokens.emplace_back("(No gender)");
            break;
          case 0x40:
            tokens.emplace_back("Male");
            break;
          case 0x80:
            tokens.emplace_back("Female");
            break;
        }
        switch (flags & 0x38) {
          case 0x00:
            tokens.emplace_back("(No race)");
            break;
          case 0x08:
            tokens.emplace_back("Human");
            break;
          case 0x10:
            tokens.emplace_back("Android");
            break;
          case 0x20:
            tokens.emplace_back("Newman");
            break;
          case 0x18:
            tokens.emplace_back("non-Newman");
            break;
          case 0x28:
            tokens.emplace_back("non-Android");
            break;
          case 0x30:
            tokens.emplace_back("non-Human");
            break;
        }
        switch (flags & 0x07) {
          case 0x00:
            tokens.emplace_back("(No class)");
            break;
          case 0x01:
            tokens.emplace_back("Hunter");
            break;
          case 0x02:
            tokens.emplace_back("Ranger");
            break;
          case 0x04:
            tokens.emplace_back("Force");
            break;
          case 0x03:
            tokens.emplace_back("non-Force");
            break;
          case 0x05:
            tokens.emplace_back("non-Ranger");
            break;
          case 0x06:
            tokens.emplace_back("non-Hunter");
            break;
        }
        return phosg::join(tokens, ", ");
    }
  };

  auto format_stat_boost = [&pmt](size_t entry_index) -> std::string {
    std::vector<std::string> tokens;
    if (entry_index < pmt->num_stat_boosts()) {
      const auto& stat_boost = pmt->get_stat_boost(entry_index);
      std::array<const char*, 0x11> stat_names{
          "(Nothing)", "ATP+", "ATA+", "EVP+", "DFP+", "MST+", "HP+", "LCK+", "ALL+",
          "ATP-", "ATA-", "EVP-", "DFP-", "MST-", "HP-", "LCK-", "ALL-"};
      if (stat_boost.stat1 && stat_boost.stat1 < 0x11) {
        tokens.emplace_back(std::format("{}{}", stat_names[stat_boost.stat1], stat_boost.amount1));
      }
      if (stat_boost.stat2 && stat_boost.stat2 < 0x11) {
        tokens.emplace_back(std::format("{}{}", stat_names[stat_boost.stat2], stat_boost.amount2));
      }
    }
    return phosg::join(tokens, ", ");
  };

  auto format_tech_boost = [&pmt](size_t entry_index) -> std::string {
    std::vector<std::string> tokens;
    if (entry_index < pmt->num_tech_boosts()) {
      const auto& tech_boost = pmt->get_tech_boost(entry_index);
      auto render_boost = [&tokens](uint8_t tech_num, uint8_t flags, float amount) -> void {
        if (flags & 1) {
          tokens.emplace_back(std::format("{} pierce", name_for_technique(tech_num)));
        } else if (flags) {
          tokens.emplace_back(std::format("{} FLAGS:{:02X}", name_for_technique(tech_num), flags));
        }
        if (amount > 0) {
          tokens.emplace_back(std::format("{} +{:g}%", name_for_technique(tech_num), amount * 100));
        } else if (amount < 0) {
          tokens.emplace_back(std::format("{} -{:g}%", name_for_technique(tech_num), -amount * 100));
        }
      };
      if (tech_boost.tech_num1 != 0xFF) {
        render_boost(tech_boost.tech_num1, tech_boost.flags1, tech_boost.amount1);
      }
      if (tech_boost.tech_num2 != 0xFF) {
        render_boost(tech_boost.tech_num2, tech_boost.flags2, tech_boost.amount2);
      }
      if (tech_boost.tech_num3 != 0xFF) {
        render_boost(tech_boost.tech_num3, tech_boost.flags3, tech_boost.amount3);
      }
    }
    return phosg::join(tokens, ", ");
  };

  auto format_stars = [](uint8_t stars) -> std::string {
    std::string ret = "<span style=\"color: #3253e6\">";
    for (size_t z = 0; z < std::min<uint8_t>(stars, 9); z++) {
      ret += "&starf;";
    }
    ret += "</span>";
    if (stars > 9) {
      ret += "<span style=\"color: #b4ad29\">";
      for (size_t z = 9; z < stars; z++) {
        ret += "&starf;";
      }
      ret += "</span>";
    }
    return ret;
  };

  auto format_name_cell = [this, &pmt](const ItemData& item) -> std::string {
    std::string name = this->describe_item(item, (item.data1[0] == 0) ? 0 : Flag::NAME_ONLY);
    if (item.is_s_rank_weapon()) {
      return std::format("<td style=\"color: #fe3c28\">{}</td>", name);
    } else if (pmt->is_item_rare(item)) {
      return std::format("<td style=\"color: #ffea00\">{}</td>", name);
    } else {
      return std::format("<td>{}</td>", name);
    }
  };

  blocks.emplace_back(std::format("<table><tr><th colspan=\"{}\">WEAPONS</th></tr>", 13 + is_v4(this->limits->version)));
  blocks.emplace_back("<tr><th>Hex</th>");
  if (is_v4(this->limits->version)) {
    blocks.emplace_back("<th>Team pts</th>");
  }
  blocks.emplace_back("<th>Usability</th><th>Requirements</th><th>ATP</th><th>ATA</th><th>MST</th><th>Max grind</th><th>Special</th><th>Stat boosts</th><th>Tech boosts</th><th>Weapon class</th><th>Stars</th><th>Name</th></tr>");
  for (size_t data1_1 = 0; data1_1 < pmt->num_weapon_classes(); data1_1++) {
    uint8_t weapon_class = pmt->get_weapon_kind(data1_1);
    size_t data1_2_limit = pmt->num_weapons_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& w = pmt->get_weapon(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(w.id);

      ItemData item;
      item.data1[0] = 0x00;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;

      std::vector<std::string> requirements_tokens;
      if (w.atp_required) {
        requirements_tokens.emplace_back(std::format("{} ATP", w.atp_required));
      }
      if (w.ata_required) {
        requirements_tokens.emplace_back(std::format("{} ATA", w.ata_required));
      }
      if (w.mst_required) {
        requirements_tokens.emplace_back(std::format("{} MST", w.mst_required));
      }

      static constexpr std::array<const char*, 19> weapon_class_names{
          "fist", "saber", "sword", "dagger", "partisan", "slicer", "handgun", "rifle", "mechgun", "shot", "cane",
          "rod", "wand", "claw", "double saber", "twin swords", "katana", "launcher", "cards"};
      std::string weapon_class_str = (weapon_class < weapon_class_names.size())
          ? weapon_class_names[weapon_class]
          : std::format("{:02X}", weapon_class);

      blocks.emplace_back("<tr>");
      blocks.emplace_back(std::format("<td>00{:02X}{:02X}</td>", data1_1, data1_2));
      if (is_v4(this->limits->version)) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", w.team_points));
      }
      blocks.emplace_back(std::format("<td>{}</td>", format_usability_flags(w.usability_flags)));
      blocks.emplace_back(std::format("<td>{}</td>", phosg::join(requirements_tokens, ", ")));
      if (w.atp_min == 0 && w.atp_max == 0) {
        blocks.emplace_back("<td></td>");
      } else if (w.atp_min == w.atp_max) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", w.atp_min));
      } else {
        blocks.emplace_back(std::format("<td class=\"right\">{}-{}</td>", w.atp_min, w.atp_max));
      }
      if (w.ata == 0) {
        blocks.emplace_back("<td></td>");
      } else {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", w.ata));
      }
      if (w.mst == 0) {
        blocks.emplace_back("<td></td>");
      } else {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", w.mst));
      }
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", w.max_grind));
      if (w.special) {
        blocks.emplace_back(std::format("<td>{}</td>", name_for_weapon_special.at(w.special)));
      } else {
        blocks.emplace_back("<td></td>");
      }
      blocks.emplace_back(std::format("<td>{}</td>", format_stat_boost(w.stat_boost_entry_index)));
      blocks.emplace_back(std::format("<td>{}</td>", format_tech_boost(w.tech_boost_entry_index)));
      blocks.emplace_back(std::format("<td>{}</td>", weapon_class_str));
      blocks.emplace_back(std::format("<td>{}</td>", format_stars(stars)));
      blocks.emplace_back(format_name_cell(item));
      blocks.emplace_back("</tr>");
    }
  }
  blocks.emplace_back("</table>");

  blocks.emplace_back(std::format("<table><tr><th colspan=\"{}\">ARMORS</th></tr>", 14 + is_v4(this->limits->version)));
  blocks.emplace_back("<tr><th>Hex</th>");
  if (is_v4(this->limits->version)) {
    blocks.emplace_back("<th>Team pts</th>");
  }
  blocks.emplace_back("<th>Usability</th><th>Req. level</th><th>DFP</th><th>EVP</th><th>EFR</th><th>ETH</th><th>EIC</th><th>EDK</th><th>ELT</th><th>Stat boosts</th><th>Tech boosts</th><th>Stars</th><th>Name</th></tr>");
  for (size_t data1_1 = 1; data1_1 < 3; data1_1++) {
    size_t data1_2_limit = pmt->num_armors_or_shields_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& a = pmt->get_armor_or_shield(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(a.id);

      ItemData item;
      item.data1[0] = 0x01;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;

      blocks.emplace_back("<tr>");
      blocks.emplace_back(std::format("<td>01{:02X}{:02X}</td>", data1_1, data1_2));
      if (is_v4(this->limits->version)) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", a.team_points));
      }
      blocks.emplace_back(std::format("<td>{}</td>", format_usability_flags(a.usability_flags)));
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", a.required_level + 1));
      if (a.dfp_range == 0 && a.dfp == 0) {
        blocks.emplace_back("<td></td>");
      } else if (a.dfp_range == 0) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", a.dfp));
      } else {
        blocks.emplace_back(std::format("<td class=\"right\">{}-{}</td>", a.dfp, a.dfp + a.dfp_range));
      }
      if (a.evp_range == 0 && a.evp == 0) {
        blocks.emplace_back("<td></td>");
      } else if (a.evp_range == 0) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", a.evp));
      } else {
        blocks.emplace_back(std::format("<td class=\"right\">{}-{}</td>", a.evp, a.evp + a.evp_range));
      }
      blocks.emplace_back(a.efr ? std::format("<td class=\"right\">{}</td>", a.efr) : "<td></td>");
      blocks.emplace_back(a.eth ? std::format("<td class=\"right\">{}</td>", a.eth) : "<td></td>");
      blocks.emplace_back(a.eic ? std::format("<td class=\"right\">{}</td>", a.eic) : "<td></td>");
      blocks.emplace_back(a.edk ? std::format("<td class=\"right\">{}</td>", a.edk) : "<td></td>");
      blocks.emplace_back(a.elt ? std::format("<td class=\"right\">{}</td>", a.elt) : "<td></td>");
      blocks.emplace_back(std::format("<td>{}</td>", format_stat_boost(a.stat_boost_entry_index)));
      blocks.emplace_back(std::format("<td>{}</td>", format_tech_boost(a.tech_boost_entry_index)));
      blocks.emplace_back(std::format("<td>{}</td>", format_stars(stars)));
      blocks.emplace_back(format_name_cell(item));
      blocks.emplace_back("</tr>");
    }
  }
  blocks.emplace_back("</table>");

  blocks.emplace_back(std::format("<table><tr><th colspan=\"{}\">UNITS</th></tr>", 4 + is_v4(this->limits->version)));
  blocks.emplace_back("<tr><th>Hex</th>");
  if (is_v4(this->limits->version)) {
    blocks.emplace_back("<th>Team pts</th>");
  }
  blocks.emplace_back("<th>Stats</th><th>Stars</th><th>Name</th></tr>");
  for (size_t data1_2 = 0; data1_2 < pmt->num_units(); data1_2++) {
    const auto& u = pmt->get_unit(data1_2);
    uint8_t stars = pmt->get_item_stars(u.id);

    ItemData item;
    item.data1[0] = 0x01;
    item.data1[1] = 0x03;
    item.data1[2] = data1_2;

    std::string stat_str;
    switch (u.stat) {
      case 0x00:
        stat_str = std::format("ATP +{}", u.amount);
        break;
      case 0x01:
        stat_str = std::format("MST +{}", u.amount);
        break;
      case 0x02:
        stat_str = std::format("ATA +{:g}", static_cast<float>(u.amount) / 10);
        break;
      case 0x03:
        stat_str = std::format("EVP +{}", u.amount);
        break;
      case 0x04:
        stat_str = std::format("HP +{}", u.amount);
        break;
      case 0x05:
        stat_str = std::format("TP +{}", u.amount);
        break;
      case 0x06:
        stat_str = std::format("DFP +{}", u.amount);
        break;
      case 0x07:
        stat_str = std::format("LCK +{}", u.amount);
        break;
      case 0x08:
        stat_str = std::format("ATP/MST/EVP/DFP/LCK +{}, ATA +{:g}", u.amount, static_cast<float>(u.amount) / 10);
        break;
      case 0x09:
        stat_str = std::format("EFR +{}", u.amount);
        break;
      case 0x0A:
        stat_str = std::format("EIC +{}", u.amount);
        break;
      case 0x0B:
        stat_str = std::format("ETH +{}", u.amount);
        break;
      case 0x0C:
        stat_str = std::format("ELT +{}", u.amount);
        break;
      case 0x0D:
        stat_str = std::format("EDK +{}", u.amount);
        break;
      case 0x0E:
        stat_str = std::format("All resists +{}", u.amount);
        break;
      case 0x0F:
        stat_str = std::format("HP generation +1 / {}sec", u.amount);
        break;
      case 0x10:
        stat_str = std::format("TP generation +1 / {}sec", u.amount);
        break;
      case 0x11:
        stat_str = std::format("PB generation +1 / {}sec", u.amount);
        break;
      case 0x12:
        stat_str = std::format("Tech levels +{}", u.amount);
        break;
      case 0x13:
        stat_str = std::format("Attack speed +{}%", u.amount);
        break;
      case 0x14:
        stat_str = "Immunity to negative status effects";
        break;
      case 0x15:
        stat_str = "Permanent trap vision";
        break;
      case 0x16:
        stat_str = "Immunity to poison";
        break;
      case 0x17:
        stat_str = "Immunity to paralysis";
        break;
      case 0x18:
        stat_str = "Immunity to shock";
        break;
      case 0x19:
        stat_str = "Immunity to slow";
        break;
      case 0x1A:
        stat_str = "Immunity to confusion";
        break;
      case 0x1B:
        stat_str = "Immunity to freezing";
        break;
      default:
        stat_str = std::format("{:02X} +{}", u.stat, u.amount);
    }

    blocks.emplace_back("<tr>");
    blocks.emplace_back(std::format("<td>0103{:02X}</td>", data1_2));
    if (is_v4(this->limits->version)) {
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", u.team_points));
    }
    blocks.emplace_back(std::format("<td>{}</td>", stat_str));
    blocks.emplace_back(std::format("<td>{}</td>", format_stars(stars)));
    blocks.emplace_back(format_name_cell(item));
    blocks.emplace_back("</tr>");
  }
  blocks.emplace_back("</table>");

  blocks.emplace_back("<table><tr><th colspan=\"9\">MAGS</th></tr>");
  blocks.emplace_back("<tr><th>Hex</th><th>Usability</th><th>Feed table</th><th>PB</th><th>On PB full</th><th>On low HP</th><th>On death</th><th>On boss arena</th><th>Name</th></tr>");
  for (size_t data1_1 = 0; data1_1 < pmt->num_mags(); data1_1++) {
    const auto& m = pmt->get_mag(data1_1);

    ItemData item;
    item.data1[0] = 0x02;
    item.data1[1] = data1_1;
    item.data1[2] = 0x00;

    static constexpr std::array<const char*, 6> pb_names{
        "Farlla", "Estlla", "Golla", "Pilla", "Leilla", "Mylla & Youlla"};

    auto format_mag_effect = [](uint8_t effect, uint8_t flag, uint8_t activation) -> std::string {
      std::string activation_str;
      switch (flag) {
        case 0:
          activation_str = std::format("{}%", activation);
          break;
        case 1:
          activation_str = std::format("({}+SS)%", activation);
          break;
        case 2:
          activation_str = "SS%";
          break;
        case 3:
          activation_str = std::format("{}%", activation - 10);
          break;
        case 4:
          activation_str = "(SS-10)%";
          break;
        default:
          activation_str = std::format("!{:02X} {}%", flag, activation - 10);
      }
      switch (effect) {
        case 0x00:
          return "";
        case 0x04:
          return std::format("Shifta+Deband {}", activation_str);
        case 0x06:
          return std::format("Resta Lv.((IQ/40)+1) {}", activation_str);
        case 0x07:
          return std::format("Reverser {}", activation_str);
        case 0x08:
          return std::format("Invincibility 30sec {}", activation_str);
        default:
          return std::format("!{:02X} {}", effect, activation_str);
      }
    };

    blocks.emplace_back("<tr>");
    blocks.emplace_back(std::format("<td>02{:02X}</td>", data1_1));
    blocks.emplace_back(std::format("<td>{}</td>", format_usability_flags(m.usability_flags)));
    blocks.emplace_back(std::format("<td>Table {}</td>", m.feed_table, m.feed_table));
    blocks.emplace_back(std::format("<td>{}</td>", (m.photon_blast == 0xFF) ? "" : pb_names.at(m.photon_blast)));
    blocks.emplace_back(std::format("<td>{}</td>", format_mag_effect(m.on_pb_full, m.on_pb_full_flag, m.activation)));
    blocks.emplace_back(std::format("<td>{}</td>", format_mag_effect(m.on_low_hp, m.on_low_hp_flag, m.activation)));
    blocks.emplace_back(std::format("<td>{}</td>", format_mag_effect(m.on_death, m.on_death_flag, m.activation)));
    blocks.emplace_back(std::format("<td>{}</td>", format_mag_effect(m.on_boss, m.on_boss_flag, m.activation)));
    blocks.emplace_back(format_name_cell(item));
    blocks.emplace_back("</tr>");
  }
  blocks.emplace_back("</table>");

  blocks.emplace_back(std::format("<table><tr><th colspan=\"{}\">TOOLS</th></tr>", 5 + (!is_v1(this->limits->version)) + is_v4(this->limits->version)));
  blocks.emplace_back("<tr><th>Hex</th>");
  if (is_v4(this->limits->version)) {
    blocks.emplace_back("<th>Team pts</th>");
  }
  blocks.emplace_back("<th>Amount</th><th>Tech</th><th>Cost</th>");
  if (!is_v1(this->limits->version)) {
    blocks.emplace_back("<th>Flags</th>");
  }
  blocks.emplace_back("<th>Name</th></tr>");
  for (size_t data1_1 = 0; data1_1 < pmt->num_tool_classes(); data1_1++) {
    size_t data1_2_limit = pmt->num_tools_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& t = pmt->get_tool(data1_1, data1_2);

      ItemData item;
      item.data1[0] = 0x03;
      item.data1[1] = data1_1;
      item.data1[(data1_1 == 0x02) ? 4 : 2] = data1_2;
      item.set_tool_item_amount(*this->limits, 1);

      std::vector<std::string> flags_tokens;
      if (!is_v1(this->limits->version)) {
        if (!(t.item_flags & 0x01)) {
          flags_tokens.emplace_back("unusable");
        }
        if (t.item_flags & 0x04) {
          flags_tokens.emplace_back("feedable");
        }
        if (!(t.item_flags & 0x08)) {
          flags_tokens.emplace_back("unusable by Androids");
        }
        if (!(t.item_flags & 0x10)) {
          flags_tokens.emplace_back("unusable in city");
        }
        if (!(t.item_flags & 0x20)) {
          flags_tokens.emplace_back("unusable in boss arenas");
        }
        if (!(t.item_flags & 0x40)) {
          flags_tokens.emplace_back("unusable in Challenge");
        }
        if (t.item_flags & 0x80) {
          flags_tokens.emplace_back("rare");
        }
      }

      blocks.emplace_back("<tr>");
      blocks.emplace_back(std::format("<td>03{:02X}{:02X}</td>", data1_1, data1_2));
      if (is_v4(this->limits->version)) {
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", t.team_points));
      }
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", t.amount));
      if (data1_1 == 2) {
        blocks.emplace_back(std::format("<td>{}</td>", name_for_technique(t.tech)));
      } else {
        blocks.emplace_back("<td></td>");
      }
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", t.cost));
      if (!is_v1(this->limits->version)) {
        if (flags_tokens.empty()) {
          blocks.emplace_back(std::format("<td>{:02X}</td>", t.item_flags));
        } else {
          blocks.emplace_back(std::format("<td>{:02X}: {}</td>", t.item_flags, phosg::join(flags_tokens, ", ")));
        }
      }
      blocks.emplace_back(format_name_cell(item));
      blocks.emplace_back("</tr>");
    }
  }
  blocks.emplace_back("</table>");

  blocks.emplace_back("<table><tr><th colspan=\"49\">MAG FEED TABLES</th></tr>");
  blocks.emplace_back("<tr><th>Item</th><th colspan=\"6\">Table 0</th><th colspan=\"6\">Table 1</th><th colspan=\"6\">Table 2</th><th colspan=\"6\">Table 3</th><th colspan=\"6\">Table 4</th><th colspan=\"6\">Table 5</th><th colspan=\"6\">Table 6</th><th colspan=\"6\">Table 7</th></tr>");
  blocks.emplace_back("<tr><th></th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th><th>DEF</th><th>POW</th><th>DEX</th><th>MIND</th><th>Sync</th><th>IQ</th></tr>");
  for (size_t which = 0; which < 11; which++) {
    static constexpr const char* names[11] = {
        "Monomate", "Dimate", "Trimate", "Monofluid", "Difluid", "Trifluid", "Antidote", "Antiparalysis",
        "Sol Atomizer", "Moon Atomizer", "Star Atomizer"};
    blocks.emplace_back("<tr>");
    blocks.emplace_back(std::format("<th>{}</th>", names[which]));
    for (size_t table_index = 0; table_index < 8; table_index++) {
      const auto& res = pmt->get_mag_feed_result(table_index, which);
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", res.def));
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", res.pow));
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", res.dex));
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", res.mind));
      blocks.emplace_back(std::format("<td class=\"right\">{}%</td>", res.synchro));
      blocks.emplace_back(std::format("<td class=\"right\">{}</td>", res.iq));
    }
    blocks.emplace_back("</tr>");
  }
  blocks.emplace_back("</table>");

  if (!pmt->item_combinations_index().empty()) {
    blocks.emplace_back("<table><tr><th colspan=\"10\">ITEM COMBINATIONS</th></tr>");
    blocks.emplace_back("<tr><th colspan=\"2\">Used item</th><th colspan=\"2\">Equipped item</th><th>Req. mag level</th><th>Req. grind</th><th>Req. player level</th><th>Req. class</th><th colspan=\"2\">Result item</th></tr>");
    for (const auto& [_, used_item_index] : pmt->item_combinations_index()) {
      for (const auto& [_, combo] : used_item_index) {
        ItemData used_item, equipped_item, result_item;
        for (size_t z = 0; z < 3; z++) {
          used_item.data1[z] = combo->used_item[z];
          equipped_item.data1[z] = combo->equipped_item[z];
          result_item.data1[z] = combo->result_item[z];
        }
        blocks.emplace_back("<tr>");
        blocks.emplace_back(std::format("<td>{:02X}{:02X}{:02X}</td>", used_item.data1[0], used_item.data1[1], used_item.data1[2]));
        blocks.emplace_back(format_name_cell(used_item));
        blocks.emplace_back(std::format("<td>{:02X}{:02X}{:02X}</td>", equipped_item.data1[0], equipped_item.data1[1], equipped_item.data1[2]));
        blocks.emplace_back(format_name_cell(equipped_item));
        blocks.emplace_back((combo->mag_level != 0xFF)
                ? std::format("<td class=\"right\">{}</td>", combo->mag_level)
                : "<td></td>");
        blocks.emplace_back((combo->grind != 0xFF)
                ? std::format("<td class=\"right\">{}</td>", combo->grind)
                : "<td></td>");
        blocks.emplace_back((combo->level != 0xFF)
                ? std::format("<td class=\"right\">{}</td>", combo->level)
                : "<td></td>");
        blocks.emplace_back((combo->char_class != 0xFF)
                ? std::format("<td class=\"right\">{}</td>", name_for_char_class(combo->char_class))
                : "<td></td>");
        blocks.emplace_back(std::format("<td>{:02X}{:02X}{:02X}</td>", result_item.data1[0], result_item.data1[1], result_item.data1[2]));
        blocks.emplace_back(format_name_cell(result_item));
        blocks.emplace_back("</tr>");
      }
    }
    blocks.emplace_back("</table>");
  }

  if (pmt->num_events() > 0) {
    blocks.emplace_back("<table><tr><th colspan=\"3\">EVENT ITEMS</th></tr>");
    for (size_t event_number = 0; event_number < pmt->num_events(); event_number++) {
      blocks.emplace_back(std::format("<tr><th colspan=\"3\">Event {}</th></tr>", event_number));
      blocks.emplace_back("<tr><th colspan=\"2\">Item</th><th>Chance</th></tr>");
      auto events_list = pmt->get_event_items(event_number);
      for (size_t z = 0; z < events_list.second; z++) {
        const auto& event_item = events_list.first[z];

        ItemData item;
        item.data1[0] = event_item.item[0];
        item.data1[1] = event_item.item[1];
        item.data1[2] = event_item.item[2];

        blocks.emplace_back("<tr>");
        blocks.emplace_back(std::format("<td>{:02X}{:02X}{:02X}</td>", item.data1[0], item.data1[1], item.data1[2]));
        blocks.emplace_back(format_name_cell(item));
        blocks.emplace_back(std::format("<td class=\"right\">{}</td>", event_item.probability));
        blocks.emplace_back("</tr>");
      }
    }
    blocks.emplace_back("</table>");
  }

  blocks.emplace_back("</body></html>");
  return phosg::join(blocks, "");
}
