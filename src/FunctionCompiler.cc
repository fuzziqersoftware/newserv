#include "FunctionCompiler.hh"

#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <phosg/Filesystem.hh>

#ifdef HAVE_RESOURCE_FILE
#include <resource_file/Emulators/PPC32Emulator.hh>
#endif

#include "CommandFormats.hh"

using namespace std;



bool function_compiler_available() {
#ifndef HAVE_RESOURCE_FILE
  return false;
#else
  return true;
#endif
}



string CompiledFunctionCode::generate_client_command(
      const unordered_map<string, uint32_t>& label_writes,
      const string& suffix) const {
  S_ExecuteCode_Footer_GC_B2 footer;
  footer.num_relocations = this->relocation_deltas.size();
  footer.unused1.clear();
  footer.entrypoint_addr_offset = this->entrypoint_offset_offset;
  footer.unused2.clear();

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
  for (uint16_t delta : this->relocation_deltas) {
    w.put_u16b(delta);
  }
  if (this->relocation_deltas.size() & 1) {
    w.put_u16(0);
  }

  w.put(footer);
  return move(w.str());
}

shared_ptr<CompiledFunctionCode> compile_function_code(
    const string& directory, const string& name, const string& text) {
#ifndef HAVE_RESOURCE_FILE
  (void)directory;
  (void)name;
  (void)text;
  throw runtime_error("PowerPC assembler is not available");

#else
  std::unordered_set<string> get_include_stack; // For mutual recursion detection
  function<string(const string&)> get_include = [&](const string& name) -> string {
    if (!get_include_stack.emplace(name).second) {
      throw runtime_error("mutual recursion between includes");
    }

    string filename = directory + "/" + name + ".inc.s";
    if (isfile(filename)) {
      return PPC32Emulator::assemble(load_file(filename), get_include).code;
    }
    filename = directory + "/" + name + ".inc.bin";
    if (isfile(filename)) {
      return load_file(filename);
    }
    throw runtime_error("data not found for include " + name);
  };

  shared_ptr<CompiledFunctionCode> ret(new CompiledFunctionCode());
  ret->name = name;
  ret->index = 0;

  auto assembled = PPC32Emulator::assemble(text, get_include);
  ret->code = move(assembled.code);
  ret->label_offsets = move(assembled.label_offsets);

  set<uint32_t> reloc_indexes;
  for (const auto& it : ret->label_offsets) {
    if (starts_with(it.first, "reloc")) {
      reloc_indexes.emplace(it.second / 4);
    } else if (starts_with(it.first, "newserv_index_")) {
      ret->index = stoul(it.first.substr(14), nullptr, 16);
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
    log(INFO, "Function compiler is not available");
    return;
  }

  uint32_t next_menu_item_id = 0;
  for (const auto& filename : list_directory(directory)) {
    if (!ends_with(filename, ".s") || ends_with(filename, ".inc.s")) {
      continue;
    }
    bool is_patch = ends_with(filename, ".patch.s");
    string name = filename.substr(0, filename.size() - (is_patch ? 8 : 2));

    try {
      string path = directory + "/" + filename;
      string text = load_file(path);
      auto code = compile_function_code(directory, name, text);
      if (code->index != 0) {
        if (!this->index_to_function.emplace(code->index, code).second) {
          throw runtime_error(string_printf(
              "duplicate function index: %08" PRIX32, code->index));
        }
      }
      this->name_to_function.emplace(name, code);
      if (is_patch) {
        this->menu_item_id_to_patch_function.emplace(next_menu_item_id++, code);
        this->name_to_patch_function.emplace(name, code);
      }
      if (code->index) {
        log(INFO, "Compiled function %02X => %s", code->index, name.c_str());
      } else {
        log(INFO, "Compiled function %s", name.c_str());
      }

    } catch (const exception& e) {
      log(WARNING, "Failed to compile function %s: %s", name.c_str(), e.what());
    }
  }
}

vector<MenuItem> FunctionCodeIndex::patch_menu() const {
  vector<MenuItem> ret;
  ret.emplace_back(PatchesMenuItemID::GO_BACK, u"Go back", u"", 0);
  for (const auto& it : this->name_to_patch_function) {
    const auto& fn = it.second;
    ret.emplace_back(fn->menu_item_id, decode_sjis(fn->name), u"",
        MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL);
  }
  return ret;
}



DOLFileIndex::DOLFileIndex(const string& directory) {
  if (!function_compiler_available()) {
    log(INFO, "Function compiler is not available");
    return;
  }
  if (!isdir(directory)) {
    log(INFO, "DOL file directory is missing");
    return;
  }

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
      dol->data = load_file(path);

      this->name_to_file.emplace(dol->name, dol);
      this->item_id_to_file.emplace_back(dol);
      log(INFO, "Loaded DOL file %s", filename.c_str());

    } catch (const exception& e) {
      log(WARNING, "Failed to load DOL file %s: %s", filename.c_str(), e.what());
    }
  }
}

vector<MenuItem> DOLFileIndex::menu() const {
  vector<MenuItem> ret;
  ret.emplace_back(ProgramsMenuItemID::GO_BACK, u"Go back", u"", 0);
  for (const auto& dol : this->item_id_to_file) {
    ret.emplace_back(dol->menu_item_id, decode_sjis(dol->name), u"",
        MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL);
  }
  return ret;
}
