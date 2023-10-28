#pragma once

#include <stdint.h>

#include <memory>
#include <phosg/JSON.hh>
#include <string>
#include <unordered_map>
#include <vector>

#include "ItemData.hh"
#include "ItemParameterTable.hh"

class ItemNameIndex {
public:
  ItemNameIndex(JSON&& v2_names, JSON&& v3_names, JSON&& v4_names);

  std::string describe_item(
      GameVersion version,
      const ItemData& item,
      std::shared_ptr<const ItemParameterTable> item_parameter_table = nullptr) const;
  ItemData parse_item_description(GameVersion version, const std::string& description) const;

private:
  ItemData parse_item_description_phase(GameVersion version, const std::string& description, bool skip_special) const;

  struct ItemMetadata {
    uint32_t primary_identifier;
    std::string v2_name;
    std::string v3_name;
    std::string v4_name;
  };

  std::unordered_map<uint32_t, std::shared_ptr<ItemMetadata>> primary_identifier_index;
  std::map<std::string, std::shared_ptr<ItemMetadata>> v2_name_index;
  std::map<std::string, std::shared_ptr<ItemMetadata>> v3_name_index;
  std::map<std::string, std::shared_ptr<ItemMetadata>> v4_name_index;
};
