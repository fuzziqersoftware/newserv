#include "Tournament.hh"

#include <phosg/Random.hh>

#include "../CommandFormats.hh"
#include "../SendCommands.hh"

using namespace std;

namespace Episode3 {



Tournament::Team::Team(
    shared_ptr<Tournament> tournament, size_t index, size_t max_players)
  : tournament(tournament),
    index(index),
    max_players(max_players),
    name(""),
    password(""),
    num_rounds_cleared(0),
    is_active(true) { }

string Tournament::Team::str() const {
  string ret = string_printf("[Team/%zu %s %zu/%zuP name=%s pass=%s rounds=%zu",
      this->index, this->is_active ? "active" : "inactive",
      this->player_serial_numbers.size(), this->max_players, this->name.c_str(),
      this->password.c_str(), this->num_rounds_cleared);
  for (uint32_t serial_number : this->player_serial_numbers) {
    ret += string_printf(" %08" PRIX32, serial_number);
  }
  return ret + "]";
}

void Tournament::Team::register_player(
    uint32_t serial_number,
    const string& team_name,
    const string& password) {
  if (this->player_serial_numbers.size() >= this->max_players) {
    throw runtime_error("team is full");
  }

  if (!this->name.empty() && (password != this->password)) {
    throw runtime_error("incorrect password");
  }

  auto tournament = this->tournament.lock();
  if (!tournament) {
    throw runtime_error("tournament has been deleted");
  }
  if (!tournament->all_player_serial_numbers.emplace(serial_number).second) {
    throw runtime_error("player already registered in same tournament");
  }

  if (!this->player_serial_numbers.emplace(serial_number).second) {
    throw logic_error("player already registered in team but not in tournament");
  }

  if (this->name.empty()) {
    this->name = team_name;
    this->password = password;
  }
}

bool Tournament::Team::unregister_player(uint32_t serial_number) {
  if (this->player_serial_numbers.erase(serial_number)) {
    if (this->player_serial_numbers.empty()) {
      this->name.clear();
      this->password.clear();
    }

    auto tournament = this->tournament.lock();
    if (!tournament) {
      return false;
    }

    // If the tournament has already started, make the team forfeit their game.
    // If any player withdraws from a team after the registration phase, the
    // entire team essentially forfeits their entry.
    if (tournament->get_state() != Tournament::State::REGISTRATION) {
      // Look through the pending matches to see if this team is involved in any
      // of them
      for (auto match : tournament->pending_matches) {
        if (!match->preceding_a || !match->preceding_b) {
          throw logic_error("zero-round match is pending after tournament registration phase");
        }
        if (match->preceding_a->winner_team.get() == this) {
          match->set_winner_team(match->preceding_b->winner_team);
          break;
        } else if (match->preceding_b->winner_team.get() == this) {
          match->set_winner_team(match->preceding_a->winner_team);
          break;
        }
      }

    // If the tournament has not started yet, just remove the player from the
    // team
    } else {
      if (!tournament->all_player_serial_numbers.erase(serial_number)) {
        throw logic_error("player removed from team but not from tournament");
      }
    }

    return true;

  } else {
    return false;
  }
}



Tournament::Match::Match(
    shared_ptr<Tournament> tournament,
    shared_ptr<Match> preceding_a,
    shared_ptr<Match> preceding_b)
  : tournament(tournament),
    preceding_a(preceding_a),
    preceding_b(preceding_b),
    winner_team(nullptr),
    round_num(0) {
  if (this->preceding_a->round_num != this->preceding_b->round_num) {
    throw logic_error("preceding matches have different round numbers");
  }
  this->round_num = this->preceding_a->round_num + 1;
}

Tournament::Match::Match(
    shared_ptr<Tournament> tournament,
    shared_ptr<Team> winner_team)
  : tournament(tournament),
    preceding_a(nullptr),
    preceding_b(nullptr),
    winner_team(winner_team),
    round_num(0) { }

string Tournament::Match::str() const {
  string winner_str = this->winner_team ? this->winner_team->str() : "(none)";
  return string_printf("[Match round=%zu winner=%s]", this->round_num, winner_str.c_str());
}

bool Tournament::Match::resolve_if_no_players() {
  if (this->winner_team) {
    return true;
  }
  // If both matches before this one are resolved and neither winner team has
  // any humans on it, skip this match entirely and just make one team advance
  // arbitrarily
  if (this->preceding_a->winner_team &&
      this->preceding_b->winner_team &&
      this->preceding_a->winner_team->player_serial_numbers.empty() &&
      this->preceding_b->winner_team->player_serial_numbers.empty()) {
    this->set_winner_team((random_object<uint8_t>() & 1)
        ? this->preceding_b->winner_team : this->preceding_a->winner_team);
    return true;
  } else {
    return false;
  }
}

void Tournament::Match::on_winner_team_set() {
  auto tournament = this->tournament.lock();
  if (!tournament) {
    return;
  }

  tournament->pending_matches.erase(this->shared_from_this());

  // Resolve the following match if possible (this skips CPU-only matches). If
  // the following match can't be resolved, mark it pending.
  auto following = this->following.lock();
  if (following && !following->resolve_if_no_players()) {
    tournament->pending_matches.emplace(following);
  }

  // If there are no pending matches, then the tournament is complete
  if (tournament->pending_matches.empty()) {
    tournament->current_state = Tournament::State::COMPLETE;
  }
}

void Tournament::Match::set_winner_team_without_triggers(shared_ptr<Team> team) {
  if (!this->preceding_a || !this->preceding_b) {
    throw logic_error("set_winner_team called on zero-round match");
  }
  if ((team != this->preceding_a->winner_team) &&
      (team != this->preceding_b->winner_team)) {
    throw logic_error("winner team did not participate in match");
  }

  this->winner_team = team;

  this->winner_team->num_rounds_cleared++;
  if (this->winner_team == this->preceding_a->winner_team) {
    this->preceding_b->winner_team->is_active = false;
  } else {
    this->preceding_a->winner_team->is_active = false;
  }
}

void Tournament::Match::set_winner_team(shared_ptr<Team> team) {
  this->set_winner_team_without_triggers(team);
  this->on_winner_team_set();
}

shared_ptr<Tournament::Team> Tournament::Match::opponent_team_for_team(
    shared_ptr<Team> team) const {
  if (!this->preceding_a || !this->preceding_b) {
    throw logic_error("zero-round matches do not have opponents");
  }
  if (team == this->preceding_a->winner_team) {
    return this->preceding_b->winner_team;
  } else if (team == this->preceding_b->winner_team) {
    return this->preceding_a->winner_team;
  } else {
    throw logic_error("team is not registered for this match");
  }
}



Tournament::Tournament(
    shared_ptr<const DataIndex> data_index,
    uint8_t number,
    const string& name,
    shared_ptr<const DataIndex::MapEntry> map,
    const Rules& rules,
    size_t num_teams,
    bool is_2v2)
  : log(string_printf("[Tournament/%02hhX] ", number)),
    data_index(data_index),
    number(number),
    name(name),
    map(map),
    rules(rules),
    num_teams(num_teams),
    is_2v2(is_2v2),
    current_state(State::REGISTRATION) {
  if (this->num_teams < 4) {
    throw invalid_argument("team count must be 4 or more");
  }
  if (this->num_teams > 32) {
    throw invalid_argument("team count must be 32 or fewer");
  }
  if (this->num_teams & (this->num_teams - 1)) {
    throw invalid_argument("team count must be a power of 2");
  }
}

Tournament::Tournament(
    std::shared_ptr<const DataIndex> data_index,
    uint8_t number,
    std::shared_ptr<const JSONObject> json)
  : log(string_printf("[Tournament/%02hhX] ", number)),
    data_index(data_index),
    source_json(json),
    number(number),
    current_state(State::REGISTRATION) { }

void Tournament::init() {
  vector<size_t> team_index_to_rounds_cleared;

  bool is_registration_complete;
  if (this->source_json) {
    auto& dict = this->source_json->as_dict();
    this->name = dict.at("name")->as_string();
    this->map = this->data_index->definition_for_map_number(dict.at("map_number")->as_int());
    this->rules = Rules(dict.at("rules"));
    this->is_2v2 = dict.at("is_2v2")->as_bool();
    is_registration_complete = dict.at("is_registration_complete")->as_bool();

    for (const auto& team_json : dict.at("teams")->as_list()) {
      auto& team_dict = team_json->as_dict();
      auto& team = this->teams.emplace_back(new Team(
          this->shared_from_this(),
          this->teams.size(),
          team_dict.at("max_players")->as_int()));
      team->name = team_dict.at("name")->as_string();
      team->password = team_dict.at("password")->as_string();
      team_index_to_rounds_cleared.emplace_back(team_dict.at("num_rounds_cleared")->as_int());
      for (const auto& serial_number_json : team_dict.at("player_serial_numbers")->as_list()) {
        uint32_t serial_number = serial_number_json->as_int();
        team->player_serial_numbers.emplace(serial_number);
        this->all_player_serial_numbers.emplace(serial_number);
      }
      for (const auto& com_deck_name_json : team_dict.at("com_deck_names")->as_list()) {
        team->com_decks.emplace_back(this->data_index->com_deck(
            com_deck_name_json->as_string()));
      }
    }
    this->num_teams = this->teams.size();

    this->source_json.reset();

  } else {
    // Create empty teams
    while (this->teams.size() < this->num_teams) {
      auto t = make_shared<Team>(
          this->shared_from_this(), this->teams.size(), this->is_2v2 ? 2 : 1);
      this->teams.emplace_back(t);
    }
    is_registration_complete = false;
  }

  // Create the match structure
  while (this->zero_round_matches.size() < this->num_teams) {
    this->zero_round_matches.emplace_back(make_shared<Match>(
        this->shared_from_this(), this->teams[this->zero_round_matches.size()]));
  }

  // Create the bracket matches
  vector<shared_ptr<Match>> current_round_matches = this->zero_round_matches;
  while (current_round_matches.size() > 1) {
    vector<shared_ptr<Match>> next_round_matches;
    for (size_t z = 0; z < current_round_matches.size(); z += 2) {
      auto m = make_shared<Match>(
          this->shared_from_this(),
          current_round_matches[z],
          current_round_matches[z + 1]);
      current_round_matches[z]->following = m;
      current_round_matches[z + 1]->following = m;
      next_round_matches.emplace_back(move(m));
    }
    current_round_matches = move(next_round_matches);
  }
  this->final_match = current_round_matches.at(0);

  // Compute the match state from the teams' states
  if (is_registration_complete) {
    this->current_state = State::IN_PROGRESS;

    // Start with all first-round matches in the match queue
    unordered_set<shared_ptr<Match>> match_queue;
    for (auto match : this->zero_round_matches) {
      match_queue.emplace(match->following.lock());
    }
    if (match_queue.count(nullptr)) {
      throw logic_error("null match in match queue");
    }

    // For each match in the queue, either resolve it from the previous state or
    // mark it as unresolvable (hence it should be pending when we're done)
    while (!match_queue.empty()) {
      auto match_it = match_queue.begin();
      auto match = *match_it;
      match_queue.erase(match_it);

      if (!match->preceding_a->winner_team || !match->preceding_b->winner_team) {
        throw logic_error("preceding matches are not resolved");
      }
      size_t& a_rounds_cleared = team_index_to_rounds_cleared[
          match->preceding_a->winner_team->index];
      size_t& b_rounds_cleared = team_index_to_rounds_cleared[
          match->preceding_b->winner_team->index];
      if (a_rounds_cleared && b_rounds_cleared) {
        throw runtime_error("both teams won the same match");
      }
      if (!a_rounds_cleared && !b_rounds_cleared) {
        this->pending_matches.emplace(match); // Neither team has won yet
      } else {
        if (a_rounds_cleared) {
          a_rounds_cleared--;
          match->set_winner_team_without_triggers(match->preceding_a->winner_team);
        } else {
          b_rounds_cleared--;
          match->set_winner_team_without_triggers(match->preceding_b->winner_team);
        }

        // If both preceding matches of the following match are resolved, put
        // the following match on the queue since it may be resolvable as well
        auto following = match->following.lock();
        if (following &&
            following->preceding_a->winner_team &&
            following->preceding_b->winner_team) {
          match_queue.emplace(following);
        }
      }
    }

    if (!this->final_match->winner_team == this->pending_matches.empty()) {
      throw logic_error("there must be pending matches if and only if the final match is not resolved");
    }

    // If all matches are resolved, then the tournament is complete
    if (this->final_match->winner_team) {
      this->current_state = State::COMPLETE;
    }

  } else {
    // Make all the zero round matches pending (this is needed so that start()
    // will auto-resolve all-CPU matches in the first round)
    for (auto m : this->zero_round_matches) {
      this->pending_matches.emplace(m);
    }

    this->current_state = State::REGISTRATION;
  }
}

std::shared_ptr<JSONObject> Tournament::json() const {
  unordered_map<string, shared_ptr<JSONObject>> dict;
  dict.emplace("name", make_json_str(this->name));
  dict.emplace("map_number", make_json_int(this->map->map.map_number));
  dict.emplace("rules", this->rules.json());
  dict.emplace("is_2v2", make_json_bool(this->is_2v2));
  dict.emplace("is_registration_complete", make_json_bool(
      this->current_state != State::REGISTRATION));

  vector<shared_ptr<JSONObject>> teams_list;
  for (auto team : this->teams) {
    unordered_map<string, shared_ptr<JSONObject>> team_dict;
    team_dict.emplace("max_players", make_json_int(team->max_players));
    vector<shared_ptr<JSONObject>> player_serial_numbers_list;
    for (uint32_t player_serial_number : team->player_serial_numbers) {
      player_serial_numbers_list.emplace_back(make_json_int(player_serial_number));
    }
    team_dict.emplace("player_serial_numbers", make_json_list(move(player_serial_numbers_list)));
    vector<shared_ptr<JSONObject>> com_deck_names_list;
    for (auto com_deck : team->com_decks) {
      com_deck_names_list.emplace_back(make_json_str(com_deck->deck_name));
    }
    team_dict.emplace("com_deck_names", make_json_list(move(com_deck_names_list)));
    team_dict.emplace("name", make_json_str(team->name));
    team_dict.emplace("password", make_json_str(team->password));
    team_dict.emplace("num_rounds_cleared", make_json_int(team->num_rounds_cleared));
    teams_list.emplace_back(new JSONObject(move(team_dict)));
  }
  dict.emplace("teams", make_json_list(move(teams_list)));
  return shared_ptr<JSONObject>(new JSONObject(move(dict)));
}

std::shared_ptr<const DataIndex> Tournament::get_data_index() const {
  return this->data_index;
}

uint8_t Tournament::get_number() const {
  return this->number;
}

const string& Tournament::get_name() const {
  return this->name;
}

shared_ptr<const DataIndex::MapEntry> Tournament::get_map() const {
  return this->map;
}

const Rules& Tournament::get_rules() const {
  return this->rules;
}

bool Tournament::get_is_2v2() const {
  return this->is_2v2;
}

Tournament::State Tournament::get_state() const {
  return this->current_state;
}

const vector<shared_ptr<Tournament::Team>>& Tournament::all_teams() const {
  return this->teams;
}

shared_ptr<Tournament::Team> Tournament::get_team(size_t index) const {
  return this->teams.at(index);
}

shared_ptr<Tournament::Team> Tournament::get_winner_team() const {
  if (this->current_state != State::COMPLETE) {
    return nullptr;
  }
  if (!this->final_match->winner_team) {
    throw logic_error("tournament is complete but winner is not set");
  }
  return this->final_match->winner_team;
}

shared_ptr<Tournament::Match> Tournament::next_match_for_team(
    shared_ptr<Team> team) const {
  if (this->current_state == Tournament::State::REGISTRATION) {
    return nullptr;
  }
  for (auto match : this->pending_matches) {
    if (!match->preceding_a || !match->preceding_b) {
      throw logic_error("zero-round match is pending after tournament registration phase");
    }
    if ((team == match->preceding_a->winner_team) ||
        (team == match->preceding_b->winner_team)) {
      return match;
    }
  }
  return nullptr;
}

shared_ptr<Tournament::Match> Tournament::get_final_match() const {
  return this->final_match;
}

shared_ptr<Tournament::Team> Tournament::team_for_serial_number(
    uint32_t serial_number) const {
  if (!this->all_player_serial_numbers.count(serial_number)) {
    return nullptr;
  }

  for (auto team : this->teams) {
    if (!team->player_serial_numbers.count(serial_number)) {
      continue;
    }
    if (!team->is_active) {
      return nullptr;
    }
    return team;
  }

  throw logic_error("serial number registered in tournament but not in any team");
}

const set<uint32_t>& Tournament::get_all_player_serial_numbers() const {
  return this->all_player_serial_numbers;
}

void Tournament::start() {
  if (this->current_state != State::REGISTRATION) {
    throw runtime_error("tournament has already started");
  }

  this->current_state = State::IN_PROGRESS;

  // Assign names to COM teams, and assign COM decks to all empty slots
  for (size_t z = 0; z < this->zero_round_matches.size(); z++) {
    auto m = this->zero_round_matches[z];
    auto t = m->winner_team;
    if (t->name.empty()) {
      t->name = string_printf("COM:%zu", z);
    }
    if (this->data_index->num_com_decks() < t->max_players - t->player_serial_numbers.size()) {
      throw runtime_error("not enough COM decks to complete team");
    }
    while (t->player_serial_numbers.size() + t->com_decks.size() < t->max_players) {
      t->com_decks.emplace_back(this->data_index->random_com_deck());
    }
  }

  // Resolve all possible CPU-only matches
  for (auto m : this->zero_round_matches) {
    m->on_winner_team_set();
  }
}

void Tournament::print_bracket(FILE* stream) const {
  function<void(shared_ptr<Match>, size_t)> print_match = [&](shared_ptr<Match> m, size_t indent_level) -> void {
    for (size_t z = 0; z < indent_level; z++) {
      fputc(' ', stream);
      fputc(' ', stream);
    }
    string match_str = m->str();
    fprintf(stream, "%s%s\n", match_str.c_str(), this->pending_matches.count(m) ? " (PENDING)" : "");
    if (m->preceding_a) {
      print_match(m->preceding_a, indent_level + 1);
    }
    if (m->preceding_b) {
      print_match(m->preceding_b, indent_level + 1);
    }
  };
  fprintf(stream, "Tournament %02hhX: %s\n", this->number, this->name.c_str());
  string map_name = this->map->map.name;
  fprintf(stream, "  Map: %08" PRIX32 " (%s)\n", this->map->map.map_number.load(), map_name.c_str());
  string rules_str = this->rules.str();
  fprintf(stream, "  Rules: %s\n", rules_str.c_str());
  fprintf(stream, "  Structure: %s, %zu entries\n", this->is_2v2 ? "2v2" : "1v1", this->num_teams);
  switch (this->current_state) {
    case State::REGISTRATION:
      fprintf(stream, "  State: REGISTRATION\n");
      break;
    case State::IN_PROGRESS:
      fprintf(stream, "  State: IN_PROGRESS\n");
      break;
    case State::COMPLETE:
      fprintf(stream, "  State: COMPLETE\n");
      break;
    default:
      fprintf(stream, "  State: UNKNOWN\n");
      break;
  }
  fprintf(stream, "  Standings:\n");
  print_match(this->final_match, 2);
  fprintf(stream, "  Pending matches:\n");
  for (const auto& match : this->pending_matches) {
    string match_str = match->str();
    fprintf(stream, "    %s\n", match_str.c_str());
  }
}



TournamentIndex::TournamentIndex(
    shared_ptr<const DataIndex> data_index,
    const string& state_filename,
    bool skip_load_state)
  : data_index(data_index), state_filename(state_filename) {
  if (this->state_filename.empty() || skip_load_state) {
    return;
  }

  auto json = JSONObject::parse(load_file(this->state_filename));

  auto& list = json->as_list();
  if (list.size() != 0x20) {
    throw runtime_error("tournament JSON list length is incorrect");
  }
  for (size_t z = 0; z < 0x20; z++) {
    if (!list.at(z)->is_null()) {
      this->tournaments[z].reset(new Tournament(this->data_index, z, list[z]));
      this->tournaments[z]->init();
    }
  }
}

void TournamentIndex::save() const {
  if (this->state_filename.empty()) {
    return;
  }

  vector<shared_ptr<JSONObject>> list;
  for (size_t z = 0; z < 0x20; z++) {
    if (this->tournaments[z]) {
      list.emplace_back(this->tournaments[z]->json());
    } else {
      list.emplace_back(make_json_null());
    }
  }
  auto json = make_json_list(move(list));
  save_file(this->state_filename, json->format());
}

vector<shared_ptr<Tournament>> TournamentIndex::all_tournaments() const {
  vector<shared_ptr<Tournament>> ret;
  for (size_t z = 0; z < 0x20; z++) {
    if (this->tournaments[z]) {
      ret.emplace_back(this->tournaments[z]);
    }
  }
  return ret;
}

shared_ptr<Tournament> TournamentIndex::create_tournament(
    const string& name,
    shared_ptr<const DataIndex::MapEntry> map,
    const Rules& rules,
    size_t num_teams,
    bool is_2v2) {
  // Find an unused tournament number
  uint8_t number;
  for (number = 0; number < 0x20; number++) {
    if (!this->tournaments[number]) {
      break;
    }
  }
  if (number >= 0x20) {
    throw runtime_error("all tournament slots are full");
  }

  auto t = make_shared<Tournament>(
      this->data_index, number, name, map, rules, num_teams, is_2v2);
  t->init();
  this->tournaments[number] = t;
  return t;
}

void TournamentIndex::delete_tournament(uint8_t number) {
  this->tournaments[number].reset();
}

shared_ptr<Tournament> TournamentIndex::get_tournament(uint8_t number) const {
  return this->tournaments[number];
}

shared_ptr<Tournament> TournamentIndex::get_tournament(const string& name) const {
  for (size_t z = 0; z < 0x20; z++) {
    if (this->tournaments[z] && (this->tournaments[z]->get_name() == name)) {
      return this->tournaments[z];
    }
  }
  return nullptr;
}

shared_ptr<Tournament::Team> TournamentIndex::team_for_serial_number(
    uint32_t serial_number) const {
  for (size_t z = 0; z < 0x20; z++) {
    if (!this->tournaments[z]) {
      continue;
    }
    auto team = this->tournaments[z]->team_for_serial_number(serial_number);
    if (team) {
      return team;
    }
  }
  return nullptr;
}



} // namespace Episode3
