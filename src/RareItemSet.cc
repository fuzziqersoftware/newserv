#include "RareItemSet.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>

using namespace std;



RareItemSet::RareItemSet(shared_ptr<const string> data) : data(data) {
  if (this->data->size() != sizeof(Table) * 10 * 4 * 3) {
    throw runtime_error("data file size is incorrect");
  }
  this->tables = reinterpret_cast<const Table*>(this->data->data());
}

const RareItemSet::Table& RareItemSet::get_table(
    uint8_t episode, uint8_t difficulty, uint8_t secid) const {
  if (episode > 2) {
    throw logic_error("incorrect episode number");
  }
  if (difficulty > 3) {
    throw logic_error("incorrect difficulty");
  }
  if (secid > 10) {
    throw logic_error("incorrect section id");
  }
  return this->tables[(episode * 10 * 4) + (difficulty * 10) + secid];
}

bool RareItemSet::sample(mt19937& random, uint8_t pc) {
  int8_t shift = ((pc >> 3) & 0x1F) - 4;
  if (shift < 0) {
    shift = 0;
  }
  uint32_t rate = ((2 << shift) * ((pc & 7) + 7));
  return (random() < rate);
}
