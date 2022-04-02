#pragma once

#include <stdint.h>

#include <string>



#define MAIN_MENU_ID                     0x11000011
#define INFORMATION_MENU_ID              0x22000022
#define LOBBY_MENU_ID                    0x33000033
#define GAME_MENU_ID                     0x44000044
#define QUEST_MENU_ID                    0x55000055
#define QUEST_FILTER_MENU_ID             0x66000066
#define PROXY_DESTINATIONS_MENU_ID       0x77000077

#define MAIN_MENU_GO_TO_LOBBY            0x11AAAA11
#define MAIN_MENU_INFORMATION            0x11BBBB11
#define MAIN_MENU_DOWNLOAD_QUESTS        0x11CCCC11
#define MAIN_MENU_PROXY_DESTINATIONS     0x11DDDD11
#define MAIN_MENU_DISCONNECT             0x11EEEE11
#define INFORMATION_MENU_GO_BACK         0x22FFFF22
#define PROXY_DESTINATIONS_MENU_GO_BACK  0x77FFFF77



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
  };

  uint32_t item_id;
  std::u16string name;
  std::u16string description;
  uint32_t flags;

  MenuItem(uint32_t item_id, const std::u16string& name,
      const std::u16string& description, uint32_t flags);
};
