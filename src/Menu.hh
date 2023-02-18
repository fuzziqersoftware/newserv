#pragma once

#include <stdint.h>

#include <string>



// Note: These aren't enums because neither enum nor enum class does what we
// want. Specifically, we need GO_BACK to be valid in multiple enums (and enums
// aren't namespaced unless they're enum classes), so we can't use enums. But we
// also want to be able to use non-enum values in switch statements without
// casting values all over the place, so we can't use enum classes either.

namespace MenuID {
  constexpr uint32_t MAIN                 = 0x11000011;
  constexpr uint32_t INFORMATION          = 0x22000022;
  constexpr uint32_t LOBBY                = 0x33000033;
  constexpr uint32_t GAME                 = 0x44000044;
  constexpr uint32_t QUEST                = 0x55000055;
  constexpr uint32_t QUEST_FILTER         = 0x66000066;
  constexpr uint32_t PROXY_DESTINATIONS   = 0x77000077;
  constexpr uint32_t PROGRAMS             = 0x88000088;
  constexpr uint32_t PATCHES              = 0x99000099;
  constexpr uint32_t PROXY_OPTIONS        = 0xAA0000AA;
  constexpr uint32_t TOURNAMENTS          = 0xBB0000BB;
  constexpr uint32_t TOURNAMENTS_FOR_SPEC = 0xBB1111BB;
  constexpr uint32_t TOURNAMENT_ENTRIES   = 0xCC0000CC;
}

namespace MainMenuItemID {
  constexpr uint32_t GO_TO_LOBBY        = 0x11222211;
  constexpr uint32_t INFORMATION        = 0x11333311;
  constexpr uint32_t DOWNLOAD_QUESTS    = 0x11444411;
  constexpr uint32_t PROXY_DESTINATIONS = 0x11555511;
  constexpr uint32_t PATCHES            = 0x11666611;
  constexpr uint32_t PROGRAMS           = 0x11777711;
  constexpr uint32_t DISCONNECT         = 0x11888811;
  constexpr uint32_t CLEAR_LICENSE      = 0x11999911;
}

namespace InformationMenuItemID {
  constexpr uint32_t GO_BACK = 0x22FFFF22;
}

namespace ProxyDestinationsMenuItemID {
  constexpr uint32_t GO_BACK = 0x77FFFF77;
  constexpr uint32_t OPTIONS = 0x77EEEE77;
}

namespace ProgramsMenuItemID {
  constexpr uint32_t GO_BACK = 0x88FFFF88;
}

namespace PatchesMenuItemID {
  constexpr uint32_t GO_BACK = 0x99FFFF99;
}

namespace ProxyOptionsMenuItemID {
  constexpr uint32_t GO_BACK =             0xAAFFFFAA;
  constexpr uint32_t CHAT_COMMANDS =       0xAA0000AA;
  constexpr uint32_t CHAT_FILTER =         0xAA1111AA;
  constexpr uint32_t INFINITE_HP =         0xAA2222AA;
  constexpr uint32_t INFINITE_TP =         0xAA3333AA;
  constexpr uint32_t SWITCH_ASSIST =       0xAA4444AA;
  constexpr uint32_t BLOCK_EVENTS =        0xAA5555AA;
  constexpr uint32_t BLOCK_PATCHES =       0xAA6666AA;
  constexpr uint32_t SAVE_FILES =          0xAA7777AA;
  constexpr uint32_t RED_NAME =            0xAA8888AA;
  constexpr uint32_t BLANK_NAME =          0xAA9999AA;
  constexpr uint32_t SUPPRESS_LOGIN =      0xAAAAAAAA;
  constexpr uint32_t SKIP_CARD =           0xAABBBBAA;
  constexpr uint32_t EP3_INFINITE_MESETA = 0xAACCCCAA;
}



struct MenuItem {
  enum Flag {
    // For menu items to be visible on DCNTE, they must not have either of the
    // following two flags. (The INVISIBLE_ON_GCNTE flag behaves similarly.)
    INVISIBLE_ON_DCNTE = 0x001,
    INVISIBLE_ON_DC = 0x002,
    INVISIBLE_ON_PC = 0x004,
    INVISIBLE_ON_GC_TRIAL_EDITION = 0x008,
    INVISIBLE_ON_GC = 0x010,
    INVISIBLE_ON_XB = 0x020,
    INVISIBLE_ON_BB = 0x040,
    DC_ONLY = INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    PC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_GC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    GC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    XB_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_BB,
    BB_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_XB,
    REQUIRES_MESSAGE_BOXES = 0x080,
    REQUIRES_SEND_FUNCTION_CALL = 0x100,
    REQUIRES_SAVE_DISABLED = 0x200,
  };

  uint32_t item_id;
  std::u16string name;
  std::u16string description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::u16string& name,
      const std::u16string& description, uint32_t flags);
};
