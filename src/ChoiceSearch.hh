#pragma once

#include <functional>
#include <memory>
#include <phosg/Encoding.hh>
#include <string>
#include <vector>

#include "Text.hh"

class Client;

struct ChoiceSearchConfig {
  le_uint32_t disabled = 1; // 0 = enabled, 1 = disabled. Unused in command C3
  struct Entry {
    le_uint16_t parent_choice_id = 0;
    le_uint16_t choice_id = 0;
  } __attribute__((packed));
  parray<Entry, 5> entries;

  int32_t get_setting(uint16_t parent_choice_id) const {
    for (size_t z = 0; z < this->entries.size(); z++) {
      if (this->entries[z].parent_choice_id == parent_choice_id) {
        return this->entries[z].choice_id;
      }
    }
    return -1;
  }
} __attribute__((packed));

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
