#include "RareItemSet.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>

#include "StaticGameData.hh"

using namespace std;

uint32_t RareItemSet::expand_rate(uint8_t pc) {
  int8_t shift = ((pc >> 3) & 0x1F) - 4;
  if (shift < 0) {
    shift = 0;
  }
  return ((2 << shift) * ((pc & 7) + 7));
}

bool RareItemSet::sample(mt19937& random, uint8_t pc) {
  return (random() < RareItemSet::expand_rate(pc));
}

GSLRareItemSet::GSLRareItemSet(shared_ptr<const string> data, bool is_big_endian)
    : gsl(data, is_big_endian) {}

const GSLRareItemSet::Table& GSLRareItemSet::get_table(
    Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const {
  if (difficulty > 3) {
    throw logic_error("incorrect difficulty");
  }
  if (secid > 10) {
    throw logic_error("incorrect section id");
  }

  if ((episode != Episode::EP1) && (episode != Episode::EP2)) {
    throw runtime_error("invalid episode");
  }

  string filename = string_printf("ItemRT%s%s%c%1d.rel",
                                  ((mode == GameMode::CHALLENGE) ? "c" : ""),
                                  ((episode == Episode::EP2) ? "l" : ""),
                                  tolower(abbreviation_for_difficulty(difficulty)), // One of "nhvu"
                                  secid);
  auto entry = this->gsl.get(filename);
  if (entry.second < sizeof(Table)) {
    throw runtime_error(string_printf("table %s is too small", filename.c_str()));
  }
  return *reinterpret_cast<const Table*>(entry.first);
}

RELRareItemSet::RELRareItemSet(shared_ptr<const string> data) : data(data) {
  if (this->data->size() != sizeof(Table) * 10 * 4 * 3) {
    throw runtime_error("data file size is incorrect");
  }
}

const RELRareItemSet::Table& RELRareItemSet::get_table(
    Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const {
  (void)mode; // TODO: Shouldn't we check for challenge mode somewhere?

  if (difficulty > 3) {
    throw logic_error("incorrect difficulty");
  }
  if (secid > 10) {
    throw logic_error("incorrect section id");
  }

  size_t ep_index;
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
      throw invalid_argument("incorrect episode");
  }

  const auto* tables = reinterpret_cast<const Table*>(this->data->data());
  return tables[(ep_index * 10 * 4) + (difficulty * 10) + secid];
}
