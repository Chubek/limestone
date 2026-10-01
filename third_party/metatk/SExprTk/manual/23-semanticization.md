# Chapter 23 — Semanticization

`Semantics` separates denotation from presentation:

```cpp
struct Semantics {
    Atom value;
    std::string rendered;
    std::vector<std::string> errors;
};
```

`ok()` checks that `errors` is empty; boolean conversion delegates to `ok()`. `str()` returns `rendered`.

A `Semanticizer` maps a parsed cartable to `Semantics`. It is not a serializer: it evaluates through a language runtime or custom interpreter. `SExprTk::semanticize(source, sem)` parses and delegates.

The generic interface intentionally does not define:

- environment persistence;
- multi-form result policy;
- side-effect isolation;
- runtime memory ownership;
- error severity.

Backends in this release choose their own policy. Both built-in runtimes treat the final program result as the denotation, but they differ in source emission and runtime setup.
