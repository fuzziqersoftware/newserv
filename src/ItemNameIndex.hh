#pragma once

#include <stdint.h>

#include <array>
#include <memory>
#include <phosg/JSON.hh>
#include <string>
#include <unordered_map>
#include <vector>

#include "ItemData.hh"
#include "ItemParameterTable.hh"

class ItemNameIndex {
public:
  struct ItemMetadata {
    uint32_t primary_identifier;
    std::string name;
  };

  ItemNameIndex(
      Version version,
      std::shared_ptr<const ItemParameterTable> pmt,
      const std::vector<std::string>& name_coll);

  inline size_t entry_count() const {
    return this->primary_identifier_index.size();
  }

  inline const std::unordered_map<uint32_t, std::shared_ptr<const ItemMetadata>>& all_by_primary_identifier() const {
    return this->primary_identifier_index;
  }
  inline const std::map<std::string, std::shared_ptr<const ItemMetadata>>& all_by_name() const {
    return this->name_index;
  }

  std::string describe_item(const ItemData& item, bool include_color_escapes = false) const;
  ItemData parse_item_description(const std::string& description) const;

private:
  ItemData parse_item_description_phase(const std::string& description, bool skip_special) const;

  Version version;
  std::shared_ptr<const ItemParameterTable> item_parameter_table;

  std::unordered_map<uint32_t, std::shared_ptr<const ItemMetadata>> primary_identifier_index;
  std::map<std::string, std::shared_ptr<const ItemMetadata>> name_index;
};
