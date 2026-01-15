#pragma once

#include <inttypes.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Menu.hh"

// TODO: Support x86 and SH4 function calls in the future. Currently we only
// support PPC32 because I haven't written an appropriate x86 assembler yet.

struct CompiledFunctionCode {
  enum class Architecture {
    UNKNOWN = 0,
    POWERPC, // GC
    X86, // PC, XB, BB
    SH4, // Dreamcast
  };
  Architecture arch;
  std::string code;
  std::vector<uint16_t> relocation_deltas;
  std::unordered_map<std::string, uint32_t> label_offsets;
  uint32_t entrypoint_offset_offset = 0;
  std::string source_path; // Path to source file from newserv root
  std::string short_name; // Based on filename
  std::string long_name; // From .meta name directive
  std::string description; // From .meta description directive
  uint64_t client_flag = 0; // From .meta client_flag directive
  uint32_t menu_item_id = 0;
  bool hide_from_patches_menu = false;
  bool show_return_value = false;
  uint32_t specific_version = 0; // 0 = not a client-selectable patch

  bool is_big_endian() const;

  template <bool BE>
  std::string generate_client_command_t(
      const std::unordered_map<std::string, uint32_t>& label_writes,
      const void* suffix_data = nullptr,
      size_t suffix_size = 0,
      uint32_t override_relocations_offset = 0) const;
  std::string generate_client_command(
      const std::unordered_map<std::string, uint32_t>& label_writes = {},
      const void* suffix_data = nullptr,
      size_t suffix_size = 0,
      uint32_t override_relocations_offset = 0) const;
};

const char* name_for_architecture(CompiledFunctionCode::Architecture arch);

struct FunctionCodeIndex {
  FunctionCodeIndex() = default;
  FunctionCodeIndex(const std::string& directory, bool raise_on_any_failure);

  std::unordered_map<std::string, std::shared_ptr<CompiledFunctionCode>> name_to_function;
  std::unordered_map<uint8_t, std::shared_ptr<CompiledFunctionCode>> index_to_function;
  std::unordered_map<uint64_t, std::shared_ptr<CompiledFunctionCode>> menu_item_id_and_specific_version_to_patch_function;
  // Key here is e.g. "PATCHNAME-SPECIFICVERSION", with the latter in hex
  std::map<std::string, std::shared_ptr<CompiledFunctionCode>> name_and_specific_version_to_patch_function;

  std::shared_ptr<const Menu> patch_switches_menu(
      uint32_t specific_version,
      const std::unordered_set<std::string>& server_auto_patches_enabled,
      const std::unordered_set<std::string>& client_auto_patches_enabled) const;
  bool patch_menu_empty(uint32_t specific_version) const;

  std::shared_ptr<const CompiledFunctionCode> get_patch(const std::string& name, uint32_t specific_version) const;
};

struct DOLFileIndex {
  struct File {
    uint32_t menu_item_id;
    std::string name;
    std::string data;
    bool is_compressed;
  };

  std::vector<std::shared_ptr<File>> item_id_to_file;
  std::unordered_map<std::string, std::shared_ptr<File>> name_to_file;
  std::shared_ptr<const Menu> menu;

  DOLFileIndex() = default;
  explicit DOLFileIndex(const std::string& directory);

  inline bool empty() const {
    return this->name_to_file.empty() && this->item_id_to_file.empty();
  }
};

uint32_t specific_version_for_gc_header_checksum(uint32_t header_checksum);
