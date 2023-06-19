#include "RareItemSet.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Random.hh>

#include "BattleParamsIndex.hh"
#include "StaticGameData.hh"

using namespace std;

uint32_t RareItemSet::PackedDrop::expand_rate(uint8_t pc) {
  int8_t shift = ((pc >> 3) & 0x1F) - 4;
  if (shift < 0) {
    shift = 0;
  }
  return ((2 << shift) * ((pc & 7) + 7));
}

RareItemSet::ExpandedDrop::ExpandedDrop() : probability(0) {
  this->item_code[0] = 0;
  this->item_code[1] = 0;
  this->item_code[2] = 0;
}

RareItemSet::ExpandedDrop::ExpandedDrop(const PackedDrop& d)
    : probability(PackedDrop::expand_rate(d.probability)) {
  this->item_code[0] = d.item_code[0];
  this->item_code[1] = d.item_code[1];
  this->item_code[2] = d.item_code[2];
}

std::vector<RareItemSet::ExpandedDrop> GSLRareItemSet::Table::get_enemy_specs(uint8_t enemy_type) const {
  vector<ExpandedDrop> ret;
  if (this->monster_rares[enemy_type].item_code[0] != 0 ||
      this->monster_rares[enemy_type].item_code[1] != 0 ||
      this->monster_rares[enemy_type].item_code[2] != 0) {
    ret.emplace_back(this->monster_rares[enemy_type]);
  }
  return ret;
}

std::vector<RareItemSet::ExpandedDrop> GSLRareItemSet::Table::get_box_specs(uint8_t area) const {
  vector<ExpandedDrop> ret;
  for (size_t z = 0; z < 0x1E; z++) {
    if (this->box_areas[z] == area) {
      ret.emplace_back(this->box_rares[z]);
    }
  }
  return ret;
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

GSLRareItemSet::GSLRareItemSet(shared_ptr<const string> data, bool is_big_endian)
    : gsl(data, is_big_endian) {
  const array<Episode, 2> episodes = {Episode::EP1, Episode::EP2};
  const array<GameMode, 4> modes = {GameMode::NORMAL, GameMode::BATTLE, GameMode::CHALLENGE, GameMode::SOLO};
  for (GameMode mode : modes) {
    for (Episode episode : episodes) {
      for (size_t difficulty = 0; difficulty < 3; difficulty++) {
        for (size_t section_id = 0; section_id < 10; section_id++) {
          string filename = string_printf("ItemRT%s%s%c%1zu.rel",
              ((mode == GameMode::CHALLENGE) ? "c" : ""),
              ((episode == Episode::EP2) ? "l" : ""),
              tolower(abbreviation_for_difficulty(difficulty)), // One of "nhvu"
              section_id);
          auto entry = this->gsl.get(filename);
          if (entry.second < sizeof(Table)) {
            throw runtime_error(string_printf("table %s is too small", filename.c_str()));
          }
          this->tables.emplace(
              this->key_for_params(mode, episode, difficulty, section_id),
              reinterpret_cast<const Table*>(entry.first));
        }
      }
    }
  }
}

std::vector<RareItemSet::ExpandedDrop> GSLRareItemSet::get_enemy_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const {
  return this->tables.at(this->key_for_params(mode, episode, difficulty, secid))->get_enemy_specs(enemy_type);
}

std::vector<RareItemSet::ExpandedDrop> GSLRareItemSet::get_box_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const {
  return this->tables.at(this->key_for_params(mode, episode, difficulty, secid))->get_box_specs(area);
}

RELRareItemSet::RELRareItemSet(shared_ptr<const string> data) : data(data) {
  if (this->data->size() != sizeof(Table) * 10 * 4 * 3) {
    throw runtime_error("data file size is incorrect");
  }
}

std::vector<RareItemSet::ExpandedDrop> RELRareItemSet::get_enemy_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const {
  return this->get_table(mode, episode, difficulty, secid).get_enemy_specs(enemy_type);
}

std::vector<RareItemSet::ExpandedDrop> RELRareItemSet::get_box_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const {
  return this->get_table(mode, episode, difficulty, secid).get_box_specs(area);
}

const RELRareItemSet::Table& RELRareItemSet::get_table(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid) const {
  (void)mode; // TODO: Shouldn't we check for challenge mode somewhere?

  if (difficulty > 3) {
    throw logic_error("incorrect difficulty");
  }
  if (secid > 10) {
    throw logic_error("incorrect section id");
  }

  size_t ep_index;
  switch (episode) {
    case Episode::EP1:
      ep_index = 0;
      break;
    case Episode::EP2:
      ep_index = 1;
      break;
    case Episode::EP4:
      ep_index = 2;
      break;
    default:
      throw invalid_argument("incorrect episode");
  }

  const auto* tables = reinterpret_cast<const Table*>(this->data->data());
  return tables[(ep_index * 10 * 4) + (difficulty * 10) + secid];
}

JSONRareItemSet::JSONRareItemSet(std::shared_ptr<const JSONObject> json) {
  for (const auto& mode_it : json->as_dict()) {
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
          for (const auto& item_it : difficulty_it.second->as_dict()) {
            vector<ExpandedDrop>* target;
            if (starts_with(item_it.first, "Box-")) {
              uint8_t area = drop_area_for_name(item_it.first.substr(4));
              if (collection.box_area_to_specs.size() <= area) {
                collection.box_area_to_specs.resize(area + 1);
              }
              target = &collection.box_area_to_specs[area];
            } else {
              size_t index = static_cast<size_t>(enum_for_name<EnemyType>(item_it.first.c_str()));
              if (collection.enemy_type_to_specs.size() <= index) {
                collection.enemy_type_to_specs.resize(index + 1);
              }
              target = &collection.enemy_type_to_specs[index];
            }

            for (const auto& spec_json : item_it.second->as_list()) {
              auto& spec_list = spec_json->as_list();
              auto& d = target->emplace_back();
              d.probability = spec_list.at(0)->as_int();
              uint32_t item_code = spec_list.at(1)->as_int();
              d.item_code[0] = (item_code >> 16) & 0xFF;
              d.item_code[1] = (item_code >> 8) & 0xFF;
              d.item_code[2] = item_code & 0xFF;
            }
          }
        }
      }
    }
  }
}

std::vector<RareItemSet::ExpandedDrop> JSONRareItemSet::get_enemy_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t enemy_type) const {
  try {
    return this->collections.at(this->key_for_params(mode, episode, difficulty, secid)).enemy_type_to_specs.at(enemy_type);
  } catch (const out_of_range&) {
    static const std::vector<ExpandedDrop> empty_vector;
    return empty_vector;
  }
}

std::vector<RareItemSet::ExpandedDrop> JSONRareItemSet::get_box_specs(
    GameMode mode, Episode episode, uint8_t difficulty, uint8_t secid, uint8_t area) const {
  try {
    return this->collections.at(this->key_for_params(mode, episode, difficulty, secid)).box_area_to_specs.at(area);
  } catch (const out_of_range&) {
    static const std::vector<ExpandedDrop> empty_vector;
    return empty_vector;
  }
}
