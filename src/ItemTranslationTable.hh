#pragma once

#include <stdint.h>

#include <array>
#include <memory>
#include <phosg/JSON.hh>
#include <string>
#include <unordered_map>

#include "ItemParameterTable.hh"
#include "Types.hh"
#include "Version.hh"

class ItemTranslationTable {
public:
  ItemTranslationTable(
      const phosg::JSON& json,
      const std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS>& item_parameter_tables);
  ~ItemTranslationTable() = default;

  phosg::JSON json() const;

  uint32_t translate(uint32_t primary_identifier, Version from_version, Version to_version) const;

private:
  struct Entry {
    std::array<uint32_t, NUM_NON_PATCH_VERSIONS> id_for_version;
    std::string name;

    explicit Entry(const phosg::JSON& json);
    phosg::JSON json() const;
  };
  std::vector<Entry> entries;
  std::array<std::unordered_map<uint32_t, size_t>, NUM_NON_PATCH_VERSIONS> entry_index_for_version;
};
