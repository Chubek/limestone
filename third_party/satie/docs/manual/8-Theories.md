# Theory Solvers (DPLL(T) Subsets)

Each theory header (`include/SatieBV.hpp`, `SatieEUF.hpp`, and so on)
pairs the Boolean engine with typed constraints for its fragment.
Bit-blasted theories extend a CNF; layered procedures first check the
loaded Boolean CNF and then check their separate theory constraints.
There is no general Boolean-literal-to-theory-atom mapping or
heterogeneous theory-combination loop. Construct theory constraints
through the concrete solver API.

The common surface is `theory_name`, `load(CNF)`, `problem()`, and
`check()`, alongside theory-specific declarations, constraints, models,
and bounds. `load` replaces the Boolean problem while preserving typed
constraints; `clear_theory` clears the latter. Use a fresh instance when
starting an unrelated typed problem.

Shared bit-blasting helpers (Tseitin gates, ripple-carry adders, unsigned
comparators, sequential-counter cardinality) live in
`include/SatieTheoryUtils.hpp`.

## Results, bounds, and arithmetic

- The implementation uses exact finite encodings in supported BV/Set
  fragments and model validation in the numerical searches.
- UNSAT routes include negative cycles, contradictory elimination rows,
  exhaustive finite-domain searches, and structural contradictions.
  Search-bound exhaustion only proves UNSAT when the configured domain
  actually covers the supported problem.
- Inconclusive searches return UNKNOWN. LIA can miss a model outside its
  encoding width; ISL, NRA, FP, ODE, and other capped searches have similar
  stopping limits.
- LP, MILP, NRA, FP, SymSolve, and ODE use floating-point arithmetic and
  tolerances. LP's rational-elimination algorithm is implemented with
  `double` coefficients, not arbitrary-precision exact rationals. Treat
  their numerical model checks according to each solver's tolerance,
  rather than assuming a machine-checkable exact proof certificate.

## Per-theory notes

- `BV`: fixed-width unsigned integers, `==`, `<`, `+` mod 2^w, plus
  constant variants. Complete via bit-blasting (widths 1..64).
- `EUF`: congruence closure over uninterpreted functions. Complete.
- `IDL`: `x - y <= c` via Bellman-Ford, with models. Complete.
- `ISL`: IDL plus `d | (x - offset)`. IDL-infeasible means UNSAT;
  otherwise bounded repair over explicitly bounded variables
  (`add_bound`), UNKNOWN when unbounded.
- `LIA`: integer linear constraints. Relaxation via LP first (relaxation
  infeasibility returns UNSAT), then bounded two's-complement candidate
  search with integer-validated models. An encoding/search miss means
  UNKNOWN, including for some feasible problems within the width.
- `LP`: Fourier-Motzkin feasibility with floating-point coefficients and
  model reconstruction. Row blowup past `set_row_cap` means UNKNOWN.
- `MILP`: branch-and-bound over LP. Exhaustion proves UNSAT; node/depth
  caps mean UNKNOWN.
- `Automata`: regex membership with length bounds and equalities, decided
  by Thompson NFA, subset DFA, and product intersection. State caps and
  variable concatenation mean UNKNOWN.
- `Sequence`: integer-element sequences with length/index/concat
  constraints, decided by length propagation plus cell union-find.
  Complete for length-bounded problems (`set_length_cap`); else UNKNOWN.
- `Set`: finite-universe bit-blasting with union/intersection/difference
  and cardinality. Complete.
- `Bag`: bounded multiplicities with exhaustive search. UNSAT is sound
  only when every bag total is explicitly capped within the multiplicity
  bound; else UNKNOWN.
- `ADT`: unification with occurs check, tester propagation,
  disequalities by finite enumeration or bounded witness search.
- `Quant`: QBF-by-expansion over Booleans and finite integer domains.
  Expansion preserves equivalence; clause blowup past
  `set_expansion_cap` means UNKNOWN.
- `Poly`: exact integer-polynomial normal forms; univariate equalities by
  the rational-root theorem; otherwise bounded-box search with explicit
  `add_bound` ranges making exhaustion sound.
- `NRA`: linear fragment via LP, unary intervals, univariate
  bisection, bounded sampling. Equalities validate against 1e-9.
- `FP`: IEEE-754 doubles (round-to-nearest) with NaN-aware comparisons;
  structural contradictions, unary intervals, then coordinate descent.
- `SymSolve`: affine systems by Gaussian elimination, univariate
  quadratics closed-form, multi-start Newton otherwise. Residuals must
  clear the tolerance.
- `ODE`: affine closed forms and validated-Euler polynomial flows with
  interval branch-and-bound safety queries. Queue/precision caps mean
  UNKNOWN.

## Concrete solver inventory

All namespaces in this table are below `satie` and each class is declared
in its corresponding `Satie<Theory>.hpp` header:

| Theory | Namespace/class | Selected controls |
|---|---|---|
| ADT | `adt::ADTSolver` | `set_depth_cap` |
| Automata | `automata::AutomataSolver` | `set_state_cap` |
| Bag | `bag::BagSolver` | `set_multiplicity_bound`, `set_search_cap` |
| BV | `bv::BVSolver` | Variable widths 1..64 |
| EUF | `euf::EUFSolver` | Terms, equalities, disequalities |
| FP | `fp::FPSolver` | `set_search_rounds` |
| IDL | `idl::IDLSolver` | `add_le`, `add_bound` |
| ISL | `isl::ISLSolver` | `set_search_cap`, explicit bounds |
| LIA | `lia::LIASolver` | `set_width` (2..32) |
| LP | `lp::LPSolver` | `set_row_cap` |
| MILP | `milp::MILPSolver` | `set_node_cap`, `set_depth_cap` |
| NRA | `nra::NRASolver` | `set_sample_cap`, `set_bisection_steps` |
| ODE | `ode::ODESolver` | `set_iteration_cap`, `set_precision` |
| Poly | `poly::PolySolver` | `set_search_bound`, explicit bounds |
| Quant | `quant::QuantSolver` | `set_expansion_cap` |
| Sequence | `sequence::SequenceSolver` | `set_length_cap` |
| Set | `set::SetSolver` | Explicit finite universe |
| SymSolve | `symsolve::SymSolveSolver` | `set_newton_cap`, `set_tolerance` |

`SolveResult::assignment` is the Boolean/encoded assignment. Retrieve
typed values through the concrete solver's model accessors after SAT.
Theory variable IDs can be zero-based even though SAT variables are
one-based; keep those namespaces distinct.

## Modular arithmetic example

```cpp
#include "SatieBV.hpp"
#include <cassert>

int main()
{
    satie::bv::BVSolver solver;
    solver.add_var("a", 4);
    solver.add_var("b", 4);
    solver.add_var("sum", 4);
    solver.add_eq_const("a", 15);
    solver.add_eq_const("b", 1);
    solver.add_add("a", "b", "sum");
    assert(solver.check().satisfiable());
    assert(solver.value("sum").value() == 0); // Modulo 16.
}
```

## Difference-logic example

```cpp
#include "SatieIDL.hpp"
#include <cassert>

int main()
{
    satie::idl::IDLSolver solver;
    int start = solver.add_var("start");
    int finish = solver.add_var("finish");
    solver.add_bound(start, 0, 0);
    solver.add_bound(finish, 3, 5);
    solver.add_le(start, finish, -3); // finish >= start + 3.
    assert(solver.check().satisfiable());
    assert(solver.value(finish) - solver.value(start) >= 3);
}
```

## Registry and generic SMT facade

`TheoryRegistry::instance().names()` discovers built-in names and
`create(name)` returns a `TheoryModule` or nullptr. Registration anchors
in the compiled library ensure built-in modules are available even with
static linking. `TheoryRegistrar` allows an additional named factory;
registering a name already present keeps its existing factory.

```cpp
#include "SatieModule.hpp"
#include <cassert>

int main()
{
    auto module = satie::TheoryRegistry::instance().create("BV");
    assert(module);
    assert(module->check(satie::CNF{}).satisfiable());
    assert(module->check(satie::CNF({{1}, {-1}})).unsatisfiable());
}
```

`TheoryModule::check(CNF)` passes Boolean clauses to a fresh module; its
interface cannot add the typed constraints illustrated above. The
[C module API](12-C-and-Foreign-Interfaces.md) has that same scope.
`smt::SMTSolver` in `SatieSMT.hpp` is a facade for Booleanized CNF;
its `Logic` enum is metadata and `check()` invokes CDCL. It does not
automatically dispatch typed constraints based on the selected logic.
The separate [SMT-LIB2 frontend](9-Frontends.md) implements its listed
Boolean/conjunctive-LIA script routes.

## Roadmap

Theory propagation (lemmas back into the SAT solver), incremental
`push`/`pop`, wider operator coverage (BV shifts/division, FP rounding
modes, sequence disequalities), and CAD-grade NRA completeness are
explicitly future work. The UNKNOWN answers above are the seams where
those extensions plug in.
