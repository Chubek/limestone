#pragma once
#include "../limestone/foundation.hpp"
#include "../schedrow/schedrow.hpp"
namespace limestone::unisel {
using NodeId=uint32_t; using PatternId=uint32_t;
struct Value { NodeId id; std::string type; };
struct Node {
  NodeId id; std::string op; std::vector<NodeId> inputs; std::optional<int64_t> constant;
   std::string type; uint32_t block=0; bool required=true, side_effect=false;
   bool produces_value=true;
};
struct PatternTree {
  // Empty op is a boundary operand; binding enforces repeated operand identity.
  std::string op, binding, type;
  std::vector<PatternTree> inputs;
  std::optional<std::pair<int64_t,int64_t>> immediate;
};
struct Pattern {
  PatternId id; std::string name, root_op, instruction; std::vector<std::string> operands; int cost=1;
  std::optional<PatternTree> tree;
  bool supports_side_effects=false;
};
struct Candidate {
  PatternId pattern; NodeId root; std::vector<NodeId> covered; int cost; std::string reason;
  std::vector<NodeId> inputs, outputs;
};
struct Selection { std::vector<Candidate> selected; int cost=0; };
struct Program { std::vector<Node> nodes; std::vector<NodeId> outputs; std::vector<schedrow::Dependency> dependencies; };
// Signed, 1-based literals. This inspectable model contains no vendor types.
struct ConstraintModel { std::vector<Candidate> candidates; std::vector<std::vector<int32_t>> clauses; };
std::vector<Candidate> match(const Program&, const std::vector<Pattern>&);
Result<int> validate(const Program&, const std::vector<Pattern>&);
Result<ConstraintModel> build_model(const Program&, const std::vector<Pattern>&);
Result<Selection> solve(const Program&, const std::vector<Pattern>&);
Result<Selection> solve_greedy(const Program&, const std::vector<Pattern>&);
Result<schedrow::Region> emit_scheduler(const Program&, const std::vector<Pattern>&, const Selection&);
}
