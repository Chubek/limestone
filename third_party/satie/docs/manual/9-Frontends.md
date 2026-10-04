# Language Frontends

Satie reads seven input languages. The C++ frontends live in
`include/SatieFrontend*.hpp` (compiled into the `satie` library); the D
ports live in `frontends/*.d` with inline `unittest` blocks. Sample inputs
and a tour binary are in `examples/Frontends/` (`example_frontends`),
and `satie-cli --format` selects a language directly.

## CNF / DIMACS

DIMACS stays in `Common.hpp` (`DimacsParser`). The CNF frontend adds full
Boolean formulas (`SatieFrontendCNF.hpp`): `&`/`&&`, `|`/`||`, `^`,
`->` (right-associative), `<->`, `~`/`!`, `true`/`false`, parentheses.
`parse_bool_formula` compiles to CNF by Tseitin encoding (n-ary `&`/`|`
share one auxiliary); the result is equisatisfiable and the symbol table
maps names back for model printing.

The facade/CLI `cnf` format is the restricted clausal DSL; the full
expression frontend is selected by CLI `--format bool` or the API below.
Read names through the returned symbol table, not through auxiliary IDs:

```cpp
#include "Satie.hpp"
#include "SatieFrontendCNF.hpp"
#include <cassert>

int main()
{
    auto formula = satie::frontend::parse_bool_formula("a & (a -> b)");
    auto result = satie::solve(formula.cnf);
    assert(result.satisfiable());
    auto b = formula.symbols.lookup("b");
    assert(b.has_value());
    assert(result.assignment.get_var(*b) == satie::Value::TRUE);
}
```

The operator precedence is `<->`, `->`, OR, XOR, AND, unary negation,
then atoms (from lowest to highest). Tseitin variables follow the
user-named variables; count/project original models accordingly.

## WCNF / MaxSAT

`SatieFrontendWCNF.hpp` parses DIMACS-WCNF (`p wcnf`, weights, `top`) and
`solve_maxsat` optimizes by binary search over relaxation blockers with a
sequential counter. The reported cost is optimal when the hard clauses are
satisfiable; hard-UNSAT is reported exactly.

Rows with a weight at least `top` are hard; the others are soft. Cost is
the sum of falsified soft-clause weights. If omitted, `top` defaults to
one plus the sum of input weights. The current optimization expands
weighted clauses into unit-weight copies, so encoding size depends on
the total soft weight, not just the number of source clauses.

```cpp
#include "SatieFrontendWCNF.hpp"
#include <cassert>

int main()
{
    auto problem = satie::frontend::parse_wcnf_text(
        "p wcnf 1 2 10\n10 1 0\n2 -1 0\n");
    auto result = satie::frontend::solve_maxsat(problem);
    assert(result.status == satie::frontend::MaxSATStatus::Optimal);
    assert(result.cost == 2);
}
```

`MaxSATResult` carries its own status (`Optimal`, `UnsatHard`, or
`Unknown`), cost, and assignment. The implementation currently uses
CDCL regardless of the supplied engine argument.

## OPB / pseudo-Boolean

`SatieFrontendOPB.hpp` parses the competition subset (`*` comments,
optional `min:`/`max:`, `coeff*xN ... (>=|<=|=) rhs ;`) and bit-blasts
constraints into signed two's-complement arithmetic solved by CDCL.
Models are re-validated by integer evaluation; `optimize_pb_min/max`
binary-search the objective. Working widths are derived from coefficient
magnitudes. A model that fails validation yields UNKNOWN; parse/argument
errors are separate failures, and coefficients/intermediate sums must fit
the implementation's integer types.

`solve_pb` checks feasibility; `PBProblem::objective` is optimized only
when invoking an optimization API. The returned `PBOptimum` has a
`SolveStatus`, objective `value`, and assignment. The current CLI's OPB
route invokes feasibility only. Use small representable coefficients
and inspect the status before interpreting an objective value.

```cpp
#include "SatieFrontendOPB.hpp"
#include <cassert>

int main()
{
    auto problem = satie::frontend::parse_opb_text(
        "min: 2 x1 + 3 x2;\n1 x1 + 1 x2 >= 1;\n");
    auto result = satie::frontend::optimize_pb_min(problem);
    assert(result.status == satie::SolveStatus::SAT);
    assert(result.value == 2);
}
```

## SMT-LIB2 (subset)

`SatieFrontendSMTLIB2.hpp` accepts `set-logic` (QF_LIA/QF_UF/QF_LRA/ALL),
`declare-const` (`Bool`/`Int`), `assert`, `check-sat`, `get-model`,
`exit` (plus ignored `set-option`/`set-info`). Purely Boolean scripts go
through Tseitin+CDCL (full); conjunctions of linear integer comparisons
go through the LIA solver's bounded integer search with model validation;
mixed Boolean+theory structure answers UNKNOWN. Feasible integer scripts
can also return UNKNOWN. Models map names to `bool`/`int64`.

For example, this conjunctive Int script is within the implemented route:

```lisp
(set-logic QF_LIA)
(declare-const x Int)
(assert (>= x 0))
(assert (<= x 3))
(check-sat)
(get-model)
```

`solve_smtlib2_text` returns `SMTCheckResult`, whose model has `bools`
and `ints` maps. The script must execute `check-sat`; request `get-model`
only after SAT. After UNKNOWN or UNSAT it raises a parse error.
Logic names do not imply support for all their
standard sorts/operators: declarations currently accept Bool/Int,
not Real, bit-vector, or uninterpreted-function declarations. `push`,
`pop`, quantified terms, and full mixed Boolean/theory solving are
outside this frontend's supported commands/fragments.

## IPASIR (incremental)

`SatieFrontendIPASIR.hpp` offers `IpasirSolver` (`add` with `0`
terminators, `assume`, `solve` returning 10/20/0, `val`, `failed`) plus C
bindings (`satie_ipasir_init/release/add/assume/solve/val/failed`).
Assumptions are first-class solver citizens: backtracking re-asserts
dropped assumptions, learnt clauses falsified by assumptions prove UNSAT,
and `failed` reports a deletion-minimized core. The D port
(`frontends/IPASIR.d`) binds the same C entry points.

```cpp
#include "SatieFrontendIPASIR.hpp"
#include <cassert>

int main()
{
    satie::frontend::IpasirSolver solver;
    solver.add(1);
    solver.add(0); // Complete the persistent unit clause.
    solver.assume(-1);
    assert(solver.solve() == 20);
    assert(solver.failed(-1) == 1);
    assert(solver.solve() == 10); // Assumptions were cleared.
    assert(solver.val(1) == 1);
}
```

An unterminated pending clause is retained but is not included in a
solve. Terminate every intended clause with `add(0)`. `val(v)` takes
a positive variable ID and returns `v`, `-v`, or `0`; `failed(lit)`
queries actual assumption literals after UNSAT. Assumptions must lie
within the completed clauses' CNF variable range. The persistent input
is re-solved with a fresh CDCL search on each call; learned clauses are
not retained across calls. The C linkage is prefixed `satie_ipasir_*`.

## SatieLisp

`SatieFrontendSatieLisp.hpp` parses `(and/or/not/xor/=>/<=>/nand/nor)`
over identifiers (`;` comments) into Tseitin CNF; `solve_satielisp`
returns a named model (auxiliaries filtered).

```lisp
(and (or a b) (=> a c) (not b))
```

`parse_satielisp` returns `LispFormula`; `solve_satielisp` returns
`LispResult` with a status and a map of user-named Booleans. Operator
aliases `implies` and `iff` are also accepted. `(and)` denotes true and
`(or)` false; negation requires one argument and binary operators two.

## D ports

`frontends/Common.d` (DIMACS), `CNF.d` (formulas+Tseitin), `WCNF.d`
(parse+cost), `OPB.d` (parse+eval), `SMT-LIB2.d` (S-expressions),
`SatieLisp.d` (parse+eval), `IPASIR.d` (bindings+wrapper).

When a D compiler (`ldc2`/`dmd`/`gdc`) is found, the default CMake build
compiles all seven modules into `frontends/libsatie_frontends.a`, exposed
as the `satie_frontends` target and `satie::frontends` alias. This library
is built even with `BUILD_TESTING=OFF`. Set `D_COMPILER` to select a compiler
or `BUILD_D_FRONTENDS=OFF` to disable the D library.

With testing enabled, the build also compiles `d_frontends_unittest`
(inline unittests) and `d_ipasir_smoke` (links the D wrapper from the
frontend archive against the C++ solver). CTest runs these executables.

Installation includes the archive and import-compatible sources under
`share/satie/frontends/satie/`. D consumers can add
`-I<prefix>/share/satie/frontends` to resolve the `satie.*` modules.

The D modules' public scope is:

| Module | D entry points |
|---|---|
| `satie.common` | `parseDimacs`, `toDimacs`, CNF types. |
| `satie.cnf` | `parseBoolFormula`, Boolean-to-CNF encoding. |
| `satie.wcnf` | `parseWcnf`, `evaluateCost`. |
| `satie.opb` | `parseOpb`, constraint evaluation with `holdsAll`. |
| `satie.smt_lib2` | `parseScript`, S-expression helpers, `evaluateGroundBool`. |
| `satie.satielisp` | `parseSatieLisp`, `evaluate`, `toFormulaString`. |
| `satie.ipasir` | `IpasirSolver` and C solver bindings. |

The D parsing/evaluation ports do not duplicate every C++ optimization or
typed-theory solve API. IPASIR supplies their implemented C++ solver
connection. Follow [Build and Installation](10-Build-and-Installation.md)
for compiler selection and [CLI and REPL](11-CLI-and-REPL.md) for input
routes and output conventions.
