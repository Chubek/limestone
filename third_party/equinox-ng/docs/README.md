# EquinoxNG Documentation

This directory contains the complete documentation set for EquinoxNG in 12 chapters, covering both:

- command-line usage through `eqvsg`
- C/C++ integration through the project headers and QaMRpp bindings

The content is grounded in repository sources such as `EQVSG.cpp`, `EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`, `qamrpp-lib/leqvsg.c`, `CMakeLists.txt`, `install.sh`, `examples/`, and `tests/`.

## Who this documentation is for

- **CLI users** who want to run rewrite transformations with `eqvsg`
- **C++ developers** integrating EquinoxNG headers and related APIs
- **Contributors** extending rewrite rules, expression definitions, tests, and internals

## Chapter Index

1. [Overview](01-overview.md)
2. [Installation and Build](02-installation-and-build.md)
3. [Project Architecture](03-project-architecture.md)
4. [CLI Usage (`eqvsg`)](04-cli-usage-eqvsg.md)
5. [C++ API Usage](05-api-usage-cpp.md)
6. [Expression Definitions](06-expression-definitions.md)
7. [Rewrite Rules](07-rewrite-rules.md)
8. [S-Expressions and Data Model](08-s-expressions-and-data-model.md)
9. [Examples and Workflows](09-examples-and-workflows.md)
10. [Testing, Validation, and Fuzzing](10-testing-validation-and-fuzzing.md)
11. [DSLUtils and Advanced Extension](11-dslutils-and-advanced-extension.md)
12. [Troubleshooting and Reference](12-troubleshooting-and-reference.md)

## Quick Start (CLI)

Build and install:

```bash
cmake -S . -B build
cmake --build build -j
cmake --install build
```

Run:

```bash
eqvsg examples/expr-definitions/01-arith-core.eqdef \
      examples/rewrite-rules/01-add-zero.rwr \
      examples/tests/input-01-add-zero.sexpr
```

## Quick Start (API)

For library/runtime integration, load the two native libraries in a QaMRpp context and call:

- `eqvsg_load_expr_definition(path)`
- `eqvsg_load_rewrite_rules(path)`
- `eqvsg_rewrite(input)`

See [C++ API Usage](05-api-usage-cpp.md) for a complete example.

## Related Repository Paths

- Build and install: `CMakeLists.txt`, `install.sh`
- CLI entrypoint: `EQVSG.cpp`
- Native rewrite runtime: `qamrpp-lib/leqvsg.c`
- Headers: `EquinoxNG.hpp`, `RdvsgNG.hpp`, `DSLUtils.hpp`
- Samples: `examples/`
- Validation: `tests/`, `tests/fuzz/`
