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

class QuestAvailabilityExpression {
public:
  QuestAvailabilityExpression(const std::string& text);
  ~QuestAvailabilityExpression() = default;
  inline bool operator==(const QuestAvailabilityExpression& other) const {
    return this->root->operator==(*other.root);
  }
  inline bool operator!=(const QuestAvailabilityExpression& other) const {
    return !this->operator==(other);
  }
  inline bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const {
    return this->root->evaluate(flags, team);
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
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const = 0;
    virtual std::string str() const = 0;

  protected:
    Node() = default;
  };

  class OrNode : public Node {
  public:
    OrNode(std::unique_ptr<const Node>&& left, std::unique_ptr<const Node>&& right);
    virtual ~OrNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    std::unique_ptr<const Node> left;
    std::unique_ptr<const Node> right;
  };

  class AndNode : public Node {
  public:
    AndNode(std::unique_ptr<const Node>&& left, std::unique_ptr<const Node>&& right);
    virtual ~AndNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    std::unique_ptr<const Node> left;
    std::unique_ptr<const Node> right;
  };

  class NotNode : public Node {
  public:
    NotNode(std::unique_ptr<const Node>&& sub);
    virtual ~NotNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    std::unique_ptr<const Node> sub;
  };

  class FlagLookupNode : public Node {
  public:
    FlagLookupNode(uint16_t flag_index);
    virtual ~FlagLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    uint16_t flag_index;
  };

  class TeamRewardLookupNode : public Node {
  public:
    TeamRewardLookupNode(const std::string& reward_name);
    virtual ~TeamRewardLookupNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    std::string reward_name;
  };

  class ConstantNode : public Node {
  public:
    ConstantNode(bool value);
    virtual ~ConstantNode() = default;
    virtual bool operator==(const Node& other) const;
    virtual bool evaluate(const QuestFlagsForDifficulty& flags, std::shared_ptr<const TeamIndex::Team> team) const;
    virtual std::string str() const;

  protected:
    bool value;
  };

  std::unique_ptr<const Node> parse_expr(std::string_view text);

  std::unique_ptr<const Node> root;
};
