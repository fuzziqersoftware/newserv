#pragma once

#include <inttypes.h>

#include <map>
#include <memory>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Menu.hh"

class ClientFunctionIndex {
public:
  struct Function {
    enum class Architecture {
      UNKNOWN = 0,
      POWERPC, // GC
      X86, // PC, XB, BB
      SH4, // Dreamcast
    };
    Architecture arch = Architecture::UNKNOWN;
    std::string code;
    std::vector<uint16_t> relocation_deltas;
    std::unordered_map<std::string, uint32_t> label_offsets;
    uint32_t entrypoint_offset_offset = 0;
    std::string short_name; // Based on filename
    std::string long_name; // From .meta name directive
    std::string description; // From .meta description directive
    uint64_t client_flag = 0; // From .meta client_flag directive
    uint32_t menu_item_id = 0;
    enum class Visibility {
      DEBUG_ONLY = 0,
      CHAT_COMMAND_ONLY_WITH_CHEAT_MODE,
      CHAT_COMMAND_ONLY,
      PATCHES_MENU_ONLY,
      PATCHES_MENU_AND_CHAT_COMMAND,
    };
    Visibility visibility;
    bool show_return_value = false;
    uint32_t specific_version;

    inline bool appears_in_patches_menu() const {
      return (this->visibility == Visibility::PATCHES_MENU_ONLY) ||
          (this->visibility == Visibility::PATCHES_MENU_AND_CHAT_COMMAND);
    }
    inline bool allowed_via_chat_command(bool cheat_mode_enabled) const {
      return (cheat_mode_enabled && (this->visibility == Visibility::CHAT_COMMAND_ONLY_WITH_CHEAT_MODE)) ||
          (this->visibility == Visibility::CHAT_COMMAND_ONLY) ||
          (this->visibility == Visibility::PATCHES_MENU_AND_CHAT_COMMAND);
    }

    inline bool is_big_endian() const {
      return (this->arch == Architecture::POWERPC);
    }

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

  ClientFunctionIndex() = default;
  ClientFunctionIndex(const std::string& directory, bool raise_on_any_failure);

  std::unordered_map<std::string, std::shared_ptr<Function>> all_functions; // Key is "PatchName-SpecificVersion"
  std::map<uint32_t, std::map<std::string, std::shared_ptr<Function>>> functions_by_specific_version;
  std::map<uint32_t, std::shared_ptr<Function>> functions_by_menu_item_id;

  std::shared_ptr<const Menu> patch_switches_menu(
      uint32_t specific_version,
      const std::unordered_set<std::string>& server_auto_patches_enabled,
      const std::unordered_set<std::string>& client_auto_patches_enabled) const;
  bool patch_menu_empty(uint32_t specific_version) const;

  std::shared_ptr<const Function> get(const std::string& name, uint32_t specific_version) const;
  std::shared_ptr<const Function> get(const std::string& name, Function::Architecture arch) const;
  std::shared_ptr<const Function> get(const std::string& name) const;
  std::shared_ptr<const Function> get_by_menu_item_id(uint32_t menu_item_id) const;
};

const char* name_for_architecture(ClientFunctionIndex::Function::Architecture arch);
uint32_t specific_version_for_gc_header_checksum(uint32_t header_checksum);

template <>
const char* phosg::name_for_enum<ClientFunctionIndex::Function::Visibility>(
    ClientFunctionIndex::Function::Visibility vis);
