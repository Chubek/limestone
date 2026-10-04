# Possible Errors

## Parse-time errors

### `satie::ParseError`

Thrown by `parse_cnf`, `parse_dimacs`, and `parse_auto` delegated parser paths.

Diagnostic payload:

- `error.diagnostic.line`
- `error.diagnostic.column`
- `error.diagnostic.message`

`what()` also formats the location and message. Not every higher-level
semantic rejection has a precise source position; some frontend errors
use `(1, 1)`.

Canonical causes:

- malformed DIMACS problem line (`p cnf ...`);
- duplicate DIMACS problem line;
- literal outside `int32` range;
- literal index exceeding declared DIMACS variable count;
- DIMACS clause count mismatch;
- unterminated DIMACS clause missing trailing `0`;
- invalid CNF DSL token;
- CNF DSL empty clause expression.

## I/O errors

### `std::runtime_error`

Thrown by file-based entrypoints:

- `parse_dimacs_file(path)`;
- `parse_cnf_file(path)`;
- `parse_auto_file(path)`;
- `parse_file(path, format)`.

Cause: file open failure or inaccessible path.

## Runtime/algorithmic errors

### `std::overflow_error`

Thrown by `NaiveSolver::count_models()` when variable count is outside guarded counting range.

### `std::invalid_argument`, `std::logic_error`, and resource failures

Invalid literal magnitudes, out-of-range CDCL assumptions, BV width/type
mismatches, undeclared typed theory variables, and invalid pool settings
are argument errors. Theory model access before a successful check can
throw `std::logic_error`. Resource allocation can throw `std::bad_alloc`.
See the concrete header for each method's accepted ranges.

Plugin registration rejects invalid/duplicate commands with
`std::invalid_argument`; invoking an unknown command throws
`std::out_of_range`. Lua/file-loading helpers propagate their interpreter
or I/O errors.

## UNKNOWN versus an error

`SolveStatus::UNKNOWN` is an inconclusive result: for example a CDCL
conflict budget, an exhausted theory search bound, a state/row cap, or an
unsupported mixed SMT-LIB2 fragment. It does not establish UNSAT.
Inspect the explicit status rather than using `!result.satisfiable()`
as an UNSAT test.

C interfaces convert failures into return values and their own
thread-local diagnostics. The ordinary solver's UNKNOWN return can also
represent an API failure, while the module API has a distinct
`SATIE_C_MODULE_ERROR`. Process exit codes are a separate convention.
See [C and Foreign Interfaces](12-C-and-Foreign-Interfaces.md) and
[CLI and REPL](11-CLI-and-REPL.md).

## Catching parse diagnostics

```cpp
#include "Satie.hpp"
#include <cassert>

int main()
{
    try {
        (void)satie::parse("p cnf 1 1\n1\n", satie::ParseFormat::DIMACS);
        return 1;
    } catch (const satie::ParseError &error) {
        assert(error.diagnostic.line >= 1);
        assert(!error.diagnostic.message.empty());
    }
}
```

DSLtk combinators instead return a `ParseOutcome`/`ExpectedResult` with
`dsl::ParseError` offset/expectation data. This is distinct from the
throwing Satie parser interface; see
[Parsing, ASTs, and DSLtk](15-Parsing-AST-and-DSLtk.md).

## Semantic pitfalls

Not exceptions, but high-risk misuse patterns:

- relying on CNF DSL variable numbering externally without stable symbol table export;
- comparing raw statistics across different engines without normalization;
- using Native solver for large formulas;
- supplying DIMACS with inconsistent header metadata.

## Defensive integration pattern

- parse once at ingestion boundary;
- catch `ParseError` and persist `(line,column,message)`;
- reject or quarantine malformed instances;
- export normalized DIMACS for reproducible downstream runs;
- use `Engine::CDCL` for primary solve path;
- reserve Native/DPLL for diagnostics and differential testing.
