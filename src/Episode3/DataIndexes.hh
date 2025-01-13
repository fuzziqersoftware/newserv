#pragma once

#include <stdint.h>

#include <array>
#include <map>
#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <phosg/Tools.hh>
#include <phosg/Types.hh>
#include <set>
#include <string>
#include <unordered_map>

#include "../CommonFileFormats.hh"
#include "../PlayerSubordinates.hh"
#include "../Text.hh"
#include "../TextIndex.hh"
#include "../Types.hh"

namespace Episode3 {

class CardIndex;
class MapIndex;
class COMDeckIndex;

const char* name_for_environment_number(uint8_t environment_number);
const char* name_for_link_color(uint8_t color);

enum BehaviorFlag : uint32_t {
  SKIP_DECK_VERIFY = 0x00000001,
  IGNORE_CARD_COUNTS = 0x00000002,
  SKIP_D1_D2_REPLACE = 0x00000004,
  DISABLE_TIME_LIMITS = 0x00000008,
  ENABLE_STATUS_MESSAGES = 0x00000010,
  ENABLE_RECORDING = 0x00000040,
  DISABLE_MASKING = 0x00000080,
  DISABLE_INTERFERENCE = 0x00000100,
  ALLOW_NON_COM_INTERFERENCE = 0x00000200,
  IS_TRIAL_EDITION = 0x00000400,
  LOG_COMMANDS_IF_LOBBY_MISSING = 0x00000800,
};

enum class StatSwapType : uint8_t {
  NONE = 0,
  A_T_SWAP = 1,
  A_H_SWAP = 2,
};

enum class ActionType : uint8_t {
  INVALID_00 = 0,
  DEFENSE = 1,
  ATTACK = 2,
};

enum class AttackMedium : uint8_t {
  UNKNOWN = 0,
  PHYSICAL = 1,
  TECH = 2,
  UNKNOWN_03 = 3, // Probably Resta
  INVALID_FF = 0xFF,
};

enum class CriterionCode : uint8_t {
  NONE = 0x00,
  HU_CLASS_SC = 0x01,
  RA_CLASS_SC = 0x02,
  FO_CLASS_SC = 0x03,
  SAME_TEAM = 0x04,
  SAME_PLAYER = 0x05,
  SAME_TEAM_NOT_SAME_PLAYER = 0x06, // Allies only
  FC = 0x07,
  NOT_SC = 0x08,
  SC = 0x09,
  HU_OR_RA_CLASS_SC = 0x0A,
  HUNTER_NON_ANDROID_SC = 0x0B,
  HUNTER_HU_CLASS_MALE_SC = 0x0C,
  HUNTER_FEMALE_SC = 0x0D,
  HUNTER_NON_RA_CLASS_HUMAN_SC = 0x0E,
  HUNTER_HU_CLASS_ANDROID_SC = 0x0F,
  HUNTER_NON_RA_CLASS_NON_NEWMAN_SC = 0x10,
  HUNTER_NON_NEWMAN_NON_FORCE_MALE_SC = 0x11,
  HUNTER_HUNEWEARL_CLASS_SC = 0x12,
  HUNTER_RA_CLASS_MALE_SC = 0x13,
  HUNTER_RA_CLASS_FEMALE_SC = 0x14,
  HUNTER_RA_OR_FO_CLASS_FEMALE_SC = 0x15,
  HUNTER_HU_OR_RA_CLASS_HUMAN_SC = 0x16,
  HUNTER_RA_CLASS_ANDROID_SC = 0x17,
  HUNTER_FO_CLASS_FEMALE_SC = 0x18,
  HUNTER_HUMAN_FEMALE_SC = 0x19,
  HUNTER_ANDROID_SC = 0x1A,
  HU_OR_FO_CLASS_SC = 0x1B,
  RA_OR_FO_CLASS_SC = 0x1C,
  PHYSICAL_OR_UNKNOWN_ATTACK_MEDIUM = 0x1D,
  TECH_OR_UNKNOWN_ATTACK_MEDIUM = 0x1E,
  PHYSICAL_OR_TECH_OR_UNKNOWN_ATTACK_MEDIUM = 0x1F,
  NON_PHYSICAL_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC = 0x20,
  NON_PHYSICAL_NON_TECH_ATTACK_MEDIUM_NON_SC = 0x21,
  NON_PHYSICAL_NON_TECH_NON_UNKNOWN_ATTACK_MEDIUM_NON_SC = 0x22,
};

enum class CardRank : uint8_t {
  N1 = 0x01,
  R1 = 0x02,
  S = 0x03,
  E = 0x04,
  N2 = 0x05,
  N3 = 0x06,
  N4 = 0x07,
  R2 = 0x08,
  R3 = 0x09,
  R4 = 0x0A,
  SS = 0x0B,
  // Cards with the D1 or D2 ranks are considered never usable by the player,
  // and are automatically removed from player decks before battle and when
  // loading the deckbuilder. Cards with the D1 rank appear in the deckbuilder
  // but are grayed out (and cannot be added to decks); cards with the D2 rank
  // don't appear in the deckbuilder at all.
  D1 = 0x0C,
  D2 = 0x0D,
  // The D3 rank is referenced in a few places, including the function that
  // determines whether or not a card can appear in post-battle draws, and the
  // function that determines whether a card should appear in the deckbuilder.
  // In these cases, it prevents the card from appearing.
  D3 = 0x0E,
};

enum class CardType : uint8_t {
  HUNTERS_SC = 0x00,
  ARKZ_SC = 0x01,
  ITEM = 0x02,
  CREATURE = 0x03,
  ACTION = 0x04,
  ASSIST = 0x05,
  INVALID_FF = 0xFF,
  END_CARD_LIST = 0xFF,
};

enum class CardClass : uint16_t {
  HU_SC = 0x0000,
  RA_SC = 0x0001,
  FO_SC = 0x0002,
  NATIVE_CREATURE = 0x000A,
  A_BEAST_CREATURE = 0x000B,
  MACHINE_CREATURE = 0x000C,
  DARK_CREATURE = 0x000D,
  GUARD_ITEM = 0x0015,
  MAG_ITEM = 0x0017,
  SWORD_ITEM = 0x0018,
  GUN_ITEM = 0x0019,
  CANE_ITEM = 0x001A,
  ATTACK_ACTION = 0x001E,
  DEFENSE_ACTION = 0x001F,
  TECH = 0x0020,
  PHOTON_BLAST = 0x0021,
  CONNECT_ONLY_ATTACK_ACTION = 0x0022,
  BOSS_ATTACK_ACTION = 0x0023,
  BOSS_TECH = 0x0024,
  ASSIST = 0x0028,
};

bool card_class_is_tech_like(CardClass cc, bool is_nte);

enum class TargetMode : uint8_t {
  NONE = 0x00, // Used for defense cards, mags, shields, etc.
  SINGLE_RANGE = 0x01,
  MULTI_RANGE = 0x02,
  SELF = 0x03,
  TEAM = 0x04,
  EVERYONE = 0x05,
  MULTI_RANGE_ALLIES = 0x06, // e.g. Shifta
  ALL_ALLIES = 0x07, // e.g. Anti, Resta, Leilla
  ALL = 0x08, // e.g. Last Judgment, Earthquake
  OWN_FCS = 0x09, // e.g. Traitor
};

const char* name_for_target_mode(TargetMode target_mode);

enum class ConditionType : uint8_t {
  NONE = 0x00,
  AP_BOOST = 0x01, // Temporarily increase AP by N
  RAMPAGE = 0x02,
  MULTI_STRIKE = 0x03, // Duplicate attack N times
  DAMAGE_MOD_1 = 0x04, // Set attack damage / AP to N after action cards applied (step 1)
  IMMOBILE = 0x05, // Give Immobile condition
  HOLD = 0x06, // Give Hold condition
  CANNOT_DEFEND = 0x07,
  TP_BOOST = 0x08, // Add N TP temporarily during attack
  GIVE_DAMAGE = 0x09, // Cause direct N HP loss
  GUOM = 0x0A, // Give Guom condition
  PARALYZE = 0x0B, // Give Paralysis condition
  A_T_SWAP_0C = 0x0C, // Swap AP and TP temporarily (same as 71?)
  A_H_SWAP = 0x0D, // Swap AP and HP temporarily
  PIERCE = 0x0E, // Attack SC directly even if they have items equipped
  UNUSED_0F = 0x0F,
  HEAL = 0x10, // Increase HP by N
  RETURN_TO_HAND = 0x11, // Return card to hand
  SET_MV_COST_TO_0 = 0x12,
  UNUSED_13 = 0x13,
  ACID = 0x14, // Give Acid condition
  ADD_1_TO_MV_COST = 0x15,
  MIGHTY_KNUCKLE = 0x16, // Temporarily increase AP by N, and set ATK dice to zero
  UNIT_BLOW = 0x17, // Temporarily increase AP by N * number of this card set within phase
  CURSE = 0x18, // Give Curse condition
  COMBO_AP = 0x19, // Temporarily increase AP by number of this card set within phase
  PIERCE_RAMPAGE_BLOCK = 0x1A, // Block attack if Pierce/Rampage
  ABILITY_TRAP = 0x1B, // Temporarily disable opponent abilities
  FREEZE = 0x1C, // Give Freeze condition
  ANTI_ABNORMALITY_1 = 0x1D, // Cure all abnormal conditions
  UNKNOWN_1E = 0x1E,
  EXPLOSION = 0x1F, // Damage all SCs and FCs by number of this same card set * 2
  UNKNOWN_20 = 0x20,
  UNKNOWN_21 = 0x21,
  UNKNOWN_22 = 0x22,
  RETURN_TO_DECK = 0x23, // Cancel discard and move to bottom of deck instead
  AERIAL = 0x24, // Give Aerial status
  AP_LOSS = 0x25, // Make attacker temporarily lose N AP during defense
  BONUS_FROM_LEADER = 0x26, // Gain AP equal to the number of cards of type N on the field
  FREE_MANEUVER = 0x27, // Enable movement over occupied tiles
  SCALE_MV_COST = 0x28, // Multiply all move action costs by expr (which may be zero)
  CLONE = 0x29, // Make setting this card free if at least one card of type N is already on the field
  DEF_DISABLE_BY_COST = 0x2A, // Disable use of any defense cards costing between (N / 10) and (N % 10) points, inclusive
  FILIAL = 0x2B, // Increase controlling SC's HP by N when this card is destroyed
  SNATCH = 0x2C, // Steal N EXP during attack
  HAND_DISRUPTER = 0x2D, // Discard N cards from hand immediately
  DROP = 0x2E, // Give Drop condition
  ACTION_DISRUPTER = 0x2F, // Destroy all action cards used by attacker
  SET_HP = 0x30, // Set HP to N
  NATIVE_SHIELD = 0x31, // Block attacks from Native creatures
  A_BEAST_SHIELD = 0x32, // Block attacks from A.Beast creatures
  MACHINE_SHIELD = 0x33, // Block attacks from Machine creatures
  DARK_SHIELD = 0x34, // Block attacks from Dark creatures
  SWORD_SHIELD = 0x35, // Block attacks from Sword items
  GUN_SHIELD = 0x36, // Block attacks from Gun items
  CANE_SHIELD = 0x37, // Block attacks from Cane items
  UNKNOWN_38 = 0x38,
  UNKNOWN_39 = 0x39,
  DEFENDER = 0x3A, // Make attacks go to setter of this card instead of original target
  SURVIVAL_DECOYS = 0x3B, // Redirect damage for multi-sided attack
  GIVE_OR_TAKE_EXP = 0x3C, // Give N EXP, or take if N is negative
  UNKNOWN_3D = 0x3D,
  DEATH_COMPANION = 0x3E, // If this card has 1 or 2 HP, set its HP to N
  EXP_DECOY = 0x3F, // If defender has EXP, lose EXP instead of getting damage when attacked
  SET_MV = 0x40, // Set MV to N
  GROUP = 0x41, // Temporarily increase AP by N * number of this card on field, excluding itself
  BERSERK = 0x42, // User of this card receives the same damage as target, and isn't helped by target's defense cards
  GUARD_CREATURE = 0x43, // Attacks on controlling SC damage this card instead
  TECH = 0x44, // Technique cards cost 1 fewer ATK point
  BIG_SWING = 0x45, // Increase all attacking ATK costs by 1
  UNKNOWN_46 = 0x46,
  SHIELD_WEAPON = 0x47, // Limit attacker's choice of target to guard items
  ATK_DICE_BOOST = 0x48, // Increase ATK dice roll by 1
  UNKNOWN_49 = 0x49,
  MAJOR_PIERCE = 0x4A, // If SC has over half of max HP, attacks target SC instead of equipped items
  HEAVY_PIERCE = 0x4B, // If SC has 3 or more items equipped, attacks target SC instead of equipped items
  MAJOR_RAMPAGE = 0x4C, // If SC has over half of max HP, attacks target SC and all equipped items
  HEAVY_RAMPAGE = 0x4D, // If SC has 3 or more items equipped, attacks target SC and all equipped items
  AP_GROWTH = 0x4E, // Permanently increase AP by N
  TP_GROWTH = 0x4F, // Permanently increase TP by N
  REBORN = 0x50, // If any card of type N is on the field, this card goes to the hand when destroyed instead of being discarded
  COPY = 0x51, // Temporarily set AP/TP to N percent (or 100% if N is 0) of opponent's values
  UNKNOWN_52 = 0x52,
  MISC_GUARDS = 0x53, // Add N to card's defense value
  AP_OVERRIDE = 0x54, // Set AP to N temporarily
  TP_OVERRIDE = 0x55, // Set TP to N temporarily
  RETURN = 0x56, // Return card to hand on destruction instead of discarding
  A_T_SWAP_PERM = 0x57, // Permanently swap AP and TP
  A_H_SWAP_PERM = 0x58, // Permanently swap AP and HP
  SLAYERS_ASSASSINS = 0x59, // Temporarily increase AP during attack
  ANTI_ABNORMALITY_2 = 0x5A, // Remove all conditions
  FIXED_RANGE = 0x5B, // Use SC's range instead of weapon or attack card ranges
  ELUDE = 0x5C, // SC does not lose HP when equipped items are destroyed
  PARRY = 0x5D, // Forward attack to a random FC within one tile of original target, excluding attacker and original target
  BLOCK_ATTACK = 0x5E, // Completely block attack
  UNKNOWN_5F = 0x5F,
  UNKNOWN_60 = 0x60,
  COMBO_TP = 0x61, // Gain TP equal to the number of cards of type N on the field
  MISC_AP_BONUSES = 0x62, // Temporarily increase AP by N
  MISC_TP_BONUSES = 0x63, // Temporarily increase TP by N
  UNKNOWN_64 = 0x64,
  MISC_DEFENSE_BONUSES = 0x65, // Decrease damage by N
  MOSTLY_HALFGUARDS = 0x66, // Reduce damage from incoming attack by N
  PERIODIC_FIELD = 0x67, // Swap immunity to tech or physical attacks
  FC_LIMIT_BY_COUNT = 0x68, // Change FC limit from 8 ATK points total to 4 FCs total
  UNKNOWN_69 = 0x69,
  MV_BONUS = 0x6A, // Increase MV by N
  FORWARD_DAMAGE = 0x6B,
  WEAK_SPOT_INFLUENCE = 0x6C, // Temporarily decrease AP by N
  DAMAGE_MODIFIER_2 = 0x6D, // Set attack damage / AP after action cards applied (step 2)
  WEAK_HIT_BLOCK = 0x6E, // Block all attacks of N damage or less
  AP_SILENCE = 0x6F, // Temporarily decrease AP of opponent by N
  TP_SILENCE = 0x70, // Temporarily decrease TP of opponent by N
  A_T_SWAP = 0x71, // Temporarily swap AP and TP
  HALFGUARD = 0x72, // Halve damage from attacks that would inflict N or more damage
  UNKNOWN_73 = 0x73,
  RAMPAGE_AP_LOSS = 0x74, // Temporarily reduce AP by N
  UNKNOWN_75 = 0x75,
  REFLECT = 0x76, // Generate reverse attack
  UNKNOWN_77 = 0x77,
  ANY = 0x78, // Not a real condition; used as a wildcard in search functions. Has value 0x64 on NTE
  UNKNOWN_79 = 0x79,
  UNKNOWN_7A = 0x7A,
  UNKNOWN_7B = 0x7B,
  UNKNOWN_7C = 0x7C,
  UNKNOWN_7D = 0x7D,
  INVALID_FF = 0xFF,
  ANY_FF = 0xFF, // Used as a wildcard in some search functions
};

enum class EffectWhen : uint8_t {
  NONE = 0x00,
  CARD_SET = 0x01, // Permanent effects like RAMPAGE/PIERCE on SCs, BIG_SWING, AERIAL, etc.; many AC effects also
  AFTER_ANY_CARD_ATTACK = 0x02, // GIVE_DAMAGE, HEAL, A_H_SWAP_PERM
  BEFORE_ANY_CARD_ATTACK = 0x03, // AP_LOSS, COMBO_TP
  BEFORE_DICE_PHASE_THIS_TEAM_TURN = 0x04, // Many different effects
  CARD_DESTROYED = 0x05, // RETURN_TO_HAND, RETURN, FILIAL, GIVE_OR_TAKE_EXP
  AFTER_SET_PHASE = 0x06, // Unused
  BEFORE_MOVE_PHASE = 0x09, // Unused
  UNKNOWN_0A = 0x0A, // ANTI_ABNORMALITY_2 on Tollaw (non-SC version of another when?)
  AFTER_ATTACK_TARGET_RESOLUTION = 0x0B, // ABILITY_TRAP via First Attack action card only
  AFTER_THIS_CARD_ATTACK = 0x0C, // Many effects
  BEFORE_THIS_CARD_ATTACK = 0x0D, // Conditions, AP_BOOST/TP_BOOST, AP_SILENCE, MULTI_STRIKE
  BEFORE_ACT_PHASE = 0x0E, // Before act phase (ANTI_ABNORMALITY_2, FIXED_RANGE)
  BEFORE_DRAW_PHASE = 0x0F, // Unused
  AFTER_CARD_MOVE = 0x13, // Unused
  UNKNOWN_15 = 0x15, // Unused
  AFTER_THIS_CARD_ATTACKED = 0x16, // Conditions, DEATH_COMPANION, GIVE_DAMAGE, AP_GROWTH (Nidra)
  BEFORE_THIS_CARD_ATTACKED = 0x17, // Defense damage adjustments
  AFTER_CREATURE_OR_HUNTER_SC_ATTACK = 0x20, // RETURN_TO_HAND, A_T_SWAP_PERM, GIVE_OR_TAKE_EXP
  BEFORE_CREATURE_OR_HUNTER_SC_ATTACK = 0x21, // Unused
  UNKNOWN_22 = 0x22, // MISC_AP_BONUSES (SCs only?)
  BEFORE_MOVE_PHASE_AND_AFTER_CARD_MOVE_FINAL = 0x27, // SET_MV
  UNKNOWN_29 = 0x29, // MIGHTY_KNUCKLE
  UNKNOWN_2A = 0x2A, // Unused
  UNKNOWN_2B = 0x2B, // Unused
  UNKNOWN_33 = 0x33, // DEF_DISABLE_BY_COST
  UNKNOWN_34 = 0x34, // Unused
  UNKNOWN_35 = 0x35, // Unused
  ATTACK_STAT_OVERRIDES = 0x3D, // BONUS_FROM_LEADER, COPY, ABILITY_TRAP
  ATTACK_DAMAGE_ADJUSTMENT = 0x3E, // AP_BOOST, SLAYERS_ASSASSINS, WEAK_SPOT_INFLUENCE, GROUP
  DEFENSE_DAMAGE_ADJUSTMENT = 0x3F, // MOSTLY_HALFGUARDS, ACTION_DISRUPTER
  BEFORE_DICE_PHASE_ALL_TURNS_FINAL = 0x46, // Pollux Timed Pierce
};

enum class AssistEffect : uint16_t {
  NONE = 0x0000,
  DICE_HALF = 0x0001,
  DICE_PLUS_1 = 0x0002,
  DICE_FEVER = 0x0003,
  CARD_RETURN = 0x0004,
  LAND_PRICE = 0x0005,
  POWERLESS_RAIN = 0x0006,
  BRAVE_WIND = 0x0007,
  SILENT_COLOSSEUM = 0x0008,
  RESISTANCE = 0x0009,
  INDEPENDENT = 0x000A,
  ASSISTLESS = 0x000B,
  ATK_DICE_2 = 0x000C,
  DEFLATION = 0x000D,
  INFLATION = 0x000E,
  EXCHANGE = 0x000F,
  INFLUENCE = 0x0010,
  SKIP_SET = 0x0011,
  SKIP_MOVE = 0x0012,
  SKIP_ACT = 0x0013,
  SKIP_DRAW = 0x0014,
  FLY = 0x0015,
  NECROMANCER = 0x0016,
  PERMISSION = 0x0017,
  SHUFFLE_ALL = 0x0018,
  LEGACY = 0x0019,
  ASSIST_REVERSE = 0x001A,
  STAMINA = 0x001B,
  AP_ABSORPTION = 0x001C,
  HEAVY_FOG = 0x001D,
  TRASH_1 = 0x001E,
  EMPTY_HAND = 0x001F,
  HITMAN = 0x0020,
  ASSIST_TRASH = 0x0021,
  SHUFFLE_GROUP = 0x0022,
  ASSIST_VANISH = 0x0023,
  CHARITY = 0x0024,
  INHERITANCE = 0x0025,
  FIX = 0x0026,
  MUSCULAR = 0x0027,
  CHANGE_BODY = 0x0028,
  GOD_WHIM = 0x0029,
  GOLD_RUSH = 0x002A,
  ASSIST_RETURN = 0x002B,
  REQUIEM = 0x002C,
  RANSOM = 0x002D,
  SIMPLE = 0x002E,
  SLOW_TIME = 0x002F,
  QUICK_TIME = 0x0030,
  TERRITORY = 0x0031,
  OLD_TYPE = 0x0032,
  FLATLAND = 0x0033,
  IMMORTALITY = 0x0034,
  SNAIL_PACE = 0x0035,
  TECH_FIELD = 0x0036,
  FOREST_RAIN = 0x0037,
  CAVE_WIND = 0x0038,
  MINE_BRIGHTNESS = 0x0039,
  RUIN_DARKNESS = 0x003A,
  SABER_DANCE = 0x003B,
  BULLET_STORM = 0x003C,
  CANE_PALACE = 0x003D,
  GIANT_GARDEN = 0x003E,
  MARCH_OF_THE_MEEK = 0x003F,
  SUPPORT = 0x0040,
  RICH = 0x0041,
  REVERSE_CARD = 0x0042,
  VENGEANCE = 0x0043,
  SQUEEZE = 0x0044,
  HOMESICK = 0x0045,
  BOMB = 0x0046,
  SKIP_TURN = 0x0047,
  BATTLE_ROYALE = 0x0048,
  DICE_FEVER_PLUS = 0x0049,
  RICH_PLUS = 0x004A,
  CHARITY_PLUS = 0x004B,
  ANY = 0x004C, // Unused on cards; used in some search functions
};

enum class BattlePhase : uint8_t {
  INVALID_00 = 0,
  DICE = 1,
  SET = 2,
  MOVE = 3,
  ACTION = 4,
  DRAW = 5,
  INVALID_FF = 0xFF,
};

enum class ActionSubphase : uint8_t {
  ATTACK = 0,
  DEFENSE = 2,
  INVALID_FF = 0xFF,
};

enum class SetupPhase : uint8_t {
  REGISTRATION = 0,
  STARTER_ROLLS = 1,
  HAND_REDRAW_OPTION = 2,
  MAIN_BATTLE = 3,
  BATTLE_ENDED = 4,
  INVALID_FF = 0xFF,
};

enum class RegistrationPhase : uint8_t {
  AWAITING_NUM_PLAYERS = 0, // num_players not set yet
  AWAITING_PLAYERS = 1, // num_players set, but some players not registered
  AWAITING_DECKS = 2, // all players registered, but some decks missing
  REGISTERED = 3, // All players/decks present, but battle not started yet
  BATTLE_STARTED = 4,
  INVALID_FF = 0xFF,
};

enum class Direction : uint8_t {
  RIGHT = 0,
  UP = 1,
  LEFT = 2,
  DOWN = 3,
  INVALID_FF = 0xFF,
};

Direction turn_left(Direction d);
Direction turn_right(Direction d);
Direction turn_around(Direction d);

struct Location {
  /* 00 */ uint8_t x;
  /* 01 */ uint8_t y;
  /* 02 */ Direction direction;
  /* 03 */ uint8_t unused;
  /* 04 */

  Location();
  Location(uint8_t x, uint8_t y);
  Location(uint8_t x, uint8_t y, Direction direction);
  bool operator==(const Location& other) const;
  bool operator!=(const Location& other) const;

  std::string str() const;

  void clear();
  void clear_FF();
} __packed_ws__(Location, 4);

struct CardDefinition {
  struct Stat {
    enum Type : uint8_t {
      BLANK = 0,
      STAT = 1,
      PLUS_STAT = 2,
      MINUS_STAT = 3,
      EQUALS_STAT = 4,
      UNKNOWN = 5,
      PLUS_UNKNOWN = 6,
      MINUS_UNKNOWN = 7,
      EQUALS_UNKNOWN = 8,
    };
    /* 00 */ be_uint16_t code;
    /* 02 */ Type type;
    /* 03 */ int8_t stat;
    /* 04 */

    void decode_code();
    std::string str() const;
    phosg::JSON json() const;
  } __packed_ws__(Stat, 4);

  struct Effect {
    // effect_num is the 1-based index of this effect within the card definition
    // (that is, .effects[0] should have effect_num == 1 if it is used).
    /* 00 */ uint8_t effect_num;
    /* 01 */ ConditionType type;
    // For ConditionTypes that need it, expr specifies "how much". (For those
    // that don't, expr may be blank.) The value may contain tokens that refer
    // to stats from the current battle (see description_for_expr_token) and
    // operators to perform basic computations on them. Operators are evaluated
    // left-to-right in the expression, and there are no operator precedence
    // rules; for example, the expression "4+4//2" results in 4, not 6.
    /* 02 */ pstring<TextEncoding::ASCII, 0x0F> expr;
    // when specifies in which phase the effect should activate.
    /* 11 */ EffectWhen when;
    // arg1 generally specifies how long the effect activates for.
    /* 12 */ pstring<TextEncoding::ASCII, 4> arg1;
    // arg2 generally specifies a condition for when the effect activates.
    /* 16 */ pstring<TextEncoding::ASCII, 4> arg2;
    // arg3 generally specifies who is targeted by the effect.
    /* 1A */ pstring<TextEncoding::ASCII, 4> arg3;
    // apply_criterion can be used to apply an additional condition for when the
    // effect should activate. For example, it can be used to make the effect
    // only activate if the target is not a Story Character.
    /* 1E */ CriterionCode apply_criterion;
    // name_index specifies which string from TextEnglish.pr2 is shown next to
    // the card when it is attacking or defending. Zero in this field means no
    // string is shown for this ability.
    /* 1F */ uint8_t name_index;
    /* 20 */

    bool is_empty() const;
    static std::string str_for_arg(const std::string& arg);
    std::string str(const char* separator = ", ", const TextSet* text_archive = nullptr) const;
    phosg::JSON json() const;
  } __packed_ws__(Effect, 0x20);

  /* 0000 */ be_uint32_t card_id;
  /* 0004 */ pstring<TextEncoding::SJIS, 0x40> jp_name;

  // The list of card definitions ends with a "sentinel" definition that isn't a
  // real card, but instead has a negative number in the type field here.
  /* 0044 */ CardType type;

  /* 0045 */ uint8_t self_cost; // ATK dice points required
  /* 0046 */ uint8_t ally_cost; // ATK points from allies required; PBs use this
  /* 0047 */ uint8_t unused1;

  // In the definitions file, only .code is populated here; .decode_code() must
  // be called to fill in .type and .stat within each of these.
  /* 0048 */ Stat hp;
  /* 004C */ Stat ap;
  /* 0050 */ Stat tp;
  /* 0054 */ Stat mv;

  // See name_for_link_color for the list of values used here.
  /* 0058 */ parray<uint8_t, 8> left_colors;
  /* 0060 */ parray<uint8_t, 8> right_colors;
  /* 0068 */ parray<uint8_t, 8> top_colors;

  // The card's attack range is defined in a somewhat odd format here. Each
  // field in this array corresponds to a single row of the range, and every
  // fourth bit, starting with bit 15, corresponds to a tile in that row. The
  // rest of the bits are ignored, except in two special cases described below.
  // For example, Ohgun's range is:
  //   [0] = 0x00000000 => -----
  //   [1] = 0x00001110 => -***-
  //   [2] = 0x00001110 => -***-
  //   [3] = 0x00000000 => -----
  //   [4] = 0x00000000 => ----- (the card itself is in the center of this row)
  //   [5] = 0x00000000 => -----
  //
  // The two special cases are as follows:
  // 1. If all six values in the range array are 0x000FFFFF, then the card's
  //    range is the entire field.
  // 2. If the cell corresponding to the card itself ((range[4] >> 8) & 0x0F) is
  //    not zero, then the rest of the range array is ignored and the card's
  //    range comes from a fixed set of ranges instead. See decode_range() for
  //    more information.
  /* 0070 */ parray<be_uint32_t, 6> range;

  /* 0088 */ be_uint32_t unused2;
  /* 008C */ TargetMode target_mode;
  /* 008D */ uint8_t assist_turns; // 90 (dec) = once, 99 (dec) = forever
  // This field is 1 if the card cannot move by itself. Item cards hare 1 here
  // because they cannot move on their own and automatically move along with
  // their SC instead. Generally only SCs and creatures have 0 here.
  /* 008E */ uint8_t cannot_move;
  // This field is 1 if the card cannot take part in an attack. Unlike
  // cannot_move, cards that cannot attack on their own but can take part in an
  // attack (such as action cards) have 0 here. Most shields, mags, defense
  // actions, and assist cards have 1 here.
  /* 008F */ uint8_t cannot_attack;
  /* 0090 */ uint8_t unused3;
  // If cannot_drop is 0, this card can't appear in post-battle rewards. A
  // value of 0 here also prevents the card from being used as a God Whim
  // random assist.
  /* 0091 */ uint8_t cannot_drop;
  // This criterion code specifies who can use the card, and when it can be
  // used. This specifies which Hero-side SCs can use which items, for example,
  // and when action cards can be played (when SC or FC is attacking, on self or
  // ally, etc.).
  /* 0092 */ CriterionCode usable_criterion;
  /* 0093 */ CardRank rank;
  /* 0094 */ be_uint16_t unused4;
  // The card class is used for checking attributes (e.g. item types). It's
  // stored big-endian here, so there's a helper function (card_class()) that
  // returns a usable CardClass enum value.
  /* 0096 */ be_uint16_t be_card_class;

  // If this card is an assist card, this field controls how COM players handle
  // playing it. This field is ignored for non-assist cards. This integer
  // encodes the following fields:
  // - assist_ai_params % 100 (that is, the two lowest decimal places) appears
  //   to specify the effect, though a few unrelated cards share values in this
  //   field. It's not yet known how exactly this is used by the COM logic.
  // - (assist_ai_params / 100) % 10 specifies the priority. It appears the COM
  //   logic always chooses the assist card with the highest value in this field
  //   if there are multiple cards to consider.
  // - (assist_ai_params / 1000) % 10 specifies on whom the assist card may be
  //   played (0 = any player, 1 = self, 2 = self or ally, 3 = enemy only).
  /* 0098 */ be_uint16_t assist_ai_params;
  // Most cards in the official definitions file have the same value stored in
  // unused5 as in assist_ai_params. Unlike assist_ai_params, unused5 does not
  // appear to be used anywhere.
  /* 009A */ be_uint16_t unused5;

  // The card drop rates control how likely the card is to appear in a standard
  // post-battle random draw. How this works is fairly complex and is explained
  // below in detail. Before any of that logic, this card can never drop and no
  // card can transform into this card if any of the following are true:
  // - type is SC_HUNTERS or SC_ARKZ
  // - card_class is BOSS_ATTACK_ACTION (0x23) or BOSS_TECH (0x24)
  // - rank is E, D1, or D2
  // - cannot_drop is 1 (specifically 1; other nonzero values for cannot_drop
  //   don't prevent the card from appearing in post-battle draws)
  // If none of these conditions apply, the logic below is used.
  //
  // Drop rates are integers which encode the following data:
  // - rate % 10 (that is, the lowest decimal place) specifies the required game
  //   mode. 0 means any mode, 1 means offline story mode, 2 means 1P free
  //   battle, 3 means 2P+ free battle (specifically, PvP - two humans vs. two
  //   COMs counts as 1P free battle), 4 means online mode, 5 means tournament.
  //   Some cards have this field set to 6, which isn't a valid game mode; it
  //   seems Sega used this as a way to make sure the drop rate never applies.
  // - (rate / 10) % 100 (that is, the tens and hundreds decimal places) specify
  //   the environment number + 1. For example, if this field contains 5, then
  //   this drop only applies if the battle took place at Molae Venti
  //   (environment number 4). If this field is zero, the drop applies
  //   regardless of where the battle took place.
  // - rate / 1000 (the thousands decimal place) specifies the rarity class.
  //   This can be any number in the range [0, 9], and affects how likely the
  //   card is to appear based on the player's level. See below for details.
  // - rate / 10000 (the ten-thousands decimal place) specifies if the drop rate
  //   applies only if the player used a Hunters deck (1), only if they used an
  //   Arkz deck (2), or if they used any deck (0).
  //
  // When determining which cards to drop, the game first checks the drop rate
  // fields on all cards. For each drop rate that applies, the game adds the
  // card ID into an appropriate bucket based on the rarity class. (If both drop
  // rates for a card apply, the card ID is added twice.) The player's level
  // class is then computed according to the following table:
  //              1    2    3      4      5      6      7      8      9    10
  //     CLvOff  1-2  3-4  5-9   10-14  15-19  20-25  26-29  30-39  40-49  50+
  //     CLvOn   1-2  3-4  5-10  11-16  17-23  24-32  33-39  40-49  50-99  100+
  // For the purposes of this computation, the player's level is used by default
  // (CLvOn or CLvOff), but the map may override it - see win_level_override and
  // loss_level_override in MapDefinition. This specifies which row in the
  // following tables will be used.
  //
  // Cards are then chosen from the buckets with a weighted distribution
  // according to these tables (row is player's level class, column is card's
  // rarity class):
  // Offline:
  //   LC | RC = 0       1       2       3       4       5     6     7  8  9
  //    1 |   8000    2000      50
  //    2 |   6000    3500     500      20
  //    3 |   4500    4000    1500     200
  //    4 |   3500    3500    2300     700      20
  //    5 |   2700    2800    2500    1500     500      10
  //    6 |   2300    2300    2300    1900     900     300     1
  //    7 |   1995    2100    2100    2100    1000     700     5
  //    8 |   1789    2100    2100    2100    1100     800    10     1
  //    9 |  14620   20000   21000   22000   13000    9000   300    80
  //   10 | 133997  190000  200000  200000  150000  120000  5000  1000  2  1
  // Online:
  //   LC | RC = 0       1       2       3       4       5       6      7   8  9
  //    1 |   8000    2000      50
  //    2 |   6000    3500     500      50
  //    3 |   4500    3500    1500     400     100
  //    4 |   3000    3000    2500    1000     450      50
  //    5 |   2000    2600    2750    2000     500     100      50
  //    6 |   1900    2200    2500    2100     830     350     100     20
  //    7 |   1900    2000    2000    2000    1000     500     500    100
  //    8 | 160000  160000  190000  190000  130000  100000   50000  19999   1
  //    9 | 120000  120000  150000  160000  150000  150000  100000  49989  10  1
  //   10 | 120000  120000  130000  150000  160000  150000  100000  69965  30  5
  // These values are all relative to other values in the same row. For example,
  // if your character is in level class 1, you'll get cards of rarity class 0
  // about 80% of the time, cards of rarity class 1 about 20% of the time, and
  // cards of rarity class 2 about 0.5% of the time. (The actual probabilities
  // are 8000/10050, 2000/10050, and 50/10050.)
  //
  // When choosing the contents of the four card packs after a battle, the game
  // first chooses how many cards to give the player based on the end-of-battle
  // rank (9 for S, 8 for A+/A, 7 for B+/B, 6 for C+/C, 5 for D+/D/E, 2 if the
  // player lost). It then decides the number of "restricted" cards; if the
  // player is getting 6 or more cards, there are 2 restricted cards per pack,
  // otherwise there is only one. The restricted cards are required to be a
  // certain type in each pack except the black pack:
  // - In the blue pack, the restricted cards must be creature cards.
  // - In the red pack, the restricted cards must be item cards.
  // - In the green pack, the restricted cards must be action cards.
  // For example, if you get a B+ rank after winning a battle and pick the green
  // pack, you will always get at least two action cards.
  //
  // The game then samples N card IDs from the appropriate buckets (where N is
  // the number chosen above), but for the first 1 or 2 cards, it applies the
  // restriction described above and re-draws if the card is the wrong type.
  // After sampling the N card IDs, it sorts them and presents them to the
  // player.
  //
  // There is one more effect to consider after cards are chosen: cards may
  // randomly transform into VIP cards or into stronger (and rarer) cards. The
  // chance of each of these occurring is based on the rarity of that card that
  // may transform, and the number of copies of that card which the player
  // already has. In the below table, P(activate) is the probability that any
  // transformation is attempted at all; if this check passes, the player sees
  // the glow effect and "The change will occur..." text. P(vip) is the
  // probability that the card becomes a VIP card, after the glow effect plays.
  // P(rare) is the probability of the card becoming a rarer card after the glow
  // effect. Therefore, the final probability that a card will transform into a
  // VIP card is P(activate) * P(vip), and the final probability of transforming
  // into a rarer card is P(activate) * P(rare).
  //        ======== Card rank N4-N1 ========  ======== Card rank R4-R1 ========
  // Count  P(activate)  P(rare)  P(vip)       P(activate)  P(rare)  P(vip)
  //  0-4   0%            0%      0%           0%            0%      0%
  //  5-10  1.923077%    55%      0.5%         2.0408163%   55%      0.5%
  // 11-16  2.1276595%   60%      0.45454544%  2.2727273%   60%      0.4761905%
  // 17-24  2.3809524%   70%      0.4347826%   2.5641026%   70%      0.45454544%
  // 25-32  2.7027028%   70%      0.4%         2.9411765%   70%      0.5%
  // 33-40  3.125%       80%      0.38461538%  3.448276%    70%      0.5%
  // 41-52  3.7037037%   80%      0.35714286%  4.1666668%   80%      0.45454544%
  // 53-99  5%           90%      0.33333334%  5.263158%    90%      0.4347826%
  //
  // If a transformation occurs, the card transforms to a card of a different
  // rank. First, the game consults the following table to determine the rank of
  // the resulting card (original card's rank on the left, new card's rank
  // across the top):
  //        N4   N3   N2   N1   R4   R3   R2   R1    S   SS
  // N4 =>            60   30   10
  // N3 =>                 60   30   10
  // N2 =>                      60   30   10
  // N1 =>                      30   55   10    5
  // R4 =>                      10   50   35    5
  // R3 =>                           20   50   28    1
  // R2 =>                                30   60    5
  // R1 =>                                    900  100    1
  // For example, when an N2 card transforms, there is a 60% chance to become
  // R4, a 30% chance to become R3, and a 10% chance to become R2. When an R1
  // card transforms, there is a 900/1001 chance of becoming another R1, a
  // 100/1001 chance of becoming an S, and a 1/1001 chance of becoming an SS.
  //
  // Once a rank is chosen, the game puts all possible cards into buckets based
  // on how many of that card the player already has, then chooses a random card
  // out of bucket 0, then bucket 1, etc. all the way up to bucket 49 (or 2 if
  // the final rank is S or SS). The first drawn card that has the final rank is
  // the card that the original card transforms into. Notably, this logic means
  // that cards are more likely to transform into cards that the player doesn't
  // already have, or only has few copies of. Also notably, it is impossible for
  // a card to transform into another card that the player already has 50 or
  // more copies of, or an S or SS card that the player already has 3 copies of.
  //
  // One curiosity about the above procedure is that the buckets can only hold
  // 400 cards each for the N ranks, 300 each for the R ranks, and 100 each for
  // S and SS. It is possible for one bucket to overflow into the next, or to
  // overflow out of bounds and cause memory corruption, if there are (for
  // example) more than 400 cards that have ranks N1-N4, and the player has 99
  // of all of them.
  //
  // Remember the drop rates mentioned way back in the second paragraph of this
  // enormous comment? That's what this array stores.
  /* 009C */ parray<be_uint16_t, 2> drop_rates;

  /* 00A0 */ pstring<TextEncoding::ISO8859, 0x14> en_name;
  /* 00B4 */ pstring<TextEncoding::SJIS, 0x0B> jp_short_name;
  /* 00BF */ pstring<TextEncoding::ISO8859, 0x08> en_short_name;
  // These effects modify the card's behavior in various situations. Only
  // effects for which effect_num is not zero are used.
  /* 00C7 */ parray<Effect, 3> effects;
  /* 0127 */ uint8_t unused6;
  /* 0128 */

  bool is_sc() const;
  bool is_fc() const;
  bool is_named_android_sc() const;
  bool any_top_color_matches(const CardDefinition& other) const;
  CardClass card_class() const;

  void decode_range();
  std::string str(bool single_line = true, const TextSet* text_archive = nullptr) const;
  phosg::JSON json() const;
} __packed_ws__(CardDefinition, 0x128);

struct CardDefinitionsFooter {
  /* 00 */ be_uint32_t num_cards1;
  /* 04 */ be_uint32_t cards_offset; // == 0
  /* 08 */ be_uint32_t num_cards2;
  /* 0C */ parray<be_uint32_t, 3> unknown_a2;
  /* 18 */ parray<be_uint16_t, 0x10> relocations;
  /* 38 */ RELFileFooterBE rel_footer;
  /* 58 */
} __packed_ws__(CardDefinitionsFooter, 0x58);

struct DeckDefinition {
  /* 00 */ pstring<TextEncoding::MARKED, 0x10> name;
  /* 10 */ be_uint32_t client_id; // 0-3
  // List of card IDs. The card count is the number of nonzero entries here
  // before a zero entry (or 50 if no entries are nonzero). The first card ID is
  // the SC card, which the game implicitly subtracts from the limit - so a
  // valid deck should actually have 31 cards in it.
  /* 14 */ parray<le_uint16_t, 50> card_ids;
  /* 78 */ be_uint32_t unknown_a1;
  // Last modification time
  /* 7C */ le_uint16_t year;
  /* 7E */ uint8_t month;
  /* 7F */ uint8_t day;
  /* 80 */ uint8_t hour;
  /* 81 */ uint8_t minute;
  /* 82 */ uint8_t second;
  /* 83 */ uint8_t unknown_a2;
  /* 84 */
} __packed_ws__(DeckDefinition, 0x84);

struct PlayerConfig {
  // The game splits this internally into two structures. The first column of
  // offsets is relative to the start of the first structure; the second column
  // is relative to the start of the second structure.
  /* 0000:---- */ pstring<TextEncoding::MARKED, 12> rank_text; // From B7 command
  /* 000C:---- */ parray<uint8_t, 0x1C> unknown_a1;
  /* 0028:---- */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
  /* 0050:---- */ parray<be_uint32_t, 10> choice_search_config;
  // This field maps to quest_counters on Episodes 1 & 2
  /* 0078:---- */ parray<be_uint32_t, 0x30> scenario_progress;
  // place_counts[0] and [1] from this field are added to the player's win and
  // loss count respectively when they're shown in the status menu. However,
  // these values start at 0 and never seem to be modified. Perhaps in an
  // earlier version, this was the offline records structure, but they later
  // decided to just count online and offline records together in the main
  // records structure and didn't remove the codepath that reads from this.
  /* 0138:---- */ PlayerRecordsBattleBE unused_offline_records;
  /* 0150:---- */ parray<uint8_t, 4> unknown_a4;
  // The PlayerDataSegment structure begins here. In newserv, we combine this
  // structure into PlayerConfig since the two are always used together.
  /* 0154:0000 */ uint8_t is_encrypted;
  /* 0155:0001 */ uint8_t basis;
  /* 0156:0002 */ parray<uint8_t, 2> unused;
  // The following fields (here through the beginning of decks) are encrypted
  // using the trivial algorithm, with the basis specified above, if
  // is_encrypted is equal to 1.
  // It appears the card counts field in this structure is actually 1000 (0x3E8)
  // bytes long, even though in every other place the counts array appears it's
  // 0x2F0 bytes long. They presumably did this because of the checksum logic.
  /* 0158:0004 */ parray<uint8_t, 1000> card_counts;
  // These appear to be an attempt at checksumming the card counts array, but
  // the algorithm doesn't cover the entire array and instead reads from later
  // parts of this structure. This appears to be due to a copy/paste error in
  // the original code. The algorithm sums card_counts [0] through [19] and puts
  // the result in card_count_checksums[0], then sums card counts [50] through
  // [69] and puts the result in card_count_checksums[1], etc. Presumably they
  // intended to use 20 as the stride instead of 50, which would have exactly
  // covered the entire card_counts array.
  /* 0540:03EC */ parray<be_uint16_t, 50> card_count_checksums;
  // These 64-bit integers encode information about when rare cards (those with
  // ranks S, SS, E, or D2) were obtained. Each integer contains the following
  // fields:
  //   ???????? PPPPPPPP PPPPVVVV VVVVVVVV VVVVVVVV VVVVVVVV VVVVVVVV VVVVVVVV
  // The meaning of the high byte is unknown, but it is not used by the decoding
  // function. P is a prime number between 1009 (0x3F1) and 2039 (0x7F7),
  // inclusive. V is a 44-bit integer that, when modulated by P, yields the card
  // ID (that is, V % P == card_id). When a non-rare card is obtained or lost,
  // the game just increments or decrements the value in the card_counts array
  // above, but when a rare card is obtained or lost, the game adds or removes a
  // token in rare_tokens and recomputes the count for that card by scanning and
  // decoding all rare tokens. It then writes that count to card_counts.
  // This seems to be an anti-cheating measure specifically targeted at memory
  // editing - the server could verify that the count in card_counts is correct
  // for rare cards by counting the valid tokens in this array. (Sega seemed
  // fairly concerned with memory editing in general in this game, since the
  // card counts array is encrypted in memory most of the time, and they went
  // out of their way to ensure the game uses an area of memory that almost no
  // other game uses, which is also used by the Action Replay.)
  /* 05A4:0450 */ parray<be_uint64_t, 450> rare_tokens;
  /* 13B4:1260 */ parray<uint8_t, 0x80> unknown_a7;
  /* 1434:12E0 */ parray<DeckDefinition, 25> decks;
  /* 2118:1FC4 */ parray<uint8_t, 0x08> unknown_a8;
  /* 2120:1FCC */ be_uint32_t offline_clv_exp; // CLvOff = (this / 100) + 1
  /* 2124:1FD0 */ be_uint32_t online_clv_exp; // CLvOn = (this / 100) + 1
  struct PlayerReference {
    /* 00 */ be_uint32_t guild_card_number;
    /* 04 */ pstring<TextEncoding::MARKED, 0x18> name;
  } __packed_ws__(PlayerReference, 0x1C);
  // These two arrays are updated when a battle is started (via a 6xB4x05
  // command). The client adds the opposing players' info to ths first two
  // entries in recent_human_opponents if the opponents are human. (The
  // existing entries are always moved back by two slots, but if one or both
  // opponents are not humans, one or both of the newly-vacated slots is not
  // filled in.) Both arrays have the most recent entries at the beginning.
  /* 2128:1FD4 */ parray<PlayerReference, 10> recent_human_opponents;
  /* 2240:20EC */ parray<be_uint32_t, 5> recent_battle_start_timestamps;
  /* 2254:2100 */ parray<uint8_t, 0x14> unknown_a10;
  /* 2268:2114 */ be_uint32_t init_timestamp;
  /* 226C:2118 */ be_uint32_t last_online_battle_start_timestamp;
  // In a certain situation, unknown_t3 is set to init_timestamp plus a multiple
  // of two weeks (1209600 seconds). unknown_t3 appears never to be used for
  // anything, though.
  /* 2270:211C */ be_uint32_t unknown_t3;
  // This visual config is copied to the player's main visual config when the
  // player's name or proportions have changed, or when certain buttons on the
  // controller (L, R, X, Y) are held at game start time.
  /* 2274:2120 */ PlayerVisualConfig backup_visual;
  /* 22C4:2170 */ parray<uint8_t, 0x8C> unknown_a14;
  /* 2350:21FC */

  void decrypt();
  void encrypt(uint8_t basis);
} __packed_ws__(PlayerConfig, 0x2350);

struct PlayerConfigNTE {
  /* 0000 */ pstring<TextEncoding::MARKED, 12> rank_text;
  /* 000C */ parray<uint8_t, 0x1C> unknown_a1;
  /* 0028 */ parray<be_uint16_t, 20> tech_menu_shortcut_entries;
  /* 0050 */ parray<be_uint32_t, 10> choice_search_config;
  /* 0078 */ parray<be_uint32_t, 0x10> scenario_progress; // Final has 0x30 entries here
  /* 00B8 */ PlayerRecordsBattleBE unused_offline_records;
  /* 00D0 */ parray<uint8_t, 4> unknown_a4;
  /* 00D4 */ uint8_t is_encrypted;
  /* 00D5 */ uint8_t basis;
  /* 00D6 */ parray<uint8_t, 2> unused;
  /* 00D8 */ parray<uint8_t, 1000> card_counts;
  /* 04C0 */ parray<be_uint16_t, 50> card_count_checksums;
  /* 0524 */ parray<be_uint64_t, 300> rare_tokens;
  /* 0E84 */ parray<DeckDefinition, 25> decks;
  /* 1B68 */ parray<uint8_t, 0x08> unknown_a8;
  /* 1B70 */ be_uint32_t offline_clv_exp;
  /* 1B74 */ be_uint32_t online_clv_exp;
  /* 1B78 */ parray<PlayerConfig::PlayerReference, 10> recent_human_opponents;
  /* 1C90 */ parray<be_uint32_t, 5> recent_battle_start_timestamps;
  /* 1CA4 */ parray<uint8_t, 0x14> unknown_a10;
  /* 1CB8 */ be_uint32_t init_timestamp;
  /* 1CBC */ be_uint32_t last_online_battle_start_timestamp;
  /* 1CC0 */ be_uint32_t unknown_t3;
  /* 1CC4 */ parray<uint8_t, 0x94> unknown_a14;
  /* 1D58 */

  PlayerConfigNTE() = default;
  explicit PlayerConfigNTE(const PlayerConfig& config);
  operator PlayerConfig() const;

  void decrypt();
  void encrypt(uint8_t basis);
} __packed_ws__(PlayerConfigNTE, 0x1D58);

enum class HPType : uint8_t {
  DEFEAT_PLAYER = 0,
  DEFEAT_TEAM = 1,
  COMMON_HP = 2,
};

enum class DiceExchangeMode : uint8_t {
  HIGH_ATK = 0,
  HIGH_DEF = 1,
  NONE = 2,
};

enum class AllowedCards : uint8_t {
  ALL = 0,
  N_ONLY = 1,
  N_R_ONLY = 2,
  N_R_S_ONLY = 3,
};

struct Rules {
  // When this structure is used in a map/quest definition, FF in any of these
  // fields means the user is allowed to override it. Any non-FF fields are
  // fixed for the map/quest and cannot be overridden.
  // The overall time limit is specified in increments of 5 minutes; that is,
  // 1 means 5 minutes, 2 means 10 minutes, etc. 0 means no overall time limit.
  /* 00 */ uint8_t overall_time_limit = 0;
  /* 01 */ uint8_t phase_time_limit = 0; // In seconds; 0 = unlimited
  /* 02 */ AllowedCards allowed_cards = AllowedCards::ALL;
  /* 03 */ uint8_t min_dice_value = 1; // 0 = default (1)
  /* 04 */ uint8_t max_dice_value = 6; // 0 = default (6)
  /* 05 */ uint8_t disable_deck_shuffle = 0; // 0 = shuffle on, 1 = off
  /* 06 */ uint8_t disable_deck_loop = 0; // 0 = loop on, 1 = off
  /* 07 */ uint8_t char_hp = 15;
  /* 08 */ HPType hp_type = HPType::DEFEAT_PLAYER;
  /* 09 */ uint8_t no_assist_cards = 0; // 1 = assist cards disallowed
  /* 0A */ uint8_t disable_dialogue = 0; // 0 = dialogue on, 1 = dialogue off
  /* 0B */ DiceExchangeMode dice_exchange_mode = DiceExchangeMode::HIGH_ATK;
  /* 0C */ uint8_t disable_dice_boost = 0; // 0 = dice boost on, 1 = off
  // NOTE: The following fields are unused in PSO's implementation, but newserv
  // uses them to implement extended rules.
  /* 0D */ uint8_t def_dice_value_range = 0; // High 4 bits = min, low 4 = max
  // These fields specify override dice ranges for the 1-player team in 2v1
  /* 0E */ uint8_t atk_dice_value_range_2v1 = 0; // High 4 bits = min, low 4 = max
  /* 0F */ uint8_t def_dice_value_range_2v1 = 0; // High 4 bits = min, low 4 = max
  /* 10 */ parray<uint8_t, 4> unused;
  /* 14 */

  // Annoyingly, this structure is a different size in Episode 3 Trial Edition.
  // This means that many command formats, as well as the map format, are
  // different, and the existing Server implementation can't serve Trial Edition
  // clients. It'd be nice to support Trial Edition battles, but that would
  // likely be more work than it's worth.

  Rules() = default;
  explicit Rules(const phosg::JSON& json);
  phosg::JSON json() const;
  bool operator==(const Rules& other) const = default;
  bool operator!=(const Rules& other) const = default;
  void clear();
  void set_defaults();

  bool check_invalid_fields() const;
  bool check_and_reset_invalid_fields();

  std::pair<uint8_t, uint8_t> atk_dice_range(bool is_1p_2v1) const;
  std::pair<uint8_t, uint8_t> def_dice_range(bool is_1p_2v1) const;

  std::string str() const;
} __packed_ws__(Rules, 0x14);

struct RulesTrial {
  // Most fields here have the same meanings as in the final version.
  /* 00 */ uint8_t overall_time_limit = 0;
  /* 01 */ uint8_t phase_time_limit = 0;
  /* 02 */ AllowedCards allowed_cards = AllowedCards::ALL;
  // In NTE, the dice behave differently than in non-NTE. A zero in either of
  // these fields means the corresponding die is random in the range [1, 6];
  // any nonzero value means that die will always take that value.
  /* 03 */ uint8_t atk_die_behavior = 0;
  /* 04 */ uint8_t def_die_behavior = 0;
  /* 05 */ uint8_t disable_deck_shuffle = 0;
  /* 06 */ uint8_t disable_deck_loop = 0;
  /* 07 */ uint8_t char_hp = 15;
  /* 08 */ HPType hp_type = HPType::DEFEAT_PLAYER;
  /* 09 */ uint8_t no_assist_cards = 0;
  /* 0A */ uint8_t disable_dialogue = 0;
  /* 0B */ DiceExchangeMode dice_exchange_mode = DiceExchangeMode::HIGH_ATK;
  /* 0C */

  RulesTrial() = default;
  RulesTrial(const Rules&);
  operator Rules() const;
} __packed_ws__(RulesTrial, 0x0C);

struct StateFlags {
  /* 00 */ le_uint16_t turn_num;
  /* 02 */ BattlePhase battle_phase;
  /* 03 */ uint8_t current_team_turn1;
  /* 04 */ uint8_t current_team_turn2;
  /* 05 */ ActionSubphase action_subphase;
  /* 06 */ SetupPhase setup_phase;
  /* 07 */ RegistrationPhase registration_phase;
  /* 08 */ parray<le_uint32_t, 2> team_exp;
  /* 10 */ parray<uint8_t, 2> team_dice_bonus;
  /* 12 */ uint8_t first_team_turn;
  /* 13 */ uint8_t tournament_flag;
  /* 14 */ parray<CardType, 4> client_sc_card_types;
  /* 18 */

  StateFlags();
  bool operator==(const StateFlags& other) const;
  bool operator!=(const StateFlags& other) const;
  void clear();
  void clear_FF();
} __packed_ws__(StateFlags, 0x18);

struct MapList {
  be_uint32_t num_maps;
  be_uint32_t unknown_a1; // Always 0?
  be_uint32_t strings_offset; // From after total_size field (add 0x10 to this value)
  be_uint32_t total_size; // Including header, entries, and strings

  struct Entry {
    // The fields in this structure have the same meanings as the corresponding
    // fields in MapDefinition.
    /* 0000 */ be_uint16_t map_x;
    /* 0002 */ be_uint16_t map_y;
    /* 0004 */ be_uint16_t environment_number;
    /* 0006 */ be_uint16_t map_number;
    // Text offsets are from the beginning of the strings block after all map
    // entries (that is, add strings_offset to them to get the string offset)
    /* 0008 */ be_uint32_t name_offset;
    /* 000C */ be_uint32_t location_name_offset;
    /* 0010 */ be_uint32_t quest_name_offset;
    /* 0014 */ be_uint32_t description_offset;
    /* 0018 */ be_uint16_t width;
    /* 001A */ be_uint16_t height;
    /* 001C */ parray<parray<uint8_t, 0x10>, 0x10> map_tiles;
    /* 011C */ parray<parray<uint8_t, 0x10>, 0x10> modification_tiles;
    /* 021C */ uint8_t map_category;
    /* 021D */ parray<uint8_t, 3> unused;
    /* 0220 */
  } __packed_ws__(Entry, 0x220);

  // Variable-length fields:
  // Entry entries[num_maps];
  // char strings[...EOF]; // Null-terminated strings, pointed to by offsets in Entry structs
} __packed_ws__(MapList, 0x10);

struct CompressedMapHeader { // .mnm file format
  le_uint32_t map_number;
  le_uint32_t compressed_data_size;
  // Compressed data immediately follows (which decompresses to a MapDefinition)
} __packed_ws__(CompressedMapHeader, 8);

struct OverlayState {
  // In the tiles array, the high 4 bits of each value are the tile type, and
  // the low 4 bits are the subtype. The types are:
  // 10: blocked by rock (as if the corresponding map_tiles value was 00)
  // 20: blocked by fence (as if the corresponding map_tiles value was 00)
  // 30-34: teleporters (2 of each value may be present)
  // 40-4F: traps on NTE
  // 40-44: traps on non-NTE (there may be up to 8 of each type, and one of
  //   each is chosen to be a real trap at battle start); the trap types are:
  //   40: Dice Fever, Heavy Fog, Muscular, Immortality, Snail Pace
  //   41: Gold Rush, Charity, Requiem
  //   42: Powerless Rain, Trash 1, Empty Hand, Skip Draw
  //   43: Brave Wind, Homesick, Fly
  //   44: Dice+1, Battle Royale, Reverse Card, Giant Garden, Fix
  // 50: blocked by metal box (appears as an improperly-z-buffered teal cube in
  //   preview; behaves like 10 and 20 in game)
  // Any other value here will behave like 00 (no special tile behavior).
  parray<parray<uint8_t, 0x10>, 0x10> tiles;

  // This field appears to be unused in both NTE and the final version. Perhaps
  // it had some meaning in a pre-NTE version.
  parray<le_uint32_t, 5> unused1;

  // TODO: Figure out exactly where these colors are used
  parray<le_uint32_t, 0x10> trap_tile_colors_nte; // Unused on non-NTE

  // This specifies the assist card IDs that each trap value (40-4F) will set
  // when triggered. This only has an effect on NTE; on non-NTE, this is unused
  // and a fixed set of assist cards is used instead. (On newserv, the set of
  // used assist cards can be overridden in the server configuration.)
  parray<le_uint16_t, 0x10> trap_card_ids_nte;

  OverlayState();
  void clear();
} __packed_ws__(OverlayState, 0x174);

struct MapDefinition { // .mnmd format; also the format of (decompressed) quests
  // If tag is not 0x00000100, the game considers the map to be corrupt in
  // offline mode and will delete it (if it's a download quest). The tag field
  // doesn't seem to have any other use.
  /* 0000 */ be_uint32_t tag;

  /* 0004 */ be_uint32_t map_number; // Must be unique across all maps

  // The maximum map size is 16 tiles in either dimension, since the various
  // tiles arrays below are fixed sizes.
  /* 0008 */ uint8_t width;
  /* 0009 */ uint8_t height;

  // The environment number specifies several things:
  // - The model to load for the main battle stage
  // - The music to play during the main battle
  // - The color of the battle tile outlines (probably)
  // - The preview image to show in the upper-left corner in the map select menu
  // The environment numbers are:
  // 00 = Unguis Lapis ("BONE")
  // 01 = Nebula Montana 1 ("ALPINE")
  // 02 = Lupus Silva 1 ("FOREST 2-1")
  // 03 = Lupus Silva 2 ("FOREST 2-2")
  // 04 = Molae Venti ("WINDMILL")
  // 05 = Nebula Montana 2 ("ALPINE 2")
  // 06 = Tener Sinus ("COAST")
  // 07 = Mortis Fons ("GEYSER")
  // 08 = Morgue (destroyed) ("BROKEN MORGUE")
  // 09 = Tower of Caelum ("TOWER")
  // 0A = ??? (crashes) ("MAPMAN")
  // 0B = Cyber ("CYBER")
  // 0C = Morgue (not destroyed) ("BOSS")
  // 0D = (Castor/Pollux map) ("REAL BOSS")
  // 0E = Dolor Odor ("STOMACH")
  // 0F = Ravum Aedes Sacra ("SACRAMENT")
  // 10 = (Amplum Umbra map) ("RUIN")
  // 11 = Via Tubus ("METRO")
  // 12 = Morgue ("NORMAL MORGUE")
  // Environment numbers above 12 are replaced with 0B (Cyber) if specified in
  // a map definition. The following environment numbers are used internally by
  // the game for various functions, but cannot be specified in MapDefinitions:
  // 13 = TCardDemo (sends CAx14 and CAx13, then softlocks at black screen)
  // 14 = crashes
  // 15 = crashes
  // 16 = Battle results screen
  // 17 = Game Over screen (if used online, client disconnects without saving)
  // 18 = Episode 3 staff roll
  // 19 = View Battle waiting room
  // 1A = TCard00_Select (debug battle setup menu)
  // 1B = nothing (softlocks at black screen)
  /* 000A */ uint8_t environment_number;

  // This field specifies how many of the camera_zone_maps are used.
  /* 000B */ uint8_t num_camera_zones;

  // In the map_tiles array, the values are usually:
  // 00 = not a valid tile (blocked)
  // 01 = valid tile unless modified out (via modification_tiles)
  // 02 = team A start (1v1)
  // 03, 04 = team A start (2v2)
  // 06, 07 = team B start (2v2)
  // 08 = team B start (1v1)
  // These values can be redefined by start_tile_definitions below, however.
  // Note that the game displays the map reversed vertically in the preview
  // window. For example, player 1 is on team A, which usually starts at the top
  // of the map as defined in this struct, or at the bottom as shown in the
  // preview window.
  /* 000C */ parray<parray<uint8_t, 0x10>, 0x10> map_tiles;

  // The start_tile_definitions field is a list of 6 bytes for each team. The
  // low 6 bits of each byte match the starting location for the relevant player
  // in map_tiles; the high 2 bits are the player's initial facing direction.
  // - If the team has 1 player, only byte [0] is used.
  // - If the team has 2 players, bytes [1] and [2] are used.
  // - If the team has 3 players, bytes [3] through [5] are used.
  /* 010C */ parray<parray<uint8_t, 6>, 2> start_tile_definitions;

  struct CameraSpec {
    /* 00 */ parray<be_float, 9> unknown_a1;
    /* 24 */ be_float camera_x;
    /* 28 */ be_float camera_y;
    /* 2C */ be_float camera_z;
    // It appears that the camera always aligns its +Y raster axis with +Y in
    // the virtual world. If the focus point is directly beneath the camera
    // point, the logic for deciding which direction should be "up" from the
    // camera's perspective can get confused and jitter back and forth as the
    // camera moves into position.
    /* 30 */ be_float focus_x;
    /* 34 */ be_float focus_y;
    /* 38 */ be_float focus_z;
    /* 3C */ parray<be_float, 3> unknown_a2;
    /* 48 */

    std::string str() const;
    phosg::JSON json() const;
  } __packed_ws__(CameraSpec, 0x48);

  // This array specifies the camera zone maps. A camera zone map is a subset of
  // the main map (specified in map_tiles). Tiles that are part of each camera
  // zone are 1 in these arrays; all other tiles are 0. The game evaluates each
  // camera zone in order; if all SCs and FCs are within a particular camera
  // zone, then the corresponding camera location is used as the default camera
  // location. If the player doesn't move the camera with the C stick, then the
  // camera zones are evaluated continuously during the battle, and the camera
  // will move to focus on the part of the field where the SCs/FCs are. (Or,
  // more accurately, where the corresponding entry in camera_zone_specs says
  // to focus.) camera_zone_maps is indexed as [team_id][camera_zone_num][y][x];
  // camera_zone_specs is indexed as [team_id][camera_zone_num]. Unused entries
  // (beyond num_camera_zones) in both arrays should be filled with FF bytes.
  /* 0118 */ parray<parray<parray<parray<uint8_t, 0x10>, 0x10>, 10>, 2> camera_zone_maps;
  /* 1518 */ parray<parray<CameraSpec, 10>, 2> camera_zone_specs;
  // These camera specs are used in the Move phase, when the player has chosen
  // an SC or FC to move, or when the player presses Start/Z. Normally these are
  // defined such that the camera is placed high above the map, giving an
  // overhead view of the entire playfield. This is indexed as [???][team_id]
  // (it is not yet known what the major index represents).
  /* 1AB8 */ parray<parray<CameraSpec, 2>, 3> overview_specs;

  // This specifies the locations of blocked tiles, teleporters, and traps. See
  // the comments in OverlayState for details.
  /* 1C68 */ OverlayState overlay_state;

  /* 1DDC */ Rules default_rules;

  /* 1DF0 */ pstring<TextEncoding::MARKED, 0x14> name;
  /* 1E04 */ pstring<TextEncoding::MARKED, 0x14> location_name;
  /* 1E18 */ pstring<TextEncoding::MARKED, 0x3C> quest_name; // == location_name if not a quest
  /* 1E54 */ pstring<TextEncoding::MARKED, 0x190> description;

  // These fields describe where the map cursor on the preview screen should
  // scroll to
  /* 1FE4 */ be_uint16_t map_x;
  /* 1FE6 */ be_uint16_t map_y;

  struct NPCDeck {
    /* 00 */ pstring<TextEncoding::MARKED, 0x18> deck_name;
    /* 18 */ parray<be_uint16_t, 0x20> card_ids; // Last one appears to always be FFFF
    /* 58 */
    phosg::JSON json(uint8_t language) const;
  } __packed_ws__(NPCDeck, 0x58);
  /* 1FE8 */ parray<NPCDeck, 3> npc_decks; // Unused if name[0] == 0

  // These are almost (but not quite) the same format as the entries in
  // aiprm.dat. These entries are only used if the corresponding NPC exists
  // (if .name[0] is not 0) and if the corresponding entry in the
  // npc_ai_params_entry_index is -1.
  struct AIParams {
    /* 0000 */ parray<be_uint16_t, 2> unknown_a1;
    /* 0004 */ uint8_t is_arkz;
    /* 0005 */ parray<uint8_t, 3> unknown_a2;
    /* 0008 */ pstring<TextEncoding::MARKED, 0x10> ai_name;
    // TODO: Figure out exactly how these are used and document here.
    /* 0018 */ parray<be_uint16_t, 0x7E> params;
    /* 0114 */
    phosg::JSON json(uint8_t language) const;
  } __packed_ws__(AIParams, 0x114);
  /* 20F0 */ parray<AIParams, 3> npc_ai_params; // Unused if name[0] == 0

  /* 242C */ parray<uint8_t, 8> unknown_a7;

  // This array specifies which set of predefined AI parameters (in aiprm.dat)
  // to use for each NPC. If a value in this array is -1 (FFFFFFFF), then the
  // corresponding NPC's AI parameters are defined in the AIParams structure
  // above. The names of the AI parameter sets defined in aiprm.dat are:
  // 00 => Sample_Hunter    0D => Sample_Dark    1A => LKnight
  // 01 => Glustar          0E => Break          1B => Boss_Castor
  // 02 => Guykild          0F => Creinu         1C => Boss_Pollux
  // 03 => Inolis           10 => Endu           1D => Sample_Dark
  // 04 => Kilia            11 => Heiz
  // 05 => Kranz            12 => KC
  // 06 => Orland           13 => Lura
  // 07 => Relmitos         14 => memoru
  // 08 => Saligun          15 => Ohgun
  // 09 => Silfer           16 => Peko
  // 0A => Sample_Hunter    17 => Reiz
  // 0B => Teifu            18 => Rio
  // 0C => Viviana          19 => Rufina
  // Presumably 0A is meant to be Stella and 1D is meant to be Amplum Umbra, but
  // they forgot to change the names.
  /* 2434 */ parray<be_int32_t, 3> npc_ai_params_entry_index;

  // before_message appears before the battle if it's not blank. after_message
  // appears after the battle if it's not blank. dispatch_message appears right
  // before the player chooses a deck if it's not blank; usually it says
  // something like "You can only dispatch <character>".
  /* 2440 */ pstring<TextEncoding::MARKED, 0x190> before_message;
  /* 25D0 */ pstring<TextEncoding::MARKED, 0x190> after_message;
  /* 2760 */ pstring<TextEncoding::MARKED, 0x190> dispatch_message;

  struct DialogueSet {
    // Dialogue sets specify lines that COMs can say at certain points during
    // the battle. They only apply to COMs which are defined as NPCs in the map
    // definition; human players and COMs chosen by humans never say lines from
    // dialogue sets.

    // when specifies the situation in which this dialogue set activates. The
    // values 0000-000C (inclusive) can be used here, or FFFF if the entire
    // dialogue set is unused. The known values are:
    //   0001: Activates at battle start if player is an opponent
    //   0006: Activates when below 50% HP
    //   0008: Activates at battle start if player is an ally
    /* 0000 */ be_int16_t when; // 0x00-0x0C, or FFFF if unused
    /* 0002 */ be_uint16_t percent_chance; // 0-100, or FFFF if unused
    // If the dialogue set activates, the game randomly chooses one of these
    // strings, excluding any that are empty or begin with the character '^'.
    /* 0004 */ parray<pstring<TextEncoding::MARKED, 0x40>, 4> strings;
    /* 0104 */
    phosg::JSON json(uint8_t language) const;
  } __packed_ws__(DialogueSet, 0x104);

  // There are up to 0x10 of these per valid NPC, but only the first 13 of them
  // are used, since each one must have a unique value for .when and the values
  // there can only be 0-12.
  /* 28F0 */ parray<parray<DialogueSet, 0x10>, 3> dialogue_sets;

  // These card IDs are always given to the player when they win a battle on
  // this map. Unused entries should be set to FFFF. Cards in this array are
  // ignored if they have any of these features (in the card definition):
  // - type is HUNTERS_SC or ARKZ_SC
  // - card_class is BOSS_ATTACK_ACTION or BOSS_TECH
  // - rank is D1, D2, or D3
  // - cannot_drop is 1 (specifically 1; other values don't prevent cards from
  //   appearing)
  /* 59B0 */ parray<be_uint16_t, 0x10> reward_card_ids;

  // These fields are used when determining which cards to drop after the battle
  // is complete. If either is negative, the player's CLv is used instead.
  /* 59D0 */ be_int32_t win_level_override;
  /* 59D4 */ be_int32_t loss_level_override;

  // The field offsets specify where the battlefield should appear relative to
  // the center of the environment. The size of one tile on the field is 25
  // units in these fields.
  /* 59D8 */ be_int16_t field_offset_x;
  /* 59DA */ be_int16_t field_offset_y;

  // map_category specifies where the map should appear in the maps menu. If
  // this is 0, 1, or 2, the map appears in the Quest section; otherwise, it
  // appears in the Free Battle section instead. It's not known if this controls
  // anything else, or what the difference is in behavior between 0, 1, and 2.
  /* 59DC */ uint8_t map_category;

  // This field determines block graphics to be used in the Cyber environment.
  // There are 10 block types (0-9); if this value is > 9, type 0 is used. This
  // field has no effect in Ep3 NTE, even though there are 6 different block
  // texture files on the NTE disc.
  /* 59DD */ uint8_t cyber_block_type;

  /* 59DE */ be_uint16_t unknown_a11;

  // This array specifies which SC characters can't participate in the quest
  // (that is, the player is not allowed to choose decks with these SC cards).
  // The values in this array don't match the SC card IDs, however:
  //   value in array => SC name (SC card ID)
  //   0000 => Guykild  (0005)      000C => Hyze   (0117)
  //   0001 => Kylria   (0006)      000D => Rufina (0118)
  //   0002 => Saligun  (0110)      000E => Peko   (0119)
  //   0003 => Relmitos (0111)      000F => Creinu (011A)
  //   0004 => Kranz    (0002)      0010 => Reiz   (011B)
  //   0005 => Sil'fer  (0004)      0011 => Lura   (0007)
  //   0006 => Ino'lis  (0003)      0012 => Break  (0008)
  //   0007 => Viviana  (0112)      0013 => Rio    (011C)
  //   0008 => Teifu    (0113)      0014 => Endu   (0116)
  //   0009 => Orland   (0001)      0015 => Memoru (011D)
  //   000A => Stella   (0114)      0016 => K.C.   (011E)
  //   000B => Glustar  (0115)      0017 => Ohgun  (011F)
  // These values normally can't be used by the player, but are recognized
  // internally by the game:
  //   0018 => HERO_1 (02AA)        0021 => DARK_4     (02B3)
  //   0019 => HERO_2 (02AB)        0022 => DARK_5     (02B4)
  //   001A => HERO_3 (02AC)        0023 => DARK_6     (02B5)
  //   001B => HERO_4 (02AD)        0024 => LEUKON     (029B)
  //   001C => HERO_5 (02AE)        0025 => CASTOR     (029C)
  //   001D => HERO_6 (02AF)        0026 => POLLUX     (029D)
  //   001E => DARK_1 (02B0)        0027 => AMPLUM     (029E)
  //   001F => DARK_2 (02B1)        0028 => CASTOR_USR (02BE)
  //   0020 => DARK_3 (02B2)        0029 => POLLUX_USR (02BF)
  // Unused entries in this array should be set to FFFF.
  /* 59E0 */ parray<be_uint16_t, 0x18> unavailable_sc_cards;

  // This array specifies which restrictions apply to each player slot.
  struct EntryState {
    // Values for player_type:
    // 00 = Player (selectable by player, COM decks not allowed)
    // 01 = Player/COM (selectable by player, player and COM decks allowed)
    // 02 = COM (selectable by player, player decks not allowed)
    // 03 = COM (not selectable by player; uses an NPC deck defined above)
    // 04 = NONE (not selectable by player)
    // FF = FREE (same as Player/COM, used in free battle mode)
    uint8_t player_type;
    // Values for deck_type:
    // 00 = HERO ONLY
    // 01 = DARK ONLY
    // FF = any deck allowed
    uint8_t deck_type;

    bool operator==(const EntryState& other) const = default;
    bool operator!=(const EntryState& other) const = default;
    phosg::JSON json() const;
  } __packed_ws__(EntryState, 2);
  /* 5A10 */ parray<EntryState, 4> entry_states;
  /* 5A18 */

  inline bool is_quest() const {
    return (this->map_category <= 2);
  }

  // This function throws runtime_error if the passed-in map is not semantically
  // equivalent to *this. Semantic equivalence means all fields that affect
  // gameplay and visuals are equivalent, but dialogue, names, and description
  // text may differ.
  void assert_semantically_equivalent(const MapDefinition& other) const;

  std::string str(const CardIndex* card_index, uint8_t language) const;
  phosg::JSON json(uint8_t language) const;
} __packed_ws__(MapDefinition, 0x5A18);

struct MapDefinitionTrial {
  // This is the format of Episode 3 Trial Edition maps. See the comments in
  // MapDefinition for what each field means.

  /* 0000 */ be_uint32_t tag;
  /* 0004 */ be_uint32_t map_number;
  /* 0008 */ uint8_t width;
  /* 0009 */ uint8_t height;
  /* 000A */ uint8_t environment_number;
  /* 000B */ uint8_t num_camera_zones;
  /* 000C */ parray<parray<uint8_t, 0x10>, 0x10> map_tiles;
  /* 010C */ parray<parray<uint8_t, 6>, 2> start_tile_definitions;
  /* 0118 */ parray<parray<parray<parray<uint8_t, 0x10>, 0x10>, 10>, 2> camera_zone_maps;
  /* 1518 */ parray<parray<MapDefinition::CameraSpec, 10>, 2> camera_zone_specs;
  /* 1AB8 */ parray<parray<MapDefinition::CameraSpec, 2>, 3> overview_specs;
  /* 1C68 */ OverlayState overlay_state;
  /* 1DDC */ RulesTrial default_rules;
  /* 1DE8 */ pstring<TextEncoding::MARKED, 0x14> name;
  /* 1DFC */ pstring<TextEncoding::MARKED, 0x14> location_name;
  /* 1E10 */ pstring<TextEncoding::MARKED, 0x3C> quest_name;
  /* 1E4C */ pstring<TextEncoding::MARKED, 0x190> description;
  /* 1FDC */ be_uint16_t map_x;
  /* 1FDE */ be_uint16_t map_y;
  /* 1FE0 */ parray<MapDefinition::NPCDeck, 3> npc_decks;
  /* 20E8 */ parray<MapDefinition::AIParams, 3> npc_ai_params;
  /* 2424 */ parray<uint8_t, 8> unknown_a7;
  /* 242C */ parray<be_int32_t, 3> npc_ai_params_entry_index;
  /* 2438 */ pstring<TextEncoding::MARKED, 0x190> before_message;
  /* 25C8 */ pstring<TextEncoding::MARKED, 0x190> after_message;
  /* 2758 */ pstring<TextEncoding::MARKED, 0x190> dispatch_message;
  /* 28E8 */ parray<parray<MapDefinition::DialogueSet, 8>, 3> dialogue_sets;
  /* 4148 */ parray<be_uint16_t, 0x10> reward_card_ids;
  /* 4168 */ be_int32_t win_level_override;
  /* 416C */ be_int32_t loss_level_override;
  /* 4170 */ be_int16_t field_offset_x;
  /* 4172 */ be_int16_t field_offset_y;
  /* 4174 */ uint8_t map_category;
  /* 4175 */ uint8_t cyber_block_type;
  /* 4176 */ be_uint16_t unknown_a11;
  // TODO: This field may contain some version of unavailable_sc_cards and/or
  // entry_states from MapDefinition, but the format isn't the same
  /* 4178 */ parray<uint8_t, 0x28> unknown_t12;
  /* 41A0 */

  MapDefinitionTrial(const MapDefinition& map);
  operator MapDefinition() const;
} __packed_ws__(MapDefinitionTrial, 0x41A0);

struct COMDeckDefinition {
  size_t index;
  std::string player_name;
  std::string deck_name;
  parray<le_uint16_t, 0x1F> card_ids;
};

class CardIndex {
public:
  CardIndex(
      const std::string& filename,
      const std::string& decompressed_filename,
      const std::string& text_filename = "",
      const std::string& decompressed_text_filename = "",
      const std::string& dice_text_filename = "",
      const std::string& decompressed_dice_text_filename = "");

  struct CardEntry {
    CardDefinition def;
    std::string text;
    std::string dice_caption;
    std::string dice_text;
    std::vector<std::string> debug_tags; // Empty unless debug == true
  };

  const std::string& get_compressed_definitions() const;
  std::shared_ptr<const CardEntry> definition_for_id(uint32_t id) const;
  std::shared_ptr<const CardEntry> definition_for_name(const std::string& name) const;
  std::shared_ptr<const CardEntry> definition_for_name_normalized(const std::string& name) const;
  std::set<uint32_t> all_ids() const;
  uint64_t definitions_mtime() const;
  phosg::JSON definitions_json() const;

private:
  static std::string normalize_card_name(const std::string& name);

  std::string compressed_card_definitions;
  std::unordered_map<uint32_t, std::shared_ptr<CardEntry>> card_definitions;
  std::unordered_map<std::string, std::shared_ptr<CardEntry>> card_definitions_by_name;
  std::unordered_map<std::string, std::shared_ptr<CardEntry>> card_definitions_by_name_normalized;
  uint64_t mtime_for_card_definitions;
};

class MapIndex {
public:
  MapIndex(const std::string& directory);

  class VersionedMap {
  public:
    std::shared_ptr<const MapDefinition> map;
    uint8_t language;

    VersionedMap(std::shared_ptr<const MapDefinition> map, uint8_t language);
    VersionedMap(std::string&& compressed_data, uint8_t language);

    std::shared_ptr<const MapDefinitionTrial> trial() const;
    const std::string& compressed(bool is_nte) const;

  private:
    mutable std::shared_ptr<const MapDefinitionTrial> trial_map;
    mutable std::string compressed_data;
    mutable std::string compressed_trial_data;
  };

  class Map {
  public:
    uint32_t map_number;
    std::shared_ptr<const VersionedMap> initial_version;

    explicit Map(std::shared_ptr<const VersionedMap> initial_version);

    void add_version(std::shared_ptr<const VersionedMap> vm);
    bool has_version(uint8_t language) const;
    std::shared_ptr<const VersionedMap> version(uint8_t language) const;
    inline const std::vector<std::shared_ptr<const VersionedMap>>& all_versions() const {
      return this->versions;
    }

  private:
    std::vector<std::shared_ptr<const VersionedMap>> versions;
  };

  const std::string& get_compressed_list(size_t num_players, uint8_t language) const;
  std::shared_ptr<const Map> for_number(uint32_t id) const;
  std::shared_ptr<const Map> for_name(const std::string& name) const;
  std::set<uint32_t> all_numbers() const;

private:
  // The compressed map lists are generated on demand from the maps map below
  mutable std::vector<std::array<std::string, 4>> compressed_map_lists;
  std::map<uint32_t, std::shared_ptr<Map>> maps;
  std::unordered_map<std::string, std::shared_ptr<Map>> maps_by_name;
};

class COMDeckIndex {
public:
  COMDeckIndex(const std::string& filename);

  size_t num_decks() const;
  std::shared_ptr<const COMDeckDefinition> deck_for_index(size_t which) const;
  std::shared_ptr<const COMDeckDefinition> deck_for_name(const std::string& name) const;
  std::shared_ptr<const COMDeckDefinition> random_deck() const;

private:
  std::vector<std::shared_ptr<COMDeckDefinition>> decks;
  std::unordered_map<std::string, std::shared_ptr<COMDeckDefinition>> decks_by_name;
};

} // namespace Episode3

// TODO: Figure out how to declare these inside the Episode3 namespace.
template <>
Episode3::HPType phosg::enum_for_name<Episode3::HPType>(const char* name);
template <>
const char* phosg::name_for_enum<Episode3::HPType>(Episode3::HPType hp_type);
template <>
Episode3::DiceExchangeMode phosg::enum_for_name<Episode3::DiceExchangeMode>(const char* name);
template <>
const char* phosg::name_for_enum<Episode3::DiceExchangeMode>(Episode3::DiceExchangeMode dice_exchange_mode);
template <>
Episode3::AllowedCards phosg::enum_for_name<Episode3::AllowedCards>(const char* name);
template <>
const char* phosg::name_for_enum<Episode3::AllowedCards>(Episode3::AllowedCards allowed_cards);

template <>
const char* phosg::name_for_enum<Episode3::BattlePhase>(Episode3::BattlePhase phase);
template <>
const char* phosg::name_for_enum<Episode3::SetupPhase>(Episode3::SetupPhase phase);
template <>
const char* phosg::name_for_enum<Episode3::RegistrationPhase>(Episode3::RegistrationPhase phase);
template <>
const char* phosg::name_for_enum<Episode3::ActionSubphase>(Episode3::ActionSubphase phase);
template <>
const char* phosg::name_for_enum<Episode3::AttackMedium>(Episode3::AttackMedium medium);
template <>
const char* phosg::name_for_enum<Episode3::CriterionCode>(Episode3::CriterionCode code);
template <>
const char* phosg::name_for_enum<Episode3::CardType>(Episode3::CardType type);
template <>
const char* phosg::name_for_enum<Episode3::CardClass>(Episode3::CardClass cc);
template <>
const char* phosg::name_for_enum<Episode3::ConditionType>(Episode3::ConditionType cond_type);
template <>
const char* phosg::name_for_enum<Episode3::EffectWhen>(Episode3::EffectWhen when);
template <>
const char* phosg::name_for_enum<Episode3::Direction>(Episode3::Direction d);
