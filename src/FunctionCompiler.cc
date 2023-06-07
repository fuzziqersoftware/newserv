#include "FunctionCompiler.hh"

#include <stdio.h>
#include <string.h>

#include <phosg/Filesystem.hh>
#include <phosg/Time.hh>
#include <stdexcept>

#ifdef HAVE_RESOURCE_FILE
#include <resource_file/Emulators/PPC32Emulator.hh>
#endif

#include "CommandFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"

using namespace std;

static bool is_function_compiler_available = true;

bool function_compiler_available() {
#ifndef HAVE_RESOURCE_FILE
  return false;
#else
  return is_function_compiler_available;
#endif
}

void set_function_compiler_available(bool is_available) {
  is_function_compiler_available = is_available;
}

const char* name_for_architecture(CompiledFunctionCode::Architecture arch) {
  switch (arch) {
    case CompiledFunctionCode::Architecture::POWERPC:
      return "PowerPC";
    case CompiledFunctionCode::Architecture::X86:
      return "x86";
    default:
      throw logic_error("invalid architecture");
  }
}

template <typename FooterT>
string CompiledFunctionCode::generate_client_command_t(
    const unordered_map<string, uint32_t>& label_writes,
    const string& suffix,
    uint32_t override_relocations_offset) const {
  FooterT footer;
  footer.num_relocations = this->relocation_deltas.size();
  footer.unused1.clear(0);
  footer.entrypoint_addr_offset = this->entrypoint_offset_offset;
  footer.unused2.clear(0);

  StringWriter w;
  if (!label_writes.empty()) {
    string modified_code = this->code;
    for (const auto& it : label_writes) {
      size_t offset = this->label_offsets.at(it.first);
      if (offset > modified_code.size() - 4) {
        throw runtime_error("label out of range");
      }
      *reinterpret_cast<be_uint32_t*>(modified_code.data() + offset) = it.second;
    }
    w.write(modified_code);
  } else {
    w.write(this->code);
  }
  w.write(suffix);
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
      w.put<typename FooterT::U16T>(delta);
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
    const string& suffix,
    uint32_t override_relocations_offset) const {
  if (this->arch == Architecture::POWERPC) {
    return this->generate_client_command_t<S_ExecuteCode_Footer_GC_B2>(
        label_writes, suffix, override_relocations_offset);
  } else if ((this->arch == Architecture::X86) || (this->arch == Architecture::SH4)) {
    return this->generate_client_command_t<S_ExecuteCode_Footer_DC_PC_XB_BB_B2>(
        label_writes, suffix, override_relocations_offset);
  } else {
    throw logic_error("invalid architecture");
  }
}

bool CompiledFunctionCode::is_big_endian() const {
  return this->arch == Architecture::POWERPC;
}

shared_ptr<CompiledFunctionCode> compile_function_code(
    CompiledFunctionCode::Architecture arch,
    const string& directory,
    const string& name,
    const string& text) {
#ifndef HAVE_RESOURCE_FILE
  (void)arch;
  (void)directory;
  (void)name;
  (void)text;
  throw runtime_error("function compiler is not available");

#else
  shared_ptr<CompiledFunctionCode> ret(new CompiledFunctionCode());
  ret->arch = arch;
  ret->name = name;
  ret->index = 0;
  ret->hide_from_patches_menu = false;

  if (arch == CompiledFunctionCode::Architecture::POWERPC) {
    auto assembled = PPC32Emulator::assemble(text, {directory});
    ret->code = std::move(assembled.code);
    ret->label_offsets = std::move(assembled.label_offsets);
  } else if (arch == CompiledFunctionCode::Architecture::X86) {
    throw runtime_error("x86 assembler is not implemented");
  }

  set<uint32_t> reloc_indexes;
  for (const auto& it : ret->label_offsets) {
    if (starts_with(it.first, "reloc")) {
      reloc_indexes.emplace(it.second / 4);
    } else if (starts_with(it.first, "newserv_index_")) {
      ret->index = stoul(it.first.substr(14), nullptr, 16);
    } else if (it.first == "hide_from_patches_menu") {
      ret->hide_from_patches_menu = true;
    }
  }

  try {
    ret->entrypoint_offset_offset = ret->label_offsets.at("entry_ptr");
  } catch (const out_of_range&) {
    throw runtime_error("code does not contain entry_ptr label");
  }

  uint32_t prev_index = 0;
  for (const auto& it : reloc_indexes) {
    uint32_t delta = it - prev_index;
    if (delta > 0xFFFF) {
      throw runtime_error("relocation delta too far away");
    }
    ret->relocation_deltas.emplace_back(delta);
    prev_index = it;
  }

  return ret;
#endif
}

FunctionCodeIndex::FunctionCodeIndex(const string& directory) {
  if (!function_compiler_available()) {
    function_compiler_log.info("Function compiler is not available");
    return;
  }

  uint32_t next_menu_item_id = 0;
  for (const auto& filename : list_directory(directory)) {
    if (!ends_with(filename, ".s") || ends_with(filename, ".inc.s")) {
      continue;
    }
    bool is_patch = ends_with(filename, ".patch.s");
    string name = filename.substr(0, filename.size() - (is_patch ? 8 : 2));

    // Check for specific_version token
    uint32_t specific_version = 0;
    string patch_name = name;
    if (is_patch &&
        (filename.size() >= 13) &&
        (filename[filename.size() - 13] == '.') &&
        isdigit(filename[filename.size() - 12]) &&
        (filename[filename.size() - 11] == 'O' || filename[filename.size() - 11] == 'S') &&
        (filename[filename.size() - 10] == 'E' || filename[filename.size() - 10] == 'J' || filename[filename.size() - 10] == 'P') &&
        isdigit(filename[filename.size() - 9])) {
      specific_version = 0x33000000 | (filename[filename.size() - 11] << 16) | (filename[filename.size() - 10] << 8) | filename[filename.size() - 9];
      patch_name = filename.substr(0, filename.size() - 13);
    }

    try {
      string path = directory + "/" + filename;
      string text = load_file(path);
      auto code = compile_function_code(
          CompiledFunctionCode::Architecture::POWERPC, directory, name, text);
      if (code->index != 0) {
        if (!this->index_to_function.emplace(code->index, code).second) {
          throw runtime_error(string_printf(
              "duplicate function index: %08" PRIX32, code->index));
        }
      }
      code->specific_version = specific_version;
      code->patch_name = patch_name;
      this->name_to_function.emplace(name, code);
      if (is_patch) {
        code->menu_item_id = next_menu_item_id++;
        this->menu_item_id_and_specific_version_to_patch_function.emplace(
            static_cast<uint64_t>(code->menu_item_id) << 32 | specific_version, code);
        this->name_and_specific_version_to_patch_function.emplace(
            string_printf("%s-%08" PRIX32, patch_name.c_str(), specific_version), code);
      }

      string index_prefix = code->index ? string_printf("%02X => ", code->index) : "";
      string patch_prefix = is_patch ? string_printf("[%08" PRIX32 "/%08" PRIX32 "] ", code->menu_item_id, code->specific_version) : "";
      function_compiler_log.info("Compiled function %s%s%s (%s)",
          index_prefix.c_str(), patch_prefix.c_str(), name.c_str(), name_for_architecture(code->arch));

    } catch (const exception& e) {
      function_compiler_log.warning("Failed to compile function %s: %s", name.c_str(), e.what());
    }
  }
}

shared_ptr<const Menu> FunctionCodeIndex::patch_menu(uint32_t specific_version) const {
  auto suffix = string_printf("-%08" PRIX32, specific_version);

  shared_ptr<Menu> ret(new Menu(MenuID::PATCHES, u"Patches"));
  ret->items.emplace_back(PatchesMenuItemID::GO_BACK, u"Go back", u"Return to the\nmain menu", 0);
  for (const auto& it : this->name_and_specific_version_to_patch_function) {
    const auto& fn = it.second;
    if (!fn->hide_from_patches_menu && ends_with(it.first, suffix)) {
      ret->items.emplace_back(fn->menu_item_id, decode_sjis(fn->patch_name), u"",
          MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL);
    }
  }
  return ret;
}

bool FunctionCodeIndex::patch_menu_empty(uint32_t specific_version) const {
  for (const auto& it : this->menu_item_id_and_specific_version_to_patch_function) {
    if ((it.first & 0xFF000000) == (specific_version & 0xFF000000)) {
      return false;
    }
  }
  return true;
}

DOLFileIndex::DOLFileIndex(const string& directory, bool compress)
    : files_compressed(compress) {
  if (!function_compiler_available()) {
    function_compiler_log.info("Function compiler is not available");
    return;
  }
  if (!isdir(directory)) {
    function_compiler_log.info("DOL file directory is missing");
    return;
  }

  shared_ptr<Menu> menu(new Menu(MenuID::PROGRAMS, u"Programs"));
  this->menu = menu;
  menu->items.emplace_back(ProgramsMenuItemID::GO_BACK, u"Go back", u"Return to the\nmain menu", 0);

  uint32_t next_menu_item_id = 0;
  for (const auto& filename : list_directory(directory)) {
    if (!ends_with(filename, ".dol")) {
      continue;
    }
    string name = filename.substr(0, filename.size() - 4);

    try {
      shared_ptr<DOLFile> dol(new DOLFile());
      dol->menu_item_id = next_menu_item_id++;
      dol->name = name;

      string path = directory + "/" + filename;
      string file_data = load_file(path);

      string description;
      if (this->files_compressed) {
        uint64_t start = now();
        string compressed = prs_compress(file_data);
        StringWriter w;
        if (compressed.size() >= file_data.size()) {
          w.put_u32b(0);
          w.put_u32b(file_data.size());
          w.write(file_data);
        } else {
          w.put_u32b(compressed.size());
          w.put_u32b(file_data.size());
          w.write(compressed);
        }
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());
        uint64_t diff = now() - start;

        string orig_size_str = format_size(file_data.size());
        string compressed_size_str = format_size(dol->data.size());
        string time_str = format_duration(diff);

        if (compressed.size() >= file_data.size()) {
          function_compiler_log.info("Loaded and compressed DOL file %s (%s -> %s, %s) (inefficient compression; using uncompressed version)",
              dol->name.c_str(), orig_size_str.c_str(), compressed_size_str.c_str(), time_str.c_str());
          description = string_printf("$C6%s$C7\n%s", dol->name.c_str(), orig_size_str.c_str());
        } else {
          function_compiler_log.info("Loaded and compressed DOL file %s (%s -> %s, %s)",
              dol->name.c_str(), orig_size_str.c_str(), compressed_size_str.c_str(), time_str.c_str());
          description = string_printf("$C6%s$C7\n%s\n%s (orig)", dol->name.c_str(), compressed_size_str.c_str(), orig_size_str.c_str());
        }

      } else {
        StringWriter w;
        w.put_u32b(0);
        w.put_u32b(file_data.size());
        w.write(file_data);
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());

        string orig_size_str = format_size(dol->data.size());
        function_compiler_log.info("Loaded DOL file %s (%s)", filename.c_str(), orig_size_str.c_str());

        description = string_printf("$C6%s$C7\n%s", dol->name.c_str(), orig_size_str.c_str());
      }

      this->name_to_file.emplace(dol->name, dol);
      this->item_id_to_file.emplace_back(dol);

      menu->items.emplace_back(dol->menu_item_id, decode_sjis(dol->name),
          decode_sjis(description), MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL);

    } catch (const exception& e) {
      function_compiler_log.warning("Failed to load DOL file %s: %s", filename.c_str(), e.what());
    }
  }
}
