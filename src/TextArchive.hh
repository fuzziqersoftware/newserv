#pragma once

#include <stdint.h>

#include <phosg/JSON.hh>
#include <string>
#include <utility>
#include <vector>

#include "Text.hh"

// This class implements loading and saving of text archives, commonly found in
// PSO games with filenames like TextEnglish.pr2 and TextEnglish.pr3. The game
// requires both files, but newserv needs only the pr2 file to load a text
// archive. When saving (serializing), both pr2 and pr3 files are generated.
class TextArchive {
public:
  using Keyboard = parray<parray<uint16_t, 0x10>, 7>;

  explicit TextArchive(const JSON& json);
  TextArchive(const std::string& pr2_data, bool big_endian);
  ~TextArchive() = default;

  JSON json() const;

  const std::string& get_string(size_t collection_index, size_t index) const;
  void set_string(size_t collection_index, size_t index, const std::string& data);
  void set_string(size_t collection_index, size_t index, std::string&& data);
  void resize_collection(size_t collection_index, size_t size);
  void resize_collection(size_t num_collections);

  Keyboard get_keyboard(size_t kb_index) const;
  void set_keyboard(size_t kb_index, const Keyboard& kb);
  void resize_keyboards(size_t num_keyboards);

  uint8_t get_keyboard_selector_width() const;
  void set_keyboard_selector_width(uint8_t width);

  // Returns (pr2_data, pr3_data)
  std::pair<std::string, std::string> serialize(bool big_endian) const;

private:
  template <bool IsBigEndian>
  void load_t(const std::string& pr2_data);
  template <bool IsBigEndian>
  std::pair<std::string, std::string> serialize_t() const;

  std::vector<std::vector<std::string>> collections;
  std::vector<std::unique_ptr<Keyboard>> keyboards;
  uint8_t keyboard_selector_width;
};
