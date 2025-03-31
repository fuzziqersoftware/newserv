#pragma once

#include <inttypes.h>

#include <phosg/Tools.hh>

#include "StaticGameData.hh"
#include "Types.hh"

enum class EnemyType : uint8_t {
  UNKNOWN = 0,
  NONE,
  NON_ENEMY_NPC,
  AL_RAPPY,
  ASTARK,
  BA_BOOTA,
  BARBA_RAY,
  BARBAROUS_WOLF,
  BEE_L,
  BEE_R,
  BOOMA,
  BOOTA,
  BULCLAW,
  BULK,
  CANADINE,
  CANADINE_GROUP,
  CANANE,
  CHAOS_BRINGER,
  CHAOS_SORCERER,
  CLAW,
  DARK_BELRA,
  DARK_FALZ_1,
  DARK_FALZ_2,
  DARK_FALZ_3,
  DARK_GUNNER,
  DARK_GUNNER_CONTROL,
  DARVANT,
  DARVANT_ULTIMATE,
  DE_ROL_LE,
  DE_ROL_LE_BODY,
  DE_ROL_LE_MINE,
  DEATH_GUNNER,
  DEL_LILY,
  DEL_RAPPY_CRATER,
  DEL_RAPPY_DESERT,
  DELBITER,
  DELDEPTH,
  DELSABER,
  DIMENIAN,
  DOLMDARL,
  DOLMOLM,
  DORPHON,
  DORPHON_ECLAIR,
  DRAGON,
  DUBCHIC,
  DUBWITCH, // Has no entry in battle params
  EGG_RAPPY,
  EPSIGARD,
  EPSILON,
  EVIL_SHARK,
  GAEL_OR_GIEL,
  GAL_GRYPHON,
  GARANZ,
  GEE,
  GI_GUE,
  GIBBLES,
  GIGOBOOMA,
  GILLCHIC,
  GIRTABLULU,
  GOBOOMA,
  GOL_DRAGON,
  GORAN,
  GORAN_DETONATOR,
  GRASS_ASSASSIN,
  GUIL_SHARK,
  HALLO_RAPPY,
  HIDOOM,
  HILDEBEAR,
  HILDEBLUE,
  ILL_GILL,
  KONDRIEU,
  LA_DIMENIAN,
  LOVE_RAPPY,
  MERICARAND,
  MERICAROL,
  MERICUS,
  MERIKLE,
  MERILLIA,
  MERILTAS,
  MERISSA_A,
  MERISSA_AA,
  MIGIUM,
  MONEST,
  MORFOS,
  MOTHMANT,
  NANO_DRAGON,
  NAR_LILY,
  OLGA_FLOW_1,
  OLGA_FLOW_2,
  PAL_SHARK,
  PAN_ARMS,
  PAZUZU_CRATER,
  PAZUZU_DESERT,
  PIG_RAY,
  POFUILLY_SLIME,
  POUILLY_SLIME,
  POISON_LILY,
  PYRO_GORAN,
  RAG_RAPPY,
  RECOBOX,
  RECON,
  SAINT_MILION,
  SAINT_RAPPY,
  SAND_RAPPY_CRATER,
  SAND_RAPPY_DESERT,
  SATELLITE_LIZARD_CRATER,
  SATELLITE_LIZARD_DESERT,
  SAVAGE_WOLF,
  SHAMBERTIN,
  SINOW_BEAT,
  SINOW_BERILL,
  SINOW_GOLD,
  SINOW_SPIGELL,
  SINOW_ZELE,
  SINOW_ZOA,
  SO_DIMENIAN,
  UL_GIBBON,
  VOL_OPT_1,
  VOL_OPT_2,
  VOL_OPT_AMP,
  VOL_OPT_CORE,
  VOL_OPT_MONITOR,
  VOL_OPT_PILLAR,
  YOWIE_CRATER,
  YOWIE_DESERT,
  ZE_BOOTA,
  ZOL_GIBBON,
  ZU_CRATER,
  ZU_DESERT,
  MAX_ENEMY_TYPE,
};

struct EnemyTypeDefinition {
  enum Flag : uint8_t {
    VALID_EP1 = 0x01,
    VALID_EP2 = 0x02,
    VALID_EP4 = 0x04,
    IS_RARE = 0x08,
  };
  EnemyType type;
  uint8_t flags;
  uint8_t rt_index; // 0xFF if not valid (e.g. not an enemy)
  uint8_t bp_index; // 0xFF if not valid (e.g. not an enemy)
  const char* enum_name;
  const char* in_game_name;
  const char* ultimate_name; // May be null if same as in_game_name

  inline bool valid_in_episode(Episode ep) const {
    switch (ep) {
      case Episode::EP1:
        return (this->flags & Flag::VALID_EP1);
      case Episode::EP2:
        return (this->flags & Flag::VALID_EP2);
      case Episode::EP4:
        return (this->flags & Flag::VALID_EP4);
      default:
        throw std::logic_error("invalid episode number");
    }
  }
  inline bool is_rare() const {
    return (this->flags & Flag::IS_RARE);
  }
  EnemyType rare_type(Episode episode, uint8_t event, uint8_t floor) const;
};

const EnemyTypeDefinition& type_definition_for_enemy(EnemyType type);

template <>
const char* phosg::name_for_enum<EnemyType>(EnemyType type);
template <>
EnemyType phosg::enum_for_name<EnemyType>(const char* name);

const std::vector<EnemyType>& enemy_types_for_rare_table_index(Episode episode, uint8_t rt_index);
