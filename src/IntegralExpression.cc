#include "IntegralExpression.hh"

#include <algorithm>
#include <mutex>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <string>
#include <unordered_map>

#include "CommandFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "QuestScript.hh"
#include "SaveFileFormats.hh"
#include "Text.hh"

using namespace std;

IntegralExpression::IntegralExpression(const string& text)
    : root(this->parse_expr(text)) {}

IntegralExpression::BinaryOperatorNode::BinaryOperatorNode(
    Type type, unique_ptr<const Node>&& left, unique_ptr<const Node>&& right)
    : type(type),
      left(std::move(left)),
      right(std::move(right)) {}

bool IntegralExpression::BinaryOperatorNode::operator==(const Node& other) const {
  try {
    const BinaryOperatorNode& other_bin = dynamic_cast<const BinaryOperatorNode&>(other);
    return other_bin.type == this->type && *other_bin.left == *this->left && *other_bin.right == *this->right;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::BinaryOperatorNode::evaluate(const Env& env) const {
  switch (this->type) {
    case Type::LOGICAL_OR:
      return this->left->evaluate(env) || this->right->evaluate(env);
    case Type::LOGICAL_AND:
      return this->left->evaluate(env) && this->right->evaluate(env);
    case Type::BITWISE_OR:
      return this->left->evaluate(env) | this->right->evaluate(env);
    case Type::BITWISE_AND:
      return this->left->evaluate(env) & this->right->evaluate(env);
    case Type::BITWISE_XOR:
      return this->left->evaluate(env) ^ this->right->evaluate(env);
    case Type::LEFT_SHIFT:
      return this->left->evaluate(env) << this->right->evaluate(env);
    case Type::RIGHT_SHIFT:
      return this->left->evaluate(env) >> this->right->evaluate(env);
    case Type::LESS_THAN:
      return this->left->evaluate(env) < this->right->evaluate(env);
    case Type::GREATER_THAN:
      return this->left->evaluate(env) > this->right->evaluate(env);
    case Type::LESS_OR_EQUAL:
      return this->left->evaluate(env) <= this->right->evaluate(env);
    case Type::GREATER_OR_EQUAL:
      return this->left->evaluate(env) >= this->right->evaluate(env);
    case Type::EQUAL:
      return this->left->evaluate(env) == this->right->evaluate(env);
    case Type::NOT_EQUAL:
      return this->left->evaluate(env) != this->right->evaluate(env);
    case Type::ADD:
      return this->left->evaluate(env) + this->right->evaluate(env);
    case Type::SUBTRACT:
      return this->left->evaluate(env) - this->right->evaluate(env);
    case Type::MULTIPLY:
      return this->left->evaluate(env) * this->right->evaluate(env);
    case Type::DIVIDE:
      return this->left->evaluate(env) / this->right->evaluate(env);
    case Type::MODULUS:
      return this->left->evaluate(env) % this->right->evaluate(env);
    default:
      throw logic_error("invalid binary operator type");
  }
}

string IntegralExpression::BinaryOperatorNode::str() const {
  switch (this->type) {
    case Type::LOGICAL_OR:
      return "(" + this->left->str() + ") || (" + this->right->str() + ")";
    case Type::LOGICAL_AND:
      return "(" + this->left->str() + ") && (" + this->right->str() + ")";
    case Type::BITWISE_OR:
      return "(" + this->left->str() + ") | (" + this->right->str() + ")";
    case Type::BITWISE_AND:
      return "(" + this->left->str() + ") & (" + this->right->str() + ")";
    case Type::BITWISE_XOR:
      return "(" + this->left->str() + ") ^ (" + this->right->str() + ")";
    case Type::LEFT_SHIFT:
      return "(" + this->left->str() + ") << (" + this->right->str() + ")";
    case Type::RIGHT_SHIFT:
      return "(" + this->left->str() + ") >> (" + this->right->str() + ")";
    case Type::LESS_THAN:
      return "(" + this->left->str() + ") < (" + this->right->str() + ")";
    case Type::GREATER_THAN:
      return "(" + this->left->str() + ") > (" + this->right->str() + ")";
    case Type::LESS_OR_EQUAL:
      return "(" + this->left->str() + ") <= (" + this->right->str() + ")";
    case Type::GREATER_OR_EQUAL:
      return "(" + this->left->str() + ") >= (" + this->right->str() + ")";
    case Type::EQUAL:
      return "(" + this->left->str() + ") == (" + this->right->str() + ")";
    case Type::NOT_EQUAL:
      return "(" + this->left->str() + ") != (" + this->right->str() + ")";
    case Type::ADD:
      return "(" + this->left->str() + ") + (" + this->right->str() + ")";
    case Type::SUBTRACT:
      return "(" + this->left->str() + ") - (" + this->right->str() + ")";
    case Type::MULTIPLY:
      return "(" + this->left->str() + ") * (" + this->right->str() + ")";
    case Type::DIVIDE:
      return "(" + this->left->str() + ") / (" + this->right->str() + ")";
    case Type::MODULUS:
      return "(" + this->left->str() + ") % (" + this->right->str() + ")";
    default:
      throw logic_error("invalid binary operator type");
  }
}

IntegralExpression::UnaryOperatorNode::UnaryOperatorNode(Type type, unique_ptr<const Node>&& sub)
    : type(type),
      sub(std::move(sub)) {}

bool IntegralExpression::UnaryOperatorNode::operator==(const Node& other) const {
  try {
    const UnaryOperatorNode& other_un = dynamic_cast<const UnaryOperatorNode&>(other);
    return other_un.type == this->type && *other_un.sub == *this->sub;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::UnaryOperatorNode::evaluate(const Env& env) const {
  switch (this->type) {
    case Type::LOGICAL_NOT:
      return !this->sub->evaluate(env);
    case Type::BITWISE_NOT:
      return ~this->sub->evaluate(env);
    case Type::NEGATIVE:
      return -this->sub->evaluate(env);
    default:
      throw logic_error("invalid unary operator type");
  }
}

string IntegralExpression::UnaryOperatorNode::str() const {
  switch (this->type) {
    case Type::LOGICAL_NOT:
      return "!(" + this->sub->str() + ")";
    case Type::BITWISE_NOT:
      return "~(" + this->sub->str() + ")";
    case Type::NEGATIVE:
      return "-(" + this->sub->str() + ")";
    default:
      throw logic_error("invalid unary operator type");
  }
}

IntegralExpression::FlagLookupNode::FlagLookupNode(uint16_t flag_index)
    : flag_index(flag_index) {}

bool IntegralExpression::FlagLookupNode::operator==(const Node& other) const {
  try {
    const FlagLookupNode& other_flag = dynamic_cast<const FlagLookupNode&>(other);
    return other_flag.flag_index == this->flag_index;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::FlagLookupNode::evaluate(const Env& env) const {
  if (!env.flags) {
    throw runtime_error("quest flags not available");
  }
  return env.flags->get(this->flag_index) ? 1 : 0;
}

string IntegralExpression::FlagLookupNode::str() const {
  return phosg::string_printf("F_%04hX", this->flag_index);
}

IntegralExpression::ChallengeCompletionLookupNode::ChallengeCompletionLookupNode(
    Episode episode, uint8_t stage_index)
    : episode(episode),
      stage_index(stage_index) {}

bool IntegralExpression::ChallengeCompletionLookupNode::operator==(const Node& other) const {
  try {
    const ChallengeCompletionLookupNode& other_cc = dynamic_cast<const ChallengeCompletionLookupNode&>(other);
    return other_cc.episode == this->episode && other_cc.stage_index == this->stage_index;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::ChallengeCompletionLookupNode::evaluate(const Env& env) const {
  if (!env.challenge_records) {
    throw runtime_error("challenge records not available");
  }
  if (this->episode == Episode::EP1) {
    return env.challenge_records->times_ep1_online.at(this->stage_index).has_value();
  } else if (this->episode == Episode::EP2) {
    return env.challenge_records->times_ep2_online.at(this->stage_index).has_value();
  }
  return false;
}

string IntegralExpression::ChallengeCompletionLookupNode::str() const {
  return phosg::string_printf("CC_%s_%hhu", abbreviation_for_episode(this->episode), static_cast<uint8_t>(this->stage_index + 1));
}

IntegralExpression::TeamRewardLookupNode::TeamRewardLookupNode(const string& reward_name)
    : reward_name(reward_name) {}

bool IntegralExpression::TeamRewardLookupNode::operator==(const Node& other) const {
  try {
    const TeamRewardLookupNode& other_team_reward = dynamic_cast<const TeamRewardLookupNode&>(other);
    return other_team_reward.reward_name == this->reward_name;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::TeamRewardLookupNode::evaluate(const Env& env) const {
  return (env.team && env.team->has_reward(this->reward_name)) ? 1 : 0;
}

string IntegralExpression::TeamRewardLookupNode::str() const {
  return "T_" + this->reward_name;
}

IntegralExpression::NumPlayersLookupNode::NumPlayersLookupNode() {}

bool IntegralExpression::NumPlayersLookupNode::operator==(const Node& other) const {
  return dynamic_cast<const NumPlayersLookupNode*>(&other) != nullptr;
}

int64_t IntegralExpression::NumPlayersLookupNode::evaluate(const Env& env) const {
  return env.num_players;
}

string IntegralExpression::NumPlayersLookupNode::str() const {
  return "V_NumPlayers";
}

IntegralExpression::EventLookupNode::EventLookupNode() {}

bool IntegralExpression::EventLookupNode::operator==(const Node& other) const {
  return dynamic_cast<const EventLookupNode*>(&other) != nullptr;
}

int64_t IntegralExpression::EventLookupNode::evaluate(const Env& env) const {
  return env.event;
}

string IntegralExpression::EventLookupNode::str() const {
  return "V_Event";
}

IntegralExpression::V1PresenceLookupNode::V1PresenceLookupNode() {}

bool IntegralExpression::V1PresenceLookupNode::operator==(const Node& other) const {
  return dynamic_cast<const V1PresenceLookupNode*>(&other) != nullptr;
}

int64_t IntegralExpression::V1PresenceLookupNode::evaluate(const Env& env) const {
  return env.v1_present ? 1 : 0;
}

string IntegralExpression::V1PresenceLookupNode::str() const {
  return "V_V1Present";
}

IntegralExpression::ConstantNode::ConstantNode(int64_t value)
    : value(value) {}

bool IntegralExpression::ConstantNode::operator==(const Node& other) const {
  try {
    const ConstantNode& other_const = dynamic_cast<const ConstantNode&>(other);
    return other_const.value == this->value;
  } catch (const bad_cast&) {
    return false;
  }
}

int64_t IntegralExpression::ConstantNode::evaluate(const Env&) const {
  return this->value;
}

string IntegralExpression::ConstantNode::str() const {
  return phosg::string_printf("%" PRId64, this->value);
}

unique_ptr<const IntegralExpression::Node> IntegralExpression::parse_expr(string_view text) {
  // Strip off spaces and fully-enclosing parentheses
  for (;;) {
    size_t starting_size = text.size();
    while (text.at(0) == ' ') {
      text = text.substr(1);
    }
    while (text.at(text.size() - 1) == ' ') {
      text = text.substr(0, text.size() - 1);
    }
    if (text.at(0) == '(' && text.at(text.size() - 1) == ')') {
      // It doesn't suffice to just check the first ant last characters, since
      // text could be like "(a) && (b)". Instead, we ignore the first and last
      // characters, and don't strip anything if the internal parentheses are
      // unbalanced.
      size_t paren_level = 1;
      for (size_t z = 1; z < text.size() - 1; z++) {
        if (text[z] == '(') {
          paren_level++;
        } else if (text[z] == ')') {
          paren_level--;
          if (paren_level == 0) {
            break;
          }
        }
      }
      if (paren_level > 0) {
        text = text.substr(1, text.size() - 2);
      }
    }
    if (text.size() == starting_size) {
      break;
    }
  }
  if (text.empty()) {
    throw runtime_error("invalid expression");
  }

  // Check for binary operators at the root level
  using BinType = BinaryOperatorNode::Type;
  static const vector<vector<pair<std::string, BinaryOperatorNode::Type>>> binary_operator_levels = {
      {{make_pair("||", BinType::LOGICAL_OR)}},
      {{make_pair("&&", BinType::LOGICAL_AND)}},
      {{make_pair("|", BinType::BITWISE_OR)}},
      {{make_pair("^", BinType::BITWISE_XOR)}},
      {{make_pair("&", BinType::BITWISE_AND)}},
      {{make_pair("==", BinType::EQUAL)}, {make_pair("!=", BinType::NOT_EQUAL)}},
      {{make_pair("<=", BinType::LESS_OR_EQUAL)}, {make_pair(">=", BinType::GREATER_OR_EQUAL)}, {make_pair("<", BinType::LESS_THAN)}, {make_pair(">", BinType::GREATER_THAN)}},
      {{make_pair("<<", BinType::LEFT_SHIFT)}, {make_pair(">>", BinType::RIGHT_SHIFT)}},
      {{make_pair("+", BinType::ADD)}, {make_pair("-", BinType::SUBTRACT)}},
      {{make_pair("*", BinType::MULTIPLY)}, {make_pair("/", BinType::DIVIDE)}, {make_pair("%", BinType::MODULUS)}},
  };
  for (const auto& operators : binary_operator_levels) {
    size_t paren_level = 0;
    for (size_t z = 0; z < text.size() - 1; z++) {
      if (text[z] == '(') {
        paren_level++;
        continue;
      } else if (text[z] == ')') {
        paren_level--;
        continue;
      }
      if (!paren_level) {
        for (const auto& oper : operators) {
          // Awful hack (because I'm too lazy to add a tokenization step): if
          // the operator is followed or preceded by another copy of itself,
          // don't match it (this prevents us from matching & when the token is
          // actually &&)
          if ((text.size() > z + oper.first.size()) &&
              ((z < oper.first.size()) || (text.compare(z - oper.first.size(), oper.first.size(), oper.first) != 0)) &&
              (text.compare(z, oper.first.size(), oper.first) == 0) &&
              (text.compare(z + oper.first.size(), oper.first.size(), oper.first) != 0)) {
            auto left = IntegralExpression::parse_expr(text.substr(0, z));
            auto right = IntegralExpression::parse_expr(text.substr(z + oper.first.size()));
            return make_unique<BinaryOperatorNode>(oper.second, std::move(left), std::move(right));
          }
        }
      }
    }
  }

  // Check for unary operators
  if (text[0] == '!') {
    return make_unique<UnaryOperatorNode>(UnaryOperatorNode::Type::LOGICAL_NOT,
        IntegralExpression::parse_expr(text.substr(1)));
  } else if (text[0] == '~') {
    return make_unique<UnaryOperatorNode>(UnaryOperatorNode::Type::BITWISE_NOT,
        IntegralExpression::parse_expr(text.substr(1)));
  } else if (text[0] == '-') {
    return make_unique<UnaryOperatorNode>(UnaryOperatorNode::Type::NEGATIVE,
        IntegralExpression::parse_expr(text.substr(1)));
  }

  // Check for env lookups
  if (text.starts_with("F_")) {
    char* endptr = nullptr;
    uint64_t flag = strtoul(text.data() + 2, &endptr, 16);
    if (endptr != text.data() + text.size()) {
      throw runtime_error("invalid flag lookup token");
    }
    if (flag >= 0x400) {
      throw runtime_error("invalid flag index");
    }
    return make_unique<FlagLookupNode>(flag);
  }
  if (text.starts_with("CC_")) {
    Episode episode;
    if (text.starts_with("CC_Ep1_")) {
      episode = Episode::EP1;
    } else if (text.starts_with("CC_Ep2_")) {
      episode = Episode::EP2;
    } else {
      throw runtime_error("invalid challenge episode");
    }
    char* endptr = nullptr;
    uint64_t stage_index = strtoul(text.data() + 7, &endptr, 0) - 1;
    if (endptr != text.data() + text.size()) {
      throw runtime_error("invalid challenge completion lookup token");
    }
    if ((episode == Episode::EP1 && stage_index > 8) || (episode == Episode::EP2 && stage_index > 4)) {
      throw runtime_error("invalid challenge stage index");
    }
    return make_unique<ChallengeCompletionLookupNode>(episode, stage_index);
  }
  if (text.starts_with("T_")) {
    return make_unique<TeamRewardLookupNode>(string(text.substr(2)));
  }
  if (text == "V_NumPlayers") {
    return make_unique<NumPlayersLookupNode>();
  }
  if (text == "V_Event") {
    return make_unique<EventLookupNode>();
  }
  if (text == "V_V1Present") {
    return make_unique<V1PresenceLookupNode>();
  }

  // Check for constants
  if (text == "true") {
    return make_unique<ConstantNode>(1);
  }
  if (text == "false") {
    return make_unique<ConstantNode>(0);
  }
  try {
    size_t endpos;
    int64_t v = stoll(string(text), &endpos, 0);
    if (endpos == text.size()) {
      return make_unique<ConstantNode>(v);
    }
  } catch (const exception&) {
  }
  throw runtime_error("unparseable expression");
}
