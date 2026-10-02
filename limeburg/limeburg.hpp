#pragma once
#include "../limestone/foundation.hpp"
#include "../schedrow/schedrow.hpp"
namespace limestone::limeburg {
using NodeId=uint32_t; using RuleId=uint32_t;
struct Node { NodeId id; std::string op, type; std::vector<NodeId> children; int64_t imm=0; bool has_imm=false; };
struct Pattern {
  std::string op, nonterminal, type;
  std::vector<Pattern> children;
  std::optional<std::pair<int64_t,int64_t>> immediate;
};
struct Rule {
  RuleId id; std::string lhs, op, result; std::vector<std::string> operands; int cost=1; std::string instruction; int priority=0;
  std::string type;
  std::optional<std::pair<int64_t,int64_t>> immediate;
  std::optional<Pattern> pattern;
};
struct RuleSet { std::vector<Rule> rules; std::unordered_map<std::string,uint32_t> nonterminals; };
struct Derivation {
  RuleId rule; int cost; std::vector<std::string> child_nt; std::vector<NodeId> children, covered;
  bool operator==(const Derivation&) const = default;
};
struct Selection { std::string root_nt; std::unordered_map<NodeId,Derivation> chosen; int cost=0; std::optional<NodeId> root; };
Result<Selection> select(const std::vector<Node>&, NodeId, const RuleSet&, std::string_view root);
Result<int> validate(const RuleSet&);
Result<schedrow::Region> emit_scheduler(const std::vector<Node>&, const RuleSet&, const Selection&);
}
