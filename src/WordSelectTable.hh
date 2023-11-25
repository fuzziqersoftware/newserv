#pragma once

#include <inttypes.h>

#include <phosg/JSON.hh>
#include <string>
#include <vector>

#include "CommandFormats.hh"
#include "QuestScript.hh"

class WordSelectTable {
public:
  explicit WordSelectTable(const JSON& json);

  WordSelectMessage translate(
      const WordSelectMessage& msg,
      Version from_version,
      Version to_version) const;

private:
  struct Token {
    uint16_t dc_value;
    uint16_t pc_value;
    uint16_t gc_value;
    uint16_t ep3_value;
    uint16_t bb_value;

    uint16_t value_for_version(Version version) const;
  };
  std::vector<size_t> dc_index;
  std::vector<size_t> pc_index;
  std::vector<size_t> gc_index;
  std::vector<size_t> ep3_index;
  std::vector<size_t> bb_index;
  std::vector<Token> tokens;
};
