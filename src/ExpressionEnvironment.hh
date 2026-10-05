#pragma once

#include <functional>
#include <resource_file/Emulators/Expression.hh>

#include "PlayerSubordinates.hh"
#include "TeamIndex.hh"

struct ExpressionEnvironment {
  size_t num_players;
  uint8_t section_id;
  uint8_t event;
  bool v1_present;
  Difficulty difficulty;
  const QuestFlags* quest_flags;
  const PlayerRecordsChallengeBB* challenge_records;
  const TeamIndex::Team* team;

  int64_t evaluate(const ResourceDASM::Expression::Node& expr) const {
    return expr.evaluate(std::bind(&ExpressionEnvironment::env_lookup, this, std::placeholders::_1)).as_int();
  }

  inline ResourceDASM::Expression::Value env_lookup(const std::string& name) const {
    if (name.empty()) {
      throw std::logic_error("Environment lookup name is empty");
    } else if (name == "P_SID") { // Player section ID
      return static_cast<int64_t>(this->section_id);
    } else if (name.starts_with("F_")) { // Quest flag lookup
      if (!this->quest_flags) {
        throw std::runtime_error("Quest flags are not available");
      }
      const char* flag_num_str;
      Difficulty flag_difficulty;
      if (name.starts_with("F_N_")) { // F_N_XXXX = quest flag lookup, normal
        flag_num_str = name.c_str() + 4;
        flag_difficulty = Difficulty::NORMAL;
      } else if (name.starts_with("F_H_")) { // F_H_XXXX = quest flag lookup, hard
        flag_num_str = name.c_str() + 4;
        flag_difficulty = Difficulty::HARD;
      } else if (name.starts_with("F_V_")) { // F_V_XXXX = quest flag lookup, very hard
        flag_num_str = name.c_str() + 4;
        flag_difficulty = Difficulty::VERY_HARD;
      } else if (name.starts_with("F_U_")) { // F_U_XXXX = quest flag lookup, ultimate
        flag_num_str = name.c_str() + 4;
        flag_difficulty = Difficulty::ULTIMATE;
      } else { // F_XXXX = quest flag lookup, current difficulty
        flag_num_str = name.c_str() + 2;
        flag_difficulty = this->difficulty;
      }
      return static_cast<int64_t>(this->quest_flags->get(flag_difficulty, strtoull(flag_num_str, nullptr, 16)));
    } else if (name.starts_with("CC_Ep1_")) { // Challenge competion lookup
      if (!this->challenge_records) {
        throw std::runtime_error("Challenge records are not available");
      }
      if (name.starts_with("CC_Ep1_")) { // CC_Ep1_X = challenge completed, episode 1 stage X
        size_t stage_index = strtoull(name.c_str() + 4, nullptr, 0);
        return static_cast<int64_t>(this->challenge_records->times_ep1_online.at(stage_index).has_value());
      } else if (name.starts_with("CC_Ep2_")) { // CC_Ep2_X = challenge completed, episode 2 stage X
        size_t stage_index = strtoull(name.c_str() + 4, nullptr, 0);
        return static_cast<int64_t>(this->challenge_records->times_ep2_online.at(stage_index).has_value());
      } else {
        throw std::runtime_error("Invalid Challenge record lookup token");
      }
    } else if (name.starts_with("T_")) { // T_XXX = team reward lookup by name
      // Unlike other cases, it is valid for the team to be missing if the player isn't in a team
      return static_cast<int64_t>((this->team && this->team->has_reward(name.substr(2))) ? 1 : 0);
    } else if (name == "V_NumPlayers") {
      return static_cast<int64_t>(this->num_players);
    } else if (name == "V_Event") {
      return static_cast<int64_t>(this->event);
    } else if (name == "V_V1Present") {
      return static_cast<int64_t>(this->v1_present);
    } else {
      throw std::runtime_error("Invalid environment lookup in expression");
    }
  }
};
