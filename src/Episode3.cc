#include "Episode3.hh"

#include <stdint.h>

#include <phosg/Filesystem.hh>

#include "Compression.hh"
#include "Text.hh"

using namespace std;



Ep3DataIndex::Ep3DataIndex(const string& directory) {
  try {
    this->compressed_card_definitions = load_file(directory + "/cardupdate.mnr");
    string data = prs_decompress(this->compressed_card_definitions);
    // There's a footer after the card definitions, but we ignore it
    if (data.size() % sizeof(Ep3CardDefinition) != sizeof(Ep3CardDefinitionsFooter)) {
      throw runtime_error(string_printf(
          "decompressed card update file size %zX is not aligned with card definition size %zX (%zX extra bytes)",
          data.size(), sizeof(Ep3CardDefinition), data.size() % sizeof(Ep3CardDefinition)));
    }
    const auto* defs = reinterpret_cast<const Ep3CardDefinition*>(data.data());
    size_t max_cards = data.size() / sizeof(Ep3CardDefinition);
    for (size_t x = 0; x < max_cards; x++) {
      // The last card entry has the build date and some other metadata (and
      // isn't a real card, obviously), so skip it. Seems like the card ID is
      // always a large number that won't fit in a uint16_t, so we use that to
      // determine if the entry is a real card or not.
      if (defs[x].card_id & 0xFFFF0000) {
        continue;
      }
      shared_ptr<Ep3CardDefinition> shared_def(new Ep3CardDefinition(defs[x]));
      if (!this->card_definitions.emplace(shared_def->card_id, shared_def).second) {
        throw runtime_error(string_printf(
            "duplicate card id: %08" PRIX32, shared_def->card_id.load()));
      }

      // TODO: remove debugging code here
      // string a2str = format_data_string(defs[x].unknown_a2.data(), sizeof(defs[x].unknown_a2));
      // string a4str = format_data_string(defs[x].unknown_a4.data(), sizeof(defs[x].unknown_a4));
      // fprintf(stderr, "[debug] %-20s = %04X %s %04X %s\n", defs[x].name.data(), defs[x].unused.load(), a2str.c_str(), defs[x].unknown_a3.load(), a4str.c_str());
    }

    log(INFO, "Indexed %zu Episode 3 card definitions", this->card_definitions.size());
  } catch (const exception& e) {
    log(WARNING, "Failed to load Episode 3 card update: %s", e.what());
  }

  for (const auto& filename : list_directory(directory)) {
    if (ends_with(filename, ".mnm")) {
      try {
        string compressed_data = load_file(directory + "/" + filename);
        // There's a small header (Ep3CompressedMapHeader) before the compressed
        // data, which we ignore
        string data_to_decompress = compressed_data.substr(8);
        string data = prs_decompress(data_to_decompress);
        if (data.size() != sizeof(Ep3Map)) {
          throw runtime_error(string_printf(
              "decompressed data size is incorrect (expected %zu bytes, read %zu bytes)",
              sizeof(Ep3Map), data.size()));
        }

        shared_ptr<MapEntry> entry(new MapEntry(
            {*reinterpret_cast<const Ep3Map*>(data.data()), move(compressed_data)}));
        if (!this->maps.emplace(entry->map.map_number, entry).second) {
          throw runtime_error("duplicate map number");
        }
        string name = entry->map.name;
        log(INFO, "Indexed Episode 3 map %s (%08" PRIX32 "; %s)",
            filename.c_str(), entry->map.map_number.load(), name.c_str());

      } catch (const exception& e) {
        log(WARNING, "Failed to index Episode 3 map %s: %s",
            filename.c_str(), e.what());
      }
    }
  }

  // TODO: Write a version of prs_compress that takes iovecs (or something
  // similar) so we can eliminate all this string copying here. At least this
  // only happens once at load time...
  StringWriter entries_w;
  StringWriter strings_w;

  for (const auto& map_it : this->maps) {
    Ep3MapList::Entry e;
    const auto& map = map_it.second->map;
    e.map_x = map.map_x;
    e.map_y = map.map_y;
    e.scene_data2 = map.scene_data2;
    e.map_number = map.map_number.load();
    e.width = map.width;
    e.height = map.height;
    e.map_tiles = map.map_tiles;
    e.modification_tiles = map.modification_tiles;

    e.name_offset = strings_w.size();
    strings_w.write(map.name.data(), map.name.len());
    strings_w.put_u8(0);
    e.location_name_offset = strings_w.size();
    strings_w.write(map.location_name.data(), map.location_name.len());
    strings_w.put_u8(0);
    e.quest_name_offset = strings_w.size();
    strings_w.write(map.quest_name.data(), map.quest_name.len());
    strings_w.put_u8(0);
    e.description_offset = strings_w.size();
    strings_w.write(map.description.data(), map.description.len());
    strings_w.put_u8(0);

    e.unknown_a2 = 0xFF000000;

    entries_w.put(e);
  }

  Ep3MapList header;
  header.num_maps = this->maps.size();
  header.unknown_a1 = 0;
  header.strings_offset = entries_w.size();
  header.total_size = sizeof(Ep3MapList) + entries_w.size() + strings_w.size();

  StringWriter w;
  w.put(header);
  w.write(entries_w.str());
  w.write(strings_w.str());

  StringWriter compressed_w;
  compressed_w.put_u32b(w.str().size());
  compressed_w.write(prs_compress(w.str()));
  this->compressed_map_list = move(compressed_w.str());
  log(INFO, "Generated Episode 3 compressed map list (%zu -> %zu bytes)",
      w.size(), this->compressed_map_list.size());
}

const string& Ep3DataIndex::get_compressed_card_definitions() const {
  if (this->compressed_card_definitions.empty()) {
    throw runtime_error("card definitions are not available");
  }
  return this->compressed_card_definitions;
}

shared_ptr<const Ep3CardDefinition> Ep3DataIndex::get_card_definition(
    uint32_t id) const {
  return this->card_definitions.at(id);
}

const string& Ep3DataIndex::get_compressed_map_list() const {
  if (this->compressed_map_list.empty()) {
    throw runtime_error("map list is not available");
  }
  return this->compressed_map_list;
}

shared_ptr<const Ep3DataIndex::MapEntry> Ep3DataIndex::get_map(uint32_t id) const {
  return this->maps.at(id);
}
