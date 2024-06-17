#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "PlayerSubordinates.hh"
#include "QuestScript.hh"
#include "StaticGameData.hh"
#include "TeamIndex.hh"

class IntegralExpression {
public:
  struct Env {
    const QuestFlagsForDifficulty* flags;
    const PlayerRecordsChallengeBB* challenge_records;
    std::shared_ptr<const TeamIndex::Team> team;
    size_t num_players;
    uint8_t event;
    bool v1_present;
  };

  IntegralExpression(const std::string& text);
  ~IntegralExpression() = default;
  inline bool operator==(const IntegralExpression& other) const {
    return this->root->operator==(*other.root);
  }
  inline bool operator!=(const IntegralExpression& other) const {
    return !this->operator==(other);
  }
  inline int64_t evaluate(const Env& env) const {
    return this->root->evaluate(env);
  }
  inline std::string str() const {
    return this->root->str();
  }

protected:
  class Node {
  public:
    virtual ~Node() = default;
    virtual bool operator==(const Node& other) const = 0;
    inline bool operator!=(const Node& other) const {
      return !this->operator==(other);
    }
    virtual int64_t evaluate(const Env& env) const = 0;
    virtual std::string str() const = 0;

  protected:
    Node() = default;
  };

  class BinaryOperatorNode : public Node {
  public:
    enum class Type {
      LOGICAL_OR = 0,
      LOGICAL_AND,
      BITWISE_OR,
      BITWISE_AND,
      BITWISE_XOR,
      LEFT_SHIFT,
      RIGHT_SHIFT,
      LESS_THAN,
      GREATER_THAN,
      LESS_OR_EQUAL,
      GREATER_OR_EQUAL,
      EQUAL,
      NOT_EQUAL,
      ADD,
      SUBTRACT,
      MULTIPLY,
      DIVIDE,
      MODULUS,
    };
    BinaryOperatorNode(Type type, std::unique_ptr<const Node>&& left, std::unique_ptr<const Node>&& right);
    virtual ~BinaryOperatorNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    Type type;
    std::unique_ptr<const Node> left;
    std::unique_ptr<const Node> right;
  };

  class UnaryOperatorNode : public Node {
  public:
    enum class Type {
      LOGICAL_NOT = 0,
      BITWISE_NOT,
      NEGATIVE,
    };
    UnaryOperatorNode(Type type, std::unique_ptr<const Node>&& sub);
    virtual ~UnaryOperatorNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    Type type;
    std::unique_ptr<const Node> sub;
  };

  class FlagLookupNode : public Node {
  public:
    FlagLookupNode(uint16_t flag_index);
    virtual ~FlagLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    uint16_t flag_index;
  };

  class ChallengeCompletionLookupNode : public Node {
  public:
    ChallengeCompletionLookupNode(Episode episode, uint8_t stage_index);
    virtual ~ChallengeCompletionLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    Episode episode;
    uint8_t stage_index;
  };

  class TeamRewardLookupNode : public Node {
  public:
    TeamRewardLookupNode(const std::string& reward_name);
    virtual ~TeamRewardLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    std::string reward_name;
  };

  class NumPlayersLookupNode : public Node {
  public:
    NumPlayersLookupNode();
    virtual ~NumPlayersLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;
  };

  class EventLookupNode : public Node {
  public:
    EventLookupNode();
    virtual ~EventLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;
  };

  class V1PresenceLookupNode : public Node {
  public:
    V1PresenceLookupNode();
    virtual ~V1PresenceLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;
  };

  class ConstantNode : public Node {
  public:
    ConstantNode(int64_t value);
    virtual ~ConstantNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual int64_t evaluate(const Env& env) const;
    virtual std::string str() const;

  protected:
    int64_t value;
  };

  std::unique_ptr<const Node> parse_expr(std::string_view text);

  std::unique_ptr<const Node> root;
};
