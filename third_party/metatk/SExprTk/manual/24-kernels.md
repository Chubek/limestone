# Chapter 24 — Kernel Interface

`Kernel` adapts a semanticizer to the generic run pipeline:

```cpp
virtual Semantics evaluate(const Cartable&) const = 0;
virtual std::string name() const = 0;
std::string run(const Cartable&) const;
```

`run` returns only `evaluate(cartable).rendered`; it does not throw or expose `Semantics::errors`.

`SExprTk::run(source, kernel)` parses and returns rendered evaluation. For diagnostics or typed results, call `kernel.evaluate(rt.parse(source))`.

The base abstraction permits custom interpreters, domain-specific evaluators, and test doubles. A kernel should preserve the semanticizer contract: input is structural, result is a denotation plus rendering, and failures belong in `Semantics::errors`.

Backend availability is compile-time. Disabled built-in kernels return a populated error vector instead of failing construction.
