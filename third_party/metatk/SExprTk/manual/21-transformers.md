# Chapter 21 — Transformation Framework

A `Transformer` rewrites a `Cartable`:

```cpp
virtual Cartable transform(const Cartable&) const = 0;
virtual std::string name() const = 0;
```

The contract requires a new result and forbids in-place mutation of the input. Built-in transformers copy the input cartable, replace `root`, and retain metadata, events, and errors. They do not regenerate events after rewriting, so retained event streams describe the pre-transform tree.

Transformers support:

- desugaring;
- canonicalization;
- constant folding;
- normalization;
- target-specific lowering.

Composition is explicit: invoke one transformer, then pass its result to the next. The framework does not provide a pass manager, invalidation model, legality framework, or transactional failure status.
