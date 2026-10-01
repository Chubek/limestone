# Chapter 25 — Lua Backend

`LuaKernelSemanticizer` emits a Lua chunk from the tree and executes it through QaMRpp. Translation rules include:

- nil, booleans, numbers, and strings become Lua literals;
- symbols become identifiers/raw expressions;
- `(f a b)` becomes `f(a, b)`;
- top-level forms become statements, with the last prefixed by `return`.

An empty document becomes `return nil`. The implementation creates a fresh `qamrpp::Context` per call. Runtime exceptions become `lua: ...` semantic errors.

QaMRpp values map back to `Atom` for nil, booleans, integers, floats, and strings. Other values become symbol atoms containing `to_string()` output; tables and functions are not represented as structured SExprTk values.

Without `SEXPRTK_WITH_LUA`, semanticization returns an explicit rebuild/configuration error. The macro requires QaMRpp, MiniZIP, and related include paths configured by CMake.
