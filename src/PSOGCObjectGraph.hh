#pragma once

#include <inttypes.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>
#include <vector>

struct PSOGCObjectGraph {
  PSOGCObjectGraph(const std::string& memory_data, uint32_t root_address);

  void print(FILE* stream) const;

  struct VTable {
    uint32_t address;
    uint32_t destroy_addr;
    uint32_t update_addr;
    uint32_t render_addr;
    uint32_t render_shadow_addr;
  };

  struct Object {
    uint32_t address;
    uint16_t flags;
    std::string type_name;
    std::shared_ptr<VTable> vtable;
    std::shared_ptr<Object> parent;
    std::vector<std::shared_ptr<Object>> children;

    void print(FILE* stream, size_t indent_level = 0) const;
  };

  std::shared_ptr<Object> root;
  std::unordered_map<uint32_t, std::shared_ptr<Object>> all_objects;
  std::unordered_map<uint32_t, std::shared_ptr<VTable>> all_vtables;

  std::shared_ptr<Object> parse_object_memo(phosg::StringReader& r, uint32_t addr);
  std::shared_ptr<VTable> parse_vtable_memo(phosg::StringReader& r, uint32_t addr);
};
