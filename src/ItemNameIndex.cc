#include "ItemNameIndex.hh"

#include "StaticGameData.hh"

using namespace std;

ItemNameIndex::ItemNameIndex(
    std::shared_ptr<const ItemParameterTable> item_parameter_table,
    std::shared_ptr<const ItemData::StackLimits> limits,
    const std::vector<std::string>& name_coll)
    : item_parameter_table(item_parameter_table),
      limits(limits) {

  for (uint32_t primary_identifier : item_parameter_table->compute_all_valid_primary_identifiers()) {
    // 00000000 is a valid primary identifier but not a valid weapon; skip it
    if (primary_identifier == 0x00000000) {
      continue;
    }

    const string* name = nullptr;
    bool is_es_weapon = false;
    try {
      ItemData item = ItemData::from_primary_identifier(*this->limits, primary_identifier);
      is_es_weapon = item.is_s_rank_weapon();
      name = &name_coll.at(item_parameter_table->get_item_id(item));
    } catch (const out_of_range&) {
    }

    if (name) {
      auto meta = make_shared<ItemMetadata>();
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

std::string ItemNameIndex::describe_item(const ItemData& item, uint8_t flags) const {
  bool include_color_escapes = flags & ItemNameIndex::Flag::INCLUDE_PSO_COLOR_ESCAPES;
  bool name_only = flags & ItemNameIndex::Flag::NAME_ONLY;

  if (item.data1[0] == 0x04) {
    return std::format("{}{} Meseta", include_color_escapes ? "$C7" : "", item.data2d);
  }

  vector<string> ret_tokens;

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
      } catch (const out_of_range&) {
        ret_tokens.emplace_back(std::format("!SP:{:02X}", special_id));
      }
    }
  }
  if (!name_only && (item.data1[0] == 0x00) && (item.data1[2] != 0x00) && item.is_s_rank_weapon()) {
    try {
      ret_tokens.emplace_back(name_for_s_rank_special.at(item.data1[2]));
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(std::format("!SSP:{:02X}", item.data1[2]));
    }
  }

  // Armors, shields, and units (0x01) can be wrapped, as can mags (0x02) and
  // non-stackable tools (0x03). However, each of these item classes has its
  // flags in a different location.
  if (!name_only &&
      (((item.data1[0] == 0x01) && (item.data1[4] & 0x40)) ||
          ((item.data1[0] == 0x02) && (item.data2[2] & 0x40)) ||
          ((item.data1[0] == 0x03) && !item.is_stackable(*this->limits) && (item.data1[3] & 0x40)))) {
    ret_tokens.emplace_back("Wrapped");
  }

  // Add the item name
  uint32_t primary_identifier = item.primary_identifier();
  if ((primary_identifier & 0xFFFF0000) == 0x03020000) {
    string technique_name;
    try {
      technique_name = tech_id_to_name.at(item.data1[4]);
      technique_name[0] = toupper(technique_name[0]);
    } catch (const out_of_range&) {
      technique_name = std::format("!TD:{:02X}", item.data1[4]);
    }
    // Hide the level for Reverser and Ryuker, unless the level isn't 1
    if ((item.data1[2] == 0) && ((item.data1[4] == 0x0E) || (item.data1[4] == 0x11))) {
      ret_tokens.emplace_back(std::format("Disk:{}", technique_name));
    } else {
      ret_tokens.emplace_back(std::format("Disk:{} Lv.{}", technique_name, item.data1[2] + 1));
    }
  } else {
    try {
      auto meta = this->primary_identifier_index.at(primary_identifier);
      ret_tokens.emplace_back(meta->name);
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(std::format("!ID:{:08X}", primary_identifier));
    }
  }

  if (item.data1[0] == 0x00) {
    // For weapons, add the grind and bonuses, or S-rank name if applicable
    if (!name_only && item.data1[3] > 0) {
      ret_tokens.emplace_back(std::format("+{}", item.data1[3]));
    }

    if (item.is_s_rank_weapon()) {
      // S-rank weapons have names instead of percent bonuses. The name is
      // encoded as 9 5-bit characters in the bytes where the bonuses usually
      // go. The first character does not appear when the name is rendered;
      // instead, it's used to determine if the weapon type name should appear
      // or not. Unlike the client, we check the first character directly and
      // don't bother decoding it.
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

      string name;
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
    auto format_stat = +[](uint16_t stat) -> string {
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
      ret_tokens.emplace_back(std::format("({})", name_for_mag_color.at(item.data2[3])));
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(std::format("(!CL:{:02X})", item.data2[3]));
    }

  } else if (item.data1[0] == 0x03) {
    // For tools, add the amount (if applicable)
    if (item.max_stack_size(*this->limits) > 1) {
      ret_tokens.emplace_back(std::format("x{}", item.data1[5]));
    }
  }

  string ret = phosg::join(ret_tokens, " ");
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
  } catch (const exception& e1) {
    try {
      ret = this->parse_item_description_phase(desc, true);
    } catch (const exception& e2) {
      try {
        ret = ItemData::from_data(phosg::parse_data_string(desc));
      } catch (const exception& ed) {
        if (strcmp(e1.what(), e2.what())) {
          throw runtime_error(std::format("cannot parse item description \"{}\" (as text 1: {}) (as text 2: {}) (as data: {})",
              desc, e1.what(), e2.what(), ed.what()));
        } else {
          throw runtime_error(std::format("cannot parse item description \"{}\" (as text: {}) (as data: {})",
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

  string desc = phosg::tolower(description);
  if (desc.ends_with(" meseta")) {
    ret.data1[0] = 0x04;
    ret.data2d = stol(desc, nullptr, 10);
    return ret;
  }

  if (desc.starts_with("es ")) {
    auto parse_name = [&](const std::string& token) -> void {
      if (token.size() > 8) {
        throw runtime_error("s-rank name too long");
      }

      uint8_t char_indexes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
      for (size_t z = 0; z < token.size(); z++) {
        char ch = toupper(token[z]);
        size_t pos = s_rank_name_characters.find(ch);
        if (pos == std::string::npos) {
          throw runtime_error(std::format("s-rank name contains invalid character {:02X} ({})", ch, ch));
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
        throw runtime_error("invalid tech disk format");
      }
      if (!tokens[1].starts_with("lv.")) {
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
      string prefix = phosg::tolower(name_for_weapon_special[z]);
      prefix += ' ';
      if (desc.starts_with(prefix)) {
        weapon_special = z;
        desc = desc.substr(prefix.size());
        break;
      }
    }
  }

  auto name_it = this->name_index.lower_bound(desc);
  // Look up to 3 places before the lower bound. We have to do this to catch
  // cases like Sange vs. Sange & Yasha - if the input is like "Sange 0/...",
  // then we'll see Sange & Yasha first, which we should skip.
  size_t lookback = 0;
  while (lookback < 4) {
    if (name_it != this->name_index.end() && desc.starts_with(name_it->first)) {
      break;
    } else if (name_it == this->name_index.begin()) {
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
  if (desc.starts_with(" ")) {
    desc = desc.substr(1);
  }

  // Tech disks should have already been handled above, so we don't need to
  // special-case 0302xxxx identifiers here.
  uint32_t primary_identifier = name_it->second->primary_identifier;
  ret.data1[0] = (primary_identifier >> 24) & 0xFF;
  ret.data1[1] = (primary_identifier >> 16) & 0xFF;
  ret.data1[2] = (primary_identifier >> 8) & 0xFF;

  if (ret.data1[0] == 0x00) {
    // Weapons: add special, grind and percentages (or name, if S-rank) and
    // kill count if unsealable
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
          throw runtime_error("invalid bonuses token");
        }
        uint8_t max_bonuses = this->item_parameter_table->is_unsealable_item(ret) ? 2 : 3;
        uint8_t bonus_index = 0;
        for (size_t z = 0; z < p_tokens.size(); z++) {
          int8_t bonus_value = stol(p_tokens[z], nullptr, 10);
          if (bonus_value == 0) {
            continue;
          }
          if (bonus_index >= max_bonuses) {
            throw runtime_error("weapon has too many bonuses");
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
      static const unordered_map<string, uint16_t> modifiers({
          {"--", 0xFFFC},
          {"-", 0xFFFE},
          {"", 0x0000},
          {"+", 0x0002},
          {"++", 0x0004},
      });
      ret.data1w[3] = modifiers.at(desc);

      bool kill_count_set = false;
      for (auto& token : phosg::split(desc, ' ')) {
        if (token.starts_with("k:")) {
          ret.set_kill_count(stoul(token.substr(2), nullptr, 0));
          kill_count_set = true;
          break;
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
          throw runtime_error("invalid armor/shield modifier");
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
      } else if (token.ends_with("%")) { // Synchro
        ret.data2[0] = stoul(token.substr(0, token.size() - 1), nullptr, 10);
      } else if (token.ends_with("iq")) { // IQ
        ret.data2[1] = stoul(token.substr(0, token.size() - 2), nullptr, 10);
      } else if (!token.empty() && isdigit(token[0])) { // Stats
        auto s_tokens = phosg::split(token, '/');
        if (s_tokens.size() != 4) {
          throw runtime_error("incorrect stat count");
        }
        for (size_t z = 0; z < 4; z++) {
          auto n_tokens = phosg::split(s_tokens[z], '.');
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
    if (ret.max_stack_size(*this->limits) > 1) {
      if (desc.starts_with("x")) {
        ret.data1[5] = stoul(desc.substr(1), nullptr, 10);
      } else {
        ret.data1[5] = 1;
      }
    } else if (!desc.empty()) {
      throw runtime_error("item cannot be stacked");
    }

    if (is_wrapped) {
      if (ret.is_stackable(*this->limits)) {
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

void ItemNameIndex::print_table(FILE* stream) const {
  auto pmt = this->item_parameter_table;

  phosg::fwrite_fmt(stream, "WEAPONS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS FLAG ATPLO ATPHI ATPRQ MSTRQ ATARQ -MST- GND PH SP ATA SB(S1:AMT1,S2:AMT2) PJ 1X 1Y 2X 2Y CL --A1-- A4 A5 TB BF V1 ST* USL ---DIVISOR--- NAME\n");
  for (size_t data1_1 = 0; data1_1 < pmt->num_weapon_classes; data1_1++) {
    uint8_t v1_replacement = pmt->get_weapon_v1_replacement(data1_1);
    float sale_divisor = pmt->get_sale_divisor(0x00, data1_1);
    string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_weapons_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& w = pmt->get_weapon(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(w.base.id);
      bool is_unsealable = pmt->is_unsealable_item(0x00, data1_1, data1_2);

      ItemData item;
      item.data1[0] = 0x00;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;
      string name = this->describe_item(item);

      auto& stat_boost = pmt->get_stat_boost(w.stat_boost_entry_index);
      phosg::fwrite_fmt(stream, "  00{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:04X} {:5} {:5} {:5} {:5} {:5} {:5} {:3} {:02X} {:02X} {:3} {:02X}({:02X}:{:04X},{:02X}:{:04X}) {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}{:02X}{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:2}* {} {} {}\n",
          data1_1,
          data1_2,
          w.base.id,
          w.base.type,
          w.base.skin,
          w.base.team_points,
          w.class_flags,
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
          stat_boost.stats[0],
          stat_boost.amounts[0],
          stat_boost.stats[1],
          stat_boost.amounts[1],
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
          w.tech_boost,
          w.behavior_flags,
          v1_replacement,
          stars,
          is_unsealable ? "YES" : " no",
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "ARMORS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS -DFP- -EVP- BP BE FLAG LVL EFR ETH EIC EDK ELT DFR EVR SB(S1:AMT1,S2:AMT2) TB FT A4 ST* ---DIVISOR--- NAME\n");
  for (size_t data1_1 = 1; data1_1 < 3; data1_1++) {
    float sale_divisor = pmt->get_sale_divisor(0x01, data1_1);
    string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_armors_or_shields_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& a = pmt->get_armor_or_shield(data1_1, data1_2);
      uint8_t stars = pmt->get_item_stars(a.base.id);

      ItemData item;
      item.data1[0] = 0x01;
      item.data1[1] = data1_1;
      item.data1[2] = data1_2;
      string name = this->describe_item(item);

      auto& stat_boost = pmt->get_stat_boost(a.stat_boost_entry_index);
      phosg::fwrite_fmt(stream, "  01{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:5} {:5} {:02X} {:02X} {:04X} {:3} {:3} {:3} {:3} {:3} {:3} {:3} {:3} {:02X}({:02X}:{:04X},{:02X}:{:04X}) {:02X} {:02X} {:02X} {:2}* {} {}\n",
          data1_1,
          data1_2,
          a.base.id,
          a.base.type,
          a.base.skin,
          a.base.team_points,
          a.dfp,
          a.evp,
          a.block_particle,
          a.block_effect,
          a.class_flags,
          static_cast<uint8_t>(a.required_level + 1),
          a.efr,
          a.eth,
          a.eic,
          a.edk,
          a.elt,
          a.dfp_range,
          a.evp_range,
          a.stat_boost_entry_index,
          stat_boost.stats[0],
          stat_boost.amounts[0],
          stat_boost.stats[1],
          stat_boost.amounts[1],
          a.tech_boost,
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
    string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_units();
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& u = pmt->get_unit(data1_2);
      uint8_t stars = pmt->get_item_stars(u.base.id);

      ItemData item;
      item.data1[0] = 0x01;
      item.data1[1] = 0x03;
      item.data1[2] = data1_2;
      string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  0103{:02X} => {:08X} {:04X} {:04X} {:6} {:04X} {:5} {:6} {:2}* {} {}\n",
          data1_2,
          u.base.id,
          u.base.type,
          u.base.skin,
          u.base.team_points,
          u.stat,
          u.stat_amount,
          u.modifier_amount,
          stars,
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "MAGS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS FTBL PB AC E1 E2 E3 E4 C1 C2 C3 C4 FLAG ---DIVISOR--- NAME\n");
  {
    size_t data1_1_limit = pmt->num_mags();
    for (size_t data1_1 = 0; data1_1 < data1_1_limit; data1_1++) {
      const auto& m = pmt->get_mag(data1_1);

      float sale_divisor = pmt->get_sale_divisor(0x02, data1_1);
      string divisor_str = std::format("{:g}", sale_divisor);
      divisor_str.resize(13, ' ');

      ItemData item;
      item.data1[0] = 0x02;
      item.data1[1] = data1_1;
      item.data1[2] = 0x00;
      string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  02{:02X}00 => {:08X} {:04X} {:04X} {:6} {:04X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:04X} {} {}\n",
          data1_1,
          m.base.id,
          m.base.type,
          m.base.skin,
          m.base.team_points,
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
          m.class_flags,
          divisor_str,
          name);
    }
  }

  phosg::fwrite_fmt(stream, "TOOLS\n");
  phosg::fwrite_fmt(stream, "  CODE   => ---ID--- TYPE SKIN POINTS COUNT TECH -COST- ITEMFLAG ---DIVISOR--- NAME\n");
  for (size_t data1_1 = 0; data1_1 < pmt->num_tool_classes; data1_1++) {
    float sale_divisor = pmt->get_sale_divisor(0x03, data1_1);
    string divisor_str = std::format("{:g}", sale_divisor);
    divisor_str.resize(13, ' ');

    size_t data1_2_limit = pmt->num_tools_in_class(data1_1);
    for (size_t data1_2 = 0; data1_2 < data1_2_limit; data1_2++) {
      const auto& t = pmt->get_tool(data1_1, data1_2);

      ItemData item;
      item.data1[0] = 0x03;
      item.data1[1] = data1_1;
      item.data1[(data1_1 == 0x02) ? 4 : 2] = data1_2;
      item.set_tool_item_amount(*this->limits, 1);
      string name = this->describe_item(item);

      phosg::fwrite_fmt(stream, "  03{:02X}{:02X} => {:08X} {:04X} {:04X} {:6} {:5} {:04X} {:6} {:08X} {} {}\n",
          data1_1,
          data1_2,
          t.base.id,
          t.base.type,
          t.base.skin,
          t.base.team_points,
          t.amount,
          t.tech,
          t.cost,
          t.item_flags,
          divisor_str,
          name);
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
        "Monomate", "Dimate", "Trimate", "Monofluid",
        "Difluid", "Trifluid", "Antidote", "Antiparalysis",
        "Sol Atomizer", "Moon Atomizer", "Star Atomizer"};
    phosg::fwrite_fmt(stream, "  TABLE {:02X}         => -DEF -POW -DEX MIND -IQ- SYNC\n", table_index);
    for (size_t which = 0; which < 11; which++) {
      const auto& res = pmt->get_mag_feed_result(table_index, which);
      phosg::fwrite_fmt(stream, "    {:14} => {:4} {:4} {:4} {:4} {:4} {:4}\n",
          names[which], res.def, res.pow, res.dex, res.mind, res.iq, res.synchro);
    }
  }

  phosg::fwrite_fmt(stream, "SPECIAL DEFINITIONS\n");
  phosg::fwrite_fmt(stream, "  SPECIAL => TYPE COUNT ST* NAME\n");
  for (size_t index = 0; index < pmt->num_specials; index++) {
    const auto& sp = pmt->get_special(index);
    uint8_t stars = pmt->get_special_stars(index);
    const char* name = "";
    if (index) {
      try {
        name = name_for_weapon_special.at(index);
      } catch (const out_of_range&) {
      }
    }
    phosg::fwrite_fmt(stream, "       {:02X} => {:04X} {:5} {:2}* {}\n", index, sp.type, sp.amount, stars, name);
  }

  phosg::fwrite_fmt(stream, "ITEM COMBINATIONS\n");
  phosg::fwrite_fmt(stream, "  ---USE + -EQUIP => RESULT MLV GND LVL CLS\n");
  for (const auto& combo_list_it : pmt->get_all_item_combinations()) {
    for (const auto& combo : combo_list_it.second) {
      phosg::fwrite_fmt(stream, "  {:02X}{:02X}{:02X} + {:02X}{:02X}{:02X} => {:02X}{:02X}{:02X}",
          combo.used_item[0], combo.used_item[1], combo.used_item[2],
          combo.equipped_item[0], combo.equipped_item[1], combo.equipped_item[2],
          combo.result_item[0], combo.result_item[1], combo.result_item[2]);
      if (combo.mag_level != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo.mag_level);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo.grind != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo.grind);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo.level != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}", combo.level);
      } else {
        phosg::fwrite_fmt(stream, "    ");
      }
      if (combo.char_class != 0xFF) {
        phosg::fwrite_fmt(stream, " {:3}\n", combo.char_class);
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
}
