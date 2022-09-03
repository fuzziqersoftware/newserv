#pragma once

#include <stdint.h>

#include <memory>
#include <string>
#include <random>



class RareItemSet {
public:
  struct Table {
    // TODO: It looks like this structure can actually vary. We see the offsets
    // 0194 and 01B2 in the unused section, along with the value 1E (number of
    // box rares). In PSOGC, these all appear to be the same size/format, but
    // that's probably not strictly required to be the case.
    // 0x280 in size; describes one difficulty, section ID, and episode
    struct Drop {
      uint8_t probability;
      uint8_t item_code[3];
    } __attribute__((packed));
    Drop monster_rares[0x65];     // 0000 - 0194 in file
    uint8_t box_areas[0x1E];      // 0194 - 01B2 in file
    Drop box_rares[0x1E]; // 01B2 - 022A in file
    uint8_t unused[0x56];
  } __attribute__((packed));

  RareItemSet(std::shared_ptr<const std::string> data);

  const Table& get_table(uint8_t episode, uint8_t difficulty, uint8_t secid) const;

  static bool sample(std::mt19937& rand, uint8_t probability);

private:
  std::shared_ptr<const std::string> data;
  const Table* tables;
};
