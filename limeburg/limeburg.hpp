#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::limeburg {
using NodeId=uint32_t; using RuleId=uint32_t;
struct Node { NodeId id; std::string op, type; std::vector<NodeId> children; int64_t imm=0; bool has_imm=false; };
struct Rule { RuleId id; std::string lhs, op, result; std::vector<std::string> operands; int cost=1; std::string instruction; int priority=0; };
struct RuleSet { std::vector<Rule> rules; std::unordered_map<std::string,uint32_t> nonterminals; };
struct Derivation { RuleId rule; int cost; std::vector<std::string> child_nt; };
struct Selection { std::string root_nt; std::unordered_map<NodeId,Derivation> chosen; int cost=0; };
Result<Selection> select(const std::vector<Node>&, NodeId, const RuleSet&, std::string_view root);
}
