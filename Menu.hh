#pragma once

#include <stdint.h>

#include <string>



enum MenuItemFlag {
  InvisibleOnDC = 0x01,
  InvisibleOnPC = 0x02,
  InvisibleOnGC = 0x04,
  InvisibleOnGCEpisode3 = 0x08,
  InvisibleOnBB = 0x10,
  RequiresMessageBoxes = 0x20,
};

struct MenuItem {
  uint32_t item_id;
  std::u16string name;
  std::u16string description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::u16string& name,
      const std::u16string& description, uint32_t flags);
};
