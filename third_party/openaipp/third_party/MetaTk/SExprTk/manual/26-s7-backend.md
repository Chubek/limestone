# Chapter 26 — S7 Scheme Backend

`S7KernelSemanticizer` emits canonical s-expression forms, wraps them in `(begin ...)`, initializes a fresh S7 interpreter, and evaluates the result. The final form supplies the denotation.

The conversion to `Atom` handles:

- booleans;
- integers and reals;
- strings and symbols;
- proper pairs recursively;
- null;
- all other values as symbol text.

Rendered output uses `s7_object_to_c_string`; unavailable rendering becomes `<#unspecified>`. S7 initialization failure is reported in `Semantics::errors`.

Without `SEXPRTK_WITH_S7`, the backend returns an explicit configuration error. CMake enables S7 only when the source and generated configuration are available.

Fresh interpreter creation means definitions and mutable state do not persist between semanticization calls. Programs relying on persistent environments require a custom kernel or a backend-specific extension.
