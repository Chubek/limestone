# Chapter 30 — Integration Patterns

Recommended pipelines:

```text
Source -> parse -> validate errors -> analyze -> transform -> semanticize
```

For data exchange:

```text
parse -> to_string/to_json -> transport
```

For streaming:

```text
parse(dispatcher) -> XASEvent -> to_c -> frame_encode -> transport
```

At the receiver:

```text
frame_validate -> frame_decode -> copy payload if needed -> dispatch
```

Use `Cartable::events` for audit/replay and `XASEventDispatcher` for synchronous observation. Use `sexprtk_xas_pump` when event production is external or incremental.

Keep structural and semantic layers separate. Serialize a tree when the consumer needs normalized data; transmit XAS when it needs source order, locations, comments, or recoverable parser errors.

The examples under `SExprTk/examples` cover data exchange, a small interpreter, XAS datagrams, and runtime kernels.
