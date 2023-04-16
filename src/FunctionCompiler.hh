#pragma once

#include <inttypes.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Menu.hh"

bool function_compiler_available();

// TODO: Support x86 and SH4 function calls in the future. Currently we only
// support PPC32 because I haven't written an appropriate x86 assembler yet.

struct CompiledFunctionCode {
  enum class Architecture {
    POWERPC = 0, // GC
    X86, // PC, XB, BB
    SH4, // Dreamcast
  };
  Architecture arch;
  std::string code;
  std::vector<uint16_t> relocation_deltas;
  std::unordered_map<std::string, uint32_t> label_offsets;
  uint32_t entrypoint_offset_offset;
  std::string name;
  uint32_t index; // 0 = unused (not registered in index_to_function)
  uint32_t menu_item_id;
  bool hide_from_patches_menu;

  bool is_big_endian() const;

  template <typename FooterT>
  std::string generate_client_command_t(
      const std::unordered_map<std::string, uint32_t>& label_writes,
      const std::string& suffix) const;
  std::string generate_client_command(
      const std::unordered_map<std::string, uint32_t>& label_writes = {},
      const std::string& suffix = "") const;
};

const char* name_for_architecture(CompiledFunctionCode::Architecture arch);

std::shared_ptr<CompiledFunctionCode> compile_function_code(
    CompiledFunctionCode::Architecture arch,
    const std::string& directory,
    const std::string& name,
    const std::string& text);

struct FunctionCodeIndex {
  FunctionCodeIndex() = default;
  explicit FunctionCodeIndex(const std::string& directory);

  std::unordered_map<std::string, std::shared_ptr<CompiledFunctionCode>> name_to_function;
  std::unordered_map<uint32_t, std::shared_ptr<CompiledFunctionCode>> index_to_function;
  std::unordered_map<uint32_t, std::shared_ptr<CompiledFunctionCode>> menu_item_id_to_patch_function;

  std::map<std::string, std::shared_ptr<CompiledFunctionCode>> name_to_patch_function;

  std::vector<MenuItem> patch_menu() const;
  inline bool patch_menu_empty() const {
    return this->name_to_patch_function.empty();
  }
};

struct DOLFileIndex {
  struct DOLFile {
    uint32_t menu_item_id;
    std::string name;
    std::string data;
  };

  std::vector<std::shared_ptr<DOLFile>> item_id_to_file;
  std::map<std::string, std::shared_ptr<DOLFile>> name_to_file;

  DOLFileIndex() = default;
  explicit DOLFileIndex(const std::string& directory);

  std::vector<MenuItem> menu() const;
  inline bool empty() const {
    return this->name_to_file.empty() && this->item_id_to_file.empty();
  }
};
