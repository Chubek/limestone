# Chapter 3: Project Architecture

## Purpose

This chapter maps the repository structure to runtime behavior so users and contributors can navigate EquinoxNG quickly.

## Top-level component map

The project is organized around five cooperating areas:

1. **CLI frontend**: `EQVSG.cpp`
2. **Native rewrite runtime**: `qamrpp-lib/leqvsg.c` and `qamrpp-lib/S-Expression.c`
3. **C/C++ headers**: `EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`
4. **Usage examples and test inputs**: `examples/`
5. **Validation suites**: `tests/unit_leqvsg.cpp`, `tests/fuzz/`

Plus vendored **QaMRpp runtime and APIs** in `third_party/QaMRpp/`.

## Build-time architecture

`CMakeLists.txt` builds three artifacts:

- `libqamrpp-sexpr.so` (`qamrpp-sexpr` target)
- `libqamrpp-leqvsg.so` (`qamrpp-leqvsg` target)
- `eqvsg` executable

Both shared libraries compile against `third_party/QaMRpp/QaMRpp-Library.c`, which is the bridging ABI used to register and invoke native functions from QaMRpp scripts.

## Runtime architecture and data flow

### Step A: Process setup (`EQVSG.cpp`)

`eqvsg` reads three input files indirectly:

- expression definition path
- rewrite rule path
- S-expression input file content

It constructs a `qamrpp::Context`, loads native libraries, and assigns three QaMRpp variables:

- `__eqvsg_expr_def_path`
- `__eqvsg_rules_path`
- `__eqvsg_input_expr`

### Step B: Script-level orchestration (`EQVSG.cpp`)

The CLI executes this QaMRpp script:

```text
eqvsg_load_expr_definition(__eqvsg_expr_def_path)
eqvsg_load_rewrite_rules(__eqvsg_rules_path)
return eqvsg_rewrite(__eqvsg_input_expr)
```

This keeps policy in the CLI thin and delegates core behavior to native runtime functions.

### Step C: Native rule engine (`qamrpp-lib/leqvsg.c`)

The native library provides (through QaMRpp registration hooks) rewrite-related functions including:

- `eqvsg_define_expr`
- `eqvsg_load_expr_definition`
- `eqvsg_load_rewrite_rules`
- `eqvsg_rewrite`

Internally, `leqvsg.c` stores:

- active expression definition text
- active rewrite-rules source text
- parsed rule array (`pattern`, `replacement`)

Rule application is string-based and iterative, with pass/rule limits guarded by constants:

- `LEQVSG_MAX_RULES` (1024)
- `LEQVSG_MAX_REWRITE_PASSES` (1024)

## Component responsibilities

### `EQVSG.cpp` (CLI adapter)

- validates argument count (`argc == 4`)
- loads libraries with `$HOME` then `bin/` fallback
- maps file paths/content into QaMRpp values
- prints rewritten output or detailed error

### `qamrpp-lib/leqvsg.c` (rewrite runtime)

- parses rule files line-by-line using `=>`
- trims whitespace and supports comment/blank lines
- rejects malformed rules (e.g., missing `=>`, empty LHS)
- applies rewrites via repeated single replacements until fixed point or pass cap

### `qamrpp-lib/S-Expression.c`

- provides companion S-expression support loaded by `eqvsg`

### Header layer (`*.hpp`)

- `EquinoxNG.hpp`: header-only e-graph framework and rewrite/extraction scaffolding
- `RdvsgNG.hpp`: C++ API surface connected to rewrite model and usage
- `DSLUtils.hpp`: composable C++20 DSL utility features for advanced extension

(See Chapter 5 and Chapter 11 for detailed API specifics.)

## Examples and tests as architecture evidence

- `examples/rewrite-rules/*.rwr` show canonical pattern/replacement pairs.
- `examples/expr-definitions/*.eqdef` show domain declaration format.
- `examples/tests/*.sexpr` provide concrete input expressions.
- `tests/unit_leqvsg.cpp` codifies accepted/invalid behavior.
- `tests/fuzz/` targets parser and rewrite robustness under random input.

## Integration with QaMRpp

EquinoxNG does not reimplement script context management. Instead it embeds through QaMRpp:

- C ABI layer via `QaMRpp-Library.h`/`.c`
- C++ wrapper via `QaMRpp.hpp` and `qamrpp::Context`

This allows the same native rewrite functions to be driven from either:

- the provided `eqvsg` CLI, or
- custom C++ applications that execute QaMRpp scripts.
