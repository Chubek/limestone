# Satie — Front Page

**A C++20 SAT solver with typed theory fragments, language frontends,
and embedding tools.**

Satie exposes exhaustive Native, DPLL, and CDCL engines through a common
facade. A compiled static library supplies the facade, memory/parser
services, C APIs, and component registration. The project also includes
18 concrete theory modules, C++/D input frontends, a CLI/REPL, plugins/Lua
integration, and a helper standard library. Individual chapters describe
the supported fragments and bounds.

## Start here

- [Build and install](manual/10-Build-and-Installation.md), including
  parser dependencies, optional D compilation, and CMake consumers.
- [Solve through C++](manual/6-Using-Satie.md) or use the
  [CLI and REPL](manual/11-CLI-and-REPL.md).
- [Choose an input language](manual/9-Frontends.md) or a
  [typed theory solver](manual/8-Theories.md).
- [Embed through C](manual/12-C-and-Foreign-Interfaces.md),
  [extend with plugins/Lua](manual/13-Plugins-and-Lua.md), or use
  [standard-library helpers](manual/14-Standard-Library.md).
- [Tune and inspect a solve](manual/16-Solver-Controls-and-Diagnostics.md)
  and interpret [errors and UNKNOWN](manual/7-Possible-Errors.md).

## Manual

| Chapter | Topic |
|---|---|
| [SAT Theory](manual/1-SAT-Theory.md) | Semantics, CNF, and complexity. |
| [Encoding for SAT](manual/2-Encoding-for-SAT.md) | Constraint encodings and DIMACS emission. |
| [SAT Algorithms](manual/3-SAT-Algorithms.md) | Engine portfolio comparison. |
| [Naive Solutions](manual/4-Naive-Solutions.md) | Exhaustive search and model counting. |
| [DPLL Algorithm](manual/4-DPLL-Algorithm.md) | Units, pure literals, and backtracking. |
| [CDCL Algorithm](manual/5-CDCL-Algorithm.md) | Watches, learning, minimization, and restarts. |
| [Using Satie](manual/6-Using-Satie.md) | Facade, data types, assignments, and reports. |
| [Possible Errors](manual/7-Possible-Errors.md) | Diagnostics, exceptions, and inconclusive results. |
| [Theory Solvers](manual/8-Theories.md) | Typed constraints/models, theory inventory, and bounds. |
| [Language Frontends](manual/9-Frontends.md) | Boolean input, MaxSAT/PB, SMT-LIB2, IPASIR, Lisp, and D. |
| [Build and Installation](manual/10-Build-and-Installation.md) | CMake, dependencies, targets, offline/install workflows, and tooling. |
| [CLI and REPL](manual/11-CLI-and-REPL.md) | Command syntax, formats, output, and sessions. |
| [C and Foreign Interfaces](manual/12-C-and-Foreign-Interfaces.md) | Handles, ownership, status values, and language-binding scope. |
| [Plugins and Lua](manual/13-Plugins-and-Lua.md) | Command hosts, standard plugins, and scripting. |
| [Standard Library](manual/14-Standard-Library.md) | Module inventory, encodings, and model reconstruction. |
| [Parsing, ASTs, and DSLtk](manual/15-Parsing-AST-and-DSLtk.md) | Grammar/backend, symbols, DOT, and parser/toolkit APIs. |
| [Solver Controls and Diagnostics](manual/16-Solver-Controls-and-Diagnostics.md) | Assumptions, cores, budgets, counters, memory, and validation. |

The [manual index](manual/README.md) also provides task-oriented reading
paths. Every chapter is available in the generated Doxygen documentation.

## Library entry points

- `Satie.hpp`, `SatieSAT.hpp`: C++ facade and SAT convenience functions.
- `Common.hpp`: literals, CNF, assignments, results, parsing, and graph export.
- `SatieNative.hpp`, `SatieDPLL.hpp`, `SatieCDCL.hpp`: concrete SAT engines.
- `Satie<Theory>.hpp`, `SatieSMT.hpp`, `SatieModule.hpp`: typed theory
  solvers, Booleanized SMT facade, and registry.
- `SatieFrontend*.hpp`: language-specific parse/solve APIs.
- `Satie.h`, `SatieModule.h`, `SatiePlugin.h`: opaque C interfaces.
- `SatiePlugin.hpp`: C++ commands and the QaMRpp Lua bridge.
- `SatieMemory.hpp`, `DSLtk.hpp`, `stdlib/Stdlib.hpp`: resource,
  language-construction, and application helper APIs.

## Quick start

```cpp
#include "Satie.hpp"
#include <cassert>

int main()
{
    satie::CNF problem({{1, 2}, {-1, 3}});
    auto result = satie::Solver(problem).solve(); // CDCL by default.
    assert(result.satisfiable());
    assert(satie::is_formula_satisfied(problem, result.assignment));
}
```

Link `satie::satie`; see the build chapter for an in-tree or installed
consumer. The complete source examples are under `examples/`.
