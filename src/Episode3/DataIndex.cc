#include "DataIndex.hh"

#include <stdint.h>

#include <array>
#include <deque>
#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>

#include "../Loggers.hh"
#include "../Compression.hh"
#include "../Text.hh"

using namespace std;

namespace Episode3 {



const char* name_for_attack_medium(AttackMedium medium) {
  switch (medium) {
    case AttackMedium::UNKNOWN:
      return "UNKNOWN";
    case AttackMedium::PHYSICAL:
      return "PHYSICAL";
    case AttackMedium::TECH:
      return "TECH";
    case AttackMedium::UNKNOWN_03:
      return "UNKNOWN_03";
    case AttackMedium::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}



Location::Location() : Location(0, 0) { }
Location::Location(uint8_t x, uint8_t y) : Location(x, y, Direction::RIGHT) { }
Location::Location(uint8_t x, uint8_t y, Direction direction)
  : x(x), y(y), direction(direction), unused(0) { }

bool Location::operator==(const Location& other) const {
  return (this->x == other.x) &&
         (this->y == other.y) &&
         (this->direction == other.direction) &&
         (this->unused == other.unused);
}
bool Location::operator!=(const Location& other) const {
  return !this->operator==(other);
}

std::string Location::str() const {
  return string_printf("Location[x=%hhu, y=%hhu, dir=%s, u=%hhu]",
      this->x, this->y, name_for_direction(this->direction), this->unused);
}

void Location::clear() {
  this->x = 0;
  this->y = 0;
  this->direction = Direction::RIGHT;
  this->unused = 0;
}

void Location::clear_FF() {
  this->x = 0xFF;
  this->y = 0xFF;
  this->direction = Direction::INVALID_FF;
  this->unused = 0xFF;
}



Direction turn_left(Direction d) {
  switch (d) {
    case Direction::RIGHT:
      return Direction::UP;
    case Direction::UP:
      return Direction::LEFT;
    case Direction::LEFT:
      return Direction::DOWN;
    case Direction::DOWN:
      return Direction::RIGHT;
    default:
      return Direction::INVALID_FF;
  }
}

Direction turn_right(Direction d) {
  switch (d) {
    case Direction::RIGHT:
      return Direction::DOWN;
    case Direction::UP:
      return Direction::RIGHT;
    case Direction::LEFT:
      return Direction::UP;
    case Direction::DOWN:
      return Direction::LEFT;
    default:
      return Direction::INVALID_FF;
  }
}

Direction turn_around(Direction d) {
  switch (d) {
    case Direction::RIGHT:
      return Direction::LEFT;
    case Direction::UP:
      return Direction::DOWN;
    case Direction::LEFT:
      return Direction::RIGHT;
    case Direction::DOWN:
      return Direction::UP;
    default:
      return Direction::INVALID_FF;
  }
}

const char* name_for_direction(Direction d) {
  switch (d) {
    case Direction::RIGHT:
      return "LEFT";
    case Direction::UP:
      return "DOWN";
    case Direction::LEFT:
      return "RIGHT";
    case Direction::DOWN:
      return "UP";
    case Direction::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}



bool card_class_is_tech_like(CardClass cc) {
  return (cc == CardClass::TECH) ||
         (cc == CardClass::PHOTON_BLAST) ||
         (cc == CardClass::BOSS_TECH);
}



static const vector<const char*> name_for_card_type({
  "HunterSC",
  "ArkzSC",
  "Item",
  "Creature",
  "Action",
  "Assist",
});

static const unordered_map<string, const char*> description_for_expr_token({
  {"f",   "Number of FCs controlled by current SC"},
  {"d",   "Die roll"},
  {"ap",  "Attacker effective AP"},
  {"tp",  "Attacker effective TP"},
  {"hp",  "Current HP"},
  {"mhp", "Maximum HP"},
  {"dm",  "Physical damage"},
  {"tdm", "Technique damage"},
  {"tf",  "Number of SC\'s destroyed FCs"},
  {"ac",  "Remaining ATK points"},
  {"php", "Maximum HP"},
  {"dc",  "Die roll"},
  {"cs",  "Card set cost"},
  {"a",   "Number of FCs on all teams"},
  {"kap", "Action cards AP"},
  {"ktp", "Action cards TP"},
  {"dn",  "Unknown: dn"},
  {"hf",  "Number of item or creature cards in hand"},
  {"df",  "Number of destroyed ally FCs (including SC\'s own)"},
  {"ff",  "Number of ally FCs (including SC\'s own)"},
  {"ef",  "Number of enemy FCs"},
  {"bi",  "Number of Native FCs on either team"},
  {"ab",  "Number of A.Beast FCs on either team"},
  {"mc",  "Number of Machine FCs on either team"},
  {"dk",  "Number of Dark FCs on either team"},
  {"sa",  "Number of Sword-type items on either team"},
  {"gn",  "Number of Gun-type items on either team"},
  {"wd",  "Number of Cane-type items on either team"},
  {"tt",  "Physical damage"},
  {"lv",  "Dice boost"},
  {"adm", "SC attack damage"},
  {"ddm", "Defending damage"},
  {"sat", "Number of Sword-type items on SC\'s team"},
  {"edm", "Defending damage"}, // TODO: How is this different from ddm?
  {"ldm", "Unknown: ldm"}, // Unused
  {"rdm", "Defending damage"}, // TODO: How is this different from ddm/edm?
  {"fdm", "Final damage (after defense)"},
  {"ndm", "Unknown: ndm"}, // Unused
  {"ehp", "Attacker HP"},
});

// Arguments are encoded as 3-character null-terminated strings (why?!), and are
// used for adding conditions to effects (e.g. making them only trigger in
// certain situations) or otherwise customizing their results. The arguments are
// heterogeneous based on their position; that is, the first argument always has
// the same meaning, and meaning letters that are valid in arg1 are not
// necessarily valid in arg2, etc.
// Argument meanings:
// a01 = ???
// e00 = effect lasts while equipped? (in contrast to tXX)
// pXX = who to target (see description_for_p_target)
// In arg2:
// bXX = require attack doing not more than XX damage
// cXY/CXY = linked items (require item with cYX/CYX to be equipped as well)
// dXY = roll one die; require result between X and Y inclusive
// hXX = require HP >= XX
// iXX = require HP <= XX
// mXX = require attack doing at least XX damage
// nXX = require condition (see description_for_n_condition below)
// oXX = seems to be "require previous random-condition effect to have happened"
//       TODO: this is used as both o01 (recovery) and o11 (reflection)
//             conditions - why the difference?
// rXX = randomly pass with XX% chance (if XX == 00, 100% chance?)
// sXY = require card cost between X and Y ATK points (inclusive)
// tXX = lasts XX turns, or activate after XX turns

static const vector<const char*> description_for_n_condition({
  /* n00 */ "Always true",
  /* n01 */ "Card is Hunters-side SC",
  /* n02 */ "Destroyed with a single attack",
  /* n03 */ "Technique or PB action card was used",
  /* n04 */ "Attack has Pierce",
  /* n05 */ "Attack has Rampage",
  /* n06 */ "Native attribute",
  /* n07 */ "A.Beast attribute",
  /* n08 */ "Machine attribute",
  /* n09 */ "Dark attribute",
  /* n10 */ "Sword-type item",
  /* n11 */ "Gun-type item",
  /* n12 */ "Cane-type item",
  /* n13 */ "Guard item or MAG",
  /* n14 */ "Story Character",
  /* n15 */ "Attacker does not use action cards",
  /* n16 */ "Aerial attribute",
  /* n17 */ "Same AP as opponent",
  /* n18 */ "Any target is an SC",
  /* n19 */ "Has Paralyzed condition",
  /* n20 */ "Has Frozen condition",
  /* n21 */ "???", // TODO: This appears related to Pierce/Rampage
  /* n22 */ "???", // TODO: This appears related to Pierce/Rampage
});

static const vector<const char*> description_for_p_target({
  /* p00 */ "Unknown: p00", // Unused; probably invalid
  /* p01 */ "SC / FC who set the card",
  /* p02 */ "Attacking SC / FC",
  /* p03 */ "Unknown: p03", // Unused
  /* p04 */ "Unknown: p04", // Unused
  /* p05 */ "SC / FC who set the card", // Identical to p01
  /* p06 */ "??? (TODO)",
  /* p07 */ "??? (TODO; Weakness)",
  /* p08 */ "FC\'s owner SC",
  /* p09 */ "Unknown: p09", // Unused
  /* p10 */ "All ally FCs",
  /* p11 */ "All ally FCs", // TODO: how is this different from p10?
  /* p12 */ "All non-aerial FCs on both teams",
  /* p13 */ "All FCs on both teams that are Frozen",
  /* p14 */ "All FCs on both teams that have <= 3 HP",
  /* p15 */ "All FCs except SCs", // TODO: used during attacks only?
  /* p16 */ "All FCs except SCs", // TODO: used during attacks only? how is this different from p15?
  /* p17 */ "This card",
  /* p18 */ "SC who equipped this card",
  /* p19 */ "Unknown: p19", // Unused
  /* p20 */ "Unknown: p20", // Unused
  /* p21 */ "Unknown: p21", // Unused
  /* p22 */ "All characters (SCs & FCs) including this card", // TODO: But why does Shifta apply only to allies then?
  /* p23 */ "All characters (SCs & FCs) except this card",
  /* p24 */ "All FCs on both teams that have Paralysis",
  /* p25 */ "Unknown: p25", // Unused
  /* p26 */ "Unknown: p26", // Unused
  /* p27 */ "Unknown: p27", // Unused
  /* p28 */ "Unknown: p28", // Unused
  /* p29 */ "Unknown: p29", // Unused
  /* p30 */ "Unknown: p30", // Unused
  /* p31 */ "Unknown: p31", // Unused
  /* p32 */ "Unknown: p32", // Unused
  /* p33 */ "Unknown: p33", // Unused
  /* p34 */ "Unknown: p34", // Unused
  /* p35 */ "All characters (SCs & FCs) within range", // Used for Explosion effect
  /* p36 */ "All ally SCs within range, but not the caster", // Resta
  /* p37 */ "All FCs or all opponent FCs (TODO)", // TODO: when to use which selector? is a3 involved here somehow?
  /* p38 */ "All allies except items within range (and not this card)",
  /* p39 */ "All FCs that cost 4 or more points",
  /* p40 */ "All FCs that cost 3 or fewer points",
  /* p41 */ "Unknown: p41", // Unused
  /* p42 */ "Attacker during defense phase", // Only used by TP Defense
  /* p43 */ "Owner SC of defending FC during attack",
  /* p44 */ "SC\'s own creature FCs within range",
  /* p45 */ "Both attacker and defender", // Used for Snatch, which moves EXP from one to the other
  /* p46 */ "All SCs & FCs one space left or right of this card",
  /* p47 */ "FC\'s owner Boss SC", // Only used for Gibbles+ which explicitly mentions Boss SC, so it looks like this is p08 but for bosses
  /* p48 */ "Everything within range, including this card\'s user", // Madness
  /* p49 */ "All ally FCs within range except this card",
});

struct ConditionDescription {
  bool has_expr;
  const char* name;
  const char* description;
};

static const vector<ConditionDescription> description_for_condition_type({
  /* 0x00 */ {false, "NONE",                 nullptr},
  /* 0x01 */ {true,  "AP_BOOST",             "Temporarily increase AP by N"},
  /* 0x02 */ {false, "RAMPAGE",              "Rampage"},
  /* 0x03 */ {true,  "MULTI_STRIKE",         "Duplicate attack N times"},
  /* 0x04 */ {true,  "DAMAGE_MOD_1",         "Set attack damage / AP to N after action cards applied (step 1)"},
  /* 0x05 */ {false, "IMMOBILE",             "Give Immobile condition"},
  /* 0x06 */ {false, "HOLD",                 "Give Hold condition"},
  /* 0x07 */ {false, "UNKNOWN_07",           nullptr},
  /* 0x08 */ {true,  "TP_BOOST",             "Add N TP temporarily during attack"},
  /* 0x09 */ {true,  "GIVE_DAMAGE",          "Cause direct N HP loss"},
  /* 0x0A */ {false, "GUOM",                 "Give Guom condition"},
  /* 0x0B */ {false, "PARALYZE",             "Give Paralysis condition"},
  /* 0x0C */ {false, "UNKNOWN_0C",           nullptr},
  /* 0x0D */ {false, "A_H_SWAP",             "Swap AP and HP temporarily"},
  /* 0x0E */ {false, "PIERCE",               "Attack SC directly even if they have items equipped"},
  /* 0x0F */ {false, "UNKNOWN_0F",           nullptr},
  /* 0x10 */ {true,  "HEAL",                 "Increase HP by N"},
  /* 0x11 */ {false, "RETURN_TO_HAND",       "Return card to hand"},
  /* 0x12 */ {false, "UNKNOWN_12",           nullptr},
  /* 0x13 */ {false, "UNKNOWN_13",           nullptr},
  /* 0x14 */ {false, "ACID",                 "Give Acid condition"},
  /* 0x15 */ {false, "UNKNOWN_15",           nullptr},
  /* 0x16 */ {true,  "MIGHTY_KNUCKLE",       "Temporarily increase AP by N, and set ATK dice to zero"},
  /* 0x17 */ {true,  "UNIT_BLOW",            "Temporarily increase AP by N * number of this card set within phase"},
  /* 0x18 */ {false, "CURSE",                "Give Curse condition"},
  /* 0x19 */ {false, "COMBO_AP",             "Temporarily increase AP by number of this card set within phase"},
  /* 0x1A */ {false, "PIERCE_RAMPAGE_BLOCK", "Block attack if Pierce/Rampage (?)"},
  /* 0x1B */ {false, "ABILITY_TRAP",         "Temporarily disable opponent abilities"},
  /* 0x1C */ {false, "FREEZE",               "Give Freeze condition"},
  /* 0x1D */ {false, "ANTI_ABNORMALITY_1",   "Cure all conditions"},
  /* 0x1E */ {false, "UNKNOWN_1E",           nullptr},
  /* 0x1F */ {false, "EXPLOSION",            "Damage all SCs and FCs by number of this same card set * 2"},
  /* 0x20 */ {false, "UNKNOWN_20",           nullptr},
  /* 0x21 */ {false, "UNKNOWN_21",           nullptr},
  /* 0x22 */ {false, "UNKNOWN_22",           nullptr},
  /* 0x23 */ {false, "RETURN_TO_DECK",       "Cancel discard and move to bottom of deck instead"},
  /* 0x24 */ {false, "AERIAL",               "Give Aerial status"},
  /* 0x25 */ {true,  "AP_LOSS",              "Make attacker temporarily lose N AP during defense"},
  /* 0x26 */ {true,  "BONUS_FROM_LEADER",    "Gain AP equal to the number of cards of type N on the field"},
  /* 0x27 */ {false, "FREE_MANEUVER",        "Enable movement over occupied tiles"},
  /* 0x28 */ {false, "HASTE",                "Make move actions free"},
  /* 0x29 */ {true,  "CLONE",                "Make setting this card free if at least one card of type N is already on the field"},
  /* 0x2A */ {true,  "DEF_DISABLE_BY_COST",  "Disable use of any defense cards costing between (N / 10) and (N % 10) points, inclusive"},
  /* 0x2B */ {true,  "FILIAL",               "Increase controlling SC\'s HP by N when this card is destroyed"},
  /* 0x2C */ {true,  "SNATCH",               "Steal N EXP during attack"},
  /* 0x2D */ {true,  "HAND_DISRUPTER",       "Discard N cards from hand immediately"},
  /* 0x2E */ {false, "DROP",                 "Give Drop condition"},
  /* 0x2F */ {false, "ACTION_DISRUPTER",     "Destroy all action cards used by attacker"},
  /* 0x30 */ {true,  "SET_HP",               "Set HP to N"},
  /* 0x31 */ {false, "NATIVE_SHIELD",        "Block attacks from Native creatures"},
  /* 0x32 */ {false, "A_BEAST_SHIELD",       "Block attacks from A.Beast creatures"},
  /* 0x33 */ {false, "MACHINE_SHIELD",       "Block attacks from Machine creatures"},
  /* 0x34 */ {false, "DARK_SHIELD",          "Block attacks from Dark creatures"},
  /* 0x35 */ {false, "SWORD_SHIELD",         "Block attacks from Sword items"},
  /* 0x36 */ {false, "GUN_SHIELD",           "Block attacks from Gun items"},
  /* 0x37 */ {false, "CANE_SHIELD",          "Block attacks from Cane items"},
  /* 0x38 */ {false, "UNKNOWN_38",           nullptr},
  /* 0x39 */ {false, "UNKNOWN_39",           nullptr},
  /* 0x3A */ {false, "DEFENDER",             "Make attacks go to setter of this card instead of original target"},
  /* 0x3B */ {false, "SURVIVAL_DECOYS",      "Redirect damage for multi-sided attack"},
  /* 0x3C */ {true,  "GIVE_OR_TAKE_EXP",     "Give N EXP, or take if N is negative"},
  /* 0x3D */ {false, "UNKNOWN_3D",           nullptr},
  /* 0x3E */ {false, "DEATH_COMPANION",      "If this card has 1 or 2 HP, set its HP to N"},
  /* 0x3F */ {true,  "EXP_DECOY",            "If defender has EXP, lose EXP instead of getting damage when attacked"},
  /* 0x40 */ {true,  "SET_MV",               "Set MV to N"},
  /* 0x41 */ {true,  "GROUP",                "Temporarily increase AP by N * number of this card on field, excluding itself"},
  /* 0x42 */ {false, "BERSERK",              "User of this card receives the same damage as target, and isn\'t helped by target\'s defense cards"},
  /* 0x43 */ {false, "GUARD_CREATURE",       "Attacks on controlling SC damage this card instead"},
  /* 0x44 */ {false, "TECH",                 "Technique cards cost 1 fewer ATK point"},
  /* 0x45 */ {false, "BIG_SWING",            "Increase all attacking ATK costs by 1"},
  /* 0x46 */ {false, "UNKNOWN_46",           nullptr},
  /* 0x47 */ {false, "SHIELD_WEAPON",        "Limit attacker\'s choice of target to guard items"},
  /* 0x48 */ {false, "ATK_DICE_BOOST",       "Increase ATK dice roll by 1"},
  /* 0x49 */ {false, "UNKNOWN_49",           nullptr},
  /* 0x4A */ {false, "MAJOR_PIERCE",         "If SC has over half of max HP, attacks target SC instead of equipped items"},
  /* 0x4B */ {false, "HEAVY_PIERCE",         "If SC has 3 or more items equipped, attacks target SC instead of equipped items"},
  /* 0x4C */ {false, "MAJOR_RAMPAGE",        "If SC has over half of max HP, attacks target SC and all equipped items"},
  /* 0x4D */ {false, "HEAVY_RAMPAGE",        "If SC has 3 or more items equipped, attacks target SC and all equipped items"},
  /* 0x4E */ {true,  "AP_GROWTH",            "Permanently increase AP by N"},
  /* 0x4F */ {true,  "TP_GROWTH",            "Permanently increase TP by N"},
  /* 0x50 */ {true,  "REBORN",               "If any card of type N is on the field, this card goes to the hand when destroyed instead of being discarded"},
  /* 0x51 */ {true,  "COPY",                 "Temporarily set AP/TP to N percent (or 100% if N is 0) of opponent\'s values"},
  /* 0x52 */ {false, "UNKNOWN_52",           nullptr},
  /* 0x53 */ {true,  "MISC_GUARDS",          "Add N to card\'s defense value"},
  /* 0x54 */ {true,  "AP_OVERRIDE",          "Set AP to N temporarily"},
  /* 0x55 */ {true,  "TP_OVERRIDE",          "Set TP to N temporarily"},
  /* 0x56 */ {false, "RETURN",               "Return card to hand on destruction instead of discarding"},
  /* 0x57 */ {false, "A_T_SWAP_PERM",        "Permanently swap AP and TP"},
  /* 0x58 */ {false, "A_H_SWAP_PERM",        "Permanently swap AP and HP"},
  /* 0x59 */ {true,  "SLAYERS_ASSASSINS",    "Temporarily increase AP during attack"},
  /* 0x5A */ {false, "ANTI_ABNORMALITY_2",   "Remove all conditions"},
  /* 0x5B */ {false, "FIXED_RANGE",          "Use SC\'s range instead of weapon or attack card ranges"},
  /* 0x5C */ {false, "ELUDE",                "SC does not lose HP when equipped items are destroyed"},
  /* 0x5D */ {false, "PARRY",                "Forward attack to a random FC within one tile of original target, excluding attacker and original target"},
  /* 0x5E */ {false, "BLOCK_ATTACK",         "Completely block attack"},
  /* 0x5F */ {false, "UNKNOWN_5F",           nullptr},
  /* 0x60 */ {false, "UNKNOWN_60",           nullptr},
  /* 0x61 */ {true,  "COMBO_TP",             "Gain TP equal to the number of cards of type N on the field"},
  /* 0x62 */ {true,  "MISC_AP_BONUSES",      "Temporarily increase AP by N"},
  /* 0x63 */ {true,  "MISC_TP_BONUSES",      "Temporarily increase TP by N"},
  /* 0x64 */ {false, "UNKNOWN_64",           nullptr},
  /* 0x65 */ {true,  "MISC_DEFENSE_BONUSES", "Decrease damage by N"},
  /* 0x66 */ {true,  "MOSTLY_HALFGUARDS",    "Reduce damage from incoming attack by N"},
  /* 0x67 */ {false, "PERIODIC_FIELD",       "Swap immunity to tech or physical attacks"},
  /* 0x68 */ {false, "FC_LIMIT_BY_COUNT",    "Change FC limit from 8 ATK points total to 4 FCs total"},
  /* 0x69 */ {false, "UNKNOWN_69",           nullptr},
  /* 0x6A */ {true,  "MV_BONUS",             "Increase MV by N"},
  /* 0x6B */ {true,  "FORWARD_DAMAGE",       "Give N damage back to attacker during defense (?) (TODO)"},
  /* 0x6C */ {true,  "WEAK_SPOT_INFLUENCE",  "Temporarily decrease AP by N"},
  /* 0x6D */ {true,  "DAMAGE_MODIFIER_2",    "Set attack damage / AP after action cards applied (step 2)"},
  /* 0x6E */ {true,  "WEAK_HIT_BLOCK",       "Block all attacks of N damage or less"},
  /* 0x6F */ {true,  "AP_SILENCE",           "Temporarily decrease AP of opponent by N"},
  /* 0x70 */ {true,  "TP_SILENCE",           "Temporarily decrease TP of opponent by N"},
  /* 0x71 */ {false, "A_T_SWAP",             "Temporarily swap AP and TP"},
  /* 0x72 */ {true,  "HALFGUARD",            "Halve damage from attacks that would inflict N or more damage"},
  /* 0x73 */ {false, "UNKNOWN_73",           nullptr},
  /* 0x74 */ {true,  "RAMPAGE_AP_LOSS",      "Temporarily reduce AP by N"},
  /* 0x75 */ {false, "UNKNOWN_75",           nullptr},
  /* 0x76 */ {false, "REFLECT",              "Generate reverse attack"},
  /* 0x77 */ {false, "UNKNOWN_77",           nullptr},
  /* 0x78 */ {false, "ANY",                  nullptr}, // Treated as "any condition" in find functions
  /* 0x79 */ {false, "UNKNOWN_79",           nullptr},
  /* 0x7A */ {false, "UNKNOWN_7A",           nullptr},
  /* 0x7B */ {false, "UNKNOWN_7B",           nullptr},
  /* 0x7C */ {false, "UNKNOWN_7C",           nullptr},
  /* 0x7D */ {false, "UNKNOWN_7D",           nullptr},
});

const char* name_for_condition_type(ConditionType cond_type) {
  try {
    return description_for_condition_type.at(static_cast<size_t>(cond_type)).name;
  } catch (const out_of_range&) {
    return "__INVALID__";
  }
}



const char* name_for_action_subphase(ActionSubphase subphase) {
  switch (subphase) {
    case ActionSubphase::ATTACK:
      return "ATTACK";
    case ActionSubphase::DEFENSE:
      return "DEFENSE";
    case ActionSubphase::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}



void CardDefinition::Stat::decode_code() {
  this->type = static_cast<Type>(this->code / 1000);
  int16_t value = this->code - (this->type * 1000);
  if (value != 999) {
    switch (this->type) {
      case Type::BLANK:
        this->stat = 0;
        break;
      case Type::STAT:
      case Type::PLUS_STAT:
      case Type::EQUALS_STAT:
        this->stat = value;
        break;
      case Type::MINUS_STAT:
        this->stat = -value;
        break;
      default:
        throw runtime_error("invalid card stat type");
    }
  } else {
    this->stat = 0;
    this->type = static_cast<Type>(this->type + 4);
  }
}

string CardDefinition::Stat::str() const {
  switch (this->type) {
    case Type::BLANK:
      return "";
    case Type::STAT:
      return string_printf("%hhd", this->stat);
    case Type::PLUS_STAT:
      return string_printf("+%hhd", this->stat);
    case Type::MINUS_STAT:
      return string_printf("-%d", -this->stat);
    case Type::EQUALS_STAT:
      return string_printf("=%hhd", this->stat);
    case Type::UNKNOWN:
      return "?";
    case Type::PLUS_UNKNOWN:
      return "+?";
    case Type::MINUS_UNKNOWN:
      return "-?";
    case Type::EQUALS_UNKNOWN:
      return "=?";
    default:
      return string_printf("[%02hhX %02hhX]", this->type, this->stat);
  }
}



bool CardDefinition::Effect::is_empty() const {
  return (this->effect_num == 0 &&
          this->type == ConditionType::NONE &&
          this->expr.is_filled_with(0) &&
          this->when == 0 &&
          this->arg1.is_filled_with(0) &&
          this->arg2.is_filled_with(0) &&
          this->arg3.is_filled_with(0) &&
          this->apply_criterion == CriterionCode::NONE &&
          this->unknown_a2 == 0);
}

string CardDefinition::Effect::str_for_arg(const string& arg) {
  if (arg.empty()) {
    return arg;
  }
  if (arg.size() != 3) {
    return arg + "/(invalid)";
  }
  size_t value;
  try {
    value = stoul(arg.c_str() + 1, nullptr, 10);
  } catch (const invalid_argument&) {
    return arg + "/(invalid)";
  }

  switch (arg[0]) {
    case 'a':
      return arg + "/(unknown)";
    case 'C':
    case 'c':
      return string_printf("%s/Req. linked item (%zu=>%zu)", arg.c_str(), value / 10, value % 10);
    case 'd':
      return string_printf("%s/Req. die roll in [%zu, %zu]", arg.c_str(), value / 10, value % 10);
    case 'e':
      return arg + "/While equipped";
    case 'h':
      return string_printf("%s/Req. HP >= %zu", arg.c_str(), value);
    case 'i':
      return string_printf("%s/Req. HP <= %zu", arg.c_str(), value);
    case 'n':
      try {
        return string_printf("%s/Req. condition: %s", arg.c_str(), description_for_n_condition.at(value));
      } catch (const out_of_range&) {
        return arg + "/(unknown)";
      }
    case 'o':
      return arg + "/Req. prev effect conditions passed";
    case 'p':
      try {
        return string_printf("%s/Target: %s", arg.c_str(), description_for_p_target.at(value));
      } catch (const out_of_range&) {
        return arg + "/(unknown)";
      }
    case 'r':
      return string_printf("%s/Req. random with %zu%% chance", arg.c_str(), value == 0 ? 100 : value);
    case 's':
      return string_printf("%s/Req. cost in [%zu, %zu]", arg.c_str(), value / 10, value % 10);
    case 't':
      return string_printf("%s/Turns: %zu", arg.c_str(), value);
    default:
      return arg + "/(unknown)";
  }
}

string CardDefinition::Effect::str() const {
  uint8_t type = static_cast<uint8_t>(this->type);
  string cmd_str = string_printf("(%hhu) %02hhX", this->effect_num, type);
  try {
    const char* name = description_for_condition_type.at(type).name;
    if (name) {
      cmd_str += ':';
      cmd_str += name;
    }
  } catch (const out_of_range&) { }

  string expr_str = this->expr;
  if (!expr_str.empty()) {
    expr_str = ", expr=" + expr_str;
  }

  string arg1str = this->str_for_arg(this->arg1);
  string arg2str = this->str_for_arg(this->arg2);
  string arg3str = this->str_for_arg(this->arg3);
  return string_printf("(cmd=%s%s, when=%02hhX, arg1=%s, arg2=%s, arg3=%s, cond=%02hhX, a2=%02hhX)",
      cmd_str.c_str(), expr_str.c_str(), this->when, arg1str.data(),
      arg2str.data(), arg3str.data(), static_cast<uint8_t>(this->apply_criterion), this->unknown_a2);
}



bool CardDefinition::is_sc() const {
  return (this->type == CardType::HUNTERS_SC) || (this->type == CardType::ARKZ_SC);
}

bool CardDefinition::is_fc() const {
  return (this->type == CardType::ITEM) || (this->type == CardType::CREATURE);
}

bool CardDefinition::is_named_android_sc() const {
  static const unordered_set<uint16_t> TARGET_IDS({
      0x0005, 0x0007, 0x0110, 0x0113, 0x0114, 0x0117, 0x011B, 0x011F});
  return TARGET_IDS.count(this->card_id);
}

bool CardDefinition::any_top_color_matches(const CardDefinition& other) const {
  for (size_t x = 0; x < this->top_colors.size(); x++) {
    if (this->top_colors[x] != 0) {
      for (size_t y = 0; y < other.top_colors.size(); y++) {
        if (this->top_colors[x] == other.top_colors[y]) {
          return true;
        }
      }
    }
  }
  return false;
}

CardClass CardDefinition::card_class() const {
  return static_cast<CardClass>(this->be_card_class.load());
}



void CardDefinition::decode_range() {
  // If the cell representing the FC is nonzero, the card has a range from a
  // list of constants. Otherwise, its range is already defined in the range
  // array and should be left alone.
  uint8_t index = (this->range[4] >> 8) & 0xF;
  if (index != 0) {
    this->range.clear(0);
    switch (index) {
      case 1: // Single cell in front of FC
        this->range[3] = 0x00000100;
        break;
      case 2: // Cell in front of FC and the front-left and front-right (Slash)
        this->range[3] = 0x00001110;
        break;
      case 3: // 3 cells in a line in front of FC
        this->range[1] = 0x00000100;
        this->range[2] = 0x00000100;
        this->range[3] = 0x00000100;
        break;
      case 4: // All 8 cells around FC
        this->range[3] = 0x00001110;
        this->range[4] = 0x00001010;
        this->range[5] = 0x00001110;
        break;
      case 5: // 2 cells in a line in front of FC
        this->range[2] = 0x00000100;
        this->range[3] = 0x00000100;
        break;
      case 6: // Entire field (renders as "A")
        for (size_t x = 0; x < 6; x++) {
          this->range[x] = 0x000FFFFF;
        }
        break;
      case 7: // Superposition of 4 and 5 (unused)
        this->range[2] = 0x00000100;
        this->range[3] = 0x00001110;
        this->range[4] = 0x00001010;
        this->range[5] = 0x00001110;
        break;
      case 8: // All 8 cells around FC and FC's cell
        this->range[3] = 0x00001110;
        this->range[4] = 0x00001110;
        this->range[5] = 0x00001110;
        break;
      case 9: // No cells
        break;
      // The table in the DOL file only appears to contain 9 entries; there are
      // some pointers immediately after. So probably if a card specified A-F,
      // its range would be filled in with garbage in the original game.
      default:
        throw runtime_error("invalid fixed range index");
    }
  }
}

string name_for_rarity(CardRarity rarity) {
  static const vector<const char*> names({
    "N1",
    "R1",
    "S",
    "E",
    "N2",
    "N3",
    "N4",
    "R2",
    "R3",
    "R4",
    "SS",
    "D1",
    "D2",
    "INVIS",
  });
  try {
    return names.at(static_cast<uint8_t>(rarity) - 1);
  } catch (const out_of_range&) {
    return string_printf("(%02hhX)", static_cast<uint8_t>(rarity));
  }
}

string name_for_target_mode(TargetMode target_mode) {
  static const vector<const char*> names({
    "NONE",
    "SINGLE",
    "MULTI",
    "SELF",
    "TEAM",
    "ALL",
    "MULTI-ALLY",
    "ALL-ALLY",
    "ALL-ATTACK",
    "OWN-FCS",
  });
  try {
    return names.at(static_cast<uint8_t>(target_mode));
  } catch (const out_of_range&) {
    return string_printf("(%02hhX)", static_cast<uint8_t>(target_mode));
  }
}

string string_for_colors(const parray<uint8_t, 8>& colors) {
  string ret;
  for (size_t x = 0; x < 8; x++) {
    if (colors[x]) {
      ret += '0' + colors[x];
    }
  }
  if (ret.empty()) {
    return "none";
  }
  return ret;
}

string string_for_assist_turns(uint8_t turns) {
  if (turns == 90) {
    return "ONCE";
  } else if (turns == 99) {
    return "FOREVER";
  } else {
    return string_printf("%hhu", turns);
  }
}

string string_for_range(const parray<be_uint32_t, 6>& range) {
  string ret;
  for (size_t x = 0; x < 6; x++) {
    ret += string_printf("%05" PRIX32 "/", range[x].load());
  }
  while (starts_with(ret, "00000/")) {
    ret = ret.substr(6);
  }
  if (!ret.empty()) {
    ret.resize(ret.size() - 1);
  }
  return ret;
}

string CardDefinition::str() const {
  string type_str;
  try {
    type_str = name_for_card_type.at(static_cast<uint8_t>(this->type));
  } catch (const out_of_range&) {
    type_str = string_printf("%02hhX", static_cast<uint8_t>(this->type));
  }
  string rarity_str = name_for_rarity(this->rarity);
  string target_mode_str = name_for_target_mode(this->target_mode);
  string range_str = string_for_range(this->range);
  string assist_turns_str = string_for_assist_turns(this->assist_turns);
  string hp_str = this->hp.str();
  string ap_str = this->ap.str();
  string tp_str = this->tp.str();
  string mv_str = this->mv.str();
  string left_str = string_for_colors(this->left_colors);
  string right_str = string_for_colors(this->right_colors);
  string top_str = string_for_colors(this->top_colors);
  string effects_str;
  for (size_t x = 0; x < 3; x++) {
    if (this->effects[x].is_empty()) {
      continue;
    }
    if (!effects_str.empty()) {
      effects_str += ", ";
    }
    effects_str += this->effects[x].str();
  }
  return string_printf(
      "[Card: %04" PRIX32 " name=%s type=%s usable_condition=%02hhX rare=%s "
      "cost=%hhX+%hhX target=%s range=%s assist_turns=%s cannot_move=%s "
      "cannot_attack=%s hidden=%s hp=%s ap=%s tp=%s mv=%s left=%s right=%s "
      "top=%s a2=%04hX class=%04hX assist_effect=[%hu, %hu] "
      "drop_rates=[%hu, %hu] effects=[%s]]",
      this->card_id.load(),
      this->en_name.data(),
      type_str.c_str(),
      static_cast<uint8_t>(this->usable_criterion),
      rarity_str.c_str(),
      this->self_cost,
      this->ally_cost,
      target_mode_str.c_str(),
      range_str.c_str(),
      assist_turns_str.c_str(),
      this->cannot_move ? "true" : "false",
      this->cannot_attack ? "true" : "false",
      this->hide_in_deck_edit ? "true" : "false",
      hp_str.c_str(),
      ap_str.c_str(),
      tp_str.c_str(),
      mv_str.c_str(),
      left_str.c_str(),
      right_str.c_str(),
      top_str.c_str(),
      this->unknown_a2.load(),
      this->be_card_class.load(),
      this->assist_effect[0].load(),
      this->assist_effect[1].load(),
      this->drop_rates[0].load(),
      this->drop_rates[1].load(),
      effects_str.c_str());
}



Rules::Rules() {
  this->clear();
}

Rules::Rules(shared_ptr<const JSONObject> json) {
  auto dict = json->as_dict();
  this->overall_time_limit = dict.at("overall_time_limit")->as_int();
  this->phase_time_limit = dict.at("phase_time_limit")->as_int();
  this->allowed_cards = static_cast<AllowedCards>(dict.at("allowed_cards")->as_int());
  this->min_dice = dict.at("min_dice")->as_int();
  this->max_dice = dict.at("max_dice")->as_int();
  this->disable_deck_shuffle = dict.at("disable_deck_shuffle")->as_int();
  this->disable_deck_loop = dict.at("disable_deck_loop")->as_int();
  this->char_hp = dict.at("char_hp")->as_int();
  this->hp_type = static_cast<HPType>(dict.at("hp_type")->as_int());
  this->no_assist_cards = dict.at("no_assist_cards")->as_int();
  this->disable_dialogue = dict.at("disable_dialogue")->as_int();
  this->dice_exchange_mode = static_cast<DiceExchangeMode>(dict.at("dice_exchange_mode")->as_int());
  this->disable_dice_boost = dict.at("disable_dice_boost")->as_int();
}

shared_ptr<JSONObject> Rules::json() const {
  unordered_map<string, shared_ptr<JSONObject>> dict;
  dict.emplace("overall_time_limit", make_json_int(this->overall_time_limit));
  dict.emplace("phase_time_limit", make_json_int(this->phase_time_limit));
  dict.emplace("allowed_cards", make_json_int(static_cast<uint8_t>(this->allowed_cards)));
  dict.emplace("min_dice", make_json_int(this->min_dice));
  dict.emplace("max_dice", make_json_int(this->max_dice));
  dict.emplace("disable_deck_shuffle", make_json_int(this->disable_deck_shuffle));
  dict.emplace("disable_deck_loop", make_json_int(this->disable_deck_loop));
  dict.emplace("char_hp", make_json_int(this->char_hp));
  dict.emplace("hp_type", make_json_int(static_cast<uint8_t>(this->hp_type)));
  dict.emplace("no_assist_cards", make_json_int(this->no_assist_cards));
  dict.emplace("disable_dialogue", make_json_int(this->disable_dialogue));
  dict.emplace("dice_exchange_mode", make_json_int(static_cast<uint8_t>(this->dice_exchange_mode)));
  dict.emplace("disable_dice_boost", make_json_int(this->disable_dice_boost));
  return shared_ptr<JSONObject>(new JSONObject(move(dict)));
}

void Rules::set_defaults() {
  this->clear();
  this->overall_time_limit = 24; // 2 hours
  this->phase_time_limit = 30;
  this->min_dice = 1;
  this->max_dice = 6;
  this->char_hp = 15;
}

void Rules::clear() {
  this->overall_time_limit = 0;
  this->phase_time_limit = 0;
  this->allowed_cards = AllowedCards::ALL;
  this->min_dice = 0;
  this->max_dice = 0;
  this->disable_deck_shuffle = 0;
  this->disable_deck_loop = 0;
  this->char_hp = 0;
  this->hp_type = HPType::DEFEAT_PLAYER;
  this->no_assist_cards = 0;
  this->disable_dialogue = 0;
  this->dice_exchange_mode = DiceExchangeMode::HIGH_ATK;
  this->disable_dice_boost = 0;
  this->unused.clear(0);
}

string Rules::str() const {
  vector<string> tokens;

  tokens.emplace_back(string_printf("char_hp=%hhu", this->char_hp));
  switch (this->hp_type) {
    case HPType::DEFEAT_PLAYER:
      tokens.emplace_back("hp_type=DEFEAT_PLAYER");
      break;
    case HPType::DEFEAT_TEAM:
      tokens.emplace_back("hp_type=DEFEAT_TEAM");
      break;
    case HPType::COMMON_HP:
      tokens.emplace_back("hp_type=COMMON_HP");
      break;
    default:
      tokens.emplace_back(string_printf("hp_type=(%02hhX)",
          static_cast<uint8_t>(this->hp_type)));
      break;
  }

  tokens.emplace_back(string_printf("min_dice=%hhu", this->min_dice));
  tokens.emplace_back(string_printf("max_dice=%hhu", this->max_dice));
  switch (this->dice_exchange_mode) {
    case DiceExchangeMode::HIGH_ATK:
      tokens.emplace_back("dice_exchange=HIGH_ATK");
      break;
    case DiceExchangeMode::HIGH_DEF:
      tokens.emplace_back("dice_exchange=HIGH_DEF");
      break;
    case DiceExchangeMode::NONE:
      tokens.emplace_back("dice_exchange=NONE");
      break;
    default:
      tokens.emplace_back(string_printf("dice_exchange=(%02hhX)",
          static_cast<uint8_t>(this->dice_exchange_mode)));
      break;
  }
  tokens.emplace_back(string_printf("dice_boost=%s", this->disable_dice_boost ? "DISABLED" : "ENABLED"));

  tokens.emplace_back(string_printf("deck_shuffle=%s", this->disable_deck_shuffle ? "DISABLED" : "ENABLED"));
  tokens.emplace_back(string_printf("deck_loop=%s", this->disable_deck_loop ? "DISABLED" : "ENABLED"));

  switch (this->allowed_cards) {
    case AllowedCards::ALL:
      tokens.emplace_back("allowed_cards=ALL");
      break;
    case AllowedCards::N_ONLY:
      tokens.emplace_back("allowed_cards=N_ONLY");
      break;
    case AllowedCards::N_R_ONLY:
      tokens.emplace_back("allowed_cards=N_R_ONLY");
      break;
    case AllowedCards::N_R_S_ONLY:
      tokens.emplace_back("allowed_cards=N_R_S_ONLY");
      break;
    default:
      tokens.emplace_back(string_printf("allowed_cards=(%02hhX)",
          static_cast<uint8_t>(this->allowed_cards)));
      break;
  }
  tokens.emplace_back(string_printf("assist_cards=%s", this->no_assist_cards ? "DISALLOWED" : "ALLOWED"));

  tokens.emplace_back(string_printf("time_limit=%zumin", static_cast<size_t>(this->overall_time_limit * 5)));
  tokens.emplace_back(string_printf("phase_time_limit=%hhusec", this->phase_time_limit));

  tokens.emplace_back(string_printf("dialogue=%s", this->disable_dialogue ? "DISABLED" : "ENABLED"));

  return "Rules[" + join(tokens, ", ") + "]";
}



StateFlags::StateFlags() {
  this->clear();
}

bool StateFlags::operator==(const StateFlags& other) const {
  return (this->turn_num == other.turn_num) &&
         (this->battle_phase == other.battle_phase) &&
         (this->current_team_turn1 == other.current_team_turn1) &&
         (this->current_team_turn2 == other.current_team_turn2) &&
         (this->action_subphase == other.action_subphase) &&
         (this->setup_phase == other.setup_phase) &&
         (this->registration_phase == other.registration_phase) &&
         (this->team_exp == other.team_exp) &&
         (this->team_dice_boost == other.team_dice_boost) &&
         (this->first_team_turn == other.first_team_turn) &&
         (this->tournament_flag == other.tournament_flag) &&
         (this->client_sc_card_types == other.client_sc_card_types);
}
bool StateFlags::operator!=(const StateFlags& other) const {
  return !this->operator==(other);
}

void StateFlags::clear() {
  this->turn_num = 0;
  this->battle_phase = BattlePhase::INVALID_00;
  this->current_team_turn1 = 0;
  this->current_team_turn2 = 0;
  this->action_subphase = ActionSubphase::ATTACK;
  this->setup_phase = SetupPhase::REGISTRATION;
  this->registration_phase = RegistrationPhase::AWAITING_NUM_PLAYERS;
  this->team_exp.clear(0);
  this->team_dice_boost.clear(0);
  this->first_team_turn = 0;
  this->tournament_flag = 0;
  this->client_sc_card_types.clear(CardType::HUNTERS_SC);
}

void StateFlags::clear_FF() {
  this->turn_num = 0xFFFF;
  this->battle_phase = BattlePhase::INVALID_FF;
  this->current_team_turn1 = 0xFF;
  this->current_team_turn2 = 0xFF;
  this->action_subphase = ActionSubphase::INVALID_FF;
  this->setup_phase = SetupPhase::INVALID_FF;
  this->registration_phase = RegistrationPhase::INVALID_FF;
  this->team_exp.clear(0xFFFFFFFF);
  this->team_dice_boost.clear(0xFF);
  this->first_team_turn = 0xFF;
  this->tournament_flag = 0xFF;
  this->client_sc_card_types.clear(CardType::INVALID_FF);
}



string MapDefinition::str(const DataIndex* data_index) const {
  deque<string> lines;
  auto add_map = [&](const parray<parray<uint8_t, 0x10>, 0x10>& tiles) {
    for (size_t y = 0; y < 0x10; y++) {
      string line = "   ";
      for (size_t x = 0; x < 0x10; x++) {
        line += string_printf(" %02hhX", tiles[y][x]);
      }
      lines.emplace_back(move(line));
    }
  };

  lines.emplace_back(string_printf("Map %08" PRIX32 ": %hhux%hhu",
      this->map_number.load(), this->width, this->height));
  lines.emplace_back(string_printf("  a1=%08" PRIX32, this->unknown_a1.load()));
  lines.emplace_back(string_printf("  environment_number=%02hhX", this->environment_number));
  lines.emplace_back(string_printf("  num_alt_maps=%02hhX", this->num_alt_maps));
  lines.emplace_back(string_printf("  num_alt_maps=%02hhX", this->num_alt_maps));
  lines.emplace_back("  tiles:");
  add_map(this->map_tiles);
  lines.emplace_back(string_printf("  start_tile_definitions=[%02hhX %02hhX %02hhX %02hhX %02hhX %02hhX], [%02hhX %02hhX %02hhX %02hhX %02hhX %02hhX]",
      this->start_tile_definitions[0][0], this->start_tile_definitions[0][1],
      this->start_tile_definitions[0][2], this->start_tile_definitions[0][3],
      this->start_tile_definitions[0][4], this->start_tile_definitions[0][5],
      this->start_tile_definitions[1][0], this->start_tile_definitions[1][1],
      this->start_tile_definitions[1][2], this->start_tile_definitions[1][3],
      this->start_tile_definitions[1][4], this->start_tile_definitions[1][5]));
  for (size_t z = 0; z < this->num_alt_maps; z++) {
    for (size_t w = 0; w < 2; w++) {
      lines.emplace_back(string_printf("  alt tiles %zu/%zu:", z, w));
      add_map(this->alt_maps1[w][z]);
    }
    for (size_t w = 0; w < 2; w++) {
      lines.emplace_back(string_printf("  alt tiles a3 %zu/%zu=%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g", z, w,
          this->alt_maps_unknown_a3[w][z][0x00].load(), this->alt_maps_unknown_a3[w][z][0x01].load(),
          this->alt_maps_unknown_a3[w][z][0x02].load(), this->alt_maps_unknown_a3[w][z][0x03].load(),
          this->alt_maps_unknown_a3[w][z][0x04].load(), this->alt_maps_unknown_a3[w][z][0x05].load(),
          this->alt_maps_unknown_a3[w][z][0x06].load(), this->alt_maps_unknown_a3[w][z][0x07].load(),
          this->alt_maps_unknown_a3[w][z][0x08].load(), this->alt_maps_unknown_a3[w][z][0x09].load(),
          this->alt_maps_unknown_a3[w][z][0x0A].load(), this->alt_maps_unknown_a3[w][z][0x0B].load(),
          this->alt_maps_unknown_a3[w][z][0x0C].load(), this->alt_maps_unknown_a3[w][z][0x0D].load(),
          this->alt_maps_unknown_a3[w][z][0x0E].load(), this->alt_maps_unknown_a3[w][z][0x0F].load(),
          this->alt_maps_unknown_a3[w][z][0x10].load(), this->alt_maps_unknown_a3[w][z][0x11].load()));
    }
  }
  for (size_t w = 0; w < 3; w++) {
    for (size_t z = 0; z < 0x24; z += 3) {
      lines.emplace_back(string_printf("  a5[%zu][0x%02zX:0x%02zX]=%g %g %g", w, z, z + 3,
          this->unknown_a5[w][z + 0].load(),
          this->unknown_a5[w][z + 1].load(),
          this->unknown_a5[w][z + 2].load()));
    }
  }
  lines.emplace_back("  modification tiles:");
  add_map(this->modification_tiles);
  for (size_t z = 0; z < 0x70; z += 0x10) {
    lines.emplace_back(string_printf("  a6[0x%02zX:0x%02zX]=%02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX", z, z + 0x10,
        this->unknown_a6[z + 0x00], this->unknown_a6[z + 0x01], this->unknown_a6[z + 0x02], this->unknown_a6[z + 0x03],
        this->unknown_a6[z + 0x04], this->unknown_a6[z + 0x05], this->unknown_a6[z + 0x06], this->unknown_a6[z + 0x07],
        this->unknown_a6[z + 0x08], this->unknown_a6[z + 0x09], this->unknown_a6[z + 0x0A], this->unknown_a6[z + 0x0B],
        this->unknown_a6[z + 0x0C], this->unknown_a6[z + 0x0D], this->unknown_a6[z + 0x0E], this->unknown_a6[z + 0x0F]));
  }
  lines.emplace_back(string_printf("  a6[0x70:0x74]=%02hhX %02hhX %02hhX %02hhX",
      this->unknown_a6[0x70], this->unknown_a6[0x71], this->unknown_a6[0x72], this->unknown_a6[0x73]));
  lines.emplace_back("  default_rules: " + this->default_rules.str());
  lines.emplace_back(string_printf("  a7=%02hhX %02hhX %02hhX %02hhX",
      this->unknown_a7[0], this->unknown_a6[1], this->unknown_a6[2], this->unknown_a6[3]));
  lines.emplace_back("  name: " + string(this->name));
  lines.emplace_back("  location_name: " + string(this->location_name));
  lines.emplace_back("  quest_name: " + string(this->quest_name));
  lines.emplace_back("  description: " + string(this->description));
  lines.emplace_back(string_printf("  map_xy: %hu %hu", this->map_x.load(), this->map_y.load()));
  for (size_t z = 0; z < 3; z++) {
    lines.emplace_back(string_printf("  npc_chars[%zu]:", z));
    lines.emplace_back(string_printf("    a1=%04hX %04hX",
        this->npc_chars[z].unknown_a1[0].load(), this->npc_chars[z].unknown_a1[1].load()));
    lines.emplace_back(string_printf("    a2=%02hX %02hX %02hX %02hX",
        this->npc_chars[z].unknown_a2[0], this->npc_chars[z].unknown_a2[1],
        this->npc_chars[z].unknown_a2[2], this->npc_chars[z].unknown_a2[3]));
    lines.emplace_back("    name: " + string(this->npc_chars[z].name));
    for (size_t w = 0; w < 0x78; w += 0x08) {
      lines.emplace_back(string_printf("    a3[0x%02zX:0x%02zX]=%04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX", z, z + 0x08,
          this->npc_chars[z].unknown_a3[w + 0x00].load(), this->npc_chars[z].unknown_a3[w + 0x01].load(),
          this->npc_chars[z].unknown_a3[w + 0x02].load(), this->npc_chars[z].unknown_a3[w + 0x03].load(),
          this->npc_chars[z].unknown_a3[w + 0x04].load(), this->npc_chars[z].unknown_a3[w + 0x05].load(),
          this->npc_chars[z].unknown_a3[w + 0x06].load(), this->npc_chars[z].unknown_a3[w + 0x07].load()));
    }
    lines.emplace_back(string_printf("    a3[0x78:0x7E]=%04hX %04hX %04hX %04hX %04hX %04hX",
        this->npc_chars[z].unknown_a3[0x78].load(), this->npc_chars[z].unknown_a3[0x79].load(),
        this->npc_chars[z].unknown_a3[0x7A].load(), this->npc_chars[z].unknown_a3[0x7B].load(),
        this->npc_chars[z].unknown_a3[0x7C].load(), this->npc_chars[z].unknown_a3[0x7D].load()));
    lines.emplace_back(string_printf("  npc_decks[%zu]:", z));
    lines.emplace_back("    name: " + string(this->npc_decks[z].name));
    for (size_t w = 0; w < 0x20; w++) {
      uint16_t card_id = this->npc_decks[z].card_ids[w];
      shared_ptr<const DataIndex::CardEntry> entry;
      if (data_index) {
        try {
          entry = data_index->definition_for_card_id(card_id);
        } catch (const out_of_range&) { }
      }
      if (entry) {
        string name = entry->def.en_name;
        lines.emplace_back(string_printf("    cards[%02zu]: %04hX (%s)", w, card_id, name.c_str()));
      } else {
        lines.emplace_back(string_printf("    cards[%02zu]: %04hX", w, card_id));
      }
    }
    for (size_t x = 0; x < 0x10; x++) {
      lines.emplace_back(string_printf("  npc_dialogue[%zu][%zu]:", z, x));
      lines.emplace_back(string_printf("    a1=%04hX", this->dialogue_sets[z][x].unknown_a1.load()));
      lines.emplace_back(string_printf("    a2=%04hX", this->dialogue_sets[z][x].unknown_a2.load()));
      for (size_t w = 0; w < 4; w++) {
        if (this->dialogue_sets[z][x].strings[w][0] != 0 &&
            static_cast<uint8_t>(this->dialogue_sets[z][x].strings[w][0]) != 0xFF) {
          lines.emplace_back(string_printf("    strings[%zu]=", w) + string(this->dialogue_sets[z][x].strings[w]));
        }
      }
    }
  }
  lines.emplace_back("  a8=" + format_data_string(this->unknown_a8.data(), this->unknown_a8.bytes()));
  if (this->before_message[0]) {
    lines.emplace_back("  before_message: " + string(this->before_message));
  }
  if (this->after_message[0]) {
    lines.emplace_back("  after_message: " + string(this->after_message));
  }
  if (this->dispatch_message[0]) {
    lines.emplace_back("  dispatch_message: " + string(this->dispatch_message));
  }
  for (size_t z = 0; z < 0x10; z++) {
    uint16_t card_id = this->reward_card_ids[z];
    shared_ptr<const DataIndex::CardEntry> entry;
    if (data_index) {
      try {
        entry = data_index->definition_for_card_id(card_id);
      } catch (const out_of_range&) { }
    }
    if (entry) {
      string name = entry->def.en_name;
      lines.emplace_back(string_printf("  reward_cards[%02zu]: %04hX (%s)", z, card_id, name.c_str()));
    } else {
      lines.emplace_back(string_printf("  reward_cards[%02zu]: %04hX", z, card_id));
    }
  }
  lines.emplace_back("  a9=" + format_data_string(this->unknown_a9.data(), this->unknown_a9.bytes()));
  lines.emplace_back("  a11=" + format_data_string(this->unknown_a11.data(), this->unknown_a11.bytes()));
  return join(lines, "\n");
}

bool Rules::check_invalid_fields() const {
  Rules t = *this;
  return t.check_and_reset_invalid_fields();
}

bool Rules::check_and_reset_invalid_fields() {
  bool ret = false;
  if (this->overall_time_limit > 36) {
    this->overall_time_limit = 6;
    ret = true;
  }
  if (this->phase_time_limit > 120) {
    this->phase_time_limit = 60;
    ret = true;
  }
  if (static_cast<uint8_t>(this->allowed_cards) > 3) {
    this->allowed_cards = AllowedCards::ALL;
    ret = true;
  }
  if (this->min_dice > 9) {
    this->min_dice = 0;
    ret = true;
  }
  if (this->max_dice > 9) {
    this->max_dice = 0;
    ret = true;
  }
  if ((this->min_dice != 0) && (this->max_dice != 0) && (this->max_dice < this->min_dice)) {
    uint8_t t = this->min_dice;
    this->min_dice = this->max_dice;
    this->max_dice = t;
    ret = true;
  }
  if (this->disable_deck_shuffle > 1) {
    this->disable_deck_shuffle = 0;
    ret = true;
  }
  if (this->disable_deck_loop > 1) {
    this->disable_deck_loop = 0;
    ret = true;
  }
  if (this->char_hp > 99) {
    this->char_hp = 0;
    ret = true;
  }
  if (static_cast<uint8_t>(this->hp_type) > 2) {
    this->hp_type = HPType::DEFEAT_PLAYER;
    ret = true;
  }
  if (this->no_assist_cards > 1) {
    this->no_assist_cards = 0;
    ret = true;
  }
  if (static_cast<uint8_t>(this->dice_exchange_mode) > 2) {
    this->dice_exchange_mode = DiceExchangeMode::HIGH_ATK;
    ret = true;
  }
  if (this->disable_dice_boost > 1) {
    this->disable_dice_boost = 0;
    ret = true;
  }
  if ((this->max_dice != 0) && (this->max_dice < 3)) {
    this->disable_dice_boost = 1;
    ret = true;
  }
  return ret;
}



DataIndex::DataIndex(const string& directory, uint32_t behavior_flags)
  : behavior_flags(behavior_flags) {

  unordered_map<uint32_t, vector<string>> card_tags;
  unordered_map<uint32_t, string> card_text;
  if (this->behavior_flags & BehaviorFlag::LOAD_CARD_TEXT) {
    try {
      string data = prs_decompress(load_file(directory + "/card-text.mnr"));
      StringReader r(data);

      while (!r.eof()) {
        string card_id_str = r.get_cstr();
        if (card_id_str.empty() || (static_cast<uint8_t>(card_id_str[0]) == 0xFF)) {
          break;
        }
        strip_leading_whitespace(card_id_str);
        uint32_t card_id = stoul(card_id_str);

        // Read all pages for this card
        string text;
        string first_page;
        for (;;) {
          string line = r.get_cstr();
          if (line.empty()) {
            break;
          }
          if (first_page.empty()) {
            first_page = line;
          }
          text += '\n';
          text += line;
        }

        // In orig_text, turn all \t into $ (following newserv conventions)
        string orig_text = text;
        for (char& ch : orig_text) {
          if (ch == '\t') {
            ch = '$';
          }
        }

        // Preprocess first page: first, delete all color markers
        size_t offset = first_page.find("\tC");
        while (offset != string::npos) {
          first_page = first_page.substr(0, offset) + first_page.substr(offset + 3);
          offset = first_page.find("\tC");
        }
        // Preprocess first page: delete all lines that don't start with \t
        offset = first_page.find('\t');
        if (offset == string::npos) {
          first_page.clear();
        } else {
          first_page = first_page.substr(offset);
        }
        // Preprocess first page: merge lines that don't begin with \t
        for (offset = 0; offset < first_page.size(); offset++) {
          if (first_page[offset] == '\n' && first_page[offset + 1] != '\t') {
            first_page = first_page.substr(0, offset) + first_page.substr(offset + 1);
            offset--;
          }
        }

        // Split first page into tags, and collapse whitespace in the tag names
        vector<string> tags;
        auto lines = split(first_page, '\n');
        for (const auto& line : lines) {
          string tag;
          if (line[0] == '\t' && line[1] == 'D') {
            tag = "D: " + line.substr(2);
          } else if (line[0] == '\t' && line[1] == 'S') {
            tag = "S: " + line.substr(2);
          }
          if (!tag.empty()) {
            for (size_t offset = tag.find("  "); offset != string::npos; offset = tag.find("  ")) {
              tag = tag.substr(0, offset) + tag.substr(offset + 1);
            }
            tags.emplace_back(move(tag));
          }
        }

        if (!card_text.emplace(card_id, move(orig_text)).second) {
          throw runtime_error("duplicate card text id");
        }
        if (!card_tags.emplace(card_id, move(tags)).second) {
          throw logic_error("duplicate card tags id");
        }

        r.go((r.where() + 0x3FF) & (~0x3FF));
      }

    } catch (const exception& e) {
      static_game_data_log.warning("Failed to load card text: %s", e.what());
    }
  }

  try {
    string decompressed_data;
    if (isfile(directory + "/card-definitions.mnrd")) {
      decompressed_data = load_file(directory + "/card-definitions.mnrd");
      this->compressed_card_definitions.clear();
    } else {
      this->compressed_card_definitions = load_file(directory + "/card-definitions.mnr");
      decompressed_data = prs_decompress(this->compressed_card_definitions);
    }
    if (decompressed_data.size() > 0x36EC0) {
      throw runtime_error("decompressed card list data is too long");
    }

    // There's a footer after the card definitions, but we ignore it
    if (decompressed_data.size() % sizeof(CardDefinition) != sizeof(CardDefinitionsFooter)) {
      throw runtime_error(string_printf(
          "decompressed card update file size %zX is not aligned with card definition size %zX (%zX extra bytes)",
          decompressed_data.size(), sizeof(CardDefinition), decompressed_data.size() % sizeof(CardDefinition)));
    }
    auto* defs = reinterpret_cast<CardDefinition*>(decompressed_data.data());
    size_t max_cards = decompressed_data.size() / sizeof(CardDefinition);
    for (size_t x = 0; x < max_cards; x++) {
      // The last card entry has the build date and some other metadata (and
      // isn't a real card, obviously), so skip it. Seems like the card ID is
      // always a large number that won't fit in a uint16_t, so we use that to
      // determine if the entry is a real card or not.
      if (defs[x].card_id & 0xFFFF0000) {
        continue;
      }

      shared_ptr<CardEntry> entry(new CardEntry({defs[x], {}, {}}));
      if (!this->card_definitions.emplace(entry->def.card_id, entry).second) {
        throw runtime_error(string_printf(
            "duplicate card id: %08" PRIX32, entry->def.card_id.load()));
      }

      // Some cards intentionally have the same name, so we just leave them
      // unindexed (they can still be looked up by ID, of course)
      string name = entry->def.en_name;
      this->card_definitions_by_name.emplace(name, entry);

      entry->def.hp.decode_code();
      entry->def.ap.decode_code();
      entry->def.tp.decode_code();
      entry->def.mv.decode_code();
      entry->def.decode_range();

      if (this->behavior_flags & BehaviorFlag::LOAD_CARD_TEXT) {
        try {
          entry->text = move(card_text.at(defs[x].card_id));
        } catch (const out_of_range&) { }
        try {
          entry->debug_tags = move(card_tags.at(defs[x].card_id));
        } catch (const out_of_range&) { }
      }
    }

    if (this->compressed_card_definitions.empty()) {
      uint64_t start = now();
      this->compressed_card_definitions = prs_compress(decompressed_data);
      uint64_t diff = now() - start;
      static_game_data_log.info(
          "Compressed card definitions (%zu bytes -> %zu bytes) in %" PRIu64 "ms",
          decompressed_data.size(), this->compressed_card_definitions.size(), diff);
    }
    if (this->compressed_card_definitions.size() > 0x7BF8) {
      throw runtime_error("compressed card list data is too long");
    }

    static_game_data_log.info("Indexed %zu Episode 3 card definitions", this->card_definitions.size());
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load Episode 3 card update: %s", e.what());
  }

  auto add_maps_from_dir = [&](const string& dir, bool is_quest) -> void {
    for (const auto& filename : list_directory(dir)) {
      try {
        shared_ptr<MapEntry> entry;

        if (ends_with(filename, ".mnmd")) {
          entry.reset(new MapEntry(load_object_file<MapDefinition>(dir + "/" + filename), is_quest));
        } else if (ends_with(filename, ".mnm")) {
          entry.reset(new MapEntry(load_file(dir + "/" + filename), is_quest));
        }

        if (entry.get()) {
          if (!this->maps.emplace(entry->map.map_number, entry).second) {
            throw runtime_error("duplicate map number");
          }
          this->maps_by_name.emplace(entry->map.name, entry);
          string name = entry->map.name;
          static_game_data_log.info("Indexed Episode 3 %s %s (%08" PRIX32 "; %s)",
              is_quest ? "online quest" : "free battle map",
              filename.c_str(), entry->map.map_number.load(), name.c_str());
        }

      } catch (const exception& e) {
        static_game_data_log.warning("Failed to index Episode 3 map %s: %s",
            filename.c_str(), e.what());
      }
    }
  };
  add_maps_from_dir(directory + "/maps-free", false);
  add_maps_from_dir(directory + "/maps-quest", true);

  try {
    auto json = JSONObject::parse(load_file(directory + "/com-decks.json"));
    for (const auto& def_json : json->as_list()) {
      auto& def = this->com_decks.emplace_back(new COMDeckDefinition());
      def->index = this->com_decks.size() - 1;
      def->player_name = def_json->at(0)->as_string();
      def->deck_name = def_json->at(1)->as_string();
      auto card_ids_json = def_json->at(2)->as_list();
      for (size_t z = 0; z < 0x1F; z++) {
        def->card_ids[z] = card_ids_json.at(z)->as_int();
      }
      if (!this->com_decks_by_name.emplace(def->deck_name, def).second) {
        throw runtime_error("duplicate COM deck name: " + def->deck_name);
      }
    }
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load Episode 3 COM decks: %s", e.what());
  }
}

DataIndex::MapEntry::MapEntry(const MapDefinition& map, bool is_quest)
  : map(map), is_quest(is_quest) { }

DataIndex::MapEntry::MapEntry(const string& compressed, bool is_quest)
  : is_quest(is_quest), compressed_data(compressed) {
  string decompressed = prs_decompress(this->compressed_data);
  if (decompressed.size() != sizeof(MapDefinition)) {
    throw runtime_error(string_printf(
        "decompressed data size is incorrect (expected %zu bytes, read %zu bytes)",
        sizeof(MapDefinition), decompressed.size()));
  }
  this->map = *reinterpret_cast<const MapDefinition*>(decompressed.data());
}

string DataIndex::MapEntry::compressed() const {
  if (this->compressed_data.empty()) {
    this->compressed_data = prs_compress(&this->map, sizeof(this->map));
  }
  return this->compressed_data;
}

const string& DataIndex::get_compressed_card_definitions() const {
  if (this->compressed_card_definitions.empty()) {
    throw runtime_error("card definitions are not available");
  }
  return this->compressed_card_definitions;
}

shared_ptr<const DataIndex::CardEntry> DataIndex::definition_for_card_id(
    uint32_t id) const {
  return this->card_definitions.at(id);
}

shared_ptr<const DataIndex::CardEntry> DataIndex::definition_for_card_name(
    const string& name) const {
  return this->card_definitions_by_name.at(name);
}

set<uint32_t> DataIndex::all_card_ids() const {
  set<uint32_t> ret;
  for (const auto& it : this->card_definitions) {
    ret.emplace(it.first);
  }
  return ret;
}

const string& DataIndex::get_compressed_map_list() const {
  if (this->compressed_map_list.empty()) {
    // TODO: Write a version of prs_compress that takes iovecs (or something
    // similar) so we can eliminate all this string copying here.
    StringWriter entries_w;
    StringWriter strings_w;

    for (const auto& map_it : this->maps) {
      MapList::Entry e;
      const auto& map = map_it.second->map;
      e.map_x = map.map_x;
      e.map_y = map.map_y;
      e.environment_number = map.environment_number;
      e.map_number = map.map_number.load();
      e.width = map.width;
      e.height = map.height;
      e.map_tiles = map.map_tiles;
      e.modification_tiles = map.modification_tiles;

      e.name_offset = strings_w.size();
      strings_w.write(map.name.data(), map.name.len());
      strings_w.put_u8(0);
      e.location_name_offset = strings_w.size();
      strings_w.write(map.location_name.data(), map.location_name.len());
      strings_w.put_u8(0);
      e.quest_name_offset = strings_w.size();
      strings_w.write(map.quest_name.data(), map.quest_name.len());
      strings_w.put_u8(0);
      e.description_offset = strings_w.size();
      strings_w.write(map.description.data(), map.description.len());
      strings_w.put_u8(0);

      e.unknown_a2 = map_it.second->is_quest ? 0x00000000 : 0xFF000000;

      entries_w.put(e);
    }

    MapList header;
    header.num_maps = this->maps.size();
    header.unknown_a1 = 0;
    header.strings_offset = entries_w.size();
    header.total_size = sizeof(MapList) + entries_w.size() + strings_w.size();

    PRSCompressor prs;
    prs.add(&header, sizeof(header));
    prs.add(entries_w.str());
    prs.add(strings_w.str());

    StringWriter compressed_w;
    compressed_w.put_u32b(prs.input_size());
    compressed_w.write(prs.close());
    this->compressed_map_list = move(compressed_w.str());
    static_game_data_log.info("Generated Episode 3 compressed map list (%zu -> %zu bytes)",
        this->compressed_map_list.size(), this->compressed_map_list.size());
  }
  return this->compressed_map_list;
}

shared_ptr<const DataIndex::MapEntry> DataIndex::definition_for_map_number(uint32_t id) const {
  return this->maps.at(id);
}

shared_ptr<const DataIndex::MapEntry> DataIndex::definition_for_map_name(
    const string& name) const {
  return this->maps_by_name.at(name);
}

set<uint32_t> DataIndex::all_map_ids() const {
  set<uint32_t> ret;
  for (const auto& it : this->maps) {
    ret.emplace(it.first);
  }
  return ret;
}

size_t DataIndex::num_com_decks() const {
  return this->com_decks.size();
}

shared_ptr<const COMDeckDefinition> DataIndex::com_deck(size_t which) const {
  return this->com_decks.at(which);
}

shared_ptr<const COMDeckDefinition> DataIndex::com_deck(const string& which) const {
  return this->com_decks_by_name.at(which);
}

shared_ptr<const COMDeckDefinition> DataIndex::random_com_deck() const {
  return this->com_decks[random_object<size_t>() % this->com_decks.size()];
}



} // namespace Episode3
