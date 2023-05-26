#include "Menu.hh"

using namespace std;

MenuItem::MenuItem(
    uint32_t item_id, const u16string& name,
    const u16string& description, uint32_t flags)
    : item_id(item_id),
      name(name),
      description(description),
      get_description(nullptr),
      flags(flags) {}

MenuItem::MenuItem(
    uint32_t item_id, const u16string& name,
    std::function<std::u16string()> get_description, uint32_t flags)
    : item_id(item_id),
      name(name),
      description(),
      get_description(std::move(get_description)),
      flags(flags) {}

Menu::Menu(uint32_t menu_id, const std::u16string& name)
    : menu_id(menu_id),
      name(name) {}
