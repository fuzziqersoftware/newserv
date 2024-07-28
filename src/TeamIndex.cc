#include "TeamIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Image.hh>
#include <phosg/Random.hh>

#include "BattleParamsIndex.hh"
#include "GVMEncoder.hh"
#include "ItemData.hh"
#include "Loggers.hh"
#include "StaticGameData.hh"

using namespace std;

TeamIndex::Team::Member::Member(const phosg::JSON& json)
    : flags(json.get_int("Flags", 0)),
      points(json.get_int("Points", 0)),
      name(json.get_string("Name", "")) {
  try {
    this->account_id = json.get_int("AccountID");
  } catch (const out_of_range&) {
    this->account_id = json.get_int("SerialNumber");
  }
}

phosg::JSON TeamIndex::Team::Member::json() const {
  return phosg::JSON::dict({
      {"AccountID", this->account_id},
      {"Flags", this->flags},
      {"Points", this->points},
      {"Name", this->name},
  });
}

uint32_t TeamIndex::Team::Member::privilege_level() const {
  if (this->check_flag(Member::Flag::IS_MASTER)) {
    return 0x40;
  } else if (this->check_flag(Member::Flag::IS_LEADER)) {
    return 0x30;
  } else {
    return 0x00;
  }
}

TeamIndex::Team::Team(uint32_t team_id) : Team() {
  this->team_id = team_id;
}

string TeamIndex::Team::json_filename() const {
  return phosg::string_printf("system/teams/%08" PRIX32 ".json", this->team_id);
}

string TeamIndex::Team::flag_filename() const {
  return phosg::string_printf("system/teams/%08" PRIX32 ".bmp", this->team_id);
}

void TeamIndex::Team::load_config() {
  auto json = phosg::JSON::parse(phosg::load_file(this->json_filename()));
  this->name = json.get_string("Name");
  this->spent_points = json.get_int("SpentPoints");
  this->points = 0;
  for (const auto& member_it : json.get_list("Members")) {
    Member m(*member_it);
    this->points += m.points;
    uint32_t account_id = m.account_id;
    if (m.check_flag(Member::Flag::IS_MASTER)) {
      this->master_account_id = account_id;
    }
    this->members.emplace(account_id, std::move(m));
  }
  try {
    for (const auto& it : json.get_list("RewardKeys")) {
      this->reward_keys.emplace(it->as_string());
    }
  } catch (const out_of_range&) {
  }
  this->reward_flags = json.get_int("RewardFlags");
}

void TeamIndex::Team::save_config() const {
  phosg::JSON members_json = phosg::JSON::list();
  for (const auto& it : this->members) {
    members_json.emplace_back(it.second.json());
  }
  phosg::JSON reward_keys_json = phosg::JSON::list();
  for (const auto& it : this->reward_keys) {
    reward_keys_json.emplace_back(it);
  }
  phosg::JSON root = phosg::JSON::dict({
      {"Name", this->name},
      {"SpentPoints", this->spent_points},
      {"Members", std::move(members_json)},
      {"RewardKeys", std::move(reward_keys_json)},
      {"RewardFlags", this->reward_flags},
  });
  phosg::save_file(this->json_filename(), root.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::HEX_INTEGERS | phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY));
}

void TeamIndex::Team::load_flag() {
  phosg::Image img(this->flag_filename());
  if (img.get_width() != 32 || img.get_height() != 32) {
    throw runtime_error("incorrect flag image dimensions");
  }
  this->flag_data.reset(new parray<le_uint16_t, 0x20 * 0x20>());
  for (size_t y = 0; y < 32; y++) {
    for (size_t x = 0; x < 32; x++) {
      this->flag_data->at(y * 0x20 + x) = encode_rgba8888_to_argb1555(img.read_pixel(x, y));
    }
  }
}

void TeamIndex::Team::save_flag() const {
  if (!this->flag_data) {
    return;
  }
  phosg::Image img(32, 32, true);
  for (size_t y = 0; y < 32; y++) {
    for (size_t x = 0; x < 32; x++) {
      img.write_pixel(x, y, decode_argb1555_to_rgba8888(this->flag_data->at(y * 0x20 + x)));
    }
  }
  img.save(this->flag_filename(), phosg::Image::Format::WINDOWS_BITMAP);
}

void TeamIndex::Team::delete_files() const {
  string json_filename = this->json_filename();
  string flag_filename = this->flag_filename();
  remove(json_filename.c_str());
  remove(flag_filename.c_str());
}

PSOBBTeamMembership TeamIndex::Team::membership_for_member(uint32_t account_id) const {
  const auto& m = this->members.at(account_id);

  PSOBBTeamMembership ret;
  ret.team_master_guild_card_number = this->master_account_id;
  ret.team_id = this->team_id;
  ret.unknown_a5 = 0;
  ret.unknown_a6 = 0;
  ret.privilege_level = m.privilege_level();
  ret.unknown_a7 = 0;
  ret.unknown_a8 = 0;
  ret.unknown_a9 = 0;
  ret.team_name.encode(this->name);
  if (this->flag_data) {
    ret.flag_data = *this->flag_data;
  } else {
    ret.flag_data.clear();
  }
  ret.reward_flags = this->reward_flags;
  return ret;
}

bool TeamIndex::Team::has_reward(const string& key) const {
  return this->reward_keys.count(key);
}

size_t TeamIndex::Team::num_members() const {
  return this->members.size();
}

size_t TeamIndex::Team::num_leaders() const {
  size_t count = 0;
  for (const auto& it : this->members) {
    if (it.second.check_flag(Member::Flag::IS_LEADER)) {
      count++;
    }
  }
  return count;
}

size_t TeamIndex::Team::max_members() const {
  if (this->check_reward_flag(RewardFlag::MEMBERS_100_LEADERS_10)) {
    return 100;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_70_LEADERS_8)) {
    return 70;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_40_LEADERS_5)) {
    return 40;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_20_LEADERS_3)) {
    return 20;
  } else {
    return 10;
  }
}

size_t TeamIndex::Team::max_leaders() const {
  if (this->check_reward_flag(RewardFlag::MEMBERS_100_LEADERS_10)) {
    return 10;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_70_LEADERS_8)) {
    return 8;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_40_LEADERS_5)) {
    return 5;
  } else if (this->check_reward_flag(RewardFlag::MEMBERS_20_LEADERS_3)) {
    return 3;
  } else {
    return 2;
  }
}

bool TeamIndex::Team::can_add_member() const {
  return this->num_members() < this->max_members();
}

bool TeamIndex::Team::can_promote_leader() const {
  return this->num_leaders() < this->max_leaders();
}

TeamIndex::Reward::Reward(uint32_t menu_item_id, const phosg::JSON& def_json)
    : menu_item_id(menu_item_id),
      key(def_json.get_string("Key")),
      name(def_json.get_string("Name")),
      description(def_json.get_string("Description")),
      is_unique(def_json.get_bool("IsUnique", true)),
      team_points(def_json.get_int("Points")) {
  try {
    for (const auto& it : def_json.get_list("PrerequisiteKeys")) {
      this->prerequisite_keys.emplace(it->as_string());
    }
  } catch (const out_of_range&) {
  }
  try {
    this->reward_flag = static_cast<Team::RewardFlag>(def_json.get_int("RewardFlag"));
  } catch (const out_of_range&) {
  }
  try {
    this->reward_item = ItemData::from_data(phosg::parse_data_string(def_json.get_string("RewardItem")));
  } catch (const out_of_range&) {
  }
}

TeamIndex::TeamIndex(const string& directory, const phosg::JSON& reward_defs_json)
    : directory(directory),
      next_team_id(1) {
  uint32_t reward_menu_item_id = 0;
  for (const auto& it : reward_defs_json.as_list()) {
    this->reward_defs.emplace_back(reward_menu_item_id++, *it);
  }

  if (!phosg::isdir(this->directory)) {
    mkdir(this->directory.c_str(), 0755);
    return;
  }
  for (const auto& filename : phosg::list_directory(this->directory)) {
    string file_path = this->directory + "/" + filename;
    if (filename == "base.json") {
      auto json = phosg::JSON::parse(phosg::load_file(file_path));
      this->next_team_id = json.get_int("NextTeamID");
    } else if (phosg::ends_with(filename, ".json")) {
      try {
        uint32_t team_id = stoul(filename.substr(0, filename.size() - 5), nullptr, 16);
        auto team = make_shared<Team>(team_id);
        team->load_config();
        try {
          team->load_flag();
        } catch (const exception& e) {
          static_game_data_log.warning("Failed to load flag for team %08" PRIX32 ": %s", team_id, e.what());
        }
        this->add_to_indexes(team);
        static_game_data_log.info("Indexed team %08" PRIX32 " (%s) (%zu members)", team_id, team->name.c_str(), team->num_members());
      } catch (const exception& e) {
        static_game_data_log.warning("Failed to index team from %s: %s", filename.c_str(), e.what());
      }
    }
  }
}

size_t TeamIndex::count() const {
  return this->id_to_team.size();
}

shared_ptr<const TeamIndex::Team> TeamIndex::get_by_id(uint32_t team_id) const {
  try {
    return this->id_to_team.at(team_id);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<const TeamIndex::Team> TeamIndex::get_by_name(const string& name) const {
  try {
    return this->name_to_team.at(name);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<const TeamIndex::Team> TeamIndex::get_by_account_id(uint32_t account_id) const {
  try {
    return this->account_id_to_team.at(account_id);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

vector<shared_ptr<const TeamIndex::Team>> TeamIndex::all() const {
  vector<shared_ptr<const Team>> ret;
  for (const auto& it : this->id_to_team) {
    ret.emplace_back(it.second);
  }
  return ret;
}

shared_ptr<const TeamIndex::Team> TeamIndex::create(const string& name, uint32_t master_account_id, const string& master_name) {
  auto team = make_shared<Team>(this->next_team_id++);
  phosg::save_file(this->directory + "/base.json", phosg::JSON::dict({{"NextTeamID", this->next_team_id}}).serialize());

  Team::Member m;
  m.account_id = master_account_id;
  m.flags = 0;
  m.points = 0;
  m.name = master_name;
  m.set_flag(Team::Member::Flag::IS_MASTER);
  team->members.emplace(master_account_id, std::move(m));
  team->name = name;

  team->save_config();
  this->add_to_indexes(team);
  return team;
}

void TeamIndex::disband(uint32_t team_id) {
  auto team = this->id_to_team.at(team_id);
  this->remove_from_indexes(team);
  team->delete_files();
}

void TeamIndex::rename(uint32_t team_id, const std::string& new_team_name) {
  auto team = this->id_to_team.at(team_id);
  if (!this->name_to_team.emplace(new_team_name, team).second) {
    throw runtime_error("team name is already in use");
  }
  this->name_to_team.erase(team->name);
  team->name = new_team_name;
  team->save_config();
}

void TeamIndex::add_member(uint32_t team_id, uint32_t account_id, const string& name) {
  auto team = this->id_to_team.at(team_id);
  if (!this->account_id_to_team.emplace(account_id, team).second) {
    throw runtime_error("user is already in a different team");
  }

  Team::Member m;
  m.account_id = account_id;
  m.flags = 0;
  m.points = 0;
  m.name = name;
  team->members.emplace(account_id, std::move(m));

  team->save_config();
}

void TeamIndex::remove_member(uint32_t account_id) {
  auto team_it = this->account_id_to_team.find(account_id);
  if (team_it == this->account_id_to_team.end()) {
    throw runtime_error("client is not in any team");
  }
  auto team = std::move(team_it->second);
  this->account_id_to_team.erase(team_it);
  team->members.erase(account_id);
  if (team->members.empty()) {
    this->disband(team->team_id);
  } else {
    team->save_config();
  }
}

void TeamIndex::update_member_name(uint32_t account_id, const std::string& name) {
  auto team = this->account_id_to_team.at(account_id);
  auto& m = team->members.at(account_id);
  m.name = name;
  team->save_config();
}

void TeamIndex::add_member_points(uint32_t account_id, uint32_t points) {
  auto team = this->account_id_to_team.at(account_id);
  auto& m = team->members.at(account_id);
  m.points += points;
  team->points += points;
  team->save_config();
}

void TeamIndex::set_flag_data(uint32_t team_id, const parray<le_uint16_t, 0x20 * 0x20>& flag_data) {
  auto team = this->id_to_team.at(team_id);
  team->flag_data.reset(new parray<le_uint16_t, 0x20 * 0x20>(flag_data));
  team->save_flag();
}

bool TeamIndex::promote_leader(uint32_t master_account_id, uint32_t leader_account_id) {
  auto team = this->account_id_to_team.at(master_account_id);
  auto& master_m = team->members.at(master_account_id);
  if (!master_m.check_flag(TeamIndex::Team::Member::Flag::IS_MASTER)) {
    throw runtime_error("incorrect master account ID");
  }
  auto& other_m = team->members.at(leader_account_id);

  if (other_m.check_flag(TeamIndex::Team::Member::Flag::IS_LEADER) || !team->can_promote_leader()) {
    return false;
  }
  other_m.set_flag(TeamIndex::Team::Member::Flag::IS_LEADER);
  team->save_config();
  return true;
}

bool TeamIndex::demote_leader(uint32_t master_account_id, uint32_t leader_account_id) {
  auto team = this->account_id_to_team.at(master_account_id);
  auto& master_m = team->members.at(master_account_id);
  if (!master_m.check_flag(TeamIndex::Team::Member::Flag::IS_MASTER)) {
    throw runtime_error("incorrect master account ID");
  }
  auto& other_m = team->members.at(leader_account_id);

  if (!other_m.check_flag(TeamIndex::Team::Member::Flag::IS_LEADER)) {
    return false;
  }
  other_m.clear_flag(TeamIndex::Team::Member::Flag::IS_LEADER);
  team->save_config();
  return true;
}

void TeamIndex::change_master(uint32_t master_account_id, uint32_t new_master_account_id) {
  auto team = this->account_id_to_team.at(master_account_id);
  auto& master_m = team->members.at(master_account_id);
  if (!master_m.check_flag(TeamIndex::Team::Member::Flag::IS_MASTER)) {
    throw runtime_error("incorrect master account ID");
  }
  auto& new_master_m = team->members.at(new_master_account_id);

  master_m.clear_flag(TeamIndex::Team::Member::Flag::IS_MASTER);
  master_m.set_flag(TeamIndex::Team::Member::Flag::IS_LEADER);
  new_master_m.clear_flag(TeamIndex::Team::Member::Flag::IS_LEADER);
  new_master_m.set_flag(TeamIndex::Team::Member::Flag::IS_MASTER);
  team->master_account_id = new_master_account_id;
  team->save_config();
}

void TeamIndex::buy_reward(uint32_t team_id, const string& key, uint32_t points, Team::RewardFlag reward_flag) {
  auto team = this->id_to_team.at(team_id);
  if (team->spent_points + points > team->points) {
    throw runtime_error("not enough points available");
  }
  team->reward_keys.emplace(key);
  team->spent_points += points;
  if (reward_flag != Team::RewardFlag::NONE) {
    team->set_reward_flag(reward_flag);
  }
  team->save_config();
}

void TeamIndex::add_to_indexes(shared_ptr<Team> team) {
  if (!this->id_to_team.emplace(team->team_id, team).second) {
    throw runtime_error("team ID is already in use");
  }
  if (!this->name_to_team.emplace(team->name, team).second) {
    this->id_to_team.erase(team->team_id);
    throw runtime_error("team name is already in use");
  }
  for (const auto& it : team->members) {
    if (!this->account_id_to_team.emplace(it.second.account_id, team).second) {
      static_game_data_log.warning("Serial number %08" PRIX32 " (%010" PRIu32 ") exists in multiple teams",
          it.second.account_id, it.second.account_id);
    }
  }
}

void TeamIndex::remove_from_indexes(shared_ptr<Team> team) {
  this->id_to_team.erase(team->team_id);
  this->name_to_team.erase(team->name);
  for (const auto& it : team->members) {
    this->account_id_to_team.erase(it.second.account_id);
  }
}
