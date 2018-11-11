#include "RareItemSet.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>

using namespace std;



RareItemSet::RareItemSet(const char* filename, uint8_t episode,
    uint8_t difficulty, uint8_t secid) {
  scoped_fd fd(filename, O_RDONLY);
  size_t offset = (episode * 0x6400) + (difficulty * 0x1900) + (secid * 0x0280);
  preadx(fd, this, sizeof(*this), offset);
}

bool sample_rare_item(uint8_t pc) {
  int8_t shift = ((pc >> 3) & 0x1F) - 4;
  if (shift < 0) {
    shift = 0;
  }
  uint32_t rate = ((2 << shift) * ((pc & 7) + 7));
  return (random_object<uint32_t>() < rate);
}
