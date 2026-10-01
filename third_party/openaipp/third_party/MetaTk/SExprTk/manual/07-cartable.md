# Chapter 7 — Cartable

`Cartable` is the canonical parse artifact:

```cpp
struct Cartable {
    List root;
    std::map<std::string, std::string> metadata;
    std::vector<XASEvent> events;
    std::vector<std::string> errors;
};
```

`root` is the structural program. `metadata["source"]` is populated by `SExprTk::parse`. `events` contains parser-generated XAS events even when no dispatcher is supplied. `errors` contains diagnostics with source, line, and column appended.

`ok()` is equivalent to `errors.empty()`. It does not validate semantics or runtime availability.

Convenience methods serialize only `root`:

```cpp
cartable.to_string();
cartable.to_json();
```

Errors do not prevent the parser from returning a partial tree. Consumers must inspect `ok()` before evaluating or transforming malformed input. Events and diagnostics are independent: an `ERROR` event is recorded, then parsing attempts recovery.
