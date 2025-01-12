#include "Tournament.hh"

#include <phosg/Random.hh>

#include "../CommandFormats.hh"
#include "../SendCommands.hh"

using namespace std;

namespace Episode3 {

Tournament::PlayerEntry::PlayerEntry(uint32_t account_id, const string& player_name)
    : account_id(account_id),
      player_name(player_name) {}

Tournament::PlayerEntry::PlayerEntry(shared_ptr<Client> c)
    : account_id(c->login->account->account_id),
      client(c),
      player_name(c->character()->disp.name.decode(c->language())) {}

Tournament::PlayerEntry::PlayerEntry(
    shared_ptr<const COMDeckDefinition> com_deck)
    : account_id(0),
      com_deck(com_deck) {}

bool Tournament::PlayerEntry::is_com() const {
  return (this->com_deck != nullptr);
}

bool Tournament::PlayerEntry::is_human() const {
  return (this->account_id != 0);
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

  string ret = phosg::string_printf("[Team/%zu %s %zuH/%zuC/%zuP name=%s pass=%s rounds=%zu",
      this->index, this->is_active ? "active" : "inactive",
      num_human_players, num_com_players, this->max_players, this->name.c_str(),
      this->password.c_str(), this->num_rounds_cleared);
  for (const auto& player : this->players) {
    if (player.is_human()) {
      if (player.player_name.empty()) {
        ret += phosg::string_printf(" %08" PRIX32, player.account_id);
      } else {
        ret += phosg::string_printf(" %08" PRIX32 " (%s)", player.account_id, player.player_name.c_str());
      }
    }
  }
  return ret + "]";
}

void Tournament::Team::register_player(
    shared_ptr<Client> c,
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
  if (!tournament->all_player_account_ids.emplace(c->login->account->account_id).second) {
    throw runtime_error("player already registered in same tournament");
  }

  for (const auto& player : this->players) {
    if (player.is_human() && (player.account_id == c->login->account->account_id)) {
      throw logic_error("player already registered in team but not in tournament");
    }
  }

  this->players.emplace_back(c);

  if (this->name.empty()) {
    this->name = team_name;
    this->password = password;
  }
}

bool Tournament::Team::unregister_player(uint32_t account_id) {
  size_t index;
  for (index = 0; index < this->players.size(); index++) {
    if (this->players[index].is_human() &&
        (this->players[index].account_id == account_id)) {
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
      if (!tournament->all_player_account_ids.erase(account_id)) {
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
  return phosg::string_printf("[Match round=%zu winner=%s]", this->round_num, winner_str.c_str());
}

bool Tournament::Match::resolve_if_skippable() {
  if (this->winner_team) {
    return true;
  }

  auto winner_a = this->preceding_a->winner_team;
  auto winner_b = this->preceding_b->winner_team;

  // If at least one match before this is not resolved, don't resolve this one
  if (!winner_a || !winner_b) {
    return false;
  }
  // If one of the preceding winner teams is empty, make the other the winner
  if (winner_a->players.empty() != winner_b->players.empty()) {
    this->set_winner_team(winner_a->players.empty() ? winner_b : winner_a);
    return true;
  }
  // If neither preceding winner team has any humans on it, skip this match
  // entirely and just make one team advance arbitrarily (note that this also
  // handles the case where both preceding winner teams are empty)
  if (!winner_a->has_any_human_players() && !winner_b->has_any_human_players()) {
    this->set_winner_team((phosg::random_object<uint8_t>() & 1) ? winner_b : winner_a);
    return true;
  }

  return false;
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
  if (following && !following->resolve_if_skippable()) {
    tournament->pending_matches.emplace(following);
  }

  // If there are no pending matches, then the tournament is complete
  if (tournament->pending_matches.empty()) {
    tournament->current_state = Tournament::State::COMPLETE;
  }

  // Unlink the losing team's players (if any) - this allows them to enter
  // another tournament before this tournament has ended
  if (this->preceding_a && this->preceding_b) {
    auto losing_team = (this->winner_team == this->preceding_a->winner_team)
        ? this->preceding_b->winner_team
        : this->preceding_a->winner_team;
    for (auto& player : losing_team->players) {
      auto c = player.client.lock();
      if (c) {
        c->ep3_tournament_team.reset();
      }
    }
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
    const string& name,
    shared_ptr<const MapIndex::Map> map,
    const Rules& rules,
    size_t num_teams,
    uint8_t flags)
    : log(phosg::string_printf("[Tournament:%s] ", name.c_str())),
      map_index(map_index),
      com_deck_index(com_deck_index),
      name(name),
      map(map),
      rules(rules),
      num_teams(num_teams),
      flags(flags),
      current_state(State::REGISTRATION),
      menu_item_id(0xFFFFFFFF) {
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
    const phosg::JSON& json)
    : log(phosg::string_printf("[Tournament:%s] ", json.get_string("name").c_str())),
      map_index(map_index),
      com_deck_index(com_deck_index),
      source_json(json),
      current_state(State::REGISTRATION) {}

void Tournament::init() {
  vector<size_t> team_index_to_rounds_cleared;

  bool is_registration_complete;
  if (!this->source_json.is_null()) {
    this->name = this->source_json.get_string("name");
    this->map = this->map_index->for_number(this->source_json.get_int("map_number"));
    this->rules = Rules(this->source_json.at("rules"));
    this->flags = this->source_json.get_int("flags", 0x02);
    if (this->source_json.get_bool("is_2v2", false)) {
      this->flags |= Flag::IS_2V2;
    }
    is_registration_complete = this->source_json.get_bool("is_registration_complete");

    for (const auto& team_json : this->source_json.get_list("teams")) {
      auto& team = this->teams.emplace_back(make_shared<Team>(
          this->shared_from_this(), this->teams.size(), team_json->get_int("max_players")));
      team->name = team_json->get_string("name");
      team->password = team_json->get_string("password");
      team_index_to_rounds_cleared.emplace_back(team_json->get_int("num_rounds_cleared"));
      for (const auto& player_json : team_json->get_list("player_specs")) {
        if (player_json->is_list()) {
          uint32_t account_id = player_json->at(0).as_int();
          team->players.emplace_back(account_id, player_json->at(1).as_string());
          this->all_player_account_ids.emplace(account_id);
        } else if (player_json->is_int()) {
          uint32_t account_id = player_json->as_int();
          team->players.emplace_back(account_id);
          this->all_player_account_ids.emplace(account_id);
        } else if (player_json->is_string()) {
          team->players.emplace_back(this->com_deck_index->deck_for_name(player_json->as_string()));
        } else {
          throw runtime_error("invalid player spec");
        }
      }
    }
    this->num_teams = this->teams.size();

    this->source_json = nullptr;

  } else {
    // Create empty teams
    while (this->teams.size() < this->num_teams) {
      auto t = make_shared<Team>(
          this->shared_from_this(), this->teams.size(), (this->flags & Flag::IS_2V2) ? 2 : 1);
      this->teams.emplace_back(t);
    }
    is_registration_complete = false;
  }

  // Compute the match state from the teams' states
  if (is_registration_complete) {
    this->current_state = State::IN_PROGRESS;
    this->create_bracket_matches();

    // Start with all zero-round matches in the match queue
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
    this->current_state = State::REGISTRATION;
  }
}

void Tournament::create_bracket_matches() {
  if (this->teams.size() < 4) {
    throw logic_error("tournaments must have at least 4 teams");
  }
  if (this->teams.size() > 32) {
    throw logic_error("tournaments must have at most 32 teams");
  }
  if (this->teams.size() & (this->teams.size() - 1)) {
    throw logic_error("tournaments team count is not a power of 2");
  }

  // Create the zero-round matches, and make them all pending if registration
  // is still open
  this->zero_round_matches.clear();
  for (const auto& team : this->teams) {
    auto m = make_shared<Match>(this->shared_from_this(), team);
    this->zero_round_matches.emplace_back(m);
    if (this->current_state == State::REGISTRATION) {
      this->pending_matches.emplace(m);
    }
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
}

phosg::JSON Tournament::json() const {
  auto teams_list = phosg::JSON::list();
  for (auto team : this->teams) {
    auto players_list = phosg::JSON::list();
    for (const auto& player : team->players) {
      if (player.is_human()) {
        if (!player.player_name.empty()) {
          players_list.emplace_back(phosg::JSON::list({player.account_id, player.player_name}));
        } else {
          players_list.emplace_back(player.account_id);
        }
      } else {
        players_list.emplace_back(player.com_deck->deck_name);
      }
    }
    teams_list.emplace_back(phosg::JSON::dict({
        {"max_players", team->max_players},
        {"player_specs", std::move(players_list)},
        {"name", team->name},
        {"password", team->password},
        {"num_rounds_cleared", team->num_rounds_cleared},
    }));
  }
  return phosg::JSON::dict({
      {"name", this->name},
      {"map_number", this->map->map_number},
      {"rules", this->rules.json()},
      {"flags", this->flags},
      {"is_registration_complete", (this->current_state != State::REGISTRATION)},
      {"teams", std::move(teams_list)},
  });
}

shared_ptr<Tournament::Team> Tournament::get_winner_team() const {
  if (this->current_state != State::COMPLETE) {
    return nullptr;
  }
  if (!this->final_match) {
    throw logic_error("tournament is complete but final match is missing");
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

shared_ptr<Tournament::Team> Tournament::team_for_account_id(
    uint32_t account_id) const {
  if (!this->all_player_account_ids.count(account_id)) {
    return nullptr;
  }

  for (auto team : this->teams) {
    for (const auto& player : team->players) {
      if (player.account_id == account_id) {
        return team->is_active ? team : nullptr;
      }
    }
  }

  throw logic_error("account ID registered in tournament but not in any team");
}

const set<uint32_t>& Tournament::get_all_player_account_ids() const {
  return this->all_player_account_ids;
}

void Tournament::start() {
  if (this->current_state != State::REGISTRATION) {
    throw runtime_error("tournament has already started");
  }

  bool has_com_teams = (this->flags & Flag::HAS_COM_TEAMS);

  // If there aren't enough entrants (1 if has_com_teams is false, else 2),
  // don't allow the tournament to start (because it would enter the COMPLETE
  // state immediately)
  size_t num_human_teams = 0;
  for (size_t z = 0; z < this->teams.size(); z++) {
    if (this->teams[z]->has_any_human_players()) {
      num_human_teams++;
    }
  }
  if (num_human_teams < (has_com_teams ? 1 : 2)) {
    throw runtime_error("not enough registrants to start tournament");
  }

  if ((this->flags & Flag::SHUFFLE_ENTRIES) && (this->flags & Flag::RESIZE_ON_START)) {
    // If both of these flags are set, pack the human teams into the lowest part
    // of the teams list so we can resize the tournament to the smallest
    // possible size. This is OK since we're going to shuffle them later anyway
    size_t r_offset = 0, w_offset = 0;
    for (; r_offset < this->teams.size(); r_offset++) {
      if (this->teams[r_offset]->has_any_human_players()) {
        if (r_offset != w_offset) {
          this->teams[r_offset].swap(this->teams[w_offset]);
        }
        w_offset++;
      }
    }
  }

  if (this->flags & Flag::RESIZE_ON_START) {
    // Resize the tournament by repeatedly deleting the second half of it, until
    // the second half contains human players or the tournament size is 4
    while (this->teams.size() > 4) {
      size_t z;
      for (z = this->teams.size() >> 1; z < this->teams.size(); z++) {
        if (this->teams[z]->has_any_human_players()) {
          break;
        }
      }
      if (z == this->teams.size()) {
        this->teams.resize(this->teams.size() >> 1);
      } else {
        break;
      }
    }
    this->num_teams = this->teams.size();
  }

  if (this->flags & Flag::SHUFFLE_ENTRIES) {
    // Shuffle all the tournament entries
    for (size_t z = this->teams.size(); z > 0; z--) {
      size_t index = phosg::random_object<uint32_t>() % z;
      if (index != z - 1) {
        this->teams[z - 1].swap(this->teams[index]);
      }
    }
  }

  this->current_state = State::IN_PROGRESS;
  this->create_bracket_matches();

  // Assign names to COM teams, and assign COM decks to all empty slots unless
  // has_com_teams is false
  for (size_t z = 0; z < this->zero_round_matches.size(); z++) {
    auto m = this->zero_round_matches[z];
    auto t = m->winner_team;
    if (t->name.empty()) {
      t->name = has_com_teams ? phosg::string_printf("COM:%zu", z) : "(no entrant)";
    }
    for (const auto& player : t->players) {
      if (player.is_com()) {
        throw logic_error("non-human player on team before tournament start");
      }
    }
    if (this->com_deck_index->num_decks() < t->max_players - t->players.size()) {
      throw runtime_error("not enough COM decks to complete team");
    }
    // If we allow all-COM teams, or this is a 2v2 tournament and the team has
    // only one human on it, add a COM
    if (has_com_teams || !t->players.empty()) {
      // TODO: Don't allow duplicate COM decks, nor duplicate COM SCs on the
      // same team
      while (t->players.size() < t->max_players) {
        t->players.emplace_back(this->com_deck_index->random_deck());
      }
    }
  }

  // Resolve all possible skippable matches
  for (auto m : this->zero_round_matches) {
    m->on_winner_team_set();
  }
}

void Tournament::send_all_state_updates() const {
  for (const auto& team : this->teams) {
    for (const auto& player : team->players) {
      auto c = player.client.lock();
      // Note: The last check here is to make sure the client is still linked
      // with this instance of the tournament - an intervening shell command
      // `reload ep3` could have changed the client's linkage
      if (c && (c->version() == Version::GC_EP3) && (c->ep3_tournament_team.lock() == team)) {
        send_ep3_confirm_tournament_entry(c, this->shared_from_this());
      }
    }
  }
}

void Tournament::send_all_state_updates_on_deletion() const {
  for (const auto& team : this->teams) {
    for (const auto& player : team->players) {
      auto c = player.client.lock();
      if (c && (c->version() == Version::GC_EP3) && (c->ep3_tournament_team.lock() == team)) {
        send_ep3_confirm_tournament_entry(c, nullptr);
      }
    }
  }
}

string Tournament::bracket_str() const {
  string ret = phosg::string_printf("Tournament \"%s\"\n", this->name.c_str());

  function<void(shared_ptr<Match>, size_t)> add_match = [&](shared_ptr<Match> m, size_t indent_level) -> void {
    ret.append(2 * indent_level, ' ');
    ret += m->str();
    if (this->pending_matches.count(m)) {
      ret += " (PENDING)";
    }
    ret.push_back('\n');
    if (m->preceding_a) {
      add_match(m->preceding_a, indent_level + 1);
    }
    if (m->preceding_b) {
      add_match(m->preceding_b, indent_level + 1);
    }
  };

  auto en_vm = this->map->version(1);
  if (en_vm) {
    string map_name = en_vm->map->name.decode(en_vm->language);
    ret += phosg::string_printf("  Map: %08" PRIX32 " (%s)\n", this->map->map_number, map_name.c_str());
  } else {
    ret += phosg::string_printf("  Map: %08" PRIX32 "\n", this->map->map_number);
  }
  string rules_str = this->rules.str();
  ret += phosg::string_printf("  Rules: %s\n", rules_str.c_str());
  ret += phosg::string_printf("  Structure: %s, %zu entries\n", (this->flags & Flag::IS_2V2) ? "2v2" : "1v1", this->num_teams);
  ret += phosg::string_printf("  COM teams: %s\n", (this->flags & Flag::HAS_COM_TEAMS) ? "allowed" : "forbidden");
  ret += phosg::string_printf("  Shuffle entries: %s\n", (this->flags & Flag::SHUFFLE_ENTRIES) ? "yes" : "no");
  ret += phosg::string_printf("  Resize on start: %s\n", (this->flags & Flag::RESIZE_ON_START) ? "yes" : "no");
  switch (this->current_state) {
    case State::REGISTRATION:
      ret += "  State: REGISTRATION\n";
      break;
    case State::IN_PROGRESS:
      ret += "  State: IN_PROGRESS\n";
      break;
    case State::COMPLETE:
      ret += "  State: COMPLETE\n";
      break;
    default:
      ret += "  State: UNKNOWN\n";
      break;
  }
  if (this->final_match) {
    ret += "  Standings:\n";
    add_match(this->final_match, 2);
  }
  if (this->current_state == State::REGISTRATION) {
    ret += "  Teams:\n";
    for (const auto& team : this->teams) {
      string team_str = team->str();
      ret += phosg::string_printf("    %s\n", team_str.c_str());
    }
  } else {
    ret += "  Pending matches:\n";
    for (const auto& match : this->pending_matches) {
      string match_str = match->str();
      ret += phosg::string_printf("    %s\n", match_str.c_str());
    }
  }

  phosg::strip_trailing_whitespace(ret);
  return ret;
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

  phosg::JSON json;
  try {
    json = phosg::JSON::parse(phosg::load_file(this->state_filename));
  } catch (const phosg::cannot_open_file&) {
    json = phosg::JSON::list();
  }

  if (json.is_list()) {
    if (json.size() > 0x20) {
      throw runtime_error("tournament phosg::JSON list length is incorrect");
    }
    for (size_t z = 0; z < min<size_t>(json.size(), 0x20); z++) {
      if (!json.at(z).is_null()) {
        auto tourn = make_shared<Tournament>(this->map_index, this->com_deck_index, json.at(z));
        tourn->init();
        if (!this->name_to_tournament.emplace(tourn->get_name(), tourn).second) {
          throw runtime_error("multiple tournaments have the same name: " + tourn->get_name());
        }
        tourn->set_menu_item_id(this->menu_item_id_to_tournament.size());
        this->menu_item_id_to_tournament.emplace_back(tourn);
      }
    }
  } else if (json.is_dict()) {
    if (json.size() > 0x20) {
      throw runtime_error("tournament phosg::JSON dict length is incorrect");
    }
    for (const auto& it : json.as_dict()) {
      auto tourn = make_shared<Tournament>(this->map_index, this->com_deck_index, *it.second);
      tourn->init();
      if (!this->name_to_tournament.emplace(tourn->get_name(), tourn).second) {
        // This is logic_error instead of runtime_error because phosg::JSON dicts are
        // supposed to already have unique keys
        throw logic_error("multiple tournaments have the same name: " + tourn->get_name());
      }
      tourn->set_menu_item_id(this->menu_item_id_to_tournament.size());
      this->menu_item_id_to_tournament.emplace_back(tourn);
    }
  } else {
    throw runtime_error("tournament state root phosg::JSON is not a list or dict");
  }
}

void TournamentIndex::save() const {
  if (this->state_filename.empty()) {
    return;
  }

  auto json = phosg::JSON::dict();
  for (const auto& it : this->name_to_tournament) {
    json.emplace(it.second->get_name(), it.second->json());
  }
  phosg::save_file(this->state_filename, json.serialize(phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::HEX_INTEGERS | phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY));
}

shared_ptr<Tournament> TournamentIndex::create_tournament(
    const string& name,
    shared_ptr<const MapIndex::Map> map,
    const Rules& rules,
    size_t num_teams,
    uint8_t flags) {
  if (this->name_to_tournament.size() >= 0x20) {
    throw runtime_error("there can be at most 32 tournaments at a time");
  }

  auto t = make_shared<Tournament>(
      this->map_index, this->com_deck_index, name, map, rules, num_teams, flags);
  t->init();
  if (!this->name_to_tournament.emplace(t->get_name(), t).second) {
    throw runtime_error("a tournament with the same name already exists");
  }

  size_t z;
  for (z = 0; z < this->menu_item_id_to_tournament.size(); z++) {
    if (!this->menu_item_id_to_tournament[z]) {
      t->set_menu_item_id(z);
      this->menu_item_id_to_tournament[z] = t;
      break;
    }
  }
  if (z == this->menu_item_id_to_tournament.size()) {
    t->set_menu_item_id(this->menu_item_id_to_tournament.size());
    this->menu_item_id_to_tournament.emplace_back(t);
  }

  this->save();
  return t;
}

bool TournamentIndex::delete_tournament(const string& name) {
  auto it = this->name_to_tournament.find(name);
  if (it == this->name_to_tournament.end()) {
    return false;
  }
  for (size_t z = 0; z < this->menu_item_id_to_tournament.size(); z++) {
    if (this->menu_item_id_to_tournament[z] == it->second) {
      this->menu_item_id_to_tournament[z] = nullptr;
      it->second->set_menu_item_id(0xFFFFFFFF);
    }
  }
  it->second->send_all_state_updates_on_deletion();
  this->name_to_tournament.erase(it);
  this->save();
  return true;
}

shared_ptr<Tournament::Team> TournamentIndex::team_for_account_id(uint32_t account_id) const {
  for (const auto& it : this->name_to_tournament) {
    const auto& tourn = it.second;
    auto team = tourn->team_for_account_id(account_id);
    if (team) {
      return team;
    }
  }
  return nullptr;
}

void TournamentIndex::link_client(shared_ptr<Client> c) {
  if (!is_ep3(c->version())) {
    return;
  }

  auto team = this->team_for_account_id(c->login->account->account_id);
  auto tourn = team ? team->tournament.lock() : nullptr;
  if (team && team->is_active && tourn) {
    for (auto& player : team->players) {
      if (player.account_id == c->login->account->account_id) {
        c->ep3_tournament_team = team;
        player.client = c;
        if (c->version() == Version::GC_EP3) {
          send_ep3_confirm_tournament_entry(c, tourn);
        }
        return;
      }
    }
    throw logic_error("tournament team found for player, but player not found on team");
  } else {
    c->ep3_tournament_team.reset();
    if (c->version() == Version::GC_EP3) {
      send_ep3_confirm_tournament_entry(c, nullptr);
    }
  }
}

void TournamentIndex::link_all_clients(std::shared_ptr<ServerState> s) {
  for (const auto& c_it : s->channel_to_client) {
    this->link_client(c_it.second);
  }
}

} // namespace Episode3
