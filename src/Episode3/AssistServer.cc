#include "AssistServer.hh"

#include "Server.hh"

using namespace std;

namespace Episode3 {

const vector<uint16_t>& all_assist_card_ids(bool is_nte) {
  // Note: This order matches the order that the cards are defined in the original
  // code. This is relevant for consistency of results when choosing a random card
  // (for God Whim).
  static const vector<uint16_t> ALL_ASSIST_CARD_IDS_TRIAL = {
      0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD,
      0x00FE, 0x00FF, 0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106,
      0x0107, 0x0108, 0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x010F,
      0x0121, 0x0125, 0x0126, 0x0127, 0x0128, 0x0129, 0x012A, 0x012B, 0x012C,
      0x012D, 0x012E, 0x012F, 0x0130, 0x0131, 0x0132, 0x0133, 0x0134, 0x0135,
      0x0136, 0x0137, 0x0138, 0x0139, 0x013A, 0x013B, 0x013C, 0x013D, 0x013E,
      0x013F, 0x0140, 0x0141, 0x0142, 0x0143, 0x0144, 0x0145, 0x0146, 0x0148,
      0x014A, 0x014B, 0x014C, 0x014D, 0x014E, 0x023F, 0x0240, 0x0241, 0x0242};
  static const vector<uint16_t> ALL_ASSIST_CARD_IDS_FINAL = {
      0x0018, 0x0019, 0x001A, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA,
      0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF, 0x0100, 0x0101, 0x0102, 0x0103,
      0x0104, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x010A, 0x010B, 0x010C,
      0x010D, 0x010E, 0x010F, 0x0121, 0x0125, 0x0126, 0x0127, 0x0128, 0x0129,
      0x012A, 0x012B, 0x012C, 0x012D, 0x012E, 0x012F, 0x0130, 0x0131, 0x0132,
      0x0133, 0x0134, 0x0135, 0x0136, 0x0137, 0x0138, 0x0139, 0x013A, 0x013B,
      0x013C, 0x013D, 0x013E, 0x013F, 0x0140, 0x0141, 0x0142, 0x0143, 0x0144,
      0x0145, 0x0146, 0x0148, 0x014A, 0x014B, 0x014C, 0x014D, 0x014E, 0x023F,
      0x0240, 0x0241, 0x0242};
  return is_nte ? ALL_ASSIST_CARD_IDS_TRIAL : ALL_ASSIST_CARD_IDS_FINAL;
}

AssistEffect assist_effect_number_for_card_id(uint16_t card_id, bool is_nte) {
  static const unordered_map<uint16_t, AssistEffect> card_id_to_effect_final_only({
      {0x0018, /* 0x0049 */ AssistEffect::DICE_FEVER_PLUS},
      {0x0019, /* 0x004A */ AssistEffect::RICH_PLUS},
      {0x001A, /* 0x004B */ AssistEffect::CHARITY_PLUS},
  });
  static const unordered_map<uint16_t, AssistEffect> card_id_to_effect({
      {0x00F5, /* 0x0001 */ AssistEffect::DICE_HALF},
      {0x00F6, /* 0x0002 */ AssistEffect::DICE_PLUS_1},
      {0x00F7, /* 0x0003 */ AssistEffect::DICE_FEVER},
      {0x00F8, /* 0x0004 */ AssistEffect::CARD_RETURN},
      {0x00F9, /* 0x0005 */ AssistEffect::LAND_PRICE},
      {0x00FA, /* 0x0006 */ AssistEffect::POWERLESS_RAIN},
      {0x00FB, /* 0x0007 */ AssistEffect::BRAVE_WIND},
      {0x00FC, /* 0x0008 */ AssistEffect::SILENT_COLOSSEUM},
      {0x00FD, /* 0x0009 */ AssistEffect::RESISTANCE},
      {0x00FE, /* 0x000A */ AssistEffect::INDEPENDENT},
      {0x00FF, /* 0x000B */ AssistEffect::ASSISTLESS},
      {0x0100, /* 0x000C */ AssistEffect::ATK_DICE_2},
      {0x0101, /* 0x000D */ AssistEffect::DEFLATION},
      {0x0102, /* 0x000E */ AssistEffect::INFLATION},
      {0x0103, /* 0x000F */ AssistEffect::EXCHANGE},
      {0x0104, /* 0x0010 */ AssistEffect::INFLUENCE},
      {0x0105, /* 0x0011 */ AssistEffect::SKIP_SET},
      {0x0106, /* 0x0012 */ AssistEffect::SKIP_MOVE},
      {0x0121, /* 0x0013 */ AssistEffect::SKIP_ACT},
      {0x0137, /* 0x0014 */ AssistEffect::SKIP_DRAW},
      {0x0107, /* 0x0015 */ AssistEffect::FLY},
      {0x0108, /* 0x0016 */ AssistEffect::NECROMANCER},
      {0x0109, /* 0x0017 */ AssistEffect::PERMISSION},
      {0x010A, /* 0x0018 */ AssistEffect::SHUFFLE_ALL},
      {0x010B, /* 0x0019 */ AssistEffect::LEGACY},
      {0x010C, /* 0x001A */ AssistEffect::ASSIST_REVERSE},
      {0x010D, /* 0x001B */ AssistEffect::STAMINA},
      {0x010E, /* 0x001C */ AssistEffect::AP_ABSORPTION},
      {0x010F, /* 0x001D */ AssistEffect::HEAVY_FOG},
      {0x0125, /* 0x001E */ AssistEffect::TRASH_1},
      {0x0126, /* 0x001F */ AssistEffect::EMPTY_HAND},
      {0x0127, /* 0x0020 */ AssistEffect::HITMAN},
      {0x0128, /* 0x0021 */ AssistEffect::ASSIST_TRASH},
      {0x0129, /* 0x0022 */ AssistEffect::SHUFFLE_GROUP},
      {0x012A, /* 0x0023 */ AssistEffect::ASSIST_VANISH},
      {0x012B, /* 0x0024 */ AssistEffect::CHARITY},
      {0x012C, /* 0x0025 */ AssistEffect::INHERITANCE},
      {0x012D, /* 0x0026 */ AssistEffect::FIX},
      {0x012E, /* 0x0027 */ AssistEffect::MUSCULAR},
      {0x012F, /* 0x0028 */ AssistEffect::CHANGE_BODY},
      {0x0130, /* 0x0029 */ AssistEffect::GOD_WHIM},
      {0x0131, /* 0x002A */ AssistEffect::GOLD_RUSH},
      {0x0132, /* 0x002B */ AssistEffect::ASSIST_RETURN},
      {0x0133, /* 0x002C */ AssistEffect::REQUIEM},
      {0x0134, /* 0x002D */ AssistEffect::RANSOM},
      {0x0135, /* 0x002E */ AssistEffect::SIMPLE},
      {0x0136, /* 0x002F */ AssistEffect::SLOW_TIME},
      {0x023F, /* 0x0030 */ AssistEffect::QUICK_TIME},
      {0x0138, /* 0x0031 */ AssistEffect::TERRITORY},
      {0x0139, /* 0x0032 */ AssistEffect::OLD_TYPE},
      {0x013A, /* 0x0033 */ AssistEffect::FLATLAND},
      {0x013B, /* 0x0034 */ AssistEffect::IMMORTALITY},
      {0x013C, /* 0x0035 */ AssistEffect::SNAIL_PACE},
      {0x013D, /* 0x0036 */ AssistEffect::TECH_FIELD},
      {0x013E, /* 0x0037 */ AssistEffect::FOREST_RAIN},
      {0x013F, /* 0x0038 */ AssistEffect::CAVE_WIND},
      {0x0140, /* 0x0039 */ AssistEffect::MINE_BRIGHTNESS},
      {0x0141, /* 0x003A */ AssistEffect::RUIN_DARKNESS},
      {0x0142, /* 0x003B */ AssistEffect::SABER_DANCE},
      {0x0143, /* 0x003C */ AssistEffect::BULLET_STORM},
      {0x0144, /* 0x003D */ AssistEffect::CANE_PALACE},
      {0x0145, /* 0x003E */ AssistEffect::GIANT_GARDEN},
      {0x0146, /* 0x003F */ AssistEffect::MARCH_OF_THE_MEEK},
      {0x0148, /* 0x0040 */ AssistEffect::SUPPORT},
      {0x014A, /* 0x0041 */ AssistEffect::RICH},
      {0x014B, /* 0x0042 */ AssistEffect::REVERSE_CARD},
      {0x014C, /* 0x0043 */ AssistEffect::VENGEANCE},
      {0x014D, /* 0x0044 */ AssistEffect::SQUEEZE},
      {0x014E, /* 0x0045 */ AssistEffect::HOMESICK},
      {0x0240, /* 0x0046 */ AssistEffect::BOMB},
      {0x0241, /* 0x0047 */ AssistEffect::SKIP_TURN},
      {0x0242, /* 0x0048 */ AssistEffect::BATTLE_ROYALE},
  });
  try {
    return card_id_to_effect.at(card_id);
  } catch (const out_of_range&) {
  }
  if (!is_nte) {
    try {
      return card_id_to_effect_final_only.at(card_id);
    } catch (const out_of_range&) {
    }
  }
  return AssistEffect::NONE;
}

AssistServer::AssistServer(shared_ptr<Server> server)
    : w_server(server),
      assist_effects(AssistEffect::NONE),
      num_assist_cards_set(0),
      client_ids_with_assists(0xFF),
      active_assist_effects(AssistEffect::NONE),
      num_active_assists(0) {}

shared_ptr<Server> AssistServer::server() {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

shared_ptr<const Server> AssistServer::server() const {
  auto s = this->w_server.lock();
  if (!s) {
    throw runtime_error("server is deleted");
  }
  return s;
}

uint16_t AssistServer::card_id_for_card_ref(uint16_t card_ref) const {
  return this->server()->card_id_for_card_ref(card_ref);
}

shared_ptr<const CardIndex::CardEntry> AssistServer::definition_for_card_id(
    uint16_t card_id) const {
  return this->server()->definition_for_card_id(card_id);
}

uint32_t AssistServer::compute_num_assist_effects_for_client(uint16_t client_id) {
  this->populate_effects();
  this->num_assist_cards_set = 0;
  if (this->should_block_assist_effects_for_client(client_id)) {
    this->num_active_assists = 0;
    return 0;
  }

  for (size_t z = 0; z < 4; z++) {
    auto ce = this->assist_card_defs[z];
    auto hes = this->hand_and_equip_states[z];
    if (ce && (!hes || (hes->assist_delay_turns < 1))) {
      bool affected = false;
      if (ce->def.target_mode == TargetMode::TEAM) {
        auto this_deck_entry = this->deck_entries[client_id];
        auto other_deck_entry = this->deck_entries[z];
        if (this_deck_entry && other_deck_entry &&
            (this_deck_entry->team_id == other_deck_entry->team_id)) {
          affected = true;
        }
      } else if ((ce->def.target_mode == TargetMode::SELF) && (z == client_id)) {
        affected = true;
      } else if (ce->def.target_mode == TargetMode::EVERYONE) {
        affected = true;
      }
      if (affected) {
        this->client_ids_with_assists[this->num_assist_cards_set++] = z;
      }
    }
  }

  this->recompute_effects();
  return this->num_assist_cards_set;
}

uint32_t AssistServer::compute_num_assist_effects_for_team(uint32_t team_id) {
  this->num_assist_cards_set = 0;
  for (size_t z = 0; z < 4; z++) {
    auto ce = this->assist_card_defs[z];
    auto hes = this->hand_and_equip_states[z];
    if (ce && (!hes || (hes->assist_delay_turns < 1))) {
      bool affected = false;
      if (ce->def.target_mode == TargetMode::TEAM) {
        if (this->deck_entries[z] && (this->deck_entries[z]->team_id == team_id)) {
          affected = true;
        }
      } else if (ce->def.target_mode == TargetMode::EVERYONE) {
        affected = true;
      }
      if (affected) {
        this->client_ids_with_assists[this->num_assist_cards_set++] = z;
      }
    }
  }
  this->recompute_effects();
  return this->num_assist_cards_set;
}

bool AssistServer::should_block_assist_effects_for_client(uint16_t client_id) const {
  for (size_t z = 0; z < 4; z++) {
    auto eff = this->assist_effects[z];
    auto ce = this->assist_card_defs[z];
    if (((eff == AssistEffect::RESISTANCE) || (eff == AssistEffect::INDEPENDENT)) && ce) {
      if (ce->def.target_mode == TargetMode::TEAM) {
        if (this->deck_entries[client_id] && this->deck_entries[z] &&
            (this->deck_entries[client_id]->team_id == this->deck_entries[z]->team_id)) {
          return true;
        }
      } else if ((ce->def.target_mode == TargetMode::SELF) && (client_id == z)) {
        return true;
      } else if (ce->def.target_mode == TargetMode::EVERYONE) {
        return true;
      }
    }
  }
  return false;
}

AssistEffect AssistServer::get_active_assist_by_index(size_t index) const {
  if (index < this->num_active_assists) {
    return this->active_assist_effects[index];
  }
  return AssistEffect::NONE;
}

void AssistServer::populate_effects() {
  bool is_nte = this->server()->options.is_nte();
  for (size_t z = 0; z < 4; z++) {
    this->assist_card_defs[z] = nullptr;
    this->assist_effects[z] = AssistEffect::NONE;
    const auto& hes = this->hand_and_equip_states[z];
    if (hes) {
      uint16_t card_id = hes->assist_card_id == 0xFFFF
          ? this->card_id_for_card_ref(hes->assist_card_ref)
          : hes->assist_card_id.load();
      this->assist_effects[z] = assist_effect_number_for_card_id(card_id, is_nte);
      if (this->assist_effects[z] != AssistEffect::NONE) {
        this->assist_card_defs[z] = this->definition_for_card_id(card_id);
      }
    }
  }
}

void AssistServer::recompute_effects() {
  for (size_t z = 0; z < 4; z++) {
    this->active_assist_effects[z] = AssistEffect::NONE;
    this->active_assist_card_defs[z] = nullptr;
  }
  this->num_active_assists = 0;

  if (this->num_assist_cards_set != 0) {
    for (size_t z = 0; z < this->num_assist_cards_set; z++) {
      auto eff = this->assist_effects[this->client_ids_with_assists[z]];
      if (eff == AssistEffect::RESISTANCE || eff == AssistEffect::INDEPENDENT) {
        return;
      }
    }

    // Note: this->num_assist_cards_set is > 0 when we get here
    for (size_t z = 0; z < this->num_assist_cards_set - 1; z++) {
      for (size_t w = z + 1; w < this->num_assist_cards_set; w++) {
        uint8_t z_client_id = this->client_ids_with_assists[z];
        uint8_t w_client_id = this->client_ids_with_assists[w];
        if (this->hand_and_equip_states[w_client_id]->assist_card_set_number <
            this->hand_and_equip_states[z_client_id]->assist_card_set_number) {
          this->client_ids_with_assists[z] = w_client_id;
          this->client_ids_with_assists[w] = z_client_id;
        }
      }
    }

    this->num_active_assists = this->num_assist_cards_set;
    for (size_t z = 0; z < this->num_assist_cards_set; z++) {
      this->active_assist_effects[z] = this->assist_effects[this->client_ids_with_assists[z]];
      this->active_assist_card_defs[z] = this->assist_card_defs[this->client_ids_with_assists[z]];
    }
  }
  return;
}

} // namespace Episode3
