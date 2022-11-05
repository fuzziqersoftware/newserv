#include "Episode3.hh"

#include <stdint.h>

#include <array>
#include <phosg/Filesystem.hh>

#include "Loggers.hh"
#include "Compression.hh"
#include "Text.hh"

using namespace std;



static const vector<const char*> name_for_card_type({
  "HunterSC",
  "ArkzSC",
  "Item",
  "Creature",
  "Action",
  "Assist",
});

static const unordered_map<uint8_t, const char*> description_for_when({
  {0x01, "Set"}, // TODO: Is 01 this, or "Permanent"?
  {0x02, "Attack"},
  {0x03, "??? (TODO)"},
  {0x04, "Before turn"},
  {0x05, "Destroyed"},
  {0x0A, "Permanent"}, // only used on Tollaw; could be same as 01
  {0x0B, "Battle"},
  {0x0C, "Opponent destroyed"}, // TODO: but this is also used for some support things like Shifta, and for Snatch, which also applies when opponents are not destroyed
  {0x0D, "Attack lands"},
  {0x0E, "Before attack phase"},
  {0x16, "Battle end"},
  {0x17, "Each defense"},
  {0x20, "Each attack"},
  {0x22, "Act phase"},
  {0x27, "Move phase"},
  {0x29, "Set and act phases"},
  {0x33, "Defense phase"},
  {0x3D, "Battle"}, // TODO: how is this different from 3D and 0B?
  {0x3E, "Battle"}, // TODO: how is this different from 3D and 0B?
  {0x3F, "Each defense"}, // TODO: how is this different from 17?
  {0x46, "On specific turn"},
});

static const unordered_map<string, const char*> description_for_expr_token({
  {"f",   "Number of FCs controlled by current SC"},
  {"d",   "Die roll"},
  {"ap",  "Attacker AP"}, // Unused
  {"tp",  "Attacker TP"},
  {"hp",  "Attacker HP"}, // TODO: How is this different from ehp?
  {"mhp", "Attacker maximum HP"},
  {"dm",  "Unknown: dm"}, // Unused
  {"tdm", "Unknown: tdm"}, // Unused
  {"tf",  "Number of SC\'s destroyed FCs"},
  {"ac",  "Remaining ATK points"},
  {"php", "Unknown: php"}, // Unused
  {"dc",  "Unknown: dc"}, // Unused
  {"cs",  "Unknown: cs"}, // Unused
  {"a",   "Unknown: a"}, // Unused
  {"kap", "Action cards AP"},
  {"ktp", "Action cards TP"},
  {"dn",  "Unknown: dn"}, // Unused
  {"hf",  "Unknown: hf"}, // Unused
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
  {"tt",  "Unknown: tt"}, // Unused
  {"lv",  "Dice bonus"},
  {"adm", "Attack damage"},
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
// certain situations) or otherwise customizing their results.
// Argument meanings:
// a01 = ???
// cXY/CXY = linked items (require item with cYX/CYX to be equipped as well)
// dXY = roll one die; require result between X and Y inclusive
// e00 = effect lasts while equipped? (in contrast to tXX)
// hXX = require HP >= XX
// iXX = require HP <= XX
// nXX = require condition XX (see description_for_n_condition)
// oXX = seems to be "require previous random-condition effect to have happened"
//       TODO: this is used as both o01 (recovery) and o11 (reflection)
//             conditions - why the difference?
// pXX = who to target (see description_for_p_target)
// rXX = randomly pass with XX% chance (if XX == 00, 100% chance?)
// sXY = require card cost between X and Y ATK points (inclusive)
// tXX = lasts XX turns, or activate after XX turns

static const vector<const char*> description_for_n_condition({
  /* n00 */ "Always true",
  /* n01 */ "??? (TODO)",
  /* n02 */ "Destroyed with a single attack?",
  /* n03 */ "Unknown", // Unused
  /* n04 */ "Attack has Pierce",
  /* n05 */ "Attack has Rampage",
  /* n06 */ "Native attribute",
  /* n07 */ "A.Beast attribute",
  /* n08 */ "Machine attribute",
  /* n09 */ "Dark attribute",
  /* n10 */ "Sword-type item",
  /* n11 */ "Gun-type item",
  /* n12 */ "Cane-type item",
  /* n13 */ "Guard item",
  /* n14 */ "Story Character",
  /* n15 */ "Attacker does not use action cards",
  /* n16 */ "Aerial attribute",
  /* n17 */ "Same AP as opponent",
  /* n18 */ "Opponent is SC",
  /* n19 */ "Has Paralyzed condition",
  /* n20 */ "Has Frozen condition",
});

static const vector<const char*> description_for_p_target({
  /* p00 */ "Unknown: p00", // Unused; probably invalid
  /* p01 */ "SC / FC who set the card",
  /* p02 */ "Attacking SC / FC",
  /* p03 */ "Unknown: p03", // Unused
  /* p04 */ "Unknown: p04", // Unused
  /* p05 */ "Unknown: p05", // Unused
  /* p06 */ "??? (TODO)",
  /* p07 */ "??? (TODO; Weakness)",
  /* p08 */ "FC's owner SC",
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

struct Ep3AbilityDescription {
  uint8_t command;
  bool has_expr;
  const char* name;
  const char* description;
};

static const std::vector<Ep3AbilityDescription> name_for_effect_command({
  {0x00, false, nullptr, nullptr},
  {0x01, true,  "AP Boost", "Temporarily increase AP by N"},
  {0x02, false, "Rampage", "Rampage"},
  {0x03, true,  "Multi Strike", "Duplicate attack N times"},
  {0x04, true,  "Damage Modifier 1", "Set attack damage / AP to N after action cards applied (step 1)"},
  {0x05, false, "Immobile", "Give Immobile condition"},
  {0x06, false, "Hold", "Give Hold condition"},
  {0x07, false, nullptr, nullptr},
  {0x08, true,  "TP Boost", "Add N TP temporarily during attack"},
  {0x09, true,  "Give Damage", "Cause direct N HP loss"},
  {0x0A, false, "Guom", "Give Guom condition"},
  {0x0B, false, "Paralyze", "Give Paralysis condition"},
  {0x0C, false, nullptr, nullptr},
  {0x0D, false, "A/H Swap", "Swap AP and HP temporarily"},
  {0x0E, false, "Pierce", "Attack SC directly even if they have items equipped"},
  {0x0F, false, nullptr, nullptr},
  {0x10, true,  "Heal", "Increase HP by N"},
  {0x11, false, "Return to Hand", "Return card to hand"},
  {0x12, false, nullptr, nullptr},
  {0x13, false, nullptr, nullptr},
  {0x14, false, "Acid", "Give Acid condition"},
  {0x15, false, nullptr, nullptr},
  {0x16, true,  "Mighty Knuckle", "Temporarily increase AP by N, and set ATK dice to zero"},
  {0x17, true,  "Unit Blow", "Temporarily increase AP by N * number of this card set within phase"},
  {0x18, false, "Curse", "Give Curse condition"},
  {0x19, false, "Combo (AP)", "Temporarily increase AP by number of this card set within phase"},
  {0x1A, false, "Pierce/Rampage Block", "Block attack if Pierce/Rampage (?)"},
  {0x1B, false, "Ability Trap", "Temporarily disable opponent abilities"},
  {0x1C, false, "Freeze", "Give Freeze condition"},
  {0x1D, false, "Anti-Abnormality", "Cure all conditions"},
  {0x1E, false, nullptr, nullptr},
  {0x1F, false, "Explosion", "Damage all SCs and FCs by number of this same card set * 2"},
  {0x20, false, nullptr, nullptr},
  {0x21, false, nullptr, nullptr},
  {0x22, false, nullptr, nullptr},
  {0x23, false, "Return to Deck", "Cancel discard and move to bottom of deck instead"},
  {0x24, false, "Aerial", "Give Aerial status"},
  {0x25, true,  "AP Loss", "Make attacker temporarily lose N AP during defense"},
  {0x26, true,  "Bonus From Leader", "Gain AP equal to the number of cards of type N on the field"},
  {0x27, false, "Free Maneuver", "Enable movement over occupied tiles"},
  {0x28, false, "Haste", "Make move actions free"},
  {0x29, true,  "Clone", "Make setting this card free if at least one card of type N is already on the field"},
  {0x2A, true,  "DEF Disable by Cost", "Disable use of any defense cards costing between (N / 10) and (N % 10) points, inclusive"},
  {0x2B, true,  "Filial", "Increase controlling SC\'s HP by N when this card is destroyed"},
  {0x2C, true,  "Snatch", "Steal N EXP during attack"},
  {0x2D, true,  "Hand Disrupter", "DIscard N cards from hand immediately"},
  {0x2E, false, "Drop", "Give Drop condition"},
  {0x2F, false, "Action Disrupter", "Destroy all action cards used by attacker"},
  {0x30, true,  "Set HP", "Set HP to N (?) (TODO)"},
  {0x31, false, "Native Shield", "Block attacks from Native creatures"},
  {0x32, false, "A.Beast Shield", "Block attacks from A.Beast creatures"},
  {0x33, false, "Machine Shield", "Block attacks from Machine creatures"},
  {0x34, false, "Dark Shield", "Block attacks from Dark creatures"},
  {0x35, false, "Sword Shield", "Block attacks from Sword items"},
  {0x36, false, "Gun Shield", "Block attacks from Gun items"},
  {0x37, false, "Cane Shield", "Block attacks from Cane items"},
  {0x38, false, nullptr, nullptr},
  {0x39, false, nullptr, nullptr},
  {0x3A, false, "Defender", "Make attacks go to setter of this card instead of original target"},
  {0x3B, false, "Survival Decoys", "Redirect damage for multi-sided attack"},
  {0x3C, true,  "Give/Take EXP", "Give N EXP, or take if N is negative"},
  {0x3D, false, nullptr, nullptr},
  {0x3E, false, "Death Companion", "If this card has 1 or 2 HP, set its HP to N"},
  {0x3F, true,  "EXP Decoy", "If defender has EXP, lose EXP instead of getting damage when attacked"},
  {0x40, true,  "Set MV", "Set MV to N"},
  {0x41, true,  "Group", "Temporarily increase AP by N * number of this card on field, excluding itself"},
  {0x42, false, "Berserk", "User of this card receives the same damage as target, and isn't helped by target's defense cards"},
  {0x43, false, "Guard Creature", "Attacks on controlling SC damage this card instead"},
  {0x44, false, "Tech", "Technique cards cost 1 fewer ATK point"},
  {0x45, false, "Big Swing", "Increase all attacking ATK costs by 1"},
  {0x46, false, nullptr, nullptr},
  {0x47, false, "Shield Weapon", "Limit attacker\'s choice of target to guard items"},
  {0x48, false, "ATK Dice Boost", "Increase ATK dice roll by 1"},
  {0x49, false, nullptr, nullptr},
  {0x4A, false, "Major Pierce", "If SC has over half of max HP, attacks target SC instead of equipped items"},
  {0x4B, false, "Heavy Pierce", "If SC has 3 or more items equipped, attacks target SC instead of equipped items"},
  {0x4C, false, "Major Rampage", "If SC has over half of max HP, attacks target SC and all equipped items"},
  {0x4D, false, "Heavy Rampage", "If SC has 3 or more items equipped, attacks target SC and all equipped items"},
  {0x4E, true,  "AP Growth", "Permanently increase AP by N"},
  {0x4F, true,  "TP Growth", "Permanently increase TP by N"},
  {0x50, true,  "Reborn", "If any card of type N is on the field, this card goes to the hand when destroyed instead of being discarded"},
  {0x51, true,  "Copy", "Temporarily set AP/TP to N percent (or 100% if N is 0) of opponent\'s values"},
  {0x52, false, nullptr, nullptr},
  {0x53, true,  "Misc. Guards", "Add N to card's defense value"},
  {0x54, true,  "AP Override", "Set AP to N temporarily"},
  {0x55, true,  "TP Override", "Set TP to N temporarily"},
  {0x56, false, "Return", "Return card to hand on destruction instead of discarding"},
  {0x57, false, "A/T Swap Perm", "Permanently swap AP and TP"},
  {0x58, false, "A/H Swap Perm", "Permanently swap AP and HP"},
  {0x59, true,  "Slayers/Assassins", "Temporarily increase AP during attack"},
  {0x5A, false, "Anti-Abnormality", "Remove all conditions"},
  {0x5B, false, "Fixed Range", "Use SC\'s range instead of weapon or attack card ranges"},
  {0x5C, false, "Elude", "SC does not lose HP when equipped items are destroyed"},
  {0x5D, false, "Parry", "Forward attack to a random FC within one tile of original target, excluding attacker and original target"},
  {0x5E, false, "Block Attack", "Completely block attack"},
  {0x5F, false, nullptr, nullptr},
  {0x60, false, nullptr, nullptr},
  {0x61, true,  "Combo (TP)", "Gain TP equal to the number of cards of type N on the field"},
  {0x62, true,  "Misc. AP Bonuses", "Temporarily increase AP by N"},
  {0x63, true,  "Misc. TP Bonuses", "Temporarily increase TP by N"},
  {0x64, false, nullptr, nullptr},
  {0x65, true,  "Misc. Defense Bonuses", "Decrease damage by N"},
  {0x66, true,  "Mostly Halfguards", "Reduce damage from incoming attack by N"},
  {0x67, false, "Periodic Field", "Swap immunity to tech or physical attacks"},
  {0x68, false, "Unlimited Summoning", "Allow unlimited summoning"},
  {0x69, false, nullptr, nullptr},
  {0x6A, true,  "MV Bonus", "Increase MV by N"},
  {0x6B, true,  "Forward Damage", "Give N damage back to attacker during defense (?) (TODO)"},
  {0x6C, true,  "Weak Spot / Influence", "Temporarily decrease AP by N"},
  {0x6D, true,  "Damage Modifier 2", "Set attack damage / AP after action cards applied (step 2)"},
  {0x6E, true,  "Weak Hit Block", "Block all attacks of N damage or less"},
  {0x6F, true,  "AP Silence", "Temporarily decrease AP of opponent by N"},
  {0x70, true,  "TP Silence", "Temporarily decrease TP of opponent by N"},
  {0x71, false, "A/T Swap", "Temporarily swap AP and TP"},
  {0x72, true,  "Halfguard", "Halve damage from attacks that would inflict N or more damage"},
  {0x73, false, nullptr, nullptr},
  {0x74, true,  "Rampage AP Loss", "Temporarily reduce AP by N"},
  {0x75, false, nullptr, nullptr},
  {0x76, false, "Reflect", "Generate reverse attack"},
});

void Ep3CardDefinition::Stat::decode_code() {
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

string Ep3CardDefinition::Stat::str() const {
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



bool Ep3CardDefinition::Effect::is_empty() const {
  return (this->command == 0 &&
          this->expr.is_filled_with(0) &&
          this->when == 0 &&
          this->arg1.is_filled_with(0) &&
          this->arg2.is_filled_with(0) &&
          this->arg3.is_filled_with(0) &&
          this->unknown_a3.is_filled_with(0));
}

string Ep3CardDefinition::Effect::str_for_arg(const std::string& arg) {
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

string Ep3CardDefinition::Effect::str() const {
  string cmd_str = string_printf("(%hhu) %02hhX", this->effect_num, this->command);
  try {
    const char* name = name_for_effect_command.at(this->command).name;
    if (name) {
      cmd_str += ':';
      cmd_str += name;
    }
  } catch (const out_of_range&) { }

  string when_str = string_printf("%02hhX", this->when);
  try {
    const char* name = description_for_when.at(this->when);
    if (name) {
      when_str += ':';
      when_str += name;
    }
  } catch (const out_of_range&) { }

  string expr_str = this->expr;
  if (!expr_str.empty()) {
    expr_str = ", expr=" + expr_str;
  }

  string arg1str = this->str_for_arg(this->arg1);
  string arg2str = this->str_for_arg(this->arg2);
  string arg3str = this->str_for_arg(this->arg3);
  string a3str = format_data_string(this->unknown_a3.data(), sizeof(this->unknown_a3));
  return string_printf("(cmd=%s%s, when=%s, arg1=%s, arg2=%s, arg3=%s, a3=%s)",
      cmd_str.c_str(), expr_str.c_str(), when_str.c_str(), arg1str.data(),
      arg2str.data(), arg3str.data(), a3str.c_str());
}



void Ep3CardDefinition::decode_range() {
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

string name_for_rarity(uint8_t rarity) {
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
    return names.at(rarity - 1);
  } catch (const out_of_range&) {
    return string_printf("(%02hhX)", rarity);
  }
}

string name_for_target_mode(uint8_t target_mode) {
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
    return names.at(target_mode);
  } catch (const out_of_range&) {
    return string_printf("(%02hhX)", target_mode);
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

string Ep3CardDefinition::str() const {
  string type_str;
  try {
    type_str = name_for_card_type.at(this->type);
  } catch (const out_of_range&) {
    type_str = string_printf("%02hhX", this->type);
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
      "[Card: %04" PRIX32 " name=%s type=%s-%02hhX rare=%s cost=%hhX+%hhX "
      "target=%s range=%s assist_turns=%s cannot_move=%s cannot_attack=%s "
      "hidden=%s hp=%s ap=%s tp=%s mv=%s left=%s right=%s top=%s a2=%04hX "
      "a3=%04hX assist_effect=[%hu, %hu] drop_rates=[%hu, %hu] effects=[%s]]",
      this->card_id.load(),
      this->name.data(),
      type_str.c_str(),
      this->subtype,
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
      this->unknown_a3.load(),
      this->assist_effect[0].load(),
      this->assist_effect[1].load(),
      this->drop_rates[0].load(),
      this->drop_rates[1].load(),
      effects_str.c_str());
}



Ep3DataIndex::Ep3DataIndex(const string& directory, bool debug)
  : debug(debug) {

  unordered_map<uint32_t, vector<string>> card_tags;
  unordered_map<uint32_t, string> card_text;
  if (this->debug) {
    try {
      string data = prs_decompress(load_file(directory + "/cardtext.mnr"));
      StringReader r(data);

      while (!r.eof()) {
        uint32_t card_id = stoul(r.get_cstr());

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
    this->compressed_card_definitions = load_file(directory + "/cardupdate.mnr");
    string data = prs_decompress(this->compressed_card_definitions);
    // There's a footer after the card definitions, but we ignore it
    if (data.size() % sizeof(Ep3CardDefinition) != sizeof(Ep3CardDefinitionsFooter)) {
      throw runtime_error(string_printf(
          "decompressed card update file size %zX is not aligned with card definition size %zX (%zX extra bytes)",
          data.size(), sizeof(Ep3CardDefinition), data.size() % sizeof(Ep3CardDefinition)));
    }
    const auto* def = reinterpret_cast<const Ep3CardDefinition*>(data.data());
    size_t max_cards = data.size() / sizeof(Ep3CardDefinition);
    for (size_t x = 0; x < max_cards; x++) {
      // The last card entry has the build date and some other metadata (and
      // isn't a real card, obviously), so skip it. Seems like the card ID is
      // always a large number that won't fit in a uint16_t, so we use that to
      // determine if the entry is a real card or not.
      if (def[x].card_id & 0xFFFF0000) {
        continue;
      }
      shared_ptr<CardEntry> entry(new CardEntry({def[x], {}, {}}));
      if (!this->card_definitions.emplace(entry->def.card_id, entry).second) {
        throw runtime_error(string_printf(
            "duplicate card id: %08" PRIX32, entry->def.card_id.load()));
      }

      entry->def.hp.decode_code();
      entry->def.ap.decode_code();
      entry->def.tp.decode_code();
      entry->def.mv.decode_code();
      entry->def.decode_range();

      if (this->debug) {
        try {
          entry->text = move(card_text.at(def[x].card_id));
        } catch (const out_of_range&) { }
        try {
          entry->debug_tags = move(card_tags.at(def[x].card_id));
        } catch (const out_of_range&) { }
      }
    }

    static_game_data_log.info("Indexed %zu Episode 3 card definitions", this->card_definitions.size());
  } catch (const exception& e) {
    static_game_data_log.warning("Failed to load Episode 3 card update: %s", e.what());
  }

  for (const auto& filename : list_directory(directory)) {
    try {
      shared_ptr<MapEntry> entry;

      if (ends_with(filename, ".mnmd")) {
        entry.reset(new MapEntry(load_object_file<Ep3Map>(directory + "/" + filename)));
      } else if (ends_with(filename, ".mnm")) {
        entry.reset(new MapEntry(load_file(directory + "/" + filename)));
      }

      if (entry.get()) {
        if (!this->maps.emplace(entry->map.map_number, entry).second) {
          throw runtime_error("duplicate map number");
        }
        string name = entry->map.name;
        static_game_data_log.info("Indexed Episode 3 map %s (%08" PRIX32 "; %s)",
            filename.c_str(), entry->map.map_number.load(), name.c_str());
      }

    } catch (const exception& e) {
      static_game_data_log.warning("Failed to index Episode 3 map %s: %s",
          filename.c_str(), e.what());
    }
  }
}

Ep3DataIndex::MapEntry::MapEntry(const Ep3Map& map) : map(map) { }

Ep3DataIndex::MapEntry::MapEntry(const string& compressed)
  : compressed_data(compressed) {
  string decompressed = prs_decompress(this->compressed_data);
  if (decompressed.size() != sizeof(Ep3Map)) {
    throw runtime_error(string_printf(
        "decompressed data size is incorrect (expected %zu bytes, read %zu bytes)",
        sizeof(Ep3Map), decompressed.size()));
  }
  this->map = *reinterpret_cast<const Ep3Map*>(decompressed.data());
}

string Ep3DataIndex::MapEntry::compressed() const {
  if (this->compressed_data.empty()) {
    this->compressed_data = prs_compress(&this->map, sizeof(this->map));
  }
  return this->compressed_data;
}

const string& Ep3DataIndex::get_compressed_card_definitions() const {
  if (this->compressed_card_definitions.empty()) {
    throw runtime_error("card definitions are not available");
  }
  return this->compressed_card_definitions;
}

shared_ptr<const Ep3DataIndex::CardEntry> Ep3DataIndex::get_card_definition(
    uint32_t id) const {
  return this->card_definitions.at(id);
}

std::set<uint32_t> Ep3DataIndex::all_card_ids() const {
  std::set<uint32_t> ret;
  for (const auto& it : this->card_definitions) {
    ret.emplace(it.first);
  }
  return ret;
}

const string& Ep3DataIndex::get_compressed_map_list() const {
  if (this->compressed_map_list.empty()) {
    // TODO: Write a version of prs_compress that takes iovecs (or something
    // similar) so we can eliminate all this string copying here.
    StringWriter entries_w;
    StringWriter strings_w;

    for (const auto& map_it : this->maps) {
      Ep3MapList::Entry e;
      const auto& map = map_it.second->map;
      e.map_x = map.map_x;
      e.map_y = map.map_y;
      e.scene_data2 = map.scene_data2;
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

      e.unknown_a2 = 0xFF000000;

      entries_w.put(e);
    }

    Ep3MapList header;
    header.num_maps = this->maps.size();
    header.unknown_a1 = 0;
    header.strings_offset = entries_w.size();
    header.total_size = sizeof(Ep3MapList) + entries_w.size() + strings_w.size();

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

shared_ptr<const Ep3DataIndex::MapEntry> Ep3DataIndex::get_map(uint32_t id) const {
  return this->maps.at(id);
}

std::set<uint32_t> Ep3DataIndex::all_map_ids() const {
  std::set<uint32_t> ret;
  for (const auto& it : this->maps) {
    ret.emplace(it.first);
  }
  return ret;
}
