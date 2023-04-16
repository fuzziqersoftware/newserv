#include "Menu.hh"

using namespace std;

MenuItem::MenuItem(
    uint32_t item_id, const u16string& name,
    const u16string& description, uint32_t flags)
    : item_id(item_id),
      name(name),
      description(description),
      flags(flags) {}
