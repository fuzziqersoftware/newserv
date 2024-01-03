#include "Map.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ItemCreator.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "Quest.hh"
#include "StaticGameData.hh"

using namespace std;

static constexpr float UINT32_MAX_AS_FLOAT = 4294967296.0f;

const char* Map::name_for_object_type(uint16_t type) {
  switch (type) {
    case 0x0000:
      return "TObjPlayerSet";
    case 0x0001:
      return "TObjParticle";
    case 0x0002:
      return "TObjAreaWarpForest";
    case 0x0003:
      return "TObjMapWarpForest";
    case 0x0004:
      return "TObjLight";
    case 0x0006:
      return "TObjEnvSound";
    case 0x0007:
      return "TObjFogCollision";
    case 0x0008:
      return "TObjEvtCollision";
    case 0x0009:
      return "TObjCollision";
    case 0x000A:
      return "TOMineIcon01";
    case 0x000B:
      return "TOMineIcon02";
    case 0x000C:
      return "TOMineIcon03";
    case 0x000D:
      return "TOMineIcon04";
    case 0x000E:
      return "TObjRoomId";
    case 0x000F:
      return "TOSensorGeneral01";
    case 0x0011:
      return "TEF_LensFlare";
    case 0x0012:
      return "TObjQuestCol";
    case 0x0013:
      return "TOHealGeneral";
    case 0x0014:
      return "TObjMapCsn";
    case 0x0015:
      return "TObjQuestColA";
    case 0x0016:
      return "TObjItemLight";
    case 0x0017:
      return "TObjRaderCol";
    case 0x0018:
      return "TObjFogCollisionSwitch";
    case 0x0019:
      return "TObjWarpBossMulti(off)/TObjWarpBoss(on)";
    case 0x001A:
      return "TObjSinBoard";
    case 0x001B:
      return "TObjAreaWarpQuest";
    case 0x001C:
      return "TObjAreaWarpEnding";
    case 0x001D:
      return "__UNNAMED_001D__";
    case 0x001E:
      return "__UNNAMED_001E__";
    case 0x001F:
      return "TObjRaderHideCol";
    case 0x0020:
      return "TOSwitchItem";
    case 0x0021:
      return "TOSymbolchatColli";
    case 0x0022:
      return "TOKeyCol";
    case 0x0023:
      return "TOAttackableCol";
    case 0x0024:
      return "TOSwitchAttack";
    case 0x0025:
      return "TOSwitchTimer";
    case 0x0026:
      return "TOChatSensor";
    case 0x0027:
      return "TObjRaderIcon";
    case 0x0028:
      return "TObjEnvSoundEx";
    case 0x0029:
      return "TObjEnvSoundGlobal";
    case 0x0040:
      return "TShopGenerator";
    case 0x0041:
      return "TObjLuker";
    case 0x0042:
      return "TObjBgmCol";
    case 0x0043:
      return "TObjCityMainWarp";
    case 0x0044:
      return "TObjCityAreaWarp";
    case 0x0045:
      return "TObjCityMapWarp";
    case 0x0046:
      return "TObjCityDoor_Shop";
    case 0x0047:
      return "TObjCityDoor_Guild";
    case 0x0048:
      return "TObjCityDoor_Warp";
    case 0x0049:
      return "TObjCityDoor_Med";
    case 0x004A:
      return "__UNNAMED_004A__";
    case 0x004B:
      return "TObjCity_Season_EasterEgg";
    case 0x004C:
      return "TObjCity_Season_ValentineHeart";
    case 0x004D:
      return "TObjCity_Season_XmasTree";
    case 0x004E:
      return "TObjCity_Season_XmasWreath";
    case 0x004F:
      return "TObjCity_Season_HalloweenPumpkin";
    case 0x0050:
      return "TObjCity_Season_21_21";
    case 0x0051:
      return "TObjCity_Season_SonicAdv2";
    case 0x0052:
      return "TObjCity_Season_Board";
    case 0x0053:
      return "TObjCity_Season_FireWorkCtrl";
    case 0x0054:
      return "TObjCityDoor_Lobby";
    case 0x0055:
      return "TObjCityMainWarpChallenge";
    case 0x0056:
      return "TODoorLabo";
    case 0x0057:
      return "TObjTradeCollision";
    case 0x0080:
      return "TObjDoor";
    case 0x0081:
      return "TObjDoorKey";
    case 0x0082:
      return "TObjLazerFenceNorm";
    case 0x0083:
      return "TObjLazerFence4";
    case 0x0084:
      return "TLazerFenceSw";
    case 0x0085:
      return "TKomorebi";
    case 0x0086:
      return "TButterfly";
    case 0x0087:
      return "TMotorcycle";
    case 0x0088:
      return "TObjContainerItem";
    case 0x0089:
      return "TObjTank";
    case 0x008B:
      return "TObjComputer";
    case 0x008C:
      return "TObjContainerIdo";
    case 0x008D:
      return "TOCapsuleAncient01";
    case 0x008E:
      return "TOBarrierEnergy01";
    case 0x008F:
      return "TObjHashi";
    case 0x0090:
      return "TOKeyGenericSw";
    case 0x0091:
      return "TObjContainerEnemy";
    case 0x0092:
      return "TObjContainerBase";
    case 0x0093:
      return "TObjContainerAbeEnemy";
    case 0x0095:
      return "TObjContainerNoItem";
    case 0x0096:
      return "TObjLazerFenceExtra";
    case 0x00C0:
      return "TOKeyCave01";
    case 0x00C1:
      return "TODoorCave01";
    case 0x00C2:
      return "TODoorCave02";
    case 0x00C3:
      return "TOHangceilingCave01Key/TOHangceilingCave01Normal/TOHangceilingCave01KeyQuick";
    case 0x00C4:
      return "TOSignCave01";
    case 0x00C5:
      return "TOSignCave02";
    case 0x00C6:
      return "TOSignCave03";
    case 0x00C7:
      return "TOAirconCave01";
    case 0x00C8:
      return "TOAirconCave02";
    case 0x00C9:
      return "TORevlightCave01";
    case 0x00CB:
      return "TORainbowCave01";
    case 0x00CC:
      return "TOKurage";
    case 0x00CD:
      return "TODragonflyCave01";
    case 0x00CE:
      return "TODoorCave03";
    case 0x00CF:
      return "TOBind";
    case 0x00D0:
      return "TOCakeshopCave01";
    case 0x00D1:
      return "TORockCaveS01";
    case 0x00D2:
      return "TORockCaveM01";
    case 0x00D3:
      return "TORockCaveL01";
    case 0x00D4:
      return "TORockCaveS02";
    case 0x00D5:
      return "TORockCaveM02";
    case 0x00D6:
      return "TORockCaveL02";
    case 0x00D7:
      return "TORockCaveSS02";
    case 0x00D8:
      return "TORockCaveSM02";
    case 0x00D9:
      return "TORockCaveSL02";
    case 0x00DA:
      return "TORockCaveS03";
    case 0x00DB:
      return "TORockCaveM03";
    case 0x00DC:
      return "TORockCaveL03";
    case 0x00DE:
      return "TODummyKeyCave01";
    case 0x00DF:
      return "TORockCaveBL01";
    case 0x00E0:
      return "TORockCaveBL02";
    case 0x00E1:
      return "TORockCaveBL03";
    case 0x0100:
      return "TODoorMachine01";
    case 0x0101:
      return "TOKeyMachine01";
    case 0x0102:
      return "TODoorMachine02";
    case 0x0103:
      return "TOCapsuleMachine01";
    case 0x0104:
      return "TOComputerMachine01";
    case 0x0105:
      return "TOMonitorMachine01";
    case 0x0106:
      return "TODragonflyMachine01";
    case 0x0107:
      return "TOLightMachine01";
    case 0x0108:
      return "TOExplosiveMachine01";
    case 0x0109:
      return "TOExplosiveMachine02";
    case 0x010A:
      return "TOExplosiveMachine03";
    case 0x010B:
      return "TOSparkMachine01";
    case 0x010C:
      return "TOHangerMachine01";
    case 0x0130:
      return "TODoorVoShip";
    case 0x0140:
      return "TObjGoalWarpAncient";
    case 0x0141:
      return "TObjMapWarpAncient";
    case 0x0142:
      return "TOKeyAncient02";
    case 0x0143:
      return "TOKeyAncient03";
    case 0x0144:
      return "TODoorAncient01";
    case 0x0145:
      return "TODoorAncient03";
    case 0x0146:
      return "TODoorAncient04";
    case 0x0147:
      return "TODoorAncient05";
    case 0x0148:
      return "TODoorAncient06";
    case 0x0149:
      return "TODoorAncient07";
    case 0x014A:
      return "TODoorAncient08";
    case 0x014B:
      return "TODoorAncient09";
    case 0x014C:
      return "TOSensorAncient01";
    case 0x014D:
      return "TOKeyAncient01";
    case 0x014E:
      return "TOFenceAncient01";
    case 0x014F:
      return "TOFenceAncient02";
    case 0x0150:
      return "TOFenceAncient03";
    case 0x0151:
      return "TOFenceAncient04";
    case 0x0152:
      return "TContainerAncient01";
    case 0x0153:
      return "TOTrapAncient01";
    case 0x0154:
      return "TOTrapAncient02";
    case 0x0155:
      return "TOMonumentAncient01";
    case 0x0156:
      return "TOMonumentAncient02";
    case 0x0159:
      return "TOWreckAncient01";
    case 0x015A:
      return "TOWreckAncient02";
    case 0x015B:
      return "TOWreckAncient03";
    case 0x015C:
      return "TOWreckAncient04";
    case 0x015D:
      return "TOWreckAncient05";
    case 0x015E:
      return "TOWreckAncient06";
    case 0x015F:
      return "TOWreckAncient07";
    case 0x0160:
      return "TObjFogCollisionPoison/TObjWarpBoss03";
    case 0x0161:
      return "TOContainerAncientItemCommon";
    case 0x0162:
      return "TOContainerAncientItemRare";
    case 0x0163:
      return "TOContainerAncientEnemyCommon";
    case 0x0164:
      return "TOContainerAncientEnemyRare";
    case 0x0165:
      return "TOContainerAncientItemNone";
    case 0x0166:
      return "TOWreckAncientBrakable05";
    case 0x0167:
      return "TOTrapAncient02R";
    case 0x0170:
      return "TOBoss4Bird";
    case 0x0171:
      return "TOBoss4Tower";
    case 0x0172:
      return "TOBoss4Rock";
    case 0x0180:
      return "TObjInfoCol";
    case 0x0181:
      return "TObjWarpLobby";
    case 0x0182:
      return "TObjLobbyMain";
    case 0x0183:
      return "__TObjPathObj_subclass_0183__";
    case 0x0184:
      return "TObjButterflyLobby";
    case 0x0185:
      return "TObjRainbowLobby";
    case 0x0186:
      return "TObjKabochaLobby";
    case 0x0187:
      return "TObjStendGlassLobby";
    case 0x0188:
      return "TObjCurtainLobby";
    case 0x0189:
      return "TObjWeddingLobby";
    case 0x018A:
      return "TObjTreeLobby";
    case 0x018B:
      return "TObjSuisouLobby";
    case 0x018C:
      return "TObjParticleLobby";
    case 0x0190:
      return "TObjCamera";
    case 0x0191:
      return "TObjTuitate";
    case 0x0192:
      return "TObjDoaEx01";
    case 0x0193:
      return "TObjBigTuitate";
    case 0x01A0:
      return "TODoorVS2Door01";
    case 0x01A1:
      return "TOVS2Wreck01";
    case 0x01A2:
      return "TOVS2Wreck02";
    case 0x01A3:
      return "TOVS2Wreck03";
    case 0x01A4:
      return "TOVS2Wreck04";
    case 0x01A5:
      return "TOVS2Wreck05";
    case 0x01A6:
      return "TOVS2Wreck06";
    case 0x01A7:
      return "TOVS2Wall01";
    case 0x01A8:
      return "__UNNAMED_01A8__";
    case 0x01A9:
      return "TObjHashiVersus1";
    case 0x01AA:
      return "TObjHashiVersus2";
    case 0x01AB:
      return "TODoorFourLightRuins";
    case 0x01C0:
      return "TODoorFourLightSpace";
    case 0x0200:
      return "TObjContainerJung";
    case 0x0201:
      return "TObjWarpJung";
    case 0x0202:
      return "TObjDoorJung";
    case 0x0203:
      return "TObjContainerJungEx";
    case 0x0204:
      return "TODoorJungleMain";
    case 0x0205:
      return "TOKeyJungleMain";
    case 0x0206:
      return "TORockJungleS01";
    case 0x0207:
      return "TORockJungleM01";
    case 0x0208:
      return "TORockJungleL01";
    case 0x0209:
      return "TOGrassJungle";
    case 0x020A:
      return "TObjWarpJungMain";
    case 0x020B:
      return "TBGLightningCtrl";
    case 0x020C:
      return "__TObjPathObj_subclass_020C__";
    case 0x020D:
      return "__TObjPathObj_subclass_020D__";
    case 0x020E:
      return "TObjContainerJungEnemy";
    case 0x020F:
      return "TOTrapChainSawDamage";
    case 0x0210:
      return "TOTrapChainSawKey";
    case 0x0211:
      return "TOBiwaMushi";
    case 0x0212:
      return "__TObjPathObj_subclass_0212__";
    case 0x0213:
      return "TOJungleDesign";
    case 0x0220:
      return "TObjFish";
    case 0x0221:
      return "TODoorFourLightSeabed";
    case 0x0222:
      return "TODoorFourLightSeabedU";
    case 0x0223:
      return "TObjSeabedSuiso_CH";
    case 0x0224:
      return "TObjSeabedSuisoBrakable";
    case 0x0225:
      return "TOMekaFish00";
    case 0x0226:
      return "TOMekaFish01";
    case 0x0227:
      return "__TObjPathObj_subclass_0227__";
    case 0x0228:
      return "TOTrapSeabed01";
    case 0x0229:
      return "TOCapsuleLabo";
    case 0x0240:
      return "TObjParticle";
    case 0x0280:
      return "__TObjAreaWarpForest_subclass_0280__";
    case 0x02A0:
      return "TObjLiveCamera";
    case 0x02B0:
      return "TContainerAncient01R";
    case 0x02B1:
      return "TObjLaboDesignBase";
    case 0x02B2:
      return "TObjLaboDesignBase";
    case 0x02B3:
      return "TObjLaboDesignBase";
    case 0x02B4:
      return "TObjLaboDesignBase";
    case 0x02B5:
      return "TObjLaboDesignBase";
    case 0x02B6:
      return "TObjLaboDesignBase";
    case 0x02B7:
      return "TObjGbAdvance";
    case 0x02B8:
      return "TObjQuestColALock2";
    case 0x02B9:
      return "TObjMapForceWarp";
    case 0x02BA:
      return "TObjQuestCol2";
    case 0x02BB:
      return "TODoorLaboNormal";
    case 0x02BC:
      return "TObjAreaWarpEndingJung";
    case 0x02BD:
      return "TObjLaboMapWarp";
    case 0x0300:
      return "__UNKNOWN_0300__";
    case 0x0301:
      return "__UNKNOWN_0301__";
    case 0x0302:
      return "__UNKNOWN_0302__";
    case 0x0303:
      return "__UNKNOWN_0303__";
    case 0x0340:
      return "__UNKNOWN_0340__";
    case 0x0341:
      return "__UNKNOWN_0341__";
    case 0x0380:
      return "__UNKNOWN_0380__";
    case 0x0381:
      return "__UNKNOWN_0381__";
    case 0x0382:
      return "__UNKNOWN_0382__";
    case 0x0383:
      return "__UNKNOWN_0383__";
    case 0x0385:
      return "__UNKNOWN_0385__";
    case 0x0386:
      return "__UNKNOWN_0386__";
    case 0x0387:
      return "__UNKNOWN_0387__";
    case 0x0388:
      return "__UNKNOWN_0388__";
    case 0x0389:
      return "__UNKNOWN_0389__";
    case 0x038A:
      return "__UNKNOWN_038A__";
    case 0x038B:
      return "__UNKNOWN_038B__";
    case 0x038C:
      return "__UNKNOWN_038C__";
    case 0x038D:
      return "__UNKNOWN_038D__";
    case 0x038E:
      return "__UNKNOWN_038E__";
    case 0x038F:
      return "__UNKNOWN_038F__";
    case 0x0390:
      return "__UNKNOWN_0390__";
    case 0x0391:
      return "__UNKNOWN_0391__";
    case 0x03C0:
      return "__UNKNOWN_03C0__";
    case 0x03C1:
      return "__UNKNOWN_03C1__";
    default:
      return "__UNKNOWN__";
  }
}

Map::RareEnemyRates::RareEnemyRates(uint32_t enemy_rate, uint32_t boss_rate)
    : hildeblue(enemy_rate),
      rappy(enemy_rate),
      nar_lily(enemy_rate),
      pouilly_slime(enemy_rate),
      merissa_aa(enemy_rate),
      pazuzu(enemy_rate),
      dorphon_eclair(enemy_rate),
      kondrieu(boss_rate) {}

Map::RareEnemyRates::RareEnemyRates(const JSON& json)
    : hildeblue(json.get_int("Hildeblue")),
      rappy(json.get_int("Rappy")),
      nar_lily(json.get_int("NarLily")),
      pouilly_slime(json.get_int("PouillySlime")),
      merissa_aa(json.get_int("MerissaAA")),
      pazuzu(json.get_int("Pazuzu")),
      dorphon_eclair(json.get_int("DorphonEclair")),
      kondrieu(json.get_int("Kondrieu")) {}

JSON Map::RareEnemyRates::json() const {
  return JSON::dict({
      {"Hildeblue", this->hildeblue},
      {"Rappy", this->rappy},
      {"NarLily", this->nar_lily},
      {"PouillySlime", this->pouilly_slime},
      {"MerissaAA", this->merissa_aa},
      {"Pazuzu", this->pazuzu},
      {"DorphonEclair", this->dorphon_eclair},
      {"Kondrieu", this->kondrieu},
  });
}

string Map::ObjectEntry::str() const {
  return string_printf("[ObjectEntry type=%04hX flags=%04hX index=%04hX a2=%04hX entity_id=%04hX group=%04hX section=%04hX a3=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 "] unused=%08" PRIX32 "]",
      this->base_type.load(),
      this->flags.load(),
      this->index.load(),
      this->unknown_a2.load(),
      this->entity_id.load(),
      this->group.load(),
      this->section.load(),
      this->unknown_a3.load(),
      this->x.load(),
      this->y.load(),
      this->z.load(),
      this->x_angle.load(),
      this->y_angle.load(),
      this->z_angle.load(),
      this->param1.load(),
      this->param2.load(),
      this->param3.load(),
      this->param4.load(),
      this->param5.load(),
      this->param6.load(),
      this->unused.load());
}

string Map::EnemyEntry::str() const {
  return string_printf("[EnemyEntry type=%04hX flags=%04hX index=%04hX num_children=%04hX floor=%04hX entity_id=%04hX section=%04hX wave_number=%04hX wave_number2=%04hX a1=%04hX x=%g y=%g z=%g x_angle=%08" PRIX32 " y_angle=%08" PRIX32 " z_angle=%08" PRIX32 " params=[%g %g %g %g %g %04hX %04hX] unused=%08" PRIX32 "]",
      this->base_type.load(),
      this->flags.load(),
      this->index.load(),
      this->num_children.load(),
      this->floor.load(),
      this->entity_id.load(),
      this->section.load(),
      this->wave_number.load(),
      this->wave_number2.load(),
      this->unknown_a1.load(),
      this->x.load(),
      this->y.load(),
      this->z.load(),
      this->x_angle.load(),
      this->y_angle.load(),
      this->z_angle.load(),
      this->fparam1.load(),
      this->fparam2.load(),
      this->fparam3.load(),
      this->fparam4.load(),
      this->fparam5.load(),
      this->uparam1.load(),
      this->uparam2.load(),
      this->unused.load());
}

Map::Enemy::Enemy(uint16_t enemy_id, size_t source_index, uint8_t floor, EnemyType type)
    : source_index(source_index),
      enemy_id(enemy_id),
      type(type),
      floor(floor),
      state_flags(0),
      last_hit_by_client_id(0) {
}

string Map::Enemy::str() const {
  return string_printf("[Map::Enemy E-%hX source %zX %s floor=%02hhX flags=%02hhX last_hit_by_client_id=%hu]",
      this->enemy_id, this->source_index, name_for_enum(this->type), this->floor, this->state_flags, this->last_hit_by_client_id);
}

string Map::Object::str() const {
  return string_printf("[Map::Object source %zX %04hX(%s) @%04hX p1=%g p456=[%08" PRIX32 " %08" PRIX32 " %08" PRIX32 "] floor=%02hhX item_drop_checked=%s]",
      this->source_index,
      this->base_type,
      Map::name_for_object_type(this->base_type),
      this->section,
      this->param1,
      this->param4,
      this->param5,
      this->param6,
      this->floor,
      this->item_drop_checked ? "true" : "false");
}

Map::Map(uint32_t lobby_id, std::shared_ptr<PSOLFGEncryption> random_crypt)
    : log(string_printf("[Lobby:%08" PRIX32 ":map] ", lobby_id), lobby_log.min_level),
      random_crypt(random_crypt) {}

void Map::clear() {
  this->objects.clear();
  this->enemies.clear();
  this->rare_enemy_indexes.clear();
}

void Map::add_objects_from_map_data(uint8_t floor, const void* data, size_t size) {
  size_t entry_count = size / sizeof(ObjectEntry);
  if (size != entry_count * sizeof(ObjectEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  const auto* objects = reinterpret_cast<const ObjectEntry*>(data);
  for (size_t z = 0; z < entry_count; z++) {
    uint16_t object_id = this->objects.size();
    this->objects.emplace_back(Object{
        .source_index = z,
        .object_id = object_id,
        .floor = floor,
        .base_type = objects[z].base_type,
        .section = objects[z].section,
        .param1 = objects[z].param1,
        .param3 = objects[z].param3,
        .param4 = objects[z].param4,
        .param5 = objects[z].param5,
        .param6 = objects[z].param6,
        .item_drop_checked = false,
    });
  }
}

bool Map::check_and_log_rare_enemy(bool default_is_rare, uint32_t rare_rate) {
  if (default_is_rare) {
    return true;
  }
  if ((this->rare_enemy_indexes.size() < 0x10) && (this->random_crypt->next() < rare_rate)) {
    this->rare_enemy_indexes.emplace_back(this->enemies.size());
    return true;
  }
  return false;
}

void Map::add_enemy(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    uint8_t floor,
    size_t index,
    const EnemyEntry& e,
    std::shared_ptr<const RareEnemyRates> rare_rates) {
  auto add = [&](EnemyType type) -> void {
    uint16_t enemy_id = this->enemies.size();
    this->enemies.emplace_back(enemy_id, index, floor, type);
  };

  EnemyType child_type = EnemyType::UNKNOWN;
  ssize_t default_num_children = 0;
  switch (e.base_type) {
    case 0x0001: // TObjNpcFemaleBase
    case 0x0002: // TObjNpcFemaleChild
    case 0x0003: // TObjNpcFemaleDwarf
    case 0x0004: // TObjNpcFemaleFat
    case 0x0005: // TObjNpcFemaleMacho
    case 0x0006: // TObjNpcFemaleOld
    case 0x0007: // TObjNpcFemaleTall
    case 0x0008: // TObjNpcMaleBase
    case 0x0009: // TObjNpcMaleChild
    case 0x000A: // TObjNpcMaleDwarf
    case 0x000B: // TObjNpcMaleFat
    case 0x000C: // TObjNpcMaleMacho
    case 0x000D: // TObjNpcMaleOld
    case 0x000E: // TObjNpcMaleTall
    case 0x0019: // TObjNpcSoldierBase
    case 0x001A: // TObjNpcSoldierMacho
    case 0x001B: // TObjNpcGovernorBase
    case 0x001C: // TObjNpcConnoisseur
    case 0x001D: // TObjNpcCloakroomBase
    case 0x001E: // TObjNpcExpertBase
    case 0x001F: // TObjNpcNurseBase
    case 0x0020: // TObjNpcSecretaryBase
    case 0x0021: // TObjNpcHHM00
    case 0x0022: // TObjNpcNHW00
    case 0x0024: // TObjNpcHRM00
    case 0x0025: // TObjNpcARM00
    case 0x0026: // TObjNpcARW00
    case 0x0027: // TObjNpcHFW00
    case 0x0028: // TObjNpcNFM00
    case 0x0029: // TObjNpcNFW00
    case 0x002B: // TObjNpcNHW01
    case 0x002C: // TObjNpcAHM01
    case 0x002D: // TObjNpcHRM01
    case 0x0030: // TObjNpcHFW01
    case 0x0031: // TObjNpcNFM01
    case 0x0032: // TObjNpcNFW01
    case 0x0033: // TObjNpcEnemy
    case 0x0045: // TObjNpcLappy
    case 0x0046: // TObjNpcMoja
    case 0x00A9: // TObjNpcBringer
    case 0x00D0: // TObjNpcKenkyu
    case 0x00D1: // TObjNpcSoutokufu
    case 0x00D2: // TObjNpcHosa
    case 0x00D3: // TObjNpcKenkyuW
    case 0x00F0: // TObjNpcHosa2
    case 0x00F1: // TObjNpcKenkyu2
    case 0x00F2: // TObjNpcNgcBase
    case 0x00F3: // TObjNpcNgcBase
    case 0x00F4: // TObjNpcNgcBase
    case 0x00F5: // TObjNpcNgcBase
    case 0x00F6: // TObjNpcNgcBase
    case 0x00F7: // TObjNpcNgcBase
    case 0x00F8: // TObjNpcNgcBase
    case 0x00F9: // TObjNpcNgcBase
    case 0x00FA: // TObjNpcNgcBase
    case 0x00FB: // TObjNpcNgcBase
    case 0x00FC: // TObjNpcNgcBase
    case 0x00FD: // TObjNpcNgcBase
    case 0x00FE: // TObjNpcNgcBase
    case 0x00FF: // TObjNpcNgcBase
    case 0x0100: // Unknown NPC
      // All of these have a default child count of zero
      add(EnemyType::NON_ENEMY_NPC);
      break;

    case 0x0040: // TObjEneMoja
      add(this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->hildeblue)
              ? EnemyType::HILDEBLUE
              : EnemyType::HILDEBEAR);
      break;
    case 0x0041: { // TObjEneLappy
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->rappy);
      switch (episode) {
        case Episode::EP1:
          add(is_rare ? EnemyType::AL_RAPPY : EnemyType::RAG_RAPPY);
          break;
        case Episode::EP2:
          if (is_rare) {
            switch (event) {
              case 0x01:
                add(EnemyType::SAINT_RAPPY);
                break;
              case 0x04:
                add(EnemyType::EGG_RAPPY);
                break;
              case 0x05:
                add(EnemyType::HALLO_RAPPY);
                break;
              default:
                add(EnemyType::LOVE_RAPPY);
            }
          } else {
            add(EnemyType::RAG_RAPPY);
          }
          break;
        case Episode::EP4:
          if (e.floor > 0x05) {
            add(is_rare ? EnemyType::DEL_RAPPY_ALT : EnemyType::SAND_RAPPY_ALT);
          } else {
            add(is_rare ? EnemyType::DEL_RAPPY : EnemyType::SAND_RAPPY);
          }
          break;
        default:
          throw logic_error("invalid episode");
      }
      break;
    }
    case 0x0042: // TObjEneBm3FlyNest
      add(EnemyType::MONEST);
      child_type = EnemyType::MOTHMANT;
      default_num_children = 30;
      break;
    case 0x0043: // TObjEneBm5Wolf
      add(e.fparam2 ? EnemyType::BARBAROUS_WOLF : EnemyType::SAVAGE_WOLF);
      break;
    case 0x0044: { // TObjEneBeast
      static const EnemyType types[3] = {EnemyType::BOOMA, EnemyType::GOBOOMA, EnemyType::GIGOBOOMA};
      add(types[e.uparam1 % 3]);
      break;
    }
    case 0x0060: // TObjGrass
      add(EnemyType::GRASS_ASSASSIN);
      break;
    case 0x0061: // TObjEneRe2Flower
      if ((episode == Episode::EP2) && (e.floor > 0x0F)) {
        add(EnemyType::DEL_LILY);
      } else {
        add(this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->nar_lily)
                ? EnemyType::NAR_LILY
                : EnemyType::POISON_LILY);
      }
      break;
    case 0x0062: // TObjEneNanoDrago
      add(EnemyType::NANO_DRAGON);
      break;
    case 0x0063: { // TObjEneShark
      static const EnemyType types[3] = {EnemyType::EVIL_SHARK, EnemyType::PAL_SHARK, EnemyType::GUIL_SHARK};
      add(types[e.uparam1 % 3]);
      break;
    }
    case 0x0064: // TObjEneSlime
      add(this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->pouilly_slime)
              ? EnemyType::POUILLY_SLIME
              : EnemyType::POFUILLY_SLIME);
      default_num_children = 4;
      break;
    case 0x0065: // TObjEnePanarms
      if ((e.num_children != 0) && (e.num_children != 2)) {
        this->log.warning("PAN_ARMS has an unusual num_children (0x%hX)", e.num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::PAN_ARMS);
      add(EnemyType::HIDOOM);
      add(EnemyType::MIGIUM);
      break;
    case 0x0080: // TObjEneDubchik
      add((e.uparam1 & 0x01) ? EnemyType::GILLCHIC : EnemyType::DUBCHIC);
      break;
    case 0x0081: // TObjEneGyaranzo
      add(EnemyType::GARANZ);
      break;
    case 0x0082: // TObjEneMe3ShinowaReal
      add(e.fparam2 ? EnemyType::SINOW_GOLD : EnemyType::SINOW_BEAT);
      default_num_children = 4;
      break;
    case 0x0083: // TObjEneMe1Canadin
      add(EnemyType::CANADINE);
      break;
    case 0x0084: // TObjEneMe1CanadinLeader
      add(EnemyType::CANANE);
      child_type = EnemyType::CANADINE_GROUP;
      default_num_children = 8;
      break;
    case 0x0085: // TOCtrlDubchik
      add(EnemyType::DUBWITCH);
      break;
    case 0x00A0: // TObjEneSaver
      add(EnemyType::DELSABER);
      break;
    case 0x00A1: // TObjEneRe4Sorcerer
      if ((e.num_children != 0) && (e.num_children != 2)) {
        this->log.warning("CHAOS_SORCERER has an unusual num_children (0x%hX)", e.num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::CHAOS_SORCERER);
      add(EnemyType::BEE_R);
      add(EnemyType::BEE_L);
      break;
    case 0x00A2: // TObjEneDarkGunner
      add(EnemyType::DARK_GUNNER);
      break;
    case 0x00A3: // TObjEneDarkGunCenter
      add(EnemyType::DEATH_GUNNER);
      break;
    case 0x00A4: // TObjEneDf2Bringer
      add(EnemyType::CHAOS_BRINGER);
      break;
    case 0x00A5: // TObjEneRe7Berura
      add(EnemyType::DARK_BELRA);
      break;
    case 0x00A6: { // TObjEneDimedian
      static const EnemyType types[3] = {EnemyType::DIMENIAN, EnemyType::LA_DIMENIAN, EnemyType::SO_DIMENIAN};
      add(types[e.uparam1 % 3]);
      break;
    }
    case 0x00A7: // TObjEneBalClawBody
      add(EnemyType::BULCLAW);
      child_type = EnemyType::CLAW;
      default_num_children = 4;
      break;
    case 0x00A8: // Unnamed subclass of TObjEneBalClawClaw
      add(EnemyType::CLAW);
      break;
    case 0x00C0: // TBoss1Dragon or TBoss5Gryphon
      if (episode == Episode::EP1) {
        add(EnemyType::DRAGON);
      } else if (episode == Episode::EP2) {
        add(EnemyType::GAL_GRYPHON);
      } else {
        throw runtime_error("DRAGON placed outside of Episode 1 or 2");
      }
      break;
    case 0x00C1: // TBoss2DeRolLe
      if ((e.num_children != 0) && (e.num_children != 0x13)) {
        this->log.warning("DE_ROL_LE has an unusual num_children (0x%hX)", e.num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::DE_ROL_LE);
      for (size_t z = 0; z < 0x0A; z++) {
        add(EnemyType::DE_ROL_LE_BODY);
      }
      for (size_t z = 0; z < 0x09; z++) {
        add(EnemyType::DE_ROL_LE_MINE);
      }
      break;
    case 0x00C2: // TBoss3Volopt
      if ((e.num_children != 0) && (e.num_children != 0x23)) {
        this->log.warning("VOL_OPT has an unusual num_children (0x%hX)", e.num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      add(EnemyType::VOL_OPT_1);
      for (size_t z = 0; z < 0x06; z++) {
        add(EnemyType::VOL_OPT_PILLAR);
      }
      for (size_t z = 0; z < 0x18; z++) {
        add(EnemyType::VOL_OPT_MONITOR);
      }
      for (size_t z = 0; z < 0x02; z++) {
        add(EnemyType::NONE);
      }
      add(EnemyType::VOL_OPT_AMP);
      add(EnemyType::VOL_OPT_CORE);
      add(EnemyType::NONE);
      break;
    case 0x00C5: // Unnamed subclass of TObjEnemyCustom
      add(EnemyType::VOL_OPT_2);
      break;
    case 0x00C8: // TBoss4DarkFalz
      if ((e.num_children != 0) && (e.num_children != 0x200)) {
        this->log.warning("DARK_FALZ has an unusual num_children (0x%hX)", e.num_children.load());
      }
      default_num_children = -1; // Skip adding children (because we do it here)
      if (difficulty) {
        add(EnemyType::DARK_FALZ_3);
      } else {
        add(EnemyType::DARK_FALZ_2);
      }
      for (size_t x = 0; x < 0x1FD; x++) {
        add(difficulty == 3 ? EnemyType::DARVANT_ULTIMATE : EnemyType::DARVANT);
      }
      add(EnemyType::DARK_FALZ_3);
      add(EnemyType::DARK_FALZ_2);
      add(EnemyType::DARK_FALZ_1);
      break;
    case 0x00CA: // TBoss6PlotFalz
      add(EnemyType::OLGA_FLOW_2);
      default_num_children = 0x200;
      break;
    case 0x00CB: // TBoss7DeRolLeC
      add(EnemyType::BARBA_RAY);
      child_type = EnemyType::PIG_RAY;
      default_num_children = 0x2F;
      break;
    case 0x00CC: // TBoss8Dragon
      add(EnemyType::GOL_DRAGON);
      default_num_children = 5;
      break;
    case 0x00D4: // TObjEneMe3StelthReal
      add((e.uparam1 & 1) ? EnemyType::SINOW_SPIGELL : EnemyType::SINOW_BERILL);
      default_num_children = 4;
      break;
    case 0x00D5: // TObjEneMerillLia
      add((e.uparam1 & 0x01) ? EnemyType::MERILTAS : EnemyType::MERILLIA);
      break;
    case 0x00D6: // TObjEneBm9Mericarol
      if (e.uparam1 == 0) {
        add(EnemyType::MERICAROL);
      } else {
        add(((e.uparam1 % 3) == 2) ? EnemyType::MERICUS : EnemyType::MERIKLE);
      }
      break;
    case 0x00D7: // TObjEneBm5GibonU
      add((e.uparam1 & 0x01) ? EnemyType::ZOL_GIBBON : EnemyType::UL_GIBBON);
      break;
    case 0x00D8: // TObjEneGibbles
      add(EnemyType::GIBBLES);
      break;
    case 0x00D9: // TObjEneMe1Gee
      add(EnemyType::GEE);
      break;
    case 0x00DA: // TObjEneMe1GiGue
      add(EnemyType::GI_GUE);
      break;
    case 0x00DB: // TObjEneDelDepth
      add(EnemyType::DELDEPTH);
      break;
    case 0x00DC: // TObjEneDellBiter
      add(EnemyType::DELBITER);
      break;
    case 0x00DD: // TObjEneDolmOlm
      add(e.uparam1 ? EnemyType::DOLMDARL : EnemyType::DOLMOLM);
      break;
    case 0x00DE: // TObjEneMorfos
      add(EnemyType::MORFOS);
      break;
    case 0x00DF: // TObjEneRecobox
      add(EnemyType::RECOBOX);
      child_type = EnemyType::RECON;
      break;
    case 0x00E0: // TObjEneMe3SinowZoaReal or TObjEneEpsilonBody
      if ((episode == Episode::EP2) && (e.floor > 0x0F)) {
        add(EnemyType::EPSILON);
        default_num_children = 4;
        child_type = EnemyType::EPSIGUARD;
      } else {
        add((e.uparam1 & 0x01) ? EnemyType::SINOW_ZELE : EnemyType::SINOW_ZOA);
      }
      break;
    case 0x00E1: // TObjEneIllGill
      add(EnemyType::ILL_GILL);
      break;
    case 0x0110:
      add(EnemyType::ASTARK);
      break;
    case 0x0111:
      if (e.floor > 0x05) {
        add(e.fparam2 ? EnemyType::YOWIE_ALT : EnemyType::SATELLITE_LIZARD_ALT);
      } else {
        add(e.fparam2 ? EnemyType::YOWIE : EnemyType::SATELLITE_LIZARD);
      }
      break;
    case 0x0112:
      add(this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->merissa_aa)
              ? EnemyType::MERISSA_AA
              : EnemyType::MERISSA_A);
      break;
    case 0x0113:
      add(EnemyType::GIRTABLULU);
      break;
    case 0x0114: {
      bool is_rare = this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->pazuzu);
      if (e.floor > 0x05) {
        add(is_rare ? EnemyType::PAZUZU_ALT : EnemyType::ZU_ALT);
      } else {
        add(is_rare ? EnemyType::PAZUZU : EnemyType::ZU);
      }
      break;
    }
    case 0x0115:
      if (e.uparam1 & 2) {
        add(EnemyType::BA_BOOTA);
      } else {
        add((e.uparam1 & 1) ? EnemyType::ZE_BOOTA : EnemyType::BOOTA);
      }
      break;
    case 0x0116:
      add(this->check_and_log_rare_enemy(e.uparam1 & 0x01, rare_rates->dorphon_eclair)
              ? EnemyType::DORPHON_ECLAIR
              : EnemyType::DORPHON);
      break;
    case 0x0117: {
      static const EnemyType types[3] = {EnemyType::GORAN, EnemyType::PYRO_GORAN, EnemyType::GORAN_DETONATOR};
      add(types[e.uparam1 % 3]);
      break;
    }
    case 0x0119: {
      bool is_rare = this->check_and_log_rare_enemy((e.fparam2 != 0.0f), rare_rates->kondrieu);
      if (is_rare) {
        add(EnemyType::KONDRIEU);
      } else {
        add((e.uparam1 & 1) ? EnemyType::SHAMBERTIN : EnemyType::SAINT_MILLION);
      }
      default_num_children = 0x18;
      break;
    }

    case 0x00C3: // TBoss3VoloptP01
    case 0x00C4: // TBoss3VoloptCore or subclass
    case 0x00C6: // TBoss3VoloptMonitor
    case 0x00C7: // TBoss3VoloptHiraisin
    case 0x0118:
      add(EnemyType::UNKNOWN);
      this->log.warning(
          "(Entry %zu, offset %zX in file) Unknown enemy type %04hX",
          index, index * sizeof(EnemyEntry), e.base_type.load());
      break;

    default:
      add(EnemyType::UNKNOWN);
      this->log.warning(
          "(Entry %zu, offset %zX in file) Invalid enemy type %04hX",
          index, index * sizeof(EnemyEntry), e.base_type.load());
      break;
  }

  if (default_num_children >= 0) {
    size_t num_children = e.num_children ? e.num_children.load() : default_num_children;
    if ((child_type == EnemyType::UNKNOWN) && !this->enemies.empty()) {
      child_type = this->enemies.back().type;
    }
    for (size_t x = 0; x < num_children; x++) {
      add(child_type);
    }
  }
}

void Map::add_enemies_from_map_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    uint8_t floor,
    const void* data,
    size_t size,
    std::shared_ptr<const RareEnemyRates> rare_rates) {
  size_t entry_count = size / sizeof(EnemyEntry);
  if (size != entry_count * sizeof(EnemyEntry)) {
    throw runtime_error("data size is not a multiple of entry size");
  }

  StringReader r(data, size);
  for (size_t y = 0; y < entry_count; y++) {
    this->add_enemy(episode, difficulty, event, floor, y, r.get<EnemyEntry>(), rare_rates);
  }
}

Map::DATParserRandomState::DATParserRandomState(uint32_t rare_seed)
    : random(rare_seed),
      location_table_random(0),
      location_indexes_populated(0),
      location_indexes_used(0),
      location_entries_base_offset(0) {
  this->location_index_table.fill(0);
}

size_t Map::DATParserRandomState::rand_int_biased(size_t min_v, size_t max_v) {
  float max_f = static_cast<float>(max_v + 1);
  uint32_t crypt_v = this->random.next();
  float det_f = static_cast<float>(crypt_v);
  return max<size_t>(floorf((max_f * det_f) / UINT32_MAX_AS_FLOAT), min_v);
}

uint32_t Map::DATParserRandomState::next_location_index() {
  if (this->location_indexes_used < this->location_indexes_populated) {
    return this->location_index_table.at(this->location_indexes_used++);
  }
  return 0;
}

void Map::DATParserRandomState::generate_shuffled_location_table(
    const Map::RandomEnemyLocationsHeader& header, StringReader r, uint16_t section) {
  if (header.num_sections == 0) {
    throw runtime_error("no locations defined");
  }

  StringReader sections_r = r.sub(header.section_table_offset, header.num_sections * sizeof(Map::RandomEnemyLocationSection));

  size_t bs_min = 0;
  size_t bs_max = header.num_sections - 1;
  do {
    size_t bs_mid = (bs_min + bs_max) / 2;
    if (sections_r.pget<Map::RandomEnemyLocationSection>(bs_mid * sizeof(Map::RandomEnemyLocationSection)).section < section) {
      bs_min = bs_mid + 1;
    } else {
      bs_max = bs_mid;
    }
  } while (bs_min < bs_max);

  const auto& sec = sections_r.pget<Map::RandomEnemyLocationSection>(bs_min * sizeof(Map::RandomEnemyLocationSection));
  if (section != sec.section) {
    return;
  }

  this->location_indexes_populated = sec.count;
  this->location_indexes_used = 0;
  this->location_entries_base_offset = sec.offset;
  for (size_t z = 0; z < sec.count; z++) {
    this->location_index_table.at(z) = z;
  }

  for (size_t z = 0; z < 4; z++) {
    for (size_t x = 0; x < sec.count; x++) {
      uint32_t crypt_v = this->location_table_random.next();
      size_t choice = floorf((static_cast<float>(sec.count) * static_cast<float>(crypt_v)) / UINT32_MAX_AS_FLOAT);
      uint32_t t = this->location_index_table[x];
      this->location_index_table[x] = this->location_index_table[choice];
      this->location_index_table[choice] = t;
    }
  }
}

void Map::add_random_enemies_from_map_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    uint8_t floor,
    StringReader wave_events_segment_r,
    StringReader locations_segment_r,
    StringReader definitions_segment_r,
    std::shared_ptr<DATParserRandomState> random_state,
    std::shared_ptr<const RareEnemyRates> rare_rates) {

  static const array<uint32_t, 41> rand_enemy_base_types = {
      0x44, 0x43, 0x41, 0x42, 0x40, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x80,
      0x81, 0x82, 0x83, 0x84, 0x85, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
      0xA7, 0xA8, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD,
      0xDE, 0xDF, 0xE0, 0xE0, 0xE1};

  const auto& wave_events_header = wave_events_segment_r.get<EventsSectionHeader>();
  if (wave_events_header.format != 0x65767432) { // 'evt2'
    throw runtime_error("cannot generate random enemies from non-evt2 event stream");
  }
  wave_events_segment_r.go(wave_events_header.entries_offset);

  const auto& locations_header = locations_segment_r.get<RandomEnemyLocationsHeader>();
  const auto& definitions_header = definitions_segment_r.get<RandomEnemyDefinitionsHeader>();
  auto definitions_r = definitions_segment_r.sub(
      definitions_header.entries_offset,
      definitions_header.entry_count * sizeof(RandomEnemyDefinition));
  auto weights_r = definitions_segment_r.sub(
      definitions_header.weight_entries_offset,
      definitions_header.weight_entry_count * sizeof(RandomEnemyWeight));

  for (size_t wave_entry_index = 0; wave_entry_index < wave_events_header.entry_count; wave_entry_index++) {
    auto entry_log = this->log.sub(string_printf("(Entry %zu/%" PRIu32 ") ", wave_entry_index, wave_events_header.entry_count.load()));
    const auto& entry = wave_events_segment_r.get<Event2Entry>();

    size_t remaining_waves = random_state->rand_int_biased(1, entry.max_waves);
    // Trace: at 0080E125 EAX is wave count

    uint32_t wave_number = entry.wave_number;
    while (remaining_waves) {
      remaining_waves--;
      auto wave_log = entry_log.sub(string_printf("(Wave %zu) ", remaining_waves));

      size_t remaining_enemies = random_state->rand_int_biased(entry.min_enemies, entry.max_enemies);
      // Trace: at 0080E208 EDI is enemy count

      random_state->generate_shuffled_location_table(locations_header, locations_segment_r, entry.section);
      // Trace: at 0080EBB0 *(EBP + 4) points to table (0x20 uint32_ts)

      while (remaining_enemies) {
        remaining_enemies--;
        auto enemy_log = wave_log.sub(string_printf("(Enemy %zu) ", remaining_enemies));

        // TODO: Factor this sum out of the loops
        weights_r.go(0);
        size_t weight_total = 0;
        while (!weights_r.eof()) {
          weight_total += weights_r.get<RandomEnemyWeight>().weight;
        }
        // Trace: at 0080E2C2 EBX is weight_total

        size_t det = random_state->rand_int_biased(0, weight_total - 1);
        // Trace: at 0080E300 EDX is det

        weights_r.go(0);
        while (!weights_r.eof()) {
          const auto& weight_entry = weights_r.get<RandomEnemyWeight>();
          if (det < weight_entry.weight) {
            if ((weight_entry.base_type_index != 0xFF) && (weight_entry.definition_entry_num != 0xFF)) {
              EnemyEntry e;
              e.base_type = rand_enemy_base_types.at(weight_entry.base_type_index);
              e.wave_number = wave_number;
              e.section = entry.section;
              e.floor = floor;

              size_t bs_min = 0;
              size_t bs_max = definitions_header.entry_count - 1;
              if (bs_max == 0) {
                throw runtime_error("no available random enemy definitions");
              }
              do {
                size_t bs_mid = (bs_min + bs_max) / 2;
                if (definitions_r.pget<RandomEnemyDefinition>(bs_mid * sizeof(RandomEnemyDefinition)).entry_num < weight_entry.definition_entry_num) {
                  bs_min = bs_mid + 1;
                } else {
                  bs_max = bs_mid;
                }
              } while (bs_min < bs_max);

              const auto& def = definitions_r.pget<RandomEnemyDefinition>(bs_min * sizeof(RandomEnemyDefinition));
              if (def.entry_num == weight_entry.definition_entry_num) {
                e.fparam1 = def.fparam1;
                e.fparam2 = def.fparam2;
                e.fparam3 = def.fparam3;
                e.fparam4 = def.fparam4;
                e.fparam5 = def.fparam5;
                e.uparam1 = def.uparam1;
                e.uparam2 = def.uparam2;
                e.num_children = random_state->rand_int_biased(def.min_children, def.max_children);
              } else {
                throw runtime_error("random enemy definition not found");
              }

              const auto& loc = locations_segment_r.pget<RandomEnemyLocationEntry>(
                  locations_header.entries_offset + sizeof(RandomEnemyLocationEntry) * random_state->next_location_index());
              e.x = loc.x;
              e.y = loc.y;
              e.z = loc.z;
              e.x_angle = loc.x_angle;
              e.y_angle = loc.y_angle;
              e.z_angle = loc.z_angle;

              // Trace: at 0080E6FE CX is base_type
              this->add_enemy(episode, difficulty, event, floor, 0, e, rare_rates);
            } else {
              enemy_log.warning("Cannot create enemy: parameters are missing");
            }
            break;
          } else {
            det -= weight_entry.weight;
          }
        }
      }
      if (remaining_waves) {
        // We don't generate the event stream here, but the client does, and in
        // doing so, it uses one value from random to determine the delay
        // parameter of the event. To keep our state in sync with what the
        // client would do, we skip a random value here.
        random_state->random.next();
        wave_number++;
      }
    }

    // For the same reason as above, we need to skip another random value here.
    random_state->random.next();
  }
}

vector<Map::DATSectionsForFloor> Map::collect_quest_map_data_sections(const void* data, size_t size) {
  vector<DATSectionsForFloor> ret;
  StringReader r(data, size);
  while (!r.eof()) {
    size_t header_offset = r.where();
    const auto& header = r.get<SectionHeader>();

    if (header.type() == SectionHeader::Type::END && header.section_size == 0) {
      break;
    }
    if (header.section_size < sizeof(header)) {
      throw runtime_error(string_printf("quest layout has invalid section header at offset 0x%zX", r.where() - sizeof(header)));
    }

    if (header.floor > 0x100) {
      throw runtime_error("section floor number too large");
    }

    if (header.floor >= ret.size()) {
      ret.resize(header.floor + 1);
    }
    auto& floor_sections = ret[header.floor];
    switch (header.type()) {
      case SectionHeader::Type::OBJECTS:
        if (floor_sections.objects != 0xFFFFFFFF) {
          throw runtime_error("multiple objects sections for same floor");
        }
        floor_sections.objects = header_offset;
        break;
      case SectionHeader::Type::ENEMIES:
        if (floor_sections.enemies != 0xFFFFFFFF) {
          throw runtime_error("multiple enemies sections for same floor");
        }
        floor_sections.enemies = header_offset;
        break;
      case SectionHeader::Type::WAVE_EVENTS:
        if (floor_sections.wave_events != 0xFFFFFFFF) {
          throw runtime_error("multiple wave events sections for same floor");
        }
        floor_sections.wave_events = header_offset;
        break;
      case SectionHeader::Type::RANDOM_ENEMY_LOCATIONS:
        if (floor_sections.random_enemy_locations != 0xFFFFFFFF) {
          throw runtime_error("multiple random enemy locations sections for same floor");
        }
        floor_sections.random_enemy_locations = header_offset;
        break;
      case SectionHeader::Type::RANDOM_ENEMY_DEFINITIONS:
        if (floor_sections.random_enemy_definitions != 0xFFFFFFFF) {
          throw runtime_error("multiple random enemy definitions sections for same floor");
        }
        floor_sections.random_enemy_definitions = header_offset;
        break;
      default:
        throw runtime_error("invalid section type");
    }
    r.skip(header.data_size);
  }
  return ret;
}

void Map::add_enemies_and_objects_from_quest_data(
    Episode episode,
    uint8_t difficulty,
    uint8_t event,
    const void* data,
    size_t size,
    uint32_t rare_seed,
    std::shared_ptr<const RareEnemyRates> rare_rates) {
  auto all_floor_sections = this->collect_quest_map_data_sections(data, size);

  StringReader r(data, size);
  shared_ptr<DATParserRandomState> random_state;
  for (size_t floor = 0; floor < all_floor_sections.size(); floor++) {
    const auto& floor_sections = all_floor_sections[floor];

    if (floor_sections.objects != 0xFFFFFFFF) {
      const auto& header = r.pget<SectionHeader>(floor_sections.objects);
      if (header.data_size % sizeof(ObjectEntry)) {
        throw runtime_error("quest layout object section size is not a multiple of object entry size");
      }
      this->log.info("(Floor %02zX) Adding objects", floor);
      this->add_objects_from_map_data(floor, r.pgetv(floor_sections.objects + sizeof(header), header.data_size), header.data_size);
    }

    if (floor_sections.enemies != 0xFFFFFFFF) {
      const auto& header = r.pget<SectionHeader>(floor_sections.enemies);
      if (header.data_size % sizeof(EnemyEntry)) {
        throw runtime_error("quest layout enemy section size is not a multiple of enemy entry size");
      }
      this->log.info("(Floor %02zX) Adding enemies", floor);
      this->add_enemies_from_map_data(
          episode,
          difficulty,
          event,
          floor,
          r.pgetv(floor_sections.enemies + sizeof(header), header.data_size),
          header.data_size,
          rare_rates);

    } else if ((floor_sections.wave_events != 0xFFFFFFFF) &&
        (floor_sections.random_enemy_locations != 0xFFFFFFFF) &&
        (floor_sections.random_enemy_definitions != 0xFFFFFFFF)) {
      this->log.info("(Floor %02zX) Adding random enemies", floor);
      const auto& wave_events_header = r.pget<SectionHeader>(floor_sections.wave_events);
      const auto& random_enemy_locations_header = r.pget<SectionHeader>(floor_sections.random_enemy_locations);
      const auto& random_enemy_definitions_header = r.pget<SectionHeader>(floor_sections.random_enemy_definitions);
      if (!random_state) {
        random_state = make_shared<DATParserRandomState>(rare_seed);
      }
      this->add_random_enemies_from_map_data(
          episode,
          difficulty,
          event,
          floor,
          r.sub(floor_sections.wave_events + sizeof(SectionHeader), wave_events_header.data_size),
          r.sub(floor_sections.random_enemy_locations + sizeof(SectionHeader), random_enemy_locations_header.data_size),
          r.sub(floor_sections.random_enemy_definitions + sizeof(SectionHeader), random_enemy_definitions_header.data_size),
          random_state,
          rare_rates);
    }
  }
}

const Map::Enemy& Map::find_enemy(uint8_t floor, EnemyType type) const {
  return const_cast<Map*>(this)->find_enemy(floor, type);
}

Map::Enemy& Map::find_enemy(uint8_t floor, EnemyType type) {
  if (enemies.empty()) {
    throw out_of_range("no enemies defined");
  }
  // TODO: Linear search is bad here. Do something better, like binary search
  // for the floor start and just linear search through the floor enemies.
  for (auto& e : this->enemies) {
    if (e.floor == floor && e.type == type) {
      return e;
    }
  }
  throw out_of_range("enemy not found");
}

string Map::disassemble_quest_data(const void* data, size_t size) {
  auto all_floor_sections = Map::collect_quest_map_data_sections(data, size);

  deque<string> ret;
  StringReader r(data, size);
  size_t object_number = 0;
  size_t enemy_number = 0;
  for (size_t floor = 0; floor < all_floor_sections.size(); floor++) {
    const auto& floor_sections = all_floor_sections[floor];

    if (floor_sections.objects != 0xFFFFFFFF) {
      ret.emplace_back(string_printf(".objects %zu", floor));
      const auto& header = r.pget<SectionHeader>(floor_sections.objects);
      auto sub_r = r.sub(floor_sections.objects + sizeof(SectionHeader), header.data_size);
      while (sub_r.remaining() >= sizeof(ObjectEntry)) {
        string o_str = sub_r.get<ObjectEntry>().str();
        ret.emplace_back(string_printf("/* K-%zX */ %s", object_number++, o_str.c_str()));
      }
      if (sub_r.remaining()) {
        ret.emplace_back("// Warning: object section size is not a multiple of object entry size");
        size_t offset = floor_sections.objects + sizeof(SectionHeader) + r.where();
        size_t bytes = r.remaining();
        ret.emplace_back(format_data(r.getv(r.remaining()), bytes, offset));
      }
    }

    if (floor_sections.enemies != 0xFFFFFFFF) {
      ret.emplace_back(string_printf(".enemies %zu", floor));
      const auto& header = r.pget<SectionHeader>(floor_sections.enemies);
      auto sub_r = r.sub(floor_sections.enemies + sizeof(SectionHeader), header.data_size);
      while (sub_r.remaining() >= sizeof(EnemyEntry)) {
        string e_str = sub_r.get<EnemyEntry>().str();
        ret.emplace_back(string_printf("/* entry %zX */ %s", enemy_number++, e_str.c_str()));
      }
      if (sub_r.remaining()) {
        ret.emplace_back("// Warning: enemy section size is not a multiple of enemy entry size");
        size_t offset = floor_sections.objects + sizeof(SectionHeader) + r.where();
        size_t bytes = r.remaining();
        ret.emplace_back(format_data(r.getv(r.remaining()), bytes, offset));
      }
    }

    // TODO: Add disassembly for these section types
    if (floor_sections.wave_events != 0xFFFFFFFF) {
      ret.emplace_back(string_printf(".wave_events %zu", floor));
      const auto& header = r.pget<SectionHeader>(floor_sections.wave_events);
      size_t offset = floor_sections.wave_events + sizeof(SectionHeader);
      auto sub_r = r.sub(offset, header.data_size);
      ret.emplace_back(format_data(r.getv(r.remaining()), header.data_size, offset));
    }
    if (floor_sections.random_enemy_locations != 0xFFFFFFFF) {
      ret.emplace_back(string_printf(".random_enemy_locations %zu", floor));
      const auto& header = r.pget<SectionHeader>(floor_sections.random_enemy_locations);
      size_t offset = floor_sections.random_enemy_locations + sizeof(SectionHeader);
      auto sub_r = r.sub(offset, header.data_size);
      ret.emplace_back(format_data(r.getv(r.remaining()), header.data_size, offset));
    }
    if (floor_sections.random_enemy_definitions != 0xFFFFFFFF) {
      ret.emplace_back(string_printf(".random_enemy_definitions %zu", floor));
      const auto& header = r.pget<SectionHeader>(floor_sections.random_enemy_definitions);
      size_t offset = floor_sections.random_enemy_definitions + sizeof(SectionHeader);
      auto sub_r = r.sub(offset, header.data_size);
      ret.emplace_back(format_data(r.getv(r.remaining()), header.data_size, offset));
    }
  }

  return join(ret, "\n") + "\n";
}

SetDataTable::SetDataTable(shared_ptr<const string> data, bool big_endian) {
  if (big_endian) {
    this->load_table_t<true>(data);
  } else {
    this->load_table_t<false>(data);
  }
}

template <bool IsBigEndian>
void SetDataTable::load_table_t(shared_ptr<const string> data) {
  using U32T = typename conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

  StringReader r(*data);

  struct Footer {
    U32T table3_offset;
    U32T table3_count; // In le_uint16_ts (so *2 for size in bytes)
    U32T unknown_a3; // == 1
    U32T unknown_a4; // == 0
    U32T root_table_offset_offset;
    U32T unknown_a6; // == 0
    U32T unknown_a7; // == 0
    U32T unknown_a8; // == 0
  } __attribute__((packed));
  if (r.size() < sizeof(Footer)) {
    throw runtime_error("set data table is too small");
  }
  auto& footer = r.pget<Footer>(r.size() - sizeof(Footer));

  uint32_t root_table_offset = r.pget<U32T>(footer.root_table_offset_offset);
  auto root_r = r.sub(root_table_offset, footer.root_table_offset_offset - root_table_offset);
  while (!root_r.eof()) {
    auto& var1_v = this->entries.emplace_back();
    uint32_t var1_table_offset = root_r.template get<U32T>();
    uint32_t var1_table_count = root_r.template get<U32T>();
    auto var1_r = r.sub(var1_table_offset, var1_table_count * 0x08);
    while (!var1_r.eof()) {
      auto& var2_v = var1_v.emplace_back();
      uint32_t var2_table_offset = var1_r.get<U32T>();
      uint32_t var2_table_count = var1_r.get<U32T>();
      auto var2_r = r.sub(var2_table_offset, var2_table_count * 0x0C);
      while (!var2_r.eof()) {
        auto& entry = var2_v.emplace_back();
        entry.object_list_basename = r.pget_cstr(var2_r.get<U32T>());
        entry.enemy_list_basename = r.pget_cstr(var2_r.get<U32T>());
        entry.event_list_basename = r.pget_cstr(var2_r.get<U32T>());
      }
    }
  }
}

void SetDataTable::print(FILE* stream) const {
  for (size_t a = 0; a < this->entries.size(); a++) {
    const auto& v1_v = this->entries[a];
    for (size_t v1 = 0; v1 < v1_v.size(); v1++) {
      const auto& v2_v = v1_v[v1];
      for (size_t v2 = 0; v2 < v2_v.size(); v2++) {
        const auto& e = v2_v[v2];
        fprintf(stream, "[%02zX/%02zX/%02zX] %s %s %s\n", a, v1, v2, e.object_list_basename.c_str(), e.enemy_list_basename.c_str(), e.event_list_basename.c_str());
      }
    }
  }
}

struct AreaMapFileInfo {
  const char* name_token;
  vector<uint32_t> variation1_values;
  vector<uint32_t> variation2_values;

  AreaMapFileInfo(
      const char* name_token,
      vector<uint32_t> variation1_values,
      vector<uint32_t> variation2_values)
      : name_token(name_token),
        variation1_values(variation1_values),
        variation2_values(variation2_values) {}
};

static const vector<AreaMapFileInfo> map_file_info_dc_nte = {
    {"city00", {}, {0}},
    {"forest01", {}, {0, 1, 2, 3, 4}},
    {"forest02", {}, {0, 1, 2, 3, 4}},
    {"cave01", {0, 1, 2}, {0, 1}},
    {"cave02", {0, 1, 2}, {0, 1}},
    {"cave03", {0, 1, 3}, {0, 1}},
    {"machine01", {0, 1}, {0, 1}},
    {"machine02", {0, 1}, {0, 1}},
    {"ancient01", {0, 1}, {0, 1}},
    {"ancient02", {0, 1}, {0, 1}},
    {"ancient03", {0, 1}, {0, 1}},
    {"boss01", {}, {}},
    {"boss02", {}, {}},
    {"boss03", {}, {}},
    {"boss04", {}, {}},
    {nullptr, {}, {}},
};

// These are indexed as [episode][is_solo][floor], where episode is 0-2
static const vector<vector<vector<AreaMapFileInfo>>> map_file_info = {
    {
        // Episode 1
        {
            // Non-solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 1, 2, 3, 4}},
            {"forest02", {}, {0, 1, 2, 3, 4}},
            {"cave01", {0, 1, 2}, {0, 1}},
            {"cave02", {0, 1, 2}, {0, 1}},
            {"cave03", {0, 1, 2}, {0, 1}},
            {"machine01", {0, 1, 2}, {0, 1}},
            {"machine02", {0, 1, 2}, {0, 1}},
            {"ancient01", {0, 1, 2}, {0, 1}},
            {"ancient02", {0, 1, 2}, {0, 1}},
            {"ancient03", {0, 1, 2}, {0, 1}},
            {"boss01", {}, {}},
            {"boss02", {}, {}},
            {"boss03", {}, {}},
            {"boss04", {}, {}},
            {nullptr, {}, {}},
        },
        {
            // Solo
            {"city00", {}, {0}},
            {"forest01", {}, {0, 2, 4}},
            {"forest02", {}, {0, 3, 4}},
            {"cave01", {0, 1, 2}, {0}},
            {"cave02", {0, 1, 2}, {0}},
            {"cave03", {0, 1, 2}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
    {
        // Episode 2
        {
            // Non-solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0, 1}},
            {"seabed02", {0, 1}, {0, 1}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
        {
            // Solo
            {"labo00", {}, {0}},
            {"ruins01", {0, 1}, {0}},
            {"ruins02", {0, 1}, {0}},
            {"space01", {0, 1}, {0}},
            {"space02", {0, 1}, {0}},
            {"jungle01", {}, {0, 1, 2}},
            {"jungle02", {}, {0, 1, 2}},
            {"jungle03", {}, {0, 1, 2}},
            {"jungle04", {0, 1}, {0, 1}},
            {"jungle05", {}, {0, 1, 2}},
            {"seabed01", {0, 1}, {0}},
            {"seabed02", {0, 1}, {0}},
            {"boss05", {}, {}},
            {"boss06", {}, {}},
            {"boss07", {}, {}},
            {"boss08", {}, {}},
        },
    },
    {
        // Episode 4
        {
            // Non-solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
        {
            // Solo
            {"city02", {0}, {0}},
            {"wilds01", {0}, {0, 1, 2}},
            {"wilds01", {1}, {0, 1, 2}},
            {"wilds01", {2}, {0, 1, 2}},
            {"wilds01", {3}, {0, 1, 2}},
            {"crater01", {0}, {0, 1, 2}},
            {"desert01", {0, 1, 2}, {0}},
            {"desert02", {0}, {0, 1, 2}},
            {"desert03", {0, 1, 2}, {0}},
            {"boss09", {0}, {0}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
            {nullptr, {}, {}},
        },
    },
};

const AreaMapFileInfo& file_info_for_variation(
    Version version, Episode episode, uint8_t area, bool is_solo) {
  const vector<AreaMapFileInfo>* multi_index = nullptr;
  const vector<AreaMapFileInfo>* solo_index = nullptr;
  if (version == Version::DC_NTE) {
    multi_index = &map_file_info_dc_nte;
  } else {
    switch (episode) {
      case Episode::EP1:
        multi_index = &map_file_info.at(0).at(0);
        solo_index = &map_file_info.at(0).at(1);
        break;
      case Episode::EP2:
        multi_index = &map_file_info.at(1).at(0);
        solo_index = &map_file_info.at(1).at(1);
        break;
      case Episode::EP3: {
        static const AreaMapFileInfo blank_info = {nullptr, {}, {}};
        return blank_info;
      }
      case Episode::EP4:
        multi_index = &map_file_info.at(2).at(0);
        solo_index = &map_file_info.at(2).at(1);
        break;
      default:
        throw invalid_argument("episode has no maps");
    }
  }

  if (is_solo && solo_index) {
    const auto& ret = solo_index->at(area);
    if (ret.name_token) {
      return ret;
    }
  }
  return multi_index->at(area);
}

void generate_variations(
    parray<le_uint32_t, 0x20>& variations,
    shared_ptr<PSOLFGEncryption> random_crypt,
    Version version,
    Episode episode,
    bool is_solo) {
  for (size_t z = 0; z < 0x10; z++) {
    const auto& a = file_info_for_variation(version, episode, z, is_solo);
    if (!a.name_token) {
      variations[z * 2 + 0] = 0;
      variations[z * 2 + 1] = 0;
    } else {
      variations[z * 2 + 0] = (a.variation1_values.size() < 2) ? 0 : (random_crypt->next() % a.variation1_values.size());
      variations[z * 2 + 1] = (a.variation2_values.size() < 2) ? 0 : (random_crypt->next() % a.variation2_values.size());
    }
  }
}

vector<string> map_filenames_for_variation(
    Version version,
    Episode episode,
    GameMode mode,
    uint8_t floor,
    uint32_t var1,
    uint32_t var2,
    bool is_enemies) {
  // Map filenames are like map_<name_token>_<VV>_<VV>(_off)?(e|o)(_s|_c1)?.dat
  //   name_token comes from AreaMapFileInfo
  //   _VV are the values from the variation<1|2>_values vector (in contrast to
  //     the values sent in the 64 command, which are INDEXES INTO THAT VECTOR)
  //   _off or _s are used for solo mode (try both - city uses _s whereas levels
  //     use _off apparently)
  //   _c1 is used for the city map in Challenge mode (which we don't load,
  //     since it contains only NPCs and not enemies)
  //   e|o specifies what kind of data: e = enemies, o = objects
  const auto& a = file_info_for_variation(version, episode, floor, mode == GameMode::SOLO);
  if (!a.name_token) {
    return vector<string>();
  }

  string filename = "map_";
  filename += a.name_token;
  if (!a.variation1_values.empty()) {
    filename += string_printf("_%02" PRIX32, a.variation1_values.at(var1));
  }
  if (!a.variation2_values.empty()) {
    filename += string_printf("_%02" PRIX32, a.variation2_values.at(var2));
  }

  vector<string> ret;
  if (is_enemies) {
    if (mode == GameMode::SOLO) {
      ret.emplace_back(filename + "_offe.dat");
      ret.emplace_back(filename + "e_s.dat");
    } else if (floor == 0) {
      if (mode == GameMode::BATTLE) {
        ret.emplace_back(filename + "e_d.dat");
      } else if (mode == GameMode::CHALLENGE) {
        ret.emplace_back(filename + "e_c1.dat");
      }
    }
    ret.emplace_back(filename + "e.dat");
  } else {
    if (mode == GameMode::SOLO) {
      ret.emplace_back(filename + "_offo.dat");
      ret.emplace_back(filename + "o_s.dat");
    } else if (floor == 0) {
      if (mode == GameMode::BATTLE) {
        ret.emplace_back(filename + "o_d.dat");
      } else if (mode == GameMode::CHALLENGE) {
        ret.emplace_back(filename + "o_c1.dat");
      }
    }
    ret.emplace_back(filename + "o.dat");
  }
  return ret;
}

const shared_ptr<const Map::RareEnemyRates> Map::NO_RARE_ENEMIES = make_shared<Map::RareEnemyRates>(0, 0);
const shared_ptr<const Map::RareEnemyRates> Map::DEFAULT_RARE_ENEMIES = make_shared<Map::RareEnemyRates>(0x0083126E, 0x1999999A);
