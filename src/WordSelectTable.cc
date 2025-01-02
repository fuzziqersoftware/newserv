#include "WordSelectTable.hh"

#include <inttypes.h>

#include <string>
#include <vector>

#include "Compression.hh"

using namespace std;

template <typename RetT, typename ReadT>
static vector<RetT> read_direct_table(const phosg::StringReader& base_r, size_t offset, size_t count) {
  vector<RetT> ret;
  auto entries_r = base_r.sub(offset, count * sizeof(ReadT));
  while (!entries_r.eof()) {
    ret.emplace_back(entries_r.get<ReadT>());
  }
  return ret;
}

template <typename RetT, typename ReadT, typename OffsetT>
static vector<vector<RetT>> read_indirect_table(const phosg::StringReader& base_r, size_t offset, size_t count) {
  vector<vector<RetT>> ret;
  auto pointers_r = base_r.sub(offset, sizeof(OffsetT) * 2 * count);
  while (!pointers_r.eof()) {
    uint32_t sub_offset = pointers_r.get<OffsetT>();
    uint32_t sub_count = pointers_r.get<OffsetT>();
    ret.emplace_back(read_direct_table<RetT, ReadT>(base_r, sub_offset, sub_count));
  }
  return ret;
}

template <bool BE>
struct NonWindowsRootT {
  U32T<BE> strings_table;
  U32T<BE> table1;
  U32T<BE> table2;
  U32T<BE> token_id_to_string_id_table;
  U32T<BE> table4;
  U32T<BE> article_types_table;
  U32T<BE> table6;
} __packed__;

using NonWindowsRoot = NonWindowsRootT<false>;
using NonWindowsRootBE = NonWindowsRootT<true>;
check_struct_size(NonWindowsRoot, 0x1C);
check_struct_size(NonWindowsRootBE, 0x1C);

struct PCV2Root {
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint32_t table1;
  le_uint32_t table2;
  le_uint32_t token_id_to_string_id_table;
  le_uint32_t table4;
  le_uint32_t article_types_table;
  le_uint32_t table6;
} __packed_ws__(PCV2Root, 0x20);

struct BBRoot {
  le_uint32_t table1;
  le_uint32_t table2;
  le_uint32_t token_id_to_string_id_table;
  le_uint32_t table4;
  le_uint32_t article_types_table;
  le_uint32_t table6;
} __packed_ws__(BBRoot, 0x18);

template <bool BE, size_t StringTableCount, size_t TokenCount>
void WordSelectSet::parse_non_windows_t(const std::string& data, bool use_sjis) {
  phosg::StringReader r(data);
  const auto& footer = r.pget<RELFileFooterT<BE>>(r.size() - sizeof(RELFileFooterT<BE>));
  const auto& root = r.pget<NonWindowsRootT<BE>>(footer.root_offset);

  {
    auto string_offset_r = r.sub(root.strings_table, sizeof(U32T<BE>) * StringTableCount);
    while (!string_offset_r.eof()) {
      string raw_s = r.pget_cstr(string_offset_r.template get<U32T<BE>>());
      this->strings.emplace_back(use_sjis ? tt_sega_sjis_to_utf8(raw_s) : tt_8859_to_utf8(raw_s));
    }
  }

  // this->table1 = read_indirect_table<uint16_t, U16T<BE>, U32T<BE>>(r, root.table1, Table1Count);
  // this->table2 = read_indirect_table<uint16_t, U16T<BE>, U32T<BE>>(r, root.table2, Table2Count);
  this->token_id_to_string_id = read_direct_table<size_t, U16T<BE>>(r, root.token_id_to_string_id_table, TokenCount);
  // this->table4 = read_indirect_table<uint16_t, U16T<BE>, U32T<BE>>(r, root.table4, Table4Count);
  // this->article_types = read_direct_table<uint8_t, uint8_t>(r, root.article_types_table, ArticleTypesCount);
  // this->table6 = read_indirect_table<uint16_t, U16T<BE>, U32T<BE>>(r, root.table6, Table6Count);
}

template <typename RootT, size_t TokenCount>
void WordSelectSet::parse_windows_t(const std::string& data, const std::vector<std::string>* unitxt_collection) {
  if (!unitxt_collection) {
    throw runtime_error("a unitxt collection is required");
  }

  phosg::StringReader r(data);
  const auto& footer = r.pget<RELFileFooter>(r.size() - sizeof(RELFileFooter));
  const auto& root = r.pget<RootT>(footer.root_offset);
  this->strings = *unitxt_collection;
  // this->table1 = read_indirect_table<uint16_t, le_uint16_t, le_uint32_t>(r, root.table1, Table1Count);
  // this->table2 = read_indirect_table<uint16_t, le_uint16_t, le_uint32_t>(r, root.table2, Table2Count);
  this->token_id_to_string_id = read_direct_table<size_t, le_uint16_t>(r, root.token_id_to_string_id_table, TokenCount);
  // this->table4 = read_indirect_table<uint16_t, le_uint16_t, le_uint32_t>(r, root.table4, Table4Count);
  // this->article_types = read_direct_table<uint8_t, uint8_t>(r, root.article_types_table, ArticleTypesCount);
  // this->table6 = read_indirect_table<uint16_t, le_uint16_t, le_uint32_t>(r, root.table6, Table6Count);
}

WordSelectSet::WordSelectSet(const string& data, Version version, const vector<string>* unitxt_collection, bool use_sjis) {
  switch (version) {
    case Version::DC_NTE: {
      if (data.size() < 4) {
        throw runtime_error("data is too small");
      }
      string decrypted = data.substr(0, data.size() - 4);
      uint32_t seed = *reinterpret_cast<const le_uint32_t*>(data.data() + data.size() - 4);
      PSOV2Encryption crypt(seed);
      crypt.decrypt(decrypted);
      this->parse_non_windows_t<false, 0x469, 0x466>(decrypted, use_sjis);
      break;
    }
    case Version::DC_11_2000:
      this->parse_non_windows_t<false, 0x45E, 0x44B>(decrypt_and_decompress_pr2_data<false>(data), use_sjis);
      break;
    case Version::DC_V1:
    case Version::DC_V2:
      this->parse_non_windows_t<false, 0x467, 0x457>(decrypt_and_decompress_pr2_data<false>(data), use_sjis);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      this->parse_windows_t<PCV2Root, 0x645>(decrypt_and_decompress_pr2_data<false>(data), unitxt_collection);
      break;
    case Version::GC_NTE:
      this->parse_non_windows_t<true, 0x63F, 0x693>(decrypt_and_decompress_pr2_data<true>(data), use_sjis);
      break;
    case Version::GC_EP3_NTE:
    case Version::GC_V3:
      this->parse_non_windows_t<true, 0x67C, 0x68C>(decrypt_and_decompress_pr2_data<true>(data), use_sjis);
      break;
    case Version::GC_EP3:
      this->parse_non_windows_t<true, 0x804, 0x68C>(decrypt_and_decompress_pr2_data<true>(data), use_sjis);
      break;
    case Version::XB_V3:
      this->parse_non_windows_t<false, 0x67B, 0x68C>(decrypt_and_decompress_pr2_data<false>(data), use_sjis);
      break;
    case Version::BB_V4:
      this->parse_windows_t<BBRoot, 0x68C>(decrypt_and_decompress_pr2_data<false>(data), unitxt_collection);
      break;
    default:
      throw runtime_error("unsupported word select data version");
  }
}

const string& WordSelectSet::string_for_token(uint16_t token_id) const {
  return this->strings.at(this->token_id_to_string_id.at(token_id));
}

void WordSelectSet::print(FILE* stream) const {
  fprintf(stream, "strings:\n");
  for (size_t z = 0; z < this->strings.size(); z++) {
    auto escaped = phosg::escape_controls_utf8(this->strings[z]);
    fprintf(stream, "  [%04zX] \"%s\"\n", z, escaped.c_str());
  }
  fprintf(stream, "token_id_to_string_id:\n");
  for (size_t z = 0; z < this->token_id_to_string_id.size(); z++) {
    auto escaped = phosg::escape_controls_utf8(this->string_for_token(z));
    fprintf(stream, "  [%04zX] %04zX \"%s\"\n", z, this->token_id_to_string_id[z], escaped.c_str());
  }
}

WordSelectTable::WordSelectTable(
    const WordSelectSet& dc_nte_ws,
    const WordSelectSet& dc_112000_ws,
    const WordSelectSet& dc_v1_ws,
    const WordSelectSet& dc_v2_ws,
    const WordSelectSet& pc_nte_ws,
    const WordSelectSet& pc_v2_ws,
    const WordSelectSet& gc_nte_ws,
    const WordSelectSet& gc_v3_ws,
    const WordSelectSet& gc_ep3_nte_ws,
    const WordSelectSet& gc_ep3_ws,
    const WordSelectSet& xb_v3_ws,
    const WordSelectSet& bb_v4_ws,
    const vector<vector<string>>& name_alias_lists) {

  unordered_map<string, string> name_to_canonical_name;
  for (const auto& alias_list : name_alias_lists) {
    if (alias_list.size() < 2) {
      continue;
    }
    auto it = alias_list.begin();
    auto canonical_name = *it;
    for (it++; it != alias_list.end(); it++) {
      name_to_canonical_name.emplace(*it, canonical_name);
    }
  }

  vector<shared_ptr<Token>> dynamic_tokens;
  {
    for (size_t z = 0; z < 12; z++) {
      auto& token = dynamic_tokens.emplace_back(make_shared<Token>());
      token->canonical_name = phosg::string_printf("__PLAYER_%zu_NAME__", z);
      this->name_to_token.emplace(token->canonical_name, token);
    }
    auto& token = dynamic_tokens.emplace_back(make_shared<Token>());
    token->canonical_name = "__BLANK__";
    this->name_to_token.emplace(token->canonical_name, token);
  }

  static_assert(NUM_NON_PATCH_VERSIONS == 12, "Don\'t forget to update the WordSelectTable constructor");
  array<const WordSelectSet*, NUM_NON_PATCH_VERSIONS> ws_sets = {
      &dc_nte_ws, &dc_112000_ws, &dc_v1_ws, &dc_v2_ws,
      &pc_nte_ws, &pc_v2_ws, &gc_nte_ws, &gc_v3_ws,
      &gc_ep3_nte_ws, &gc_ep3_ws, &xb_v3_ws, &bb_v4_ws};

  for (size_t s_version = 0; s_version < ws_sets.size(); s_version++) {
    Version version = static_cast<Version>(static_cast<size_t>(Version::DC_NTE) + s_version);
    const auto& ws_set = *ws_sets[s_version];
    auto& index = this->tokens_by_version.at(s_version);

    index.reserve(ws_set.num_tokens());
    for (size_t token_id = 0; token_id < ws_set.num_tokens(); token_id++) {
      const string& str = ws_set.string_for_token(token_id);

      string canonical_name;
      try {
        canonical_name = name_to_canonical_name.at(str);
      } catch (const out_of_range&) {
        canonical_name = str;
      }

      auto token_it = this->name_to_token.find(canonical_name);
      if (token_it == this->name_to_token.end()) {
        token_it = this->name_to_token.emplace(canonical_name, make_shared<Token>()).first;
        token_it->second->canonical_name = std::move(canonical_name);
      }
      token_it->second->slot_for_version(version) = token_id;
      index.emplace_back(token_it->second);
    }

    size_t dynamic_token_base_id = ws_set.num_tokens();
    for (size_t z = 0; z < dynamic_tokens.size(); z++) {
      auto& token = dynamic_tokens[z];
      token->slot_for_version(version) = dynamic_token_base_id + z;
      index.emplace_back(token);
    }
  }
}

void WordSelectTable::print(FILE* stream) const {
  fprintf(stream, "DCN  DC11 DCv1 DCv2 PCN  PCv2 GCN  GCv3 Ep3N Ep3  XBv3 BBv4 CANONICAL-NAME\n");
  for (const auto& it : this->name_to_token) {
    const auto& token = it.second;
    for (size_t z = 0; z < 12; z++) {
      if (token->values_by_version[z] == 0xFFFF) {
        fprintf(stream, "     ");
      } else {
        fprintf(stream, "%04hX ", token->values_by_version[z]);
      }
    }
    string serialized = phosg::JSON(token->canonical_name).serialize(phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY);
    fprintf(stream, "%s\n", serialized.c_str());
  }
}

void WordSelectTable::print_index(FILE* stream, Version v) const {
  fprintf(stream, "        DCN  DC11 DCv1 DCv2 PCN  PCv2 GCN  GCv3 Ep3N Ep3  XBv3 BBv4 CANONICAL-NAME\n");
  const auto& index = this->tokens_for_version(v);
  for (size_t token_id = 0; token_id < index.size(); token_id++) {
    const auto& token = index[token_id];
    fprintf(stream, "%04zX => ", token_id);
    for (size_t z = 0; z < 12; z++) {
      if (token->values_by_version[z] == 0xFFFF) {
        fprintf(stream, "     ");
      } else {
        fprintf(stream, "%04hX ", token->values_by_version[z]);
      }
    }
    string serialized = phosg::JSON(token->canonical_name).serialize(phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY);
    fprintf(stream, "%s\n", serialized.c_str());
  }
}

void WordSelectTable::validate(const WordSelectMessage& msg, Version version) const {
  const auto& index = this->tokens_for_version(version);

  for (size_t z = 0; z < msg.tokens.size(); z++) {
    if (msg.tokens[z] == 0xFFFF) {
      continue;
    }
    const auto& token = index.at(msg.tokens[z]);
    if (!token) {
      throw runtime_error(phosg::string_printf("token %04hX does not exist in the index", msg.tokens[z].load()));
    }
  }
}

WordSelectMessage WordSelectTable::translate(
    const WordSelectMessage& msg,
    Version from_version,
    Version to_version) const {
  const auto& index = this->tokens_for_version(from_version);

  WordSelectMessage ret;
  for (size_t z = 0; z < ret.tokens.size(); z++) {
    if (msg.tokens[z] == 0xFFFF) {
      ret.tokens[z] = 0xFFFF;
    } else {
      const auto& token = index.at(msg.tokens[z]);
      if (!token) {
        throw runtime_error(phosg::string_printf("token %04hX does not exist in the index", msg.tokens[z].load()));
      }
      ret.tokens[z] = token->slot_for_version(to_version);
      if (ret.tokens[z] == 0xFFFF) {
        throw runtime_error(phosg::string_printf("token %04hX has no translation", msg.tokens[z].load()));
      }
    }
  }
  ret.num_tokens = msg.num_tokens;
  ret.target_type = msg.target_type;
  ret.numeric_parameter = msg.numeric_parameter;
  ret.unknown_a4 = msg.unknown_a4;
  return ret;
}

WordSelectTable::Token::Token() {
  this->values_by_version.fill(0xFFFF);
}
