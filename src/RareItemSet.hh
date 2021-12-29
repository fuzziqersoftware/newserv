#pragma once

#include <stdint.h>



struct RareItemDrop {
  uint8_t probability;
  uint8_t item_code[3];
};

struct RareItemSet {
  RareItemDrop rares[0x65];     // 0000 - 0194 in file
  uint8_t box_areas[0x1E];      // 0194 - 01B2 in file
  RareItemDrop box_rares[0x1E]; // 01B2 - 022A in file
  uint8_t unused[0x56];

  RareItemSet(const char* filename, uint8_t episode, uint8_t difficulty,
  	  uint8_t secid);
}; // 0x280 in size; describes one difficulty, section ID, and episode

bool sample_rare_item(uint8_t pc);
