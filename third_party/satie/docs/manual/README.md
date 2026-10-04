# Satie Manual Index

The manual describes the current public APIs, build targets, supported
language/theory fragments, and extension helpers. Runnable examples use
the compiled library; component-level limitations are stated alongside
their interfaces.

## Chapter map

| Chapter | Contents |
|---|---|
| [SAT Theory](1-SAT-Theory.md) | Semantics, CNF, complexity, and invariants. |
| [Encoding for SAT](2-Encoding-for-SAT.md) | Primitive constraints, cardinality patterns, DIMACS export. |
| [SAT Algorithms](3-SAT-Algorithms.md) | Native/DPLL/CDCL comparison and solver lifecycle. |
| [Naive Solutions](4-Naive-Solutions.md) | Exhaustive search and model counting. |
| [DPLL Algorithm](4-DPLL-Algorithm.md) | Units, pure literals, branching, chronological backtracking. |
| [CDCL Algorithm](5-CDCL-Algorithm.md) | Watched literals, first-UIP analysis, phases, minimization, restarts. |
| [Using Satie](6-Using-Satie.md) | C++ facade, CNF/assignment conventions, reports, counting, visualization. |
| [Possible Errors](7-Possible-Errors.md) | Diagnostics, exceptions, UNKNOWN, C error channels. |
| [Theory Solvers](8-Theories.md) | Concrete solver inventory, typed models, bounds, registry, SMT facade scope. |
| [Language Frontends](9-Frontends.md) | Boolean formulas, WCNF/OPB optimization, SMT-LIB2, IPASIR, SatieLisp, D ports. |
| [Build and Installation](10-Build-and-Installation.md) | Dependencies, CMake options/targets, offline builds, consumers, docs, tooling. |
| [CLI and REPL](11-CLI-and-REPL.md) | Formats, commands, models, optimization routes, exit codes, session state. |
| [C and Foreign Interfaces](12-C-and-Foreign-Interfaces.md) | Opaque handles, ownership, status conventions, module/plugin APIs, binding scope. |
| [Plugins and Lua](13-Plugins-and-Lua.md) | Command hosts, standard plugins, `lsatie`, callback/context lifetimes, script modules. |
| [Standard Library](14-Standard-Library.md) | Helper inventory, fresh variables, encodings, preprocessing/model reconstruction, Lua counterparts. |
| [Parsing, ASTs, and DSLtk](15-Parsing-AST-and-DSLtk.md) | Grammar selection, symbol numbering, backend, DAG/DOT, toolkit and parser combinators. |
| [Solver Controls and Diagnostics](16-Solver-Controls-and-Diagnostics.md) | Budgets, assumptions/cores, statistics, memory resources, validation/test targets. |

## Recommended reading paths

- **First build:** [Build and Installation](10-Build-and-Installation.md)
  → [Using Satie](6-Using-Satie.md) → [CLI and REPL](11-CLI-and-REPL.md).
- **SAT internals:** [SAT Theory](1-SAT-Theory.md)
  → [Encoding](2-Encoding-for-SAT.md) → [Algorithms](3-SAT-Algorithms.md)
  → Native, DPLL, and CDCL chapters.
- **Typed constraints:** [Theory Solvers](8-Theories.md)
  → [Language Frontends](9-Frontends.md) → [Possible Errors](7-Possible-Errors.md).
- **Embedding/extending:** [C interfaces](12-C-and-Foreign-Interfaces.md)
  → [Plugins and Lua](13-Plugins-and-Lua.md)
  → [Standard Library](14-Standard-Library.md)
  → [Parsing and DSLtk](15-Parsing-AST-and-DSLtk.md).
- **Tuning and debugging:** [Solver Controls](16-Solver-Controls-and-Diagnostics.md)
  → [Possible Errors](7-Possible-Errors.md).

Return to the [documentation front page](../FrontPage.md).
