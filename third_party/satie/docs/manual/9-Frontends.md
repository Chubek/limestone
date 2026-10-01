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

## WCNF / MaxSAT

`SatieFrontendWCNF.hpp` parses DIMACS-WCNF (`p wcnf`, weights, `top`) and
`solve_maxsat` optimizes by binary search over relaxation blockers with a
sequential counter. The reported cost is optimal when the hard clauses are
satisfiable; hard-UNSAT is reported exactly.

## OPB / pseudo-Boolean

`SatieFrontendOPB.hpp` parses the competition subset (`*` comments,
optional `min:`/`max:`, `coeff*xN ... (>=|<=|=) rhs ;`) and bit-blasts
constraints into signed two's-complement arithmetic solved by CDCL.
Models are re-validated by integer evaluation; `optimize_pb_min/max`
binary-search the objective. Widths are derived from the magnitudes and
capped (overflow degrades to UNKNOWN, never a wrong answer).

## SMT-LIB2 (subset)

`SatieFrontendSMTLIB2.hpp` accepts `set-logic` (QF_LIA/QF_UF/QF_LRA/ALL),
`declare-const` (`Bool`/`Int`), `assert`, `check-sat`, `get-model`,
`exit` (plus ignored `set-option`/`set-info`). Purely Boolean scripts go
through Tseitin+CDCL (full); conjunctions of linear integer comparisons
go through the LIA solver (sound, bounded-complete); mixed Boolean+theory
structure answers UNKNOWN honestly. Models map names to `bool`/`int64`.

## IPASIR (incremental)

`SatieFrontendIPASIR.hpp` offers `IpasirSolver` (`add` with `0`
terminators, `assume`, `solve` returning 10/20/0, `val`, `failed`) plus C
bindings (`satie_ipasir_init/release/add/assume/solve/val/failed`).
Assumptions are first-class solver citizens: backtracking re-asserts
dropped assumptions, learnt clauses falsified by assumptions prove UNSAT,
and `failed` reports a deletion-minimized core. The D port
(`frontends/IPASIR.d`) binds the same C entry points.

## SatieLisp

`SatieFrontendSatieLisp.hpp` parses `(and/or/not/xor/=>/<=>/nand/nor)`
over identifiers (`;` comments) into Tseitin CNF; `solve_satielisp`
returns a named model (auxiliaries filtered).

## D ports

`frontends/Common.d` (DIMACS), `CNF.d` (formulas+Tseitin), `WCNF.d`
(parse+cost), `OPB.d` (parse+eval), `SMT-LIB2.d` (S-expressions),
`SatieLisp.d` (parse+eval), `IPASIR.d` (bindings+wrapper). Verified by
`d_frontends_unittest` (inline unittests) and `d_ipasir_smoke` (links the
D wrapper against the built static library) whenever a D compiler
(`ldc2`/`dmd`/`gdc`) is found.
