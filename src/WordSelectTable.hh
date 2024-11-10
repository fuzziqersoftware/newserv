#pragma once

#include <inttypes.h>

#include <string>
#include <vector>

#include "CommandFormats.hh"
#include "QuestScript.hh"

class WordSelectSet {
public:
  WordSelectSet(const std::string& data, Version version, const std::vector<std::string>* unitxt_collection, bool use_sjis);
  ~WordSelectSet() = default;

  inline size_t num_strings() const {
    return this->strings.size();
  }
  inline size_t num_tokens() const {
    return this->token_id_to_string_id.size();
  }

  const std::string& string_for_token(uint16_t token_id) const;

  void print(FILE* stream) const;

protected:
  template <bool BE, size_t StringTableCount, size_t TokenCount>
  void parse_non_windows_t(const std::string& data, bool use_sjis);
  template <typename RootT, size_t TokenCount>
  void parse_windows_t(const std::string& data, const std::vector<std::string>* unitxt_collection);

  std::vector<std::string> strings;
  std::vector<size_t> token_id_to_string_id;
  // Note: PC NTE and PC have exactly the same parameters
  //                                               => DC NTE   DC112000 DCv1     DCv2     PCNTE/PC GC NTE   GC       XB       Ep3 NTE  Ep3 USA  BB
  // root:                                         => 000074DC 000072A4 0000755C 0000755C 00002B50 0000AB04 0000BCAC 0000B620 0000B648 0000B914 0000B5FC
  //   u32 ???:                                    =>                                     00002A9C
  //     TODO
  //   u32 ???:                                    =>                                     00002B14
  //     TODO
  //   u32 strings_table:                          => 00006338 0000612C 000063C0 000063C0 (unitxt) 00009208 00009C9C 00009C34 00009C5C 00009904 (unitxt)
  //     u32 string_offset[COUNT]:                 =>      469      45E      467      467 (unitxt)      63F      804      67B      67C      804 (unitxt)
  //       char string[...\0]
  //   u32 table1:                                 => 00000B90 00000B54 00000D3C 00000D3C 00001018 0000100C 000012F0 000012F0 000012F0 000011D0 000012F0
  //     {u32 offset, u32 count}[COUNT]:           =>       94      122       93       93       F9       F9      126      126      126      17F      126
  //       u16[count]
  //   u32 table2:                                 => 00001178 00001108 00001300 00001300 000019D8 000019CC 00001EE8 00001EE8 00001EE8 00001DC8 00001EE8
  //     {u32 offset, u32 count}[COUNT]:           =>        7        7        7        7        7        7       13       13       13       13       13
  //       u16[count]
  //   u32 token_id_to_string_id_table             => 000011B0 00001140 00001338 00001338 00001A10 00001A04 00001F80 00001F80 00001F80 00001E60 00001F80
  //     u16[COUNT] string_id_for_token_id         =>      466      44B      457      457      645      693      68C      68C      68C      68C      68C
  //   u32 table4:                                 => 00001A5C 00001B08 00001D1C 00001D1C 000027D0 000027C4 00002DCC 00002DCC 00002DCC 00002CAC 00002DCC
  //     (non-NTE) {u32 offset, u32 count}[COUNT]: =>                 2        2        2        2        2        2        2        2        2        2
  //       u16[count]
  //     (NTE) u16[COUNT]                          =>       E1
  //   u32 article_types_table:                    => 00001C1E 00001B18 00001D2C 00001D2C 000027E0 000027D4 00002DDC 00002DDC 00002DDC 00002CBC 00002DDC
  //     u8[COUNT] article_types                   =>      1C8      166      166      166      266      266      28A      28A      28A      28A      266
  //   u32 table6:                                 => 00001E28 00001CBC 00001ED0 00001ED0 00002A84 00002A78 000030A4 000030A4 000030A4 00002F84 00003080
  //     {u32 offset, u32 count}[3]:
  //       u16[count]
};

class WordSelectTable {
public:
  WordSelectTable(
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
      const std::vector<std::vector<std::string>>& name_alias_lists);

  void print(FILE* stream) const;
  void print_index(FILE* stream, Version v) const;

  void validate(const WordSelectMessage& msg, Version version) const; // Throws runtime_error if invalid
  WordSelectMessage translate(const WordSelectMessage& msg, Version from_version, Version to_version) const;

private:
  struct Token {
    std::array<uint16_t, 12> values_by_version;
    std::string canonical_name;

    Token();

    inline uint16_t& slot_for_version(Version version) {
      return this->values_by_version.at(static_cast<size_t>(version) - static_cast<size_t>(Version::DC_NTE));
    }
    inline uint16_t slot_for_version(Version version) const {
      return this->values_by_version.at(static_cast<size_t>(version) - static_cast<size_t>(Version::DC_NTE));
    }
  };

  std::map<std::string, std::shared_ptr<Token>> name_to_token;
  std::array<std::vector<std::shared_ptr<Token>>, 12> tokens_by_version;

  inline const std::vector<std::shared_ptr<Token>>& tokens_for_version(Version version) const {
    return this->tokens_by_version.at(static_cast<size_t>(version) - static_cast<size_t>(Version::DC_NTE));
  }
};
