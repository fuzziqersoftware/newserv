#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

string BattleParamsIndex::Entry::str() const {
  string a1str = format_data_string(this->unknown_a1.data(), this->unknown_a1.bytes());
  return string_printf(
      "BattleParamsEntry[ATP=%hu PSV=%hu EVP=%hu HP=%hu DFP=%hu ATA=%hu LCK=%hu ESP=%hu a1=%s EXP=%" PRIu32 " diff=%" PRIu32 "]",
      this->atp.load(),
      this->psv.load(),
      this->evp.load(),
      this->hp.load(),
      this->dfp.load(),
      this->ata.load(),
      this->lck.load(),
      this->esp.load(),
      a1str.c_str(),
      this->experience.load(),
      this->difficulty.load());
}

void BattleParamsIndex::Table::print(FILE* stream) const {
  auto print_entry = +[](FILE* stream, const Entry& e) {
    string a1str = format_data_string(e.unknown_a1.data(), e.unknown_a1.bytes());
    fprintf(stream,
        "%5hu %5hu %5hu %5hu %5hu %5hu %5hu %5hu %s %5" PRIu32 " %5" PRIu32,
        e.atp.load(),
        e.psv.load(),
        e.evp.load(),
        e.hp.load(),
        e.dfp.load(),
        e.ata.load(),
        e.lck.load(),
        e.esp.load(),
        a1str.c_str(),
        e.experience.load(),
        e.difficulty.load());
  };

  for (size_t diff = 0; diff < 4; diff++) {
    fprintf(stream, "%c ZZ   ATP   PSV   EVP    HP   DFP   ATA   LCK   ESP                       A1   EXP  DIFF\n",
        abbreviation_for_difficulty(diff));
    for (size_t z = 0; z < 0x60; z++) {
      fprintf(stream, "  %02zX ", z);
      print_entry(stream, this->difficulty[diff][z]);
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
        throw runtime_error(string_printf(
            "battle params table size is incorrect (expected %zX bytes, have %zX bytes; is_solo=%hhu, episode=%hhu)",
            sizeof(Table), file.data->size(), is_solo, episode));
      }
      file.table = reinterpret_cast<const Table*>(file.data->data());
    }
  }
}

const BattleParamsIndex::Entry& BattleParamsIndex::get(
    bool solo, Episode episode, uint8_t difficulty, EnemyType type) const {
  return this->get(solo, episode, difficulty, battle_param_index_for_enemy_type(episode, type));
}

const BattleParamsIndex::Entry& BattleParamsIndex::get(
    bool solo, Episode episode, uint8_t difficulty, size_t index) const {
  if (difficulty > 4) {
    throw invalid_argument("incorrect difficulty");
  }
  if (index >= 0x60) {
    throw invalid_argument("incorrect monster type");
  }

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

  return this->files[!!solo][ep_index].table->difficulty[difficulty][index];
}
