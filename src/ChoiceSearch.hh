#pragma once

#include <functional>
#include <memory>
#include <phosg/Encoding.hh>
#include <string>
#include <vector>

#include "Text.hh"
#include "Types.hh"

class Client;

template <bool BE>
struct ChoiceSearchConfigT {
  U32T<BE> disabled = 1; // 0 = enabled, 1 = disabled. Unused in command C3
  struct Entry {
    U16T<BE> parent_choice_id = 0;
    U16T<BE> choice_id = 0;
  } __packed_ws__(Entry, 4);
  parray<Entry, 5> entries;

  int32_t get_setting(uint16_t parent_choice_id) const {
    for (size_t z = 0; z < this->entries.size(); z++) {
      if (this->entries[z].parent_choice_id == parent_choice_id) {
        return this->entries[z].choice_id;
      }
    }
    return -1;
  }

  operator ChoiceSearchConfigT<!BE>() const {
    ChoiceSearchConfigT<!BE> ret;
    ret.disabled = this->disabled.load();
    for (size_t z = 0; z < this->entries.size(); z++) {
      auto& ret_e = ret.entries[z];
      const auto& this_e = this->entries[z];
      ret_e.parent_choice_id = this_e.parent_choice_id.load();
      ret_e.choice_id = this_e.choice_id.load();
    }
    return ret;
  }
} __packed__;

using ChoiceSearchConfig = ChoiceSearchConfigT<false>;
using ChoiceSearchConfigBE = ChoiceSearchConfigT<true>;
check_struct_size(ChoiceSearchConfig, 0x18);
check_struct_size(ChoiceSearchConfigBE, 0x18);

struct ChoiceSearchCategory {
  struct Choice {
    uint16_t id;
    const char* name;
  };

  uint16_t id;
  const char* name;
  std::vector<Choice> choices;
  std::function<bool(std::shared_ptr<Client> searcher_c, std::shared_ptr<Client> target_c, uint16_t choice_id)> client_matches;
};

extern const std::vector<ChoiceSearchCategory> CHOICE_SEARCH_CATEGORIES;
