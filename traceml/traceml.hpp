#pragma once
#include "../limestone/foundation.hpp"
#include "../unisel/unisel.hpp"
namespace limestone::traceml {
struct Expr {
  enum class Kind { Symbol, Integer, Apply, Lambda, If, Begin };
  Kind kind; std::string atom; int64_t integer=0; std::vector<std::shared_ptr<Expr>> children;
  size_t offset=0; uint32_t line=1, column=1;
};
Result<std::shared_ptr<Expr>> parse(std::string_view);
struct Trace { uint32_t id; std::shared_ptr<Expr> guard, body; };
struct Program { std::vector<std::shared_ptr<Expr>> forms; };
Result<Program> compile(std::string_view);
Result<int> verify(const Program&);
// Call-by-name lexical closures; integer primitives are strict, left-to-right.
Result<int64_t> evaluate(const Program&, size_t step_limit=100000);
// Structured lowering errors are available through this checked entry point.
Result<std::string> lower_checked(const Program&);
std::string lower_to_machineir(const Program&);
// Portable constant/return adapter used by the orchestration convenience API.
struct PortableLowering {
  unisel::Program program;
  std::vector<unisel::Pattern> patterns;
  std::vector<schedrow::Instruction> instruction_models;
};
PortableLowering lower_graph(int64_t value);
}
