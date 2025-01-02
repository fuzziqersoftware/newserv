#include "TextIndex.hh"

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <set>
#include <stdexcept>

#include "CommonFileFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

TextSet::TextSet(const phosg::JSON& json) {
  for (const auto& coll_json : json.as_list()) {
    auto& collection = this->collections.emplace_back();
    for (const auto& s_json : coll_json->as_list()) {
      collection.emplace_back(s_json->as_string());
    }
  }
}

TextSet::TextSet(phosg::JSON&& json) {
  for (const auto& coll_json : json.as_list()) {
    auto& collection = this->collections.emplace_back();
    for (const auto& s_json : coll_json->as_list()) {
      collection.emplace_back(std::move(s_json->as_string()));
    }
  }
}

phosg::JSON TextSet::json() const {
  phosg::JSON j = phosg::JSON::list();
  for (const auto& collection : this->collections) {
    phosg::JSON& coll_j = j.emplace_back(phosg::JSON::list());
    for (const auto& s : collection) {
      coll_j.emplace_back(s);
    }
  }
  return j;
}

size_t TextSet::count(size_t collection_index) const {
  return this->collections.at(collection_index).size();
}

size_t TextSet::count() const {
  return this->collections.size();
}

const std::string& TextSet::get(size_t collection_index, size_t string_index) const {
  return this->get(collection_index).at(string_index);
}

const vector<string>& TextSet::get(size_t collection_index) const {
  return this->collections.at(collection_index);
}

void TextSet::set(size_t collection_index, size_t string_index, const std::string& data) {
  this->ensure_slot_exists(collection_index, string_index);
  this->collections[collection_index][string_index] = data;
}

void TextSet::set(size_t collection_index, size_t string_index, std::string&& data) {
  this->ensure_slot_exists(collection_index, string_index);
  this->collections[collection_index][string_index] = std::move(data);
}

void TextSet::set(size_t collection_index, const std::vector<std::string>& coll) {
  this->ensure_collection_exists(collection_index);
  this->collections[collection_index] = coll;
}

void TextSet::set(size_t collection_index, std::vector<std::string>&& coll) {
  this->ensure_collection_exists(collection_index);
  this->collections[collection_index] = std::move(coll);
}

void TextSet::truncate_collection(size_t collection_index, size_t new_entry_count) {
  if (collection_index >= this->collections.size()) {
    this->collections.resize(collection_index + 1);
  }
  this->collections[collection_index].resize(new_entry_count);
}

void TextSet::truncate(size_t new_collection_count) {
  this->collections.resize(new_collection_count);
}

void TextSet::ensure_slot_exists(size_t collection_index, size_t string_index) {
  this->ensure_collection_exists(collection_index);
  auto& coll = this->collections[collection_index];
  if (string_index >= coll.size()) {
    coll.resize(string_index + 1);
  }
}

void TextSet::ensure_collection_exists(size_t collection_index) {
  if (collection_index >= this->collections.size()) {
    this->collections.resize(collection_index + 1);
  }
}

UnicodeTextSet::UnicodeTextSet(const string& prs_data) {
  string data = prs_decompress(prs_data);
  phosg::StringReader r(data);

  uint32_t num_collections = r.get_u32l();
  deque<uint32_t> collection_sizes;
  while (collection_sizes.size() < num_collections) {
    collection_sizes.emplace_back(r.get_u32l());
  }

  this->collections.reserve(collection_sizes.size());
  while (!collection_sizes.empty()) {
    uint32_t num_strings = collection_sizes.front();
    collection_sizes.pop_front();

    auto& strings = this->collections.emplace_back();
    strings.reserve(num_strings);
    while (strings.size() < num_strings) {
      phosg::StringReader sub_r = r.sub(r.get_u32l());
      phosg::StringWriter w;
      for (uint16_t ch = sub_r.get_u16l(); ch != 0; ch = sub_r.get_u16l()) {
        w.put_u16l(ch);
      }
      strings.emplace_back(tt_utf16_to_utf8(w.str()));
    }
  }
}

string UnicodeTextSet::serialize() const {
  phosg::StringWriter header_w;
  phosg::StringWriter data_w;

  size_t total_num_strings = 0;
  header_w.put_u32l(this->collections.size());
  for (const auto& collection : this->collections) {
    header_w.put_u32l(collection.size());
    total_num_strings += collection.size();
  }

  unordered_map<string, uint32_t> encoded;

  size_t data_base_offset = (total_num_strings * 4) + header_w.size();
  for (const auto& collection : this->collections) {
    for (const auto& s : collection) {
      auto encoded_it = encoded.find(s);
      if (encoded_it == encoded.end()) {
        uint32_t offset = data_base_offset + data_w.size();
        encoded_it = encoded.emplace(s, offset).first;
        string s_utf16 = tt_utf8_to_utf16(s);
        data_w.write(s_utf16.data(), s_utf16.size());
        data_w.put_u16(0);
        while (data_w.size() & 3) {
          data_w.put_u8(0);
        }
      }
      header_w.put_u32l(encoded_it->second);
    }
  }

  header_w.write(data_w.str());
  return prs_compress_optimal(header_w.str());
}

BinaryTextSet::BinaryTextSet(const std::string& pr2_data, size_t collection_count, bool has_rel_footer, bool is_sjis) {
  auto pr2_decrypted = decrypt_pr2_data<false>(pr2_data);
  auto decompressed = prs_decompress(pr2_decrypted.compressed_data);
  phosg::StringReader r(decompressed);

  // Annoyingly, there doesn't appear to be any bounds-checking on the language
  // functions, so there are no counts of strings in each collection. We have to
  // figure out where each collection ends by collecting all the relevant
  // offsets in the file instead.
  ::set<uint32_t> used_offsets;
  size_t root_offset = has_rel_footer
      ? r.pget<RELFileFooter>(r.size() - 0x20).root_offset.load()
      : (r.size() - collection_count * sizeof(le_uint32_t));

  phosg::StringReader collection_offsets_r = r.sub(root_offset, collection_count * sizeof(le_uint32_t));
  while (!collection_offsets_r.eof()) {
    used_offsets.emplace(collection_offsets_r.get_u32l());
  }
  used_offsets.emplace(root_offset);

  auto& tt = is_sjis ? tt_sega_sjis_to_utf8 : tt_8859_to_utf8;

  collection_offsets_r.go(0);
  while (!collection_offsets_r.eof()) {
    auto& collection = this->collections.emplace_back();
    uint32_t first_string_offset_offset = collection_offsets_r.get_u32l();
    // TODO: Apparently the early formats do actually include keyboards, but
    // they're just in the middle of the collections list. Sigh...
    try {
      for (uint32_t string_offset_offset = first_string_offset_offset;
           (string_offset_offset == first_string_offset_offset) || !used_offsets.count(string_offset_offset);
           string_offset_offset += 4) {
        collection.emplace_back(tt(r.pget_cstr(r.pget_u32l(string_offset_offset))));
      }
    } catch (const out_of_range&) {
    }
  }
}

BinaryTextAndKeyboardsSet::BinaryTextAndKeyboardsSet(const string& pr2_data, bool big_endian, bool is_sjis) {
  if (big_endian) {
    this->parse_t<true>(pr2_data, is_sjis);
  } else {
    this->parse_t<false>(pr2_data, is_sjis);
  }
}

BinaryTextAndKeyboardsSet::BinaryTextAndKeyboardsSet(const phosg::JSON& json) {
  for (const auto& collection_json : json.at("collections").as_list()) {
    auto& collection = this->collections.emplace_back();
    for (const auto& string_json : collection_json->as_list()) {
      collection.emplace_back(string_json->as_string());
    }
  }

  for (const auto& keyboard_json : json.at("keyboards").as_list()) {
    auto& keyboard = this->keyboards.emplace_back(make_unique<Keyboard>());
    for (size_t y = 0; y < keyboard->size(); y++) {
      auto& row = keyboard->at(y);
      const auto& row_json = keyboard_json->at(y);
      for (size_t x = 0; x < row.size(); x++) {
        row[x] = row_json.at(x).as_int();
      }
    }
  }

  this->keyboard_selector_width = json.at("keyboard_selector_width").as_int();
}

phosg::JSON BinaryTextAndKeyboardsSet::json() const {
  auto collections_json = this->TextSet::json();
  auto keyboards_json = phosg::JSON::list();
  for (const auto& kb : this->keyboards) {
    phosg::JSON keyboard_json = phosg::JSON::list();
    for (size_t y = 0; y < kb->size(); y++) {
      const auto& row = kb->at(y);
      phosg::JSON row_json = phosg::JSON::list();
      for (size_t x = 0; x < row.size(); x++) {
        row_json.emplace_back(row[x]);
      }
      keyboard_json.emplace_back(std::move(row_json));
    }
    keyboards_json.emplace_back(std::move(keyboard_json));
  }
  return phosg::JSON::dict({
      {"collections", std::move(collections_json)},
      {"keyboards", std::move(keyboards_json)},
      {"keyboard_selector_width", this->keyboard_selector_width},
  });
}

const BinaryTextAndKeyboardsSet::Keyboard& BinaryTextAndKeyboardsSet::get_keyboard(size_t kb_index) const {
  return *this->keyboards.at(kb_index);
}

void BinaryTextAndKeyboardsSet::set_keyboard(size_t kb_index, const Keyboard& kb) {
  if (kb_index >= this->keyboards.size()) {
    this->keyboards.resize(kb_index + 1);
  }
  this->keyboards[kb_index] = make_unique<Keyboard>(kb);
}

void BinaryTextAndKeyboardsSet::resize_keyboards(size_t num_keyboards) {
  this->keyboards.resize(num_keyboards);
}

pair<string, string> BinaryTextAndKeyboardsSet::serialize(bool big_endian, bool is_sjis) const {
  if (big_endian) {
    return this->serialize_t<true>(is_sjis);
  } else {
    return this->serialize_t<false>(is_sjis);
  }
}

template <bool BE>
void BinaryTextAndKeyboardsSet::parse_t(const string& pr2_data, bool is_sjis) {
  auto& tt = is_sjis ? tt_sega_sjis_to_utf8 : tt_8859_to_utf8;

  // The structure is as follows:
  // Footer:
  //   U32T keyboard_index_offset ->:
  //     U8 num_keyboards
  //     U8 keyboard_selector_width
  //     U8 unused[2]
  //     U32T keyboards_offset ->:
  //       U32T keyboard_offset[num_keyboards] ->:
  //         U16T key_defs[7][16]
  //   U32T collections_offset ->:
  //     U32T[...] strings_offset ->:
  //       U32T[...] string_offset ->:
  //         char string[...\0]
  //   <EOF>

  auto pr2_decrypted = decrypt_pr2_data<BE>(pr2_data);
  auto decompressed = prs_decompress(pr2_decrypted.compressed_data);
  phosg::StringReader r(decompressed);

  // Annoyingly, there doesn't appear to be any bounds-checking on the language
  // functions, so there are no counts of strings in each collection. We have to
  // figure out where each collection ends by collecting all the relevant
  // offsets in the file instead.
  ::set<uint32_t> used_offsets;
  used_offsets.emplace(r.size() - 8);

  uint32_t keyboard_index_offset = r.pget<U32T<BE>>(r.size() - 8);
  used_offsets.emplace(keyboard_index_offset);
  size_t num_keyboards = r.pget_u8(keyboard_index_offset);
  this->keyboard_selector_width = r.pget_u8(keyboard_index_offset + 1);
  uint32_t keyboards_offset = r.pget<U32T<BE>>(keyboard_index_offset + 4);
  used_offsets.emplace(keyboards_offset);
  while (this->keyboards.size() < num_keyboards) {
    uint32_t keyboard_offset = r.pget<U32T<BE>>(keyboards_offset + 4 * this->keyboards.size());
    used_offsets.emplace(keyboard_offset);
    auto& kb = this->keyboards.emplace_back(make_unique<Keyboard>());
    auto key_r = r.sub(keyboard_offset, sizeof(Keyboard));
    for (size_t y = 0; y < kb->size(); y++) {
      auto& row = kb->at(y);
      for (size_t x = 0; x < row.size(); x++) {
        row[x] = key_r.get<U16T<BE>>();
      }
    }
  }

  uint32_t collections_offset = r.pget<U32T<BE>>(r.size() - 4);
  for (uint32_t offset = collections_offset; !used_offsets.count(offset); offset += 4) {
    used_offsets.emplace(r.pget<U32T<BE>>(offset));
  }
  used_offsets.emplace(collections_offset);

  for (uint32_t offset = collections_offset; (offset == collections_offset) || !used_offsets.count(offset); offset += 4) {
    auto& collection = this->collections.emplace_back();
    uint32_t first_string_offset_offset = r.pget<U32T<BE>>(offset);
    for (uint32_t string_offset_offset = first_string_offset_offset;
         (string_offset_offset == first_string_offset_offset) || !used_offsets.count(string_offset_offset);
         string_offset_offset += 4) {
      collection.emplace_back(tt(r.pget_cstr(r.pget<U32T<BE>>(string_offset_offset))));
    }
  }
}

template <bool BE>
pair<string, string> BinaryTextAndKeyboardsSet::serialize_t(bool is_sjis) const {
  auto& tt = is_sjis ? tt_utf8_to_sega_sjis : tt_utf8_to_8859;

  phosg::StringWriter w;
  ::set<size_t> relocation_offsets;
  auto put_offset_u32 = [&](uint32_t v) {
    relocation_offsets.emplace(w.size());
    w.put<U32T<BE>>(v);
  };

  uint32_t collections_offset;
  {
    unordered_map<string, uint32_t> string_to_offset;
    for (const auto& collection : this->collections) {
      for (const auto& s : collection) {
        if (string_to_offset.emplace(s, w.size()).second) {
          w.write(tt(s));
          w.put_u8(0);
          while (w.size() & 3) {
            w.put_u8(0);
          }
        }
      }
    }

    vector<uint32_t> collection_offsets;
    for (const auto& collection : this->collections) {
      collection_offsets.emplace_back(w.size());
      for (const auto& s : collection) {
        put_offset_u32(string_to_offset.at(s));
      }
    }

    collections_offset = w.size();
    for (uint32_t collection_offset : collection_offsets) {
      put_offset_u32(collection_offset);
    }
  }

  uint32_t keyboard_index_offset;
  {
    vector<uint32_t> keyboard_offsets;
    for (const auto& keyboard : this->keyboards) {
      keyboard_offsets.emplace_back(w.size());
      for (size_t y = 0; y < keyboard->size(); y++) {
        const auto& row = keyboard->at(y);
        for (size_t x = 0; x < row.size(); x++) {
          w.put<U16T<BE>>(row[x]);
        }
      }
    }

    uint32_t keyboards_offset = w.size();
    for (uint32_t keyboard_offset : keyboard_offsets) {
      put_offset_u32(keyboard_offset);
    }

    keyboard_index_offset = w.size();
    w.put_u8(keyboard_offsets.size());
    w.put_u8(this->keyboard_selector_width);
    w.put_u16(0);
    put_offset_u32(keyboards_offset);
  }

  put_offset_u32(keyboard_index_offset);
  put_offset_u32(collections_offset);

  phosg::StringWriter reloc_w;
  reloc_w.put_u32(0);
  reloc_w.put<U32T<BE>>(relocation_offsets.size());
  reloc_w.put_u64(0);
  reloc_w.put<U32T<BE>>(w.size() - 8);
  reloc_w.put_u32(0);
  reloc_w.put_u64(0);
  {
    size_t offset = 0;
    for (size_t reloc_offset : relocation_offsets) {
      if (reloc_offset & 3) {
        throw logic_error("misaligned relocation");
      }
      size_t num_words = (reloc_offset - offset) >> 2;
      if (num_words > 0xFFFF) {
        throw runtime_error("relocation offset too far away");
      }
      reloc_w.put<U16T<BE>>(num_words);
      offset = reloc_offset;
    }
  }

  const string& pr2_data = w.str();
  const string& pr3_data = reloc_w.str();
  string pr2_compressed = prs_compress_optimal(pr2_data.data(), pr2_data.size());
  string pr3_compressed = prs_compress_optimal(pr3_data.data(), pr3_data.size());
  string pr2_ret = encrypt_pr2_data<BE>(pr2_compressed, pr2_data.size(), phosg::random_object<uint32_t>());
  string pr3_ret = encrypt_pr2_data<BE>(pr3_compressed, pr3_data.size(), phosg::random_object<uint32_t>());
  return make_pair(std::move(pr2_ret), std::move(pr3_ret));
}

TextIndex::TextIndex(
    const string& directory,
    function<shared_ptr<const string>(Version, const string&)> get_patch_file)
    : log("[TextIndex] ", static_game_data_log.min_level) {
  if (!directory.empty()) {
    auto add_version = [&](Version version, const string& subdirectory, function<shared_ptr<TextSet>(const string&, bool)> make_set) -> void {
      static const map<string, uint8_t> bintext_filenames({
          {"TextJapanese.pr2", 0x00},
          {"TextEnglish.pr2", 0x01},
          {"TextGerman.pr2", 0x02},
          {"TextFrench.pr2", 0x03},
          {"TextSpanish.pr2", 0x04},
      });
      static const map<string, uint8_t> unitext_filenames({
          {"unitxt_j.prs", 0x00}, // PC/BB Japanese
          {"unitxt_e.prs", 0x01}, // PC/BB English
          {"unitxt_g.prs", 0x02}, // PC/BB German
          {"unitxt_f.prs", 0x03}, // PC/BB French
          {"unitxt_s.prs", 0x04}, // PC/BB Spanish
          {"unitxt_b.prs", 0x05}, // PC Simplified Chinese
          {"unitxt_cs.prs", 0x05}, // BB Simplified Chinese
          {"unitxt_t.prs", 0x06}, // PC Traditional Chinese
          {"unitxt_ct.prs", 0x06}, // BB Traditional Chinese
          {"unitxt_k.prs", 0x07}, // PC Korean
          {"unitxt_h.prs", 0x07}, // BB Korean
      });
      if (!uses_utf16(version)) {
        for (const auto& it : bintext_filenames) {
          string file_path = directory + "/" + subdirectory + "/" + it.first;
          string json_path = file_path + ".json";
          if (phosg::isfile(json_path)) {
            this->log.info("Loading %s %c JSON text set from %s", phosg::name_for_enum(version), char_for_language_code(it.second), json_path.c_str());
            this->add_set(version, it.second, make_shared<BinaryTextSet>(phosg::JSON::parse(phosg::load_file(json_path))));
          } else if (phosg::isfile(file_path)) {
            this->log.info("Loading %s %c binary text set from %s", phosg::name_for_enum(version), char_for_language_code(it.second), file_path.c_str());
            this->add_set(version, it.second, make_set(phosg::load_file(file_path), it.second == 0));
          }
        }
      } else {
        for (const auto& it : unitext_filenames) {
          string file_path = directory + "/" + subdirectory + "/" + it.first;
          string json_path = file_path + ".json";
          if (phosg::isfile(json_path)) {
            this->log.info("Loading %s %c JSON text set from %s", phosg::name_for_enum(version), char_for_language_code(it.second), json_path.c_str());
            this->add_set(version, it.second, make_shared<UnicodeTextSet>(phosg::JSON::parse(phosg::load_file(json_path))));
          } else {
            auto patch_file = get_patch_file ? get_patch_file(version, it.first) : nullptr;
            if (patch_file) {
              this->log.info("Loading %s %c Unicode text set from %s in patch tree", phosg::name_for_enum(version), char_for_language_code(it.second), it.first.c_str());
              this->add_set(version, it.second, make_set(*patch_file, it.second == 0));
            } else {
              if (phosg::isfile(file_path)) {
                this->log.info("Loading %s %c Unicode text set from %s", phosg::name_for_enum(version), char_for_language_code(it.second), file_path.c_str());
                this->add_set(version, it.second, make_set(phosg::load_file(file_path), it.second == 0));
              }
            }
          }
        }
      }
    };

    auto make_binary_dc112000 = +[](const string& data, bool is_sjis) { return make_shared<BinaryTextSet>(data, 21, true, is_sjis); };
    auto make_binary_dcnte_dcv1 = +[](const string& data, bool is_sjis) { return make_shared<BinaryTextSet>(data, 26, true, is_sjis); };
    auto make_binary_dcv2 = +[](const string& data, bool is_sjis) { return make_shared<BinaryTextSet>(data, 37, false, is_sjis); };
    auto make_binary_gc = +[](const string& data, bool is_sjis) { return make_shared<BinaryTextAndKeyboardsSet>(data, true, is_sjis); };
    auto make_binary_xb = +[](const string& data, bool is_sjis) { return make_shared<BinaryTextAndKeyboardsSet>(data, false, is_sjis); };
    auto make_unitxt = +[](const string& data, bool) { return make_shared<UnicodeTextSet>(data); };

    add_version(Version::DC_NTE, "dc-nte", make_binary_dcnte_dcv1);
    add_version(Version::DC_11_2000, "dc-11-2000", make_binary_dc112000);
    add_version(Version::DC_V1, "dc-v1", make_binary_dcnte_dcv1);
    add_version(Version::DC_V2, "dc-v2", make_binary_dcv2);
    add_version(Version::PC_NTE, "pc-nte", make_unitxt);
    add_version(Version::PC_V2, "pc-v2", make_unitxt);
    add_version(Version::GC_NTE, "gc-nte", make_binary_gc);
    add_version(Version::GC_V3, "gc-v3", make_binary_gc);
    add_version(Version::GC_EP3_NTE, "gc-ep3-nte", make_binary_gc);
    add_version(Version::GC_EP3, "gc-ep3", make_binary_gc);
    add_version(Version::XB_V3, "xb-v3", make_binary_xb);
    add_version(Version::BB_V4, "bb-v4", make_unitxt);
  }
}

void TextIndex::add_set(Version version, uint8_t language, std::shared_ptr<const TextSet> ts) {
  this->sets[this->key_for_set(version, language)] = ts;
}

void TextIndex::delete_set(Version version, uint8_t language) {
  this->sets.erase(this->key_for_set(version, language));
}

const std::string& TextIndex::get(Version version, uint8_t language, size_t collection_index, size_t string_index) const {
  return this->get(version, language)->get(collection_index, string_index);
}

const std::vector<std::string>& TextIndex::get(Version version, uint8_t language, size_t collection_index) const {
  return this->get(version, language)->get(collection_index);
}

std::shared_ptr<const TextSet> TextIndex::get(Version version, uint8_t language) const {
  return this->sets.at(this->key_for_set(version, language));
}

uint32_t TextIndex::key_for_set(Version version, uint8_t language) {
  return (static_cast<uint32_t>(version) << 8) | language;
}
