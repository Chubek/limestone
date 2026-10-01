# Chapter 19 — Analysis Framework

An `Analyzer` observes a `Cartable` and returns `AnalysisResult`:

```cpp
class Analyzer {
    virtual AnalysisResult analyze(const Cartable&) const = 0;
    virtual std::string name() const = 0;
};
```

`AnalysisResult::facts` is a string-keyed map. `set` accepts strings and `size_t`; `get` returns an optional string; `get_count` parses an unsigned decimal fallback.

`notes` is an unstructured vector for nonfatal observations. The framework imposes no schema, namespace, or diagnostic severity.

The analyzer contract is nonmutation. Implementations should treat the tree, metadata, events, and errors as read-only and should document fact keys as an API. `SExprTk::analyze(source, analyzer)` parses first and then invokes the pass; parse errors remain in the temporary cartable and are not automatically converted into analysis failure.

Analysis is suitable for metrics, symbol discovery, legality checks, free-variable collection, and rewrite preconditions.
