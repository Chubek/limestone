# Chapter 1: Overview

## Purpose

This chapter explains what EquinoxNG is, what it does, and how the two usage modes (CLI and API) fit together.

## What EquinoxNG is

EquinoxNG in this repository has two practical layers:

1. A **rewriting runtime exposed through QaMRpp native functions** implemented in `qamrpp-lib/leqvsg.c`.
2. A **CLI wrapper** in `EQVSG.cpp` that loads those native libraries and runs a three-step rewrite flow.

In parallel, the repository also provides C++ headers (`EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`) for library/developer use.

## Problem it solves

EquinoxNG automates expression rewriting by applying rule files to input S-expressions.

- Expression definitions (`*.eqdef`) model a domain vocabulary (name, sort, ops).
- Rewrite rules (`*.rwr`) define pattern-to-replacement transforms using `=>`.
- Input expressions (`*.sexpr`) are rewritten and emitted as output.

This lets you standardize symbolic transforms for arithmetic, logic, lists, and related term domains with reproducible file-driven workflows.

## High-level workflow

Grounded in `EQVSG.cpp`, the runtime flow is:

1. Read input expression file as text.
2. Create a `qamrpp::Context`.
3. Load `libqamrpp-sexpr.so` and `libqamrpp-leqvsg.so`.
4. Bind file paths/input into QaMRpp variables.
5. Execute:
   - `eqvsg_load_expr_definition(path)`
   - `eqvsg_load_rewrite_rules(path)`
   - `eqvsg_rewrite(input)`
6. Print result.

## Where the CLI fits

The `eqvsg` executable is for end users who want file-to-file rewrite runs without writing C++ code.

Invocation shape (from `EQVSG.cpp`):

```text
eqvsg <expr-definition.eq> <rewrite-rules.eq> <input.sexpr>
```

In practice, this works directly with files under `examples/`.

## Where the C++ API fits

Developers can:

- use the same QaMRpp-native rewrite API from C++ (`qamrpp::Context` + native functions), or
- integrate higher-level C++ components from the project headers (`EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`) as part of custom tooling.

The API mode is useful when you want embedding, custom orchestration, or advanced extension beyond the default CLI behavior.

## Key repository anchors

- CLI entrypoint: `EQVSG.cpp`
- Native rewrite engine: `qamrpp-lib/leqvsg.c`
- Core headers: `EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`
- Build/install: `CMakeLists.txt`, `install.sh`
- Concrete examples: `examples/expr-definitions/`, `examples/rewrite-rules/`, `examples/tests/`
- Behavioral guarantees: `tests/unit_leqvsg.cpp`, `tests/fuzz/`
