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

TeamIndex::Team::Member::Member(const JSON& json)
    : serial_number(json.get_int("SerialNumber")),
      flags(json.get_int("Flags", 0)),
      points(json.get_int("Points", 0)),
      name(json.get_string("Name", "")) {}

JSON TeamIndex::Team::Member::json() const {
  return JSON::dict({
      {"SerialNumber", this->serial_number},
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
  return string_printf("system/teams/%08" PRIX32 ".json", this->team_id);
}

string TeamIndex::Team::flag_filename() const {
  return string_printf("system/teams/%08" PRIX32 ".bmp", this->team_id);
}

void TeamIndex::Team::load_config() {
  auto json = JSON::parse(load_file(this->json_filename()));
  this->name = json.get_string("Name");
  for (const auto& member_it : json.get_list("Members")) {
    Member m(*member_it);
    uint32_t serial_number = m.serial_number;
    this->members.emplace(serial_number, std::move(m));
  }
  this->reward_flags = json.get_int("RewardFlags");
}

void TeamIndex::Team::save_config() const {
  JSON members_json = JSON::list();
  for (const auto& it : this->members) {
    members_json.emplace_back(it.second.json());
  }
  JSON root = JSON::dict({
      {"Name", this->name},
      {"Members", std::move(members_json)},
      {"RewardFlags", this->reward_flags},
  });
  save_file(this->json_filename(), root.serialize(JSON::SerializeOption::FORMAT | JSON::SerializeOption::HEX_INTEGERS));
}

void TeamIndex::Team::load_flag() {
  Image img(this->flag_filename());
  if (img.get_width() != 32 || img.get_height() != 32) {
    throw runtime_error("incorrect flag image dimensions");
  }
  this->flag_data.reset(new parray<le_uint16_t, 0x20 * 0x20>());
  for (size_t y = 0; y < 32; y++) {
    for (size_t x = 0; x < 32; x++) {
      this->flag_data->at(y * 0x20 + x) = encode_rgbx8888_to_xrgb1555(img.read_pixel(x, y));
    }
  }
}

void TeamIndex::Team::save_flag() const {
  if (!this->flag_data) {
    return;
  }
  Image img(32, 32, false);
  for (size_t y = 0; y < 32; y++) {
    for (size_t x = 0; x < 32; x++) {
      img.write_pixel(x, y, decode_xrgb1555_to_rgba8888(this->flag_data->at(y * 0x20 + x)));
    }
  }
  img.save(this->flag_filename(), Image::Format::WINDOWS_BITMAP);
}

void TeamIndex::Team::delete_files() const {
  string json_filename = this->json_filename();
  string flag_filename = this->flag_filename();
  remove(json_filename.c_str());
  remove(flag_filename.c_str());
}

PSOBBTeamMembership TeamIndex::Team::membership_for_member(uint32_t serial_number) const {
  const auto& m = this->members.at(serial_number);

  PSOBBTeamMembership ret;
  ret.guild_card_number = serial_number;
  ret.team_id = this->team_id;
  ret.unknown_a4 = 0;
  ret.privilege_level = m.privilege_level();
  ret.unknown_a6 = 0;
  ret.unknown_a7 = 0;
  ret.unknown_a8 = 0;
  ret.unknown_a9 = 0;
  ret.team_name.encode("\tE" + this->name);
  if (this->flag_data) {
    ret.flag_data = *this->flag_data;
  } else {
    ret.flag_data.clear();
  }
  ret.reward_flags = this->reward_flags;
  return ret;
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

TeamIndex::TeamIndex(const string& directory)
    : directory(directory),
      next_team_id(1) {
  if (!isdir(this->directory)) {
    mkdir(this->directory.c_str(), 0755);
    return;
  }
  for (const auto& filename : list_directory(this->directory)) {
    string file_path = this->directory + "/" + filename;
    if (filename == "base.json") {
      auto json = JSON::parse(load_file(file_path));
      this->next_team_id = json.get_int("NextTeamID");
    }
    if (ends_with(filename, ".json")) {
      try {
        uint32_t team_id = stoul(filename.substr(0, filename.size() - 5), nullptr, 16);
        shared_ptr<Team> team(new Team(team_id));
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

shared_ptr<TeamIndex::Team> TeamIndex::get_by_id(uint32_t team_id) {
  try {
    return this->id_to_team.at(team_id);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<TeamIndex::Team> TeamIndex::get_by_name(const string& name) {
  try {
    return this->name_to_team.at(name);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

shared_ptr<TeamIndex::Team> TeamIndex::get_by_serial_number(uint32_t serial_number) {
  try {
    return this->serial_number_to_team.at(serial_number);
  } catch (const out_of_range&) {
    return nullptr;
  }
}

vector<shared_ptr<TeamIndex::Team>> TeamIndex::all() {
  vector<shared_ptr<Team>> ret;
  for (const auto& it : this->id_to_team) {
    ret.emplace_back(it.second);
  }
  return ret;
}

shared_ptr<TeamIndex::Team> TeamIndex::create(string& name, uint32_t master_serial_number, const string& master_name) {
  shared_ptr<Team> team(new Team(this->next_team_id++));
  save_file(this->directory + "/base.json", JSON::dict({{"NextTeamID", this->next_team_id}}).serialize());

  Team::Member m;
  m.serial_number = master_serial_number;
  m.flags = 0;
  m.points = 0;
  m.name = master_name;
  m.set_flag(Team::Member::Flag::IS_MASTER);
  team->members.emplace(master_serial_number, std::move(m));
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

void TeamIndex::add_member(uint32_t team_id, uint32_t serial_number, const string& name) {
  auto team = this->id_to_team.at(team_id);
  if (!this->serial_number_to_team.emplace(serial_number, team).second) {
    throw runtime_error("user is already in a different team");
  }

  Team::Member m;
  m.serial_number = serial_number;
  m.flags = 0;
  m.points = 0;
  m.name = name;
  team->members.emplace(serial_number, std::move(m));

  team->save_config();
}

void TeamIndex::remove_member(uint32_t serial_number) {
  auto team_it = this->serial_number_to_team.find(serial_number);
  if (team_it == this->serial_number_to_team.end()) {
    throw runtime_error("client is not in any team");
  }
  auto team = std::move(team_it->second);
  this->serial_number_to_team.erase(team_it);
  team->members.erase(serial_number);
  if (team->members.empty()) {
    this->disband(team->team_id);
  } else {
    team->save_config();
  }
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
    if (!this->serial_number_to_team.emplace(it.second.serial_number, team).second) {
      static_game_data_log.warning("Serial number %08" PRIX32 " (%010" PRIu32 ") exists in multiple teams",
          it.second.serial_number, it.second.serial_number);
    }
  }
}

void TeamIndex::remove_from_indexes(shared_ptr<Team> team) {
  this->id_to_team.erase(team->team_id);
  this->name_to_team.erase(team->name);
  for (const auto& it : team->members) {
    this->serial_number_to_team.erase(it.second.serial_number);
  }
}
