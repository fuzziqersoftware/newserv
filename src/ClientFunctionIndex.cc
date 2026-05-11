#include "ClientFunctionIndex.hh"

#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Time.hh>
#include <stdexcept>

#include <resource_file/Emulators/PPC32Emulator.hh>
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>

#include "CommandFormats.hh"
#include "CommonFileFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"

using namespace std;

using Arch = ClientFunctionIndex::Function::Architecture;

const char* name_for_architecture(Arch arch) {
  switch (arch) {
    case Arch::SH4:
      return "SH-4";
    case Arch::POWERPC:
      return "PowerPC";
    case Arch::X86:
      return "x86";
    default:
      throw logic_error("invalid architecture");
  }
}

uint32_t specific_version_for_architecture(Arch arch) {
  switch (arch) {
    case Arch::SH4:
      return SPECIFIC_VERSION_SH4_INDETERMINATE;
    case Arch::POWERPC:
      return SPECIFIC_VERSION_PPC_INDETERMINATE;
    case Arch::X86:
      return SPECIFIC_VERSION_X86_INDETERMINATE;
    default:
      throw logic_error("invalid architecture");
  }
}

Arch architecture_for_specific_version(uint32_t specific_version) {
  if (specific_version == SPECIFIC_VERSION_SH4_INDETERMINATE) {
    return Arch::SH4;
  } else if (specific_version == SPECIFIC_VERSION_PPC_INDETERMINATE) {
    return Arch::POWERPC;
  } else if (specific_version == SPECIFIC_VERSION_X86_INDETERMINATE) {
    return Arch::X86;
  } else if (specific_version_is_dc(specific_version)) {
    return Arch::SH4;
  } else if (specific_version_is_gc(specific_version)) {
    return Arch::POWERPC;
  } else {
    return Arch::X86;
  }
}

static inline std::string cache_key(const std::string& name, uint32_t specific_version) {
  return std::format("{}-{:08X}", name, specific_version);
}

template <typename T>
const T& get_with_sv_fallback(
    const std::unordered_map<std::string, T>& index, const std::string& name, uint32_t specific_version) {
  try {
    return index.at(cache_key(name, specific_version));
  } catch (const std::out_of_range&) {
  }
  uint32_t arch_specific_version = specific_version_for_architecture(architecture_for_specific_version(
      specific_version));
  if (arch_specific_version != specific_version) {
    try {
      return index.at(cache_key(name, arch_specific_version));
    } catch (const std::out_of_range&) {
    }
  }
  return index.at(name);
}

template <bool BE>
string ClientFunctionIndex::Function::generate_client_command_t(
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t override_relocations_offset) const {
  using FooterT = RELFileFooterT<BE>;

  FooterT footer;
  footer.num_relocations = this->relocation_deltas.size();
  footer.unused1.clear(0);
  footer.root_offset = this->entrypoint_offset_offset;
  footer.unused2.clear(0);

  phosg::StringWriter w;
  if (!label_writes.empty()) {
    string modified_code = this->code;
    for (const auto& it : label_writes) {
      size_t offset = this->label_offsets.at(it.first);
      if (offset > modified_code.size() - 4) {
        throw runtime_error("label out of range");
      }
      *reinterpret_cast<U32T<FooterT::IsBE>*>(modified_code.data() + offset) = it.second;
    }
    w.write(modified_code);
  } else {
    w.write(this->code);
  }
  if (suffix_size) {
    w.write(suffix_data, suffix_size);
  }
  while (w.size() & 3) {
    w.put_u8(0);
  }

  footer.relocations_offset = w.size();

  // Always write at least 4 bytes even if there are no relocations
  if (this->relocation_deltas.empty()) {
    w.put_u32(0);
  }

  if (override_relocations_offset) {
    footer.relocations_offset = override_relocations_offset;
  } else {
    for (uint16_t delta : this->relocation_deltas) {
      w.put<U16T<FooterT::IsBE>>(delta);
    }
    if (this->relocation_deltas.size() & 1) {
      w.put_u16(0);
    }
  }

  w.put(footer);
  return std::move(w.str());
}

string ClientFunctionIndex::Function::generate_client_command(
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t override_relocations_offset) const {
  if (this->is_big_endian()) {
    return this->generate_client_command_t<true>(label_writes, suffix_data, suffix_size, override_relocations_offset);
  } else if ((this->arch == Architecture::X86) || (this->arch == Architecture::SH4)) {
    return this->generate_client_command_t<false>(label_writes, suffix_data, suffix_size, override_relocations_offset);
  } else {
    throw logic_error("invalid architecture");
  }
}

static unordered_map<uint32_t, std::string> preprocess_function_code(const std::string& text) {
  std::unordered_set<uint32_t> all_specific_versions;
  struct Line {
    std::string text;
    std::unordered_map<uint32_t, size_t> new_specific_versions; // Nonempty iff line is a .versions directive
    bool enable_all_versions = false;
  };

  std::vector<Line> lines;
  for (auto& line_text : phosg::split(text, '\n')) {
    auto& line = lines.emplace_back();
    line.text = std::move(line_text);

    string stripped_line = line.text;
    phosg::strip_whitespace(stripped_line);

    if (stripped_line == ".all_versions") {
      line.enable_all_versions = true;
    } else if (stripped_line.starts_with(".versions ")) {
      for (auto& vers_token : phosg::split(stripped_line.substr(10), ' ')) {
        phosg::strip_whitespace(vers_token);
        if (!vers_token.empty()) {
          uint32_t specific_version = specific_version_for_str(vers_token);
          size_t version_index = line.new_specific_versions.size();
          all_specific_versions.emplace(specific_version);
          line.new_specific_versions.emplace(std::move(specific_version), version_index);
        }
      }
    }
  }

  static const std::string empty_str = "";

  unordered_map<uint32_t, std::string> ret;
  for (uint32_t specific_version : all_specific_versions) {
    std::deque<std::string> version_lines;
    bool include_current_line = true;
    size_t current_vers_index = all_specific_versions.size();
    for (size_t line_znum = 0; line_znum < lines.size(); line_znum++) {
      const auto& line = lines[line_znum];

      if (line.enable_all_versions) {
        include_current_line = true;
        current_vers_index = all_specific_versions.size();
        version_lines.emplace_back(empty_str);

      } else if (!line.new_specific_versions.empty()) {
        auto it = line.new_specific_versions.find(specific_version);
        if (it == line.new_specific_versions.end()) {
          include_current_line = false;
          current_vers_index = all_specific_versions.size();
        } else {
          include_current_line = true;
          current_vers_index = it->second;
        }
        version_lines.emplace_back(empty_str);

      } else if (!include_current_line) {
        version_lines.emplace_back(empty_str);

      } else {
        std::string line_text = line.text;
        size_t vers_offset = line_text.find("<VERS ");
        while (vers_offset != string::npos) {
          size_t end_offset = line_text.find('>', vers_offset + 6);
          if (end_offset == string::npos) {
            throw runtime_error(std::format("(version {}) (line {}) unterminated <VERS> replacement",
                str_for_specific_version(specific_version), line_znum + 1));
          }
          auto tokens = phosg::split(line_text.substr(vers_offset + 6, end_offset - vers_offset - 6), ' ');
          if (current_vers_index >= tokens.size()) {
            throw runtime_error(std::format("(version {}) (line {}) invalid <VERS> replacement",
                str_for_specific_version(specific_version), line_znum + 1));
          }
          line_text = line_text.substr(0, vers_offset) + tokens[current_vers_index] + line_text.substr(end_offset + 1);
          vers_offset = line_text.find("<VERS ");
        }
        version_lines.emplace_back(std::move(line_text));
      }
    }
    ret.emplace(specific_version, phosg::join(version_lines, "\n"));
  }

  return ret;
}

ClientFunctionIndex::ClientFunctionIndex(const string& root_dir, bool raise_on_any_failure) {
  map<string, string> source_files;
  std::function<void(const std::string&)> add_directory = [&](const std::string& dir) -> void {
    for (const auto& item : std::filesystem::directory_iterator(dir)) {
      string item_name = item.path().filename().string();
      string item_path = dir.ends_with("/") ? (dir + item_name) : (dir + "/" + item_name);
      if (std::filesystem::is_directory(item_path)) {
        add_directory(item_path);
      } else if (item_path.ends_with(".s") && std::filesystem::is_regular_file(item_path)) {
        client_functions_log.debug_f("Adding {} from {}", item_name, item_path);
        if (!source_files.emplace(item_name, phosg::load_file(item_path)).second) {
          throw std::runtime_error(std::format("Duplicate source filename: {}", item_name));
        }
      } else if (item_path.ends_with(".bin") && std::filesystem::is_regular_file(item_path)) {
        client_functions_log.debug_f("Adding {} from {}", item_name, item_path);
        if (!source_files.emplace(item_name, phosg::load_file(item_path)).second) {
          throw std::runtime_error(std::format("Duplicate binary filename: {}", item_name));
        }
      } else {
        client_functions_log.debug_f("Ignoring {}", item_path);
      }
    }
  };
  add_directory(root_dir);

  unordered_map<string, string> include_cache;
  uint32_t last_menu_item_id = 0;
  for (const auto& [source_filename, source] : source_files) {
    if (!source_filename.ends_with(".s")) {
      client_functions_log.debug_f("Skipping root compile for {} because it is not a .s file", source_filename);
      continue;
    }
    if (source_filename.ends_with(".inc.s")) {
      client_functions_log.debug_f("Skipping root compile for {} because it is an include", source_filename);
      continue;
    }

    std::unordered_map<uint32_t, std::string> preprocessed;
    try {
      preprocessed = preprocess_function_code(source);
    } catch (const std::exception& e) {
      throw std::runtime_error(std::format("({} preprocessing) {}", source_filename, e.what()));
    }

    for (const auto& [specific_version, source] : preprocessed) {
      shared_ptr<Function> fn = make_shared<Function>();
      fn->short_name = source_filename.substr(0, source_filename.size() - 2);
      fn->specific_version = specific_version;
      fn->menu_item_id = ++last_menu_item_id;
      fn->arch = architecture_for_specific_version(fn->specific_version);

      try {
        unordered_set<string> get_include_stack;
        function<string(const string&, uint32_t)> get_include_for_sv = [&include_cache, &source_files, &get_include_stack, &get_include_for_sv](const string& name, uint32_t specific_version) -> string {
          try {
            return get_with_sv_fallback(include_cache, name, specific_version);
          } catch (const std::out_of_range&) {
          }
          if (client_functions_log.should_log(phosg::LogLevel::L_DEBUG)) {
            client_functions_log.debug_f("({}) Include {}-{} needs to be compiled",
                get_include_stack.size(), name, str_for_specific_version(specific_version));
          }

          auto it = source_files.find(name + ".inc.s");
          if (it != source_files.end()) {
            if (!get_include_stack.emplace(name).second) {
              throw runtime_error("Mutual recursion between includes: " + name);
            }
            for (const auto& [include_specific_version, include_source] : preprocess_function_code(it->second)) {
              ResourceDASM::EmulatorBase::AssembleResult ret;
              auto get_include = std::bind(get_include_for_sv, std::placeholders::_1, include_specific_version);
              switch (architecture_for_specific_version(include_specific_version)) {
                case Arch::POWERPC:
                  ret = ResourceDASM::PPC32Emulator::assemble(include_source, get_include);
                  break;
                case Arch::X86:
                  ret = ResourceDASM::X86Emulator::assemble(include_source, get_include);
                  break;
                case Arch::SH4:
                  ret = ResourceDASM::SH4Emulator::assemble(include_source, get_include);
                  break;
                default:
                  throw runtime_error("unknown architecture");
              }
              if (client_functions_log.should_log(phosg::LogLevel::L_DEBUG)) {
                client_functions_log.debug_f("({}) Compiled include {}-{}",
                    get_include_stack.size(), name, str_for_specific_version(include_specific_version));
              }
              include_cache.emplace(cache_key(name, include_specific_version), std::move(ret.code));
            }
            get_include_stack.erase(name);

          } else {
            it = source_files.find(name + ".inc.bin");
            if (it != source_files.end()) {
              include_cache.emplace(name, it->second).first->second;
              client_functions_log.debug_f("({}) Cached binary include {}", get_include_stack.size(), name);
            }
          }

          try {
            return get_with_sv_fallback(include_cache, name, specific_version);
          } catch (const std::out_of_range&) {
          }
          throw runtime_error(std::format(
              "Data not found for include {} ({})", name, str_for_specific_version(specific_version)));
        };

        try {
          ResourceDASM::EmulatorBase::AssembleResult assembled;
          auto get_include = std::bind(get_include_for_sv, std::placeholders::_1, specific_version);
          switch (fn->arch) {
            case Arch::POWERPC:
              assembled = ResourceDASM::PPC32Emulator::assemble(source, get_include);
              break;
            case Arch::X86:
              assembled = ResourceDASM::X86Emulator::assemble(source, get_include);
              break;
            case Arch::SH4:
              assembled = ResourceDASM::SH4Emulator::assemble(source, get_include);
              break;
            default:
              throw runtime_error("invalid architecture");
          }

          fn->code = std::move(assembled.code);
          fn->label_offsets = std::move(assembled.label_offsets);
          for (const auto& [key, value] : assembled.metadata_keys) {
            if (key == "visibility") {
              if (value == "hidden") {
                fn->visibility = Function::Visibility::DEBUG_ONLY;
              } else if (value == "cheat") {
                fn->visibility = Function::Visibility::CHAT_COMMAND_ONLY_WITH_CHEAT_MODE;
              } else if (value == "chat") {
                fn->visibility = Function::Visibility::CHAT_COMMAND_ONLY;
              } else if (value == "menu") {
                fn->visibility = Function::Visibility::PATCHES_MENU_ONLY;
              } else if (value == "all") {
                fn->visibility = Function::Visibility::PATCHES_MENU_AND_CHAT_COMMAND;
              } else {
                throw std::runtime_error("Invalid visibility value");
              }
            } else if (key == "key") {
              fn->short_name = value;
            } else if (key == "name") {
              fn->long_name = value;
            } else if (key == "description") {
              fn->description = value;
            } else if (key == "client_flag") {
              fn->client_flag = stoull(value, nullptr, 0);
            } else if (key == "show_return_value") {
              fn->show_return_value = true;
            } else {
              throw runtime_error("unknown metadata key: " + key);
            }
          }

          try {
            fn->entrypoint_offset_offset = fn->label_offsets.at("entry_ptr");
          } catch (const out_of_range&) {
            throw runtime_error("code does not contain entry_ptr label");
          }

          set<uint32_t> reloc_indexes;
          for (const auto& it : fn->label_offsets) {
            if (it.first.starts_with("reloc")) {
              reloc_indexes.emplace(it.second / 4);
            }
          }
          uint32_t prev_index = 0;
          for (const auto& it : reloc_indexes) {
            uint32_t delta = it - prev_index;
            if (delta > 0xFFFF) {
              throw runtime_error("relocation delta too far away");
            }
            fn->relocation_deltas.emplace_back(delta);
            prev_index = it;
          }

        } catch (const exception& e) {
          if (raise_on_any_failure) {
            throw;
          }
          client_functions_log.warning_f("Failed to compile function {} ({}): {}",
              fn->short_name, str_for_specific_version(specific_version), e.what());
        }

        auto key = cache_key(fn->short_name, specific_version);
        if (!this->all_functions.emplace(key, fn).second) {
          throw std::runtime_error("Duplicate function key: " + key);
        }
        this->functions_by_specific_version[specific_version].emplace(key, fn);
        this->functions_by_menu_item_id.emplace(fn->menu_item_id, fn);

        client_functions_log.debug_f("Compiled function {} ({}; {}; {})",
            fn->short_name, str_for_specific_version(fn->specific_version), name_for_architecture(fn->arch),
            phosg::name_for_enum(fn->visibility));
      } catch (const std::exception& e) {
        throw std::runtime_error(std::format(
            "({}-{}) {}", fn->short_name, str_for_specific_version(specific_version), e.what()));
      }
    }
  }
}

shared_ptr<const Menu> ClientFunctionIndex::patch_switches_menu(
    uint32_t specific_version,
    const std::unordered_set<std::string>& server_auto_patches_enabled,
    const std::unordered_set<std::string>& client_auto_patches_enabled) const {
  auto ret = make_shared<Menu>(MenuID::PATCH_SWITCHES, "Patches");
  ret->items.emplace_back(PatchesMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);

  auto map_it = this->functions_by_specific_version.find(specific_version);
  if (map_it != this->functions_by_specific_version.end()) {
    for (auto [name, fn] : map_it->second) {
      if (fn->appears_in_patches_menu() && server_auto_patches_enabled.count(fn->short_name)) {
        string item_text;
        item_text.push_back(client_auto_patches_enabled.count(fn->short_name) ? '*' : '-');
        item_text += fn->long_name.empty() ? fn->short_name : fn->long_name;
        ret->items.emplace_back(
            fn->menu_item_id, item_text, fn->description, MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE);
      }
    }
  }
  return ret;
}

bool ClientFunctionIndex::patch_menu_empty(uint32_t specific_version) const {
  uint32_t mask = specific_version_is_indeterminate(specific_version) ? 0xFF000000 : 0xFFFFFFFF;
  auto it = this->functions_by_specific_version.lower_bound(specific_version & mask);
  return ((it == this->functions_by_specific_version.end()) || ((it->first & mask) != (specific_version & mask)));
}

std::shared_ptr<const ClientFunctionIndex::Function> ClientFunctionIndex::get(
    const std::string& name, uint32_t specific_version) const {
  return get_with_sv_fallback(this->all_functions, name, specific_version);
}

std::shared_ptr<const ClientFunctionIndex::Function> ClientFunctionIndex::get(
    const std::string& name, Arch arch) const {
  return get_with_sv_fallback(this->all_functions, name, specific_version_for_architecture(arch));
}

std::shared_ptr<const ClientFunctionIndex::Function> ClientFunctionIndex::get(const std::string& name) const {
  return this->all_functions.at(name);
}

std::shared_ptr<const ClientFunctionIndex::Function> ClientFunctionIndex::get_by_menu_item_id(
    uint32_t menu_item_id) const {
  return this->functions_by_menu_item_id.at(menu_item_id);
}

uint32_t specific_version_for_gc_header_checksum(uint32_t header_checksum) {
  static unordered_map<uint32_t, uint32_t> checksum_to_specific_version;
  if (checksum_to_specific_version.empty()) {
    struct {
      char system_code = 'G';
      char game_code1 = 'P';
      char game_code2;
      char region_code;
      char developer_code1 = '8';
      char developer_code2 = 'P';
      uint8_t disc_number = 0;
      uint8_t version_code;
    } __attribute__((packed)) data;
    for (const char* game_code2 = "OS"; *game_code2; game_code2++) {
      data.game_code2 = *game_code2;
      for (const char* region_code = "JEP"; *region_code; region_code++) {
        data.region_code = *region_code;
        for (uint8_t version_code = 0; version_code < 8; version_code++) {
          data.version_code = version_code;
          uint32_t checksum = phosg::crc32(&data, sizeof(data));
          uint32_t specific_version = 0x33000030 | (*game_code2 << 16) | (*region_code << 8) | version_code;
          if (!checksum_to_specific_version.emplace(checksum, specific_version).second) {
            throw logic_error("multiple specific_versions have same header checksum");
          }
        }
      }
      {
        // Generate entries for Trial Editions
        data.region_code = 'J';
        data.system_code = 'D';
        data.version_code = 0;
        uint32_t checksum = phosg::crc32(&data, sizeof(data));
        uint32_t specific_version = 0x33004A54 | (*game_code2 << 16);
        if (!checksum_to_specific_version.emplace(checksum, specific_version).second) {
          throw logic_error("multiple specific_versions have same header checksum");
        }
        data.system_code = 'G';
      }
    }
  }
  return checksum_to_specific_version.at(header_checksum);
}

template <>
const char* phosg::name_for_enum<ClientFunctionIndex::Function::Visibility>(
    ClientFunctionIndex::Function::Visibility vis) {
  switch (vis) {
    case ClientFunctionIndex::Function::Visibility::DEBUG_ONLY:
      return "DEBUG_ONLY";
    case ClientFunctionIndex::Function::Visibility::CHAT_COMMAND_ONLY_WITH_CHEAT_MODE:
      return "CHAT_COMMAND_ONLY_WITH_CHEAT_MODE";
    case ClientFunctionIndex::Function::Visibility::CHAT_COMMAND_ONLY:
      return "CHAT_COMMAND_ONLY";
    case ClientFunctionIndex::Function::Visibility::PATCHES_MENU_ONLY:
      return "PATCHES_MENU_ONLY";
    case ClientFunctionIndex::Function::Visibility::PATCHES_MENU_AND_CHAT_COMMAND:
      return "PATCHES_MENU_AND_CHAT_COMMAND";
    default:
      throw std::logic_error("Invalid client function visibility");
  }
}
