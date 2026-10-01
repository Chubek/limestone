# Chapter 1 — Overview

SExprTk is a header-only C++20 toolkit for s-expression documents. Its public surface separates:

- source acquisition;
- parsing into a typed tree;
- serialization to s-expression, JSON, and TOML;
- XAS event streaming and datagram framing;
- read-only analysis;
- persistent tree transformation;
- semanticization through external runtimes;
- package metadata and assembly.

The central parse result is `sexprtk::Cartable`. It contains a root `List`, source metadata, XAS events, and diagnostics. Parsing does not evaluate forms. `SExprTk::run(source)` is a round-trip serialization operation; evaluation requires a `Kernel` or `Semanticizer`.

The implementation is concentrated in `SExprTk/include/SExprTk.hpp`; the C-compatible XAS contract is in `SExprTk/include/SExprTk-XASEvent.h`. The latter declares interfaces only. The C++ header supplies inline reference implementations when included from C++.

The design is deliberately runtime-neutral. SExprTk owns syntax and structural passes; Lua/QaMRpp and S7 provide optional meanings.
