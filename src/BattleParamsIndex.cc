#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

void BattleParamsIndex::Table::print(FILE* stream) const {
  auto print_entry = +[](FILE* stream, const PlayerStats& e) {
    fprintf(stream,
        "%5hu %5hu %5hu %5hu %5hu %5hu %5hu %5hu %5" PRIu32 " %5" PRIu32,
        e.char_stats.atp.load(),
        e.char_stats.mst.load(),
        e.char_stats.evp.load(),
        e.char_stats.hp.load(),
        e.char_stats.dfp.load(),
        e.char_stats.ata.load(),
        e.char_stats.lck.load(),
        e.esp.load(),
        e.experience.load(),
        e.meseta.load());
  };

  for (size_t diff = 0; diff < 4; diff++) {
    fprintf(stream, "%c ZZ   ATP   PSV   EVP    HP   DFP   ATA   LCK   ESP   EXP  DIFF\n",
        abbreviation_for_difficulty(diff));
    for (size_t z = 0; z < 0x60; z++) {
      fprintf(stream, "  %02zX ", z);
      print_entry(stream, this->stats[diff][z]);
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
        throw runtime_error(phosg::string_printf(
            "battle params table size is incorrect (expected %zX bytes, have %zX bytes; is_solo=%hhu, episode=%hhu)",
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
