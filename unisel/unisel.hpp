#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::unisel {
using NodeId=uint32_t; using PatternId=uint32_t;
struct Value { NodeId id; std::string type; };
struct Node { NodeId id; std::string op; std::vector<NodeId> inputs; std::optional<int64_t> constant; };
struct Pattern { PatternId id; std::string name, root_op, instruction; std::vector<std::string> operands; int cost=1; };
struct Candidate { PatternId pattern; NodeId root; std::vector<NodeId> covered; int cost; std::string reason; };
struct Selection { std::vector<Candidate> selected; int cost=0; };
struct Program { std::vector<Node> nodes; };
std::vector<Candidate> match(const Program&, const std::vector<Pattern>&);
Result<Selection> solve(const Program&, const std::vector<Pattern>&);
Result<Selection> solve_greedy(const Program&, const std::vector<Pattern>&);
}
