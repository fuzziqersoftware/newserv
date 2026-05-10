#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

BattleParamsIndex::AttackData BattleParamsIndex::AttackData::from_json(const phosg::JSON& json) {
  return AttackData{
      json.get_int("MinATP"),
      json.get_int("MaxATP"),
      json.get_int("MinATA"),
      json.get_int("MaxATA"),
      json.get_float("DistanceX"),
      json.get_int("Angle"),
      json.get_float("DistanceY"),
      json.get_int("UnknownA8"),
      json.get_int("UnknownA9"),
      json.get_int("UnknownA10"),
      json.get_int("UnknownA11"),
      json.get_int("UnknownA12"),
      json.get_int("UnknownA13"),
      json.get_int("UnknownA14"),
      json.get_int("UnknownA15"),
      json.get_int("UnknownA16"),
  };
}
phosg::JSON BattleParamsIndex::AttackData::json() const {
  return phosg::JSON::dict({
      {"MinATP", this->min_atp.load()},
      {"MaxATP", this->max_atp.load()},
      {"MinATA", this->min_ata.load()},
      {"MaxATA", this->max_ata.load()},
      {"DistanceX", this->distance_x.load()},
      {"Angle", this->angle.load()},
      {"DistanceY", this->distance_y.load()},
      {"UnknownA8", this->unknown_a8.load()},
      {"UnknownA9", this->unknown_a9.load()},
      {"UnknownA10", this->unknown_a10.load()},
      {"UnknownA11", this->unknown_a11.load()},
      {"UnknownA12", this->unknown_a12.load()},
      {"UnknownA13", this->unknown_a13.load()},
      {"UnknownA14", this->unknown_a14.load()},
      {"UnknownA15", this->unknown_a15.load()},
      {"UnknownA16", this->unknown_a16.load()},
  });
}

BattleParamsIndex::ResistData BattleParamsIndex::ResistData::from_json(const phosg::JSON& json) {
  return BattleParamsIndex::ResistData{
      json.get_int("EVPBonus"),
      json.get_int("EFR"),
      json.get_int("EIC"),
      json.get_int("ETH"),
      json.get_int("ELT"),
      json.get_int("EDK"),
      json.get_int("UnknownA6"),
      json.get_int("UnknownA7"),
      json.get_int("UnknownA8"),
      json.get_int("UnknownA9"),
      json.get_int("DFPBonus"),
  };
}
phosg::JSON BattleParamsIndex::ResistData::json() const {
  return phosg::JSON::dict({
      {"EVPBonus", this->evp_bonus.load()},
      {"EFR", this->efr.load()},
      {"EIC", this->eic.load()},
      {"ETH", this->eth.load()},
      {"ELT", this->elt.load()},
      {"EDK", this->edk.load()},
      {"UnknownA6", this->unknown_a6.load()},
      {"UnknownA7", this->unknown_a7.load()},
      {"UnknownA8", this->unknown_a8.load()},
      {"UnknownA9", this->unknown_a9.load()},
      {"DFPBonus", this->dfp_bonus.load()},
  });
}

BattleParamsIndex::MovementData BattleParamsIndex::MovementData::from_json(const phosg::JSON& json) {
  const auto& fparams_json = json.at("FParams").as_list();
  const auto& iparams_json = json.at("IParams").as_list();
  return BattleParamsIndex::MovementData{
      fparams_json.at(0)->as_float(),
      fparams_json.at(1)->as_float(),
      fparams_json.at(2)->as_float(),
      fparams_json.at(3)->as_float(),
      fparams_json.at(4)->as_float(),
      fparams_json.at(5)->as_float(),
      iparams_json.at(0)->as_float(),
      iparams_json.at(1)->as_float(),
      iparams_json.at(2)->as_float(),
      iparams_json.at(3)->as_float(),
      iparams_json.at(4)->as_float(),
      iparams_json.at(5)->as_float(),
  };
}
phosg::JSON BattleParamsIndex::MovementData::json() const {
  auto fparams_list = phosg::JSON::list({
      this->fparam1.load(),
      this->fparam2.load(),
      this->fparam3.load(),
      this->fparam4.load(),
      this->fparam5.load(),
      this->fparam6.load(),
  });
  auto iparams_list = phosg::JSON::list({
      this->iparam1.load(),
      this->iparam2.load(),
      this->iparam3.load(),
      this->iparam4.load(),
      this->iparam5.load(),
      this->iparam6.load(),
  });
  return phosg::JSON::dict({{"FParams", std::move(fparams_list)}, {"IParams", std::move(iparams_list)}});
}

BattleParamsIndex::Table BattleParamsIndex::Table::from_json(const phosg::JSON& json) {
  BattleParamsIndex::Table ret;
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    const auto& diff_json = json.at(name_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& entry_json = diff_json.at(z);
      ret.stats[static_cast<size_t>(difficulty)][z] = PlayerStats::from_json(entry_json.at("Stats"));
      ret.attack_data[static_cast<size_t>(difficulty)][z] = AttackData::from_json(entry_json.at("AttackData"));
      ret.resist_data[static_cast<size_t>(difficulty)][z] = ResistData::from_json(entry_json.at("ResistData"));
      ret.movement_data[static_cast<size_t>(difficulty)][z] = MovementData::from_json(entry_json.at("MovementData"));
    }
  }
  return ret;
}

phosg::JSON BattleParamsIndex::Table::json() const {
  auto ret = phosg::JSON::dict();
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    auto diff_ret = phosg::JSON::list();
    for (size_t z = 0; z < 0x60; z++) {
      auto stats_json = this->stats_for_index(difficulty, z).json();
      auto attack_data_json = this->attack_data_for_index(difficulty, z).json();
      auto resist_data_json = this->resist_data_for_index(difficulty, z).json();
      auto movement_data_json = this->movement_data_for_index(difficulty, z).json();
      std::set<EnemyType> stats_names;
      std::set<EnemyType> attack_data_names;
      std::set<EnemyType> resist_data_names;
      std::set<EnemyType> movement_data_names;
      for (Episode episode : ALL_EPISODES_V4) {
        for (const auto& enemy_type : enemy_types_for_battle_param_stats_index(episode, z)) {
          stats_names.emplace(enemy_type);
        }
        for (const auto& enemy_type : enemy_types_for_battle_param_attack_data_index(episode, z)) {
          attack_data_names.emplace(enemy_type);
        }
        for (const auto& enemy_type : enemy_types_for_battle_param_resist_data_index(episode, z)) {
          resist_data_names.emplace(enemy_type);
        }
        for (const auto& enemy_type : enemy_types_for_battle_param_movement_data_index(episode, z)) {
          movement_data_names.emplace(enemy_type);
        }
      }
      auto stats_names_json = phosg::JSON::list();
      for (EnemyType enemy_type : stats_names) {
        stats_names_json.emplace_back(phosg::name_for_enum(enemy_type));
      }
      auto attack_data_names_json = phosg::JSON::list();
      for (EnemyType enemy_type : attack_data_names) {
        attack_data_names_json.emplace_back(phosg::name_for_enum(enemy_type));
      }
      auto resist_data_names_json = phosg::JSON::list();
      for (EnemyType enemy_type : resist_data_names) {
        resist_data_names_json.emplace_back(phosg::name_for_enum(enemy_type));
      }
      auto movement_data_names_json = phosg::JSON::list();
      for (EnemyType enemy_type : movement_data_names) {
        movement_data_names_json.emplace_back(phosg::name_for_enum(enemy_type));
      }
      stats_json.emplace("Enemies", std::move(stats_names_json));
      attack_data_json.emplace("Enemies", std::move(attack_data_names_json));
      resist_data_json.emplace("Enemies", std::move(resist_data_names_json));
      movement_data_json.emplace("Enemies", std::move(movement_data_names_json));
      diff_ret.emplace_back(phosg::JSON::dict({
          {"BPIndex", z},
          {"Stats", std::move(stats_json)},
          {"AttackData", std::move(attack_data_json)},
          {"ResistData", std::move(resist_data_json)},
          {"MovementData", std::move(movement_data_json)},
      }));
    }
    ret.emplace(name_for_difficulty(difficulty), std::move(diff_ret));
  }
  return ret;
}

void BattleParamsIndex::Table::print(FILE* stream, Episode episode) const {
  phosg::fwrite_fmt(stream, "========== STATS\n");
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    phosg::fwrite_fmt(stream, "{} ZZ   ATP   PSV   EVP    HP   DFP   ATA   LCK   ESP   EXP  DIFF  NAMES\n",
        abbreviation_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& e = this->stats[static_cast<size_t>(difficulty)][z];
      phosg::fwrite_fmt(stream, "  {:02X} ", z);
      string names_str;
      for (auto type : enemy_types_for_battle_param_stats_index(episode, z)) {
        if (!names_str.empty()) {
          names_str += ", ";
        }
        names_str += phosg::name_for_enum(type);
      }
      phosg::fwrite_fmt(stream,
          "{:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5}  {}",
          e.char_stats.atp, e.char_stats.mst, e.char_stats.evp, e.char_stats.hp, e.char_stats.dfp, e.char_stats.ata,
          e.char_stats.lck, e.esp, e.exp, e.meseta, names_str);
      fputc('\n', stream);
    }
  }

  phosg::fwrite_fmt(stream, "========== ATTACK DATA\n");
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    phosg::fwrite_fmt(stream, "{} ZZ ATP- ATP+ ATA- ATA+ -DIST-X- -ANGLE-- -DIST-Y- -A8- -A9- A10- A11- --A12--- --A13--- --A14--- --A15--- --A16---\n",
        abbreviation_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& e = this->attack_data[static_cast<size_t>(difficulty)][z];
      phosg::fwrite_fmt(stream,
          "  {:02X} {:04X} {:04X} {:04X} {:04X} {:8.3f} {:08X} {:8.3f} {:04X} {:04X} {:04X} {:04X} {:08X} {:08X} {:08X} {:08X} {:08X}",
          z, e.min_atp, e.max_atp, e.min_ata, e.max_ata, e.distance_x, e.angle, e.distance_y, e.unknown_a8,
          e.unknown_a9, e.unknown_a10, e.unknown_a11, e.unknown_a12, e.unknown_a13, e.unknown_a14, e.unknown_a15,
          e.unknown_a16);
      fputc('\n', stream);
    }
  }

  phosg::fwrite_fmt(stream, "========== RESIST DATA\n");
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    phosg::fwrite_fmt(stream, "{} ZZ EVP- EFR- EIC- ETH- ELT- EDK- ---A6--- ---A7--- ---A8--- ---A9--- --DFP---\n",
        abbreviation_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& e = this->resist_data[static_cast<size_t>(difficulty)][z];
      phosg::fwrite_fmt(stream,
          "  {:02X} {:04X} {:04X} {:04X} {:04X} {:04X} {:04X} {:08X} {:08X} {:08X} {:08X} {:08X}",
          z, e.evp_bonus, e.efr, e.eic, e.eth, e.elt, e.edk, e.unknown_a6, e.unknown_a7, e.unknown_a8, e.unknown_a9,
          e.dfp_bonus);
      fputc('\n', stream);
    }
  }

  phosg::fwrite_fmt(stream, "========== MOVEMENT DATA\n");
  for (Difficulty difficulty : ALL_DIFFICULTIES_V234) {
    phosg::fwrite_fmt(stream, "{} ZZ FPARAM-1 FPARAM-2 FPARAM-3 FPARAM-4 FPARAM-5 FPARAM-6 IPARAM-1 IPARAM-2 IPARAM-3 IPARAM-4 IPARAM-5 IPARAM-6\n",
        abbreviation_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& e = this->movement_data[static_cast<size_t>(difficulty)][z];
      phosg::fwrite_fmt(stream,
          "  {:02X} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}",
          z, e.fparam1, e.fparam2, e.fparam3, e.fparam4, e.fparam5, e.fparam6,
          e.iparam1, e.iparam2, e.iparam3, e.iparam4, e.iparam5, e.iparam6);
      fputc('\n', stream);
    }
  }
}

phosg::JSON BattleParamsIndex::json() const {
  return phosg::JSON::dict({
      {"Episode1-Online", this->get_table(false, Episode::EP1).json()},
      {"Episode2-Online", this->get_table(false, Episode::EP2).json()},
      {"Episode4-Online", this->get_table(false, Episode::EP4).json()},
      {"Episode1-Solo", this->get_table(true, Episode::EP1).json()},
      {"Episode2-Solo", this->get_table(true, Episode::EP2).json()},
      {"Episode4-Solo", this->get_table(true, Episode::EP4).json()},
  });
}

JSONBattleParamsIndex::JSONBattleParamsIndex(const phosg::JSON& json) {
  this->tables[0][0] = Table::from_json(json.at("Episode1-Online"));
  this->tables[0][1] = Table::from_json(json.at("Episode2-Online"));
  this->tables[0][2] = Table::from_json(json.at("Episode4-Online"));
  this->tables[1][0] = Table::from_json(json.at("Episode1-Solo"));
  this->tables[1][1] = Table::from_json(json.at("Episode2-Solo"));
  this->tables[1][2] = Table::from_json(json.at("Episode4-Solo"));
}

const BattleParamsIndex::Table& JSONBattleParamsIndex::get_table(bool solo, Episode episode) const {
  switch (episode) {
    case Episode::EP1:
      return this->tables[!!solo][0];
    case Episode::EP2:
      return this->tables[!!solo][1];
    case Episode::EP4:
      return this->tables[!!solo][2];
    default:
      throw invalid_argument("invalid episode");
  }
}

BinaryBattleParamsIndex::BinaryBattleParamsIndex(
    shared_ptr<const string> data_on_ep1,
    shared_ptr<const string> data_on_ep2,
    shared_ptr<const string> data_on_ep4,
    shared_ptr<const string> data_off_ep1,
    shared_ptr<const string> data_off_ep2,
    shared_ptr<const string> data_off_ep4) {
  this->files[0][0].data = data_on_ep1;
  this->files[0][1].data = data_on_ep2;
  this->files[0][2].data = data_on_ep4;
  this->files[1][0].data = data_off_ep1;
  this->files[1][1].data = data_off_ep2;
  this->files[1][2].data = data_off_ep4;

  for (uint8_t is_solo = 0; is_solo < 2; is_solo++) {
    for (uint8_t episode = 0; episode < 3; episode++) {
      auto& file = this->files[is_solo][episode];
      if (file.data->size() < sizeof(Table)) {
        throw runtime_error(std::format(
            "battle params table size is incorrect (expected {:X} bytes, have {:X} bytes; is_solo={}, episode={})",
            sizeof(Table), file.data->size(), is_solo, episode));
      }
      file.table = reinterpret_cast<const Table*>(file.data->data());
    }
  }
}

const BattleParamsIndex::Table& BinaryBattleParamsIndex::get_table(bool solo, Episode episode) const {
  switch (episode) {
    case Episode::EP1:
      return *this->files[!!solo][0].table;
    case Episode::EP2:
      return *this->files[!!solo][1].table;
    case Episode::EP4:
      return *this->files[!!solo][2].table;
    default:
      throw invalid_argument("invalid episode");
  }
}
