#include "DataIndexes.hh"

#include <stdint.h>

#include <array>
#include <deque>
#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>

#include "../CommonFileFormats.hh"
#include "../Compression.hh"
#include "../Loggers.hh"
#include "../PSOEncryption.hh"
#include "../Quest.hh"
#include "../Text.hh"

using namespace std;

namespace Episode3 {

const char* name_for_environment_number(uint8_t environment_number) {
  static constexpr array<const char*, 0x1C> names = {
      "Unguis Lapis",
      "Nebula Montana 1",
      "Lupus Silva 1",
      "Lupus Silva 2",
      "Molae Venti",
      "Nebula Montana 2",
      "Tener Sinus",
      "Mortis Fons",
      "Morgue (destroyed)",
      "Tower of Caelum",
      "MapMan",
      "Cyber",
      "Morgue (not destroyed)",
      "Castor/Pollux map",
      "Dolor Odor",
      "Ravum Aedes Sacra",
      "Amplum Umbra map",
      "Via Tubus",
      "Morgue",
      "TCardDemo",
      "unknown",
      "unknown",
      "Battle Results",
      "Game Over",
      "Staff roll",
      "View Battle waiting room",
      "TCard00_Select",
      "blank",
  };
  if (environment_number >= names.size()) {
    return "unknown";
  }
  return names[environment_number];
}

const char* name_for_link_color(uint8_t color) {
  switch (color) {
    case 1:
      return "blue"; // HP halver
    case 2:
      return "red"; // Physical attacks
    case 3:
      return "yellow"; // Techniques
    case 4:
      return "brown"; // Leukon Knight
    case 5:
      return "orange"; // Penetrate/confuse
    case 6:
      return "purple"; // Instant death
    case 7:
      return "white"; // Castor
    case 8:
      return "gray"; // Pollux
    case 9:
      return "green"; // Status effects
    default:
      throw invalid_argument("unknown color");
  }
}

phosg::JSON json_for_link_colors(const parray<uint8_t, 8>& colors) {
  phosg::JSON ret = phosg::JSON::list();
  for (size_t z = 0; z < colors.size(); z++) {
    if (colors[z]) {
      ret.emplace_back(name_for_link_color(colors[z]));
    }
  }
  return ret;
}

Location::Location() : Location(0, 0) {}
Location::Location(uint8_t x, uint8_t y) : Location(x, y, Direction::RIGHT) {}
Location::Location(uint8_t x, uint8_t y, Direction direction)
    : x(x),
      y(y),
      direction(direction),
      unused(0) {}

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
  return phosg::string_printf("Location[x=%hhu, y=%hhu, dir=%hhu:%s, u=%hhu]",
      this->x, this->y, static_cast<uint8_t>(this->direction), phosg::name_for_enum(this->direction), this->unused);
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

bool card_class_is_tech_like(CardClass cc, bool is_nte) {
  // NTE does not consider BOSS_TECH to be a tech-like card class, but that's
  // probably because that card class just doesn't exist on NTE.
  if (is_nte) {
    return (cc == CardClass::TECH) || (cc == CardClass::PHOTON_BLAST);
  } else {
    return (cc == CardClass::TECH) || (cc == CardClass::PHOTON_BLAST) || (cc == CardClass::BOSS_TECH);
  }
}

static const unordered_map<string, const char*> description_for_expr_token({
    {"f", "Number of FCs controlled by current SC"},
    {"d", "Die roll"},
    {"ap", "Attacker effective AP"},
    {"tp", "Attacker effective TP"},
    {"hp", "Current HP"},
    {"mhp", "Maximum HP"},
    {"dm", "Physical damage"},
    {"tdm", "Technique damage"},
    {"tf", "Number of SC\'s destroyed FCs"},
    {"ac", "Remaining ATK points"},
    {"php", "Maximum HP"},
    {"dc", "Die roll"},
    {"cs", "Card set cost"},
    {"a", "Number of FCs on all teams"},
    {"kap", "Action cards AP"},
    {"ktp", "Action cards TP"},
    {"dn", "Unknown: dn"},
    {"hf", "Number of item or creature cards in hand"},
    {"df", "Number of destroyed ally FCs (including SC\'s own)"},
    {"ff", "Number of ally FCs (including SC\'s own)"},
    {"ef", "Number of enemy FCs"},
    {"bi", "Number of Native FCs on either team"},
    {"ab", "Number of A.Beast FCs on either team"},
    {"mc", "Number of Machine FCs on either team"},
    {"dk", "Number of Dark FCs on either team"},
    {"sa", "Number of Sword-type items on either team"},
    {"gn", "Number of Gun-type items on either team"},
    {"wd", "Number of Cane-type items on either team"},
    {"tt", "Physical damage"},
    {"lv", "Dice boost"},
    {"adm", "SC attack damage"},
    {"ddm", "Attack bonus"},
    {"sat", "Number of Sword-type items on SC\'s team"},
    {"edm", "Target attack bonus"},
    {"ldm", "Last attack damage before defense"}, // Unused
    {"rdm", "Last attack damage"},
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
    /* n07 */ "Altered Beast attribute",
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
    /* n21 */ "Target is affected by Pierce",
    /* n22 */ "Target is affected by Rampage",
});

static const vector<const char*> description_for_p_target({
    /* p00 */ "(Invalid)",
    /* p01 */ "SC / FC who set the card",
    /* p02 */ "Attacking SC / FC",
    /* p03 */ "All item FCs from both teams within attack range", // Unused
    /* p04 */ "All action cards in the chain after this one", // Unused
    /* p05 */ "SC / FC who set the card", // Identical to p01
    /* p06 */ "Attacking card, or SC if attacking card is an item",
    /* p07 */ "Attacking card",
    /* p08 */ "FC\'s owner SC",
    /* p09 */ "All cards from both teams within attack range", // Unused
    /* p10 */ "All ally SCs and FCs",
    /* p11 */ "All ally FCs",
    /* p12 */ "All non-aerial FCs on both teams",
    /* p13 */ "All FCs on both teams that are Frozen",
    /* p14 */ "All FCs on both teams with <= 3 HP",
    /* p15 */ "All FCs on both teams",
    /* p16 */ "All FCs on both teams with >= 8 HP",
    /* p17 */ "This card",
    /* p18 */ "SC who equipped this card",
    /* p19 */ "All HU-class SCs", // Unused
    /* p20 */ "All RA-class SCs", // Unused
    /* p21 */ "All FO-class SCs", // Unused
    /* p22 */ "All characters (SCs & FCs) including this card", // TODO: But why does Shifta apply only to allies then?
    /* p23 */ "All characters (SCs & FCs) except this card",
    /* p24 */ "All FCs on both teams that have Paralysis",
    /* p25 */ "All aerial SCs and FCs", // Unused
    /* p26 */ "All cards not at maximum HP", // Unused
    /* p27 */ "All Native creatures", // Unused
    /* p28 */ "All Altered Beast creatures", // Unused
    /* p29 */ "All Machine creatures", // Unused
    /* p30 */ "All Dark creatures", // Unused
    /* p31 */ "All Sword-type items", // Unused
    /* p32 */ "All Gun-type items", // Unused
    /* p33 */ "All Cane-type items", // Unused
    /* p34 */ "All non-SC targets", // Unused
    /* p35 */ "All characters (SCs & FCs) within range", // Used for Explosion effect
    /* p36 */ "All ally SCs within range, but not the caster", // Resta
    /* p37 */ "All opponent FCs",
    /* p38 */ "All allies except items within range (and not this card)",
    /* p39 */ "All FCs that cost 4 or more points",
    /* p40 */ "All FCs that cost 3 or fewer points",
    /* p41 */ "All FCs next to attacker, and all of attacker\'s equipped items", // Unused
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
    /* 0x00 */ {false, "NONE", nullptr},
    /* 0x01 */ {true, "AP_BOOST", "Temporarily increase AP"},
    /* 0x02 */ {false, "RAMPAGE", "Rampage"},
    /* 0x03 */ {true, "MULTI_STRIKE", "Duplicate attack N times"},
    /* 0x04 */ {true, "DAMAGE_MODIFIER_1", "Set attack damage / AP to N after action cards applied (step 1)"},
    /* 0x05 */ {false, "IMMOBILE", "Give Immobile condition"},
    /* 0x06 */ {false, "HOLD", "Give Hold condition"},
    /* 0x07 */ {false, "CANNOT_DEFEND", "Cannot defend"},
    /* 0x08 */ {true, "TP_BOOST", "Add N TP temporarily during attack"},
    /* 0x09 */ {true, "GIVE_DAMAGE", "Cause direct N HP loss"},
    /* 0x0A */ {false, "GUOM", "Give Guom condition"},
    /* 0x0B */ {false, "PARALYZE", "Give Paralysis condition"},
    /* 0x0C */ {false, "A_T_SWAP_0C", "Swap AP and TP temporarily"},
    /* 0x0D */ {false, "A_H_SWAP", "Swap AP and HP temporarily"},
    /* 0x0E */ {false, "PIERCE", "Attack SC directly even if they have items equipped"},
    /* 0x0F */ {false, "UNUSED_0F", nullptr},
    /* 0x10 */ {true, "HEAL", "Increase HP"},
    /* 0x11 */ {false, "RETURN_TO_HAND", "Return card to hand"},
    /* 0x12 */ {false, "SET_MV_COST_TO_0", "Movement costs nothing"},
    /* 0x13 */ {false, "UNUSED_13", nullptr},
    /* 0x14 */ {false, "ACID", "Give Acid condition"},
    /* 0x15 */ {false, "ADD_1_TO_MV_COST", "Add 1 to move costs"},
    /* 0x16 */ {true, "MIGHTY_KNUCKLE", "Temporarily increase AP, and set ATK dice to zero"},
    /* 0x17 */ {true, "UNIT_BLOW", "Temporarily increase AP by (expr) * number of this card set within phase"},
    /* 0x18 */ {false, "CURSE", "Give Curse condition"},
    /* 0x19 */ {false, "COMBO_AP", "Temporarily increase AP by number of this card set within phase"},
    /* 0x1A */ {false, "PIERCE_RAMPAGE_BLOCK", "Block attack if Pierce/Rampage (?)"},
    /* 0x1B */ {false, "ABILITY_TRAP", "Temporarily disable opponent abilities"},
    /* 0x1C */ {false, "FREEZE", "Give Freeze condition"},
    /* 0x1D */ {false, "ANTI_ABNORMALITY_1", "Cure all conditions"},
    /* 0x1E */ {false, "UNKNOWN_1E", nullptr},
    /* 0x1F */ {false, "EXPLOSION", "Damage all SCs and FCs by number of this same card set * 2"},
    /* 0x20 */ {false, "UNKNOWN_20", nullptr},
    /* 0x21 */ {false, "UNKNOWN_21", nullptr},
    /* 0x22 */ {false, "UNKNOWN_22", nullptr},
    /* 0x23 */ {false, "RETURN_TO_DECK", "Cancel discard and move to bottom of deck instead"},
    /* 0x24 */ {false, "AERIAL", "Give Aerial status"},
    /* 0x25 */ {true, "AP_LOSS", "Make attacker temporarily lose N AP during defense"},
    /* 0x26 */ {true, "BONUS_FROM_LEADER", "Gain AP equal to the number of cards of type N on the field"},
    /* 0x27 */ {false, "FREE_MANEUVER", "Enable movement over occupied tiles"},
    /* 0x28 */ {false, "SCALE_MV_COST", "Multiply movement costs by a factor"},
    /* 0x29 */ {true, "CLONE", "Make setting this card free if at least one card of type N is already on the field"},
    /* 0x2A */ {true, "DEF_DISABLE_BY_COST", "Disable use of any defense cards costing between (N / 10) and (N % 10) points, inclusive"},
    /* 0x2B */ {true, "FILIAL", "Increase controlling SC\'s HP when this card is destroyed"},
    /* 0x2C */ {true, "SNATCH", "Steal N EXP during attack"},
    /* 0x2D */ {true, "HAND_DISRUPTER", "Discard N cards from hand immediately"},
    /* 0x2E */ {false, "DROP", "Give Drop condition"},
    /* 0x2F */ {false, "ACTION_DISRUPTER", "Destroy all action cards used by attacker"},
    /* 0x30 */ {true, "SET_HP", "Set HP to N"},
    /* 0x31 */ {false, "NATIVE_SHIELD", "Block attacks from Native creatures"},
    /* 0x32 */ {false, "A_BEAST_SHIELD", "Block attacks from A.Beast creatures"},
    /* 0x33 */ {false, "MACHINE_SHIELD", "Block attacks from Machine creatures"},
    /* 0x34 */ {false, "DARK_SHIELD", "Block attacks from Dark creatures"},
    /* 0x35 */ {false, "SWORD_SHIELD", "Block attacks from Sword items"},
    /* 0x36 */ {false, "GUN_SHIELD", "Block attacks from Gun items"},
    /* 0x37 */ {false, "CANE_SHIELD", "Block attacks from Cane items"},
    /* 0x38 */ {false, "UNKNOWN_38", nullptr},
    /* 0x39 */ {false, "UNKNOWN_39", nullptr},
    /* 0x3A */ {false, "DEFENDER", "Make attacks go to setter of this card instead of original target"},
    /* 0x3B */ {false, "SURVIVAL_DECOYS", "Redirect damage for multi-sided attack"},
    /* 0x3C */ {true, "GIVE_OR_TAKE_EXP", "Give N EXP, or take if N is negative"},
    /* 0x3D */ {false, "UNKNOWN_3D", nullptr},
    /* 0x3E */ {false, "DEATH_COMPANION", "If this card has 1 or 2 HP, set its HP to N"},
    /* 0x3F */ {true, "EXP_DECOY", "If defender has EXP, lose EXP instead of getting damage when attacked"},
    /* 0x40 */ {true, "SET_MV", "Set MV to N"},
    /* 0x41 */ {true, "GROUP", "Temporarily increase AP by (expr) * number of this card on field, excluding itself"},
    /* 0x42 */ {false, "BERSERK", "User of this card receives the same damage as target, and isn\'t helped by target\'s defense cards"},
    /* 0x43 */ {false, "GUARD_CREATURE", "Attacks on controlling SC damage this card instead"},
    /* 0x44 */ {false, "TECH", "Technique cards cost 1 fewer ATK point"},
    /* 0x45 */ {false, "BIG_SWING", "Increase all attacking ATK costs by 1"},
    /* 0x46 */ {false, "UNKNOWN_46", nullptr},
    /* 0x47 */ {false, "SHIELD_WEAPON", "Limit attacker\'s choice of target to guard items"},
    /* 0x48 */ {false, "ATK_DICE_BOOST", "Increase ATK dice roll by 1"},
    /* 0x49 */ {false, "UNKNOWN_49", nullptr},
    /* 0x4A */ {false, "MAJOR_PIERCE", "If SC has over half of max HP, attacks target SC instead of equipped items"},
    /* 0x4B */ {false, "HEAVY_PIERCE", "If SC has 3 or more items equipped, attacks target SC instead of equipped items"},
    /* 0x4C */ {false, "MAJOR_RAMPAGE", "If SC has over half of max HP, attacks target SC and all equipped items"},
    /* 0x4D */ {false, "HEAVY_RAMPAGE", "If SC has 3 or more items equipped, attacks target SC and all equipped items"},
    /* 0x4E */ {true, "AP_GROWTH", "Permanently increase AP"},
    /* 0x4F */ {true, "TP_GROWTH", "Permanently increase TP"},
    /* 0x50 */ {true, "REBORN", "If any card of type N is on the field, this card goes to the hand when destroyed instead of being discarded"},
    /* 0x51 */ {true, "COPY", "Temporarily set AP/TP to N percent (or 100% if N is 0) of opponent\'s values"},
    /* 0x52 */ {false, "UNKNOWN_52", nullptr},
    /* 0x53 */ {true, "MISC_GUARDS", "Add N to card\'s defense value"},
    /* 0x54 */ {true, "AP_OVERRIDE", "Set AP to N temporarily"},
    /* 0x55 */ {true, "TP_OVERRIDE", "Set TP to N temporarily"},
    /* 0x56 */ {false, "RETURN", "Return card to hand on destruction instead of discarding"},
    /* 0x57 */ {false, "A_T_SWAP_PERM", "Permanently swap AP and TP"},
    /* 0x58 */ {false, "A_H_SWAP_PERM", "Permanently swap AP and HP"},
    /* 0x59 */ {true, "SLAYERS_ASSASSINS", "Temporarily increase AP during attack"},
    /* 0x5A */ {false, "ANTI_ABNORMALITY_2", "Remove all conditions"},
    /* 0x5B */ {false, "FIXED_RANGE", "Use SC\'s range instead of weapon or attack card ranges"},
    /* 0x5C */ {false, "ELUDE", "SC does not lose HP when equipped items are destroyed"},
    /* 0x5D */ {false, "PARRY", "Forward attack to a random FC within one tile of original target, excluding attacker and original target"},
    /* 0x5E */ {false, "BLOCK_ATTACK", "Completely block attack"},
    /* 0x5F */ {false, "UNKNOWN_5F", nullptr},
    /* 0x60 */ {false, "UNKNOWN_60", nullptr},
    /* 0x61 */ {true, "COMBO_TP", "Gain TP equal to the number of cards of type N on the field"},
    /* 0x62 */ {true, "MISC_AP_BONUSES", "Temporarily increase AP"},
    /* 0x63 */ {true, "MISC_TP_BONUSES", "Temporarily increase TP"},
    /* 0x64 */ {false, "UNKNOWN_64", nullptr},
    /* 0x65 */ {true, "MISC_DEFENSE_BONUSES", "Decrease damage"},
    /* 0x66 */ {true, "MOSTLY_HALFGUARDS", "Reduce damage from incoming attack"},
    /* 0x67 */ {false, "PERIODIC_FIELD", "Swap immunity to tech or physical attacks"},
    /* 0x68 */ {false, "FC_LIMIT_BY_COUNT", "Change FC limit from 8 ATK points total to 4 FCs total"},
    /* 0x69 */ {false, "UNKNOWN_69", nullptr},
    /* 0x6A */ {true, "MV_BONUS", "Increase MV"},
    /* 0x6B */ {true, "FORWARD_DAMAGE", "Give N damage back to attacker during defense (?) (TODO)"},
    /* 0x6C */ {true, "WEAK_SPOT_INFLUENCE", "Temporarily decrease AP"},
    /* 0x6D */ {true, "DAMAGE_MODIFIER_2", "Set attack damage / AP after action cards applied (step 2)"},
    /* 0x6E */ {true, "WEAK_HIT_BLOCK", "Block all attacks of N damage or less"},
    /* 0x6F */ {true, "AP_SILENCE", "Temporarily decrease AP of opponent"},
    /* 0x70 */ {true, "TP_SILENCE", "Temporarily decrease TP of opponent"},
    /* 0x71 */ {false, "A_T_SWAP", "Temporarily swap AP and TP"},
    /* 0x72 */ {true, "HALFGUARD", "Halve damage from attacks that would inflict N or more damage"},
    /* 0x73 */ {false, "UNKNOWN_73", nullptr},
    /* 0x74 */ {true, "RAMPAGE_AP_LOSS", "Temporarily reduce AP"},
    /* 0x75 */ {false, "UNKNOWN_75", nullptr},
    /* 0x76 */ {false, "REFLECT", "Generate reverse attack"},
    /* 0x77 */ {false, "UNKNOWN_77", nullptr},
    /* 0x78 */ {false, "ANY", nullptr}, // Treated as "any condition" in find functions
    /* 0x79 */ {false, "UNKNOWN_79", nullptr},
    /* 0x7A */ {false, "UNKNOWN_7A", nullptr},
    /* 0x7B */ {false, "UNKNOWN_7B", nullptr},
    /* 0x7C */ {false, "UNKNOWN_7C", nullptr},
    /* 0x7D */ {false, "UNKNOWN_7D", nullptr},
});

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
      return "(blank)";
    case Type::STAT:
      return phosg::string_printf("%hhd", this->stat);
    case Type::PLUS_STAT:
      return phosg::string_printf("+%hhd", this->stat);
    case Type::MINUS_STAT:
      return phosg::string_printf("-%d", -this->stat);
    case Type::EQUALS_STAT:
      return phosg::string_printf("=%hhd", this->stat);
    case Type::UNKNOWN:
      return "?";
    case Type::PLUS_UNKNOWN:
      return "+?";
    case Type::MINUS_UNKNOWN:
      return "-?";
    case Type::EQUALS_UNKNOWN:
      return "=?";
    default:
      return phosg::string_printf("[%02hhX %02hhX]", this->type, this->stat);
  }
}

bool CardDefinition::Effect::is_empty() const {
  return (this->effect_num == 0 &&
      this->type == ConditionType::NONE &&
      this->expr.empty() &&
      this->when == EffectWhen::NONE &&
      this->arg1.empty() &&
      this->arg2.empty() &&
      this->arg3.empty() &&
      this->apply_criterion == CriterionCode::NONE &&
      this->name_index == 0);
}

string CardDefinition::Effect::str_for_arg(const string& arg) {
  if (arg.empty()) {
    return arg;
  }
  if (arg.size() != 3) {
    return arg + " (invalid)";
  }
  size_t value;
  try {
    value = stoul(arg.c_str() + 1, nullptr, 10);
  } catch (const invalid_argument&) {
    return arg + " (invalid)";
  }

  switch (arg[0]) {
    case 'a':
      return phosg::string_printf("%s (Each activation lasts for %zu attack%s)", arg.c_str(), value, (value == 1) ? "" : "s");
    case 'C':
    case 'c':
      return phosg::string_printf("%s (Req. linked item (%zu=>%zu))", arg.c_str(), value / 10, value % 10);
    case 'd':
      return phosg::string_printf("%s (Req. die roll in [%zu, %zu])", arg.c_str(), value / 10, value % 10);
    case 'e':
      return arg + " (While equipped)";
    case 'h':
      return phosg::string_printf("%s (Req. HP >= %zu)", arg.c_str(), value);
    case 'i':
      return phosg::string_printf("%s (Req. HP <= %zu)", arg.c_str(), value);
    case 'n':
      try {
        return phosg::string_printf("%s (Req. condition: %s)", arg.c_str(), description_for_n_condition.at(value));
      } catch (const out_of_range&) {
        return arg + " (Req. condition: unknown)";
      }
    case 'o': {
      const char* suffix = ((value / 10) == 1) ? " on opponent card" : " on self";
      if (value == 0) {
        return phosg::string_printf("%s (Req. any previous effect%s)", arg.c_str(), suffix);
      } else {
        return phosg::string_printf("%s (Req. effect %zu passed%s)", arg.c_str(), static_cast<size_t>(value % 10), suffix);
      }
    }
    case 'p':
      try {
        return phosg::string_printf("%s (Target: %s)", arg.c_str(), description_for_p_target.at(value));
      } catch (const out_of_range&) {
        return arg + " (Target: unknown)";
      }
    case 'r':
      return phosg::string_printf("%s (Random with %zu%% chance)", arg.c_str(), value == 0 ? 100 : value);
    case 's':
      return phosg::string_printf("%s (Req. cost in [%zu, %zu])", arg.c_str(), value / 10, value % 10);
    case 't':
      return phosg::string_printf("%s (Turns: %zu)", arg.c_str(), value);
    default:
      return arg + " (unknown)";
  }
}

string CardDefinition::Effect::str(const char* separator, const TextSet* text_archive) const {
  vector<string> tokens;
  tokens.emplace_back(phosg::string_printf("%hhu:", this->effect_num));
  {
    uint8_t type = static_cast<uint8_t>(this->type);
    string cmd_str = phosg::string_printf("cmd=%02hhX", type);
    try {
      const char* name = description_for_condition_type.at(type).name;
      if (name) {
        cmd_str += ':';
        cmd_str += name;
      }
    } catch (const out_of_range&) {
    }
    tokens.emplace_back(std::move(cmd_str));
  }
  if (!this->expr.empty()) {
    tokens.emplace_back("expr=" + this->expr.decode());
  }
  tokens.emplace_back(phosg::string_printf("when=%02hhX:%s", static_cast<uint8_t>(this->when), phosg::name_for_enum(this->when)));
  tokens.emplace_back("arg1=" + this->str_for_arg(this->arg1.decode()));
  tokens.emplace_back("arg2=" + this->str_for_arg(this->arg2.decode()));
  tokens.emplace_back("arg3=" + this->str_for_arg(this->arg3.decode()));
  {
    uint8_t type = static_cast<uint8_t>(this->apply_criterion);
    string cond_str = phosg::string_printf("cond=%02hhX", type);
    try {
      const char* name = phosg::name_for_enum(this->apply_criterion);
      cond_str += ':';
      cond_str += name;
    } catch (const invalid_argument&) {
    }
    tokens.emplace_back(std::move(cond_str));
  }

  const char* name = nullptr;
  if (this->name_index && text_archive) {
    try {
      name = text_archive->get(45, this->name_index).c_str();
    } catch (const exception&) {
    }
  }
  if (name) {
    string formatted_name = name;
    for (char& ch : formatted_name) {
      if (ch == '\t') {
        ch = '$';
      }
    }
    tokens.emplace_back(phosg::string_printf("name=%02hhX \"%s\"", this->name_index, formatted_name.c_str()));
  } else {
    tokens.emplace_back(phosg::string_printf("name=%02hhX", this->name_index));
  }

  return phosg::join(tokens, separator);
}

bool CardDefinition::is_sc() const {
  return (this->type == CardType::HUNTERS_SC) || (this->type == CardType::ARKZ_SC);
}

bool CardDefinition::is_fc() const {
  return (this->type == CardType::ITEM) || (this->type == CardType::CREATURE);
}

bool CardDefinition::is_named_android_sc() const {
  static const unordered_set<uint16_t> TARGET_IDS({0x0005, 0x0007, 0x0110, 0x0113, 0x0114, 0x0117, 0x011B, 0x011F});
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
      case 1: // Single cell in front of FC (Attack)
        this->range[3] = 0x00000100;
        break;
      case 2: // Cell in front of FC and the front-left and front-right (Slash)
        this->range[3] = 0x00001110;
        break;
      case 3: // 3 cells in a line in front of FC (Long Arm)
        this->range[1] = 0x00000100;
        this->range[2] = 0x00000100;
        this->range[3] = 0x00000100;
        break;
      case 4: // All 8 cells around FC (Gifoie)
        this->range[3] = 0x00001110;
        this->range[4] = 0x00001010;
        this->range[5] = 0x00001110;
        break;
      case 5: // 2 cells in a line in front of FC (Mechgun)
        this->range[2] = 0x00000100;
        this->range[3] = 0x00000100;
        break;
      case 6: // Entire field (Grants)
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

string name_for_rank(CardRank rank) {
  static const vector<const char*> names(
      {"N1", "R1", "S", "E", "N2", "N3", "N4", "R2", "R3", "R4", "SS", "D1", "D2"});
  try {
    return names.at(static_cast<uint8_t>(rank) - 1);
  } catch (const out_of_range&) {
    return phosg::string_printf("(%02hhX)", static_cast<uint8_t>(rank));
  }
}

const char* name_for_target_mode(TargetMode target_mode) {
  static const vector<const char*> names({
      "NONE",
      "SINGLE_RANGE",
      "MULTI_RANGE",
      "SELF",
      "TEAM",
      "EVERYONE",
      "MULTI_RANGE_ALLIES",
      "ALL_ALLIES",
      "ALL",
      "OWN_FCS",
  });
  try {
    return names.at(static_cast<uint8_t>(target_mode));
  } catch (const out_of_range&) {
    return "__UNKNOWN__";
  }
}

string string_for_colors(const parray<uint8_t, 8>& colors) {
  string ret;
  for (size_t x = 0; x < 8; x++) {
    if (colors[x]) {
      if (!ret.empty()) {
        ret += ",";
      }
      try {
        ret += name_for_link_color(colors[x]);
      } catch (const invalid_argument&) {
        ret += phosg::string_printf("%02hhX", colors[x]);
      }
    }
  }
  if (ret.empty()) {
    return "(none)";
  }
  return ret;
}

string string_for_assist_turns(uint8_t turns) {
  if (turns == 90) {
    return "ONCE";
  } else if (turns == 99) {
    return "FOREVER";
  } else {
    return phosg::string_printf("%hhu", turns);
  }
}

string string_for_range(const parray<be_uint32_t, 6>& range) {
  string ret;
  for (size_t x = 0; x < 6; x++) {
    ret += phosg::string_printf("%05" PRIX32 "/", range[x].load());
  }
  while (phosg::starts_with(ret, "00000/")) {
    ret = ret.substr(6);
  }
  if (!ret.empty()) {
    ret.resize(ret.size() - 1);
  }
  return ret;
}

string string_for_drop_rate(uint16_t drop_rate) {
  vector<string> tokens;
  switch (drop_rate % 10) {
    case 0:
      tokens.emplace_back("mode=ANY");
      break;
    case 1:
      tokens.emplace_back("mode=OFFLINE_STORY");
      break;
    case 2:
      tokens.emplace_back("mode=OFFLINE_FREE_BATTLE");
      break;
    case 3:
      tokens.emplace_back("mode=OFFLINE_FREE_BATTLE_PVP");
      break;
    case 4:
      tokens.emplace_back("mode=ONLINE");
      break;
    case 5:
      tokens.emplace_back("mode=TOURNAMENT");
      break;
    case 6:
      tokens.emplace_back("mode=FORBIDDEN");
      break;
    default:
      tokens.emplace_back("mode=__UNKNOWN__");
  }
  uint8_t environment_number = (drop_rate / 10) % 100;
  if (environment_number) {
    tokens.emplace_back(phosg::string_printf("environment_number=%02hhX", static_cast<uint8_t>(environment_number - 1)));
  } else {
    tokens.emplace_back("environment_number=ANY");
  }
  tokens.emplace_back(phosg::string_printf("rarity_class=%hhu", static_cast<uint8_t>((drop_rate / 1000) % 10)));
  switch ((drop_rate / 10000) % 10) {
    case 0:
      tokens.emplace_back("deck_type=ANY");
      break;
    case 1:
      tokens.emplace_back("deck_type=HUNTERS");
      break;
    case 2:
      tokens.emplace_back("deck_type=ARKZ");
      break;
    default:
      tokens.emplace_back("deck_type=__UNKNOWN__");
  }
  string description = phosg::join(tokens, ", ");
  return phosg::string_printf("[%hu: %s]", drop_rate, description.c_str());
}

static const char* short_name_for_assist_ai_param_target(uint8_t target) {
  switch (target) {
    case 0:
      return "ANY";
    case 1:
      return "SELF";
    case 2:
      return "SELF_OR_ALLY";
    case 3:
      return "ENEMY";
    default:
      return "__UNKNOWN__";
  }
}

static const char* name_for_assist_ai_param_target(uint8_t target) {
  switch (target) {
    case 0:
      return "any player";
    case 1:
      return "self";
    case 2:
      return "self or ally";
    case 3:
      return "enemy player";
    default:
      return "__UNKNOWN__";
  }
}

string CardDefinition::str(bool single_line, const TextSet* text_archive) const {
  string type_str = phosg::name_for_enum(this->type);
  string criterion_str = phosg::name_for_enum(this->usable_criterion);
  string card_class_str = phosg::name_for_enum(this->card_class());
  string rank_str = name_for_rank(this->rank);
  const char* target_mode_str = name_for_target_mode(this->target_mode);
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
    if (!single_line) {
      effects_str += "\n    ";
    } else if (!effects_str.empty()) {
      effects_str += ", ";
    }
    effects_str += this->effects[x].str(single_line ? ", " : "\n      ", text_archive);
  }
  if (!single_line && effects_str.empty()) {
    effects_str = " (none)";
  }

  string drop0_str = string_for_drop_rate(this->drop_rates[0]);
  string drop1_str = string_for_drop_rate(this->drop_rates[1]);

  string cost_str = phosg::string_printf("%hhX", this->self_cost);
  if (this->ally_cost) {
    if (single_line) {
      cost_str += phosg::string_printf("+%hhX", this->ally_cost);
    } else {
      cost_str += phosg::string_printf(" (self) + %hhX (ally)", this->ally_cost);
    }
  }

  string en_name_s = this->en_name.decode();
  if (single_line) {
    string range_str = string_for_range(this->range);
    return phosg::string_printf(
        "[Card: %04" PRIX32 " name=%s type=%s usable_condition=%s rank=%s "
        "cost=%s target=%s range=%s assist_turns=%s cannot_move=%s "
        "cannot_attack=%s cannot_drop=%s hp=%s ap=%s tp=%s mv=%s left=%s right=%s "
        "top=%s class=%s assist_ai_params=[target=%s priority=%hhu effect=%hhu] drop_rates=[%s, %s] effects=[%s]]",
        this->card_id.load(),
        en_name_s.c_str(),
        type_str.c_str(),
        criterion_str.c_str(),
        rank_str.c_str(),
        cost_str.c_str(),
        target_mode_str,
        range_str.c_str(),
        assist_turns_str.c_str(),
        this->cannot_move ? "true" : "false",
        this->cannot_attack ? "true" : "false",
        this->cannot_drop ? "true" : "false",
        hp_str.c_str(),
        ap_str.c_str(),
        tp_str.c_str(),
        mv_str.c_str(),
        left_str.c_str(),
        right_str.c_str(),
        top_str.c_str(),
        card_class_str.c_str(),
        short_name_for_assist_ai_param_target((this->assist_ai_params / 1000) % 10),
        static_cast<uint8_t>((this->assist_ai_params / 100) % 10),
        static_cast<uint8_t>(this->assist_ai_params % 100),
        drop0_str.c_str(),
        drop1_str.c_str(),
        effects_str.c_str());

  } else { // Not single-line
    string range_str;
    if (this->range[0] == 0x000FFFFF) {
      range_str = " (entire field)";
    } else {
      for (size_t x = 0; x < 6; x++) {
        range_str += "\n    ";
        for (size_t z = 0; z < 5; z++) {
          bool is_included = ((this->range[x] >> (16 - (z * 4))) & 0xF);
          if (x == 4 && z == 2) {
            range_str += is_included ? "@" : "#";
          } else {
            range_str += is_included ? "*" : "-";
          }
        }
      }
    }
    string jp_name_s = this->jp_name.decode();
    string en_name_short_s = this->en_short_name.decode();
    string jp_name_short_s = this->jp_short_name.decode();
    string names_str;
    if (!en_name_s.empty()) {
      names_str += phosg::string_printf(" EN: \"%s\"", en_name_s.c_str());
      if (!en_name_short_s.empty() && en_name_short_s != en_name_s) {
        names_str += phosg::string_printf(" (Abr. \"%s\")", en_name_short_s.c_str());
      }
    }
    if (!jp_name_s.empty()) {
      names_str += phosg::string_printf(" JP: \"%s\"", jp_name_s.c_str());
      if (!jp_name_short_s.empty() && jp_name_short_s != jp_name_s) {
        names_str += phosg::string_printf(" (Abr. \"%s\")", jp_name_short_s.c_str());
      }
    }
    return phosg::string_printf(
        "\
Card: %04" PRIX32 "%s\n\
  Type: %s, class: %s\n\
  Usability condition: %s\n\
  Rank: %s\n\
  Cost: %s\n\
  Target mode: %s\n\
  Range:%s\n\
  Assist turns: %s\n\
  Capabilities: %s move, %s attack\n\
  HP: %s, AP: %s, TP: %s, MV: %s\n\
  Colors:\n\
    Left: %s\n\
    Right: %s\n\
    Top: %s\n\
  Assist AI parameters: [target %s, priority %hu, effect %hu]\n\
  Drop rates:\n\
    %s\n\
    %s\n\
    %s\n\
  Effects:%s",
        this->card_id.load(),
        names_str.c_str(),
        type_str.c_str(),
        card_class_str.c_str(),
        criterion_str.c_str(),
        rank_str.c_str(),
        cost_str.c_str(),
        target_mode_str,
        range_str.c_str(),
        assist_turns_str.c_str(),
        this->cannot_move ? "cannot" : "can",
        this->cannot_attack ? "cannot" : "can",
        hp_str.c_str(),
        ap_str.c_str(),
        tp_str.c_str(),
        mv_str.c_str(),
        left_str.c_str(),
        right_str.c_str(),
        top_str.c_str(),
        name_for_assist_ai_param_target((this->assist_ai_params / 1000) % 10),
        static_cast<uint8_t>((this->assist_ai_params / 100) % 10),
        static_cast<uint8_t>(this->assist_ai_params % 100),
        drop0_str.c_str(),
        drop1_str.c_str(),
        this->cannot_drop ? "Forbidden" : "Permitted",
        effects_str.c_str());
  }
}

phosg::JSON CardDefinition::Stat::json() const {
  const char* type_str = "unknown";
  switch (this->type) {
    case Type::BLANK:
      type_str = "BLANK";
      break;
    case Type::STAT:
      type_str = "DEFAULT";
      break;
    case Type::PLUS_STAT:
      type_str = "PLUS";
      break;
    case Type::MINUS_STAT:
      type_str = "MINUS";
      break;
    case Type::EQUALS_STAT:
      type_str = "EQUALS";
      break;
    case Type::UNKNOWN:
      type_str = "UNKNOWN";
      break;
    case Type::PLUS_UNKNOWN:
      type_str = "PLUS_UNKNOWN";
      break;
    case Type::MINUS_UNKNOWN:
      type_str = "MINUS_UNKNOWN";
      break;
    case Type::EQUALS_UNKNOWN:
      type_str = "EQUALS_UNKNOWN";
      break;
  }
  return phosg::JSON::dict({
      {"type", type_str},
      {"value", this->stat},
  });
}

phosg::JSON CardDefinition::Effect::json() const {
  return phosg::JSON::dict({
      {"EffectNum", this->effect_num},
      {"ConditionType", phosg::name_for_enum(this->type)},
      {"Expression", this->expr.decode()},
      {"When", phosg::name_for_enum(this->when)},
      {"Arg1", this->arg1.decode()},
      {"Arg2", this->arg2.decode()},
      {"Arg3", this->arg3.decode()},
      {"ApplyCriterion", phosg::name_for_enum(this->apply_criterion)},
      {"NameIndex", this->name_index},
  });
}

phosg::JSON CardDefinition::json() const {
  phosg::JSON range_json;
  if (this->range[0] == 0x000FFFFF) {
    range_json = "ENTIRE_FIELD";
  } else {
    range_json = phosg::JSON::list();
    for (size_t y = 0; y < 6; y++) {
      uint32_t row = this->range[y];
      auto& row_json = range_json.emplace_back(phosg::JSON::list());
      for (size_t x = 0; x < 5; x++) {
        row_json.emplace_back((row & 0x00010000) ? true : false);
        row <<= 4;
      }
    }
  }

  phosg::JSON effects_json = phosg::JSON::list();
  for (size_t z = 0; z < this->effects.size(); z++) {
    if (!this->effects[z].is_empty()) {
      effects_json.emplace_back(this->effects[z].json());
    }
  }

  return phosg::JSON::dict({
      {"CardID", this->card_id.load()},
      {"JPName", this->jp_name.decode()},
      {"CardType", phosg::name_for_enum(this->type)},
      {"SelfCost", this->self_cost},
      {"AllyCost", this->ally_cost},
      {"HP", this->hp.json()},
      {"AP", this->ap.json()},
      {"TP", this->tp.json()},
      {"MV", this->mv.json()},
      {"LeftColors", json_for_link_colors(this->left_colors)},
      {"RightColors", json_for_link_colors(this->right_colors)},
      {"TopColors", json_for_link_colors(this->top_colors)},
      {"Range", std::move(range_json)},
      {"TargetMode", name_for_target_mode(this->target_mode)},
      {"AssistTurns", this->assist_turns},
      {"CannotMove", this->cannot_move ? true : false},
      {"CannotAttack", this->cannot_attack ? true : false},
      {"CannotDrop", this->cannot_drop ? true : false},
      {"UsableCriterion", phosg::name_for_enum(this->usable_criterion)},
      {"Rank", name_for_rank(this->rank)},
      {"CardClass", phosg::name_for_enum(this->card_class())},
      {"AssistAIParams", this->assist_ai_params.load()},
      {"DropRates", phosg::JSON::list({this->drop_rates[0].load(), this->drop_rates[1].load()})},
      {"ENName", this->en_name.decode()},
      {"JPShortName", this->jp_short_name.decode()},
      {"ENShortName", this->en_short_name.decode()},
      {"Effects", std::move(effects_json)},
  });
}

void PlayerConfig::decrypt() {
  if (!this->is_encrypted) {
    return;
  }
  decrypt_trivial_gci_data(
      &this->card_counts,
      offsetof(PlayerConfig, decks) - offsetof(PlayerConfig, card_counts),
      this->basis);
  this->is_encrypted = 0;
  this->basis = 0;
}

void PlayerConfig::encrypt(uint8_t basis) {
  if (this->is_encrypted) {
    if (this->basis == basis) {
      return;
    }
    this->decrypt();
  }
  decrypt_trivial_gci_data(
      &this->card_counts,
      offsetof(PlayerConfig, decks) - offsetof(PlayerConfig, card_counts),
      basis);
  this->is_encrypted = 1;
  this->basis = basis;
}

PlayerConfigNTE::PlayerConfigNTE(const PlayerConfig& config)
    : rank_text(config.rank_text),
      unknown_a1(config.unknown_a1),
      tech_menu_shortcut_entries(config.tech_menu_shortcut_entries),
      choice_search_config(config.choice_search_config),
      scenario_progress(config.scenario_progress),
      unused_offline_records(config.unused_offline_records),
      unknown_a4(config.unknown_a4),
      is_encrypted(config.is_encrypted),
      basis(config.basis),
      unused(config.unused),
      card_counts(config.card_counts),
      card_count_checksums(config.card_count_checksums),
      rare_tokens(config.rare_tokens),
      decks(config.decks),
      unknown_a8(config.unknown_a8),
      offline_clv_exp(config.offline_clv_exp),
      online_clv_exp(config.online_clv_exp),
      recent_human_opponents(config.recent_human_opponents),
      recent_battle_start_timestamps(config.recent_battle_start_timestamps),
      unknown_a10(config.unknown_a10),
      init_timestamp(config.init_timestamp),
      last_online_battle_start_timestamp(config.last_online_battle_start_timestamp),
      unknown_t3(config.unknown_t3),
      unknown_a14(config.unknown_a14) {
  // TODO: Do we need to recompute card_count_checksums? (Here or in operator
  // PlayerConfig?)
}

PlayerConfigNTE::operator PlayerConfig() const {
  PlayerConfig ret;
  ret.rank_text = this->rank_text;
  ret.unknown_a1 = this->unknown_a1;
  ret.tech_menu_shortcut_entries = this->tech_menu_shortcut_entries;
  ret.choice_search_config = this->choice_search_config;
  ret.scenario_progress = this->scenario_progress;
  ret.unused_offline_records = this->unused_offline_records;
  ret.unknown_a4 = this->unknown_a4;
  ret.is_encrypted = this->is_encrypted;
  ret.basis = this->basis;
  ret.unused = this->unused;
  ret.card_counts = this->card_counts;
  ret.card_count_checksums = this->card_count_checksums;
  ret.rare_tokens = this->rare_tokens;
  ret.decks = this->decks;
  ret.unknown_a8 = this->unknown_a8;
  ret.offline_clv_exp = this->offline_clv_exp;
  ret.online_clv_exp = this->online_clv_exp;
  ret.recent_human_opponents = this->recent_human_opponents;
  ret.recent_battle_start_timestamps = this->recent_battle_start_timestamps;
  ret.unknown_a10 = this->unknown_a10;
  ret.init_timestamp = this->init_timestamp;
  ret.last_online_battle_start_timestamp = this->last_online_battle_start_timestamp;
  ret.unknown_t3 = this->unknown_t3;
  ret.unknown_a14 = this->unknown_a14;
  return ret;
}

Rules::Rules(const phosg::JSON& json) {
  this->clear();
  this->overall_time_limit = json.get_int("overall_time_limit", this->overall_time_limit);
  this->phase_time_limit = json.get_int("phase_time_limit", this->phase_time_limit);
  this->allowed_cards = json.get_enum("allowed_cards", this->allowed_cards);
  this->min_dice_value = json.get_int("min_dice", this->min_dice_value);
  this->max_dice_value = json.get_int("max_dice", this->max_dice_value);
  this->disable_deck_shuffle = json.get_bool("disable_deck_shuffle", this->disable_deck_shuffle);
  this->disable_deck_loop = json.get_bool("disable_deck_loop", this->disable_deck_loop);
  this->char_hp = json.get_int("char_hp", this->char_hp);
  this->hp_type = json.get_enum("hp_type", this->hp_type);
  this->no_assist_cards = json.get_bool("no_assist_cards", this->no_assist_cards);
  this->disable_dialogue = json.get_bool("disable_dialogue", this->disable_dialogue);
  this->dice_exchange_mode = json.get_enum("dice_exchange_mode", this->dice_exchange_mode);
  this->disable_dice_boost = json.get_bool("disable_dice_boost", this->disable_dice_boost);
  uint8_t min_dice = json.get_int("min_def_dice", (this->def_dice_value_range >> 4) & 0x0F);
  uint8_t max_dice = json.get_int("max_def_dice", this->def_dice_value_range & 0x0F);
  this->def_dice_value_range = ((min_dice << 4) & 0xF0) | (max_dice & 0x0F);
  min_dice = json.get_int("min_atk_dice_2v1", (this->atk_dice_value_range_2v1 >> 4) & 0x0F);
  max_dice = json.get_int("max_atk_dice_2v1", this->atk_dice_value_range_2v1 & 0x0F);
  this->atk_dice_value_range_2v1 = ((min_dice << 4) & 0xF0) | (max_dice & 0x0F);
  min_dice = json.get_int("min_def_dice_2v1", (this->def_dice_value_range_2v1 >> 4) & 0x0F);
  max_dice = json.get_int("max_def_dice_2v1", this->def_dice_value_range_2v1 & 0x0F);
  this->def_dice_value_range_2v1 = ((min_dice << 4) & 0xF0) | (max_dice & 0x0F);
}

phosg::JSON Rules::json() const {
  return phosg::JSON::dict({
      {"overall_time_limit", this->overall_time_limit},
      {"phase_time_limit", this->phase_time_limit},
      {"allowed_cards", phosg::name_for_enum(this->allowed_cards)},
      {"min_dice", this->min_dice_value},
      {"max_dice", this->max_dice_value},
      {"disable_deck_shuffle", static_cast<bool>(this->disable_deck_shuffle)},
      {"disable_deck_loop", static_cast<bool>(this->disable_deck_loop)},
      {"char_hp", this->char_hp},
      {"hp_type", phosg::name_for_enum(this->hp_type)},
      {"no_assist_cards", static_cast<bool>(this->no_assist_cards)},
      {"disable_dialogue", static_cast<bool>(this->disable_dialogue)},
      {"dice_exchange_mode", phosg::name_for_enum(this->dice_exchange_mode)},
      {"disable_dice_boost", static_cast<bool>(this->disable_dice_boost)},
      {"min_def_dice", ((this->def_dice_value_range >> 4) & 0x0F)},
      {"max_def_dice", (this->def_dice_value_range & 0x0F)},
      {"min_atk_dice_2v1", ((this->atk_dice_value_range_2v1 >> 4) & 0x0F)},
      {"max_atk_dice_2v1", (this->atk_dice_value_range_2v1 & 0x0F)},
      {"min_def_dice_2v1", ((this->def_dice_value_range_2v1 >> 4) & 0x0F)},
      {"max_def_dice_2v1", (this->def_dice_value_range_2v1 & 0x0F)},
  });
}

void Rules::set_defaults() {
  this->clear();
  this->overall_time_limit = 24; // 2 hours
  this->phase_time_limit = 30;
  this->min_dice_value = 1;
  this->max_dice_value = 6;
  this->char_hp = 15;
}

void Rules::clear() {
  this->overall_time_limit = 0;
  this->phase_time_limit = 0;
  this->allowed_cards = AllowedCards::ALL;
  this->min_dice_value = 0;
  this->max_dice_value = 0;
  this->disable_deck_shuffle = 0;
  this->disable_deck_loop = 0;
  this->char_hp = 0;
  this->hp_type = HPType::DEFEAT_PLAYER;
  this->no_assist_cards = 0;
  this->disable_dialogue = 0;
  this->dice_exchange_mode = DiceExchangeMode::HIGH_ATK;
  this->disable_dice_boost = 0;
  this->def_dice_value_range = 0;
  this->atk_dice_value_range_2v1 = 0;
  this->def_dice_value_range_2v1 = 0;
  this->unused.clear(0);
}

pair<uint8_t, uint8_t> Rules::atk_dice_range(bool is_1p_2v1) const {
  pair<uint8_t, uint8_t> ret;
  if (is_1p_2v1 && this->atk_dice_value_range_2v1 && (this->atk_dice_value_range_2v1 != 0xFF)) {
    ret = make_pair((this->atk_dice_value_range_2v1 >> 4) & 0x0F, this->atk_dice_value_range_2v1 & 0x0F);
  } else {
    ret = make_pair(this->min_dice_value, this->max_dice_value);
  }
  if (ret.first == 0) {
    ret.first = 1;
  }
  if (ret.second == 0) {
    ret.second = 6;
  }
  if (ret.first > ret.second) {
    ret = make_pair(ret.second, ret.first);
  }
  return ret;
}

pair<uint8_t, uint8_t> Rules::def_dice_range(bool is_1p_2v1) const {
  pair<uint8_t, uint8_t> ret;
  if (is_1p_2v1 && this->def_dice_value_range_2v1 && (this->def_dice_value_range_2v1 != 0xFF)) {
    ret = make_pair((this->def_dice_value_range_2v1 >> 4) & 0x0F, this->def_dice_value_range_2v1 & 0x0F);
  } else if (this->def_dice_value_range && (this->def_dice_value_range != 0xFF)) {
    ret = make_pair((this->def_dice_value_range >> 4) & 0x0F, this->def_dice_value_range & 0x0F);
  } else {
    ret = make_pair(this->min_dice_value, this->max_dice_value);
  }
  if (ret.first == 0) {
    ret.first = 1;
  }
  if (ret.second == 0) {
    ret.second = 6;
  }
  if (ret.first > ret.second) {
    ret = make_pair(ret.second, ret.first);
  }
  return ret;
}

string Rules::str() const {
  vector<string> tokens;

  if (this->char_hp == 0xFF) {
    tokens.emplace_back("char_hp=(open)");
  } else {
    tokens.emplace_back(phosg::string_printf("char_hp=%hhu", this->char_hp));
  }

  switch (this->hp_type) {
    case HPType::DEFEAT_PLAYER:
      tokens.emplace_back("hp_type=defeat-player");
      break;
    case HPType::DEFEAT_TEAM:
      tokens.emplace_back("hp_type=defeat-team");
      break;
    case HPType::COMMON_HP:
      tokens.emplace_back("hp_type=common-hp");
      break;
    default:
      if (static_cast<uint8_t>(this->hp_type) == 0xFF) {
        tokens.emplace_back("hp_type=(open)");
      } else {
        tokens.emplace_back(phosg::string_printf("hp_type=(%02hhX)",
            static_cast<uint8_t>(this->hp_type)));
      }
      break;
  }

  auto format_dice_range = +[](std::pair<uint8_t, uint8_t> range) -> string {
    string s = "[";
    if (range.first == 0xFF) {
      s += "min=(open), ";
    } else if (range.first == 0x00) {
      s += "min=(default), ";
    } else {
      s += phosg::string_printf("min=%hhu, ", range.first);
    }
    if (range.second == 0xFF) {
      s += "max=(open)]";
    } else if (range.second == 0x00) {
      s += "max=(default)]";
    } else {
      s += phosg::string_printf("max=%hhu]", range.second);
    }
    return s;
  };
  tokens.emplace_back("dice_range=" + format_dice_range(make_pair(this->min_dice_value, this->max_dice_value)));
  if (this->def_dice_value_range) {
    tokens.emplace_back("def_dice_range=" + format_dice_range(this->def_dice_range(false)));
  }
  if (this->atk_dice_value_range_2v1) {
    tokens.emplace_back("atk_dice_range_2v1=" + format_dice_range(this->atk_dice_range(true)));
  }
  if (this->def_dice_value_range_2v1) {
    tokens.emplace_back("def_dice_range_2v1=" + format_dice_range(this->def_dice_range(true)));
  }

  switch (this->dice_exchange_mode) {
    case DiceExchangeMode::HIGH_ATK:
      tokens.emplace_back("dice_exchange=high-atk");
      break;
    case DiceExchangeMode::HIGH_DEF:
      tokens.emplace_back("dice_exchange=high-def");
      break;
    case DiceExchangeMode::NONE:
      tokens.emplace_back("dice_exchange=none");
      break;
    default:
      if (static_cast<uint8_t>(this->dice_exchange_mode) == 0xFF) {
        tokens.emplace_back("dice_exchange=(open)");
      } else {
        tokens.emplace_back(phosg::string_printf("dice_exchange=(%02hhX)",
            static_cast<uint8_t>(this->dice_exchange_mode)));
      }
      break;
  }

  auto str_for_disable_bool = +[](uint8_t v) -> string {
    switch (v) {
      case 0x00:
        return "on";
      case 0x01:
        return "off";
      case 0xFF:
        return "(open)";
      default:
        return phosg::string_printf("(%02hhX)", v);
    }
  };

  tokens.emplace_back("dice_boost=" + str_for_disable_bool(this->disable_dice_boost));
  tokens.emplace_back("deck_shuffle=" + str_for_disable_bool(this->disable_deck_shuffle));
  tokens.emplace_back("deck_loop=" + str_for_disable_bool(this->disable_deck_loop));

  switch (this->allowed_cards) {
    case AllowedCards::ALL:
      tokens.emplace_back("allowed_cards=all");
      break;
    case AllowedCards::N_ONLY:
      tokens.emplace_back("allowed_cards=n-only");
      break;
    case AllowedCards::N_R_ONLY:
      tokens.emplace_back("allowed_cards=n-r-only");
      break;
    case AllowedCards::N_R_S_ONLY:
      tokens.emplace_back("allowed_cards=n-r-s-only");
      break;
    default:
      if (static_cast<uint8_t>(this->allowed_cards) == 0xFF) {
        tokens.emplace_back("allowed_cards=(open)");
      } else {
        tokens.emplace_back(phosg::string_printf("allowed_cards=(%02hhX)",
            static_cast<uint8_t>(this->allowed_cards)));
      }
      break;
  }
  tokens.emplace_back("allow_assist_cards=" + str_for_disable_bool(this->no_assist_cards));

  if (this->overall_time_limit == 0xFF) {
    tokens.emplace_back("overall_time_limit=(open)");
  } else if (this->overall_time_limit) {
    tokens.emplace_back(phosg::string_printf("overall_time_limit=%zumin", static_cast<size_t>(this->overall_time_limit * 5)));
  } else {
    tokens.emplace_back("overall_time_limit=(infinite)");
  }
  if (this->phase_time_limit == 0xFF) {
    tokens.emplace_back("phase_time_limit=(open)");
  } else if (this->phase_time_limit) {
    tokens.emplace_back(phosg::string_printf("phase_time_limit=%hhusec", this->phase_time_limit));
  } else {
    tokens.emplace_back("phase_time_limit=(infinite)");
  }

  tokens.emplace_back("dialogue=" + str_for_disable_bool(this->disable_dialogue));

  return "Rules[" + phosg::join(tokens, ", ") + "]";
}

RulesTrial::RulesTrial(const Rules& r)
    : overall_time_limit(r.overall_time_limit),
      phase_time_limit(r.phase_time_limit),
      allowed_cards(r.allowed_cards),
      // ATK/DEF behaviors set below
      disable_deck_shuffle(r.disable_deck_shuffle),
      disable_deck_loop(r.disable_deck_loop),
      char_hp(r.char_hp),
      hp_type(r.hp_type),
      no_assist_cards(r.no_assist_cards),
      disable_dialogue(r.disable_dialogue),
      dice_exchange_mode(r.dice_exchange_mode) {
  if (r.max_dice_value == r.min_dice_value) {
    this->atk_die_behavior = r.max_dice_value;
  } else {
    this->atk_die_behavior = 0; // Random
  }
  if (r.def_dice_value_range == 0xFF) {
    this->def_die_behavior = 0xFF;
  } else {
    auto def_range = r.def_dice_range(false);
    if (def_range.first == def_range.second) {
      this->def_die_behavior = def_range.first;
    } else {
      this->def_die_behavior = 0;
    }
  }
}

RulesTrial::operator Rules() const {
  Rules ret;
  ret.overall_time_limit = this->overall_time_limit;
  ret.phase_time_limit = this->phase_time_limit;
  ret.allowed_cards = this->allowed_cards;
  if (this->atk_die_behavior) {
    ret.min_dice_value = this->atk_die_behavior;
    ret.max_dice_value = this->atk_die_behavior;
  } else {
    ret.min_dice_value = 1;
    ret.max_dice_value = 6;
  }
  ret.disable_deck_shuffle = this->disable_deck_shuffle;
  ret.disable_deck_loop = this->disable_deck_loop;
  ret.char_hp = this->char_hp;
  ret.hp_type = this->hp_type;
  ret.no_assist_cards = this->no_assist_cards;
  ret.disable_dialogue = this->disable_dialogue;
  ret.dice_exchange_mode = this->dice_exchange_mode;
  ret.disable_dice_boost = 0;
  if (this->def_die_behavior) {
    ret.def_dice_value_range = (this->def_die_behavior << 4) | this->def_die_behavior;
  } else {
    ret.def_dice_value_range = 0x16;
  }
  ret.atk_dice_value_range_2v1 = 0x00;
  ret.def_dice_value_range_2v1 = 0x00;
  ret.unused.clear(0);
  return ret;
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
      (this->team_dice_bonus == other.team_dice_bonus) &&
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
  this->team_dice_bonus.clear(0);
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
  this->team_dice_bonus.clear(0xFF);
  this->first_team_turn = 0xFF;
  this->tournament_flag = 0xFF;
  this->client_sc_card_types.clear(CardType::INVALID_FF);
}

OverlayState::OverlayState() {
  this->clear();
}

void OverlayState::clear() {
  for (size_t y = 0; y < this->tiles.size(); y++) {
    this->tiles[y].clear(0);
  }
  this->unused1.clear(0);
  this->trap_tile_colors_nte.clear(0);
  this->trap_card_ids_nte.clear(0);
}

void MapDefinition::assert_semantically_equivalent(const MapDefinition& other) const {
  if (this->map_number != other.map_number) {
    throw runtime_error("map number not equal");
  }
  if (this->width != other.width) {
    throw runtime_error("width not equal");
  }
  if (this->height != other.height) {
    throw runtime_error("width not equal");
  }
  if (this->environment_number != other.environment_number) {
    throw runtime_error("environment number not equal");
  }
  if (this->map_tiles != other.map_tiles) {
    throw runtime_error("tiles not equal");
  }
  if (this->start_tile_definitions != other.start_tile_definitions) {
    throw runtime_error("start tile definitions not equal");
  }
  if (this->overlay_state.tiles != other.overlay_state.tiles) {
    throw runtime_error("modification tiles not equal");
  }
  if (this->default_rules != other.default_rules) {
    throw runtime_error("default rules not equal");
  }
  for (size_t z = 0; z < this->npc_decks.size(); z++) {
    if (this->npc_decks[z].card_ids != other.npc_decks[z].card_ids) {
      throw runtime_error("npc deck card IDs not equal");
    }
    const auto& this_ai_params = this->npc_ai_params[z];
    const auto& other_ai_params = other.npc_ai_params[z];
    if (this_ai_params.unknown_a1 != other_ai_params.unknown_a1) {
      throw runtime_error("npc AI params unknown_a1 not equal");
    }
    if (this_ai_params.is_arkz != other_ai_params.is_arkz) {
      throw runtime_error("npc AI params is_arkz not equal");
    }
    if (this_ai_params.unknown_a2 != other_ai_params.unknown_a2) {
      throw runtime_error("npc AI params unknown_a2 not equal");
    }
    if (this_ai_params.params != other_ai_params.params) {
      throw runtime_error("npc AI params not equal");
    }
  }
  if (this->unknown_a7 != other.unknown_a7) {
    throw runtime_error("unknown_a7 not equal");
  }
  if (this->npc_ai_params_entry_index != other.npc_ai_params_entry_index) {
    throw runtime_error("npc AI params entry indexes not equal");
  }
  if (this->reward_card_ids != other.reward_card_ids) {
    throw runtime_error("reward card IDs not equal");
  }
  if (this->win_level_override != other.win_level_override) {
    throw runtime_error("win level override not equal");
  }
  if (this->loss_level_override != other.loss_level_override) {
    throw runtime_error("loss level override not equal");
  }
  if (this->field_offset_x != other.field_offset_x) {
    throw runtime_error("field x offset not equal");
  }
  if (this->field_offset_y != other.field_offset_y) {
    throw runtime_error("field y offset not equal");
  }
  if (this->map_category != other.map_category) {
    throw runtime_error("map category not equal");
  }
  if (this->cyber_block_type != other.cyber_block_type) {
    throw runtime_error("cyber block type not equal");
  }
  if (this->unknown_a11 != other.unknown_a11) {
    throw runtime_error("unknown_a11 not equal");
  }
  if (this->unavailable_sc_cards != other.unavailable_sc_cards) {
    throw runtime_error("unavailable SC cards not equal");
  }
  if (this->entry_states != other.entry_states) {
    throw runtime_error("entry states not equal");
  }
}

phosg::JSON MapDefinition::CameraSpec::json() const {
  return phosg::JSON::dict({
      {"Camera", phosg::JSON::list({this->camera_x.load(), this->camera_y.load(), this->camera_z.load()})},
      {"Focus", phosg::JSON::list({this->focus_x.load(), this->focus_y.load(), this->focus_z.load()})},
  });
}

phosg::JSON MapDefinition::NPCDeck::json(uint8_t language) const {
  phosg::JSON card_ids_json = phosg::JSON::list();
  for (size_t z = 0; z < this->card_ids.size(); z++) {
    if (this->card_ids[z] != 0xFFFF) {
      card_ids_json.emplace_back(this->card_ids[z].load());
    }
  }
  return phosg::JSON::dict({
      {"Name", this->deck_name.decode(language)},
      {"CardIDs", std::move(card_ids_json)},
  });
}

phosg::JSON MapDefinition::AIParams::json(uint8_t language) const {
  phosg::JSON params_json = phosg::JSON::list();
  for (size_t z = 0; z < this->params.size(); z++) {
    params_json.emplace_back(this->params[z].load());
  }
  return phosg::JSON::dict({
      {"IsArkz", this->is_arkz ? true : false},
      {"Name", this->ai_name.decode(language)},
      {"CardIDs", std::move(params_json)},
  });
}

phosg::JSON MapDefinition::DialogueSet::json(uint8_t language) const {
  phosg::JSON strings_json = phosg::JSON::list();
  for (size_t z = 0; z < this->strings.size(); z++) {
    strings_json.emplace_back(this->strings[z].decode(language));
  }
  return phosg::JSON::dict({
      {"When", this->when.load()},
      {"PercentChance", this->percent_chance.load()},
      {"CardIDs", std::move(strings_json)},
  });
}

phosg::JSON MapDefinition::EntryState::json() const {
  phosg::JSON player_type_json;
  switch (this->player_type) {
    case 0x00:
      player_type_json = "Player";
      break;
    case 0x01:
      player_type_json = "Player/COM";
      break;
    case 0x02:
      player_type_json = "COM";
      break;
    case 0x03:
      player_type_json = "NPC";
      break;
    case 0x04:
      player_type_json = "NONE";
      break;
    case 0xFF:
      player_type_json = "FREE";
      break;
    default:
      player_type_json = this->player_type;
  }
  phosg::JSON deck_type_json;
  switch (this->deck_type) {
    case 0x00:
      deck_type_json = "HERO ONLY";
      break;
    case 0x01:
      deck_type_json = "DARK ONLY";
      break;
    case 0xFF:
      deck_type_json = "ANY";
      break;
    default:
      deck_type_json = this->deck_type;
  }
  return phosg::JSON::dict({
      {"PlayerType", std::move(player_type_json)},
      {"DeckType", std::move(deck_type_json)},
  });
}

// TODO:
// phosg::JSON MapDefinition::json() const { ... }

string MapDefinition::CameraSpec::str() const {
  return phosg::string_printf(
      "CameraSpec[a1=(%g %g %g %g %g %g %g %g %g) camera=(%g %g %g) focus=(%g %g %g) a2=(%g %g %g)]",
      this->unknown_a1[0].load(), this->unknown_a1[1].load(),
      this->unknown_a1[2].load(), this->unknown_a1[3].load(),
      this->unknown_a1[4].load(), this->unknown_a1[5].load(),
      this->unknown_a1[6].load(), this->unknown_a1[7].load(),
      this->unknown_a1[8].load(), this->camera_x.load(),
      this->camera_y.load(), this->camera_z.load(),
      this->focus_x.load(), this->focus_y.load(),
      this->focus_z.load(), this->unknown_a2[0].load(),
      this->unknown_a2[1].load(), this->unknown_a2[2].load());
}

string MapDefinition::str(const CardIndex* card_index, uint8_t language) const {
  deque<string> lines;
  auto add_map = [&](const parray<parray<uint8_t, 0x10>, 0x10>& tiles) {
    for (size_t y = 0; y < this->height; y++) {
      string line = "   ";
      for (size_t x = 0; x < this->width; x++) {
        line += phosg::string_printf(" %02hhX", tiles[y][x]);
      }
      lines.emplace_back(std::move(line));
    }
  };

  lines.emplace_back(phosg::string_printf("Map %08" PRIX32 ": %hhux%hhu",
      this->map_number.load(), this->width, this->height));
  lines.emplace_back(phosg::string_printf("  tag: %08" PRIX32, this->tag.load()));
  lines.emplace_back(phosg::string_printf("  environment_number: %02hhX (%s)", this->environment_number, name_for_environment_number(this->environment_number)));
  lines.emplace_back(phosg::string_printf("  num_camera_zones: %02hhX", this->num_camera_zones));
  lines.emplace_back("  tiles:");
  add_map(this->map_tiles);
  lines.emplace_back(phosg::string_printf(
      "  start_tile_definitions: A:[1p: %02hhX; 2p: %02hhX,%02hhX; 3p: %02hhX,%02hhX,%02hhX], B:[1p: %02hhX; 2p: %02hhX,%02hhX; 3p: %02hhX,%02hhX,%02hhX]",
      this->start_tile_definitions[0][0], this->start_tile_definitions[0][1],
      this->start_tile_definitions[0][2], this->start_tile_definitions[0][3],
      this->start_tile_definitions[0][4], this->start_tile_definitions[0][5],
      this->start_tile_definitions[1][0], this->start_tile_definitions[1][1],
      this->start_tile_definitions[1][2], this->start_tile_definitions[1][3],
      this->start_tile_definitions[1][4], this->start_tile_definitions[1][5]));
  for (size_t z = 0; z < this->num_camera_zones; z++) {
    for (size_t w = 0; w < 2; w++) {
      lines.emplace_back(phosg::string_printf("  camera zone %zu (team %c):", z, w ? 'A' : 'B'));
      add_map(this->camera_zone_maps[w][z]);
      lines.emplace_back("    " + this->camera_zone_specs[w][z].str());
    }
  }
  for (size_t w = 0; w < 3; w++) {
    for (size_t z = 0; z < 2; z++) {
      string spec_str = this->overview_specs[w][z].str();
      lines.emplace_back(phosg::string_printf("  overview_specs[%zu][team %zu]: %s", w, z, spec_str.c_str()));
    }
  }
  lines.emplace_back("  overlay tiles:");
  add_map(this->overlay_state.tiles);
  lines.emplace_back(phosg::string_printf(
      "  unused1: %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32,
      this->overlay_state.unused1[0].load(),
      this->overlay_state.unused1[1].load(),
      this->overlay_state.unused1[2].load(),
      this->overlay_state.unused1[3].load(),
      this->overlay_state.unused1[4].load()));
  lines.emplace_back(phosg::string_printf(
      "  trap_tile_colors_nte: %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32,
      this->overlay_state.trap_tile_colors_nte[0].load(),
      this->overlay_state.trap_tile_colors_nte[1].load(),
      this->overlay_state.trap_tile_colors_nte[2].load(),
      this->overlay_state.trap_tile_colors_nte[3].load(),
      this->overlay_state.trap_tile_colors_nte[4].load(),
      this->overlay_state.trap_tile_colors_nte[5].load(),
      this->overlay_state.trap_tile_colors_nte[6].load(),
      this->overlay_state.trap_tile_colors_nte[7].load(),
      this->overlay_state.trap_tile_colors_nte[8].load(),
      this->overlay_state.trap_tile_colors_nte[9].load(),
      this->overlay_state.trap_tile_colors_nte[10].load(),
      this->overlay_state.trap_tile_colors_nte[11].load(),
      this->overlay_state.trap_tile_colors_nte[12].load(),
      this->overlay_state.trap_tile_colors_nte[13].load(),
      this->overlay_state.trap_tile_colors_nte[14].load(),
      this->overlay_state.trap_tile_colors_nte[15].load()));
  lines.emplace_back(phosg::string_printf(
      "  trap_card_ids_nte: #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX #%04hX",
      this->overlay_state.trap_card_ids_nte[0].load(),
      this->overlay_state.trap_card_ids_nte[1].load(),
      this->overlay_state.trap_card_ids_nte[2].load(),
      this->overlay_state.trap_card_ids_nte[3].load(),
      this->overlay_state.trap_card_ids_nte[4].load(),
      this->overlay_state.trap_card_ids_nte[5].load(),
      this->overlay_state.trap_card_ids_nte[6].load(),
      this->overlay_state.trap_card_ids_nte[7].load(),
      this->overlay_state.trap_card_ids_nte[8].load(),
      this->overlay_state.trap_card_ids_nte[9].load(),
      this->overlay_state.trap_card_ids_nte[10].load(),
      this->overlay_state.trap_card_ids_nte[11].load(),
      this->overlay_state.trap_card_ids_nte[12].load(),
      this->overlay_state.trap_card_ids_nte[13].load(),
      this->overlay_state.trap_card_ids_nte[14].load(),
      this->overlay_state.trap_card_ids_nte[15].load()));

  lines.emplace_back("  default_rules: " + this->default_rules.str());
  lines.emplace_back("  name: " + this->name.decode(language));
  lines.emplace_back("  location_name: " + this->location_name.decode(language));
  lines.emplace_back("  quest_name: " + this->quest_name.decode(language));
  lines.emplace_back("  description: " + this->description.decode(language));
  lines.emplace_back(phosg::string_printf("  map_xy: %hu %hu", this->map_x.load(), this->map_y.load()));
  for (size_t z = 0; z < 3; z++) {
    lines.emplace_back(phosg::string_printf("  npc_chars[%zu]:", z));
    lines.emplace_back("    name: " + this->npc_ai_params[z].ai_name.decode(language));
    lines.emplace_back(phosg::string_printf(
        "    ai_params: (a1: %04hX %04hX, is_arkz: %02hhX, a2: %02hX %02hX %02hX)",
        this->npc_ai_params[z].unknown_a1[0].load(), this->npc_ai_params[z].unknown_a1[1].load(),
        this->npc_ai_params[z].is_arkz, this->npc_ai_params[z].unknown_a2[0],
        this->npc_ai_params[z].unknown_a2[1], this->npc_ai_params[z].unknown_a2[2]));
    for (size_t w = 0; w < 0x78; w += 0x08) {
      lines.emplace_back(phosg::string_printf(
          "    ai_params.a3[0x%02zX:0x%02zX]: %04hX %04hX %04hX %04hX %04hX %04hX %04hX %04hX",
          w, w + 0x08,
          this->npc_ai_params[z].params[w + 0x00].load(), this->npc_ai_params[z].params[w + 0x01].load(),
          this->npc_ai_params[z].params[w + 0x02].load(), this->npc_ai_params[z].params[w + 0x03].load(),
          this->npc_ai_params[z].params[w + 0x04].load(), this->npc_ai_params[z].params[w + 0x05].load(),
          this->npc_ai_params[z].params[w + 0x06].load(), this->npc_ai_params[z].params[w + 0x07].load()));
    }
    lines.emplace_back(phosg::string_printf(
        "    ai_params.a3[0x78:0x7E]: %04hX %04hX %04hX %04hX %04hX %04hX",
        this->npc_ai_params[z].params[0x78].load(), this->npc_ai_params[z].params[0x79].load(),
        this->npc_ai_params[z].params[0x7A].load(), this->npc_ai_params[z].params[0x7B].load(),
        this->npc_ai_params[z].params[0x7C].load(), this->npc_ai_params[z].params[0x7D].load()));
    lines.emplace_back(phosg::string_printf("  npc_decks[%zu]:", z));
    lines.emplace_back("    name: " + this->npc_decks[z].deck_name.decode(language));
    for (size_t w = 0; w < 0x20; w++) {
      uint16_t card_id = this->npc_decks[z].card_ids[w];
      shared_ptr<const CardIndex::CardEntry> entry;
      if (card_index) {
        try {
          entry = card_index->definition_for_id(card_id);
        } catch (const out_of_range&) {
        }
      }
      if (entry) {
        string name = entry->def.en_name.decode(language);
        lines.emplace_back(phosg::string_printf("    cards[%02zu]: #%04hX (%s)", w, card_id, name.c_str()));
      } else {
        lines.emplace_back(phosg::string_printf("    cards[%02zu]: #%04hX", w, card_id));
      }
    }
    for (size_t x = 0; x < 0x10; x++) {
      const auto& set = this->dialogue_sets[z][x];
      if (set.when == -1 && set.percent_chance == 0xFFFF) {
        continue;
      }
      lines.emplace_back(phosg::string_printf("  npc_dialogue[%zu][%zu] (when: %04hX, chance: %hu%%):",
          z, x, set.when.load(), set.percent_chance.load()));
      for (size_t w = 0; w < 4; w++) {
        if (!set.strings[w].empty() && set.strings[w].at(0) != 0xFF) {
          string s = set.strings[w].decode(language);
          lines.emplace_back(phosg::string_printf("    strings[%zu]: %s", w, s.c_str()));
        }
      }
    }
  }
  lines.emplace_back("  a7: " + phosg::format_data_string(this->unknown_a7.data(), this->unknown_a7.bytes()));
  lines.emplace_back(phosg::string_printf("  npc_ai_params_entry_index: [%08" PRIX32 ", %08" PRIX32 ", %08" PRIX32 "]",
      this->npc_ai_params_entry_index[0].load(), this->npc_ai_params_entry_index[1].load(), this->npc_ai_params_entry_index[2].load()));
  if (!this->before_message.empty()) {
    lines.emplace_back("  before_message: " + this->before_message.decode(language));
  }
  if (!this->after_message.empty()) {
    lines.emplace_back("  after_message: " + this->after_message.decode(language));
  }
  if (!this->dispatch_message.empty()) {
    lines.emplace_back("  dispatch_message: " + this->dispatch_message.decode(language));
  }
  for (size_t z = 0; z < 0x10; z++) {
    uint16_t card_id = this->reward_card_ids[z];
    shared_ptr<const CardIndex::CardEntry> entry;
    if (card_index) {
      try {
        entry = card_index->definition_for_id(card_id);
      } catch (const out_of_range&) {
      }
    }
    if (entry) {
      string name = entry->def.en_name.decode(language);
      lines.emplace_back(phosg::string_printf("  reward_cards[%02zu]: #%04hX (%s)", z, card_id, name.c_str()));
    } else {
      lines.emplace_back(phosg::string_printf("  reward_cards[%02zu]: #%04hX", z, card_id));
    }
  }
  lines.emplace_back(phosg::string_printf("  level_overrides: [win: %" PRId32 ", loss: %" PRId32 "]",
      this->win_level_override.load(), this->loss_level_override.load()));
  lines.emplace_back(phosg::string_printf("  field_offset: (x: %hd units, y:%hd units) (x: %lg tiles, y: %lg tiles)", this->field_offset_x.load(), this->field_offset_y.load(), static_cast<double>(this->field_offset_x) / 25.0, static_cast<double>(this->field_offset_y) / 25.0));
  lines.emplace_back(phosg::string_printf("  map_category: %02hhX", this->map_category));
  lines.emplace_back(phosg::string_printf("  cyber_block_type: %02hhX", this->cyber_block_type));
  lines.emplace_back(phosg::string_printf("  a11: %04hX", this->unknown_a11.load()));
  static const array<const char*, 0x18> sc_card_entry_names = {
      "00 (Guykild; 0005)",
      "01 (Kylria; 0006)",
      "02 (Saligun; 0110)",
      "03 (Relmitos; 0111)",
      "04 (Kranz; 0002)",
      "05 (Sil'fer; 0004)",
      "06 (Ino'lis; 0003)",
      "07 (Viviana; 0112)",
      "08 (Teifu; 0113)",
      "09 (Orland; 0001)",
      "0A (Stella; 0114)",
      "0B (Glustar; 0115)",
      "0C (Hyze; 0117)",
      "0D (Rufina; 0118)",
      "0E (Peko; 0119)",
      "0F (Creinu; 011A)",
      "10 (Reiz; 011B)",
      "11 (Lura; 0007)",
      "12 (Break; 0008)",
      "13 (Rio; 011C)",
      "14 (Endu; 0116)",
      "15 (Memoru; 011D)",
      "16 (K.C.; 011E)",
      "17 (Ohgun; 011F)",
  };
  string unavailable_sc_cards = "  unavailable_sc_cards: [";
  for (size_t z = 0; z < 0x18; z++) {
    if (this->unavailable_sc_cards[z] == 0xFFFF) {
      continue;
    }
    if (unavailable_sc_cards.size() > 25) {
      unavailable_sc_cards += ", ";
    }
    if (this->unavailable_sc_cards[z] >= sc_card_entry_names.size()) {
      unavailable_sc_cards += phosg::string_printf("%04hX (invalid)", this->unavailable_sc_cards[z].load());
    } else {
      unavailable_sc_cards += sc_card_entry_names[this->unavailable_sc_cards[z]];
    }
  }
  unavailable_sc_cards += ']';
  lines.emplace_back(std::move(unavailable_sc_cards));
  for (size_t z = 0; z < 4; z++) {
    string player_type;
    switch (this->entry_states[z].player_type) {
      case 0x00:
        player_type = "Player";
        break;
      case 0x01:
        player_type = "Player/COM";
        break;
      case 0x02:
        player_type = "COM";
        break;
      case 0x03:
        player_type = "FIXED_COM";
        break;
      case 0x04:
        player_type = "NONE";
        break;
      case 0xFF:
        player_type = "FREE";
        break;
      default:
        player_type = phosg::string_printf("(%02hhX)", this->entry_states[z].player_type);
        break;
    }
    string deck_type;
    switch (this->entry_states[z].deck_type) {
      case 0x00:
        deck_type = "HERO ONLY";
        break;
      case 0x01:
        deck_type = "DARK ONLY";
        break;
      case 0xFF:
        deck_type = "any deck allowed";
        break;
      default:
        deck_type = phosg::string_printf("(%02hhX)", this->entry_states[z].deck_type);
        break;
    }
    lines.emplace_back(phosg::string_printf(
        "  entry_states[%zu]: %s / %s", z, player_type.c_str(), deck_type.c_str()));
  }
  return phosg::join(lines, "\n");
}

MapDefinitionTrial::MapDefinitionTrial(const MapDefinition& map)
    : tag(map.tag),
      map_number(map.map_number),
      width(map.width),
      height(map.height),
      environment_number(map.environment_number),
      num_camera_zones(map.num_camera_zones),
      map_tiles(map.map_tiles),
      start_tile_definitions(map.start_tile_definitions),
      camera_zone_maps(map.camera_zone_maps),
      camera_zone_specs(map.camera_zone_specs),
      overview_specs(map.overview_specs),
      overlay_state(map.overlay_state),
      default_rules(map.default_rules),
      name(map.name),
      location_name(map.location_name),
      quest_name(map.quest_name),
      description(map.description),
      map_x(map.map_x),
      map_y(map.map_y),
      npc_decks(map.npc_decks),
      npc_ai_params(map.npc_ai_params),
      unknown_a7(map.unknown_a7),
      npc_ai_params_entry_index(map.npc_ai_params_entry_index),
      before_message(map.before_message),
      after_message(map.after_message),
      dispatch_message(map.dispatch_message),
      dialogue_sets(),
      reward_card_ids(map.reward_card_ids),
      win_level_override(map.win_level_override),
      loss_level_override(map.loss_level_override),
      field_offset_x(map.field_offset_x),
      field_offset_y(map.field_offset_y),
      map_category(map.map_category),
      cyber_block_type(map.cyber_block_type),
      unknown_a11(map.unknown_a11),
      unknown_t12(0xFF) {
  for (size_t z = 0; z < this->dialogue_sets.size(); z++) {
    this->dialogue_sets[z] = map.dialogue_sets[z].sub<8>(0);
  }

  // TODO: It'd be nice to rewrite start_tile_definitions, since it seems NTE
  // always expects team A to be represented by 3 and 4, and team B to be
  // represented by 7 and 8.

  // TODO: NTE also expects team A to always be facing up, and B to always be
  // facing down, so it would be nice to automatically rotate the map to make
  // that the case. However, we'd also have to fix up the camera zones and
  // camera specs, and the spec structure is not (yet) fully understood.
}

MapDefinitionTrial::operator MapDefinition() const {
  MapDefinition ret;
  // Trial Edition maps seem to have different tag values; we just always use
  // the value that the final version expects.
  ret.tag = 0x00000100;
  ret.map_number = this->map_number;
  ret.width = this->width;
  ret.height = this->height;
  ret.environment_number = this->environment_number;
  ret.num_camera_zones = this->num_camera_zones;
  ret.map_tiles = this->map_tiles;
  ret.start_tile_definitions = this->start_tile_definitions;
  ret.camera_zone_maps = this->camera_zone_maps;
  ret.camera_zone_specs = this->camera_zone_specs;
  ret.overview_specs = this->overview_specs;
  ret.overlay_state = this->overlay_state;
  ret.default_rules = this->default_rules;
  ret.name = this->name;
  ret.location_name = this->location_name;
  ret.quest_name = this->quest_name;
  ret.description = this->description;
  ret.map_x = this->map_x;
  ret.map_y = this->map_y;
  ret.npc_decks = this->npc_decks;
  ret.npc_ai_params = this->npc_ai_params;
  ret.unknown_a7 = this->unknown_a7;
  ret.npc_ai_params_entry_index = this->npc_ai_params_entry_index;
  ret.before_message = this->before_message;
  ret.after_message = this->after_message;
  ret.dispatch_message = this->dispatch_message;
  for (size_t z = 0; z < ret.dialogue_sets.size(); z++) {
    ret.dialogue_sets[z].sub<8>(0) = this->dialogue_sets[z];
    for (size_t x = 8; x < ret.dialogue_sets[z].size(); x++) {
      ret.dialogue_sets[z][x].when = -1;
      ret.dialogue_sets[z][x].percent_chance = 0xFFFF;
      for (size_t w = 0; w < 4; w++) {
        ret.dialogue_sets[z][x].strings[w].clear(0xFF);
        ret.dialogue_sets[z][x].strings[w].set_byte(0, 0);
      }
    }
  }
  ret.reward_card_ids = this->reward_card_ids;
  ret.win_level_override = this->win_level_override;
  ret.loss_level_override = this->loss_level_override;
  ret.field_offset_x = this->field_offset_x;
  ret.field_offset_y = this->field_offset_y;
  ret.map_category = this->map_category;
  ret.cyber_block_type = this->cyber_block_type;
  ret.unknown_a11 = this->unknown_a11;
  ret.unavailable_sc_cards.clear(0xFFFF);
  // The trial edition doesn't seem to have entry_states at all, so we have to
  // guess and fill in the field appropriately here.
  size_t num_npc_decks = 0;
  for (size_t z = 0; z < ret.npc_decks.size(); z++) {
    if (!ret.npc_decks[z].deck_name.empty()) {
      num_npc_decks++;
    }
  }
  for (size_t z = 0; z < 4; z++) {
    ret.entry_states[z].deck_type = 0xFF;
  }
  switch (num_npc_decks) {
    case 0: // No NPCs; it's a free battle map
      ret.entry_states[0].player_type = 0xFF;
      ret.entry_states[1].player_type = 0xFF;
      ret.entry_states[2].player_type = 0xFF;
      ret.entry_states[3].player_type = 0xFF;
      break;
    case 1: // One NPC; assume it's a 1v1 quest (Player vs. COM)
      ret.entry_states[0].player_type = 0x00;
      ret.entry_states[1].player_type = 0x04;
      ret.entry_states[2].player_type = 0x03;
      ret.entry_states[3].player_type = 0x04;
      break;
    case 2: // Two NPCs; assume it's a 2v2 quest (Player+Player/COM vs. COM+COM)
      ret.entry_states[0].player_type = 0x00;
      ret.entry_states[1].player_type = 0x02;
      ret.entry_states[2].player_type = 0x03;
      ret.entry_states[3].player_type = 0x03;
      break;
    case 3: // Three NPCs; assume it's a 2v2 quest (Player+COM vs. COM+COM)
      ret.entry_states[0].player_type = 0x00;
      ret.entry_states[1].player_type = 0x03;
      ret.entry_states[2].player_type = 0x03;
      ret.entry_states[3].player_type = 0x03;
      break;
    default: // Should be impossible
      throw logic_error("too many NPC decks in trial map definition");
  }
  return ret;
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
  if (this->min_dice_value > 9) {
    this->min_dice_value = 0;
    ret = true;
  }
  if (this->max_dice_value > 9) {
    this->max_dice_value = 0;
    ret = true;
  }
  if ((this->min_dice_value != 0) && (this->max_dice_value != 0) && (this->max_dice_value < this->min_dice_value)) {
    uint8_t t = this->min_dice_value;
    this->min_dice_value = this->max_dice_value;
    this->max_dice_value = t;
    ret = true;
  }

  // These ranges are a newserv-specific extension and are not part of the
  // original implementation.
  auto check_compressed_dice_range = +[](uint8_t* range) -> bool {
    bool ret = false;
    uint8_t min_dice = ((*range) >> 4) & 0x0F;
    uint8_t max_dice = (*range) & 0x0F;
    if (min_dice > 9) {
      min_dice = 0;
      ret = true;
    }
    if (max_dice > 9) {
      max_dice = 0;
      ret = true;
    }
    if ((min_dice != 0) && (max_dice != 0) && (max_dice < min_dice)) {
      uint8_t t = min_dice;
      min_dice = max_dice;
      max_dice = t;
      ret = true;
    }
    *range = ((min_dice << 4) & 0xF0) | (max_dice & 0x0F);
    return ret;
  };
  ret |= check_compressed_dice_range(&this->def_dice_value_range);
  ret |= check_compressed_dice_range(&this->atk_dice_value_range_2v1);
  ret |= check_compressed_dice_range(&this->def_dice_value_range_2v1);

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
  // Due to newserv's custom range overrides, it doesn't make sense to disable
  // Dice Boost for everyone based on the Rules struct. Instead, we skip setting
  // the flag at roll time.
  // if ((this->max_dice_value != 0) && (this->max_dice_value < 3)) {
  //   this->disable_dice_boost = 1;
  //   ret = true;
  // }
  return ret;
}

CardIndex::CardIndex(
    const string& filename,
    const string& decompressed_filename,
    const string& text_filename,
    const string& decompressed_text_filename,
    const string& dice_text_filename,
    const string& decompressed_dice_text_filename) {
  unordered_map<uint32_t, vector<string>> card_tags;
  unordered_map<uint32_t, string> card_text;
  try {
    string text_bin_data;
    if (!decompressed_text_filename.empty() && phosg::isfile(decompressed_text_filename)) {
      text_bin_data = phosg::load_file(decompressed_text_filename);
    } else if (!text_filename.empty() && phosg::isfile(text_filename)) {
      text_bin_data = prs_decompress(phosg::load_file(text_filename));
    }
    if (!text_bin_data.empty()) {
      phosg::StringReader r(text_bin_data);

      while (!r.eof()) {
        string card_id_str = r.get_cstr();
        if (card_id_str.empty() || (static_cast<uint8_t>(card_id_str[0]) == 0xFF)) {
          break;
        }
        phosg::strip_leading_whitespace(card_id_str);
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
        auto lines = phosg::split(first_page, '\n');
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
            tags.emplace_back(std::move(tag));
          }
        }
        phosg::strip_leading_whitespace(orig_text);

        if (!card_text.emplace(card_id, std::move(orig_text)).second) {
          throw runtime_error("duplicate card text id");
        }
        if (!card_tags.emplace(card_id, std::move(tags)).second) {
          throw logic_error("duplicate card tags id");
        }

        r.go((r.where() + 0x3FF) & (~0x3FF));
      }
    }
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load card text: %s", e.what());
  }

  unordered_map<uint32_t, pair<string, string>> card_dice_text;
  try {
    string text_bin_data;
    if (!decompressed_dice_text_filename.empty() && phosg::isfile(decompressed_dice_text_filename)) {
      text_bin_data = phosg::load_file(decompressed_dice_text_filename);
    } else if (!dice_text_filename.empty() && phosg::isfile(dice_text_filename)) {
      text_bin_data = prs_decompress(phosg::load_file(dice_text_filename));
    }
    if (!text_bin_data.empty()) {
      phosg::StringReader r(text_bin_data);

      while (!r.eof()) {
        uint32_t card_id = r.get_u32l();
        string dice_caption = r.read(0xFE);
        string dice_text = r.read(0xFE);
        phosg::strip_trailing_zeroes(dice_caption);
        phosg::strip_trailing_zeroes(dice_text);
        card_dice_text.emplace(card_id, make_pair(std::move(dice_caption), std::move(dice_text)));
      }
    }
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load card dice text: %s", e.what());
  }

  try {
    string decompressed_data;
    this->mtime_for_card_definitions = phosg::stat(filename).st_mtime;
    try {
      decompressed_data = phosg::load_file(decompressed_filename);
      this->compressed_card_definitions.clear();
    } catch (const phosg::cannot_open_file&) {
      this->compressed_card_definitions = phosg::load_file(filename);
      decompressed_data = prs_decompress(this->compressed_card_definitions);
    }

    // The client can't handle files larger than this
    if (decompressed_data.size() > 0x36EC0) {
      throw runtime_error("decompressed card list data is too long");
    }

    // The card definitions file is a standard REL file; the root offset points
    // to an ArrayRef which specifies an array of CardDefinition structs
    phosg::StringReader r(decompressed_data);
    const auto& footer = r.pget<RELFileFooterBE>(r.size() - sizeof(RELFileFooterBE));
    uint32_t offset = r.pget_u32b(footer.root_offset);
    uint32_t count = r.pget_u32b(footer.root_offset + 4);
    if (offset > decompressed_data.size() ||
        ((offset + count * sizeof(CardDefinition)) > decompressed_data.size())) {
      throw runtime_error("definitions array reference out of bounds");
    }
    CardDefinition* defs = reinterpret_cast<CardDefinition*>(decompressed_data.data() + offset);
    for (size_t x = 0; x < count; x++) {
      auto& def = defs[x];

      // The last card entry has the build date and some other metadata (and
      // isn't a real card, obviously), so skip it. The game detects this by
      // checking for a negative value in type, which we also do here.
      if (static_cast<int8_t>(def.type) < 0) {
        continue;
      }

      auto entry = make_shared<CardEntry>(CardEntry{def, "", "", "", {}});
      if (!this->card_definitions.emplace(entry->def.card_id, entry).second) {
        throw runtime_error(phosg::string_printf(
            "duplicate card id: %08" PRIX32, entry->def.card_id.load()));
      }

      // Some cards intentionally have the same name, so we just leave them
      // unindexed (they can still be looked up by ID, of course)
      string name = entry->def.en_name.decode(1);
      this->card_definitions_by_name.emplace(name, entry);
      this->card_definitions_by_name_normalized.emplace(this->normalize_card_name(name), entry);

      entry->def.hp.decode_code();
      entry->def.ap.decode_code();
      entry->def.tp.decode_code();
      entry->def.mv.decode_code();
      entry->def.decode_range();

      if (!text_filename.empty() || !decompressed_text_filename.empty()) {
        try {
          entry->text = std::move(card_text.at(def.card_id));
        } catch (const out_of_range&) {
        }
        try {
          entry->debug_tags = std::move(card_tags.at(def.card_id));
        } catch (const out_of_range&) {
        }
      }
      if (!dice_text_filename.empty() || !decompressed_dice_text_filename.empty()) {
        try {
          auto& dice_text_it = card_dice_text.at(def.card_id);
          entry->dice_caption = std::move(dice_text_it.first);
          entry->dice_text = std::move(dice_text_it.second);
        } catch (const out_of_range&) {
        }
      }
    }

    if (this->compressed_card_definitions.empty()) {
      uint64_t start = phosg::now();
      this->compressed_card_definitions = prs_compress(decompressed_data);
      uint64_t diff = phosg::now() - start;
      static_game_data_log.info(
          "Compressed card definitions (%zu bytes -> %zu bytes) in %" PRIu64 "us",
          decompressed_data.size(), this->compressed_card_definitions.size(), diff);
    }

    if (this->compressed_card_definitions.size() > 0x7BF8) {
      // Try to reduce the compressed size by clearing out text
      static_game_data_log.info("Compressed card list data is too long (0x%zX bytes); removing text", this->compressed_card_definitions.size());
      for (size_t x = 0; x < count; x++) {
        if (static_cast<int8_t>(defs[x].type) < 0) {
          continue;
        }
        defs[x].jp_name.clear();
        defs[x].jp_short_name.clear();
      }
      uint64_t start = phosg::now();
      this->compressed_card_definitions = prs_compress_optimal(decompressed_data.data(), decompressed_data.size());
      uint64_t diff = phosg::now() - start;
      static_game_data_log.info(
          "Compressed card definitions (0x%zX bytes -> 0x%zX bytes) in %" PRIu64 "us",
          decompressed_data.size(), this->compressed_card_definitions.size(), diff);
    }

    if (this->compressed_card_definitions.size() > 0x7BF8) {
      throw runtime_error("compressed card list data is too long");
    }

    static_game_data_log.info("Indexed %zu Episode 3 card definitions", this->card_definitions.size());
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load Episode 3 card update: %s", e.what());
  }
}

const string& CardIndex::get_compressed_definitions() const {
  if (this->compressed_card_definitions.empty()) {
    throw runtime_error("card definitions are not available");
  }
  return this->compressed_card_definitions;
}

shared_ptr<const CardIndex::CardEntry> CardIndex::definition_for_id(uint32_t id) const {
  return this->card_definitions.at(id);
}

shared_ptr<const CardIndex::CardEntry> CardIndex::definition_for_name(const string& name) const {
  return this->card_definitions_by_name.at(name);
}

shared_ptr<const CardIndex::CardEntry> CardIndex::definition_for_name_normalized(const string& name) const {
  return this->card_definitions_by_name_normalized.at(this->normalize_card_name(name));
}

set<uint32_t> CardIndex::all_ids() const {
  set<uint32_t> ret;
  for (const auto& it : this->card_definitions) {
    ret.emplace(it.first);
  }
  return ret;
}

uint64_t CardIndex::definitions_mtime() const {
  return this->mtime_for_card_definitions;
}

phosg::JSON CardIndex::definitions_json() const {
  phosg::JSON ret = phosg::JSON::dict();
  for (const auto& it : this->card_definitions_by_name) {
    ret.emplace(it.first, it.second->def.json());
  }
  return ret;
}

string CardIndex::normalize_card_name(const string& name) {
  string ret;
  for (char ch : name) {
    if (ch == ' ') {
      continue;
    }
    ret.push_back(tolower(ch));
  }
  return ret;
}

MapIndex::VersionedMap::VersionedMap(shared_ptr<const MapDefinition> map, uint8_t language)
    : map(map),
      language(language) {}

MapIndex::VersionedMap::VersionedMap(std::string&& compressed_data, uint8_t language)
    : language(language),
      compressed_data(std::move(compressed_data)) {
  string decompressed = prs_decompress(this->compressed_data);
  if (decompressed.size() == sizeof(MapDefinitionTrial)) {
    this->map = make_shared<MapDefinition>(*reinterpret_cast<const MapDefinitionTrial*>(decompressed.data()));
  } else if (decompressed.size() == sizeof(MapDefinition)) {
    this->map = make_shared<MapDefinition>(*reinterpret_cast<const MapDefinition*>(decompressed.data()));
  } else {
    throw runtime_error(phosg::string_printf(
        "decompressed data size is incorrect (expected %zu bytes, read %zu bytes)",
        sizeof(MapDefinition), decompressed.size()));
  }
}

shared_ptr<const MapDefinitionTrial> MapIndex::VersionedMap::trial() const {
  if (!this->trial_map) {
    this->trial_map = make_shared<MapDefinitionTrial>(*this->map);
  }
  return this->trial_map;
}

const std::string& MapIndex::VersionedMap::compressed(bool is_nte) const {
  if (is_nte) {
    if (this->compressed_trial_data.empty()) {
      auto md = this->trial();
      this->compressed_trial_data = prs_compress(md.get(), sizeof(*md));
    }
    return this->compressed_trial_data;
  } else {
    if (this->compressed_data.empty()) {
      this->compressed_data = prs_compress(this->map.get(), sizeof(*this->map));
    }
    return this->compressed_data;
  }
}

MapIndex::Map::Map(shared_ptr<const VersionedMap> initial_version)
    : map_number(initial_version->map->map_number),
      initial_version(initial_version) {
  this->versions.resize(this->initial_version->language + 1);
  this->versions[this->initial_version->language] = initial_version;
}

void MapIndex::Map::add_version(std::shared_ptr<const VersionedMap> vm) {
  if (this->versions.size() <= vm->language) {
    this->versions.resize(vm->language + 1);
  }
  if (this->versions[vm->language]) {
    throw runtime_error("map version already exists");
  }
  this->initial_version->map->assert_semantically_equivalent(*vm->map);
  this->versions[vm->language] = vm;
}

bool MapIndex::Map::has_version(uint8_t language) const {
  return (this->versions.size() > language) && !!this->versions[language];
}

shared_ptr<const MapIndex::VersionedMap> MapIndex::Map::version(uint8_t language) const {
  // If the requested language exists, return it
  if ((language < this->versions.size()) && this->versions[language]) {
    return this->versions[language];
  }
  // If English exists, return it
  if ((1 < this->versions.size()) && this->versions[1]) {
    return this->versions[1];
  }
  // Return the first version that exists
  for (const auto& vm : this->versions) {
    if (vm) {
      return vm;
    }
  }
  // This should never happen because Map cannot be constructed without an
  // initial_version
  throw logic_error("no map versions exist");
}

MapIndex::MapIndex(const string& directory) {
  for (const auto& filename : phosg::list_directory_sorted(directory)) {
    try {
      string base_filename;
      string compressed_data;
      shared_ptr<MapDefinition> decompressed_data;
      if (phosg::ends_with(filename, ".mnmd") || phosg::ends_with(filename, ".bind")) {
        decompressed_data = make_shared<MapDefinition>(phosg::load_object_file<MapDefinition>(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 5);
      } else if (phosg::ends_with(filename, ".mnm") || phosg::ends_with(filename, ".bin")) {
        compressed_data = phosg::load_file(directory + "/" + filename);
        base_filename = filename.substr(0, filename.size() - 4);
      } else if (phosg::ends_with(filename, ".bin.gci") || phosg::ends_with(filename, ".mnm.gci")) {
        compressed_data = decode_gci_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 8);
      } else if (phosg::ends_with(filename, ".gci")) {
        compressed_data = decode_gci_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 4);
      } else if (phosg::ends_with(filename, ".bin.vms") || phosg::ends_with(filename, ".mnm.vms")) {
        compressed_data = decode_vms_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 8);
      } else if (phosg::ends_with(filename, ".vms")) {
        compressed_data = decode_vms_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 4);
      } else if (phosg::ends_with(filename, ".bin.dlq") || phosg::ends_with(filename, ".mnm.dlq")) {
        compressed_data = decode_dlq_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 8);
      } else if (phosg::ends_with(filename, ".dlq")) {
        compressed_data = decode_dlq_data(phosg::load_file(directory + "/" + filename));
        base_filename = filename.substr(0, filename.size() - 4);
      } else {
        continue; // Silently skip file
      }

      if (base_filename.size() < 2) {
        throw runtime_error("filename too short for language code");
      }
      if (base_filename[base_filename.size() - 2] != '-') {
        throw runtime_error("language code not present");
      }
      uint8_t language = language_code_for_char(base_filename[base_filename.size() - 1]);

      shared_ptr<VersionedMap> vm;
      if (decompressed_data) {
        vm = make_shared<VersionedMap>(decompressed_data, language);
      } else if (!compressed_data.empty()) {
        vm = make_shared<VersionedMap>(std::move(compressed_data), language);
      } else {
        throw runtime_error("unknown map file format");
      }

      string name = vm->map->name.decode(vm->language);
      auto map_it = this->maps.find(vm->map->map_number);
      if (map_it == this->maps.end()) {
        map_it = this->maps.emplace(vm->map->map_number, make_shared<Map>(vm)).first;
        static_game_data_log.info("(%s) Created Episode 3 map %08" PRIX32 " %c (%s; %s)",
            filename.c_str(),
            vm->map->map_number.load(),
            char_for_language_code(vm->language),
            vm->map->is_quest() ? "quest" : "free",
            name.c_str());
      } else {
        map_it->second->add_version(vm);
        static_game_data_log.info("(%s) Added Episode 3 map version %08" PRIX32 " %c (%s; %s)",
            filename.c_str(),
            vm->map->map_number.load(),
            char_for_language_code(vm->language),
            vm->map->is_quest() ? "quest" : "free",
            name.c_str());
      }
      this->maps_by_name.emplace(vm->map->name.decode(vm->language), map_it->second);

    } catch (const exception& e) {
      static_game_data_log.warning("Failed to index Episode 3 map %s: %s",
          filename.c_str(), e.what());
    }
  }
}

const string& MapIndex::get_compressed_list(size_t num_players, uint8_t language) const {
  if (num_players == 0) {
    throw runtime_error("cannot generate map list for no players");
  }
  if (num_players > 4) {
    throw logic_error("player count is too high in map list generation");
  }

  if (language >= this->compressed_map_lists.size()) {
    this->compressed_map_lists.resize(language + 1);
  }
  string& compressed_map_list = this->compressed_map_lists[language].at(num_players - 1);
  if (compressed_map_list.empty()) {
    phosg::StringWriter entries_w;
    phosg::StringWriter strings_w;

    size_t num_maps = 0;
    for (const auto& map_it : this->maps) {
      auto vm = map_it.second->version(language);
      size_t map_num_players = 0;
      for (size_t z = 0; z < 4; z++) {
        uint8_t player_type = vm->map->entry_states[z].player_type;
        if (player_type == 0x00 || player_type == 0x01 || player_type == 0xFF) {
          map_num_players++;
        }
      }
      if (map_num_players < num_players) {
        continue;
      }

      MapList::Entry e;
      e.map_x = vm->map->map_x;
      e.map_y = vm->map->map_y;
      e.environment_number = vm->map->environment_number;
      e.map_number = vm->map->map_number.load();
      e.width = vm->map->width;
      e.height = vm->map->height;
      e.map_tiles = vm->map->map_tiles;
      e.modification_tiles = vm->map->overlay_state.tiles;

      e.name_offset = strings_w.size();
      strings_w.write(vm->map->name.data, vm->map->name.used_chars_8());
      strings_w.put_u8(0);
      e.location_name_offset = strings_w.size();
      strings_w.write(vm->map->location_name.data, vm->map->location_name.used_chars_8());
      strings_w.put_u8(0);
      e.quest_name_offset = strings_w.size();
      strings_w.write(vm->map->quest_name.data, vm->map->quest_name.used_chars_8());
      strings_w.put_u8(0);
      e.description_offset = strings_w.size();
      strings_w.write(vm->map->description.data, vm->map->description.used_chars_8());
      strings_w.put_u8(0);
      e.map_category = vm->map->map_category;

      entries_w.put(e);
      num_maps++;
    }

    MapList header;
    header.num_maps = num_maps;
    header.unknown_a1 = 0;
    header.strings_offset = entries_w.size();
    header.total_size = sizeof(MapList) + entries_w.size() + strings_w.size();

    PRSCompressor prs;
    prs.add(&header, sizeof(header));
    prs.add(entries_w.str());
    prs.add(strings_w.str());

    phosg::StringWriter compressed_w;
    compressed_w.put_u32b(prs.input_size());
    compressed_w.write(prs.close());
    compressed_map_list = std::move(compressed_w.str());
    if (compressed_map_list.size() > 0x7BEC) {
      throw runtime_error(phosg::string_printf("compressed map list for %zu players is too large (0x%zX bytes)", num_players, compressed_map_list.size()));
    }
    size_t decompressed_size = sizeof(header) + entries_w.size() + strings_w.size();
    static_game_data_log.info("Generated Episode 3 compressed map list for %zu player(s) (%zu maps; 0x%zX -> 0x%zX bytes)",
        num_players, num_maps, decompressed_size, compressed_map_list.size());
  }
  return compressed_map_list;
}

shared_ptr<const MapIndex::Map> MapIndex::for_number(uint32_t id) const {
  return this->maps.at(id);
}

shared_ptr<const MapIndex::Map> MapIndex::for_name(const string& name) const {
  return this->maps_by_name.at(name);
}

set<uint32_t> MapIndex::all_numbers() const {
  set<uint32_t> ret;
  for (const auto& it : this->maps) {
    ret.emplace(it.first);
  }
  return ret;
}

COMDeckIndex::COMDeckIndex(const string& filename) {
  try {
    auto json = phosg::JSON::parse(phosg::load_file(filename));
    for (const auto& def_json : json.as_list()) {
      auto& def = this->decks.emplace_back(make_shared<COMDeckDefinition>());
      def->index = this->decks.size() - 1;
      def->player_name = def_json->at(0).as_string();
      def->deck_name = def_json->at(1).as_string();
      auto card_ids_json = def_json->at(2);
      for (size_t z = 0; z < 0x1F; z++) {
        def->card_ids[z] = card_ids_json.at(z).as_int();
      }
      if (!this->decks_by_name.emplace(def->deck_name, def).second) {
        throw runtime_error("duplicate COM deck name: " + def->deck_name);
      }
    }
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load Episode 3 COM decks: %s", e.what());
  }
}

size_t COMDeckIndex::num_decks() const {
  return this->decks.size();
}

shared_ptr<const COMDeckDefinition> COMDeckIndex::deck_for_index(size_t which) const {
  return this->decks.at(which);
}

shared_ptr<const COMDeckDefinition> COMDeckIndex::deck_for_name(const string& which) const {
  return this->decks_by_name.at(which);
}

shared_ptr<const COMDeckDefinition> COMDeckIndex::random_deck() const {
  return this->decks[phosg::random_object<size_t>() % this->decks.size()];
}

} // namespace Episode3

template <>
Episode3::HPType phosg::enum_for_name<Episode3::HPType>(const char* name) {
  if (!strcmp(name, "DEFEAT_PLAYER")) {
    return Episode3::HPType::DEFEAT_PLAYER;
  } else if (!strcmp(name, "DEFEAT_TEAM")) {
    return Episode3::HPType::DEFEAT_TEAM;
  } else if (!strcmp(name, "COMMON_HP")) {
    return Episode3::HPType::COMMON_HP;
  } else {
    throw out_of_range("invalid HP type name");
  }
}

template <>
const char* phosg::name_for_enum<Episode3::HPType>(Episode3::HPType hp_type) {
  switch (hp_type) {
    case Episode3::HPType::DEFEAT_PLAYER:
      return "DEFEAT_PLAYER";
    case Episode3::HPType::DEFEAT_TEAM:
      return "DEFEAT_TEAM";
    case Episode3::HPType::COMMON_HP:
      return "COMMON_HP";
    default:
      throw out_of_range("invalid HP type");
  }
}

template <>
Episode3::DiceExchangeMode phosg::enum_for_name<Episode3::DiceExchangeMode>(const char* name) {
  if (!strcmp(name, "HIGH_ATK")) {
    return Episode3::DiceExchangeMode::HIGH_ATK;
  } else if (!strcmp(name, "HIGH_DEF")) {
    return Episode3::DiceExchangeMode::HIGH_DEF;
  } else if (!strcmp(name, "NONE")) {
    return Episode3::DiceExchangeMode::NONE;
  } else {
    throw out_of_range("invalid dice exchange mode name");
  }
}

template <>
const char* phosg::name_for_enum<Episode3::DiceExchangeMode>(Episode3::DiceExchangeMode dice_exchange_mode) {
  switch (dice_exchange_mode) {
    case Episode3::DiceExchangeMode::HIGH_ATK:
      return "HIGH_ATK";
    case Episode3::DiceExchangeMode::HIGH_DEF:
      return "HIGH_DEF";
    case Episode3::DiceExchangeMode::NONE:
      return "NONE";
    default:
      throw out_of_range("invalid dice exchange mode");
  }
}

template <>
Episode3::AllowedCards phosg::enum_for_name<Episode3::AllowedCards>(const char* name) {
  if (!strcmp(name, "ALL")) {
    return Episode3::AllowedCards::ALL;
  } else if (!strcmp(name, "N_ONLY")) {
    return Episode3::AllowedCards::N_ONLY;
  } else if (!strcmp(name, "N_R_ONLY")) {
    return Episode3::AllowedCards::N_R_ONLY;
  } else if (!strcmp(name, "N_R_S_ONLY")) {
    return Episode3::AllowedCards::N_R_S_ONLY;
  } else {
    throw out_of_range("invalid allowed cards name");
  }
}

template <>
const char* phosg::name_for_enum<Episode3::AllowedCards>(Episode3::AllowedCards allowed_cards) {
  switch (allowed_cards) {
    case Episode3::AllowedCards::ALL:
      return "ALL";
    case Episode3::AllowedCards::N_ONLY:
      return "N_ONLY";
    case Episode3::AllowedCards::N_R_ONLY:
      return "N_R_ONLY";
    case Episode3::AllowedCards::N_R_S_ONLY:
      return "N_R_S_ONLY";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::BattlePhase>(Episode3::BattlePhase phase) {
  switch (phase) {
    case Episode3::BattlePhase::INVALID_00:
      return "INVALID_00";
    case Episode3::BattlePhase::DICE:
      return "DICE";
    case Episode3::BattlePhase::SET:
      return "SET";
    case Episode3::BattlePhase::MOVE:
      return "MOVE";
    case Episode3::BattlePhase::ACTION:
      return "ACTION";
    case Episode3::BattlePhase::DRAW:
      return "DRAW";
    case Episode3::BattlePhase::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::SetupPhase>(Episode3::SetupPhase phase) {
  switch (phase) {
    case Episode3::SetupPhase::REGISTRATION:
      return "REGISTRATION";
    case Episode3::SetupPhase::STARTER_ROLLS:
      return "STARTER_ROLLS";
    case Episode3::SetupPhase::HAND_REDRAW_OPTION:
      return "HAND_REDRAW_OPTION";
    case Episode3::SetupPhase::MAIN_BATTLE:
      return "MAIN_BATTLE";
    case Episode3::SetupPhase::BATTLE_ENDED:
      return "BATTLE_ENDED";
    case Episode3::SetupPhase::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::RegistrationPhase>(Episode3::RegistrationPhase phase) {
  switch (phase) {
    case Episode3::RegistrationPhase::AWAITING_NUM_PLAYERS:
      return "AWAITING_NUM_PLAYERS";
    case Episode3::RegistrationPhase::AWAITING_PLAYERS:
      return "AWAITING_PLAYERS";
    case Episode3::RegistrationPhase::AWAITING_DECKS:
      return "AWAITING_DECKS";
    case Episode3::RegistrationPhase::REGISTERED:
      return "REGISTERED";
    case Episode3::RegistrationPhase::BATTLE_STARTED:
      return "BATTLE_STARTED";
    case Episode3::RegistrationPhase::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::ActionSubphase>(Episode3::ActionSubphase phase) {
  switch (phase) {
    case Episode3::ActionSubphase::ATTACK:
      return "ATTACK";
    case Episode3::ActionSubphase::DEFENSE:
      return "DEFENSE";
    case Episode3::ActionSubphase::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::AttackMedium>(Episode3::AttackMedium medium) {
  switch (medium) {
    case Episode3::AttackMedium::UNKNOWN:
      return "UNKNOWN";
    case Episode3::AttackMedium::PHYSICAL:
      return "PHYSICAL";
    case Episode3::AttackMedium::TECH:
      return "TECH";
    case Episode3::AttackMedium::UNKNOWN_03:
      return "UNKNOWN_03";
    case Episode3::AttackMedium::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::CriterionCode>(Episode3::CriterionCode code) {
  switch (code) {
    case Episode3::CriterionCode::NONE:
      return "NONE";
    case Episode3::CriterionCode::HU_CLASS_SC:
      return "HU_CLASS_SC";
    case Episode3::CriterionCode::RA_CLASS_SC:
      return "RA_CLASS_SC";
    case Episode3::CriterionCode::FO_CLASS_SC:
      return "FO_CLASS_SC";
    case Episode3::CriterionCode::SAME_TEAM:
      return "SAME_TEAM";
    case Episode3::CriterionCode::SAME_PLAYER:
      return "SAME_PLAYER";
    case Episode3::CriterionCode::SAME_TEAM_NOT_SAME_PLAYER:
      return "SAME_TEAM_NOT_SAME_PLAYER";
    case Episode3::CriterionCode::FC:
      return "FC";
    case Episode3::CriterionCode::NOT_SC:
      return "NOT_SC";
    case Episode3::CriterionCode::SC:
      return "SC";
    case Episode3::CriterionCode::HU_OR_RA_CLASS_SC:
      return "HU_OR_RA_CLASS_SC";
    case Episode3::CriterionCode::HUNTER_NON_ANDROID_SC:
      return "HUNTER_NON_ANDROID_SC";
    case Episode3::CriterionCode::HUNTER_HU_CLASS_MALE_SC:
      return "HUNTER_HU_CLASS_MALE_SC";
    case Episode3::CriterionCode::HUNTER_FEMALE_SC:
      return "HUNTER_FEMALE_SC";
    case Episode3::CriterionCode::HUNTER_NON_RA_CLASS_HUMAN_SC:
      return "HUNTER_NON_RA_CLASS_HUMAN_SC";
    case Episode3::CriterionCode::HUNTER_HU_CLASS_ANDROID_SC:
      return "HUNTER_HU_CLASS_ANDROID_SC";
    case Episode3::CriterionCode::HUNTER_NON_RA_CLASS_NON_NEWMAN_SC:
      return "HUNTER_NON_RA_CLASS_NON_NEWMAN_SC";
    case Episode3::CriterionCode::HUNTER_NON_NEWMAN_NON_FORCE_MALE_SC:
      return "HUNTER_NON_NEWMAN_NON_FORCE_MALE_SC";
    case Episode3::CriterionCode::HUNTER_HUNEWEARL_CLASS_SC:
      return "HUNTER_HUNEWEARL_CLASS_SC";
    case Episode3::CriterionCode::HUNTER_RA_CLASS_MALE_SC:
      return "HUNTER_RA_CLASS_MALE_SC";
    case Episode3::CriterionCode::HUNTER_RA_CLASS_FEMALE_SC:
      return "HUNTER_RA_CLASS_FEMALE_SC";
    case Episode3::CriterionCode::HUNTER_RA_OR_FO_CLASS_FEMALE_SC:
      return "HUNTER_RA_OR_FO_CLASS_FEMALE_SC";
    case Episode3::CriterionCode::HUNTER_HU_OR_RA_CLASS_HUMAN_SC:
      return "HUNTER_HU_OR_RA_CLASS_HUMAN_SC";
    case Episode3::CriterionCode::HUNTER_RA_CLASS_ANDROID_SC:
      return "HUNTER_RA_CLASS_ANDROID_SC";
    case Episode3::CriterionCode::HUNTER_FO_CLASS_FEMALE_SC:
      return "HUNTER_FO_CLASS_FEMALE_SC";
    case Episode3::CriterionCode::HUNTER_HUMAN_FEMALE_SC:
      return "HUNTER_HUMAN_FEMALE_SC";
    case Episode3::CriterionCode::HUNTER_ANDROID_SC:
      return "HUNTER_ANDROID_SC";
    case Episode3::CriterionCode::HU_OR_FO_CLASS_SC:
      return "HU_OR_FO_CLASS_SC";
    case Episode3::CriterionCode::RA_OR_FO_CLASS_SC:
      return "RA_OR_FO_CLASS_SC";
    case Episode3::CriterionCode::PHYSICAL_OR_UNKNOWN_ATTACK_MEDIUM:
      return "PHYSICAL_OR_UNKNOWN_ATTACK_MEDIUM";
    case Episode3::CriterionCode::TECH_OR_UNKNOWN_ATTACK_MEDIUM:
      return "TECH_OR_UNKNOWN_ATTACK_MEDIUM";
    case Episode3::CriterionCode::PHYSICAL_OR_TECH_OR_UNKNOWN_ATTACK_MEDIUM:
      return "PHYSICAL_OR_TECH_OR_UNKNOWN_ATTACK_MEDIUM";
    case Episode3::CriterionCode::NON_PHYSICAL_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC:
      return "NON_PHYSICAL_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC";
    case Episode3::CriterionCode::NON_PHYSICAL_NON_TECH_ATTACK_MEDIUM_NON_SC:
      return "NON_PHYSICAL_NON_TECH_ATTACK_MEDIUM_NON_SC";
    case Episode3::CriterionCode::NON_PHYSICAL_NON_TECH_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC:
      return "NON_PHYSICAL_NON_TECH_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC";
    default:
      return "__UNKNOWN__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::CardType>(Episode3::CardType type) {
  switch (type) {
    case Episode3::CardType::HUNTERS_SC:
      return "HUNTERS_SC";
    case Episode3::CardType::ARKZ_SC:
      return "ARKZ_SC";
    case Episode3::CardType::ITEM:
      return "ITEM";
    case Episode3::CardType::CREATURE:
      return "CREATURE";
    case Episode3::CardType::ACTION:
      return "ACTION";
    case Episode3::CardType::ASSIST:
      return "ASSIST";
    case Episode3::CardType::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__UNKNOWN__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::CardClass>(Episode3::CardClass cc) {
  switch (cc) {
    case Episode3::CardClass::HU_SC:
      return "HU_SC";
    case Episode3::CardClass::RA_SC:
      return "RA_SC";
    case Episode3::CardClass::FO_SC:
      return "FO_SC";
    case Episode3::CardClass::NATIVE_CREATURE:
      return "NATIVE_CREATURE";
    case Episode3::CardClass::A_BEAST_CREATURE:
      return "A_BEAST_CREATURE";
    case Episode3::CardClass::MACHINE_CREATURE:
      return "MACHINE_CREATURE";
    case Episode3::CardClass::DARK_CREATURE:
      return "DARK_CREATURE";
    case Episode3::CardClass::GUARD_ITEM:
      return "GUARD_ITEM";
    case Episode3::CardClass::MAG_ITEM:
      return "MAG_ITEM";
    case Episode3::CardClass::SWORD_ITEM:
      return "SWORD_ITEM";
    case Episode3::CardClass::GUN_ITEM:
      return "GUN_ITEM";
    case Episode3::CardClass::CANE_ITEM:
      return "CANE_ITEM";
    case Episode3::CardClass::ATTACK_ACTION:
      return "ATTACK_ACTION";
    case Episode3::CardClass::DEFENSE_ACTION:
      return "DEFENSE_ACTION";
    case Episode3::CardClass::TECH:
      return "TECH";
    case Episode3::CardClass::PHOTON_BLAST:
      return "PHOTON_BLAST";
    case Episode3::CardClass::CONNECT_ONLY_ATTACK_ACTION:
      return "CONNECT_ONLY_ATTACK_ACTION";
    case Episode3::CardClass::BOSS_ATTACK_ACTION:
      return "BOSS_ATTACK_ACTION";
    case Episode3::CardClass::BOSS_TECH:
      return "BOSS_TECH";
    case Episode3::CardClass::ASSIST:
      return "ASSIST";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::ConditionType>(Episode3::ConditionType cond_type) {
  try {
    return Episode3::description_for_condition_type.at(static_cast<size_t>(cond_type)).name;
  } catch (const out_of_range&) {
    return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::EffectWhen>(Episode3::EffectWhen when) {
  switch (when) {
    case Episode3::EffectWhen::NONE:
      return "NONE";
    case Episode3::EffectWhen::CARD_SET:
      return "CARD_SET";
    case Episode3::EffectWhen::AFTER_ANY_CARD_ATTACK:
      return "AFTER_ANY_CARD_ATTACK";
    case Episode3::EffectWhen::BEFORE_ANY_CARD_ATTACK:
      return "BEFORE_ANY_CARD_ATTACK";
    case Episode3::EffectWhen::BEFORE_DICE_PHASE_THIS_TEAM_TURN:
      return "BEFORE_DICE_PHASE_THIS_TEAM_TURN";
    case Episode3::EffectWhen::CARD_DESTROYED:
      return "CARD_DESTROYED";
    case Episode3::EffectWhen::AFTER_SET_PHASE:
      return "AFTER_SET_PHASE";
    case Episode3::EffectWhen::BEFORE_MOVE_PHASE:
      return "BEFORE_MOVE_PHASE";
    case Episode3::EffectWhen::UNKNOWN_0A:
      return "UNKNOWN_0A";
    case Episode3::EffectWhen::AFTER_ATTACK_TARGET_RESOLUTION:
      return "AFTER_ATTACK_TARGET_RESOLUTION";
    case Episode3::EffectWhen::AFTER_THIS_CARD_ATTACK:
      return "AFTER_THIS_CARD_ATTACK";
    case Episode3::EffectWhen::BEFORE_THIS_CARD_ATTACK:
      return "BEFORE_THIS_CARD_ATTACK";
    case Episode3::EffectWhen::BEFORE_ACT_PHASE:
      return "BEFORE_ACT_PHASE";
    case Episode3::EffectWhen::BEFORE_DRAW_PHASE:
      return "BEFORE_DRAW_PHASE";
    case Episode3::EffectWhen::AFTER_CARD_MOVE:
      return "AFTER_CARD_MOVE";
    case Episode3::EffectWhen::UNKNOWN_15:
      return "UNKNOWN_15";
    case Episode3::EffectWhen::AFTER_THIS_CARD_ATTACKED:
      return "AFTER_THIS_CARD_ATTACKED";
    case Episode3::EffectWhen::BEFORE_THIS_CARD_ATTACKED:
      return "BEFORE_THIS_CARD_ATTACKED";
    case Episode3::EffectWhen::AFTER_CREATURE_OR_HUNTER_SC_ATTACK:
      return "AFTER_CREATURE_OR_HUNTER_SC_ATTACK";
    case Episode3::EffectWhen::BEFORE_CREATURE_OR_HUNTER_SC_ATTACK:
      return "BEFORE_CREATURE_OR_HUNTER_SC_ATTACK";
    case Episode3::EffectWhen::UNKNOWN_22:
      return "UNKNOWN_22";
    case Episode3::EffectWhen::BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL:
      return "BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL";
    case Episode3::EffectWhen::UNKNOWN_29:
      return "UNKNOWN_29";
    case Episode3::EffectWhen::UNKNOWN_2A:
      return "UNKNOWN_2A";
    case Episode3::EffectWhen::UNKNOWN_2B:
      return "UNKNOWN_2B";
    case Episode3::EffectWhen::UNKNOWN_33:
      return "UNKNOWN_33";
    case Episode3::EffectWhen::UNKNOWN_34:
      return "UNKNOWN_34";
    case Episode3::EffectWhen::UNKNOWN_35:
      return "UNKNOWN_35";
    case Episode3::EffectWhen::ATTACK_STAT_OVERRIDES:
      return "ATTACK_STAT_OVERRIDES";
    case Episode3::EffectWhen::ATTACK_DAMAGE_ADJUSTMENT:
      return "ATTACK_DAMAGE_ADJUSTMENT";
    case Episode3::EffectWhen::DEFENSE_DAMAGE_ADJUSTMENT:
      return "DEFENSE_DAMAGE_ADJUSTMENT";
    case Episode3::EffectWhen::BEFORE_DICE_PHASE_ALL_TURNS_FINAL:
      return "BEFORE_DICE_PHASE_ALL_TURNS_FINAL";
    default:
      return "__INVALID__";
  }
}

template <>
const char* phosg::name_for_enum<Episode3::Direction>(Episode3::Direction d) {
  switch (d) {
    case Episode3::Direction::RIGHT:
      return "LEFT";
    case Episode3::Direction::UP:
      return "DOWN";
    case Episode3::Direction::LEFT:
      return "RIGHT";
    case Episode3::Direction::DOWN:
      return "UP";
    case Episode3::Direction::INVALID_FF:
      return "INVALID_FF";
    default:
      return "__INVALID__";
  }
}
