#include "ItemTranslationTable.hh"

using namespace std;

static constexpr bool is_canonical(uint32_t id) {
  return !(id & 0x80000000);
}
static constexpr uint32_t make_canonical(uint32_t id) {
  return (id & 0x7FFFFFFF);
}

ItemTranslationTable::ItemTranslationTable(
    const phosg::JSON& json,
    const std::array<std::shared_ptr<const ItemParameterTable>, NUM_VERSIONS>& item_parameter_tables) {
  // Parse the table and build the indexes
  const auto& l = json.as_list();
  for (size_t z = 0; z < l.size(); z++) {
    const auto& e = this->entries.emplace_back(*l[z]);

    bool has_any_canonical_id = false;
    for (size_t v_s = 0; v_s < NUM_NON_PATCH_VERSIONS; v_s++) {
      uint32_t id = e.id_for_version[v_s];
      if (is_canonical(id)) {
        has_any_canonical_id = true;
        if (!this->entry_index_for_version[v_s].emplace(id, z).second) {
          throw runtime_error(phosg::string_printf("(row %zu) duplicate canonical ID %08" PRIX32, z, id));
        }
      }
    }
    if (!has_any_canonical_id) {
      throw runtime_error(phosg::string_printf("(row %zu) no canonical ID present in row", z));
    }
  }

  // Validate the table by checking that:
  // - Each non-canonical ID points to an existing canonical ID
  // - Each canonical ID has an entry in the item parameter table
  // - All items in the parameter tables are represented in the translation table
  for (size_t v_s = 0; v_s < NUM_NON_PATCH_VERSIONS; v_s++) {
    Version v = static_cast<Version>(v_s + NUM_PATCH_VERSIONS);
    auto item_parameter_table = item_parameter_tables.at(v_s + NUM_PATCH_VERSIONS);
    auto remaining_identifiers = item_parameter_table->compute_all_valid_primary_identifiers();

    const auto& entry_index = this->entry_index_for_version[v_s];
    for (size_t z = 0; z < this->entries.size(); z++) {
      uint32_t e_id = this->entries[z].id_for_version[v_s];
      if (is_canonical(e_id)) {
        if (!entry_index.count(e_id)) {
          throw logic_error(phosg::string_printf("(row %zu version %s) canonical ID %" PRIX32 " is missing from the index", z, phosg::name_for_enum(v), e_id));
        }
        try {
          item_parameter_table->definition_for_primary_identifier(e_id);
        } catch (const out_of_range&) {
          throw runtime_error(phosg::string_printf("(row %zu version %s) ID %" PRIX32 " not defined in item parameter table", z, phosg::name_for_enum(v), e_id));
        }
        if (!remaining_identifiers.erase(e_id)) {
          throw runtime_error(phosg::string_printf("(row %zu version %s) ID %" PRIX32 " not in item parameter table's primary identifier list", z, phosg::name_for_enum(v), e_id));
        }
      } else if (!entry_index.count(make_canonical(e_id))) {
        throw runtime_error(phosg::string_printf("(row %zu version %s) ID %" PRIX32 " refers to nonexistent canonical ID", z, phosg::name_for_enum(v), e_id));
      }
    }

    if (!remaining_identifiers.empty()) {
      string missing_str = phosg::string_printf("(version %s) not all identifiers in the item parameter table are defined in the translation table; missing:", phosg::name_for_enum(v));
      for (uint32_t id : remaining_identifiers) {
        missing_str += phosg::string_printf(" %08" PRIX32, id);
      }
      throw runtime_error(missing_str);
    }
  }
}

phosg::JSON ItemTranslationTable::json() const {
  auto ret = phosg::JSON::list();
  for (const auto& entry : this->entries) {
    ret.emplace_back(entry.json());
  }
  return ret;
}

uint32_t ItemTranslationTable::translate(uint32_t primary_identifier, Version from_version, Version to_version) const {
  if (from_version == to_version) {
    return primary_identifier;
  }
  const auto& entry_index = this->entry_index_for_version.at(static_cast<size_t>(from_version) - NUM_PATCH_VERSIONS);
  size_t entry_num = entry_index.at(primary_identifier);
  const auto& entry = this->entries.at(entry_num);
  return make_canonical(entry.id_for_version.at(static_cast<size_t>(to_version) - NUM_PATCH_VERSIONS));
}

ItemTranslationTable::Entry::Entry(const phosg::JSON& json) {
  const auto& l = json.as_list();
  if (l.size() != NUM_NON_PATCH_VERSIONS + 1) {
    throw runtime_error("list length is incorrect");
  }
  for (size_t z = 0; z < NUM_NON_PATCH_VERSIONS; z++) {
    this->id_for_version[z] = l[z]->as_int();
  }
  this->name = l[NUM_NON_PATCH_VERSIONS]->as_string();
}

phosg::JSON ItemTranslationTable::Entry::json() const {
  auto ret = phosg::JSON::list();
  for (size_t z = 0; z < NUM_NON_PATCH_VERSIONS; z++) {
    ret.emplace_back(this->id_for_version[z]);
  }
  ret.emplace_back(this->name);
  return ret;
}
