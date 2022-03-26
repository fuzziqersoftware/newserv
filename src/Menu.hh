#pragma once

#include <stdint.h>

#include <string>



enum MenuItemFlag {
  INVISIBLE_ON_DC = 0x01,
  INVISIBLE_ON_PC = 0x02,
  INVISIBLE_ON_GC = 0x04,
  INVISIBLE_ON_GC_EPISODE_3 = 0x08,
  INVISIBLE_ON_BB = 0x10,
  REQUIRES_MESSAGE_BOXES = 0x20,
};

struct MenuItem {
  uint32_t item_id;
  std::u16string name;
  std::u16string description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::u16string& name,
      const std::u16string& description, uint32_t flags);
};
