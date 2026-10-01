# Chapter 27 — SExprTk Driver

`SExprTk` is the orchestration façade:

```cpp
SExprTk rt("compiler");
auto tree = rt.parse(source);
auto text = rt.run(source);
auto value = rt.semanticize(source, semanticizer);
auto facts = rt.analyze(source, analyzer);
auto rewritten = rt.transform(source, transformer);
auto pkg = rt.package(source, manifest);
```

`run(source)` means parse plus `Cartable::to_string`, not evaluation. Overloads accepting `Kernel` or `Semanticizer` evaluate.

Every operation that accepts `Source` reparses it. There is no internal cache. Reuse a `Cartable` when multiple passes must share one parse artifact and its event/diagnostic state.

The driver name is metadata only; `name()` returns the constructor-supplied string and does not affect parsing or runtime selection.
