#pragma once

#include <stdint.h>

#include <string>



// Note: These aren't enums because neither enum nor enum class does what we
// want. Specifically, we need GO_BACK to be valid in multiple enums (and enums
// aren't namespaced unless they're enum classes), so we can't use enums. But we
// also want to be able to use non-enum values in switch statements without
// casting values all over the place, so we can't use enum classes either.

namespace MenuID {
  constexpr uint32_t MAIN               = 0x11000011;
  constexpr uint32_t INFORMATION        = 0x22000022;
  constexpr uint32_t LOBBY              = 0x33000033;
  constexpr uint32_t GAME               = 0x44000044;
  constexpr uint32_t QUEST              = 0x55000055;
  constexpr uint32_t QUEST_FILTER       = 0x66000066;
  constexpr uint32_t PROXY_DESTINATIONS = 0x77000077;
  constexpr uint32_t PROGRAMS           = 0x88000088;
  constexpr uint32_t PATCHES            = 0x99000099;
}

namespace MainMenuItemID {
  constexpr uint32_t GO_TO_LOBBY        = 0x11222211;
  constexpr uint32_t INFORMATION        = 0x11333311;
  constexpr uint32_t DOWNLOAD_QUESTS    = 0x11444411;
  constexpr uint32_t PROXY_DESTINATIONS = 0x11555511;
  constexpr uint32_t PATCHES            = 0x11666611;
  constexpr uint32_t PROGRAMS           = 0x11777711;
  constexpr uint32_t DISCONNECT         = 0x11888811;
}

namespace InformationMenuItemID {
  constexpr uint32_t GO_BACK = 0x22FFFF22;
};

namespace ProxyDestinationsMenuItemID {
  constexpr uint32_t GO_BACK = 0x77FFFF77;
};

namespace ProgramsMenuItemID {
  constexpr uint32_t GO_BACK = 0x88FFFF88;
};

namespace PatchesMenuItemID {
  constexpr uint32_t GO_BACK = 0x99FFFF99;
};



struct MenuItem {
  enum Flag {
    INVISIBLE_ON_DC = 0x01,
    INVISIBLE_ON_PC = 0x02,
    INVISIBLE_ON_GC = 0x04,
    INVISIBLE_ON_BB = 0x08,
    DC_ONLY = INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_BB,
    PC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_GC | INVISIBLE_ON_BB,
    GC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_BB,
    BB_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_GC,
    REQUIRES_MESSAGE_BOXES = 0x10,
    REQUIRES_SEND_FUNCTION_CALL = 0x20,
    REQUIRES_SAVE_DISABLED = 0x40,
  };

  uint32_t item_id;
  std::u16string name;
  std::u16string description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::u16string& name,
      const std::u16string& description, uint32_t flags);
};
