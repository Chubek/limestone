#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::tunah {
struct Term { std::string op; std::vector<uint32_t> children; std::optional<int64_t> constant; std::vector<Term> arguments; };
struct RewriteRule { std::string name; Term lhs, rhs; };
struct Limits { size_t iterations=20, nodes=10000, classes=10000; uint64_t time_ms=0; std::function<bool()> cancelled; };
struct SaturationResult {
  size_t rewrites=0; bool saturated=false;
  std::string expression; size_t cost=0, nodes=0, iterations=0;
  bool limit_reached=false;
  std::vector<std::string> trace;
};
class Session {
  std::vector<RewriteRule> rules_;
  std::unordered_map<std::string,size_t> operators_;
public:
  void add_rule(RewriteRule rule) { rules_.push_back(std::move(rule)); }
  const std::vector<RewriteRule>& rules() const { return rules_; }
  void define_operator(std::string name, size_t arity) { operators_[std::move(name)]=arity; }
  Result<int> load_rules(std::string_view specification);
  // Rules are caller-supplied equivalences; operators with operands need signatures.
  Result<SaturationResult> saturate(std::string_view root, Limits = {}) const;
};
}
