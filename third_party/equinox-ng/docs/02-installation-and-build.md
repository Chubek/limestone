# Chapter 2: Installation and Build

## Purpose

This chapter documents the real build and install workflow defined in `CMakeLists.txt` and `install.sh`.

## Prerequisites

From project sources:

- CMake >= 3.10 (`CMakeLists.txt`)
- C compiler and C++ compiler (`project(... LANGUAGES C CXX)`)
- A valid `HOME` environment variable (required by install paths in `CMakeLists.txt`)

## What gets built

`CMakeLists.txt` defines three targets:

- `qamrpp-sexpr` shared library from:
  - `third_party/QaMRpp/QaMRpp-Library.c`
  - `qamrpp-lib/S-Expression.c`
- `qamrpp-leqvsg` shared library from:
  - `third_party/QaMRpp/QaMRpp-Library.c`
  - `qamrpp-lib/leqvsg.c`
- `eqvsg` executable from:
  - `EQVSG.cpp`

Optimization is configurable through cache variable `OPT_LEVEL` (default `2`) and mapped to `-O<level>`.

## Standard CMake build

```bash
cmake -S . -B build
cmake --build build -j
```

If needed, tune optimization:

```bash
cmake -S . -B build -DOPT_LEVEL=3
cmake --build build -j
```

## Installation layout

Install destinations are set relative to `$HOME` in `CMakeLists.txt`:

- Libraries: `$HOME/.qamrpp/eqvsglib`
  - `libqamrpp-sexpr.so`
  - `libqamrpp-leqvsg.so`
- CLI binary: `$HOME/.local/bin/eqvsg`
- Headers (`*.hpp` from repo root): `$HOME/.local/include`

Run install:

```bash
cmake --install build
```

## `install.sh` workflow

`install.sh` provides a scripted GCC-based build/install path that mirrors the CMake intent:

- builds shared objects into `bin/`
- installs `.so` files into `$HOME/.qamrpp/eqvsglib/`
- builds `eqvsg` and installs into `$HOME/.local/bin/`
- installs headers into `$HOME/.local/include/`

Use it when you prefer a simple shell install path.

## Runtime library resolution and why install paths matter

`EQVSG.cpp` loads libraries in this order:

1. `$HOME/.qamrpp/eqvsglib/libqamrpp-sexpr.so`
2. fallback `bin/libqamrpp-sexpr.so`
3. `$HOME/.qamrpp/eqvsglib/libqamrpp-leqvsg.so`
4. fallback `bin/libqamrpp-leqvsg.so`

So either:

- install libraries to `$HOME/.qamrpp/eqvsglib`, or
- keep built `.so` files in `bin/` for local runs.

## Examples and docs in workflow

- Input assets for testing runs: `examples/expr-definitions/`, `examples/rewrite-rules/`, `examples/tests/`
- Expected outputs: `examples/outputs/` (if present/populated in your checkout)
- Documentation target: `docs/CMakeLists.txt` (integrated from root CMake)
