#pragma once

#include <event2/event.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <phosg/Strings.hh>
#include <string>
#include <variant>

#include "../PlayerSubordinates.hh"

struct Lobby;

namespace Episode3 {

// The comment in Server.hh does not apply to this file (and BattleRecord.cc).

class BattleRecord {
public:
  struct PlayerEntry {
    PlayerLobbyDataDCGC lobby_data;
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
    le_uint32_t level;

    void print(FILE* stream) const;
  } __packed_ws__(PlayerEntry, 0x440);

  struct Event {
    enum class Type : uint8_t {
      PLAYER_JOIN = 0,
      PLAYER_LEAVE = 1,
      SET_INITIAL_PLAYERS = 2,
      BATTLE_COMMAND = 3,
      GAME_COMMAND = 4,
      EP3_GAME_COMMAND = 5,
      CHAT_MESSAGE = 6,
      SERVER_DATA_COMMAND = 7,
    };

    // Fields used for all events
    Type type;
    uint64_t timestamp;
    // Fields used for PLAYER_JOIN and SET_INITIAL_PLAYERS only
    std::vector<PlayerEntry> players;
    // Fields used for PLAYER_LEAVE only
    uint8_t leaving_client_id;
    // Fields used for CHAT_MESSAGE only
    uint32_t guild_card_number;
    // Fields used for the COMMAND types and CHAT_MESSAGE
    std::string data;

    Event() = default;
    explicit Event(phosg::StringReader& r);
    void serialize(phosg::StringWriter& w) const;
    void print(FILE* stream) const;
  };

  explicit BattleRecord(uint32_t behavior_flags);
  explicit BattleRecord(const std::string& data);
  std::string serialize() const;

  bool writable() const;
  bool battle_in_progress() const;

  const Event* get_first_event() const;

  void add_player(
      const PlayerLobbyDataDCGC& lobby_data,
      const PlayerInventory& inventory,
      const PlayerDispDataDCPCV3& disp,
      uint32_t level);
  void delete_player(uint8_t client_id);
  void add_command(Event::Type type, const void* data, size_t size);
  void add_command(Event::Type type, std::string&& data);
  void add_chat_message(uint32_t guild_card_number, std::string&& data);
  void add_random_data(const void* data, size_t size);
  // This function collapses all the existing player join/leave events into a
  // single SET_INITIAL_PLAYERS event, and deletes all events before the latest
  // BATTLE_COMMAND command that specifies the battle map. This should provide a
  // minimal set of commands to set up and start the battle during a replay.
  void set_battle_start_timestamp();
  void set_battle_end_timestamp();

  void print(FILE* stream) const;

  std::vector<std::string> get_all_server_data_commands() const;
  const std::string& get_random_stream() const;

private:
  static constexpr uint64_t SIGNATURE_V1 = 0x14C946D56D1DAC50;
  static constexpr uint64_t SIGNATURE_V2 = 0xD01E5EC12853C377;

  static bool is_map_definition_event(const Event& ev);

  bool is_writable;

  uint32_t behavior_flags;
  uint64_t battle_start_timestamp;
  uint64_t battle_end_timestamp;
  std::deque<Event> events;
  std::string random_stream;

  friend class BattleRecordPlayer;
};

class BattleRecordPlayer {
public:
  BattleRecordPlayer(std::shared_ptr<const BattleRecord> rec, std::shared_ptr<struct event_base> base);
  ~BattleRecordPlayer() = default;

  std::shared_ptr<const BattleRecord> get_record() const;

  void set_lobby(std::shared_ptr<Lobby> l);
  void start();

private:
  static void dispatch_schedule_events(evutil_socket_t, short, void* ctx);
  void schedule_events();

  std::shared_ptr<const BattleRecord> record;
  std::deque<BattleRecord::Event>::const_iterator event_it;
  uint64_t play_start_timestamp;
  std::shared_ptr<struct event_base> base;
  std::weak_ptr<Lobby> lobby;
  std::shared_ptr<struct event> next_command_ev;
  phosg::StringReader random_r;
};

} // namespace Episode3
