# Using Satie

## Public interface

Primary include:

```cpp
#include "Satie.hpp"
```

Link the compiled `satie::satie` CMake target. The facade and memory/parser
services have out-of-line definitions; see
[Build and Installation](10-Build-and-Installation.md). `SatieSAT.hpp`
also supplies the `SolverType` alias and `solve_sat` convenience overloads.

Core API:

- `satie::Solver`
- `satie::Engine`
- `satie::SolveOptions`
- `satie::SolverReport`
- `satie::parse_auto`
- `satie::parse_auto_file`
- `satie::parse(text, format)`
- `satie::parse_file(path, format)`
- `satie::solve` overloads
- `satie::solve_with_report`

## Engine selection

`Engine` values:

- `Engine::Native`: exhaustive search baseline.
- `Engine::DPLL`: unit propagation + pure literal + branching.
- `Engine::CDCL`: clause learning + non-chronological backjumping.

Default engine in unified API: `Engine::CDCL`.

## Input formats

The facade's `ParseFormat` selects two input notations:

- CNF DSL: `(a | ~b) & (c | d)`
- DIMACS CNF:

```text
c sample
p cnf 3 2
1 -2 0
3 0
```

`parse_auto` dispatches by lexical shape:

- leading `p` line after blank/comment lines => DIMACS;
- otherwise => CNF DSL.

Headerless DIMACS needs an explicit format. Other languages use the
[frontend APIs](9-Frontends.md); `ParseFormat::CNF` means the restricted
clausal DSL, not the full Boolean-expression frontend.

Deterministic parsing API:

- `parse(text, ParseFormat::CNF)`;
- `parse(text, ParseFormat::DIMACS)`;
- `parse(text, ParseFormat::Auto)`.

## Minimal solve flow

```cpp
using namespace satie;

CNF cnf = parse_auto("(a | b) & (~a | c)");
SolveResult result = solve(cnf, Engine::CDCL);

if (result.satisfiable()) {
  // result.assignment.get_var(1) ...
}
```

## Stateful solver flow

```cpp
using namespace satie;

Solver solver;
solver.load(parse_auto("(a | b) & (~a | c)"));

SolveResult sat = solver.solve(Engine::DPLL);
bool ok = solver.satisfiable(Engine::Native);
```

## Statistics flow

```cpp
using namespace satie;

Solver solver(parse_auto("(a | b) & (~a | c)"));
SolverReport report = solver.solve_with_report({.engine = Engine::CDCL});

if (report.statistics.cdcl) {
  auto st = *report.statistics.cdcl;
  // st.decisions, st.conflicts, st.learned_clauses, st.restarts ...
}
```

## File-based flow

```cpp
using namespace satie;

CNF cnf = parse_auto_file("problem.cnf");
SolverReport report = solve_with_report(cnf, {.engine = Engine::DPLL});
```

## Result contract

`SolveResult`:

- `status`: `SAT`, `UNSAT`, `UNKNOWN`.
- `assignment`: a satisfying Boolean model when `SAT`; do not read it as
  a model when the status is `UNSAT` or `UNKNOWN`.

Convenience predicates:

- `satisfiable()`
- `unsatisfiable()`

These predicates both return false for UNKNOWN. Test `status` explicitly
when a budgeted search or theory solver can be inconclusive.

## CNF and assignment conventions

`Var` and `Lit` are signed 32-bit integers. SAT variables start at one;
a negative literal represents negation. Zero is a DIMACS terminator.
The CNF container sorts literals by variable, removes duplicates and
zeros, and rejects the minimum signed literal whose magnitude is not
representable. It retains tautological clauses as input data.

`CNF{}` is the empty, satisfiable conjunction; `CNF({{}})` contains an
empty clause and is UNSAT. `set_declared_variable_count(n)` preserves
unused variables, including when exporting DIMACS or counting models.
`operator&` combines clauses using the same variable IDs; it does not
rename colliding variables from independent problems.

`Assignment::get_var(v)` and `get_literal(lit)` return the tri-valued
`Value` enum. DPLL may stop with unknown don't-care variables once every
clause is satisfied. `completed()` supplies values for unknown variables.
Validate a returned SAT model with `is_formula_satisfied`:

```cpp
#include "Satie.hpp"
#include <cassert>

int main()
{
    satie::CNF problem({{1, 2}, {-1, 3}});
    auto result = satie::solve(problem, satie::Engine::CDCL);
    assert(result.satisfiable());
    assert(satie::is_formula_satisfied(problem, result.assignment));
    auto total = result.assignment.completed();
    assert(total.fully_assigned());
}
```

## Model counting and concrete engine controls

`NaiveSolver::count_models()` and `count_models_naive(cnf)` enumerate
all satisfying assignments over the CNF's variable universe. Counting
resets native statistics and rejects 64 or more variables with
`std::overflow_error`. This is exhaustive counting, not sampling.

The `Solver` facade constructs an engine per call. For assumptions,
conflict budgets, restarts, learned clauses, or cores, use `CDCLSolver`
directly. [Solver Controls and Diagnostics](16-Solver-Controls-and-Diagnostics.md)
documents those settings and all statistics fields.

## CNF/DIMACS interop

Helpers from `Common.hpp` remain available through `Satie.hpp`:

- `cnf_to_dimacs(cnf)`
- `dimacs_to_cnf(dimacs)`
- `to_dimacs_string(cnf)`
- `parse_dimacs(stream)`

Solvers canonicalize through CNF↔DIMACS conversion path during `load`.

## DAG visualization

Graph construction and DOT emission:

- `ProblemDAG dag = cnf_to_dag(cnf);`
- `std::string dot = dag_to_dot(dag);`
- `std::string dot2 = cnf_to_dot(cnf);`

Render with Graphviz:

```bash
dot -Tpng problem.dot -o problem.png
```

## Error model

Parsing operations may throw:

- `satie::ParseError` with line/column diagnostics.
- `std::runtime_error` on file I/O failures.

DIMACS strict checks include:

- malformed `p cnf` header;
- clause count mismatch;
- out-of-range or undeclared-variable literals;
- unterminated clause not ending in `0`.

## Integration guidance

- Use `Engine::CDCL` for production default.
- Use `Engine::DPLL` for smaller formulas and deterministic teaching traces.
- Use `Engine::Native` only for baseline verification and model counting experiments.
- Normalize and export DIMACS for reproducible benchmarks.
- Persist `SolverReport` statistics for regression tracking.
- CDCL uses Luby-scheduled restarts; monitor `statistics.cdcl->restarts`.

For C handles, read [C and Foreign Interfaces](12-C-and-Foreign-Interfaces.md).
For scripting and reusable helpers, read [Plugins and Lua](13-Plugins-and-Lua.md)
and [Standard Library](14-Standard-Library.md).
