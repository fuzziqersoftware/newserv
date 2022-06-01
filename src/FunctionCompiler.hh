#pragma once

#include <inttypes.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>



bool function_compiler_available();



// TODO: Support x86 function calls in the future. Currently we only support
// PPC32 because I haven't written an appropriate x86 assembler yet.

struct CompiledFunctionCode {
  std::string code;
  std::vector<uint16_t> relocation_deltas;
  std::unordered_map<std::string, uint32_t> label_offsets;
  uint32_t entrypoint_offset_offset;
  uint8_t index; // 00-FE (FF = index not specified)

  std::string generate_client_command(
      const std::unordered_map<std::string, uint32_t>& label_writes = {},
      const std::string& suffix = "") const;
};

std::shared_ptr<CompiledFunctionCode> compile_function_code(const std::string& text);



struct FunctionCodeIndex {
  FunctionCodeIndex(const std::string& directory);

  std::unordered_map<std::string, std::shared_ptr<CompiledFunctionCode>> name_to_function;
  std::vector<std::shared_ptr<CompiledFunctionCode>> index_to_function;
};
