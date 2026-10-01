# Chapter 3 — Architecture

The processing graph is:

```text
Source -> Parser -> Cartable -> {Serializer, Analyzer, Transformer, Semanticizer}
                              \-> XAS dispatcher / C frame protocol
```

`Source` owns immutable-by-convention text and a diagnostic name. `Parser` constructs a `List` tree and emits ordered XAS events. `Cartable` is the durable interchange object.

The tree layer is structural. `Atom` carries a tagged value; `Cell` combines a head with an optional tail; `List` stores cells. This representation preserves both ordinary nested lists and the parser’s quote sugar.

Analysis is observational: an `Analyzer` returns `AnalysisResult` and must not mutate its input. Transformation is functional at the API boundary: a `Transformer` returns a new `Cartable`. Semanticization evaluates a parsed tree and returns `Semantics`.

The runtime adapters are thin. `Kernel::evaluate` delegates to a backend semanticizer, while `Kernel::run` exposes only rendered output. This keeps runtime-specific behavior out of parsing and structural passes.
