#pragma once

#include <stdint.h>

#include <array>
#include <memory>
#include <phosg/JSON.hh>
#include <random>
#include <string>

#include "ItemNameIndex.hh"
#include "SaveFileFormats.hh"
#include "StaticGameData.hh"
#include "Text.hh"
#include "Version.hh"

class TeamIndex {
public:
  struct Team {
    struct Member {
      enum class Flag {
        IS_MASTER = 0x01,
        IS_LEADER = 0x02,
      };
      uint32_t serial_number = 0;
      uint8_t flags = 0;
      uint64_t points = 0;
      std::string name;

      Member() = default;
      explicit Member(const JSON& json);
      JSON json() const;

      [[nodiscard]] inline bool check_flag(Flag flag) const {
        return !!(static_cast<uint8_t>(flag) & this->flags);
      }
      inline void set_flag(Flag flag) {
        this->flags |= static_cast<uint8_t>(flag);
      }
      inline void clear_flag(Flag flag) {
        this->flags &= (~static_cast<uint8_t>(flag));
      }

      uint32_t privilege_level() const;
    };

    enum class RewardFlag {
      // Only 0x00000001 and 0x00000002 are used by the client; the rest are
      // free to be used however the server chooses.
      TEAM_FLAG = 0x00000001,
      DRESSING_ROOM = 0x00000002,
      MEMBERS_20_LEADERS_3 = 0x00000004,
      MEMBERS_40_LEADERS_5 = 0x00000008,
      MEMBERS_70_LEADERS_8 = 0x00000010,
      MEMBERS_100_LEADERS_10 = 0x00000020,
    };

    uint32_t team_id = 0;
    std::string name;
    std::unordered_map<uint32_t, Member> members;
    uint32_t reward_flags = 0;
    std::shared_ptr<parray<le_uint16_t, 0x20 * 0x20>> flag_data;

    Team() = default;
    explicit Team(uint32_t team_id);
    JSON json() const;

    std::string json_filename() const;
    std::string flag_filename() const;

    void load_config();
    void save_config() const;
    void load_flag();
    void save_flag() const;
    void delete_files() const;

    PSOBBTeamMembership membership_for_member(uint32_t serial_number) const;

    [[nodiscard]] inline bool check_reward_flag(RewardFlag flag) const {
      return !!(static_cast<uint8_t>(flag) & this->reward_flags);
    }
    inline void set_reward_flag(RewardFlag flag) {
      this->reward_flags |= static_cast<uint8_t>(flag);
    }
    inline void clear_reward_flag(RewardFlag flag) {
      this->reward_flags &= (~static_cast<uint8_t>(flag));
    }

    size_t num_members() const;
    size_t num_leaders() const;
    size_t max_members() const;
    size_t max_leaders() const;
    bool can_add_member() const;
    bool can_promote_leader() const;
  };

  explicit TeamIndex(const std::string& directory);
  ~TeamIndex() = default;

  size_t count() const;
  std::shared_ptr<Team> get_by_id(uint32_t team_id);
  std::shared_ptr<Team> get_by_name(const std::string& name);
  std::shared_ptr<Team> get_by_serial_number(uint32_t serial_number);
  std::vector<std::shared_ptr<Team>> all();

  std::shared_ptr<Team> create(std::string& name, uint32_t master_serial_number, const std::string& master_name);
  void disband(uint32_t team_id);

  void add_member(uint32_t team_id, uint32_t serial_number, const std::string& name);
  void remove_member(uint32_t serial_number);

protected:
  std::string directory;
  uint32_t next_team_id;
  std::unordered_map<uint32_t, std::shared_ptr<Team>> id_to_team;
  std::unordered_map<std::string, std::shared_ptr<Team>> name_to_team;
  std::unordered_map<uint32_t, std::shared_ptr<Team>> serial_number_to_team;

  void add_to_indexes(std::shared_ptr<Team> team);
  void remove_from_indexes(std::shared_ptr<Team> team);
};
