#include "ChoiceSearch.hh"

#include <inttypes.h>
#include <string.h>

#include "Client.hh"

using namespace std;

const vector<ChoiceSearchCategory> CHOICE_SEARCH_CATEGORIES({
    ChoiceSearchCategory{
        .id = 0x0001,
        .name = "Level",
        .choices = {
            {0x0000, "Any"},
            {0x0001, "Own level +/- 5"},
            {0x0002, "Level 1-10"},
            {0x0003, "Level 11-20"},
            {0x0004, "Level 21-40"},
            {0x0005, "Level 41-60"},
            {0x0006, "Level 61-80"},
            {0x0007, "Level 81-100"},
            {0x0008, "Level 101-120"},
            {0x0009, "Level 121-160"},
            {0x000A, "Level 161-200"},
        },
        .client_matches = +[](shared_ptr<Client> searcher_c, shared_ptr<Client> target_c, uint16_t choice_id) -> bool {
          if (choice_id == 0x0000) {
            return true;
          }
          uint32_t target_level = target_c->character()->disp.stats.level + 1;
          switch (choice_id) {
            case 0x0001:
              return (labs(static_cast<int32_t>(target_level - searcher_c->character()->disp.stats.level)) <= 5);
            case 0x0002:
              return (target_level <= 10);
            case 0x0003:
              return (target_level > 10) && (target_level <= 20);
            case 0x0004:
              return (target_level > 20) && (target_level <= 40);
            case 0x0005:
              return (target_level > 40) && (target_level <= 60);
            case 0x0006:
              return (target_level > 60) && (target_level <= 80);
            case 0x0007:
              return (target_level > 80) && (target_level <= 100);
            case 0x0008:
              return (target_level > 100) && (target_level <= 120);
            case 0x0009:
              return (target_level > 120) && (target_level <= 160);
            case 0x000A:
              return (target_level > 160) && (target_level <= 200);
          }
          return false;
        },
    },
    ChoiceSearchCategory{
        .id = 0x0002,
        .name = "Class",
        .choices = {
            {0x0000, "Any"},
            {0x0010, "Hunter"},
            {0x0001, "HUmar"},
            {0x0002, "HUnewearl"},
            {0x0003, "HUcast"},
            {0x000A, "HUcaseal"},
            {0x0011, "Ranger"},
            {0x0004, "RAmar"},
            {0x000C, "RAmarl"},
            {0x0005, "RAcast"},
            {0x0006, "RAcaseal"},
            {0x0012, "Force"},
            {0x000B, "FOmar"},
            {0x0007, "FOmarl"},
            {0x0008, "FOnewm"},
            {0x0009, "FOnewearl"},
        },
        .client_matches = +[](shared_ptr<Client>, shared_ptr<Client> target_c, uint16_t choice_id) -> bool {
          switch (choice_id) {
            case 0x0000:
              return true;
            case 0x0010:
              return target_c->character()->disp.visual.class_flags & 0x20;
            case 0x0011:
              return target_c->character()->disp.visual.class_flags & 0x40;
            case 0x0012:
              return target_c->character()->disp.visual.class_flags & 0x80;
            default:
              return ((choice_id - 1) == target_c->character()->disp.visual.char_class);
          }
        },
    },
    ChoiceSearchCategory{
        .id = 0x0003,
        .name = "Platform",
        .choices = {
            {0x0000, "Any"},
            {0x0001, "DC betas"},
            {0x0002, "DC V1"},
            {0x0003, "DC V2 / PC"},
            {0x0004, "GC / Xbox Episodes 1&2"},
            {0x0005, "GC Episode 3"},
            {0x0006, "BB"},
        },
        .client_matches = +[](shared_ptr<Client>, shared_ptr<Client> target_c, uint16_t choice_id) -> bool {
          if (choice_id == 0x0000) {
            return true;
          }
          switch (target_c->version()) {
            case Version::DC_NTE:
            case Version::DC_11_2000:
              return (choice_id == 0x0001);
            case Version::DC_V1:
              return (choice_id == 0x0002);
            case Version::DC_V2:
            case Version::PC_NTE:
            case Version::PC_V2:
              return (choice_id == 0x0003);
            case Version::GC_NTE:
            case Version::GC_V3:
            case Version::XB_V3:
              return (choice_id == 0x0004);
            case Version::GC_EP3_NTE:
            case Version::GC_EP3:
              return (choice_id == 0x0005);
            case Version::BB_V4:
              return (choice_id == 0x0006);
            default:
              return false;
          }
        },
    },
    ChoiceSearchCategory{
        .id = 0x0204,
        .name = "Game mode",
        .choices = {
            {0x0000, "Any"},
            {0x0001, "Normal"},
            {0x0002, "Hard"},
            {0x0003, "Very Hard"},
            {0x0004, "Ultimate"},
            {0x0005, "Battle"},
            {0x0006, "Challenge"},
        },
        .client_matches = +[](shared_ptr<Client>, shared_ptr<Client> target_c, uint16_t choice_id) -> bool {
          uint16_t target_choice_id = target_c->character()->choice_search_config.get_setting(0x0204);
          return (choice_id == 0) || (target_choice_id == 0) || (choice_id == target_choice_id);
        },
    },
});
