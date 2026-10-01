# Theory Solvers (DPLL(T) Subsets)

Each theory header (`include/Satie<BV,EUF,LP,...>.hpp`) pairs the shared
Boolean engines with a decision procedure for its fragment. The combination
is a layered DPLL(T) without theory propagation back into the SAT solver
yet: the loaded CNF is solved first, then the theory constraints are
decided; `check` returns SAT only when both hold. Every solver keeps the
batch-1 contract (`theory_name`, `load(CNF)`, `problem()`, `check()`), so
the `TheoryRegistry` and the C module API work unchanged.

Shared bit-blasting helpers (Tseitin gates, ripple-carry adders, unsigned
comparators, sequential-counter cardinality) live in
`include/SatieTheoryUtils.hpp`.

## Soundness contract

- SAT is always witnessed: by a validated model (arithmetic theories
  re-evaluate every constraint on the extracted model) or by construction
  (exact encodings).
- UNSAT is reported only from exact certificates: negative cycles (IDL),
  empty Fourier-Motzkin systems (LP), exhaustive bounded search with
  provable bound completeness (Set, Bag, bounded Poly/Sequence/Quant),
  unification failures (ADT/EUF), or structural contradictions (FP/NRA).
- Anything inconclusive is UNKNOWN, never a guessed UNSAT. Bounded
  encodings that miss a model outside the bound are the main UNKNOWN
  source (LIA, ISL repair, NRA sampling, FP/ODE search).

## Per-theory notes

- `BV`: fixed-width unsigned integers, `==`, `<`, `+` mod 2^w, plus
  constant variants. Complete via bit-blasting (widths 1..64).
- `EUF`: congruence closure over uninterpreted functions. Complete.
- `IDL`: `x - y <= c` via Bellman-Ford, with models. Complete.
- `ISL`: IDL plus `d | (x - offset)`. IDL-infeasible means UNSAT;
  otherwise bounded repair over explicitly bounded variables
  (`add_bound`), UNKNOWN when unbounded.
- `LIA`: integer linear constraints. Rational relaxation via LP first
  (infeasible means UNSAT), then bounded two's-complement blasting with
  integer-validated models. Bounded miss means UNKNOWN.
- `LP`: rational feasibility via Fourier-Motzkin with model
  reconstruction. Row blowup past `set_row_cap` means UNKNOWN.
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
- `NRA`: linear fragment via LP (exact UNSAT), unary intervals, univariate
  bisection, bounded sampling. Equalities validate against 1e-9.
- `FP`: IEEE-754 doubles (round-to-nearest) with NaN-aware comparisons;
  structural contradictions, unary intervals, then coordinate descent.
- `SymSolve`: affine systems by Gaussian elimination, univariate
  quadratics closed-form, multi-start Newton otherwise. Residuals must
  clear the tolerance.
- `ODE`: affine closed forms and validated-Euler polynomial flows with
  interval branch-and-bound safety queries. Queue/precision caps mean
  UNKNOWN.

## Roadmap

Theory propagation (lemmas back into the SAT solver), incremental
`push`/`pop`, wider operator coverage (BV shifts/division, FP rounding
modes, sequence disequalities), and CAD-grade NRA completeness are
explicitly future work. The UNKNOWN answers above are the seams where
those extensions plug in.
