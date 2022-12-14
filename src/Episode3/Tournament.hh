#pragma once

#include <stdint.h>
#include <event2/event.h>

#include <memory>
#include <vector>
#include <unordered_set>
#include <string>
#include <phosg/JSON.hh>
#include <phosg/Strings.hh>

#include "../Player.hh"

struct Lobby;

namespace Episode3 {

// The comment in Server.hh does not apply to this file (and Tournament.cc).



class Tournament : public std::enable_shared_from_this<Tournament> {
public:
  enum class State {
    REGISTRATION = 0,
    IN_PROGRESS,
    COMPLETE,
  };

  struct PlayerEntry {
    // Invariant: (serial_number == 0) != (com_deck == nullptr)
    // (that is, exactly one of the following must be valid)
    uint32_t serial_number;
    std::shared_ptr<const COMDeckDefinition> com_deck;

    explicit PlayerEntry(uint32_t serial_number);
    explicit PlayerEntry(std::shared_ptr<const COMDeckDefinition> com_deck);

    bool is_com() const;
    bool is_human() const;
  };

  struct Team : public std::enable_shared_from_this<Team> {
    std::weak_ptr<Tournament> tournament;
    size_t index;
    size_t max_players;

    std::vector<PlayerEntry> players;
    std::string name;
    std::string password;
    size_t num_rounds_cleared;
    bool is_active;

    Team(
        std::shared_ptr<Tournament> tournament,
        size_t index,
        size_t max_players);
    std::string str() const;

    void register_player(
        uint32_t serial_number,
        const std::string& team_name,
        const std::string& password);
    bool unregister_player(uint32_t serial_number);

    bool has_any_human_players() const;
    size_t num_human_players() const;
    size_t num_com_players() const;
  };

  struct Match : public std::enable_shared_from_this<Match> {
    std::weak_ptr<Tournament> tournament;
    std::shared_ptr<Match> preceding_a;
    std::shared_ptr<Match> preceding_b;
    std::weak_ptr<Match> following;
    std::shared_ptr<Team> winner_team;
    size_t round_num;

    Match(
        std::shared_ptr<Tournament> tournament,
        std::shared_ptr<Match> preceding_a,
        std::shared_ptr<Match> preceding_b);
    Match(
        std::shared_ptr<Tournament> tournament,
        std::shared_ptr<Team> winner_team);
    std::string str() const;

    bool resolve_if_no_human_players();
    void on_winner_team_set();
    void set_winner_team(std::shared_ptr<Team> team);
    void set_winner_team_without_triggers(std::shared_ptr<Team> team);
    std::shared_ptr<Team> opponent_team_for_team(std::shared_ptr<Team> team) const;
  };

  Tournament(
      std::shared_ptr<const DataIndex> data_index,
      uint8_t number,
      const std::string& name,
      std::shared_ptr<const DataIndex::MapEntry> map,
      const Rules& rules,
      size_t num_teams,
      bool is_2v2);
  Tournament(
      std::shared_ptr<const DataIndex> data_index,
      uint8_t number,
      std::shared_ptr<const JSONObject> json);
  ~Tournament() = default;
  void init();

  std::shared_ptr<JSONObject> json() const;

  std::shared_ptr<const DataIndex> get_data_index() const;
  uint8_t get_number() const;
  const std::string& get_name() const;
  std::shared_ptr<const DataIndex::MapEntry> get_map() const;
  const Rules& get_rules() const;
  bool get_is_2v2() const;
  State get_state() const;

  const std::vector<std::shared_ptr<Team>>& all_teams() const;
  std::shared_ptr<Team> get_team(size_t index) const;
  std::shared_ptr<Team> get_winner_team() const;
  std::shared_ptr<Match> next_match_for_team(std::shared_ptr<Team> team) const;
  std::shared_ptr<Match> get_final_match() const;
  std::shared_ptr<Team> team_for_serial_number(uint32_t serial_number) const;
  const std::set<uint32_t>& get_all_player_serial_numbers() const;

  void start();

  void print_bracket(FILE* stream) const;

private:
  PrefixedLogger log;

  std::shared_ptr<const DataIndex> data_index;
  std::shared_ptr<const JSONObject> source_json;
  uint8_t number;
  std::string name;
  std::shared_ptr<const DataIndex::MapEntry> map;
  Rules rules;
  size_t num_teams;
  bool is_2v2;
  State current_state;

  std::set<uint32_t> all_player_serial_numbers;
  std::unordered_set<std::shared_ptr<Match>> pending_matches;

  // This vector contains all teams in the original starting order of the
  // tournament (that is, all teams in the first round). The order within this
  // vector determines which team will play against which other team in the
  // first round: [0] will play against [1], [2] will play against [3], etc.
  std::vector<std::shared_ptr<Team>> teams;
  // The tournament begins with a "zero round", in which each team automatically
  // "wins" a match, putting them into the first round. This is just to make the
  // data model easier to manage, so we don't have to have a type of match with
  // no preceding round.
  std::vector<std::shared_ptr<Match>> zero_round_matches;
  std::shared_ptr<Match> final_match;
};

class TournamentIndex {
public:
  explicit TournamentIndex(
      std::shared_ptr<const DataIndex> data_index,
      const std::string& state_filename,
      bool skip_load_state = false);
  ~TournamentIndex() = default;

  void save() const;

  std::vector<std::shared_ptr<Tournament>> all_tournaments() const;

  std::shared_ptr<Tournament> create_tournament(
    const std::string& name,
    std::shared_ptr<const DataIndex::MapEntry> map,
    const Rules& rules,
    size_t num_teams,
    bool is_2v2);
  void delete_tournament(uint8_t number);
  std::shared_ptr<Tournament> get_tournament(uint8_t number) const;
  std::shared_ptr<Tournament> get_tournament(const std::string& name) const;

  std::shared_ptr<Tournament::Team> team_for_serial_number(
      uint32_t serial_number) const;

private:
  std::shared_ptr<const DataIndex> data_index;
  std::string state_filename;
  std::shared_ptr<Tournament> tournaments[0x20];
};



} // namespace Episode3
