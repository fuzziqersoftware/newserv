#pragma once

#include <stdint.h>

#include <phosg/JSON.hh>
#include <string>
#include <utility>
#include <vector>

#include "Text.hh"
#include "Version.hh"

class TextSet {
public:
  virtual ~TextSet() = default;
  virtual phosg::JSON json() const;

  size_t count(size_t collection_index) const;
  size_t count() const;

  const std::string& get(size_t collection, size_t index) const;
  const std::vector<std::string>& get(size_t collection) const;

  void set(size_t collection_index, size_t string_index, const std::string& data);
  void set(size_t collection_index, size_t string_index, std::string&& data);
  void set(size_t collection_index, const std::vector<std::string>& coll);
  void set(size_t collection_index, std::vector<std::string>&& coll);

  void truncate_collection(size_t collection, size_t new_entry_count);
  void truncate(size_t new_collection_count);

protected:
  std::vector<std::vector<std::string>> collections;

  TextSet() = default;
  TextSet(const phosg::JSON& json);
  TextSet(phosg::JSON&& json);

  void ensure_slot_exists(size_t collection_index, size_t string_index);
  void ensure_collection_exists(size_t collection_index);
};

class UnicodeTextSet : public TextSet {
public:
  explicit UnicodeTextSet(const phosg::JSON& json) : TextSet(json) {}
  explicit UnicodeTextSet(phosg::JSON&& json) : TextSet(json) {}
  explicit UnicodeTextSet(const std::string& unitxt_prs_data);
  virtual ~UnicodeTextSet() = default;
  std::string serialize() const;
};

class BinaryTextSet : public TextSet {
public:
  explicit BinaryTextSet(const phosg::JSON& json) : TextSet(json) {}
  explicit BinaryTextSet(phosg::JSON&& json) : TextSet(json) {}
  BinaryTextSet(const std::string& pr2_data, size_t collection_count, bool has_rel_footer, bool is_sjis);
  ~BinaryTextSet() = default;
  // TODO: Implement serialize functions
};

class BinaryTextAndKeyboardsSet : public TextSet {
public:
  using Keyboard = parray<parray<uint16_t, 0x10>, 7>;

  explicit BinaryTextAndKeyboardsSet(const phosg::JSON& json);
  explicit BinaryTextAndKeyboardsSet(phosg::JSON&& json);
  BinaryTextAndKeyboardsSet(const std::string& pr2_data, bool big_endian, bool is_sjis);
  ~BinaryTextAndKeyboardsSet() = default;

  virtual phosg::JSON json() const;

  const Keyboard& get_keyboard(size_t kb_index) const;
  void set_keyboard(size_t kb_index, const Keyboard& kb);
  void resize_keyboards(size_t num_keyboards);

  uint8_t get_keyboard_selector_width() const;
  void set_keyboard_selector_width(uint8_t width);

  // Returns (pr2_data, pr3_data)
  std::pair<std::string, std::string> serialize(bool big_endian, bool is_sjis) const;

protected:
  template <bool BE>
  void parse_t(const std::string& pr2_data, bool is_sjis);
  template <bool BE>
  std::pair<std::string, std::string> serialize_t(bool is_sjis) const;

  std::vector<std::unique_ptr<Keyboard>> keyboards;
  uint8_t keyboard_selector_width;
};

class TextIndex {
public:
  explicit TextIndex(
      const std::string& directory = "",
      std::function<std::shared_ptr<const std::string>(Version, const std::string&)> get_patch_file = nullptr);
  ~TextIndex() = default;

  void add_set(Version version, uint8_t language, std::shared_ptr<const TextSet> ts);
  void delete_set(Version version, uint8_t language);

  const std::string& get(Version version, uint8_t language, size_t collection_index, size_t string_index) const;
  const std::vector<std::string>& get(Version version, uint8_t language, size_t collection_index) const;
  std::shared_ptr<const TextSet> get(Version version, uint8_t language) const;

protected:
  static uint32_t key_for_set(Version version, uint8_t language);

  phosg::PrefixedLogger log;
  std::unordered_map<uint32_t, std::shared_ptr<const TextSet>> sets;
};
