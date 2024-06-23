#pragma once

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

// Note: These aren't enums because neither enum nor enum class does what we
// want. Specifically, we need GO_BACK to be valid in multiple enums (and enums
// aren't namespaced unless they're enum classes), so we can't use enums. But we
// also want to be able to use non-enum values in switch statements without
// casting values all over the place, so we can't use enum classes either.

namespace MenuID {
constexpr uint32_t MAIN = 0x11000011;
constexpr uint32_t CLEAR_LICENSE_CONFIRMATION = 0x11111111;
constexpr uint32_t INFORMATION = 0x22000022;
constexpr uint32_t LOBBY = 0x33000033;
constexpr uint32_t GAME = 0x44000044;
constexpr uint32_t QUEST_EP1 = 0x55010155;
constexpr uint32_t QUEST_EP2 = 0x55020255;
// See the decsription of the A2 command in CommandFormats.hh for why these
// menu IDs don't fit the rest of the pattern.
constexpr uint32_t QUEST_CATEGORIES_EP1 = 0x01000001;
constexpr uint32_t QUEST_CATEGORIES_EP2 = 0x02000002;
constexpr uint32_t PROXY_DESTINATIONS = 0x77000077;
constexpr uint32_t PROGRAMS = 0x88000088;
constexpr uint32_t PATCHES = 0x99000099;
constexpr uint32_t PATCH_SWITCHES = 0x99010199;
constexpr uint32_t PROXY_OPTIONS = 0xAA0000AA;
constexpr uint32_t TOURNAMENTS = 0xBB0000BB;
constexpr uint32_t TOURNAMENTS_FOR_SPEC = 0xBB1111BB;
constexpr uint32_t TOURNAMENT_ENTRIES = 0xCC0000CC;
} // namespace MenuID

namespace MainMenuItemID {
constexpr uint32_t GO_TO_LOBBY = 0x11222211;
constexpr uint32_t INFORMATION = 0x11333311;
constexpr uint32_t DOWNLOAD_QUESTS = 0x11444411;
constexpr uint32_t PROXY_DESTINATIONS = 0x11555511;
constexpr uint32_t PATCHES = 0x11666611;
constexpr uint32_t PATCH_SWITCHES = 0x11676711;
constexpr uint32_t PROGRAMS = 0x11777711;
constexpr uint32_t DISCONNECT = 0x11888811;
constexpr uint32_t CLEAR_LICENSE = 0x11999911;
} // namespace MainMenuItemID

namespace ClearLicenseConfirmationMenuItemID {
constexpr uint32_t CANCEL = 0x01010101;
constexpr uint32_t CLEAR_LICENSE = 0x02020202;
} // namespace ClearLicenseConfirmationMenuItemID

namespace InformationMenuItemID {
constexpr uint32_t GO_BACK = 0x22FFFF22;
}

namespace ProxyDestinationsMenuItemID {
constexpr uint32_t GO_BACK = 0x77FFFF77;
constexpr uint32_t OPTIONS = 0x77EEEE77;
} // namespace ProxyDestinationsMenuItemID

namespace ProgramsMenuItemID {
constexpr uint32_t GO_BACK = 0x88FFFF88;
}

namespace PatchesMenuItemID {
constexpr uint32_t GO_BACK = 0x99FFFF99;
}

namespace ProxyOptionsMenuItemID {
constexpr uint32_t GO_BACK = 0xAAFFFFAA;
constexpr uint32_t CHAT_COMMANDS = 0xAA0101AA;
constexpr uint32_t PLAYER_NOTIFICATIONS = 0xAA0202AA;
constexpr uint32_t DROP_NOTIFICATIONS = 0xAA0303AA;
constexpr uint32_t BLOCK_PINGS = 0xAA0404AA;
constexpr uint32_t INFINITE_HP = 0xAA0505AA;
constexpr uint32_t INFINITE_TP = 0xAA0606AA;
constexpr uint32_t SWITCH_ASSIST = 0xAA0707AA;
constexpr uint32_t BLOCK_EVENTS = 0xAA0808AA;
constexpr uint32_t BLOCK_PATCHES = 0xAA0909AA;
constexpr uint32_t SAVE_FILES = 0xAA0A0AAA;
constexpr uint32_t VIRTUAL_CLIENT = 0xAA0B0BAA;
constexpr uint32_t RED_NAME = 0xAA0C0CAA;
constexpr uint32_t BLANK_NAME = 0xAA0D0DAA;
constexpr uint32_t SUPPRESS_LOGIN = 0xAA0E0EAA;
constexpr uint32_t SKIP_CARD = 0xAA0F0FAA;
constexpr uint32_t EP3_INFINITE_MESETA = 0xAA1010AA;
constexpr uint32_t EP3_INFINITE_TIME = 0xAA1111AA;
constexpr uint32_t EP3_UNMASK_WHISPERS = 0xAA1212AA;
} // namespace ProxyOptionsMenuItemID

namespace TeamRewardMenuItemID {
constexpr uint32_t TEAM_FLAG = 0x01010101;
constexpr uint32_t DRESSING_ROOM = 0x02020202;
constexpr uint32_t MEMBERS_20_LEADERS_3 = 0x03030303;
constexpr uint32_t MEMBERS_40_LEADERS_5 = 0x04040404;
constexpr uint32_t MEMBERS_70_LEADERS_8 = 0x05050505;
constexpr uint32_t MEMBERS_100_LEADERS_10 = 0x06060606;
// constexpr uint32_t POINT_OF_DISASTER = ...;
// constexpr uint32_t TOYS_TWILIGHT = ...;
// constexpr uint32_t COMMANDER_BLADE = ...;
// constexpr uint32_t UNION_GUARD = ...;
// constexpr uint32_t TEAM_POINTS_500 = ...;
// constexpr uint32_t TEAM_POINTS_1000 = ...;
// constexpr uint32_t TEAM_POINTS_5000 = ...;
// constexpr uint32_t TEAM_POINTS_10000 = ...;
} // namespace TeamRewardMenuItemID

struct MenuItem {
  enum Flag {
    // For menu items to be visible on DC NTE, they must not have either of the
    // following two flags. (The INVISIBLE_ON_GC_NTE flag behaves similarly.)
    INVISIBLE_ON_DC_PROTOS = 0x001,
    INVISIBLE_ON_DC = 0x002,
    INVISIBLE_ON_PC_NTE = 0x004,
    INVISIBLE_ON_PC = 0x008,
    INVISIBLE_ON_GC_NTE = 0x010,
    INVISIBLE_ON_GC = 0x020,
    INVISIBLE_ON_XB = 0x040,
    INVISIBLE_ON_BB = 0x080,
    DC_ONLY = INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    PC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_GC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    GC_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_XB | INVISIBLE_ON_BB,
    XB_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_BB,
    BB_ONLY = INVISIBLE_ON_DC | INVISIBLE_ON_PC | INVISIBLE_ON_GC | INVISIBLE_ON_XB,
    REQUIRES_MESSAGE_BOXES = 0x100,
    REQUIRES_SEND_FUNCTION_CALL = 0x200,
    REQUIRES_SAVE_DISABLED = 0x400,
    INVISIBLE_IN_INFO_MENU = 0x800,
  };

  uint32_t item_id;
  std::string name;
  std::string description;
  std::function<std::string()> get_description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::string& name, const std::string& description, uint32_t flags);
  MenuItem(uint32_t item_id, const std::string& name, std::function<std::string()> get_description, uint32_t flags);
};

struct Menu {
  uint32_t menu_id;
  std::string name;
  std::vector<MenuItem> items;

  Menu() = delete;
  Menu(uint32_t menu_id, const std::string& name);
};
