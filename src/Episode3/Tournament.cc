#include "Tournament.hh"

#include <phosg/Random.hh>

#include "../CommandFormats.hh"
#include "../SendCommands.hh"

using namespace std;

namespace Episode3 {

Tournament::PlayerEntry::PlayerEntry(uint32_t serial_number)
    : serial_number(serial_number),
      com_deck() {}

Tournament::PlayerEntry::PlayerEntry(
    shared_ptr<const COMDeckDefinition> com_deck)
    : serial_number(0),
      com_deck(com_deck) {}

bool Tournament::PlayerEntry::is_com() const {
  return (this->com_deck != nullptr);
}

bool Tournament::PlayerEntry::is_human() const {
  return (this->serial_number != 0);
}

Tournament::Team::Team(
    shared_ptr<Tournament> tournament, size_t index, size_t max_players)
    : tournament(tournament),
      index(index),
      max_players(max_players),
      name(""),
      password(""),
      num_rounds_cleared(0),
      is_active(true) {}

string Tournament::Team::str() const {
  size_t num_human_players = 0;
  size_t num_com_players = 0;
  for (const auto& player : this->players) {
    num_human_players += player.is_human();
    num_com_players += player.is_com();
  }

  string ret = string_printf("[Team/%zu %s %zuH/%zuC/%zuP name=%s pass=%s rounds=%zu",
      this->index, this->is_active ? "active" : "inactive",
      num_human_players, num_com_players, this->max_players, this->name.c_str(),
      this->password.c_str(), this->num_rounds_cleared);
  for (const auto& player : this->players) {
    if (player.is_human()) {
      ret += string_printf(" %08" PRIX32, player.serial_number);
    }
  }
  return ret + "]";
}

void Tournament::Team::register_player(
    uint32_t serial_number,
    const string& team_name,
    const string& password) {
  if (this->players.size() >= this->max_players) {
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

  for (const auto& player : this->players) {
    if (player.is_human() && (player.serial_number == serial_number)) {
      throw logic_error("player already registered in team but not in tournament");
    }
  }

  this->players.emplace_back(serial_number);

  if (this->name.empty()) {
    this->name = team_name;
    this->password = password;
  }
}

bool Tournament::Team::unregister_player(uint32_t serial_number) {
  size_t index;
  for (index = 0; index < this->players.size(); index++) {
    if (this->players[index].is_human() &&
        (this->players[index].serial_number == serial_number)) {
      break;
    }
  }

  if (index < this->players.size()) {
    this->players.erase(this->players.begin() + index);

    if (this->players.empty()) {
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

bool Tournament::Team::has_any_human_players() const {
  for (const auto& player : this->players) {
    if (player.is_human()) {
      return true;
    }
  }
  return false;
}

size_t Tournament::Team::num_human_players() const {
  size_t ret = 0;
  for (const auto& player : this->players) {
    ret += player.is_human();
  }
  return ret;
}

size_t Tournament::Team::num_com_players() const {
  size_t ret = 0;
  for (const auto& player : this->players) {
    ret += player.is_com();
  }
  return ret;
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
      round_num(0) {}

string Tournament::Match::str() const {
  string winner_str = this->winner_team ? this->winner_team->str() : "(none)";
  return string_printf("[Match round=%zu winner=%s]", this->round_num, winner_str.c_str());
}

bool Tournament::Match::resolve_if_no_human_players() {
  if (this->winner_team) {
    return true;
  }
  // If both matches before this one are resolved and neither winner team has
  // any humans on it, skip this match entirely and just make one team advance
  // arbitrarily
  if (this->preceding_a->winner_team &&
      this->preceding_b->winner_team &&
      !this->preceding_a->winner_team->has_any_human_players() &&
      !this->preceding_b->winner_team->has_any_human_players()) {
    this->set_winner_team((random_object<uint8_t>() & 1)
            ? this->preceding_b->winner_team
            : this->preceding_a->winner_team);
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
  if (following && !following->resolve_if_no_human_players()) {
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
    shared_ptr<const MapIndex> map_index,
    shared_ptr<const COMDeckIndex> com_deck_index,
    uint8_t number,
    const string& name,
    shared_ptr<const MapIndex::MapEntry> map,
    const Rules& rules,
    size_t num_teams,
    bool is_2v2)
    : log(string_printf("[Tournament/%02hhX] ", number)),
      map_index(map_index),
      com_deck_index(com_deck_index),
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
    shared_ptr<const MapIndex> map_index,
    shared_ptr<const COMDeckIndex> com_deck_index,
    uint8_t number,
    const JSON& json)
    : log(string_printf("[Tournament/%02hhX] ", number)),
      map_index(map_index),
      com_deck_index(com_deck_index),
      source_json(json),
      number(number),
      current_state(State::REGISTRATION) {}

void Tournament::init() {
  vector<size_t> team_index_to_rounds_cleared;

  bool is_registration_complete;
  if (!this->source_json.is_null()) {
    this->name = this->source_json.get_string("name");
    this->map = this->map_index->definition_for_number(this->source_json.get_int("map_number"));
    this->rules = Rules(this->source_json.at("rules"));
    this->is_2v2 = this->source_json.get_bool("is_2v2");
    is_registration_complete = this->source_json.get_bool("is_registration_complete");

    for (const auto& team_json : this->source_json.get_list("teams")) {
      auto& team = this->teams.emplace_back(new Team(
          this->shared_from_this(),
          this->teams.size(),
          team_json->get_int("max_players")));
      team->name = team_json->get_string("name");
      team->password = team_json->get_string("password");
      team_index_to_rounds_cleared.emplace_back(team_json->get_int("num_rounds_cleared"));
      for (const auto& player_json : team_json->get_list("player_specs")) {
        if (player_json->is_int()) {
          team->players.emplace_back(player_json->as_int());
          this->all_player_serial_numbers.emplace(player_json->as_int());
        } else {
          team->players.emplace_back(this->com_deck_index->deck_for_name(player_json->as_string()));
        }
      }
    }
    this->num_teams = this->teams.size();

    this->source_json = nullptr;

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
      next_round_matches.emplace_back(std::move(m));
    }
    current_round_matches = std::move(next_round_matches);
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
      size_t& a_rounds_cleared = team_index_to_rounds_cleared[match->preceding_a->winner_team->index];
      size_t& b_rounds_cleared = team_index_to_rounds_cleared[match->preceding_b->winner_team->index];
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

JSON Tournament::json() const {
  auto teams_list = JSON::list();
  for (auto team : this->teams) {
    auto players_list = JSON::list();
    for (const auto& player : team->players) {
      if (player.is_human()) {
        players_list.emplace_back(player.serial_number);
      } else {
        players_list.emplace_back(player.com_deck->deck_name);
      }
    }
    teams_list.emplace_back(JSON::dict({
        {"max_players", team->max_players},
        {"player_specs", std::move(players_list)},
        {"name", team->name},
        {"password", team->password},
        {"num_rounds_cleared", team->num_rounds_cleared},
    }));
  }
  return JSON::dict({
      {"name", this->name},
      {"map_number", this->map->map.map_number.load()},
      {"rules", this->rules.json()},
      {"is_2v2", this->is_2v2},
      {"is_registration_complete", (this->current_state != State::REGISTRATION)},
      {"teams", std::move(teams_list)},
  });
}

uint8_t Tournament::get_number() const {
  return this->number;
}

const string& Tournament::get_name() const {
  return this->name;
}

shared_ptr<const MapIndex::MapEntry> Tournament::get_map() const {
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
    for (const auto& player : team->players) {
      if (player.serial_number == serial_number) {
        return team->is_active ? team : nullptr;
      }
    }
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
    for (const auto& player : t->players) {
      if (player.is_com()) {
        throw logic_error("non-human player on team before tournament start");
      }
    }
    if (this->com_deck_index->num_decks() < t->max_players - t->players.size()) {
      throw runtime_error("not enough COM decks to complete team");
    }
    // TODO: Don't allow duplicate COM decks, nor duplicate COM SCs on the same
    // team
    while (t->players.size() < t->max_players) {
      t->players.emplace_back(this->com_deck_index->random_deck());
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
    shared_ptr<const MapIndex> map_index,
    shared_ptr<const COMDeckIndex> com_deck_index,
    const string& state_filename,
    bool skip_load_state)
    : map_index(map_index),
      com_deck_index(com_deck_index),
      state_filename(state_filename) {
  if (this->state_filename.empty() || skip_load_state) {
    return;
  }

  auto json = JSON::parse(load_file(this->state_filename));
  if (json.size() > 0x20) {
    throw runtime_error("tournament JSON list length is incorrect");
  }
  for (size_t z = 0; z < min<size_t>(json.size(), 0x20); z++) {
    if (!json.at(z).is_null()) {
      this->tournaments[z].reset(new Tournament(this->map_index, this->com_deck_index, z, json.at(z)));
      this->tournaments[z]->init();
    }
  }
}

void TournamentIndex::save() const {
  if (this->state_filename.empty()) {
    return;
  }

  auto list = JSON::list();
  for (size_t z = 0; z < 0x20; z++) {
    if (this->tournaments[z]) {
      list.emplace_back(this->tournaments[z]->json());
    } else {
      list.emplace_back(nullptr);
    }
  }
  save_file(this->state_filename, list.serialize(JSON::SerializeOption::FORMAT));
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
    shared_ptr<const MapIndex::MapEntry> map,
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
      this->map_index, this->com_deck_index, number, name, map, rules, num_teams, is_2v2);
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
