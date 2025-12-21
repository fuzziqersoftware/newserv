#include "FunctionCompiler.hh"

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

const char* name_for_architecture(CompiledFunctionCode::Architecture arch) {
  switch (arch) {
    case CompiledFunctionCode::Architecture::POWERPC:
      return "PowerPC";
    case CompiledFunctionCode::Architecture::X86:
      return "x86";
    case CompiledFunctionCode::Architecture::SH4:
      return "SH-4";
    default:
      throw logic_error("invalid architecture");
  }
}

template <bool BE>
string CompiledFunctionCode::generate_client_command_t(
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

string CompiledFunctionCode::generate_client_command(
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t override_relocations_offset) const {
  if (this->arch == Architecture::POWERPC) {
    return this->generate_client_command_t<true>(label_writes, suffix_data, suffix_size, override_relocations_offset);
  } else if ((this->arch == Architecture::X86) || (this->arch == Architecture::SH4)) {
    return this->generate_client_command_t<false>(label_writes, suffix_data, suffix_size, override_relocations_offset);
  } else {
    throw logic_error("invalid architecture");
  }
}

bool CompiledFunctionCode::is_big_endian() const {
  return (this->arch == Architecture::POWERPC);
}

static unordered_map<uint32_t, std::string> preprocess_function_code(const std::string& text) {
  auto parse_specific_version_list = +[](std::string&& text) -> vector<uint32_t> {
    phosg::strip_whitespace(text);
    vector<uint32_t> ret;
    for (auto& vers_token : phosg::split(text, ' ')) {
      phosg::strip_whitespace(vers_token);
      if (vers_token.empty()) {
        continue;
      }
      if (vers_token.size() != 4) {
        throw std::runtime_error("invalid specific_version: " + vers_token);
      }
      ret.emplace_back(*reinterpret_cast<const be_uint32_t*>(vers_token.data()));
    }
    return ret;
  };

  // Find a .versions directive and populate specific_versions
  vector<uint32_t> specific_versions;
  auto lines = phosg::split(text, '\n');
  for (auto& line : lines) {
    if (line.starts_with(".versions ")) {
      if (!specific_versions.empty()) {
        throw std::runtime_error("multiple .versions directives in file");
      }
      specific_versions = parse_specific_version_list(line.substr(10));
      if (specific_versions.empty()) {
        throw std::runtime_error(".versions directive does not specify any versions");
      }
      line.clear();
    }
  }

  // If there's no .versions directive, just return the text as-is
  if (specific_versions.empty()) {
    return {{0, std::move(text)}};
  }

  vector<deque<string>> version_lines;
  version_lines.resize(specific_versions.size());

  size_t line_num = 1;
  vector<uint32_t> current_only_versions;
  unordered_set<uint32_t> current_only_versions_set;
  for (auto& line : lines) {
    phosg::strip_whitespace(line);
    if (line.starts_with(".only_versions ")) {
      current_only_versions = parse_specific_version_list(line.substr(15));
      current_only_versions_set.clear();
      for (uint32_t specific_version : current_only_versions) {
        current_only_versions_set.emplace(specific_version);
      }

    } else if (line == ".all_versions") {
      current_only_versions.clear();
      current_only_versions_set.clear();

    } else {
      size_t vers_offset = line.find("<VERS ");
      if (vers_offset == string::npos) {
        for (size_t vers_index = 0; vers_index < specific_versions.size(); vers_index++) {
          if (current_only_versions.empty() || current_only_versions_set.count(specific_versions[vers_index])) {
            version_lines[vers_index].emplace_back(line);
          } else {
            version_lines[vers_index].emplace_back("");
          }
        }

      } else {
        size_t token_index = 0;
        for (size_t vers_index = 0; vers_index < specific_versions.size(); vers_index++) {
          if (current_only_versions.empty() || current_only_versions_set.count(specific_versions[vers_index])) {
            string version_line = line;
            size_t vers_offset = line.find("<VERS ");
            while (vers_offset != string::npos) {
              size_t end_offset = version_line.find('>', vers_offset + 6);
              if (end_offset == string::npos) {
                throw runtime_error(std::format("(line {}) unterminated <VERS> replacement", line_num));
              }
              auto tokens = phosg::split(version_line.substr(vers_offset + 6, end_offset - vers_offset - 6), ' ');
              if (tokens.size() <= token_index) {
                throw runtime_error(std::format("(line {}) invalid <VERS> replacement", line_num));
              }
              version_line = version_line.substr(0, vers_offset) + tokens.at(token_index) + version_line.substr(end_offset + 1);
              vers_offset = version_line.find("<VERS ");
            }
            version_lines[vers_index].emplace_back(version_line);
            token_index++;
          } else {
            version_lines[vers_index].emplace_back("");
          }
        }
      }
    }

    line_num++;
  }

  unordered_map<uint32_t, string> ret;
  for (size_t z = 0; z < specific_versions.size(); z++) {
    ret.emplace(specific_versions[z], phosg::join(version_lines.at(z), "\n"));
  }
  return ret;
}

static vector<shared_ptr<CompiledFunctionCode>> compile_function_code(
    CompiledFunctionCode::Architecture arch,
    const string& function_directory,
    const string& system_directory,
    const string& name,
    const string& text,
    bool raise_on_any_failure) {
  unordered_set<string> get_include_stack;
  function<string(const string&)> get_include = [&](const string& name) -> string {
    const char* arch_name_token;
    switch (arch) {
      case CompiledFunctionCode::Architecture::POWERPC:
        arch_name_token = "ppc";
        break;
      case CompiledFunctionCode::Architecture::X86:
        arch_name_token = "x86";
        break;
      case CompiledFunctionCode::Architecture::SH4:
        arch_name_token = "sh4";
        break;
      default:
        throw runtime_error("unknown architecture");
    }

    // Look in the function directory first, then the system directory
    string asm_filename = std::format("{}/{}.{}.inc.s", function_directory, name, arch_name_token);
    if (!std::filesystem::is_regular_file(asm_filename)) {
      asm_filename = std::format("{}/{}.{}.inc.s", system_directory, name, arch_name_token);
    }
    if (std::filesystem::is_regular_file(asm_filename)) {
      if (!get_include_stack.emplace(name).second) {
        throw runtime_error("mutual recursion between includes: " + name);
      }
      ResourceDASM::EmulatorBase::AssembleResult ret;
      switch (arch) {
        case CompiledFunctionCode::Architecture::POWERPC:
          ret = ResourceDASM::PPC32Emulator::assemble(phosg::load_file(asm_filename), get_include);
          break;
        case CompiledFunctionCode::Architecture::X86:
          ret = ResourceDASM::X86Emulator::assemble(phosg::load_file(asm_filename), get_include);
          break;
        case CompiledFunctionCode::Architecture::SH4:
          ret = ResourceDASM::SH4Emulator::assemble(phosg::load_file(asm_filename), get_include);
          break;
        default:
          throw runtime_error("unknown architecture");
      }
      get_include_stack.erase(name);
      return ret.code;
    }

    string bin_filename = function_directory + "/" + name + ".inc.bin";
    if (std::filesystem::is_regular_file(bin_filename)) {
      return phosg::load_file(bin_filename);
    }
    bin_filename = system_directory + "/" + name + ".inc.bin";
    if (std::filesystem::is_regular_file(bin_filename)) {
      return phosg::load_file(bin_filename);
    }
    throw runtime_error("data not found for include: " + name + " (from " + asm_filename + " or " + bin_filename + ")");
  };

  auto version_texts = preprocess_function_code(text);

  vector<shared_ptr<CompiledFunctionCode>> ret;
  for (const auto& [specific_version, version_text] : version_texts) {
    try {
      ResourceDASM::EmulatorBase::AssembleResult assembled;
      if (arch == CompiledFunctionCode::Architecture::POWERPC) {
        assembled = ResourceDASM::PPC32Emulator::assemble(version_text, get_include);
      } else if (arch == CompiledFunctionCode::Architecture::X86) {
        assembled = ResourceDASM::X86Emulator::assemble(version_text, get_include);
      } else if (arch == CompiledFunctionCode::Architecture::SH4) {
        assembled = ResourceDASM::SH4Emulator::assemble(version_text, get_include);
      } else {
        throw runtime_error("invalid architecture");
      }

      auto compiled = ret.emplace_back(make_shared<CompiledFunctionCode>());
      compiled->arch = arch;
      compiled->short_name = name;
      compiled->specific_version = specific_version;
      compiled->code = std::move(assembled.code);
      compiled->label_offsets = std::move(assembled.label_offsets);
      for (const auto& it : assembled.metadata_keys) {
        if (it.first == "hide_from_patches_menu") {
          compiled->hide_from_patches_menu = true;
        } else if (it.first == "name") {
          compiled->long_name = it.second;
        } else if (it.first == "description") {
          compiled->description = it.second;
        } else if (it.first == "client_flag") {
          compiled->client_flag = stoull(it.second, nullptr, 0);
        } else {
          throw runtime_error("unknown metadata key: " + it.first);
        }
      }

      set<uint32_t> reloc_indexes;
      for (const auto& it : compiled->label_offsets) {
        if (it.first.starts_with("reloc")) {
          reloc_indexes.emplace(it.second / 4);
        }
      }

      try {
        compiled->entrypoint_offset_offset = compiled->label_offsets.at("entry_ptr");
      } catch (const out_of_range&) {
        throw runtime_error("code does not contain entry_ptr label");
      }

      uint32_t prev_index = 0;
      for (const auto& it : reloc_indexes) {
        uint32_t delta = it - prev_index;
        if (delta > 0xFFFF) {
          throw runtime_error("relocation delta too far away");
        }
        compiled->relocation_deltas.emplace_back(delta);
        prev_index = it;
      }

    } catch (const exception& e) {
      string version_str = specific_version ? (" (" + str_for_specific_version(specific_version) + ")") : "";
      if (raise_on_any_failure) {
        throw;
      }
      function_compiler_log.warning_f("Failed to compile function {}{}: {}", name, version_str, e.what());
    }
  }

  return ret;
}

FunctionCodeIndex::FunctionCodeIndex(const string& directory, bool raise_on_any_failure) {
  string system_dir_path = directory.ends_with("/") ? (directory + "System") : (directory + "/System");

  uint32_t next_menu_item_id = 1;
  for (const auto& item : std::filesystem::directory_iterator(directory)) {
    string subdir_name = item.path().filename().string();
    string subdir_path = directory.ends_with("/") ? (directory + subdir_name) : (directory + "/" + subdir_name);

    auto add_file = [&](string filename) -> void {
      try {
        if (!filename.ends_with(".s")) {
          return;
        }

        string name = filename.substr(0, filename.size() - 2);
        if (name.ends_with(".inc")) {
          return;
        }

        bool is_patch = name.ends_with(".patch");
        if (is_patch) {
          name.resize(name.size() - 6);
        }

        // Figure out the version or specific_version
        CompiledFunctionCode::Architecture arch = CompiledFunctionCode::Architecture::UNKNOWN;
        uint32_t specific_version = 0;
        string short_name = name;
        if (name.ends_with(".ppc")) {
          arch = CompiledFunctionCode::Architecture::POWERPC;
          name.resize(name.size() - 4);
          short_name = name;
        } else if (name.ends_with(".x86")) {
          arch = CompiledFunctionCode::Architecture::X86;
          name.resize(name.size() - 4);
          short_name = name;
        } else if (name.ends_with(".sh4")) {
          arch = CompiledFunctionCode::Architecture::SH4;
          name.resize(name.size() - 4);
          short_name = name;
        } else if (is_patch && (name.size() >= 5) && (name[name.size() - 5] == '.')) {
          specific_version = (name[name.size() - 4] << 24) | (name[name.size() - 3] << 16) | (name[name.size() - 2] << 8) | name[name.size() - 1];
          if (specific_version_is_dc(specific_version)) {
            arch = CompiledFunctionCode::Architecture::SH4;
          } else if (specific_version_is_gc(specific_version)) {
            arch = CompiledFunctionCode::Architecture::POWERPC;
          } else if (specific_version_is_pc_v2(specific_version) ||
              specific_version_is_xb(specific_version) ||
              specific_version_is_bb(specific_version)) {
            arch = CompiledFunctionCode::Architecture::X86;
          } else {
            throw runtime_error("unable to determine architecture from specific_version");
          }
          short_name = name.substr(0, name.size() - 5);
        }

        if (arch == CompiledFunctionCode::Architecture::UNKNOWN) {
          throw runtime_error("unable to determine architecture");
        }

        string path = subdir_path + "/" + filename;
        string text = phosg::load_file(path);
        for (auto code : compile_function_code(arch, subdir_path, system_dir_path, name, text, raise_on_any_failure)) {
          if (code->specific_version == 0) {
            code->specific_version = specific_version;
          }
          code->source_path = path;
          code->short_name = short_name;
          this->name_to_function.emplace(name, code);
          if (is_patch) {
            code->menu_item_id = next_menu_item_id++;
            this->menu_item_id_and_specific_version_to_patch_function.emplace(
                static_cast<uint64_t>(code->menu_item_id) << 32 | code->specific_version, code);
            this->name_and_specific_version_to_patch_function.emplace(
                std::format("{}-{:08X}", code->short_name, code->specific_version), code);
          }

          string patch_prefix = is_patch ? std::format("[{:08X}] ", code->menu_item_id) : "";
          function_compiler_log.debug_f("Compiled function {}{} ({}; {})",
              patch_prefix, name, str_for_specific_version(code->specific_version), name_for_architecture(code->arch));
        }

      } catch (const exception& e) {
        if (raise_on_any_failure) {
          throw runtime_error(format("({}) {}", filename, e.what()));
        }
        function_compiler_log.warning_f("Failed to compile function {}: {}", filename, e.what());
      }
    };

    if (std::filesystem::is_regular_file(subdir_path)) {
      add_file(subdir_path);
    } else if (std::filesystem::is_directory(subdir_path)) {
      for (const auto& item : std::filesystem::directory_iterator(subdir_path)) {
        string filename = item.path().filename().string();
        add_file(filename);
      }
    } else {
      function_compiler_log.warning_f("Skipping {} (unknown file type)", subdir_name);
      continue;
    }
  }
}

shared_ptr<const Menu> FunctionCodeIndex::patch_switches_menu(
    uint32_t specific_version,
    const std::unordered_set<std::string>& server_auto_patches_enabled,
    const std::unordered_set<std::string>& client_auto_patches_enabled) const {
  auto suffix = std::format("-{:08X}", specific_version);

  auto ret = make_shared<Menu>(MenuID::PATCH_SWITCHES, "Patches");
  ret->items.emplace_back(PatchesMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);
  for (const auto& it : this->name_and_specific_version_to_patch_function) {
    const auto& fn = it.second;
    if (fn->hide_from_patches_menu || !it.first.ends_with(suffix) || server_auto_patches_enabled.count(fn->short_name)) {
      continue;
    }
    string name;
    name.push_back(client_auto_patches_enabled.count(fn->short_name) ? '*' : '-');
    name += fn->long_name.empty() ? fn->short_name : fn->long_name;
    ret->items.emplace_back(fn->menu_item_id, name, fn->description, MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE);
  }
  return ret;
}

bool FunctionCodeIndex::patch_menu_empty(uint32_t specific_version) const {
  uint32_t mask = specific_version_is_indeterminate(specific_version) ? 0xFF000000 : 0xFFFFFFFF;
  for (const auto& it : this->menu_item_id_and_specific_version_to_patch_function) {
    if ((it.first & mask) == (specific_version & mask)) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<const CompiledFunctionCode> FunctionCodeIndex::get_patch(
    const std::string& name, uint32_t specific_version) const {
  return this->name_and_specific_version_to_patch_function.at(std::format("{}-{:08X}", name, specific_version));
}

DOLFileIndex::DOLFileIndex(const string& directory) {
  if (!std::filesystem::is_directory(directory)) {
    function_compiler_log.info_f("DOL file directory is missing");
    return;
  }

  auto menu = make_shared<Menu>(MenuID::PROGRAMS, "Programs");
  this->menu = menu;
  menu->items.emplace_back(ProgramsMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);

  uint32_t next_menu_item_id = 0;
  for (const auto& item : std::filesystem::directory_iterator(directory)) {
    string filename = item.path().filename().string();
    bool is_dol = filename.ends_with(".dol");
    bool is_compressed_dol = filename.ends_with(".dol.prs");
    if (!is_dol && !is_compressed_dol) {
      continue;
    }
    string name = filename.substr(0, filename.size() - (is_compressed_dol ? 8 : 4));

    try {
      auto dol = make_shared<File>();
      dol->menu_item_id = next_menu_item_id++;
      dol->name = name;

      string path = directory + "/" + filename;
      string file_data = phosg::load_file(path);

      string description;
      if (is_compressed_dol) {
        size_t decompressed_size = prs_decompress_size(file_data);

        phosg::StringWriter w;
        w.put_u32b(file_data.size());
        w.put_u32b(decompressed_size);
        w.write(file_data);
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());

        string compressed_size_str = phosg::format_size(file_data.size());
        string decompressed_size_str = phosg::format_size(decompressed_size);
        function_compiler_log.debug_f("Loaded compressed DOL file {} ({} -> {})",
            dol->name, compressed_size_str, decompressed_size_str);
        description = std::format("$C6{}$C7\n{}\n{} (orig)", dol->name, compressed_size_str, decompressed_size_str);

      } else {
        phosg::StringWriter w;
        w.put_u32b(0);
        w.put_u32b(file_data.size());
        w.write(file_data);
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());

        string size_str = phosg::format_size(dol->data.size());
        function_compiler_log.debug_f("Loaded DOL file {} ({})", filename, size_str);
        description = std::format("$C6{}$C7\n{}", dol->name, size_str);
      }

      this->name_to_file.emplace(dol->name, dol);
      this->item_id_to_file.emplace_back(dol);

      menu->items.emplace_back(dol->menu_item_id, dol->name, description, MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE);

    } catch (const exception& e) {
      function_compiler_log.warning_f("Failed to load DOL file {}: {}", filename, e.what());
    }
  }
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
