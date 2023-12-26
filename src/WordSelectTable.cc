#include "WordSelectTable.hh"

#include <inttypes.h>

#include <string>
#include <vector>

using namespace std;

static void index_add(vector<size_t>& index, uint16_t position, size_t value) {
  if (position != 0xFFFF) {
    if (index.size() <= position) {
      index.resize(position + 1);
    }
    index[position] = value;
  }
}

WordSelectTable::WordSelectTable(const JSON& json) {
  this->tokens.reserve(json.size());
  for (const auto& item : json.as_list()) {
    JSON dc_value_json = item->at(0);
    JSON pc_value_json = item->at(1);
    JSON gc_value_json = item->at(2);
    JSON ep3_value_json = item->at(3);
    JSON bb_value_json = item->at(4);
    uint16_t dc_value = dc_value_json.is_null() ? 0xFFFF : dc_value_json.as_int();
    uint16_t pc_value = pc_value_json.is_null() ? 0xFFFF : pc_value_json.as_int();
    uint16_t gc_value = gc_value_json.is_null() ? 0xFFFF : gc_value_json.as_int();
    uint16_t ep3_value = ep3_value_json.is_null() ? 0xFFFF : ep3_value_json.as_int();
    uint16_t bb_value = bb_value_json.is_null() ? 0xFFFF : bb_value_json.as_int();
    this->tokens.emplace_back(Token{
        .dc_value = dc_value,
        .pc_value = pc_value,
        .gc_value = gc_value,
        .ep3_value = ep3_value,
        .bb_value = bb_value,
    });
    index_add(this->dc_index, dc_value, this->tokens.size() - 1);
    index_add(this->pc_index, pc_value, this->tokens.size() - 1);
    index_add(this->gc_index, gc_value, this->tokens.size() - 1);
    index_add(this->ep3_index, ep3_value, this->tokens.size() - 1);
    index_add(this->bb_index, bb_value, this->tokens.size() - 1);
  }
}

uint16_t WordSelectTable::Token::value_for_version(Version version) const {
  switch (version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
      return this->dc_value;
    case Version::PC_NTE:
    case Version::PC_V2:
      return this->pc_value;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::XB_V3:
      // TODO: Which index does GC_NTE use? Here we presume it's the same as GC,
      // but this may not be true
      return this->gc_value;
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      return this->ep3_value;
    case Version::BB_V4:
      return this->bb_value;
    default:
      throw logic_error("invalid word select version");
  }
}

WordSelectMessage WordSelectTable::translate(
    const WordSelectMessage& msg,
    Version from_version,
    Version to_version) const {
  const std::vector<size_t>* index;
  switch (from_version) {
    case Version::DC_NTE:
    case Version::DC_V1_11_2000_PROTOTYPE:
    case Version::DC_V1:
    case Version::DC_V2:
      index = &this->dc_index;
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      index = &this->pc_index;
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::XB_V3:
      // TODO: Which index does GC_NTE use? Here we presume it's the same as GC,
      // but this may not be true
      index = &this->gc_index;
      break;
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      index = &this->ep3_index;
      break;
    case Version::BB_V4:
      index = &this->bb_index;
      break;
    default:
      throw logic_error("invalid word select version");
  }

  WordSelectMessage ret;
  for (size_t z = 0; z < ret.tokens.size(); z++) {
    if (msg.tokens[z] == 0xFFFF) {
      ret.tokens[z] = 0xFFFF;
    } else {
      ret.tokens[z] = this->tokens.at(index->at(msg.tokens[z])).value_for_version(to_version);
      if (ret.tokens[z] == 0xFFFF) {
        throw runtime_error(string_printf("token %04hX has no translation", msg.tokens[z].load()));
      }
    }
  }
  ret.num_tokens = msg.num_tokens;
  ret.target_type = msg.target_type;
  ret.numeric_parameter = msg.numeric_parameter;
  ret.unknown_a4 = msg.unknown_a4;
  return ret;
}
