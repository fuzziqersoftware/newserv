#pragma once

#include <inttypes.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Menu.hh"

struct DOLFileIndex {
  struct File {
    uint32_t menu_item_id;
    std::string name;
    std::string data;
    bool is_compressed;
  };

  std::vector<std::shared_ptr<File>> item_id_to_file;
  std::shared_ptr<const Menu> menu;

  DOLFileIndex() = default;
  explicit DOLFileIndex(const std::string& directory);

  inline bool empty() const {
    return this->item_id_to_file.empty();
  }
};
