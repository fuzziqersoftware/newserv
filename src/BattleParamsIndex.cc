#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

void BattleParamsIndex::Table::print(FILE* stream, Episode episode) const {
  auto print_entry = [stream, episode](const PlayerStats& e, size_t z) {
    string names_str;
    for (auto type : enemy_types_for_battle_param_index(episode, z)) {
      if (!names_str.empty()) {
        names_str += ", ";
      }
      names_str += phosg::name_for_enum(type);
    }
    phosg::fwrite_fmt(stream,
        "{:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5} {:5}  {}",
        e.char_stats.atp,
        e.char_stats.mst,
        e.char_stats.evp,
        e.char_stats.hp,
        e.char_stats.dfp,
        e.char_stats.ata,
        e.char_stats.lck,
        e.esp,
        e.experience,
        e.meseta,
        names_str);
  };

  for (size_t diff = 0; diff < 4; diff++) {
    phosg::fwrite_fmt(stream, "{} ZZ   ATP   PSV   EVP    HP   DFP   ATA   LCK   ESP   EXP  DIFF  NAMES\n",
        abbreviation_for_difficulty(diff));
    for (size_t z = 0; z < 0x60; z++) {
      phosg::fwrite_fmt(stream, "  {:02X} ", z);
      print_entry(this->stats[diff][z], z);
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
