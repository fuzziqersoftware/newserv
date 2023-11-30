#include "TextArchive.hh"

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>
#include <set>
#include <stdexcept>

#include "Compression.hh"
#include "PSOEncryption.hh"
#include "Text.hh"

using namespace std;

TextArchive::TextArchive(const string& pr2_data, bool big_endian) {
  if (big_endian) {
    this->load_t<true>(pr2_data);
  } else {
    this->load_t<false>(pr2_data);
  }
}

TextArchive::TextArchive(const JSON& json) {
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

JSON TextArchive::json() const {
  auto collections_json = JSON::list();
  for (const auto& collection : this->collections) {
    auto collection_json = JSON::list();
    for (const auto& s : collection) {
      collection_json.emplace_back(s);
    }
    collections_json.emplace_back(std::move(collection_json));
  }
  auto keyboards_json = JSON::list();
  for (const auto& kb : this->keyboards) {
    JSON keyboard_json = JSON::list();
    for (size_t y = 0; y < kb->size(); y++) {
      const auto& row = kb->at(y);
      JSON row_json = JSON::list();
      for (size_t x = 0; x < row.size(); x++) {
        row_json.emplace_back(row[x]);
      }
      keyboard_json.emplace_back(std::move(row_json));
    }
    keyboards_json.emplace_back(std::move(keyboard_json));
  }
  return JSON::dict({
      {"collections", std::move(collections_json)},
      {"keyboards", std::move(keyboards_json)},
      {"keyboard_selector_width", this->keyboard_selector_width},
  });
}

const string& TextArchive::get_string(size_t collection_index, size_t index) const {
  return this->collections.at(collection_index).at(index);
}

void TextArchive::set_string(size_t collection_index, size_t index, const string& data) {
  if (collection_index >= this->collections.size()) {
    this->collections.resize(collection_index + 1);
  }
  auto& coll = this->collections[collection_index];
  if (index >= coll.size()) {
    coll.resize(index + 1);
  }
  coll[index] = data;
}

void TextArchive::set_string(size_t collection_index, size_t index, string&& data) {
  if (collection_index >= this->collections.size()) {
    this->collections.resize(collection_index + 1);
  }
  auto& coll = this->collections[collection_index];
  if (index >= coll.size()) {
    coll.resize(index + 1);
  }
  coll[index] = std::move(data);
}

void TextArchive::resize_collection(size_t collection_index, size_t size) {
  if (collection_index >= this->collections.size()) {
    this->collections.resize(collection_index + 1);
  }
  this->collections[collection_index].resize(size);
}

void TextArchive::resize_collection(size_t num_collections) {
  this->collections.resize(num_collections);
}

TextArchive::Keyboard TextArchive::get_keyboard(size_t kb_index) const {
  return *this->keyboards.at(kb_index);
}

void TextArchive::set_keyboard(size_t kb_index, const Keyboard& kb) {
  if (kb_index >= this->keyboards.size()) {
    this->keyboards.resize(kb_index + 1);
  }
  this->keyboards[kb_index] = make_unique<Keyboard>(kb);
}

void TextArchive::resize_keyboards(size_t num_keyboards) {
  this->keyboards.resize(num_keyboards);
}

pair<string, string> TextArchive::serialize(bool big_endian) const {
  if (big_endian) {
    return this->serialize_t<true>();
  } else {
    return this->serialize_t<false>();
  }
}

template <bool IsBigEndian>
void TextArchive::load_t(const string& pr2_data) {
  using U32T = std::conditional_t<IsBigEndian, be_uint32_t, le_uint32_t>;
  using U16T = std::conditional_t<IsBigEndian, be_uint16_t, le_uint16_t>;

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

  auto pr2_decrypted = decrypt_pr2_data<IsBigEndian>(pr2_data);
  auto decompressed = prs_decompress(pr2_decrypted.compressed_data);
  StringReader r(decompressed);

  // Annoyingly, there doesn't appear to be any bounds-checking on the language
  // functions, so there are no counts of strings in each collection. We have to
  // figure out where each collection ends by collecting all the relevant
  // offsets in the file instead.
  set<uint32_t> used_offsets;
  used_offsets.emplace(r.size() - 8);

  uint32_t keyboard_index_offset = r.pget<U32T>(r.size() - 8);
  used_offsets.emplace(keyboard_index_offset);
  size_t num_keyboards = r.pget_u8(keyboard_index_offset);
  this->keyboard_selector_width = r.pget_u8(keyboard_index_offset + 1);
  uint32_t keyboards_offset = r.pget<U32T>(keyboard_index_offset + 4);
  used_offsets.emplace(keyboards_offset);
  while (this->keyboards.size() < num_keyboards) {
    uint32_t keyboard_offset = r.pget<U32T>(keyboards_offset + 4 * this->keyboards.size());
    used_offsets.emplace(keyboard_offset);
    auto& kb = this->keyboards.emplace_back(make_unique<Keyboard>());
    auto key_r = r.sub(keyboard_offset, sizeof(Keyboard));
    for (size_t y = 0; y < kb->size(); y++) {
      auto& row = kb->at(y);
      for (size_t x = 0; x < row.size(); x++) {
        row[x] = key_r.get<U16T>();
      }
    }
  }

  uint32_t collections_offset = r.pget<U32T>(r.size() - 4);
  for (uint32_t offset = collections_offset; !used_offsets.count(offset); offset += 4) {
    used_offsets.emplace(r.pget<U32T>(offset));
  }
  used_offsets.emplace(collections_offset);

  for (uint32_t offset = collections_offset; (offset == collections_offset) || !used_offsets.count(offset); offset += 4) {
    auto& collection = this->collections.emplace_back();
    uint32_t first_string_offset_offset = r.pget<U32T>(offset);
    for (uint32_t string_offset_offset = first_string_offset_offset;
         (string_offset_offset == first_string_offset_offset) || !used_offsets.count(string_offset_offset);
         string_offset_offset += 4) {
      collection.emplace_back(r.pget_cstr(r.pget<U32T>(string_offset_offset)));
    }
  }
}

template <bool IsBigEndian>
pair<string, string> TextArchive::serialize_t() const {
  using U32T = std::conditional_t<IsBigEndian, be_uint32_t, le_uint32_t>;
  using U16T = std::conditional_t<IsBigEndian, be_uint16_t, le_uint16_t>;

  StringWriter w;
  set<size_t> relocation_offsets;
  auto put_offset_u32 = [&](uint32_t v) {
    relocation_offsets.emplace(w.size());
    w.put<U32T>(v);
  };

  uint32_t collections_offset;
  {
    unordered_map<string, uint32_t> string_to_offset;
    for (const auto& collection : this->collections) {
      for (const auto& s : collection) {
        if (string_to_offset.emplace(s, w.size()).second) {
          w.write(s);
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
          w.put<U16T>(row[x]);
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

  StringWriter reloc_w;
  reloc_w.put_u32(0);
  reloc_w.put<U32T>(relocation_offsets.size());
  reloc_w.put_u64(0);
  reloc_w.put<U32T>(w.size() - 8);
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
      reloc_w.put<U16T>(num_words);
      offset = reloc_offset;
    }
  }

  const string& pr2_data = w.str();
  const string& pr3_data = reloc_w.str();
  print_data(stderr, pr2_data);
  string pr2_compressed = prs_compress_optimal(pr2_data.data(), pr2_data.size());
  string pr3_compressed = prs_compress_optimal(pr3_data.data(), pr3_data.size());
  print_data(stderr, pr2_compressed);
  string pr2_ret = encrypt_pr2_data<IsBigEndian>(pr2_compressed, pr2_data.size(), random_object<uint32_t>());
  string pr3_ret = encrypt_pr2_data<IsBigEndian>(pr3_compressed, pr3_data.size(), random_object<uint32_t>());
  print_data(stderr, pr2_ret);
  return make_pair(std::move(pr2_ret), std::move(pr3_ret));
}
