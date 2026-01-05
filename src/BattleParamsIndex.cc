#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

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
          e.char_stats.lck, e.esp, e.experience, e.meseta, names_str);
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
    phosg::fwrite_fmt(stream, "{} ZZ FPARAM-0 FPARAM-1 FPARAM-2 FPARAM-3 FPARAM-4 FPARAM-5 IPARAM-0 IPARAM-1 IPARAM-2 IPARAM-3 IPARAM-4 IPARAM-5\n",
        abbreviation_for_difficulty(difficulty));
    for (size_t z = 0; z < 0x60; z++) {
      const auto& e = this->movement_data[static_cast<size_t>(difficulty)][z];
      phosg::fwrite_fmt(stream,
          "  {:02X} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:8.3f} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}",
          z, e.fparam0, e.fparam1, e.fparam2, e.fparam3, e.fparam4, e.fparam5,
          e.iparam0, e.iparam1, e.iparam2, e.iparam3, e.iparam4, e.iparam5);
      fputc('\n', stream);
    }
  }
}

BattleParamsIndex::BattleParamsIndex(
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

const BattleParamsIndex::Table& BattleParamsIndex::get_table(bool solo, Episode episode) const {
  uint8_t ep_index;
  switch (episode) {
    case Episode::EP1:
      ep_index = 0;
      break;
    case Episode::EP2:
      ep_index = 1;
      break;
    case Episode::EP4:
      ep_index = 2;
      break;
    default:
      throw invalid_argument("invalid episode");
  }

  return *this->files[!!solo][ep_index].table;
}
