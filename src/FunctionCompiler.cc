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



std::string CompiledFunctionCode::generate_client_command(
      const std::unordered_map<std::string, uint32_t>& label_writes,
      const std::string& suffix) const {
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

shared_ptr<CompiledFunctionCode> compile_function_code(const std::string& text) {
#ifndef HAVE_RESOURCE_FILE
  (void)text;
  throw runtime_error("PowerPC assembler is not available");

#else
  auto assembled = PPC32Emulator::assemble(text);

  shared_ptr<CompiledFunctionCode> ret(new CompiledFunctionCode());
  ret->code = move(assembled.code);
  ret->label_offsets = move(assembled.label_offsets);
  ret->index = 0xFF;

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



FunctionCodeIndex::FunctionCodeIndex(const std::string& directory) {
  this->index_to_function.resize(0x100);

  if (!function_compiler_available()) {
    log(INFO, "Function compiler is not available");
    return;
  }

  for (const auto& filename : list_directory(directory)) {
    if (!ends_with(filename, ".s")) {
      continue;
    }
    string name = filename.substr(0, filename.size() - 2);

    try {
      string path = directory + "/" + filename;
      string text = load_file(path);
      auto code = compile_function_code(text);
      if (code->index < 0xFF) {
        this->index_to_function.at(code->index) = code;
      }
      this->name_to_function.emplace(name, code);
      log(WARNING, "Compiled function %s", name.c_str());

    } catch (const exception& e) {
      log(WARNING, "Failed to compile function %s: %s", name.c_str(), e.what());
    }
  }
}
