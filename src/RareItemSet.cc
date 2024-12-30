#include "RareItemSet.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Math.hh>
#include <phosg/Random.hh>

#include "BattleParamsIndex.hh"
#include "CommonFileFormats.hh"
#include "ItemData.hh"
#include "StaticGameData.hh"

using namespace std;

string RareItemSet::ExpandedDrop::str() const {
  auto frac = phosg::reduce_fraction<uint64_t>(this->probability, 0x100000000);
  auto hex = this->data.hex();
  return phosg::string_printf(
      "(%08" PRIX32 " => %" PRIu64 "/%" PRIu64 ") %s",
      this->probability, frac.first, frac.second, hex.c_str());
}

string RareItemSet::ExpandedDrop::str(shared_ptr<const ItemNameIndex> name_index) const {
  string ret = this->str();
  ret += " (";
  ret += name_index->describe_item(this->data);
  ret += ")";
  return ret;
}

uint32_t RareItemSet::expand_rate(uint8_t pc) {
  // To compute the actual drop rare drop rate from pc, first decode pc into
  // shift and value:
  //   pc = bits SSSSSVVV
  //     shift = S - 4 (so shift is 0-27)
  //     value = V + 7 (so value is 7-14)
  // Then, take the value 0x00000002, shift it left by shift (0-27), and
  // multiply the result by value (7-14) to get the actual drop rate. The result
  // is a probability out of 0xFFFFFFFF (so 0x40000000 means the item will drop
  // 25% of the time, for example).
  int8_t shift = ((pc >> 3) & 0x1F) - 4;
  if (shift < 0) {
    shift = 0;
  }
  return ((2 << shift) * ((pc & 7) + 7));
}

uint8_t RareItemSet::compress_rate(uint32_t probability) {
  // I'm too lazy to figure out the reverse bitwise math, so we just compute all
  // the expansions and take the closest one
  static std::map<uint32_t, uint8_t> inverse_map;
  if (inverse_map.empty()) {
    for (size_t z = 0; z < 0x100; z++) {
      inverse_map.emplace(RareItemSet::expand_rate(z), z);
    }
  }

  auto it = inverse_map.lower_bound(probability);
  if (it == inverse_map.end()) {
    // The expanded probability is less likely than the least likely value
    return inverse_map.rbegin()->second;
  } else if (it->first == probability) {
    // The expanded probability is exactly equal to this entry
    return it->second;
  } else if (it == inverse_map.begin()) {
    // The expanded probability more likely than the most likely value
    return it->second;
  } else {
    // The expanded probability is between two entries; choose the closer one
    auto prev_it = it;
    prev_it--;
    int32_t prev_diff = static_cast<int32_t>(prev_it->first - probability);
    int32_t next_diff = static_cast<int32_t>(it->first - probability);
    return (prev_diff < next_diff) ? prev_it->second : it->second;
  }
}

RareItemSet::ParsedRELData::PackedDrop::PackedDrop(const ExpandedDrop& exp)
    : probability(RareItemSet::compress_rate(exp.probability)) {
  if (!exp.data.can_be_encoded_in_rel_rare_table()) {
    throw runtime_error("item " + exp.data.short_hex() + " has extended attributes and cannot be encoded in a REL file");
  }
  this->item_code[0] = exp.data.data1[0];
  this->item_code[1] = exp.data.data1[1];
  this->item_code[2] = exp.data.data1[2];
}

RareItemSet::ExpandedDrop RareItemSet::ParsedRELData::PackedDrop::expand() const {
  ExpandedDrop ret;
  ret.probability = RareItemSet::expand_rate(this->probability);
  ret.data.data1[0] = this->item_code[0];
  ret.data.data1[1] = this->item_code[1];
  ret.data.data1[2] = this->item_code[2];
  return ret;
}

template <bool BE>
void RareItemSet::ParsedRELData::parse_t(phosg::StringReader r, bool is_v1) {
  const auto& footer = r.pget<RELFileFooterT<BE>>(r.size() - sizeof(RELFileFooterT<BE>));
  const auto& root = r.pget<OffsetsT<BE>>(footer.root_offset);

  phosg::StringReader monsters_r = r.sub(root.monster_rares_offset);
  for (size_t z = 0; z < (is_v1 ? 0x33 : 0x65); z++) {
    const auto& d = monsters_r.get<PackedDrop>();
    this->monster_rares.emplace_back(d.expand());
  }

  phosg::StringReader box_areas_r = r.sub(root.box_areas_offset, root.box_count * sizeof(uint8_t));
  phosg::StringReader box_drops_r = r.sub(root.box_rares_offset, root.box_count * sizeof(PackedDrop));
  for (size_t z = 0; z < root.box_count; z++) {
    uint8_t area = box_areas_r.get_u8();
    const auto& drop = box_drops_r.get<PackedDrop>();
    if (!drop.item_code.is_filled_with(0)) {
      auto& box_rare = this->box_rares.emplace_back();
      box_rare.area = area;
      box_rare.drop = drop.expand();
    }
  }
}

template <bool BE>
std::string RareItemSet::ParsedRELData::serialize_t(bool is_v1) const {
  static const PackedDrop empty_drop;

  OffsetsT<BE> root;
  root.box_count = this->box_rares.size();

  phosg::StringWriter w;
  root.monster_rares_offset = w.size();
  for (const auto& drop : this->monster_rares) {
    w.put(PackedDrop(drop));
  }
  while (w.size() < root.monster_rares_offset + (is_v1 ? 0x33 : 0x65) * sizeof(PackedDrop)) {
    w.put(empty_drop);
  }
  root.box_areas_offset = w.size();
  for (const auto& drop : this->box_rares) {
    w.put_u8(drop.area);
  }
  for (size_t z = this->box_rares.size(); z < 30; z++) {
    w.put_u8(0xFF);
  }
  root.box_rares_offset = w.size();
  for (const auto& drop : this->box_rares) {
    w.put(PackedDrop(drop.drop));
  }
  for (size_t z = this->box_rares.size(); z < 30; z++) {
    w.put_u32l(0x00000000);
  }
  while (w.size() & 3) {
    w.put_u8(0);
  }
  uint32_t root_offset = w.size();
  w.put(root);
  while (w.size() & 0x1F) {
    w.put_u8(0);
  }
  uint32_t relocations_offset = w.size();
  w.put<U16T<BE>>(root_offset >> 2);
  w.put<U16T<BE>>(2);
  w.put<U16T<BE>>(1);
  while (w.size() & 0x1F) {
    w.put_u8(0);
  }

  RELFileFooterT<BE> footer;
  footer.relocations_offset = relocations_offset;
  footer.num_relocations = 3;
  footer.unused1[0] = 1; // TODO: What is this used for?
  footer.root_offset = root_offset;
  w.put<RELFileFooterT<BE>>(footer);
  return std::move(w.str());
}

RareItemSet::ParsedRELData::ParsedRELData(phosg::StringReader r, bool big_endian, bool is_v1) {
  if (big_endian) {
    this->parse_t<true>(r, is_v1);
  } else {
    this->parse_t<false>(r, is_v1);
  }
}

RareItemSet::ParsedRELData::ParsedRELData(const SpecCollection& collection) {
  for (const auto& specs : collection.rt_index_to_specs) {
    ExpandedDrop effective_spec;
    for (const auto& spec : specs) {
      if (effective_spec.data.empty()) {
        effective_spec = spec;
      } else if ((effective_spec.probability != spec.probability) || (effective_spec.data != spec.data)) {
        throw runtime_error("monster spec cannot be converted to ItemRT format");
      }
    }
    this->monster_rares.emplace_back(specs.empty() ? ExpandedDrop() : specs[0]);
  }

  if (collection.box_area_to_specs.size() > 0xFF) {
    throw runtime_error("area value too high");
  }
  for (uint8_t area = 0; area < collection.box_area_to_specs.size(); area++) {
    for (const auto& spec : collection.box_area_to_specs[area]) {
      this->box_rares.emplace_back(BoxRare{.area = area, .drop = spec});
    }
  }
}

std::string RareItemSet::ParsedRELData::serialize(bool big_endian, bool is_v1) const {
  if (big_endian) {
    return this->serialize_t<true>(is_v1);
  } else {
    return this->serialize_t<false>(is_v1);
  }
}

RareItemSet::SpecCollection RareItemSet::ParsedRELData::as_collection() const {
  SpecCollection ret;
  for (size_t z = 0; z < this->monster_rares.size(); z++) {
    const auto& drop = this->monster_rares[z];
    if (drop.data.empty()) {
      continue;
    }
    if (z >= ret.rt_index_to_specs.size()) {
      ret.rt_index_to_specs.resize(z + 1);
    }
    ret.rt_index_to_specs[z].emplace_back(drop);
  }
  for (const auto& drop : this->box_rares) {
    if (drop.drop.data.empty()) {
      continue;
    }
    if (drop.area >= ret.box_area_to_specs.size()) {
      ret.box_area_to_specs.resize(drop.area + 1);
    }
    ret.box_area_to_specs[drop.area].emplace_back(drop.drop);
  }
  return ret;
}

RareItemSet::RareItemSet(const AFSArchive& afs, bool is_v1) {
  const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (GameMode mode : modes) {
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        try {
          size_t index = difficulty * 10 + section_id;
          ParsedRELData rel(afs.get_reader(index), false, is_v1);
          this->collections.emplace(
              this->key_for_params(mode, Episode::EP1, difficulty, section_id),
              rel.as_collection());
        } catch (const out_of_range&) {
        }
      }
    }
  }
}

string RareItemSet::gsl_entry_name_for_table(GameMode mode, Episode episode, uint8_t difficulty, uint8_t section_id) {
  return phosg::string_printf("ItemRT%s%s%c%1hhu.rel",
      ((mode == GameMode::CHALLENGE) ? "c" : ""),
      ((episode == Episode::EP2) ? "l" : ""),
      tolower(abbreviation_for_difficulty(difficulty)), // One of "nhvu"
      section_id);
}

RareItemSet::RareItemSet(const GSLArchive& gsl, bool is_big_endian) {
  const array<Episode, 2> episodes = {Episode::EP1, Episode::EP2};
  const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (GameMode mode : modes) {
    for (Episode episode : episodes) {
      for (size_t difficulty = 0; difficulty < 4; difficulty++) {
        for (size_t section_id = 0; section_id < 10; section_id++) {
          try {
            string filename = this->gsl_entry_name_for_table(mode, episode, difficulty, section_id);
            ParsedRELData rel(gsl.get_reader(filename), is_big_endian, false);
            this->collections.emplace(
                this->key_for_params(mode, episode, difficulty, section_id),
                rel.as_collection());
          } catch (const out_of_range&) {
          }
        }
      }
    }
  }
}

RareItemSet::RareItemSet(const string& rel_data, bool is_big_endian) {
  // Tables are 0x280 bytes in size in this format, laid out sequentially
  phosg::StringReader r(rel_data);
  array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
  for (size_t ep_index = 0; ep_index < episodes.size(); ep_index++) {
    for (size_t difficulty = 0; difficulty < 4; difficulty++) {
      for (size_t section_id = 0; section_id < 10; section_id++) {
        try {
          size_t index = (ep_index * 40) + difficulty * 10 + section_id;
          ParsedRELData rel(r.sub(0x280 * index, 0x280), is_big_endian, false);
          this->collections.emplace(
              this->key_for_params(GameMode::NORMAL, episodes[ep_index], difficulty, section_id),
              rel.as_collection());
        } catch (const out_of_range&) {
        }
      }
    }
  }
}

RareItemSet::RareItemSet(const phosg::JSON& json, shared_ptr<const ItemNameIndex> name_index) {
  for (const auto& mode_it : json.as_dict()) {
    static const unordered_map<string, GameMode> mode_keys(
        {{"Normal", GameMode::NORMAL}, {"Battle", GameMode::BATTLE}, {"Challenge", GameMode::CHALLENGE}, {"Solo", GameMode::SOLO}});
    GameMode mode = mode_keys.at(mode_it.first);

    for (const auto& episode_it : mode_it.second->as_dict()) {
      static const unordered_map<string, Episode> episode_keys(
          {{"Episode1", Episode::EP1}, {"Episode2", Episode::EP2}, {"Episode4", Episode::EP4}});
      Episode episode = episode_keys.at(episode_it.first);

      for (const auto& difficulty_it : episode_it.second->as_dict()) {
        static const unordered_map<string, uint8_t> difficulty_keys(
            {{"Normal", 0}, {"Hard", 1}, {"VeryHard", 2}, {"Ultimate", 3}});
        uint8_t difficulty = difficulty_keys.at(difficulty_it.first);

        for (const auto& section_id_it : difficulty_it.second->as_dict()) {
          uint8_t section_id = section_id_for_name(section_id_it.first);

          auto& collection = this->collections[this->key_for_params(mode, episode, difficulty, section_id)];
          for (const auto& item_it : section_id_it.second->as_dict()) {
            vector<ExpandedDrop>* target;
            if (phosg::starts_with(item_it.first, "Box-")) {
              uint8_t area = floor_for_name(item_it.first.substr(4));
              if (collection.box_area_to_specs.size() <= area) {
                collection.box_area_to_specs.resize(area + 1);
              }
              target = &collection.box_area_to_specs[area];
            } else {
              size_t index = rare_table_index_for_enemy_type(phosg::enum_for_name<EnemyType>(item_it.first.c_str()));
              if (collection.rt_index_to_specs.size() <= index) {
                collection.rt_index_to_specs.resize(index + 1);
              }
              target = &collection.rt_index_to_specs[index];
            }

            for (const auto& spec_json : item_it.second->as_list()) {
              auto& d = target->emplace_back();

              auto prob_desc = spec_json->at(0);
              if (prob_desc.is_int()) {
                d.probability = prob_desc.as_int();
              } else if (prob_desc.is_string()) {
                auto tokens = phosg::split(prob_desc.as_string(), '/');
                if (tokens.size() != 2) {
                  throw runtime_error("invalid probability specification");
                }
                uint64_t numerator = stoull(tokens[0], nullptr, 0);
                uint64_t denominator = stoull(tokens[1], nullptr, 0);
                if (numerator == denominator) {
                  d.probability = 0xFFFFFFFF;
                } else {
                  d.probability = (static_cast<uint64_t>(numerator) << 32) / denominator;
                }
              }

              auto item_desc = spec_json->at(1);
              if (item_desc.is_int()) {
                uint32_t item_code = item_desc.as_int();
                d.data.data1[0] = (item_code >> 16) & 0xFF;
                d.data.data1[1] = (item_code >> 8) & 0xFF;
                d.data.data1[2] = item_code & 0xFF;
              } else if (item_desc.is_string()) {
                if (!name_index) {
                  throw runtime_error("item name index is not available");
                }
                d.data = name_index->parse_item_description(item_desc.as_string());
              } else {
                throw runtime_error("invalid item description type");
              }
            }
          }
        }
      }
    }
  }
}

std::string RareItemSet::serialize_afs(bool is_v1) const {
  vector<string> files;
  for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
    for (uint8_t section_id = 0; section_id < 10; section_id++) {
      ParsedRELData rel(this->get_collection(GameMode::NORMAL, Episode::EP1, difficulty, section_id));
      files.emplace_back(rel.serialize(false, is_v1));
    }
  }
  return AFSArchive::generate(files, false);
}

std::string RareItemSet::serialize_gsl(bool big_endian) const {
  unordered_map<string, string> files;

  static const std::array<Episode, 2> episodes = {Episode::EP1, Episode::EP2};
  for (Episode episode : episodes) {
    for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
      for (uint8_t section_id = 0; section_id < 10; section_id++) {
        try {
          string filename = this->gsl_entry_name_for_table(GameMode::NORMAL, episode, difficulty, section_id);
          ParsedRELData rel(this->get_collection(GameMode::NORMAL, episode, difficulty, section_id));
          files.emplace(filename, rel.serialize(big_endian, false));
        } catch (const out_of_range&) {
          // Collection does not exist; skip it
        }
      }
    }
  }

  for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
    for (uint8_t section_id = 0; section_id < 10; section_id++) {
      try {
        string filename = this->gsl_entry_name_for_table(GameMode::CHALLENGE, Episode::EP1, difficulty, section_id);
        ParsedRELData rel(this->get_collection(GameMode::CHALLENGE, Episode::EP1, difficulty, section_id));
        files.emplace(filename, rel.serialize(big_endian, false));
      } catch (const out_of_range&) {
        // Collection does not exist; skip it
      }
    }
  }
  return GSLArchive::generate(files, big_endian);
}

phosg::JSON RareItemSet::json(shared_ptr<const ItemNameIndex> name_index) const {
  auto modes_dict = phosg::JSON::dict();
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (const auto& mode : modes) {
    auto episodes_dict = phosg::JSON::dict();
    static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
    for (const auto& episode : episodes) {
      auto difficulty_dict = phosg::JSON::dict();
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        auto section_id_dict = phosg::JSON::dict();
        for (uint8_t section_id = 0; section_id < 10; section_id++) {
          auto collection_dict = phosg::JSON::dict();
          for (size_t rt_index = 0; rt_index < 0x80; rt_index++) {
            const auto& enemy_types = enemy_types_for_rare_table_index(episode, rt_index);
            if (enemy_types.empty()) {
              continue;
            }

            for (const auto& spec : this->get_enemy_specs(GameMode::NORMAL, episode, difficulty, section_id, rt_index)) {
              if (spec.data.empty()) {
                continue;
              }
              auto frac = phosg::reduce_fraction<uint64_t>(spec.probability, 0x100000000);
              auto spec_json = phosg::JSON::list({phosg::string_printf("%" PRIu64 "/%" PRIu64, frac.first, frac.second)});
              if (spec.data.can_be_encoded_in_rel_rare_table()) {
                spec_json.emplace_back((spec.data.data1[0] << 16) | (spec.data.data1[1] << 8) | spec.data.data1[2]);
              } else {
                spec_json.emplace_back(spec.data.short_hex());
              }
              if (name_index) {
                spec_json.emplace_back(name_index->describe_item(spec.data));
              }
              for (const auto& enemy_type : enemy_types) {
                if (enemy_type_valid_for_episode(episode, enemy_type)) {
                  phosg::JSON this_spec_json = spec_json;
                  collection_dict.emplace(phosg::name_for_enum(enemy_type), phosg::JSON::list()).first->second->emplace_back(std::move(this_spec_json));
                }
              }
            }
          }

          for (size_t area = 0; area < 0x12; area++) {
            auto area_list = phosg::JSON::list();

            for (const auto& spec : this->get_box_specs(GameMode::NORMAL, episode, difficulty, section_id, area)) {
              if (spec.data.empty()) {
                continue;
              }
              auto frac = phosg::reduce_fraction<uint64_t>(spec.probability, 0x100000000);
              auto spec_json = phosg::JSON::list({phosg::string_printf("%" PRIu64 "/%" PRIu64, frac.first, frac.second)});
              if (spec.data.can_be_encoded_in_rel_rare_table()) {
                spec_json.emplace_back((spec.data.data1[0] << 16) | (spec.data.data1[1] << 8) | spec.data.data1[2]);
              } else {
                spec_json.emplace_back(spec.data.short_hex());
              }
              if (name_index) {
                spec_json.emplace_back(name_index->describe_item(spec.data));
              }
              area_list.emplace_back(std::move(spec_json));
            }

            if (!area_list.empty()) {
              collection_dict.emplace(
                  phosg::string_printf("Box-%s", name_for_floor(episode, area)),
                  std::move(area_list));
            }
          }

          if (!collection_dict.empty()) {
            section_id_dict.emplace(name_for_section_id(section_id), std::move(collection_dict));
          }
        }
        difficulty_dict.emplace(token_name_for_difficulty(difficulty), std::move(section_id_dict));
      }
      episodes_dict.emplace(token_name_for_episode(episode), std::move(difficulty_dict));
    }
    modes_dict.emplace(name_for_mode(mode), std::move(episodes_dict));
  }

  return modes_dict;
}

void RareItemSet::multiply_all_rates(double factor) {
  auto multiply_rates_vec = +[](vector<vector<ExpandedDrop>>& vec, double factor) -> void {
    for (auto& vec_it : vec) {
      for (auto& z_it : vec_it) {
        uint64_t new_probability = z_it.probability * factor;
        z_it.probability = min<uint64_t>(new_probability, 0xFFFFFFFF);
      }
    }
  };
  for (auto& coll_it : this->collections) {
    multiply_rates_vec(coll_it.second.rt_index_to_specs, factor);
    multiply_rates_vec(coll_it.second.box_area_to_specs, factor);
  }
}

void RareItemSet::print_collection(
    FILE* stream,
    GameMode mode,
    Episode episode,
    uint8_t difficulty,
    uint8_t section_id,
    shared_ptr<const ItemNameIndex> name_index) const {
  const SpecCollection* collection;
  try {
    collection = &this->get_collection(mode, episode, difficulty, section_id);
  } catch (const out_of_range&) {
    return;
  }

  fprintf(stream, "%s %s %s %s\n",
      name_for_mode(mode),
      name_for_episode(episode),
      name_for_difficulty(difficulty),
      name_for_section_id(section_id));

  fprintf(stream, "  Monster rares:\n");
  for (size_t z = 0; z < collection->rt_index_to_specs.size(); z++) {
    string enemy_types_str;
    const auto& enemy_types = enemy_types_for_rare_table_index(episode, z);
    for (EnemyType enemy_type : enemy_types) {
      enemy_types_str += phosg::name_for_enum(enemy_type);
      enemy_types_str.push_back(',');
    }
    if (!enemy_types_str.empty()) {
      enemy_types_str.resize(enemy_types_str.size() - 1);
    }

    for (const auto& spec : collection->rt_index_to_specs[z]) {
      string s = name_index ? spec.str(name_index) : spec.str();
      fprintf(stream, "    %02zX: %s (%s)\n", z, s.c_str(), enemy_types_str.c_str());
    }
  }

  fprintf(stream, "  Box rares:\n");
  for (size_t area = 0; area < collection->box_area_to_specs.size(); area++) {
    for (const auto& spec : collection->box_area_to_specs[area]) {
      string s = name_index ? spec.str(name_index) : spec.str();
      fprintf(stream, "    (area %02zX) %s\n", area, s.c_str());
    }
  }
}

void RareItemSet::print_all_collections(FILE* stream, std::shared_ptr<const ItemNameIndex> name_index) const {
  static const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  static const array<Episode, 3> episodes = {Episode::EP1, Episode::EP2, Episode::EP4};
  for (GameMode mode : modes) {
    for (Episode episode : episodes) {
      for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
        for (uint8_t section_id = 0; section_id < 10; section_id++) {
          try {
            this->print_collection(stream, mode, episode, difficulty, section_id, name_index);
          } catch (const out_of_range& e) {
          }
        }
      }
    }
  }
}

std::vector<RareItemSet::ExpandedDrop> RareItemSet::get_enemy_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t rt_index) const {
  try {
    return this->get_collection(mode, episode, difficulty, secid).rt_index_to_specs.at(rt_index);
  } catch (const out_of_range&) {
    static const std::vector<ExpandedDrop> empty_vector;
    return empty_vector;
  }
}

std::vector<RareItemSet::ExpandedDrop> RareItemSet::get_box_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const {
  try {
    return this->get_collection(mode, episode, difficulty, secid).box_area_to_specs.at(area);
  } catch (const out_of_range&) {
    static const std::vector<ExpandedDrop> empty_vector;
    return empty_vector;
  }
}

const RareItemSet::SpecCollection& RareItemSet::get_collection(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid) const {
  return this->collections.at(this->key_for_params(mode, episode, difficulty, secid));
}

uint16_t RareItemSet::key_for_params(GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid) {
  if (difficulty > 3) {
    throw logic_error("incorrect difficulty");
  }
  if (secid > 10) {
    throw logic_error("incorrect section id");
  }

  uint16_t key = ((difficulty & 3) << 4) | (secid & 0x0F);
  switch (mode) {
    case GameMode::NORMAL:
      break;
    case GameMode::BATTLE:
      key |= 0x0040;
      break;
    case GameMode::CHALLENGE:
      key |= 0x0080;
      break;
    case GameMode::SOLO:
      key |= 0x00C0;
      break;
    default:
      throw logic_error("invalid episode in RareItemSet");
  }
  switch (episode) {
    case Episode::EP1:
      break;
    case Episode::EP2:
      key |= 0x0100;
      break;
    case Episode::EP4:
      key |= 0x0200;
      break;
    default:
      throw logic_error("invalid episode in RareItemSet");
  }
  return key;
}
