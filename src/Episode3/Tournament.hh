#pragma once

#include <event2/event.h>
#include <stdint.h>

#include <memory>
#include <phosg/JSON.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_set>
#include <vector>

#include "DataIndexes.hh"

struct Lobby;
class Client;
struct ServerState;

namespace Episode3 {

class Tournament : public std::enable_shared_from_this<Tournament> {
public:
  enum Flag : uint8_t {
    IS_2V2 = 0x01,
    HAS_COM_TEAMS = 0x02,
    SHUFFLE_ENTRIES = 0x04,
    RESIZE_ON_START = 0x08,
  };
  enum class State {
    REGISTRATION = 0,
    IN_PROGRESS,
    COMPLETE,
  };

  struct PlayerEntry {
    // Invariant: (account_id == 0) != (com_deck == nullptr)
    // (that is, exactly one of the following must be valid)
    uint32_t account_id;
    std::shared_ptr<const COMDeckDefinition> com_deck;

    // client is valid if account_id is nonzero and the client is connected
    std::weak_ptr<Client> client;
    std::string player_name; // Not used for COM decks

    explicit PlayerEntry(uint32_t account_id, const std::string& player_name = "");
    explicit PlayerEntry(std::shared_ptr<Client> c);
    explicit PlayerEntry(std::shared_ptr<const COMDeckDefinition> com_deck);

    bool is_com() const;
    bool is_human() const;

    phosg::JSON json() const;
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
        std::shared_ptr<Client> c,
        const std::string& team_name,
        const std::string& password);
    bool unregister_player(uint32_t account_id);

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

    bool resolve_if_skippable();
    void on_winner_team_set();
    void set_winner_team(std::shared_ptr<Team> team);
    void set_winner_team_without_triggers(std::shared_ptr<Team> team);
    std::shared_ptr<Team> opponent_team_for_team(std::shared_ptr<Team> team) const;
  };

  Tournament(
      std::shared_ptr<const MapIndex> map_index,
      std::shared_ptr<const COMDeckIndex> com_deck_index,
      const std::string& name,
      std::shared_ptr<const MapIndex::Map> map,
      const Rules& rules,
      size_t num_teams,
      uint8_t flags);
  Tournament(
      std::shared_ptr<const MapIndex> map_index,
      std::shared_ptr<const COMDeckIndex> com_deck_index,
      const phosg::JSON& json);
  ~Tournament() = default;
  void init();

  phosg::JSON json() const;

  inline const std::string& get_name() const {
    return this->name;
  }
  inline std::shared_ptr<const MapIndex::Map> get_map() const {
    return this->map;
  }
  inline const Rules& get_rules() const {
    return this->rules;
  }
  inline uint8_t get_flags() const {
    return this->flags;
  }
  inline State get_state() const {
    return this->current_state;
  }
  inline const std::vector<std::shared_ptr<Team>>& all_teams() const {
    return this->teams;
  }
  inline std::shared_ptr<Team> get_team(size_t index) const {
    return this->teams.at(index);
  }
  inline uint32_t get_menu_item_id() const {
    return this->menu_item_id;
  }
  inline void set_menu_item_id(uint32_t menu_item_id) {
    this->menu_item_id = menu_item_id;
  }

  std::shared_ptr<Team> get_winner_team() const;
  std::shared_ptr<Match> next_match_for_team(std::shared_ptr<Team> team) const;
  std::shared_ptr<Match> get_final_match() const;
  std::shared_ptr<Team> team_for_account_id(uint32_t account_id) const;
  const std::set<uint32_t>& get_all_player_account_ids() const;

  void start();

  void send_all_state_updates() const;
  void send_all_state_updates_on_deletion() const;

  std::string bracket_str() const;

private:
  void create_bracket_matches();

  phosg::PrefixedLogger log;

  std::shared_ptr<const MapIndex> map_index;
  std::shared_ptr<const COMDeckIndex> com_deck_index;
  phosg::JSON source_json;
  std::string name;
  std::shared_ptr<const MapIndex::Map> map;
  Rules rules;
  size_t num_teams;
  uint8_t flags;
  State current_state;
  uint32_t menu_item_id;

  std::set<uint32_t> all_player_account_ids;
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
      std::shared_ptr<const MapIndex> map_index,
      std::shared_ptr<const COMDeckIndex> com_deck_index,
      const std::string& state_filename,
      bool skip_load_state = false);
  ~TournamentIndex() = default;

  void save() const;

  inline const std::unordered_map<std::string, std::shared_ptr<Tournament>>& all_tournaments() const {
    return this->name_to_tournament;
  }
  inline std::shared_ptr<Tournament> get_tournament(uint32_t menu_item_id) const {
    try {
      return this->menu_item_id_to_tournament.at(menu_item_id);
    } catch (const std::out_of_range&) {
      return nullptr;
    }
  }
  inline std::shared_ptr<Tournament> get_tournament(const std::string& name) const {
    try {
      return this->name_to_tournament.at(name);
    } catch (const std::out_of_range&) {
      return nullptr;
    }
  }

  std::shared_ptr<Tournament> create_tournament(
      const std::string& name,
      std::shared_ptr<const MapIndex::Map> map,
      const Rules& rules,
      size_t num_teams,
      uint8_t flags);
  bool delete_tournament(const std::string& name);

  std::shared_ptr<Tournament::Team> team_for_account_id(uint32_t account_id) const;

  void link_client(std::shared_ptr<Client> c);
  void link_all_clients(std::shared_ptr<ServerState> s);

private:
  std::shared_ptr<const MapIndex> map_index;
  std::shared_ptr<const COMDeckIndex> com_deck_index;
  std::string state_filename;
  std::unordered_map<std::string, std::shared_ptr<Tournament>> name_to_tournament;
  std::vector<std::shared_ptr<Tournament>> menu_item_id_to_tournament;
};

} // namespace Episode3
