#include "QuestAvailabilityExpression.hh"

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

QuestAvailabilityExpression::QuestAvailabilityExpression(const string& text)
    : root(this->parse_expr(text)) {}

QuestAvailabilityExpression::OrNode::OrNode(unique_ptr<const Node>&& left, unique_ptr<const Node>&& right)
    : left(std::move(left)),
      right(std::move(right)) {}

bool QuestAvailabilityExpression::OrNode::operator==(const Node& other) const {
  try {
    const OrNode& other_or = dynamic_cast<const OrNode&>(other);
    return *other_or.left == *this->left && *other_or.right == *this->right;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::OrNode::evaluate(
    const QuestFlagsForDifficulty& flags, shared_ptr<const TeamIndex::Team> team) const {
  return this->left->evaluate(flags, team) || this->right->evaluate(flags, team);
}

string QuestAvailabilityExpression::OrNode::str() const {
  return "(" + this->left->str() + ") || (" + this->right->str() + ")";
}

QuestAvailabilityExpression::AndNode::AndNode(unique_ptr<const Node>&& left, unique_ptr<const Node>&& right)
    : left(std::move(left)),
      right(std::move(right)) {}

bool QuestAvailabilityExpression::AndNode::operator==(const Node& other) const {
  try {
    const AndNode& other_and = dynamic_cast<const AndNode&>(other);
    return *other_and.left == *this->left && *other_and.right == *this->right;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::AndNode::evaluate(
    const QuestFlagsForDifficulty& flags, shared_ptr<const TeamIndex::Team> team) const {
  return this->left->evaluate(flags, team) && this->right->evaluate(flags, team);
}

string QuestAvailabilityExpression::AndNode::str() const {
  return "(" + this->left->str() + ") && (" + this->right->str() + ")";
}

QuestAvailabilityExpression::NotNode::NotNode(unique_ptr<const Node>&& sub)
    : sub(std::move(sub)) {}

bool QuestAvailabilityExpression::NotNode::operator==(const Node& other) const {
  try {
    const NotNode& other_not = dynamic_cast<const NotNode&>(other);
    return *other_not.sub == *this->sub;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::NotNode::evaluate(
    const QuestFlagsForDifficulty& flags, shared_ptr<const TeamIndex::Team> team) const {
  return !this->sub->evaluate(flags, team);
}

string QuestAvailabilityExpression::NotNode::str() const {
  return "!(" + this->sub->str() + ")";
}

QuestAvailabilityExpression::FlagLookupNode::FlagLookupNode(uint16_t flag_index)
    : flag_index(flag_index) {}

bool QuestAvailabilityExpression::FlagLookupNode::operator==(const Node& other) const {
  try {
    const FlagLookupNode& other_flag = dynamic_cast<const FlagLookupNode&>(other);
    return other_flag.flag_index == this->flag_index;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::FlagLookupNode::evaluate(
    const QuestFlagsForDifficulty& flags, shared_ptr<const TeamIndex::Team>) const {
  return flags.get(this->flag_index);
}

string QuestAvailabilityExpression::FlagLookupNode::str() const {
  return string_printf("F_%04hX", this->flag_index);
}

QuestAvailabilityExpression::TeamRewardLookupNode::TeamRewardLookupNode(const string& reward_name)
    : reward_name(reward_name) {}

bool QuestAvailabilityExpression::TeamRewardLookupNode::operator==(const Node& other) const {
  try {
    const TeamRewardLookupNode& other_team_reward = dynamic_cast<const TeamRewardLookupNode&>(other);
    return other_team_reward.reward_name == this->reward_name;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::TeamRewardLookupNode::evaluate(
    const QuestFlagsForDifficulty&, shared_ptr<const TeamIndex::Team> team) const {
  return team && team->has_reward(this->reward_name);
}

string QuestAvailabilityExpression::TeamRewardLookupNode::str() const {
  return "T_" + this->reward_name;
}

QuestAvailabilityExpression::ConstantNode::ConstantNode(bool value)
    : value(value) {}

bool QuestAvailabilityExpression::ConstantNode::operator==(const Node& other) const {
  try {
    const ConstantNode& other_const = dynamic_cast<const ConstantNode&>(other);
    return other_const.value == this->value;
  } catch (const bad_cast&) {
    return false;
  }
}

bool QuestAvailabilityExpression::ConstantNode::evaluate(
    const QuestFlagsForDifficulty&, shared_ptr<const TeamIndex::Team>) const {
  return this->value;
}

string QuestAvailabilityExpression::ConstantNode::str() const {
  return this->value ? "true" : "false";
}

unique_ptr<const QuestAvailabilityExpression::Node> QuestAvailabilityExpression::parse_expr(string_view text) {
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

  // Check for binary operators at the root level
  size_t paren_level = 0;
  size_t and_pos = 0;
  size_t or_pos = 0;
  for (size_t z = 0; z < text.size() - 1; z++) {
    if (text[z] == '(') {
      paren_level++;
    } else if (text[z] == ')') {
      paren_level--;
    } else if ((text[z] == '&') && (text[z + 1] == '&') && !paren_level) {
      and_pos = z;
    } else if ((text[z] == '|') && (text[z + 1] == '|') && !paren_level) {
      or_pos = z;
    }
  }
  if ((or_pos && (!and_pos || (and_pos > or_pos)))) {
    auto left = QuestAvailabilityExpression::parse_expr(text.substr(0, or_pos));
    auto right = QuestAvailabilityExpression::parse_expr(text.substr(or_pos + 2));
    return make_unique<OrNode>(std::move(left), std::move(right));
  }
  if ((and_pos && (!or_pos || (or_pos > and_pos)))) {
    auto left = QuestAvailabilityExpression::parse_expr(text.substr(0, and_pos));
    auto right = QuestAvailabilityExpression::parse_expr(text.substr(and_pos + 2));
    return make_unique<AndNode>(std::move(left), std::move(right));
  }

  // Check for not operator
  if (text.at(0) == '!') {
    auto sub = QuestAvailabilityExpression::parse_expr(text.substr(1));
    return make_unique<NotNode>(std::move(sub));
  }

  // Check for constants
  if (text == "true") {
    return make_unique<ConstantNode>(true);
  }
  if (text == "false") {
    return make_unique<ConstantNode>(false);
  }

  // Check for flag lookups
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

  if (text.starts_with("T_")) {
    return make_unique<TeamRewardLookupNode>(string(text.substr(2)));
  }

  throw runtime_error("unparseable expression");
}
